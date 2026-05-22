// =================================================================
// src/core/codec/AlexFilterMap.cpp  (NereusSDR)
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

#include "AlexFilterMap.h"

namespace NereusSDR::codec::alex {

// From Thetis console.cs:6830-6942 [@501e3f5]
// Upstream tags preserved: //N1GP (from cited console.cs:6830) [v2.10.3.15]
// Upstream inline attribution preserved verbatim:
//   :6830  || (HardwareSpecific.Hardware == HPSDRHW.HermesIII)) //DK1HLM
// Decision rationale: spec §6.3.1
quint8 computeHpf(double freqMhz)
{
    if (freqMhz < 1.5)  { return 0x20; }    // bypass
    if (freqMhz < 6.5)  { return 0x10; }    // 1.5 MHz HPF
    if (freqMhz < 9.5)  { return 0x08; }    // 6.5 MHz HPF
    if (freqMhz < 13.0) { return 0x04; }    // 9.5 MHz HPF
    if (freqMhz < 20.0) { return 0x01; }    // 13 MHz HPF
    if (freqMhz < 50.0) { return 0x02; }    // 20 MHz HPF
    return 0x40;                             // 6m preamp
}

// From Thetis console.cs:7168-7234 [@501e3f5]
// Decision rationale: spec §6.3.1
quint8 computeLpf(double freqMhz)
{
    if (freqMhz < 2.0)   { return 0x08; }   // 160m LPF
    if (freqMhz < 4.0)   { return 0x04; }   // 80m LPF
    if (freqMhz < 7.3)   { return 0x02; }   // 60/40m LPF
    if (freqMhz < 14.35) { return 0x01; }   // 30/20m LPF
    if (freqMhz < 21.45) { return 0x40; }   // 17/15m LPF
    if (freqMhz < 29.7)  { return 0x20; }   // 12/10m LPF
    return 0x10;                             // 6m LPF
}

} // namespace NereusSDR::codec::alex
