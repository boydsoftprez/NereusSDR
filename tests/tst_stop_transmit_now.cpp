// no-port-check: NereusSDR-original.
// Task 33 (R-IOS-03, remote design §12.1): stopping transmission at once.
//
//   returnsAtOnceWithBothLanesBusy: with both DSP lanes held for 200 ms,
//     RadioModel::stopTransmitNow returns within 2 ms with the RF gate shut,
//     makes no WDSP call, and MOX and the relay reach the connection's
//     thread before either lane is free; no TX I/Q block reaches the
//     connection after it returns.
//   aQueuedChannelOnDoesNotReopenTheGate: a channel-on still queued on a
//     busy transmit lane does not open the gate the stop closed; the next
//     fresh key does.
//   normalUnkeyDrainsBeforeTheHardware: the normal unkey follows Thetis
//     (console.cs chkMOX_CheckedChanged2, TX to RX): the TX channel drains
//     with the gate open, then the gate closes, then MOX goes off, then the
//     receiver comes back; the drain finishes instead of timing out.
//
// REALTIME: wall-clock bounds while a real TX channel and a real RX channel
// run and feeder threads drive them.
#include <QtTest/QtTest>
#include "RealtimeTestLoad.h"

#include <QThread>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <thread>
#include <vector>

#include "core/DspControlThread.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/TxAnalyzer.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspThreadCheck.h"
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
constexpr int kLaneHoldMs = 200;
constexpr double kStopBoundMs = 2.0;
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

private:
    struct Entry {
        QString what;
        Clock::time_point at;
    };
    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
};

TimedLog* g_log = nullptr;

void recordingHook(int channel, int kind)
{
    if (g_log != nullptr && channel == kTxId && kind == kWdspCallerSetChannelState) {
        g_log->add(QStringLiteral("SetChannelState"));
    }
}

// Records MOX and relay writes with their times, and every TX I/Q block
// (whether it carried a non-zero sample, and its time).
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockConnection(TimedLog* log) : RadioConnection(nullptr), m_log(log)
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
    void sendTxIq(const float* iq, int n) override
    {
        bool nonZero = false;
        for (int i = 0; i < 2 * n; ++i) {
            if (iq[i] != 0.0f) {
                nonZero = true;
                break;
            }
        }
        const Clock::time_point now = Clock::now();
        std::lock_guard<std::mutex> lock(m_blockMutex);
        ++m_blocks;
        m_lastBlockAt = now;
        if (nonZero) {
            ++m_nonZeroBlocks;
            m_lastNonZeroAt = now;
        }
        if (now >= m_countFrom) {
            ++m_blocksSince;
            if (nonZero) {
                ++m_nonZeroSince;
            }
        }
    }
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool on) override
    {
        m_log->add(on ? QStringLiteral("MOX on") : QStringLiteral("MOX off"));
    }
    void setTrxRelay(bool on) override
    {
        m_log->add(on ? QStringLiteral("relay on") : QStringLiteral("relay off"));
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

    int blocks() const
    {
        std::lock_guard<std::mutex> lock(m_blockMutex);
        return m_blocks;
    }
    // Start counting blocks that arrive from `from` on.
    void countFrom(Clock::time_point from)
    {
        std::lock_guard<std::mutex> lock(m_blockMutex);
        m_countFrom = from;
        m_blocksSince = 0;
        m_nonZeroSince = 0;
    }
    int blocksSince() const
    {
        std::lock_guard<std::mutex> lock(m_blockMutex);
        return m_blocksSince;
    }
    int nonZeroSince() const
    {
        std::lock_guard<std::mutex> lock(m_blockMutex);
        return m_nonZeroSince;
    }
    Clock::time_point lastNonZeroAt() const
    {
        std::lock_guard<std::mutex> lock(m_blockMutex);
        return m_lastNonZeroAt;
    }

private:
    TimedLog* m_log;
    mutable std::mutex m_blockMutex;
    int m_blocks{0};
    int m_nonZeroBlocks{0};
    Clock::time_point m_lastBlockAt{};
    Clock::time_point m_lastNonZeroAt{};
    Clock::time_point m_countFrom{Clock::time_point::max()};
    int m_blocksSince{0};
    int m_nonZeroSince{0};
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

// The TX worker's per-block calls: a 1 kHz tone into DEXP, then fexchange0.
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

// Holds a lane for kLaneHoldMs; logs when the hold starts and ends.
void holdLane(DspControlThread* lane, TimedLog* log, const QString& name,
              std::shared_ptr<std::atomic<bool>> started)
{
    lane->post([log, name, started]() {
        started->store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(kLaneHoldMs));
        log->add(name + QStringLiteral(" free"));
    });
}

} // namespace

class TestStopTransmitNow : public QObject {
    Q_OBJECT

    // A local RadioModel (with both lanes) whose TX channel is real and
    // wired to MoxController as the connect path wires it, a real RX
    // channel, and a mock connection living on its own thread, as the
    // radio connection does.
    struct Rig {
        std::unique_ptr<RadioModel> model;
        std::unique_ptr<MockConnection> conn;
        std::unique_ptr<QThread> connThread;
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
        rig.connThread = std::make_unique<QThread>();
        rig.conn->moveToThread(rig.connThread.get());
        rig.connThread->start();
        model.injectConnectionForTest(rig.conn.get());
        model.setTuneOffSettleMsForTest(0);
        model.addSlice();
        if (SliceModel* slice = model.activeSlice()) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14'200'000.0);
        }

        // The TX siphon feeds display 5; its calls run on the transmit lane.
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
        if (rig.model) {
            RadioModel& model = *rig.model;
            if (rig.tx) {
                rig.tx->setRfGateObserverForTest({});
                rig.tx->setDrainObserverForTest({});
                rig.tx->setConnection(nullptr);
            }
            model.injectTxChannelForTest(nullptr);
            if (rig.engine) {
                rig.engine->shutdown();
            }
            rig.analyzer.reset();
            model.injectConnectionForTest(nullptr);
            rig.model.reset();
        }
        if (rig.connThread) {
            rig.connThread->quit();
            rig.connThread->wait();
        }
        rig.conn.reset();
        rig.connThread.reset();
    }

    // Keys MOX with both feeders running and waits for TX blocks.
    static bool keyAndWaitForBlocks(Rig& rig)
    {
        rig.model->moxController()->setMox(true);
        if (!QTest::qWaitFor([&] { return rig.tx->isRfGateOpen(); }, kLaneIdleTimeoutMs)) {
            return false;
        }
        const int before = rig.conn->blocks();
        return QTest::qWaitFor([&] { return rig.conn->blocks() > before + 50; }, 10000);
    }

    // Unkeys and waits until the TX to RX walk is back at Rx.
    static bool unkeyAndWait(Rig& rig)
    {
        rig.model->moxController()->setMox(false);
        return QTest::qWaitFor([&] {
            return rig.model->moxController()->state() == MoxState::Rx;
        }, 5000);
    }

private slots:
    void cleanup()
    {
        WDSPSetCallerCheckHook(nullptr);
        g_log = nullptr;
        RealtimeTestLoad::printLoadAverageIfFailed();
    }

#ifdef HAVE_WDSP
    void returnsAtOnceWithBothLanesBusy()
    {
        TimedLog log;
        Rig rig;
        QVERIFY2(buildRig(rig, &log), "the rig did not come up");
        Feeder rxFeeder(rxBlockTick(rig.rx));
        Feeder txFeeder(txBlockTick(rig.tx));
        QVERIFY2(keyAndWaitForBlocks(rig), "keying did not reach the connection");

        auto txHeld = std::make_shared<std::atomic<bool>>(false);
        auto rxHeld = std::make_shared<std::atomic<bool>>(false);
        holdLane(rig.model->transmitLane(), &log, QStringLiteral("transmit lane"), txHeld);
        holdLane(rig.model->receiveLane(), &log, QStringLiteral("receive lane"), rxHeld);
        QVERIFY(QTest::qWaitFor([&] { return txHeld->load() && rxHeld->load(); }, 5000));

        WdspThreadCheck::install(QThread::currentThread());
        const Clock::time_point stopAt = Clock::now();
        rig.model->stopTransmitNow(QStringLiteral("test"));
        const Clock::time_point returnedAt = Clock::now();
        rig.conn->countFrom(returnedAt);
        const bool gateOpen = rig.tx->isRfGateOpen();
        const quint64 eventLoopCalls = WdspThreadCheck::eventLoopEntries();
        WdspThreadCheck::uninstall();

        QTRY_VERIFY_WITH_TIMEOUT(log.first(QStringLiteral("transmit lane free")).has_value()
                                 && log.first(QStringLiteral("receive lane free")).has_value(),
                                 5000);
        const auto moxOff = log.first(QStringLiteral("MOX off"), stopAt);
        const auto relayOff = log.first(QStringLiteral("relay off"), stopAt);
        const auto txFree = log.first(QStringLiteral("transmit lane free"));
        const auto rxFree = log.first(QStringLiteral("receive lane free"));
        // The feeder keeps running: nothing may reach the connection now.
        QTest::qWait(300);

        qInfo("stopTransmitNow returned in %.3f ms (limit %.1f); MOX off +%.2f ms, relay off "
              "+%.2f ms; transmit lane free +%.1f ms, receive lane free +%.1f ms; "
              "TX blocks after it %d (non-zero %d); WDSP calls on the event loop %llu",
              msBetween(stopAt, returnedAt), kStopBoundMs,
              moxOff ? msBetween(stopAt, *moxOff) : -1.0,
              relayOff ? msBetween(stopAt, *relayOff) : -1.0,
              msBetween(stopAt, *txFree), msBetween(stopAt, *rxFree),
              rig.conn->blocksSince(), rig.conn->nonZeroSince(),
              static_cast<unsigned long long>(eventLoopCalls));
        QVERIFY2(msBetween(stopAt, returnedAt) < kStopBoundMs, "stopTransmitNow did not return at once");
        QVERIFY2(!gateOpen, "the RF gate was open when stopTransmitNow returned");
        QCOMPARE(eventLoopCalls, quint64(0));
        QVERIFY(moxOff && relayOff);
        QVERIFY2(*moxOff < *txFree && *moxOff < *rxFree,
                 "MOX off waited for a lane");
        QVERIFY2(*relayOff < *txFree && *relayOff < *rxFree,
                 "relay off waited for a lane");
        QCOMPARE(rig.conn->nonZeroSince(), 0);
        QCOMPARE(rig.conn->blocksSince(), 0);

        // The caller then clears MOX; the walk ends at Rx without keying.
        // With the gate shut first the drain waits out WDSP's timeout, so
        // the walk's bound releases the hardware (already off) without it.
        QVERIFY(unkeyAndWait(rig));
        QVERIFY(!rig.tx->isRfGateOpen());
        QCOMPARE(rig.conn->nonZeroSince(), 0);
        QVERIFY(!log.first(QStringLiteral("MOX on"), stopAt).has_value());
        QVERIFY(rig.model->waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
        txFeeder.stop();
        rxFeeder.stop();
    }

    void aQueuedChannelOnDoesNotReopenTheGate()
    {
        TimedLog log;
        Rig rig;
        QVERIFY2(buildRig(rig, &log), "the rig did not come up");
        Feeder txFeeder(txBlockTick(rig.tx));

        auto held = std::make_shared<std::atomic<bool>>(false);
        holdLane(rig.model->transmitLane(), &log, QStringLiteral("transmit lane"), held);
        QVERIFY(QTest::qWaitFor([&] { return held->load(); }, 5000));
        rig.tx->setRunningAsync(true);    // queued behind the hold
        rig.tx->closeRfGate();
        QVERIFY(rig.model->waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
        QTest::qWait(50);
        QVERIFY2(!rig.tx->isRfGateOpen(), "a channel-on queued before the stop opened the gate");

        // A fresh on after the stop opens it.
        rig.tx->setRunningAsync(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.tx->isRfGateOpen(), 5000);
        rig.tx->setRunningAsync(false);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.tx->isRfGateOpen(), 5000);
        QVERIFY(rig.model->waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
        txFeeder.stop();
    }

    // Thetis's unkey (console.cs chkMOX_CheckedChanged2, TX to RX): the TX
    // channel drains first, with the hardware still keyed; mox_delay; the
    // hardware flip; ptt_out_delay; the receiver on.
    void normalUnkeyDrainsBeforeTheHardware()
    {
        TimedLog log;
        Rig rig;
        QVERIFY2(buildRig(rig, &log), "the rig did not come up");
        Feeder rxFeeder(rxBlockTick(rig.rx));
        Feeder txFeeder(txBlockTick(rig.tx));
        QVERIFY2(keyAndWaitForBlocks(rig), "keying did not reach the connection");

        std::atomic<double> drainMs{-1.0};
        rig.tx->setDrainObserverForTest([&](double ms) {
            drainMs.store(ms);
            log.add(QStringLiteral("drain returned"));
        });
        rig.tx->setRfGateObserverForTest([&log](bool open) {
            log.add(open ? QStringLiteral("gate open") : QStringLiteral("gate closed"));
        });
        connect(rig.rx, &RxChannel::activeChanged, this, [&log](bool on) {
            log.add(on ? QStringLiteral("rx on") : QStringLiteral("rx off"));
        });
        g_log = &log;
        WDSPSetCallerCheckHook(&recordingHook);

        const Clock::time_point unkeyAt = Clock::now();
        rig.conn->countFrom(unkeyAt);
        QVERIFY(unkeyAndWait(rig));
        QTRY_VERIFY_WITH_TIMEOUT(log.first(QStringLiteral("rx on"), unkeyAt).has_value(), 5000);
        WDSPSetCallerCheckHook(nullptr);

        const auto channelOff = log.first(QStringLiteral("SetChannelState"), unkeyAt);
        const auto drained = log.first(QStringLiteral("drain returned"), unkeyAt);
        const auto gateClosed = log.first(QStringLiteral("gate closed"), unkeyAt);
        const auto moxOff = log.first(QStringLiteral("MOX off"), unkeyAt);
        const auto relayOff = log.first(QStringLiteral("relay off"), unkeyAt);
        const auto rxOn = log.first(QStringLiteral("rx on"), unkeyAt);
        const Clock::time_point lastNonZero = rig.conn->lastNonZeroAt();
        qInfo("normal unkey: SetChannelState(tx, 0, 1) +%.2f ms, drain %.1f ms, gate closed "
              "+%.2f ms, MOX off +%.2f ms, relay off +%.2f ms, receiver on +%.2f ms; "
              "TX blocks during the drain %d (non-zero %d), last non-zero +%.2f ms",
              channelOff ? msBetween(unkeyAt, *channelOff) : -1.0, drainMs.load(),
              gateClosed ? msBetween(unkeyAt, *gateClosed) : -1.0,
              moxOff ? msBetween(unkeyAt, *moxOff) : -1.0,
              relayOff ? msBetween(unkeyAt, *relayOff) : -1.0,
              rxOn ? msBetween(unkeyAt, *rxOn) : -1.0,
              rig.conn->blocksSince(), rig.conn->nonZeroSince(),
              msBetween(unkeyAt, lastNonZero));
        QVERIFY(channelOff && drained && gateClosed && moxOff && relayOff && rxOn);
        QVERIFY2(*channelOff < *gateClosed, "the gate closed before the drain began");
        QVERIFY2(*drained <= *gateClosed, "the gate closed before the drain returned");
        QVERIFY2(*gateClosed <= *moxOff, "MOX went off before the TX channel drained");
        QVERIFY2(*moxOff <= *relayOff, "the relay dropped before MOX");
        QVERIFY2(*moxOff < *rxOn, "the receiver came back before the hardware flip");
        // The drain finished (WDSP's down-slew went out); it did not wait
        // out SetChannelState's 100 x Sleep(1) timeout.
        QVERIFY2(drainMs.load() >= 0.0 && drainMs.load() < 100.0,
                 "the drain timed out instead of finishing");
        QVERIFY2(rig.conn->blocksSince() > 0, "no block went out during the drain");
        QVERIFY2(lastNonZero < *moxOff, "a non-zero block went out after MOX off");

        QVERIFY(rig.model->waitForTransmitLaneForTest(kLaneIdleTimeoutMs));
        txFeeder.stop();
        rxFeeder.stop();
    }
#endif // HAVE_WDSP
};

QTEST_MAIN(TestStopTransmitNow)
#include "tst_stop_transmit_now.moc"
