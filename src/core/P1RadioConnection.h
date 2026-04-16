// src/core/P1RadioConnection.h
//
// Protocol 1 connection for Atlas/Hermes/HermesII/Angelia/Orion/HermesLite.
// Faithfully ported from Thetis networkproto1.c + NetworkIO.cs +
// mi0bot IoBoardHl2.cs + bandwidth_monitor.{c,h}.
//
// Follows the same pattern as P2RadioConnection (which ports network.c).
// Instances live on the Connection worker thread.
// Call init() after moveToThread() to create sockets/timers on the worker.

#pragma once

#include "RadioConnection.h"
#include "BoardCapabilities.h"
#include "HpsdrModel.h"

#include <QUdpSocket>
#include <QTimer>
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
    void onReconnectTimeout();

private:
    // --- Wire format (networkproto1.c) — implemented in Tasks 7 & 8 ---
    void sendMetisStart(bool iqAndMic);
    void sendMetisStop();
    // Send one ep2 command frame. `sub1TxBank=true` puts TX VFO (bank 1,
    // C0=0x02) on subframe 1; false puts RX1 VFO (bank 2, C0=0x04).
    // The steady-state watchdog and the initial priming burst alternate
    // between the two, matching Thetis's ForceCandCFrame(N) pattern in
    // networkproto1.c:134-139.
    void sendCommandFrame(bool sub1TxBank = false);
    // Match Thetis ForceCandCFrame(count): send `count` TX-freq ep2 frames
    // then `count` RX1-freq ep2 frames. Called before and after metis-start
    // on both the initial connect and watchdog-driven reconnect paths.
    void sendPrimingBurst(int countPerBank);
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

    // --- Per-board quirks — implemented in Tasks 11 & 12 ---
    void applyBoardQuirks();
    void hl2SendIoBoardTlv(const QByteArray& tlv);

    // HL2-specific helpers (mi0bot Hermes-Lite branch, Task 12).
    // hl2SendIoBoardInit — issues I2C register reads at startup to detect the
    //   HL2 I/O board and latch its hardware version.
    //   Source: mi0bot IoBoardHl2.cs:129-145 IOBoard.readRequest() —
    //     I2C reads on bus 1 at addr 0x41 (HW version) and 0x1d (registers).
    //     Full I2C init sequence is deferred (TODO(3I-T12)); the standard
    //     NetworkIO start path already handles HL2 ep2 init.
    void hl2SendIoBoardInit();

    // hl2CheckBandwidthMonitor — detects HL2 LAN PHY throttling via ep6
    //   sequence-gap heuristic; called from onWatchdogTick when hasBandwidthMonitor.
    //   Source: mi0bot bandwidth_monitor.{c,h} (MW0LGE) — original is a byte-rate
    //     meter (GetInboundBps/GetOutboundBps) using Windows InterlockedAdd64 and
    //     GetTickCount64; NereusSDR uses an ep6 sequence-gap approach instead
    //     (the byte-rate variant requires platform-specific atomics — TODO(3I-T12)).
    void hl2CheckBandwidthMonitor();
    bool hl2IsThrottled() const { return m_hl2Throttled; }

    void checkFirmwareMinimum(int fw);

    // --- State ---
    QUdpSocket* m_socket{nullptr};
    QTimer*     m_watchdogTimer{nullptr};
    QTimer*     m_reconnectTimer{nullptr};

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
    // 25 ms (= 40 fps) is enough to keep the radio's ep2 command pipe fed
    // without matching Thetis's full ~368 fps audio-drain cadence. Each tick
    // sends one ep2 command frame, alternating the subframe-1 bank between
    // TX VFO and RX1 VFO so both frequency banks are refreshed every ~50 ms.
    // The previous 500 ms interval was a keep-alive tick, not a command
    // stream, which left the radio's DDC stuck in its pre-primed idle state.
    static constexpr int kWatchdogTickMs       = 25;            // watchdog + ep2 command cadence
    static constexpr int kWatchdogSilenceMs    = 2000;          // silence → Error threshold
    static constexpr int kReconnectIntervalMs  = 5000;          // delay between retry attempts
    static constexpr int kMaxReconnectAttempts = 3;             // max retries before staying in Error

    quint32 m_epSendSeq{0};
    quint32 m_epRecvSeqExpected{0};
    int     m_ccRoundRobinIdx{0};

    int     m_sampleRate{48000};
    int     m_activeRxCount{1};
    quint64 m_rxFreqHz[7]{};
    quint64 m_txFreqHz{0};
    int     m_atten{0};
    bool    m_preamp{false};
    bool    m_mox{false};
    int     m_antennaIdx{0};

    const BoardCapabilities* m_caps{nullptr};

    // --- HL2 bandwidth monitor state (Task 12) ---
    // Source: mi0bot bandwidth_monitor.{c,h} — adapted to ep6 sequence-gap heuristic.
    bool      m_hl2Throttled{false};
    int       m_hl2ThrottleCount{0};
    QDateTime m_hl2LastThrottleTick;

#ifdef NEREUS_BUILD_TESTS
public:
    // Test-only helpers — allow unit tests to inject board caps without a live radio.
    void setBoardForTest(HPSDRHW board) { m_caps = &BoardCapsTable::forBoard(board); applyBoardQuirks(); }
    int currentAttenForTest() const { return m_atten; }
    bool hl2ThrottledForTest() const { return m_hl2Throttled; }
#endif
};

} // namespace NereusSDR
