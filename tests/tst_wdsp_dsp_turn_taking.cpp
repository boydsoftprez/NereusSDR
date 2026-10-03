// no-port-check: NereusSDR-original linked-WDSP integration test. It drives a
// real RX channel to prove control calls get a bounded turn at the channel's
// DSP lock while the DSP worker is overloaded (R-R3-39), and that the
// worker's per-block load counters measure that overload (R-R3-40).
#include <QtTest>
#include "RealtimeTestLoad.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <thread>
#include <vector>

#ifndef Q_OS_WIN
#include <time.h>
#endif
#ifdef Q_OS_MAC
#include <pthread/qos.h>
#endif

#include "core/wdsp_api.h"

namespace {

using Clock = std::chrono::steady_clock;

// A WDSP channel id no application object uses in this process.
constexpr int kChannel = 20;
constexpr int kSampleRate = 48000;
constexpr int kInSize = 1024;
constexpr int kDspSize = 4096;

// Simulated per-block DSP overload, busy-waited inside the worker's csDSP.
constexpr int kBlockDelayUs = 20000;
// Feed input ten times faster than real time with bfo off, so the worker
// always has its next block ready and never idles.
constexpr int kFeedSpeedup = 10;

constexpr int kSingleCalls = 50;
// Not a multiple of the block time, so calls land at every block phase.
constexpr std::chrono::milliseconds kSingleCallSpacing{37};
constexpr int kBurstCalls = 14;

constexpr std::chrono::milliseconds kWarmup{500};
constexpr std::chrono::milliseconds kBaselineWindow{2000};

// Acceptance bounds. Block time is wall clock, so on a machine busy with
// other work every block (and every wait for one) stretches; fixed
// millisecond limits then fail for reasons that have nothing to do with the
// lock. The bounds are therefore counted in worker blocks, read from the
// worker's own counters around each call, and the millisecond limits are
// built from the longest block measured in the same window.
//
// min(block period / 4, 20 ms): the worker's per-block hold-off budget.
constexpr double kBudgetMs = std::min(1000.0 * kDspSize / kSampleRate / 4.0, 20.0);
// With turn-taking a control call waits for the block in progress, and for
// one more only if its thread was not scheduled within the worker's
// hold-off budget. Without it a call waited for 9 to 15 blocks here.
constexpr long long kMaxBlocksPerCall = 2;
// A burst stays together through the 1 ms grace; a burst whose thread is
// descheduled longer than that between two calls waits one more block. The
// burst waited about 6 blocks without turn-taking.
constexpr long long kMaxBlocksPerBurst = 3;
// Scheduling slack on top of the block-derived millisecond limits.
constexpr double kSchedulingMarginMs = 10.0;
// Throughput cost of control calls, as the worker's utilization: the share
// of wall time it spent inside blocks (busy time from its own counters over
// the window). The worker has a backlog, so without control calls it runs
// blocks back to back; the only time turn-taking takes from it is the time
// it holds off for waiters. Output rate itself is not compared across the
// two windows: on a busy machine the worker is preempted inside its blocks
// by different amounts in each window, which moves blocks per second
// without any holding off (seen: 0.69 at load 24 with every call served
// after one block). Preemption inside a block counts as busy time in both
// windows, so utilization does not move with it.
//
// The time lost may be 10% of the window (the plan's 90% throughput), or,
// on a busy machine, the time the worker actually held off for the control
// calls, whichever is more. That hold-off is measured in the same run from
// the worker's own counters, for every call: during the call, the call's
// duration less the block time that elapsed in it (the waiting thread
// waking, taking the lock and making the call); after the call, the time
// until the worker's next block started (the 1 ms burst grace plus however
// long the worker took to wake and retake the lock), read from the start
// time the worker publishes for its block in progress. A further 2% covers
// the two windows' edges (a block counted in one window may have started in
// the other; baseline utilization reads 0.99 to 1.01). Under a heavy build
// both parts stretch from about a millisecond to several, which a fixed 10%
// cannot allow for; seen: 0.897 at load 35 with every call served after one
// block. Because that allowance grows with what the worker held off, the
// hold-off after a call is also checked on its own: its median must stay
// within the 1 ms burst grace plus the scheduling margin (or the same run's
// idle grace median plus that margin, below), so a worker that holds off
// longer than the calls need (up to its whole budget, say) fails.
constexpr double kWindowEdgeShare = 0.02;
constexpr double kBurstGraceMs = 1.0;
// JJ's transmit-twin ruling (applied in tst_dsp_control_receive, 745aca33e)
// for a fixed wall-clock bound that load stretches: the median hold-off
// after a call may be no more than the larger of kBurstGraceMs +
// kSchedulingMarginMs and the same run's idle median plus
// kSchedulingMarginMs. The hold-off after a call is the worker's burst
// grace (dsplock.c worker_defer: kDspWorkerBurstGraceUs, checked between
// kDspWorkerPauseNs nanosleeps) plus however late the worker thread wakes
// from those sleeps; on a busy computer the wake alone takes milliseconds
// (median 13.01 ms against 11.0 at load 97, and failing alone at load 24
// to 41). The idle probe times the same grace wait (after the same block
// of work, with the same pause and QoS class on macOS) on a thread of this
// process during the baseline
// window, when the worker runs blocks and nothing asks for the lock: a
// median against a median, so a worker that holds off its whole 20 ms
// budget still fails on a computer that wakes a thread in under 10 ms.
constexpr int kIdleProbes = kSingleCalls;
constexpr std::chrono::milliseconds kPostCallWatch{60};
constexpr double kMinThroughputRatio = 0.90;

// Load counters (R-R3-40): block period is dsp_size / dsp_rate.
constexpr int kBlockPeriodUs = static_cast<int>(1000000LL * kDspSize / kSampleRate);
// A delay that is a large share of the period, so the chain's own work is a
// small part of each measured block.
constexpr int kLoadDelayUs = 40000;
// A delay longer than the period: every block is late.
constexpr int kLateDelayUs = 100000;
constexpr std::chrono::milliseconds kLoadWindow{2000};
// The measured load must lie between delay / period (every block contains
// the busy-wait, so it can only read higher, when the worker is preempted
// inside it) and the load the window itself allows: the worker has a
// backlog, so its blocks run back to back and cannot add up to more than the
// window, i.e. mean block <= window / blocks. Both within 10%.
constexpr double kLoadTolerance = 0.10;

double msSince(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

} // namespace

class TestWdspDspTurnTaking : public QObject {
    Q_OBJECT

private slots:
    // The load when a real-time case failed (R-R3-21, R-R3-40).
    void cleanup() { NereusSDR::RealtimeTestLoad::printLoadAverageIfFailed(); }

    void initTestCase()
    {
        OpenChannel(kChannel, kInSize, kDspSize, kSampleRate, kSampleRate, kSampleRate,
                    0,       // type: RX
                    1,       // state: on
                    0.010, 0.025, 0.000, 0.010,
                    0);      // bfo off: fexchange2 never waits for output
        WDSPSetTestBlockDelayUs(kChannel, kBlockDelayUs);
        m_feeder = std::thread([this] { feed(); });
    }

    void cleanupTestCase()
    {
        WDSPSetTestBlockDelayUs(kChannel, 0);
        m_stop.store(true);
        if (m_feeder.joinable()) {
            m_feeder.join();
        }
        // Let the worker drain its backlog at full speed before the channel
        // is torn down, so teardown does not wait out a queue of blocks.
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        CloseChannel(kChannel);
    }

    void controlCallsGetABoundedTurnWhileTheWorkerIsOverloaded()
    {
        std::this_thread::sleep_for(kWarmup);

        // Baseline: worker throughput with no control calls.
        // The same run's idle hold-off, timed during the baseline window.
        double baselineUtilization = 0.0;
        std::vector<double> idleGraceMs;
        const double baselineRate = outputRateOver([&idleGraceMs] {
            const auto windowEnd = Clock::now() + kBaselineWindow;
            idleGraceMs = idleGraceWaits();
            std::this_thread::sleep_until(windowEnd);
        }, baselineUtilization);

        double worstSingleMs = 0.0;
        long long worstSingleBlocks = 0;
        double burstMs = 0.0;
        long long burstBlocks = 0;
        double handoffMs = 0.0;
        std::vector<double> afterCallMs;
        // Starts the interval whose longest block the limits are built from.
        (void)TakeChannelDspIntervalMaxBlockUs(kChannel);
        double loadedUtilization = 0.0;
        const double loadedRate = outputRateOver([&] {
            for (int call = 0; call < kSingleCalls; ++call) {
                double top = 0.0;
                const WdspChannelLoad before = currentLoad();
                const auto start = Clock::now();
                GetRXAAGCTop(kChannel, &top);
                const double callMs = msSince(start);
                const WdspChannelLoad after = currentLoad();
                worstSingleMs = std::max(worstSingleMs, callMs);
                worstSingleBlocks = std::max(worstSingleBlocks, after.blocks - before.blocks);
                handoffMs += handoffDuring(before, after, callMs);
                const auto callEnd = start + std::chrono::duration_cast<Clock::duration>(
                                                 std::chrono::duration<double, std::milli>(callMs));
                afterCallMs.push_back(holdOffAfter(callEnd, after.blocks));
                handoffMs += afterCallMs.back();
                std::this_thread::sleep_until(callEnd + kSingleCallSpacing);
            }
            const WdspChannelLoad before = currentLoad();
            const auto start = Clock::now();
            for (int call = 0; call < kBurstCalls; ++call) {
                double value = 0.0;
                if (call % 2 == 0) {
                    GetRXAAGCTop(kChannel, &value);
                } else {
                    GetRXAAGCThresh(kChannel, &value, 4096.0, kSampleRate);
                }
            }
            burstMs = msSince(start);
            const WdspChannelLoad after = currentLoad();
            burstBlocks = after.blocks - before.blocks;
            handoffMs += handoffDuring(before, after, burstMs);
            handoffMs += holdOffAfter(Clock::now(), after.blocks);
            std::this_thread::sleep_for(kSingleCallSpacing);
        }, loadedUtilization, &m_loadedWindowMs);
        const double longestBlockMs = TakeChannelDspIntervalMaxBlockUs(kChannel) / 1000.0;
        const double singleLimitMs =
            kMaxBlocksPerCall * longestBlockMs + kBudgetMs + kSchedulingMarginMs;
        const double burstLimitMs =
            kMaxBlocksPerBurst * longestBlockMs + kBudgetMs + kSchedulingMarginMs;

        const double ratio =
            baselineUtilization > 0.0 ? loadedUtilization / baselineUtilization : 0.0;
        std::nth_element(afterCallMs.begin(),
                         afterCallMs.begin() + afterCallMs.size() / 2, afterCallMs.end());
        const double medianAfterCallMs = afterCallMs[afterCallMs.size() / 2];
        QCOMPARE(static_cast<int>(idleGraceMs.size()), kIdleProbes);
        std::nth_element(idleGraceMs.begin(),
                         idleGraceMs.begin() + idleGraceMs.size() / 2, idleGraceMs.end());
        const double idleMedianMs = idleGraceMs[idleGraceMs.size() / 2];
        const double afterCallLimitMs = std::max(kBurstGraceMs + kSchedulingMarginMs,
                                                 idleMedianMs + kSchedulingMarginMs);
        const double minRatio = std::min(
            kMinThroughputRatio, 1.0 - handoffMs / m_loadedWindowMs - kWindowEdgeShare);
        qInfo("longest block %.2f ms; worst single call %.2f ms (limit %.1f), %lld block(s) "
              "(max %lld); %d-call burst %.2f ms (limit %.1f), %lld block(s) (max %lld); "
              "worker %.1f vs baseline %.1f outputs/s; utilization %.3f vs baseline %.3f, "
              "ratio %.3f (min %.3f; worker held off %.2f ms of %.0f ms; median after a "
              "call %.2f ms, limit %.1f: idle grace median %.2f ms over %d waits)",
              longestBlockMs, worstSingleMs, singleLimitMs, worstSingleBlocks,
              kMaxBlocksPerCall, kBurstCalls, burstMs, burstLimitMs, burstBlocks,
              kMaxBlocksPerBurst, loadedRate, baselineRate, loadedUtilization,
              baselineUtilization, ratio, minRatio, handoffMs, m_loadedWindowMs,
              medianAfterCallMs, afterCallLimitMs, idleMedianMs, kIdleProbes);

        QVERIFY(baselineRate > 0.0);
        QVERIFY(longestBlockMs > 0.0);
        QVERIFY2(worstSingleBlocks <= kMaxBlocksPerCall,
                 "a single control call waited for too many worker blocks");
        QVERIFY2(burstBlocks <= kMaxBlocksPerBurst,
                 "the control-call burst waited for too many worker blocks");
        QVERIFY2(worstSingleMs <= singleLimitMs, "a single control call waited too long");
        QVERIFY2(burstMs <= burstLimitMs, "the control-call burst waited too long");
        // The median hold-off is the real guard against a worker that
        // over-defers to control calls. The throughput ratio below proves
        // little on its own: its minimum is derived from the hold-off this
        // same run measured, so a worker that holds off its whole budget
        // lowers its own bar and still passes (seen: ratio 0.651 against a
        // minimum of 0.628). Keep this check even if the ratio is loosened.
        QVERIFY2(medianAfterCallMs <= afterCallLimitMs,
                 "the worker held off too long after control calls finished");
        QVERIFY2(ratio >= minRatio, "control calls cost the worker too much throughput");
    }

    void readerRejectsABadChannelOrOutput()
    {
        WdspChannelLoad load{};
        QCOMPARE(GetChannelDspLoad(-1, &load), -1);
        QCOMPARE(GetChannelDspLoad(1000, &load), -1);
        QCOMPARE(GetChannelDspLoad(kChannel, nullptr), -1);
    }

    void measuredLoadMatchesTheBlockDelay()
    {
        WDSPSetTestBlockDelayUs(kChannel, kLoadDelayUs);
        // Let any block started under the previous delay finish.
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        WdspChannelLoad before{};
        QCOMPARE(GetChannelDspLoad(kChannel, &before), 0);
        double worstReadMs = 0.0;
        const auto windowStart = Clock::now();
        while (Clock::now() - windowStart < kLoadWindow) {
            WdspChannelLoad sample{};
            const auto start = Clock::now();
            QCOMPARE(GetChannelDspLoad(kChannel, &sample), 0);
            worstReadMs = std::max(worstReadMs, msSince(start));
            std::this_thread::sleep_for(std::chrono::milliseconds(7));
        }
        WdspChannelLoad after{};
        QCOMPARE(GetChannelDspLoad(kChannel, &after), 0);
        const double windowUs = msSince(windowStart) * 1000.0;

        const long long blocks = after.blocks - before.blocks;
        QVERIFY2(blocks > 0, "the worker completed no blocks in the window");
        const double meanBlockUs = (after.busyNs - before.busyNs) / 1000.0 / blocks;
        const double load = meanBlockUs / after.blockPeriodUs;
        const double floor = double(kLoadDelayUs) / kBlockPeriodUs;
        const double ceiling = windowUs / blocks / after.blockPeriodUs;
        qInfo("%lld blocks, mean block %.1f us, period %d us: load %.3f; delay / period %.3f, "
              "window / blocks / period %.3f; worst read %.3f ms",
              blocks, meanBlockUs, after.blockPeriodUs, load, floor, ceiling, worstReadMs);

        QCOMPARE(after.blockPeriodUs, kBlockPeriodUs);
        QVERIFY2(load >= floor * (1.0 - kLoadTolerance),
                 "measured load is below the block delay / block period");
        QVERIFY2(load <= ceiling * (1.0 + kLoadTolerance),
                 "measured load exceeds the time the worker had in the window");
        QVERIFY(after.maxBlockUs >= kLoadDelayUs);
        // Late blocks are not checked here: a 40 ms block preempted for more
        // than 45 ms is honestly late on a busy machine.
        // blocksLongerThanThePeriodCountAsLate covers the late counter.
        // A read that waited for the worker's lock would take up to a whole
        // block (at least the 40 ms delay); an unlocked read takes
        // microseconds even when this thread is briefly preempted.
        QVERIFY2(worstReadMs < kLoadDelayUs / 1000.0 / 2.0,
                 "reading the load waited for the worker");
    }

    void blocksLongerThanThePeriodCountAsLate()
    {
        WDSPSetTestBlockDelayUs(kChannel, kLateDelayUs);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        WdspChannelLoad before{};
        QCOMPARE(GetChannelDspLoad(kChannel, &before), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        WdspChannelLoad after{};
        QCOMPARE(GetChannelDspLoad(kChannel, &after), 0);
        WDSPSetTestBlockDelayUs(kChannel, kBlockDelayUs);

        const long long blocks = after.blocks - before.blocks;
        const long long late = after.lateBlocks - before.lateBlocks;
        qInfo("%lld blocks, %lld late, longest %lld us", blocks, late, after.maxBlockUs);
        QVERIFY(blocks > 0);
        QCOMPARE(late, blocks);
        QVERIFY(after.maxBlockUs >= kLateDelayUs);
    }

    // The reader sees a block that has not finished yet, and the interval
    // maximum covers only blocks since the previous take (R-R3-40).
    void aBlockInProgressAndTheIntervalMaximumAreVisible()
    {
        WDSPSetTestBlockDelayUs(kChannel, kLateDelayUs);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        // Starts the interval this case measures.
        QVERIFY(TakeChannelDspIntervalMaxBlockUs(kChannel) >= 0);

        long long longestInProgressNs = 0;
        const auto windowStart = Clock::now();
        while (Clock::now() - windowStart < std::chrono::milliseconds(1000)) {
            WdspChannelLoad sample{};
            QCOMPARE(GetChannelDspLoad(kChannel, &sample), 0);
            longestInProgressNs = std::max(longestInProgressNs, sample.currentBlockNs);
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
        const long long intervalMaxUs = TakeChannelDspIntervalMaxBlockUs(kChannel);

        // Short blocks now; the next interval must not carry the long ones.
        WDSPSetTestBlockDelayUs(kChannel, kBlockDelayUs);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        (void)TakeChannelDspIntervalMaxBlockUs(kChannel);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const long long shortIntervalMaxUs = TakeChannelDspIntervalMaxBlockUs(kChannel);

        qInfo("longest block seen in progress %.1f ms; interval maximum %lld us, "
              "then %lld us with %d us blocks",
              longestInProgressNs / 1e6, intervalMaxUs, shortIntervalMaxUs, kBlockDelayUs);
        QVERIFY2(longestInProgressNs >= kLateDelayUs * 1000LL / 2,
                 "the reader never saw a block in progress");
        QVERIFY(intervalMaxUs >= kLateDelayUs);
        QVERIFY(shortIntervalMaxUs >= kBlockDelayUs);
        // Short blocks stretch under load as the long ones did, so compare
        // with the long interval's own maximum, not a fixed number.
        QVERIFY2(shortIntervalMaxUs < intervalMaxUs,
                 "the interval maximum kept a block from an earlier interval");
        QCOMPARE(TakeChannelDspIntervalMaxBlockUs(-1), -1LL);
    }

private:
    static WdspChannelLoad currentLoad()
    {
        WdspChannelLoad load{};
        GetChannelDspLoad(kChannel, &load);
        return load;
    }

    // How long after callEnd the worker started its next block (it holds off
    // for the burst grace after the call, then retakes the lock). Read from
    // the start the worker publishes for its block in progress; 0 if the
    // block was missed, kPostCallWatch if none started in that time.
    static double holdOffAfter(Clock::time_point callEnd, long long blocksAtEnd)
    {
        const auto deadline = callEnd + kPostCallWatch;
        while (Clock::now() < deadline) {
            const auto readAt = Clock::now();
            const WdspChannelLoad load = currentLoad();
            if (load.blocks != blocksAtEnd) {
                return 0.0;
            }
            if (load.currentBlockNs > 0) {
                const auto blockStart = readAt - std::chrono::nanoseconds(load.currentBlockNs);
                return std::max(0.0, std::chrono::duration<double, std::milli>(
                                         blockStart - callEnd).count());
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        return std::chrono::duration<double, std::milli>(kPostCallWatch).count();
    }

    // kIdleProbes waits shaped as the worker's burst grace after a call:
    // busy for kBlockDelayUs as a block is, then nanosleep the worker's own
    // pause (WDSPGetTestWorkerPauseNs, dsplock.c kDspWorkerPauseNs)
    // (a yield on Windows, as the worker does) until kBurstGraceMs has
    // passed, on a thread with the worker's QoS class (linux_port.c starts
    // RX workers QOS_CLASS_USER_INTERACTIVE on macOS). A probe that only
    // sleeps is not like the worker: under 150 spinners it woke in 1.03 ms
    // while the worker's hold-off ran 12 to 18 ms; with the block's work
    // first it tracked the worker (11.5 to 21 ms against 13.8 to 22 ms).
    // Each is how long the wait took, grace included.
    static std::vector<double> idleGraceWaits()
    {
        std::vector<double> waits;
        waits.reserve(kIdleProbes);
        std::thread probe([&waits] {
#ifdef Q_OS_MAC
            pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
            for (int i = 0; i < kIdleProbes; ++i) {
                // A block's worth of work first, as the worker does before
                // it holds off: the scheduler treats a thread that has just
                // used its time slice differently from one that slept.
                const auto blockStart = Clock::now();
                while (Clock::now() - blockStart < std::chrono::microseconds(kBlockDelayUs)) {
                    // busy-wait, as WDSPSetTestBlockDelayUs does in the worker
                }
                const auto start = Clock::now();
                while (msSince(start) < kBurstGraceMs) {
#ifdef Q_OS_WIN
                    std::this_thread::yield();   // dsplock.c: SwitchToThread on Windows
#else
                    const timespec pause{0, WDSPGetTestWorkerPauseNs()};
                    nanosleep(&pause, nullptr);
#endif
                }
                waits.push_back(msSince(start));
            }
        });
        probe.join();
        return waits;
    }

    // The part of a control call's wait (callMs) the worker spent outside
    // its blocks: the call's duration minus the block time that elapsed
    // during it (the blocks it completed, less what the first had already
    // run when the call began). This is the hand-off the worker held off
    // for; on an idle machine it is microseconds.
    static double handoffDuring(const WdspChannelLoad& before,
                                const WdspChannelLoad& after, double callMs)
    {
        const double blockMsDuringCall =
            (after.busyNs - before.busyNs - before.currentBlockNs) / 1e6;
        return std::max(0.0, callMs - std::max(0.0, blockMsDuringCall));
    }

    // Runs body and returns the worker's output rate (successful fexchange2
    // outputs per second) over that time. Each worker block yields a fixed
    // number of outputs, so this tracks worker blocks per second. Also sets
    // utilization: the worker's busy time over the same wall time.
    template <typename Body>
    double outputRateOver(Body body, double& utilization, double* windowMs = nullptr)
    {
        WdspChannelLoad loadBefore{};
        GetChannelDspLoad(kChannel, &loadBefore);
        const long long before = m_outputs.load();
        const auto start = Clock::now();
        body();
        const double seconds = msSince(start) / 1000.0;
        if (windowMs != nullptr) {
            *windowMs = seconds * 1000.0;
        }
        WdspChannelLoad loadAfter{};
        GetChannelDspLoad(kChannel, &loadAfter);
        utilization = (loadAfter.busyNs - loadBefore.busyNs) / 1e9 / seconds;
        return static_cast<double>(m_outputs.load() - before) / seconds;
    }

    void feed()
    {
        std::array<float, kInSize> inI{}, inQ{}, outI{}, outQ{};
        const auto period = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(double(kInSize) / kSampleRate / kFeedSpeedup));
        double phase = 0.0;
        const double step = 2.0 * std::numbers::pi * 1000.0 / kSampleRate;
        auto next = Clock::now();
        while (!m_stop.load()) {
            for (int i = 0; i < kInSize; ++i) {
                inI[i] = static_cast<float>(0.01 * std::cos(phase));
                inQ[i] = static_cast<float>(0.01 * std::sin(phase));
                phase = std::fmod(phase + step, 2.0 * std::numbers::pi);
            }
            int error = 0;
            fexchange2(kChannel, inI.data(), inQ.data(), outI.data(), outQ.data(), &error);
            if (error == 0) {
                m_outputs.fetch_add(1);
            }
            next += period;
            std::this_thread::sleep_until(next);
        }
    }

    double m_loadedWindowMs{1.0};
    std::thread m_feeder;
    std::atomic<bool> m_stop{false};
    std::atomic<long long> m_outputs{0};
};

QTEST_MAIN(TestWdspDspTurnTaking)
#include "tst_wdsp_dsp_turn_taking.moc"
