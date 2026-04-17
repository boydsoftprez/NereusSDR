#pragma once

// =================================================================
// src/gui/meters/DialItem.h  (NereusSDR)
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

namespace NereusSDR {

// From Thetis clsDialDisplay (MeterManager.cs:15399+)
// Circular dial with quadrant buttons for VFO/ACCEL/LOCK.
class DialItem : public MeterItem {
    Q_OBJECT

public:
    enum class SpeedState { Slow, Hold, Fast };

    explicit DialItem(QObject* parent = nullptr) : MeterItem(parent) {}

    // Colors (from Thetis clsDialDisplay properties)
    void setTextColour(const QColor& c) { m_textColour = c; }
    QColor textColour() const { return m_textColour; }

    void setCircleColour(const QColor& c) { m_circleColour = c; }
    QColor circleColour() const { return m_circleColour; }

    void setPadColour(const QColor& c) { m_padColour = c; }
    QColor padColour() const { return m_padColour; }

    void setRingColour(const QColor& c) { m_ringColour = c; }
    QColor ringColour() const { return m_ringColour; }

    void setButtonOnColour(const QColor& c) { m_btnOnColour = c; }
    QColor buttonOnColour() const { return m_btnOnColour; }

    void setButtonOffColour(const QColor& c) { m_btnOffColour = c; }
    QColor buttonOffColour() const { return m_btnOffColour; }

    void setButtonHighlightColour(const QColor& c) { m_btnHighlight = c; }
    QColor buttonHighlightColour() const { return m_btnHighlight; }

    void setSlowColour(const QColor& c) { m_slowColour = c; }
    QColor slowColour() const { return m_slowColour; }

    void setHoldColour(const QColor& c) { m_holdColour = c; }
    QColor holdColour() const { return m_holdColour; }

    void setFastColour(const QColor& c) { m_fastColour = c; }
    QColor fastColour() const { return m_fastColour; }

    // State
    void setActiveQuadrant(int q) { m_activeQuadrant = q; } // 0-3, -1 = none
    int activeQuadrant() const { return m_activeQuadrant; }

    void setSpeedState(SpeedState s) { m_speedState = s; }
    SpeedState speedState() const { return m_speedState; }

    void setAccelEnabled(bool e) { m_accelEnabled = e; }
    bool accelEnabled() const { return m_accelEnabled; }

    void setLockEnabled(bool e) { m_lockEnabled = e; }
    bool lockEnabled() const { return m_lockEnabled; }

    // Multi-layer
    bool participatesIn(Layer layer) const override;
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) override;
    void paint(QPainter& p, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    void paintStatic(QPainter& p, const QRect& dialRect);
    void paintDynamic(QPainter& p, const QRect& dialRect);
    QRect squareRect(int widgetW, int widgetH) const;

    // Colors (Thetis defaults from clsDialDisplay)
    QColor m_textColour{0xc8, 0xd8, 0xe8};
    QColor m_circleColour{0x1a, 0x2a, 0x3a};
    QColor m_padColour{0x15, 0x20, 0x30};
    QColor m_ringColour{0x40, 0x50, 0x60};
    QColor m_btnOnColour{0x00, 0x70, 0xc0};
    QColor m_btnOffColour{0x30, 0x40, 0x50};
    QColor m_btnHighlight{0x00, 0x90, 0xe0};
    QColor m_slowColour{0x00, 0xff, 0x88};   // green
    QColor m_holdColour{0xff, 0xb8, 0x00};   // amber
    QColor m_fastColour{0xff, 0x44, 0x44};   // red

    // State
    int m_activeQuadrant{0};  // 0=VFOA active by default
    SpeedState m_speedState{SpeedState::Slow};
    bool m_accelEnabled{false};
    bool m_lockEnabled{false};
};

} // namespace NereusSDR
