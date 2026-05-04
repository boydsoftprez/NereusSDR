// =================================================================
// src/core/PaTelemetryScaling.h  (NereusSDR)
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
//                 Callsite migration deferred to Phase 4 Agent 4A; the
//                 RadioModel.cpp private copy is intentionally retained
//                 here so this commit is a non-breaking ADD (not a
//                 refactor).
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

#pragma once

#include <QtGlobal>

#include "core/HpsdrModel.h"

namespace NereusSDR {

/// Scale raw PA forward-power ADC counts (typically 0..4095, 12-bit) to
/// calibrated forward power in watts using the per-board curve from
/// Thetis console.cs computeAlexFwdPower (`bridge_volt`, `refvoltage`,
/// `adc_cal_offset` triplet selected by HPSDRModel).
///
/// Formula:
///   volts = (raw - adc_cal_offset) / 4095.0 * refvoltage   clamp >= 0
///   watts = volts^2 / bridge_volt                          clamp >= 0
///
/// Boards without an explicit case (HERMES, HERMESLITE, ANAN10/10E,
/// HPSDR, FIRST sentinel, etc.) fall through to the default triplet
/// `{bridge_volt=0.09, refvoltage=3.3, adc_cal_offset=6}` per Thetis
/// console.cs:25049-25053. Caller may treat the default-branch value
/// as "best-effort uncalibrated" — accurate calibration belongs in
/// PaCalProfile (per-board cal table).
///
/// From Thetis console.cs:25008 [v2.10.3.13] — computeAlexFwdPower entry
/// point.  The full per-board switch + post-switch math (with `//DH1KLM`
/// tag preserved on REDPITAYA) lives in PaTelemetryScaling.cpp.
double scaleFwdPowerWatts(HPSDRModel model, quint16 raw) noexcept;

/// Scale raw forward-power or reverse-power ADC counts to volts at
/// the PA output directional coupler. Used to drive the FWD / REV
/// voltage labels in PaValuesPage (Thetis SetupForm.textFwdVoltage /
/// textRevVoltage).
///
/// Formula (FWD-side per-board curve):
///   volts = (raw - adc_cal_offset) / 4095.0 * refvoltage   clamp >= 0
///
/// Uses the FWD-side `adc_cal_offset` table (same as scaleFwdPowerWatts).
/// Thetis's REV-side computeRefPower uses a slightly different per-board
/// `adc_cal_offset` (~3 ADC counts smaller than FWD on most boards),
/// which produces a ~2.4 mV difference at the UI label. That is below
/// the 0.01 V (`f2` formatting) display resolution Thetis uses for
/// these readouts (console.cs:25068 / :25002), so a single FWD-side
/// curve is the chosen NereusSDR API surface.
///
/// From Thetis console.cs:25060 [v2.10.3.13] — `volts` value computed
/// inside computeAlexFwdPower (the same value Thetis writes to
/// SetupForm.textFwdVoltage at console.cs:25068).
double scaleFwdRevVoltage(HPSDRModel model, quint16 raw) noexcept;

}  // namespace NereusSDR
