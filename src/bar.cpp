// somebar - dwl bar
// See LICENSE file for copyright and license details.

#include <QColor>
#include <QImage>
#include <QPainter>
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

    for (auto i=1; i<=4; i++) {
        _tags.push_back({ QString::number(i), i%2 == 0 });
    }
}

Bar::~Bar()
{
    wl_surface_destroy(_surface);
    zwlr_layer_surface_v1_destroy(_layerSurface);
}

void Bar::layerSurfaceConfigure(uint32_t serial, uint32_t width, uint32_t height)
{
    zwlr_layer_surface_v1_ack_configure(_layerSurface, serial);
    _bufs.emplace(width, height, WL_SHM_FORMAT_XRGB8888);
    render();
}

void Bar::render()
{
    auto img = QImage {
        _bufs->data(),
        _bufs->width,
        _bufs->height,
        _bufs->stride,
        QImage::Format_ARGB32
    };
    auto painter = QPainter {&img};
    _painter = &painter;
    auto font = painter.font();
    font.setBold(true);
    font.setPixelSize(18);
    painter.setFont(font);

    setColorScheme(colorActive);
    painter.fillRect(0, 0, img.width(), img.height(), painter.brush());
    _fontMetrics.emplace(painter.font());
    _textY = _fontMetrics->ascent() + paddingY;
    renderTags(painter);
    
    _painter = nullptr;
    wl_surface_attach(_surface, _bufs->buffer(), 0, 0);
    wl_surface_commit(_surface);
    _bufs->flip();
}

void Bar::setColorScheme(const ColorScheme &scheme)
{
    _painter->setBrush(QBrush {scheme.bg});
    _painter->setPen(QPen {QBrush {scheme.fg}, 1});
}

void Bar::renderTags(QPainter &painter)
{
    auto x = 0;
    for (const auto &tag : _tags) {
        auto size = textWidth(tag.name) + paddingX*2;
        setColorScheme(tag.active ? colorActive : colorInactive);
        painter.fillRect(x, 0, size, barSize, _painter->brush());
        painter.drawText(paddingX+x, _textY, tag.name);
        x += size;
    }
}

int Bar::textWidth(const QString &text)
{
    return _fontMetrics->size(Qt::TextSingleLine, text).width();
}
