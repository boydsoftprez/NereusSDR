// =================================================================
// src/gui/meters/HistoryGraphItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-03 — Responsive object text and measured role fitting by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23: R-R3-13: a no-reading sample (isNoMeterReading) is not
//                 pushed, so it moves neither the axis scale nor the line;
//                 sampleCount0() / sampleCount1() read-only accessors.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-09-23: R-R3-13 fix wave: no reading applies only to
//                 receive-signal bindings (isReceiveSignalBinding); a TX
//                 meter keeps WDSP's -400 zero-power floor as a number.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-09-25 - R-R3-32 (remote-window parity Task 6): axis 1 skips the
//                 sentinel on hardware bindings too. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - HISTORY building block: axis 1 fed through
//                 readingBindings / pushBindingValue and data source
//                 variable 2; per-axis update interval; vertical ratio,
//                 back, lines and time colours and manual min/max per axis
//                 (record v3); kHistoryThetisDefaults for new items.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Keep for up to 1800 s, as Thetis's Keep For control;
//                 axis 1 keeps its reading while data source variable 1
//                 feeds axis 0 (readingBindingsBesideMmio). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
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

// Modification history (NereusSDR):
//   2026-10-12 - Apply the approved workspace-only 11 px text minimum.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-26 - A graph whose length the operator set in its editor
//                 (hasOwnLength, saved as field [17]) keeps it; the
//                 Multimeter history duration applies only to graphs that
//                 never set one (followGlobalDurationMs). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - A saved capacity loads as written; deserialize no longer
//                 lets the saved duration recompute it, and clamps it to
//                 kMaxCapacity. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - deserialize comments give the field counts it reads
//                 (16, 17, 18). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - hasOwnLength is written at [25], after the v3 fields
//                 [17]..[24] (v4, 26 fields); an 18-field record still
//                 loads it from [17]. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.

#include "gui/styles/WorkspacePreview.h"
#include "HistoryGraphItem.h"
#include "ResponsiveText.h"

// From Thetis clsHistoryItem (MeterManager.cs:16149+)

#include <QPainter>
#include <QStringList>
#include <cmath>
#include <algorithm>

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
// setDurationMs() — Task 3.3
// Convert duration (milliseconds) to capacity (sample count).
// Capacity = durationMs * pollHz / 1000, where pollHz = 1000 / pollIntervalMs.
// Default poll interval is 100 ms (10 Hz), but MeterPoller may change it.
// We conservatively assume 10 Hz and allow some slack for timing variations.
//
// Formula: capacity = durationMs / (1000 / 10) = durationMs / 100
// Minimum capacity is 2; maximum is clamped by practical display limits.
// ---------------------------------------------------------------------------
void HistoryGraphItem::setDurationMs(int ms)
{
    // Clamp to Thetis's Keep For range, 1 s to 1800 s.
    // From Thetis setup.designer.cs:62510-62519 [v2.10.3.15] (nudHistory_keep_for
    // Maximum 1800, Minimum 1, in seconds)
    if (ms < kMinHistoryKeepForMs) { ms = kMinHistoryKeepForMs; }
    if (ms > kMaxHistoryKeepForMs) { ms = kMaxHistoryKeepForMs; }

    m_durationMs = ms;

    // Compute capacity from duration assuming 10 Hz poll (100 ms interval).
    // Thetis clsHistoryItem stores ~300 samples (30 s at 100 ms),
    // so factor of 10 per second is established.
    const int pollIntervalMs = 100;
    const int newCapacity = std::max(2, (ms / pollIntervalMs) + 1);

    setCapacity(newCapacity);
}

// ---------------------------------------------------------------------------
// applyNewItemDefaults()
// From Thetis clsHistoryItem constructor (MeterManager.cs:17528-17571 [v2.10.3.15])
// ---------------------------------------------------------------------------
void HistoryGraphItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const HistoryItemThetisDefaults& d = kHistoryThetisDefaults;
    m_verticalRatio = d.verticalRatio;
    m_backColour    = QColor::fromRgb(d.backColour);
    m_lineColor0    = QColor::fromRgb(d.axis0Colour);
    m_lineColor1    = QColor::fromRgb(d.axis1Colour);
    m_linesColour   = QColor::fromRgb(d.linesColour);
    m_timeColour    = QColor::fromRgb(d.timeColour);
    setBindingId(d.reading0);
    m_bindingId1    = d.reading1;
    setDurationMs(d.keepForMs);
    m_min0Manual    = d.min0Manual;
    m_max0Manual    = d.max0Manual;
    m_min1Manual    = d.min1Manual;
    m_max1Manual    = d.max1Manual;
    m_manualRange   = true;
    m_autoScale0    = d.autoScale0;
    m_autoScale1    = d.autoScale1;
    m_showScale1    = d.showScale1;
    setUpdateIntervalMs(d.updateIntervalMs);
}

void HistoryGraphItem::setVerticalRatio(float ratio)
{
    m_verticalRatio = std::clamp(ratio, kMinHistoryVerticalRatio, kMaxHistoryVerticalRatio);
}

void HistoryGraphItem::setManualMin(int axis, float v)
{
    const float clamped = std::clamp(v, kMinHistoryAxisMin, kMaxHistoryAxisMin);
    if (axis == 1) { m_min1Manual = clamped; } else { m_min0Manual = clamped; }
    m_manualRange = true;
}

void HistoryGraphItem::setManualMax(int axis, float v)
{
    const float clamped = std::clamp(v, kMinHistoryAxisMax, kMaxHistoryAxisMax);
    if (axis == 1) { m_max1Manual = clamped; } else { m_max0Manual = clamped; }
    m_manualRange = true;
}

// ---------------------------------------------------------------------------
// axisRange()
// From Thetis clsHistoryItem Min0/Max0/Min1/Max1 getters: the running range
// while auto scaling, else the manual one (MeterManager.cs:17896-17975 [v2.10.3.15]).
// A record without a manual range keeps auto ranging, as it always drew.
// ---------------------------------------------------------------------------
void HistoryGraphItem::axisRange(int axis, float& yMin, float& yMax) const
{
    const bool autoScale = axis == 1 ? m_autoScale1 : m_autoScale0;
    if (autoScale || !m_manualRange) {
        computeScale(axis == 1 ? m_buf1 : m_buf0, yMin, yMax);
        return;
    }
    yMin = manualMin(axis);
    yMax = manualMax(axis);
}

// ---------------------------------------------------------------------------
// Reading path: both axes.
// From Thetis clsHistoryItem: _reading_0 and _reading_1 are both read
// (ZeroOut, MeterManager.cs:17594-17615 [v2.10.3.15]).
// ---------------------------------------------------------------------------
QSet<int> HistoryGraphItem::readingBindings() const
{
    QSet<int> bindings;
    if (m_bindingId >= 0) { bindings.insert(m_bindingId); }
    // A data source variable on axis 1 replaces its reading.
    if (m_bindingId1 >= 0 && !hasMmioBinding(1)) { bindings.insert(m_bindingId1); }
    return bindings;
}

QSet<int> HistoryGraphItem::readingBindingsBesideMmio() const
{
    // From Thetis MeterManager.cs:18101-18165 [v2.10.3.15]: each axis reads
    // its data source or its reading on its own.
    if (m_bindingId1 >= 0 && !hasMmioBinding(1)) { return {m_bindingId1}; }
    return {};
}

void HistoryGraphItem::pushBindingValue(int binding, double value)
{
    // Data source variable 1 feeds axis 0 (setValue through MeterWidget).
    if (binding >= 0 && binding == m_bindingId && !hasMmioBinding() && axisAccepts(0)) {
        setValue(value);
    }
    if (binding >= 0 && binding == m_bindingId1 && !hasMmioBinding(1) && axisAccepts(1)) {
        setValue1(value);
    }
    m_deliveryMs = -1;
}

bool HistoryGraphItem::acceptsUpdate(qint64 monotonicMs)
{
    // The interval is kept per axis in pushBindingValue(); MeterWidget asks
    // once per reading, so an item-wide gate would drop the second axis.
    m_deliveryMs = monotonicMs;
    return true;
}

bool HistoryGraphItem::axisAccepts(int axis)
{
    const int interval = updateIntervalMs();
    if (interval <= 0 || m_deliveryMs < 0) { return true; }
    qint64& last = m_lastAxisMs[static_cast<size_t>(axis)];
    // A clock that moved backwards (a new time base) starts a new interval.
    if (last >= 0 && m_deliveryMs >= last && m_deliveryMs - last < interval) {
        return false;
    }
    last = m_deliveryMs;
    return true;
}

void HistoryGraphItem::setSecondaryValue(int index, double value)
{
    if (index != 1 || !std::isfinite(value)) { return; }
    m_buf1.push(static_cast<float>(value));
}

// ---------------------------------------------------------------------------
// followGlobalDurationMs()
// NereusSDR: the Multimeter history duration reaches only graphs that never
// set their own length.
// ---------------------------------------------------------------------------
void HistoryGraphItem::followGlobalDurationMs(int ms)
{
    if (m_hasOwnLength) {
        return;
    }
    setDurationMs(ms);
}

// ---------------------------------------------------------------------------
// setValue() — axis 0
// From Thetis clsHistoryItem addReading() (MeterManager.cs:16468+)
// ---------------------------------------------------------------------------
void HistoryGraphItem::setValue(double v)
{
    MeterItem::setValue(v);
    // NereusSDR (R-R3-13): skip a no-reading sample so neither the axis
    // scale nor the line plots the -400 dBm sentinel.
    if (isNoReading(v)) {
        return;
    }
    m_buf0.push(static_cast<float>(v));
}

// ---------------------------------------------------------------------------
// setValue1() — axis 1
// From Thetis clsHistoryItem addReading() second history list
// ---------------------------------------------------------------------------
void HistoryGraphItem::setValue1(double v)
{
    // NereusSDR (R-R3-13): skip a no-reading sample (see setValue()); axis 1
    // follows its own binding.
    if (isNoReadingBinding(m_bindingId1) && isNoMeterReading(v)) {
        return;
    }
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
    axisRange(0, yMin0, yMax0);
    paintLine(p, rect, m_buf0, m_lineColor0, yMin0, yMax0);

    if (m_bindingId1 >= 0 || hasMmioBinding(1)) {
        float yMin1 = 0.0f, yMax1 = 1.0f;
        axisRange(1, yMin1, yMax1);
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
        axisRange(0, yMin0, yMax0);
        paintLine(p, rect, m_buf0, m_lineColor0, yMin0, yMax0);

        if (m_bindingId1 >= 0 || hasMmioBinding(1)) {
            float yMin1 = 0.0f, yMax1 = 1.0f;
            axisRange(1, yMin1, yMax1);
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
// Colors: background backColour(), border and grid linesColour(), labels
// timeColour() (older records: #0a0a18, #203040, #8090a0)
// ---------------------------------------------------------------------------
void HistoryGraphItem::paintGrid(QPainter& p, const QRect& rect)
{
    if (rect.isEmpty()) { return; }

    // Background fill
    p.fillRect(rect, m_backColour);

    // Border
    p.setPen(QPen(m_linesColour, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect.adjusted(0, 0, -1, -1));

    if (!m_showGrid) { return; }

    // 5 horizontal grid lines evenly spaced
    // From Thetis clsHistoryItem — grid line rendering
    const int lineCount = 5;
    p.setPen(QPen(m_linesColour, 1));

    // Compute Y scale for label values using axis 0
    float yMin = 0.0f, yMax = 1.0f;
    if (m_manualRange) {
        axisRange(0, yMin, yMax);
    } else if (m_autoScale0 && m_buf0.count > 0) {
        computeScale(m_buf0, yMin, yMax);
    }

    QFont labelFont;
    const double scale=objectTextScale(rect,{260,120});
    labelFont.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(1,qRound(9*scale))));
    p.setFont(labelFont);
    p.setPen(m_timeColour);

    for (int i = 0; i <= lineCount; ++i) {
        const float fraction = static_cast<float>(i) / static_cast<float>(lineCount);
        const int y = rect.top() + static_cast<int>(fraction * rect.height());

        // Grid line
        p.setPen(QPen(m_linesColour, 1));
        p.drawLine(rect.left(), y, rect.right(), y);

        // Y-axis label (value at this grid line, top=yMax, bottom=yMin)
        if (m_showScale0) {
            const float labelVal = yMax - fraction * (yMax - yMin);
            const QString labelText = QString::number(static_cast<double>(labelVal), 'f', 1);
            p.setPen(m_timeColour);
            const QFontMetricsF metrics(labelFont,p.device());
            const double textWidth=WorkspacePreview::enabled() ? qMax(40*scale,metrics.horizontalAdvance(labelText)+2) : 40*scale;
            const double textHeight=WorkspacePreview::enabled() ? qMax(12*scale,metrics.height()) : 12*scale;
            QRectF textRect(rect.left()+2*scale,y-8*scale,textWidth,textHeight);
            if(textRect.top()<rect.top()) { textRect.moveTop(rect.top()); }
            if(textRect.bottom()>rect.bottom()) { textRect.moveBottom(rect.bottom()); }
            drawObjectText(p,textRect,labelText,9*scale,Qt::AlignLeft|Qt::AlignVCenter);
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
//         showGrid|autoScale0|autoScale1|showScale0|showScale1|bindingId1|durationMs (v2)|
//         verticalRatio|backColour|linesColour|timeColour|min0|max0|min1|max1 (v3)
// Task 3.3: durationMs appended for forward compatibility
// v3: an item without a manual range writes [21]..[24] empty, so a record
// loaded from before v3 keeps auto ranging when saved again.
// ---------------------------------------------------------------------------
QString HistoryGraphItem::serialize() const
{
    const auto manual = [this](float v) {
        return m_manualRange ? QString::number(static_cast<double>(v)) : QString();
    };
    return QStringLiteral("HISTORY|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14|%15|%16|%17|%18|%19|%20|%21|%22|%23|%24")
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
        .arg(m_bindingId1)
        .arg(m_durationMs)  // Task 3.3: new in v2
        .arg(static_cast<double>(m_verticalRatio))  // v3 from here
        .arg(m_backColour.name(QColor::HexArgb))
        .arg(m_linesColour.name(QColor::HexArgb))
        .arg(m_timeColour.name(QColor::HexArgb))
        .arg(manual(m_min0Manual))
        .arg(manual(m_max0Manual))
        .arg(manual(m_min1Manual))
        .arg(manual(m_max1Manual))
        // NereusSDR v4: appended, never renumbered.
        + QLatin1Char('|') + QString::number(m_hasOwnLength ? 1 : 0);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts:
// v1 (16 fields): [0]=HISTORY [1]=x [2]=y [3]=w [4]=h [5]=bindingId [6]=zOrder
//                 [7]=capacity [8]=lineColor0 [9]=lineColor1
//                 [10]=showGrid [11]=autoScale0 [12]=autoScale1
//                 [13]=showScale0 [14]=showScale1 [15]=bindingId1
// v2 (17 fields): ...all v1... [16]=durationMs
// v3 (25 fields): ...all v2... [17]=verticalRatio [18]=backColour
//                 [19]=linesColour [20]=timeColour [21]=min0 [22]=max0
//                 [23]=min1 [24]=max1 (min/max empty: no manual range)
// v4 (26 fields): ...all v3... [25]=hasOwnLength (0/1)
// An 18-field record (v2 plus hasOwnLength at [17]) was written by a build
// that had the own-length field before the v3 fields; it loads with the
// older look and its own-length flag.
// Task 3.3: v1 to v4 all load, for backward compatibility; v1, v2 and
// 18-field records keep their look (older colours, no manual range).
// ---------------------------------------------------------------------------
bool HistoryGraphItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    // Accept 16 parts (v1), 17 parts (v2), 18 parts (own length, before
    // v3), 25 parts (v3) or 26 parts (v4)
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
    int capacity = parts[7].toInt(&ok);  if (!ok || capacity < 2) { return false; }
    // NereusSDR: a capacity above the largest NereusSDR produces (a pasted
    // or hand-edited record) loads at that bound instead of allocating it.
    capacity = std::min(capacity, kMaxCapacity);

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

    // Task 3.3: optional durationMs (v2 format)
    int durationMs = m_durationMs;  // default to current value
    if (parts.size() >= 17) {
        durationMs = parts[16].toInt(&ok);  if (!ok) { durationMs = m_durationMs; }
    }
    // NereusSDR: a record without the field never set its own length. v4
    // holds it at [25]; an 18-field record (before the v3 fields) at [17].
    bool hasOwnLength = false;
    if (parts.size() >= 26) {
        hasOwnLength = (parts[25].toInt(&ok) != 0) && ok;
    } else if (parts.size() == 18) {
        hasOwnLength = (parts[17].toInt(&ok) != 0) && ok;
    }

    setRect(x, y, w, h);
    setBindingId(bindingId);
    setZOrder(zOrder);

    m_lineColor0 = lineColor0;
    m_lineColor1 = lineColor1;
    m_showGrid   = (showGrid   != 0);
    m_autoScale0 = (autoScale0 != 0);
    m_autoScale1 = (autoScale1 != 0);
    m_showScale0 = (showScale0 != 0);
    m_showScale1 = (showScale1 != 0);
    m_bindingId1 = bindingId1;

    // v3 options. A field that does not parse keeps the older look.
    m_verticalRatio = kHistoryThetisDefaults.verticalRatio;
    m_backColour    = QColor::fromRgb(kLegacyBackColour);
    m_linesColour   = QColor::fromRgb(kLegacyLinesColour);
    m_timeColour    = QColor::fromRgb(kLegacyTimeColour);
    m_manualRange   = false;
    if (parts.size() >= 25) {
        const float ratio = parts[17].toFloat(&ok);
        if (ok) { setVerticalRatio(ratio); }
        const QColor back(parts[18]);
        if (back.isValid()) { m_backColour = back; }
        const QColor lines(parts[19]);
        if (lines.isValid()) { m_linesColour = lines; }
        const QColor time(parts[20]);
        if (time.isValid()) { m_timeColour = time; }
        bool okMin0 = false, okMax0 = false, okMin1 = false, okMax1 = false;
        const float min0 = parts[21].toFloat(&okMin0);
        const float max0 = parts[22].toFloat(&okMax0);
        const float min1 = parts[23].toFloat(&okMin1);
        const float max1 = parts[24].toFloat(&okMax1);
        if (okMin0 && okMax0 && okMin1 && okMax1) {
            setManualMin(0, min0);
            setManualMax(0, max0);
            setManualMin(1, min1);
            setManualMax(1, max1);
        }
    }

    // Task 3.3: apply duration (which updates capacity via setDurationMs)
    setDurationMs(durationMs);
    // The saved capacity is applied after the duration, so it loads as
    // written rather than as the capacity the duration implies.
    setCapacity(capacity);
    m_hasOwnLength = hasOwnLength;

    return true;
}

} // namespace NereusSDR
