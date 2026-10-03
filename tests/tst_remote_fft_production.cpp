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
        QVERIFY(source.update(key, sourceConfig(1024, 10100000.0, 48000.0)));
        QTRY_VERIFY(source.isActive(key));
        source.submitIq(0, syntheticIq(1026, 0.25));
        QTRY_VERIFY(frames.count() >= 2);
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
        QVERIFY(!source.takeLatest(key).has_value());
        QTest::qWait(20);
        QCOMPARE(frames.count(), 1);
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
