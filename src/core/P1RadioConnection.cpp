// =================================================================
// src/core/P1RadioConnection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/NetworkIO.cs (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/cmaster.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/bandwidth_monitor.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/bandwidth_monitor.h, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/IoBoardHl2.cs (mi0bot/OpenHPSDR-Thetis fork), original licence from upstream included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*
 * networkprot1.c
 * Copyright (C) 2020 Doug Wigley (W5WC)
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

//
// Upstream source 'Project Files/Source/Console/HPSDR/NetworkIO.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

/*  cmaster.cs

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

/*  bandwidth_monitor.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2025 Richard Samphire, MW0LGE

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


/*  bandwidth_monitor.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2025 Richard Samphire, MW0LGE

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


/*
*
* Copyright (C) 2025 Reid Campbell, MI0BOT, mi0bot@trom.uk 
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
//
// (mi0bot HL2 fork's IOBoard logic; the C# class wraps closed-source
// I2C register code in ChannelMaster.dll — only the public API surface
// has been ported into NereusSDR's P1 path.)

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include "P1RadioConnection.h"
#include "LogCategories.h"
#include "codec/P1CodecStandard.h"
#include "codec/P1CodecAnvelinaPro3.h"
#include "codec/P1CodecRedPitaya.h"
#include "codec/P1CodecHl2.h"
#include "codec/AlexFilterMap.h"

#include <cstdlib>
#include <cstring>    // memset
#include <vector>
#include <QtEndian>
#include <QNetworkDatagram>
#include <QThread>
#include <QVariant>
#include <QCoreApplication>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// composeEp2Frame
//
// Builds the 1032-byte Metis ep2 UDP payload that is sent TO the radio.
// Layout (source: networkproto1.c:216-236 MetisWriteFrame, and :597-864
// WriteMainLoop which populates the two 512-byte USB subframes):
//
//   [0..3]    Magic header: EF FE 01 02
//   [4..7]    Sequence number, big-endian uint32
//   [8..519]  USB subframe 0 (512 bytes):
//               [8..10]   sync 7F 7F 7F  (networkproto1.c:600-602)
//               [11..15]  C0..C4 command/control bytes
//               [16..519] TX I/Q + mic samples (zeros for RX-only)
//   [520..1031] USB subframe 1 (512 bytes):
//               [520..522] sync 7F 7F 7F  (networkproto1.c:881-883)
//               [523..527] C0..C4 for second subframe (same bank for now)
//               [528..1031] TX I/Q + mic samples (zeros for RX-only)
// ---------------------------------------------------------------------------
void P1RadioConnection::composeEp2Frame(quint8 out[1032], quint32 seq,
                                         int /*ccAddress*/,
                                         int sampleRate, bool mox,
                                         quint64 rx1FreqHz,
                                         int activeRxCount) noexcept
{
    // Source: networkproto1.c:223-230 — MetisWriteFrame() header + sequence
    out[0] = 0xEF;
    out[1] = 0xFE;
    out[2] = 0x01;
    out[3] = 0x02;  // endpoint 2

    // Sequence number big-endian
    out[4] = static_cast<quint8>((seq >> 24) & 0xFF);
    out[5] = static_cast<quint8>((seq >> 16) & 0xFF);
    out[6] = static_cast<quint8>((seq >>  8) & 0xFF);
    out[7] = static_cast<quint8>( seq        & 0xFF);

    // Source: networkproto1.c:597-602 — WriteMainLoop() USB subframe 0 sync
    out[8]  = 0x7F;
    out[9]  = 0x7F;
    out[10] = 0x7F;

    // C0..C4 for subframe 0 -- compose bank 0 (general settings)
    quint8 cc0[5] = {};
    composeCcBank0(cc0, sampleRate, mox, activeRxCount);
    out[11] = cc0[0];
    out[12] = cc0[1];
    out[13] = cc0[2];
    out[14] = cc0[3];
    out[15] = cc0[4];

    // Bytes 16..519: TX I/Q + mic data — zeros (RX-only; TX producer added in TX phase)

    // Source: networkproto1.c:878-883 — WriteMainLoop_HL2() USB subframe 1 sync
    // (same layout in WriteMainLoop at :880-883)
    out[520] = 0x7F;
    out[521] = 0x7F;
    out[522] = 0x7F;

    // C0..C4 for subframe 1 — RX1 frequency bank (address 0x02, Thetis case 2).
    // Without this, the radio streams ADC data at LO=0 → huge DC spike with no
    // visible signals. Partial round-robin: bank 0 on subframe 0, RX1 freq on
    // subframe 1. Remaining banks (TX freq, Alex, atten, OC, RX2+) come with
    // the full round-robin in the outstanding P1 tasks.
    quint8 cc1[5] = {};
    composeCcBankRxFreq(cc1, 0 /* rxIndex */, rx1FreqHz);
    out[523] = cc1[0];
    out[524] = cc1[1];
    out[525] = cc1[2];
    out[526] = cc1[3];
    out[527] = cc1[4];

    // Bytes 528..1031: TX I/Q + mic data — zeros (RX-only)
}

// ---------------------------------------------------------------------------
// composeCcBank0
//
// Source: networkproto1.c:619-641 — WriteMainLoop() case 0 (general settings)
//   C0 = XmitBit (MOX, bit 0); address bits 7..1 = 0x00 (no C0 |= address)
//   C1 = SampleRateIn2Bits & 3  (48k=0, 96k=1, 192k=2, 384k=3)
//   C2 = OC output mask (bits 7..1) | EER bit (bit 0) — zero for stub
//   C3 = BPF/atten/preamp flags — zero for stub
//   C4 = antenna, duplex, NDDCs — zero for stub
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBank0(quint8 out[5], int sampleRate, bool mox,
                                        int activeRxCount) noexcept
{
    // Source: networkproto1.c:615 -- C0 = (unsigned char)XmitBit
    out[0] = mox ? 0x01 : 0x00;

    // Source: networkproto1.c:620 -- C1 = (SampleRateIn2Bits & 3)
    // 48000->0, 96000->1, 192000->2, 384000->3
    quint8 srBits = 0;
    if      (sampleRate >= 384000) { srBits = 3; }
    else if (sampleRate >= 192000) { srBits = 2; }
    else if (sampleRate >= 96000)  { srBits = 1; }
    else                            { srBits = 0; }
    out[1] = srBits & 0x03;

    // C2, C3: OC outputs / filter bits -- zero for Task 7 scope.
    out[2] = 0;
    out[3] = 0;

    // C4: number of DDCs to run, encoded as (nddc - 1) << 3
    // Source: networkproto1.c:470. Thetis sends 0x08 for nddc=2 even on
    // single-RX setups (diversity pre-allocation). We send the actual
    // count so 1-RX configurations write 0x00. The Hermes firmware
    // accepts both.
    int nddc = (activeRxCount < 1) ? 1 : (activeRxCount > 7 ? 7 : activeRxCount);
    out[4] = static_cast<quint8>((nddc - 1) << 3);
}

// ---------------------------------------------------------------------------
// composeCcBankRxFreq
//
// Source: networkproto1.c:484-494 — case 2 (RX1/DDC0 frequency)
//   rxIndex 0 → C0 |= 4 (address 0x02)
//   rxIndex 1 → C0 |= 6 (address 0x03)  [case 3]
//   rxIndex 2 → C0 |= 8 (address 0x04)  [case 5]
//   C1..C4 = (prn->rx[id].frequency >> {24,16,8,0}) & 0xff — raw Hz, big-endian
//
// P1 (Protocol 1 / ENC "USB") sends **raw Hz** on the wire — NOT a phase
// word. Thetis NetworkIO.cs:215-223 VFOfreq() selects the encoding based on
// CurrentRadioProtocol:
//
//     if (CurrentRadioProtocol == RadioProtocol.USB)   // USB == P1
//         SetVFOfreq(id, f_freq, tx);                   // raw Hz
//     else SetVFOfreq(id, Freq2PhaseWord(f_freq), tx);  // phase word (P2)
//
// The RadioProtocol.USB enum value maps to Protocol 1 (see cmaster.cs:520
// "case RadioProtocol.USB: //p1"). The native side (networkproto1.c:491-494)
// then splats prn->rx[0].frequency directly into C1..C4 with no conversion.
//
// Previous revisions of this helper pre-converted to a phase word and the
// on-wire bytes were interpreted by Hermes firmware as raw Hz far above
// Nyquist, which produced an aliased waterfall and a non-tracking VFO on
// ANAN-10E (pcap4 from alpha tester, 2026-04-15).
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankRxFreq(quint8 out[5], int rxIndex, quint64 freqHz) noexcept
{
    // Address assignments from networkproto1.c (C0 OR bits; the table stores
    // the already-shifted value so callers just assign out[0] = addrBits):
    //   rxIndex 0 → case 2 (:485):  C0 |= 0x04  (RX1 / DDC0)
    //   rxIndex 1 → case 3 (:498):  C0 |= 0x06  (RX2 / DDC1)
    //   rxIndex 2 → case 5 (:526):  C0 |= 0x08  (RX3 / DDC2)
    //   rxIndex 3 → case 6 (:539):  C0 |= 0x0A  (RX4 / DDC3)
    //   rxIndex 4 → case 7 (:549):  C0 |= 0x0C  (RX5 / DDC4)
    //   rxIndex 5 → case 8      :  C0 |= 0x0E  (RX6 / DDC5)
    //   rxIndex 6 → case 9 (:569):  C0 |= 0x10  (RX7 / DDC6)
    //
    // History: prior revision had {4,6,8,0x0C,0x0E,0x10,0x12}, dropping 0x0A
    // entirely and aliasing rxIndex 6 onto bank 10's 0x12 slot. Fixed per
    // pcap analysis of RedPitaya (#38).
    static const quint8 kRxC0Address[] = { 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10 };
    quint8 addrBits = (rxIndex >= 0 && rxIndex < 7) ? kRxC0Address[rxIndex] : 4;
    out[0] = addrBits;  // MOX=0; address is already left-shifted in the table

    // Raw Hz, big-endian — see header comment for Thetis source citations.
    // Thetis stores freq as int32 in prn->rx[0].frequency; clamp to 32 bits
    // here so anything above ~4.29 GHz (impossible for HF) truncates cleanly.
    const quint32 hz = static_cast<quint32>(freqHz);
    out[1] = static_cast<quint8>((hz >> 24) & 0xFF);
    out[2] = static_cast<quint8>((hz >> 16) & 0xFF);
    out[3] = static_cast<quint8>((hz >>  8) & 0xFF);
    out[4] = static_cast<quint8>( hz        & 0xFF);
}

// ---------------------------------------------------------------------------
// composeCcBankTxFreq
//
// Source: networkproto1.c:476-482 — case 1 (TX VFO frequency)
//   C0 |= 2 → address 0x01
//   C1..C4 = (prn->tx[0].frequency >> {24,16,8,0}) & 0xff — raw Hz, big-endian
//
// Same raw-Hz encoding as composeCcBankRxFreq — see that function's header
// comment for the NetworkIO.cs:215-223 branching rule.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankTxFreq(quint8 out[5], quint64 freqHz) noexcept
{
    // Source: networkproto1.c:477 — C0 |= 2  (case 1 = TX VFO)
    out[0] = 0x02;  // address 0x01, MOX=0

    const quint32 hz = static_cast<quint32>(freqHz);
    out[1] = static_cast<quint8>((hz >> 24) & 0xFF);
    out[2] = static_cast<quint8>((hz >> 16) & 0xFF);
    out[3] = static_cast<quint8>((hz >>  8) & 0xFF);
    out[4] = static_cast<quint8>( hz        & 0xFF);
}

// ---------------------------------------------------------------------------
// composeCcBankAtten
//
// Source: networkproto1.c:762-771 — case 11 (Preamp control / step attenuator)
//   C0 |= 0x14 → address 0x0A (networkproto1.c:763)
//   C4 = (adc[0].rx_step_attn & 0b00011111) | 0b00100000
//        Low 5 bits = dB value; bit 5 = "enable step attenuator" flag
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankAtten(quint8 out[5], int dB) noexcept
{
    // Source: networkproto1.c:763 — C0 |= 0x14 → address 0x0A
    out[0] = 0x14;  // MOX=0; address bits = 0x14

    // C1..C3: preamp/mic/dig-out flags — zero for Task 7 scope
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;

    // Source: networkproto1.c:770 — C4 = (rx_step_attn & 0b00011111) | 0b00100000
    out[4] = static_cast<quint8>((dB & 0x1F) | 0x20);
}

// ---------------------------------------------------------------------------
// composeCcBankAlexRx
//
// Source: networkproto1.c:826-835 — case 16 (BPF2 / ALEX RX filter mask)
//   C0 |= 0x24 → address 0x12
//   C1 = BPF HPF filter bits (low 7 bits of alexRxMask)
//   C2 = xvtr_enable + puresignal flags (bits 0 + 6 of upper byte)
//   C3, C4 = 0
//
// TODO(3I-T7): The "ALEX RX antenna routing" (RX1 IN, RX2 IN, XVTR) is
// encoded in bank 0 C3 bits 5..6 (networkproto1.c:625-630), not a separate
// bank. The alexRxMask here is treated as the BPF2 HPF filter bitmap
// (case 16). Full ALEX RX antenna routing requires bank 0 state.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankAlexRx(quint8 out[5], quint32 alexRxMask) noexcept
{
    // Source: networkproto1.c:827 — C0 |= 0x24
    out[0] = 0x24;
    out[1] = static_cast<quint8>(alexRxMask & 0x7F);  // HPF filter bits
    out[2] = static_cast<quint8>((alexRxMask >> 8) & 0x41); // xvtr + puresignal flags
    out[3] = 0;
    out[4] = 0;
}

// ---------------------------------------------------------------------------
// composeCcBankAlexTx
//
// Source: networkproto1.c:747-760 — case 10 (TX drive level / ALEX TX LPF)
//   C0 |= 0x12 → address 0x09
//   C3 = HPF filter bits (bits from alexTxMask low byte)
//   C4 = LPF filter bits (bits from alexTxMask high byte)
//   C1 = drive level (0 for stub), C2 = mic/apollo flags (0 for stub)
//
// TODO(3I-T7): Drive level and mic/apollo flags are separate state fields not
// carried in alexTxMask. Full encoding requires those fields; this stub
// encodes only the filter mask portion as mapped from Task 7 scope.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankAlexTx(quint8 out[5], quint32 alexTxMask) noexcept
{
    // Source: networkproto1.c:748 — C0 |= 0x12
    out[0] = 0x12;
    out[1] = 0;  // drive level — TODO(3I-T7): wire from state
    out[2] = 0;  // mic/apollo flags — TODO(3I-T7): wire from state
    out[3] = static_cast<quint8>(alexTxMask & 0xFF);        // HPF bits
    out[4] = static_cast<quint8>((alexTxMask >> 8) & 0x7F); // LPF bits
}

// ---------------------------------------------------------------------------
// composeCcBankOcOutputs
//
// Source: networkproto1.c:621 — case 0: C2 = (cw.eer & 1) | ((oc_output << 1) & 0xFE)
//   OC output bits live in bank 0 C2, bits 7..1. Not a separate bank.
//   For Task 7 scope this helper encodes only the OC mask into C2 of a
//   bank-0-shaped output buffer (C0 address = 0x00, C1 = default 48k).
//
// TODO(3I-T7): In Thetis the OC mask is always sent as part of bank 0 C2
// (networkproto1.c:621). This standalone helper is for the test interface;
// in the real send path it will be merged into composeCcBank0 state.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBankOcOutputs(quint8 out[5], quint8 ocMask) noexcept
{
    // Source: networkproto1.c:621 — C2 = (cw.eer & 1) | ((oc_output << 1) & 0xFE)
    out[0] = 0;  // address 0x00, MOX=0
    out[1] = 0;
    out[2] = static_cast<quint8>((ocMask << 1) & 0xFE);  // OC bits in C2 bits 7..1
    out[3] = 0;
    out[4] = 0;
}



P1RadioConnection::P1RadioConnection(QObject* parent)
    : RadioConnection(parent)
{
}

P1RadioConnection::~P1RadioConnection()
{
    if (m_running) {
        disconnect();
    }
}

// ---------------------------------------------------------------------------
// init
//
// Creates the UDP socket and watchdog timer on the worker thread.
// Must be called after moveToThread().
// Source: P2RadioConnection::init() pattern — socket + timer created here,
// not in constructor, to ensure thread affinity is correct.
// ---------------------------------------------------------------------------
void P1RadioConnection::init()
{
    // Source: networkproto1.c:203 equivalent — bind to any available port
    m_socket = new QUdpSocket(this);

    if (!m_socket->bind(QHostAddress::Any, 0)) {
        qCWarning(lcConnection) << "P1: Failed to bind UDP socket";
        return;
    }

    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption,
                              QVariant(0xfa000));
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                              QVariant(0xfa000));

    connect(m_socket, &QUdpSocket::readyRead, this, &P1RadioConnection::onReadyRead);

    // Watchdog timer — polls every kWatchdogTickMs ms; started in connectToRadio.
    // Source: NereusSDR design doc §3.6 — silence detection + reconnect state machine.
    m_watchdogTimer = new QTimer(this);
    m_watchdogTimer->setInterval(kWatchdogTickMs);
    connect(m_watchdogTimer, &QTimer::timeout, this, &P1RadioConnection::onWatchdogTick);

    // EP2 pacer — dedicated high-resolution timer that feeds the radio's
    // 48 kHz audio DAC. Kept separate from the watchdog so silence detection
    // cadence (25 ms) and EP2 cadence (2 ms) can evolve independently.
    m_ep2PacerTimer = new QTimer(this);
    m_ep2PacerTimer->setInterval(kEp2PacerIntervalMs);
    m_ep2PacerTimer->setTimerType(Qt::PreciseTimer);
    connect(m_ep2PacerTimer, &QTimer::timeout, this, &P1RadioConnection::onEp2PacerTick);

    // Reconnect timer — single-shot; fires kReconnectIntervalMs after watchdog trips.
    // Source: NereusSDR design doc §3.6 — 5-second reconnect interval, max 3 retries.
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &P1RadioConnection::onReconnectTimeout);

    qCDebug(lcConnection) << "P1: init() socket port:" << m_socket->localPort();
}

// ---------------------------------------------------------------------------
// connectToRadio
//
// Binds the socket, sends metis-start, transitions to Connected.
// Source: networkproto1.c:33 SendStartToMetis — sends EF FE 04 01 then
// waits for the first ep6 frame. For Phase 3I Task 9 we transition to
// Connected immediately after sending start, matching P2 behavior.
// ---------------------------------------------------------------------------
void P1RadioConnection::connectToRadio(const RadioInfo& info)
{
    if (m_running) {
        disconnect();
    }

    m_radioInfo = info;
    // Use HardwareProfile for caps (Phase 3I-RP).
    // Fall back to board-byte lookup if setHardwareProfile() was never called
    // (e.g. direct construction in tests without RadioModel).
    m_caps = m_hardwareProfile.caps
             ? m_hardwareProfile.caps
             : &BoardCapsTable::forBoard(info.boardType);

    // Initialize per-ADC state from profile
    for (int i = 0; i < 3; ++i) {
        m_dither[i] = true;
        m_random[i] = true;
        m_rxPreamp[i] = false;
        m_stepAttn[i] = 0;
    }
    m_txStepAttn = 0;
    m_paEnabled = m_caps->hasPaProfile;
    m_duplex = true;
    m_reconnectedLogged = false;

    m_intentionalDisconnect = false;
    m_epSendSeq = 0;
    m_epRecvSeqExpected = 0;
    m_ccRoundRobinIdx = 0;

    // Thetis hardcodes nddc=4 for plain Hermes/ANAN10/ANAN100 and nddc=2 for
    // ANAN10E/100B (HermesII) in console.cs:8378-8454. No current-source P1
    // path uses nddc=1 for Hermes-class boards. Captured Thetis traffic with
    // the tester's radio shows nddc=2 on the wire (sub0 C4 = 0x08 bits).
    // Default m_activeRxCount to 2 for Hermes-class P1 until we have a
    // model-override setting. parseEp6Frame uses this same value to select
    // the 14-byte nddc=2 slot layout, so sent and received must agree.
    if (m_radioInfo.protocol == ProtocolVersion::Protocol1) {
        m_activeRxCount = 2;
    }

    // Reset reconnect state — fresh connection resets the retry counter.
    // Source: NereusSDR design doc §3.6 — explicit user reconnect clears attempts.
    m_reconnectAttempts = 0;
    m_lastEp6At = QDateTime();
    m_firstEp6Logged = false;
    m_parseFailLogged = false;
    m_firstEmitLogged = false;

    // Apply per-board quirks (attenuator clamp, OC zero-force, etc.) now that
    // m_caps is set. Source: specHPSDR.cs per-HPSDRHW branches.
    applyBoardQuirks();

    // HL2 I/O board init — probe the I/O board via I2C after quirks are set.
    // Source: mi0bot IoBoardHl2.cs:129-145 IOBoard.readRequest(); Task 12.
    if (m_caps->hasIoBoardHl2) {
        hl2SendIoBoardInit();
    }

    // Firmware version refusal/stale-warning paths removed 2026-04-13.
    // Background: Thetis enforces only one firmware refusal in its entire
    // connect path (NetworkIO.cs:136-143, HermesII < 103 only) and has no
    // "stale firmware" warning concept at all. The previous per-board
    // BoardCapabilities thresholds were unattested guesses (see TODO(3I-T2))
    // and were locking out legitimate radios — most visibly, plain Hermes
    // running stock v15 firmware which Thetis accepts without complaint.
    // The RadioConnectionError::FirmwareTooOld / FirmwareStale enum entries
    // remain defined but are no longer emitted from this path.

    setState(ConnectionState::Connecting);
    qCDebug(lcConnection) << "P1: Connecting to" << m_caps->displayName
                          << "at" << info.address.toString() << "port" << info.port
                          << "from local port" << (m_socket ? m_socket->localPort() : 0);

    // Prime the radio with alternating TX-VFO and RX1-VFO ep2 frames BEFORE
    // metis-start so the DDC initializes with the correct phase words,
    // sample rate, and nddc. Without priming, the Hermes firmware streams
    // ADC-idle data (I=DC offset, Q=0) -- confirmed in the tester's pcap
    // where every sample had Q=0 for 20 seconds.
    //
    // Port of Thetis networkproto1.c:35-68 SendStartToMetis + :281
    // MetisReadThreadMainLoop. Thetis sends up to 16 primed ep2 frames
    // (5x ForceCandCFrame(1) inside SendStartToMetis + ForceCandCFrame(3)
    // at the top of MetisReadThreadMainLoop). We collapse that into two
    // ForceCandCFrame(3) equivalents: 3 TX + 3 RX1 before metis-start, and
    // 3 TX + 3 RX1 after.
    m_running = true;
    sendPrimingBurst(3);

    // Send metis-start to begin ep6 stream.
    // Source: networkproto1.c:33-68 SendStartToMetis -- cmd byte 0x01 = IQ only
    sendMetisStart(false);

    // Second priming burst after start, matching Thetis's ForceCandCFrame(3)
    // at MetisReadThreadMainLoop:281.
    sendPrimingBurst(3);

    m_watchdogTimer->start();
    m_ep2PacerClock.restart();
    m_ep2PacketsSent = 0;
    m_ep2PacerTimer->start();
    setState(ConnectionState::Connected);

    qCDebug(lcConnection) << "P1: Connected (metis-start sent)";
}

// ---------------------------------------------------------------------------
// disconnect
//
// Sends metis-stop and closes the socket.
// Source: networkproto1.c:72-110 SendStopToMetis — EF FE 04 00
// ---------------------------------------------------------------------------
void P1RadioConnection::disconnect()
{
    m_intentionalDisconnect = true;

    if (m_watchdogTimer) {
        m_watchdogTimer->stop();
    }
    if (m_ep2PacerTimer) {
        m_ep2PacerTimer->stop();
    }
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }

    // Clear reconnect state on explicit disconnect.
    // Source: NereusSDR design doc §3.6 — user reconnect resets the cycle.
    m_reconnectAttempts = 0;
    m_lastEp6At = QDateTime();
    m_reconnectedLogged = false;

    if (m_running && m_socket && !m_radioInfo.address.isNull()) {
        m_running = false;
        sendMetisStop();
        qCDebug(lcConnection) << "P1: metis-stop sent";
    }

    m_running = false;

    // Don't close the socket — leave it open for re-use in Task 10.
    // P2 pattern: socket stays bound across reconnect cycles.

    setState(ConnectionState::Disconnected);
    qCDebug(lcConnection) << "P1: Disconnected";
}

void P1RadioConnection::setReceiverFrequency(int receiverIndex, quint64 frequencyHz)
{
    if (receiverIndex < 0 || receiverIndex >= 7) { return; }
    m_rxFreqHz[receiverIndex] = frequencyHz;
    // RX0 drives the Alex HPF bank — recompute on every change.
    // Source: console.cs:6830-6942 [@501e3f5]
    if (receiverIndex == 0 && m_caps && m_caps->hasAlexFilters) {
        m_alexHpfBits = codec::alex::computeHpf(double(frequencyHz) / 1e6);
    }
}

void P1RadioConnection::setTxFrequency(quint64 frequencyHz)
{
    m_txFreqHz = frequencyHz;
    // TX freq drives Alex LPF — recompute on every change.
    // Source: console.cs:7168-7234 [@501e3f5]
    if (m_caps && m_caps->hasAlexFilters) {
        m_alexLpfBits = codec::alex::computeLpf(double(frequencyHz) / 1e6);
    }
}
void P1RadioConnection::setActiveReceiverCount(int count)    { m_activeRxCount = count; }
void P1RadioConnection::setSampleRate(int sampleRate)        { m_sampleRate = sampleRate; }
void P1RadioConnection::setAttenuator(int dB)
{
    // Source: specHPSDR.cs per-HPSDRHW branches + BoardCapabilities registry.
    // Clamp to board-reported range so UI callers can't exceed hardware limits.
    if (m_caps && m_caps->attenuator.present) {
        if (dB > m_caps->attenuator.maxDb) { dB = m_caps->attenuator.maxDb; }
        if (dB < m_caps->attenuator.minDb) { dB = m_caps->attenuator.minDb; }
    } else if (m_caps && !m_caps->attenuator.present) {
        dB = 0;
    }
    m_stepAttn[0] = dB;
}
void P1RadioConnection::setPreamp(bool enabled)              { m_rxPreamp[0] = enabled; }
void P1RadioConnection::setTxDrive(int /*level*/)            { /* stub — Task 7 */ }
void P1RadioConnection::setMox(bool enabled)                 { m_mox = enabled; }
void P1RadioConnection::setAntenna(int antennaIndex)         { m_antennaIdx = antennaIndex; }

// ---------------------------------------------------------------------------
// applyBoardQuirks
//
// Reads BoardCapabilities (m_caps) and enforces runtime constraints.
// Must be called after m_caps is set in connectToRadio() and from
// setBoardForTest() in unit tests.
//
// Source: specHPSDR.cs per-HPSDRHW branches + BoardCapabilities registry.
// Thetis clamps the step-attenuator value in SetupForm per board type and
// enforces the limits again in NetworkIO.cs before sending C&C frames.
//
// (HL2 IoBoardHl2 TLV init + bandwidth monitor come in Task 12)
// ---------------------------------------------------------------------------
void P1RadioConnection::applyBoardQuirks()
{
    if (!m_caps) { return; }

    // Clamp attenuator to board range.
    // Source: specHPSDR.cs — per-HPSDRHW min/max dB ranges enforced at setup.
    if (m_caps->attenuator.present) {
        if (m_stepAttn[0] > m_caps->attenuator.maxDb) { m_stepAttn[0] = m_caps->attenuator.maxDb; }
        if (m_stepAttn[0] < m_caps->attenuator.minDb) { m_stepAttn[0] = m_caps->attenuator.minDb; }
    } else {
        m_stepAttn[0] = 0;
    }

    selectCodec();
}

// ---------------------------------------------------------------------------
// selectCodec
//
// Builds m_codec from m_hardwareProfile.model. Called from applyBoardQuirks().
// Phase 3P-A Task 12.
// ---------------------------------------------------------------------------
void P1RadioConnection::selectCodec()
{
    m_codec.reset();
    m_useLegacyCodec = (qEnvironmentVariableIntValue("NEREUS_USE_LEGACY_P1_CODEC") == 1);
    if (m_useLegacyCodec) {
        qCInfo(lcConnection) << "P1: NEREUS_USE_LEGACY_P1_CODEC=1 — using pre-refactor compose path";
        return;
    }
    if (!m_caps) {
        qCWarning(lcConnection) << "P1: no caps; codec selection deferred";
        return;
    }
    // Note: the per-board codec is keyed off the **logical** HPSDRModel
    // (which distinguishes HermesLite from Hermes-family even though they
    // share the same physical HPSDRHW::HermesLite wire dialect), not the
    // physical HPSDRHW. Use m_hardwareProfile.model for codec selection.
    // RedPitaya and AnvelinaPro3 are HPSDRModel values that map to physical
    // OrionMKII at the wire layer but need different bank-12/bank-17
    // encoding — that's why they get their own codec subclasses.
    using HPM = HPSDRModel;
    switch (m_hardwareProfile.model) {
        case HPM::HERMESLITE:   m_codec = std::make_unique<P1CodecHl2>();          break;
        case HPM::ANVELINAPRO3: m_codec = std::make_unique<P1CodecAnvelinaPro3>(); break;
        case HPM::REDPITAYA:    m_codec = std::make_unique<P1CodecRedPitaya>();    break;
        default:                m_codec = std::make_unique<P1CodecStandard>();     break;
    }
    qCInfo(lcConnection) << "P1: selected codec for HPSDRModel" << int(m_hardwareProfile.model);
}

// ---------------------------------------------------------------------------
// buildCodecContext
//
// Snapshot all live state into a CodecContext for the codec call.
// Phase 3P-A Task 12.
// ---------------------------------------------------------------------------
CodecContext P1RadioConnection::buildCodecContext() const
{
    CodecContext ctx;
    ctx.mox            = m_mox;
    ctx.sampleRateCode = (m_sampleRate >= 384000) ? 3
                       : (m_sampleRate >= 192000) ? 2
                       : (m_sampleRate >=  96000) ? 1 : 0;
    ctx.activeRxCount  = m_activeRxCount;
    ctx.txDrive        = m_txDrive;
    ctx.paEnabled      = m_paEnabled;
    ctx.duplex         = m_duplex;
    ctx.diversity      = m_diversity;
    ctx.antennaIdx     = m_antennaIdx;
    ctx.ocByte         = m_ocOutput;
    ctx.adcCtrl        = m_adcCtrl;
    ctx.alexHpfBits    = m_alexHpfBits;
    ctx.alexLpfBits    = m_alexLpfBits;
    ctx.txFreqHz       = m_txFreqHz;
    for (int i = 0; i < 7; ++i) { ctx.rxFreqHz[i]   = m_rxFreqHz[i]; }
    for (int i = 0; i < 3; ++i) { ctx.rxStepAttn[i] = m_stepAttn[i]; }
    // m_txStepAttn is a single int; broadcast to all 3 ADCs (HL2 codec
    // only uses index 0). Standard codec uses a separate path for
    // bank 4 TX drive — txStepAttn array isn't read there.
    for (int i = 0; i < 3; ++i) { ctx.txStepAttn[i] = m_txStepAttn; }
    for (int i = 0; i < 3; ++i) { ctx.rxPreamp[i]   = m_rxPreamp[i]; }
    for (int i = 0; i < 3; ++i) { ctx.dither[i]     = m_dither[i]; }
    for (int i = 0; i < 3; ++i) { ctx.random[i]     = m_random[i]; }
    // HL2-only fields default to 0 / false; populated by Phase E.
    return ctx;
}

// ---------------------------------------------------------------------------
// onReadyRead
//
// Drains incoming datagrams. For each 1032-byte ep6 frame, calls the static
// parseEp6Frame helper and emits iqDataReceived for each receiver.
// Source: networkproto1.c:319-415 MetisReadThreadMainLoop
//
// Handles both Connected and Connecting states so that reconnect attempts
// (which send metis-start from Connecting) can transition to Connected on
// the first good ep6 frame (design doc §3.6 step 5).
// ---------------------------------------------------------------------------
void P1RadioConnection::onReadyRead()
{
    if (!m_socket) { return; }

    const ConnectionState cs = state();
    // Only process ep6 data when Connected or Connecting (reconnect attempt).
    if (cs != ConnectionState::Connected && cs != ConnectionState::Connecting) {
        // Drain the socket anyway to avoid buffering.
        while (m_socket->hasPendingDatagrams()) {
            m_socket->receiveDatagram();
        }
        return;
    }

    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram dg = m_socket->receiveDatagram();
        const QByteArray& data = dg.data();

        if (data.isEmpty()) { continue; }

        // ep6 frames are exactly 1032 bytes
        // Source: networkproto1.c:319 — MetisReadThreadMainLoop receives 1032-byte frames
        if (data.size() == 1032) {
            // Update watchdog timestamp on every good ep6 arrival.
            // Source: NereusSDR design doc §3.6 — successful data resets the retry counter.
            m_lastEp6At = QDateTime::currentDateTimeUtc();

            // Diagnostic: log the first EP6 frame of this session so
            // remote debugging can tell "connected but no data" apart
            // from "connected and streaming".
            if (!m_firstEp6Logged) {
                m_firstEp6Logged = true;
                qCInfo(lcConnection) << "P1: first ep6 frame received from"
                                     << dg.senderAddress().toString()
                                     << "(1032 bytes)";
            }

            // If we were in a reconnect attempt, the first good frame means recovery.
            // Stop the reconnect timer and transition to Connected.
            if (cs == ConnectionState::Connecting) {
                m_reconnectAttempts = 0;
                if (m_reconnectTimer) { m_reconnectTimer->stop(); }
                if (!m_watchdogTimer->isActive()) { m_watchdogTimer->start(); }
                setState(ConnectionState::Connected);
                if (!m_reconnectedLogged) {
                    qCDebug(lcConnection) << "P1: Reconnected — ep6 stream restored";
                    m_reconnectedLogged = true;
                }
            }

            parseEp6Frame(data);
        }
    }
}

// ---------------------------------------------------------------------------
// onWatchdogTick
//
// Fires every kWatchdogTickMs ms while connected or reconnecting.
// Silence-detection only — EP2 pacing lives on m_ep2PacerTimer
// (onEp2PacerTick) to match Thetis' 381 pps audio-clock cadence.
//
// Detects ep6 silence: if no frame has arrived for kWatchdogSilenceMs,
// transitions to Error and arms the reconnect timer.
// Applies to both Connected (initial silence detection) and Connecting
// (reconnect attempt timed out — the retry got no response).
// Source: NereusSDR design doc §3.6.
// ---------------------------------------------------------------------------
void P1RadioConnection::onWatchdogTick()
{
    if (!m_running || !m_socket || m_radioInfo.address.isNull()) { return; }

    const ConnectionState cs = state();

    // Watchdog is silence-detection only. EP2 pacing lives on m_ep2PacerTimer
    // (onEp2PacerTick) to match Thetis' 381 pps audio-clock cadence. See
    // kEp2PacerIntervalMs.

    // HL2 bandwidth monitor — check for LAN PHY throttle on every watchdog tick.
    // Source: mi0bot bandwidth_monitor.{c,h} — NereusSDR sequence-gap adaptation.
    if (m_caps && m_caps->hasBandwidthMonitor) {
        hl2CheckBandwidthMonitor();
    }

    // Silence detection applies to both Connected and Connecting states.
    // In Connecting: the reconnect attempt sent metis-start but got no ep6 response.
    if (cs != ConnectionState::Connected && cs != ConnectionState::Connecting) { return; }

    // If we haven't received any ep6 frame yet, don't trip the watchdog.
    if (!m_lastEp6At.isValid()) { return; }

    const qint64 silenceMs = m_lastEp6At.msecsTo(QDateTime::currentDateTimeUtc());
    if (silenceMs > kWatchdogSilenceMs) {
        qCWarning(lcConnection) << "P1: Watchdog — ep6 silent for" << silenceMs
                                << "ms (state=" << static_cast<int>(cs)
                                << "); transitioning to Error and scheduling reconnect";
        m_watchdogTimer->stop();
        if (m_ep2PacerTimer) {
            m_ep2PacerTimer->stop();
        }
        m_reconnectedLogged = false;
        setState(ConnectionState::Error);
        emit errorOccurred(RadioConnectionError::NoDataTimeout,
                           QStringLiteral("Radio stopped responding"));

        // Arm the reconnect timer for the next retry attempt (or first if from Connected).
        // Source: NereusSDR design doc §3.6 — 5-second reconnect interval.
        m_reconnectTimer->start(kReconnectIntervalMs);
    }
}

// ---------------------------------------------------------------------------
// onEp2PacerTick
//
// Fires every kEp2PacerIntervalMs ms while connected. Uses a catch-up loop
// against m_ep2PacerClock so the aggregate send rate tracks 380.95 pps
// (48 kHz audio / 126 samples per EP2 frame) even when Qt's PreciseTimer
// under-delivers on Windows (10-15 ms resolution). Each tick emits a small
// burst to make up the deficit, capped at kEp2MaxBurstPerTick for safety.
//
// This matches Thetis sendProtocol1Samples (networkproto1.c:700-747), which
// is driven by a 48 kHz audio-semaphore clock — not by timer precision.
//
// Unlike the watchdog's former EP2 send, this path intentionally does NOT
// consult m_hl2Throttled. Thetis never pauses sends, and pausing egress is
// a weak remedy for an HL2 ingress PHY stall. The throttle flag and
// bandwidth monitor logic remain in place for future use.
// ---------------------------------------------------------------------------
void P1RadioConnection::onEp2PacerTick()
{
    if (!m_running || !m_socket || m_radioInfo.address.isNull()) { return; }

    const ConnectionState cs = state();
    if (cs != ConnectionState::Connected && cs != ConnectionState::Connecting) { return; }

    // Catch-up loop: compute how many packets should have been sent by now at
    // the 380.95 pps target, and emit the missing ones. On Windows the Qt
    // PreciseTimer only delivers ~10 ms ticks (multimedia timer floor), so
    // each tick typically emits a burst of 3-5 packets. This matches Thetis'
    // sendProtocol1Samples pacing (networkproto1.c:700-747), which is driven
    // by a 48 kHz audio-subsystem semaphore rather than timer precision.
    if (!m_ep2PacerClock.isValid()) {
        m_ep2PacerClock.start();
        m_ep2PacketsSent = 0;
    }

    const qint64 elapsedUs = m_ep2PacerClock.nsecsElapsed() / 1000;
    const qint64 due       = elapsedUs / kEp2PacketIntervalUs;
    int burst = 0;
    while (m_ep2PacketsSent < due && burst < kEp2MaxBurstPerTick) {
        sendCommandFrame();
        ++m_ep2PacketsSent;
        ++burst;
    }
}

// ---------------------------------------------------------------------------
// onReconnectTimeout
//
// Called when the single-shot reconnect timer fires.
// Implements bounded retries: up to kMaxReconnectAttempts, then stays in Error.
// Source: NereusSDR design doc §3.6.
// ---------------------------------------------------------------------------
void P1RadioConnection::onReconnectTimeout()
{
    // Guard: don't retry after an intentional disconnect.
    if (m_intentionalDisconnect) { return; }

    if (m_reconnectAttempts >= kMaxReconnectAttempts) {
        qCWarning(lcConnection) << "P1: Reconnect — bounded retries exhausted after"
                                << kMaxReconnectAttempts << "attempts; staying in Error";
        // Stay in Error — user must explicitly call connectToRadio() to reset.
        return;
    }

    ++m_reconnectAttempts;
    qCDebug(lcConnection) << "P1: Reconnect attempt" << m_reconnectAttempts
                          << "of" << kMaxReconnectAttempts;

    // Transition to Connecting for this retry attempt.
    setState(ConnectionState::Connecting);

    // Send stop then prime then start so the radio re-arms its ep6 sender
    // with the current RX1 frequency latched. Without the primed C&C burst,
    // the radio comes back up in ADC-idle state (I=DC, Q=0).
    // Source: networkproto1.c:49-110 SendStopToMetis / SendStartToMetis plus
    // the ForceCandCFrame bracket pattern from MetisReadThreadMainLoop:281.
    sendMetisStop();
    sendPrimingBurst(3);
    sendMetisStart(false);
    sendPrimingBurst(3);

    // Re-arm the watchdog so onReadyRead can complete the transition to Connected.
    if (!m_watchdogTimer->isActive()) {
        m_watchdogTimer->start();
    }
    if (m_ep2PacerTimer && !m_ep2PacerTimer->isActive()) {
        m_ep2PacerClock.restart();
        m_ep2PacketsSent = 0;
        m_ep2PacerTimer->start();
    }

    // Re-arm the reconnect timer so if this attempt also fails, the next retry
    // is scheduled automatically (the watchdog will stop itself and re-arm this
    // timer again when it detects silence).
    // We do NOT re-arm here unconditionally — the watchdog arms it when needed.
    // But we schedule a fallback in case no ep6 data arrives within the window
    // (i.e., watchdog trips again → re-arms reconnect timer).
    // No extra start() needed; see onWatchdogTick for the arming path.
}

// ---------------------------------------------------------------------------
// sendMetisStart
//
// Source: networkproto1.c:33-68 SendStartToMetis
//   outpacket.packetbuf[0] = 0xef  (line 43)
//   outpacket.packetbuf[1] = 0xfe  (line 44)
//   outpacket.packetbuf[2] = 0x04  (line 49)
//   outpacket.packetbuf[3] = 0x01  (line 50) — start IQ stream
//   Packet is 64 bytes, padded with zeros.
//   iqAndMic=true → cmd 0x02 (IQ+mic), false → cmd 0x01 (IQ only)
// ---------------------------------------------------------------------------
void P1RadioConnection::sendMetisStart(bool iqAndMic)
{
    if (!m_socket) { return; }

    // Source: networkproto1.c:33-68 SendStartToMetis — 64-byte packet
    QByteArray pkt(64, '\0');
    pkt[0] = static_cast<char>(0xEF);
    pkt[1] = static_cast<char>(0xFE);
    pkt[2] = static_cast<char>(0x04);
    pkt[3] = iqAndMic ? static_cast<char>(0x02) : static_cast<char>(0x01);

    m_socket->writeDatagram(pkt, m_radioInfo.address, m_radioInfo.port);
}

// ---------------------------------------------------------------------------
// sendMetisStop
//
// Source: networkproto1.c:72-110 SendStopToMetis
//   outpacket.packetbuf[2] = 0x04  (line 84)
//   outpacket.packetbuf[3] = 0x00  (stop command)
//   Packet is 64 bytes, padded with zeros.
// ---------------------------------------------------------------------------
void P1RadioConnection::sendMetisStop()
{
    if (!m_socket) { return; }

    // Source: networkproto1.c:72-110 SendStopToMetis — 64-byte packet
    QByteArray pkt(64, '\0');
    pkt[0] = static_cast<char>(0xEF);
    pkt[1] = static_cast<char>(0xFE);
    pkt[2] = static_cast<char>(0x04);
    pkt[3] = static_cast<char>(0x00);

    m_socket->writeDatagram(pkt, m_radioInfo.address, m_radioInfo.port);
}

// ---------------------------------------------------------------------------
// sendCommandFrame
//
// Builds a 1032-byte ep2 frame with two C&C subframes drawn from the full
// 17-bank round-robin sequence. Each call advances m_ccRoundRobinIdx by 2.
// Source: networkproto1.c:216-236 MetisWriteFrame + :597-884 WriteMainLoop
// ---------------------------------------------------------------------------
void P1RadioConnection::sendCommandFrame()
{
    if (!m_socket) { return; }

    const int maxBank = (m_hardwareProfile.model == HPSDRModel::ANVELINAPRO3) ? 17 : 16;

    quint8 frame[1032];
    memset(frame, 0, sizeof(frame));

    // EP2 header (networkproto1.c:223-230)
    frame[0] = 0xEF; frame[1] = 0xFE; frame[2] = 0x01; frame[3] = 0x02;
    const quint32 seq = m_epSendSeq++;
    frame[4] = static_cast<quint8>((seq >> 24) & 0xFF);
    frame[5] = static_cast<quint8>((seq >> 16) & 0xFF);
    frame[6] = static_cast<quint8>((seq >>  8) & 0xFF);
    frame[7] = static_cast<quint8>( seq        & 0xFF);

    // Subframe 0: current bank
    frame[8] = 0x7F; frame[9] = 0x7F; frame[10] = 0x7F;
    quint8 cc0[5] = {};
    composeCcForBank(m_ccRoundRobinIdx, cc0);
    frame[11] = cc0[0]; frame[12] = cc0[1]; frame[13] = cc0[2];
    frame[14] = cc0[3]; frame[15] = cc0[4];

    m_ccRoundRobinIdx++;
    if (m_ccRoundRobinIdx > maxBank) { m_ccRoundRobinIdx = 0; }

    // Subframe 1: next bank
    frame[520] = 0x7F; frame[521] = 0x7F; frame[522] = 0x7F;
    quint8 cc1[5] = {};
    composeCcForBank(m_ccRoundRobinIdx, cc1);
    frame[523] = cc1[0]; frame[524] = cc1[1]; frame[525] = cc1[2];
    frame[526] = cc1[3]; frame[527] = cc1[4];

    m_ccRoundRobinIdx++;
    if (m_ccRoundRobinIdx > maxBank) { m_ccRoundRobinIdx = 0; }

    QByteArray pkt(reinterpret_cast<const char*>(frame), 1032);
    m_socket->writeDatagram(pkt, m_radioInfo.address, m_radioInfo.port);
}

// ---------------------------------------------------------------------------
// sendPrimingBurst
//
// Port of Thetis networkproto1.c:134-139 ForceCandCFrame(count):
//   ForceCandCFrames(count, 2, prn->tx[0].frequency);  // TX bank
//   Sleep(10);
//   ForceCandCFrames(count, 4, prn->rx[0].frequency);  // RX1 bank
//   Sleep(10);
//
// Thetis calls this with count=1 inside each retry of SendStartToMetis and
// with count=3 once at the top of MetisReadThreadMainLoop. We call it with
// count=3 before metis-start and count=3 after, matching the MetisReadThread
// invocation. The Sleep(10) gap between TX and RX bursts is preserved because
// some Hermes firmware revisions need a small idle gap between bank changes.
// ---------------------------------------------------------------------------
void P1RadioConnection::sendPrimingBurst(int countPerBank)
{
    for (int i = 0; i < countPerBank; ++i) {
        sendCommandFrame();
    }
    QThread::msleep(10);
    for (int i = 0; i < countPerBank; ++i) {
        sendCommandFrame();
    }
    QThread::msleep(10);
}

// ---------------------------------------------------------------------------
// parseEp6Frame (instance method)
//
// Calls the static parseEp6Frame helper and emits iqDataReceived for each
// receiver's interleaved I/Q samples.
// Source: networkproto1.c:319-415 MetisReadThreadMainLoop
// ---------------------------------------------------------------------------
void P1RadioConnection::parseEp6Frame(const QByteArray& pkt)
{
    if (pkt.size() != 1032) { return; }

    std::vector<std::vector<float>> perRx;
    const auto* frame = reinterpret_cast<const quint8*>(pkt.constData());

    if (!P1RadioConnection::parseEp6Frame(frame, m_activeRxCount, perRx)) {
        if (!m_parseFailLogged) {
            m_parseFailLogged = true;
            qCWarning(lcConnection) << "P1: parseEp6Frame rejected frame;"
                                    << "activeRxCount=" << m_activeRxCount
                                    << "magic=" << QString::asprintf("%02X %02X %02X %02X",
                                                                     frame[0], frame[1], frame[2], frame[3])
                                    << "sync0=" << QString::asprintf("%02X %02X %02X",
                                                                     frame[8], frame[9], frame[10])
                                    << "sync1=" << QString::asprintf("%02X %02X %02X",
                                                                     frame[520], frame[521], frame[522]);
        }
        return;
    }

    //[2.10.3.13]MW0LGE adc_overload accumulated (or'd) across EP6 frames, cleared only by reader [Thetis networkproto1.c:335]
    // From Thetis networkproto1.c — ADC overflow in C&C status bytes.
    // C0[0] bit 0 = LT2208 overflow (ADC0).
    const quint8 c0_sub0 = frame[11];
    const quint8 c0_sub1 = frame[523];
    if ((c0_sub0 & 0x01) || (c0_sub1 & 0x01)) {
        emit adcOverflow(0);
    }

    // Emit iqDataReceived for each receiver
    // Contract: hwReceiverIndex (0-based), interleaved float I/Q pairs, [-1, 1]
    // Source: RadioConnection.h:82 iqDataReceived signal
    for (int r = 0; r < static_cast<int>(perRx.size()); ++r) {
        if (!perRx[static_cast<size_t>(r)].empty()) {
            QVector<float> samples(perRx[static_cast<size_t>(r)].begin(),
                                   perRx[static_cast<size_t>(r)].end());
            if (!m_firstEmitLogged) {
                m_firstEmitLogged = true;
                qCInfo(lcConnection) << "P1: first iqDataReceived emit; rx=" << r
                                     << "samples=" << samples.size();
            }
            emit iqDataReceived(r, samples);
        }
    }
}

// ---------------------------------------------------------------------------
// composeCcForBankLegacy — pre-refactor compose path (kept for rollback hatch)
//
// Identical to the original composeCcForBank body. Preserved for one release
// cycle until Task 16 drops the dual-call diagnostic. Do not edit.
// Ported from Thetis networkproto1.c WriteMainLoop cases 0-17.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcForBankLegacy(int bankIdx, quint8 out[5]) const
{
    memset(out, 0, 5);
    const quint8 C0base = m_mox ? 0x01 : 0x00;

    switch (bankIdx) {
    case 0: // General settings (networkproto1.c:450-471)
        composeCcBank0Full(out);
        return;

    case 1: // TX VFO (networkproto1.c:476-482)
        composeCcBankTxFreq(out, m_txFreqHz);
        return;

    case 2: // RX1 VFO DDC0 (networkproto1.c:484-494)
        composeCcBankRxFreq(out, 0, m_rxFreqHz[0]);
        return;

    case 3: { // RX2 VFO DDC1 (networkproto1.c:497-511)
        // RX2 frequency uses the same phase-word encoding as RX1.
        composeCcBankRxFreq(out, 1, m_rxFreqHz[1]);
        return;
    }

    case 4: // ADC assignments + TX ATT (networkproto1.c:517-523)
        out[0] = C0base | 0x1C;
        out[1] = static_cast<quint8>(m_adcCtrl & 0xFF);
        out[2] = static_cast<quint8>((m_adcCtrl >> 8) & 0x3F);
        out[3] = static_cast<quint8>(m_txStepAttn & 0x1F);
        out[4] = 0;
        return;

    case 5: case 6: case 7: case 8: case 9: {
        // RX3-RX7 VFOs (networkproto1.c:525-575)
        // Unused DDCs get TX freq as a safe default.
        int rxIdx = bankIdx - 3; // bank 5 → rxIdx 2, bank 9 → rxIdx 6
        composeCcBankRxFreq(out, rxIdx, m_txFreqHz);
        return;
    }

    case 10: // TX drive, mic, Alex HPF/LPF, PA (networkproto1.c:578-591)
        out[0] = C0base | 0x12;
        out[1] = static_cast<quint8>(m_txDrive & 0xFF);
        out[2] = 0x40; // line_in=0, mic_boost=0 defaults
        out[3] = m_alexHpfBits | (m_paEnabled ? 0x80 : 0);
        out[4] = m_alexLpfBits;
        return;

    case 11: // Preamp control (networkproto1.c:593-601)
        out[0] = C0base | 0x14;
        out[1] = static_cast<quint8>(
                   (m_rxPreamp[0] ? 0x01 : 0)
                 | (m_rxPreamp[1] ? 0x02 : 0)
                 | (m_rxPreamp[2] ? 0x04 : 0)
                 | (m_rxPreamp[0] ? 0x08 : 0)); // bit3 = rx0 again (Thetis quirk)
        out[2] = 0; // line_in_gain + puresignal
        out[3] = 0; // user digital outputs
        out[4] = static_cast<quint8>((m_stepAttn[0] & 0x1F) | 0x20); // ADC0 step ATT + enable
        return;

    case 12: { // Step ATT ADC1/2, CW keyer (networkproto1.c:604-628)
        out[0] = C0base | 0x16;
        // RedPitaya-specific: don't force 31dB on ADC1 during TX
        // From networkproto1.c:606-616 (DH1KLM fix)
        if (m_mox && m_hardwareProfile.model != HPSDRModel::REDPITAYA) {
            out[1] = 0x1F;
        } else if (m_hardwareProfile.model == HPSDRModel::REDPITAYA) {
            out[1] = static_cast<quint8>(m_stepAttn[1] & 0x1F);
        } else {
            out[1] = static_cast<quint8>(m_stepAttn[1] & 0xFF);
        }
        out[1] |= 0x20; // enable bit
        out[2] = static_cast<quint8>((m_stepAttn[2] & 0x1F) | 0x20);
        out[3] = 0; // CW keyer defaults
        out[4] = 0;
        return;
    }

    case 13: // CW enable (networkproto1.c:633-639)
        out[0] = C0base | 0x1E;
        out[1] = 0; out[2] = 0; out[3] = 0; out[4] = 0;
        return;

    case 14: // CW hang/sidetone (networkproto1.c:641-646)
        out[0] = C0base | 0x20;
        out[1] = 0; out[2] = 0; out[3] = 0; out[4] = 0;
        return;

    case 15: // EER PWM (networkproto1.c:649-654)
        out[0] = C0base | 0x22;
        out[1] = 0; out[2] = 0; out[3] = 0; out[4] = 0;
        return;

    case 16: // BPF2 (networkproto1.c:657-665)
        out[0] = C0base | 0x24;
        out[1] = 0; out[2] = 0; out[3] = 0; out[4] = 0;
        return;

    case 17: // AnvelinaPro3 extra OC (networkproto1.c:668-673)
        out[0] = C0base | 0x26;
        out[1] = 0; out[2] = 0; out[3] = 0; out[4] = 0;
        return;

    default:
        return;
    }
}

// ---------------------------------------------------------------------------
// composeCcForBank — dispatcher for all 18 C&C banks (0-17)
//
// Phase 3P-A Task 12: delegates to per-board IP1Codec subclass chosen at
// applyBoardQuirks() time. Falls back to legacy path when:
//   - NEREUS_USE_LEGACY_P1_CODEC=1 env var is set, or
//   - m_codec is null (pre-connect).
//
// Regression-freeze gate (Task 16) proved codec and legacy agree byte-for-byte
// on all non-HL2 boards. Dual-call diagnostic dropped. Legacy path still
// reachable via NEREUS_USE_LEGACY_P1_CODEC=1 env var until Phase B merges.
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcForBank(int bankIdx, quint8 out[5]) const
{
    if (m_useLegacyCodec || !m_codec) {
        composeCcForBankLegacy(bankIdx, out);
        return;
    }
    const CodecContext ctx = buildCodecContext();
    m_codec->composeCcForBank(bankIdx, ctx, out);
}

// ---------------------------------------------------------------------------
// composeCcBank0Full — instance-level bank 0 using actual state
//
// Source: Thetis networkproto1.c:450-471 (WriteMainLoop case 0)
// ---------------------------------------------------------------------------
void P1RadioConnection::composeCcBank0Full(quint8 out[5]) const
{
    // C0: MOX bit (networkproto1.c:446)
    out[0] = m_mox ? 0x01 : 0x00;

    // C1: sample rate (networkproto1.c:451)
    quint8 srBits = 0;
    if      (m_sampleRate >= 384000) { srBits = 3; }
    else if (m_sampleRate >= 192000) { srBits = 2; }
    else if (m_sampleRate >= 96000)  { srBits = 1; }
    out[1] = srBits & 0x03;

    // C2: OC outputs (networkproto1.c:452)
    out[2] = (m_ocOutput << 1) & 0xFE;

    // C3: preamp, dither, random, RX input (networkproto1.c:453-461)
    out[3] = (m_rxPreamp[0] ? 0x04 : 0)
           | (m_dither[0]   ? 0x08 : 0)
           | (m_random[0]   ? 0x10 : 0);
    // RX input select: default Rx_1_In (networkproto1.c:458)
    out[3] |= 0x20;

    // C4: antenna, duplex, NDDCs, diversity (networkproto1.c:463-471)
    out[4] = static_cast<quint8>(m_antennaIdx & 0x03);
    out[4] |= 0x04; // duplex bit
    int nddc = (m_activeRxCount < 1) ? 1 : (m_activeRxCount > 7 ? 7 : m_activeRxCount);
    out[4] |= static_cast<quint8>((nddc - 1) << 3);
    out[4] |= (m_diversity ? 0x80 : 0);
}

void P1RadioConnection::composeCcBank0(quint8*) { /* full implementation in Task 7 static helpers */ }
void P1RadioConnection::composeCcBank1(quint8*) { /* Task 7 */ }
void P1RadioConnection::composeCcBank2(quint8*) { /* Task 7 */ }
void P1RadioConnection::composeCcBank3(quint8*) { /* Task 7 */ }
void P1RadioConnection::composeCcTxFreq(quint8*)  { /* Task 7 */ }
void P1RadioConnection::composeCcAlexRx(quint8*)  { /* Task 7 */ }
void P1RadioConnection::composeCcAlexTx(quint8*)  { /* Task 7 */ }
void P1RadioConnection::composeCcOcOutputs(quint8*) { /* Task 7 */ }

void P1RadioConnection::hl2SendIoBoardTlv(const QByteArray&) { /* internal TLV helper — used by hl2SendIoBoardInit */ }
void P1RadioConnection::checkFirmwareMinimum(int)  { /* superseded by connectToRadio firmware check (Task 11) */ }

// ---------------------------------------------------------------------------
// hl2SendIoBoardInit
//
// Called from connectToRadio() after applyBoardQuirks() when
// m_caps->hasIoBoardHl2 is true.
//
// Source: mi0bot IoBoardHl2.cs — IOBoard singleton, readRequest() at lines
//   129-145 initiates I2C reads:
//     • addr 0x41, reg 0 → hardware version   (IoBoardHl2.cs:133-136)
//     • addr 0x1d, reg REG_FIRMWARE_MAJOR (9)  (IoBoardHl2.cs:139)
//     • addr 0x1d, reg REG_FIRMWARE_MINOR (10) (IoBoardHl2.cs:139)
//
// The C# side calls NetworkIO.I2CReadInitiate / I2CWrite which are P/Invoke
// into ChannelMaster.dll.  The underlying wire encoding is handled by the
// DLL's own I2C framing over ep2; there is no standalone TLV byte sequence
// in IoBoardHl2.cs that can be extracted and sent verbatim.
//
// TODO(3I-T12): When Phase 3L lands, locate the I2C-over-ep2 wire encoding
//   in ChannelMaster/network.c and port the full I2C read/write sequence so
//   that NereusSDR can probe the HL2 I/O board hardware version and register
//   map at startup.  For now we log a notice and return; the standard metis
//   start sequence is sufficient for HL2 RX operation without the I/O board.
// ---------------------------------------------------------------------------
void P1RadioConnection::hl2SendIoBoardInit()
{
    if (!m_caps || !m_caps->hasIoBoardHl2) { return; }

    // Source: mi0bot IoBoardHl2.cs:83-125 — IOBoard Registers enum:
    //   HardwareVersion  = -1 (I2C addr 0x41, reg 0)
    //   REG_FIRMWARE_MAJOR = 9, REG_FIRMWARE_MINOR = 10 (I2C addr 0x1d)
    //   All accessed via NetworkIO.I2CReadInitiate(bus=1, addr, reg).
    //
    // The I2C read/write wire format is internal to ChannelMaster.dll.
    // TODO(3I-T12): Port ChannelMaster I2C-over-ep2 framing for full init.
    qCInfo(lcConnection) << "HL2: I/O board init — I2C probe deferred (TODO(3I-T12));"
                         << "standard metis start is sufficient for RX";
}

// ---------------------------------------------------------------------------
// hl2CheckBandwidthMonitor
//
// Called from onWatchdogTick() when m_caps->hasBandwidthMonitor is true.
//
// Source: mi0bot bandwidth_monitor.{c,h} (copyright MW0LGE) —
//   GetInboundBps / GetOutboundBps compute a rolling byte-rate using Windows
//   InterlockedAdd64 and GetTickCount64 (bandwidth_monitor.c:86-123).
//   The original does NOT implement throttle detection; it is a byte-rate
//   telemetry helper that callers compare against an expected rate.
//
// NereusSDR interpretation: use ep6 sequence-gap count as a throttle proxy.
//   m_epRecvSeqExpected is incremented by parseEp6Frame on every good frame;
//   if the watchdog fires and m_epRecvSeqExpected has not advanced since the
//   previous tick the HL2 LAN PHY may be throttling the ep6 stream.
//
// TODO(3I-T12): Port the full byte-rate monitor using std::atomic<int64_t>
//   and std::chrono::steady_clock once Phase 3L adds the I/Q byte accounting
//   plumbing.  The throttle threshold (kBwThrottleGapCount below) should be
//   calibrated against a real HL2.
// ---------------------------------------------------------------------------
void P1RadioConnection::hl2CheckBandwidthMonitor()
{
    if (!m_caps || !m_caps->hasBandwidthMonitor) { return; }

    // Source: bandwidth_monitor.c:86-113 — compute_bps() measures the delta
    //   in total bytes between two ticks divided by elapsed ms.  We use
    //   sequence number stall as a simpler proxy: if m_epRecvSeqExpected
    //   is the same as last tick AND we have previously received at least
    //   one frame, count a potential throttle event.
    //
    // Throttle: 3 consecutive stall ticks (3 × 500 ms = 1.5 s) → flag.
    // Clear:    any single advancing tick resets the counter.

    static constexpr int kBwThrottleGapCount = 3;  // NereusSDR heuristic; TODO(3I-T12) calibrate

    static quint32 s_lastSeq = 0;

    if (!m_lastEp6At.isValid()) {
        // No frames yet — nothing to monitor.
        s_lastSeq = m_epRecvSeqExpected;
        return;
    }

    if (m_epRecvSeqExpected == s_lastSeq) {
        // Sequence stalled this tick.
        ++m_hl2ThrottleCount;
        if (!m_hl2Throttled && m_hl2ThrottleCount >= kBwThrottleGapCount) {
            m_hl2Throttled = true;
            m_hl2LastThrottleTick = QDateTime::currentDateTimeUtc();
            qCWarning(lcConnection) << "HL2: LAN PHY throttle detected —"
                                    << "ep6 sequence stalled for"
                                    << m_hl2ThrottleCount << "watchdog ticks;"
                                    << "pausing ep2 command frames";
            emit errorOccurred(RadioConnectionError::None,
                               QStringLiteral("HL2 LAN throttled — pausing ep2"));
        }
    } else {
        // Sequence advanced — clear throttle.
        if (m_hl2Throttled) {
            qCInfo(lcConnection) << "HL2: LAN throttle cleared — ep6 stream resumed";
            m_hl2Throttled = false;
        }
        m_hl2ThrottleCount = 0;
    }

    s_lastSeq = m_epRecvSeqExpected;
}

// ---------------------------------------------------------------------------
// scaleSample24
//
// Source: networkproto1.c:367-374 MetisReadThreadMainLoop — sample extraction
// uses (bptr[k+0] << 24 | bptr[k+1] << 16 | bptr[k+2] << 8) to sign-extend
// the 24-bit big-endian value into a 32-bit int, then multiplies by
// const_1_div_2147483648_ (= 1/2^31). The << 8 fill + divide by 2^31 is
// mathematically equivalent to our sign-extend then divide by 2^23 (= 8388608).
// ---------------------------------------------------------------------------
float P1RadioConnection::scaleSample24(const quint8 be24[3]) noexcept
{
    // Sign-extend 24-bit big-endian to qint32 via left-shift trick.
    // Source: networkproto1.c:368-370 — (bptr[k+0] << 24 | bptr[k+1] << 16 | bptr[k+2] << 8)
    qint32 v = (qint32(be24[0]) << 24)
             | (qint32(be24[1]) << 16)
             | (qint32(be24[2]) << 8);
    v >>= 8;  // arithmetic right-shift to sign-extend from 24 bits
    // Source: networkproto1.c:367 — const_1_div_2147483648_ * (shifted value >> 8)
    // Equivalent: v / 2^23 = v / 8388608
    return float(v) / 8388608.0f;
}

// ---------------------------------------------------------------------------
// parseEp6Frame
//
// Source: networkproto1.c:319-415 MetisReadThreadMainLoop — iterates 2 subframes,
// each 512 bytes. Within each subframe (bptr = FPGAReadBufp + 512*frame, which
// strips the 8-byte Metis header):
//   bptr[0..2]  = sync 7F 7F 7F
//   bptr[3..7]  = C0..C4 (C&C from radio)
//   samples at  bptr[8 + isample*(6*nddc+2) + iddc*6] (networkproto1.c:366)
//
// In our 1032-byte ep6 datagram the 8-byte Metis header is still present:
//   subframe 0: bptr equivalent starts at frame+8  → samples at frame+16
//   subframe 1: bptr equivalent starts at frame+520 → samples at frame+528
//
// Slot size: 6*numRx + 2  (networkproto1.c:361 — spr = 504 / (6*nddc + 2))
// Samples per subframe: 504 / slotBytes
// ---------------------------------------------------------------------------
bool P1RadioConnection::parseEp6Frame(const quint8 frame[1032],
                                       int numRx,
                                       std::vector<std::vector<float>>& perRx) noexcept
{
    // Validate numRx range (1..7 — Thetis supports up to 7 DDCs)
    if (numRx < 1 || numRx > 7) { return false; }

    // Validate Metis ep6 header: EF FE 01 06 + 4-byte sequence
    // Source: networkproto1.c:326-327 — check first four bytes (after MetisReadDirect strips header)
    // In our full datagram the magic lives at [0..3]
    if (frame[0] != 0xEF || frame[1] != 0xFE ||
        frame[2] != 0x01 || frame[3] != 0x06) {
        return false;
    }

    // Validate sync bytes for both USB subframes
    // Source: networkproto1.c:327 — (bptr[0]==0x7f && bptr[1]==0x7f && bptr[2]==0x7f)
    if (frame[8]   != 0x7F || frame[9]   != 0x7F || frame[10]  != 0x7F) { return false; }
    if (frame[520] != 0x7F || frame[521] != 0x7F || frame[522] != 0x7F) { return false; }

    // Source: networkproto1.c:361 — spr = 504 / (6 * nddc + 2)
    const int slotBytes        = 6 * numRx + 2;  // (I24+Q24)*numRx + Mic16
    const int samplesPerSubframe = 504 / slotBytes;

    perRx.assign(numRx, std::vector<float>());
    for (auto& v : perRx) {
        v.reserve(static_cast<size_t>(samplesPerSubframe * 2 * 2));  // 2 subframes × 2 floats/sample
    }

    // Parse one 512-byte subframe; sampleStart is the offset of the first sample
    // slot within the full 1032-byte datagram.
    // Source: networkproto1.c:366 — k = 8 + isample*(6*nddc+2) + iddc*6
    //   (where k is relative to bptr which starts at sync bytes)
    //   In our frame: sampleStart = subframeBase + 8 (sync3 + C&C5)
    auto parseSubframe = [&](int sampleStart) {
        for (int s = 0; s < samplesPerSubframe; ++s) {
            for (int r = 0; r < numRx; ++r) {
                // Source: networkproto1.c:366 — k = 8 + isample*slotBytes + iddc*6
                const int off = sampleStart + s * slotBytes + r * 6;
                const float i = scaleSample24(&frame[off]);
                const float q = scaleSample24(&frame[off + 3]);
                perRx[static_cast<size_t>(r)].push_back(i);
                perRx[static_cast<size_t>(r)].push_back(q);
            }
            // Mic16 bytes at offset sampleStart + s*slotBytes + numRx*6 are skipped
        }
    };

    // Subframe 0: sync at frame[8..10], C&C at [11..15], samples at [16..]
    parseSubframe(16);
    // Subframe 1: sync at frame[520..522], C&C at [523..527], samples at [528..]
    parseSubframe(528);

    return true;
}

// ---------------------------------------------------------------------------
// getAdcForDdc
// ---------------------------------------------------------------------------
int P1RadioConnection::getAdcForDdc(int /*ddc*/) const
{
    return 0;  // All P1 DDCs map to ADC0 for now
}

} // namespace NereusSDR
