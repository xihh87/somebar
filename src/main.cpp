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
#include <QGuiApplication>
#include <QSocketNotifier>
#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
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

// called after we have received the initial batch of globals
static void onReady()
{
    requireGlobal(compositor, "wl_compositor");
    requireGlobal(shm, "wl_shm");
    requireGlobal(wlrLayerShell, "zwlr_layer_shell_v1");
    setupStatusFifo();
    std::ignore = new Bar(nullptr);
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
