// =================================================================
// src/core/HpsdrModel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/enums.cs, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/network.h, original licence from Thetis source is included below
//   Project Files/Source/Console/clsHardwareSpecific.cs, original licence from Thetis source is included below
//
// =================================================================
// Additional copyright holders whose code is preserved in this file via
// inline markers (upstream file-header blocks do not name them):
//   Laurence Barker (G8NJJ) — ANAN-G2 / Saturn hardware support (preserved
//     via inline markers on HPSDRHW::Saturn enum and HPSDRModel::ANAN_G2/ANAN_G2_1K)
//   Reid Campbell (MI0BOT) — HermesLite 2 enum mappings (preserved via
//     inline markers on HPSDRModel::HERMESLITE and HPSDRHW::HermesLite)
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-27 - rfPowerShownFor, tunePowerShownFor,
//                 tunePowerStoredFromShown and the shown places and unit,
//                 moved from TxApplet and PowerPage with their mi0bot cites
//                 so the Core's catalogue reads what the widgets show
//                 (R-IOS-06, R-IOS-27). J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27 - rfPowerShownFor ports the rest of mi0bot's UpdateDriveLabel:
//                 the HL2 drive snap near the ends and C#'s Math.Round (a
//                 half to even), so an in-between drive shows as mi0bot
//                 shows it. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-27 - The HL2 tune readouts as mi0bot shows them:
//                 tuneSliderShownFor (UpdateTuneLabel's end snap and
//                 Math.Round) for the TX applet, tunePowerShownFor with C#'s
//                 integer division for the fixed tune spinbox, and the
//                 PowerShownRule the catalogue sends. J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29 - rxDisplayCalOffsetDefaultFor ports the per-model RX
//                 display calibration defaults from clsHardwareSpecific.cs
//                 RXDisplayCalbrationOffsetDefauls [v2.10.3.15]; that file's
//                 header is now carried below alongside the existing
//                 rxMeterCalOffsetDefaultFor cite. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
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

/*  network.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015-2020 Doug Wigley, W5WC

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

*/

// --- From clsHardwareSpecific.cs ---
/*  clsHardwareSpecific.cs

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

#pragma once

#include <QMetaType>

#include <cmath>

namespace NereusSDR {

// Logical radio model — what the user says they have in Setup.
// Source: enums.cs:109
enum class HPSDRModel : int {
    FIRST        = -1,
    HPSDR        =  0,  // Atlas/Metis kit
    HERMES       =  1,
    ANAN10       =  2,
    ANAN10E      =  3,
    ANAN100      =  4,
    ANAN100B     =  5,
    ANAN100D     =  6,
    ANAN200D     =  7,
    ORIONMKII    =  8,
    ANAN7000D    =  9,
    ANAN8000D    = 10,
    ANAN_G2      = 11,  // G8NJJ [Thetis enums.cs:125]
    ANAN_G2_1K   = 12,  // G8NJJ [Thetis enums.cs:126]
    ANVELINAPRO3 = 13,
    HERMESLITE   = 14,  // MI0BOT [Thetis enums.cs:128]
    REDPITAYA    = 15,  // DH1KLM contribution — enum slot preserved, impl deferred
    // From Thetis network.h:446 [v2.10.3.15] //N1GP G2E added
    ANAN_G2E     = 16,
    LAST         = 17,  // was 16; bumped for ANAN_G2E
};

// Physical board — what's actually on the wire.
// Source: enums.cs:388 + ChannelMaster/network.h:446
enum class HPSDRHW : int {
    Atlas      =   0,  // HPSDR kit (aka Metis in PowerSDR)
    Hermes     =   1,  // ANAN-10 / ANAN-100
    HermesII   =   2,  // ANAN-10E / ANAN-100B
    Angelia    =   3,  // ANAN-100D
    Orion      =   4,  // ANAN-200D
    OrionMKII  =   5,  // ANAN-7000DLE / 8000DLE / AnvelinaPro3
    HermesLite =   6,  // Hermes Lite 2 — MI0BOT: HL2 allocated number [Thetis network.h:422 / enums.cs:396]
    // 7..9 reserved — DO NOT REUSE (Thetis wire format compares these ints)
    Saturn     =  10,  // ANAN-G2: added G8NJJ [Thetis network.h:423 / enums.cs:397]
    SaturnMKII =  11,  // ANAN-G2 MkII board revision
    // NereusSDR-original SKU slots — NOT on the Thetis wire; integers chosen
    // above the Thetis-defined range (0-11) and below Unknown(999).
    HermesLiteRxOnly = 12, // HL2 RX-only kit (no TX driver). Phase 3M-0.
    // 13..19 available for future NereusSDR-original SKU slots.
    // From Thetis network.h:425 [v2.10.3.15] //N1GP G2E added (HermesC10)
    HermesC10        = 20, // ANAN-G2E (formerly G1) single-ADC HERMES-class RX + OrionMKII TX
    // NereusSDR-native; relocated from 20 to 21 on 2026-05-21 (G2E port) to free Thetis byte 20.
    // See docs/architecture/2026-05-21-anan-g2e-port-design.md §4 for rationale.
    Andromeda        = 21, // Andromeda console (Ganymede PA trip). Phase 3M-0.
    Unknown    = 999
};

constexpr HPSDRHW boardForModel(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::HPSDR:        return HPSDRHW::Atlas;
        case HPSDRModel::HERMES:       return HPSDRHW::Hermes;
        case HPSDRModel::ANAN10:       return HPSDRHW::Hermes;
        case HPSDRModel::ANAN10E:      return HPSDRHW::HermesII;
        case HPSDRModel::ANAN100:      return HPSDRHW::Hermes;
        case HPSDRModel::ANAN100B:     return HPSDRHW::HermesII;
        case HPSDRModel::ANAN100D:     return HPSDRHW::Angelia;
        case HPSDRModel::ANAN200D:     return HPSDRHW::Orion;
        case HPSDRModel::ORIONMKII:    return HPSDRHW::OrionMKII;
        case HPSDRModel::ANAN7000D:    return HPSDRHW::OrionMKII;
        case HPSDRModel::ANAN8000D:    return HPSDRHW::OrionMKII;
        case HPSDRModel::ANAN_G2:      return HPSDRHW::Saturn;
        case HPSDRModel::ANAN_G2_1K:   return HPSDRHW::Saturn;
        case HPSDRModel::ANVELINAPRO3: return HPSDRHW::OrionMKII;
        case HPSDRModel::HERMESLITE:   return HPSDRHW::HermesLite;
        case HPSDRModel::REDPITAYA:    return HPSDRHW::OrionMKII;
        case HPSDRModel::ANAN_G2E:     return HPSDRHW::HermesC10;  // //N1GP G2E added
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:         return HPSDRHW::Unknown;
    }
    return HPSDRHW::Unknown;
}

constexpr const char* displayName(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::HPSDR:        return "HPSDR (Atlas/Metis)";
        case HPSDRModel::HERMES:       return "Hermes";
        case HPSDRModel::ANAN10:       return "ANAN-10";
        case HPSDRModel::ANAN10E:      return "ANAN-10E";
        case HPSDRModel::ANAN100:      return "ANAN-100";
        case HPSDRModel::ANAN100B:     return "ANAN-100B";
        case HPSDRModel::ANAN100D:     return "ANAN-100D";
        case HPSDRModel::ANAN200D:     return "ANAN-200D";
        case HPSDRModel::ORIONMKII:    return "Orion MkII";
        case HPSDRModel::ANAN7000D:    return "ANAN-7000DLE";
        case HPSDRModel::ANAN8000D:    return "ANAN-8000DLE";
        case HPSDRModel::ANAN_G2:      return "ANAN-G2";
        case HPSDRModel::ANAN_G2_1K:   return "ANAN-G2 1K";
        case HPSDRModel::ANVELINAPRO3: return "Anvelina Pro 3";
        case HPSDRModel::HERMESLITE:   return "Hermes Lite 2";
        case HPSDRModel::REDPITAYA:    return "Red Pitaya";
        case HPSDRModel::ANAN_G2E:     return "ANAN-G2E";  // From Thetis setup.designer.cs:8572 [v2.10.3.15] //N1GP G2E added
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:         return "Unknown";
    }
    return "Unknown";
}

// paMaxWattsFor — per-SKU PA output ceiling, used to scale forward-power
// meter ranges so a 5 W QRP HL2 doesn't share the 0-200 W ANAN-8000DLE
// scale.  Returns the manufacturer's spec'd PA maximum, NOT a software
// clamp (the math kernel never enforces a ceiling).  Meter widgets use
// this to set their bar / gauge ranges and red-zone thresholds.
//
// Sourced from the per-SKU max-watts table in the v0.3.2 PA-cal hotfix
// design doc (docs/architecture/pa-calibration-hotfix.md §9 test matrix
// + plan glowing-snacking-mochi.md Phase 3B safety-ceiling matrix).
constexpr int paMaxWattsFor(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::HPSDR:
        case HPSDRModel::HERMES:
        case HPSDRModel::ANAN10:
        case HPSDRModel::ANAN10E:
        case HPSDRModel::REDPITAYA:    return  10;
        case HPSDRModel::ANAN100:
        case HPSDRModel::ANAN100B:
        case HPSDRModel::ANAN_G2:
        case HPSDRModel::ANAN_G2E:     return 100;  // ANAN-G2E is a 100 W class radio (same PA tier as G2)
        case HPSDRModel::ANAN100D:
        case HPSDRModel::ANAN200D:
        case HPSDRModel::ORIONMKII:
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN8000D:
        case HPSDRModel::ANVELINAPRO3: return 200;
        case HPSDRModel::ANAN_G2_1K:   return 1000;
        case HPSDRModel::HERMESLITE:   return   5;
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:         return 100;   // sentinel default
    }
    return 100;
}

// =============================================================================
// Per-SKU RX meter calibration offset (Thetis-faithful port)
// =============================================================================
//
// Factory default cal offset (dB) added to WDSP S-meter readings + MaxBin
// readings to convert ADC dBFS to antenna dBm.  Without this offset, raw
// WDSP `GetRXAMeter(RXA_S_PK/AV)` and `GetDetectMaxBin` values are in
// dBFS relative to the ADC full-scale point, not at-antenna dBm.
//
// Ported byte-for-byte from Thetis clsHardwareSpecific.cs:395-411 [v2.10.3.13]:
//   case HPSDRModel.ANAN7000D:
//   case HPSDRModel.ANAN8000D:
//   case HPSDRModel.ORIONMKII:
//   case HPSDRModel.ANVELINAPRO3:
//   case HPSDRModel.REDPITAYA:      return 4.841644f;
//   case HPSDRModel.ANAN_G2:
//   case HPSDRModel.ANAN_G2_1K:     return -4.476f;
//   default:                        return 0.98f;
//
// Applied by MeterPoller::pollSMeter and the SignalPeak/SignalAvg loop in
// poll(), matching Thetis console.cs:46824 and :46881 [v2.10.3.13]:
//   _RX1MeterValues[Reading.SIGNAL_STRENGTH] =
//       WDSP.CalculateRXMeter(...) + offset;     // offset = RXOffset(1)
//   _RX1MeterValues[Reading.SIGNAL_MAX_BIN] =
//       WDSP.GetDetectMaxBin(0) + offset;
//
// Level Cal keeps the user's value per model (RxMeterCalOffsetDbByRadio;
// "RX1_MeterCalOffsetDb" is the one-value form of earlier builds) (same
// Thetis convention as RX1MeterCalOffset, console.cs:21051).  The default
// is hidden from the UI in 0.4.x; only Setup -> Multimeter exposes it (no
// page yet, deferred to follow-up).
constexpr float rxMeterCalOffsetDefaultFor(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN8000D:
        case HPSDRModel::ORIONMKII:
        case HPSDRModel::ANVELINAPRO3:
        case HPSDRModel::REDPITAYA:    return  4.841644f;  //DH1KLM
        case HPSDRModel::ANAN_G2:
        case HPSDRModel::ANAN_G2_1K:   return -4.476f;
        // From clsHardwareSpecific.cs:409 [v2.10.3.13] default branch.
        // Covers HPSDR/Atlas, all ANAN-10/100/100B/100D/200D variants, HermesLite.
        case HPSDRModel::HPSDR:
        case HPSDRModel::HERMES:
        case HPSDRModel::ANAN10:
        case HPSDRModel::ANAN10E:
        case HPSDRModel::ANAN100:
        case HPSDRModel::ANAN100B:
        case HPSDRModel::ANAN100D:
        case HPSDRModel::ANAN200D:
        case HPSDRModel::HERMESLITE:
        // From Thetis clsHardwareSpecific.cs:408-423 [v2.10.3.15]
        // RXMeterCalbrationOffsetDefaults enumerates ANAN7000D, ANAN8000D,
        // ORIONMKII, ANVELINAPRO3, REDPITAYA (//DH1KLM), ANAN_G2 and
        // ANAN_G2_1K only.  It carries no `case HPSDRModel.ANAN_G2E`, so the
        // G2E SKU takes `default: return 0.98f` upstream and is listed in the
        // default group here rather than given a number of its own.  The N1GP
        // G2E port added `case HPSDRModel.ANAN_G2E: //N1GP G2E added` at seven
        // other sites in that same file (:129, :250, :260, :358, :385, :699,
        // :794) and deliberately left this switch alone, so the omission is
        // upstream intent rather than an upstream oversight.
        // Do NOT graft the ANAN_G2 value onto G2E: -4.476f is G2 / G2-1K only.
        case HPSDRModel::ANAN_G2E:
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:         return  0.98f;
    }
    return 0.98f;  // unreachable; matches Thetis default
}

// Per-radio RX display calibration default (dB), the level calibration's
// display offset.  From Thetis clsHardwareSpecific.cs:424-440 [v2.10.3.15]
// (RXDisplayCalbrationOffsetDefauls):
//   case HPSDRModel.ANAN7000D:
//   case HPSDRModel.ANAN8000D:
//   case HPSDRModel.ORIONMKII:
//   case HPSDRModel.ANVELINAPRO3:
//   case HPSDRModel.REDPITAYA: //DH1KLM
//       return 5.259f;
//   case HPSDRModel.ANAN_G2:
//   case HPSDRModel.ANAN_G2_1K:
//       return -4.4005f;
//   default:
//       return -2.1f;
//
// User value under the AppSettings key "RX1_DisplayCalOffsetDb" (Thetis
// RX1DisplayCalOffset, console.cs:21113-21122 [v2.10.3.15]).  It reaches
// TCI calibration_ex only; the panadapter follows the meter offset
// (console.cs:12305-12311 [v2.10.3.15], UpdateRX1DisplayOffsets).
constexpr float rxDisplayCalOffsetDefaultFor(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN8000D:
        case HPSDRModel::ORIONMKII:
        case HPSDRModel::ANVELINAPRO3:
        case HPSDRModel::REDPITAYA:    return  5.259f;  //DH1KLM
        case HPSDRModel::ANAN_G2:
        case HPSDRModel::ANAN_G2_1K:   return -4.4005f;
        // The default branch.  As in RXMeterCalbrationOffsetDefaults, the
        // switch has no ANAN_G2E case, so the G2E takes the default.
        case HPSDRModel::HPSDR:
        case HPSDRModel::HERMES:
        case HPSDRModel::ANAN10:
        case HPSDRModel::ANAN10E:
        case HPSDRModel::ANAN100:
        case HPSDRModel::ANAN100B:
        case HPSDRModel::ANAN100D:
        case HPSDRModel::ANAN200D:
        case HPSDRModel::HERMESLITE:
        case HPSDRModel::ANAN_G2E:
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:         return -2.1f;
    }
    return -2.1f;  // unreachable; matches Thetis default
}

// Per-preamp-mode RX offset (dB), applied when step-att is DISABLED.
// Ported byte-for-byte from Thetis console.cs:1999-2009 [v2.10.3.15]:
//   rx1_preamp_offset[(int)PreampMode.HPSDR_OFF]      = 20.0f;  // atten inline
//   rx1_preamp_offset[(int)PreampMode.HPSDR_ON]       =  0.0f;  // no atten
//   rx1_preamp_offset[(int)PreampMode.HPSDR_MINUS10]  = 10.0f;
//   rx1_preamp_offset[(int)PreampMode.HPSDR_MINUS20]  = 20.0f;
//   rx1_preamp_offset[(int)PreampMode.HPSDR_MINUS30]  = 30.0f;
//   rx1_preamp_offset[(int)PreampMode.HPSDR_MINUS40]  = 40.0f;
//   rx1_preamp_offset[(int)PreampMode.HPSDR_MINUS50]  = 50.0f;
//   rx1_preamp_offset[(int)PreampMode.SA_MINUS10] = 10.0f;
//   rx1_preamp_offset[(int)PreampMode.SA_MINUS20] = 20.0f; //MW0LGE_21d step atten
//   rx1_preamp_offset[(int)PreampMode.SA_MINUS30] = 30.0f;
//
// Called from RxMeterCalibration::computeOffsetDb in the
// `!stepAttEnabled` branch of Thetis RXPreampOffset (console.cs:20989).
constexpr float rxPreampOffsetDbFor(int preampModeIdx) noexcept {
    switch (preampModeIdx) {
        case 0: return 20.0f;   // PreampMode::Off       == HPSDR_OFF (atten inline)
        case 1: return  0.0f;   // PreampMode::On        == HPSDR_ON
        case 2: return 10.0f;   // PreampMode::Minus10
        case 3: return 20.0f;   // PreampMode::Minus20
        case 4: return 30.0f;   // PreampMode::Minus30
        case 5: return 40.0f;   // PreampMode::Minus40
        case 6: return 50.0f;   // PreampMode::Minus50
        case 7: return 10.0f;   // PreampMode::SaMinus10 == SA_MINUS10
        case 8: return 20.0f;   // PreampMode::SaMinus20 == SA_MINUS20 //MW0LGE_21d step atten
        case 9: return 30.0f;   // PreampMode::SaMinus30 == SA_MINUS30
        default: return 0.0f;
    }
}

// =============================================================================
// Per-SKU PA UI constants — mi0bot-Thetis HL2 parity
// =============================================================================
//
// HL2 (HERMESLITE) treats RF Power and Tune Power sliders as dB attenuators
// rather than watts targets. Specifically:
//   - RF Power slider: Max=90, step=6 (16-step output attenuator, 0.5 dB/step)
//   - Tune Power slider: Max=99, step=3 (33 sub-steps; 0..51 = DSP audio gain
//     modulation; 52..99 = PA attenuator territory)
//   - Fixed-mode Tune Power spinbox: -16.5..0 dB, 0.5 dB increments
//
// Non-HL2 SKUs keep the canonical Thetis 0..100 unitless drive scale.
//
// From mi0bot-Thetis console.cs:2101-2108 [v2.10.3.13-beta2]
//   ptbPWR.Maximum = 90;        // MI0BOT: Changes for HL2 only having a 16 step output attenuator
//   ptbPWR.LargeChange = 6;
//   ptbPWR.SmallChange = 6;
//   ptbTune.Maximum = 99;
//   ptbTune.LargeChange = 3;
//   ptbTune.SmallChange = 3;
// From mi0bot-Thetis setup.cs:20328-20331 [v2.10.3.13-beta2]
//   udTXTunePower.DecimalPlaces = 1;
//   udTXTunePower.Increment = (decimal)0.5;
//   udTXTunePower.Maximum = (decimal)0;
//   udTXTunePower.Minimum = (decimal)-16.5;

constexpr int rfPowerSliderMaxFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 90 : 100;
}

constexpr int rfPowerSliderStepFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 6 : 1;
}

constexpr int tuneSliderMaxFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 99 : 100;
}

constexpr int tuneSliderStepFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 3 : 1;
}

constexpr float fixedTuneSpinboxMinFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? -16.5f : 0.0f;
}

constexpr float fixedTuneSpinboxMaxFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 0.0f : 100.0f;
}

constexpr float fixedTuneSpinboxStepFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 0.5f : 1.0f;
}

constexpr int fixedTuneSpinboxDecimalsFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 1 : 0;
}

constexpr const char* fixedTuneSpinboxSuffixFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? " dB" : " W";
}

// HL2 attenuator hardware model (used by RF Power label formula at TxApplet
// updatePowerSliderLabels). 16 levels (slider 0/6/12/.../84/90), 0.5 dB per
// half-step → -7.5..0 dB total range.
//
// From mi0bot-Thetis console.cs:29245-29274 [v2.10.3.13-beta2]
//   formula: lblPWR.Text = "Drive: " + ((round(drv/6.0)/2) - 7.5) + "dB"
constexpr float hl2AttenuatorDbPerStep() noexcept { return 0.5f; }
constexpr int   hl2AttenuatorStepCount() noexcept { return 16; }

// What each transmit power control shows for its value, per SKU. The TX
// applet's RF Power and Tune labels, Setup > Transmit > Power's fixed tune
// spinbox and the Core's catalogue (board.transmit) all read these, so an
// app shows exactly what the desktop does.
//
// C#'s Math.Round(double), which mi0bot's labels use: the nearest whole
// number, a half to the even one (MidpointRounding.ToEven).
inline double csharpMathRound(double x) noexcept {
    const double down = std::floor(x);
    const double diff = x - down;
    if (diff < 0.5) {
        return down;
    }
    if (diff > 0.5) {
        return down + 1.0;
    }
    return (std::fmod(down, 2.0) == 0.0) ? down : down + 1.0;
}

// RF Power (the `power` property, slider 0..rfPowerSliderMaxFor): the HL2
// shows its output attenuator in dB, other SKUs the bare drive value. A
// drive between the slider's steps snaps as mi0bot's label does first.
// From mi0bot-Thetis console.cs:29245-29264 [v2.10.3.13-beta2]
//   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)       // MI0BOT: HL2 has only 15 output power levels
//   {
//       if (4 > drv)
//       {
//           drv = 0;
//       }
//       else if (3 < drv && 6 > drv)
//       {
//           drv = 6;
//       }
//       else if (87 < drv)
//       {
//           drv = 90;
//       }
//       else if (84 < drv && 88 > drv)
//       {
//           drv = 84;
//       }
//
//       lblPWR.Text = "Drive:  " + ((Math.Round(drv / 6.0) / 2) - 7.5).ToString() + "dB";
inline double rfPowerShownFor(HPSDRModel m, int drive) noexcept {
    if (m == HPSDRModel::HERMESLITE) {       // MI0BOT: HL2 has only 15 output power levels
        int drv = drive;
        if (4 > drv) {
            drv = 0;
        } else if (3 < drv && 6 > drv) {
            drv = 6;
        } else if (87 < drv) {
            drv = 90;
        } else if (84 < drv && 88 > drv) {
            drv = 84;
        }
        return (csharpMathRound(drv / 6.0) / 2.0) - 7.5;
    }
    return static_cast<double>(drive);
}

// Tune slider (the `tunePowerForTxBand` property, 0..tuneSliderMaxFor): the
// HL2 shows -16.5..0 dB, a value near the ends snapped and the rest rounded
// to its step as mi0bot's label does; other SKUs the bare value.
// From mi0bot-Thetis console.cs:47470-47481 [v2.10.3.13-beta2]
//   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)
//   {
//       if (3 > drv)
//       {
//           drv = 0;
//       }
//       else if (96 < drv)
//       {
//           drv = 99;
//       }
//
//       sValue = ((Math.Round(drv / 3.0) / 2) - 16.5).ToString() + "dB";
inline double tuneSliderShownFor(HPSDRModel m, int value) noexcept {
    if (m == HPSDRModel::HERMESLITE) {
        int drv = value;
        if (3 > drv) {
            drv = 0;
        } else if (96 < drv) {
            drv = 99;
        }
        return (csharpMathRound(drv / 3.0) / 2.0) - 16.5;
    }
    return static_cast<double>(value);
}

// Fixed tune power (the `tunePower` property, stored 0..tuneSliderMaxFor),
// as Setup > Transmit > Power's spinbox shows it: the HL2 -16.5..0 dB,
// with C#'s integer division (value/3 on an int drops the remainder),
// other SKUs the stored value in W.
// From mi0bot-Thetis setup.cs:5305-5307 [v2.10.3.13-beta2]
//   if (HPSDRModel.HERMESLITE == HardwareSpecific.Model)
//       udTXTunePower.Value = (decimal)(value/3 - 33)/2;    // MI0BOT: Now only has a -16.5 to 0 range in HL2 for Tune power
inline double tunePowerShownFor(HPSDRModel m, int stored) noexcept {
    if (m == HPSDRModel::HERMESLITE) {
        return static_cast<double>(stored / 3 - 33) / 2.0;
    }
    return static_cast<double>(stored);
}

// How each power control takes a value between its steps before showing
// it, for the Core's catalogue (board.transmit's `shown`): `endSnap` (a
// value below `below` shows as the slider's minimum, one above `above` as
// its maximum), then `rounding`: "halfEven" (the nearest step, a half to
// the even step: C#'s Math.Round), "down" (the step at or below: C#'s
// integer division) or "none". These restate rfPowerShownFor,
// tuneSliderShownFor and tunePowerShownFor above; tst_catalogue_ranges
// holds them against the widgets at every value.
struct PowerShownRule {
    const char* rounding;
    bool hasEndSnap;
    int below;
    int above;
};

constexpr PowerShownRule rfPowerShownRuleFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? PowerShownRule{"halfEven", true, 4, 87}
                                         : PowerShownRule{"halfEven", false, 0, 0};
}

constexpr PowerShownRule tuneSliderShownRuleFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? PowerShownRule{"halfEven", true, 3, 96}
                                         : PowerShownRule{"none", false, 0, 0};
}

constexpr PowerShownRule tunePowerShownRuleFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? PowerShownRule{"down", false, 0, 0}
                                         : PowerShownRule{"none", false, 0, 0};
}

// The stored tune power a shown value writes (the fixed tune spinbox).
// From mi0bot-Thetis setup.cs:9396-9397 [v2.10.3.13-beta2]
//   // MI0BOT: Range is 0 to -16.5 - convert to 99 - 0
//   console.TunePower = (int) ((33 + (udTXTunePower.Value * 2)) * 3);
// The C# int cast truncates; std::lround picks the nearest legal sub-step
// for a value between steps (identical on exact 0.5 dB steps).
inline int tunePowerStoredFromShown(HPSDRModel m, double shown) noexcept {
    if (m == HPSDRModel::HERMESLITE) {
        return static_cast<int>(std::lround((33.0 + shown * 2.0) * 3.0));
    }
    return static_cast<int>(std::lround(shown));
}

// Places and unit for the RF Power and Tune sliders' labels: the HL2's dB
// to one place, other SKUs a bare whole number.
constexpr int powerSliderShownDecimalsFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? 1 : 0;
}

constexpr const char* powerSliderShownUnitFor(HPSDRModel m) noexcept {
    return (m == HPSDRModel::HERMESLITE) ? "dB" : "";
}

// boardCodeName — returns the HPSDRHW enum label as a short model-code string.
// Used in the status-bar board widget to show "Saturn" instead of the full
// marketing name "ANAN-G2 (Saturn)" which truncates at typical status-bar widths.
constexpr const char* boardCodeName(HPSDRHW hw) noexcept {
    switch (hw) {
        case HPSDRHW::Atlas:            return "Atlas";
        case HPSDRHW::Hermes:           return "Hermes";
        case HPSDRHW::HermesII:         return "HermesII";
        case HPSDRHW::Angelia:          return "Angelia";
        case HPSDRHW::Orion:            return "Orion";
        case HPSDRHW::OrionMKII:        return "OrionMKII";
        case HPSDRHW::HermesLite:       return "HL2";
        case HPSDRHW::Saturn:           return "Saturn";
        case HPSDRHW::SaturnMKII:       return "SaturnMKII";
        case HPSDRHW::HermesLiteRxOnly: return "HL2-RX";
        case HPSDRHW::HermesC10:       return "HermesC10";  // //N1GP G2E added (HermesC10)
        case HPSDRHW::Andromeda:        return "Andromeda";
        case HPSDRHW::Unknown:          return "Unknown";
    }
    return "Unknown";
}

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::HPSDRModel)
Q_DECLARE_METATYPE(NereusSDR::HPSDRHW)
