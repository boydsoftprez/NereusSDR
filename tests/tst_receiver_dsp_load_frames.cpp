// no-port-check: NereusSDR-original linked-WDSP integration test (R-R3-40,
// R-R3-37). A real receive channel with the Core's settings (192 kHz input in
// chunks of 256, 64-sample DSP buffer at 48 kHz), fed in real time the way
// RxDspWorker feeds it, sampled every 500 ms through the real
// ReceiverDspLoadSampler and NnrLoadGovernor.
//
// Two references, one per kind of defect:
//   - The worker's busy share measured the way the load is defined (its time
//     inside blocks over wall time), from each block's start and end reported
//     by dsplock.c's test-only block hook, independently of the load counters
//     and the sampler under test. Every interval reads it within
//     kBusyAgreement. This catches a sampler that reads the counters wrongly,
//     but not wrong block edges: the hook reports the same edges the counters
//     use.
//   - The worker's own CPU clock (captured through the thread-start hook),
//     which knows nothing of dsplock.c's edges. It bounds the load from both
//     sides, so an edge that takes waiting time into the block (a start
//     stamped before the worker waits for its buffer or its lock) fails.
//
// The CPU comparison only holds while the worker ran whenever it was inside a
// block. On a busy machine a worker preempted inside a block is still inside
// it, so its busy time grows while its CPU time does not (loads 7 to 70 on an
// 18-core Mac: load 0.59 against CPU 0.43 with the load right). So the CPU
// claims are held in the intervals whose busy share is within the tolerance
// of the CPU share (corroborated intervals; every interval on a quiet
// machine), and each case needs at least kMinCorroborated of them. Waiting
// time taken into the blocks raises the busy share over the CPU share in
// every interval, so a wrong edge leaves too few and fails.
//
// Claims:
//   - every interval reads the busy share within kBusyAgreement;
//   - every corroborated interval reads the CPU share within the case's
//     tolerance, both ways, and there are at least kMinCorroborated;
//   - the frame-like load reads under the display line in every corroborated
//     interval, and neither it nor the uniform load ever steps back (both are
//     well under the governor's line of work, so a step-back is the Rock 5C
//     symptom below);
//   - the overload reads at least kOverloadMinLoad in every interval in
//     which the worker never waited a block period for input (a fed
//     interval), never reads under its CPU share, and steps back within
//     kTicks. It needs kMinFed fed intervals, not corroborated ones: its
//     claims hold whatever share of a core the worker gets (see the case).
//
// The test keeps its own record of the conditions each claim needs, instead
// of assuming a quiet machine. A busy machine can preempt the feeder, which
// stands in for the radio, long enough that the worker waits for input: at
// load averages of 17 to 120 on an 18-core Mac, one interval of the overload
// read 0.897 (worker CPU 0.868), correctly, as the worker sat idle waiting
// for input for about a tenth of it (block-hook gaps of 8 to 24 ms were
// seen at that load). So the overload's floor is held in the fed intervals (from
// the block hook's edges: the longest stretch of the interval outside any
// block). And a busy machine can leave few corroborated intervals in 20, so
// each case samples past kTicks, up to kMaxTicks, until it has the
// kMinCorroborated intervals its claims need. On a quiet machine every
// interval qualifies and each case still runs kTicks.
//
// The feeder stands in for the radio, whose packets arrive in real time
// however busy the computer is. It runs at the priority RxDspWorker, the
// production feeder, takes (macOS: USER_INTERACTIVE).
//
// The frame-like case is the defect found on the Rock 5C: a stage that works
// in frames longer than one block (neural noise reduction: a 768-sample hop at
// 48 kHz is one heavy block in 12 at a 64-sample buffer) makes one long block
// in twelve. A sample that landed inside such a block used to read its time
// so far over one block period (up to several times 1.0), and the step-back
// turned noise reduction off with the worker at about 60% of a core. The load
// is now busy time over wall time between two reads.
//
// Real time: each case runs about 10 s. Opens no audio device.
#include <QtTest>
#include "RealtimeTestLoad.h"

#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numbers>
#include <thread>
#include <vector>

#include <pthread.h>
#include <time.h>
#ifdef Q_OS_MAC
#include <mach/mach.h>
#include <mach/thread_act.h>
#include <pthread/qos.h>
#endif

#include "core/dsp/NnrLoadGovernor.h"
#include "core/wdsp_api.h"
#include "models/ReceiverDspLoadSampler.h"

using namespace NereusSDR;

namespace {

using Clock = std::chrono::steady_clock;

// A WDSP channel id no application object uses in this process.
constexpr int kChannel = 21;
constexpr int kSlice = 0;

// The Core's receive settings: 192 kHz input, in_size 64 * rate / 48000
// (RadioModel's bufferSizeForRate), DSP and output at 48 kHz (RadioModel's
// createRxChannel), and the default 64-sample DSP buffer
// (RxChannel's DspOptionsBufferSize default).
constexpr int kInputRate = 192000;
constexpr int kInSize = 256;
constexpr int kDspRate = 48000;
constexpr int kOpenDspSize = 4096;
constexpr int kDspSize = 64;
// The radio's packets: 238 samples each (one every 1.24 ms at 192 kHz),
// drained in whole kInSize chunks, as RxDspWorker does.
constexpr int kPacket = 238;
// A feeder more than this far behind its schedule (after a deliberately
// overloaded or stuck worker) starts again from now rather than bursting,
// as RxDspWorker drops input to keep its wait bounded.
constexpr std::chrono::milliseconds kFeederResync{100};

constexpr auto kSampleInterval = std::chrono::milliseconds(ReceiverDspLoadSampler::kSampleIntervalMs);
// About 10 s of samples per case. The overload's step-back is due after
// NnrLoadGovernor's kNnrStepDownHoldMs (2 s) at the line; kNnrStepSettleMs
// (5 s) only delays a second step, which no case waits for.
constexpr int kTicks = 20;
// The most intervals a case samples while waiting for the intervals its
// claims need (about 30 s).
constexpr int kMaxTicks = 60;
// Corroborated intervals (busy share within the tolerance of the CPU share)
// each case needs before its CPU claims count. A block start stamped before
// the worker waits for its buffer left none of 20 in the frame-like and
// uniform cases; a busy machine (load 19 to 25) left as few as 4 in the
// uniform case with the edges right.
constexpr int kMinCorroborated = 2;
// Fed intervals the overload case needs before its floor counts.
constexpr int kMinFed = 2;
// Time for the worker to settle after a delay changes.
constexpr std::chrono::milliseconds kSettle{1000};

// Frame-like load: 9 ms of work in every 12th block (one NNR hop).
constexpr int kFrameDelayUs = 9000;
constexpr int kFrameEvery = 12;
constexpr double kFrameMaxLoad = 0.75;          // the display governor's busy line
constexpr double kFrameCpuTolerance = 0.10;     // absolute, busy share and CPU
// Uniform load well under real time: 900 us of a 1333 us block.
constexpr int kUniformDelayUs = 900;
constexpr double kUniformCpuTolerance = 0.10;   // relative, busy share and CPU
// One DSP block period at the Core's settings: 64 samples at 48 kHz.
constexpr qint64 kBlockPeriodNs = qint64(kDspSize) * 1'000'000'000LL / kDspRate;
// Real overload: two block periods of work in every block (2666 us of a
// 1333 us block), so the worker falls behind by a block every block. By the
// end of kSettle it has about 375 blocks (a second of work) queued, and a
// feeder the machine holds off for less than that never leaves it waiting
// for input. At 1400 us (5% over) it had about 40 queued blocks, and a
// busy machine's feeder stall of tens of ms drained them: the worker then
// waited for input and read under 0.95 in overload intervals.
constexpr int kOverloadDelayUs = int(2 * kBlockPeriodNs / 1000);
constexpr double kOverloadMinLoad = 0.95;
constexpr double kOverloadCpuTolerance = 0.10;  // absolute, busy share and CPU
// A reading that is right agrees with the busy-share reference to within
// this (the same clock and the same block edges; observed within 0.001).
constexpr double kBusyAgreement = 0.02;
// One stuck block of 1.5 s.
constexpr int kStuckDelayUs = 1500000;
constexpr double kStuckMinLoad = 0.95;

// The channel's current DSP worker, as the thread-start hook reported it.
// SetDSPBuffsize rebuilds the channel, so the worker OpenChannel started
// ends and a new one starts; the old one may report its end after the new
// one has started.
QMutex g_workerMutex;
bool g_haveWorker = false;
int g_workerStarts = 0;
pthread_t g_worker{};

// Runs on a WDSP thread as it starts, and on a channel worker as it ends.
void onWdspThreadStart(int kind, int channel)
{
    if (channel != kChannel) {
        return;
    }
    QMutexLocker lock(&g_workerMutex);
    if (kind == kWdspThreadRxMain) {
        g_worker = pthread_self();
        g_haveWorker = true;
        ++g_workerStarts;
    } else if (kind == kWdspThreadWorkerExit && g_haveWorker
               && pthread_equal(g_worker, pthread_self())) {
        g_haveWorker = false;
    }
}

// Every block the channel's worker ran since sampleFor cleared the list, as
// the block hook reported it: start and end on dsplock.c's monotonic clock
// (the clock of GetChannelDspLoad's readNs); end is -1 while it runs. The
// worker runs one block at a time, so only the last can be running.
struct BlockSpan {
    qint64 startNs{0};
    qint64 endNs{-1};
};
QMutex g_blocksMutex;
std::vector<BlockSpan> g_blocks;

void onBlock(int channel, long long startNs, long long endNs)
{
    if (channel != kChannel) {
        return;
    }
    QMutexLocker lock(&g_blocksMutex);
    if (endNs == 0) {
        g_blocks.push_back(BlockSpan{startNs, -1});
    } else if (!g_blocks.empty() && g_blocks.back().startNs == startNs) {
        g_blocks.back().endNs = endNs;
    } else {
        g_blocks.push_back(BlockSpan{startNs, endNs});   // started before the hook
    }
}

// Forgets the finished blocks; a block still running stays, so its end is
// matched to its start.
void clearBlocks()
{
    QMutexLocker lock(&g_blocksMutex);
    const bool running = !g_blocks.empty() && g_blocks.back().endNs < 0;
    const BlockSpan last = running ? g_blocks.back() : BlockSpan{};
    g_blocks.clear();
    if (running) {
        g_blocks.push_back(last);
    }
}

// The share of [fromNs, toNs] the worker spent inside blocks; a block still
// running counts up to toNs, as the load counts the block in progress.
// Intervals only move forward, so blocks that ended by fromNs are dropped.
// The worker's hook takes g_blocksMutex inside its timed block, so the lock
// is held only to trim and copy, and the walk runs outside it.
double busyShare(qint64 fromNs, qint64 toNs)
{
    std::vector<BlockSpan> spans;
    {
        QMutexLocker lock(&g_blocksMutex);
        g_blocks.erase(std::remove_if(g_blocks.begin(), g_blocks.end(),
                                      [fromNs](const BlockSpan& b) {
                                          return b.endNs >= 0 && b.endNs <= fromNs;
                                      }),
                       g_blocks.end());
        spans = g_blocks;
    }
    qint64 busy = 0;
    for (const BlockSpan& b : spans) {
        const qint64 end = b.endNs < 0 ? toNs : b.endNs;
        const qint64 overlap = std::min(end, toNs) - std::max(b.startNs, fromNs);
        if (overlap > 0) {
            busy += overlap;
        }
    }
    return toNs > fromNs ? double(busy) / double(toNs - fromNs) : 0.0;
}

// The longest stretch of [fromNs, toNs] the worker spent outside any block:
// waiting for its input (or for the DSP lock, which nothing else takes here).
qint64 longestWaitNs(qint64 fromNs, qint64 toNs)
{
    std::vector<BlockSpan> spans;
    {
        QMutexLocker lock(&g_blocksMutex);
        spans = g_blocks;
    }
    qint64 cursor = fromNs;
    qint64 longest = 0;
    for (const BlockSpan& b : spans) {
        const qint64 end = b.endNs < 0 ? toNs : b.endNs;
        if (end <= fromNs || b.startNs >= toNs) {
            continue;
        }
        longest = std::max(longest, b.startNs - cursor);
        cursor = std::max(cursor, end);
    }
    return std::max(longest, toNs - cursor);
}

bool haveWorkerAfter(int starts)
{
    QMutexLocker lock(&g_workerMutex);
    return g_haveWorker && g_workerStarts >= starts;
}

// The current worker's CPU time so far, or -1 when it cannot be read.
qint64 workerCpuNs()
{
    QMutexLocker lock(&g_workerMutex);
    if (!g_haveWorker) {
        return -1;
    }
#ifdef Q_OS_MAC
    thread_basic_info_data_t info{};
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    if (thread_info(pthread_mach_thread_np(g_worker), THREAD_BASIC_INFO,
                    reinterpret_cast<thread_info_t>(&info), &count) != KERN_SUCCESS) {
        return -1;
    }
    return (qint64(info.user_time.seconds) + info.system_time.seconds) * 1'000'000'000LL
        + (qint64(info.user_time.microseconds) + info.system_time.microseconds) * 1000LL;
#else
    clockid_t clock{};
    timespec now{};
    if (pthread_getcpuclockid(g_worker, &clock) != 0 || clock_gettime(clock, &now) != 0) {
        return -1;
    }
    return qint64(now.tv_sec) * 1'000'000'000LL + now.tv_nsec;
#endif
}

qint64 monotonicMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now().time_since_epoch()).count();
}

// One reading the way RadioModel::sampleReceiverDspLoad builds it; a read
// GetChannelDspLoad flags as possibly torn is not consistent, and the
// sampler keeps what it had.
ReceiverDspLoadSampler::Reading readChannel()
{
    WdspChannelLoad load{};
    const int result = GetChannelDspLoad(kChannel, &load);
    ReceiverDspLoadSampler::Reading reading;
    reading.consistent = result == 0;
    reading.blocks = load.blocks;
    reading.busyNs = load.busyNs;
    reading.lateBlocks = load.lateBlocks;
    reading.lifetimeMaxBlockUs = load.maxBlockUs;
    reading.blockPeriodUs = load.blockPeriodUs;
    reading.currentBlockNs = load.currentBlockNs;
    reading.readNs = load.readNs;
    reading.intervalMaxBlockUs = TakeChannelDspIntervalMaxBlockUs(kChannel);
    return reading;
}

QHash<int, ReceiverDspLoadSampler::Reading> one(const ReceiverDspLoadSampler::Reading& r)
{
    QHash<int, ReceiverDspLoadSampler::Reading> readings;
    readings.insert(kSlice, r);
    return readings;
}

struct Sample {
    double atSeconds{0.0};
    double load{0.0};
    bool idle{false};
    double cpuShare{0.0};
    // The busy share from the block hook over the same interval.
    double busyShare{0.0};
    // The longest the worker waited outside a block in the interval.
    qint64 longestWaitNs{0};
    qint64 maxBlockUs{0};
    bool steppedBack{false};
};

struct Run {
    std::vector<Sample> samples;
    int stepBacks{0};
};

// Samples kSampleInterval apart through the real sampler and the real
// step-back governor (a receiver running Premium NNR) for `ticks` intervals,
// and on past them, up to kMaxTicks, until `needed` intervals satisfy
// `qualifies`. The first reading only seeds the sampler and sets the
// governor's time base.
Run sampleFor(int ticks, const std::function<bool(const Sample&)>& qualifies = {},
              int needed = 0)
{
    ReceiverDspLoadSampler sampler;
    NnrLoadGovernor governor;
    NnrLoadGovernor::Receiver receiver;
    receiver.nnrSelected = true;
    receiver.savedModelSlot = 1;

    Run run;
    clearBlocks();
    const auto start = Clock::now();
    auto next = start;
    ReceiverDspLoadSampler::Reading reading;
    do {
        reading = readChannel();
    } while (!reading.consistent);
    qint64 previousCpu = workerCpuNs();
    sampler.update(one(reading));
    qint64 previousReadNs = reading.readNs;
    governor.observe(kSlice, monotonicMs(), receiver);

    int qualifying = 0;
    for (int tick = 0; tick < kMaxTicks; ++tick) {
        if (tick >= ticks && (!qualifies || qualifying >= needed)) {
            break;
        }
        next += kSampleInterval;
        std::this_thread::sleep_until(next);
        reading = readChannel();
        const qint64 cpu = workerCpuNs();
        sampler.update(one(reading));
        if (!reading.consistent) {
            // The sampler kept its baseline; so does the reference.
            continue;
        }
        const auto snapshot = sampler.snapshot(kSlice);

        Sample sample;
        sample.atSeconds = std::chrono::duration<double>(Clock::now() - start).count();
        // Over the interval the load covers (readNs to readNs); the CPU
        // clock is read right after the load.
        const qint64 fromNs = previousReadNs;
        const qint64 toNs = reading.readNs;
        sample.cpuShare = toNs > fromNs ? double(cpu - previousCpu) / double(toNs - fromNs) : 0.0;
        sample.longestWaitNs = longestWaitNs(fromNs, toNs);
        sample.busyShare = busyShare(fromNs, toNs);
        if (snapshot) {
            sample.load = snapshot->load;
            sample.idle = snapshot->idle;
            sample.maxBlockUs = snapshot->maxBlockUs;
            if (!snapshot->idle) {
                receiver.load = snapshot->load;
            } else {
                receiver.load.reset();
            }
        } else {
            receiver.load.reset();
        }
        const auto stepped = governor.observe(kSlice, monotonicMs(), receiver);
        if (stepped) {
            sample.steppedBack = true;
            ++run.stepBacks;
            receiver.limit = *stepped;
        }
        if (qualifies && qualifies(sample)) {
            ++qualifying;
        }
        run.samples.push_back(sample);
        previousCpu = cpu;
        previousReadNs = reading.readNs;
    }
    return run;
}

// Whether the worker ran whenever it was inside a block in this interval:
// its busy share is within `tolerance` of its CPU share.
bool corroborated(const Sample& s, double tolerance)
{
    return s.busyShare - s.cpuShare <= tolerance;
}

void logRun(const char* name, const Run& run)
{
    for (const Sample& s : run.samples) {
        qInfo("%s t=%.1fs load=%.3f busy=%.3f cpu=%.3f max_block_us=%lld longest_wait_us=%lld%s%s",
              name, s.atSeconds, s.load, s.busyShare, s.cpuShare,
              static_cast<long long>(s.maxBlockUs),
              static_cast<long long>(s.longestWaitNs / 1000),
              s.idle ? " idle" : "", s.steppedBack ? " STEP-BACK" : "");
    }
}

} // namespace

class TestReceiverDspLoadFrames : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        WDSPSetThreadStartHook(onWdspThreadStart);
        WDSPSetTestBlockHook(onBlock);
        // WdspEngine::createRxChannel's OpenChannel and seeding.
        OpenChannel(kChannel, kInSize, kOpenDspSize, kInputRate, kDspRate, kDspRate,
                    0,       // type: RX
                    0,       // state: off until configured
                    0.010, 0.025, 0.000, 0.010,
                    1);      // bfo: fexchange2 waits for output, as in the Core
        SetRXAMode(kChannel, 1);  // USB
        SetRXABandpassFreqs(kChannel, 150.0, 2850.0);
        RXANBPSetFreqs(kChannel, 150.0, 2850.0);
        SetRXAAGCMode(kChannel, 3);  // Med
        SetRXAAGCTop(kChannel, 80.0);
        SetRXAPanelBinaural(kChannel, 0);
        SetDSPBuffsize(kChannel, kDspSize);
        SetChannelState(kChannel, 1, 0);

        // One worker from OpenChannel, a second from SetDSPBuffsize's rebuild.
        const auto deadline = Clock::now() + std::chrono::seconds(5);
        while (!haveWorkerAfter(2) && Clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        QVERIFY2(haveWorkerAfter(2), "the rebuilt channel's DSP worker never reported its start");
        QVERIFY2(workerCpuNs() >= 0, "the worker's CPU clock cannot be read");
        m_feeder = std::thread([this] { feed(); });
        std::this_thread::sleep_for(kSettle);
    }

    void cleanupTestCase()
    {
        clearDelays();
        m_stop.store(true);
        if (m_feeder.joinable()) {
            m_feeder.join();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        CloseChannel(kChannel);
        WDSPSetTestBlockHook(nullptr);
        WDSPSetThreadStartHook(nullptr);
    }

    void cleanup()
    {
        // The load when a real-time case failed (R-R3-21, R-R3-40).
        NereusSDR::RealtimeTestLoad::printLoadAverageIfFailed();
        clearDelays();
        std::this_thread::sleep_for(kSettle);
    }

    // R-R3-40 fix wave: when the worker keeps busyNs and the block in
    // progress changing through every attempt, GetChannelDspLoad says so
    // (1) instead of handing back a pair that may be torn, and reads
    // normally again once the pair settles.
    void aReadThatNeverFindsThePairAtRestIsFlagged()
    {
        WdspChannelLoad load{};
        QCOMPARE(GetChannelDspLoad(kChannel, &load), 0);
        // The hold is steady while the worker keeps running its blocks:
        // every read while held is flagged, not only most of them.
        WDSPSetTestHoldLoadPair(kChannel, 1);
        const auto until = Clock::now() + std::chrono::milliseconds(500);
        int reads = 0;
        int unflagged = 0;
        int held = 1;
        while (Clock::now() < until) {
            const int result = GetChannelDspLoad(kChannel, &load);
            ++reads;
            if (result != 1) {
                ++unflagged;
                held = result;
            }
        }
        WDSPSetTestHoldLoadPair(kChannel, 0);
        QVERIFY2(unflagged == 0, qPrintable(QStringLiteral("%1 of %2 reads not flagged")
                                                .arg(unflagged).arg(reads)));
        QCOMPARE(held, 1);
        QVERIFY(load.readNs > 0);
        QCOMPARE(GetChannelDspLoad(kChannel, &load), 0);
    }

    // The defect: one 9 ms block in 12 is about 60% of a core, not overload.
    void aFrameLikeLoadReadsItsBusyAndCpuShare()
    {
        WDSPSetTestPeriodicDelayUs(kChannel, kFrameDelayUs, kFrameEvery);
        std::this_thread::sleep_for(kSettle);
        const Run run = sampleFor(kTicks, [](const Sample& s) {
            return corroborated(s, kFrameCpuTolerance);
        }, kMinCorroborated);
        logRun("frame-like", run);
        QVERIFY(run.samples.size() >= std::size_t(kTicks) - 2);
        int corroboratedIntervals = 0;
        int uncorroboratedStepBacks = 0;
        for (const Sample& s : run.samples) {
            QVERIFY2(!s.idle, qPrintable(QStringLiteral("idle at %1 s").arg(s.atSeconds)));
            QVERIFY2(std::abs(s.load - s.busyShare) <= kBusyAgreement,
                     qPrintable(QStringLiteral("load %1 vs worker busy share %2 at %3 s")
                                    .arg(s.load).arg(s.busyShare).arg(s.atSeconds)));
            if (corroborated(s, kFrameCpuTolerance)) {
                ++corroboratedIntervals;
                QVERIFY2(std::abs(s.load - s.cpuShare) <= kFrameCpuTolerance,
                         qPrintable(QStringLiteral("load %1 vs worker CPU %2 at %3 s")
                                        .arg(s.load).arg(s.cpuShare).arg(s.atSeconds)));
                QVERIFY2(s.load < kFrameMaxLoad,
                         qPrintable(QStringLiteral("load %1 at %2 s (worker CPU %3)")
                                        .arg(s.load).arg(s.atSeconds).arg(s.cpuShare)));
                // About 60% of a core of work, under the governor's line: a
                // step-back here is the Rock 5C symptom.
                QVERIFY2(!s.steppedBack,
                         qPrintable(QStringLiteral("the step-back tripped at %1 s (load %2, "
                                                   "worker CPU %3)")
                                        .arg(s.atSeconds).arg(s.load).arg(s.cpuShare)));
            } else if (s.steppedBack) {
                // Load findings 4: as in the uniform case below, the
                // machine held the worker off its core; the governor judges
                // wall share by JJ's ruling.
                ++uncorroboratedStepBacks;
            }
        }
        QVERIFY2(corroboratedIntervals >= kMinCorroborated,
                 qPrintable(QStringLiteral("%1 of %2 intervals had the busy share within %3 of the "
                                           "worker's CPU")
                                .arg(corroboratedIntervals).arg(run.samples.size())
                                .arg(kFrameCpuTolerance)));
        QCOMPARE(run.stepBacks, uncorroboratedStepBacks);
    }

    // Uniform loads read as before: close to the worker's busy and CPU share.
    void aUniformLoadReadsItsBusyAndCpuShare()
    {
        WDSPSetTestProcessDelayUs(kChannel, kUniformDelayUs);
        std::this_thread::sleep_for(kSettle);
        const Run run = sampleFor(kTicks, [](const Sample& s) {
            return corroborated(s, kUniformCpuTolerance * s.cpuShare);
        }, kMinCorroborated);
        logRun("uniform-900us", run);
        QVERIFY(run.samples.size() >= std::size_t(kTicks) - 2);
        int corroboratedIntervals = 0;
        int uncorroboratedStepBacks = 0;
        for (const Sample& s : run.samples) {
            QVERIFY2(!s.idle, qPrintable(QStringLiteral("idle at %1 s").arg(s.atSeconds)));
            QVERIFY2(std::abs(s.load - s.busyShare) <= kBusyAgreement,
                     qPrintable(QStringLiteral("load %1 vs worker busy share %2 at %3 s")
                                    .arg(s.load).arg(s.busyShare).arg(s.atSeconds)));
            const double tolerance = kUniformCpuTolerance * s.cpuShare;
            if (corroborated(s, tolerance)) {
                ++corroboratedIntervals;
                QVERIFY2(std::abs(s.load - s.cpuShare) <= tolerance,
                         qPrintable(QStringLiteral("load %1 vs worker CPU %2 at %3 s")
                                        .arg(s.load).arg(s.cpuShare).arg(s.atSeconds)));
                // About 70% of a core of work, under the governor's line.
                QVERIFY2(!s.steppedBack,
                         qPrintable(QStringLiteral("the step-back tripped at %1 s (load %2, "
                                                   "worker CPU %3)")
                                        .arg(s.atSeconds).arg(s.load).arg(s.cpuShare)));
            } else if (s.steppedBack) {
                // Load findings 4: the machine, not the DSP, held the
                // worker off its core for this interval (its busy share of
                // wall time well over its CPU share), and the governor
                // judges wall share by JJ's ruling (2026-09-28), so a
                // step-back here is the governor doing its job.
                ++uncorroboratedStepBacks;
            }
        }
        if (uncorroboratedStepBacks > 0) {
            qInfo("%d step-backs in intervals the machine, not the DSP, saturated",
                  uncorroboratedStepBacks);
        }
        QVERIFY2(corroboratedIntervals >= kMinCorroborated,
                 qPrintable(QStringLiteral("%1 of %2 intervals had the busy share within %3 of the "
                                           "worker's CPU")
                                .arg(corroboratedIntervals).arg(run.samples.size())
                                .arg(kUniformCpuTolerance)));
        QCOMPARE(run.stepBacks, uncorroboratedStepBacks);
    }

    // A worker that really cannot keep up still reads as overloaded, and
    // the step-back still trips.
    //
    // This case's claims do not depend on the worker's share of a core. At
    // load averages of 64 to 95 on an 18-core Mac the worker (already at the
    // production QoS, USER_INTERACTIVE, from linux_port.c) got 0.14 to 0.35
    // of a core: preempted inside its blocks in every interval, so no
    // interval had the busy share within the tolerance of the CPU share, and
    // requiring kMinCorroborated of them failed the case with the reading
    // right (every fed interval at least 0.95, the step-back on time). The
    // corroborated count is how the frame-like and uniform cases catch an
    // edge that takes waiting time into a block; a fed overload has its
    // input queued and nothing else takes its lock, so there is no waiting
    // for such an edge to take in, and the count proves nothing here. The
    // case holds instead:
    //   - the CPU share as a floor in every interval: preemption can only
    //     lower the CPU share against the busy share, never raise it, so an
    //     edge that misses work fails here however busy the machine is;
    //   - the CPU share as a ceiling in the corroborated intervals, as before;
    //   - kOverloadMinLoad in every fed interval, corroborated or not (a
    //     worker preempted inside a block is still inside it), and at least
    //     kMinFed of them.
    void aRealOverloadStillStepsBack()
    {
        WDSPSetTestProcessDelayUs(kChannel, kOverloadDelayUs);
        std::this_thread::sleep_for(kSettle);
        // Fed: the worker never waited a whole block period for input, so
        // the interval was the overload the case sets up.
        const auto fed = [](const Sample& s) { return s.longestWaitNs < kBlockPeriodNs; };
        const Run run = sampleFor(kTicks, fed, kMinFed);
        logRun("overload-2x", run);
        QVERIFY(run.samples.size() >= std::size_t(kTicks) - 2);
        int fedIntervals = 0;
        int corroboratedIntervals = 0;
        for (const Sample& s : run.samples) {
            QVERIFY2(!s.idle, qPrintable(QStringLiteral("idle at %1 s").arg(s.atSeconds)));
            QVERIFY2(std::abs(s.load - s.busyShare) <= kBusyAgreement,
                     qPrintable(QStringLiteral("load %1 vs worker busy share %2 at %3 s")
                                    .arg(s.load).arg(s.busyShare).arg(s.atSeconds)));
            QVERIFY2(s.load >= s.cpuShare - kOverloadCpuTolerance,
                     qPrintable(QStringLiteral("load %1 under worker CPU %2 at %3 s")
                                    .arg(s.load).arg(s.cpuShare).arg(s.atSeconds)));
            if (corroborated(s, kOverloadCpuTolerance)) {
                ++corroboratedIntervals;
                QVERIFY2(std::abs(s.load - s.cpuShare) <= kOverloadCpuTolerance,
                         qPrintable(QStringLiteral("load %1 vs worker CPU %2 at %3 s")
                                        .arg(s.load).arg(s.cpuShare).arg(s.atSeconds)));
            }
            if (fed(s)) {
                ++fedIntervals;
                QVERIFY2(s.load >= kOverloadMinLoad,
                         qPrintable(QStringLiteral("load %1 at %2 s (worker CPU %3, "
                                                   "longest wait %4 us)")
                                        .arg(s.load).arg(s.atSeconds).arg(s.cpuShare)
                                        .arg(s.longestWaitNs / 1000)));
            }
        }
        if (corroboratedIntervals < int(run.samples.size())) {
            qInfo("%d of %zu intervals had the worker preempted inside its blocks "
                  "(busy share over its CPU share by more than %.2f)",
                  int(run.samples.size()) - corroboratedIntervals, run.samples.size(),
                  kOverloadCpuTolerance);
        }
        QVERIFY2(fedIntervals >= kMinFed,
                 qPrintable(QStringLiteral("%1 of %2 intervals had the worker fed throughout")
                                .arg(fedIntervals).arg(run.samples.size())));
        // Within kTicks, however long the case sampled after.
        const bool steppedInTime = std::any_of(
            run.samples.begin(),
            run.samples.begin() + std::min<std::ptrdiff_t>(kTicks, std::ptrdiff_t(run.samples.size())),
            [](const Sample& s) { return s.steppedBack; });
        QVERIFY2(steppedInTime, "a real overload must step back");
    }

    // A worker stuck inside one long block reads as fully busy.
    void aStuckBlockReadsAsFullyBusy()
    {
        WDSPSetTestProcessDelayUs(kChannel, kStuckDelayUs);
        // Wait for the long block to start, then let no other block be long.
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        bool inside = false;
        while (Clock::now() < deadline) {
            WdspChannelLoad load{};
            GetChannelDspLoad(kChannel, &load);
            if (load.currentBlockNs > 20'000'000LL) {
                inside = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        WDSPSetTestProcessDelayUs(kChannel, 0);
        QVERIFY2(inside, "the long block never started");
        // Two full intervals inside the block (it has about 1.47 s left).
        const Run run = sampleFor(2);
        logRun("stuck-1.5s", run);
        for (const Sample& s : run.samples) {
            QVERIFY2(!s.idle, qPrintable(QStringLiteral("idle at %1 s").arg(s.atSeconds)));
            QVERIFY2(s.load >= kStuckMinLoad,
                     qPrintable(QStringLiteral("load %1 at %2 s").arg(s.load).arg(s.atSeconds)));
            // The block was running across both intervals.
            QVERIFY2(s.busyShare >= kStuckMinLoad,
                     qPrintable(QStringLiteral("busy share %1 at %2 s")
                                    .arg(s.busyShare).arg(s.atSeconds)));
        }
    }

private:
    static void clearDelays()
    {
        WDSPSetTestPeriodicDelayUs(kChannel, 0, 0);
        WDSPSetTestProcessDelayUs(kChannel, 0);
    }

    // The radio's packets in real time, drained through fexchange2 in whole
    // kInSize chunks (RxDspWorker::processIqBatch).
    void feed()
    {
#ifdef Q_OS_MAC
        // RxDspWorker::onThreadStarted's elevateAudioThreadPriority
        // (RealtimeAudioPriority.cpp): USER_INTERACTIVE. The workgroup join
        // is left out; it looks up the audio output device.
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
        std::vector<float> accI(kInSize + kPacket), accQ(kInSize + kPacket);
        std::array<float, kInSize> inI{}, inQ{}, outI{}, outQ{};
        int have = 0;
        double phase = 0.0;
        const double step = 2.0 * std::numbers::pi * 1200.0 / kInputRate;
        const auto packetPeriod = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(double(kPacket) / kInputRate));
        auto next = Clock::now();
        while (!m_stop.load()) {
            if (Clock::now() - next > kFeederResync) {
                next = Clock::now();
            }
            std::this_thread::sleep_until(next);
            next += packetPeriod;
            for (int i = 0; i < kPacket; ++i) {
                accI[have + i] = static_cast<float>(1e-3 * std::cos(phase));
                accQ[have + i] = static_cast<float>(1e-3 * std::sin(phase));
                phase = std::fmod(phase + step, 2.0 * std::numbers::pi);
            }
            have += kPacket;
            while (have >= kInSize) {
                std::copy_n(accI.begin(), kInSize, inI.begin());
                std::copy_n(accQ.begin(), kInSize, inQ.begin());
                std::copy(accI.begin() + kInSize, accI.begin() + have, accI.begin());
                std::copy(accQ.begin() + kInSize, accQ.begin() + have, accQ.begin());
                have -= kInSize;
                int error = 0;
                fexchange2(kChannel, inI.data(), inQ.data(), outI.data(), outQ.data(), &error);
            }
        }
    }

    std::thread m_feeder;
    std::atomic<bool> m_stop{false};
};

QTEST_GUILESS_MAIN(TestReceiverDspLoadFrames)
#include "tst_receiver_dsp_load_frames.moc"
