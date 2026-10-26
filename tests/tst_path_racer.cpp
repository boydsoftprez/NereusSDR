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
//   2026-10-15: the address list and admitted rungs of a first race and
//               a better-path search for a saved Core with IPv6-only
//               learned addresses, on the web relay. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-15: what a search admits against each current path, and the
//               race's admitted, dropped, ended and nothing-to-try log
//               lines. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-15: the live sequence: two failed races, the reconnect
//               retry through the service, then its better-path searches.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-15: an ICE pair not known yet ranks below ServiceDirect, and
//               a session through the service with no pair known searches
//               ICE (without the relay); one with a host pair does not.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-15: PR 524 review: a search's own ICE rung that opens with no
//               pair known moves a session with no pair known, or one on
//               the TURN relay, once; documentation addresses; the
//               heartbeat guard for an unknown pair; "canceled", "dialed".
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-15: Windows: the closed address may be stopped because the
//               service's path won first. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-24: an attempt that gets no answer says, in its log line, how
//               long after its dial and what it met: a refused connect, a
//               connection closed during TLS, a WebSocket that dropped with
//               no close frame before the Core's hello, or one the far end
//               closed with a code and a reason, quoted and escaped. Each
//               case matches one whole line.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QPointer>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSet>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>

#include <atomic>
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
#include "core/session/SessionTransport.h"
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

// A rung of the test's own that opens a plain WebSocket to `url` and hands
// it over, as a direct rung does once its TLS and upgrade are done; the far
// end then decides what the Core would have said.
class WebSocketRung final : public PathRung {
public:
    explicit WebSocketRung(const QUrl& url) : m_url(url) {}
    void start() override
    {
        auto* socket = new QWebSocket();
        auto* transport = new WebSocketTransport(socket, StationClient::kMaxIncomingMessageBytes, this);
        connect(socket, &QWebSocket::connected, this, [this, transport] {
            if (m_stopped) {
                return;
            }
            transport->setParent(nullptr);
            emit opened(transport);
        });
        socket->open(m_url);
    }
    void stop() override { m_stopped = true; }
    int rank() const override { return PathRacer::ThisNetwork; }
    PathKind kind() const override { return PathKind::ThisNetwork; }
    QString address() const override { return m_url.authority(); }

private:
    QUrl m_url;
    bool m_stopped = false;
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

// The planned rungs (address, start delay) of each race a window starts,
// read at the line it logs just before the race starts ("Racing every
// path to the Core", "Looking for a better path to the Core").
// The first connection's ICE pair in a service session.
enum class FirstPair { Unknown, Host, Relayed };

struct RacePlans {
    QPointer<StationClient> client;
    QList<QList<QPair<QString, int>>> first;
    QList<QList<QPair<QString, int>>> upgrades;
    // Milliseconds from each "Looking for a better path" to the racer's
    // own "All N attempt(s) failed".
    QList<qint64> upgradeFailMs;
    QElapsedTimer upgradeClock;
    bool upgradeRunning = false;
    // Per better-path search: whether it holds a service rung, and if so
    // whether that rung may take the relay.
    QList<std::optional<bool>> upgradeServiceAllowsRelay;
};
RacePlans* s_plans = nullptr;
QtMessageHandler s_previousHandler = nullptr;

void capturePlans(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (s_plans != nullptr && s_plans->client) {
        const bool first = message.contains(QStringLiteral("Racing every path to the Core"));
        const bool upgrade = message.contains(QStringLiteral("Looking for a better path to the Core"));
        if (upgrade) {
            s_plans->upgradeClock.start();
            s_plans->upgradeRunning = true;
        }
        if (s_plans->upgradeRunning && message.contains(QStringLiteral("attempt(s) failed"))
            && QByteArray(context.category) == QByteArray("nereus.session.pathracer")) {
            s_plans->upgradeFailMs.append(s_plans->upgradeClock.elapsed());
            s_plans->upgradeRunning = false;
        }
        if (first || upgrade) {
            const QList<PathRacer*> racers =
                s_plans->client->findChildren<PathRacer*>(Qt::FindDirectChildrenOnly);
            if (!racers.isEmpty()) {
                (first ? s_plans->first : s_plans->upgrades)
                    .append(racers.last()->plannedStartsForTest());
                if (upgrade) {
                    const auto rungs = racers.last()->findChildren<RendezvousPathRung*>();
                    s_plans->upgradeServiceAllowsRelay.append(
                        rungs.isEmpty() ? std::nullopt
                                        : std::optional<bool>(rungs.first()->allowsRelay()));
                }
            }
        }
    }
    if (s_previousHandler != nullptr) {
        s_previousHandler(type, context, message);
    }
}

int delayFor(const QList<QPair<QString, int>>& plan, const QString& address)
{
    for (const auto& [planned, delay] : plan) {
        if (planned == address) {
            return delay;
        }
    }
    return -1;
}

// Every line the racer logs, while a test holds one of these.
QStringList* s_racerLines = nullptr;
QtMessageHandler s_racerPrevious = nullptr;

void captureRacerLines(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (s_racerLines != nullptr
        && QByteArray(context.category) == QByteArray("nereus.session.pathracer")) {
        s_racerLines->append(message);
    }
    if (s_racerPrevious != nullptr) {
        s_racerPrevious(type, context, message);
    }
}

struct RacerLines {
    QStringList lines;
    RacerLines()
    {
        s_racerLines = &lines;
        s_racerPrevious = qInstallMessageHandler(captureRacerLines);
    }
    ~RacerLines()
    {
        qInstallMessageHandler(s_racerPrevious);
        s_racerPrevious = nullptr;
        s_racerLines = nullptr;
    }
    bool has(const QString& text) const
    {
        for (const QString& line : lines) {
            if (line.contains(text)) {
                return true;
            }
        }
        return false;
    }
    bool matches(const QRegularExpression& pattern) const
    {
        for (const QString& line : lines) {
            if (pattern.match(line).hasMatch()) {
                return true;
            }
        }
        return false;
    }
};

// An attempt's whole no-answer line: its address, the time after its dial
// and what it met, so a detail from another line cannot pass.
QRegularExpression noAnswerLine(const QString& address, const QString& detailPattern)
{
    return QRegularExpression(
        QStringLiteral("^Attempt at %1 ended: no answer \\[after \\d+ ms: %2\\]$")
            .arg(QRegularExpression::escape(address), detailPattern));
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

    // A saved Core like JJ's: its own address the IPv4 listener, its
    // learned addresses two IPv6 ones, its last good addresses all three.
    // Whatever rank the session has on the service (direct, TURN, or the
    // web relay), a better-path search admits the IPv4 address, once,
    // 250 ms after the IPv6 ones. Nothing is started, so nothing is dialed.
    void anUpgradeOnTheRelayAdmitsTheEntryAddress_data()
    {
        QTest::addColumn<int>("rank");
        QTest::newRow("service direct") << int(PathRacer::ServiceDirect);
        QTest::newRow("TURN relay") << int(PathRacer::ServiceRelayed);
        QTest::newRow("web relay") << int(PathRacer::Floor);
    }
    void anUpgradeOnTheRelayAdmitsTheEntryAddress()
    {
        QFETCH(int, rank);
        const QUrl v4(QStringLiteral("wss://192.0.2.106:47910"));
        const QUrl v6a(QStringLiteral("wss://[2001:db8:1:2::4602]:47910"));
        const QUrl v6b(QStringLiteral("wss://[2001:db8:1:2::15f2]:47910"));
        // The GUI's last good + learned list, repeats included.
        // StationClient::setCachedAddresses drops the repeats
        // (StationClient.cpp:1416-1420); the racer drops them too.
        const QList<QUrl> plan{v6a, v6b, v4, v6a, v6b, v4};
        PathRacer racer;
        racer.setBetterThan(rank);
        racer.addDirectUrls(plan, StationClient::kMaxIncomingMessageBytes);
        const auto planned = racer.plannedStartsForTest();
        QVERIFY2(PathRacer::rankFor(v4) < rank, "the IPv4 address ranks better than the service");
        QCOMPARE(planned.size(), 3);
        QCOMPARE(delayFor(planned, QStringLiteral("192.0.2.106:47910")), PathRacer::kIpv4DelayMs);
        QCOMPARE(delayFor(planned, QStringLiteral("[2001:db8:1:2::4602]:47910")), 0);
        QCOMPARE(delayFor(planned, QStringLiteral("[2001:db8:1:2::15f2]:47910")), 0);
    }

    // A search that admits nothing says so in its own words, not as a
    // path that failed, and says what bar it started with.
    void aSearchThatAdmitsNothingSaysSo()
    {
        RacerLines log;
        PathRacer racer;
        QSignalSpy failed(&racer, &PathRacer::failed);
        racer.setBetterThan(PathRacer::ThisNetwork);
        racer.addDirectUrls({QUrl(QStringLiteral("wss://192.0.2.106:47910"))},
                            StationClient::kMaxIncomingMessageBytes);
        racer.start();
        QCOMPARE(failed.size(), 1);
        QVERIFY2(log.has(QStringLiteral("No address was worth trying for a better path")),
                 qPrintable(log.lines.join(QLatin1Char('\n'))));
        QVERIFY2(!log.has(QStringLiteral("No path reached the Core")),
                 qPrintable(log.lines.join(QLatin1Char('\n'))));
        QVERIFY2(log.has(QStringLiteral("better than rank 0 (this network)")),
                 qPrintable(log.lines.join(QLatin1Char('\n'))));
    }

    // Each address is logged as admitted or dropped, with the reason and
    // the ranks compared, and each attempt's end with its outcome.
    void eachAddressIsLoggedAdmittedOrDropped()
    {
        const QUrl v4(QStringLiteral("wss://192.0.2.106:47910"));
        const QUrl v6(QStringLiteral("wss://[2001:db8:1:2::4602]:47910"));
        {
            RacerLines log;
            PathRacer racer;
            racer.setBetterThan(PathRacer::ServiceDirect);
            racer.addDirectUrls({v6, v4, v4}, StationClient::kMaxIncomingMessageBytes);
            const QString joined = log.lines.join(QLatin1Char('\n'));
            QVERIFY2(log.has(QStringLiteral("192.0.2.106:47910 admitted: rank %1")
                                 .arg(PathRacer::rankFor(v4))), qPrintable(joined));
            QVERIFY2(log.has(QStringLiteral("is below 2 (service direct)")), qPrintable(joined));
            QVERIFY2(log.has(QStringLiteral("192.0.2.106:47910 dropped: already in this race")),
                     qPrintable(joined));
        }
        {
            RacerLines log;
            PathRacer upgrade;
            upgrade.setBetterThan(PathRacer::ThisNetwork);
            upgrade.addDirectUrls({v6}, StationClient::kMaxIncomingMessageBytes);
            const QString joined = log.lines.join(QLatin1Char('\n'));
            // A search's own lines say they are a better-path search's.
            QVERIFY2(log.has(QStringLiteral(
                         "Better-path search: [2001:db8:1:2::4602]:47910 dropped")),
                     qPrintable(joined));
            QVERIFY2(log.has(QStringLiteral(
                         "[2001:db8:1:2::4602]:47910 dropped: rank %1")
                         .arg(PathRacer::rankFor(v6))), qPrintable(joined));
            QVERIFY2(log.has(QStringLiteral("is not below 0 (this network)")), qPrintable(joined));
        }
        {
            // An attempt that ends at once (not a secure address): its end,
            // with the outcome. Nothing is dialed.
            RacerLines log;
            PathRacer first;
            first.addDirectUrls({QUrl(QStringLiteral("ws://127.0.0.1:1"))},
                                StationClient::kMaxIncomingMessageBytes);
            first.start();
            QTRY_VERIFY2(log.has(QStringLiteral("Attempt at 127.0.0.1:1 ended: failed")),
                         qPrintable(log.lines.join(QLatin1Char('\n'))));
            const QString joined = log.lines.join(QLatin1Char('\n'));
            QVERIFY2(log.has(QStringLiteral("127.0.0.1:1 admitted: rank 0 (this network)")),
                     qPrintable(joined));
            QVERIFY2(log.has(QStringLiteral("Race started with 1 attempt(s), any rank")),
                     qPrintable(joined));
            // The racer's failure is its own line, not the session's.
            QTRY_VERIFY2(log.has(QStringLiteral("All 1 attempt(s) failed, any rank")),
                         qPrintable(log.lines.join(QLatin1Char('\n'))));
            QVERIFY2(!log.has(QStringLiteral("No path reached the Core")), qPrintable(joined));
        }
    }

    // An attempt that gets no answer says in its log line how long after
    // its dial it ended and what it met, so a log can tell a refused
    // connect from a connection closed during TLS, a link that dropped with
    // no close frame, or one the far end closed before the Core's hello.
    // The operator's words are unchanged.
    void anAttemptWithNoAnswerSaysWhatItMet()
    {
        {
            // Nothing listening: the connect is refused.
            RacerLines log;
            PathRacer racer;
            const quint16 port = closedPort();
            racer.addDirectUrls({QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(port))},
                                StationClient::kMaxIncomingMessageBytes);
            racer.start();
            QTRY_VERIFY2(log.matches(noAnswerLine(QStringLiteral("127.0.0.1:%1").arg(port),
                                                  QStringLiteral("socket error ConnectionRefusedError, .+"))),
                         qPrintable(log.lines.join(QLatin1Char('\n'))));
        }
        {
            // Something accepts and closes at once, before TLS is done. The
            // TLS backends differ in how they name that (Secure Transport and
            // OpenSSL see the remote close; Schannel may fail the handshake).
            QTcpServer closing;
            QVERIFY(closing.listen(QHostAddress::LocalHost));
            connect(&closing, &QTcpServer::newConnection, &closing, [&closing] {
                while (QTcpSocket* accepted = closing.nextPendingConnection()) {
                    accepted->abort();
                    accepted->deleteLater();
                }
            });
            RacerLines log;
            PathRacer racer;
            racer.addDirectUrls({QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(closing.serverPort()))},
                                StationClient::kMaxIncomingMessageBytes);
            racer.start();
            QTRY_VERIFY2(log.matches(noAnswerLine(
                             QStringLiteral("127.0.0.1:%1").arg(closing.serverPort()),
                             QStringLiteral("socket error (RemoteHostClosedError|SslHandshakeFailedError), .+"))),
                         qPrintable(log.lines.join(QLatin1Char('\n'))));
        }
        {
            // The far end opens the WebSocket and drops the link before any
            // hello, with no close frame: Qt's close code would read 1000.
            QWebSocketServer far(QStringLiteral("drops-before-hello"),
                                 QWebSocketServer::NonSecureMode);
            QVERIFY(far.listen(QHostAddress::LocalHost, 0));
            connect(&far, &QWebSocketServer::newConnection, &far, [&far] {
                while (QWebSocket* accepted = far.nextPendingConnection()) {
                    accepted->setParent(&far);
                    accepted->flush();
                    accepted->abort();
                }
            });
            RacerLines log;
            PathRacer racer;
            const QUrl url(QStringLiteral("ws://127.0.0.1:%1").arg(far.serverPort()));
            racer.addRung(new WebSocketRung(url));
            QSignalSpy failed(&racer, &PathRacer::failed);
            racer.start();
            QTRY_COMPARE(failed.count(), 1);
            QVERIFY2(log.matches(noAnswerLine(
                         url.authority(),
                         QStringLiteral("closed before the Core's hello, no close frame(, socket error \\w+, .+)?"))),
                     qPrintable(log.lines.join(QLatin1Char('\n'))));
        }
        {
            // The far end opens the WebSocket and closes it with a code
            // before any hello. Its reason is quoted and escaped, so a line
            // break in it cannot start a line of its own.
            QWebSocketServer far(QStringLiteral("closes-before-hello"),
                                 QWebSocketServer::NonSecureMode);
            QVERIFY(far.listen(QHostAddress::LocalHost, 0));
            connect(&far, &QWebSocketServer::newConnection, &far, [&far] {
                while (QWebSocket* accepted = far.nextPendingConnection()) {
                    accepted->setParent(&far);
                    accepted->close(QWebSocketProtocol::CloseCodePolicyViolated,
                                    QStringLiteral("full\nAttempt at forged \"x\""));
                }
            });
            RacerLines log;
            PathRacer racer;
            const QUrl url(QStringLiteral("ws://127.0.0.1:%1").arg(far.serverPort()));
            racer.addRung(new WebSocketRung(url));
            QSignalSpy failed(&racer, &PathRacer::failed);
            racer.start();
            QTRY_COMPARE(failed.count(), 1);
            QVERIFY2(log.matches(noAnswerLine(
                         url.authority(),
                         QRegularExpression::escape(QStringLiteral(
                             "closed before the Core's hello, close code 1008, "
                             "reason \"full\\u000aAttempt at forged \\\"x\\\"\"")))),
                     qPrintable(log.lines.join(QLatin1Char('\n'))));
            // The operator's words do not carry it.
            QVERIFY(!failed.first().first().toString().contains(QStringLiteral("close code")));
        }
    }

    // A race canceled with attempts still open (the session ended, or a
    // new race stopped it) logs its end, naming them and why.
    void aCanceledRaceLogsItsOpenAttempts()
    {
        RacerLines log;
        PathRacer racer;
        racer.addDirectUrls({QUrl(QStringLiteral("wss://127.0.0.1:1")),
                             QUrl(QStringLiteral("wss://127.0.0.1:2"))},
                            StationClient::kMaxIncomingMessageBytes);
        racer.cancel(QStringLiteral("the session ended"));
        const QString joined = log.lines.join(QLatin1Char('\n'));
        QVERIFY2(log.has(QStringLiteral(
                     "Race canceled (the session ended) with 2 attempt(s) still open: "
                     "127.0.0.1:1, 127.0.0.1:2")), qPrintable(joined));

        // Canceled again, or once everything has ended: no second line.
        log.lines.clear();
        racer.cancel(QStringLiteral("stopped"));
        QVERIFY2(!log.has(QStringLiteral("Race canceled")), qPrintable(log.lines.join(QLatin1Char('\n'))));
    }

    // JJ's entry against every path a session can be on. A search admits
    // exactly the addresses that rank below the session's path; each
    // service path admits all three. The this-network and direct rows
    // depend on this computer's interfaces (192.0.2.106 is this
    // network only where an interface holds that subnet), which is why
    // their expectation comes from rankFor.
    void anUpgradeAdmitsWhatBeatsEachCurrentPath_data()
    {
        QTest::addColumn<int>("rank");
        QTest::newRow("this network") << int(PathRacer::ThisNetwork);
        QTest::newRow("direct") << int(PathRacer::Direct);
        QTest::newRow("service direct") << int(PathRacer::ServiceDirect);
        QTest::newRow("TURN relay") << int(PathRacer::ServiceRelayed);
        QTest::newRow("web relay") << int(PathRacer::Floor);
    }
    void anUpgradeAdmitsWhatBeatsEachCurrentPath()
    {
        QFETCH(int, rank);
        const QList<QUrl> entry{
            QUrl(QStringLiteral("wss://[2001:db8:1:2::4602]:47910")),
            QUrl(QStringLiteral("wss://[2001:db8:1:2::15f2]:47910")),
            QUrl(QStringLiteral("wss://192.0.2.106:47910"))};
        PathRacer racer;
        racer.setBetterThan(rank);
        racer.addDirectUrls(entry + entry, StationClient::kMaxIncomingMessageBytes);
        QStringList expected;
        for (const QUrl& url : entry) {
            if (PathRacer::rankFor(url) < rank) {
                expected.append(url.port() > 0 ? QStringLiteral("%1:%2").arg(
                    url.host().contains(QLatin1Char(':')) ? QLatin1Char('[') + url.host() + QLatin1Char(']')
                                                          : url.host()).arg(url.port())
                                               : url.host());
            }
        }
        QStringList admitted;
        for (const auto& [address, delay] : racer.plannedStartsForTest()) {
            admitted.append(address);
        }
        QCOMPARE(admitted, expected);
        if (rank >= PathRacer::ServiceDirect) {
            QCOMPARE(admitted.size(), 3);
        }
    }

    // The same entry shape with a Core on this computer: the IPv6
    // addresses refuse at once and the IPv4 one is closed. The first race
    // after a start and each better-path search while the session runs on
    // the web relay plan the entry's own address, 250 ms after the IPv6
    // ones, even after it failed in the race before, and no such search
    // reports "No path" before that address's turn.
    void aWebRelaySessionsSearchesPlanTheEntryAddress()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        MediaIcePath webRelay;
        webRelay.localType = QStringLiteral("host");
        webRelay.remoteType = QStringLiteral("host");
        webRelay.localAddress = QStringLiteral("127.0.0.1");
        webRelay.remoteAddress = QStringLiteral("127.0.0.1");
        webRelay.ownedLoopbackShim = true;
        DataChannelTransport::setSelectedPathOverrideForTest(
            [webRelay](const DataChannelTransport*) { return std::optional<MediaIcePath>(webRelay); });
        const auto clearOverride = qScopeGuard([] { DataChannelTransport::setSelectedPathOverrideForTest({}); });

        const quint16 v4Port = closedPort();
        const QUrl v4(QStringLiteral("wss://127.0.0.1:%1").arg(v4Port));
        const QUrl v6a(QStringLiteral("wss://[::1]:%1").arg(closedPort()));
        const QUrl v6b(QStringLiteral("wss://[::1]:%1").arg(closedPort()));
        const QString v4Line = QStringLiteral("127.0.0.1:%1").arg(v4Port);
        StationClient::ServiceRoute route;
        route.servers = {service.url()};
        route.rendezvousId = rendezvous.client()->stationId();
        route.relayAllowed = true;
        route.controlChannelVersion = 1;
        // As RemoteConnectionController's source gives it for this entry:
        // directCandidates (last good + learned), last good, learned.
        window.route(service, rendezvous.client()->stationId());
        window.client->setCandidateSource([=]() -> std::optional<StationClient::ConnectionCandidates> {
            return StationClient::ConnectionCandidates{{v6a, v6b, v4}, route};
        });
        window.client->setUpgradeScheduleForTest({300});

        RacePlans plans;
        plans.client = window.client.get();
        s_plans = &plans;
        s_previousHandler = qInstallMessageHandler(capturePlans);
        const auto restore = qScopeGuard([] {
            qInstallMessageHandler(s_previousHandler);
            s_previousHandler = nullptr;
            s_plans = nullptr;
        });

        window.client->connectToStation(v4, QString(), QString(), false, identityOf(core));
        QString why;
        QVERIFY2(waitForHandshake(*window.client, kServiceConnectBudgetMs, &why), qPrintable(why));
        QVERIFY(!plans.first.isEmpty());
        const auto first = plans.first.first();
        QCOMPARE(delayFor(first, v4Line), PathRacer::kIpv4DelayMs);
        QCOMPARE(delayFor(first, QStringLiteral("[::1]:%1").arg(v6a.port())), 0);
        QCOMPARE(delayFor(first, QStringLiteral("[::1]:%1").arg(v6b.port())), 0);

        // The GUI hands the window the entry's last good + learned
        // addresses after the sign-in (GuiConnectionController.cpp:1030-1035).
        window.client->setCachedAddresses({v6a, v6b, v4});
        QTRY_VERIFY_WITH_TIMEOUT(plans.upgrades.size() >= 2, 20000);
        QCOMPARE(window.client->pathRank(), int(PathRacer::Floor));
        // A search that holds an IPv6 address and the IPv4 one cannot end
        // before the IPv4 rung's turn: its failure comes 250 ms or more after
        // "Looking for a better path", never within a few milliseconds.
        // A coarse QTimer may fire up to 5% early.
        constexpr qint64 kEarliestFailMs = PathRacer::kIpv4DelayMs * 19 / 20;
        QVERIFY(!plans.upgradeFailMs.isEmpty());
        for (const qint64 ms : std::as_const(plans.upgradeFailMs)) {
            QVERIFY2(ms >= kEarliestFailMs, qPrintable(QString::number(ms)));
        }
        for (const auto& upgrade : std::as_const(plans.upgrades)) {
            QCOMPARE(delayFor(upgrade, v4Line), PathRacer::kIpv4DelayMs);
            QCOMPARE(delayFor(upgrade, QStringLiteral("[::1]:%1").arg(v6a.port())), 0);
            QCOMPARE(delayFor(upgrade, QStringLiteral("[::1]:%1").arg(v6b.port())), 0);
        }
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    // libjuice may report no selected pair when the service's connection
    // opens. Such a pair is not ranked as direct ICE: a better-path search
    // still tries ICE through the service (without the relay). A known
    // host pair is direct ICE already, and its searches leave the service
    // out.
    void anUnknownIcePairRanksWorseThanServiceDirect()
    {
        QVERIFY(PathRacer::rankForPath(std::nullopt) > int(PathRacer::ServiceDirect));
        MediaIcePath host;
        host.localType = QStringLiteral("host");
        host.remoteType = QStringLiteral("host");
        host.localAddress = QStringLiteral("127.0.0.1");
        host.remoteAddress = QStringLiteral("127.0.0.1");
        QCOMPARE(PathRacer::rankForPath(host), int(PathRacer::ServiceDirect));
    }

    // The session's own path: a host pair, no pair known, or the TURN
    // relay. Every better-path search's ICE rung opens before its own pair
    // is known; it takes no relay, so it is direct ICE through the
    // service, and a session not known to be on it moves there once.
    void aServiceSessionSearchesIceWhileItsPairIsUnknown_data()
    {
        QTest::addColumn<int>("first");
        QTest::newRow("no selected pair") << int(FirstPair::Unknown);
        QTest::newRow("host pair") << int(FirstPair::Host);
        QTest::newRow("TURN relay") << int(FirstPair::Relayed);
    }

    void aServiceSessionSearchesIceWhileItsPairIsUnknown()
    {
        QFETCH(int, first);
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        RacePlans plans;
        plans.client = window.client.get();
        MediaIcePath host;
        host.localType = QStringLiteral("host");
        host.remoteType = QStringLiteral("host");
        host.localAddress = QStringLiteral("127.0.0.1");
        host.remoteAddress = QStringLiteral("127.0.0.1");
        MediaIcePath turn = host;
        turn.localType = QStringLiteral("relay");
        // The first connection's own pair; no pair known for any later one
        // (the search's own connection included).
        std::atomic<const DataChannelTransport*> firstTransport{nullptr};
        DataChannelTransport::setSelectedPathOverrideForTest(
            [host, turn, first, &firstTransport](const DataChannelTransport* transport) -> std::optional<MediaIcePath> {
                if (first == int(FirstPair::Host)) {
                    return host;
                }
                const DataChannelTransport* expected = nullptr;
                firstTransport.compare_exchange_strong(expected, transport);
                if (first == int(FirstPair::Relayed) && firstTransport.load() == transport) {
                    return turn;
                }
                return std::nullopt;
            });
        const auto clearOverride = qScopeGuard([] { DataChannelTransport::setSelectedPathOverrideForTest({}); });

        const QUrl v4(QStringLiteral("wss://127.0.0.1:%1").arg(closedPort()));
        StationClient::ServiceRoute route;
        route.servers = {service.url()};
        route.rendezvousId = rendezvous.client()->stationId();
        route.relayAllowed = true;
        route.controlChannelVersion = 1;
        window.route(service, rendezvous.client()->stationId());
        window.client->setCandidateSource([=]() -> std::optional<StationClient::ConnectionCandidates> {
            return StationClient::ConnectionCandidates{{v4}, route};
        });
        window.client->setUpgradeScheduleForTest({300});

        s_plans = &plans;
        s_previousHandler = qInstallMessageHandler(capturePlans);
        const auto restore = qScopeGuard([] {
            qInstallMessageHandler(s_previousHandler);
            s_previousHandler = nullptr;
            s_plans = nullptr;
        });

        window.client->connectToStation(v4, QString(), QString(), false, identityOf(core));
        QString why;
        QVERIFY2(waitForHandshake(*window.client, kServiceConnectBudgetMs, &why), qPrintable(why));
        QVERIFY(window.client->connectedUrl().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(plans.upgradeFailMs.size() >= 2, 20000);
        QCOMPARE(plans.upgradeServiceAllowsRelay.size(), plans.upgrades.size());
        const bool moves = first != int(FirstPair::Host);
        for (qsizetype index = 0; index < plans.upgradeServiceAllowsRelay.size(); ++index) {
            const auto& allowsRelay = plans.upgradeServiceAllowsRelay.at(index);
            if (moves && index == 0) {
                // The search from the first path: ICE through the service,
                // without the relay.
                QVERIFY(allowsRelay.has_value());
                QVERIFY(!*allowsRelay);
            } else {
                // From direct ICE through the service: the service is no
                // better.
                QVERIFY2(!allowsRelay.has_value(), qPrintable(QString::number(index)));
            }
        }
        QCOMPARE(window.client->pathSwitches(), moves ? 1 : 0);
        QCOMPARE(window.client->pathRank(), int(PathRacer::ServiceDirect));
        QVERIFY(!window.client->pathPairUnknown());
        window.client->disconnectFromStation(QStringLiteral("test done"));
    }

    // The guards that keep a pair not known yet from reading as the relay:
    // the heartbeat keeps the direct interval, and a known relay takes the
    // relayed one.
    void anUnknownPairKeepsTheDirectHeartbeat()
    {
        Core core;
        Window window(core);
        constexpr int kIntervalMs = 10000;
        window.client->setHeartbeatIntervalMs(kIntervalMs);
        window.client->setPathRankForTest(PathRacer::ServiceRelayed, /*pairUnknown=*/true);
        QCOMPARE(window.client->effectiveHeartbeatIntervalMs(), kIntervalMs);
        window.client->setPathRankForTest(PathRacer::ServiceRelayed, /*pairUnknown=*/false);
        QCOMPARE(window.client->effectiveHeartbeatIntervalMs(),
                 StationClient::kRelayedHeartbeatIntervalMs);
        window.client->setPathRankForTest(PathRacer::ServiceDirect, /*pairUnknown=*/false);
        QCOMPARE(window.client->effectiveHeartbeatIntervalMs(), kIntervalMs);
    }

    // The live sequence of 2026-10-15 10:11-10:12: the first race and its
    // first retry fail (the service not reachable then), the window's own
    // reconnect retry reaches the Core through the service ("Remote access
    // service (no direct Core address)": race mode, no direct winner), and
    // the GUI then hands over the entry's last good + learned addresses.
    // Every better-path search of that session still admits the entry's
    // IPv4 address and does not fail before its turn.
    void theRetrySessionThroughTheServiceSearchesTheEntryAddress()
    {
        LocalService service;
        QVERIFY2(service.start(), qPrintable(service.startFailure()));
        Core core;
        StationRendezvous rendezvous(core.server.get(), {service.url()}, /*relayAllowed=*/true);
        QSignalSpy registered(rendezvous.client(), &RendezvousClient::registered);
        QVERIFY(rendezvous.start());
        QTRY_COMPARE_WITH_TIMEOUT(registered.size(), 1, 10000);
        Window window(core);
        window.client->setReconnectBackoffUnitMs(50);
        window.client->setUpgradeScheduleForTest({300});

        const quint16 v4Port = closedPort();
        const QUrl v4(QStringLiteral("wss://127.0.0.1:%1").arg(v4Port));
        const QUrl v6a(QStringLiteral("wss://[::1]:%1").arg(closedPort()));
        const QUrl v6b(QStringLiteral("wss://[::1]:%1").arg(closedPort()));
        const QString v4Line = QStringLiteral("127.0.0.1:%1").arg(v4Port);
        StationClient::ServiceRoute route;
        route.servers = {service.url()};
        route.rendezvousId = rendezvous.client()->stationId();
        route.relayAllowed = true;
        route.controlChannelVersion = 1;
        // As RemoteConnectionController's source gives this entry: its
        // direct candidates (last good + learned), last good, learned. The
        // first two races find no service.
        auto calls = std::make_shared<int>(0);
        window.client->setCandidateSource([=]() -> std::optional<StationClient::ConnectionCandidates> {
            ++*calls;
            StationClient::ConnectionCandidates candidates;
            candidates.addresses = {v6a, v6b, v4, v6a, v6b, v4, v6a, v6b};
            if (*calls > 2) {
                candidates.service = route;
            }
            return candidates;
        });

        RacePlans plans;
        plans.client = window.client.get();
        s_plans = &plans;
        s_previousHandler = qInstallMessageHandler(capturePlans);
        const auto restore = qScopeGuard([] {
            qInstallMessageHandler(s_previousHandler);
            s_previousHandler = nullptr;
            s_plans = nullptr;
        });

        QSignalSpy ended(window.client.get(), &StationClient::sessionEnded);
        window.client->connectToStation(v4, QString(), QString(), false, identityOf(core));
        // waitForHandshake() gives up at a session's end; here two end first.
        QTRY_VERIFY_WITH_TIMEOUT(window.client->isHandshakeComplete(), kServiceConnectBudgetMs);
        QCOMPARE(ended.size(), 2);
        QVERIFY(*calls >= 3);
        // "no direct Core address": the service won, in race mode.
        QVERIFY(window.client->connectedUrl().isEmpty());
        QVERIFY(window.client->pathRank() >= int(PathRacer::ServiceDirect));
        QCOMPARE(plans.first.size(), 3);
        for (const auto& race : std::as_const(plans.first)) {
            QCOMPARE(delayFor(race, v4Line), PathRacer::kIpv4DelayMs);
        }

        // rememberAuthenticatedRadio: last good (IPv6 x2 + IPv4), then
        // learned (IPv6 x2), repeats removed.
        window.client->setCachedAddresses({v6a, v6b, v4});
        QTRY_VERIFY_WITH_TIMEOUT(plans.upgradeFailMs.size() >= 2, 20000);
        constexpr qint64 kEarliestFailMs = PathRacer::kIpv4DelayMs * 19 / 20;
        for (const auto& upgrade : std::as_const(plans.upgrades)) {
            QCOMPARE(delayFor(upgrade, v4Line), PathRacer::kIpv4DelayMs);
        }
        for (const qint64 ms : std::as_const(plans.upgradeFailMs)) {
            QVERIFY2(ms >= kEarliestFailMs, qPrintable(QString::number(ms)));
        }
        window.client->disconnectFromStation(QStringLiteral("test done"));
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
#ifdef Q_OS_WIN
        // Windows takes about 2 s to refuse a closed localhost port, so the
        // service's path can win first and the address is stopped.
        QVERIFY2(window.outcomeFor(StationConnectionAttempt::Path::ThisNetwork)
                         == StationConnectionAttempt::Outcome::NoAnswer
                     || window.outcomeFor(StationConnectionAttempt::Path::ThisNetwork)
                            == StationConnectionAttempt::Outcome::AnotherPathFirst,
                 qPrintable(window.client->connectionAttempt().summary()));
#else
        QCOMPARE(window.outcomeFor(StationConnectionAttempt::Path::ThisNetwork),
                 StationConnectionAttempt::Outcome::NoAnswer);
#endif
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
