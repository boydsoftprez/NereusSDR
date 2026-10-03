// no-port-check: NereusSDR-original daemon lifecycle regression tests.
#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QThread>
#include <QSemaphore>
#include <QScopeGuard>
#include <QTimer>
#include <QUdpSocket>
#include <QSignalSpy>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/MoxController.h"
#include "core/RadeChannel.h"
#include "core/RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/ReceiveLayoutStore.h"
#include "core/RxChannel.h"
#include "core/WidebandFrameAccumulator.h"
#include "core/WidebandFftEngine.h"
#include "core/WdspEngine.h"
#include "core/session/ObjectRegistry.h"
#include "core/session/StateMirror.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#define private public
#include "core/daemon/DaemonApp.h"
#undef private
#include "core/station/StationRadios.h"
#include "fakes/FakeAudioBus.h"
#include "fakes/P1FakeRadio.h"
#include "fakes/P2FakeRadio.h"

using namespace NereusSDR;

namespace {
// iPhone app Task 12 put the Core's listener on by default (TCP 47910 on
// every interface). A test Core opens no listener unless the test asks for
// one on a port of its own.
NereusSDR::DaemonConfig testCoreConfig()
{
    NereusSDR::DaemonConfig config = NereusSDR::DaemonConfig::defaults();
    config.remotePort = 0;
    // iPhone app plan Task 34: remote_transmit deny, so these recoveries
    // still prove the Core's receive-only policy survives them (allow, the
    // default, installs no policy to keep).
    config.remoteTransmitAllowed = false;
    return config;
}
} // namespace
using NereusSDR::Test::P1FakeRadio;

namespace {
void installOpenAudioBuses(AudioEngine& engine)
{
    AudioFormat format;
    format.sampleRate = 48000;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;

    auto speakers = std::make_unique<FakeAudioBus>(QStringLiteral("Fake speakers"));
    auto txInput = std::make_unique<FakeAudioBus>(QStringLiteral("Fake TX input"));
    const bool speakersOpened = speakers->open(format);
    const bool txInputOpened = txInput->open(format);
    Q_ASSERT(speakersOpened);
    Q_ASSERT(txInputOpened);
    Q_UNUSED(speakersOpened);
    Q_UNUSED(txInputOpened);

    engine.setSpeakersBusForTest(std::move(speakers));
    engine.setTxInputBusForTest(std::move(txInput));
}

void configureAudioForEachStart(RadioModel* model)
{
    Q_ASSERT(model);
    Q_ASSERT(model->audioEngine());
    model->audioEngine()->setStartInitializerForTest(installOpenAudioBuses);
}

RadioInfo infoFor(const P1FakeRadio& fake)
{
    RadioInfo info;
    info.address = fake.localAddress();
    info.port = fake.localPort();
    info.boardType = HPSDRHW::HermesLite;
    info.protocol = ProtocolVersion::Protocol1;
    info.macAddress = QStringLiteral("aa:bb:cc:11:22:33");
    info.firmwareVersion = 72;
    return info;
}

void prepare(DaemonApp& app)
{
    app.m_synchronousWdspForTest = true;
    app.m_radioInitializerForTest = configureAudioForEachStart;
    app.m_radioRetryInitialMs = 10;
    app.m_radioRetryMaximumMs = 40;
    RadioDiscovery::clearHoldOffForTest();
}

// Discovery that answers `radios`, except while `hold` is set: then the
// scan waits (`held` says one is waiting) until it is let go or the Core
// cancels it (requestInterruption), as a real scan's cancellation does.
std::function<QList<RadioInfo>()> holdableScans(const QList<RadioInfo>& radios,
                                                std::atomic<bool>& hold,
                                                std::atomic<bool>& held)
{
    return [radios, &hold, &held]() {
        while (hold.load() && !QThread::currentThread()->isInterruptionRequested()) {
            held = true;
            QThread::msleep(1);
        }
        return radios;
    };
}

// Exercise the real P2 assembler-to-FFT boundary. UDP parsing has its own
// focused coverage; this fixture controls delivery around the two queues.
void feedWidebandBurst(P2RadioConnection* connection)
{
    connection->setWidebandEnabled(0, true);
    const auto accumulators = connection->findChildren<WidebandFrameAccumulator*>();
    Q_ASSERT(accumulators.size() == 8);
    const QByteArray payload(1024, char(0x20));
    for (int sequence = 0; sequence < 32; ++sequence) {
        accumulators.first()->pushPacket(sequence, payload);
    }
}
}

class TestDaemonRadioRecovery : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // The process-wide singleton otherwise shares the Qt test sandbox
        // with concurrent executables. Give this lifecycle fixture its own
        // file before the first AppSettings::instance() access.
        AppSettings::setProfileOverride(QStringLiteral("daemon-radio-recovery-%1")
                                        .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        // A prior slot may persist receive membership for the same fake MAC.
        // Empty both the singleton and its file so every scenario starts from
        // an explicit no-manifest state.
        AppSettings::instance().clear();
        QString error;
        QVERIFY2(AppSettings::instance().save(&error), qPrintable(error));
    }

    void cleanupTestCase()
    {
        QFile::remove(AppSettings::instance().filePath());
    }

    void realP2SilenceRebuildsOnlySelectedRadio()
    {
        NereusSDR::Test::P2FakeRadio fake;
        QVERIFY(fake.start());
        const RadioInfo info = fake.radioInfo();
        std::atomic<bool> available {true};
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [&]() {
            return available.load() ? QList<RadioInfo>{info} : QList<RadioInfo>{};
        };
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress;
        cfg.sliceCount = 2;
        cfg.sampleRateHz = 48000;
        QVERIFY(app.start(cfg));
        RadioModel* const model = app.m_radioModel.get();
        QVERIFY(!model->connection()); // discovery is scheduled after start returns.
        model->configureP2TransportForTest(fake.outboundPortBase(),
                                          fake.inputRolePortBase(), 1000, 200);
        QTimer ingress;
        ingress.setInterval(5);
        connect(&ingress, &QTimer::timeout, &app, [&]() {
            if (fake.hasClient()) {
                fake.sendDdc(2);
                fake.sendDdc(3);
                fake.sendStatus();
            }
        });
        ingress.start();
        QTRY_VERIFY_WITH_TIMEOUT(model->isConnected(), 15000);
        QTRY_COMPARE(app.sliceCount(), 2);
        QPointer<RadioConnection> oldConnection(model->connection());
        SliceModel* const a = model->slices().first();
        SliceModel* const b = model->slices().last();
        a->setFrequency(3865100);
        b->setFrequency(14225000);
        b->setDspMode(DSPMode::USB);
        b->setPanKey(QStringLiteral("pan-1"));
        model->setActiveSlice(b->sliceIndex());
        QVERIFY(model->wdspEngine()->isInitialized());
        QVERIFY(fake.totalEgressDatagrams() > 0);
        // Queue old-radio wideband work behind a blocked dispatch thread.
        // Recovery must retire it, including work already off the UDP thread.
        QSignalSpy widebandFrames(model, &RadioModel::widebandSpectrumReady);
        QSemaphore entered;
        QSemaphore release;
        QSemaphore frameProcessed;
        std::atomic<bool> oldWorkDrained {false};
        auto releaseOnExit = qScopeGuard([&]() {
            release.release();
            // Failure paths must join before the queued callbacks' stack
            // captures are destroyed, including when an assertion returns.
            app.widebandThread()->quit();
            app.widebandThread()->wait();
        });
        auto* blocker = new QObject;
        blocker->moveToThread(app.widebandThread());
        connect(app.widebandThread(), &QThread::finished, blocker, &QObject::deleteLater);
        QMetaObject::invokeMethod(blocker, [&]() {
            entered.release();
            release.acquire();
        }, Qt::QueuedConnection);
        QVERIFY(entered.tryAcquire(1, 1000));
        auto* const oldP2 = qobject_cast<P2RadioConnection*>(model->connection());
        QVERIFY(oldP2);
        QVERIFY(QMetaObject::invokeMethod(oldP2, [oldP2]() {
            feedWidebandBurst(oldP2);
        }, Qt::BlockingQueuedConnection));
        available = false;
        fake.stopIngress();
        QTRY_VERIFY_WITH_TIMEOUT(!model->connection(), 5000);
        QTRY_VERIFY(oldConnection.isNull());
        QCOMPARE(model->connectionState(), ConnectionState::Disconnected);
        QVERIFY(!model->wdspEngine()->isInitialized());
        QVERIFY(a->streamIndex() < 0);
        QVERIFY(b->streamIndex() < 0);
        QCOMPARE(fake.lastHighPriorityFlags() & 3, 0);
        QCOMPARE(fake.moxAssertedCount(), 0);
        QCOMPARE(model->activeSlice(), b);
        QCOMPARE(model->slices().first(), a);
        QCOMPARE(model->slices().last(), b);
        release.release();
        QMetaObject::invokeMethod(blocker, [&]() { oldWorkDrained = true; }, Qt::QueuedConnection);
        QTRY_VERIFY(oldWorkDrained.load());
        QCoreApplication::processEvents();
        QCOMPARE(widebandFrames.count(), 0);

        fake.resumeIngress();
        available = true;
        RadioDiscovery::clearHoldOffForTest();
        app.m_radioRetryTimer->start(0);
        QTRY_VERIFY_WITH_TIMEOUT(model->isConnected(), 15000);
        QCOMPARE(model->currentRadioMac(), info.macAddress);
        QCOMPARE(model->activeSlice(), b);
        QCOMPARE(model->slices().first(), a);
        QCOMPARE(model->slices().last(), b);
        QCOMPARE(a->frequency(), 3865100.0);
        QCOMPARE(b->frequency(), 14225000.0);
        QCOMPARE(b->panKey(), QStringLiteral("pan-1"));
        QVERIFY(a->streamIndex() >= 0);
        QVERIFY(b->streamIndex() >= 0);
        QVERIFY(model->wdspEngine()->isInitialized());
        QVERIFY(model->receiveOnlyStationPolicy());
        QCOMPARE(fake.moxAssertedCount(), 0);
        auto* const freshP2 = qobject_cast<P2RadioConnection*>(model->connection());
        QVERIFY(freshP2);
        // A remote view can acknowledge the real capture identity and paint
        // empty wings before any ADC samples arrive. DDC and ADC cadence are
        // independent; waiting for the first survey would stall first zoom.
        QVERIFY(QMetaObject::invokeMethod(freshP2, [freshP2]() {
            freshP2->setWidebandEnabled(0, true);
        }, Qt::BlockingQueuedConnection));
        QTRY_VERIFY(model->widebandSourceDescriptor(0));
        const auto preparedSource = model->widebandSourceDescriptor(0);
        QVERIFY(preparedSource->sourceGeneration != 0);
        QVERIFY(!model->latestWidebandSpectrum(0));
        QVERIFY(QMetaObject::invokeMethod(freshP2, [freshP2]() {
            feedWidebandBurst(freshP2);
        }, Qt::BlockingQueuedConnection));
        QTRY_COMPARE(widebandFrames.count(), 1);

        const auto firstWideband = model->latestWidebandSpectrum(0);
        QVERIFY(firstWideband);
        QCOMPARE(firstWideband->source, *preparedSource);
        QCOMPARE(firstWideband->source.physicalAdcIndex, 0);
        QCOMPARE(firstWideband->source.adcRateHz, 122880000.0);
        QVERIFY(firstWideband->source.sourceGeneration != 0);
        QVERIFY(firstWideband->producedAtNs > 0);
        QCOMPARE(firstWideband->rawDbBins.size(), WidebandFftEngine::kOutputBins);
        QVERIFY(!model->latestWidebandSpectrum(1));
        QVERIFY(!model->widebandAdcRateHz(-1));
        QVERIFY(!model->widebandAdcRateHz(2));

        // A still-connected radio can also retire an ADC capture. A row
        // queued before disable/re-enable must not enter the next capture.
        widebandFrames.clear();
        QMetaObject::invokeMethod(blocker, [&]() {
            entered.release();
            release.acquire();
        }, Qt::QueuedConnection);
        QVERIFY(entered.tryAcquire(1, 1000));
        QVERIFY(QMetaObject::invokeMethod(freshP2, [freshP2]() {
            feedWidebandBurst(freshP2);
            freshP2->setWidebandEnabled(0, false);
            freshP2->setWidebandEnabled(0, true);
        }, Qt::BlockingQueuedConnection));
        // The owner-thread retirement notification is still queued. A read
        // must already refuse the old cached frame using the retained token.
        QVERIFY(!model->latestWidebandSpectrum(0));
        release.release();
        QMetaObject::invokeMethod(blocker, [&]() {
            frameProcessed.release();
        }, Qt::QueuedConnection);
        QVERIFY(frameProcessed.tryAcquire(1, 1000));
        QCoreApplication::processEvents();
        QCOMPARE(widebandFrames.count(), 0);

        // Retirement after FFT but before owner-thread publication must also
        // drop that row. The valid row afterward proves capture still works.
        QVERIFY(QMetaObject::invokeMethod(freshP2, [freshP2]() {
            feedWidebandBurst(freshP2);
        }, Qt::BlockingQueuedConnection));
        QMetaObject::invokeMethod(blocker, [&]() {
            frameProcessed.release();
        }, Qt::QueuedConnection);
        QVERIFY(frameProcessed.tryAcquire(1, 1000));
        QVERIFY(QMetaObject::invokeMethod(freshP2, [freshP2]() {
            freshP2->setWidebandEnabled(0, false);
            freshP2->setWidebandEnabled(0, true);
        }, Qt::BlockingQueuedConnection));
        QCoreApplication::processEvents();
        QCOMPARE(widebandFrames.count(), 0);
        QVERIFY(QMetaObject::invokeMethod(freshP2, [freshP2]() {
            feedWidebandBurst(freshP2);
        }, Qt::BlockingQueuedConnection));
        QTRY_COMPARE(widebandFrames.count(), 1);

        const auto renewed = model->latestWidebandSpectrum(0);
        QVERIFY(renewed);
        QVERIFY(renewed->source.sourceGeneration > firstWideband->source.sourceGeneration);

        // Geometry changes retire both cached rows and frames waiting before
        // FFT, even if the configured rate is changed back before dispatch.
        widebandFrames.clear();
        QMetaObject::invokeMethod(blocker, [&]() {
            entered.release();
            release.acquire();
        }, Qt::QueuedConnection);
        QVERIFY(entered.tryAcquire(1, 1000));
        QVERIFY(QMetaObject::invokeMethod(freshP2, [freshP2]() {
            feedWidebandBurst(freshP2);
        }, Qt::BlockingQueuedConnection));
        model->widebandFftEngine(0)->setAdcSampleRateHz(61440000.0);
        model->widebandFftEngine(0)->setAdcSampleRateHz(122880000.0);
        QVERIFY(!model->latestWidebandSpectrum(0));
        release.release();
        QMetaObject::invokeMethod(blocker, [&]() { frameProcessed.release(); }, Qt::QueuedConnection);
        QVERIFY(frameProcessed.tryAcquire(1, 1000));
        QCoreApplication::processEvents();
        QCOMPARE(widebandFrames.count(), 0);

        // Host production time predates FFT queueing; it cannot be refreshed
        // merely because a blocked worker eventually finishes the transform.
        QMetaObject::invokeMethod(blocker, [&]() {
            entered.release();
            release.acquire();
        }, Qt::QueuedConnection);
        QVERIFY(entered.tryAcquire(1, 1000));
        QVERIFY(QMetaObject::invokeMethod(freshP2, [freshP2]() {
            feedWidebandBurst(freshP2);
        }, Qt::BlockingQueuedConnection));
        const qint64 beforeRelease = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        release.release();
        QTRY_COMPARE(widebandFrames.count(), 1);
        const auto delayed = model->latestWidebandSpectrum(0);
        QVERIFY(delayed);
        QVERIFY(delayed->producedAtNs <= beforeRelease);
        QVERIFY(delayed->source.sourceGeneration > renewed->source.sourceGeneration);

        const int demandSlice = model->slices().first()->sliceIndex();
        const auto demandOwner = model->acquireWidebandDemand(demandSlice);
        QVERIFY(demandOwner != 0);
        QVERIFY(model->setWidebandDemandActive(demandOwner, true));

        // Complete another FFT while the owner thread is synchronously
        // waiting. Its publication is now queued here but not delivered.
        // Retiring the connection must also reject this later race window.
        widebandFrames.clear();
        QVERIFY(QMetaObject::invokeMethod(freshP2, [freshP2]() {
            feedWidebandBurst(freshP2);
        }, Qt::BlockingQueuedConnection));
        QVERIFY(QMetaObject::invokeMethod(blocker, [&]() {
            frameProcessed.release();
        }, Qt::QueuedConnection));
        QVERIFY(frameProcessed.tryAcquire(1, 1000));
        int retirementNotifications = 0;
        bool retiredSourceOffered = false;
        bool retiredDemandAdmitted = false;
        connect(model, &RadioModel::widebandSourceChanged, model, [&](int) {
            ++retirementNotifications;
            retiredSourceOffered |= model->widebandAdcRateHz(0).has_value()
                || model->widebandAdcRateHz(1).has_value();
            retiredDemandAdmitted |= model->acquireWidebandDemand(demandSlice) != 0
                || model->setWidebandDemandActive(demandOwner, true);
        }, Qt::DirectConnection);
        model->disconnectFromRadio();
        QVERIFY(retirementNotifications > 0);
        QVERIFY(!retiredSourceOffered);
        QVERIFY(!retiredDemandAdmitted);
        QVERIFY(!model->setWidebandDemandActive(demandOwner, true));
        QVERIFY(!model->latestWidebandSpectrum(0));
        QVERIFY(!model->widebandAdcRateHz(0));
        QCoreApplication::processEvents();
        QCOMPARE(widebandFrames.count(), 0);
        app.stop();
    }

    void lateSelectedRadioKeepsControlResponsive()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        RadioInfo wrong = info;
        wrong.macAddress = QStringLiteral("aa:bb:cc:99:99:99");
        std::atomic<int> scans {0};
        std::atomic<bool> available {false};
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [&]() {
            ++scans;
            return available.load() ? QList<RadioInfo>{wrong, info} : QList<RadioInfo>{wrong};
        };
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        cfg.sliceCount = 2;
        QVERIFY(app.start(cfg));
        SliceModel* const initial = app.m_radioModel->slices().first();
        QTRY_VERIFY_WITH_TIMEOUT(scans >= 2, 1000);
        QVERIFY(!app.m_radioModel->connection());
        QVERIFY(!fake.isRunning());
        QVERIFY(app.m_radioModel->receiveOnlyStationPolicy());
        available = true;
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);
        QTRY_COMPARE(app.sliceCount(), 2);
        QCOMPARE(app.m_radioModel->slices().first(), initial);
        QCOMPARE(app.m_radioModel->currentRadioMac(), info.macAddress);
        QVERIFY(fake.isRunning());
        QVERIFY(!app.m_radioModel->mox());
        app.stop();
        QCOMPARE(app.sliceCount(), 0);
        QVERIFY(!app.m_radioDiscoveryThread);
        QVERIFY(!app.m_radioRetryTimer->isActive());
    }

    // Fix wave, C1 and I6 (parity Task 21): a chosen radio that answers
    // discovery but never connects ends the change, so another window can
    // choose again, and it is never saved; the radio chosen next connects
    // and is saved.
    void aRadioChangeThatNeverConnectsLetsAWindowChooseAgain()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        // Answers discovery, never answers a connect (a loopback socket that
        // reads nothing back).
        QUdpSocket silent;
        QVERIFY(silent.bind(QHostAddress::LocalHost, 0));
        RadioInfo dead = info;
        dead.macAddress = QStringLiteral("AA:BB:CC:44:55:66");
        dead.address = QHostAddress::LocalHost;
        dead.port = silent.localPort();
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [&]() { return QList<RadioInfo>{info, dead}; };
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);

        QString reason;
        QVERIFY(app.m_stationRadios->select(dead.macAddress, &reason));
        QVERIFY(app.m_stationRadios->switching());
        QTRY_VERIFY_WITH_TIMEOUT(!app.m_stationRadios->switching(), 15000);
        QVERIFY(app.m_radioModel && !app.m_radioModel->isConnected());
        QVERIFY(app.m_stationRadios->savedChoice().isEmpty());
        QVERIFY(!app.m_radioModel->stationRadioChangeUnderway());

        // Another window chooses the first radio again: taken, connects,
        // and is saved.
        QVERIFY2(app.m_stationRadios->select(info.macAddress, &reason), qPrintable(reason));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel && app.m_radioModel->isConnected(), 15000);
        QTRY_VERIFY(!app.m_stationRadios->switching());
        // The choice is saved by the Connected handler (confirmChoice),
        // queued after the model reports connected. Under load this
        // change's own deadline can end it before that handler runs, so
        // wait for the save itself, not for the change to end.
        QTRY_COMPARE(app.m_stationRadios->savedChoice(), info.macAddress.toUpper());
        QVERIFY(app.m_stationRadios->pendingChoice().isEmpty());
        app.stop();
    }

    // The change's deadline (kRadioSwitchConnectBoundMs) and the connect
    // watchdog are both 2000 ms, so the deadline can end a change while the
    // failure of the radio it chose is still on its way. A window that
    // chooses again in that moment starts a new change, and the late
    // failure of the old radio must not end it: until the new radio has
    // connected, the change is under way (a third choice is refused) and,
    // once it has, its choice is saved. Found at load 10.75, where
    // aRadioChangeThatNeverConnectsLetsAWindowChooseAgain read an empty
    // saved choice from a change already marked finished. Here the
    // deadline is short so it ends the change first, and the old radio's
    // failure is held until the new choice has been made.
    void aLateFailureOfTheOldRadioDoesNotEndTheNextChange()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        QUdpSocket silent;
        QVERIFY(silent.bind(QHostAddress::LocalHost, 0));
        RadioInfo dead = info;
        dead.macAddress = QStringLiteral("AA:BB:CC:44:55:66");
        dead.address = QHostAddress::LocalHost;
        dead.port = silent.localPort();
        DaemonApp app;
        prepare(app);
        app.m_radioSwitchBoundMs = 300;
        app.m_discoveryProviderForTest = [&]() { return QList<RadioInfo>{info, dead}; };
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);

        QString reason;
        QVERIFY(app.m_stationRadios->select(dead.macAddress, &reason));
        // The silent radio's connection, watched on its own thread.
        std::atomic<bool> deadFailed {false};
        QPointer<RadioConnection> deadConnection;
        QTRY_VERIFY_WITH_TIMEOUT(
            app.m_radioModel && app.m_radioModel->connection()
                && app.m_radioModel->connection()->radioInfo().macAddress.compare(
                       dead.macAddress, Qt::CaseInsensitive) == 0,
            10000);
        deadConnection = app.m_radioModel->connection();
        connect(deadConnection.data(), &RadioConnection::connectFailed, deadConnection.data(),
                [&deadFailed](ConnectFailure, const QString&) { deadFailed = true; },
                Qt::DirectConnection);
        // The deadline ends the change before the watchdog fires.
        QTRY_VERIFY_WITH_TIMEOUT(!app.m_stationRadios->switching(), 1500);
        QVERIFY(!deadFailed);
        // The watchdog fires; its report waits in this thread's queue.
        const QDeadlineTimer watchdog(10000);
        while (!deadFailed && !watchdog.hasExpired()) {
            QThread::msleep(10);
        }
        QVERIFY(deadFailed);

        // Another window chooses the first radio while that report waits.
        QVERIFY2(app.m_stationRadios->select(info.macAddress, &reason), qPrintable(reason));
        QCoreApplication::processEvents();
        QVERIFY(app.m_stationRadios->switching());
        QVERIFY(!app.m_stationRadios->select(dead.macAddress, &reason));
        QCOMPARE(reason, StationRadios::switchingReason());

        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel && app.m_radioModel->isConnected(), 15000);
        QTRY_VERIFY(!app.m_stationRadios->switching());
        QCOMPARE(app.m_stationRadios->savedChoice(), info.macAddress.toUpper());
        QVERIFY(app.m_stationRadios->pendingChoice().isEmpty());
        app.stop();
    }

    // Fix wave, C1: the chosen radio's own connect failure ends the change.
    // The change's deadline is set far past this test's wait, so the change
    // can only end through that failure (the connect watchdog, 2000 ms), not
    // through the deadline.
    void theNewRadiosConnectFailureEndsTheChange()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        QUdpSocket silent;
        QVERIFY(silent.bind(QHostAddress::LocalHost, 0));
        RadioInfo dead = info;
        dead.macAddress = QStringLiteral("AA:BB:CC:44:55:66");
        dead.address = QHostAddress::LocalHost;
        dead.port = silent.localPort();
        DaemonApp app;
        prepare(app);
        app.m_radioSwitchBoundMs = 60000;
        app.m_discoveryProviderForTest = [&]() { return QList<RadioInfo>{info, dead}; };
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);

        QString reason;
        QVERIFY(app.m_stationRadios->select(dead.macAddress, &reason));
        std::atomic<bool> deadFailed {false};
        QTRY_VERIFY_WITH_TIMEOUT(
            app.m_radioModel && app.m_radioModel->connection()
                && app.m_radioModel->connection()->radioInfo().macAddress.compare(
                       dead.macAddress, Qt::CaseInsensitive) == 0,
            10000);
        QVERIFY(app.m_stationRadios->switching());
        QVERIFY(app.m_radioSwitchDeadline->isActive());
        connect(app.m_radioModel->connection(), &RadioConnection::connectFailed,
                app.m_radioModel->connection(),
                [&deadFailed](ConnectFailure, const QString&) { deadFailed = true; },
                Qt::DirectConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!app.m_stationRadios->switching(), 15000);
        QVERIFY(deadFailed);
        QVERIFY(!app.m_radioSwitchDeadline->isActive());
        QVERIFY(!app.m_radioModel->stationRadioChangeUnderway());
        QVERIFY(app.m_stationRadios->savedChoice().isEmpty());
        app.stop();
    }

    // Between a choice and the restart that runs it, a Connected from the
    // run the Core has now (the old radio's, reported late) is not the
    // change's, so the change stays under way and a third choice is still
    // refused. The state handler is called in that window directly, as a
    // queued state report would reach it before the restart's turn.
    void aConnectedBeforeTheRestartDoesNotEndTheChange()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        RadioInfo other = info;
        other.macAddress = QStringLiteral("AA:BB:CC:77:88:99");
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [&]() { return QList<RadioInfo>{info, other}; };
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);

        QString reason;
        QVERIFY(app.m_stationRadios->select(other.macAddress, &reason));
        QVERIFY(app.m_radioChangeRestartPending);
        QVERIFY(app.m_stationRadios->switching());
        QCOMPARE(app.m_radioModel->connectionState(), ConnectionState::Connected);
        const QString savedBefore = app.m_stationRadios->savedChoice();
        app.onRadioStateForRecovery(ConnectionState::Connected);
        QVERIFY(app.m_stationRadios->switching());
        QVERIFY(app.m_radioModel->stationRadioChangeUnderway());
        QVERIFY(!app.m_stationRadios->select(info.macAddress, &reason));
        QCOMPARE(reason, StationRadios::switchingReason());
        // The old radio's Connected does not save the pending choice.
        QCOMPARE(app.m_stationRadios->savedChoice(), savedBefore);
        QCOMPARE(app.m_stationRadios->pendingChoice(), other.macAddress.toUpper());
        app.stop();
    }

    // A discovery completion already queued when a window chooses runs
    // ahead of the restart: it connects the run's radio while the change is
    // switching, so it starts a deadline. That deadline is the old run's;
    // the restart stops it, so it cannot end the new change before the new
    // radio is found. Here the new run's discovery is held, so only that
    // leftover deadline could end the change.
    void aDeadlineFromTheRunBeforeARestartDoesNotEndTheChange()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        P1FakeRadio otherFake;
        otherFake.start();
        RadioInfo other = infoFor(otherFake);
        other.macAddress = QStringLiteral("AA:BB:CC:77:88:99");
        // 0: only the other radio is in sight (the run waits for its own).
        // 1: the old run's scan, held until released, then both radios.
        // 2: the new run's scan, held until released, then both radios.
        std::atomic<int> phase {0};
        std::atomic<bool> oldScanHeld {false};
        std::atomic<bool> newScanHeld {false};
        QSemaphore releaseOldScan;
        QSemaphore releaseNewScan;
        DaemonApp app;
        prepare(app);
        app.m_radioSwitchBoundMs = 300;
        app.m_discoveryProviderForTest = [&]() {
            const int now = phase.load();
            if (now == 1) {
                oldScanHeld = true;
                releaseOldScan.tryAcquire(1, 30000);
            } else if (now == 2) {
                newScanHeld = true;
                releaseNewScan.tryAcquire(1, 30000);
            }
            return now == 0 ? QList<RadioInfo>{other} : QList<RadioInfo>{info, other};
        };
        const auto releaseScans = qScopeGuard([&]() {
            releaseOldScan.release(100);
            releaseNewScan.release(100);
        });
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_stationRadios->radioFor(other.macAddress).has_value(),
                                 10000);
        QVERIFY(!app.m_radioModel->connection());

        // The old run's next scan finds its radio; its completion is queued
        // before the choice is made.
        phase = 1;
        QTRY_VERIFY_WITH_TIMEOUT(oldScanHeld.load(), 10000);
        QThread* const oldScan = app.m_radioDiscoveryThread.get();
        QVERIFY(oldScan);
        phase = 2;
        releaseOldScan.release();
        QVERIFY(oldScan->wait(10000));

        QString reason;
        QVERIFY(app.m_stationRadios->select(other.macAddress, &reason));
        QVERIFY(app.m_radioChangeRestartPending);
        QTRY_VERIFY_WITH_TIMEOUT(!app.m_radioChangeRestartPending, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(newScanHeld.load(), 10000);
        QVERIFY(app.m_stationRadios->switching());
        QVERIFY(!app.m_radioSwitchDeadline->isActive());
        // Past the old run's deadline, the change is still under way.
        QTest::qWait(app.m_radioSwitchBoundMs * 2);
        QVERIFY(app.m_stationRadios->switching());
        QVERIFY(app.m_radioModel->stationRadioChangeUnderway());
        QVERIFY(!app.m_stationRadios->select(info.macAddress, &reason));
        QCOMPARE(reason, StationRadios::switchingReason());

        // The new run finds its radio, which connects and ends the change.
        app.m_radioSwitchBoundMs = DaemonApp::kRadioSwitchConnectBoundMs;
        releaseNewScan.release();
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 15000);
        QCOMPARE(app.m_radioModel->connection()->radioInfo().macAddress.toUpper(),
                 other.macAddress.toUpper());
        QTRY_COMPARE(app.m_stationRadios->savedChoice(), other.macAddress.toUpper());
        QTRY_VERIFY(!app.m_stationRadios->switching());
        app.stop();
    }

    // After a radio change's restart and before the new run's first
    // discovery, the operator disconnects: the run stops looking for the
    // radio, so nothing would end the change (no connect, no deadline).
    // It ends where the run stops, so a window can choose again.
    void aDisconnectAfterTheRestartEndsTheChange()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        RadioInfo other = info;
        other.macAddress = QStringLiteral("AA:BB:CC:77:88:99");
        std::atomic<bool> hold {false};
        std::atomic<bool> held {false};
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = holdableScans({info, other}, hold, held);
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);

        hold = true;
        QString reason;
        QVERIFY(app.m_stationRadios->select(other.macAddress, &reason));
        QTRY_VERIFY_WITH_TIMEOUT(!app.m_radioChangeRestartPending, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(held.load(), 10000);
        QVERIFY(app.m_stationRadios->switching());
        QVERIFY(!app.m_radioSwitchDeadline->isActive());

        app.m_radioModel->disconnectFromRadio();
        QVERIFY(!app.m_radioRecoveryEnabled);
        QVERIFY(!app.m_radioDiscoveryThread);
        QVERIFY(!app.m_stationRadios->switching());
        QVERIFY(!app.m_radioModel->stationRadioChangeUnderway());
        app.stop();
    }

    // The same moment, ended by a station release instead: the release
    // cancels discovery, so it ends the change too.
    void aStationReleaseAfterTheRestartEndsTheChange()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        RadioInfo other = info;
        other.macAddress = QStringLiteral("AA:BB:CC:77:88:99");
        std::atomic<bool> hold {false};
        std::atomic<bool> held {false};
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = holdableScans({info, other}, hold, held);
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);

        hold = true;
        QString reason;
        QVERIFY(app.m_stationRadios->select(other.macAddress, &reason));
        QTRY_VERIFY_WITH_TIMEOUT(!app.m_radioChangeRestartPending, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(held.load(), 10000);
        QVERIFY(app.m_stationRadios->switching());

        app.beginStationRelease();
        QVERIFY(!app.m_radioDiscoveryThread);
        QVERIFY(!app.m_stationRadios->switching());
        QVERIFY(!app.m_radioModel->stationRadioChangeUnderway());
        app.stop();
    }

    // The operator disconnects while the new radio's connect is running
    // (its nested WDSP start): finishRadioDiscovery then returns with
    // recovery off and arms no deadline. The disconnect ends the change.
    void aDisconnectDuringTheNewRadiosConnectEndsTheChange()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        P1FakeRadio otherFake;
        otherFake.start();
        RadioInfo other = infoFor(otherFake);
        other.macAddress = QStringLiteral("AA:BB:CC:77:88:99");
        std::atomic<bool> hold {false};
        std::atomic<bool> held {false};
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = holdableScans({info, other}, hold, held);
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);

        hold = true;
        QString reason;
        QVERIFY(app.m_stationRadios->select(other.macAddress, &reason));
        QTRY_VERIFY_WITH_TIMEOUT(!app.m_radioChangeRestartPending, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(held.load(), 10000);
        bool insideConnect = false;
        connect(app.m_radioModel->wdspEngine(), &WdspEngine::initializedChanged,
                &app, [&](bool ready) {
            if (ready && !insideConnect) {
                insideConnect = app.m_radioConnectInProgress;
                app.m_radioModel->disconnectFromRadio();
            }
        });
        hold = false;
        QTRY_VERIFY_WITH_TIMEOUT(insideConnect, 10000);
        QTRY_VERIFY(!app.m_radioConnectInProgress);
        QVERIFY(!app.m_radioRecoveryEnabled);
        QVERIFY(!app.m_stationRadios->switching());
        QVERIFY(!app.m_radioSwitchDeadline->isActive());
        QVERIFY(!app.m_radioModel->stationRadioChangeUnderway());
        app.stop();
    }

    // The Core stops while the new radio's connect is running: the stop is
    // deferred (finishRadioDiscovery's m_stopDeferred return) and arms no
    // deadline. The stop ends the change, so the Core started again in
    // this process is not left refusing every choice and every key.
    void aStopDuringTheNewRadiosConnectEndsTheChange()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        P1FakeRadio otherFake;
        otherFake.start();
        RadioInfo other = infoFor(otherFake);
        other.macAddress = QStringLiteral("AA:BB:CC:77:88:99");
        std::atomic<bool> hold {false};
        std::atomic<bool> held {false};
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = holdableScans({info, other}, hold, held);
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);

        hold = true;
        QString reason;
        QVERIFY(app.m_stationRadios->select(other.macAddress, &reason));
        QTRY_VERIFY_WITH_TIMEOUT(!app.m_radioChangeRestartPending, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(held.load(), 10000);
        bool deferred = false;
        connect(app.m_radioModel->wdspEngine(), &WdspEngine::initializedChanged,
                &app, [&](bool ready) {
            if (ready && !deferred) {
                app.stop();
                deferred = app.m_stopDeferred;
            }
        });
        hold = false;
        QTRY_VERIFY_WITH_TIMEOUT(deferred, 10000);
        QTRY_VERIFY(!app.m_radioModel);
        QVERIFY(!app.m_stationRadios->switching());
        QVERIFY(!app.m_radioSwitchDeadline->isActive());

        QVERIFY(app.start(cfg));
        QVERIFY(!app.m_radioModel->stationRadioChangeUnderway());
        app.stop();
    }

    // A restart onto a run with no discovery (the test board) never finds
    // the chosen radio, so the restart ends the change itself.
    void aRestartOntoATestBoardEndsTheChange()
    {
        DaemonApp app;
        prepare(app);
        app.primeBoardForTest(HPSDRHW::HermesLite, QStringLiteral("AA:BB:CC:11:22:33"));
        QVERIFY(app.start(testCoreConfig()));
        RadioInfo other;
        other.macAddress = QStringLiteral("AA:BB:CC:77:88:99");
        other.boardType = HPSDRHW::HermesLite;
        other.protocol = ProtocolVersion::Protocol1;
        app.m_stationRadios->setVisible({other});

        QString reason;
        QVERIFY2(app.m_stationRadios->select(other.macAddress, &reason), qPrintable(reason));
        QVERIFY(app.m_stationRadios->switching());
        QTRY_VERIFY_WITH_TIMEOUT(!app.m_radioChangeRestartPending, 10000);
        QVERIFY(app.m_radioModel);
        QVERIFY(!app.m_radioRecoveryEnabled);
        QVERIFY(!app.m_stationRadios->switching());
        QVERIFY(!app.m_radioModel->stationRadioChangeUnderway());
        app.stop();
    }

    // A restart whose start() fails (its configuration does not validate)
    // leaves the old run on its own radio. The change ends and its choice
    // is dropped: kept, the next start() in this process would switch to a
    // radio nobody chose for it, and forget() would refuse that radio.
    void aRestartThatCannotStartDropsTheChoice()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        RadioInfo other = info;
        other.macAddress = QStringLiteral("AA:BB:CC:77:88:99");
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [&]() { return QList<RadioInfo>{info, other}; };
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);
        RadioModel* const model = app.m_radioModel.get();

        DaemonConfig invalid = cfg;
        invalid.displayApplicationBytesPerSecond = 1;
        invalid.spectrumSampleUnitsPerSecond.reset();
        QString error;
        QVERIFY(!invalid.validate(&error));
        app.m_radioConfig = invalid;

        QString reason;
        QVERIFY(app.m_stationRadios->select(other.macAddress, &reason));
        QTRY_VERIFY_WITH_TIMEOUT(!app.m_radioChangeRestartPending, 10000);
        QCOMPARE(app.m_radioModel.get(), model);
        QVERIFY(model->isConnected());
        QVERIFY(!app.m_stationRadios->switching());
        QVERIFY(!model->stationRadioChangeUnderway());
        QVERIFY(app.m_stationRadios->pendingChoice().isEmpty());
        QCOMPARE(app.m_selectedRadioMac, info.macAddress.toUpper());
        QVERIFY2(app.m_stationRadios->forget(other.macAddress, &reason), qPrintable(reason));

        app.stop();
        QVERIFY(app.start(cfg));
        QCOMPARE(app.m_selectedRadioMac, info.macAddress.toUpper());
        app.stop();
    }

    // A stop, a station release or the operator's disconnect between a
    // window's choice and the restart that runs it: the run stays stopped
    // (the queued restart does not start it again), the change ends, and
    // its choice is dropped.
    void aStopBeforeTheRestartRunsStaysStopped_data()
    {
        QTest::addColumn<int>("how");
        QTest::newRow("stop") << 0;
        QTest::newRow("station release") << 1;
        QTest::newRow("operator disconnect") << 2;
    }

    void aStopBeforeTheRestartRunsStaysStopped()
    {
        QFETCH(int, how);
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        RadioInfo other = info;
        other.macAddress = QStringLiteral("AA:BB:CC:77:88:99");
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [&]() { return QList<RadioInfo>{info, other}; };
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);
        RadioModel* const model = app.m_radioModel.get();

        QString reason;
        QVERIFY(app.m_stationRadios->select(other.macAddress, &reason));
        QVERIFY(app.m_radioChangeRestartPending);
        // The same turn: the restart is still queued.
        if (how == 0) {
            app.stop();
        } else if (how == 1) {
            app.beginStationRelease();
            QCOMPARE(app.tryCompleteStationRelease(&reason),
                     DaemonApp::StationReleaseResult::Stopped);
        } else {
            model->disconnectFromRadio();
        }

        // The restart's turn comes and goes without starting anything.
        QTest::qWait(50);
        QVERIFY(!app.m_radioRecoveryEnabled);
        QVERIFY(!app.m_radioDiscoveryThread);
        if (how == 2) {
            QCOMPARE(app.m_radioModel.get(), model);
            QVERIFY(!model->isConnected());
            QVERIFY(!model->stationRadioChangeUnderway());
        } else {
            QVERIFY(!app.m_radioModel);
        }
        QCOMPARE(app.m_selectedRadioMac, info.macAddress.toUpper());
        QVERIFY(!app.m_radioChangeRestartPending);
        QVERIFY(!app.m_stationRadios->switching());
        QVERIFY(app.m_stationRadios->pendingChoice().isEmpty());
        app.stop();
    }

    // Fix wave, M3: a key that arrives while the Core changes its radio is
    // refused (the old radio is still connected until the change runs), so
    // nothing keyed is torn down.
    void aKeyDuringARadioChangeIsRefused()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        RadioInfo other = info;
        other.macAddress = QStringLiteral("AA:BB:CC:77:88:99");
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [&]() { return QList<RadioInfo>{info, other}; };
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        cfg.remoteTransmitAllowed = true; // so the change is what refuses
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);
        MoxController* const mox = app.m_radioModel->moxController();
        QVERIFY(mox);
        QSignalSpy rejected(mox, &MoxController::moxRejected);

        QString reason;
        QVERIFY(app.m_stationRadios->select(other.macAddress, &reason));
        // The same turn as the accepted answer, before the change runs.
        mox->setMox(true);
        QVERIFY(!mox->isMox());
        QVERIFY(rejected.count() >= 1);
        QCOMPARE(rejected.last().at(0).toString(), StationRadios::switchingReason());
        // Were the radio on the air all the same when the change runs (the
        // gate lifted here to force it), the change is refused then: the
        // radio stays connected and keyed, and the choice goes.
        RadioModel* const model = app.m_radioModel.get();
        model->setStationRadioChangeUnderway(false);
        mox->setMox(true);
        QVERIFY(mox->isMox());
        QTRY_VERIFY(!app.m_stationRadios->switching());
        QCOMPARE(app.m_radioModel.get(), model);
        QVERIFY(model->isConnected());
        QVERIFY(mox->isMox());
        QVERIFY(app.m_stationRadios->pendingChoice().isEmpty());
        QCOMPARE(app.m_selectedRadioMac, info.macAddress.toUpper());
        mox->setMox(false);
        app.stop();
    }

    // TX safety fix round 3: after a lost link the Core retires the radio
    // and rebuilds it. A rebuilt link that never answers reports
    // Disconnected itself, from its own connect timeout
    // (P1RadioConnection::onConnectTimeout), before the Core's next retry.
    // The lost-link lock holds through that, a key in the next attempt's
    // Connecting is refused with the link reason, and the lock lifts only
    // when a link reaches Connected.
    void aFailedRebuildKeepsTheLostLinkLock()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        // The same radio at an address that reads nothing back.
        QUdpSocket silent;
        QVERIFY(silent.bind(QHostAddress::LocalHost, 0));
        RadioInfo dead = info;
        dead.address = QHostAddress::LocalHost;
        dead.port = silent.localPort();
        std::atomic<bool> offerDead {false};
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [&]() {
            return QList<RadioInfo>{offerDead.load() ? dead : info};
        };
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress.toUpper();
        cfg.remoteTransmitAllowed = true; // so the lost link is what refuses
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);
        RadioModel* const model = app.m_radioModel.get();
        MoxController* const mox = model->moxController();
        QVERIFY(mox);
        const QString reason = RadioModel::radioLinkDownReason();
        QSignalSpy downChanged(model, &RadioModel::radioLinkDownChanged);
        QSignalSpy rejected(mox, &MoxController::moxRejected);

        // Count the rebuilt links that time out on their own. The watchers
        // hang off `watchScope`, declared after the locals they capture, so
        // they are disconnected before those locals go (app outlives them).
        int timedOut = 0;
        QPointer<RadioConnection> failed;
        QPointer<RadioConnection> watched;
        QObject watchScope;
        connect(model, &RadioModel::connectionStateChanged, &watchScope,
                [&](ConnectionState) {
            RadioConnection* const conn = model->connection();
            if (conn == nullptr || conn == watched) {
                return;
            }
            watched = conn;
            // connectFailed comes from the connection's thread, queued, and
            // the model can delete that connection before the slot runs; a
            // raw pointer assigned to `failed` then wrote through freed
            // memory (the Linux SegFault). The guard is taken here, while
            // the connection is alive, and reads null once it is gone.
            const QPointer<RadioConnection> guard(conn);
            connect(conn, &RadioConnection::connectFailed, &watchScope,
                    [&, guard](ConnectFailure, const QString&) {
                ++timedOut;
                failed = guard;
            });
        });

        offerDead = true;
        model->onConnectionStateChangedForTest(ConnectionState::LinkLost);
        QTRY_VERIFY(!model->connection());
        QVERIFY(model->isRadioLinkDown());
        QCOMPARE(downChanged.count(), 1);

        // The rebuilt link times out and reports Disconnected itself.
        QTRY_VERIFY_WITH_TIMEOUT(timedOut >= 1, 15000);
        QVERIFY(model->isRadioLinkDown());
        // The Core's next attempt, Connecting: still locked, a key refused.
        QTRY_VERIFY_WITH_TIMEOUT(model->connection() != nullptr
                                     && model->connection() != failed.data()
                                     && model->connectionState() == ConnectionState::Connecting,
                                 15000);
        QVERIFY(model->isRadioLinkDown());
        QVERIFY(mox->isRadioLinkDown());
        QCOMPARE(mox->transmitBlockReason(), reason);
        const int before = rejected.count();
        mox->setMox(true);
        QCoreApplication::processEvents();
        QVERIFY(!mox->isMox());
        QVERIFY(!model->mox());
        QCOMPARE(rejected.count(), before + 1);
        QCOMPARE(rejected.last().at(0).toString(), reason);
        for (const QList<QVariant>& args : downChanged) {
            QVERIFY2(args.at(0).toBool(), "the lock lifted before a link was Connected");
        }

        // The radio answers again: Connected lifts the lock.
        offerDead = false;
        QTRY_VERIFY_WITH_TIMEOUT(model->isConnected(), 20000);
        QTRY_VERIFY(!model->isRadioLinkDown());
        QCOMPARE(downChanged.last().at(0).toBool(), false);
        QVERIFY(mox->transmitBlockReason() != reason);
        app.stop();
    }

    void quietPeriodAndBusyRadioDoNotStartAConnection()
    {
        std::atomic<int> scans {0};
        DaemonApp app;
        prepare(app);
        RadioInfo busy;
        busy.macAddress = QStringLiteral("aa:bb:cc:11:22:33");
        busy.inUse = true;
        app.m_discoveryProviderForTest = [&]() {
            ++scans;
            return QList<RadioInfo>{busy};
        };
        RadioDiscovery guard;
        guard.holdOffScans(std::chrono::seconds(10));
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = busy.macAddress;
        QVERIFY(app.start(cfg));
        QTRY_VERIFY(app.m_radioRetryTimer->interval() > 1000);
        QCOMPARE(scans.load(), 0);
        QVERIFY(!app.m_radioDiscoveryThread);
        RadioDiscovery::clearHoldOffForTest();
        app.m_radioRetryTimer->start(0);
        QTRY_VERIFY(scans >= 2);
        QVERIFY(!app.m_radioModel->connection());
        QVERIFY(app.m_radioRetryNextMs <= app.m_radioRetryMaximumMs);
        app.stop();
        QVERIFY(!app.m_radioRetryTimer->isActive());
    }

    void workerAndQueuedResultsCancelOnStop()
    {
        std::atomic<bool> entered {false};
        std::atomic<bool> workerThread {false};
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [&]() {
            workerThread = QThread::currentThread() != app.thread();
            entered = true;
            while (!QThread::currentThread()->isInterruptionRequested()) {
                QThread::msleep(1);
            }
            RadioInfo late;
            late.macAddress = QStringLiteral("aa:bb:cc:11:22:33");
            return QList<RadioInfo>{late};
        };
        int ticks = 0;
        QTimer pulse;
        pulse.setInterval(1);
        connect(&pulse, &QTimer::timeout, &app, [&]() { ++ticks; });
        pulse.start();
        QVERIFY(app.start(testCoreConfig()));
        QTRY_VERIFY(entered.load() && ticks > 2);
        QVERIFY(workerThread.load());
        app.stop();
        QCoreApplication::processEvents();
        QVERIFY(!app.m_radioModel);
        QVERIFY(!app.m_radioDiscoveryThread);
        QVERIFY(!app.m_radioRetryTimer->isActive());

        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(testCoreConfig()));
        QCoreApplication::processEvents();
        QVERIFY(!app.m_radioModel->connection());
        QVERIFY(!app.m_radioDiscoveryThread);
        app.stop();
    }

    void stopDuringConnectionSetupDoesNotDialAfterCancellation()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [info]() { return QList<RadioInfo>{info}; };
        QVERIFY(app.start(testCoreConfig()));
        bool stopReached = false;
        connect(app.m_radioModel->wdspEngine(), &WdspEngine::initializedChanged,
                &app, [&](bool ready) {
            if (ready) {
                stopReached = true;
                app.stop();
                // The current connect stack still owns this model until it
                // returns, even though all recovery work is cancelled.
                QVERIFY(app.m_radioModel);
                QVERIFY(app.m_stopDeferred);
            }
        });
        QTRY_VERIFY_WITH_TIMEOUT(stopReached, 10000);
        QTRY_VERIFY(!app.m_radioModel);
        QVERIFY(!fake.isRunning());
        QVERIFY(!app.m_radioRetryTimer->isActive());
        QVERIFY(!app.m_radioDiscoveryThread);
    }

    void lossRetiresPipelinePreservesSlicesAndPinnedIdentity()
    {
        P1FakeRadio fake;
        fake.start();
        const RadioInfo info = infoFor(fake);
        std::atomic<bool> offerSelected {true};
        std::atomic<int> scans {0};
        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [&]() {
            ++scans;
            RadioInfo candidate = info;
            if (!offerSelected.load()) {
                candidate.macAddress = QStringLiteral("aa:bb:cc:99:99:99");
            }
            return QList<RadioInfo>{candidate};
        };
        DaemonConfig cfg = testCoreConfig();
        cfg.sliceCount = 2;
        QVERIFY(app.start(cfg));
        QTRY_VERIFY_WITH_TIMEOUT(app.m_radioModel->isConnected(), 10000);
        QTRY_COMPARE(app.sliceCount(), 2);
        RadioModel* const model = app.m_radioModel.get();
        const QPointer<SliceModel> a = model->slices().first();
        const QPointer<SliceModel> b = model->slices().last();
        a->setFrequency(3865100);
        b->setFrequency(14225000);
        b->setDspMode(DSPMode::USB);
        model->setActiveSlice(b->sliceIndex());
        offerSelected = false;
        // P2 UDP loss detection has its own real-wire regression. Here inject
        // its model-state boundary to exercise actual daemon/DSP retirement.
        model->onConnectionStateChangedForTest(ConnectionState::LinkLost);
        QTRY_VERIFY(!model->connection());
        QCOMPARE(model->connectionState(), ConnectionState::Disconnected);
        // TX safety fix round 3: the Core's retire is a recovery, not the
        // operator's disconnect, so the lost-link lock holds through it.
        QVERIFY(model->isRadioLinkDown());
        QVERIFY(model->moxController()->isRadioLinkDown());
        QVERIFY(!model->wdspEngine()->isInitialized());
        QVERIFY2(a, "loss retirement deleted the original Slice A object");
        QVERIFY2(b, "loss retirement deleted the original Slice B object");
        QVERIFY(a->streamIndex() < 0);
        QVERIFY(b->streamIndex() < 0);
        QCOMPARE(app.m_selectedRadioMac, info.macAddress);
        QCOMPARE(model->activeSlice(), b.data());
        const int afterLoss = scans.load();
        RadioDiscovery::clearHoldOffForTest();
        app.m_radioRetryTimer->start(0);
        QTRY_VERIFY(scans > afterLoss);
        QVERIFY(!model->connection());
        offerSelected = true;
        QTRY_VERIFY_WITH_TIMEOUT(model->isConnected(), 10000);
        QCOMPARE(model->slices().size(), 2);
        QVERIFY2(a, "recovery deleted the original Slice A object");
        QVERIFY2(b, "recovery deleted the original Slice B object");
        QCOMPARE(model->slices().first(), a.data());
        QCOMPARE(model->slices().last(), b.data());
        QCOMPARE(model->activeSlice(), b.data());
        QCOMPARE(a->frequency(), 3865100.0);
        QCOMPARE(b->frequency(), 14225000.0);
        QVERIFY(model->receiveOnlyStationPolicy());
        QVERIFY(!model->mox());
        QVERIFY(!model->isRadioLinkDown());

        model->disconnectFromRadio();
        QVERIFY(!app.m_radioRecoveryEnabled);
        QVERIFY(!app.m_radioRetryTimer->isActive());
        QCoreApplication::processEvents();
        QVERIFY(!model->connection());
        app.stop();
    }

    // R-R3-34, native check G1 (2026-09-22). The operator collapsed the GUI
    // to one pane, which moves a RADE receiver on 40 m onto pan 0 by writing
    // nothing but its panKey. Core kept that receiver on its own DDC, and the
    // layout it saved was exactly the Rock's record. The next Core start
    // forced the receiver into pan 0's 20 m window and refused it. Built from
    // live use, not a hand-written record, so the saved form is whatever Core
    // really writes.
    void livePanMoveSurvivesCoreRestart()
    {
        NereusSDR::Test::P2FakeRadio fake;
        QVERIFY(fake.start());
        RadioInfo info = fake.radioInfo();
        info.name = QStringLiteral("Fake P2 ANAN-G2");
        info.boardType = HPSDRHW::Saturn; // ANAN-G2 capability row: five receivers
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress;
        cfg.sliceCount = 1;
        cfg.sampleRateHz = 48000;

        QTimer ingress;
        ingress.setInterval(5);
        connect(&ingress, &QTimer::timeout, &fake, [&fake]() {
            if (fake.hasClient()) {
                fake.sendDdc(2);
                fake.sendDdc(3);
                fake.sendStatus();
            }
        });
        ingress.start();
        const auto startDaemon = [&](DaemonApp& app) -> RadioModel* {
            prepare(app);
            app.m_discoveryProviderForTest = [info]() { return QList<RadioInfo>{info}; };
            if (!app.start(cfg)) {
                return nullptr;
            }
            // Discovery completes on a later event-loop turn, so the loopback
            // ports are in place before the connection is created. The
            // timeouts are the production ones.
            app.m_radioModel->configureP2TransportForTest(
                fake.outboundPortBase(), fake.inputRolePortBase(), 2000, 3000);
            return app.m_radioModel.get();
        };

        {
            DaemonApp app;
            RadioModel* const model = startDaemon(app);
            QVERIFY(model);
            QTRY_VERIFY_WITH_TIMEOUT(model->isConnected(), 15000);
            QTRY_COMPARE(app.sliceCount(), 1);
            QTRY_COMPARE(model->receiveLayoutRestoreState(), QStringLiteral("accepted"));
            SliceModel* const a = model->sliceById(0);
            QVERIFY(a);
            QCOMPARE(model->addSlice(QStringLiteral("pan-1")), 1);
            SliceModel* const b = model->sliceById(1);
            QVERIFY(b);

            {
                // A GUI edit reaches Core as StationServer::handlePropertyWrite
                // -> StateMirror::applyInbound, over the same StateMirror and
                // ObjectRegistry pair StationServer builds for the session.
                StateMirror mirror;
                ObjectRegistry registry(model, &mirror);
                registry.backfillExistingSlices();
                const auto write = [&mirror](int id, const QByteArray& property,
                                             const QVariant& value) {
                    return mirror.applyInbound(ObjectRegistry::keyForSlice(id),
                                               property, value);
                };
                QVERIFY(write(0, "frequency", 14'290'000.0).accepted);
                QVERIFY(write(0, "dspMode", int(DSPMode::USB)).accepted);
                QVERIFY(write(1, "frequency", 7'227'600.0).accepted);
                QVERIFY(write(1, "dspMode", int(DSPMode::RADE_U)).accepted);
                // MainWindow::applyPanLayout("1") -> rehomeSlicesToPans writes
                // only panKey on each remote slice.
                QVERIFY(write(1, "panKey", QStringLiteral("pan-0")).accepted);
            }

            // The move is a label change: B keeps its own receiver running.
            QCOMPARE(b->panKey(), QStringLiteral("pan-0"));
            QCOMPARE(b->frequency(), 7'227'600.0);
            QCOMPARE(b->dspMode(), DSPMode::RADE_U);
            QVERIFY(a->streamIndex() >= 0);
            QVERIFY(b->streamIndex() >= 0);
            QVERIFY(b->streamIndex() != a->streamIndex());
            RxChannel* const bChannel = model->wdspEngine()->rxChannel(1);
            QVERIFY(bChannel && bChannel->isActive());
            QCOMPARE(model->restoredRadeReceiveOwner(), std::optional<int>(1));
            RadeChannel* const bRade = model->wdspEngine()->radeChannel(1);
            QVERIFY(bRade && bRade->isActive());
            app.stop();
        }

        // What a process restart reads: the file, not this process's memory.
        AppSettings::instance().clear();
        AppSettings::instance().load();
        const auto saved = ReceiveLayoutStore::load(AppSettings::instance(), info.macAddress);
        QCOMPARE(static_cast<int>(saved.state),
                 static_cast<int>(ReceiveLayoutStore::LoadState::Loaded));
        QCOMPARE(saved.radeRxOwnerId, std::optional<int>(1));
        QCOMPARE(saved.slices.size(), 2);
        QCOMPARE(saved.slices.at(0).id, 0);
        QCOMPARE(saved.slices.at(0).panKey, QStringLiteral("pan-0"));
        QCOMPARE(saved.slices.at(0).frequencyHz, 14'290'000.0);
        QCOMPARE(saved.slices.at(0).dspMode, DSPMode::USB);
        QCOMPARE(saved.slices.at(1).id, 1);
        QCOMPARE(saved.slices.at(1).panKey, QStringLiteral("pan-0"));
        QCOMPARE(saved.slices.at(1).frequencyHz, 7'227'600.0);
        QCOMPARE(saved.slices.at(1).dspMode, DSPMode::RADE_U);

        {
            DaemonApp app;
            RadioModel* const model = startDaemon(app);
            QVERIFY(model);
            QTRY_VERIFY_WITH_TIMEOUT(model->isConnected(), 15000);
            QTRY_COMPARE(model->receiveLayoutRestoreState(), QStringLiteral("accepted"));
            QVERIFY2(model->receiveLayoutRestoreMessage().isEmpty(),
                     qPrintable(model->receiveLayoutRestoreMessage()));
            QCOMPARE(model->slices().size(), 2);
            SliceModel* const a = model->sliceById(0);
            SliceModel* const b = model->sliceById(1);
            QVERIFY(a && b);
            QCOMPARE(a->frequency(), 14'290'000.0);
            QCOMPARE(a->dspMode(), DSPMode::USB);
            QCOMPARE(b->frequency(), 7'227'600.0);
            QCOMPARE(b->dspMode(), DSPMode::RADE_U);
            QCOMPARE(b->panKey(), QStringLiteral("pan-0"));
            QVERIFY(a->streamIndex() >= 0);
            QVERIFY(b->streamIndex() >= 0);
            QVERIFY(b->streamIndex() != a->streamIndex());
            QCOMPARE(model->restoredRadeReceiveOwner(), std::optional<int>(1));
            RadeChannel* const bRade = model->wdspEngine()->radeChannel(1);
            QVERIFY(bRade && bRade->isActive());
            app.stop();
        }
    }

    // RADE reason (2026-09-30). A saved layout with two RADE receivers
    // starts RADE on the one it names (B). A stays in RADE with no decoder,
    // muted, and says why. When B leaves RADE nothing starts A's decoder by
    // itself; A's reason says how to start it, and doing so clears it. With
    // the configured model file missing, B says that instead.
    void restoredRadeSlicesWithoutADecoderSayWhy_data()
    {
        QTest::addColumn<bool>("modelMissing");
        QTest::newRow("B decodes") << false;
        QTest::newRow("model file missing") << true;
    }

    void restoredRadeSlicesWithoutADecoderSayWhy()
    {
        QFETCH(bool, modelMissing);
        NereusSDR::Test::P2FakeRadio fake;
        QVERIFY(fake.start());
        RadioInfo info = fake.radioInfo();
        info.name = QStringLiteral("Fake P2 ANAN-G2");
        info.boardType = HPSDRHW::Saturn;
        DaemonConfig cfg = testCoreConfig();
        cfg.radioMac = info.macAddress;
        cfg.sliceCount = 1;
        cfg.sampleRateHz = 48000;

        QString error;
        QVERIFY2(ReceiveLayoutStore::stage(AppSettings::instance(), info.macAddress,
                     {{0, QStringLiteral("pan-0"), 14'236'000.0, DSPMode::RADE_U},
                      {1, QStringLiteral("pan-0"), 7'177'000.0, DSPMode::RADE_U}},
                     &error, std::optional<int>(1)),
                 qPrintable(error));
        if (modelMissing) {
            AppSettings::instance().setValue(
                QStringLiteral("Rade/ModelPath"),
                QDir::temp().filePath(QStringLiteral("nereus-no-such-rade-model.bin")));
        }
        QVERIFY2(AppSettings::instance().save(&error), qPrintable(error));

        QTimer ingress;
        ingress.setInterval(5);
        connect(&ingress, &QTimer::timeout, &fake, [&fake]() {
            if (fake.hasClient()) {
                fake.sendDdc(2);
                fake.sendDdc(3);
                fake.sendStatus();
            }
        });
        ingress.start();

        DaemonApp app;
        prepare(app);
        app.m_discoveryProviderForTest = [info]() { return QList<RadioInfo>{info}; };
        QVERIFY(app.start(cfg));
        RadioModel* const model = app.m_radioModel.get();
        model->configureP2TransportForTest(fake.outboundPortBase(),
                                           fake.inputRolePortBase(), 2000, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(model->isConnected(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(
            model->receiveLayoutRestoreState() == QStringLiteral("accepted")
                || model->receiveLayoutRestoreState() == QStringLiteral("degraded"),
            15000);
        QCOMPARE(model->slices().size(), 2);
        SliceModel* const a = model->sliceById(0);
        SliceModel* const b = model->sliceById(1);
        QVERIFY(a && b);
        QCOMPARE(a->dspMode(), DSPMode::RADE_U);
        QCOMPARE(b->dspMode(), DSPMode::RADE_U);
        QCOMPARE(model->restoredRadeReceiveOwner(), std::optional<int>(1));
        QVERIFY(model->wdspEngine()->radeChannel(0) == nullptr);
        const QString notDecoding =
            QStringLiteral("RADE is not decoding on slice A. Change slice A to another mode "
                           "and back to RADE to start it.");

        if (modelMissing) {
            RadeChannel* const bRade = model->wdspEngine()->radeChannel(1);
            QVERIFY(bRade == nullptr || !bRade->isActive());
            QTRY_COMPARE(b->radeReason(),
                         QStringLiteral("RADE could not start on slice B: the RADE model file "
                                        "was not found."));
            QCOMPARE(a->radeReason(), notDecoding);
            app.stop();
            return;
        }

        RadeChannel* const bRade = model->wdspEngine()->radeChannel(1);
        QVERIFY(bRade && bRade->isActive());
        QVERIFY2(b->radeReason().isEmpty(), qPrintable(b->radeReason()));
        QTRY_COMPARE(a->radeReason(),
                     QStringLiteral("RADE could not start on slice A: slice B is already "
                                    "decoding RADE."));

        // B leaves RADE. A's decoder does not start by itself.
        b->setDspMode(DSPMode::USB);
        QCoreApplication::processEvents();
        QVERIFY(model->wdspEngine()->radeChannel(0) == nullptr);
        QVERIFY(b->radeReason().isEmpty());
        QCOMPARE(a->radeReason(), notDecoding);

        // What the reason says to do starts it, and clears the reason.
        a->setDspMode(DSPMode::USB);
        QCoreApplication::processEvents();
        QVERIFY(a->radeReason().isEmpty());
        a->setDspMode(DSPMode::RADE_U);
        QCoreApplication::processEvents();
        RadeChannel* const aRade = model->wdspEngine()->radeChannel(0);
        QVERIFY(aRade && aRade->isActive());
        QVERIFY2(a->radeReason().isEmpty(), qPrintable(a->radeReason()));
        app.stop();
    }
};

QTEST_MAIN(TestDaemonRadioRecovery)
#include "tst_daemon_radio_recovery.moc"
