// =================================================================
// src/core/BoardCapabilities.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/clsHardwareSpecific.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/enums.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/NetworkIO.cs (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/clsDiscoveredRadioPicker.cs, original licence from Thetis source is included below
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
// P1 sample rates: {48k,96k,192k} standard; HermesLite also supports 384k
//   (Setup.cs:850-853 — "The HL supports 384K")
// P2 sample rates: {48k,96k,192k,384k,768k,1536k} all six, per Setup.cs:854
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

//
// Upstream source 'Project Files/Source/Console/HPSDR/NetworkIO.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

/*  clsDiscoveredRadioPicker.cs

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

#include "BoardCapabilities.h"

namespace NereusSDR {
namespace {

// ─── Atlas / HPSDR kit ──────────────────────────────────────────────────────
// Source: network.h:448 (Atlas=0), clsHardwareSpecific.cs Model→Hardware map
// ADC: single (clsHardwareSpecific.cs: no explicit SetRxADC for HPSDR model,
//      defaults to 1). Metis/Atlas board is Protocol 1 only.
// No Alex port on bare Atlas kit; OC outputs via Penny board (not addressable
//   the same way). Atlas max receivers = 7 (P1 hardware limit), though
//   typically 3 in practice. maxSampleRate = 192k for P1 standard.
// Firmware floor: none. Thetis NetworkIO.cs has no Atlas firmware check.
// NOTE: Changed from constexpr to const in Phase 3P-B Task 6 because
// BoardCapabilities now contains QList<SaturnBpf1Edge> (p2SaturnBpf1Edges),
// which is not constexpr-compatible.  All board entries follow suit.
//
// Phase 3P-F Task 2 (accessories):
//   hasApollo=false — bare Atlas/Metis kit; HPSDRModel.HPSDR is not in the
//     RadioModelChanged() switch at setup.cs:19834-20310 [@501e3f5], so no
//     per-model case fires; chkApolloPresent defaults to disabled.
//   hasAlex=false — Atlas is the bare board; no Alex physical slot.
//   hasPennyLane=true — Penny board is Atlas's OC-output companion;
//     AddHPSDRPages() adds tpPennyCtrl for all HPSDR models (setup.cs:6364).
const BoardCapabilities kAtlas = {
    .board            = HPSDRHW::Atlas,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 3,
    .sampleRates      = {48000, 96000, 192000, 0, 0, 0},
    .maxSampleRate    = 192000,
    .attenuator       = {0, 0, 0, false, 0x1F, 0x20, false},
    .preamp           = {false, false},
    .ocOutputCount    = 0,
    .hasAlexFilters   = false,
    .hasAlexTxRouting = false,
    .xvtrJackCount    = 0,
    .antennaInputCount = 1,
    .hasAlex2         = false,
    .hasRxBypassRelay = false,
    .rxOnlyAntennaCount = 0,
    .hasPureSignal    = false,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = false,
    .hasPaProfile     = false,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,  // Atlas bare board — no Apollo port
    .hasAlex          = false,  // Atlas bare board — Alex is external
    .hasPennyLane     = true,   // Penny is the Atlas OC companion; tpPennyCtrl added for all HPSDR models
    .minFirmwareVersion = 0,
    .knownGoodFirmware  = 0,
    .displayName      = "Atlas / HPSDR kit",
    .sourceCitation   = "network.h:448, enums.cs:390, clsHardwareSpecific.cs Atlas branch",
};

// ─── Hermes (ANAN-10 / ANAN-100) ────────────────────────────────────────────
// Source: network.h:449 (Hermes=1), clsHardwareSpecific.cs:87-93
// ADC: SetRxADC(1) — single ADC. Protocol 1.
// Attenuator: 0..31 dB step 1 (Setup.cs:16099 standard path).
// Alex: present on ANAN-100; optional accessory on ANAN-10.
//   Treated as available since the hardware supports it.
// OC: 7 outputs on Hermes board.
// maxReceivers: 4 for P1 with single ADC (DDC config slots 0-3).
// Firmware floor: none. NetworkIO.cs:136-143 — the only firmware refusal in
//   Thetis is HermesII-only (DeviceType == HPSDRHW.HermesII && CodeVersion < 103).
//   Plain Hermes has no equivalent guard. v15 ("FW v1.5") is a normal
//   legitimate Hermes firmware that Thetis accepts without complaint.
//
// Phase 3P-F Task 2 (accessories):
//   hasApollo=true — HPSDRModel.HERMES is the only case where
//     chkApolloPresent.Enabled=true (setup.cs:19834 [@501e3f5]).
//   hasAlex=true — chkAlexPresent.Enabled=true for HERMES (setup.cs:19835).
//   hasPennyLane=true — tpPennyCtrl added for all HPSDR models; labeled "Hermes Ctrl"
//     when Model==HERMES (setup.cs:6327-6328 [@501e3f5]).
const BoardCapabilities kHermes = {
    .board            = HPSDRHW::Hermes,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 4,
    .sampleRates      = {48000, 96000, 192000, 0, 0, 0},
    .maxSampleRate    = 192000,
    .attenuator       = {0, 31, 1, true, 0x1F, 0x20, false},
    .preamp           = {true, false},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasAlex2         = false,
    .hasRxBypassRelay = true,
    .rxOnlyAntennaCount = 3,
    .hasPureSignal    = false,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = false,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = true,   // only board with chkApolloPresent.Enabled=true (setup.cs:19834)
    .hasAlex          = true,   // chkAlexPresent.Enabled=true (setup.cs:19835)
    .hasPennyLane     = true,   // tpPennyCtrl always added; labeled "Hermes Ctrl" for this board
    .minFirmwareVersion = 0,   // No Thetis-attested floor for plain Hermes
    .knownGoodFirmware  = 0,   // No Thetis-attested "known good" reference
    .displayName      = "Hermes (ANAN-10/100)",
    .sourceCitation   = "network.h:449, enums.cs:391, clsHardwareSpecific.cs:87-93; "
                        "fw floor: NetworkIO.cs:136-143 (no plain-Hermes check)",
};

// ─── HermesII (ANAN-10E / ANAN-100B) ────────────────────────────────────────
// Source: network.h:450 (HermesII=2), clsHardwareSpecific.cs:108-127
// ADC: SetRxADC(1) — single ADC. Protocol 1.
// HermesII supports PureSignal (console.cs:30276-30277 ANAN10E psform.PSEnabled).
// No diversity (single ADC). Step attenuator cal: present on HermesII.
// Note: Setup.cs:16088-16099 — HermesII is explicitly excluded from the
//   "Maximum=61" branch; uses standard 0..31 range.
// TODO(3I-T2): verify maxReceivers=4 vs 7 for HermesII
//
// Phase 3P-F Task 2 (accessories):
//   hasApollo=false — setup.cs:19899 chkApolloPresent.Enabled=false for ANAN10E/100B.
//   hasAlex=true — chkAlexPresent.Checked=true for ANAN10E/100B (setup.cs:19896-19897).
//   hasPennyLane=true — tpPennyCtrl added for all HPSDR models (setup.cs:6364).
const BoardCapabilities kHermesII = {
    .board            = HPSDRHW::HermesII,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 4,
    .sampleRates      = {48000, 96000, 192000, 0, 0, 0},
    .maxSampleRate    = 192000,
    .attenuator       = {0, 31, 1, true, 0x1F, 0x20, false},
    .preamp           = {true, false},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasAlex2         = false,
    .hasRxBypassRelay = true,
    .rxOnlyAntennaCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,  // chkApolloPresent.Enabled=false for ANAN10E/100B (setup.cs:19899)
    .hasAlex          = true,   // chkAlexPresent.Checked=true (setup.cs:19896)
    .hasPennyLane     = true,   // tpPennyCtrl added for all HPSDR models (setup.cs:6364)
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,   // Thetis NetworkIO.cs:138 still rejects <103
                               // for HermesII, but enforcement moved upstream
                               // (see P1RadioConnection.cpp 2026-04-13)
    .displayName      = "Hermes II (ANAN-10E/100B)",
    .sourceCitation   = "network.h:450, enums.cs:392, clsHardwareSpecific.cs:108-127; "
                        "fw refusal in upstream Thetis: NetworkIO.cs:136-143 (<103)",
};

// ─── Angelia (ANAN-100D) ────────────────────────────────────────────────────
// Source: network.h:451 (Angelia=3), clsHardwareSpecific.cs:129-134
// ADC: SetRxADC(2) — dual ADC. Protocol 1.
// Diversity + PureSignal: yes (2 ADCs, console.cs GetDDC P1 Angelia branch:8680)
// maxReceivers: 7 (P1 dual-ADC limit, console.cs GetDDC Angelia branch)
// TODO(3I-T2): verify firmware versions
//
// Phase 3P-F Task 2 (accessories):
//   hasApollo=false — setup.cs:20009-20010 chkApolloPresent.Enabled=false for ANAN100D.
//   hasAlex=true — chkAlexPresent.Checked=true, Enabled=true (setup.cs:20007-20008).
//   hasPennyLane=true — tpPennyCtrl added for all HPSDR models (setup.cs:6364).
const BoardCapabilities kAngelia = {
    .board            = HPSDRHW::Angelia,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 2,
    .maxReceivers     = 7,
    .sampleRates      = {48000, 96000, 192000, 384000, 0, 0},
    .maxSampleRate    = 384000,
    .attenuator       = {0, 31, 1, true, 0x1F, 0x20, false},
    .preamp           = {true, false},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasAlex2         = false,
    .hasRxBypassRelay = true,
    .rxOnlyAntennaCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,  // chkApolloPresent.Enabled=false for ANAN100D (setup.cs:20009)
    .hasAlex          = true,   // chkAlexPresent.Checked=true, Enabled=true (setup.cs:20007)
    .hasPennyLane     = true,   // tpPennyCtrl added for all HPSDR models (setup.cs:6364)
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    .displayName      = "ANAN-100D (Angelia)",
    .sourceCitation   = "network.h:451, enums.cs:393, clsHardwareSpecific.cs:129-134",
};

// ─── Orion (ANAN-200D) ──────────────────────────────────────────────────────
// Source: network.h:452 (Orion=4), clsHardwareSpecific.cs:136-141
// ADC: SetRxADC(2) — dual ADC. Protocol 1 (P1 board).
// Diversity + PureSignal: yes (2 ADCs, console.cs GetDDC P1 Orion branch:8681)
// Preamp: hasBypassAndPreamp=true (50V supply vs 33V for Hermes/Angelia,
//   clsHardwareSpecific.cs:139 SetADCSupply(0,50))
// TODO(3I-T2): verify firmware versions
//
// Phase 3P-F Task 2 (accessories):
//   hasApollo=false — setup.cs:20059-20060 chkApolloPresent.Enabled=false for ANAN200D.
//   hasAlex=true — chkAlexPresent.Checked=true, Enabled=true (setup.cs:20057-20058).
//   hasPennyLane=true — tpPennyCtrl added for all HPSDR models (setup.cs:6364).
const BoardCapabilities kOrion = {
    .board            = HPSDRHW::Orion,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 2,
    .maxReceivers     = 7,
    .sampleRates      = {48000, 96000, 192000, 384000, 0, 0},
    .maxSampleRate    = 384000,
    .attenuator       = {0, 31, 1, true, 0x1F, 0x20, false},
    .preamp           = {true, true},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasAlex2         = false,
    .hasRxBypassRelay = true,
    .rxOnlyAntennaCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,  // chkApolloPresent.Enabled=false for ANAN200D (setup.cs:20059)
    .hasAlex          = true,   // chkAlexPresent.Checked=true, Enabled=true (setup.cs:20057)
    .hasPennyLane     = true,   // tpPennyCtrl added for all HPSDR models (setup.cs:6364)
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    .displayName      = "ANAN-200D (Orion)",
    .sourceCitation   = "network.h:452, enums.cs:394, clsHardwareSpecific.cs:136-141",
};

// ─── OrionMKII (ANAN-7000DLE / 8000DLE / AnvelinaPro3 / RedPitaya) ──────────
// Source: network.h:453 (OrionMKII=5), clsHardwareSpecific.cs:143-190
// ADC: SetRxADC(2) — dual ADC. Protocol 2 (clsHardwareSpecific.cs HasAudioAmplifier
//   check: "protocol 2 only for 7000D/8000D/AnvelinaPro3/RedPitaya").
// P2 sample rates: 48k/96k/192k/384k/768k/1536k all six (Setup.cs:854).
// Correction vs plan baseline: maxSampleRate updated to 1536000 (P2 boards)
// TODO(3I-T2): verify firmware versions for OrionMKII
//
// Phase 3P-F Task 2 (accessories):
//   hasApollo=false — setup.cs:20100-20101 chkApolloPresent.Enabled=false for ANAN7000D;
//     same for ANAN8000D (setup.cs:20151-20152) and ANVELINAPRO3 (setup.cs:20205-20206).
//   hasAlex=true — chkAlexPresent.Checked=true, Enabled=true (setup.cs:20098-20099 etc).
//   hasPennyLane=true — tpPennyCtrl added for all HPSDR models (setup.cs:6364).
const BoardCapabilities kOrionMKII = {
    .board            = HPSDRHW::OrionMKII,
    .protocol         = ProtocolVersion::Protocol2,
    .adcCount         = 2,
    .maxReceivers     = 7,
    .sampleRates      = {48000, 96000, 192000, 384000, 768000, 1536000},
    .maxSampleRate    = 1536000,
    .attenuator       = {0, 31, 1, true, 0x1F, 0x20, false},
    .preamp           = {true, true},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasAlex2         = true,
    .hasRxBypassRelay = true,
    .rxOnlyAntennaCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,  // chkApolloPresent.Enabled=false for ANAN7000D/8000D/AnvelinaPro3 (setup.cs:20100)
    .hasAlex          = true,   // chkAlexPresent.Checked=true, Enabled=true (setup.cs:20098)
    .hasPennyLane     = true,   // tpPennyCtrl added for all HPSDR models (setup.cs:6364)
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    // Phase 3P-B Task 6: OrionMKII family has independent per-ADC preamp control
    // (ANAN-7000DLE / 8000DLE / AnvelinaPro3). p2SaturnBpf1Edges stays empty
    // (OrionMKII uses standard Alex HPF/LPF; Saturn BPF1 override is G2/G2-1K only).
    .p2PreampPerAdc   = true,
    .displayName      = "ANAN-7000DLE/8000DLE (OrionMkII)",
    .sourceCitation   = "network.h:453, enums.cs:395, clsHardwareSpecific.cs:143-190",
};

// ─── HermesLite (HL2) ───────────────────────────────────────────────────────
// Source: network.h:454 (HermesLite=6), IoBoardHl2.cs, clsHardwareSpecific.cs:94-99
// ADC: SetRxADC(1) — single ADC. Protocol 1.
// HL2 supports 384k (Setup.cs:850-853 "The HL supports 384K", include_extra_p1_rate=true)
// Attenuator: HL2 uses LNA range -28..+32 (60 dB span), step 1
//   (Setup.cs:1084-1088 PerformDelayedInitialisation, :16085-16086 udHermesStepAttenuatorData)
//   Stored as minDb=0, maxDb=60 to represent the total span; the sign-aware
//   range [-28..+32] is handled at UI layer (not in capabilities struct).
// No Alex filters or OC outputs on HL2.
// hasBandwidthMonitor: HL2 has bandwidth_monitor via IoBoardHl2
//   (ChannelMaster/bandwidth_monitor.h + IoBoardHl2.cs)
// hasIoBoardHl2: true — IoBoardHl2.cs present and active for HL2
// hasSidetoneGenerator: true — HL2 firmware implements sidetone
//   (IoBoardHl2.cs references sidetone register)
// maxReceivers: 4 for HL2 (console.cs GetDDC HermesLite branch:8734, same as Hermes)
// Firmware: HL2 v72+ recommended (mi0bot Thetis-HL2 release notes)
//
// Phase 3P-F Task 2 (accessories):
//   hasApollo=false — HL2 is not in the RadioModelChanged() switch; no Apollo
//     port exists on HL2 hardware.
//   hasAlex=false — HL2 has no Alex board slot; ocOutputCount=0; hasAlexFilters=false.
//   hasPennyLane=false — HL2 has no OC ext-ctrl outputs (ocOutputCount=0);
//     AddHPSDRPages() is called for HL2 but the tpPennyCtrl tab is meaningless
//     without OC pins. HL2 uses IoBoardHl2 for I2C accessory control instead.
const BoardCapabilities kHermesLite = {
    .board            = HPSDRHW::HermesLite,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 4,
    .sampleRates      = {48000, 96000, 192000, 384000, 0, 0},
    .maxSampleRate    = 384000,
    // HL2: signed user-facing range −28..+32 dB (mi0bot setup.cs:16085-16086
    // [v2.10.3.13-beta2] udHermesStepAttenuatorData.{Maximum=32, Minimum=-28}).
    // Wire conversion `wire = 31 - userDb` lives in P1CodecHl2.cpp; mask=0x3F,
    // enableBit=0x40 (6-bit field per mi0bot WriteMainLoop_HL2 [@c26a8a4]);
    // MOX branches ATT.
    .attenuator       = {-28, 32, 1, true, 0x3F, 0x40, true},
    .preamp           = {false, false},
    .ocOutputCount    = 0,
    .hasAlexFilters   = false,
    .hasAlexTxRouting = false,
    .xvtrJackCount    = 0,
    .antennaInputCount = 1,
    .hasAlex2         = false,
    .hasRxBypassRelay = false,
    .rxOnlyAntennaCount = 0,
    .hasPureSignal    = false,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = false,
    .hasPaProfile     = true,        // mi0bot setup.cs:6432-6435 [v2.10.3.13-beta2]
                                     // HL2 has FWD power sensor; PA cal applies
                                     // (1W intervals via Anan10 class — same
                                     // ud10PA1W..ud10PA10W spinbox set as
                                     // ANAN10/ANAN10E per setup.cs:5463-5466).
    .hasBandwidthMonitor = true,
    .hasIoBoardHl2    = true,
    .hasSidetoneGenerator = true,
    .hasMicJack       = false,  // HL2 has no radio-side mic input (3M-1b §11)
    .hasApollo        = false,  // HL2 has no Apollo port; not in RadioModelChanged() switch
    .hasAlex          = false,  // HL2 has no Alex board slot (ocOutputCount=0, hasAlexFilters=false)
    .hasPennyLane     = false,  // HL2 has no OC ext-ctrl; uses IoBoardHl2 for I2C accessories
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    .displayName      = "Hermes Lite 2",
    .sourceCitation   = "network.h:454, IoBoardHl2.cs, Setup.cs:1082-1093,:16083-16086",
};

// ─── HermesLiteRxOnly ───────────────────────────────────────────────────────
// NereusSDR-original SKU slot — Phase 3M-0 Task 1.
// HL2 RX-only kits ship without a TX driver board.  All capabilities match
// standard HermesLite (kHermesLite) except isRxOnlySku=true.
// Thetis treats RX-only purely as a user toggle (chkGeneralRXOnly,
// console.cs:15283-15307 [v2.10.3.13]); NereusSDR adds this SKU-level flag
// so HL2 RX-only kits are hard-blocked from TX regardless of user settings.
// Not a Thetis wire-format value — integer 12, above the 0-11 Thetis range.
const BoardCapabilities kHermesLiteRxOnly = {
    .board            = HPSDRHW::HermesLiteRxOnly,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 4,
    .sampleRates      = {48000, 96000, 192000, 384000, 0, 0},
    .maxSampleRate    = 384000,
    // HL2 RX-only inherits the standard HL2 signed −28..+32 dB ATT range
    // (mi0bot setup.cs:16085-16086 [v2.10.3.13-beta2]).
    .attenuator       = {-28, 32, 1, true, 0x3F, 0x40, true},
    .preamp           = {false, false},
    .ocOutputCount    = 0,
    .hasAlexFilters   = false,
    .hasAlexTxRouting = false,
    .xvtrJackCount    = 0,
    .antennaInputCount = 1,
    .hasAlex2         = false,
    .hasRxBypassRelay = false,
    .rxOnlyAntennaCount = 0,
    .isRxOnlySku      = true,   // HL2 RX-only kit — no TX driver board
    .canDriveGanymede = false,
    .hasPureSignal    = false,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = false,
    .hasPaProfile     = false,
    .hasBandwidthMonitor = true,
    .hasIoBoardHl2    = true,
    .hasSidetoneGenerator = true,
    .hasMicJack       = false,  // HL2 RX-only has no radio-side mic input (3M-1b §11)
    .hasApollo        = false,
    .hasAlex          = false,
    .hasPennyLane     = false,
    .minFirmwareVersion = 0,
    .knownGoodFirmware  = 0,
    .displayName      = "Hermes Lite 2 (RX-only)",
    .sourceCitation   = "NereusSDR-original Phase 3M-0; based on kHermesLite",
};

// ─── Saturn (ANAN-G2) ───────────────────────────────────────────────────────
// Source: network.h:455 (Saturn=10), enums.cs:397, clsHardwareSpecific.cs:164-176
// ADC: SetRxADC(2) — dual ADC. Protocol 2.
//   (clsHardwareSpecific.cs HasAudioAmplifier: Saturn = P2 protocol)
// P2 sample rates: all six (Setup.cs:854)
// Diversity + PureSignal: yes (console.cs GetDDC P2 Saturn branch:8598)
// PSDefaultPeak = 0.6121 for Saturn (clsHardwareSpecific.cs:323-324 P2 switch)
// Correction vs plan baseline: maxSampleRate updated to 1536000 (P2 board)
// TODO(3I-T2): verify firmware versions for Saturn/ANAN-G2
//
// Phase 3P-F Task 2 (accessories):
//   hasApollo=false — setup.cs:20203-20206 chkApolloPresent.Enabled=false for ANAN_G2.
//   hasAlex=true — chkAlexPresent.Checked=true, Enabled=true (setup.cs:20201-20202).
//   hasPennyLane=true — tpPennyCtrl added for all HPSDR models including G2 (setup.cs:6364).
//     Tab labeled "OC Control" for non-HERMES boards (setup.cs:6328).
const BoardCapabilities kSaturn = {
    .board            = HPSDRHW::Saturn,
    .protocol         = ProtocolVersion::Protocol2,
    .adcCount         = 2,
    .maxReceivers     = 7,
    .sampleRates      = {48000, 96000, 192000, 384000, 768000, 1536000},
    .maxSampleRate    = 1536000,
    .attenuator       = {0, 31, 1, true, 0x1F, 0x20, false},
    .preamp           = {true, true},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasAlex2         = true,
    .hasRxBypassRelay = true,
    .rxOnlyAntennaCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,  // chkApolloPresent.Enabled=false for ANAN_G2 (setup.cs:20203)
    .hasAlex          = true,   // chkAlexPresent.Checked=true, Enabled=true (setup.cs:20201)
    .hasPennyLane     = true,   // tpPennyCtrl added for all HPSDR models (setup.cs:6364); "OC Control"
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    .displayName      = "ANAN-G2 (Saturn)",
    .sourceCitation   = "network.h:455, enums.cs:397, clsHardwareSpecific.cs:164-176",
};

// ─── SaturnMKII (ANAN-G2 MkII board revision) ───────────────────────────────
// Source: network.h:456 (SaturnMKII=11), enums.cs:398
// Comment in enums.cs: "ANAN-G2: MKII board?" — planned variant of Saturn.
// Capabilities identical to Saturn; protocol P2 by derivation from Saturn base.
// Correction vs plan baseline: maxSampleRate updated to 1536000 (P2 board)
// TODO(3I-T2): verify SaturnMKII vs Saturn capability differences when hardware ships
//
// Phase 3P-F Task 2 (accessories):
//   Matches ANAN_G2_1K case in Thetis (setup.cs:20254-20257): hasApollo=false,
//   hasAlex=true, hasPennyLane=true (same as Saturn, derived from ANAN_G2_1K case).
const BoardCapabilities kSaturnMKII = {
    .board            = HPSDRHW::SaturnMKII,
    .protocol         = ProtocolVersion::Protocol2,
    .adcCount         = 2,
    .maxReceivers     = 7,
    .sampleRates      = {48000, 96000, 192000, 384000, 768000, 1536000},
    .maxSampleRate    = 1536000,
    .attenuator       = {0, 31, 1, true, 0x1F, 0x20, false},
    .preamp           = {true, true},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasAlex2         = true,
    .hasRxBypassRelay = true,
    .rxOnlyAntennaCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,  // chkApolloPresent.Enabled=false for ANAN_G2_1K (setup.cs:20256)
    .hasAlex          = true,   // chkAlexPresent.Checked=true, Enabled=true (setup.cs:20254)
    .hasPennyLane     = true,   // tpPennyCtrl added for all HPSDR models (setup.cs:6364); "OC Control"
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    .displayName      = "ANAN-G2 MkII (SaturnMKII)",
    .sourceCitation   = "network.h:456, enums.cs:398 \"ANAN-G2: MKII board?\"",
};

// ─── Andromeda (Andromeda console family — Ganymede 500W PA) ────────────────
// NereusSDR-original SKU slot — Phase 3M-0 Task 1.
// The Andromeda console family connects to a Ganymede 500W PA.
// canDriveGanymede captures the PA-trip capability flag per
// Andromeda.cs:914-920 [v2.10.3.13].
// Capabilities derived from kSaturn (the closest P2 full-featured board in the
// table) with canDriveGanymede=true.  Full hardware details TBD when Andromeda
// board specs are confirmed (3M-1 epic).
// Not a Thetis wire-format value — integer 20, above the 0-11 Thetis range.
const BoardCapabilities kAndromeda = {
    .board            = HPSDRHW::Andromeda,
    .protocol         = ProtocolVersion::Protocol2,
    .adcCount         = 2,
    .maxReceivers     = 7,
    .sampleRates      = {48000, 96000, 192000, 384000, 768000, 1536000},
    .maxSampleRate    = 1536000,
    .attenuator       = {0, 31, 1, true, 0x1F, 0x20, false},
    .preamp           = {true, true},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasAlex2         = true,
    .hasRxBypassRelay = true,
    .rxOnlyAntennaCount = 3,
    .isRxOnlySku      = false,
    .canDriveGanymede = true,   // Andromeda console drives Ganymede 500W PA (Andromeda.cs:914-920 [v2.10.3.13])
    .hasPureSignal    = true,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,
    .hasAlex          = true,
    .hasPennyLane     = true,
    .minFirmwareVersion = 0,
    .knownGoodFirmware  = 0,
    .displayName      = "Andromeda (Ganymede PA)",
    .sourceCitation   = "NereusSDR-original Phase 3M-0; based on kSaturn; Andromeda.cs:914-920 [v2.10.3.13]",
};

// ─── Unknown (fallback) ─────────────────────────────────────────────────────
// Safe defaults for unrecognised boards. Used as forBoard() fallback.
const BoardCapabilities kUnknown = {
    .board            = HPSDRHW::Unknown,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 1,
    .sampleRates      = {48000, 0, 0, 0, 0, 0},
    .maxSampleRate    = 48000,
    .attenuator       = {0, 0, 0, false, 0x1F, 0x20, false},
    .preamp           = {false, false},
    .ocOutputCount    = 0,
    .hasAlexFilters   = false,
    .hasAlexTxRouting = false,
    .xvtrJackCount    = 0,
    .antennaInputCount = 1,
    .hasAlex2         = false,
    .hasRxBypassRelay = false,
    .rxOnlyAntennaCount = 0,
    .hasPureSignal    = false,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = false,
    .hasPaProfile     = false,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    // Phase 3M-1b Task I.4: Unknown board uses the global TransmitModel
    // fallback range (-50/+70) rather than the Thetis runtime defaults
    // (-40/+10), so a user with an unidentified board still has the widest
    // permissible slider range.
    // From Thetis console.cs:19151-19171 [v2.10.3.13]: defaults are -40/+10;
    // TransmitModel::kMicGainDbMin/Max = -50/+70 is NereusSDR's outer bound.
    .micGainMinDb     = -50,
    .micGainMaxDb     = +70,
    .hasApollo        = false,  // unknown board — all accessories disabled
    .hasAlex          = false,
    .hasPennyLane     = false,
    .minFirmwareVersion = 0,
    .knownGoodFirmware  = 0,
    .displayName      = "Unknown board",
    .sourceCitation   = "fallback — no Thetis source for HPSDRHW::Unknown",
};

// const (not constexpr) because BoardCapabilities contains QList<SaturnBpf1Edge>
// which is not constexpr-compatible.  Introduced in Phase 3P-B Task 6.
// Size bumped from 10 → 12 in Phase 3M-0 Task 1 (added kHermesLiteRxOnly,
// kAndromeda). kUnknown remains last as the forBoard() fallback.
const std::array<BoardCapabilities, 12> kTable = {
    kAtlas, kHermes, kHermesII, kAngelia, kOrion,
    kOrionMKII, kHermesLite, kHermesLiteRxOnly,
    kSaturn, kSaturnMKII, kAndromeda, kUnknown
};

} // anonymous namespace

namespace BoardCapsTable {

const BoardCapabilities& forBoard(HPSDRHW hw) noexcept {
    for (const auto& e : kTable) {
        if (e.board == hw) { return e; }
    }
    return kUnknown;
}

const BoardCapabilities& forModel(HPSDRModel m) noexcept {
    return forBoard(boardForModel(m));
}

std::span<const BoardCapabilities> all() noexcept {
    return std::span<const BoardCapabilities>(kTable.data(), kTable.size());
}

// --- Per-model preamp item tables ---
// From Thetis console.cs:40755-40825 SetComboPreampForHPSDR().
// PreampMode mapping: 0=Off(-20dB HPSDR), 1=On(0dB), 2=Minus10, 3=Minus20,
//                     4=Minus30, 5=Minus40, 6=Minus50.
// Upstream inline attribution preserved verbatim:
//   :40790  case HPSDRModel.REDPITAYA: // DH1KLM: changed to enable on_off_preamp_settings for OpenHPSDR compat. DIY PA/Filter boards
//   :40805  // case HPSDRModel.REDPITAYA: // DH1KLM: removed for compatibility reasons
//   :40813  ... HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM

// on_off_preamp_settings = { "0dB", "-20dB" }
static constexpr PreampItem kOnOff[] = {
    {"0dB",   1},  // PreampMode::On
    {"-20dB", 0},  // PreampMode::Off
};

// anan100d_preamp_settings = { "0dB", "-10dB", "-20dB", "-30dB" }
static constexpr PreampItem kAnan100d[] = {
    {"0dB",   1},  // PreampMode::On
    {"-10dB", 2},  // PreampMode::Minus10
    {"-20dB", 3},  // PreampMode::Minus20
    {"-30dB", 4},  // PreampMode::Minus30
};

// alex_preamp_settings = { "-10db", "-20db", "-30db", "-40db", "-50db" }
// (lowercase "db" distinguishes ALEX modes from SA modes in Thetis)

// Combined: on_off + alex (for HPSDR/Hermes/ANAN100/etc with ALEX)
static constexpr PreampItem kOnOffPlusAlex[] = {
    {"0dB",   1},  // PreampMode::On
    {"-20dB", 0},  // PreampMode::Off
    {"-10db", 2},  // PreampMode::Minus10 (ALEX)
    {"-20db", 3},  // PreampMode::Minus20 (ALEX)
    {"-30db", 4},  // PreampMode::Minus30 (ALEX)
    {"-40db", 5},  // PreampMode::Minus40 (ALEX)
    {"-50db", 6},  // PreampMode::Minus50 (ALEX)
};

// Hermes Lite 2: uses anan100d 4-step set (Off / -10 / -20 / -30 dB).
// HL2 is not in Thetis SetComboPreampForHPSDR (it postdates that switch),
// but its LNA supports the same 4-level control as the anan100d set.
// Per spec §8 and mi0bot HL2 LNA design [@c26a8a4].
// Phase 3P-C Step 2 correction: was 1-item "On only" — updated to anan100d.

// Helper to produce dynamic-extent spans from static arrays.
template<std::size_t N>
static constexpr std::span<const PreampItem> items(const PreampItem (&a)[N]) noexcept {
    return {a, N};
}

std::span<const PreampItem> preampItemsForBoard(HPSDRHW hw, bool alexPresent) noexcept
{
    // From Thetis console.cs:40755 SetComboPreampForHPSDR — per-model switch.
    switch (hw) {
    case HPSDRHW::Atlas:
        return alexPresent ? items(kOnOffPlusAlex)
                           : items(kOnOff);

    case HPSDRHW::Hermes:
        // Hermes: with ALEX → on_off + alex; without → anan100d (4-step).
        // ANAN-10 uses Hermes board but always gets anan100d (no ALEX check).
        // ANAN-100 uses Hermes board but always gets on_off+alex.
        // Since we dispatch by HPSDRHW not HPSDRModel, and Hermes covers
        // both ANAN-10 and ANAN-100, we use the ALEX flag to differentiate.
        return alexPresent ? items(kOnOffPlusAlex)
                           : items(kAnan100d);

    case HPSDRHW::HermesII:
        // HermesII: ANAN-10E (no ALEX, anan100d) and ANAN-100B (always on_off+alex).
        return alexPresent ? items(kOnOffPlusAlex)
                           : items(kAnan100d);

    case HPSDRHW::Angelia:
        // ANAN-100D: with ALEX → on_off+alex; without → anan100d.
        return alexPresent ? items(kOnOffPlusAlex)
                           : items(kAnan100d);

    case HPSDRHW::Orion:
        // ANAN-200D: same as Angelia.
        return alexPresent ? items(kOnOffPlusAlex)
                           : items(kAnan100d);

    case HPSDRHW::OrionMKII:
    case HPSDRHW::Saturn:
        // ANAN-7000D/8000D/OrionMkII/G2/G2-1K/AnvelinaPro3: always anan100d.
        return items(kAnan100d);

    case HPSDRHW::HermesLite:
        // HL2: anan100d 4-step set (Off / -10 / -20 / -30 dB). Not in Thetis
        // SetComboPreampForHPSDR switch (HL2 postdates it); uses anan100d set
        // per spec §8 and mi0bot HL2 LNA design [@c26a8a4]. Phase 3P-C Step 2.
        return items(kAnan100d);

    default:
        return items(kAnan100d);
    }
}

std::span<const PreampItem> rx2PreampItemsForBoard(HPSDRHW hw) noexcept
{
    // From Thetis console.cs:40815 — RX2 always uses either on_off or anan100d.
    // Upstream inline attribution preserved verbatim (console.cs:40813):
    //   ... HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    switch (hw) {
    case HPSDRHW::Angelia:
    case HPSDRHW::Orion:
    case HPSDRHW::OrionMKII:
    case HPSDRHW::Saturn:
        return items(kAnan100d);
    default:
        return items(kOnOff);
    }
}

int stepAttMaxDb(HPSDRHW hw, bool alexPresent) noexcept
{
    // Phase 3P-A Task 15: delegate to BoardCapabilities::attenuator.maxDb
    // as the single source of truth.
    //
    // HL2 stores maxDb=63 (mi0bot [@c26a8a4] 6-bit LNA range); all
    // standard boards store 31. Alex-equipped boards that are not in the
    // Thetis exclusion list (setup.cs:15765 [v2.10.3.13]) can reach 61
    // via the Alex relay — that is captured by hasAlexRelayAttMax in the
    // caps table and applied below.
    const BoardCapabilities& caps = forBoard(hw);
    if (!caps.attenuator.present) { return 0; }

    // HL2 (and any future board with a custom maxDb) bypasses Alex logic:
    // its native range is already encoded in attenuator.maxDb.
    if (caps.attenuator.maxDb != 31) { return caps.attenuator.maxDb; }

    // Standard 31 dB boards: Alex presence can extend to 61 for boards
    // that are NOT in Thetis's exclusion list. The exclusion list in
    // Thetis setup.cs:15765 excludes OrionMKII, Saturn (G2/G2-1K),
    // HermesLite, AnvelinaPro3, and RedPitaya. We map by HPSDRHW.
    // From Thetis setup.cs:15765 [v2.10.3.13].
    if (!alexPresent) { return 31; }

    switch (hw) {
    case HPSDRHW::Atlas:
    case HPSDRHW::Hermes:
    case HPSDRHW::HermesII:
    case HPSDRHW::Angelia:
    case HPSDRHW::Orion:
        return 61;
    default:
        return 31;
    }
}

} // namespace BoardCapsTable
} // namespace NereusSDR
