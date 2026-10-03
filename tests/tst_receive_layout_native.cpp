// no-port-check: NereusSDR-original receive restoration integration tests.
#include <QtTest>
#include <QFile>
#include <QPointer>
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/RadeChannel.h"
#include "core/ReceiveLayoutStore.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/daemon/DaemonConfig.h"
#include "fakes/FakeAudioBus.h"
#include "fakes/P1FakeRadio.h"
#define private public
#include "core/daemon/DaemonApp.h"
#include "models/RadioModel.h"
#include "models/RxDspWorker.h"
#undef private

using namespace NereusSDR;

class TstReceiveLayoutNative : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("receive-layout-native-%1")
                                       .arg(QCoreApplication::applicationPid()));
    }
    void init()
    {
        AppSettings::instance().clear();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery::clearHoldOffForTest();
    }
    void cleanupTestCase() { QFile::remove(AppSettings::instance().filePath()); }

    void nativeStartupAndRecovery_data()
    {
        QTest::addColumn<bool>("radeOwnerB");
        QTest::newRow("sparse-ids-without-A") << false;
        QTest::newRow("A-USB-B-RADE-owner") << true;
    }

    void nativeStartupAndRecovery()
    {
        QFETCH(bool, radeOwnerB);
        Test::P1FakeRadio fake;
        fake.start();
        RadioInfo info;
        info.address = fake.localAddress();
        info.port = fake.localPort();
        info.boardType = HPSDRHW::HermesLite;
        info.protocol = ProtocolVersion::Protocol1;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:52");
        info.firmwareVersion = 72;
        info.name = QStringLiteral("Receive layout loopback");
        const QList<ReceiveSliceState> layout = radeOwnerB
            ? QList<ReceiveSliceState>{{0, "pan-0", 14293000, DSPMode::USB},
                                       {2, "pan-1", 7200000, DSPMode::RADE_L}}
            : QList<ReceiveSliceState>{{2, "pan-1", 7200000, DSPMode::LSB},
                                       {4, "pan-3", 7215000, DSPMode::USB}};
        QVERIFY(ReceiveLayoutStore::stage(AppSettings::instance(), info.macAddress,
            layout, nullptr, radeOwnerB ? std::optional<int>(2) : std::nullopt));
        QVERIFY(AppSettings::instance().save());

        DaemonApp app;
        app.m_synchronousWdspForTest = true;
        app.m_discoveryProviderForTest = [info] { return QList<RadioInfo>{info}; };
        app.m_radioInitializerForTest = [](RadioModel* model) {
            model->audioEngine()->setStartInitializerForTest([](AudioEngine& audio) {
                AudioFormat format;
                format.sampleRate = 48000;
                format.channels = 2;
                format.sample = AudioFormat::Sample::Float32;
                auto speakers = std::make_unique<FakeAudioBus>();
                auto mic = std::make_unique<FakeAudioBus>();
                speakers->open(format);
                mic->open(format);
                audio.setSpeakersBusForTest(std::move(speakers));
                audio.setTxInputBusForTest(std::move(mic));
            });
        };
        DaemonConfig config = DaemonConfig::defaults();
        config.radioMac = info.macAddress;
        config.remotePort = 0;
        config.sampleRateHz = 48000;
        config.sliceCount = 1;
        QVERIFY(app.start(config));
        RadioModel* model = app.m_radioModel.get();
        QCOMPARE(model->slices().size(), 2);
        QVERIFY(!model->wdspEngine()->radeChannel(2)); // passive until discovery
        QTRY_VERIFY_WITH_TIMEOUT(model->isConnected(), 10000);
        QCOMPARE(model->receiveLayoutRestoreState(), QStringLiteral("accepted"));
        QCOMPARE(model->slices().size(), 2);
        for (const auto& entry : layout) {
            SliceModel* slice = model->sliceById(entry.id);
            QVERIFY(slice);
            QCOMPARE(slice->frequency(), entry.frequencyHz);
            QCOMPARE(slice->dspMode(), entry.dspMode);
            QVERIFY(slice->streamIndex() >= 0);
            RxChannel* channel = model->wdspEngine()->rxChannel(entry.id);
            QVERIFY(channel && channel->isActive());
        }
        // These are independent pans even when their frequency windows
        // overlap. Recovery must retain separate DDCs as startup does.
        QVERIFY(model->sliceById(layout.first().id)->streamIndex()
                != model->sliceById(layout.last().id)->streamIndex());
        if (radeOwnerB) {
            QCOMPARE(model->txBoundSlice()->sliceIndex(), 0);
            QCOMPARE(model->m_restoredRadeReceiveOwner.value_or(-1), 2);
            QVERIFY(model->m_radeRxRoutes.contains(2));
            QVERIFY(model->wdspEngine()->radeChannel(2)->isActive());
            QVERIFY(!model->wdspEngine()->radeChannel(0));
        } else {
            QVERIFY(!model->sliceById(0));
            QVERIFY(!model->wdspEngine()->rxChannel(0)->isActive());
        }

        // In-process recovery must use these edited live objects, not load
        // the older saved tuning or let cfg.sliceCount change membership.
        SliceModel* const retained = model->sliceById(2);
        retained->setFrequency(7210000);
        // A QPointer, not a raw address: the new worker can reuse the old one's
        // address, so only the old object's destruction proves a new worker.
        const QPointer<RxDspWorker> oldWorker(model->m_dspWorker);
        model->disconnectFromRadio();
        model->connectToRadioPreservingSlices(info);
        QTRY_VERIFY_WITH_TIMEOUT(model->isConnected(), 10000);
        QCOMPARE(model->sliceById(2), retained);
        QCOMPARE(retained->frequency(), 7210000.0);
        QCOMPARE(model->slices().size(), 2);
        QCOMPARE(model->receiveLayoutRestoreState(), QStringLiteral("accepted"));
        QVERIFY(model->sliceById(layout.first().id)->streamIndex()
                != model->sliceById(layout.last().id)->streamIndex());
        if (radeOwnerB) {
            QCOMPARE(model->m_restoredRadeReceiveOwner.value_or(-1), 2);
            QVERIFY(model->m_radeRxRoutes.contains(2));
            // RADE threads: recovery replays the route onto the new worker.
            QTRY_VERIFY_WITH_TIMEOUT(oldWorker.isNull(), 5000);
            QVERIFY(model->m_dspWorker != nullptr);
            QTRY_COMPARE_WITH_TIMEOUT(model->m_dspWorker->radeRxRouteCount(), 1, 5000);
            QVERIFY(model->wdspEngine()->radeChannel(2)->isActive());
            // Keep two RADE mode descriptors, explicitly choose B last,
            // then remove B. A's metadata is not permission to make it the
            // decoded receive owner on the next process start.
            model->sliceById(0)->setDspMode(DSPMode::RADE_U);
            retained->setDspMode(DSPMode::USB);
            retained->setDspMode(DSPMode::RADE_L);
            QCOMPARE(model->m_restoredRadeReceiveOwner.value_or(-1), 2);
            // RADE threads: A decodes too, on its own route.
            QVERIFY(model->m_radeRxRoutes.contains(0));
            QVERIFY(model->m_radeRxRoutes.contains(2));
            model->flushPendingSettingsSave();
            const QString previous = AppSettings::instance().hardwareValue(
                info.macAddress, "receiveLayout").toString();
            model->removeSlice(2);
            model->flushPendingSettingsSave();
            QCOMPARE(model->settingsSaveError(),
                     QStringLiteral("Your receivers were not saved because it is not "
                                    "clear which receiver should play RADE audio. "
                                    "Select RADE mode again on the receiver you want "
                                    "to hear."));
            QCOMPARE(AppSettings::instance().hardwareValue(info.macAddress,
                       "receiveLayout").toString(), previous);
            // An explicit ordinary receive mode resolves the invalid live
            // capture, allowing the smaller topology to persist normally.
            model->sliceById(0)->setDspMode(DSPMode::USB);
            model->flushPendingSettingsSave();
            QVERIFY(model->settingsSaveError().isEmpty());
        } else {
            QVERIFY(!model->wdspEngine()->rxChannel(0)->isActive());
        }
        app.stop();
    }
};

QTEST_MAIN(TstReceiveLayoutNative)
#include "tst_receive_layout_native.moc"
