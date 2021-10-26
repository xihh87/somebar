// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include "common.hpp"

constexpr bool topbar = true;

constexpr int paddingX = 10;
constexpr int paddingY = 3;

constexpr const char *fontFamily = "Source Sans Pro";
constexpr int fontSizePt = 12;
constexpr bool fontBold = false;

constexpr ColorScheme colorInactive = {QColor(0xbb, 0xbb, 0xbb), QColor(0x22, 0x22, 0x22)};
constexpr ColorScheme colorActive = {QColor(0xee, 0xee, 0xee), QColor(0x00, 0x55, 0x77)};
constexpr ColorScheme colorUrgent = {colorActive.bg, colorActive.fg};

constexpr Button buttons[] = {
    { Control::TagBar,       0, BTN_LEFT,   toggleview, {0} },
    { Control::TagBar,       0, BTN_MIDDLE, view,       {0} },
    //{ Control::TagBar, 0, BTN_RIGHT, tag, {0} },
    { Control::LayoutSymbol, 0, BTN_LEFT,   setlayout,  {.ui = 0} },
    { Control::LayoutSymbol, 0, BTN_RIGHT,  setlayout,  {.ui = 2} },
};
