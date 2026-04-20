// =================================================================
// src/gui/meters/presets/PowerSwrPresetItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//   (AddPWRBar, line 23854/23862; AddSWRBar, line 23990)
//   verbatim upstream header reproduced below; the .cpp carries the
//   same block in front of the calibration tables.
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
//                via Claude Opus 4.7. First-class MeterItem subclass
//                collapsing the Power + SWR dual-bar composite
//                (backdrop + power bar 0..150W + SWR bar 1..10 +
//                per-bar scale + per-bar readout) into one entity.
//                Power range (150W) is NereusSDR's choice — Thetis
//                AddPWRBar uses 120W for its default P=100 scale; we
//                align to the existing createPowerSwrPreset factory
//                (0..150W) so the visual match with the previous
//                container contents is preserved during the refactor.
// =================================================================
#pragma once

#include "gui/meters/MeterItem.h"

#include <QColor>
#include <QString>
#include <array>

namespace NereusSDR {

// PowerSwrPresetItem — first-class Power + SWR dual-bar composite.
//
// Collapses the backdrop + Power bar + Power scale + Power readout +
// SWR bar + SWR scale + SWR readout emitted by
// `ItemGroup::createPowerSwrPreset()` into one MeterItem. Bindings
// mirror Thetis (TxPower / TxSwr).
class PowerSwrPresetItem : public MeterItem
{
    Q_OBJECT

public:
    static constexpr int kBarCount = 2;

    explicit PowerSwrPresetItem(QObject* parent = nullptr);

    // --- MeterItem overrides ---
    Layer renderLayer() const override { return Layer::Geometry; }
    void  paint(QPainter& p, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool    deserialize(const QString& data) override;

    // --- Bar introspection ---
    int     barCount() const { return kBarCount; }
    QString barName(int i) const;
    int     barBindingId(int i) const;
    double  barMinVal(int i) const;
    double  barMaxVal(int i) const;

    // --- Readout toggle ---
    void setShowReadout(bool on) { m_showReadout = on; }
    bool showReadout() const { return m_showReadout; }

private:
    struct Bar {
        QString label;
        int     bindingId{-1};
        double  minVal{0.0};
        double  maxVal{1.0};
        double  redThreshold{1.0};
        QString suffix;
    };

    std::array<Bar, kBarCount> m_bars;
    QColor m_backdropColor{0x0f, 0x0f, 0x1a};
    QColor m_barColor{0x00, 0xb4, 0xd8};
    QColor m_barRedColor{0xff, 0x44, 0x44};
    QColor m_readoutColor{0xc8, 0xd8, 0xe8};
    bool   m_showReadout{true};
};

} // namespace NereusSDR
