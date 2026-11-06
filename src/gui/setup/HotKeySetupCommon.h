// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 11: what the Keyboard and
//                MIDI pages share, moved out of HotKeySetupPages.cpp.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 18: the action lists and table
//                cells the MIDI and Knobs pages share, moved out of MidiSetupPage.cpp.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// The look and the words the Keys and controllers pages have in common.
#pragma once

#include "core/hotkeys/HotKeyAction.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "gui/StyleConstants.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QList>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QWidget>

#include <algorithm>

namespace NereusSDR::HotKeySetup {

inline QString switchedOffReason()
{
    return QStringLiteral("Use shortcuts is off.");
}

inline QString fixedTargetReason(HotKeyScope scope)
{
    switch (scope) {
    case HotKeyScope::TransmitSlice:
        return QStringLiteral("Transmit actions always use the transmit slice.");
    case HotKeyScope::Pan:
        return QStringLiteral("Zoom always acts on the panadapter you are using.");
    case HotKeyScope::Window:
        return QStringLiteral("This action always acts on the window you are using.");
    case HotKeyScope::Slice:
        break;
    }
    return {};
}

inline QString fixedTargetName(HotKeyScope scope)
{
    switch (scope) {
    case HotKeyScope::TransmitSlice:
        return QStringLiteral("Transmit slice");
    case HotKeyScope::Pan:
        return QStringLiteral("This panadapter");
    case HotKeyScope::Window:
        return QStringLiteral("This window");
    case HotKeyScope::Slice:
        break;
    }
    return {};
}

inline void setReason(QWidget* widget, const QString& reason)
{
    widget->setToolTip(reason);
    widget->setAccessibleDescription(reason);
}

inline QStringList groupsInOrder()
{
    QStringList groups;
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        if (!groups.contains(action.group)) {
            groups.append(action.group);
        }
    }
    return groups;
}

inline QString smallButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: #1a2a3a; border: 1px solid #304050; border-radius: 3px;"
        " color: #c8d8e8; font-size: 11px; padding: 2px 7px; }"
        "QPushButton:hover { background: #203040; }"
        "QPushButton:pressed { background: #00b4d8; color: #0f0f1a; }"
        "QPushButton:checked { background: #00b4d8; color: #0f0f1a; border-color: #00b4d8; }"
        "QPushButton:disabled { background: #141a22; color: #5a6a7a; border-color: #243240; }");
}

inline QString comboStyle()
{
    return QString::fromLatin1(Style::kComboStyle)
        + QStringLiteral("QComboBox:disabled { background: #141a22; color: #5a6a7a; border-color: #243240; }");
}

// A table's cell shows the row's color through it, selected or not.
inline void makeCellClear(QWidget* cell)
{
    cell->setObjectName(QStringLiteral("hotKeyCell"));
    cell->setStyleSheet(QStringLiteral("QWidget#hotKeyCell { background: transparent; }"));
}

inline void setComboData(QComboBox* combo, const QVariant& data)
{
    const QSignalBlocker blocker(combo);
    combo->setCurrentIndex(std::max(0, combo->findData(data)));
}

inline void disableComboItem(QComboBox* combo, int index, const QString& reason)
{
    auto* const model = qobject_cast<QStandardItemModel*>(combo->model());
    if (QStandardItem* const item = model != nullptr ? model->item(index) : nullptr) {
        item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        item->setToolTip(reason);
    }
}

// Every action under its group's name, as the Keyboard page's list orders them.
inline void fillActionCombo(QComboBox* combo, const QString& noneText)
{
    combo->addItem(noneText, QString());
    for (const QString& group : groupsInOrder()) {
        combo->addItem(group);
        disableComboItem(combo, combo->count() - 1, {});
        for (const HotKeyAction& action : HotKeyActionList::all()) {
            if (action.group == group) {
                combo->addItem(action.name, action.id);
            }
        }
    }
}

// The name goes in the widget's own place, so it can't be found again once the
// row is gone: a removed row's widgets are only deleted later.
inline void retireNames(QWidget* cell)
{
    if (cell == nullptr) {
        return;
    }
    cell->setObjectName({});
    const QList<QWidget*> children = cell->findChildren<QWidget*>();
    for (QWidget* const child : children) {
        child->setObjectName({});
    }
}

inline QWidget* makeCell(QWidget* content)
{
    auto* const cell = new QWidget;
    makeCellClear(cell);
    auto* const layout = new QHBoxLayout(cell);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(3);
    layout->addWidget(content);
    return cell;
}

inline QString lineEditStyle()
{
    return QString::fromLatin1(Style::kLineEditStyle);
}

inline QString listStyle()
{
    return QStringLiteral("QTableWidget { background: #131326; color: #c8d8e8;"
                          "  gridline-color: #304050; border: 1px solid #304050; }"
                          "QTableWidget::item { padding: 2px 4px; border-bottom: 1px solid #1a2030; }"
                          "QTableWidget::item:selected { background: #1a3050; color: #c8d8e8; }"
                          "QHeaderView::section { background: #1a2030; color: #8aa8c0;"
                          "  border: 1px solid #304050; padding: 4px; }");
}

} // namespace NereusSDR::HotKeySetup
