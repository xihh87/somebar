// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <array>
#include <wayland-client.h>

// double buffered shm
// format is must be 32-bit
class ShmBuffer {
    struct Buf {
        char *data {nullptr};
        wl_buffer *buffer {nullptr};
    };
    std::array<Buf, 2> _buffers;
    int _current {0};
    size_t _totalSize {0};
public:
    int width, height, stride;
    explicit ShmBuffer(int width, int height, wl_shm_format format);
    char* data() const;
    wl_buffer* buffer() const;
    void flip();
    ~ShmBuffer();
};
