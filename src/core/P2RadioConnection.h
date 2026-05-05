#pragma once

// =================================================================
// src/core/P2RadioConnection.h  (NereusSDR)
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
//   2026-05-04 — setMicPTT renamed to setMicPTTDisabled (issue #182): direct polarity matches Thetis console.cs:19757-19766 [v2.10.3.13+501e3f51]; default MicState::micControl flipped 0x24→0x20 so PTT is enabled at firmware out of the box. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "RadioConnection.h"
#include "BoardCapabilities.h"

#include <QDateTime>
#include <QUdpSocket>
#include <QTimer>
#include <QVector>

#include <array>
#include <memory>

#include "codec/IP2Codec.h"
#include "codec/CodecContext.h"

namespace NereusSDR { class OcMatrix; }              // forward decl — full header in .cpp
namespace NereusSDR { class CalibrationController; } // forward decl — Phase 3P-G

namespace NereusSDR {

// Protocol 2 connection for Orion MkII / Saturn (ANAN-G2) radios.
//
// Faithfully ported from Thetis ChannelMaster/network.c and network.h.
// Uses a single UDP socket matching Thetis listenSock.
// State structs mirror Thetis _radionet (network.h:53).
class P2RadioConnection : public RadioConnection {
    Q_OBJECT

public:
    explicit P2RadioConnection(QObject* parent = nullptr);
    ~P2RadioConnection() override;

    int getAdcForDdc(int ddc) const override;

    // Protocol identifier — 2 for OpenHPSDR P2.  See RadioConnection::protocolVersion.
    int protocolVersion() const override { return 2; }

public slots:
    void init() override;
    void connectToRadio(const NereusSDR::RadioInfo& info) override;
    void disconnect() override;

    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) override;
    void setTxFrequency(quint64 frequencyHz) override;
    void setActiveReceiverCount(int count) override;
    void setSampleRate(int sampleRate) override;
    void setAttenuator(int dB) override;
    void setPreamp(bool enabled) override;
    void setTxDrive(int level) override;
    void setMox(bool enabled) override;
    void setAntennaRouting(AntennaRouting routing) override;
    void setWatchdogEnabled(bool enabled) override;
    void sendTxIq(const float* iq, int n) override;
    void setTrxRelay(bool enabled) override;
    void setTxStepAttenuation(int dB) override;
    void setMicBoost(bool on) override;
    void setLineIn(bool on) override;
    void setMicTipRing(bool tipHot) override;
    void setMicBias(bool on) override;
    void setLineInGain(int gain) override;
    void setUserDigOut(quint8 dig) override;
    void setPuresignalRun(bool run) override;
    void setMicPTTDisabled(bool disabled) override;
    void setMicXlr(bool xlrJack) override;

    // Bench fix round 3 (Issue B): P2 TX I/Q output is always at 192 kHz.
    // This rate is used by WdspEngine::createTxChannel() to open the WDSP
    // TX channel with the correct outputSampleRate so WDSP's rsmpout stage
    // (TXA stage 29) delivers fexchange2 Iout/Qout at 192000 samples/sec.
    //
    // From Thetis netInterface.c:1513 [v2.10.3.13]:
    //   prn->tx[i].sampling_rate = 192;  // P2 TX always 192 kHz
    // Stored as m_tx[i].samplingRate (kHz); return value in Hz.
    int txSampleRate() const override { return m_tx[0].samplingRate * 1000; }

    // Phase 3P-B Task 10: per-ADC RX1 preamp control for OrionMKII family.
    // Routes to m_rx[1].preamp → CodecContext.p2Rx1Preamp →
    // P2CodecOrionMkII::composeCmdHighPriority byte 1403 bit 1.
    // ADC0 preamp uses the existing setPreamp(bool) (byte 1403 bit 0).
    void setRx1Preamp(bool enabled);

    // Wire RadioModel's OcMatrix so buildCodecContext() can set ctx.ocByte
    // from maskFor(currentBand, mox).  No P2 codec reads ocByte yet — this
    // is a symmetric companion to P1 so the field is ready when P2 OC
    // support lands.  Phase 3P-D Task 3.
    void setOcMatrix(const OcMatrix* matrix);

    // Wire CalibrationController so hzToPhaseWord() can multiply by
    // effectiveFreqCorrectionFactor(). When null (default 1.0 behaviour),
    // the phase word is byte-identical to pre-calibration output.
    // Phase 3P-G.
    void setCalibrationController(const CalibrationController* cal);

    // Phase 3M-1c TX pump v3: wire the radio-mic cadence source.
    // Mic frames arrive on UDP port 1026 (132 bytes each carrying 64
    // samples).  Each frame is decoded and pushed into
    // TxMicSource::inbound() so the network thread is the cadence source
    // for TxWorkerThread.  Mirrors Thetis network.c:761-772 [v2.10.3.13].
    // Owned by RadioModel; this class keeps a non-owning pointer.
    void setTxMicSource(class TxMicSource* src);

    // Static decoder for a 132-byte P2 mic frame (port 1026).  Used by
    // onReadyRead() and exposed for unit tests.  Output is exactly 64
    // mono float samples (mic.spp=64 from netInterface.c:1458 [v2.10.3.13]).
    // Returns false if data.size() != 132.
    static bool decodeMicFrame132(const QByteArray& data,
                                  std::array<float, 64>& outSamples,
                                  quint32* outSeq = nullptr) noexcept;

private slots:
    void onReadyRead();
    void onKeepAliveTick();
    void onReconnectTimeout();
    // Fires kConnectTimeoutMs after connectToRadio() if no first DDC I/Q frame
    // arrives. Emits connectFailed(Timeout, ...) — Phase 3Q Task 3.
    void onConnectTimeout();

private:
    // --- Phase 3P-B: per-board codec chosen at connectToRadio() time ---
    std::unique_ptr<IP2Codec> m_codec;
    bool m_useLegacyP2Codec{false};

    void selectCodec();
    CodecContext buildCodecContext() const;

    // Legacy compose paths — preserved for the NEREUS_USE_LEGACY_P2_CODEC rollback flag.
    void composeCmdGeneralLegacy     (char buf[60])   const;
    void composeCmdHighPriorityLegacy(char buf[1444]) const;
    void composeCmdRxLegacy          (char buf[1444]) const;
    void composeCmdTxLegacy          (char buf[60])   const;

    // --- Command composers (extract buffer-fill logic; sendCmd* calls these then UDP-sends) ---
    void composeCmdGeneral(char buf[60]) const;               // network.c:821
    void composeCmdHighPriority(char buf[1444]) const;        // network.c:913  (1444 == kBufLen)
    void composeCmdRx(char buf[1444]) const;                  // network.c:1066 (1444 == kBufLen)
    void composeCmdTx(char buf[60]) const;                    // network.c:1181

    // --- Command senders (compose + UDP dispatch) ---
    void sendCmdGeneral();       // network.c:821 → port 1024
    void sendCmdHighPriority();  // network.c:913 → port 1027
    void sendCmdRx();            // network.c:1066 → port 1025
    void sendCmdTx();            // network.c:1181 → port 1026

    void processIqPacket(const QByteArray& data, int ddcIndex);
    void processHighPriorityStatus(const QByteArray& data);

    static void writeBE32(char* buf, int offset, quint32 value);

    // From pcap analysis: phase_word = freq_hz * 2^32 / 122880000
    // General cmd byte 37 bit 3 = 1 means radio expects phase words, not Hz.
    quint32 hzToPhaseWord(quint64 freqHz) const;  // non-static: reads m_calController

    // --- Constants from Thetis network.h ---
    static constexpr int kMaxRxStreams = 12;  // network.h:34 MAX_RX_STREAMS
    static constexpr int kMaxTxStreams = 3;   // network.h:35 MAX_TX_STREAMS
    static constexpr int kMaxAdc = 3;         // network.h:33 MAX_ADC
    static constexpr int kMaxDdc = 7;         // DDC0-DDC6
    static constexpr int kBufLen = 1444;      // Thetis BUFLEN
    static constexpr int kKeepAliveIntervalMs = 500; // network.c:1428

    // --- Board capabilities (set in connectToRadio, used for clamp/dispatch) ---
    const BoardCapabilities* m_caps{nullptr};

    // Non-owning pointer to RadioModel's OcMatrix.  When non-null,
    // buildCodecContext() fills ctx.ocByte via
    // m_ocMatrix->maskFor(bandFromFrequency(rx0Hz), m_mox).
    // No P2 codec reads ocByte yet; the field is populated for symmetry
    // with P1 so Phase F P2 OC wiring can read it without further changes.
    // Phase 3P-D Task 3.
    const OcMatrix* m_ocMatrix{nullptr};

    // Non-owning pointer to RadioModel's CalibrationController.
    // When non-null, hzToPhaseWord() multiplies by effectiveFreqCorrectionFactor().
    // Default null → factor 1.0 → byte-identical to pre-cal output.
    // Phase 3P-G.
    const CalibrationController* m_calController{nullptr};

    // --- Single socket (Thetis listenSock, network.c:67) ---
    QUdpSocket* m_socket{nullptr};
    QTimer* m_keepAliveTimer{nullptr};
    QTimer* m_reconnectTimer{nullptr};
    QTimer* m_txIqTimer{nullptr};
    // Single-shot connect watchdog — fires kConnectTimeoutMs after
    // connectToRadio() if no first DDC I/Q frame arrives. Emits
    // connectFailed(Timeout, ...). Cancelled in processIqPacket() on first
    // valid frame. Created in init(), Qt-parent-owned. Phase 3Q Task 3.
    QTimer* m_connectWatchdog{nullptr};

    // Connect watchdog budget — 2 s matches P1.
    static constexpr int kConnectTimeoutMs = 2000;

    // Periodic protocol heartbeat: fires every 100 ms while connected and
    // dispatches HighPri / RX-spec / TX-spec / General on the cycling cadence
    // documented in deskhpsdr/src/new_protocol.c:2870-2898 [@120188f].
    // The radio expects the high-priority packet at 100 ms intervals to keep
    // TX state fresh — without it, MOX + drive bytes go stale and the PA
    // never engages.  3M-1a, 2026-04-27.
    QTimer* m_p2HeartbeatTimer{nullptr};
    int     m_p2HeartbeatCycle{0};   // 0..7, drives RX/TX-spec + General cadence

    // --- Port configuration (from Thetis _radionet, network.h:55-56) ---
    int m_p2CustomPortBase{1025};    // prn->p2_custom_port_base
    int m_baseOutboundPort{1024};    // prn->base_outbound_port

    // Phase 3M-1c TX pump v3: non-owning TxMicSource pointer.  Set via
    // setTxMicSource() at connect time; null in tests that don't wire
    // RadioModel.  onReadyRead() port-1026 case decodes 132-byte mic
    // frames and pushes via inbound(); LOS injection on watchdog tick.
    class TxMicSource* m_txMicSource{nullptr};
    QDateTime m_lastMicAt;
    static constexpr int kMicLosTimeoutMs = 3000;  // network.c:656 [v2.10.3.13]

    // --- Run state (from Thetis _radionet, network.h:65-66) ---
    bool m_running{false};           // prn->run
    int m_wdt{0};                    // prn->wdt (watchdog timer, 0=disabled)
    bool m_intentionalDisconnect{false};

    // --- MOX (transmit) state (3M-1a E.7) ---
    // Separate from m_tx[0].pttOut.  pttOut is the rear-panel PTT-out relay
    // (a TX-confirmation output, not the MOX initiator).  m_mox is the
    // software-asserted transmit state that maps to byte 4 bit 1 (0x02) of
    // CmdHighPriority.
    //
    // From deskhpsdr/src/new_protocol.c:739-762 [@120188f]:
    //   high_priority_buffer_to_radio[4] = P2running;   // bit 0 = run
    //   if (xmit) { high_priority_buffer_to_radio[4] |= 0x02; }  // bit 1 = MOX
    //
    // THREAD SAFETY: m_mox must only be written from the connection thread.
    // All compose functions read it on the connection thread.  Cross-thread
    // callers (e.g., MoxController on main thread post-F.1) must dispatch via
    // QMetaObject::invokeMethod with Qt::QueuedConnection, matching the
    // existing pattern for setTxFrequency / setRxFrequency.
    bool m_mox{false};

    // --- Hardware config (from Thetis _radionet) ---
    int m_numAdc{1};                 // prn->num_adc
    int m_numDac{1};                 // prn->num_dac

    // --- Sequence counters ---
    quint32 m_seqGeneral{0};
    quint32 m_seqRx{0};
    quint32 m_seqTx{0};
    quint32 m_seqTxIq{0};
    quint32 m_seqHighPri{0};
    quint32 m_ccSeqNo{0};            // prn->cc_seq_no

    // ── TX I/Q ring buffer (3M-1a E.6) ────────────────────────────────────────
    // Pre-allocated interleaved [I0,Q0,I1,Q1,...] float samples.
    //
    // P2 wire format: 240 samples per UDP frame (1444 bytes total):
    //   4-byte BE sequence number + 1440-byte payload (240 × 6 bytes).
    //   Each sample: 3-byte BE signed int24 I + 3-byte BE signed int24 Q.
    //   Float→int24 gain: 8388607.0 (0x7FFFFF).
    //
    // Layout ported from deskhpsdr/src/new_protocol.c:2795-2837 [@120188f]
    // (new_protocol_iq_samples / TXIQRINGBUFLEN).
    //
    // kTxIqRingCapacityFloats: sized to hold one full fexchange2 output block
    // (kTxDspBufferSize = 4096 samples) plus headroom for the consumer drain.
    //
    // Background: TxChannel::driveOneTxBlock() calls fexchange2 with n =
    // WdspEngine::kTxDspBufferSize (4096) samples and pushes 4096 complex pairs
    // = 8192 floats per call via sendTxIq().  The previous ring size of 2048
    // floats (= 1024 sample pairs) was 4× smaller than a single producer block,
    // causing 75% sample loss every tick (3M-1a bench fix, 2026-04-26).
    //
    // New size: 16384 floats = 8192 complex samples.
    //   - One fexchange2 block at 4096 samples = 8192 floats → ring must be ≥ 8192.
    //   - Rounded up to the next power-of-two (16384) for simpler modular arithmetic.
    //   - Consumer drains 240 samples (480 floats) per 5 ms tick → ring holds ~170 ticks
    //     of headroom, which is more than enough to absorb the 42.67 ms block cadence.
    //   - deskhpsdr reference: TXIQRINGBUFLEN 97920 (85 msec ring) — we remain leaner
    //     than deskhpsdr while fixing the overflow.
    //   Source: deskhpsdr/src/new_protocol.c:186 [@120188f]
    //     TXIQRINGBUFLEN 97920  (85 msec; NereusSDR uses a leaner in-memory float ring)
    //
    // SPSC ring buffer — audio thread (sendTxIq) writes, connection thread
    // (m_txIqTimer lambda) reads.  Same atomic-ordering discipline as P1:
    //   - m_txIqRingWrite: single audio-thread writer; relaxed store;
    //     the release on m_txIqRingCount publishes the float writes.
    //   - m_txIqRingRead: single connection-thread writer; relaxed store;
    //     the acquire on m_txIqRingCount makes audio writes visible.
    //   - m_txIqRingCount: cross-thread fetch_add (audio, release) /
    //     fetch_sub (conn, relaxed).  The release/acquire pair provides
    //     the memory ordering fence for the float data.
    //
    // CLAUDE.md mandates atomics for cross-thread DSP parameters.
    static constexpr int kTxIqRingCapacityFloats = 16384;  // ≥1 full DSP block; power-of-two
    std::array<float, kTxIqRingCapacityFloats> m_txIqRing{};
    std::atomic<int> m_txIqRingWrite{0};  // audio thread writes; relaxed store
    std::atomic<int> m_txIqRingRead{0};   // connection thread writes; relaxed store
    std::atomic<int> m_txIqRingCount{0};  // both threads: fetch_add (audio, release) / fetch_sub (conn)

    // --- RX state (from Thetis _radionet._rx, network.h:191-213) ---
    struct RxState {
        int id{0};
        int rxAdc{0};                // prn->rx[i].rx_adc
        int frequency{0};            // prn->rx[i].frequency (Hz)
        int enable{0};               // prn->rx[i].enable
        int sync{0};                 // prn->rx[i].sync
        int samplingRate{48};        // prn->rx[i].sampling_rate (kHz value)
        int bitDepth{24};            // prn->rx[i].bit_depth
        int preamp{0};               // prn->rx[i].preamp
        int spp{238};                // prn->rx[i].spp (IQ-samples per packet)
        quint32 rxInSeqNo{0};        // prn->rx[i].rx_in_seq_no
        quint32 rxInSeqErr{0};       // prn->rx[i].rx_in_seq_err
    };
    std::array<RxState, kMaxRxStreams> m_rx;

    // --- TX state (from Thetis _radionet._tx, network.h:215-236) ---
    struct TxState {
        int id{0};
        int frequency{0};            // prn->tx[i].frequency (Hz)
        int samplingRate{192};       // From Thetis create_rnet (netInterface.c:1513) — P2 always 192
        int cwx{0};                  // prn->tx[i].cwx
        int dash{0};
        int dot{0};
        int pttOut{0};               // prn->tx[i].ptt_out
        int driveLevel{0};           // prn->tx[i].drive_level
        int phaseShift{0};           // prn->tx[i].phase_shift
        int pa{0};                   // prn->tx[i].pa — DisablePA flag (1 = PA OFF)
        int epwmMax{0};
        int epwmMin{0};
    };
    std::array<TxState, kMaxTxStreams> m_tx;

    // --- ADC state (from Thetis _radionet._adc, network.h:125-140) ---
    struct AdcState {
        int rxStepAttn{0};           // prn->adc[i].rx_step_attn
        int txStepAttn{31};          // prn->adc[i].tx_step_attn (default 31 from create_rnet:1472)
        int dither{0};
        int random{0};
    };
    std::array<AdcState, kMaxAdc> m_adc;

    // --- CW state (from Thetis _radionet._cw, network.h:142-167) ---
    struct CwState {
        int sidetoneLevel{0};
        int sidetoneFreq{0};
        int keyerSpeed{0};
        int keyerWeight{0};
        int hangDelay{0};
        int rfDelay{0};
        int edgeLength{7};           // From create_rnet:1454
        unsigned char modeControl{0};
    };
    CwState m_cw;

    // --- Mic state (from Thetis _radionet._mic, network.h:169-189) ---
    // mic_control bit-field (from Thetis network.c:1227-1233):
    //   Bit 0: Line In (0=off, 1=on)
    //   Bit 1: Mic Boost (0=off, 1=on)
    //   Bit 2: Orion Mic PTT (0=enabled, 1=disabled) — INVERTED POLARITY
    //   Bit 3: Tip/Ring (0=ptt-ring/mic-tip, 1=ptt-tip/mic-ring) — INVERTED POLARITY
    //   Bit 4: Mic Bias (0=disabled, 1=enabled)
    //   Bit 5: Balanced Input (0=disabled, 1=enabled, Saturn only)
    //
    // Initial value 0x20: reflects one default-set bit:
    //   bit 2 (0x04) CLEAR = PTT enabled at firmware (matches m_micPTTDisabled=false
    //     default in RadioConnection.h — direct polarity: false = 0 on wire).
    //     From Thetis console.cs:19757 [v2.10.3.13+501e3f51]:
    //       private bool mic_ptt_disabled = false;
    //   bit 5 (0x20) SET = XLR jack selected by default (matches m_micXlr=true
    //     default in RadioConnection.h — no inversion: true = 1 on wire).
    //     deskhpsdr src/new_protocol.c:1500-1502 [@120188f]: mic_input_xlr → set bit.
    //     Saturn G2 ships with XLR-enabled config; default true per pre-code review §2.7.
    // Bit 3 CLEAR = Tip-is-mic (matches m_micTipRing=true default — !true = 0 on wire).
    //
    // Issue #182: bit 2 was previously SET (0x24) to mark "PTT disabled" out of
    // the box, which orphaned the mic-jack PTT line on every Protocol 2 OrionMKII
    // / Saturn family board because no model→connection wiring ever cleared it.
    // Default now matches Thetis mic_ptt_disabled=false out of the box.
    struct MicState {
        unsigned char micControl{0x20};  // PTT enabled (bit 2 clear) + XLR selected (bit 5)
        int lineInGain{0};
    };
    MicState m_mic;

    // --- Wideband settings (from Thetis create_rnet:1461-1466) ---
    int m_wbSamplesPerPacket{512};
    int m_wbSampleSize{16};
    int m_wbUpdateRate{70};
    int m_wbPacketsPerFrame{32};

    // --- Alex filter/antenna state ---
    // From Thetis ChannelMaster/network.h bpfilter struct
    // Each Alex register is a 32-bit value written to CmdHighPriority bytes 1428-1435.
    // Alex0 (bytes 1432-1435): RX antenna, HPF bits, LPF bits
    // Alex1 (bytes 1428-1431): TX antenna, HPF/LPF bits
    struct AlexState {
        // Antenna selection — from Thetis netInterface.c:459-499 SetAntBits
        int rxAnt{1};       // 1=ANT1, 2=ANT2, 3=ANT3
        int txAnt{1};       // 1=ANT1, 2=ANT2, 3=ANT3
        int hpfBits{0x20};  // HPF filter bits (default: bypass = 0x20)
        int lpfBits{0x10};  // LPF filter bits (default: 6m LPF = 0x10)

        // RX-only antenna mux — from Thetis ChannelMaster/network.h:279-281
        // [v2.10.3.13 @501e3f5]. Alex0 bits 8-10:
        //   0 = no RX-only path, 1 = _Rx_1_In (bit 10), 2 = _Rx_2_In (bit 9),
        //   3 = _XVTR_Rx_In (bit 8).
        // Encoding per netInterface.c:479-481 SetAntBits().
        int  rxOnlyAnt{0};

        // _Rx_1_Out relay (K36 RL17 RX-Bypass-Out) — from Thetis
        // ChannelMaster/network.h:282 [v2.10.3.13 @501e3f5]. Alex0 bit 11.
        bool rxOut{false};
    };
    AlexState m_alex;

    // Build Alex0 and Alex1 32-bit register values from current state.
    quint32 buildAlex0() const;
    quint32 buildAlex1() const;

    // --- DDC→ADC mapping register (from Thetis network.c rx_adc_ctrl1) ---
    quint32 m_rxAdcCtrl1{0};

    // --- I/Q buffers and packet counters ---
    std::array<QVector<float>, kMaxDdc> m_iqBuffers;
    int m_totalIqPackets{0};

#ifdef NEREUS_BUILD_TESTS
public:
    // Test-only helpers — allow unit tests to inject board state without a live radio.
    void setBoardForTest(HPSDRHW board) {
        m_caps = &BoardCapsTable::forBoard(board);
        switch (board) {
            case HPSDRHW::OrionMKII:  m_hardwareProfile.model = HPSDRModel::ORIONMKII; break;
            case HPSDRHW::Saturn:     m_hardwareProfile.model = HPSDRModel::ANAN_G2;   break;
            case HPSDRHW::SaturnMKII: m_hardwareProfile.model = HPSDRModel::ANAN_G2;   break;
            default:                  m_hardwareProfile.model = HPSDRModel::ORIONMKII; break;
        }
        m_hardwareProfile.effectiveBoard = board;
        selectCodec();
    }
    // Expose compose methods for regression-freeze capture (Task 1) and gate test (Task 7).
    void composeCmdGeneralForTest(quint8 buf[60]) const {
        char tmp[60];
        memset(tmp, 0, sizeof(tmp));
        composeCmdGeneral(tmp);
        memcpy(buf, tmp, 60);
    }
    void composeCmdHighPriorityForTest(quint8 buf[kBufLen]) const {
        char tmp[kBufLen];
        memset(tmp, 0, sizeof(tmp));
        composeCmdHighPriority(tmp);
        memcpy(buf, tmp, kBufLen);
    }
    void composeCmdRxForTest(quint8 buf[kBufLen]) const {
        char tmp[kBufLen];
        memset(tmp, 0, sizeof(tmp));
        composeCmdRx(tmp);
        memcpy(buf, tmp, kBufLen);
    }
    void composeCmdTxForTest(quint8 buf[60]) const {
        char tmp[60];
        memset(tmp, 0, sizeof(tmp));
        composeCmdTx(tmp);
        memcpy(buf, tmp, 60);
    }
    // Expose the High-Priority status packet parser so PA-telemetry tests can
    // feed a hand-crafted 60-byte packet and assert paTelemetryUpdated() emits
    // the expected raw values.  Phase 3P-H Task 4.
    void processHighPriorityStatusForTest(const QByteArray& data) {
        processHighPriorityStatus(data);
    }

    // txIqFrameForTest — 3M-1a Task E.6 test seam
    //
    // Feeds n interleaved float I/Q samples through sendTxIq() (audio-thread
    // producer side), then composes ONE 1444-byte TX I/Q frame from the ring
    // without sending a UDP datagram, and returns the frame as a QByteArray.
    //
    // IMPORTANT: this function must keep in sync with the production composition
    // path in the m_txIqTimer lambda (P2RadioConnection.cpp, E.6 consumer).
    // Any change to the 24-bit BE packing or sequence-number write in the
    // production path must be reflected here identically.
    //
    // Cite: deskhpsdr/src/new_protocol.c:1945-1956 [@120188f]
    //   (production send path structure: 4-byte seq + 1440-byte payload)
    QByteArray txIqFrameForTest(const float* iq, int n) {
        if (n > 0 && iq != nullptr) {
            sendTxIq(iq, n);
        }

        // 1444 bytes: 4-byte BE sequence number + 1440-byte payload.
        static constexpr int kTxPktLen = 4 + 240 * 6;
        QByteArray frame(kTxPktLen, '\0');
        auto* buf = reinterpret_cast<quint8*>(frame.data());

        // Sequence number (4-byte BE) — mirrors the production consumer.
        buf[0] = static_cast<quint8>((m_seqTxIq >> 24) & 0xFF);
        buf[1] = static_cast<quint8>((m_seqTxIq >> 16) & 0xFF);
        buf[2] = static_cast<quint8>((m_seqTxIq >>  8) & 0xFF);
        buf[3] = static_cast<quint8>( m_seqTxIq        & 0xFF);
        ++m_seqTxIq;

        // Drain up to 240 samples from ring into payload — mirrors production consumer.
        // Cite: deskhpsdr/src/new_protocol.c:2811-2816 [@120188f]
        for (int s = 0; s < 240; ++s) {
            int i24 = 0;
            int q24 = 0;
            if (m_txIqRingCount.load(std::memory_order_acquire) >= 2) {
                int rp = m_txIqRingRead.load(std::memory_order_relaxed);
                const float fI = m_txIqRing[rp];
                rp = (rp + 1) % kTxIqRingCapacityFloats;
                const float fQ = m_txIqRing[rp];
                rp = (rp + 1) % kTxIqRingCapacityFloats;
                m_txIqRingRead.store(rp, std::memory_order_relaxed);
                m_txIqRingCount.fetch_sub(2, std::memory_order_relaxed);

                // Float → int24; clamp to ±8388607.
                // Cite: deskhpsdr/src/new_protocol.c:2795 [@120188f] (isample/qsample args are int)
                auto toInt24 = [](float v) -> int {
                    const float scaled = v * 8388607.0f;
                    if (scaled >= 8388607.0f)  { return  8388607; }
                    if (scaled <= -8388607.0f) { return -8388607; }
                    return static_cast<int>(scaled);
                };
                i24 = toInt24(fI);
                q24 = toInt24(fQ);
            }
            // Pack 3-byte BE I then 3-byte BE Q into payload.
            // Cite: deskhpsdr/src/new_protocol.c:2811-2816 [@120188f]
            const int offset = 4 + s * 6;
            const quint32 ui = static_cast<quint32>(i24);
            const quint32 uq = static_cast<quint32>(q24);
            buf[offset + 0] = static_cast<quint8>((ui >> 16) & 0xFF);
            buf[offset + 1] = static_cast<quint8>((ui >>  8) & 0xFF);
            buf[offset + 2] = static_cast<quint8>( ui        & 0xFF);
            buf[offset + 3] = static_cast<quint8>((uq >> 16) & 0xFF);
            buf[offset + 4] = static_cast<quint8>((uq >>  8) & 0xFF);
            buf[offset + 5] = static_cast<quint8>( uq        & 0xFF);
        }

        return frame;
    }

    // Return number of floats currently buffered in the TX I/Q ring.
    int txIqRingCountForTest() const { return m_txIqRingCount.load(std::memory_order_acquire); }
#endif
};

} // namespace NereusSDR
