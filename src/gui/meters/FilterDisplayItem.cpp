// =================================================================
// src/gui/meters/FilterDisplayItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Contrast round: with the workspace look the item's own
//                 background follows the meter background at its default.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Colours at their defaults follow the workspace look when
//                 drawn (ItemColourLook.h); saved colours are unchanged.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Fix wave round 2 (I7): a saved display in the old look is
//                 hit where that look draws: legacyLayout maps the frame's
//                 span across the full rect, edges and notches are lines,
//                 and the buttons, click-to-step areas, top line and wheel
//                 off an edge do nothing. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-26 - R-R3-49 (remote-window parity Task 16): the
//                 high-resolution curve can come from a Core's bins
//                 (setFilterResponseBins) in a remote window, resampled as
//                 the local channel's is. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-25 - Saves and restores the text colour after the older fields,
//                 so they survive a reload. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - Thetis filter display settings and colours appended to
//                 the record (older records load with the constructor
//                 values); applyNewItemDefaults copies
//                 kFilterDisplayThetisDefaults. Fixes the palette load: a
//                 saved palette loads as itself whatever the frame
//                 interval, and one outside WaterfallPalette loads as
//                 Enhanced. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-26 - Thetis filter display: renderFilterDisplay (paintThetis),
//                 the MouseUp/MouseWheel handlers and the Low, High, Shift
//                 and AdjustNotch setters, ported as requests the item
//                 emits; the MNF, +MNF, SNAP and FIT buttons; the Thetis
//                 look saved as field [43]. FilterDisplayRequests applies
//                 the requests for MainWindow. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Final review fix wave (C2, I7): TX state from the
//                 display's own slice (txActive), not the window's MOX;
//                 input no longer needs the Thetis look, so saved items
//                 keep their look and take drags, notches, wheel and
//                 buttons. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
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
#include "ItemColourLook.h"

// From Thetis clsFilterItem (MeterManager.cs:16852+)

#include "core/RxChannel.h"
#include "core/spectrum/WaterfallPalettes.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QAction>
#include <QFontMetricsF>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QPolygonF>
#include <QStringList>
#include <algorithm>
#include <iterator>
#include <cmath>
#include <limits>

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
// applyNewItemDefaults()
// New items only. From Thetis MeterManager.cs:27651-27705 [v2.10.3.15]
// (see kFilterDisplayThetisDefaults).
// ---------------------------------------------------------------------------
void FilterDisplayItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const FilterDisplayItemThetisDefaults& d = kFilterDisplayThetisDefaults;
    m_padding = d.verticalRatio;
    m_showFilterLimits = d.showFilterLimits;
    m_fixedRxZoom = d.fixedRxZoom;
    m_fixedTxZoom = d.fixedTxZoom;
    m_rxZoom = d.rxZoom;
    m_txZoom = d.txZoom;
    m_sidebandsScale = d.sidebandsScale;
    m_cwScale = d.cwScale;
    m_othersScale = d.othersScale;
    setDisplayMode(d.displayMode);
    m_fontScale = d.fontScale;
    m_dataLineColour = QColor::fromRgb(d.dataLineColour);
    m_waterfallLowColour = QColor::fromRgb(d.waterfallLowColour);
    m_textColour = QColor::fromRgb(d.textColour);
    m_numberHighlightColour = QColor::fromRgb(d.numberHighlightColour);
    m_edgesColourRX = QColor::fromRgb(d.edgesColourRx);
    m_edgesColourTX = QColor::fromRgb(d.edgesColourTx);
    m_edgeHighlightColour = QColor::fromRgb(d.edgeHighlightColour);
    m_meterBackColour = QColor::fromRgb(d.meterBackColour);
    m_notchColour = QColor::fromRgb(d.notchColour);
    m_notchHighlightColour = QColor::fromRgb(d.notchHighlightColour);
    m_extentsColour = QColor::fromRgb(d.extentsColour);
    m_sidebandMode = d.sidebandMode;
    setWaterfallFrameInterval(d.frameInterval);
    m_greyWhenNotActive = d.greyWhenNotActive;
    m_snapLines = d.snapLines;
    m_autoZoom = d.autoZoom;
    m_snapLineColour = QColor::fromRgb(d.snapLineColour);
    m_settingOnColour = QColor::fromRgb(d.settingOnColour);
    m_buttonHighlightColour = QColor::fromRgb(d.buttonHighlightColour);
    m_showCharacteristic = d.showCharacteristic;
    m_characteristicLow = d.characteristicLow;
    // New items take the Thetis render and input.
    m_thetisLook = true;
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
    // No frame, no slice to act on: the edges and notches are stale.
    m_state = SliceFilterState{};
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
    if (m_thetisLook) {
        paintThetis(p, widgetW, widgetH);
        return;
    }
    const QRect rect = pixelRect(widgetW, widgetH);
    if (m_displayMode == DisplayMode::None) {
        return;
    }

    // Background
    p.fillRect(rect, ItemColourLook::effective("FilterDisplay.meterBackColour", m_meterBackColour, this));
    if (!m_frameAvailable) {
        p.setPen(ItemColourLook::effective("FilterDisplay.textColour", m_textColour, this));
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
        p.setBrush(ItemColourLook::effective("FilterDisplay.dataFillColour", m_dataFillColour, this));
        p.drawPolygon(poly);
    }

    // Draw spectrum line
    p.setPen(QPen(ItemColourLook::effective("FilterDisplay.dataLineColour", m_dataLineColour, this), 1.0f));
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
    p.setPen(QPen(ItemColourLook::effective("FilterDisplay.edgesColourRX", m_edgesColourRX, this), 2));
    if (m_rxLow >= 0) {
        p.drawLine(pixelToX(m_rxLow),  rect.top(), pixelToX(m_rxLow),  rect.bottom());
    }
    if (m_rxHigh >= 0) {
        p.drawLine(pixelToX(m_rxHigh), rect.top(), pixelToX(m_rxHigh), rect.bottom());
    }

    // TX filter edges — 1px dashed red (only when txLow >= 0)
    // From Thetis _edges_colour_tx (MeterManager.cs:16952)
    if (m_txLow >= 0 || m_txHigh >= 0) {
        QPen txPen(ItemColourLook::effective("FilterDisplay.edgesColourTX", m_edgesColourTX, this), 1, Qt::DashLine);
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
    p.setPen(QPen(ItemColourLook::effective("FilterDisplay.notchColour", m_notchColour, this), 1));

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
//         waterfallPalette|waterfallFrameInterval|textColour|
//         showFilterLimits|fixedRxZoom|fixedTxZoom|rxZoom|txZoom|
//         sidebandsScale|cwScale|othersScale|fontScale|sidebandMode|
//         greyWhenNotActive|snapLines|autoZoom|showCharacteristic|
//         characteristicLow|numberHighlightColour|edgeHighlightColour|
//         extentsColour|notchHighlightColour|snapLineColour|settingOnColour|
//         buttonHighlightColour|thetisLook
// textColour and the Thetis settings follow the older fields, so a record
// saved before they existed still loads (with the constructor values).
// padding is the Thetis vertical ratio.
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
        .arg(m_waterfallFrameInterval)
        + QLatin1Char('|') + m_textColour.name(QColor::HexArgb)
        + QStringLiteral("|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14|%15")
              .arg(m_showFilterLimits ? 1 : 0)
              .arg(m_fixedRxZoom ? 1 : 0)
              .arg(m_fixedTxZoom ? 1 : 0)
              .arg(static_cast<double>(m_rxZoom))
              .arg(static_cast<double>(m_txZoom))
              .arg(static_cast<double>(m_sidebandsScale))
              .arg(static_cast<double>(m_cwScale))
              .arg(static_cast<double>(m_othersScale))
              .arg(static_cast<double>(m_fontScale))
              .arg(m_sidebandMode ? 1 : 0)
              .arg(m_greyWhenNotActive ? 1 : 0)
              .arg(m_snapLines ? 1 : 0)
              .arg(m_autoZoom ? 1 : 0)
              .arg(m_showCharacteristic ? 1 : 0)
              .arg(static_cast<double>(m_characteristicLow))
        + QStringLiteral("|%1|%2|%3|%4|%5|%6|%7")
              .arg(m_numberHighlightColour.name(QColor::HexArgb))
              .arg(m_edgeHighlightColour.name(QColor::HexArgb))
              .arg(m_extentsColour.name(QColor::HexArgb))
              .arg(m_notchHighlightColour.name(QColor::HexArgb))
              .arg(m_snapLineColour.name(QColor::HexArgb))
              .arg(m_settingOnColour.name(QColor::HexArgb))
              .arg(m_buttonHighlightColour.name(QColor::HexArgb))
        + QStringLiteral("|%1").arg(m_thetisLook ? 1 : 0);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts (20 total):
// [0]=FILTERDISPLAY [1]=x [2]=y [3]=w [4]=h [5]=bindingId [6]=zOrder
// [7]=displayMode [8]=dataLineColour [9]=dataFillColour
// [10]=edgesColourRX [11]=edgesColourTX [12]=notchColour [13]=meterBackColour
// [14]=fillSpectrum [15]=padding [16]=specMinDb [17]=specMaxDb
// [18]=waterfallPalette [19]=waterfallFrameInterval
// Optional (absent from older records): [20]=textColour
// [21]=showFilterLimits [22]=fixedRxZoom [23]=fixedTxZoom [24]=rxZoom
// [25]=txZoom [26]=sidebandsScale [27]=cwScale [28]=othersScale
// [29]=fontScale [30]=sidebandMode [31]=greyWhenNotActive [32]=snapLines
// [33]=autoZoom [34]=showCharacteristic [35]=characteristicLow
// [36]=numberHighlightColour [37]=edgeHighlightColour [38]=extentsColour
// [39]=notchHighlightColour [40]=snapLineColour [41]=settingOnColour
// [42]=buttonHighlightColour [43]=thetisLook
// A missing or unreadable optional field keeps its current value; numbers
// are clamped to the ranges the Thetis Apply uses (MeterManager.cs:27651-27705
// [v2.10.3.15]).
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
    // The palette loads as itself when it names a WaterfallPalette, else
    // Enhanced. It used to test the frame interval here, so a negative
    // interval reset a valid palette and a bad palette went unchecked.
    const bool paletteKnown = waterfallPalette >= static_cast<int>(WaterfallPalette::Enhanced)
        && waterfallPalette <= static_cast<int>(WaterfallPalette::Custom);
    m_waterfallPalette  = paletteKnown ? static_cast<WaterfallPalette>(waterfallPalette)
                                       : WaterfallPalette::Enhanced;
    m_waterfallFrameInterval = (waterfallFrameInterval > 0) ? waterfallFrameInterval : 4;

    if (parts.size() > 20) {
        const QColor textColour(parts[20]);
        if (textColour.isValid()) { m_textColour = textColour; }
    }

    const auto readBool = [&parts](int index, bool& out) {
        if (index >= parts.size()) { return; }
        bool okField = false;
        const int v = parts[index].toInt(&okField);
        if (okField) { out = (v != 0); }
    };
    const auto readFloat = [&parts](int index, float lo, float hi, float& out) {
        if (index >= parts.size()) { return; }
        bool okField = false;
        const float v = parts[index].toFloat(&okField);
        if (okField && std::isfinite(v)) { out = std::clamp(v, lo, hi); }
    };
    const auto readColour = [&parts](int index, QColor& out) {
        if (index >= parts.size()) { return; }
        const QColor c(parts[index]);
        if (c.isValid()) { out = c; }
    };
    readBool(21, m_showFilterLimits);
    readBool(22, m_fixedRxZoom);
    readBool(23, m_fixedTxZoom);
    readFloat(24, kRxZoomMin, kRxZoomMax, m_rxZoom);
    readFloat(25, kTxZoomMin, kTxZoomMax, m_txZoom);
    readFloat(26, kScaleMin, kScaleMax, m_sidebandsScale);
    readFloat(27, kScaleMin, kScaleMax, m_cwScale);
    readFloat(28, kScaleMin, kScaleMax, m_othersScale);
    readFloat(29, kFontScaleMin, kFontScaleMax, m_fontScale);
    readBool(30, m_sidebandMode);
    readBool(31, m_greyWhenNotActive);
    readBool(32, m_snapLines);
    readBool(33, m_autoZoom);
    readBool(34, m_showCharacteristic);
    readFloat(35, kCharacteristicLowMin, kCharacteristicLowMax, m_characteristicLow);
    readColour(36, m_numberHighlightColour);
    readColour(37, m_edgeHighlightColour);
    readColour(38, m_extentsColour);
    readColour(39, m_notchHighlightColour);
    readColour(40, m_snapLineColour);
    readColour(41, m_settingOnColour);
    readColour(42, m_buttonHighlightColour);
    // A record saved before the Thetis look existed paints as it did.
    m_thetisLook = false;
    readBool(43, m_thetisLook);

    return true;
}


// ===========================================================================
// Thetis look: renderFilterDisplay and the clsFilterItem input handlers.
// From Thetis MeterManager.cs:35466-36313 (render, hzToPixels, pixelsToHz,
// isMouseNearLine, pointToSegmentDistance) and :18858-19412 (MouseUp,
// MouseWheel, Low, High, AdjustNotch, Shift) [v2.10.3.15].
// ===========================================================================

namespace {

// From Thetis MeterManager.cs:18323 [v2.10.3.15]: clsFilterItem _colour,
// the item background and the text overlay colour (text_overlay_colour =
// filter.Colour; // same as the background). NereusSDR items have no
// item-group colour, so the constructor value is used.
const QColor kThetisItemBackground(32, 32, 32);

// From Thetis MeterManager.cs:662-712 [v2.10.3.15] (FilterItemFrequencies):
// OTHER keeps -v and v; SIDEBANDS and CW also keep 0; the list is sorted.
QVector<float> buildSnapList(std::initializer_list<float> values, bool withZero)
{
    QVector<float> out;
    for (float v : values) {
        v = std::abs(v);
        if (withZero && !out.contains(0.0f)) { out.append(0.0f); }
        if (!out.contains(-v)) { out.append(-v); }
        if (!out.contains(v)) { out.append(v); }
    }
    std::sort(out.begin(), out.end());
    return out;
}

float pointToSegmentDistance(const QPointF& lineStart, const QPointF& lineEnd,
                             const QPointF& point)
{
    // From Thetis MeterManager.cs:36288-36308 [v2.10.3.15].
    const double dx = lineEnd.x() - lineStart.x();
    const double dy = lineEnd.y() - lineStart.y();
    const double lineLengthSquared = dx * dx + dy * dy;
    if (lineLengthSquared == 0.0) {
        return float(std::hypot(point.x() - lineStart.x(), point.y() - lineStart.y()));
    }
    double t = ((point.x() - lineStart.x()) * dx + (point.y() - lineStart.y()) * dy)
        / lineLengthSquared;
    t = std::max(0.0, std::min(1.0, t));
    const QPointF closest(lineStart.x() + t * dx, lineStart.y() + t * dy);
    return float(std::hypot(closest.x() - point.x(), closest.y() - point.y()));
}

bool isMouseNearLine(const QPointF& a, const QPointF& b, const QPointF& mouse,
                     float threshold)
{
    // From Thetis MeterManager.cs:36282-36286 [v2.10.3.15].
    return pointToSegmentDistance(a, b, mouse) <= threshold;
}

float distanceBetweenPoints(const QPointF& a, const QPointF& b)
{
    return float(std::hypot(a.x() - b.x(), a.y() - b.y()));
}

float roundTo10(float v)
{
    // Thetis (float)(Math.Round(v / 10) * 10). Math.Round is banker's
    // rounding; std::nearbyint uses the default round-half-to-even mode.
    return float(std::nearbyint(double(v) / 10.0) * 10.0);
}

QString hzText(float hz)
{
    return QString::number(double(hz), 'g', 10);
}

QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(alpha);
    return c;
}

QColor greyOf(const QColor& c)
{
    // From Thetis MeterManager.cs:35523-35526 [v2.10.3.15].
    const int grey = int(c.red() * 0.3 + c.green() * 0.59 + c.blue() * 0.11);
    return QColor(grey, grey, grey, c.alpha());
}

QColor blend(const QColor& base, const QColor& grey, float attenuation)
{
    // From Thetis MeterManager.cs:35653-35657, 35674-35678 [v2.10.3.15].
    return QColor(int((1 - attenuation) * base.red() + attenuation * grey.red()),
                  int((1 - attenuation) * base.green() + attenuation * grey.green()),
                  int((1 - attenuation) * base.blue() + attenuation * grey.blue()),
                  base.alpha());
}

} // namespace

const QVector<float>& FilterDisplayItem::snapFrequencies(DSPMode mode)
{
    // From Thetis setup.designer.cs:12405, 12414, 12423 [v2.10.3.15].
    static const QVector<float> kOther = buildSnapList({1000, 2500, 5000, 6000, 8000}, false);
    static const QVector<float> kCw = buildSnapList({50, 100, 250, 500, 700, 750, 1000}, true);
    static const QVector<float> kSidebands =
        buildSnapList({50, 100, 1000, 1500, 2800, 3000, 4000, 5000}, true);
    static const QVector<float> kNone;
    // From Thetis MeterManager.cs:18562-18585 [v2.10.3.15] (VGridFrequencies).
    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::DIGL:
    case DSPMode::USB:
    case DSPMode::DIGU:
        return kSidebands;
    case DSPMode::CWL:
    case DSPMode::CWU:
        return kCw;
    case DSPMode::SAM:
    case DSPMode::DSB:
    case DSPMode::AM:
        return kOther;
    default:
        return kNone;
    }
}

bool FilterDisplayItem::interactive() const
{
    // Input does not depend on the look: a saved item that paints the old
    // look still takes drags, notches, the wheel and the buttons.
    return m_requestsEnabled && m_state.sliceIndex >= 0;
}

bool FilterDisplayItem::txActive() const
{
    // From Thetis MeterManager.cs:5264 [v2.10.3.15] (OnMox):
    //   m.MOX = rx == m.RX && newMox;
    // MOX reaches a meter only on its own receiver. Here: MOX on, and this
    // display's slice is the transmit slice.
    if (m_state.sliceIndex < 0) { return transmitting(); }
    return transmitting() && m_state.transmitSlice;
}

int FilterDisplayItem::sidebandModeSign() const
{
    // From Thetis MeterManager.cs:20015-20035 [v2.10.3.15].
    if (!m_sidebandMode) { return 0; }
    return modeSign();
}

int FilterDisplayItem::modeSign() const
{
    // From Thetis MeterManager.cs:20045-20062 [v2.10.3.15].
    switch (m_state.mode) {
    case DSPMode::CWL:
    case DSPMode::LSB:
    case DSPMode::DIGL:
        return -1;
    case DSPMode::CWU:
    case DSPMode::USB:
    case DSPMode::DIGU:
        return 1;
    default:
        return 0;
    }
}

bool FilterDisplayItem::isCw() const
{
    // From Thetis MeterManager.cs:20037-20044 [v2.10.3.15].
    return m_state.mode == DSPMode::CWL || m_state.mode == DSPMode::CWU;
}

bool FilterDisplayItem::twoSidebands() const
{
    // From Thetis console.cs:33344 [v2.10.3.15] (CurrentDSPhasTwoSidebands).
    const DSPMode m = m_state.mode;
    return m == DSPMode::AM || m == DSPMode::DSB || m == DSPMode::FM
        || m == DSPMode::SAM || m == DSPMode::SPEC || m == DSPMode::DRM;
}

bool FilterDisplayItem::filterEnabled() const
{
    // From Thetis MeterManager.cs:18764-18791 [v2.10.3.15] (RXEnabled,
    // TXEnabled).
    if (txActive()) {
        return !(m_state.mode == DSPMode::CWL || m_state.mode == DSPMode::CWU
                 || m_state.mode == DSPMode::DRM);
    }
    return m_state.mode != DSPMode::DRM;
}

bool FilterDisplayItem::canAdjust() const
{
    // From Thetis MeterManager.cs:18738-18763 [v2.10.3.15] (CanAdjust).
    return m_state.mode != DSPMode::FM && m_state.mode != DSPMode::SPEC
        && filterEnabled();
}

int FilterDisplayItem::extentHz() const
{
    // Thetis ExtentHZ (MeterManager.cs:19451-19478 [v2.10.3.15]): the RX
    // extent is FilterMaxWidth, the TX extent TX_BANDWIDTH. A delivered
    // frame carries its own span (twice the extent, MainWindow
    // kMiniRxSpanHz), so that wins when there is one.
    if (m_frameAvailable && m_frameSpanHz > 0.0) {
        return std::max(1, int(std::lround(m_frameSpanHz / 2.0)));
    }
    return txActive() ? kTxExtentHz : kRxExtentHz;
}

float FilterDisplayItem::displayLowHz() const
{
    // From Thetis MeterManager.cs:19151-19178 [v2.10.3.15] (Low getter).
    if (txActive()) {
        switch (m_state.mode) {
        case DSPMode::AM:
        case DSPMode::SAM:
        case DSPMode::FM:
        case DSPMode::DSB:
        case DSPMode::DRM:
        case DSPMode::SPEC:
        //case DSPMode.LSB:
        //case DSPMode.DIGL:
        // [original inline comment from MeterManager.cs:19167-19168]
        // NereusSDR differs: Thetis leaves LSB and DIGL on the default
        // (_tx_low), which draws the TX passband on the upper side while
        // its Low setter writes TXFilterHigh for them. The lower sideband
        // is drawn where it is, -TXFilterHigh..-TXFilterLow.
        case DSPMode::LSB:
        case DSPMode::DIGL:
            return float(-m_state.txHighHz);
        default:
            return float(m_state.txLowHz);
        }
    }
    return float(m_state.rxLowHz);
}

float FilterDisplayItem::displayHighHz() const
{
    // From Thetis MeterManager.cs:19257-19278 [v2.10.3.15] (High getter).
    if (txActive()) {
        switch (m_state.mode) {
        //case DSPMode.LSB:
        //case DSPMode.DIGL:
        //    return -_tx_low;
        // [original inline comment from MeterManager.cs:19267-19269]
        // NereusSDR differs: the commented case is restored so the lower
        // sideband's top edge is -TXFilterLow (see displayLowHz).
        case DSPMode::LSB:
        case DSPMode::DIGL:
            return float(-m_state.txLowHz);
        default:
            return float(m_state.txHighHz);
        }
    }
    return float(m_state.rxHighHz);
}

float FilterDisplayItem::hzToPixels(double hz, float pixelSpan, int hzSpan) const
{
    // From Thetis MeterManager.cs:36272-36276 [v2.10.3.15].
    if (hz <= 0 || pixelSpan <= 0 || hzSpan <= 0) { return 0; }
    return float((pixelSpan / float(hzSpan)) * hz);
}

double FilterDisplayItem::pixelsToHz(float pixels, float pixelSpan, int hzSpan) const
{
    // From Thetis MeterManager.cs:36277-36281 [v2.10.3.15].
    if (pixelSpan <= 0 || hzSpan <= 0) { return 0; }
    return (double(pixels) / double(pixelSpan)) * hzSpan;
}

int FilterDisplayItem::findNearestVGrid(int hz) const
{
    // From Thetis MeterManager.cs:18586-18605 [v2.10.3.15].
    const QVector<float>& frequencies = snapFrequencies(m_state.mode);
    if (frequencies.isEmpty()) { return hz; }
    float nearestFrequency = frequencies[0];
    float smallestDifference = std::abs(float(hz) - nearestFrequency);
    for (float frequency : frequencies) {
        if (sidebandModeSign() < 0 && frequency > 0) { continue; }
        if (sidebandModeSign() > 0 && frequency < 0) { continue; }
        const float difference = std::abs(float(hz) - frequency);
        if (difference < smallestDifference) {
            smallestDifference = difference;
            nearestFrequency = frequency;
        }
    }
    return int(nearestFrequency);
}

const FilterDisplayItem::NotchMark* FilterDisplayItem::notchById(int id) const
{
    for (const NotchMark& n : m_state.notches) {
        if (n.id == id) { return &n; }
    }
    return nullptr;
}

void FilterDisplayItem::clearSelection()
{
    m_lowSelected = false;
    m_highSelected = false;
    m_topSelected = false;
    m_selectedNotchId = -1;
    m_notchStartHz = -1.0;
}

FilterDisplayItem::ThetisLayout FilterDisplayItem::legacyLayout(int widgetW, int widgetH) const
{
    // The old look (paint() without m_thetisLook) draws the frame's span
    // across the full rect (setRfMarkers, paintFilterEdges, paintNotches):
    // no extent text margins, no slopes, no zoom and no controls. Input on
    // a saved display hits what that look draws: the edges and notches,
    // full height, nothing else (no hidden touch surfaces).
    ThetisLayout l;
    const QRect r = pixelRect(widgetW, widgetH);
    if (r.width() <= 0 || r.height() <= 0 || !m_frameAvailable || m_frameSpanHz <= 0.0
        || m_displayMode == DisplayMode::None) {
        return l;
    }
    l.itemRect = QRectF(r);
    l.x = float(r.left());
    l.y = float(r.top());
    l.w = float(r.width());
    l.h = float(r.height());
    l.zoom = 1.0f;
    l.hzSpan = std::max(1, int(std::lround(m_frameSpanHz)));
    l.extentHz = std::max(1, l.hzSpan / 2);
    l.pixelSpan = l.w;
    l.extentL = l.x;
    l.extentH = l.x + l.w;
    // The slice's frequency where the frame draws it.
    const double frameLowHz = m_frameCentreHz - m_frameSpanHz / 2.0;
    l.centre = l.x + float((m_state.vfoHz - frameLowHz) / m_frameSpanHz) * l.w;
    l.lowHz = displayLowHz();
    l.highHz = displayHighHz();
    l.filterL = l.centre + hzToPixels(std::abs(l.lowHz), l.pixelSpan, l.hzSpan) * (l.lowHz < 0 ? -1.0f : 1.0f);
    l.filterH = l.centre + hzToPixels(std::abs(l.highHz), l.pixelSpan, l.hzSpan) * (l.highHz < 0 ? -1.0f : 1.0f);
    l.lowBot = QPointF(l.filterL, l.y + l.h);
    l.lowTop = QPointF(l.filterL, l.y);
    l.highBot = QPointF(l.filterH, l.y + l.h);
    l.highTop = QPointF(l.filterH, l.y);
    l.topLeft = l.lowTop;
    l.topRight = l.highTop;
    l.textY = l.y + l.h;
    l.topLineY = l.topPos = l.y;
    l.botLineY = l.botPos = l.y + l.h;
    l.spectrumHeight = l.h;
    // mnfRect, mnfPlusRect, snapRect and fitRect stay empty: not drawn.
    l.valid = true;
    return l;
}

FilterDisplayItem::ThetisLayout FilterDisplayItem::thetisLayout(int widgetW, int widgetH) const
{
    if (!m_thetisLook) { return legacyLayout(widgetW, widgetH); }
    // From Thetis MeterManager.cs:35470-35612, 35771-35776, 35834-35866,
    // 35873-35884 [v2.10.3.15].
    ThetisLayout l;
    const QRect r = pixelRect(widgetW, widgetH);
    if (r.width() <= 0 || r.height() <= 0 || widgetW <= 0) { return l; }
    l.itemRect = QRectF(r);
    l.x = float(r.left());
    l.y = float(r.top());
    l.w = float(r.width());
    l.h = float(r.height());
    const float originalW = l.w;

    l.sidebandSign = sidebandModeSign();
    switch (l.sidebandSign) {
    case -1:
        l.w *= kSidebandWidthFactor;
        break;
    case 0:
        break;
    case 1: {
        l.w *= kSidebandWidthFactor;
        const float expandedBy = l.w - originalW;
        l.x -= expandedBy;
        break;
    }
    default:
        break;
    }

    const bool tx = txActive();
    const float modeZoom = (m_state.mode == DSPMode::LSB || m_state.mode == DSPMode::USB
                            || m_state.mode == DSPMode::DIGL || m_state.mode == DSPMode::DIGU)
        ? m_sidebandsScale : (isCw() ? m_cwScale : m_othersScale);
    l.zoom = 1.0f;
    if (tx) {
        if (!m_autoZoom && m_fixedTxZoom) { l.zoom = m_txZoom + modeZoom; }
    } else {
        if (!m_autoZoom && m_fixedRxZoom) { l.zoom = m_rxZoom + modeZoom; }
    }
    const double minNotchWidth = m_state.minNotchWidthHz / 2.0;

    //calc pixels per hz etc
    l.extentHz = extentHz();
    l.hzSpan = l.extentHz * 2;
    const float fontSizeScaled = kBaseFontSize * m_fontScale;
    const float fontSizeEmScaled = (fontSizeScaled / 16.0f) * (float(widgetW) / 52.0f);
    l.font = QFont(QStringLiteral("Trebuchet MS"));
    // DirectWrite sizes are device-independent pixels.
    l.font.setPixelSize(std::max(1, int(std::lround(fontSizeEmScaled))));
    const QFontMetricsF fm(l.font);
    const QString lowExtentText = QString::number(-l.extentHz);
    const QString highExtentText = QStringLiteral("+") + QString::number(l.extentHz);
    l.textHeight = float(fm.height());
    l.maxExtentTextWidth = float(std::max(fm.horizontalAdvance(lowExtentText),
                                          fm.horizontalAdvance(highExtentText)));
    float pixelSpan = l.w - (l.maxExtentTextWidth * 2.0f);
    l.textY = l.y + l.h - l.textHeight;

    l.lowHz = displayLowHz();
    l.highHz = displayHighHz();

    // autozoom
    if (m_autoZoom) {
        const double hzSpanForWidth = pixelsToHz(l.w, pixelSpan, l.hzSpan);
        float sbw;
        if (modeSign() == -1) {
            sbw = std::abs(l.lowHz);
            if (isCw()) { sbw += std::abs(l.highHz); }
        } else {
            sbw = l.highHz;
            if (isCw()) { sbw += l.lowHz; }
        }
        sbw *= 2.0f;
        float zoom = sbw != 0.0f ? float(1.0 / (sbw / hzSpanForWidth)) : 1.0f;
        zoom *= l.sidebandSign == 0 ? kAutoZoomTwoSidebandMargin : kAutoZoomSidebandMargin;
        zoom = std::max(1.0f, zoom);
        zoom = std::min(zoom, kAutoZoomMax);
        l.zoom = zoom;
    }

    //lines
    l.lineWidth = std::max(2.0f, l.w * 0.001f);
    const float lineWidthHalf = l.lineWidth / 2.0f;

    //slope extent edges
    l.slopePixels = hzToPixels(minNotchWidth, pixelSpan, l.hzSpan);
    const float hzZoomDiff = (float(l.extentHz) * l.zoom) - float(l.extentHz);
    l.zoomDiffPixels = hzToPixels(std::abs(hzZoomDiff), pixelSpan, l.hzSpan)
        * (hzZoomDiff < 0 ? -1.0f : 1.0f);

    //top/bottom lines
    l.topLineY = l.y + lineWidthHalf + l.textHeight;
    l.botLineY = l.y + l.h - lineWidthHalf - l.textHeight;

    //slope extents
    l.extentL = l.x + l.maxExtentTextWidth + l.slopePixels - l.zoomDiffPixels;
    l.extentH = l.x + l.w - l.maxExtentTextWidth - l.slopePixels + l.zoomDiffPixels;
    l.spectrumHeight = l.h - ((lineWidthHalf + l.textHeight) * 2.0f); // between top/bottom lines
    l.topPos = l.y + lineWidthHalf + l.textHeight;
    l.botPos = l.y + l.h - lineWidthHalf - l.textHeight;
    if (m_displayMode == DisplayMode::Panafall) { l.spectrumHeight /= 2.0f; } // half display, spectrum at the top, lower will be waterfall

    //centre line
    l.centre = l.x + (l.w / 2.0f);
    //adjust pixel span between the two extents as that is the filter width, the slopes are extra
    l.pixelSpan = l.extentH - l.extentL;

    //mnf/mnf+ button hover
    l.zeroWidth = float(fm.horizontalAdvance(QStringLiteral("0")));
    l.zeroHeight = float(fm.height());
    const QSizeF mnf(fm.horizontalAdvance(QLatin1String(kMnfLabel)), fm.height());
    const QSizeF mnfPlus(fm.horizontalAdvance(QLatin1String(kMnfPlusLabel)), fm.height());
    const QSizeF snap(fm.horizontalAdvance(QLatin1String(kSnapLabel)), fm.height());
    const QSizeF zoomSize(fm.horizontalAdvance(QLatin1String(kFitLabel)), fm.height());
    const float zero = l.zeroWidth;
    switch (l.sidebandSign) {
    case -1:
        l.mnfRect = QRectF(l.centre - (zero * 3.0f) - mnf.width(), l.textY, mnf.width(), mnf.height());
        l.mnfPlusRect = QRectF(l.mnfRect.x() - (zero * 2.0f) - mnfPlus.width(), l.textY, mnfPlus.width(), mnfPlus.height());
        l.snapRect = QRectF(l.mnfPlusRect.x() - (zero * 2.0f) - snap.width(), l.textY, snap.width(), snap.height());
        l.fitRect = QRectF(l.snapRect.x() - (zero * 2.0f) - zoomSize.width(), l.textY, zoomSize.width(), zoomSize.height());
        break;
    case 1:
        l.mnfRect = QRectF(l.centre + (zero * 3.0f), l.textY, mnf.width(), mnf.height());
        l.mnfPlusRect = QRectF(l.mnfRect.x() + (zero * 2.0f) + mnf.width(), l.textY, mnfPlus.width(), mnfPlus.height());
        l.snapRect = QRectF(l.mnfPlusRect.x() + (zero * 2.0f) + l.mnfPlusRect.width(), l.textY, snap.width(), snap.height());
        l.fitRect = QRectF(l.snapRect.x() + (zero * 2.0f) + l.snapRect.width(), l.textY, zoomSize.width(), zoomSize.height());
        break;
    default:
        l.mnfRect = QRectF(l.centre - (zero * 3.0f) - mnf.width(), l.textY, mnf.width(), mnf.height());
        l.mnfPlusRect = QRectF(l.centre + (zero * 3.0f), l.textY, mnfPlus.width(), mnfPlus.height());
        l.snapRect = QRectF(l.mnfRect.x() - (zero * 2.0f) - snap.width(), l.textY, snap.width(), snap.height());
        l.fitRect = QRectF(l.mnfPlusRect.x() + (zero * 2.0f) + l.mnfPlusRect.width(), l.textY, zoomSize.width(), zoomSize.height());
        break;
    }

    l.filterL = l.centre + hzToPixels(std::abs(l.lowHz), l.pixelSpan, l.hzSpan) * (l.lowHz < 0 ? -1.0f : 1.0f);
    l.filterH = l.centre + hzToPixels(std::abs(l.highHz), l.pixelSpan, l.hzSpan) * (l.highHz < 0 ? -1.0f : 1.0f);
    l.lowBot = QPointF(l.filterL - l.slopePixels, l.y + l.h);
    l.lowTop = QPointF(l.filterL, l.y + l.textHeight);
    l.highBot = QPointF(l.filterH + l.slopePixels, l.y + l.h);
    l.highTop = QPointF(l.filterH, l.y + l.textHeight);
    l.topLeft = QPointF(l.filterL, l.y + lineWidthHalf + l.textHeight);
    l.topRight = QPointF(l.filterH, l.y + lineWidthHalf + l.textHeight);
    l.valid = true;
    return l;
}

FilterDisplayItem::Hover FilterDisplayItem::hoverAt(const ThetisLayout& l,
                                                    const QPointF& mouse) const
{
    Hover hv;
    if (!l.valid || !filterEnabled() || !m_mouseInside) { return hv; }
    const bool tx = txActive();

    // From Thetis MeterManager.cs:35871 [v2.10.3.15]: bool highlight_button =
    // mouse_entered && (any button rect contains the mouse).
    hv.onButton = l.mnfRect.contains(mouse) || l.mnfPlusRect.contains(mouse)
        || l.snapRect.contains(mouse) || l.fitRect.contains(mouse);

    // From Thetis MeterManager.cs:35890-35908 [v2.10.3.15].
    const bool selected = m_lowSelected || m_highSelected || m_topSelected
        || m_selectedNotchId != -1;
    if (!hv.onButton && canAdjust() && !m_autoZoom && !selected) {
        const float lowDist = pointToSegmentDistance(l.lowBot, l.lowTop, mouse);
        const float highDist = pointToSegmentDistance(l.highBot, l.highTop, mouse);
        // The old look draws no top line: only the edges are hit.
        const float shiftDist = m_thetisLook ? pointToSegmentDistance(l.topLeft, l.topRight, mouse)
                                             : std::numeric_limits<float>::max();
        if (lowDist <= highDist && lowDist <= shiftDist) {
            hv.low = isMouseNearLine(l.lowBot, l.lowTop, mouse, kEdgeGrabPixels);
        } else if (highDist <= lowDist && highDist <= shiftDist) {
            hv.high = isMouseNearLine(l.highBot, l.highTop, mouse, kEdgeGrabPixels);
        } else {
            hv.top = !tx && isMouseNearLine(l.topLeft, l.topRight, mouse, kTopGrabPixels);
        }
    }

    // notches. From Thetis MeterManager.cs:36076-36142 [v2.10.3.15]: only
    // while not transmitting; the last notch under the mouse wins.
    const bool edgeSelected = m_lowSelected || m_highSelected || m_topSelected;
    if (!tx && !edgeSelected) {
        if (m_selectedNotchId != -1) {
            hv.notchId = m_selectedNotchId;
        } else if (!hv.onButton) {
            const double extent = l.extentHz;
            for (const NotchMark& notch : m_state.notches) {
                const double shiftHz = notch.centreHz - m_state.vfoHz;
                if (std::abs(shiftHz) > extent + extent / 2.0) { continue; }
                const float notchX = l.centre + hzToPixels(std::abs(shiftHz), l.pixelSpan, l.hzSpan)
                    * (shiftHz < 0 ? -1.0f : 1.0f);
                // The old look draws a notch as a line at its centre.
                const float widthPix = m_thetisLook
                    ? hzToPixels(std::abs(notch.widthHz / 2.0), l.pixelSpan, l.hzSpan) : 0.0f;
                if (mouse.x() >= notchX - widthPix - kNotchGrabPixels
                    && mouse.x() <= notchX + widthPix + kNotchGrabPixels
                    && mouse.y() >= l.y + l.textHeight && mouse.y() <= l.y + l.h) {
                    hv.notchId = notch.id;
                }
            }
        }
    }
    if (hv.notchId != -1) {
        hv.low = hv.high = hv.top = false;
    }

    //mnf and +mnf buttons. From Thetis MeterManager.cs:36147-36192 [v2.10.3.15].
    const bool sel = selected || hv.notchId != -1;
    if (!sel) {
        if (l.mnfRect.contains(mouse)) {
            hv.mnf = true;
        } else if (l.mnfPlusRect.contains(mouse)) {
            hv.mnfPlus = true;
        } else if (l.snapRect.contains(mouse) && !m_autoZoom) {
            hv.snap = true;
        } else if (l.fitRect.contains(mouse)) {
            hv.fit = true;
        }
    }

    // closest to which filter edge? From Thetis MeterManager.cs:36213-36234
    // [v2.10.3.15]. The old look draws none of the number text these
    // click-to-step areas stand for, so it has none.
    if (m_thetisLook && hv.notchId == -1 && !hv.onButton && canAdjust() && !edgeSelected) {
        const float centreTopLine = float(l.topLeft.x() + ((l.topRight.x() - l.topLeft.x()) / 2.0));
        const float lowDist = distanceBetweenPoints(QPointF(l.filterL, l.textY + (l.textHeight / 2.0f)), mouse);
        const float highDist = distanceBetweenPoints(QPointF(l.filterH, l.textY + (l.textHeight / 2.0f)), mouse);
        const float shiftDist = distanceBetweenPoints(QPointF(centreTopLine, l.y), mouse);
        if (lowDist <= highDist && lowDist <= shiftDist) {
            hv.adjustLow = true;
        } else if (highDist <= lowDist && highDist <= shiftDist) {
            hv.adjustHigh = true;
        } else {
            hv.adjustShift = !tx;
        }
    }
    return hv;
}

QVector<float> FilterDisplayItem::greyscale(const ThetisLayout& l) const
{
    // From Thetis MeterManager.cs:19560-19614 [v2.10.3.15]
    // (buildSpectrumGreyScale): 1 outside the passband, 0 inside.
    const int n = int(m_spectrumData.size());
    QVector<float> grey(n, 0.0f);
    if (!m_greyWhenNotActive || n == 0) { return grey; }
    const float bandwidth = float(l.extentHz) * 2.0f;
    const float frequencyStep = bandwidth / float(n);
    const int centerIndex = n / 2;
    const int lowIndex = centerIndex + int(l.lowHz / frequencyStep);
    const int highIndex = centerIndex + int(l.highHz / frequencyStep);
    for (int i = 0; i < n; ++i) {
        grey[i] = (i < lowIndex || i > highIndex) ? 1.0f : 0.0f;
    }
    return grey;
}

QSizeF FilterDisplayItem::plotText(QPainter& p, const ThetisLayout& l, const QString& text,
                                   float x, float y, const QColor& colour, int alpha,
                                   bool alignRight, bool alignCentre, bool fillBackground,
                                   const QColor& back) const
{
    // From Thetis MeterManager.cs:37858-37920 [v2.10.3.15] (plotText).
    if (text.isEmpty()) { return {}; }
    const QFontMetricsF fm(l.font);
    const QSizeF sz(fm.horizontalAdvance(text), fm.height());
    const float left = alignRight ? x - float(sz.width())
                                  : (alignCentre ? x - float(sz.width()) * 0.5f : x);
    const float top = alignCentre ? y - float(sz.height()) * 0.5f : y;
    const QRectF rect(left, top, sz.width(), sz.height());
    if (fillBackground) { p.fillRect(rect, withAlpha(back, std::min(alpha, back.alpha()))); }
    p.setFont(l.font);
    p.setPen(withAlpha(colour, std::min(alpha, colour.alpha())));
    p.drawText(rect, Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, text);
    return sz;
}

void FilterDisplayItem::paintThetisSpectrum(QPainter& p, const ThetisLayout& l)
{
    // From Thetis MeterManager.cs:35614-35694 [v2.10.3.15].
    const int totData = int(m_spectrumData.size());
    if (totData < 2) { return; }
    const float widthBetweenSlopes = l.extentH - l.extentL;
    const float xStep = widthBetweenSlopes / float(totData);
    const float specRange = m_specMaxDb - m_specMinDb;
    if (specRange <= 0.0f) { return; }
    const float gridMax = m_specMaxDb;
    const float lineWidthHalf = l.lineWidth / 2.0f;
    const QRectF clipRect(l.extentL, l.y + lineWidthHalf + l.textHeight,
                          widthBetweenSlopes, l.spectrumHeight);
    p.save();
    p.setClipRect(clipRect, Qt::IntersectClip);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(clipRect, ItemColourLook::effective("FilterDisplay.meterBackColour", m_meterBackColour, this));

    const QVector<float> grey = greyscale(l);
    const QColor greyLine = greyOf(ItemColourLook::effective("FilterDisplay.dataLineColour", m_dataLineColour, this));
    const QColor greyFill = greyOf(ItemColourLook::effective("FilterDisplay.dataFillColour", m_dataFillColour, this));
    const float dbmToPixel = l.spectrumHeight / specRange;
    float py = (gridMax - m_spectrumData[0]) * dbmToPixel;
    QPointF oldPos(l.extentL, l.topPos + py);
    QPen linePen(ItemColourLook::effective("FilterDisplay.dataLineColour", m_dataLineColour, this), std::max(1.0f, xStep), Qt::SolidLine, Qt::RoundCap);
    QPen fillPen(ItemColourLook::effective("FilterDisplay.dataFillColour", m_dataFillColour, this), std::max(1.0f, xStep), Qt::SolidLine, Qt::FlatCap);
    for (int px = 1; px < totData; ++px) {
        py = (gridMax - m_spectrumData[size_t(px)]) * dbmToPixel;
        const QPointF newPos(l.extentL + (px * xStep), l.topPos + py);
        const float attenuation = grey[px];
        if (m_fillSpectrum) {
            fillPen.setColor(m_greyWhenNotActive ? blend(ItemColourLook::effective("FilterDisplay.dataFillColour", m_dataFillColour, this), greyFill, attenuation)
                                                 : ItemColourLook::effective("FilterDisplay.dataFillColour", m_dataFillColour, this));
            p.setPen(fillPen);
            p.drawLine(QPointF(newPos.x(), l.botPos), newPos);
        }
        linePen.setColor(m_greyWhenNotActive ? blend(ItemColourLook::effective("FilterDisplay.dataLineColour", m_dataLineColour, this), greyLine, attenuation)
                                             : ItemColourLook::effective("FilterDisplay.dataLineColour", m_dataLineColour, this));
        p.setPen(linePen);
        p.drawLine(oldPos, newPos);
        oldPos = newPos;
    }
    p.restore();
}

void FilterDisplayItem::paintThetisWaterfall(QPainter& p, const ThetisLayout& l)
{
    // From Thetis MeterManager.cs:35695-35768 [v2.10.3.15]. NereusSDR keeps
    // one history image that restarts on a TX edge (presentFrame), so the
    // RX/TX bitmap rescale between extents is not needed.
    const float lineWidthHalf = l.lineWidth / 2.0f;
    const float topY = l.y + lineWidthHalf + l.textHeight
        + (m_displayMode == DisplayMode::Panafall ? l.spectrumHeight : 0.0f);
    const QRectF clipRect(l.extentL, topY, l.extentH - l.extentL, l.spectrumHeight);
    const float bitmapHeight = std::max(float(m_waterfallImage.height()), l.spectrumHeight);
    const QRectF dest(clipRect.left(), topY, clipRect.width(), bitmapHeight);
    p.save();
    p.setClipRect(clipRect, Qt::IntersectClip);
    p.fillRect(clipRect, ItemColourLook::effective("FilterDisplay.meterBackColour", m_meterBackColour, this));
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(dest, m_waterfallImage);
    p.restore();
}

void FilterDisplayItem::paintThetisCharacteristic(QPainter& p, const ThetisLayout& l,
                                                  const QColor& lineColour)
{
    // From Thetis MeterManager.cs:35941-36070 [v2.10.3.15] (characteristic
    // plot). Thetis plots MiniSpec's simulated filter characteristic; here
    // the slice's computed filter response (the same data the DSP Options
    // high-resolution curve uses) is plotted from 0 dB at the top line down
    // to CharacteristicLow at the bottom line. TX has no response data, so
    // only the dashed edges show while transmitting.
    if (txActive() || m_rfFilterResponseDb.isEmpty() || m_rfFilterStepHz <= 0.0
        || !m_frameAvailable) {
        return;
    }
    const float cpHeight = l.botLineY - l.topLineY;
    const float fcMax = 0.0f;
    const float range = fcMax - m_characteristicLow;
    if (cpHeight <= 0.0f || range <= 0.0f) { return; }
    const float cpDbmToPixel = cpHeight / range;
    const QRectF clipRect(l.extentL, l.topLineY, l.extentH - l.extentL, cpHeight);
    p.save();
    p.setClipRect(clipRect, Qt::IntersectClip);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPolygonF curve;
    const int first = int(std::floor(l.extentL));
    const int last = int(std::ceil(l.extentH));
    for (int px = first; px <= last; ++px) {
        const double offsetFromCentre = pixelsToHz(std::abs(float(px) - l.centre), l.pixelSpan, l.hzSpan)
            * (float(px) < l.centre ? -1.0 : 1.0);
        const double rfHz = m_state.vfoHz + offsetFromCentre;
        const double index = (std::abs(rfHz - m_rfFilterReferenceHz) - m_rfFilterStartHz)
            / m_rfFilterStepHz;
        float db = m_characteristicLow;
        if (index >= 0.0 && index < m_rfFilterResponseDb.size() - 1) {
            const int lower = int(std::floor(index));
            const double alpha = index - lower;
            db = float(m_rfFilterResponseDb[lower] * (1.0 - alpha)
                       + m_rfFilterResponseDb[lower + 1] * alpha);
        }
        db = std::clamp(db, m_characteristicLow, fcMax);
        curve.append(QPointF(px, l.topPos + ((fcMax - db) * cpDbmToPixel)));
    }
    p.setPen(QPen(lineColour, l.lineWidth));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(curve);
    p.restore();
}

void FilterDisplayItem::paintThetis(QPainter& p, int widgetW, int widgetH)
{
    // From Thetis renderFilterDisplay (MeterManager.cs:35466-36270
    // [v2.10.3.15]).
    const ThetisLayout l = thetisLayout(widgetW, widgetH);
    if (!l.valid) { return; }
    p.save();
    p.setClipRect(l.itemRect, Qt::IntersectClip);
    // NereusSDR: with the workspace look the item's own background follows
    // the meter background (the panel, or the container's user background)
    // while that colour is at its default.
    const QColor itemBackground = ItemColourLook::followsLook(QStringLiteral("FilterDisplay.meterBackColour"), m_meterBackColour, this)
        ? ItemColourLook::effective("FilterDisplay.meterBackColour", m_meterBackColour, this)
        : kThetisItemBackground;
    p.fillRect(QRectF(l.x, l.y, l.w, l.h), itemBackground);
    if (m_state.sliceIndex < 0) {
        // No slice delivered yet (or it closed): nothing to draw against.
        p.fillRect(l.itemRect, ItemColourLook::effective("FilterDisplay.meterBackColour", m_meterBackColour, this));
        p.setPen(ItemColourLook::effective("FilterDisplay.textColour", m_textColour, this));
        p.drawText(l.itemRect, Qt::AlignCenter, QStringLiteral("Spectrum unavailable"));
        p.restore();
        return;
    }
    const bool tx = txActive();
    const QColor extentTextColour = ItemColourLook::effective("FilterDisplay.textColour", m_textColour, this);
    const QColor extentColour = ItemColourLook::effective("FilterDisplay.extentsColour", m_extentsColour, this);
    const QColor snaplineColour = ItemColourLook::effective("FilterDisplay.snapLineColour", m_snapLineColour, this);
    const QColor textOverlayColour = itemBackground; // same as the background
    const QColor filterLineColour = tx ? ItemColourLook::effective("FilterDisplay.edgesColourTX", m_edgesColourTX, this) : ItemColourLook::effective("FilterDisplay.edgesColourRX", m_edgesColourRX, this);
    const QColor filterLineFaded = withAlpha(filterLineColour, 128);
    const QColor filterLineHighlight = ItemColourLook::effective("FilterDisplay.edgeHighlightColour", m_edgeHighlightColour, this);
    const QColor textOverlayHighlight = ItemColourLook::effective("FilterDisplay.numberHighlightColour", m_numberHighlightColour, this);
    const Hover hv = hoverAt(l, m_mousePoint);

    p.setRenderHint(QPainter::Antialiasing, true);
    //top/bottom lines (Thetis ends them at w, not x + w; drawn to x + w here)
    p.setPen(QPen(extentColour, l.lineWidth));
    p.drawLine(QPointF(l.x, l.topLineY), QPointF(l.x + l.w, l.topLineY));
    p.drawLine(QPointF(l.x, l.botLineY), QPointF(l.x + l.w, l.botLineY));

    //spectrum + waterfall display
    if (m_displayMode != DisplayMode::None) {
        if (!m_frameAvailable) {
            p.setPen(ItemColourLook::effective("FilterDisplay.textColour", m_textColour, this));
            p.drawText(QRectF(l.extentL, l.topLineY, l.extentH - l.extentL, l.botLineY - l.topLineY),
                       Qt::AlignCenter, QStringLiteral("Spectrum unavailable"));
        } else {
            if (m_displayMode == DisplayMode::Panadapter || m_displayMode == DisplayMode::Panafall) {
                paintThetisSpectrum(p, l);
            }
            if (m_displayMode == DisplayMode::Waterfall || m_displayMode == DisplayMode::Panafall) {
                paintThetisWaterfall(p, l);
            }
        }
    }
    p.setRenderHint(QPainter::Antialiasing, true);

    //centre line
    p.setPen(QPen(extentColour, l.lineWidth));
    p.drawLine(QPointF(l.centre, l.y + l.h), QPointF(l.centre, l.y + l.textHeight));

    //vgrid
    if (m_snapLines && !m_autoZoom) {
        QPen dash(snaplineColour, l.lineWidth, Qt::DashLine);
        p.setPen(dash);
        for (float f : snapFrequencies(m_state.mode)) {
            if (f == 0.0f) { continue; }
            if (m_sidebandMode && ((l.sidebandSign == -1 && f > 0) || (l.sidebandSign == 1 && f < 0))) { continue; } // skip sideband not interested in
            const float xp = hzToPixels(std::abs(f), l.pixelSpan, l.hzSpan) * (f < 0 ? -1.0f : 1.0f);
            p.drawLine(QPointF(l.centre + xp, l.y + l.h), QPointF(l.centre + xp, l.y + l.textHeight));
        }
    }

    if (m_showFilterLimits) {
        //extent text
        const QString lowExtentText = QString::number(-l.extentHz);
        const QString highExtentText = QStringLiteral("+") + QString::number(l.extentHz);
        plotText(p, l, lowExtentText, l.x - l.zoomDiffPixels, l.textY, extentTextColour, 255,
                 false, false, false, {});
        plotText(p, l, highExtentText, l.x + l.w + l.zoomDiffPixels, l.textY, extentTextColour, 255,
                 true, false, false, {});
        p.setPen(QPen(extentColour, l.lineWidth));
        p.drawLine(QPointF(l.x + l.maxExtentTextWidth - l.zoomDiffPixels, l.y + l.h),
                   QPointF(l.extentL, l.y + l.textHeight));
        p.drawLine(QPointF(l.x + l.w - l.maxExtentTextWidth + l.zoomDiffPixels, l.y + l.h),
                   QPointF(l.extentH, l.y + l.textHeight));
    }

    //mode text
    const QSizeF md = plotText(p, l, SliceModel::modeName(m_state.mode), float(l.itemRect.left()), l.y,
                               extentTextColour, 255, false, false, false, {});
    const float modeTextRight = float(l.itemRect.left() + md.width());

    //filter
    const QString filterName = tx ? (m_state.txProfileName.isEmpty() ? QStringLiteral("TX")
                                                                     : m_state.txProfileName)
                                  : m_state.filterName;
    const QSizeF fn = plotText(p, l, filterName, float(l.itemRect.right() + 1), l.y,
                               extentTextColour, 255, true, false, false, {});
    const float filterNameLeft = float(l.itemRect.right() + 1 - fn.width());

    if (!filterEnabled()) {
        p.restore();
        return; // return if we are not able to do anything with this filter
    }

    //filter lines
    QPen edgePen(filterLineColour, l.lineWidth);
    const bool lowHighlighted = hv.low && !m_lowSelected;
    const bool highHighlighted = hv.high && !m_highSelected;
    const bool topHighlighted = hv.top && !m_topSelected;
    if (!m_showCharacteristic) {
        p.setPen(edgePen);
        if (!lowHighlighted) { p.drawLine(l.lowBot, l.lowTop); }
        if (!highHighlighted) { p.drawLine(l.highBot, l.highTop); }
        if (!topHighlighted) { p.drawLine(l.topLeft, l.topRight); }
    } else {
        //characteristic plot

        //dashed lines for filter edge settings
        p.setPen(QPen(filterLineFaded, l.lineWidth, Qt::DashLine));
        if (!lowHighlighted) { p.drawLine(l.lowBot, l.lowTop); }
        if (!highHighlighted) { p.drawLine(l.highBot, l.highTop); }

        //simulated plot
        paintThetisCharacteristic(p, l, filterLineColour);
    }

    p.setPen(QPen(filterLineHighlight, l.lineWidth * 3.0f));
    if (lowHighlighted || m_lowSelected) { p.drawLine(l.lowBot, l.lowTop); }
    if (highHighlighted || m_highSelected) { p.drawLine(l.highBot, l.highTop); }
    if (topHighlighted || m_topSelected) { p.drawLine(l.topLeft, l.topRight); }

    // notches
    if (!tx) {
        const QColor notchFill = withAlpha(ItemColourLook::effective("FilterDisplay.notchColour", m_notchColour, this), 96);
        const QColor notchFillHighlight = withAlpha(ItemColourLook::effective("FilterDisplay.notchHighlightColour", m_notchHighlightColour, this), 192);
        const QColor notchLineHighlight = filterLineHighlight;
        const QColor notchFillDisabled = withAlpha(ItemColourLook::effective("FilterDisplay.notchColour", m_notchColour, this), 48);
        const double extent = l.extentHz;
        for (const NotchMark& notch : m_state.notches) {
            const double shiftHz = notch.centreHz - m_state.vfoHz;
            if (std::abs(shiftHz) > extent + extent / 2.0) { continue; }
            const float notchX = l.centre + hzToPixels(std::abs(shiftHz), l.pixelSpan, l.hzSpan)
                * (shiftHz < 0 ? -1.0f : 1.0f);
            const float widthPix = hzToPixels(std::abs(notch.widthHz / 2.0), l.pixelSpan, l.hzSpan);
            const bool highlightNotch = hv.notchId == notch.id;
            const bool on = notch.active && m_state.tnfActive;
            const QRectF notchRect(QPointF(notchX - widthPix, l.y + l.textHeight),
                                   QPointF(notchX + widthPix, l.y + l.h));
            p.fillRect(notchRect, highlightNotch ? notchFillHighlight
                                                 : (on ? notchFill : notchFillDisabled));
            p.setPen(QPen(highlightNotch ? notchLineHighlight : (on ? notchFill : notchFillDisabled),
                          l.lineWidth));
            p.drawLine(QPointF(notchX, l.y + l.h), QPointF(notchX, l.y + l.textHeight));
        }
    }

    // centre text
    plotText(p, l, QStringLiteral("0"), l.centre, l.textY + (l.textHeight / 2.0f), extentTextColour, 255,
             false, true, false, {});

    //mnf and +mnf buttons
    const QColor mnfHighlight = hv.mnf ? ItemColourLook::effective("FilterDisplay.buttonHighlightColour", m_buttonHighlightColour, this) : textOverlayColour;
    const QColor mnfPlusHighlight = hv.mnfPlus ? ItemColourLook::effective("FilterDisplay.buttonHighlightColour", m_buttonHighlightColour, this) : textOverlayColour;
    const QColor snapHighlight = hv.snap ? ItemColourLook::effective("FilterDisplay.buttonHighlightColour", m_buttonHighlightColour, this) : textOverlayColour;
    const QColor zoomHighlight = hv.fit ? ItemColourLook::effective("FilterDisplay.buttonHighlightColour", m_buttonHighlightColour, this) : textOverlayColour;
    plotText(p, l, QLatin1String(kMnfLabel), float(l.mnfRect.x()), l.textY,
             m_state.tnfActive ? QColor(Qt::white) : extentTextColour, 255, false, false, true,
             m_state.tnfActive ? ItemColourLook::effective("FilterDisplay.settingOnColour", m_settingOnColour, this) : mnfHighlight);
    plotText(p, l, QLatin1String(kMnfPlusLabel), float(l.mnfPlusRect.x()), l.textY,
             extentTextColour, 255, false, false, true, mnfPlusHighlight);
    plotText(p, l, QLatin1String(kSnapLabel), float(l.snapRect.x()), l.textY,
             extentTextColour, m_autoZoom ? 92 : 255, false, false, true,
             m_snapLines ? ItemColourLook::effective("FilterDisplay.settingOnColour", m_settingOnColour, this) : snapHighlight);
    plotText(p, l, QLatin1String(kFitLabel), float(l.fitRect.x()), l.textY,
             extentTextColour, 255, false, false, true,
             m_autoZoom ? ItemColourLook::effective("FilterDisplay.settingOnColour", m_settingOnColour, this) : zoomHighlight);

    //filter edge values
    const QFontMetricsF fm(l.font);
    const QString lowText = hzText(l.lowHz);
    const QString highText = (l.highHz > 0 ? QStringLiteral("+") : QString()) + hzText(l.highHz);
    const float lowW = float(fm.horizontalAdvance(lowText));
    const float highW = float(fm.horizontalAdvance(highText));
    int collideCount = 0;
    if ((l.filterL + (lowW / 2.0f) >= l.filterH - (highW / 2.0f))
        && (l.filterL - (lowW / 2.0f) <= l.filterH + (highW / 2.0f))) {
        collideCount++;
    }
    const float centreTopLine = float(l.topLeft.x() + ((l.topRight.x() - l.topLeft.x()) / 2.0));

    // bw text
    const QString bw = hzText(std::abs(l.highHz - l.lowHz));
    const float bwW = float(fm.horizontalAdvance(bw));
    const float bwH = float(fm.height());
    const bool bwCollide = (centreTopLine + (bwW / 2.0f) > filterNameLeft)
        || (centreTopLine - (bwW / 2.0f) < modeTextRight);
    plotText(p, l, bw, centreTopLine, l.y + (l.textHeight / 2.0f) + (bwCollide ? bwH : 0.0f),
             extentTextColour, 255, false, true, true,
             hv.adjustShift ? textOverlayHighlight : textOverlayColour);

    // filter edge number text
    const float yCollideOffset = float(collideCount) * -l.zeroHeight;
    plotText(p, l, lowText, l.filterL, l.textY + yCollideOffset - l.textHeight, extentTextColour, 255,
             false, true, true, hv.adjustLow ? textOverlayHighlight : textOverlayColour);
    plotText(p, l, highText, l.filterH, l.textY - l.textHeight, extentTextColour, 255,
             false, true, true, hv.adjustHigh ? textOverlayHighlight : textOverlayColour);
    p.restore();
}

// ---------------------------------------------------------------------------
// Requests. From Thetis Low / High / Shift / AdjustNotch (MeterManager.cs:
// 19151-19412 [v2.10.3.15]); where Thetis calls the console the item emits.
// ---------------------------------------------------------------------------
void FilterDisplayItem::requestLow(float value, Qt::KeyboardModifiers mods, bool ignoreSnap)
{
    if (!interactive()) { return; }
    const bool ctrl = mods.testFlag(Qt::ControlModifier);
    const bool shift = mods.testFlag(Qt::ShiftModifier);
    if (txActive()) {
        float val = ctrl ? roundTo10(value) : value;
        if (!ignoreSnap && m_snapLines && !m_autoZoom) { val = float(findNearestVGrid(int(val))); }
        int low = m_state.txLowHz;
        int high = m_state.txHighHz;
        switch (m_state.mode) {
        case DSPMode::AM:
        case DSPMode::DIGL:
        case DSPMode::DRM:
        case DSPMode::DSB:
        case DSPMode::FM:
        case DSPMode::LSB:
        case DSPMode::SAM:
        case DSPMode::SPEC:
            if (val > 0) { val = 0; }
            high = int(std::abs(val));
            break;
        default:
            if (val < 0) { val = 0; }
            low = int(std::abs(val));
            break;
        }
        if (low == m_state.txLowHz && high == m_state.txHighHz) { return; }
        emit filterEdgesRequested(m_state.sliceIndex, low, high, true);
        return;
    }
    int low = int(ctrl ? roundTo10(value) : value);
    if (!ignoreSnap && m_snapLines && !m_autoZoom) { low = findNearestVGrid(low); }
    if (low == m_state.rxLowHz) { return; }
    const bool mirror = twoSidebands() && !shift;
    const int high = m_state.rxHighHz;
    if (mirror) {
        if (low > 0) { low = 0; }
        emit filterEdgesRequested(m_state.sliceIndex, low, std::abs(low), false);
    } else {
        if (low > high - 10) { low = high - 10; }
        emit filterEdgesRequested(m_state.sliceIndex, low, high, false);
    }
}

void FilterDisplayItem::requestHigh(float value, Qt::KeyboardModifiers mods, bool ignoreSnap)
{
    if (!interactive()) { return; }
    const bool ctrl = mods.testFlag(Qt::ControlModifier);
    const bool shift = mods.testFlag(Qt::ShiftModifier);
    if (txActive()) {
        float val = ctrl ? roundTo10(value) : value;
        if (!ignoreSnap && m_snapLines && !m_autoZoom) { val = float(findNearestVGrid(int(val))); }
        int low = m_state.txLowHz;
        int high = m_state.txHighHz;
        switch (m_state.mode) {
        case DSPMode::DIGL:
        case DSPMode::LSB:
            if (val > 0) { val = 0; }
            low = int(std::abs(val));
            break;
        default:
            if (val < 0) { val = 0; }
            high = int(std::abs(val));
            break;
        }
        if (low == m_state.txLowHz && high == m_state.txHighHz) { return; }
        emit filterEdgesRequested(m_state.sliceIndex, low, high, true);
        return;
    }
    int high = int(ctrl ? roundTo10(value) : value);
    if (!ignoreSnap && m_snapLines && !m_autoZoom) { high = findNearestVGrid(high); }
    if (high == m_state.rxHighHz) { return; }
    const bool mirror = twoSidebands() && !shift;
    const int low = m_state.rxLowHz;
    if (mirror) {
        if (high < 0) { high = 0; }
        emit filterEdgesRequested(m_state.sliceIndex, -high, high, false);
    } else {
        if (high < low + 10) { high = low + 10; }
        emit filterEdgesRequested(m_state.sliceIndex, low, high, false);
    }
}

void FilterDisplayItem::requestShift(float shiftHz, Qt::KeyboardModifiers mods)
{
    // From Thetis MeterManager.cs:19378-19412 [v2.10.3.15]: nothing while
    // MOX.
    if (!interactive() || txActive()) { return; }
    if (shiftHz == 0) { return; }
    const float shift = float(int(shiftHz));
    float fLow = m_startLow + shift;
    float fHigh = m_startHigh + shift;
    if (mods.testFlag(Qt::ControlModifier)) {
        fLow = roundTo10(fLow);
        fHigh = roundTo10(fHigh);
    }
    const int low = int(fLow);
    const int high = int(fHigh);
    if (low == m_state.rxLowHz && high == m_state.rxHighHz) { return; }
    emit filterShiftRequested(m_state.sliceIndex, low, high);
}

void FilterDisplayItem::requestNotchMove(float deltaHz, Qt::KeyboardModifiers mods)
{
    if (!interactive() || m_selectedNotchId == -1) { return; }
    // From Thetis MeterManager.cs:19351-19377 [v2.10.3.15] (AdjustNotch).
    double newFreq = double(int(deltaHz)) + m_notchStartHz;
    newFreq = mods.testFlag(Qt::ControlModifier) ? std::nearbyint(newFreq / 10.0) * 10.0 : newFreq;
    for (NotchMark& n : m_state.notches) {
        if (n.id != m_selectedNotchId) { continue; }
        const double vfoFreq = m_state.vfoHz;
        const double extent = extentHz();
        const bool ok = (n.centreHz != newFreq)
            && ((newFreq >= vfoFreq - extent) && (newFreq <= vfoFreq + extent));
        if (ok) {
            n.centreHz = newFreq; //[2.10.3.9]MW0LGE update the data, prevents loads of updates
            emit notchChangeRequested(n.id, newFreq, n.widthHz);
        }
        return;
    }
}

void FilterDisplayItem::resetFilterPosition()
{
    // From Thetis MeterManager.cs:18871-18920 [v2.10.3.15]: right click on
    // the top line puts the passband back at the mode's default place,
    // keeping its width. Thetis sets Low then High; one request carries
    // both here.
    if (!interactive() || txActive()) { return; }
    const int bw = int(displayHighHz() - displayLowHz());
    int high = int(displayHighHz());
    int low = int(displayLowHz());
    switch (m_state.mode) {
    case DSPMode::AM:
    case DSPMode::DSB:
    case DSPMode::SPEC:
    case DSPMode::DRM:
    case DSPMode::FM:
    case DSPMode::SAM:
        low = -bw / 2;
        high = bw / 2;
        break;
    case DSPMode::LSB:
        high = -kDefaultLowCutHz;
        low = high - bw;
        break;
    case DSPMode::USB:
        low = kDefaultLowCutHz;
        high = low + bw;
        break;
    case DSPMode::DIGL:
        high = -m_state.diglOffsetHz + bw / 2;
        low = -m_state.diglOffsetHz - bw / 2;
        break;
    case DSPMode::DIGU:
        low = m_state.diguOffsetHz - bw / 2;
        high = m_state.diguOffsetHz + bw / 2;
        break;
    case DSPMode::CWL:
        high = -m_state.cwPitchHz + bw / 2;
        low = -m_state.cwPitchHz - bw / 2;
        break;
    case DSPMode::CWU:
        low = m_state.cwPitchHz - bw / 2;
        high = m_state.cwPitchHz + bw / 2;
        break;
    default:
        break;
    }
    emit filterEdgesRequested(m_state.sliceIndex, low, high, false);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
bool FilterDisplayItem::hitTest(const QPointF& pos, int widgetW, int widgetH) const
{
    // A drag keeps the item while the mouse leaves it, as Thetis keeps the
    // selection until MouseUp.
    if (m_mouseDown && interactive()) { return true; }
    return MeterItem::hitTest(pos, widgetW, widgetH);
}

bool FilterDisplayItem::handleMousePress(QMouseEvent* event, int widgetW, int widgetH)
{
    if (!interactive() || !event) { return false; }
    m_mousePoint = event->position();
    m_mouseInside = true;
    m_mouseDown = true;
    m_mouseButton = event->button();
    const ThetisLayout l = thetisLayout(widgetW, widgetH);
    const Hover hv = hoverAt(l, m_mousePoint);
    // From Thetis MeterManager.cs:35909-35929, 36127-36132 [v2.10.3.15]:
    // a press on a highlighted part selects it.
    if (hv.notchId != -1) {
        if (const NotchMark* notch = notchById(hv.notchId)) {
            m_startShiftX = float(m_mousePoint.x());
            m_startLow = displayLowHz();
            m_startHigh = displayHighHz();
            m_selectedNotchId = notch->id;
            m_notchStartHz = notch->centreHz;
        }
    } else if (hv.low) {
        m_lowSelected = true;
    } else if (hv.high) {
        m_highSelected = true;
    } else if (hv.top) {
        m_startShiftX = float(m_mousePoint.x());
        m_startLow = displayLowHz();
        m_startHigh = displayHighHz();
        m_topSelected = true;
    }
    return true;
}

bool FilterDisplayItem::handleMouseMove(QMouseEvent* event, int widgetW, int widgetH)
{
    if (!interactive() || !event) { return false; }
    m_mousePoint = event->position();
    m_mouseInside = MeterItem::hitTest(m_mousePoint, widgetW, widgetH);
    if (!m_mouseDown) { return true; }
    // From Thetis MeterManager.cs:36247-36269 [v2.10.3.15] (mouse down).
    const ThetisLayout l = thetisLayout(widgetW, widgetH);
    const Qt::KeyboardModifiers mods = event->modifiers();
    const float mx = float(m_mousePoint.x());
    if (m_lowSelected) {
        requestLow(float(pixelsToHz(mx - l.centre, l.pixelSpan, l.hzSpan)), mods, false);
    } else if (m_highSelected) {
        requestHigh(float(pixelsToHz(mx - l.centre, l.pixelSpan, l.hzSpan)), mods, false);
    } else if (m_topSelected) {
        const float delta = mx - m_startShiftX;
        requestShift(float(pixelsToHz(std::abs(delta), l.pixelSpan, l.hzSpan)) * (delta < 0 ? -1.0f : 1.0f),
                     mods);
    } else if (m_selectedNotchId != -1 && m_mouseButton != Qt::RightButton) {
        //adjust notch
        const float delta = mx - m_startShiftX;
        requestNotchMove(float(pixelsToHz(std::abs(delta), l.pixelSpan, l.hzSpan)) * (delta < 0 ? -1.0f : 1.0f),
                         mods);
    }
    return true;
}

bool FilterDisplayItem::handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH)
{
    if (!interactive() || !event) {
        m_mouseDown = false;
        return false;
    }
    // From Thetis MeterManager.cs:18858-19012 [v2.10.3.15] (MouseUp).
    m_mousePoint = event->position();
    m_mouseInside = MeterItem::hitTest(m_mousePoint, widgetW, widgetH);
    const ThetisLayout l = thetisLayout(widgetW, widgetH);
    const Hover hv = hoverAt(l, m_mousePoint);
    m_mouseDown = false;
    const Qt::MouseButton button = event->button();
    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool ctrl = mods.testFlag(Qt::ControlModifier);
    const bool tx = txActive();

    if (button == Qt::RightButton && m_topSelected) {
        // reset
        resetFilterPosition();
        m_topSelected = false;
        return true;
    } else if (button == Qt::RightButton && hv.notchId != -1 && !ctrl) {
        const int id = hv.notchId;
        clearSelection();
        // MainWindow owns the menu (buildNotchMenu fills it).
        emit notchMenuRequested(id, event->globalPosition().toPoint());
        return true;
    }

    clearSelection();

    //notch at mouse pos with ctrl
    if (button == Qt::RightButton && ctrl && filterEnabled() && !tx) {
        // add a notch at frequency
        const double mouseFrequency = pixelsToHz(float(m_mousePoint.x()) - l.centre, l.pixelSpan, l.hzSpan);
        const double freq = std::nearbyint(m_state.vfoHz + mouseFrequency);
        emit notchAddRequested(m_state.sliceIndex, freq);
        return true;
    }

    const float step = mods.testFlag(Qt::ShiftModifier) ? kFineStepHz : kStepHz;
    if (hv.adjustLow) {
        if (button == Qt::LeftButton) { requestLow(displayLowHz() - step, mods, true); }
        if (button == Qt::RightButton) { requestLow(displayLowHz() + step, mods, true); }
    }
    if (hv.adjustHigh) {
        if (button == Qt::LeftButton) { requestHigh(displayHighHz() - step, mods, true); }
        if (button == Qt::RightButton) { requestHigh(displayHighHz() + step, mods, true); }
    }
    if (hv.adjustShift) {
        m_startLow = displayLowHz();
        m_startHigh = displayHighHz();
        if (button == Qt::LeftButton) { requestShift(-step, mods); }
        if (button == Qt::RightButton) { requestShift(step, mods); }
    }

    if (hv.mnf) {
        emit tnfToggleRequested();
    } else if (hv.mnfPlus && !tx) {
        emit tnfAddRequested(m_state.sliceIndex);
    } else if (hv.snap && !m_autoZoom) {
        m_snapLines = !m_snapLines;
    } else if (hv.fit) {
        m_autoZoom = !m_autoZoom;
    }
    return true;
}

bool FilterDisplayItem::handleWheel(QWheelEvent* event, int widgetW, int widgetH)
{
    if (!interactive() || !event) { return false; }
    // From Thetis MeterManager.cs:19013-19068 [v2.10.3.15] (MouseWheel).
    m_mousePoint = event->position();
    m_mouseInside = true;
    const int delta = event->angleDelta().y();
    const int sign = (delta > 0) - (delta < 0);
    if (sign == 0) { return true; }
    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool ctrl = mods.testFlag(Qt::ControlModifier);
    const float step = mods.testFlag(Qt::ShiftModifier) ? kFineStepHz : kStepHz;
    const ThetisLayout l = thetisLayout(widgetW, widgetH);
    const Hover hv = hoverAt(l, m_mousePoint);

    if (!m_thetisLook && hv.notchId == -1 && m_selectedNotchId == -1) {
        // The old look: the wheel acts only on a drawn edge under the
        // pointer (and on a drawn notch, below).
        if (hv.low) {
            const float tmp = ctrl ? roundTo10(displayLowHz()) : displayLowHz();
            requestLow(tmp + (sign * step), mods, true);
        } else if (hv.high) {
            const float tmp = ctrl ? roundTo10(displayHighHz()) : displayHighHz();
            requestHigh(tmp + (sign * step), mods, true);
        }
    } else if (hv.notchId == -1 && m_selectedNotchId == -1) {
        if (hv.adjustLow) {
            const float tmp = ctrl ? roundTo10(displayLowHz()) : displayLowHz();
            requestLow(tmp + (sign * step), mods, true);
        } else if (hv.adjustHigh) {
            const float tmp = ctrl ? roundTo10(displayHighHz()) : displayHighHz();
            requestHigh(tmp + (sign * step), mods, true);
        } else if (hv.adjustShift) {
            m_startLow = displayLowHz();
            m_startHigh = displayHighHz();
            requestShift(sign * step, mods);
        }
    } else {
        //notch
        const int id = hv.notchId != -1 ? hv.notchId : m_selectedNotchId;
        if (const NotchMark* n = notchById(id)) {
            double width = n->widthHz;
            width += sign * step;
            if (width < 0) { width = 0; }
            emit notchChangeRequested(n->id, n->centreHz, width);
        }
    }
    return true;
}

void FilterDisplayItem::buildNotchMenu(int id, QMenu& menu)
{
    // Thetis opens frmNotchPopup (console.cs:33541 [v2.10.3.15]) for the
    // notch: its width, its active flag and delete. NereusSDR offers the
    // same choices the panadapter's notch menu does
    // (SpectrumWidget::buildNotchContextMenu).
    const NotchMark* n = notchById(id);
    if (!n) { return; }
    const double centreHz = n->centreHz;
    const int widthHz = qRound(n->widthHz);
    const bool active = n->active;
    QAction* info = menu.addAction(QStringLiteral("%1 MHz    %2 Hz")
                                       .arg(centreHz / 1.0e6, 0, 'f', 6)
                                       .arg(widthHz));
    info->setEnabled(false);
    menu.addSeparator();
    const int minWidthHz = int(std::ceil(m_state.minNotchWidthHz));
    QMenu* widthMenu = menu.addMenu(QStringLiteral("Width"));
    for (int presetHz : {50, 100, 200, 500}) {
        const bool realisable = presetHz >= minWidthHz;
        QAction* a = widthMenu->addAction(
            realisable ? QStringLiteral("%1 Hz").arg(presetHz)
                       : QStringLiteral("%1 Hz  (min %2 Hz)").arg(presetHz).arg(minWidthHz),
            this, [this, id, centreHz, presetHz]() {
                emit notchChangeRequested(id, centreHz, double(presetHz));
            });
        a->setCheckable(true);
        a->setChecked(widthHz == presetHz);
        a->setEnabled(realisable);
    }
    menu.addSeparator();
    menu.addAction(active ? QStringLiteral("Bypass Notch") : QStringLiteral("Activate Notch"),
                   this, [this, id, active]() { emit notchActiveRequested(id, !active); });
    menu.addSeparator();
    menu.addAction(QStringLiteral("Remove Notch"), this,
                   [this, id]() { emit notchRemoveRequested(id); });
}

// ---------------------------------------------------------------------------
// FilterDisplayRequests: how MainWindow applies the item's requests.
// ---------------------------------------------------------------------------
namespace FilterDisplayRequests {

void applyEdges(SliceModel* slice, TransmitModel* transmit, int lowHz, int highHz, bool tx)
{
    if (tx) {
        // The TX passband (Thetis console.TXFilterLow/High). This changes
        // the transmitted filter only; it never keys the radio.
        if (!transmit) { return; }
        // TransmitModel swaps an inverted pair on commit, so set the edge
        // that keeps the pair ordered first.
        if (lowHz > transmit->filterHigh()) {
            transmit->setFilterHigh(highHz);
            transmit->setFilterLow(lowHz);
        } else {
            if (lowHz != transmit->filterLow()) { transmit->setFilterLow(lowHz); }
            if (highHz != transmit->filterHigh()) { transmit->setFilterHigh(highHz); }
        }
        return;
    }
    if (slice) { slice->setFilter(lowHz, highHz); }
}

void applyShift(SliceModel* slice, int lowHz, int highHz)
{
    if (slice) { slice->shiftFilter(lowHz, highHz); }
}

void applyNotchChange(NotchModel* notches, int id, double hz, double widthHz)
{
    if (!notches) { return; }
    const Notch* current = notches->notchById(id);
    if (!current) { return; }
    if (current->centerHz != hz) { notches->setCenter(id, hz); }
    if (current->widthHz != widthHz) { notches->setWidth(id, widthHz); }
}

void applyTnfToggle(NotchModel* notches)
{
    if (notches) { notches->setGlobalEnabled(!notches->globalEnabled()); }
}

bool applyNotchAdd(RadioModel* radio, SliceModel* slice, double hz)
{
    if (!radio || !slice || radio->isTransmitting()) { return false; }
    return radio->addNotchForSlice(slice, hz, NotchModel::kDefaultNotchWidthHz) >= 0;
}

bool applyTnfAdd(RadioModel* radio, SliceModel* slice)
{
    if (!radio || !slice || radio->isTransmitting()) { return false; }
    return radio->addTnfForSlice(slice) >= 0;
}

} // namespace FilterDisplayRequests

} // namespace NereusSDR
