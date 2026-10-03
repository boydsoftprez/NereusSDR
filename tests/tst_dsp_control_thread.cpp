// no-port-check: NereusSDR-original.
// The DSP control lanes (DspControlThread) and the WDSP caller check
// (WdspThreadCheck, dsplock.c's WDSPSetCallerCheckHook), R-R3-39: lane
// ordering, keyed replacement and barriers, request completion, the event
// loop staying responsive while a lane waits on a slow DSP worker, and the
// count of WDSP calls made from the event loop.
#include <QtTest>
#include "RealtimeTestLoad.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QPointer>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <numbers>
#include <thread>
#include <vector>

#include "core/DspControlThread.h"
#include "core/WdspThreadCheck.h"
#include "core/wdsp_api.h"

using namespace NereusSDR;

namespace {

using Clock = std::chrono::steady_clock;

// A WDSP channel id no application object uses in this process (the same
// one the other WDSP lock tests use).
constexpr int kChannel = 20;
constexpr int kSampleRate = 48000;
constexpr int kInSize = 1024;
constexpr int kDspSize = 4096;
// The acceptance's slow DSP block.
constexpr int kSlowBlockUs = 200000;

constexpr int kPosters = 4;
constexpr int kJobsPerPoster = 250;
constexpr int kKeyedPosts = 100;
constexpr int kAgcTopCalls = 50;
constexpr int kStatePairs = 5;

// Acceptance bounds (not to be loosened; report measurements instead).
constexpr double kMaxPostMs = 1.0;
constexpr int kTimerIntervalMs = 10;
constexpr double kMaxTimerGapMs = 25.0;

constexpr int kIdleTimeoutMs = 30000;

double msBetween(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

/// A gate a lane job waits on, so the test can hold the lane busy.
class Gate {
public:
    void open()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open = true;
        m_cv.notify_all();
    }
    void wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_entered = true;
        m_cv.notify_all();
        m_cv.wait(lock, [this] { return m_open; });
    }
    bool waitEntered(int timeoutMs)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                             [this] { return m_entered; });
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_open{false};
    bool m_entered{false};
};

/// Holds `lane` inside one job until the gate opens.
void blockLane(DspControlThread& lane, Gate& gate)
{
    lane.post([&gate] { gate.wait(); });
    QVERIFY(gate.waitEntered(5000));
}

/// Delivers the event loop's queued calls without waiting on the clock.
void deliverPostedEvents()
{
    for (int pass = 0; pass < 5; ++pass) {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

} // namespace

class TestDspControlThread : public QObject {
    Q_OBJECT

private slots:
    // The load when a real-time case failed (R-R3-21).
    void cleanup() { RealtimeTestLoad::printLoadAverageIfFailed(); }

    // ── Step 1: the lane ────────────────────────────────────────────────

    void fourPostersRunOnTheLaneInEachPostersOrder()
    {
        DspControlThread lane(DspLane::Receive);
        lane.start();

        struct Ran {
            int poster;
            int sequence;
            bool onLane;
            Qt::HANDLE thread;
        };
        std::vector<Ran> ran;   // touched only on the lane
        ran.reserve(kPosters * kJobsPerPoster);

        std::vector<std::thread> posters;
        for (int poster = 0; poster < kPosters; ++poster) {
            posters.emplace_back([&lane, &ran, poster] {
                for (int sequence = 0; sequence < kJobsPerPoster; ++sequence) {
                    lane.post([&lane, &ran, poster, sequence] {
                        ran.push_back({poster, sequence, lane.isCurrentThread(),
                                       QThread::currentThreadId()});
                    });
                }
            });
        }
        for (std::thread& poster : posters) {
            poster.join();
        }
        QVERIFY(lane.waitIdleForTest(kIdleTimeoutMs));

        QCOMPARE(int(ran.size()), kPosters * kJobsPerPoster);
        std::array<int, kPosters> next{};
        const Qt::HANDLE laneThread = ran.front().thread;
        QVERIFY(laneThread != QThread::currentThreadId());
        for (const Ran& job : ran) {
            QVERIFY(job.onLane);
            QCOMPARE(job.thread, laneThread);
            QCOMPARE(job.sequence, next[job.poster]);
            ++next[job.poster];
        }
        QVERIFY(!lane.isCurrentThread());
        lane.stop();
    }

    void keyedPostsLeaveOnlyTheLastValue()
    {
        DspControlThread lane(DspLane::Transmit);
        lane.start();
        Gate gate;
        blockLane(lane, gate);

        std::vector<int> ran;   // lane only
        for (int value = 0; value < kKeyedPosts; ++value) {
            lane.postKeyed(7, [&ran, value] { ran.push_back(value); });
        }
        // Another key is its own job.
        lane.postKeyed(8, [&ran] { ran.push_back(-8); });
        gate.open();
        QVERIFY(lane.waitIdleForTest(kIdleTimeoutMs));

        QCOMPARE(ran.size(), std::size_t(2));
        QCOMPARE(ran[0], kKeyedPosts - 1);
        QCOMPARE(ran[1], -8);
        lane.stop();
    }

    void keyedPostsNeverCrossABarrier()
    {
        DspControlThread lane(DspLane::Transmit);
        lane.start();
        Gate gate;
        blockLane(lane, gate);

        static constexpr int kBarrier = -1;
        std::vector<int> ran;   // lane only
        for (int value = 0; value < kKeyedPosts; ++value) {
            if (value == kKeyedPosts / 2) {
                lane.postBarrier([&ran] { ran.push_back(kBarrier); });
            }
            lane.postKeyed(7, [&ran, value] { ran.push_back(value); });
        }
        gate.open();
        QVERIFY(lane.waitIdleForTest(kIdleTimeoutMs));

        // One survivor on each side, each the last value posted on its side.
        const std::vector<int> expected{kKeyedPosts / 2 - 1, kBarrier, kKeyedPosts - 1};
        QCOMPARE(ran, expected);
        lane.stop();
    }

    void keyedPostTakesTheNewestPosition()
    {
        DspControlThread lane(DspLane::Receive);
        lane.start();
        Gate gate;
        blockLane(lane, gate);

        std::vector<int> ran;   // lane only
        lane.postKeyed(3, [&ran] { ran.push_back(1); });
        lane.post([&ran] { ran.push_back(2); });
        lane.postKeyed(3, [&ran] { ran.push_back(3); });
        gate.open();
        QVERIFY(lane.waitIdleForTest(kIdleTimeoutMs));

        const std::vector<int> expected{2, 3};
        QCOMPARE(ran, expected);
        lane.stop();
    }

    void requestAnswersOnTheContextsThread()
    {
        DspControlThread lane(DspLane::Receive);
        lane.start();
        QObject context;

        Qt::HANDLE jobThread = nullptr;
        Qt::HANDLE doneThread = nullptr;
        int answer = 0;
        // Asked from another thread; answered on the context's (this) one.
        std::thread asker([&] {
            lane.request<int>(
                [&jobThread] {
                    jobThread = QThread::currentThreadId();
                    return 42;
                },
                &context,
                [&doneThread, &answer](int value) {
                    doneThread = QThread::currentThreadId();
                    answer = value;
                });
        });
        asker.join();
        QTRY_COMPARE_WITH_TIMEOUT(answer, 42, kIdleTimeoutMs);
        QCOMPARE(doneThread, QThread::currentThreadId());
        QVERIFY(jobThread != nullptr);
        QVERIFY(jobThread != QThread::currentThreadId());
        lane.stop();
    }

    void requestNeverAnswersADeletedContext()
    {
        DspControlThread lane(DspLane::Receive);
        lane.start();
        int answers = 0;
        int jobsRun = 0;   // lane only until waitIdleForTest returns

        // Deleted while the job is still queued.
        {
            Gate gate;
            blockLane(lane, gate);
            auto context = std::make_unique<QObject>();
            lane.request<int>([&jobsRun] { return ++jobsRun; }, context.get(),
                              [&answers](int) { ++answers; });
            context.reset();
            gate.open();
            QVERIFY(lane.waitIdleForTest(kIdleTimeoutMs));
            deliverPostedEvents();
        }
        // Deleted after the lane has answered, before the event loop
        // delivered the answer.
        {
            auto context = std::make_unique<QObject>();
            lane.request<int>([&jobsRun] { return ++jobsRun; }, context.get(),
                              [&answers](int) { ++answers; });
            QVERIFY(lane.waitIdleForTest(kIdleTimeoutMs));
            context.reset();
            deliverPostedEvents();
        }
        QCOMPARE(jobsRun, 2);
        QCOMPARE(answers, 0);

        // A live context is still answered (the check above is not vacuous).
        QObject live;
        lane.request<int>([] { return 5; }, &live, [&answers](int value) { answers = value; });
        QTRY_COMPARE_WITH_TIMEOUT(answers, 5, kIdleTimeoutMs);
        lane.stop();
    }

    void jobsWaitForStartAndStopRunsWhatIsQueued()
    {
        DspControlThread lane(DspLane::Transmit);
        std::atomic<int> ran{0};
        lane.post([&ran] { ran.fetch_add(1); });   // before start
        QCOMPARE(ran.load(), 0);
        lane.start();
        QVERIFY(lane.waitIdleForTest(kIdleTimeoutMs));
        QCOMPARE(ran.load(), 1);

        Gate gate;
        blockLane(lane, gate);
        for (int job = 0; job < 20; ++job) {
            lane.post([&ran] { ran.fetch_add(1); });
        }
        std::thread opener([&gate] { gate.open(); });
        lane.stop();   // runs the 20 queued jobs, then joins
        opener.join();
        QCOMPARE(ran.load(), 21);

        // A stopped lane starts again.
        lane.start();
        lane.post([&ran] { ran.fetch_add(1); });
        QVERIFY(lane.waitIdleForTest(kIdleTimeoutMs));
        QCOMPARE(ran.load(), 22);
        lane.stop();
    }

#ifdef HAVE_WDSP
    // ── Step 2: the caller check, with a real WDSP channel ─────────────

    void callerHookHearsEachCheckedEntryPoint()
    {
        // A raw hook (not WdspThreadCheck) proves dsplock.c and channel.c
        // report each kind on the calling thread, before their work.
        s_hookThread = QThread::currentThreadId();
        s_kindsSeen.fill(0);
        WDSPSetCallerCheckHook(&recordingHook);
        openChannel();
        double top = 0.0;
        GetRXAAGCTop(kChannel, &top);
        SetChannelState(kChannel, 1, 0);
        CloseChannel(kChannel);
        WDSPSetCallerCheckHook(nullptr);

        QVERIFY(s_kindsSeen[kWdspCallerOpenChannel] >= 1);
        QVERIFY(s_kindsSeen[kWdspCallerEnterCs] >= 1);
        QVERIFY(s_kindsSeen[kWdspCallerSetChannelState] >= 1);
        QVERIFY(s_kindsSeen[kWdspCallerWaitWorkerExit] >= 1);
        QVERIFY(s_dspLockEntries >= 1);   // GetRXAAGCTop's csDSP, channel 20

        // Removed: nothing more is heard.
        const int before = s_totalSeen.load();
        openChannel();
        GetRXAAGCTop(kChannel, &top);
        CloseChannel(kChannel);
        QCOMPARE(s_totalSeen.load(), before);
    }

    void checkCountsEventLoopCallsButNotLaneCalls()
    {
        QVERIFY(kWdspThreadCheckCompiled);   // a test build
        openChannel();
        DspControlThread lane(DspLane::Receive);
        lane.start();
        WdspThreadCheck::install(QThread::currentThread());
        QCOMPARE(WdspThreadCheck::eventLoopEntries(), quint64(0));

        double top = 0.0;
        GetRXAAGCTop(kChannel, &top);
        const quint64 afterDirect = WdspThreadCheck::eventLoopEntries();
        QVERIFY2(afterDirect >= 1, "a direct call from the event loop was not counted");

        lane.post([] {
            double value = 0.0;
            GetRXAAGCTop(kChannel, &value);
        });
        QVERIFY(lane.waitIdleForTest(kIdleTimeoutMs));
        QCOMPARE(WdspThreadCheck::eventLoopEntries(), afterDirect);

        SetChannelState(kChannel, 0, 1);
        const quint64 afterState = WdspThreadCheck::eventLoopEntries();
        QVERIFY2(afterState > afterDirect, "a direct SetChannelState was not counted");
        SetChannelState(kChannel, 1, 0);

        WdspThreadCheck::uninstall();
        const quint64 atUninstall = WdspThreadCheck::eventLoopEntries();
        GetRXAAGCTop(kChannel, &top);
        QCOMPARE(WdspThreadCheck::eventLoopEntries(), atUninstall);

        lane.stop();
        CloseChannel(kChannel);
    }

    void checkInstalledFromAnotherThreadCountsTheEventLoop()
    {
        openChannel();
        std::thread installer([] { WdspThreadCheck::install(QCoreApplication::instance()->thread()); });
        installer.join();
        deliverPostedEvents();   // the event loop learns its own thread ID
        double top = 0.0;
        GetRXAAGCTop(kChannel, &top);
        QVERIFY(WdspThreadCheck::eventLoopEntries() >= 1);
        WdspThreadCheck::uninstall();
        CloseChannel(kChannel);
    }

    void eventLoopStaysResponsiveWhileTheLaneWaitsOnASlowBlock()
    {
        openChannel();
        WDSPSetTestBlockDelayUs(kChannel, kSlowBlockUs);
        std::atomic<bool> stopFeeding{false};
        std::thread feeder([&stopFeeding] { feed(stopFeeding); });
        // Wait until the worker is inside its slow blocks.
        QTRY_VERIFY_WITH_TIMEOUT(workerBlocks() >= 1, 10000);

        DspControlThread lane(DspLane::Receive);
        lane.start();
        WdspThreadCheck::install(QThread::currentThread());

        constexpr int kJobs = kAgcTopCalls + 2 * kStatePairs;
        std::atomic<int> jobsRun{0};
        std::vector<double> postMs;
        postMs.reserve(kJobs);
        double worstGapMs = 0.0;
        int ticks = 0;
        bool posted = false;
        Clock::time_point lastTick = Clock::now();
        Clock::time_point postedAt{};
        Clock::time_point allRunAt{};
        QEventLoop loop;

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
            if (ticks == 5 && !posted) {
                posted = true;
                postedAt = now;
                // Every post happens here, on the event loop, in one tick.
                auto timedPost = [&](DspControlThread::Job job) {
                    const Clock::time_point start = Clock::now();
                    lane.post(std::move(job));
                    postMs.push_back(msBetween(start, Clock::now()));
                };
                for (int call = 0; call < kAgcTopCalls; ++call) {
                    timedPost([&jobsRun] {
                        double value = 0.0;
                        GetRXAAGCTop(kChannel, &value);
                        jobsRun.fetch_add(1);
                    });
                }
                for (int pair = 0; pair < kStatePairs; ++pair) {
                    timedPost([&jobsRun] {
                        SetChannelState(kChannel, 0, 1);
                        jobsRun.fetch_add(1);
                    });
                    timedPost([&jobsRun] {
                        SetChannelState(kChannel, 1, 0);
                        jobsRun.fetch_add(1);
                    });
                }
            }
            if (posted && jobsRun.load() == kJobs) {
                if (allRunAt == Clock::time_point{}) {
                    allRunAt = now;
                }
                // Keep measuring a little past the last job.
                if (msBetween(allRunAt, now) >= 100.0) {
                    loop.quit();
                }
            }
        });
        QTimer deadline;
        deadline.setSingleShot(true);
        connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        deadline.start(kIdleTimeoutMs);
        ticker.start();
        const Clock::time_point loopStart = Clock::now();
        loop.exec();
        const double loopMs = msBetween(loopStart, Clock::now());
        ticker.stop();
        const quint64 eventLoopCalls = WdspThreadCheck::eventLoopEntries();
        WdspThreadCheck::uninstall();

        stopFeeding.store(true);
        feeder.join();
        WDSPSetTestBlockDelayUs(kChannel, 0);
        QVERIFY(lane.waitIdleForTest(kIdleTimeoutMs));
        lane.stop();
        CloseChannel(kChannel);

        // The lane really waited on the slow worker while the timer ran
        // (at least half a block), so the gap bound means something.
        const double laneBusyMs = msBetween(postedAt, allRunAt);
        const double worstPostMs =
            postMs.empty() ? 0.0 : *std::max_element(postMs.begin(), postMs.end());
        qInfo("%d jobs run of %d in %.0f ms on the lane, over %.0f ms of event loop, %d ticks; "
              "worst post %.3f ms (limit %.1f); worst %d ms timer gap %.2f ms (limit %.1f); "
              "WDSP calls on the event loop %llu",
              jobsRun.load(), kJobs, laneBusyMs, loopMs, ticks, worstPostMs, kMaxPostMs,
              kTimerIntervalMs, worstGapMs, kMaxTimerGapMs,
              static_cast<unsigned long long>(eventLoopCalls));

        QCOMPARE(jobsRun.load(), kJobs);
        QCOMPARE(int(postMs.size()), kJobs);
        QVERIFY2(laneBusyMs >= kSlowBlockUs / 2000.0,
                 "the lane never waited on the slow DSP block");
        QVERIFY2(worstPostMs <= kMaxPostMs, "a post() took longer than 1 ms");
        QVERIFY2(worstGapMs <= kMaxTimerGapMs, "the event loop's 10 ms timer gapped");
        QCOMPARE(eventLoopCalls, quint64(0));
    }
#endif // HAVE_WDSP

private:
#ifdef HAVE_WDSP
    static void openChannel()
    {
        OpenChannel(kChannel, kInSize, kDspSize, kSampleRate, kSampleRate, kSampleRate,
                    0,       // type: RX
                    1,       // state: on
                    0.010, 0.025, 0.000, 0.010,
                    0);      // bfo off: fexchange2 never waits for output
    }

    static long long workerBlocks()
    {
        WdspChannelLoad load{};
        GetChannelDspLoad(kChannel, &load);
        return load.blocks + (load.currentBlockNs > 0 ? 1 : 0);
    }

    // Feeds the channel faster than real time, so its worker always has
    // its next (slow) block ready.
    static void feed(const std::atomic<bool>& stop)
    {
        std::array<float, kInSize> inI{}, inQ{}, outI{}, outQ{};
        const auto period = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(double(kInSize) / kSampleRate / 10.0));
        double phase = 0.0;
        const double step = 2.0 * std::numbers::pi * 1000.0 / kSampleRate;
        Clock::time_point next = Clock::now();
        while (!stop.load()) {
            for (int i = 0; i < kInSize; ++i) {
                inI[i] = static_cast<float>(0.01 * std::cos(phase));
                inQ[i] = static_cast<float>(0.01 * std::sin(phase));
                phase = std::fmod(phase + step, 2.0 * std::numbers::pi);
            }
            int error = 0;
            fexchange2(kChannel, inI.data(), inQ.data(), outI.data(), outQ.data(), &error);
            next += period;
            std::this_thread::sleep_until(next);
        }
    }

    static void recordingHook(int channel, int kind)
    {
        if (QThread::currentThreadId() != s_hookThread) {
            return;   // the channel's own threads also take locks
        }
        s_totalSeen.fetch_add(1);
        if (kind >= 0 && kind < int(s_kindsSeen.size())) {
            ++s_kindsSeen[std::size_t(kind)];
        }
        if (kind == kWdspCallerEnterCs && channel == kChannel) {
            ++s_dspLockEntries;
        }
    }

    static inline Qt::HANDLE s_hookThread = nullptr;
    static inline std::array<int, 8> s_kindsSeen{};
    static inline std::atomic<int> s_totalSeen{0};
    static inline int s_dspLockEntries = 0;
#endif
};

QTEST_GUILESS_MAIN(TestDspControlThread)
#include "tst_dsp_control_thread.moc"
