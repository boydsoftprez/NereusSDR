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
    // Default-init to Unknown so PureSignal::m_caps (default-constructed
    // BoardCapabilities) doesn't read uninitialized memory in the HL2
    // clamp branch added by PR #212 codex-fix B.  Production code always
    // overwrites this via applyBoardCapabilities() before the auto-att
    // path runs; the default protects against test fixtures that skip
    // the apply-caps call.
    HPSDRHW         board{HPSDRHW::Unknown};
    ProtocolVersion protocol;

    int  adcCount;
    int  maxReceivers;

    // Phase 3F: user-facing slice cap, distinct from maxReceivers (= total DDC count).
    // For 2-ADC boards this is typically maxReceivers - 2 (DDC0/1 reserved for PS + diversity).
    // For 1-ADC boards this often equals maxReceivers, except HL2 which is force-capped to 1.
    // See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §2.
    int  maxSlices {0};

    // Phase 3F Sub-Epic I: DDCs available for operator slices, after the
    // per-SKU PureSignal / diversity reservations. On 2-ADC P2 boards
    // DDC0+DDC1 are reserved as a synced pair, so user DDCs are DDC2-6.
    // Design doc §2 "Resolved values per SKU" is the authority; the
    // per-board codec's slice-to-DDC table must agree (for example
    // P2CodecSaturn::kSliceToDdc = {2,3,4,5,6}).
    //
    // This is a distinct axis from maxSlices: several slices can share one
    // DDC when their frequencies fall inside its window, so maxSlices can
    // legitimately exceed userDdcCount.
    int  userDdcCount {0};

    // Phase 3F: number of ADCs that support the wideband (real-sample) stream.
    // P2 boards: typically equals adcCount. P1 boards: 0 (different mechanism, deferred to 3F-W).
    int  widebandAdcs {0};

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

    // Antenna extensions (Phase 3P-I-a)
    // hasAlex2:           P2 second BPF bank (OrionMKII + Saturn family).
    //                     Gates Setup → Antenna → Alex-2 Filters sub-tab.
    //                     Source: setup.cs:6228 [v2.10.3.13 @501e3f5]
    //                     Upstream inline attribution preserved verbatim:
    //                       setup.cs:6223  HardwareSpecific.Model == HPSDRModel.REDPITAYA)//DH1KLM
    //                       setup.cs:6228  HardwareSpecific.Model == HPSDRModel.REDPITAYA))//DH1KLM
    // hasRxBypassRelay:   Physical RxOut relay present on the Alex board.
    //                     Source: HPSDR/Alex.cs:377 UpdateAlexAntSelection,
    //                     "rx_out" param. False on HL2/Atlas (no Alex).
    //                     Upstream inline attribution preserved verbatim:
    //                       HPSDR/Alex.cs:377  // G8NJJ support for external Aries ATU on antenna port 1
    // rxOnlyAntennaCount: Number of RX-only inputs (RX1/RX2/XVTR). 0 on
    //                     HL2/Atlas; 3 on Alex boards.
    //                     Source: HPSDR/Alex.cs:59 RxOnlyAnt[12] enum (1=rx1,2=rx2,3=xv).
    bool hasAlex2            {false};
    bool hasRxBypassRelay    {false};
    int  rxOnlyAntennaCount  {0};

    // Phase 3M-0 Task 1: SKU-level safety flags (NereusSDR-original).
    //
    // isRxOnlySku: True iff this SKU ships without TX hardware (Hermes Lite 2
    //   RX-only kits, etc.). Thetis treats RX-only purely as a user toggle
    //   (chkGeneralRXOnly, console.cs:15283-15307 [v2.10.3.13]); we add this
    //   so SKUs without TX drivers are hard-blocked regardless of user settings.
    bool isRxOnlySku     {false};

    // canDriveGanymede: True iff this SKU is the Andromeda-console family that
    //   connects to a Ganymede 500W PA (Andromeda.cs:914-920 [v2.10.3.13]).
    bool canDriveGanymede {false};

    bool hasPureSignal;

    // Phase 3M-4 Task 1: PureSignal hardware-default peak (per-protocol).
    //
    // From Thetis clsHardwareSpecific.cs:285-310 PSDefaultPeak [v2.10.3.13]:
    //   P1 (USB protocol, all boards):  return 0.4072
    //   P2 (UDP protocol, default):     return 0.2899
    //   P2 HPSDRHW.Saturn:              return 0.6121
    // Plus mi0bot HL2 override at clsHardwareSpecific.cs:300-330
    // [v2.10.3.13-beta2]:
    //   P1 HPSDRHW.HermesLite:          return 0.233
    //   P2 HPSDRHW.HermesLite:          return 0.233
    //
    // Type is double in Thetis (public static double PSDefaultPeak); preserved.
    // Used by PsForm to flag drift via pbWarningSetPk (PSForm.cs:802).
    // Boards without PureSignal keep the 0.0 default (irrelevant; gated by
    // hasPureSignal).
    double psDefaultPeak {0.0};

    // Phase 3M-4 Task 1: PureSignal feedback channel sample rate during MOX.
    //
    // 192000 for G2-class boards (cmaster.cs:424 ps_rate=192000 [v2.10.3.13]).
    // 0 sentinel = "use current rx1_rate" for HL2.  When MOX is asserted with
    // PS on, mi0bot console.cs:8472-8488 [v2.10.3.13-beta2] overrides the
    // default ps_rate path:
    //   if (hpsdr_model == HPSDRModel.HERMESLITE) // MI0BOT: HL2 can work at a high sample rate
    //   {
    //       Rate[0] = rx1_rate;
    //       Rate[1] = rx1_rate;
    //   }
    // Consumed by per-board codec applyPureSignalDdcConfig() and the
    // PureSignal coordinator.
    int psSampleRate {192000};

    bool hasDiversityReceiver;
    bool hasStepAttenuatorCal;
    bool hasPaProfile;

    bool hasBandwidthMonitor;
    bool hasIoBoardHl2;
    bool hasSidetoneGenerator;

    // Radio-side microphone input present.
    //
    // True for Hermes / Atlas / Orion / ANAN family + Saturn G2.
    // False for HermesLite 2 (no radio-side mic input).
    //
    // Source: NereusSDR-original; derived from Thetis Setup->Audio->Primary
    // per-board panel visibility:
    //   panelSaturnMicInput  (setup.designer.cs:8613 [v2.10.3.13])
    //   panelOrionMic        (setup.designer.cs:8661 [v2.10.3.13])
    //   panelOrionPTT        (setup.designer.cs:8709 [v2.10.3.13])
    //   panelOrionBias       (setup.designer.cs:8755 [v2.10.3.13])
    //   grpBoxMic            (setup.designer.cs:5154 [v2.10.3.13])
    // All hidden when CurrentModel == HermesLite2 per setup.cs:19834-20310
    // RadioModelChanged() per-model if-ladder [@501e3f5].
    //
    // Drives Setup -> Audio -> TX Input page Radio-Mic radio button visibility.
    // 3M-1b.
    bool hasMicJack {true};

    // Per-board mic gain slider range (Phase 3M-1b Task I.4).
    //
    // Porting from Thetis console.cs:19151-19171 [v2.10.3.13]:
    //   private int mic_gain_min = -40;
    //   private int mic_gain_max = 10;
    //
    // These are the runtime defaults for all known boards.  In Thetis they
    // are user-adjustable at runtime via udMicGainMin / udMicGainMax spinboxes
    // in the grpBoxMic settings group (setup.designer.cs:46808-46866
    // [v2.10.3.13]; spinbox widget range: Min -96..0, Max 1..70).
    //
    // NereusSDR uses the Thetis runtime defaults (-40/+10) for all known
    // boards, and the TransmitModel fallback (-50/+70) for Unknown.
    // The slider default is coded in TransmitModel::kMicGainDbMin/Max.
    //
    // Unknown board (kUnknown) defaults to -50/+70 (matches the global
    // TransmitModel::kMicGainDbMin/Max fallback).
    // All known boards default to -40/+10 per Thetis runtime defaults.
    //
    // TODO [3M-1b L.x]: persist user-overridden mic gain range per-MAC
    // in AppSettings (mirrors Thetis udMicGainMin/Max persistence in
    // setup.cs DB.SaveVarsDictionary).
    int micGainMinDb {-40};
    int micGainMaxDb {+10};

    // Phase 3P-F Task 2: accessory board enable rules.
    // Source: setup.cs:19834-20310 RadioModelChanged() per-model if-ladder [@501e3f5]
    //         setup.cs:6338 AddHPSDRPages() for tpPennyCtrl / tpAlexControl visibility.
    // Upstream inline attribution preserved verbatim:
    //   setup.cs:19855  if (initializing) return; // forceallevents will call this  // [2.10.1.0] MW0LGE renabled
    //   setup.cs:19904  case HPSDRModel.ANAN_G1: //N1GP G1 added
    //   setup.cs:20202  case HPSDRModel.ANAN_G2:                 // added G8NJJ
    //   setup.cs:20253  case HPSDRModel.ANAN_G2_1K:              // added G8NJJ
    //
    // hasApollo:    chkApolloPresent.Enabled = true only for HPSDRModel.HERMES (bare HPSDR kit).
    //               All ANAN family boards set chkApolloPresent.Enabled=false + Checked=false.
    // hasAlex:      chkAlexPresent shown for all HPSDR family; absent on HermesLite (no Alex port).
    // hasPennyLane: tpPennyCtrl inserted by AddHPSDRPages() for all HPSDR boards; absent on HL2
    //               (ocOutputCount=0, no Penny/OC ext-ctrl). For HERMES, tab is labeled "Hermes Ctrl";
    //               for all other boards it is labeled "OC Control".
    bool hasApollo{false};
    bool hasAlex{false};
    bool hasPennyLane{false};

    // From Thetis HasVolts (clsHardwareSpecific.cs:245-254 [v2.10.3.15]).
    // True for boards with on-board PA voltage telemetry sensor.
    // SKU set: ANAN7000D, ANAN8000D, ANVELINAPRO3, ANAN_G2, ANAN_G2_1K, REDPITAYA, ANAN_G2E. //N1GP G2E added
    bool hasPaVoltsTelemetry{false};

    // From Thetis HasAmps (clsHardwareSpecific.cs:255-264 [v2.10.3.15]).
    // True for boards with on-board PA current telemetry sensor.
    // SKU set: same as HasVolts. //N1GP G2E added
    bool hasPaAmpsTelemetry{false};

    // From Thetis setup.cs:19918 [v2.10.3.15] //N1GP G2E added —
    // chkAutoPACalibrate.Visible=false for G2E (auto-cal UI hidden).
    // Defaults true so existing boards retain auto-cal visibility.
    bool allowsAutoPaCalibrate{true};

    // From Thetis setup.cs:19920 [v2.10.3.15] //N1GP G2E added —
    // chkBypassANANPASettings.Visible=true for G2E ("Bypass ANAN PA Settings"
    // checkbox visible). Defaults false; G2E opts in.
    bool showsBypassPaSettingsUi{false};

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
    // Upstream inline attribution preserved verbatim (console.cs:40813):
    //   ... HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    std::span<const PreampItem> rx2PreampItemsForBoard(HPSDRHW hw) noexcept;

    // Returns the step attenuator maximum dB for a given board + ALEX presence.
    // Delegates to BoardCapabilities::attenuator.maxDb (single source of truth).
    // From Thetis setup.cs:15765 [v2.10.3.13]: 31 for most boards, 61 for
    // ALEX-equipped boards not in the exclusion list (OrionMKII/Saturn/HL2).
    // Exception: HL2 returns 63 (6-bit LNA range, mi0bot [@c26a8a4]).
    int stepAttMaxDb(HPSDRHW hw, bool alexPresent) noexcept;
}

} // namespace NereusSDR
