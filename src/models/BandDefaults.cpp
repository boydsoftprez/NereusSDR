// =================================================================
// src/models/BandDefaults.cpp  (NereusSDR)
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
//   Thetis cite stamps below use [v2.10.3.13] from the tagged release
//   v2.10.3.13-7-g501e3f51.
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
//   2026-09-28 - 2 m seed (R-IOS-26, R-R3-49). J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
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

#include "BandDefaults.h"

namespace NereusSDR {
namespace BandDefaults {

BandSeed seedFor(Band b)
{
    switch (b) {
        // From Thetis clsBandStackManager.cs:2104 [v2.10.3.13] —
        // first voice entry in AddRegion2BandStack "160M" list.
        case Band::Band160m: return { Band::Band160m,  1840000.0,  DSPMode::LSB, true };
        // From Thetis clsBandStackManager.cs:2109 [v2.10.3.13]
        case Band::Band80m:  return { Band::Band80m,   3650000.0,  DSPMode::LSB, true };
        // From Thetis clsBandStackManager.cs:2114 [v2.10.3.13] —
        // sole uncommented 60m entry; 60m is channelized USB.
        case Band::Band60m:  return { Band::Band60m,   5354000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2120 [v2.10.3.13]
        case Band::Band40m:  return { Band::Band40m,   7152000.0,  DSPMode::LSB, true };
        // From Thetis clsBandStackManager.cs:2123 [v2.10.3.13] —
        // 30m is CW/digital only per FCC §97.305(c); no voice entry exists.
        case Band::Band30m:  return { Band::Band30m,  10107000.0,  DSPMode::CWU, true };
        // From Thetis clsBandStackManager.cs:2130 [v2.10.3.13]
        case Band::Band20m:  return { Band::Band20m,  14155000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2135 [v2.10.3.13]
        case Band::Band17m:  return { Band::Band17m,  18125000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2140 [v2.10.3.13]
        case Band::Band15m:  return { Band::Band15m,  21205000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2145 [v2.10.3.13]
        case Band::Band12m:  return { Band::Band12m,  24931000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2150 [v2.10.3.13]
        case Band::Band10m:  return { Band::Band10m,  28305000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2155 [v2.10.3.13]
        case Band::Band6m:   return { Band::Band6m,   50125000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2160 [v2.10.3.15] -
        // first voice entry of the "2M" list: "2M", "USB", "F6", 144.200000.
        case Band::Band2m:   return { Band::Band2m,  144200000.0,  DSPMode::USB, true };
        // From Thetis clsBandStackManager.cs:2165 [v2.10.3.13] —
        // 10 MHz is mid-list of 5 WWV entries and most commonly usable.
        // Thetis uses synchronous AM (SAM), not plain AM.
        case Band::WWV:      return { Band::WWV,      10000000.0,  DSPMode::SAM, true };
        // From Thetis clsBandStackManager.cs:2168 [v2.10.3.13] —
        // index 0 of GEN stack (SW broadcast). Full GEN sub-band port
        // (LMF/120m/90m/…) deferred to Phase 3H.
        case Band::GEN:      return { Band::GEN,      13845000.0,  DSPMode::SAM, true };
        // XVTR has no intrinsic seed — depends on user-configured
        // transverters. Handler must no-op on XVTR first-visit until
        // the XVTR epic lands.
        case Band::XVTR:     return { Band::XVTR,     0.0,         DSPMode::USB, false };
        // The SWL bands (see Band.h) have no seed yet: like the GEN
        // sub-bands above, their stacks come with the Phase 3H port. They
        // take the no-seed answer below, as before this list existed.
        case Band::Band120m:
        case Band::Band90m:
        case Band::Band61m:
        case Band::Band49m:
        case Band::Band41m:
        case Band::Band31m:
        case Band::Band25m:
        case Band::Band22m:
        case Band::Band19m:
        case Band::Band16m:
        case Band::Band14m:
        case Band::Band13m:
        case Band::Band11m:
        case Band::Count:    break;
    }
    return { Band::GEN, 0.0, DSPMode::USB, false };
}

} // namespace BandDefaults
} // namespace NereusSDR
