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

#include <QPainter>
#include <QPolygonF>
#include <QStringList>
#include <algorithm>
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
    // From Thetis clsFilterItem ctor (MeterManager.cs:17006) — MiniSpec.PIXELS
    m_spectrumData.assign(kSpectrumPixels, 0.0f);

    // Waterfall image: kSpectrumPixels wide, 200 rows tall, RGB32
    m_waterfallImage = QImage(kSpectrumPixels, 200, QImage::Format_RGB32);
    m_waterfallImage.fill(Qt::black);
}

// ---------------------------------------------------------------------------
// setSpectrumData()
// Copy up to kSpectrumPixels bins; zero-pad if count < kSpectrumPixels.
// From Thetis clsFilterItem Update() (MeterManager.cs:18296+)
// ---------------------------------------------------------------------------
void FilterDisplayItem::setSpectrumData(const float* bins, int count)
{
    if (!bins || count <= 0) {
        return;
    }

    const int copyCount = std::min(count, kSpectrumPixels);
    std::copy(bins, bins + copyCount, m_spectrumData.begin());

    // Zero-pad remaining bins if count < kSpectrumPixels
    if (copyCount < kSpectrumPixels) {
        std::fill(m_spectrumData.begin() + copyCount, m_spectrumData.end(), 0.0f);
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

    const int n = kSpectrumPixels;
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

    ++m_waterfallFrameCount;

    // From Thetis _waterfall_frame_interval — only update every Nth frame
    if ((m_waterfallFrameCount % m_waterfallFrameInterval) == 0) {
        const int imgW = m_waterfallImage.width();
        const int imgH = m_waterfallImage.height();

        // Shift all rows down by 1 (oldest row at bottom, newest at top)
        for (int row = imgH - 1; row > 0; --row) {
            const uchar* srcLine = m_waterfallImage.constScanLine(row - 1);
            uchar*       dstLine = m_waterfallImage.scanLine(row);
            std::copy(srcLine, srcLine + static_cast<size_t>(m_waterfallImage.bytesPerLine()), dstLine);
        }

        // Write new top row from current spectrum data
        QRgb* topRow = reinterpret_cast<QRgb*>(m_waterfallImage.scanLine(0));
        for (int i = 0; i < imgW && i < kSpectrumPixels; ++i) {
            const QColor c = dbToWaterfallColor(m_spectrumData[static_cast<size_t>(i)]);
            topRow[i] = c.rgb();
        }
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
    const float dbRange = m_specMaxDb - m_specMinDb;
    float intensity = (dbRange > 1e-6f)
        ? std::clamp((db - m_specMinDb) / dbRange, 0.0f, 1.0f)
        : 0.0f;

    // From Thetis Enhanced palette: blue (240°) → cyan → green → yellow → red (0°)
    const int hue = static_cast<int>(240.0f - intensity * 240.0f); // 240 → 0
    const int sat = 255;
    const int val = static_cast<int>(60.0f + intensity * 195.0f);  // dim floor at low signals

    return QColor::fromHsv(hue, sat, val);
}

// ---------------------------------------------------------------------------
// paintFilterEdges()
// Draw RX and TX filter edge markers.
// From Thetis clsFilterItem _edges_colour_rx/_edges_colour_tx (MeterManager.cs:16951+)
// Pixel positions (0-511) are mapped to the item's pixel rect width.
// ---------------------------------------------------------------------------
void FilterDisplayItem::paintFilterEdges(QPainter& p, const QRect& rect)
{
    if (rect.isEmpty()) {
        return;
    }

    const float scale = static_cast<float>(rect.width()) / static_cast<float>(kSpectrumPixels);

    auto pixelToX = [&](int pixPos) -> int {
        return rect.left() + static_cast<int>(static_cast<float>(pixPos) * scale);
    };

    p.setRenderHint(QPainter::Antialiasing, false);

    // RX filter edges — 2px solid yellow
    // From Thetis _edges_colour_rx (MeterManager.cs:16951)
    p.setPen(QPen(m_edgesColourRX, 2));
    p.drawLine(pixelToX(m_rxLow),  rect.top(), pixelToX(m_rxLow),  rect.bottom());
    p.drawLine(pixelToX(m_rxHigh), rect.top(), pixelToX(m_rxHigh), rect.bottom());

    // TX filter edges — 1px dashed red (only when txLow >= 0)
    // From Thetis _edges_colour_tx (MeterManager.cs:16952)
    if (m_txLow >= 0 && m_txHigh >= 0) {
        QPen txPen(m_edgesColourTX, 1, Qt::DashLine);
        p.setPen(txPen);
        p.drawLine(pixelToX(m_txLow),  rect.top(), pixelToX(m_txLow),  rect.bottom());
        p.drawLine(pixelToX(m_txHigh), rect.top(), pixelToX(m_txHigh), rect.bottom());
    }
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

    const float scale = static_cast<float>(rect.width()) / static_cast<float>(kSpectrumPixels);

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
