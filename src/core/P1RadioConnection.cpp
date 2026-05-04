// =================================================================
// src/core/P1RadioConnection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/netInterface.c, original licence from Thetis source is included below
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
#include "OcMatrix.h"
#include "IoBoardHl2.h"
#include "HermesLiteBandwidthMonitor.h"
#include "audio/TxMicSource.h"
#include "codec/P1CodecStandard.h"
#include "codec/P1CodecAnvelinaPro3.h"
#include "codec/P1CodecRedPitaya.h"
#include "codec/P1CodecHl2.h"
#include "codec/AlexFilterMap.h"
#include "models/Band.h"

#include <array>
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
    // (Nearby upstream context — case 12 Step ATT control — carries the
    //  RedPitaya guard: `//[2.10.3.9]DH1KLM  //model needed as board type
    //  (prn->discovery.BoardType) is an OrionII` on networkproto1.c:612.)
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

    // Connect watchdog — single-shot; fires kConnectTimeoutMs after connectToRadio()
    // if no first ep6 frame arrives. Emits connectFailed(Timeout, ...).
    // Cancelled on first good ep6 in onReadyRead(). Design §4.1.
    m_connectWatchdog = new QTimer(this);
    m_connectWatchdog->setSingleShot(true);
    connect(m_connectWatchdog, &QTimer::timeout, this, &P1RadioConnection::onConnectTimeout);

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

    // Arm the connect watchdog: if no first ep6 arrives within kConnectTimeoutMs,
    // emit connectFailed(Timeout, ...). onReadyRead() cancels this on first good frame.
    if (m_connectWatchdog) {
        m_connectWatchdog->start(kConnectTimeoutMs);
    }

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
    // Cancel the connect watchdog so intentional disconnect() does not
    // trigger connectFailed() after the user has already moved on.
    if (m_connectWatchdog) {
        m_connectWatchdog->stop();
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

    // Close the UDP socket so the OS releases the bound port and any
    // pending I/O is drained on this (worker) thread. P2 already does
    // this; P1 previously kept the socket open "for re-use in Task 10"
    // but that never materialized and no caller reuses the socket.
    //
    // Issue #83: on Windows, handing the socket to Qt's parent-chain
    // destructor during process teardown (rather than closing it
    // explicitly while the worker's event loop is still servicing it)
    // left the HL2 UDP endpoint in a state that cascaded into a
    // Winsock stack that could not accept new binds until reboot —
    // Thetis failed to bind port 51188 the next time it was launched.
    if (m_socket) {
        m_socket->close();
    }

    setState(ConnectionState::Disconnected);
    qCDebug(lcConnection) << "P1: Disconnected";
}

void P1RadioConnection::setReceiverFrequency(int receiverIndex, quint64 frequencyHz)
{
    if (receiverIndex < 0 || receiverIndex >= 7) { return; }
    m_rxFreqHz[receiverIndex] = frequencyHz;
    // RX0 drives the Alex HPF bank — recompute on every change.
    // Source: console.cs:6830-6942 [@501e3f5]
    // Upstream inline attribution preserved verbatim:
    //   :6830  || (HardwareSpecific.Hardware == HPSDRHW.HermesIII)) //DK1HLM
    //
    // HL2 carve-out: HL2 has hasAlexFilters=false (no Alex card slot) but
    // mi0bot's WriteMainLoop_HL2 still emits these bits at bank 10 C3/C4
    // (networkproto1.c:1086-1093 [v2.10.3.14-beta1]) — the HL2 firmware
    // and the optional N2ADR I/O board read them to engage the band-correct
    // LPF/HPF.  Without them the HL2 PA can't safely engage on TX and the
    // T/R relay flutters.  Compute the bits for HL2 too, gated on the
    // hardware profile so other "no Alex" boards (Atlas, basic Hermes)
    // remain byte-identical to pre-fix behavior.
    if (receiverIndex == 0
        && (   (m_caps && m_caps->hasAlexFilters)
            || m_hardwareProfile.model == HPSDRModel::HERMESLITE)) {
        m_alexHpfBits = codec::alex::computeHpf(double(frequencyHz) / 1e6);
    }
}

void P1RadioConnection::setTxFrequency(quint64 frequencyHz)
{
    m_txFreqHz = frequencyHz;
    // TX freq drives Alex LPF — recompute on every change.
    // Source: console.cs:7168-7234 [@501e3f5]
    //
    // HL2 carve-out: see setReceiverFrequency above.  mi0bot
    // networkproto1.c:1090-1093 [v2.10.3.14-beta1] emits these bits on HL2
    // even though it has no Alex card.
    if (   (m_caps && m_caps->hasAlexFilters)
        || m_hardwareProfile.model == HPSDRModel::HERMESLITE) {
        m_alexLpfBits = codec::alex::computeLpf(double(frequencyHz) / 1e6);
    }
}
void P1RadioConnection::setActiveReceiverCount(int count)    { m_activeRxCount = count; }
void P1RadioConnection::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;
    // From Thetis ChannelMaster/netInterface.c:1287-1310 [v2.10.3.14]:
    // mic_decimation_factor is set from sample rate; counter is reset.
    const int newFactor = micDecimationFactorFor(sampleRate);
    if (newFactor != m_micDecimationFactor) {
        m_micDecimationFactor = newFactor;
        m_micDecimationCount  = 0;  // matches netInterface.c:1310
    }
}

// ---------------------------------------------------------------------------
// micDecimationFactorFor — sample-rate → factor lookup.
// Source: Thetis ChannelMaster/netInterface.c:1287-1310 [v2.10.3.14]
//   case 48000:  mic_decimation_factor = 1; break;
//   case 96000:  mic_decimation_factor = 2; break;
//   case 192000: mic_decimation_factor = 4; break;
//   case 384000: mic_decimation_factor = 8; break;
//   default:     mic_decimation_factor = 4; break;
// ---------------------------------------------------------------------------
int P1RadioConnection::micDecimationFactorFor(int sampleRate) noexcept
{
    switch (sampleRate) {
        case 48000:  return 1;
        case 96000:  return 2;
        case 192000: return 4;
        case 384000: return 8;
        default:     return 4;  // matches netInterface.c:1306-1307 default branch
    }
}

// ---------------------------------------------------------------------------
// decimateMicSamples — downsample radio-rate mic to 48 kHz.
//
// Source: Thetis ChannelMaster/networkproto1.c:391-410 [v2.10.3.14] —
//   for each input sample:
//     mic_decimation_count++;
//     if (mic_decimation_count == mic_decimation_factor) {
//         mic_decimation_count = 0;
//         <emit sample>;
//     }
//
// `counter` is the persistent state — caller owns it (matches the global
// static mic_decimation_count in Thetis, reset once at thread init per
// networkproto1.c:275 [v2.10.3.14]).
//
// factor==1 fast path: copy through unchanged (matches Thetis behavior at
// 48 kHz where the equality check fires every sample).
// ---------------------------------------------------------------------------
void P1RadioConnection::decimateMicSamples(const float* in, int n, int factor,
                                            int& counter,
                                            std::vector<float>& out) noexcept
{
    if (n <= 0 || in == nullptr) { return; }
    if (factor <= 1) {
        out.insert(out.end(), in, in + n);
        return;
    }
    out.reserve(out.size() + static_cast<size_t>(n / factor) + 1);
    for (int i = 0; i < n; ++i) {
        ++counter;
        if (counter == factor) {
            counter = 0;
            out.push_back(in[i]);
        }
    }
}
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
// ---------------------------------------------------------------------------
// setTxDrive — 3M-1c follow-up (HL2 bench triage 2026-04-29)
//
// Stores the TX drive level (0-255) and forces bank 10 onto the wire on the
// next sendCommandFrame() so the new drive level reaches the radio within
// ≤1 EP2 frame (~2.6 ms at 380.95 pps).
//
// Bank 10 C1 byte carries the drive level; the codec reads ctx.txDrive which
// is sourced from m_txDrive in buildCodecContext.  Without this setter, the
// drive level was silently fixed at zero and HL2 / Atlas / Hermes / Angelia /
// Orion never produced RF on TUN or SSB.  The bug was latent because the
// 3M-1b SSB voice bench tests ran on ANAN-G2 (P2), which has its own
// setTxDrive on P2RadioConnection that always worked.
//
// From Thetis ChannelMaster/networkproto1.c:579 [v2.10.3.13]:
//   C1 = prn->tx[0].drive_level & 0xFF;
// From mi0bot networkproto1.c:1061 [@c26a8a4]: identical encoding.
// ---------------------------------------------------------------------------
void P1RadioConnection::setTxDrive(int level)
{
    const int clamped = qBound(0, level, 255);
    if (m_txDrive == clamped) {
        return;  // idempotent — wire emit already in flight via round-robin
    }
    m_txDrive = clamped;
    // Codex P2 safety-effect pattern: force bank 10 on the next frame so
    // the new drive level reaches the wire within ≤1 EP2 frame.  Without
    // this, the round-robin schedule would defer bank 10 by up to 17
    // frames (~45 ms), which is plenty long for a TUN tap to land mid-
    // round-robin and never see a non-zero drive.
    m_forceBank10Next = true;
}

// ---------------------------------------------------------------------------
// setTxStepAttenuation — 3M-1a Task F.2
//
// Mirrors Thetis ChannelMaster/netInterface.c:1006 SetTxAttenData(int bits)
// [v2.10.3.13]: broadcasts the TX step attenuator value to all ADCs.
// The P1 codec reads m_txStepAttn in composeCcBank0 / composeCcBank1 and
// writes it to the appropriate C&C byte.
//
// From Thetis ChannelMaster/netInterface.c:1006-1016 [v2.10.3.13]:
//   void SetTxAttenData(int bits) {
//     for (i = 0; i < MAX_ADC; i++) prn->adc[i].tx_step_attn = bits;
//     if (listenSock != INVALID_SOCKET) CmdTx();
//   }
// ---------------------------------------------------------------------------
void P1RadioConnection::setTxStepAttenuation(int dB)
{
    if (dB < 0)  { dB = 0; }
    if (dB > 63) { dB = 63; }  // HL2 has 6-bit field; standard boards 5-bit
    m_txStepAttn = dB;
}
// ---------------------------------------------------------------------------
// setMox — 3M-1a Task E.3
//
// Wire-byte emission: the MOX bit is C0 byte 3 bit 0 (0x01) in the P1 C&C
// bank-0 frame.  composeCcBank0Full() writes:
//   out[0] = m_mox ? 0x01 : 0x00;
// Source: deskhpsdr/src/old_protocol.c:3597 [@120188f]
//   output_buffer[C0] |= 0x01;  // Always set MOX if non-CW transmitting
// HL2 firmware cross-check: dsopenhpsdr1.v:297
//   ds_cmd_ptt_next = eth_data[0]  // bit 0 of the C0 byte = PTT/MOX
//
// Codex P2 — safety-effect-first idempotent-guard pattern:
//   1. Force a bank-0 frame on the NEXT sendCommandFrame() call so the MOX
//      bit lands on the wire within ≤1 frame regardless of round-robin phase.
//      This is the safety effect; it fires even on repeated calls with the
//      same value (ensures the bit is actually emitted).
//   2. Guard: if the requested value equals the stored value, update the
//      flush flag and return — no further state change.
//
// CW gating (txmode == modeCWU/L branch from deskhpsdr:3596-3598) is 3M-2.
// ---------------------------------------------------------------------------
void P1RadioConnection::setMox(bool enabled)
{
    // Codex P2 safety effect: force bank 0 on next frame so the MOX bit
    // is emitted within ≤1 frame of this call.
    // Source: deskhpsdr/src/old_protocol.c:3595-3599 [@120188f]
    m_forceBank0Next = true;

    if (m_mox == enabled) {
        return;  // idempotent — state unchanged, flush flag already set above
    }
    m_mox = enabled;
}
// ---------------------------------------------------------------------------
// setAntennaRouting — Phase 3P-I-a
//
// Stores trxAnt into m_antennaIdx; the next round-robin pass through
// bank 0 composes it into C4 bits 0-1 via P1CodecStandard::bank0 (or
// P1CodecHl2::bank0 on HL2 — identical encoding at the antenna bit level).
//
// From Thetis ChannelMaster/networkproto1.c:463-468 [v2.10.3.13 @501e3f5] —
//   if (prbpfilter->_ANT_3 == 1)       C4 = 0b10;
//   else if (prbpfilter->_ANT_2 == 1)  C4 = 0b01;
//   else                                C4 = 0b0;
//
// 3P-I-b (T4): rxOnlyAnt (C3 bits 5-6) and rxOut (C3 bit 7) are live —
// forwarded through buildCodecContext() into P1Codec::bank0 per Thetis
// networkproto1.c:455-461 [v2.10.3.13 @501e3f5].
// ---------------------------------------------------------------------------
void P1RadioConnection::setAntennaRouting(AntennaRouting r)
{
    const int clamped = (r.trxAnt < 1 || r.trxAnt > 3) ? 0 : (r.trxAnt - 1);
    m_antennaIdx = clamped;  // 0..2 (or 0 for "no selection")
    // RX-only input mux: clamp to 0..3 per netInterface.c:479-481 [v2.10.3.13 @501e3f5]
    m_rxOnlyAnt = (r.rxOnlyAnt < 0) ? 0 : (r.rxOnlyAnt > 3 ? 3 : r.rxOnlyAnt);
    m_rxOut     = r.rxOut;
    // P1 has no high-priority packet; the next EP2 frame picks up all
    // antenna fields via buildCodecContext() → P1Codec::bank0.
}

// ---------------------------------------------------------------------------
// setWatchdogEnabled — 3M-1a Task E.5
//
// Records the requested watchdog enable state in the base-class
// m_watchdogEnabled field (shared with P2). The wire bit is emitted by
// sendMetisStart() and sendMetisStop() on the next start/stop cycle — not
// immediately — matching deskhpsdr behavior (no re-send on toggle).
//
// Wire format (HL2 firmware primary cite):
//   Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400
//     watchdog_disable <= eth_data[7]; // Bit 7 can be used to disable watchdog
//   Inverted semantic: bit 7 = 1 means disabled, bit 7 = 0 means enabled.
//
// Thetis call-site (setup.cs:17986 [v2.10.3.13]):
//   NetworkIO.SetWatchdogTimer(Convert.ToInt32(chkNetworkWDT.Checked));
//   chkNetworkWDT.Checked == true => value 1 => watchdog ENABLED => bit 7 = 0.
//
// Thetis DllImport (NetworkIOImports.cs:197-198 [v2.10.3.13]):
//   public static extern void SetWatchdogTimer(int bits);
//
// deskhpsdr reference (deskhpsdr/src/old_protocol.c:3811 [@120188f]):
//   buffer[3] = command;  // no bit-7 OR -- watchdog always enabled (bit 7 = 0)
//   deskhpsdr has no user-configurable watchdog disable; it never re-sends
//   RUNSTOP on a watchdog toggle.  NereusSDR matches: state stored here,
//   picked up on the next sendMetisStart() / sendMetisStop() call.
// ---------------------------------------------------------------------------
void P1RadioConnection::setWatchdogEnabled(bool enabled)
{
    if (m_watchdogEnabled == enabled) {
        return;
    }
    m_watchdogEnabled = enabled;
    // No immediate re-send: matches deskhpsdr pattern (no standalone RUNSTOP
    // packet for watchdog toggle). The new state is included in the next
    // sendMetisStart() or sendMetisStop() call.
}

// ---------------------------------------------------------------------------
// sendTxIq — 3M-1a Task E.2
//
// Porting from deskhpsdr/src/old_protocol.c:2373-2459 [@120188f]
// (old_protocol_iq_samples) — original C logic:
//
//   Per sample (8 bytes each, TXRING_AUDIO_SAMPLE_BYTES = 8):
//     TXRINGBUF[iptr++] = left_audio_sample >> 8;   // mic L hi
//     TXRINGBUF[iptr++] = left_audio_sample;         // mic L lo
//     TXRINGBUF[iptr++] = right_audio_sample >> 8;   // mic R hi
//     TXRINGBUF[iptr++] = right_audio_sample;         // mic R lo
//     TXRINGBUF[iptr++] = isample >> 8;               // I hi
//     TXRINGBUF[iptr++] = isample;                    // I lo
//     TXRINGBUF[iptr++] = qsample >> 8;               // Q hi
//     TXRINGBUF[iptr++] = qsample;                    // Q lo
//
//   Float→int16 gain (old protocol = 16-bit, not 24-bit):
//     gain = 32767.0  (deskhpsdr/src/transmitter.c:1541 [@120188f])
//     isample = (long)(is * gain + (is >= 0.0 ? 0.5 : -0.5))
//
// Accepts n interleaved float32 I/Q pairs [I0,Q0, I1,Q1, ...] from the
// WDSP TX channel output.  Converts each pair to 8 wire bytes and appends
// them to the ring buffer m_txIqBuf.  If the buffer is full, excess samples
// are dropped with a debug log (matches deskhpsdr overflow path).
//
// The EP2 pacer (onEp2PacerTick → sendCommandFrame) drains 63+63 = 126
// samples per frame call via fillTxZone().
//
// Cite: deskhpsdr/src/old_protocol.c:458-463 [@120188f]
//   TXRING_AUDIO_SAMPLE_BYTES   8
//   TXRING_AUDIO_FRAMES_PER_BLOCK 126  — one SDR block (= two EP2 subframes)
// ---------------------------------------------------------------------------
void P1RadioConnection::sendTxIq(const float* iq, int n)
{
    if (n <= 0 || iq == nullptr) { return; }

    // From deskhpsdr/src/transmitter.c:1541 [@120188f]
    //   gain = 32767.0;  // 16 bit (ORIGINAL_PROTOCOL)
    static constexpr float kGain = 32767.0f;

    static constexpr int kBufBytes = kTxIqBufSamples * kTxIqBytesPerSample;

    // HL2 CWX firmware workaround: clear LSB of I/Q low bytes to avoid
    // the CWX activation-while-key-asserted misbehavior. Per
    // deskhpsdr/src/old_protocol.c:2441-2453 [@120188f]. Cost: 1 LSB of
    // I/Q resolution on HL2's 12-bit DAC (immaterial).
    const bool isHl2 = (m_hardwareProfile.model == HPSDRModel::HERMESLITE);
    const quint8 iLoMask = isHl2 ? 0xFE : 0xFF;
    const quint8 qLoMask = isHl2 ? 0xFE : 0xFF;

    for (int k = 0; k < n; ++k) {
        // Ring-buffer full: drop sample, matching deskhpsdr overflow path.
        // acquire: see the latest fetch_sub from the connection thread so we
        // don't overfill after a drain.
        if (m_txIqCount.load(std::memory_order_acquire) >= kTxIqBufSamples) {
            qCDebug(lcConnection) << "P1 TX I/Q ring buffer overflow — dropping sample";
            break;
        }

        const float fI = iq[k * 2];
        const float fQ = iq[k * 2 + 1];

        // Float → int16 conversion.
        // From deskhpsdr/src/transmitter.c:1787-1788 [@120188f]
        //   isample = (long)(is * gain + (is >= 0.0 ? 0.5 : -0.5));
        //   qsample = (long)(qs * gain + (qs >= 0.0 ? 0.5 : -0.5));
        auto toInt16 = [](float v) -> int16_t {
            const float scaled = v * kGain + (v >= 0.0f ? 0.5f : -0.5f);
            if (scaled >= 32767.0f)  { return  32767; }
            if (scaled <= -32767.0f) { return -32767; }
            return static_cast<int16_t>(scaled);
        };
        const int16_t iSample = toInt16(fI);
        const int16_t qSample = toInt16(fQ);

        // Write 8 bytes: [mic_L hi][mic_L lo][mic_R hi][mic_R lo][I hi][I lo][Q hi][Q lo]
        // From deskhpsdr/src/old_protocol.c:2429-2458 [@120188f]
        //   mic bytes zero — NullMicSource (3M-1b will fill them)
        int wp = m_txIqWritePos.load(std::memory_order_relaxed);
        m_txIqBuf[wp++] = 0;                                                      // mic_L hi
        m_txIqBuf[wp++] = 0;                                                      // mic_L lo
        m_txIqBuf[wp++] = 0;                                                      // mic_R hi
        m_txIqBuf[wp++] = 0;                                                      // mic_R lo
        m_txIqBuf[wp++] = static_cast<quint8>((iSample >> 8) & 0xFF);            // I hi
        m_txIqBuf[wp++] = static_cast<quint8>( iSample       & iLoMask);         // I lo
        m_txIqBuf[wp++] = static_cast<quint8>((qSample >> 8) & 0xFF);            // Q hi
        m_txIqBuf[wp++] = static_cast<quint8>( qSample       & qLoMask);         // Q lo
        if (wp >= kBufBytes) { wp = 0; }
        // relaxed: single writer; the release on m_txIqCount below provides
        // the visibility fence for the byte writes.
        m_txIqWritePos.store(wp, std::memory_order_relaxed);
        // release: publishes the byte writes above before the count increment
        // is observed by the connection thread's acquire load.
        m_txIqCount.fetch_add(1, std::memory_order_release);
    }
}

// ---------------------------------------------------------------------------
// fillTxZone
//
// Drains exactly 63 samples from the TX I/Q ring buffer into a 504-byte EP2
// TX data zone (63 × 8 bytes = 504 bytes).  If fewer than 63 samples are
// buffered, the zone is zero-filled (silence — matches deskhpsdr behaviour
// when the ring buffer underruns).
//
// Called from sendCommandFrame() for each of the two 504-byte zones in the
// 1032-byte Metis EP2 frame ([16..519] and [528..1031]).
//
// Cite: deskhpsdr/src/old_protocol.c:545-549 [@120188f]
//   memcpy(output_buffer + 8, &TXRINGBUF[out], 504);
//   ozy_send_buffer();
//   memcpy(output_buffer + 8, &TXRINGBUF[out + 504], 504);
//   ozy_send_buffer();
//
// Returns true when samples were available, false on underrun.
// ---------------------------------------------------------------------------
bool P1RadioConnection::fillTxZone(quint8* zone63) noexcept
{
    static constexpr int kSamplesPerZone = 63;
    static constexpr int kBufBytes       = kTxIqBufSamples * kTxIqBytesPerSample;

    // acquire: makes the audio thread's byte writes (published via release
    // fetch_add on m_txIqCount) visible before we read m_txIqBuf.
    if (m_txIqCount.load(std::memory_order_acquire) < kSamplesPerZone) {
        // Underrun — zero-fill the zone (silence).  zone63 is already zeroed
        // by sendCommandFrame()'s memset, so no explicit fill is needed.
        return false;
    }

    for (int i = 0; i < kSamplesPerZone; ++i) {
        int rp = m_txIqReadPos.load(std::memory_order_relaxed);
        zone63[i * kTxIqBytesPerSample + 0] = m_txIqBuf[rp++]; // mic_L hi
        zone63[i * kTxIqBytesPerSample + 1] = m_txIqBuf[rp++]; // mic_L lo
        zone63[i * kTxIqBytesPerSample + 2] = m_txIqBuf[rp++]; // mic_R hi
        zone63[i * kTxIqBytesPerSample + 3] = m_txIqBuf[rp++]; // mic_R lo
        zone63[i * kTxIqBytesPerSample + 4] = m_txIqBuf[rp++]; // I hi
        zone63[i * kTxIqBytesPerSample + 5] = m_txIqBuf[rp++]; // I lo
        zone63[i * kTxIqBytesPerSample + 6] = m_txIqBuf[rp++]; // Q hi
        zone63[i * kTxIqBytesPerSample + 7] = m_txIqBuf[rp++]; // Q lo
        if (rp >= kBufBytes) { rp = 0; }
        // relaxed: single writer on this side; the acquire on m_txIqCount
        // above already provides the required ordering fence.
        m_txIqReadPos.store(rp, std::memory_order_relaxed);
    }
    // relaxed: the audio thread observes this via its acquire load on
    // m_txIqCount before deciding whether the buffer has space.
    m_txIqCount.fetch_sub(kSamplesPerZone, std::memory_order_relaxed);
    return true;
}

// ---------------------------------------------------------------------------
// setTrxRelay — 3M-1a Task E.4
//
// Sets or clears the Alex T/R relay engage state.
// Wire location: bank 10 (C0=0x12), C3 byte, bit 7 (0x80).
// Semantic INVERTED vs MOX: 1 = relay disabled (PA bypass / RX-only protect),
//                           0 = relay engaged (normal TX path).
//
// enabled=true  → bit 7 = 0 (relay engaged, current flows through relay)
// enabled=false → bit 7 = 1 (relay open / PA bypassed)
//
// Primary cite: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
//   if (txband->disablePA || !pa_enabled)
//       output_buffer[C3] |= 0x80; // disable Alex T/R relay
//
// HL2 note: HL2 clears C2/C3/C4 entirely for its own PA-enable path
// (old_protocol.c:2964-2966 [@120188f]), so this bit is irrelevant for
// HL2 hardware.  The composeCcForBank case 10 emits it only for
// Alex-equipped boards; the codec layer handles HL2-specific encoding.
//
// Codex P2 pattern (safety effect before idempotent guard):
// Force bank 10 onto the wire within ≤1 frame of this call so the relay
// state change is immediate, matching deskhpsdr's non-deferred behaviour.
// ---------------------------------------------------------------------------
void P1RadioConnection::setTrxRelay(bool enabled)
{
    // Codex P2: force bank 10 flush BEFORE the idempotent guard so the
    // relay bit lands on the next outbound frame regardless of whether
    // the state actually changed.
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f] — the
    // reference implementation writes the T/R relay bit every command
    // frame; we flush immediately on state change to match timing.
    m_forceBank10Next = true;

    if (m_trxRelay == enabled) {
        return;  // idempotent — flush flag already set above
    }
    m_trxRelay = enabled;
}

// ---------------------------------------------------------------------------
// setMicBoost (3M-1b G.1)
//
// Sets the hardware mic-jack 20 dB boost preamp bit.
// Wire bit: bank 10 (C0=0x12) C2 byte bit 0 (mask 0x01).
// Polarity: 1 = boost on (no inversion).
//
// Porting from Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
//   C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1) | ...)
//   mic_boost occupies the lowest bit of C2.
//
// Flush pattern mirrors setTrxRelay (Codex P2): m_forceBank10Next is set
// before the idempotent guard so the bit lands on the wire within ≤1 frame.
// ---------------------------------------------------------------------------
void P1RadioConnection::setMicBoost(bool on)
{
    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank10Next = true;

    if (m_micBoost == on) {
        return;  // idempotent — flush flag already set above
    }
    m_micBoost = on;
}

// ---------------------------------------------------------------------------
// setLineIn (3M-1b G.2)
//
// Sets the hardware mic-jack line-in path bit.
// Wire bit: bank 10 (C0=0x12) C2 byte bit 1 (mask 0x02).
// Polarity: 1 = line in active (no inversion).
//
// Porting from Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
//   C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1) | ...)
//   line_in occupies bit 1 of C2 (mic_boost is bit 0).
//
// Flush pattern mirrors setMicBoost (Codex P2): m_forceBank10Next is set
// before the idempotent guard so the bit lands on the wire within ≤1 frame.
// ---------------------------------------------------------------------------
void P1RadioConnection::setLineIn(bool on)
{
    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank10Next = true;

    if (m_lineIn == on) {
        return;  // idempotent — flush flag already set above
    }
    m_lineIn = on;
}

// ---------------------------------------------------------------------------
// setMicTipRing (3M-1b G.3)
//
// Selects mic-jack Tip/Ring polarity.
// NereusSDR parameter convention: tipHot = true → Tip carries the mic signal.
//
// POLARITY INVERSION AT THE WIRE LAYER:
// Thetis field mic_trs and deskhpsdr field mic_ptt_tip_bias_ring both mean
// "1 = Tip is BIAS/PTT" (i.e. NOT the mic).  So:
//   tipHot = true  → Tip is mic    → wire bit CLEAR (0)
//   tipHot = false → Tip is BIAS   → wire bit SET   (1)
// The implementation writes (!m_micTipRing) to bit 4 of bank-11 C1.
//
// Wire bit: bank 11 (C0=0x14) C1 byte bit 4 (mask 0x10), INVERTED.
//
// Porting from Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
//   C1 = ... | ((prn->mic.mic_trs & 1) << 4) | ...
//   mic_trs: 1 = tip is BIAS/PTT (ring is mic), 0 = tip is mic (normal).
//
// First touch of case 11 / bank 11: adds m_forceBank11Next flush flag +
// round-robin chooser extension + captureBank11ForTest/forceBank11NextForTest
// test seams.  Bits 0-3 of C1 carry per-ADC preamp flags (Thetis quirk:
// bit 3 = rx0 again) and are untouched by this setter (OR into C1, never AND).
//
// Flush pattern mirrors setLineIn (Codex P2): m_forceBank11Next is set
// before the idempotent guard so the bit lands on the wire within ≤1 frame.
// ---------------------------------------------------------------------------
void P1RadioConnection::setMicTipRing(bool tipHot)
{
    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_micTipRing == tipHot) {
        return;  // idempotent — flush flag already set above
    }
    m_micTipRing = tipHot;
}

// ---------------------------------------------------------------------------
// setMicBias (3M-1b G.4)
//
// Enables or disables hardware mic-jack phantom power (bias voltage).
// Polarity: on=true → bias enabled → wire bit SET (no inversion).
//
// Wire bit: bank 11 (C0=0x14) C1 byte bit 5 (mask 0x20).
// This is the SAME C1 byte as G.3 (mic_trs bit 4) — both are OR'd in.
//
// Porting from Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
//   C1 = ... | ((prn->mic.mic_bias & 1) << 5) | ...
//   mic_bias: 1 = bias on, 0 = bias off (no polarity inversion).
//
// Flush pattern mirrors setMicTipRing (Codex P2): m_forceBank11Next is set
// before the idempotent guard so the bit lands on the wire within ≤1 frame.
// Reuses m_forceBank11Next + captureBank11ForTest infrastructure added in G.3.
// ---------------------------------------------------------------------------
void P1RadioConnection::setMicBias(bool on)
{
    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_micBias == on) {
        return;  // idempotent — flush flag already set above
    }
    m_micBias = on;
}

// ---------------------------------------------------------------------------
// setLineInGain (Task 2.1 of P1 full-parity epic)
//
// Updates m_lineInGain (shared base member) and arms m_forceBank11Next so
// the new line-in-gain value lands on the wire within ≤1 frame of the call.
// 5-bit field (0..31) — clamped at the API boundary.  Higher values produce
// greater line-in attenuation per Thetis upstream semantic.
//
// Wire bytes: bank 11 (C0=0x14) C2 low 5 bits.  C2 also carries
// puresignal_run at bit 6 (Task 2.5 plumbing); both bits OR into the same
// codec C2 byte and never collide.
//
// Porting from Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]
//   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
//
// Flush pattern mirrors setMicBias (Codex P2): m_forceBank11Next is set
// BEFORE the idempotent guard so the bit lands on the wire within ≤1 frame.
// Reuses m_forceBank11Next + captureBank11ForTest infrastructure from G.3.
// ---------------------------------------------------------------------------
void P1RadioConnection::setLineInGain(int gain)
{
    const int clamped = qBound(0, gain, 31);

    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_lineInGain == clamped) {
        return;  // idempotent — flush flag already set above
    }
    m_lineInGain = clamped;
}

// ---------------------------------------------------------------------------
// setUserDigOut (Task 2.2 of P1 full-parity epic)
//
// Updates m_userDigOut (shared base member) and arms m_forceBank11Next so
// the new user-dig-out value lands on the wire within ≤1 frame of the call.
// 4-bit field (0..15) — masked at the API boundary via & 0x0F so values above
// 15 silently wrap to the low nibble (the codec also masks via & 0x0F, so
// stored state matches the wire bytes 1:1 either way).
//
// Wire bytes: bank 11 (C0=0x14) C3 low 4 bits — the same bank as
// setLineInGain (C2) and setMicBias / setMicTipRing / setMicPTT (C1), all of
// which OR into different bytes of the same case-11 frame.
//
// Porting from Thetis ChannelMaster/networkproto1.c:601 [v2.10.3.13]
//   C3 = prn->user_dig_out & 0b00001111;
//
// Flush pattern mirrors setLineInGain / setMicBias (Codex P2): m_forceBank11Next
// is set BEFORE the idempotent guard so the bits land on the wire within
// ≤1 frame.  Reuses m_forceBank11Next + captureBank11ForTest infrastructure
// from G.3.
// ---------------------------------------------------------------------------
void P1RadioConnection::setUserDigOut(quint8 dig)
{
    const quint8 masked = dig & 0x0F;

    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_userDigOut == masked) {
        return;  // idempotent — flush flag already set above
    }
    m_userDigOut = masked;
}

// ---------------------------------------------------------------------------
// setPuresignalRun (Task 2.3 of P1 full-parity epic)
//
// Updates m_puresignalRun (shared base member) and arms m_forceBank11Next so
// the new puresignal-run flag lands on the wire within ≤1 frame of the call.
// Bool field — wire bit 6 of bank 11 C2.
//
// Wire bytes: bank 11 (C0=0x14) C2 bit 6 (mask 0x40) — the same C2 byte that
// also carries line_in_gain (low 5 bits, Task 2.1).  Both fields OR together
// into the same byte; this setter only flips bit 6, never touching the low
// 5 bits — see test §3 (cross-bit guard for line_in_gain).
//
// Porting from Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]
//   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
//
// Semantic: this flag tracks whether PureSignal feedback DDC routing is
// currently *active* on the wire — distinct from BoardCapabilities.hasPureSignal
// (capability) and from TransmitModel::puresignalEnabled (user toggle).  The
// wiring from PureSignalApplet's "Enable" toggle to this setter is Task 2.5,
// not this task; until 3M-4 lights up the actual feedback DDC routing, the
// applet toggle is the proxy that drives this flag.
//
// Flush pattern mirrors setLineInGain / setUserDigOut (Codex P2):
// m_forceBank11Next is set BEFORE the idempotent guard so the bit lands on
// the wire within ≤1 frame.  Reuses m_forceBank11Next + captureBank11ForTest
// infrastructure from G.3.
// ---------------------------------------------------------------------------
void P1RadioConnection::setPuresignalRun(bool run)
{
    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_puresignalRun == run) {
        return;  // idempotent — flush flag already set above
    }
    m_puresignalRun = run;
}

// ---------------------------------------------------------------------------
// setMicPTT (3M-1b G.5)
//
// Enables or disables the hardware mic-jack PTT line (Orion/ANAN front-panel).
// NereusSDR API contract: enabled=true → PTT enabled → wire bit 6 SET; this
// is direct polarity, mirroring Thetis networkproto1.c:597-598 [v2.10.3.13]
// (commit @501e3f51):
//   C1 = ... | ((prn->mic.mic_ptt & 1) << 6);
//
// Wire bit: bank 11 (C0=0x14) C1 byte bit 6 (mask 0x40), DIRECT polarity.
// This is the SAME C1 byte as G.3 (bit 4) + G.4 (bit 5) — all OR'd in.
//
// Pre-fix history: the codec wrote `!enabled` to the wire (inverted),
// mirroring the same bug PR #161 fixed in P1CodecHl2 (commit ca8cd73).  With
// the default m_micPTT=false, the inverted code put bit 6 = 1 every CC frame;
// Hermes-class firmware reads bit 6 as "track mic-jack tip as PTT source" and
// the floating mic tip caused phantom PTT signals fighting software MOX —
// rapid T/R relay flutter on TUNE/TX (ANAN-10E bench symptom).  Codec is now
// direct; setMicPTT(true) sets the wire bit, setMicPTT(false) clears it.
//
// Cross-reference notes:
//   deskhpsdr/src/old_protocol.c:3000-3002 [@120188f]: deskhpsdr's
//     `mic_ptt_enabled` is a higher-level wrapper that inverts before
//     writing the wire field; the wire field itself (mic_ptt) is direct.
//   Thetis console.cs:19758 [v2.10.3.13]: `MicPTTDisabled` property is a
//     UI-facing inverted view; it calls `NetworkIO.SetMicPTT(disabled?1:0)`,
//     and the wire field is direct.
//   deskhpsdr/src/new_protocol.c:1488-1490 [@120188f] — P2 byte 50 bit 2
//     (P2 mic_ptt path, separate codec, same direct-polarity convention).
//
// Flush pattern mirrors setMicBias (Codex P2): m_forceBank11Next is set
// BEFORE the idempotent guard so the bit lands on the wire within ≤1 frame.
// Reuses m_forceBank11Next + captureBank11ForTest infrastructure from G.3.
// ---------------------------------------------------------------------------
void P1RadioConnection::setMicPTT(bool enabled)
{
    // Codex P2: set flush flag BEFORE idempotent guard.
    m_forceBank11Next = true;

    if (m_micPTT == enabled) {
        return;  // idempotent — flush flag already set above
    }
    m_micPTT = enabled;
}

// ---------------------------------------------------------------------------
// setMicXlr (3M-1b G.6)
//
// Saturn G2 P2-only feature; P1 hardware has no XLR jack.
// Setter stores the flag for cross-board API consistency but does NOT
// emit any wire bytes. P1 case-10 and case-11 C&C bytes are UNCHANGED
// regardless of m_micXlr value.
//
// P2 source: deskhpsdr/src/new_protocol.c:1500-1502 [@120188f]:
//   if (mic_input_xlr) { transmit_specific_buffer[50] |= 0x20; }
//   (byte 50 bit 5 = 0x20, polarity 1=XLR jack — no inversion)
//   P2 implementation in P2RadioConnection::setMicXlr().
// ---------------------------------------------------------------------------
void P1RadioConnection::setMicXlr(bool xlrJack)
{
    // Saturn G2 P2-only feature; P1 hardware has no XLR jack.
    // Setter stores the flag for cross-board consistency but does NOT
    // emit any wire bytes. P1 case-10 and case-11 C&C bytes are
    // unchanged regardless of m_micXlr value.
    if (m_micXlr == xlrJack) {
        return;
    }
    m_micXlr = xlrJack;
}

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
    // RadioModel calls setIoBoard() BEFORE the connection thread starts (and
    // therefore before applyBoardQuirks() runs selectCodec()), so the cached
    // pointer is already in m_ioBoard but did not land on the freshly-built
    // codec.  Push it now so HL2 I2C compose works on the very first frame.
    if (m_ioBoard) {
        if (auto* hl2Codec = dynamic_cast<P1CodecHl2*>(m_codec.get())) {
            hl2Codec->setIoBoard(m_ioBoard);
        }
    }
    qCInfo(lcConnection) << "P1: selected codec for HPSDRModel" << int(m_hardwareProfile.model);
}

// ---------------------------------------------------------------------------
// setOcMatrix — Phase 3P-D Task 3
//
// Wires the RadioModel's OcMatrix to this connection so buildCodecContext()
// can source ctx.ocByte from maskFor(currentBand, mox) at C&C compose time.
// Called by RadioModel::connectToRadio() on the main thread before the
// connection thread is started, so no synchronisation is needed.
// ---------------------------------------------------------------------------
void P1RadioConnection::setOcMatrix(const OcMatrix* matrix)
{
    m_ocMatrix = matrix;
}

// ---------------------------------------------------------------------------
// setIoBoard — Phase 3P-E Task 2
//
// Wires the RadioModel's IoBoardHl2 to this connection for I2C intercept
// (outbound) and ep6 response parsing (inbound).  On HL2 boards, pushes
// the pointer into P1CodecHl2 so tryComposeI2cFrame() can dequeue txns.
// On non-HL2 boards, m_codec is not a P1CodecHl2, so the dynamic_cast
// returns null and the codec push is a noop — only m_ioBoard is stored
// (which is itself only used if the codec pushes a frame).
// Called by RadioModel::connectToRadio() after selectCodec() returns.
// ---------------------------------------------------------------------------
void P1RadioConnection::setIoBoard(IoBoardHl2* io)
{
    m_ioBoard = io;
    if (auto* hl2Codec = dynamic_cast<P1CodecHl2*>(m_codec.get())) {
        hl2Codec->setIoBoard(io);
    }
    // Non-HL2 boards: codec cast returns null — noop (no I2C support).
}

// ---------------------------------------------------------------------------
// setBandwidthMonitor — Phase 3P-E Task 3
//
// Wires the RadioModel-owned HermesLiteBandwidthMonitor into the connection.
// Called by RadioModel::connectToRadio() when m_caps->hasBandwidthMonitor.
// The pointer is non-owning — lifetime is managed by RadioModel.
// ---------------------------------------------------------------------------
void P1RadioConnection::setBandwidthMonitor(HermesLiteBandwidthMonitor* monitor)
{
    m_bwMonitor = monitor;
}

// ---------------------------------------------------------------------------
// setTxMicSource — Phase 3M-1c TX pump v3
//
// Wires the RadioModel-owned TxMicSource into the connection.  Called by
// RadioModel::connectToRadio() unconditionally (the source itself handles
// HL2 mic16-zero quirks via the existing PC mic-source-locked mechanism).
// The pointer is non-owning — lifetime is managed by RadioModel.
// ---------------------------------------------------------------------------
void P1RadioConnection::setTxMicSource(TxMicSource* src)
{
    // Caller contract: invoked on this connection's affinity thread.
    // Today that is the main thread, because RadioModel::connectToRadio
    // calls setTxMicSource at line 1764-1767 BEFORE the connection is
    // moved to its worker thread at line 1842.  The assignment + the
    // m_lastMicAt arming below therefore race-free with the connection-
    // thread reads in onWatchdogTick / parseEp6Frame mic16 extraction.
    // If a future refactor reorders these RadioModel calls, this
    // function will need atomic / mutex protection.
    m_txMicSource = src;

    // Stage-2 review fix I3: arm the LOS timer at attach time so the
    // 3 s zero-block injection (onWatchdogTick) fires even if the
    // radio never delivers a mic frame.  Without this, m_lastMicAt
    // stays default-constructed (invalid) and the mic-LOS guard at
    // P1RadioConnection.cpp:1628 short-circuits forever — the worker
    // would block on waitForBlock(INFINITE) with no recovery.
    //
    // Mirrors Thetis network.c:655-666 [v2.10.3.13] — WSA_WAIT_TIMEOUT
    // injects zero buffer via Inbound regardless of whether real
    // samples have been observed.  Setting m_lastMicAt = now here is
    // equivalent to Thetis's implicit "the timer started counting the
    // moment we attached", because the watchdog tick does
    // sinceMicMs = now - m_lastMicAt > kMicLosTimeoutMs.
    m_lastMicAt = QDateTime::currentDateTimeUtc();
}

// ---------------------------------------------------------------------------
// parseI2cResponse — Phase 3P-E Task 2
//
// Called from the instance parseEp6Frame() when incoming C&C status byte C0
// has bit 7 set, indicating an I2C read response frame (not normal status).
// Routes C1-C4 read data back into the IoBoardHl2 register mirror.
//
// Source: mi0bot networkproto1.c:478-493 [@c26a8a4]
// ---------------------------------------------------------------------------
void P1RadioConnection::parseI2cResponse(quint8 c0, quint8 c1, quint8 c2,
                                          quint8 c3, quint8 c4)
{
    if (!m_ioBoard) { return; }  // no IoBoard wired (non-HL2 board)

    // Persist all 4 response bytes plus the 7-bit returned address into the
    // IoBoardHl2 model. Upstream stores these in prn->i2c.read_data[0..3]
    // and sets ctrl_read_available = 1; we mirror both via
    // IoBoardHl2::applyI2cReadResponse(), which also emits a signal so the
    // HL2 I/O diagnostics page can reflect live hardware state instead of
    // defaults.
    //
    // Full returnedAddress → Register slot dispatch (so reads auto-populate
    // m_registers[]) is Phase 3P-E Task 3 state-machine work; for now
    // downstream consumers read the response via IoBoardHl2::lastI2cRead().
    //
    // Source: mi0bot networkproto1.c:478-493 [@c26a8a4]
    m_ioBoard->applyI2cReadResponse(c0, c1, c2, c3, c4);
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
    ctx.trxRelay       = m_trxRelay;
    ctx.p1MicBoost     = m_micBoost;
    ctx.p1LineIn       = m_lineIn;
    ctx.p1MicTipRing   = m_micTipRing;
    ctx.p1MicBias      = m_micBias;     // 3M-1b G.4
    ctx.p1LineInGain   = m_lineInGain;  // Task 2.1 of P1 full-parity epic
    ctx.p1UserDigOut   = m_userDigOut;  // Task 2.2 of P1 full-parity epic
    ctx.p1PuresignalRun = m_puresignalRun;  // Task 2.3 of P1 full-parity epic
    ctx.p1MicPTT       = m_micPTT;      // 3M-1b G.5
    ctx.duplex         = m_duplex;
    ctx.diversity      = m_diversity;
    ctx.antennaIdx     = m_antennaIdx;
    ctx.rxOnlyAnt      = m_rxOnlyAnt;
    ctx.rxOut          = m_rxOut;

    // Source OC byte from OcMatrix per current band + MOX state.  Falls
    // through to legacy m_ocOutput when matrix is unset (e.g. tests that
    // construct P1RadioConnection without a RadioModel).  Default state is
    // byte-identical: empty matrix → maskFor()==0 == m_ocOutput==0.
    // Phase 3P-D Task 3 — From Thetis HPSDR/Penny.cs:117-132 [@501e3f5]
    // setBandABitMask — OC mask derived per-band at transmit time.
    if (m_ocMatrix) {
        const Band currentBand = bandFromFrequency(static_cast<double>(m_rxFreqHz[0]));
        ctx.ocByte = m_ocMatrix->maskFor(currentBand, m_mox);
    } else {
        ctx.ocByte = m_ocOutput;
    }
    if (ctx.ocByte != m_lastOcByteLogged) {
        const int bandIdx = int(bandFromFrequency(static_cast<double>(m_rxFreqHz[0])));
        qDebug("HL2 ocByte=0x%02X band=%d mox=%d (matrix=%p)",
               ctx.ocByte, bandIdx, int(m_mox),
               static_cast<const void*>(m_ocMatrix));
        if (m_ioBoard) {
            emit m_ioBoard->currentOcByteChanged(ctx.ocByte, bandIdx, m_mox);
        }
        m_lastOcByteLogged = ctx.ocByte;
    }
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
    // HL2-only fields.  ptt_hang and tx_latency MUST be the mi0bot HL2
    // defaults (12 and 20) — without them the HL2 firmware drops the PTT
    // immediately on any TX-buffer underrun and the T/R relay flutters.
    // Source: mi0bot ChannelMaster/netInterface.c:1713-1714 [v2.10.3.14-beta1]
    //   prn->tx[i].tx_latency = 20;  // MI0BOT: HL2
    //   prn->tx[i].ptt_hang   = 12;  // MI0BOT: HL2
    if (m_hardwareProfile.model == HPSDRModel::HERMESLITE) {
        ctx.hl2PttHang   = 12;   // 5-bit field (bank 17 C3): 12 frames hang
        ctx.hl2TxLatency = 20;   // 7-bit field (bank 17 C4): 20 sample latency
    }
    return ctx;
}

// ---------------------------------------------------------------------------
// onReadyRead
//
// Drains incoming datagrams. For each 1032-byte ep6 frame, calls the static
// parseEp6Frame helper and emits iqDataReceived for each receiver.
// Source: networkproto1.c:319-415 MetisReadThreadMainLoop
//
// Upstream inline attribution in that range — preserved verbatim per
// CLAUDE.md §"Inline comment preservation — SHIP-BLOCKING":
//   :335  adc[0].adc_overload |= ControlBytesIn[1] & 0x01; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
//   :353  adc[0].adc_overload |= ControlBytesIn[1] & 1;    // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
//   :354  adc[1].adc_overload |= (ControlBytesIn[2] & 1) << 1; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
//   :355  adc[2].adc_overload |= (ControlBytesIn[3] & 1) << 2; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
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

            // Cancel the connect watchdog — first good ep6 means we reached
            // the radio successfully. Design §4.1.
            if (m_connectWatchdog && m_connectWatchdog->isActive()) {
                m_connectWatchdog->stop();
            }

            // Per-frame activity signal for TitleBar LED (throttled to 10 Hz
            // by the receiver). Design §4.1.
            emit frameReceived();

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

            // Phase 3P-E Task 3: record ep6 ingress bytes for bandwidth monitor.
            // Source: mi0bot bandwidth_monitor.c:74-78 bandwidth_monitor_in() [@c26a8a4]
            if (m_bwMonitor) { m_bwMonitor->recordEp6Bytes(data.size()); }

            // Shell-chrome sub-PR-2 B.1: record ingress bytes for ▼ Mbps readout.
            recordBytesReceived(static_cast<qint64>(data.size()));

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
// transitions to LinkLost and arms the reconnect timer (Phase 3Q-1).
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

    // Phase 3M-1c TX pump v3: mic-frame LOS injection.
    // Mirrors Thetis network.c:655-666 [v2.10.3.13] — when no UDP frame
    // has arrived for 3000 ms, push a zero block into the TX inbound ring
    // so the worker keeps ticking through silence (otherwise the worker
    // would block forever on waitForBlock(INFINITE)).  We use mono-sample
    // count == kBlockFrames so exactly one ring block is released.
    if (m_txMicSource != nullptr && m_lastMicAt.isValid()) {
        const qint64 sinceMicMs = m_lastMicAt.msecsTo(QDateTime::currentDateTimeUtc());
        if (sinceMicMs > kMicLosTimeoutMs) {
            std::array<float, TxMicSource::kBlockFrames> zeros{};
            m_txMicSource->inbound(zeros.data(), TxMicSource::kBlockFrames);
            // Reset the LOS timer so we inject one block per kMicLosTimeoutMs
            // window (not one block per watchdog tick).
            m_lastMicAt = QDateTime::currentDateTimeUtc();
        }
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
                                << "); transitioning to LinkLost and scheduling reconnect";
        m_watchdogTimer->stop();
        if (m_ep2PacerTimer) {
            m_ep2PacerTimer->stop();
        }
        m_reconnectedLogged = false;
        // Phase 3Q-1: ConnectionState::Error removed from the 5-value enum.
        // Watchdog timeout (was Connected, frames stopped) → LinkLost.
        setState(ConnectionState::LinkLost);
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
// Implements bounded retries: up to kMaxReconnectAttempts, then stays in LinkLost.
// Source: NereusSDR design doc §3.6.
// ---------------------------------------------------------------------------
void P1RadioConnection::onReconnectTimeout()
{
    // Guard: don't retry after an intentional disconnect.
    if (m_intentionalDisconnect) { return; }

    if (m_reconnectAttempts >= kMaxReconnectAttempts) {
        qCWarning(lcConnection) << "P1: Reconnect — bounded retries exhausted after"
                                << kMaxReconnectAttempts << "attempts; staying in LinkLost";
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
// onConnectTimeout — Phase 3Q Task 3
//
// Fires kConnectTimeoutMs after connectToRadio() if no first ep6 frame
// arrived. The radio is either unreachable (wrong IP, powered off, VPN
// blocking UDP) or too slow to reply within the budget.
// Emits connectFailed(Timeout, ...) so the UI can surface a typed message.
// onReadyRead() cancels this timer on the first valid ep6 frame.
// ---------------------------------------------------------------------------
void P1RadioConnection::onConnectTimeout()
{
    // Guard: intentional disconnect() races are harmless (disconnect() stops
    // the timer, but if both fire in the same event-loop turn, check here).
    if (m_intentionalDisconnect) { return; }

    // Guard: if we somehow received a first frame already (timer fired late),
    // do nothing.
    if (m_lastEp6At.isValid()) { return; }

    qCWarning(lcConnection) << "P1: Connect watchdog fired — no ep6 frame within"
                            << kConnectTimeoutMs << "ms; emitting connectFailed(Timeout)";

    emit connectFailed(ConnectFailure::Timeout,
                       QStringLiteral("No response from radio within %1 ms — "
                                      "check IP address, radio power, and network")
                           .arg(kConnectTimeoutMs));
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

    // RUNSTOP byte (pkt[3]) encodes three independent fields from the same byte:
    //   eth_data[0] = run         (1 = start IQ/mic stream)
    //   eth_data[1] = wide_spectrum (iqAndMic path sets 0x02)
    //   eth_data[7] = watchdog_disable (1 = disabled, 0 = enabled -- inverted)
    //
    // Source: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:200-203
    //   RUNSTOP: begin
    //     run_next = eth_data[0];
    //     wide_spectrum_next = eth_data[1];
    //     runstop_watchdog_valid = 1'b1;
    //   end
    //
    // Source: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400
    //   watchdog_disable <= eth_data[7]; // Bit 7 can be used to disable watchdog
    //
    // deskhpsdr reference (deskhpsdr/src/old_protocol.c:3811 [@120188f]):
    //   buffer[3] = command;  // 0x01 start -- bit 7 = 0 implicitly (watchdog enabled)
    //   deskhpsdr never sets bit 7; watchdog is always enabled there.
    //
    // Thetis (setup.cs:17986 [v2.10.3.13]):
    //   NetworkIO.SetWatchdogTimer(Convert.ToInt32(chkNetworkWDT.Checked));
    //   When checked (enabled): passes 1 -> bit 7 = 0 (not disabled).
    const quint8 runBits     = iqAndMic ? quint8(0x02) : quint8(0x01);
    // From Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400 [@7472bd1]:
    //   watchdog_disable <= eth_data[7]; -- 1=disabled, 0=enabled (inverted)
    const quint8 watchdogBit = m_watchdogEnabled ? quint8(0x00) : quint8(0x80);
    pkt[3] = static_cast<char>(runBits | watchdogBit);

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

    // Stop packet (run = 0).  Watchdog bit still emitted for consistency:
    //   eth_data[0] = 0 (stop)
    //   eth_data[7] = watchdog_disable (inverted -- see sendMetisStart for full cite)
    //
    // Source: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400
    //   watchdog_disable <= eth_data[7]; // Bit 7 can be used to disable watchdog
    //
    // deskhpsdr reference (deskhpsdr/src/old_protocol.c:3811 [@120188f]):
    //   buffer[3] = command;  // 0x00 stop -- bit 7 = 0 implicitly
    //   deskhpsdr doesn't set bit 7 on stop either; NereusSDR emits it
    //   explicitly so the watchdog state is preserved if the radio re-reads
    //   the last RUNSTOP byte on reconnect.
    const quint8 watchdogBit = m_watchdogEnabled ? quint8(0x00) : quint8(0x80);
    pkt[3] = static_cast<char>(watchdogBit); // run = 0; watchdog bit set if disabled

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

    // Phase 3P-D follow-up: drive the bank ceiling from the active codec's
    // maxBank() so HL2 (18) and AnvelinaPro3 (17) both emit their full bank
    // range. Standard = 16. The legacy compose path (m_codec == nullptr under
    // NEREUS_USE_LEGACY_P1_CODEC=1) retains the pre-refactor model-keyed
    // constant to preserve the regression-freeze byte-identical guarantee.
    const int maxBank = m_codec
        ? m_codec->maxBank()
        : ((m_hardwareProfile.model == HPSDRModel::ANVELINAPRO3) ? 17 : 16);

    quint8 frame[1032];
    memset(frame, 0, sizeof(frame));

    // EP2 header (networkproto1.c:223-230)
    frame[0] = 0xEF; frame[1] = 0xFE; frame[2] = 0x01; frame[3] = 0x02;
    const quint32 seq = m_epSendSeq++;
    frame[4] = static_cast<quint8>((seq >> 24) & 0xFF);
    frame[5] = static_cast<quint8>((seq >> 16) & 0xFF);
    frame[6] = static_cast<quint8>((seq >>  8) & 0xFF);
    frame[7] = static_cast<quint8>( seq        & 0xFF);

    // 3M-1a E.3: if setMox() requested a bank-0 flush, reset the round-robin
    // to 0 so this frame carries the MOX bit within ≤1 frame of the call.
    // Source: deskhpsdr/src/old_protocol.c:3595-3599 [@120188f] — the reference
    // implementation does not defer MOX; it sets the bit on the very next frame.
    // 3M-1a E.4: if setTrxRelay() requested a bank-10 flush, jump to bank 10
    // so the T/R relay bit (C3 bit 7) lands within ≤1 frame of the call.
    // 3M-1b G.3: if setMicTipRing() requested a bank-11 flush, jump to bank 11
    // so the mic_trs bit (C1 bit 4) lands within ≤1 frame of the call.
    // Priority: bank 0 > bank 10 > bank 11.  Losing flags are preserved and
    // fire on the following frame (same pattern as bank 0 vs bank 10).
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f].
    // Source: Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13].
    if (m_forceBank0Next) {
        m_ccRoundRobinIdx = 0;
        m_forceBank0Next  = false;
    } else if (m_forceBank10Next) {
        m_ccRoundRobinIdx = 10;
        m_forceBank10Next = false;
    } else if (m_forceBank11Next) {
        m_ccRoundRobinIdx = 11;
        m_forceBank11Next = false;
    }

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

    // 3M-1a E.2: fill the two 504-byte TX I/Q zones from the ring buffer.
    // Each zone holds 63 samples × 8 bytes = 504 bytes.
    // From deskhpsdr/src/old_protocol.c:545-549 [@120188f]:
    //   memcpy(output_buffer + 8, &TXRINGBUF[out],       504); ozy_send_buffer();
    //   memcpy(output_buffer + 8, &TXRINGBUF[out + 504], 504); ozy_send_buffer();
    // frame[16..519]   = subframe 0 TX data zone (after 3-byte sync + 5-byte C&C)
    // frame[528..1031] = subframe 1 TX data zone (after 3-byte sync + 5-byte C&C)
    fillTxZone(frame + 16);
    fillTxZone(frame + 528);

    QByteArray pkt(reinterpret_cast<const char*>(frame), 1032);
    m_socket->writeDatagram(pkt, m_radioInfo.address, m_radioInfo.port);

    // Phase 3P-E Task 3: record ep2 egress bytes for bandwidth monitor.
    // Source: mi0bot bandwidth_monitor.c:80-84 bandwidth_monitor_out() [@c26a8a4]
    if (m_bwMonitor) { m_bwMonitor->recordEp2Bytes(pkt.size()); }

    // Shell-chrome sub-PR-2 B.1: record egress bytes for ▲ Mbps readout.
    recordBytesSent(static_cast<qint64>(pkt.size()));

    // Shell-chrome sub-PR-2 B.2: bracket C&C round-trip for ping RTT.
    // P1 EP2 → EP6 is the existing once-per-frame (380.95 pps) exchange.
    // We note send here; receive is noted in parseEp6Frame on the success path.
    notePingSent();
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
// Upstream inline attribution preserved verbatim (networkproto1.c:335/353/354/355):
//   `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
// ---------------------------------------------------------------------------
void P1RadioConnection::parseEp6Frame(const QByteArray& pkt)
{
    if (pkt.size() != 1032) { return; }

    std::vector<std::vector<float>> perRx;
    std::vector<float> micSamples;
    const auto* frame = reinterpret_cast<const quint8*>(pkt.constData());

    // Phase 3M-1c TX pump v3: extract mic16 byte zone alongside RX I/Q so
    // the network thread is the cadence source for TX (matches Thetis
    // network.c:655-666 [v2.10.3.13] which calls Inbound() for both rx and
    // mic in lockstep with EP6 frame arrival).
    std::vector<float>* micOut = (m_txMicSource != nullptr) ? &micSamples : nullptr;
    if (!P1RadioConnection::parseEp6Frame(frame, m_activeRxCount, perRx, micOut)) {
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

    // Cache each subframe's C0 byte for the mic_ptt extraction below.
    // ADC overflow does NOT live in C0 — per Thetis networkproto1.c:332-355
    // [v2.10.3.13] it lives in C1 bit 0 of case-0x00 frames and in
    // C1/C2/C3 bit 0 of case-0x20 frames.  See the C0-type switch lower
    // in this function.
    //
    // Phase 3P-E Task 2: C0 bit 7 = I2C response frame (HL2 only).
    // Source: mi0bot networkproto1.c:478-493 [@c26a8a4]
    const quint8 c0_sub0 = frame[11];
    const quint8 c0_sub1 = frame[523];

    // Check each subframe's C0 for I2C response marker (bit 7).
    // Source: mi0bot networkproto1.c:478-480 [@c26a8a4]
    for (int sub = 0; sub < 2; ++sub) {
        const int base = 8 + sub * 512;  // sync bytes at base+0..2, C&C at base+3..7
        const quint8 c0 = frame[base + 3];
        if (c0 & 0x80) {
            // I2C response frame: C1-C4 = read_data[0-3]
            // Source: mi0bot networkproto1.c:480-492 [@c26a8a4]
            parseI2cResponse(c0, frame[base + 4], frame[base + 5],
                             frame[base + 6], frame[base + 7]);
        }
    }

    // H.5: mic_ptt extraction — P1 status frame C0 bit 0 (PTT from radio).
    // From Thetis networkproto1.c:329 [v2.10.3.13]:
    //   prn->ptt_in = ControlBytesIn[0] & 0x1;
    // + console.cs:25426 [v2.10.3.13]:
    //   bool mic_ptt = (dotdashptt & 0x01) != 0; // PTT from radio
    //
    // C0 is ControlBytesIn[0]; bit 0 is ptt_in.  Both sub-frames carry the
    // same instantaneous PTT state; OR them to produce the frame-level value
    // (matches Thetis nativeGetDotDashPTT() which reads a single prn->ptt_in
    // accumulated across all sub-frame writes).
    //
    // Emitted unconditionally each frame: MoxController::onMicPttFromRadio
    // is idempotent, so repeated false→false calls are harmless.
    const bool micPtt = ((c0_sub0 & 0x01) != 0) || ((c0_sub1 & 0x01) != 0);
    emit micPttFromRadio(micPtt);

    // Phase 3P-H Task 4: PA telemetry — extract raw 16-bit ADC counts from
    // each subframe's C&C status bytes.  Per-board scaling lives in RadioModel
    // (console.cs computeAlexFwdPower / computeRefPower / convertToVolts /
    // convertToAmps), because bridge_volt / refvoltage / adc_cal_offset
    // depend on HardwareSpecific.Model.
    //
    // From Thetis networkproto1.c:332-356 [@501e3f5]
    //   case 0x00: // C0 0000 0000
    //       prn->adc[0].adc_overload = prn->adc[0].adc_overload || ControlBytesIn[1] & 0x01; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
    //   case 0x08: // C0 0000 1xxx
    //       prn->tx[0].exciter_power = ((C1 << 8) & 0xff00) | (C2 & 0xff);  // (AIN5) drive power
    //       prn->tx[0].fwd_power     = ((C3 << 8) & 0xff00) | (C4 & 0xff);  // (AIN1) PA coupler
    //   case 0x10: // C0 0001 0xxx
    //       prn->tx[0].rev_power     = ((C1 << 8) & 0xff00) | (C2 & 0xff);  // (AIN2) PA reverse power
    //       prn->user_adc0           = ((C3 << 8) & 0xff00) | (C4 & 0xff);  // AIN3 MKII PA Volts
    //   case 0x18: // C0 0001 1xxx
    //       prn->user_adc1           = ((C1 << 8) & 0xff00) | (C2 & 0xff);  // AIN4 MKII PA Amps
    //       prn->supply_volts        = ((C3 << 8) & 0xff00) | (C4 & 0xff);  // AIN6 Hermes Volts
    //   case 0x20: // C0 0010 0xxx
    //       prn->adc[0].adc_overload = prn->adc[0].adc_overload || ControlBytesIn[1] & 1;        // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
    //       prn->adc[1].adc_overload = prn->adc[1].adc_overload || (ControlBytesIn[2] & 1) << 1; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
    //       prn->adc[2].adc_overload = prn->adc[2].adc_overload || (ControlBytesIn[3] & 1) << 2; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
    //
    // We accumulate the latest value of each field across this frame's two
    // subframes and emit one paTelemetryUpdated() with the latched values.
    // Last-writer-wins matches Thetis (which mutates prn->* in place each subframe).
    bool telemetryDirty = false;
    for (int sub = 0; sub < 2; ++sub) {
        const int base   = 8 + sub * 512;
        const quint8 c0  = frame[base + 3];
        const quint8 c1  = frame[base + 4];
        const quint8 c2  = frame[base + 5];
        const quint8 c3  = frame[base + 6];
        const quint8 c4  = frame[base + 7];
        if (c0 & 0x80) {
            // I2C response — already handled above; not a telemetry frame.
            continue;
        }
        // Source: networkproto1.c:332 — switch (ControlBytesIn[0] & 0xf8)
        // Cases 0x00/0x20 carry ADC-overload bits (one per ADC); the
        // `//[2.10.3.13]MW0LGE only cleared by getAndResetADC_Overload(),
        // or'ed with existing state` inline attributions are preserved
        // verbatim within each case body below.  In NereusSDR the SAC
        // hysteresis state machine (StepAttenuatorController) plays the
        // role of `getAndResetADC_Overload()` — it OR-accumulates every
        // adcOverflow() emission until its 100 ms tick consumes them.
        switch (c0 & 0xF8) {
        case 0x00: {
            // Issue #176 fix — ADC0 overload reported in C1 bit 0.
            // From Thetis networkproto1.c:335 [v2.10.3.13]:
            //   prn->adc[0].adc_overload = prn->adc[0].adc_overload || ControlBytesIn[1] & 0x01;
            //   // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
            if (c1 & 0x01) {
                emit adcOverflow(0);
            }
            break;
        }
        case 0x08: {
            // From Thetis networkproto1.c:339 [@501e3f5] — exciter_power AIN5
            const quint16 exciter = static_cast<quint16>((c1 << 8) | c2);
            // From Thetis networkproto1.c:340 [@501e3f5] — fwd_power AIN1
            const quint16 fwd     = static_cast<quint16>((c3 << 8) | c4);
            m_paExciterRaw = exciter;
            m_paFwdRaw     = fwd;
            telemetryDirty = true;
            break;
        }
        case 0x10: {
            // From Thetis networkproto1.c:344 [@501e3f5] — rev_power AIN2
            const quint16 rev      = static_cast<quint16>((c1 << 8) | c2);
            // From Thetis networkproto1.c:346 [@501e3f5] — user_adc0 AIN3 MKII PA Volts
            const quint16 userAdc0 = static_cast<quint16>((c3 << 8) | c4);
            m_paRevRaw      = rev;
            m_paUserAdc0Raw = userAdc0;
            telemetryDirty  = true;
            // Shell-chrome sub-PR-2 B.3: emit userAdc0Changed for MKII-class boards.
            // Matches gate in RadioModel scalePaVolts() [RadioModel.cpp:402-417].
            // From Thetis networkproto1.c:346 [@501e3f5] — user_adc0 AIN3 MKII PA Volts
            switch (m_hardwareProfile.model) {
            case HPSDRModel::ORIONMKII:
            case HPSDRModel::ANAN8000D:
            case HPSDRModel::ANAN7000D:
            case HPSDRModel::ANAN_G2:
            case HPSDRModel::ANAN_G2_1K:
            case HPSDRModel::ANVELINAPRO3:
                handleUserAdc0Raw(userAdc0);
                break;
            default:
                break;
            }
            break;
        }
        case 0x18: {
            // From Thetis networkproto1.c:349 [@501e3f5] — user_adc1 AIN4 MKII PA Amps
            // Upstream cite :353/:354/:355 (case 0x20) carry the MW0LGE ADC
            // overload comments — see comment block above the switch.
            const quint16 userAdc1 = static_cast<quint16>((c1 << 8) | c2);
            // From Thetis networkproto1.c:350 [@501e3f5] — supply_volts AIN6 Hermes Volts //[2.10.3.13]MW0LGE (nearby upstream cases)
            const quint16 supply   = static_cast<quint16>((c3 << 8) | c4);
            m_paUserAdc1Raw = userAdc1;
            m_paSupplyRaw   = supply;
            telemetryDirty  = true;
            // Shell-chrome sub-PR-2 B.3: emit supplyVoltsChanged for all radios.
            // From Thetis networkproto1.c:350 [@501e3f5] — supply_volts AIN6 Hermes Volts //[2.10.3.13]MW0LGE
            handleSupplyRaw(supply);
            break;
        }
        case 0x20: {
            // Issue #176 fix — multi-ADC overload report (Orion / ANAN-8000 class).
            // From Thetis networkproto1.c:353-355 [v2.10.3.13]:
            //   prn->adc[0].adc_overload = prn->adc[0].adc_overload || ControlBytesIn[1] & 1;        // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
            //   prn->adc[1].adc_overload = prn->adc[1].adc_overload || (ControlBytesIn[2] & 1) << 1; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
            //   prn->adc[2].adc_overload = prn->adc[2].adc_overload || (ControlBytesIn[3] & 1) << 2; // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
            if (c1 & 0x01) { emit adcOverflow(0); }
            if (c2 & 0x01) { emit adcOverflow(1); }
            if (c3 & 0x01) { emit adcOverflow(2); }
            break;
        }
        default:
            break;
        }
    }
    if (telemetryDirty) {
        emit paTelemetryUpdated(m_paFwdRaw, m_paRevRaw, m_paExciterRaw,
                                m_paUserAdc0Raw, m_paUserAdc1Raw, m_paSupplyRaw);
    }

    // Shell-chrome sub-PR-2 B.2: complete the ping RTT measurement.
    // Each valid EP6 frame constitutes the inbound leg of the EP2→EP6 exchange.
    notePingReceived();

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

    // Phase 3M-1c TX pump v3: dispatch the extracted mic16 samples to
    // TxMicSource as the cadence source for TxWorkerThread.
    //
    // The radio embeds one mic sample per I/Q sample group in EP6 frames,
    // so mic arrives at the operating sample rate (192 kHz at sampleRate=
    // 192000).  TxMicSource and TxChannel WDSP both expect 48 kHz mic, so
    // we decimate by mic_decimation_factor before dispatch.
    //
    // Mirrors Thetis networkproto1.c:391-410 [v2.10.3.14] (the loop body
    // that gates each Inbound() call by mic_decimation_count).  Without
    // this, at 192 kHz the worker semaphore-wakes 4× per block (3000/sec
    // instead of 750/sec), CPU thrashes, the SPSC TX I/Q ring overflows
    // ~3000/sec during MOX-on, and the EP2 pacer is starved into bursty
    // delivery — observed as rapid HL2 T/R relay clicking on bench.
    if (m_txMicSource != nullptr && !micSamples.empty()) {
        std::vector<float> decimated;
        decimateMicSamples(micSamples.data(), static_cast<int>(micSamples.size()),
                           m_micDecimationFactor, m_micDecimationCount, decimated);
        if (!decimated.empty()) {
            m_txMicSource->inbound(decimated.data(),
                                   static_cast<int>(decimated.size()));
            m_lastMicAt = QDateTime::currentDateTimeUtc();
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

    case 10: // TX drive, mic, Alex HPF/LPF, T/R relay (networkproto1.c:578-591)
        // C3 bit 7 (0x80) = Alex T/R relay DISABLED.  Inverted vs MOX.
        // Write 0x80 only when relay should be disengaged (PA bypass / RX-only).
        // m_trxRelay: true  = relay engaged (normal TX path) → bit 7 = 0;
        //             false = relay disengaged (PA bypass)   → bit 7 = 1.
        // From deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]:
        //   if (txband->disablePA || !pa_enabled)
        //       output_buffer[C3] |= 0x80; // disable Alex T/R relay
        // BUG FIX (3M-1a E.4): prior code wrote (m_paEnabled ? 0x80 : 0), which
        // is INVERTED — it asserted "disable" when PA was enabled.  Latent from
        // 3M-0; never surfaced because no TX I/Q was live on EP2 before E.4.
        out[0] = C0base | 0x12;
        out[1] = static_cast<quint8>(m_txDrive & 0xFF);
        // C2: mic_boost → bit 0 (0x01); line_in → bit 1 (0x02); bit 6 always set per upstream default.
        // From Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
        //   C2 = ((prn->mic.mic_boost & 1) | ((prn->mic.line_in & 1) << 1) | ... | 0b01000000) & 0x7f;
        out[2] = static_cast<quint8>((m_micBoost ? 0x01 : 0x00) | (m_lineIn ? 0x02 : 0x00) | 0x40); // 3M-1b G.1+G.2
        out[3] = m_alexHpfBits | (m_trxRelay ? 0x00 : 0x80); // 3M-1a E.4
        out[4] = m_alexLpfBits;
        return;

    case 11: // Preamp control (networkproto1.c:593-601)
        out[0] = C0base | 0x14;
        // C1: preamp bits 0-3 (bit 3 = rx0 again, Thetis quirk) + mic_trs bit 4
        //     + mic_bias bit 5 + mic_ptt bit 6 (INVERTED — LEGACY ONLY).
        // mic_trs polarity inversion: 1 = tip is BIAS/PTT → write !m_micTipRing.
        // mic_bias polarity: 1 = bias on (no inversion) → write m_micBias.
        // mic_ptt polarity inversion: 1 = PTT DISABLED on wire → write !m_micPTT.
        // TODO(legacy-codec-cleanup): same mic_ptt inversion bug as the codec
        // path; flip when re-enabling NEREUS_USE_LEGACY_P1_CODEC=1.  P1CodecStandard
        // (the production path) was flipped to direct polarity in the same
        // commit that introduced this TODO; this legacy compose path is
        // kept inverted only because it is unreachable in shipped builds
        // (gated by the env var).  See P1CodecStandard.cpp:bank11 for the
        // direct-polarity wire formula and Thetis cite.
        // From Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13]
        //   C1 = ... | ((prn->mic.mic_trs & 1) << 4) | ((prn->mic.mic_bias & 1) << 5)
        //           | ((prn->mic.mic_ptt & 1) << 6);
        out[1] = static_cast<quint8>(
                   (m_rxPreamp[0] ? 0x01 : 0)
                 | (m_rxPreamp[1] ? 0x02 : 0)
                 | (m_rxPreamp[2] ? 0x04 : 0)
                 | (m_rxPreamp[0] ? 0x08 : 0)        // bit3 = rx0 again (Thetis quirk)
                 | (!m_micTipRing ? 0x10 : 0x00)      // 3M-1b G.3 — mic_trs (inverted)
                 | (m_micBias    ? 0x20 : 0x00)       // 3M-1b G.4 — mic_bias (no inversion)
                 | (!m_micPTT    ? 0x40 : 0x00));     // 3M-1b G.5 — mic_ptt (INVERTED — LEGACY ONLY; see TODO above)
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

    // Phase 3P-E Task 2: HL2 I2C intercept — when IoBoardHl2 has pending
    // I2C transactions, the next C&C frame carries I2C TLV bytes instead of
    // the normal bank payload.
    // Source: mi0bot networkproto1.c:898-943 [@c26a8a4]
    if (m_codec->usesI2cIntercept()) {
        // const_cast: tryComposeI2cFrame mutates the queue (dequeues txn),
        // but composeCcForBank is const because the rest of compose is pure.
        // Documented exception: only the I2C queue pointer is mutated, not
        // the codec or connection state itself.
        auto* hl2Codec = const_cast<P1CodecHl2*>(
            dynamic_cast<const P1CodecHl2*>(m_codec.get()));
        if (hl2Codec && hl2Codec->tryComposeI2cFrame(out, m_mox)) {
            return;  // I2C frame written; skip normal bank compose
        }
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
// m_caps->hasIoBoardHl2 is true; also reachable from the user-initiated
// "Probe" button on Setup → Hardware → HL2 I/O Board.
//
// Enqueues 3 I2C read transactions on the IoBoardHl2 queue:
//   1. bus=1, addr=0x41, reg=0   → HW version   (sets m_hardwareVersion +
//                                                setDetected if version==0xF1)
//   2. bus=1, addr=0x1d, reg=9   → FW major     (REG_FIRMWARE_MAJOR)
//   3. bus=1, addr=0x1d, reg=10  → FW minor     (REG_FIRMWARE_MINOR)
//
// The wire encoder (P1CodecHl2::tryComposeI2cFrame) drains the queue one
// transaction per ep2 frame, pushing each read into IoBoardHl2's pending-read
// FIFO. Inbound responses (parseI2cResponse → applyI2cReadResponse) pop the
// oldest pending-read and steer the bytes to the right destination
// (hardwareVersion or m_registers[]).
//
// Source: mi0bot IoBoardHl2.cs:129-145 readRequest() [@c26a8a4]
//         mi0bot ChannelMaster/netInterface.c:1471-1499 I2CReadInitiate [@c26a8a4]
// ---------------------------------------------------------------------------
void P1RadioConnection::requestIoBoardProbe()
{
    hl2SendIoBoardInit();
}

void P1RadioConnection::hl2SendIoBoardInit()
{
    if (!m_caps || !m_caps->hasIoBoardHl2) { return; }
    if (!m_ioBoard) {
        qCWarning(lcConnection) << "HL2: I/O board init — m_ioBoard not wired; skipping probe";
        return;
    }

    // mi0bot enforces ONE outstanding read at a time.  I2CReadInitiate refuses
    // when in_index != out_index (netInterface.c:1478), and the C# driver
    // busy-waits for each response before issuing the next (console.cs:25796-
    // 25808).  We mirror that protocol via a step machine driven off the
    // IoBoardHl2::i2cReadResponseReceived signal.  Source: [@c26a8a4]
    m_hl2ProbeStep = Hl2ProbeStep::Idle;
    if (!m_hl2ProbeWired) {
        // Gate the step machine on (retAddr, retSubAddr) matching the
        // expected (deviceAddr, register) for the current step.  Without
        // this gate, unrelated I2C reads (e.g. from the new Hl2OptionsTab
        // manual R/W tool, or NEREUS_HL2_I2C_SCAN traffic) would advance
        // the probe out of order — false aborts or misleading "init
        // complete" before the intended registers were probed.  Codex P2
        // on PR #157.
        connect(m_ioBoard, &IoBoardHl2::i2cReadResponseReceived, this,
                [this](quint8 retAddr, quint8 retSubAddr,
                       quint8, quint8, quint8, quint8) {
                    hl2ProbeAdvance(retAddr, retSubAddr);
                });
        m_hl2ProbeWired = true;
    }
    // Initial dispatch: bootstrap from Idle.  No retAddr/retSubAddr
    // because no read has fired yet — Idle ignores them.
    hl2ProbeAdvance(/*retAddr=*/0, /*retSubAddr=*/0);

    if (qEnvironmentVariableIntValue("NEREUS_HL2_I2C_SCAN") != 0) {
        requestI2cBusScan();
    }
}

void P1RadioConnection::hl2ProbeAdvance(quint8 retAddr, quint8 retSubAddr)
{
    if (!m_caps || !m_caps->hasIoBoardHl2 || !m_ioBoard) { return; }
    using Reg = IoBoardHl2::Register;

    auto enqueueRead = [this](quint8 deviceAddr, quint8 subAddr) {
        IoBoardHl2::I2cTxn txn;
        txn.bus = IoBoardHl2::kI2cBusIndex;
        txn.address = deviceAddr;
        txn.control = subAddr;
        txn.writeData = 0x00;
        txn.isRead = true;
        txn.needsResponse = true;
        m_ioBoard->enqueueI2c(txn);
    };
    auto enqueueWrite = [this](quint8 deviceAddr, quint8 subAddr, quint8 data) {
        IoBoardHl2::I2cTxn txn;
        txn.bus = IoBoardHl2::kI2cBusIndex;
        txn.address = deviceAddr;
        txn.control = subAddr;
        txn.writeData = data;
        txn.isRead = false;
        txn.needsResponse = false;
        m_ioBoard->enqueueI2c(txn);
    };

    // Helper: did the just-received response match the read this step
    // is waiting on?  If not, the response belongs to some other consumer
    // (manual R/W tool, bus scan) and the step machine must NOT advance.
    auto matches = [retAddr, retSubAddr](quint8 wantAddr, quint8 wantSub) {
        return retAddr == wantAddr && retSubAddr == wantSub;
    };

    switch (m_hl2ProbeStep) {
        case Hl2ProbeStep::Idle:
            qCInfo(lcConnection) << "HL2: probe step 1 — reading HW version";
            enqueueRead(IoBoardHl2::kI2cAddrHwVersion, 0);
            m_hl2ProbeStep = Hl2ProbeStep::WaitingForHwVersion;
            return;

        case Hl2ProbeStep::WaitingForHwVersion:
            if (!matches(IoBoardHl2::kI2cAddrHwVersion, 0)) {
                // Stray response (likely from manual R/W tool) — ignore.
                return;
            }
            // Response landed.  IoBoardHl2 has already routed C4 → setHardwareVersion()
            // and called setDetected() if it matched 0xF1.  If not detected,
            // abort the probe — mi0bot does the same (console.cs:25810).
            if (!m_ioBoard->isDetected()) {
                qCInfo(lcConnection) << "HL2: HW version =" << Qt::hex
                                     << m_ioBoard->hardwareVersion()
                                     << "(expected 0xF1) — I/O board absent, probe aborted";
                m_hl2ProbeStep = Hl2ProbeStep::Done;
                return;
            }
            qCInfo(lcConnection) << "HL2: HW version 0xF1 confirmed — reading FW major";
            enqueueRead(IoBoardHl2::kI2cAddrGeneral,
                        static_cast<quint8>(Reg::REG_FIRMWARE_MAJOR));
            m_hl2ProbeStep = Hl2ProbeStep::WaitingForFwMajor;
            return;

        case Hl2ProbeStep::WaitingForFwMajor:
            if (!matches(IoBoardHl2::kI2cAddrGeneral,
                         static_cast<quint8>(Reg::REG_FIRMWARE_MAJOR))) {
                return;
            }
            qCInfo(lcConnection) << "HL2: FW major received — reading FW minor";
            enqueueRead(IoBoardHl2::kI2cAddrGeneral,
                        static_cast<quint8>(Reg::REG_FIRMWARE_MINOR));
            m_hl2ProbeStep = Hl2ProbeStep::WaitingForFwMinor;
            return;

        case Hl2ProbeStep::WaitingForFwMinor:
            if (!matches(IoBoardHl2::kI2cAddrGeneral,
                         static_cast<quint8>(Reg::REG_FIRMWARE_MINOR))) {
                return;
            }
            // Mi0bot writes REG_CONTROL=1 to enable the board after version check.
            // Source: console.cs:25831 — `ioBoard.writeRequest(REG_CONTROL, 1);`
            qCInfo(lcConnection) << "HL2: FW minor received — writing REG_CONTROL=1 (init)";
            enqueueWrite(IoBoardHl2::kI2cAddrGeneral,
                         static_cast<quint8>(Reg::REG_CONTROL), 1);
            // Writes don't have responses we wait for; advance immediately.
            m_hl2ProbeStep = Hl2ProbeStep::Done;
            qCInfo(lcConnection) << "HL2: I/O board init complete";
            return;

        case Hl2ProbeStep::WaitingForControlInitAck:
        case Hl2ProbeStep::Done:
            return;
    }
}

void P1RadioConnection::requestI2cBusScan()
{
    if (!m_caps || !m_caps->hasIoBoardHl2 || !m_ioBoard) {
        qCWarning(lcConnection) << "HL2: I2C bus scan — caps/ioboard not wired; skipping";
        return;
    }
    // Curated probe set — 32 slot queue, common HL2 companion-board addresses:
    //   0x20-0x27 = MCP23008/MCP23017 GPIO expanders (HL2 smallio uses 0x20)
    //   0x1D, 0x41 = mi0bot custom IoBoardHl2 (general regs / HW version)
    //   0x40-0x4F = INA219, PCA9685, TMP102 sensor range
    //   0x68-0x6F = DS3231 RTC, MPU6050, etc.
    static const std::array<quint8, 14> kProbeAddrs = {{
        0x1D,
        0x20, 0x21, 0x22, 0x23,
        0x40, 0x41, 0x42, 0x44,
        0x48, 0x4A, 0x4C,
        0x50, 0x68,
    }};
    int enqueued = 0;
    for (quint8 bus : {quint8(0), quint8(1)}) {
        for (quint8 addr : kProbeAddrs) {
            IoBoardHl2::I2cTxn txn;
            txn.bus           = bus;
            txn.address       = addr;
            txn.control       = 0;
            txn.writeData     = 0;
            txn.isRead        = true;
            txn.needsResponse = true;
            if (m_ioBoard->enqueueI2c(txn)) { ++enqueued; }
            else { goto done; }  // queue full
        }
    }
done:
    qCInfo(lcConnection) << "HL2: I2C bus scan — enqueued" << enqueued
                         << "address probes across bus 0 + bus 1";
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

    // Phase 3P-E Task 3: delegate to HermesLiteBandwidthMonitor when wired.
    // The monitor records ep6/ep2 bytes via recordEp6Bytes()/recordEp2Bytes()
    // in onReadyRead()/sendCommandFrame() respectively, then tick() here runs
    // the upstream compute_bps() algorithm (mi0bot bandwidth_monitor.c:86-113
    // [@c26a8a4]) and the NereusSDR throttle-detection layer.
    if (m_bwMonitor) {
        m_bwMonitor->tick();
        // Mirror throttle state into the legacy m_hl2Throttled flag so that
        // hl2IsThrottled() and the test seam hl2ThrottledForTest() remain valid.
        const bool nowThrottled = m_bwMonitor->isThrottled();
        if (nowThrottled && !m_hl2Throttled) {
            m_hl2Throttled = true;
            m_hl2LastThrottleTick = QDateTime::currentDateTimeUtc();
            qCWarning(lcConnection) << "HL2: LAN PHY throttle detected via byte-rate monitor —"
                                    << "ep6 ingress silent for"
                                    << HermesLiteBandwidthMonitor::kThrottleTickThreshold
                                    << "watchdog ticks;"
                                    << "throttle events:" << m_bwMonitor->throttleEventCount();
            emit errorOccurred(RadioConnectionError::None,
                               QStringLiteral("HL2 LAN throttled — pausing ep2"));
        } else if (!nowThrottled && m_hl2Throttled) {
            m_hl2Throttled = false;
            qCInfo(lcConnection) << "HL2: LAN throttle cleared — ep6 stream resumed";
        }
        return;
    }

    // Fallback: legacy sequence-gap heuristic used when m_bwMonitor is not wired
    // (non-HL2 board or test seam without RadioModel).
    // Source: NereusSDR design — sequence-gap proxy for byte-rate throttle detect.
    // The upstream bandwidth_monitor.{c,h} (MW0LGE [@c26a8a4]) is a byte-rate
    // telemetry helper; throttle detection is a NereusSDR addition.
    static constexpr int kBwThrottleGapCount = 3;  // NereusSDR heuristic
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
            qCWarning(lcConnection) << "HL2: LAN PHY throttle detected (seq-gap fallback) —"
                                    << "ep6 sequence stalled for"
                                    << m_hl2ThrottleCount << "watchdog ticks;"
                                    << "pausing ep2 command frames";
            emit errorOccurred(RadioConnectionError::None,
                               QStringLiteral("HL2 LAN throttled — pausing ep2"));
        }
    } else {
        // Sequence advanced — clear throttle.
        if (m_hl2Throttled) {
            qCInfo(lcConnection) << "HL2: LAN throttle cleared (seq-gap fallback) — ep6 stream resumed";
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
// each 512 bytes.
// Upstream inline attribution (networkproto1.c:335/353/354/355, preserved
// verbatim): `// only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE`
// Within each subframe (bptr = FPGAReadBufp + 512*frame, which
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
    // Delegate to the mic-aware overload with a null mic output (back-compat
    // for callers / tests that don't care about the mic16 byte zone).
    return parseEp6Frame(frame, numRx, perRx, /*micOut=*/nullptr);
}

bool P1RadioConnection::parseEp6Frame(const quint8 frame[1032],
                                       int numRx,
                                       std::vector<std::vector<float>>& perRx,
                                       std::vector<float>* micOut) noexcept
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

    if (micOut != nullptr) {
        micOut->clear();
        micOut->reserve(static_cast<size_t>(samplesPerSubframe * 2));
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
            // Mic16 bytes at offset sampleStart + s*slotBytes + numRx*6.
            // From Thetis networkproto1.c:401-404 [v2.10.3.13] — extracts a
            // 16-bit big-endian mic sample from the slot's last two bytes.
            // We extract every mic sample at the operating sample rate
            // (one per I/Q sample group) — decimation to 48 kHz happens in
            // the caller via P1RadioConnection::decimateMicSamples (see
            // dispatch site above).  Thetis applies mic_decimation_factor
            // inline at networkproto1.c:391-410 [v2.10.3.14]; we split the
            // concerns so the static parser stays pure.
            //   prn->TxReadBufp[2 * mic_sample_count + 0] = const_1_div_2147483648_ *
            //       (double)(bptr[k + 0] << 24 |
            //                bptr[k + 1] << 16);
            //   prn->TxReadBufp[2 * mic_sample_count + 1] = 0.0;
            // Equivalent to: (int16)(bptr[k]<<8 | bptr[k+1]) / 32768.0  — both
            // give the same float in [-1, +1].  We use the int16/32768 form
            // because the bytes are signed 16-bit big-endian.
            if (micOut != nullptr) {
                const int micOff = sampleStart + s * slotBytes + numRx * 6;
                const int16_t mic16 = static_cast<int16_t>(
                    (static_cast<uint16_t>(frame[micOff]) << 8) |
                    static_cast<uint16_t>(frame[micOff + 1]));
                micOut->push_back(static_cast<float>(mic16) / 32768.0f);
            }
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
