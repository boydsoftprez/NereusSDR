#pragma once

// =================================================================
// src/gui/meters/TuneStepButtonItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-26 - Thetis's 26 tuning steps (console.cs tune_step_list),
//                 first ten shown on a new grid; old 7-step grids are
//                 remapped on load. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

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

#include "ButtonBoxItem.h"

namespace NereusSDR {

// Tuning step buttons: one per kTuneStepList entry (1 Hz .. 10 MHz).
// Ported from Thetis clsTunestepButtons (MeterManager.cs:8026+).
class TuneStepButtonItem : public ButtonBoxItem {
    Q_OBJECT

public:
    explicit TuneStepButtonItem(QObject* parent = nullptr);

    // From Thetis MeterManager.cs:8040 [v2.10.3.15]: one button per
    // TuneStepList entry (kTuneStepListSize, SliceModel.h).
    static constexpr int kStepCount = 26;

    void setActiveStep(int index);
    /// R-R3-21: the tuning step on button `index`, in Hz: kTuneStepList's
    /// value (1 Hz .. 10 MHz); 0 outside 0..kStepCount-1.
    static int stepHz(int index);
    /// The label on button `index`: the step's name without "Hz"; empty
    /// outside 0..kStepCount-1.
    static QString stepLabel(int index);
    int activeStep() const { return m_activeStep; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    QString serialize() const override;
    bool deserialize(const QString& data) override;

    // The column counts this grid offers (Thetis setup.cs).
    std::pair<int, int> columnRange() const override;

signals:
    void tuneStepSelected(int stepIndex);

private:
    void onButtonClicked(int index, Qt::MouseButton button);
    int m_activeStep{-1};
};

} // namespace NereusSDR
