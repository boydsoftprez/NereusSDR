// no-port-check: NereusSDR-original paired relay watch owner regression.
#include <QtTest>

#include <QDateTime>
#include <QHostAddress>
#include <QPointer>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

#include "core/AppSettings.h"
#include "core/security/CertificateStore.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/IceConfiguration.h"
#include "core/session/RelayLeg.h"
#include "core/session/RemoteTransmitClient.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"
#include "fakes/DataChannelPair.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

namespace {
class LocalRelay {
public:
    LocalRelay() : server(QStringLiteral("watch-owner"), QWebSocketServer::NonSecureMode)
    {
        server.listen(QHostAddress::LocalHost, 0);
        QObject::connect(&server, &QWebSocketServer::newConnection, &server, [this] {
            while (QWebSocket* socket = server.nextPendingConnection()) {
                QObject::connect(socket, &QWebSocket::binaryMessageReceived, &server,
                                 [this, socket](const QByteArray& frame) { receive(socket, frame); });
            }
        });
    }
    QUrl url() const { return QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.serverPort())); }
    bool ready() const { return server.isListening(); }
    bool watchPair() const { return peers.value(QStringLiteral("client-watch"))
                                && peers.value(QStringLiteral("core-watch")); }
    int watchDatagrams = 0;
    int primaryDatagrams = 0;
private:
    void receive(QWebSocket* socket, const QByteArray& frame)
    {
        if (frame.isEmpty()) { socket->close(); return; }
        const quint8 tag = quint8(frame.at(0));
        if (tag == RelayLeg::kTagJoin) {
            const QString token = QString::fromLatin1(frame.mid(1));
            if (!QStringList{QStringLiteral("primary"), QStringLiteral("client-watch"),
                             QStringLiteral("core-watch")}.contains(token)
                || peers.contains(token)) { socket->close(); return; }
            peers.insert(token, socket);
            roles.insert(socket, token);
            if (token == QLatin1String("primary")) {
                socket->sendBinaryMessage(QByteArray::fromHex("810101"));
            } else if (watchPair()) {
                const QByteArray ready = QByteArray::fromHex("810101");
                socket->sendBinaryMessage(ready);
                peers.value(token == QLatin1String("core-watch")
                                ? QStringLiteral("client-watch") : QStringLiteral("core-watch"))
                    ->sendBinaryMessage(ready);
            }
            return;
        }
        const QString role = roles.value(socket);
        if (role == QLatin1String("primary")) { ++primaryDatagrams; return; }
        if (tag != RelayLeg::kTagWatch || frame.size() < 2
            || frame.size() > RelayLeg::kMaxDatagramBytes + 1) { socket->close(); return; }
        QWebSocket* other = peers.value(role == QLatin1String("core-watch")
                                            ? QStringLiteral("client-watch")
                                            : QStringLiteral("core-watch"));
        if (other) { ++watchDatagrams; other->sendBinaryMessage(frame); }
    }
    QWebSocketServer server;
    QHash<QString, QPointer<QWebSocket>> peers;
    QHash<QWebSocket*, QString> roles;
};

IceConfiguration iceFor(const std::shared_ptr<RelayLeg>& leg)
{
    IceConfiguration ice = IceConfiguration::throughRendezvous(
        {}, true, IceConfiguration::localAddressFamilies(), {});
    ice.setRelay(std::nullopt, 1);
    ice.setCandidateSourceFactory(RelayLeg::factoryFor(leg), true);
    return ice;
}
}

class TestStationTxWatchRelay : public QObject {
    Q_OBJECT
private slots:
    void cleanup()
    {
        RelayLeg::setRelayUrlForTest({});
        DataChannelTransport::setSelectedPathOverrideForTest({});
    }

    void pairedOwnerNegotiatesDedicatedWatch_data()
    {
        QTest::addColumn<int>("caseId");
        QTest::newRow("matched certificate") << 0;
        QTest::newRow("different watch certificate") << 1;
        QTest::newRow("wrong command id") << 2;
        QTest::newRow("extra reply field") << 3;
        QTest::newRow("transmit permission absent") << 4;
        QTest::newRow("watch capability absent") << 5;
        QTest::newRow("auxiliary loss keeps primary") << 6;
    }

    void pairedOwnerNegotiatesDedicatedWatch()
    {
        QFETCH(int, caseId);
        const bool wrongCertificate = caseId == 1;
        LocalRelay relay;
        QVERIFY(relay.ready());
        RelayLeg::setRelayUrlForTest(relay.url());
        QTemporaryDir settingsDir, securityDir, deviceDir, otherCertDir;
        QVERIFY(settingsDir.isValid() && securityDir.isValid() && deviceDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        station.setBoardForTest(HPSDRHW::HermesLite);
        station.addSlice(QStringLiteral("pan-0"));
        StationServer server(&station, settings, Test::seedCoreIdentity(securityDir.path()));
        server.setRemoteTransmitAllowed(true);
        auto device = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(deviceDir.path()));
        QVERIFY(device->isValid());
        PairedDevice record;
        record.id = device->fingerprint();
        record.publicKeySpki = device->publicKeySpki();
        record.name = QStringLiteral("Watch desktop");
        record.kind = QStringLiteral("computer");
        QVERIFY(server.deviceStore()->add(record));
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));

        auto primaryLeg = RelayLeg::create();
        QVERIFY(primaryLeg);
        primaryLeg->open(QUrl(QStringLiteral("wss://relay.example/ws")), QStringLiteral("primary"));
        QTRY_VERIFY(primaryLeg->state() == RelayLeg::State::Joined && primaryLeg->peerPresent());
        auto* offerer = new DataChannelTransport;
        auto* answerer = new DataChannelTransport;
        QVERIFY(Test::startDataChannelPair(offerer, answerer,
                                          StationClient::kMaxIncomingMessageBytes,
                                          StationServer::kMaxIncomingMessageBytes,
                                          server.certificatePemPath(), server.privateKeyPemPath()));
        QTRY_VERIFY_WITH_TIMEOUT(offerer->isOpen() && answerer->isOpen(), 15000);
        DataChannelTransport::setSelectedPathOverrideForTest(
            [offerer](const DataChannelTransport* peer) -> std::optional<MediaIcePath> {
            if (peer != offerer) { return std::nullopt; }
            MediaIcePath path;
            path.remoteAddress = QStringLiteral("127.0.0.1");
            path.ownedLoopbackShim = true;
            return path;
        });
        const qint64 grantExpires = QDateTime::currentSecsSinceEpoch()
            + (caseId == 0 ? 7 : 120);
        QVERIFY(offerer->setWatchRelayGrant({QUrl(QStringLiteral("wss://relay.example/ws")),
                                             QStringLiteral("client-watch"),
                                             grantExpires, primaryLeg}));
        QVERIFY(offerer->canOpenWatchRelay());
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        client.setDeviceIdentity(device, record.name);
        // Substitute the bounded command responder after authentication but
        // before the client's first watch attempt. Waiting for the handshake
        // outside this signal can miss the command emitted immediately after.
        QObject::connect(&client, &StationClient::handshakeComplete, &client, [answerer] {
            if (answerer->parent()) { answerer->disconnect(answerer->parent()); }
        });
        std::shared_ptr<RelayLeg> coreLeg;
        QPointer<DataChannelTransport> coreWatch;
        QList<QByteArray> watchFrames;
        int offers = 0;
        bool coreJoined = false;
        bool startedAnswerer = false;
        bool acceptedDescription = false;
        bool acceptedOffer = false;
        QByteArray ticket(32, '\x5a');
        QByteArray responseWire;
        QObject::connect(answerer, &SessionTransport::textReceived, &client,
                         [&](const QByteArray& wire) {
            SessionMessage request;
            if (!SessionMessages::decode(wire, &request)
                || request.commandVerb != QByteArrayLiteral("tx.watchRelay")) { return; }
            ++offers;
            if (request.arguments.size() != 1 || request.arguments.first().name != "offer"
                || request.arguments.first().kind != MirrorWireKind::Utf8) { return; }
            const QString offer = request.arguments.first().value.toString();
            const quint32 commandId = request.commandId;
            if (offer.isEmpty() || offer.toUtf8().size() > IMediaTransport::kMaxDescriptionBytes) {
                return;
            }
            coreLeg = RelayLeg::createWatch();
            coreWatch = new DataChannelTransport(&client);
            DataChannelTransport::Options options;
            options.role = DataChannelTransport::Role::Answerer;
            options.purpose = DataChannelTransport::Purpose::TxWatch;
            options.maxIncomingBytes = DataChannelTransport::kMaxWatchFrameBytes;
            options.ice = iceFor(coreLeg);
            CertificateStore other(otherCertDir.path());
            options.certificatePemPath = wrongCertificate ? other.certificatePath()
                                                           : server.certificatePemPath();
            options.privateKeyPemPath = wrongCertificate ? other.privateKeyPath()
                                                         : server.privateKeyPemPath();
            QObject::connect(coreWatch, &SessionTransport::binaryReceived, &client,
                             [&](const QByteArray& frame) {
                watchFrames.append(frame);
                if (watchFrames.size() == 1 && frame.size() == 33 && frame.at(0) == 1) {
                    coreWatch->sendBinary(QByteArray::fromHex("0100"));
                }
            });
            QObject::connect(coreWatch, &DataChannelTransport::localDescription, &client,
                             [&, commandId](const QString& answer, const QString& type) {
                if (type != QLatin1String("answer")) { return; }
                const QString encoded = StationIdentity::toBase64Url(ticket);
                QList<MirrorUpdate> fields{
                    {0, QByteArrayLiteral("ticket"), MirrorWireKind::Utf8, encoded},
                    {0, QByteArrayLiteral("expiresInMs"), MirrorWireKind::Int64, qint64(10000)},
                    {0, QByteArrayLiteral("path"), MirrorWireKind::Utf8,
                     QStringLiteral("relay-dtls-v1")},
                    {0, QByteArrayLiteral("answer"), MirrorWireKind::Utf8, answer}};
                if (caseId == 3) {
                    fields.append({0, QByteArrayLiteral("extra"), MirrorWireKind::Utf8,
                                   QStringLiteral("refused")});
                }
                responseWire = SessionMessages::encode(SessionMessages::commandResult(
                    QByteArrayLiteral("tx.watchRelay"), commandId + quint32(caseId == 2),
                    true, {}, {}, fields));
                answerer->sendText(responseWire);
            });
            QObject::connect(coreLeg.get(), &RelayLeg::joined, &client,
                             [&, options, offer](bool) {
                coreJoined = true;
                startedAnswerer = coreWatch->start(options);
                acceptedDescription = startedAnswerer
                    && coreWatch->acceptDescription(offer, QStringLiteral("offer"));
                acceptedOffer = startedAnswerer && acceptedDescription;
            });
            coreLeg->open(QUrl(QStringLiteral("wss://relay.example/ws")),
                          QStringLiteral("core-watch"));
        });
        client.startSession(offerer, QString(), QString(), server.stationIdentity().fingerprint());
        server.acceptTransport(answerer);
        QTRY_VERIFY_WITH_TIMEOUT(client.isHandshakeComplete(), 10000);
        QVERIFY(client.auxiliaryWatchTelemetry());
        QCOMPARE(client.auxiliaryWatchTelemetry()->submittedPayloadBytes, quint64(0));
        // Load findings 3: the Core sends this session its capabilities
        // again once the snapshot is complete (txPermitted can be true only
        // from then, StationServer::publishTxPermitted), with its own
        // txWatchPathVersion 0. Under load that publication could land
        // after the capabilities this test sends below and put the watch
        // path back to 0, so the client never started a watch attempt
        // (offers 0 at load 342). Wait for it before sending ours.
        QTRY_VERIFY_WITH_TIMEOUT(client.capabilities().txPermitted, 10000);
        StationCapabilities caps = client.capabilities();
        caps.txWatchPathVersion = caseId == 5 ? 0 : 1;
        if (caseId == 4) { caps.txPermitted = false; }
        answerer->sendText(SessionMessages::encode(SessionMessages::capabilities(caps.toUpdates())));
        QTRY_COMPARE(client.capabilities().txWatchPathVersion, caps.txWatchPathVersion);
        if (caseId == 4 || caseId == 5) {
            QTest::qWait(1200);
            QCOMPARE(offers, 0);
            QVERIFY(!client.transmitWatchReady());
            QVERIFY(client.isHandshakeComplete());
            return;
        }
        QTRY_COMPARE_WITH_TIMEOUT(offers, 1, 12000);
        QTRY_VERIFY_WITH_TIMEOUT(relay.watchPair(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(coreJoined, 3000);
        QVERIFY2(startedAnswerer, "watch answerer did not start");
        QVERIFY2(acceptedDescription, "watch answerer refused offered SDP");
        QVERIFY(acceptedOffer);
        if (caseId != 0 && caseId != 6) {
            QTest::qWait(1500);
            QVERIFY(watchFrames.isEmpty());
            QVERIFY(!client.transmitWatchReady());
            QVERIFY(client.isHandshakeComplete());
        } else {
            QTRY_VERIFY_WITH_TIMEOUT(client.transmitWatchReady(), 15000);
            QCOMPARE(client.auxiliaryWatchTelemetry()->submittedPayloadBytes, quint64(33));
            QCOMPARE(client.auxiliaryWatchTelemetry()->receivedPayloadBytes, quint64(2));
            QVERIFY(!client.directWatchReady());
            QTRY_COMPARE_WITH_TIMEOUT(watchFrames.size(), 1, 5000);
            QCOMPARE(watchFrames.first(), QByteArray(1, char(1)) + ticket);
            answerer->sendText(responseWire); // duplicate result cannot reattach
            QTest::qWait(100);
            QVERIFY(client.transmitWatchReady());
            QCOMPARE(watchFrames.size(), 1);
            client.remoteTransmit()->setSessionKeepalive([](quint64, quint32) { return false; });
            client.remoteTransmit()->setVoxArmed(true); // logical watch only, no RF
            QTRY_VERIFY_WITH_TIMEOUT(watchFrames.size() >= 2, 3000);
            QCOMPARE(watchFrames.at(1).size(), 13);
            QVERIFY(client.auxiliaryWatchTelemetry()->submittedPayloadBytes
                    >= quint64(33 + RemoteTxWatchdog::kChannelKeepaliveBytes));
            if (caseId == 0) {
                QTRY_VERIFY_WITH_TIMEOUT(!offerer->canOpenWatchRelay(), 9000);
                QVERIFY(offerer->hasWatchRelayRoute());
                QVERIFY(client.transmitWatchReady());
            }
            if (caseId == 6) {
                coreWatch->closeLink(QStringLiteral("test auxiliary loss"));
                QTRY_VERIFY_WITH_TIMEOUT(!client.transmitWatchReady(), 3000);
                QVERIFY(client.isHandshakeComplete());
                QVERIFY(client.auxiliaryWatchTelemetry()->submittedPayloadBytes
                        >= quint64(33 + RemoteTxWatchdog::kChannelKeepaliveBytes));
            }
            client.disconnectFromStation(QStringLiteral("test primary close"));
            QVERIFY(!client.transmitWatchReady());
        }
        QVERIFY(relay.watchDatagrams > 0 || (caseId != 0 && caseId != 6));
        QCOMPARE(relay.primaryDatagrams, 0);
    }
};

QTEST_GUILESS_MAIN(TestStationTxWatchRelay)
#include "tst_station_tx_watch_relay.moc"
