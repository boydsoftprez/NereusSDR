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

#include <memory>
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
    void setAntenna(int antennaIndex) override;

private slots:
    void onReadyRead();
    void onWatchdogTick();
    void onEp2PacerTick();
    void onReconnectTimeout();

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

    // Phase 3P-A: per-board codec chosen at applyBoardQuirks() time.
    // Null when m_caps is null (pre-connect) or env var
    // NEREUS_USE_LEGACY_P1_CODEC=1 forces legacy compose path.
    std::unique_ptr<IP1Codec> m_codec;
    bool m_useLegacyCodec{false};

    int     m_sampleRate{48000};
    int     m_activeRxCount{1};
    quint64 m_rxFreqHz[7]{};
    quint64 m_txFreqHz{0};
    bool    m_mox{false};
    int     m_antennaIdx{0};

    // Per-ADC state — initialized from HardwareProfile at connect time
    bool    m_dither[3]{true, true, true};
    bool    m_random[3]{true, true, true};
    bool    m_rxPreamp[3]{};
    int     m_stepAttn[3]{};      // per-ADC step attenuator (0-31)
    int     m_txStepAttn{0};

    // Alex filter state — computed from frequency
    quint8  m_alexHpfBits{0};     // Bank 10 C3: HPF select bits
    quint8  m_alexLpfBits{0};     // Bank 10 C4: LPF select bits

    // Hardware config from profile
    int     m_txDrive{0};
    bool    m_paEnabled{false};
    bool    m_duplex{true};
    bool    m_diversity{false};
    quint8  m_ocOutput{0};
    quint16 m_adcCtrl{0};          // ADC-to-DDC assignment bits

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
#endif
};

} // namespace NereusSDR
