// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include "common.hpp"

constexpr int barSize = 26;
constexpr bool topbar = 1;
constexpr int paddingX = 10;
constexpr int paddingY = 3;

constexpr ColorScheme colorInactive = {QColor(255, 255, 255), QColor(0, 0, 0)};
constexpr ColorScheme colorActive = {QColor(255, 255, 255), QColor(0, 0, 255)};
