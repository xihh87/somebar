// somebar - dwl bar
// See LICENSE file for copyright and license details.

#include <QImage>
#include "bar.hpp"
#include "config.hpp"

const zwlr_layer_surface_v1_listener Bar::_layerSurfaceListener = {
    [](void *owner, zwlr_layer_surface_v1*, uint32_t serial, uint32_t width, uint32_t height)
    {
        static_cast<Bar*>(owner)->layerSurfaceConfigure(serial, width, height);
    }
};

Bar::Bar(const wl_output *output)
{
    _surface = wl_compositor_create_surface(compositor);
    _layerSurface = zwlr_layer_shell_v1_get_layer_surface(wlrLayerShell,
        _surface, nullptr, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, "net.tapesoftware.Somebar");
    zwlr_layer_surface_v1_add_listener(_layerSurface, &_layerSurfaceListener, this);
    auto anchor = topbar ? ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP : ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    zwlr_layer_surface_v1_set_anchor(_layerSurface,
        anchor | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_size(_layerSurface, 0, barSize);
    zwlr_layer_surface_v1_set_exclusive_zone(_layerSurface, barSize);
    wl_surface_commit(_surface);
}

Bar::~Bar()
{
    wl_surface_destroy(_surface);
    zwlr_layer_surface_v1_destroy(_layerSurface);
    waylandFlush();
}

void Bar::layerSurfaceConfigure(uint32_t serial, uint32_t width, uint32_t height)
{
    zwlr_layer_surface_v1_ack_configure(_layerSurface, serial);
    _bufs.emplace(width, height, WL_SHM_FORMAT_XRGB8888);
    auto root = _widget.root();
    root->setFixedSize(width, height);
    render();
}

void Bar::render()
{
    auto img = QImage {
        _bufs->data(),
        _bufs->width,
        _bufs->height,
        _bufs->stride,
        QImage::Format_RGBX8888
    };
    auto root = _widget.root();
    root->render(&img);
    wl_surface_attach(_surface, _bufs->buffer(), 0, 0);
    wl_surface_commit(_surface);
    _bufs->flip();
    waylandFlush();
}
