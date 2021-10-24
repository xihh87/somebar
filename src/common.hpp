// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <wayland-client.h>
#include <vector>
#include <QColor>
#include <QString>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

extern wl_display *display;
extern wl_compositor *compositor;
extern wl_shm *shm;
extern zwlr_layer_shell_v1 *wlrLayerShell;
extern std::vector<QString> tagNames;

struct ColorScheme {
    QColor fg, bg;
};
