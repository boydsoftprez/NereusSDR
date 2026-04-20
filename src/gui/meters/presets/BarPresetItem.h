// =================================================================
// src/gui/meters/presets/BarPresetItem.h  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources (3 flavours: Alc / AlcGain /
// AlcGroup) and NereusSDR ItemGroup factories (15 flavours: Mic /
// Comp / Cfc / CfcGain / Leveler / LevelerGain / Agc / AgcGain /
// Pbsnr / Eq / SignalBar / AvgSignalBar / MaxBinBar / AdcBar /
// AdcMaxMag). The Thetis-sourced flavours port byte-for-byte from
// Project Files/Source/Console/MeterManager.cs
// (AddALCBar / AddALCGainBar / AddALCGroupBar, lines
// 23326 / 23412 / 23473 @501e3f5). The NereusSDR-local flavours
// collapse the matching `ItemGroup::create*BarPreset()` factories
// (ItemGroup.cpp:857-1014). The Custom flavour is a generic
// caller-parameterised pass-through and has no upstream provenance.
//
// The Thetis MeterManager.cs header is reproduced verbatim once
// below for the 3 Thetis-sourced flavours collectively; the .cpp
// repeats the same block in front of the per-flavour configureAs*
// method bodies.
//
// =================================================================

// --- From MeterManager.cs ---
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

// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Reimplemented in C++20/Qt6 for NereusSDR by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Claude Opus 4.7. One first-class MeterItem
//                subclass collapsing 18 ItemGroup::create*BarPreset()
//                factories — flavour is selected via an enum and
//                configureAs*(). The 3 Thetis-sourced flavours
//                (Alc / AlcGain / AlcGroup) port their min/max and
//                red-threshold constants byte-for-byte from Thetis
//                MeterManager.cs (AddALC*Bar). The 15 NereusSDR-local
//                flavours mirror the ItemGroup factory
//                min/max/binding triples; those factories themselves
//                originally chain-cite Thetis AddMicBar / AddCompBar /
//                AddEQBar / AddLevelerBar / AddCFCBar / AddADCBar /
//                AddADCMaxMag, and the chain is preserved on each
//                configureAs* method's setCommon() call.
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"

#include <QColor>
#include <QString>

namespace NereusSDR {

// BarPresetItem — parameterised bar-row preset.
//
// Collapses 18 near-identical ItemGroup bar-row factories into one
// configurable MeterItem. The flavour enum selects one of 18
// canned configurations (3 Thetis-sourced ALC variants + 15
// NereusSDR-local bar-row presets) or a Custom caller-parameterised
// variant; each configureAs*() method sets the binding ID, value
// range, default label, and colour/threshold defaults appropriate
// for that flavour.
//
// Serialization round-trips binding / min / max / label / colour /
// red-threshold independently of the flavour so Task 11's property
// editor can override any one of those fields without having to
// introduce a new flavour enumerator.
class BarPresetItem : public MeterItem
{
    Q_OBJECT

public:
    // 18 canonical flavours + 1 user-parameterised Custom flavour.
    enum class Kind {
        Mic,
        Alc,
        AlcGain,
        AlcGroup,
        Comp,
        Cfc,
        CfcGain,
        Leveler,
        LevelerGain,
        Agc,
        AgcGain,
        Pbsnr,
        Eq,
        SignalBar,
        AvgSignalBar,
        MaxBinBar,
        AdcBar,
        AdcMaxMag,
        Custom
    };

    explicit BarPresetItem(QObject* parent = nullptr);

    // --- MeterItem overrides ---
    // Background -> MeterWidget calls paintForLayer() -> paint();
    // primitives (BarItem / NeedleItem / etc.) on Geometry /
    // OverlayDynamic paint on top. This matches the user's mental
    // model that a preset is the "background meter" the container
    // shows, with the in-use list's item order driving z-order
    // (top of list = behind, bottom = front). Preset classes
    // override paint() only (not emitVertices()), so Layer::Geometry
    // would route through the empty GPU vertex path.
    Layer renderLayer() const override { return Layer::Background; }
    void  paint(QPainter& p, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool    deserialize(const QString& data) override;

    // --- Configure helpers (one per flavour) ---
    // Each method sets (kind, binding, min, max, label, bar colour,
    // red-threshold). Thetis-sourced flavours cite upstream on their
    // setCommon() call; NereusSDR-local flavours cite the backing
    // ItemGroup factory (which may itself chain-cite Thetis).
    void configureAsMic();
    void configureAsAlc();
    void configureAsAlcGain();
    void configureAsAlcGroup();
    void configureAsComp();
    void configureAsCfc();
    void configureAsCfcGain();
    void configureAsLeveler();
    void configureAsLevelerGain();
    void configureAsAgc();
    void configureAsAgcGain();
    void configureAsPbsnr();
    void configureAsEq();
    void configureAsSignalBar();
    void configureAsAvgSignalBar();
    void configureAsMaxBinBar();
    void configureAsAdcBar();
    void configureAsAdcMaxMag();
    void configureAsCustom(int bindingId, double minV, double maxV,
                           const QString& label);

    // --- Read-only accessors (used by tests + Task 11 editor) ---
    Kind    presetKind()    const { return m_kind; }
    double  minValue()      const { return m_minValue; }
    double  maxValue()      const { return m_maxValue; }
    QString label()         const { return m_label; }
    QColor  barColor()      const { return m_barColor; }
    QColor  backdropColor() const { return m_backdropColor; }
    double  redThreshold()  const { return m_redThreshold; }

    // --- Setters (Task 11 property editor + serialization overrides) ---
    void setMinValue(double v)            { m_minValue = v; }
    void setMaxValue(double v)            { m_maxValue = v; }
    void setLabel(const QString& l)       { m_label = l; }
    void setBarColor(const QColor& c)     { m_barColor = c; }
    void setBackdropColor(const QColor& c){ m_backdropColor = c; }
    void setRedThreshold(double t)        { m_redThreshold = t; }

    // --- Enum <-> string round-trip (used in the serialized blob) ---
    QString kindString() const;
    static Kind kindFromString(const QString& s, Kind fallback = Kind::Custom);

private:
    // Central helper: sets every field identified by the flavour.
    // Called from each configureAs* with the flavour-specific values.
    void setCommon(Kind k, int binding, double minV, double maxV,
                   const QString& label, const QColor& barColor,
                   double redThreshold);

    Kind    m_kind          = Kind::Custom;
    double  m_minValue      = 0.0;
    double  m_maxValue      = 1.0;
    QString m_label;
    QColor  m_barColor      {255, 255, 255, 255};       // Thetis bar low-colour default = white
    QColor  m_backdropColor {32, 32, 32, 255};           // Thetis backdrop Color.FromArgb(32,32,32)
    double  m_redThreshold  = 0.0;                       // 0 = disabled; bar turns red above this
};

} // namespace NereusSDR
