// =================================================================
// src/gui/widgets/GradientStop.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucLGPicker.cs (private GradColours
//   struct at lines 75-80), original licence from Thetis source is
//   included below.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-10 — Reimplemented in C++20/Qt6 for NereusSDR by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Claude Opus 4.7 (1M context).
//                Phase 3M-5c (Custom Gradient Picker widget).
//                Public POD struct (data model only); the Thetis
//                "highlighted" flag is omitted because in NereusSDR
//                that is widget-internal view state, not data.
// =================================================================
//
// --- From ucLGPicker.cs ---
/*  ucLGPicker.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

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

#pragma once

#include <QColor>

namespace NereusSDR {

/// Single stop in a multi-stop linear gradient. POD storage for
/// GradientPickerWidget. Ports the data fields of Thetis's private
/// GradColours struct (ucLGPicker.cs:75-80 [v2.10.3.13+501e3f51]):
///
///     private struct GradColours {
///         public float percent;
///         public Color color;
///         public bool enabled;
///         public bool highlighted;
///     }
///
/// Thetis's `highlighted` flag is widget-internal view state, not gradient
/// data, so it is dropped from the public struct and tracked separately
/// inside GradientPickerWidget. Equality is defined for round-trip tests.
struct GradientStop
{
    float  percent  {0.0f};   // [0.0, 1.0]; matches Thetis percent (0..1)
    QColor color    {Qt::black};
    bool   enabled  {true};

    bool operator==(const GradientStop& other) const noexcept
    {
        return percent == other.percent
            && color   == other.color
            && enabled == other.enabled;
    }
    bool operator!=(const GradientStop& other) const noexcept
    {
        return !(*this == other);
    }
};

} // namespace NereusSDR
