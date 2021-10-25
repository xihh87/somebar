// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <array>
#include <wayland-client.h>
#include "common.hpp"

// double buffered shm
// format is must be 32-bit
class ShmBuffer {
    struct Buf {
        uint8_t *data {nullptr};
        wl_unique_ptr<wl_buffer> buffer;
    };
    std::array<Buf, 2> _buffers;
    int _current {0};
    size_t _totalSize {0};
public:
    int width, height, stride;

    explicit ShmBuffer(int width, int height, wl_shm_format format);
    ShmBuffer(const ShmBuffer&) = delete;
    ShmBuffer(ShmBuffer&&) = default;
    ShmBuffer& operator=(const ShmBuffer&) = delete;
    ShmBuffer& operator=(ShmBuffer&&) = default;
    ~ShmBuffer();

    uint8_t* data() const;
    wl_buffer* buffer() const;
    void flip();
};
