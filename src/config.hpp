// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include "common.hpp"

constexpr bool topbar = 1;

constexpr int paddingX = 10;
constexpr int paddingY = 3;

constexpr const char *fontFamily = "Source Sans Pro";
constexpr int fontSizePt = 12;
constexpr bool fontBold = true;

constexpr ColorScheme colorInactive = {QColor(255, 255, 255), QColor(0, 0, 0)};
constexpr ColorScheme colorActive = {QColor(255, 255, 255), QColor(0, 0, 255)};
