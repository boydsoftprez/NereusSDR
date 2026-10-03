// no-port-check: NereusSDR-original.
// Receive DSP off the event loop (R-R3-39, Task 31): with the receive lane
// set, RxChannel, NbFamily and NNR calls, channel create, destroy and rate
// change, the live sample-rate change and the meter reads make no WDSP call
// on the event loop, the event loop keeps its 10 ms timer while the lane
// waits on 150 ms DSP blocks, and the lane leaves WDSP with the last values
// set.
//
// Modification history: 2026-09-30 J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code: the loaded phase's idle reference also ticks on its
// own thread while the lane works, so a neighbour's load that starts after
// the idle phase is in both.
// 2026-09-30 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
// (fix wave, RD-I9): closingOnTheLaneDoesNotWaitOutTheDrain.
//
// REALTIME: the timer-gap bound is wall-clock time while two real channels
// run slow blocks and a feeder thread hands them I/Q.
#include <QtTest/QtTest>
#include "RealtimeTestLoad.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QSignalSpy>
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
#include "core/P1RadioConnection.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "core/dsp/ChannelConfig.h"
#include "core/WdspThreadCheck.h"
#include "core/wdsp_api.h"
#include "gui/meters/MeterPoller.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

#ifdef HAVE_WDSP
// The leading fields of WDSP's per-channel record (third_party/wdsp/src/
// channel.h, struct _ch), enough to read ch[0].in_rate, out_rate and
// dsp_rate, the rates WDSP's channel really runs at. Index 0 only: the declared type is shorter than
// WDSP's, so no other element may be addressed through it.
extern "C" {
struct WdspChannelHead {
    int type;
    volatile long run;
    volatile long exchange;
    int in_rate;
    int out_rate;
    int in_size;
    int dsp_rate;
};
extern WdspChannelHead ch[];
}
#endif

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kRateHz = 48000;
constexpr int kNewRateHz = 96000;
constexpr int kDspSize = 4096;
constexpr int kSlowBlockUs = 150000;
constexpr int kSetters = 200;
constexpr int kTimerIntervalMs = 10;
constexpr double kMaxTimerGapMs = 25.0;
// JJ's ruling for the transmit twin (2026-09-28 night), applied here: the
// loop may gap no more than the larger of kMaxTimerGapMs and the same
// run's idle worst gap plus this, so the machine's own scheduling (the
// idle loop alone misses 25 ms at a load of 24) is not charged to the lane.
constexpr double kIdleAllowanceMs = 15.0;
// The idle phase's length: about the loaded phase's (73 ticks at load 18).
constexpr int kIdleTicks = 75;
// The first OpenChannel in a process plans its FFTs with no wisdom: about
// 30 s on a quiet machine, over 100 s under a heavy build.
constexpr int kLaneIdleTimeoutMs = 600000;
constexpr double kMaxAsyncReturnMs = 2.0;

double msBetween(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
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

// Hands every channel one input block per round, looking each one up in
// the engine every time, the way RxDspWorker does. pause() returns once the
// feeder is outside every processIq call; it is the test's DSP-worker
// quiesce.
class Feeder {
public:
    Feeder(WdspEngine& engine, std::vector<int> channelIds)
        : m_engine(engine), m_ids(std::move(channelIds))
    {
        m_thread = std::thread([this] { run(); });
    }
    ~Feeder() { stop(); }

    void pause()
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        ++m_pauseDepth;
        m_cv.notify_all();
        m_cv.wait(lk, [this] { return m_parked || m_quit; });
    }
    void resume()
    {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            --m_pauseDepth;
        }
        m_cv.notify_all();
    }
    void stop()
    {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_quit = true;
        }
        m_cv.notify_all();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
    int pauses() const { return m_pauses.load(); }

private:
    void run()
    {
        std::vector<float> inI, inQ, outI, outQ;
        long n = 0;
        for (;;) {
            {
                std::unique_lock<std::mutex> lk(m_mutex);
                if (m_pauseDepth > 0 && !m_quit) {
                    m_parked = true;
                    m_pauses.fetch_add(1);
                    m_cv.notify_all();
                    m_cv.wait(lk, [this] { return m_pauseDepth == 0 || m_quit; });
                    m_parked = false;
                }
                if (m_quit) {
                    return;
                }
            }
            for (int id : m_ids) {
                RxChannel* rx = m_engine.rxChannel(id);
                if (!rx) {
                    continue;
                }
                const int inSize = rx->bufferSize();
                inI.resize(static_cast<std::size_t>(inSize));
                inQ.assign(static_cast<std::size_t>(inSize), 0.0f);
                for (int i = 0; i < inSize; ++i, ++n) {
                    inI[static_cast<std::size_t>(i)] = static_cast<float>(
                        0.01 * std::cos(2.0 * std::numbers::pi * 1000.0
                                        * static_cast<double>(n) / kRateHz));
                }
                outI.assign(static_cast<std::size_t>(inSize), 0.0f);
                outQ.assign(static_cast<std::size_t>(inSize), 0.0f);
                rx->processIq(inI.data(), inQ.data(), outI.data(), outQ.data(), inSize, 64);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    WdspEngine& m_engine;
    std::vector<int> m_ids;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    int m_pauseDepth{0};
    bool m_parked{false};
    bool m_quit{false};
    std::atomic<int> m_pauses{0};
    std::thread m_thread;
};

struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

} // namespace

class TestDspControlReceive : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { RealtimeTestLoad::printLoadAverageIfFailed(); }

    // WdspEngine::drainReceiveLane stops and starts the lane; teardown does
    // it several times in a row, with and without work queued.
    void theLaneStopsAndStartsRepeatedly()
    {
        DspControlThread lane(DspLane::Receive);
        lane.start();
        std::atomic<int> ran{0};
        for (int cycle = 0; cycle < 50; ++cycle) {
            if (cycle % 2 == 0) {
                lane.post([&ran]() { ran.fetch_add(1); });
            }
            lane.stop();
            lane.start();
        }
        lane.stop();
        QCOMPARE(ran.load(), 25);
    }

#ifdef HAVE_WDSP
    // The acceptance run: two real channels with 150 ms blocks, 200 mixed
    // setters, an off and on, a rate change and a destroy and recreate,
    // all from one event-loop tick. None of it may reach WDSP on the event
    // loop, and the event loop's 10 ms timer keeps running.
    void twoSlowChannelsLeaveTheEventLoopAlone()
    {
        DspControlThread lane(DspLane::Receive);
        lane.start();
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        engine.setReceiveLane(&lane);

        const int inSize = bufferSizeForRate(kRateHz);
        RxChannel* ch0 = engine.createRxChannel(0, inSize, kDspSize, kRateHz, kRateHz, kRateHz);
        RxChannel* ch1 = engine.createRxChannel(1, inSize, kDspSize, kRateHz, kRateHz, kRateHz);
        QVERIFY(ch0 && ch1);
        // The wrappers exist at once; their channels are open once the lane
        // has run the create barriers.
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        QVERIFY(ch0->isWdspReady() && ch1->isWdspReady());
        ch0->setActive(true);
        ch1->setActive(true);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));

        Feeder feeder(engine, {0, 1});
        engine.setRxWorkerQuiesce([&feeder]() -> std::function<void()> {
            feeder.pause();
            return [&feeder]() { feeder.resume(); };
        });
        WDSPSetTestBlockDelayUs(0, kSlowBlockUs);
        WDSPSetTestBlockDelayUs(1, kSlowBlockUs);
        // Let both workers get into their slow blocks.
        QTest::qWait(3 * kSlowBlockUs / 1000);

        // The same run's idle scheduling, under the same slow workers.
        double idleWorstGapMs = 0.0;
        int idleTicks = 0;
        {
            QEventLoop idleLoop;
            QTimer idleTicker;
            idleTicker.setTimerType(Qt::PreciseTimer);
            idleTicker.setInterval(kTimerIntervalMs);
            Clock::time_point previous = Clock::now();
            connect(&idleTicker, &QTimer::timeout, &idleLoop, [&] {
                const Clock::time_point now = Clock::now();
                if (idleTicks > 0) {
                    idleWorstGapMs = std::max(idleWorstGapMs, msBetween(previous, now));
                }
                previous = now;
                if (++idleTicks >= kIdleTicks) {
                    idleLoop.quit();
                }
            });
            idleTicker.start();
            idleLoop.exec();
        }

        WdspThreadCheck::install(QThread::currentThread());

        double lastTop[2]{0.0, 0.0};
        double lastShift[2]{0.0, 0.0};
        bool rateAnswered = false;
        bool rateOk = false;
        bool laneDone = false;
        Clock::time_point doneAt{};
        Clock::time_point postedAt{};
        double worstGapMs = 0.0;
        double actionsMs = 0.0;
        int ticks = 0;
        bool posted = false;
        // The idle reference also ticks while the lane works, so load that
        // starts after the idle phase is charged to both loops, not the lane.
        IdleTwinLoop idleTwin;
        Clock::time_point lastTick = Clock::now();
        QEventLoop loop;
        QObject answers;

        auto applySetter = [](RxChannel* rx, int i, double& top, double& shift) {
            switch (i % 10) {
            case 0: top = 40.0 + (i % 50); rx->setAgcTop(top); break;
            case 1: rx->setAfGain(0.2 + 0.001 * i); break;
            case 2: rx->setAgcHang(100 + i); break;
            case 3: rx->setAgcDecay(200 + i); break;
            case 4: rx->setFilterFreqs(-2800.0 + i, -100.0 - i); break;
            case 5: rx->setMode(i % 20 < 10 ? DSPMode::USB : DSPMode::LSB); break;
            case 6: shift = 100.0 + i; rx->setShiftFrequency(shift); break;
            case 7: rx->setApfFreq(500.0 + i); break;
            case 8: rx->setNbThreshold(3.0 + 0.01 * i); break;
            default: rx->setAudioPan(-0.5 + 0.005 * (i % 100)); break;
            }
        };

        QTimer ticker;
        ticker.setTimerType(Qt::PreciseTimer);
        ticker.setInterval(kTimerIntervalMs);
        connect(&ticker, &QTimer::timeout, &loop, [&] {
            const Clock::time_point now = Clock::now();
            if (ticks > 0) {
                worstGapMs = std::max(worstGapMs, msBetween(lastTick, now));
            }
            lastTick = now;
            ++ticks;
            // Meters every tick, as the pollers read them.
            (void)ch0->getMeter(RxMeterType::SignalAvg);
            if (RxChannel* one = engine.rxChannel(1)) {
                (void)one->getMeter(RxMeterType::SignalPeak);
            }
            if (ticks == 5 && !posted) {
                posted = true;
                postedAt = now;
                // Everything below happens on the event loop in this tick.
                for (int i = 0; i < kSetters - 20; ++i) {
                    RxChannel* rx = (i % 2 == 0) ? ch0 : ch1;
                    const int c = i % 2;
                    applySetter(rx, i / 2, lastTop[c], lastShift[c]);
                }
                ch0->setActive(false);
                ch0->setActive(true);
                engine.setRxChannelRateAsync(0, kNewRateHz, &answers, [&](bool ok) {
                    rateAnswered = true;
                    rateOk = ok;
                });
                engine.destroyRxChannel(1);
                RxChannel* again = engine.createRxChannel(1, inSize, kDspSize,
                                                          kRateHz, kRateHz, kRateHz);
                again->setActive(true);
                for (int i = 0; i < 18; ++i) {
                    applySetter(again, 500 + i, lastTop[1], lastShift[1]);
                }
                again->setAgcTop(77.0);
                lastTop[1] = 77.0;
                again->setShiftFrequency(1234.0);
                lastShift[1] = 1234.0;
                actionsMs = msBetween(now, Clock::now());
                lane.request<int>([]() { return 0; }, &answers, [&](int) {
                    laneDone = true;
                    doneAt = Clock::now();
                });
            }
            if (laneDone && msBetween(doneAt, now) >= 100.0) {
                loop.quit();
            }
        });
        QTimer deadline;
        deadline.setSingleShot(true);
        connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        deadline.start(kLaneIdleTimeoutMs);
        ticker.start();
        loop.exec();
        ticker.stop();
        idleTwin.stop();
        const double idleReferenceMs = std::max(idleWorstGapMs, idleTwin.worstGapMs());
        const double gapBoundMs = std::max(kMaxTimerGapMs, idleReferenceMs + kIdleAllowanceMs);
        const quint64 eventLoopCalls = WdspThreadCheck::eventLoopEntries();
        WdspThreadCheck::uninstall();

        QVERIFY2(laneDone, "the lane never finished the jobs");
        const double laneMs = msBetween(postedAt, doneAt);

        // Read back on the lane what WDSP holds now.
        feeder.stop();
        WDSPSetTestBlockDelayUs(0, 0);
        WDSPSetTestBlockDelayUs(1, 0);
        engine.setRxWorkerQuiesce({});
        std::array<double, 2> top{-1.0, -1.0};
        int inRate0 = 0;
        lane.post([&top, &inRate0]() {
            GetRXAAGCTop(0, &top[0]);
            GetRXAAGCTop(1, &top[1]);
            inRate0 = ch[0].in_rate;
        });
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        RxChannel* one = engine.rxChannel(1);
        QVERIFY(one);

        qInfo("%d setters, an off and on, a rate change and a destroy and recreate in "
              "%.2f ms on the event loop; lane busy %.0f ms; %d ticks; worst %d ms timer "
              "gap %.2f ms (limit %.1f: idle worst %.2f ms over %d ticks, idle twin worst "
              "%.2f ms over %d ticks); feeder parked %d times; WDSP calls on the event "
              "loop %llu",
              kSetters, actionsMs, laneMs, ticks, kTimerIntervalMs, worstGapMs,
              gapBoundMs, idleWorstGapMs, idleTicks, idleTwin.worstGapMs(),
              idleTwin.ticks(), feeder.pauses(),
              static_cast<unsigned long long>(eventLoopCalls));

        QCOMPARE(eventLoopCalls, quint64(0));
        QVERIFY2(laneMs >= kSlowBlockUs / 2000.0, "the lane never waited on a slow block");
        QCOMPARE(idleTicks, kIdleTicks);
        QVERIFY2(idleTwin.ticks() >= ticks / 2, "the idle twin loop never ran");
        QVERIFY2(worstGapMs <= gapBoundMs, "the event loop's 10 ms timer gapped");
        QVERIFY(rateAnswered);
        QVERIFY(rateOk);
        QCOMPARE(top[0], lastTop[0]);
        QCOMPARE(top[1], lastTop[1]);
        QCOMPARE(ch0->notchShiftHz(), lastShift[0]);
        QCOMPARE(one->notchShiftHz(), lastShift[1]);
        QCOMPARE(inRate0, kNewRateHz);
        QCOMPARE(ch0->sampleRate(), kNewRateHz);
        QVERIFY(ch0->isActive() && one->isActive());

        engine.shutdown();
        engine.setReceiveLane(nullptr);
        lane.stop();
    }

    // RD-I9 (fix wave 2026-09-30): the lane closes a channel, for a destroy
    // or a rebuild, with the DSP worker quiesced, so no I/Q reaches the
    // channel and a draining stop can only time out. WDSP's draining
    // SetChannelState waits 100 Sleep(1) calls before it gives up
    // (third_party/wdsp/src/channel.c:288-304), so each close cost at least
    // kWdspDrainTimeoutMs of receive DSP. The lane closes without that wait,
    // as Thetis's destroy_rcvr closes its channels (cmaster.c:96-108).
    // Two destroys and two rebuilds of running channels, no feeder: with a
    // drain per close they take at least four drain timeouts on the lane.
    // A rebuild reopens at 48 kHz DSP and output rates whatever its input
    // rate, as createRxChannel's callers open it (WdspEngine minor).
    void closingOnTheLaneDoesNotWaitOutTheDrain()
    {
        // The draining SetChannelState's bound: 100 Sleep(1) calls.
        constexpr int kWdspDrainTimeoutMs = 100;
        constexpr int kCloses = 4;
        DspControlThread lane(DspLane::Receive);
        lane.start();
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        engine.setReceiveLane(&lane);
        // Production quiesces the receive worker around each close; here no
        // worker feeds the channels at all.
        int quiesced = 0;
        engine.setRxWorkerQuiesce([&quiesced]() -> std::function<void()> {
            ++quiesced;
            return {};
        });

        const int inSize = bufferSizeForRate(kRateHz);
        std::array<RxChannel*, kCloses> channels{};
        for (int id = 0; id < kCloses; ++id) {
            channels[id] = engine.createRxChannel(id, inSize, kDspSize, kRateHz, kRateHz, kRateHz);
            QVERIFY(channels[id]);
        }
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        for (RxChannel* rx : channels) {
            QVERIFY(rx->isWdspReady());
            rx->setActive(true);
        }
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        const int quiescedBefore = quiesced;

        ChannelConfig cfg;
        cfg.sampleRate = kRateHz;
        cfg.bufferSize = inSize;
        cfg.filterSize = kDspSize;
        // Channel 0 comes back at a new input rate; its DSP and output rates
        // stay 48 kHz, as at create (Thetis cmaster.c:76-78).
        ChannelConfig fasterCfg = cfg;
        fasterCfg.sampleRate = kNewRateHz;
        fasterCfg.bufferSize = bufferSizeForRate(kNewRateHz);
        QElapsedTimer timer;
        timer.start();
        engine.destroyRxChannel(1);
        engine.destroyRxChannel(2);
        QVERIFY(engine.rebuildRxChannel(0, fasterCfg) >= 0);
        QVERIFY(engine.rebuildRxChannel(3, cfg) >= 0);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        const qint64 closeMs = timer.elapsed();
        int inRate0 = 0;
        int dspRate0 = 0;
        int outRate0 = 0;
        lane.post([&inRate0, &dspRate0, &outRate0]() {
            inRate0 = ch[0].in_rate;
            dspRate0 = ch[0].dsp_rate;
            outRate0 = ch[0].out_rate;
        });
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));

        qInfo("2 destroys and 2 rebuilds of running channels on the lane in %lld ms "
              "(a drain per close would take at least %d ms)",
              static_cast<long long>(closeMs), kCloses * kWdspDrainTimeoutMs);
        QCOMPARE(quiesced - quiescedBefore, kCloses);
        QVERIFY2(closeMs < kCloses * kWdspDrainTimeoutMs,
                 "the lane waited out WDSP's drain on a channel no I/Q reaches");
        RxChannel* rebuilt = engine.rxChannel(0);
        QVERIFY(rebuilt && rebuilt->isWdspReady());
        QCOMPARE(rebuilt->sampleRate(), kNewRateHz);
        QVERIFY(engine.rxChannel(1) == nullptr);
        QCOMPARE(inRate0, kNewRateHz);
        QCOMPARE(dspRate0, kRateHz);
        QCOMPARE(outRate0, kRateHz);

        engine.shutdown();
        engine.setReceiveLane(nullptr);
        lane.stop();
    }

    // setSampleRateLiveAsync returns at once and finishes exactly once; a
    // request for the rate the radio is at, or is moving to, emits nothing.
    void liveRateChangeIsAsynchronousAndFinishesOnce()
    {
        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};
        QVERIFY(model.receiveLane() != nullptr);

        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        model.configureStreamPool(1, 1, kRateHz);
        const int id = model.addSlice();
        QVERIFY(id >= 0);
        model.openRxChannelPool(1, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(model.waitForReceiveLaneForTest(kLaneIdleTimeoutMs));

        QSignalSpy finished(&model, &RadioModel::sampleRateChangeFinished);
        QSignalSpy wire(&model, &RadioModel::wireSampleRateChanged);
        WdspThreadCheck::install(QThread::currentThread());

        QElapsedTimer timer;
        timer.start();
        model.setSampleRateLiveAsync(192000);
        const double returnMs = static_cast<double>(timer.nsecsElapsed()) / 1.0e6;
        // Asked again while it runs: the same rate starts nothing.
        model.setSampleRateLiveAsync(192000);

        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, kLaneIdleTimeoutMs);
        QVERIFY(model.waitForReceiveLaneForTest(kLaneIdleTimeoutMs));
        QTest::qWait(50);
        const quint64 eventLoopCalls = WdspThreadCheck::eventLoopEntries();
        WdspThreadCheck::uninstall();

        qInfo("setSampleRateLiveAsync returned in %.3f ms (limit %.1f); WDSP calls on "
              "the event loop %llu", returnMs, kMaxAsyncReturnMs,
              static_cast<unsigned long long>(eventLoopCalls));
        QVERIFY2(returnMs <= kMaxAsyncReturnMs, "setSampleRateLiveAsync waited");
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(0).toInt(), 192000);
        QCOMPARE(finished.at(0).at(1).toBool(), true);
        QCOMPARE(wire.count(), 1);
        QCOMPARE(model.connectionSampleRateHz(), 192000);
        QCOMPARE(eventLoopCalls, quint64(0));
        RxChannel* rx = engine->rxChannel(model.sliceById(id)->sliceIndex());
        QVERIFY(rx);
        QCOMPARE(rx->sampleRate(), 192000);

        // At the rate already: nothing starts, nothing is emitted.
        model.setSampleRateLiveAsync(192000);
        model.setSampleRateLiveAsync(192000);
        QVERIFY(model.waitForReceiveLaneForTest(kLaneIdleTimeoutMs));
        QTest::qWait(50);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(wire.count(), 1);
    }

    // The desktop's MeterPoller reads the cache: polling a running channel
    // on a lane makes no WDSP call on the GUI thread, and the lane keeps the
    // cache fresh.
    void meterPollerReadsTheLaneCache()
    {
        DspControlThread lane(DspLane::Receive);
        lane.start();
        WdspEngine engine;
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        engine.setReceiveLane(&lane);
        const int inSize = bufferSizeForRate(kRateHz);
        RxChannel* rx = engine.createRxChannel(0, inSize, kDspSize, kRateHz, kRateHz, kRateHz);
        QVERIFY(rx);
        rx->setActive(true);
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));

        Feeder feeder(engine, {0});
        QTest::qWait(300);

        MeterPoller poller;
        poller.setRxChannel(rx);
        poller.setWdspEngine(&engine);
        poller.setIntervalMs(10);

        WdspThreadCheck::install(QThread::currentThread());
        const double before = rx->getMeter(RxMeterType::AdcPeak);
        poller.start();
        QTest::qWait(300);
        poller.stop();
        const quint64 eventLoopCalls = WdspThreadCheck::eventLoopEntries();
        WdspThreadCheck::uninstall();
        QVERIFY(lane.waitIdleForTest(kLaneIdleTimeoutMs));
        const double after = rx->getMeter(RxMeterType::AdcPeak);
        feeder.stop();

        qInfo("ADC peak from the cache: %.1f dBm before polling, %.1f after; WDSP calls "
              "on the GUI thread %llu", before, after,
              static_cast<unsigned long long>(eventLoopCalls));
        QCOMPARE(eventLoopCalls, quint64(0));
        // The fed tone reaches the ADC meter once the lane has read it.
        QVERIFY2(after > -140.0, "the lane never refreshed the meter cache");

        engine.shutdown();
        engine.setReceiveLane(nullptr);
        lane.stop();
    }
#endif // HAVE_WDSP
};

QTEST_MAIN(TestDspControlReceive)
#include "tst_dsp_control_receive.moc"
