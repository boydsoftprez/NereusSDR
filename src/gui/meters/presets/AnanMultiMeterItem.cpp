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
//                via Claude Opus 4.7. The Signal/Amps/Power/SWR/Comp
//                calibration tables are byte-for-byte transcriptions
//                of Thetis MeterManager.cs AddAnanMM @501e3f5 —
//                those 5 needles still visually align to the new
//                NereusSDR-original face art. The ALC and Volts
//                tables are re-derived for the new small corner
//                arcs at bottom-left / bottom-right (the original
//                Thetis art placed ALC + Volts on the main meter).
//                The arc-anchoring fix (`bgRect()` + the
//                paintNeedle() pivot/tip mapping) is a NereusSDR
//                addition: Thetis's needle painter anchors to the
//                container rect, which produces visible drift when
//                the container aspect deviates from the background
//                image's natural ratio.
//
//   2026-04-19 — Ported Thetis clsNeedleItem renderNeedle math
//                faithfully (MeterManager.cs:38808-39000 [@501e3f5]).
//                Hoisted pivot/radiusRatio/lengthFactor onto the
//                per-Needle struct so the 3 distinct arc centres
//                on the new face (main center-bottom, bottom-left
//                ALC, bottom-right Volts) can be represented.
// =================================================================

#include "gui/meters/presets/AnanMultiMeterItem.h"
#include "gui/meters/presets/PresetGeometry.h"  // letterboxedBgRect
#include "gui/meters/MeterPoller.h"  // MeterBinding::*

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>
#include <QtGlobal>

#include <cmath>
#include <limits>

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

// NereusSDR-original — Volts calibration re-derived from the new
// face's bottom-right small arc (labels "0 11 12 13 14 15 V", green
// zone marks 13..15 V). The original Thetis Volts table pointed at
// coordinates inside the main meter, which no longer exists on the
// new face. Tick positions measured by overlaying a 0.01-step
// coordinate grid on the image; see session notes for methodology.
// no-port-check: NereusSDR-original calibration.
QMap<float, QPointF> makeVoltsCal()
{
    QMap<float, QPointF> c;
    // Measured on resources/meters/ananMM.png (1504×688).
    c.insert( 0.0f, QPointF(0.722, 0.795));  // "0" — left endpoint
    c.insert(11.0f, QPointF(0.755, 0.770));  // "11" — first numeric tick
    c.insert(12.0f, QPointF(0.790, 0.758));
    c.insert(13.0f, QPointF(0.820, 0.755));
    c.insert(14.0f, QPointF(0.855, 0.760));
    c.insert(15.0f, QPointF(0.890, 0.772));  // "15" — right endpoint
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

// NereusSDR-original — ALC calibration re-derived from the new
// face's bottom-left small arc (labels "0 2 4 6 8 10", red zone
// marks the upper half of the 0..10 dB scale). The original Thetis
// ALC table pointed at coordinates inside the main meter which no
// longer exists on the new face; the scale range also changed
// (Thetis -30..25 dB span → 0..10 dB on the new art).
// no-port-check: NereusSDR-original calibration.
QMap<float, QPointF> makeAlcCal()
{
    QMap<float, QPointF> c;
    // Measured on resources/meters/ananMM.png (1504×688).
    c.insert( 0.0f, QPointF(0.080, 0.795));  // "0" — left endpoint
    c.insert( 2.0f, QPointF(0.130, 0.772));
    c.insert( 4.0f, QPointF(0.180, 0.758));
    c.insert( 6.0f, QPointF(0.230, 0.758));
    c.insert( 8.0f, QPointF(0.280, 0.772));
    c.insert(10.0f, QPointF(0.335, 0.795));  // "10" — right endpoint
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
    // Per-needle geometry (pivot/radiusRatio/lengthFactor):
    //
    //   Signal/Amps/Power/SWR/Compression — keep the Thetis
    //   NeedleOffset (0.004, 0.736) + RadiusRatio (1.0, 0.58) +
    //   per-needle LengthFactor. These 5 needles share the Thetis
    //   main-meter centre-bottom pivot and still align to the new
    //   NereusSDR face image (verified by pixel overlay of the
    //   Thetis calibration tables against the new arc tick marks).
    //
    //   Volts — new face places Volts on a small bottom-right
    //   arc with its own pivot below and right of centre.
    //   Pivot/radius/length re-derived from the new face.
    //
    //   ALC — new face places ALC on a small bottom-left arc
    //   with its own pivot below and left of centre. Pivot/
    //   radius/length re-derived from the new face.
    //
    // Thetis LengthFactor values cited inline below. The shared
    // NeedleOffset (0.004, 0.736) is from MeterManager.cs:22486
    // [@501e3f5]; RadiusRatio (1.0, 0.58) is from :22487 [@501e3f5].

    // Shared main-arc geometry ports (5 needles).
    constexpr QPointF kMainPivot(0.004, 0.736);
    constexpr QPointF kMainRadius(1.0, 0.58);

    // Signal — Thetis LengthFactor 1.65 (MeterManager.cs:22488 [@501e3f5])
    m_needles[0].name         = QStringLiteral("Signal");
    m_needles[0].bindingId    = MeterBinding::SignalAvg;
    m_needles[0].calibration  = makeSignalCal();
    m_needles[0].color        = kColorSignal;
    m_needles[0].pivot        = kMainPivot;
    m_needles[0].radiusRatio  = kMainRadius;
    m_needles[0].lengthFactor = 1.65f;

    // Volts — NereusSDR-original, bottom-right small arc.
    // Arc centre on the new face art is approximately at pixel
    // (1094, 550) on the 1504×688 image, offset from rect centre
    // (752, 344) by (342, 206) — i.e. rect-fraction offset
    // (0.227, 0.299). Arc radius ≈ 185 px; LengthFactor = 2 * 185
    // / w = 370 / 1504 ≈ 0.246. RadiusRatio (1, 1) = circular
    // (the new small arc is a true circular segment); using
    // lengthFactor 0.28 for a small overshoot past the tick marks.
    m_needles[1].name         = QStringLiteral("Volts");
    m_needles[1].bindingId    = MeterBinding::HwVolts;
    m_needles[1].calibration  = makeVoltsCal();
    m_needles[1].color        = kColorVolts;
    m_needles[1].pivot        = QPointF(0.227, 0.299);
    m_needles[1].radiusRatio  = QPointF(1.0, 1.0);
    m_needles[1].lengthFactor = 0.28f;

    // Amps — Thetis LengthFactor 1.15 (MeterManager.cs:22558 [@501e3f5])
    m_needles[2].name         = QStringLiteral("Amps");
    m_needles[2].bindingId    = MeterBinding::HwAmps;
    m_needles[2].calibration  = makeAmpsCal();
    m_needles[2].color        = kColorAmps;
    m_needles[2].pivot        = kMainPivot;
    m_needles[2].radiusRatio  = kMainRadius;
    m_needles[2].lengthFactor = 1.15f;

    // Power — Thetis LengthFactor 1.55 (MeterManager.cs:22628 [@501e3f5])
    m_needles[3].name         = QStringLiteral("Power");
    m_needles[3].bindingId    = MeterBinding::TxPower;
    m_needles[3].calibration  = makePowerCal();
    m_needles[3].color        = kColorPower;
    m_needles[3].pivot        = kMainPivot;
    m_needles[3].radiusRatio  = kMainRadius;
    m_needles[3].lengthFactor = 1.55f;

    // SWR — Thetis LengthFactor 1.36 (MeterManager.cs:22691 [@501e3f5])
    m_needles[4].name         = QStringLiteral("SWR");
    m_needles[4].bindingId    = MeterBinding::TxSwr;
    m_needles[4].calibration  = makeSwrCal();
    m_needles[4].color        = kColorSwr;
    m_needles[4].pivot        = kMainPivot;
    m_needles[4].radiusRatio  = kMainRadius;
    m_needles[4].lengthFactor = 1.36f;

    // Compression — Thetis LengthFactor 0.96 (MeterManager.cs:22722 [@501e3f5])
    m_needles[5].name         = QStringLiteral("Compression");
    m_needles[5].bindingId    = MeterBinding::TxAlcGain;
    m_needles[5].calibration  = makeCompCal();
    m_needles[5].color        = kColorComp;
    m_needles[5].pivot        = kMainPivot;
    m_needles[5].radiusRatio  = kMainRadius;
    m_needles[5].lengthFactor = 0.96f;

    // ALC — NereusSDR-original, bottom-left small arc.
    // Arc centre on the new face art is approximately at pixel
    // (400, 550) on the 1504×688 image, offset from rect centre
    // (752, 344) by (-352, 206) — rect-fraction offset
    // (-0.234, 0.299). Arc radius ≈ 160 px. Same circular
    // RadiusRatio (1, 1) as Volts; LengthFactor 0.24 places the
    // tip on the tick marks.
    m_needles[6].name         = QStringLiteral("ALC");
    m_needles[6].bindingId    = MeterBinding::TxAlcGroup;
    m_needles[6].calibration  = makeAlcCal();
    m_needles[6].color        = kColorAlc;
    m_needles[6].pivot        = QPointF(-0.234, 0.299);
    m_needles[6].radiusRatio  = QPointF(1.0, 1.0);
    m_needles[6].lengthFactor = 0.24f;
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
    // Arc-fix: delegate to the shared preset geometry helper so the
    // letterbox math is identical for every preset that anchors
    // rendering to the background image rect (AnanMM + CrossNeedle).
    // See src/gui/meters/presets/PresetGeometry.h.
    return letterboxedBgRect(pixelRect(widgetW, widgetH),
                             m_background,
                             m_anchorToBgRect);
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
        return QPointF(0.5, 0.5);
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

    // Background image (if present).
    // Wrap drawImage with save/restore + SmoothPixmapTransform so the
    // scaled ananMM.png stays crisp at non-native container sizes
    // (QPainter defaults to nearest-neighbour which aliases badly).
    // Antialiasing also covers sub-pixel seams at the image edge.
    if (!m_background.isNull()) {
        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.drawImage(bg, m_background);
        p.restore();
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

// ---------------------------------------------------------------------------
// paintNeedle — faithful port of Thetis clsNeedleItem renderNeedle math
// (MeterManager.cs:38808-39000 [@501e3f5]). See the inline `// From
// Thetis ...` cites in-body for each step.
//
// Geometry summary (all in bg-rect pixel space):
//   cX, cY       = bg rect centre
//   startX/Y     = pivot = centre + NeedleOffset * bg.size
//                 (NeedleOffset fractions are relative to bg.size, not
//                  normalized image coords; this is what let Thetis
//                  place the pivot OFF the rect, e.g. at y=1.236*h for
//                  the main arcs.)
//   radiusX/Y    = (w/2) * LengthFactor * RadiusRatio.X/Y
//                 (BOTH radii scale by w/2 — not w×h — so wide rects
//                  can reach the right+left tips while RadiusRatio.Y
//                  compresses the vertical sweep.)
//   (eX, eY)     = pixel position of the calibrated point on the face
//   (dX, dY)     = start - cal, then expanded via /= RadiusRatio
//                 (undo the ellipse warp so atan2 gives the arc angle
//                  before the ellipse stretch re-applies it via
//                  radiusX/radiusY on the output.)
//   ang          = atan2(dY, dX) — angle from pivot back to the
//                  calibrated point (i.e. "pointing into the arc from
//                  pivot"); the +180° rotation flips it to point
//                  outward (needle sweeps over the face).
//   endX/Y       = start + cos/sin(ang + 180°) * radiusX/Y
// ---------------------------------------------------------------------------

void AnanMultiMeterItem::paintNeedle(QPainter& p,
                                     const Needle& n,
                                     const QRect& bg) const
{
    if (n.calibration.isEmpty()) {
        return;
    }
    // Edit-container refactor Task 20 — prefer the live value routed
    // via pushBindingValue() over the calibration midpoint. The midpoint
    // is kept as a "no data" fallback so the settings-dialog preview and
    // tests still draw a visible needle before any poll cycle has run.
    auto first = n.calibration.constBegin();
    auto last  = std::prev(n.calibration.constEnd());
    float seed;
    if (std::isnan(n.currentValue)) {
        seed = 0.5f * (first.key() + last.key());
    } else {
        seed = static_cast<float>(n.currentValue);
    }

    // Rect metrics — Thetis uses `x, y, w, h` directly; we mirror that
    // by extracting from the letterboxed bg rect.
    const float x = static_cast<float>(bg.x());
    const float y = static_cast<float>(bg.y());
    const float w = static_cast<float>(bg.width());
    const float h = static_cast<float>(bg.height());

    // From Thetis MeterManager.cs:38823-38826 [@501e3f5] — needle offset
    // from centre. startX/Y is the pivot in pixel space.
    const float cX     = x + (w / 2.0f);
    const float cY     = y + (h / 2.0f);
    const float startX = cX + (w * static_cast<float>(n.pivot.x()));
    const float startY = cY + (h * static_cast<float>(n.pivot.y()));

    // From Thetis MeterManager.cs:38828 [@501e3f5] — rotation is 180°
    // for Bottom placement (the only placement used by AddAnanMM).
    constexpr float kRotationDeg = 180.0f;
    const float rotationRad = kRotationDeg * static_cast<float>(M_PI) / 180.0f;

    // From Thetis MeterManager.cs:38830-38831 [@501e3f5] — both radii
    // scale by (w/2), multiplied by LengthFactor and the per-needle
    // RadiusRatio. CRITICAL: both axes use w/2 (not h/2); this is what
    // lets the main arc sweep across the full width at 1504:688 aspect.
    const float radiusX = (w / 2.0f) * n.lengthFactor *
                          static_cast<float>(n.radiusRatio.x());
    const float radiusY = (w / 2.0f) * n.lengthFactor *
                          static_cast<float>(n.radiusRatio.y());

    // From Thetis MeterManager.cs:38928 [@501e3f5] — getPerc returns
    // the fully-interpolated (eX, eY) pixel position of the calibrated
    // value on the face. We collapse that into a single step: the
    // calibration table already holds normalized (x_frac, y_frac) of
    // the face image, so eX/eY is just bg.topleft + cal*bg.size.
    const QPointF calNorm = calibratedPosition(n, seed);
    const float eX = x + static_cast<float>(calNorm.x()) * w;
    const float eY = y + static_cast<float>(calNorm.y()) * h;

    // From Thetis MeterManager.cs:38935-38940 [@501e3f5] — vector from
    // pivot back to calibrated point, ellipse-expanded so atan2 gives
    // the undistorted arc angle.
    float dX = startX - eX;
    float dY = startY - eY;
    const float rrx = static_cast<float>(n.radiusRatio.x());
    const float rry = static_cast<float>(n.radiusRatio.y());
    if (rrx != 0.0f) { dX /= rrx; }
    if (rry != 0.0f) { dY /= rry; }
    const float ang = std::atan2(dY, dX);

    // From Thetis MeterManager.cs:38942-38943 [@501e3f5] — final tip
    // on the ellipse defined by (radiusX, radiusY), at angle
    // ang + 180° from pivot.
    const float endX = startX + std::cos(ang + rotationRad) * radiusX;
    const float endY = startY + std::sin(ang + rotationRad) * radiusY;

    p.save();
    // Antialiasing on the needle line — drawLine without it produces
    // visibly stair-stepped pixels at the non-axis-aligned angles the
    // needle sweeps through. Negligible cost for a one-pixel line.
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(n.color);
    // From Thetis MeterManager.cs:38945-38951 [@501e3f5] — ScaleStrokeWidth
    // multiplies a 3 px base by sqrt(w^2+h^2)/450. All 7 ANAN needles
    // have ScaleStrokeWidth=true so we always scale.
    const float diag = std::sqrt(w * w + h * h);
    const float fScale = (diag > 0.0f) ? diag / 450.0f : 1.0f;
    constexpr float kStrokeBase = 3.0f;
    pen.setWidthF(qMax(1.0, static_cast<double>(kStrokeBase * fScale)));
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.drawLine(QPointF(startX, startY), QPointF(endX, endY));
    p.restore();
}

// ---------------------------------------------------------------------------
// Edit-container refactor Task 20 — live-value routing. MeterPoller calls
// pushBindingValue(bindingId, value) on every item each tick; walk the 7
// needles and stash the value on the one whose bindingId matches, so the
// next paint() call uses the live tip position.
// ---------------------------------------------------------------------------
void AnanMultiMeterItem::pushBindingValue(int bindingId, double v)
{
    if (bindingId < 0) {
        return;
    }
    for (Needle& n : m_needles) {
        if (n.bindingId == bindingId) {
            n.currentValue = v;
            // Keep MeterItem::m_value roughly tracking whatever binding
            // most recently arrived, mainly so observers that still read
            // value() (e.g. the generic property editor's readout) see a
            // non-stale number. The dispatch above is what actually
            // drives the needle.
            MeterItem::setValue(v);
        }
    }
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
