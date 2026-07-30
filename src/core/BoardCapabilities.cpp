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
// Upstream tags preserved: //N1GP (from cited network.h:446) [v2.10.3.15]
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
    .maxSlices        = 3,   // Phase 3F: Atlas/Metis 3-slice cap (1-ADC, 3 DDCs, no PS support)
    .userDdcCount     = 3,   // Phase 3F Sub-Epic I: Metis user DDCs = DDC0-2 (design doc §2)
    .widebandAdcs     = 0,   // Phase 3F: P1 board — wideband mechanism differs; deferred to 3F-W
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
// Upstream tags preserved: //N1GP (from cited network.h:446) [v2.10.3.15]
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
    .maxSlices        = 4,   // Phase 3F: Hermes 4-slice cap (1-ADC, 4 DDCs; PS reclaims DDC0+1 on TX)
    .userDdcCount     = 4,   // Phase 3F Sub-Epic I: Hermes user DDCs = DDC0-3 (design doc §2)
    .widebandAdcs     = 0,   // Phase 3F: P1 board — wideband mechanism differs; deferred to 3F-W
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
    // Phase 3M-4 Task 17 P1 follow-up: enable PureSignal for plain Hermes.
    // From mi0bot console.cs:8408-8413 [v2.10.3.13-beta2] HERMES is grouped
    // with HL2 / ANAN10 / ANAN100 (nddc=4 family), all of which hit the
    // PS-on UpdateDDCs branch at line 8469-8488 with ddcEnable=DDC0,
    // syncEnable=DDC1, cntrl1=4 ADC steering for the PS feedback path.
    // P1CodecStandard handles the wire compose; P1RadioConnection
    // ::applyPsDdcConfig consumes the codec output.
    //
    // Inline tag preserved per CLAUDE.md "Inline comment preservation":
    //MI0BOT  [original `case HPSDRModel.HERMESLITE: // MI0BOT: HL2`
    //         attribution at console.cs:8409 — HL2 case-statement marker]
    .hasPureSignal    = true,
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
// Upstream tags preserved: //N1GP (from cited network.h:446) [v2.10.3.15]
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
    .maxSlices        = 2,   // Phase 3F: HermesII 2-slice cap (1-ADC, 2 DDCs)
    .userDdcCount     = 2,   // Phase 3F Sub-Epic I: HermesII user DDCs = DDC0-1 (design doc §2)
    .widebandAdcs     = 0,   // Phase 3F: P1 board — wideband mechanism differs; deferred to 3F-W
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
    // PureSignal hardware-default peak: P1 default 0.4072.
    // From Thetis clsHardwareSpecific.cs:297 [v2.10.3.13]
    //   if (NetworkIO.CurrentRadioProtocol == RadioProtocol.USB) // protocol 1
    //   { ... default: return 0.4072; ... }
    .psDefaultPeak    = 0.4072,
    // PS feedback rate: 192000 (cmaster.cs:424 ps_rate=192000 [v2.10.3.13]).
    .psSampleRate     = 192000,
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
// Upstream tags preserved: //N1GP (from cited network.h:446) [v2.10.3.15]
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
    .maxSlices        = 5,   // Phase 3F: Angelia 5-slice cap (2-ADC, 7 DDCs; DDC0+1 reserved for PS+Div)
    .userDdcCount     = 5,   // Phase 3F Sub-Epic I: Angelia user DDCs = DDC2-6 (design doc §2)
    .widebandAdcs     = 0,   // Phase 3F: P1 board — wideband mechanism differs; deferred to 3F-W
                             // Was 2, which contradicted every other Protocol1 row and this
                             // row's own .protocol field. NereusSDR has no P1 wideband receive
                             // path, so advertising ADCs for it let extended view bypass the
                             // Alex preselector for a stream that never arrives, losing receive
                             // filtering for nothing. (Codex review round 5, PR #293.)
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
    // PureSignal hardware-default peak: P1 default 0.4072.
    // From Thetis clsHardwareSpecific.cs:297 [v2.10.3.13] (P1 default branch).
    .psDefaultPeak    = 0.4072,
    // PS feedback rate: 192000 (cmaster.cs:424 ps_rate=192000 [v2.10.3.13]).
    .psSampleRate     = 192000,
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
    .maxSlices        = 5,   // Phase 3F: Orion 5-slice cap (2-ADC, 7 DDCs; DDC0+1 reserved for PS+Div)
    .userDdcCount     = 5,   // Phase 3F Sub-Epic I: Orion user DDCs = DDC2-6 (design doc §2)
    .widebandAdcs     = 0,   // Phase 3F: P1 board — wideband mechanism differs; deferred to 3F-W
                             // Was 2, which contradicted every other Protocol1 row and this
                             // row's own .protocol field. NereusSDR has no P1 wideband receive
                             // path, so advertising ADCs for it let extended view bypass the
                             // Alex preselector for a stream that never arrives, losing receive
                             // filtering for nothing. (Codex review round 5, PR #293.)
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
    // PureSignal hardware-default peak: P1 default 0.4072.
    // From Thetis clsHardwareSpecific.cs:297 [v2.10.3.13] (P1 default branch).
    .psDefaultPeak    = 0.4072,
    // PS feedback rate: 192000 (cmaster.cs:424 ps_rate=192000 [v2.10.3.13]).
    .psSampleRate     = 192000,
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
    // Two driven RX filter chains. ORIONMKII, ANAN7000D, ANAN8000D,
    // ANVELINAPRO3 and REDPITAYA all resolve to this HW row and all five are
    // in the setAlex2HPF model list at console.cs:15435-15443 [v2.10.3.15].
    .rxFilterChainCount = 2,
    .maxReceivers     = 7,
    .maxSlices        = 5,   // Phase 3F: OrionMKII/7000D/8000D/AnvelinaPro3/RedPitaya 5-slice cap (2-ADC P2; DDC0+1 reserved)
    .userDdcCount     = 5,   // Phase 3F Sub-Epic I: OrionMKII/7000D/8000D/AnvelinaPro3/RedPitaya user DDCs = DDC2-6 (design doc §2)
    .widebandAdcs     = 2,   // Phase 3F: 2-ADC P2 board — ADC0 + ADC1 both support wideband stream on ports 1027+1028
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
    // PureSignal hardware-default peak: P2 default 0.2899.
    // From Thetis clsHardwareSpecific.cs:307 [v2.10.3.13]
    //   else // protocol 2
    //   { ... default: return 0.2899; ... }
    .psDefaultPeak    = 0.2899,
    // PS feedback rate: 192000 (cmaster.cs:424 ps_rate=192000 [v2.10.3.13]).
    .psSampleRate     = 192000,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,  // chkApolloPresent.Enabled=false for ANAN7000D/8000D/AnvelinaPro3 (setup.cs:20100)
    .hasAlex          = true,   // chkAlexPresent.Checked=true, Enabled=true (setup.cs:20098)
    .hasPennyLane     = true,   // tpPennyCtrl added for all HPSDR models (setup.cs:6364)
    .hasPaVoltsTelemetry = true,  // HasVolts (clsHardwareSpecific.cs:249 [v2.10.3.15])
    .hasPaAmpsTelemetry  = true,  // HasAmps  (clsHardwareSpecific.cs:259 [v2.10.3.15])
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    // Phase 3P-B Task 6: OrionMKII family has independent per-ADC preamp control
    // (ANAN-7000DLE / 8000DLE / AnvelinaPro3).
    //
    // CORRECTION (2026-07-25): this comment used to read "OrionMKII uses
    // standard Alex HPF/LPF; Saturn BPF1 override is G2/G2-1K only".  That is
    // wrong, and it is where the wrong-ladder defect on 80m / 60m / 40m / 15m
    // came from.  Thetis dispatches OrionMKII to setBPF1ForOrionIISaturn right
    // alongside Saturn and HermesC10 (console.cs:6827-6837 [v2.10.3.15]), and
    // deskhpsdr independently groups NEW_DEVICE_ORION2 with NEW_DEVICE_SATURN
    // for the band-pass bank (new_protocol.c:1288-1290 [@f3d857c]).  OrionMKII
    // carries the MkII BAND-PASS front end, not the legacy high-pass ladder.
    // The ladder is selected by codec::alex::usesBpf1Preselector, which is the
    // single source of truth for the board split.
    //
    // p2SaturnBpf1Edges stays empty on every board: it is the per-band USER
    // OVERRIDE channel (populated from the Setup > Antenna > Alex-1 Filters
    // page, persisted under hardware/<mac>/alex/bpf1/<band>/{start,end}).
    // Empty means "use the shipped Thetis defaults", which now live in
    // codec::alex::computeBpf1 rather than being absent entirely.
    .p2PreampPerAdc   = true,
    .displayName      = "ANAN-7000DLE/8000DLE (OrionMkII)",
    .sourceCitation   = "network.h:453, enums.cs:395, clsHardwareSpecific.cs:143-190",
};

// ─── HermesC10 (ANAN-G2E, formerly G1) ──────────────────────────────────────
// From Thetis ChannelMaster/network.h:420-425 [v2.10.3.15] — upstream enum context:
//   HermesLite = 6,     // MI0BOT
//   Saturn = 10,        // ANAN-G2: added G8NJJ
//   HermesC10 = 20      // ANAN-G2E //N1GP G2E added (HermesC10)
// Source: network.h:425 (HermesC10=20) [v2.10.3.15], clsHardwareSpecific.cs:129-135 [v2.10.3.15]
//   N1GP G2E added — single-ADC entry-level G2 SKU; HERMES-class 4-DDC RX.
// Differs from ANAN_G2/G2_1K: no RX2 preamp, no RX2 stepped att, 1 ADC, no diversity.
// Shares Alex-2 (MKII BPF) routing with G2 family; PA gain table shared with G2/7000D tier.
// PA telemetry: HasVolts=true, HasAmps=true (clsHardwareSpecific.cs:245-264 [v2.10.3.15]).
const BoardCapabilities kHermesC10 = {
    .board            = HPSDRHW::HermesC10,
    // ANAN-G2E ships with N1GP community P2 (ETH) firmware that responds to P2
    // discovery and uses P2 wire format.  Thetis's static SKU classification
    // (Hermes-class) is a logical grouping for DSP/UI purposes; the wire
    // protocol is P2.  Empirically verified 2026-05-22 by discovery byte 20 ->
    // HermesC10 arriving via P2 reply, and successful CmdRx → DDC0 I/Q
    // streaming after the dither/mask fixes landed.
    .protocol         = ProtocolVersion::Protocol2,
    .adcCount         = 1,                                  // SetRxADC(1) [v2.10.3.15]
    .maxReceivers     = 4,                                  // P1_rxcount=4 nddc=4 (console.cs:8388 [v2.10.3.15])
    // 5 slices over 4 DDCs is legitimate: slices sharing a window share a DDC.
    // The old comment here read "2-ADC P2 class", which contradicts adcCount=1
    // two lines up; the G2E is Thetis's 1-ADC HERMES-class branch
    // (console.cs:8388 [v2.10.3.15]) and has no DDC0/DDC1 reservation of the
    // kind the 2-ADC rows describe.
    .maxSlices        = 5,   // Phase 3F: HermesC10/G2E 5-slice cap (1-ADC HERMES class; slices may share a DDC)
    // Phase 3F Sub-Epic I closeout, defect F2: was 5 with a "DDC2-6" comment.
    // Both were wrong. Thetis asks for 4 on this family (console.cs:8391-8392
    // [v2.10.3.15]: P1_rxcount = 4; nddc = 4) and places rx1 on DDC0, not DDC2
    // (console.cs:8610-8642 [v2.10.3.15] GetDDC groups HermesC10 with Hermes
    // and HermesII on rx1 = 0). A fifth stream was allocatable but left at
    // streamDdc[4] == -1, so it drew no enable bit and its slice was silently
    // dead. maxReceivers=4 on the line above was already telling the truth.
    //
    // Per the header's policy-vs-hardware caveat: 4 is Thetis's client policy,
    // not a verified hardware bound. The G2E has no public gateware to check it
    // against (see docs/attribution/GATEWARE-PROVENANCE.md), so this is the
    // best evidence available rather than silicon truth. Both codecs that serve
    // this SKU agree at 4 (P2CodecHermes::familyDdcCount(), and P1CodecStandard
    // on P1), so nothing downstream is currently reaching past it.
    .userDdcCount     = 4,   // Phase 3F Sub-Epic I: HermesC10/G2E user DDCs = DDC0-3 (console.cs:8391-8392 [v2.10.3.15])
    // Bounded by adcCount, and unlike the DDC counts above this one IS a
    // hardware bound: ADC count is a physical part on the board, not a Verilog
    // synthesis parameter that moves between firmware builds. The G2E has one
    // ADC (clsHardwareSpecific.cs:130 [v2.10.3.15] SetRxADC(1)), so there is no
    // ADC1 to carry a second wideband stream.
    //
    // Was 2 with a "P2 2-ADC class" comment inherited from the design doc §2
    // table, which mis-filed the G2E among the 2-ADC boards. That contradicted
    // the adcCount = 1 line at the top of this row.
    //
    // Note that Thetis only ever enables ADC0's wideband stream on any board:
    // SetWBEnable(adc, enable) sets one bit per ADC in wb_enable (wire byte 23,
    // ChannelMaster/network.c:879 [v2.10.3.15]), and both call sites pass 0
    // (console.cs:43558 on, wideband.cs:52 off).
    //N1GP G2E added  [original inline tag from clsHardwareSpecific.cs:129 — ANAN_G2E case label]
    .widebandAdcs     = 1,   // Phase 3F: 1-ADC board — ADC0 only (SetRxADC(1), clsHardwareSpecific.cs:130 [v2.10.3.15])
    // Thetis setup.cs:849-850 [v2.10.3.15] — every P2/ETH board gets the
    // full 6-rate list; Thetis has no per-board cap, only per-protocol.
    // Pcap of working Thetis-on-G2E ran at 768 kHz (CmdRx byte 18-19 =
    // 0x03 0x00 = 768 kHz big-endian).
    .sampleRates      = {48000, 96000, 192000, 384000, 768000, 1536000},
    .maxSampleRate    = 1536000,
    // RX1 stepped att: 0..31 dB, G2E in exclusion list (setup.cs:15810-15824 [v2.10.3.15]).
    // RX2: HasSteppedAttenuation(rx==2)==false (clsHardwareSpecific.cs:790-803 [v2.10.3.15]).
    .attenuator       = {0, 31, 1, true, 0x1F, 0x20, false},
    // RX1 preamp present; RX2 preamp explicitly absent (console.cs:14835 [v2.10.3.15]).
    .preamp           = {true, false},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasAlex2         = true,   // SetMKIIBPF(1) (clsHardwareSpecific.cs:131 [v2.10.3.15])
    .hasRxBypassRelay = true,
    .rxOnlyAntennaCount = 3,
    .hasPureSignal    = true,   // P1_rxcount=4 nddc=4; RX4 = PS feedback (console.cs:8388 [v2.10.3.15])
    .psDefaultPeak    = 0.2899, // P2 default (clsHardwareSpecific.cs:307 [v2.10.3.15])
    .psSampleRate     = 192000, // cmaster.cs:424 ps_rate=192000 [v2.10.3.15]
    .hasDiversityReceiver = false, // 1 ADC — no diversity
    .hasStepAttenuatorCal = true,  // RX1 stepped att present and calibratable
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,  // chkApolloPresent.Enabled=false (setup.cs:19908 [v2.10.3.15])
    .hasAlex          = true,   // chkAlexPresent.Checked=true (setup.cs:19905 [v2.10.3.15])
    .hasPennyLane     = true,   // tpPennyCtrl added for all HPSDR models (setup.cs:6364)
    .hasPaVoltsTelemetry = true,    // HasVolts true (clsHardwareSpecific.cs:250 [v2.10.3.15]) //N1GP G2E added
    .hasPaAmpsTelemetry  = true,    // HasAmps  true (clsHardwareSpecific.cs:260 [v2.10.3.15]) //N1GP G2E added
    .allowsAutoPaCalibrate = false, // chkAutoPACalibrate.Visible=false (setup.cs:19919 [v2.10.3.15]) //N1GP G2E added
    .showsBypassPaSettingsUi = true, // chkBypassANANPASettings.Visible=true (setup.cs:19921 [v2.10.3.15]) //N1GP G2E added
    .minFirmwareVersion = 0,
    .knownGoodFirmware  = 0,
    .p2PreampPerAdc   = false,  // 1 ADC; per-ADC control not applicable
    .displayName      = "ANAN-G2E",
    .sourceCitation   = "network.h:425, clsHardwareSpecific.cs:129-135 / 245-264 / 699-730 / 790-803, "
                        "console.cs:8388/14835/25007 [v2.10.3.15]; //N1GP G2E added",
};

// ─── HermesLite (HL2) ───────────────────────────────────────────────────────
// Source: network.h:454 (HermesLite=6), IoBoardHl2.cs, clsHardwareSpecific.cs:94-99
// ADC: SetRxADC(1) — single ADC. Protocol 1.
// HL2 supports 384k (Setup.cs:850-853 "The HL supports 384K", include_extra_p1_rate=true)
// Attenuator: HL2 uses LNA range -28..+31 (59 dB span), step 1
//   (Setup.cs:1084-1088 PerformDelayedInitialisation, :16085-16086 udHermesStepAttenuatorData)
//   NOTE: mi0bot widens the upper bound to +32 at runtime (console.cs:11043
//   [v2.10.3.13-beta2] RadioModelChanged HL2 branch) but that is an
//   off-by-one upstream bug — wire encoding `31 - userDb` (console.cs:11075)
//   produces wire = -1 at userDb=32 which 6-bit-masks to 0x3F, the same
//   wire value the chip would interpret as max-LNA-gain wraparound, NOT
//   "32 dB attenuation".  mi0bot's own InitConsole at console.cs:2111
//   sets Max=31 (chip-correct).  NereusSDR caps at +31 per maintainer
//   approval (issue #175 follow-up bench finding).
// No Alex filters or OC outputs on HL2.
// hasBandwidthMonitor: HL2 has bandwidth_monitor via IoBoardHl2
//   (ChannelMaster/bandwidth_monitor.h + IoBoardHl2.cs)
// hasIoBoardHl2: true — IoBoardHl2.cs present and active for HL2
// hasSidetoneGenerator: true — HL2 firmware implements sidetone
//   (IoBoardHl2.cs references sidetone register)
// maxReceivers: 4 for HL2 (console.cs GetDDC HermesLite branch:8734, same as Hermes)
// Firmware floor: NONE — both ramdor and mi0bot accept any HL2 firmware version
//   (NetworkIO.cs:136-143 [v2.10.3.13] guards only `HermesII && CodeVersion < 103`,
//   no HL2 branch).  Therefore minFirmwareVersion=0 / knownGoodFirmware=0
//   below are source-first correct: NereusSDR mirrors mi0bot by NOT enforcing
//   a firmware floor for HL2.  External (non-source) HL2 community
//   recommendations exist around mi0bot HL2 firmware versions but are
//   intentionally not encoded here per the source-first rule — recommending
//   versions the upstream doesn't enforce would be NereusSDR-original gating.
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
    .maxSlices        = 1,   // Phase 3F: HL2 single-slice cap (1-ADC; DDCs reserved for firmware quirks)
    .userDdcCount     = 1,   // Phase 3F Sub-Epic I: HL2 user DDCs = DDC0 only (design doc §2)
    .widebandAdcs     = 0,   // Phase 3F: P1 board — wideband mechanism differs; deferred to 3F-W
    .sampleRates      = {48000, 96000, 192000, 384000, 0, 0},
    .maxSampleRate    = 384000,
    // HL2: signed user-facing range −28..+31 dB.  mi0bot widens to +32 at
    // console.cs:11043 [v2.10.3.13-beta2] (RadioModelChanged HL2 branch) but
    // that is an off-by-one upstream bug: wire encoding `31 - userDb`
    // (console.cs:11075) produces wire = -1 at userDb=32 which 6-bit-masks
    // to 0x3F, indistinguishable from the LNA-gain wraparound region.
    // mi0bot's own InitConsole at console.cs:2111 sets Max=31 (chip-correct).
    // NereusSDR honors the chip-correct cap per maintainer approval
    // (issue #175 follow-up bench finding).
    // Wire conversion `wire = 31 - userDb` lives in P1CodecHl2.cpp; mask=0x3F,
    // enableBit=0x40 (6-bit field per mi0bot WriteMainLoop_HL2 [@c26a8a4]);
    // MOX branches ATT.
    .attenuator       = {-28, 31, 1, true, 0x3F, 0x40, true},
    .preamp           = {false, false},
    .ocOutputCount    = 0,
    .hasAlexFilters   = false,
    .hasAlexTxRouting = false,
    .xvtrJackCount    = 0,
    .antennaInputCount = 1,
    .hasAlex2         = false,
    .hasRxBypassRelay = false,
    .rxOnlyAntennaCount = 0,
    // Phase 3M-4 Task 17 P1 follow-up: enable PureSignal for HL2.
    //
    // From mi0bot console.cs:8409-8488 [v2.10.3.13-beta2]:
    //   case HPSDRModel.HERMES:
    //   case HPSDRModel.HERMESLITE:     // MI0BOT: HL2
    //   case HPSDRModel.ANAN10:
    //   case HPSDRModel.ANAN100:
    //       P1_rxcount = 4;
    //       nddc = 4;
    //       ...
    //       else // transmitting and PS is ON
    //       {
    //           P1_DDCConfig = 6;
    //           DDCEnable = DDC0;
    //           SyncEnable = DDC1;
    //           if (hpsdr_model == HPSDRModel.HERMESLITE)  // HL2 high-rate
    //           { Rate[0] = rx1_rate; Rate[1] = rx1_rate; }
    //           else
    //           { Rate[0] = ps_rate; Rate[1] = ps_rate; }
    //           cntrl1 = 4;
    //           cntrl2 = 0;
    //       }
    //
    // P1CodecHl2::applyPureSignalDdcConfig (port at P1CodecHl2.cpp:461)
    // already implements this branch.  The previously-deferred work was
    // wiring P1RadioConnection::applyPsDdcConfig to consume that output —
    // landed in this commit.  Bench validation pending; see
    // docs/architecture/phase3m-4-handoff-bench-debug.md "P1/HL2 PS bring-up".
    .hasPureSignal    = true,
    // PureSignal hardware-default peak: HL2-specific 0.233.  MI0BOT.
    // From mi0bot-Thetis clsHardwareSpecific.cs:312 [v2.10.3.13-beta2]
    //   if (NetworkIO.CurrentRadioProtocol == RadioProtocol.USB) // protocol 1
    //   { case HPSDRHW.HermesLite: return 0.233; ... }
    .psDefaultPeak    = 0.233,
    // PS feedback rate: 0 sentinel = "use rx1_rate" for HL2.  MI0BOT.
    // From mi0bot-Thetis console.cs:8472-8488 [v2.10.3.13-beta2]:
    //   else // transmitting and PS is ON
    //   {
    //       ...
    //       if (hpsdr_model == HPSDRModel.HERMESLITE) // MI0BOT: HL2 can work at a high sample rate
    //       {
    //           Rate[0] = rx1_rate;
    //           Rate[1] = rx1_rate;
    //       }
    //       else
    //       { Rate[0] = ps_rate; Rate[1] = ps_rate; }
    //   }
    .psSampleRate     = 0,
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
    .maxSlices        = 1,   // Phase 3F: HL2 RX-only 1-slice cap (mirrors kHermesLite; single-ADC firmware cap)
    .userDdcCount     = 1,   // Phase 3F Sub-Epic I: HL2 RX-only user DDCs = DDC0 only (design doc §2)
    .widebandAdcs     = 0,   // Phase 3F: P1 board — wideband mechanism differs; deferred to 3F-W
    .sampleRates      = {48000, 96000, 192000, 384000, 0, 0},
    .maxSampleRate    = 384000,
    // HL2 RX-only inherits the standard HL2 signed −28..+31 dB ATT range.
    // See kHermesLite for the maintainer-approved deviation from mi0bot's
    // off-by-one upstream bug at console.cs:11043 (issue #175 follow-up).
    .attenuator       = {-28, 31, 1, true, 0x3F, 0x40, true},
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
    // Phase 3M-4 Task 1: PS capability fields irrelevant on RX-only SKU
    // (no TX, so no PureSignal feedback path).  Populated to match HL2
    // family for table consistency.  Source: mirrors kHermesLite.
    .psDefaultPeak    = 0.233,
    .psSampleRate     = 0,
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
    // Two driven RX filter chains. ANAN_G2 and ANAN_G2_1K both resolve here
    // (clsHardwareSpecific.cs:164-177 [v2.10.3.15]) and both are in the
    // setAlex2HPF model list at console.cs:15435-15443 [v2.10.3.15].
    .rxFilterChainCount = 2,
    .maxReceivers     = 7,
    .maxSlices        = 5,   // Phase 3F: Saturn/ANAN-G2/G2-1K 5-slice cap (2-ADC P2; DDC0+1 reserved)
    .userDdcCount     = 5,   // Phase 3F Sub-Epic I: Saturn/ANAN-G2/G2-1K user DDCs = DDC2-6 (design doc §2)
    .widebandAdcs     = 2,   // Phase 3F: 2-ADC P2 board — ADC0 + ADC1 both support wideband stream on ports 1027+1028
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
    // PureSignal hardware-default peak: Saturn P2 special case 0.6121.
    // From Thetis clsHardwareSpecific.cs:304-305 [v2.10.3.13]
    //   else // protocol 2
    //   { case HPSDRHW.Saturn: return 0.6121; ... }
    .psDefaultPeak    = 0.6121,
    // PS feedback rate: 192000 (cmaster.cs:424 ps_rate=192000 [v2.10.3.13]).
    .psSampleRate     = 192000,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,  // chkApolloPresent.Enabled=false for ANAN_G2 (setup.cs:20203)
    .hasAlex          = true,   // chkAlexPresent.Checked=true, Enabled=true (setup.cs:20201)
    .hasPennyLane     = true,   // tpPennyCtrl added for all HPSDR models (setup.cs:6364); "OC Control"
    .hasPaVoltsTelemetry = true,  // HasVolts (clsHardwareSpecific.cs:251 [v2.10.3.15])
    .hasPaAmpsTelemetry  = true,  // HasAmps  (clsHardwareSpecific.cs:261 [v2.10.3.15])
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
    // Two driven RX filter chains, taken from the physical board rather than
    // from upstream list membership: HPSDRHW.SaturnMKII appears once in all
    // of Thetis (enums.cs:399 [v2.10.3.15], "ANAN-G2: MKII board?") and no
    // HPSDRModel resolves to it, so console.cs:15435-15443 cannot answer for
    // it. This row describes the ANAN-G2 MkII board revision, so it inherits
    // the ANAN-G2's two chains along with the rest of kSaturn.
    .rxFilterChainCount = 2,
    .maxReceivers     = 7,
    .maxSlices        = 5,   // Phase 3F: SaturnMKII 5-slice cap (2-ADC P2 Saturn variant; DDC0+1 reserved)
    .userDdcCount     = 5,   // Phase 3F Sub-Epic I: SaturnMKII user DDCs = DDC2-6 (design doc §2)
    .widebandAdcs     = 2,   // Phase 3F: 2-ADC P2 board — ADC0 + ADC1 both support wideband stream on ports 1027+1028
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
    // PureSignal hardware-default peak: Saturn-derived P2 case 0.6121.
    // From Thetis clsHardwareSpecific.cs:304-305 [v2.10.3.13]
    //   else // protocol 2
    //   { case HPSDRHW.Saturn: return 0.6121; ... }
    // SaturnMKII is the planned MkII board revision of Saturn (enums.cs:398
    // "ANAN-G2: MKII board?"); inherits the Saturn PS branch by derivation
    // until upstream adds a distinct case.
    .psDefaultPeak    = 0.6121,
    // PS feedback rate: 192000 (cmaster.cs:424 ps_rate=192000 [v2.10.3.13]).
    .psSampleRate     = 192000,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .hasApollo        = false,  // chkApolloPresent.Enabled=false for ANAN_G2_1K (setup.cs:20256)
    .hasAlex          = true,   // chkAlexPresent.Checked=true, Enabled=true (setup.cs:20254)
    .hasPennyLane     = true,   // tpPennyCtrl added for all HPSDR models (setup.cs:6364); "OC Control"
    .hasPaVoltsTelemetry = false,  // From Thetis HasVolts clsHardwareSpecific.cs:245-254 [v2.10.3.15] — SaturnMKII NOT in HasVolts SKU group
    .hasPaAmpsTelemetry  = false,  // From Thetis HasAmps  clsHardwareSpecific.cs:255-264 [v2.10.3.15] — same
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
    // Two driven RX filter chains, derived from kSaturn like the rest of this
    // row. Thetis has no HPSDRHW entry for Andromeda at all (enums.cs:389-402
    // [v2.10.3.15] stops at HermesC10), so console.cs:15435-15443 cannot
    // answer for it and this is a NereusSDR judgement, not an upstream fact.
    // Revisit with the rest of the row when Andromeda hardware specs land.
    .rxFilterChainCount = 2,
    .maxReceivers     = 7,
    .maxSlices        = 5,   // Phase 3F: Andromeda 5-slice cap (2-ADC P2; DDC0+1 reserved per Saturn class)
    .userDdcCount     = 5,   // Phase 3F Sub-Epic I: Andromeda user DDCs = DDC2-6 (design doc §2)
    .widebandAdcs     = 2,   // Phase 3F: 2-ADC P2 board — ADC0 + ADC1 both support wideband stream on ports 1027+1028
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
    // Phase 3M-4 Task 1: PureSignal hardware-default peak.
    // Andromeda is a NereusSDR-original SKU slot — no upstream Thetis HPSDRHW
    // enum entry for it.  Thetis clsHardwareSpecific.cs:300-310 PSDefaultPeak
    // [v2.10.3.13] only special-cases HPSDRHW.Saturn (=10) on the P2 branch
    // (returns 0.6121); all other P2 hardware values fall through to the
    // default branch returning 0.2899.  Andromeda gets the P2 default.
    .psDefaultPeak    = 0.2899,
    // PS feedback rate: 192000 (cmaster.cs:424 ps_rate=192000 [v2.10.3.13]).
    .psSampleRate     = 192000,
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
// kAndromeda). Bumped 12 → 13 in ANAN-G2E port (added kHermesC10). //N1GP G2E added
// kUnknown remains last as the forBoard() fallback.
const std::array<BoardCapabilities, 13> kTable = {
    kAtlas, kHermes, kHermesII, kAngelia, kOrion,
    kOrionMKII, kHermesC10, kHermesLite, kHermesLiteRxOnly,
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
    case HPSDRHW::HermesC10:  // ANAN-G2E //N1GP G2E added
        // ANAN-7000D/8000D/OrionMkII/G2/G2-1K/AnvelinaPro3/G2E: always anan100d.
        // From Thetis console.cs:40871-40879 [v2.10.3.15] //N1GP G2E added:
        //   case HPSDRModel.ANAN_G2E: comboPreamp.Items.AddRange(anan100d_preamp_settings)
        // NereusSDR dispatches by HPSDRHW; HermesC10 is the board for ANAN_G2E.
        // Upstream comment preserved verbatim (console.cs:40878):
        //   // case HPSDRModel.REDPITAYA: // DH1KLM: removed for compatibility reasons
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
