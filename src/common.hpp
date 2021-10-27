// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <memory>
#include <string>
#include <vector>
#include <wayland-client.h>
#include <linux/input-event-codes.h>
#include <cairo/cairo.h>
#include <pango/pango.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "net-tapesoftware-dwl-wm-unstable-v1-client-protocol.h"

struct Color {
    Color() {}
    constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255) : r(r), g(g), b(b), a(a) { }
    uint8_t r, g, b, a {255};
};
struct ColorScheme {
    Color fg, bg;
};
union Arg {
	unsigned int ui;
	const void *v;
};
struct Monitor;

enum { ClkNone, ClkTagBar, ClkLayoutSymbol, ClkWinTitle, ClkStatusText };
struct Button {
	int control;
	int btn; // <linux/input-event-codes.h>
	void (*func)(Monitor &mon, const Arg &arg);
	const Arg arg;
};

extern wl_display *display;
extern wl_compositor *compositor;
extern wl_shm *shm;
extern zwlr_layer_shell_v1 *wlrLayerShell;
extern std::vector<std::string> tagNames;
extern std::vector<std::string> layoutNames;

void view(Monitor &m, const Arg &arg);
void toggleview(Monitor &m, const Arg &arg);
void setlayout(Monitor &m, const Arg &arg);
void tag(Monitor &m, const Arg &arg);
void toggletag(Monitor &m, const Arg &arg);
void spawn(Monitor&, const Arg &arg);

// wayland smart pointers
template<typename T>
struct wl_deleter;
#define WL_DELETER(type, fn) template<> struct wl_deleter<type> { void operator()(type *v) { if(v) fn(v); } }

template<typename T>
using wl_unique_ptr = std::unique_ptr<T, wl_deleter<T>>;

WL_DELETER(wl_buffer, wl_buffer_destroy);
WL_DELETER(wl_output, wl_output_release);
WL_DELETER(wl_pointer, wl_pointer_release);
WL_DELETER(wl_seat, wl_seat_release);
WL_DELETER(wl_surface, wl_surface_destroy);
WL_DELETER(znet_tapesoftware_dwl_wm_monitor_v1, znet_tapesoftware_dwl_wm_monitor_v1_release);
WL_DELETER(zwlr_layer_surface_v1, zwlr_layer_surface_v1_destroy);

WL_DELETER(cairo_t, cairo_destroy);
WL_DELETER(cairo_surface_t, cairo_surface_destroy);

WL_DELETER(PangoContext, g_object_unref);
WL_DELETER(PangoLayout, g_object_unref);
