// =================================================================
// tests/tst_daemon_agc_source.cpp  (NereusSDR)
// =================================================================
// Headless Auto AGC source coverage.  The test reaches the same tagged
// RadioModel I/Q tap used by a daemon, without a display subscription.
// =================================================================

#include <QtTest>

#include "core/HpsdrModel.h"
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
        const bool invoked = QMetaObject::invokeMethod(
            &radio, "rawIqDataForStream", Qt::DirectConnection,
            Q_ARG(int, streamIndex), Q_ARG(QVector<float>, noiseIq(amplitude)));
        Q_ASSERT(invoked);
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

        h.radio.transmitModel().setMox(true);
        QVERIFY(!h.first->stationAutoAgcNoiseFloorValid());
        QCOMPARE(source.activeStreamCount(), 0);
        h.radio.transmitModel().setMox(false);
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
