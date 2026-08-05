// no-port-check: NereusSDR-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/chrome/ChromeFoldPlan.h"

#include <algorithm>

namespace NereusSDR {

int ChromeFoldPlan::requiredWidth(const QVector<ChromeFoldEntry>& items,
                                  int foldThroughRung)
{
    int content = 0;
    int visible = 0;
    for (const ChromeFoldEntry& e : items) {
        if (e.rung != 0 && e.rung <= foldThroughRung) {
            continue;
        }
        // At rung 0 nothing is folded, so the overflow chip has nothing to
        // report and is not shown; it costs zero. Charging it here made the
        // bar's layout depend on how it ARRIVED at a width rather than on
        // the width itself. Widths between "everything fits" and
        // "everything fits plus the chip" could be entered from below but
        // never left: rung 0 was rejected for a chip that only rung >= 1
        // puts on screen, so an item stayed folded that had room, and the
        // chip stayed up citing it. Coming down from a wider window the
        // same pixel width gave the correct, fully-unfolded bar. Two
        // answers for one width is the history-dependence this class exists
        // to remove. Found by Codex on PR #316.
        if (e.onlyWhenFolded && foldThroughRung == 0) {
            continue;
        }
        content += e.widthPx;
        ++visible;
    }
    if (visible == 0) {
        return kPadPx;
    }
    return content + kGapPx * (visible - 1) + kPadPx;
}

int ChromeFoldPlan::planFold(const QVector<ChromeFoldEntry>& items,
                             int barWidthPx)
{
    int maxRung = 0;
    for (const ChromeFoldEntry& e : items) {
        maxRung = std::max(maxRung, e.rung);
    }
    for (int rung = 0; rung <= maxRung; ++rung) {
        if (requiredWidth(items, rung) <= barWidthPx) {
            return rung;
        }
    }
    return maxRung;
}

QStringList ChromeFoldPlan::foldedLabels(const QVector<ChromeFoldEntry>& items,
                                         int foldThroughRung)
{
    QVector<ChromeFoldEntry> hidden;
    for (const ChromeFoldEntry& e : items) {
        if (e.rung != 0 && e.rung <= foldThroughRung) {
            hidden.append(e);
        }
    }
    std::stable_sort(hidden.begin(), hidden.end(),
                     [](const ChromeFoldEntry& a, const ChromeFoldEntry& b) {
                         return a.rung < b.rung;
                     });
    QStringList out;
    out.reserve(hidden.size());
    for (const ChromeFoldEntry& e : hidden) {
        if (!e.label.isEmpty()) {
            out << e.label;
        }
    }
    return out;
}

} // namespace NereusSDR
