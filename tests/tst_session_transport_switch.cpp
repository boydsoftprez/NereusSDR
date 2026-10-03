// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_session_transport_switch.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 (R-IOS-16, R-IOS-08; the link document, section
// 21.2): a session moves to another connection make-before-break, beneath
// the session, ordered by the path.switch barrier, with no close, sign-in
// or snapshot.
//
//   - SwitchableTransport alone: every message both ways arrives once and
//     in order across a move, over connections of different delays; the
//     barrier never reaches the session; an old connection lost mid-move
//     counts as the barrier; a client that hears no barrier gives up and
//     stays; a Core that hears none ends the session; what a move holds
//     is bounded.
//   - StationServer and StationClient: a signed-in session moves to a new
//     connection with a ticket; it keeps its epoch, snapshot and sign-in;
//     deltas and writes carry on over the new connection; the old one
//     closes. A wrong or spent ticket ends only the new connection. A Core
//     on the air refuses the ticket and the join ("Not while the radio is
//     transmitting."); nothing keys.
//
// Everything runs over in-process links; no network, no radio, no audio
// device.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>

#include <memory>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/SwitchableTransport.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "OperatorWording.h"
#include "core/session/RendezvousDialer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "core/ConnectionState.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

// A link whose deliveries each take `delayMs`, in order: one path of a
// move that is slower or faster than the other.
class DelayLink final : public SessionTransport {
public:
    DelayLink(const QString& description, int delayMs)
        : m_description(description), m_delayMs(delayMs)
    {
    }

    static void pair(DelayLink* a, DelayLink* b)
    {
        a->m_peer = b;
        b->m_peer = a;
    }

    void sendText(const QByteArray& wire) override
    {
        if (!m_open || m_peer.isNull()) {
            return;
        }
        const QPointer<DelayLink> peer(m_peer);
        QTimer::singleShot(m_delayMs, peer, [peer, wire] {
            if (peer && peer->m_open) {
                emit peer->textReceived(wire);
            }
        });
    }
    void ping() override
    {
        const QPointer<DelayLink> peer(m_peer);
        const QPointer<DelayLink> self(this);
        QTimer::singleShot(m_delayMs * 2, this, [self] {
            if (self && self->m_open) {
                emit self->pongReceived();
            }
        });
    }
    void closeLink(const QString&) override
    {
        if (!m_open) {
            return;
        }
        m_open = false;
        emit closed();
        const QPointer<DelayLink> peer(m_peer);
        QTimer::singleShot(m_delayMs, peer, [peer] {
            if (peer) {
                peer->closeLink(QString());
            }
        });
    }
    bool isOpen() const override { return m_open; }
    QString peerDescription() const override { return m_description; }
    std::optional<NetworkPathSnapshot> networkPathSnapshot() const override
    {
        if (!m_open) { return std::nullopt; }
        NetworkPathSnapshot path;
        path.kind = NetworkPathSnapshot::Kind::Direct;
        path.carrier = NetworkPathSnapshot::Carrier::WebSocket;
        path.endpoints = NetworkPathSnapshot::Endpoints::Socket;
        path.remoteAddress = QStringLiteral("127.0.0.1");
        path.remotePort = static_cast<quint16>(10000 + m_delayMs);
        return path;
    }

private:
    QString m_description;
    int m_delayMs = 0;
    QPointer<DelayLink> m_peer;
    bool m_open = true;
};

QByteArray numbered(const char* from, int n)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("type"), QStringLiteral("test.message")},
                                     {QStringLiteral("from"), QLatin1String(from)},
                                     {QStringLiteral("n"), n}})
        .toJson(QJsonDocument::Compact);
}

QList<int> numbersIn(const QList<QByteArray>& wires)
{
    QList<int> out;
    for (const QByteArray& wire : wires) {
        out.append(QJsonDocument::fromJson(wire).object().value(QStringLiteral("n")).toInt());
    }
    return out;
}

QList<int> oneTo(int count)
{
    QList<int> out;
    for (int i = 1; i <= count; ++i) {
        out.append(i);
    }
    return out;
}

// Both ends of one session over switchable transports, starting on an old
// link, with a new link ready to take over.
struct SwitchPair {
    SwitchableTransport* station = nullptr;
    SwitchableTransport* client = nullptr;
    DelayLink* newStation = nullptr;
    DelayLink* newClient = nullptr;
    QList<QByteArray> atStation;
    QList<QByteArray> atClient;

    SwitchPair(int oldDelayMs, int newDelayMs)
    {
        auto* oldStation = new DelayLink(QStringLiteral("old station end"), oldDelayMs);
        auto* oldClient = new DelayLink(QStringLiteral("old client end"), oldDelayMs);
        DelayLink::pair(oldStation, oldClient);
        station = new SwitchableTransport(oldStation, SwitchableTransport::Side::Station,
                                          1024 * 1024);
        client = new SwitchableTransport(oldClient, SwitchableTransport::Side::Client,
                                         1024 * 1024);
        newStation = new DelayLink(QStringLiteral("new station end"), newDelayMs);
        newClient = new DelayLink(QStringLiteral("new client end"), newDelayMs);
        DelayLink::pair(newStation, newClient);
        QObject::connect(station, &SessionTransport::textReceived,
                         [this](const QByteArray& wire) { atStation.append(wire); });
        QObject::connect(client, &SessionTransport::textReceived,
                         [this](const QByteArray& wire) { atClient.append(wire); });
    }
    ~SwitchPair()
    {
        // A new link a move took is its switchable's child by now.
        const QPointer<DelayLink> newStationLeft(newStation);
        const QPointer<DelayLink> newClientLeft(newClient);
        delete station;
        delete client;
        delete newStationLeft.data();
        delete newClientLeft.data();
    }
};

// A Core and a window signed in to it over one in-process link, and a
// second link the session can move to.
struct Session {
    QTemporaryDir directory;
    AppSettings settings;
    RadioModel radio;
    std::unique_ptr<StationServer> server;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* stationA = nullptr;
    LoopbackTransport* clientA = nullptr;
    LoopbackTransport* stationB = nullptr;
    LoopbackTransport* clientB = nullptr;
    QList<QByteArray> onB;

    Session()
        : settings(directory.filePath(QStringLiteral("station.settings")))
    {
        server = std::make_unique<StationServer>(
            &radio, settings, NereusSDR::Test::seedUpgradedCoreToken(directory.path()));
        server->setHeartbeatIntervalMs(0);
        client = std::make_unique<StationClient>(&remote, &proxy);
        client->setHeartbeatIntervalMs(0);
        radio.addSlice();
    }

    bool signIn()
    {
        stationA = new LoopbackTransport(QStringLiteral("station A"));
        clientA = new LoopbackTransport(QStringLiteral("client A"));
        stationA->linkTo(clientA);
        client->startSession(clientA, server->token());
        server->acceptTransport(stationA);
        return QTest::qWaitFor([this] { return client->isHandshakeComplete(); }, 10000);
    }

    // The new connection, up to the Core's hello on it (what a race reads
    // before it hands a connection over).
    bool openB()
    {
        stationB = new LoopbackTransport(QStringLiteral("station B"));
        clientB = new LoopbackTransport(QStringLiteral("client B"));
        stationB->linkTo(clientB);
        QObject::connect(clientB, &SessionTransport::textReceived,
                         [this](const QByteArray& wire) { onB.append(wire); });
        server->acceptTransport(stationB);
        if (!QTest::qWaitFor([this] { return !onB.isEmpty(); }, 5000)) {
            return false;
        }
        SessionMessage hello;
        if (!SessionMessages::decode(onB.first(), &hello)
            || hello.kind != SessionMessageKind::Hello) {
            return false;
        }
        QObject::disconnect(clientB, &SessionTransport::textReceived, nullptr, nullptr);
        return true;
    }
};

// A radio that keys, as the several-devices tests stand it up
// (MultiDeviceHarness.h allowTransmit): connected, transmit allowed, MOX
// with no delays, the radio's own microphone, a slice on 20 m USB.
MoxController* makeKeyable(Session& s)
{
    s.radio.setBoardForTest(HPSDRHW::HermesLite);
    s.radio.setConnectionStateForTest(ConnectionState::Connected);
    MoxController* mox = s.radio.moxController();
    mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
    s.radio.transmitModel().setMicSourceLocked(false);
    s.radio.transmitModel().setMicSource(MicSource::Radio);
    if (SliceModel* slice = s.radio.slices().value(0)) {
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14200000.0);
    }
    s.server->setRemoteTransmitAllowed(true);
    return mox;
}

// The ticket session.pathTicket gives, read off the window's wire (the
// window routes its own ticket requests to itself alone).
QString requestTicket(Session& s)
{
    QString ticket;
    const QMetaObject::Connection watch = QObject::connect(
        s.clientA, &SessionTransport::textReceived, [&ticket](const QByteArray& wire) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::CommandResult
                && message.commandVerb == "session.pathTicket" && message.accepted) {
                for (const MirrorUpdate& value : message.updates) {
                    if (value.name == "ticket") {
                        ticket = value.value.toString();
                    }
                }
            }
        });
    s.client->invokeCommand(QByteArrayLiteral("session.pathTicket"), {});
    // An empty ticket after the wait is the caller's failure to report.
    const bool arrived = QTest::qWaitFor([&ticket] { return !ticket.isEmpty(); }, 5000);
    Q_UNUSED(arrived);
    QObject::disconnect(watch);
    return ticket;
}

void sendJoin(LoopbackTransport* link, const QString& ticket)
{
    link->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("test"),
        {kSessionProtocolMajor}, {})));
    link->sendText(SessionMessages::encode(SessionMessages::pathJoin(ticket)));
}

} // namespace

class TstSessionTransportSwitch final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    // ── SwitchableTransport alone ─────────────────────────────────────

    void routeObservationFollowsCurrentSendingLink()
    {
        SwitchPair pair(80, 5);
        QVERIFY(pair.station->networkPathSnapshot());
        QCOMPARE(pair.station->networkPathSnapshot()->remotePort, quint16(10080));
        QVERIFY(pair.client->beginClientSwitch(pair.newClient));
        // Client still sends on old until the barrier arrives.
        QCOMPARE(pair.client->networkPathSnapshot()->remotePort, quint16(10080));
        QVERIFY(pair.station->beginStationSwitch(pair.newStation));
        QCOMPARE(pair.station->networkPathSnapshot()->remotePort, quint16(10005));
        QTRY_COMPARE(pair.client->networkPathSnapshot()->remotePort, quint16(10005));
        pair.station->closeLink(QStringLiteral("test done"));
        QVERIFY(!pair.station->networkPathSnapshot());
    }

    // A move from a slow path to a fast one (relay to direct) and the
    // reverse: messages sent before, during and after the move arrive
    // once each, in order, both ways, and neither barrier is delivered.
    void messagesKeepTheirOrderAcrossAMove_data()
    {
        QTest::addColumn<int>("oldDelayMs");
        QTest::addColumn<int>("newDelayMs");
        QTest::newRow("slow to fast") << 80 << 5;
        QTest::newRow("fast to slow") << 5 << 80;
        QTest::newRow("equal") << 20 << 20;
    }
    void messagesKeepTheirOrderAcrossAMove()
    {
        QFETCH(int, oldDelayMs);
        QFETCH(int, newDelayMs);
        SwitchPair pair(oldDelayMs, newDelayMs);
        QSignalSpy stationSwitched(pair.station, &SwitchableTransport::switched);
        QSignalSpy clientSwitched(pair.client, &SwitchableTransport::switched);
        QSignalSpy stationClosed(pair.station, &SessionTransport::closed);
        QSignalSpy clientClosed(pair.client, &SessionTransport::closed);

        int n = 0;
        const auto burst = [&pair, &n](int count) {
            for (int i = 0; i < count; ++i) {
                ++n;
                pair.station->sendText(numbered("station", n));
                pair.client->sendText(numbered("client", n));
            }
        };
        burst(10);
        // The client joins the new connection (its path.join went out on
        // it); the Core takes it and sends its barrier.
        QVERIFY(pair.client->beginClientSwitch(pair.newClient));
        burst(10);
        QVERIFY(pair.station->beginStationSwitch(pair.newStation));
        burst(10);
        QTRY_COMPARE(clientSwitched.size(), 1);
        burst(10);
        QTRY_COMPARE(stationSwitched.size(), 1);
        burst(10);
        QTRY_COMPARE(pair.atStation.size(), 50);
        QTRY_COMPARE(pair.atClient.size(), 50);
        QTest::qWait(3 * std::max(oldDelayMs, newDelayMs));
        QCOMPARE(numbersIn(pair.atStation), oneTo(50));
        QCOMPARE(numbersIn(pair.atClient), oneTo(50));
        for (const QByteArray& wire : pair.atStation + pair.atClient) {
            QVERIFY(!SwitchableTransport::isPathSwitch(wire));
        }
        QVERIFY(stationClosed.isEmpty());
        QVERIFY(clientClosed.isEmpty());
        QCOMPARE(pair.station->inner(), static_cast<SessionTransport*>(pair.newStation));
        QCOMPARE(pair.client->inner(), static_cast<SessionTransport*>(pair.newClient));
        QVERIFY(pair.station->isOpen());
        QVERIFY(pair.client->isOpen());
    }

    // The old connection dies before the device's barrier reaches the
    // Core: the Core reads the new one from then on, and the session goes
    // on.
    void anOldConnectionLostMidMoveCountsAsTheBarrier()
    {
        SwitchPair pair(40, 5);
        QSignalSpy stationSwitched(pair.station, &SwitchableTransport::switched);
        QSignalSpy stationClosed(pair.station, &SessionTransport::closed);
        QVERIFY(pair.station->beginStationSwitch(pair.newStation));
        // The client side of the old link goes away before it answers.
        auto* oldClient = static_cast<DelayLink*>(pair.client->inner());
        QVERIFY(oldClient != nullptr);
        pair.client->beginClientSwitch(pair.newClient);
        oldClient->closeLink(QStringLiteral("lost"));
        QTRY_COMPARE(stationSwitched.size(), 1);
        QVERIFY(stationClosed.isEmpty());
        pair.newClient->sendText(numbered("client", 1));
        QTRY_COMPARE(pair.atStation.size(), 1);
    }

    // No barrier from the Core in time: the move is given up, the new
    // connection closes and the session stays on the old one.
    void aClientWithNoBarrierGivesUpAndStays()
    {
        SwitchPair pair(5, 5);
        QSignalSpy failed(pair.client, &SwitchableTransport::switchFailed);
        QSignalSpy closed(pair.client, &SessionTransport::closed);
        QSignalSpy newClosed(pair.newClient, &SessionTransport::closed);
        SessionTransport* old = pair.client->inner();
        QVERIFY(pair.client->beginClientSwitch(pair.newClient));
        pair.newClient = nullptr;  // the switchable closes and deletes it
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1,
                                  SwitchableTransport::kSwitchDeadlineMs + 5000);
        QVERIFY(closed.isEmpty());
        QCOMPARE(pair.client->inner(), old);
        QVERIFY(!pair.client->switching());
        pair.client->sendText(numbered("client", 7));
        QTRY_COMPARE(pair.atStation.size(), 1);
    }

    // LINK-I2: no barrier from the device in time. Whatever the device sent
    // on the old connection may still be in flight, so the Core ends the
    // session (the device reconnects and resyncs) instead of closing the
    // old connection and carrying on as if nothing were lost.
    void aStationWithNoBarrierEndsTheSession()
    {
        SwitchPair pair(5, 5);
        pair.station->setSwitchDeadlineMsForTest(200);
        QSignalSpy stationSwitched(pair.station, &SwitchableTransport::switched);
        QSignalSpy stationClosed(pair.station, &SessionTransport::closed);
        // The device never answers the barrier: it has not joined the new
        // connection on its side.
        QVERIFY(pair.station->beginStationSwitch(pair.newStation));
        pair.newStation = nullptr;  // the switchable owns it now
        QTRY_COMPARE(stationClosed.size(), 1);
        QVERIFY(stationSwitched.isEmpty());
        QVERIFY(!pair.station->isOpen());
        QVERIFY(!pair.station->switching());
    }

    // What a move holds from the new connection is bounded: past it the
    // session ends rather than growing without limit.
    void whatAMoveHoldsIsBounded()
    {
        auto* oldStation = new LoopbackTransport(QStringLiteral("old station"));
        auto* oldClient = new LoopbackTransport(QStringLiteral("old client"));
        oldStation->linkTo(oldClient);
        auto* newStation = new LoopbackTransport(QStringLiteral("new station"));
        auto* newClient = new LoopbackTransport(QStringLiteral("new client"));
        newStation->linkTo(newClient);
        SwitchableTransport client(oldClient, SwitchableTransport::Side::Client, 1000);
        QSignalSpy closed(&client, &SessionTransport::closed);
        QVERIFY(client.beginClientSwitch(newClient));
        const QByteArray big(1000, 'x');
        for (int i = 0; i < 5; ++i) {
            newStation->sendText(big);
        }
        QTRY_COMPARE(closed.size(), 1);
        delete oldStation;
        delete newStation;
    }

    // A path.switch outside a move is dropped, never delivered.
    void aStrayBarrierIsDropped()
    {
        SwitchPair pair(5, 5);
        auto* oldClient = static_cast<DelayLink*>(pair.client->inner());
        oldClient->sendText(SessionMessages::encode(SessionMessages::pathSwitch()));
        oldClient->sendText(numbered("client", 1));
        QTRY_COMPARE(pair.atStation.size(), 1);
        QCOMPARE(numbersIn(pair.atStation), QList<int>{1});
        QVERIFY(!pair.station->switching());
    }

    // ── A session between a Core and a window ─────────────────────────

    // The whole move: the window asks for a ticket, joins the new
    // connection with its hello and path.join, and the session carries on
    // there: the same epoch, no new sign-in or snapshot, the old
    // connection closed, deltas and writes flowing over the new one.
    void aSessionMovesWithoutASignInOrASnapshot()
    {
        Session s;
        QSignalSpy authenticated(s.server.get(), &StationServer::clientAuthenticated);
        QVERIFY(s.signIn());
        QCOMPARE(authenticated.size(), 1);
        QCOMPARE(s.client->sessionTransport()->inner(), static_cast<SessionTransport*>(s.clientA));
        const quint32 epoch = s.client->sessionEpoch();
        QSignalSpy handshakes(s.client.get(), &StationClient::handshakeComplete);
        QSignalSpy snapshots(s.client.get(), &StationClient::stateSnapshotApplied);
        QSignalSpy ended(s.client.get(), &StationClient::sessionEnded);
        QSignalSpy moved(s.client.get(), &StationClient::pathChanged);
        QVERIFY(s.openB());
        // The old connection is closed and let go once the session moves.
        const QPointer<LoopbackTransport> oldLink(s.clientA);

        QVERIFY(s.client->moveSessionForTest(s.clientB, PathRacer::ThisNetwork));
        QTRY_COMPARE(moved.size(), 1);
        QTRY_COMPARE(s.server->sessionsMoved(), 1);
        QCOMPARE(s.client->pathSwitches(), 1);
        QCOMPARE(s.client->pathRank(), int(PathRacer::ThisNetwork));
        QCOMPARE(s.client->transport(), static_cast<SessionTransport*>(s.clientB));
        QTRY_VERIFY(oldLink.isNull() || !oldLink->isOpen());
        QCOMPARE(s.client->sessionEpoch(), epoch);
        QVERIFY(s.client->isHandshakeComplete());
        QCOMPARE(authenticated.size(), 1);
        QVERIFY(handshakes.isEmpty());
        QVERIFY(snapshots.isEmpty());
        QVERIFY(ended.isEmpty());
        QCOMPARE(s.server->peerCount(), 1);

        // The Core's changes reach the window over the new connection...
        SliceModel* coreSlice = s.radio.slices().value(0);
        QVERIFY(coreSlice != nullptr);
        QTRY_VERIFY(!s.remote.slices().isEmpty());
        coreSlice->setFrequency(14'074'000.0);
        QTRY_COMPARE(s.remote.slices().value(0)->frequency(), 14'074'000.0);
        // ...and the window's reach the Core.
        s.remote.slices().value(0)->setFrequency(7'074'000.0);
        QTRY_COMPARE(coreSlice->frequency(), 7'074'000.0);
        QVERIFY(ended.isEmpty());
    }

    // A ticket that is wrong, spent or from another session ends only the
    // new connection (not retryable, protocolError); the session goes on
    // over the old one.
    void aBadTicketEndsOnlyTheNewConnection()
    {
        Session s;
        QVERIFY(s.signIn());
        QVERIFY(s.openB());
        QSignalSpy ended(s.client.get(), &StationClient::sessionEnded);
        QList<QByteArray> onB;
        QObject::connect(s.clientB, &SessionTransport::textReceived,
                         [&onB](const QByteArray& wire) { onB.append(wire); });
        s.clientB->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("test"),
            {kSessionProtocolMajor}, {})));
        s.clientB->sendText(SessionMessages::encode(SessionMessages::pathJoin(
            QStringLiteral("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"))));
        QTRY_VERIFY(!s.clientB->isOpen());
        bool sawEnd = false;
        for (const QByteArray& wire : onB) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::SessionEnd) {
                sawEnd = true;
                QCOMPARE(message.reason,
                         QStringLiteral("The Core did not move the connection here."));
                QCOMPARE(message.endCode,
                         QString::fromLatin1(SessionEndCode::kProtocolError));
                QVERIFY(!message.retryable);
            }
        }
        QVERIFY(sawEnd);
        QCOMPARE(s.server->sessionsMoved(), 0);
        QVERIFY(s.clientA->isOpen());
        QVERIFY(s.client->isHandshakeComplete());
        QVERIFY(ended.isEmpty());
    }

    // A ticket is good once: a second connection presenting it is refused.
    void aTicketIsGoodOnce()
    {
        Session s;
        QVERIFY(s.signIn());
        const QString ticket = requestTicket(s);
        // 32 bytes, base64url without padding.
        QCOMPARE(ticket.size(), 43);
        QVERIFY(s.openB());
        sendJoin(s.clientB, ticket);
        QTRY_COMPARE(s.server->sessionsMoved(), 1);
        auto* stationC = new LoopbackTransport(QStringLiteral("station C"));
        auto* clientC = new LoopbackTransport(QStringLiteral("client C"));
        stationC->linkTo(clientC);
        s.server->acceptTransport(stationC);
        sendJoin(clientC, ticket);
        QTRY_VERIFY(!clientC->isOpen());
        QCOMPARE(s.server->sessionsMoved(), 1);
        delete clientC;
    }

    // Link section 21.2: nothing moves while the Core is on the air. The
    // ticket is refused in plain words, and the radio stays as it was: a
    // move never keys, and never unkeys either.
    void aCoreOnTheAirRefusesTheTicket()
    {
        Session s;
        QVERIFY(s.signIn());
        MoxController* mox = makeKeyable(s);
        mox->setMox(true);
        QTRY_VERIFY(mox->state() != MoxState::Rx);
        QVERIFY(s.openB());
        // The window, which hears the Core is on the air, does not start.
        QTRY_VERIFY(s.remote.isTransmitting());
        QVERIFY(!s.client->moveSessionForTest(s.clientB, PathRacer::ThisNetwork));
        QString refusal;
        QObject::connect(s.clientA, &SessionTransport::textReceived,
                         [&refusal](const QByteArray& wire) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::CommandResult
                && message.commandVerb == "session.pathTicket" && !message.accepted) {
                refusal = message.reason;
            }
        });
        s.client->invokeCommand(QByteArrayLiteral("session.pathTicket"), {});
        QTRY_COMPARE(refusal, QStringLiteral("Not while the radio is transmitting."));
        QVERIFY(mox->state() != MoxState::Rx);
        QCOMPARE(s.server->sessionsMoved(), 0);
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // A ticket taken off the air is no good once the radio keys: the join
    // is refused and the key goes on untouched.
    void aJoinWhileOnTheAirIsRefused()
    {
        Session s;
        QVERIFY(s.signIn());
        MoxController* mox = makeKeyable(s);
        const QString ticket = requestTicket(s);
        QVERIFY(!ticket.isEmpty());
        QVERIFY(s.openB());
        mox->setMox(true);
        QTRY_VERIFY(mox->state() != MoxState::Rx);
        sendJoin(s.clientB, ticket);
        QTRY_VERIFY(!s.clientB->isOpen());
        QCOMPARE(s.server->sessionsMoved(), 0);
        QVERIFY(mox->state() != MoxState::Rx);
        QVERIFY(s.client->isHandshakeComplete());
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
    }

    // Task 29 fix wave (review Important 2): MOX released but its unkey
    // delay still running (MOX off, the state not yet receive): the ticket
    // and a join with a ticket taken earlier are both refused, and the
    // window does not start a move; once back on receive, it moves.
    void theTicketAndTheJoinWaitForTheUnkeyDelay()
    {
        Session s;
        QVERIFY(s.signIn());
        MoxController* mox = makeKeyable(s);
        const QString ticket = requestTicket(s);
        QVERIFY(!ticket.isEmpty());
        QVERIFY(s.openB());
        mox->setMox(true);
        QTRY_VERIFY(mox->state() != MoxState::Rx);
        QTRY_VERIFY(mox->isMox());
        mox->setTimerIntervals(3000, 3000, 3000, 3000, 3000, 3000);
        mox->setMox(false);
        QTRY_VERIFY(!mox->isMox());
        QVERIFY(mox->state() != MoxState::Rx);

        QString refusal;
        QObject::connect(s.clientA, &SessionTransport::textReceived,
                         [&refusal](const QByteArray& wire) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::CommandResult
                && message.commandVerb == "session.pathTicket" && !message.accepted) {
                refusal = message.reason;
            }
        });
        s.client->invokeCommand(QByteArrayLiteral("session.pathTicket"), {});
        QTRY_COMPARE(refusal, QStringLiteral("Not while the radio is transmitting."));
        sendJoin(s.clientB, ticket);
        QTRY_VERIFY(!s.clientB->isOpen());
        QCOMPARE(s.server->sessionsMoved(), 0);
        QVERIFY(mox->state() != MoxState::Rx);
        QVERIFY(s.client->isHandshakeComplete());

        // Back on receive: a new connection and a new ticket move it.
        QTRY_COMPARE_WITH_TIMEOUT(mox->state(), MoxState::Rx, 20000);
        QTRY_VERIFY(!s.remote.isTransmitting());
        QVERIFY(s.openB());
        QSignalSpy moved(s.client.get(), &StationClient::pathChanged);
        QVERIFY(s.client->moveSessionForTest(s.clientB, PathRacer::ThisNetwork));
        QTRY_COMPARE(moved.size(), 1);
        QCOMPARE(s.server->sessionsMoved(), 1);
    }

    // A ticket runs out: one presented after its lifetime is refused.
    void aTicketRunsOut()
    {
        Session s;
        s.server->setPathTicketLifetimeMsForTest(50);
        QVERIFY(s.signIn());
        const QString ticket = requestTicket(s);
        QVERIFY(!ticket.isEmpty());
        QVERIFY(s.openB());
        QTest::qWait(150);
        sendJoin(s.clientB, ticket);
        QTRY_VERIFY(!s.clientB->isOpen());
        QCOMPARE(s.server->sessionsMoved(), 0);
    }

    // Every sentence a move shows a person is in plain words: the Core's
    // refusals, and the attempt record's.
    void everyWordAMoveShowsIsPlain()
    {
        for (const QString& text :
             {QStringLiteral("Not while the radio is transmitting."),
              QStringLiteral("The Core did not move the connection here."),
              QStringLiteral("The Core could not move this connection."),
              QString::fromLatin1(RendezvousDialer::kCoreTooOldReason)}) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
        using O = StationConnectionAttempt::Outcome;
        for (const O outcome : {O::AnotherPathFirst, O::RelayOff, O::CoreTooOld, O::MovedOn}) {
            const QString text = StationConnectionAttempt::outcomeText(outcome);
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
        QVERIFY(OperatorWording::isPlain(
            StationConnectionAttempt::pathText(StationConnectionAttempt::Path::Service)));
    }
};

QTEST_MAIN(TstSessionTransportSwitch)
#include "tst_session_transport_switch.moc"
