// somebar - dwl bar
// See LICENSE file for copyright and license details.

#include "bar_widget.hpp"

BarWidget::BarWidget()
    : _box {QBoxLayout::LeftToRight, &_root}
    , _label {&_root}
{
    _root.setAttribute(Qt::WA_DontShowOnScreen);
    _label.setText("somebar");
}

BarWidget::~BarWidget()
{
}

QWidget* BarWidget::root() { return &_root; }
