// =================================================================
// tests/tst_rx_channel_stop_feed.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. It drives real WDSP RX channels
// through RxChannel and RadioModel::setSampleRateLive.
//
// Task 8 of the receiver and transmit gaps plan: a stopping DSP channel is
// fed until WDSP finishes its stop.
//
// WDSP stops a channel by slewing its output down inside the channel's own
// exchange calls. SetChannelState(ch, 0, dmode) sets slew.downflag and
// flushflag (third_party/wdsp/src/channel.c:288-290); the next fexchange2
// calls slew the output down, then clear exchange and release the flush
// thread that clears flushflag (iobuffs.c:553-560, channel.c:146-166). With
// dmode 1 SetChannelState waits for flushflag, and gives up after 100
// Sleep(1) calls, clearing the flags itself (channel.c:291-304). Upstream
// keeps I/Q flowing into every channel while it stops: its receive loop calls
// fexchange0 on every sub-receiver channel on every pass whatever the
// channel's state (Thetis ChannelMaster/cmaster.c:365-366 [v2.10.3.15]), and
// the rate change stops its channels "while data is flowing"
// (Console/setup.cs:7042 and 7111 [v2.10.3.15]).
//
// Before Task 8 RxChannel marked itself inactive before the stop and
// processIq stopped exchanging on an inactive channel, so a draining stop
// always waited out the 100 ms timeout and a no-drain stop left the flags set
// until a rebuild.
//
// REALTIME: the drain bounds are wall-clock times while I/Q arrives paced in
// real time from a feeder thread, as it does from a radio.
// =================================================================
#include <QtTest/QtTest>
#include "RealtimeTestLoad.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <numbers>
#include <thread>
#include <vector>

#include "core/P1RadioConnection.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

using Clock = std::chrono::steady_clock;

// The WDSP block every receive channel is opened with: dsp_size 4096 at a
// 48 kHz DSP rate (RadioModel::openRxChannelPool, WdspEngine::createRxChannel).
constexpr int kDspSize = 4096;
constexpr int kDspRateHz = 48000;
constexpr double kDspBlockMs = 1000.0 * kDspSize / kDspRateHz;   // 85.3 ms

// setSampleRateLive's fixed waits: 10 ms after the stops, 25 ms for inflight
// packets, 5 ms before the restarts.
constexpr double kRateChangeFixedWaitsMs = 10.0 + 25.0 + 5.0;

// Real-time input before a channel counts as running: one DSP block fills
// WDSP's pipeline, the rest lets the output settle past the up-slew.
constexpr std::chrono::milliseconds kWarmUp{400};

constexpr int kMaxFedChannels = 8;

float toneSample(long n, int rateHz)
{
    // A real-valued 1 kHz tone lands in both sidebands, so the result does
    // not depend on the channel's mode.
    return static_cast<float>(
        0.01 * std::cos(2.0 * std::numbers::pi * 1000.0
                        * static_cast<double>(n) / rateHz));
}

// Feeds a steady tone to a set of channels from its own thread, one input
// block per channel per block period, the way RxDspWorker hands every bound
// slice each chunk the radio delivers. Each channel gets a block at its own
// input rate (WDSP reads its configured in_size from the buffer); a block is
// 64 output samples of real time at any rate (bufferSizeForRate). Records
// each channel's loudest output sample since the last resetPeaks(). The rate
// is read from the feeder thread; tests change it only while it is stopped.
class Feeder {
public:
    explicit Feeder(std::vector<RxChannel*> channels)
        : m_channels(std::move(channels))
    {
        m_thread = std::thread([this] { run(); });
    }

    ~Feeder() { stop(); }

    // Returns once the feeder is outside every processIq call and will make
    // no more.
    void pause()
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_pauseRequested = true;
        m_cv.wait(lk, [this] { return m_parked || m_quit; });
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

    void resetPeaks()
    {
        for (auto& p : m_peaks) {
            p.store(0.0f, std::memory_order_relaxed);
        }
    }

    float peak(int index) const
    {
        return m_peaks[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
    }

private:
    void run()
    {
        std::vector<long> n(m_channels.size(), 0);
        const std::chrono::microseconds period(64LL * 1000000LL / 48000LL);
        Clock::time_point next = Clock::now();
        std::vector<float> inI, inQ, outI, outQ;
        for (;;) {
            {
                std::unique_lock<std::mutex> lk(m_mutex);
                if (m_quit) {
                    return;
                }
                if (m_pauseRequested) {
                    m_parked = true;
                    m_cv.notify_all();
                    m_cv.wait(lk, [this] { return m_quit; });
                    return;
                }
            }
            for (std::size_t c = 0; c < m_channels.size(); ++c) {
                RxChannel* rx = m_channels[c];
                const int rateHz = rx->sampleRate();
                const int inSize = bufferSizeForRate(rateHz);
                const int outSize = inSize * 48000 / rateHz;
                inI.resize(static_cast<std::size_t>(inSize));
                inQ.assign(static_cast<std::size_t>(inSize), 0.0f);
                for (int i = 0; i < inSize; ++i) {
                    inI[static_cast<std::size_t>(i)] = toneSample(n[c]++, rateHz);
                }
                // RxDspWorker zeroes its scratch before every call; so does
                // this, so a channel WDSP does not exchange on reads silent.
                outI.assign(static_cast<std::size_t>(inSize), 0.0f);
                outQ.assign(static_cast<std::size_t>(inSize), 0.0f);
                rx->processIq(inI.data(), inQ.data(), outI.data(), outQ.data(),
                              inSize, outSize);
                float p = 0.0f;
                for (int i = 0; i < outSize; ++i) {
                    p = std::max(p, std::abs(outI[static_cast<std::size_t>(i)]));
                }
                auto& slot = m_peaks[c];
                if (p > slot.load(std::memory_order_relaxed)) {
                    slot.store(p, std::memory_order_relaxed);
                }
            }
            // One block of real time per round, as a radio delivers it.
            next += period;
            const Clock::time_point now = Clock::now();
            if (now > next + std::chrono::milliseconds(50)) {
                next = now;   // fell behind: do not burst to catch up
            }
            std::this_thread::sleep_until(next);
        }
    }

    std::vector<RxChannel*> m_channels;
    std::array<std::atomic<float>, kMaxFedChannels> m_peaks{};
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_pauseRequested{false};
    bool m_parked{false};
    bool m_quit{false};
    std::thread m_thread;
};

// The loudest output sample a channel produces from a steady tone, fed
// synchronously at `rateHz`: `settleBlocks` input blocks first, then the peak
// over the next `measureBlocks`. Only the settled blocks count, because a
// channel WDSP is about to silence still plays out its slew-down first.
double settledPeakFromATone(RxChannel* rx, int rateHz,
                            int settleBlocks = 100, int measureBlocks = 50)
{
    const int inSize = bufferSizeForRate(rateHz);
    const int outSize = inSize * 48000 / rateHz;
    std::vector<float> inI(static_cast<std::size_t>(inSize));
    std::vector<float> inQ(static_cast<std::size_t>(inSize), 0.0f);
    std::vector<float> outI(static_cast<std::size_t>(inSize));
    std::vector<float> outQ(static_cast<std::size_t>(inSize));
    double peak = 0.0;
    long n = 0;
    for (int block = 0; block < settleBlocks + measureBlocks; ++block) {
        for (int i = 0; i < inSize; ++i, ++n) {
            inI[static_cast<std::size_t>(i)] = toneSample(n, rateHz);
        }
        std::fill(outI.begin(), outI.end(), 0.0f);
        std::fill(outQ.begin(), outQ.end(), 0.0f);
        rx->processIq(inI.data(), inQ.data(), outI.data(), outQ.data(), inSize, outSize);
        if (block < settleBlocks) {
            continue;
        }
        for (int i = 0; i < outSize; ++i) {
            peak = std::max(peak, std::abs(static_cast<double>(outI[static_cast<std::size_t>(i)])));
        }
    }
    return peak;
}

double msSince(Clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// How long a fed channel may take to play at all. kWarmUp of real-time input
// is enough on an idle machine; on a loaded one (the Linux CI runner) a
// channel was still silent after it. This is the precondition before a stop,
// not a bound under test, so it is a deadline to wait to, not a fixed sleep.
constexpr std::chrono::milliseconds kAudibleDeadline{5000};

// Waits until each of the feeder's first `channels` channels has played (a
// peak above zero), polling, until kAudibleDeadline. The index of the first
// silent channel, or -1 once all play.
int firstSilentChannel(const Feeder& feeder, int channels)
{
    const Clock::time_point deadline = Clock::now() + kAudibleDeadline;
    for (;;) {
        int silent = -1;
        for (int c = 0; c < channels; ++c) {
            if (!(feeder.peak(c) > 0.0f)) {
                silent = c;
                break;
            }
        }
        if (silent < 0 || Clock::now() >= deadline) {
            return silent;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Detaches a stack-injected RadioConnection however the scope is left.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

} // namespace

class TestRxChannelStopFeed : public QObject {
    Q_OBJECT

private:
    static constexpr int kRateHz = 192000;

    RxChannel* openChannel(WdspEngine& engine, int id)
    {
        engine.m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        return engine.createRxChannel(id, bufferSizeForRate(kRateHz), kDspSize,
                                      kRateHz, kDspRateHz, 48000);
    }

private slots:
    void cleanup() { RealtimeTestLoad::printLoadAverageIfFailed(); }

    // A draining stop finishes once WDSP has slewed the channel down, which
    // takes a few blocks of real-time input, not WDSP's 100 ms timeout. The
    // channel still counts as running until then.
    void aDrainedStopFinishesWhileIqKeepsFlowing()
    {
        WdspEngine engine;
        RxChannel* rx = openChannel(engine, 0);
        QVERIFY(rx);
        rx->setActive(true);

        Feeder feeder({rx});
        QVERIFY2(firstSilentChannel(feeder, 1) < 0, "the channel is silent before the stop");
        // Then kWarmUp of steady input, so the stop lands in steady play,
        // not in WDSP's up-slew.
        std::this_thread::sleep_for(kWarmUp);

        const Clock::time_point t0 = Clock::now();
        rx->setActive(false);
        const double stopMs = msSince(t0);
        feeder.stop();

        QVERIFY(!rx->isActive());
        qInfo("draining stop with I/Q flowing: %.1f ms", stopMs);
        QVERIFY2(stopMs < kDspBlockMs,
                 qPrintable(QStringLiteral("the draining stop took %1 ms; one DSP "
                                           "block is %2 ms, WDSP's drain timeout 100 ms")
                                .arg(stopMs, 0, 'f', 1)
                                .arg(kDspBlockMs, 0, 'f', 1)));
    }

    // A channel stopped without a drain, and fed while it stops, restarts
    // without a rebuild and plays. Before Task 8 it restarted silent: the
    // slew-down flag the stop set was never cleared, so the first block after
    // the restart slewed down and cleared exchange (fix wave 1, C1).
    void aNoDrainStopFedThroughRestartsAudible()
    {
        WdspEngine engine;
        RxChannel* rx = openChannel(engine, 0);
        QVERIFY(rx);
        rx->setActive(true);

        Feeder feeder({rx});
        QVERIFY2(firstSilentChannel(feeder, 1) < 0, "the channel is silent before the stop");
        // Then kWarmUp of steady input, so the stop lands in steady play,
        // not in WDSP's up-slew.
        std::this_thread::sleep_for(kWarmUp);

        rx->deactivateWithoutDrain();
        QVERIFY(!rx->isActive());
        // Several DSP blocks of input: the slew-down completes in the first.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // The feed itself finished the stop, before any restart: processIq
        // saw its sentinel survive fexchange2 and cleared the pending stop.
        // A restart would hide a broken feed, since finishPendingStop then
        // completes the stop itself. The wait only absorbs a loaded machine.
        QTRY_VERIFY2_WITH_TIMEOUT(!rx->stopPendingForTest(),
                                  "the no-drain stop was still pending after "
                                  "the feed ran on through it",
                                  2000);

        const Clock::time_point t0 = Clock::now();
        rx->setActive(true);
        const double restartMs = msSince(t0);
        QVERIFY(rx->isActive());

        // Past the restart's up-slew, then only what the channel plays from
        // here on counts.
        std::this_thread::sleep_for(kWarmUp);
        feeder.resetPeaks();
        const bool audible = firstSilentChannel(feeder, 1) < 0;
        feeder.stop();

        QVERIFY2(audible, "the channel restarted silent after a no-drain stop");
        // Its stop had completed, so the restart does not wait for one.
        QVERIFY2(restartMs < kDspBlockMs,
                 qPrintable(QStringLiteral("the restart took %1 ms")
                                .arg(restartMs, 0, 'f', 1)));
    }

    // With no I/Q at all through a no-drain stop (a stream that stopped
    // delivering), nothing can complete WDSP's slew-down. The restart must
    // finish that stop itself rather than come back silent.
    void aNoDrainStopWithNoIqStillRestartsAudible()
    {
        WdspEngine engine;
        RxChannel* rx = openChannel(engine, 0);
        QVERIFY(rx);
        rx->setActive(true);
        QVERIFY2(settledPeakFromATone(rx, kRateHz) > 0.0, "silent before the stop");

        rx->deactivateWithoutDrain();
        rx->setActive(true);

        QVERIFY(rx->isActive());
        QVERIFY2(settledPeakFromATone(rx, kRateHz) > 0.0,
                 "the channel restarted silent after an unfed no-drain stop");
    }

    // A live rate change with five slices, I/Q flowing as from a radio. The
    // stops take at most one DSP block per channel (before Task 8 each one
    // waited out WDSP's 100 ms drain timeout, about half a second for five),
    // and every channel plays afterwards, including one that was already at
    // the new rate and so is not rebuilt.
    void aLiveRateChangeWithFiveSlicesStopsWithinOneDspBlockPerChannel()
    {
        constexpr int kNewRateHz = 384000;
        constexpr int kSlices = 5;

        RadioModel model;
        P1RadioConnection conn;
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};

        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(kSlices, kSlices, kRateHz);
        const std::array<double, kSlices> freqs{
            14200000.0, 7150000.0, 3700000.0, 21200000.0, 28400000.0};
        std::vector<RxChannel*> channels;
        std::vector<int> ids;
        for (double f : freqs) {
            const int id = model.addSlice();
            QVERIFY(id >= 0);
            model.sliceById(id)->setFrequency(f);
            ids.push_back(id);
        }
        model.openRxChannelPool(kSlices, bufferSizeForRate(kRateHz), kRateHz);
        for (int id : ids) {
            RxChannel* rx = engine->rxChannel(model.sliceById(id)->sliceIndex());
            QVERIFY(rx && rx->isActive());
            channels.push_back(rx);
        }
        QCOMPARE(channels.front()->channelId(), 0);

        // One slice's channel already runs at the new rate, as the per-slice
        // rate menu leaves it on Protocol 2; the change does not rebuild it.
        RxChannel* alreadyThere = channels.back();
        QVERIFY(engine->setRxChannelRate(alreadyThere->channelId(), kNewRateHz));

        // The pool's channels are set up and started on the receive lane
        // (RxChannel::runOrdered); a channel the lane has not reached yet is
        // silent. Let the lane finish first, so the warm-up below is real-time
        // input to running channels, not time spent waiting for the lane.
        QVERIFY(model.waitForReceiveLaneForTest(5000));

        // The feeder hands every channel one block per period at its own rate.
        Feeder feeder(channels);
        const int silent = firstSilentChannel(feeder, kSlices);
        QVERIFY2(silent < 0,
                 qPrintable(QStringLiteral("channel %1 silent before the change")
                                .arg(silent < 0 ? -1
                                                : channels[static_cast<std::size_t>(silent)]
                                                      ->channelId())));
        // Then kWarmUp of steady input, so the change starts in steady play,
        // not in WDSP's up-slew.
        std::this_thread::sleep_for(kWarmUp);

        // setSampleRateLive stops channel 0 last. Its off event marks the end
        // of the stops; production then disconnects the I/Q feed (step 2),
        // which here is the feeder parking before any channel is re-rated.
        Clock::time_point t0;
        double stopsMs = -1.0;
        QObject recorder;
        connect(channels.front(), &RxChannel::activeChanged, &recorder,
                [&](bool on) {
                    if (!on && stopsMs < 0.0) {
                        stopsMs = msSince(t0);
                        feeder.pause();
                    }
                }, Qt::DirectConnection);

        t0 = Clock::now();
        const qint64 elapsedMs = model.setSampleRateLive(kNewRateHz, false);
        feeder.stop();

        QVERIFY(elapsedMs >= 0);
        QVERIFY2(stopsMs >= 0.0, "channel 0 was never stopped");
        qInfo("five-slice rate change: stops %.1f ms, whole change %lld ms",
              stopsMs, static_cast<long long>(elapsedMs));
        const double stopBoundMs = kSlices * kDspBlockMs;
        QVERIFY2(stopsMs < stopBoundMs,
                 qPrintable(QStringLiteral("stopping %1 channels took %2 ms; the bound "
                                           "is one DSP block (%3 ms) per channel")
                                .arg(kSlices)
                                .arg(stopsMs, 0, 'f', 1)
                                .arg(kDspBlockMs, 0, 'f', 1)));
        const double totalBoundMs = kRateChangeFixedWaitsMs + stopBoundMs;
        QVERIFY2(elapsedMs < totalBoundMs,
                 qPrintable(QStringLiteral("the rate change took %1 ms; the bound is "
                                           "%2 ms")
                                .arg(elapsedMs)
                                .arg(totalBoundMs, 0, 'f', 1)));

        for (RxChannel* rx : channels) {
            QCOMPARE(rx->sampleRate(), kNewRateHz);
            QVERIFY(rx->isActive());
            QVERIFY2(settledPeakFromATone(rx, kNewRateHz) > 0.0,
                     qPrintable(QStringLiteral("channel %1 is silent after the change")
                                    .arg(rx->channelId())));
        }
    }
};

QTEST_MAIN(TestRxChannelStopFeed)
#include "tst_rx_channel_stop_feed.moc"
