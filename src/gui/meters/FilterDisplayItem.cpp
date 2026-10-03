// =================================================================
// src/gui/meters/FilterDisplayItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-26 - R-R3-49 (remote-window parity Task 16): the
//                 high-resolution curve can come from a Core's bins
//                 (setFilterResponseBins) in a remote window, resampled as
//                 the local channel's is. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
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

#include "FilterDisplayItem.h"

// From Thetis clsFilterItem (MeterManager.cs:16852+)

#include "core/RxChannel.h"
#include "core/spectrum/WaterfallPalettes.h"

#include <QPainter>
#include <QPolygonF>
#include <QStringList>
#include <algorithm>
#include <iterator>
#include <cmath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Constructor
// From Thetis clsFilterItem ctor (MeterManager.cs:16981+)
// Defaults: PANAFALL display mode, 4-frame waterfall interval, fill enabled
// ---------------------------------------------------------------------------
FilterDisplayItem::FilterDisplayItem(QObject* parent)
    : MeterItem(parent)
{
    // From Thetis clsFilterItem ctor (MeterManager.cs:18326) [@3759d096] — MiniSpec.PIXELS
    m_waterfallImage = QImage(kSpectrumPixels, 200, QImage::Format_RGB32);
    m_waterfallImage.fill(Qt::black);
}

// ---------------------------------------------------------------------------
// setHighResolution() — Task 4.4
// Toggle high-resolution FIR magnitude rendering on/off.
// When on, paintFilterEdges() calls paintHighResolutionFilterCurve() instead
// of the simplified box passband.  Calls update() so the host MeterWidget
// repaints on the next frame.
// ---------------------------------------------------------------------------
void FilterDisplayItem::setHighResolution(bool h)
{
    if (m_highResolution == h) {
        return;
    }
    m_highResolution = h;
    // MeterItem is not a QWidget — the host MeterWidget repaints on its own
    // timer, so no explicit update() call is needed here.  The new mode takes
    // effect on the next paint frame automatically.
}

// ---------------------------------------------------------------------------
// bindRxChannel() — Task 4.4
// Bind the RxChannel whose filterResponseMagnitudes() supplies the FIR curve.
// Non-owning pointer — caller must clear (bindRxChannel(nullptr)) before the
// channel is destroyed.
// ---------------------------------------------------------------------------
void FilterDisplayItem::bindRxChannel(RxChannel* channel)
{
    m_rxChannel = channel;
}

// ---------------------------------------------------------------------------
// setSpectrumData()
// Compatibility entry point for a trace already reduced to display dBm.
// ---------------------------------------------------------------------------
void FilterDisplayItem::setSpectrumData(const float* bins, int count)
{
    if (!bins || count <= 0) {
        return;
    }

    m_spectrumData.assign(bins, bins + count);
    m_frameAvailable = true;
}

void FilterDisplayItem::clearFrame()
{
    m_spectrumData.clear();
    m_frameAvailable = false;
    m_frameTransmitting = false;
    m_frameCentreHz = 0.0;
    m_frameSpanHz = 0.0;
    m_rxLow = m_rxHigh = m_txLow = m_txHigh = -1;
    m_notchPositions.clear();
    m_filterResponseBins.clear();
    m_rfFilterResponseDb.clear();
    m_rxChannel = nullptr;
    m_waterfallFrameCount = 0;
    m_waterfallImage.fill(Qt::black);
}

int FilterDisplayItem::tracePeakPixelForTest() const
{
    if (m_spectrumData.empty()) { return -1; }
    return int(std::distance(m_spectrumData.begin(),
        std::max_element(m_spectrumData.begin(), m_spectrumData.end())));
}

void FilterDisplayItem::setRfFilterResponse(const QVector<double>& magnitudesDb,
                                            double startHz, double stepHz,
                                            double referenceHz)
{
    m_rxChannel = nullptr;
    m_filterResponseBins.clear();
    m_rfFilterResponseDb = magnitudesDb;
    m_rfFilterStartHz = startHz;
    m_rfFilterStepHz = stepHz;
    m_rfFilterReferenceHz = referenceHz;
}

void FilterDisplayItem::setRfMarkers(double rxLowHz, double rxHighHz,
                                     double txLowHz, double txHighHz,
                                     const QVector<double>& notchCentresHz)
{
    if (!m_frameAvailable) { return; }
    const auto pixel = [this](double frequency) {
        const double position = (frequency - (m_frameCentreHz - m_frameSpanHz / 2.0))
            / m_frameSpanHz * m_spectrumData.size();
        return std::isfinite(position) && position >= 0.0
                && position < m_spectrumData.size()
            ? std::min(int(m_spectrumData.size()) - 1, int(std::lround(position))) : -1;
    };
    m_rxLow = pixel(rxLowHz);
    m_rxHigh = pixel(rxHighHz);
    m_txLow = pixel(txLowHz);
    m_txHigh = pixel(txHighHz);
    m_notchPositions.clear();
    for (double centre : notchCentresHz) {
        const int x = pixel(centre);
        if (x >= 0) { m_notchPositions.push_back(x); }
    }
}

void FilterDisplayItem::presentFrame(const QVector<float>& traceDbm,
                                     const QVector<float>& waterfallDbm,
                                     double centreHz, double spanHz,
                                     bool transmit, bool waterfallAdvance)
{
    if (traceDbm.isEmpty() || waterfallDbm.size() != traceDbm.size()
        || !std::isfinite(centreHz) || !std::isfinite(spanHz) || spanHz <= 0.0) {
        clearFrame();
        return;
    }
    if (m_frameCentreHz != centreHz || m_frameSpanHz != spanHz
        || m_frameTransmitting != transmit || m_waterfallImage.width() != traceDbm.size()) {
        m_waterfallImage = QImage(traceDbm.size(), 200, QImage::Format_RGB32);
        m_waterfallImage.fill(Qt::black);
        m_waterfallFrameCount = 0;
    }
    m_frameCentreHz = centreHz;
    m_frameSpanHz = spanHz;
    m_frameTransmitting = transmit;
    m_spectrumData.assign(traceDbm.cbegin(), traceDbm.cend());
    m_frameAvailable = true;
    if (!waterfallAdvance || ++m_waterfallFrameCount % std::max(1, m_waterfallFrameInterval)) {
        return;
    }
    // From Thetis MeterManager.cs:34289-34305,35690-35730 [v2.10.3.15]:
    // each new analyzer waterfall row is coloured from its own dB plane.
    for (int row = m_waterfallImage.height() - 1; row > 0; --row) {
        const uchar* source = m_waterfallImage.constScanLine(row - 1);
        uchar* destination = m_waterfallImage.scanLine(row);
        std::copy(source, source + m_waterfallImage.bytesPerLine(), destination);
    }
    QRgb* top = reinterpret_cast<QRgb*>(m_waterfallImage.scanLine(0));
    if (m_waterfallPalette == WaterfallPalette::LinAuto) {
        const auto [low, high] = std::minmax_element(waterfallDbm.cbegin(),
                                                     waterfallDbm.cend());
        // From Thetis MeterManager.cs:35041-35043 [v2.10.3.15]:
        // LINAUTO maps each new row from its minimum minus 5 dB to maximum.
        m_autoWaterfallMinDb = *low - 5.0f;
        m_autoWaterfallMaxDb = *high;
    }
    for (int i = 0; i < waterfallDbm.size(); ++i) {
        top[i] = dbToWaterfallColor(waterfallDbm[i]).rgb();
    }
}

// ---------------------------------------------------------------------------
// paint()
// Dispatch to sub-painters based on display mode.
// From Thetis clsFilterItem (MeterManager.cs:16865) — FIDisplayMode enum
// ---------------------------------------------------------------------------
void FilterDisplayItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (m_displayMode == DisplayMode::None) {
        return;
    }

    // Background
    p.fillRect(rect, m_meterBackColour);
    if (!m_frameAvailable) {
        p.setPen(m_textColour);
        p.drawText(rect, Qt::AlignCenter, QStringLiteral("Spectrum unavailable"));
        return;
    }

    if (m_displayMode == DisplayMode::Panafall) {
        // From Thetis FIDisplayMode.PANAFALL: spectrum top half, waterfall bottom
        QRect specRect(rect.left(), rect.top(), rect.width(), rect.height() / 2);
        QRect wfRect(rect.left(), rect.top() + rect.height() / 2, rect.width(), rect.height() / 2);
        paintSpectrum(p, specRect);
        paintWaterfall(p, wfRect);
    } else if (m_displayMode == DisplayMode::Panadapter) {
        // From Thetis FIDisplayMode.PANADAPTOR
        paintSpectrum(p, rect);
    } else if (m_displayMode == DisplayMode::Waterfall) {
        // From Thetis FIDisplayMode.WATERFALL
        paintWaterfall(p, rect);
    }

    paintFilterEdges(p, rect);
    paintNotches(p, rect);
}

// ---------------------------------------------------------------------------
// paintSpectrum()
// Render spectrum line (and optional fill) into the given rect.
// From Thetis clsFilterItem spectrum rendering logic (MeterManager.cs:16852+)
// ---------------------------------------------------------------------------
void FilterDisplayItem::paintSpectrum(QPainter& p, const QRect& rect)
{
    if (m_spectrumData.empty() || rect.isEmpty()) {
        return;
    }

    const int n = int(m_spectrumData.size());
    const float dbRange = m_specMaxDb - m_specMinDb;
    if (std::abs(dbRange) < 1e-6f) {
        return;
    }

    const float rectW = static_cast<float>(rect.width());
    const float rectH = static_cast<float>(rect.height());

    // Build point list: one point per bin
    QPolygonF points;
    points.reserve(n);

    for (int i = 0; i < n; ++i) {
        const float val  = m_spectrumData[static_cast<size_t>(i)];
        const float x    = static_cast<float>(rect.left()) + static_cast<float>(i) * rectW / static_cast<float>(n);
        const float frac = std::clamp((val - m_specMinDb) / dbRange, 0.0f, 1.0f);
        const float y    = static_cast<float>(rect.bottom()) - frac * rectH;
        points.append(QPointF(x, y));
    }

    p.setRenderHint(QPainter::Antialiasing, true);

    // From Thetis _fill_spec (MeterManager.cs:16936) — fill polygon under spectrum line
    if (m_fillSpectrum) {
        QPolygonF poly = points;
        poly.prepend(QPointF(static_cast<float>(rect.left()), static_cast<float>(rect.bottom())));
        poly.append(QPointF(static_cast<float>(rect.right()), static_cast<float>(rect.bottom())));

        p.setPen(Qt::NoPen);
        p.setBrush(m_dataFillColour);
        p.drawPolygon(poly);
    }

    // Draw spectrum line
    p.setPen(QPen(m_dataLineColour, 1.0f));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(points);
}

// ---------------------------------------------------------------------------
// paintWaterfall()
// Rolling waterfall: shift image down 1 row per interval, write new top row.
// From Thetis clsFilterItem _waterfall_frame_interval (MeterManager.cs:16940+)
// ---------------------------------------------------------------------------
void FilterDisplayItem::paintWaterfall(QPainter& p, const QRect& rect)
{
    if (rect.isEmpty()) {
        return;
    }

    // Draw the waterfall image scaled to the destination rect
    p.drawImage(rect, m_waterfallImage);
}

// ---------------------------------------------------------------------------
// dbToWaterfallColor()
// Map dB value to waterfall color using Enhanced palette.
// From Thetis FIWaterfallPalette.ENHANCED (MeterManager.cs:16854+)
// HSV: hue 240 (blue) at low signal → 0 (red) at high signal
// ---------------------------------------------------------------------------
QColor FilterDisplayItem::dbToWaterfallColor(float db) const
{
    const float lowDb = m_waterfallPalette == WaterfallPalette::LinAuto
        ? m_autoWaterfallMinDb : m_specMinDb;
    const float highDb = m_waterfallPalette == WaterfallPalette::LinAuto
        ? m_autoWaterfallMaxDb : m_specMaxDb;
    if (!std::isfinite(db) || highDb <= lowDb) { return Qt::black; }
    const float range = highDb - lowDb;
    const float fraction = std::clamp((db - lowDb) / range, 0.0f, 1.0f);
    // Direct MiniSpec colour rules, MeterManager.cs:34321-35100
    // [v2.10.3.15]. In particular LinLog/LinRad/LinAuto use hard 23-band
    // colours rather than the full pan's interpolated waterfall gradients.
    if (m_waterfallPalette == WaterfallPalette::Custom) {
        const QVector<QColor>& lut = m_frameTransmitting ? m_customTxGradient
                                                         : m_customRxGradient;
        return lut.size() == 101 ? lut[std::min(100, int(fraction * 100.0f))]
                                 : Qt::black;
    }
    if (m_waterfallPalette == WaterfallPalette::BlackWhite) {
        const int grey = db >= highDb ? 255 : int(fraction * 255.0f);
        return QColor(grey, grey, grey);
    }
    if (m_waterfallPalette == WaterfallPalette::Spectran) {
        if (db >= highDb) { return QColor(240, 240, 240); }
        const float percent = fraction * 100.0f;
        if (percent < 51.0f) { return QColor(0, 0, std::min(255, int(percent) * 5)); }
        const int multiplier = percent < 66.0f ? 2 : percent < 77.0f ? 3
                               : percent < 88.0f ? 4 : 5;
        const int rg = std::clamp(int(percent - 50.0f) * multiplier, 0, 255);
        return QColor(rg, rg, 255);
    }
    if (m_waterfallPalette == WaterfallPalette::Enhanced) {
        if (db <= lowDb) { return m_waterfallLowColour; }
        if (db >= highDb) { return QColor(192, 124, 255); }
        const auto blend = [](float local, int left, int right) {
            return std::clamp(int((1.0f - local) * left + local * right), 0, 255);
        };
        const float p = fraction;
        if (p < 2.0f / 9.0f) {
            const float q = p * 9.0f / 2.0f;
            return QColor(blend(q, m_waterfallLowColour.red(), 0),
                          blend(q, m_waterfallLowColour.green(), 0),
                          blend(q, m_waterfallLowColour.blue(), 255));
        }
        if (p < 3.0f / 9.0f) { return QColor(0, blend(p * 9 - 2, 0, 255), 255); }
        if (p < 4.0f / 9.0f) { return QColor(0, 255, blend(p * 9 - 3, 255, 0)); }
        if (p < 5.0f / 9.0f) { return QColor(blend(p * 9 - 4, 0, 255), 255, 0); }
        if (p < 7.0f / 9.0f) { return QColor(255, blend((p * 9 - 5) / 2, 255, 0), 0); }
        if (p < 8.0f / 9.0f) { return QColor(255, 0, blend(p * 9 - 7, 0, 255)); }
        const float q = p * 9 - 8;
        return QColor(int((1.0f - 0.25f * q) * 255), int(q * 127.5f), 255);
    }
    struct Rgb { int r; int g; int b; };
    static constexpr Rgb kBands[] = {
        {0,0,0}, {32,0,0}, {64,0,0}, {96,0,0}, {104,40,0}, {112,60,0},
        {116,88,0}, {92,112,0}, {80,132,0}, {20,140,0}, {0,160,40},
        {0,160,120}, {0,140,148}, {0,132,192}, {0,112,200}, {0,88,208},
        {0,60,232}, {0,40,252}, {80,80,252}, {124,124,252},
        {172,172,252}, {252,252,252}
    };
    if (db <= lowDb) { return Qt::black; }
    if (db >= highDb) { return QColor(252, 252, 252); }
    float mapped = fraction;
    if (m_waterfallPalette == WaterfallPalette::LinRad) {
        mapped = (db - lowDb + 2.0f) / range;
    } else if (m_waterfallPalette == WaterfallPalette::LinLog) {
        const float value = 1024.0f * (db - lowDb - 14.0f) / range;
        mapped = value > 0.0f ? std::log10(value) / std::log10(1024.0f) : -1.0f;
    }
    const int band = std::clamp(int(std::floor(mapped * 23.0f)), 0, 21);
    return QColor(kBands[band].r, kBands[band].g, kBands[band].b);
}

// ---------------------------------------------------------------------------
// paintFilterEdges()
// Draw RX and TX filter edge markers, and (when high-res mode is active) the
// computed FIR magnitude curve.
// From Thetis clsFilterItem _edges_colour_rx/_edges_colour_tx (MeterManager.cs:16951+)
// Pixel positions (0-511) are mapped to the item's pixel rect width.
// High-res path added in Task 4.4 (design Section 4D).
// ---------------------------------------------------------------------------
void FilterDisplayItem::paintFilterEdges(QPainter& p, const QRect& rect)
{
    if (rect.isEmpty()) {
        return;
    }

    // Task 4.4: render the actual FIR magnitude curve when high-res is enabled.
    // Drawn before the edge markers so the edge lines appear on top.
    if (m_highResolution) {
        paintHighResolutionFilterCurve(p, rect);
    }

    const float scale = static_cast<float>(rect.width()) /
        static_cast<float>(std::max(1, int(m_spectrumData.size())));

    auto pixelToX = [&](int pixPos) -> int {
        return rect.left() + static_cast<int>(static_cast<float>(pixPos) * scale);
    };

    p.setRenderHint(QPainter::Antialiasing, false);

    // RX filter edges — 2px solid yellow
    // From Thetis _edges_colour_rx (MeterManager.cs:16951)
    p.setPen(QPen(m_edgesColourRX, 2));
    if (m_rxLow >= 0) {
        p.drawLine(pixelToX(m_rxLow),  rect.top(), pixelToX(m_rxLow),  rect.bottom());
    }
    if (m_rxHigh >= 0) {
        p.drawLine(pixelToX(m_rxHigh), rect.top(), pixelToX(m_rxHigh), rect.bottom());
    }

    // TX filter edges — 1px dashed red (only when txLow >= 0)
    // From Thetis _edges_colour_tx (MeterManager.cs:16952)
    if (m_txLow >= 0 || m_txHigh >= 0) {
        QPen txPen(m_edgesColourTX, 1, Qt::DashLine);
        p.setPen(txPen);
        if (m_txLow >= 0) {
            p.drawLine(pixelToX(m_txLow), rect.top(), pixelToX(m_txLow), rect.bottom());
        }
        if (m_txHigh >= 0) {
            p.drawLine(pixelToX(m_txHigh), rect.top(), pixelToX(m_txHigh), rect.bottom());
        }
    }
}

// ---------------------------------------------------------------------------
// paintHighResolutionFilterCurve() — Task 4.4, design Section 4D
// Render the actual computed FIR magnitude response from
// RxChannel::filterResponseMagnitudes(nPoints) as a poly-line overlay.
//
// The magnitude vector is sampled at rect.width() points covering the
// positive half-spectrum [0 .. sampleRate/2).  Each value is in dB.
// We map the dB range [kHighResCurveDbFloor .. 0] to [rect.bottom() .. rect.top()].
// Values above 0 dB are clamped to the top; values below the floor are clamped
// to the bottom.
//
// The curve is drawn in a distinct "filter-response" cyan so it stands out from
// the ordinary spectrum trace (m_dataLineColour) without conflicting with the
// RX/TX edge markers.
// ---------------------------------------------------------------------------
void FilterDisplayItem::paintHighResolutionFilterCurve(QPainter& p, const QRect& rect)
{
    if ((!m_rxChannel && m_filterResponseBins.isEmpty()
         && m_rfFilterResponseDb.isEmpty()) || rect.isEmpty()) {
        return;
    }

    const int nPoints = rect.width();
    if (nPoints <= 0) {
        return;
    }

    // R-R3-49 (parity Task 16): the bound channel's own curve, or in a
    // remote window the Core's bins resampled the same way.
    QVector<float> mag;
    if (!m_rfFilterResponseDb.isEmpty() && m_rfFilterStepHz > 0.0
        && m_frameAvailable) {
        mag.reserve(nPoints);
        for (int x = 0; x < nPoints; ++x) {
            const double rfHz = m_frameCentreHz - m_frameSpanHz / 2.0
                + (double(x) + 0.5) / double(nPoints) * m_frameSpanHz;
            const double offsetHz = std::abs(rfHz - m_rfFilterReferenceHz);
            const double index = (offsetHz - m_rfFilterStartHz) / m_rfFilterStepHz;
            if (index < 0.0 || index >= m_rfFilterResponseDb.size() - 1) {
                mag.append(-120.0f);
            } else {
                const int lower = int(std::floor(index));
                const double alpha = index - lower;
                mag.append(float(m_rfFilterResponseDb[lower] * (1.0 - alpha)
                                 + m_rfFilterResponseDb[lower + 1] * alpha));
            }
        }
    } else {
        mag = m_rxChannel
            ? m_rxChannel->filterResponseMagnitudes(nPoints)
            : RxChannel::resampleFilterResponse(m_filterResponseBins, nPoints);
    }
    if (mag.size() != nPoints) {
        // filterResponseMagnitudes() returns empty when WDSP/FFTW3 unavailable
        // or when nPoints is invalid — silently skip.
        return;
    }

    // dB range for the high-res curve display.
    // 0 dB = passband; -80 dB covers the stopband for typical windowed-sinc FIRs.
    static constexpr float kHighResCurveDbFloor = -80.0f;
    const float dbRange = 0.0f - kHighResCurveDbFloor; // 80 dB window

    QPolygonF curve;
    curve.reserve(nPoints);

    for (int x = 0; x < nPoints; ++x) {
        const float dB      = mag[x];
        const float frac    = (dB - kHighResCurveDbFloor) / dbRange; // 0..1
        const float fracClamped = std::clamp(frac, 0.0f, 1.0f);
        // frac = 1.0 → top of rect (0 dB), frac = 0.0 → bottom (−80 dB)
        const float y = static_cast<float>(rect.bottom())
                        - fracClamped * static_cast<float>(rect.height());
        curve.append(QPointF(rect.left() + x, y));
    }

    p.setRenderHint(QPainter::Antialiasing, true);
    // Distinct cyan colour — contrasts with the yellow/red edge markers and the
    // lime-green spectrum trace.
    p.setPen(QPen(QColor(0x78, 0xc8, 0xff), 1));  // #78C8FF light-blue
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(curve);
}

// ---------------------------------------------------------------------------
// paintNotches()
// Draw notch position markers as vertical orange lines.
// From Thetis clsFilterItem _notch_colour (MeterManager.cs:16957)
// ---------------------------------------------------------------------------
void FilterDisplayItem::paintNotches(QPainter& p, const QRect& rect)
{
    if (m_notchPositions.empty() || rect.isEmpty()) {
        return;
    }

    const float scale = static_cast<float>(rect.width()) /
        static_cast<float>(std::max(1, int(m_spectrumData.size())));

    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(m_notchColour, 1));

    for (int pos : m_notchPositions) {
        const int x = rect.left() + static_cast<int>(static_cast<float>(pos) * scale);
        p.drawLine(x, rect.top(), x, rect.bottom());
    }
}

// ---------------------------------------------------------------------------
// serialize()
// Tag: FILTERDISPLAY
// Format: FILTERDISPLAY|x|y|w|h|bindingId|zOrder|displayMode|dataLineColour|
//         dataFillColour|edgesColourRX|edgesColourTX|notchColour|
//         meterBackColour|fillSpectrum|padding|specMinDb|specMaxDb|
//         waterfallPalette|waterfallFrameInterval
// ---------------------------------------------------------------------------
QString FilterDisplayItem::serialize() const
{
    return QStringLiteral("FILTERDISPLAY|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14|%15|%16|%17|%18|%19")
        .arg(static_cast<double>(m_x))
        .arg(static_cast<double>(m_y))
        .arg(static_cast<double>(m_w))
        .arg(static_cast<double>(m_h))
        .arg(m_bindingId)
        .arg(m_zOrder)
        .arg(static_cast<int>(m_displayMode))
        .arg(m_dataLineColour.name(QColor::HexArgb))
        .arg(m_dataFillColour.name(QColor::HexArgb))
        .arg(m_edgesColourRX.name(QColor::HexArgb))
        .arg(m_edgesColourTX.name(QColor::HexArgb))
        .arg(m_notchColour.name(QColor::HexArgb))
        .arg(m_meterBackColour.name(QColor::HexArgb))
        .arg(m_fillSpectrum ? 1 : 0)
        .arg(static_cast<double>(m_padding))
        .arg(static_cast<double>(m_specMinDb))
        .arg(static_cast<double>(m_specMaxDb))
        .arg(static_cast<int>(m_waterfallPalette))
        .arg(m_waterfallFrameInterval);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts (20 total):
// [0]=FILTERDISPLAY [1]=x [2]=y [3]=w [4]=h [5]=bindingId [6]=zOrder
// [7]=displayMode [8]=dataLineColour [9]=dataFillColour
// [10]=edgesColourRX [11]=edgesColourTX [12]=notchColour [13]=meterBackColour
// [14]=fillSpectrum [15]=padding [16]=specMinDb [17]=specMaxDb
// [18]=waterfallPalette [19]=waterfallFrameInterval
// ---------------------------------------------------------------------------
bool FilterDisplayItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 20 || parts[0] != QLatin1String("FILTERDISPLAY")) {
        return false;
    }

    bool ok = true;

    const float x = parts[1].toFloat(&ok);  if (!ok) { return false; }
    const float y = parts[2].toFloat(&ok);  if (!ok) { return false; }
    const float w = parts[3].toFloat(&ok);  if (!ok) { return false; }
    const float h = parts[4].toFloat(&ok);  if (!ok) { return false; }

    const int bindingId = parts[5].toInt(&ok);   if (!ok) { return false; }
    const int zOrder    = parts[6].toInt(&ok);   if (!ok) { return false; }
    const int dispMode  = parts[7].toInt(&ok);   if (!ok) { return false; }

    const QColor dataLineColour(parts[8]);    if (!dataLineColour.isValid())    { return false; }
    const QColor dataFillColour(parts[9]);    if (!dataFillColour.isValid())    { return false; }
    const QColor edgesColourRX(parts[10]);    if (!edgesColourRX.isValid())     { return false; }
    const QColor edgesColourTX(parts[11]);    if (!edgesColourTX.isValid())     { return false; }
    const QColor notchColour(parts[12]);      if (!notchColour.isValid())       { return false; }
    const QColor meterBackColour(parts[13]);  if (!meterBackColour.isValid())   { return false; }

    const int fillSpectrum         = parts[14].toInt(&ok);   if (!ok) { return false; }
    const float padding            = parts[15].toFloat(&ok); if (!ok) { return false; }
    const float specMinDb          = parts[16].toFloat(&ok); if (!ok) { return false; }
    const float specMaxDb          = parts[17].toFloat(&ok); if (!ok) { return false; }
    const int waterfallPalette     = parts[18].toInt(&ok);   if (!ok) { return false; }
    const int waterfallFrameInterval = parts[19].toInt(&ok); if (!ok) { return false; }

    setRect(x, y, w, h);
    setBindingId(bindingId);
    setZOrder(zOrder);

    m_displayMode       = static_cast<DisplayMode>(dispMode);
    m_dataLineColour    = dataLineColour;
    m_dataFillColour    = dataFillColour;
    m_edgesColourRX     = edgesColourRX;
    m_edgesColourTX     = edgesColourTX;
    m_notchColour       = notchColour;
    m_meterBackColour   = meterBackColour;
    m_fillSpectrum      = (fillSpectrum != 0);
    m_padding           = padding;
    m_specMinDb         = specMinDb;
    m_specMaxDb         = specMaxDb;
    m_waterfallPalette  = static_cast<WaterfallPalette>(waterfallFrameInterval < 0 ? 0 : waterfallPalette);
    m_waterfallFrameInterval = (waterfallFrameInterval > 0) ? waterfallFrameInterval : 4;

    return true;
}

} // namespace NereusSDR
