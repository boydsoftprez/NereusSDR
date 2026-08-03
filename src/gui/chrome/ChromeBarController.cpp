// no-port-check: NereusSDR-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/chrome/ChromeBarController.h"

#include <QWidget>

namespace NereusSDR {

ChromeBarController::ChromeBarController(QObject* parent)
    : QObject(parent)
{
}

void ChromeBarController::addItem(QWidget* widget, QWidget* separator,
                                  int rung, const QString& overflowLabel)
{
    if (!widget) {
        return;
    }
    Registered r;
    r.widget    = widget;
    r.separator = separator;
    r.rung      = rung;
    r.label     = overflowLabel;
    // ensurePolished() forces stylesheet-driven font/metric resolution that
    // would otherwise wait for first show, so this measurement is final
    // rather than merely usually-final. Without it, a widget whose
    // setStyleSheet() effect is deferred to QStyle::polish() would bake a
    // wrong width into the cache for its entire lifetime, silently.
    widget->ensurePolished();
    r.naturalWidth = widget->sizeHint().width();
    if (separator) {
        separator->ensurePolished();
        r.naturalWidth += separator->sizeHint().width() + ChromeFoldPlan::kGapPx;
    }
    m_indexByWidget.insert(widget, m_items.size());
    m_items.append(r);
    // A new item invalidates the last decision.
    m_foldedThrough = -1;
}

void ChromeBarController::setNaturalWidth(QWidget* widget, int px)
{
    const auto it = m_indexByWidget.constFind(widget);
    if (it == m_indexByWidget.constEnd()) {
        return;
    }
    Registered& r = m_items[*it];
    if (r.naturalWidth == px) {
        return;
    }
    r.naturalWidth = px;
    // Content width moved, so the previous decision no longer applies.
    m_foldedThrough = -1;
}

void ChromeBarController::setItemAvailable(QWidget* widget, bool available)
{
    const auto it = m_indexByWidget.constFind(widget);
    if (it == m_indexByWidget.constEnd()) {
        return;
    }
    Registered& r = m_items[*it];
    if (r.available == available) {
        return;
    }
    r.available = available;
    // Availability moved, exactly like a width change: the previous
    // decision no longer applies, and an unavailable item's width must
    // stop (or start) counting toward the budget -- see buildTable().
    m_foldedThrough = -1;
}

QVector<ChromeFoldEntry> ChromeBarController::buildTable() const
{
    QVector<ChromeFoldEntry> table;
    table.reserve(m_items.size());
    for (const Registered& r : m_items) {
        // An unavailable item is not a candidate for the width budget at
        // all: it contributes exactly what it does in the real hbox,
        // which is zero, matching a plain hidden widget. Folding math
        // that ignored this would under-fold when something ELSE needs
        // the space an unavailable-but-still-registered item is not
        // using (Task A8 fix round 1 finding 2 in reverse: the original
        // bug was UNDER-counting an item that WAS showing; the fix must
        // not now OVER-count one that currently is not).
        if (!r.available) {
            continue;
        }
        table.append(ChromeFoldEntry{r.rung, r.naturalWidth, r.label});
    }
    return table;
}

void ChromeBarController::relayout(int barWidthPx)
{
    const QVector<ChromeFoldEntry> table = buildTable();
    const int rung = ChromeFoldPlan::planFold(table, barWidthPx);
    if (rung == m_foldedThrough) {
        return;
    }
    m_foldedThrough = rung;

    for (const Registered& r : m_items) {
        const bool folded  = (r.rung != 0 && r.rung <= rung);
        const bool visible = r.available && !folded;
        r.widget->setVisible(visible);
        if (r.separator) {
            r.separator->setVisible(visible);
        }
    }

    const QStringList labels = ChromeFoldPlan::foldedLabels(table, rung);
    if (labels != m_foldedLabels) {
        m_foldedLabels = labels;
        emit foldStateChanged(m_foldedLabels);
    }
}

} // namespace NereusSDR
