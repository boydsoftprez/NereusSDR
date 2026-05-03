// =================================================================
// src/core/P2RadioConnection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/network.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/network.h, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/netInterface.c, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
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
//   2026-04-28 — setMicXlr (G.6): byte 50 bit 5 (0x20), P2-only, polarity 1=XLR. deskhpsdr new_protocol.c:1500-1502 [@120188f]. MicState::micControl default updated 0x04 -> 0x24. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

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

#include "P2RadioConnection.h"
#include "LogCategories.h"
#include "OcMatrix.h"
#include "CalibrationController.h"
#include "audio/TxMicSource.h"
#include "codec/AlexFilterMap.h"
#include "codec/P2CodecOrionMkII.h"
#include "codec/P2CodecSaturn.h"
#include "models/Band.h"

#include <QNetworkDatagram>
#include <QVariant>
#include <QtEndian>

namespace NereusSDR {

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
}

P2RadioConnection::~P2RadioConnection()
{
    if (m_running) {
        disconnect();
    }
}

// --- Thread Lifecycle ---
// Porting from Thetis nativeInitMetis() network.c:84
// Creates a single UDP socket, matching Thetis listenSock

void P2RadioConnection::init()
{
    m_socket = new QUdpSocket(this);

    // From Thetis nativeInitMetis:203 — bind to any available port
    if (!m_socket->bind(QHostAddress::Any, 0)) {
        qCWarning(lcConnection) << "P2: Failed to bind UDP socket";
        return;
    }

    // From Thetis nativeInitMetis:163-194 — socket buffer sizing
    // const int sndbuf_bytes = 0xfa000; const int rcvbuf_bytes = 0xfa000;
    m_socket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption,
                              QVariant(0xfa000));
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                              QVariant(0xfa000));

    connect(m_socket, &QUdpSocket::readyRead, this, &P2RadioConnection::onReadyRead);

    // From Thetis KeepAliveLoop network.c:1428 — timer fires every 500ms
    m_keepAliveTimer = new QTimer(this);
    m_keepAliveTimer->setInterval(kKeepAliveIntervalMs);
    connect(m_keepAliveTimer, &QTimer::timeout, this, &P2RadioConnection::onKeepAliveTick);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(3000);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &P2RadioConnection::onReconnectTimeout);

    // TX I/Q stream — radio expects continuous TX data on port 1029 even in
    // RX-only mode.  When the TX I/Q ring (m_txIqRing) has samples, they are
    // drained into the frame; when empty, zeros (silence) are sent.
    //
    // At 192 kHz TX rate: each 240-sample frame represents 240/192000 = 1.25 ms.
    // With a 5 ms timer tick, we need to send 4 frames per tick to match the
    // producer rate (238 samples/5 ms ≈ 190 kHz ≈ 4 × 240 samples drained).
    //
    // Bench fix round 3 (Issue C): previous code sent only 1 frame per 5 ms tick
    // (= 240 samples / 5 ms = 48 kHz drain rate), so the ring would saturate
    // immediately when the producer pushes 952 samples per 5 ms at 192 kHz.
    // Fix: drain kTxFramesPerTick frames per tick.
    //
    // From pcap: ~800 packets/sec at 192 kHz / 240 spp.
    // Cite: deskhpsdr/src/new_protocol.c:1929-1935 [@120188f]
    //   "Ideally, a TX IQ buffer with 240 sample is sent every 1250 usecs."
    //
    // From Thetis netInterface.c:1513 [v2.10.3.13] — tx sampling_rate = 192.
    // kTxFramesPerTick = 192000 Hz × 0.005 s / 240 spp = 4 frames per 5 ms tick.
    static constexpr int kTxSamplesPerSec = 192000;
    static constexpr int kTxSamplesPerFrame = 240;
    static constexpr int kTxTimerIntervalMs = 5;
    static constexpr int kTxFramesPerTick =
        (kTxSamplesPerSec * kTxTimerIntervalMs / 1000 + kTxSamplesPerFrame - 1)
        / kTxSamplesPerFrame;   // = ceil(960 / 240) = 4
    m_txIqTimer = new QTimer(this);
    m_txIqTimer->setInterval(kTxTimerIntervalMs);
    connect(m_txIqTimer, &QTimer::timeout, this, [this]() {
        if (!m_running || !m_socket) {
            return;
        }

        // Drain kTxFramesPerTick (4) frames per 5 ms tick at 192 kHz.
        // Each frame: 4-byte BE sequence number + 240 samples × 6 bytes = 1444 bytes.
        // Cite: deskhpsdr/src/new_protocol.h:37 [@120188f]
        //   #define TX_IQ_FROM_HOST_PORT 1029
        // Cite: deskhpsdr/src/new_protocol.c:1945-1948 [@120188f]
        //   iqbuffer[0..3] = tx_iq_sequence BE bytes; tx_iq_sequence++;
        //
        // IMPORTANT: keep the inner drain loop in sync with txIqFrameForTest() in
        // P2RadioConnection.h.  The test seam mirrors this byte-packing path
        // exactly so wire-byte snapshot tests stay authoritative.  If you
        // change the encoding here (gain, clip bounds, byte order, layout),
        // update the helper to match — there is no shared composer function
        // (E.5 fixup convention).
        static constexpr int kTxPktLen = 4 + 240 * 6;

        // Float → int24 converter.  Shared across all frames in this tick.
        // Cite: deskhpsdr/src/new_protocol.c:2795 [@120188f]
        //   void new_protocol_iq_samples(int isample, int qsample)
        auto toInt24 = [](float v) -> int {
            const float scaled = v * 8388607.0f;
            if (scaled >= 8388607.0f)  { return  8388607; }
            if (scaled <= -8388607.0f) { return -8388607; }
            return static_cast<int>(scaled);
        };

        for (int frame = 0; frame < kTxFramesPerTick; ++frame) {
            char buf[kTxPktLen];
            memset(buf, 0, sizeof(buf));
            writeBE32(buf, 0, m_seqTxIq++);

            // Drain up to 240 samples from the ring into the payload, or send
            // zeros if the ring is empty (silence — matches deskhpsdr underrun).
            // Cite: deskhpsdr/src/new_protocol.c:1950-1956, 2811-2816 [@120188f]
            //   memcpy(&iqbuffer[4], &TXIQRINGBUF[txiq_outptr], 1440);
            for (int s = 0; s < 240; ++s) {
                int i24 = 0;
                int q24 = 0;
                // acquire: makes audio-thread byte writes (published via release
                // fetch_add on m_txIqRingCount) visible before we read m_txIqRing.
                if (m_txIqRingCount.load(std::memory_order_acquire) >= 2) {
                    int rp = m_txIqRingRead.load(std::memory_order_relaxed);
                    const float fI = m_txIqRing[rp];
                    rp = (rp + 1) % kTxIqRingCapacityFloats;
                    const float fQ = m_txIqRing[rp];
                    rp = (rp + 1) % kTxIqRingCapacityFloats;
                    // relaxed: single writer on this side; acquire on m_txIqRingCount
                    // above already provides the required ordering fence.
                    m_txIqRingRead.store(rp, std::memory_order_relaxed);
                    // relaxed: audio thread observes this via acquire on m_txIqRingCount.
                    m_txIqRingCount.fetch_sub(2, std::memory_order_relaxed);

                    i24 = toInt24(fI);
                    q24 = toInt24(fQ);
                }
                // Pack 3-byte BE I, then 3-byte BE Q.
                // Cast via quint32 to guarantee arithmetic right-shift semantics
                // on the high bytes; the sign bit is already represented in two's
                // complement by the int → quint32 reinterpretation.
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

            QByteArray pkt(buf, sizeof(buf));
            // 3M-1a (2026-04-27): TX I/Q port is base + 5 (= 1029), NOT
            // base + 4 (= 1028; that's the RX-audio port).  Verified by
            // pcap: NereusSDR was sending all 240-sample TX I/Q packets
            // to port 1028 the whole time, which the radio routes to its
            // RX-audio sink and discards — exciter saw zero TX samples,
            // PA stayed silent, no carrier on the SO-239.
            // Source: Thetis network.c:1388 [v2.10.3.13]:
            //   sendPacket(..., prn->base_outbound_port + 5);// 1029);
            // Source: deskhpsdr/src/new_protocol.h:37 [@120188f]:
            //   #define TX_IQ_FROM_HOST_PORT 1029
            m_socket->writeDatagram(pkt, m_radioInfo.address, m_baseOutboundPort + 5);
        }
    });

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
    m_p2HeartbeatTimer = new QTimer(this);
    m_p2HeartbeatTimer->setInterval(100);
    m_p2HeartbeatTimer->setTimerType(Qt::PreciseTimer);
    connect(m_p2HeartbeatTimer, &QTimer::timeout, this, [this]() {
        if (!m_running) { return; }
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

    qCDebug(lcConnection) << "P2: init() socket port:" << m_socket->localPort();
}

// --- Connection Lifecycle ---
// Porting from Thetis SendStart() network.c:362
// prn->run = 1; CmdGeneral(); CmdRx(); CmdTx(); CmdHighPriority();

void P2RadioConnection::connectToRadio(const RadioInfo& info)
{
    if (m_running) {
        disconnect();
    }

    m_radioInfo = info;
    m_intentionalDisconnect = false;
    m_totalIqPackets = 0;

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
    m_wdt = 1;  // Watchdog timer MUST be enabled — radio requires it for streaming

    // From Thetis console.cs:8216 UpdateDDCs() for ANAN-G2 (OrionMkII/Saturn)
    // In non-diversity, non-PureSignal RX mode:
    //   DDCEnable = DDC2 (bit 2), Rate[2] = rx1_rate
    // This means DDC2 is the primary receiver, not DDC0!
    // From Thetis console.cs:8234-8241
    // Upstream inline attribution preserved verbatim (console.cs:8238):
    //   if (p1) Rate[0] = rx1_rate; // [2.10.3.13]MW0LGE p1 !
    m_rx[2].enable = 1;
    // Phase 3P-I-a bench-bug fix (KG4VCF 2026-04-22):
    // RadioModel::connectToRadio queues setReceiverFrequency BEFORE
    // this method in the worker-thread FIFO — so by the time we run,
    // m_rx[2].frequency already holds the persisted VFO (e.g. 14.3 MHz
    // for 20m) and m_alex.hpfBits/lpfBits reflect the same. Only seed
    // the 80m default when nothing has been stored yet (m_rx[2].frequency==0);
    // the prior unconditional assign clobbered the real VFO, so the
    // initial CmdHighPriority packet went out with DDC2 tuned to 80m
    // but BPF bits for 20m — radio heard nothing until the next
    // setReceiverFrequency fired from a user tune. The comment that used
    // to read "overridden by setReceiverFrequency" had the FIFO order
    // backwards.
    if (m_rx[2].frequency == 0) {
        m_rx[2].frequency = 3865000;   // 80m LSB — first-boot default only
        double freqMhz = m_rx[2].frequency / 1.0e6;
        m_alex.hpfBits = NereusSDR::codec::alex::computeHpf(freqMhz);
        m_alex.lpfBits = NereusSDR::codec::alex::computeLpf(freqMhz);
    }
    // DDC2 samplingRate is set by setSampleRate() which RadioModel queues
    // before connectToRadio in the FIFO (see RadioModel::connectToRadio).
    // Do NOT hardcode a rate here — it would stomp the user-selected value.

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
    }

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
    m_txIqTimer->start();
    // 3M-1a (2026-04-27): protocol heartbeat — high-pri every 100 ms, etc.
    m_p2HeartbeatCycle = 0;
    m_p2HeartbeatTimer->start();

    setState(ConnectionState::Connected);

    // Arm the connect watchdog: if no first DDC I/Q frame arrives within
    // kConnectTimeoutMs, emit connectFailed(Timeout, ...).
    // processIqPacket() cancels this on the first valid frame. Phase 3Q Task 3.
    if (m_connectWatchdog) {
        m_connectWatchdog->start(kConnectTimeoutMs);
    }
}

// Porting from Thetis SendStop() network.c:372
// prn->run = 0; CmdHighPriority();

void P2RadioConnection::disconnect()
{
    m_intentionalDisconnect = true;

    if (m_keepAliveTimer) {
        m_keepAliveTimer->stop();
    }
    if (m_txIqTimer) {
        m_txIqTimer->stop();
    }
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

    if (m_running && m_socket && !m_radioInfo.address.isNull()) {
        // From Thetis SendStop() network.c:372-376
        m_running = false;       // prn->run = 0;
        sendCmdHighPriority();   // CmdHighPriority();
        qCDebug(lcConnection) << "P2: SendStop complete (run=0)";
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

    // Update Alex HPF/LPF based on new frequency
    // From Thetis console.cs:6830-7234 [@501e3f5] — auto-select band filters
    // Upstream inline attribution preserved verbatim:
    //   :6830  || (HardwareSpecific.Hardware == HPSDRHW.HermesIII)) //DK1HLM
    double freqMhz = frequencyHz / 1e6;
    m_alex.hpfBits = NereusSDR::codec::alex::computeHpf(freqMhz);
    m_alex.lpfBits = NereusSDR::codec::alex::computeLpf(freqMhz);

    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setTxFrequency(quint64 frequencyHz)
{
    m_tx[0].frequency = static_cast<int>(frequencyHz);
    if (m_running) {
        sendCmdHighPriority();
    }
}

void P2RadioConnection::setActiveReceiverCount(int count)
{
    // Clamp to board-reported maximum if caps are available.
    // kMaxRxStreams (12) is the wire-protocol ceiling; board caps may be lower.
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
    // From Thetis: sampling_rate stored as kHz value (48, 96, 192, 384)
    int rateKhz = sampleRate / 1000;
    for (int i = 0; i < kMaxRxStreams; ++i) {
        m_rx[i].samplingRate = rateKhz;
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
    const int maxDb = m_caps ? m_caps->attenuator.maxDb : 31;
    m_adc[0].rxStepAttn = qBound(minDb, dB, maxDb);
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
        return;  // idempotent — periodic cadence covers any state drift
    }
    m_mox = enabled;
    if (m_running) {
        sendCmdHighPriority();  // immediate emit on state change for low latency
    }
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
// setWatchdogEnabled — Phase 3M-0 Task 5
//
// Records the requested watchdog enable state in the base-class
// m_watchdogEnabled field (shared with P1).
//
// From Thetis NetworkIOImports.cs:197-198 [v2.10.3.13]:
//   [DllImport("ChannelMaster.dll", CallingConvention = CallingConvention.Cdecl)]
//   public static extern void SetWatchdogTimer(int bits);
//
// The callsite (setup.cs:17986 [v2.10.3.13]):
//   NetworkIO.SetWatchdogTimer(Convert.ToInt32(chkNetworkWDT.Checked));
//
// NOTE: P2RadioConnection already carries m_wdt (int, maps to prn->wdt) which
// is set to 1 unconditionally in connectToRadio() because the radio requires
// the watchdog for streaming. m_watchdogEnabled records the *user* toggle from
// Setup → Network WDT checkbox; the relationship to m_wdt is unresolved.
//
// 3M-1a Task E.8 — DEFERRED with documented blocker
// (pre-code review §7.8, "P2 BPF2Gnd / Alex T/R / Network watchdog —
//  DEFERRED to research"):
//
//   "deskhpsdr does not currently emit a P2 watchdog command.  Likely a
//    Saturn-specific register; documented blocker."
//   "P2 watchdog wire bit stays a state-tracking stub.  Update the TODO
//    comment to reference this pre-code review §7.8 and file a tracking
//    issue."
//
// State-only stub: setWatchdogEnabled stores the requested value in the
// base-class m_watchdogEnabled field (default true, set by E.5).  No P2
// wire emission.  P1 wire bit was resolved in E.5 (RUNSTOP pkt[3] bit 7).
//
// Tracking: see GitHub issue (filed post-merge — link to be added when
// the issue number is known).  Re-port path: when Saturn register layout
// is identified (likely via deskhpsdr saturndrivers.c / saturnregisters.c
// once they document the watchdog control register), restore the wire-bit
// emission via sendCmdGeneral() and remove the deferral note.
// Cite: NetworkIOImports.cs:197-198 [v2.10.3.13] (DllImport entry that
// indirects through ChannelMaster.dll's closed-source watchdog handler).
// ---------------------------------------------------------------------------
void P2RadioConnection::setWatchdogEnabled(bool enabled)
{
    if (m_watchdogEnabled == enabled) {
        return;
    }
    m_watchdogEnabled = enabled;
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
// If the ring is full, excess samples are dropped (matches deskhpsdr overflow).
//
// The m_txIqTimer lambda (connection thread) drains pairs per frame.
//
// No HL2 CWX LSB-clear workaround needed for P2: the workaround applies to
// P1 old_protocol only (deskhpsdr/src/old_protocol.c:2441-2453 [@120188f]).
// deskhpsdr new_protocol.c has no analogous &=~1 pattern near this path.
// ---------------------------------------------------------------------------
void P2RadioConnection::sendTxIq(const float* iq, int n)
{
    if (n <= 0 || iq == nullptr) { return; }

    for (int k = 0; k < n * 2; k += 2) {
        // acquire: see the latest fetch_sub from the connection thread so we
        // don't overfill after a drain.  Pair count: each sample = 2 floats.
        if (m_txIqRingCount.load(std::memory_order_acquire) >= kTxIqRingCapacityFloats - 1) {
            qCDebug(lcConnection) << "P2 TX I/Q ring buffer overflow — dropping samples";
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
    }
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
    m_puresignalRun = run;
    // No wire emission: P2's PureSignal feedback DDC routing is 3M-4 deferred.
    // When 3M-4 lands, this setter will additionally toggle the feedback DDC
    // enable byte in CmdHighPriority and trigger sendCmdTx() / sendCmdHigh()
    // as appropriate.
}

// ---------------------------------------------------------------------------
// setMicPTT (3M-1b G.5)
//
// Enables or disables the hardware mic-jack PTT line (Orion/ANAN front-panel).
// NereusSDR parameter convention: enabled=true → PTT enabled (intuitive).
//
// POLARITY INVERSION AT THE WIRE LAYER:
// deskhpsdr carries the *disable* flag at byte 50 bit 2 (mask 0x04):
//   mic_ptt_enabled == 0 → set the bit (bit set means PTT is DISABLED)
// Thetis console.cs:19758 MicPTTDisabled property confirms the disable-flag
//   convention — MicPTTDisabled=true (PTT off) maps to the wire bit = 1.
// Thetis networkproto1.c case 11 C1 bit 6 (0x40) also carries the disable flag.
// Therefore this implementation writes (!enabled) to the wire bit:
//   enabled=true  → PTT enabled  → bit 2 CLEAR (0)
//   enabled=false → PTT disabled → bit 2 SET   (1)
//
// Wire byte: transmit_specific_buffer[50] bit 2 (mask 0x04), INVERTED.
//
// Porting from deskhpsdr/src/new_protocol.c:1488-1490 [@120188f]:
//   if (mic_ptt_enabled == 0) { // set if disabled
//     transmit_specific_buffer[50] |= 0x04;
//   }
//
// Cross-reference:
//   deskhpsdr/src/old_protocol.c:3000-3002 [@120188f] — P1 same inversion at C1 bit 6.
//   Thetis console.cs:19764 [v2.10.3.13] — MicPTTDisabled calls SetMicPTT(value).
//
// Note: P2 bit position (bit 2 = 0x04) differs from P1 bit position
// (bit 6 = 0x40 in C1 of bank 11). Both carry the same inverted semantics.
// ---------------------------------------------------------------------------
void P2RadioConnection::setMicPTT(bool enabled)
{
    if (m_micPTT == enabled) {
        return;  // idempotent — 100 ms heartbeat covers any state drift
    }
    m_micPTT = enabled;
    // POLARITY INVERSION: mic_ptt_enabled == 0 → bit 2 SET (PTT disabled on wire).
    // setMicPTT(true)  = PTT enabled  → wire bit 2 CLEAR.
    // setMicPTT(false) = PTT disabled → wire bit 2 SET.
    // From deskhpsdr/src/new_protocol.c:1488-1490 [@120188f]:
    //   if (mic_ptt_enabled == 0) { transmit_specific_buffer[50] |= 0x04; }
    if (!enabled) {
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
// [v2.10.3.13]: broadcasts the TX step attenuator value to all ADCs.
// P2 frame layout: bytes 57-59 carry per-ADC TX step ATT
// (P2CodecOrionMkII.cpp lines 252-254).
// ---------------------------------------------------------------------------
void P2RadioConnection::setTxStepAttenuation(int dB)
{
    if (dB < 0)  { dB = 0; }
    if (dB > 31) { dB = 31; }
    for (auto& adc : m_adc) {
        adc.txStepAttn = dB;
    }
}

// --- UDP Reception ---
// Porting from Thetis ReadUDPFrame() network.c:481
// Single socket, dispatch by source port: inport = ntohs(fromaddr.sin_port)

void P2RadioConnection::onReadyRead()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();
        quint16 sourcePort = datagram.senderPort();

        // From Thetis ReadUDPFrame:514-515
        // inport = ntohs(fromaddr.sin_port);
        // int portIdx = inport - prn->p2_custom_port_base;
        int portIdx = sourcePort - m_p2CustomPortBase;

        // Filter out spurious empty datagrams (Windows loopback from our own sends)
        if (data.isEmpty()) {
            continue;
        }

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
            if (m_txMicSource != nullptr) {
                std::array<float, 64> samples{};
                if (decodeMicFrame132(data, samples)) {
                    m_txMicSource->inbound(samples.data(), 64);
                    m_lastMicAt = QDateTime::currentDateTimeUtc();
                }
            }
            break;

        case 2:  // 1027: wideband ADC data
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            // From Thetis ReadUDPFrame:550-603 (wideband, not used yet)
            break;

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

// From Thetis KeepAliveLoop network.c:1417-1440
// Fires every 500ms, sends CmdGeneral when running
void P2RadioConnection::onKeepAliveTick()
{
    // From Thetis network.c:1436
    // if (prn->run && prn->wdt) CmdGeneral();
    // Note: we send CmdGeneral unconditionally when running (wdt=0 means no watchdog,
    // but keepalive still runs per Thetis behavior)
    if (m_running && !m_radioInfo.address.isNull()) {
        sendCmdGeneral();
    }

    // Phase 3M-1c TX pump v3: mic-frame LOS injection.
    // Mirrors Thetis network.c:655-666 [v2.10.3.13] — when no mic
    // datagram has arrived for kMicLosTimeoutMs, push a zero block into
    // the TX inbound ring so the worker keeps ticking through silence.
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
        default:
            m_codec = std::make_unique<P2CodecOrionMkII>();
            break;
    }
    qCInfo(lcConnection) << "P2: selected codec for board"
                         << int(m_hardwareProfile.effectiveBoard);
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
    // Today that is the main thread, because RadioModel::connectToRadio
    // calls setTxMicSource at line 1764-1767 BEFORE the connection is
    // moved to its worker thread at line 1842.  The assignment + the
    // m_lastMicAt arming below therefore race-free with the connection-
    // thread reads in onKeepAliveTick / decodeMicFrame132 callsites.
    // If a future refactor reorders these RadioModel calls, this
    // function will need atomic / mutex protection.
    m_txMicSource = src;

    // Stage-2 review fix I3: arm the LOS timer at attach time so the
    // mic-LOS zero-block injection (onKeepAliveTick) fires even if the
    // radio never delivers a mic frame.  Without this, m_lastMicAt
    // stays default-constructed (invalid) and the guard at
    // P2RadioConnection.cpp:1285 short-circuits forever — the worker
    // would block on waitForBlock(INFINITE) with no recovery.
    //
    // Mirrors Thetis network.c:655-666 [v2.10.3.13] — WSA_WAIT_TIMEOUT
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

    // Per-ADC dither + random
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
        const bool txInBand = (txBand != Band::GEN && txBand != Band::WWV);
        ctx.p2DriveLevel = txInBand ? m_tx[0].driveLevel : 0;
    }
    ctx.p2TxPa           = m_tx[0].pa;
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

    // Alex HPF / LPF bits (recomputed by setReceiverFrequency on freq change)
    ctx.alexHpfBits = static_cast<quint8>(m_alex.hpfBits);
    ctx.alexLpfBits = static_cast<quint8>(m_alex.lpfBits);

    // Port / wideband config
    ctx.p2CustomPortBase     = m_p2CustomPortBase;
    ctx.p2WbSamplesPerPacket = m_wbSamplesPerPacket;
    ctx.p2WbSampleSize       = m_wbSampleSize;
    ctx.p2WbUpdateRate       = m_wbUpdateRate;
    ctx.p2WbPacketsPerFrame  = m_wbPacketsPerFrame;

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
    // No P2 codec reads ctx.ocByte yet; populated here symmetrically with P1
    // so Phase F P2 OC wiring can consume it without further changes.
    // Phase 3P-D Task 3 — From Thetis HPSDR/Penny.cs:117-132 [@501e3f5]
    if (m_ocMatrix) {
        const quint64 rx0Hz = static_cast<quint64>(m_rx[0].frequency);
        const Band currentBand = bandFromFrequency(static_cast<double>(rx0Hz));
        ctx.ocByte = m_ocMatrix->maskFor(currentBand, m_mox);  // 3M-1a E.7: was m_tx[0].pttOut != 0
    } else {
        ctx.ocByte = 0;
    }

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
    if (m_useLegacyP2Codec || !m_codec) {
        composeCmdHighPriorityLegacy(buf);
        return;
    }
    const CodecContext ctx = buildCodecContext();
    quint8 tmp[kBufLen] = {};
    m_codec->composeCmdHighPriority(ctx, tmp);
    memcpy(buf, tmp, kBufLen);
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
    buf[23] = 0;    // wb_enable
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
        const bool txInBand = (txBand != Band::GEN && txBand != Band::WWV);
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
    m_socket->writeDatagram(pkt, m_radioInfo.address, m_baseOutboundPort + 3);
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
    if (seq != (1 + m_rx[ddcIndex].rxInSeqNo) && seq != 0
        && m_rx[ddcIndex].rxInSeqNo != 0) {
        m_rx[ddcIndex].rxInSeqErr += 1;
        qCDebug(lcProtocol) << "P2: DDC" << ddcIndex
                            << "seq error this:" << seq
                            << "last:" << m_rx[ddcIndex].rxInSeqNo;
    }
    m_rx[ddcIndex].rxInSeqNo = seq;

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
    } else if (m_totalIqPackets % 10000 == 0) {
        qCDebug(lcProtocol) << "P2: I/Q packets:" << m_totalIqPackets;
    }

    // Per-frame activity signal for TitleBar LED (throttled to 10 Hz by
    // the receiver). Design §4.1.
    emit frameReceived();

    emit iqDataReceived(ddcIndex, buf);
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
                            << kConnectTimeoutMs << "ms; emitting connectFailed(Timeout)";

    emit connectFailed(ConnectFailure::Timeout,
                       QStringLiteral("No response from radio within %1 ms — "
                                      "check IP address, radio power, and network")
                           .arg(kConnectTimeoutMs));
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
        case HPSDRModel::ANAN_G2:
        case HPSDRModel::ANAN_G2_1K:
        case HPSDRModel::ANVELINAPRO3:
            handleUserAdc0Raw(userAdc0Raw);
            break;
        default:
            break;
        }
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
    // Use floating-point for the correction, then convert to quint32.
    // When factor == 1.0, the result is byte-identical to the pre-cal formula.
    // Use 64-bit math to avoid overflow: freq * 2^32 / 122880000
    const double correctedHz = static_cast<double>(freqHz) * factor;
    return static_cast<quint32>((correctedHz * 4294967296.0) / 122880000.0);
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
    if (m_alex.lpfBits & 0x01) { reg |= (1 << 20); }  // 30/20m
    if (m_alex.lpfBits & 0x02) { reg |= (1 << 21); }  // 60/40m
    if (m_alex.lpfBits & 0x04) { reg |= (1 << 22); }  // 80m
    if (m_alex.lpfBits & 0x08) { reg |= (1 << 23); }  // 160m
    if (m_alex.lpfBits & 0x10) { reg |= (1 << 29); }  // 6m
    if (m_alex.lpfBits & 0x20) { reg |= (1 << 30); }  // 12/10m
    if (m_alex.lpfBits & 0x40) { reg |= (1 << 31); }  // 17/15m

    // HPF bits — from Thetis netInterface.c:605-621
    // Bits map: 13MHz[1], 20MHz[2], 6M_preamp[3], 9.5MHz[4], 6.5MHz[5], 1.5MHz[6]
    if (m_alex.hpfBits & 0x01) { reg |= (1 << 1); }   // 13 MHz
    if (m_alex.hpfBits & 0x02) { reg |= (1 << 2); }   // 20 MHz
    if (m_alex.hpfBits & 0x04) { reg |= (1 << 4); }   // 9.5 MHz
    if (m_alex.hpfBits & 0x08) { reg |= (1 << 5); }   // 6.5 MHz
    if (m_alex.hpfBits & 0x10) { reg |= (1 << 6); }   // 1.5 MHz
    if (m_alex.hpfBits & 0x20) { reg |= (1 << 12); }  // Bypass
    if (m_alex.hpfBits & 0x40) { reg |= (1 << 3); }   // 6M preamp

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

    // Same LPF bits as Alex0 (TX uses same LPF selection)
    if (m_alex.lpfBits & 0x01) { reg |= (1 << 20); }
    if (m_alex.lpfBits & 0x02) { reg |= (1 << 21); }
    if (m_alex.lpfBits & 0x04) { reg |= (1 << 22); }
    if (m_alex.lpfBits & 0x08) { reg |= (1 << 23); }
    if (m_alex.lpfBits & 0x10) { reg |= (1 << 29); }
    if (m_alex.lpfBits & 0x20) { reg |= (1 << 30); }
    if (m_alex.lpfBits & 0x40) { reg |= (1 << 31); }

    // Same HPF bits
    if (m_alex.hpfBits & 0x01) { reg |= (1 << 1); }
    if (m_alex.hpfBits & 0x02) { reg |= (1 << 2); }
    if (m_alex.hpfBits & 0x04) { reg |= (1 << 4); }
    if (m_alex.hpfBits & 0x08) { reg |= (1 << 5); }
    if (m_alex.hpfBits & 0x10) { reg |= (1 << 6); }
    if (m_alex.hpfBits & 0x20) { reg |= (1 << 12); }
    if (m_alex.hpfBits & 0x40) { reg |= (1 << 3); }

    return reg;
}

} // namespace NereusSDR
