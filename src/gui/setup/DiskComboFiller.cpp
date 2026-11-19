// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/setup/DiskComboFiller.cpp  (NereusSDR)
// =================================================================
//
// See DiskComboFiller.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-17 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Fixes the
//                 Spectrum Recorder setup page crash on a Core update
//                 (PR 808).
// =================================================================

#include "gui/setup/DiskComboFiller.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QEvent>
#include <QStandardItemModel>

namespace NereusSDR {

namespace {

bool itemEnabled(QComboBox* combo, int index)
{
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    const QStandardItem* item = model != nullptr ? model->item(index) : nullptr;
    return item == nullptr || item->isEnabled();
}

void setItemEnabled(QComboBox* combo, int index, bool enabled)
{
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    QStandardItem* item = model != nullptr ? model->item(index) : nullptr;
    if (item != nullptr && item->isEnabled() != enabled) {
        item->setFlags(enabled ? item->flags() | Qt::ItemIsEnabled : item->flags() & ~Qt::ItemIsEnabled);
    }
}

void setRole(QComboBox* combo, int index, int role, const QVariant& value)
{
    if (combo->itemData(index, role) != value) {
        combo->setItemData(index, value, role);
    }
}

}  // namespace

DiskComboFiller::DiskComboFiller(QComboBox* combo, std::function<void()> again, QObject* parent)
    : QObject(parent)
    , m_combo(combo)
    , m_again(std::move(again))
{
    if (combo != nullptr) {
        combo->view()->installEventFilter(this);
    }
}

QList<DiskComboItem> DiskComboFiller::shown() const
{
    QList<DiskComboItem> items;
    for (int i = 0; i < m_combo->count(); ++i) {
        items.append(DiskComboItem{m_combo->itemText(i), m_combo->itemData(i, DiskComboItem::kRootRole),
                                   m_combo->itemData(i, DiskComboItem::kSdRole),
                                   m_combo->itemData(i, DiskComboItem::kFreeRole), itemEnabled(m_combo, i)});
    }
    return items;
}

DiskComboFiller::Result DiskComboFiller::fill(const QList<DiskComboItem>& items, int current)
{
    if (!m_combo) {
        return Result::Unchanged;
    }
    const QList<DiskComboItem> now = shown();
    bool sameDisks = now.size() == items.size();
    for (int i = 0; sameDisks && i < items.size(); ++i) {
        sameDisks = now.at(i).sameDisk(items.at(i));
    }

    if (sameDisks) {
        m_pending = false;
        bool changed = false;
        for (int i = 0; i < items.size(); ++i) {
            const DiskComboItem& item = items.at(i);
            if (now.at(i).text != item.text) {
                m_combo->setItemText(i, item.text);
                changed = true;
            }
            if (now.at(i).free != item.free) {
                m_combo->setItemData(i, item.free, DiskComboItem::kFreeRole);
                changed = true;
            }
        }
        if (m_combo->currentIndex() != current) {
            m_combo->setCurrentIndex(current);
            changed = true;
        }
        return changed ? Result::Updated : Result::Unchanged;
    }

    // Other disks. Not while the list is open: the rows under the pointer
    // would change, so wait for it to close.
    if (m_combo->view()->isVisible()) {
        m_pending = true;
        return Result::Deferred;
    }
    m_pending = false;

    // A reset, never QComboBox::clear() (rows removed one block at a time
    // under accessibility: see the header).
    if (auto* model = qobject_cast<QStandardItemModel*>(m_combo->model())) {
        model->clear();
    } else {
        m_combo->clear();
    }
    for (int i = 0; i < items.size(); ++i) {
        const DiskComboItem& item = items.at(i);
        m_combo->addItem(item.text, item.root);
        setRole(m_combo, i, DiskComboItem::kSdRole, item.sd);
        setRole(m_combo, i, DiskComboItem::kFreeRole, item.free);
        setItemEnabled(m_combo, i, item.enabled);
    }
    m_combo->setCurrentIndex(current);
    return Result::Rebuilt;
}

bool DiskComboFiller::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Hide && m_combo && watched == m_combo->view() && m_pending) {
        // Queued: the drop-down is still closing its list.
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (m_pending && m_again) {
                    m_again();
                }
            },
            Qt::QueuedConnection);
    }
    return QObject::eventFilter(watched, event);
}

}  // namespace NereusSDR
