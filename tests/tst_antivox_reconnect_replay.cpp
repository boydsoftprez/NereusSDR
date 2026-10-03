// no-port-check: NereusSDR-original integration regressions, no upstream
// code translated. References: mi0bot console.cs:47660-47673 (HL2 TUNE
// magnitude), Thetis setup.cs:18980-18996 (anti-VOX run and tau).
//
// Actual RadioModel fresh startup and reconnect paths against the loopback
// P1 fake. WDSP state is read through a C probe rather than a cached getter.
// No real audio device or radio is opened by ConnectableRadioModel.
#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/TxChannel.h"
#include "core/TxWorkerThread.h"
#include "core/WdspEngine.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "fakes/ConnectableRadioModel.h"

#include <array>

#ifdef HAVE_WDSP
extern "C" {
double nereus_issue299_antivox_tau(int channel);
int nereus_issue299_antivox_running(int channel);
}
#endif

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;

class TestAntiVoxReconnectReplay : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void reconnectReplaysAntiVox_data()
    {
        QTest::addColumn<bool>("checkRun");
        QTest::addColumn<bool>("deferredStartup");
        QTest::newRow("fresh-worker-gate-and-detector-run") << true << false;
        QTest::newRow("fresh-detector-tau") << false << false;
        QTest::newRow("deferred-startup-worker-gate-and-detector-run") << true << true;
        QTest::newRow("deferred-startup-detector-tau") << false << true;
    }

    void reconnectReplaysAntiVox()
    {
#ifndef HAVE_WDSP
        QSKIP("Requires the real WDSP anti-VOX detector");
#else
        QFETCH(bool, checkRun);
        QFETCH(bool, deferredStartup);
        const QString settingsPrefix = QStringLiteral("hardware/aa:bb:cc:11:22:33/tx/");
        AppSettings::instance().setValue(settingsPrefix + QStringLiteral("AntiVox_Enable"),
                                         QStringLiteral("True"));
        AppSettings::instance().setValue(settingsPrefix + QStringLiteral("AntiVox_Tau_Ms"),
                                         QStringLiteral("80"));
        auto harness = ConnectableRadioModel::create(
            10000, RadioModel::Role::Local, [deferredStartup](RadioModel& model) {
                if (deferredStartup) {
                    model.wdspEngine()->setSynchronousInitForTest(false);
                    model.wdspEngine()->setDeferredInitForTest(true);
                }
            });
        QVERIFY(harness);
        RadioModel& model = harness->model();
        MoxController* const controller = model.moxController();
        QVERIFY(!model.wdspEngine()->wisdomThreadSpawnedForTest());
        QTRY_VERIFY_WITH_TIMEOUT(model.txChannel() && model.txWorkerMutableForTest(), 10000);
        // The saved values must reach a fresh worker before any control is moved.
        QVERIFY(model.transmitModel().antiVoxRun());
        QCOMPARE(model.transmitModel().antiVoxTauMs(), 80);
        QCoreApplication::processEvents();
        QVERIFY(model.waitForTransmitLaneForTest(5000));
        int channelId = model.txChannel()->channelId();
        if (checkRun) {
            QCOMPARE(nereus_issue299_antivox_running(channelId), 1);
            TxWorkerThread* const firstWorker = model.txWorkerMutableForTest();
            QVERIFY(firstWorker);
            QSignalSpy copied(firstWorker, &TxWorkerThread::antiVoxReferenceCopied);
            const std::array<float, 128> reference{};
            firstWorker->onAntiVoxBlockReady(reference.data(), 64);
            QCOMPARE(copied.count(), 1);
        } else {
            QCOMPARE(nereus_issue299_antivox_tau(channelId), 0.080);
        }

        model.disconnectFromRadio();
        RadioDiscovery::clearHoldOffForTest();
        model.connectToRadio(harness->radioInfo());
        QTRY_COMPARE_WITH_TIMEOUT(model.connectionState(), ConnectionState::Connected, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(model.txChannel() && model.txWorkerMutableForTest(), 10000);
        QCoreApplication::processEvents();
        QVERIFY(model.waitForTransmitLaneForTest(10000));
        QCOMPARE(model.moxController(), controller);
        QVERIFY(model.transmitModel().antiVoxRun());
        QCOMPARE(model.transmitModel().antiVoxTauMs(), 80);
        channelId = model.txChannel()->channelId();
        if (checkRun) {
            // This goes through the real worker's atomic gate. A model or
            // channel getter alone cannot prove the reference feed is live.
            TxWorkerThread* const worker = model.txWorkerMutableForTest();
            QSignalSpy copied(worker, &TxWorkerThread::antiVoxReferenceCopied);
            const std::array<float, 128> reference{};
            worker->onAntiVoxBlockReady(reference.data(), 64);
            QCOMPARE(copied.count(), 1);
            QCOMPARE(nereus_issue299_antivox_running(channelId), 1);
        } else {
            QCOMPARE(nereus_issue299_antivox_tau(channelId), 0.080);
        }
#endif
    }
};

QTEST_MAIN(TestAntiVoxReconnectReplay)
#include "tst_antivox_reconnect_replay.moc"
