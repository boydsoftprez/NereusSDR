// =================================================================
// src/core/P1RadioConnection.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/ChannelMaster/networkproto1.c, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/NetworkIO.cs (upstream has no top-of-file header — project-level LICENSE applies)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23 - R-R3-21: EP6 sequence error count ported from Thetis
//                 networkproto1.c MetisReadDirect [v2.10.3.15] for
//                 Diagnostics > Connection Quality. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the Network Watchdog wait is Thetis's 3000 ms
//                 (networkproto1.c:292-294 [v2.10.3.15]) and the RUNSTOP test
//                 seams no longer carry the setting. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-32 (remote-window parity Task 6): m_ep6SeqPrimed.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-49 / R-R3-46: Setup > Transmit > Power's Disable HF PA
//                applied (Thetis DisablePA and hf_tr_relay,
//                transmitSettingsVersion 11). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 Band Volts and Disable PS Sync: on a Hermes Lite 2,
//                bank 0 C3 bits 3 and 4 follow the saved HL2 options (off by
//                default), as mi0bot setup.cs:2843-2848 and 13376-13390
//                [@c26a8a4] do; other boards keep Thetis's dither and random
//                on. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 1: the HL2 TX buffer latency and PTT hang
//                (bank 17) are the saved HL2 options, as mi0bot
//                setup.cs:21236-21248 [@c26a8a4] sends them. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 1: bank 18 carries the saved Reset on
//                Ethernet disconnect, as mi0bot setup.cs:21257-21262
//                [@c26a8a4] sends it. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - HL2 clock: Enable CL2, CL2 frequency and External 10 MHz
//                program the HL2 clock chip over I2C at connect and on a
//                change (setHl2Clock; mi0bot setup.cs:21558-21756,
//                console.cs:28033-28040 [@c26a8a4]). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 1: the HL2 I/O board's ongoing poll (mode,
//                TX frequency, aerial registers, input-pin reads) and mi0bot's
//                I2C frame spacing with the round-robin kept on an I2C
//                subframe (console.cs:25781-25945, networkproto1.c:898-906
//                [@c26a8a4]). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-28 - R-R3-49 (found bug): the VFO frequencies sent carry the
//                 calibration correction factor, as Thetis NetworkIO.VFOfreq
//                 does on Protocol 1 [v2.10.3.15] (setCalibrationController,
//                 wireFrequencyHz). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 - Level Cal: the Alex receive attenuator (Thetis SetAlexAtten,
//                netInterface.c:421-432 [v2.10.3.15]) on the wire, and the step
//                attenuator range above 31 dB on Alex boards (value + 2,
//                console.cs:11044-11056 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal fix wave: setRx2Preamp, the second receiver's
//                preamp bit (Thetis SetRX2Preamp, netInterface.c:758-767
//                [v2.10.3.15]). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - Radio codec: the EP2 L/R bytes carry the receive audio
//                for the radio's own speaker out, swapped as Thetis
//                sendProtocol1Samples does (networkproto1.c:726-731
//                [v2.10.3.15]); on the HL2 only when Swap audio channels is
//                on, as mi0bot (networkproto1.c:1231-1239 [@c26a8a4]).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Shared-input filters (ruling (c)): m_countedSlotsAdc0,
//                the slots the receive low-pass and the HL2 OC receive band
//                follow. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-30 - Shared-input filters, follow-up: countedCandidates.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - bindToRadioFacingAddress, radioFacingAddress and the
//                receive-socket test seams: the socket binds the address that
//                reaches the radio (network.c:116-118, 203 [v2.10.3.15]).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - TX safety: a lost link and every reconnect start unkeyed
//                (dropTransmitForNewLink) and a key after the loss is refused
//                until released (m_linkLossLatched), as the priming frames of
//                Thetis networkproto1.c:106-138 [v2.10.3.15] never carry MOX.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - TX safety fix round 1: the latch only for a link lost
//                keyed, lifted when the reconnect is back unkeyed.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01 - Diversity lane B4: setDiversity, the writer m_diversity
//                never had, for the VFO lock bit (bank 0 C4 bit 7,
//                networkproto1.c:471 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
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

#pragma once

#include "RadioConnection.h"
#include "BoardCapabilities.h"
#include "HpsdrModel.h"
#include "SharedInputLowPass.h"
#include "codec/IP1Codec.h"
#include "codec/CodecContext.h"

namespace NereusSDR { class OcMatrix; }                  // forward decl — full header in .cpp
namespace NereusSDR { class CalibrationController; }     // R-R3-49: frequency correction
namespace NereusSDR { class IoBoardHl2; }                // forward decl — full header in .cpp
namespace NereusSDR { class HermesLiteBandwidthMonitor; }// forward decl — full header in .cpp
namespace NereusSDR { class TxMicSource; }               // forward decl — full header in audio/TxMicSource.h

#include <atomic>
#include <memory>
#include <array>
#include <QUdpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QDateTime>
#include <utility>
#include <vector>

namespace NereusSDR {

class P1RadioConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit P1RadioConnection(QObject* parent = nullptr);
    ~P1RadioConnection() override;

    // Radio codec (2026-09-30): the EP2 L/R bytes carry the receive audio.
    bool carriesRadioAudio() const noexcept override { return true; }

    int getAdcForDdc(int ddc) const override;

    // Protocol identifier — 1 for OpenHPSDR P1.  See RadioConnection::protocolVersion.
    int protocolVersion() const override { return 1; }

    // Wire-format compose helpers — static, testable in isolation.
    // Each implementation cites its Thetis source line.
    static void composeEp2Frame(quint8 out[1032], quint32 seq, int ccAddress,
                                int sampleRate, bool mox,
                                quint64 rx1FreqHz = 0,
                                int activeRxCount = 1) noexcept;
    static void composeCcBank0(quint8 out[5], int sampleRate, bool mox,
                               int activeRxCount = 1) noexcept;
    static void composeCcBankRxFreq(quint8 out[5], int rxIndex, quint64 freqHz) noexcept;
    static void composeCcBankTxFreq(quint8 out[5], quint64 freqHz) noexcept;
    static void composeCcBankAtten(quint8 out[5], int dB) noexcept;
    static void composeCcBankAlexRx(quint8 out[5], quint32 alexRxMask) noexcept;
    static void composeCcBankAlexTx(quint8 out[5], quint32 alexTxMask) noexcept;
    static void composeCcBankOcOutputs(quint8 out[5], quint8 ocMask) noexcept;

    // Source: networkproto1.c:367-374 — sign-extend 24-bit big-endian sample
    // and scale to float [-1, 1] using 2^23 full scale.
    static float scaleSample24(const quint8 be24[3]) noexcept;

    // Source: networkproto1.c:361-376 MetisReadThreadMainLoop — parse a
    // 1032-byte metis ep6 datagram into per-receiver interleaved I/Q float pairs.
    // perRx[i] is interleaved (I0, Q0, I1, Q1, ...) for receiver i.
    // Returns false on malformed input (wrong magic, missing sync, bad numRx).
    static bool parseEp6Frame(const quint8 frame[1032],
                              int numRx,
                              std::vector<std::vector<float>>& perRx) noexcept;

    // Phase 3M-1c TX pump v3 overload — also extracts mic16 byte zone
    // samples per Thetis networkproto1.c:393-411 [v2.10.3.13].  micOut
    // receives the float-scaled mono mic samples for both subframes (in
    // network order — subframe 0 first, then subframe 1).  Sample count
    // == 2 * samplesPerSubframe at the operating sample rate.
    //
    // NOTE: parseEp6Frame extracts mic samples at the radio's full sample
    // rate (e.g. 192 kHz at sampleRate=192000).  The caller is responsible
    // for decimating to 48 kHz before feeding TxMicSource — see
    // decimateMicSamples below.
    //
    // Backed by the same parser body as the 3-arg overload; the mic
    // extraction is gated on a non-null micOut.
    static bool parseEp6Frame(const quint8 frame[1032],
                              int numRx,
                              std::vector<std::vector<float>>& perRx,
                              std::vector<float>* micOut) noexcept;

    // Decimate radio-rate mic samples to 48 kHz before feeding TxMicSource.
    // The caller passes a persistent counter (state across calls); this
    // mirrors Thetis's global mic_decimation_count which is reset once at
    // MetisReadThreadMainLoop init and incremented through every Inbound()
    // call thereafter.
    //
    // Behaviour: counter++; if (counter == factor) { counter = 0; keep; }
    // For factor==1, every sample is kept (counter is unused).
    //
    // Source: Thetis ChannelMaster/networkproto1.c:391-410 [v2.10.3.14]
    //         Thetis ChannelMaster/netInterface.c:1287-1310 [v2.10.3.14]
    static void decimateMicSamples(const float* in, int n, int factor,
                                    int& counter,
                                    std::vector<float>& out) noexcept;

    // Sample-rate → mic-decimation factor lookup.  Mirrors the Thetis
    // switch in netInterface.c:1287-1310 [v2.10.3.14] (1/2/4/8 for
    // 48/96/192/384 kHz; default factor=4 for any other rate).  Pure
    // function; safe to call from any thread.
    static int micDecimationFactorFor(int sampleRate) noexcept;

    // Read the current mic decimation factor (set by setSampleRate).
    // Test seam — production code reads m_micDecimationFactor directly.
    int micDecimationFactor() const noexcept { return m_micDecimationFactor; }

    // Wire RadioModel's OcMatrix so buildCodecContext() can source the OC
    // byte from maskFor(currentBand, mox) at C&C compose time.
    // Phase 3P-D Task 3 — called by RadioModel::connectToRadio().
    void setOcMatrix(const OcMatrix* matrix);

    // R-R3-49: Setup > Calibration's frequency correction factor, applied to
    // every VFO frequency sent (Thetis NetworkIO.VFOfreq). Null: factor 1.0.
    void setCalibrationController(const CalibrationController* cal);

    // Phase 3P-E Task 2: wire IoBoardHl2 for I2C intercept (HL2 only).
    // On HL2, pushes the pointer into P1CodecHl2 and stores it locally for
    // the ep6 response parser.  On non-HL2 boards this is a noop.
    // Called by RadioModel::connectToRadio() after selectCodec().
    void setIoBoard(IoBoardHl2* io);

    // Phase 3P-E Task 3: wire HermesLiteBandwidthMonitor (HL2 only).
    // Called by RadioModel::connectToRadio() when hasBandwidthMonitor is true.
    // The monitor is owned by RadioModel; P1RadioConnection holds a non-owning
    // pointer and records ep6/ep2 bytes + calls tick() from the watchdog.
    // Null-safe — all call sites guard with if (m_bwMonitor).
    void setBandwidthMonitor(HermesLiteBandwidthMonitor* monitor);

    // Phase 3M-1c TX pump v3: wire the radio-mic cadence source.
    // The mic16 byte zone in EP6 frames (Thetis networkproto1.c:393-411
    // [v2.10.3.13]) is decoded and pushed into TxMicSource::inbound() on
    // every parsed EP6 frame.  Owned by RadioModel; this class keeps a
    // non-owning pointer.  Null-safe.
    //
    // HL2 quirk: the HL2 has no mic jack, so the mic16 byte zone is all
    // zeros — but the byte zone IS still present in EP6 frames at the
    // documented offsets, so cadence still flows.  RadioModel forces the
    // PC mic override on HL2 via setMicSourceLocked(true) so the mic16
    // zeros never reach fexchange0.
    // [Radio codec lane, 2026-09-30] Correction to the note above: a stock
    // HL2 has no mic codec and sends zero mic bytes, but an HL2 built with
    // the AK4951 audio add-on board (gateware variants hl2b5up_ak4951v3/v4)
    // does fill the zone; mi0bot has no HL2 mic lock. The gateware's
    // discovery reply carries no AK4951 field (Hermes-Lite2
    // usopenhpsdr1.v:254-314 @7472bd1), so NereusSDR cannot tell the two
    // apart, so the HL2 keeps Radio Mic open with a note that it needs the
    // add-on board (BoardCapabilities::radioMicNeedsAddOn) and no PC mic
    // lock.
    void setTxMicSource(TxMicSource* src);

public slots:
    void init() override;
    void connectToRadio(const NereusSDR::RadioInfo& info) override;
    void disconnect() override;

    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) override;
    void setTxFrequency(quint64 frequencyHz) override;

    // How many receivers the PANADAPTERS want. One of the two inputs to the
    // announced count; see announceRxCount() for why there are two and how
    // they combine. Restarts the ep6 stream when the announced count moves.
    void setActiveReceiverCount(int count) override;

    // Which frame slots carry a live receiver (bit n = slot n). Phase 3F
    // section 16.3.2. Two uses.
    //
    // The announced count covers the highest live slot, so every slot in use
    // is inside the frame (see announceRxCount).
    //
    // The receive filters: Thetis derives the Alex high-pass, the receive
    // half of the low-pass and the OC outputs from RX1, and its RX1 always
    // exists. Routing by frame slot means slice A can be closed while slice
    // B keeps slot 1 (slot 2 on the Orion class). The live receiver in the
    // LOWEST slot then stands in for RX1, and keeps that role until a
    // receiver comes back on a lower slot.
    //
    // An empty mask (every stream suspended, as the HermesII class does
    // while PureSignal transmits) leaves the stand-in where it was, so the
    // filters do not move for a state that lasts one transmission.
    void setLiveReceiverSlots(quint32 slotMask) override;
    void setReceiverVfoFrequencies(const QVector<quint64>& vfoHzBySlot) override;

    void setSampleRate(int sampleRate) override;

    // Task 1.6: live-apply a sample-rate change to a running P1 connection.
    //
    // Updates m_sampleRate then issues sendMetisStop() + priming burst +
    // sendMetisStart() so the radio re-arms its EP6 sender with the new
    // sample rate encoded in bank-0 C0 bits 24-25.
    //
    // Must be called on the connection thread (invoke via QMetaObject::
    // invokeMethod(Qt::QueuedConnection) from the main thread).
    //
    // No-op when m_running is false (not yet connected) or when the
    // given rate equals the current m_sampleRate (idempotent).
    //
    // Cite: networkproto1.c onReconnectTimeout restart pattern
    // [v2.10.3.13] — sendMetisStop + sendPrimingBurst(3) + sendMetisStart.
    void restartStreamWithRate(int newSampleRate);

    void setAttenuator(int dB) override;
    void setAttenuatorForAdc(int adc, int dB) override;
    void setPreamp(bool enabled) override;
    // Level Cal: prn->rx[1].preamp (C1 bit 1 of the bank carrying the
    // preamp bits), Thetis SetRX2Preamp.
    void setRx2Preamp(bool enabled) override;
    void setTxDrive(int level) override;
    void setMox(bool enabled) override;
    void setAntennaRouting(AntennaRouting routing) override;
    void setAlexRxBpf(AlexRxBpf bpf) override;
    // Level Cal: the Alex receive attenuator (Thetis SetAlexAtten).
    void setAlexAtten(int bits) override;
    void setWatchdogEnabled(bool enabled) override;
    void sendTxIq(const float* iq, int n) override;
    // G-07: Protocol 1 keeps only the full-ring loss count
    // (TxSendStats::overflowOnly); the rest of the counters are Protocol 2's.
    TxSendStats txSendStats() const override;
    // G-05: the unkey's wait for the send ring (RadioConnection).
    bool txIqRingDrained() const override;
    double txIqRingLengthMs() const override;
    void setTrxRelay(bool enabled) override;
    // "Disable HF PA": bank 10 C3 bit 7 (and the HL2's C2 bit 3), on the
    // next frame (RadioConnection::setPaDisabled).
    void setPaDisabled(bool disabled) override;
    // The Alex tab's receive filter rows: the high-pass is re-selected at
    // once (RadioConnection::setAlexHpfEdges).
    void setAlexHpfEdges(const codec::alex::AlexHpfEdges& edges) override;
    // The Alex-1 low-pass rows: stored for the next selection
    // (RadioConnection::setAlexLpfEdges).
    void setAlexLpfEdges(const codec::alex::AlexLpfEdges& edges) override;
    // 6m/ByPass on RX: re-selects the low-pass at once
    // (RadioConnection::setAlexLpfBypass).
    void setAlexLpfBypass(bool on) override;
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

    // HL2 only: the Band Volts and Disable PS Sync options, which a Hermes
    // Lite 2 takes in bank 0 C3 bits 3 and 4 (the ADC dither and random bits
    // on every other board). Stored for the next connect; on an HL2 already
    // connected they reach the wire on the next frame. Ignored on any other
    // board. From mi0bot Console/setup.cs:13376-13390 [@c26a8a4].
    void setHl2BandVolts(bool on);
    void setHl2PsSync(bool on);

    // HL2 only: the TX buffer latency (ms, 0..70) and PTT hang (ms, 0..30)
    // bank 17 carries (C4 and C3). Stored; the next round of the banks sends
    // them, as mi0bot's SetTxLatency / SetPttHang only store the value.
    // From mi0bot Console/setup.cs:21236-21248 [@c26a8a4]:
    //   // MI0BOT: Controls the hardware tx buffer in the HL2
    //   // MI0BOT: Controls the hardware PTT hang in the HL2
    void setHl2TxLatency(int ms);
    void setHl2PttHang(int ms);

    // HL2 only: Reset on Ethernet disconnect, bank 18 C4. Stored; the next
    // round of the banks sends it, as mi0bot's SetResetOnDisconnect only
    // stores the bit.
    // From mi0bot Console/setup.cs:21257-21262 [@c26a8a4]:
    //   // MI0BOT: Controls if the HL2 will reset after an Ethernet disconnect
    void setHl2ResetOnDisconnect(bool on);

    // HL2 only: Swap audio channels. mi0bot swaps the L/R audio bytes only
    // while this is on (default off), where Thetis always swaps them.
    // From mi0bot ChannelMaster/networkproto1.c:1231 [@c26a8a4]:
    //   if (prn->swap_audio_channels)				// To cater for different firmware at the hardware, allow control of audio channels swapping
    void setHl2SwapAudioChannels(bool on);

    // HL2 only: the clock options External 10 MHz (CL1 input), Enable CL2
    // and the CL2 frequency (kHz, clamped to 1000..200000: mi0bot's box
    // holds 1 to 200 MHz to three decimal places). Each is written to the
    // HL2's clock chip (I2C bus 0, address 0xd4) as a list of register
    // writes. At connect (when the first ep6 frame arrives) an option that
    // is on is sent, and so is one whose list did not finish last time; an
    // option left off sends nothing otherwise, as mi0bot's connect path
    // does. On an HL2 already connected, a change is sent at once: External
    // 10 MHz sends its on or off table and then the CL2 table the new
    // reference needs, and a CL2 change sends the CL2 table (on) or the CL2
    // off table (off). Lists that have not started when a change arrives
    // are dropped and rebuilt from the values now wanted. While not
    // connected the values are only stored.
    // From mi0bot Console/setup.cs:21558-21756 and console.cs:28033-28040
    // [@c26a8a4]:
    //   // MI0BOT: Support for HL2 10MHz input
    //   // MI0BOT: Support for HL2 Cl2 clock output
    void setHl2Clock(bool ext10MHz, bool cl2Enable, int cl2FreqKHz);

    // HL2 I/O board poll inputs. The TX VFO's DSP mode (Thetis DSPMode
    // value) and frequency in Hz, written to REG_OP_MODE and
    // REG_TX_FREQ_BYTE4..0; and the aerial mode and ports written to
    // REG_RF_INPUTS and REG_ANTENNA (console.cs SetIOBoardAerialPorts).
    // From mi0bot Console/console.cs:25781-25945 UpdateIOBoard [@c26a8a4].
    void setIoBoardTxState(int dspMode, qint64 frequencyHz);
    void setIoBoardAerials(quint8 aerialMode, quint8 aerialPorts);

    // Set the P1-only per-DDC ADC routing word (Thetis `P1_adc_cntrl`).
    //
    // 14 bits wide, 2 bits per DDC, layout 66554433221100 per
    // console.cs:15120 [v2.10.3.13]:
    //   bits  1: 0 = DDC0's ADC select  (0=ADC0, 1=ADC1, 2=ADC2)
    //   bits  3: 2 = DDC1's ADC select
    //   ... DDC6 = bits 13:12
    //
    // Bits above 13 are masked off. Wire bytes update on the next
    // bank-4 emit (bank 4 is a steady-state round-robin slot, so the
    // change reaches the radio within ~16 frames at default cadence).
    //
    // Mirrors Thetis SetADC_cntrl_P1 (netInterface.c:992-996 [v2.10.3.13])
    // semantics: the field becomes the C1/C2 bytes of bank 4 directly.
    //
    // Called from:
    //   - RadioModel after AppSettings restore on connect (per-MAC).
    //   - The future Setup → Hardware → P1 ADC Routing page when the
    //     user toggles a radDDC*ADC* radio button. Without that page
    //     end users can edit `hardware/<mac>/p1AdcCntrl` in the XML
    //     settings file directly to override the board default.
    void setP1AdcCntrl(int bits);

    // Phase 3M-4 Task 17 P1 follow-up — apply per-board PS DDC config.
    //
    // P1 mirror of P2RadioConnection::applyPsDdcConfig.  Receives the
    // wire-byte map computed by the per-board P1 codec
    // (P1CodecHl2::applyPureSignalDdcConfig and friends) and applies it
    // to the connection-thread state that downstream bank composers
    // read from buildCodecContext().
    //
    // Mirrors Thetis console.cs:8527-8534 UpdateDDCs() [v2.10.3.13]:
    //   NetworkIO.EnableRxs(ddcEnable);
    //   NetworkIO.EnableRxSync(0, syncEnable);
    //   for (int i = 0; i < 4; i++) NetworkIO.SetDDCRate(i, rate[i]);
    //   NetworkIO.SetADC_cntrl1(cntrl1);
    //   NetworkIO.SetADC_cntrl2(cntrl2);
    //   NetworkIO.Protocol1DDCConfig(p1DdcConfig, P1_diversity, p1RxCount, nDdc);
    //
    // For P1, m_adcCtrl absorbs cntrl1+cntrl2 (bank 4 C1/C2),
    // m_psNDdc captures cfg.nDdc for the bank-2/3 freq-override gate
    // ((nddc==2 && XmitBit && puresignal_run) per networkproto1.c:985-1000),
    // and m_activeRxCount is updated from cfg.p1RxCount so EP6 frame
    // parsing layout matches what the radio sends during PS-MOX.
    //
    // P1 has no single CmdRx-equivalent — the 17/18 bank round-robin
    // picks up the new state on the next cycle.  setMox / setPuresignalRun
    // already arm m_forceBank0Next / m_forceBank11Next so the gating
    // bits land within ≤1 frame; the per-DDC freq overrides land in
    // banks 2-9 over the next 4-9 frames (~10-25 ms at 380.95 fps).
    void applyPsDdcConfig(const NereusSDR::PsDdcConfig& cfg);

    // Diversity lane B4: the en_diversity argument of Thetis
    // Protocol1DDCConfig, which locks the radio's VFOs while diversity runs
    // (bank 0 C4 bit 7). RadioModel passes DdcAssignment::p1Diversity on
    // every DDC assignment, so the bit sets when diversity starts and clears
    // when it stops or slice A closes. The Thetis lines are quoted at the
    // definition.
    //   From Thetis ChannelMaster/networkproto1.c:471 [v2.10.3.15]
    //     C4 |= (P1_en_diversity) << 7;		// if diversity, locks VFOs
    // Connection thread only (RadioModel marshals the call).
    void setDiversity(bool on);
    bool diversityForTest() const { return m_diversity; }

    // Phase 3M-4 Task 17 P1 follow-up — read-only access to the per-board
    // codec for ReceiverManager::setP1Codec injection.  Returns nullptr
    // until selectCodec() runs at applyBoardQuirks() time; subscribers
    // listen to p1CodecChanged() to know when this becomes valid.
    NereusSDR::IP1Codec* p1Codec() const { return m_codec.get(); }

    // HL2-specific: enqueue the I/O board probe (HW version + FW major/minor
    // reads). Public entry point for the Setup → Hardware → HL2 I/O Board
    // tab's "Probe" button; also called from connectToRadio() at startup.
    // Noop on non-HL2 boards.
    void requestIoBoardProbe();

    // Diagnostic: enqueue a read to every standard 7-bit I2C address (0x08..0x77)
    // on bus 1 / sub-register 0 so we can discover what device IS on the bus
    // when the IoBoardHl2 mi0bot probe (0x41/0x1D) returns floating-bus 0xFF.
    // The IoBoardHl2 dispatch will route each response into the lastI2cRead
    // signal which the Setup → HL2 I/O page logs raw.
    void requestI2cBusScan();

signals:
    // Phase 3M-4 Task 17 P1 follow-up — emitted from selectCodec() once
    // m_codec is assigned.  RadioModel::wireConnectionSignals subscribes
    // and forwards p1Codec() into ReceiverManager::setP1Codec so the
    // ddcConfigChanged dispatch path becomes live.
    void p1CodecChanged();

private slots:
    void onReadyRead();
    void onWatchdogTick();
    void onEp2PacerTick();
    void onReconnectTimeout();
    // Fires kConnectTimeoutMs after connectToRadio() if no first ep6 frame
    // arrives. Emits connectFailed(Timeout, ...) — Phase 3Q Task 3.
    void onConnectTimeout();

private:
    // ── The announced receiver count has two inputs ──────────────────────
    //
    // Two independent things need a say in how many receivers the wire
    // announces, and neither can see the other:
    //
    //   m_codecRxCount  what the DDC configuration requires. PureSignal
    //                   needs four (DDC0/1 as the sync pair, DDC2 feedback,
    //                   DDC3 TX monitor -- mi0bot console.cs:8757-8762
    //                   [v2.10.3.13-beta2] GetDDC). Written by
    //                   applyPsDdcConfig from PsDdcConfig::p1RxCount.
    //
    //   m_panRxCount    what the panadapters want. Written by
    //                   setActiveReceiverCount, which follows the operator
    //                   adding and removing panadapters.
    //
    //   m_slotRxCount   the highest live frame slot + 1, from
    //                   setLiveReceiverSlots (Phase 3F section 16.3.2). A
    //                   count of receivers stops being enough once routing
    //                   is by frame slot: with slice A closed, slices B and
    //                   C sit on slots 1 and 2, the panadapter axis says 2,
    //                   and slot 2 falls out of the frame. On the Atlas
    //                   (HPSDR) nothing else raises the count, because
    //                   Thetis gives that model no P1_rxcount
    //                   (console.cs:8533-8534 [v2.10.3.15]); wherever Thetis
    //                   does give one, m_codecRxCount still carries it.
    //
    // The announced count is the max of the three, so no axis can starve
    // another. Bench-caught 2026-08-01 (J.J. Boyd, KG4VCF) on a live HL2:
    // these were a single field written by three call sites, last writer
    // wins. Removing the second panadapter with PureSignal on dropped the
    // announcement to one receiver, and DDC2 and DDC3 left the ep6 frame
    // entirely until the next key-down happened to rewrite it.
    //
    // Latent until this branch, because the HL2 exposed one panadapter and
    // the pan axis never moved.
    //
    // announceRxCount() is the single writer; restartStreamWithCount is its
    // mechanism and is deliberately private, so no caller can set the count
    // while knowing only one of the two axes.
    void announceRxCount();

    // Live-apply an announced-count change to a running P1 connection.
    //
    // Updates m_activeRxCount then issues sendMetisStop() + sendPrimingBurst(3)
    // + sendMetisStart() so the radio re-arms its EP6 sender with the new
    // per-frame slot count encoded in bank-0 C0 bits 8-10 (nrx-1).
    //
    // Both sides have to change together: a frame composed under the old
    // layout and parsed under the new one is silently misparsed, because the
    // 7F 7F 7F sync check does not encode the layout.
    //
    // P1 EP6 frame parsing: parseEp6Frame() takes numRx as a parameter on
    // every call (not cached); it reads m_activeRxCount from the instance
    // method overload.  Updating m_activeRxCount is therefore sufficient to
    // handle the mid-stream count change — no MetisFrameParser rework needed.
    //
    // Must be called on the connection thread.
    //
    // Records the value without a restart when m_running is false; no-op
    // when the count is unchanged.
    //
    // Cite: networkproto1.c WriteMainLoop bank-0 C0 nrx bits [v2.10.3.13]
    // — same stop+prime+start cycle as restartStreamWithRate().
    void restartStreamWithCount(int newActiveRxCount);

    // Binds m_socket to the local address that reaches m_radioInfo.address,
    // on a port the OS chooses, as Thetis binds its listening socket to the
    // network card's address (network.c nativeInitMetis). Called by
    // connectToRadio() and each reconnect attempt. With no route, or if that
    // address will not bind, warns and binds every address (Any); the socket
    // is always left bound with the buffer sizes applied.
    void bindToRadioFacingAddress();
    // The local address that reaches m_radioInfo.address (a UDP connect),
    // null if none does within kRouteLookupMs, or the test override.
    QHostAddress radioFacingAddress() const;
    // Test stand-in for the route lookup (setRadioFacingAddressForTest).
    // Unconditional members: the class layout must not depend on
    // NEREUS_BUILD_TESTS.
    bool m_radioFacingOverridden{false};
    QHostAddress m_radioFacingOverride;
    // The socket buffer sizes init() sets; applied again after a rebind.
    void applySocketBufferSizes();

    // --- Wire format (networkproto1.c) — implemented in Tasks 7 & 8 ---
    void sendMetisStart(bool iqAndMic);
    void sendMetisStop();
    // Send one ep2 command frame with two C&C subframes drawn from the
    // round-robin bank sequence (0-17). Each call advances m_ccRoundRobinIdx
    // by 2 (one per subframe). Source: networkproto1.c:597-884 WriteMainLoop.
    void sendCommandFrame();
    // One subframe's C&C bytes (sendCommandFrame sends two per frame).
    void composeSubframe(quint8 out[5], int maxBank);
    // The last round-robin bank for this codec.
    int ccMaxBank() const;
    // The I2C frame for this subframe, if one is queued (false otherwise),
    // and the ordinary bank.
    bool composeI2cFrame(quint8 out[5]) const;
    void composeBank(int bankIdx, quint8 out[5]) const;
    // One 40 ms step of the HL2 I/O board poll (UpdateIOBoard).
    void ioBoardPollTick();
    // Send `countPerBank` ep2 command frames in two bursts with a 10ms gap.
    // Replaces the old ForceCandCFrame pattern — now simply drives the
    // round-robin forward, ensuring all banks get primed.
    void sendPrimingBurst(int countPerBank);
    // Compose 5 C&C bytes for any bank index 0-17.
    // Dispatcher ported from Thetis networkproto1.c WriteMainLoop cases 0-17.
    void composeCcForBank(int bankIdx, quint8 out[5]) const;
    void parseEp6Frame(const QByteArray& pkt);

    // --- Command & Control banks (NetworkIO.cs SetC0..SetC4) ---
    void composeCcBank0(quint8* out);
    void composeCcBank1(quint8* out);
    void composeCcBank2(quint8* out);
    void composeCcBank3(quint8* out);
    void composeCcTxFreq(quint8* out);
    void composeCcAlexRx(quint8* out);
    void composeCcAlexTx(quint8* out);
    void composeCcOcOutputs(quint8* out);
    void composeCcBank0Full(quint8 out[5]) const;

    // --- Per-board quirks — implemented in Tasks 11 & 12 ---
    void applyBoardQuirks();
    void hl2SendIoBoardTlv(const QByteArray& tlv);

    // Build m_codec from m_hardwareProfile.model. Called from applyBoardQuirks().
    void selectCodec();

    // Legacy compose path — preserved for the rollback feature flag for
    // one release cycle. Identical to the pre-refactor composeCcForBank.
    void composeCcForBankLegacy(int bankIdx, quint8 out[5]) const;

    // Snapshot all live state into a CodecContext for the codec call.
    CodecContext buildCodecContext() const;
    // Plan Task 14 fix wave: report bank 0's band-output byte (bandOutputsComposed).
    void publishBank0BandOutputs(const quint8 bank0[5]) const;

    // HL2-specific helpers (mi0bot Hermes-Lite branch, Task 12).
    // hl2SendIoBoardInit — issues I2C register reads at startup to detect the
    //   HL2 I/O board and latch its hardware version.
    //   Source: mi0bot IoBoardHl2.cs:129-145 IOBoard.readRequest() —
    //     I2C reads on bus 1 at addr 0x41 (HW version) and 0x1d (registers).
    //     Full I2C init sequence is deferred (TODO(3I-T12)); the standard
    //     NetworkIO start path already handles HL2 ep2 init.
    void hl2SendIoBoardInit();

    // Step machine for the mi0bot probe sequence.  mi0bot's I2CReadInitiate
    // refuses to enqueue if `in_index != out_index` (queue not empty), so reads
    // must be sequenced ONE AT A TIME.  Driver: i2cReadAnswered signal
    // from IoBoardHl2.  Source: console.cs:25796-25831 [@c26a8a4]
    enum class Hl2ProbeStep {
        Idle,
        WaitingForHwVersion,
        WaitingForFwMajor,
        WaitingForFwMinor,
        WaitingForControlInitAck,
        Done
    };
    Hl2ProbeStep m_hl2ProbeStep{Hl2ProbeStep::Idle};
    bool         m_hl2ProbeWired{false};   // one-shot signal-connect guard
    // Dispatch next step.  retAddr/retSubAddr identify the I2C read whose
    // response just arrived: the device address and register of the
    // pending read it answered, not C0's address bits (zero+zero from the
    // initial bootstrap call so the Idle state's transition fires).  Phase
    // 3L Codex P2 fix on PR #157; parity Task 14 follow-up.
    void hl2ProbeAdvance(quint8 retAddr, quint8 retSubAddr);

    // hl2CheckBandwidthMonitor — drives the HermesLiteBandwidthMonitor tick.
    //   When m_bwMonitor is wired, delegates to m_bwMonitor->tick() which runs
    //   the upstream compute_bps() algorithm and the NereusSDR throttle-detection
    //   layer.  When m_bwMonitor is null (non-HL2 or test seam without RadioModel),
    //   falls back to the legacy sequence-gap heuristic.
    //   Source: mi0bot bandwidth_monitor.{c,h} (MW0LGE) [@c26a8a4]
    void hl2CheckBandwidthMonitor();
    bool hl2IsThrottled() const { return m_hl2Throttled; }

    // Phase 3P-E Task 2: ep6 I2C response parsing.
    // Called from the instance parseEp6Frame() when C0 bit 7 is set.
    // Routes read data (C1-C4) back into the IoBoardHl2 register mirror.
    // Source: mi0bot networkproto1.c:478-493 [@c26a8a4]
    void parseI2cResponse(quint8 c0, quint8 c1, quint8 c2, quint8 c3, quint8 c4);

    void checkFirmwareMinimum(int fw);

    // --- State ---
    QUdpSocket* m_socket{nullptr};
    QTimer*     m_watchdogTimer{nullptr};
    QTimer*       m_ep2PacerTimer{nullptr};
    QElapsedTimer m_ep2PacerClock;
    qint64        m_ep2PacketsSent{0};
    QTimer*       m_reconnectTimer{nullptr};
    // Single-shot timer — fires kConnectTimeoutMs after connectToRadio() if
    // no first ep6 frame arrives. Emits connectFailed(Timeout, ...). Cancelled
    // on first good ep6 in onReadyRead(). Created in init(), Qt-parent-owned.
    QTimer*       m_connectWatchdog{nullptr};

    bool        m_running{false};
    bool        m_intentionalDisconnect{false};

    // --- Reconnect state machine (§3.6) ---
    // Timing constants are NereusSDR policy (documented in design doc §3.6),
    // not ported from Thetis.
    QDateTime   m_lastEp6At;                                    // UTC timestamp of last good ep6 frame
    bool        m_firstEp6Logged{false};                        // diagnostic: log once on first EP6 arrival per session
    bool        m_parseFailLogged{false};                       // diagnostic: log once if first ep6 parse fails
    bool        m_firstEmitLogged{false};                       // diagnostic: log once on first iqDataReceived emit
    int         m_reconnectAttempts{0};                         // how many retries so far this cycle
    // 25 ms watchdog tick for silence detection only. EP2 pacing has moved
    // to m_ep2PacerTimer (kEp2PacerIntervalMs) — see onEp2PacerTick.
    static constexpr int kWatchdogTickMs       = 25;            // watchdog silence-detection cadence
    // Connect watchdog: fires this many ms after connectToRadio() if no first
    // ep6 frame arrives → emits connectFailed(Timeout, ...). Design §4.1.
    static constexpr int kConnectTimeoutMs     = 2000;
    // How long bindToRadioFacingAddress() waits for the OS to name the local
    // address that reaches the radio. A UDP connect sends nothing and is
    // normally answered at once. NereusSDR-original.
    static constexpr int kRouteLookupMs        = 100;
    // Mic-frame LOS timeout — after this long without a successful mic16
    // dispatch, inject a zero block into TxMicSource so the worker keeps
    // ticking through silence.  Matches Thetis network.c:656 [v2.10.3.13]:
    //   prn->wdt ? 3000 : WSA_INFINITE
    static constexpr int kMicLosTimeoutMs      = 3000;
    // EP2 send cadence — target 381 pps (48 kHz audio / 126 samples per
    // EP2 frame) to match Thetis' sendProtocol1Samples semaphore-driven
    // audio clock. 2 ms PreciseTimer yields ~350-500 pps on Windows under
    // normal scheduling jitter. Source: networkproto1.c:700-747.
    static constexpr int kEp2PacerIntervalMs  = 2;

    // Spec rate: 48000 samples/s / 126 samples per EP2 packet = 380.95 pps
    //   → one packet every 2625 microseconds. Integer math keeps the catch-up
    //   loop simple and exact. Source: networkproto1.c:700-747 (48 kHz audio
    //   clock on sendProtocol1Samples).
    static constexpr qint64 kEp2PacketIntervalUs = 2625;

    // Safety cap on bursts per tick. Under normal Windows scheduling the
    // pacer fires every 10-15 ms, so a typical catch-up burst is 4-6 packets.
    // This cap protects against pathological scheduler stalls where a single
    // tick covers 100+ ms; without it one tick could dump 40+ packets into
    // the socket and overrun the radio's UDP receive buffer.
    static constexpr int kEp2MaxBurstPerTick = 16;
    // Defaults unchanged. As of 2026-07-25 the first two are seeded into
    // instance members so tests can compress the reconnect timeline;
    // nothing outside the test suite calls setReconnectTimingForTest(),
    // so production timing is bit-identical to before.
    // R-R3-49: the wait for data while the Network Watchdog is on (was 2000,
    // a NereusSDR value).
    // From Thetis networkproto1.c:292-294 [v2.10.3.15]:
    //   //MW0LGE_21g WSAWaitForMultipleEvents(1, &prn->hDataEvent, FALSE, WSA_INFINITE, FALSE);
    //   //added similar timout code from ReadThreadMainLoop
    //   DWORD retVal = WSAWaitForMultipleEvents(1, &prn->hDataEvent, FALSE, prn->wdt ? 3000 : WSA_INFINITE, FALSE);
    static constexpr int kWatchdogSilenceMs    = 3000;          // silence → LinkLost threshold
    static constexpr int kReconnectIntervalMs  = 5000;          // delay between retry attempts
    static constexpr int kMaxReconnectAttempts = 3;             // max retries before staying in Error

    int m_watchdogSilenceMs{kWatchdogSilenceMs};
    int m_reconnectIntervalMs{kReconnectIntervalMs};

    quint32 m_epSendSeq{0};
    quint32 m_epRecvSeqExpected{0};
    // R-R3-21: last EP6 sequence number received, for the sequence-error
    // count. From Thetis networkproto1.c:28 "int MetisLastRecvSeq = 0;"
    // [v2.10.3.15].
    quint32 m_ep6LastRecvSeq{0};
    // R-R3-32 (parity Task 6): an ep6 frame has arrived since connect, so
    // the next one has a number to follow (RadioLinkStats packet loss).
    bool m_ep6SeqPrimed{false};
    int     m_ccRoundRobinIdx{0};

    // 3M-1a E.3: force the next sendCommandFrame() to start with bank 0 so
    // the MOX bit lands on the wire within ≤1 frame of setMox().
    // Set by setMox() on every call (Codex P2: safety effect before guard).
    // Cleared by sendCommandFrame() after it emits bank 0.
    bool    m_forceBank0Next{false};

    // 3M-1a E.4: force the next sendCommandFrame() to jump to bank 10 so
    // the T/R relay bit (C3 bit 7) lands on the wire within ≤1 frame of
    // setTrxRelay().  Mirrored from E.3's forceBank0Next pattern.
    // Set by setTrxRelay() on every call (Codex P2: safety effect before guard).
    // Cleared by sendCommandFrame() after it emits bank 10.
    bool    m_forceBank10Next{false};

    // 3M-1b G.3: force the next sendCommandFrame() to jump to bank 11 so
    // the mic_trs bit (C1 bit 4) lands on the wire within ≤1 frame of
    // setMicTipRing().  Mirrors the m_forceBank10Next pattern exactly.
    // Set by setMicTipRing() on every call (Codex P2: safety effect before guard).
    // Cleared by sendCommandFrame() after it emits bank 11.
    bool    m_forceBank11Next{false};

    // Phase 3M-4 Task 17 P1 follow-up: force the next sendCommandFrame() to
    // jump to bank 4 so the TX step attenuator value (bank 4 C3) lands on
    // the wire within ≤1 frame of setTxStepAttenuation().  Mirrors the
    // m_forceBank0/10/11Next pattern.  Required for PureSignal auto-attenuate
    // tick (PSForm.cs:763-778 SetNewValues transition) where the 100 ms
    // tick computes a fresh deltaDb from feedbackLevel and pushes it via
    // setAttOnTxValue → setTxStepAttenuation; without this flush, the new
    // ATT value waits up to ~16 frames (~42 ms) for bank 4 to come around
    // in the round-robin, which adds non-determinism to convergence
    // measurements on bench.  Set by setTxStepAttenuation() on every call
    // (Codex P2 ordering: flush flag before idempotent guard).  Cleared by
    // sendCommandFrame() after it emits bank 4.
    //
    // Mirrors Thetis ChannelMaster/netInterface.c:1006-1016 SetTxAttenData()
    // [v2.10.3.13] which fires CmdTx() (the P1-equivalent of an explicit
    // bank-4 send) immediately after writing prn->adc[i].tx_step_attn.
    bool    m_forceBank4Next{false};

    // Phase 3M-4 Task 17 P1 follow-up: PureSignal nDdc state for the
    // bank 2/3 (RX1/RX2 VFO DDC) freq-override gate.  Captured from
    // PsDdcConfig.nDdc by applyPsDdcConfig().  When (m_psNDdc == 2 &&
    // m_mox && m_puresignalRun), banks 2 and 3 emit prn->tx[0].frequency
    // for DDC0/DDC1 instead of prn->rx[0/1].frequency.  Mirrors the
    // (nddc == 2) gate in mi0bot networkproto1.c:985 + 1000
    // [v2.10.3.13-beta2].
    //
    // Default 2 matches the connect-time default of m_activeRxCount=2
    // (P1RadioConnection.cpp:686 [HermesII Hermes/Hermes]) so the gate
    // remains correct for HermesII boards even before the codec layer
    // pushes a PsDdcConfig.  HL2 / Hermes / ANAN10 / ANAN100 will set
    // this to 4 once the codec config arrives, disabling the override
    // (correct: the firmware handles freq routing internally for nddc==4
    // boards via cntrl1=4 ADC-to-DDC steering, console.cs:8486
    // [v2.10.3.13-beta2]).
    int     m_psNDdc{2};

    // Phase 3M-4 bench-fix 2026-05-23 (J.J. Boyd KG4VCF): PureSignal DDC
    // pair latched from applyPsDdcConfig().  Both default to -1 (no PS
    // pair configured) so the parseEp6Frame paired-emit only fires when
    // the codec has actually configured a pair.
    //
    // mi0bot networkproto1.c:549-553 [v2.10.3.13-beta2] HL2 case 4:
    //   xrouter(0, 0, 0, spr, prn->RxBuff[0]);  // DDC0 → main RX
    //   twist(spr, 2, 3, 1);                    // DDC2+DDC3 → PS pair
    //   xrouter(0, 0, 2, spr, prn->RxBuff[1]);  // DDC1 → secondary RX
    // The twist() call pairs slots 2+3 from the same RxBuff inside the
    // same xrouter pass — mirrors what we do per-EP6-frame here.
    //
    // Mirrors Thetis cmaster.cs:533-534 [v2.10.3.13] SetPSRxIdx /
    // SetPSTxIdx convention — the per-board codec is authoritative.
    int     m_psFbDdc{-1};       // PS-feedback DDC (Thetis ps_rx_idx)
    int     m_psTxMonDdc{-1};    // TX-monitor DDC  (Thetis ps_tx_idx)

    // Rate limit for the PS stream diagnostic in parseEp6Frame. One line per
    // second; at 192 kHz the paired emit fires roughly 750 times a second.
    qint64  m_psDiagLastMs{0};

    // A per-sample bin histogram lived here while the HL2 PureSignal stall was
    // being chased (2026-08-01). It found the cause -- at 48 kHz a 700/1900 Hz
    // two-tone envelope repeats every 40 samples with only 20 distinct
    // magnitudes, and bin 5 of 16 falls in a gap, so LCOLLECT can never fill
    // all its bins -- and was removed once answered: it cost a sqrt per sample
    // on the connection thread at up to 192k samples/sec. The envelope
    // min/max/mean line that remains is enough to recognise the same shape
    // again, and is gated on MOX. Full analysis in the HL2 slice-cap design
    // doc, PureSignal section.

    // Phase 3P-A: per-board codec chosen at applyBoardQuirks() time.
    // Null when m_caps is null (pre-connect) or env var
    // NEREUS_USE_LEGACY_P1_CODEC=1 forces legacy compose path.
    std::unique_ptr<IP1Codec> m_codec;
    bool m_useLegacyCodec{false};

    int     m_sampleRate{48000};

    // The count actually on the wire, and read back by parseEp6Frame for the
    // slot layout. Derived: announceRxCount() is its only writer. See the
    // announceRxCount declaration above for the two axes that feed it.
    int     m_activeRxCount{1};
    int     m_codecRxCount{1};   ///< DDC configuration axis (PureSignal, diversity)
    int     m_panRxCount{1};     ///< panadapter axis
    int     m_slotRxCount{0};    ///< highest live frame slot + 1 (0 = none / not told yet)

    // The live frame slots and the slot standing in for Thetis's RX1. See
    // setLiveReceiverSlots. m_rx1Slot starts at 0, so until a mask arrives,
    // and whenever slot 0 is live, every filter reads exactly what it read
    // before routing by frame slot.
    quint32 m_liveSlotMask{0};
    int     m_rx1Slot{0};

    // The frame slots of the slices counted on the input (AlexRxBpf::
    // countedSlotsAdc0). The receive low-pass follows the highest, and on
    // the HL2 the OC receive band follows the highest band among them
    // (mi0bot Penny.cs UpdateExtCtrl). 0 = none counted, and the RX1
    // stand-in rule applies.
    quint32 m_countedSlotsAdc0{0};

    // Recompute the receive-side Alex selections (m_alexHpfBits and
    // m_alexLpfBitsRx) from the RX1 stand-in. `changedSlot` is the frame slot
    // whose frequency just moved, or -1 when the stand-in itself moved.
    void recomputeReceiveFilters(int changedSlot);

    // setAlexLPF (codec::alex::setAlexLpf) on m_alexLpfBitsRx / Tx, the Alex0
    // and Alex1 masks, then reports the low-pass in use. `freqIsTx` is
    // Thetis's freqIsTX; the keyed state is m_mox.
    void applyAlexLpf(double freqMhz, bool freqIsTx);
    // UpdateAlexTXFilter: the receive-frequency selection, unkeyed only.
    void applyReceiveAlexLpf(quint64 rx1Hz, quint64 fallbackHz);
    // Whether setAlexLPF's alexpresent holds: an Alex board, or the HL2,
    // whose firmware and N2ADR board read the same bits.
    bool alexLpfPresent() const;

    // The frequency whose band selects the OC outputs: the transmitting
    // slice's while keyed, the RX1 stand-in's while not (plan Task 14).
    quint64 ocBandFrequencyHz() const;

    // The receivers counted on the input, in slot order, with their DDC
    // centres and slice VFOs (shared-input filters, ruling (c)).
    QList<SharedInputLowPass::Candidate> countedCandidates() const;

    // HL2 mic decimation state.  At sample rates above 48 kHz the radio
    // embeds one mic sample per I/Q sample group in EP6 frames (so mic
    // arrives at 192 kHz when sampleRate=192000); we decimate to 48 kHz
    // before feeding TxMicSource.  Updated by setSampleRate() per the
    // table in Thetis netInterface.c:1287-1310 [v2.10.3.14]:
    //   48k → 1, 96k → 2, 192k → 4, 384k → 8 (default 4).
    // m_micDecimationCount persists across EP6 frames; reset to 0 when
    // the factor changes (matches netInterface.c:1310).
    int     m_micDecimationFactor{1};
    int     m_micDecimationCount{0};

    quint64 m_rxFreqHz[7]{};
    // Each slot's slice VFO frequency (setReceiverVfoFrequencies). 0 = not
    // told, and the band falls back to the slot's DDC centre above.
    quint64 m_rxVfoHz[7]{};
    quint64 m_txFreqHz{0};
    // THREAD SAFETY: written only from the connection thread; every compose
    // function and fillTxZone() read it there too.
    //
    // Atomic because of one genuine cross-thread READER, exactly as on
    // P2RadioConnection::m_mox: sendTxIq() runs on the TX/audio producer
    // thread and gates its producer-rate telemetry on m_mox.  A plain bool
    // there is a data race with setMox() on the connection thread.  Codex
    // review, PR #291.
    //
    // Default seq_cst ordering is deliberate: read once per sendTxIq() call,
    // not once per sample, so the ordering cost is irrelevant next to the
    // surrounding ring arithmetic.
    std::atomic<bool> m_mox{false};
    // Link-loss latch (TX safety, whole-branch review 2026-09-30, fix
    // round 1). Set when the watchdog declares a link lost while keyed, and
    // at a reconnect attempt that found a key made during the outage. While
    // set, setMox(true) is refused; the reconnect reaching Connected
    // unkeyed clears it, as do setMox(false) and connectToRadio. Follows
    // P2RadioConnection::m_linkLossLatched, which refuses a key through a
    // lost link the same way; Thetis itself never reconnects a lost P1
    // link (see dropTransmitForNewLink).
    // Connection thread only.
    bool    m_linkLossLatched{false};
    bool    m_linkLossRefusalLogged{false};
    int     m_antennaIdx{0};
    int     m_rxOnlyAnt{0};   // RX-only input mux (0..3). Bank 0 C3 bits 5-6.
    bool    m_rxOut{false};   // _Rx_1_Out relay. Bank 0 C3 bit 7.
    // Alex attenuator _20_dB_Atten / _10_dB_Atten. Bank 0 C3 bits 1 / 0.
    bool    m_alex20dB{false};
    bool    m_alex10dB{false};

    // Per-ADC state — initialized from HardwareProfile at connect time
    bool    m_dither[3]{true, true, true};
    bool    m_random[3]{true, true, true};
    // The HL2 options behind bank 0 C3 bits 3 and 4 on a Hermes Lite 2
    // (setHl2BandVolts / setHl2PsSync), off until the saved options arrive.
    bool    m_hl2BandVolts{false};
    bool    m_hl2PsSync{false};
    // HL2 bank 17: TX buffer latency and PTT hang, mi0bot's create_rnet
    // defaults (netInterface.c:1709-1710 [@c26a8a4]) until the saved
    // values arrive.
    int     m_hl2TxLatencyMs{20};
    int     m_hl2PttHangMs{12};
    // HL2 bank 18: reset on Ethernet disconnect, off as mi0bot's
    // create_rnet leaves it (netInterface.c:1724 [@c26a8a4]).
    bool    m_hl2ResetOnDisconnect{false};
    // HL2 Swap audio channels (setHl2SwapAudioChannels), off as mi0bot's
    // prn->swap_audio_channels starts. Connection thread.
    bool    m_hl2SwapAudioChannels{false};
    // HL2 clock options (setHl2Clock): off, off and 116 MHz as mi0bot's
    // designer leaves them (setup.designer.cs:11159 udCl2Freq.Value = 116
    // [@c26a8a4]) until the saved options arrive.
    bool    m_hl2Ext10MHz{false};
    bool    m_hl2Cl2Enable{false};
    int     m_hl2Cl2FreqKHz{116000};
    // Clock chip writes waiting for room in the I2C queue, one list per
    // mi0bot WriteVersaClockAsync call, sent in order. `failures` counts
    // attempts in a row that found the queue full with nothing leaving it
    // (`dequeuedAtFailure` is the queue's count at the last one).
    enum class Hl2ClockKind { Ext10, Cl2 };
    struct Hl2ClockSequence {
        Hl2ClockKind kind{Hl2ClockKind::Cl2};
        bool extOn{false};                               // Ext10 lists only
        std::vector<std::pair<quint8, quint8>> writes;   // (register, data)
        std::size_t next{0};
        int failures{0};
        quint64 dequeuedAtFailure{0};
    };
    std::vector<Hl2ClockSequence> m_hl2ClockPending;
    QTimer* m_hl2ClockRetryTimer{nullptr};
    // An option whose list was dropped before it finished (timed out, the
    // link went down, no I/O board): its current value is sent again at
    // the next connect even when it is off.
    bool    m_hl2Ext10Incomplete{false};
    bool    m_hl2Cl2Incomplete{false};
    // A list whose writes all went into the I2C queue, not yet known to
    // have reached the radio: `leavesAt` is the queue's enqueued count
    // after its last write, and `left` is set by the first ep6 frame that
    // finds that write gone from the queue. The next ep6 frame after that
    // clears the option's incomplete flag; a disconnect or a lost link
    // before then sets it (hl2ClockMarkUnconfirmed), so frames that sat
    // in the queue, or went out while the radio had stopped answering,
    // are sent again at the next connect. One entry per kind.
    struct Hl2ClockSent {
        Hl2ClockKind kind{Hl2ClockKind::Cl2};
        quint64 leavesAt{0};
        bool left{false};
    };
    std::vector<Hl2ClockSent> m_hl2ClockUnconfirmed;
    // The I/O board's clear count last seen (hl2ClockCheckCleared).
    quint64 m_hl2ClockClearSeen{0};
    // The first ep6 frame promotes Connecting to Connected; the HL2 clock
    // options that are on go to the radio then.
    void enterDataFlowing();
    void hl2ClockOnDataFlowing();
    void hl2ClockRequest(bool ext10, bool cl2);
    void hl2ClockDropPending();
    void hl2ClockListDone(const Hl2ClockSequence& seq);
    void hl2ClockListDropped(const Hl2ClockSequence& seq);
    void hl2ClockPump();
    void hl2ClockConfirmSent();
    void hl2ClockMarkUnconfirmed();
    bool hl2ClockCheckCleared();

    // mi0bot prn->i2c.delay: subframes until the next I2C frame may go
    // (composeSubframe).
    int     m_i2cDelay{0};
    // HL2 I/O board poll (ioBoardPollTick): 40 ms steps, the TX state and
    // aerial values to write, and what was last written.
    static constexpr int kIoBoardPollMs = 40;
    QTimer* m_ioBoardPollTimer{nullptr};
    int     m_ioTxMode{-1};
    qint64  m_ioTxFrequencyHz{0};
    quint8  m_ioAerialMode{0};
    quint8  m_ioAerialPorts{0};
    int     m_ioWrittenMode{-1};
    qint64  m_ioWrittenFrequencyHz{0};
    quint8  m_ioWrittenAerialMode{0};
    quint8  m_ioWrittenAerialPorts{0};
    // The radio is run as a Hermes Lite 2 (HPSDRModel::HERMESLITE).
    bool    isHl2() const;
    bool    m_rxPreamp[3]{};
    int     m_stepAttn[3]{};      // per-ADC step attenuator (0-31)
    int     m_txStepAttn{0};

    // Alex filter state — computed from frequency
    quint8  m_alexHpfBits{0};     // Bank 10 C3: HPF select bits

    // Bank 10 C4 carries ONE low-pass field, not Protocol 2's Alex0/Alex1
    // pair, and Thetis fills it from prbpfilter, the Alex0 struct:
    //   From Thetis ChannelMaster/networkproto1.c:587-590 [v2.10.3.15]
    //     C4 = (prbpfilter->_30_20_LPF & 1) | ((prbpfilter->_60_40_LPF & 1) << 1) | ...
    // and mi0bot's HL2 loop emits the same struct
    // (networkproto1.c:1085-1088 [v2.10.3.14-beta1]), so the HL2 is not a
    // carve-out.
    //
    // Alex0 is written by the `isMox || !isTX` arm, which makes the single
    // field carry the TRANSMIT selection while keyed and the RECEIVE
    // selection while not:
    //   From Thetis ChannelMaster/netInterface.c:682-726 [v2.10.3.15]
    //     void SetAlexLPFBits(int bits, bool isTX, bool isMox)
    //     if (isMox || isTX)   -> Alex1LPFMask (prbpfilter2, P2 only)
    //     if (isMox || !isTX)  -> AlexLPFMask  (prbpfilter,  this byte)
    //   Upstream comment preserved verbatim (netInterface.c:676-680):
    //     // LPF bits can be used in older radioas as part of RX filtering too.
    //     // Change to protocol 2 from 4.3 onwards: TX settings are encoded in
    //     // the Alex1 word to remain comparible with older hardware, the logic
    //     // will be:
    //     // if MOX, write settings to alex0 and alex1
    //     // if not MOX, write to alex1 if a TX setting else write to alex0
    //
    // Keeping only the transmit-derived mask was fine while one slice both
    // transmitted and received on the same frequency. Phase 3F binds the
    // transmitter to one slice while the operator listens on another, so a
    // transmitter parked on 80 m put a ~4 MHz low-pass in front of a
    // receiver listening on 10 m. These are the "older radios" the upstream
    // comment names, so this is where it bites.
    //
    // Both default to 0, not to the 6 m fall-through, and that is upstream
    // parity rather than an oversight. Thetis's AlexLPFMask is a
    // zero-initialised C global (ChannelMaster/network.h:392 [v2.10.3.15])
    // and setAlexLPF only ever writes it under `alexpresent && !initializing`
    // (console.cs:7186 [v2.10.3.15]), so a board with no Alex card emits a
    // zero C4 for the life of the session and every board emits zero until
    // the first selection is computed. Protocol 2's AlexState seeds 0x10
    // instead, but it seeds the frequencies alongside it in connectToRadio,
    // so it never ships an uninitialised pair.
    //
    // Nothing here relies on the zero as a filter choice: RadioModel pushes
    // setTxFrequency on Connected and queues setReceiverFrequency before
    // connectToRadio, so both masks carry a real selection before the
    // operator can key.
    // Thetis's two masks (netInterface.c AlexLPFMask / Alex1LPFMask
    // [v2.10.3.15]), written as SetAlexLPFBits writes them: unkeyed, the
    // receive selection goes to Alex0 and the transmit selection to Alex1;
    // keyed, the transmit selection goes to both.
    quint8  m_alexLpfBitsRx{0};  // Alex0: the receive selection (unkeyed)
    quint8  m_alexLpfBitsTx{0};  // Alex1: the transmit selection

    // Phase 3F: AlexController's decision for the single P1 filter chain.
    // -1 = no decision yet, use the RX0-frequency-derived m_alexHpfBits.
    int     m_alexRxHpfOverride{-1};

    // Effective bank-10 C3 HPF bits (override when set, else m_alexHpfBits).
    quint8  effectiveAlexHpfBits() const;

    // The board capabilities to filter-gate on, valid before connectToRadio.
    //
    // m_caps is assigned inside connectToRadio, but RadioModel queues the
    // first setReceiverFrequency BEFORE dispatching connectToRadio so the
    // opening C&C frame carries the persisted VFO (RadioModel.cpp, the
    // "Now dispatch connectToRadio" comment). At that moment m_caps is still
    // null while m_hardwareProfile has already been handed over, so gating on
    // m_caps alone silently skipped the connect-time filter selection on
    // every Alex board that is not the HL2. Harmless while the low-pass was
    // written from setTxFrequency on Connected; not harmless once the
    // receive-derived mask is the one the wire reads while unkeyed, because
    // nothing else would write it until the operator turned the VFO.
    const BoardCapabilities* filterCaps() const {
        return m_caps ? m_caps : m_hardwareProfile.caps;
    }

    // Effective bank-10 C4 LPF bits: the transmit selection while keyed, the
    // receive selection while not. The compose-time form of SetAlexLPFBits's
    // `isMox || !isTX` guard on the Alex0 word
    // (netInterface.c:705-717 [v2.10.3.15]); Thetis reaches the same state by
    // re-driving on both MOX edges instead
    // (console.cs:29083-29099 + 29140-29148 HdwMOXChanged [v2.10.3.15]).
    quint8  effectiveAlexLpfBits() const;

    // ── TX I/Q ring buffer (3M-1a E.2) ───────────────────────────────────────
    // Pre-allocated to hold kTxIqBufSamples×8 bytes.  Each slot is one
    // P1 wire sample: [mic_L hi][mic_L lo][mic_R hi][mic_R lo]
    //                 [I hi][I lo][Q hi][Q lo]  — big-endian int16.
    //
    // Layout ported from deskhpsdr/src/old_protocol.c:2421-2458
    // [@120188f] (old_protocol_iq_samples / TXRING_AUDIO_SAMPLE_BYTES).
    //
    // One EP2 frame carries 2×63 = 126 samples.  kTxIqBufSamples is sized
    // to match deskhpsdr's TXRING_MAX_BLOCKS×126 (32 blocks): 4032 samples,
    // ~84 ms at the 48 kHz wire rate, before the ring is full.  A full ring
    // drops the rest of the producer's block (nothing unread is
    // overwritten) and counts it in m_txIqOverflowSamples (G-07).
    // Source: deskhpsdr/src/old_protocol.c:460-461 [@120188f]
    //   TXRING_AUDIO_FRAMES_PER_BLOCK 126
    //   TXRING_MAX_BLOCKS             32
    //
    // SPSC ring buffer — audio thread writes, connection thread reads.
    // Memory ordering:
    //   - m_txIqWritePos: single audio-thread writer; relaxed store /
    //     acquire load from connection thread (mirrors classic Lamport SPSC).
    //   - m_txIqReadPos: single connection-thread writer; relaxed store /
    //     acquire load from audio thread.
    //   - m_txIqCount: cross-thread fetch_add (audio) / fetch_sub (conn);
    //     release on writes that publish new bytes, acquire on reads that
    //     consume bytes. The release/acquire pair pins the byte-write
    //     order before the count update.
    //
    // CLAUDE.md mandates atomics for cross-thread DSP; deskhpsdr's
    // old_protocol.c:466-469 [@120188f] uses the same atomic_int pattern.
    static constexpr int kTxIqBufSamples = 126 * 32;  // 4032 samples, 32256 bytes
    static constexpr int kTxIqBytesPerSample = 8;
    // Pre-allocated — no heap in the hot path.
    std::array<quint8, kTxIqBufSamples * kTxIqBytesPerSample> m_txIqBuf{};
    std::atomic<int> m_txIqWritePos{0};  // audio thread writes; relaxed store
    std::atomic<int> m_txIqReadPos{0};   // connection thread writes; relaxed store
    std::atomic<int> m_txIqCount{0};     // both threads: fetch_add (audio, release) / fetch_sub (conn)
    // G-07: samples a full ring refused since the last key, reported as
    // TxSendStats::overflowSamples.  Audio thread adds; any thread reads.
    std::atomic<quint64> m_txIqOverflowSamples{0};

    // TX I/Q ring pre-prime flag.  setMox(true) sets it on the connection
    // thread; sendTxIq consumes it on the TX worker thread (single-writer
    // invariant preserved).  When consumed, sendTxIq pushes a 20 ms
    // cushion of zero samples (960 samples = 7680 bytes at the P1 48 kHz
    // wire rate) into the ring BEFORE the real first-block samples, so
    // fillTxZone's 63-sample-per-zone drain has headroom while the
    // producer settles.  Same mechanism as the P2 cushion; see
    // P2RadioConnection.h for the architectural rationale.
    std::atomic<bool> m_txIqPrimePending{false};

    // Float→int16 + EP2 zone fill helper.
    // Returns true if 63 samples were available and written, false if underrun.
    bool fillTxZone(quint8* zone63) noexcept;
    // Radio codec (2026-09-30): writes 63 samples of the radio's receive
    // audio into the L/R bytes (0-3) of each 8-byte slot of an EP2 zone,
    // after fillTxZone. Leaves them zero while the audio ring has none due.
    void fillRadioAudioZone(quint8* zone63) noexcept;
    // G-05 follow-up: setMox(false) drops whatever is still queued (and an
    // unused key cushion), so none of it leads the next key.
    void discardTxIqOnUnkey() noexcept;
    // TX safety (whole-branch review 2026-09-30): every new link starts
    // unkeyed. Unkeys (as setMox(false) does), drops the PureSignal run
    // flag, releases the T/R relay, zeroes the drive and puts bank 0 on the
    // next frame. Called by connectToRadio, by the watchdog when it declares
    // the link lost, and by every reconnect attempt before its priming burst.
    void dropTransmitForNewLink();

    // Hardware config from profile
    int     m_txDrive{0};
    bool    m_paEnabled{false};
    bool    m_duplex{true};
    bool    m_diversity{false};
    quint8  m_ocOutput{0};
    // Diagnostic: log a single line whenever the resolved ocByte changes.
    // mutable so const buildCodecContext() can update it.
    mutable int m_lastOcByteLogged{0xFFFF};
    // Diagnostic: log the TX-deciding wire bytes on each MOX edge. Same
    // mutable-for-const reason as above. -1 so the first edge always logs.
    mutable int m_lastMoxLogged{-1};
    quint16 m_adcCtrl{0};          // ADC-to-DDC assignment bits

    // P1-only ADC-to-DDC routing — Thetis `P1_adc_cntrl` global mirror.
    //
    // Source: Thetis console.cs:15120 [v2.10.3.13] declares
    //   `private int rx_adc_ctrl_P1 = 4`. Setter at console.cs:15121-15128
    //   invokes UpdateRXADCCtrlP1() (line 7325-7328) which calls
    //   NetworkIO.SetADC_cntrl_P1 (netInterface.c:992-996 [v2.10.3.13])
    //   storing into `P1_adc_cntrl`. The P1 wire reader is
    //   networkproto1.c:519-520 [v2.10.3.13]:
    //     C1 = P1_adc_cntrl & 0xFF;
    //     C2 = (P1_adc_cntrl >> 8) & 0b0011111111;
    //
    // Distinct from `m_adcCtrl` above: that field carries the P2-side
    // cntrl1/cntrl2 from UpdateDDCs (Thetis prn->rx[i].rx_adc fields).
    // Before 2026-05-17 NereusSDR P1 codecs conflated the two — surfaced
    // while diagnosing issue #263, fixed by this struct + the codec
    // bank-4 read switching to ctx.p1AdcCntrl.
    //
    // Default 0 is the NereusSDR-side practical default: matches wire
    // bytes observed on a working Thetis-driven ANAN-10E on 2026-05-09.
    // Thetis fresh-install defaults to 4; we override based on board
    // adcCount at connect time (applyBoardQuirks sets 4 for 2-ADC SKUs,
    // 0 for 1-ADC SKUs). Per-MAC AppSettings under
    //   hardware/<mac>/p1AdcCntrl
    // overrides the board default for users who configured a specific
    // per-DDC ADC routing via the future Setup → Hardware → P1 ADC
    // Routing page (planned follow-up; manual AppSettings edit works
    // today).
    quint16 m_p1AdcCntrl{0};

    // Reconnect log guard
    bool    m_reconnectedLogged{false};

    const BoardCapabilities* m_caps{nullptr};

    // Non-owning pointer to RadioModel's OcMatrix.  When non-null,
    // buildCodecContext() derives ctx.ocByte from
    // m_ocMatrix->maskFor(bandFromFrequency(rx0Hz), m_mox) instead of
    // the legacy m_ocOutput field.  Null in test seams that don't wire
    // RadioModel (falls back to m_ocOutput == 0).  Phase 3P-D Task 3.
    const OcMatrix* m_ocMatrix{nullptr};
    // R-R3-49: non-owning; RadioModel's CalibrationController.
    const CalibrationController* m_calController{nullptr};
    // The frequency the radio is sent for a tuned one: corrected by the
    // calibration factor as Thetis's VFOfreq does.
    quint64 wireFrequencyHz(quint64 tunedHz) const;

    // Non-owning pointer to RadioModel's IoBoardHl2.  Set via setIoBoard()
    // at connect time; null on non-HL2 boards and in tests that don't wire
    // RadioModel.  Used by the ep6 read path to route I2C response bytes
    // back into the register mirror.  Phase 3P-E Task 2.
    IoBoardHl2* m_ioBoard{nullptr};

    // Non-owning pointer to RadioModel's HermesLiteBandwidthMonitor.
    // Set via setBandwidthMonitor() at connect time; null on non-HL2 boards
    // and in tests that don't wire RadioModel.  Used by:
    //   - onReadyRead(): recordEp6Bytes(pkt.size()) on each good ep6 frame
    //   - sendCommandFrame(): recordEp2Bytes(1032) on each ep2 send
    //   - onWatchdogTick(): tick() once per watchdog fire (via hl2CheckBandwidthMonitor)
    // Phase 3P-E Task 3.
    HermesLiteBandwidthMonitor* m_bwMonitor{nullptr};

    // Non-owning pointer to RadioModel's TxMicSource.  Set via
    // setTxMicSource() at connect time.  parseEp6Frame extracts mic16 bytes
    // and pushes them via inbound(); onWatchdogTick injects a zero block
    // after kMicLosTimeoutMs (3000 ms — Thetis network.c:656 [v2.10.3.13]).
    // Phase 3M-1c TX pump v3.
    TxMicSource* m_txMicSource{nullptr};

    // Wall-clock timestamp of the last successful mic16 dispatch.  Drives
    // LOS-zero injection at kMicLosTimeoutMs.  Reset on every successful
    // parseEp6Frame mic dispatch.  Phase 3M-1c TX pump v3.
    QDateTime m_lastMicAt;

    // Phase 3P-H Task 4: PA telemetry latches.
    // C0 cases 0x08/0x10/0x18 each carry only two of the six fields, so we
    // hold the most recent value of each between subframes and emit one
    // paTelemetryUpdated() per parsed frame.
    // Source: networkproto1.c:332-356 [@501e3f5]
    // C0 0x00/0x20 carry the ADC overflow flags — upstream cases at lines
    // 335/353/354/355 each read:
    //   adc[n].adc_overload = adc[n].adc_overload || (...);
    //   // only cleared by getAndResetADC_Overload(), or'ed with existing state //[2.10.3.13]MW0LGE
    quint16 m_paFwdRaw{0};
    quint16 m_paRevRaw{0};
    quint16 m_paExciterRaw{0};
    quint16 m_paUserAdc0Raw{0};   // AIN3 — MKII PA Volts
    quint16 m_paUserAdc1Raw{0};   // AIN4 — MKII PA Amps
    quint16 m_paSupplyRaw{0};     // AIN6 — Hermes supply Volts

    // --- HL2 bandwidth monitor legacy state (replaced by HermesLiteBandwidthMonitor) ---
    // Retained so hl2IsThrottled() still compiles and existing callers are unbroken
    // until they migrate to m_bwMonitor->isThrottled().  Phase 3P-E Task 3.
    bool      m_hl2Throttled{false};
    int       m_hl2ThrottleCount{0};
    QDateTime m_hl2LastThrottleTick;

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
    // Test-only helpers — allow unit tests to inject board caps without a live radio.
    void setBoardForTest(HPSDRHW board) {
        m_caps = &BoardCapsTable::forBoard(board);
        // Map HPSDRHW → canonical HPSDRModel so selectCodec() picks the right subclass.
        switch (board) {
            case HPSDRHW::HermesLite: m_hardwareProfile.model = HPSDRModel::HERMESLITE;   break;
            case HPSDRHW::HermesLiteRxOnly: m_hardwareProfile.model = HPSDRModel::HERMESLITE; break; // Task 15
            case HPSDRHW::OrionMKII:  m_hardwareProfile.model = HPSDRModel::ORIONMKII;    break;
            case HPSDRHW::Angelia:    m_hardwareProfile.model = HPSDRModel::ANAN100D;      break;
            case HPSDRHW::Orion:      m_hardwareProfile.model = HPSDRModel::ANAN200D;      break;
            case HPSDRHW::HermesII:   m_hardwareProfile.model = HPSDRModel::ANAN10E;       break;
            case HPSDRHW::Saturn:     m_hardwareProfile.model = HPSDRModel::ANAN_G2;       break;
            default:                  m_hardwareProfile.model = HPSDRModel::HERMES;        break;
        }
        applyBoardQuirks();
    }
    int currentAttenForTest() const { return m_stepAttn[0]; }
    int currentAttenForAdcForTest(int adc) const
    {
        return adc >= 0 && adc < 3 ? m_stepAttn[adc] : -1;
    }
    bool hl2ThrottledForTest() const { return m_hl2Throttled; }
    // Compress the silence-watchdog and reconnect-retry timeline so
    // tst_reconnect_on_silence does not sleep for 42 real seconds (which
    // set the parallel floor for the whole ctest run).  Guarded rather
    // than merely "unused in production": this object is moveToThread'd
    // onto the connection thread (RadioModel.cpp), so a public non-atomic
    // setter would be a data race waiting for its first caller.
    void setReconnectTimingForTest(int watchdogSilenceMs, int reconnectIntervalMs) {
        m_watchdogSilenceMs   = watchdogSilenceMs;
        m_reconnectIntervalMs = reconnectIntervalMs;
    }
    // R-R3-49: how long the link waits for data before it is declared lost
    // while the Network Watchdog is on.
    int watchdogSilenceMsForTest() const { return m_watchdogSilenceMs; }
    // The keyed state the next ep2 frame's C0 bit 0 carries.
    bool moxForTest() const { return m_mox; }
    bool linkLossLatchedForTest() const { return m_linkLossLatched; }
    // Expose private composeCcForBank for regression-freeze capture (Task 1) and
    // byte-table assertion tests (Task 16).
    void composeCcForBankForTest(int bankIdx, quint8 out[5]) const { composeCcForBank(bankIdx, out); }
    // Expose parseI2cResponse for ep6 read path unit tests (Phase 3P-E Task 2).
    void parseI2cResponseForTest(quint8 c0, quint8 c1, quint8 c2, quint8 c3, quint8 c4) {
        parseI2cResponse(c0, c1, c2, c3, c4);
    }
    // Expose the instance parseEp6Frame so PA-telemetry tests can feed a
    // hand-crafted 1032-byte ep6 datagram and assert paTelemetryUpdated()
    // emits the expected raw values.  Phase 3P-H Task 4.
    void parseEp6FrameForTest(const QByteArray& pkt) { parseEp6Frame(pkt); }

    // ── 3M-1a E.3 MOX wire test seams ────────────────────────────────────────
    // captureBank0ForTest — compose 5 bank-0 C&C bytes from current state
    // and return them.  Used by tst_p1_mox_wire to verify the MOX bit (C0
    // bit 0) is correct without needing a live socket.
    // Source for bit position: deskhpsdr/src/old_protocol.c:3597 [@120188f]
    //   output_buffer[C0] |= 0x01;  // MOX = byte 3 (C0), bit 0
    // HL2 firmware cross-check: dsopenhpsdr1.v:297 — ds_cmd_ptt_next = eth_data[0]
    QByteArray captureBank0ForTest() const {
        quint8 out[5] = {};
        composeCcForBankForTest(0, out);
        return QByteArray(reinterpret_cast<const char*>(out), 5);
    }

    // roundRobinIdxForTest — returns the current m_ccRoundRobinIdx value.
    int roundRobinIdxForTest() const { return m_ccRoundRobinIdx; }
    // The next subframe as sendCommandFrame composes it (I2C spacing and
    // the round-robin included), without a socket.
    void composeNextSubframeForTest(quint8 out[5]) { composeSubframe(out, ccMaxBank()); }
    void ioBoardPollTickForTest() { ioBoardPollTick(); }
    // HL2 clock seams: promote to Connected as the first ep6 frame does
    // (without a socket), run the clock write pump once, and count the
    // clock writes still waiting for room in the I2C queue.
    void simulateDataFlowingForTest() { enterDataFlowing(); }
    void hl2ClockPumpForTest() { hl2ClockPump(); }
    int hl2ClockPendingForTest() const
    {
        int n = 0;
        for (const Hl2ClockSequence& seq : m_hl2ClockPending) {
            n += static_cast<int>(seq.writes.size() - seq.next);
        }
        return n;
    }
    int hl2Cl2FreqKHzForTest() const { return m_hl2Cl2FreqKHz; }
    bool hl2SwapAudioChannelsForTest() const { return m_hl2SwapAudioChannels; }
    // What an ep6 frame does for the clock lists (hl2ClockConfirmSent),
    // and what a lost link does (the watchdog's LinkLost branch).
    void hl2ClockEp6ForTest() { hl2ClockConfirmSent(); }
    void hl2ClockLinkLostForTest() { hl2ClockMarkUnconfirmed(); }
    bool hl2ClockIncompleteForTest(bool ext10) const
    {
        return ext10 ? m_hl2Ext10Incomplete : m_hl2Cl2Incomplete;
    }

    // forceBank0NextForTest — returns m_forceBank0Next (the flush flag state).
    bool forceBank0NextForTest() const { return m_forceBank0Next; }

    // ── 3M-1a E.4 TRX relay wire test seams ──────────────────────────────────
    // captureBank10ForTest — compose 5 bank-10 C&C bytes from current state
    // and return them.  Used by tst_p1_trx_relay_wire to verify the T/R relay
    // bit (C3 bit 7, INVERTED: 0 = engaged, 1 = disabled) without needing a
    // live socket.
    // Source: deskhpsdr/src/old_protocol.c:2909-2910 [@120188f]
    //   if (txband->disablePA || !pa_enabled)
    //       output_buffer[C3] |= 0x80; // disable Alex T/R relay
    QByteArray captureBank10ForTest() const {
        quint8 out[5] = {};
        composeCcForBankForTest(10, out);
        return QByteArray(reinterpret_cast<const char*>(out), 5);
    }

    // forceBank10NextForTest — returns m_forceBank10Next (the bank-10 flush
    // flag state).  Used by tst_p1_trx_relay_wire to verify Codex P2 pattern.
    bool forceBank10NextForTest() const { return m_forceBank10Next; }

    // ── 3M-1b G.3 / G.4 / G.5 bank-11 wire test seams ──────────────────────
    // captureBank11ForTest — compose 5 bank-11 C&C bytes from current state
    // and return them.  Used by tst_p1_mic_tip_ring_wire (G.3), tst_p1_mic_bias_wire
    // (G.4), and tst_p1_mic_ptt_wire (G.5) to verify C1 bits 4/5/6 without
    // needing a live socket.
    // Source: Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
    //   C1 = ... | ((prn->mic.mic_trs & 1) << 4)   — bit 4, INVERTED
    //           | ((prn->mic.mic_bias & 1) << 5)    — bit 5
    //           | ((prn->mic.mic_ptt  & 1) << 6);   — bit 6, INVERTED
    QByteArray captureBank11ForTest() const {
        quint8 out[5] = {};
        composeCcForBankForTest(11, out);
        return QByteArray(reinterpret_cast<const char*>(out), 5);
    }

    // forceBank11NextForTest — returns m_forceBank11Next (the bank-11 flush
    // flag state).  Used by tst_p1_mic_tip_ring_wire to verify Codex P2 pattern.
    bool forceBank11NextForTest() const { return m_forceBank11Next; }

    // Phase 3M-4 Task 17 P1 follow-up — bank 2/3/4 capture + bank-4 flush
    // flag exposed for the P1 PureSignal regression tests.
    QByteArray captureBank2ForTest() const {
        quint8 out[5] = {};
        composeCcForBankForTest(2, out);
        return QByteArray(reinterpret_cast<const char*>(out), 5);
    }
    QByteArray captureBank3ForTest() const {
        quint8 out[5] = {};
        composeCcForBankForTest(3, out);
        return QByteArray(reinterpret_cast<const char*>(out), 5);
    }
    QByteArray captureBank4ForTest() const {
        quint8 out[5] = {};
        composeCcForBankForTest(4, out);
        return QByteArray(reinterpret_cast<const char*>(out), 5);
    }
    bool forceBank4NextForTest() const { return m_forceBank4Next; }
    int  psNDdcForTest() const { return m_psNDdc; }
    int  rx1SlotForTest() const { return m_rx1Slot; }
    int  activeRxCountForTest() const { return m_activeRxCount; }
    quint16 adcCtrlForTest() const { return m_adcCtrl; }
    quint16 p1AdcCntrlForTest() const { return m_p1AdcCntrl; }

    // ── 3M-1a E.2 TX I/Q test seams ─────────────────────────────────────────
    // sendTxIqAndCapture — feeds n interleaved float I/Q samples through the
    // ring buffer, drains one EP2 frame's worth (126 samples), and returns the
    // 1032-byte Metis frame as a QByteArray for wire-byte assertions.
    // The socket send is skipped (m_socket is null in tests), but frame
    // composition still happens normally so byte layout can be asserted.
    QByteArray sendTxIqAndCapture(const float* iq, int n) {
        sendTxIq(iq, n);
        // Build a full EP2 frame and fill both TX zones from the ring buffer.
        quint8 frame[1032];
        memset(frame, 0, sizeof(frame));
        frame[0] = 0xEF; frame[1] = 0xFE; frame[2] = 0x01; frame[3] = 0x02;
        const quint32 seq = m_epSendSeq++;
        frame[4] = quint8((seq >> 24) & 0xFF);
        frame[5] = quint8((seq >> 16) & 0xFF);
        frame[6] = quint8((seq >>  8) & 0xFF);
        frame[7] = quint8( seq        & 0xFF);
        frame[8] = 0x7F; frame[9] = 0x7F; frame[10] = 0x7F;
        // C&C bytes: zeros in test context (no codec wired)
        fillTxZone(frame + 16);
        fillRadioAudioZone(frame + 16);
        frame[520] = 0x7F; frame[521] = 0x7F; frame[522] = 0x7F;
        fillTxZone(frame + 528);
        fillRadioAudioZone(frame + 528);
        return QByteArray(reinterpret_cast<const char*>(frame), 1032);
    }

    // Access buffer count for buffer-state tests.
    int txIqBufferedSamplesForTest() const { return m_txIqCount.load(std::memory_order_acquire); }
#endif
};

} // namespace NereusSDR
