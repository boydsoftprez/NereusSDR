// =================================================================
// src/core/HardwareProfile.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/clsHardwareSpecific.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/NetworkIO.cs (upstream has no top-of-file header — project-level LICENSE applies)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  clsHardwareSpecific.cs

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

//
// WORK IN PROGRESS
//

//
// Upstream source 'Project Files/Source/Console/HPSDR/NetworkIO.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

#pragma once

#include "HpsdrModel.h"
#include "BoardCapabilities.h"
#include <QList>

namespace NereusSDR {

struct HardwareProfile {
    HPSDRModel               model{HPSDRModel::HERMES};
    HPSDRHW                  effectiveBoard{HPSDRHW::Hermes};
    const BoardCapabilities* caps{nullptr};
    int                      adcCount{1};
    bool                     mkiiBpf{false};
    int                      adcSupplyVoltage{33};
    bool                     lrAudioSwap{true};
};

// Compute a HardwareProfile for the given model.
// From Thetis clsHardwareSpecific.cs:85-184
HardwareProfile profileForModel(HPSDRModel model);

// Return the default (auto-guessed) HPSDRModel for a discovered board byte.
HPSDRModel defaultModelForBoard(HPSDRHW board);

// Return the list of HPSDRModel values compatible with a discovered board byte.
// From Thetis NetworkIO.cs:164-171
QList<HPSDRModel> compatibleModels(HPSDRHW board);

} // namespace NereusSDR
