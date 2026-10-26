#pragma once

// =================================================================
// src/gui/meters/HistoryGraphItem.h  (NereusSDR)
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
//   2026-09-23: R-R3-13: a no-reading sample (isNoMeterReading) is not
//                 pushed, so it moves neither the axis scale nor the line;
//                 sampleCount0() / sampleCount1() read-only accessors.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-10-26 - HISTORY building block: axis 1 is fed through the base
//                 reading path (readingBindings / pushBindingValue, and
//                 data source variable 2 via setSecondaryValue), each axis
//                 keeps its own update interval; vertical ratio, back,
//                 lines and time colours, manual min/max per axis appended
//                 to the record; kHistoryThetisDefaults for new items.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - kMinHistoryKeepForMs / kMaxHistoryKeepForMs (1-1800 s);
//                 readingBindingsBesideMmio for axis 1. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - A graph whose length the operator set in its editor
//                 (hasOwnLength, saved as field [17]) keeps it; the
//                 Multimeter history duration applies only to graphs that
//                 never set one (followGlobalDurationMs). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - kMaxCapacity: deserialize clamps a loaded capacity to the
//                 largest one NereusSDR produces. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - The own-length cite names Thetis clsHistoryItem._keep_for.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - kMaxCapacity follows the 1800 s Keep For maximum
//                 (18001), so a long graph reloads at its saved length.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - hasOwnLength moves to field [25], after this branch's v3
//                 fields [17]..[24]; an 18-field record keeps it at [17].
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "MeterItem.h"
#include "MeterPoller.h"
#include <QColor>
#include <array>
#include <vector>

namespace NereusSDR {

// Option ranges, from the Thetis setup designer.
// From Thetis setup.designer.cs:62576-62580 [v2.10.3.15] (nudHistory_vertical_ratio.Minimum, 130 scale 3)
constexpr float kMinHistoryVerticalRatio = 0.130f;
// From Thetis setup.designer.cs:62571-62575 [v2.10.3.15] (nudHistory_vertical_ratio.Maximum)
constexpr float kMaxHistoryVerticalRatio = 1.0f;
// From Thetis setup.designer.cs:62485-62489 [v2.10.3.15] (nudHistory_axis0_min.Minimum)
constexpr float kMinHistoryAxisMin = -40000.0f;
// From Thetis setup.designer.cs:62480-62484 [v2.10.3.15] (nudHistory_axis0_min.Maximum)
constexpr float kMaxHistoryAxisMin = 200.0f;
// From Thetis setup.designer.cs:62454-62458 [v2.10.3.15] (nudHistory_axis0_max.Minimum)
constexpr float kMinHistoryAxisMax = -200.0f;
// From Thetis setup.designer.cs:62449-62453 [v2.10.3.15] (nudHistory_axis0_max.Maximum)
constexpr float kMaxHistoryAxisMax = 40000.0f;
// From Thetis setup.designer.cs:62443-62447 [v2.10.3.15] (nudHistory_axis0_max.Increment, 1 scale 1; DecimalPlaces = 1)
constexpr double kHistoryAxisStep = 0.1;
// From Thetis setup.designer.cs:62565-62569 [v2.10.3.15] (nudHistory_vertical_ratio.Increment, 1 scale 3; DecimalPlaces = 3)
constexpr double kHistoryVerticalRatioStep = 0.001;
// Keep For, 1 s to 1800 s.
// From Thetis setup.designer.cs:62510-62519 [v2.10.3.15] (nudHistory_keep_for)
constexpr int kMinHistoryKeepForMs = 1 * 1000;
constexpr int kMaxHistoryKeepForMs = 1800 * 1000;

// New HISTORY items (makeEntry) take the clsHistoryItem constructor's values.
// From Thetis MeterManager.cs:17528-17555 [v2.10.3.15]; the update interval
// is the constructor's UpdateInterval = 100 (MeterManager.cs:17571), which
// is also the designer's value (setup.designer.cs:62555-62559).
struct HistoryItemThetisDefaults {
    float verticalRatio;
    QRgb  backColour;
    QRgb  axis0Colour;
    QRgb  axis1Colour;
    QRgb  linesColour;
    QRgb  timeColour;
    int   reading0;
    int   reading1;
    int   keepForMs;
    float min0Manual;
    float max0Manual;
    float min1Manual;
    float max1Manual;
    bool  autoScale0;
    bool  autoScale1;
    bool  showScale1;
    int   updateIntervalMs;
};
inline constexpr HistoryItemThetisDefaults kHistoryThetisDefaults{
    0.5f,                       // _vertical_ratio = 0.5f
    qRgb(0x00, 0x00, 0x00),     // _back_colour = System.Drawing.Color.Black
    qRgb(0xff, 0x00, 0x00),     // _axis0_colour = System.Drawing.Color.Red
    qRgb(0xff, 0xff, 0x00),     // _axis1_colour = System.Drawing.Color.Yellow
    qRgb(0xff, 0xff, 0xff),     // _lines_colour = System.Drawing.Color.White
    qRgb(0x80, 0x80, 0x80),     // _time_colour = System.Drawing.Color.Gray
    MeterBinding::SignalPeak,   // _reading_0 = Reading.SIGNAL_STRENGTH
    MeterBinding::SignalPeak,   // _reading_1 = Reading.SIGNAL_STRENGTH
    60000,                      // _keep_for = 60; // seconds
    -150.0f,                    // _min_0_manual = -150f
    0.0f,                       // _max_0_manual = 0f
    -150.0f,                    // _min_1_manual = -150f
    0.0f,                       // _max_1_manual = 0f
    true,                       // _auto_scale_0 = true
    true,                       // _auto_scale_1 = true
    false,                      // _show_scale_1 = false
    100,                        // UpdateInterval = 100
};

// From Thetis clsHistoryItem (MeterManager.cs:16149+)
// Scrolling time-series graph with dual-axis ring buffer.
// Renders grid in OverlayStatic, line data in OverlayDynamic.

// ---------------------------------------------------------------------------
// RingBuffer — fixed-size O(1) push, ordered read oldest→newest
// NOT Thetis's List+time-cleanup approach.
// ---------------------------------------------------------------------------
struct RingBuffer {
    std::vector<float> data;
    int writeIdx{0};
    int count{0};
    float runMin{1e30f};
    float runMax{-1e30f};

    void resize(int cap) {
        data.assign(cap, 0.0f);
        writeIdx = 0;
        count = 0;
        runMin = 1e30f;
        runMax = -1e30f;
    }

    void push(float val) {
        if (data.empty()) { return; }
        data[writeIdx] = val;
        writeIdx = (writeIdx + 1) % static_cast<int>(data.size());
        if (count < static_cast<int>(data.size())) { ++count; }
        if (val < runMin) { runMin = val; }
        if (val > runMax) { runMax = val; }
    }

    // oldest = index 0, newest = count-1
    float at(int i) const {
        if (data.empty() || count == 0) { return 0.0f; }
        int idx = (writeIdx - count + i);
        if (idx < 0) { idx += static_cast<int>(data.size()); }
        return data[idx % static_cast<int>(data.size())];
    }
};

class HistoryGraphItem : public MeterItem {
    Q_OBJECT

public:
    // From Thetis clsHistoryItem (MeterManager.cs:16149+)
    static constexpr int kDefaultCapacity = 300;  // 30s at 100ms poll
    // NereusSDR: the largest capacity NereusSDR itself produces.
    // setDurationMs at the Keep For maximum (kMaxHistoryKeepForMs,
    // 1800000 ms) gives 1800000 / 100 + 1 = 18001, above the Multimeter
    // history duration's 600000 ms (6001) and the item editor's Capacity
    // range (HistoryGraphItemEditor.cpp, 10..3000). deserialize clamps a
    // loaded capacity to it, so a pasted record cannot ask for a huge
    // buffer.
    static constexpr int kMaxCapacity = kMaxHistoryKeepForMs / 100 + 1;

    explicit HistoryGraphItem(QObject* parent = nullptr);

    // --- Properties ---
    int capacity() const { return m_capacity; }
    void setCapacity(int cap);

    int durationMs() const { return m_durationMs; }
    void setDurationMs(int ms);

    // NereusSDR: true once the operator sets this graph's length in its
    // editor. Thetis keeps a history length on each history item:
    // clsHistoryItem._keep_for, in seconds, default 60 (MeterManager.cs:17485,
    // :17538, KeepFor at :17708 [v2.10.3.15]), loaded per item clamped to
    // 1..86400 (history_keep_for, MeterManager.cs:26937 [v2.10.3.15]).
    // Thetis history items never follow a global duration; following the
    // Multimeter setting until an own length is set is NereusSDR's choice.
    // With an own length, the Multimeter history duration leaves this
    // graph alone.
    bool hasOwnLength() const { return m_hasOwnLength; }
    void setHasOwnLength(bool v) { m_hasOwnLength = v; }

    // Applies the Multimeter history duration (setDurationMs) unless this
    // graph has its own length.
    void followGlobalDurationMs(int ms);

    QColor lineColor0() const { return m_lineColor0; }
    void setLineColor0(const QColor& c) { m_lineColor0 = c; }

    QColor lineColor1() const { return m_lineColor1; }
    void setLineColor1(const QColor& c) { m_lineColor1 = c; }

    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool v) { m_showGrid = v; }

    bool autoScale0() const { return m_autoScale0; }
    void setAutoScale0(bool v) { m_autoScale0 = v; }

    bool autoScale1() const { return m_autoScale1; }
    void setAutoScale1(bool v) { m_autoScale1 = v; }

    bool showScale0() const { return m_showScale0; }
    void setShowScale0(bool v) { m_showScale0 = v; }

    bool showScale1() const { return m_showScale1; }
    void setShowScale1(bool v) { m_showScale1 = v; }

    int bindingId1() const { return m_bindingId1; }
    void setBindingId1(int id) { m_bindingId1 = id; }

    // Thetis VerticalRatio (height compared to width), 0.130 .. 1. Saved
    // for the history composite; the primitive keeps its own rect.
    float verticalRatio() const { return m_verticalRatio; }
    void setVerticalRatio(float ratio);

    QColor backColour() const { return m_backColour; }
    void setBackColour(const QColor& c) { m_backColour = c; }
    // Border and grid lines.
    QColor linesColour() const { return m_linesColour; }
    void setLinesColour(const QColor& c) { m_linesColour = c; }
    // Thetis TimeColour, the time label colour. The primitive has no time
    // axis, so its scale labels take this colour.
    QColor timeColour() const { return m_timeColour; }
    void setTimeColour(const QColor& c) { m_timeColour = c; }

    // Manual range per axis (axis 0 or 1), used while that axis's auto
    // scale is off. A record saved before these fields existed has no
    // manual range and keeps auto ranging (hasManualRange() false).
    float manualMin(int axis) const { return axis == 1 ? m_min1Manual : m_min0Manual; }
    float manualMax(int axis) const { return axis == 1 ? m_max1Manual : m_max0Manual; }
    void setManualMin(int axis, float v);
    void setManualMax(int axis, float v);
    bool hasManualRange() const { return m_manualRange; }

    // The range axis 0 or 1 is drawn against: auto scaled from its samples,
    // or its manual range when auto scale is off.
    void axisRange(int axis, float& yMin, float& yMax) const;

    void applyNewItemDefaults() override;

    // --- Data ---
    void setValue(double v) override;   // axis 0
    void setValue1(double v);           // axis 1

    // Both axes read through the base reading path: readingBindings() is
    // {bindingId(), bindingId1()} (each when set), and pushBindingValue()
    // feeds axis 0 for bindingId() and axis 1 for bindingId1(), both when
    // the two are the same reading. Each axis keeps the item's update
    // interval on its own clock: acceptsUpdate() only records the time
    // MeterWidget delivers at, and pushBindingValue() drops a sample that
    // arrives within the interval of that axis's last one.
    QSet<int> readingBindings() const override;
    // Data source variable 1 feeds axis 0 alone: axis 1 keeps its own
    // reading (bindingId1()) unless variable 2 feeds it.
    QSet<int> readingBindingsBesideMmio() const override;
    void pushBindingValue(int binding, double value) override;
    bool acceptsUpdate(qint64 monotonicMs) override;
    // Data source variable 2 (index 1) feeds axis 1; a non-finite value
    // means the source has no reading and is not plotted.
    void setSecondaryValue(int index, double value) override;

    // Samples held per axis (R-R3-13: no-reading samples are skipped).
    int sampleCount0() const { return m_buf0.count; }
    int sampleCount1() const { return m_buf1.count; }

    // --- Multi-layer rendering ---
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    bool participatesIn(Layer layer) const override;
    void paint(QPainter& p, int widgetW, int widgetH) override;
    void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) override;

    // --- Serialization ---
    // Tag: HISTORY
    // Format: HISTORY|x|y|w|h|bindingId|zOrder|capacity|lineColor0|lineColor1|
    //         showGrid|autoScale0|autoScale1|showScale0|showScale1|bindingId1|durationMs (v2)|
    //         verticalRatio|backColour|linesColour|timeColour|min0|max0|min1|max1 (v3)|
    //         hasOwnLength (v4)
    // (v2 adds durationMs at index [16]; v1 stops at [15]; v3 appends [17]..[24];
    //  v4 adds hasOwnLength 0/1 at [25], and a record without it follows the
    //  Multimeter setting. An 18-field record, written before the v3 fields
    //  joined it, holds hasOwnLength at [17].)
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    void paintGrid(QPainter& p, const QRect& rect);
    void paintLine(QPainter& p, const QRect& rect, const RingBuffer& buf,
                   const QColor& color, float yMin, float yMax);
    void computeScale(const RingBuffer& buf, float& yMin, float& yMax) const;
    bool axisAccepts(int axis);

    int     m_capacity{kDefaultCapacity};
    int     m_durationMs{60000};  // Task 3.3: default 60 s, corresponds to AppSettings default
    bool    m_hasOwnLength{false};
    // From Thetis clsHistoryItem _history_data_list_0 / _history_data_list_1
    RingBuffer m_buf0;
    RingBuffer m_buf1;

    // Colors: cyan for axis 0 (AetherSDR palette), amber for axis 1
    QColor  m_lineColor0{0x00, 0xb4, 0xd8};   // #00b4d8 cyan
    QColor  m_lineColor1{0xff, 0xb8, 0x00};   // #ffb800 amber
    bool    m_showGrid{true};
    bool    m_autoScale0{true};
    bool    m_autoScale1{true};
    bool    m_showScale0{true};
    bool    m_showScale1{false};
    int     m_bindingId1{-1};

    // The look of records saved before these options existed.
    static constexpr QRgb kLegacyBackColour  = qRgb(0x0a, 0x0a, 0x18);
    static constexpr QRgb kLegacyLinesColour = qRgb(0x20, 0x30, 0x40);
    static constexpr QRgb kLegacyTimeColour  = qRgb(0x80, 0x90, 0xa0);

    float   m_verticalRatio{kHistoryThetisDefaults.verticalRatio};
    QColor  m_backColour{QColor::fromRgb(kLegacyBackColour)};
    QColor  m_linesColour{QColor::fromRgb(kLegacyLinesColour)};
    QColor  m_timeColour{QColor::fromRgb(kLegacyTimeColour)};
    float   m_min0Manual{kHistoryThetisDefaults.min0Manual};
    float   m_max0Manual{kHistoryThetisDefaults.max0Manual};
    float   m_min1Manual{kHistoryThetisDefaults.min1Manual};
    float   m_max1Manual{kHistoryThetisDefaults.max1Manual};
    bool    m_manualRange{false};

    // Per-axis update interval clocks (-1: none yet). m_deliveryMs is the
    // time of the delivery in progress, -1 outside one.
    qint64  m_deliveryMs{-1};
    std::array<qint64, 2> m_lastAxisMs{{-1, -1}};
};

} // namespace NereusSDR
