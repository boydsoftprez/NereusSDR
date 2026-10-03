// no-port-check: AetherSDR-derived NereusSDR file. The spot mode resolver is
// ported from AetherSDR src/core/SpotModeResolver.{h,cpp} [@1e0718ad].
// Registered in docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/models/SpotModeResolver.h  (NereusSDR)
// =================================================================
//
// The mode a left-click on a spot puts the pan's slice in: the spot's own
// mode, else a mode word in its comment, else the band plan at its
// frequency.
//
// Ported from AetherSDR src/core/SpotModeResolver.{h,cpp} [@1e0718ad].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26  J.J. Boyd / KG4VCF  Remote-window parity Task 18: ported
//                                    for the spot left-click. AetherSDR's
//                                    four functions kept as they are
//                                    (namespace AetherSDR -> NereusSDR);
//                                    dspModeForSpot() is NereusSDR's: it
//                                    turns AetherSDR's radio mode names
//                                    into DSPMode values and a FreeDV
//                                    spot into RADE. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================

#pragma once

#include "core/WdspTypes.h"

#include <QString>

#include <optional>

namespace NereusSDR {

struct SpotData;

namespace SpotModeResolver {

// From AetherSDR src/core/SpotModeResolver.h:7-17 [@1e0718ad].
QString extractSpotModeFromComment(const QString& comment);

QString inferSpotModeFromBand(double rxFreqMhz);

QString mapSpotModeToRadioMode(const QString& spotMode, double rxFreqMhz);

// All-in-one: explicit mode → comment parse → band-plan inference → radio mode.
// Returns empty string if no radio mode could be determined.
QString resolveSpotRadioMode(const QString& explicitMode,
                             const QString& comment,
                             double rxFreqMhz);

/// NereusSDR: the DSPMode a left-click on `spot` asks for, or nothing when
/// the spot says no mode NereusSDR has. A FreeDV spot asks for RADE, on the
/// sideband the band's own default uses (AetherSDR activates RADE for a
/// FreeDV spot, MainWindow_Wiring.cpp:4427-4432 [@1e0718ad], on DIGU or DIGL
/// by the band's default mode, MainWindow_DigitalModes.cpp:340-350
/// [@1e0718ad]). AetherSDR's radio mode names become DSPMode values: CW
/// takes the sideband AetherSDR's own SSB rule uses (upper at 10 MHz and
/// above, lower below), NFM is FM.
std::optional<DSPMode> dspModeForSpot(const SpotData& spot);

} // namespace SpotModeResolver

} // namespace NereusSDR
