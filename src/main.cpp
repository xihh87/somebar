// somebar - dwl bar
// See LICENSE file for copyright and license details.

#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <QApplication>
#include <QSocketNotifier>
#include <wayland-client.h>
#include "qnamespace.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "common.hpp"
#include "bar.hpp"

static void waylandFlush();
static void waylandWriteReady();
static void requireGlobal(const void *p, const char *name);

wl_display *display;
wl_compositor *compositor;
wl_shm *shm;
zwlr_layer_shell_v1 *wlrLayerShell;
static QSocketNotifier *displayWriteNotifier;

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
    std::ignore = new Bar(nullptr);
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
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("tape software");
    QCoreApplication::setOrganizationDomain("tapesoftware.net");
    QCoreApplication::setApplicationName("somebar");

    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }

    auto registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, nullptr);

    auto initialSync = wl_display_sync(display);
    auto initialSyncListener = wl_callback_listener {
        [](void*, wl_callback *cb, uint32_t) {
            onReady();
            wl_callback_destroy(cb);
        }
    };
    wl_callback_add_listener(initialSync, &initialSyncListener, nullptr);

    QSocketNotifier displayReadNotifier(wl_display_get_fd(display), QSocketNotifier::Read);
    displayReadNotifier.setEnabled(true);
    QObject::connect(&displayReadNotifier, &QSocketNotifier::activated, [=]() { wl_display_dispatch(display); });
    displayWriteNotifier = new QSocketNotifier(wl_display_get_fd(display), QSocketNotifier::Write);
    displayWriteNotifier->setEnabled(false);
    QObject::connect(displayWriteNotifier, &QSocketNotifier::activated, waylandWriteReady);

    while (true) {
        waylandFlush();
        app.processEvents(QEventLoop::WaitForMoreEvents);
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
    exit(1);
}
