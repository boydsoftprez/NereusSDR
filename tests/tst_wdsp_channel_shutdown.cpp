// no-port-check: NereusSDR-original linked-WDSP integration test. It drives a
// real RX channel to prove channel teardown and rebuilds wait for a slow DSP
// worker to leave its loop before freeing its memory (R-R3-39).
//
// Where each case holds the worker (the two test seams in dsplock.c):
// - "...WaitsForABusyWorker" and "...LongerThanTheLogInterval" use
//   WDSPSetTestBlockDelayUs, which busy-waits right after the worker takes
//   csDSP and BEFORE its exec_bypass check. A teardown that lands during that
//   delay sets exec_bypass first, so the worker then skips dexchange and the
//   receive chain: these cases prove the wait, not teardown during DSP work.
// - "...DuringAProcessedBlock" use WDSPSetTestProcessDelayUs, which
//   busy-waits AFTER the exec_bypass check and before dexchange, so the
//   worker goes on into dexchange and xrxa after teardown has started. They
//   cover the path where teardown used to clear run without csDSP and the
//   worker then left through dexchange's _endthread while holding csDSP,
//   never counting its exit, so teardown waited forever.
// Every teardown runs under a watchdog, so a hang reports instead of hanging
// the test. A teardown that never returns still owns its channel, so once a
// watchdog fires the remaining cases fail at once instead of reusing it.
// - idleWorkerExitsAtOnce covers a worker waiting for input.
// - teardownOfAWorkerThatNeverStartedDoesNotWait covers start_thread's
//   failure path through dsplock.c directly (a failed _beginthread cannot be
//   forced here), then proves the channel's exit pairing is intact.
#include <QtTest>
#include "RealtimeTestLoad.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "core/wdsp_api.h"

// WDSP-internal (third_party/wdsp/src/dsplock.h); the test links wdsp_static,
// so it can drive the start-result path directly.
extern "C" {
void WdspWorkerStarted(int channel, int started);
void WdspWaitWorkerExit(int channel);
}

namespace {

using Clock = std::chrono::steady_clock;

// A WDSP channel id no application object (or other test) uses.
constexpr int kChannel = 21;
constexpr int kSampleRate = 48000;
constexpr int kInSize = 1024;
constexpr int kDspSize = 4096;

// Simulated per-block DSP overload, busy-waited inside the worker's csDSP.
// Longer than the 25 ms teardown used to allow.
constexpr int kBlockDelayUs = 60000;
// Input buffers queued up front: six worker blocks at 48 kHz (four input
// buffers per 4096-sample block), so the worker always has its next block.
constexpr int kQueuedInputBuffers = 24;
// After the worker lets a control call through between blocks, it starts
// its next block within the 1 ms burst grace. Waiting this long puts the
// teardown call well inside that 60 ms block.
constexpr std::chrono::milliseconds kIntoBlock{5};

// A block longer than teardown's 2000 ms log interval: teardown must still
// wait it out (and log once while it does).
constexpr int kLongBlockDelayUs = 2500000;
constexpr double kLongBlockMinCloseMs = 2000.0;
// That close takes about two long blocks: the pre-bypass delay runs once
// more before the worker sees exec_bypass (about 5 s measured). Its watchdog
// allows three.
constexpr std::chrono::milliseconds kLongBlockWatchdog{3 * kLongBlockDelayUs / 1000};

// An idle worker must exit at once. The old teardown slept a fixed 25 ms.
// The bound is on teardown's wait for the worker alone
// (WDSPGetTestLastWorkerExitWaitUs), not the whole CloseChannel, which also
// joins the flush thread through destroy_iobuffs' Sleep(1) loop (a 15.6 ms
// tick on Windows). An idle worker's exit is one wake-up of that worker: the
// semaphore release wakes it, it passes its loop once and counts its exit.
// So the wait may take the worst wake-up of this same worker measured just
// before, in the same run (input released one block to it; the delay until
// the block started is read from the start time the worker publishes), plus
// teardown's poll step after its fine-poll phase (a Sleep(1): one timer
// tick), plus a small margin. Unloaded that is a few ms, far under the old
// fixed 25 ms; under a heavy build it grows with the measured wake-up, not
// by guesswork (a separate thread handoff measured 0.02 ms while this
// worker took 2.1 ms at load 30, so the worker itself is measured).
constexpr int kWakeupSamples = 10;
constexpr int kIdleCloses = 5;
// Long enough that the reader sees the block in progress.
constexpr int kWakeupBlockDelayUs = 5000;
// Input buffers per worker block at these sizes (kDspSize / kInSize).
constexpr int kBuffersPerBlock = kDspSize / kInSize;
#ifdef Q_OS_WIN
constexpr double kPollStepMs = 16.0;
#else
constexpr double kPollStepMs = 1.0;
#endif
constexpr double kIdleExitMarginMs = 2.0;
constexpr std::chrono::milliseconds kIdleSettle{100};

// A teardown that has not returned by now is hung, not slow: the longest
// legitimate wait in the other cases is a few 60 ms blocks.
constexpr std::chrono::milliseconds kTeardownWatchdog{5000};

// A second channel id, for the never-started worker case.
constexpr int kUnstartedChannel = 22;

double msSince(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// True while kChannel is open and a teardown can still run on it.
bool g_channelOpen = false;
// Set when a teardown did not return within the watchdog. Its thread still
// owns the channel, so no later case may open or close it.
bool g_teardownHung = false;

void openChannel()
{
    g_channelOpen = true;
    OpenChannel(kChannel, kInSize, kDspSize, kSampleRate, kSampleRate, kSampleRate,
                0,       // type: RX
                1,       // state: on
                0.010, 0.025, 0.000, 0.010,
                0);      // bfo off: fexchange2 never waits for output
}

// Queues a backlog of worker blocks, then returns just after the worker has
// started one of them with the given test delay in force.
void putTheWorkerInsideABlock(int blockDelayUs = kBlockDelayUs)
{
    WDSPSetTestBlockDelayUs(kChannel, blockDelayUs);
    std::array<float, kInSize> inI{}, inQ{}, outI{}, outQ{};
    for (int i = 0; i < kInSize; ++i) {
        inI[i] = 0.01f;
    }
    for (int buffer = 0; buffer < kQueuedInputBuffers; ++buffer) {
        int error = 0;
        fexchange2(kChannel, inI.data(), inQ.data(), outI.data(), outQ.data(), &error);
    }
    // Takes csDSP, so it returns between two of the worker's blocks.
    double top = 0.0;
    GetRXAAGCTop(kChannel, &top);
    std::this_thread::sleep_for(kIntoBlock);
}

// As putTheWorkerInsideABlock, but the delay runs inside a processed block,
// after the worker's exec_bypass check.
void putTheWorkerInsideAProcessedBlock()
{
    WDSPSetTestProcessDelayUs(kChannel, kBlockDelayUs);
    putTheWorkerInsideABlock(0);
}

// Worst time, over kWakeupSamples tries, from releasing one block of input
// to kChannel's idle worker to that worker starting the block: the wake-up
// an idle worker's exit needs, on this machine under its current load.
double worstWorkerWakeupMs()
{
    std::array<float, kInSize> inI{}, inQ{}, outI{}, outQ{};
    WDSPSetTestBlockDelayUs(kChannel, kWakeupBlockDelayUs);
    double worst = 0.0;
    for (int sample = 0; sample < kWakeupSamples; ++sample) {
        WdspChannelLoad before{};
        GetChannelDspLoad(kChannel, &before);
        int error = 0;
        for (int buffer = 0; buffer + 1 < kBuffersPerBlock; ++buffer) {
            fexchange2(kChannel, inI.data(), inQ.data(), outI.data(), outQ.data(), &error);
        }
        // The last buffer of the block releases the worker.
        fexchange2(kChannel, inI.data(), inQ.data(), outI.data(), outQ.data(), &error);
        const auto released = Clock::now();
        const auto deadline = released + std::chrono::milliseconds(500);
        while (Clock::now() < deadline) {
            const auto readAt = Clock::now();
            WdspChannelLoad load{};
            GetChannelDspLoad(kChannel, &load);
            if (load.currentBlockNs > 0) {
                const auto started = readAt - std::chrono::nanoseconds(load.currentBlockNs);
                worst = std::max(worst, std::max(0.0, std::chrono::duration<double, std::milli>(
                                                          started - released).count()));
                break;
            }
            if (load.blocks != before.blocks) {
                break;   // missed it; the delay makes that rare
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        // Let the block finish and the worker go idle again.
        std::this_thread::sleep_for(std::chrono::microseconds(kWakeupBlockDelayUs)
                                    + std::chrono::milliseconds(20));
    }
    WDSPSetTestBlockDelayUs(kChannel, 0);
    return worst;
}

// Runs teardown on its own thread. Returns false if it has not returned
// within the watchdog; the channel is then left to the stuck thread.
template <typename Teardown>
bool runUnderWatchdog(Teardown teardown, double& elapsedMs,
                      std::chrono::milliseconds watchdog = kTeardownWatchdog)
{
    std::packaged_task<void()> task(teardown);
    std::future<void> done = task.get_future();
    const auto start = Clock::now();
    std::thread runner(std::move(task));
    if (done.wait_for(watchdog) != std::future_status::ready) {
        elapsedMs = msSince(start);
        runner.detach();
        g_channelOpen = false;
        g_teardownHung = true;
        return false;
    }
    runner.join();
    elapsedMs = msSince(start);
    return true;
}

// CloseChannel(channel) under the watchdog.
bool closeUnderWatchdog(int channel, double& elapsedMs,
                        std::chrono::milliseconds watchdog = kTeardownWatchdog)
{
    return runUnderWatchdog([channel] { CloseChannel(channel); }, elapsedMs, watchdog);
}

} // namespace

class TestWdspChannelShutdown : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        if (g_teardownHung) {
            QFAIL("An earlier teardown never returned and still owns the channel.");
        }
    }

    void cleanup()
    {
        // The load when a real-time case failed (R-R3-21, R-R3-40).
        NereusSDR::RealtimeTestLoad::printLoadAverageIfFailed();
        WDSPSetTestBlockDelayUs(kChannel, 0);
        WDSPSetTestProcessDelayUs(kChannel, 0);
        // A case that failed before its own CloseChannel must not leave the
        // channel open for the next case's OpenChannel.
        if (g_channelOpen) {
            // The longest watchdog: the case may have left a long block running.
            double closeMs = 0.0;
            closeUnderWatchdog(kChannel, closeMs, kLongBlockWatchdog);
            g_channelOpen = false;
        }
    }

    void closeChannelDuringAProcessedBlock()
    {
        openChannel();
        putTheWorkerInsideAProcessedBlock();

        const int exitsBefore = WDSPGetTestWorkerExitCount(kChannel);
        double closeMs = 0.0;
        const bool returned = closeUnderWatchdog(kChannel, closeMs);
        const int exitsAtReturn = WDSPGetTestWorkerExitCount(kChannel);
        qInfo("CloseChannel during a processed block took %.2f ms; worker exits %d -> %d",
              closeMs, exitsBefore, exitsAtReturn);
        QVERIFY2(returned, "CloseChannel during a processed block never returned");
        g_channelOpen = false;
        QVERIFY2(exitsAtReturn == exitsBefore + 1,
                 "CloseChannel returned without the worker leaving its loop");
    }

    void inputSamplerateRebuildDuringAProcessedBlock()
    {
        openChannel();
        putTheWorkerInsideAProcessedBlock();

        const int exitsBefore = WDSPGetTestWorkerExitCount(kChannel);
        double rebuildMs = 0.0;
        const bool returned = runUnderWatchdog(
            [] { SetInputSamplerate(kChannel, 2 * kSampleRate); }, rebuildMs);
        const int exitsAtReturn = WDSPGetTestWorkerExitCount(kChannel);
        qInfo("SetInputSamplerate during a processed block took %.2f ms; worker exits %d -> %d",
              rebuildMs, exitsBefore, exitsAtReturn);
        QVERIFY2(returned, "SetInputSamplerate during a processed block never returned");
        QVERIFY2(exitsAtReturn == exitsBefore + 1,
                 "SetInputSamplerate rebuilt without the old worker leaving its loop");

        WDSPSetTestProcessDelayUs(kChannel, 0);
        double closeMs = 0.0;
        QVERIFY2(closeUnderWatchdog(kChannel, closeMs), "CloseChannel never returned");
        g_channelOpen = false;
        QCOMPARE(WDSPGetTestWorkerExitCount(kChannel), exitsBefore + 2);
    }

    void closeChannelWaitsForABusyWorker()
    {
        openChannel();
        putTheWorkerInsideABlock();

        const int exitsBefore = WDSPGetTestWorkerExitCount(kChannel);
        double closeMs = 0.0;
        QVERIFY2(closeUnderWatchdog(kChannel, closeMs), "CloseChannel never returned");
        g_channelOpen = false;
        const int exitsAtReturn = WDSPGetTestWorkerExitCount(kChannel);
        qInfo("CloseChannel with a busy worker took %.2f ms; worker exits %d -> %d",
              closeMs, exitsBefore, exitsAtReturn);
        QVERIFY2(exitsAtReturn == exitsBefore + 1,
                 "CloseChannel returned while the worker was still inside its block");
    }

    void inputSamplerateRebuildWaitsForABusyWorker()
    {
        openChannel();
        putTheWorkerInsideABlock();

        const int exitsBefore = WDSPGetTestWorkerExitCount(kChannel);
        double rebuildMs = 0.0;
        QVERIFY2(runUnderWatchdog([] { SetInputSamplerate(kChannel, 2 * kSampleRate); },
                                  rebuildMs),
                 "SetInputSamplerate never returned");
        const int exitsAtReturn = WDSPGetTestWorkerExitCount(kChannel);
        qInfo("SetInputSamplerate with a busy worker took %.2f ms; worker exits %d -> %d",
              rebuildMs, exitsBefore, exitsAtReturn);
        QVERIFY2(exitsAtReturn == exitsBefore + 1,
                 "SetInputSamplerate rebuilt while the old worker was still inside its block");

        WDSPSetTestBlockDelayUs(kChannel, 0);
        double closeMs = 0.0;
        QVERIFY2(closeUnderWatchdog(kChannel, closeMs), "CloseChannel never returned");
        g_channelOpen = false;
        QCOMPARE(WDSPGetTestWorkerExitCount(kChannel), exitsBefore + 2);
    }

    void inputBuffsizeRebuildWaitsForABusyWorker()
    {
        openChannel();
        putTheWorkerInsideABlock();

        const int exitsBefore = WDSPGetTestWorkerExitCount(kChannel);
        double rebuildMs = 0.0;
        QVERIFY2(runUnderWatchdog([] { SetInputBuffsize(kChannel, 2 * kInSize); }, rebuildMs),
                 "SetInputBuffsize never returned");
        const int exitsAtReturn = WDSPGetTestWorkerExitCount(kChannel);
        qInfo("SetInputBuffsize with a busy worker took %.2f ms; worker exits %d -> %d",
              rebuildMs, exitsBefore, exitsAtReturn);
        QVERIFY2(exitsAtReturn == exitsBefore + 1,
                 "SetInputBuffsize rebuilt while the old worker was still inside its block");

        WDSPSetTestBlockDelayUs(kChannel, 0);
        double closeMs = 0.0;
        QVERIFY2(closeUnderWatchdog(kChannel, closeMs), "CloseChannel never returned");
        g_channelOpen = false;
        QCOMPARE(WDSPGetTestWorkerExitCount(kChannel), exitsBefore + 2);
    }

    void closeChannelWaitsOutABlockLongerThanTheLogInterval()
    {
        openChannel();
        putTheWorkerInsideABlock(kLongBlockDelayUs);

        const int exitsBefore = WDSPGetTestWorkerExitCount(kChannel);
        double closeMs = 0.0;
        QVERIFY2(closeUnderWatchdog(kChannel, closeMs, kLongBlockWatchdog),
                 "CloseChannel never returned");
        g_channelOpen = false;
        const int exitsAtReturn = WDSPGetTestWorkerExitCount(kChannel);
        qInfo("CloseChannel with a %d ms block took %.2f ms; worker exits %d -> %d",
              kLongBlockDelayUs / 1000, closeMs, exitsBefore, exitsAtReturn);
        QVERIFY2(exitsAtReturn == exitsBefore + 1,
                 "CloseChannel returned while the worker was still inside its block");
        QVERIFY2(closeMs >= kLongBlockMinCloseMs, "CloseChannel did not wait out the long block");
    }

    void teardownOfAWorkerThatNeverStartedDoesNotWait()
    {
        // start_thread reports a failed _beginthread (it returns -1).
        WdspWorkerStarted(kUnstartedChannel, 0);
        double waitMs = 0.0;
        const bool returned =
            runUnderWatchdog([] { WdspWaitWorkerExit(kUnstartedChannel); }, waitMs);
        qInfo("teardown of a channel whose worker never started took %.2f ms", waitMs);
        QVERIFY2(returned, "teardown waited for a worker that never started");

        // The skipped wait must not shift the exit pairing: a real channel
        // on the same id still closes and counts exactly one exit.
        OpenChannel(kUnstartedChannel, kInSize, kDspSize, kSampleRate, kSampleRate,
                    kSampleRate, 0, 1, 0.010, 0.025, 0.000, 0.010, 0);
        const int exitsBefore = WDSPGetTestWorkerExitCount(kUnstartedChannel);
        double closeMs = 0.0;
        const bool closed = closeUnderWatchdog(kUnstartedChannel, closeMs);
        QVERIFY2(closed, "closing the channel after a skipped wait never returned");
        QCOMPARE(WDSPGetTestWorkerExitCount(kUnstartedChannel), exitsBefore + 1);
    }

    void idleWorkerExitsAtOnce()
    {
        // Several idle closes; the median is checked. A fixed teardown delay
        // shows in every close, while a woken worker that waits for a core
        // on a saturated machine (seen once: 2.1 ms at load 30, where the
        // same worker's measured wake-ups were 0.03 ms) shows in one.
        std::vector<double> waits;
        double worstWakeupMs = 0.0;
        for (int close = 0; close < kIdleCloses; ++close) {
            openChannel();
            std::this_thread::sleep_for(kIdleSettle);
            worstWakeupMs = std::max(worstWakeupMs, worstWorkerWakeupMs());
            std::this_thread::sleep_for(kIdleSettle);

            const int exitsBefore = WDSPGetTestWorkerExitCount(kChannel);
            double closeMs = 0.0;
            QVERIFY2(closeUnderWatchdog(kChannel, closeMs), "CloseChannel never returned");
            g_channelOpen = false;
            const double waitMs = WDSPGetTestLastWorkerExitWaitUs(kChannel) / 1000.0;
            qInfo("idle close %d: CloseChannel %.2f ms, its wait for the worker %.2f ms",
                  close + 1, closeMs, waitMs);
            QCOMPARE(WDSPGetTestWorkerExitCount(kChannel), exitsBefore + 1);
            QVERIFY2(waitMs >= 0.0, "teardown did not record its wait");
            waits.push_back(waitMs);
        }

        std::sort(waits.begin(), waits.end());
        const double medianMs = waits[waits.size() / 2];
        const double limitMs = worstWakeupMs + kPollStepMs + kIdleExitMarginMs;
        qInfo("CloseChannel with an idle worker: median wait for the worker %.2f ms, "
              "longest %.2f ms (limit %.2f = worst wake-up %.2f + poll step %.1f + "
              "margin %.1f)",
              medianMs, waits.back(), limitMs, worstWakeupMs, kPollStepMs,
              kIdleExitMarginMs);
        QVERIFY2(medianMs <= limitMs, "teardown waited too long for an idle worker");
    }
};

QTEST_MAIN(TestWdspChannelShutdown)
#include "tst_wdsp_channel_shutdown.moc"
