#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/setup/DiskComboFiller.h  (NereusSDR)
// =================================================================
//
// Fills a Core disk drop-down (Setup > Cores > Recordings and Digital
// Spectrum Recorder) from the `recordings` document without tearing the
// list down on every update.
//
// The Core sends the recordings document about once a second while the
// recorder runs (its used bytes and write position). Each update used to
// call QComboBox::clear() and add the disks again. Removing the rows of
// the drop-down's list with Qt accessibility active runs
// QAccessibleTable::modelChange's RowsRemoved branch, which in Qt 6.11.0
// calls iface->role() on QAccessible::accessibleInterface(id) for every
// cell it cached without checking for null; a cell whose interface was
// already deleted crashes it (JJ's Mac, 2026-11-17, PR 808). Its
// ModelReset branch only deletes by id, which tolerates a stale id, and
// with its cache emptied the insert and remove branches return at once.
//
// So:
//   * the same disks (root, SD card, enabled, in order): only the words
//     and the free figure are set in place (dataChanged, never a row
//     removed or added);
//   * other disks: the model is reset (QStandardItemModel::clear()), never
//     row by row, and the disks added again;
//   * other disks while the list is open: nothing changes until it
//     closes, and then `again` runs (queued) so the caller fills it anew.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-17 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Fixes the
//                 Spectrum Recorder setup page crash on a Core update
//                 (PR 808).
// =================================================================

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>

#include <functional>

#include "gui/NereusGuiExport.h"

class QComboBox;

namespace NereusSDR {

/// One line of a Core disk drop-down. Roles as the setup pages read them:
/// Qt::UserRole the root sent back, Qt::UserRole + 1 whether it is an SD
/// card, Qt::UserRole + 2 its free bytes. An invalid QVariant leaves that
/// role unset.
struct DiskComboItem {
    static constexpr int kRootRole = Qt::UserRole;
    static constexpr int kSdRole = Qt::UserRole + 1;
    static constexpr int kFreeRole = Qt::UserRole + 2;

    QString text;
    QVariant root;
    QVariant sd;
    QVariant free;
    bool enabled = true;

    /// The same disk in the same place: the words and free figure aside.
    bool sameDisk(const DiskComboItem& other) const
    {
        return root == other.root && sd == other.sd && enabled == other.enabled;
    }
};

class NEREUS_GUI_EXPORT DiskComboFiller : public QObject {
public:
    enum class Result {
        Unchanged,   // nothing differed
        Updated,     // the same disks: words or figures set in place
        Rebuilt,     // other disks: the model was reset and filled
        Deferred,    // other disks while the list is open: left as it was
    };

    /// `again` runs (queued) after the list closes when a fill was
    /// deferred; it should fill the drop-down again from the latest
    /// document.
    DiskComboFiller(QComboBox* combo, std::function<void()> again, QObject* parent = nullptr);

    /// Shows `items` with `current` chosen (-1 for none). The caller blocks
    /// the drop-down's signals.
    Result fill(const QList<DiskComboItem>& items, int current);

    /// A fill is waiting for the list to close.
    bool pending() const { return m_pending; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QList<DiskComboItem> shown() const;

    QPointer<QComboBox> m_combo;
    std::function<void()> m_again;
    bool m_pending = false;
};

}  // namespace NereusSDR
