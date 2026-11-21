// src/gui/instruments/InstrumentSpine.cpp

// --- From src/gui/instruments/InstrumentSpine.h ---
// =================================================================
// src/gui/instruments/InstrumentSpine.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die Geometrie zweimal, die Mittel einmal ─────────────────────────
//
// OE5SOS, 2026-08-17: „sag, welchen gemeinsamen Zeichenweg du für
// Mulde, Verlauf und Glut vorsiehst — die drei Mittel sollen an einer
// Stelle stehen, nicht dreimal."
//
// Die drei Mittel stehen in InstrumentPainter, genau einmal. Sie
// müssen aber zwei Formen bedienen: den Bogen (Zeigerinstrument,
// VFO B) und die Gerade (Balken, VFO A). Der Betreiber hat es selbst
// so beschrieben: „die Rille der Zeiger, gerade gezogen".
//
// Also trennt diese Datei, was verschieden ist, von dem, was gleich
// ist. Eine Spine beantwortet, WO etwas liegt; der Painter weiss, WIE
// es aussieht. Fünf Ansichten fallen daraus:
//
//   Zeigerinstrument   ArcSpine
//   Balkeninstrument   LinearSpine
//   VFO A Bandstreifen LinearSpine, breit und flach
//   VFO B flacher Bogen ArcSpine, grosser Radius, kleiner Sweep
//   VFO C nur die Zahl  keine Spine — nur die Glut
//
// ── Anteile, keine Werte ─────────────────────────────────────────────
//
// Eine Spine kennt keine Messgrössen. Sie rechnet mit Anteilen 0..1;
// die Umrechnung Wert → Anteil macht ReadingDescriptor::fraction(),
// weil dort die Kennlinie steht (die Empfangsskala ist nicht linear).
// Eine Geometrie, die Skalen kennt, wäre die dritte Stelle, an der
// eine S-Stauchung stehen könnte.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

// --- From src/gui/instruments/InstrumentSpine.cpp ---
// =================================================================
// src/gui/instruments/InstrumentSpine.cpp  (Longpath)
// =================================================================
// Siehe InstrumentSpine.h — die Geometrie zweimal, die Mittel einmal.
//
// Die Zahlen (Stopps der Verläufe, Sektor-Innenradius, Glutweite)
// stammen aus den Entwürfen des Betreibers:
//   ~/Downloads/zeiger-verfeinert.html      — Bogen
//   ~/Downloads/frequenz-im-zeigerstil.html — Gerade
// =================================================================

// =================================================================
// src/gui/instruments/InstrumentSpine.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/instruments/InstrumentSpine.h
//   src/gui/instruments/InstrumentSpine.cpp
//   at revision 551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banners are copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
//   Namespace Longpath becomes NereusSDR.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/instruments/InstrumentSpine.h"

#include <QLinearGradient>
#include <QRadialGradient>
#include <QtMath>

namespace NereusSDR {

// QPainterPath::arcTo rechnet in ganzen Grad, 0° auf drei Uhr, positive
// Winkel gegen den Uhrzeigersinn — dieselbe Zählung wie die Entwürfe.
// Die 1/16-Grad-Zählung von QPainter::drawArc kommt hier nicht vor.

namespace {

// Masse aus ~/Downloads/zeiger-verfeinert.html, als Anteil von
// kDesignUnit — siehe die Begründung dort im Header.

// From Longpath src/gui/instruments/InstrumentSpine.cpp:31 [@551576e3]
/// Die dunkle Kante über der Rille. Entwurf: stroke-width 2.2 bei
/// Rillenbreite 13, bündig mit der äusseren Begrenzung.
constexpr qreal kShadowOfTrough  = 2.2 / 13.0;

// From Longpath src/gui/instruments/InstrumentSpine.cpp:34 [@551576e3]
/// Innenradius des Sektors: Entwurf 12.
constexpr qreal kSectorInnerOfUnit = 12.0 / kDesignUnit;

// From Longpath src/gui/instruments/InstrumentSpine.cpp:38 [@551576e3]
/// Luft zwischen Sektor und Rille (Entwurf 2) bzw. zwischen Teilstrich
/// und Rille (Entwurf 1).
constexpr qreal kSectorGapOfUnit = 2.0 / kDesignUnit;
constexpr qreal kTickGapOfUnit   = 1.0 / kDesignUnit;

} // namespace

// ── ArcSpine ─────────────────────────────────────────────────────────

ArcSpine::ArcSpine(QPointF centre, qreal radius, qreal width,
                   qreal deg0, qreal deg1)
    : m_c(centre), m_r(radius), m_w(width), m_deg0(deg0), m_deg1(deg1)
{
    m_sectorInner = unit() * kSectorInnerOfUnit;
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:52 [@551576e3]
qreal ArcSpine::degreeAt(qreal f) const
{
    return m_deg0 + (m_deg1 - m_deg0) * qBound(0.0, f, 1.0);
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:57 [@551576e3]
QPointF ArcSpine::pointAt(qreal r, qreal deg) const
{
    const qreal a = qDegreesToRadians(deg);
    // Minus bei y, weil der Bildschirm nach unten zählt und die
    // Entwürfe nach oben.
    return QPointF(m_c.x() + qCos(a) * r, m_c.y() - qSin(a) * r);
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:65 [@551576e3]
QPainterPath ArcSpine::arcPath(qreal r, qreal d0, qreal d1) const
{
    QPainterPath p;
    const QRectF box(m_c.x() - r, m_c.y() - r, 2 * r, 2 * r);
    p.arcMoveTo(box, d0);
    p.arcTo(box, d0, d1 - d0);
    return p;
}

QPainterPath ArcSpine::sectorPath(qreal rOut, qreal rIn,
                                  qreal d0, qreal d1) const
{
    QPainterPath p;
    const QRectF outBox(m_c.x() - rOut, m_c.y() - rOut, 2 * rOut, 2 * rOut);
    const QRectF inBox(m_c.x() - rIn, m_c.y() - rIn, 2 * rIn, 2 * rIn);
    p.arcMoveTo(outBox, d0);
    p.arcTo(outBox, d0, d1 - d0);
    p.arcTo(inBox, d1, d0 - d1);
    p.closeSubpath();
    return p;
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:87 [@551576e3]
QPainterPath ArcSpine::troughPath() const
{
    return arcPath(m_r, m_deg0, m_deg1);
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:92 [@551576e3]
QPainterPath ArcSpine::shadowPath() const
{
    // Die dunkle Kante sitzt an der ÄUSSEREN Begrenzung der Mulde,
    // knapp innerhalb, damit sie nicht über den Rand hinausragt: der
    // Pfad läuft eine halbe Kantenbreite innerhalb, dann liegt der
    // Strich bündig auf der Aussenkante.
    return arcPath(m_r + m_w / 2.0 - shadowWidth() / 2.0, m_deg0, m_deg1);
}

qreal ArcSpine::shadowWidth() const { return m_w * kShadowOfTrough; }

// From Longpath src/gui/instruments/InstrumentSpine.cpp:103 [@551576e3]
QPainterPath ArcSpine::troughSpan(qreal a, qreal b) const
{
    return arcPath(m_r, degreeAt(a), degreeAt(b));
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:108 [@551576e3]
QPainterPath ArcSpine::fillArea(qreal f) const
{
    // Vom Drehpunkt bis knapp unter die Rille — der Sektor trägt die
    // Fläche des Zifferblatts, nicht die Mulde.
    return sectorPath(m_r - m_w / 2.0 - unit() * kSectorGapOfUnit,
                      m_sectorInner, m_deg0, degreeAt(f));
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:116 [@551576e3]
QBrush ArcSpine::fadeBrush(const QColor& c) const
{
    QRadialGradient g(m_c, m_r - m_w / 2.0 - unit() * kSectorGapOfUnit);
    QColor c0 = c; c0.setAlphaF(0.0f);
    QColor c1 = c; c1.setAlphaF(0.10f);
    QColor c2 = c; c2.setAlphaF(0.26f);
    g.setColorAt(0.18, c0);
    g.setColorAt(0.72, c1);
    g.setColorAt(1.00, c2);
    return QBrush(g);
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:128 [@551576e3]
QRectF ArcSpine::glowBounds() const
{
    const qreal r = m_r * m_glowExtent;
    return QRectF(m_c.x() - r, m_c.y() - r, 2 * r, 2 * r);
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:134 [@551576e3]
QPainterPath ArcSpine::glowClip() const
{
    // Nur der Sektor über dem Drehpunkt. Ohne diese Beschneidung
    // liefe die Glut als voller Kreis unter das Instrument.
    return sectorPath(m_r * m_glowExtent, 0.0, m_deg0, m_deg1);
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:141 [@551576e3]
QLineF ArcSpine::tick(qreal f, qreal len) const
{
    const qreal d = degreeAt(f);
    const qreal r0 = m_r - m_w / 2.0 - unit() * kTickGapOfUnit;
    return QLineF(pointAt(r0, d), pointAt(r0 - len, d));
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:148 [@551576e3]
QPointF ArcSpine::labelAnchor(qreal f, qreal off) const
{
    return pointAt(m_r - m_w / 2.0 - off, degreeAt(f));
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:153 [@551576e3]
QLineF ArcSpine::crossAt(qreal f, qreal outward, qreal inward) const
{
    const qreal d = degreeAt(f);
    return QLineF(pointAt(m_r + m_w / 2.0 + outward, d),
                  pointAt(m_r - m_w / 2.0 - inward, d));
}

// ── LinearSpine ──────────────────────────────────────────────────────

// From Longpath src/gui/instruments/InstrumentSpine.cpp:162 [@551576e3]
LinearSpine::LinearSpine(QRectF trough, qreal cornerRadius)
    : m_r(trough), m_rad(cornerRadius)
{
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:167 [@551576e3]
qreal LinearSpine::xAt(qreal f) const
{
    return m_r.left() + m_r.width() * qBound(0.0, f, 1.0);
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:172 [@551576e3]
QPainterPath LinearSpine::troughPath() const
{
    QPainterPath p;
    p.addRoundedRect(m_r, m_rad, m_rad);
    return p;
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:179 [@551576e3]
QPainterPath LinearSpine::shadowPath() const
{
    // Die Kante liegt an der OBEREN Begrenzung — bei der Geraden ist
    // „aussen" oben, so wie beim Bogen „aussen" weg vom Drehpunkt ist.
    QPainterPath p;
    p.addRect(QRectF(m_r.left(), m_r.top(), m_r.width(), shadowWidth()));
    return p.intersected(troughPath());
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:188 [@551576e3]
QPainterPath LinearSpine::troughSpan(qreal a, qreal b) const
{
    QPainterPath p;
    const qreal x0 = xAt(qMin(a, b));
    const qreal x1 = xAt(qMax(a, b));
    p.addRect(QRectF(x0, m_r.top(), x1 - x0, m_r.height()));
    return p.intersected(troughPath());
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:197 [@551576e3]
QPainterPath LinearSpine::fillArea(qreal f) const
{
    // Bei der Geraden ist die Fläche das Innere der Mulde selbst,
    // beschnitten auf die abgerundeten Ecken.
    QPainterPath p;
    p.addRect(QRectF(m_r.left(), m_r.top(), xAt(f) - m_r.left(),
                     m_r.height()));
    return p.intersected(troughPath());
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:207 [@551576e3]
QBrush LinearSpine::fadeBrush(const QColor& c) const
{
    QLinearGradient g(m_r.topLeft(), m_r.topRight());
    QColor c0 = c; c0.setAlphaF(0.05f);
    QColor c1 = c; c1.setAlphaF(0.34f);
    g.setColorAt(0.0, c0);
    g.setColorAt(1.0, c1);
    return QBrush(g);
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:217 [@551576e3]
QRectF LinearSpine::glowBounds() const
{
    // Wie im Entwurf: 78 % der Breite, anderthalb Höhen, um die Mitte.
    const qreal w = m_r.width() * 0.78;
    const qreal h = m_r.height() * 1.5;
    return QRectF(m_r.center().x() - w / 2.0, m_r.center().y() - h / 2.0,
                  w, h);
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:226 [@551576e3]
QPainterPath LinearSpine::glowClip() const
{
    return QPainterPath();   // leer heisst: nicht beschneiden
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:231 [@551576e3]
QLineF LinearSpine::tick(qreal f, qreal len) const
{
    const qreal x = xAt(f);
    return QLineF(QPointF(x, m_r.bottom()), QPointF(x, m_r.bottom() + len));
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:237 [@551576e3]
QPointF LinearSpine::labelAnchor(qreal f, qreal off) const
{
    return QPointF(xAt(f), m_r.bottom() + off);
}

// From Longpath src/gui/instruments/InstrumentSpine.cpp:242 [@551576e3]
QLineF LinearSpine::crossAt(qreal f, qreal outward, qreal inward) const
{
    const qreal x = xAt(f);
    return QLineF(QPointF(x, m_r.top() - outward),
                  QPointF(x, m_r.bottom() + inward));
}

} // namespace NereusSDR
