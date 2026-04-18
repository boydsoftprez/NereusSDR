// =================================================================
// src/gui/meters/HistoryGraphItem.cpp  (NereusSDR)
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

#include "HistoryGraphItem.h"

// From Thetis clsHistoryItem (MeterManager.cs:16149+)

#include <QPainter>
#include <QStringList>
#include <cmath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Constructor
// From Thetis clsHistoryItem — default capacity 300 (30s at 100ms poll)
// ---------------------------------------------------------------------------
HistoryGraphItem::HistoryGraphItem(QObject* parent)
    : MeterItem(parent)
{
    m_buf0.resize(kDefaultCapacity);
    m_buf1.resize(kDefaultCapacity);
}

// ---------------------------------------------------------------------------
// setCapacity()
// Resize both ring buffers, clearing all data.
// ---------------------------------------------------------------------------
void HistoryGraphItem::setCapacity(int cap)
{
    if (cap < 2) { cap = 2; }
    m_capacity = cap;
    m_buf0.resize(cap);
    m_buf1.resize(cap);
}

// ---------------------------------------------------------------------------
// setValue() — axis 0
// From Thetis clsHistoryItem addReading() (MeterManager.cs:16468+)
// ---------------------------------------------------------------------------
void HistoryGraphItem::setValue(double v)
{
    MeterItem::setValue(v);
    m_buf0.push(static_cast<float>(v));
}

// ---------------------------------------------------------------------------
// setValue1() — axis 1
// From Thetis clsHistoryItem addReading() second history list
// ---------------------------------------------------------------------------
void HistoryGraphItem::setValue1(double v)
{
    m_buf1.push(static_cast<float>(v));
}

// ---------------------------------------------------------------------------
// participatesIn()
// Renders to both OverlayStatic (grid) and OverlayDynamic (line data).
// ---------------------------------------------------------------------------
bool HistoryGraphItem::participatesIn(Layer layer) const
{
    return layer == Layer::OverlayStatic || layer == Layer::OverlayDynamic;
}

// ---------------------------------------------------------------------------
// paint() — single-layer fallback; draws both layers in order
// ---------------------------------------------------------------------------
void HistoryGraphItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    paintGrid(p, rect);

    float yMin0 = 0.0f, yMax0 = 1.0f;
    computeScale(m_buf0, yMin0, yMax0);
    paintLine(p, rect, m_buf0, m_lineColor0, yMin0, yMax0);

    if (m_bindingId1 >= 0) {
        float yMin1 = 0.0f, yMax1 = 1.0f;
        computeScale(m_buf1, yMin1, yMax1);
        paintLine(p, rect, m_buf1, m_lineColor1, yMin1, yMax1);
    }
}

// ---------------------------------------------------------------------------
// paintForLayer() — multi-layer dispatch
// ---------------------------------------------------------------------------
void HistoryGraphItem::paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer)
{
    const QRect rect = pixelRect(widgetW, widgetH);

    if (layer == Layer::OverlayStatic) {
        paintGrid(p, rect);
    } else if (layer == Layer::OverlayDynamic) {
        float yMin0 = 0.0f, yMax0 = 1.0f;
        computeScale(m_buf0, yMin0, yMax0);
        paintLine(p, rect, m_buf0, m_lineColor0, yMin0, yMax0);

        if (m_bindingId1 >= 0) {
            float yMin1 = 0.0f, yMax1 = 1.0f;
            computeScale(m_buf1, yMin1, yMax1);
            paintLine(p, rect, m_buf1, m_lineColor1, yMin1, yMax1);
        }
    }
}

// ---------------------------------------------------------------------------
// computeScale()
// Auto-scale: yMin = runMin - 5% padding, yMax = runMax + 5% padding.
// If range < 1.0, pad to at least ±0.5 around midpoint.
// From Thetis clsHistoryItem addReading() scale logic (MeterManager.cs:16468+)
// ---------------------------------------------------------------------------
void HistoryGraphItem::computeScale(const RingBuffer& buf, float& yMin, float& yMax) const
{
    if (buf.count == 0) {
        yMin = 0.0f;
        yMax = 1.0f;
        return;
    }

    const float rawMin = buf.runMin;
    const float rawMax = buf.runMax;
    const float range = rawMax - rawMin;

    if (range < 1.0f) {
        const float mid = (rawMin + rawMax) * 0.5f;
        yMin = mid - 0.5f;
        yMax = mid + 0.5f;
    } else {
        const float pad = range * 0.05f;
        yMin = rawMin - pad;
        yMax = rawMax + pad;
    }
}

// ---------------------------------------------------------------------------
// paintGrid()  [OverlayStatic layer]
// 5 horizontal grid lines, Y-axis labels on left edge.
// From Thetis clsHistoryItem render logic (MeterManager.cs:16149+)
// Colors: background #0a0a18, border #203040, grid #203040, labels #8090a0
// ---------------------------------------------------------------------------
void HistoryGraphItem::paintGrid(QPainter& p, const QRect& rect)
{
    if (rect.isEmpty()) { return; }

    // Background fill
    p.fillRect(rect, QColor(0x0a, 0x0a, 0x18));

    // Border
    p.setPen(QPen(QColor(0x20, 0x30, 0x40), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect.adjusted(0, 0, -1, -1));

    if (!m_showGrid) { return; }

    // 5 horizontal grid lines evenly spaced
    // From Thetis clsHistoryItem — grid line rendering
    const int lineCount = 5;
    p.setPen(QPen(QColor(0x20, 0x30, 0x40), 1));

    // Compute Y scale for label values using axis 0
    float yMin = 0.0f, yMax = 1.0f;
    if (m_autoScale0 && m_buf0.count > 0) {
        computeScale(m_buf0, yMin, yMax);
    }

    QFont labelFont;
    labelFont.setPixelSize(9);
    p.setFont(labelFont);
    p.setPen(QColor(0x80, 0x90, 0xa0));

    for (int i = 0; i <= lineCount; ++i) {
        const float fraction = static_cast<float>(i) / static_cast<float>(lineCount);
        const int y = rect.top() + static_cast<int>(fraction * rect.height());

        // Grid line
        p.setPen(QPen(QColor(0x20, 0x30, 0x40), 1));
        p.drawLine(rect.left(), y, rect.right(), y);

        // Y-axis label (value at this grid line, top=yMax, bottom=yMin)
        if (m_showScale0) {
            const float labelVal = yMax - fraction * (yMax - yMin);
            const QString labelText = QString::number(static_cast<double>(labelVal), 'f', 1);
            p.setPen(QColor(0x80, 0x90, 0xa0));
            const QRect textRect(rect.left() + 2, y - 8, 40, 12);
            p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, labelText);
        }
    }
}

// ---------------------------------------------------------------------------
// paintLine()  [OverlayDynamic layer]
// Connects ring buffer points left-to-right with antialiased line.
// From Thetis clsHistoryItem render loop (MeterManager.cs:16149+)
// ---------------------------------------------------------------------------
void HistoryGraphItem::paintLine(QPainter& p, const QRect& rect,
                                  const RingBuffer& buf,
                                  const QColor& color,
                                  float yMin, float yMax)
{
    if (buf.count < 2) { return; }
    if (rect.isEmpty()) { return; }

    const float yRange = yMax - yMin;
    if (std::abs(yRange) < 1e-6f) { return; }

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(color, 1.5f));
    p.setBrush(Qt::NoBrush);

    const int n = buf.count;
    const float rectW = static_cast<float>(rect.width());
    const float rectH = static_cast<float>(rect.height());

    // Compute first point
    float prevX = static_cast<float>(rect.left());
    float prevY = static_cast<float>(rect.bottom()) -
                  (buf.at(0) - yMin) / yRange * rectH;

    for (int i = 1; i < n; ++i) {
        const float x = static_cast<float>(rect.left()) +
                        static_cast<float>(i) * rectW / static_cast<float>(n - 1);
        const float y = static_cast<float>(rect.bottom()) -
                        (buf.at(i) - yMin) / yRange * rectH;

        p.drawLine(QPointF(prevX, prevY), QPointF(x, y));
        prevX = x;
        prevY = y;
    }
}

// ---------------------------------------------------------------------------
// serialize()
// Tag: HISTORY
// Format: HISTORY|x|y|w|h|bindingId|zOrder|capacity|lineColor0|lineColor1|
//         showGrid|autoScale0|autoScale1|showScale0|showScale1|bindingId1
// ---------------------------------------------------------------------------
QString HistoryGraphItem::serialize() const
{
    return QStringLiteral("HISTORY|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14|%15")
        .arg(static_cast<double>(m_x))
        .arg(static_cast<double>(m_y))
        .arg(static_cast<double>(m_w))
        .arg(static_cast<double>(m_h))
        .arg(m_bindingId)
        .arg(m_zOrder)
        .arg(m_capacity)
        .arg(m_lineColor0.name(QColor::HexArgb))
        .arg(m_lineColor1.name(QColor::HexArgb))
        .arg(m_showGrid ? 1 : 0)
        .arg(m_autoScale0 ? 1 : 0)
        .arg(m_autoScale1 ? 1 : 0)
        .arg(m_showScale0 ? 1 : 0)
        .arg(m_showScale1 ? 1 : 0)
        .arg(m_bindingId1);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts (16 total):
// [0]=HISTORY [1]=x [2]=y [3]=w [4]=h [5]=bindingId [6]=zOrder
// [7]=capacity [8]=lineColor0 [9]=lineColor1
// [10]=showGrid [11]=autoScale0 [12]=autoScale1
// [13]=showScale0 [14]=showScale1 [15]=bindingId1
// ---------------------------------------------------------------------------
bool HistoryGraphItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 16 || parts[0] != QLatin1String("HISTORY")) {
        return false;
    }

    bool ok = true;

    const float x = parts[1].toFloat(&ok);  if (!ok) { return false; }
    const float y = parts[2].toFloat(&ok);  if (!ok) { return false; }
    const float w = parts[3].toFloat(&ok);  if (!ok) { return false; }
    const float h = parts[4].toFloat(&ok);  if (!ok) { return false; }

    const int bindingId = parts[5].toInt(&ok);  if (!ok) { return false; }
    const int zOrder    = parts[6].toInt(&ok);  if (!ok) { return false; }
    const int capacity  = parts[7].toInt(&ok);  if (!ok || capacity < 2) { return false; }

    const QColor lineColor0(parts[8]);
    if (!lineColor0.isValid()) { return false; }
    const QColor lineColor1(parts[9]);
    if (!lineColor1.isValid()) { return false; }

    const int showGrid   = parts[10].toInt(&ok);  if (!ok) { return false; }
    const int autoScale0 = parts[11].toInt(&ok);  if (!ok) { return false; }
    const int autoScale1 = parts[12].toInt(&ok);  if (!ok) { return false; }
    const int showScale0 = parts[13].toInt(&ok);  if (!ok) { return false; }
    const int showScale1 = parts[14].toInt(&ok);  if (!ok) { return false; }
    const int bindingId1 = parts[15].toInt(&ok);  if (!ok) { return false; }

    setRect(x, y, w, h);
    setBindingId(bindingId);
    setZOrder(zOrder);
    setCapacity(capacity);

    m_lineColor0 = lineColor0;
    m_lineColor1 = lineColor1;
    m_showGrid   = (showGrid   != 0);
    m_autoScale0 = (autoScale0 != 0);
    m_autoScale1 = (autoScale1 != 0);
    m_showScale0 = (showScale0 != 0);
    m_showScale1 = (showScale1 != 0);
    m_bindingId1 = bindingId1;

    return true;
}

} // namespace NereusSDR
