// =================================================================
// src/gui/meters/presets/AnanMultiMeterItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs (AddAnanMM, line 22461)
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
//                via Claude Opus 4.7. The seven calibration tables
//                below are byte-for-byte transcriptions of Thetis
//                MeterManager.cs AddAnanMM @501e3f5; they preserve
//                the Reading-source assignment order
//                (Signal=0, Volts=1, Amps=2, Pwr=3, Swr=4,
//                AlcComp=5, AlcGroup=6) that the Thetis poller and
//                the NereusSDR MeterPoller / MeterBinding constants
//                already follow. The arc-anchoring fix
//                (`bgRect()` + the paintNeedle() pivot/tip mapping)
//                is a NereusSDR addition: Thetis's needle painter
//                anchors to the container rect, which produces
//                visible drift when the container aspect deviates
//                from the background image's natural ratio.
// =================================================================

#include "gui/meters/presets/AnanMultiMeterItem.h"
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
// AddAnanMM (~line 22461). Each `// From Thetis ...` cite below
// names the upstream line range that lists the ScaleCalibration
// inserts. Values preserved EXACTLY (per CLAUDE.md "Constants and
// Magic Numbers" rule); float trailing-zero stripping in the
// transcription is cosmetic and does not change the value.
// =================================================================

// From Thetis MeterManager.cs:22491-22507 [@501e3f5] — Signal (-127..-13 dBm, 16 points)
QMap<float, QPointF> makeSignalCal()
{
    QMap<float, QPointF> c;
    c.insert(-127.0f, QPointF(0.076, 0.310));
    c.insert(-121.0f, QPointF(0.131, 0.272));
    c.insert(-115.0f, QPointF(0.189, 0.254));
    c.insert(-109.0f, QPointF(0.233, 0.211));
    c.insert(-103.0f, QPointF(0.284, 0.207));
    c.insert( -97.0f, QPointF(0.326, 0.177));
    c.insert( -91.0f, QPointF(0.374, 0.177));
    c.insert( -85.0f, QPointF(0.414, 0.151));
    c.insert( -79.0f, QPointF(0.459, 0.168));
    c.insert( -73.0f, QPointF(0.501, 0.142));
    c.insert( -63.0f, QPointF(0.564, 0.172));
    c.insert( -53.0f, QPointF(0.630, 0.164));
    c.insert( -43.0f, QPointF(0.695, 0.203));
    c.insert( -33.0f, QPointF(0.769, 0.211));
    c.insert( -23.0f, QPointF(0.838, 0.272));
    c.insert( -13.0f, QPointF(0.926, 0.310));
    return c;
}

// From Thetis MeterManager.cs:22534-22537 [@501e3f5] — Volts (10..15 V, 3 points)
QMap<float, QPointF> makeVoltsCal()
{
    QMap<float, QPointF> c;
    c.insert(10.0f,  QPointF(0.559, 0.756));
    c.insert(12.5f,  QPointF(0.605, 0.772));
    c.insert(15.0f,  QPointF(0.665, 0.784));
    return c;
}

// From Thetis MeterManager.cs:22562-22573 [@501e3f5] — Amps (0..20 A, 11 points)
QMap<float, QPointF> makeAmpsCal()
{
    QMap<float, QPointF> c;
    c.insert( 0.0f, QPointF(0.199, 0.576));
    c.insert( 2.0f, QPointF(0.270, 0.540));
    c.insert( 4.0f, QPointF(0.333, 0.516));
    c.insert( 6.0f, QPointF(0.393, 0.504));
    c.insert( 8.0f, QPointF(0.448, 0.492));
    c.insert(10.0f, QPointF(0.499, 0.492));
    c.insert(12.0f, QPointF(0.554, 0.488));
    c.insert(14.0f, QPointF(0.608, 0.500));
    c.insert(16.0f, QPointF(0.667, 0.516));
    c.insert(18.0f, QPointF(0.728, 0.540));
    c.insert(20.0f, QPointF(0.799, 0.576));
    return c;
}

// From Thetis MeterManager.cs:22632-22641 [@501e3f5] — Power (0..150 W, 10 points)
QMap<float, QPointF> makePowerCal()
{
    QMap<float, QPointF> c;
    c.insert(  0.0f, QPointF(0.099, 0.352));
    c.insert(  5.0f, QPointF(0.164, 0.312));
    c.insert( 10.0f, QPointF(0.224, 0.280));
    c.insert( 25.0f, QPointF(0.335, 0.236));
    c.insert( 30.0f, QPointF(0.367, 0.228));
    c.insert( 40.0f, QPointF(0.436, 0.220));
    c.insert( 50.0f, QPointF(0.499, 0.212));
    c.insert( 60.0f, QPointF(0.559, 0.216));
    c.insert(100.0f, QPointF(0.751, 0.272));
    c.insert(150.0f, QPointF(0.899, 0.352));
    return c;
}

// From Thetis MeterManager.cs:22694-22699 [@501e3f5] — SWR (1..10, 6 points)
QMap<float, QPointF> makeSwrCal()
{
    QMap<float, QPointF> c;
    c.insert( 1.0f, QPointF(0.152, 0.468));
    c.insert( 1.5f, QPointF(0.280, 0.404));
    c.insert( 2.0f, QPointF(0.393, 0.372));
    c.insert( 2.5f, QPointF(0.448, 0.360));
    c.insert( 3.0f, QPointF(0.499, 0.360));
    c.insert(10.0f, QPointF(0.847, 0.476));
    return c;
}

// From Thetis MeterManager.cs:22725-22731 [@501e3f5] — Compression (0..30 dB, 7 points)
QMap<float, QPointF> makeCompCal()
{
    QMap<float, QPointF> c;
    c.insert( 0.0f, QPointF(0.249, 0.680));
    c.insert( 5.0f, QPointF(0.342, 0.640));
    c.insert(10.0f, QPointF(0.425, 0.624));
    c.insert(15.0f, QPointF(0.499, 0.620));
    c.insert(20.0f, QPointF(0.571, 0.628));
    c.insert(25.0f, QPointF(0.656, 0.640));
    c.insert(30.0f, QPointF(0.751, 0.688));
    return c;
}

// From Thetis MeterManager.cs:22759-22761 [@501e3f5] — ALC (-30..25 dB, 3 points)
QMap<float, QPointF> makeAlcCal()
{
    QMap<float, QPointF> c;
    c.insert(-30.0f, QPointF(0.295, 0.804));
    c.insert(  0.0f, QPointF(0.332, 0.784));
    c.insert( 25.0f, QPointF(0.499, 0.756));
    return c;
}

// Default needle colours — byte-for-byte from Thetis AddAnanMM.
// Signal and Power are the red "primary" needles
// (FromArgb(255, 233, 51, 50)); all other needles are Color.Black.
// Stored per-needle and editable via the property pane (Task 11);
// these are only the construction defaults.
constexpr QColor kColorSignal{233,  51,  50};   // From Thetis MeterManager.cs:22483 [@501e3f5]
constexpr QColor kColorVolts {  0,   0,   0};   // From Thetis MeterManager.cs:22526 [@501e3f5]
constexpr QColor kColorAmps  {  0,   0,   0};   // From Thetis MeterManager.cs:22554 [@501e3f5]
constexpr QColor kColorPower {233,  51,  50};   // From Thetis MeterManager.cs:22623 [@501e3f5]
constexpr QColor kColorSwr   {  0,   0,   0};   // From Thetis MeterManager.cs:22686 [@501e3f5]
constexpr QColor kColorComp  {  0,   0,   0};   // From Thetis MeterManager.cs:22718 [@501e3f5]
constexpr QColor kColorAlc   {  0,   0,   0};   // From Thetis MeterManager.cs:22750 [@501e3f5]

} // namespace

// ---------------------------------------------------------------------------
// AnanMultiMeterItem — construction
// ---------------------------------------------------------------------------

AnanMultiMeterItem::AnanMultiMeterItem(QObject* parent)
    : MeterItem(parent)
{
    initialiseNeedles();
    // Background image — registered in resources.qrc:14 as
    // ":/meters/ananMM.png". Loaded once at construction; a missing
    // resource leaves m_background null and the paint path falls back
    // to drawing into the raw item rect.
    m_background.load(QStringLiteral(":/meters/ananMM.png"));
}

void AnanMultiMeterItem::initialiseNeedles()
{
    // The 0..6 array index below mirrors Thetis AddAnanMM's
    // `MMIOVariableIndex` slot ordering (Signal=0, Volts=1, Amps=2,
    // Pwr=3, Swr=4, AlcComp=5, AlcGroup=6). That ordinal is a
    // per-preset Thetis MMIO export slot — NOT a runtime data-source
    // identifier and NOT the same integer space as
    // `MeterBinding::*`. Runtime needle-to-WDSP routing uses the
    // `MeterBinding::*` constants in `src/gui/meters/MeterPoller.h`,
    // which are wholly independent values (e.g. SignalAvg = 1,
    // HwVolts = 200, HwAmps = 201, TxPower = 100, TxSwr = 102,
    // TxAlcGain = 109, TxAlcGroup = 110). The two schemes happen to
    // collide for slot 1 (both Volts and SignalAvg are `1`), which
    // is coincidence — keep them mentally separate.
    //
    // Per-needle `lengthFactor` values are byte-for-byte from
    // Thetis MeterManager.cs AddAnanMM; see paintNeedle() for how
    // the factor extends the tip past the calibration point.
    m_needles[0] = {QStringLiteral("Signal"),      MeterBinding::SignalAvg,    makeSignalCal(), kColorSignal, true,
                    1.65f /* From Thetis MeterManager.cs:22488 [@501e3f5] */};
    m_needles[1] = {QStringLiteral("Volts"),       MeterBinding::HwVolts,      makeVoltsCal(),  kColorVolts,  true,
                    0.75f /* From Thetis MeterManager.cs:22530 [@501e3f5] */};
    m_needles[2] = {QStringLiteral("Amps"),        MeterBinding::HwAmps,       makeAmpsCal(),   kColorAmps,   true,
                    1.15f /* From Thetis MeterManager.cs:22558 [@501e3f5] */};
    m_needles[3] = {QStringLiteral("Power"),       MeterBinding::TxPower,      makePowerCal(),  kColorPower,  true,
                    1.55f /* From Thetis MeterManager.cs:22628 [@501e3f5] */};
    m_needles[4] = {QStringLiteral("SWR"),         MeterBinding::TxSwr,        makeSwrCal(),    kColorSwr,    true,
                    1.36f /* From Thetis MeterManager.cs:22691 [@501e3f5] */};
    m_needles[5] = {QStringLiteral("Compression"), MeterBinding::TxAlcGain,    makeCompCal(),   kColorComp,   true,
                    0.96f /* From Thetis MeterManager.cs:22722 [@501e3f5] */};
    m_needles[6] = {QStringLiteral("ALC"),         MeterBinding::TxAlcGroup,   makeAlcCal(),    kColorAlc,    true,
                    0.75f /* From Thetis MeterManager.cs:22755 [@501e3f5] */};
}

// ---------------------------------------------------------------------------
// AnanMultiMeterItem — needle introspection
// ---------------------------------------------------------------------------

QString AnanMultiMeterItem::needleName(int i) const
{
    if (i < 0 || i >= kNeedleCount) {
        return QString();
    }
    return m_needles[i].name;
}

QMap<float, QPointF> AnanMultiMeterItem::needleCalibration(int i) const
{
    if (i < 0 || i >= kNeedleCount) {
        return {};
    }
    return m_needles[i].calibration;
}

QColor AnanMultiMeterItem::needleColor(int i) const
{
    if (i < 0 || i >= kNeedleCount) {
        return QColor();
    }
    return m_needles[i].color;
}

bool AnanMultiMeterItem::needleVisible(int i) const
{
    if (i < 0 || i >= kNeedleCount) {
        return false;
    }
    return m_needles[i].visible;
}

void AnanMultiMeterItem::setNeedleVisible(int i, bool v)
{
    if (i < 0 || i >= kNeedleCount) {
        return;
    }
    m_needles[i].visible = v;
}

// ---------------------------------------------------------------------------
// Arc-fix — bgRect()
//
// Returns the rectangle the painted background image occupies inside
// the item rect, preserving the image's natural aspect ratio
// (letterbox layout). Pivot/radius are normalized against this rect
// rather than the raw item rect so the needles stay glued to the
// painted meter face when the container aspect ratio deviates from
// the image's natural ratio. When `m_anchorToBgRect` is false (or
// the background failed to load) the method returns the item's pixel
// rect directly, restoring the pre-port behaviour.
// ---------------------------------------------------------------------------

QRect AnanMultiMeterItem::bgRect(int widgetW, int widgetH) const
{
    const QRect itemR = pixelRect(widgetW, widgetH);
    // Guard against (a) anchor disabled, (b) background image failed
    // to load, and (c) degenerate image dimensions that would divide
    // by zero in the aspect-ratio calculation. In any of those cases,
    // fall back to the item's pixel rect (pre-port behaviour).
    if (!m_anchorToBgRect || m_background.isNull() || m_background.height() <= 0) {
        return itemR;
    }
    if (itemR.width() <= 0 || itemR.height() <= 0) {
        return itemR;
    }
    const float imgAspect = static_cast<float>(m_background.width())
                          / static_cast<float>(m_background.height());
    const float rectAspect = static_cast<float>(itemR.width())
                           / static_cast<float>(itemR.height());

    if (rectAspect > imgAspect) {
        // Item wider than the image — letterbox horizontally.
        const int drawW = static_cast<int>(itemR.height() * imgAspect);
        const int drawX = itemR.x() + (itemR.width() - drawW) / 2;
        return QRect(drawX, itemR.y(), drawW, itemR.height());
    }
    // Item taller than (or matches) the image — letterbox vertically.
    const int drawH = static_cast<int>(itemR.width() / imgAspect);
    const int drawY = itemR.y() + (itemR.height() - drawH) / 2;
    return QRect(itemR.x(), drawY, itemR.width(), drawH);
}

// ---------------------------------------------------------------------------
// Calibration interpolation — piecewise linear between adjacent points.
// Falls back to the calibration midpoint when no live data is bound,
// matching Thetis's AddAnanMM "Value = first calibration key" seed for
// each needle (MeterManager.cs:22510, 22538, 22573, 22606, 22643,
// 22678, 22707). For a no-data needle this puts the tip on the meter
// face rather than at an undefined position.
// ---------------------------------------------------------------------------

QPointF AnanMultiMeterItem::calibratedPosition(const Needle& n, float value) const
{
    if (n.calibration.isEmpty()) {
        return QPointF(m_pivot.x(), m_pivot.y());
    }
    // Clamp to the calibration range — Thetis's needle paint also
    // clamps via "Value = first key" / "Value = last key" guards.
    auto first = n.calibration.constBegin();
    auto last  = std::prev(n.calibration.constEnd());
    if (value <= first.key()) {
        return first.value();
    }
    if (value >= last.key()) {
        return last.value();
    }

    // lowerBound returns first iterator with key >= value. Pair it
    // with the previous iterator to bracket the value.
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
// paint — orchestrates background + needles + (optional) debug overlay
// ---------------------------------------------------------------------------

void AnanMultiMeterItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect bg = bgRect(widgetW, widgetH);
    if (bg.width() <= 0 || bg.height() <= 0) {
        return;
    }

    // Background image (if present)
    if (!m_background.isNull()) {
        p.drawImage(bg, m_background);
    }

    // Needles
    for (const Needle& n : m_needles) {
        if (!n.visible) {
            continue;
        }
        paintNeedle(p, n, bg);
    }

    // Debug overlay (calibration-point dots)
    if (m_debugOverlay) {
        paintDebugOverlay(p, bg);
    }
}

void AnanMultiMeterItem::paintNeedle(QPainter& p,
                                     const Needle& n,
                                     const QRect& bg) const
{
    if (n.calibration.isEmpty()) {
        return;
    }
    // TODO(wdsp-poller): replace midpoint seed with live value from the
    // MeterPoller once composite-item polling is wired (see design spec
    // §3.1 binding notes — per-needle m_value map or poller-side dispatch).
    auto first = n.calibration.constBegin();
    auto last  = std::prev(n.calibration.constEnd());
    const float seed = 0.5f * (first.key() + last.key());

    // pivot_px = bg.topLeft + m_pivot * bg.size; the calibrated
    // point sits on the meter face. Two independent geometry knobs
    // shape the final tip:
    //   - m_radiusRatio (Thetis `RadiusRatio`, default (1, 0.58)):
    //     shrinks/stretches the whole pivot→calibration vector.
    //     One value per preset; wired to the property pane in
    //     Task 11.
    //   - n.lengthFactor (Thetis `LengthFactor`, per-needle):
    //     extends the tip past the calibrated point by the factor
    //     (1.0 = exact calibration, >1.0 overshoots, <1.0 stops
    //     short). Set in initialiseNeedles() from byte-for-byte
    //     Thetis values.
    // The two are orthogonal — radiusRatio modulates the arc as a
    // whole; lengthFactor scales each needle's reach individually.
    const QPointF pivotPx(bg.x() + m_pivot.x() * bg.width(),
                          bg.y() + m_pivot.y() * bg.height());
    const QPointF calNorm = calibratedPosition(n, seed);
    const QPointF calPx(bg.x() + calNorm.x() * bg.width(),
                        bg.y() + calNorm.y() * bg.height());
    // Apply LengthFactor in the pivot→calibration direction
    // (Thetis: tip = pivot + LengthFactor * (cal - pivot)).
    const QPointF tipBase(pivotPx.x() + n.lengthFactor * (calPx.x() - pivotPx.x()),
                          pivotPx.y() + n.lengthFactor * (calPx.y() - pivotPx.y()));
    const QPointF tipPx(pivotPx.x() + (tipBase.x() - pivotPx.x()) * m_radiusRatio.x(),
                        pivotPx.y() + (tipBase.y() - pivotPx.y()) * m_radiusRatio.y());

    p.save();
    QPen pen(n.color);
    pen.setWidthF(qMax(1.0, bg.height() * 0.005));
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(pivotPx, tipPx);
    p.restore();
}

void AnanMultiMeterItem::paintDebugOverlay(QPainter& p, const QRect& bg) const
{
    p.save();
    p.setPen(Qt::NoPen);
    const qreal dotR = qMax<qreal>(2.5, bg.height() * 0.006);
    for (const Needle& n : m_needles) {
        if (!n.visible) {
            continue;
        }
        p.setBrush(n.color);
        for (auto it = n.calibration.constBegin();
             it != n.calibration.constEnd(); ++it) {
            const QPointF& norm = it.value();
            const QPointF px(bg.x() + norm.x() * bg.width(),
                             bg.y() + norm.y() * bg.height());
            p.drawEllipse(px, dotR, dotR);
        }
    }
    p.restore();
}

// ---------------------------------------------------------------------------
// Serialization — compact JSON, "AnanMM" kind tag.
// ---------------------------------------------------------------------------

QString AnanMultiMeterItem::serialize() const
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"),     QStringLiteral("AnanMM"));
    o.insert(QStringLiteral("x"),        static_cast<double>(m_x));
    o.insert(QStringLiteral("y"),        static_cast<double>(m_y));
    o.insert(QStringLiteral("w"),        static_cast<double>(m_w));
    o.insert(QStringLiteral("h"),        static_cast<double>(m_h));
    o.insert(QStringLiteral("pivotX"),   m_pivot.x());
    o.insert(QStringLiteral("pivotY"),   m_pivot.y());
    o.insert(QStringLiteral("radiusX"),  m_radiusRatio.x());
    o.insert(QStringLiteral("radiusY"),  m_radiusRatio.y());
    o.insert(QStringLiteral("anchorBg"), m_anchorToBgRect);
    o.insert(QStringLiteral("debug"),    m_debugOverlay);

    QJsonArray vis;
    for (const Needle& n : m_needles) {
        vis.append(n.visible);
    }
    o.insert(QStringLiteral("needlesVisible"), vis);

    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool AnanMultiMeterItem::deserialize(const QString& data)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("AnanMM")) {
        return false;
    }

    setRect(static_cast<float>(o.value(QStringLiteral("x")).toDouble(m_x)),
            static_cast<float>(o.value(QStringLiteral("y")).toDouble(m_y)),
            static_cast<float>(o.value(QStringLiteral("w")).toDouble(m_w)),
            static_cast<float>(o.value(QStringLiteral("h")).toDouble(m_h)));
    m_pivot = QPointF(o.value(QStringLiteral("pivotX")).toDouble(m_pivot.x()),
                      o.value(QStringLiteral("pivotY")).toDouble(m_pivot.y()));
    m_radiusRatio = QPointF(o.value(QStringLiteral("radiusX")).toDouble(m_radiusRatio.x()),
                            o.value(QStringLiteral("radiusY")).toDouble(m_radiusRatio.y()));
    m_anchorToBgRect = o.value(QStringLiteral("anchorBg")).toBool(m_anchorToBgRect);
    m_debugOverlay   = o.value(QStringLiteral("debug")).toBool(m_debugOverlay);

    const QJsonArray vis = o.value(QStringLiteral("needlesVisible")).toArray();
    for (int i = 0; i < kNeedleCount; ++i) {
        if (i < vis.size()) {
            m_needles[i].visible = vis.at(i).toBool(true);
        }
    }
    return true;
}

} // namespace NereusSDR
