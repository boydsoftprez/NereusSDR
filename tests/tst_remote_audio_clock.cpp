// =================================================================
// tests/tst_remote_audio_clock.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original deterministic acceptance test. It drives
// the already-attributed RemoteAudioRateMatcher through independent logical
// producer and consumer clocks; no device, timer, transport, or source port
// is involved.
// =================================================================

#include <QtTest>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>

#include "core/session/media/RemoteAudioRateMatcher.h"

using namespace NereusSDR;

namespace {

constexpr int kInputFrames = 1'920;
constexpr int kOutputFrames = 480;
constexpr int kRingFrames = 8'640;
constexpr int kSimulatedSeconds = 60 * 60;
constexpr qint64 kPostStartupTicks = 5'000'000;
constexpr double kToneAmplitude = 0.20;
constexpr double kLeftHz = 701.0;
constexpr double kRightHz = 1703.0;

QVector<float> stereoBlock(quint64 firstFrame)
{
    QVector<float> pcm(kInputFrames * RemoteAudioRateMatcher::kChannels);
    for (int frame = 0; frame < kInputFrames; ++frame) {
        const double seconds = static_cast<double>(firstFrame + frame)
            / RemoteAudioRateMatcher::kSampleRateHz;
        pcm[frame * 2] = static_cast<float>(kToneAmplitude
            * std::sin(2.0 * M_PI * kLeftHz * seconds));
        pcm[frame * 2 + 1] = static_cast<float>(kToneAmplitude
            * std::sin(2.0 * M_PI * kRightHz * seconds));
    }
    return pcm;
}

struct ClockRun {
    RemoteAudioRateMatcherStats stats;
    double maximumRatioDeviation = 0.0;
    double maximumPostStartupAdjacentDelta = 0.0;
    double leftSquareSum = 0.0;
    double rightSquareSum = 0.0;
    double crossProductSum = 0.0;
    quint64 postStartupFrames = 0;
    bool valid = true;
};

// One simulated hour at a clock offset, run in pieces (load findings 3):
// advanceTo() carries the producer and consumer clocks, the matcher and
// the running measurements on from where the last piece stopped, so the
// hour is the same deterministic run whether it goes at once or in parts.
class ClockSimulation {
public:
    static constexpr qint64 kTicksPerSecond = 1'000'000;

    explicit ClockSimulation(int ppm)
        : m_consumerRateDenominator(kTicksPerSecond + ppm)
    {
        if (!m_matcher.configure(kInputFrames, kOutputFrames, kRingFrames)) {
            m_run.valid = false;
        }
    }

    qint64 reachedTicks() const { return m_reachedTicks; }
    const ClockRun& run() const { return m_run; }

    // Integer phase accumulators keep both clocks deterministic. The network
    // producer is exactly 48 kHz; consumer is 48 kHz * (1 + ppm / 1e6).
    // Every event before endTicks runs, in time order (a producer tie
    // first), exactly as one pass to the hour's end runs them.
    void advanceTo(qint64 endTicks)
    {
        constexpr qint64 producerPeriodTicks = 40'000;
        constexpr qint64 consumerPeriodNumerator = 10'000'000'000LL;
        while (m_run.valid
               && (m_nextProducer < endTicks
                   || m_nextConsumerNumerator / m_consumerRateDenominator < endTicks)) {
            const qint64 nextConsumer = m_nextConsumerNumerator / m_consumerRateDenominator;
            if (m_nextProducer < endTicks && m_nextProducer <= nextConsumer) {
                if (!m_matcher.push(stereoBlock(m_sourceFrame))) {
                    m_run.valid = false;
                    break;
                }
                m_sourceFrame += kInputFrames;
                m_nextProducer += producerPeriodTicks;
                if (m_nextProducer >= m_nextDiagnosticTicks) {
                    m_run.maximumRatioDeviation = std::max(m_run.maximumRatioDeviation,
                        std::abs(m_matcher.stats().currentRatio - 1.0));
                    m_nextDiagnosticTicks += kTicksPerSecond;
                }
                continue;
            }

            const QVector<float> output = m_matcher.take();
            if (output.size() != kOutputFrames * RemoteAudioRateMatcher::kChannels) {
                m_run.valid = false;
                break;
            }
            for (int sample = 0; sample < output.size(); sample += 2) {
                if (!std::isfinite(output.at(sample)) || !std::isfinite(output.at(sample + 1))) {
                    m_run.valid = false;
                    break;
                }
                const bool postStartup = nextConsumer > kPostStartupTicks;
                if (postStartup) {
                    const double left = output.at(sample);
                    const double right = output.at(sample + 1);
                    if (m_hasPrevious) {
                        m_run.maximumPostStartupAdjacentDelta = std::max(
                            m_run.maximumPostStartupAdjacentDelta,
                            std::abs(left - static_cast<double>(m_previousLeft)));
                        m_run.maximumPostStartupAdjacentDelta = std::max(
                            m_run.maximumPostStartupAdjacentDelta,
                            std::abs(right - static_cast<double>(m_previousRight)));
                    }
                    m_run.leftSquareSum += left * left;
                    m_run.rightSquareSum += right * right;
                    m_run.crossProductSum += left * right;
                    ++m_run.postStartupFrames;
                }
                m_previousLeft = output.at(sample);
                m_previousRight = output.at(sample + 1);
                m_hasPrevious = true;
            }
            if (!m_run.valid) {
                break;
            }
            m_nextConsumerNumerator += consumerPeriodNumerator;
            if (nextConsumer >= m_nextDiagnosticTicks) {
                m_run.maximumRatioDeviation = std::max(m_run.maximumRatioDeviation,
                    std::abs(m_matcher.stats().currentRatio - 1.0));
                m_nextDiagnosticTicks += kTicksPerSecond;
            }
        }
        m_reachedTicks = endTicks;
        m_run.stats = m_matcher.stats();
    }

private:
    RemoteAudioRateMatcher m_matcher;
    const qint64 m_consumerRateDenominator;
    qint64 m_nextProducer = 0;
    qint64 m_nextConsumerNumerator = 0;
    quint64 m_sourceFrame = 0;
    bool m_hasPrevious = false;
    float m_previousLeft = 0.0f;
    float m_previousRight = 0.0f;
    // One schedule for both clocks' ratio samples, as the single pass had.
    qint64 m_nextDiagnosticTicks = kTicksPerSecond;
    qint64 m_reachedTicks = 0;
    ClockRun m_run;
};

} // namespace

class TstRemoteAudioClock : public QObject
{
    Q_OBJECT

private slots:
    void rejectsUndersizedRing()
    {
        RemoteAudioRateMatcher matcher;
        QVERIFY(!matcher.configure(kInputFrames, kOutputFrames, 4'032));
        QVERIFY(matcher.configure(kInputFrames, kOutputFrames, kRingFrames));
    }

    void rejectsMalformedPublicInput()
    {
        RemoteAudioRateMatcher matcher;
        QVERIFY(!matcher.configure(RemoteAudioRateMatcher::kMaxFramesPerCall + 1,
                                   kOutputFrames, kRingFrames));
        QVERIFY(!matcher.configure(kInputFrames,
                                   RemoteAudioRateMatcher::kMaxFramesPerCall + 1,
                                   kRingFrames));
        QVERIFY(!matcher.configure(kInputFrames, kOutputFrames,
                                   RemoteAudioRateMatcher::kMaxRingFrames + 1));
        QVERIFY(matcher.configure(kInputFrames, kOutputFrames, kRingFrames));

        QVector<float> malformed(kInputFrames * RemoteAudioRateMatcher::kChannels, 0.0f);
        malformed[0] = std::numeric_limits<float>::quiet_NaN();
        QVERIFY(!matcher.push(malformed));
        malformed[0] = std::numeric_limits<float>::infinity();
        QVERIFY(!matcher.push(malformed));
    }

    void resetRecreatesFreshConfiguration()
    {
        RemoteAudioRateMatcher matcher;
        QVERIFY(matcher.configure(kInputFrames, kOutputFrames, kRingFrames));
        QVERIFY(matcher.push(stereoBlock(0)));
        QCOMPARE(matcher.take().size(), kOutputFrames * RemoteAudioRateMatcher::kChannels);
        matcher.reset();

        const RemoteAudioRateMatcherStats stats = matcher.stats();
        QCOMPARE(stats.underflows, 0);
        QCOMPARE(stats.overflows, 0);
        QCOMPARE(stats.ringCapacityFrames, kRingFrames);
        QCOMPARE(matcher.take().size(), kOutputFrames * RemoteAudioRateMatcher::kChannels);
    }

    void carriesArbitraryPublicDimensionsThroughNativeBlocks()
    {
        RemoteAudioRateMatcher matcher;
        constexpr int inputFrames = 65;
        constexpr int outputFrames = 63;
        QVERIFY(matcher.configure(inputFrames, outputFrames, 640));
        QVector<float> pcm(inputFrames * RemoteAudioRateMatcher::kChannels, 0.0f);
        for (int frame = 0; frame < inputFrames; ++frame) {
            pcm[frame * 2] = 0.2f;
            pcm[frame * 2 + 1] = -0.2f;
        }
        for (int call = 0; call < 130; ++call) {
            QVERIFY(matcher.push(pcm));
            const QVector<float> output = matcher.take();
            QCOMPARE(output.size(), outputFrames * RemoteAudioRateMatcher::kChannels);
            for (float sample : output) {
                QVERIFY(std::isfinite(sample));
            }
        }
        const RemoteAudioRateMatcherStats stats = matcher.stats();
        QCOMPARE(stats.underflows, 0);
        QCOMPARE(stats.overflows, 0);
    }

    void reportsUnconsumedNativeOutputCarryInFill()
    {
        RemoteAudioRateMatcher matcher;
        QVERIFY(matcher.configure(64, 63, 640));
        const RemoteAudioRateMatcherStats before = matcher.stats();
        QCOMPARE(matcher.take().size(), 63 * RemoteAudioRateMatcher::kChannels);
        const RemoteAudioRateMatcherStats after = matcher.stats();
        QCOMPARE(after.ringFillFrames, before.ringFillFrames - 63);
        QCOMPARE(after.underflows, 0);
    }

    // R-R3-35 (I1): an input frame leaves take() kFilterDelayFrames after
    // the ring fill that was ahead of it when it was pushed. An impulse in a
    // lossless-sized push comes out at the varsamp FIR's centre, including
    // across a partly returned native output block.
    void filterDelayIsTheNamedConstant()
    {
        constexpr int kPacketFrames = 192;
        RemoteAudioRateMatcher matcher;
        QVERIFY(matcher.configure(kPacketFrames, kOutputFrames, kRingFrames));
        qint64 pushed = 0;
        QVector<float> out;
        const auto push = [&](int impulseAt) {
            QVector<float> pcm(kPacketFrames * RemoteAudioRateMatcher::kChannels, 0.0f);
            if (impulseAt >= 0) {
                pcm[impulseAt * 2] = 1.0f;
                pcm[impulseAt * 2 + 1] = 1.0f;
            }
            QVERIFY(matcher.push(pcm));
            pushed += kPacketFrames;
        };
        const auto take = [&] { out += matcher.take(); };
        for (int packet = 0; packet < 10; ++packet) { push(-1); }
        // Three takes leave half a native 64-frame block in the carry.
        take(); take(); take();
        const qint64 taken = out.size() / RemoteAudioRateMatcher::kChannels;
        const qint64 impulseFrame = pushed + 100;
        push(100);
        const qint64 end = pushed;
        const int fill = matcher.stats().ringFillFrames;
        while (out.size() / RemoteAudioRateMatcher::kChannels < taken + fill + 400) {
            push(-1);
            take();
        }
        QCOMPARE(matcher.stats().underflows, 0);
        QCOMPARE(matcher.stats().overflows, 0);
        qint64 peakFrame = -1;
        float peak = 0.0f;
        for (qint64 frame = 0; frame < out.size() / 2; ++frame) {
            if (std::abs(out.at(frame * 2)) > peak) {
                peak = std::abs(out.at(frame * 2));
                peakFrame = frame;
            }
        }
        QVERIFY(peak > 0.5f);
        // Without the filter the impulse would leave at taken + fill -
        // (end - impulseFrame): the fill ahead of the push, less the frames
        // pushed after it.
        const qint64 unfiltered = taken + fill - (end - impulseFrame);
        QCOMPARE(peakFrame - unfiltered, qint64(RemoteAudioRateMatcher::kFilterDelayFrames));
    }

    void readinessAccountsForNativeOutputBlockRounding()
    {
        RemoteAudioRateMatcher matcher;
        QVERIFY(matcher.configure(kInputFrames, kOutputFrames, kRingFrames));

        // Eight public 480-frame takes leave 480 frames and no 64-frame
        // output carry. The next request needs eight complete native blocks
        // (512 frames), so reported fill alone must not claim it is safe.
        for (int call = 0; call < 8; ++call) {
            QVERIFY(matcher.canTakeWithoutUnderflow());
            QCOMPARE(matcher.take().size(), kOutputFrames
                * RemoteAudioRateMatcher::kChannels);
        }
        QCOMPARE(matcher.stats().ringFillFrames, 480);
        QVERIFY(!matcher.canTakeWithoutUnderflow());
        QCOMPARE(matcher.stats().underflows, 0);

        QVERIFY(matcher.push(stereoBlock(0)));
        QVERIFY(matcher.canTakeWithoutUnderflow());
        QCOMPARE(matcher.take().size(), kOutputFrames
            * RemoteAudioRateMatcher::kChannels);
        QCOMPARE(matcher.stats().underflows, 0);
    }

    // One simulated hour per data row (R-R3-21). QtTest's watchdog bounds
    // each row, not the whole function, at 300 s; both hours in one row
    // took 347 s on a Linux aarch64 host and were aborted mid-hour.
    // Load findings 3: each hour runs as four quarter-hour data rows, so
    // QtTest's 300 s watchdog bounds a quarter, not the hour (the hour
    // passed 300 s above load 200). The rows carry one simulation on in
    // order; a row run on its own first runs the quarters before it. Every
    // row checks what must hold throughout; the last also checks the
    // hour's totals.
    void adaptiveClockStaysBoundedAtPlusAndMinus500Ppm_data()
    {
        QTest::addColumn<int>("ppm");
        QTest::addColumn<int>("quarter");
        for (const int ppm : {-500, 500}) {
            for (int quarter = 1; quarter <= kQuarters; ++quarter) {
                QTest::addRow("%+d ppm, quarter %d", ppm, quarter) << ppm << quarter;
            }
        }
    }

    void adaptiveClockStaysBoundedAtPlusAndMinus500Ppm()
    {
#ifndef HAVE_WDSP
        QSKIP("WDSP is disabled");
#else
        QFETCH(int, ppm);
        QFETCH(int, quarter);
        const qint64 quarterTicks =
            qint64(kSimulatedSeconds) * ClockSimulation::kTicksPerSecond / kQuarters;
        std::unique_ptr<ClockSimulation>& simulation = m_simulations[ppm];
        if (!simulation || simulation->reachedTicks() > quarterTicks * (quarter - 1)) {
            simulation = std::make_unique<ClockSimulation>(ppm);
        }
        simulation->advanceTo(quarterTicks * quarter);
        const ClockRun& run = simulation->run();
        QVERIFY(run.valid);
        QCOMPARE(run.stats.underflows, 0);
        QCOMPARE(run.stats.overflows, 0);
        QCOMPARE(run.stats.ringCapacityFrames, kRingFrames);
        QVERIFY(run.stats.ringFillFrames >= 0);
        QVERIFY(run.stats.ringFillFrames <= run.stats.ringCapacityFrames);
        QVERIFY(run.postStartupFrames > 0);

        // The 1703 Hz, 0.2-amplitude channel has the largest normal
        // sample-to-sample slope: 2A sin(pi*f/48000) is about 0.0445.
        // 0.08 permits normal filter interpolation while rejecting a
        // post-startup discontinuity or periodic repair splice.
        QVERIFY(run.maximumPostStartupAdjacentDelta < 0.08);
        if (quarter < kQuarters) {
            return;
        }

        // The staged 64-frame calls expose WDSP's native call cadence.
        // Its instantaneous ratio follows the two carry phases, so its
        // final sign is not a reliable clock-direction measurement. The
        // nonzero observed deviation verifies that force == 0 left the
        // existing feedback active; zero under/overflows above verify it
        // absorbed both independent clock drifts without repair.
        QVERIFY(run.maximumRatioDeviation > 0.00001);

        const double leftRms = std::sqrt(run.leftSquareSum / run.postStartupFrames);
        const double rightRms = std::sqrt(run.rightSquareSum / run.postStartupFrames);
        const double normalizedCrossCorrelation = run.crossProductSum
            / std::sqrt(run.leftSquareSum * run.rightSquareSum);
        QVERIFY(leftRms > 0.05 && leftRms < 0.30);
        QVERIFY(rightRms > 0.05 && rightRms < 0.30);
        QVERIFY(std::abs(normalizedCrossCorrelation) < 0.10);
        simulation.reset();
#endif
    }

private:
    static constexpr int kQuarters = 4;
    std::map<int, std::unique_ptr<ClockSimulation>> m_simulations;
};

QTEST_GUILESS_MAIN(TstRemoteAudioClock)
#include "tst_remote_audio_clock.moc"
