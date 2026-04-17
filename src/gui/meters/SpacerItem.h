#pragma once

// =================================================================
// src/gui/meters/SpacerItem.h  (NereusSDR)
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

// From Thetis MeterManager.cs:16116 — clsSpacerItem
// Solid or gradient background fill rectangle.

#include "MeterItem.h"
#include <QColor>

namespace NereusSDR {

class SpacerItem : public MeterItem {
    Q_OBJECT
public:
    explicit SpacerItem(QObject* parent = nullptr);

    // From Thetis MeterManager.cs:16143 — Colour1 / Colour2 / Padding
    void setColour1(const QColor& c) { m_colour1 = c; }
    QColor colour1() const { return m_colour1; }

    void setColour2(const QColor& c) { m_colour2 = c; }
    QColor colour2() const { return m_colour2; }

    void setPadding(float p) { m_padding = p; }
    float padding() const { return m_padding; }

    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    // From Thetis MeterManager.cs:16123 — default Color.FromArgb(32, 32, 32)
    QColor m_colour1{0x20, 0x20, 0x20};
    QColor m_colour2{0x20, 0x20, 0x20};
    // From Thetis MeterManager.cs:16126 — default 0.1f
    float  m_padding{0.1f};
};

} // namespace NereusSDR
