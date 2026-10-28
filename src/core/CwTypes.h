// no-port-check: NereusSDR-original. The CW transmit settings' value types
// and the CW pitch accessor. The pitch range and default are Thetis facts,
// cited at each constant; no upstream logic is ported here.
// =================================================================
// src/core/CwTypes.h  (NereusSDR)
// =================================================================
//
// The CW keyer mode, break-in mode and serial key line values the
// TransmitModel CW settings store as int (R-CW-01), and the one accessor
// every reader of the station CW pitch uses (R-CW-02): the AppSettings key
// "CWPitch" (SettingsScope::Station), clamped to the Setup pitch control's
// range and rounded to its 10 Hz step.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the CW value types and the station CW pitch
//               accessor (R-CW-01, R-CW-02). J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#pragma once

#include "core/NereusCoreExport.h"

namespace NereusSDR {

// TransmitModel::cwKeyerMode (CW_KeyerMode).
enum class CwKeyerMode : int { Straight = 0, IambicA = 1, IambicB = 2 };

// TransmitModel::cwBreakInMode (CW_BreakInMode).
enum class CwBreakInMode : int { Off = 0, Semi = 1, Qsk = 2 };

// TransmitModel::cwSerialKeyLine and cwSerialPttLine (CW_SerialKeyLine,
// CW_SerialPttLine).
enum class CwSerialLine : int { None = 0, Dsr = 1, Cts = 2 };

// From Thetis setup.designer.cs:38256-38285 [v2.10.3.15] udDSPCWPitch
//   Increment = 10, Maximum = 2250, Minimum = 200, Value = 600
inline constexpr int kCwPitchMinHz = 200;
inline constexpr int kCwPitchMaxHz = 2250;
inline constexpr int kCwPitchDefaultHz = 600;
inline constexpr int kCwPitchStepHz = 10;

// Clamps to [kCwPitchMinHz, kCwPitchMaxHz] and rounds to the nearest
// kCwPitchStepHz (a value halfway between two steps rounds up).
NEREUS_CORE_EXPORT int clampCwPitchHz(int hz);

// The station CW pitch: AppSettings "CWPitch", default kCwPitchDefaultHz,
// read through clampCwPitchHz so a stored value outside the range reads
// clamped. On a remote window the key is the Core's (the settings proxy).
NEREUS_CORE_EXPORT int stationCwPitchHz();

} // namespace NereusSDR
