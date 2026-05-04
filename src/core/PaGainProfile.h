// =================================================================
// src/core/PaGainProfile.h  (NereusSDR)
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
//                 safety hotfix (K2GX field report — drive-slider math
//                 without per-band PA gain compensation produced >300 W
//                 output on a 200 W ANAN-8000DLE). Pure factory-table
//                 free functions; no UI / no persistence / no controller
//                 wiring (those land in Phase 2/3 of the same plan via
//                 PaProfile + PaProfileManager + TransmitModel).
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

#pragma once

#include "HpsdrModel.h"
#include "models/Band.h"

namespace NereusSDR {

/// Returns the factory PA gain (in dB) for the given (model, band) pair,
/// matching Thetis `DefaultPAGainsForBands(HPSDRModel)` byte-for-byte.
///
/// The returned value is a per-band "PA attenuation" — Thetis's term for
/// the dB delta the dBm-target math (`SetPowerUsingTargetDBM`,
/// console.cs:46645-46762 [v2.10.3.13]) subtracts from the requested power
/// in dBm before computing the audio-volume scalar. Higher-gain PAs (ANAN-
/// 8000DLE 80m = 50.5 dB) reach full output well below slider 100%, which
/// is why the K2GX field report saw >300 W output on a 200 W radio when
/// this compensation was missing.
///
/// `100.0f` is the upstream sentinel (Thetis loop initializer at
/// clsHardwareSpecific.cs:466 — "100 is no output power"). The HL2 row from
/// mi0bot leaves all HF entries at 100.0f, expecting the user to calibrate
/// real values. Downstream `TransmitModel::computeAudioVolume` short-
/// circuits (gbb >= 99.5) to a linear-fallback path so HL2 users don't
/// regress on HF transmit until they calibrate.
///
/// NereusSDR-specific Band slots (`GEN`, `WWV`, `XVTR`, and the SWL bands
/// `Band120m`..`Band11m`) have NO Thetis equivalent in the gain table.
/// They return the 100.0f sentinel; the same downstream short-circuit
/// handles them.
///
/// Implementation must list every `HPSDRModel` enumerator explicitly so
/// `-Wswitch` flags any new SKU added to `HpsdrModel.h` that isn't routed
/// through this table.
float defaultPaGainsForBand(HPSDRModel model, Band band) noexcept;

/// All-100.0f sentinel profile used for the `Bypass` factory entry in
/// `PaProfileManager`. Returns 100.0f for every band including the
/// NereusSDR-specific slots. Same downstream-short-circuit semantics as
/// `defaultPaGainsForBand` — the linear-fallback math takes over.
float bypassPaGainsForBand(Band band) noexcept;

}  // namespace NereusSDR
