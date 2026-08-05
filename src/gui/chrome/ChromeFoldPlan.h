// no-port-check: NereusSDR-original. No upstream port. Pure fold math
// for the bottom-banner layout authority; see
// docs/architecture/2026-08-02-bottom-banner-and-pan-menu-design.md §5.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace NereusSDR {

/// One banner item's contribution to the width budget.
///
/// rung 0 never folds. Rungs 1..N fold in ascending order; entries sharing
/// a rung fold together (CAT and TCI, per design §6).
struct ChromeFoldEntry {
    int     rung{0};
    int     widthPx{0};
    QString label;
    /// True for an item that only EXISTS because something folded: the
    /// overflow chip, and nothing else. Such an item costs nothing while
    /// nothing is folded, because it is not on screen then. Charging it at
    /// rung 0 anyway asks "does everything fit alongside the chip that
    /// announces things did not fit", which has no consistent answer and
    /// left the bar with a width band it could only enter, never leave.
    /// See requiredWidth.
    bool    onlyWhenFolded{false};
};

/// Fold decisions as pure functions over a width table.
///
/// Deliberately free of QWidget so the whole ladder is testable without a
/// GUI, and so no decision can read a live sizeHint that a previous fold
/// step just changed. That feedback path is what made the three systems
/// this replaces oscillate.
class ChromeFoldPlan {
public:
    /// Layout spacing, mirroring buildStatusBar()'s QHBoxLayout.
    static constexpr int kGapPx = 6;
    /// Left plus right content margin, mirroring setContentsMargins(6,0,6,0).
    static constexpr int kPadPx = 12;

    /// Width the bar needs with every rung up to and including
    /// foldThroughRung hidden. Pass 0 for "nothing folded".
    static int requiredWidth(const QVector<ChromeFoldEntry>& items,
                             int foldThroughRung);

    /// Lowest rung that makes the bar fit in barWidthPx, or 0 if it already
    /// fits. Returns the highest rung present when nothing makes it fit.
    static int planFold(const QVector<ChromeFoldEntry>& items,
                        int barWidthPx);

    /// Labels of every entry hidden at foldThroughRung, in rung order.
    static QStringList foldedLabels(const QVector<ChromeFoldEntry>& items,
                                    int foldThroughRung);
};

} // namespace NereusSDR
