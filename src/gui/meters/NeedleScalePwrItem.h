#pragma once

// =================================================================
// src/gui/meters/NeedleScalePwrItem.h  (NereusSDR)
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
#include <QMap>
#include <QPointF>

namespace NereusSDR {

// From Thetis clsNeedleScalePwrItem (MeterManager.cs:14888+)
// Renders power scale text labels at calibration points around needle arcs.
class NeedleScalePwrItem : public MeterItem {
    Q_OBJECT

public:
    explicit NeedleScalePwrItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setLowColour(const QColor& c) { m_lowColour = c; }
    QColor lowColour() const { return m_lowColour; }

    void setHighColour(const QColor& c) { m_highColour = c; }
    QColor highColour() const { return m_highColour; }

    void setFontFamily(const QString& f) { m_fontFamily = f; }
    QString fontFamily() const { return m_fontFamily; }

    void setFontSize(float s) { m_fontSize = s; }
    float fontSize() const { return m_fontSize; }

    void setFontBold(bool b) { m_fontBold = b; }
    bool fontBold() const { return m_fontBold; }

    void setMarks(int m) { m_marks = m; }
    int marks() const { return m_marks; }

    void setShowMarkers(bool s) { m_showMarkers = s; }
    bool showMarkers() const { return m_showMarkers; }

    void setMaxPower(float p) { m_maxPower = p; }
    float maxPower() const { return m_maxPower; }

    void setDarkMode(bool d) { m_darkMode = d; }
    bool darkMode() const { return m_darkMode; }

    // --- CrossNeedle extensions (Phase 3G-4) ---
    // From Thetis AddCrossNeedle() (MeterManager.cs:22817-23002)
    enum class Direction { Clockwise, CounterClockwise };
    void setDirection(Direction d) { m_direction = d; }
    Direction direction() const { return m_direction; }

    void setNeedleOffset(const QPointF& off) { m_needleOffset = off; }
    QPointF needleOffset() const { return m_needleOffset; }

    void setLengthFactor(float f) { m_lengthFactor = f; }
    float lengthFactor() const { return m_lengthFactor; }

    // Scale calibration: value (0-100 normalized) → normalized (x,y) position on gauge face
    void setScaleCalibration(const QMap<float, QPointF>& cal) { m_calibration = cal; }
    QMap<float, QPointF> scaleCalibration() const { return m_calibration; }
    void addCalibrationPoint(float value, float nx, float ny) {
        m_calibration.insert(value, QPointF(nx, ny));
    }

    Layer renderLayer() const override { return Layer::OverlayStatic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    // From Thetis renderNeedleScale (MeterManager.cs:31822-31823)
    static QString tidyPower(float watts, bool useMilliwatts);

    QColor  m_lowColour{0x80, 0x80, 0x80};    // Gray
    QColor  m_highColour{0xff, 0x00, 0x00};    // Red
    QString m_fontFamily{QStringLiteral("Trebuchet MS")};
    float   m_fontSize{20.0f};
    bool    m_fontBold{true};
    int     m_marks{7};
    bool    m_showMarkers{true};
    float   m_maxPower{150.0f};
    bool    m_darkMode{false};
    // CrossNeedle extensions
    Direction m_direction{Direction::Clockwise};
    QPointF   m_needleOffset{0.0, 0.0};
    float     m_lengthFactor{1.0f};
    QMap<float, QPointF> m_calibration;
};

} // namespace NereusSDR
