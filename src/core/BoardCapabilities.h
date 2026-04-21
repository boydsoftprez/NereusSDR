// =================================================================
// src/core/BoardCapabilities.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/clsHardwareSpecific.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/specHPSDR.cs, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/network.h, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
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

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

//=================================================================
// console.cs
//=================================================================
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
//=================================================================
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

/*
*
* Copyright (C) 2010-2018  Doug Wigley 
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

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

#pragma once

#include "HpsdrModel.h"
#include "RadioDiscovery.h"  // for ProtocolVersion
#include <QList>
#include <array>
#include <span>

namespace NereusSDR {

// Saturn BPF1 band edges — per-band start/end frequency in MHz.
// When populated by user via Phase B Task 8 Alex-1 Filters page, the
// per-MAC AppSettings copy is the source of truth; BoardCapsTable::forBoard
// returns an empty list as the default. P2RadioConnection populates
// CodecContext.p2SaturnBpfHpfBits from this list when computing freq→bits
// for ANAN-G2 / G2-1K boards.
struct SaturnBpf1Edge {
    double startMhz{0.0};
    double endMhz{0.0};
};

struct BoardCapabilities {
    HPSDRHW         board;
    ProtocolVersion protocol;

    int  adcCount;
    int  maxReceivers;
    std::array<int, 6> sampleRates;  // zero-pad unused slots; up to 6 for P2 boards
    int  maxSampleRate;

    struct Atten {
        int minDb;
        int maxDb;
        int stepDb;
        bool present;
        // Phase 3P-A Task 11: per-board wire-byte encoding parameters.
        // From spec §6.3.2 (HL2 vs Standard) and §6.3.3 (RedPitaya gate).
        // Standard (ramdor [@501e3f5]): 5-bit mask 0x1F, enable bit 0x20.
        // HL2 (mi0bot [@c26a8a4]):      6-bit mask 0x3F, enable bit 0x40, MOX branch.
        quint8 mask;           // 0x1F (5-bit std) or 0x3F (6-bit HL2)
        quint8 enableBit;      // 0x20 (std) or 0x40 (HL2)
        bool   moxBranchesAtt; // true → send txStepAttn under MOX (HL2 only)
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

    // Phase 3P-B Task 6: P2-specific capability fields.
    // Source: spec §7 / plan Task 6.
    //
    // p2SaturnBpf1Edges: empty → use Alex HPF/LPF defaults.  User populates
    // via Phase B Task 8's Alex-1 Filters page; persisted per-MAC in AppSettings
    // under hardware/<mac>/alex/bpf1/<band>/{start,end}.
    // Non-constexpr field — see BoardCapabilities.cpp for initialisation note.
    QList<SaturnBpf1Edge> p2SaturnBpf1Edges;

    // p2PreampPerAdc: true for boards with independent per-ADC preamp control
    // (OrionMKII family: ANAN-7000DLE / 8000DLE / AnvelinaPro3).
    // Saturn is single-ADC at the wire layer; set false.
    bool p2PreampPerAdc{false};

    // ditherByDefault / randomByDefault: ADC dither and randomiser enable
    // per ADC index (0..2).  All boards default true (Thetis protocol2.cs).
    bool ditherByDefault[3]{true, true, true};
    bool randomByDefault[3]{true, true, true};

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
    // Delegates to BoardCapabilities::attenuator.maxDb (single source of truth).
    // From Thetis setup.cs:15765 [v2.10.3.13]: 31 for most boards, 61 for
    // ALEX-equipped boards not in the exclusion list (OrionMKII/Saturn/HL2).
    // Exception: HL2 returns 63 (6-bit LNA range, mi0bot [@c26a8a4]).
    int stepAttMaxDb(HPSDRHW hw, bool alexPresent) noexcept;
}

} // namespace NereusSDR
