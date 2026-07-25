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
#include "codec/IP1Codec.h"
#include "codec/CodecContext.h"

namespace NereusSDR { class OcMatrix; }                  // forward decl — full header in .cpp
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
#include <vector>

namespace NereusSDR {

class P1RadioConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit P1RadioConnection(QObject* parent = nullptr);
    ~P1RadioConnection() override;

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
    void setTxMicSource(TxMicSource* src);

public slots:
    void init() override;
    void connectToRadio(const NereusSDR::RadioInfo& info) override;
    void disconnect() override;

    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) override;
    void setTxFrequency(quint64 frequencyHz) override;
    void setActiveReceiverCount(int count) override;
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

    // Task 1.7: live-apply an active-RX-count change to a running P1 connection.
    //
    // Updates m_activeRxCount then issues sendMetisStop() + sendPrimingBurst(3)
    // + sendMetisStart() so the radio re-arms its EP6 sender with the new
    // per-frame slot count encoded in bank-0 C0 bits 8-10 (nrx-1).
    //
    // P1 EP6 frame parsing: parseEp6Frame() takes numRx as a parameter on
    // every call (not cached); it reads m_activeRxCount from the instance
    // method overload.  Updating m_activeRxCount is therefore sufficient to
    // handle the mid-stream count change — no MetisFrameParser rework needed.
    //
    // Must be called on the connection thread (invoke via QMetaObject::
    // invokeMethod(Qt::QueuedConnection) from the main thread).
    //
    // No-op when m_running is false or when count equals m_activeRxCount.
    //
    // Cite: networkproto1.c WriteMainLoop bank-0 C0 nrx bits [v2.10.3.13]
    // — same stop+prime+start cycle as restartStreamWithRate().
    void restartStreamWithCount(int newActiveRxCount);
    void setAttenuator(int dB) override;
    void setPreamp(bool enabled) override;
    void setTxDrive(int level) override;
    void setMox(bool enabled) override;
    void setAntennaRouting(AntennaRouting routing) override;
    void setAlexRxBpf(AlexRxBpf bpf) override;
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
    // --- Wire format (networkproto1.c) — implemented in Tasks 7 & 8 ---
    void sendMetisStart(bool iqAndMic);
    void sendMetisStop();
    // Send one ep2 command frame with two C&C subframes drawn from the
    // round-robin bank sequence (0-17). Each call advances m_ccRoundRobinIdx
    // by 2 (one per subframe). Source: networkproto1.c:597-884 WriteMainLoop.
    void sendCommandFrame();
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
    // must be sequenced ONE AT A TIME.  Driver: i2cReadResponseReceived signal
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
    // response just arrived (zero+zero from the initial bootstrap call so
    // the Idle state's transition fires).  Phase 3L Codex P2 fix on PR #157.
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
    static constexpr int kWatchdogSilenceMs    = 2000;          // silence → Error threshold
    static constexpr int kReconnectIntervalMs  = 5000;          // delay between retry attempts
    static constexpr int kMaxReconnectAttempts = 3;             // max retries before staying in Error

    quint32 m_epSendSeq{0};
    quint32 m_epRecvSeqExpected{0};
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

    // Phase 3P-A: per-board codec chosen at applyBoardQuirks() time.
    // Null when m_caps is null (pre-connect) or env var
    // NEREUS_USE_LEGACY_P1_CODEC=1 forces legacy compose path.
    std::unique_ptr<IP1Codec> m_codec;
    bool m_useLegacyCodec{false};

    int     m_sampleRate{48000};
    int     m_activeRxCount{1};

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
    quint64 m_txFreqHz{0};
    bool    m_mox{false};
    int     m_antennaIdx{0};
    int     m_rxOnlyAnt{0};   // RX-only input mux (0..3). Bank 0 C3 bits 5-6.
    bool    m_rxOut{false};   // _Rx_1_Out relay. Bank 0 C3 bit 7.

    // Per-ADC state — initialized from HardwareProfile at connect time
    bool    m_dither[3]{true, true, true};
    bool    m_random[3]{true, true, true};
    bool    m_rxPreamp[3]{};
    int     m_stepAttn[3]{};      // per-ADC step attenuator (0-31)
    int     m_txStepAttn{0};

    // Alex filter state — computed from frequency
    quint8  m_alexHpfBits{0};     // Bank 10 C3: HPF select bits
    quint8  m_alexLpfBits{0};     // Bank 10 C4: LPF select bits

    // Phase 3F: AlexController's decision for the single P1 filter chain.
    // -1 = no decision yet, use the RX0-frequency-derived m_alexHpfBits.
    int     m_alexRxHpfOverride{-1};

    // Effective bank-10 C3 HPF bits (override when set, else m_alexHpfBits).
    quint8  effectiveAlexHpfBits() const;

    // ── TX I/Q ring buffer (3M-1a E.2) ───────────────────────────────────────
    // Pre-allocated to hold kTxIqBufSamples×8 bytes.  Each slot is one
    // P1 wire sample: [mic_L hi][mic_L lo][mic_R hi][mic_R lo]
    //                 [I hi][I lo][Q hi][Q lo]  — big-endian int16.
    //
    // Layout ported from deskhpsdr/src/old_protocol.c:2421-2458
    // [@120188f] (old_protocol_iq_samples / TXRING_AUDIO_SAMPLE_BYTES).
    //
    // One EP2 frame carries 2×63 = 126 samples.  kTxIqBufSamples is sized
    // to match deskhpsdr's TXRING_MAX_BLOCKS×126 (32 blocks) giving ~21 msec
    // of headroom at 48 kHz before the producer stalls.
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

    // Hardware config from profile
    int     m_txDrive{0};
    bool    m_paEnabled{false};
    bool    m_duplex{true};
    bool    m_diversity{false};
    quint8  m_ocOutput{0};
    // Diagnostic: log a single line whenever the resolved ocByte changes.
    // mutable so const buildCodecContext() can update it.
    mutable int m_lastOcByteLogged{0xFFFF};
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
    // Test-only helpers — allow unit tests to inject board caps without a live radio.
    void setBoardForTest(HPSDRHW board) {
        m_caps = &BoardCapsTable::forBoard(board);
        // Map HPSDRHW → canonical HPSDRModel so selectCodec() picks the right subclass.
        switch (board) {
            case HPSDRHW::HermesLite: m_hardwareProfile.model = HPSDRModel::HERMESLITE;   break;
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
    bool hl2ThrottledForTest() const { return m_hl2Throttled; }
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
        frame[520] = 0x7F; frame[521] = 0x7F; frame[522] = 0x7F;
        fillTxZone(frame + 528);
        return QByteArray(reinterpret_cast<const char*>(frame), 1032);
    }

    // Access buffer count for buffer-state tests.
    int txIqBufferedSamplesForTest() const { return m_txIqCount.load(std::memory_order_acquire); }

    // ── 3M-1a E.5 RUNSTOP watchdog wire test seams ───────────────────────────
    // metisStartPacketForTest — compose the 64-byte RUNSTOP start packet that
    // sendMetisStart() would send (without a live socket), so unit tests can
    // assert the watchdog bit (pkt[3] bit 7) without needing a real UDP socket.
    //
    // IMPORTANT: keep in sync with sendMetisStart() / sendMetisStop() in P1RadioConnection.cpp.
    //
    // Wire format: pkt[3] = run_bits | watchdog_disable_bit
    //   run_bits:     0x01 (IQ only) or 0x02 (IQ + mic)
    //   watchdog_bit: 0x00 if m_watchdogEnabled == true, 0x80 if false
    //
    // Source: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:200-203, 399-400
    //   eth_data[0] = run; eth_data[7] = watchdog_disable (1=disabled, 0=enabled)
    QByteArray metisStartPacketForTest(bool iqAndMic) const {
        QByteArray pkt(64, '\0');
        pkt[0] = static_cast<char>(0xEF);
        pkt[1] = static_cast<char>(0xFE);
        pkt[2] = static_cast<char>(0x04);
        const quint8 runBits     = iqAndMic ? quint8(0x02) : quint8(0x01);
        const quint8 watchdogBit = m_watchdogEnabled ? quint8(0x00) : quint8(0x80);
        pkt[3] = static_cast<char>(runBits | watchdogBit);
        return pkt;
    }

    // metisStopPacketForTest — compose the 64-byte RUNSTOP stop packet (run = 0).
    //
    // IMPORTANT: keep in sync with sendMetisStart() / sendMetisStop() in P1RadioConnection.cpp.
    //
    // Wire format: pkt[3] = watchdog_disable_bit (run bits = 0)
    //   0x00 if m_watchdogEnabled == true, 0x80 if false
    //
    // Source: Hermes-Lite2/gateware/rtl/dsopenhpsdr1.v:399-400
    //   watchdog_disable <= eth_data[7]; // Bit 7 can be used to disable watchdog
    QByteArray metisStopPacketForTest() const {
        QByteArray pkt(64, '\0');
        pkt[0] = static_cast<char>(0xEF);
        pkt[1] = static_cast<char>(0xFE);
        pkt[2] = static_cast<char>(0x04);
        const quint8 watchdogBit = m_watchdogEnabled ? quint8(0x00) : quint8(0x80);
        pkt[3] = static_cast<char>(watchdogBit);
        return pkt;
    }
#endif
};

} // namespace NereusSDR
