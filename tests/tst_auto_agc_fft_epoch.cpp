// no-port-check: NereusSDR-original AUTO acquisition metadata regression.
// 2026-11-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include "core/FFTEngine.h"
#include "core/NoiseFloorTracker.h"
#include "models/RadioModel.h"
#include "core/MoxController.h"

using namespace NereusSDR;

class TstAutoAgcFftEpoch final : public QObject {
    Q_OBJECT
private slots:
    void epochsGateEveryWindowWithoutChangingDisplay()
    {
#ifndef HAVE_FFTW3
        QSKIP("requires FFTW3");
#endif
        NereusSDR::FFTEngine tagged(0), plain(0);
        qint64 taggedMs = 0, plainMs = 0;
        for (NereusSDR::FFTEngine* engine : {&tagged, &plain}) {
            engine->setFftSize(1024);
            engine->setSampleRate(30720.0); // half-window advance at 60 FPS
            engine->setOutputFps(60);
            engine->setInputGeneration(17);
        }
        tagged.setFrameGapClockForTest([&]() { return taggedMs += 10; });
        plain.setFrameGapClockForTest([&]() { return plainMs += 10; });
        QVector<QVector<float>> taggedDisplay, plainDisplay, admitted;
        QVector<quint64> epochs;
        connect(&tagged, &NereusSDR::FFTEngine::fftReady, &tagged,
                [&](int, const QVector<float>& bins) { taggedDisplay.append(bins); });
        connect(&plain, &NereusSDR::FFTEngine::fftReady, &plain,
                [&](int, const QVector<float>& bins) { plainDisplay.append(bins); });
        connect(&tagged, &NereusSDR::FFTEngine::autoAgcFrameReady, &tagged,
                [&](int, const QVector<float>& bins, quint64 epoch) {
                    admitted.append(bins); epochs.append(epoch);
                });
        auto feed = [&](int pairs, quint64 epoch, float value) {
            QVector<float> iq(pairs * 2, value);
            tagged.feedIQForAutoAgc(iq, epoch);
            plain.feedIQ(iq);
        };
        feed(800, 2, 0.1f); // partial old RX window
        feed(3200, 3, 0.2f); // one TX batch contains several transforms
        QVERIFY(admitted.isEmpty());
        const int before = taggedDisplay.size();
        feed(3200, 4, 0.3f); // first frames still include TX overlap
        QVERIFY(taggedDisplay.size() > before + 2);
        QVERIFY(!admitted.isEmpty());
        // 800 + 3200 leaves 928 samples in the overlapping buffer. The
        // first two transforms after release have only 96 and 608 new RX
        // samples respectively; neither is a complete RX-only window.
        QCOMPARE(admitted.first(), taggedDisplay.at(before + 2));
        for (quint64 epoch : epochs) { QCOMPARE(epoch, quint64(4)); }
        QCOMPARE(taggedDisplay, plainDisplay);
        QCOMPARE(tagged.inputGeneration(), quint64(17));
        QCOMPARE(tagged.windowResets(), plain.windowResets());
        QCOMPARE(tagged.minimumGapSkips(), plain.minimumGapSkips());

        // Replanning must not reuse clean-sample metadata from old size.
        admitted.clear();
        tagged.setFftSize(2048);
        tagged.feedIQForAutoAgc(QVector<float>(1600, 0.4f), 4);
        QVERIFY(admitted.isEmpty());
        tagged.feedIQForAutoAgc(QVector<float>(5200, 0.4f), 4);
        QVERIFY(!admitted.isEmpty());
        QCOMPARE(admitted.first().size(), 2048);
    }

    void queuedOldFrameIsRejectedAfterRelease()
    {
        NoiseFloorTracker tracker;
        tracker.setAcquisitionEpoch(2);
        QObject receiver;
        NereusSDR::FFTEngine engine(0);
        connect(&engine, &NereusSDR::FFTEngine::autoAgcFrameReady, &receiver,
                [&](int, const QVector<float>& bins, quint64 epoch) {
                    tracker.feedForEpoch(bins, 33.0f, epoch);
                }, Qt::QueuedConnection);
        engine.autoAgcFrameReady(0, QVector<float>(256, -115.0f), 2);
        tracker.setAcquisitionEpoch(3);
        tracker.setAcquisitionEpoch(4);
        QCoreApplication::sendPostedEvents(&receiver);
        QVERIFY(!tracker.acquisitionCurrent());
        QCOMPARE(tracker.noiseFloor(), -200.0f);
        engine.autoAgcFrameReady(0, QVector<float>(256, -85.0f), 4);
        QCoreApplication::sendPostedEvents(&receiver);
        QVERIFY(tracker.acquisitionCurrent());
        QCOMPARE(tracker.noiseFloor(), -85.0f);
    }

    void ingressHandleOutlivesModelAndIsInhibited()
    {
        auto model = std::make_unique<RadioModel>();
        const auto handle = model->autoAgcAcquisitionHandle();
        model->setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY((handle->load() & 1U) == 0);
        model.reset();
        QVERIFY((handle->load() & 1U) != 0);
        // Queued input may retain the handle after the model's destruction.
        NereusSDR::FFTEngine engine(0);
        QMetaObject::invokeMethod(&engine, [handle, &engine]() {
            engine.feedIQForAutoAgc({}, handle->load());
        }, Qt::QueuedConnection);
        QCoreApplication::sendPostedEvents(&engine);
    }

    void ingressStampsBeforeQueueAndDestroyedReceiverDropsPendingWork()
    {
        RadioModel model;
        model.setConnectionStateForTest(ConnectionState::Connected);
        QVector<quint64> delivered;
        auto receiver = std::make_unique<QObject>();
        connect(&model, &RadioModel::rawIqDataForAutoAgc, receiver.get(),
                [&](int, const QVector<float>&, quint64 epoch) { delivered.append(epoch); },
                Qt::QueuedConnection);
        const quint64 receiving = model.autoAgcAcquisitionEpoch();
        model.forkIqToTapsForTest(0, {0.1f, 0.2f});
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.moxController()->setMox(true);
        const quint64 keyed = model.autoAgcAcquisitionEpoch();
        QVERIFY(keyed != receiving);
        model.forkIqToTapsForTest(0, {0.3f, 0.4f});
        QCoreApplication::sendPostedEvents(receiver.get());
        QCOMPARE(delivered, (QVector<quint64>{receiving, keyed}));
        model.forkIqToTapsForTest(0, {0.5f, 0.6f});
        receiver.reset();
        QCoreApplication::sendPostedEvents();
        QCOMPARE(delivered.size(), 2);
        model.moxController()->setMox(false);
        QTRY_VERIFY(!model.autoAgcAcquisitionInhibited());
    }
};
QTEST_MAIN(TstAutoAgcFftEpoch)
#include "tst_auto_agc_fft_epoch.moc"
