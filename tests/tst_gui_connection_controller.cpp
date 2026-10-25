// =================================================================
// tests/tst_gui_connection_controller.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-38 controller presentation and
// session-switch acceptance coverage.
// 2026-10-17: run as three ctest entries (TestFunctionGroups). J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-22: ports a test hands to a later listener come from
// unusedLoopbackPort() (UnusedLoopbackPort.h), below the kernel's ephemeral
// range, so a parallel test cannot take one between the pick and the listen.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "UnusedLoopbackPort.h"
#include <QtTest/QtTest>

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QTcpServer>
#include <QScopeGuard>
#include <QHostAddress>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebSocketServer>
#include <QWebSocket>

#include <chrono>

#include "OperatorWording.h"
#include "TestFunctionGroups.h"
#include "core/AppSettings.h"
#include "core/station/StationHandover.h"
#include "core/security/PairingWindow.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "core/session/PathRacer.h"
#include "core/session/StationClient.h"
#include "core/session/RemoteDevicesState.h"
#include "core/session/StationServer.h"
#include "gui/ConnectionSelector.h"
#include "core/session/StationRendezvous.h"
#include "gui/CoreTargetEditor.h"
#include "gui/CoreTargetStore.h"
#include "gui/GuiConnectionController.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "gui/RemoteConnectionController.h"
#include "models/RadioModel.h"
#include "fakes/MainWindowTestSettings.h"
#include "fakes/UpgradedCoreToken.h"
#include "RendezvousTestHarness.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/P1FakeRadio.h"
#include "fakes/NoCaptureHelper.h"
#include "core/session/PathRacer.h"
#include "core/session/StationDevicesFacade.h"

using namespace NereusSDR;

namespace {

SavedCoreTarget savedTarget(const QString& id, const QString& label, quint16 port,
                            const QString& token)
{
    SavedCoreTarget target;
    target.id = id;
    target.label = label;
    target.connection.url = QStringLiteral("ws://127.0.0.1:%1").arg(port);
    target.connection.token = token;
    target.connection.allowUnpinned = true;
    return target;
}

bool installTargets(const QList<SavedCoreTarget>& targets, const QString& selectedId)
{
    CoreTargetStore store(AppSettings::instance());
    if (!store.load()) {
        return false;
    }
    for (const SavedCoreTarget& target : targets) {
        if (!store.upsert(target)) {
            return false;
        }
    }
    return store.select(selectedId);
}

QPushButton* button(ConnectionSelector* selector, const QString& objectName)
{
    return selector->findChild<QPushButton*>(objectName);
}

QAction* managedConnectAction(MainWindow* window)
{
    for (QAction* action : window->findChildren<QAction*>()) {
        if (action->text() == QStringLiteral("&Connect")
            || action->shortcut() == QKeySequence(QStringLiteral("Ctrl+K"))) {
            return action;
        }
    }
    return nullptr;
}

struct LoopbackCores final {
    QTemporaryDir firstDirectory;
    QTemporaryDir secondDirectory;
    AppSettings firstSettings{firstDirectory.filePath(QStringLiteral("station.settings"))};
    AppSettings secondSettings{secondDirectory.filePath(QStringLiteral("station.settings"))};
    // Construct the two station models before any GUI session installs its
    // remote settings proxy. They are real StationServer models, not devices.
    RadioModel firstStation;
    RadioModel secondStation;
    StationServer firstServer{&firstStation, firstSettings,
                              NereusSDR::Test::seedUpgradedCoreToken(firstDirectory.path())};
    StationServer secondServer{&secondStation, secondSettings,
                               NereusSDR::Test::seedUpgradedCoreToken(secondDirectory.path())};
    QWebSocketServer firstListener{QStringLiteral("controller-A"), QWebSocketServer::NonSecureMode};
    QWebSocketServer secondListener{QStringLiteral("controller-B"), QWebSocketServer::NonSecureMode};

    bool start()
    {
        if (!firstDirectory.isValid() || !secondDirectory.isValid()
            || !firstListener.listen(QHostAddress::LocalHost, 0)
            || !secondListener.listen(QHostAddress::LocalHost, 0)) {
            return false;
        }
        firstServer.setHeartbeatIntervalMs(0);
        secondServer.setHeartbeatIntervalMs(0);
        QObject::connect(&firstListener, &QWebSocketServer::newConnection, &firstServer, [this] {
            firstServer.acceptTransport(new WebSocketTransport(firstListener.nextPendingConnection(),
                StationServer::kMaxIncomingMessageBytes));
        });
        QObject::connect(&secondListener, &QWebSocketServer::newConnection, &secondServer, [this] {
            secondServer.acceptTransport(new WebSocketTransport(secondListener.nextPendingConnection(),
                StationServer::kMaxIncomingMessageBytes));
        });
        return true;
    }

    SavedCoreTarget firstTarget() const
    {
        return savedTarget(QStringLiteral("a"), QStringLiteral("Core A"),
                           firstListener.serverPort(), firstServer.token());
    }

    SavedCoreTarget secondTarget() const
    {
        return savedTarget(QStringLiteral("b"), QStringLiteral("Core B"),
                           secondListener.serverPort(), secondServer.token());
    }
};

} // namespace

class TestGuiConnectionController final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void managedConnectActionOpensSelectorForEmptyLocalProfile();
    void selectingAndCancellingEditLeaveLiveAAndDoNotDialB();
    void explicitConnectToSavedBReplacesWholeLiveA();
    void localRadioSuccessDismissesOnlyItsRequestedConnection();
    void automaticLocalStartupDoesNotOpenSelector_data();
    void automaticLocalStartupDoesNotOpenSelector();
    void staleAttemptCannotDismissSelector_data();
    void staleAttemptCannotDismissSelector();
    void pendingAndTransportFailureKeepSelectorOpen();
    void closingAndReopeningDuringPendingConnectionKeepsSelectorOpen();
    void disconnectCancelsRetryWithoutUsingHighlightedB();
    void connectionsDisconnectReopensConnectionsOnceWithoutDialling();
    void aManualReconnectReadsTheStoresLastAddresses();
    void savedRowShowsLastAuthenticatedAddress();
    void verifiedPathWinnerIsKeptWithoutAnotherSnapshot();
    void persistentLocalChoiceReturnsToEmbeddedCoreWithoutRadioAutoconnect();
    void savedCoreEditsDoNotChangeCurrentTupleBeforeConnect();
    void corruptStartupDocumentShowsIdleLocalAndNotice();
    void codeOnlyDialogPairsThroughMailboxAndOpensRemoteCore();
    void acceptingCodeDialogAfterShutdownDoesNotStartPairing();
};

void TestGuiConnectionController::initTestCase()
{
    AppSettings::setProfileOverride(QStringLiteral("gui-connection-controller-%1")
        .arg(QCoreApplication::applicationPid()));
}

void TestGuiConnectionController::init()
{
    QVERIFY(!AppSettings::instance().remoteBackend());
    AppSettings::instance().clear();
    Test::markAudioFirstRunDone();
    QVERIFY(AppSettings::instance().save());
    RadioDiscovery::clearHoldOffForTest();
    RadioDiscovery discovery;
    discovery.holdOffScans(std::chrono::minutes{5});
}

void TestGuiConnectionController::cleanup()
{
    QVERIFY(!AppSettings::instance().remoteBackend());
    RadioDiscovery::clearHoldOffForTest();
}

void TestGuiConnectionController::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TestGuiConnectionController::managedConnectActionOpensSelectorForEmptyLocalProfile()
{
    GuiConnectionController controller;
    controller.start({});
    MainWindow* window = controller.sessions()->window();
    QVERIFY(window != nullptr);
    QVERIFY(window->radioModel()->ownsLocalDsp());
    QVERIFY(window->findChild<StationClient*>() == nullptr);
    QAction* connectAction = managedConnectAction(window);
    QVERIFY(connectAction != nullptr);
    QVERIFY(connectAction->isEnabled());

    connectAction->trigger();
    QTRY_VERIFY(controller.selector()->isVisible());
    QCOMPARE(controller.sessions()->window(), window);
    controller.selector()->setSelectedKey(QStringLiteral("local"));
    auto* details = button(controller.selector(), QStringLiteral("connectionSelectorDetails"));
    QVERIFY(details && details->isEnabled());
    details->click();
    auto* notice = controller.selector()->findChild<QLabel*>(QStringLiteral("connectionSelectorNotice"));
    QVERIFY(notice && notice->text().contains(QStringLiteral("This computer runs its own Core")));
    QVERIFY(OperatorWording::isPlain(notice->text()));
    QVERIFY(window->radioModel()->connectionState() == ConnectionState::Disconnected);
    controller.shutdown();
}

void TestGuiConnectionController::selectingAndCancellingEditLeaveLiveAAndDoNotDialB()
{
    LoopbackCores cores;
    QVERIFY(cores.start());
    QVERIFY(installTargets({cores.firstTarget(), cores.secondTarget()}, QStringLiteral("a")));

    GuiConnectionController controller;
    controller.start({});
    QTRY_VERIFY(cores.firstServer.hasAuthenticatedSession());
    MainWindow* firstWindow = controller.sessions()->window();
    QPointer<MainWindow> firstWindowLifetime = firstWindow;
    QPointer<StationClient> firstClient = firstWindow->findChild<StationClient*>();
    QVERIFY(firstClient);
    {
        QString handshakeWhy;  // QTRY_VERIFY's default timeout
        QVERIFY2(Test::Rendezvous::waitForHandshake(*firstClient, 5000, &handshakeWhy),
                 qPrintable(handshakeWhy));
    }

    QAction* connectAction = managedConnectAction(firstWindow);
    QVERIFY(connectAction != nullptr);
    connectAction->trigger();
    QTRY_VERIFY(controller.selector()->isVisible());
    controller.selector()->setSelectedKey(QStringLiteral("saved:b"));
    QCoreApplication::processEvents();
    QCOMPARE(controller.sessions()->window(), firstWindow);
    QVERIFY(firstClient->isHandshakeComplete());
    QVERIFY(!cores.secondServer.hasAuthenticatedSession());

    bool cancelledEditor = false;
    QTimer::singleShot(0, &controller, [&] {
        auto* editor = qobject_cast<CoreTargetEditor*>(QApplication::activeModalWidget());
        if (editor == nullptr) {
            return;
        }
        auto* cancel = editor->findChild<QPushButton*>(QStringLiteral("coreTargetEditorCancel"));
        if (cancel != nullptr) {
            cancelledEditor = true;
            cancel->click();
        }
    });
    auto* edit = button(controller.selector(), QStringLiteral("connectionSelectorEdit"));
    QVERIFY(edit != nullptr && edit->isEnabled());
    edit->click();
    QVERIFY(cancelledEditor);
    QCOMPARE(controller.sessions()->window(), firstWindow);
    QVERIFY(!firstWindowLifetime.isNull() && !firstClient.isNull());
    QVERIFY(firstClient->isHandshakeComplete());
    QVERIFY(!cores.secondServer.hasAuthenticatedSession());
    controller.shutdown();
}

void TestGuiConnectionController::explicitConnectToSavedBReplacesWholeLiveA()
{
    LoopbackCores cores;
    QVERIFY(cores.start());
    QVERIFY(installTargets({cores.firstTarget(), cores.secondTarget()}, QStringLiteral("a")));

    GuiConnectionController controller;
    controller.start({});
    QTRY_VERIFY(cores.firstServer.hasAuthenticatedSession());
    MainWindow* firstWindow = controller.sessions()->window();
    QPointer<MainWindow> firstWindowLifetime = firstWindow;
    QPointer<RadioModel> firstModel = firstWindow->radioModel();
    QPointer<StationClient> firstClient = firstWindow->findChild<StationClient*>();
    QVERIFY(firstClient);
    {
        QString handshakeWhy;  // QTRY_VERIFY's default timeout
        QVERIFY2(Test::Rendezvous::waitForHandshake(*firstClient, 5000, &handshakeWhy),
                 qPrintable(handshakeWhy));
    }

    controller.showConnections();
    controller.selector()->setSelectedKey(QStringLiteral("saved:b"));
    auto* connect = button(controller.selector(), QStringLiteral("connectionSelectorConnect"));
    QVERIFY(connect != nullptr && connect->isEnabled());
    connect->click();
    QTRY_VERIFY(firstWindowLifetime.isNull() && firstModel.isNull() && firstClient.isNull());
    QTRY_VERIFY(cores.secondServer.hasAuthenticatedSession());
    QCOMPARE(controller.sessions()->selection().savedId, QStringLiteral("b"));
    StationClient* secondClient = controller.sessions()->window()->findChild<StationClient*>();
    QVERIFY(secondClient != nullptr);
    {
        QString handshakeWhy;  // QTRY_VERIFY's default timeout
        QVERIFY2(Test::Rendezvous::waitForHandshake(*secondClient, 5000, &handshakeWhy),
                 qPrintable(handshakeWhy));
    }
    QTRY_VERIFY(!controller.selector()->isVisible());
    // The Core can be authenticated while its radio is offline.
    QVERIFY(!controller.sessions()->window()->radioModel()->isConnected());
    controller.showConnections();
    secondClient->pathChanged();
    QCoreApplication::processEvents();
    QVERIFY(controller.selector()->isVisible());
    controller.shutdown();
}

void TestGuiConnectionController::localRadioSuccessDismissesOnlyItsRequestedConnection()
{
    Test::P1FakeRadio fake;
    fake.start();
    RadioInfo radio;
    radio.address = fake.localAddress();
    radio.port = fake.localPort();
    radio.boardType = HPSDRHW::HermesLite;
    radio.protocol = ProtocolVersion::Protocol1;
    radio.macAddress = QStringLiteral("AA:BB:CC:11:22:33");
    radio.firmwareVersion = 72;
    AppSettings::instance().saveRadio(radio, false, false);
    QVERIFY(AppSettings::instance().save());
    GuiConnectionController controller;
    controller.start({});
    QTRY_VERIFY(controller.selector()->isVisible());
    connect(controller.sessions(), &GuiSessionCoordinator::windowChanged, &controller,
        [](MainWindow* window) {
            if (window) {
                window->radioModel()->wdspEngine()->setSynchronousInitForTest(true);
                Test::useNoCaptureHelper(*window->radioModel());
            }
        });
    controller.selector()->connectRequested(QStringLiteral("radio:") + radio.macAddress);
    QTRY_VERIFY(controller.sessions()->window()->radioModel()->connectionState() == ConnectionState::Connected);
    QTRY_VERIFY(!controller.selector()->isVisible());
    controller.showConnections();
    controller.sessions()->window()->radioModel()->infoChanged();
    QCoreApplication::processEvents();
    QVERIFY(controller.selector()->isVisible());
    controller.shutdown();
}

void TestGuiConnectionController::automaticLocalStartupDoesNotOpenSelector_data()
{
    QTest::addColumn<bool>("hosted");
    QTest::newRow("saved direct target") << false;
    QTest::newRow("hosted desktop startup") << true;
}

void TestGuiConnectionController::automaticLocalStartupDoesNotOpenSelector()
{
    QFETCH(bool, hosted);
    Test::P1FakeRadio fake;
    fake.start();
    RadioInfo radio;
    radio.address = fake.localAddress();
    radio.port = fake.localPort();
    radio.boardType = HPSDRHW::HermesLite;
    radio.protocol = ProtocolVersion::Protocol1;
    radio.macAddress = QStringLiteral("AA:BB:CC:44:55:66");
    radio.firmwareVersion = 72;
    AppSettings& settings = AppSettings::instance();
    settings.saveRadio(radio, false, true);
    settings.setLastConnected(radio.macAddress);
    settings.setValue(QStringLiteral("DesktopCore/Run"), hosted);
    QVERIFY(settings.save());
    const QString configPath = QFileInfo(settings.filePath()).absolutePath() + QStringLiteral("/station.conf");
    const auto removeConfig = qScopeGuard([&configPath] { QFile::remove(configPath); });
    std::unique_ptr<StationHandover> ownership;
    GuiConnectionController controller;
    if (hosted) {
        ownership = std::make_unique<StationHandover>(AppSettings::profileOverride());
        QString error;
        QVERIFY2(ownership->acquire(0, &error), qPrintable(error));
        const quint16 selectedPort = unusedLoopbackPort();
        QVERIFY(selectedPort != 0);
        QFile config(configPath);
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray bytes = QStringLiteral("remote_bind = 127.0.0.1\nremote_port = %1\n"
            "status_page = off\nrendezvous_servers =\nradio_mac = %2\n")
            .arg(selectedPort).arg(radio.macAddress).toUtf8();
        QCOMPARE(config.write(bytes), bytes.size());
        config.close();
        QVERIFY(controller.sessions()->configureDesktopStation(AppSettings::profileOverride(), true));
    }
    connect(controller.sessions(), &GuiSessionCoordinator::windowChanged, &controller,
        [&radio, hosted](MainWindow* window) {
            if (!window) { return; }
            window->radioModel()->wdspEngine()->setSynchronousInitForTest(true);
            Test::useNoCaptureHelper(*window->radioModel());
            if (hosted) {
                window->radioModel()->discovery()->injectLastSeenForTest(radio.macAddress, radio, 0);
            }
        });
    QSignalSpy requests(controller.sessions(), &GuiSessionCoordinator::connectionsRequested);
    controller.start({}); // No selector gesture: this must exercise normal automatic startup.
    RadioModel* model = controller.sessions()->window()->radioModel();
    if (hosted) { model->discovery()->discoveryFinished(); }
    QTRY_COMPARE(model->connectionState(), ConnectionState::Connected);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCoreApplication::processEvents();
    QCOMPARE(requests.count(), 0);
    QVERIFY(!controller.selector()->isVisible());

    controller.showConnections();
    model->connectionStateChanged(ConnectionState::Connected);
    QCoreApplication::processEvents();
    QVERIFY(controller.selector()->isVisible());
    controller.selector()->close();

    // Existing explicit failure presentation must remain available.
    model->autoConnectFailed(radio.macAddress, ConnectFailure::Timeout);
    QTRY_VERIFY(controller.selector()->isVisible());
    controller.selector()->close();
    // Exercise the actual MainWindow signal handler without waiting for a
    // hardware watchdog; no DSP/transport action is inferred from these seams.
    model->setConnectionStateForTest(ConnectionState::LinkLost);
    QTRY_VERIFY(controller.selector()->isVisible());
    controller.selector()->close();
    model->setConnectionStateForTest(ConnectionState::Disconnected);
    QTRY_VERIFY(controller.selector()->isVisible());
    controller.shutdown();
}

void TestGuiConnectionController::staleAttemptCannotDismissSelector_data()
{
    QTest::addColumn<int>("cause");
    QTest::newRow("superseded request") << 0;
    QTest::newRow("failed attempt then independent retry") << 1;
    QTest::newRow("replacement session") << 2;
}

void TestGuiConnectionController::staleAttemptCannotDismissSelector()
{
    QFETCH(int, cause);
    LoopbackCores cores;
    QVERIFY(cores.start());
    QObject::disconnect(&cores.firstListener, nullptr, &cores.firstServer, nullptr);
    QVERIFY(installTargets({cores.firstTarget(), cores.secondTarget()}, QStringLiteral("local")));
    GuiConnectionController controller;
    controller.start({});
    QTRY_VERIFY(controller.selector()->isVisible());
    controller.selector()->connectRequested(QStringLiteral("saved:a"));
    QTRY_VERIFY(cores.firstListener.hasPendingConnections());
    auto socket = std::unique_ptr<QWebSocket>(cores.firstListener.nextPendingConnection());
    QPointer<StationClient> firstClient = controller.sessions()->window()->findChild<StationClient*>();
    QVERIFY(firstClient);
    if (cause == 0) {
        // A refused newer selection still revokes the old attempt's UI ownership.
        controller.selector()->connectRequested(QStringLiteral("saved:missing"));
        auto* notice = controller.selector()->findChild<QLabel*>(QStringLiteral("connectionSelectorNotice"));
        QVERIFY(notice);
        QTRY_VERIFY(notice->text().contains(QStringLiteral("no longer available")));
    } else if (cause == 1) {
        firstClient->disconnectFromStation(QStringLiteral("test failure"), false);
        QTRY_VERIFY(!firstClient->isConnectionActive());
        QVERIFY(controller.selector()->isVisible());
        auto* controls = controller.sessions()->window()->findChild<RemoteConnectionController*>();
        QVERIFY(controls);
        controls->connectToStation(); // An independent retry does not own the selector.
        QTRY_VERIFY(cores.firstListener.hasPendingConnections());
        socket.reset(cores.firstListener.nextPendingConnection());
    } else {
        const SavedCoreTarget target = cores.secondTarget();
        const quint64 oldGeneration = controller.sessions()->generation();
        QVERIFY(controller.sessions()->replace({target.connection, target.id, {}}, true));
        QVERIFY(controller.sessions()->generation() != oldGeneration);
        QVERIFY(firstClient.isNull());
        auto* replacement = controller.sessions()->window()->findChild<StationClient*>();
        QVERIFY(replacement);
        QTRY_VERIFY(replacement->isHandshakeComplete());
        QVERIFY(controller.selector()->isVisible());
        controller.shutdown();
        return;
    }
    auto transport = std::make_unique<WebSocketTransport>(socket.release(), StationServer::kMaxIncomingMessageBytes);
    cores.firstServer.acceptTransport(transport.release());
    QTRY_VERIFY(firstClient->isHandshakeComplete());
    QVERIFY(controller.selector()->isVisible());
    controller.shutdown();
}

void TestGuiConnectionController::pendingAndTransportFailureKeepSelectorOpen()
{
    QWebSocketServer listener(QStringLiteral("delayed Core"), QWebSocketServer::NonSecureMode);
    QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
    QVERIFY(installTargets({savedTarget(QStringLiteral("pending"), QStringLiteral("Pending"),
        listener.serverPort(), QStringLiteral("invalid-token"))}, QStringLiteral("local")));
    GuiConnectionController controller;
    controller.start({});
    QTRY_VERIFY(controller.selector()->isVisible());
    controller.selector()->connectRequested(QStringLiteral("saved:pending"));
    QTRY_VERIFY(listener.hasPendingConnections());
    std::unique_ptr<QWebSocket> socket(listener.nextPendingConnection());
    QVERIFY(controller.selector()->isVisible());
    auto* model = controller.sessions()->window()->radioModel();
    // A mirrored radio state is not proof of an authenticated Core session.
    model->setConnectionStateForTest(ConnectionState::Connected);
    QVERIFY(controller.selector()->isVisible());
    socket->close();
    auto* client = controller.sessions()->window()->findChild<StationClient*>();
    QVERIFY(client);
    QTRY_VERIFY(!client->isConnectionActive() || client->isReconnectPending());
    QVERIFY(controller.selector()->isVisible());
    controller.shutdown();
}

void TestGuiConnectionController::closingAndReopeningDuringPendingConnectionKeepsSelectorOpen()
{
    LoopbackCores cores;
    QVERIFY(cores.start());
    // Delay authentication by disconnecting the listener's server admission callback.
    QObject::disconnect(&cores.firstListener, nullptr, &cores.firstServer, nullptr);
    QVERIFY(installTargets({cores.firstTarget()}, QStringLiteral("local")));
    GuiConnectionController controller;
    controller.start({});
    QTRY_VERIFY(controller.selector()->isVisible());
    controller.selector()->connectRequested(QStringLiteral("saved:a"));
    QTRY_VERIFY(cores.firstListener.hasPendingConnections());
    controller.selector()->close();
    controller.showConnections();
    auto transport = std::make_unique<WebSocketTransport>(cores.firstListener.nextPendingConnection(),
        StationServer::kMaxIncomingMessageBytes);
    cores.firstServer.acceptTransport(transport.release());
    auto* client = controller.sessions()->window()->findChild<StationClient*>();
    QVERIFY(client);
    QTRY_VERIFY(client->isHandshakeComplete());
    QVERIFY(controller.selector()->isVisible());
    controller.shutdown();
}

void TestGuiConnectionController::disconnectCancelsRetryWithoutUsingHighlightedB()
{
    LoopbackCores cores;
    QVERIFY(cores.start());
    QVERIFY(installTargets({cores.firstTarget(), cores.secondTarget()}, QStringLiteral("a")));

    GuiConnectionController controller;
    controller.start({});
    QTRY_VERIFY(cores.firstServer.hasAuthenticatedSession());
    MainWindow* firstWindow = controller.sessions()->window();
    QPointer<StationClient> firstClient = firstWindow->findChild<StationClient*>();
    QVERIFY(firstClient);
    {
        QString handshakeWhy;  // QTRY_VERIFY's default timeout
        QVERIFY2(Test::Rendezvous::waitForHandshake(*firstClient, 5000, &handshakeWhy),
                 qPrintable(handshakeWhy));
    }
    firstClient->disconnectFromStation(QStringLiteral("test retry"), true);
    QTRY_VERIFY(firstClient->isReconnectPending());

    controller.showConnections();
    controller.selector()->setSelectedKey(QStringLiteral("saved:b"));
    auto* disconnect = button(controller.selector(), QStringLiteral("connectionSelectorDisconnect"));
    QVERIFY(disconnect != nullptr && disconnect->isEnabled());
    QCOMPARE(disconnect->text(), QStringLiteral("Cancel retry"));
    disconnect->click();
    QTRY_VERIFY(!firstClient->isReconnectPending());
    QVERIFY(!firstClient->isConnectionActive());
    QCOMPARE(controller.sessions()->window(), firstWindow);
    QCOMPARE(controller.sessions()->selection().savedId, QStringLiteral("a"));
    QVERIFY(!cores.secondServer.hasAuthenticatedSession());
    controller.shutdown();
}

// R-R3-16 / R-R3-38: the Connections window's Disconnect is the operator's
// own, so it reaches RemoteConnectionController::operatorDisconnected and
// asks for Connections exactly once. Nothing dials: the Core holds no session afterwards.
void TestGuiConnectionController::connectionsDisconnectReopensConnectionsOnceWithoutDialling()
{
    LoopbackCores cores;
    QVERIFY(cores.start());
    QVERIFY(installTargets({cores.firstTarget()}, QStringLiteral("a")));

    GuiConnectionController controller;
    controller.start({});
    QTRY_VERIFY(cores.firstServer.hasAuthenticatedSession());
    MainWindow* window = controller.sessions()->window();
    QPointer<StationClient> client = window->findChild<StationClient*>();
    auto* remoteControls = window->findChild<RemoteConnectionController*>();
    QVERIFY(client);
    QVERIFY(remoteControls);
    {
        QString handshakeWhy;  // QTRY_VERIFY's default timeout
        QVERIFY2(Test::Rendezvous::waitForHandshake(*client, 5000, &handshakeWhy),
                 qPrintable(handshakeWhy));
    }
    // These loopback Cores name no radio, so the old automatic open (which
    // needs a radio name) could not fire here either way; the remote
    // window harness covers that half with a named radio.

    controller.showConnections();
    QVERIFY(controller.selector()->isVisible());
    QSignalSpy operatorDisconnects(remoteControls,
                                   &RemoteConnectionController::operatorDisconnected);
    QSignalSpy windowRequests(window, &MainWindow::connectionsRequested);
    QSignalSpy pickerRequests(controller.sessions(), &GuiSessionCoordinator::connectionsRequested);
    QVERIFY(operatorDisconnects.isValid());
    QVERIFY(windowRequests.isValid());
    QVERIFY(pickerRequests.isValid());
    auto* disconnect = button(controller.selector(), QStringLiteral("connectionSelectorDisconnect"));
    QVERIFY(disconnect != nullptr && disconnect->isEnabled());
    disconnect->click();
    QCOMPARE(operatorDisconnects.size(), 1);
    QCOMPARE(windowRequests.size(), 1);
    QCOMPARE(pickerRequests.size(), 0); // MainWindow forwards this queued.
    QTRY_COMPARE(pickerRequests.size(), 1);
    QTRY_VERIFY(!cores.firstServer.hasAuthenticatedSession());
    // Long enough for any queued reopen or dial to have run.
    QTest::qWait(300);

    QCOMPARE(operatorDisconnects.size(), 1);
    QCOMPARE(windowRequests.size(), 1);
    QCOMPARE(pickerRequests.size(), 1);
    QVERIFY(controller.selector()->isVisible());
    QVERIFY(!client->isConnectionActive());
    QVERIFY(!client->isReconnectPending());
    QVERIFY(!cores.firstServer.hasAuthenticatedSession());
    QCOMPARE(controller.sessions()->window(), window);
    controller.shutdown();
}

void TestGuiConnectionController::verifiedPathWinnerIsKeptWithoutAnotherSnapshot()
{
    LoopbackCores cores;
    QVERIFY(cores.start());
    const SavedCoreTarget saved = cores.firstTarget();
    QVERIFY(installTargets({saved}, saved.id));
    GuiConnectionController controller;
    controller.start({});
    auto* client = controller.sessions()->window()->findChild<StationClient*>();
    QVERIFY(client);
    QTRY_VERIFY(client->isHandshakeComplete());
    QSignalSpy snapshots(client, &StationClient::stateSnapshotApplied);
    QSignalSpy moved(client, &StationClient::pathChanged);
    auto* stationB = new Test::LoopbackTransport(QStringLiteral("station B"));
    auto* clientB = new Test::LoopbackTransport(QStringLiteral("client B"));
    stationB->linkTo(clientB);
    QSignalSpy hello(clientB, &SessionTransport::textReceived);
    cores.firstServer.acceptTransport(stationB);
    QTRY_VERIFY(!hello.isEmpty());
    const QUrl provedUrl(QStringLiteral("wss://192.0.2.8:47911"));
    QVERIFY(client->moveSessionForTest(clientB, PathRacer::ThisNetwork, provedUrl));
    QTRY_COMPARE(moved.size(), 1);
    QCOMPARE(snapshots.size(), 0);
    QCOMPARE(client->connectedUrl(), provedUrl);
    CoreTargetStore persisted(AppSettings::instance());
    QVERIFY(persisted.load());
    QCOMPARE(persisted.target(saved.id)->connection.cachedAddresses.first(), provedUrl.toString());
    auto* wrongStation = new Test::LoopbackTransport(QStringLiteral("wrong station"));
    auto* wrongClient = new Test::LoopbackTransport(QStringLiteral("wrong client"));
    wrongStation->linkTo(wrongClient);
    QSignalSpy wrongHello(wrongClient, &SessionTransport::textReceived);
    cores.secondServer.acceptTransport(wrongStation);
    QTRY_VERIFY(!wrongHello.isEmpty());
    QVERIFY(client->moveSessionForTest(wrongClient, PathRacer::ThisNetwork,
                                    QUrl(QStringLiteral("wss://192.0.2.99:47910"))));
    QTRY_VERIFY(!client->upgradeUnderWayForTest());
    QCOMPARE(moved.size(), 1);
    QCOMPARE(client->connectedUrl(), provedUrl);
    CoreTargetStore rejected(AppSettings::instance());
    QVERIFY(rejected.load());
    QCOMPARE(rejected.target(saved.id)->connection.cachedAddresses.first(), provedUrl.toString());
    controller.selector()->forgetRequested(QStringLiteral("saved:") + saved.id);
    client->pathChanged(); // A late notification must not recreate a forgotten entry.
    CoreTargetStore forgotten(AppSettings::instance());
    QVERIFY(forgotten.load());
    QVERIFY(!forgotten.target(saved.id));
    QVERIFY(client->isHandshakeComplete());
    controller.shutdown();
}

void TestGuiConnectionController::savedRowShowsLastAuthenticatedAddress()
{
    SavedCoreTarget saved = savedTarget(QStringLiteral("address"), QStringLiteral("My Core"), 47910, QStringLiteral("fake"));
    saved.connection.cachedAddresses = {QStringLiteral("wss://192.0.2.8:47911")};
    const auto row = GuiConnectionController::savedCoreRow(saved, true);
    QCOMPARE(row.name, QStringLiteral("My Core (local name)"));
    QCOMPARE(row.address, QStringLiteral("192.0.2.8:47911 (last worked)"));
    QCOMPARE(saved.connection.url, QStringLiteral("ws://127.0.0.1:47910"));
    saved.connection.coreAddresses = {QStringLiteral("wss://[2001:db8::5]:47912")};
    CoreTargetEditor editor(saved);
    auto* address = editor.findChild<QLineEdit*>(QStringLiteral("coreTargetEditorAddress"));
    QVERIFY(address);
    address->setText(QStringLiteral("wss://192.0.2.10:47910"));
    QVERIFY(editor.target().connection.coreAddresses.isEmpty());
    QVERIFY(editor.target().connection.cachedAddresses.isEmpty());
    const QString detail = GuiConnectionController::savedCoreDetails(saved);
    if (!qEnvironmentVariableIsEmpty("NEREUS_ADDRESS_EXAMPLE_DIR")) {
        ConnectionSelector example;
        example.resize(720, 700);
        example.setTargets({GuiConnectionController::savedCoreRow(saved, true)});
        example.setNotice(detail);
        example.show();
        QTRY_VERIFY(example.isVisible());
        QVERIFY(example.grab().save(qEnvironmentVariable("NEREUS_ADDRESS_EXAMPLE_DIR") + QStringLiteral("/saved-address-details.png")));
        SavedCoreTarget full = saved;
        full.connection.identityFingerprint = QByteArray(32, 'k');
        full.connection.cachedAddresses = {QStringLiteral("wss://192.0.2.8:47911"), QStringLiteral("wss://192.0.2.9:47912"),
            QStringLiteral("wss://192.0.2.10:47913"), QStringLiteral("wss://192.0.2.11:47914")};
        full.connection.coreAddresses.clear();
        for (int i = 1; i <= 8; ++i) { full.connection.coreAddresses.append(QStringLiteral("wss://[2001:db8::%1]:47910").arg(i)); }
        example.setTargets({GuiConnectionController::savedCoreRow(full, true)});
        example.setNotice(GuiConnectionController::savedCoreDetails(full));
        QApplication::processEvents();
        QVERIFY(example.grab().save(qEnvironmentVariable("NEREUS_ADDRESS_EXAMPLE_DIR") + QStringLiteral("/full-address-details.png")));
        qInfo() << "Full inventory dialog dimensions" << example.size();
    }
    QVERIFY(detail.contains(QStringLiteral("Name on this computer: My Core")));
    QVERIFY(detail.contains(QStringLiteral("Configured address: 127.0.0.1:47910")));
    QVERIFY(detail.contains(QStringLiteral("Last worked: 192.0.2.8:47911")));
    QVERIFY(detail.contains(QStringLiteral("authenticated Core (may not be reachable)")));
    QVERIFY(!detail.contains(QStringLiteral("fake")));
}

// iPhone app plan Task 27 fix wave: a connect remembers where the Core
// was reached in the store, and a manual reconnect in the same window
// passes the store's current list, not the one the window was made with.
void TestGuiConnectionController::aManualReconnectReadsTheStoresLastAddresses()
{
    LoopbackCores cores;
    QVERIFY(cores.start());
    const SavedCoreTarget saved = cores.firstTarget();
    QVERIFY(saved.connection.cachedAddresses.isEmpty());
    QVERIFY(installTargets({saved}, QStringLiteral("a")));

    GuiConnectionController controller;
    controller.start({});
    QTRY_VERIFY(cores.firstServer.hasAuthenticatedSession());
    MainWindow* window = controller.sessions()->window();
    QPointer<StationClient> client = window->findChild<StationClient*>();
    auto* remoteControls = window->findChild<RemoteConnectionController*>();
    QVERIFY(client);
    QVERIFY(remoteControls);
    {
        QString handshakeWhy;  // QTRY_VERIFY's default timeout
        QVERIFY2(Test::Rendezvous::waitForHandshake(*client, 5000, &handshakeWhy),
                 qPrintable(handshakeWhy));
    }
    QVERIFY(!client->signedInWithDeviceKey());
    QCOMPARE(client->capabilities().coreAddressesVersion, 0);
    client->remoteDevices()->applyObject(QByteArrayLiteral("devices"),
        {{9, QByteArrayLiteral("coreAddresses"), MirrorWireKind::Utf8, QString::fromUtf8("{\"addresses\":[\"[2001:db8::5]:47912\"]}")}});
    CoreTargetStore unsupported(AppSettings::instance());
    QVERIFY(unsupported.load());
    QVERIFY(unsupported.target(saved.id)->connection.coreAddresses.isEmpty());
    const QStringList remembered{saved.connection.url};
    QTRY_VERIFY([&] {
        CoreTargetStore store(AppSettings::instance());
        return store.load() && store.target(QStringLiteral("a"))
               && store.target(QStringLiteral("a"))->connection.cachedAddresses == remembered;
    }());

    remoteControls->disconnectFromStation();
    QTRY_VERIFY(!client->isConnectionActive());
    remoteControls->connectToStation();
    QCOMPARE(client->cachedAddresses(), QList<QUrl>{QUrl(saved.connection.url)});
    {
        QString handshakeWhy;  // QTRY_VERIFY's default timeout
        QVERIFY2(Test::Rendezvous::waitForHandshake(*client, 5000, &handshakeWhy),
                 qPrintable(handshakeWhy));
    }
    controller.shutdown();
}

void TestGuiConnectionController::persistentLocalChoiceReturnsToEmbeddedCoreWithoutRadioAutoconnect()
{
    LoopbackCores cores;
    QVERIFY(cores.start());
    QVERIFY(installTargets({cores.firstTarget(), cores.secondTarget()}, QStringLiteral("a")));
    RadioInfo oldLocalRadio;
    oldLocalRadio.macAddress = QStringLiteral("AA:BB:CC:DD:EE:99");
    oldLocalRadio.name = QStringLiteral("Old local radio");
    oldLocalRadio.address = QHostAddress::LocalHost;
    oldLocalRadio.port = 1024;
    AppSettings::instance().saveRadio(oldLocalRadio, false, true);
    QVERIFY(AppSettings::instance().save());

    GuiConnectionController controller;
    controller.start({});
    QTRY_VERIFY(cores.firstServer.hasAuthenticatedSession());
    QPointer<MainWindow> remoteWindow = controller.sessions()->window();
    QVERIFY(remoteWindow);
    controller.showConnections();
    controller.selector()->setSelectedKey(QStringLiteral("local"));
    auto* connect = button(controller.selector(), QStringLiteral("connectionSelectorConnect"));
    QVERIFY(connect != nullptr && connect->isEnabled());
    connect->click();
    QTRY_VERIFY(remoteWindow.isNull());
    MainWindow* localWindow = controller.sessions()->window();
    QVERIFY(localWindow != nullptr);
    QVERIFY(localWindow->radioModel()->ownsLocalDsp());
    QVERIFY(localWindow->findChild<StationClient*>() == nullptr);
    QCOMPARE(localWindow->radioModel()->connectionState(), ConnectionState::Disconnected);
    CoreTargetStore store(AppSettings::instance());
    QVERIFY(store.load());
    QCOMPARE(store.selectedId(), QStringLiteral("local"));
    controller.shutdown();
}

void TestGuiConnectionController::savedCoreEditsDoNotChangeCurrentTupleBeforeConnect()
{
    LoopbackCores cores;
    QVERIFY(cores.start());
    const SavedCoreTarget first = cores.firstTarget();
    const SavedCoreTarget second = cores.secondTarget();
    QVERIFY(installTargets({first, second}, QStringLiteral("a")));

    GuiConnectionController controller;
    controller.start({});
    QTRY_VERIFY(cores.firstServer.hasAuthenticatedSession());
    QPointer<StationClient> firstClient = controller.sessions()->window()->findChild<StationClient*>();
    QVERIFY(firstClient);
    {
        QString handshakeWhy;  // QTRY_VERIFY's default timeout
        QVERIFY2(Test::Rendezvous::waitForHandshake(*firstClient, 5000, &handshakeWhy),
                 qPrintable(handshakeWhy));
    }
    const StationStartupSelection active = controller.sessions()->selection();

    controller.showConnections();
    controller.selector()->setSelectedKey(QStringLiteral("saved:a"));
    bool savedEditor = false;
    QTimer::singleShot(0, &controller, [&] {
        auto* editor = qobject_cast<CoreTargetEditor*>(QApplication::activeModalWidget());
        if (editor == nullptr) {
            return;
        }
        auto* address = editor->findChild<QLineEdit*>(QStringLiteral("coreTargetEditorAddress"));
        auto* token = editor->findChild<QLineEdit*>(QStringLiteral("coreTargetEditorToken"));
        auto* save = editor->findChild<QPushButton*>(QStringLiteral("coreTargetEditorSave"));
        if (address != nullptr && token != nullptr && save != nullptr) {
            address->setText(second.connection.url);
            token->setText(second.connection.token);
            savedEditor = true;
            save->click();
        }
    });
    auto* edit = button(controller.selector(), QStringLiteral("connectionSelectorEdit"));
    QVERIFY(edit != nullptr && edit->isEnabled());
    edit->click();
    QVERIFY(savedEditor);
    QCOMPARE(controller.sessions()->selection().savedId, active.savedId);
    QCOMPARE(controller.sessions()->selection().connection.url, active.connection.url);
    QCOMPARE(controller.sessions()->selection().connection.token, active.connection.token);
    QVERIFY(!firstClient.isNull() && firstClient->isHandshakeComplete());
    QVERIFY(!cores.secondServer.hasAuthenticatedSession());
    controller.shutdown();
}

void TestGuiConnectionController::corruptStartupDocumentShowsIdleLocalAndNotice()
{
    AppSettings::instance().setValue(QStringLiteral("ConnectionTargets/V1"),
                                     QStringLiteral("{not valid JSON"));
    QVERIFY(AppSettings::instance().save());

    GuiConnectionController controller;
    controller.start({});
    MainWindow* window = controller.sessions()->window();
    QVERIFY(window != nullptr);
    QVERIFY(window->radioModel()->ownsLocalDsp());
    QCOMPARE(window->radioModel()->connectionState(), ConnectionState::Disconnected);
    QVERIFY(window->findChild<StationClient*>() == nullptr);
    QTRY_VERIFY(controller.selector()->isVisible());
    auto* notice = controller.selector()->findChild<QLabel*>(QStringLiteral("connectionSelectorNotice"));
    QVERIFY(notice != nullptr && notice->isVisible());
    QVERIFY(!notice->text().isEmpty());
    controller.shutdown();
}

void TestGuiConnectionController::codeOnlyDialogPairsThroughMailboxAndOpensRemoteCore()
{
    using namespace NereusSDR::Test::Rendezvous;
    LocalService service;
    QVERIFY2(service.start(), qPrintable(service.startFailure()));
    Core core;
    core.server->devicesFacade()->setCoreAddresses(QString::fromUtf8("{\"addresses\":[\"[2001:db8::4]:47914\"]}"));
    StationRendezvous rendezvous(core.server.get(), {service.url()}, true);
    QSignalSpy nameplates(rendezvous.client(), &RendezvousClient::nameplateClaimed);
    QVERIFY(nameplates.isValid());
    QVERIFY(rendezvous.start());
    QTRY_COMPARE_WITH_TIMEOUT(nameplates.size(), 1, 10000);
    const QString code = core.server->pairingWindow()->currentCode();
    QVERIFY(!code.isEmpty());
    AppSettings::instance().setValue(QStringLiteral("RemoteAccessServers"), service.url().toString());
    QVERIFY(AppSettings::instance().save());

    GuiConnectionController controller;
    controller.start({});
    MainWindow* originalWindow = controller.sessions()->window();
    QVERIFY(originalWindow && originalWindow->radioModel()->ownsLocalDsp());
    controller.showConnections();
    auto* addByCode = button(controller.selector(), QStringLiteral("connectionSelectorAddByCode"));
    QVERIFY(addByCode);
    bool canceledDialog = false;
    QTimer::singleShot(0, &controller, [&] {
        if (auto* dialog = qobject_cast<AddCoreByCodeDialog*>(QApplication::activeModalWidget())) {
            dialog->reject();
            canceledDialog = true;
        }
    });
    addByCode->click();
    QVERIFY(canceledDialog);
    QCOMPARE(controller.sessions()->window(), originalWindow);
    QVERIFY(!core.server->deviceStore()->find(
        ClientDeviceIdentity::forThisProfile()->fingerprint()).has_value());
    const int claimedNameplate = code.section(QLatin1Char('-'), 0, 0).toInt();
    const int otherNameplate = claimedNameplate == 1 ? 2 : 1;
    const QString missingCode = QString::number(otherNameplate)
        + code.mid(code.indexOf(QLatin1Char('-')));
    bool failedDialogAccepted = false;
    QTimer::singleShot(0, &controller, [&] {
        auto* dialog = qobject_cast<AddCoreByCodeDialog*>(QApplication::activeModalWidget());
        if (!dialog) { return; }
        auto* entry = dialog->findChild<QLineEdit*>(QStringLiteral("addCoreByCodeCode"));
        auto* pair = dialog->findChild<QPushButton*>(QStringLiteral("addCoreByCodePair"));
        if (!entry || !pair) { dialog->reject(); return; }
        entry->setText(missingCode);
        pair->click();
        failedDialogAccepted = dialog->result() == QDialog::Accepted;
    });
    addByCode->click();
    QVERIFY(failedDialogAccepted);
    auto* notice = controller.selector()->findChild<QLabel*>(QStringLiteral("connectionSelectorNotice"));
    QVERIFY(notice);
    QTRY_VERIFY_WITH_TIMEOUT(notice->text().contains(QStringLiteral("No Core is showing")), 10000);
    QCOMPARE(controller.sessions()->window(), originalWindow);
    QVERIFY(!core.server->deviceStore()->find(
        ClientDeviceIdentity::forThisProfile()->fingerprint()).has_value());
    bool dialogAccepted = false;
    QTimer::singleShot(0, &controller, [&] {
        auto* dialog = qobject_cast<AddCoreByCodeDialog*>(QApplication::activeModalWidget());
        if (!dialog) { return; }
        auto* entry = dialog->findChild<QLineEdit*>(QStringLiteral("addCoreByCodeCode"));
        auto* pair = dialog->findChild<QPushButton*>(QStringLiteral("addCoreByCodePair"));
        if (!entry || !pair) { dialog->reject(); return; }
        entry->setText(code);
        pair->click();
        dialogAccepted = dialog->result() == QDialog::Accepted;
    });
    addByCode->click();
    QVERIFY(dialogAccepted);

    {
        QString handshakeWhy;
        QVERIFY2(waitForHandshake(
                     [&controller]() -> StationClient* {
                         MainWindow* window = controller.sessions()->window();
                         return window ? window->findChild<StationClient*>() : nullptr;
                     },
                     kServiceConnectBudgetMs, &handshakeWhy),
                 qPrintable(handshakeWhy));
    }
    const auto selected = controller.sessions()->selection();
    QVERIFY(selected.connection.url.isEmpty());
    QVERIFY(selected.connection.isRemote());
    QCOMPARE(selected.connection.identityFingerprint,
             core.server->stationIdentity().fingerprint());
    QCOMPARE(selected.connection.rendezvousId,
             RendezvousWire::rendezvousId(core.server->stationIdentity().publicKeySpki()));
    QVERIFY(!controller.sessions()->window()->radioModel()->ownsLocalDsp());
    StationClient* client = controller.sessions()->window()->findChild<StationClient*>();
    QCOMPARE(client->connectionAttempt().tries.size(), 1);
    QVERIFY(client->connectionAttempt().tries.first().path
                == StationConnectionAttempt::Path::Service
            || client->connectionAttempt().tries.first().path
                == StationConnectionAttempt::Path::Relay
            || client->connectionAttempt().tries.first().path
                == StationConnectionAttempt::Path::WebRelay);
    CoreTargetStore persisted(AppSettings::instance());
    QVERIFY(persisted.load());
    QCOMPARE(persisted.targets().size(), 1);
    QCOMPARE(persisted.targets().first().connection.identityFingerprint,
             selected.connection.identityFingerprint);
    QVERIFY(persisted.targets().first().connection.url.isEmpty());
    QCOMPARE(persisted.targets().first().connection.coreAddresses,
             QStringList{QStringLiteral("wss://[2001:db8::4]:47914")});
    QVERIFY(client->signedInWithDeviceKey());
    QVERIFY(client->agreedMinor() >= 11);
    QCOMPARE(client->capabilities().coreAddressesVersion, 1);
    auto* controls = controller.sessions()->window()->findChild<RemoteConnectionController*>();
    QVERIFY(controls);
    QVERIFY(controls->endpointText().contains(QStringLiteral("Remote access")));
    {
        // An ICE pair not known yet is not shown as the relay; a known
        // relay is.
        const int rank = client->pathRank();
        const bool pairUnknown = client->pathPairUnknown();
        client->setPathRankForTest(PathRacer::ServiceRelayed, /*pairUnknown=*/true);
        QCOMPARE(controls->endpointText(),
                 QStringLiteral("Remote access service (no direct Core address)"));
        client->setPathRankForTest(PathRacer::ServiceRelayed, /*pairUnknown=*/false);
        QCOMPARE(controls->endpointText(),
                 QStringLiteral("Remote access relay (no direct Core address)"));
        client->setPathRankForTest(rank, pairUnknown);
    }
    QVERIFY(client->connectedUrl().isEmpty());
    if (!qEnvironmentVariableIsEmpty("NEREUS_ADDRESS_EXAMPLE_DIR")) {
        controller.showConnections();
        controller.selector()->detailsRequested(QStringLiteral("saved:") + selected.savedId);
        QApplication::processEvents();
        QVERIFY(controller.selector()->grab().save(qEnvironmentVariable("NEREUS_ADDRESS_EXAMPLE_DIR") + QStringLiteral("/active-service-details.png")));
    }
    const QString firstList = QString::fromUtf8("{\"addresses\":[\"[2001:db8::5]:47912\"]}");
    core.server->devicesFacade()->setCoreAddresses(firstList);
    QTRY_VERIFY([&] {
        CoreTargetStore current(AppSettings::instance());
        return current.load() && current.target(selected.savedId)->connection.coreAddresses
            == QStringList{QStringLiteral("wss://[2001:db8::5]:47912")};
    }());
    CoreTargetStore learned(AppSettings::instance());
    QVERIFY(learned.load());
    QVERIFY(learned.target(selected.savedId)->connection.cachedAddresses.isEmpty());
    QCOMPARE(client->cachedAddresses(), QList<QUrl>{QUrl(QStringLiteral("wss://[2001:db8::5]:47912"))});
    // An addressless service session must include its newly authenticated
    // list on automatic retry, without waiting for a manual reconnect.
    client->setReconnectBackoffUnitMs(1);
    client->disconnectFromStation(QStringLiteral("station link lost"), true);
    QTRY_VERIFY_WITH_TIMEOUT([&] {
        for (const auto& attempt : client->connectionAttempt().tries) {
            if (attempt.path == StationConnectionAttempt::Path::Direct) { return true; }
        }
        return false;
    }(), 5000);
    {
        QString why;
        QVERIFY2(waitForHandshake(*client, kServiceConnectBudgetMs, &why), qPrintable(why));
    }
    // A malformed list keeps the saved Core addresses; an explicit empty
    // list clears them.
    core.server->devicesFacade()->setCoreAddresses(QStringLiteral("malformed"));
    QTRY_COMPARE(client->remoteDevices()->coreInfo().coreAddresses, QStringLiteral("malformed"));
    CoreTargetStore malformed(AppSettings::instance());
    QVERIFY(malformed.load());
    QCOMPARE(malformed.target(selected.savedId)->connection.coreAddresses, learned.target(selected.savedId)->connection.coreAddresses);
    core.server->devicesFacade()->setCoreAddresses(QString::fromUtf8("{\"addresses\":[]}"));
    QTRY_COMPARE(client->remoteDevices()->coreInfo().coreAddresses, QString::fromUtf8("{\"addresses\":[]}"));
    CoreTargetStore clearedAddresses(AppSettings::instance());
    QVERIFY(clearedAddresses.load());
    QVERIFY(clearedAddresses.target(selected.savedId)->connection.coreAddresses.isEmpty());
    // Keep later service-only reconnect fixture local: no dialing advertised
    // documentation endpoints. Real list/race composition tested separately.
    SavedCoreTarget reset = *clearedAddresses.target(selected.savedId);
    reset.connection.coreAddresses.clear();
    QVERIFY(clearedAddresses.upsert(reset));
    controller.shutdown();
    QTRY_VERIFY_WITH_TIMEOUT(!core.server->hasAuthenticatedSession(), 10000);

    GuiConnectionController relaunched;
    relaunched.start({});
    {
        QString handshakeWhy;
        QVERIFY2(waitForHandshake(
                     [&relaunched]() -> StationClient* {
                         MainWindow* window = relaunched.sessions()->window();
                         return window ? window->findChild<StationClient*>() : nullptr;
                     },
                     kServiceConnectBudgetMs, &handshakeWhy),
                 qPrintable(handshakeWhy));
    }
    QCOMPARE(relaunched.sessions()->selection().savedId, persisted.targets().first().id);
    QVERIFY(relaunched.sessions()->selection().connection.url.isEmpty());
    QCOMPARE(relaunched.sessions()->selection().connection.identityFingerprint,
             selected.connection.identityFingerprint);
    relaunched.shutdown();
    QTRY_VERIFY_WITH_TIMEOUT(!core.server->hasAuthenticatedSession(), 10000);

    // Re-pair the same authenticated identity after the address book has a
    // direct route and a disabled service preference. The mailbox result has
    // no address, so the GUI must merge it into the existing row.
    CoreTargetStore beforeRepair(AppSettings::instance());
    QVERIFY(beforeRepair.load());
    SavedCoreTarget retained = *beforeRepair.target(persisted.targets().first().id);
    retained.label = QStringLiteral("Older saved name");
    // This fixture's Core listens through rendezvous only. Keep the old
    // direct address on loopback and verify its retention without claiming
    // that this re-pair also proves a direct WSS connection.
    retained.connection.url = QStringLiteral("wss://127.0.0.1:1");
    retained.connection.cachedAddresses = {retained.connection.url};
    retained.connection.reachFromAnywhere = false;
    retained.autoConnect = false;
    QVERIFY(beforeRepair.upsert(retained));
    core.server->pairingWindow()->reopen();
    QTRY_COMPARE_WITH_TIMEOUT(nameplates.size(), 2, 10000);
    const QString renewedCode = core.server->pairingWindow()->currentCode();
    QVERIFY(renewedCode.startsWith(QString::number(nameplates.last().first().toInt())
                                   + QLatin1Char('-')));
    GuiConnectionController repairing;
    repairing.start({});
    QVERIFY(repairing.sessions()->selection().connection.isRemote());
    repairing.showConnections();
    auto* again = button(repairing.selector(), QStringLiteral("connectionSelectorAddByCode"));
    QVERIFY(again);
    bool renewedDialogAccepted = false;
    QTimer::singleShot(0, &repairing, [&] {
        auto* dialog = qobject_cast<AddCoreByCodeDialog*>(QApplication::activeModalWidget());
        if (!dialog) { return; }
        auto* entry = dialog->findChild<QLineEdit*>(QStringLiteral("addCoreByCodeCode"));
        auto* pair = dialog->findChild<QPushButton*>(QStringLiteral("addCoreByCodePair"));
        if (!entry || !pair) { dialog->reject(); return; }
        entry->setText(renewedCode);
        pair->click();
        renewedDialogAccepted = dialog->result() == QDialog::Accepted;
    });
    again->click();
    QVERIFY(renewedDialogAccepted);
    QTRY_VERIFY_WITH_TIMEOUT(!core.server->pairingWindow()->isOpen(), 60000);
    const QString pairedLabel = persisted.targets().first().label;
    const auto savedRepairedLabel = [&] {
        CoreTargetStore current(AppSettings::instance());
        return current.load() && current.target(retained.id)
            && current.target(retained.id)->label == pairedLabel;
    };
    QTRY_VERIFY_WITH_TIMEOUT(savedRepairedLabel(), 10000);
    CoreTargetStore afterRepair(AppSettings::instance());
    QVERIFY(afterRepair.load());
    QCOMPARE(afterRepair.targets().size(), 1);
    const SavedCoreTarget repaired = afterRepair.targets().first();
    QCOMPARE(repaired.id, retained.id);
    QCOMPARE(repaired.label, pairedLabel);
    QCOMPARE(repaired.connection.url, retained.connection.url);
    QCOMPARE(repaired.connection.cachedAddresses, retained.connection.cachedAddresses);
    QCOMPARE(repaired.connection.reachFromAnywhere, false);
    QCOMPARE(repaired.connection.identityFingerprint, retained.connection.identityFingerprint);
    QCOMPARE(repaired.connection.rendezvousId, retained.connection.rendezvousId);
    repairing.shutdown();
}

void TestGuiConnectionController::acceptingCodeDialogAfterShutdownDoesNotStartPairing()
{
    GuiConnectionController controller;
    controller.start({});
    controller.showConnections();
    auto* addByCode = button(controller.selector(), QStringLiteral("connectionSelectorAddByCode"));
    auto* notice = controller.selector()->findChild<QLabel*>(QStringLiteral("connectionSelectorNotice"));
    QVERIFY(addByCode && notice);
    QTimer::singleShot(0, &controller, [&] {
        auto* dialog = qobject_cast<AddCoreByCodeDialog*>(QApplication::activeModalWidget());
        if (!dialog) { return; }
        controller.shutdown();
        dialog->accept();
    });
    addByCode->click();
    QVERIFY(notice->text().isEmpty());
    CoreTargetStore persisted(AppSettings::instance());
    QVERIFY(persisted.load());
    QVERIFY(persisted.targets().isEmpty());
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TestGuiConnectionController test;
    QTEST_SET_MAIN_SOURCE_PATH
    // 88 s by itself on a hosted CI runner, and past ctest's 120 s with
    // every case still passing when the runner ran four tests at once (CI
    // run 38999633360). Two groups run as their own ctest entries,
    // tst_gui_connection_controller_selector and _reconnect
    // (tests/CMakeLists.txt); the rest, with the pairing dialog cases, as
    // tst_gui_connection_controller. The limit is not raised.
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_GUI_CONNECTION_CONTROLLER_GROUP",
        {{QStringLiteral("selector"),
          {QStringLiteral("managedConnectActionOpensSelectorForEmptyLocalProfile"),
           QStringLiteral("selectingAndCancellingEditLeaveLiveAAndDoNotDialB"),
           QStringLiteral("explicitConnectToSavedBReplacesWholeLiveA"),
           QStringLiteral("localRadioSuccessDismissesOnlyItsRequestedConnection"),
           QStringLiteral("automaticLocalStartupDoesNotOpenSelector"),
           QStringLiteral("staleAttemptCannotDismissSelector"),
           QStringLiteral("pendingAndTransportFailureKeepSelectorOpen")}},
         {QStringLiteral("reconnect"),
          {QStringLiteral("closingAndReopeningDuringPendingConnectionKeepsSelectorOpen"),
           QStringLiteral("disconnectCancelsRetryWithoutUsingHighlightedB"),
           QStringLiteral("connectionsDisconnectReopensConnectionsOnceWithoutDialling"),
           QStringLiteral("aManualReconnectReadsTheStoresLastAddresses"),
           QStringLiteral("savedRowShowsLastAuthenticatedAddress"),
           QStringLiteral("verifiedPathWinnerIsKeptWithoutAnotherSnapshot")}}});
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}

#include "tst_gui_connection_controller.moc"
