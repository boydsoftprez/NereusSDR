// =================================================================
// src/core/PaGainProfile.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/clsHardwareSpecific.cs:459-751
//     (DefaultPAGainsForBands(HPSDRModel) — per-model PA gain table
//      indexed by Band; values in dB)
//   original licence from Thetis source is included below
//
// Ported from mi0bot-Thetis sources:
//   Project Files/Source/Console/clsHardwareSpecific.cs:769-797
//     (HERMESLITE case adding HL2 gain row to the same factory table)
//   original licence from mi0bot-Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Phase 1 Agent 1A of issue #167 PA-cal
//                 safety hotfix. NereusSDR-specific divergence: the
//                 NereusSDR Band enum has 14 + 13 SWL slots vs Thetis's
//                 11 HF + 14 VHF (B160M..B6M, VHF0..VHF13). Only the 11
//                 HF slots map across; NereusSDR GEN/WWV/XVTR and the SWL
//                 bands return the 100.0f sentinel (Thetis loop init at
//                 clsHardwareSpecific.cs:466 — "100 is no output power").
//                 Downstream `TransmitModel::computeAudioVolume` short-
//                 circuits (gbb >= 99.5) to a linear-fallback path so
//                 these slots don't block transmit.
// =================================================================

// --- From clsHardwareSpecific.cs (Thetis v2.10.3.13) ---

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

// --- From mi0bot-Thetis clsHardwareSpecific.cs [v2.10.3.13-beta2] ---

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

#include "PaGainProfile.h"

namespace NereusSDR {

namespace {

// Sentinel value used by Thetis when no per-band gain is set.
// From Thetis clsHardwareSpecific.cs:466 [v2.10.3.13] —
//   for (int i = 0; i < (int)Band.LAST; i++) gains[i] = 100f;
//   // max them out, these gains are PA attenuations, so 100 is no output power
constexpr float kPaGainSentinel = 100.0f;

// Per-band HF gain rows. Each row holds 11 floats indexed by NereusSDR
// `Band` ordinal positions Band160m..Band6m (0..10). The corresponding
// Thetis enum slots are B160M, B80M, B60M, B40M, B30M, B20M, B17M, B15M,
// B12M, B10M, B6M.
//
// NereusSDR has no equivalent for Thetis's VHF0..VHF13 slots (they were
// for transverter sub-bands above 6m); those values are dropped here. The
// upstream switch-statement assigns identical values to VHF0..VHF13 within
// each model row, so dropping them loses no per-band specificity — and
// NereusSDR's single XVTR slot returns the 100.0f sentinel anyway.
struct HfRow {
    float band160m;
    float band80m;
    float band60m;
    float band40m;
    float band30m;
    float band20m;
    float band17m;
    float band15m;
    float band12m;
    float band10m;
    float band6m;
};

// FIRST/HERMES/HPSDR/ORIONMKII shared row.
// From Thetis clsHardwareSpecific.cs:475-485 [v2.10.3.13]:
//   gains[(int)Band.B160M] = 41.0f;
//   gains[(int)Band.B80M]  = 41.2f;
//   gains[(int)Band.B60M]  = 41.3f;
//   gains[(int)Band.B40M]  = 41.3f;
//   gains[(int)Band.B30M]  = 41.0f;
//   gains[(int)Band.B20M]  = 40.5f;
//   gains[(int)Band.B17M]  = 39.9f;
//   gains[(int)Band.B15M]  = 38.8f;
//   gains[(int)Band.B12M]  = 38.8f;
//   gains[(int)Band.B10M]  = 38.8f;
//   gains[(int)Band.B6M]   = 38.8f;
constexpr HfRow kHermesRow = {
    41.0f, 41.2f, 41.3f, 41.3f, 41.0f,
    40.5f, 39.9f, 38.8f, 38.8f, 38.8f, 38.8f
};

// ANAN10/ANAN10E shared row — identical values to kHermesRow but Thetis
// keeps the case statement separate (clsHardwareSpecific.cs:504-516).
// From Thetis clsHardwareSpecific.cs:506-516 [v2.10.3.13].
constexpr HfRow kAnan10Row = {
    41.0f, 41.2f, 41.3f, 41.3f, 41.0f,
    40.5f, 39.9f, 38.8f, 38.8f, 38.8f, 38.8f
};

// ANAN100 row.
// From Thetis clsHardwareSpecific.cs:536-546 [v2.10.3.13].
constexpr HfRow kAnan100Row = {
    50.0f, 50.5f, 50.5f, 50.0f, 49.5f,
    48.5f, 48.0f, 47.5f, 46.5f, 42.0f, 43.0f
};

// ANAN100B row — Thetis keeps it as its own case (values match ANAN100).
// From Thetis clsHardwareSpecific.cs:566-576 [v2.10.3.13].
constexpr HfRow kAnan100bRow = {
    50.0f, 50.5f, 50.5f, 50.0f, 49.5f,
    48.5f, 48.0f, 47.5f, 46.5f, 42.0f, 43.0f
};

// ANAN100D row.
// From Thetis clsHardwareSpecific.cs:596-606 [v2.10.3.13].
constexpr HfRow kAnan100dRow = {
    49.5f, 50.5f, 50.5f, 50.0f, 49.0f,
    48.0f, 47.0f, 46.5f, 46.0f, 43.5f, 43.0f
};

// ANAN200D row — Thetis keeps it as its own case (values match ANAN100D).
// From Thetis clsHardwareSpecific.cs:626-636 [v2.10.3.13].
constexpr HfRow kAnan200dRow = {
    49.5f, 50.5f, 50.5f, 50.0f, 49.0f,
    48.0f, 47.0f, 46.5f, 46.0f, 43.5f, 43.0f
};

// ANAN-8000DLE row (the K2GX flagship).
// From Thetis clsHardwareSpecific.cs:656-666 [v2.10.3.13]:
//   gains[(int)Band.B160M] = 50.0f;
//   gains[(int)Band.B80M]  = 50.5f;   // K2GX reproducer pin
//   gains[(int)Band.B60M]  = 50.5f;
//   gains[(int)Band.B40M]  = 50.0f;
//   gains[(int)Band.B30M]  = 49.5f;
//   gains[(int)Band.B20M]  = 48.5f;
//   gains[(int)Band.B17M]  = 48.0f;
//   gains[(int)Band.B15M]  = 47.5f;
//   gains[(int)Band.B12M]  = 46.5f;
//   gains[(int)Band.B10M]  = 42.0f;
//   gains[(int)Band.B6M]   = 43.0f;
constexpr HfRow kAnan8000dRow = {
    50.0f, 50.5f, 50.5f, 50.0f, 49.5f,
    48.5f, 48.0f, 47.5f, 46.5f, 42.0f, 43.0f
};

// ANAN7000D / ANAN_G2 / ANVELINAPRO3 / REDPITAYA shared row.
// From Thetis clsHardwareSpecific.cs:689-699 [v2.10.3.13].
constexpr HfRow kAnan7000dRow = {
    47.9f, 50.5f, 50.8f, 50.8f, 50.9f,
    50.9f, 50.5f, 47.0f, 47.9f, 46.5f, 44.6f
};

// ANAN_G2_1K row — Thetis keeps it as its own case (values match
// kAnan7000dRow). From Thetis clsHardwareSpecific.cs:719-729 [v2.10.3.13].
constexpr HfRow kAnanG21kRow = {
    47.9f, 50.5f, 50.8f, 50.8f, 50.9f,
    50.9f, 50.5f, 47.0f, 47.9f, 46.5f, 44.6f
};

// HERMESLITE row from mi0bot fork. HF (160m..10m) all 100.0f sentinel; 6m
// is 38.8f (mi0bot ports the Thetis 6m value through). The downstream
// TransmitModel::computeAudioVolume short-circuit (gbb >= 99.5) gives HF
// users a linear-fallback path until they calibrate.
// From mi0bot clsHardwareSpecific.cs:770-780 [v2.10.3.13-beta2]:
//   gains[(int)Band.B160M] = 100f;
//   gains[(int)Band.B80M]  = 100f;
//   gains[(int)Band.B60M]  = 100f;
//   gains[(int)Band.B40M]  = 100f;
//   gains[(int)Band.B30M]  = 100f;
//   gains[(int)Band.B20M]  = 100f;
//   gains[(int)Band.B17M]  = 100f;
//   gains[(int)Band.B15M]  = 100f;
//   gains[(int)Band.B12M]  = 100f;
//   gains[(int)Band.B10M]  = 100f;
//   gains[(int)Band.B6M]   = 38.8f;
constexpr HfRow kHermesliteRow = {
    100.0f, 100.0f, 100.0f, 100.0f, 100.0f,
    100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 38.8f
};

// Look up an HfRow column by NereusSDR Band. Returns the sentinel for any
// non-HF band (NereusSDR-specific GEN/WWV/XVTR/SWL slots — no Thetis
// equivalent in the gain table).
constexpr float lookupHfBand(const HfRow& row, Band band) noexcept {
    switch (band) {
        case Band::Band160m: return row.band160m;
        case Band::Band80m:  return row.band80m;
        case Band::Band60m:  return row.band60m;
        case Band::Band40m:  return row.band40m;
        case Band::Band30m:  return row.band30m;
        case Band::Band20m:  return row.band20m;
        case Band::Band17m:  return row.band17m;
        case Band::Band15m:  return row.band15m;
        case Band::Band12m:  return row.band12m;
        case Band::Band10m:  return row.band10m;
        case Band::Band6m:   return row.band6m;
        // NereusSDR-specific slots — no Thetis equivalent.
        case Band::GEN:
        case Band::WWV:
        case Band::XVTR:
        // SWL bands (mi0bot enums.cs:310-322 [v2.10.3.13-beta2]).
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
            return kPaGainSentinel;
        case Band::Count:
            // `Count` is not a real band; NereusSDR-internal sentinel.
            return kPaGainSentinel;
    }
    // Defensive fallback — unreachable because every enumerator above is
    // handled. Kept so a casted-int Band value still returns sane data.
    return kPaGainSentinel;
}

}  // namespace

// --- defaultPaGainsForBand --------------------------------------------------
//
// Faithful port of Thetis `DefaultPAGainsForBands(HPSDRModel)`
// (clsHardwareSpecific.cs:459-751 [v2.10.3.13]) plus the mi0bot HERMESLITE
// arm (clsHardwareSpecific.cs:769-797 [v2.10.3.13-beta2]).
//
// Switch-statement arrangement preserves the upstream case-grouping — e.g.
// FIRST/HERMES/HPSDR/ORIONMKII share kHermesRow because Thetis fall-through
// groups them at clsHardwareSpecific.cs:471-474. ANAN100 / ANAN100B /
// ANAN100D / ANAN200D / ANAN8000D get separate cases despite some pairwise
// matches — Thetis enumerates them all explicitly so a future per-model
// gain edit lands cleanly.
//
// Every HPSDRModel enumerator is listed explicitly so `-Wswitch` warns
// when a new SKU is added to HpsdrModel.h without a gain row here.
float defaultPaGainsForBand(HPSDRModel model, Band band) noexcept {
    switch (model) {
        // FIRST/HERMES/HPSDR/ORIONMKII shared row.
        // From Thetis clsHardwareSpecific.cs:471-502 [v2.10.3.13].
        case HPSDRModel::FIRST:
        case HPSDRModel::HERMES:
        case HPSDRModel::HPSDR:
        case HPSDRModel::ORIONMKII:
            return lookupHfBand(kHermesRow, band);

        // ANAN10/ANAN10E shared row.
        // From Thetis clsHardwareSpecific.cs:504-533 [v2.10.3.13].
        case HPSDRModel::ANAN10:
        case HPSDRModel::ANAN10E:
            return lookupHfBand(kAnan10Row, band);

        // ANAN100 — separate case in Thetis switch.
        // From Thetis clsHardwareSpecific.cs:535-563 [v2.10.3.13].
        case HPSDRModel::ANAN100:
            return lookupHfBand(kAnan100Row, band);

        // ANAN100B — separate case (values match ANAN100).
        // From Thetis clsHardwareSpecific.cs:565-593 [v2.10.3.13].
        case HPSDRModel::ANAN100B:
            return lookupHfBand(kAnan100bRow, band);

        // ANAN100D — separate case.
        // From Thetis clsHardwareSpecific.cs:595-623 [v2.10.3.13].
        case HPSDRModel::ANAN100D:
            return lookupHfBand(kAnan100dRow, band);

        // ANAN200D — separate case (values match ANAN100D).
        // From Thetis clsHardwareSpecific.cs:625-653 [v2.10.3.13].
        case HPSDRModel::ANAN200D:
            return lookupHfBand(kAnan200dRow, band);

        // ANAN-8000DLE — separate case. K2GX-flagship; 80m = 50.5f is the
        // pin for issue #167's regression test.
        // From Thetis clsHardwareSpecific.cs:655-683 [v2.10.3.13].
        case HPSDRModel::ANAN8000D:
            return lookupHfBand(kAnan8000dRow, band);

        // ANAN7000D / ANAN_G2 / ANVELINAPRO3 / REDPITAYA shared row.
        // From Thetis clsHardwareSpecific.cs:685-716 [v2.10.3.13].
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN_G2:
        case HPSDRModel::ANVELINAPRO3:
        case HPSDRModel::REDPITAYA:
            return lookupHfBand(kAnan7000dRow, band);

        // ANAN_G2_1K — separate case (values match ANAN7000D group).
        // From Thetis clsHardwareSpecific.cs:718-746 [v2.10.3.13].
        case HPSDRModel::ANAN_G2_1K:
            return lookupHfBand(kAnanG21kRow, band);

        // HERMESLITE — mi0bot fork case; HF all 100.0f sentinel + 6m at
        // 38.8f. Downstream TransmitModel::computeAudioVolume gbb >= 99.5
        // short-circuit gives the HF user a linear fallback until they
        // calibrate (matches v0.3.1 behaviour pre-hotfix).
        // From mi0bot clsHardwareSpecific.cs:769-797 [v2.10.3.13-beta2].
        case HPSDRModel::HERMESLITE:
            return lookupHfBand(kHermesliteRow, band);

        // LAST sentinel — not a real model.
        case HPSDRModel::LAST:
            return kPaGainSentinel;
    }
    // Defensive fallback — unreachable because every enumerator above is
    // handled explicitly.
    return kPaGainSentinel;
}

// --- bypassPaGainsForBand ---------------------------------------------------
//
// Returns the all-100.0f sentinel profile used for the "Bypass" factory
// entry in PaProfileManager. This is the same sentinel value Thetis uses
// to initialize the gain array (clsHardwareSpecific.cs:466 [v2.10.3.13])
// before the per-model switch fills it in. Picking this profile turns off
// the dBm-target compensation entirely — the linear-fallback
// (gbb >= 99.5) branch in TransmitModel::computeAudioVolume takes over.
float bypassPaGainsForBand(Band /*band*/) noexcept {
    return kPaGainSentinel;
}

}  // namespace NereusSDR
