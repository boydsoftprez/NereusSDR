// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_relay_session.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08; the rendezvous
// document, section 12; the options survey's section D, rung 3): a whole
// session over the web relay on this computer. A window reaches a Core
// through the local rendezvous service, which mints relay grants; a stand-in
// relay (below) pairs the two legs by their grant's session and forwards
// their frames as section 12.3 says. With every candidate but the web
// relay's refused (IceConfiguration's test seam, since host pairs always
// work on one computer) the session runs over it: ranked as the floor, the
// attempt record says so, and nothing the relay carries holds the control
// channel's JSON in the clear. Without the seam, a direct pair wins and the
// leg is only a low-priority candidate that ICE never chooses.
//
// The Docker traversal harness proves the same against the real relay
// behind TLS with only TCP 443 open (tests/scripts/traversal-harness.sh).
//
// Loopback only.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: every wait ends as soon as what it waits on closes or
//               fails, and says why (EndWatch, waitUntil). A QTRY_* wait
//               runs on for twice its timeout after it expires, so a session
//               the handshake deadline had closed held the test to QTest's
//               300 s function timeout with no reason given.
// =================================================================

#include <QtTest>

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QSignalSpy>
#include <QWebSocket>
#include <QWebSocketServer>

#include <algorithm>
#include <functional>
#include <memory>

#include "RendezvousTestHarness.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/StationIdentity.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/IceConfiguration.h"
#include "core/session/PathRacer.h"
#include "core/session/RelayLeg.h"
#include "core/session/media/LibDataChannelMediaTransport.h"
#include "core/session/StationClient.h"
#include "core/session/StationRendezvous.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using namespace NereusSDR::Test::Rendezvous;

namespace {

// A stand-in for the relay (rendezvous section 12): pairs legs by the
// session named in their token (section 12.2's payload, bytes 2 to 17),
// answers READY and PEER, forwards tagged datagrams to the other leg, and
// keeps every forwarded payload for the test to read.
class StandInRelay {
public:
    StandInRelay()
    {
        server = std::make_unique<QWebSocketServer>(QStringLiteral("relay"),
                                                    QWebSocketServer::NonSecureMode);
        server->listen(QHostAddress::LocalHost, 0);
        QObject::connect(server.get(), &QWebSocketServer::newConnection, server.get(), [this] {
            while (QWebSocket* socket = server->nextPendingConnection()) {
                QObject::connect(socket, &QWebSocket::binaryMessageReceived, socket,
                                 [this, socket](const QByteArray& message) {
                                     onMessage(socket, message);
                                 });
                QObject::connect(socket, &QWebSocket::disconnected, socket, [this, socket] {
                    for (auto it = legs.begin(); it != legs.end(); ++it) {
                        if (it.value() == socket) {
                            legs.erase(it);
                            break;
                        }
                    }
                    socket->deleteLater();
                });
            }
        });
    }

    QString url() const
    {
        return QStringLiteral("ws://127.0.0.1:%1/v1/relay").arg(server->serverPort());
    }

    // Leg keys: session bytes and the leg byte (1 Core, 2 device).
    QHash<QByteArray, QPointer<QWebSocket>> legs;
    QHash<QWebSocket*, QByteArray> keyOf;
    QList<QByteArray> forwarded[3];
    int joins = 0;
    std::unique_ptr<QWebSocketServer> server;

private:
    static QByteArray otherKey(const QByteArray& key)
    {
        QByteArray other = key;
        other[other.size() - 1] = key.back() == 1 ? 2 : 1;
        return other;
    }

    void onMessage(QWebSocket* socket, const QByteArray& message)
    {
        if (message.isEmpty()) {
            return;
        }
        const auto tag = static_cast<quint8>(message.at(0));
        if (tag == RelayLeg::kTagJoin) {
            bool ok = false;
            const QByteArray token =
                StationIdentity::fromBase64Url(QString::fromLatin1(message.mid(1)), &ok);
            if (!ok || token.size() < 18) {
                socket->close();
                return;
            }
            const QByteArray key = token.mid(2, 16) + token.mid(1, 1);
            legs.insert(key, socket);
            keyOf.insert(socket, key);
            ++joins;
            const QPointer<QWebSocket> other = legs.value(otherKey(key));
            QByteArray ready("\x81\x01", 2);
            ready.append(other ? '\x01' : '\x00');
            socket->sendBinaryMessage(ready);
            if (other) {
                other->sendBinaryMessage(QByteArray("\x82\x01", 2));
            }
            return;
        }
        if (tag == RelayLeg::kTagControl || tag == RelayLeg::kTagMedia) {
            const QPointer<QWebSocket> other = legs.value(otherKey(keyOf.value(socket)));
            forwarded[tag].append(message.mid(1));

            if (other) {
                other->sendBinaryMessage(message);
            }
        }
    }
};

// A Core registered with the local service, with the web relay allowed.
struct CoreThroughService {
    Core core;
    std::unique_ptr<StationRendezvous> rendezvous;
    QString failure;

    bool start(LocalService& service)
    {
        rendezvous = std::make_unique<StationRendezvous>(core.server.get(),
                                                         QList<QUrl>{service.url()},
                                                         /*relayAllowed=*/true);
        bool registered = false;
        QObject::connect(rendezvous->client(), &RendezvousClient::registered,
                         rendezvous->client(), [&registered] { registered = true; });
        EndWatch ends;
        ends.watch(rendezvous->client());
        if (!rendezvous->start()) {
            failure = QStringLiteral("the Core's rendezvous did not start");
            return false;
        }
        // The bound this fixture always had for registering.
        return waitUntil([&registered] { return registered; }, 10000, ends,
                         QStringLiteral("the Core to register with the service"), &failure);
    }
};

// Connects `window` to `station` through `service` and waits for the
// handshake, ending early if the session closes or fails. The watch is
// set before the connect call, so a failure inside it is seen too.
bool connectWindow(StationClient& window, LocalService& service, CoreThroughService& station,
                   QString* why, qint64* elapsedMs = nullptr)
{
    EndWatch ends;
    ends.watch(&window);
    window.connectThroughService({service.url()}, station.rendezvous->client()->stationId(),
                                 station.core.server->stationIdentity().fingerprint());
    return waitForHandshake(window, kServiceConnectBudgetMs, why, elapsedMs, &ends);
}

} // namespace

class TstRelaySession final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void cleanup()
    {
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(false);
        RelayLeg::setRelayUrlForTest(QUrl());
    }

    // Only the web relay works: the session runs over it, both ends' legs
    // carry the control connection's datagrams, and none holds its JSON in
    // the clear (DTLS end to end).
    void aSessionRunsOverTheWebRelay()
    {
        StandInRelay relay;
        QVERIFY(relay.server->isListening());
        LocalService service(/*stun=*/false, /*relay=*/false);
        service.setRelayGrants(true);
        RelayLeg::setRelayUrlForTest(QUrl(relay.url()));
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        CoreThroughService station;
        QVERIFY2(station.start(service), qPrintable(station.failure));
        IceConfiguration::setOnlyLoopbackShimCandidatesForTest(true);

        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(station.core.pairComputer(*key));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        QString why;
        QVERIFY2(connectWindow(window, service, station, &why), qPrintable(why));
        QCOMPARE(relay.joins, 2);
        QCOMPARE(window.pathRank(), int(PathRacer::Floor));
        QVERIFY2(window.connectionAttempt().summary().contains(QLatin1String("web relay")),
                 qPrintable(window.connectionAttempt().summary()));
        const auto* channel = qobject_cast<const DataChannelTransport*>(window.transport());
        QVERIFY(channel != nullptr);
        QVERIFY(channel->selectedPath().has_value() && channel->selectedPath()->viaLoopbackShim());
        // The control lane carried the whole sign-in; its JSON never
        // crossed the relay in the clear.
        QVERIFY(relay.forwarded[RelayLeg::kTagControl].size() > 10);
        for (const QByteArray& datagram : relay.forwarded[RelayLeg::kTagControl]) {
            QVERIFY(!datagram.contains("snapshot.complete"));
            QVERIFY(!datagram.contains("capabilities"));
            QVERIFY(!datagram.contains("\"type\""));
        }

        // Both agents on one leg: a media connection made with each end's
        // session settings (what the media controllers use) runs on the
        // media lane of the same two legs, and the relay never sees its
        // SRTP payload's marker in the clear.
        const std::optional<IceConfiguration> coreIce =
            station.core.server->sessionIceConfiguration(station.core.server->mediaSessionEpoch());
        const std::optional<IceConfiguration> windowIce = window.sessionIceConfiguration();
        QVERIFY(coreIce && coreIce->hasCandidateSourceFactory());
        QVERIFY(windowIce && windowIce->hasCandidateSourceFactory());
        LibDataChannelMediaTransport offerer(nullptr, LibDataChannelMediaTransport::CandidatePolicy::AnyIceType);
        LibDataChannelMediaTransport answerer(nullptr, LibDataChannelMediaTransport::CandidatePolicy::AnyIceType);
        QObject::connect(&offerer, &IMediaTransport::localDescription, &answerer,
                         [&answerer](const QString& sdp, const QString& type) {
                             answerer.acceptDescription(sdp, type);
                         });
        QObject::connect(&answerer, &IMediaTransport::localDescription, &offerer,
                         [&offerer](const QString& sdp, const QString& type) {
                             offerer.acceptDescription(sdp, type);
                         });
        QObject::connect(&offerer, &IMediaTransport::localCandidate, &answerer,
                         [&answerer](const QString& candidate, const QString& mid) {
                             answerer.acceptCandidate(candidate, mid);
                         });
        QObject::connect(&answerer, &IMediaTransport::localCandidate, &offerer,
                         [&offerer](const QString& candidate, const QString& mid) {
                             offerer.acceptCandidate(candidate, mid);
                         });
        QSignalSpy received(&answerer, &IMediaTransport::rtpReceived);
        EndWatch mediaEnds;
        mediaEnds.watch(&offerer, QStringLiteral("offering"));
        mediaEnds.watch(&answerer, QStringLiteral("answering"));
        mediaEnds.watch(&window);
        IMediaTransport::StartOptions core;
        core.role = IMediaTransport::Role::Offerer;
        core.localAudioSsrc = 0x11223344u;
        core.ice = coreIce;
        IMediaTransport::StartOptions device;
        device.role = IMediaTransport::Role::Answerer;
        device.localAudioSsrc = 0x55667788u;
        device.ice = windowIce;
        QVERIFY(answerer.start(device));
        QVERIFY(offerer.start(core));
        QVERIFY2(waitUntil([&] { return offerer.isReady() && answerer.isReady(); }, 30000,
                           mediaEnds, QStringLiteral("both media connections to be ready"), &why),
                 qPrintable(why));
        QVERIFY(offerer.selectedPath() && offerer.selectedPath()->viaLoopbackShim());
        QVERIFY(answerer.selectedPath() && answerer.selectedPath()->viaLoopbackShim());
        QByteArray rtp(12 + 60, '\0');
        rtp[0] = static_cast<char>(0x80);
        rtp[1] = static_cast<char>(111);
        qToBigEndian<quint32>(core.localAudioSsrc, rtp.data() + 8);
        rtp.replace(12, 16, QByteArrayLiteral("NEREUS-PLAINTEXT"));
        for (int i = 0; i < 20 && received.isEmpty() && !mediaEnds.ended(); ++i) {
            qToBigEndian<quint16>(static_cast<quint16>(i + 1), rtp.data() + 2);
            offerer.sendRtp(rtp);
            QTest::qWait(50);
        }
        // QTRY_VERIFY's default timeout, without its doubled overrun.
        QVERIFY2(waitUntil([&received] { return !received.isEmpty(); }, 5000, mediaEnds,
                           QStringLiteral("the media packet to arrive"), &why),
                 qPrintable(why));
        QVERIFY(received.first().first().toByteArray().contains("NEREUS-PLAINTEXT"));
        QVERIFY(!relay.forwarded[RelayLeg::kTagMedia].isEmpty());
        for (const QByteArray& datagram : relay.forwarded[RelayLeg::kTagMedia]) {
            QVERIFY(!datagram.contains("NEREUS-PLAINTEXT"));
        }
        QCOMPARE(relay.joins, 2); // one leg per end carried both
        offerer.stop();
        answerer.stop();
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // Every path open: the direct pair wins; the web relay is offered as
    // the lowest-priority candidate and never chosen.
    void aDirectPairStillWins()
    {
        StandInRelay relay;
        LocalService service(/*stun=*/false, /*relay=*/false);
        service.setRelayGrants(true);
        RelayLeg::setRelayUrlForTest(QUrl(relay.url()));
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        CoreThroughService station;
        QVERIFY2(station.start(service), qPrintable(station.failure));

        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(station.core.pairComputer(*key));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        QString why;
        QVERIFY2(connectWindow(window, service, station, &why), qPrintable(why));
        QCOMPARE(window.pathRank(), int(PathRacer::ServiceDirect));
        const auto* channel = qobject_cast<const DataChannelTransport*>(window.transport());
        QVERIFY(channel != nullptr && channel->selectedPath().has_value());
        QVERIFY(!channel->selectedPath()->viaLoopbackShim());
        // Both legs joined at the introduction all the same.
        EndWatch ends;
        ends.watch(&window);
        QVERIFY2(waitUntil([&relay] { return relay.joins == 2; }, 5000, ends,
                           QStringLiteral("both relay legs to join"), &why),
                 qPrintable(QStringLiteral("%1 (joins: %2)").arg(why).arg(relay.joins)));
        window.disconnectFromStation(QStringLiteral("test done"));
    }

    // The waits above end when the session does: a session the handshake
    // deadline closes (the close seen under load) ends the handshake wait
    // at once with that reason, not at the connect budget.
    void aClosedSessionEndsTheWaitWithItsReason()
    {
        StandInRelay relay;
        LocalService service(/*stun=*/false, /*relay=*/false);
        service.setRelayGrants(true);
        RelayLeg::setRelayUrlForTest(QUrl(relay.url()));
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        CoreThroughService station;
        QVERIFY2(station.start(service), qPrintable(station.failure));

        QTemporaryDir keyDir;
        auto key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        QVERIFY(station.core.pairComputer(*key));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient window(&remote, &proxy);
        window.setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        // Forces the close: no sign-in finishes in a millisecond.
        window.setHandshakeDeadlineMs(1);
        QString why;
        qint64 elapsedMs = -1;
        QVERIFY(!connectWindow(window, service, station, &why, &elapsedMs));
        QVERIFY2(why.contains(StationClient::handshakeDeadlineReason()), qPrintable(why));
        QVERIFY2(elapsedMs < kServiceConnectBudgetMs,
                 qPrintable(QStringLiteral("ended after %1 ms: %2").arg(elapsedMs).arg(why)));
        window.disconnectFromStation(QStringLiteral("test done"));
    }
};

QTEST_MAIN(TstRelaySession)
#include "tst_relay_session.moc"
