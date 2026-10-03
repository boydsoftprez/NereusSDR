// =================================================================
// src/core/P2RadioConnection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/network.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/network.h, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/netInterface.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/obbuffs.c, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/NetworkIO.cs (upstream has no top-of-file header — project-level LICENSE applies)
//
// --- From deskhpsdr/src/new_protocol.c (3M-1b G.1–G.6) ---
// Byte 50 mic control bits: G.1 mic_boost (0x02), G.2 line_in (0x01),
// G.3 mic_tip_ring (0x08, INVERTED), G.4 mic_bias (0x10), G.5 mic_ptt (0x04, INVERTED),
// G.6 mic_xlr (0x20, P2-only). Lines 1480-1502 [@120188f].
// See modification history and DESKHPSDR-PROVENANCE.md.
//
/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
* 2024,2025 - Heiko Amft, DL1BZ (Project deskHPSDR)
*
*   This source code has been forked and was adapted from piHPSDR by DL1YCF to deskHPSDR in October 2024
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*/
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-27 — setMicBoost: first deskhpsdr port. Byte 50 bit 1 (0x02)
//                 from deskhpsdr new_protocol.c:1484-1486 [@120188f].
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-04-28 — setLineIn: 2nd deskhpsdr port. Byte 50 bit 0 (0x01) from
//                 deskhpsdr new_protocol.c:1480-1482 [@120188f].
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-04-28 — setMicTipRing: 3rd deskhpsdr port. Byte 50 bit 3 (0x08, INVERTED)
//                 from deskhpsdr new_protocol.c:1492-1494 [@120188f].
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-04-28 — setMicBias (G.4): byte 50 bit 4 (0x10), polarity 1=on. deskhpsdr new_protocol.c:1496-1498 [@120188f]. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-04-28 — setMicPTT (G.5): byte 50 bit 2 (0x04, INVERTED). deskhpsdr new_protocol.c:1488-1490 [@120188f]. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-05-04 — setMicPTT renamed to setMicPTTDisabled (issue #182): direct polarity matches Thetis console.cs:19757-19766 [v2.10.3.13+501e3f51]; default MicState::micControl flipped 0x24→0x20 so PTT is enabled at firmware out of the box. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-04-28 — setMicXlr (G.6): byte 50 bit 5 (0x20), P2-only, polarity 1=XLR. deskhpsdr new_protocol.c:1500-1502 [@120188f]. MicState::micControl default updated 0x04 -> 0x24. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-22 - Established UDP silence: Thetis ChannelMaster/network.c:656-667 [v2.10.3.15]; stop/report, daemon-owned recovery.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. Also retire incomplete wideband bursts at capture/connection changes.
//   2026-09-23 - Established silence judged only when no datagram is waiting (R-R3-29): Thetis ChannelMaster/network.c:656-671 [v2.10.3.15].
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the Network Watchdog setting drives general packet byte 38, is sent at once on a change,
//                 gates the 500 ms keepalive and the established-silence wait: Thetis network.c:656, 897-898, 1436 and
//                 netInterface.c:1364-1372 [v2.10.3.15]. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 fix wave: operator decision, the radio's safety timer stays on. Byte 38 is always 1 and the
//                 keepalive always runs (deliberate divergence from network.c:897-898, 1436 [v2.10.3.15]); the setting
//                 governs only the established-silence wait (network.c:656). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - R-R3-49 fix wave: setup.cs's header added below, since setWatchdogEnabled quotes
//                 setup.cs:18024-18028 [v2.10.3.15]. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-32 / R-R3-49 (remote-window parity Task 6): every datagram from the radio,
//                 the per-DDC sequence errors and the DDC arrivals feed the link counters
//                 (RadioLinkStats). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 13: high-priority status ReadBufp[55] (datagram byte 59)
//                 reported as the user digital inputs (network.c:756 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-IOS-13, R-R3-42: the transmit I/Q send path moves off the connection thread's
//                 fixed 4-frames-per-5-ms timer to a send thread of its own, after Thetis ob_main
//                 (obbuffs.c:153-170 [v2.10.3.15]), paced by the radio's transmit buffer estimated
//                 from elapsed time (deskhpsdr new_protocol.c:2243-2272 [@f3d857c]; buffer size from
//                 n1gp-Anvelina_PROIII Tx1_IQ_fifo.vhd:106 [@8e86a61]). The ring grows to 341 ms and a
//                 full ring is counted and logged. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-27 - R-IOS-13: txIqQueuedMs() reads the send ring's fill for the remote microphone's
//                 buffer, which sheds a standing excess only in silence; the key-on cushion is the
//                 radio's target lead plus one frame (16.25 ms, was 20 ms), so no standing 5 ms
//                 stays in the ring. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-49 / R-R3-46: Setup > Transmit > Power's Disable HF PA
//                applied (Thetis DisablePA and hf_tr_relay,
//                transmitSettingsVersion 11). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - G-05: txIqRingDrained and txIqRingLengthMs, so an
//                operator's unkey waits for the transmit I/Q ring to drain,
//                for at most its 341 ms length. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-49: the corrected phase word is Thetis's to the count
//                 (whole corrected Hz, then integer Freq2PhaseWord;
//                 NetworkIO.cs [v2.10.3.15]). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-29 - setActiveReceiverCount notes why its clamp stays on the
//                board row (a wire value; Thetis keeps the radio's
//                reported receiver count for its radio list only). J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: the Alex receive attenuator (Thetis SetAlexAtten,
//                netInterface.c:421-432 [v2.10.3.15]) on the wire, and the step
//                attenuator range above 31 dB on Alex boards (value + 2,
//                console.cs:11044-11056 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Radio codec: the receive audio stream to port 1028 for the
//                radio's own speaker out (Thetis sendOutbound id 0 and
//                WriteUDPFrame case 0, network.c:1276-1294, 1363-1373
//                [v2.10.3.15]), from the transmit I/Q send thread.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - onReadyRead brackets each socket drain with iqBatchStarted /
//                iqBatchFinished so ReceiverManager posts the drain's I/Q to
//                the DSP worker once per stream, and frameReceived is posted
//                once per drain, not once per packet. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Shared-input filters (ruling (c)): the receive low-pass
//                follows the highest slice the model counted on ADC0's input
//                (Thetis UpdateAlexTXFilter, console.cs:15487-15498
//                [v2.10.3.15]). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - Shared-input filters, follow-up: the choice goes through
//                SharedInputLowPass::highest on the DDC centre, the call
//                RadioModel's reason makes. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-30 - The receive socket binds to the local address that reaches
//                the radio, as Thetis binds listenSock to the network card's
//                address for P2 too (NetworkIO.cs:69-70, 149; network.c:84,
//                116-118, 203 [v2.10.3.15]), and is bound again on every
//                connect (disconnect closes it). On macOS a socket bound to
//                Any could share its port with another socket on that
//                address, which then took the radio's frames. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01 - TX diagnostics lane: serviceTxIqSend places the key's
//                padded silence (start, mid-key, tail), its first radio ran
//                dry and its catch-up bursts in time, for the unkey line.
//                Measurement only; nothing sent changes. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-10-01 - TX diagnostics lane, review round: the unkey tail's start,
//                each port 1026 microphone frame's sequence number to the TX
//                pump's wake watch (begun at key, ended at unkey), and its
//                figures in txSendStats. Measurement only. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01 - Diversity lane: setReceiverFrequency tunes DDC0, DDC1 and
//                DDC2 together to RX1 outside the Hermes class (Thetis
//                UpdateRX1DDSFreq), so diversity's partner DDC1 is no longer
//                left at 0 Hz. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// =================================================================

//
// Upstream source 'Project Files/Source/Console/HPSDR/NetworkIO.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

/*
 * network.c
 * Copyright (C) 2015-2020 Doug Wigley (W5WC)
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

// --- From obbuffs.c ---

/*  obbuffs.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014 Warren Pratt, NR0V

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

warren@wpratt.com

*/

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

// --- From setup.cs ---

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

#include "P2RadioConnection.h"
#include "LogCategories.h"
#include "OcMatrix.h"
#include "SharedInputLowPass.h"
#include "CalibrationController.h"
#include "PerfMonitor.h"
#include "audio/RealtimeAudioPriority.h"
#include "audio/TxMicSource.h"
#include "codec/AlexFilterMap.h"
#include "codec/P2CodecHermes.h"
#include "codec/P2CodecOrionMkII.h"
#include "codec/P2CodecSaturn.h"
#include "models/Band.h"
#include "platform/ThreadPlacement.h"

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <QScopeGuard>
#include <QNetworkDatagram>
#include <QThread>
#include <QVariant>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <bit>
#include <cerrno>
#include <chrono>
#include <limits>
#include <thread>

namespace NereusSDR {

// primaryRxDdcForBoard — see header.
//
// From Thetis console.cs:8554-8632 GetDDC() P2 branch [v2.10.3.13].
// Upstream tags preserved: //N1GP (from cited console.cs:8612) [v2.10.3.15]
//
// Upstream inline attribution preserved verbatim (console.cs:8559):
//   case HPSDRHW.Saturn:        // ANAN-G2, G21K    (G8NJJ)
int P2RadioConnection::primaryRxDdcForBoard(HPSDRHW board) noexcept
{
    switch (board) {
    case HPSDRHW::Hermes:
    case HPSDRHW::HermesII:
    case HPSDRHW::HermesC10:  // ANAN-G2E //N1GP G2E added (HermesC10)
        // ANAN-10 / ANAN-100 / ANAN-10E / ANAN-100B / ANAN-G2E running
        // community P2 firmware: rx1 = DDC0 (console.cs:8600-8632 [v2.10.3.13]).
        // Empirically confirmed 2026-05-22 by wire-byte capture of working
        // Thetis-on-G2E session (MAC 40:84:32:B0:B0:8D, N1GP community P2
        // firmware v110): CmdRx byte 7 = 0x01 (DDC0 enable bit) when RX
        // active.  An earlier intermediate fix tried DDC2 — that was wrong;
        // Thetis stays with Hermes-class DDC0 per console.cs:8615-8620
        // [v2.10.3.15] //N1GP G2E added.
        return 0;
    default:
        // Angelia / Orion / OrionMKII / Saturn / SaturnMKII and any future
        // 2-ADC SKU: rx1 = DDC2 (console.cs:8556-8598 [v2.10.3.13]).
        // (G8NJJ Saturn case at console.cs:8559 [v2.10.3.13])
        return 2;
    }
}

P2RadioConnection::P2RadioConnection(QObject* parent)
    : RadioConnection(parent)
{
    // From Thetis create_rnet() netInterface.c:1416
    // Initialize rx state with Thetis defaults
    for (int i = 0; i < kMaxRxStreams; ++i) {
        m_rx[i].id = i;
        m_rx[i].rxAdc = 0;
        m_rx[i].frequency = 0;
        m_rx[i].enable = 0;
        m_rx[i].sync = 0;
        m_rx[i].samplingRate = 48;     // From Thetis create_rnet:1488
        m_rx[i].bitDepth = 24;         // From Thetis create_rnet:1489
        m_rx[i].preamp = 0;
        m_rx[i].spp = 238;             // From Thetis create_rnet:1496
        m_rx[i].rxInSeqNo = 0;
        m_rx[i].rxInSeqErr = 0;
    }

    // From Thetis create_rnet() netInterface.c:1504-1514 [v2.10.3.13]
    for (int i = 0; i < kMaxTxStreams; ++i) {
        m_tx[i].id = i;
        m_tx[i].frequency = 0;
        // 3M-1a bench fix: P2 TX is ALWAYS 192 kHz (Thetis netInterface.c:1513
        // [v2.10.3.13]).  This was incorrectly initialised to 48 (a copy-paste
        // from the RX block above where 48 kHz IS correct), causing
        // txSampleRate() to return 48000 → createTxChannel opened the WDSP TX
        // channel with outputSampleRate=48000 → WDSP rsmpout produced samples
        // at the wrong rate → G2 saw silence/aliased noise instead of carrier.
        m_tx[i].samplingRate = 192;
        m_tx[i].cwx = 0;
        m_tx[i].dash = 0;
        m_tx[i].dot = 0;
        m_tx[i].pttOut = 0;
        m_tx[i].driveLevel = 0;
        m_tx[i].phaseShift = 0;
        // 3M-1a (2026-04-27): Thetis's `prn->tx[0].pa` is the *DisablePA* flag,
        // not "PA enabled".  CmdGeneral byte 58 is written as `(!pa) & 0x01`
        // (Thetis network.c:904 [v2.10.3.13]), so pa=0 ⇒ wire byte 58 = 1
        // ⇒ radio enables its PA.  Previously initialised to 1 (DisablePA on),
        // which silently kept the radio's PA off during MOX — exact symptom
        // of "MOX engages, TX I/Q on the wire, no carrier on the SO-239".
        // deskhpsdr's equivalent default is `pa_enabled = 1` (new_protocol.c
        // [@120188f]) — same effective wire byte (1).
        m_tx[i].pa = 0;   // PA enabled (NOT inverted: this is the DisablePA bit)
        m_tx[i].epwmMax = 0;
        m_tx[i].epwmMin = 0;
    }

    // Phase 3F Sub-Epic F Task 3: construct per-ADC wideband frame
    // accumulators. Eight max (one per possible ADC index 0..7 — port
    // indices 2..9 / UDP ports 1027..1034 per Thetis network.c:550-602
    // [v2.10.3.15]). Only those whose bit is set in m_wbEnableMask will
    // actually receive packets. The lambda captures `i` by value so the
    // forwarded widebandFrameReady carries the correct ADC index.
    for (int i = 0; i < 8; ++i) {
        m_wbCaptureEpochs[i] = std::make_shared<std::atomic<quint64>>(1);
        m_wbAccumulators[i] = new WidebandFrameAccumulator(this);
        connect(m_wbAccumulators[i], &WidebandFrameAccumulator::frameReady,
                this, [this, i](const QVector<float>& samples) {
            // Capture before emitting either signal. A direct tagged observer
            // may disable/re-enable capture; this completed row must retain the
            // generation under which its assembler emitted it.
            const quint64 captureGeneration =
                m_wbCaptureEpochs[i]->load(std::memory_order_acquire);
            const qint64 producedAtNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            emit widebandFrameReadyForGeneration(i, captureGeneration, samples, producedAtNs);
            emit widebandFrameReady(i, samples);
        });
    }
}

P2RadioConnection::~P2RadioConnection()
{
    if (m_running) {
        disconnect();
    }
    // The send thread uses this object and its socket's descriptor.
    stopTxIqSender();
    // Workers may retain the shared atomic after QObject destruction. Always
    // invalidate their last observation, including never-started/already-
    // stopped connections for which disconnect() did not run here.
    advanceAllWidebandCaptureEpochs();
}

// --- Thread Lifecycle ---
// Porting from Thetis nativeInitMetis() network.c:84
// Creates a single UDP socket, matching Thetis listenSock

void P2RadioConnection::init()
{
    m_socket = new QUdpSocket(this);

    // A placeholder binding until connectToRadio() binds the address that
    // reaches the radio (bindToRadioFacingAddress), as Thetis binds the
    // network card's address in nativeInitMetis (network.c:116-118, 203
    // [v2.10.3.15]).
    if (!m_socket->bind(QHostAddress::Any, 0)) {
        qCWarning(lcConnection) << "P2: Failed to bind UDP socket";
        return;
    }

    applySocketBufferSizes();

    connect(m_socket, &QUdpSocket::readyRead, this, &P2RadioConnection::onReadyRead);

    // From Thetis KeepAliveLoop network.c:1428 — timer fires every 500ms
    m_keepAliveTimer = new QTimer(this);
    m_keepAliveTimer->setInterval(kKeepAliveIntervalMs);
    connect(m_keepAliveTimer, &QTimer::timeout, this, &P2RadioConnection::onKeepAliveTick);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(3000);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &P2RadioConnection::onReconnectTimeout);

    // TX I/Q stream: the radio expects continuous TX data on port 1029 even
    // in RX-only mode. R-IOS-13, R-R3-42: a send thread of its own now
    // carries it (startTxIqSender, from SendStart); see the TxIqPacer
    // comment in the header for how it paces.

    // 3M-1a (2026-04-27): periodic protocol heartbeat at 100 ms.  The radio
    // expects high-priority packets at this cadence to keep TX state fresh
    // (MOX, drive, freq, antenna) — without it the radio treats TX state as
    // stale and never engages the PA, so even though our TX I/Q is on the
    // wire there is no carrier coming out the SO-239.
    //
    // Cadence per deskhpsdr/src/new_protocol.c:2870-2898 [@120188f]:
    //   every 100 ms       — high-pri
    //   every 200 ms (alt) — RX-spec / TX-spec
    //   every 800 ms       — General
    //
    // Dispatch table over an 8-cycle wheel (0..7) — matches deskhpsdr's
    // `switch (cycling)` cases 1..8 (we're 0-indexed).
    //
    // 2026-07-27 (ANAN-G2E lockup): the wheel is now gated on MOX, so it
    // only runs while transmitting.  Rationale, from a TZSP wire capture of
    // Thetis and NereusSDR against the same ANAN-G2E (analysis in
    // captures/g2e-disconnect-NOTES.md):
    //
    //   command          Thetis            NereusSDR (before)
    //   CmdHighPriority  1 per session     12.47/s
    //   CmdTx            1 per session     5.11/s
    //   CmdRx            1.28/s bursty     5.70/s
    //
    // Thetis never polls.  From Thetis network.c [v2.10.3.15], CmdHighPriority()
    // is reached only from SendStart():362-369, SendStop():372-376, and
    // per-control state changes; only CmdGeneral() is periodic, from
    // KeepAliveLoop():1428-1437 (`if (prn->run && prn->wdt) CmdGeneral();`),
    // whose job is feeding the board's watchdog (CmdGeneral byte 38 carries
    // prn->wdt, network.c:897-898).  The radio-side watchdog interval is not
    // stated anywhere in the Thetis tree; Thetis only sends every 500 ms.
    //
    // NereusSDR already pushes every state change immediately (11 change-driven
    // sendCmdHighPriority sites, 4 for CmdRx, 10 for CmdTx), so the wheel was
    // purely additive polling.  The original 3M-1a rationale above is a TX
    // concern ("keep TX state fresh ... never engages the PA"), so it is
    // preserved verbatim for the transmit case and simply not run during RX,
    // where there is no PA state to keep fresh.  On MOX transitions
    // setMox() already emits an immediate CmdHighPriority (see below), so the
    // first TX frame does not wait on this timer.
    m_p2HeartbeatTimer = new QTimer(this);
    m_p2HeartbeatTimer->setInterval(100);
    m_p2HeartbeatTimer->setTimerType(Qt::PreciseTimer);
    connect(m_p2HeartbeatTimer, &QTimer::timeout, this, [this]() {
        if (!m_running) { return; }
        // RX: Thetis-faithful, event-driven only.  The MOX-off grace window
        // keeps the wheel turning briefly after unkey so a lost MOX-off frame
        // is retransmitted rather than leaving the radio keyed — see setMox().
        if (!m_mox && !withinMoxOffGrace()) { return; }
        sendCmdHighPriority();                // every 100 ms (every cycle)
        switch (m_p2HeartbeatCycle) {
            case 0: case 2: case 4: case 6:   // odd-numbered cycles → TX-spec
                sendCmdTx();                  // every 200 ms
                break;
            case 1: case 3: case 5:           // even-numbered cycles → RX-spec
                sendCmdRx();                  // every 200 ms
                break;
            case 7:
                sendCmdRx();                  // every 200 ms
                sendCmdGeneral();             // every 800 ms (once per wheel)
                break;
        }
        m_p2HeartbeatCycle = (m_p2HeartbeatCycle + 1) % 8;
    });

    // Connect watchdog — single-shot; fires kConnectTimeoutMs after
    // connectToRadio() if no first DDC I/Q frame arrives. Phase 3Q Task 3.
    m_connectWatchdog = new QTimer(this);
    m_connectWatchdog->setSingleShot(true);
    connect(m_connectWatchdog, &QTimer::timeout, this, &P2RadioConnection::onConnectTimeout);

    // Thetis ReadThreadMainLoop waits up to three seconds for any inbound
    // P2 UDP after the stream is established (network.c:656-667
    // [v2.10.3.15]). The QTimer is only the wakeup; QDeadlineTimer below is
    // the monotonic authority and the connection generation rejects stale
    // queued callbacks.
    m_establishedSilenceTimer = new QTimer(this);
    m_establishedSilenceTimer->setSingleShot(true);
    m_establishedSilenceTimer->setTimerType(Qt::PreciseTimer);
    connect(m_establishedSilenceTimer, &QTimer::timeout,
            this, &P2RadioConnection::onEstablishedSilenceTimeout);

    qCDebug(lcConnection) << "P2: init() socket port:" << m_socket->localPort();
}

// ---------------------------------------------------------------------------
// applySocketBufferSizes
//
// The send and receive buffer sizes, set on every bind (init() and
// bindToRadioFacingAddress(); a closed socket loses them).
// ---------------------------------------------------------------------------
void P2RadioConnection::applySocketBufferSizes()
{
    // From Thetis nativeInitMetis:163-194 — socket buffer sizing
    // const int sndbuf_bytes = 0xfa000; const int rcvbuf_bytes = 0xfa000;
    //
    // 2026-05-26 KG4VCF bench fix: bumped recv buffer from Thetis's
    // 1000 KB (0xfa000) to 4 MB so the kernel can soak up a brief
    // preemption window without dropping I/Q packets.  Under heavy
    // build load on macOS, even with the ConnectionThread elevated to
    // USER_INTERACTIVE QoS, the kernel-to-userspace handoff can stall
    // a few ms when ninja workers saturate all cores; the original
    // 1 MB buffer held ~100 ms of P2 I/Q which was enough for the
    // occasional miss to drop frames.  macOS kern.ipc.maxsockbuf
    // typically caps at 2 MB on stock systems, so the actual size is
    // min(4 MB, sysctl cap) -- both numbers headroom for build-load
    // stalls.
    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption,
                              QVariant(0xfa000));
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                              QVariant(0x400000));  // 4 MB requested; kernel may cap
}

// ---------------------------------------------------------------------------
// bindToRadioFacingAddress
//
// Binds the socket to the local address this host reaches the radio from,
// on a port the OS chooses. Thetis binds its single listenSock this way for
// Protocol 2 as for Protocol 1, to the selected network card's own IPv4
// address, port 0 unless set:
//   From Thetis NetworkIO.cs:69-70 [v2.10.3.15]:
//     string hostIP = nic.LocalIPv4.ToString();
//     int hostPort = c.SetupForm.ListenToRadioOnUDPPort; // will be any os available port if 0, or specific if set
//   From Thetis NetworkIO.cs:149 [v2.10.3.15]:
//     ret = nativeInitMetis(radioIP, ratioPort, hostIP, hostPort, protocol, model_id);
//   From Thetis network.c:116-118 and 203 [v2.10.3.15]:
//     local.sin_port = htons((u_short)localport);
//     local.sin_family = AF_INET;
//     local.sin_addr.s_addr = inet_addr(localaddr);
//     rc = bind(listenSock, (SOCKADDR*)&local, sizeof(local));
// Every Protocol 2 stream, to and from the radio (general, receive specific,
// transmit specific, high priority, receive audio, transmit I/Q; status,
// DDC I/Q, mic, wideband back), goes through that one socket, as through
// m_socket here, so this one bind covers every receive port. Sends:
// network.c:910, 1062, 1178, 1247, 1373, 1388 [v2.10.3.15]. Receives, every
// port on the one socket:
//   From Thetis network.c:650 [v2.10.3.15]:
//     WSAEventSelect(listenSock, prn->hDataEvent, FD_READ);
//   From Thetis network.c:493 [v2.10.3.15]:
//     nrecv = recvfrom(listenSock, readbuf, sizeof(readbuf), 0, (SOCKADDR*)&fromaddr, &fromlen);
//
// NereusSDR bound to every address (Any) instead. On macOS the OS can give
// a socket bound to Any a port that another socket already holds on one
// address, and a datagram to that address and port goes to the other
// socket: the radio streams, its frames go elsewhere, and the connect
// watchdog fires. A socket bound to the address itself gets a port no other
// socket holds there.
//
// NereusSDR has no network card selection, so the address is the one the
// OS routes to the radio from: the source address the radio answers (on a
// loopback radio, 127.0.0.1; with several interfaces, the one the route to
// the radio leaves by). A UDP connect sends nothing. With no route, or if
// that address cannot be bound, it says so with a warning and listens on
// every address, as before.
// ---------------------------------------------------------------------------
void P2RadioConnection::bindToRadioFacingAddress()
{
    if (!m_socket || m_radioInfo.address.isNull()) { return; }

    QHostAddress local = radioFacingAddress();
    if (m_socket->state() == QAbstractSocket::BoundState
        && !local.isNull() && m_socket->localAddress() == local) {
        return;
    }

    // The send thread writes on the socket's descriptor; it is restarted
    // with the new one later in connectToRadio.
    stopTxIqSender();
    m_socket->close();
    if (local.isNull()) {
        qCWarning(lcConnection) << "P2: no local address reaches"
                                << m_radioInfo.address.toString()
                                << "; listening on every address";
    } else if (!m_socket->bind(local, 0)) {
        qCWarning(lcConnection) << "P2: could not listen on" << local.toString()
                                << "(" << m_socket->errorString()
                                << "); listening on every address";
        local = QHostAddress();
    }
    if (local.isNull() && !m_socket->bind(QHostAddress::Any, 0)) {
        qCWarning(lcConnection) << "P2: Failed to bind UDP socket";
        return;
    }
    applySocketBufferSizes();
}

// ---------------------------------------------------------------------------
// radioFacingAddress
//
// The local address the OS routes to m_radioInfo.address from, or a null
// address when none does within kRouteLookupMs. Tests may stand in for the
// lookup (setRadioFacingAddressForTest) to reach the fallback paths.
// ---------------------------------------------------------------------------
QHostAddress P2RadioConnection::radioFacingAddress() const
{
    if (m_radioFacingOverridden) { return m_radioFacingOverride; }
    QUdpSocket route;
    route.connectToHost(m_radioInfo.address, m_radioInfo.port);
    if (route.waitForConnected(kRouteLookupMs)) {
        return route.localAddress();
    }
    return QHostAddress();
}

// --- Connection Lifecycle ---
// Porting from Thetis SendStart() network.c:362
// prn->run = 1; CmdGeneral(); CmdRx(); CmdTx(); CmdHighPriority();

void P2RadioConnection::connectToRadio(const RadioInfo& info)
{
    if (m_running) {
        disconnect();
    }

    ++m_connectionGeneration;
    discardWidebandFrames();
    m_linkLossLatched = false;
    m_establishedSilenceGeneration = 0;
    m_establishedSilenceDeadline = QDeadlineTimer();
    if (m_establishedSilenceTimer) {
        m_establishedSilenceTimer->stop();
    }

    // A new transport generation always starts unkeyed. In particular, a
    // stale setter delivered after the previous LinkLost must not carry MOX,
    // PureSignal, or relay intent into this SendStart sequence.
    m_mox.store(false);
    m_puresignalRun = false;
    m_trxRelay = false;
    m_tx[0].pttOut = 0;
    m_txIqPrimePending.store(false, std::memory_order_release);
    m_moxOffGrace = QDeadlineTimer();

    m_radioInfo = info;
    m_intentionalDisconnect = false;
    m_totalIqPackets = 0;

    // Listen on the address the radio answers, before anything is sent.
    // disconnect() closes the socket, so every connect binds it again.
    bindToRadioFacingAddress();

    // Use HardwareProfile for capability lookup (Phase 3I-RP).
    // Fall back to board-byte lookup if setHardwareProfile() was never called.
    m_caps = m_hardwareProfile.caps
             ? m_hardwareProfile.caps
             : &BoardCapsTable::forBoard(info.boardType);

    // Phase 3P-B Task 7: select per-board codec now that m_caps is available.
    selectCodec();

    // Reset sequence counters
    m_seqGeneral = 0;
    m_seqRx = 0;
    m_seqTx = 0;
    m_seqHighPri = 0;
    m_ccSeqNo = 0;

    // 3M-1a (2026-04-27 v4): num_adc = hardware ADC count.  Earlier guess
    // (numAdc=1) was wrong — drove from a freeze-fixture test scenario, not
    // runtime.  Authoritative reference: captures/thetis-3865-lsb-pcap-analysis.md
    // line 34 — Thetis sends "Num ADC | 2 | ANAN-G2 has 2 ADCs" for the same
    // ANAN-G2 in non-diversity RX-only mode.  The freeze fixture's byte 4 = 1
    // captures a default-init state before SetADCCount(2) is applied.
    m_numAdc = m_hardwareProfile.caps ? m_hardwareProfile.adcCount : m_caps->adcCount;
    m_numDac = 1;
    // R-R3-49: byte 38 enables the radio's own safety timer, which drops the
    // radio out of transmit when general packets stop arriving. NereusSDR
    // deliberately keeps it on whatever the Network Watchdog setting says: a
    // radio left keyed when the computer dies is a hazard (operator decision
    // 2026-09-24). Thetis lets byte 38 follow the setting instead:
    // From Thetis network.c:897-898 [v2.10.3.15]:
    //   // Watchdog Timer default = 0 disabled
    //   packetbuf[38] = prn->wdt;
    // The setting governs only the wait for data (onEstablishedSilenceTimeout).
    m_wdt = 1;

    // From Thetis console.cs:8216 UpdateDDCs() — 2-ADC P2 boards (Angelia /
    // Orion / OrionMKII / Saturn / ANAN-G2) place RX1 on DDC2 because DDC0/
    // DDC1 are reserved for the diversity / PureSignal pair.  1-ADC P2 boards
    // (Hermes / HermesII — ANAN-10E / ANAN-100B running community P2
    // firmware) place RX1 on DDC0 (console.cs:8451-8521 + 8600-8632
    // [v2.10.3.13]).  Picking the wrong DDC means the radio either ignores
    // the enable bit or streams on a DDC NereusSDR isn't listening to —
    // either way no I/Q frames arrive, the connect watchdog fires after
    // kConnectTimeoutMs, and the user sees "connects for a few seconds
    // then disconnects" (issue #263).
    //
    // Upstream inline attribution preserved verbatim (console.cs:8238):
    //   if (p1) Rate[0] = rx1_rate; // [2.10.3.13]MW0LGE p1 !
    const int primaryDdc = primaryRxDdcForBoard(info.boardType);

    // New session: no codec has computed a mask yet, so the board-aware
    // bootstrap on the next line plus setActiveReceiverCount hold byte 7
    // until the first applyDdcAssignment takes ownership.
    // Reset here (not in disconnect) so a reconnect on the same object gets
    // the same start state as a freshly constructed one.
    m_ddcMaskOwnedByCodec = false;

    m_rx[primaryDdc].enable = 1;
    // Phase 3P-I-a bench-bug fix (KG4VCF 2026-04-22):
    // RadioModel::connectToRadio queues setReceiverFrequency BEFORE
    // this method in the worker-thread FIFO — so by the time we run,
    // m_rx[primaryDdc].frequency already holds the persisted VFO (e.g.
    // 14.3 MHz for 20m) and m_alex.hpfBits/lpfBits reflect the same. Only
    // seed the 80m default when nothing has been stored yet
    // (m_rx[primaryDdc].frequency==0); the prior unconditional assign
    // clobbered the real VFO, so the initial CmdHighPriority packet went
    // out with the primary DDC tuned to 80m but BPF bits for 20m — radio
    // heard nothing until the next setReceiverFrequency fired from a user
    // tune. The comment that used to read "overridden by
    // setReceiverFrequency" had the FIFO order backwards.
    if (m_rx[primaryDdc].frequency == 0) {
        m_rx[primaryDdc].frequency = 3865000;   // 80m LSB — first-boot default only
        double freqMhz = m_rx[primaryDdc].frequency / 1.0e6;
        m_alex.hpfBits   = NereusSDR::codec::alex::computeRxPreselector(
            freqMhz, m_caps ? m_caps->board : HPSDRHW::Unknown, m_alexHpfEdges);
        applyAlexLpf(freqMhz, /*freqIsTx=*/false);
    } else {
        // Same FIFO ordering as above, with a consequence the original fix did
        // not have to think about: setReceiverFrequency ran BEFORE us, and at
        // that point m_caps was still null (it is assigned at the top of this
        // method), so its preselector call fell back to the legacy high-pass
        // ladder.  On a Saturn-class board that is the wrong bank, and nothing
        // would correct it until the operator retuned by hand.  m_caps is
        // valid now, so redo the selection from the frequency already stored.
        //
        // The low-pass is re-selected too: setReceiverFrequency ran before the
        // saved rows and 6m/ByPass on RX reached this connection, and the
        // selection reads both (setAlexLPF, console.cs:7177-7243 [v2.10.3.15]).
        // From Thetis console.cs:6827-6837 setAlex1HPF [v2.10.3.15]
        // Upstream inline attribution preserved verbatim (console.cs:6830):
        //    || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
        const double freqMhz = m_rx[primaryDdc].frequency / 1.0e6;
        m_alex.hpfBits = NereusSDR::codec::alex::computeRxPreselector(
            freqMhz, m_caps ? m_caps->board : HPSDRHW::Unknown, m_alexHpfEdges);
        applyReceiveAlexLpf();
    }
    // The primary DDC's samplingRate is set by setSampleRate() which
    // RadioModel queues before connectToRadio in the FIFO (see
    // RadioModel::connectToRadio).  Do NOT hardcode a rate here — it
    // would stomp the user-selected value.

    // From pcap: Thetis enables dither and random on all ADCs
    m_adc[0].dither = 1;
    m_adc[1].dither = 1;
    m_adc[2].dither = 1;
    m_adc[0].random = 1;
    m_adc[1].random = 1;
    m_adc[2].random = 1;

    // TX frequency — only seed default if nothing's been set (RadioModel
    // doesn't queue an explicit setTxFrequency before connectToRadio today,
    // but simplex TX will follow RX once TX phase lands. Same FIFO-order
    // rationale as m_rx[2].frequency above.)
    if (m_tx[0].frequency == 0) {
        m_tx[0].frequency = 3865000;
        // Seed the transmit low-pass from the same number, exactly as the
        // receive seed twenty lines up does for lpfBitsRx. Setting the TX NCO
        // without it left the wire self-contradictory: TX frequency 3.865 MHz
        // alongside m_alex.lpfBitsTx still at its 6 m default. Thetis never
        // splits the two -- UpdateTXDDSFreq assigns the low-pass and the NCO
        // from tx_dds_freq_mhz in one call (console.cs:15464-15468
        // [v2.10.3.15]) -- so neither do we.
    }
    // Re-selected either way: an earlier setTxFrequency ran before the saved
    // low-pass rows reached this connection.
    applyAlexLpf(m_tx[0].frequency / 1.0e6, /*freqIsTx=*/true);

    // R-R3-32 (parity Task 6): the link counters start with the connection.
    m_linkStats.reset();

    setState(ConnectionState::Connecting);

    qCDebug(lcConnection) << "P2: Connecting to" << info.displayName()
                          << "at" << info.address.toString()
                          << "from port" << m_socket->localPort();

    // From Thetis SendStart() network.c:362-369
    m_running = true;         // prn->run = 1;
    sendCmdGeneral();         // CmdGeneral(); //1024
    sendCmdRx();              // CmdRx(); //1025
    sendCmdTx();              // CmdTx(); //1026
    sendCmdHighPriority();    // CmdHighPriority(); //1027

    qCDebug(lcConnection) << "P2: SendStart complete (run=1)";

    // From Thetis StartAudioNative netInterface.c:83
    // prn->hKeepAliveThread = _beginthreadex(NULL, 0, KeepAliveMain, 0, 0, NULL);
    m_keepAliveTimer->start();
    // 3M-1a bench fix: re-enabled — the producer side (TxChannel::driveOneTxBlock,
    // commit e6e48bd) now feeds the SPSC ring with real TX I/Q during TUN.  Earlier
    // "weak signals" concern was an artifact of the unwired producer (silence stream
    // made the carrier appear low-energy on the radio).  Confirmed correct after
    // E.6 + E.7 + e6e48bd landed.
    // R-IOS-13, R-R3-42: the send thread, after Thetis ob_main
    // (obbuffs.c:153-170 [v2.10.3.15]); started once the socket has sent
    // (and so is bound) so its descriptor is final.
    startTxIqSender();
    // 3M-1a (2026-04-27): protocol heartbeat — high-pri every 100 ms, etc.
    m_p2HeartbeatCycle = 0;
    m_p2HeartbeatTimer->start();

    // State stays Connecting; processIqPacket() promotes Connecting -> Connected
    // on the first DDC I/Q packet. ConnectionState.h:17-20: Connecting means
    // "start sent, awaiting first frame"; Connected means "data flowing".
    // Issue #239: previously transitioned to Connected here, which made the
    // UI claim "Connected" for the full 2 s connect-watchdog window even when
    // the radio was powered off.

    // Arm the connect watchdog: if no first DDC I/Q frame arrives within
    // kConnectTimeoutMs, emit connectFailed(Timeout, ...).
    // processIqPacket() cancels this on the first valid frame. Phase 3Q Task 3.
    if (m_connectWatchdog) {
        m_connectWatchdog->start(m_connectTimeoutMs);
    }
}

// Porting from Thetis SendStop() network.c:372
// prn->run = 0; CmdHighPriority();

void P2RadioConnection::disconnect()
{
    m_intentionalDisconnect = true;
    m_linkLossLatched = true;
    ++m_connectionGeneration;
    discardWidebandFrames();
    m_establishedSilenceGeneration = 0;
    m_establishedSilenceDeadline = QDeadlineTimer();

    if (m_keepAliveTimer) {
        m_keepAliveTimer->stop();
    }
    stopTxIqSender();
    if (m_p2HeartbeatTimer) {
        m_p2HeartbeatTimer->stop();
    }
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    // Cancel the connect watchdog so intentional disconnect() does not
    // trigger connectFailed() after the user has already moved on.
    if (m_connectWatchdog) {
        m_connectWatchdog->stop();
    }
    if (m_establishedSilenceTimer) {
        m_establishedSilenceTimer->stop();
    }

    if (m_running && m_socket && !m_radioInfo.address.isNull()) {
        // 2026-05-22 bench-finding (second pcap capture, clean Power-Off
        // event): Thetis's actual disconnect signal is ONE CmdHighPriority
        // frame with byte 4 = 0x00 (run=0), then total radio silence.  Per
        // captured frame 29900 at t=9.595 in g2e-tzsp.pcap (Power button
        // off click).
        //
        // Frame content: byte 4 = 0x00, freqs (bytes 9-16) preserved from
        // active state — NOT zeroed.  This is what composeCmdHighPriority()
        // already does when m_running=false (run bit clears, other state
        // carried forward), matching Thetis byte-for-byte.
        //
        // Earlier intermediate guess (5x CmdGeneral burst) was based on a
        // misread of the first pcap, which actually contained only a
        // partial session, not a true disconnect.  Reverting to the
        // Thetis-faithful single CmdHighPriority(run=0) pattern.
        //
        // Defensive: flush + 20 ms sleep before close so the run=0 frame
        // is actually on the wire before the socket goes away.
        // 2026-07-27: quiesce before the stop frame.  A TZSP capture of a
        // NereusSDR disconnect showed a CmdRx leaving 4.4 ms ahead of the
        // run=0 frame — the last 100 ms heartbeat tick firing between the
        // timer stops above and the send below.  Each CmdRx re-latches
        // EnableRx0_7 and the per-DDC rates in the gateware
        // (Hermes.v:717/741 hold a DDC FIFO in reset from its enable bit),
        // so landing one immediately before the stop reconfigures the DDCs
        // as they are being shut down.  Thetis never does this: its capture
        // shows the stop frame arriving with no other command traffic near
        // it.  The heartbeat is now MOX-gated so this cannot happen on an RX
        // disconnect, and the settle covers a disconnect taken during TX.
        QThread::msleep(kStopQuiesceMs);

        m_running = false;       // prn->run = 0;
        sendCmdHighPriority();   // From Thetis SendStop() network.c:372-376
        m_socket->flush();
        QThread::msleep(kStopDrainMs);
        qCDebug(lcConnection)
            << "P2: SendStop sent (1x CmdHighPriority run=0, Thetis-faithful);"
            << "any send failure is logged separately above";
    }

    m_running = false;

    if (m_socket) {
        m_socket->close();
    }

    setState(ConnectionState::Disconnected);
    qCDebug(lcConnection) << "P2: Disconnected. I/Q packets:" << m_totalIqPackets;
}

// --- Hardware Control Slots ---

void P2RadioConnection::setReceiverFrequency(int receiverIndex, quint64 frequencyHz)
{
    if (receiverIndex < 0 || receiverIndex >= kMaxRxStreams) {
        return;
    }
    m_rx[receiverIndex].frequency = static_cast<int>(frequencyHz);

    // Diversity's second leg. Outside the Hermes class, RX1 owns DDC0, DDC1
    // and DDC2: it runs on DDC2, or on the DDC0 + DDC1 synchronized pair
    // while diversity is on, and Thetis tunes all three to RX1's frequency
    // together so the partner DDC1 (which carries no receiver) mixes the
    // same signal as DDC0. The DDC0/DDC1 PureSignal override stays the
    // codec's (network.c:936-945).
    // From Thetis console.cs:15398-15423 UpdateRX1DDSFreq [v2.10.3.15]:
    //   switch (HardwareSpecific.Model)
    //   {
    //       case HPSDRModel.HERMES:
    //       case HPSDRModel.ANAN10:
    //       case HPSDRModel.ANAN10E:
    //       case HPSDRModel.ANAN100:
    //       case HPSDRModel.ANAN100B:
    //       case HPSDRModel.ANAN_G2E: //N1GP G2E added
    //           NetworkIO.VFOfreq(0, rx1_dds_freq_mhz, 0);
    //           break;
    //       default:
    //           NetworkIO.VFOfreq(0, rx1_dds_freq_mhz, 0);
    //           NetworkIO.VFOfreq(1, rx1_dds_freq_mhz, 0);
    //           NetworkIO.VFOfreq(2, rx1_dds_freq_mhz, 0);
    //           break;
    //   }
    // The Hermes-class models are exactly the boards primaryRxDdcForBoard
    // puts RX1 on DDC0 (Hermes, HermesII, HermesC10); there DDC1 is RX2's
    // own receiver (UpdateRX2DDSFreq, console.cs:15446-15459 [v2.10.3.15])
    // and is left alone. Before the board is known (a frequency queued
    // ahead of connectToRadio) nothing is mirrored; the codec's assignment
    // re-pushes the receiver's frequency once diversity moves RX1 to DDC0.
    // DDC0, DDC1 and DDC2: the three VFOfreq ids in the default arm above.
    constexpr int kRx1Ddcs = 3;
    const bool hermesClass = m_caps && primaryRxDdcForBoard(m_caps->board) == 0;
    if (m_caps && !m_ddcMaskOwnedByCodec && !hermesClass && receiverIndex < kRx1Ddcs) {
        for (int ddc = 0; ddc < kRx1Ddcs; ++ddc) {
            m_rx[ddc].frequency = static_cast<int>(frequencyHz);
        }
    }

    // Once a codec owns the map, DDC2 may be another operator's stream.
    // Tune only the active pair's unhosted sync leg from its primary.
    if (m_ddcMaskOwnedByCodec && receiverIndex == 0 && m_rx[0].sync == 2) {
        m_rx[1].frequency = static_cast<int>(frequencyHz);
    }

    m_lastRetunedDdc = receiverIndex;
    recomputeReceiveFilters();

    if (m_running) {
        sendCmdHighPriority();
    }
}

// ---------------------------------------------------------------------------
// recomputeReceiveFilters: the receive-side Alex selections (the fallback
// high-pass m_alex.hpfBits and the receive low-pass m_alex.lpfBitsRx) from
// the RX1 stand-in (rx1Ddc, plan Task 14, Phase 3F section 16.3.2). Until a
// live-slot mask arrives the stand-in is the DDC retuned last, which is
// exactly the selection this made before.
// ---------------------------------------------------------------------------
void P2RadioConnection::recomputeReceiveFilters()
{
    const int rx1 = rx1Ddc();
    const int rx1Hz = m_rx[static_cast<size_t>(rx1)].frequency;
    if (rx1Hz <= 0) {
        return;
    }

    // Update Alex HPF/LPF based on new frequency
    // From Thetis console.cs:6830-7234 [@501e3f5] — auto-select band filters
    // Upstream tags preserved: //N1GP (from cited console.cs:6830) [v2.10.3.15]
    // Upstream inline attribution preserved verbatim:
    //   :6830  || (HardwareSpecific.Hardware == HPSDRHW.HermesIII)) //DK1HLM
    //
    // Saturn-class boards (Orion MkII / Saturn / HermesC10) carry a BAND-PASS
    // bank on these same relay bits rather than the legacy high-pass ladder,
    // so the ladder has to be chosen by board.  Selecting the wrong one is
    // silent on the air: the radio still hears the band, just through the
    // neighbouring filter.
    // From Thetis console.cs:6827-6837 setAlex1HPF [v2.10.3.15]
    const double freqMhz = rx1Hz / 1e6;
    m_alex.hpfBits = NereusSDR::codec::alex::computeRxPreselector(
        freqMhz, m_caps ? m_caps->board : HPSDRHW::Unknown, m_alexHpfEdges);

    // RF-SAFETY: a receive frequency selects the RECEIVE low-pass only. It
    // must never reach m_alex.lpfBitsTx, which is the transmit low-pass.
    //
    // This used to write a single shared mask that both Alex words read, so
    // on a multi-slice radio the TX low-pass followed whichever DDC was
    // retuned last. Adding a slice on 80 m while slice A transmits on 10 m
    // left roughly 100 W of 28.4 MHz driving a ~4 MHz low-pass, presenting
    // as an SWR alarm or power foldback on the high band that only appears
    // when a low-band slice happens to exist.
    //
    // Thetis routes the write by intent instead:
    //   From Thetis ChannelMaster/netInterface.c:682-726 [v2.10.3.15]
    //     SetAlexLPFBits(bits, isTX, isMox)
    //     if (isMox || !isTX) -> AlexLPFMask  (Alex0)
    //   and its receive-derived caller passes isTX = false:
    //   From Thetis console.cs:15487-15498 UpdateAlexTXFilter [v2.10.3.15]
    //     if (!_mox) { ... setAlexLPF(rx1_dds_freq_mhz, false); }
    //
    // That `if (!_mox)` wrapper is why this is gated on the transmit state:
    // Thetis cannot reach the receive-derived write at all while keyed, so a
    // retune arriving mid-transmission must leave both words untouched.
    //
    // Which receive frequency: Thetis's rule, RX1 alone, or the HIGHER of
    // RX1 and RX2 when RX2 shares this filter (no RX2 front end of its own),
    // because a low-pass passes everything below its corner:
    //   From Thetis console.cs:15487-15498 UpdateAlexTXFilter [v2.10.3.15]
    //     if (!_rx2_preamp_present && chkRX2.Checked)
    //     {
    //         if (rx1_dds_freq_mhz > rx2_dds_freq_mhz) setAlexLPF(rx1_dds_freq_mhz, false);
    //         else setAlexLPF(rx2_dds_freq_mhz, false);
    //     }
    //     else setAlexLPF(rx1_dds_freq_mhz, false);
    // Plan Task 14: this used to be whichever DDC was retuned last, so on
    // the G2 (RX2 has its own front end) adding slice B on a lower band put
    // slice A behind B's low-pass. RX1 is the stand-in (rx1Ddc).
    //
    // Fix wave M6: RX2 is the highest live receiver other than RX1, not the
    // next one above it. Thetis has exactly two receivers; with more live
    // slices on a shared front end the low-pass has to pass the highest of
    // them, as the rule's "higher of the two" passes RX2, or a third slice
    // on a higher band is filtered out. Where RX2 has its own front end
    // (rx2PreampPresent) RX1 still decides alone.
    //
    // RF-SAFETY: the gate is load-bearing. setAlexLPF writes BOTH words while
    // keyed (SetAlexLPFBits `isMox || ...`), so a receive selection reaching
    // it mid-transmission would put the receive frequency's low-pass on the
    // transmitter. Thetis never calls it keyed; neither does this.
    if (!m_mox) {
        applyReceiveAlexLpf();
    }
}

// ---------------------------------------------------------------------------
// applyReceiveAlexLpf: UpdateAlexTXFilter's receive-frequency selection
// (see recomputeReceiveFilters for the rule). Unkeyed callers only.
// ---------------------------------------------------------------------------
void P2RadioConnection::applyReceiveAlexLpf()
{
    // Shared-input filters, ruling (c) 2026-09-30: the low-pass follows the
    // highest receiver among the slices the model counted on ADC0's input
    // (AlexRxBpf::countedSlotsAdc0), the same set the band-pass was chosen
    // over. That is Thetis's "higher of the two" generalised to every slice
    // that shares the input:
    //   From Thetis console.cs:15491-15495 UpdateAlexTXFilter [v2.10.3.15]
    //     if (!_rx2_preamp_present && chkRX2.Checked)
    //     {
    //         if (rx1_dds_freq_mhz > rx2_dds_freq_mhz) setAlexLPF(rx1_dds_freq_mhz, false);
    //         else setAlexLPF(rx2_dds_freq_mhz, false);
    //     }
    // Thetis gates the rule on the board flag _rx2_preamp_present; here the
    // counted set is already per input, so a receiver on its own front end
    // is simply not in it. With nothing counted the RX1 stand-in rule below
    // stands, as the band-pass falls back to its frequency-derived bits.
    //
    // The frequency compared is each DDC's centre, Thetis's DDS frequency
    // (SharedInputLowPass::Rule::HighestCentre), and the choice is the one
    // RadioModel names in the low-pass reason.
    {
        const quint32 counted = (m_liveSlotMask != 0)
            ? (m_countedSlotsAdc0 & m_liveSlotMask) : m_countedSlotsAdc0;
        QList<SharedInputLowPass::Candidate> candidates;
        for (int ddc = 0; ddc < kMaxRxStreams; ++ddc) {
            if ((counted & (1u << ddc)) == 0) { continue; }
            const int hz = m_rx[static_cast<size_t>(ddc)].frequency;
            candidates.append({ddc, hz > 0 ? static_cast<quint64>(hz) : 0, 0});
        }
        const int best = SharedInputLowPass::highest(
            SharedInputLowPass::Rule::HighestCentre, candidates);
        if (best >= 0) {
            applyAlexLpf(double(candidates.at(best).centreHz) / 1e6, /*freqIsTx=*/false);
            return;
        }
    }

    const int rx1 = rx1Ddc();
    const int rx1Hz = m_rx[static_cast<size_t>(rx1)].frequency;
    if (rx1Hz <= 0) {
        return;
    }
    bool rx2Live = false;
    double rx2Mhz = 0.0;
    for (int ddc = 0; ddc < kMaxRxStreams; ++ddc) {
        if (ddc == rx1 || (m_liveSlotMask & (1u << ddc)) == 0) { continue; }
        const int hz = m_rx[static_cast<size_t>(ddc)].frequency;
        if (hz <= 0) { continue; }
        rx2Live = true;
        rx2Mhz = std::max(rx2Mhz, hz / 1e6);
    }
    applyAlexLpf(NereusSDR::codec::alex::receiveLpfFrequencyMhz(
                     rx1Hz / 1e6, rx2Mhz, rx2Live,
                     m_caps ? m_caps->rx2PreampPresent : false),
                 /*freqIsTx=*/false);
}

// ---------------------------------------------------------------------------
// applyAlexLpf: Thetis's setAlexLPF on the two words.
// From Thetis console.cs:7177-7243 [v2.10.3.15]
//   if (!_mox && lpf_bypass) { NetworkIO.SetAlexLPFBits(0x10, false, _mox); ... }
//   if (alexpresent && !initializing) { ... SetAlexLPFBits(bits, freqIsTX, _mox); }
// Every Protocol 2 board carries the Alex words, so alexpresent holds.
// ---------------------------------------------------------------------------
void P2RadioConnection::applyAlexLpf(double freqMhz, bool freqIsTx)
{
    NereusSDR::codec::alex::AlexLpfMasks masks{
        static_cast<quint8>(m_alex.lpfBitsRx), static_cast<quint8>(m_alex.lpfBitsTx)};
    NereusSDR::codec::alex::setAlexLpf(masks, freqMhz, freqIsTx, m_mox,
                                       m_alexLpfBypass, /*alexPresent=*/true,
                                       m_alexLpfEdges);
    m_alex.lpfBitsRx = masks.alex0;
    m_alex.lpfBitsTx = masks.alex1;
    publishAlexLpfBits(effectiveLpfBitsAlex0());
}

void P2RadioConnection::setTxFrequency(quint64 frequencyHz)
{
    m_tx[0].frequency = static_cast<int>(frequencyHz);

    // The transmit low-pass, and the only thing allowed to select it.
    //   From Thetis console.cs:15464-15468 UpdateTXDDSFreq [v2.10.3.15]
    //     private void UpdateTXDDSFreq()
    //     { if (initializing) return;
    //       setAlexLPF(tx_dds_freq_mhz, true); ... }
    // Upstream inline attribution preserved verbatim (console.cs:15471):
    //   if (MOX)//[2.10.3.13]MW0LGE
    //   From Thetis ChannelMaster/netInterface.c:682-726 [v2.10.3.15]
    //     if (isMox || isTX) -> Alex1LPFMask  (Alex1)
    //
    // tx_dds_freq_mhz is the TX VFO's frequency: VFO B under split
    // (console.cs:32867, the chkVFOBTX arm) else VFO A (console.cs:31891,
    // guarded by `!chkVFOBTX.Checked`), with XIT already folded in
    // (console.cs:31782-31784 `if (chkXIT.Checked) tx_freq += udXIT`) and
    // RIT deliberately excluded — RIT moves rx_freq only. NereusSDR's
    // caller applies the same rule from the TX-bound slice.
    //
    // Unlike the receive path this is NOT gated on MOX: Thetis re-drives
    // UpdateTXDDSFreq on both MOX edges (console.cs:29099 + 29148
    // HdwMOXChanged [v2.10.3.15]), so the transmit selection is kept live
    // whether the radio is keyed or not.
    // setAlexLPF(tx_dds_freq_mhz, true) over the saved rows: Alex1 unkeyed,
    // both words keyed, and 6m/ByPass on RX on Alex0 while unkeyed.
    const int oldLpfBitsTx = m_alex.lpfBitsTx;
    applyAlexLpf(static_cast<double>(frequencyHz) / 1e6, /*freqIsTx=*/true);
    const int newLpfBitsTx = m_alex.lpfBitsTx;
    if (newLpfBitsTx != oldLpfBitsTx) {
        // The one line that makes the transmit low-pass observable on a bench.
        // Logged on change only, so it marks the event rather than the
        // cadence. Same shape as setAlexRxBpf's line above.
        qCDebug(lcConnection) << "P2::setTxFrequency txLpf="
                              << Qt::hex << newLpfBitsTx << Qt::dec
                              << "for" << bandLabel(bandFromFrequency(
                                     static_cast<double>(frequencyHz)))
                              << "tx=" << frequencyHz << "Hz";
    }

    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setActiveReceiverCount(int count)
{
    // ── Phase 3F Sub-Epic I closeout: the codec owns the enable mask ──────
    //
    // A receiver COUNT cannot name DDCs. The DDC0..N-1 range below is only
    // ever right on a board whose user DDCs start at 0, and every 2-ADC P2
    // SKU reserves DDC0/DDC1 for the PureSignal / diversity pair and puts
    // slice A on DDC2 (P2CodecSaturn stream table; Thetis
    // console.cs:8244-8245 [v2.10.3.15]). Once the per-board codec has
    // computed a mask for this session it is the only writer, so this
    // returns before touching a single enable bit.
    //
    // Upstream authority is quoted in full on m_ddcMaskOwnedByCodec's
    // declaration: Thetis writes the mask in exactly one place,
    // NetworkIO.EnableRxs(DDCEnable) at console.cs:8537 [v2.10.3.15], from
    // UpdateDDCs's per-board switch, and DERIVES the receiver count from
    // the mask inside EnableRxs (netInterface.c:1229-1236 [v2.10.3.15]).
    //
    // Ownership, not ordering: publishDdcAssignment's activateReceiver()
    // reaches here through rebuildHardwareMapping ->
    // hardwareReceiverCountChanged, but so does any other rebuild trigger
    // (destroyReceiver, deactivateReceiver, setDdcMapping, reset). Gating
    // on the owner rather than on call order is what makes every one of
    // them safe.
    //
    // Before the codec speaks, the count-derived mask stays exactly as it
    // was: that is the pre-connect seed the P2 wire-lock baseline
    // (tests/data/p2_baseline_bytes.json, CmdRx byte 7) is frozen against,
    // and on the live path connectToRadio's board-aware
    // m_rx[primaryDdc].enable = 1 bootstrap is what actually starts the
    // stream.
    if (m_ddcMaskOwnedByCodec) {
        return;
    }

    // Clamp to board-reported maximum if caps are available.
    // kMaxRxStreams (12) is the wire-protocol ceiling; board caps may be lower.
    //
    // This is a wire value, so it stays on the board table rather than
    // BoardCapsTable::effectiveReceiverCount. Thetis reads the radio's
    // receiver count from discovery (From Thetis
    // HPSDR/clsRadioDiscovery.cs:1194 [v2.10.3.15], r.NumRxs = data[20])
    // and keeps it for its radio list only (From Thetis
    // ucRadioList.cs:633 [v2.10.3.15], item.RadioNumRxs = radio.NumRxs);
    // its DDC enables come from UpdateDDCs's per-model switch
    // (console.cs:8537 [v2.10.3.15], quoted above).
    const int maxRx = m_caps ? m_caps->maxReceivers : kMaxRxStreams;
    const int clamped = qBound(1, count, maxRx);
    for (int i = 0; i < kMaxRxStreams; ++i) {
        m_rx[i].enable = (i < clamped) ? 1 : 0;
    }
    if (m_running) {
        sendCmdRx();
    }
}

void P2RadioConnection::setSampleRate(int sampleRate)
{
    // From Thetis: sampling_rate stored as kHz value (48, 96, 192, 384).
    //
    // Thetis console.cs:8537-8540 [v2.10.3.15] writes Rate[0..3] only
    // (`for (int i = 0; i < 4; i++) NetworkIO.SetDDCRate(i, Rate[i])`).
    // RX4..RX6 are NEVER touched by UpdateDDCs so they stay at the
    // `prn->rx[i].sampling_rate = 48` default set by create_rnet
    // (netInterface.c:1488).  Wire-capture of Thetis-on-ANAN-G2E confirms
    // RX4/5/6 sample_rate = 48 kHz throughout the session.
    //
    // Within RX0..RX3: Thetis's Rate[] is a fresh int[8] (default 0) each
    // UpdateDDCs call, then the per-board switch sets only the active
    // entries.  So inactive-but-in-nddc slots end up at 0 on the wire.
    //
    // History note: a 2026-05-22 attempt to gate this on `m_rx[i].enable`
    // and write 0 to disabled slots was a misdiagnosis — the G2E
    // connect-unblock came from the primary-DDC fix, not the rate-zero
    // gate.  G2E firmware accepts the Thetis-faithful 48 on RX4..6 fine
    // (proven by Thetis wire capture on the same radio).  Writing 0 on
    // RX4..6 may be what's blocking PureSignal calcc convergence by
    // perturbing the firmware's PS-engaged DDC validation.
    int rateKhz = sampleRate / 1000;

    // 2026-05-23 review fix (PR #280 review): the prior bound at 4 only
    // covered the Hermes-class nddc=4 boards.  Saturn / OrionMKII / Angelia
    // have nddc up to 7, and setActiveReceiverCount() already enables up to
    // m_caps->maxReceivers.  If the user enables RX4+ at, say, 192 kHz, the
    // old loop left those slots advertising 48 kHz on the wire while the
    // app-side receiver was configured for 192 kHz.
    //
    // Fix: walk all kMaxDdc (7) slots, write the requested rate ONLY to
    // enabled slots, and leave disabled slots at their constructor-default
    // 48 kHz so we don't reintroduce the zero-write regression noted above.
    // This matches Thetis byte 43/49/55 = 48 kHz for inactive slots while
    // correctly tracking the requested rate for any active RX4+ user.
    for (int i = 0; i < kMaxDdc; ++i) {
        if (m_rx[i].enable) {
            m_rx[i].samplingRate = rateKhz;
        }
        // disabled slots: leave constructor 48 kHz default untouched
    }

    m_tx[0].samplingRate = rateKhz;
    if (m_running) {
        sendCmdRx();
        sendCmdTx();
    }
}

void P2RadioConnection::setAttenuator(int dB)
{
    // From Thetis: prn->adc[0].rx_step_attn
    // Clamp to board-specific attenuator range from BoardCapabilities.
    // Saturn/SaturnMKII: minDb=0, maxDb=31, stepDb=1 (kSaturn in BoardCapabilities.cpp).
    // Fallback to [0, 31] if m_caps is not yet set (should not occur in normal flow).
    const int minDb = m_caps ? m_caps->attenuator.minDb : 0;
    // Level Cal: an Alex board's wire range reaches the Alex range + 2
    // (console.cs:11044-11056 [v2.10.3.15]).
    const int maxDb = m_caps ? BoardCapsTable::stepAttWireMaxDb(*m_caps) : 31;
    m_adc[0].rxStepAttn = qBound(minDb, dB, maxDb);
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setAlexAtten(int bits)
{
    // Level Cal: the Alex receive attenuator, Alex0 bits 13 / 14.
    // From Thetis ChannelMaster/netInterface.c:421-432 [v2.10.3.15]:
    //   void SetAlexAtten(int bits)
    //   {
    //       if (mkiibpf) return;
    //
    //       if ((prbpfilter->_20_dB_Atten | prbpfilter->_10_dB_Atten) != bits)
    //       {
    //           prbpfilter->_20_dB_Atten = (bits & 0x2) == 0x2;
    //           prbpfilter->_10_dB_Atten = bits & 0x1;
    //           if (listenSock != INVALID_SOCKET)
    //               CmdHighPriority();
    // (The OR compare drops a change to 1 from 2 or 3, as in Thetis.)
    if (m_hardwareProfile.mkiiBpf) {
        return;
    }
    const int current = (m_alex.atten20dB ? 1 : 0) | (m_alex.atten10dB ? 1 : 0);
    if (current == bits) {
        return;
    }
    m_alex.atten20dB = (bits & 0x2) == 0x2;
    m_alex.atten10dB = (bits & 0x1) != 0;
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setAttenuatorForAdc(int adc, int dB)
{
    // R-R3-46 / R-R3-11: one ADC's receive step attenuator, sent in
    // CmdHighPriority (bytes 1443 ADC0, 1442 ADC1), as Thetis sets it:
    // From Thetis netInterface.c:860-868 [v2.10.3.15]:
    //   void SetADC2StepAttenData(int data)
    //   {
    //       if (prn->adc[1].rx_step_attn != data)
    //       {
    //           prn->adc[1].rx_step_attn = data;
    //           if (listenSock != INVALID_SOCKET && prn->sendHighPriority != 0)
    //               CmdHighPriority();
    if (adc == 0) {
        setAttenuator(dB);
        return;
    }
    if (adc < 1 || adc >= kMaxAdc) {
        return;
    }
    const int minDb = m_caps ? m_caps->attenuator.minDb : 0;
    // Level Cal: an Alex board's wire range reaches the Alex range + 2
    // (console.cs:11044-11056 [v2.10.3.15]).
    const int maxDb = m_caps ? BoardCapsTable::stepAttWireMaxDb(*m_caps) : 31;
    m_adc[static_cast<size_t>(adc)].rxStepAttn = qBound(minDb, dB, maxDb);
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setPreamp(bool enabled)
{
    // From Thetis: prn->rx[0].preamp
    m_rx[0].preamp = enabled ? 1 : 0;
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setRx1Preamp(bool enabled)
{
    // Phase 3P-B Task 10: per-ADC RX1 preamp for OrionMKII family.
    // Routes to byte 1403 bit 1 in CmdHighPriority via buildCodecContext():
    //   ctx.p2Rx1Preamp = (m_rx[1].preamp != 0)
    // P2CodecOrionMkII::composeCmdHighPriority:
    //   buf[1403] = p2Rx1Preamp << 1 | rxPreamp[0]
    m_rx[1].preamp = enabled ? 1 : 0;
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setTxDrive(int level)
{
    // From Thetis: prn->tx[0].drive_level
    m_tx[0].driveLevel = qBound(0, level, 255);
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setMox(bool enabled)
{
    // Once a generation is terminal, late queued control work may still call
    // this slot. It must neither emit another packet nor latch keyed state
    // that a later connection generation could inherit.
    if (enabled && m_linkLossLatched) {
        return;
    }

    // Guard idempotent transitions: the 100 ms high-priority periodic cadence
    // already re-emits the current m_mox state on every tick, so there is no
    // need to force an extra packet when the value is unchanged.  This matches
    // P1 setMox (which uses m_forceBank0Next for its safety effect and early-
    // returns on idempotent) and deskhpsdr's model where new_protocol_high_
    // priority() fires only when the transmit state actually transitions.
    //
    // From deskhpsdr/src/new_protocol.c:739-762 [@120188f]:
    //   high_priority_buffer_to_radio[4] = P2running;   // bit 0 = run
    //   if (xmit) { high_priority_buffer_to_radio[4] |= 0x02; }  // bit 1 = MOX
    //
    // m_mox drives bit 1 (0x02) of byte 4 in composeCmdHighPriority.
    // m_tx[0].pttOut remains for the rear-panel PTT-out relay (TX-confirmation
    // output, deferred to 3M-3 per the plan); it is NOT the MOX source here.
    if (m_mox == enabled) {
        return;  // idempotent — see the MOX-off grace window below
    }
    // On MOX engage, arm the TX I/Q ring pre-prime flag.  The next sendTxIq
    // (which runs on the TX worker thread) will push a 20 ms cushion of
    // zero samples ahead of its real first-block data, giving the send
    // thread a cushion before the producer needs to keep pace.  Closes the 2% zero-padded TX gap observed on the
    // bench 2026-05-26.  Single-writer safety: only sendTxIq mutates the
    // ring; the flag is the cross-thread handshake.
    if (enabled) {
        m_txIqPrimePending.store(true, std::memory_order_release);
        // R-IOS-13, R-R3-42: the send path's counters run per key.
        resetTxSendStats();
    } else if (m_txMicSource != nullptr) {
        // TX diagnostics lane: the pump's wakes are timed during the key.
        m_txMicSource->wakeWatch().end();
    }
    m_mox = enabled;
    if (!enabled) {
        // 2026-07-27 (Codex review, PR #306): open a bounded grace window so
        // the heartbeat keeps re-emitting MOX-off for a short while after
        // unkey.
        //
        // The idempotent early-return above was justified by the 100 ms
        // cadence "already re-emitting the current m_mox state on every tick"
        // — but that cadence is now gated on m_mox, so it stops the instant
        // MOX drops.  Without this window a single lost MOX-off datagram
        // leaves the radio keyed: repeat setMox(false) calls early-return
        // without sending, and the CmdGeneral keepalive keeps feeding the
        // firmware deadman so it will not self-clear either.  A stuck
        // transmitter is a PA and interference hazard, so cover transient
        // loss with retransmissions rather than a single unacknowledged frame.
        //
        // Monotonic deadline (Codex review, PR #306): a wall-clock window can
        // be stepped shut by NTP mid-unkey, which would strand a lost MOX-off
        // frame unretransmitted.  Qt::PreciseTimer because this bounds a
        // transmitter-safety window.
        m_moxOffGrace = QDeadlineTimer(
            std::chrono::milliseconds(kMoxOffGraceMs), Qt::PreciseTimer);
    }

    // The low-pass on both MOX edges, as HdwMOXChanged re-drives it:
    //   From Thetis console.cs:29097-29099 (key) and 29146-29148 (unkey)
    //   [v2.10.3.15]
    //     UpdateRX1DDSFreq();   // -> UpdateAlexTXFilter, if (!_mox) only
    //     UpdateRX2DDSFreq();
    //     UpdateTXDDSFreq();    // -> setAlexLPF(tx_dds_freq_mhz, true)
    // Keyed, the transmit selection goes to both words; unkeyed, the receive
    // selection returns to Alex0 and the transmit one stays on Alex1.
    if (!m_mox) {
        applyReceiveAlexLpf();
    }
    if (m_tx[0].frequency > 0) {  // Thetis's tx_dds_freq_mhz is always set
        applyAlexLpf(m_tx[0].frequency / 1.0e6, /*freqIsTx=*/true);
    }
    publishAlexLpfBits(effectiveLpfBitsAlex0());

    if (m_running) {
        sendCmdHighPriority();  // immediate emit on state change for low latency
    }
}

// True while the post-unkey grace window is open.  See setMox().
bool P2RadioConnection::withinMoxOffGrace() const
{
    // Default-constructed QDeadlineTimer is already expired, so the
    // never-armed case needs no separate sentinel check.
    return !m_moxOffGrace.hasExpired();
}

// ---------------------------------------------------------------------------
// setAntennaRouting — Phase 3P-I-a + 3P-I-b (T5)
//
// Ports Thetis ChannelMaster/netInterface.c:459-485 SetAntBits. Alex0
// (RX) and Alex1 (TX) register encoding per network.h:263-358:
//   Alex0 bits 24-26: _ANT_1/_ANT_2/_ANT_3 (from trxAnt)
//   Alex0 bits  8-11: _XVTR_Rx_In / _Rx_2_In / _Rx_1_In / _Rx_1_Out
//                     (from rxOnlyAnt 3/2/1 + rxOut)
//   Alex1 bits 24-26: _TXANT_1/_TXANT_2/_TXANT_3 (from txAnt)
//
// Encoding of rxOnlyAnt (bit-pair per netInterface.c:479-481 [@501e3f5]):
//   rxOnlyAnt & 0x03 == 0x01 → _Rx_1_In (bit 10, EXT2)
//   rxOnlyAnt & 0x03 == 0x02 → _Rx_2_In (bit 9, EXT1)
//   rxOnlyAnt & 0x03 == 0x03 → _XVTR_Rx_In (bit 8)
//   rxOut → _Rx_1_Out (bit 11, K36 RL17 bypass relay)
//
// From Thetis HPSDR/Alex.cs:401 [v2.10.3.13 @501e3f5] —
//   NetworkIO.SetAntBits(rx_only_ant, trx_ant, tx_ant, rx_out, tx);
// MOX coupling (the `tx` arg) deferred to Phase 3M-1.
// ---------------------------------------------------------------------------
void P2RadioConnection::setAntennaRouting(AntennaRouting r)
{
    // trxAnt drives the Alex0 RX antenna; txAnt drives the Alex1 TX.
    // Clamp to 1..3 (AntennaRouting defaults to 1; 0 means "no-op write"
    // used by RadioModel when caps.hasAlex is false — we still want the
    // Alex register at zero on the wire in that case).
    auto clamp = [](int v) { return (v < 1 || v > 3) ? 0 : v; };
    m_alex.rxAnt = clamp(r.trxAnt);
    m_alex.txAnt = clamp(r.txAnt);

    // RX-only antenna mux + RX-Bypass-Out relay — Phase 3P-I-b T5.
    // From Thetis ChannelMaster/netInterface.c:479-481 + network.h:279-282
    // [v2.10.3.13 @501e3f5]. rxOnlyAnt 0=none, 1=Rx1In, 2=Rx2In, 3=XVTRRxIn.
    m_alex.rxOnlyAnt = (r.rxOnlyAnt < 0 || r.rxOnlyAnt > 3) ? 0 : r.rxOnlyAnt;
    m_alex.rxOut     = r.rxOut;

    qCDebug(lcConnection) << "P2::setAntennaRouting rxAnt=" << m_alex.rxAnt
                          << "txAnt=" << m_alex.txAnt
                          << "rxOnlyAnt=" << m_alex.rxOnlyAnt
                          << "rxOut=" << m_alex.rxOut
                          << "running=" << m_running;
    if (m_running) {
        sendCmdHighPriority();
    }
}

// ---------------------------------------------------------------------------
// setAlexRxBpf — Phase 3F. Per-ADC RX band-pass decision.
//
// Reported by CT1IQI on PR #293 (2026-05-31): a 2-ADC radio has two input
// filter chains, and the filter for each has to be chosen over the set of
// frequencies its ADC actually serves, not over one receiver's.
//
// Thetis solves this with two independent wire words. Alex0 (`prbpfilter`,
// bytes 1432-1435) carries ADC0's RX filter and is fed from
// setAlex1HPF(_rx1_dds_freq); Alex1 (`prbpfilter2`, bytes 1428-1431) carries
// ADC1's and is fed from setAlex2HPF(rx2_dds_freq_mhz).
//   From Thetis console.cs:15401 + 15435-15443 [v2.10.3.15]
//[2.10.3.13]MW0LGE
//   From Thetis ChannelMaster/network.c:1040-1050 [v2.10.3.15]
//   From Thetis ChannelMaster/netInterface.c:604-651 [v2.10.3.15]
//   Upstream inline attribution preserved verbatim (console.cs:15441):
//     HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
//
// setReceiverFrequency still recomputes the frequency-derived m_alex.hpfBits
// (it is the only source before any slice binds, and it is what an ADC with
// no decision falls back to), but once AlexController has an answer for a
// chain, that answer wins.
// ---------------------------------------------------------------------------
void P2RadioConnection::setAlexRxBpf(AlexRxBpf b)
{
    if (m_alex.rxHpfBitsAdc0 == b.hpfBitsAdc0
        && m_alex.rxHpfBitsAdc1 == b.hpfBitsAdc1
        && m_countedSlotsAdc0 == b.countedSlotsAdc0) {
        return;
    }
    m_alex.rxHpfBitsAdc0 = b.hpfBitsAdc0;
    m_alex.rxHpfBitsAdc1 = b.hpfBitsAdc1;

    // Shared-input filters, ruling (c): the receive low-pass follows the
    // same counted slices. Unkeyed only, as every receive-derived low-pass
    // write (console.cs:15487-15498 [v2.10.3.15], `if (!_mox)`).
    if (m_countedSlotsAdc0 != b.countedSlotsAdc0) {
        m_countedSlotsAdc0 = b.countedSlotsAdc0;
        if (!m_mox) {
            applyReceiveAlexLpf();
        }
    }

    qCDebug(lcConnection) << "P2::setAlexRxBpf adc0=" << m_alex.rxHpfBitsAdc0
                          << "adc1=" << m_alex.rxHpfBitsAdc1
                          << "running=" << m_running;
    if (m_running) {
        sendCmdHighPriority();
    }
}

// ADC0's effective RX HPF bits: AlexController's per-ADC decision when there
// is one, otherwise the frequency-derived value setReceiverFrequency keeps.
quint8 P2RadioConnection::effectiveRxHpfBitsAdc0() const
{
    return static_cast<quint8>(m_alex.rxHpfBitsAdc0 >= 0 ? m_alex.rxHpfBitsAdc0
                                                         : m_alex.hpfBits);
}

// ---------------------------------------------------------------------------
// effectiveLpfBitsAlex0 — which low-pass mask the Alex0 word carries.
//
// Alex1 is unambiguous: it is the transmit word and always carries the
// transmit low-pass. Alex0 is dual-purpose. Thetis spells out why in
// ChannelMaster/netInterface.c:676-680 [v2.10.3.15], preserved verbatim:
//   // LPF bits can be used in older radioas as part of RX filtering too.
//   // Change to protocol 2 from 4.3 onwards: TX settings are encoded in the
//   // Alex1 word to remain comparible with older hardware, the logic will be:
//   // if MOX, write settings to alex0 and alex1
//   // if not MOX, write to alex1 if a TX setting else write to alex0
//
// So on pre-4.3 hardware Alex0's low-pass is the one physically in the TX
// path, which is why it must switch to the transmit selection while keyed.
// Dropping that would put the receive band's low-pass in front of the PA on
// those radios — the same hazard this whole split exists to prevent.
//
// Thetis arrives here by re-driving on the MOX edges rather than by
// selecting at compose time:
//   From Thetis console.cs:29083-29099 HdwMOXChanged [v2.10.3.15]
//     if (tx) { ... UpdateTXDDSFreq(); }        // isMox now true -> both words
//   From Thetis console.cs:29140-29148 [v2.10.3.15]
//     UpdateRX1DDSFreq(); ... UpdateTXDDSFreq(); // MOX off -> Alex0 back to RX
// and the receive-derived write can never run while keyed because
// UpdateAlexTXFilter is wrapped in `if (!_mox)` (console.cs:15487-15498).
// Selecting here instead produces the identical wire bytes in every one of
// those states, and cannot drift out of sync if a MOX edge is ever missed.
// ---------------------------------------------------------------------------
quint8 P2RadioConnection::effectiveLpfBitsAlex0() const
{
    return static_cast<quint8>(m_mox ? m_alex.lpfBitsTx : m_alex.lpfBitsRx);
}

// ---------------------------------------------------------------------------
// setLiveReceiverSlots: which DDCs carry a live receiver.
//
// Phase 3F section 16.3.2, plan Task 14. Thetis takes the OC band from RX1
// (VFO A). Slice A can be closed in NereusSDR, so the live receiver on the
// LOWEST DDC stands in for RX1, as on Protocol 1
// (P1RadioConnection::setLiveReceiverSlots). On the G2 slice A is DDC2 and
// slice B DDC3 (P2CodecOrionMkII::applyDdcAssignment), so with A open this
// is A. The PureSignal and diversity DDC0/DDC1 pair is not a receiver and
// never appears in the mask. An empty mask keeps the previous stand-in.
// ---------------------------------------------------------------------------
void P2RadioConnection::setLiveReceiverSlots(quint32 slotMask)
{
    m_liveSlotMask = slotMask;
    if (slotMask == 0) {
        return;
    }
    int lowest = 0;
    while (lowest < 31 && (slotMask & (1u << lowest)) == 0) { ++lowest; }
    if (lowest < kMaxRxStreams) {
        m_rx1Slot = lowest;
    }
    // The stand-in or the receiver beside it may have changed.
    recomputeReceiveFilters();
    if (m_running) {
        sendCmdHighPriority();
    }
}

// ---------------------------------------------------------------------------
// setHpfBypassOnTx: "HPF Bypass on TX" (plan Task 14). Stored by the base
// and read by buildCodecContext; sent at once, because Thetis's setter
// re-applies the high-pass immediately (console.cs:18754-18762
// DisableHPFonTX [v2.10.3.15]) and a Protocol 2 high-priority packet goes
// out only when something changes.
// ---------------------------------------------------------------------------
void P2RadioConnection::setHpfBypassOnTx(bool on)
{
    if (on == m_hpfBypassOnTx) {
        return;
    }
    RadioConnection::setHpfBypassOnTx(on);
    if (m_running) {
        sendCmdHighPriority();
    }
}

// ---------------------------------------------------------------------------
// setHpfBypassOnPs: "HPF Bypass on PureSignal feedback" (plan Task 14 fix
// wave). Sent at once, as for HPF Bypass on TX: Thetis's setter re-applies
// the high-pass (console.cs:18764-18773 DisableHPFonPS [v2.10.3.15]).
// ---------------------------------------------------------------------------
void P2RadioConnection::setHpfBypassOnPs(bool on)
{
    if (on == m_hpfBypassOnPs) {
        return;
    }
    RadioConnection::setHpfBypassOnPs(on);
    if (m_running) {
        sendCmdHighPriority();
    }
}

// ---------------------------------------------------------------------------
// setAlexHpfBypass: "HPF Bypass" (plan Task 14 fix wave). Sent at once:
// Thetis's setter re-applies the high-pass (console.cs:18793-18803
// AlexHPFBypass [v2.10.3.15]).
// ---------------------------------------------------------------------------
void P2RadioConnection::setAlexHpfBypass(bool on)
{
    if (on == m_alexHpfBypass) {
        return;
    }
    RadioConnection::setAlexHpfBypass(on);
    if (m_running) {
        sendCmdHighPriority();
    }
}

// ---------------------------------------------------------------------------
// setPaDisabled: "Disable HF PA" (Setup > Transmit > Power). Thetis's
// DisablePA sets tx[0].pa and sends CmdGeneral, whose byte 58 is (!pa):
//   From Thetis ChannelMaster/netInterface.c:623-631 [v2.10.3.15]
//     void DisablePA(int bit)
//     {
//         if (prn->tx[0].pa != bit)
//         {
//             prn->tx[0].pa = bit;
//             if (listenSock != INVALID_SOCKET)
//                 CmdGeneral();
//   From Thetis ChannelMaster/network.c:903-904 [v2.10.3.15]
//     // Bits - PA, Apollo, Mercury, Clock source
//     packetbuf[58] = (!prn->tx[0].pa) & 0x01;
// The same flag leaves the Alex T/R relay open while keyed
// (P2CodecOrionMkII::buildAlex0, netInterface.c:378 SetTRXrelay), which goes
// out with the next high-priority packet, as Thetis's does.
// ---------------------------------------------------------------------------
void P2RadioConnection::setPaDisabled(bool disabled)
{
    RadioConnection::setPaDisabled(disabled);
    const int bit = disabled ? 1 : 0;
    if (m_tx[0].pa == bit) {
        return;
    }
    m_tx[0].pa = bit;
    if (m_running && m_socket) {
        sendCmdGeneral();
    }
}

// ---------------------------------------------------------------------------
// setAlexHpfEdges: the Alex tab's receive filter rows, re-selected and sent
// at once, as Thetis's per-row bypass setters re-select the high-pass
// (console.cs:18823-18833 Alex1_5BPHPFBypass { ... setAlex1HPF(freq); }
// [v2.10.3.15]).
// ---------------------------------------------------------------------------
void P2RadioConnection::setAlexHpfEdges(const codec::alex::AlexHpfEdges& edges)
{
    if (edges == m_alexHpfEdges) {
        return;
    }
    RadioConnection::setAlexHpfEdges(edges);
    recomputeReceiveFilters();
    if (m_running) {
        sendCmdHighPriority();
    }
}

// ---------------------------------------------------------------------------
// setAlexLpfEdges: the Alex-1 low-pass rows. Thetis's udAlex*LPF spinner
// handlers only keep the rows contiguous; none re-selects the low-pass
// (setup.cs:15888-15994 [v2.10.3.15]), so the rows are read by the next
// selection (a retune, a key or an unkey), keyed or not.
// ---------------------------------------------------------------------------
void P2RadioConnection::setAlexLpfEdges(const codec::alex::AlexLpfEdges& edges)
{
    RadioConnection::setAlexLpfEdges(edges);
}

// ---------------------------------------------------------------------------
// setAlexLpfBypass: 6m/ByPass on RX re-selects at once.
//   From Thetis console.cs:18775-18790 [v2.10.3.15]
//     lpf_bypass = value;
//     if (chkPower.Checked)
//     { double freq = VFOAFreq; if (_mox) freq = tx_dds_freq_mhz;
//       setAlexLPF(freq, _mox); ... txtVFOAFreq_LostFocus(...) }
// The LostFocus re-runs the receive selection, so unkeyed this is the
// receive selection over the live receivers.
// ---------------------------------------------------------------------------
void P2RadioConnection::setAlexLpfBypass(bool on)
{
    if (on == m_alexLpfBypass) {
        return;
    }
    RadioConnection::setAlexLpfBypass(on);
    if (m_mox) {
        if (m_tx[0].frequency > 0) {
            applyAlexLpf(m_tx[0].frequency / 1.0e6, /*freqIsTx=*/true);
        }
    } else {
        applyReceiveAlexLpf();
    }
    if (m_running) {
        sendCmdHighPriority();
    }
}

// ---------------------------------------------------------------------------
// setDisable6mLna: "Disable 6m LNA on RX / TX" (plan Task 14 fix wave). Sent
// at once: Thetis's setters re-apply the high-pass (console.cs:18719-18751
// Disable6mLNAonRX / Disable6mLNAonTX [v2.10.3.15]).
// ---------------------------------------------------------------------------
void P2RadioConnection::setDisable6mLna(bool onRx, bool onTx)
{
    if (onRx == m_disable6mLnaOnRx && onTx == m_disable6mLnaOnTx) {
        return;
    }
    RadioConnection::setDisable6mLna(onRx, onTx);
    if (m_running) {
        sendCmdHighPriority();
    }
}

// ---------------------------------------------------------------------------
// setReceiverVfoFrequencies: each DDC's slice VFO, for the OC band.
// ---------------------------------------------------------------------------
void P2RadioConnection::setReceiverVfoFrequencies(const QVector<quint64>& vfoHzBySlot)
{
    bool changed = false;
    for (int ddc = 0; ddc < kMaxRxStreams; ++ddc) {
        const quint64 hz = (ddc < vfoHzBySlot.size()) ? vfoHzBySlot.at(ddc) : 0;
        if (m_rxVfoHz[static_cast<size_t>(ddc)] != hz) {
            m_rxVfoHz[static_cast<size_t>(ddc)] = hz;
            changed = true;
        }
    }
    // Fix wave M3: a VFO step sends a packet only when the OC byte it
    // selects changes, as SetOCBits does. It used to send on every step.
    if (changed) {
        pushBandOutputsIfChanged();
    }
}

// ---------------------------------------------------------------------------
// ocBandFrequencyHz: the frequency whose band selects the OC outputs.
//
// Plan Task 14. The rule is Penny.cs's, which Thetis applies on every
// protocol before NetworkIO.SetOCBits hands the bits to network.c:
//   From Thetis HPSDR/Penny.cs:174-177 [v2.10.3.15]
//     if (tx && VFOBTX)
//         bits = TXABitMasks[idxb];
//     else if (tx)
//         bits = TXABitMasks[idx];
//     else bits = RXABitMasks[idx];
// Keyed: the transmitting slice's frequency plus XIT (m_tx[0], fed by
// RadioModel::pushTxFrequencyFromTxSlice, the frequency the Alex transmit
// low-pass uses). Unkeyed: the RX1 stand-in's VFO, falling back to its DDC
// centre when no VFO has been told. Thetis's band is the VFO's:
//   From Thetis console.cs:29101-29106 [v2.10.3.15] (HdwMOXChanged)
//     Band lo_band = BandByFreq(XVTRForm.TranslateFreq(VFOAFreq), rx1_xvtr_index, current_region);
//     Band lo_bandb = BandByFreq(XVTRForm.TranslateFreq(VFOBFreq), rx2_xvtr_index, current_region);
//     if (penny_ext_ctrl_enabled) //MW0LGE_21k
//     {
//         int bits = Penny.getPenny().UpdateExtCtrl(lo_band, lo_bandb, _mox, _tuning, SetupForm.TestIMD, chkExternalPA.Checked); //MW0LGE_21j
// A transmit frequency of 0 has never been pushed, and keeps the receive
// band.
// ---------------------------------------------------------------------------
quint64 P2RadioConnection::ocBandFrequencyHz() const
{
    if (m_mox && m_tx[0].frequency > 0) {
        return static_cast<quint64>(m_tx[0].frequency);
    }
    const auto slot = static_cast<size_t>(rx1Ddc());
    if (m_rxVfoHz[slot] != 0) {
        return m_rxVfoHz[slot];
    }
    return m_rx[slot].frequency > 0 ? static_cast<quint64>(m_rx[slot].frequency) : 0;
}

// ---------------------------------------------------------------------------
// composedOcByte: the OC byte buildCodecContext puts in byte 1401. Only a
// board with OC outputs (ocOutputCount, every Protocol 2 row) drives them.
// ---------------------------------------------------------------------------
quint8 P2RadioConnection::composedOcByte() const
{
    if (m_ocMatrix && m_caps && m_caps->ocOutputCount > 0) {
        const Band currentBand = bandFromFrequency(static_cast<double>(ocBandFrequencyHz()));
        return m_ocMatrix->maskFor(currentBand, m_mox);  // 3M-1a E.7: was m_tx[0].pttOut != 0
    }
    return 0;
}

// ---------------------------------------------------------------------------
// pushBandOutputsIfChanged: send a high-priority packet when the OC byte it
// would carry differs from the one last sent (plan Task 14 fix wave, M2 and
// M3). Thetis sends one exactly then:
//   From Thetis ChannelMaster/netInterface.c:399-407 [v2.10.3.15]
//     void SetOCBits(int b)
//     {
//         if (prn->oc_output != b)
//         {
//             prn->oc_output = b;
//             if (listenSock != INVALID_SOCKET && prn->sendHighPriority != 0)
//                 CmdHighPriority();
// A band change that leaves the byte alone sends nothing; the band shown
// with the byte is still updated.
// ---------------------------------------------------------------------------
void P2RadioConnection::pushBandOutputsIfChanged()
{
    if (!m_running) {
        return;
    }
    const quint8 byte = (m_useLegacyP2Codec || !m_codec) ? quint8(0) : composedOcByte();
    if (int(byte) != publishedOcByte()) {
        sendCmdHighPriority();
        return;
    }
    publishBandOutputs(byte,
                       int(bandFromFrequency(static_cast<double>(ocBandFrequencyHz()))),
                       m_mox);
}

// ---------------------------------------------------------------------------
// onBandOutputPinsChanged: a band-output pin was edited, on this window or
// from a remote one (plan Task 14 fix wave, M2). Thetis pushes a pin edit to
// the radio at once:
//   From Thetis setup.cs:12718 [v2.10.3.15] (chkPenOCrcv160_CheckedChanged)
//     console.PennyExtCtrlEnabled = chkPennyExtCtrl.Checked;  // need side effect of this to push change to native code
// whose setter ends in NetworkIO.SetOCBits (Penny.cs:134-194 ExtCtrlEnable).
// ---------------------------------------------------------------------------
void P2RadioConnection::onBandOutputPinsChanged()
{
    pushBandOutputsIfChanged();
}

// ---------------------------------------------------------------------------
// setWatchdogEnabled
//
// R-R3-49: the Network Watchdog setting (Setup > General > Options). It sets
// how long an established stream waits for data before the radio is declared
// lost: three seconds on, no limit off.
//
// From Thetis setup.cs:18024-18028 [v2.10.3.15]:
//   private void chkNetworkWDT_CheckedChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       NetworkIO.SetWatchdogTimer(Convert.ToInt32(chkNetworkWDT.Checked));
//   }
// From Thetis network.c:656 [v2.10.3.15]:
//   DWORD retVal = WSAWaitForMultipleEvents(1, &prn->hDataEvent, FALSE, prn->wdt ? 3000 : WSA_INFINITE, FALSE);
//
// Deliberate divergence (operator decision 2026-09-24): in Thetis the same
// setting also turns off the radio's own safety timer (general packet byte
// 38, network.c:897-898 [v2.10.3.15]) and the 500 ms keepalive that feeds it
// (network.c:1436 [v2.10.3.15]), and SetWatchdogTimer sends the general
// packet at once to carry the change (netInterface.c:1364-1372
// [v2.10.3.15]). NereusSDR keeps byte 38 at 1 and the keepalive running
// whatever the setting says, because a radio left keyed when the computer
// dies is a hazard. Nothing on the wire changes here, so nothing is sent.
//
// Turning the watchdog off stops a wait in progress; turning it on starts the
// wait from then, so the radio is not declared lost the moment the box is
// ticked.
// ---------------------------------------------------------------------------
void P2RadioConnection::setWatchdogEnabled(bool enabled)
{
    if (m_watchdogEnabled == enabled) {
        return;
    }
    m_watchdogEnabled = enabled;

    if (!enabled) {
        if (m_establishedSilenceTimer) {
            m_establishedSilenceTimer->stop();
        }
    } else if (m_running && !m_linkLossLatched
               && state() == ConnectionState::Connected
               && m_establishedSilenceTimer) {
        m_establishedSilenceGeneration = m_connectionGeneration;
        m_establishedSilenceDeadline = QDeadlineTimer(
            std::chrono::milliseconds(m_establishedSilenceTimeoutMs),
            Qt::PreciseTimer);
        m_establishedSilenceTimer->start(m_establishedSilenceTimeoutMs);
    }
}

// ---------------------------------------------------------------------------
// setWidebandEnabled — Phase 3F Sub-Epic F Task 1
//
// Toggle the wideband ADC stream for a specific ADC. The radio reads
// CmdGeneral byte 23 to learn which ADCs have wideband enabled; bit N
// corresponds to ADCN.
//
// Source: Thetis ChannelMaster/network.c:879 [v2.10.3.15]
//   packetbuf[23] = (char)_InterlockedAnd(&prn->wb_enable, 0xff);
//
// Idempotent: if the resulting mask is unchanged we skip the CmdGeneral
// send. When connected, the new mask is pushed to the radio via
// sendCmdGeneral so the wideband stream goes live without waiting for the
// next periodic emit.
// ---------------------------------------------------------------------------
void P2RadioConnection::setWidebandEnabled(int adcIndex, bool on)
{
    if (adcIndex < 0 || adcIndex >= 8) {
        return;
    }
    const quint8 bit = static_cast<quint8>(1u << adcIndex);
    const quint8 newMask = on
        ? static_cast<quint8>(m_wbEnableMask | bit)
        : static_cast<quint8>(m_wbEnableMask & static_cast<quint8>(~bit));
    if (newMask == m_wbEnableMask) {
        // A local request can enable capture while the model is Connecting.
        // Its Connected reconciliation still needs the actual identity even
        // if no ADC data has arrived. Acknowledge without advancing epochs or
        // sending another CmdGeneral packet.
        emit widebandCaptureStateApplied(adcIndex,
            m_wbCaptureEpochs[adcIndex]->load(std::memory_order_acquire), on);
        return;
    }
    // Nereus capture lifetime: trailing packets after an enable transition
    // cannot finish the previous burst. Publish the new identity before
    // retiring partial data; other ADCs retain their own state and identity.
    advanceWidebandCaptureEpoch(adcIndex);
    m_wbAccumulators[adcIndex]->discardPartialFrame();
    m_wbEnableMask = newMask;
    const quint64 generation = m_wbCaptureEpochs[adcIndex]->load(std::memory_order_acquire);
    emit widebandCaptureRetired(adcIndex, generation);
    // Direct observers may start a different capture while retiring this one.
    // Never announce the outer transition after such a replacement.
    if (generation == m_wbCaptureEpochs[adcIndex]->load(std::memory_order_acquire)
        && bool(m_wbEnableMask & bit) == on) {
        emit widebandCaptureStateApplied(adcIndex, generation, on);
    }
    if (m_state == ConnectionState::Connected) {
        sendCmdGeneral();
    }
}

std::shared_ptr<const std::atomic<quint64>>
P2RadioConnection::widebandCaptureEpoch(int adcIndex) const
{
    if (adcIndex < 0 || adcIndex >= int(m_wbCaptureEpochs.size())) {
        return {};
    }
    return m_wbCaptureEpochs[adcIndex];
}

void P2RadioConnection::advanceWidebandCaptureEpoch(int adcIndex)
{
    std::atomic<quint64>& epoch = *m_wbCaptureEpochs[adcIndex];
    quint64 current = epoch.load(std::memory_order_relaxed);
    for (;;) {
        const quint64 next = current == std::numeric_limits<quint64>::max()
            ? quint64(1)
            : current + quint64(1);
        if (epoch.compare_exchange_weak(current, next,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
            return;
        }
    }
}

void P2RadioConnection::advanceAllWidebandCaptureEpochs()
{
    for (int adcIndex = 0; adcIndex < int(m_wbCaptureEpochs.size()); ++adcIndex) {
        advanceWidebandCaptureEpoch(adcIndex);
    }
}

void P2RadioConnection::discardWidebandFrames()
{
    // Retire each ADC identity before touching any partial assembler state.
    // The model's separate connection epoch guards the whole radio lifetime.
    advanceAllWidebandCaptureEpochs();
    for (WidebandFrameAccumulator* accumulator : m_wbAccumulators) {
        accumulator->discardPartialFrame();
    }
    for (int adc = 0; adc < int(m_wbCaptureEpochs.size()); ++adc) {
        emit widebandCaptureRetired(adc,
            m_wbCaptureEpochs[adc]->load(std::memory_order_acquire));
    }
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// sendTxIq — 3M-1a Task E.6
//
// Porting from deskhpsdr/src/new_protocol.c:2795-2837 [@120188f]
// (new_protocol_iq_samples) — original C logic:
//
//   void new_protocol_iq_samples(int isample, int qsample) {
//     int iptr = txiq_inptr + 6 * txiq_count;
//     TXIQRINGBUF[iptr++] = (isample >> 16) & 0xFF;
//     TXIQRINGBUF[iptr++] = (isample >>  8) & 0xFF;
//     TXIQRINGBUF[iptr++] = (isample      ) & 0xFF;
//     TXIQRINGBUF[iptr++] = (qsample >> 16) & 0xFF;
//     TXIQRINGBUF[iptr++] = (qsample >>  8) & 0xFF;
//     TXIQRINGBUF[iptr++] = (qsample      ) & 0xFF;
//     txiq_count++;
//     if (txiq_count >= 240) { /* signal send thread */ }
//   }
//
//   P2 frame: 4-byte BE sequence + 240 samples × 6 bytes = 1444 bytes.
//   Float→int24 gain: 8388607.0 (0x7FFFFF).
//   Cite: deskhpsdr/src/new_protocol.h:37 [@120188f]
//     #define TX_IQ_FROM_HOST_PORT 1029
//   Cite: deskhpsdr/src/new_protocol.c:1476 [@120188f]
//     transmit_specific_buffer[16] = 0; // should be 24: TX IQ sample width 24 bits
//   Cite: deskhpsdr/src/new_protocol.c:1596 [@120188f]
//     // there are 24 bits per sample
//
// Accepts n interleaved float32 I/Q pairs [I0,Q0, I1,Q1, ...] from the WDSP
// TX channel output.  Pushes each float into the SPSC ring m_txIqRing.
// If the ring is full, the excess is lost: counted, and logged at most once a
// second (R-IOS-13, R-R3-42). The ring holds 341 ms, so only a stall longer
// than that reaches it.
//
// The send thread (serviceTxIqSend) drains pairs per frame.
//
// No HL2 CWX LSB-clear workaround needed for P2: the workaround applies to
// P1 old_protocol only (deskhpsdr/src/old_protocol.c:2441-2453 [@120188f]).
// deskhpsdr new_protocol.c has no analogous &=~1 pattern near this path.
// ---------------------------------------------------------------------------
void P2RadioConnection::sendTxIq(const float* iq, int n)
{
    if (n <= 0 || iq == nullptr) { return; }

    // First call after MOX engage: push a zero-sample cushion into the
    // ring BEFORE the real first-block I/Q.  It gives the send thread a
    // cushion while the producer settles into its 192 kHz steady-state
    // cadence (see the header).  R-IOS-13 (2026-09-27): the cushion is the
    // radio's target lead plus one frame (16.25 ms), not 20 ms: the send
    // thread moves the lead into the radio at once, and anything past it
    // would stand in the ring for the whole over as added latency (the
    // 20 ms cushion left 5 ms there, measured in tst_tx_mic_latency).
    // Safe to write here because
    // sendTxIq is the single writer to m_txIqRingWrite / m_txIqRingCount;
    // setMox(true) on the connection thread merely sets the flag.  See
    // m_txIqPrimePending declaration in the header for the full rationale.
    if (m_txIqPrimePending.exchange(false, std::memory_order_acq_rel)) {
        // 3120 sample-pairs = 16.25 ms at 192 kHz.
        constexpr int kPrimeFloats =
            2 * (TxIqPacer::kTargetLeadSamples + TxIqPacer::kSamplesPerFrame);
        // Clamp the cushion to whatever the ring can actually take.
        // Every other write in this function checks the count first;
        // this one used to add all 7680 floats unconditionally, so any
        // residue left by a fast MOX off/on cycle (the send thread
        // normally keeps the ring at ~0, but nothing guarantees it)
        // would push m_txIqRingCount past kTxIqRingCapacityFloats and
        // hand the send thread a count for samples the producer had
        // already overwritten -- garbled I/Q on the air.  Review of
        // PR #291.
        const int used = m_txIqRingCount.load(std::memory_order_acquire);
        const int primeFloats =
            std::min(kPrimeFloats,
                     std::max(0, kTxIqRingCapacityFloats - used - 2));
        if (primeFloats > 0) {
            int wp = m_txIqRingWrite.load(std::memory_order_relaxed);
            for (int i = 0; i < primeFloats; ++i) {
                m_txIqRing[wp] = 0.0f;
                wp = (wp + 1) % kTxIqRingCapacityFloats;
            }
            m_txIqRingWrite.store(wp, std::memory_order_relaxed);
            // release: publishes the zero writes above before the count
            // increment becomes visible to the connection-thread drain
            // timer.
            m_txIqRingCount.fetch_add(primeFloats, std::memory_order_release);
        }
        if (primeFloats < kPrimeFloats) {
            qCDebug(lcConnection)
                << "P2 TX I/Q pre-prime truncated to" << primeFloats
                << "floats (ring already held" << used << ")";
        }
    }

    int pushedPairs = 0;
    for (int k = 0; k < n * 2; k += 2) {
        // acquire: see the latest fetch_sub from the send thread so we
        // don't overfill after a drain.  Pair count: each sample = 2 floats.
        if (m_txIqRingCount.load(std::memory_order_acquire) >= kTxIqRingCapacityFloats - 1) {
            break;
        }

        int wp = m_txIqRingWrite.load(std::memory_order_relaxed);
        m_txIqRing[wp] = iq[k];         // I
        wp = (wp + 1) % kTxIqRingCapacityFloats;
        m_txIqRing[wp] = iq[k + 1];     // Q
        wp = (wp + 1) % kTxIqRingCapacityFloats;
        // relaxed: single writer; the release on m_txIqRingCount below
        // provides the visibility fence for the float writes.
        m_txIqRingWrite.store(wp, std::memory_order_relaxed);
        // release: publishes the float writes above before the count increment
        // is observed by the connection thread's acquire load.
        m_txIqRingCount.fetch_add(2, std::memory_order_release);
        ++pushedPairs;
    }
    // R-IOS-13, R-R3-42: the deepest the ring got this key, and what a full
    // ring refused. A full ring is the only loss on this path: counted, and
    // a warning at most once a second (never one line per block).
    const int depthPairs = m_txIqRingCount.load(std::memory_order_acquire) / 2;
    if (depthPairs > m_txIqMaxRingPairs.load(std::memory_order_relaxed)) {
        m_txIqMaxRingPairs.store(depthPairs, std::memory_order_relaxed);
    }
    if (pushedPairs < n) {
        const quint64 lost = static_cast<quint64>(n - pushedPairs);
        const quint64 total =
            m_txIqOverflowSamples.fetch_add(lost, std::memory_order_relaxed) + lost;
        const qint64 nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (nowNs - m_txIqOverflowLogNs >= 1'000'000'000LL) {
            qCWarning(lcConnection).noquote()
                << QStringLiteral("P2 transmit I/Q buffer full (%1 ms queued): %2 samples lost since the last report")
                       .arg(depthPairs * 1000 / 192000)
                       .arg(total - m_txIqOverflowLogged);
            m_txIqOverflowLogNs = nowNs;
            m_txIqOverflowLogged = total;
        }
    }
    if (pushedPairs > 0 && m_mox) {
        // Producer-side rate telemetry.  Compared against the 192 kHz
        // P2 wire rate in the perf overlay, this tells us whether the
        // upstream WDSP TXA + mic source path is keeping up.  Gated on
        // m_mox so the metric reflects only TX-engaged samples.
        PerfMonitor::instance().incTxIqProduced(pushedPairs);
    }
}

// ---------------------------------------------------------------------------
// R-IOS-13, R-R3-42: the transmit I/Q send thread.
//
// Thetis sends P2 transmit I/Q from ob_main (obbuffs.c:153-170 [v2.10.3.15]):
//
//   void ob_main (void *pargs)
//   {
//       HANDLE hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
//       if (hTask != 0) AvSetMmThreadPriority(hTask, 2);
//       else SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
//       ...
//       while (_InterlockedAnd (&a->run, 1))
//       {
//           WaitForSingleObject(a->Sem_BuffReady,INFINITE);
//           ...
//           obdata (id, a->out);
//           sendOutbound(id, a->out);
//       }
//   }
//
// sendOutbound (network.c:1250-1274 [v2.10.3.15]) converts each double to a
// 24-bit big-endian integer and sends the 240-sample frame to
// base_outbound_port + 5 (1029) through WriteUDPFrame (network.c:1377-1391
// [v2.10.3.15]).
//
// NereusSDR keeps Thetis's thread and priority but paces by the radio's
// buffer (see TxIqPacer in the header): our producer pushes 256-sample blocks
// from a pump the radio's mic packets pace, and a connection-thread stall
// holds those packets back, so sending "as each frame fills" would burst a
// stall's worth into a radio that holds 21.3 ms and drop the rest there.
// ---------------------------------------------------------------------------

double P2RadioConnection::TxIqPacer::advance(qint64 nowNs, qint64* gapNs)
{
    // From deskhpsdr new_protocol.c:2251-2259 [@f3d857c]:
    //   now = ts.tv_sec + 1.0E-9 * ts.tv_nsec;
    //   FIFO -= (now - last) * 192000.0;
    //   last = now;
    //   if (FIFO < 0.0) {
    //     //
    //     // normally this occurs at the RX-TX transition
    //     //
    //     FIFO = 0.0;
    //   }
    qint64 gap = 0;
    if (lastNs >= 0 && nowNs > lastNs) {
        gap = nowNs - lastNs;
        estimate -= static_cast<double>(gap) * kSamplesPerNs;
    }
    if (lastNs < 0 || nowNs > lastNs) {
        lastNs = nowNs;
    }
    if (gapNs != nullptr) {
        *gapNs = gap;
    }
    double dry = 0.0;
    if (estimate < 0.0) {
        dry = -estimate;
        estimate = 0.0;
    }
    return dry;
}

int P2RadioConnection::composeTxIqFrame(char* buf)
{
    // Frame: 4-byte BE sequence number + 240 samples x 6 bytes = 1444 bytes.
    // Cite: deskhpsdr/src/new_protocol.c:1945-1948 [@120188f]
    //   iqbuffer[0..3] = tx_iq_sequence BE bytes; tx_iq_sequence++;
    // The txIqFrameForTest seam calls this same composer, so the wire-byte
    // snapshot tests cover the production encoding.
    memset(buf, 0, kTxIqFrameBytes);
    writeBE32(buf, 0, m_seqTxIq++);

    // Float -> int24, clamped to +/-8388607.
    // Cite: deskhpsdr/src/new_protocol.c:2795 [@120188f]
    //   void new_protocol_iq_samples(int isample, int qsample)
    auto toInt24 = [](float v) -> int {
        const float scaled = v * 8388607.0f;
        if (scaled >= 8388607.0f)  { return  8388607; }
        if (scaled <= -8388607.0f) { return -8388607; }
        return static_cast<int>(scaled);
    };

    // Drain up to 240 samples from the ring into the payload, or zeros once
    // the ring is empty (silence, matches deskhpsdr underrun).
    // Cite: deskhpsdr/src/new_protocol.c:1950-1956, 2811-2816 [@120188f]
    //   memcpy(&iqbuffer[4], &TXIQRINGBUF[txiq_outptr], 1440);
    int underrunSamples = 0;
    for (int s = 0; s < 240; ++s) {
        int i24 = 0;
        int q24 = 0;
        // acquire: makes producer float writes (published via release
        // fetch_add on m_txIqRingCount) visible before we read m_txIqRing.
        if (m_txIqRingCount.load(std::memory_order_acquire) >= 2) {
            int rp = m_txIqRingRead.load(std::memory_order_relaxed);
            const float fI = m_txIqRing[rp];
            rp = (rp + 1) % kTxIqRingCapacityFloats;
            const float fQ = m_txIqRing[rp];
            rp = (rp + 1) % kTxIqRingCapacityFloats;
            // relaxed: single consumer; the acquire above is the fence.
            m_txIqRingRead.store(rp, std::memory_order_relaxed);
            // relaxed: the producer observes this via its acquire load.
            m_txIqRingCount.fetch_sub(2, std::memory_order_relaxed);
            i24 = toInt24(fI);
            q24 = toInt24(fQ);
        } else {
            ++underrunSamples;
        }
        // Pack 3-byte BE I, then 3-byte BE Q (two's complement via quint32).
        // Cite: deskhpsdr/src/new_protocol.c:2811-2816 [@120188f]
        const int offset = 4 + s * 6;
        const quint32 ui = static_cast<quint32>(i24);
        const quint32 uq = static_cast<quint32>(q24);
        buf[offset + 0] = static_cast<char>((ui >> 16) & 0xFF);
        buf[offset + 1] = static_cast<char>((ui >>  8) & 0xFF);
        buf[offset + 2] = static_cast<char>( ui        & 0xFF);
        buf[offset + 3] = static_cast<char>((uq >> 16) & 0xFF);
        buf[offset + 4] = static_cast<char>((uq >>  8) & 0xFF);
        buf[offset + 5] = static_cast<char>( uq        & 0xFF);
    }
    return underrunSamples;
}

// TX diagnostics lane (2026-10-01): one composed frame's padding while
// keyed, sorted by where it fell. Before the first frame carrying the TX
// channel's I/Q it is the start; a run that ends with the I/Q resuming is
// mid-key; the run still open when the figures are read is the unkey tail.
// Send thread only; measurement only.
void P2RadioConnection::noteTxIqPadding(qint64 nowNs, int underrunSamples)
{
    const int carried = TxIqPacer::kSamplesPerFrame - underrunSamples;
    if (carried > 0) {
        if (!m_txIqDiagSawBlock) {
            m_txIqDiagSawBlock = true;
            m_txIqFirstBlockAtNs.store(nowNs - m_txIqDiagKeyNs, std::memory_order_relaxed);
        }
        if (m_txIqDiagRun > 0) {
            m_txIqPadMid.fetch_add(m_txIqDiagRun, std::memory_order_relaxed);
            if (m_txIqDiagRun > m_txIqLongestMidPad.load(std::memory_order_relaxed)) {
                m_txIqLongestMidPad.store(m_txIqDiagRun, std::memory_order_relaxed);
                m_txIqLongestMidPadAtNs.store(m_txIqDiagRunStartNs - m_txIqDiagKeyNs,
                                              std::memory_order_relaxed);
            }
            m_txIqDiagRun = 0;
        }
    }
    if (underrunSamples <= 0) {
        m_txIqPadOpen.store(m_txIqDiagRun, std::memory_order_relaxed);
        return;
    }
    if (!m_txIqDiagSawBlock) {
        m_txIqPadStart.fetch_add(static_cast<quint64>(underrunSamples),
                                 std::memory_order_relaxed);
        return;
    }
    if (m_txIqDiagRun == 0) {
        m_txIqDiagRunStartNs = nowNs;
        m_txIqPadOpenAtNs.store(nowNs - m_txIqDiagKeyNs, std::memory_order_relaxed);
    }
    m_txIqDiagRun += static_cast<quint64>(underrunSamples);
    m_txIqPadOpen.store(m_txIqDiagRun, std::memory_order_relaxed);
}

int P2RadioConnection::serviceTxIqSend(qint64 nowNs, TxIqFrameSink sink, void* ctx)
{
    const bool keyed = m_mox.load();
    qint64 gapNs = 0;
    const double dry = m_txIqPacer.advance(nowNs, &gapNs);
    // TX diagnostics lane: a new key (resetTxSendStats) starts the send
    // thread's placement afresh; the key's time is its first keyed pass.
    const quint32 diagGeneration = m_txIqDiagGeneration.load(std::memory_order_acquire);
    if (diagGeneration != m_txIqDiagSeen) {
        m_txIqDiagSeen = diagGeneration;
        m_txIqDiagKeyNs = -1;
        m_txIqDiagSawBlock = false;
        m_txIqDiagRun = 0;
        m_txIqDiagRunStartNs = -1;
    }
    if (keyed && m_txIqDiagKeyNs < 0) {
        m_txIqDiagKeyNs = nowNs;
        m_txIqKeyNs.store(nowNs, std::memory_order_relaxed);
    }
    if (keyed) {
        if (gapNs > TxIqPacer::kLateWakeNs) {
            m_txIqLateWakes.fetch_add(1, std::memory_order_relaxed);
        }
        // Past the first frame of the key: the radio's buffer ran out
        // between two passes (a stall of this thread longer than the lead).
        if (dry >= TxIqPacer::kSamplesPerFrame
            && m_txIqFramesSent.load(std::memory_order_relaxed) > 0) {
            if (m_txIqRadioRanDry.fetch_add(1, std::memory_order_relaxed) == 0) {
                m_txIqFirstDryAtNs.store(nowNs - m_txIqDiagKeyNs, std::memory_order_relaxed);
                m_txIqFirstDryGapNs.store(gapNs, std::memory_order_relaxed);
            }
        }
    }

    // Frames already out this key: the first pass of a key refills the
    // radio from the cushion, which is no catch-up.
    const quint64 sentBefore = m_txIqFramesSent.load(std::memory_order_relaxed);
    int sent = 0;
    // A frame the socket refused last time goes first, unchanged (its
    // sequence number is already on it).
    if (m_txIqPending) {
        const TxIqSinkResult r = sink(ctx, m_txIqPendingFrame, kTxIqFrameBytes);
        if (r == TxIqSinkResult::Retry) {
            m_txIqSendErrors.fetch_add(1, std::memory_order_relaxed);
            return 0;
        }
        m_txIqPending = false;
        m_txIqPacer.frameSent();
        if (r == TxIqSinkResult::Sent) {
            ++sent;
        } else {
            m_txIqSendErrors.fetch_add(1, std::memory_order_relaxed);
        }
    }

    while (m_txIqPacer.roomForFrame()) {
        const int ringPairs = m_txIqRingCount.load(std::memory_order_acquire) / 2;
        // Less than a frame queued: wait for the producer unless the radio
        // is about to run dry, then send what there is and silence.
        if (ringPairs < TxIqPacer::kSamplesPerFrame && !m_txIqPacer.belowLowWater()) {
            break;
        }
        const int underrunSamples = composeTxIqFrame(m_txIqPendingFrame);
        // Only count holes while MOX is engaged: unkeyed, the radio
        // discards our TX I/Q, so a silent frame then is expected.
        if (underrunSamples > 0 && keyed) {
            PerfMonitor::instance().incTxIqUnderrun(underrunSamples);
            m_txIqZeroPadded.fetch_add(static_cast<quint64>(underrunSamples),
                                       std::memory_order_relaxed);
        }
        if (keyed) {
            noteTxIqPadding(nowNs, underrunSamples);
        }
        const TxIqSinkResult r = sink(ctx, m_txIqPendingFrame, kTxIqFrameBytes);
        if (r == TxIqSinkResult::Retry) {
            m_txIqPending = true;
            m_txIqSendErrors.fetch_add(1, std::memory_order_relaxed);
            break;
        }
        // A frame that failed for good still takes its time slot, so the
        // pacing holds and a dead socket does not spin this thread.
        m_txIqPacer.frameSent();
        if (r == TxIqSinkResult::Sent) {
            ++sent;
        } else {
            m_txIqSendErrors.fetch_add(1, std::memory_order_relaxed);
        }
    }
    if (sent > 0) {
        m_txIqFramesSent.fetch_add(static_cast<quint64>(sent), std::memory_order_relaxed);
    }
    // A normal pass sends 1-2 frames (one 5 ms tick's worth is 4); more is a
    // refill after this thread or the producer fell behind.
    if (keyed && sent > 4 && sentBefore > 0) {
        const quint64 burst = m_txIqCatchUpBursts.fetch_add(1, std::memory_order_relaxed);
        // TX diagnostics lane: the first bursts placed in time.
        if (burst < static_cast<quint64>(TxSendStats::kMaxBurstEvents)) {
            const auto i = static_cast<size_t>(burst);
            m_txIqBurstAtNs[i].store(nowNs - m_txIqDiagKeyNs, std::memory_order_relaxed);
            m_txIqBurstGapNs[i].store(gapNs, std::memory_order_relaxed);
            m_txIqBurstFrames[i].store(sent, std::memory_order_relaxed);
            m_txIqBurstEvents.store(static_cast<int>(burst) + 1, std::memory_order_release);
        }
    }
    return sent;
}

// ---------------------------------------------------------------------------
// Radio codec (2026-09-30): the receive audio stream to the radio's own
// speaker out, port 1028.
//
// Porting from Thetis ChannelMaster/network.c:1276-1294 [v2.10.3.15]
// (sendOutbound, the receive audio, id 0):
//   if (prn->lr_audio_swap)
//   {
//       double swap;
//       for (i = 0; i < 2 * prn->audio[0].spp; i += 2)
//       {
//           swap       = out[i + 0];
//           out[i + 0] = out[i + 1];
//           out[i + 1] = swap;
//       }
//   }
//   for (i = 0; i < 2 * prn->audio[0].spp; i++)
//   {
//       temp = out[i] >= 0.0 ? (short)floor(out[i] * 32767.0 + 0.5) :
//           (short)ceil(out[i] * 32767.0 - 0.5);
//       prn->OutBufp[i * 2] = (char)((temp >> 8) & 0xff);
//       prn->OutBufp[i * 2 + 1] = (char)(temp & 0xff);
//   }
//   WriteUDPFrame(id, prn->OutBufp, prn->audio[0].spp * 4);
// and network.c:1363-1373 [v2.10.3.15] (WriteUDPFrame):
//   case 0: // receiver audio
//       p = (unsigned char*)&prn->rx[0].rx_out_seq_no;
//       framebuf[0] = p[3];
//       framebuf[1] = p[2];
//       framebuf[2] = p[1];
//       framebuf[3] = p[0];
//       ++prn->rx[0].rx_out_seq_no;
//       memcpy(framebuf + 4, bufp, buflen);
//       ...
//           sendPacket(listenSock, framebuf, buflen + 4, prn->base_outbound_port + 4);// 1028);
//
// NereusSDR: the value is held to +/-32767 before the conversion, where
// Thetis's (short) cast of a sample past full scale is undefined in C++.
// ---------------------------------------------------------------------------
void P2RadioConnection::composeRadioAudioPacket(const float* lr, char* buf)
{
    const quint32 seq = m_rxOutSeqNo++;
    buf[0] = static_cast<char>((seq >> 24) & 0xFF);
    buf[1] = static_cast<char>((seq >> 16) & 0xFF);
    buf[2] = static_cast<char>((seq >> 8) & 0xFF);
    buf[3] = static_cast<char>(seq & 0xFF);
    const bool swap = m_radioAudioSwap.load(std::memory_order_relaxed);
    for (int i = 0; i < 2 * kRadioAudioSpp; ++i) {
        // The swap exchanges each pair's L and R (i ^ 1 is the other one).
        const float x = swap ? lr[i ^ 1] : lr[i];
        const double scaled = x >= 0.0f ? std::floor(double(x) * 32767.0 + 0.5)
                                        : std::ceil(double(x) * 32767.0 - 0.5);
        const auto temp = static_cast<int16_t>(std::clamp(scaled, -32767.0, 32767.0));
        buf[4 + i * 2] = static_cast<char>((temp >> 8) & 0xFF);
        buf[4 + i * 2 + 1] = static_cast<char>(temp & 0xFF);
    }
}

int P2RadioConnection::serviceRadioAudioSend(qint64 nowNs, TxIqFrameSink sink, void* ctx)
{
    // The radio's audio buffer drains at 48 kHz between passes.
    if (m_radioAudioLastNs >= 0 && nowNs > m_radioAudioLastNs) {
        m_radioAudioLead -= double(nowNs - m_radioAudioLastNs) * double(kRadioAudioRateHz) / 1.0e9;
        if (m_radioAudioLead < 0.0) {
            m_radioAudioLead = 0.0;
        }
    }
    m_radioAudioLastNs = nowNs;

    int sent = 0;
    float lr[2 * kRadioAudioSpp];
    char packet[kRadioAudioPacketBytes];
    while (m_radioAudioLead + kRadioAudioSpp <= kRadioAudioTargetLeadFrames
           && takeRadioAudio(lr, kRadioAudioSpp)) {
        composeRadioAudioPacket(lr, packet);
        // A packet the socket refuses is not retried: the next one follows
        // in its time slot, as the receive audio never waits.
        if (sink(ctx, packet, kRadioAudioPacketBytes) == TxIqSinkResult::Sent) {
            ++sent;
        } else {
            m_radioAudioSendErrors.fetch_add(1, std::memory_order_relaxed);
        }
        m_radioAudioLead += kRadioAudioSpp;
    }
    if (sent > 0) {
        m_radioAudioPacketsSent.fetch_add(quint64(sent), std::memory_order_relaxed);
    }
    return sent;
}

namespace {

// The native send on the connection socket's descriptor (the same source
// port as every other packet to the radio, as Thetis's single listenSock).
// The kernel serialises datagrams on one socket, so this thread and the
// connection thread may send on it at once. QUdpSocket binds
// QHostAddress::Any as a dual-stack IPv6 socket, so the destination is
// written in the socket's own family (an IPv4-mapped address on IPv6).
struct TxIqNativeDest {
    qintptr fd{-1};
    sockaddr_storage addr{};
    int addrLen{0};
    int lastError{0};
};

bool fillTxIqNativeDest(TxIqNativeDest* d, qintptr fd, quint32 ipv4, quint16 port)
{
    d->fd = fd;
    sockaddr_storage local{};
#ifdef Q_OS_WIN
    int localLen = static_cast<int>(sizeof(local));
    if (::getsockname(static_cast<SOCKET>(fd), reinterpret_cast<sockaddr*>(&local), &localLen) != 0) {
        return false;
    }
#else
    socklen_t localLen = sizeof(local);
    if (::getsockname(static_cast<int>(fd), reinterpret_cast<sockaddr*>(&local), &localLen) != 0) {
        return false;
    }
#endif
    if (local.ss_family == AF_INET6) {
        auto* a6 = reinterpret_cast<sockaddr_in6*>(&d->addr);
        a6->sin6_family = AF_INET6;
        a6->sin6_port = htons(port);
        // ::ffff:a.b.c.d
        unsigned char* b = reinterpret_cast<unsigned char*>(&a6->sin6_addr);
        b[10] = 0xff;
        b[11] = 0xff;
        b[12] = static_cast<unsigned char>((ipv4 >> 24) & 0xff);
        b[13] = static_cast<unsigned char>((ipv4 >> 16) & 0xff);
        b[14] = static_cast<unsigned char>((ipv4 >> 8) & 0xff);
        b[15] = static_cast<unsigned char>(ipv4 & 0xff);
        d->addrLen = static_cast<int>(sizeof(sockaddr_in6));
        return true;
    }
    if (local.ss_family == AF_INET) {
        auto* a4 = reinterpret_cast<sockaddr_in*>(&d->addr);
        a4->sin_family = AF_INET;
        a4->sin_port = htons(port);
        a4->sin_addr.s_addr = htonl(ipv4);
        d->addrLen = static_cast<int>(sizeof(sockaddr_in));
        return true;
    }
    return false;
}

P2RadioConnection::TxIqSinkResult sendTxIqNative(void* ctx, const char* frame, int len)
{
    using Result = P2RadioConnection::TxIqSinkResult;
    auto* d = static_cast<TxIqNativeDest*>(ctx);
#ifdef Q_OS_WIN
    const int rc = ::sendto(static_cast<SOCKET>(d->fd), frame, len, 0,
                            reinterpret_cast<const sockaddr*>(&d->addr), d->addrLen);
    if (rc == len) {
        return Result::Sent;
    }
    const int err = WSAGetLastError();
    d->lastError = err;
    // A full send buffer: keep the frame and try again next pass.
    return (err == WSAEWOULDBLOCK || err == WSAENOBUFS) ? Result::Retry : Result::Failed;
#else
    const ssize_t rc = ::sendto(static_cast<int>(d->fd), frame, static_cast<size_t>(len), 0,
                                reinterpret_cast<const sockaddr*>(&d->addr),
                                static_cast<socklen_t>(d->addrLen));
    if (rc == len) {
        return Result::Sent;
    }
    const int err = errno;
    d->lastError = err;
    return (err == EAGAIN || err == EWOULDBLOCK || err == ENOBUFS || err == EINTR)
        ? Result::Retry : Result::Failed;
#endif
}

} // namespace

void P2RadioConnection::startTxIqSender()
{
    stopTxIqSender();
    if (!m_socket || m_radioInfo.address.isNull()) {
        return;
    }
    bool ipv4Ok = false;
    const quint32 ipv4 = m_radioInfo.address.toIPv4Address(&ipv4Ok);
    const qintptr fd = m_socket->socketDescriptor();
    if (!ipv4Ok || fd < 0) {
        qCWarning(lcConnection) << "P2: no socket for the transmit I/Q stream; it is not sent";
        return;
    }
    m_txIqSocketFd = fd;
    m_txIqDestIpv4 = ipv4;
    // 3M-1a (2026-04-27): TX I/Q port is base + 5 (= 1029), NOT base + 4
    // (= 1028; that's the RX-audio port).
    // Source: Thetis network.c:1388 [v2.10.3.13]:
    //   sendPacket(..., prn->base_outbound_port + 5);// 1029);
    // Source: deskhpsdr/src/new_protocol.h:37 [@120188f]:
    //   #define TX_IQ_FROM_HOST_PORT 1029
    m_txIqDestPort = static_cast<quint16>(m_baseOutboundPort + 5);
    m_txIqPacer = TxIqPacer{};
    m_txIqPending = false;
    // Radio codec (2026-09-30): the receive audio to the radio's speaker
    // out goes to base + 4 (1028). From Thetis network.c:1373 [v2.10.3.15]:
    //   sendPacket(listenSock, framebuf, buflen + 4, prn->base_outbound_port + 4);// 1028);
    // Its sequence number starts at 0 once (netInterface.c:1492
    // [v2.10.3.15]: prn->rx[i].rx_out_seq_no = 0;, in create_rnet, which
    // cmaster.cs:547 calls once), so a restart of the stream carries on
    // from the last number (m_rxOutSeqNo's initialiser). The L/R swap is
    // the model's (LRAudioSwap, netInterface.c:1409-1413 [v2.10.3.15]).
    m_radioAudioDestPort = static_cast<quint16>(m_baseOutboundPort + 4);
    m_radioAudioSwap.store(m_hardwareProfile.lrAudioSwap, std::memory_order_relaxed);
    m_radioAudioLead = 0.0;
    m_radioAudioLastNs = -1;
    m_txIqSenderRun.store(true, std::memory_order_release);
    m_txIqSender.reset(QThread::create([this]() { txIqSenderMain(); }));
    m_txIqSender->setObjectName(QStringLiteral("P2TxIqSender"));
    m_txIqSender->start();
}

void P2RadioConnection::stopTxIqSender()
{
    m_txIqSenderRun.store(false, std::memory_order_release);
    if (m_txIqSender) {
        m_txIqSender->wait();
        m_txIqSender.reset();
    }
    m_txIqPending = false;
}

void P2RadioConnection::txIqSenderMain()
{
    // Priority as the TX pump has it (TxWorkerThread::run): in nereusd on
    // Linux thread placement gives this thread a core and raised priority
    // while transmitting (R-R3-41); elsewhere it runs at audio priority, as
    // Thetis's ob_main runs at "Pro Audio" (obbuffs.c:155-158 [v2.10.3.15]).
    const bool placed = ThreadPlacement::managesThreadPriority();
    AudioPriorityToken* prio = nullptr;
    if (placed) {
        ThreadPlacement::instance().registerCurrentThread(ThreadRole::TxIqSender);
    } else {
        prio = elevateAudioThreadPriority();
    }

    TxIqNativeDest dest;
    if (!fillTxIqNativeDest(&dest, m_txIqSocketFd, m_txIqDestIpv4, m_txIqDestPort)) {
        qCWarning(lcConnection) << "P2: the transmit I/Q socket has no usable address; the stream is not sent";
    }
    TxIqNativeDest audioDest;
    const bool audioDestOk =
        fillTxIqNativeDest(&audioDest, m_txIqSocketFd, m_txIqDestIpv4, m_radioAudioDestPort);
    quint64 errorsLogged = 0;
    qint64 errorLogNs = 0;

#ifdef Q_OS_WIN
    // A 1 ms wait needs a high-resolution timer on Windows (a plain Sleep(1)
    // can last a whole 15.6 ms scheduler tick, longer than the lead).
#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
                                          CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS);
#else
    HANDLE timer = nullptr;  // SDK without it: Sleep(1) below
#endif
    if (timer == nullptr) {
        qCWarning(lcConnection) << "P2: no high-resolution timer; the transmit I/Q"
                                   " send thread falls back to Sleep(1)";
    }
#endif
    while (m_txIqSenderRun.load(std::memory_order_acquire)) {
        const qint64 nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        serviceTxIqSend(nowNs, &sendTxIqNative, &dest);
        if (audioDestOk) {
            serviceRadioAudioSend(nowNs, &sendTxIqNative, &audioDest);
        }
        // A socket that refuses the stream: a warning at most once a second.
        const quint64 errors = m_txIqSendErrors.load(std::memory_order_relaxed);
        if (errors > errorsLogged && nowNs - errorLogNs >= 1'000'000'000LL) {
            qCWarning(lcConnection) << "P2: transmit I/Q send failed" << (errors - errorsLogged)
                                    << "times; last error" << dest.lastError;
            errorsLogged = errors;
            errorLogNs = nowNs;
        } else if (errors < errorsLogged) {
            errorsLogged = errors;  // reset at a key
        }
        // One pass a millisecond: a frame is 1.25 ms, the lead 15 ms.
#ifdef Q_OS_WIN
        if (timer != nullptr) {
            LARGE_INTEGER due;
            due.QuadPart = -10000;  // 1 ms, relative, in 100 ns units
            if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
                WaitForSingleObject(timer, INFINITE);
                continue;
            }
        }
        Sleep(1);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
    }
#ifdef Q_OS_WIN
    if (timer != nullptr) {
        CloseHandle(timer);
    }
#endif

    leaveAudioThreadPriority(prio);
    if (placed) {
        ThreadPlacement::instance().deregisterCurrentThread();
    }
}

void P2RadioConnection::resetTxSendStats()
{
    m_txIqFramesSent.store(0, std::memory_order_relaxed);
    m_txIqZeroPadded.store(0, std::memory_order_relaxed);
    m_txIqLateWakes.store(0, std::memory_order_relaxed);
    m_txIqCatchUpBursts.store(0, std::memory_order_relaxed);
    m_txIqRadioRanDry.store(0, std::memory_order_relaxed);
    m_txIqOverflowSamples.store(0, std::memory_order_relaxed);
    m_txIqSendErrors.store(0, std::memory_order_relaxed);
    m_txIqMaxRingPairs.store(0, std::memory_order_relaxed);
    // TX diagnostics lane: the send thread starts its placement afresh at
    // its next pass.
    m_txIqKeyNs.store(-1, std::memory_order_relaxed);
    m_txIqFirstBlockAtNs.store(-1, std::memory_order_relaxed);
    m_txIqPadStart.store(0, std::memory_order_relaxed);
    m_txIqPadMid.store(0, std::memory_order_relaxed);
    m_txIqPadOpen.store(0, std::memory_order_relaxed);
    m_txIqPadOpenAtNs.store(-1, std::memory_order_relaxed);
    m_txIqLongestMidPad.store(0, std::memory_order_relaxed);
    m_txIqLongestMidPadAtNs.store(-1, std::memory_order_relaxed);
    m_txIqFirstDryAtNs.store(-1, std::memory_order_relaxed);
    m_txIqFirstDryGapNs.store(-1, std::memory_order_relaxed);
    m_txIqBurstEvents.store(0, std::memory_order_relaxed);
    m_txIqDiagGeneration.fetch_add(1, std::memory_order_acq_rel);
    // The TX pump's wakes, timed afresh for this key.
    if (m_txMicSource != nullptr) {
        m_txMicSource->wakeWatch().begin();
    }
}

RadioConnection::TxSendStats P2RadioConnection::txSendStats() const
{
    TxSendStats st;
    st.valid = true;
    st.framesSent = m_txIqFramesSent.load(std::memory_order_relaxed);
    st.zeroPaddedSamples = m_txIqZeroPadded.load(std::memory_order_relaxed);
    st.lateWakes = m_txIqLateWakes.load(std::memory_order_relaxed);
    st.catchUpBursts = m_txIqCatchUpBursts.load(std::memory_order_relaxed);
    st.radioRanDry = m_txIqRadioRanDry.load(std::memory_order_relaxed);
    st.overflowSamples = m_txIqOverflowSamples.load(std::memory_order_relaxed);
    st.sendErrors = m_txIqSendErrors.load(std::memory_order_relaxed);
    st.maxRingMs = m_txIqMaxRingPairs.load(std::memory_order_relaxed) * 1000 / 192000;
    // TX diagnostics lane: the placement figures, in ms since the key.
    const auto msOf = [](qint64 ns) { return ns < 0 ? -1.0 : static_cast<double>(ns) / 1.0e6; };
    st.placed = true;
    st.keySteadyNs = m_txIqKeyNs.load(std::memory_order_relaxed);
    st.firstBlockAtMs = msOf(m_txIqFirstBlockAtNs.load(std::memory_order_relaxed));
    st.padStartSamples = m_txIqPadStart.load(std::memory_order_relaxed);
    st.padMidSamples = m_txIqPadMid.load(std::memory_order_relaxed);
    st.padTailSamples = m_txIqPadOpen.load(std::memory_order_relaxed);
    st.padTailAtMs = st.padTailSamples > 0
        ? msOf(m_txIqPadOpenAtNs.load(std::memory_order_relaxed))
        : -1.0;
    st.longestMidPadSamples = m_txIqLongestMidPad.load(std::memory_order_relaxed);
    st.longestMidPadAtMs = msOf(m_txIqLongestMidPadAtNs.load(std::memory_order_relaxed));
    st.firstDryAtMs = msOf(m_txIqFirstDryAtNs.load(std::memory_order_relaxed));
    st.firstDryGapMs = msOf(m_txIqFirstDryGapNs.load(std::memory_order_relaxed));
    st.burstEvents = std::min(m_txIqBurstEvents.load(std::memory_order_acquire),
                              TxSendStats::kMaxBurstEvents);
    for (int i = 0; i < st.burstEvents; ++i) {
        const auto k = static_cast<size_t>(i);
        st.bursts[k].atMs = msOf(m_txIqBurstAtNs[k].load(std::memory_order_relaxed));
        st.bursts[k].gapMs = msOf(m_txIqBurstGapNs[k].load(std::memory_order_relaxed));
        st.bursts[k].frames = m_txIqBurstFrames[k].load(std::memory_order_relaxed);
    }
    // The TX pump's longest wait for a microphone block during the key.
    if (const TxMicSource* source = m_txMicSourceForStats.load(std::memory_order_acquire)) {
        const TxMicWakeWatch::Stats wake = source->wakeWatch().stats();
        if (wake.longestGapNs >= 0) {
            st.longestWakeGapMs = static_cast<double>(wake.longestGapNs) / 1.0e6;
            st.longestWakeGapAtMs = st.keySteadyNs >= 0 && wake.gapStartSteadyNs >= 0
                ? static_cast<double>(wake.gapStartSteadyNs - st.keySteadyNs) / 1.0e6
                : 0.0;
            st.wakeGapSequenceStep = wake.sequenceStep;
        }
    }
    return st;
}

// G-05 (JJ's ruling 2026-09-28): the unkey waits for the ring to drain,
// bounded by its length. A frame takes whatever is queued once the radio's
// buffer is low (serviceTxIqSend), so the ring drains to empty; its length
// is kTxIqRingCapacityFloats / 2 pairs at 192 kHz (341.3 ms).
bool P2RadioConnection::txIqRingDrained() const
{
    return m_txIqRingCount.load(std::memory_order_acquire) < 2;
}

double P2RadioConnection::txIqRingLengthMs() const
{
    return static_cast<double>(kTxIqRingCapacityFloats / 2) * 1000.0 / 192000.0;
}

double P2RadioConnection::txIqQueuedMs() const
{
    // R-IOS-13: the ring counts floats, two per I/Q pair at 192 kHz.
    return static_cast<double>(m_txIqRingCount.load(std::memory_order_acquire)) / 2.0 * 1000.0
        / 192000.0;
}

// ---------------------------------------------------------------------------
// setTrxRelay — 3M-1a Task E.1 (stub; deferred to 3M-3)
//
// P2 T/R relay is routed via Saturn register C0=0x24 indirect writes.
// State is stored in base-class m_trxRelay so isTrxRelayEngaged() is
// available before the full P2 relay control lands in 3M-3.
//
// TODO [3M-3]: implement P2 T/R relay via Saturn register writes.
// Cite: pre-code review §7.2 (note: P2 path deferred to 3M-3).
// ---------------------------------------------------------------------------
void P2RadioConnection::setTrxRelay(bool enabled)
{
    if (m_trxRelay == enabled) {
        return;
    }
    m_trxRelay = enabled;
    // 3M-1a (2026-04-27): m_trxRelay state is encoded into Alex0 bits 27/18
    // (see buildAlex0) and goes to the radio in CmdHighPriority.  Push the
    // state immediately so the antenna switches to the TX path before the
    // first TX I/Q packet (rather than waiting up to 100 ms for the next
    // heartbeat tick).  setMox() does the same thing on the MOX bit.
    if (m_running && m_socket) {
        sendCmdHighPriority();
    }
    // TODO [3M-3]: emit Saturn register write for T/R relay (in addition
    // to the Alex0 bit, deskhpsdr does a Saturn register write for some
    // hardware variants — defer until we hit a radio that requires it).
}

// ---------------------------------------------------------------------------
// setMicBoost (3M-1b G.1)
//
// Sets the hardware mic-jack 20 dB boost preamp bit.
// Wire byte: transmit_specific_buffer[50] bit 1 (mask 0x02).
// Polarity: 1 = boost on (no inversion).
//
// Porting from deskhpsdr/src/new_protocol.c:1484-1486 [@120188f]:
//   if (mic_boost) {
//     transmit_specific_buffer[50] |= 0x02;
//   }
//
// Note: P2 bit position (bit 1 = 0x02) differs from P1 bit position
// (bit 0 = 0x01 in C2 of bank 10). Both mean "boost on = 1".
// The bit field is written via m_mic.micControl which is used in
// composeCmdTxLegacy() at buf[50] and in P2CodecOrionMkII at byte 50.
// ---------------------------------------------------------------------------
void P2RadioConnection::setMicBoost(bool on)
{
    if (m_micBoost == on) {
        return;  // idempotent — 100 ms heartbeat covers any state drift
    }
    m_micBoost = on;
    // From deskhpsdr/src/new_protocol.c:1484-1486 [@120188f]:
    //   if (mic_boost) { transmit_specific_buffer[50] |= 0x02; }
    if (on) {
        m_mic.micControl |= 0x02;
    } else {
        m_mic.micControl &= ~quint8(0x02);
    }
    if (m_running && m_socket) {
        sendCmdTx();
    }
}

// ---------------------------------------------------------------------------
// setLineIn (3M-1b G.2)
//
// Sets the hardware mic-jack line-in path bit.
// Wire byte: transmit_specific_buffer[50] bit 0 (mask 0x01).
// Polarity: 1 = line in active (no inversion).
//
// Porting from deskhpsdr/src/new_protocol.c:1480-1482 [@120188f]:
//   if (mic_linein) {
//     transmit_specific_buffer[50] |= 0x01;
//   }
//
// Note: P2 bit position (bit 0 = 0x01) differs from P1 bit position
// (bit 1 = 0x02 in C2 of bank 10). Both mean "line in active = 1".
// The bit field is written via m_mic.micControl which is used in
// composeCmdTxLegacy() at buf[50] and in P2CodecOrionMkII at byte 50.
// ---------------------------------------------------------------------------
void P2RadioConnection::setLineIn(bool on)
{
    if (m_lineIn == on) {
        return;  // idempotent — 100 ms heartbeat covers any state drift
    }
    m_lineIn = on;
    // From deskhpsdr/src/new_protocol.c:1480-1482 [@120188f]:
    //   if (mic_linein) { transmit_specific_buffer[50] |= 0x01; }
    if (on) {
        m_mic.micControl |= 0x01;
    } else {
        m_mic.micControl &= ~quint8(0x01);
    }
    if (m_running && m_socket) {
        sendCmdTx();
    }
}

// ---------------------------------------------------------------------------
// setMicTipRing (3M-1b G.3)
//
// Selects mic-jack Tip/Ring polarity.
// NereusSDR parameter convention: tipHot = true → Tip carries the mic signal.
//
// POLARITY INVERSION AT THE WIRE LAYER:
// deskhpsdr field mic_ptt_tip_bias_ring means "1 = Tip is BIAS/PTT" (i.e.
// NOT the mic).  Thetis field mic_trs carries identical semantics.
// Therefore: tipHot = true → Tip is mic → wire bit CLEAR (0)
//            tipHot = false → Tip is BIAS → wire bit SET (1)
//
// Wire byte: transmit_specific_buffer[50] bit 3 (mask 0x08), INVERTED.
//
// Porting from deskhpsdr/src/new_protocol.c:1492-1494 [@120188f]:
//   if (mic_ptt_tip_bias_ring) {
//     transmit_specific_buffer[50] |= 0x08;
//   }
//
// Note: P2 bit position (bit 3 = 0x08) differs from P1 bit position
// (bit 4 = 0x10 in C1 of bank 11). Both carry the same inverted semantics.
// The bit field is written via m_mic.micControl which is used in
// composeCmdTxLegacy() at buf[50] and in P2CodecOrionMkII at byte 50.
// ---------------------------------------------------------------------------
void P2RadioConnection::setMicTipRing(bool tipHot)
{
    if (m_micTipRing == tipHot) {
        return;  // idempotent — 100 ms heartbeat covers any state drift
    }
    m_micTipRing = tipHot;
    // POLARITY INVERSION: mic_ptt_tip_bias_ring = 1 means Tip is BIAS/PTT.
    // setMicTipRing(true) = Tip-is-mic → wire bit 3 CLEAR.
    // setMicTipRing(false) = Tip-is-BIAS → wire bit 3 SET.
    // From deskhpsdr/src/new_protocol.c:1492-1494 [@120188f]:
    //   if (mic_ptt_tip_bias_ring) { transmit_specific_buffer[50] |= 0x08; }
    if (!tipHot) {
        m_mic.micControl |= 0x08;
    } else {
        m_mic.micControl &= ~quint8(0x08);
    }
    if (m_running && m_socket) {
        sendCmdTx();
    }
}

// ---------------------------------------------------------------------------
// setMicBias (3M-1b G.4)
//
// Enables or disables hardware mic-jack phantom power (bias voltage).
// Polarity: on=true → bias enabled → wire bit SET (no inversion).
//
// Wire byte: transmit_specific_buffer[50] bit 4 (mask 0x10).
//
// Porting from deskhpsdr/src/new_protocol.c:1496-1498 [@120188f]:
//   if (mic_bias_enabled) {
//     transmit_specific_buffer[50] |= 0x10;
//   }
//
// Note: P2 bit position (bit 4 = 0x10) differs from P1 bit position
// (bit 5 = 0x20 in C1 of bank 11). Both carry the same polarity (1=on).
// The bit field is written via m_mic.micControl which is used in
// composeCmdTxLegacy() at buf[50] and in P2CodecOrionMkII at byte 50.
// ---------------------------------------------------------------------------
void P2RadioConnection::setMicBias(bool on)
{
    if (m_micBias == on) {
        return;  // idempotent — 100 ms heartbeat covers any state drift
    }
    m_micBias = on;
    // No polarity inversion: mic_bias_enabled = 1 means bias on.
    // From deskhpsdr/src/new_protocol.c:1496-1498 [@120188f]:
    //   if (mic_bias_enabled) { transmit_specific_buffer[50] |= 0x10; }
    if (on) {
        m_mic.micControl |= 0x10;
    } else {
        m_mic.micControl &= ~quint8(0x10);
    }
    if (m_running && m_socket) {
        sendCmdTx();
    }
}

// ---------------------------------------------------------------------------
// setLineInGain (Task 2.1 of P1 full-parity epic) — P2 path
//
// Bridges the new RadioConnection::setLineInGain virtual to P2's existing
// m_mic.lineInGain field, which is already plumbed onto byte 51 of
// CmdHighPriority (sendCmdTx() emits buf[51] = m_mic.lineInGain at the
// 100 ms pacer tick).  Store, clamp to 5 bits, mirror into shared base
// storage for cross-API consistency, and let the next high-priority frame
// carry the new value.
//
// Source: Thetis ChannelMaster/network.c CmdHighPriority byte 51 — line_in_gain
// (P2 mic struct already wired pre-this-task; this bridges the new virtual).
// ---------------------------------------------------------------------------
void P2RadioConnection::setLineInGain(int gain)
{
    const int clamped = qBound(0, gain, 31);
    if (m_mic.lineInGain == clamped) {
        return;  // idempotent — 100 ms heartbeat covers any state drift
    }
    m_mic.lineInGain = clamped;
    m_lineInGain = clamped;  // keep base storage in sync (debug visibility, no functional dependency)
    if (m_running && m_socket) {
        sendCmdTx();
    }
}

// ---------------------------------------------------------------------------
// setUserDigOut (Task 2.2 of P1 full-parity epic) — P2 path
//
// P1-ONLY FEATURE.  user_dig_out drives the 4 user-controllable digital pins
// on the Penny / Hermes Ctrl accessory header — a P1 hardware feature with
// no equivalent in P2.  Thetis P2 (network.c CmdHighPriority) carries no
// byte for prn->user_dig_out: the field exists in the prn struct but is
// only read by the P1 networkproto1.c case-11 emitter (line 601).
//
// This override therefore does NOT touch the wire.  It stores the value in
// the shared base m_userDigOut for symmetric-API consistency only — callers
// that want to set user_dig_out on a P2 connection get a silent no-op on
// the wire (the value is preserved in base storage for debug visibility,
// matching the P1 path's m_userDigOut semantics).
//
// Source: absence-of-byte in Thetis ChannelMaster/network.c CmdHighPriority
// frame layout — searched the P2 emitter; user_dig_out has no corresponding
// byte position.
// ---------------------------------------------------------------------------
void P2RadioConnection::setUserDigOut(quint8 dig)
{
    // Mask to low 4 bits at the API boundary — symmetric with P1.
    m_userDigOut = dig & 0x0F;
    // No wire emission: P2 has no user_dig_out byte in CmdHighPriority.
}

// ---------------------------------------------------------------------------
// setPuresignalRun (Task 2.3 of P1 full-parity epic) — P2 path
//
// PureSignal feedback DDC routing on P2 is handled by feedback DDC plumbing
// planned for 3M-4.  Thetis's `prn->puresignal_run` is a P1-bank-11 wire
// field (networkproto1.c:600 [v2.10.3.13]); P2 (CmdHighPriority) has no
// equivalent direct wire-bit because P2's PureSignal architecture routes
// the feedback signal through a dedicated DDC on a separate UDP port,
// gated by the per-DDC enable bytes — there is no single "PS running"
// byte in CmdHighPriority that mirrors the P1 bank 11 C2 bit 6.
//
// This override therefore does NOT touch the wire today.  It stores the
// flag in the shared base m_puresignalRun for symmetric-API consistency
// only — callers that wire `setPuresignalRun(true)` on a P2 connection
// get a silent no-op on the wire until 3M-4 lights up the feedback DDC
// routing, at which point this setter will gain wire side-effects.
//
// Source: absence-of-byte in Thetis ChannelMaster/network.c CmdHighPriority
// frame layout — searched the P2 emitter; puresignal_run has no
// corresponding byte position.  See PureSignal phase 3M-4 plan for the
// actual P2 feedback DDC routing implementation.
// ---------------------------------------------------------------------------
void P2RadioConnection::setPuresignalRun(bool run)
{
    if (m_puresignalRun == run) {
        return;
    }
    m_puresignalRun = run;
    // Phase 3M-4 Task 17 fix: push fresh CmdHighPriority so the radio
    // applies the byte-9..16 frequency override (DDC0/DDC1 → TX freq
    // during MOX).  From Thetis PSForm.cs:246-247 [v2.10.3.13]:
    //   NetworkIO.SetPureSignal(1);
    //   NetworkIO.SendHighPriority(1); // send the HP packet
    // Without the explicit send, the override doesn't take effect until
    // the next 100 ms keep-alive tick, leaving DDC1 staring at the wrong
    // baseband for several blocks of pscc data.
    if (m_running) {
        sendCmdHighPriority();
    }
    qCInfo(lcConnection) << "P2: setPuresignalRun(" << run
                         << ") — CmdHighPriority sent";
}

// ---------------------------------------------------------------------------
// applyDdcAssignment (Phase 3F Sub-Epic B Task 15)
//
// Receives the wire-byte map computed by a per-board 5-slice codec
// (P2CodecOrionMkII::computeDdcAssignment and friends), writes it into
// m_rx[i] state, and re-sends CmdRx so the radio reconfigures its DDCs.
//
// Mirrors Thetis console.cs:8527-8534 UpdateDDCs() [v2.10.3.15]:
//   NetworkIO.EnableRxs(ddcEnable);            // byte 7 of CmdRx
//   NetworkIO.EnableRxSync(0, syncEnable);     // byte 1363 of CmdRx
//   for (int i = 0; i < 4; i++)
//       NetworkIO.SetDDCRate(i, rate[i]);      // bytes 18-19, 24-25, ... per RX
//   NetworkIO.SetADC_cntrl1(cntrl1);          // ADC routing DDC0-3
//   NetworkIO.SetADC_cntrl2(cntrl2);          // ADC routing DDC4-7
//
// ADC routing extraction mirrors Thetis netInterface.c:949-983
// SetADC_cntrl1/SetADC_cntrl2 [v2.10.3.13].
// ---------------------------------------------------------------------------
void P2RadioConnection::applyDdcAssignment(const DdcAssignment& a)
{
    bool changed = false;

    // The per-board codec computed this mask, so from here on it owns byte 7
    // and setActiveReceiverCount must not rewrite it. Set before the writes
    // below rather than after, so it holds even on the no-change path: a
    // second slice that lands on an already-enabled DDC still transfers
    // ownership. See m_ddcMaskOwnedByCodec for the upstream evidence.
    m_ddcMaskOwnedByCodec = true;

    // From Thetis console.cs:8527 [v2.10.3.15]: NetworkIO.EnableRxs(ddcEnable).
    // ddcEnable bitmask: bit 0 = DDC0, ..., bit 6 = DDC6.
    constexpr int kMaxDdcRoutes = 7;
    for (int i = 0; i < kMaxDdcRoutes; ++i) {
        const int wantEnable = (a.ddcEnable >> i) & 0x01;
        if (m_rx[i].enable != wantEnable) {
            m_rx[i].enable = wantEnable;
            changed = true;
        }
    }

    // From Thetis console.cs:8529-8530 [v2.10.3.15]:
    //   for (int i = 0; i < 4; i++) NetworkIO.SetDDCRate(i, rate[i]);
    // DdcAssignment.rate[] is in Hz; m_rx[i].samplingRate is in kHz
    // (Thetis netInterface.c:1430-1450 [v2.10.3.15]).
    // Iterate all 8 slots; leave existing kHz value when rate==0.
    for (int i = 0; i < kMaxDdcRoutes; ++i) {
        if (a.rate[i] > 0) {
            const int wantKhz = a.rate[i] / 1000;
            if (m_rx[i].samplingRate != wantKhz) {
                m_rx[i].samplingRate = wantKhz;
                changed = true;
            }
        }
    }

    // From Thetis console.cs:8528 [v2.10.3.15]:
    //   NetworkIO.EnableRxSync(0, syncEnable);
    // The sync byte (CmdRx byte 1363) carries the sync-enable mask.
    // Store in m_rx[0].sync (composeCmdRx packs ctx.p2RxSync from there).
    if (m_rx[0].sync != static_cast<int>(a.syncEnable)) {
        m_rx[0].sync = static_cast<int>(a.syncEnable);
        changed = true;
    }

    // From Thetis netInterface.c:949-983 SetADC_cntrl1/cntrl2 [v2.10.3.15]:
    //   prn->rx[0].rx_adc = bits & 0x3;
    //   prn->rx[1].rx_adc = (bits >> 2) & 0x3;
    //   prn->rx[2].rx_adc = (bits >> 4) & 0x3;
    //   prn->rx[3].rx_adc = (bits >> 6) & 0x3;
    // adcCtrl1 covers DDC0-3, adcCtrl2 covers DDC4-6.
    {
        const int wantAdc[7] = {
            (a.adcCtrl1 >> 0) & 0x3,
            (a.adcCtrl1 >> 2) & 0x3,
            (a.adcCtrl1 >> 4) & 0x3,
            (a.adcCtrl1 >> 6) & 0x3,
            (a.adcCtrl2 >> 0) & 0x3,
            (a.adcCtrl2 >> 2) & 0x3,
            (a.adcCtrl2 >> 4) & 0x3,
        };
        for (int i = 0; i < kMaxDdcRoutes; ++i) {
            if (m_rx[i].rxAdc != wantAdc[i]) {
                m_rx[i].rxAdc = wantAdc[i];
                changed = true;
            }
        }
    }

    // Latch PS pair so the deinterleave loop can emit the source-first paired
    // signal. Mirrors Thetis cmaster.cs:533-534 [v2.10.3.15]
    // SetPSRxIdx / SetPSTxIdx convention.
    if (a.psFwdDdc != m_psFbDdc) {
        m_psFbDdc = a.psFwdDdc;
        changed = true;
    }
    if (a.psRevDdc != m_psTxMonDdc) {
        m_psTxMonDdc = a.psRevDdc;
        changed = true;
    }

    if (changed) {
        qCInfo(lcConnection) << "P2: applyDdcAssignment — ddcEnable=" << a.ddcEnable
                             << "syncEnable=" << a.syncEnable
                             << "rate[0]=" << a.rate[0]
                             << "rate[1]=" << a.rate[1]
                             << "rate[2]=" << a.rate[2]
                             << "rate[3]=" << a.rate[3]
                             << "nDdc=" << a.nDdc
                             << "psFwdDdc=" << m_psFbDdc
                             << "psRevDdc=" << m_psTxMonDdc;
        if (m_running) {
            sendCmdRx();
        }
    }
}

// ---------------------------------------------------------------------------
// setMicPTTDisabled (issue #182 — renamed from setMicPTT for parameter parity
// with Thetis MicPTTDisabled / mic_ptt_disabled storage name).
//
// Wire convention matches Thetis byte-for-byte:
//   disabled=true  → wire bit 2 SET   (firmware ignores mic-jack PTT line)
//   disabled=false → wire bit 2 CLEAR (firmware honors mic-jack PTT line)
//
// Wire byte: transmit_specific_buffer[50] bit 2 (mask 0x04), direct polarity.
//
// From Thetis console.cs:19757-19766 [v2.10.3.13+501e3f51]:
// Upstream tags preserved: //MW0LGE (from cited console.cs:19758) [v2.10.3.15]
//   private bool mic_ptt_disabled = false;        // default PTT enabled
//   public bool MicPTTDisabled {
//       set {
//           mic_ptt_disabled = value;
//           NetworkIO.SetMicPTT(Convert.ToInt32(value));
//       }
//   }
// From deskhpsdr/src/new_protocol.c:1488-1490 [@120188f]:
//   if (mic_ptt_enabled == 0) { // bit set when PTT is disabled
//     transmit_specific_buffer[50] |= 0x04;
//   }
// (deskhpsdr's mic_ptt_enabled is the inverse of mic_ptt_disabled; both
//  collapse to the same wire-bit-set-when-disabled convention.)
//
// Note: P2 bit position (bit 2 = 0x04) differs from P1 bit position
// (bit 6 = 0x40 in C1 of bank 11). Both carry the same direct semantics now.
// ---------------------------------------------------------------------------
void P2RadioConnection::setMicPTTDisabled(bool disabled)
{
    if (m_micPTTDisabled == disabled) {
        return;  // idempotent — 100 ms heartbeat covers any state drift
    }
    m_micPTTDisabled = disabled;
    // Direct polarity: disabled=true → SET bit 2; disabled=false → CLEAR bit 2.
    // From Thetis console.cs:19764 [v2.10.3.13+501e3f51]:
    //   NetworkIO.SetMicPTT(Convert.ToInt32(mic_ptt_disabled));
    if (disabled) {
        m_mic.micControl |= 0x04;
    } else {
        m_mic.micControl &= ~quint8(0x04);
    }
    if (m_running && m_socket) {
        sendCmdTx();
    }
}

// ---------------------------------------------------------------------------
// setMicXlr (3M-1b G.6)
//
// Selects between XLR balanced input (xlrJack=true) and TRS unbalanced
// input (xlrJack=false) on Saturn G2 / ANAN-G2 hardware.
// P2-only feature — P1 hardware has no XLR jack.
// P1 implementation is storage-only (no wire emission).
//
// Polarity: 1 = XLR jack selected (no inversion — parameter maps directly
//   to wire bit). Default true (Saturn G2 ships with XLR-enabled config).
//
// Wire byte: transmit_specific_buffer[50] bit 5 (mask 0x20).
//
// Porting from deskhpsdr/src/new_protocol.c:1500-1502 [@120188f]:
//   if (mic_input_xlr) {
//     transmit_specific_buffer[50] |= 0x20;
//   }
//   // Saturn G2 only
//
// Cross-reference:
//   Thetis console.cs [v2.10.3.13] — MicXLR property (Saturn-gated in setup UI).
//
// Note: This is the 6th and final byte-50 mic-control bit (G.1–G.6 complete).
//   MicState::micControl default was updated from 0x04 to 0x24 to reflect
//   m_micXlr=true default at construction (bit 5 pre-set at init).
// ---------------------------------------------------------------------------
void P2RadioConnection::setMicXlr(bool xlrJack)
{
    if (m_micXlr == xlrJack) {
        return;  // idempotent — 100 ms heartbeat covers any state drift
    }
    m_micXlr = xlrJack;
    // Polarity 1=XLR (no inversion). No inversion needed.
    // From deskhpsdr/src/new_protocol.c:1500-1502 [@120188f]:
    //   if (mic_input_xlr) { transmit_specific_buffer[50] |= 0x20; }
    if (xlrJack) {
        m_mic.micControl |= 0x20;
    } else {
        m_mic.micControl &= ~quint8(0x20);
    }
    if (m_running && m_socket) {
        sendCmdTx();
    }
}

// ---------------------------------------------------------------------------
// setTxStepAttenuation — 3M-1a Task F.2
//
// Mirrors Thetis ChannelMaster/netInterface.c:1006 SetTxAttenData(int bits)
// [v2.10.3.13]: broadcasts the TX step attenuator value to all ADCs and
// dispatches a CmdTx() so the radio applies the new value immediately.
// P2 frame layout: bytes 57-59 carry per-ADC TX step ATT
// (P2CodecOrionMkII.cpp lines 252-254).
//
// Phase 3M-4 Task 17 fix: previously this stored the value but did NOT
// send CmdTx — so PureSignal::autoAttentionTick's setAttOnTxValue(31)
// updates were silently dropped on the wire, calcc kept seeing fbLevel=438
// (PA-feedback ADC clipping), and PS calibration never converged.
// Verbatim parity with SetTxAttenData (netInterface.c:1010-1018):
//   if (prn->adc[0].tx_step_attn != bits) {
//       for (i = 0; i < MAX_ADC; i++) prn->adc[i].tx_step_attn = bits;
//       if (listenSock != INVALID_SOCKET) CmdTx();
//   }
// ---------------------------------------------------------------------------
void P2RadioConnection::setTxStepAttenuation(int dB)
{
    if (dB < 0)  { dB = 0; }
    if (dB > 31) { dB = 31; }
    if (m_adc[0].txStepAttn == dB) {
        return;  // idempotent — Thetis early-returns on unchanged value
    }
    for (auto& adc : m_adc) {
        adc.txStepAttn = dB;
    }
    if (m_running) {
        sendCmdTx();
    }
    qCInfo(lcConnection) << "P2: setTxStepAttenuation(" << dB
                         << ") — CmdTx sent";
}

// --- UDP Reception ---
// Porting from Thetis ReadUDPFrame() network.c:481
// Single socket, dispatch by source port: inport = ntohs(fromaddr.sin_port)

void P2RadioConnection::onReadyRead()
{
    // One drain is one batch: ReceiverManager holds the stamped I/Q it
    // queues to the DSP worker until iqBatchFinished, and frameReceived is
    // emitted once at the end, so the drain costs one post per stream and
    // one to the main thread rather than one of each per packet. Samples
    // and their order per stream are unchanged.
    const bool outerDrain = !m_inIqDrain;
    if (outerDrain) {
        m_inIqDrain = true;
        m_frameReceivedPending = false;
        emit iqBatchStarted();
    }
    const auto finishDrain = qScopeGuard([this, outerDrain] {
        if (!outerDrain) {
            return;
        }
        m_inIqDrain = false;
        emit iqBatchFinished();
        if (m_frameReceivedPending) {
            m_frameReceivedPending = false;
            emit frameReceived();
        }
    });
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();
        quint16 sourcePort = datagram.senderPort();

        // A closed generation may still have readyRead work queued. Drain it
        // without decoding, refreshing liveness, or reviving state.
        if (!m_running || m_linkLossLatched) {
            continue;
        }
        const quint64 datagramGeneration = m_connectionGeneration;

        // P2 payloads contain no MAC identity. The selected radio address is
        // therefore the strongest identity available at this layer; daemon
        // rediscovery owns the MAC-pinned recovery decision above us.
        if (!isSelectedSourceAddress(datagram.senderAddress())) {
            continue;
        }

        // From Thetis ReadUDPFrame:514-515
        // inport = ntohs(fromaddr.sin_port);
        // int portIdx = inport - prn->p2_custom_port_base;
        int portIdx = sourcePort - m_p2CustomPortBase;

        // Filter out spurious empty datagrams (Windows loopback from our own sends)
        if (data.isEmpty()) {
            continue;
        }

        // R-R3-32 / R-R3-49 (parity Task 6): every datagram from the radio,
        // on every port, counts toward UDP packets seen and the packet gap.
        // Atomic stores only; no lock, no allocation (RadioLinkStats).
        m_linkStats.noteDatagram(RadioLinkStats::nowUs());

        // Debug: log first 5 real packets
        static int debugCount = 0;
        if (debugCount < 5) {
            qCDebug(lcConnection) << "P2: UDP packet: port" << sourcePort
                                  << "idx" << portIdx << "size" << data.size()
                                  << "from" << datagram.senderAddress().toString();
            ++debugCount;
        }

        // From Thetis ReadUDPFrame:517-637 switch(portIdx)
        switch (portIdx) {
        case 0:  // 1025: 60 bytes - High Priority C&C data
            // From Thetis ReadUDPFrame:519-532
            if (data.size() == 60) {
                processHighPriorityStatus(data);
                noteAcceptedInboundDatagram(datagramGeneration);
            }
            break;

        case 1:  // 1026: 132 bytes — 16-bit BE mic samples (48 ksps)
            // From Thetis network.c:761-772 [v2.10.3.13]:
            //   case 1://1026: // 1440 bytes 16-bit mic samples
            //       for (i = 0, k = 0; i < prn->mic.spp; i++, k += 2)
            //           prn->TxReadBufp[2 * i] = const_1_div_2147483648_ *
            //               (double)(prn->ReadBufp[k + 0] << 24 |
            //                        prn->ReadBufp[k + 1] << 16);
            //           prn->TxReadBufp[2 * i + 1] = 0.0;
            //       Inbound(inid(1, 0), prn->mic.spp, prn->TxReadBufp);
            //
            // P2 mic.spp = 64 (netInterface.c:1458 [v2.10.3.13]).  Frame is
            // 4 bytes seq + 64 * 2 bytes samples = 132 bytes total.
            //
            // Thetis's `(b0<<24 | b1<<16) / 2^31` is equivalent to
            // `(int16)(b0<<8 | b1) / 32768` because the upper 16 bits hold
            // a sign-extended int16 — both yield the same float in [-1, 1].
            // We use the int16/32768 form to make the byte order explicit.
            if (data.size() == 132) {
                std::array<float, 64> samples{};
                quint32 micSequence = 0;
                if (decodeMicFrame132(data, samples, &micSequence) && m_txMicSource != nullptr) {
                    // TX diagnostics lane: the frame's sequence number, to
                    // place a gap in the pump's wakes (measurement only).
                    // From Thetis network.c:534-546 [v2.10.3.15]:
                    //   if (seqnum != (1 + prn->tx[0].mic_in_seq_no) && seqnum != 0)
                    //   prn->tx[0].mic_in_seq_no = seqnum;
                    m_txMicSource->wakeWatch().noteSequence(micSequence);
                    m_txMicSource->inbound(samples.data(), 64);
                    m_lastMicAt = QDateTime::currentDateTimeUtc();
                }
                noteAcceptedInboundDatagram(datagramGeneration);
            }
            break;

        case 2:  // 1027: wideband ADC0
        case 3:  // 1028: wideband ADC1
        case 4:  // 1029: wideband ADC2
        case 5:  // 1030: wideband ADC3
        case 6:  // 1031: wideband ADC4
        case 7:  // 1032: wideband ADC5
        case 8:  // 1033: wideband ADC6
        case 9:  // 1034: wideband ADC7
        {
            // From Thetis network.c:550-603 [v2.10.3.15] (wideband ADC).
            // Wire format: 1028 bytes total = 4 byte BE sequence number
            // + 1024 byte payload (512 × 16-bit BE samples). The matching
            // network.c block decodes `(readbuf[jj]<<24 | readbuf[jj+1]<<16)
            // * const_1_div_2147483648_` per sample. Thetis dispatches
            // adc_id = portIdx - 2 (network.c:561). Phase 3F Sub-Epic F
            // Task 3 forwards (seq, payload) to the matching per-ADC
            // WidebandFrameAccumulator, which folds the same state
            // machine (wait-for-seq-0 / zero-pad-on-mismatch / emit at
            // seq=31) into a 16384-sample frame and emits frameReady.
            if (data.size() != 1028) {
                break;  // check for malformed packet
            }
            const int adcId = portIdx - 2;
            if (adcId < 0 || adcId >= 8) {
                break;
            }
            // A syntactically valid packet on a disabled wideband role is not
            // accepted negotiated ingress. Without this gate, stale or stray
            // wideband UDP could keep a dead primary stream Connected forever.
            if ((m_wbEnableMask & static_cast<quint8>(1u << adcId)) == 0) {
                break;
            }
            const quint32 seq = (quint32(quint8(data[0])) << 24) |
                                (quint32(quint8(data[1])) << 16) |
                                (quint32(quint8(data[2])) << 8)  |
                                 quint32(quint8(data[3]));
            const QByteArray payload = data.mid(4);
            m_wbAccumulators[adcId]->pushPacket(int(seq), payload);
            noteAcceptedInboundDatagram(datagramGeneration);
            break;
        }

        case 10: // 1035: DDC0 I/Q
        case 11: // 1036: DDC1
        case 12: // 1037: DDC2
        case 13: // 1038: DDC3
        case 14: // 1039: DDC4
        case 15: // 1040: DDC5
        case 16: // 1041: DDC6
        {
            // From Thetis ReadUDPFrame:605-631
            if (data.size() != 1444) {
                break;  // check for malformed packet
            }
            // Shell-chrome sub-PR-2 B.1: record IQ ingress bytes for ▼ Mbps readout.
            recordBytesReceived(static_cast<qint64>(data.size()));
            int ddc = portIdx - 10;
            processIqPacket(data, ddc);
            noteAcceptedInboundDatagram(datagramGeneration);
            break;
        }

        default:
            // From Thetis ReadUDPFrame:633-635
            qCDebug(lcConnection) << "P2: Data on port" << sourcePort
                                  << "portIdx" << portIdx
                                  << "size" << data.size()
                                  << "from" << datagram.senderAddress().toString();
            break;
        }
    }
}

bool P2RadioConnection::isSelectedSourceAddress(const QHostAddress& sender) const
{
    // An Any-bound dual-stack QUdpSocket reports IPv4 peers as IPv4-mapped
    // IPv6 on some platforms (observed on macOS as ::ffff:127.0.0.1). Treat
    // only that representation as equal; genuine IPv6 peers such as ::1
    // remain distinct from an IPv4-selected radio.
    return sender.isEqual(m_radioInfo.address,
                          QHostAddress::ConvertV4MappedToIPv4);
}

// Refresh the established-stream deadline only after onReadyRead has
// accepted the selected address, negotiated role, and that role's existing
// packet validity rule. Before first DDC, the separate connect watchdog is
// authoritative and status/mic/wideband traffic cannot establish the link.
void P2RadioConnection::noteAcceptedInboundDatagram(quint64 datagramGeneration)
{
    // R-R3-49: with the Network Watchdog off the wait has no limit.
    // From Thetis network.c:656 [v2.10.3.15]:
    //   prn->wdt ? 3000 : WSA_INFINITE
    if (!m_watchdogEnabled) {
        return;
    }
    if (!m_running || m_linkLossLatched
        || datagramGeneration != m_connectionGeneration
        || state() != ConnectionState::Connected
        || !m_establishedSilenceTimer) {
        return;
    }

    m_establishedSilenceGeneration = m_connectionGeneration;
    m_establishedSilenceDeadline = QDeadlineTimer(
        std::chrono::milliseconds(m_establishedSilenceTimeoutMs),
        Qt::PreciseTimer);
    // Keep the high-rate I/Q path free of repeated timer registration. The
    // active wakeup may fire against an older deadline; its callback consults
    // the monotonic authority above and re-arms only the current remainder.
    if (!m_establishedSilenceTimer->isActive()) {
        m_establishedSilenceTimer->start(m_establishedSilenceTimeoutMs);
    }
}

void P2RadioConnection::onEstablishedSilenceTimeout()
{
    if (!m_watchdogEnabled) {
        return; // R-R3-49: watchdog off, no limit (network.c:656 [v2.10.3.15])
    }
    if (!m_running || m_linkLossLatched
        || state() != ConnectionState::Connected
        || m_establishedSilenceGeneration != m_connectionGeneration) {
        return;
    }

    // The Qt timer is a wakeup mechanism, not the clock authority. If it was
    // delivered before the monotonic deadline (for example after a refresh
    // raced an already queued timeout event), re-arm only the remainder.
    if (!m_establishedSilenceDeadline.hasExpired()) {
        const qint64 remainingMs = m_establishedSilenceDeadline.remainingTime();
        m_establishedSilenceTimer->start(
            static_cast<int>(std::max<qint64>(1, remainingMs)));
        return;
    }

    // From Thetis ChannelMaster/network.c:656-671 [v2.10.3.15]:
    //   DWORD retVal = WSAWaitForMultipleEvents(1, &prn->hDataEvent, FALSE,
    //                      prn->wdt ? 3000 : WSA_INFINITE, FALSE);
    //   if ((retVal == WSA_WAIT_FAILED) || (retVal == WSA_WAIT_TIMEOUT))
    //   {
    //       HaveSync = 0; //send console LOS
    //       SendStop();
    //       ...
    //       continue;
    //   }
    //   else
    //   {
    //       WSAEnumNetworkEvents(listenSock, prn->hDataEvent, ...);
    //       if (prn->wsaProcessEvents.lNetworkEvents & FD_READ)
    // The FD_READ event stays signalled while a datagram is waiting, so the
    // wait only times out when nothing is waiting. A Qt timer can be
    // dispatched ahead of readyRead work already queued behind a stalled
    // event loop; read what is waiting first (each accepted datagram
    // refreshes the deadline) and judge silence again afterwards.
    if (m_socket && m_socket->hasPendingDatagrams()) {
        const quint64 wakeGeneration = m_connectionGeneration;
        onReadyRead();
        // Direct observers of the signals emitted while reading may have
        // closed or replaced this connection; never act on a newer one.
        if (!m_running || m_linkLossLatched
            || m_connectionGeneration != wakeGeneration
            || state() != ConnectionState::Connected
            || m_establishedSilenceGeneration != m_connectionGeneration
            || !m_establishedSilenceTimer) {
            return;
        }
        if (!m_establishedSilenceDeadline.hasExpired()) {
            const qint64 remainingMs =
                m_establishedSilenceDeadline.remainingTime();
            m_establishedSilenceTimer->start(
                static_cast<int>(std::max<qint64>(1, remainingMs)));
            return;
        }
        // More arrived while reading, none of it accepted yet. Return to
        // the event loop and check again rather than spinning here.
        if (m_socket && m_socket->hasPendingDatagrams()) {
            m_establishedSilenceTimer->start(1);
            return;
        }
    }

    stopForEstablishedSilence();
}

void P2RadioConnection::stopForEstablishedSilence()
{
    if (!m_running || m_linkLossLatched
        || state() != ConnectionState::Connected) {
        return;
    }

    // Thetis ChannelMaster/network.c:656-667 [v2.10.3.15] sends one stop
    // after three seconds with no inbound UDP and does not reconnect. Nereus
    // first retires every socket producer, then emits the stop unkeyed, closes
    // ingress, and reports one typed terminal loss to the model/daemon layer.
    m_linkLossLatched = true;
    m_intentionalDisconnect = true;
    ++m_connectionGeneration;
    discardWidebandFrames();
    m_establishedSilenceGeneration = 0;
    m_establishedSilenceDeadline = QDeadlineTimer();

    if (m_keepAliveTimer) { m_keepAliveTimer->stop(); }
    stopTxIqSender();
    if (m_p2HeartbeatTimer) { m_p2HeartbeatTimer->stop(); }
    if (m_connectWatchdog) { m_connectWatchdog->stop(); }
    if (m_establishedSilenceTimer) { m_establishedSilenceTimer->stop(); }
    if (m_reconnectTimer) { m_reconnectTimer->stop(); }

    // Safety state must be applied before composing the terminal high-priority
    // packet: byte 4 must be exactly run=0/MOX=0. Clear the related local
    // transmit intents as well so a reused object cannot re-key itself.
    m_mox.store(false);
    m_puresignalRun = false;
    m_trxRelay = false;
    m_tx[0].pttOut = 0;
    m_txIqPrimePending.store(false, std::memory_order_release);
    m_moxOffGrace = QDeadlineTimer();
    m_running = false;

    if (m_socket && !m_radioInfo.address.isNull()) {
        sendCmdHighPriority();
        m_socket->flush();
        QThread::msleep(kStopDrainMs);
        m_socket->close();
    }

    const quint64 terminalGeneration = m_connectionGeneration;
    const QString detail =
        QStringLiteral("No accepted UDP from selected radio for %1 ms")
            .arg(m_establishedSilenceTimeoutMs);
    emit errorOccurred(RadioConnectionError::NoDataTimeout, detail);
    // errorOccurred is a public Qt signal and direct observers may tear down
    // or replace this connection synchronously. Never stamp LinkLost onto the
    // newer generation after control returns from such an observer.
    if (m_connectionGeneration == terminalGeneration && m_linkLossLatched) {
        setState(ConnectionState::LinkLost);
    }
}

// From Thetis KeepAliveLoop network.c:1417-1440
// Fires every 500ms, sends CmdGeneral when running
void P2RadioConnection::onKeepAliveTick()
{
    // R-R3-49: the keepalive general packet feeds the radio's safety timer
    // (byte 38, always on here), so it runs whatever the Network Watchdog
    // setting says. Deliberate divergence (operator decision 2026-09-24: a
    // radio left keyed when the computer dies is a hazard). Thetis stops it
    // with the setting off:
    // From Thetis network.c:1436 [v2.10.3.15]:
    //   if (prn->run && prn->wdt) CmdGeneral();
    if (m_running && !m_radioInfo.address.isNull()) {
        sendCmdGeneral();
    }

    // Phase 3M-1c TX pump v3: mic-frame LOS injection.
    // Mirrors Thetis network.c:656-667 [v2.10.3.15] (mic zero block at
    // 662 and 665): when no mic datagram has arrived for
    // kMicLosTimeoutMs, push a zero block into the TX inbound ring so the
    // worker keeps ticking through silence.
    if (m_txMicSource != nullptr && m_lastMicAt.isValid()) {
        const qint64 sinceMicMs = m_lastMicAt.msecsTo(QDateTime::currentDateTimeUtc());
        if (sinceMicMs > kMicLosTimeoutMs) {
            std::array<float, TxMicSource::kBlockFrames> zeros{};
            m_txMicSource->inbound(zeros.data(), TxMicSource::kBlockFrames);
            m_lastMicAt = QDateTime::currentDateTimeUtc();
        }
    }
}

void P2RadioConnection::onReconnectTimeout()
{
    if (!m_intentionalDisconnect && !m_radioInfo.address.isNull()) {
        qCDebug(lcConnection) << "P2: Reconnecting to" << m_radioInfo.displayName();
        connectToRadio(m_radioInfo);
    }
}

// --- Command Composers ---
// Each fills the packet buffer from current state. sendCmd* calls compose + UDP dispatch.
// Extracting compose from send allows test seams (composeCmdGeneralForTest etc.) to
// capture the exact bytes that would be sent, without requiring a live UDP socket.

// ---------------------------------------------------------------------------
// selectCodec — Phase 3P-B Task 7
//
// Builds m_codec from the physical board type. Called from connectToRadio()
// after m_caps is set, and from setBoardForTest() in unit tests.
//
// For P2, codec dispatch is on the physical board (HPSDRHW), not the logical
// model, because the wire dialect differences (Saturn BPF1 override) are
// physical-board-specific, unlike P1 where codec dispatch is on HPSDRModel.
// ---------------------------------------------------------------------------
void P2RadioConnection::selectCodec()
{
    m_codec.reset();
    m_useLegacyP2Codec = (qEnvironmentVariableIntValue("NEREUS_USE_LEGACY_P2_CODEC") == 1);
    if (m_useLegacyP2Codec) {
        qCInfo(lcConnection) << "P2: NEREUS_USE_LEGACY_P2_CODEC=1 — using pre-refactor compose path";
        return;
    }
    if (!m_caps) {
        qCWarning(lcConnection) << "P2: no caps; codec selection deferred";
        return;
    }
    using HW = HPSDRHW;
    switch (m_hardwareProfile.effectiveBoard) {
        case HW::Saturn:
        case HW::SaturnMKII:
            m_codec = std::make_unique<P2CodecSaturn>();
            break;
        // Phase 3F Sub-Epic I Task 7c: the 1-ADC Hermes-class family running
        // community P2 firmware puts rx1 on DDC0, not DDC2. Named explicitly
        // rather than left on `default:` so the dispatch reads the same way
        // primaryRxDdcForBoard does, and so a future 2-ADC SKU still lands on
        // P2CodecOrionMkII.
        //
        // From Thetis console.cs:8610-8642 [v2.10.3.15] GetDDC() P2 branch:
        //   case HPSDRHW.Hermes: // ANAN-10 ANAN-100 Heremes
        //   case HPSDRHW.HermesII: // ANAN-10E ANAN-100B HeremesII
        //   case HPSDRHW.HermesC10: // ANAN-G2E //N1GP G2E added (HermesC10)
        //       ... rx1 = 0; rx2 = 1;
        // versus console.cs:8556-8608 [v2.10.3.15] for the 2-ADC family
        // (rx1 = 2, rx2 = 3). Thetis keeps the two in separate switch cases;
        // we keep them in separate codecs.
        //
        // Without this, applyDdcAssignment inherited the 2-ADC DDC2 layout
        // while connectToRadio seeded DDC0 from primaryRxDdcForBoard, so the
        // operator's first VFO turn recomputed the assignment, dropped DDC0
        // and receive stopped. Same defect class as issue #263, one layer up.
        // HermesII (ANAN-10E / ANAN-100B) is Thetis's third 1-ADC branch:
        // same DDC placement, but nddc=2 rather than 4
        // (console.cs:8463-8464 [v2.10.3.15]).
        case HW::HermesII:
            m_codec = std::make_unique<P2CodecHermesII>();
            break;
        case HW::Hermes:
        case HW::HermesC10:  // ANAN-G2E //N1GP G2E added (HermesC10)
            m_codec = std::make_unique<P2CodecHermes>();
            break;
        default:
            m_codec = std::make_unique<P2CodecOrionMkII>();
            break;
    }
    qCInfo(lcConnection) << "P2: selected codec for board"
                         << int(m_hardwareProfile.effectiveBoard);

    // Phase 3M-4 Task 17 chunk B/E: notify subscribers (RadioModel /
    // ReceiverManager) that m_codec is now valid so they can inject it
    // into the ReceiverManager's per-board PsDdcConfig dispatch path.
    // Without this, ReceiverManager::updateDdcAssignment short-circuits
    // because m_p2Codec is null, ddcConfigChanged never fires, and the
    // PS DDC reconfig wire bytes are never sent to the radio.
    emit p2CodecChanged();
}

// ---------------------------------------------------------------------------
// setOcMatrix — Phase 3P-D Task 3
//
// Symmetric companion to P1RadioConnection::setOcMatrix.  Wires the
// RadioModel's OcMatrix so buildCodecContext() fills ctx.ocByte from
// maskFor(currentBand, mox).  No P2 codec reads ocByte yet; the field is
// populated here so Phase F P2 OC wiring can consume it without further
// changes to this class.
// ---------------------------------------------------------------------------
void P2RadioConnection::setOcMatrix(const OcMatrix* matrix)
{
    m_ocMatrix = matrix;
}

// ---------------------------------------------------------------------------
// setCalibrationController — Phase 3P-G
//
// Wires RadioModel's CalibrationController so hzToPhaseWord() multiplies
// by effectiveFreqCorrectionFactor(). When null, factor defaults to 1.0
// and output is byte-identical to pre-calibration.
//
// Source: HPSDR/NetworkIO.cs:227-249 FreqCorrectionFactor property,
//   Freq2PhaseWord(): long pw = (long)Math.Pow(2, 32) * freq / 122880000
//   (correction factor applied before Freq2PhaseWord in Thetis via the
//   VFOfreq → SetVFOfreq → Freq2PhaseWord chain) [@501e3f5]
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// decodeMicFrame132 — Phase 3M-1c TX pump v3
//
// Pure decoder for one 132-byte P2 mic frame from UDP port 1026:
//   bytes 0..3:   big-endian 32-bit sequence number
//   bytes 4..131: 64 * 16-bit big-endian signed mic samples
//
// Output is 64 float samples in [-1, 1].  Mirrors Thetis network.c:761-772
// [v2.10.3.13]; we use the int16/32768 form rather than the (b0<<24|b1<<16)/2^31
// form because the upper-16-bits-as-int16 + sign extension is identical.
// ---------------------------------------------------------------------------
bool P2RadioConnection::decodeMicFrame132(const QByteArray& data,
                                          std::array<float, 64>& outSamples,
                                          quint32* outSeq) noexcept
{
    if (data.size() != 132) {
        return false;
    }
    const auto* buf = reinterpret_cast<const uint8_t*>(data.constData());
    if (outSeq != nullptr) {
        *outSeq = (static_cast<quint32>(buf[0]) << 24) |
                  (static_cast<quint32>(buf[1]) << 16) |
                  (static_cast<quint32>(buf[2]) << 8) |
                  static_cast<quint32>(buf[3]);
    }
    for (int s = 0; s < 64; ++s) {
        const int16_t v = static_cast<int16_t>(
            (static_cast<uint16_t>(buf[4 + s * 2]) << 8) |
            static_cast<uint16_t>(buf[4 + s * 2 + 1]));
        outSamples[static_cast<size_t>(s)] = static_cast<float>(v) / 32768.0f;
    }
    return true;
}

// ---------------------------------------------------------------------------
// setTxMicSource — Phase 3M-1c TX pump v3
//
// Wires the RadioModel-owned TxMicSource into the connection.  Called by
// RadioModel::connectToRadio() unconditionally.  The pointer is non-owning.
// ---------------------------------------------------------------------------
void P2RadioConnection::setTxMicSource(TxMicSource* src)
{
    // Caller contract: invoked on this connection's affinity thread.
    // Both callers now satisfy that by construction rather than by
    // ordering: RadioModel::connectToRadio marshals the attach through
    // QMetaObject::invokeMethod and RadioModel::teardownConnection
    // marshals the detach.  The assignment and the m_lastMicAt arming
    // below are therefore race-free with the connection-thread reads in
    // onKeepAliveTick / decodeMicFrame132 whether or not the connection
    // has already been moved to its worker thread.
    //
    // This used to rest on ordering alone (the attach ran before the
    // moveToThread in connectToRadio).  That held on the hot path only.
    // The issue #153 sub-bug 1 cold-start retry re-runs the same attach
    // from a WdspEngine::initializedChanged handler on the main thread,
    // long after the move, which is what made marshalling necessary.
    // A future caller that reaches this setter without marshalling will
    // need atomic / mutex protection.
    m_txMicSource = src;
    // TX diagnostics lane: txSendStats reads the pump's wake watch.
    m_txMicSourceForStats.store(src, std::memory_order_release);

    // Stage-2 review fix I3: arm the LOS timer at attach time so the
    // mic-LOS zero-block injection (onKeepAliveTick) fires even if the
    // radio never delivers a mic frame.  Without this, m_lastMicAt
    // stays default-constructed (invalid) and the guard at
    // P2RadioConnection.cpp:1285 short-circuits forever — the worker
    // would block on waitForBlock(INFINITE) with no recovery.
    //
    // Mirrors Thetis network.c:656-667 [v2.10.3.15]: WSA_WAIT_TIMEOUT
    // injects zero buffer via Inbound regardless of whether real
    // samples have been observed.
    m_lastMicAt = QDateTime::currentDateTimeUtc();
}

void P2RadioConnection::setCalibrationController(const CalibrationController* cal)
{
    m_calController = cal;
}

// ---------------------------------------------------------------------------
// buildCodecContext — Phase 3P-B Task 7
//
// Snapshots all live P2RadioConnection state into a CodecContext so the
// codec compose methods are pure functions of (ctx × packet-type).
// ---------------------------------------------------------------------------
CodecContext P2RadioConnection::buildCodecContext() const
{
    CodecContext ctx;

    // Run/PTT state
    // ctx.p2PttOut drives byte 4 bit 1 (0x02) of CmdHighPriority = MOX.
    // Source: deskhpsdr/src/new_protocol.c:739-762 [@120188f]:
    //   high_priority_buffer_to_radio[4] = P2running;    // bit 0 = run
    //   if (xmit) { high_priority_buffer_to_radio[4] |= 0x02; }  // bit 1 = MOX
    // m_tx[0].pttOut is the rear-panel PTT-out relay (deferred to 3M-3) — NOT
    // the MOX source.  m_mox (3M-1a E.7) is the correct source for byte 4 bit 1.
    ctx.p2Running  = m_running;
    ctx.p2PttOut   = m_mox ? 1 : 0;    // 3M-1a E.7: was m_tx[0].pttOut (latent bug)
    ctx.p2Cwx      = m_tx[0].cwx;
    ctx.p2Dot      = m_tx[0].dot;
    ctx.p2Dash     = m_tx[0].dash;
    // Phase 3M-4 Task 17: PureSignal run flag — gates the DDC0/DDC1
    // frequency override in composeCmdHighPriority.  Sourced from
    // RadioConnection::m_puresignalRun which is set by setPuresignalRun(1)
    // when PureSignal is enabled (PsForm.cs:246 [v2.10.3.13]:
    // NetworkIO.SetPureSignal(1)).
    ctx.puresignalRun = m_puresignalRun;
    // 3M-1a (2026-04-27): mox + trxRelay drive the Alex bits 27 (_TR_Relay)
    // and 18 (_trx_status) in the codec's buildAlex0/buildAlex1.  Without
    // these the radio's antenna stays connected to the RX path during MOX
    // and no carrier reaches the SO-239.
    // Source: Thetis network.h:290,300,339,349 [v2.10.3.13];
    //         deskhpsdr/src/alex.h:91-96 + new_protocol.c:996-1004 [@120188f].
    // Note: P1 already wires these (P1RadioConnection.cpp:1268-1275); P2
    // had a gap that this commit closes.
    ctx.mox        = m_mox;
    ctx.trxRelay   = m_trxRelay;

    // ADC / DAC counts
    ctx.p2NumAdc   = m_numAdc;
    ctx.p2NumDac   = m_numDac;

    // Per-ADC dither + random.
    //
    // From Thetis network.c:1078-1090 [v2.10.3.13/.15] CmdRx byte 5/6:
    //   packetbuf[5] = (prn->adc[2].dither << 2 | prn->adc[1].dither << 1
    //                  | prn->adc[0].dither) & 0x7;
    //   packetbuf[6] = (prn->adc[2].random << 2 | prn->adc[1].random << 1
    //                  | prn->adc[0].random) & 0x7;
    //
    // Thetis emits bits for ALL 3 ADC slots regardless of how many ADCs the
    // board actually has — wire-capture of Thetis-on-ANAN-G2E (HermesC10,
    // num_adc=1) confirms byte 5 = byte 6 = 0x07 throughout the session.
    // The earlier "mask to num_adc" gate (2026-05-22) was based on a
    // mis-diagnosis of the G2E connect bug; the real unblock was the
    // primary-DDC + dither-fix combo and the enable-byte fix.  Reverted
    // so we match Thetis on the wire — PS calcc convergence depends on
    // it.
    for (int i = 0; i < 3; ++i) {
        ctx.dither[i] = (m_adc[i].dither != 0);
        ctx.random[i] = (m_adc[i].random != 0);
    }

    // Per-ADC step attenuators
    for (int i = 0; i < 3; ++i) {
        ctx.rxStepAttn[i] = m_adc[i].rxStepAttn;
        ctx.txStepAttn[i] = m_adc[i].txStepAttn;
    }

    // Per-RX preamp (index 0 = RX1 preamp at byte 1403 bit 0,
    // m_rx[1].preamp = RX2 preamp at byte 1403 bit 1)
    ctx.rxPreamp[0]  = (m_rx[0].preamp != 0);
    ctx.p2Rx1Preamp  = (m_rx[1].preamp != 0);  // "Mercury Attenuator" byte bit 1

    // RX per-DDC state (up to 7 DDCs)
    for (int i = 0; i < 7; ++i) {
        ctx.p2RxEnable[i]      = m_rx[i].enable;
        ctx.p2RxAdcAssign[i]   = m_rx[i].rxAdc;
        ctx.p2RxSamplingRate[i]= m_rx[i].samplingRate;
        ctx.p2RxBitDepth[i]    = m_rx[i].bitDepth;
        ctx.rxFreqHz[i]        = static_cast<quint64>(m_rx[i].frequency);
    }
    ctx.p2RxSync = m_rx[0].sync;

    // TX frequency + drive + PA + sampling rate + phase shift
    ctx.txFreqHz         = static_cast<quint64>(m_tx[0].frequency);

    // Out-of-band TX drive level gate.
    // From deskhpsdr/src/new_protocol.c:864-876 [@120188f]:
    //   int power = 0;
    //   if ((txfreq >= txband->frequencyMin && txfreq <= txband->frequencyMax)
    //       || tx_out_of_band_allowed) {
    //     power = transmitter->drive_level;
    //   }
    //   high_priority_buffer_to_radio[345] = power & 0xFF;
    //
    // NereusSDR uses bandFromFrequency() as a fast band-range check.
    // GEN and WWV map to out-of-ham-band TX frequencies.  BandPlanGuard is
    // a predicate — it does not zero driveLevel upstream — so the gate is
    // applied here at compose time, matching deskhpsdr behaviour.
    // tx_out_of_band_allowed is not yet wired in NereusSDR; when it is,
    // this gate should pass through driveLevel unconditionally.
    //
    // XVTR note: bandFromFrequency() never returns Band::XVTR — it falls
    // through to Band::GEN for any unmapped frequency.  Transverter operation
    // works because the IF frequency (radio-side) is in a ham band; the LO
    // offset is applied by the transverter hardware.  If a future task
    // explicitly handles XVTR with a known LO offset, revisit this gate.
    {
        const Band txBand = bandFromFrequency(static_cast<double>(m_tx[0].frequency));
        // 2 m (R-IOS-26) stays out of band here, as it was while 2 m fell
        // into GEN: Thetis's IsOKToTX takes HF rows only and 2 m is a VHF
        // row, so no drive goes out on 2 m.
        const bool txInBand = (txBand != Band::GEN && txBand != Band::WWV
                               && txBand != Band::Band2m);
        ctx.p2DriveLevel = txInBand ? m_tx[0].driveLevel : 0;
    }
    ctx.p2TxPa           = m_tx[0].pa;
    ctx.txPaDisabled     = m_tx[0].pa != 0;  // "Disable HF PA": the T/R relay (buildAlex0)
    ctx.p2TxSamplingRate = m_tx[0].samplingRate;
    ctx.p2TxPhaseShift   = m_tx[0].phaseShift;

    // CW state
    ctx.p2CwModeControl   = m_cw.modeControl;
    ctx.p2CwSidetoneLevel = m_cw.sidetoneLevel;
    ctx.p2CwSidetoneFreq  = m_cw.sidetoneFreq;
    ctx.p2CwKeyerSpeed    = m_cw.keyerSpeed;
    ctx.p2CwKeyerWeight   = m_cw.keyerWeight;
    ctx.p2CwHangDelay     = m_cw.hangDelay;
    ctx.p2CwRfDelay       = m_cw.rfDelay;
    ctx.p2CwEdgeLength    = m_cw.edgeLength;

    // Mic state
    ctx.p2MicControl    = m_mic.micControl;
    ctx.p2MicLineInGain = m_mic.lineInGain;

    // Alex antenna selection (1-based)
    ctx.p2AlexRxAnt = m_alex.rxAnt;
    ctx.p2AlexTxAnt = m_alex.txAnt;

    // RX-only antenna mux + RX-Bypass-Out relay — Phase 3P-I-b T5.
    // From Thetis ChannelMaster/network.h:279-282 + netInterface.c:479-481
    // [v2.10.3.13 @501e3f5]. Consumed by P2CodecOrionMkII::buildAlex0().
    ctx.rxOnlyAnt = m_alex.rxOnlyAnt;
    ctx.rxOut     = m_alex.rxOut;
    // Level Cal: the Alex attenuator (Alex0 bits 13 / 14).
    ctx.alexAttenBits = (m_alex.atten20dB ? 0x2 : 0) | (m_alex.atten10dB ? 0x1 : 0);

    // Mk II BPF board flag — drives the rx-only relay encoding split in
    // P2CodecOrionMkII::buildAlex0(). True for ORIONMKII / ANAN-7000D /
    // ANAN-8000D / ANAN_G2 / ANAN_G2_1K / ANVELINAPRO3 per
    // HardwareProfile.cpp:137-172 (which mirrors Thetis
    // clsHardwareSpecific.cs:85-184 SetMKIIBPF(1) callsites).
    //
    // Source: Thetis ChannelMaster/netInterface.c:461-477 [v2.10.3.13 @501e3f51]
    // (the `if (mkiibpf)` branch in SetAntBits — without this, RX-only
    // selections fire the wrong relay on Mk II BPF boards and the antenna
    // never actually reaches the receiver). Issue #257.
    ctx.mkiiBpf = m_hardwareProfile.mkiiBpf;

    // Alex HPF / LPF bits.
    //
    // HPF: Phase 3F routes ADC0's chain through AlexController's per-ADC
    // decision (see setAlexRxBpf); it falls back to the frequency-derived
    // value setReceiverFrequency maintains when no slice is bound yet.
    // ADC1's decision travels separately so buildAlex1 can write its own
    // chain rather than mirroring Alex0 — Thetis feeds the two words from
    // two different receivers (console.cs:15401 + 15435-15443 [v2.10.3.15]).
    // Upstream inline attribution preserved verbatim (console.cs:15441):
    //   HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    //
    // LPF: the RX band-pass decision must never touch it. The transmit
    // low-pass (Alex1) follows the transmit frequency and nothing else;
    // Alex0's low-pass follows the receive frequency while receiving and
    // the transmit frequency while keyed.
    //   From Thetis ChannelMaster/netInterface.c:682-726 [v2.10.3.15]
    ctx.alexHpfBits     = effectiveRxHpfBitsAdc0();
    ctx.alexHpfBitsAdc1 = m_alex.rxHpfBitsAdc1;
    ctx.alexLpfBits     = effectiveLpfBitsAlex0();
    ctx.alexLpfBitsTx   = static_cast<quint8>(m_alex.lpfBitsTx);

    // The Alex tab's high-pass switches (plan Task 14 and its fix wave).
    //
    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): HPF Bypass during MOX+PS.
    // From Thetis console.cs:6957 setBPF1ForOrionIISaturn [v2.10.3.13]:
    //   if (_mox && (disable_hpf_on_tx || (disable_hpf_on_ps && PureSignalEnabled)))
    //       NetworkIO.SetAlexHPFBits(0x20);   // Bypass — bit 12 of Alex0
    // HermesC10 dispatches into this branch (console.cs:6830 //N1GP G2E
    // added).  Without this on a 1-ADC G2E the FB DDC sees HPF-attenuated
    // coupler signal which calcc can't fit, and PureSignal oscillates
    // instead of locking.
    // Wire-confirmed by diffing Thetis-locked pcap (Alex0=0x09441C00, bit 12
    // set) against our pre-fix pcap (Alex0=0x09240C20, bit 12 clear) on
    // 2026-05-23 at /tmp/nereus-g2e-ps.pcap{.first, current}.
    //
    // Fix wave: the word is now 0x20 in place of the band's selection, not
    // the selection with 0x20 added. That is what SetAlexHPFBits(0x20)
    // leaves (netInterface.c:604-621 [v2.10.3.15]), and what the pcap above
    // shows: Thetis's 0x...1C00 has the 6.5 MHz relay (bit 5) clear, where
    // the OR-in kept it. The PureSignal arm now also runs only on the boards
    // setAlex1HPF sends to setBPF1ForOrionIISaturn (Orion MkII, Saturn,
    // HermesC10), and follows the Alex tab's check box, which it did not.
    //
    // "HPF Bypass on TX" (plan Task 14): keyed, Alex0's high-pass word is
    // 0x20, whatever the receive selection was.
    //   From Thetis console.cs:6843-6848 [v2.10.3.15] (setAlexHPF)
    //     if (_mox && disable_hpf_on_tx)
    //     { NetworkIO.SetAlexHPFBits(0x20); ... return; }
    // "HPF Bypass" (fix wave): 0x20 keyed or not (console.cs:6850-6855).
    // "Disable 6m LNA on RX / TX" (fix wave): on 6 m the BPF/LNA (0x40)
    // becomes 0x20 (console.cs:6935, 7050).
    //
    // SetAlexHPFBits writes prbpfilter (Alex0) only (netInterface.c:604-621
    // [v2.10.3.15]); Alex1's high-pass is not touched. Alex1 mirrors Alex0's
    // filter bits when ADC1 has no decision of its own
    // (P2CodecOrionMkII::buildAlex1), so that mirror is pinned to the
    // selection Alex0 had before a switch replaced it.
    if (m_caps && m_caps->hasAlexFilters) {
        const quint8 selected = ctx.alexHpfBits;
        const quint8 applied = NereusSDR::codec::alex::applyAlex1HpfSwitches(
            selected, m_caps->board, m_mox, m_puresignalRun, alexHpfSwitches());
        if (applied != selected) {
            if (ctx.alexHpfBitsAdc1 < 0) {
                ctx.alexHpfBitsAdc1 = static_cast<int>(selected & ~0x20u);
            }
            ctx.alexHpfBits = applied;
        }
    }

    // Port / wideband config
    ctx.p2CustomPortBase     = m_p2CustomPortBase;
    ctx.p2WbSamplesPerPacket = m_wbSamplesPerPacket;
    ctx.p2WbSampleSize       = m_wbSampleSize;
    ctx.p2WbUpdateRate       = m_wbUpdateRate;
    ctx.p2WbPacketsPerFrame  = m_wbPacketsPerFrame;
    // Phase 3F Sub-Epic F Task 1: thread the wideband per-ADC enable mask
    // through to the codec for CmdGeneral byte 23 (Thetis network.c:879
    // [v2.10.3.15]).
    ctx.p2WbEnableMask       = m_wbEnableMask;

    // Watchdog timer
    ctx.p2Wdt = m_wdt;

    // Frequency-correction factor — Phase 3P-G. Source: setup.cs:14036-14050.
    // Codec uses this in hzToPhaseWord() so calibration UI changes propagate
    // through the codec cutover path; default 1.0 when no controller wired.
    ctx.freqCorrectionFactor = m_calController
                               ? m_calController->effectiveFreqCorrectionFactor()
                               : 1.0;

    // Saturn BPF1 override bits — left at default 0 until Phase F
    // configures them via user-entered band-edge table.
    ctx.p2SaturnBpfHpfBits = 0;
    ctx.p2SaturnBpfLpfBits = 0;

    // OC output byte — sourced from OcMatrix when wired; legacy 0 otherwise.
    // Phase 3P-D Task 3 — From Thetis HPSDR/Penny.cs:117-132 [@501e3f5]
    //
    // Plan Task 14: the codecs now write it to high-priority byte 1401
    // (P2CodecOrionMkII::composeCmdHighPriority, network.c:1031). The band
    // was DDC0's centre, which on the G2 is not a receiver at all; it is now
    // the transmitting slice's band while keyed and the RX1 stand-in's VFO
    // band while not (ocBandFrequencyHz). Only a board with OC outputs
    // (ocOutputCount, every Protocol 2 row) drives the pins.
    ctx.ocByte = composedOcByte();

    // From Thetis cmaster.SetADCSupply / NetworkIO.LRAudioSwap [v2.10.3.15]
    // Per clsHardwareSpecific.cs:85-191 — forwarded to WDSP, not a P2 wire byte.
    ctx.adcSupplyVoltage = m_hardwareProfile.adcSupplyVoltage;
    ctx.lrAudioSwap      = m_hardwareProfile.lrAudioSwap;

    return ctx;
}

// ---------------------------------------------------------------------------
// composeCmd* wrappers — Phase 3P-B Task 7
//
// Each wrapper delegates to the per-board codec (m_codec) unless the
// NEREUS_USE_LEGACY_P2_CODEC=1 env-var is set (rollback hatch).
// Legacy compose bodies are preserved as composeCmd*Legacy for one release.
// ---------------------------------------------------------------------------

void P2RadioConnection::composeCmdGeneral(char buf[60]) const
{
    if (m_useLegacyP2Codec || !m_codec) {
        composeCmdGeneralLegacy(buf);
        return;
    }
    const CodecContext ctx = buildCodecContext();
    quint8 tmp[60] = {};
    m_codec->composeCmdGeneral(ctx, tmp);
    memcpy(buf, tmp, 60);
}

void P2RadioConnection::composeCmdHighPriority(char buf[kBufLen]) const
{
    // Plan Task 14 fix wave (R-R3-49): byte 1401 of this packet is the band
    // outputs, so the byte composed here is the one the radio gets. Every
    // window shows it (RadioModel::bandOutputsByte), as Thetis's LED strip
    // shows the bits UpdateExtCtrl returned (console.cs:29106-29107
    // [v2.10.3.15]).
    const int band = int(bandFromFrequency(static_cast<double>(ocBandFrequencyHz())));
    if (m_useLegacyP2Codec || !m_codec) {
        composeCmdHighPriorityLegacy(buf);
        // The rollback compose does not write byte 1401: the radio gets 0.
        publishBandOutputs(0, band, m_mox);
        return;
    }
    const CodecContext ctx = buildCodecContext();
    quint8 tmp[kBufLen] = {};
    m_codec->composeCmdHighPriority(ctx, tmp);
    memcpy(buf, tmp, kBufLen);
    publishBandOutputs(ctx.ocByte, band, m_mox);
}

void P2RadioConnection::composeCmdRx(char buf[kBufLen]) const
{
    if (m_useLegacyP2Codec || !m_codec) {
        composeCmdRxLegacy(buf);
        return;
    }
    const CodecContext ctx = buildCodecContext();
    quint8 tmp[kBufLen] = {};
    m_codec->composeCmdRx(ctx, tmp);
    memcpy(buf, tmp, kBufLen);
}

void P2RadioConnection::composeCmdTx(char buf[60]) const
{
    if (m_useLegacyP2Codec || !m_codec) {
        composeCmdTxLegacy(buf);
        return;
    }
    const CodecContext ctx = buildCodecContext();
    quint8 tmp[60] = {};
    m_codec->composeCmdTx(ctx, tmp);
    memcpy(buf, tmp, 60);
}

// ---------------------------------------------------------------------------
// Legacy compose implementations — preserved for NEREUS_USE_LEGACY_P2_CODEC
// rollback hatch. These are byte-for-byte the pre-Task-7 bodies.
// ---------------------------------------------------------------------------

// Porting from Thetis CmdGeneral() network.c:821-911
void P2RadioConnection::composeCmdGeneralLegacy(char buf[60]) const
{
    // From Thetis network.c:826
    buf[4] = 0x00;  // Command

    // From Thetis network.c:831-876 — PORT assignments
    int tmp;

    // PC outbound source ports (radio receives FROM these)
    // From Thetis network.c:839-857
    tmp = m_p2CustomPortBase + 0;  // Rx Specific #1025
    buf[5] = tmp >> 8; buf[6] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 1;  // Tx Specific #1026
    buf[7] = tmp >> 8; buf[8] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 2;  // High Priority from PC #1027
    buf[9] = tmp >> 8; buf[10] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 3;  // Rx Audio #1028
    buf[13] = tmp >> 8; buf[14] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 4;  // Tx0 IQ #1029
    buf[15] = tmp >> 8; buf[16] = tmp & 0xff;

    // Radio outbound source ports (radio sends FROM these)
    // From Thetis network.c:860-875
    tmp = m_p2CustomPortBase + 0;  // High Priority to PC #1025
    buf[11] = tmp >> 8; buf[12] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 10; // Rx0 DDC IQ #1035
    buf[17] = tmp >> 8; buf[18] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 1;  // Mic Samples #1026
    buf[19] = tmp >> 8; buf[20] = tmp & 0xff;
    tmp = m_p2CustomPortBase + 2;  // Wideband ADC0 #1027
    buf[21] = tmp >> 8; buf[22] = tmp & 0xff;

    // From Thetis network.c:878-888 — Wideband settings
    // From Thetis network.c:879 [v2.10.3.15] - wb_enable mask, bit N = ADCN.
    // Phase 3F Sub-Epic F Task 1 wired this from a hardcoded 0 placeholder to
    // m_wbEnableMask. Driven by setWidebandEnabled().
    buf[23] = static_cast<char>(m_wbEnableMask);
    buf[24] = (m_wbSamplesPerPacket >> 8) & 0xff;
    buf[25] = m_wbSamplesPerPacket & 0xff;
    buf[26] = m_wbSampleSize;      // 16 bits
    buf[27] = m_wbUpdateRate;      // 70ms
    buf[28] = m_wbPacketsPerFrame; // 32

    // From Thetis network.c:896 — 0x08 = bit[3] "freq or phase word"
    // Thetis sends 0x08 but stores frequencies as Hz in prn->rx[].frequency
    // Keep this matching Thetis exactly
    buf[37] = 0x08;

    // From Thetis network.c:898
    buf[38] = m_wdt;  // Watchdog timer (0 = disabled)

    // From Thetis network.c:904
    buf[58] = (!m_tx[0].pa) & 0x01;  // PA enable

    // From Thetis network.c:906 — Alex enable (BPF board)
    // prbpfilter->enable | prbpfilter2->enable
    buf[59] = 0x03;  // Enable both Alex0 and Alex1

    // Note: sequence number NOT written here — sendCmdGeneral() stamps it just
    // before transmission so composeCmdGeneralForTest() captures a deterministic
    // zero-sequence snapshot for regression baseline purposes.
}

// Porting from Thetis CmdHighPriority() network.c:913-1063
void P2RadioConnection::composeCmdHighPriorityLegacy(char buf[kBufLen]) const
{
    // From deskhpsdr/src/new_protocol.c:739-762 [@120188f]:
    //   high_priority_buffer_to_radio[4] = P2running;   // bit 0 = run
    //   if (xmit) { high_priority_buffer_to_radio[4] |= 0x02; }  // bit 1 = MOX
    //
    // 3M-1a E.7: prior code used m_tx[0].pttOut (rear-panel PTT-out relay) for
    // bit 1, which is wrong — pttOut is a TX-confirmation output, not the MOX
    // initiator.  m_mox is the correct source.  m_tx[0].pttOut is retained for
    // future 3M-3 rear-panel-PTT-out wiring but must NOT drive the MOX wire bit.
    buf[4] = static_cast<char>((m_mox ? 0x02 : 0x00) | (m_running ? 0x01 : 0x00));

    // From Thetis network.c:931-933
    buf[5] = (m_tx[0].dash << 2 | m_tx[0].dot << 1 | m_tx[0].cwx) & 0x7;

    // From Thetis network.c:936-1005
    // RX frequencies — 4 bytes each, big-endian phase words.
    // General cmd byte 37 = 0x08 (bit 3) means frequencies are NCO phase words.
    // From pcap analysis: phase_word = freq_hz * 2^32 / 122880000
    // RX0-RX1 have PureSignal override logic; for now use straight frequency
    for (int i = 0; i < kMaxRxStreams; ++i) {
        int offset = 9 + (i * 4);
        if (offset + 3 < kBufLen) {
            quint32 phaseWord = hzToPhaseWord(m_rx[i].frequency);
            writeBE32(buf, offset, phaseWord);
        }
    }

    // From Thetis network.c:1008-1011 — TX0 frequency (also phase word)
    writeBE32(buf, 329, hzToPhaseWord(m_tx[0].frequency));

    // From deskhpsdr/src/new_protocol.c:864-876 [@120188f]:
    //   int power = 0;
    //   if ((txfreq >= txband->frequencyMin && txfreq <= txband->frequencyMax)
    //       || tx_out_of_band_allowed) { power = transmitter->drive_level; }
    //   high_priority_buffer_to_radio[345] = power & 0xFF;
    //
    // Out-of-band TX drive level gate: zero byte 345 when TX frequency is
    // outside a recognised ham band.  BandPlanGuard does not zero driveLevel
    // upstream; gate applied at compose time.  tx_out_of_band_allowed not yet
    // wired in NereusSDR.
    //
    // XVTR note: bandFromFrequency() never returns Band::XVTR — it falls
    // through to Band::GEN for any unmapped frequency.  Transverter operation
    // works because the IF frequency (radio-side) is in a ham band; the LO
    // offset is applied by the transverter hardware.  If a future task
    // explicitly handles XVTR with a known LO offset, revisit this gate.
    {
        const Band txBand = bandFromFrequency(static_cast<double>(m_tx[0].frequency));
        // 2 m (R-IOS-26) stays out of band here, as it was while 2 m fell
        // into GEN: Thetis's IsOKToTX takes HF rows only and 2 m is a VHF
        // row, so no drive goes out on 2 m.
        const bool txInBand = (txBand != Band::GEN && txBand != Band::WWV
                               && txBand != Band::Band2m);
        buf[345] = static_cast<char>(txInBand ? m_tx[0].driveLevel : 0);
    }

    // From Thetis network.c:1037-1038 — Mercury Attenuator
    buf[1403] = m_rx[1].preamp << 1 | m_rx[0].preamp;

    // From Thetis network.c:1055-1057 — Step Attenuators
    buf[1442] = m_adc[1].rxStepAttn;
    buf[1443] = m_adc[0].rxStepAttn;

    // Alex filter/antenna registers (bytes 1428-1435)
    // From Thetis ChannelMaster/network.c:1040-1050
    // Alex0 (bytes 1432-1435): RX antenna + HPF + LPF
    // Alex1 (bytes 1428-1431): TX antenna + HPF + LPF
    writeBE32(buf, 1432, buildAlex0());
    writeBE32(buf, 1428, buildAlex1());
}

// Porting from Thetis CmdRx() network.c:1066-1179
void P2RadioConnection::composeCmdRxLegacy(char buf[kBufLen]) const
{
    // From Thetis network.c:1074
    buf[4] = m_numAdc;

    // From Thetis network.c:1080-1082 — Dither
    buf[5] = (m_adc[2].dither << 2 | m_adc[1].dither << 1 | m_adc[0].dither) & 0x7;

    // From Thetis network.c:1088-1090 — Random
    buf[6] = (m_adc[2].random << 2 | m_adc[1].random << 1 | m_adc[0].random) & 0x7;

    // From Thetis network.c:1097-1103 — Enable bitmask
    buf[7] = (m_rx[6].enable << 6 | m_rx[5].enable << 5 |
              m_rx[4].enable << 4 | m_rx[3].enable << 3 |
              m_rx[2].enable << 2 | m_rx[1].enable << 1 |
              m_rx[0].enable) & 0xff;

    // From Thetis network.c:1106-1169 — Per-RX config
    // Layout: each RX is 6 bytes apart, starting at byte 17
    // byte+0: ADC, byte+1-2: sampling rate, byte+5: bit depth
    for (int i = 0; i < 7; ++i) {
        int base = 17 + (i * 6);
        buf[base] = m_rx[i].rxAdc;
        buf[base + 1] = (m_rx[i].samplingRate >> 8) & 0xff;
        buf[base + 2] = m_rx[i].samplingRate & 0xff;
        buf[base + 5] = m_rx[i].bitDepth;
    }

    // From Thetis network.c:1172
    buf[1363] = m_rx[0].sync;
}

// Porting from Thetis CmdTx() network.c:1181-1248
void P2RadioConnection::composeCmdTxLegacy(char buf[60]) const
{
    // From Thetis network.c:1188
    buf[4] = m_numDac;

    // From Thetis network.c:1199 — CW mode control
    buf[5] = m_cw.modeControl;

    // From Thetis network.c:1202-1216
    buf[6] = m_cw.sidetoneLevel;
    buf[7] = (m_cw.sidetoneFreq >> 8) & 0xff;
    buf[8] = m_cw.sidetoneFreq & 0xff;
    buf[9] = m_cw.keyerSpeed;
    buf[10] = m_cw.keyerWeight;
    buf[11] = (m_cw.hangDelay >> 8) & 0xff;
    buf[12] = m_cw.hangDelay & 0xff;
    buf[13] = m_cw.rfDelay;

    // From Thetis network.c:1218-1220 — TX0 sampling rate
    buf[14] = (m_tx[0].samplingRate >> 8) & 0xff;
    buf[15] = m_tx[0].samplingRate & 0xff;

    // From Thetis network.c:1222
    buf[17] = m_cw.edgeLength & 0xff;

    // From Thetis network.c:1224-1226 — TX0 phase shift
    buf[26] = (m_tx[0].phaseShift >> 8) & 0xff;
    buf[27] = m_tx[0].phaseShift & 0xff;

    // From Thetis network.c:1234 — Mic control
    buf[50] = m_mic.micControl;

    // From Thetis network.c:1236
    buf[51] = m_mic.lineInGain;

    // From Thetis network.c:1238-1242 — Step attenuators on TX
    buf[57] = m_adc[2].txStepAttn;
    buf[58] = m_adc[1].txStepAttn;
    buf[59] = m_adc[0].txStepAttn;
}

// --- Command Senders (compose + UDP dispatch) ---

void P2RadioConnection::sendCmdGeneral()
{
    char buf[60];
    memset(buf, 0, sizeof(buf));
    // Stamp sequence number before compose so wire bytes include it.
    writeBE32(buf, 0, m_seqGeneral++);
    composeCmdGeneral(buf);
    // From Thetis network.c:910
    // sendPacket(listenSock, packetbuf, sizeof(packetbuf), prn->base_outbound_port);
    QByteArray pkt(buf, sizeof(buf));
    m_socket->writeDatagram(pkt, m_radioInfo.address, m_baseOutboundPort);
}

void P2RadioConnection::sendCmdHighPriority()
{
    char buf[kBufLen];
    memset(buf, 0, sizeof(buf));
    writeBE32(buf, 0, m_seqHighPri++);
    composeCmdHighPriority(buf);
    // From Thetis network.c:1062
    // sendPacket(listenSock, packetbuf, BUFLEN, prn->base_outbound_port + 3);
    QByteArray pkt(buf, sizeof(buf));
    // 2026-07-27: check the send result.  Previously the return was discarded,
    // so disconnect() logged "SendStop complete" whether or not the run=0 frame
    // ever reached the wire — three rounds of ANAN-G2E lockup debugging trusted
    // that line as evidence the stop had been sent.  It was not evidence.
    const qint64 written =
        m_socket->writeDatagram(pkt, m_radioInfo.address, m_baseOutboundPort + 3);
    if (written != pkt.size()) {
        qCWarning(lcConnection)
            << "P2: CmdHighPriority send failed — wrote" << written
            << "of" << pkt.size() << "bytes:" << m_socket->errorString();
    }
    // Shell-chrome sub-PR-2 B.1: record egress bytes for ▲ Mbps readout.
    recordBytesSent(static_cast<qint64>(pkt.size()));
    // Shell-chrome sub-PR-2 B.2: bracket C&C round-trip for ping RTT.
    // P2 high-priority command → high-priority status reply (100 ms cadence).
    // We note send here; receive is noted in processHighPriorityStatus.
    notePingSent();
}

void P2RadioConnection::sendCmdRx()
{
    char buf[kBufLen];
    memset(buf, 0, sizeof(buf));
    writeBE32(buf, 0, m_seqRx++);
    composeCmdRx(buf);
    // From Thetis network.c:1178
    QByteArray pkt(buf, sizeof(buf));
    m_socket->writeDatagram(pkt, m_radioInfo.address, m_baseOutboundPort + 1);
}

void P2RadioConnection::sendCmdTx()
{
    char buf[60];
    memset(buf, 0, sizeof(buf));
    writeBE32(buf, 0, m_seqTx++);
    composeCmdTx(buf);
    // From Thetis network.c:1247
    QByteArray pkt(buf, sizeof(buf));
    m_socket->writeDatagram(pkt, m_radioInfo.address, m_baseOutboundPort + 2);
}

// --- Data Parsing ---
// Porting from Thetis ReadUDPFrame:605-631 and ReadThreadMainLoop:790-808

void P2RadioConnection::processIqPacket(const QByteArray& data, int ddcIndex)
{
    if (ddcIndex < 0 || ddcIndex >= kMaxDdc) {
        return;
    }

    const auto* raw = reinterpret_cast<const unsigned char*>(data.constData());

    // From Thetis ReadUDPFrame:509-512 — sequence number extraction
    quint32 seq = (static_cast<quint32>(raw[0]) << 24)
               | (static_cast<quint32>(raw[1]) << 16)
               | (static_cast<quint32>(raw[2]) << 8)
               | (static_cast<quint32>(raw[3]));

    // From Thetis ReadUDPFrame:619-626 — sequence error detection
    bool seqError = false;
    if (seq != (1 + m_rx[ddcIndex].rxInSeqNo) && seq != 0
        && m_rx[ddcIndex].rxInSeqNo != 0) {
        m_rx[ddcIndex].rxInSeqErr += 1;
        seqError = true;
        qCDebug(lcProtocol) << "P2: DDC" << ddcIndex
                            << "seq error this:" << seq
                            << "last:" << m_rx[ddcIndex].rxInSeqNo;
    }
    m_rx[ddcIndex].rxInSeqNo = seq;

    // R-R3-32 (parity Task 6): the same per-DDC count (one per mismatch, as
    // rx_in_seq_err) feeds the link's packet loss, and this DDC's arrivals
    // the NereusSDR-native jitter of the lowest active stream (RFC 3550
    // section 6.4.1, see RadioLinkStats): a datagram carries spp samples at
    // the DDC's rate (kHz).
    {
        const qint64 arrivalUs = RadioLinkStats::nowUs();
        m_linkStats.noteSequenced(arrivalUs, seqError ? 1U : 0U);
        const int rateKhz = m_rx[ddcIndex].samplingRate;
        const double spacingUs = rateKhz > 0
            ? (static_cast<double>(m_rx[ddcIndex].spp) * 1000.0) / static_cast<double>(rateKhz)
            : 0.0;
        m_linkStats.noteStreamArrival(ddcIndex, seq, arrivalUs, spacingUs);
    }

    // From Thetis ReadUDPFrame:629 — copy I/Q data (skip 16-byte header)
    // memcpy(bufp, readbuf + 16, 1428);
    // Then ReadThreadMainLoop:790-806 — convert 24-bit to float
    int spp = m_rx[ddcIndex].spp;  // 238 samples per packet
    QVector<float>& buf = m_iqBuffers[ddcIndex];
    if (buf.size() != spp * 2) {
        buf.resize(spp * 2);
    }

    // From Thetis ReadThreadMainLoop:790-806
    // for (i = 0, k = 0; i < prn->rx[0].spp; i++, k += 6)
    //   prn->RxReadBufp[2*i+0] = const_1_div_2147483648_ *
    //     (double)(prn->ReadBufp[k+0]<<24 | prn->ReadBufp[k+1]<<16 | prn->ReadBufp[k+2]<<8);
    const unsigned char* iqData = raw + 16;
    for (int i = 0, k = 0; i < spp; ++i, k += 6) {
        // I sample
        qint32 iVal = (static_cast<qint32>(iqData[k + 0]) << 24)
                    | (static_cast<qint32>(iqData[k + 1]) << 16)
                    | (static_cast<qint32>(iqData[k + 2]) << 8);
        buf[2 * i + 0] = static_cast<float>(iVal) / 2147483648.0f;

        // Q sample
        qint32 qVal = (static_cast<qint32>(iqData[k + 3]) << 24)
                    | (static_cast<qint32>(iqData[k + 4]) << 16)
                    | (static_cast<qint32>(iqData[k + 5]) << 8);
        buf[2 * i + 1] = static_cast<float>(qVal) / 2147483648.0f;
    }

    ++m_totalIqPackets;

    if (m_totalIqPackets == 1) {
        qCDebug(lcConnection) << "P2: First I/Q packet! DDC" << ddcIndex
                              << "seq:" << seq << "spp:" << spp;

        // Cancel the connect watchdog — first valid DDC frame means we
        // reached the radio successfully. Design §4.1.
        if (m_connectWatchdog && m_connectWatchdog->isActive()) {
            m_connectWatchdog->stop();
        }

        // Issue #239: promote Connecting -> Connected only after the first
        // I/Q frame actually arrives. Until now connectToRadio() left state
        // at Connecting; without this transition the UI would never show
        // "Connected" for a healthy radio.
        if (state() == ConnectionState::Connecting) {
            setState(ConnectionState::Connected);
        }
    } else if (m_totalIqPackets % 10000 == 0) {
        qCDebug(lcProtocol) << "P2: I/Q packets:" << m_totalIqPackets;
    }

    // Per-frame activity signal for TitleBar LED (throttled to 10 Hz by
    // the receiver). Design §4.1.
    // Inside a socket drain it is emitted once, at the drain's end.
    if (m_inIqDrain) {
        m_frameReceivedPending = true;
    } else {
        emit frameReceived();
    }

    // ── Phase 3M-4 Task 17 — multi-stream sync de-interleaver ─────────────
    //
    // When DDC0 has sync bits set in m_rx[0].sync (byte 1363 of the prior
    // CmdRx), the radio folds the sync'd DDCs' samples into DDC0's packet
    // stream as sample-pair-level interleaved I/Q.  Mirrors the Thetis
    // ChannelMaster xrouter de-interleave at router.c:91-103 [v2.10.3.13]
    // (function=2 case): each pair of 6 bytes is one I/Q sample for one
    // stream, the streams cycle in ascending order across consecutive
    // sample blocks.  After de-interleaving, sync.c:53-58 [v2.10.3.13]
    // calls pscc(channel, sps, data[ps_tx_idx], data[ps_rx_idx]).
    //
    // For NereusSDR, we emit one iqDataReceived per de-interleaved stream
    // with the appropriate DDC index, so PsccPump and ReceiverManager
    // see the streams as if they had arrived on separate UDP ports.
    int nstreams = 1;
    if (ddcIndex == 0 && m_rx[0].sync != 0) {
        // popcount of sync byte + 1 for DDC0 itself.  Stream 0 is always
        // DDC0; streams 1+ map to the sync'd DDCs in ascending bit order.
        nstreams = 1 + std::popcount(static_cast<unsigned int>(m_rx[0].sync));
    }

    if (nstreams == 1) {
        emit iqDataReceived(ddcIndex, buf);
    } else {
        // De-interleave: input buf has packet sample order
        //   s0_sample0, s1_sample0, ..., sN-1_sample0, s0_sample1, ...
        // Output per-stream buffers have sample order
        //   sX_sample0, sX_sample1, ..., sX_sample(sps-1)
        // where sps = spp / nstreams.
        const int sps = spp / nstreams;
        int streamDdc[8] = {0};
        streamDdc[0] = 0;   // Stream slot 0 is DDC0 itself
        int slot = 1;
        for (int j = 0; j < 7 && slot < nstreams; ++j) {
            if ((m_rx[0].sync >> j) & 1) {
                streamDdc[slot++] = j;
            }
        }
        // BENCH DIAGNOSTIC (Phase 3M-4 Task 17): log first sample of each
        // de-interleaved stream once per ~800 packets (~1 sec at 192 kHz)
        // so we can verify the split actually produces different streams.
        static int diagDeintCounter = 0;
        const bool logThis = (++diagDeintCounter % 800 == 1);

        // Allocate fresh QVectors per stream to defeat any COW + queued-
        // copy weirdness that would let two emits share the same buffer.
        QVector<float> streamBufs[8];
        for (int s = 0; s < nstreams; ++s) {
            streamBufs[s].resize(sps * 2);
            for (int i = 0; i < sps; ++i) {
                const int packetSampleIdx = nstreams * i + s;
                streamBufs[s][2 * i + 0] = buf[2 * packetSampleIdx + 0];
                streamBufs[s][2 * i + 1] = buf[2 * packetSampleIdx + 1];
            }
            if (logThis) {
                qCInfo(lcConnection).nospace()
                    << "P2 deint stream " << s << " (DDC" << streamDdc[s] << "): "
                    << "first I/Q=" << streamBufs[s][0] << "," << streamBufs[s][1]
                    << " mid I/Q=" << streamBufs[s][sps] << "," << streamBufs[s][sps+1]
                    << " last I/Q=" << streamBufs[s][2*(sps-1)] << "," << streamBufs[s][2*(sps-1)+1];
            }
        }

        // Phase 3M-4 bench-fix 2026-05-23 (J.J. Boyd KG4VCF): source-first
        // PS pairing per Thetis sync.c:53-58 InboundBlock(id=1)
        // [v2.10.3.15] + router.c:91-102 case 2 [v2.10.3.15].
        //
        // When this packet carries both the PS-feedback and TX-monitor
        // DDCs (latched from applyDdcAssignment), emit them as a single
        // paired signal so PsccPump can call pscc() once with both
        // buffers from the SAME deinterleave pass — no host-side ring
        // buffering, no cross-stream drift.
        //
        // Issued BEFORE the per-stream iqDataReceived loop so the order
        // of observation matches Thetis: ChannelMaster fires the
        // PS-paired call (xrouter case 2 → InboundBlock(1)) on the same
        // packet that feeds the regular per-stream consumers.
        if (m_psFbDdc >= 0 && m_psTxMonDdc >= 0) {
            int psFbSlot   = -1;
            int psTxMonSlot = -1;
            for (int s = 0; s < nstreams; ++s) {
                if (streamDdc[s] == m_psFbDdc)    { psFbSlot   = s; }
                if (streamDdc[s] == m_psTxMonDdc) { psTxMonSlot = s; }
            }
            if (psFbSlot >= 0 && psTxMonSlot >= 0) {
                emit psPairedIqDataReceived(m_psFbDdc,    streamBufs[psFbSlot],
                                            m_psTxMonDdc, streamBufs[psTxMonSlot]);
            }
        }

        // Per-stream emission for the legacy RX path (RX1 audio etc.).
        // The PsccPump's old onIqData slot is now a no-op, so this loop
        // no longer drives PS; it remains the source of truth for all
        // non-PS DDC consumers.
        for (int s = 0; s < nstreams; ++s) {
            emit iqDataReceived(streamDdc[s], streamBufs[s]);
        }
    }
}

// ---------------------------------------------------------------------------
// onConnectTimeout — Phase 3Q Task 3
//
// Fires kConnectTimeoutMs after connectToRadio() if no first DDC I/Q frame
// arrived. Emits connectFailed(Timeout, ...) so the UI can surface a typed
// error instead of leaving the spinner running forever.
// processIqPacket() cancels this timer on the first valid frame.
// ---------------------------------------------------------------------------
void P2RadioConnection::onConnectTimeout()
{
    if (m_intentionalDisconnect) { return; }

    // Guard: if we somehow received a first frame already, do nothing.
    if (m_totalIqPackets > 0) { return; }

    qCWarning(lcConnection) << "P2: Connect watchdog fired — no DDC I/Q frame within"
                            << m_connectTimeoutMs << "ms; tearing down and emitting connectFailed(Timeout)";

    // Issue #239: tear down to Disconnected so the UI does not claim
    // "Connected" while the radio is unreachable. Stop the keep-alive,
    // TX-IQ, heartbeat, and reconnect timers, close the socket, and clear
    // m_running. m_intentionalDisconnect is set so any straggling datagrams
    // drained later are dropped without re-arming the state machine.
    m_running = false;
    m_intentionalDisconnect = true;
    m_linkLossLatched = true;
    ++m_connectionGeneration;
    discardWidebandFrames();
    m_establishedSilenceGeneration = 0;
    m_establishedSilenceDeadline = QDeadlineTimer();
    if (m_keepAliveTimer) { m_keepAliveTimer->stop(); }
    stopTxIqSender();
    if (m_p2HeartbeatTimer) { m_p2HeartbeatTimer->stop(); }
    if (m_reconnectTimer) { m_reconnectTimer->stop(); }
    if (m_establishedSilenceTimer) { m_establishedSilenceTimer->stop(); }
    if (m_socket) { m_socket->close(); }
    setState(ConnectionState::Disconnected);

    emit connectFailed(ConnectFailure::Timeout,
                       QStringLiteral("No response from radio within %1 ms — "
                                      "check IP address, radio power, and network")
                           .arg(m_connectTimeoutMs));
}

// Porting from Thetis ReadUDPFrame:519-532 — High Priority C&C status
void P2RadioConnection::processHighPriorityStatus(const QByteArray& data)
{
    const auto* raw = reinterpret_cast<const unsigned char*>(data.constData());

    // From Thetis ReadUDPFrame:522-530
    quint32 seq = (static_cast<quint32>(raw[0]) << 24)
               | (static_cast<quint32>(raw[1]) << 16)
               | (static_cast<quint32>(raw[2]) << 8)
               | (static_cast<quint32>(raw[3]));

    if (seq != (1 + m_ccSeqNo) && seq != 0 && m_ccSeqNo != 0) {
        qCDebug(lcProtocol) << "P2: CC seq error this:" << seq << "last:" << m_ccSeqNo;
    }
    m_ccSeqNo = seq;

    // Status data starts at byte 4 (Thetis copies readbuf+4, 56 bytes)
    // Extract key fields for meter data
    // These offsets are from the Thetis high-priority status parsing
    // (varies by firmware; basic fields for now)
    //
    // Byte layout (relative to raw[], which includes the 4-byte seq prefix):
    //   raw[0..3] = sequence number
    //   raw[4]    = ReadBufp[0] = PTT/dot/dash byte
    //               Bit [0] = PTT,  Bit [1] = Dot,  Bit [2] = Dash
    //   raw[5]    = ReadBufp[1] = ADC overload bitmap
    //               Bit [0] = ADC0, Bit [1] = ADC1, Bit [2] = ADC2
    // Source: Thetis network.c:686-708 [v2.10.3.13] (ReadUDPFrame strips seq,
    //   memcpy(bufp, readbuf+4, 56) — so ReadBufp[N] = raw[N+4]).

    // H.5: mic_ptt extraction — P2 High-Priority status ReadBufp[0] bit 0.
    // From Thetis network.c:686-689 [v2.10.3.13]:
    //   //Byte 0 - Bit [0] - PTT  1 = active, 0 = inactive
    //   prn->ptt_in = prn->ReadBufp[0] & 0x1;
    // + console.cs:25426 [v2.10.3.13]:
    //   bool mic_ptt = (dotdashptt & 0x01) != 0; // PTT from radio
    // + deskhpsdr new_protocol.c:2525 [@120188f]:
    //   radio_ptt = (buffer[4]) & 0x01;
    //
    // Emitted unconditionally each frame: MoxController::onMicPttFromRadio
    // is idempotent on repeated same-state calls.
    const bool micPtt = (raw[4] & 0x01) != 0;
    emit micPttFromRadio(micPtt);

    //[2.10.3.13]MW0LGE adc_overload bits accumulated across status frames; reset-on-read pattern preserved [Thetis network.c:708]
    // From Thetis network.c:695-708 [v2.10.3.13]: ReadBufp[1] is the ADC overload
    // bitmap.  In NereusSDR raw[], ReadBufp[1] = raw[5] (after 4-byte seq prefix).
    // Bit 0=ADC0, Bit 1=ADC1, Bit 2=ADC2 (Thetis network.c:708).
    const quint8 adcOverloadBits = raw[5];
    observeAdcOverloads(0x07, adcOverloadBits);
    for (int i = 0; i < 3; ++i) {
        if (adcOverloadBits & (1 << i)) {
            emit adcOverflow(i);
        }
    }

    // Phase 3P-H Task 4: PA telemetry — extract raw 16-bit ADC counts from
    // the High-Priority status packet body.  Per-board scaling lives in
    // RadioModel (console.cs computeAlexFwdPower / computeRefPower /
    // convertToVolts / convertToAmps), since bridge_volt / refvoltage /
    // adc_cal_offset depend on HardwareSpecific.Model.
    //
    // The 4-byte sequence number sits at raw[0..3]; the status payload
    // (matching Thetis's prn->ReadBufp[0..]) starts at raw[4].  Indices below
    // are relative to the Thetis ReadBufp pointer; we add 3 to land in raw[].
    //
    // From Thetis network.c:711-748 [@501e3f5]:
    // Upstream inline attribution preserved verbatim (network.c:708):
    //   prn->adc[i].adc_overload = prn->adc[i].adc_overload || (((prn->ReadBufp[1] >> i) & 0x1) != 0); // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
    //   //Bytes 2,3      Exciter Power [15:0]     * 12 bits sign extended to 16
    //   //Bytes 10,11    FWD Power [15:0]           ditto
    //   //Bytes 18,19    REV Power [15:0]           ditto
    //   prn->tx[0].exciter_power = prn->ReadBufp[2]  << 8 | prn->ReadBufp[3];
    //   prn->tx[0].fwd_power     = prn->ReadBufp[10] << 8 | prn->ReadBufp[11];
    //   prn->tx[0].rev_power     = prn->ReadBufp[18] << 8 | prn->ReadBufp[19];
    //   //Bytes 45,46  Supply Volts [15:0]
    //   prn->supply_volts        = prn->ReadBufp[45] << 8 | prn->ReadBufp[46];
    //   //Bytes 51,52  User ADC1 [15:0]
    //   //Bytes 53,54  User ADC0 [15:0]
    //   prn->user_adc1           = prn->ReadBufp[51] << 8 | prn->ReadBufp[52];  // AIN4
    //   prn->user_adc0           = prn->ReadBufp[53] << 8 | prn->ReadBufp[54];  // AIN3
    //
    // The High-Priority status packet body must be at least 55 bytes for the
    // user_adc0 read (offset 53-54 from the ReadBufp base = data.size() ≥ 4 + 55).
    // Defensive: skip telemetry emit on truncated packets.
    if (data.size() >= 4 + 55) {
        // ReadBufp[N] → raw[N + 4] (account for 4-byte sequence prefix).
        // From Thetis network.c:714 [@501e3f5] — exciter AIN5
        const quint16 exciterRaw  = static_cast<quint16>((raw[ 4 +  2] << 8) | raw[ 4 +  3]);
        // From Thetis network.c:715 [@501e3f5] — fwd AIN1
        const quint16 fwdRaw      = static_cast<quint16>((raw[ 4 + 10] << 8) | raw[ 4 + 11]);
        // From Thetis network.c:716 [@501e3f5] — rev AIN2
        const quint16 revRaw      = static_cast<quint16>((raw[ 4 + 18] << 8) | raw[ 4 + 19]);
        // From Thetis network.c:738 [@501e3f5] — supply_volts
        const quint16 supplyRaw   = static_cast<quint16>((raw[ 4 + 45] << 8) | raw[ 4 + 46]);
        // From Thetis network.c:746 [@501e3f5] — user_adc1 AIN4 PA Amps
        const quint16 userAdc1Raw = static_cast<quint16>((raw[ 4 + 51] << 8) | raw[ 4 + 52]);
        // From Thetis network.c:747 [@501e3f5] — user_adc0 AIN3 PA Volts
        const quint16 userAdc0Raw = static_cast<quint16>((raw[ 4 + 53] << 8) | raw[ 4 + 54]);

        emit paTelemetryUpdated(fwdRaw, revRaw, exciterRaw,
                                userAdc0Raw, userAdc1Raw, supplyRaw);

        // Shell-chrome sub-PR-2 B.3: emit voltage signals.
        // handleSupplyRaw applies for all radios (supply_volts AIN6).
        // From Thetis network.c:738 [@501e3f5] — supply_volts
        handleSupplyRaw(supplyRaw);
        // handleUserAdc0Raw gated on MKII-class boards (PA drain sense AIN3).
        // Matches gate in RadioModel scalePaVolts() [RadioModel.cpp:402-417].
        // From Thetis network.c:747 [@501e3f5] — user_adc0 AIN3 PA Volts
        switch (m_hardwareProfile.model) {
        case HPSDRModel::ORIONMKII:
        case HPSDRModel::ANAN8000D:
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN_G2E: //N1GP G2E added [Thetis console.cs:25007 v2.10.3.15 grouping]
        case HPSDRModel::ANAN_G2:
        case HPSDRModel::ANAN_G2_1K:
        case HPSDRModel::ANVELINAPRO3:
            handleUserAdc0Raw(userAdc0Raw);
            break;
        default:
            break;
        }
    }

    // Task 13: the user digital inputs, which carry the TX inhibit input
    // TxInhibitMonitor reads. ReadBufp[55] is raw[59]: ReadUDPFrame copies
    // readbuf + 4 (network.c:531 [v2.10.3.15]). console.cs's "byte 59"
    // comments count from the datagram; network.c counts from ReadBufp.
    // From Thetis network.c:750-756 [v2.10.3.15]:
    //   //Byte 55 - Bit [0] - User I/O (IO4) 1 = active, 0 = inactive
    //   //          Bit [1] - User I/O (IO5) 1 = active, 0 = inactive
    //   //          Bit [2] - User I/O (IO6) 1 = active, 0 = inactive
    //   //          Bit [3] - User I/O (IO8) 1 = active, 0 = inactive
    //   //          Bit [4] - User I/O (IO2) 1 = active, 0 = inactive
    //   prn->user_dig_in = prn->ReadBufp[55];
    if (data.size() >= 4 + 56) {
        reportUserDigitalInputs(raw[4 + 55]);
    }

    // Shell-chrome sub-PR-2 B.2: complete the ping RTT measurement.
    // The High-Priority status packet is the inbound leg of the
    // high-priority command → status exchange (100 ms cadence).
    notePingReceived();

    emit meterDataReceived(0.0f, 0.0f, 0.0f, 0.0f);
}

// ---------------------------------------------------------------------------
// getAdcForDdc
// ---------------------------------------------------------------------------
int P2RadioConnection::getAdcForDdc(int ddc) const
{
    if (ddc < 0 || ddc > 6) { return 0; }
    return static_cast<int>((m_rxAdcCtrl1 >> (ddc * 2)) & 0x3);
}

// --- Utility ---

void P2RadioConnection::writeBE32(char* buf, int offset, quint32 value)
{
    buf[offset]     = static_cast<char>((value >> 24) & 0xFF);
    buf[offset + 1] = static_cast<char>((value >> 16) & 0xFF);
    buf[offset + 2] = static_cast<char>((value >> 8)  & 0xFF);
    buf[offset + 3] = static_cast<char>( value        & 0xFF);
}

// From pcap analysis: phase_word = freq_hz * 2^32 / 122880000
// The ANAN-G2 clock is 122.88 MHz. Phase word mode (General byte 37 bit 3)
// tells the radio to interpret frequency fields as NCO phase increments.
//
// Phase 3P-G: multiplied by CalibrationController::effectiveFreqCorrectionFactor()
// when a controller is wired.  Default factor 1.0 → byte-identical to pre-cal.
//
// Source: HPSDR/NetworkIO.cs:251-254 Freq2PhaseWord():
//   long pw = (long)Math.Pow(2, 32) * freq / 122880000;
//   (Thetis applies FreqCorrectionFactor to the freq argument via VFOfreq()
//   before calling Freq2PhaseWord — we fold the factor into this helper.)
//   [@501e3f5]
quint32 P2RadioConnection::hzToPhaseWord(quint64 freqHz) const
{
    // Apply frequency correction factor if a CalibrationController is wired.
    // Source: setup.cs:14036-14050 udHPSDRFreqCorrectFactor_ValueChanged:
    //   NetworkIO.FreqCorrectionFactor = (double)udHPSDRFreqCorrectFactor.Value;
    //   (factor sent from setup to NetworkIO; NereusSDR folds it here instead)
    //   [@501e3f5]
    const double factor = m_calController
                          ? m_calController->effectiveFreqCorrectionFactor()
                          : 1.0;
    // R-R3-49: the codec's conversion, Thetis's own (NetworkIO.cs VFOfreq
    // and Freq2PhaseWord [v2.10.3.15]; see P2CodecOrionMkII::hzToPhaseWord).
    return P2CodecOrionMkII::hzToPhaseWord(freqHz, factor);
}

// Build Alex0 32-bit register (bytes 1432-1435 in CmdHighPriority).
// Contains: RX antenna (bits 24-26), LPF (bits 20-31), HPF (bits 0-6),
//           RX relay bits (bits 8-15).
// From Thetis ChannelMaster/network.h:263-358 bpfilter struct.
quint32 P2RadioConnection::buildAlex0() const
{
    quint32 reg = 0;

    // RX antenna selection — from Thetis netInterface.c:479-485
    // ANT1=0x01, ANT2=0x02, ANT3=0x03 → bits 24-26
    int antBits = m_alex.rxAnt & 0x03;
    if (antBits == 0x01) {
        reg |= (1 << 24);  // _ANT_1
    } else if (antBits == 0x02) {
        reg |= (1 << 25);  // _ANT_2
    } else if (antBits == 0x03) {
        reg |= (1 << 26);  // _ANT_3
    }

    // LPF bits — from Thetis netInterface.c:682-726
    // Bits map: 30_20[20], 60_40[21], 80[22], 160[23], 6[29], 12_10[30], 17_15[31]
    // Alex0's low-pass: the receive selection while receiving, the transmit
    // selection while keyed. From Thetis netInterface.c:705-719 [v2.10.3.15]
    //   if (isMox || !isTX) { ... AlexLPFMask = bits; }
    const quint8 lpf0 = effectiveLpfBitsAlex0();
    if (lpf0 & 0x01) { reg |= (1 << 20); }  // 30/20m
    if (lpf0 & 0x02) { reg |= (1 << 21); }  // 60/40m
    if (lpf0 & 0x04) { reg |= (1 << 22); }  // 80m
    if (lpf0 & 0x08) { reg |= (1 << 23); }  // 160m
    if (lpf0 & 0x10) { reg |= (1 << 29); }  // 6m
    if (lpf0 & 0x20) { reg |= (1 << 30); }  // 12/10m
    if (lpf0 & 0x40) { reg |= (1 << 31); }  // 17/15m

    // Level Cal: the Alex attenuator, network.h:284-285 [v2.10.3.15]
    //   _20_dB_Atten : 1, // bit 13
    //   _10_dB_Atten : 1, // bit 14 (RX MASTER IN SEL RL22)
    if (!m_hardwareProfile.mkiiBpf) {
        if (m_alex.atten20dB) { reg |= (1u << 13); }
        if (m_alex.atten10dB) { reg |= (1u << 14); }
    }

    // HPF bits — from Thetis netInterface.c:605-621
    // Bits map: 13MHz[1], 20MHz[2], 6M_preamp[3], 9.5MHz[4], 6.5MHz[5], 1.5MHz[6]
    // Phase 3F: ADC0's chain, per AlexController's decision when there is one.
    const quint8 hpf0 = effectiveRxHpfBitsAdc0();
    if (hpf0 & 0x01) { reg |= (1 << 1); }   // 13 MHz
    if (hpf0 & 0x02) { reg |= (1 << 2); }   // 20 MHz
    if (hpf0 & 0x04) { reg |= (1 << 4); }   // 9.5 MHz
    if (hpf0 & 0x08) { reg |= (1 << 5); }   // 6.5 MHz
    if (hpf0 & 0x10) { reg |= (1 << 6); }   // 1.5 MHz
    if (hpf0 & 0x20) { reg |= (1 << 12); }  // Bypass
    if (hpf0 & 0x40) { reg |= (1 << 3); }   // 6M preamp

    return reg;
}

// Build Alex1 32-bit register (bytes 1428-1431 in CmdHighPriority).
// Contains: TX antenna (bits 24-26), same LPF/HPF layout as Alex0.
// From Thetis ChannelMaster/network.h bpfilter2 struct.
quint32 P2RadioConnection::buildAlex1() const
{
    quint32 reg = 0;

    // TX antenna selection — same encoding as RX but in Alex1
    int antBits = m_alex.txAnt & 0x03;
    if (antBits == 0x01) {
        reg |= (1 << 24);  // _TXANT_1
    } else if (antBits == 0x02) {
        reg |= (1 << 25);  // _TXANT_2
    } else if (antBits == 0x03) {
        reg |= (1 << 26);  // _TXANT_3
    }

    // Alex1's low-pass is the TRANSMIT low-pass, so it takes the mask
    // derived from the transmit frequency — never Alex0's, which carries a
    // receive selection whenever the radio is not keyed. Mirroring Alex0
    // here is what let a receive retune onto a low band leave the
    // transmitter driving a low-pass below its own carrier.
    //   From Thetis ChannelMaster/netInterface.c:688-704 [v2.10.3.15]
    //     if (isMox || isTX) { ... Alex1LPFMask = bits; }
    const quint8 lpf1 = static_cast<quint8>(m_alex.lpfBitsTx);
    if (lpf1 & 0x01) { reg |= (1 << 20); }
    if (lpf1 & 0x02) { reg |= (1 << 21); }
    if (lpf1 & 0x04) { reg |= (1 << 22); }
    if (lpf1 & 0x08) { reg |= (1 << 23); }
    if (lpf1 & 0x10) { reg |= (1 << 29); }
    if (lpf1 & 0x20) { reg |= (1 << 30); }
    if (lpf1 & 0x40) { reg |= (1 << 31); }

    // HPF bits. Phase 3F: Alex1 is ADC1's own chain, so it takes ADC1's
    // decision when one exists (Thetis feeds it from setAlex2HPF, a separate
    // source from Alex0's setAlex1HPF — console.cs:15401 + 15435-15443 [v2.10.3.15]).
    // Upstream inline attribution preserved verbatim (console.cs:15441):
    //   HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    // With nothing on ADC1 this keeps the pre-Phase-3F mirror of Alex0.
    const quint8 hpf1 = static_cast<quint8>(
        m_alex.rxHpfBitsAdc1 >= 0 ? m_alex.rxHpfBitsAdc1
                                  : effectiveRxHpfBitsAdc0());
    if (hpf1 & 0x01) { reg |= (1 << 1); }
    if (hpf1 & 0x02) { reg |= (1 << 2); }
    if (hpf1 & 0x04) { reg |= (1 << 4); }
    if (hpf1 & 0x08) { reg |= (1 << 5); }
    if (hpf1 & 0x10) { reg |= (1 << 6); }
    if (hpf1 & 0x20) { reg |= (1 << 12); }
    if (hpf1 & 0x40) { reg |= (1 << 3); }

    return reg;
}

} // namespace NereusSDR
