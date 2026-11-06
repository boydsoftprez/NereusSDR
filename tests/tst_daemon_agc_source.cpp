// =================================================================
// tests/tst_daemon_agc_source.cpp  (NereusSDR)
// =================================================================
// Headless Auto AGC source coverage.  The test reaches the same tagged
// RadioModel I/Q tap used by a daemon, without a display subscription.
// =================================================================

#include <QtTest>

#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/NoiseFloorTracker.h"
#include "core/daemon/DaemonAgcSource.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <algorithm>
#include <cmath>

using namespace NereusSDR;

namespace {

QVector<float> noiseIq(float amplitude)
{
    // Fixed, broad-band pseudo-noise: reproducible and unlike a single tone,
    // it supplies a meaningful average across FFT bins to NoiseFloorTracker.
    QVector<float> samples;
    samples.reserve(4096 * 2);
    quint32 state = 0x4d595df4U;
    for (int i = 0; i < 4096 * 2; ++i) {
        state = state * 1664525U + 1013904223U;
        const float unit = static_cast<float>((state >> 8) & 0xffffU) / 32767.5f - 1.0f;
        samples.append(unit * amplitude);
    }
    return samples;
}

struct Harness {
    RadioModel radio;
    SliceModel* first{nullptr};
    SliceModel* second{nullptr};
    int firstStream{-1};
    int secondStream{-1};

    Harness()
    {
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        const int firstId = radio.addSlice(QStringLiteral("agc-pan-a"));
        const int secondId = radio.addSlice(QStringLiteral("agc-pan-b"));
        Q_ASSERT(firstId >= 0 && secondId >= 0);
        first = radio.sliceById(firstId);
        second = radio.sliceById(secondId);
        Q_ASSERT(first && second);
        firstStream = first->streamIndex();
        secondStream = second->streamIndex();
        Q_ASSERT(firstStream >= 0 && secondStream >= 0 && firstStream != secondStream);
    }

    void feed(int streamIndex, float amplitude)
    {
        radio.forkIqToTapsForTest(streamIndex, noiseIq(amplitude));
    }

    bool warm(SliceModel* slice, int streamIndex, float amplitude)
    {
        // NoiseFloorTracker uses the explicitly supplied 33 ms source frame
        // cadence.  Give the worker enough bounded frames to pass its 2 s
        // convergence gate while yielding so its latest-frame notification
        // can run on this test's event loop.
        for (int frame = 0; frame < 90 && !slice->stationAutoAgcNoiseFloorValid(); ++frame) {
            feed(streamIndex, amplitude);
            QTest::qWait(35);
        }
        return slice->stationAutoAgcNoiseFloorValid();
    }
};

int expectedThreshold(const SliceModel& slice)
{
    const float calOffset = slice.agcMode() == AGCMode::Off ? 0.0f : 2.0f;
    const double threshold = slice.stationAutoAgcNoiseFloorDbm()
        + slice.autoAgcOffset() - calOffset;
    return static_cast<int>(std::round(std::clamp(threshold, -160.0, 2.0)));
}

} // namespace

class TstDaemonAgcSource final : public QObject {
    Q_OBJECT

private slots:
    void transmitStampedPacketCannotEnterSourceActivatedDuringItsFanout()
    {
        Harness h;
        qint64 clock = 0;
        DaemonSpectrumSource source;
        source.setRadioModel(&h.radio);
        source.setComputesBinsDbm(true);
        const MediaSourceKey key{h.firstStream, FftTier::Wide};
        DaemonSpectrumSourceConfig config;
        config.centreHz = h.radio.streamCentreHz(h.firstStream);
        config.sampleRateHz = h.radio.streamSampleRateHz(h.firstStream);
        config.maxPendingIqFloats = config.fft.fftSize * 4;
        NoiseFloorTracker tracker;
        for (int i = 0; i < 70; ++i) { tracker.feed(QVector<float>(4096, -85.0f), 33.0f); }
        const float held = tracker.agcNoiseFloor();
        auto* controller = h.radio.moxController();
        controller->setTimerIntervals(0, 0, 0, 0, 0, 0);
        controller->setMox(true);
        QTRY_VERIFY(h.radio.isTransmitting());
        const quint64 transmitEpoch = h.radio.autoAgcAcquisitionEpoch();
        QVERIFY((transmitEpoch & 1U) != 0);
        bool reactivated = false;
        // forkIqToTaps captures the TX epoch and emits the companion first.
        // Pause there; release TX and activate a new receiving source before
        // the same packet's ordinary untagged fan-out can resume.
        const auto pause = connect(&h.radio, &RadioModel::rawIqDataForAutoAgc, &h.radio,
            [&](int stream, const QVector<float>&, quint64 epoch) {
                if (reactivated || stream != key.streamIndex || epoch != transmitEpoch) { return; }
                controller->setMox(false);
                QTRY_VERIFY(!h.radio.autoAgcAcquisitionInhibited());
                config.autoAgcEpoch = h.radio.autoAgcAcquisitionEpoch();
                tracker.setAcquisitionEpoch(*config.autoAgcEpoch);
                QVERIFY(source.activate(key, config));
                QTRY_VERIFY(source.isActive(key));
                reactivated = true;
            }, Qt::DirectConnection);
        h.radio.forkIqToTapsForTest(h.firstStream, noiseIq(0.0f));
        disconnect(pause);
        QVERIFY(reactivated);
        h.radio.rawIqDataForAutoAgc(h.firstStream, noiseIq(0.0f), transmitEpoch);
        h.radio.rawIqDataForAutoAgc(h.firstStream, noiseIq(0.0f), transmitEpoch - 1);
        // A source configured for stamped AUTO input also rejects the
        // explicitly untagged non-radio/test input API.
        source.submitIq(h.firstStream, noiseIq(0.0f));
        const auto queue = source.inputQueueDiagnostics(key);
        QCOMPARE(queue.pendingIqFloats, 0);
        QVERIFY(!queue.drainQueued);
        QCOMPARE(source.completedInputHandoffs(key), quint64(0));
        // The setter locks the source state and refuses after a worker has
        // started a handoff, closing the race with an already-taken queue.
        QVERIFY(source.setEngineFrameGapClockForTest(key, [&]() { return clock += 10; }));
        QCOMPARE(source.publishedFrames(key), quint64(0));
        QVERIFY(!tracker.acquisitionCurrent());
        QCOMPARE(tracker.agcNoiseFloor(), held);
        h.radio.forkIqToTapsForTest(h.firstStream, noiseIq(0.15f));
        h.radio.forkIqToTapsForTest(h.firstStream, {0.0f, 0.0f});
        QTRY_VERIFY(source.publishedFrames(key) > 0);
        const auto fresh = source.takeLatest(key);
        QVERIFY(fresh && fresh->binsDbmValid);
        QCOMPARE(fresh->autoAgcEpoch, config.autoAgcEpoch);
        tracker.feedForEpoch(fresh->binsDbm, 33.0f, *fresh->autoAgcEpoch);
        QVERIFY(tracker.acquisitionCurrent());
        auto anotherEpoch = config;
        anotherEpoch.autoAgcEpoch = *config.autoAgcEpoch + 2;
        QVERIFY(!source.update(key, anotherEpoch)); // connection fixed at activation
        source.deactivate(key);
        anotherEpoch.autoAgcEpoch = transmitEpoch;
        QVERIFY(!source.activate(key, anotherEpoch)); // an epoch must be receiving
    }

    void controllerTxRetiresOnlyProducerAndPreservesSettledEstimator()
    {
        Harness h;
        DaemonAgcSource source(&h.radio);
        QVERIFY(h.warm(h.first, h.firstStream, 0.15f));
        NoiseFloorTracker* tracker = h.radio.noiseFloorTrackerForSlice(h.first);
        QVERIFY(tracker && tracker->isGood());
        const float held = tracker->agcNoiseFloor();
        h.first->setAutoAgcEnabled(true);
        h.radio.runAutoAgcTickForTest();
        const int threshold = h.first->agcThreshold();
        // Work queued before the edge cannot publish into the resumed
        // source; retirement clears its latest frame and input queue.
        h.feed(h.firstStream, 0.0f);
        auto* controller = h.radio.moxController();
        controller->setTimerIntervals(0, 0, 0, 0, 0, 0);
        controller->setMox(true);
        QTRY_VERIFY(h.radio.isTransmitting());
        QVERIFY(!h.radio.transmitModel().isMox());
        QCOMPARE(source.activeStreamCount(), 0);
        for (int i = 0; i < 100; ++i) { h.feed(h.firstStream, 0.0f); }
        QCoreApplication::processEvents();
        h.radio.runAutoAgcTickForTest();
        QCOMPARE(tracker->agcNoiseFloor(), held);
        QVERIFY(tracker->isGood());
        QCOMPARE(h.first->agcThreshold(), threshold);
        QVERIFY(!h.first->stationAutoAgcNoiseFloorValid());
        controller->setMox(false);
        QTRY_VERIFY(!h.radio.autoAgcAcquisitionInhibited());
        QCOMPARE(tracker, h.radio.noiseFloorTrackerForSlice(h.first));
        QCOMPARE(tracker->agcNoiseFloor(), held);
        QVERIFY(tracker->isGood());
        QVERIFY(!tracker->acquisitionCurrent());
        h.radio.runAutoAgcTickForTest();
        QCOMPARE(h.first->agcThreshold(), threshold);
        QVERIFY(!h.first->stationAutoAgcNoiseFloorValid());
        QVERIFY(h.warm(h.first, h.firstStream, 0.15f));
        QVERIFY(tracker->isGood());
        h.radio.runAutoAgcTickForTest();
        QCOMPARE(h.first->agcThreshold(), threshold);
    }

    void initTestCase()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        qRegisterMetaType<QVector<float>>();
        qRegisterMetaType<MediaSourceKey>();
    }

    void twoHeadlessStreamsPublishDistinctFloorsAndDriveTheirOwnAutoAgc()
    {
        Harness h;
        DaemonAgcSource source(&h.radio);
        QTRY_COMPARE(source.activeStreamCount(), 2);

        QVERIFY(h.warm(h.first, h.firstStream, 0.0f));
        QVERIFY(h.warm(h.second, h.secondStream, 0.20f));
        QVERIFY(std::abs(h.first->stationAutoAgcNoiseFloorDbm()
                         - h.second->stationAutoAgcNoiseFloorDbm()) > 10.0);

        h.first->setAutoAgcEnabled(true);
        h.second->setAutoAgcEnabled(true);
        h.first->setAgcThreshold(-42);
        h.second->setAgcThreshold(-42);
        h.radio.runAutoAgcTickForTest();
        QCOMPARE(h.first->agcThreshold(), expectedThreshold(*h.first));
        QCOMPARE(h.second->agcThreshold(), expectedThreshold(*h.second));
    }

    void retuneAndReceiveLifecycleRetireMeasurementsUntilTheyReconverge()
    {
        Harness h;
        DaemonAgcSource source(&h.radio);
        QVERIFY(h.warm(h.first, h.firstStream, 0.15f));

        const quint64 initialGeneration = h.first->stationAutoAgcNoiseFloorGeneration();
        h.first->setFrequency(h.first->frequency() + 100.0);
        QVERIFY(!h.first->stationAutoAgcNoiseFloorValid());
        QVERIFY(h.first->stationAutoAgcNoiseFloorGeneration() > initialGeneration);
        QVERIFY(h.warm(h.first, h.first->streamIndex(), 0.15f));

        h.radio.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        h.radio.moxController()->setMox(true);
        QTRY_VERIFY(h.radio.moxController()->isMox());
        QVERIFY(!h.radio.transmitModel().isMox());
        QVERIFY(!h.first->stationAutoAgcNoiseFloorValid());
        QCOMPARE(source.activeStreamCount(), 0);
        h.radio.moxController()->setMox(false);
        QTRY_VERIFY(!h.radio.isTransmitting());
        QVERIFY(h.warm(h.first, h.first->streamIndex(), 0.15f));

        h.radio.setConnectionStateForTest(ConnectionState::Disconnected);
        QVERIFY(!h.first->stationAutoAgcNoiseFloorValid());
        QCOMPARE(source.activeStreamCount(), 0);
        h.radio.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(h.warm(h.first, h.first->streamIndex(), 0.15f));

        h.radio.streamsSuspended({h.first->streamIndex()}, QStringLiteral("test"));
        QVERIFY(!h.first->stationAutoAgcNoiseFloorValid());
        QCOMPARE(source.activeStreamCount(), 1); // second stream remains receive-capable
        h.radio.streamsSuspended({}, QStringLiteral("resumed"));
        QVERIFY(h.warm(h.first, h.first->streamIndex(), 0.15f));
    }

    void streamUnbindInvalidatesImmediatelyAndTeardownUnregisters()
    {
        Harness h;
        auto source = std::make_unique<DaemonAgcSource>(&h.radio);
        QVERIFY(h.warm(h.first, h.firstStream, 0.15f));
        QVERIFY(h.radio.noiseFloorTrackerForSlice(h.first) != nullptr);

        const int oldStream = h.first->streamIndex();
        h.first->setStreamIndex(-1);
        QVERIFY(!h.first->stationAutoAgcNoiseFloorValid());
        QVERIFY(h.radio.noiseFloorTrackerForSlice(h.first) == nullptr);

        // Restore the actual previous DDC assignment.  This exercises a
        // retained StreamState whose DaemonSpectrumSource was deactivated.
        h.first->setStreamIndex(oldStream);
        QVERIFY(h.warm(h.first, oldStream, 0.15f));
        source.reset();
        QVERIFY(!h.first->stationAutoAgcNoiseFloorValid());
        QVERIFY(h.radio.noiseFloorTrackerForSlice(h.first) == nullptr);
    }
};

QTEST_MAIN(TstDaemonAgcSource)
#include "tst_daemon_agc_source.moc"
