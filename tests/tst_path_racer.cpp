// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_path_racer.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 (R-IOS-16, R-IOS-08; the pairing design, section
// 5.4; the link document, section 21.1): a paired Core is raced on every
// path at once, the first path whose hello proves the Core wins, and a
// better path found later takes the session over.
//
//   - PathRacer with rungs of its own: the first ready rung wins; a better
//     one ready later is kept for the session; another computer answering
//     ends only its rung; with every rung ended the race says why, in the
//     rung's own words; an upgrade takes only a better rank; IPv6
//     addresses start at once and IPv4 ones kIpv4DelayMs later.
//   - A window and a Core on this computer, with the remote access
//     service (the real Python service and the fake STUN/TURN server):
//       * with every path open the direct path wins, and the record says
//         the service's path was not needed;
//       * with the Core's address closed the service's path wins, and the
//         record says what the address met;
//       * the session then moves to the Core's address once it opens,
//         with no new sign-in or snapshot;
//       * a Core that has the relay turned off is raced without it, and
//         the record says the Core turned it off;
//       * a Core that never answers its introduction ends the service's
//         path in plain words ("This Core can't be reached through the
//         internet service. Updating the Core may help."), and one that
//         recorded controlChannelVersion 0 is not tried there at all.
//
// Nothing here reaches beyond this computer: the service and the Core are
// on loopback, and the client's service list is the local service alone.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-02: cover adopted-session retry ownership by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via OpenAI Codex.
// =================================================================

#include <QtTest>

#include <QPointer>
#include <QScopeGuard>
#include <QSet>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <memory>

#include "core/security/ClientDeviceIdentity.h"
#include "core/security/StationIdentity.h"
#include "core/session/PathRacer.h"
#include "core/session/DataChannelTransport.h"
#include "core/HpsdrModel.h"
#include "core/ConnectionState.h"
#include "models/TransmitModel.h"
#include "models/SliceModel.h"
#include "core/session/RemoteTransmitClient.h"
#include "core/MoxController.h"
#include "core/session/RendezvousDialer.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationRendezvous.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "models/RadioModel.h"
#include "RendezvousTestHarness.h"
#include "OperatorWording.h"
#include "fakes/LoginProxy.h"

using namespace NereusSDR;
using namespace NereusSDR::Test::Rendezvous;
using NereusSDR::Test::LoopbackTransport;

namespace {

// A rung of the test's own: after `delayMs` it opens an in-process link
// whose far end sends a hello naming `peer` (the vetter below takes only
// "right core"), or it ends with `outcome` and `reason`.
class FakeRung final : public PathRung {
public:
    FakeRung(int rank, PathKind kind, const QString& address, int delayMs, const QString& peer,
             PathOutcome outcome = PathOutcome::Ready, const QString& reason = QString())
        : m_rank(rank), m_kind(kind), m_address(address), m_delayMs(delayMs), m_peer(peer),
          m_outcome(outcome), m_reason(reason)
    {
    }
    void start() override
    {
        started = true;
        QTimer::singleShot(m_delayMs, this, [this] {
            if (stopped) {
                return;
            }
            if (m_outcome != PathOutcome::Ready) {
                emit ended(m_outcome, m_reason);
                return;
            }
            auto* near = new LoopbackTransport(m_address);
            farEnd = new LoopbackTransport(QStringLiteral("core at ") + m_address, this);
            near->linkTo(farEnd);
            emit opened(near);
            farEnd->sendText(SessionMessages::encode(SessionMessages::hello(
                kSessionProtocolMajor, kSessionProtocolMinor, 0, m_peer,
                {kSessionProtocolMajor}, {})));
        });
    }
    void stop() override { stopped = true; }
    int rank() const override { return m_rank; }
    PathKind kind() const override { return m_kind; }
    QString address() const override { return m_address; }

    bool started = false;
    bool stopped = false;
    QPointer<LoopbackTransport> farEnd;

private:
    int m_rank;
    PathKind m_kind;
    QString m_address;
    int m_delayMs;
    QString m_peer;
    PathOutcome m_outcome;
    QString m_reason;
};

PathRacer::Vetter vetByPeerName()
{
    return [](const SessionMessage& hello, SessionTransport*) {
        return hello.peerName == QStringLiteral("right core");
    };
}

PathRacer::Outcome outcomeAt(const PathRacer& racer, const QString& address)
{
    for (const PathRacer::Line& line : racer.lines()) {
        if (line.address == address) {
            return line.outcome;
        }
    }
    return PathRacer::Outcome::Trying;
}

QByteArray identityOf(const Core& core)
{
    return StationIdentity::fingerprintOf(core.server->stationIdentity().publicKeySpki());
}

// A paired window of its own: a device key paired with `core`, and the
// service on this computer as its route to it.
struct Window {
    QTemporaryDir keyDir;
    std::shared_ptr<const ClientDeviceIdentity> key;
    RadioModel remote{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;

    explicit Window(Core& core)
    {
        key = std::make_shared<const ClientDeviceIdentity>(
            ClientDeviceIdentity::loadOrCreate(keyDir.path()));
        core.pairComputer(*key);
        client = std::make_unique<StationClient>(&remote, &proxy);
        client->setDeviceIdentity(key, QStringLiteral("Shack MacBook"));
        client->setHeartbeatIntervalMs(0);
    }

    void route(const LocalService& service, const QString& stationId, bool relayAllowed = true,
               int controlChannelVersion = 1)
    {
        StationClient::ServiceRoute route;
        route.servers = {service.url()};
        route.rendezvousId = stationId;
        route.relayAllowed = relayAllowed;
        route.controlChannelVersion = controlChannelVersion;
        client->setServiceRoute(route);
    }

    StationConnectionAttempt::Outcome outcomeFor(StationConnectionAttempt::Path path) const
    {
        for (const StationConnectionAttempt::Try& attempt : client->connectionAttempt().tries) {
            if (attempt.path == path) {
                return attempt.outcome;
            }
        }
        return StationConnectionAttempt::Outcome::Trying;
    }

    bool hasOutcome(StationConnectionAttempt::Outcome outcome) const
    {
        for (const StationConnectionAttempt::Try& attempt : client->connectionAttempt().tries) {
            if (attempt.outcome == outcome) {
                return true;
            }
        }
        return false;
    }
};

// A port nothing listens on, on this computer.
quint16 closedPort()
{
    return freeTcpPort();
}

} // namespace

class TstPathRacer final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        qRegisterMetaType<NereusSDR::PathOutcome>();
    }

    // ── The race alone ────────────────────────────────────────────────

    // The first rung to prove the Core wins, whatever its rank; a better
    // rung ready later is kept for the session to move to; one that never
    // answers ends as no answer.
    void theFirstReadyRungWinsAndABetterOneIsKept()
    {
        PathRacer racer;
        racer.setVetter(vetByPeerName());
        auto* relayed = new FakeRung(PathRacer::ServiceRelayed, PathKind::Relay,
                                     QStringLiteral("rv"), 10, QStringLiteral("right core"));
        auto* direct = new FakeRung(PathRacer::Direct, PathKind::Direct,
                                    QStringLiteral("203.0.113.7:47910"), 80,
                                    QStringLiteral("right core"));
        auto* lan = new FakeRung(PathRacer::ThisNetwork, PathKind::ThisNetwork,
                                 QStringLiteral("192.168.1.20:47910"), 20, QString(),
                                 PathOutcome::NoAnswer);
        racer.addRung(relayed);
        racer.addRung(direct);
        racer.addRung(lan);
        QSignalSpy won(&racer, &PathRacer::won);
        QSignalSpy better(&racer, &PathRacer::better);
        racer.start();
        QTRY_COMPARE(won.size(), 1);
        const auto winner = won.at(0).at(0).value<PathRacer::Ready>();
        QCOMPARE(winner.rank, int(PathRacer::ServiceRelayed));
        QVERIFY(winner.transport != nullptr);
        SessionMessage hello;
        QVERIFY(SessionMessages::decode(winner.hello, &hello));
        QCOMPARE(hello.kind, SessionMessageKind::Hello);
        QTRY_COMPARE(better.size(), 1);
        QCOMPARE(outcomeAt(racer, QStringLiteral("192.168.1.20:47910")),
                 PathRacer::Outcome::NoAnswer);
        racer.finish();
        const std::optional<PathRacer::Ready> standby = racer.takeStandby();
        QVERIFY(standby.has_value());
        QCOMPARE(standby->rank, int(PathRacer::Direct));
        QCOMPARE(standby->address, QStringLiteral("203.0.113.7:47910"));
        QVERIFY(standby->transport != nullptr);
        delete winner.transport.data();
        delete standby->transport.data();
    }

    // Another computer answering at an address ends that rung alone.
    void anotherComputerEndsOnlyItsRung()
    {
        PathRacer racer;
        racer.setVetter(vetByPeerName());
        racer.addRung(new FakeRung(PathRacer::ThisNetwork, PathKind::ThisNetwork,
                                   QStringLiteral("192.168.1.9:47910"), 5,
                                   QStringLiteral("some other core")));
        racer.addRung(new FakeRung(PathRacer::Direct, PathKind::Direct,
                                   QStringLiteral("203.0.113.7:47910"), 40,
                                   QStringLiteral("right core")));
        QSignalSpy won(&racer, &PathRacer::won);
        racer.start();
        QTRY_COMPARE(won.size(), 1);
        QCOMPARE(won.at(0).at(0).value<PathRacer::Ready>().address,
                 QStringLiteral("203.0.113.7:47910"));
        QCOMPARE(outcomeAt(racer, QStringLiteral("192.168.1.9:47910")),
                 PathRacer::Outcome::NotThisCore);
        delete won.at(0).at(0).value<PathRacer::Ready>().transport.data();
    }

    // Every rung ended: the race says why, in the most telling rung's own
    // words (a Core too old for the service), and the record keeps each.
    void everyRungEndedSaysWhy()
    {
        PathRacer racer;
        racer.setVetter(vetByPeerName());
        racer.addRung(new FakeRung(PathRacer::Direct, PathKind::Direct,
                                   QStringLiteral("203.0.113.7:47910"), 5, QString(),
                                   PathOutcome::NoAnswer));
        racer.addRung(new FakeRung(PathRacer::ServiceDirect, PathKind::Service,
                                   QStringLiteral("rv.example"), 10, QString(),
                                   PathOutcome::CoreTooOld,
                                   QString::fromLatin1(RendezvousDialer::kCoreTooOldReason)));
        racer.addNote(PathKind::Relay, QStringLiteral("rv.example"), PathOutcome::RelayOff,
                      QStringLiteral("The Core has the relay turned off."));
        QSignalSpy failed(&racer, &PathRacer::failed);
        racer.start();
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(failed.at(0).at(0).toString(),
                 QStringLiteral("This Core can't be reached through the internet service. "
                                "Updating the Core may help."));
        QCOMPARE(racer.lines().size(), 3);
        QCOMPARE(outcomeAt(racer, QStringLiteral("203.0.113.7:47910")),
                 PathRacer::Outcome::NoAnswer);
    }

    // An upgrade takes only a rank better than the session's: a rung no
    // better closes ("another path connected first").
    void anUpgradeTakesOnlyABetterRank()
    {
        PathRacer racer;
        racer.setVetter(vetByPeerName());
        racer.setBetterThan(PathRacer::ServiceDirect);
        racer.addRung(new FakeRung(PathRacer::ServiceRelayed, PathKind::Relay,
                                   QStringLiteral("rv"), 5, QStringLiteral("right core")));
        racer.addRung(new FakeRung(PathRacer::Direct, PathKind::Direct,
                                   QStringLiteral("203.0.113.7:47910"), 40,
                                   QStringLiteral("right core")));
        QSignalSpy won(&racer, &PathRacer::won);
        racer.start();
        QTRY_COMPARE(won.size(), 1);
        QCOMPARE(won.at(0).at(0).value<PathRacer::Ready>().rank, int(PathRacer::Direct));
        QCOMPARE(outcomeAt(racer, QStringLiteral("rv")), PathRacer::Outcome::Stopped);
        delete won.at(0).at(0).value<PathRacer::Ready>().transport.data();
    }

    // IPv6 first: an IPv6 address starts at once and an IPv4 one
    // kIpv4DelayMs later; with no IPv6 address, IPv4 starts at once; a
    // name's addresses are ordered the same way once it resolves.
    // Task 29 fix wave (review Minor 8): an upgrade never dials an address
    // that cannot beat the path in use, a target is dialled once however
    // it is named, and direct connections open two at a time (the Core's
    // handshakes per address).
    void anUpgradeDialsOnlyWhatCanWinAndEachTargetOnce()
    {
        {
            PathRacer racer;
            racer.setBetterThan(PathRacer::Direct);
            racer.addDirectUrls({QUrl(QStringLiteral("wss://192.0.2.1:9")),
                                 QUrl(QStringLiteral("wss://127.0.0.1:9"))},
                                StationClient::kMaxIncomingMessageBytes);
            const auto planned = racer.plannedStartsForTest();
            QCOMPARE(planned.size(), 1);
            QCOMPARE(planned.at(0).first, QStringLiteral("127.0.0.1:9"));
        }
        {
            QTcpServer silent;
            QVERIFY(silent.listen(QHostAddress::LocalHost));
            const quint16 port = silent.serverPort();
            const QUrl numeric(QStringLiteral("wss://127.0.0.1:%1").arg(port));
            const QUrl numericWithSlash(QStringLiteral("wss://127.0.0.1:%1/").arg(port));
            const QUrl hostname(QStringLiteral("wss://localhost:%1").arg(port));
            PathRacer racer;
            QSignalSpy failed(&racer, &PathRacer::failed);
            racer.addDirectUrls({numeric, numericWithSlash, hostname},
                                StationClient::kMaxIncomingMessageBytes);
            racer.start();
            QTRY_VERIFY(silent.hasPendingConnections());
            QTcpSocket* peer = silent.nextPendingConnection();
            QVERIFY(peer != nullptr);
            silent.close();
            peer->disconnectFromHost();
            // failed() waits for every pending name lookup and rung to end.
            QTRY_COMPARE(failed.size(), 1);
            const QString unresolvedName = QStringLiteral("localhost:%1").arg(port);
            for (const auto& line : racer.lines()) {
                QVERIFY(line.address != unresolvedName);
            }
            QSet<QString> seen;
            for (const auto& [address, delay] : racer.plannedStartsForTest()) {
                Q_UNUSED(delay);
                QVERIFY(!seen.contains(address));
                seen.insert(address);
            }
            QVERIFY(seen.contains(QStringLiteral("127.0.0.1:%1").arg(port)));
            racer.cancel();
        }
        {
            // Three addresses that accept and never answer: two open at
            // once, the third waits its turn.
            std::vector<std::unique_ptr<QTcpServer>> silent;
            QList<QUrl> urls;
            for (int i = 0; i < 3; ++i) {
                auto server = std::make_unique<QTcpServer>();
                QVERIFY(server->listen(QHostAddress::LocalHost));
                urls.append(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server->serverPort())));
                silent.push_back(std::move(server));
            }
            PathRacer racer;
            racer.addDirectUrls(urls, StationClient::kMaxIncomingMessageBytes);
            racer.start();
            QTRY_COMPARE(racer.directOpeningForTest(), PathRacer::kMaxDirectOpening);
            QCOMPARE(racer.directWaitingForTest(), 1);
            racer.cancel();
        }
    }

    void ipv6StartsAtOnceAndIpv4Later()
    {
        {
            PathRacer racer;
            racer.addDirectUrls({QUrl(QStringLiteral("wss://192.0.2.1:9")),
                                 QUrl(QStringLiteral("wss://[2001:db8::1]:9"))},
                                StationClient::kMaxIncomingMessageBytes);
            const auto planned = racer.plannedStartsForTest();
            QCOMPARE(planned.size(), 2);
            QCOMPARE(planned.at(0), qMakePair(QStringLiteral("192.0.2.1:9"),
                                              PathRacer::kIpv4DelayMs));
            QCOMPARE(planned.at(1), qMakePair(QStringLiteral("[2001:db8::1]:9"), 0));
        }
        {
            PathRacer racer;
            racer.addDirectUrls({QUrl(QStringLiteral("wss://192.0.2.1:9"))},
                                StationClient::kMaxIncomingMessageBytes);
            QCOMPARE(racer.plannedStartsForTest().value(0).second, 0);
        }
        {
            PathRacer racer;
            QSignalSpy lines(&racer, &PathRacer::linesChanged);
            racer.addDirectUrls({QUrl(QStringLiteral("wss://localhost:9"))},
                                StationClient::kMaxIncomingMessageBytes);
            racer.start();
            QTRY_VERIFY(!racer.plannedStartsForTest().isEmpty());
            bool sawV4 = false;
            bool sawV6 = false;
            for (const auto& [address, delay] : racer.plannedStartsForTest()) {
                if (address.startsWith(QLatin1Char('['))) {
                    sawV6 = true;
                    QCOMPARE(delay, 0);
                } else {
                    sawV4 = true;
                    QCOMPARE(delay, sawV6 || racer.plannedStartsForTest().size() > 1
                                        ? PathRacer::kIpv4DelayMs
                                        : 0);
                }
            }
            QVERIFY(sawV4 || sawV6);
            racer.cancel();
        }
    }

    // ── A window and a Core on this computer ──────────────────────────

    // Every path open: the Core's address wins, and the service's path is
    // recorded as not needed.
    void withEveryPathOpenTheDirectPathWins()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        QVERIFY(core.server->listen(QHostAddress::LocalHost, 0));
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        window.route(service, rendezvous.client()->stationId());
        window.client->connectToStation(core.url(), QString(), QString(), false,
                                        identityOf(core));
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(*window.client, 20000, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QCOMPARE(window.client->pathRank(), int(PathRacer::ThisNetwork));
        QCOMPARE(window.outcomeFor(StationConnectionAttempt::Path::ThisNetwork),
                 StationConnectionAttempt::Outcome::Connected);
        // The service's path stopped once the Core's address won (or was
        // still on its way when it did).
        const StationConnectionAttempt::Outcome service_ =
            window.outcomeFor(StationConnectionAttempt::Path::Service);
        QVERIFY2(service_ == StationConnectionAttempt::Outcome::AnotherPathFirst,
                 qPrintable(window.client->connectionAttempt().summary()));
        QCOMPARE(window.client->stationRendezvousId(), rendezvous.client()->stationId());
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    // The Core's address closed: the service's path wins, and the record
    // says what the address met. Then the Core's address opens, and the
    // session moves to it: no new sign-in, no snapshot.
    void withOnlyTheServiceTheServiceWinsThenMovesToTheAddress()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        const quint16 port = closedPort();
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        window.route(service, rendezvous.client()->stationId());
        window.client->setUpgradeScheduleForTest({300});
        QSignalSpy handshakes(window.client.get(), &StationClient::handshakeComplete);
        QSignalSpy moved(window.client.get(), &StationClient::pathChanged);
        QSignalSpy authenticated(core.server.get(), &StationServer::clientAuthenticated);
        window.client->connectToStation(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(port)), QString(), QString(), false,
            identityOf(core));
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(*window.client, kServiceConnectBudgetMs, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QVERIFY(window.client->pathRank() == int(PathRacer::ServiceDirect)
                || window.client->pathRank() == int(PathRacer::ServiceRelayed));
        QCOMPARE(window.outcomeFor(StationConnectionAttempt::Path::ThisNetwork),
                 StationConnectionAttempt::Outcome::NoAnswer);
        QVERIFY2(window.hasOutcome(StationConnectionAttempt::Outcome::Connected),
                 qPrintable(window.client->connectionAttempt().summary()));
        QCOMPARE(handshakes.size(), 1);
        QCOMPARE(authenticated.size(), 1);

        // The Core's address opens; the next look finds it.
        QVERIFY(core.server->listen(QHostAddress::LocalHost, port));
        QTRY_COMPARE_WITH_TIMEOUT(moved.size(), 1, 30000);
        QCOMPARE(window.client->pathRank(), int(PathRacer::ThisNetwork));
        QCOMPARE(window.client->pathSwitches(), 1);
        QCOMPARE(core.server->sessionsMoved(), 1);
        QVERIFY(window.client->isHandshakeComplete());
        QCOMPARE(handshakes.size(), 1);
        QCOMPARE(authenticated.size(), 1);
        QCOMPARE(window.client->connectedUrl(),
                 QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(port)));
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    void adoptedSessionDoesNotReuseEarlierPairedRaceRoutes()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        StationRendezvous rendezvous(core.server.get(), {service.url()}, true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        window.route(service, rendezvous.client()->stationId());
        window.client->connectToStation(QUrl(), QString(), QString(), false, identityOf(core));
        QString handshakeWhy;
        QVERIFY2(waitForHandshake(*window.client, kServiceConnectBudgetMs, &handshakeWhy),
                 qPrintable(handshakeWhy));
        QVERIFY(window.client->connectedUrl().isEmpty());
        QVERIFY(!window.client->serviceRoute().servers.isEmpty());
        window.client->disconnectFromStation(QStringLiteral("operator closed prior Core"), false);

        // This adopted link owns no dial target. A different identity must
        // never inherit the previous paired Core's service route on loss.
        QSignalSpy scheduled(window.client.get(), &StationClient::reconnectScheduled);
        auto* adopted = new LoopbackTransport(QStringLiteral("adopted link"));
        window.client->startSession(adopted, QString(), QString(), QByteArray(32, 'x'));
        window.client->disconnectFromStation(QStringLiteral("station link lost"), true);
        QCOMPARE(scheduled.size(), 0);
        QVERIFY(!window.client->isReconnectPending());
        QVERIFY(window.client->serviceRoute().servers.isEmpty());
    }

    // Task 29 fix wave (review Important 2): the window's upgrade
    // schedule waits while this window has VOX armed (its keepalives run)
    // and while the Core is on the air, and moves once both are over.
    void anUpgradeWaitsWhileKeyedOrVoxArmed()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        const quint16 port = closedPort();
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        window.route(service, rendezvous.client()->stationId());
        window.client->setUpgradeScheduleForTest({300});
        QSignalSpy moved(window.client.get(), &StationClient::pathChanged);
        window.client->connectToStation(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(port)), QString(), QString(), false,
            identityOf(core));
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(*window.client, kServiceConnectBudgetMs, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QVERIFY(window.client->pathRank() > int(PathRacer::ThisNetwork));

        // VOX armed at this window: its keepalives run, and no look starts.
        RemoteTransmitClient* transmit = window.client->remoteTransmit();
        QVERIFY(transmit != nullptr);
        transmit->setAvailable(true);
        transmit->setVoxArmed(true);
        QVERIFY(transmit->keepaliveRunning());
        QVERIFY(core.server->listen(QHostAddress::LocalHost, port));
        QTest::qWait(1500);
        QCOMPARE(moved.size(), 0);

        // VOX off, the Core keyed on its own: still no look.
        core.model->setBoardForTest(HPSDRHW::HermesLite);
        core.model->setConnectionStateForTest(ConnectionState::Connected);
        MoxController* mox = core.model->moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        core.model->transmitModel().setMicSourceLocked(false);
        core.model->transmitModel().setMicSource(MicSource::Radio);
        if (core.model->slices().isEmpty()) {
            core.model->addSlice();
        }
        if (SliceModel* slice = core.model->slices().value(0)) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14200000.0);
        }
        core.server->setRemoteTransmitAllowed(true);
        mox->setMox(true);
        QTRY_VERIFY(mox->state() != MoxState::Rx);
        QTRY_VERIFY_WITH_TIMEOUT(window.remote.isTransmitting(), 5000);
        transmit->setVoxArmed(false);
        QVERIFY(!transmit->keepaliveRunning());
        QTest::qWait(1500);
        QCOMPARE(moved.size(), 0);
        QCOMPARE(core.server->sessionsMoved(), 0);

        // Back on receive: the schedule resumes and the session moves.
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        QTRY_COMPARE_WITH_TIMEOUT(moved.size(), 1, 30000);
        QCOMPARE(window.client->pathRank(), int(PathRacer::ThisNetwork));
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    // A Core with the relay turned off: the service's path runs without
    // it, and the record says the Core turned it off.
    // Task 29 step 2a re-review (Minor 7): an agent that nominated a relayed
    // pair first and settled on a direct one by snapshot.complete: the
    // session is ranked by the settled pair, and the record says so.
    void theRankIsTheSettledPairsAtSnapshotComplete()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        window.route(service, rendezvous.client()->stationId());
        bool settled = false;
        MediaIcePath relayed;
        relayed.localType = QStringLiteral("relay");
        relayed.remoteType = QStringLiteral("srflx");
        relayed.localAddress = QStringLiteral("198.51.100.2");
        relayed.remoteAddress = QStringLiteral("198.51.100.10");
        MediaIcePath direct = relayed;
        direct.localType = QStringLiteral("srflx");
        direct.localAddress = QStringLiteral("198.51.100.6");
        DataChannelTransport::setSelectedPathOverrideForTest(
            [&settled, relayed, direct](const DataChannelTransport*) {
                return std::optional<MediaIcePath>(settled ? direct : relayed);
            });
        // The Core signs the window in before it sends snapshot.complete.
        QObject::connect(core.server.get(), &StationServer::clientAuthenticated,
                         core.server.get(), [&settled] { settled = true; });
        window.client->connectToStation(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(closedPort())), QString(), QString(),
            false, identityOf(core));
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(*window.client, kServiceConnectBudgetMs, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        const QString summary = window.client->connectionAttempt().summary();
        DataChannelTransport::setSelectedPathOverrideForTest({});
        QCOMPARE(window.client->pathRank(), int(PathRacer::ServiceDirect));
        QCOMPARE(window.outcomeFor(StationConnectionAttempt::Path::Service),
                 StationConnectionAttempt::Outcome::Connected);
        QVERIFY2(window.outcomeFor(StationConnectionAttempt::Path::Relay)
                     != StationConnectionAttempt::Outcome::Connected,
                 qPrintable(summary));
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    // Task 29 step 2a re-review (Minor 14): the Core keys in the same turn
    // the window starts a move, before the window hears of it; the Core
    // refuses the ticket ("Not while the radio is transmitting.") and the
    // look schedule stays on its step.
    void aTicketRefusedForNowKeepsTheLookStep()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        window.route(service, rendezvous.client()->stationId());
        window.client->setUpgradeScheduleForTest({60000, 60000});
        window.client->connectToStation(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(closedPort())), QString(), QString(),
            false, identityOf(core));
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(*window.client, kServiceConnectBudgetMs, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QVERIFY(window.client->pathRank() > int(PathRacer::ThisNetwork));
        QCOMPARE(window.client->upgradeAttemptForTest(), 0);

        core.model->setBoardForTest(HPSDRHW::HermesLite);
        core.model->setConnectionStateForTest(ConnectionState::Connected);
        MoxController* mox = core.model->moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        core.model->transmitModel().setMicSourceLocked(false);
        core.model->transmitModel().setMicSource(MicSource::Radio);
        if (core.model->slices().isEmpty()) {
            core.model->addSlice();
        }
        if (SliceModel* slice = core.model->slices().value(0)) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14200000.0);
        }
        core.server->setRemoteTransmitAllowed(true);
        // A better connection, its hello read.
        auto* stationB = new NereusSDR::Test::LoopbackTransport(QStringLiteral("station B"));
        auto* clientB = new NereusSDR::Test::LoopbackTransport(QStringLiteral("client B"));
        stationB->linkTo(clientB);
        QList<QByteArray> hello;
        const QMetaObject::Connection watch = QObject::connect(
            clientB, &SessionTransport::textReceived, clientB,
            [&hello](const QByteArray& wire) { hello.append(wire); });
        core.server->acceptTransport(stationB);
        QTRY_VERIFY(!hello.isEmpty());
        QObject::disconnect(watch);

        // One turn: the Core keys, and the window, not yet told, asks.
        QSignalSpy moved(window.client.get(), &StationClient::pathChanged);
        mox->setMox(true);
        QVERIFY(!window.remote.isTransmitting());
        QVERIFY(window.client->moveSessionForTest(clientB, PathRacer::ThisNetwork));
        QVERIFY(window.client->upgradeUnderWayForTest());
        QTRY_VERIFY(!window.client->upgradeUnderWayForTest());
        QCOMPARE(moved.size(), 0);
        QCOMPARE(core.server->sessionsMoved(), 0);
        QCOMPARE(window.client->upgradeAttemptForTest(), 0);
        mox->setMox(false);
        QTRY_COMPARE(mox->state(), MoxState::Rx);
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    // Task 29 step 2a re-review: a service with a relay secret and no TURN
    // secret answers without relay credentials but sends a relay grant
    // (rendezvous sections 10 and 12.1): the Core allowed the relay, and
    // the record does not say it turned it off.
    void aServiceWithoutTurnDoesNotBlameTheCore()
    {
        LocalService service(/*stun=*/true, /*relay=*/false);
        service.setRelayGrants(true);
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QSignalSpy granted(rendezvous.client(), &RendezvousClient::relayGrantReceived);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        window.route(service, rendezvous.client()->stationId());
        window.client->connectToStation(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(closedPort())), QString(), QString(),
            false, identityOf(core));
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(*window.client, kServiceConnectBudgetMs, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QTRY_COMPARE(granted.size(), 1);
        QVERIFY2(!window.hasOutcome(StationConnectionAttempt::Outcome::RelayOff),
                 qPrintable(window.client->connectionAttempt().summary()));
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    void aCoreWithTheRelayOffIsRacedWithoutIt_data()
    {
        QTest::addColumn<bool>("recorded");
        QTest::newRow("recorded at the last sign-in") << true;
        // Task 29 fix wave (review Minor 6): nothing recorded yet; the
        // Core's answer without relay credentials says it.
        QTest::newRow("first connect") << false;
    }
    void aCoreWithTheRelayOffIsRacedWithoutIt()
    {
        QFETCH(bool, recorded);
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/false);
        core.server->setRelayAllowed(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        window.route(service, rendezvous.client()->stationId(), /*relayAllowed=*/!recorded);
        window.client->connectToStation(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(closedPort())), QString(), QString(),
            false, identityOf(core));
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(*window.client, kServiceConnectBudgetMs, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QCOMPARE(window.client->pathRank(), int(PathRacer::ServiceDirect));
        QVERIFY2(window.hasOutcome(StationConnectionAttempt::Outcome::RelayOff),
                 qPrintable(window.client->connectionAttempt().summary()));
        QVERIFY(window.client->connectionAttempt().summary().contains(
            QStringLiteral("the Core has the relay turned off")));
        // And the Core says so in its capabilities.
        QVERIFY(window.client->capabilities().relayAllowedEntry);
        QVERIFY(!window.client->capabilities().relayAllowed);
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    // A Core that never answers its introduction ends the service's path
    // in plain words, and the race fails with them.
    void anOlderCoreIsToldInPlainWords_data()
    {
        QTest::addColumn<int>("recordedVersion");
        QTest::addColumn<QString>("words");
        QTest::addColumn<bool>("tooOld");
        // No session recorded yet: probably a Core too old to answer.
        QTest::newRow("nothing recorded")
            << -1
            << QStringLiteral("This Core can't be reached through the internet service. "
                              "Updating the Core may help.")
            << true;
        // Task 29 fix wave (review Minor 5): its last session said it
        // answers, so it is slow or offline, and is not told to update.
        QTest::newRow("answered before")
            << 1
            << QStringLiteral("The Core did not answer through the internet service. Check "
                              "that it is on and online.")
            << false;
    }
    void anOlderCoreIsToldInPlainWords()
    {
        QFETCH(int, recordedVersion);
        QFETCH(QString, words);
        QFETCH(bool, tooOld);
        QVERIFY(OperatorWording::isPlain(words));
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        rendezvous.setAnswersIntroductionsForTest(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        window.route(service, rendezvous.client()->stationId(), true, recordedVersion);
        window.client->setServiceRungDeadlinesForTest(0, 1500);
        QSignalSpy ended(window.client.get(), &StationClient::sessionEnded);
        window.client->connectToStation(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(closedPort())), QString(), QString(),
            false, identityOf(core));
        QTRY_VERIFY_WITH_TIMEOUT(!ended.isEmpty(), 20000);
        QCOMPARE(ended.first().first().toString(), words);
        QCOMPARE(window.hasOutcome(StationConnectionAttempt::Outcome::CoreTooOld), tooOld);
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    // A Core whose last session declared controlChannelVersion 0 is not
    // tried through the service at all; the record says why.
    void aCoreThatDeclaredNoControlChannelIsNotTriedThere()
    {
        Core core;
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Window window(core);
        window.route(service, RendezvousWire::rendezvousId(
                                  core.server->stationIdentity().publicKeySpki()),
                     true, /*controlChannelVersion=*/0);
        QSignalSpy ended(window.client.get(), &StationClient::sessionEnded);
        window.client->connectToStation(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(closedPort())), QString(), QString(),
            false, identityOf(core));
        QTRY_VERIFY_WITH_TIMEOUT(!ended.isEmpty(), 20000);
        QCOMPARE(ended.first().first().toString(),
                 QString::fromLatin1(RendezvousDialer::kCoreTooOldReason));
        QCOMPARE(window.outcomeFor(StationConnectionAttempt::Path::Service),
                 StationConnectionAttempt::Outcome::CoreTooOld);
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    void aStaleNegativeRouteIsRecheckedWhenTheRaceStarts()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/false);
        core.server->setRelayAllowed(false);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        window.route(service, rendezvous.client()->stationId(), false,
                     /*controlChannelVersion=*/0);
        StationClient::ServiceRoute route = window.client->serviceRoute();
        int currentVersion = -1; // expired observation from the saved Core
        int routeChecks = 0;
        route.currentControlChannelVersion = [&currentVersion, &routeChecks] {
            ++routeChecks;
            return currentVersion;
        };
        window.client->setServiceRoute(route);
        MediaIcePath direct;
        direct.localType = QStringLiteral("srflx");
        direct.remoteType = QStringLiteral("srflx");
        direct.localAddress = QStringLiteral("198.51.100.6");
        direct.remoteAddress = QStringLiteral("198.51.100.10");
        DataChannelTransport::setSelectedPathOverrideForTest(
            [direct](const DataChannelTransport*) {
                return std::optional<MediaIcePath>(direct);
            });
        const auto clearSelectedPath = qScopeGuard([] {
            DataChannelTransport::setSelectedPathOverrideForTest({});
        });
        window.client->connectToStation(
            QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(closedPort())), QString(), QString(),
            false, identityOf(core));
        {
            QString handshakeWhy;
            QVERIFY2(waitForHandshake(*window.client, 60000, &handshakeWhy),
                     qPrintable(handshakeWhy));
        }
        QVERIFY(routeChecks > 0);
        QCOMPARE(window.client->pathRank(), int(PathRacer::ServiceDirect));
        QCOMPARE(window.outcomeFor(StationConnectionAttempt::Path::Service),
                 StationConnectionAttempt::Outcome::Connected);
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    // A network whose proxy demands a login: the Core's address ends with
    // plain words saying so, not in silence.
    void aProxyThatNeedsALoginIsSaidPlainly()
    {
        NereusSDR::Test::LoginProxy proxy;
        QVERIFY(proxy.listen());
        proxy.useAsSystemProxy();
        DirectPathRung rung(QUrl(QStringLiteral("wss://127.0.0.1:9/")), 1 << 20);
        QSignalSpy ended(&rung, &PathRung::ended);
        rung.start();
        QTRY_COMPARE_WITH_TIMEOUT(ended.size(), 1, 10000);
        QVERIFY(proxy.requests() >= 1);
        QCOMPARE(ended.at(0).at(0).value<PathOutcome>(), PathOutcome::NoAnswer);
        QCOMPARE(ended.at(0).at(1).toString(),
                 QStringLiteral("This network's proxy needs a login, which NereusSDR can't provide."));
    }
};

QTEST_MAIN(TstPathRacer)
#include "tst_path_racer.moc"
