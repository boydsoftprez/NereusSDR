// no-port-check: NereusSDR-original. No upstream port. Single layout
// authority for the bottom banner; replaces RxDashboard's internal
// ladder and MainWindow::reapplyRightStripDropPriority. See
// docs/architecture/2026-08-02-bottom-banner-and-pan-menu-design.md §5.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVector>

#include "gui/chrome/ChromeFoldPlan.h"

class QWidget;

namespace NereusSDR {

/// Owns the banner's item table and applies one fold decision per resize.
///
/// Invariants this class exists to hold (design §5.1):
///   1. Nothing shrinks. An item is at its natural width or hidden.
///   2. Widths are cached, never re-measured mid-decision.
///   3. One pass per relayout. No second look, no feedback.
class ChromeBarController : public QObject {
    Q_OBJECT

public:
    explicit ChromeBarController(QObject* parent = nullptr);

    /// Register a banner item. rung 0 never folds. separator may be null;
    /// when present it is hidden and shown with its item so the dot run
    /// never dangles. Natural width is taken from the widget's sizeHint at
    /// registration; override it later with setNaturalWidth.
    void addItem(QWidget* widget, QWidget* separator, int rung,
                 const QString& overflowLabel);

    /// Update a cached width after a content change (a new PA reading with
    /// more digits, a longer radio name). Explicit, because an implicit
    /// re-measure is the feedback path this design removes.
    void setNaturalWidth(QWidget* widget, int px);

    /// Apply the fold decision for this bar width. Idempotent.
    void relayout(int barWidthPx);

    /// Labels of everything currently folded, in rung order.
    QStringList foldedLabels() const { return m_foldedLabels; }

    /// Rung currently folded through. 0 means nothing is folded.
    int foldedThroughRung() const noexcept { return m_foldedThrough; }

signals:
    /// Emitted only when the folded set actually changes, so OverflowChip
    /// is not rewritten on every resize event.
    void foldStateChanged(const QStringList& foldedLabels);

private:
    struct Registered {
        QWidget* widget{nullptr};
        QWidget* separator{nullptr};
        int      rung{0};
        int      naturalWidth{0};
        QString  label;
    };

    QVector<ChromeFoldEntry> buildTable() const;

    QVector<Registered>    m_items;
    QHash<QWidget*, int>   m_indexByWidget;
    QStringList            m_foldedLabels;
    int                    m_foldedThrough{-1};
};

} // namespace NereusSDR
