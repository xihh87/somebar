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
static QFont font = getFont();
static QFontMetrics fontMetrics = QFontMetrics {font};

const wl_surface* Bar::surface() const { return _surface.get(); }

Bar::Bar(Monitor *mon)
{
    _mon = mon;
    for (auto i=0u; i<tagNames.size(); i++) {
        _tags.push_back({ ZNET_TAPESOFTWARE_DWL_WM_MONITOR_V1_TAG_STATE_NONE, 0, 0, 0 });
    }
}

void Bar::create(wl_output *output)
{
    _surface.reset(wl_compositor_create_surface(compositor));
    _layerSurface.reset(zwlr_layer_shell_v1_get_layer_surface(wlrLayerShell,
        _surface.get(), nullptr, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, "net.tapesoftware.Somebar"));
    zwlr_layer_surface_v1_add_listener(_layerSurface.get(), &_layerSurfaceListener, this);
    auto anchor = topbar ? ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP : ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
    zwlr_layer_surface_v1_set_anchor(_layerSurface.get(),
        anchor | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

    auto barSize = fontMetrics.ascent() + fontMetrics.descent() + paddingY * 2;
    _textY = fontMetrics.ascent() + paddingY;

    zwlr_layer_surface_v1_set_size(_layerSurface.get(), 0, barSize);
    zwlr_layer_surface_v1_set_exclusive_zone(_layerSurface.get(), barSize);
    wl_surface_commit(_surface.get());
}

void Bar::click(int x, int, int btn)
{
    Arg arg = {0};
    Arg *argp = nullptr;
    int control = ClkNone;
    if (x > _statusX) {
        control = ClkStatusText;
    } else if (x > _titleX) {
        control = ClkWinTitle;
    } else if (x > _layoutX) {
        control = ClkLayoutSymbol;
    } else for (auto tag = _tags.size()-1; tag >= 0; tag--) {
        if (x > _tags[tag].x) {
            control = ClkTagBar;
            arg.ui = 1<<tag;
            argp = &arg;
            break;
        }
    }
    for (auto i = 0u; i < sizeof(buttons)/sizeof(buttons[0]); i++) {
        const auto& button = buttons[i];
        if (button.control == control && button.btn == btn) {
            button.func(*_mon, *(argp ? argp : &button.arg));
            return;
        }
    }
}

void Bar::invalidate()
{
    if (_invalid) return;
    _invalid = true;
    auto frame = wl_surface_frame(_surface.get());
    wl_callback_add_listener(frame, &_frameListener, this);
    wl_surface_commit(_surface.get());
}

void Bar::setTag(int tag, znet_tapesoftware_dwl_wm_monitor_v1_tag_state state, int numClients, int focusedClient)
{
    auto& t = _tags[tag];
    t.state = state;
    t.numClients = numClients;
    t.focusedClient = focusedClient;
}

void Bar::setSelected(bool selected) { _selected = selected; }
void Bar::setLayout(int layout) { _layout = layout; }
void Bar::setTitle(const char *title) { _title = title; }
void Bar::setStatus(const QString &status) { _status = status; }

void Bar::layerSurfaceConfigure(uint32_t serial, uint32_t width, uint32_t height)
{
    zwlr_layer_surface_v1_ack_configure(_layerSurface.get(), serial);
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
    painter.setFont(font);

    renderTags();
    setColorScheme(_selected ? colorActive : colorInactive);
    _layoutX = _x;
    renderText(layoutNames[_layout]);
    _titleX = _x;
    renderText(_title);
    renderStatus();
    
    _painter = nullptr;
    wl_surface_attach(_surface.get(), _bufs->buffer(), 0, 0);
    wl_surface_damage(_surface.get(), 0, 0, INT_MAX, INT_MAX);
    wl_surface_commit(_surface.get());
    _bufs->flip();
    _invalid = false;
}

void Bar::setColorScheme(const ColorScheme &scheme, bool invert)
{
    _painter->setBrush(QBrush {invert ? scheme.fg : scheme.bg});
    _painter->setPen(QPen {QBrush {invert ? scheme.bg : scheme.fg}, 1});
}

void Bar::renderTags()
{
    for (auto i=0u; i<_tags.size(); i++) {
        auto& tag = _tags[i];
        tag.x = _x;
        setColorScheme(
            tag.state & ZNET_TAPESOFTWARE_DWL_WM_MONITOR_V1_TAG_STATE_ACTIVE ? colorActive : colorInactive,
            tag.state & ZNET_TAPESOFTWARE_DWL_WM_MONITOR_V1_TAG_STATE_URGENT);
        renderText(tagNames[i]);
        auto indicators = qMin(tag.numClients, _bufs->height/2);
        for (auto ind = 0; ind < indicators; ind++) {
            if (ind == tag.focusedClient) {
                _painter->drawLine(tag.x, ind*2, tag.x+5, ind*2);
            } else {
                _painter->drawPoint(tag.x, ind*2);
            }
        }
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
    _painter->fillRect(_x, 0, _bufs->width-_x, _bufs->height, _painter->brush());
    auto size = textWidth(_status) + paddingX;
    _statusX = _bufs->width - size;
    _painter->drawText(paddingX+_statusX, _textY, _status);
    _x = _bufs->width;
}

int Bar::textWidth(const QString &text)
{
    return fontMetrics.size(Qt::TextSingleLine, text).width();
}
