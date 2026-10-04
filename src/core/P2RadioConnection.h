#pragma once

// =================================================================
// src/core/P2RadioConnection.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/network.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/network.h, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/netInterface.c, original licence from Thetis source is included below
//   Project Files/Source/ChannelMaster/obbuffs.c, original licence from Thetis source is included below
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
//   2026-09-22 — Established UDP silence: Thetis ChannelMaster/network.c:655-666 [v2.10.3.15]; stop/report, daemon-owned recovery; Nereus per-ADC capture epochs.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
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
//   2026-09-28 - R-R3-49 / R-R3-46: Setup > Transmit > Power's Disable HF PA
//                applied (Thetis DisablePA and hf_tr_relay,
//                transmitSettingsVersion 11). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: the Alex receive attenuator (Thetis SetAlexAtten,
//                netInterface.c:421-432 [v2.10.3.15]) on the wire, and the step
//                attenuator range above 31 dB on Alex boards (value + 2,
//                console.cs:11044-11056 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal fix wave: setRx2Preamp, the second receiver's
//                preamp bit (Thetis SetRX2Preamp, netInterface.c:758-767
//                [v2.10.3.15]). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - Radio codec: byte 50 starts with mic boost on (0x22), as
//                Thetis mic_boost = true (console.cs:13259 [v2.10.3.15]).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Radio codec: the receive audio stream to port 1028
//                (serviceRadioAudioSend). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-30 - Radio codec review: radioAudioStats adds the stream's
//                packet and send-error counters; rx_out_seq_no starts at 0
//                once per connection, not at each sender start. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - onReadyRead brackets each socket drain with iqBatchStarted /
//                iqBatchFinished so ReceiverManager posts the drain's I/Q to
//                the DSP worker once per stream, and frameReceived is posted
//                once per drain, not once per packet. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Shared-input filters (ruling (c)): m_countedSlotsAdc0,
//                the DDCs the receive low-pass follows. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-30 - bindToRadioFacingAddress, applySocketBufferSizes,
//                kRouteLookupMs, radioFacingAddress and the receive-socket
//                test seams: the receive socket binds the address that
//                reaches the radio (network.c:116-118, 203 [v2.10.3.15]).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01 - TX diagnostics lane: the send thread places the key's
//                padded silence (start, mid-key, tail), its first radio ran
//                dry and its catch-up bursts in time (txSendStats). Measurement
//                only. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                Code.
//   2026-10-01 - TX diagnostics lane, review round: the unkey tail's start,
//                the microphone frame sequence number to the TX pump's wake
//                watch, and its figures in txSendStats. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
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

#include "core/NereusCoreExport.h"
#include "RadioConnection.h"
#include "BoardCapabilities.h"
#include "WidebandFrameAccumulator.h"

#include <QDateTime>
#include <QDeadlineTimer>
#include <QThread>
#include <QUdpSocket>
#include <QTimer>
#include <QVector>

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>

#include "codec/IP2Codec.h"
#include "codec/CodecContext.h"
#include "DdcAssignment.h"

namespace NereusSDR { class OcMatrix; }              // forward decl — full header in .cpp
namespace NereusSDR { class CalibrationController; } // forward decl — Phase 3P-G

namespace NereusSDR {

// Protocol 2 connection for Orion MkII / Saturn (ANAN-G2) radios.
//
// Faithfully ported from Thetis ChannelMaster/network.c and network.h.
// Uses a single UDP socket matching Thetis listenSock.
// State structs mirror Thetis _radionet (network.h:53).
class NEREUS_CORE_EXPORT P2RadioConnection : public RadioConnection {
    Q_OBJECT

public:
    explicit P2RadioConnection(QObject* parent = nullptr);
    ~P2RadioConnection() override;

    // Radio codec (2026-09-30): the receive audio goes to the radio's own
    // speaker out on port 1028 (base + 4), from the send thread.
    bool carriesRadioAudio() const noexcept override { return true; }

    int getAdcForDdc(int ddc) const override;

    // Protocol identifier — 2 for OpenHPSDR P2.  See RadioConnection::protocolVersion.
    int protocolVersion() const override { return 2; }

    // primaryRxDdcForBoard — which DDC carries RX1 I/Q on the wire for this board.
    //
    // 2-ADC P2 boards (Angelia / Orion / OrionMKII / Saturn / SaturnMKII) place
    // RX1 on DDC2 because DDC0 and DDC1 are reserved for the diversity /
    // PureSignal pair. 1-ADC P2 boards (Hermes / HermesII — ANAN-10E /
    // ANAN-100B running community P2 firmware) place RX1 on DDC0 with no
    // such reservation.
    //
    // From Thetis console.cs:8554-8632 GetDDC() P2 branch [v2.10.3.13]:
    // Upstream tags preserved: //N1GP (from cited console.cs:8612) [v2.10.3.15]
    //   case HPSDRHW.Angelia / Orion / OrionMKII / Saturn:   rx1 = DDC2, rx2 = DDC3
    //   case HPSDRHW.Hermes / HermesII:                       rx1 = DDC0, rx2 = DDC1
    //
    // Upstream inline attribution preserved verbatim (console.cs:8559):
    //   case HPSDRHW.Saturn:        // ANAN-G2, G21K    (G8NJJ)
    //
    // Defaults to 2 for unknown / future boards (matches the prior NereusSDR
    // hardcode and is the right answer for every modern Apache Labs P2 SKU).
    static int primaryRxDdcForBoard(HPSDRHW board) noexcept;

public slots:
    void init() override;
    void connectToRadio(const NereusSDR::RadioInfo& info) override;
    void disconnect() override;

    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) override;
    void setTxFrequency(quint64 frequencyHz) override;
    void setLiveReceiverSlots(quint32 slotMask) override;
    void setHpfBypassOnTx(bool on) override;
    void setHpfBypassOnPs(bool on) override;
    void setAlexHpfBypass(bool on) override;
    void setPaDisabled(bool disabled) override;
    void setAlexHpfEdges(const codec::alex::AlexHpfEdges& edges) override;
    // The Alex-1 low-pass rows (stored for the next selection) and
    // 6m/ByPass on RX (re-selects at once).
    void setAlexLpfEdges(const codec::alex::AlexLpfEdges& edges) override;
    void setAlexLpfBypass(bool on) override;
    void setDisable6mLna(bool onRx, bool onTx) override;
    void onBandOutputPinsChanged() override;
    void setReceiverVfoFrequencies(const QVector<quint64>& vfoHzBySlot) override;
    void setActiveReceiverCount(int count) override;
    void setSampleRate(int sampleRate) override;
    void setAttenuator(int dB) override;
    void setAttenuatorForAdc(int adc, int dB) override;
    void setPreamp(bool enabled) override;
    void setTxDrive(int level) override;
    void setMox(bool enabled) override;
    void setAntennaRouting(AntennaRouting routing) override;
    void setAlexRxBpf(AlexRxBpf bpf) override;
    // Level Cal: the Alex receive attenuator (Thetis SetAlexAtten).
    void setAlexAtten(int bits) override;
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

    // Phase 3F Sub-Epic B Task 15: apply a multi-slice DDC assignment from
    // the codec.  Updates m_rx[i].{enable, samplingRate, rxAdc, sync} from the
    // struct, then re-sends CmdRx so the radio reconfigures its DDCs.
    //
    // This is the sole Protocol 2 DDC wire writer. PureSignal and diversity
    // transitions recompute this full assignment instead of dispatching a
    // second, partial PsDdcConfig writer.
    //
    // Mirrors Thetis console.cs:8527-8534 UpdateDDCs() [v2.10.3.15]:
    //   NetworkIO.EnableRxs(ddcEnable);
    //   NetworkIO.EnableRxSync(0, syncEnable);
    //   for (int i = 0; i < 4; i++) NetworkIO.SetDDCRate(i, rate[i]);
    //   NetworkIO.SetADC_cntrl1(cntrl1);
    //   NetworkIO.SetADC_cntrl2(cntrl2);
    void applyDdcAssignment(const DdcAssignment& assignment);

    // Bench fix round 3 (Issue B): P2 TX I/Q output is always at 192 kHz.
    // This rate is used by WdspEngine::createTxChannel() to open the WDSP
    // TX channel with the correct outputSampleRate so WDSP's rsmpout stage
    // (TXA stage 29) delivers fexchange2 Iout/Qout at 192000 samples/sec.
    //
    // From Thetis netInterface.c:1513 [v2.10.3.13]:
    //   prn->tx[i].sampling_rate = 192;  // P2 TX always 192 kHz
    // Stored as m_tx[i].samplingRate (kHz); return value in Hz.
    int txSampleRate() const override { return m_tx[0].samplingRate * 1000; }

public:
    // R-IOS-13, R-R3-42: the send path's counters since the last key.
    TxSendStats txSendStats() const override;
    // Radio codec: the ring's counters plus the port 1028 stream's.
    RadioAudioStats radioAudioStats() const override
    {
        RadioAudioStats st = RadioConnection::radioAudioStats();
        st.hasPackets = true;
        st.packetsSent = m_radioAudioPacketsSent.load(std::memory_order_relaxed);
        st.sendErrors = m_radioAudioSendErrors.load(std::memory_order_relaxed);
        return st;
    }
    double txIqQueuedMs() const override;
    // G-05: the unkey's wait for the send ring (RadioConnection).
    bool txIqRingDrained() const override;
    double txIqRingLengthMs() const override;

    // What a transmit I/Q frame sink did with a frame: sent it; refused it
    // for now (a full socket buffer: keep it and retry next pass); or failed
    // for good (counted as a send error and not retried).
    enum class TxIqSinkResult { Sent, Retry, Failed };
    using TxIqFrameSink = TxIqSinkResult (*)(void* ctx, const char* frame, int len);

public slots:

    // Phase 3P-B Task 10: per-ADC RX1 preamp control for OrionMKII family.
    // Routes to m_rx[1].preamp → CodecContext.p2Rx1Preamp →
    // P2CodecOrionMkII::composeCmdHighPriority byte 1403 bit 1.
    // ADC0 preamp uses the existing setPreamp(bool) (byte 1403 bit 0).
    void setRx1Preamp(bool enabled);
    // Level Cal: Thetis SetRX2Preamp is the same prn->rx[1].preamp.
    void setRx2Preamp(bool enabled) override { setRx1Preamp(enabled); }

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

    // Phase 3M-4 Task 17 chunk B/E: read-only access to the per-board
    // codec for ReceiverManager::setP2Codec injection.  Returns nullptr
    // until selectCodec() runs at connectToRadio time; subscribers
    // should listen to p2CodecChanged() to know when this becomes valid.
    NereusSDR::IP2Codec* p2Codec() const { return m_codec.get(); }

    // Phase 3F Sub-Epic F Task 1: enable the wideband ADC stream for the
    // given ADC index. Bit N of m_wbEnableMask corresponds to ADCN.
    // See Thetis network.c:879 [v2.10.3.15] for the wire format (CmdGeneral
    // byte 23). When in Connected state, triggers a CmdGeneral send so
    // the radio learns the new mask promptly. No-op when adcIndex is out
    // of range (0..7) or when the resulting mask is unchanged.
    void setWidebandEnabled(int adcIndex, bool on);

    // Stable lifetime token for asynchronous wideband consumers. The shared
    // atomic remains readable after this connection is destroyed; each capture
    // boundary publishes a new nonzero epoch with release ordering.
    // Invalid ADC indices return an empty pointer.
    std::shared_ptr<const std::atomic<quint64>> widebandCaptureEpoch(int adcIndex) const;

    // Phase 3F Sub-Epic F Task 1: read current wideband per-ADC enable
    // mask. Used by buildCodecContext to thread the value into CodecContext
    // for the codec-driven composeCmdGeneral path.
    quint8 wbEnableMask() const { return m_wbEnableMask; }

signals:
    // Phase 3M-4 Task 17 chunk B/E: emitted from selectCodec() once
    // m_codec is assigned.  RadioModel::wireConnectionSignals subscribes
    // and forwards p2Codec() into ReceiverManager::setP2Codec so the
    // ddcConfigChanged observation path (for example PsccPump) becomes live.
    // Protocol 2 wire state is written only by applyDdcAssignment().
    void p2CodecChanged();

    // Phase 3F Sub-Epic F Task 3: emitted once a full 32-packet wideband
    // frame (16384 normalized samples) has been assembled for the given
    // ADC index by the matching per-ADC WidebandFrameAccumulator. Consumed
    // by WidebandFftEngine (Task 4) / SpectrumWidget extended-pan view
    // (Task 5+).
    void widebandFrameReady(int adcIndex, QVector<float> samples);

    // Nereus-original capture identity attached before any direct observer can
    // change the ADC lifetime. Consumers compare this value with the retained
    // widebandCaptureEpoch() token before and after asynchronous processing.
    void widebandFrameReadyForGeneration(int adcIndex,
                                         quint64 captureGeneration,
                                         QVector<float> samples,
                                         qint64 producedAtNs);
    /// Invalidates already-published survey data as well as queued FFT work.
    void widebandCaptureRetired(int adcIndex, quint64 captureGeneration);
    /// Applied local capture state (including idempotent requests), independent
    /// of first ADC data. This does not claim acknowledgement from the radio.
    void widebandCaptureStateApplied(int adcIndex, quint64 captureGeneration, bool enabled);

private slots:
    void onReadyRead();
    void onKeepAliveTick();
    void onReconnectTimeout();
    // Fires kConnectTimeoutMs after connectToRadio() if no first DDC I/Q frame
    // arrives. Emits connectFailed(Timeout, ...) — Phase 3Q Task 3.
    void onConnectTimeout();
    void onEstablishedSilenceTimeout();

private:
    int telemetryUdpBasePort() const override { return m_baseOutboundPort; }

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
    bool isSelectedSourceAddress(const QHostAddress& sender) const;
    void noteAcceptedInboundDatagram(quint64 datagramGeneration);
    void stopForEstablishedSilence();
    void advanceWidebandCaptureEpoch(int adcIndex);
    void advanceAllWidebandCaptureEpochs();
    void discardWidebandFrames();

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

    // Disconnect timing (2026-07-27, ANAN-G2E lockup investigation).
    // kStopQuiesceMs: settle after stopping the command timers and before
    //   emitting run=0, so no CmdRx/CmdTx lands on top of the stop frame.
    //   One 100 ms heartbeat period covers the worst-case in-flight tick.
    // kStopDrainMs: let the kernel put the run=0 datagram on the wire before
    //   close(); Thetis keeps listenSock open across SendStop
    //   (network.c:398-404 StopReadThread) so it needs no equivalent.
    static constexpr int kStopQuiesceMs = 100;
    static constexpr int kStopDrainMs   = 20;

    // The receive socket listens on the local address that reaches the radio,
    // as Thetis's listenSock binds the network card's address
    // (network.c:116-118, 203 [v2.10.3.15]). kRouteLookupMs bounds the route
    // lookup (a UDP connect, which sends nothing and completes at once).
    void bindToRadioFacingAddress();
    void applySocketBufferSizes();
    static constexpr int kRouteLookupMs = 100;
    // The local address that reaches m_radioInfo.address (a UDP connect),
    // null if none does within kRouteLookupMs, or the test override.
    QHostAddress radioFacingAddress() const;
    // Test stand-in for the route lookup (setRadioFacingAddressForTest).
    // Unconditional members: the class layout must not depend on
    // NEREUS_BUILD_TESTS.
    bool m_radioFacingOverridden{false};
    QHostAddress m_radioFacingOverride;

    // MOX-off grace window (2026-07-27, Codex review PR #306).  After unkey
    // the 100 ms heartbeat keeps running this long so the MOX-off state is
    // retransmitted ~10 times instead of once; a single lost datagram would
    // otherwise leave the radio keyed indefinitely.  Only ever active
    // immediately after a transmission, so it does not reintroduce RX-idle
    // polling.  See setMox() for the full rationale.
    //
    // MONOTONIC, not wall-clock (Codex review, PR #306).  This was
    // QDateTime::currentMSecsSinceEpoch() arithmetic.  An NTP step forward
    // inside the window would make the grace read as already expired, so a
    // lost MOX-off datagram would stop being retransmitted and the radio
    // could stay keyed — precisely the hazard the window exists to close.
    // A step backward would hold RX polling open far past one second.
    // QDeadlineTimer measures against a monotonic source.  Default-constructed
    // is already expired, which is the "never armed" state.
    static constexpr qint64 kMoxOffGraceMs = 1000;
    QDeadlineTimer m_moxOffGrace;
    bool withinMoxOffGrace() const;

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
    // Single-shot connect watchdog — fires kConnectTimeoutMs after
    // connectToRadio() if no first DDC I/Q frame arrives. Emits
    // connectFailed(Timeout, ...). Cancelled in processIqPacket() on first
    // valid frame. Created in init(), Qt-parent-owned. Phase 3Q Task 3.
    QTimer* m_connectWatchdog{nullptr};

    // Thetis ChannelMaster/network.c:655-666 [v2.10.3.15] waits three
    // seconds for any inbound P2 UDP before sending stop. Nereus arms this
    // only after the first valid DDC packet establishes the stream, keeps a
    // monotonic deadline behind the Qt wakeup, then quiesces and reports loss
    // rather than retrying inside the transport.
    QTimer* m_establishedSilenceTimer{nullptr};
    QDeadlineTimer m_establishedSilenceDeadline;
    quint64 m_establishedSilenceGeneration{0};
    quint64 m_connectionGeneration{0};
    bool m_linkLossLatched{false};

    // Connect watchdog budget — 2 s matches P1.
    static constexpr int kConnectTimeoutMs = 2000;
    static constexpr int kEstablishedSilenceTimeoutMs = 3000;
    int m_connectTimeoutMs{kConnectTimeoutMs};
    int m_establishedSilenceTimeoutMs{kEstablishedSilenceTimeoutMs};

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
    int m_wdt{1};                    // prn->wdt; byte 38, always 1 so it is never 0 on the wire (R-R3-49, see setWatchdogEnabled)
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
    // THREAD SAFETY: m_mox is written only from the connection thread.  All
    // compose functions read it on the connection thread.  Cross-thread
    // callers (e.g., MoxController on main thread post-F.1) must dispatch via
    // QMetaObject::invokeMethod with Qt::QueuedConnection, matching the
    // existing pattern for setTxFrequency / setRxFrequency.
    //
    // Atomic because of one genuine cross-thread READER: sendTxIq() runs on
    // the TX/audio producer thread (it is the producer side of the SPSC ring
    // below, using explicit acquire/release on m_txIqRingCount) and gates its
    // producer-rate telemetry on m_mox.  A plain bool there is a data race
    // with setMox() on the connection thread.  Review blocker [P2] on PR
    // #291.  Matches the existing pattern for AudioEngine::m_moxActive.
    //
    // Default seq_cst ordering is deliberate: this is read once per
    // sendTxIq() call, not once per sample, so the ordering cost is
    // irrelevant next to the surrounding ring arithmetic.
    std::atomic<bool> m_mox{false};

    // --- PureSignal DDC pair (Phase 3M-4 bench-fix 2026-05-23) ───────────
    //
    // Latched from applyDdcAssignment() each time the per-board codec emits
    // a full assignment. Both default to -1 (no PS pair configured) so
    // the deinterleave loop in processIqPacket only emits the source-first
    // paired signal psPairedIqDataReceived when the codec has actually
    // configured a PS pair.
    //
    // Used to:
    //   * map deinterleaved stream slots back to (psFbDdc, txMonDdc) for
    //     RadioConnection::psPairedIqDataReceived emission;
    //   * mirror Thetis cmaster.cs:533-534 [v2.10.3.13] SetPSRxIdx /
    //     SetPSTxIdx convention — the per-board codec is authoritative.
    //
    // Read on the connection thread only (deinterleave runs there).
    int m_psFbDdc{-1};       // PS-feedback DDC (Thetis ps_rx_idx); -1 = no pair
    int m_psTxMonDdc{-1};    // TX-monitor DDC  (Thetis ps_tx_idx); -1 = no pair

    // --- DDC enable-mask ownership (Phase 3F Sub-Epic I closeout) ─────────
    //
    // False until a per-board codec has computed a mask for this session,
    // true from the first applyDdcAssignment() onward.
    // While true, setActiveReceiverCount() writes no enable bits.
    //
    // Upstream owns the mask in exactly one place. From Thetis
    // console.cs:8537 [v2.10.3.15]:
    //     NetworkIO.EnableRxs(DDCEnable);
    // and DDCEnable is produced entirely by UpdateDDCs's per-board
    // `switch (HardwareSpecific.Model)` (console.cs:8218-8534 [v2.10.3.15]).
    // There is no count-to-mask direction anywhere upstream; the direction
    // runs the other way. From Thetis ChannelMaster/netInterface.c:1229-1236
    // [v2.10.3.15], inside EnableRxs itself:
    //     if (RadioProtocol == USB)
    //     {
    //         sum = 0;
    //         for (i = 0; i < 4; i++)
    //         {
    //             sum += (prn->rx[i].enable);
    //         }
    //         nreceivers = sum;
    // i.e. the receiver COUNT is derived from the mask. The only other
    // writer, EnableRx(id, enable) (netInterface.c:1200-1209 [v2.10.3.15]),
    // takes an explicit DDC id from rxa.cs:54 and setup.cs:7054-7056, never
    // a DDC0..N-1 range.
    //
    // Cleared in connectToRadio() so a reconnect on the same object starts
    // a fresh session with the board-aware primary-DDC bootstrap in charge
    // until the codec speaks.
    //
    // Written and read on the connection thread only.
    bool m_ddcMaskOwnedByCodec{false};

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

    // ── TX I/Q ring buffer (3M-1a E.6; resized R-IOS-13, R-R3-42) ──────────────
    // Interleaved [I0,Q0,I1,Q1,...] float samples at the 192 kHz wire rate.
    //
    // P2 wire format: 240 samples per UDP frame (1444 bytes total):
    //   4-byte BE sequence number + 1440-byte payload (240 × 6 bytes).
    //   Each sample: 3-byte BE signed int24 I + 3-byte BE signed int24 Q.
    //   Float→int24 gain: 8388607.0 (0x7FFFFF).
    //
    // Layout ported from deskhpsdr/src/new_protocol.c:2795-2837 [@120188f]
    // (new_protocol_iq_samples / TXIQRINGBUFLEN).
    //
    // Size (R-IOS-13, R-R3-42): 131072 floats = 65536 sample pairs = 341 ms.
    // The old 16384 floats (42.7 ms) held the 20 ms cushion plus one
    // 4096-sample block and little else, so the first connection-thread
    // stall past about 20 ms overflowed it (the Rock, 2026-09-26: 7806
    // overflow lines in four keys). 341 ms covers a 200 ms stall with the
    // cushion and a block on top. Thetis's own ring is OBB_MULT * 2048 =
    // 4096 samples (obbuffs.h:43 [v2.10.3.15], create_obbuffs(1, 1, 2048,
    // prn->tx[0].spp) netInterface.c:1679 [v2.10.3.15]); its send thread
    // sends each frame as soon as it fills, so it never needs more.
    // deskhpsdr's is 85 ms (new_protocol.c:192 [@f3d857c]). The ring is on
    // the heap: tests build this class on the stack.
    //
    // A full ring is the one place samples are lost: sendTxIq keeps what
    // fits, counts the rest (m_txIqOverflowSamples) and logs a warning at
    // most once a second.
    //
    // SPSC ring buffer: the TX worker thread (sendTxIq) writes, the send
    // thread (serviceTxIqSend) reads.
    //   - m_txIqRingWrite: single producer writer; relaxed store;
    //     the release on m_txIqRingCount publishes the float writes.
    //   - m_txIqRingRead: single consumer writer; relaxed store;
    //     the acquire on m_txIqRingCount makes producer writes visible.
    //   - m_txIqRingCount: cross-thread fetch_add (producer, release) /
    //     fetch_sub (consumer, relaxed).
    //
    // CLAUDE.md mandates atomics for cross-thread DSP parameters.
    static constexpr int kTxIqRingCapacityFloats = 131072;  // power of two
    std::unique_ptr<float[]> m_txIqRing{std::make_unique<float[]>(kTxIqRingCapacityFloats)};
    std::atomic<int> m_txIqRingWrite{0};  // producer writes; relaxed store
    std::atomic<int> m_txIqRingRead{0};   // consumer writes; relaxed store
    std::atomic<int> m_txIqRingCount{0};  // both threads: fetch_add (producer, release) / fetch_sub (consumer)

    // TX I/Q ring pre-prime flag.  setMox(true) sets it on the connection
    // thread; sendTxIq consumes it on the TX worker thread (single-writer
    // invariant preserved -- only the worker mutates the ring write index
    // and count).  When consumed, sendTxIq pushes a cushion of zero
    // samples into the ring BEFORE the real first-block samples: 20 ms
    // until R-IOS-13 (2026-09-27), now the radio's target lead plus one
    // frame (3120 sample-pairs, 16.25 ms), so nothing past what the radio
    // takes stands in the ring as added latency.  Bench evidence 2026-05-26:
    // without this cushion, ~2% of a 10 s SSB TX gets zero-padded on the
    // wire -- listeners hear it as "digital jitter".
    //
    // With the send thread (R-IOS-13, R-R3-42) the cushion is what the
    // send thread draws on between a mic packet (which paces the pump)
    // and the block the pump makes from it: it fills the radio's buffer
    // to its target lead and leaves the rest in the ring.
    //
    // Thetis comparison: network.c:1259-1297 sendOutbound is producer-
    // paced, so the equivalent failure mode does not exist upstream.
    // This cushion is a NereusSDR-specific patch for our clock-paced
    // consumer divergence.
    std::atomic<bool> m_txIqPrimePending{false};

    // ── TX I/Q send thread (R-IOS-13, R-R3-42) ─────────────────────────────
    //
    // Thetis sends P2 transmit I/Q from a thread of its own, ob_main
    // (obbuffs.c:153-170 [v2.10.3.15]), at "Pro Audio" priority, woken for
    // each full 240-sample frame (OutBound, obbuffs.c:99-128
    // [v2.10.3.15]; spp 240 netInterface.c:1670 [v2.10.3.15]) and sending it
    // at once (sendOutbound, network.c:1250-1274 [v2.10.3.15]). Its read
    // thread, which paces the transmit DSP through the radio's mic packets,
    // runs at the same priority (network.c:1475-1479 [v2.10.3.15]).
    //
    // NereusSDR used a 5 ms QTimer on the connection thread that sent a
    // fixed 4 frames a tick. A late tick was never made up, so every
    // connection-thread stall moved the ring toward full for good. Now a
    // send thread of its own sends by the radio's buffer, estimated from
    // elapsed time the way deskhpsdr's TX IQ thread does
    // (new_protocol.c:2243-2272 [@f3d857c]): the estimate falls at
    // 192 kHz and rises 240 per frame sent, and a frame goes out whenever
    // one more fits under the target lead. After a stall the thread
    // refills the radio's buffer to the target in one burst (capped there,
    // never above what the radio holds) and leaves the rest queued in the
    // ring, so nothing is dropped and the radio is never overfilled. When
    // the ring holds less than a frame, silence goes out only while the
    // radio's buffer is below the low-water mark (so it never runs dry,
    // and the unkeyed stream the radio expects on port 1029 keeps going).
    //
    // The radio's buffer: the P2 gateware's Tx1_IQ_fifo holds 4096 samples
    // (21.3 ms at 192 kHz), n1gp-Anvelina_PROIII Tx1_IQ_fifo.vhd:106
    // [@8e86a61], and its writer ignores "full" (Orion.v:1673 [@8e86a61]),
    // so anything sent past 4096 ahead is lost in the radio. The 15 ms
    // target keeps 1216 samples of margin for timing error.
    struct TxIqPacer {
        static constexpr int kSamplesPerFrame = 240;
        static constexpr double kSamplesPerNs = 192000.0 / 1.0e9;
        static constexpr int kTargetLeadSamples = 2880;  // 15 ms at 192 kHz
        static constexpr int kLowWaterSamples = 960;     // 5 ms at 192 kHz
        static constexpr qint64 kLateWakeNs = 5'000'000; // later than 5 ms

        double estimate{0.0};   // samples in the radio's buffer, estimated
        qint64 lastNs{-1};

        // Moves the estimate to nowNs. Returns how many samples the radio
        // went without (0 when it did not run dry); *gapNs is the time
        // since the last call (0 on the first).
        double advance(qint64 nowNs, qint64* gapNs);
        bool roomForFrame() const noexcept
        {
            return estimate + kSamplesPerFrame <= kTargetLeadSamples;
        }
        bool belowLowWater() const noexcept { return estimate < kLowWaterSamples; }
        void frameSent() noexcept { estimate += kSamplesPerFrame; }
    };

    static constexpr int kTxIqFrameBytes = 4 + 240 * 6;  // 1444


    // Composes one frame from the ring (padding with silence when the ring
    // runs out) into buf, advancing m_seqTxIq. Returns the padded count.
    int composeTxIqFrame(char* buf);
    // One pass of the send thread at nowNs: sends every frame the radio's
    // buffer has room for. Returns the frames sent. Runs on the send
    // thread (or the caller's, in tests); never on two at once.
    int serviceTxIqSend(qint64 nowNs, TxIqFrameSink sink, void* ctx);
    void startTxIqSender();
    void stopTxIqSender();
    void txIqSenderMain();
    void resetTxSendStats();

    // Radio codec (2026-09-30): the receive audio stream to the radio's
    // speaker out. Thetis sends it from the same outbound path as the
    // transmit I/Q, one packet per 64 L/R samples (netInterface.c:1531
    // [v2.10.3.15]: prn->audio[i].spp = 64; // LR-samples per packet):
    // a 4-byte big-endian sequence number, then 64 16-bit big-endian L/R
    // pairs, to base_outbound_port + 4 (network.c:1276-1294, 1363-1376
    // [v2.10.3.15]). The send thread sends a packet whenever the radio's
    // audio buffer, estimated from elapsed time at 48 kHz, is below the
    // target lead and the ring has one due (takeRadioAudio).
    static constexpr int kRadioAudioSpp = 64;
    static constexpr int kRadioAudioPacketBytes = 4 + kRadioAudioSpp * 4;
    static constexpr int kRadioAudioTargetLeadFrames = 960;  // 20 ms at 48 kHz
    // Composes one packet of `lr` (kRadioAudioSpp pairs) into buf and
    // advances m_rxOutSeqNo.
    void composeRadioAudioPacket(const float* lr, char* buf);
    // One pass of the send thread at nowNs: sends every audio packet due.
    // Returns the packets sent. Send thread (or the caller's, in tests).
    int serviceRadioAudioSend(qint64 nowNs, TxIqFrameSink sink, void* ctx);
    // prn->rx[0].rx_out_seq_no; send thread. 0 from construction only, as
    // Thetis zeroes it once in create_rnet (netInterface.c:1492
    // [v2.10.3.15]), not at each start.
    quint32 m_rxOutSeqNo{0};
    double m_radioAudioLead{0.0};             // radio's buffer, estimated; send thread
    qint64 m_radioAudioLastNs{-1};            // send thread
    std::atomic<bool> m_radioAudioSwap{false};  // prn->lr_audio_swap, set at sender start
    quint16 m_radioAudioDestPort{0};
    std::atomic<quint64> m_radioAudioPacketsSent{0};
    std::atomic<quint64> m_radioAudioSendErrors{0};

    std::unique_ptr<QThread> m_txIqSender;
    std::atomic<bool> m_txIqSenderRun{false};
    qintptr m_txIqSocketFd{-1};
    quint32 m_txIqDestIpv4{0};   // host order
    quint16 m_txIqDestPort{0};
    TxIqPacer m_txIqPacer;       // send thread only
    char m_txIqPendingFrame[kTxIqFrameBytes]{};
    bool m_txIqPending{false};   // send thread only: a refused frame to retry

    std::atomic<quint64> m_txIqFramesSent{0};
    std::atomic<quint64> m_txIqZeroPadded{0};
    std::atomic<quint64> m_txIqLateWakes{0};
    std::atomic<quint64> m_txIqCatchUpBursts{0};
    std::atomic<quint64> m_txIqRadioRanDry{0};
    std::atomic<quint64> m_txIqOverflowSamples{0};
    std::atomic<quint64> m_txIqSendErrors{0};
    std::atomic<int> m_txIqMaxRingPairs{0};
    // TX diagnostics lane (2026-10-01): where the key's dropouts fell.
    // resetTxSendStats bumps the generation and clears the published
    // figures; the send thread clears its own state when it sees the new
    // generation, then publishes (one writer each, relaxed: log only).
    void noteTxIqPadding(qint64 nowNs, int underrunSamples);
    std::atomic<quint32> m_txIqDiagGeneration{0};
    std::atomic<qint64> m_txIqKeyNs{-1};
    std::atomic<qint64> m_txIqFirstBlockAtNs{-1};
    std::atomic<quint64> m_txIqPadStart{0};
    std::atomic<quint64> m_txIqPadMid{0};
    std::atomic<quint64> m_txIqPadOpen{0};
    std::atomic<qint64> m_txIqPadOpenAtNs{-1};
    std::atomic<quint64> m_txIqLongestMidPad{0};
    std::atomic<qint64> m_txIqLongestMidPadAtNs{-1};
    std::atomic<qint64> m_txIqFirstDryAtNs{-1};
    std::atomic<qint64> m_txIqFirstDryGapNs{-1};
    std::array<std::atomic<qint64>, TxSendStats::kMaxBurstEvents> m_txIqBurstAtNs{};
    std::array<std::atomic<qint64>, TxSendStats::kMaxBurstEvents> m_txIqBurstGapNs{};
    std::array<std::atomic<int>, TxSendStats::kMaxBurstEvents> m_txIqBurstFrames{};
    std::atomic<int> m_txIqBurstEvents{0};
    // Send thread only.
    quint32 m_txIqDiagSeen{0};
    qint64 m_txIqDiagKeyNs{-1};
    bool m_txIqDiagSawBlock{false};
    quint64 m_txIqDiagRun{0};
    qint64 m_txIqDiagRunStartNs{-1};
    // The TX pump's wake watch, read by txSendStats on the owner's thread
    // (m_txMicSource itself is the connection thread's).
    std::atomic<const class TxMicSource*> m_txMicSourceForStats{nullptr};
    // Producer only: a full ring's warning, at most once a second.
    qint64 m_txIqOverflowLogNs{0};
    quint64 m_txIqOverflowLogged{0};

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
    // Initial value 0x22: reflects the default-set bits:
    //   bit 2 (0x04) CLEAR = PTT enabled at firmware (matches m_micPTTDisabled=false
    //     default in RadioConnection.h — direct polarity: false = 0 on wire).
    //     From Thetis console.cs:19757 [v2.10.3.13+501e3f51]:
    // Upstream tags preserved: //MW0LGE (from cited console.cs:19758) [v2.10.3.15]
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
    // Bit 1 (0x02) SET = mic boost on, matching the m_micBoost=true default
    //   in RadioConnection.h. From Thetis console.cs:13259 [v2.10.3.15] —
    //   private bool mic_boost = true;
    struct MicState {
        unsigned char micControl{0x22};  // boost on (bit 1) + PTT enabled (bit 2 clear) + XLR selected (bit 5)
        int lineInGain{0};
    };
    MicState m_mic;

    // --- Wideband settings (from Thetis create_rnet:1461-1466) ---
    int m_wbSamplesPerPacket{512};
    int m_wbSampleSize{16};
    int m_wbUpdateRate{70};
    int m_wbPacketsPerFrame{32};

    // Phase 3F Sub-Epic F Task 1: per-ADC wideband stream enable mask.
    // Bit N corresponds to ADCN. Default 0 (all disabled) preserves
    // pre-Phase-3F wire behaviour (composeCmdGeneral previously hardcoded
    // buf[23] = 0). Driven by setWidebandEnabled(); read by
    // composeCmdGeneralLegacy and (via CodecContext::p2WbEnableMask)
    // by P2CodecOrionMkII::composeCmdGeneral. See Thetis network.c:879
    // [v2.10.3.15].
    quint8 m_wbEnableMask{0};

    // Nereus-original per-ADC capture identity. The pointees deliberately
    // outlive this QObject when a worker retains a shared pointer. Connection
    // and capture transitions advance them; zero is reserved as invalid.
    std::array<std::shared_ptr<std::atomic<quint64>>, 8> m_wbCaptureEpochs{};

    // Phase 3F Sub-Epic F Task 3: per-ADC wideband frame accumulators
    // (up to 8 ADCs). Constructed in the P2RadioConnection ctor and
    // parented to this. Each entry owns the 32-packet state machine
    // that converts raw UDP wideband packets (1028 bytes, port indices
    // 2..9 → ADC 0..7 per Thetis network.c:550-602 [v2.10.3.15]) into
    // 16384-sample float frames. The matching frameReady signal is
    // re-emitted as P2RadioConnection::widebandFrameReady(adcIndex, ...)
    // by a lambda installed in the ctor. Only ADCs whose bit is set
    // in m_wbEnableMask will actually receive packets from the radio.
    std::array<WidebandFrameAccumulator*, 8> m_wbAccumulators{};

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

        // Two independent LPF masks, one per Alex word. Thetis keeps the
        // same split (AlexLPFMask for Alex0, Alex1LPFMask for Alex1) and
        // routes each write by whether it came from a transmit or a receive
        // frequency:
        //   From Thetis ChannelMaster/netInterface.c:682-726 [v2.10.3.15]
        //     void SetAlexLPFBits(int bits, bool isTX, bool isMox)
        //     if (isMox || isTX)   -> Alex1LPFMask (prbpfilter2)
        //     if (isMox || !isTX)  -> AlexLPFMask  (prbpfilter)
        //
        // Collapsing these into one mask is an RF-safety bug: a receive
        // retune onto a low band drags the transmit low-pass down with it,
        // so keying up on a high band drives full power into a low-pass
        // well below the carrier. The two masks must stay separate.
        //
        // Defaults are 0x10 (6 m, the widest low-pass) rather than 0 —
        // the LPF has no bypass encoding, and Thetis's fall-through arm
        // picks 6 m too (console.cs:7237-7241 [v2.10.3.15]).
        int lpfBitsRx{0x10};  // Alex0 LPF: the receive selection (unkeyed)
        int lpfBitsTx{0x10};  // Alex1 LPF: the transmit selection

        // Phase 3F: per-ADC RX band-pass decision from AlexController, which
        // reviews every slice band on a chain instead of taking whichever
        // receiver was retuned last. -1 = no slice on that ADC, fall back to
        // the frequency-derived hpfBits above. See AlexRxBpf in
        // RadioConnection.h for the full rationale and Thetis cites.
        int rxHpfBitsAdc0{-1};
        int rxHpfBitsAdc1{-1};

        // RX-only antenna mux — from Thetis ChannelMaster/network.h:279-281
        // [v2.10.3.13 @501e3f5]. Alex0 bits 8-10:
        //   0 = no RX-only path, 1 = _Rx_1_In (bit 10), 2 = _Rx_2_In (bit 9),
        //   3 = _XVTR_Rx_In (bit 8).
        // Encoding per netInterface.c:479-481 SetAntBits().
        int  rxOnlyAnt{0};

        // _Rx_1_Out relay (K36 RL17 RX-Bypass-Out) — from Thetis
        // ChannelMaster/network.h:282 [v2.10.3.13 @501e3f5]. Alex0 bit 11.
        bool rxOut{false};

        // Alex attenuator _20_dB_Atten / _10_dB_Atten (Thetis SetAlexAtten),
        // Alex0 bits 13 / 14, network.h:284-285 [v2.10.3.15].
        bool atten20dB{false};
        bool atten10dB{false};
    };
    AlexState m_alex;

    // Build Alex0 and Alex1 32-bit register values from current state.
    quint32 buildAlex0() const;
    quint32 buildAlex1() const;

    // ADC0's effective RX HPF bits — AlexController's per-ADC decision when
    // one exists, else the frequency-derived m_alex.hpfBits. Phase 3F.
    quint8 effectiveRxHpfBitsAdc0() const;

    // Alex0's effective LPF bits. Receiving, Alex0 carries the receive
    // selection; transmitting, it carries the transmit selection, because
    // on pre-4.3 hardware Alex0's low-pass is the one in the TX path.
    // This is the compose-time form of Thetis's two write guards:
    //   From Thetis ChannelMaster/netInterface.c:682-726 [v2.10.3.15]
    //     if (isMox || !isTX) -> AlexLPFMask = bits
    // Thetis reaches the same state by re-driving on the MOX edges
    // (console.cs:29083-29099 + 29140-29148 HdwMOXChanged [v2.10.3.15]
    // both call UpdateTXDDSFreq, and UpdateAlexTXFilter is wrapped in
    // `if (!_mox)` at console.cs:15487-15498), so the wire bytes match.
    quint8 effectiveLpfBitsAlex0() const;

    // The live DDCs and the one standing in for Thetis's RX1 (plan Task 14,
    // Phase 3F section 16.3.2): the lowest live DDC, as on Protocol 1
    // (P1RadioConnection::setLiveReceiverSlots). -1 until a mask arrives;
    // until then RX1 is the DDC retuned last, the behaviour before the
    // stand-in existed.
    quint32 m_liveSlotMask{0};
    int     m_rx1Slot{-1};
    int     m_lastRetunedDdc{0};
    int     rx1Ddc() const { return m_rx1Slot >= 0 ? m_rx1Slot : m_lastRetunedDdc; }

    // The DDCs of the slices counted on ADC0's input (AlexRxBpf::
    // countedSlotsAdc0); the receive low-pass follows the highest. 0 = none
    // counted, and the RX1 stand-in rule applies.
    quint32 m_countedSlotsAdc0{0};

    // Recompute m_alex.hpfBits and m_alex.lpfBitsRx from the RX1 stand-in
    // (and, for the low-pass, the receiver beside it).
    void recomputeReceiveFilters();

    // setAlexLPF (codec::alex::setAlexLpf) on m_alex.lpfBitsRx / Tx, the
    // Alex0 and Alex1 masks, then reports the low-pass in use.
    void applyAlexLpf(double freqMhz, bool freqIsTx);
    // UpdateAlexTXFilter: the receive-frequency selection, unkeyed only.
    void applyReceiveAlexLpf();

    // Each DDC's slice VFO frequency (setReceiverVfoFrequencies); 0 = not
    // told, and the band falls back to the DDC's centre.
    std::array<quint64, kMaxRxStreams> m_rxVfoHz{};

    // The frequency whose band selects the OC outputs (byte 1401): the
    // transmitting slice's while keyed, the RX1 stand-in's VFO while not.
    quint64 ocBandFrequencyHz() const;
    // The OC byte byte 1401 carries (buildCodecContext's ctx.ocByte).
    quint8 composedOcByte() const;
    // Plan Task 14 fix wave (M2, M3): a high-priority packet when that byte
    // differs from the one last sent, as Thetis's SetOCBits.
    void pushBandOutputsIfChanged();

    // --- DDC→ADC mapping register (from Thetis network.c rx_adc_ctrl1) ---
    quint32 m_rxAdcCtrl1{0};

    // --- I/Q buffers and packet counters ---
    std::array<QVector<float>, kMaxDdc> m_iqBuffers;
    int m_totalIqPackets{0};
    // Connection thread only: inside onReadyRead's drain, processIqPacket
    // marks frameReceived pending and the drain emits it once at its end.
    bool m_inIqDrain{false};
    bool m_frameReceivedPending{false};

#ifdef NEREUS_BUILD_TESTS
public:
    // Stand in for the route lookup in bindToRadioFacingAddress(): a null
    // address is "no route", an address this host does not own cannot be
    // bound. Both must leave the socket bound to every address.
    void setRadioFacingAddressForTest(const QHostAddress& local) {
        m_radioFacingOverridden = true;
        m_radioFacingOverride = local;
    }
    QAbstractSocket::SocketState socketStateForTest() const {
        return m_socket ? m_socket->state() : QAbstractSocket::UnconnectedState;
    }
    QHostAddress socketAddressForTest() const {
        return m_socket ? m_socket->localAddress() : QHostAddress();
    }
    int socketReceiveBufferForTest() const {
        return m_socket ? m_socket->socketOption(
                   QAbstractSocket::ReceiveBufferSizeSocketOption).toInt() : -1;
    }
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
    // Feeds n interleaved float I/Q samples through sendTxIq() (producer
    // side), then composes ONE 1444-byte TX I/Q frame from the ring with the
    // production composer (composeTxIqFrame) without sending a UDP
    // datagram, and returns the frame as a QByteArray.
    //
    // Cite: deskhpsdr/src/new_protocol.c:1945-1956 [@120188f]
    //   (production send path structure: 4-byte seq + 1440-byte payload)
    QByteArray txIqFrameForTest(const float* iq, int n) {
        if (n > 0 && iq != nullptr) {
            sendTxIq(iq, n);
        }
        QByteArray frame(kTxIqFrameBytes, '\0');
        composeTxIqFrame(frame.data());
        return frame;
    }

    // R-IOS-13, R-R3-42: one pass of the send thread at nowNs, with the
    // frames captured instead of sent. The send thread must not be running.
    // refuse > 0 makes the sink refuse that many frames first (a full
    // socket buffer).
    struct TxIqCapture {
        QList<QByteArray> frames;
        int refuse{0};
    };
    int serviceTxIqSendForTest(qint64 nowNs, TxIqCapture* capture) {
        Q_ASSERT(!m_txIqSender);
        return serviceTxIqSend(nowNs, [](void* ctx, const char* frame, int len) {
            auto* c = static_cast<TxIqCapture*>(ctx);
            if (c->refuse > 0) {
                --c->refuse;
                return TxIqSinkResult::Retry;
            }
            c->frames.append(QByteArray(frame, len));
            return TxIqSinkResult::Sent;
        }, capture);
    }
    double txIqRadioLeadForTest() const { return m_txIqPacer.estimate; }
    // Radio codec: one pass of the audio send at nowNs, packets captured.
    // The send thread must not be running.
    int serviceRadioAudioSendForTest(qint64 nowNs, TxIqCapture* capture) {
        Q_ASSERT(!m_txIqSender);
        return serviceRadioAudioSend(nowNs, [](void* ctx, const char* frame, int len) {
            auto* c = static_cast<TxIqCapture*>(ctx);
            if (c->refuse > 0) {
                --c->refuse;
                return TxIqSinkResult::Retry;
            }
            c->frames.append(QByteArray(frame, len));
            return TxIqSinkResult::Sent;
        }, capture);
    }
    void setLrAudioSwapForTest(bool swap) { m_radioAudioSwap.store(swap); }
    // Runs the real send thread against `radio` (a loopback receiver in
    // tests) on the socket init() bound, without SendStart.
    void startTxIqSenderForTest(const QHostAddress& radio) {
        m_radioInfo.address = radio;
        startTxIqSender();
    }
    void stopTxIqSenderForTest() { stopTxIqSender(); }

    // Return number of floats currently buffered in the TX I/Q ring.
    int txIqRingCountForTest() const { return m_txIqRingCount.load(std::memory_order_acquire); }

    // Keep loopback fixtures off the protocol's fixed 1024-1041 ports while
    // preserving the real role arithmetic in compose/dispatch paths.
    void setPortBasesForTest(quint16 outboundBase, quint16 inputRoleBase) {
        Q_ASSERT(!m_running);
        m_baseOutboundPort = static_cast<int>(outboundBase);
        m_p2CustomPortBase = static_cast<int>(inputRoleBase);
    }

    // Compress only timer budgets; packet parsing and socket paths remain the
    // production implementations. Values are clamped away from zero so a
    // queued start cannot become an accidental immediate callback.
    void setSilenceTimeoutsForTest(int connectTimeoutMs,
                                   int establishedTimeoutMs) {
        Q_ASSERT(!m_running);
        m_connectTimeoutMs = std::max(1, connectTimeoutMs);
        m_establishedSilenceTimeoutMs = std::max(1, establishedTimeoutMs);
    }

    // Deliver the established-silence wakeup now, as if the Qt timer had
    // been dispatched ahead of any readyRead work already queued behind it.
    void runEstablishedSilenceWakeupForTest() { onEstablishedSilenceTimeout(); }

#endif
};

} // namespace NereusSDR
