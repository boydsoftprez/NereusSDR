// =================================================================
// src/core/codec/AlexFilterMap.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs:6830-6942 (setAlexHPF)
//   Project Files/Source/Console/console.cs:7177-7243 (setAlexLPF) [v2.10.3.15]
//   Project Files/Source/ChannelMaster/netInterface.c:680-725 (SetAlexLPFBits) [v2.10.3.15]
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Lifted from P2RadioConnection::computeAlexHpf/Lpf
//                (which had ported the same console.cs logic) into a
//                shared header so P1RadioConnection can call it too.
//                Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
//   2026-09-25: applyAlex1HpfSwitches: the Alex tab's high-pass
//                switches (HPF Bypass on TX, HPF Bypass on PureSignal
//                feedback, HPF Bypass, Disable 6m LNA on RX / TX) applied to the RX1 high-pass word as Thetis's
//                setAlexHPF / setBPF1ForOrionIISaturn apply them. Plan
//                Task 14 fix wave (R-R3-49). J.J. Boyd (KG4VCF), with
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: the Alex-1 Filters tab's low-pass rows
//                and 6m/ByPass on RX select the low-pass as Thetis's setAlexLPF
//                does, written to the Alex0 / Alex1 masks as netInterface.c
//                SetAlexLPFBits writes them (radioHardwareVersion 10). J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The low-pass edges' ranges (setup.designer.cs spinner
//                Minimum / Maximum) and the neighbour rule (setup.cs
//                udAlex*LPF*_ValueChanged) shared by the tab and the Core.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
// === Verbatim Thetis console.cs header (lines 1-50) ===
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
//
// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12
// --- From netInterface.c ---
/*
 * netinterface.c
 * Copyright (C) 2006,2007  Bill Tracey (bill@ejwt.com) (KD5TFD)
 * Copyright (C) 2010-2020 Doug Wigley (W5WC)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
// =================================================================

#pragma once

#include <QtGlobal>
#include "../HpsdrModel.h"

#include <array>
#include <optional>
#include <vector>

namespace NereusSDR::codec::alex {

// ── Two RX preselector designs, one set of relay bits ────────────────────────
//
// The Alex RX preselector comes in two physically different flavours that
// share the same relay bit positions, so the same byte engages a different
// filter depending on which board is on the other end of the cable:
//
//   * Legacy HIGH-PASS ladder: ANAN-100/200 class (Hermes, HermesII,
//     Angelia, Orion).  Each relay selects a high-pass corner.
//   * MkII BAND-PASS bank: Orion MkII / Saturn class (ANAN-7000DLE,
//     ANAN-8000DLE, Anvelina Pro 3, ANAN-G2, ANAN-G2-1K, ANAN-G2E).  Each
//     relay selects a band-pass filter with entirely different corners.
//
// deskhpsdr states it outright at alex.h:78 [@f3d857c]:
//   "NOTE: Anan-7000/8000 use band-pass filters here"
// and defines both banks over the same bits (alex.h:80-86 vs 116-122
// [@f3d857c]), in the same relay order.
//
// Thetis keeps the two apart by dispatching on board model in setAlex1HPF;
// see computeRxPreselector below.  Call THAT, not computeHpf, from anything
// that selects a receive filter for a real radio.

// ── The Alex tab's per-row high-pass edges and bypass switches ──────────────
//
// Thetis selects each receive high-pass (or band-pass) row by the Setup
// spinners' edges, `freq >= Start && freq <= End`, row by row in a fixed
// order, and each row has its own bypass check box that sends 0x20 in place
// of the row's filter:
//   From Thetis console.cs:6857-6871 [v2.10.3.15] (setAlexHPF, first row)
//     if ((decimal)freq >= SetupForm.udAlex1_5HPFStart.Value && // 1.5 MHz HPF
//          (decimal)freq <= SetupForm.udAlex1_5HPFEnd.Value)
//     {
//         if (alex1_5bphpf_bypass)
//         {
//             NetworkIO.SetAlexHPFBits(0x20); // Bypass HPF
//         ...
//         else
//         {
//             NetworkIO.SetAlexHPFBits(0x10);
// A frequency no row holds gets the bypass:
//   From Thetis console.cs:6946-6950 [v2.10.3.15]
//     else
//     {
//         NetworkIO.SetAlexHPFBits(0x20); // Bypass HPF
// setBPF1ForOrionIISaturn (console.cs:6953-7067) and setAlex2HPF
// (console.cs:7069-7175) run the same six rows over their own spinners and
// check boxes; setAlex2HPF also has its master switch (alex2_hpf_bypass,
// "ByPass/55 MHz BPF"). The rows are held here in Thetis's order: the
// 1.5 MHz, 6.5 MHz, 9.5 MHz, 13 MHz, 20 MHz and 6 m BPF/LNA rows, whose
// selections are 0x10, 0x08, 0x04, 0x01, 0x02 and 0x40. RadioModel reads
// the saved rows (hardware/<mac>/alex/hpf, alex/bpf1, alex2/hpf) and hands
// them to the connection; the defaults are the spinners' shipped values.
struct AlexHpfRow {
    double startMhz {0.0};
    double endMhz   {0.0};
    bool   bypass   {false};
    bool operator==(const AlexHpfRow&) const noexcept = default;
};

inline constexpr int kAlexHpfRowCount = 6;
using AlexHpfRows = std::array<AlexHpfRow, kAlexHpfRowCount>;

struct AlexHpfEdges {
    AlexHpfRows hpf;    // Alex-1 high-pass ladder (udAlex*HPF*, chkAlex*BPHPF)
    AlexHpfRows bpf1;   // Alex-1 band-pass bank   (ud*BPF1*, chkBPF1_*BP)
    AlexHpfRows alex2;  // Alex-2 high-pass bank   (udAlex2*HPF*, chkAlex2*BPHPF)
    bool alex2Bypass {false};  // chkAlex2HPFBypass -> alex2_hpf_bypass

    // Thetis's shipped spinner values, every bypass off.
    static AlexHpfEdges thetisDefaults() noexcept;
    bool operator==(const AlexHpfEdges&) const noexcept = default;
};

// The row selection: the first row whose edges hold `freqMhz` gives its
// filter (or 0x20 when its bypass is checked); no row gives 0x20. Compared
// in whole hertz, as Thetis compares the decimal spinner values.
quint8 selectAlexHpfRow(double freqMhz, const AlexHpfRows& rows) noexcept;

// Frequency → legacy Alex HIGH-PASS select bits (bank 10 C3 in the P1 packet,
// or bytes 1432-1435 in the P2 CmdHighPriority packet).
//
// ANAN-100/200 class only.  Saturn-class boards must go through
// computeBpf1 / computeRxPreselector instead.
//
// From Thetis console.cs:6830-6942 [@501e3f5]
// Upstream tags preserved: //N1GP (from cited console.cs:6830) [v2.10.3.15]
// Upstream inline attribution preserved verbatim:
//   :6830  || (HardwareSpecific.Hardware == HPSDRHW.HermesIII)) //DK1HLM
quint8 computeHpf(double freqMhz);

// Frequency → MkII BAND-PASS (BPF1) select bits, for Orion MkII / Saturn
// class boards.  Same wire encoding as computeHpf, different crossovers.
//
// From Thetis console.cs:6953-7067 setBPF1ForOrionIISaturn [v2.10.3.15]
quint8 computeBpf1(double freqMhz);

// True when `board` carries the MkII band-pass bank rather than the legacy
// high-pass ladder.
//
// From Thetis console.cs:6827-6837 setAlex1HPF [v2.10.3.15]
// Upstream inline attribution preserved verbatim:
//   :6829  || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
bool usesBpf1Preselector(NereusSDR::HPSDRHW board) noexcept;

// Frequency + board → RX preselector select bits.  This is the entry point
// every receive-filter call site should use; it picks the ladder the board
// actually has.
//
// From Thetis console.cs:6827-6837 setAlex1HPF [v2.10.3.15]
quint8 computeRxPreselector(double freqMhz, NereusSDR::HPSDRHW board);

// The same with the Alex tab's saved rows: the high-pass ladder's rows
// (setAlexHPF) or, on the band-pass boards, the BPF1 rows
// (setBPF1ForOrionIISaturn), each with its own per-row bypass.
quint8 computeRxPreselector(double freqMhz, NereusSDR::HPSDRHW board,
                            const AlexHpfEdges& edges) noexcept;

// The Alex-2 (second ADC) high-pass word with the saved rows and the
// Alex-2 master bypass:
//   From Thetis console.cs:7069-7079 [v2.10.3.15] (setAlex2HPF)
//     if (alex2_hpf_bypass)
//     {
//         NetworkIO.SetAlex2HPFBits(0x20); // Bypass HPF
//         ...
//         return;
//     }
// followed by the six rows over udAlex2*HPF* and chkAlex2*BPHPF.
quint8 computeAlex2Hpf(double freqMhz, const AlexHpfEdges& edges) noexcept;

// True for the radios on which Thetis sets the Alex-2 high-pass from RX2:
//   From Thetis console.cs:15435-15444 [v2.10.3.15] (UpdateRX2DDSFreq)
//   (Upstream inline attribution nearby, preserved verbatim, console.cs:15449:
//                case HPSDRModel.ANAN_G2E: //N1GP G2E added)
//     if (HardwareSpecific.Model == HPSDRModel.ORIONMKII ||
//         HardwareSpecific.Model == HPSDRModel.ANAN7000D ||
//         HardwareSpecific.Model == HPSDRModel.ANAN8000D ||
//         HardwareSpecific.Model == HPSDRModel.ANAN_G2 ||
//         HardwareSpecific.Model == HPSDRModel.ANAN_G2_1K ||
//         HardwareSpecific.Model == HPSDRModel.ANVELINAPRO3 ||
//         HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
//     {
//         setAlex2HPF(rx2_dds_freq_mhz);
bool usesAlex2Hpf(NereusSDR::HPSDRModel model) noexcept;

// Frequency → Alex TRANSMIT low-pass select bits (bank 10 C4 in the P1 packet,
// or bytes 1428-1431 in the P2 CmdHighPriority packet).
//
// Deliberately board-independent, unlike the RX preselector above: Thetis has
// a single setAlexLPF with no HardwareSpecific branch inside it
// (console.cs:7177-7270 [v2.10.3.15]), and deskhpsdr says so explicitly at
// alex.h:110 [@f3d857c]: "The TX bits are just as for the generic case."
// The MkII boards changed the RX front end, not the TX low-pass bank.
//
// The low-pass selection over Thetis's shipped row defaults (see
// AlexLpfEdges below); the connections hand the saved rows to selectAlexLpf.
// From Thetis console.cs:7177-7243 [v2.10.3.15]
quint8 computeLpf(double freqMhz);

// ---------------------------------------------------------------------------
// The Alex-1 Filters tab's low-pass rows, as Thetis's setAlexLPF reads them.
// ---------------------------------------------------------------------------
//
// From Thetis console.cs:7177-7243 [v2.10.3.15] (setAlexLPF)
//   if (!_mox && lpf_bypass)
//   {
//       NetworkIO.SetAlexLPFBits(0x10, false, _mox); // 6m LPF
//       SetupForm.rad6LPFled.Checked = true;
//       return;
//   }
//   if (alexpresent && !initializing)
//   {
//       if ((decimal)freq >= SetupForm.udAlex20mLPFStart.Value && // 30/20m LPF
//                 (decimal)freq <= SetupForm.udAlex20mLPFEnd.Value)
//           NetworkIO.SetAlexLPFBits(0x01, freqIsTX, _mox);
//       ... 40m 0x02, 80m 0x04, 160m 0x08, 6m 0x10, 10m 0x20, 15m 0x40 ...
//       else
//           NetworkIO.SetAlexLPFBits(0x10, freqIsTX, _mox); // 6m LPF
//   }
// The rows are held here in the tab's order (160 m first); the selection
// tests them in Thetis's order (20 m first), so an overlap goes to the
// row Thetis tests first. The defaults are the spinners' shipped values,
// setup.designer.cs:24492-24881 [v2.10.3.15].
struct AlexLpfRow {
    double startMhz {0.0};
    double endMhz   {0.0};
    bool operator==(const AlexLpfRow&) const noexcept = default;
};

inline constexpr int kAlexLpfRowCount = 7;
using AlexLpfRows = std::array<AlexLpfRow, kAlexLpfRowCount>;

// Settings-key slugs, in row order: hardware/<mac>/alex/lpf/<slug>/{start,end}.
inline constexpr std::array<const char*, kAlexLpfRowCount> kAlexLpfRowSlugs = {
    "160m", "80m", "40m", "20m", "15m", "10m", "6m",
};

struct AlexLpfEdges {
    AlexLpfRows rows;  // 160m, 80m, 40m, 20m, 15m, 10m, 6m (udAlex*LPFStart/End)

    // Thetis's shipped spinner values.
    static AlexLpfEdges thetisDefaults() noexcept;
    bool operator==(const AlexLpfEdges&) const noexcept = default;
};

// setAlexLPF's row selection: the first row, in Thetis's order, whose edges
// hold `freqMhz` (inclusive, compared in whole hertz as Thetis compares the
// decimal spinner values); no row gives the 6 m low-pass, 0x10.
quint8 selectAlexLpf(double freqMhz, const AlexLpfEdges& edges) noexcept;

// The two low-pass masks the radio carries: Alex0 (the receive word, and the
// only one Protocol 1 sends) and Alex1 (the transmit word, Protocol 2).
struct AlexLpfMasks {
    quint8 alex0 {0};
    quint8 alex1 {0};
    bool operator==(const AlexLpfMasks&) const noexcept = default;
};

// From Thetis ChannelMaster/netInterface.c:680-725 [v2.10.3.15]
//   // if not MOX, write to alex1 if a TX setting else write to alex0
//   if (isMox || isTX)        // true if Alex1 should be written
//   if (isMox || !isTX)        // true if Alex0 should be written
// Returns true when either mask changed.
bool setAlexLpfBits(AlexLpfMasks& masks, quint8 bits, bool isTx, bool isMox) noexcept;

// setAlexLPF: the 6m/ByPass on RX branch (unkeyed only, tested before the
// Alex-present gate, as Thetis tests it), then the row selection written with
// setAlexLpfBits. `alexPresent` false and no bypass: nothing is written.
// Thetis's `initializing` guard has no equivalent: a connection only selects
// once it is running. Returns true when either mask changed.
bool setAlexLpf(AlexLpfMasks& masks, double freqMhz, bool freqIsTx, bool mox,
                bool lpfBypass, bool alexPresent, const AlexLpfEdges& edges) noexcept;

// Each low-pass edge's allowed range, the spinners' Minimum / Maximum.
// From Thetis setup.designer.cs [v2.10.3.15]:
//   udAlex160mLPFStart 0 (:24872) .. 1.999999 (:24867)
//   udAlex160mLPFEnd   1.5 (:24842) .. 2.5 (:24837)
//   udAlex80mLPFStart  1.8 (:24812) .. 2.999999 (:24807)
//   udAlex80mLPFEnd    3 (:24782) .. 5 (:24777)
//   udAlex40mLPFStart  4 (:24752) .. 6.5 (:24747)
//   udAlex40mLPFEnd    6.500001 (:24722) .. 8.0 (:24717)
//   udAlex20mLPFStart  7 (:24602) .. 12 (:24597)
//   udAlex20mLPFEnd    12.000001 (:24662) .. 16.5 (:24657)
//   udAlex15mLPFStart  15.5 (:24632) .. 21 (:24627)
//   udAlex15mLPFEnd    23.000001 (:24692) .. 25.0 (:24687)
//   udAlex10mLPFStart  24 (:24513) .. 30 (:24508)
//   udAlex10mLPFEnd    30.000001 (:24483) .. 35.6 (:24478)
//   udAlex6mLPFStart   34 (:24572) .. 50 (:24567)
//   udAlex6mLPFEnd     50.000001 (:24543) .. 61.44 (:24538)
// A 160 m End of 30 MHz would put a 25 MHz transmission through the 160 m
// low-pass, so no path may store or apply an edge outside these.
struct AlexLpfEdgeLimits {
    double startMin;
    double startMax;
    double endMin;
    double endMax;
};
inline constexpr std::array<AlexLpfEdgeLimits, kAlexLpfRowCount> kAlexLpfEdgeLimits = {{
    {0.0,       1.999999,  1.5,       2.5},    // 160m
    {1.8,       2.999999,  3.0,       5.0},    // 80m
    {4.0,       6.5,       6.500001,  8.0},    // 40m
    {7.0,       12.0,      12.000001, 16.5},   // 20m
    {15.5,      21.0,      23.000001, 25.0},   // 15m
    {24.0,      30.0,      30.000001, 35.6},   // 10m
    {34.0,      50.0,      50.000001, 61.44},  // 6m
}};

// True when `mhz` is finite and inside the row edge's range (compared in
// whole micro-MHz, the spinners' six decimals).
bool alexLpfEdgeAllowed(int row, bool isEnd, double mhz) noexcept;

// `mhz` held to the row edge's range; a non-finite value gives `fallback`
// (itself held to the range).
double clampAlexLpfEdge(int row, bool isEnd, double mhz, double fallback) noexcept;

// One edge value an edit moved.
struct AlexLpfEdgeMove {
    int row {0};
    bool isEnd {false};
    double mhz {0.0};
    bool operator==(const AlexLpfEdgeMove&) const noexcept = default;
};

// The Filters tab's neighbour rule for one edited edge: the neighbouring edge
// it pushes, if any, rounded to six decimals and held to that edge's range.
// From Thetis setup.cs:15888-15994 [v2.10.3.15]
//   udAlex160mLPFStart: if (Start >= End + 0.000001) End = Start + 0.000001;
//   udAlex160mLPFEnd:   if (End <= Start) Start = End - 0.000001;
//                       else if (End >= 80mStart) 80mStart = End + 0.000001;
//   udAlex<N>LPFStart (80..6m):  if (Start <= prevEnd) prevEnd = Start - 0.000001;
//   udAlex<N>LPFEnd (80..10m):   if (End >= nextStart) nextStart = End + 0.000001;
//   (udAlex6mLPFEnd has no handler.)
std::optional<AlexLpfEdgeMove> alexLpfNeighbourMove(const AlexLpfRows& rows,
                                                    int row, bool isEnd);

// Sets one edge in `rows` (held to its range) and runs the neighbour rule to
// rest, as Thetis's ValueChanged handlers cascade: each moved edge fires its
// own handler. Returns the neighbours moved, in order (not the edited edge).
// The Core runs this on every accepted low-pass write, so every window and
// the phone see the moved neighbours; the desktop tab uses
// alexLpfNeighbourMove one step at a time through its spin boxes.
std::vector<AlexLpfEdgeMove> applyAlexLpfEdgeEdit(AlexLpfRows& rows, int row,
                                                  bool isEnd, double mhz);

// Whether "6m/ByPass on RX" applies on `model`. Thetis hides and unchecks
// chkLPFBypass on five boards (NereusSDR shows it disabled, with the reason,
// and treats it as off):
// From Thetis setup.cs:6190-6205 [v2.10.3.15]
//   if (HardwareSpecific.Model == HPSDRModel.ANAN8000D ||
//       HardwareSpecific.Model == HPSDRModel.ANAN7000D ||
//       HardwareSpecific.Model == HPSDRModel.ANAN_G2 ||
//       HardwareSpecific.Model == HPSDRModel.ANAN_G2_1K ||
//       HardwareSpecific.Model == HPSDRModel.ANVELINAPRO3 ||
//       HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
//   {
//       if (HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
//       {
//           chkLPFBypass.Visible = true;
//       }
//       else
//       {
//           chkLPFBypass.Checked = false;
//           chkLPFBypass.Visible = false;
//       }
bool lpfBypassAvailable(NereusSDR::HPSDRModel model) noexcept;

// Which RECEIVE frequency selects the low-pass when more than one receiver is
// listening. Returns the frequency to hand to computeLpf, in MHz.
//
// The low-pass the radio is receiving through is a different selection from
// the one it transmits through, and it is not simply "whichever receiver moved
// last". A low-pass passes everything BELOW its corner, so with two receivers
// sharing the Alex chain the HIGHER frequency has to pick the filter: choosing
// the lower receiver's would attenuate the higher one into silence.
//
// From Thetis console.cs:15487-15498 UpdateAlexTXFilter [v2.10.3.15]
//   private void UpdateAlexTXFilter()
//   {
//       if (!_mox)
//       {
//           if (!_rx2_preamp_present && chkRX2.Checked)
//           {
//               if (rx1_dds_freq_mhz > rx2_dds_freq_mhz) setAlexLPF(rx1_dds_freq_mhz, false);
//               else setAlexLPF(rx2_dds_freq_mhz, false);
//           }
//           else setAlexLPF(rx1_dds_freq_mhz, false);
//       }
//   }
//
// Its mirror image takes the LOWER frequency for the high-pass, so the pair
// together spans both receivers:
//   From Thetis console.cs:15500-15510 UpdateAlexRXFilter [v2.10.3.15]
//     if (rx1_dds_freq_mhz < rx2_dds_freq_mhz) setAlex1HPF(rx1_dds_freq_mhz);
//     else setAlex1HPF(rx2_dds_freq_mhz);
//
// `rx2Live` is Thetis's chkRX2.Checked, meaning a second receiver is actually
// listening. With more than two live receivers on one front end the callers
// pass the highest of the others as RX2 (plan Task 14 fix wave, M6), so the
// low-pass passes every one of them. `rx2PreampPresent` is BoardCapabilities::rx2PreampPresent: true
// means RX2 has its own front end and never shares this filter, so the first
// receiver decides alone.
double receiveLpfFrequencyMhz(double rx1Mhz, double rx2Mhz,
                              bool rx2Live, bool rx2PreampPresent) noexcept;

// The Alex tab's high-pass switches, as Thetis holds them on the console.
// Defaults are Thetis's console fields (console.cs:18719, 18741, 18753,
// 18764 and 18793 [v2.10.3.15]); what the Setup tab saves is handed in by RadioModel.
struct Alex1HpfSwitches {
    bool hpfBypassOnTx {false};   // chkDisableHPFonTX  -> disable_hpf_on_tx
    bool hpfBypassOnPs {false};   // chkDisableHPFonPSb -> disable_hpf_on_ps
    bool hpfBypass     {false};   // chkAlexHPFBypass   -> alex_hpf_bypass
    bool disable6mLnaOnRx {false};  // chkDisable6mLNAonRX -> disable_6m_lna_on_rx
    bool disable6mLnaOnTx {true};   // chkDisable6mLNAonTX -> disable_6m_lna_on_tx
};

// The RX1 high-pass word (Alex0, SetAlexHPFBits) after those switches.
// `selected` is the band's selection (computeRxPreselector, or the per-chain
// decision); `keyed` is _mox; `pureSignalRunning` is PureSignalEnabled.
// The caller gates on an Alex filter board being present (alexpresent).
//
// From Thetis console.cs:6839-6848 setAlexHPF [v2.10.3.15]
//   if (_mox && disable_hpf_on_tx)
//   { NetworkIO.SetAlexHPFBits(0x20); ... return; }
// From Thetis console.cs:6953-6962 setBPF1ForOrionIISaturn [v2.10.3.15]
//   if (_mox && (disable_hpf_on_tx || (disable_hpf_on_ps && PureSignalEnabled)))
//   { NetworkIO.SetAlexHPFBits(0x20); ... return; }
// From Thetis console.cs:6850-6855 setAlexHPF [v2.10.3.15]
//   if (alex_hpf_bypass)
//   { NetworkIO.SetAlexHPFBits(0x20); // Bypass HPF ... return; }
// (setBPF1ForOrionIISaturn has the same at 6965-6970), keyed or not.
// On 6 m, where the selection is the BPF/LNA (0x40):
// From Thetis console.cs:6935 setAlexHPF [v2.10.3.15]
//   if (alex6bphpf_bypass || disable_6m_lna_on_rx || (_mox && disable_6m_lna_on_tx))
//   { NetworkIO.SetAlexHPFBits(0x20); // Bypass HPF
// (setBPF1ForOrionIISaturn: console.cs:7050).

// The PureSignal arm exists only in the band-pass function, which Thetis
// runs for Orion MkII, Saturn and HermesC10 alone (usesBpf1Preselector,
// console.cs:6827-6837 setAlex1HPF [v2.10.3.15]).
quint8 applyAlex1HpfSwitches(quint8 selected, NereusSDR::HPSDRHW board,
                             bool keyed, bool pureSignalRunning,
                             const Alex1HpfSwitches& switches) noexcept;

} // namespace NereusSDR::codec::alex
