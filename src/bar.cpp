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
const wl_callback_listener Bar::_frameListener = {
    [](void *owner, wl_callback *cb, uint32_t)
    {
        static_cast<Bar*>(owner)->render();
        wl_callback_destroy(cb);
    }
};

static QFont getFont()
{
    QFont font {fontFamily, fontSizePt};
    font.setBold(fontBold);
    return font;
}

Bar::Bar(const wl_output *output)
    : _font {getFont()}
    , _fontMetrics {_font}
{
    _surface = wl_compositor_create_surface(compositor);
    _layerSurface = zwlr_layer_shell_v1_get_layer_surface(wlrLayerShell,
        _surface, nullptr, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, "net.tapesoftware.Somebar");
    zwlr_layer_surface_v1_add_listener(_layerSurface, &_layerSurfaceListener, this);
    auto anchor = topbar ? ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP : ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    zwlr_layer_surface_v1_set_anchor(_layerSurface,
        anchor | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

    auto barSize = _fontMetrics.ascent() + _fontMetrics.descent() + paddingY * 2;
    _textY = _fontMetrics.ascent() + paddingY;

    zwlr_layer_surface_v1_set_size(_layerSurface, 0, barSize);
    zwlr_layer_surface_v1_set_exclusive_zone(_layerSurface, barSize);
    wl_surface_commit(_surface);

    for (auto i=1; i<=4; i++) {
        _tags.push_back({ QString::number(i), i%2 == 0 });
    }
    _windowTitle = "Window title";
    _status = "Status";
}

Bar::~Bar()
{
    wl_surface_destroy(_surface);
    zwlr_layer_surface_v1_destroy(_layerSurface);
}

void Bar::invalidate()
{
    if (_invalid) return;
    _invalid = true;
    auto frame = wl_surface_frame(_surface);
    wl_callback_add_listener(frame, &_frameListener, this);
    wl_surface_commit(_surface);
}

void Bar::setStatus(const QString &status)
{
    _status = status;
    invalidate();
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
    _x = 0;
    painter.setFont(_font);

    setColorScheme(colorActive);
    painter.fillRect(0, 0, img.width(), img.height(), painter.brush());
    renderTags();
    setColorScheme(colorActive);
    renderText(_windowTitle);
    renderStatus();
    
    _painter = nullptr;
    wl_surface_attach(_surface, _bufs->buffer(), 0, 0);
    wl_surface_damage(_surface, 0, 0, INT_MAX, INT_MAX);
    wl_surface_commit(_surface);
    _bufs->flip();
    _invalid = false;
}

void Bar::setColorScheme(const ColorScheme &scheme)
{
    _painter->setBrush(QBrush {scheme.bg});
    _painter->setPen(QPen {QBrush {scheme.fg}, 1});
}

void Bar::renderTags()
{
    for (const auto &tag : _tags) {
        setColorScheme(tag.active ? colorActive : colorInactive);
        renderText(tag.name);
    }
}

void Bar::renderText(const QString &text)
{
    auto size = textWidth(text) + paddingX*2;
    _painter->fillRect(_x, 0, size, _bufs->height, _painter->brush());
    _painter->drawText(paddingX+_x, _textY, text);
    _x += size;
}

void Bar::renderStatus()
{
    auto size = textWidth(_status) + paddingX*2;
    _x = _bufs->width - size;
    _painter->fillRect(_x, 0, size, _bufs->height, _painter->brush());
    _painter->drawText(paddingX+_x, _textY, _status);
    _x = _bufs->width;
}

int Bar::textWidth(const QString &text)
{
    return _fontMetrics.size(Qt::TextSingleLine, text).width();
}
