// =================================================================
// src/core/PaTelemetryScaling.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis [v2.10.3.13 @501e3f51]:
//   Project Files/Source/Console/console.cs
//
// Original licence from the Thetis source file is included below,
// verbatim, with // --- From [filename] --- marker per
// CLAUDE.md "Byte-for-byte headers and multi-file attribution".
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Phase 1 Agent 1B of issue #167 PA calibration safety
//                 hotfix.  Lifts scaleFwdPowerWatts out of
//                 src/models/RadioModel.cpp (private file-scope helper) into
//                 a public free-function API, plus adds the new
//                 scaleFwdRevVoltage companion needed to drive the
//                 PaValuesPage FWD voltage / REV voltage / Raw FWD power
//                 readout labels.
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

#include "core/PaTelemetryScaling.h"

namespace NereusSDR {

namespace {

// Per-board PA forward-power scaling triplet. One row per Thetis case in
// computeAlexFwdPower.
struct PaFwdTriplet {
    double bridgeVolt;     // bridge_volt — directional coupler coupling factor
    double refVoltage;     // refvoltage — ADC reference voltage in volts
    int    adcCalOffset;   // adc_cal_offset — zero-offset correction
};

// From Thetis console.cs:25008 [v2.10.3.13] — computeAlexFwdPower entry,
// per-board switch body cited per case below.  Returns
// {bridge_volt, refvoltage, adc_cal_offset}.
//
// Boards without an explicit case (HERMES, ANAN10/10E, HPSDR, FIRST/LAST
// sentinels) fall through to the default triplet, matching the existing
// private helper in RadioModel.cpp byte-for-byte.  HERMESLITE has its
// own row (added per mi0bot console.cs:25269-25273 [v2.10.3.13-beta2])
// since the HL2 directional coupler has a substantially larger
// coupling factor than the default ANAN-100 coupler.
//
// Inline upstream attribution preserved verbatim:
//   :25007  case HPSDRModel.ANAN_G1: //N1GP G1 added   (NereusSDR has no G1 enum)
//   :25037  case HPSDRModel.ANAN_G2_1K:             // !K will need different scaling
//   :25038  case HPSDRModel.REDPITAYA: //DH1KLM
PaFwdTriplet fwdTripletFor(HPSDRModel model) noexcept
{
    switch (model) {
    // From Thetis console.cs:25018-25028 [v2.10.3.13]
    case HPSDRModel::ANAN100:
    case HPSDRModel::ANAN100B:
    case HPSDRModel::ANAN100D:
        return { 0.095, 3.3, 6 };
    // From Thetis console.cs:25029-25033 [v2.10.3.13]
    case HPSDRModel::ANAN200D:
        return { 0.108, 5.0, 4 };
    // From Thetis console.cs:25034-25042 [v2.10.3.13]
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANVELINAPRO3:
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:             // !K will need different scaling
    case HPSDRModel::REDPITAYA: //DH1KLM
        return { 0.12, 5.0, 32 };
    // From Thetis console.cs:25043-25048 [v2.10.3.13]
    case HPSDRModel::ORIONMKII:
    case HPSDRModel::ANAN8000D:
        return { 0.08, 5.0, 18 };
    // From mi0bot console.cs:25269-25273 [v2.10.3.13-beta2] — HL2 has its
    // own directional-coupler coupling factor (bridge_volt=1.5) which is
    // ~16.7× larger than the default 0.09.  Bench-reported #167 follow-up:
    // without this case HL2 was reading ~75 W at 50% TUNE (a ~15× over-
    // read on a 5 W QRP radio) because the default ANAN-100 coupler
    // coefficient was being applied to HL2's much-smaller bridge.
    //   //MI0BOT: HL2  [original inline comment from mi0bot console.cs:25269]
    case HPSDRModel::HERMESLITE:
        return { 1.5, 3.3, 6 };
    // From Thetis console.cs:25049-25053 [v2.10.3.13] — default branch.
    // HERMES / ANAN10 / ANAN10E / HPSDR / FIRST / LAST land here.
    // (HERMESLITE used to fall through to this default in the original
    // ramdor/Thetis port; mi0bot adds the explicit case above for the
    // HL2's distinct directional-coupler scaling.)
    default:
        return { 0.09, 3.3, 6 };
    }
}

}  // namespace

// From Thetis console.cs:25056-25071 [v2.10.3.13] computeAlexFwdPower
// post-switch computation. Clamps `volts` and `watts` floor to zero
// (matches Thetis `if (volts < 0) volts = 0; ... if (watts < 0) watts = 0`).
//
// This function takes pre-fetched ADC counts rather than calling
// NetworkIO.getFwdPower() directly — the caller (RadioModel /
// PaValuesPage) owns the hardware interface.
//
// Lifted verbatim from src/models/RadioModel.cpp scaleFwdPowerWatts
// (private file-scope helper).  The RadioModel.cpp copy is intentionally
// retained until Phase 4 Agent 4A migrates the call sites.
double scaleFwdPowerWatts(HPSDRModel model, quint16 raw) noexcept
{
    const PaFwdTriplet t = fwdTripletFor(model);
    double adc = static_cast<double>(raw);
    if (adc < 0) { adc = 0; }
    double volts = (adc - t.adcCalOffset) / 4095.0 * t.refVoltage;
    if (volts < 0) { volts = 0; }
    double watts = (volts * volts) / t.bridgeVolt;
    if (watts < 0) { watts = 0; }
    return watts;
}

// From Thetis console.cs:25060-25068 [v2.10.3.13] — the `volts` value
// computed inside computeAlexFwdPower is the same value Thetis writes
// to SetupForm.textFwdVoltage (`volts.ToString("f2") + " V"`).
// computeRefPower at console.cs:24993 emits the corresponding REV-side
// `volts` to SetupForm.textRevVoltage with the same formula shape.
//
// NereusSDR exposes a single FWD-side curve here (the most common
// case).  See the header docstring for the design note explaining why
// REV-side gets the same FWD-offset table — the per-board offset
// difference is below the f2 UI display resolution.
double scaleFwdRevVoltage(HPSDRModel model, quint16 raw) noexcept
{
    const PaFwdTriplet t = fwdTripletFor(model);
    double adc = static_cast<double>(raw);
    if (adc < 0) { adc = 0; }
    double volts = (adc - t.adcCalOffset) / 4095.0 * t.refVoltage;
    if (volts < 0) { volts = 0; }
    return volts;
}

}  // namespace NereusSDR
