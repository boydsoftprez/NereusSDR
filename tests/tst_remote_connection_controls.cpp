// no-port-check: NereusSDR-original. Remote GUI connection and hydration boundaries.
#include <QTest>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTimer>
#include <QWebSocketServer>
#include <QSslSocket>

#include <chrono>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/RadioDiscovery.h"
#include "core/session/IStationLink.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/settings/SettingsProxy.h"
#include "gui/MainWindow.h"
#include "gui/GuiConnectionController.h"
#include "gui/RemoteAudioStatus.h"
#include "gui/RemoteConnectionController.h"
#include "gui/RemoteDiagnosticsDialog.h"
#include "gui/RemoteMediaController.h"
#include "gui/NetworkDiagnosticsDialog.h"
#include "gui/TitleBar.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/RemoteAudioSessionHarness.h"
#include "fakes/MainWindowTestSettings.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

class RecoveryMediaTransport final : public IMediaTransport {
public:
    explicit RecoveryMediaTransport(QObject* parent) : IMediaTransport(parent) {}
    bool start(const StartOptions&) override
    {
        if (errorOnStart) { emit errorOccurred(QStringLiteral("test error during start")); }
        return true;
    }
    void stop() override { m_ready = false; }
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray&) override { return m_ready; }
    bool sendRtp(const QByteArray&) override { return m_ready; }
    bool isReady() const override { return m_ready; }
    void activate() { m_ready = true; emit ready(); }
    void closeUnexpectedly() { m_ready = false; emit closed(); }
    void failConnection(const QString& reason) { emit connectionFailed(reason); }
    void reportError(const QString& reason) { emit errorOccurred(reason); }
    // Starts, but reports an error on the way through start.
    bool errorOnStart = false;
private:
    bool m_ready = false;
};

// A transport whose peer cannot be built: it reports why and refuses to
// start, as LibDataChannelMediaTransport::start's catch does.
class BuildFailingMediaTransport final : public IMediaTransport {
public:
    explicit BuildFailingMediaTransport(QObject* parent) : IMediaTransport(parent) {}
    bool start(const StartOptions&) override
    {
        emit errorOccurred(QStringLiteral("could not create the peer connection"));
        return false;
    }
    void stop() override {}
    bool acceptDescription(const QString&, const QString&) override { return true; }
    bool acceptCandidate(const QString&, const QString&) override { return true; }
    bool sendDisplay(const QByteArray&) override { return false; }
    bool sendRtp(const QByteArray&) override { return false; }
    bool isReady() const override { return false; }
};

// mediaEndedWithoutRetryResetsTheBackoff: how the media ends.
constexpr int kCoreRejectsStart = 0;
constexpr int kErrorAfterStart = 1;
constexpr int kErrorDuringStart = 2;

class RecordingStationLink final : public IStationLink {
public:
    QStringList additions;
    CommandOutcome requestAddSlice(const QString& pan) override
    { additions << pan; return {true, {}}; }
    CommandOutcome requestAddSliceOnPan(const QString& pan) override
    { additions << pan; return {true, {}}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
};

class ScopedDiscoveryHoldOff final {
public:
    ScopedDiscoveryHoldOff()
    {
        RadioDiscovery::clearHoldOffForTest();
        m_discovery.holdOffScans(std::chrono::minutes{5});
    }

    ~ScopedDiscoveryHoldOff()
    {
        RadioDiscovery::clearHoldOffForTest();
    }

private:
    RadioDiscovery m_discovery;
};

class ScopedRemoteBackend final {
public:
    explicit ScopedRemoteBackend(ISettingsBackend* backend)
    {
        AppSettings::instance().setRemoteBackend(backend);
    }

    ~ScopedRemoteBackend()
    {
        AppSettings::instance().setRemoteBackend(nullptr);
    }
};

QAction* networkDiagnosticsToolsAction(MainWindow& window)
{
    for (QMenu* menu : window.findChildren<QMenu*>()) {
        if (menu->title() != QStringLiteral("&Tools")) { continue; }
        for (QAction* action : menu->actions()) {
            if (action->text() == QStringLiteral("&Network Diagnostics...")) {
                return action;
            }
        }
    }
    return nullptr;
}

// R-R3-23 Task 4: how many "op":"audio" control messages the station side
// has received so far.
int countAudioControlMessages(const QSignalSpy& spy)
{
    int count = 0;
    for (const auto& call : spy) {
        if (call.at(0).toJsonObject().value(QStringLiteral("op"))
            == QLatin1String("audio")) {
            ++count;
        }
    }
    return count;
}

// What the controller logs when a playing speaker stops reporting timing.
// Mirrors tst_remote_media_controller.cpp's speakerTimingLostLog(): the
// worker's pacing check or its next write notices first.
QRegularExpression remoteAudioSpeakerTimingLostLog()
{
    return QRegularExpression(QStringLiteral(
        "^Remote audio playback failed: (Speaker device timing became unavailable"
        "|Could not write remote audio to the speaker device) \\[ageMs="));
}

class TestRemoteConnectionControls : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        const QString profile = QStringLiteral("remote-connection-controls-%1")
                                    .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
        AppSettings::instance().clear();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery::clearHoldOffForTest();
    }

    void init()
    {
        AppSettings::instance().clear();
        // Every case builds a MainWindow; the Linux audio first-run dialog
        // is modal and would block the first event-loop turn (R-R3-21).
        Test::suppressLinuxAudioFirstRun();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery::clearHoldOffForTest();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
        RadioDiscovery::clearHoldOffForTest();
    }

    void toolsNetworkDiagnosticsActionRoutesWithoutChangingLocalConnection()
    {
        ScopedDiscoveryHoldOff holdOff;
        MainWindow window;
        RadioModel* const model = window.findChild<RadioModel*>();
        RadioDiscovery* const discovery = window.findChild<RadioDiscovery*>();
        QVERIFY(model);
        QVERIFY(discovery);
        QAction* const action = networkDiagnosticsToolsAction(window);
        QVERIFY(action);
        QVERIFY(action->isEnabled());

        QSignalSpy connectionChanges(model, &RadioModel::connectionStateChanged);
        QSignalSpy discoveryStarts(discovery, &RadioDiscovery::discoveryStarted);
        QVERIFY(model->connection() == nullptr);
        action->trigger();

        QVERIFY(window.findChild<NetworkDiagnosticsDialog*>() != nullptr);
        QCOMPARE(connectionChanges.count(), 0);
        QCOMPARE(discoveryStarts.count(), 0);
        QVERIFY(model->connection() == nullptr);
    }

    void toolsNetworkDiagnosticsActionRoutesDisconnectedRemoteWithoutRedial()
    {
        SettingsProxy proxy;
        ScopedRemoteBackend remoteBackend(&proxy);
        QTcpServer port;
        QVERIFY(port.listen(QHostAddress::LocalHost, 0));
        const quint16 unusedPort = port.serverPort();
        port.close();

        MainWindow window({QStringLiteral("ws://127.0.0.1:%1").arg(unusedPort), {}, {}, true});
        RadioModel* const model = window.findChild<RadioModel*>();
        StationClient* const client = window.findChild<StationClient*>();
        QVERIFY(model && client);
        QVERIFY(!model->ownsLocalDsp());
        client->disconnectFromStation(QStringLiteral("test setup"));
        QVERIFY(!client->isConnectionActive());
        QVERIFY(!client->isReconnectPending());
        const quint32 epoch = client->sessionEpoch();
        QAction* const action = networkDiagnosticsToolsAction(window);
        QVERIFY(action);
        QVERIFY(action->isEnabled());
        action->trigger();

        QTRY_VERIFY(window.findChild<RemoteDiagnosticsDialog*>() != nullptr);
        QCOMPARE(client->sessionEpoch(), epoch);
        QVERIFY(!client->isConnectionActive());
        QVERIFY(!client->isReconnectPending());
        QVERIFY(model->connection() == nullptr);
    }

    void remoteTitleIsClickableInEverySessionState()
    {
        ConnectionSegment segment;
        QSignalSpy clicked(&segment, &ConnectionSegment::rttClicked);
        for (const auto state : {ConnectionState::Disconnected, ConnectionState::Connecting,
                                 ConnectionState::LinkLost, ConnectionState::Connected}) {
            segment.setState(state);
            segment.setRemoteStatusText(QStringLiteral("Core status"));
            QTest::mouseClick(&segment, Qt::LeftButton, Qt::NoModifier, QPoint(2, 2));
        }
        QCOMPARE(clicked.size(), 4);
        QCOMPARE(segment.accessibleName(), QStringLiteral("Core status"));
    }

    void connectionButtonsControlCoreWhileRadioIsOffline()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        QWebSocketServer listener(QStringLiteral("control test"), QWebSocketServer::NonSecureMode);
        QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
        connect(&listener, &QWebSocketServer::newConnection, &server, [&] {
            server.acceptTransport(new WebSocketTransport(listener.nextPendingConnection(),
                                                           StationServer::kMaxIncomingMessageBytes));
        });
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        const RemoteStationOptions options{
            QStringLiteral("ws://127.0.0.1:%1").arg(listener.serverPort()),
            server.token(), {}, true};
        RemoteConnectionController controls(&client, &remote, options);
        RemoteConnectionPanel panel(&controls);
        auto* dial = panel.findChild<QPushButton*>(QStringLiteral("connectCore"));
        auto* stop = panel.findChild<QPushButton*>(QStringLiteral("disconnectCore"));
        auto* details = panel.findChild<QLabel*>(QStringLiteral("coreConnectionDetails"));
        QVERIFY(dial && stop && details);
        QVERIFY(dial->isEnabled());
        QVERIFY(!stop->isEnabled());
        dial->click();
        QTRY_VERIFY(client.isHandshakeComplete());
        QVERIFY(!remote.isConnected());
        QCOMPARE(controls.state(), ConnectionState::Connected);
        QCOMPARE(controls.radioText(), QStringLiteral("Radio offline"));
        QVERIFY(details->text().contains(controls.endpointText()));
        QVERIFY(!dial->isEnabled());
        QVERIFY(stop->isEnabled());
        const QString refused = QStringLiteral("Receiver 2 (pan-1): no DDC is available. The saved layout is retained.");
        QVERIFY(remote.applyStationReceiveLayoutStatus("receiveLayoutRestoreState", "degraded"));
        QVERIFY(remote.applyStationReceiveLayoutStatus("receiveLayoutRestoreMessage", refused));
        QVERIFY(details->text().contains(refused));
        const auto epoch = client.sessionEpoch();
        controls.connectToStation(); // Duplicate surface must not replace the live client.
        QCOMPARE(client.sessionEpoch(), epoch);
        stop->click();
        QVERIFY(!client.isConnectionActive());
        QVERIFY(dial->isEnabled());
        QVERIFY(!stop->isEnabled());
        QCOMPARE(controls.statusText(), QStringLiteral("Core disconnected"));
        QVERIFY(!details->text().contains(refused)); // retained snapshot is stale while disconnected
        dial->click();
        QTRY_VERIFY(client.isHandshakeComplete());
        QVERIFY(client.sessionEpoch() > epoch);
        stop->click();
    }

    // R-R3-23 Task 4: the panel's "Remote audio" section over a real media
    // session. A real speaker fault (PacedAudioBus withdrawing its device
    // timing, never a receiver signal emitted directly) enables Retry;
    // clicking it sends a new "audio" control revision to Core.
    void remoteAudioSectionTracksARealFaultAndRetrySendsANewRevision()
    {
        using State = RemoteAudioStatus::State;
        Test::RemoteAudioSessionHarness h;
        RemoteMediaController remoteMedia(&h.client, &h.remote, nullptr);
        DaemonMediaController daemonMedia(&h.server, &h.station);
        RemoteConnectionController controls(&h.client, &h.remote, {});
        QSignalSpy coreControls(&h.server, &StationServer::mediaControlReceived);

        // The station's two-slice tone and this computer's speaker, each
        // paced every 10 ms, as the real audio session tests drive them.
        QTimer sourceTimer;
        sourceTimer.setInterval(10);
        sourceTimer.setTimerType(Qt::PreciseTimer);
        connect(&sourceTimer, &QTimer::timeout, &sourceTimer, [&h] { h.feedMixedTone(); });
        QTimer speakerTimer;
        speakerTimer.setInterval(10);
        speakerTimer.setTimerType(Qt::PreciseTimer);
        connect(&speakerTimer, &QTimer::timeout, &speakerTimer, [&h] {
            h.remoteBus->render(Test::RemoteAudioSessionHarness::kFrames);
        });
        sourceTimer.start();
        speakerTimer.start();

        h.connectSession();
        QTRY_VERIFY2_WITH_TIMEOUT(remoteMedia.audioStatus().state == State::Playing,
                                  h.mediaStage(remoteMedia).constData(),
                                  h.kMediaConnectionWaitMs);

        RemoteConnectionPanel panel(&controls, nullptr, &remoteMedia);
        auto* audioTimer = panel.findChild<QTimer*>(QStringLiteral("remoteAudioPanelTimer"));
        QVERIFY(audioTimer);
        QVERIFY(!audioTimer->isActive()); // built but never shown: no polling
        panel.show();
        QTRY_VERIFY(panel.isVisible());
        QVERIFY(audioTimer->isActive());
        auto* audioDetails = panel.findChild<QLabel*>(QStringLiteral("remoteAudioDetails"));
        auto* retry = panel.findChild<QPushButton*>(QStringLiteral("retryRemoteAudio"));
        QVERIFY(audioDetails);
        QVERIFY(retry);
        QVERIFY(audioDetails->text().contains(QStringLiteral("Remote audio: Playing")));
        QVERIFY(!retry->isEnabled());

        // The speaker goes away mid-play and stops reporting its timing.
        // The real receiver notices through AudioEngine; nothing fakes it.
        QTest::ignoreMessage(QtWarningMsg, remoteAudioSpeakerTimingLostLog());
        h.remoteBus->setOutputPacingAvailableForTesting(false);
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().state, State::PlaybackProblem, 5000);
        QTRY_VERIFY(retry->isEnabled());
        QVERIFY(audioDetails->text().contains(QStringLiteral("Problem:")));
        QVERIFY(audioDetails->text().contains(QStringLiteral("Arrival jitter")));

        // The rendering is saved under this test's own temporary directory.
        // Set NEREUS_REMOTE_PANEL_DUMP_DIR to also keep a copy for review;
        // that copy is best effort and never asserted.
        QCoreApplication::processEvents();
        QTemporaryDir captureDir;
        QVERIFY(captureDir.isValid());
        const QPixmap rendered = panel.grab();
        QVERIFY(!rendered.isNull());
        QVERIFY(rendered.save(captureDir.filePath(QStringLiteral("panel-capture.png")), "PNG"));
        const QString dumpDir = qEnvironmentVariable("NEREUS_REMOTE_PANEL_DUMP_DIR");
        if (!dumpDir.isEmpty()) {
            rendered.save(dumpDir + QStringLiteral("/panel-capture.png"), "PNG");
        }

        // The speaker is back; Retry asks Core again with a newer revision.
        h.remoteBus->setOutputPacingAvailableForTesting(true);
        const int audioRequestsBeforeRetry = countAudioControlMessages(coreControls);
        retry->click();
        QTRY_VERIFY(countAudioControlMessages(coreControls) > audioRequestsBeforeRetry);
        QTRY_COMPARE_WITH_TIMEOUT(remoteMedia.audioStatus().state, State::Playing, 15000);
        QVERIFY(!retry->isEnabled());
        QVERIFY(audioDetails->text().contains(QStringLiteral("Remote audio: Playing")));

        // The height follows the wrapped text: every section fits inside
        // the panel with no fixed gap left below the buttons.
        QCoreApplication::processEvents();
        const int contentHeight = panel.layout()->totalHeightForWidth(panel.width());
        QVERIFY(contentHeight > 0);
        QCOMPARE(panel.height(), contentHeight);
        auto* close = panel.findChild<QDialogButtonBox*>();
        QVERIFY(close);
        QVERIFY(audioDetails->geometry().bottom() < retry->geometry().top());
        QVERIFY(close->geometry().bottom() < panel.height());

        // Hidden, the panel stops polling; shown again, it resumes.
        panel.hide();
        QVERIFY(!audioTimer->isActive());
        panel.show();
        QTRY_VERIFY(panel.isVisible());
        QVERIFY(audioTimer->isActive());
        panel.hide();
        QVERIFY(!audioTimer->isActive());

        sourceTimer.stop();
        speakerTimer.stop();
        h.client.disconnectFromStation(QStringLiteral("test complete"));
    }

    void failedConnectShowsReasonAndCancelStopsBackoff()
    {
        QTcpServer port;
        QVERIFY(port.listen(QHostAddress::LocalHost, 0));
        const quint16 unusedPort = port.serverPort();
        port.close();
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setReconnectBackoffUnitMs(100);
        RemoteConnectionController controls(&client, &remote,
            {QStringLiteral("ws://127.0.0.1:%1").arg(unusedPort), {}, {}, true});
        QSignalSpy retries(&client, &StationClient::reconnectScheduled);
        controls.connectToStation();
        QTRY_VERIFY(client.isReconnectPending());
        QVERIFY(!client.lastError().isEmpty());
        QVERIFY(controls.detailText().contains(client.lastError()));
        QVERIFY(controls.canDisconnect());
        controls.disconnectFromStation();
        const auto epoch = client.sessionEpoch();
        const int scheduled = retries.size();
        // Observe past the cancelled timer's deadline: no stale retry may dial.
        QTest::qWait(150);
        QCOMPARE(client.sessionEpoch(), epoch);
        QCOMPARE(retries.size(), scheduled);
        QVERIFY(!client.isConnectionActive());
        QVERIFY(controls.canConnect());
        QVERIFY(!controls.detailText().contains(QStringLiteral("Last failure:")));
    }

    // The operator's ruling of 2026-09-26: the Core changes its radio by
    // restarting its run. The window takes that end as a reconnect, not a
    // failure: it says so in its own words beside the Core's, and is back
    // by itself.
    void aRadioChangeEndIsAReconnectWithTheCoresWords()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt SSL support is unavailable");
        }
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(station.addSlice(QStringLiteral("pan-0")) >= 0);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setReconnectBackoffUnitMs(400);
        RemoteConnectionController controls(&client, &remote,
            {QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()),
             server.token(), server.certificateFingerprint(), false});
        QSignalSpy handshakes(&client, &StationClient::handshakeComplete);
        controls.connectToStation();
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 1, 15000);

        const QString reason =
            QStringLiteral("The Core is switching to Bench G2. This app reconnects by itself.");
        server.endSessionsForRadioChange(reason);
        QTRY_VERIFY(client.isReconnectPending());
        QCOMPARE(client.radioChangeReason(), reason);
        QCOMPARE(controls.stopNotice(), CoreStopNotice::None);
        QCOMPARE(controls.statusText(), QStringLiteral("Core changing radio, reconnecting"));
        QVERIFY2(controls.detailText().contains(reason), qPrintable(controls.detailText()));
        QVERIFY(controls.detailText().contains(
            QStringLiteral("This window reconnects by itself when the Core is back.")));
        QVERIFY(!controls.detailText().contains(QStringLiteral("Last failure:")));
        QVERIFY(OperatorWording::isPlain(controls.statusText()));

        // Back by itself, with nothing left of the change.
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 2, 15000);
        QVERIFY(client.radioChangeReason().isEmpty());
        QCOMPARE(controls.statusText(), QStringLiteral("Core connected"));
        controls.disconnectFromStation();
    }

    // Follow-up N2: a Core that never comes back after a radio change. The
    // change reading holds through the backoff's steps, and the failed
    // redial after the longest (60 s) wait drops it, so the real failure
    // shows in the status, the detail and the toast.
    void aRadioChangeThatNeverComesBackShowsTheRealFailure()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt SSL support is unavailable");
        }
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(station.addSlice(QStringLiteral("pan-0")) >= 0);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setReconnectBackoffUnitMs(400);
        RemoteConnectionController controls(&client, &remote,
            {QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()),
             server.token(), server.certificateFingerprint(), false});
        QSignalSpy handshakes(&client, &StationClient::handshakeComplete);
        controls.connectToStation();
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 1, 15000);

        // What the window reads as each failed redial ends (sessionEnded
        // comes before the next retry is scheduled).
        QStringList readingAtEachEnd;
        connect(&client, &StationClient::sessionEnded, &client,
                [&client, &readingAtEachEnd](const QString&) {
                    readingAtEachEnd.append(client.radioChangeReason());
                });
        const QString reason =
            QStringLiteral("The Core is switching to Bench G2. This app reconnects by itself.");
        server.endSessionsForRadioChange(reason);
        QTRY_VERIFY(client.isReconnectPending());
        QCOMPARE(client.radioChangeReason(), reason);
        // The Core never comes back; the schedule shrinks so the test does
        // not wait out a minute.
        server.close();
        client.setReconnectBackoffUnitMs(5);

        // The radio-change end, then one failed redial per step: 1, 2, 5,
        // 10, 30 and 60. The redial after the 60 s wait is the seventh end.
        QTRY_VERIFY_WITH_TIMEOUT(readingAtEachEnd.size() >= 7, 30000);
        for (int i = 0; i < 6; ++i) {
            QCOMPARE(readingAtEachEnd.at(i), reason);
        }
        QVERIFY(readingAtEachEnd.at(6).isEmpty());
        QVERIFY(client.radioChangeReason().isEmpty());
        QTRY_VERIFY(client.isReconnectPending());
        QCOMPARE(controls.statusText().left(14), QStringLiteral("Retrying Core "));
        QVERIFY2(controls.detailText().contains(QStringLiteral("Last failure:")),
                 qPrintable(controls.detailText()));
        QVERIFY(!controls.detailText().contains(reason));
        controls.disconnectFromStation();
    }

    // Follow-up N2: the Core answers the redial with some other end. That
    // end is the reading from then on, not the radio change.
    void anotherEndAfterARadioChangeIsShownAsItself()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt SSL support is unavailable");
        }
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(station.addSlice(QStringLiteral("pan-0")) >= 0);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        // One wrong token locks sign-in, which the Core answers with a
        // retryable refusal.
        server.setAuthRateLimit(1, 60000);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));
        const QUrl url(QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setReconnectBackoffUnitMs(50);
        RemoteConnectionController controls(&client, &remote,
            {url.toString(), server.token(), server.certificateFingerprint(), false});
        QSignalSpy handshakes(&client, &StationClient::handshakeComplete);
        controls.connectToStation();
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 1, 15000);

        // A stranger's wrong token locks sign-in; the window's session
        // stays up.
        RadioModel strangerModel(RadioModel::Role::Remote);
        SettingsProxy strangerProxy;
        StationClient stranger(&strangerModel, &strangerProxy);
        QSignalSpy strangerEnded(&stranger, &StationClient::sessionEnded);
        stranger.connectToStation(url, QStringLiteral("not-the-token"),
                                  server.certificateFingerprint());
        QTRY_COMPARE_WITH_TIMEOUT(strangerEnded.count(), 1, 15000);
        QVERIFY(client.isHandshakeComplete());

        QSignalSpy ended(&client, &StationClient::sessionEnded);
        const QString reason =
            QStringLiteral("The Core is switching to Bench G2. This app reconnects by itself.");
        server.endSessionsForRadioChange(reason);
        QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 15000);
        QCOMPARE(client.radioChangeReason(), reason);

        // The window's own redial meets the lockout.
        QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 2, 15000);
        QVERIFY2(client.lastError().contains(QStringLiteral("too many wrong ones")),
                 qPrintable(client.lastError()));
        QVERIFY(client.radioChangeReason().isEmpty());
        QVERIFY(client.isReconnectPending());
        QVERIFY(controls.statusText() != QStringLiteral("Core changing radio, reconnecting"));
        QVERIFY2(controls.detailText().contains(QStringLiteral("Last failure:")),
                 qPrintable(controls.detailText()));
        controls.disconnectFromStation();
        server.close();
    }

    void mediaRecoveryUsesPinnedCoreReconnectAndRetainsSlice()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt SSL support is unavailable");
        }
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        const int sliceId = station.addSlice(QStringLiteral("pan-0"));
        QVERIFY(sliceId >= 0);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setReconnectBackoffUnitMs(50);
        RemoteConnectionController controls(&client, &remote,
            {QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()),
             server.token(), server.certificateFingerprint(), false});
        QPointer<RecoveryMediaTransport> transport;
        RemoteMediaController media(&client, &remote, nullptr, nullptr,
            [&transport](QObject* owner) -> IMediaTransport* {
                transport = new RecoveryMediaTransport(owner);
                return transport;
            });
        connect(&media, &RemoteMediaController::recoveryRequested,
                &controls, &RemoteConnectionController::recoverMediaSession,
                Qt::QueuedConnection);
        QSignalSpy handshakes(&client, &StationClient::handshakeComplete);
        QSignalSpy retries(&client, &StationClient::reconnectScheduled);

        controls.connectToStation();
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 1, 15000);
        QTRY_VERIFY(transport);
        transport->activate();
        SliceModel* const retained = remote.sliceById(sliceId);
        QVERIFY(retained);
        retained->setFrequency(retained->frequency() + 731.0);
        QTRY_COMPARE(station.sliceById(sliceId)->frequency(), retained->frequency());
        const double retainedFrequency = retained->frequency();
        const quint32 failedEpoch = client.sessionEpoch();

        // Drive the production chain: transport close -> MediaPeer close ->
        // RemoteMediaController recovery -> queued connection controller.
        transport->closeUnexpectedly();
        QTRY_VERIFY_WITH_TIMEOUT(!retries.isEmpty(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 2, 15000);
        QVERIFY(client.sessionEpoch() > failedEpoch);
        QCOMPARE(remote.sliceById(sliceId), retained);
        QCOMPARE(retained->frequency(), retainedFrequency);
        QCOMPARE(station.slices().size(), 1);
        QCOMPARE(remote.slices().size(), 1);

        // A stale close from the retired media epoch must not disturb the
        // newly authenticated session.
        const quint32 secondEpoch = client.sessionEpoch();
        const int retriesAfterRecovery = retries.size();
        emit media.recoveryRequested(failedEpoch,
                                     QStringLiteral("stale media connection closed"));
        QTest::qWait(100);
        QCOMPARE(client.sessionEpoch(), secondEpoch);
        QCOMPARE(retries.size(), retriesAfterRecovery);
        QVERIFY(client.isHandshakeComplete());

        // Peer failure and its trailing close can enqueue the same recovery
        // twice. Only one station teardown/reconnect is admitted per epoch.
        emit media.recoveryRequested(secondEpoch, QStringLiteral("media peer failed"));
        emit media.recoveryRequested(secondEpoch, QStringLiteral("media peer closed"));
        QTRY_COMPARE_WITH_TIMEOUT(retries.size(), retriesAfterRecovery + 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 3, 15000);
        QCOMPARE(remote.sliceById(sliceId), retained);
        QCOMPARE(retained->frequency(), retainedFrequency);
        QCOMPARE(station.slices().size(), 1);
        QCOMPARE(remote.slices().size(), 1);

        // The operator can cancel after recovery is queued but before its
        // deferred teardown runs. The operator's intent wins.
        const quint32 thirdEpoch = client.sessionEpoch();
        emit media.recoveryRequested(thirdEpoch,
                                     QStringLiteral("media peer connection failed"));
        controls.disconnectFromStation();
        const int retryCount = retries.size();
        QTest::qWait(100);
        QCOMPARE(retries.size(), retryCount);
        QVERIFY(!client.isConnectionActive());
    }

    // R-R3-28 through the production chain: media that fails before ready
    // after each good Core handshake retries with a growing delay, and only
    // established media starts the schedule over.
    void repeatedMediaFailuresBackOffUntilMediaIsEstablished()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt SSL support is unavailable");
        }
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(station.addSlice(QStringLiteral("pan-0")) >= 0);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        constexpr int kUnitMs = 50;
        client.setReconnectBackoffUnitMs(kUnitMs);
        RemoteConnectionController controls(&client, &remote,
            {QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()),
             server.token(), server.certificateFingerprint(), false});
        QPointer<RecoveryMediaTransport> transport;
        int built = 0;
        RemoteMediaController media(&client, &remote, nullptr, nullptr,
            [&transport, &built](QObject* owner) -> IMediaTransport* {
                ++built;
                transport = new RecoveryMediaTransport(owner);
                return transport;
            });
        connect(&media, &RemoteMediaController::recoveryRequested,
                &controls, &RemoteConnectionController::recoverMediaSession,
                Qt::QueuedConnection);
        QSignalSpy handshakes(&client, &StationClient::handshakeComplete);
        QSignalSpy retries(&client, &StationClient::reconnectScheduled);
        const auto delays = [&retries] {
            QList<int> values;
            for (const auto& call : retries) { values.append(call.at(1).toInt()); }
            return values;
        };

        controls.connectToStation();
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 1, 15000);
        for (int failure = 1; failure <= 3; ++failure) {
            QTRY_COMPARE(built, failure);
            QVERIFY(transport);
            transport->failConnection(QStringLiteral("media peer connection failed"));
            QTRY_COMPARE_WITH_TIMEOUT(retries.size(), failure, 5000);
            QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), failure + 1, 15000);
        }
        QCOMPARE(delays(), QList<int>({1 * kUnitMs, 2 * kUnitMs, 5 * kUnitMs}));

        // Media that becomes ready resets the schedule: its later loss
        // retries at the first step again.
        QTRY_COMPARE(built, 4);
        QVERIFY(transport);
        transport->activate();
        transport->closeUnexpectedly();
        QTRY_COMPARE_WITH_TIMEOUT(retries.size(), 4, 5000);
        QCOMPARE(delays().constLast(), 1 * kUnitMs);
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 5, 15000);

        // Manual Disconnect during the backoff wait cancels it. A longer
        // unit keeps the wait open while the poll below notices it.
        QTRY_COMPARE(built, 5);
        QVERIFY(transport);
        constexpr int kLongUnitMs = 1000;
        client.setReconnectBackoffUnitMs(kLongUnitMs);
        transport->failConnection(QStringLiteral("media peer connection failed"));
        QTRY_COMPARE_WITH_TIMEOUT(retries.size(), 5, 5000);
        QCOMPARE(delays().constLast(), 2 * kLongUnitMs);
        QVERIFY(client.isReconnectPending());
        controls.disconnectFromStation();
        QTest::qWait(3 * kLongUnitMs);
        QCOMPARE(retries.size(), 5);
        QCOMPARE(handshakes.size(), 5);
        QVERIFY(!client.isConnectionActive());
    }

    // R-R3-28, review I1. Media that ends without a retry (Core refuses the
    // start, a media error after start, or an error reported on the way
    // through start) leaves a control-only session that has proven itself.
    // Its backoff must start over, so a later, unrelated control drop
    // retries at the first step rather than where earlier media failures
    // left the schedule.
    void mediaEndedWithoutRetryResetsTheBackoff_data()
    {
        QTest::addColumn<int>("ending");
        QTest::newRow("Core rejects the media start") << int(kCoreRejectsStart);
        QTest::newRow("media error after start") << int(kErrorAfterStart);
        QTest::newRow("error reported during start") << int(kErrorDuringStart);
    }

    void mediaEndedWithoutRetryResetsTheBackoff()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt SSL support is unavailable");
        }
        QFETCH(int, ending);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(station.addSlice(QStringLiteral("pan-0")) >= 0);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        bool coreRefuses = false;
        int coreStarts = 0;
        // Core answers the media start; with coreRefuses its peer cannot
        // start, and Core sends "rejected" for the connection.
        DaemonMediaController core(&server, &station, nullptr,
            [&coreRefuses, &coreStarts](QObject* owner) -> IMediaTransport* {
                ++coreStarts;
                return coreRefuses ? nullptr : new RecoveryMediaTransport(owner);
            });
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        constexpr int kUnitMs = 20;
        client.setReconnectBackoffUnitMs(kUnitMs);
        RemoteConnectionController controls(&client, &remote,
            {QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()),
             server.token(), server.certificateFingerprint(), false});
        QPointer<RecoveryMediaTransport> transport;
        int built = 0;
        bool errorOnStart = false;
        RemoteMediaController media(&client, &remote, nullptr, nullptr,
            [&transport, &built, &errorOnStart](QObject* owner) -> IMediaTransport* {
                ++built;
                transport = new RecoveryMediaTransport(owner);
                transport->errorOnStart = errorOnStart;
                return transport;
            });
        connect(&media, &RemoteMediaController::recoveryRequested,
                &controls, &RemoteConnectionController::recoverMediaSession,
                Qt::QueuedConnection);
        QSignalSpy handshakes(&client, &StationClient::handshakeComplete);
        QSignalSpy retries(&client, &StationClient::reconnectScheduled);
        QSignalSpy errors(&media, &RemoteMediaController::errorOccurred);

        // Two media failures after good handshakes climb the schedule.
        controls.connectToStation();
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 1, 15000);
        for (int failure = 1; failure <= 2; ++failure) {
            QTRY_COMPARE(built, failure);
            QVERIFY(transport);
            transport->failConnection(QStringLiteral("media peer connection failed"));
            QTRY_COMPARE_WITH_TIMEOUT(retries.size(), failure, 5000);
            QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), failure + 1, 15000);
        }
        QCOMPARE(retries.constLast().at(1).toInt(), 2 * kUnitMs);

        // The third session's media ends without a retry.
        QString reason;
        if (ending == kCoreRejectsStart) {
            // Core refuses the next session's start, once this session's
            // own start has reached Core. Retire this session's media, so
            // the next session is the one Core refuses.
            QTRY_COMPARE(coreStarts, 3);
            coreRefuses = true;
            transport->failConnection(QStringLiteral("media peer connection failed"));
            QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 4, 15000);
            reason = QStringLiteral("The Core could not start audio and display.");
        } else if (ending == kErrorDuringStart) {
            errorOnStart = true;
            transport->failConnection(QStringLiteral("media peer connection failed"));
            QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 4, 15000);
            reason = QStringLiteral("test error during start");
        } else {
            transport->failConnection(QStringLiteral("media peer connection failed"));
            QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 4, 15000);
            QTRY_COMPARE(built, 4);
            QVERIFY(transport);
            reason = QStringLiteral("invalid media packet");
            transport->reportError(reason);
        }
        QCOMPARE(retries.constLast().at(1).toInt(), 5 * kUnitMs);
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            for (const auto& call : errors) {
                if (call.at(0).toString() == reason) { return true; }
            }
            return false;
        }(), 5000);
        QCoreApplication::processEvents();
        QCOMPARE(retries.size(), 3);
        QVERIFY(client.isHandshakeComplete());

        // A later, unrelated control drop retries at the first step.
        client.disconnectFromStation(QStringLiteral("station link lost"), true);
        QCOMPARE(retries.size(), 4);
        QCOMPARE(retries.constLast().at(1).toInt(), 1 * kUnitMs);
        controls.disconnectFromStation();
        QVERIFY(!client.isReconnectPending());
    }

    // R-R3-28, amended 2026-09-23. A transport that cannot be built is
    // retried, with the schedule growing, until retries reach the backoff
    // ceiling. The next such refusal stops retrying and leaves a
    // control-only session with the reason shown.
    void transientStartRefusalStopsRetryingAtTheBackoffCeiling()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt SSL support is unavailable");
        }
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::Saturn);
        station.configureStreamPool(5, 5, 192000);
        station.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(station.addSlice(QStringLiteral("pan-0")) >= 0);
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        server.setMediaEnabled(true);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

        RadioModel remote(RadioModel::Role::Remote);
        remote.audioEngine()->setMasterMuted(true);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        constexpr int kUnitMs = 10;
        client.setReconnectBackoffUnitMs(kUnitMs);
        RemoteConnectionController controls(&client, &remote,
            {QStringLiteral("wss://127.0.0.1:%1").arg(server.serverPort()),
             server.token(), server.certificateFingerprint(), false});
        int built = 0;
        RemoteMediaController media(&client, &remote, nullptr, nullptr,
            [&built](QObject* owner) -> IMediaTransport* {
                ++built;
                return new BuildFailingMediaTransport(owner);
            });
        connect(&media, &RemoteMediaController::recoveryRequested,
                &controls, &RemoteConnectionController::recoverMediaSession,
                Qt::QueuedConnection);
        QSignalSpy handshakes(&client, &StationClient::handshakeComplete);
        QSignalSpy retries(&client, &StationClient::reconnectScheduled);
        QSignalSpy errors(&media, &RemoteMediaController::errorOccurred);
        const auto delays = [&retries] {
            QList<int> values;
            for (const auto& call : retries) { values.append(call.at(1).toInt()); }
            return values;
        };
        const QString reason = QStringLiteral(
            "Station media could not start on this computer: could not create the peer connection");

        controls.connectToStation();
        // Every step of the schedule once, the ceiling included, then the
        // session after the ceiling wait keeps control and stops.
        QTRY_COMPARE_WITH_TIMEOUT(handshakes.size(), 7, 30000);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 7, 5000);
        QTest::qWait(3 * 60 * kUnitMs);
        QCOMPARE(delays(), QList<int>({1 * kUnitMs, 2 * kUnitMs, 5 * kUnitMs,
                                       10 * kUnitMs, 30 * kUnitMs, 60 * kUnitMs}));
        QCOMPARE(built, 7);
        QCOMPARE(handshakes.size(), 7);
        QCOMPARE(errors.constLast().at(0).toString(), reason);
        QVERIFY(client.isHandshakeComplete());
        QVERIFY(!client.isReconnectPending());

        // That control-only session proved itself: a later drop starts over.
        client.disconnectFromStation(QStringLiteral("station link lost"), true);
        QCOMPARE(delays().constLast(), 1 * kUnitMs);
        controls.disconnectFromStation();
        QVERIFY(!client.isReconnectPending());
    }

    void pairedLearnedListenersAllowServiceOffConnection()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        RemoteStationOptions options;
        options.identityFingerprint = QByteArray(32, 'k');
        options.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        options.reachFromAnywhere = false;
        options.coreAddresses = {QStringLiteral("wss://[2001:db8::5]:47912")};
        RemoteConnectionController controls(&client, &remote, options);
        QVERIFY(GuiConnectionController::isReadyToConnect(options));
        QVERIFY(controls.canConnect());
        controls.connectToStation();
        QCOMPARE(client.cachedAddresses(), QList<QUrl>{QUrl(options.coreAddresses.first())});
        QVERIFY(client.serviceRoute().servers.isEmpty());
        QVERIFY(!client.connectionAttempt().tries.isEmpty());
        QCOMPARE(client.connectionAttempt().tries.first().path, StationConnectionAttempt::Path::Direct);
        QCOMPARE(options.url, QString());
        controls.disconnectFromStation();
        // Existing service availability never overrides authenticated direct listeners.
        for (const int version : {-1, 0, 1}) {
            RemoteStationOptions gated = options;
            gated.reachFromAnywhere = true;
            gated.controlChannelVersion = version;
            gated.negativeControlObservedMs = QDateTime::currentMSecsSinceEpoch();
            QVERIFY(GuiConnectionController::isReadyToConnect(gated));
            RemoteConnectionController next(&client, &remote, gated);
            QVERIFY(next.canConnect());
        }
        AppSettings::instance().setValue(QStringLiteral("RemoteAccessServers"), QStringLiteral("https://invalid.test"));
        QVERIFY(configuredRemoteAccessServers().isEmpty());
        RemoteStationOptions unavailable = options;
        unavailable.reachFromAnywhere = true;
        QVERIFY(GuiConnectionController::isReadyToConnect(unavailable));
        RemoteConnectionController withDirect(&client, &remote, unavailable);
        QVERIFY(withDirect.canConnect());
        for (const QByteArray& identity : {QByteArray(), QByteArray(31, 'k')}) {
            RemoteStationOptions invalid = options;
            invalid.identityFingerprint = identity;
            QVERIFY(!GuiConnectionController::isReadyToConnect(invalid));
            RemoteConnectionController blocked(&client, &remote, invalid);
            QVERIFY(!blocked.canConnect());
        }
        for (int fault = 0; fault < 3; ++fault) {
            RemoteStationOptions invalid = options;
            if (fault == 0) { invalid.allowUnpinned = true; }
            if (fault == 1) { invalid.token = QStringLiteral("fixture-token"); }
            if (fault == 2) { invalid.coreAddresses.clear(); }
            QVERIFY(!GuiConnectionController::isReadyToConnect(invalid));
            RemoteConnectionController blocked(&client, &remote, invalid);
            QVERIFY(!blocked.canConnect());
        }
    }

    void displayedEndpointExcludesCredentials()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        RemoteConnectionController controls(&client, &remote,
            {QStringLiteral("wss://name:secret@[::1]:50055/path?token=secret#secret"),
             QStringLiteral("private-token"), {}, false});
        QCOMPARE(controls.endpointText(), QStringLiteral("[::1]:50055"));
        QVERIFY(!controls.detailText().contains(QStringLiteral("secret")));
        QVERIFY(!controls.detailText().contains(QStringLiteral("private-token")));
    }

    void addresslessCoreShowsRemoteAccessAndRefusesWhenDisabled()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        RemoteStationOptions options;
        options.identityFingerprint = QByteArray(32, 'k');
        options.rendezvousId = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        options.reachFromAnywhere = false;
        RemoteConnectionController controls(&client, &remote, options);
        QCOMPARE(controls.endpointText(), QStringLiteral("Remote access"));
        QVERIFY(!controls.canConnect());
        QVERIFY(controls.detailText().contains(QStringLiteral("Turn on remote access")));
        controls.connectToStation();
        QVERIFY(!client.isConnectionActive());
        QVERIFY(client.connectionAttempt().summary().isEmpty());
    }

    // The extra-slice startup reproduction that used to live here copied
    // MainWindow's populateEmptyPans hook into a lambda. It now runs through
    // the real window: tst_remote_window_harness
    // heldSnapshotCreatesNoSliceOnConnectOrReconnect (R-R3-24).

    void automaticPanRestorationNeverCreatesStationSlices()
    {
        RecordingStationLink link;
        RadioModel model(RadioModel::Role::Remote);
        model.attachStation(&link);
        model.setConnectionStateForTest(ConnectionState::Connected);
        // Capability exchange can say radio-connected before any slice
        // snapshot arrives. This is the production startup population path.
        MainWindow::populatePanSlices(&model, {QStringLiteral("pan-0")}, false, false);
        QCOMPARE(link.additions.size(), 0);
        MainWindow::populatePanSlices(&model, {QStringLiteral("pan-0")}, false, true);
        QCOMPARE(link.additions.size(), 0);
    }

    void explicitPopulationWaitsForSnapshotThenCreates()
    {
        RecordingStationLink link;
        RadioModel model(RadioModel::Role::Remote);
        model.attachStation(&link);
        model.setConnectionStateForTest(ConnectionState::Connected);
        MainWindow::populatePanSlices(&model, {QStringLiteral("pan-0")}, true, false);
        QCOMPARE(link.additions.size(), 0);
        MainWindow::populatePanSlices(&model, {QStringLiteral("pan-0")}, true, true);
        QCOMPARE(link.additions, QStringList{QStringLiteral("pan-0")});
    }
};

QTEST_MAIN(TestRemoteConnectionControls)
#include "tst_remote_connection_controls.moc"
