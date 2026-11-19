// SPDX-License-Identifier: GPL-3.0-or-later
// src/gui/widgets/FlowLayout.cpp  (Longpath) — see FlowLayout.h
//
// Longpath-original. No Thetis port.
#include "gui/widgets/FlowLayout.h"

#include <QSpacerItem>
#include <QWidget>

#include <utility>

namespace Longpath {

FlowLayout::FlowLayout(QWidget* parent, int hSpacing, int vSpacing)
    : QLayout(parent)
    , m_hSpace(hSpacing)
    , m_vSpace(vSpacing)
{
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem* it = takeAt(0)) { delete it; }
}

void FlowLayout::addItem(QLayoutItem* item)
{
    m_items.append(item);
    invalidate();
}

void FlowLayout::addStretch(int)
{
    addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
}

void FlowLayout::addSpacing(int breite)
{
    addItem(new QSpacerItem(qMax(0, breite), 0,
                            QSizePolicy::Fixed, QSizePolicy::Minimum));
}

void FlowLayout::insertWidget(int index, QWidget* widget)
{
    if (!widget) { return; }
    // Dasselbe wie QLayout::addWidget(), nur an einer Stelle statt am
    // Ende.
    addChildWidget(widget);
    m_items.insert(qBound(0, index, int(m_items.size())),
                   new QWidgetItem(widget));
    invalidate();
}

QLayoutItem* FlowLayout::takeAt(int i)
{
    if (i < 0 || i >= m_items.size()) { return nullptr; }
    QLayoutItem* it = m_items.takeAt(i);
    invalidate();
    return it;
}

bool FlowLayout::wantsWidth(QLayoutItem* item)
{
    if (QSpacerItem* sp = item->spacerItem()) {
        return sp->sizePolicy().horizontalPolicy() & QSizePolicy::ExpandFlag;
    }
    if (const QWidget* w = item->widget()) {
        // Ignored heisst in Qt: „bekommt so viel Platz wie moeglich".
        return w->sizePolicy().horizontalPolicy()
               & (QSizePolicy::ExpandFlag | QSizePolicy::IgnoreFlag);
    }
    return item->expandingDirections() & Qt::Horizontal;
}

int FlowLayout::heightForWidth(int width) const
{
    return layOut(QRect(0, 0, width, 0), false);
}

QSize FlowLayout::sizeHint() const
{
    // Alles in einer Zeile: so breit wie die Summe, so hoch wie das
    // hoechste Element. Der Umbruch kommt erst, wenn weniger da ist.
    int w = 0, h = 0, n = 0;
    for (QLayoutItem* it : m_items) {
        if (QSpacerItem* sp = it->spacerItem()) {
            // Ein fester Abstand gehoert zur Breite der einen Zeile.
            if (!wantsWidth(it)) { w += sp->sizeHint().width(); }
            continue;
        }
        const QSize s = it->sizeHint();
        w += s.width() + (n > 0 ? m_hSpace : 0);
        h = qMax(h, s.height());
        ++n;
    }
    const QMargins m = contentsMargins();
    return QSize(w + m.left() + m.right(), h + m.top() + m.bottom());
}

QSize FlowLayout::minimumSize() const
{
    int w = 0, h = 0;
    for (QLayoutItem* it : m_items) {
        if (it->spacerItem()) { continue; }
        const QSize s = it->minimumSize();
        w = qMax(w, s.width());
        h = qMax(h, s.height());
    }
    const QMargins m = contentsMargins();
    // Die Hoehe: so viele Zeilen, wie die AKTUELLE Breite braucht. Ein
    // QSplitter oder QBoxLayout fragt die Mindesthoehe ohne Breite —
    // meldet die Leiste dann nur eine Zeile, drueckt das Fenster die
    // Nachbarn unter sie (Kartenspalte bei 640 px, 2026-09-22). Breiter
    // gezogen faellt der Wert nach dem naechsten setGeometry wieder.
    int minH = h + m.top() + m.bottom();
    if (geometry().width() > 0) {
        minH = qMax(minH, heightForWidth(geometry().width()));
    }
    return QSize(w + m.left() + m.right(), minH);
}

void FlowLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    layOut(rect, true);
}

// Zeile fuer Zeile: Elemente aufsammeln, bis eines nicht mehr passt;
// dann die Zeile setzen — was uebrig ist, an die dehnbaren Elemente —
// und weiter. Liefert die Gesamthoehe.
int FlowLayout::layOut(const QRect& rect, bool apply) const
{
    const QMargins m = contentsMargins();
    const QRect area = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
    const int width = qMax(0, area.width());

    struct Placed { QLayoutItem* item; int w; int h; bool stretch; };
    QList<Placed> row;
    int rowsDone = 0;
    int y = area.top();

    auto flush = [&]() {
        if (row.isEmpty()) { return; }
        int used = 0, rowH = 0, stretchers = 0, visible = 0;
        for (const Placed& p : row) {
            used += p.w;
            rowH = qMax(rowH, p.h);
            if (p.stretch) { ++stretchers; }
            if (!p.item->spacerItem()) { ++visible; }
        }
        // Abstaende nur zwischen sichtbaren Elementen; eine Luecke
        // (addStretch) ist keins und kostet keinen.
        used += m_hSpace * qMax(0, visible - 1);
        const int spare = qMax(0, width - used);
        const int each = stretchers > 0 ? spare / stretchers : 0;
        int extra = stretchers > 0 ? spare - each * stretchers : 0;
        if (apply) {
            int x = area.left();
            for (const Placed& p : row) {
                int w = p.w;
                if (p.stretch) {
                    w += each + (extra > 0 ? 1 : 0);
                    if (extra > 0) { --extra; }
                }
                if (p.item->spacerItem()) {
                    x += w;          // die Luecke selbst, ohne Abstand
                } else {
                    p.item->setGeometry(QRect(x, y, w, rowH));
                    x += w + m_hSpace;
                }
            }
        }
        y += rowH + m_vSpace;
        ++rowsDone;
        row.clear();
    };

    int lineWidth = 0;   // Breite der sichtbaren Elemente samt Abstaenden
    int visibleInRow = 0;
    // Ein fester Abstand wartet, bis das naechste sichtbare Element
    // kommt: steht es in derselben Zeile, liegt der Abstand davor;
    // bricht es um, entfaellt er.
    QLayoutItem* gap = nullptr;
    int gapWidth = 0;
    // Dehnbare Luecken, die auf ihr naechstes Element warten
    // (setStretchFollowsNext).
    QList<QLayoutItem*> waiting;
    for (QLayoutItem* it : m_items) {
        if (QSpacerItem* sp = it->spacerItem()) {
            if (wantsWidth(it)) {
                if (m_stretchFollows) {
                    waiting.append(it);
                } else {
                    row.append({it, 0, 0, true});
                }
            } else {
                gap = it;
                gapWidth += sp->sizeHint().width();
            }
            continue;
        }
        // Ein VERSTECKTES Widget bleibt ein Element der Zeile, mit
        // Breite null und seinem Abstand -- wie seit 2026-09-21. Am
        // 2026-11-18 stand hier kurz „ueberspringen"; die Kartenleiste
        // traegt aber ein Dutzend versteckte Elemente, passte dadurch
        // vorübergehend in EINE Zeile, und `tst_logbuch_karte_pfeil`
        // fand den Pfeil unter dem Radar (Linux-CI und lokal). Wer in
        // seiner Reihe etwas versteckt, steckt es in ein eigenes Stueck
        // mit QHBoxLayout -- der kennt versteckte Kinder.
        const QSize hint = it->sizeHint();
        const int minW = it->minimumSize().width();
        int w = qMax(hint.width(), minW);
        // Breiter als die ganze Zeile: so breit wie die Zeile, soweit
        // die Mindestbreite es erlaubt -- statt rechts hinauszuragen.
        if (w > width) { w = qMax(width, minW); }
        const int h = hint.height();
        int before = visibleInRow > 0 ? m_hSpace + gapWidth : 0;
        if (visibleInRow > 0 && lineWidth + before + w > width) {
            flush();
            lineWidth = 0;
            visibleInRow = 0;
            before = 0;
        }
        if (visibleInRow > 0 && gapWidth > 0 && gap) {
            row.append({gap, gapWidth, 0, false});
        }
        gap = nullptr;
        gapWidth = 0;
        for (QLayoutItem* s : std::as_const(waiting)) {
            row.append({s, 0, 0, true});
        }
        waiting.clear();
        row.append({it, w, h, wantsWidth(it)});
        lineWidth += before + w;
        ++visibleInRow;
    }
    // Eine Luecke, auf die nichts mehr folgt, bleibt in der letzten Zeile.
    for (QLayoutItem* s : std::as_const(waiting)) {
        row.append({s, 0, 0, true});
    }
    flush();
    m_rows = rowsDone;
    const int total = y - area.top() - (rowsDone > 0 ? m_vSpace : 0);
    return total + m.top() + m.bottom();
}

} // namespace Longpath
