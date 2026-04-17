#pragma once
// =================================================================
// src/gui/meters/ClockItem.h  (NereusSDR)
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
#include <QTimer>

namespace NereusSDR {
// Dual UTC/Local time display. From Thetis clsClock (MeterManager.cs:14075+).
class ClockItem : public MeterItem {
    Q_OBJECT
public:
    explicit ClockItem(QObject* parent = nullptr);

    void setShow24Hour(bool v) { m_show24Hour = v; }
    bool show24Hour() const { return m_show24Hour; }
    void setShowType(bool v) { m_showType = v; }
    bool showType() const { return m_showType; }

    void setTimeColour(const QColor& c) { m_timeColour = c; }
    QColor timeColour() const { return m_timeColour; }
    void setDateColour(const QColor& c) { m_dateColour = c; }
    QColor dateColour() const { return m_dateColour; }
    void setTypeTitleColour(const QColor& c) { m_typeTitleColour = c; }
    QColor typeTitleColour() const { return m_typeTitleColour; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    bool m_show24Hour{true};
    bool m_showType{true};
    QColor m_timeColour{0xc8, 0xd8, 0xe8};
    QColor m_dateColour{0x80, 0x90, 0xa0};
    QColor m_typeTitleColour{0x70, 0x80, 0x90};
    QTimer m_updateTimer;
};
} // namespace NereusSDR
