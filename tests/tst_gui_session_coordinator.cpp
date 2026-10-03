// no-port-check: NereusSDR-original. R-R3-38 session replacement invariants.
// iPhone app Task 71 (R-IOS-02): an end the operator did not ask for is the
// Core retiring the token, now that a second window is admitted beside the
// first. J.J. Boyd (KG4VCF), 2026-09-25, AI-assisted via Anthropic Claude Code.
#include <QtTest/QtTest>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTemporaryDir>
#include <QUrl>
#include <QTcpServer>
#include <QWebSocketServer>
#include <QScopeGuard>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/SliceOwnership.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "core/MoxController.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/StationDevicesFacade.h"
#include "core/security/StationIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/settings/SettingsProxy.h"
#include "core/station/StationHandover.h"
#include "core/station/StationRadios.h"
#include "gui/GuiDesktopStationRuntime.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "gui/SetupDialog.h"
#include "gui/setup/RemoteStationPage.h"
#include "gui/RemoteConnectionController.h"
#include "gui/SpectrumWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/widgets/StatusToast.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "fakes/MainWindowTestSettings.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

namespace {
StationStartupSelection core(const QString& id, quint16 port = 4433)
{
    return {{QStringLiteral("ws://127.0.0.1:%1").arg(port), {}, {}, true}, id};
}

int toastsStartingWith(MainWindow* window, const QString& prefix)
{
    int count = 0;
    for (StatusToast* toast : window->findChildren<StatusToast*>()) {
        if (toast->message().startsWith(prefix)) { ++count; }
    }
    return count;
}

void dismissToasts(MainWindow* window)
{
    qDeleteAll(window->findChildren<StatusToast*>());
}
}

class TestGuiSessionCoordinator : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("station-switch-%1")
            .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        QVERIFY(!AppSettings::instance().remoteBackend());
        AppSettings::instance().clear();
        Test::markAudioFirstRunDone();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{5});
    }

    void cleanup()
    {
        QVERIFY(!AppSettings::instance().remoteBackend());
        RadioDiscovery::clearHoldOffForTest();
        QFile::remove(QFileInfo(AppSettings::instance().filePath()).absolutePath()
                      + QStringLiteral("/station.conf"));
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
    void configuredDeferredLocalBootstrapOwnsSlicesWithRunOff()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        auto& settings = AppSettings::instance();
        settings.setValue("DesktopCore/Run", false);
        StationServiceOptions service;
        service.profileDirectory = QFileInfo(settings.filePath()).absolutePath();
        service.homeDirectory = temp.filePath("home");
        service.inheritActiveProfile = false;
        service.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.configureDesktopStation(AppSettings::profileOverride(), true, service));
        QString error;
        QVERIFY2(sessions.replace({}, false, &error), qPrintable(error));
        auto* model = sessions.window()->radioModel();
        QVERIFY(!model->isConnected());
        QVERIFY(!sessions.desktopRuntime()->controller()->server());
        const auto generation = sessions.generation();
        for (int i = 0; i < 3; ++i) {
            const int id = model->addSlice("pan-0");
            QVERIFY(id >= 0);
            QCOMPARE(model->sliceOwnership()->mark(id).owner, SliceOwnership::stationDevice());
        }
        QCOMPARE(sessions.generation(), generation);
        QVERIFY(!model->isConnected());
        QVERIFY(!QFileInfo::exists(service.profileDirectory + "/station-identity.pem"));
    }

    void desktopReclaimKeepsCoreRadioChoice_data()
    {
        QTest::addColumn<QString>("saved");
        QTest::addColumn<QString>("configured");
        QTest::addColumn<bool>("showA");
        QTest::addColumn<bool>("showB");
        QTest::addColumn<bool>("attempt");
        QTest::addColumn<bool>("retryFailure");
        const QString b = QStringLiteral("AA:BB:CC:11:22:44");
        QTest::newRow("saved-choice-beats-legacy-auto") << b << QString() << true << true << true << false;
        QTest::newRow("config-choice-beats-legacy-auto") << QString() << b << true << true << true << false;
        QTest::newRow("missing-choice-waits") << b << QString() << true << false << false << false;
        QTest::newRow("ambiguous-waits") << QString() << QString() << true << true << false << false;
        QTest::newRow("one-visible-radio") << QString() << QString() << false << true << true << false;
        QTest::newRow("transient-failure-retries") << b << QString() << true << true << true << true;
    }

    void desktopReclaimKeepsCoreRadioChoice()
    {
        QFETCH(QString, saved);
        QFETCH(QString, configured);
        QFETCH(bool, showA);
        QFETCH(bool, showB);
        QFETCH(bool, attempt);
        QFETCH(bool, retryFailure);
        AppSettings& settings = AppSettings::instance();
        StationHandover ownership(AppSettings::profileOverride());
        QString error;
        QVERIFY2(ownership.acquire(0, &error), qPrintable(error));
        QTcpServer port;
        QVERIFY(port.listen(QHostAddress::LocalHost, 0));
        const quint16 selectedPort = port.serverPort();
        port.close();
        QFile config(QFileInfo(settings.filePath()).absolutePath() + QStringLiteral("/station.conf"));
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray bytes = QStringLiteral("remote_bind = 127.0.0.1\nremote_port = %1\n"
            "status_page = off\nrendezvous_servers =\nradio_mac = %2\n")
            .arg(selectedPort).arg(configured).toUtf8();
        QCOMPARE(config.write(bytes), bytes.size());
        config.close();
        RadioInfo a;
        a.address = QHostAddress::LocalHost;
        a.port = 9;
        a.boardType = HPSDRHW::HermesLite;
        a.protocol = ProtocolVersion::Protocol1;
        a.macAddress = QStringLiteral("AA:BB:CC:11:22:33");
        RadioInfo b = a;
        b.macAddress = QStringLiteral("AA:BB:CC:11:22:44");
        settings.saveRadio(a, false, true);
        settings.setLastConnected(a.macAddress);
        settings.setValue(QLatin1String(StationRadios::kChoiceKey), saved);
        settings.setValue(QStringLiteral("DesktopCore/Run"), true);
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.configureDesktopStation(AppSettings::profileOverride(), true));
        QVERIFY2(sessions.replace({}, false, &error), qPrintable(error));
        QVERIFY(sessions.desktopRuntime()->controller()->enabled());
        QCOMPARE(sessions.stationRadios()->target(), !saved.isEmpty() ? saved : configured);
        RadioModel* model = sessions.window()->radioModel();
        RadioDiscovery* discovery = model->discovery();
        if (showA) { discovery->injectLastSeenForTest(a.macAddress, a, 0); }
        if (showB) { discovery->injectLastSeenForTest(b.macAddress, b, 0); }
        QString attemptedMac;
        int attempts = 0;
        connect(model->wdspEngine(), &WdspEngine::initializedChanged, &sessions, [&](bool ready) {
            if (!ready) { return; }
            attemptedMac = model->currentRadioInfo().macAddress;
            ++attempts;
            // Cancel before opening audio/radio sockets. For the failure row,
            // suppress the manual-disconnect notification and inject the
            // transport failure boundary below. Its retry uses loopback only.
            if (retryFailure && attempts == 1) {
                const QSignalBlocker blocked(model);
                model->disconnectFromRadio();
            } else {
                model->disconnectFromRadio();
            }
        });
        sessions.window()->startInitialConnection();
        emit discovery->discoveryFinished();
        if (attempt) {
            QTRY_COMPARE(attemptedMac, b.macAddress);
            QCOMPARE(sessions.stationRadios()->target(), b.macAddress);
        } else {
            QTRY_VERIFY(!sessions.stationRadios()->waitingReason().isEmpty());
            QVERIFY(attemptedMac.isEmpty());
        }
        if (retryFailure) {
            model->onConnectionStateChangedForTest(ConnectionState::LinkLost);
            emit discovery->discoveryFinished();
            // The aborted first setup already initialized WDSP, so its ready
            // signal does not fire a second time. Observe the real retry's
            // loopback-only connection instead of counting initialization.
            QTRY_VERIFY(model->connection());
            QCOMPARE(model->currentRadioInfo().macAddress, b.macAddress);
            // TX safety fix round 3: the hosted retry is a recovery, not the
            // operator's disconnect, so the lost-link lock holds through it
            // into the rebuilt link; the operator's Disconnect lifts it.
            QVERIFY(model->isRadioLinkDown());
            QVERIFY(model->moxController()->isRadioLinkDown());
            // Radio > Disconnect, the way out of the lock, stays enabled.
            QAction* radioDisconnect = nullptr;
            for (QAction* action : sessions.window()->findChildren<QAction*>()) {
                if (action->text() == QStringLiteral("&Disconnect")) { radioDisconnect = action; }
            }
            QVERIFY(radioDisconnect != nullptr);
            QVERIFY(model->connectionState() != ConnectionState::Connected);
            QVERIFY(radioDisconnect->isEnabled());
            model->disconnectFromRadio();
            QVERIFY(!model->isRadioLinkDown());
            QVERIFY(!radioDisconnect->isEnabled());
            // A late failure or discovery completion must not undo Disconnect.
            model->onConnectionStateChangedForTest(ConnectionState::LinkLost);
            emit discovery->discoveryFinished();
            QCoreApplication::processEvents();
            QVERIFY(!model->connection());
        }
        QVERIFY(!model->connection());
        // Merely attempting a radio must not replace the last confirmed choice.
        QCOMPARE(sessions.stationRadios()->savedChoice(), saved);
    }

    void desktopHostRetiresBeforeModelWithoutStartingBackgroundOnReplacement()
    {
        AppSettings& settings = AppSettings::instance();
        StationHandover ownership(AppSettings::profileOverride());
        QString error;
        QVERIFY2(ownership.acquire(0, &error), qPrintable(error));
        QTcpServer port;
        QVERIFY(port.listen(QHostAddress::LocalHost, 0));
        const quint16 selectedPort = port.serverPort();
        port.close();
        QFile config(QFileInfo(settings.filePath()).absolutePath() + QStringLiteral("/station.conf"));
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray configBytes = QStringLiteral("remote_bind = 127.0.0.1\nremote_port = %1\n"
            "status_page = off\nrendezvous_servers =\n").arg(selectedPort).toUtf8();
        QCOMPARE(config.write(configBytes), configBytes.size());
        config.close();
        settings.setValue(QStringLiteral("DesktopCore/Run"), true);
        settings.setValue(QStringLiteral("DesktopCore/KeepRunning"), true);
        QTemporaryDir serviceHome;
        QVERIFY(serviceHome.isValid());
        StationServiceOptions options;
        options.homeDirectory = serviceHome.path();
        options.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.configureDesktopStation(AppSettings::profileOverride(),
                                                   ownership.ownsProfile(), options));
        QVERIFY2(sessions.replace({}, false, &error), qPrintable(error));
        auto* runtime = sessions.desktopRuntime();
        QVERIFY(runtime && runtime->controller()->enabled());
        QVERIFY(sessions.stationRadios());
        QVERIFY(!sessions.backgroundServiceOptions());
        SetupDialog* setup = nullptr;
        connect(sessions.window(), &MainWindow::setupDialogCreated, &sessions,
                [&setup](SetupDialog* dialog) { setup = dialog; });
        QVERIFY(QMetaObject::invokeMethod(sessions.window(), "createSetupDialog"));
        QVERIFY(setup);
        setup->selectPage(QStringLiteral("Remote Access"));
        auto* page = setup->findChild<RemoteStationPage*>();
        QVERIFY(page && page->state().available && page->state().runCore);
        setup->close();
        QPointer<GuiDesktopStationRuntime> oldRuntime(runtime);
        QPointer<StationHost> oldHost(runtime->controller()->host());
        QPointer<RadioModel> oldModel(sessions.window()->radioModel());
        bool hostGoneBeforeModel = false;
        connect(oldModel, &QObject::destroyed, &sessions, [&] {
            hostGoneBeforeModel = oldHost.isNull() && oldRuntime.isNull();
        });
        QVERIFY2(sessions.replace(core(QStringLiteral("other")), false, &error), qPrintable(error));
        QVERIFY(oldModel.isNull());
        QVERIFY(hostGoneBeforeModel);
        QVERIFY(!sessions.desktopRuntime());
        QVERIFY(!sessions.stationRadios());
        QVERIFY(!sessions.backgroundServiceOptions());
        QVERIFY(ownership.ownsProfile());
        QVERIFY(port.listen(QHostAddress::LocalHost, selectedPort));
        sessions.shutdown();
    }

    void checkedQuitKeepsBackgroundIntentAndProfileUntilModelRetirement()
    {
        AppSettings& settings = AppSettings::instance();
        StationHandover ownership(AppSettings::profileOverride());
        QString error;
        QVERIFY2(ownership.acquire(0, &error), qPrintable(error));
        QTcpServer port;
        QVERIFY(port.listen(QHostAddress::LocalHost, 0));
        const quint16 selectedPort = port.serverPort();
        port.close();
        QFile config(QFileInfo(settings.filePath()).absolutePath() + QStringLiteral("/station.conf"));
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray configBytes = QStringLiteral("remote_bind = 127.0.0.1\nremote_port = %1\n"
            "status_page = off\nrendezvous_servers =\n").arg(selectedPort).toUtf8();
        QCOMPARE(config.write(configBytes), configBytes.size());
        config.close();
        settings.setValue(QStringLiteral("DesktopCore/Run"), true);
        settings.setValue(QStringLiteral("DesktopCore/KeepRunning"), true);
        QTemporaryDir serviceHome;
        QVERIFY(serviceHome.isValid());
        StationServiceOptions options;
        options.homeDirectory = serviceHome.path();
        options.runner = [](const QString&, const QStringList&) {
            return StationServiceCommandResult{0, {}};
        };
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.configureDesktopStation(AppSettings::profileOverride(),
                                                   ownership.ownsProfile(), options));
        QVERIFY2(sessions.replace({}, false, &error), qPrintable(error));
        QPointer<RadioModel> model(sessions.window()->radioModel());
        // A checked close must preserve the live window/profile when the
        // destination cannot be replaced, and a repaired retry keeps Keep on.
        const QString backup = settings.filePath() + QStringLiteral(".handover-test-original");
        QVERIFY(QFile::rename(settings.filePath(), backup));
        const auto restoreFile = qScopeGuard([&] {
            QDir().rmdir(settings.filePath());
            if (QFile::exists(backup)) { QFile::rename(backup, settings.filePath()); }
        });
        QVERIFY(QDir().mkdir(settings.filePath()));
        QSignalSpy failed(&sessions, &GuiSessionCoordinator::stationOperationFailed);
        QCloseEvent close;
        QCoreApplication::sendEvent(sessions.window(), &close);
        QVERIFY(!close.isAccepted());
        QVERIFY(!failed.isEmpty());
        QVERIFY(model && sessions.window());
        QVERIFY(ownership.ownsProfile());
        QVERIFY(!sessions.backgroundServiceOptions());
        QVERIFY(QDir().rmdir(settings.filePath()));
        QVERIFY(QFile::rename(backup, settings.filePath()));
        QVERIFY2(sessions.prepareApplicationQuit(&error), qPrintable(error));
        QVERIFY(sessions.backgroundServiceOptions());
        QVERIFY(!sessions.desktopRuntime()->controller()->host());
        QVERIFY(model && ownership.ownsProfile());
        QVERIFY(sessions.prepareApplicationQuit(&error));
        sessions.shutdown();
        QVERIFY(model.isNull());
        QVERIFY(sessions.backgroundServiceOptions());
        QCOMPARE(sessions.backgroundServiceOptions()->profile, AppSettings::profileOverride());
        QVERIFY(!sessions.backgroundServiceOptions()->inheritActiveProfile);
        StationHandover competitor(AppSettings::profileOverride());
        QVERIFY(!competitor.acquire(0, &error));
        QVERIFY(settings.save(&error));
        ownership.release();
        QVERIFY(competitor.acquire(0, &error));
    }

    void replacesWholeSessionAndRetiresQueuedPickerAndProxy()
    {
        AppSettings::instance().setValue(QStringLiteral("DisplayFftSize"), QStringLiteral("2048"));
        GuiSessionCoordinator sessions;
        QSignalSpy pickerRequests(&sessions, &GuiSessionCoordinator::connectionsRequested);
        QVERIFY(sessions.replace({}, false));
        QPointer<MainWindow> local = sessions.window();
        QVERIFY(local->radioModel()->ownsLocalDsp());
        QVERIFY(!AppSettings::instance().remoteBackend());
        QVERIFY(sessions.replace(core(QStringLiteral("a")), false));
        QVERIFY(local.isNull());
        QPointer<MainWindow> first = sessions.window();
        QPointer<RadioModel> firstModel = first->radioModel();
        QPointer<StationClient> firstClient = first->findChild<StationClient*>();
        auto* firstProxy = dynamic_cast<SettingsProxy*>(AppSettings::instance().remoteBackend());
        QVERIFY(firstProxy);
        QVERIFY(!firstProxy->ready());
        QVERIFY(firstClient);
        QVERIFY(!firstClient->isConnectionActive());
        QVERIFY(!firstModel->ownsLocalDsp());
        QVERIFY(!firstModel->connection());
        firstProxy->applySnapshot({{QStringLiteral("DisplayFftSize"), QStringLiteral("8192")}});
        QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayFftSize")).toString(),
                 QStringLiteral("8192"));
        QCOMPARE(firstModel->addSliceWithStationId(42, QStringLiteral("pan-0")), 42);
        QPointer<SliceModel> oldSlice = firstModel->sliceById(42);
        QVERIFY(oldSlice);

        bool backendPresentDuringOldWindowDestruction = false;
        bool detachedBeforeProxyDestruction = false;
        connect(first, &QObject::destroyed, &sessions, [&] {
            backendPresentDuringOldWindowDestruction =
                AppSettings::instance().remoteBackend() == firstProxy;
        });
        connect(firstProxy, &QObject::destroyed, &sessions, [&] {
            detachedBeforeProxyDestruction = AppSettings::instance().remoteBackend() == nullptr;
        });
        // The forwarded call is queued in the coordinator, where disconnect
        // alone cannot retract it. The generation guard must reject it.
        QVERIFY(QMetaObject::invokeMethod(first, "connectionsRequested", Qt::DirectConnection));
        const quint64 oldGeneration = sessions.generation();
        QVERIFY(sessions.replace(core(QStringLiteral("b")), false));
        QVERIFY(sessions.generation() > oldGeneration);
        QVERIFY(first.isNull() && firstModel.isNull() && firstClient.isNull() && oldSlice.isNull());
        QVERIFY(backendPresentDuringOldWindowDestruction);
        QVERIFY(detachedBeforeProxyDestruction);
        QCoreApplication::processEvents();
        QCOMPARE(pickerRequests.count(), 0);
        QVERIFY(sessions.window()->radioModel()->slices().isEmpty());
        auto* secondProxy = dynamic_cast<SettingsProxy*>(AppSettings::instance().remoteBackend());
        QVERIFY(secondProxy && !secondProxy->ready());
        QVERIFY(AppSettings::instance().value(QStringLiteral("DisplayFftSize")).toString()
                != QStringLiteral("8192"));

        QPointer<MainWindow> second = sessions.window();
        QPointer<SettingsProxy> secondProxyLifetime = secondProxy;
        QVERIFY(sessions.replace({}, false));
        QVERIFY(second.isNull() && secondProxyLifetime.isNull());
        QVERIFY(!AppSettings::instance().remoteBackend());
        QVERIFY(sessions.window()->radioModel()->ownsLocalDsp());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayFftSize")).toString(),
                 QStringLiteral("2048"));
        sessions.shutdown();
        QVERIFY(!sessions.window());
    }

    void constructedWindowBindsNotchCreateOnce()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        SpectrumWidget* sw = window->activeSpectrumWidget();
        QVERIFY(sw);
        QVERIFY(QMetaObject::invokeMethod(window, "wirePanNotchHandlers", Qt::DirectConnection));
        QVERIFY(QMetaObject::invokeMethod(window, "wirePanNotchHandlers", Qt::DirectConnection));
        QSignalSpy added(window->radioModel()->notchModel(), &NotchModel::notchAdded);
        QSignalSpy rejected(window->radioModel()->notchModel(), &NotchModel::notchAddRejected);
        sw->notchCreateRequested(14'200'000.0, false);
        QCOMPARE(added.count(), 1);
        QCOMPARE(rejected.count(), 0);
    }

    void initializationCannotRetireItsOwnModel()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        QPointer<MainWindow> original = sessions.window();
        RadioModel* model = original->radioModel();
        model->wdspEngine()->setSynchronousInitForTest(true);
        bool observed = false;
        connect(model->wdspEngine(), &WdspEngine::initializedChanged, &sessions, [&](bool ready) {
            if (!ready) { return; }
            observed = true;
            QVERIFY(model->localConnectionSetupActive());
            QString error;
            QVERIFY(!sessions.replace(core(QStringLiteral("other")), false, &error));
            QVERIFY(!error.isEmpty());
            QCOMPARE(sessions.window(), original.data());
            // Before the model's initialization callback: do not open audio
            // devices or create a radio connection in this ownership test.
            model->disconnectFromRadio();
        });
        RadioInfo info;
        info.address = QHostAddress::LocalHost;
        info.port = 9;
        info.boardType = HPSDRHW::HermesLite;
        info.protocol = ProtocolVersion::Protocol1;
        info.macAddress = QStringLiteral("AA:BB:CC:11:22:33");
        model->connectToRadio(info);
        QVERIFY(observed);
        QVERIFY(!model->localConnectionSetupActive());
        QVERIFY(!model->connection());
        QVERIFY(sessions.replace(core(QStringLiteral("other")), false));
        QVERIFY(original.isNull());
    }

    void invalidTargetAndTransmitStateLeaveCurrentSessionUntouched()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace(core(QStringLiteral("current")), false));
        MainWindow* original = sessions.window();
        ISettingsBackend* originalBackend = AppSettings::instance().remoteBackend();
        const quint64 generation = sessions.generation();
        auto invalid = core(QStringLiteral("invalid"));
        invalid.connection.url = QStringLiteral("https://secret@invalid");
        QString error;
        QVERIFY(!sessions.replace(invalid, true, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!error.contains(QStringLiteral("secret")));
        QCOMPARE(sessions.window(), original);
        QCOMPARE(AppSettings::instance().remoteBackend(), originalBackend);
        QCOMPARE(sessions.generation(), generation);

        // Mirror observations only: no attached radio, PTT request or RF.
        original->radioModel()->transmitModel().setMox(true);
        QVERIFY(!sessions.replace({}, false, &error));
        QCOMPARE(sessions.window(), original);
        original->radioModel()->transmitModel().setMox(false);
        original->radioModel()->transmitModel().setTune(true);
        QVERIFY(!sessions.replace({}, false, &error));
        QCOMPARE(sessions.window(), original);
        original->radioModel()->transmitModel().setTune(false);
        QVERIFY(sessions.replace({}, false));
    }

    void radioSwitchWaitsForTransmitToReceiveHandover()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        RadioModel* model = sessions.window()->radioModel();
        QVERIFY(!model->connection()); // logical keying only, no attached radio
        MoxController* mox = model->moxController();
        mox->setMoxCheck({});
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox->setMox(true);
        QTRY_COMPARE(mox->state(), MoxState::Tx);
        bool checkedFlush = false;
        connect(mox, &MoxController::stateChanged, &sessions, [&](MoxState state) {
            if (state != MoxState::TxToRxFlush) { return; }
            checkedFlush = true;
            QVERIFY(!model->mox() && !model->transmitModel().isMox());
            QVERIFY(model->stationOnAirRefusal(nullptr));
            QString error;
            QVERIFY(!sessions.canReplace({}, &error));
            QVERIFY(!error.isEmpty());
        });
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QVERIFY(checkedFlush);
        QVERIFY(sessions.canReplace({}));
    }

    void cancelsRetryAndLateOldCoreStateOnSwitch()
    {
        QTemporaryDir firstDir;
        QTemporaryDir secondDir;
        AppSettings firstSettings(firstDir.filePath(QStringLiteral("station.settings")));
        AppSettings secondSettings(secondDir.filePath(QStringLiteral("station.settings")));
        firstSettings.setValue(QStringLiteral("DisplayFftSize"), QStringLiteral("8192"));
        secondSettings.setValue(QStringLiteral("DisplayFftSize"), QStringLiteral("4096"));
        RadioModel firstStation;
        RadioModel secondStation;
        StationServer firstServer(&firstStation, firstSettings, NereusSDR::Test::seedUpgradedCoreToken(firstDir.path()));
        StationServer secondServer(&secondStation, secondSettings, NereusSDR::Test::seedUpgradedCoreToken(secondDir.path()));
        QWebSocketServer firstListener(QStringLiteral("A"), QWebSocketServer::NonSecureMode);
        QWebSocketServer secondListener(QStringLiteral("B"), QWebSocketServer::NonSecureMode);
        QVERIFY(firstListener.listen(QHostAddress::LocalHost, 0));
        QVERIFY(secondListener.listen(QHostAddress::LocalHost, 0));
        connect(&firstListener, &QWebSocketServer::newConnection, &firstServer, [&] {
            firstServer.acceptTransport(new WebSocketTransport(firstListener.nextPendingConnection(),
                StationServer::kMaxIncomingMessageBytes));
        });
        connect(&secondListener, &QWebSocketServer::newConnection, &secondServer, [&] {
            secondServer.acceptTransport(new WebSocketTransport(secondListener.nextPendingConnection(),
                StationServer::kMaxIncomingMessageBytes));
        });
        auto a = core(QStringLiteral("a"), firstListener.serverPort());
        a.connection.token = firstServer.token();
        auto b = core(QStringLiteral("b"), secondListener.serverPort());
        b.connection.token = secondServer.token();
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace(a, true));
        QPointer<StationClient> firstClient = sessions.window()->findChild<StationClient*>();
        QVERIFY(firstClient);
        QTRY_VERIFY(firstClient->isHandshakeComplete());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayFftSize")).toString(),
                 QStringLiteral("8192"));
        firstClient->disconnectFromStation(QStringLiteral("test link failure"), true);
        QVERIFY(firstClient->isReconnectPending());
        QPointer<StatusToast> pendingNotice = sessions.window()->findChild<StatusToast*>();
        QVERIFY(pendingNotice); // The teardown regression requires a live notice.
        // Queue an old-window picker event along with the pending retry.
        QSignalSpy picker(&sessions, &GuiSessionCoordinator::connectionsRequested);
        QVERIFY(QMetaObject::invokeMethod(sessions.window(), "connectionsRequested", Qt::DirectConnection));
        QVERIFY(sessions.replace(b, true));
        QVERIFY(firstClient.isNull());
        QVERIFY(pendingNotice.isNull());
        StationClient* secondClient = sessions.window()->findChild<StationClient*>();
        QVERIFY(secondClient);
        QTRY_VERIFY(secondClient->isHandshakeComplete());
        QCOMPARE(picker.count(), 0);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayFftSize")).toString(),
                 QStringLiteral("4096"));
        QVERIFY(!secondClient->isReconnectPending());
        QVERIFY(!sessions.window()->radioModel()->isConnected()); // Core online, radio offline.
        sessions.shutdown();
    }

    // R-R3-17: a redial that keeps failing reports the same reason at every
    // backoff step, up to once a minute. The reason stays on screen in the
    // Connections window, Core panel and title bar, so the toasts announce
    // each distinct reason once. Toasts are dismissed between steps so a
    // repeat is counted as a new toast rather than merged into a live one.
    void linkLossToastsOncePerDistinctReason()
    {
        QTemporaryDir stationDir;
        AppSettings stationSettings(stationDir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(stationDir.path()));
        QWebSocketServer listener(QStringLiteral("A"), QWebSocketServer::NonSecureMode);
        QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
        connect(&listener, &QWebSocketServer::newConnection, &server, [&] {
            server.acceptTransport(new WebSocketTransport(listener.nextPendingConnection(),
                StationServer::kMaxIncomingMessageBytes));
        });
        auto a = core(QStringLiteral("a"), listener.serverPort());
        a.connection.token = server.token();
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace(a, true));
        MainWindow* window = sessions.window();
        StationClient* client = window->findChild<StationClient*>();
        QVERIFY(client);
        QTRY_VERIFY(client->isHandshakeComplete());
        const QString lost = QStringLiteral("Link to the Core lost: ");
        const QString retry = QStringLiteral("Reconnecting to the Core");
        const auto failure = [&](const QString& reason, int attempt) {
            dismissToasts(window);
            emit client->sessionEnded(reason);
            emit client->reconnectScheduled(attempt, 1000 * attempt);
        };

        failure(QStringLiteral("Remote host closed"), 1);
        QCOMPARE(toastsStartingWith(window, lost), 1);
        QCOMPARE(toastsStartingWith(window, retry), 1);
        // The same reason again, as each backoff step reports it: silent.
        failure(QStringLiteral("Remote host closed"), 2);
        QCOMPARE(toastsStartingWith(window, lost), 0);
        QCOMPARE(toastsStartingWith(window, retry), 0);
        failure(QStringLiteral("Remote host closed"), 3);
        QCOMPARE(toastsStartingWith(window, lost), 0);
        QCOMPARE(toastsStartingWith(window, retry), 0);
        // A different reason is news.
        failure(QStringLiteral("Connection refused"), 4);
        QCOMPARE(toastsStartingWith(window, lost), 1);
        QCOMPARE(toastsStartingWith(window, retry), 1);

        // The real path with the remembered reason stays silent too, and
        // its retry reaches the live Core, whose handshake clears the memory.
        dismissToasts(window);
        client->setReconnectBackoffUnitMs(50);
        client->disconnectFromStation(QStringLiteral("Connection refused"), true);
        QVERIFY(client->isReconnectPending());
        QCOMPARE(toastsStartingWith(window, lost), 0);
        QCOMPARE(toastsStartingWith(window, retry), 0);
        QTRY_VERIFY(client->isHandshakeComplete());
        failure(QStringLiteral("Connection refused"), 1);
        QCOMPARE(toastsStartingWith(window, lost), 1);
        QCOMPARE(toastsStartingWith(window, retry), 1);

        // An operator disconnect clears it as well, without a toast of its own.
        dismissToasts(window);
        QVERIFY(QMetaObject::invokeMethod(window, "disconnectFromStation", Qt::DirectConnection));
        QVERIFY(!client->isConnectionActive());
        QCOMPARE(toastsStartingWith(window, lost), 0);
        failure(QStringLiteral("Connection refused"), 1);
        QCOMPARE(toastsStartingWith(window, lost), 1);
        QCOMPARE(toastsStartingWith(window, retry), 1);

        // R-R3-17/21: a reason in the Core's own terms is toasted in user
        // words; the comparison above still ran on the raw text.
        failure(QStringLiteral("heartbeat timeout"), 1);
        QCOMPARE(toastsStartingWith(window,
                     lost + QStringLiteral("The connection to the Core went quiet, so it was closed.")),
                 1);
        const QList<StatusToast*> toasts = window->findChildren<StatusToast*>();
        QVERIFY2(toasts.size() >= 2, qPrintable(QString::number(toasts.size())));
        for (StatusToast* toast : toasts) {
            QVERIFY2(OperatorWording::isPlain(toast->message()), qPrintable(toast->message()));
        }
        sessions.shutdown();
    }

    // R-R3-17 fix wave, Important 1: the Connections window's Disconnect /
    // "Cancel retry" and the Core panel's Disconnect reach
    // RemoteConnectionController::disconnectFromStation() directly, not
    // MainWindow::disconnectFromStation(). With a retry pending there is no
    // transport, so sessionEnded never reaches the toast handler. Cancelling
    // the retry must still forget the remembered reason, so the next
    // explicit Connect that fails the same way is announced.
    void cancellingRetryThroughControllerForgetsToastedReason()
    {
        QTemporaryDir stationDir;
        AppSettings stationSettings(stationDir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(stationDir.path()));
        QWebSocketServer listener(QStringLiteral("A"), QWebSocketServer::NonSecureMode);
        QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
        connect(&listener, &QWebSocketServer::newConnection, &server, [&] {
            server.acceptTransport(new WebSocketTransport(listener.nextPendingConnection(),
                StationServer::kMaxIncomingMessageBytes));
        });
        auto a = core(QStringLiteral("a"), listener.serverPort());
        a.connection.token = server.token();
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace(a, true));
        MainWindow* window = sessions.window();
        StationClient* client = window->findChild<StationClient*>();
        auto* controller = window->findChild<RemoteConnectionController*>();
        QVERIFY(client);
        QVERIFY(controller);
        QTRY_VERIFY(client->isHandshakeComplete());
        const QString lost = QStringLiteral("Link to the Core lost: ");
        const QString retry = QStringLiteral("Reconnecting to the Core");

        // The link drops and a retry is pending: announced once.
        dismissToasts(window);
        client->setReconnectBackoffUnitMs(60000);
        client->disconnectFromStation(QStringLiteral("Connection refused"), true);
        QVERIFY(client->isReconnectPending());
        QCOMPARE(toastsStartingWith(window, lost), 1);
        QCOMPARE(toastsStartingWith(window, retry), 1);

        // The operator cancels the retry from the Connections window.
        dismissToasts(window);
        controller->disconnectFromStation();
        QVERIFY(!client->isConnectionActive());
        QCOMPARE(toastsStartingWith(window, lost), 0);

        // Their next Connect fails for the same reason: that is news again.
        emit client->sessionEnded(QStringLiteral("Connection refused"));
        emit client->reconnectScheduled(1, 1000);
        QCOMPARE(toastsStartingWith(window, lost), 1);
        QCOMPARE(toastsStartingWith(window, retry), 1);
        sessions.shutdown();
    }

    // R-R3-16 / R-R3-38: the picker opens Connections only after the
    // operator's own Disconnect. A retry after link loss and a disconnect
    // the app did not ask for (here another window taking over the Core's
    // one session) ask for nothing; the operator's Disconnect, even while
    // a retry waits, asks exactly once.
    void pickerOpensConnectionsOnlyForOperatorDisconnect()
    {
        QTemporaryDir stationDir;
        AppSettings stationSettings(stationDir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, stationSettings, NereusSDR::Test::seedUpgradedCoreToken(stationDir.path()));
        server.setHeartbeatIntervalMs(0);
        QWebSocketServer listener(QStringLiteral("A"), QWebSocketServer::NonSecureMode);
        QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
        connect(&listener, &QWebSocketServer::newConnection, &server, [&] {
            server.acceptTransport(new WebSocketTransport(listener.nextPendingConnection(),
                StationServer::kMaxIncomingMessageBytes));
        });
        // The other window's client, built before the session installs its
        // settings proxy so its model writes nothing through it.
        RadioModel otherRemote(RadioModel::Role::Remote);
        SettingsProxy otherProxy;
        StationClient other(&otherRemote, &otherProxy);
        auto a = core(QStringLiteral("a"), listener.serverPort());
        a.connection.token = server.token();
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace(a, true));
        MainWindow* window = sessions.window();
        StationClient* client = window->findChild<StationClient*>();
        auto* controller = window->findChild<RemoteConnectionController*>();
        QVERIFY(client);
        QVERIFY(controller);
        QTRY_VERIFY(client->isHandshakeComplete());
        QSignalSpy picker(&sessions, &GuiSessionCoordinator::connectionsRequested);

        // Link loss: the window retries and asks for nothing.
        client->setReconnectBackoffUnitMs(60000);
        client->disconnectFromStation(QStringLiteral("Connection refused"), true);
        QVERIFY(client->isReconnectPending());
        QTest::qWait(100);
        QCOMPARE(picker.count(), 0);

        // The operator cancels the retry: one request.
        controller->disconnectFromStation();
        QTRY_COMPARE(picker.count(), 1);
        QTest::qWait(100);
        QCOMPARE(picker.count(), 1);

        // The Core ends this session without the operator here asking
        // (iPhone app Task 71: another window signing in is now admitted
        // beside this one, so the end is the Core retiring the token this
        // window signed in with): nothing opens.
        controller->connectToStation();
        QTRY_VERIFY(client->isHandshakeComplete());
        picker.clear();
        other.connectToStation(QUrl(a.connection.url), a.connection.token, {}, true);
        QTRY_VERIFY(other.isHandshakeComplete());
        QVERIFY(client->isConnectionActive());
        PairedDevice paired;
        {
            QTemporaryDir keyDir;
            const StationIdentity key = StationIdentity::loadOrCreate(keyDir.path());
            paired.id = key.fingerprint();
            paired.publicKeySpki = key.publicKeySpki();
        }
        paired.name = QStringLiteral("Shack iPhone");
        paired.kind = QStringLiteral("phone");
        QVERIFY(server.deviceStore()->add(paired));
        QVERIFY(server.devicesFacade()->retireToken().accepted);
        QTRY_VERIFY(!client->isConnectionActive());
        QVERIFY(!client->isReconnectPending());
        QTest::qWait(100);
        QCOMPARE(picker.count(), 0);
        other.disconnectFromStation(QStringLiteral("operator disconnect"));
        sessions.shutdown();
    }

    // Fix wave, Important 2 (R-R3-13): a local window shows no receive
    // reading while its radio link is not up. On LinkLost the RX channels
    // stay alive, so the poller cannot tell from the channel alone.
    void localWindowMetersFollowRadioLink()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        RadioModel* model = window->radioModel();
        auto* poller = window->findChild<MeterPoller*>();
        QVERIFY(poller);
        emit model->connectionStateChanged(ConnectionState::Connected);
        QVERIFY(poller->localRxReadingAvailable());
        emit model->connectionStateChanged(ConnectionState::LinkLost);
        QVERIFY(!poller->localRxReadingAvailable());
        emit model->connectionStateChanged(ConnectionState::Connected);
        QVERIFY(poller->localRxReadingAvailable());
        emit model->connectionStateChanged(ConnectionState::Disconnected);
        QVERIFY(!poller->localRxReadingAvailable());
        sessions.shutdown();
    }

    void replacementKeepsEventLoopAliveAndNormalCloseQuits()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        const bool originalQuit = QApplication::quitOnLastWindowClosed();
        bool replacementSucceeded = false;
        bool nextEventRan = false;
        QTimer::singleShot(0, &sessions, [&] {
            replacementSucceeded = sessions.replace(core(QStringLiteral("other")), false);
            QTimer::singleShot(0, &sessions, [&] {
                nextEventRan = true;
                QCoreApplication::quit();
            });
        });
        QCoreApplication::exec();
        QVERIFY(replacementSucceeded && nextEventRan);
        QCOMPARE(QApplication::quitOnLastWindowClosed(), originalQuit);
        sessions.shutdown();

        QVERIFY(sessions.replace({}, false));
        bool ordinaryCloseRan = false;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        bool timedOut = false;
        connect(&watchdog, &QTimer::timeout, &sessions, [&] {
            timedOut = true;
            QCoreApplication::quit();
        });
        watchdog.start(5000);
        QTimer::singleShot(0, &sessions, [&] {
            ordinaryCloseRan = sessions.window()->close();
        });
        QCoreApplication::exec();
        QVERIFY(ordinaryCloseRan);
        QVERIFY(!timedOut);
    }
};

QTEST_MAIN(TestGuiSessionCoordinator)
#include "tst_gui_session_coordinator.moc"
