// =================================================================
// src/gui/meters/MeterItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
// =================================================================

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

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include "MeterItem.h"
#include "MeterPoller.h"  // MeterBinding namespace

#include <QPainter>
#include <QMouseEvent>
#include <QFont>
#include <QFontMetrics>
#include <QStringList>
#include <cmath>
#include <algorithm>
#include <QtMath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// readingName — ported verbatim from Thetis MeterManager.cs:2258-2318
// ReadingName() switch. Used by ScaleItem::ShowType to label bar-row
// presets with their canonical Thetis names (e.g. "Signal Max FFT Bin").
// ---------------------------------------------------------------------------
QString readingName(int bindingId)
{
    switch (bindingId) {
        // RX
        case MeterBinding::SignalPeak:   return QStringLiteral("Signal Peak");
        case MeterBinding::SignalAvg:    return QStringLiteral("Signal Average");
        case MeterBinding::AdcPeak:      return QStringLiteral("ADC Peak");
        case MeterBinding::AdcAvg:       return QStringLiteral("ADC Average");
        case MeterBinding::AgcGain:      return QStringLiteral("AGC Gain");
        case MeterBinding::AgcPeak:      return QStringLiteral("AGC Peak");
        case MeterBinding::AgcAvg:       return QStringLiteral("AGC Average");
        case MeterBinding::SignalMaxBin: return QStringLiteral("Signal Max FFT Bin");
        case MeterBinding::PbSnr:        return QStringLiteral("Estimated PBSNR");
        // TX
        case MeterBinding::TxPower:        return QStringLiteral("Power");
        case MeterBinding::TxReversePower: return QStringLiteral("Reverse Power");
        case MeterBinding::TxSwr:          return QStringLiteral("SWR");
        case MeterBinding::TxMic:          return QStringLiteral("MIC");
        case MeterBinding::TxComp:         return QStringLiteral("Compression");
        case MeterBinding::TxAlc:          return QStringLiteral("ALC");
        case MeterBinding::TxEq:           return QStringLiteral("EQ");
        case MeterBinding::TxLeveler:      return QStringLiteral("Leveler");
        case MeterBinding::TxLevelerGain:  return QStringLiteral("Leveler Gain");
        // Thetis has no ReadingName for ALC_GAIN specifically; ALC_G is
        // "ALC Compression". NereusSDR splits these two signals into
        // separate bindings so we use the natural label for ALC Gain.
        case MeterBinding::TxAlcGain:      return QStringLiteral("ALC Gain");
        case MeterBinding::TxAlcGroup:     return QStringLiteral("ALC Group");
        case MeterBinding::TxCfc:          return QStringLiteral("CFC Compression Average");
        case MeterBinding::TxCfcGain:      return QStringLiteral("CFC Compression");
        // Hardware
        case MeterBinding::HwVolts:       return QStringLiteral("Volts");
        case MeterBinding::HwAmps:        return QStringLiteral("Amps");
        case MeterBinding::HwTemperature: return QStringLiteral("Temperature");
        // Rotator
        case MeterBinding::RotatorAz:  return QStringLiteral("Azimuth");
        case MeterBinding::RotatorEle: return QStringLiteral("Elevation");
    }
    return QString();
}


// ---------------------------------------------------------------------------
// MeterItem base — serialize / deserialize
// Format: x|y|w|h|bindingId|zOrder
// ---------------------------------------------------------------------------

QString MeterItem::serialize() const
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(m_x))
        .arg(static_cast<double>(m_y))
        .arg(static_cast<double>(m_w))
        .arg(static_cast<double>(m_h))
        .arg(m_bindingId)
        .arg(m_zOrder);
}

bool MeterItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 6) {
        return false;
    }

    bool ok = true;
    const float x  = parts[0].toFloat(&ok);  if (!ok) { return false; }
    const float y  = parts[1].toFloat(&ok);  if (!ok) { return false; }
    const float w  = parts[2].toFloat(&ok);  if (!ok) { return false; }
    const float h  = parts[3].toFloat(&ok);  if (!ok) { return false; }
    const int bid  = parts[4].toInt(&ok);    if (!ok) { return false; }
    const int zo   = parts[5].toInt(&ok);    if (!ok) { return false; }

    setRect(x, y, w, h);
    setBindingId(bid);
    setZOrder(zo);
    return true;
}

// ---------------------------------------------------------------------------
// MeterItem base — stacked-row layout (Phase 3G-9 post-revert)
// ---------------------------------------------------------------------------
//
// Thetis-parity stack layout. Each row's effective normalized y/h
// is computed from:
//
//   slotHNorm = slotHeightPx / widgetHeightPx
//   slotYNorm = bandTop + stackSlot * slotHNorm
//   m_y       = slotYNorm + m_slotLocalY * slotHNorm
//   m_h       = m_slotLocalH * slotHNorm
//
// The caller is responsible for choosing slotHeightPx — in the
// normal NereusSDR path that's
// `max(kNormalRowHNorm * widgetHeightPx, kMinRowPx)` (Thetis's
// `_fHeight = 0.05f` with a pixel floor so rows stay readable when
// the container is unusually small). bandTop is derived per-reflow
// from the composite sitting above the stack — typically 0 when no
// composite is present, or the composite's max y+h otherwise.
// Rows with slotYNorm + m_h > 1.0 overflow the widget bottom and
// clip naturally at paint time, matching Thetis Default Multimeter
// container behaviour when more rows are added than fit.

void MeterItem::layoutInStackSlot(int widgetHeightPx, int slotHeightPx,
                                   float bandTop)
{
    if (m_stackSlot < 0 || widgetHeightPx <= 0 || slotHeightPx <= 0) {
        return;
    }
    const float slotHNorm = static_cast<float>(slotHeightPx)
                          / static_cast<float>(widgetHeightPx);
    const float slotYNorm = bandTop
                          + static_cast<float>(m_stackSlot) * slotHNorm;
    m_y = slotYNorm + m_slotLocalY * slotHNorm;
    m_h = m_slotLocalH * slotHNorm;
}

// ---------------------------------------------------------------------------
// MeterItem base — mouse interaction (Phase 3G-5)
// ---------------------------------------------------------------------------

bool MeterItem::hitTest(const QPointF& pos, int widgetW, int widgetH) const
{
    return pixelRect(widgetW, widgetH).contains(pos.toPoint());
}

bool MeterItem::handleMousePress(QMouseEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(event); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    return false;
}

bool MeterItem::handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(event); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    return false;
}

bool MeterItem::handleMouseMove(QMouseEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(event); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    return false;
}

bool MeterItem::handleWheel(QWheelEvent* event, int widgetW, int widgetH)
{
    Q_UNUSED(event); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    return false;
}

// ---------------------------------------------------------------------------
// Helper: build the base fields string (indices 1-6) for concrete types.
// Concrete types prepend their prefix and append type-specific fields.
// ---------------------------------------------------------------------------
static QString baseFields(const MeterItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(item.x()))
        .arg(static_cast<double>(item.y()))
        .arg(static_cast<double>(item.itemWidth()))
        .arg(static_cast<double>(item.itemHeight()))
        .arg(item.bindingId())
        .arg(item.zOrder());
}

// Helper: parse base fields from indices 1..6 of parts (index 0 is the prefix).
static bool parseBaseFields(MeterItem& item, const QStringList& parts)
{
    if (parts.size() < 7) {
        return false;
    }
    // Re-join indices 1..6 and delegate to the base deserialize
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    return item.MeterItem::deserialize(base);
}

// ---------------------------------------------------------------------------
// SolidColourItem
// Format: SOLID|x|y|w|h|bindingId|zOrder|#aarrggbb
// ---------------------------------------------------------------------------

void SolidColourItem::paint(QPainter& p, int widgetW, int widgetH)
{
    p.fillRect(pixelRect(widgetW, widgetH), m_colour);
}

QString SolidColourItem::serialize() const
{
    return QStringLiteral("SOLID|%1|%2")
        .arg(baseFields(*this))
        .arg(m_colour.name(QColor::HexArgb));
}

bool SolidColourItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 8 || parts[0] != QLatin1String("SOLID")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }
    m_colour = QColor(parts[7]);
    return m_colour.isValid();
}

// ---------------------------------------------------------------------------
// ImageItem
// Format: IMAGE|x|y|w|h|bindingId|zOrder|path
// ---------------------------------------------------------------------------

void ImageItem::setImagePath(const QString& path)
{
    m_imagePath = path;
    m_image.load(path);
}

void ImageItem::paint(QPainter& p, int widgetW, int widgetH)
{
    if (m_image.isNull()) {
        return;
    }
    const QRect rect = pixelRect(widgetW, widgetH);
    p.drawImage(rect, m_image);
}

QString ImageItem::serialize() const
{
    return QStringLiteral("IMAGE|%1|%2")
        .arg(baseFields(*this))
        .arg(m_imagePath);
}

bool ImageItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 8 || parts[0] != QLatin1String("IMAGE")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }
    setImagePath(parts[7]);
    return true;
}

// ---------------------------------------------------------------------------
// BarItem
// Format (fields 0-14 legacy, 15-19 Edge, 20-22 Phase A1, 23-25 A2,
// 26-29 A3, 30 A4, 31 E2):
//   BAR|x|y|w|h|bindingId|zOrder|orientation|minVal|maxVal|
//       barColor|barRedColor|redThreshold|attackRatio|decayRatio|
//       barStyle|edgeBgColor|edgeLowColor|edgeHighColor|edgeAvgColor|
//       showValue|showPeakValue|fontColour|
//       showHistory|historyColour|historyDurationMs|
//       showMarker|markerColour|peakHoldMarkerColour|peakHoldDecayRatio|
//       scaleCalibration|peakFontColour
// ---------------------------------------------------------------------------

// Override setValue() for exponential smoothing.
// From Thetis MeterManager.cs — attack when rising, decay when falling.
// ---------------------------------------------------------------------------
// BarItem — Phase A4 ScaleCalibration (non-linear value → X map).
// From Thetis clsBarItem (MeterManager.cs:20192 field, 21547-21549 addSMeterBar
// 3-point S-meter calibration, 21638-21640 AddADCMaxMag 3-point ADC curve).
// ---------------------------------------------------------------------------
void BarItem::addScaleCalibration(double value, float normalizedX)
{
    m_scaleCalibration.insert(value, normalizedX);
}

float BarItem::valueToNormalizedX(double value) const
{
    // Empty calibration: fall back to linear minVal..maxVal mapping. This
    // preserves pre-A4 behavior for every Filled/Edge preset.
    if (m_scaleCalibration.isEmpty()) {
        const double range = m_maxVal - m_minVal;
        if (range <= 0.0) { return 0.0f; }
        const double frac = std::clamp((value - m_minVal) / range, 0.0, 1.0);
        return static_cast<float>(frac);
    }

    // Clamp below the first waypoint.
    auto itFirst = m_scaleCalibration.constBegin();
    if (value <= itFirst.key()) {
        return itFirst.value();
    }
    // Clamp above the last waypoint.
    auto itLast = std::prev(m_scaleCalibration.constEnd());
    if (value >= itLast.key()) {
        return itLast.value();
    }

    // Piecewise-linear interpolation between adjacent waypoints.
    auto hi = m_scaleCalibration.upperBound(value);
    auto lo = std::prev(hi);
    const double k0 = lo.key();
    const double k1 = hi.key();
    const float  x0 = lo.value();
    const float  x1 = hi.value();
    const double span = k1 - k0;
    if (span <= 0.0) { return x0; }
    const double t = (value - k0) / span;
    return static_cast<float>(x0 + (x1 - x0) * t);
}

void BarItem::setValue(double v)
{
    MeterItem::setValue(v);
    if (v > m_smoothedValue) {
        m_smoothedValue = static_cast<double>(m_attackRatio) * v
                        + (1.0 - static_cast<double>(m_attackRatio)) * m_smoothedValue;
    } else {
        m_smoothedValue = static_cast<double>(m_decayRatio) * v
                        + (1.0 - static_cast<double>(m_decayRatio)) * m_smoothedValue;
    }
    // Phase A1/A3 — peak-hold tracking. When PeakHoldDecayRatio == 0 the
    // peak sticks at the highest value ever seen (A1 behavior). When > 0,
    // the peak decays geometrically toward the live value on every frame
    // where v < peak, matching Thetis clsBarItem peak-hold marker semantics
    // (MeterManager.cs:21541, 21544).
    if (v > m_peakValue || !std::isfinite(m_peakValue)) {
        m_peakValue = v;
    } else if (m_peakHoldDecayRatio > 0.0f) {
        const double r = static_cast<double>(m_peakHoldDecayRatio);
        m_peakValue = m_peakValue + (v - m_peakValue) * r;
    }

    // Phase A2 — history ring buffer. Thetis clsBarItem pushes samples at
    // UpdateInterval and keeps (HistoryDuration / UpdateInterval) entries.
    // Without a poll-clock reference at setValue time we bound the buffer
    // by a conservative upper limit (HistoryDurationMs at 100 Hz = 400).
    if (m_showHistory) {
        m_history.append(v);
        const int maxSamples = qMax(2, m_historyDurationMs / 10);
        while (m_history.size() > maxSamples) {
            m_history.removeFirst();
        }
    }
}

void BarItem::paint(QPainter& p, int widgetW, int widgetH)
{
    // Dispatch to Edge renderer if configured
    // From Thetis console.cs:12612-12678
    if (m_barStyle == BarStyle::Edge) {
        paintEdge(p, widgetW, widgetH);
        return;
    }

    const QRect rect = pixelRect(widgetW, widgetH);
    const QColor& fillColor = (m_smoothedValue >= m_redThreshold)
                                  ? m_barRedColor : m_barColor;

    // Bar body. Uses valueToNormalizedX() so both the linear min..max
    // fallback (empty calibration) and the Phase A4 piecewise-linear
    // calibration route through one code path. Only Horizontal
    // orientation supports the Phase A extensions; Vertical keeps its
    // pre-A1 behavior because no Thetis preset uses it.
    if (m_orientation == Orientation::Vertical) {
        const double range = m_maxVal - m_minVal;
        const double frac  = (range > 0.0)
            ? std::clamp((m_smoothedValue - m_minVal) / range, 0.0, 1.0)
            : 0.0;
        const int fillH = static_cast<int>(frac * rect.height());
        p.fillRect(QRect(rect.left(), rect.bottom() - fillH,
                         rect.width(), fillH),
                   fillColor);
        return;
    }

    const float fracLive = valueToNormalizedX(m_smoothedValue);
    const int fillW = static_cast<int>(fracLive * rect.width());

    // Phase A2 — history trailing fill. Drawn FIRST so the live bar
    // overlays it. Each sample produces one vertical line at its own
    // calibrated X, with full-height alpha-blended stroke using
    // m_historyColour. Older samples fade via the alpha in the colour
    // itself; a stronger trail effect will come with VBO work later.
    if (m_showHistory && !m_history.isEmpty()) {
        p.setPen(QPen(m_historyColour, 1));
        for (double v : m_history) {
            const float hx = valueToNormalizedX(v);
            const int px = rect.left() + static_cast<int>(hx * rect.width());
            p.drawLine(px, rect.top(), px, rect.bottom());
        }
    }

    // Phase A4 — Line style draws only a thin baseline at the bottom
    // of the bar rect instead of a filled quad. Matches Thetis
    // BarStyle.Line where the "bar" is really a baseline + markers +
    // labels composition.
    if (m_barStyle == BarStyle::Line) {
        p.setPen(QPen(fillColor, 2));
        const int baselineY = rect.bottom() - 2;
        p.drawLine(rect.left(), baselineY,
                   rect.left() + fillW, baselineY);
    } else {
        // Default Filled bar (existing pre-A behavior, now routed
        // through valueToNormalizedX so a calibrated Filled bar also
        // works if anyone wants it).
        p.fillRect(QRect(rect.left(), rect.top(),
                         fillW, rect.height()),
                   fillColor);
    }

    // Phase A3 — live value marker + peak-hold marker (Thetis
    // MeterManager.cs:21543-21544). Thin vertical lines at the live
    // smoothed X and the separate peak X.
    if (m_showMarker) {
        p.setPen(QPen(m_markerColour, 2));
        const int liveX = rect.left() + static_cast<int>(fracLive * rect.width());
        p.drawLine(liveX, rect.top(), liveX, rect.bottom());

        if (std::isfinite(m_peakValue)) {
            p.setPen(QPen(m_peakHoldMarkerColour, 2));
            const float fracPeak = valueToNormalizedX(m_peakValue);
            const int peakX = rect.left() + static_cast<int>(fracPeak * rect.width());
            p.drawLine(peakX, rect.top(), peakX, rect.bottom());
        }
    }

    // Phase A1 / E — ShowValue / ShowPeakValue text labels. ShowValue
    // is drawn top-left in m_fontColour (Thetis default white), and
    // ShowPeakValue top-right in peakFontColour() (Thetis default red
    // — per Setup → Appearance → Meters/Gadgets Peak Value swatch).
    // Both use a small proportional bold font. Idle ShowValue is
    // skipped; idle ShowPeakValue renders "--" as a placeholder so
    // parked bindings (TX-only readings while RX active, etc.) still
    // show a slot where the peak text lives.
    if (m_showValue || m_showPeakValue) {
        QFont font = p.font();
        const int fontPx = qMax(9, rect.height() / 3);
        font.setPixelSize(fontPx);
        font.setBold(true);
        p.setFont(font);
        const QFontMetrics fm(font);

        if (m_showValue && std::isfinite(m_smoothedValue)) {
            p.setPen(m_fontColour);
            const QString s = QString::number(m_smoothedValue, 'f', 1);
            p.drawText(rect.left() + 2,
                       rect.top() + fm.ascent(),
                       s);
        }
        if (m_showPeakValue) {
            const QString s = std::isfinite(m_peakValue)
                ? QString::number(m_peakValue, 'f', 1)
                : QStringLiteral("--");
            p.setPen(peakFontColour());
            const int w = fm.horizontalAdvance(s);
            p.drawText(rect.right() - w - 2,
                       rect.top() + fm.ascent(),
                       s);
        }
    }
}

void BarItem::emitVertices(QVector<float>& verts, int widgetW, int widgetH)
{
    Q_UNUSED(widgetW); Q_UNUSED(widgetH);

    const double range = m_maxVal - m_minVal;
    const double frac  = (range > 0.0)
        ? std::clamp((m_smoothedValue - m_minVal) / range, 0.0, 1.0)
        : 0.0;

    // Convert normalized item rect to NDC
    const float ndcL = m_x * 2.0f - 1.0f;
    const float ndcR = (m_x + m_w) * 2.0f - 1.0f;
    const float ndcT = 1.0f - m_y * 2.0f;
    const float ndcB = 1.0f - (m_y + m_h) * 2.0f;

    const QColor& fillColor = (m_smoothedValue >= m_redThreshold) ? m_barRedColor : m_barColor;
    const float r = static_cast<float>(fillColor.redF());
    const float g = static_cast<float>(fillColor.greenF());
    const float b = static_cast<float>(fillColor.blueF());
    const float a = static_cast<float>(fillColor.alphaF());

    // Compute filled quad corners
    float qL, qR, qT, qB;
    if (m_orientation == Orientation::Horizontal) {
        qL = ndcL;
        qR = ndcL + static_cast<float>(frac) * (ndcR - ndcL);
        qT = ndcT;
        qB = ndcB;
    } else {
        // Vertical: fill from bottom
        qL = ndcL;
        qR = ndcR;
        qB = ndcB;
        qT = ndcB + static_cast<float>(frac) * (ndcT - ndcB);
    }

    // Emit 6 vertices (2 triangles) for Triangles topology.
    // This avoids degenerate strip artifacts when multiple bars share the VBO.
    // Triangle 1: TL, BL, TR
    verts << qL << qT << r << g << b << a;
    verts << qL << qB << r << g << b << a;
    verts << qR << qT << r << g << b << a;
    // Triangle 2: TR, BL, BR
    verts << qR << qT << r << g << b << a;
    verts << qL << qB << r << g << b << a;
    verts << qR << qB << r << g << b << a;
}

// ---------------------------------------------------------------------------
// BarItem::paintEdge()
// Edge rendering mode: draws a single vertical indicator line instead of
// a filled bar. From Thetis console.cs:23589-23624
// ---------------------------------------------------------------------------
void BarItem::paintEdge(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);

    // Background outline (from Thetis console.cs:23589)
    p.setPen(QPen(m_edgeBgColor, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect);

    // Calculate meter position
    const double range = m_maxVal - m_minVal;
    const double frac = (range > 0.0)
        ? std::clamp((m_smoothedValue - m_minVal) / range, 0.0, 1.0)
        : 0.0;
    const int pixelX = rect.left() + static_cast<int>(frac * rect.width());

    // Shadow color: midpoint blend of indicator and background
    // From Thetis console.cs:23605-23608
    const QColor shadowColor = QColor(
        (m_edgeAvgColor.red()   + m_edgeBgColor.red())   / 2,
        (m_edgeAvgColor.green() + m_edgeBgColor.green()) / 2,
        (m_edgeAvgColor.blue()  + m_edgeBgColor.blue())  / 2
    );

    // Three vertical lines: shadow-center-shadow
    // From Thetis console.cs:23622-23624
    p.setPen(QPen(shadowColor, 1));
    p.drawLine(pixelX - 1, rect.top() + 1, pixelX - 1, rect.bottom() - 1);
    p.drawLine(pixelX + 1, rect.top() + 1, pixelX + 1, rect.bottom() - 1);

    p.setPen(QPen(m_edgeAvgColor, 1));
    p.drawLine(pixelX, rect.top() + 1, pixelX, rect.bottom() - 1);
}

QString BarItem::serialize() const
{
    // Fields 0-14:  BAR|x|y|w|h|bindingId|zOrder|orientation|min|max|
    //               barColor|barRedColor|redThreshold|attack|decay
    // Fields 15-19: barStyle|edgeBgColor|edgeLowColor|edgeHighColor|edgeAvgColor
    // Fields 20-22: showValue|showPeakValue|fontColour           (Phase A1)
    QString base = QStringLiteral("BAR|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14")
        .arg(baseFields(*this))
        .arg(m_orientation == Orientation::Horizontal ? QStringLiteral("H") : QStringLiteral("V"))
        .arg(m_minVal)
        .arg(m_maxVal)
        .arg(m_barColor.name(QColor::HexArgb))
        .arg(m_barRedColor.name(QColor::HexArgb))
        .arg(m_redThreshold)
        .arg(static_cast<double>(m_attackRatio))
        .arg(static_cast<double>(m_decayRatio))
        .arg(m_barStyle == BarStyle::Edge ? QStringLiteral("Edge")
            : (m_barStyle == BarStyle::Line ? QStringLiteral("Line")
                                            : QStringLiteral("Filled")))
        .arg(m_edgeBgColor.name(QColor::HexArgb))
        .arg(m_edgeLowColor.name(QColor::HexArgb))
        .arg(m_edgeHighColor.name(QColor::HexArgb))
        .arg(m_edgeAvgColor.name(QColor::HexArgb));
    // Append-only tail (Phase A1). Legacy readers stop at field 19 and
    // these get the defaults — see tail-tolerant parse below.
    base += QLatin1Char('|') + (m_showValue     ? QStringLiteral("1") : QStringLiteral("0"));
    base += QLatin1Char('|') + (m_showPeakValue ? QStringLiteral("1") : QStringLiteral("0"));
    base += QLatin1Char('|') + m_fontColour.name(QColor::HexArgb);
    // Phase A2 tail: showHistory | historyColour | historyDurationMs
    base += QLatin1Char('|') + (m_showHistory ? QStringLiteral("1") : QStringLiteral("0"));
    base += QLatin1Char('|') + m_historyColour.name(QColor::HexArgb);
    base += QLatin1Char('|') + QString::number(m_historyDurationMs);
    // Phase A3 tail: showMarker | markerColour | peakHoldMarkerColour | peakHoldDecayRatio
    base += QLatin1Char('|') + (m_showMarker ? QStringLiteral("1") : QStringLiteral("0"));
    base += QLatin1Char('|') + m_markerColour.name(QColor::HexArgb);
    base += QLatin1Char('|') + m_peakHoldMarkerColour.name(QColor::HexArgb);
    base += QLatin1Char('|') + QString::number(static_cast<double>(m_peakHoldDecayRatio));
    // Phase A4 tail: scaleCalibration (semicolon-separated value=x pairs).
    // Empty-calibration BarItems emit an empty string, keeping the parser
    // identical for pre-A4 saves. The barStyle field (index 15) is already
    // serialized above — the Line value just joins Filled/Edge in the
    // existing string slot, so no new slot is needed for the enum itself.
    QStringList calParts;
    for (auto it = m_scaleCalibration.constBegin();
         it != m_scaleCalibration.constEnd(); ++it) {
        calParts << QStringLiteral("%1=%2")
                        .arg(it.key())
                        .arg(static_cast<double>(it.value()));
    }
    base += QLatin1Char('|') + calParts.join(QLatin1Char(';'));
    // Phase E2 tail: peakFontColour (field 31). Invalid QColor means
    // "fall back to m_fontColour" at render time — emit an empty slot
    // to preserve that sentinel across round-trip. Legacy readers stop
    // at field 30 and keep their default invalid m_peakFontColour.
    base += QLatin1Char('|');
    if (m_peakFontColour.isValid()) {
        base += m_peakFontColour.name(QColor::HexArgb);
    }
    return base;
}

bool BarItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    // Legacy format: 15 fields (indices 0-14), new format: 20 fields (adds Edge style fields)
    if (parts.size() < 15 || parts[0] != QLatin1String("BAR")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }

    m_orientation = (parts[7] == QLatin1String("H")) ? Orientation::Horizontal : Orientation::Vertical;

    bool ok = true;
    m_minVal       = parts[8].toDouble(&ok);  if (!ok) { return false; }
    m_maxVal       = parts[9].toDouble(&ok);  if (!ok) { return false; }
    m_barColor     = QColor(parts[10]);       if (!m_barColor.isValid()) { return false; }
    m_barRedColor  = QColor(parts[11]);       if (!m_barRedColor.isValid()) { return false; }
    m_redThreshold = parts[12].toDouble(&ok); if (!ok) { return false; }
    m_attackRatio  = parts[13].toFloat(&ok);  if (!ok) { return false; }
    m_decayRatio   = parts[14].toFloat(&ok);  if (!ok) { return false; }

    // Optional Edge mode fields (new format, 20 fields total)
    // From Thetis console.cs:12612-12678
    if (parts.size() >= 20) {
        // Phase A4: BarStyle is now a 3-way enum (Filled, Edge, Line)
        if (parts[15] == QLatin1String("Edge")) {
            m_barStyle = BarStyle::Edge;
        } else if (parts[15] == QLatin1String("Line")) {
            m_barStyle = BarStyle::Line;
        } else {
            m_barStyle = BarStyle::Filled;
        }
        QColor ec1 = QColor(parts[16]); if (ec1.isValid()) { m_edgeBgColor   = ec1; }
        QColor ec2 = QColor(parts[17]); if (ec2.isValid()) { m_edgeLowColor  = ec2; }
        QColor ec3 = QColor(parts[18]); if (ec3.isValid()) { m_edgeHighColor = ec3; }
        QColor ec4 = QColor(parts[19]); if (ec4.isValid()) { m_edgeAvgColor  = ec4; }
    }

    // Phase A1 — ShowValue / ShowPeakValue / FontColour (append-only tail)
    if (parts.size() >= 21) { m_showValue     = (parts[20] == QLatin1String("1")); }
    if (parts.size() >= 22) { m_showPeakValue = (parts[21] == QLatin1String("1")); }
    if (parts.size() >= 23) {
        QColor fc = QColor(parts[22]);
        if (fc.isValid()) { m_fontColour = fc; }
    }

    // Phase A2 — ShowHistory / HistoryColour / HistoryDurationMs
    if (parts.size() >= 24) { m_showHistory = (parts[23] == QLatin1String("1")); }
    if (parts.size() >= 25) {
        QColor hc = QColor(parts[24]);
        if (hc.isValid()) { m_historyColour = hc; }
    }
    if (parts.size() >= 26) {
        bool okDur = true;
        int dur = parts[25].toInt(&okDur);
        if (okDur) { m_historyDurationMs = dur; }
    }

    // Phase A3 — ShowMarker / MarkerColour / PeakHoldMarkerColour / PeakHoldDecayRatio
    if (parts.size() >= 27) { m_showMarker = (parts[26] == QLatin1String("1")); }
    if (parts.size() >= 28) {
        QColor mc = QColor(parts[27]);
        if (mc.isValid()) { m_markerColour = mc; }
    }
    if (parts.size() >= 29) {
        QColor pc = QColor(parts[28]);
        if (pc.isValid()) { m_peakHoldMarkerColour = pc; }
    }
    if (parts.size() >= 30) {
        bool okR = true;
        float r = parts[29].toFloat(&okR);
        if (okR) { m_peakHoldDecayRatio = r; }
    }

    // Phase A4 — ScaleCalibration (semicolon-separated value=x pairs).
    if (parts.size() >= 31) {
        m_scaleCalibration.clear();
        const QString& calStr = parts[30];
        if (!calStr.isEmpty()) {
            const QStringList calParts =
                calStr.split(QLatin1Char(';'), Qt::SkipEmptyParts);
            for (const QString& p : calParts) {
                const int eq = p.indexOf(QLatin1Char('='));
                if (eq <= 0) { continue; }
                bool okK = true;
                bool okV = true;
                const double k = p.left(eq).toDouble(&okK);
                const float  v = p.mid(eq + 1).toFloat(&okV);
                if (okK && okV) {
                    m_scaleCalibration.insert(k, v);
                }
            }
        }
    }

    // Phase E2 — PeakFontColour (append-only, field 31). Empty slot
    // means "leave invalid" so peakFontColour() falls back to
    // m_fontColour at render time; a populated hex string restores
    // the explicit override the user picked in the editor.
    if (parts.size() >= 32) {
        const QString& pfcStr = parts[31];
        if (!pfcStr.isEmpty()) {
            QColor pfc(pfcStr);
            if (pfc.isValid()) { m_peakFontColour = pfc; }
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// ScaleItem
// Format: SCALE|x|y|w|h|bindingId|zOrder|orientation|minVal|maxVal|
//               majorTicks|minorTicks|tickColor|labelColor|fontSize
// ---------------------------------------------------------------------------

void ScaleItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    const double range = m_maxVal - m_minVal;

    // Phase B2 — ShowType centered title.
    // From Thetis MeterManager.cs:31879-31886. Thetis draws the title
    // above the scale rect in the container background area. NereusSDR
    // draws it inside the top ~25% of the ScaleItem's own rect so the
    // item owns its title space; presets leave room at the top when
    // building the row (Phase D1).
    if (m_showType) {
        const QString title = readingName(m_bindingId);
        if (!title.isEmpty()) {
            const int titleHeight = qMax(8, rect.height() / 3);
            QFont titleFont = p.font();
            titleFont.setPixelSize(titleHeight);
            titleFont.setBold(true);
            p.setFont(titleFont);
            p.setPen(m_titleColour);
            const QRect titleRect(rect.left(),
                                  rect.top(),
                                  rect.width(),
                                  titleHeight);
            p.drawText(titleRect, Qt::AlignCenter, title);
        }
    }

    // Phase B3 — if the scale is in GeneralScale mode, render the
    // two-tone Thetis-style baseline + ticks instead of the NereusSDR
    // evenly-spaced renderer below. Ported from MeterManager.cs:32338-32423
    // generalScale(). Text labels (per-tick numeric values) are drawn
    // using the existing QFontMetrics-based positioning; colours come
    // from m_lowColour / m_highColour / m_labelColor.
    if (m_scaleStyle == ScaleStyle::GeneralScale) {
        const float rx = static_cast<float>(rect.left());
        const float ry = static_cast<float>(rect.top());
        const float rw = static_cast<float>(rect.width());
        const float rh = static_cast<float>(rect.height());

        // lowToHighPoint — the x-fraction at which the two-tone split
        // happens. Falls back to the even (lowLongTicks-1)/(totalTicks-1)
        // distribution when m_centrePerc < 0 (Thetis default).
        float lowToHighPoint =
            static_cast<float>(m_lowLongTicks - 1)
                / static_cast<float>(m_lowLongTicks + m_highLongTicks - 1);
        if (m_centrePerc >= 0.0f) {
            lowToHighPoint = m_centrePerc;
        }

        // Per-segment spacing. `lowSpacing` is the distance between
        // consecutive long ticks in the low half; `highSpacing` is the
        // same for the high half but measured over (w - lowSpan - w*0.01)
        // so the high baseline stops at x + w*0.99 — matches Thetis.
        const float lowSpacing =
            (rw * lowToHighPoint) / static_cast<float>(m_lowLongTicks - 1);
        const float lowSpan = lowSpacing * static_cast<float>(m_lowLongTicks - 1);
        const float highSpacing =
            ((rw - lowSpan) - (rw * 0.01f))
                / static_cast<float>(m_highLongTicks);

        const float fLineBaseY = ry + (rh * 0.85f);

        // Baseline horizontal line, split at end of low segment.
        p.setPen(QPen(m_lowColour, 2));
        p.drawLine(QPointF(rx, fLineBaseY),
                   QPointF(rx + lowSpan, fLineBaseY));
        p.setPen(QPen(m_highColour, 2));
        p.drawLine(QPointF(rx + lowSpan, fLineBaseY),
                   QPointF(rx + rw * 0.99f, fLineBaseY));

        // Pick a font for the per-tick labels.
        const int dynFontPx = qMax(m_fontSize, rect.height() / 4);
        QFont font = p.font();
        font.setPixelSize(dynFontPx);
        p.setFont(font);
        const QFontMetrics fm(font);

        // Short-row guard: when ShowType eats the top third of the rect
        // and the rect is under ~40 px, shrink the tick row by half so
        // the major-tick tops don't collide with the title text.
        // NereusSDR-original polish — Thetis renders its title outside
        // the scale rect so doesn't hit this case.
        const float tickScale =
            (m_showType && rh < 40.0f) ? 0.5f : 1.0f;
        const float majorTickFrac = 0.30f * tickScale;
        const float minorTickFrac = 0.15f * tickScale;

        auto drawTick = [&](float tickX, bool isMajor, const QColor& colour) {
            const float topY = fLineBaseY
                             - (rh * (isMajor ? majorTickFrac : minorTickFrac));
            p.setPen(QPen(colour, 2));
            p.drawLine(QPointF(tickX, fLineBaseY), QPointF(tickX, topY));
        };

        auto drawLabel = [&](float tickX, const QString& text,
                             const QColor& colour, bool rightAligned) {
            const float topY = fLineBaseY - (rh * majorTickFrac);
            const int w = fm.horizontalAdvance(text);
            const int h = fm.height();
            float lx = rightAligned
                ? (rx + rw - w)
                : (tickX - w / 2.0f);
            p.setPen(colour);
            p.drawText(QPointF(lx, topY - (h - fm.ascent())), text);
        };

        // Low segment ticks + labels.
        for (int i = 1; i < m_lowLongTicks; ++i) {
            // Short tick offset half a space to the left of the long one
            // (Thetis line 32373).
            const float shortX = rx + (static_cast<float>(i) * lowSpacing)
                                    - (lowSpacing * 0.5f);
            drawTick(shortX, /*isMajor*/ false, m_lowColour);

            const float longX = rx + (static_cast<float>(i) * lowSpacing);
            drawTick(longX, /*isMajor*/ true, m_lowColour);

            const int val = m_lowStartNumber + i * m_lowIncrement;
            drawLabel(longX, QString::number(val), m_labelColor,
                      /*rightAligned*/ false);
        }

        // High segment ticks + labels.
        for (int i = 1; i < m_highLongTicks + 1; ++i) {
            const float shortX = rx + lowSpan
                               + (static_cast<float>(i) * highSpacing)
                               - (highSpacing * 0.5f);
            drawTick(shortX, /*isMajor*/ false, m_highColour);

            const float longX = rx + lowSpan
                              + (static_cast<float>(i) * highSpacing);
            drawTick(longX, /*isMajor*/ true, m_highColour);

            const int val = (m_highEndNumber - (m_highLongTicks * m_highIncrement))
                          + i * m_highIncrement;
            const bool isLast = (i == m_highLongTicks);
            drawLabel(longX, QString::number(val), m_labelColor,
                      /*rightAligned*/ isLast);
        }

        return;
    }

    if (range <= 0.0 || m_majorTicks < 2) {
        return;
    }

    // Dynamic pixel-based font sizing (matches NeedleItem pattern).
    // Labels occupy bottom 2/3 of rect; scale font to fit, m_fontSize is floor.
    const int pixH = rect.height();
    const int dynFontSize = qMax(m_fontSize, pixH / 3);

    QFont font = p.font();
    font.setPixelSize(dynFontSize);
    p.setFont(font);

    if (m_orientation == Orientation::Horizontal) {
        // Major ticks with labels — ticks go downward from top, labels below
        for (int i = 0; i < m_majorTicks; ++i) {
            const double frac = static_cast<double>(i) / (m_majorTicks - 1);
            const int x = rect.left() + static_cast<int>(frac * rect.width());

            p.setPen(m_tickColor);
            p.drawLine(x, rect.top(), x, rect.top() + rect.height() / 3);

            const double val = m_minVal + frac * range;
            const QString label = QString::number(val, 'f', 0);
            p.setPen(m_labelColor);
            const QFontMetrics fm(font);
            const int labelW = fm.horizontalAdvance(label);
            p.drawText(x - labelW / 2, rect.top() + rect.height() / 3 + fm.ascent(), label);

            // Minor ticks (between major ticks, except after last major)
            if (i < m_majorTicks - 1 && m_minorTicks > 0) {
                const double nextFrac = static_cast<double>(i + 1) / (m_majorTicks - 1);
                for (int j = 1; j <= m_minorTicks; ++j) {
                    const double minorFrac = frac + (nextFrac - frac) * (static_cast<double>(j) / (m_minorTicks + 1));
                    const int mx = rect.left() + static_cast<int>(minorFrac * rect.width());
                    p.setPen(m_tickColor);
                    p.drawLine(mx, rect.top(), mx, rect.top() + rect.height() / 6);
                }
            }
        }
    } else {
        // Vertical — ticks go left from right edge, labels to the left
        for (int i = 0; i < m_majorTicks; ++i) {
            const double frac = static_cast<double>(i) / (m_majorTicks - 1);
            // Top = maxVal, bottom = minVal for vertical orientation
            const int y = rect.top() + static_cast<int>((1.0 - frac) * rect.height());

            p.setPen(m_tickColor);
            p.drawLine(rect.right(), y, rect.right() - rect.width() / 3, y);

            const double val = m_minVal + frac * range;
            const QString label = QString::number(val, 'f', 0);
            p.setPen(m_labelColor);
            const QFontMetrics fm(font);
            const int labelW = fm.horizontalAdvance(label);
            const int labelX = rect.right() - rect.width() / 3 - labelW - 2;
            p.drawText(labelX, y + fm.ascent() / 2, label);

            // Minor ticks
            if (i < m_majorTicks - 1 && m_minorTicks > 0) {
                const double nextFrac = static_cast<double>(i + 1) / (m_majorTicks - 1);
                for (int j = 1; j <= m_minorTicks; ++j) {
                    const double minorFrac = frac + (nextFrac - frac) * (static_cast<double>(j) / (m_minorTicks + 1));
                    const int my = rect.top() + static_cast<int>((1.0 - minorFrac) * rect.height());
                    p.setPen(m_tickColor);
                    p.drawLine(rect.right(), my, rect.right() - rect.width() / 6, my);
                }
            }
        }
    }
}

QString ScaleItem::serialize() const
{
    QString base = QStringLiteral("SCALE|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(baseFields(*this))
        .arg(m_orientation == Orientation::Horizontal ? QStringLiteral("H") : QStringLiteral("V"))
        .arg(m_minVal)
        .arg(m_maxVal)
        .arg(m_majorTicks)
        .arg(m_minorTicks)
        .arg(m_tickColor.name(QColor::HexArgb))
        .arg(m_labelColor.name(QColor::HexArgb))
        .arg(m_fontSize);
    // Phase B2 tail (append-only): showType | titleColour
    base += QLatin1Char('|') + (m_showType ? QStringLiteral("1") : QStringLiteral("0"));
    base += QLatin1Char('|') + m_titleColour.name(QColor::HexArgb);
    // Phase B3 tail (append-only): scaleStyle | lowColour | highColour |
    //   lowLongTicks | highLongTicks | lowStartNumber | highEndNumber |
    //   lowIncrement | highIncrement | centrePerc
    base += QLatin1Char('|') + (m_scaleStyle == ScaleStyle::GeneralScale
                                    ? QStringLiteral("GS")
                                    : QStringLiteral("L"));
    base += QLatin1Char('|') + m_lowColour.name(QColor::HexArgb);
    base += QLatin1Char('|') + m_highColour.name(QColor::HexArgb);
    base += QLatin1Char('|') + QString::number(m_lowLongTicks);
    base += QLatin1Char('|') + QString::number(m_highLongTicks);
    base += QLatin1Char('|') + QString::number(m_lowStartNumber);
    base += QLatin1Char('|') + QString::number(m_highEndNumber);
    base += QLatin1Char('|') + QString::number(m_lowIncrement);
    base += QLatin1Char('|') + QString::number(m_highIncrement);
    base += QLatin1Char('|') + QString::number(static_cast<double>(m_centrePerc));
    return base;
}

bool ScaleItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 15 || parts[0] != QLatin1String("SCALE")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }

    m_orientation = (parts[7] == QLatin1String("H")) ? Orientation::Horizontal : Orientation::Vertical;

    bool ok = true;
    m_minVal     = parts[8].toDouble(&ok);  if (!ok) { return false; }
    m_maxVal     = parts[9].toDouble(&ok);  if (!ok) { return false; }
    m_majorTicks = parts[10].toInt(&ok);    if (!ok) { return false; }
    m_minorTicks = parts[11].toInt(&ok);    if (!ok) { return false; }
    m_tickColor  = QColor(parts[12]);       if (!m_tickColor.isValid()) { return false; }
    m_labelColor = QColor(parts[13]);       if (!m_labelColor.isValid()) { return false; }
    m_fontSize   = parts[14].toInt(&ok);    if (!ok) { return false; }

    // Phase B2 — ShowType / TitleColour (append-only tail)
    if (parts.size() >= 16) { m_showType = (parts[15] == QLatin1String("1")); }
    if (parts.size() >= 17) {
        QColor tc = QColor(parts[16]);
        if (tc.isValid()) { m_titleColour = tc; }
    }

    // Phase B3 — GeneralScale parameters (append-only tail)
    if (parts.size() >= 18) {
        m_scaleStyle = (parts[17] == QLatin1String("GS"))
                           ? ScaleStyle::GeneralScale
                           : ScaleStyle::Linear;
    }
    if (parts.size() >= 19) {
        QColor c = QColor(parts[18]);
        if (c.isValid()) { m_lowColour = c; }
    }
    if (parts.size() >= 20) {
        QColor c = QColor(parts[19]);
        if (c.isValid()) { m_highColour = c; }
    }
    {
        bool ok2 = true;
        if (parts.size() >= 21) { int v = parts[20].toInt(&ok2); if (ok2) { m_lowLongTicks = v; } }
        if (parts.size() >= 22) { int v = parts[21].toInt(&ok2); if (ok2) { m_highLongTicks = v; } }
        if (parts.size() >= 23) { int v = parts[22].toInt(&ok2); if (ok2) { m_lowStartNumber = v; } }
        if (parts.size() >= 24) { int v = parts[23].toInt(&ok2); if (ok2) { m_highEndNumber = v; } }
        if (parts.size() >= 25) { int v = parts[24].toInt(&ok2); if (ok2) { m_lowIncrement = v; } }
        if (parts.size() >= 26) { int v = parts[25].toInt(&ok2); if (ok2) { m_highIncrement = v; } }
        if (parts.size() >= 27) { float v = parts[26].toFloat(&ok2); if (ok2) { m_centrePerc = v; } }
    }

    return true;
}

// ---------------------------------------------------------------------------
// TextItem
// Format: TEXT|x|y|w|h|bindingId|zOrder|label|textColor|fontSize|bold(0/1)|suffix|decimals
// ---------------------------------------------------------------------------

void TextItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);

    // Dynamic pixel-based font sizing (matches NeedleItem pattern).
    // Scale with rect height, m_fontSize is the minimum floor.
    const int pixH = rect.height();
    int dynFontSize = qMax(m_fontSize, static_cast<int>(pixH * 0.7f));

    QFont font = p.font();
    font.setBold(m_bold);

    QString text;
    if (m_bindingId >= 0 && m_value < m_minValidValue && !m_idleText.isEmpty()) {
        text = m_idleText;
    } else if (m_bindingId >= 0) {
        text = QString::number(m_value, 'f', m_decimals) + m_suffix;
    } else {
        text = m_label;
    }

    // Shrink font if text overflows rect width
    font.setPixelSize(dynFontSize);
    while (dynFontSize > m_fontSize) {
        const QFontMetrics fm(font);
        if (fm.horizontalAdvance(text) <= rect.width()) {
            break;
        }
        --dynFontSize;
        font.setPixelSize(dynFontSize);
    }

    p.setFont(font);
    p.setPen(m_textColor);
    p.drawText(rect, Qt::AlignCenter, text);
}

QString TextItem::serialize() const
{
    return QStringLiteral("TEXT|%1|%2|%3|%4|%5|%6|%7")
        .arg(baseFields(*this))
        .arg(m_label)
        .arg(m_textColor.name(QColor::HexArgb))
        .arg(m_fontSize)
        .arg(m_bold ? 1 : 0)
        .arg(m_suffix)
        .arg(m_decimals);
}

bool TextItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 13 || parts[0] != QLatin1String("TEXT")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }

    m_label     = parts[7];
    m_textColor = QColor(parts[8]);
    if (!m_textColor.isValid()) { return false; }

    bool ok = true;
    m_fontSize  = parts[9].toInt(&ok);   if (!ok) { return false; }
    m_bold      = parts[10].toInt(&ok) != 0; if (!ok) { return false; }
    m_suffix    = parts[11];
    m_decimals  = parts[12].toInt(&ok); if (!ok) { return false; }

    return true;
}

// ---------------------------------------------------------------------------
// NeedleItem — Arc-style S-meter needle
// Ported from AetherSDR src/gui/SMeterWidget.cpp
// ---------------------------------------------------------------------------

NeedleItem::NeedleItem(QObject* parent)
    : MeterItem(parent)
{
}

// From AetherSDR sizeHint 280x140: lock to 2:1 aspect ratio.
// Centers the meter drawing area within the item's pixel rect.
QRect NeedleItem::meterRect(int widgetW, int widgetH) const
{
    // Use the full pixel rect — no aspect-ratio letterboxing.
    // The arc geometry already adapts via effectiveW = min(pw, ph * kTargetAspect)
    // in each paint method, so the needle scales properly while the background
    // and text labels fill the full available width.
    return pixelRect(widgetW, widgetH);
}

bool NeedleItem::participatesIn(Layer layer) const
{
    // NeedleItem renders across all 4 pipeline layers
    Q_UNUSED(layer);
    return true;
}

// From AetherSDR SMeterWidget.cpp setLevel() — SMOOTH_ALPHA = 0.3
void NeedleItem::setValue(double v)
{
    MeterItem::setValue(v);

    const float val = static_cast<float>(v);

    // Exponential smoothing (from AetherSDR SMOOTH_ALPHA = 0.3)
    m_smoothedDbm += kSmoothAlpha * (val - m_smoothedDbm);

    // Clamp to the appropriate value range. Calibrated needles
    // (ANANMM Volts/Amps/Power/SWR/Compression/ALC) use the
    // calibration map's key range — they're in native units (volts,
    // amps, watts, etc.), NOT dBm. Clamping a 13V reading to the
    // [-127, -13] dBm range mangles it to -13, then
    // calibratedPosition() looks up -13 in a {10, 12.5, 15} map and
    // returns the first calibration point — the symptom was every
    // calibrated needle drawing as a near-horizontal line at the
    // offset row.
    if (m_scaleCalibration.isEmpty()) {
        m_smoothedDbm = std::clamp(m_smoothedDbm, kS0Dbm, kMaxDbm);
    } else {
        const float minKey = m_scaleCalibration.firstKey();
        const float maxKey = m_scaleCalibration.lastKey();
        m_smoothedDbm = std::clamp(m_smoothedDbm, minKey, maxKey);
    }

    // Peak tracking (from AetherSDR Medium preset: 500ms hold, 10 dB/sec decay)
    if (m_smoothedDbm > m_peakDbm) {
        m_peakDbm = m_smoothedDbm;
        m_peakHoldCounter = kPeakHoldFrames;
    } else if (m_peakHoldCounter > 0) {
        --m_peakHoldCounter;
    } else {
        m_peakDbm -= kPeakDecayPerFrame;
        if (m_peakDbm < m_smoothedDbm) {
            m_peakDbm = m_smoothedDbm;
        }
    }

    // Hard reset every 10 seconds (from AetherSDR m_peakReset = 10000ms)
    ++m_peakResetCounter;
    if (m_peakResetCounter >= kPeakResetFrames) {
        m_peakDbm = m_smoothedDbm;
        m_peakResetCounter = 0;
    }
}

// From AetherSDR SMeterWidget.cpp dbmToFraction()
// S0-S9 occupies left 60% of arc; S9-S9+60 occupies right 40%
float NeedleItem::dbmToFraction(float dbm) const
{
    const float clamped = std::clamp(dbm, kS0Dbm, kMaxDbm);
    if (clamped <= kS9Dbm) {
        return 0.6f * (clamped - kS0Dbm) / (kS9Dbm - kS0Dbm);
    }
    return 0.6f + 0.4f * (clamped - kS9Dbm) / (kMaxDbm - kS9Dbm);
}

// Interpolate needle position from scale calibration map.
// From Thetis MeterManager.cs calibration point interpolation.
// Finds the two calibration points bracketing `value` and linearly
// interpolates the (x,y) position between them.
QPointF NeedleItem::calibratedPosition(float value) const
{
    if (m_scaleCalibration.isEmpty()) {
        return QPointF(0.5, 0.5);
    }

    // Use QMap's ordered iteration to find bracketing keys
    auto it = m_scaleCalibration.lowerBound(value);

    // Value is at or beyond the last calibration point
    if (it == m_scaleCalibration.end()) {
        return (--it).value();
    }

    // Value is at or before the first calibration point
    if (it == m_scaleCalibration.begin()) {
        return it.value();
    }

    // Exact match
    if (it.key() == value) {
        return it.value();
    }

    // Interpolate between the two bracketing points
    auto upper = it;
    auto lower = --it;
    const float range = upper.key() - lower.key();
    if (range <= 0.0f) {
        return lower.value();
    }
    const float t = (value - lower.key()) / range;
    const QPointF& p0 = lower.value();
    const QPointF& p1 = upper.value();
    return QPointF(p0.x() + t * (p1.x() - p0.x()),
                   p0.y() + t * (p1.y() - p0.y()));
}

// From AetherSDR SMeterWidget.cpp sUnitsText()
QString NeedleItem::sUnitsText(float dbm) const
{
    if (dbm <= kS0Dbm) {
        return QStringLiteral("S0");
    }
    if (dbm <= kS9Dbm) {
        const int sUnit = qBound(0, qRound((dbm - kS0Dbm) / kDbPerS), 9);
        return QStringLiteral("S%1").arg(sUnit);
    }
    const int over = qRound(dbm - kS9Dbm);
    return QStringLiteral("S9+%1").arg(over);
}

void NeedleItem::paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    switch (layer) {
    case Layer::Background:     paintBackground(p, widgetW, widgetH); break;
    case Layer::OverlayStatic:  paintOverlayStatic(p, widgetW, widgetH); break;
    case Layer::OverlayDynamic: paintOverlayDynamic(p, widgetW, widgetH); break;
    default: break; // Geometry handled by emitVertices()
    }
    p.restore();
}

void NeedleItem::paint(QPainter& p, int widgetW, int widgetH)
{
    // CPU fallback: render all layers in a single QPainter pass
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    paintBackground(p, widgetW, widgetH);
    paintOverlayStatic(p, widgetW, widgetH);

    // CPU needle: draw as a simple line (no vertex geometry available)
    const QRect rect = meterRect(widgetW, widgetH);
    const float pw = static_cast<float>(rect.width());
    const float ph = static_cast<float>(rect.height());
    const float cx = rect.left() + pw * 0.5f;
    const float radius = pw * kRadiusRatio;
    const float cy = rect.top() + ph + radius - ph * kCenterYRatio;
    const float pivotY = rect.top() + ph + 6.0f;

    const float frac = dbmToFraction(m_smoothedDbm);
    const float angleRad = qDegreesToRadians(kArcEndDeg - frac * (kArcEndDeg - kArcStartDeg));
    const float tipDist = radius + 14.0f;
    const float tipX = cx + tipDist * std::cos(angleRad);
    const float tipY = cy - tipDist * std::sin(angleRad);

    // Shadow
    QPen shadowPen(QColor(0, 0, 0, 80));
    shadowPen.setWidthF(3.0f);
    p.setPen(shadowPen);
    p.drawLine(QPointF(cx + 1.0f, pivotY + 1.0f), QPointF(tipX + 1.0f, tipY + 1.0f));

    // Needle
    QPen needlePen(Qt::white);
    needlePen.setWidthF(2.0f);
    p.setPen(needlePen);
    p.drawLine(QPointF(cx, pivotY), QPointF(tipX, tipY));

    paintOverlayDynamic(p, widgetW, widgetH);
    p.restore();
}

// From AetherSDR SMeterWidget.cpp paintEvent() — arc drawing section
void NeedleItem::paintBackground(QPainter& p, int widgetW, int widgetH)
{
    // Phase 3G-6 block 2: calibration-driven mode (ANANMM, CrossNeedle)
    // paints the gauge face via a companion ImageItem earlier in the
    // Background layer. Skip the default arc + fillRect entirely so
    // that image is visible.
    if (!m_scaleCalibration.isEmpty()) {
        return;
    }

    const QRect rect = meterRect(widgetW, widgetH);
    const float pw = static_cast<float>(rect.width());
    const float ph = static_cast<float>(rect.height());

    // Background fill — from AetherSDR SMeterWidget.cpp line 197
    p.fillRect(rect, QColor(0x0f, 0x0f, 0x1a));

    // From AetherSDR: cx = width*0.5, radius = width*0.85
    // cy = height + radius - height*0.65
    const float cx = rect.left() + pw * 0.5f;
    const float radius = pw * kRadiusRatio;
    const float cy = rect.top() + ph + radius - ph * kCenterYRatio;

    const QRectF arcRect(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // S9 at fraction 0.6 → angle = 125 - 0.6*70 = 83 degrees
    const float arcSpan = kArcEndDeg - kArcStartDeg;  // 70 degrees
    const float s9Frac = 0.6f;
    const float s9Angle = kArcEndDeg - s9Frac * arcSpan;  // 83 degrees

    // AetherSDR uses default cap style (SquareCap) — do not set FlatCap
    QPen arcPen;
    arcPen.setWidthF(3.0f);

    // White segment: S9 angle to ARC_END (left portion, S0-S9)
    // QPainter drawArc: angles in 1/16 degrees, 0=3 o'clock, positive=CCW
    arcPen.setColor(QColor(0xc8, 0xd8, 0xe8));  // From AetherSDR
    p.setPen(arcPen);
    const int whiteStart = static_cast<int>(s9Angle * 16.0f);
    const int whiteSpan  = static_cast<int>((kArcEndDeg - s9Angle) * 16.0f);
    p.drawArc(arcRect, whiteStart, whiteSpan);

    // Red segment: ARC_START to S9 angle (right portion, S9+)
    arcPen.setColor(QColor(0xff, 0x44, 0x44));  // From AetherSDR
    p.setPen(arcPen);
    const int redStart = static_cast<int>(kArcStartDeg * 16.0f);
    const int redSpan  = static_cast<int>((s9Angle - kArcStartDeg) * 16.0f);
    p.drawArc(arcRect, redStart, redSpan);

    // Inner TX arc — from AetherSDR SMeterWidget.cpp lines 236-269
    // Blue arc 6px inside the outer arc (always drawn)
    const float innerR = radius - 6.0f;
    const QRectF innerArc(cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);
    arcPen.setColor(QColor(0x00, 0x80, 0xd0));  // From AetherSDR blueColor
    p.setPen(arcPen);
    const int fullStart = static_cast<int>(kArcStartDeg * 16.0f);
    const int fullSpan  = static_cast<int>((kArcEndDeg - kArcStartDeg) * 16.0f);
    p.drawArc(innerArc, fullStart, fullSpan);
}

// Needle geometry: tapered quad from pivot to tip (2 triangles = 6 verts).
// Peak marker: amber triangle at arc edge (3 verts).
// From AetherSDR SMeterWidget.cpp paintEvent() — needle drawing section
//
// Phase 3G-6 block 2: branches on m_scaleCalibration. If non-empty,
// the pivot and tip come from m_needleOffset + calibratedPosition()
// (normalized gauge-face coordinates) with m_radiusRatio applied as
// an elliptical scale around the pivot and m_lengthFactor as a
// radial multiplier. The peak marker is suppressed in this mode —
// it isn't meaningful on a TX power/SWR gauge.
void NeedleItem::emitVertices(QVector<float>& verts, int widgetW, int widgetH)
{
    const QRect rect = meterRect(widgetW, widgetH);
    const float pw = static_cast<float>(rect.width());
    const float ph = static_cast<float>(rect.height());

    const bool calibrated = !m_scaleCalibration.isEmpty();

    float pivotX = 0.0f, pivotY = 0.0f, tipX = 0.0f, tipY = 0.0f;
    // Arc-mode intermediates reused by the peak marker block below.
    float arcCx = 0.0f, arcCy = 0.0f, arcRadius = 0.0f;

    if (calibrated) {
        // Pivot = m_needleOffset * rect (normalized to pixels).
        pivotX = rect.left() + pw * static_cast<float>(m_needleOffset.x());
        pivotY = rect.top()  + ph * static_cast<float>(m_needleOffset.y());

        // Tip: calibrated normalized position for the current value,
        // then apply per-axis radius ratio and length factor as an
        // offset from the pivot.
        const QPointF tipNorm = calibratedPosition(static_cast<float>(m_smoothedDbm));
        const float dxN = (static_cast<float>(tipNorm.x()) - static_cast<float>(m_needleOffset.x()))
                        * static_cast<float>(m_radiusRatio.x()) * m_lengthFactor;
        const float dyN = (static_cast<float>(tipNorm.y()) - static_cast<float>(m_needleOffset.y()))
                        * static_cast<float>(m_radiusRatio.y()) * m_lengthFactor;
        tipX = pivotX + pw * dxN;
        tipY = pivotY + ph * dyN;
    } else {
        arcCx = rect.left() + pw * 0.5f;
        arcRadius = pw * kRadiusRatio;
        arcCy = rect.top() + ph + arcRadius - ph * kCenterYRatio;

        // From AetherSDR: needleCy = height + 6.0f (pivot below widget bottom)
        pivotX = arcCx;
        pivotY = rect.top() + ph + 6.0f;

        const float frac = dbmToFraction(m_smoothedDbm);
        const float angleDeg = kArcEndDeg - frac * (kArcEndDeg - kArcStartDeg);
        const float angleRad = qDegreesToRadians(angleDeg);

        // From AetherSDR: needle tip at radius + 14
        const float tipDist = arcRadius + 14.0f;
        tipX = arcCx + tipDist * std::cos(angleRad);
        tipY = arcCy - tipDist * std::sin(angleRad);
    }

    // Perpendicular direction for needle width
    const float dx = tipX - pivotX;
    const float dy = tipY - pivotY;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) { return; }

    const float perpX = -dy / len;
    const float perpY =  dx / len;

    // From AetherSDR: shadow pen width 3 (halfWidth 1.5), needle pen width 2 (halfWidth 1.0)
    // Uniform width along entire length — NOT tapered (matches QPainter drawLine)
    const float shadowHW = 1.5f;
    const float needleHW = 1.0f;

    // Pixel to NDC conversion lambdas
    const float invW = 2.0f / static_cast<float>(widgetW);
    const float invH = 2.0f / static_cast<float>(widgetH);
    auto ndcX = [invW](float px) { return px * invW - 1.0f; };
    auto ndcY = [invH](float py) { return 1.0f - py * invH; };

    // --- Shadow quad (black, offset +1px) ---
    // From AetherSDR SMeterWidget.cpp line 429: QPen(QColor(0,0,0,80), 3), offset (+1,+1)
    const float sr = 0.0f, sg = 0.0f, sb = 0.0f, sa = 80.0f / 255.0f;
    const float sOff = 1.0f;
    const float sp1x = pivotX + perpX * shadowHW + sOff, sp1y = pivotY + perpY * shadowHW + sOff;
    const float sp2x = pivotX - perpX * shadowHW + sOff, sp2y = pivotY - perpY * shadowHW + sOff;
    const float sp3x = tipX + perpX * shadowHW + sOff,   sp3y = tipY + perpY * shadowHW + sOff;
    const float sp4x = tipX - perpX * shadowHW + sOff,   sp4y = tipY - perpY * shadowHW + sOff;
    // Triangle 1
    verts << ndcX(sp1x) << ndcY(sp1y) << sr << sg << sb << sa;
    verts << ndcX(sp2x) << ndcY(sp2y) << sr << sg << sb << sa;
    verts << ndcX(sp3x) << ndcY(sp3y) << sr << sg << sb << sa;
    // Triangle 2
    verts << ndcX(sp3x) << ndcY(sp3y) << sr << sg << sb << sa;
    verts << ndcX(sp2x) << ndcY(sp2y) << sr << sg << sb << sa;
    verts << ndcX(sp4x) << ndcY(sp4y) << sr << sg << sb << sa;

    // --- Needle quad (color from m_needleColor, uniform width) ---
    // From AetherSDR SMeterWidget.cpp line 433: QPen(#ffffff, 2).
    // Phase 3G-6 block 2: honour per-needle color from the factory.
    const float nr = static_cast<float>(m_needleColor.redF());
    const float ng = static_cast<float>(m_needleColor.greenF());
    const float nb = static_cast<float>(m_needleColor.blueF());
    const float na = static_cast<float>(m_needleColor.alphaF());
    const float np1x = pivotX + perpX * needleHW, np1y = pivotY + perpY * needleHW;
    const float np2x = pivotX - perpX * needleHW, np2y = pivotY - perpY * needleHW;
    const float np3x = tipX + perpX * needleHW,   np3y = tipY + perpY * needleHW;
    const float np4x = tipX - perpX * needleHW,   np4y = tipY - perpY * needleHW;
    // Triangle 1
    verts << ndcX(np1x) << ndcY(np1y) << nr << ng << nb << na;
    verts << ndcX(np2x) << ndcY(np2y) << nr << ng << nb << na;
    verts << ndcX(np3x) << ndcY(np3y) << nr << ng << nb << na;
    // Triangle 2
    verts << ndcX(np3x) << ndcY(np3y) << nr << ng << nb << na;
    verts << ndcX(np2x) << ndcY(np2y) << nr << ng << nb << na;
    verts << ndcX(np4x) << ndcY(np4y) << nr << ng << nb << na;

    // --- Peak marker triangle (amber, points INWARD toward center) ---
    // From AetherSDR SMeterWidget.cpp lines 437-462: tip at radius-2, base 6px behind.
    // Phase 3G-6 block 2: calibration mode (TX power/SWR/etc.) has no
    // peak marker — skip emitting the triangle entirely.
    if (!calibrated && m_peakDbm > m_smoothedDbm + 1.0f) {
        const float peakFrac = dbmToFraction(m_peakDbm);
        const float peakAngleDeg = kArcEndDeg - peakFrac * (kArcEndDeg - kArcStartDeg);
        const float peakAngleRad = qDegreesToRadians(peakAngleDeg);

        const float mr = 1.0f, mg = 0.667f, mb = 0.0f, ma = 1.0f;  // #ffaa00
        const float cosA = std::cos(peakAngleRad);
        const float sinA = std::sin(peakAngleRad);

        // Tip at radius-2 (inside the arc), base 6px behind toward center
        const float markerTipR = arcRadius - 2.0f;
        const float markerBaseR = arcRadius - 8.0f;
        const float markerHalf = 3.0f;

        // Tip (inward, on arc inner edge)
        const float mtx = arcCx + markerTipR * cosA;
        const float mty = arcCy - markerTipR * sinA;
        // Base center (further inward)
        const float mbcx = arcCx + markerBaseR * cosA;
        const float mbcy = arcCy - markerBaseR * sinA;
        // Perpendicular at base — from AetherSDR: perpCos = -sinA, perpSin = cosA
        const float mpx = -sinA * markerHalf;
        const float mpy =  cosA * markerHalf;

        verts << ndcX(mtx) << ndcY(mty) << mr << mg << mb << ma;
        verts << ndcX(mbcx + mpx) << ndcY(mbcy + mpy) << mr << mg << mb << ma;
        verts << ndcX(mbcx - mpx) << ndcY(mbcy - mpy) << mr << mg << mb << ma;
    }
}

// From AetherSDR SMeterWidget.cpp paintEvent() — tick drawing section
void NeedleItem::paintOverlayStatic(QPainter& p, int widgetW, int widgetH)
{
    // Phase 3G-6 block 2: calibration-driven mode gets its scale ticks
    // and source labels from the companion ImageItem gauge face /
    // NeedleScalePwrItem. Skip the built-in S-meter ticks + source
    // label entirely.
    if (!m_scaleCalibration.isEmpty()) {
        return;
    }

    const QRect rect = meterRect(widgetW, widgetH);
    const float pw = static_cast<float>(rect.width());
    const float ph = static_cast<float>(rect.height());
    const float cx = rect.left() + pw * 0.5f;
    const float radius = pw * kRadiusRatio;
    const float cy = rect.top() + ph + radius - ph * kCenterYRatio;

    // Tick definitions from AetherSDR SMeterWidget.cpp lines 340-347
    // Only odd S-units + over-S9 marks shown. BARE numbers (no "S" prefix).
    struct TickDef { float dbm; const char* label; bool isRed; };
    static const TickDef ticks[] = {
        {-121.0f, "1",   false},  // From AetherSDR: QString::number(s)
        {-109.0f, "3",   false},
        { -97.0f, "5",   false},
        { -85.0f, "7",   false},
        { -73.0f, "9",   false},
        { -53.0f, "+20", true},   // From AetherSDR: QString("+%1").arg(over)
        { -33.0f, "+40", true},
    };

    // Needle pivot for needleDir calculation
    // From AetherSDR SMeterWidget.cpp line 279-286: direction from (cx, needleCy) through arc point
    const float pivotY = rect.top() + ph + 6.0f;

    // From AetherSDR: tick font = max(10, height/10) px, bold
    QFont tickFont = p.font();
    const int tickFontSize = qMax(10, static_cast<int>(ph) / 10);
    tickFont.setPixelSize(tickFontSize);
    tickFont.setBold(true);
    p.setFont(tickFont);
    const QFontMetrics tfm(tickFont);

    for (const auto& tick : ticks) {
        const float frac = dbmToFraction(tick.dbm);
        const float angleDeg = kArcEndDeg - frac * (kArcEndDeg - kArcStartDeg);
        const float angleRad = qDegreesToRadians(angleDeg);

        // Arc point
        const float arcPtX = cx + radius * std::cos(angleRad);
        const float arcPtY = cy - radius * std::sin(angleRad);

        // needleDir: direction from pivot (cx, pivotY) through arc point, normalized
        // From AetherSDR SMeterWidget.cpp lines 279-286
        const float ndx = arcPtX - cx;
        const float ndy = arcPtY - pivotY;
        const float ndLen = std::sqrt(ndx * ndx + ndy * ndy);
        const float ux = ndx / ndLen;
        const float uy = ndy / ndLen;

        // From AetherSDR: tick starts 2px outside arc, extends 14px outward (both outside)
        // SMeterWidget.cpp lines 296-297
        const QPointF inner(arcPtX + 2.0f * ux, arcPtY + 2.0f * uy);
        const QPointF outer(arcPtX + 14.0f * ux, arcPtY + 14.0f * uy);

        const QColor tickColor = tick.isRed ? QColor(0xff, 0x44, 0x44)
                                            : QColor(0xc8, 0xd8, 0xe8);
        QPen tickPen(tickColor);
        tickPen.setWidthF(1.5f);
        p.setPen(tickPen);
        p.drawLine(inner, outer);

        // From AetherSDR: label 26px from arc point along needleDir
        // SMeterWidget.cpp line 303
        const QPointF labelPt(arcPtX + 26.0f * ux, arcPtY + 26.0f * uy);
        p.setPen(tickColor);
        const QString labelStr = QString::fromLatin1(tick.label);
        const int tw = tfm.horizontalAdvance(labelStr);
        p.drawText(QPointF(labelPt.x() - tw / 2.0f,
                            labelPt.y() + tfm.ascent() / 2.0f), labelStr);
    }

    // From AetherSDR: source label centered, max(9, h/14) px, non-bold, #8090a0
    QFont srcFont = p.font();
    const int srcFontSize = qMax(9, static_cast<int>(ph) / 14);
    srcFont.setPixelSize(srcFontSize);
    srcFont.setBold(false);
    p.setFont(srcFont);
    p.setPen(QColor(0x80, 0x90, 0xa0));
    const QFontMetrics srcFm(srcFont);
    const int srcW = srcFm.horizontalAdvance(m_sourceLabel);
    p.drawText(QPointF(rect.left() + (pw - srcW) / 2.0f,
                        rect.top() + srcFm.height() + 2.0f), m_sourceLabel);
}

// From AetherSDR SMeterWidget.cpp paintEvent() — readout drawing section
void NeedleItem::paintOverlayDynamic(QPainter& p, int widgetW, int widgetH)
{
    // Phase 3G-6 block 2: calibration-driven mode has no S-unit / dBm
    // readouts — the gauge face provides the reference frame visually.
    if (!m_scaleCalibration.isEmpty()) {
        return;
    }

    const QRect rect = meterRect(widgetW, widgetH);
    const float ph = static_cast<float>(rect.height());

    // Compute source label baseline for vertical positioning
    const int srcFontSize = qMax(9, static_cast<int>(ph) / 14);
    QFont srcFont = p.font();
    srcFont.setPixelSize(srcFontSize);
    const QFontMetrics srcFm(srcFont);
    const float topY = rect.top() + srcFm.height() + 2.0f;

    // From AetherSDR: value font = max(13, h/8) px, bold
    QFont valFont = p.font();
    const int valFontSize = qMax(13, static_cast<int>(ph) / 8);
    valFont.setPixelSize(valFontSize);
    valFont.setBold(true);
    p.setFont(valFont);
    const QFontMetrics valFm(valFont);

    // S-unit text (left, cyan) — from AetherSDR SMeterWidget.cpp line 533
    // AetherSDR uses drawText(x, topY, text) where topY is the baseline.
    // QPointF overload also treats Y as baseline — pass topY directly, no ascent offset.
    const QString sText = sUnitsText(m_smoothedDbm);
    p.setPen(QColor(0x00, 0xb4, 0xd8));
    p.drawText(QPointF(rect.left() + 6.0f, topY), sText);

    // dBm text (right, light steel) — from AetherSDR SMeterWidget.cpp line 537
    const QString dbmText = QString::number(static_cast<int>(std::round(m_smoothedDbm)))
                          + QStringLiteral(" dBm");
    p.setPen(QColor(0xc8, 0xd8, 0xe8));
    const int dbmW = valFm.horizontalAdvance(dbmText);
    p.drawText(QPointF(rect.right() - dbmW - 6.0f, topY), dbmText);
}

// Format:
//   NEEDLE|x|y|w|h|bindingId|zOrder|sourceLabel
//     [|onlyRx|onlyTx|displayGroup]                    -- added block 1
//     [|calCount|v:x:y|v:x:y|...]                      -- added block 2
//     [|needleColor|offsetX|offsetY|radX|radY|lenFactor|direction|normalise100|maxPower|attack|decay]
//                                                      -- added block 2
// Each trailing group is optional for backward compatibility: older
// serialized state deserializes cleanly with defaults for any missing
// tail fields.
QString NeedleItem::serialize() const
{
    QString s = QStringLiteral("NEEDLE|%1|%2|%3|%4|%5")
        .arg(baseFields(*this))
        .arg(m_sourceLabel)
        .arg(m_onlyWhenRx ? 1 : 0)
        .arg(m_onlyWhenTx ? 1 : 0)
        .arg(m_displayGroup);

    // Calibration map — value:x:y triplets with a leading count.
    s += QStringLiteral("|%1").arg(m_scaleCalibration.size());
    for (auto it = m_scaleCalibration.constBegin();
         it != m_scaleCalibration.constEnd(); ++it) {
        s += QStringLiteral("|%1:%2:%3")
            .arg(static_cast<double>(it.key()))
            .arg(it.value().x())
            .arg(it.value().y());
    }

    // Visual / geometry / smoothing extras.
    s += QStringLiteral("|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11")
        .arg(m_needleColor.name(QColor::HexArgb))
        .arg(m_needleOffset.x())
        .arg(m_needleOffset.y())
        .arg(m_radiusRatio.x())
        .arg(m_radiusRatio.y())
        .arg(static_cast<double>(m_lengthFactor))
        .arg(static_cast<int>(m_direction))
        .arg(m_normaliseTo100W ? 1 : 0)
        .arg(static_cast<double>(m_maxPower))
        .arg(static_cast<double>(m_attackRatio))
        .arg(static_cast<double>(m_decayRatio));

    return s;
}

bool NeedleItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 8 || parts[0] != QLatin1String("NEEDLE")) {
        return false;
    }
    if (!parseBaseFields(*this, parts)) {
        return false;
    }
    m_sourceLabel = parts[7];

    // Filter fields (block 1)
    int idx = 8;
    if (parts.size() >= idx + 3) {
        m_onlyWhenRx   = (parts[idx++].toInt() != 0);
        m_onlyWhenTx   = (parts[idx++].toInt() != 0);
        m_displayGroup = parts[idx++].toInt();
    }

    // Calibration map (block 2)
    if (parts.size() > idx) {
        bool ok = false;
        const int calCount = parts[idx].toInt(&ok);
        if (ok && calCount >= 0 && parts.size() >= idx + 1 + calCount) {
            ++idx;
            QMap<float, QPointF> cal;
            for (int i = 0; i < calCount; ++i) {
                const QStringList tri = parts[idx + i].split(QLatin1Char(':'));
                if (tri.size() < 3) { cal.clear(); break; }
                bool okv = true, okx = true, oky = true;
                const float v  = tri[0].toFloat(&okv);
                const float nx = tri[1].toFloat(&okx);
                const float ny = tri[2].toFloat(&oky);
                if (!okv || !okx || !oky) { cal.clear(); break; }
                cal.insert(v, QPointF(nx, ny));
            }
            m_scaleCalibration = cal;
            idx += calCount;
        }
    }

    // Visual / geometry / smoothing extras (block 2)
    if (parts.size() >= idx + 11) {
        const QColor col(parts[idx]);
        if (col.isValid()) { m_needleColor = col; }
        m_needleOffset    = QPointF(parts[idx + 1].toDouble(), parts[idx + 2].toDouble());
        m_radiusRatio     = QPointF(parts[idx + 3].toDouble(), parts[idx + 4].toDouble());
        m_lengthFactor    = parts[idx + 5].toFloat();
        m_direction       = static_cast<NeedleDirection>(parts[idx + 6].toInt());
        m_normaliseTo100W = (parts[idx + 7].toInt() != 0);
        m_maxPower        = parts[idx + 8].toFloat();
        m_attackRatio     = parts[idx + 9].toFloat();
        m_decayRatio      = parts[idx + 10].toFloat();
    }

    return true;
}

} // namespace NereusSDR
