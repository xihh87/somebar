// somebar - dwl bar
// See LICENSE file for copyright and license details.

#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <QCoreApplication>
#include <QSocketNotifier>
#include <wayland-client.h>
#include "qnamespace.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "common.hpp"
#include "shm_buffer.hpp"

constexpr uint32_t barSize = 20;

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
        waylandFlush();
    }
};

// app globals
static wl_surface *surface;
static zwlr_layer_surface_v1 *layerSurface;
static const struct wl_surface_listener surfaceListener = {
    // todo
};
static ShmBuffer *xbuf;
static const struct zwlr_layer_surface_v1_listener layerSurfaceListener = {
    [](void*, zwlr_layer_surface_v1 *layerSurface, uint32_t serial, uint32_t width, uint32_t height) {
        zwlr_layer_surface_v1_ack_configure(layerSurface, serial);
        printf("configured to %d x %d\n", width, height);
        xbuf = new ShmBuffer(width, height, WL_SHM_FORMAT_XRGB8888);
        auto buffer = xbuf->data();

        auto w = 2*M_PI/(width / 10);
        for (auto x = 0; x < width; x++) {
            auto val = 255*(sin(x*w)/2+0.5);
            for (auto y = 0; y < height; y++) {
                auto p = &buffer[y*xbuf->stride+x*4];
                *p++ = 0;
                *p++ = 0;
                *p++ = val;
                *p++ = val;
            }
        }

        wl_surface_attach(surface, xbuf->buffer(), 0, 0);
        wl_surface_commit(surface);
        waylandFlush();
    }
};

// called after we have received the initial batch of globals
static void onReady()
{
    requireGlobal(compositor, "wl_compositor");
    requireGlobal(shm, "wl_shm");
    requireGlobal(wlrLayerShell, "zwlr_layer_shell_v1");
    surface = wl_compositor_create_surface(compositor);
    layerSurface = zwlr_layer_shell_v1_get_layer_surface(wlrLayerShell, surface, nullptr, ZWLR_LAYER_SHELL_V1_LAYER_TOP, "net.tapesoftware.Somebar");
    zwlr_layer_surface_v1_add_listener(layerSurface, &layerSurfaceListener, nullptr);
    zwlr_layer_surface_v1_set_anchor(layerSurface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_size(layerSurface, 0, barSize);
    zwlr_layer_surface_v1_set_exclusive_zone(layerSurface, barSize);
    wl_surface_commit(surface);
    waylandFlush();
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
    QCoreApplication app(argc, argv);
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
    waylandFlush();

    return app.exec();
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
