// somebar - dwl bar
// See LICENSE file for copyright and license details.

#include <sys/mman.h>
#include <unistd.h>
#include "shm_buffer.hpp"
#include "common.hpp"

constexpr int n = 2;

ShmBuffer::ShmBuffer(int w, int h, wl_shm_format format)
    : width(w)
    , height(h)
    , stride(w*4)
{
    auto oneSize = stride*h;
    _totalSize = oneSize * n;
    auto fd = memfd_create("wl_shm", MFD_CLOEXEC);
    ftruncate(fd, _totalSize);
    auto pool = wl_shm_create_pool(shm, fd, _totalSize);
    auto ptr = reinterpret_cast<char*>(mmap(nullptr, _totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    close(fd);
    for (auto i=0; i<n; i++) {
        auto offset = oneSize*i;
        auto buffer = wl_shm_pool_create_buffer(pool, offset, width, height, stride, format);
        _buffers[i] = { ptr+offset, buffer };
    }
    wl_shm_pool_destroy(pool);
}

ShmBuffer::~ShmBuffer()
{
    munmap(_buffers[0].data, _totalSize);
    for (auto i=0; i<n; i++) {
        wl_buffer_destroy(_buffers[i].buffer);
    }
    waylandFlush();
}

char* ShmBuffer::data() const { return _buffers[_current].data; }
wl_buffer* ShmBuffer::buffer() const { return _buffers[_current].buffer; }
void ShmBuffer::flip() { _current = 1-_current; }
