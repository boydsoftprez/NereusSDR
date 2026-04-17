#pragma once

// =================================================================
// src/gui/meters/MagicEyeItem.h  (NereusSDR)
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
#include <QImage>

namespace NereusSDR {

// From Thetis clsMagicEyeItem (MeterManager.cs:15855+)
// Vacuum tube magic eye — green phosphor arc opens/closes with signal.
class MagicEyeItem : public MeterItem {
    Q_OBJECT

public:
    explicit MagicEyeItem(QObject* parent = nullptr) : MeterItem(parent) {}

    // S-meter scale constants (from NeedleItem / AetherSDR)
    static constexpr float kS0Dbm  = -127.0f;
    static constexpr float kMaxDbm = -13.0f;
    static constexpr float kSmoothAlpha = 0.3f;

    // Shadow wedge range (degrees)
    static constexpr float kMaxShadowDeg = 120.0f; // no signal — wide open
    static constexpr float kMinShadowDeg = 5.0f;   // full signal — nearly closed

    void setGlowColor(const QColor& c) { m_glowColor = c; }
    QColor glowColor() const { return m_glowColor; }

    void setBezelImagePath(const QString& path);
    QString bezelImagePath() const { return m_bezelPath; }

    void setValue(double v) override;

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    float dbmToFraction(float dbm) const;

    QColor  m_glowColor{0x00, 0xff, 0x88};
    QString m_bezelPath;
    QImage  m_bezelImage;
    float   m_smoothedDbm{kS0Dbm};
};

} // namespace NereusSDR
