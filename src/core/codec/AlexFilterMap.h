// =================================================================
// src/core/codec/AlexFilterMap.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs:6830-6942 (setAlexHPF)
//   Project Files/Source/Console/console.cs:7168-7234 (setAlexLPF)
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
// =================================================================

#pragma once

#include <QtGlobal>
#include "../HpsdrModel.h"

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

// Frequency → Alex TRANSMIT low-pass select bits (bank 10 C4 in the P1 packet,
// or bytes 1428-1431 in the P2 CmdHighPriority packet).
//
// Deliberately board-independent, unlike the RX preselector above: Thetis has
// a single setAlexLPF with no HardwareSpecific branch inside it
// (console.cs:7177-7270 [v2.10.3.15]), and deskhpsdr says so explicitly at
// alex.h:110 [@f3d857c]: "The TX bits are just as for the generic case."
// The MkII boards changed the RX front end, not the TX low-pass bank.
//
// From Thetis console.cs:7168-7234 [@501e3f5]
quint8 computeLpf(double freqMhz);

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
// listening. `rx2PreampPresent` is BoardCapabilities::rx2PreampPresent: true
// means RX2 has its own front end and never shares this filter, so the first
// receiver decides alone.
double receiveLpfFrequencyMhz(double rx1Mhz, double rx2Mhz,
                              bool rx2Live, bool rx2PreampPresent) noexcept;

} // namespace NereusSDR::codec::alex
