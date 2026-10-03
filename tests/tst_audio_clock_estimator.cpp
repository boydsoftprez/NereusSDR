// =================================================================
// tests/tst_audio_clock_estimator.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. R-R3-35 measured audio delay:
// the clock offset from probe exchanges and the one-way delay with its
// accuracy, against simulated paths whose true delays are known.
// =================================================================

#include <QtTest/QtTest>

#include "gui/AudioClockEstimator.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <random>

using namespace NereusSDR;

namespace {

constexpr qint64 kMs = 1'000'000;
constexpr qint64 kS = 1'000'000'000;

// A simulated pair of clocks. True time is this computer's clock; the
// Core's clock runs at 1 + ppm * 1e-6 of it, plus an offset.
struct Clocks {
    qint64 coreOffsetNs = 0;
    double corePpm = 0.0;
    qint64 core(qint64 trueNs) const
    {
        return coreOffsetNs + std::llround(double(trueNs) * (1.0 + corePpm * 1e-6));
    }
};

// One probe sent at true time `sentNs`, taking `forwardNs` to reach the
// Core, held there `holdNs`, and `backNs` to come back.
AudioClockSample exchange(const Clocks& clocks, qint64 sentNs, qint64 forwardNs,
                          qint64 holdNs, qint64 backNs)
{
    return {sentNs, clocks.core(sentNs + forwardNs), clocks.core(sentNs + forwardNs + holdNs),
            sentNs + forwardNs + holdNs + backNs};
}

// The measurement for a sample the Core captured at true time `captureNs`
// (its anchor, RTP 480000) and this computer played out `delayNs` later,
// `spanFrames` after the anchor's RTP time. The playout point is read
// `queuedFrames` of queue before it is heard.
struct Scenario {
    Clocks clocks;
    qint64 captureNs = 20 * kS;
    qint64 delayNs = 85 * kMs;
    qint32 spanFrames = 0;
    int matcherFrames = 0;
    int speakerFrames = 0;
    std::optional<qint64> deviceNs;
    quint32 generation = 7;
    // Fixed pipeline delay (codec and rate matcher), the device callback,
    // where in its cycle the callback was when the queue was read (0 just
    // after a callback, 1 just before the next), and the read window with
    // where inside it the queue was actually read.
    int pipelineFrames = 0;
    int callbackFrames = 0;
    double callbackPhase = 0.5;
    qint64 readWindowNs = 0;
    double readInstant = 0.5;
};

AudioDelayInputs inputsFor(const Scenario& scenario, const AudioClockEstimator& estimator,
                           qint64 nowNs)
{
    constexpr quint32 kAnchorRtp = 480000;
    // The anchor sample's RTP time is kAnchorRtp; the played sample is
    // spanFrames later, which on the Core's clock is spanFrames / 48 kHz.
    const qint64 spanCoreNs = qint64(scenario.spanFrames) * kS / 48000;
    const double coreRate = 1.0 + scenario.clocks.corePpm * 1e-6;
    const qint64 playedCaptureNs = scenario.captureNs + std::llround(double(spanCoreNs) / coreRate);
    const qint64 heardNs = playedCaptureNs + scenario.delayNs;
    // The queue drains a callback at a time: the head of the queue read now
    // leaves with the next callback, callbackPhase of a callback away.
    const qint64 queuedNs = qint64(scenario.matcherFrames + scenario.speakerFrames
                                   + scenario.pipelineFrames) * kS / 48000
        + std::llround((1.0 - scenario.callbackPhase) * double(scenario.callbackFrames)
                       * double(kS) / 48000.0)
        + scenario.deviceNs.value_or(0);
    const qint64 readNs = heardNs - queuedNs;
    const qint64 windowStartNs = readNs - std::llround(scenario.readInstant
                                                       * double(scenario.readWindowNs));
    AudioDelayInputs inputs;
    inputs.offset = estimator.offset(nowNs);
    inputs.capture = AudioCaptureAnchor{scenario.generation, kAnchorRtp,
                                        scenario.clocks.core(scenario.captureNs)};
    inputs.playingGeneration = scenario.generation;
    RemoteAudioPlayoutPoint playout;
    playout.rtpTimestamp = quint32(kAnchorRtp + quint32(scenario.spanFrames));
    playout.measuredNs = windowStartNs + scenario.readWindowNs / 2;
    playout.matcherFillFrames = scenario.matcherFrames;
    playout.speakerQueuedFrames = scenario.speakerFrames;
    playout.deviceLatencyNs = scenario.deviceNs;
    playout.pipelineDelayFrames = scenario.pipelineFrames;
    playout.callbackFrames = scenario.callbackFrames;
    playout.readWindowNs = scenario.readWindowNs;
    inputs.playout = playout;
    return inputs;
}

} // namespace

class TstAudioClockEstimator : public QObject {
    Q_OBJECT
private slots:
    // The formulas themselves, on one exchange: offset
    // ((t1 - t0) + (t2 - t3)) / 2 and round trip (t3 - t0) - (t2 - t1).
    void offsetAndRoundTripFollowTheFormulas()
    {
        AudioClockEstimator estimator;
        QVERIFY(estimator.addSample({1000, 5'000'004'000, 5'000'004'500, 9000}));
        const std::optional<AudioClockOffset> offset = estimator.offset(9000);
        QVERIFY(offset);
        QCOMPARE(offset->offsetNs, qint64((5'000'004'000 - 1000 + 5'000'004'500 - 9000) / 2));
        QCOMPARE(offset->roundTripNs, qint64((9000 - 1000) - 500));
        QCOMPARE(offset->sampleNs, qint64(9000));
        QCOMPARE(offset->exchangeNs, qint64(8000));
        // Half the round trip, plus drift over the exchange itself.
        QCOMPARE(offset->boundNsAt(9000), 3750.0 + 8000.0 * 100.0 / 1e6);
    }

    // With the same time out and back, no buffering and no device, the
    // measured delay is exactly the injected one-way delay.
    void symmetricPathsWithoutBufferingMeasureTheInjectedDelay()
    {
        for (const qint64 oneWay : {qint64(1 * kMs), qint64(12 * kMs), qint64(140 * kMs)}) {
            Scenario scenario;
            scenario.clocks.coreOffsetNs = 5 * kS;
            scenario.delayNs = oneWay;
            AudioClockEstimator estimator;
            for (int probe = 0; probe < 20; ++probe) {
                QVERIFY(estimator.addSample(
                    exchange(scenario.clocks, probe * kS, oneWay, 300'000, oneWay)));
            }
            const auto estimate = measureAudioDelay(inputsFor(scenario, estimator, 20 * kS));
            QVERIFY(estimate);
            QVERIFY2(std::abs(estimate->delayMs - double(oneWay) / double(kMs)) < 1e-6,
                     qPrintable(QString::number(estimate->delayMs, 'f', 9)));
            // The accuracy is half the round trip plus drift, not a delay.
            QVERIFY(estimate->boundMs >= double(oneWay) / double(kMs));
            QVERIFY(!estimate->includesDevice);
        }
    }

    // Any mix of forward and return times, Core hold times, clock offsets
    // and drift within the assumed limit: the true delay always lies within
    // the value plus or minus the bound, and the bound is half the chosen
    // round trip plus drift, never the delay itself.
    void asymmetricPathsAlwaysContainTheTrueDelay()
    {
        std::mt19937_64 random(0x5eed35);
        std::uniform_int_distribution<qint64> pathMs(1, 300);
        std::uniform_int_distribution<qint64> holdUs(0, 5000);
        std::uniform_int_distribution<qint64> offsetS(-100000, 100000);
        std::uniform_real_distribution<double> ppm(-AudioClockEstimator::kMaxDriftPpm,
                                                   AudioClockEstimator::kMaxDriftPpm);
        std::uniform_int_distribution<qint64> delayMs(5, 2000);
        std::uniform_int_distribution<int> span(-48000, 48000);
        std::uniform_int_distribution<int> queued(0, 9600);
        int contained = 0;
        for (int trial = 0; trial < 2000; ++trial) {
            Scenario scenario;
            scenario.clocks.coreOffsetNs = offsetS(random) * kS + 1'000'000'000'000;
            scenario.clocks.corePpm = ppm(random);
            scenario.delayNs = delayMs(random) * kMs;
            scenario.spanFrames = span(random);
            scenario.matcherFrames = queued(random);
            scenario.speakerFrames = queued(random);
            if (trial % 2) { scenario.deviceNs = queued(random) * kS / 48000; }
            // I1: the fixed pipeline delay, the callback quantum and phase,
            // and a read window the worker may have been preempted inside.
            scenario.pipelineFrames = std::uniform_int_distribution<int>(0, 400)(random);
            scenario.callbackFrames = std::uniform_int_distribution<int>(0, 2048)(random);
            scenario.callbackPhase = std::uniform_real_distribution<double>(0.0, 1.0)(random);
            scenario.readWindowNs = std::uniform_int_distribution<qint64>(0, 20 * kMs)(random);
            scenario.readInstant = std::uniform_real_distribution<double>(0.0, 1.0)(random);
            AudioClockEstimator estimator;
            for (int probe = 0; probe < 16; ++probe) {
                const qint64 forward = pathMs(random) * kMs;
                const qint64 back = pathMs(random) * kMs / (trial % 3 + 1);
                QVERIFY(estimator.addSample(exchange(scenario.clocks, (4 + probe) * kS,
                                                     forward, holdUs(random) * 1000, back)));
            }
            const qint64 now = 20 * kS + 500 * kMs;
            const auto offset = estimator.offset(now);
            QVERIFY(offset);
            const auto estimate = measureAudioDelay(inputsFor(scenario, estimator, now));
            QVERIFY(estimate);
            const double truth = double(scenario.delayNs) / double(kMs);
            // One nanosecond of slack for the integer clock readings.
            QVERIFY2(std::abs(estimate->delayMs - truth) <= estimate->boundMs + 1e-6,
                     qPrintable(QStringLiteral("trial %1: %2 +- %3 against %4")
                                    .arg(trial).arg(estimate->delayMs).arg(estimate->boundMs)
                                    .arg(truth)));
            QVERIFY(estimate->boundMs >= double(offset->roundTripNs) / 2.0 / double(kMs));
            const RemoteAudioPlayoutPoint point = *inputsFor(scenario, estimator, now).playout;
            QVERIFY(estimate->boundMs >= double(offset->roundTripNs + 2 * point.accuracyNs())
                                              / 2.0 / double(kMs));
            QVERIFY(estimate->boundMs <= double(offset->roundTripNs) / 2.0 / double(kMs)
                                             + double(point.accuracyNs()) / double(kMs) + 3.0);
            QCOMPARE(estimate->includesDevice, scenario.deviceNs.has_value());
            const AudioDelayDisplay shown = roundAudioDelay(estimate->delayMs, estimate->boundMs);
            QVERIFY(std::abs(double(shown.valueMs) - truth) <= double(shown.accuracyMs) + 1e-6);
            ++contained;
        }
        QCOMPARE(contained, 2000);
    }

    // The sample with the lowest round trip is the one used; a slower path
    // around it does not move the estimate. Samples leave after 16 s.
    void theLowestRoundTripWithinTheWindowIsUsed()
    {
        Clocks clocks{3 * kS, 0.0};
        AudioClockEstimator estimator;
        QVERIFY(estimator.addSample(exchange(clocks, 0, 40 * kMs, 0, 40 * kMs)));
        QVERIFY(estimator.addSample(exchange(clocks, 1 * kS, 2 * kMs, 0, 1 * kMs)));
        QVERIFY(estimator.addSample(exchange(clocks, 2 * kS, 90 * kMs, 0, 5 * kMs)));
        std::optional<AudioClockOffset> offset = estimator.offset(3 * kS);
        QVERIFY(offset);
        QCOMPARE(offset->roundTripNs, 3 * kMs);
        QCOMPARE(offset->sampleNs, 1 * kS + 3 * kMs);
        // The drift over the sample's age is part of the bound.
        QCOMPARE(offset->boundNsAt(offset->sampleNs + 10 * kS),
                 1.5 * double(kMs)
                     + double(10 * kS + 3 * kMs) * AudioClockEstimator::kMaxDriftPpm / 1e6);

        // Later probes are all slower; once the good one is 16 s old it is
        // gone and the best of the rest is used.
        for (int probe = 3; probe <= 18; ++probe) {
            QVERIFY(estimator.addSample(exchange(clocks, probe * kS, 10 * kMs, 0, 10 * kMs)));
        }
        offset = estimator.offset(18 * kS + 20 * kMs);
        QVERIFY(offset);
        QCOMPARE(offset->roundTripNs, 20 * kMs);
        QVERIFY(estimator.sampleCount() <= 17);
    }

    // Echoes stop: three seconds after the newest one, nothing is measured.
    // Impossible exchanges are refused.
    void nothingWhenEchoesStopOrTimesAreImpossible()
    {
        Clocks clocks{kS, 0.0};
        AudioClockEstimator estimator;
        QVERIFY(estimator.addSample(exchange(clocks, 0, kMs, 0, kMs)));
        QVERIFY(estimator.offset(2 * kMs + AudioClockEstimator::kEchoStaleNs));
        QVERIFY(!estimator.offset(2 * kMs + AudioClockEstimator::kEchoStaleNs + 1));

        QVERIFY(!estimator.addSample({100, 10, 20, 50}));   // echo before probe
        QVERIFY(!estimator.addSample({0, 20, 10, 50}));     // Core answered before it heard
        QVERIFY(!estimator.addSample({0, 0, 100, 50}));     // held longer than the round trip
        QCOMPARE(estimator.sampleCount(), std::size_t(1));
        estimator.reset();
        QVERIFY(!estimator.offset(0));
    }

    // The delay needs every part, from one audio context and one stream.
    void nothingWithoutAMatchingContextOrStream()
    {
        Scenario scenario;
        AudioClockEstimator estimator;
        QVERIFY(estimator.addSample(exchange(scenario.clocks, 19 * kS, kMs, 0, kMs)));
        const qint64 now = 20 * kS;
        QVERIFY(measureAudioDelay(inputsFor(scenario, estimator, now)));

        AudioDelayInputs inputs = inputsFor(scenario, estimator, now);
        inputs.playingGeneration = scenario.generation + 1; // the context changed
        QVERIFY(!measureAudioDelay(inputs));
        inputs = inputsFor(scenario, estimator, now);
        inputs.capture->generation = 0; // the Core had no capture to report
        inputs.playingGeneration = 0;
        QVERIFY(!measureAudioDelay(inputs));
        inputs = inputsFor(scenario, estimator, now);
        inputs.offset.reset();
        QVERIFY(!measureAudioDelay(inputs));
        inputs = inputsFor(scenario, estimator, now);
        inputs.capture.reset();
        QVERIFY(!measureAudioDelay(inputs));
        inputs = inputsFor(scenario, estimator, now);
        inputs.playout.reset();
        QVERIFY(!measureAudioDelay(inputs));
        inputs = inputsFor(scenario, estimator, now);
        inputs.playout->rtpTimestamp = inputs.capture->rtpTimestamp + 48000 * 61;
        QVERIFY(!measureAudioDelay(inputs));
    }

    // Delivery (capture to leaving the reorder buffer) is its own figure,
    // and a known device latency is counted and said to be counted.
    void deliveryAndDeviceAreReportedSeparately()
    {
        Scenario scenario;
        scenario.delayNs = 120 * kMs;
        scenario.speakerFrames = 960;  // 20 ms
        scenario.matcherFrames = 480;  // 10 ms
        scenario.deviceNs = 15 * kMs;
        AudioClockEstimator estimator;
        QVERIFY(estimator.addSample(exchange(scenario.clocks, 19 * kS, 2 * kMs, 0, 2 * kMs)));
        AudioDelayInputs inputs = inputsFor(scenario, estimator, 20 * kS);
        // Released 75 ms after capture (45 ms before it was heard).
        inputs.release = RemoteAudioReleasePoint{inputs.playout->rtpTimestamp,
                                                 scenario.captureNs + 75 * kMs};
        const auto estimate = measureAudioDelay(inputs);
        QVERIFY(estimate);
        QVERIFY(estimate->includesDevice);
        QVERIFY(std::abs(estimate->delayMs - 120.0) < 1e-6);
        QVERIFY(estimate->deliveryMs);
        QVERIFY(std::abs(*estimate->deliveryMs - 75.0) < 1e-6);
        QVERIFY(estimate->deliveryBoundMs && *estimate->deliveryBoundMs >= 2.0);
        QCOMPARE(inputs.playout->playoutNs(),
                 inputs.playout->measuredNs + 30 * kMs + 15 * kMs);
    }

    void roundingKeepsTheTrueValueInside()
    {
        AudioDelayDisplay shown = roundAudioDelay(85.2, 0.4);
        QCOMPARE(shown.valueMs, qint64(85));
        QCOMPARE(shown.accuracyMs, qint64(1));
        shown = roundAudioDelay(85.6, 1.3);
        QCOMPARE(shown.valueMs, qint64(86));
        QCOMPARE(shown.accuracyMs, qint64(2));
        shown = roundAudioDelay(40.0, 0.0);
        QCOMPARE(shown.accuracyMs, qint64(1));
    }

    // I1: the playout time counts the fixed pipeline delay and half a
    // device callback; its accuracy is the other half callback plus half
    // the read window, and the delay's bound carries it.
    void playoutCountsThePipelineAndHalfACallback()
    {
        RemoteAudioPlayoutPoint point;
        point.measuredNs = 1000 * kMs;
        point.matcherFillFrames = 480;
        point.speakerQueuedFrames = 960;
        point.pipelineDelayFrames = 312 + 69;
        point.callbackFrames = 480;
        point.readWindowNs = 2 * kMs;
        const qint64 expected = 1000 * kMs + qint64(480 + 960 + 381) * kS / 48000 + 5 * kMs;
        QVERIFY(std::abs(point.playoutNs() - expected) <= 1);
        QCOMPARE(point.accuracyNs(), 5 * kMs + 1 * kMs);
        point.deviceLatencyNs = 12 * kMs;
        QVERIFY(std::abs(point.playoutNs() - expected - 12 * kMs) <= 1);

        Scenario scenario;
        scenario.pipelineFrames = 381;
        scenario.callbackFrames = 480;
        scenario.readWindowNs = 2 * kMs;
        AudioClockEstimator estimator;
        for (int probe = 0; probe < 20; ++probe) {
            QVERIFY(estimator.addSample(exchange(scenario.clocks, probe * kS, kMs, 0, kMs)));
        }
        const auto estimate = measureAudioDelay(inputsFor(scenario, estimator, 20 * kS));
        QVERIFY(estimate);
        // Half the 2 ms round trip, 0.1 ms of drift over the second since
        // the last probe, plus 6 ms of playout accuracy.
        QVERIFY2(estimate->boundMs >= 7.1 && estimate->boundMs < 7.2,
                 qPrintable(QString::number(estimate->boundMs, 'f', 4)));
        // Mid-cycle and mid-window: the figure is the true delay.
        QVERIFY(std::abs(estimate->delayMs - 85.0) < 0.001);
    }

    // Follow-up item 5 (R-R3-35): while the rate matcher corrects, the audio
    // ahead of the newest sample was made at its ratio (output frames per
    // input frame), so the sample heard at the reading was captured
    // play-time / ratio before the newest one, not play-time. The figure is
    // the heard sample's delay: the newest sample's less the stretch.
    void matcherRatioStretchIsCountedForTheHeardSample()
    {
        RemoteAudioPlayoutPoint point;
        point.measuredNs = 1000 * kMs;
        point.matcherFillFrames = 4800;
        point.speakerQueuedFrames = 960;
        point.pipelineDelayFrames = 312 + 69;
        point.codecDelayFrames = 312;
        point.callbackFrames = 48;
        QCOMPARE(point.matcherStretchNs(), qint64(0)); // ratio 1
        point.matcherRatio = 1.004;
        // Made by the matcher: fill, queue, filter delay and half a callback,
        // 4800 + 960 + 69 + 24 = 5853 frames = 121.9375 ms; the codec's 312
        // frames lie before it.
        const double made = 5853.0 * double(kS) / 48000.0;
        const qint64 expected = std::llround(made * (1.0 - 1.0 / 1.004));
        QCOMPARE(point.matcherStretchNs(), expected);
        QVERIFY(expected > 480'000 && expected < 490'000); // about 0.486 ms
        point.deviceLatencyNs = 10 * kMs; // the device's frames were made too
        QCOMPARE(point.matcherStretchNs(),
                 std::llround((made + 10.0 * double(kMs)) * (1.0 - 1.0 / 1.004)));
        point.matcherRatio = 0.996; // slowing: the newest sample's delay is less
        QVERIFY(point.matcherStretchNs() < 0);
        point.matcherRatio = std::numeric_limits<double>::quiet_NaN();
        QCOMPARE(point.matcherStretchNs(), qint64(0));

        // measureAudioDelay reports the heard sample's delay.
        Scenario scenario;
        scenario.pipelineFrames = 381;
        scenario.callbackFrames = 480;
        scenario.readWindowNs = 2 * kMs;
        AudioClockEstimator estimator;
        for (int probe = 0; probe < 20; ++probe) {
            QVERIFY(estimator.addSample(exchange(scenario.clocks, probe * kS, kMs, 0, kMs)));
        }
        AudioDelayInputs inputs = inputsFor(scenario, estimator, 20 * kS);
        const auto steady = measureAudioDelay(inputs);
        QVERIFY(steady);
        RemoteAudioPlayoutPoint stretched = *inputs.playout;
        stretched.codecDelayFrames = 312;
        stretched.matcherRatio = 1.002;
        inputs.playout = stretched;
        const auto correcting = measureAudioDelay(inputs);
        QVERIFY(correcting);
        QVERIFY(std::abs((steady->delayMs - correcting->delayMs)
                         - double(stretched.matcherStretchNs()) / 1e6) < 1e-6);
        QCOMPARE(correcting->boundMs, steady->boundMs);
    }
};

QTEST_GUILESS_MAIN(TstAudioClockEstimator)
#include "tst_audio_clock_estimator.moc"
