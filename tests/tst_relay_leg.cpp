// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_relay_leg.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08; the rendezvous
// document, section 12.8): the Core's relay-leg runner. It plays the relay
// towards a RelayLeg on the connection named `core` of every fixture in
// rendezvous/conformance/v1/relay/ whose `runs` holds "core", with the
// leg's two ICE agents played by loopback UDP sockets behind its shim, and
// checks the leg's own rules besides: the lanes' queues drop their oldest,
// a datagram over 1500 bytes is never sent, each END code's words are
// plain, and a connection's candidate source claims and lets go its lane.
//
// Loopback only; nothing leaves this computer.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: closing a leg whose WebSocket is still opening writes
//               nothing to it (aLegClosedWhileOpeningWritesNothing).
// =================================================================

#include <QtTest>

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QRegularExpression>
#include <QNetworkProxy>
#include <QPointer>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QUuid>
#include <QWebSocket>
#include <QWebSocketServer>

#include <memory>
#include <thread>

#include "OperatorWording.h"
#include "core/security/StationIdentity.h"
#include "core/session/RelayLeg.h"
#include "core/session/SystemProxy.h"
#include "fakes/LoginProxy.h"

using namespace NereusSDR;

namespace {

const QString kSuite = QStringLiteral(NEREUS_SOURCE_DIR "/rendezvous/conformance/v1/relay");

QJsonObject readJson(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

QByteArray randomBytes(int count)
{
    QByteArray bytes(count, Qt::Uninitialized);
    for (int i = 0; i < count; ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return bytes;
}

// One connection the runner accepted from the leg, with what it received.
struct Accepted {
    QPointer<QWebSocket> socket;
    QList<QByteArray> received;
    bool endSent = false;
};

// Plays the relay towards one leg (section 12.8's core runner).
class RelayPlayer {
public:
    RelayPlayer()
    {
        server = std::make_unique<QWebSocketServer>(QStringLiteral("relay"),
                                                    QWebSocketServer::NonSecureMode);
        server->listen(QHostAddress::LocalHost, 0);
        QObject::connect(server.get(), &QWebSocketServer::newConnection, server.get(), [this] {
            while (QWebSocket* socket = server->nextPendingConnection()) {
                auto accepted = std::make_shared<Accepted>();
                accepted->socket = socket;
                QObject::connect(socket, &QWebSocket::binaryMessageReceived, socket,
                                 [accepted](const QByteArray& message) {
                                     accepted->received.append(message);
                                 });
                pending.append(accepted);
            }
        });
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("ws://127.0.0.1:%1/v1/relay").arg(server->serverPort()));
    }

    std::shared_ptr<Accepted> waitForConnection(int ms = 8000)
    {
        if (!QTest::qWaitFor([this] { return !pending.isEmpty(); }, ms)) {
            return nullptr;
        }
        return pending.takeFirst();
    }

    std::unique_ptr<QWebSocketServer> server;
    QList<std::shared_ptr<Accepted>> pending;
};

// A token for a placeholder key, the same every time (section 12.8).
QString tokenFor(QHash<QString, QString>& tokens, const QString& key)
{
    auto it = tokens.find(key);
    if (it == tokens.end()) {
        it = tokens.insert(key, StationIdentity::toBase64Url(randomBytes(62)));
    }
    return it.value();
}

// The bytes of a step's `binary` parts, filling placeholders; `sent`
// records `$bytes` values made here.
QByteArray bytesOf(const QJsonArray& parts, QHash<QString, QString>& tokens,
                   QHash<QString, QByteArray>& recorded)
{
    QByteArray out;
    for (const QJsonValue& value : parts) {
        const QString part = value.toString();
        if (part.startsWith(QLatin1String("$token:"))) {
            out.append(tokenFor(tokens, part.mid(7)).toLatin1());
        } else if (part.startsWith(QLatin1String("$bytes:"))) {
            const QStringList fields = part.split(QLatin1Char(':'));
            const QString name = fields.value(2);
            if (!recorded.contains(name)) {
                recorded.insert(name, randomBytes(fields.value(1).toInt()));
            }
            out.append(recorded.value(name));
        } else if (part.startsWith(QLatin1String("$ref:"))) {
            out.append(recorded.value(part.mid(5)));
        } else {
            out.append(QByteArray::fromHex(part.toLatin1()));
        }
    }
    return out;
}

} // namespace

class TstRelayLeg final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void cleanup() { SystemProxy::setProxyForTest(std::nullopt); }

    void routeObservationFollowsUsablePeerAndCurrentSocket()
    {
        RelayPlayer relay;
        auto leg = RelayLeg::create();
        QVERIFY(leg);
        auto source = leg->sourceFor(IceConfiguration::kControlLane);
        QVERIFY(source);
        source->start([](const QString&) {});
        QVERIFY(!source->networkPathSnapshot());
        leg->open(relay.url(), QStringLiteral("test-token"));
        auto first = relay.waitForConnection();
        QVERIFY(first);
        QTRY_VERIFY(!first->received.isEmpty());
        first->socket->sendBinaryMessage(QByteArray("\x81\x01\x00", 3));
        QTRY_COMPARE(leg->state(), RelayLeg::State::Joined);
        QVERIFY(!source->networkPathSnapshot()); // Joined, but no peer.
        first->socket->sendBinaryMessage(QByteArray("\x82\x01", 2));
        QTRY_VERIFY(leg->peerPresent());
        const auto route = source->networkPathSnapshot();
        QVERIFY(route);
        QCOMPARE(route->carrier, NetworkPathSnapshot::Carrier::WebRelay);
        QCOMPARE(route->kind, NetworkPathSnapshot::Kind::Relayed);
        QCOMPARE(route->endpoints, NetworkPathSnapshot::Endpoints::Socket);
        QCOMPARE(route->remoteAddress, QStringLiteral("127.0.0.1"));
        QCOMPARE(route->remotePort, relay.server->serverPort());
        QVERIFY(route->localPort != 0);
        std::optional<NetworkPathSnapshot> wrongThread;
        std::thread observer([&] { wrongThread = source->networkPathSnapshot(); });
        observer.join();
        QVERIFY(!wrongThread);
        first->socket->close();
        QTRY_VERIFY(!source->networkPathSnapshot());
        auto next = relay.waitForConnection();
        QVERIFY(next);
        QTRY_VERIFY(!next->received.isEmpty());
        QVERIFY(!source->networkPathSnapshot());
        next->socket->sendBinaryMessage(QByteArray("\x81\x01\x01", 3));
        QTRY_VERIFY(source->networkPathSnapshot().has_value());
        source->stop();
        QVERIFY(!source->networkPathSnapshot());
        leg->close();
    }

    void coreFixtures_data()
    {
        QTest::addColumn<QString>("file");
        const QJsonArray fixtures =
            readJson(kSuite + QStringLiteral("/manifest.json")).value(QStringLiteral("fixtures")).toArray();
        int count = 0;
        for (const QJsonValue& entry : fixtures) {
            const QString file = entry.toObject().value(QStringLiteral("file")).toString();
            const QJsonArray runs = readJson(kSuite + QLatin1Char('/') + file)
                                        .value(QStringLiteral("runs"))
                                        .toArray();
            if (runs.contains(QJsonValue(QStringLiteral("core")))) {
                QTest::newRow(qPrintable(file)) << file;
                ++count;
            }
        }
        // Every fixture the manifest lists for a Core's leg runs (19 in
        // frame version 1).
        QVERIFY2(count >= 19, qPrintable(QString::number(count)));
    }

    void coreFixtures()
    {
        QFETCH(QString, file);
        const QJsonObject fixture = readJson(kSuite + QLatin1Char('/') + file);
        const QJsonArray steps = fixture.value(QStringLiteral("steps")).toArray();

        RelayPlayer relay;
        QVERIFY(relay.server->isListening());
        RelayLeg leg;
        QVERIFY(leg.bindLanes());
        // The leg's two ICE agents.
        QUdpSocket agents[2];
        for (int lane = 0; lane < 2; ++lane) {
            QVERIFY(agents[lane].bind(QHostAddress::LocalHost, 0));
            leg.setAgentForTest(lane + 1, QHostAddress::LocalHost, agents[lane].localPort());
        }
        QHash<QString, QString> tokens;
        QHash<QString, QByteArray> recorded;
        // The grant: the first token of the leg's own connection.
        QString grantKey;
        for (const QJsonValue& value : steps) {
            const QJsonObject step = value.toObject();
            if (step.value(QStringLiteral("from")).toString() == QLatin1String("core")) {
                for (const QJsonValue& part : step.value(QStringLiteral("binary")).toArray()) {
                    if (part.toString().startsWith(QLatin1String("$token:"))) {
                        grantKey = part.toString().mid(7);
                        break;
                    }
                }
            }
            if (!grantKey.isEmpty()) {
                break;
            }
        }
        if (grantKey.isEmpty()) {
            grantKey = QStringLiteral("core:s:a");
        }
        leg.open(relay.url(), tokenFor(tokens, grantKey));

        std::shared_ptr<Accepted> current;
        int read = 0; // messages of `current` already matched
        const auto nextMessage = [&current, &read](QByteArray* out) {
            if (!current) {
                return false;
            }
            if (!QTest::qWaitFor([&current, &read] { return current->received.size() > read; },
                                 5000)) {
                return false;
            }
            *out = current->received.at(read++);
            return true;
        };
        for (int index = 0; index < steps.size(); ++index) {
            const QJsonObject step = steps.at(index).toObject();
            const QString where = QStringLiteral("%1 step %2").arg(file).arg(index);
            if (step.contains(QStringLiteral("connect"))) {
                if (step.value(QStringLiteral("connect")).toString() != QLatin1String("core")) {
                    continue;
                }
                current = relay.waitForConnection();
                QVERIFY2(current, qPrintable(where + QStringLiteral(": the leg did not connect")));
                read = 0;
                continue;
            }
            if (step.contains(QStringLiteral("from"))) {
                const QString from = step.value(QStringLiteral("from")).toString();
                const QByteArray bytes =
                    bytesOf(step.value(QStringLiteral("binary")).toArray(), tokens, recorded);
                if (from != QLatin1String("core")
                    || step.value(QStringLiteral("role")).toString() != QLatin1String("behaviour")) {
                    continue; // another connection's, or not the leg's to send
                }
                const auto tag = static_cast<quint8>(bytes.at(0));
                if (tag == RelayLeg::kTagControl || tag == RelayLeg::kTagMedia) {
                    // The runner makes the leg send it: the agent writes the
                    // payload into the lane's socket.
                    agents[tag - 1].writeDatagram(bytes.mid(1), QHostAddress::LocalHost,
                                                  leg.lanePort(tag));
                }
                QByteArray message;
                QVERIFY2(nextMessage(&message),
                         qPrintable(where + QStringLiteral(": the leg sent nothing")));
                QVERIFY2(message == bytes,
                         qPrintable(where + QStringLiteral(": got %1").arg(
                                                QString::fromLatin1(message.left(8).toHex()))));
                continue;
            }
            if (step.contains(QStringLiteral("to"))) {
                const QByteArray bytes =
                    bytesOf(step.value(QStringLiteral("binary")).toArray(), tokens, recorded);
                if (step.value(QStringLiteral("to")).toString() != QLatin1String("core")) {
                    continue;
                }
                QVERIFY2(current && current->socket,
                         qPrintable(where + QStringLiteral(": no connection")));
                current->socket->sendBinaryMessage(bytes);
                const auto tag = static_cast<quint8>(bytes.at(0));
                if (tag == RelayLeg::kTagEnd) {
                    current->endSent = true;
                }
                if (tag == RelayLeg::kTagControl || tag == RelayLeg::kTagMedia) {
                    // The leg writes exactly the payload to that agent, and
                    // nothing reached either agent before it.
                    QUdpSocket& agent = agents[tag - 1];
                    QVERIFY2(QTest::qWaitFor([&agent] { return agent.hasPendingDatagrams(); },
                                             5000),
                             qPrintable(where + QStringLiteral(": nothing reached the agent")));
                    const QNetworkDatagram datagram = agent.receiveDatagram();
                    QVERIFY2(datagram.data() == bytes.mid(1),
                             qPrintable(where + QStringLiteral(": another payload reached it")));
                    QVERIFY2(!agents[2 - tag].hasPendingDatagrams(),
                             qPrintable(where + QStringLiteral(": the other agent got something")));
                }
                continue;
            }
            if (step.contains(QStringLiteral("drop"))) {
                if (step.value(QStringLiteral("drop")).toString() == QLatin1String("core")
                    && current && current->socket) {
                    current->socket->abort();
                    current.reset();
                }
                continue;
            }
            if (step.contains(QStringLiteral("disconnect"))) {
                if (step.value(QStringLiteral("disconnect")).toString() == QLatin1String("core")
                    && current && current->socket) {
                    current->socket->close();
                    current.reset();
                }
                continue;
            }
            if (step.contains(QStringLiteral("expectClosed"))) {
                if (step.value(QStringLiteral("expectClosed")).toString() == QLatin1String("core")
                    && current && current->socket) {
                    const int code = step.value(QStringLiteral("code")).toInt(1000);
                    current->socket->close(static_cast<QWebSocketProtocol::CloseCode>(code));
                    current.reset();
                }
                continue;
            }
            if (step.contains(QStringLiteral("expectSilent"))) {
                if (step.value(QStringLiteral("expectSilent")).toString() != QLatin1String("core")) {
                    continue;
                }
                const int before = current ? static_cast<int>(current->received.size()) : 0;
                QTest::qWait(1000);
                if (current && !current->endSent) {
                    QVERIFY2(current->received.size() == before,
                             qPrintable(where + QStringLiteral(": the leg sent something")));
                }
                QVERIFY2(relay.pending.isEmpty(),
                         qPrintable(where + QStringLiteral(": the leg opened a connection")));
                continue;
            }
            // advanceMs and shutdown: the leg keeps no clock the runner
            // moves; shutdown's END is the next step's.
        }
        // A datagram the leg had to drop reached no agent.
        QTest::qWait(100);
        QVERIFY2(!agents[0].hasPendingDatagrams() && !agents[1].hasPendingDatagrams(),
                 qPrintable(file + QStringLiteral(": a dropped datagram reached an agent")));
        leg.close();
    }

    // The words each END code shows are plain, and codes that show none
    // show none.
    void everyEndCodesWordsArePlain()
    {
        for (const char* code : {"protocolError", "timeout", "badToken", "expired", "ended",
                                 "full", "tooManyConnections", "tooManySessions", "peerGone",
                                 "shuttingDown", "lost", "somethingNew"}) {
            const QString words = RelayLeg::wordsFor(QLatin1String(code));
            QVERIFY2(!words.isEmpty(), code);
            QVERIFY2(OperatorWording::isPlain(words), qPrintable(words));
        }
        QVERIFY(RelayLeg::wordsFor(QStringLiteral("replaced")).isEmpty());
        QVERIFY(RelayLeg::wordsFor(QStringLiteral("idle")).isEmpty());
    }

    // What the leg will not send, and the lanes' bounded queues: a
    // datagram over 1500 bytes is dropped, never sent; with the relay not
    // reading, each lane holds at most its bound and drops its oldest.
    void theLegSendsNothingOversizeAndBoundsItsQueues()
    {
        RelayPlayer relay;
        RelayLeg leg;
        QVERIFY(leg.bindLanes());
        QUdpSocket agent;
        QVERIFY(agent.bind(QHostAddress::LocalHost, 0));
        leg.open(relay.url(), QStringLiteral("tok"));
        std::shared_ptr<Accepted> current = relay.waitForConnection();
        QVERIFY(current);
        QTRY_VERIFY(!current->received.isEmpty()); // JOIN
        agent.writeDatagram(QByteArray(RelayLeg::kMaxDatagramBytes + 1, 'x'), QHostAddress::LocalHost,
                            leg.lanePort(1));
        QTRY_COMPARE(leg.droppedOversize(), quint64(1));
        agent.writeDatagram(QByteArray(RelayLeg::kMaxDatagramBytes, 'y'), QHostAddress::LocalHost,
                            leg.lanePort(1));
        QTRY_COMPARE(current->received.size(), qsizetype(2));
        QCOMPARE(current->received.at(1).size(), RelayLeg::kMaxDatagramBytes + 1);
        QCOMPARE(static_cast<quint8>(current->received.at(1).at(0)), RelayLeg::kTagControl);

        // A relay that takes the connection and never answers the
        // WebSocket upgrade: nothing can be written, so each lane keeps
        // its newest kQueueFrames and drops the rest, oldest first.
        QTcpServer silent;
        QVERIFY(silent.listen(QHostAddress::LocalHost));
        RelayLeg stuck;
        QVERIFY(stuck.bindLanes());
        stuck.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/v1/relay").arg(silent.serverPort())),
                   QStringLiteral("tok"));
        QTRY_VERIFY(silent.hasPendingConnections());
        const int sent = RelayLeg::kQueueFrames + 36;
        for (int i = 0; i < sent; ++i) {
            agent.writeDatagram(QByteArray(20, 'z'), QHostAddress::LocalHost, stuck.lanePort(2));
        }
        QTRY_COMPARE(stuck.droppedQueueFull(), quint64(36));
        QCOMPARE(stuck.datagramsSent(), quint64(0));
    }

    // A leg closed while its WebSocket is still opening (the relay took the
    // TCP connection and has not answered the upgrade) writes nothing to
    // it: there is no open WebSocket to send a close frame on, so the
    // socket is aborted and the relay sees the connection go.
    void aLegClosedWhileOpeningWritesNothing()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral("QNativeSocketEngine::write")));
        QTcpServer silent;
        QVERIFY(silent.listen(QHostAddress::LocalHost));
        RelayLeg leg;
        QVERIFY(leg.bindLanes());
        leg.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/v1/relay").arg(silent.serverPort())),
                 QStringLiteral("tok"));
        QTRY_VERIFY(silent.hasPendingConnections());
        QTcpSocket* accepted = silent.nextPendingConnection();
        QVERIFY(accepted != nullptr);
        // The upgrade request arrives; it is never answered.
        QTRY_VERIFY(accepted->bytesAvailable() > 0);
        accepted->readAll();
        leg.close();
        QTRY_VERIFY(accepted->state() == QAbstractSocket::UnconnectedState);
        QCOMPARE(accepted->bytesAvailable(), qint64(0));
    }

    // A connection's candidate source gives its agent the lane socket's
    // candidate, at the lowest priority; another connection cannot take
    // over the same legacy lane while the first ICE owner still holds it.
    void aSourceClaimsItsLane()
    {
        std::shared_ptr<RelayLeg> leg = RelayLeg::create();
        QVERIFY(leg);
        IceConfiguration ice = IceConfiguration::throughRendezvous(
            {}, /*relayAllowed=*/true, IceConfiguration::localAddressFamilies(), HostFamilies{});
        ice.setCandidateSourceFactory(RelayLeg::factoryFor(leg), /*needsRelay=*/true);
        auto first = ice.makeCandidateSource(IceConfiguration::kMediaLane);
        QVERIFY(first);
        QString candidate;
        first->start([&candidate](const QString& line) { candidate = line; });
        QCOMPARE(candidate, QStringLiteral("candidate:wsrelay2 1 UDP 1 127.0.0.1 %1 typ host")
                                .arg(leg->lanePort(2)));
        auto second = ice.makeCandidateSource(IceConfiguration::kMediaLane);
        bool secondOffered = false;
        second->start([&](const QString&) { secondOffered = true; });
        QVERIFY(!secondOffered);
        first->stop();
        second->start([&](const QString&) { secondOffered = true; });
        QVERIFY(secondOffered);
        // `relay = deny` stops the web relay's sources as it stops TURN.
        IceConfiguration denied = IceConfiguration::throughRendezvous(
            {}, /*relayAllowed=*/false, IceConfiguration::localAddressFamilies(), HostFamilies{});
        denied.setCandidateSourceFactory(RelayLeg::factoryFor(leg), /*needsRelay=*/true);
        QVERIFY(!denied.makeCandidateSource(IceConfiguration::kControlLane));
        second->stop();
    }

    void routedMediaKeepsOverlappingAgentsSeparate()
    {
        RelayPlayer relay;
        auto leg = RelayLeg::create();
        QVERIFY(leg);
        IceConfiguration ice = IceConfiguration::throughRendezvous(
            {}, true, IceConfiguration::localAddressFamilies(), HostFamilies{});
        ice.setCandidateSourceFactory(RelayLeg::factoryFor(leg), true);
        ice.setMediaRouting(true);
        QVERIFY(!ice.makeCandidateSource(IceConfiguration::kMediaLane,
                                         QStringLiteral("00000000-0000-0000-0000-000000000000")));
        const QString oldId = QStringLiteral("11111111-1111-4111-8111-111111111111");
        const QString newId = QStringLiteral("22222222-2222-4222-8222-222222222222");
        auto oldSource = ice.makeCandidateSource(IceConfiguration::kMediaLane, oldId);
        auto newSource = ice.makeCandidateSource(IceConfiguration::kMediaLane, newId);
        QVERIFY(oldSource);
        QVERIFY(newSource);
        quint16 oldPort = 0;
        quint16 newPort = 0;
        oldSource->start([&](const QString& line) { oldPort = line.split(' ').at(5).toUShort(); });
        newSource->start([&](const QString& line) { newPort = line.split(' ').at(5).toUShort(); });
        QVERIFY(oldPort != 0);
        QVERIFY(newPort != 0);
        QVERIFY(oldPort != newPort);
        QUdpSocket oldAgent;
        QUdpSocket newAgent;
        QVERIFY(oldAgent.bind(QHostAddress::LocalHost, 0));
        QVERIFY(newAgent.bind(QHostAddress::LocalHost, 0));
        leg->open(relay.url(), QStringLiteral("test-token"));
        auto connection = relay.waitForConnection();
        QVERIFY(connection);
        QTRY_VERIFY(!connection->received.isEmpty()); // JOIN
        oldAgent.writeDatagram("old", QHostAddress::LocalHost, oldPort);
        newAgent.writeDatagram("new", QHostAddress::LocalHost, newPort);
        QTRY_VERIFY(connection->received.size() >= 3);
        const QByteArray oldUuid = QUuid::fromString(oldId).toRfc4122();
        const QByteArray newUuid = QUuid::fromString(newId).toRfc4122();
        QVERIFY(connection->received.contains(QByteArray(1, '\x02') + oldUuid + "old"));
        QVERIFY(connection->received.contains(QByteArray(1, '\x02') + newUuid + "new"));
        QUdpSocket otherSender;
        QVERIFY(otherSender.bind(QHostAddress::LocalHost, 0));
        otherSender.writeDatagram("wrong-old", QHostAddress::LocalHost, oldPort);
        otherSender.writeDatagram("wrong-new", QHostAddress::LocalHost, newPort);
        QTRY_COMPARE(leg->droppedWrongSender(), quint64(2));
        QCOMPARE(connection->received.size(), 3); // JOIN and two original datagrams
        oldAgent.writeDatagram(QByteArray(1485, 'x'), QHostAddress::LocalHost, oldPort);
        QTRY_COMPARE(leg->droppedOversize(), quint64(1));
        connection->socket->sendBinaryMessage(QByteArray(1, '\x02') +
                                              QUuid::fromString(oldId).toRfc4122() +
                                              QByteArray(1485, 'x'));
        QTRY_COMPARE(leg->droppedOversize(), quint64(2));
        connection->socket->sendBinaryMessage(QByteArray(1, '\x02') + oldUuid + "to-old");
        connection->socket->sendBinaryMessage(QByteArray(1, '\x02') + newUuid + "to-new");
        QTRY_VERIFY(oldAgent.hasPendingDatagrams());
        QTRY_VERIFY(newAgent.hasPendingDatagrams());
        QCOMPARE(oldAgent.receiveDatagram().data(), QByteArray("to-old"));
        QCOMPARE(newAgent.receiveDatagram().data(), QByteArray("to-new"));
        QVERIFY(!otherSender.hasPendingDatagrams());
        oldSource->stop();
        connection->socket->sendBinaryMessage(QByteArray(1, '\x02') + oldUuid + "stale");
        connection->socket->sendBinaryMessage(QByteArray(1, '\x02') + newUuid + "live");
        QTRY_VERIFY(newAgent.hasPendingDatagrams());
        QCOMPARE(newAgent.receiveDatagram().data(), QByteArray("live"));
        QVERIFY(!oldAgent.hasPendingDatagrams());
        newSource->stop();
    }

    void legacyMediaKeepsRawTagTwoBytes()
    {
        RelayPlayer relay;
        auto leg = RelayLeg::create();
        QVERIFY(leg);
        IceConfiguration ice = IceConfiguration::throughRendezvous(
            {}, true, IceConfiguration::localAddressFamilies(), HostFamilies{});
        ice.setCandidateSourceFactory(RelayLeg::factoryFor(leg), true);
        auto source = ice.makeCandidateSource(IceConfiguration::kMediaLane);
        QVERIFY(source);
        quint16 port = 0;
        source->start([&](const QString& line) { port = line.split(' ').at(5).toUShort(); });
        QVERIFY(port != 0);
        QUdpSocket agent;
        QVERIFY(agent.bind(QHostAddress::LocalHost, 0));
        leg->open(relay.url(), QStringLiteral("test-token"));
        auto connection = relay.waitForConnection();
        QVERIFY(connection);
        QTRY_VERIFY(!connection->received.isEmpty());
        agent.writeDatagram("legacy", QHostAddress::LocalHost, port);
        QTRY_VERIFY(connection->received.contains(QByteArray(1, '\x02') + "legacy"));
        QUdpSocket otherSender;
        QVERIFY(otherSender.bind(QHostAddress::LocalHost, 0));
        otherSender.writeDatagram("wrong-legacy", QHostAddress::LocalHost, port);
        QTRY_COMPARE(leg->droppedWrongSender(), quint64(1));
        QCOMPARE(connection->received.size(), 2);
        connection->socket->sendBinaryMessage(QByteArray(1, '\x02') + "reply");
        QTRY_VERIFY(agent.hasPendingDatagrams());
        QCOMPARE(agent.receiveDatagram().data(), QByteArray("reply"));
        QVERIFY(!otherSender.hasPendingDatagrams());
        source->stop();
    }

    void watchUsesItsOwnSocketAndOnlyTagThree()
    {
        RelayPlayer relay;
        auto primary = RelayLeg::create();
        auto watch = RelayLeg::createWatch();
        QVERIFY(primary);
        QVERIFY(watch);
        QVERIFY(primary->lanePort(1) != watch->lanePort(1));
        QCOMPARE(watch->lanePort(2), quint16(0));
        QVERIFY(!watch->sourceFor(IceConfiguration::kMediaLane));
        QVERIFY(!watch->sourceFor(IceConfiguration::kMediaLane,
                                  QStringLiteral("11111111-1111-4111-8111-111111111111"), true));
        QVERIFY(!watch->sourceFor(IceConfiguration::kControlLane, {}, true));
        QVERIFY(!watch->sourceFor(IceConfiguration::kControlLane,
                                  QStringLiteral("11111111-1111-4111-8111-111111111111")));
        auto source = watch->sourceFor(IceConfiguration::kControlLane);
        QVERIFY(source);
        QString candidate;
        source->start([&candidate](const QString& line) { candidate = line; });
        QVERIFY(candidate.contains(QString::number(watch->lanePort(1))));

        QUdpSocket primaryAgent;
        QUdpSocket watchAgent;
        QVERIFY(primaryAgent.bind(QHostAddress::LocalHost, 0));
        QVERIFY(watchAgent.bind(QHostAddress::LocalHost, 0));
        primary->setAgentForTest(1, QHostAddress::LocalHost, primaryAgent.localPort());
        watch->setAgentForTest(1, QHostAddress::LocalHost, watchAgent.localPort());
        primary->open(relay.url(), QStringLiteral("primary-secret"));
        auto first = relay.waitForConnection();
        QVERIFY(first);
        watch->open(relay.url(), QStringLiteral("watch-secret"));
        auto second = relay.waitForConnection();
        QVERIFY(second);
        QTRY_VERIFY(!first->received.isEmpty() && !second->received.isEmpty());
        QCOMPARE(first->received.first(), QByteArray("\x80primary-secret", 15));
        QCOMPARE(second->received.first(), QByteArray("\x80watch-secret", 13));

        primaryAgent.writeDatagram("control", QHostAddress::LocalHost, primary->lanePort(1));
        watchAgent.writeDatagram("heartbeat", QHostAddress::LocalHost, watch->lanePort(1));
        QTRY_VERIFY(first->received.contains(QByteArray(1, '\x01') + "control"));
        QTRY_VERIFY(second->received.contains(QByteArray("\x03heartbeat", 10)));
        QCOMPARE(first->received.size(), 2);
        QCOMPARE(second->received.size(), 2);

        first->socket->sendBinaryMessage(QByteArray("\x03reserved", 9));
        second->socket->sendBinaryMessage(QByteArray(1, '\x01') + "control");
        second->socket->sendBinaryMessage(QByteArray("\x02media", 6));
        QTRY_COMPARE(primary->droppedUnknownTag(), quint64(1));
        QTRY_COMPARE(watch->droppedUnknownTag(), quint64(2));
        QVERIFY(!primaryAgent.hasPendingDatagrams());
        QVERIFY(!watchAgent.hasPendingDatagrams());

        second->socket->sendBinaryMessage(QByteArray("\x03response", 9));
        QTRY_VERIFY(watchAgent.hasPendingDatagrams());
        QCOMPARE(watchAgent.receiveDatagram().data(), QByteArray("response"));
        QVERIFY(!primaryAgent.hasPendingDatagrams());
        // LINK minor 11: a replaced leg ends as every other end does, its
        // ended() said with no words.
        QSignalSpy watchEnded(watch.get(), &RelayLeg::ended);
        second->socket->sendBinaryMessage(QByteArray("\x83replaced", 9));
        QTRY_COMPARE(watch->state(), RelayLeg::State::Ended);
        QTRY_COMPARE(watchEnded.size(), 1);
        QCOMPARE(watchEnded.first().at(0).toString(), QStringLiteral("replaced"));
        QVERIFY(watchEnded.first().at(1).toString().isEmpty());
        second->socket->close();
        QCOMPARE(primary->state(), RelayLeg::State::Connecting);
        primaryAgent.writeDatagram("still-live", QHostAddress::LocalHost, primary->lanePort(1));
        QTRY_VERIFY(first->received.contains(QByteArray("\x01still-live", 11)));
        source->stop();
    }

    void watchQueueHasItsOwnFrameAndByteBounds()
    {
        QTcpServer silent;
        QVERIFY(silent.listen(QHostAddress::LocalHost));
        auto watch = RelayLeg::createWatch();
        QVERIFY(watch);
        watch->open(QUrl(QStringLiteral("ws://127.0.0.1:%1/v1/relay").arg(silent.serverPort())),
                    QStringLiteral("watch-secret"));
        QTRY_VERIFY(silent.hasPendingConnections());
        QUdpSocket agent;
        QVERIFY(agent.bind(QHostAddress::LocalHost, 0));
        for (int i = 0; i < RelayLeg::kWatchQueueFrames + 4; ++i) {
            agent.writeDatagram(QByteArray(20, 'x'), QHostAddress::LocalHost, watch->lanePort(1));
        }
        QTRY_COMPARE(watch->droppedQueueFull(), quint64(4));
        for (int i = 0; i < 6; ++i) {
            agent.writeDatagram(QByteArray(1500, 'y'), QHostAddress::LocalHost, watch->lanePort(1));
        }
        QTRY_COMPARE(watch->droppedQueueFull(), quint64(21));
        QCOMPARE(watch->datagramsSent(), quint64(0));
    }

    // A network whose proxy demands a login: when the leg gives up, its
    // words say why (NereusSDR has no login to give the proxy).
    void aProxyThatNeedsALoginIsSaidPlainly()
    {
        NereusSDR::Test::LoginProxy proxy;
        QVERIFY(proxy.listen());
        proxy.useAsSystemProxy();
        auto leg = RelayLeg::create();
        QVERIFY(leg);
        QSignalSpy ended(leg.get(), &RelayLeg::ended);
        leg->open(QUrl(QStringLiteral("ws://127.0.0.1:9/v1/relay")),
                  QStringLiteral("proxy-token"));
        QTRY_VERIFY(proxy.requests() >= 1);
        // The leg joins again for its rejoin window before it gives up.
        QTRY_COMPARE_WITH_TIMEOUT(ended.size(), 1, RelayLeg::kRejoinWindowMs + 15000);
        QCOMPARE(ended.at(0).at(0).toString(), QStringLiteral("lost"));
        QCOMPARE(ended.at(0).at(1).toString(),
                 QStringLiteral("This network's proxy needs a login, which NereusSDR can't provide."));
    }

    void aLocalConnectProxyCarriesTheWebRelayLeg()
    {
        RelayPlayer relay;
        QTcpServer proxy;
        QVERIFY(proxy.listen(QHostAddress::LocalHost, 0));
        bool sawConnect = false;
        QObject::connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
            QTcpSocket* downstream = proxy.nextPendingConnection();
            auto* upstream = new QTcpSocket(&proxy);
            QObject::connect(downstream, &QTcpSocket::readyRead, &proxy,
                             [&, downstream, upstream] {
                static QByteArray request;
                if (!sawConnect) {
                    request += downstream->readAll();
                    if (!request.contains("\r\n\r\n")) { return; }
                    sawConnect = request.startsWith("CONNECT ");
                    if (!sawConnect) { return; }
                    upstream->connectToHost(QHostAddress::LocalHost, relay.server->serverPort());
                    QObject::connect(upstream, &QTcpSocket::connected, &proxy,
                                     [downstream] {
                        downstream->write("HTTP/1.1 200 Connection Established\r\n\r\n");
                    });
                    request.clear();
                    return;
                }
                upstream->write(downstream->readAll());
            });
            QObject::connect(upstream, &QTcpSocket::readyRead, &proxy,
                             [downstream, upstream] { downstream->write(upstream->readAll()); });
            QObject::connect(downstream, &QTcpSocket::disconnected, upstream,
                             &QTcpSocket::disconnectFromHost);
            QObject::connect(upstream, &QTcpSocket::disconnected, downstream,
                             &QTcpSocket::disconnectFromHost);
        });
        SystemProxy::setProxyForTest(QNetworkProxy(QNetworkProxy::HttpProxy,
                                                  QStringLiteral("127.0.0.1"), proxy.serverPort()));
        auto leg = RelayLeg::create();
        QVERIFY(leg);
        leg->open(relay.url(), QStringLiteral("proxy-token"));
        auto connection = relay.waitForConnection();
        QVERIFY(connection);
        QTRY_VERIFY(sawConnect);
        QTRY_VERIFY(!connection->received.isEmpty());
        QCOMPARE(connection->received.first(), QByteArray("\x80proxy-token", 12));
        leg->close();
    }
};

QTEST_MAIN(TstRelayLeg)
#include "tst_relay_leg.moc"
