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

#include <QFont>
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

    // Seed per-needle peak-hold tracking below the calibration
    // range so the first push_value() acts as the initial max.
    for (int i = 0; i < kNeedleCount; ++i) {
        m_peakHolds[i] = -std::numeric_limits<double>::infinity();
    }
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

    // Shared main-arc geometry — NereusSDR-original, derived by
    // visual + numeric iteration against the new face art.
    //
    // Pivot at rect-fraction offset (0.0, 0.390) → pixel (752, 612)
    // for the 1504×688 face: centre-x, 89% down, on the visible
    // root-arc line just above the "NEREUS SDR" text band.
    //
    // RadiusRatio (0.50, 0.40) combined with per-needle LFs below
    // (tuned so each needle's midpoint tip lands on its respective
    // scale on the face) scales the tip ellipse to fit inside the
    // face instead of Thetis's wider ananMM.png geometry. Verified
    // numerically: Signal/Amps/Comp midpoints spot-on; Power/SWR
    // within ~25 px of their calibration midpoints.
    constexpr QPointF kMainPivot(0.0, 0.390);
    constexpr QPointF kMainRadius(0.50, 0.40);

    // Signal — NereusSDR-original lengthFactor 1.70 (tuned for new face;
    // Thetis used 1.65 at MeterManager.cs:22488 [@501e3f5] for its
    // different face geometry).
    m_needles[0].name         = QStringLiteral("Signal");
    m_needles[0].bindingId    = MeterBinding::SignalAvg;
    m_needles[0].calibration  = makeSignalCal();
    m_needles[0].color        = kColorSignal;
    m_needles[0].pivot        = kMainPivot;
    m_needles[0].radiusRatio  = kMainRadius;
    m_needles[0].lengthFactor = 1.70f;

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
    m_needles[1].lengthFactor = 0.10f;   // Volts — trim tip to sub-arc

    // Amps — NereusSDR-original lengthFactor 0.91 (Thetis 1.15 at
    // MeterManager.cs:22558 [@501e3f5] — retuned for new face geometry).
    m_needles[2].name         = QStringLiteral("Amps");
    m_needles[2].bindingId    = MeterBinding::HwAmps;
    m_needles[2].calibration  = makeAmpsCal();
    m_needles[2].color        = kColorAmps;
    m_needles[2].pivot        = kMainPivot;
    m_needles[2].radiusRatio  = kMainRadius;
    m_needles[2].lengthFactor = 0.91f;

    // Power — NereusSDR-original lengthFactor 1.67 (Thetis 1.55 at
    // MeterManager.cs:22628 [@501e3f5]).
    m_needles[3].name         = QStringLiteral("Power");
    m_needles[3].bindingId    = MeterBinding::TxPower;
    m_needles[3].calibration  = makePowerCal();
    m_needles[3].color        = kColorPower;
    m_needles[3].pivot        = kMainPivot;
    m_needles[3].radiusRatio  = kMainRadius;
    m_needles[3].lengthFactor = 1.67f;

    // SWR — NereusSDR-original lengthFactor 1.28 (Thetis 1.36 at
    // MeterManager.cs:22691 [@501e3f5]).
    m_needles[4].name         = QStringLiteral("SWR");
    m_needles[4].bindingId    = MeterBinding::TxSwr;
    m_needles[4].calibration  = makeSwrCal();
    m_needles[4].color        = kColorSwr;
    m_needles[4].pivot        = kMainPivot;
    m_needles[4].radiusRatio  = kMainRadius;
    m_needles[4].lengthFactor = 1.28f;

    // Compression — NereusSDR-original lengthFactor 0.61 (Thetis 0.96 at
    // MeterManager.cs:22722 [@501e3f5]).
    m_needles[5].name         = QStringLiteral("Compression");
    m_needles[5].bindingId    = MeterBinding::TxAlcGain;
    m_needles[5].calibration  = makeCompCal();
    m_needles[5].color        = kColorComp;
    m_needles[5].pivot        = kMainPivot;
    m_needles[5].radiusRatio  = kMainRadius;
    m_needles[5].lengthFactor = 0.61f;

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
    m_needles[6].lengthFactor = 0.13f;   // ALC — trim tip to sub-arc
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

void AnanMultiMeterItem::setNeedleColor(int i, const QColor& c)
{
    if (i < 0 || i >= kNeedleCount) {
        return;
    }
    m_needles[i].color = c;
}

// Thetis property-editor parity — switching the Signal source
// rebinds the Signal needle (index 0) to the matching
// MeterBinding::* ID so MeterPoller will route the picked
// reading into the same needle slot on the next tick.
void AnanMultiMeterItem::setSignalSource(SignalSource s)
{
    m_signalSource = s;
    int newBinding = MeterBinding::SignalAvg;
    switch (s) {
        case SignalSource::Avg:    newBinding = MeterBinding::SignalAvg;    break;
        case SignalSource::Peak:   newBinding = MeterBinding::SignalPeak;   break;
        case SignalSource::MaxBin: newBinding = MeterBinding::SignalMaxBin; break;
    }
    m_needles[0].bindingId = newBinding;
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

    // Thetis property-editor parity — user-chosen background colour
    // painted underneath the face image. alpha == 0 skips the fill so
    // the default behaviour (just the image) stays intact. Dark mode
    // overrides the user colour with a near-black tint so the rest of
    // the render scheme reads correctly on a dark chrome.
    const QColor bgFill = m_darkMode ? QColor(10, 10, 18, 255)
                                     : m_backgroundColor;
    if (bgFill.alpha() > 0) {
        p.save();
        p.fillRect(bg, bgFill);
        p.restore();
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

    // TODO(render-mode-shadow): implement paint-time effect;
    // currently storage-only. Thetis Shadow decorates the bar
    // portion of a composite — ANAN MM is pure needles so this
    // field is preserved for Thetis-parity round-trip but produces
    // no visible effect here.
    // TODO(render-mode-segmented): implement paint-time effect;
    // currently storage-only. Same reasoning as Shadow.
    // TODO(render-mode-solid): implement paint-time effect;
    // currently storage-only. Same reasoning as Shadow.
    // TODO(render-mode-history): implement paint-time effect;
    // currently storage-only. Needs a rolling per-needle sample
    // buffer driven off m_historyMs.
    // TODO(render-mode-attack-decay): Attack/Decay ratios stored;
    // needle smoothing applies at setValue() time (future work).
    // TODO(fade-coupling): once RadioModel exposes a global
    // mox()/rx state, hide the needles whose opposite flag is set
    // for the current state.

    // Fade on RX/TX: without a RadioModel handle here, we fall back
    // to "hide the whole needle array when user has asked to fade in
    // the current environment". The flag is evaluated against the
    // settings-dialog preview context (neither RX nor TX) as a no-op,
    // so the storage path round-trips without breaking the preview.
    const bool suppressNeedles = false;  // TODO(fade-coupling) honour live TX/RX

    // Needles
    for (int i = 0; i < kNeedleCount; ++i) {
        const Needle& n = m_needles[i];
        if (!n.visible || suppressNeedles) {
            continue;
        }
        paintNeedle(p, n, bg);
    }

    // Thetis property-editor parity — Meter Title painted centred
    // near the top of the face. Falls back silently when the title
    // text is empty.
    if (m_showMeterTitle && !m_meterTitleText.isEmpty()) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        QFont f = p.font();
        const int titlePx = qMax(10, static_cast<int>(bg.height() * 0.06));
        f.setPixelSize(titlePx);
        f.setBold(true);
        p.setFont(f);
        p.setPen(m_darkMode ? QColor(220, 220, 220) : m_meterTitleColor);
        const QRect titleRect(bg.x(),
                              bg.y() + static_cast<int>(bg.height() * 0.02),
                              bg.width(),
                              titlePx * 2);
        p.drawText(titleRect, Qt::AlignHCenter | Qt::AlignTop, m_meterTitleText);
        p.restore();
    }

    // Thetis property-editor parity — Peak Value numeric readout.
    // For ANAN MM we draw each needle's current value near the left
    // edge of the face, stacked vertically, so the user can read the
    // live numbers without relying on the needle position.
    if (m_showPeakValue) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        QFont f = p.font();
        const int valPx = qMax(8, static_cast<int>(bg.height() * 0.04));
        f.setPixelSize(valPx);
        p.setFont(f);
        p.setPen(m_darkMode ? QColor(220, 220, 120) : m_peakValueColor);
        const int x = bg.x() + 4;
        int y = bg.y() + bg.height() - (valPx + 2) * kNeedleCount - 4;
        for (const Needle& n : m_needles) {
            if (!n.visible) { y += valPx + 2; continue; }
            const double v = std::isnan(n.currentValue) ? 0.0 : n.currentValue;
            p.drawText(x, y + valPx,
                       QStringLiteral("%1: %2").arg(n.name).arg(v, 0, 'f', 1));
            y += valPx + 2;
        }
        p.restore();
    }

    // Thetis property-editor parity — Peak Hold marker. Draws a
    // small filled triangle at the peak-hold position on each
    // visible needle's arc.
    if (m_showPeakHold) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(m_peakHoldColor);
        const qreal mR = qMax<qreal>(3.0, bg.height() * 0.012);
        for (int i = 0; i < kNeedleCount; ++i) {
            const Needle& n = m_needles[i];
            if (!n.visible) { continue; }
            if (n.calibration.isEmpty()) { continue; }
            double peak = m_peakHolds[i];
            if (std::isinf(peak) || std::isnan(peak)) {
                auto first = n.calibration.constBegin();
                auto last  = std::prev(n.calibration.constEnd());
                peak = 0.5 * (first.key() + last.key());
            }
            const QPointF norm = calibratedPosition(n, static_cast<float>(peak));
            const QPointF px(bg.x() + norm.x() * bg.width(),
                             bg.y() + norm.y() * bg.height());
            p.drawEllipse(px, mR, mR);
        }
        p.restore();
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
    for (int i = 0; i < kNeedleCount; ++i) {
        Needle& n = m_needles[i];
        if (n.bindingId == bindingId) {
            n.currentValue = v;
            // Thetis property-editor parity — rolling peak-hold for the
            // Peak Hold marker. Tracks the maximum value per needle; a
            // future TODO(render-mode-peak-hold-decay) refinement may
            // apply an Ignore-ms-driven decay curve.
            if (std::isnan(m_peakHolds[i]) || v > m_peakHolds[i]) {
                m_peakHolds[i] = v;
            }
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

// ---------------------------------------------------------------------------
// Serialization helpers — keep the colour hex format consistent with
// ColorSwatchButton so the property editor round-trip matches the
// on-disk form.
// ---------------------------------------------------------------------------
namespace {
QString colorToHex(const QColor& c)
{
    return QStringLiteral("#%1%2%3%4")
        .arg(c.red(),   2, 16, QLatin1Char('0'))
        .arg(c.green(), 2, 16, QLatin1Char('0'))
        .arg(c.blue(),  2, 16, QLatin1Char('0'))
        .arg(c.alpha(), 2, 16, QLatin1Char('0'));
}
QColor colorFromHex(const QString& hex, const QColor& fallback)
{
    if (hex.size() < 7 || !hex.startsWith(QLatin1Char('#'))) {
        return fallback;
    }
    bool ok = false;
    const int r = hex.mid(1, 2).toInt(&ok, 16); if (!ok) return fallback;
    const int g = hex.mid(3, 2).toInt(&ok, 16); if (!ok) return fallback;
    const int b = hex.mid(5, 2).toInt(&ok, 16); if (!ok) return fallback;
    int a = 255;
    if (hex.size() >= 9) { a = hex.mid(7, 2).toInt(&ok, 16); if (!ok) a = 255; }
    return QColor(r, g, b, a);
}
} // namespace

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
    QJsonArray cols;
    for (const Needle& n : m_needles) {
        vis.append(n.visible);
        cols.append(colorToHex(n.color));
    }
    o.insert(QStringLiteral("needlesVisible"), vis);
    o.insert(QStringLiteral("needleColors"),   cols);

    // Property-editor parity knobs. Every field added to the class is
    // round-tripped here so the serialization schema stays stable from
    // the first commit of the parity series onward.
    o.insert(QStringLiteral("signalSource"), static_cast<int>(m_signalSource));
    o.insert(QStringLiteral("updateMs"),    m_updateMs);
    o.insert(QStringLiteral("attack"),      static_cast<double>(m_attackRatio));
    o.insert(QStringLiteral("decay"),       static_cast<double>(m_decayRatio));

    o.insert(QStringLiteral("bgColor"),      colorToHex(m_backgroundColor));
    o.insert(QStringLiteral("lowColor"),     colorToHex(m_lowColor));
    o.insert(QStringLiteral("highColor"),    colorToHex(m_highColor));
    o.insert(QStringLiteral("indicatorColor"), colorToHex(m_indicatorColor));
    o.insert(QStringLiteral("subColor"),     colorToHex(m_subColor));

    o.insert(QStringLiteral("fadeRx"),   m_fadeOnRx);
    o.insert(QStringLiteral("fadeTx"),   m_fadeOnTx);
    o.insert(QStringLiteral("darkMode"), m_darkMode);

    o.insert(QStringLiteral("showTitle"),  m_showMeterTitle);
    o.insert(QStringLiteral("titleColor"), colorToHex(m_meterTitleColor));
    o.insert(QStringLiteral("titleText"),  m_meterTitleText);

    o.insert(QStringLiteral("showPeakValue"),  m_showPeakValue);
    o.insert(QStringLiteral("peakValueColor"), colorToHex(m_peakValueColor));

    o.insert(QStringLiteral("showPeakHold"),  m_showPeakHold);
    o.insert(QStringLiteral("peakHoldColor"), colorToHex(m_peakHoldColor));

    o.insert(QStringLiteral("showHistory"),   m_showHistory);
    o.insert(QStringLiteral("historyMs"),     m_historyMs);
    o.insert(QStringLiteral("ignoreHistMs"),  m_ignoreHistoryMs);
    o.insert(QStringLiteral("historyColor"),  colorToHex(m_historyColor));

    o.insert(QStringLiteral("showShadow"),  m_showShadow);
    o.insert(QStringLiteral("shadowLow"),   m_shadowLow);
    o.insert(QStringLiteral("shadowHigh"),  m_shadowHigh);
    o.insert(QStringLiteral("shadowColor"), colorToHex(m_shadowColor));

    o.insert(QStringLiteral("showSegmented"),  m_showSegmented);
    o.insert(QStringLiteral("segmentedLow"),   m_segmentedLow);
    o.insert(QStringLiteral("segmentedHigh"),  m_segmentedHigh);
    o.insert(QStringLiteral("segmentedColor"), colorToHex(m_segmentedColor));

    o.insert(QStringLiteral("showSolid"),  m_showSolid);
    o.insert(QStringLiteral("solidColor"), colorToHex(m_solidColor));

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
    const QJsonArray cols = o.value(QStringLiteral("needleColors")).toArray();
    for (int i = 0; i < kNeedleCount && i < cols.size(); ++i) {
        m_needles[i].color = colorFromHex(cols.at(i).toString(),
                                          m_needles[i].color);
    }

    m_signalSource = static_cast<SignalSource>(
        o.value(QStringLiteral("signalSource")).toInt(static_cast<int>(m_signalSource)));
    // Apply via setSignalSource() so the needle binding stays in sync.
    setSignalSource(m_signalSource);
    m_updateMs    = o.value(QStringLiteral("updateMs")).toInt(m_updateMs);
    m_attackRatio = static_cast<float>(o.value(QStringLiteral("attack")).toDouble(m_attackRatio));
    m_decayRatio  = static_cast<float>(o.value(QStringLiteral("decay")).toDouble(m_decayRatio));

    m_backgroundColor = colorFromHex(o.value(QStringLiteral("bgColor")).toString(),       m_backgroundColor);
    m_lowColor        = colorFromHex(o.value(QStringLiteral("lowColor")).toString(),      m_lowColor);
    m_highColor       = colorFromHex(o.value(QStringLiteral("highColor")).toString(),     m_highColor);
    m_indicatorColor  = colorFromHex(o.value(QStringLiteral("indicatorColor")).toString(),m_indicatorColor);
    m_subColor        = colorFromHex(o.value(QStringLiteral("subColor")).toString(),      m_subColor);

    m_fadeOnRx = o.value(QStringLiteral("fadeRx")).toBool(m_fadeOnRx);
    m_fadeOnTx = o.value(QStringLiteral("fadeTx")).toBool(m_fadeOnTx);
    m_darkMode = o.value(QStringLiteral("darkMode")).toBool(m_darkMode);

    m_showMeterTitle  = o.value(QStringLiteral("showTitle")).toBool(m_showMeterTitle);
    m_meterTitleColor = colorFromHex(o.value(QStringLiteral("titleColor")).toString(), m_meterTitleColor);
    m_meterTitleText  = o.value(QStringLiteral("titleText")).toString(m_meterTitleText);

    m_showPeakValue  = o.value(QStringLiteral("showPeakValue")).toBool(m_showPeakValue);
    m_peakValueColor = colorFromHex(o.value(QStringLiteral("peakValueColor")).toString(), m_peakValueColor);

    m_showPeakHold  = o.value(QStringLiteral("showPeakHold")).toBool(m_showPeakHold);
    m_peakHoldColor = colorFromHex(o.value(QStringLiteral("peakHoldColor")).toString(), m_peakHoldColor);

    m_showHistory      = o.value(QStringLiteral("showHistory")).toBool(m_showHistory);
    m_historyMs        = o.value(QStringLiteral("historyMs")).toInt(m_historyMs);
    m_ignoreHistoryMs  = o.value(QStringLiteral("ignoreHistMs")).toInt(m_ignoreHistoryMs);
    m_historyColor     = colorFromHex(o.value(QStringLiteral("historyColor")).toString(), m_historyColor);

    m_showShadow  = o.value(QStringLiteral("showShadow")).toBool(m_showShadow);
    m_shadowLow   = o.value(QStringLiteral("shadowLow")).toDouble(m_shadowLow);
    m_shadowHigh  = o.value(QStringLiteral("shadowHigh")).toDouble(m_shadowHigh);
    m_shadowColor = colorFromHex(o.value(QStringLiteral("shadowColor")).toString(), m_shadowColor);

    m_showSegmented  = o.value(QStringLiteral("showSegmented")).toBool(m_showSegmented);
    m_segmentedLow   = o.value(QStringLiteral("segmentedLow")).toDouble(m_segmentedLow);
    m_segmentedHigh  = o.value(QStringLiteral("segmentedHigh")).toDouble(m_segmentedHigh);
    m_segmentedColor = colorFromHex(o.value(QStringLiteral("segmentedColor")).toString(), m_segmentedColor);

    m_showSolid  = o.value(QStringLiteral("showSolid")).toBool(m_showSolid);
    m_solidColor = colorFromHex(o.value(QStringLiteral("solidColor")).toString(), m_solidColor);

    return true;
}

} // namespace NereusSDR
