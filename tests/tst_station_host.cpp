// no-port-check: NereusSDR-original. Task 48, step 1. No radio hardware is opened here.
//
// Modification history (NereusSDR):
//   2026-10-22: NSDC v2 negotiation: RemoteDisplayCodecV2 is read at start.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "UnusedLoopbackPort.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/ConnectionState.h"
#include "core/daemon/DaemonTelemetryController.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/ConnectedDevicesFacade.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/SliceOwnership.h"
#include "core/settings/SettingsProxy.h"
#include "core/station/StationHost.h"
#include "core/session/media/DaemonMediaController.h"
#include "core/settings/SettingsScope.h"
#include "fakes/LoopbackTransport.h"
#include <QJsonDocument>
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"

#include <QPointer>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace NereusSDR;

namespace {
StationHostOptions listenerOptions()
{
    StationHostOptions options;
    options.settings = &AppSettings::instance();
    options.remoteBind = QStringLiteral("127.0.0.1");
    options.statusPage = false;
    options.rendezvousServers.clear();
    options.remotePort = unusedLoopbackPort();
    return options;
}
}

class TstStationHost : public QObject {
    Q_OBJECT
private slots:
    void hostingDeviceIsFirstPlaceBeforeAnyListener()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        const int unowned = model.addSlice(QStringLiteral("pan-0"));
        const int foreign = model.addSlice(QStringLiteral("pan-0"));
        QVERIFY(unowned >= 0 && foreign >= 0);
        model.sliceOwnership()->setOwner(foreign, QByteArray("peer"));
        StationHostOptions options = listenerOptions();
        options.settings = &settings;
        options.securityDirectory = directory.path();
        options.hostingDevice = StationHostOptions::HostingDevice{
            QStringLiteral("Shack Mac mini"), QStringLiteral("Shack")};
        StationHost host(&model, options);
        bool checkedBeforeListen = false;
        host.setServerCreatedForTest([&](StationServer* server) {
            QVERIFY(!server->isListening());
            QCOMPARE(server->deviceSessions()->placesTaken(), 1);
            const auto entry = server->deviceSessions()->entries().first();
            QCOMPARE(entry.kind, DeviceSessionRegistry::Kind::Hosting);
            QCOMPARE(entry.deviceId, SliceOwnership::stationDevice());
            QCOMPARE(entry.name, QStringLiteral("Shack Mac mini"));
            const QJsonArray visible = QJsonDocument::fromJson(
                server->connectedDevices()->listJson().toUtf8()).array();
            QCOMPARE(visible.size(), 1);
            QVERIFY(visible.first().toObject().value(QStringLiteral("hostsCore")).toBool());
            QVERIFY(!visible.first().toObject().value(QStringLiteral("revocable")).toBool());
            QCOMPARE(model.sliceOwnership()->mark(unowned).owner,
                     SliceOwnership::stationDevice());
            QCOMPARE(model.sliceOwnership()->mark(foreign).owner, QByteArray("peer"));
            checkedBeforeListen = true;
        });
        QVERIFY(host.start());
        QVERIFY(checkedBeforeListen);
        QVERIFY(host.listenerReady());
        const int createdAfterListen = model.addSlice(QStringLiteral("pan-0"));
        QVERIFY(createdAfterListen >= 0);
        QCOMPARE(model.sliceOwnership()->mark(createdAfterListen).owner,
                 SliceOwnership::stationDevice());
        QObject first, second, third, fourth;
        const QObject* sessions[] = {&first, &second, &third, &fourth};
        for (int i = 0; i < 4; ++i) {
            DeviceSessionRegistry::Entry device;
            device.deviceId = QByteArray("device-") + QByteArray::number(i);
            device.kind = DeviceSessionRegistry::Kind::Paired;
            device.name = QStringLiteral("Phone %1").arg(i);
            const auto admission = host.server()->deviceSessions()->admit(device, sessions[i]);
            QCOMPARE(admission.admission, i < 3 ? DeviceSessionRegistry::Admission::Admitted
                                                : DeviceSessionRegistry::Admission::Full);
        }
        QCOMPARE(host.server()->deviceSessions()->placesTaken(), 4);
        host.stop();
        QCOMPARE(model.sliceOwnership()->mark(foreign).owner, QByteArray("peer"));
    }

    // Bench setting (JJ approved 2026-10-22): the Core's own
    // DisplayKeyframeEveryFrame reaches the media hub as the host starts;
    // only "True" turns it on, and the settings proxy never carries it.
    void displayKeyframeEveryFrameIsReadAtStart_data()
    {
        QTest::addColumn<QString>("stored");
        QTest::addColumn<bool>("expected");
        QTest::newRow("unset") << QString() << false;
        QTest::newRow("False") << QStringLiteral("False") << false;
        QTest::newRow("True") << QStringLiteral("True") << true;
        QTest::newRow("lower-case true") << QStringLiteral("true") << false;
    }

    void displayKeyframeEveryFrameIsReadAtStart()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QFETCH(QString, stored);
        QFETCH(bool, expected);
        QCOMPARE(QString::fromLatin1(kDisplayKeyframeEveryFrameSettingsKey),
                 QStringLiteral("DisplayKeyframeEveryFrame"));
        QCOMPARE(classifySettingsKey(QString::fromLatin1(kDisplayKeyframeEveryFrameSettingsKey)),
                 SettingsScope::OperatorLocal);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        if (!stored.isNull()) {
            settings.setValue(QLatin1String(kDisplayKeyframeEveryFrameSettingsKey), stored);
        }
        RadioModel model;
        StationHostOptions options = listenerOptions();
        options.settings = &settings;
        options.securityDirectory = directory.path();
        StationHost host(&model, options);
        QVERIFY(host.start());
        QVERIFY(host.mediaHubForTest() != nullptr);
        QCOMPARE(host.mediaHubForTest()->displayKeyframeEveryFrame(), expected);
        host.stop();
    }

    // Display codec v2 (NSDC v2): the Core's fallback switch reaches the
    // server as the host starts; absent and "True" leave it on, anything
    // else turns it off, and the settings proxy never carries it.
    void remoteDisplayCodecV2IsReadAtStart_data()
    {
        QTest::addColumn<QString>("stored");
        QTest::addColumn<bool>("expected");
        QTest::newRow("absent") << QString() << true;
        QTest::newRow("True") << QStringLiteral("True") << true;
        QTest::newRow("False") << QStringLiteral("False") << false;
        QTest::newRow("lower-case true") << QStringLiteral("true") << false;
        QTest::newRow("0") << QStringLiteral("0") << false;
    }

    void remoteDisplayCodecV2IsReadAtStart()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QFETCH(QString, stored);
        QFETCH(bool, expected);
        QCOMPARE(QString::fromLatin1(kRemoteDisplayCodecV2SettingsKey),
                 QStringLiteral("RemoteDisplayCodecV2"));
        QCOMPARE(classifySettingsKey(QStringLiteral("RemoteDisplayCodecV2")),
                 SettingsScope::OperatorLocal);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        if (!stored.isNull()) {
            settings.setValue(QLatin1String(kRemoteDisplayCodecV2SettingsKey), stored);
        }
        RadioModel model;
        StationHostOptions options = listenerOptions();
        options.settings = &settings;
        options.securityDirectory = directory.path();
        StationHost host(&model, options);
        bool serverMade = false;
        host.setServerCreatedForTest([&](StationServer* server) {
            // The server is made with the default; the host sets it before
            // the server listens.
            QVERIFY(!server->isListening());
            QVERIFY(server->displayCodecV2Enabled());
            serverMade = true;
        });
        QVERIFY(host.start());
        QVERIFY(serverMade);
        QVERIFY(host.server() != nullptr);
        QCOMPARE(host.server()->displayCodecV2Enabled(), expected);
        host.stop();
    }

    void borrowedModelSurvivesStopAndRestart()
    {
        RadioModel model;
        QPointer<RadioModel> borrowed(&model);
        StationHostOptions options;
        options.settings = &AppSettings::instance();
        options.remotePort = 0;
        options.remoteTransmitAllowed = false;
        StationHost host(&model, options);
        QVERIFY(host.server() == nullptr); // construction has no server/profile work
        QVERIFY(host.start());
        QVERIFY(host.server() == nullptr);
        QVERIFY(model.pcCaptureAllowed());
        QVERIFY(model.audioEngine()->vaxOutputsAllowed());
        QVERIFY(!model.receiveOnlyStationPolicy());
        host.stop();
        QVERIFY(borrowed == &model);
        QVERIFY(host.start());
        host.stop();
        QVERIFY(borrowed == &model);
    }

    void syntheticConnectionChangesAnnouncement()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        RadioModel model;
        StationHostOptions options = listenerOptions();
        QVERIFY(options.remotePort > 0);
        StationHost host(&model, options);
        QVERIFY(host.start());
        QVERIFY(host.listenerReady());
        QVERIFY(!host.stationAnnouncementForTest().radioConnected);
        model.setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(host.stationAnnouncementForTest().radioConnected);
        model.setConnectionStateForTest(ConnectionState::Disconnected);
        QVERIFY(!host.stationAnnouncementForTest().radioConnected);
        host.stop();
        QVERIFY(host.server() == nullptr);
    }

    void earlyBorrowedModelDestructionClosesHost()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        auto model = std::make_unique<RadioModel>();
        StationHostOptions options = listenerOptions();
        StationHost host(model.get(), options);
        QVERIFY(host.start());
        QVERIFY(host.server() != nullptr);
        model.reset();
        QVERIFY(host.server() == nullptr);
        QVERIFY(!host.listenerReady());
        host.stop();
    }

    void stopReenteredFromListenerCloseFinishes()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        RadioModel model;
        StationHostOptions options = listenerOptions();
        StationHost host(&model, options);
        QVERIFY(host.start());
        StationServer* server = host.server();
        QVERIFY(server && server->isListening());
        int callbacks = 0;
        connect(server, &StationServer::listeningChanged, &host, [&](bool listening) {
            if (!listening) {
                ++callbacks;
                host.stop();
            }
        });
        host.stop();
        QCOMPARE(callbacks, 1);
        QVERIFY(host.server() == nullptr);
        QVERIFY(!host.listenerRetryPending());
        QVERIFY(host.start());
        host.stop();
    }

    void quiesceDuringListenDoesNotStartRendezvous()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        RadioModel model;
        StationHostOptions options = listenerOptions();
        StationHost host(&model, options);
        int callbacks = 0;
        host.setServerCreatedForTest([&](StationServer* server) {
            connect(server, &StationServer::listeningChanged, &host, [&](bool listening) {
                if (listening) {
                    ++callbacks;
                    host.quiesce();
                }
            });
        });
        QVERIFY(!host.start());
        QCOMPARE(callbacks, 1);
        QVERIFY(host.server() != nullptr); // quiesce retains borrowed dependencies
        QVERIFY(!host.listenerReady());
        QVERIFY(!host.listenerRetryPending());
        host.stop();
        QVERIFY(host.server() == nullptr);
    }

    void endedTelemetryPendingDeletionSurvivesBorrowedModelDeath()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt reports no working TLS backend.");
        }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        Test::seedUpgradedCoreToken(directory.path());
        auto station = std::make_unique<RadioModel>();
        StationHostOptions options = listenerOptions();
        options.settings = &settings;
        options.securityDirectory = directory.path();
        StationHost host(station.get(), options);
        QVERIFY(host.start());
        StationServer* server = host.server();
        QVERIFY(server && server->isListening());
        const QString token = server->token();
        QVERIFY(!token.isEmpty());

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* stationLink = new Test::LoopbackTransport(QStringLiteral("host-station"), &host);
        auto* clientLink = new Test::LoopbackTransport(QStringLiteral("host-client"), &host);
        stationLink->linkTo(clientLink);
        client.startSession(clientLink, token);
        server->acceptTransport(stationLink);
        QTRY_VERIFY(client.telemetryAvailable());
        QTRY_VERIFY(!host.findChildren<DaemonTelemetryController*>().isEmpty());
        DaemonTelemetryController* controller = host.findChild<DaemonTelemetryController*>();
        QVERIFY(controller && controller->isCollecting());
        const quint64 epoch = controller->sessionEpoch();
        QVERIFY(epoch != 0);

        // Simulate the session-ended emission: Host releases the controller
        // from its map and schedules deleteLater, while its own slot stops
        // sampling. Do not process DeferredDelete before removing the model.
        emit server->telemetrySessionEnded(epoch);
        QPointer<DaemonTelemetryController> pending(controller);
        QVERIFY(pending);
        QVERIFY(!controller->isCollecting());
        station.reset(); // Host closes and destroys its server, never the model.
        QVERIFY(host.server() == nullptr);
        controller->sampleNow(); // inert with both borrowed pointers retired
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(pending.isNull());
    }
};

QTEST_MAIN(TstStationHost)
#include "tst_station_host.moc"
