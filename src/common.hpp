// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

void waylandFlush();

extern wl_display *display;
extern wl_compositor *compositor;
extern wl_shm *shm;
extern zwlr_layer_shell_v1 *wlrLayerShell;
