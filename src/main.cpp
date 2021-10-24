// somebar - dwl bar
// See LICENSE file for copyright and license details.

#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/input-event-codes.h>
#include <optional>
#include <QGuiApplication>
#include <QSocketNotifier>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "net-tapesoftware-dwl-wm-unstable-v1-client-protocol.h"
#include "common.hpp"
#include "bar.hpp"

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
static std::optional<Bar> bar;
static std::string statusFifoName;
static int statusFifoFd {-1};
static int statusFifoWriter {-1};
static QSocketNotifier *displayWriteNotifier;
static bool quitting {false};

static xdg_wm_base *xdgWmBase;
static const struct xdg_wm_base_listener xdgWmBaseListener = {
    [](void*, xdg_wm_base *sender, uint32_t serial) {
        xdg_wm_base_pong(sender, serial);
    }
};

struct PointerState {
    wl_pointer *pointer;
    wl_surface *cursorSurface;
    wl_cursor_image *cursorImage;
    Bar *focusedBar;
    int x, y;
    bool leftButtonClick;
};
static PointerState pointerState;
static const struct wl_pointer_listener pointerListener = {
    .enter = [](void*, wl_pointer *pointer, uint32_t serial,
                wl_surface*, wl_fixed_t x, wl_fixed_t y)
    {
        pointerState.focusedBar = &bar.value();
        wl_pointer_set_cursor(pointer, serial, pointerState.cursorSurface,
            pointerState.cursorImage->hotspot_x, pointerState.cursorImage->hotspot_y);
    },
    .leave = [](void*, wl_pointer*, uint32_t serial, wl_surface*) {
        pointerState.focusedBar = nullptr;
    },
    .motion = [](void*, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
        pointerState.x = wl_fixed_to_int(x);
        pointerState.y = wl_fixed_to_int(y);
    },
    .button = [](void*, wl_pointer*, uint32_t, uint32_t, uint32_t button, uint32_t pressed) {
        if (button == BTN_LEFT) {
            pointerState.leftButtonClick = pressed == WL_POINTER_BUTTON_STATE_PRESSED;
        }
    },
    .axis = [](void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t) { },
    .frame = [](void*, wl_pointer*) {
        if (!pointerState.focusedBar) return;
        if (pointerState.leftButtonClick) {
            pointerState.leftButtonClick = false;
            pointerState.focusedBar->click(pointerState.x, pointerState.y);
        }
    },
    .axis_source = [](void*, wl_pointer*, uint32_t) { },
    .axis_stop = [](void*, wl_pointer*, uint32_t, uint32_t) { },
    .axis_discrete = [](void*, wl_pointer*, uint32_t, int32_t) { },
};

static wl_seat *seat;
static const struct wl_seat_listener seatListener = {
    [](void*, wl_seat*, uint32_t cap)
    {
        if (!pointerState.pointer && WL_SEAT_CAPABILITY_POINTER) {
            auto cursorTheme = wl_cursor_theme_load(NULL, 24, shm);
            auto cursorImage = wl_cursor_theme_get_cursor(cursorTheme, "left_ptr")->images[0];
            pointerState.cursorImage = cursorImage;
            pointerState.cursorSurface = wl_compositor_create_surface(compositor);
            wl_surface_attach(pointerState.cursorSurface,
                wl_cursor_image_get_buffer(cursorImage), 0, 0);
            wl_surface_commit(pointerState.cursorSurface);
            pointerState.pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(pointerState.pointer, &pointerListener, nullptr);
        }
    },
    [](void*, wl_seat*, const char *name) { }
};

static const struct znet_tapesoftware_dwl_wm_v1_listener dwlWmListener = {
    .tag = [](void*, znet_tapesoftware_dwl_wm_v1*, const char *name) {
        tagNames.push_back(name);
    },
};

// called after we have received the initial batch of globals
static void onReady()
{
    requireGlobal(compositor, "wl_compositor");
    requireGlobal(shm, "wl_shm");
    requireGlobal(seat, "wl_seat");
    requireGlobal(wlrLayerShell, "zwlr_layer_shell_v1");
    setupStatusFifo();
    bar.emplace(nullptr);
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
    auto str = QString::fromUtf8(buffer, n);
    if (bar) {
        bar->setStatus(str);
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
    printf("got global: %s v%d\n", interface, version);
    if (reg.handle(compositor, wl_compositor_interface, 4)) return;
    if (reg.handle(shm, wl_shm_interface, 1)) return;
    if (reg.handle(wlrLayerShell, zwlr_layer_shell_v1_interface, 4)) return;
    if (reg.handle(xdgWmBase, xdg_wm_base_interface, 2)) {
        xdg_wm_base_add_listener(xdgWmBase, &xdgWmBaseListener, nullptr);
        return;
    }
    if (seat == nullptr && reg.handle(seat, wl_seat_interface, 7)) {
        wl_seat_add_listener(seat, &seatListener, nullptr);
    }
    if (reg.handle(dwlWm, znet_tapesoftware_dwl_wm_v1_interface, 1)) {
        znet_tapesoftware_dwl_wm_v1_add_listener(dwlWm, &dwlWmListener, nullptr);
        wl_display_roundtrip(display);
    }
}
static const struct wl_registry_listener registry_listener = { registryHandleGlobal, nullptr };

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
    QObject::connect(&displayReadNotifier, &QSocketNotifier::activated, [=]() { wl_display_dispatch(display); });
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
