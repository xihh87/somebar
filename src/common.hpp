// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <memory>
#include <vector>
#include <wayland-client.h>
#include <linux/input-event-codes.h>
#include <QColor>
#include <QString>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "net-tapesoftware-dwl-wm-unstable-v1-client-protocol.h"

struct ColorScheme {
    QColor fg, bg;
};
union Arg {
	int i;
	unsigned int ui;
	float f;
	const void *v;
};
struct Monitor;

extern wl_display *display;
extern wl_compositor *compositor;
extern wl_shm *shm;
extern zwlr_layer_shell_v1 *wlrLayerShell;
extern std::vector<QString> tagNames;
extern std::vector<QString> layoutNames;

void toggleview(Monitor &m, const Arg &arg);
void view(Monitor &m, const Arg &arg);
void setlayout(Monitor &m, const Arg &arg);

enum class Control { None, TagBar, LayoutSymbol, WinTitle, StatusText };
struct Button {
	Control control;
	unsigned int modifiers; // todo xkbcommon
	int btn; // <linux/input-event-codes.h>
	void (*func)(Monitor &mon, const Arg &arg);
	const Arg arg;
};

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
