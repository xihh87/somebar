# somebar - dwm-like bar for dwl

![Screenshot](screenshot.png)

This project is rather new. Beware of bugs.

The mailing list for this project is
[~raphi/somedesktop@lists.sr.ht](mailto:~raphi/somedesktop@lists.sr.ht).

## Dependencies

* c++ compiler, meson, and ninja
* wayland-scanner
* libwayland-client
* libwayland-cursor
* libcairo
* libpango
* libpangocairo

```
sudo apt install build-essential meson ninja-build \
    libwayland-bin libwayland-client0 libwayland-cursor0 libwayland-dev \
    libcairo2 libcairo2-dev \
    libpango-1.0-0 libpango1.0-dev libpangocairo-1.0-0
```

**dwl must have the [wayland-ipc patch](https://gitlab.com/raphaelr/dwl/-/raw/master/patches/wayland-ipc.patch) applied**,
since that's how the bar communicates with dwl.

## Configuration

Copy `src/config.def.hpp` to `src/config.hpp`, and adjust if needed.

## Building

    meson setup build
    ninja -C build
    ./build/somebar

## Usage

somebar doesn't use dwl's status feature. You can start somebar using dwl's `-s` option,
but if you do, close stdin, as somebar will not read from it.

Somebar can be controlled by writing to `$XDG_RUNTIME_DIR/somebar-0`. The following
commands are supported:

* `status TEXT`: Updates the status bar
* `hide MONITOR` Hides somebar on the specified monitor
* `shows MONITOR` Shows somebar on the specified monitor
* `toggle MONITOR` Toggles somebar on the specified monitor

MONITOR is an zxdg_output_v1 name, which can be determined e.g. using `weston-info`.
Additionally, MONITOR can be `all` (all monitors) or `selected` (the monitor with focus).

Commands can be sent either by writing to the file name above, or equivalently by calling
somebar with the `-c` argument. For example: `somebar -c toggle all`. This is recommended
for shell scripts, as there is no race-free way to write to a file only if it exists.

## License

somebar - dwm-like bar for dwl  
Copyright (c) 2021 Raphael Robatsch

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
