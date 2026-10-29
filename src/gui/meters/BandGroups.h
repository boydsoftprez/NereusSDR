#pragma once

// =================================================================
// src/gui/meters/BandGroups.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// The band groups of Thetis clsBandButtonBox (GEN, HF, VHF): which band or
// jump each of the 15 buttons holds in each group, and the group a band
// belongs to. One table, shared by BandButtonItem and NereusBandButtonItem.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Band groups and their layouts ported from Thetis
//                 clsBandButtonBox.setupButtons and GetBandGroupFromBand.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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


#include "gui/NereusGuiExport.h"
#include "models/Band.h"

#include <QVector>

namespace NereusSDR {

// From Thetis MeterManager.cs:234-240 [v2.10.3.15] (enum BandGroups: GEN,
// HF, VHF; its LAST marker has no NereusSDR use).
enum class BandGroup { Gen, Hf, Vhf };

// What one button of a band group holds: a band, nothing (a blank), or a
// jump to another group.
struct BandGroupSlot {
    ::NereusSDR::Band band;
    enum Kind { Band, Blank, JumpGen, JumpHf, JumpVhf } kind;
};

// From Thetis MeterManager.cs:12848 [v2.10.3.15]: Buttons = 15. Every group
// lays out the same 15 buttons, so the grid keeps its size on a jump.
inline constexpr int kBandGroupButtons = 15;

// The 15 buttons of `group`, in grid order (MeterManager.cs:12930-13066
// [v2.10.3.15], setupButtons):
//   GEN: GEN, the SWL bands 120m to 11m, an HF jump at 14.
//   HF:  160m to 6m, a blank at 11, a VHF jump at 12, WWV at 13, an SWL
//        jump (to GEN) at 14.
//   VHF: 2m, XVTR, blanks, an HF jump at 14.
NEREUS_GUI_EXPORT const QVector<BandGroupSlot>& bandGroupLayout(BandGroup group);

// The group whose layout holds `band` (Thetis GetBandGroupFromBand,
// MeterManager.cs:25864-25916 [v2.10.3.15]).
NEREUS_GUI_EXPORT BandGroup bandGroupForBand(Band band);

// The position of `band` in `group`'s layout, or -1 when the group has no
// button for it.
NEREUS_GUI_EXPORT int bandGroupPosition(BandGroup group, Band band);

} // namespace NereusSDR
