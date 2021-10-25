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
