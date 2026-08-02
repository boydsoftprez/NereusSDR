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
    r.widget       = widget;
    r.separator    = separator;
    r.rung         = rung;
    r.naturalWidth = widget->sizeHint().width();
    r.label        = overflowLabel;
    if (separator) {
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

QVector<ChromeFoldEntry> ChromeBarController::buildTable() const
{
    QVector<ChromeFoldEntry> table;
    table.reserve(m_items.size());
    for (const Registered& r : m_items) {
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
        const bool hide = (r.rung != 0 && r.rung <= rung);
        r.widget->setVisible(!hide);
        if (r.separator) {
            r.separator->setVisible(!hide);
        }
    }

    const QStringList labels = ChromeFoldPlan::foldedLabels(table, rung);
    if (labels != m_foldedLabels) {
        m_foldedLabels = labels;
        emit foldStateChanged(m_foldedLabels);
    }
}

} // namespace NereusSDR
