#pragma once

// =================================================================
// src/gui/meters/SignalTextItem.h  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
//   Project Files/Source/Console/MeterManager.cs
//   Project Files/Source/Console/console.cs
//
// Original Thetis copyright and license (preserved per GNU GPL,
// representing the union of contributors across all cited sources):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2020-2026  Richard Samphire (MW0LGE) [MeterManager.cs]
//   Copyright (C) 2017-2019  Chris Codella (W2PA) — console.cs / setup.cs / NetworkIO.cs inline mods
//   Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — Andromeda / Aries / Saturn / ANAN-G2
//   Copyright (C) 2020-2025  Joe (WD5Y) — console.cs UI tweaks / mute / ForeColor
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
//   2026-04-17 — Synthesized in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Combines logic from the Thetis sources
//                 listed above.
// =================================================================

#include "MeterItem.h"
#include <QColor>

namespace NereusSDR {

// From Thetis clsSignalText (MeterManager.cs:20286+)
// Large text signal display with S-units/dBm/uV format switching.
class SignalTextItem : public MeterItem {
    Q_OBJECT

public:
    // From Thetis clsSignalText.Units enum (MeterManager.cs:20288)
    enum class Units { Dbm, SUnits, Uv };

    // From Thetis clsSignalText.BarStyle enum (MeterManager.cs:20294)
    enum class BarStyle { None, Line, SolidFilled, GradientFilled, Segments };

    explicit SignalTextItem(QObject* parent = nullptr);

    void setUnits(Units u) { m_units = u; }
    Units units() const { return m_units; }

    void setShowValue(bool s) { m_showValue = s; }
    bool showValue() const { return m_showValue; }

    void setShowPeakValue(bool s) { m_showPeakValue = s; }
    bool showPeakValue() const { return m_showPeakValue; }

    void setShowType(bool s) { m_showType = s; }
    bool showType() const { return m_showType; }

    void setPeakHold(bool p) { m_peakHold = p; }
    bool peakHold() const { return m_peakHold; }

    void setColour(const QColor& c) { m_colour = c; }
    QColor colour() const { return m_colour; }

    void setPeakValueColour(const QColor& c) { m_peakColour = c; }
    QColor peakValueColour() const { return m_peakColour; }

    void setMarkerColour(const QColor& c) { m_markerColour = c; }
    QColor markerColour() const { return m_markerColour; }

    void setFontFamily(const QString& f) { m_fontFamily = f; }
    QString fontFamily() const { return m_fontFamily; }

    void setFontSize(float s) { m_fontSize = s; }
    float fontSize() const { return m_fontSize; }

    void setBarStyleMode(BarStyle s) { m_barStyle = s; }
    BarStyle barStyleMode() const { return m_barStyle; }

    void setValue(double v) override;

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    // From Thetis console.cs — format helpers
    QString formatDbm(float dbm) const;
    QString formatSUnits(float dbm) const;
    QString formatUv(float dbm) const;
    QString formatValue(float dbm) const;

    // From Thetis Common.UVfromDBM
    static double uvFromDbm(double dbm);

    Units    m_units{Units::Dbm};
    bool     m_showValue{true};
    bool     m_showPeakValue{false};
    bool     m_showType{true};
    bool     m_peakHold{false};
    QColor   m_colour{0xff, 0x00, 0x00};         // Red (from Thetis line 20333)
    QColor   m_peakColour{0xff, 0x00, 0x00};      // Red (from Thetis line 20335)
    QColor   m_markerColour{0xff, 0xff, 0x00};    // Yellow (from Thetis line 20334)
    QString  m_fontFamily{QStringLiteral("Trebuchet MS")};
    float    m_fontSize{20.0f};
    BarStyle m_barStyle{BarStyle::None};

    // Smoothing (from Thetis line 20353-20354)
    float m_attackRatio{0.8f};
    float m_decayRatio{0.2f};
    float m_smoothedDbm{-140.0f};
    float m_peakDbm{-140.0f};
    int   m_peakHoldCounter{0};
};

} // namespace NereusSDR
