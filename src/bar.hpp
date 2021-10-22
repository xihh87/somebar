// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <optional>
#include <vector>
#include <wayland-client.h>
#include <QString>
#include <QFontMetrics>
#include <QPainter>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "common.hpp"
#include "shm_buffer.hpp"

struct Tag {
    QString name;
    bool active;
};

class Bar {
    static const zwlr_layer_surface_v1_listener _layerSurfaceListener;
    static const wl_callback_listener _frameListener;

    wl_surface *_surface {nullptr};
    zwlr_layer_surface_v1 *_layerSurface {nullptr};
    QPainter *_painter {nullptr};
    QFont _font;
    QFontMetrics _fontMetrics;
    std::optional<ShmBuffer> _bufs;
    int _textY, _x;
    bool _invalid {false};

    std::vector<Tag> _tags;
    QString _windowTitle;
    QString _status;

    void layerSurfaceConfigure(uint32_t serial, uint32_t width, uint32_t height);
    void render();
    void renderTags();
    void renderStatus();
    void renderText(const QString &text);
    int textWidth(const QString &text);
    void setColorScheme(const ColorScheme &scheme);
    void invalidate();
public:
    explicit Bar(const wl_output *output);
    void setStatus(const QString &status);
    ~Bar();
};
