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
///
/// Two independent axes decide an item's visibility, both owned here:
///   - FOLD, computed internally from width pressure (the rung ladder).
///   - AVAILABILITY, an external fact a caller reports via
///     setItemAvailable (PS-A armed, TGXL present, a second ADC chain,
///     an RX pill's DSP-active state). An item is visible only when
///     BOTH hold: available && !folded. This closes the gap Task A8's
///     first round left: several banner items already have a legitimate
///     second visibility owner (their feature's own on/armed/present
///     state), and this class's addItem contract requires it to be the
///     SOLE writer of setVisible. Before this axis existed, the only way
///     to reconcile the two was an external caller ANDing a live query
///     on top of relayout()'s decision after the fact, which cannot see
///     signals that fire between relayout() calls and does not adjust
///     the WIDTH budget for an item that is currently unavailable
///     (Task A8 fix round 1 findings 1 and 2). availability is a stored
///     input, not a live read, so it cannot introduce the
///     measure-mutate-measure oscillation this whole design exists to
///     remove: it only changes when a caller explicitly says so.
class ChromeBarController : public QObject {
    Q_OBJECT

public:
    explicit ChromeBarController(QObject* parent = nullptr);

    /// Register a banner item. rung 0 never folds. separator may be null;
    /// when present it is hidden and shown with its item so the dot run
    /// never dangles. Natural width is taken from the widget's sizeHint at
    /// registration; override it later with setNaturalWidth.
    ///
    /// Precondition and contract: the width measured here is authoritative
    /// for the widget's entire lifetime in this controller. It is cached,
    /// not re-read, on every subsequent relayout (design §5.1 point 2).
    /// addItem calls ensurePolished() on widget (and separator, if given)
    /// before measuring, so a stylesheet-driven font or metric change that
    /// would otherwise wait for QStyle::polish() on first show is resolved
    /// first and the measurement is final rather than merely usually-final.
    /// Any later change that alters a registered widget's width MUST be
    /// reported via setNaturalWidth; there is no other way for this
    /// controller to learn about it. This controller also assumes it is the
    /// sole writer of visibility for every item it registers; calling
    /// setVisible directly on a registered widget or separator from
    /// elsewhere will desync it from the next relayout's decision. If the
    /// item's visibility ALSO depends on something other than width (an
    /// armed/present/DSP-active flag), report that through
    /// setItemAvailable instead of a direct setVisible call -- that is the
    /// one sanctioned side channel. Availability defaults to true, so an
    /// item with no such flag needs no extra call.
    void addItem(QWidget* widget, QWidget* separator, int rung,
                 const QString& overflowLabel);

    /// Register the ladder's own output surface: the chip that names what
    /// folded. Separate from addItem because this item alone is charged to
    /// the budget conditionally -- it is on screen only while something is
    /// folded, so it costs nothing at rung 0. See ChromeFoldEntry's
    /// onlyWhenFolded for the width band that charging it unconditionally
    /// made unreachable. Registered at rung 0: the chip must never itself
    /// fold, or the bar would hide the one thing explaining what it hid.
    void addOverflowChip(QWidget* chip);

    /// Update a cached width after a content change (a new PA reading with
    /// more digits, a longer radio name). Explicit, because an implicit
    /// re-measure is the feedback path this design removes.
    /// Report a new width for a registered item after a content change.
    /// `px` is the PRIMARY widget's width only; any separator overhead
    /// recorded at registration is re-added internally.
    void setNaturalWidth(QWidget* widget, int px);

    /// Report whether a registered item is currently applicable at all,
    /// independent of width. An item is visible only when available AND
    /// not folded. No-op if widget was never registered. Idempotent (a
    /// repeated call with the same value does not invalidate the cached
    /// decision). Like setNaturalWidth, this only updates stored state;
    /// the caller must still call relayout() to apply it.
    void setItemAvailable(QWidget* widget, bool available);

    /// Apply the fold decision for this bar width. Idempotent.
    void relayout(int barWidthPx);

    /// Labels of everything currently folded, in rung order.
    QStringList foldedLabels() const { return m_foldedLabels; }

    /// Rung currently folded through. 0 means the bar fits with nothing
    /// folded; -1 (the constructed default) means relayout() has never
    /// run, or that a subsequent addItem/setNaturalWidth/setItemAvailable
    /// call has invalidated the last decision and it has not been
    /// recomputed yet. -1 is a distinct sentinel, not a third "nothing
    /// folded" spelling: buildTable()'s callers must not treat it as 0.
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
        /// Separator width plus its gap, held apart from the widget's own
        /// width so setNaturalWidth can re-add it instead of dropping it.
        int      separatorOverhead{0};
        QString  label;
        /// See setItemAvailable. Defaults true: most items have no second
        /// visibility owner and are available whenever not folded.
        bool     available{true};
        /// Set only by addOverflowChip. Carried into ChromeFoldEntry so the
        /// pure math can leave the chip out of the nothing-folded case.
        bool     onlyWhenFolded{false};
    };

    QVector<ChromeFoldEntry> buildTable() const;

    QVector<Registered>    m_items;
    QHash<QWidget*, int>   m_indexByWidget;
    QStringList            m_foldedLabels;
    int                    m_foldedThrough{-1};
};

} // namespace NereusSDR
