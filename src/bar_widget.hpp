// somebar - dwl bar
// See LICENSE file for copyright and license details.

#pragma once
#include <QBoxLayout>
#include <QLabel>
#include <QWidget>

class BarWidget {
    QWidget _root;
    QBoxLayout _box;
    QLabel _label;
public:
    explicit BarWidget();
    ~BarWidget();
    QWidget* root();
};
