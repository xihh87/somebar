# somebar - dwl-like bar for dwm

![Screenshot](screenshot.png)

This project is rather new. Beware of bugs.

## Dependencies

* c++ compiler, meson, and ninja
* wayland-scanner
* libwayland-client
* libwayland-cursor
* libcairo
* libpango
* libpangocairo

```
sudo apt install build-essential meson ninja \
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

somebar - dwl-like bar for dwm  
Copyright (C) 2021 Raphael Robatsch

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
