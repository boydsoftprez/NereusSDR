// no-port-check: NereusSDR-original direct paired watch integration test.
#include <QtTest>

#include <QHostAddress>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/session/RemoteTransmitClient.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "gui/RemoteTelemetryController.h"
#include "models/RadioModel.h"
#include "fakes/DataChannelPair.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

class TestStationTxWatchDirect : public QObject {
    Q_OBJECT
private slots:
    void pairedDataChannelDoesNotRequestDirectWatch()
    {
        QTemporaryDir settingsDir;
        QTemporaryDir securityDir;
        QTemporaryDir deviceDir;
        QVERIFY(settingsDir.isValid());
        QVERIFY(securityDir.isValid());
        QVERIFY(deviceDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.addSlice(QStringLiteral("pan-0"));
        StationServer server(&station, settings, Test::seedCoreIdentity(securityDir.path()));
        server.setRemoteTransmitAllowed(true);
        const auto device = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(deviceDir.path()));
        QVERIFY(device->isValid());
        PairedDevice record;
        record.id = device->fingerprint();
        record.publicKeySpki = device->publicKeySpki();
        record.name = QStringLiteral("Watch desktop");
        record.kind = QStringLiteral("computer");
        QVERIFY(server.deviceStore()->add(record));
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

        auto* offerer = new DataChannelTransport;
        auto* answerer = new DataChannelTransport;
        QSignalSpy inbound(answerer, &SessionTransport::textReceived);
        QVERIFY(Test::startDataChannelPair(offerer, answerer,
                                          StationClient::kMaxIncomingMessageBytes,
                                          StationServer::kMaxIncomingMessageBytes,
                                          server.certificatePemPath(),
                                          server.privateKeyPemPath()));
        QTRY_VERIFY_WITH_TIMEOUT(offerer->isOpen() && answerer->isOpen(), 10000);
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setDeviceIdentity(device, record.name);
        QVERIFY(!client.auxiliaryWatchTelemetry());
        client.startSession(offerer, QString(), QString(),
                            server.stationIdentity().fingerprint());
        server.acceptTransport(answerer);
        QTRY_VERIFY_WITH_TIMEOUT(client.isHandshakeComplete(), 10000);
        QVERIFY(client.auxiliaryWatchTelemetry());
        QCOMPARE(client.auxiliaryWatchTelemetry()->receivedPayloadBytes, quint64(0));
        QCOMPARE(client.auxiliaryWatchTelemetry()->submittedPayloadBytes, quint64(0));
        // Even a mistaken or malicious capability claim cannot make a
        // DataChannel primary request a direct WSS watch ticket.
        StationCapabilities claimed = client.capabilities();
        claimed.txWatchPathVersion = 1;
        answerer->sendText(SessionMessages::encode(
            SessionMessages::capabilities(claimed.toUpdates())));
        QTRY_COMPARE(client.capabilities().txWatchPathVersion, 1);
        QTest::qWait(1200); // includes the earliest allowed ticket retry interval
        QVERIFY(!client.directWatchReady());
        for (const QList<QVariant>& received : inbound) {
            SessionMessage message;
            QVERIFY(SessionMessages::decode(received.first().toByteArray(), &message));
            QVERIFY(message.commandVerb != QByteArrayLiteral("tx.watchTicket"));
        }
    }

    void pairedPrimaryAttachesIndependentWatchAndPrimaryCloseStopsIt()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        QTemporaryDir settingsDir;
        QTemporaryDir securityDir;
        QTemporaryDir deviceDir;
        QVERIFY(settingsDir.isValid());
        QVERIFY(securityDir.isValid());
        QVERIFY(deviceDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.addSlice(QStringLiteral("pan-0"));
        StationServer server(&station, settings, Test::seedCoreIdentity(securityDir.path()));
        server.setRemoteTransmitAllowed(true);
        const auto device = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(deviceDir.path()));
        QVERIFY(device->isValid());
        PairedDevice record;
        record.id = device->fingerprint();
        record.publicKeySpki = device->publicKeySpki();
        record.name = QStringLiteral("Watch desktop");
        record.kind = QStringLiteral("computer");
        QVERIFY(server.deviceStore()->add(record));
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setDeviceIdentity(device, record.name);
        QVERIFY(!client.auxiliaryWatchTelemetry());
        const QUrl url(QStringLiteral("wss://127.0.0.1:%1/").arg(server.serverPort()));
        client.connectToStation(url, QString(), QString(), false,
                                server.stationIdentity().fingerprint());
        QTRY_VERIFY_WITH_TIMEOUT(client.isHandshakeComplete(), 10000);
        // The GUI samples this actual paired primary before its independent
        // watch attaches; a fixed media observer isolates the extra channel.
        qint64 now = 10000;
        std::optional<MediaPeerTelemetry> media{MediaPeerTelemetry{7, {}}};
        RemoteAudioReceiverTelemetry playback;
        playback.running = true;
        playback.generation = 3;
        const auto controlBefore = client.transportTelemetry();
        const auto watchBefore = client.auxiliaryWatchTelemetry();
        QVERIFY(controlBefore && watchBefore);
        RemoteTelemetryController controller(&client, nullptr, nullptr,
            [&] { return now; }, [&] { return playback; }, [&] { return media; });
        QTRY_VERIFY_WITH_TIMEOUT(client.directWatchReady(), 10000);
        QCOMPARE(client.auxiliaryWatchTelemetry()->submittedPayloadBytes, quint64(33));
        QCOMPARE(client.auxiliaryWatchTelemetry()->receivedPayloadBytes, quint64(2));
        const auto controlAfter = client.transportTelemetry();
        const auto watchAfter = client.auxiliaryWatchTelemetry();
        QVERIFY(controlAfter && watchAfter);
        media->traffic.receivedDisplayPayloadBytes = 1000;
        media->traffic.submittedDisplayPayloadBytes = 500;
        media->traffic.receivedRtpBytes = 2000;
        media->traffic.submittedRtpBytes = 100;
        playback.receivedAudioPayloadBytes = 900; // already inside RTP
        now += 1000;
        controller.sampleNow();
        const double expectedRx = double(controlAfter->receivedPayloadBytes
            - controlBefore->receivedPayloadBytes + watchAfter->receivedPayloadBytes
            - watchBefore->receivedPayloadBytes + 1000 + 2000) * 8.0 / 1000.0;
        const double expectedTx = double(controlAfter->acceptedPayloadBytes
            - controlBefore->acceptedPayloadBytes + watchAfter->submittedPayloadBytes
            - watchBefore->submittedPayloadBytes + 500 + 100) * 8.0 / 1000.0;
        QVERIFY(controller.current().coreGuiRxKbps);
        QVERIFY(controller.current().coreGuiTxKbps);
        QVERIFY(controller.current().coreGuiTotalKbps);
        QVERIFY(qAbs(*controller.current().coreGuiRxKbps - expectedRx) < 0.001);
        QVERIFY(qAbs(*controller.current().coreGuiTxKbps - expectedTx) < 0.001);
        QVERIFY(qAbs(*controller.current().coreGuiTotalKbps - expectedRx - expectedTx) < 0.001);
        QCOMPARE(controller.current().audioRtpRxKbps, std::optional<double>(16.0));
        QCOMPARE(controller.current().audioPayloadRxKbps, std::optional<double>(7.2));
        QCOMPARE(client.connectedUrl().host(), QStringLiteral("127.0.0.1"));
        QCOMPARE(server.peerCount(), 1); // auxiliary does not adopt a primary peer

        // Revoke watch eligibility, take a graph baseline, then admit and
        // retire a whole new authenticated watch between graph ticks. The
        // primary epoch stays put; the second attach and ACK must survive in
        // the folded tally and appear in the next total exactly once.
        const quint32 samePrimaryEpoch = client.sessionEpoch();
        server.setRemoteTransmitAllowed(false);
        QTRY_VERIFY_WITH_TIMEOUT(!client.directWatchReady(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.capabilities().txPermitted, 3000);
        now += 1000;
        controller.sampleNow();
        const auto controlAtTick = client.transportTelemetry();
        const auto watchAtTick = client.auxiliaryWatchTelemetry();
        QVERIFY(controlAtTick && watchAtTick);
        QSignalSpy graphTicks(&controller, &RemoteTelemetryController::changed);
        server.setRemoteTransmitAllowed(true);
        QTRY_VERIFY_WITH_TIMEOUT(client.capabilities().txPermitted, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.directWatchReady(), 10000);
        server.setRemoteTransmitAllowed(false);
        QTRY_VERIFY_WITH_TIMEOUT(!client.directWatchReady(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!client.capabilities().txPermitted, 3000);
        QCOMPARE(client.sessionEpoch(), samePrimaryEpoch);
        QCOMPARE(graphTicks.size(), 0);
        const auto controlAfterShortWatch = client.transportTelemetry();
        const auto watchAfterShortWatch = client.auxiliaryWatchTelemetry();
        QVERIFY(controlAfterShortWatch && watchAfterShortWatch);
        QCOMPARE(watchAfterShortWatch->receivedPayloadBytes - watchAtTick->receivedPayloadBytes,
                 quint64(2));
        QCOMPARE(watchAfterShortWatch->submittedPayloadBytes - watchAtTick->submittedPayloadBytes,
                 quint64(33));
        now += 1000;
        controller.sampleNow();
        const double shortWatchRx = double(controlAfterShortWatch->receivedPayloadBytes
            - controlAtTick->receivedPayloadBytes + 2) * 8.0 / 1000.0;
        const double shortWatchTx = double(controlAfterShortWatch->acceptedPayloadBytes
            - controlAtTick->acceptedPayloadBytes + 33) * 8.0 / 1000.0;
        QVERIFY(controller.current().coreGuiRxKbps);
        QVERIFY(controller.current().coreGuiTxKbps);
        QVERIFY(qAbs(*controller.current().coreGuiRxKbps - shortWatchRx) < 0.001);
        QVERIFY(qAbs(*controller.current().coreGuiTxKbps - shortWatchTx) < 0.001);
        server.setRemoteTransmitAllowed(true);
        QTRY_VERIFY_WITH_TIMEOUT(client.capabilities().txPermitted, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(client.directWatchReady(), 10000);
        QCOMPARE(client.sessionEpoch(), samePrimaryEpoch);
        QCOMPARE(client.auxiliaryWatchTelemetry()->receivedPayloadBytes,
                 watchAfterShortWatch->receivedPayloadBytes + quint64(2));
        QCOMPARE(client.auxiliaryWatchTelemetry()->submittedPayloadBytes,
                 watchAfterShortWatch->submittedPayloadBytes + quint64(33));

        RemoteTxWatchdog* watchdog = server.txWatchdog();
        QVERIFY(watchdog != nullptr);
        QSignalSpy heard(watchdog, &RemoteTxWatchdog::keepaliveHeard);
        QSignalSpy tripped(watchdog, &RemoteTxWatchdog::tripped);
        watchdog->setVoxArmed(record.id, true); // logical watch; no radio key
        client.remoteTransmit()->setSessionKeepalive([](quint64, quint32) { return false; });
        client.remoteTransmit()->setVoxArmed(true);
        QTRY_VERIFY_WITH_TIMEOUT(!heard.isEmpty(), 2000);
        QVERIFY(client.auxiliaryWatchTelemetry()->submittedPayloadBytes
                >= quint64(33 + RemoteTxWatchdog::kChannelKeepaliveBytes));
        QCOMPARE(heard.first().at(0).toByteArray(), record.id);

        client.disconnectFromStation(QStringLiteral("test primary close"));
        QVERIFY(!client.auxiliaryWatchTelemetry());
        QTRY_VERIFY_WITH_TIMEOUT(!client.directWatchReady(), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(tripped.size(), 1, 5000);
        QCOMPARE(tripped.first().at(1).toBool(), true);

        // A fresh primary authentication, rather than the retired auxiliary,
        // is the only route back to an independent watch.
        client.connectToStation(url, QString(), QString(), false,
                                server.stationIdentity().fingerprint());
        QTRY_VERIFY_WITH_TIMEOUT(client.isHandshakeComplete(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(client.directWatchReady(), 10000);
        QCOMPARE(client.auxiliaryWatchTelemetry()->receivedPayloadBytes, quint64(2));
        // VOX may arm another valid keepalive as soon as the new watch is
        // ready; the new session's one ACK proves the old receive tally reset.
        QVERIFY(client.auxiliaryWatchTelemetry()->submittedPayloadBytes >= quint64(33));
        QCOMPARE(server.peerCount(), 1);

        // Identity withdrawal retires the active auxiliary inside this
        // authenticated primary. A second retirement has nothing to fold.
        client.remoteTransmit()->setVoxArmed(false);
        now += 1000;
        controller.sampleNow(); // baseline the replacement watch in this epoch
        const AuxiliaryWatchTelemetry beforeRetirement = *client.auxiliaryWatchTelemetry();
        client.setDeviceIdentity(std::shared_ptr<const ClientDeviceIdentity>{},
                                 QStringLiteral("No watch identity"));
        QVERIFY(client.isHandshakeComplete());
        QVERIFY(!client.transmitWatchReady());
        QCOMPARE(client.auxiliaryWatchTelemetry()->submittedPayloadBytes,
                 beforeRetirement.submittedPayloadBytes);
        QCOMPARE(client.auxiliaryWatchTelemetry()->receivedPayloadBytes,
                 beforeRetirement.receivedPayloadBytes);
        now += 1000;
        controller.sampleNow();
        QCOMPARE(controller.current().coreGuiTotalKbps, std::optional<double>(0.0));
        client.setDeviceIdentity(device, record.name);
        QCOMPARE(client.auxiliaryWatchTelemetry()->submittedPayloadBytes,
                 beforeRetirement.submittedPayloadBytes);
        QCOMPARE(client.auxiliaryWatchTelemetry()->receivedPayloadBytes,
                 beforeRetirement.receivedPayloadBytes);
        now += 1000;
        controller.sampleNow();
        QCOMPARE(controller.current().coreGuiTotalKbps, std::optional<double>(0.0));
    }
};

QTEST_MAIN(TestStationTxWatchDirect)
#include "tst_station_tx_watch_direct.moc"
