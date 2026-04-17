#pragma once

// =================================================================
// src/gui/meters/HistoryGraphItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

#include "MeterItem.h"
#include <QColor>
#include <vector>

namespace NereusSDR {

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

    explicit HistoryGraphItem(QObject* parent = nullptr);

    // --- Properties ---
    int capacity() const { return m_capacity; }
    void setCapacity(int cap);

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

    // --- Data ---
    void setValue(double v) override;   // axis 0 — called by MeterWidget
    void setValue1(double v);           // axis 1 — called by MeterWidget for bindingId1

    // --- Multi-layer rendering ---
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    bool participatesIn(Layer layer) const override;
    void paint(QPainter& p, int widgetW, int widgetH) override;
    void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) override;

    // --- Serialization ---
    // Tag: HISTORY
    // Format: HISTORY|x|y|w|h|bindingId|zOrder|capacity|lineColor0|lineColor1|
    //         showGrid|autoScale0|autoScale1|showScale0|showScale1|bindingId1
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    void paintGrid(QPainter& p, const QRect& rect);
    void paintLine(QPainter& p, const QRect& rect, const RingBuffer& buf,
                   const QColor& color, float yMin, float yMax);
    void computeScale(const RingBuffer& buf, float& yMin, float& yMax) const;

    int     m_capacity{kDefaultCapacity};
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
};

} // namespace NereusSDR
