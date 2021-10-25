# somebar - dwl-like bar for dwm

This is extremely work in progress.

## Dependencies

* c++ compiler, meson, and ninja
* Qt 5 GUI
* libwayland
* wayland-scanner

    sudo apt install build-essential meson ninja qtbase5-dev libqt5core5a libqt5gui5 \
        libwayland-bin libwayland-client0 libwayland-cursor0 libwayland-dev

dwl must have the [wayland-ipc patch](https://gitlab.com/raphaelr/dwl/-/raw/master/patches/wayland-ipc.patch) applied,
since that's how the bar communicates with dwl.

## Configuration

src/config.hpp

## Building

    meson setup build
    ninja -C build

## Usage

somebar doesn't use dwl's status feature. You can start somebar using dwl's `-s` option,
but if you do, close stdout.

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
