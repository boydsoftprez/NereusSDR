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
//   Project Files/Source/Console/clsBandStackManager.cs:1360-1372 (the
//   US region's SWL frequency data) and :2236-2303 (AddBandStackSWL),
//   [v2.10.3.15] — the SWL band ranges and seeds.
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
//   2026-10-26 - SWL band seeds (120m to 11m) from AddBandStackSWL and the
//                 SWL band ranges from the US region's frequency data, for
//                 the band grid's GEN group. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
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
namespace {

// From Thetis clsBandStackManager.cs:2246-2303 [v2.10.3.15] (AddBandStackSWL):
// the first entry of each SWL band, every one "SAM", "F4".
constexpr DSPMode kSwlMode = DSPMode::SAM;
// From Thetis clsBandStackManager.cs:2246 [v2.10.3.15]: "120M", "SAM", "F4", 2.400000
constexpr double kSwl120mHz = 2400000.0;
// From Thetis clsBandStackManager.cs:2250 [v2.10.3.15]: "90M", "SAM", "F4", 3.300000
constexpr double kSwl90mHz = 3300000.0;
// From Thetis clsBandStackManager.cs:2254 [v2.10.3.15]: "61M", "SAM", "F4", 4.700000
constexpr double kSwl61mHz = 4700000.0;
// From Thetis clsBandStackManager.cs:2258 [v2.10.3.15]: "49M", "SAM", "F4", 5.600000
constexpr double kSwl49mHz = 5600000.0;
// From Thetis clsBandStackManager.cs:2266 [v2.10.3.15]: "41M", "SAM", "F4", 7.310000
constexpr double kSwl41mHz = 7310000.0;
// From Thetis clsBandStackManager.cs:2271 [v2.10.3.15]: "31M", "SAM", "F4", 9.100000
constexpr double kSwl31mHz = 9100000.0;
// From Thetis clsBandStackManager.cs:2279 [v2.10.3.15]: "25M", "SAM", "F4", 11.700000
constexpr double kSwl25mHz = 11700000.0;
// From Thetis clsBandStackManager.cs:2283 [v2.10.3.15]: "22M", "SAM", "F4", 13.600000
constexpr double kSwl22mHz = 13600000.0;
// From Thetis clsBandStackManager.cs:2287 [v2.10.3.15]: "19M", "SAM", "F4", 15.200000
constexpr double kSwl19mHz = 15200000.0;
// From Thetis clsBandStackManager.cs:2291 [v2.10.3.15]: "16M", "SAM", "F4", 17.500000
constexpr double kSwl16mHz = 17500000.0;
// From Thetis clsBandStackManager.cs:2295 [v2.10.3.15]: "14M", "SAM", "F4", 18.900000
constexpr double kSwl14mHz = 18900000.0;
// From Thetis clsBandStackManager.cs:2299 [v2.10.3.15]: "13M", "SAM", "F4", 21.500000
constexpr double kSwl13mHz = 21500000.0;
// From Thetis clsBandStackManager.cs:2303 [v2.10.3.15]: "11M", "SAM", "F4", 25.700000
constexpr double kSwl11mHz = 25700000.0;

struct SwlRange {
    Band band;
    double lowHz;
    double highHz;
};

// From Thetis clsBandStackManager.cs:1360-1372 [v2.10.3.15] (case
// FRSRegion.US, the SWL BandFrequencyData rows, low and high in MHz):
// NereusSDR's band edges are Region 2's, as the seeds above are.
constexpr SwlRange kSwlRanges[] = {
    {Band::Band120m,  1800000.0,  3000000.0},  // 1.8, 3.0
    {Band::Band90m,   3000000.0,  4100000.0},  // 3.0, 4.1
    {Band::Band61m,   4100000.0,  5060000.0},  // 4.1, 5.06
    {Band::Band49m,   5060000.0,  7200000.0},  // 5.06, 7.2
    {Band::Band41m,   7200000.0,  9000000.0},  // 7.2, 9
    {Band::Band31m,   9000000.0, 11600000.0},  // 9.0, 11.6
    {Band::Band25m,  11600000.0, 13570000.0},  // 11.6, 13.57
    {Band::Band22m,  13570000.0, 13870000.0},  // 13.57, 13.87
    {Band::Band19m,  13870000.0, 17000000.0},  // 13.87, 17.0
    {Band::Band16m,  17000000.0, 18000000.0},  // 17, 18.0
    {Band::Band14m,  18000000.0, 21000000.0},  // 18.0, 21.0
    {Band::Band13m,  21000000.0, 25000000.0},  // 21.0, 25
    {Band::Band11m,  25000000.0, 28000000.0},  // 25.0, 28.0
};

} // namespace

Band bandGridBandFromFrequency(double hz)
{
    // The ham bands and WWV come first, as they do in bandFromFrequency.
    const Band band = bandFromFrequency(hz);
    if (band != Band::GEN) { return band; }
    // From Thetis clsBandStackManager.cs:1240 [v2.10.3.15]:
    //   if (freq >= low && freq <= high) - the first row that holds it.
    for (const SwlRange& range : kSwlRanges) {
        if (hz >= range.lowHz && hz <= range.highHz) { return range.band; }
    }
    return Band::GEN;
}

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
        // The SWL bands: the first entry of each band's list in
        // AddBandStackSWL, which Thetis adds in every region ("// add to
        // all", clsBandStackManager.cs:1156 [v2.10.3.15]). All are SAM.
        case Band::Band120m: return { Band::Band120m, kSwl120mHz, kSwlMode, true };
        case Band::Band90m:  return { Band::Band90m,  kSwl90mHz,  kSwlMode, true };
        case Band::Band61m:  return { Band::Band61m,  kSwl61mHz,  kSwlMode, true };
        case Band::Band49m:  return { Band::Band49m,  kSwl49mHz,  kSwlMode, true };
        case Band::Band41m:  return { Band::Band41m,  kSwl41mHz,  kSwlMode, true };
        case Band::Band31m:  return { Band::Band31m,  kSwl31mHz,  kSwlMode, true };
        case Band::Band25m:  return { Band::Band25m,  kSwl25mHz,  kSwlMode, true };
        case Band::Band22m:  return { Band::Band22m,  kSwl22mHz,  kSwlMode, true };
        case Band::Band19m:  return { Band::Band19m,  kSwl19mHz,  kSwlMode, true };
        case Band::Band16m:  return { Band::Band16m,  kSwl16mHz,  kSwlMode, true };
        case Band::Band14m:  return { Band::Band14m,  kSwl14mHz,  kSwlMode, true };
        case Band::Band13m:  return { Band::Band13m,  kSwl13mHz,  kSwlMode, true };
        case Band::Band11m:  return { Band::Band11m,  kSwl11mHz,  kSwlMode, true };
        case Band::Count:    break;
    }
    return { Band::GEN, 0.0, DSPMode::USB, false };
}

} // namespace BandDefaults
} // namespace NereusSDR
