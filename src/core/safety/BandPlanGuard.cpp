// =================================================================
// src/core/safety/BandPlanGuard.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis [v2.10.3.13 @501e3f5]:
//   Project Files/Source/Console/console.cs
//   Project Files/Source/Console/clsBandStackManager.cs
//   Project Files/Source/Console/setup.designer.cs
//
// Original licences from each Thetis source file are included below,
// verbatim, separated by // --- From [filename] --- markers per
// CLAUDE.md "Byte-for-byte headers and multi-file attribution".
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-25 — Ported to C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
// =================================================================

// --- From console.cs ---
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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
// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// --- From clsBandStackManager.cs ---
/*  clsBandStackManager.cs

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

// --- From setup.designer.cs ---
// (Auto-generated Visual Studio designer file; no upstream copyright header present.
//  Cite stamp v2.10.3.13 @501e3f5 traces back to that designer file in the Thetis repo.
//  The comboFRSRegion 24-region list is sourced from
//  setup.designer.cs:8084-8108 [v2.10.3.13].)

#include "core/safety/BandPlanGuard.h"
#include <array>

namespace NereusSDR::safety {

namespace {

// ---------------------------------------------------------------------------
// Internal data types
// ---------------------------------------------------------------------------

struct ChannelEntry {
    std::int64_t centerHz;
    std::int64_t bwHz;
};

struct BandRange {
    Band      band;
    std::int64_t loHz;
    std::int64_t hiHz;
};

// ---------------------------------------------------------------------------
// 60m channel tables
// Verbatim from Thetis console.cs:2643-2669 [v2.10.3.13] (Init60mChannels).
// Channel constructor signature: Channel(double MHz, int bwHz).
// ---------------------------------------------------------------------------

// UK: 11 channels with per-channel BW ranging 3–12.5 kHz
// (console.cs:2644-2654 [v2.10.3.13]).
constexpr std::array<ChannelEntry, 11> kUkChannels60m{{
    { 5'261'250,  5'500 },   // console.cs:2644 — Channel(5.26125,  5500)
    { 5'280'000,  8'000 },   // console.cs:2645 — Channel(5.2800,   8000)
    { 5'290'250,  3'500 },   // console.cs:2646 — Channel(5.29025,  3500)
    { 5'302'500,  9'000 },   // console.cs:2647 — Channel(5.3025,   9000)
    { 5'318'000, 10'000 },   // console.cs:2648 — Channel(5.3180,  10000)
    { 5'335'500,  5'000 },   // console.cs:2649 — Channel(5.3355,   5000)
    { 5'356'000,  4'000 },   // console.cs:2650 — Channel(5.3560,   4000)
    { 5'368'250, 12'500 },   // console.cs:2651 — Channel(5.36825, 12500)
    { 5'380'000,  4'000 },   // console.cs:2652 — Channel(5.3800,   4000)
    { 5'398'250,  6'500 },   // console.cs:2653 — Channel(5.39825,  6500)
    { 5'405'000,  3'000 },   // console.cs:2654 — Channel(5.4050,   3000)
}};

// US: 5 channels @ 2.8 kHz each (console.cs:2657-2661 [v2.10.3.13]).
constexpr std::array<ChannelEntry, 5> kUsChannels60m{{
    { 5'332'000, 2'800 },   // console.cs:2657 — Channel(5.3320, 2800)
    { 5'348'000, 2'800 },   // console.cs:2658 — Channel(5.3480, 2800)
    { 5'358'500, 2'800 },   // console.cs:2659 — Channel(5.3585, 2800)
    { 5'373'000, 2'800 },   // console.cs:2660 — Channel(5.3730, 2800)
    { 5'405'000, 2'800 },   // console.cs:2661 — Channel(5.4050, 2800)
}};

// Japan 60m: 4.63 MHz discrete allocation.
// (clsBandStackManager.cs:1471 [v2.10.3.13]):
//   BandFrequencyData(4.629995, 4.630005, Band.B60M, BandType.HF, false, region)
// BW = (4.630005 - 4.629995) MHz × 1e6 = 10 Hz — center 4.630000 MHz.
constexpr std::array<ChannelEntry, 1> kJapanChannels60m{{
    { 4'630'000, 10 },   // 4.629995-4.630005 MHz
}};

// ---------------------------------------------------------------------------
// Per-region HF band-edge tables
// Verbatim from Thetis clsBandStackManager.cs:1287-1497 [v2.10.3.13].
// Only HF-class bands are TX-allowed (IsOKToTX excludes BandType.VHF —
// clsBandStackManager.cs:1072-1079 [v2.10.3.13]).
// B60M rows are placeholders; 60m is handled via the channel tables above.
// ---------------------------------------------------------------------------

// US (clsBandStackManager.cs AddRegion2BandStack, Region2 = Region.US)
// clsBandStackManager.cs:1088-1098 [v2.10.3.13].
constexpr std::array<BandRange, 11> kUsBandRanges{{
    { Band::Band160m, 1'800'000,  2'000'000 },
    { Band::Band80m,  3'500'000,  4'000'000 },
    { Band::Band40m,  7'000'000,  7'300'000 },
    { Band::Band30m, 10'100'000, 10'150'000 },
    { Band::Band20m, 14'000'000, 14'350'000 },
    { Band::Band17m, 18'068'000, 18'168'000 },
    { Band::Band15m, 21'000'000, 21'450'000 },
    { Band::Band12m, 24'890'000, 24'990'000 },
    { Band::Band10m, 28'000'000, 29'700'000 },
    { Band::Band6m,  50'000'000, 54'000'000 },
    { Band::Band60m,  5'100'000,  5'500'000 },  // overlaid by kUsChannels60m
}};

// Europe / Region 1 (clsBandStackManager.cs AddRegion1BandStack)
// clsBandStackManager.cs:1104-1116 [v2.10.3.13].
constexpr std::array<BandRange, 11> kEuropeBandRanges{{
    { Band::Band160m, 1'810'000,  2'000'000 },
    { Band::Band80m,  3'500'000,  3'800'000 },
    { Band::Band40m,  7'000'000,  7'200'000 },
    { Band::Band30m, 10'100'000, 10'150'000 },
    { Band::Band20m, 14'000'000, 14'350'000 },
    { Band::Band17m, 18'068'000, 18'168'000 },
    { Band::Band15m, 21'000'000, 21'450'000 },
    { Band::Band12m, 24'890'000, 24'990'000 },
    { Band::Band10m, 28'000'000, 29'700'000 },
    { Band::Band6m,  50'000'000, 52'000'000 },
    { Band::Band60m,  5'100'000,  5'500'000 },
}};

// United Kingdom (clsBandStackManager.cs AddRegion1BandStack with UK edits)
// clsBandStackManager.cs:1409-1435 [v2.10.3.13].
constexpr std::array<BandRange, 11> kUkBandRanges{{
    { Band::Band160m, 1'810'000,  2'000'000 },
    { Band::Band80m,  3'500'000,  3'800'000 },
    { Band::Band40m,  7'000'000,  7'200'000 },
    { Band::Band30m, 10'100'000, 10'150'000 },
    { Band::Band20m, 14'000'000, 14'350'000 },
    { Band::Band17m, 18'068'000, 18'168'000 },
    { Band::Band15m, 21'000'000, 21'450'000 },
    { Band::Band12m, 24'890'000, 24'990'000 },
    { Band::Band10m, 28'000'000, 29'700'000 },
    { Band::Band6m,  50'030'000, 52'000'000 },
    { Band::Band60m,  5'250'000,  5'410'000 },  // overlaid by kUkChannels60m
}};

// Japan (clsBandStackManager.cs:1467-1480 [v2.10.3.13]).
constexpr std::array<BandRange, 11> kJapanBandRanges{{
    { Band::Band160m, 1'830'000,  1'912'500 },
    { Band::Band80m,  3'500'000,  3'805'000 },
    { Band::Band60m,  4'629'995,  4'630'005 },  // overlaid by kJapanChannels60m
    { Band::Band40m,  6'975'000,  7'200'000 },
    { Band::Band30m, 10'100'000, 10'150'000 },
    { Band::Band20m, 14'000'000, 14'350'000 },
    { Band::Band17m, 18'068'000, 18'168'000 },
    { Band::Band15m, 21'000'000, 21'450'000 },
    { Band::Band12m, 24'890'000, 24'990'000 },
    { Band::Band10m, 28'000'000, 29'700'000 },
    { Band::Band6m,  50'000'000, 52'000'000 },
}};

// Australia (clsBandStackManager.cs:1483-1499 [v2.10.3.13]).
constexpr std::array<BandRange, 11> kAustraliaBandRanges{{
    { Band::Band160m, 1'810'000,  1'875'000 },
    { Band::Band80m,  3'500'000,  3'800'000 },
    { Band::Band60m,  5'000'000,  7'000'000 },  // wide allocation
    { Band::Band40m,  7'000'000,  7'300'000 },
    { Band::Band30m, 10'100'000, 10'150'000 },
    { Band::Band20m, 14'000'000, 14'350'000 },
    { Band::Band17m, 18'068'000, 18'168'000 },
    { Band::Band15m, 21'000'000, 21'450'000 },
    { Band::Band12m, 24'890'000, 24'990'000 },
    { Band::Band10m, 28'000'000, 29'700'000 },
    { Band::Band6m,  50'000'000, 54'000'000 },
}};

// Spain (clsBandStackManager.cs:1440-1465 [v2.10.3.13]).
constexpr std::array<BandRange, 11> kSpainBandRanges{{
    { Band::Band160m, 1'810'000,  2'000'000 },
    { Band::Band80m,  3'500'000,  3'800'000 },
    { Band::Band60m,  5'100'000,  5'500'000 },
    { Band::Band40m,  7'000'000,  7'200'000 },
    { Band::Band30m, 10'100'000, 10'150'000 },
    { Band::Band20m, 14'000'000, 14'350'000 },
    { Band::Band17m, 18'068'000, 18'168'000 },
    { Band::Band15m, 21'000'000, 21'450'000 },
    { Band::Band12m, 24'890'000, 24'990'000 },
    { Band::Band10m, 28'000'000, 29'700'000 },
    { Band::Band6m,  50'000'000, 52'000'000 },
}};

// ---------------------------------------------------------------------------
// Helper: return 60m channels for region (empty span → no channelization)
// ---------------------------------------------------------------------------

// NOTE: Apple-clang < 16 cannot deduce std::span from a constexpr switch
// return whose arms have different array element counts. We return a
// ChannelEntry const* + std::size_t pair and wrap at call-site to avoid
// the deduction issue while preserving constexpr semantics.
struct ChannelSpan {
    const ChannelEntry* data;
    std::size_t         size;
};

static ChannelSpan channels60mFor(Region region) noexcept
{
    switch (region) {
    case Region::UnitedKingdom:
        return { kUkChannels60m.data(), kUkChannels60m.size() };
    case Region::Japan:
        return { kJapanChannels60m.data(), kJapanChannels60m.size() };
    case Region::UnitedStates:
        return { kUsChannels60m.data(), kUsChannels60m.size() };
    // TODO(3M follow-up): Add per-region 60m channel arms for regions with
    // discrete channelization (Italy, Slovakia, etc. — see Thetis source).
    // Regions with NO channelization MUST NOT fall through to US channels —
    // they must return { nullptr, 0 } so the band-range check governs.
    default:
        return { nullptr, 0 };
    }
}

// ---------------------------------------------------------------------------
// Helper: return band-edge ranges for region
// ---------------------------------------------------------------------------

struct RangeSpan {
    const BandRange* data;
    std::size_t      size;
};

static RangeSpan bandRangesFor(Region region) noexcept
{
    switch (region) {
    case Region::Europe:
    case Region::Region1:
        return { kEuropeBandRanges.data(), kEuropeBandRanges.size() };
    case Region::UnitedKingdom:
        return { kUkBandRanges.data(), kUkBandRanges.size() };
    case Region::Japan:
        return { kJapanBandRanges.data(), kJapanBandRanges.size() };
    case Region::Australia:
        return { kAustraliaBandRanges.data(), kAustraliaBandRanges.size() };
    case Region::Spain:
        return { kSpainBandRanges.data(), kSpainBandRanges.size() };
    // TODO(3M follow-up): Add per-region band-range arms for the 18
    // remaining regions (India, Italy, Israel, Norway, Denmark, Sweden,
    // Latvia, Slovakia, Bulgaria, Greece, Hungary, Netherlands, France,
    // Russia, Region3, Germany — Region1 already maps to Europe, Region2
    // to UnitedStates). Each region needs ~12 lines of constexpr array
    // transcribed verbatim from clsBandStackManager.cs:1287-1497
    // [v2.10.3.13] plus 2 test cases.
    //
    // 3M-0 ships with US-fallback default — BandPlanGuard is inert
    // until 3M-1a wires it into the MOX path. Safe to defer.
    case Region::UnitedStates:
    case Region::Region2:
    default:
        return { kUsBandRanges.data(), kUsBandRanges.size() };
    }
}

// ---------------------------------------------------------------------------
// Predicate helpers
// ---------------------------------------------------------------------------

// US 60m mode restriction — console.cs:29416-29432 [v2.10.3.13]:
//   _tx_band == Band.B60M && current_region == FRSRegion.US && !extended
//   → switch on CurrentDSPMode: allow USB / CWL / CWU / DIGU, reject all others.
static bool isUs60mModeAllowed(DSPMode mode) noexcept
{
    return mode == DSPMode::USB  ||   // console.cs:29420
           mode == DSPMode::CWL  ||   // console.cs:29421
           mode == DSPMode::CWU  ||   // console.cs:29422
           mode == DSPMode::DIGU;     // console.cs:29423
}

static bool isInChannel(std::int64_t freqHz, const ChannelEntry& ch) noexcept
{
    const auto half = ch.bwHz / 2;
    return freqHz >= (ch.centerHz - half) &&
           freqHz <= (ch.centerHz + half);
}

static bool isInBandRange(std::int64_t freqHz, const BandRange& range) noexcept
{
    return freqHz >= range.loHz && freqHz <= range.hiHz;
}

} // namespace

// ---------------------------------------------------------------------------
// BandPlanGuard predicates
// ---------------------------------------------------------------------------

bool BandPlanGuard::isValidTxFreq(Region region, std::int64_t freqHz,
                                  DSPMode mode, bool extended) const noexcept
{
    // Extended toggle bypass — console.cs:6772, 6810 [v2.10.3.13].
    if (extended) {
        return true;
    }

    // NereusSDR safety enhancement: gates US 60m TX on the 5 discrete channel
    // centers (±BW/2). This is more restrictive than Thetis's IsOKToTX
    // (clsBandStackManager.cs:1063-1083 [v2.10.3.13]), which allows TX
    // anywhere in the broad B60M range. Thetis uses the channels_60m table
    // (console.cs:2643-2669 [v2.10.3.13]) only for VFO snap and band-stack
    // display, NOT for TX gating. The channelized gate is intentional here per
    // FCC Part 97.303(h), which permits US 60m operation on these 5 channels
    // only.
    //
    // US 60m mode restriction (USB/CWL/CWU/DIGU only) per console.cs:29423.
    const ChannelSpan ch60m = channels60mFor(region);
    for (std::size_t i = 0; i < ch60m.size; ++i) {
        if (isInChannel(freqHz, ch60m.data[i])) {
            if (region == Region::UnitedStates && !isUs60mModeAllowed(mode)) {
                return false;
            }
            return true;
        }
    }

    // All other bands: check per-region band edges.
    // Skip B60M rows only when the region has discrete channels — for those
    // regions, channelization was the authoritative check above and the broad
    // B60M range entry is a placeholder. For regions with NO channelization
    // (e.g. Australia's wide 5.0-7.0 MHz allocation), the B60M range row
    // is the authoritative source and must NOT be skipped.
    const RangeSpan ranges = bandRangesFor(region);
    for (std::size_t i = 0; i < ranges.size; ++i) {
        if (ranges.data[i].band == Band::Band60m && ch60m.size > 0) {
            // B60M range placeholder — channelization was the authoritative
            // check above. Skip to avoid false positives on the broad edge.
            continue;
        }
        if (isInBandRange(freqHz, ranges.data[i])) {
            return true;
        }
    }

    return false;
}

bool BandPlanGuard::isValidTxBand(Band rxBand, Band txBand,
                                  bool preventDifferentBand) const noexcept
{
    // From console.cs:29401-29414 [2.9.0.7]MW0LGE
    if (!preventDifferentBand) {
        return true;
    }
    return rxBand == txBand;
}

// ---------------------------------------------------------------------------
// 3M-1b SSB-mode allow-list (NereusSDR-native)
// ---------------------------------------------------------------------------

bool BandPlanGuard::isModeAllowedForTx(DSPMode mode) const noexcept
{
    // 3M-1b ships SSB voice only. CW is 3M-2; AM/SAM/FM/DSB/DRM are 3M-3.
    // SPEC is never a TX mode.
    switch (mode) {
        case DSPMode::LSB:
        case DSPMode::USB:
        case DSPMode::DIGL:
        case DSPMode::DIGU:
            return true;
        default:
            return false;
    }
}

BandPlanGuard::MoxCheckResult
BandPlanGuard::checkMoxAllowed(Region region, std::int64_t freqHz,
                                DSPMode mode, Band rxBand, Band txBand,
                                bool preventDifferentBand,
                                bool extended) const noexcept
{
    // Mode check first — cheaper and more directly user-facing.
    if (!isModeAllowedForTx(mode)) {
        QString reason;
        switch (mode) {
            case DSPMode::CWL:
            case DSPMode::CWU:
                reason = QStringLiteral("CW TX coming in Phase 3M-2");
                break;
            case DSPMode::AM:
            case DSPMode::SAM:
            case DSPMode::DSB:
            case DSPMode::FM:
            case DSPMode::DRM:
                reason = QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)");
                break;
            default:
                reason = QStringLiteral("Mode not supported for TX");
                break;
        }
        return {false, reason};
    }

    // Frequency / band-edge check.
    if (!isValidTxFreq(region, freqHz, mode, extended)) {
        return {false, QStringLiteral("Frequency outside TX-allowed range")};
    }

    // Band-mismatch check.
    if (!isValidTxBand(rxBand, txBand, preventDifferentBand)) {
        return {false, QStringLiteral("RX/TX band mismatch — cross-band TX disabled")};
    }

    return {true, QString()};
}

} // namespace NereusSDR::safety
