// =================================================================
// tests/tst_spectrum_sample_clock.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. The Core's spectrum frames are stamped
// with the time of their samples (SpectrumSampleClock), and FFTEngine says
// where each frame's samples end (frameInputPosition) and counts what its
// 5 ms rule drops.
//
// Modification history (NereusSDR):
//   2026-10-21: Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include "core/FFTEngine.h"
#include "core/session/media/SpectrumSampleClock.h"

#include <algorithm>
#include <cmath>
#include <random>

using namespace NereusSDR;

namespace {

constexpr double kRateHz = 192'000.0;
constexpr qint64 kPairsPerFrame = 6'400;  // 30 fps at 192 kHz
constexpr qint64 kFramePeriodNs = 33'333'333;
constexpr qint64 kStartNs = 5'000'000'000LL;
constexpr qint64 kBaseLatencyNs = 10'000'000;

qint64 sampleTimeNs(qint64 position, double rateHz = kRateHz)
{
    return kStartNs + std::llround(static_cast<double>(position) * 1.0e9 / rateHz);
}

} // namespace

class TstSpectrumSampleClock : public QObject
{
    Q_OBJECT

private slots:
    // Evenly spaced samples published with up to 8 ms of jitter: the stamps
    // are evenly spaced, never after their publish and never before the
    // earliest publish latency.
    void jitteredPublishGivesEvenStamps()
    {
        SpectrumSampleClock clock;
        std::mt19937 random(21);
        std::uniform_int_distribution<qint64> jitter(0, 8'000'000);
        qint64 previous = 0;
        qint64 previousPublish = 0;
        qint64 largestPublishGapErrorNs = 0;
        qint64 largestStampGapErrorNs = 0;
        double squaredStampGapErrors = 0.0;
        int settledGaps = 0;
        for (int frame = 0; frame < 3000; ++frame) {
            const qint64 position = frame * kPairsPerFrame;
            const qint64 publishNs = sampleTimeNs(position) + kBaseLatencyNs + jitter(random);
            const qint64 stampNs = clock.stamp(position, kRateHz, publishNs);
            QVERIFY(stampNs <= publishNs);
            QVERIFY(stampNs >= sampleTimeNs(position) + kBaseLatencyNs - 2);
            if (frame > 0) {
                QVERIFY(stampNs > previous);
                largestPublishGapErrorNs = std::max(
                    largestPublishGapErrorNs,
                    std::abs(publishNs - previousPublish - kFramePeriodNs));
            }
            if (frame >= 50) {
                // Settled: the anchor has met a publish near the earliest.
                const qint64 errorNs = std::abs(stampNs - previous - kFramePeriodNs);
                largestStampGapErrorNs = std::max(largestStampGapErrorNs, errorNs);
                squaredStampGapErrors += double(errorNs) * double(errorNs);
                ++settledGaps;
            }
            previous = stampNs;
            previousPublish = publishNs;
        }
        const double rmsStampGapErrorNs = std::sqrt(squaredStampGapErrors / settledGaps);
        qInfo("publish gaps off by up to %.2f ms; stamp gaps by up to %.3f ms, rms %.3f ms",
              largestPublishGapErrorNs / 1.0e6, largestStampGapErrorNs / 1.0e6,
              rmsStampGapErrorNs / 1.0e6);
        // The publish times themselves were not even; the stamps are.
        QVERIFY(largestPublishGapErrorNs > 7'000'000);
        QVERIFY(largestStampGapErrorNs < 1'500'000);
        QVERIFY(rmsStampGapErrorNs < 300'000.0);
        QCOMPARE(clock.largeErrorReanchors(), quint64(0));
    }

    // Once settled, a stamp stays within 1.5 ms of where the sample count
    // puts it, against 8 ms of publish jitter.
    void settledStampsFollowTheSampleCount()
    {
        SpectrumSampleClock clock;
        std::mt19937 random(7);
        std::uniform_int_distribution<qint64> jitter(0, 8'000'000);
        qint64 settledOffset = 0;
        for (int frame = 0; frame < 3000; ++frame) {
            const qint64 position = frame * kPairsPerFrame;
            const qint64 publishNs = sampleTimeNs(position) + kBaseLatencyNs + jitter(random);
            const qint64 offsetNs = clock.stamp(position, kRateHz, publishNs)
                - sampleTimeNs(position);
            if (frame == 300) {
                settledOffset = offsetNs;
            } else if (frame > 300) {
                QVERIFY2(std::abs(offsetNs - settledOffset) < 1'500'000,
                         qPrintable(QStringLiteral("frame %1 moved %2 ns")
                                        .arg(frame).arg(offsetNs - settledOffset)));
            }
        }
    }

    // A radio whose sample clock is 100 ppm slow (its samples take longer
    // than the nominal rate says) is followed without a step.
    void aSlowRadioClockIsFollowed()
    {
        SpectrumSampleClock clock;
        qint64 previous = 0;
        for (int frame = 0; frame < 18'000; ++frame) {  // ten minutes
            const qint64 position = frame * kPairsPerFrame;
            const qint64 realNs = std::llround(static_cast<double>(position) * 1.0e9
                                               / kRateHz * (1.0 + 100.0e-6));
            const qint64 publishNs = kStartNs + realNs + kBaseLatencyNs;
            const qint64 stampNs = clock.stamp(position, kRateHz, publishNs);
            QVERIFY(std::abs(publishNs - stampNs) < 2'000'000);
            if (frame > 0) {
                QVERIFY(std::abs(stampNs - previous - kFramePeriodNs) < 100'000);
            }
            previous = stampNs;
        }
        QCOMPARE(clock.largeErrorReanchors(), quint64(0));
    }

    // And one 100 ppm fast.
    void aFastRadioClockIsFollowed()
    {
        SpectrumSampleClock clock;
        for (int frame = 0; frame < 18'000; ++frame) {
            const qint64 position = frame * kPairsPerFrame;
            const qint64 realNs = std::llround(static_cast<double>(position) * 1.0e9
                                               / kRateHz * (1.0 - 100.0e-6));
            const qint64 publishNs = kStartNs + realNs + kBaseLatencyNs;
            const qint64 stampNs = clock.stamp(position, kRateHz, publishNs);
            QVERIFY(stampNs <= publishNs);
            QVERIFY(publishNs - stampNs < 2'000'000);
        }
        QCOMPARE(clock.largeErrorReanchors(), quint64(0));
    }

    // Frames held up by a stall and published at once take their publish
    // times, as before this clock, and the frames after the stall are back
    // on the sample count.
    void stalledFramesTakeTheirPublishTimes()
    {
        SpectrumSampleClock clock;
        qint64 previous = 0;
        for (int frame = 0; frame < 100; ++frame) {
            const qint64 position = frame * kPairsPerFrame;
            qint64 publishNs = sampleTimeNs(position) + kBaseLatencyNs;
            if (frame >= 50 && frame < 55) {
                // Five frames held up to 133 ms, then all published at once.
                publishNs = sampleTimeNs(55 * kPairsPerFrame) + kBaseLatencyNs;
            }
            const qint64 stampNs = clock.stamp(position, kRateHz, publishNs);
            QVERIFY(stampNs > previous);
            if (frame >= 50 && frame < 55) {
                QVERIFY(stampNs - publishNs >= 0 && stampNs - publishNs <= 5);
            } else {
                QVERIFY2(std::abs(stampNs - publishNs) <= 5,
                         qPrintable(QStringLiteral("frame %1 off by %2 ns")
                                        .arg(frame).arg(stampNs - publishNs)));
            }
            previous = stampNs;
        }
        QCOMPARE(clock.largeErrorReanchors(), quint64(1));
    }

    // I/Q that arrives slower than its rate (a test's synthetic feed: 1026
    // pairs at 192 kHz every 50 ms) gives each frame its publish time.
    void aSlowFeedTakesThePublishTimes()
    {
        SpectrumSampleClock clock;
        for (int frame = 0; frame < 40; ++frame) {
            const qint64 publishNs = kStartNs + frame * 50'000'000LL;
            QCOMPARE(clock.stamp(frame * 1'026, kRateHz, publishNs), publishNs);
        }
        QCOMPARE(clock.largeErrorReanchors(), quint64(39));
    }

    // Samples lost without a reset leave every later frame late by what was
    // lost: the first such frame takes a new anchor, once.
    void lostSamplesTakeANewAnchor()
    {
        SpectrumSampleClock clock;
        constexpr qint64 kLostNs = 150'000'000;
        qint64 previous = 0;
        qint64 previousPublish = 0;
        for (int frame = 0; frame < 100; ++frame) {
            const qint64 position = frame * kPairsPerFrame;
            const qint64 publishNs = sampleTimeNs(position) + kBaseLatencyNs
                + (frame >= 40 ? kLostNs : 0);
            const qint64 stampNs = clock.stamp(position, kRateHz, publishNs);
            QVERIFY(stampNs > previous);
            if (frame >= 40) {
                // On the new anchor, to the nanosecond the sample count
                // rounds to.
                QVERIFY(std::abs(stampNs - publishNs) <= 1);
            }
            previous = stampNs;
            previousPublish = publishNs;
        }
        Q_UNUSED(previousPublish);
        QCOMPARE(clock.largeErrorReanchors(), quint64(1));
    }

    // A first frame published long after its samples (the engine's first
    // plan) is replaced as the anchor by the first frame far earlier than
    // the clock says, at once.
    void aLateFirstAnchorIsReplaced()
    {
        SpectrumSampleClock clock;
        const qint64 first = clock.stamp(0, kRateHz, sampleTimeNs(0) + 400'000'000);
        const qint64 second = clock.stamp(kPairsPerFrame, kRateHz,
                                          sampleTimeNs(kPairsPerFrame) + kBaseLatencyNs);
        QCOMPARE(clock.largeErrorReanchors(), quint64(1));
        // Stamps never go back: the second is one nanosecond after the first.
        QCOMPARE(second, first + 1);
        qint64 previous = second;
        for (int frame = 2; frame < 30; ++frame) {
            const qint64 position = frame * kPairsPerFrame;
            const qint64 stampNs = clock.stamp(position, kRateHz,
                                               sampleTimeNs(position) + kBaseLatencyNs);
            QVERIFY(stampNs > previous);
            previous = stampNs;
        }
        // Back on the sample grid within a second.
        const qint64 position = 30 * kPairsPerFrame;
        QCOMPARE(clock.stamp(position, kRateHz, sampleTimeNs(position) + kBaseLatencyNs),
                 sampleTimeNs(position) + kBaseLatencyNs);
    }

    // The anchor is a publish time: the first frame, the first after
    // reset(), after a rate change and after the position runs back. None of
    // these is a large error, and the stamps still increase.
    void newAnchors()
    {
        SpectrumSampleClock clock;
        QCOMPARE(clock.stamp(1'000, kRateHz, 7'000'000'000LL), 7'000'000'000LL);
        QCOMPARE(clock.stamp(1'000 + kPairsPerFrame, kRateHz, 7'040'000'000LL),
                 7'000'000'000LL + kFramePeriodNs + (7'040'000'000LL - 7'000'000'000LL
                                                     - kFramePeriodNs)
                                                        / SpectrumSampleClock::kLateCreepDivisor);

        clock.reset();
        QCOMPARE(clock.stamp(900'000, kRateHz, 8'000'000'000LL), 8'000'000'000LL);
        // A new rate.
        QCOMPARE(clock.stamp(910'000, 96'000.0, 8'050'000'000LL), 8'050'000'000LL);
        // A position that ran back (another engine).
        QCOMPARE(clock.stamp(10, 96'000.0, 8'100'000'000LL), 8'100'000'000LL);
        // A reset whose publish time is not later than the last stamp.
        clock.reset();
        QCOMPARE(clock.stamp(20, 96'000.0, 8'100'000'000LL), 8'100'000'001LL);
        // No rate: the publish time.
        QCOMPARE(clock.stamp(30, 0.0, 9'000'000'000LL), 9'000'000'000LL);
        QCOMPARE(clock.largeErrorReanchors(), quint64(0));
    }

    // Decimation and the frame rate only change which positions frames
    // fall on: at decimation 2 a 30 fps advance spans 12 800 input pairs,
    // and the stamps are 66.7 ms apart from the count alone.
    void decimationNeedsNoNewAnchor()
    {
        SpectrumSampleClock clock;
        qint64 previous = 0;
        for (int frame = 0; frame < 50; ++frame) {
            const qint64 position = frame * 2 * kPairsPerFrame;
            const qint64 stampNs = clock.stamp(position, kRateHz,
                                               sampleTimeNs(position) + kBaseLatencyNs);
            if (frame > 0) {
                QVERIFY(std::abs(stampNs - previous - 2 * kFramePeriodNs) <= 1);
            }
            previous = stampNs;
        }
    }

    // FFTEngine counts every pair fed, decimated or dropped, and a frame's
    // position is where its samples end. The second frame of a burst falls
    // under the 5 ms minimum gap: it is held, the rest of the batch is
    // dropped and counted, and the held frame goes out at the next batch.
    void engineFramePositionsAndMinimumGapSkips()
    {
        FFTEngine engine(3);
        engine.setFftSize(1024);
        engine.setSampleRate(48'000.0);
        engine.setOutputFps(60);  // advance 800 pairs, overlap 224
        QList<qint64> positions;
        connect(&engine, &FFTEngine::fftReadyLinear, this,
                [&](int, const QVector<float>&, double, double) {
                    positions.append(engine.frameInputPosition());
                }, Qt::DirectConnection);

        QVector<float> burst(4'000 * 2, 0.5f);
        engine.feedIQ(burst);
        QCOMPARE(positions, QList<qint64>({1'024}));
        QCOMPARE(engine.minimumGapSkips(), quint64(1));
        // The held frame filled at pair 1824; the 2176 after it were dropped.
        QCOMPARE(engine.minimumGapDroppedPairs(), quint64(4'000 - 1'824));

        QElapsedTimer waited;
        waited.start();
        QTRY_VERIFY(waited.elapsed() >= 6);
        engine.feedIQ(QVector<float>(100 * 2, 0.5f));
        QCOMPARE(positions, QList<qint64>({1'024, 4'000}));

        // Decimation passes over pairs; the count takes them all.
        FFTEngine decimated(4);
        decimated.setFftSize(1024);
        decimated.setSampleRate(48'000.0);
        decimated.setOutputFps(60);
        decimated.setDecimation(2);
        QList<qint64> decimatedPositions;
        connect(&decimated, &FFTEngine::fftReadyLinear, this,
                [&](int, const QVector<float>&, double, double) {
                    decimatedPositions.append(decimated.frameInputPosition());
                }, Qt::DirectConnection);
        decimated.feedIQ(QVector<float>(2'100 * 2, 0.5f));
        QCOMPARE(decimatedPositions, QList<qint64>({2'048}));
        QCOMPARE(decimated.minimumGapSkips(), quint64(0));
    }
};

QTEST_MAIN(TstSpectrumSampleClock)
#include "tst_spectrum_sample_clock.moc"
