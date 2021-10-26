// somebar - dwl bar
// See LICENSE file for copyright and license details.

#include <cstdio>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <list>
#include <optional>
#include <vector>
#include <QGuiApplication>
#include <QSocketNotifier>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "net-tapesoftware-dwl-wm-unstable-v1-client-protocol.h"
#include "common.hpp"
#include "bar.hpp"

struct Monitor {
    uint32_t name;
    wl_unique_ptr<wl_output> wlOutput;
    wl_unique_ptr<znet_tapesoftware_dwl_wm_monitor_v1> dwlMonitor;
    std::optional<Bar> bar;
    bool created;
};
struct SeatPointer {
    wl_unique_ptr<wl_pointer> wlPointer;
    Bar *focusedBar;
    int x, y;
    std::vector<int> btns;
};
struct Seat {
    uint32_t name;
    wl_unique_ptr<wl_seat> wlSeat;
    std::optional<SeatPointer> pointer;
};

static void waylandFlush();
static void waylandWriteReady();
static void requireGlobal(const void *p, const char *name);
static void setupStatusFifo();
static void onStatus();
static void cleanup();

wl_display *display;
wl_compositor *compositor;
wl_shm *shm;
zwlr_layer_shell_v1 *wlrLayerShell;
znet_tapesoftware_dwl_wm_v1 *dwlWm;
std::vector<QString> tagNames;
std::vector<QString> layoutNames;
static xdg_wm_base *xdgWmBase;
static wl_surface *cursorSurface;
static wl_cursor_image *cursorImage;
static bool ready;
static std::list<Monitor> monitors;
static std::list<Seat> seats;
static QString lastStatus;
static std::string statusFifoName;
static int statusFifoFd {-1};
static int statusFifoWriter {-1};
static QSocketNotifier *displayWriteNotifier;
static bool quitting {false};

void view(Monitor &m, const Arg &arg)
{
    znet_tapesoftware_dwl_wm_monitor_v1_set_tags(m.dwlMonitor.get(), arg.ui, 1);
}
void toggleview(Monitor &m, const Arg &arg)
{
    znet_tapesoftware_dwl_wm_monitor_v1_set_tags(m.dwlMonitor.get(), arg.ui, 0);
}
void setlayout(Monitor &m, const Arg &arg)
{
    znet_tapesoftware_dwl_wm_monitor_v1_set_layout(m.dwlMonitor.get(), arg.ui);
}
void tag(Monitor &m, const Arg &arg)
{
    znet_tapesoftware_dwl_wm_monitor_v1_set_client_tags(m.dwlMonitor.get(), 0, arg.ui);
}
void toggletag(Monitor &m, const Arg &arg)
{
    znet_tapesoftware_dwl_wm_monitor_v1_set_client_tags(m.dwlMonitor.get(), 0xffffff, arg.ui);
}

static const struct xdg_wm_base_listener xdgWmBaseListener = {
    [](void*, xdg_wm_base *sender, uint32_t serial) {
        xdg_wm_base_pong(sender, serial);
    }
};

static Bar* barFromSurface(const wl_surface *surface)
{
    auto mon = std::find_if(begin(monitors), end(monitors), [surface](const Monitor &mon) {
        return mon.bar && mon.bar->surface() == surface;
    });
    return mon != end(monitors) && mon->bar ? &*mon->bar : nullptr;
}
static const struct wl_pointer_listener pointerListener = {
    .enter = [](void *sp, wl_pointer *pointer, uint32_t serial,
                wl_surface *surface, wl_fixed_t x, wl_fixed_t y)
    {
        auto& seat = *static_cast<Seat*>(sp);
        seat.pointer->focusedBar = barFromSurface(surface);
        if (!cursorImage) {
            auto cursorTheme = wl_cursor_theme_load(NULL, 24, shm);
            cursorImage = wl_cursor_theme_get_cursor(cursorTheme, "left_ptr")->images[0];
            cursorSurface = wl_compositor_create_surface(compositor);
            wl_surface_attach(cursorSurface, wl_cursor_image_get_buffer(cursorImage), 0, 0);
            wl_surface_commit(cursorSurface);
        }
        wl_pointer_set_cursor(pointer, serial, cursorSurface,
            cursorImage->hotspot_x, cursorImage->hotspot_y);
    },
    .leave = [](void *sp, wl_pointer*, uint32_t serial, wl_surface*) {
        auto& seat = *static_cast<Seat*>(sp);
        seat.pointer->focusedBar = nullptr;
    },
    .motion = [](void *sp, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
        auto& seat = *static_cast<Seat*>(sp);
        seat.pointer->x = wl_fixed_to_int(x);
        seat.pointer->y = wl_fixed_to_int(y);
    },
    .button = [](void *sp, wl_pointer*, uint32_t, uint32_t, uint32_t button, uint32_t pressed) {
        auto& seat = *static_cast<Seat*>(sp);
        auto it = std::find(begin(seat.pointer->btns), end(seat.pointer->btns), button);
        if (pressed == WL_POINTER_BUTTON_STATE_PRESSED && it == end(seat.pointer->btns)) {
            seat.pointer->btns.push_back(button);
        } else if (pressed == WL_POINTER_BUTTON_STATE_RELEASED && it != end(seat.pointer->btns)) {
            seat.pointer->btns.erase(it);
        }
    },
    .axis = [](void *sp, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) { },
    .frame = [](void *sp, wl_pointer*) {
        auto& seat = *static_cast<Seat*>(sp);
        if (!seat.pointer->focusedBar) return;
        for (auto btn : seat.pointer->btns) {
            seat.pointer->focusedBar->click(seat.pointer->x, seat.pointer->y, btn);
        }
        seat.pointer->btns.clear();
    },
    .axis_source = [](void*, wl_pointer*, uint32_t) { },
    .axis_stop = [](void*, wl_pointer*, uint32_t, uint32_t) { },
    .axis_discrete = [](void*, wl_pointer*, uint32_t, int32_t) { },
};

static const struct wl_seat_listener seatListener = {
    .capabilities = [](void *sp, wl_seat*, uint32_t cap)
    {
        auto& seat = *static_cast<Seat*>(sp);
        auto hasPointer = cap & WL_SEAT_CAPABILITY_POINTER;
        if (!seat.pointer && hasPointer) {
            auto &pointer = seat.pointer.emplace();
            pointer.wlPointer = wl_unique_ptr<wl_pointer> {wl_seat_get_pointer(seat.wlSeat.get())};
            wl_pointer_add_listener(seat.pointer->wlPointer.get(), &pointerListener, &seat);
        } else if (seat.pointer && !hasPointer) {
            seat.pointer.reset();
        }
    },
    .name = [](void*, wl_seat*, const char *name) { }
};

static const struct znet_tapesoftware_dwl_wm_v1_listener dwlWmListener = {
    .tag = [](void*, znet_tapesoftware_dwl_wm_v1*, const char *name) {
        tagNames.push_back(name);
    },
    .layout = [](void*, znet_tapesoftware_dwl_wm_v1*, const char *name) {
        layoutNames.push_back(name);
    },
};

static const struct znet_tapesoftware_dwl_wm_monitor_v1_listener dwlWmMonitorListener {
    .selected = [](void *mv, znet_tapesoftware_dwl_wm_monitor_v1*, uint32_t selected) {
        auto mon = static_cast<Monitor*>(mv);
        mon->bar->setSelected(selected);
    },
    .tag = [](void *mv, znet_tapesoftware_dwl_wm_monitor_v1*, uint32_t tag, uint32_t state, uint32_t numClients, int32_t focusedClient) {
        auto mon = static_cast<Monitor*>(mv);
        mon->bar->setTag(tag, static_cast<znet_tapesoftware_dwl_wm_monitor_v1_tag_state>(state), numClients, focusedClient);
    },
    .layout = [](void *mv, znet_tapesoftware_dwl_wm_monitor_v1*, uint32_t layout) {
        auto mon = static_cast<Monitor*>(mv);
        mon->bar->setLayout(layout);
    },
    .title = [](void *mv, znet_tapesoftware_dwl_wm_monitor_v1*, const char *title) {
        auto mon = static_cast<Monitor*>(mv);
        mon->bar->setTitle(title);
    },
    .frame = [](void *mv, znet_tapesoftware_dwl_wm_monitor_v1*) {
        auto mon = static_cast<Monitor*>(mv);
        if (mon->created) {
            mon->bar->invalidate();
        } else {
            mon->bar->create(mon->wlOutput.get());
            mon->created = true;
        }
    }
};

static void setupMonitor(Monitor &monitor) {
    monitor.dwlMonitor.reset(znet_tapesoftware_dwl_wm_v1_get_monitor(dwlWm, monitor.wlOutput.get()));
    monitor.bar.emplace(&monitor);
    monitor.bar->setStatus(lastStatus);
    znet_tapesoftware_dwl_wm_monitor_v1_add_listener(monitor.dwlMonitor.get(), &dwlWmMonitorListener, &monitor);
}

// called after we have received the initial batch of globals
static void onReady()
{
    requireGlobal(compositor, "wl_compositor");
    requireGlobal(shm, "wl_shm");
    requireGlobal(wlrLayerShell, "zwlr_layer_shell_v1");
    requireGlobal(dwlWm, "znet_tapesoftware_dwl_wm_v1");
    setupStatusFifo();
    wl_display_roundtrip(display); // roundtrip so we receive all dwl tags etc.
    ready = true;
    for (auto& monitor : monitors) {
        setupMonitor(monitor);
    }
}

static void setupStatusFifo()
{
    for (auto i=0; i<100; i++) {
        auto path = std::string{getenv("XDG_RUNTIME_DIR")} + "/somebar-" + std::to_string(i);
        auto result = mkfifo(path.c_str(), 0666);
        if (result == 0) {
            auto fd = open(path.c_str(), O_CLOEXEC | O_NONBLOCK | O_RDONLY);
            if (fd == -1) {
                perror("open status fifo reader");
                cleanup();
                exit(1);
            }
            statusFifoName = path;
            statusFifoFd = fd;

            fd = open(path.c_str(), O_CLOEXEC | O_WRONLY);
            if (fd == -1) {
                perror("open status fifo writer");
                cleanup();
                exit(1);
            }
            statusFifoWriter = fd;

            auto statusNotifier = new QSocketNotifier(statusFifoFd, QSocketNotifier::Read);
            statusNotifier->setEnabled(true);
            QObject::connect(statusNotifier, &QSocketNotifier::activated, onStatus);
            return;
        } else if (errno != EEXIST) {
            perror("mkfifo");
        }
    }
}

static void onStatus()
{
    char buffer[512];
    auto n = read(statusFifoFd, buffer, sizeof(buffer));
    lastStatus = QString::fromUtf8(buffer, n);
    for (auto &monitor : monitors) {
        if (monitor.bar) {
            monitor.bar->setStatus(lastStatus);
            monitor.bar->invalidate();
        }
    }
}

struct HandleGlobalHelper {
    wl_registry *registry;
    uint32_t name;
    const char *interface;

    template<typename T>
    bool handle(T &store, const wl_interface &iface, int version) {
        if (strcmp(interface, iface.name)) return false;
        store = static_cast<T>(wl_registry_bind(registry, name, &iface, version));
        return true;
    }
};
static void registryHandleGlobal(void*, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    auto reg = HandleGlobalHelper { registry, name, interface };
    if (reg.handle(compositor, wl_compositor_interface, 4)) return;
    if (reg.handle(shm, wl_shm_interface, 1)) return;
    if (reg.handle(wlrLayerShell, zwlr_layer_shell_v1_interface, 4)) return;
    if (reg.handle(xdgWmBase, xdg_wm_base_interface, 2)) {
        xdg_wm_base_add_listener(xdgWmBase, &xdgWmBaseListener, nullptr);
        return;
    }
    if (reg.handle(dwlWm, znet_tapesoftware_dwl_wm_v1_interface, 1)) {
        znet_tapesoftware_dwl_wm_v1_add_listener(dwlWm, &dwlWmListener, nullptr);
        return;
    }
    if (wl_seat *wlSeat; reg.handle(wlSeat, wl_seat_interface, 7)) {
        auto& seat = seats.emplace_back(Seat {name, wl_unique_ptr<wl_seat> {wlSeat}});
        wl_seat_add_listener(wlSeat, &seatListener, &seat);
        return;
    }
    if (wl_output *output; reg.handle(output, wl_output_interface, 1)) {
        auto& m = monitors.emplace_back(Monitor {name, wl_unique_ptr<wl_output> {output}});
        if (ready) {
            setupMonitor(m);
        }
        return;
    }
}
static void registryHandleRemove(void*, wl_registry *registry, uint32_t name)
{
    monitors.remove_if([name](const Monitor &mon) { return mon.name == name; });
    seats.remove_if([name](const Seat &seat) { return seat.name == name; });
}
static const struct wl_registry_listener registry_listener = {
    .global = registryHandleGlobal,
    .global_remove = registryHandleRemove,
};

int main(int argc, char **argv)
{
    static sigset_t blockedsigs;
    sigemptyset(&blockedsigs);
    sigaddset(&blockedsigs, SIGINT);
    sigaddset(&blockedsigs, SIGTERM);
    sigprocmask(SIG_BLOCK, &blockedsigs, nullptr);

    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("tape software");
    QCoreApplication::setOrganizationDomain("tapesoftware.net");
    QCoreApplication::setApplicationName("somebar");

    int sfd = signalfd(-1, &blockedsigs, SFD_CLOEXEC | SFD_NONBLOCK);
    if (sfd < 0) {
        perror("signalfd");
        cleanup();
        exit(1);
    }
    QSocketNotifier signalNotifier {sfd, QSocketNotifier::Read};
    QObject::connect(&signalNotifier, &QSocketNotifier::activated, []() { quitting = true; });

    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }

    auto registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, nullptr);
    wl_display_roundtrip(display);
    onReady();

    QSocketNotifier displayReadNotifier(wl_display_get_fd(display), QSocketNotifier::Read);
    displayReadNotifier.setEnabled(true);
    QObject::connect(&displayReadNotifier, &QSocketNotifier::activated, [=]() {
        auto res = wl_display_dispatch(display);
        if (res < 0) {
            perror("wl_display_dispatch");
            cleanup();
            exit(1);
        }
    });
    displayWriteNotifier = new QSocketNotifier(wl_display_get_fd(display), QSocketNotifier::Write);
    displayWriteNotifier->setEnabled(false);
    QObject::connect(displayWriteNotifier, &QSocketNotifier::activated, waylandWriteReady);

    while (!quitting) {
        waylandFlush();
        app.processEvents(QEventLoop::WaitForMoreEvents);
    }
    cleanup();
}

void cleanup() {
    if (!statusFifoName.empty()) {
        unlink(statusFifoName.c_str());
    }
}

void waylandFlush()
{
    wl_display_dispatch_pending(display);
    if (wl_display_flush(display) < 0 && errno == EAGAIN) {
        displayWriteNotifier->setEnabled(true);
    }
}
static void waylandWriteReady() 
{
    displayWriteNotifier->setEnabled(false);
    waylandFlush();
}
static void requireGlobal(const void *p, const char *name)
{
    if (p) return;
    fprintf(stderr, "Wayland compositor does not export required global %s, aborting.\n", name);
    cleanup();
    exit(1);
}
