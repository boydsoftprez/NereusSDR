// no-port-check: NereusSDR-original. A desktop window acting as Core
// publishes each slice's Auto AGC noise floor for signed-in windows.
#include <QtTest>

#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/WdspEngine.h"
#include "core/RxChannel.h"
#include "core/NoiseFloorTracker.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
void warm(NoiseFloorTracker& tracker, float dbm)
{
    for (int i = 0; i < 70; ++i) { tracker.feed(QVector<float>(32, dbm), 33.0f); }
}
}

class TstDesktopCoreAutoAgcFloor final : public QObject {
    Q_OBJECT

private slots:
    void controllerTransmitHoldsEveryAutoThresholdAndFallback()
    {
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(5, 5, 192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        SliceModel* first = radio.sliceById(radio.addSlice(QStringLiteral("auto-first")));
        SliceModel* second = radio.sliceById(radio.addSlice(QStringLiteral("auto-second")));
        QVERIFY(first && second);
        NoiseFloorTracker own, fallback;
        radio.setStreamNoiseFloorTracker(first->streamIndex(), &own);
        radio.setNoiseFloorTracker(&fallback);
        warm(own, -85.0f);
        warm(fallback, -95.0f);
        first->setAutoAgcEnabled(true);
        second->setAutoAgcEnabled(true);
        radio.runAutoAgcTickForTest();
        const int firstThreshold = first->agcThreshold();
        const int secondThreshold = second->agcThreshold();
        const quint64 before = radio.autoAgcAcquisitionEpoch();
        radio.setLocalDisplayDuplex(true);
        auto* controller = radio.moxController();
        controller->setTimerIntervals(0, 0, 0, 0, 0, 0);
        controller->setMox(true);
        QVERIFY(radio.autoAgcAcquisitionInhibited()); // commit, before timers
        QVERIFY(!radio.transmitModel().isMox());
        QTRY_VERIFY(radio.isTransmitting());
        const quint64 keyed = radio.autoAgcAcquisitionEpoch();
        for (int i = 0; i < 100; ++i) {
            own.feedForEpoch(QVector<float>(4096, -115.0f), 33.0f, keyed);
            fallback.feedForEpoch(QVector<float>(4096, -125.0f), 33.0f, keyed);
            radio.runAutoAgcTickForTest();
        }
        QCOMPARE(own.noiseFloor(), -85.0f);
        QCOMPARE(fallback.noiseFloor(), -95.0f);
        QCOMPARE(first->agcThreshold(), firstThreshold);
        QCOMPARE(second->agcThreshold(), secondThreshold);
        controller->setMox(false);
        QVERIFY(radio.autoAgcAcquisitionInhibited()); // handover still live
        QTRY_VERIFY(!radio.autoAgcAcquisitionInhibited());
        own.feedForEpoch(QVector<float>(4096, -115.0f), 33.0f, before);
        radio.runAutoAgcTickForTest();
        QCOMPARE(first->agcThreshold(), firstThreshold);
        QCOMPARE(second->agcThreshold(), secondThreshold);
        QVERIFY(!first->stationAutoAgcNoiseFloorValid());
        own.feedForEpoch(QVector<float>(4096, -85.0f), 33.0f,
                         radio.autoAgcAcquisitionEpoch());
        fallback.feedForEpoch(QVector<float>(4096, -95.0f), 33.0f,
                              radio.autoAgcAcquisitionEpoch());
        radio.runAutoAgcTickForTest();
        QCOMPARE(first->agcThreshold(), firstThreshold);
        QCOMPARE(second->agcThreshold(), secondThreshold);
        QVERIFY(first->stationAutoAgcNoiseFloorValid());
        QVERIFY(second->stationAutoAgcNoiseFloorValid());
        controller->setTune(true);
        QVERIFY(radio.autoAgcAcquisitionInhibited());
        QVERIFY(!radio.transmitModel().isMox());
        QTRY_VERIFY(controller->isMox());
        controller->setTune(false);
        QTRY_VERIFY(!radio.autoAgcAcquisitionInhibited());
        radio.clearStreamNoiseFloorTracker(first->streamIndex(), &own);
        radio.setNoiseFloorTracker(nullptr);
    }

    void remoteRoleCannotAcquireOrWriteStationAuto()
    {
        RadioModel radio(RadioModel::Role::Remote);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(radio.autoAgcAcquisitionInhibited());
        NoiseFloorTracker tracker;
        radio.setNoiseFloorTracker(&tracker);
        tracker.feedForEpoch(QVector<float>(4096, -85.0f), 33.0f,
                             radio.autoAgcAcquisitionEpoch());
        QVERIFY(!tracker.acquisitionCurrent());
        radio.runAutoAgcTickForTest();
        QCOMPARE(tracker.noiseFloor(), -200.0f);
        radio.setNoiseFloorTracker(nullptr);
    }

    void controllerHoldPreservesAppliedWdspThreshold()
    {
#ifndef HAVE_WDSP
        QSKIP("requires WDSP");
#else
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(5, 5, 192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        QTemporaryDir config;
        QVERIFY(config.isValid());
        WdspEngine* engine = radio.wdspEngine();
        engine->setSynchronousInitForTest(true);
        QVERIFY(engine->initialize(config.path()));
        RxChannel* rx = engine->createRxChannel(slice->sliceIndex());
        QVERIFY(rx);
        NoiseFloorTracker tracker;
        radio.setStreamNoiseFloorTracker(slice->streamIndex(), &tracker);
        warm(tracker, -85.0f);
        slice->setAutoAgcEnabled(true);
        radio.runAutoAgcTickForTest();
        double applied = 0.0;
        bool read = false;
        const auto readback = [&]() {
            read = false;
            rx->requestAgcReadBack(&radio, [&](double, double threshold) {
                applied = threshold; read = true;
            });
        };
        readback();
        QTRY_VERIFY(read);
        QVERIFY(std::abs(applied - slice->agcThreshold()) < 0.01);
        const double held = applied;
        auto* controller = radio.moxController();
        controller->setTimerIntervals(0, 0, 0, 0, 0, 0);
        controller->setMox(true);
        QTRY_VERIFY(radio.isTransmitting());
        QVERIFY(!radio.transmitModel().isMox());
        tracker.feedForEpoch(QVector<float>(4096, -115.0f), 3000.0f,
                             radio.autoAgcAcquisitionEpoch());
        radio.runAutoAgcTickForTest();
        readback();
        QTRY_VERIFY(read);
        QCOMPARE(applied, held);
        controller->setMox(false);
        QTRY_VERIFY(!radio.autoAgcAcquisitionInhibited());
        radio.runAutoAgcTickForTest();
        readback();
        QTRY_VERIFY(read);
        QCOMPARE(applied, held);
        radio.clearStreamNoiseFloorTracker(slice->streamIndex(), &tracker);
#endif
    }

    // The fields below are what a signed-in window mirrors and shows as
    // "NF -98 dB" or "NF awaiting measurement" (MirrorPolicy, outbound).
    void desktopCorePublishesEachSlicesOwnFloor()
    {
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        SliceModel* first = radio.sliceById(radio.addSlice(QStringLiteral("pan-a")));
        SliceModel* second = radio.sliceById(radio.addSlice(QStringLiteral("pan-b")));
        QVERIFY(first && second);
        QVERIFY(first->streamIndex() >= 0 && second->streamIndex() >= 0);
        QVERIFY(first->streamIndex() != second->streamIndex());

        // As the desktop window registers its per-stream trackers.
        NoiseFloorTracker firstFloor, secondFloor;
        radio.setStreamNoiseFloorTracker(first->streamIndex(), &firstFloor);
        radio.setStreamNoiseFloorTracker(second->streamIndex(), &secondFloor);

        // No reading yet: not valid.
        radio.runAutoAgcTickForTest();
        QVERIFY(!first->stationAutoAgcNoiseFloorValid());
        QVERIFY(!second->stationAutoAgcNoiseFloorValid());

        // A good local reading: the same number, valid, AUTO on or off.
        warm(firstFloor, -98.0f);
        warm(secondFloor, -71.0f);
        QVERIFY(firstFloor.isGood() && secondFloor.isGood());
        radio.runAutoAgcTickForTest();
        QVERIFY(first->stationAutoAgcNoiseFloorValid());
        QVERIFY(second->stationAutoAgcNoiseFloorValid());
        QCOMPARE(first->stationAutoAgcNoiseFloorDbm(),
                 static_cast<double>(firstFloor.noiseFloor()));
        QCOMPARE(second->stationAutoAgcNoiseFloorDbm(),
                 static_cast<double>(secondFloor.noiseFloor()));
        const quint64 generation = first->stationAutoAgcNoiseFloorGeneration();

        // The local reading is lost (a retune restarts the tracker).
        firstFloor.triggerFastAttack();
        radio.runAutoAgcTickForTest();
        QVERIFY(!first->stationAutoAgcNoiseFloorValid());
        QVERIFY(first->stationAutoAgcNoiseFloorGeneration() > generation);
        QVERIFY(second->stationAutoAgcNoiseFloorValid());

        // Measured again.
        warm(firstFloor, -90.0f);
        radio.runAutoAgcTickForTest();
        QVERIFY(first->stationAutoAgcNoiseFloorValid());
        QCOMPARE(first->stationAutoAgcNoiseFloorDbm(),
                 static_cast<double>(firstFloor.noiseFloor()));

        // Transmit: not valid at once, as on the headless Core, and it
        // stays so for the whole of the over.
        radio.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        radio.moxController()->setMox(true);
        QTRY_VERIFY(radio.moxController()->isMox());
        QVERIFY(!radio.transmitModel().isMox());
        QVERIFY(!first->stationAutoAgcNoiseFloorValid());
        QVERIFY(!second->stationAutoAgcNoiseFloorValid());
        QCOMPARE(first->stationAutoAgcNoiseFloorDbm(), -200.0);
        radio.runAutoAgcTickForTest();
        QVERIFY(!first->stationAutoAgcNoiseFloorValid());
        radio.moxController()->setMox(false);
        QTRY_VERIFY(!radio.isTransmitting());
        radio.runAutoAgcTickForTest();
        QVERIFY(!first->stationAutoAgcNoiseFloorValid());
        QVERIFY(!second->stationAutoAgcNoiseFloorValid());
        warm(firstFloor, -90.0f);
        warm(secondFloor, -71.0f);
        radio.runAutoAgcTickForTest();
        QVERIFY(first->stationAutoAgcNoiseFloorValid());
        QVERIFY(second->stationAutoAgcNoiseFloorValid());

        // The radio drops.
        radio.setConnectionStateForTest(ConnectionState::Disconnected);
        radio.runAutoAgcTickForTest();
        QVERIFY(!first->stationAutoAgcNoiseFloorValid());
        QVERIFY(!second->stationAutoAgcNoiseFloorValid());

        radio.clearStreamNoiseFloorTracker(first->streamIndex(), &firstFloor);
        radio.clearStreamNoiseFloorTracker(second->streamIndex(), &secondFloor);
    }

    // A source of its own (the headless Core) keeps the fields to itself.
    void aSourceOfItsOwnKeepsTheFields()
    {
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(5, 5, 192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);
        SliceModel* slice = radio.sliceById(radio.addSlice(QStringLiteral("pan-a")));
        QVERIFY(slice);
        NoiseFloorTracker floor;
        warm(floor, -98.0f);
        radio.setStreamNoiseFloorTracker(slice->streamIndex(), &floor);
        radio.setAutoAgcFloorPublishedBySource(true);
        radio.runAutoAgcTickForTest();
        QVERIFY(!slice->stationAutoAgcNoiseFloorValid());
        radio.clearStreamNoiseFloorTracker(slice->streamIndex(), &floor);
    }
};

QTEST_MAIN(TstDesktopCoreAutoAgcFloor)
#include "tst_desktop_core_auto_agc_floor.moc"
