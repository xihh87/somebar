// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "common.hpp"
#include "shm_buffer.hpp"

class Bar {
    wl_surface *_surface {nullptr};
    zwlr_layer_surface_v1 *_layerSurface {nullptr};
    ShmBuffer _bufs;
public:
    explicit Bar(wl_output *output);
};
