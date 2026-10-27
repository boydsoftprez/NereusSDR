// =================================================================
// src/models/BandDefaults.h  (NereusSDR)
// =================================================================
//
// Seed lookup table for per-band default freq + demod mode used on the
// first band-button click before per-band persistence has been written.
//
// Issue #118. Spec: docs/architecture/band-button-auto-mode-design.md.
//
// Ported from Thetis source:
//   Project Files/Source/Console/clsBandStackManager.cs:2099-2176
//   (AddRegion2BandStack, v2.10.3.13 @ 501e3f51) — US Region 2 seeds.
//
// Policy: single-entry seed picks the first LSB/USB entry from Thetis's
// multi-entry stack, not index 0 (which is CW for most bands). This is
// a deliberate NereusSDR UI choice that matches the #118 reporter's
// expectation of SSB on first click. Frequencies are verbatim from
// Thetis (source-first on constants).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — New file for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-10-26 - bandGridBandFromFrequency, and SWL seeds (see the .cpp).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

//=================================================================
// clsBandStackManager.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2019-2026  Richard Samphire (MW0LGE)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include "../core/WdspTypes.h"
#include "Band.h"

namespace NereusSDR {

/// Seed entry for a single band: the default VFO frequency and DSP mode
/// applied by RadioModel::onBandButtonClicked(Band) when the slice has
/// never been on `band` before. `valid == false` means "no seed for this
/// band" (only XVTR — its seed becomes meaningful once transverter
/// config ships).
struct BandSeed {
    Band    band;
    double  frequencyHz;
    DSPMode mode;
    bool    valid;
};

namespace BandDefaults {

/// Returns the seed entry for `b`. `seedFor(Band::XVTR)` returns
/// `valid == false` — callers must handle the XVTR no-op path.
BandSeed seedFor(Band b);

/// The band a band button grid takes `hz` to be in: the ham band or WWV
/// (bandFromFrequency), otherwise the SWL band whose range holds it (Thetis
/// US region frequency data), otherwise GEN. The band click and the grid's
/// lit band use it, so a slice on an SWL band clicks and saves as that
/// band, not as GEN.
Band bandGridBandFromFrequency(double hz);

} // namespace BandDefaults
} // namespace NereusSDR
