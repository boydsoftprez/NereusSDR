// no-port-check: NereusSDR-original Core relay watch integration tests.
#include <QtTest>
#include <QScopeGuard>

#include "core/AppSettings.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/DeviceStore.h"
#include "core/security/StationIdentity.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/RelayLeg.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationServer.h"
#include "core/session/TxWatchServer.h"
#include "core/session/media/IMediaTransport.h"
#include "fakes/DataChannelPair.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

using namespace NereusSDR;

namespace {

// A real paired control DTLS primary with a grant and a valid watch offer.
// The offerer deliberately withholds the answer, leaving the Core's watch
// peer pending so each retirement path can be inspected before any attach.
struct PendingRig {
    QTemporaryDir settingsDir;
    QTemporaryDir securityDir;
    QTemporaryDir deviceDir;
    AppSettings settings;
    RadioModel model;
    StationServer station;
    StationIdentity device;
    PairedDevice record;
    QWebSocketServer relay;
    QHash<QString, QPointer<QWebSocket>> sockets;
    std::shared_ptr<RelayLeg> primaryLeg;
    std::shared_ptr<RelayLeg> watchLeg;
    QPointer<DataChannelTransport> primary;
    QPointer<DataChannelTransport> client;
    std::unique_ptr<DataChannelTransport> watchOfferer;
    QList<SessionMessage> messages;
    QString offer;
    QString failure;

    PendingRig()
        : settings(settingsDir.filePath(QStringLiteral("pending.settings")))
        , station(&model, settings,
                  Test::withSharedTlsIdentity(Test::seedCoreIdentity(securityDir.path())))
        , device(StationIdentity::loadOrCreate(deviceDir.path()))
        , relay(QStringLiteral("pending relay"), QWebSocketServer::NonSecureMode)
    {}

    ~PendingRig()
    {
        watchOfferer.reset();
        station.close();
        if (client) {
            client->closeLink(QStringLiteral("fixture ended"));
            client->deleteLater();
        }
        if (primaryLeg) {
            primaryLeg->close();
        }
    }

    bool bad(const QString& step) { failure = step; return false; }

    bool start()
    {
        if (!settingsDir.isValid() || !securityDir.isValid() || !deviceDir.isValid()) {
            return bad(QStringLiteral("temporary directories"));
        }
        model.setBoardForTest(HPSDRHW::HermesLite);
        model.addSlice(QStringLiteral("pan-0"));
        station.setRemoteTransmitAllowed(true);
        record.id = device.fingerprint();
        record.publicKeySpki = device.publicKeySpki();
        record.name = QStringLiteral("Pending watch device");
        record.kind = QStringLiteral("computer");
        if (!station.deviceStore()->add(record)
            || !station.listen(QHostAddress::LocalHost, 0)
            || !relay.listen(QHostAddress::LocalHost, 0)) {
            return bad(QStringLiteral("station/relay listen or pair"));
        }
        QObject::connect(&relay, &QWebSocketServer::newConnection, &relay, [this]() {
            QWebSocket* socket = relay.nextPendingConnection();
            QObject::connect(socket, &QWebSocket::binaryMessageReceived, &relay,
                             [this, socket](const QByteArray& frame) {
                if (frame.isEmpty() || quint8(frame.at(0)) != RelayLeg::kTagJoin) {
                    socket->close();
                    return;
                }
                const QString token = QString::fromLatin1(frame.mid(1));
                if (token != QLatin1String("core-primary")
                    && token != QLatin1String("core-watch")) {
                    socket->close();
                    return;
                }
                sockets.insert(token, socket);
                socket->sendBinaryMessage(QByteArray::fromHex("810101"));
            });
        });
        RelayLeg::setRelayUrlForTest(QUrl(QStringLiteral("ws://127.0.0.1:%1")
                                            .arg(relay.serverPort())));
        primaryLeg = RelayLeg::create();
        if (!primaryLeg) {
            return bad(QStringLiteral("primary leg"));
        }
        primaryLeg->open(QUrl(QStringLiteral("wss://relay.example/v1/relay")),
                         QStringLiteral("core-primary"));
        if (!QTest::qWaitFor([this] {
                return primaryLeg->state() == RelayLeg::State::Joined
                    && primaryLeg->peerPresent();
            }, 5000)) {
            return bad(QStringLiteral("primary relay joined"));
        }
        client = new DataChannelTransport;
        primary = new DataChannelTransport;
        QObject::connect(primary, &DataChannelTransport::opened, &station,
                         [this]() { if (primary) station.acceptTransport(primary); });
        QObject::connect(client, &SessionTransport::textReceived, client,
                         [this](const QByteArray& wire) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)) {
                messages.append(message);
            }
        });
        if (!Test::startDataChannelPair(client, primary, 1024 * 1024,
                                        StationServer::kMaxIncomingMessageBytes,
                                        station.certificatePemPath(), station.privateKeyPemPath())
            || !QTest::qWaitFor([this] { return client && primary
                                            && client->isOpen() && primary->isOpen(); }, 15000)) {
            return bad(QStringLiteral("control DTLS pair"));
        }
        DataChannelTransport::WatchRelayGrant grant{
            QUrl(QStringLiteral("wss://relay.example/v1/relay")),
            QStringLiteral("core-watch"), QDateTime::currentSecsSinceEpoch() + 40, primaryLeg};
        if (!primary->setWatchRelayGrant(grant)) {
            return bad(QStringLiteral("watch grant"));
        }
        const QPointer<DataChannelTransport> exactPrimary = primary;
        DataChannelTransport::setSelectedPathOverrideForTest(
            [exactPrimary](const DataChannelTransport* channel) -> std::optional<MediaIcePath> {
            if (!exactPrimary || channel != exactPrimary) {
                return std::nullopt;
            }
            MediaIcePath path;
            path.remoteAddress = QStringLiteral("127.0.0.1");
            path.ownedLoopbackShim = true;
            return path;
        });
        if (!QTest::qWaitFor([this] {
                return result(SessionMessageKind::Hello).kind == SessionMessageKind::Hello;
            }, 5000)) {
            return bad(QStringLiteral("Core hello"));
        }
        QString fingerprint = station.certificateFingerprint();
        fingerprint.remove(QLatin1Char(':'));
        const SessionDeviceBlock proof{
            StationIdentity::toBase64Url(record.id),
            StationIdentity::toBase64Url(device.publicKeySpki()),
            record.name, record.kind,
            StationIdentity::toBase64Url(device.sign(DeviceAuthenticator::transcript(
                StationIdentity::fromBase64Url(result(SessionMessageKind::Hello).challenge),
                QByteArray::fromHex(fingerprint.toLatin1()),
                station.stationIdentity().publicKeySpki(), device.publicKeySpki())))};
        send(SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 0,
            record.name, {kSessionProtocolMajor},
            {{"deviceAuth", 1}, {"remoteTx", 1}, {"txWatchPath", 1}, {"txWatchRelay", 1}}));
        send(SessionMessages::authRequest(QString(), proof));
        if (!QTest::qWaitFor([this] {
                return station.authenticatedSessionCount() == 1
                    && result(SessionMessageKind::SnapshotComplete).kind
                        == SessionMessageKind::SnapshotComplete;
            }, 5000)) {
            return bad(QStringLiteral("paired snapshot"));
        }
        watchLeg = RelayLeg::createWatch();
        watchOfferer = std::make_unique<DataChannelTransport>();
        if (!watchLeg) {
            return bad(QStringLiteral("watch offer leg"));
        }
        IceConfiguration ice = IceConfiguration::throughRendezvous({}, true, {}, {});
        ice.setRelay(std::nullopt, 1);
        ice.setCandidateSourceFactory(RelayLeg::factoryFor(watchLeg), true);
        DataChannelTransport::Options options;
        options.role = DataChannelTransport::Role::Offerer;
        options.purpose = DataChannelTransport::Purpose::TxWatch;
        options.maxIncomingBytes = DataChannelTransport::kMaxWatchFrameBytes;
        options.ice = ice;
        QObject::connect(watchOfferer.get(), &DataChannelTransport::localDescription,
                         watchOfferer.get(), [this](const QString& sdp, const QString&) {
            offer = sdp;
        });
        if (!watchOfferer->start(options)
            || !QTest::qWaitFor([this] { return !offer.isEmpty(); }, 5000)) {
            return bad(QStringLiteral("watch SDP offer"));
        }
        return true;
    }

    void send(const SessionMessage& message)
    {
        if (client) {
            client->sendText(SessionMessages::encode(message));
        }
    }

    SessionMessage result(SessionMessageKind kind, quint32 id = 0) const
    {
        for (const SessionMessage& message : messages) {
            if (message.kind == kind && (id == 0 || message.commandId == id)) {
                return message;
            }
        }
        return {};
    }

    int resultsFor(quint32 id) const
    {
        int count = 0;
        for (const SessionMessage& message : messages) {
            count += message.kind == SessionMessageKind::CommandResult && message.commandId == id;
        }
        return count;
    }

    void requestWatch(quint32 id)
    {
        send(SessionMessages::commandInvoke(QByteArrayLiteral("tx.watchRelay"), id,
                                            {{0, "offer", MirrorWireKind::Utf8, offer}}));
    }

    TxWatchServer* watchServer() const
    {
        for (QObject* child : station.children()) {
            if (auto* server = dynamic_cast<TxWatchServer*>(child)) {
                return server;
            }
        }
        return nullptr;
    }

    bool coreWatchSocketClosed() const
    {
        const QPointer<QWebSocket> socket = sockets.value(QStringLiteral("core-watch"));
        return !socket || socket->state() != QAbstractSocket::ConnectedState;
    }
};

} // namespace

class TstStationWatchRelay : public QObject {
    Q_OBJECT
private slots:
    void cleanup()
    {
        RelayLeg::setRelayUrlForTest({});
        DataChannelTransport::setSelectedPathOverrideForTest({});
    }

    void pendingWatchIsRetiredOnDeviceRevoke()
    {
        PendingRig rig;
        QVERIFY2(rig.start(), qPrintable(rig.failure));
        TxWatchServer* owner = rig.watchServer();
        QVERIFY(owner);
        rig.requestWatch(901);
        QTRY_COMPARE(rig.resultsFor(901), 1);
        const SessionMessage answer = rig.result(SessionMessageKind::CommandResult, 901);
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(owner->bindingCount(), 1);
        QVERIFY(rig.station.deviceStore()->remove(rig.record.id));
        QTRY_COMPARE(rig.station.authenticatedSessionCount(), 0);
        QTRY_COMPARE(owner->bindingCount(), 0);
        QTRY_VERIFY(rig.coreWatchSocketClosed());
        QCOMPARE(rig.resultsFor(901), 1); // no late result from the retired peer
        QVERIFY(rig.watchOfferer->acceptDescription(answer.updates.at(3).value.toString(),
                                                   QStringLiteral("answer")));
        QTest::qWait(250);
        QVERIFY(!rig.watchOfferer->isOpen());
        QCOMPARE(owner->bindingCount(), 0);
        QVERIFY(rig.station.deviceStore()->add(rig.record)); // revoked place is reusable
    }

    void pendingWatchIsRetiredBeforePrimaryPathMove()
    {
        PendingRig rig;
        QVERIFY2(rig.start(), qPrintable(rig.failure));
        TxWatchServer* owner = rig.watchServer();
        QVERIFY(owner);
        rig.requestWatch(911);
        QTRY_COMPARE(rig.resultsFor(911), 1);
        const SessionMessage answer = rig.result(SessionMessageKind::CommandResult, 911);
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(owner->bindingCount(), 1);
        rig.send(SessionMessages::commandInvoke(QByteArrayLiteral("session.pathTicket"), 912, {}));
        QTRY_COMPARE(rig.resultsFor(912), 1);
        const SessionMessage pathTicket = rig.result(SessionMessageKind::CommandResult, 912);
        QVERIFY2(pathTicket.accepted, qPrintable(pathTicket.reason));
        const QString ticket = pathTicket.updates.first().value.toString();
        QVERIFY(!ticket.isEmpty());
        auto* nextStation = new Test::LoopbackTransport(QStringLiteral("next station path"));
        Test::LoopbackTransport nextClient(QStringLiteral("next device path"));
        nextStation->linkTo(&nextClient);
        rig.station.acceptTransport(nextStation);
        QTRY_VERIFY(!nextClient.received().isEmpty());
        nextClient.sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("next path"),
            {kSessionProtocolMajor}, {})));
        nextClient.sendText(SessionMessages::encode(SessionMessages::pathJoin(ticket)));
        QTRY_COMPARE(rig.station.sessionsMoved(), 1);
        QTRY_COMPARE(owner->bindingCount(), 0);
        QTRY_VERIFY(rig.coreWatchSocketClosed());
        QCOMPARE(rig.resultsFor(911), 1);
        QVERIFY(rig.watchOfferer->acceptDescription(answer.updates.at(3).value.toString(),
                                                   QStringLiteral("answer")));
        QTest::qWait(250);
        QVERIFY(!rig.watchOfferer->isOpen());
        QCOMPARE(rig.station.authenticatedSessionCount(), 1); // new path retains the session
        rig.station.close();
    }

    void pendingWatchDeadlineReclaimsItsBinding()
    {
        PendingRig rig;
        QVERIFY2(rig.start(), qPrintable(rig.failure));
        TxWatchServer* owner = rig.watchServer();
        QVERIFY(owner);
        rig.requestWatch(921);
        QTRY_COMPARE(rig.resultsFor(921), 1);
        const SessionMessage first = rig.result(SessionMessageKind::CommandResult, 921);
        QVERIFY2(first.accepted, qPrintable(first.reason));
        QCOMPARE(owner->bindingCount(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(owner->bindingCount(), 0,
                                  TxWatchServer::kTicketLifetimeMs + 3000);
        QTRY_VERIFY(rig.coreWatchSocketClosed());
        QCOMPARE(rig.resultsFor(921), 1); // expiry does not issue another answer
        QVERIFY(rig.watchOfferer->acceptDescription(first.updates.at(3).value.toString(),
                                                   QStringLiteral("answer")));
        QTest::qWait(250);
        QVERIFY(!rig.watchOfferer->isOpen());
        rig.requestWatch(922);
        QTRY_COMPARE(rig.resultsFor(922), 1);
        QVERIFY2(rig.result(SessionMessageKind::CommandResult, 922).accepted,
                 qPrintable(rig.result(SessionMessageKind::CommandResult, 922).reason));
        QCOMPARE(owner->bindingCount(), 1); // fresh ticket uses the released slot
        QCOMPARE(rig.resultsFor(921), 1);
    }

    void pendingWatchCannotCrossPrimarySessionReplacement()
    {
        PendingRig rig;
        QVERIFY2(rig.start(), qPrintable(rig.failure));
        TxWatchServer* owner = rig.watchServer();
        QVERIFY(owner);
        rig.requestWatch(931);
        QTRY_COMPARE(rig.resultsFor(931), 1);
        const SessionMessage oldAnswer = rig.result(SessionMessageKind::CommandResult, 931);
        QVERIFY2(oldAnswer.accepted, qPrintable(oldAnswer.reason));
        QCOMPARE(owner->bindingCount(), 1);
        rig.client->closeLink(QStringLiteral("old primary ended"));
        QTRY_COMPARE(rig.station.authenticatedSessionCount(), 0);
        QTRY_COMPARE(owner->bindingCount(), 0);
        QTRY_VERIFY(rig.coreWatchSocketClosed());

        auto* nextClient = new DataChannelTransport;
        auto* nextPrimary = new DataChannelTransport;
        QPointer<DataChannelTransport> nextClientGuard(nextClient);
        QObject::connect(nextPrimary, &DataChannelTransport::opened, &rig.station,
                         [&rig, nextPrimary]() { rig.station.acceptTransport(nextPrimary); });
        QList<SessionMessage> nextMessages;
        QObject::connect(nextClient, &SessionTransport::textReceived, nextClient,
                         [&nextMessages](const QByteArray& wire) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)) {
                nextMessages.append(message);
            }
        });
        QVERIFY(Test::startDataChannelPair(nextClient, nextPrimary, 1024 * 1024,
                                           StationServer::kMaxIncomingMessageBytes,
                                           rig.station.certificatePemPath(),
                                           rig.station.privateKeyPemPath()));
        QTRY_VERIFY(nextClient->isOpen() && nextPrimary->isOpen());
        DataChannelTransport::WatchRelayGrant grant{
            QUrl(QStringLiteral("wss://relay.example/v1/relay")),
            QStringLiteral("core-watch"), QDateTime::currentSecsSinceEpoch() + 40,
            rig.primaryLeg};
        QVERIFY(nextPrimary->setWatchRelayGrant(grant));
        const QPointer<DataChannelTransport> exact(nextPrimary);
        DataChannelTransport::setSelectedPathOverrideForTest(
            [exact](const DataChannelTransport* channel) -> std::optional<MediaIcePath> {
            if (!exact || channel != exact) {
                return std::nullopt;
            }
            MediaIcePath path;
            path.remoteAddress = QStringLiteral("127.0.0.1");
            path.ownedLoopbackShim = true;
            return path;
        });
        const auto newResult = [&nextMessages](SessionMessageKind kind, quint32 id = 0) {
            for (const SessionMessage& message : nextMessages) {
                if (message.kind == kind && (id == 0 || message.commandId == id)) {
                    return message;
                }
            }
            return SessionMessage{};
        };
        QTRY_VERIFY(newResult(SessionMessageKind::Hello).kind == SessionMessageKind::Hello);
        QString fingerprint = rig.station.certificateFingerprint();
        fingerprint.remove(QLatin1Char(':'));
        const SessionDeviceBlock proof{
            StationIdentity::toBase64Url(rig.record.id),
            StationIdentity::toBase64Url(rig.device.publicKeySpki()),
            rig.record.name, rig.record.kind,
            StationIdentity::toBase64Url(rig.device.sign(DeviceAuthenticator::transcript(
                StationIdentity::fromBase64Url(newResult(SessionMessageKind::Hello).challenge),
                QByteArray::fromHex(fingerprint.toLatin1()),
                rig.station.stationIdentity().publicKeySpki(),
                rig.device.publicKeySpki())))};
        const auto sendNext = [nextClientGuard](const SessionMessage& message) {
            if (nextClientGuard) {
                nextClientGuard->sendText(SessionMessages::encode(message));
            }
        };
        sendNext(SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 0,
            rig.record.name, {kSessionProtocolMajor},
            {{"deviceAuth", 1}, {"remoteTx", 1}, {"txWatchPath", 1}, {"txWatchRelay", 1}}));
        sendNext(SessionMessages::authRequest(QString(), proof));
        QTRY_COMPARE(rig.station.authenticatedSessionCount(), 1);
        QTRY_VERIFY(newResult(SessionMessageKind::SnapshotComplete).kind
                    == SessionMessageKind::SnapshotComplete);
        sendNext(SessionMessages::commandInvoke(QByteArrayLiteral("tx.watchRelay"), 932,
            {{0, "offer", MirrorWireKind::Utf8, rig.offer}}));
        QTRY_VERIFY(newResult(SessionMessageKind::CommandResult, 932).commandId == 932);
        const SessionMessage fresh = newResult(SessionMessageKind::CommandResult, 932);
        QVERIFY2(fresh.accepted, qPrintable(fresh.reason));
        QCOMPARE(owner->bindingCount(), 1);
        QVERIFY(fresh.updates.first().value.toString()
                != oldAnswer.updates.first().value.toString());
        QCOMPARE(rig.resultsFor(931), 1);
        for (const SessionMessage& message : nextMessages) {
            QVERIFY(message.kind != SessionMessageKind::CommandResult || message.commandId != 931);
        }
        QVERIFY(rig.watchOfferer->acceptDescription(oldAnswer.updates.at(3).value.toString(),
                                                   QStringLiteral("answer")));
        QTest::qWait(250);
        QVERIFY(!rig.watchOfferer->isOpen());
        QCOMPARE(owner->bindingCount(), 1); // stale offer did not consume the fresh ticket
        rig.station.close();
        nextClient->closeLink(QStringLiteral("fixture ended"));
        nextClient->deleteLater();
    }

    void pairedPrimaryIssuesSeparateRelayDtlsWatch()
    {
        QTemporaryDir settingsDir;
        QTemporaryDir securityDir;
        QTemporaryDir deviceDir;
        QVERIFY(settingsDir.isValid() && securityDir.isValid() && deviceDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        model.addSlice(QStringLiteral("pan-0"));
        StationServer station(&model, settings,
            Test::withSharedTlsIdentity(Test::seedCoreIdentity(securityDir.path())));
        station.setRemoteTransmitAllowed(true);
        StationIdentity device = StationIdentity::loadOrCreate(deviceDir.path());
        PairedDevice record;
        record.id = device.fingerprint();
        record.publicKeySpki = device.publicKeySpki();
        record.name = QStringLiteral("Relay watch device");
        record.kind = QStringLiteral("computer");
        QVERIFY(station.deviceStore()->add(record));
        QVERIFY2(station.listen(QHostAddress::LocalHost, 0), qPrintable(station.lastError()));

        QWebSocketServer relay(QStringLiteral("local watch relay"),
                               QWebSocketServer::NonSecureMode);
        QVERIFY(relay.listen(QHostAddress::LocalHost, 0));
        QHash<QString, QPointer<QWebSocket>> legs;
        QList<QByteArray> watchFrames;
        connect(&relay, &QWebSocketServer::newConnection, &relay, [&]() {
            QWebSocket* socket = relay.nextPendingConnection();
            connect(socket, &QWebSocket::binaryMessageReceived, &relay,
                    [&, socket](const QByteArray& frame) {
                if (frame.isEmpty()) {
                    socket->close();
                    return;
                }
                if (quint8(frame.at(0)) == RelayLeg::kTagJoin) {
                    const QString token = QString::fromLatin1(frame.mid(1));
                    if (token != QLatin1String("core-primary")
                        && token != QLatin1String("core-watch")
                        && token != QLatin1String("client-watch")) {
                        socket->close();
                        return;
                    }
                    legs.insert(token, socket);
                    socket->sendBinaryMessage(QByteArray::fromHex("810101"));
                    return;
                }
                if (quint8(frame.at(0)) != RelayLeg::kTagWatch) {
                    return;
                }
                watchFrames.append(frame);
                const QString destination = legs.value(QStringLiteral("core-watch")) == socket
                    ? QStringLiteral("client-watch") : QStringLiteral("core-watch");
                if (QWebSocket* other = legs.value(destination)) {
                    other->sendBinaryMessage(frame);
                }
            });
        });
        RelayLeg::setRelayUrlForTest(QUrl(QStringLiteral("ws://127.0.0.1:%1")
                                            .arg(relay.serverPort())));
        auto primaryLeg = RelayLeg::create();
        QVERIFY(primaryLeg);
        primaryLeg->open(QUrl(QStringLiteral("wss://relay.example/v1/relay")),
                         QStringLiteral("core-primary"));
        QTRY_VERIFY(primaryLeg->state() == RelayLeg::State::Joined && primaryLeg->peerPresent());

        auto* client = new DataChannelTransport;
        auto* primary = new DataChannelTransport;
        connect(primary, &DataChannelTransport::opened, &station,
                [&station, primary]() { station.acceptTransport(primary); });
        QList<SessionMessage> messages;
        connect(client, &SessionTransport::textReceived, client,
                [&messages](const QByteArray& wire) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)) {
                messages.append(message);
            }
        });
        QVERIFY(Test::startDataChannelPair(client, primary,
                                           1024 * 1024, StationServer::kMaxIncomingMessageBytes,
                                           station.certificatePemPath(), station.privateKeyPemPath()));
        QTRY_VERIFY(client->isOpen() && primary->isOpen());
        // The grant's expiry is read against a clock this test moves, so the
        // expiry below is the event the test waits on, not eight seconds of
        // a loaded machine's time.
        qint64 nowSecs = QDateTime::currentSecsSinceEpoch();
        DataChannelTransport::setWatchRelayClockForTest([&nowSecs]() { return nowSecs; });
        const auto restoreClock = qScopeGuard(
            []() { DataChannelTransport::setWatchRelayClockForTest({}); });
        DataChannelTransport::WatchRelayGrant grant{
            QUrl(QStringLiteral("wss://relay.example/v1/relay")),
            QStringLiteral("core-watch"), nowSecs + 8, primaryLeg};
        QVERIFY(primary->setWatchRelayGrant(grant));
        DataChannelTransport::setSelectedPathOverrideForTest(
            [primary](const DataChannelTransport* channel) -> std::optional<MediaIcePath> {
            if (channel != primary) {
                return std::nullopt;
            }
            MediaIcePath path;
            path.remoteAddress = QStringLiteral("127.0.0.1");
            path.ownedLoopbackShim = true;
            return path;
        });
        const auto latest = [&messages](SessionMessageKind kind, quint32 id = 0) {
            for (const SessionMessage& message : messages) {
                if (message.kind == kind && (id == 0 || message.commandId == id)) {
                    return message;
                }
            }
            return SessionMessage{};
        };
        QTRY_VERIFY(latest(SessionMessageKind::Hello).kind == SessionMessageKind::Hello);
        const SessionMessage serverHello = latest(SessionMessageKind::Hello);
        QString fingerprint = station.certificateFingerprint();
        fingerprint.remove(QLatin1Char(':'));
        const QByteArray certHash = QByteArray::fromHex(fingerprint.toLatin1());
        const SessionDeviceBlock proof{
            StationIdentity::toBase64Url(record.id),
            StationIdentity::toBase64Url(device.publicKeySpki()),
            record.name, record.kind,
            StationIdentity::toBase64Url(device.sign(DeviceAuthenticator::transcript(
                StationIdentity::fromBase64Url(serverHello.challenge), certHash,
                station.stationIdentity().publicKeySpki(), device.publicKeySpki())))};
        const auto sendPrimary = [client](const SessionMessage& message) {
            client->sendText(SessionMessages::encode(message));
        };
        sendPrimary(SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 0,
            record.name, {kSessionProtocolMajor},
            {{"deviceAuth", 1}, {"remoteTx", 1}, {"txWatchPath", 1}, {"txWatchRelay", 1}}));
        sendPrimary(SessionMessages::authRequest(QString(), proof));
        QTRY_VERIFY(station.authenticatedSessionCount() == 1);
        QTRY_VERIFY(latest(SessionMessageKind::SnapshotComplete).kind
                    == SessionMessageKind::SnapshotComplete);
        QVERIFY(primary->canOpenWatchRelay());
        // Load findings 3: the watch path is published by its own
        // capability update, which can come after the snapshot; wait for
        // it rather than read it at SnapshotComplete.
        const auto advertised = [&messages]() {
            for (const SessionMessage& message : messages) {
                if (message.kind != SessionMessageKind::Capabilities) {
                    continue;
                }
                for (const MirrorUpdate& update : message.updates) {
                    if (update.name == QByteArrayLiteral("txWatchPathVersion")
                        && update.value.toInt() == 1) {
                        return true;
                    }
                }
            }
            return false;
        };
        QTRY_VERIFY(advertised());

        sendPrimary(SessionMessages::commandInvoke(QByteArrayLiteral("tx.watchRelay"), 800,
            {{0, "offer", MirrorWireKind::Utf8,
              QString(IMediaTransport::kMaxDescriptionBytes + 1, QLatin1Char('x'))}}));
        QTRY_VERIFY(latest(SessionMessageKind::CommandResult, 800).commandId == 800);
        QVERIFY(!latest(SessionMessageKind::CommandResult, 800).accepted);

        sendPrimary(SessionMessages::commandInvoke(QByteArrayLiteral("tx.watchRelay"), 801,
            {{0, "offer", MirrorWireKind::Utf8, QStringLiteral("bad")}}));
        QTRY_VERIFY(latest(SessionMessageKind::CommandResult, 801).commandId == 801);
        QVERIFY(!latest(SessionMessageKind::CommandResult, 801).accepted);

        auto clientWatchLeg = RelayLeg::createWatch();
        QVERIFY(clientWatchLeg);
        clientWatchLeg->open(grant.url, QStringLiteral("client-watch"));
        QTRY_COMPARE(clientWatchLeg->state(), RelayLeg::State::Joined);
        IceConfiguration ice = IceConfiguration::throughRendezvous({}, true, {}, {});
        ice.setRelay(std::nullopt, 1);
        ice.setCandidateSourceFactory(RelayLeg::factoryFor(clientWatchLeg), true);
        DataChannelTransport watch;
        DataChannelTransport::Options options;
        options.role = DataChannelTransport::Role::Offerer;
        options.purpose = DataChannelTransport::Purpose::TxWatch;
        options.maxIncomingBytes = DataChannelTransport::kMaxWatchFrameBytes;
        options.ice = ice;
        QSignalSpy localOffer(&watch, &DataChannelTransport::localDescription);
        QSignalSpy attached(&watch, &SessionTransport::binaryReceived);
        QVERIFY(watch.start(options));
        QTRY_VERIFY(!localOffer.isEmpty());
        const QString offer = localOffer.first().first().toString();
        sendPrimary(SessionMessages::commandInvoke(QByteArrayLiteral("tx.watchRelay"), 802,
            {{0, "offer", MirrorWireKind::Utf8, offer}}));
        QTRY_VERIFY_WITH_TIMEOUT(latest(SessionMessageKind::CommandResult, 802).commandId == 802,
                                 10000);
        QVERIFY(!latest(SessionMessageKind::CommandResult, 802).accepted);
        QTest::qWait(TxWatchServer::kIssueIntervalMs);
        sendPrimary(SessionMessages::commandInvoke(QByteArrayLiteral("tx.watchRelay"), 803,
            {{0, "offer", MirrorWireKind::Utf8, offer}}));
        QTRY_VERIFY_WITH_TIMEOUT(latest(SessionMessageKind::CommandResult, 803).commandId == 803,
                                 10000);
        const SessionMessage result = latest(SessionMessageKind::CommandResult, 803);
        QVERIFY2(result.accepted, qPrintable(result.reason));
        QCOMPARE(result.updates.size(), 4);
        QCOMPARE(result.updates.at(2).value.toString(), QStringLiteral("relay-dtls-v1"));
        bool canonical = false;
        const QByteArray ticket = StationIdentity::fromBase64Url(
            result.updates.at(0).value.toString(), &canonical);
        QVERIFY(canonical);
        QCOMPARE(ticket.size(), TxWatchServer::kTicketBytes);
        QVERIFY(watch.acceptDescription(result.updates.at(3).value.toString(),
                                        QStringLiteral("answer")));
        QTRY_VERIFY_WITH_TIMEOUT(watch.isOpen(), 10000);
        QVERIFY(watch.sendBinary(QByteArray(1, char(1)) + ticket));
        QTRY_VERIFY(!attached.isEmpty());
        QCOMPARE(attached.first().first().toByteArray(), QByteArray::fromHex("0100"));
        RemoteTxWatchdog* watchdog = station.txWatchdog();
        QVERIFY(watchdog);
        QSignalSpy heard(watchdog, &RemoteTxWatchdog::keepaliveHeard);
        watchdog->setKeyed(record.id, true, 9);
        QVERIFY(watch.sendBinary(RemoteTxWatchdog::channelKeepalive(1, 9)));
        QTRY_COMPARE(heard.size(), 1);
        watchdog->setKeyed(record.id, false);
        QVERIFY(!watchFrames.isEmpty());
        QCOMPARE(legs.size(), 3);
        const auto lastWatchVersion = [&messages]() {
            for (auto message = messages.crbegin(); message != messages.crend(); ++message) {
                if (message->kind != SessionMessageKind::Capabilities) {
                    continue;
                }
                for (const MirrorUpdate& update : message->updates) {
                    if (update.name == QByteArrayLiteral("txWatchPathVersion")) {
                        return update.value.toInt();
                    }
                }
                return 0; // a zero route is omitted from the descriptor
            }
            return -1; // no descriptor yet
        };
        QCOMPARE(lastWatchVersion(), 1);
        // The grant expires: no new watch may open, and the route the
        // attached watch uses stays.
        nowSecs = grant.expires;
        QVERIFY(!primary->canOpenWatchRelay());
        QVERIFY(primary->hasWatchRelayRoute());
        // A capability publication after the expiry (the heartbeat's, run
        // here at once) keeps the watch path advertised for the live
        // binding. The 805 answer, on the same channel after it, is the
        // barrier: anything the publication sent has arrived by then.
        station.setRemoteTransmitAllowed(true);
        sendPrimary(SessionMessages::commandInvoke(QByteArrayLiteral("tx.watchRelay"), 805,
            {{0, "offer", MirrorWireKind::Utf8, offer}}));
        // Load findings 3: the same bound 802 and 803 wait under, the
        // product's own ticket lifetime (the refusal itself is immediate;
        // the time is the command's trip over the primary's data channel).
        QTRY_VERIFY_WITH_TIMEOUT(latest(SessionMessageKind::CommandResult, 805).commandId == 805,
                                 TxWatchServer::kTicketLifetimeMs);
        QCOMPARE(latest(SessionMessageKind::CommandResult, 805).reason,
                 QStringLiteral("The Core cannot relay the transmit watch connection for this device."));
        QCOMPARE(lastWatchVersion(), 1);
        watchdog->setKeyed(record.id, true, 10);
        QVERIFY(watch.sendBinary(RemoteTxWatchdog::channelKeepalive(2, 10)));
        QTRY_COMPARE(heard.size(), 2);
        QVERIFY(watch.isOpen());
        watchdog->setKeyed(record.id, false);
        primaryLeg->close();
        // The route is gone; the next publication (run here at once, as the
        // heartbeat would) withdraws the watch path.
        QTRY_VERIFY(!primary->hasWatchRelayRoute());
        station.setRemoteTransmitAllowed(true);
        QTRY_COMPARE(lastWatchVersion(), 0);
        QVERIFY(watch.sendBinary(RemoteTxWatchdog::channelKeepalive(2, 9)));
        QTRY_VERIFY(!watch.isOpen());
        QCOMPARE(heard.size(), 2); // a vanished primary route cannot refresh the watch
        client->closeLink(QStringLiteral("primary ended"));
        QTRY_VERIFY(station.authenticatedSessionCount() == 0);
        station.close();
        primaryLeg->close();
        clientWatchLeg->close();
        client->deleteLater();
    }
};

QTEST_MAIN(TstStationWatchRelay)
#include "tst_station_watch_relay.moc"
