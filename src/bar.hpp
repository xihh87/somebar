// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <optional>
#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "common.hpp"
#include "shm_buffer.hpp"

class Bar {
    static const zwlr_layer_surface_v1_listener _layerSurfaceListener;

    wl_surface *_surface {nullptr};
    zwlr_layer_surface_v1 *_layerSurface {nullptr};
    std::optional<ShmBuffer> _bufs;

    void layerSurfaceConfigure(uint32_t serial, uint32_t width, uint32_t height);
public:
    explicit Bar(const wl_output *output);
    ~Bar();
};
