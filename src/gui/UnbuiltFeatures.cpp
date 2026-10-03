// no-port-check: NereusSDR-original. See header.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/UnbuiltFeatures.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for full
// Modification history (NereusSDR).
// =================================================================

#include "gui/UnbuiltFeatures.h"

#include <QAction>
#include <QBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QLayout>
#include <QWidget>

namespace NereusSDR::UnbuiltFeatures {

namespace {

// The layout that holds `w` directly, searched from its parent widget's
// top layout down through nested layouts.
QLayout* findHolder(QLayout* layout, const QWidget* w)
{
    if (layout == nullptr) { return nullptr; }
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (item == nullptr) { continue; }
        if (item->widget() == w) { return layout; }
        if (QLayout* found = findHolder(item->layout(), w)) { return found; }
    }
    return nullptr;
}

QLayout* holderOf(const QWidget* w)
{
    const QWidget* parent = w->parentWidget();
    return parent != nullptr ? findHolder(parent->layout(), w) : nullptr;
}

void hideAllIn(QLayout* layout)
{
    if (layout == nullptr) { return; }
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (item == nullptr) { continue; }
        if (QWidget* w = item->widget()) {
            w->setVisible(false);
        } else if (QLayout* inner = item->layout()) {
            hideAllIn(inner);
        }
    }
}

// Hide every item on the grid row that holds `index`.
void hideGridRow(QGridLayout* grid, int index)
{
    int row = 0;
    int col = 0;
    int rowSpan = 0;
    int colSpan = 0;
    grid->getItemPosition(index, &row, &col, &rowSpan, &colSpan);
    for (int i = 0; i < grid->count(); ++i) {
        int r = 0;
        int c = 0;
        int rs = 0;
        int cs = 0;
        grid->getItemPosition(i, &r, &c, &rs, &cs);
        if (r != row) { continue; }
        QLayoutItem* item = grid->itemAt(i);
        if (QWidget* w = item->widget()) {
            w->setVisible(false);
        } else {
            hideAllIn(item->layout());
        }
    }
}

// The index of `unit` (a widget, or a nested layout) in `layout`.
int indexIn(QLayout* layout, const QWidget* widget, const QLayout* inner)
{
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (widget != nullptr && item->widget() == widget) { return i; }
        if (inner != nullptr && item->layout() == inner) { return i; }
    }
    return -1;
}

} // namespace

void disableUnlessBuilt(QWidget* widget, UnbuiltFeature feature, const QString& reason)
{
    if (widget == nullptr || isBuilt(feature)) { return; }
    widget->setEnabled(false);
    widget->setToolTip(reason);
}

QString notBuiltReason()
{
    return QStringLiteral("This option does not change anything in NereusSDR at the moment.");
}

void hideUnlessBuilt(QWidget* widget, UnbuiltFeature feature)
{
    if (widget == nullptr || isBuilt(feature)) { return; }
    if (auto* form = qobject_cast<QFormLayout*>(holderOf(widget))) {
        form->setRowVisible(widget, false);
        return;
    }
    widget->setVisible(false);
}

void hideRowUnlessBuilt(QWidget* control, UnbuiltFeature feature, QLayout* searchFrom)
{
    if (control == nullptr || isBuilt(feature)) { return; }
    QLayout* holder = searchFrom != nullptr ? findHolder(searchFrom, control)
                                            : holderOf(control);
    const QWidget* unitWidget = control;
    const QLayout* unitLayout = nullptr;
    while (holder != nullptr) {
        if (auto* form = qobject_cast<QFormLayout*>(holder)) {
            if (unitLayout != nullptr) {
                form->setRowVisible(const_cast<QLayout*>(unitLayout), false);
            } else {
                form->setRowVisible(const_cast<QWidget*>(unitWidget), false);
            }
            return;
        }
        if (auto* grid = qobject_cast<QGridLayout*>(holder)) {
            const int index = indexIn(grid, unitLayout ? nullptr : unitWidget, unitLayout);
            if (index >= 0) {
                hideGridRow(grid, index);
                return;
            }
            break;
        }
        auto* box = qobject_cast<QBoxLayout*>(holder);
        const bool horizontal = box != nullptr
            && (box->direction() == QBoxLayout::LeftToRight
                || box->direction() == QBoxLayout::RightToLeft);
        if (!horizontal) { break; }
        // A row layout. When it is itself a cell of a form or a grid, the
        // row's label sits beside it there: hide that whole row.
        auto* outer = qobject_cast<QLayout*>(holder->parent());
        if (outer != nullptr && (qobject_cast<QFormLayout*>(outer) != nullptr
                                 || qobject_cast<QGridLayout*>(outer) != nullptr)) {
            unitLayout = holder;
            unitWidget = nullptr;
            holder = outer;
            continue;
        }
        hideAllIn(holder);
        return;
    }
    control->setVisible(false);
}

void hideLayoutUnlessBuilt(QLayout* layout, UnbuiltFeature feature)
{
    if (layout == nullptr || isBuilt(feature)) { return; }
    hideAllIn(layout);
}

void hideUnlessBuilt(QAction* action, UnbuiltFeature feature)
{
    if (action == nullptr || isBuilt(feature)) { return; }
    action->setVisible(false);
}

} // namespace NereusSDR::UnbuiltFeatures
