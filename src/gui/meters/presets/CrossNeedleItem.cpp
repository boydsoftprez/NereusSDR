// =================================================================
// src/gui/meters/presets/CrossNeedleItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs (AddCrossNeedle, line 22817)
//   verbatim upstream header reproduced below.
//
// =================================================================

// --- From MeterManager.cs ---
/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Reimplemented in C++20/Qt6 for NereusSDR by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Claude Opus 4.7. The two calibration tables below
//                are byte-for-byte transcriptions of Thetis
//                MeterManager.cs AddCrossNeedle @501e3f5; the Fwd
//                table has 15 points (0..100W) and the Reflected
//                table has 19 points (0..20 SWR-normalized). The
//                reflected needle's NeedleOffset mirrors the pivot
//                to the right-hand side via a negative X, matching
//                Thetis exactly.
//   2026-04-19 — Arc-fix: pivot/tip geometry now anchors to the
//                letterboxed background image rect (bgRect()) rather
//                than the raw container rect, matching the fix
//                AnanMultiMeterItem already ships. Shared helper in
//                src/gui/meters/presets/PresetGeometry.h.
// =================================================================

#include "gui/meters/presets/CrossNeedleItem.h"
#include "gui/meters/presets/PresetGeometry.h"  // letterboxedBgRect
#include "gui/meters/MeterPoller.h"  // MeterBinding::*

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>
#include <QtGlobal>

namespace NereusSDR {

namespace {

// =================================================================
// Calibration ports — byte-for-byte from Thetis MeterManager.cs
// AddCrossNeedle (~line 22817). Each `// From Thetis ...` cite below
// names the upstream line range that lists the ScaleCalibration
// inserts. Values preserved EXACTLY (per CLAUDE.md "Constants and
// Magic Numbers" rule); float trailing-zero stripping in the
// transcription is cosmetic and does not change the value.
// =================================================================

// From Thetis MeterManager.cs:22843-22857 [@501e3f5] — Forward power
// (0..100W, 15 points).
QMap<float, QPointF> makeForwardCal()
{
    QMap<float, QPointF> c;
    c.insert(  0.0f, QPointF(0.052, 0.732));
    c.insert(  5.0f, QPointF(0.146, 0.528));
    c.insert( 10.0f, QPointF(0.188, 0.434));
    c.insert( 15.0f, QPointF(0.235, 0.387));
    c.insert( 20.0f, QPointF(0.258, 0.338));
    c.insert( 25.0f, QPointF(0.303, 0.313));
    c.insert( 30.0f, QPointF(0.321, 0.272));
    c.insert( 35.0f, QPointF(0.361, 0.257));
    c.insert( 40.0f, QPointF(0.381, 0.223));
    c.insert( 50.0f, QPointF(0.438, 0.181));
    c.insert( 60.0f, QPointF(0.483, 0.155));
    c.insert( 70.0f, QPointF(0.532, 0.130));
    c.insert( 80.0f, QPointF(0.577, 0.111));
    c.insert( 90.0f, QPointF(0.619, 0.098));
    c.insert(100.0f, QPointF(0.662, 0.083));
    return c;
}

// From Thetis MeterManager.cs:22914-22932 [@501e3f5] — Reflected
// power (0..20 SWR-normalized, 19 points).
QMap<float, QPointF> makeReflectedCal()
{
    QMap<float, QPointF> c;
    c.insert( 0.00f, QPointF(0.948, 0.740));
    c.insert( 0.25f, QPointF(0.913, 0.700));
    c.insert( 0.50f, QPointF(0.899, 0.638));
    c.insert( 0.75f, QPointF(0.875, 0.594));
    c.insert( 1.00f, QPointF(0.854, 0.538));
    c.insert( 2.00f, QPointF(0.814, 0.443));
    c.insert( 3.00f, QPointF(0.769, 0.400));
    c.insert( 4.00f, QPointF(0.744, 0.351));
    c.insert( 5.00f, QPointF(0.702, 0.321));
    c.insert( 6.00f, QPointF(0.682, 0.285));
    c.insert( 7.00f, QPointF(0.646, 0.268));
    c.insert( 8.00f, QPointF(0.626, 0.234));
    c.insert( 9.00f, QPointF(0.596, 0.228));
    c.insert(10.00f, QPointF(0.569, 0.196));
    c.insert(12.00f, QPointF(0.524, 0.166));
    c.insert(14.00f, QPointF(0.476, 0.140));
    c.insert(16.00f, QPointF(0.431, 0.121));
    c.insert(18.00f, QPointF(0.393, 0.109));
    c.insert(20.00f, QPointF(0.349, 0.098));
    return c;
}

// Default needle colours — byte-for-byte from Thetis AddCrossNeedle:
// both needles are Color.Black (MeterManager.cs:22837 forward,
// MeterManager.cs:22908 reflected).
constexpr QColor kColorForward  {  0,   0,   0};   // From Thetis MeterManager.cs:22837 [@501e3f5]
constexpr QColor kColorReflected{  0,   0,   0};   // From Thetis MeterManager.cs:22908 [@501e3f5]

} // namespace

// ---------------------------------------------------------------------------
// CrossNeedleItem — construction
// ---------------------------------------------------------------------------

CrossNeedleItem::CrossNeedleItem(QObject* parent)
    : MeterItem(parent)
{
    initialiseNeedles();
    // Background image — registered in resources.qrc as
    // ":/meters/cross-needle.png". Loaded once at construction; a
    // missing resource leaves m_background null and the paint path
    // falls back to drawing into the raw item rect.
    m_background.load(QStringLiteral(":/meters/cross-needle.png"));
    m_bandOverlay.load(QStringLiteral(":/meters/cross-needle-bg.png"));
}

void CrossNeedleItem::initialiseNeedles()
{
    // Forward power needle — Thetis MeterManager.cs:22824-22861.
    m_needles[0] = {
        QStringLiteral("Forward"),
        MeterBinding::TxPower,
        makeForwardCal(),
        kColorForward,
        true,
        QPointF(0.322, 0.611) /* From Thetis MeterManager.cs:22840 [@501e3f5] */,
        1.62f                  /* From Thetis MeterManager.cs:22841 [@501e3f5] */
    };
    // Reflected power needle — Thetis MeterManager.cs:22895-22932.
    // NeedleOffset X is negative — Thetis uses this to mirror the
    // pivot to the right-hand side of the cross face (see
    // MeterManager.cs:22911 `new PointF(-0.322f, 0.611f)`).
    m_needles[1] = {
        QStringLiteral("Reflected"),
        MeterBinding::TxReversePower,
        makeReflectedCal(),
        kColorReflected,
        true,
        QPointF(-0.322, 0.611) /* From Thetis MeterManager.cs:22911 [@501e3f5] */,
        1.62f                   /* From Thetis MeterManager.cs:22912 [@501e3f5] */
    };
}

// ---------------------------------------------------------------------------
// CrossNeedleItem — needle introspection
// ---------------------------------------------------------------------------

QString CrossNeedleItem::needleName(int i) const
{
    if (i < 0 || i >= kNeedleCount) {
        return QString();
    }
    return m_needles[i].name;
}

QMap<float, QPointF> CrossNeedleItem::needleCalibration(int i) const
{
    if (i < 0 || i >= kNeedleCount) {
        return {};
    }
    return m_needles[i].calibration;
}

QColor CrossNeedleItem::needleColor(int i) const
{
    if (i < 0 || i >= kNeedleCount) {
        return QColor();
    }
    return m_needles[i].color;
}

bool CrossNeedleItem::needleVisible(int i) const
{
    if (i < 0 || i >= kNeedleCount) {
        return false;
    }
    return m_needles[i].visible;
}

void CrossNeedleItem::setNeedleVisible(int i, bool v)
{
    if (i < 0 || i >= kNeedleCount) {
        return;
    }
    m_needles[i].visible = v;
}

// ---------------------------------------------------------------------------
// Calibration interpolation — piecewise linear between adjacent points.
// Falls back to the calibration first-point seed when no live data is
// bound, matching Thetis's AddCrossNeedle:22858 and :22933
// "Value = first calibration key" seed for each needle.
// ---------------------------------------------------------------------------

QPointF CrossNeedleItem::calibratedPosition(const Needle& n, float value) const
{
    if (n.calibration.isEmpty()) {
        return QPointF(0.5, 0.5);
    }
    auto first = n.calibration.constBegin();
    auto last  = std::prev(n.calibration.constEnd());
    if (value <= first.key()) {
        return first.value();
    }
    if (value >= last.key()) {
        return last.value();
    }
    auto upper = n.calibration.lowerBound(value);
    if (upper == n.calibration.constBegin()) {
        return upper.value();
    }
    auto lower = std::prev(upper);

    const float a   = lower.key();
    const float b   = upper.key();
    const float t   = (b == a) ? 0.0f : (value - a) / (b - a);
    const QPointF& pa = lower.value();
    const QPointF& pb = upper.value();
    return QPointF(pa.x() + t * (pb.x() - pa.x()),
                   pa.y() + t * (pb.y() - pa.y()));
}

// ---------------------------------------------------------------------------
// bgRect — letterboxed background-image rect (arc-fix)
// ---------------------------------------------------------------------------
//
// Same math as AnanMultiMeterItem::bgRect() — both presets share the
// helper in PresetGeometry.h. At non-default aspect ratios the
// container rect is wider or taller than the cross-needle.png image;
// letterboxing anchors the needles to the painted image so the arcs
// stay on the meter face.

QRect CrossNeedleItem::bgRect(int widgetW, int widgetH) const
{
    return letterboxedBgRect(pixelRect(widgetW, widgetH),
                             m_background,
                             m_anchorToBgRect);
}

// ---------------------------------------------------------------------------
// paint — orchestrates background + band overlay + needles
// ---------------------------------------------------------------------------

void CrossNeedleItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.width() <= 0 || rect.height() <= 0) {
        return;
    }
    // Arc-fix: anchor everything that maps to background-image
    // normalized coordinates (drawImage target, needle pivot/tip,
    // band overlay strip) to the letterboxed rect.
    const QRect bg = bgRect(widgetW, widgetH);

    // z=1 Background image (Thetis line 22973-22980).
    if (!m_background.isNull()) {
        p.drawImage(bg, m_background);
    }

    // z=4 / z=3 Needles (Thetis line 22838 fwd ZOrder=4, 22907 rev
    // ZOrder=3). Paint the higher-z needle first so the lower-z
    // (reflected) draws on top — matches Thetis paint ordering.
    for (const Needle& n : m_needles) {
        if (!n.visible) {
            continue;
        }
        paintNeedle(p, n, bg);
    }

    // z=5 Band overlay (Thetis line 22982-22989). Small band image
    // along the bottom strip; cross-needle-bg.png is 1:0.217 aspect.
    // Anchor to `bg` (the letterboxed image rect) rather than the
    // container `rect` so the overlay stays pinned to the bottom of
    // the meter face at non-default aspect ratios.
    if (m_showBandOverlay && !m_bandOverlay.isNull()) {
        const int overlayH = static_cast<int>(bg.height() * 0.217);
        const int overlayY = bg.y() + bg.height() - overlayH;
        const QRect overlayRect(bg.x(), overlayY, bg.width(), overlayH);
        p.drawImage(overlayRect, m_bandOverlay);
    }
}

void CrossNeedleItem::paintNeedle(QPainter& p,
                                  const Needle& n,
                                  const QRect& rect) const
{
    if (n.calibration.isEmpty()) {
        return;
    }
    // TODO(wdsp-poller): replace first-key seed with live value from
    // the MeterPoller once composite-item polling is wired.
    auto first = n.calibration.constBegin();
    const float seed = first.key();

    // pivot_px = rect.topLeft + NeedleOffset * rect.size.
    // NeedleOffset is the Thetis per-needle pivot position in
    // normalized image coordinates; the reflected needle uses
    // negative X to mirror to the right-hand side.
    const QPointF pivotPx(rect.x() + n.needleOffset.x() * rect.width(),
                          rect.y() + n.needleOffset.y() * rect.height());
    const QPointF calNorm = calibratedPosition(n, seed);
    const QPointF calPx(rect.x() + calNorm.x() * rect.width(),
                        rect.y() + calNorm.y() * rect.height());
    // Apply LengthFactor in the pivot→calibration direction
    // (Thetis: tip = pivot + LengthFactor * (cal - pivot)).
    const QPointF tipPx(pivotPx.x() + n.lengthFactor * (calPx.x() - pivotPx.x()),
                        pivotPx.y() + n.lengthFactor * (calPx.y() - pivotPx.y()));

    p.save();
    QPen pen(n.color);
    // Thetis StrokeWidth = 2.5f (MeterManager.cs:22836 / :22902).
    pen.setWidthF(qMax(1.0, rect.height() * 0.005));
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(pivotPx, tipPx);
    p.restore();
}

// ---------------------------------------------------------------------------
// Serialization — compact JSON, "CrossNeedle" kind tag.
// ---------------------------------------------------------------------------

QString CrossNeedleItem::serialize() const
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"),         QStringLiteral("CrossNeedle"));
    o.insert(QStringLiteral("x"),            static_cast<double>(m_x));
    o.insert(QStringLiteral("y"),            static_cast<double>(m_y));
    o.insert(QStringLiteral("w"),            static_cast<double>(m_w));
    o.insert(QStringLiteral("h"),            static_cast<double>(m_h));
    o.insert(QStringLiteral("bandOverlay"),  m_showBandOverlay);
    o.insert(QStringLiteral("anchorToBg"),   m_anchorToBgRect);

    QJsonArray vis;
    for (const Needle& n : m_needles) {
        vis.append(n.visible);
    }
    o.insert(QStringLiteral("needlesVisible"), vis);

    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool CrossNeedleItem::deserialize(const QString& data)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("CrossNeedle")) {
        return false;
    }

    setRect(static_cast<float>(o.value(QStringLiteral("x")).toDouble(m_x)),
            static_cast<float>(o.value(QStringLiteral("y")).toDouble(m_y)),
            static_cast<float>(o.value(QStringLiteral("w")).toDouble(m_w)),
            static_cast<float>(o.value(QStringLiteral("h")).toDouble(m_h)));
    m_showBandOverlay = o.value(QStringLiteral("bandOverlay")).toBool(m_showBandOverlay);
    m_anchorToBgRect  = o.value(QStringLiteral("anchorToBg")).toBool(m_anchorToBgRect);

    const QJsonArray vis = o.value(QStringLiteral("needlesVisible")).toArray();
    for (int i = 0; i < kNeedleCount; ++i) {
        if (i < vis.size()) {
            m_needles[i].visible = vis.at(i).toBool(true);
        }
    }
    return true;
}

} // namespace NereusSDR
