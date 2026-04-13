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

    // Wire-format compose helpers — static, testable in isolation.
    // Each implementation cites its Thetis source line.
    static void composeEp2Frame(quint8 out[1032], quint32 seq, int ccAddress,
                                int sampleRate, bool mox) noexcept;
    static void composeCcBank0(quint8 out[5], int sampleRate, bool mox) noexcept;
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
    void sendCommandFrame();
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
    void hl2CheckBandwidthMonitor();
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
    int         m_reconnectAttempts{0};                         // how many retries so far this cycle
    static constexpr int kWatchdogTickMs       = 500;           // watchdog polling interval
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

#ifdef NEREUS_BUILD_TESTS
public:
    // Test-only helpers — allow unit tests to inject board caps without a live radio.
    void setBoardForTest(HPSDRHW board) { m_caps = &BoardCapsTable::forBoard(board); applyBoardQuirks(); }
    int currentAttenForTest() const { return m_atten; }
#endif
};

} // namespace NereusSDR
