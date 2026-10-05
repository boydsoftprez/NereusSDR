// no-port-check: NereusSDR-original.
// Transmit DSP off the event loop (R-R3-39, Task 32): with the transmit lane
// set, the TX channel's WDSP calls (keying, tune, two-tone, PureSignal, its
// meters) run on the lane; keying keeps its rf_delay order (MOX and relay,
// then the channel, then the RF gate); twenty key and unkey cycles make no
// WDSP call on the event loop and leave its 10 ms timer running; setters
// posted before a rebuild never run; and the unkey drain is measured with
// and without microphone blocks arriving.
//
// The TCI transmit resampler (R-R3-39 follow-up): it is freed on the
// transmit lane at every teardown (a TCI holder's disconnect, the TCI
// server's stop, a rate change, the channel's rebuild, destroy and the
// engine's shutdown), and the live count ends at zero.
//
// Modification history: 2026-09-27 J.J. Boyd (KG4VCF), AI-assisted via
// OpenAI Codex: retain idle/key-call scheduling evidence without relaxing bounds.
// 2026-09-30 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code: the
// keyed phase's idle reference also ticks on its own thread while the keys
// run, so a neighbour's load that starts after the idle phase is in both.
//
// REALTIME: the timer-gap bound is wall-clock time while a real TX channel
// and a real RX channel run 200 ms blocks and feeder threads drive them.
#include <QtTest/QtTest>
#include "RealtimeTestLoad.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QUrl>
#include <QWebSocket>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <numbers>
#include <thread>
#include <vector>

#ifdef Q_OS_MAC
#include <pthread/qos.h>
#endif

#include "core/DspControlThread.h"
#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "core/RadioConnection.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/TwoToneController.h"
#include "core/TxAnalyzer.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/TciBinaryFrame.h"
#include "core/TciServer.h"
#include "core/WdspThreadCheck.h"
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"
#include "core/wdsp_api.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kTxId = WdspEngine::kTxChannelId;
constexpr int kRxId = 0;
constexpr int kRateHz = 48000;
constexpr int kTxInSize = 64;
constexpr int kSlowBlockUs = 200000;
constexpr int kTimerIntervalMs = 10;
constexpr double kMaxTimerGapMs = 25.0;
// The keyed phase is judged against the idle phase the same run measured:
// its worst gap may exceed the idle worst gap by at most this much, the
// allowance the 25 ms bound grants over the 10 ms tick (JJ, 2026-09-28).
constexpr double kKeyedOverIdleAllowanceMs = kMaxTimerGapMs - kTimerIntervalMs;
constexpr int kKeyCycles = 20;
constexpr double kRfDelayMs = 30.0;   // MoxController's rf_delay default
// MoxController's rf_delay is a coarse QTimer; Qt rounds a coarse deadline
// under 50 ms to an even millisecond, so it can fire up to 1 ms early. The
// order is checked against txReady itself; the time from the key allows
// that rounding and nothing more.
constexpr double kCoarseTimerRoundingMs = 1.0;
// The first OpenChannel in a process plans its FFTs with no wisdom: about
// 30 s on a quiet machine, over 100 s under a heavy build.
constexpr int kLaneIdleTimeoutMs = 600000;

double msBetween(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// What happened, when, from any thread.
class TimedLog {
public:
    void add(const QString& what)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.push_back({what, Clock::now()});
    }
    // The time of the first entry named `what` at or after `from`, or
    // nothing.
    std::optional<Clock::time_point> first(const QString& what,
                                           Clock::time_point from = {}) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& e : m_entries) {
            if (e.what == what && e.at >= from) {
                return e.at;
            }
        }
        return std::nullopt;
    }
    QStringList namesSince(Clock::time_point from) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        QStringList out;
        for (const auto& e : m_entries) {
            if (e.at >= from) {
                out << e.what;
            }
        }
        return out;
    }

private:
    struct Entry {
        QString what;
        Clock::time_point at;
    };
    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
};

// WDSP's caller hook, recording the transmit channel's SetChannelState
// calls and (while armed) its lock entries made on the lane.
TimedLog* g_log = nullptr;
std::atomic<bool> g_recordTxLocks{false};

void recordingHook(int channel, int kind)
{
    if (g_log == nullptr || channel != kTxId) {
        return;
    }
    if (kind == kWdspCallerSetChannelState) {
        g_log->add(QStringLiteral("SetChannelState"));
    } else if (kind == kWdspCallerEnterCs && g_recordTxLocks.load()) {
        g_log->add(QStringLiteral("EnterCs"));
    }
}

class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockConnection(TimedLog* log, QObject* parent = nullptr)
        : RadioConnection(parent), m_log(log)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override { m_txBlocks.fetch_add(1); }
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool on) override
    {
        if (m_log) {
            m_log->add(on ? QStringLiteral("MOX on") : QStringLiteral("MOX off"));
        }
    }
    void setTrxRelay(bool on) override
    {
        if (m_log) {
            m_log->add(on ? QStringLiteral("relay on") : QStringLiteral("relay off"));
        }
    }
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}

    int txBlocks() const { return m_txBlocks.load(); }

private:
    TimedLog* m_log{nullptr};
    std::atomic<int> m_txBlocks{0};
};

// WDSP's analyzer table (third_party/wdsp/src/analyzer.c: DP pdisp[]).
// XCreateAnalyzer fills a slot; Spectrum0 dereferences it unchecked.
extern "C" {
extern void* pdisp[];
}

bool wdspDisplayExists(int disp)
{
    return pdisp[disp] != nullptr;
}

// An idle event loop on its own thread, ticking every kTimerIntervalMs at the
// calling thread's QoS class (macOS) until stopped: the idle reference that
// runs at the same time as the loop it is compared with, so both see the
// same machine. Nothing else runs on it.
class IdleTwinLoop {
public:
    IdleTwinLoop()
    {
#ifdef Q_OS_MAC
        qos_class_t qos = QOS_CLASS_UNSPECIFIED;
        int relative = 0;
        pthread_get_qos_class_np(pthread_self(), &qos, &relative);
#endif
        m_thread.reset(QThread::create([this
#ifdef Q_OS_MAC
                                        , qos, relative
#endif
        ] {
#ifdef Q_OS_MAC
            if (qos != QOS_CLASS_UNSPECIFIED) {
                pthread_set_qos_class_self_np(qos, relative);
            }
#endif
            QEventLoop loop;
            QTimer ticker;
            ticker.setTimerType(Qt::PreciseTimer);
            ticker.setInterval(kTimerIntervalMs);
            Clock::time_point previous = Clock::now();
            QObject::connect(&ticker, &QTimer::timeout, &loop, [&] {
                const Clock::time_point now = Clock::now();
                if (m_ticks > 0) {
                    m_worstGapMs = std::max(m_worstGapMs, msBetween(previous, now));
                }
                previous = now;
                ++m_ticks;
                if (m_quit.load()) {
                    loop.quit();
                }
            });
            ticker.start();
            m_running.store(true);
            loop.exec();
        }));
        m_thread->start();
        while (!m_running.load()) {
            std::this_thread::yield();
        }
    }
    ~IdleTwinLoop() { stop(); }
    void stop()
    {
        m_quit.store(true);
        m_thread->wait();
    }
    // Valid after stop().
    double worstGapMs() const { return m_worstGapMs; }
    int ticks() const { return m_ticks; }

private:
    std::unique_ptr<QThread> m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_quit{false};
    double m_worstGapMs{0.0};
    int m_ticks{0};
};

// A thread that calls `tick` about once a millisecond until stopped.
class Feeder {
public:
    explicit Feeder(std::function<void()> tick) : m_tick(std::move(tick))
    {
        m_thread = std::thread([this] {
            while (!m_quit.load()) {
                m_tick();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    ~Feeder() { stop(); }
    void stop()
    {
        m_quit.store(true);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

private:
    std::function<void()> m_tick;
    std::atomic<bool> m_quit{false};
    std::thread m_thread;
};

// The TX worker's per-block calls, as TxWorkerThread::dispatchOneBlock
// makes them: a 1 kHz tone into DEXP, then fexchange0.
std::function<void()> txBlockTick(TxChannel* tx)
{
    auto block = std::make_shared<std::vector<double>>(2 * kTxInSize, 0.0);
    auto n = std::make_shared<long>(0);
    return [tx, block, n]() {
        for (int i = 0; i < kTxInSize; ++i, ++*n) {
            (*block)[static_cast<std::size_t>(2 * i)] =
                0.1 * std::sin(2.0 * std::numbers::pi * 1000.0
                               * static_cast<double>(*n) / kRateHz);
        }
        tx->pumpDexp(block->data());
        tx->driveOneTxBlockFromInterleaved(block->data());
    };
}

std::function<void()> rxBlockTick(RxChannel* rx)
{
    auto n = std::make_shared<long>(0);
    return [rx, n]() {
        const int inSize = rx->bufferSize();
        std::vector<float> inI(static_cast<std::size_t>(inSize));
        std::vector<float> inQ(static_cast<std::size_t>(inSize), 0.0f);
        std::vector<float> outI(static_cast<std::size_t>(inSize), 0.0f);
        std::vector<float> outQ(static_cast<std::size_t>(inSize), 0.0f);
        for (int i = 0; i < inSize; ++i, ++*n) {
            inI[static_cast<std::size_t>(i)] = static_cast<float>(
                0.01 * std::cos(2.0 * std::numbers::pi * 1000.0
                                * static_cast<double>(*n) / kRateHz));
        }
        rx->processIq(inI.data(), inQ.data(), outI.data(), outQ.data(), inSize, 64);
    };
}

} // namespace

class TestDspControlTransmit : public QObject {
    Q_OBJECT

    // A local RadioModel (with both lanes) whose TX channel is real and
    // wired to MoxController as the connect path wires it, plus a real RX
    // channel, a mock connection and PureSignal.
    struct Rig {
        std::unique_ptr<RadioModel> model;
        std::unique_ptr<MockConnection> conn;
        // The TX siphon feeds this analyzer (disp 5) once the channel runs,
        // as the desktop's does; its calls run on the transmit lane.
        std::unique_ptr<TxAnalyzer> analyzer;
        WdspEngine* engine{nullptr};
        TxChannel* tx{nullptr};
        RxChannel* rx{nullptr};
        ~Rig() { tearDownRig(*this); }
    };

    static bool buildRig(Rig& rig, TimedLog* log)
    {
        rig.model = std::make_unique<RadioModel>();
        RadioModel& model = *rig.model;
        if (model.transmitLane() == nullptr || model.receiveLane() == nullptr) {
            return false;
        }
        model.setCapsForTest(/*hasAlex=*/false);
        rig.conn = std::make_unique<MockConnection>(log);
        model.injectConnectionForTest(rig.conn.get());
        model.setTuneOffSettleMsForTest(0);
        model.addSlice();
        if (SliceModel* slice = model.activeSlice()) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14'200'000.0);
        }

        rig.analyzer = std::make_unique<TxAnalyzer>(TxAnalyzer::kTxDispId, nullptr,
                                                    model.transmitLane());
        rig.analyzer->setSampleRate(96000.0);
        rig.analyzer->setOutputFps(15);
        rig.analyzer->start();

        rig.engine = model.wdspEngine();
        rig.engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        rig.rx = rig.engine->createRxChannel(kRxId, bufferSizeForRate(kRateHz), 4096,
                                             kRateHz, kRateHz, kRateHz);
        rig.tx = rig.engine->createTxChannel(kTxId, kTxInSize,
                                             WdspEngine::kTxDspBufferSize, kRateHz,
                                             WdspEngine::kTxDspSampleRate, kRateHz);
        if (rig.rx == nullptr || rig.tx == nullptr) {
            return false;
        }
        if (!model.waitForReceiveLaneForTest(kLaneIdleTimeoutMs)
            || !model.waitForTransmitLaneForTest(kLaneIdleTimeoutMs)) {
            return false;
        }
        // The siphon hands display 5 one DSP block per call and Spectrum0
        // reads bf_sz samples from it, so the analyzer takes the channel's
        // block size before any block, as the desktop's MOX edge sets it
        // (without it bf_sz is the FFT size and reads past the siphon).
        rig.analyzer->setBlockSize(rig.tx->dspBlockFrames());
        rig.rx->setActive(true);
        rig.tx->setConnection(rig.conn.get());
        model.injectTxChannelForTest(rig.tx);
        model.wireTxChannelKeyingForTest();
        return model.waitForReceiveLaneForTest(kLaneIdleTimeoutMs)
            && model.waitForTransmitLaneForTest(kLaneIdleTimeoutMs);
    }

    static void tearDownRig(Rig& rig)
    {
        WDSPSetTestBlockDelayUs(kTxId, 0);
        WDSPSetTestBlockDelayUs(kRxId, 0);
        if (!rig.model) {
            rig.conn.reset();
            return;
        }
        RadioModel& model = *rig.model;
        if (rig.tx) {
            rig.tx->setConnection(nullptr);
        }
        // Retire PureSignal's borrowed channel before WDSP destroys it.
        if (PureSignal* pureSignal = model.pureSignal()) {
            pureSignal->setTxChannel(nullptr);
        }
        model.injectTxChannelForTest(nullptr);
        if (rig.engine) {
            rig.engine->shutdown();
        }
        // After the TX channel is closed: nothing feeds the analyzer now.
        rig.analyzer.reset();
        model.injectConnectionForTest(nullptr);
        rig.model.reset();
        rig.conn.reset();
    }

private slots:
    void cleanup()
    {
        WDSPSetCallerCheckHook(nullptr);
        g_log = nullptr;
        g_recordTxLocks.store(false);
        RealtimeTestLoad::printLoadAverageIfFailed();
    }

#ifdef HAVE_WDSP
    // Keying keeps the rf_delay order with 200 ms blocks on both lanes'
    // channels: MOX and the relay first, then (at least rf_delay after the
    // key) the lane switches the TX channel on, then the RF gate opens.
    void keyingKeepsTheRfDelayOrder()
    {
        TimedLog log;
        Rig rig;
        QVERIFY2(buildRig(rig, &log), "the rig did not come up");

        Feeder rxFeeder(rxBlockTick(rig.rx));
        Feeder txFeeder(txBlockTick(rig.tx));
        WDSPSetTestBlockDelayUs(kRxId, kSlowBlockUs);
        WDSPSetTestBlockDelayUs(kTxId, kSlowBlockUs);
        rig.tx->setRfGateObserverForTest([&log](bool open) {
            log.add(open ? QStringLiteral("gate open") : QStringLiteral("gate closed"));
        });
        g_log = &log;
        WDSPSetCallerCheckHook(&recordingHook);

        connect(rig.model->moxController(), &MoxController::txReady, this,
                [&log]() { log.add(QStringLiteral("txReady")); });
        const Clock::time_point keyAt = Clock::now();
        rig.model->moxController()->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.tx->isRunning(), kLaneIdleTimeoutMs);
        WDSPSetCallerCheckHook(nullptr);

        const auto moxOn = log.first(QStringLiteral("MOX on"), keyAt);
        const auto relayOn = log.first(QStringLiteral("relay on"), keyAt);
        const auto txReady = log.first(QStringLiteral("txReady"), keyAt);
        const auto channelOn = log.first(QStringLiteral("SetChannelState"), keyAt);
        const auto gateOpen = log.first(QStringLiteral("gate open"), keyAt);
        qInfo("key: MOX on +%.2f ms, relay on +%.2f ms, txReady +%.2f ms, "
              "SetChannelState(tx, 1, 0) +%.2f ms, RF gate open +%.2f ms (rf_delay %.0f ms)",
              moxOn ? msBetween(keyAt, *moxOn) : -1.0,
              relayOn ? msBetween(keyAt, *relayOn) : -1.0,
              txReady ? msBetween(keyAt, *txReady) : -1.0,
              channelOn ? msBetween(keyAt, *channelOn) : -1.0,
              gateOpen ? msBetween(keyAt, *gateOpen) : -1.0, kRfDelayMs);
        QVERIFY(moxOn && relayOn && txReady && channelOn && gateOpen);
        QVERIFY2(*moxOn <= *relayOn, "the relay came before MOX");
        QVERIFY2(*relayOn < *channelOn, "the channel came on before the relay");
        QVERIFY2(*txReady <= *channelOn, "the channel came on before rf_delay ended");
        QVERIFY2(msBetween(keyAt, *channelOn) >= kRfDelayMs - kCoarseTimerRoundingMs,
                 "the channel came on inside rf_delay");
        QVERIFY2(*channelOn <= *gateOpen, "the RF gate opened before the channel");

        // Unkey (Task 33): the TX channel drains with the gate open, then
        // the lane closes the gate.
        rig.model->moxController()->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.tx->isRunning(), 5000);
        QVERIFY(rig.model->waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
        txFeeder.stop();
        rxFeeder.stop();
    }

    // R-R3-39, R-IOS-03: a Core keys without crashing. createTxChannel
    // points the TX siphon at analyzer display 5 (TXASetSipMode 1,
    // TXASetSipDisplay 5), and only the desktop's TxAnalyzer created it;
    // nereusd has no MainWindow, so its first keyed block dereferenced
    // pdisp[5] == NULL in Spectrum0 (analyzer.c). The model DaemonApp
    // builds now has the desktop's TX analyzer: display 5 exists before the
    // first block, the analyzer runs while keyed, and keyed blocks go
    // through the siphon without a crash.
    void aCoreKeysWithoutCrashing()
    {
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(DaemonConfig::defaults()));
        RadioModel* model = app.radioModelForTest();
        QVERIFY(model != nullptr);
        QVERIFY(model->transmitLane() != nullptr);
        QVERIFY(model->waitForTransmitLaneForTest(kLaneIdleTimeoutMs));

        // nereusd refuses transmit today (a receive-only station policy,
        // which remote transmit lifts); lifted here to key the Core's own
        // model the way a remote key will.
        model->setReceiveOnlyStationPolicy(false);
        model->setCapsForTest(/*hasAlex=*/false);
        MockConnection conn(nullptr);
        model->injectConnectionForTest(&conn);
        model->setTuneOffSettleMsForTest(0);
        if (SliceModel* slice = model->activeSlice()) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14'200'000.0);
        }
        WdspEngine* engine = model->wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        TxChannel* tx = engine->createTxChannel(kTxId, kTxInSize,
                                                WdspEngine::kTxDspBufferSize, kRateHz,
                                                WdspEngine::kTxDspSampleRate, kRateHz);
        QVERIFY(tx != nullptr);
        QVERIFY(model->waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
        tx->setConnection(&conn);
        model->injectTxChannelForTest(tx);
        model->wireTxChannelKeyingForTest();
        QVERIFY(model->waitForTransmitLaneForTest(kLaneIdleTimeoutMs));

        // Keyed blocks: each is recorded with whether display 5 existed as
        // it went through the siphon.
        std::atomic<int> keyedBlocks{0};
        std::atomic<int> keyedBlocksWithoutDisplay{0};
        std::function<void()> block = txBlockTick(tx);
        Feeder feeder([&]() {
            const bool keyed = tx->isRunning();
            const bool display = wdspDisplayExists(TxAnalyzer::kTxDispId);
            block();
            if (keyed) {
                keyedBlocks.fetch_add(1);
                if (!display) {
                    keyedBlocksWithoutDisplay.fetch_add(1);
                }
            }
        });

        QString rejected;
        connect(model->moxController(), &MoxController::moxRejected, this,
                [&rejected](const QString& reason) { rejected = reason; });
        model->moxController()->setMox(true);
        QVERIFY2(rejected.isEmpty(), qPrintable(rejected));
        QTRY_VERIFY_WITH_TIMEOUT(tx->isRunning(), 30000);
        // About 32 input blocks of 64 samples at 48 kHz per 4096-sample DSP
        // block at 96 kHz: several DSP blocks, each through the siphon.
        QTRY_VERIFY_WITH_TIMEOUT(keyedBlocks.load() >= 400, 60000);
        QVERIFY(model->txAnalyzer() != nullptr);
        QVERIFY(model->txAnalyzer()->isRunning());
        QCOMPARE(model->txAnalyzer()->blockSize(), tx->dspBlockFrames());

        model->moxController()->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(!tx->isRunning(), 5000);
        // Task 33: the analyzer stops on hardwareFlipped(false), which now
        // follows the drain and mox_delay (Thetis's unkey order).
        QTRY_COMPARE_WITH_TIMEOUT(model->moxController()->state(), MoxState::Rx, 5000);
        QVERIFY(model->waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
        feeder.stop();
        QVERIFY(!model->txAnalyzer()->isRunning());
        QCOMPARE(keyedBlocksWithoutDisplay.load(), 0);

        tx->setConnection(nullptr);
        model->injectTxChannelForTest(nullptr);
        engine->shutdown();
        model->injectConnectionForTest(nullptr);
        app.stop();
        QVERIFY(app.radioModelForTest() == nullptr);
    }

    // Twenty key and unkey cycles (MOX, TUNE and two-tone in turn) with
    // PureSignal's 100 ms poll running and 200 ms blocks on both lanes'
    // channels: no WDSP call on the event loop, and its 10 ms timer never
    // gaps more than the larger of 25 ms and the same run's idle worst gap
    // plus 15 ms. Under heavy machine load the idle loop alone can miss
    // 25 ms (idle 31.19 ms, keyed 39.10 ms at load 234, key call 0.32 ms);
    // keying still must not add more than the 15 ms the bound allows over
    // the tick, so a keying stall of the 200 ms kind this guards still fails.
    // The idle worst gap is the idle phase's and that of an idle loop on its
    // own thread ticking through the keyed phase: a neighbour's load that
    // started after the idle phase stalled the keyed loop 39 to 44 ms before
    // any key (idle 15 ms, load 24), and the same onset during the idle
    // phase stalls it 25 to 36 ms; the twin loop sees such a load too, and a
    // stall on this thread's loop does not reach it.
    void keyCyclesLeaveTheEventLoopAlone()
    {
        TimedLog log;
        Rig rig;
        QVERIFY2(buildRig(rig, &log), "the rig did not come up");
        RadioModel& model = *rig.model;
        PureSignal* ps = model.installPureSignalForTest(rig.tx);
        QVERIFY(ps);
        TwoToneController* twoTone = model.twoToneController();
        QVERIFY(twoTone);
        twoTone->setTxChannel(rig.tx);
        twoTone->setPowerOn(true);
        twoTone->setSettleDelaysMs(20, 20);
        QVERIFY(model.waitForTransmitLaneForTest(kLaneIdleTimeoutMs));

        Feeder rxFeeder(rxBlockTick(rig.rx));
        Feeder txFeeder(txBlockTick(rig.tx));
        WDSPSetTestBlockDelayUs(kRxId, kSlowBlockUs);
        WDSPSetTestBlockDelayUs(kTxId, kSlowBlockUs);
        // Let both workers get into their slow blocks.
        QTest::qWait(3 * kSlowBlockUs / 1000);

        WdspThreadCheck::install(QThread::currentThread());

        // Measure idle scheduling under the same live load and slow DSP
        // workers; the keyed bound below is relative to it.
        double idleWorstGapMs = 0.0;
        int idleTicks = 0;
        {
            QEventLoop idleLoop;
            QTimer idleTicker;
            idleTicker.setTimerType(Qt::PreciseTimer);
            idleTicker.setInterval(kTimerIntervalMs);
            auto previous = Clock::now();
            connect(&idleTicker, &QTimer::timeout, &idleLoop, [&] {
                const auto now = Clock::now();
                if (idleTicks > 0) {
                    idleWorstGapMs = std::max(idleWorstGapMs, msBetween(previous, now));
                }
                previous = now;
                if (++idleTicks >= 555) {
                    idleLoop.quit();
                }
            });
            QTimer::singleShot(120000, &idleLoop, &QEventLoop::quit);
            idleTicker.start();
            idleLoop.exec();
        }
        double worstKeyCallMs = 0.0;
        double worstUnkeyCallMs = 0.0;
        constexpr int kKeyedTicks = 12;    // 120 ms keyed
        constexpr int kSettleTicks = 15;   // 150 ms between cycles
        int ticks = 0;
        int cycle = 0;
        int phaseTick = 0;
        bool keyed = false;
        int keysTaken = 0;
        double worstGapMs = 0.0;
        IdleTwinLoop idleTwin;
        Clock::time_point lastTick = Clock::now();
        QEventLoop loop;
        QTimer ticker;
        ticker.setTimerType(Qt::PreciseTimer);
        ticker.setInterval(kTimerIntervalMs);
        const auto key = [&](bool on) {
            const auto callStarted = Clock::now();
            switch (cycle % 3) {
            case 0:
                model.moxController()->setMox(on);
                break;
            case 1:
                model.setTune(on);
                break;
            default:
                twoTone->setActive(on);
                break;
            }
            double& maximum = on ? worstKeyCallMs : worstUnkeyCallMs;
            maximum = std::max(maximum, msBetween(callStarted, Clock::now()));
        };
        connect(&ticker, &QTimer::timeout, &loop, [&] {
            const Clock::time_point now = Clock::now();
            if (ticks > 0) {
                worstGapMs = std::max(worstGapMs, msBetween(lastTick, now));
            }
            lastTick = now;
            ++ticks;
            ++phaseTick;
            if (!keyed && phaseTick >= kSettleTicks) {
                if (cycle >= kKeyCycles) {
                    loop.quit();
                    return;
                }
                key(true);
                keyed = true;
                ++keysTaken;
                phaseTick = 0;
            } else if (keyed && phaseTick >= kKeyedTicks) {
                key(false);
                keyed = false;
                ++cycle;
                phaseTick = 0;
            }
        });
        QTimer deadline;
        deadline.setSingleShot(true);
        connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        deadline.start(120000);
        ticker.start();
        loop.exec();
        ticker.stop();
        idleTwin.stop();
        const quint64 eventLoopCalls = WdspThreadCheck::eventLoopEntries();
        WdspThreadCheck::uninstall();

        txFeeder.stop();
        rxFeeder.stop();
        WDSPSetTestBlockDelayUs(kRxId, 0);
        WDSPSetTestBlockDelayUs(kTxId, 0);
        QVERIFY(model.waitForTransmitLaneForTest(kLaneIdleTimeoutMs));

        const double idleReferenceMs = std::max(idleWorstGapMs, idleTwin.worstGapMs());
        const double keyedLimitMs =
            std::max(kMaxTimerGapMs, idleReferenceMs + kKeyedOverIdleAllowanceMs);
        qInfo("%d key and unkey cycles (MOX, TUNE, two-tone) with the PureSignal poll: "
              "%d ticks; worst %d ms timer gap %.2f ms (limit %.2f); TX blocks sent %d; "
              "WDSP calls on the event loop %llu",
              keysTaken, ticks, kTimerIntervalMs, worstGapMs, keyedLimitMs,
              rig.conn->txBlocks(), static_cast<unsigned long long>(eventLoopCalls));
        qInfo("Scheduling comparison: idle ticks %d, idle worst gap %.2f ms, "
              "idle twin ticks %d, twin worst gap %.2f ms, "
              "key call worst %.2f ms, unkey call worst %.2f ms",
              idleTicks, idleWorstGapMs, idleTwin.ticks(), idleTwin.worstGapMs(),
              worstKeyCallMs, worstUnkeyCallMs);
        QCOMPARE(idleTicks, 555);
        // The twin ticked through the keyed phase (about as often as the
        // keyed loop: it is idle).
        QVERIFY2(idleTwin.ticks() >= ticks / 2, qPrintable(QString::number(idleTwin.ticks())));
        QCOMPARE(keysTaken, kKeyCycles);
        QCOMPARE(eventLoopCalls, quint64(0));
        QVERIFY2(worstGapMs <= keyedLimitMs, "the event loop's 10 ms timer gapped");
        QVERIFY2(rig.conn->txBlocks() > 0, "no TX block reached the connection");
        QVERIFY(!rig.tx->isRunning());

        twoTone->setTxChannel(nullptr);
        // The coordinator outlives the channel in this rig (the connect
        // path resets it first); detach it before the channel closes.
        ps->setTxChannel(nullptr);
    }

    // R-R3-39: TCI transmit audio at a rate other than 48 kHz goes through
    // WDSP's float resampler (create_resampleFV, xresampleFV,
    // destroy_resampleFV). With TCI transmit audio flowing (24 kHz stereo,
    // 8 kHz mono, a rate change, then 48 kHz) and a cycle stop, none of it
    // runs on the event loop, and the ring holds the blocks in the order they
    // came: the 48 kHz block, which needs no resampler, is last.
    void tciTransmitAudioLeavesTheEventLoopAlone()
    {
        TimedLog log;
        Rig rig;
        QVERIFY2(buildRig(rig, &log), "the rig did not come up");
        RadioModel& model = *rig.model;

        auto tone = [](int frames, int channels, double hz, int rate) {
            QByteArray bytes(frames * channels * static_cast<int>(sizeof(float)), '\0');
            auto* out = reinterpret_cast<float*>(bytes.data());
            for (int f = 0; f < frames; ++f) {
                const auto v = static_cast<float>(
                    0.1 * std::sin(2.0 * std::numbers::pi * hz * f / rate));
                for (int c = 0; c < channels; ++c) {
                    out[f * channels + c] = v;
                }
            }
            return bytes;
        };
        constexpr int kMarkerFrames = 256;
        QByteArray marker(kMarkerFrames * static_cast<int>(sizeof(float)), '\0');
        auto* markerValues = reinterpret_cast<float*>(marker.data());
        for (int f = 0; f < kMarkerFrames; ++f) {
            markerValues[f] = 0.25f + 0.0005f * static_cast<float>(f);
        }

        WdspThreadCheck::install(QThread::currentThread());
        rig.tx->feedTxAudioFromTci(tone(1024, 2, 1000.0, 24000), 1024, 2, 24000);
        rig.tx->feedTxAudioFromTci(tone(512, 1, 700.0, 8000), 512, 1, 8000);
        rig.tx->feedTxAudioFromTci(tone(512, 1, 700.0, 8000), 512, 1, 8000);
        rig.tx->feedTxAudioFromTci(marker, kMarkerFrames, 1, 48000);
        QVERIFY(model.waitForTransmitLaneForTest(kLaneIdleTimeoutMs));

        std::vector<float> ring;
        std::array<float, 512> chunk{};
        for (int got = rig.tx->pullTciAudio(chunk.data(), static_cast<int>(chunk.size()));
             got > 0;
             got = rig.tx->pullTciAudio(chunk.data(), static_cast<int>(chunk.size()))) {
            ring.insert(ring.end(), chunk.begin(), chunk.begin() + got);
        }
        // The resampled blocks produced samples, and the 48 kHz block came
        // after them.
        QVERIFY2(ring.size() > static_cast<std::size_t>(kMarkerFrames),
                 qPrintable(QString::number(ring.size())));
        for (int f = 0; f < kMarkerFrames; ++f) {
            QCOMPARE(ring[ring.size() - kMarkerFrames + static_cast<std::size_t>(f)],
                     markerValues[f]);
        }

        // A cycle stop with a resampler live: the ring is cleared and the
        // resampler destroyed, still off the event loop.
        rig.tx->feedTxAudioFromTci(tone(1024, 2, 1000.0, 24000), 1024, 2, 24000);
        rig.tx->clearTciAudio();
        QVERIFY(model.waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
        QCOMPARE(rig.tx->pullTciAudio(chunk.data(), static_cast<int>(chunk.size())), 0);
        // A new cycle at 48 kHz reaches the ring.
        rig.tx->feedTxAudioFromTci(marker, kMarkerFrames, 1, 48000);
        QVERIFY(model.waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
        QCOMPARE(rig.tx->pullTciAudio(chunk.data(), static_cast<int>(chunk.size())),
                 kMarkerFrames);
        QCOMPARE(chunk[0], markerValues[0]);

        const quint64 eventLoopCalls = WdspThreadCheck::eventLoopEntries();
        WdspThreadCheck::uninstall();
        QCOMPARE(eventLoopCalls, quint64{0});
    }

    // R-R3-39: the TCI transmit resampler never leaks. A block at a rate
    // other than 48 kHz makes one on the transmit lane; every teardown frees
    // it there: the TX audio holder's disconnect, a rate change (the old one
    // goes, one new one lives), the TCI server's stop, the channel's
    // rebuild and destroy, and the engine's shutdown.
    void tciResamplerIsFreedOnEveryTeardown()
    {
        const auto txFrame = [](int rate) {
            std::vector<float> samples(512, 0.1f);   // 256 stereo frames
            return TciBinaryFrame::buildStreamPayload(
                0, rate, static_cast<int>(TciSampleType::Float32),
                static_cast<int>(samples.size()),
                static_cast<int>(TciStreamType::TxAudioStream), 2, samples.data());
        };
        const auto claimTxAudio = [](QWebSocket& app, TciServer& server) {
            QSignalSpy connected(&app, &QWebSocket::connected);
            app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
            if (!connected.wait(2000)) {
                return false;
            }
            app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
            return QTest::qWaitFor([&server]() { return server.activeTxClientCount() == 1; },
                                   3000);
        };
        QCOMPARE(TxChannel::liveTciResamplersForTest(), 0);
        {
            TimedLog log;
            Rig rig;
            QVERIFY2(buildRig(rig, &log), "the rig did not come up");
            RadioModel& model = *rig.model;
            // The app holds the TX audio while nothing is keyed. Since the
            // gaps plan's Task 7 follow-up, a trx that keys nothing gives
            // the TX audio back unless its TCI level is still held, so the
            // trx is held off by a manual key (Thetis _manual_mox: PollPTT
            // neither keys nor drops the level) rather than refused.
            model.moxController()->setManualKey(true);
            TciServer server(&model);
            QVERIFY(server.start(0));

            // A holder's disconnect mid-cycle, after a rate change.
            {
                QWebSocket app;
                QVERIFY(claimTxAudio(app, server));
                app.sendBinaryMessage(txFrame(24000));
                QTRY_COMPARE_WITH_TIMEOUT(TxChannel::liveTciResamplersForTest(), 1, 3000);
                app.sendBinaryMessage(txFrame(8000));   // a rate change
                QTest::qWait(100);
                QVERIFY(model.waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
                QCOMPARE(TxChannel::liveTciResamplersForTest(), 1);
                app.close();
                QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 0, 3000);
                QVERIFY(model.waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
                QCOMPARE(TxChannel::liveTciResamplersForTest(), 0);
            }
            // The server's stop with a holder mid-cycle.
            {
                QWebSocket app;
                QVERIFY(claimTxAudio(app, server));
                app.sendBinaryMessage(txFrame(12000));
                QTRY_COMPARE_WITH_TIMEOUT(TxChannel::liveTciResamplersForTest(), 1, 3000);
                server.stop();
                QVERIFY(model.waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
                QCOMPARE(TxChannel::liveTciResamplersForTest(), 0);
            }
        }
        QCOMPARE(TxChannel::liveTciResamplersForTest(), 0);

        // The channel's own teardowns, on a transmit lane.
        DspControlThread lane(DspLane::Transmit);
        lane.start();
        {
            WdspEngine engine;
            engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
            engine.setTransmitLane(&lane);
            const auto create = [&engine]() {
                return engine.createTxChannel(kTxId, kTxInSize, WdspEngine::kTxDspBufferSize,
                                              kRateHz, WdspEngine::kTxDspSampleRate, kRateHz);
            };
            const auto feed24k = [](TxChannel* tx) {
                QByteArray block(256 * 2 * static_cast<int>(sizeof(float)), '\0');
                tx->feedTxAudioFromTci(block, 256, 2, 24000);
            };
            TxChannel* tx = create();
            QVERIFY(tx);
            feed24k(tx);
            QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
            QCOMPARE(TxChannel::liveTciResamplersForTest(), 1);

            // Rebuild mid-cycle: the old wrapper's resampler goes.
            ChannelConfig cfg;
            cfg.bufferSize = kTxInSize;
            cfg.filterSize = WdspEngine::kTxDspBufferSize;
            cfg.sampleRate = kRateHz;
            QVERIFY(engine.rebuildTxChannel(kTxId, cfg) >= 0);
            QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
            QCOMPARE(TxChannel::liveTciResamplersForTest(), 0);

            // Destroy mid-cycle.
            tx = engine.txChannel(kTxId);
            QVERIFY(tx);
            feed24k(tx);
            QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
            QCOMPARE(TxChannel::liveTciResamplersForTest(), 1);
            engine.destroyTxChannel(kTxId);
            QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
            QCOMPARE(TxChannel::liveTciResamplersForTest(), 0);

            // Shutdown mid-cycle (the engine goes with the next brace).
            tx = create();
            QVERIFY(tx);
            QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
            feed24k(tx);
            QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
            QCOMPARE(TxChannel::liveTciResamplersForTest(), 1);
        }
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        QCOMPARE(TxChannel::liveTciResamplersForTest(), 0);
        lane.stop();
    }

    // The rebuild's generation check: a setter still queued for the old
    // wrapper when the rebuild starts never runs, nor does one posted
    // through the old pointer afterwards; the new wrapper's setters do.
    // Settings apply with no microphone audio arriving (no feeder here).
    void settersQueuedBeforeARebuildAreSkipped()
    {
        DspControlThread lane(DspLane::Transmit);
        lane.start();
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        engine.setTransmitLane(&lane);
        TxChannel* tx = engine.createTxChannel(kTxId, kTxInSize,
                                               WdspEngine::kTxDspBufferSize, kRateHz,
                                               WdspEngine::kTxDspSampleRate, kRateHz);
        QVERIFY(tx);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        QVERIFY(tx->isWdspReady());

        const auto readOnLane = [&lane](TxChannel* channel, TxChannel::Stage stage) {
            auto value = std::make_shared<std::atomic<bool>>(false);
            lane.post([channel, stage, value]() { value->store(channel->stageRunning(stage)); });
            lane.waitIdleForTest(kLaneIdleTimeoutMs);
            return value->load();
        };

        // A setter applies on the lane with no mic block arriving.
        tx->setStageRunning(TxChannel::Stage::Eqp, true);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        QVERIFY(readOnLane(tx, TxChannel::Stage::Eqp));
        QVERIFY2(tx->stageRunning(TxChannel::Stage::Eqp), "the stage cache missed the lane's value");
        tx->setStageRunning(TxChannel::Stage::Eqp, false);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));

        TimedLog log;
        g_log = &log;
        WDSPSetCallerCheckHook(&recordingHook);

        // Control: a setter queued behind a held lane runs once released.
        std::mutex gateMutex;
        std::condition_variable gateCv;
        bool released = false;
        const auto holdLane = [&]() {
            lane.post([&]() {
                std::unique_lock<std::mutex> lk(gateMutex);
                gateCv.wait(lk, [&] { return released; });
            });
        };
        const auto releaseLane = [&]() {
            {
                std::lock_guard<std::mutex> lk(gateMutex);
                released = true;
            }
            gateCv.notify_all();
        };
        holdLane();
        tx->setStageRunning(TxChannel::Stage::Eqp, true);
        Clock::time_point armedAt = Clock::now();
        g_recordTxLocks.store(true);
        releaseLane();
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        g_recordTxLocks.store(false);
        const QStringList controlCalls = log.namesSince(armedAt);
        QVERIFY2(controlCalls.contains(QStringLiteral("EnterCs")),
                 "the control setter made no WDSP call on the lane");
        tx->setStageRunning(TxChannel::Stage::Eqp, false);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));

        // Now: hold the lane, queue setters for the old wrapper, rebuild,
        // post through the old pointer, release.
        released = false;
        holdLane();
        tx->setStageRunning(TxChannel::Stage::Eqp, true);
        tx->setTxPostGenTTFreq1(1234.0);
        ChannelConfig cfg;
        cfg.bufferSize = kTxInSize;
        cfg.filterSize = WdspEngine::kTxDspBufferSize;
        cfg.sampleRate = kRateHz;
        QVERIFY(engine.rebuildTxChannel(kTxId, cfg) >= 0);
        TxChannel* rebuilt = engine.txChannel(kTxId);
        QVERIFY(rebuilt && rebuilt != tx);
        QVERIFY(tx->isRetired());
        tx->setTxPostGenRun(true);   // through the stale pointer
        rebuilt->setStageRunning(TxChannel::Stage::Compressor, true);
        armedAt = Clock::now();
        g_recordTxLocks.store(true);
        releaseLane();
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        g_recordTxLocks.store(false);
        WDSPSetCallerCheckHook(nullptr);

        // Nothing touched the channel between the release and the rebuild's
        // close (its first call is SetChannelState).
        const QStringList calls = log.namesSince(armedAt);
        qInfo("after release: %s", qPrintable(calls.mid(0, 6).join(QStringLiteral(", "))));
        QVERIFY(!calls.isEmpty());
        QCOMPARE(calls.first(), QStringLiteral("SetChannelState"));

        QVERIFY(!readOnLane(rebuilt, TxChannel::Stage::Eqp));
        QVERIFY(!readOnLane(rebuilt, TxChannel::Stage::Gen1));
        QVERIFY(readOnLane(rebuilt, TxChannel::Stage::Compressor));
        QVERIFY(rebuilt->isWdspReady());

        engine.shutdown();
        engine.setTransmitLane(nullptr);
        lane.stop();
    }

    // How long the unkey drain (SetChannelState with dmode=1) takes on the
    // lane. Task 33: the normal unkey keeps the RF gate open through the
    // drain (Thetis's order), so with microphone blocks arriving the worker
    // keeps calling fexchange0 and the drain finishes; the emergency stop
    // closes the gate first and the drain waits out WDSP's timeout, as it
    // does with no blocks. The numbers go to the task's ledger.
    void unkeyDrainIsMeasured()
    {
        DspControlThread lane(DspLane::Transmit);
        lane.start();
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        engine.setTransmitLane(&lane);
        TxChannel* tx = engine.createTxChannel(kTxId, kTxInSize,
                                               WdspEngine::kTxDspBufferSize, kRateHz,
                                               WdspEngine::kTxDspSampleRate, kRateHz);
        QVERIFY(tx);
        TimedLog log;
        MockConnection conn(&log);
        tx->setConnection(&conn);
        auto analyzer = std::make_unique<TxAnalyzer>(TxAnalyzer::kTxDispId, nullptr, &lane);
        analyzer->setSampleRate(96000.0);
        analyzer->start();
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        analyzer->setBlockSize(tx->dspBlockFrames());   // as buildRig does
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));

        std::mutex drainMutex;
        std::vector<double> drains;
        tx->setDrainObserverForTest([&](double ms) {
            std::lock_guard<std::mutex> lock(drainMutex);
            drains.push_back(ms);
        });
        const auto lastDrain = [&]() {
            std::lock_guard<std::mutex> lock(drainMutex);
            return drains.empty() ? -1.0 : drains.back();
        };

        // With mic blocks arriving: the normal unkey, then the emergency
        // stop's order (gate first).
        double withBlocks = -1.0;
        double gateFirst = -1.0;
        {
            Feeder txFeeder(txBlockTick(tx));
            tx->setRunningAsync(true);
            QTRY_VERIFY_WITH_TIMEOUT(tx->isRunning(), kLaneIdleTimeoutMs);
            QTRY_VERIFY_WITH_TIMEOUT(conn.txBlocks() > 50, 10000);
            tx->setRunningAsync(false);
            QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
            QVERIFY(!tx->isRunning());
            withBlocks = lastDrain();

            tx->setRunningAsync(true);
            QTRY_VERIFY_WITH_TIMEOUT(tx->isRunning(), kLaneIdleTimeoutMs);
            const int before = conn.txBlocks();
            QTRY_VERIFY_WITH_TIMEOUT(conn.txBlocks() > before + 50, 10000);
            tx->closeRfGate();
            tx->setRunningAsync(false);
            QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
            gateFirst = lastDrain();
            txFeeder.stop();
        }

        // With none.
        tx->setRunningAsync(true);
        QTRY_VERIFY_WITH_TIMEOUT(tx->isRunning(), kLaneIdleTimeoutMs);
        QTest::qWait(100);
        tx->setRunningAsync(false);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        const double withoutBlocks = lastDrain();

        qInfo("unkey drain on the transmit lane: %.1f ms with mic blocks arriving "
              "(gate open, Thetis's order), %.1f ms with the gate closed first "
              "(emergency stop), %.1f ms without blocks (SetChannelState's timeout "
              "is 100 ms)",
              withBlocks, gateFirst, withoutBlocks);
        {
            std::lock_guard<std::mutex> lock(drainMutex);
            QCOMPARE(drains.size(), std::size_t(3));
        }
        QVERIFY2(withBlocks >= 0.0 && withBlocks < 100.0,
                 "the normal unkey's drain timed out instead of finishing");

        tx->setDrainObserverForTest({});
        tx->setConnection(nullptr);
        engine.shutdown();
        analyzer.reset();
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        engine.setTransmitLane(nullptr);
        lane.stop();
    }
#endif // HAVE_WDSP
};

QTEST_MAIN(TestDspControlTransmit)
#include "tst_dsp_control_transmit.moc"
