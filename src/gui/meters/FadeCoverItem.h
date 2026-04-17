#pragma once

// =================================================================
// src/gui/meters/FadeCoverItem.h  (NereusSDR)
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

// From Thetis MeterManager.cs:7665 — clsFadeCover / renderFadeCover (line 36292)
// Semi-transparent overlay that activates on RX or TX state.

#include "MeterItem.h"
#include <QColor>

namespace NereusSDR {

class FadeCoverItem : public MeterItem {
    Q_OBJECT
public:
    explicit FadeCoverItem(QObject* parent = nullptr);

    // From Thetis MeterManager.cs:1900 — FadeOnRx / FadeOnTx
    void setFadeOnRx(bool on) { m_fadeOnRx = on; }
    bool fadeOnRx() const { return m_fadeOnRx; }

    void setFadeOnTx(bool on) { m_fadeOnTx = on; }
    bool fadeOnTx() const { return m_fadeOnTx; }

    // Overlay colours (gradient if they differ, solid if equal)
    void setColour1(const QColor& c) { m_colour1 = c; }
    QColor colour1() const { return m_colour1; }

    void setColour2(const QColor& c) { m_colour2 = c; }
    QColor colour2() const { return m_colour2; }

    // Alpha of the overlay when active (0-1). Default 0.75.
    void setAlpha(float a) { m_alpha = a; }
    float alpha() const { return m_alpha; }

    // From Thetis MeterManager.cs:7671 — ZOrder = int.MaxValue (always on top)
    // Call this when MOX state changes to update m_active.
    // m_active = (isTx && m_fadeOnTx) || (!isTx && m_fadeOnRx)
    void setTxState(bool isTx);

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    QColor m_colour1{0x20, 0x20, 0x20};
    QColor m_colour2{0x20, 0x20, 0x20};
    float  m_alpha{0.75f};
    bool   m_fadeOnRx{false};
    bool   m_fadeOnTx{false};
    bool   m_active{false};
};

} // namespace NereusSDR
