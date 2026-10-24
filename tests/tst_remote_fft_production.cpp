// =================================================================
// tests/tst_remote_fft_production.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Headless source/pool production tests.
// =================================================================

#include <QtTest>

// The source's engines are private; the teardown-order case below reads
// them to watch one being destroyed.
#define private public
#include "core/session/media/DaemonSpectrumSource.h"
#include "core/spectrum/FftEnginePool.h"
#undef private
#include "core/FFTEngine.h"
#include "core/NoiseFloorTracker.h"
#include "models/RadioModel.h"

#include <QElapsedTimer>
#include <QScopeGuard>
#include <QSemaphore>
#include <QThread>

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <memory>
#include <numbers>

using namespace NereusSDR;

namespace {

DaemonSpectrumSourceConfig sourceConfig(int fftSize, double centreHz,
                                        double sampleRateHz)
{
    DaemonSpectrumSourceConfig config;
    config.fft.fftSize = fftSize;
    config.fft.fps = 60;
    config.fft.windowType = static_cast<int>(WindowFunction::Hann);
    config.centreHz = centreHz;
    config.sampleRateHz = sampleRateHz;
    // The production service must choose this budget explicitly.  The test
    // uses four 1024-complex-sample packets' worth of interleaved storage.
    config.maxPendingIqFloats = 8192;
    return config;
}

QVector<float> syntheticIq(int complexSamples, double cyclesPerSample)
{
    QVector<float> iq;
    iq.reserve(complexSamples * 2);
    for (int sample = 0; sample < complexSamples; ++sample) {
        const double phase = 2.0 * std::numbers::pi * cyclesPerSample * sample;
        iq.append(static_cast<float>(std::cos(phase)));
        iq.append(static_cast<float>(std::sin(phase)));
    }
    return iq;
}


// The conversion exactly as DaemonAgcSource::onFrameAvailable ran it on the
// main thread before it moved to the engine threads (2026-09-30), kept here
// as the reference the moved code must match bit for bit.
bool mainThreadAgcDbm(const QVector<float>& binsLinear, double dbmOffset,
                      QVector<float>& binsDbm)
{
    constexpr float kFftPowerFloor = 1.0e-20f;
    constexpr float kFftDbmFloor = -200.0f;
    binsDbm.clear();
    binsDbm.reserve(binsLinear.size());
    for (const float power : binsLinear) {
        if (!std::isfinite(power)) {
            return false;
        }
        const float dbm = power < kFftPowerFloor
            ? kFftDbmFloor
            : static_cast<float>(10.0 * std::log10(static_cast<double>(power))
                                 + dbmOffset);
        if (!std::isfinite(dbm)) {
            return false;
        }
        binsDbm.append(dbm);
    }
    return true;
}

bool bitIdentical(const QVector<float>& a, const QVector<float>& b)
{
    return a.size() == b.size()
        && (a.isEmpty()
            || std::memcmp(a.constData(), b.constData(),
                           size_t(a.size()) * sizeof(float)) == 0);
}

// QObject::receivers() is protected; a member pointer named through a
// derived class reads it on any QObject.
struct ReceiverCount : QObject {
    static int of(const QObject& object, const char* signal)
    {
        return (object.*(&ReceiverCount::receivers))(signal);
    }
};

int dominantBin(const QVector<float>& bins)
{
    int index = -1;
    float highest = -std::numeric_limits<float>::infinity();
    for (int i = 0; i < bins.size(); ++i) {
        if (bins.at(i) > highest) {
            highest = bins.at(i);
            index = i;
        }
    }
    return index;
}

} // namespace

class TstRemoteFftProduction : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        qRegisterMetaType<MediaSourceKey>();
    }

    void sourceDoesNotStartBeforeExplicitActivation()
    {
        DaemonSpectrumSource source;
        const MediaSourceKey key{0, FftTier::Wide};
        QVERIFY(source.activeSources().isEmpty());
        QVERIFY(!source.activate(key, {}));
        QVERIFY(source.activeSources().isEmpty());
    }

    // R-R3-49: the radio delivers raw I/Q to each engine from its
    // connection thread over a direct connection. When an engine is
    // destroyed while that route is still connected, Qt's ~QObject clears
    // the route's slot object under any emission already in flight, and the
    // connection thread then calls into the dying engine (the Core crash
    // at stop, tst_receive_layout_native). Destroying a source must take
    // the route down before any of its engines goes.
    void destroyingTheSourceDisconnectsTheRadioBeforeItsEngines()
    {
        RadioModel radio;
        const char* const iqSignal = SIGNAL(rawIqDataForStream(int,QList<float>));
        const int baseline = ReceiverCount::of(radio, iqSignal);
        auto source = std::make_unique<DaemonSpectrumSource>();
        source->setRadioModel(&radio);
        const MediaSourceKey key{0, FftTier::Wide};
        QVERIFY(source->activate(key, sourceConfig(1024, 7100000.0, 48000.0)));
        QTRY_VERIFY(source->isActive(key));
        QCOMPARE(ReceiverCount::of(radio, iqSignal), baseline + 1);

        const QList<NereusSDR::FFTEngine*> engines = source->m_pool->m_engines.values();
        QCOMPARE(engines.size(), 1);
        int routesWhenEngineDestroyed = -1;
        // destroyed() is emitted from ~QObject before Qt removes the
        // engine's incoming connections: exactly the moment of the crash.
        connect(engines.first(), &QObject::destroyed, this,
                [&radio, iqSignal, &routesWhenEngineDestroyed]() {
            routesWhenEngineDestroyed = ReceiverCount::of(radio, iqSignal);
        }, Qt::DirectConnection);

        source.reset();
        QCOMPARE(routesWhenEngineDestroyed, baseline);
        QCOMPARE(ReceiverCount::of(radio, iqSignal), baseline);
    }

    void rejectsUnknownTierAndNonFiniteHzPerBinTarget()
    {
        DaemonSpectrumSource source;
        auto config = sourceConfig(1024, 7100000.0, 48000.0);
        const MediaSourceKey invalidTier{0, static_cast<FftTier>(99)};
        QVERIFY(!source.activate(invalidTier, config));

        config.fft.hzPerBinTarget = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!source.activate({0, FftTier::Wide}, config));
        QVERIFY(source.activeSources().isEmpty());
    }

    void twoStreamsAndTiersProduceIndependentFrames()
    {
        DaemonSpectrumSource source;
        const MediaSourceKey stream0Wide{0, FftTier::Wide};
        const MediaSourceKey stream0Fine{0, FftTier::Fine};
        const MediaSourceKey stream1Wide{1, FftTier::Wide};
        QVERIFY(source.activate(stream0Wide, sourceConfig(1024, 7100000.0, 48000.0)));
        QVERIFY(source.activate(stream0Fine, sourceConfig(2048, 7100000.0, 48000.0)));
        // Double both rate and FFT size so this distinct stream has the
        // same Hz-per-bin mapping as stream 0 Wide.
        QVERIFY(source.activate(stream1Wide, sourceConfig(2048, 14200000.0, 96000.0)));
        QTRY_VERIFY(source.isActive(stream0Wide));
        QTRY_VERIFY(source.isActive(stream0Fine));
        QTRY_VERIFY(source.isActive(stream1Wide));

        QSignalSpy frames(&source, &DaemonSpectrumSource::frameAvailable);
        source.submitIq(0, syntheticIq(2050, 0.125));
        source.submitIq(1, syntheticIq(2050, 0.0625));
        QTRY_VERIFY(frames.count() >= 3);

        const auto wide = source.takeLatest(stream0Wide);
        const auto fine = source.takeLatest(stream0Fine);
        const auto stream1 = source.takeLatest(stream1Wide);
        QVERIFY(wide.has_value());
        QVERIFY(fine.has_value());
        QVERIFY(stream1.has_value());
        QCOMPARE(wide->binsLinear.size(), 1024);
        QCOMPARE(fine->binsLinear.size(), 2048);
        QCOMPARE(stream1->binsLinear.size(), 2048);
        QCOMPARE(wide->sampleRateHz, 48000.0);
        QCOMPARE(stream1->sampleRateHz, 96000.0);
        QCOMPARE(wide->centreHz, 7100000.0);
        QCOMPARE(stream1->centreHz, 14200000.0);
        QCOMPARE(48000.0 / wide->binsLinear.size(),
                 96000.0 / stream1->binsLinear.size());
        QVERIFY(wide->producedAtNs > 0);
        QVERIFY(fine->windowEnb > 0.0);
    }

    void updateChangesGenerationAndFrequencyContext()
    {
        DaemonSpectrumSource source;
        const MediaSourceKey key{0, FftTier::Wide};
        QVERIFY(source.activate(key, sourceConfig(1024, 7100000.0, 48000.0)));
        QTRY_VERIFY(source.isActive(key));

        QSignalSpy frames(&source, &DaemonSpectrumSource::frameAvailable);
        source.submitIq(0, syntheticIq(1026, 0.125));
        QTRY_VERIFY(frames.count() >= 1);
        const auto before = source.takeLatest(key);
        QVERIFY(before.has_value());

        QVERIFY(source.update(key, sourceConfig(2048, 10100000.0, 96000.0)));
        QTRY_VERIFY(source.isActive(key));
        source.submitIq(0, syntheticIq(2050, 0.0625));
        QTRY_VERIFY(frames.count() >= 2);
        const auto after = source.takeLatest(key);
        QVERIFY(after.has_value());
        QCOMPARE(after->binsLinear.size(), 2048);
        QCOMPARE(after->centreHz, 10100000.0);
        QCOMPARE(after->sampleRateHz, 96000.0);
        QVERIFY(after->generation > before->generation);
    }

    // R-DISP-RETUNE. This test was centreRetuneResetsSameSizeRateInputHistory
    // and pinned the opposite: that a center change dropped the engine's
    // input history so the first frame after it held no old samples. The
    // operator's choice is now that the picture carries on through a retune
    // as the local display does, so a center-only update relabels the
    // running source. What it must still guarantee is in the assertions:
    // no frame labeled with the old center is handed out after the move.
    void centerOnlyUpdateRelabelsTheRunningSource()
    {
        DaemonSpectrumSource source;
        const MediaSourceKey key{0, FftTier::Wide};
        QVERIFY(source.activate(key, sourceConfig(1024, 7100000.0, 48000.0)));
        QTRY_VERIFY(source.isActive(key));

        QSignalSpy frames(&source, &DaemonSpectrumSource::frameAvailable);
        // 1024 samples fill the window, sample 1025 runs the transform. At
        // 48 kHz and 60 frames a second the window then advances 800, so
        // the engine holds 224 samples of overlap plus the two that
        // followed: 226 of the old tone.
        source.submitIq(0, syntheticIq(1026, 0.125));
        QTRY_VERIFY(frames.count() >= 1);
        const auto oldFrame = source.takeLatest(key);
        QVERIFY(oldFrame.has_value());
        const int oldPeak = dominantBin(oldFrame->binsLinear);
        QVERIFY(oldPeak >= 0);
        const quint64 droppedBefore = source.droppedInputFrames(key);

        // A frame published before the move and not taken by anybody.
        QVERIFY(source.publishFrameForTest(key, oldFrame->producedAtNs + 1));

        DaemonSpectrumSourceUpdate result;
        QVERIFY(source.update(key, sourceConfig(1024, 10100000.0, 48000.0),
                              DaemonSpectrumCenterMove::Relabel, &result));
        QVERIFY(result.relabeled);
        QCOMPARE(result.fromGeneration, oldFrame->generation);
        QVERIFY(result.toGeneration > result.fromGeneration);
        // No restart: active at once, with no configuration under way.
        QVERIFY(source.isActive(key));
        const DaemonSpectrumSource::InputQueueDiagnostics queue =
            source.inputQueueDiagnostics(key);
        QVERIFY(queue.active);
        QVERIFY(!queue.configurationPending);
        QCOMPARE(queue.generation, result.toGeneration);
        // The old-center frame is gone, so nothing can be configured from it.
        QVERIFY(!source.takeLatest(key).has_value());

        // 798 more samples fill the window again (226 + 798 = 1024). A
        // restarted engine would hold only these 798 and need 1025.
        const int framesBefore = frames.count();
        quint64 handoffs = source.completedInputHandoffs(key);
        source.submitIq(0, syntheticIq(798, 0.25));
        QTRY_VERIFY(source.completedInputHandoffs(key) > handoffs);
        // One more sample runs the transform. The engine refuses a second
        // transform within 5 ms of the first and keeps its window full, so
        // the trigger is offered again, a bounded number of times: 798 + 200
        // samples stay short of the 1025 a restarted engine would need.
        constexpr int kMaxTriggers = 200;
        int triggers = 0;
        while (frames.count() == framesBefore && triggers < kMaxTriggers) {
            handoffs = source.completedInputHandoffs(key);
            source.submitIq(0, syntheticIq(1, 0.25));
            QTRY_VERIFY(source.completedInputHandoffs(key) > handoffs);
            QCoreApplication::processEvents();
            ++triggers;
        }
        QVERIFY2(frames.count() > framesBefore,
                 "the relabeled source lost its input history");
        const auto retuned = source.takeLatest(key);
        QVERIFY(retuned.has_value());
        QCOMPARE(retuned->centreHz, 10100000.0);
        QCOMPARE(retuned->generation, result.toGeneration);
        QCOMPARE(retuned->sampleRateHz, 48000.0);
        QCOMPARE(retuned->binsLinear.size(), 1024);
        QVERIFY(retuned->producedAtNs > oldFrame->producedAtNs);
        QCOMPARE(source.droppedInputFrames(key), droppedBefore);

        // The frame is what the local display would show: mostly the new
        // tone, with the overlap from before the move still in the window.
        const int newPeak = dominantBin(retuned->binsLinear);
        QVERIFY(newPeak >= 0);
        QVERIFY(newPeak != oldPeak);
        QVERIFY2(retuned->binsLinear.at(oldPeak)
                     > retuned->binsLinear.at(newPeak) * 1.0e-4f,
                 "the relabeled source dropped the overlap from before the move");
    }

    // R-DISP-RETUNE: the relabel is the display's choice alone. A caller
    // that does not ask for it (the auto-AGC noise floor source, level
    // calibration) gets what it had before: a center change restarts the
    // source and empties the engine's input history, so the first frame
    // under the new center holds no sample from before the move.
    void centreRetuneResetsSameSizeRateInputHistory()
    {
        DaemonSpectrumSource source;
        const MediaSourceKey key{0, FftTier::Wide};
        const auto firstConfig = sourceConfig(1024, 7100000.0, 48000.0);
        QVERIFY(source.activate(key, firstConfig));
        QTRY_VERIFY(source.isActive(key));

        QSignalSpy frames(&source, &DaemonSpectrumSource::frameAvailable);
        source.submitIq(0, syntheticIq(1026, 0.125));
        QTRY_VERIFY(frames.count() >= 1);
        const auto oldFrame = source.takeLatest(key);
        QVERIFY(oldFrame.has_value());
        const int oldPeak = dominantBin(oldFrame->binsLinear);
        QVERIFY(oldPeak >= 0);

        // Same FFT size and rate intentionally exercise the path where
        // setFftSize()/setSampleRate alone would retain FFTEngine overlap.
        DaemonSpectrumSourceUpdate result;
        result.relabeled = true;
        QVERIFY(source.update(key, sourceConfig(1024, 10100000.0, 48000.0),
                              DaemonSpectrumCenterMove::Restart, &result));
        QVERIFY(!result.relabeled);
        QCOMPARE(result.fromGeneration, oldFrame->generation);
        QVERIFY(result.toGeneration > result.fromGeneration);
        QTRY_VERIFY(source.isActive(key));

        // The engine held 226 samples of the old tone. Had it kept them, 798
        // more would fill its window and the next would run a transform. A
        // restarted engine holds nothing and needs 1025, so 998 make no
        // frame. The wait puts them clear of the 5 ms in which the engine
        // refuses a second transform, so a kept history would show.
        const int framesBefore = frames.count();
        QTest::qWait(10);
        const quint64 handoffs = source.completedInputHandoffs(key);
        source.submitIq(0, syntheticIq(998, 0.25));
        QTRY_VERIFY(source.completedInputHandoffs(key) > handoffs);
        QCoreApplication::processEvents();
        QVERIFY2(!source.takeLatest(key).has_value(),
                 "the restarted source kept input from before the retune");
        QCOMPARE(frames.count(), framesBefore);

        source.submitIq(0, syntheticIq(1026, 0.25));
        QTRY_VERIFY(frames.count() > framesBefore);
        const auto retuned = source.takeLatest(key);
        QVERIFY(retuned.has_value());
        QCOMPARE(retuned->centreHz, 10100000.0);
        QVERIFY(retuned->generation > oldFrame->generation);

        const int newPeak = dominantBin(retuned->binsLinear);
        QVERIFY(newPeak >= 0);
        QVERIFY(newPeak != oldPeak);
        QVERIFY2(retuned->binsLinear.at(oldPeak)
                     < retuned->binsLinear.at(newPeak) * 0.01f,
                 "the retuned generation retained energy from the old tone");
    }

    void anyOtherUpdateStillRestartsTheSource_data()
    {
        QTest::addColumn<QString>("change");
        QTest::newRow("sample rate") << QStringLiteral("sampleRate");
        QTest::newRow("FFT size") << QStringLiteral("fftSize");
        QTest::newRow("window") << QStringLiteral("window");
        QTest::newRow("decimation") << QStringLiteral("decimation");
        QTest::newRow("sample rate and center") << QStringLiteral("sampleRateAndCenter");
        // One row for each field DaemonSpectrumSource::onlyCenterDiffers
        // reads: a move of the center that comes with any of them is not a
        // relabel. Dropping a field from that function fails its row here.
        QTest::newRow("frame rate and center") << QStringLiteral("frameRateAndCenter");
        QTest::newRow("FFT size and center") << QStringLiteral("fftSizeAndCenter");
        QTest::newRow("window and center") << QStringLiteral("windowAndCenter");
        QTest::newRow("bin target and center") << QStringLiteral("binTargetAndCenter");
        QTest::newRow("thread count and center") << QStringLiteral("threadCountAndCenter");
        QTest::newRow("queue capacity and center")
            << QStringLiteral("queueCapacityAndCenter");
        QTest::newRow("transforms follow frame rate and center")
            << QStringLiteral("transformsFollowAndCenter");
        QTest::newRow("decimation and center") << QStringLiteral("decimationAndCenter");
    }

    // R-DISP-RETUNE: only the center is a label. Whatever changes what a
    // bin holds restarts the source as before: the first frame after it is
    // made of new input alone.
    void anyOtherUpdateStillRestartsTheSource()
    {
        QFETCH(QString, change);
        DaemonSpectrumSource source;
        const MediaSourceKey key{0, FftTier::Wide};
        QVERIFY(source.activate(key, sourceConfig(1024, 7100000.0, 48000.0)));
        QTRY_VERIFY(source.isActive(key));
        QSignalSpy frames(&source, &DaemonSpectrumSource::frameAvailable);
        source.submitIq(0, syntheticIq(1026, 0.125));
        QTRY_VERIFY(frames.count() >= 1);
        const auto oldFrame = source.takeLatest(key);
        QVERIFY(oldFrame.has_value());

        DaemonSpectrumSourceConfig next = sourceConfig(1024, 7100000.0, 48000.0);
        int feed = 1026;
        // A new tone that sits exactly on a bin of the restarted engine.
        double tone = 0.25;
        if (change.endsWith(QLatin1String("AndCenter"))) {
            next.centreHz = 10100000.0;
        }
        if (change == QLatin1String("sampleRate")
            || change == QLatin1String("sampleRateAndCenter")) {
            next.sampleRateHz = 96000.0;
        } else if (change == QLatin1String("frameRateAndCenter")) {
            next.fft.fps = 30;
        } else if (change == QLatin1String("binTargetAndCenter")) {
            // The width of a bin as it is, so the engine keeps its size.
            next.fft.hzPerBinTarget = 48000.0 / 1024.0;
        } else if (change == QLatin1String("threadCountAndCenter")) {
            next.fft.threadCount += 1;
        } else if (change == QLatin1String("queueCapacityAndCenter")) {
            next.maxPendingIqFloats = 16384;
        } else if (change == QLatin1String("transformsFollowAndCenter")) {
            next.transformsFollowFrameRate = true;
        } else if (change == QLatin1String("fftSize")
                   || change == QLatin1String("fftSizeAndCenter")) {
            next.fft.fftSize = 2048;
            feed = 2050;
        } else if (change == QLatin1String("window")
                   || change == QLatin1String("windowAndCenter")) {
            next.fft.windowType = static_cast<int>(WindowFunction::BlackmanHarris4);
        } else {
            next.decimation = 2;
            feed = 2052;
            // Every second sample reaches the FFT: 0.375 cycles a sample
            // there, clear of both the band edge and the old tone.
            tone = 0.1875;
        }
        DaemonSpectrumSourceUpdate result;
        result.relabeled = true;
        QVERIFY(source.update(key, next, DaemonSpectrumCenterMove::Relabel, &result));
        QVERIFY(!result.relabeled);
        QCOMPARE(result.fromGeneration, oldFrame->generation);
        QVERIFY(result.toGeneration > result.fromGeneration);
        QTRY_VERIFY(source.isActive(key));

        const int framesBefore = frames.count();
        source.submitIq(0, syntheticIq(feed, tone));
        QTRY_VERIFY(frames.count() > framesBefore);
        const auto restarted = source.takeLatest(key);
        QVERIFY(restarted.has_value());
        QCOMPARE(restarted->generation, result.toGeneration);
        QCOMPARE(restarted->centreHz, next.centreHz);
        QCOMPARE(restarted->sampleRateHz, next.sampleRateHz);
        QCOMPARE(restarted->binsLinear.size(), next.fft.fftSize);

        // No sample of the old tone is in the window: everything away from
        // the new tone is far below it. One kept overlap would show at
        // about four parts in a thousand of the peak.
        const int peak = dominantBin(restarted->binsLinear);
        QVERIFY(peak >= 0);
        constexpr int kMainLobeBins = 8;
        float elsewhere = 0.0f;
        for (int bin = 0; bin < restarted->binsLinear.size(); ++bin) {
            if (std::abs(bin - peak) > kMainLobeBins) {
                elsewhere = std::max(elsewhere, restarted->binsLinear.at(bin));
            }
        }
        QVERIFY2(elsewhere < restarted->binsLinear.at(peak) * 1.0e-4f,
                 "the restarted source retained energy from the old tone");
    }

    void stopAndRecreateDropsOldFrameAndGetsFreshGeneration()
    {
        DaemonSpectrumSource source;
        const MediaSourceKey key{1, FftTier::Fine};
        const auto config = sourceConfig(1024, 3500000.0, 48000.0);
        QVERIFY(source.activate(key, config));
        QTRY_VERIFY(source.isActive(key));
        QSignalSpy frames(&source, &DaemonSpectrumSource::frameAvailable);
        source.submitIq(1, syntheticIq(1026, 0.125));
        QTRY_VERIFY(frames.count() >= 1);
        const auto first = source.takeLatest(key);
        QVERIFY(first.has_value());

        source.deactivate(key);
        QVERIFY(!source.isActive(key));
        QVERIFY(!source.takeLatest(key).has_value());

        QVERIFY(source.activate(key, config));
        QTRY_VERIFY(source.isActive(key));
        source.submitIq(1, syntheticIq(1026, 0.125));
        QTRY_VERIFY(frames.count() >= 2);
        const auto recreated = source.takeLatest(key);
        QVERIFY(recreated.has_value());
        QVERIFY(recreated->generation > first->generation);
    }

    // Architecture design section 9.4: latest value wins at the producer.
    // Ten frames published while the owning thread cannot run its event
    // loop leave exactly one queued notification and one latest frame.
    void framesProducedWhileOwnerBlockedCoalesceToLatest()
    {
        // fps 1 makes the FFT advance a whole buffer, so each packet below
        // completes exactly one frame of its own tone with no overlap.
        auto config = sourceConfig(1024, 7100000.0, 48000.0);
        config.fft.fps = 1;
        const MediaSourceKey key{0, FftTier::Wide};
        constexpr int kFrames = 10;
        const auto tone = [](int frame) { return 0.03125 * (frame + 1); };

        DaemonSpectrumSource reference;
        QVERIFY(reference.activate(key, config));
        QTRY_VERIFY(reference.isActive(key));
        QSignalSpy referenceFrames(&reference, &DaemonSpectrumSource::frameAvailable);
        reference.submitIq(0, syntheticIq(1025, tone(kFrames - 1)));
        QTRY_VERIFY(referenceFrames.count() >= 1);
        const auto expected = reference.takeLatest(key);
        QVERIFY(expected.has_value());
        const int newestPeak = dominantBin(expected->binsLinear);
        QVERIFY(newestPeak >= 0);

        DaemonSpectrumSource source;
        QVERIFY(source.activate(key, config));
        QTRY_VERIFY(source.isActive(key));
        QSignalSpy frames(&source, &DaemonSpectrumSource::frameAvailable);

        // From here the test thread runs no events until all ten frames exist.
        for (int frame = 0; frame < kFrames; ++frame) {
            const quint64 before = source.completedInputHandoffs(key);
            // The first packet also fills the empty buffer's leading sample.
            source.submitIq(0, syntheticIq(frame == 0 ? 1025 : 1024, tone(frame)));
            QElapsedTimer waited;
            waited.start();
            while (source.completedInputHandoffs(key) == before) {
                QVERIFY2(waited.elapsed() < 5000, "engine thread did not take the packet");
                QThread::msleep(1);
            }
            // Past FFTEngine's 5 ms defensive emit cap before the next frame.
            QThread::msleep(10);
        }
        QCOMPARE(frames.count(), 0);
        // Every packet really made its own frame; the slot kept only the last.
        QCOMPARE(source.publishedFrames(key), quint64(kFrames));

        QCoreApplication::processEvents();
        QTest::qWait(50);
        QCOMPARE(frames.count(), 1);
        const auto latest = source.takeLatest(key);
        QVERIFY(latest.has_value());
        QCOMPARE(dominantBin(latest->binsLinear), newestPeak);
        // The frame taken says how many were replaced before it.
        QCOMPARE(latest->replacedBefore, quint32(kFrames - 1));
        QVERIFY(!source.takeLatest(key).has_value());
        QTest::qWait(20);
        QCOMPARE(frames.count(), 1);
    }

    // The slot holds one frame. A frame taken from it says how many frames
    // before it were replaced there with nobody having taken them.
    void takenFrameCountsTheFramesReplacedBeforeIt()
    {
        DaemonSpectrumSource source;
        const MediaSourceKey key{0, FftTier::Wide};
        QVERIFY(source.activate(key, sourceConfig(1024, 7100000.0, 48000.0)));
        // The engine takes its configuration on its own thread; no I/Q is
        // fed, so every frame below comes from here. Each is published
        // once: a retried publish would itself replace a frame.
        const auto ready = [&source, &key] {
            const DaemonSpectrumSource::InputQueueDiagnostics queue =
                source.inputQueueDiagnostics(key);
            return queue.active && !queue.configurationPending;
        };
        QTRY_VERIFY(ready());
        QVERIFY(source.publishFrameForTest(key, 1'000));
        auto frame = source.takeLatest(key);
        QVERIFY(frame.has_value());
        QCOMPARE(frame->replacedBefore, quint32(0));

        // Three frames with nobody taking: the two older ones are replaced.
        QVERIFY(source.publishFrameForTest(key, 2'000));
        QVERIFY(source.publishFrameForTest(key, 3'000));
        QVERIFY(source.publishFrameForTest(key, 4'000));
        frame = source.takeLatest(key);
        QVERIFY(frame.has_value());
        QCOMPARE(frame->producedAtNs, qint64(4'000));
        QCOMPARE(frame->replacedBefore, quint32(2));

        // The count starts again with each frame taken.
        QVERIFY(source.publishFrameForTest(key, 5'000));
        frame = source.takeLatest(key);
        QVERIFY(frame.has_value());
        QCOMPARE(frame->replacedBefore, quint32(0));

        // Frames a retune discards were not lost to a slow taker: the frame
        // replaced before the move and the one the move discarded are both
        // left out of the next frame's count.
        QVERIFY(source.publishFrameForTest(key, 6'000));
        QVERIFY(source.publishFrameForTest(key, 7'000));
        DaemonSpectrumSourceUpdate result;
        QVERIFY(source.update(key, sourceConfig(1024, 10100000.0, 48000.0),
                              DaemonSpectrumCenterMove::Relabel, &result));
        QVERIFY(result.relabeled);
        QVERIFY(!source.takeLatest(key).has_value());
        QVERIFY(source.publishFrameForTest(key, 8'000));
        frame = source.takeLatest(key);
        QVERIFY(frame.has_value());
        QCOMPARE(frame->replacedBefore, quint32(0));
    }

    // Waterfall row times at the source: frames whose samples are evenly
    // spaced are stamped evenly however late the FFT published them, each
    // frame carries the spacing from the frame before it, and an input
    // reset (a MOX edge, a dropped packet) starts a new anchor.
    void framesAreStampedWithTheTimeOfTheirSamples()
    {
        DaemonSpectrumSource source;
        const MediaSourceKey key{0, FftTier::Wide};
        QVERIFY(source.activate(key, sourceConfig(1024, 7100000.0, 48000.0)));
        const auto ready = [&source, &key] {
            const DaemonSpectrumSource::InputQueueDiagnostics queue =
                source.inputQueueDiagnostics(key);
            return queue.active && !queue.configurationPending;
        };
        QTRY_VERIFY(ready());

        // 1600 pairs at 48 kHz: a frame every 33.3 ms, published up to 8 ms
        // after its samples.
        constexpr qint64 kPairsPerFrame = 1600;
        constexpr qint64 kFramePeriodNs = 33'333'333;
        constexpr qint64 kBaseNs = 5'000'000'000;
        constexpr std::array<qint64, 8> kPublishDelayNs{
            0, 7'000'000, 2'000'000, 6'000'000, 1'000'000, 8'000'000, 0, 4'000'000};
        qint64 previousNs = 0;
        qint64 largestPublishGapErrorNs = 0;
        for (int index = 0; index < int(kPublishDelayNs.size()); ++index) {
            const qint64 publishedAtNs =
                kBaseNs + index * kFramePeriodNs + kPublishDelayNs[size_t(index)];
            const std::optional<qint64> stampNs = source.publishSampledFrameForTest(
                key, (index + 1) * kPairsPerFrame, publishedAtNs);
            QVERIFY(stampNs.has_value());
            QVERIFY(*stampNs <= publishedAtNs);
            const auto frame = source.takeLatest(key);
            QVERIFY(frame.has_value());
            QCOMPARE(frame->producedAtNs, *stampNs);
            if (index == 0) {
                QCOMPARE(*stampNs, publishedAtNs);
                QCOMPARE(frame->sourceFrameGapNs, qint64(0));
            } else {
                QCOMPARE(frame->sourceFrameGapNs, *stampNs - previousNs);
                // The sample spacing, but for the clock's creep toward late
                // publishes (at most 50 us a frame), which a frame published
                // at the earliest latency takes back.
                QVERIFY2(std::abs(frame->sourceFrameGapNs - kFramePeriodNs) <= 400'000,
                         qPrintable(QStringLiteral("frame %1 gap %2 ns")
                                        .arg(index).arg(frame->sourceFrameGapNs)));
                largestPublishGapErrorNs = std::max(largestPublishGapErrorNs,
                    std::abs(kPublishDelayNs[size_t(index)]
                             - kPublishDelayNs[size_t(index - 1)]));
            }
            previousNs = *stampNs;
        }
        // The publish times themselves were 8 ms uneven.
        QCOMPARE(largestPublishGapErrorNs, qint64(8'000'000));
        QCOMPARE(source.sampleClockReanchors(key), quint64(0));
        QCOMPARE(source.inputQueueOverflows(key), quint64(0));
        QCOMPARE(source.engineMinimumGapSkips(key), quint64(0));

        // After an input reset the samples are not continuous: the next
        // frame is stamped with its own publish time and has no spacing.
        const quint64 resets = source.inputHistoryResets(key);
        source.resetInputHistoryAll();
        QTRY_VERIFY(source.inputHistoryResets(key) > resets);
        const qint64 afterResetNs = kBaseNs + 20 * kFramePeriodNs + 3'000'000;
        const std::optional<qint64> resetStampNs = source.publishSampledFrameForTest(
            key, 9 * kPairsPerFrame, afterResetNs);
        QVERIFY(resetStampNs.has_value());
        QCOMPARE(*resetStampNs, afterResetNs);
        const auto afterReset = source.takeLatest(key);
        QVERIFY(afterReset.has_value());
        QCOMPARE(afterReset->sourceFrameGapNs, qint64(0));
        QCOMPARE(source.sampleClockReanchors(key), quint64(0));
    }

    void ingressIsBoundedAndReportsDroppedWholeFrames()
    {
        DaemonSpectrumSource source;
        const MediaSourceKey key{0, FftTier::Wide};
        auto config = sourceConfig(1024, 7100000.0, 48000.0);
        config.maxPendingIqFloats = 2048;
        QVERIFY(source.activate(key, config));
        QTRY_VERIFY(source.isActive(key));

        source.submitIq(0, syntheticIq(1025, 0.125));
        source.submitIq(0, syntheticIq(1025, 0.125));
        QTRY_VERIFY(source.droppedInputFrames(key) > 0);
    }

    void validNewPacketSurvivesPendingOverflow()
    {
        const MediaSourceKey key{0, FftTier::Wide};
        auto config = sourceConfig(1024, 7100000.0, 48000.0);
        config.maxPendingIqFloats = 4096; // Core's FFT-size * 4 budget.

        DaemonSpectrumSource reference;
        QVERIFY(reference.activate(key, config));
        QTRY_VERIFY(reference.isActive(key));
        QSignalSpy referenceFrames(&reference, &DaemonSpectrumSource::frameAvailable);
        reference.submitIq(0, syntheticIq(1026, 0.25));
        QTRY_VERIFY(referenceFrames.count() > 0);
        const auto expected = reference.takeLatest(key);
        QVERIFY(expected.has_value());

        DaemonSpectrumSource source;
        QVERIFY(source.activate(key, config));
        QTRY_VERIFY(source.isActive(key));
        QSignalSpy frames(&source, &DaemonSpectrumSource::frameAvailable);
        // A partial old-tone history has already reached the FFT ring. The
        // pending overflow must reset that history before the new packet.
        source.submitIq(0, syntheticIq(511, 0.125));
        QTRY_COMPARE(source.completedInputHandoffs(key), quint64(1));
        QCOMPARE(frames.count(), 0);
        NereusSDR::FFTEngine* engine = source.m_pool->engineForSource(key, config.fft);
        QVERIFY(engine);
        auto entered = std::make_shared<QSemaphore>();
        auto release = std::make_shared<QSemaphore>();
        bool releaseNeeded = true;
        const auto unblock = qScopeGuard([release, &releaseNeeded] {
            if (releaseNeeded) { release->release(); }
        });
        QVERIFY(QMetaObject::invokeMethod(engine, [entered, release] {
            entered->release();
            release->acquire();
        }, Qt::QueuedConnection));
        QVERIFY(entered->tryAcquire(1, 5000));

        // Both packets are individually valid. With the worker paused, their
        // combined 4,104 floats exceed the 4,096-float pending budget by 8.
        // The newest whole packet must replace the old pending data after a
        // gap, rather than both packets disappearing.
        source.submitIq(0, syntheticIq(1026, 0.125));
        source.submitIq(0, syntheticIq(1026, 0.25));
        QCOMPARE(source.droppedInputFrames(key), quint64(1));
        QCOMPARE(source.inputQueueOverflows(key), quint64(1));
        const auto blocked = source.inputQueueDiagnostics(key);
        QCOMPARE(blocked.maxPendingIqFloats, 4096);
        QCOMPARE(blocked.pendingIqFloats, 2052);
        QVERIFY(blocked.drainQueued);

        releaseNeeded = false;
        release->release();
        QTRY_VERIFY(source.completedInputHandoffs(key) > 1);
        QTRY_VERIFY(frames.count() > 0);
        const auto actual = source.takeLatest(key);
        QVERIFY(actual.has_value());
        QCOMPARE(dominantBin(actual->binsLinear), dominantBin(expected->binsLinear));
        QCOMPARE(actual->binsLinear, expected->binsLinear);
    }

    void overflowCreatesInputHistoryDiscontinuity()
    {
        DaemonSpectrumSource source;
        const MediaSourceKey key{0, FftTier::Wide};
        auto config = sourceConfig(1024, 7100000.0, 48000.0);
        // The rejected middle packet is one float over this explicit bound.
        // The first and suffix packets both fit, so a source that only counts
        // overflow would concatenate their unrelated tones into one history.
        config.maxPendingIqFloats = 8192;
        QVERIFY(source.activate(key, config));
        QTRY_VERIFY(source.isActive(key));

        // Obtain the old-tone position from an independent, ordinary FFT
        // rather than depending on a bin-layout constant in this test.
        DaemonSpectrumSource reference;
        QVERIFY(reference.activate(key, config));
        QTRY_VERIFY(reference.isActive(key));
        QSignalSpy referenceFrames(&reference, &DaemonSpectrumSource::frameAvailable);
        reference.submitIq(0, syntheticIq(1026, 0.125));
        QTRY_VERIFY(referenceFrames.count() >= 1);
        const auto oldFrame = reference.takeLatest(key);
        QVERIFY(oldFrame.has_value());
        const int oldPeak = dominantBin(oldFrame->binsLinear);
        QVERIFY(oldPeak >= 0);

        QSignalSpy frames(&source, &DaemonSpectrumSource::frameAvailable);
        // A partial pre-gap prefix has entered FFTEngine's ring but cannot
        // publish an FFT. Wait for its completed source handoff rather than
        // sleeping; the first emitted frame below must be the post-gap tone.
        const quint64 handoffs = source.completedInputHandoffs(key);
        source.submitIq(0, syntheticIq(511, 0.125));
        QTRY_VERIFY(source.completedInputHandoffs(key) > handoffs);
        source.submitIq(0, syntheticIq(4097, 0.0625)); // 8194 floats: rejected
        QTRY_VERIFY(source.droppedInputFrames(key) >= 1);
        source.submitIq(0, syntheticIq(1026, 0.25));
        QTRY_VERIFY(frames.count() >= 1);
        const auto postGap = source.takeLatest(key);
        QVERIFY(postGap.has_value());
        const int newPeak = dominantBin(postGap->binsLinear);
        QVERIFY(newPeak >= 0);
        QVERIFY2(postGap->binsLinear.at(oldPeak)
                     < postGap->binsLinear.at(newPeak) * 0.01f,
                 "post-gap FFT retained the dropped packet's preceding tone");
    }

    // The per-bin dBm conversion DaemonAgcSource ran on the main thread now
    // runs in DaemonSpectrumSource; every value, and every rejection, must
    // be the same bits.
    void binsDbmMatchesTheMainThreadConversionBitForBit()
    {
        QVector<float> edges{0.0f,
                             std::numeric_limits<float>::denorm_min(),
                             1.0e-21f,
                             std::nextafter(1.0e-20f, 0.0f),
                             1.0e-20f,
                             std::nextafter(1.0e-20f, 1.0f),
                             1.0e-12f,
                             0.5f,
                             1.0f,
                             3.0f,
                             1.0e10f,
                             std::numeric_limits<float>::max(),
                             -1.0f};
        std::mt19937 rng(20260930u);
        std::uniform_real_distribution<float> exponent(-24.0f, 12.0f);
        QVector<float> random;
        for (int i = 0; i < 4096; ++i) {
            random.append(std::pow(10.0f, exponent(rng)));
        }
        const double offsets[] = {0.0, -3.0103, 47.95, -174.2, 1.0e300};
        for (const QVector<float>* bins : {&edges, &random}) {
            for (const double offset : offsets) {
                QVector<float> expected;
                QVector<float> actual;
                const bool expectedOk = mainThreadAgcDbm(*bins, offset, expected);
                const bool actualOk =
                    DaemonSpectrumSource::binsLinearToDbm(*bins, offset, actual);
                QCOMPARE(actualOk, expectedOk);
                if (expectedOk) {
                    QVERIFY(bitIdentical(actual, expected));
                } else {
                    QVERIFY(actual.isEmpty());
                }
            }
        }
        // A non-finite power rejects the frame, as before.
        for (const float bad : {std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity()}) {
            QVector<float> bins = random;
            bins[17] = bad;
            QVector<float> out;
            QVERIFY(!DaemonSpectrumSource::binsLinearToDbm(bins, 0.0, out));
            QVERIFY(out.isEmpty());
        }
    }

    // Frames from the engine thread: the source that computes dBm carries
    // the same bits the main thread would have made, its linear bins equal
    // those of a source that does not, and a noise-floor tracker fed from
    // either path reads the same value.
    void engineThreadDbmLeavesSpectrumAndAgcValuesUnchanged()
    {
        DaemonSpectrumSource plain;
        DaemonSpectrumSource agc;
        agc.setComputesBinsDbm(true);
        QVERIFY(!plain.computesBinsDbm());
        const MediaSourceKey key{0, FftTier::Wide};
        QVERIFY(plain.activate(key, sourceConfig(1024, 7100000.0, 48000.0)));
        QVERIFY(agc.activate(key, sourceConfig(1024, 7100000.0, 48000.0)));
        QTRY_VERIFY(plain.isActive(key));
        QTRY_VERIFY(agc.isActive(key));

        NoiseFloorTracker reference;
        NoiseFloorTracker moved;
        for (int round = 0; round < 3; ++round) {
            QSignalSpy plainFrames(&plain, &DaemonSpectrumSource::frameAvailable);
            QSignalSpy agcFrames(&agc, &DaemonSpectrumSource::frameAvailable);
            const QVector<float> iq = syntheticIq(1026, 0.125 / (round + 1));
            plain.submitIq(0, iq);
            agc.submitIq(0, iq);
            QTRY_VERIFY(plainFrames.count() >= 1);
            QTRY_VERIFY(agcFrames.count() >= 1);
            const auto plainFrame = plain.takeLatest(key);
            const auto agcFrame = agc.takeLatest(key);
            QVERIFY(plainFrame.has_value());
            QVERIFY(agcFrame.has_value());

            // Spectrum: the linear bins and offset are untouched.
            QVERIFY(bitIdentical(agcFrame->binsLinear, plainFrame->binsLinear));
            QCOMPARE(agcFrame->dbmOffset, plainFrame->dbmOffset);
            QVERIFY(plainFrame->binsDbm.isEmpty());
            QVERIFY(!plainFrame->binsDbmValid);

            // AGC: the engine thread's dBm is the main thread's, bit for bit.
            QVector<float> expected;
            QVERIFY(mainThreadAgcDbm(agcFrame->binsLinear, agcFrame->dbmOffset,
                                     expected));
            QVERIFY(agcFrame->binsDbmValid);
            QVERIFY(bitIdentical(agcFrame->binsDbm, expected));

            reference.feed(expected, 33.0f);
            moved.feed(agcFrame->binsDbm, 33.0f);
            const float referenceFloor = reference.noiseFloor();
            const float movedFloor = moved.noiseFloor();
            QVERIFY(std::memcmp(&referenceFloor, &movedFloor, sizeof(float)) == 0);
            QCOMPARE(moved.isGood(), reference.isGood());
        }
    }
};

QTEST_MAIN(TstRemoteFftProduction)
#include "tst_remote_fft_production.moc"
