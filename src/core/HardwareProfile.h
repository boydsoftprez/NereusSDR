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
//   2026-09-23: profileForStation() added for remote windows (R-R3-46),
//                 NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-28: defaultVoltCalibrationFor() ported from
//                 GetDefaultVoltCalibration (clsHardwareSpecific.cs:265-292
//                 [v2.10.3.15]) for the PA current calibration. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
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
// Upstream inline attribution preserved verbatim:
//   :129  case HPSDRModel.ANAN_G1: //N1GP G1 added
//   :164  case HPSDRModel.ANAN_G2_1K:             // G8NJJ: likely to need further changes for PA
//   :178  case HPSDRModel.REDPITAYA: //DH1KLM
//   :180      NetworkIO.SetMKIIBPF(0); // DH1KLM: changed for compatibility reasons for OpenHPSDR compat. DIY PA/Filter boards
HardwareProfile profileForModel(HPSDRModel model);

// Return the default (auto-guessed) HPSDRModel for a discovered board byte.
HPSDRModel defaultModelForBoard(HPSDRHW board);

// Plan Task 15 (NereusSDR-original): the profile for a radio whose board and
// model are both known, the way a connect has them. profileForModel(model),
// except that the HL2 receive-only kit (HPSDRHW::HermesLiteRxOnly, a
// NereusSDR-only board with no Thetis value) keeps its own capability row
// under the HL2 model. mi0bot-Thetis has one HL2 board (HPSDRHW.HermesLite,
// enums.cs:396 [v2.10.3.13-beta2]) and one HL2 model (HERMESLITE), and
// treats receive-only as the operator's RXOnly toggle (console.cs:15374-15395
// [v2.10.3.13-beta2]); the kit's row carries isRxOnlySku, which blocks
// transmit whatever the operator sets.
HardwareProfile profileForRadio(HPSDRHW board, HPSDRModel model);

// R-R3-46 (NereusSDR-original, remote windows only): the profile a remote
// window uses for the Core's radio. The Core's reported model wins when its
// own profile resolves to the reported board, so an ANAN-8000DLE or
// ANAN-G2 1K keeps its row instead of the first model on its board. With no
// usable model the board decides through defaultModelForBoard(), except that
// an Unknown board (the Core has no radio) gives the Unknown profile
// (model FIRST, the Unknown capability row), never Hermes. Local connects do
// not use this: defaultModelForBoard() is unchanged for them.
HardwareProfile profileForStation(HPSDRHW board, HPSDRModel reportedModel);

// The PA current sensor's calibration: the sensor voltage offset (mV) and
// the reading sensitivity (mV per amp), Thetis AmpVoff / AmpSens.
struct VoltCalibration {
    float voff{360.0f};
    float sens{120.0f};
};

// The model's factory volt calibration, Thetis btnAmpDefault's source.
// From Thetis clsHardwareSpecific.cs:265-292 [v2.10.3.15]
// GetDefaultVoltCalibration. Upstream inline comments preserved verbatim:
// Adjacent upstream tag (HasAmps, clsHardwareSpecific.cs:260): //N1GP G2E added
//   :279  voff = 0.001f;                                // current sensor voltage offset
//   :280  sens = 66.23f;                                // current reading sensitivity //0.001 to prevent /0 in the calcs
//   :282  case HPSDRModel.ANAN_G2_1K:                       // will need adjustment probably
VoltCalibration defaultVoltCalibrationFor(HPSDRModel model);

// Return the list of HPSDRModel values compatible with a discovered board byte.
// From Thetis NetworkIO.cs:164-171
// Upstream inline attribution preserved verbatim:
//   :160  //[2.10.3.9]MW0LGE added board check, issue icon shown in setup
QList<HPSDRModel> compatibleModels(HPSDRHW board);

} // namespace NereusSDR
