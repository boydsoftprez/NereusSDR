// =================================================================
// src/core/BoardCapabilities.h  (NereusSDR)
// =================================================================
//
// Ported from multiple Thetis sources:
//   Project Files/Source/Console/clsHardwareSpecific.cs
//   Project Files/Source/Console/setup.cs
//   Project Files/Source/Console/console.cs
//   Project Files/Source/Console/HPSDR/specHPSDR.cs
//   Project Files/Source/ChannelMaster/network.h
//
// Original Thetis copyright and license (preserved per GNU GPL,
// representing the union of contributors across all cited sources):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2015-2020  Doug Wigley (W5WC) [ChannelMaster — LGPL]
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//   Copyright (C) 2013-2019  Warren Pratt (NR0V) — dsp.cs / cmaster.c / setup.cs resampler
//   Copyright (C) 2017-2019  Chris Codella (W2PA) — console.cs / setup.cs / NetworkIO.cs inline mods
//   Copyright (C) 2018-2025  Laurence Barker (G8NJJ) — Andromeda / Aries / Saturn / ANAN-G2
//   Copyright (C) 2023-2025  Bryan Rambo (W4WMT) — Resampler / VAC / cmASIO / setup.cs [2.10.3.5]
//   Copyright (C) 2006-2007  Bill Tracey (KD5TFD) — NetworkIO.cs / netInterface.c historical
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Synthesized in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Combines logic from the Thetis sources
//                 listed above.
// =================================================================

// src/core/BoardCapabilities.h
//
// Pure-data capability registry. One entry per HPSDRHW value.
// Drives both protocol connections and the Hardware setup UI.
//
// Source: mi0bot/Thetis-HL2 clsHardwareSpecific.cs, Setup.cs, console.cs,
//         HPSDR/specHPSDR.cs, ChannelMaster/network.h

#pragma once

#include "HpsdrModel.h"
#include "RadioDiscovery.h"  // for ProtocolVersion
#include <array>
#include <span>

namespace NereusSDR {

struct BoardCapabilities {
    HPSDRHW         board;
    ProtocolVersion protocol;

    int  adcCount;
    int  maxReceivers;
    std::array<int, 4> sampleRates;  // zero-pad unused slots
    int  maxSampleRate;

    struct Atten {
        int minDb;
        int maxDb;
        int stepDb;
        bool present;
    } attenuator;

    struct Preamp {
        bool present;
        bool hasBypassAndPreamp;
    } preamp;

    int  ocOutputCount;
    bool hasAlexFilters;
    bool hasAlexTxRouting;
    int  xvtrJackCount;
    int  antennaInputCount;

    bool hasPureSignal;
    bool hasDiversityReceiver;
    bool hasStepAttenuatorCal;
    bool hasPaProfile;

    bool hasBandwidthMonitor;
    bool hasIoBoardHl2;
    bool hasSidetoneGenerator;

    int  minFirmwareVersion;
    int  knownGoodFirmware;

    const char* displayName;
    const char* sourceCitation;
};

namespace BoardCapsTable {
    const BoardCapabilities& forBoard(HPSDRHW hw) noexcept;
    const BoardCapabilities& forModel(HPSDRModel m) noexcept;
    std::span<const BoardCapabilities> all() noexcept;

    // --- Per-model preamp/attenuator helpers ---
    // Porting from Thetis console.cs:40755-40825 SetComboPreampForHPSDR().

    // Preamp combo item: display text + underlying PreampMode-like index.
    struct PreampItem {
        const char* label;   // e.g. "0dB", "-10dB", "-20db" (case from Thetis)
        int         modeInt; // index into NereusSDR PreampMode (0=Off..6=Minus50)
    };

    // Returns the RX1 preamp combo items for a given board + ALEX presence.
    // From Thetis console.cs:40755 SetComboPreampForHPSDR.
    std::span<const PreampItem> preampItemsForBoard(HPSDRHW hw, bool alexPresent) noexcept;

    // Returns the RX2 preamp combo items for a given board.
    // From Thetis console.cs:40815 comboRX2Preamp population.
    std::span<const PreampItem> rx2PreampItemsForBoard(HPSDRHW hw) noexcept;

    // Returns the step attenuator maximum dB for a given board + ALEX presence.
    // From Thetis setup.cs:15765 udHermesStepAttenuatorData max logic.
    // 31 for most boards, 61 for ALEX-equipped boards not in the exclusion list.
    int stepAttMaxDb(HPSDRHW hw, bool alexPresent) noexcept;
}

} // namespace NereusSDR
