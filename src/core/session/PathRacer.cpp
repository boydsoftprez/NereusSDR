// =================================================================
// src/core/session/PathRacer.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// iPhone app plan Task 29 (R-IOS-16): see PathRacer.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-04: Qt 6.4 WebSocket error-signal compatibility. J.J. Boyd
//               (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: LINK minor 10: a standby the Core closed is released and
//               deleted. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/NetworkTrouble.h"
#include "core/session/SystemProxy.h"
#include "core/session/PathRacer.h"

#include "core/security/ClientDeviceIdentity.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/RendezvousDialer.h"
#include "core/session/SessionTransport.h"
#include "core/session/StationClient.h"

#include <memory>
#include <QAuthenticator>
#include <QHostAddress>
#include <QHostInfo>
#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QSslError>
#include <QTimer>
#include <QWebSocket>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcPathRacer, "nereus.session.pathracer")

namespace {

constexpr const char* kNoPath = "The Core could not be reached from here.";

bool isIpv6Host(const QString& host)
{
    const QHostAddress address(host);
    return !address.isNull() && address.protocol() == QAbstractSocket::IPv6Protocol;
}

QString hostPort(const QUrl& url)
{
    const QString host = url.host().contains(QLatin1Char(':'))
        ? QLatin1Char('[') + url.host() + QLatin1Char(']')
        : url.host();
    return url.port() > 0 ? QStringLiteral("%1:%2").arg(host).arg(url.port()) : host;
}

} // namespace

// ── The race ──────────────────────────────────────────────────────────────

struct PathRacer::Entry {
    QPointer<Rung> rung;
    int line = -1;
    int startDelayMs = 0;
    QTimer* startTimer = nullptr;
    QTimer* helloTimer = nullptr;
    QPointer<SessionTransport> transport;
    Outcome outcome = Outcome::Trying;
    bool ended = false;
    /// Ready and not yet taken: the standby.
    bool standby = false;
    QByteArray hello;
    /// A direct rung started and not yet ready or ended (review Minor 8).
    bool opening = false;
};

PathRacer::PathRacer(QObject* parent) : QObject(parent) {}

PathRacer::~PathRacer()
{
    cancel();
}

int PathRacer::rankForPath(const std::optional<MediaIcePath>& path)
{
    if (!path) {
        return ServiceDirect;
    }
    if (path->viaLoopbackShim()) {
        return Floor;
    }
    return path->relayed() ? ServiceRelayed : ServiceDirect;
}

int PathRacer::rankFor(const QUrl& url)
{
    return StationConnectionAttempt::pathFor(url) == StationConnectionAttempt::Path::ThisNetwork
        ? ThisNetwork
        : Direct;
}

int PathRacer::addEntry(Rung* rung, int startDelayMs)
{
    rung->setParent(this);
    auto entry = std::make_unique<Entry>();
    entry->rung = rung;
    entry->startDelayMs = std::max(0, startDelayMs);
    Line line;
    line.kind = rung->kind();
    line.address = rung->address();
    m_lines.append(line);
    entry->line = static_cast<int>(m_lines.size()) - 1;
    m_entries.push_back(std::move(entry));
    return static_cast<int>(m_entries.size()) - 1;
}

void PathRacer::addRung(Rung* rung, int startDelayMs)
{
    if (rung == nullptr || m_started) {
        return;
    }
    addEntry(rung, startDelayMs);
}

void PathRacer::addNote(PathKind kind, const QString& address, Outcome outcome,
                        const QString& reason)
{
    Line line;
    line.kind = kind;
    line.address = address;
    line.outcome = outcome;
    line.reason = reason;
    m_lines.append(line);
    emit linesChanged();
}

void PathRacer::addDirectUrls(const QList<QUrl>& urls, quint64 maxIncomingBytes)
{
    if (m_started) {
        return;
    }
    bool ipv6Present = false;
    for (const QUrl& url : urls) {
        if (isIpv6Host(url.host())) {
            ipv6Present = true;
        }
    }
    QList<QUrl> seen;
    for (const QUrl& url : urls) {
        if (!url.isValid() || seen.contains(url)) {
            continue;
        }
        seen.append(url);
        if (!QHostAddress(url.host()).isNull()) {
            if (!admitDirect(url)) {
                continue;
            }
            const bool ipv4 = !isIpv6Host(url.host());
            addRung(new DirectPathRung(url, maxIncomingBytes),
                    ipv4 && ipv6Present ? kIpv4DelayMs : 0);
        } else {
            m_pendingLookups.append(url);
        }
    }
    m_lookupMaxIncoming = maxIncomingBytes;
}

void PathRacer::resolveAndAdd(const QUrl& url, quint64 maxIncomingBytes, bool ipv6Present)
{
    ++m_lookupsRunning;
    const QPointer<PathRacer> self(this);
    QHostInfo::lookupHost(url.host(), this, [this, self, url, maxIncomingBytes,
                                             ipv6Present](const QHostInfo& info) {
        if (!self || m_done) {
            return;
        }
        --m_lookupsRunning;
        QList<QHostAddress> v6;
        QList<QHostAddress> v4;
        for (const QHostAddress& address : info.addresses()) {
            if (address.protocol() == QAbstractSocket::IPv6Protocol) {
                v6.append(address);
            } else if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                v4.append(address);
            }
        }
        if (v6.isEmpty() && v4.isEmpty()) {
            // A name that does not resolve is an address that does not
            // answer, in the record under its own name.
            addNote(rankFor(url) == ThisNetwork ? PathKind::ThisNetwork : PathKind::Direct,
                    hostPort(url), Outcome::NoAnswer, QString());
            checkAllEnded();
            return;
        }
        const bool anyV6 = ipv6Present || !v6.isEmpty();
        QList<QPair<QHostAddress, int>> ordered;
        for (const QHostAddress& address : std::as_const(v6)) {
            ordered.append({address, 0});
        }
        for (const QHostAddress& address : std::as_const(v4)) {
            ordered.append({address, anyV6 ? kIpv4DelayMs : 0});
        }
        for (const auto& [address, delay] : ordered) {
            // Review Minor 8: an IPv6 address keeps its scope (a link-local
            // one reaches nothing without it; QUrl carries it as %25).
            QUrl resolved = url;
            resolved.setHost(address.toString());
            if (!admitDirect(resolved)) {
                continue;
            }
            const int index = addEntry(new DirectPathRung(resolved, maxIncomingBytes), delay);
            emit linesChanged();
            startRung(*m_entries[static_cast<size_t>(index)]);
        }
        checkAllEnded();
    });
}

bool PathRacer::admitDirect(const QUrl& url)
{
    // An upgrade never dials an address whose rank cannot beat the path in
    // use (a connection every five minutes, for ever, for nothing).
    if (m_betterThan && rankFor(url) >= *m_betterThan) {
        return false;
    }
    const QHostAddress address(url.host());
    const QString key = (address.isNull() ? url.host().toLower() : address.toString())
        + QLatin1Char('/') + QString::number(url.port());
    if (m_directTargets.contains(key)) {
        return false;
    }
    m_directTargets.insert(key);
    return true;
}

void PathRacer::startWaitingDirect()
{
    while (m_directOpening < kMaxDirectOpening && !m_directWaiting.isEmpty() && !m_done) {
        const int index = m_directWaiting.takeFirst();
        Entry& e = *m_entries[static_cast<size_t>(index)];
        if (e.ended || e.rung == nullptr) {
            continue;
        }
        ++m_directOpening;
        e.opening = true;
        qCDebug(lcPathRacer) << "Trying" << e.rung->address();
        e.rung->start();
    }
}

void PathRacer::releaseDirectTurn(Entry& entry)
{
    if (!entry.opening) {
        return;
    }
    entry.opening = false;
    --m_directOpening;
    // The next waiting direct rung, after this call's own bookkeeping.
    QTimer::singleShot(0, this, [self = QPointer<PathRacer>(this)] {
        if (self) {
            self->startWaitingDirect();
        }
    });
}

void PathRacer::start()
{
    if (m_started) {
        return;
    }
    m_started = true;
    bool ipv6Present = false;
    for (const auto& entry : m_entries) {
        if (auto* direct = qobject_cast<DirectPathRung*>(entry->rung.data());
            direct != nullptr && isIpv6Host(direct->url().host())) {
            ipv6Present = true;
        }
    }
    const QList<QUrl> lookups = std::exchange(m_pendingLookups, {});
    for (const QUrl& url : lookups) {
        resolveAndAdd(url, m_lookupMaxIncoming, ipv6Present);
    }
    for (auto& entry : m_entries) {
        startRung(*entry);
    }
    emit linesChanged();
    checkAllEnded();
}

void PathRacer::startRung(Entry& entry)
{
    Rung* rung = entry.rung;
    if (rung == nullptr || entry.ended || entry.startTimer != nullptr) {
        return;
    }
    const int index = [this, &entry] {
        for (size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].get() == &entry) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }();
    connect(rung, &Rung::opened, this,
            [this, index](SessionTransport* transport) { onOpened(index, transport); });
    connect(rung, &Rung::ended, this, [this, index](Outcome outcome, const QString& reason) {
        endRung(index, outcome, reason);
    });
    connect(rung, &Rung::noted, this,
            [this, rung](PathKind kind, Outcome outcome, const QString& reason) {
        for (const Line& line : std::as_const(m_lines)) {
            if (line.kind == kind && line.outcome == outcome) {
                return;
            }
        }
        addNote(kind, rung->address(), outcome, reason);
    });
    entry.startTimer = new QTimer(this);
    entry.startTimer->setSingleShot(true);
    connect(entry.startTimer, &QTimer::timeout, this, [this, index] {
        if (m_done || index < 0) {
            return;
        }
        Entry& e = *m_entries[static_cast<size_t>(index)];
        if (e.ended || e.rung == nullptr) {
            return;
        }
        // Review Minor 8: direct connections take their turn within the
        // Core's per-address handshake cap.
        if (qobject_cast<DirectPathRung*>(e.rung.data()) != nullptr) {
            m_directWaiting.append(index);
            startWaitingDirect();
            return;
        }
        qCDebug(lcPathRacer) << "Trying" << e.rung->address();
        e.rung->start();
    });
    entry.startTimer->start(entry.startDelayMs);
}

void PathRacer::onOpened(int index, SessionTransport* transport)
{
    if (index < 0 || transport == nullptr) {
        return;
    }
    Entry& entry = *m_entries[static_cast<size_t>(index)];
    if (m_done || entry.ended || m_finished) {
        transport->closeLink(QStringLiteral("not needed"));
        transport->deleteLater();
        return;
    }
    transport->setParent(this);
    entry.transport = transport;
    connect(transport, &SessionTransport::textReceived, this,
            [this, index](const QByteArray& wire) { onHello(index, wire); });
    connect(transport, &SessionTransport::closed, this, [this, index] {
        Entry& e = *m_entries[static_cast<size_t>(index)];
        if (!e.ended) {
            endRung(index, e.hello.isEmpty() ? Outcome::NoAnswer : Outcome::Failed, QString());
        } else if (e.standby) {
            // A standby the Core closed (its connect deadline): gone.
            // LINK minor 10: released (deleted), not only forgotten.
            e.standby = false;
            releaseTransport(e, /*close=*/false);
        }
    });
    entry.helloTimer = new QTimer(this);
    entry.helloTimer->setSingleShot(true);
    connect(entry.helloTimer, &QTimer::timeout, this, [this, index] {
        endRung(index, Outcome::TimedOut, QString());
    });
    entry.helloTimer->start(kHelloDeadlineMs);
}

void PathRacer::onHello(int index, const QByteArray& wire)
{
    Entry& entry = *m_entries[static_cast<size_t>(index)];
    if (entry.ended || !entry.hello.isEmpty() || entry.transport.isNull()) {
        return;
    }
    if (entry.helloTimer != nullptr) {
        entry.helloTimer->stop();
    }
    SessionMessage message;
    if (!SessionMessages::decode(wire, &message) || message.kind != SessionMessageKind::Hello) {
        // The Core speaks first, with its hello; anything else did not
        // come from a Core (a refusal, session.end, comes from a full
        // one, and ends this rung the same way).
        endRung(index, Outcome::Failed, QString());
        return;
    }
    if (m_vetter && !m_vetter(message, entry.transport)) {
        endRung(index, Outcome::NotThisCore, QString());
        return;
    }
    entry.hello = wire;
    // Re-review: the turn is kept past the hello. The Core counts a
    // connection against its handshakes per address until it is signed in
    // (snapshot.complete, finish()) or let go (releaseTransport()).
    const int rank = entry.rung ? entry.rung->rank() : Floor;
    const PathKind kind = entry.rung ? entry.rung->kind() : PathKind::Direct;
    Line& line = m_lines[entry.line];
    line.kind = kind;
    // The rank is only known now for the rendezvous (relayed or not).
    if (m_betterThan && rank >= *m_betterThan) {
        endRung(index, Outcome::Stopped, QString());
        return;
    }
    entry.ended = true;
    entry.outcome = Outcome::Ready;
    line.outcome = Outcome::Ready;
    disconnect(entry.transport, &SessionTransport::textReceived, this, nullptr);
    emit linesChanged();
    if (!m_winnerRank) {
        m_winnerRank = rank;
        Ready ready;
        ready.transport = entry.transport;
        ready.hello = wire;
        ready.rank = rank;
        ready.kind = kind;
        ready.address = line.address;
        if (const auto* direct = qobject_cast<const DirectPathRung*>(entry.rung.data())) {
            ready.url = direct->url();
        }
        // Handed over: the session's from here.
        disconnect(entry.transport, nullptr, this, nullptr);
        entry.transport->setParent(nullptr);
        entry.transport = nullptr;
        qCInfo(lcPathRacer) << "Reached the Core at" << ready.address << "rank" << rank;
        emit won(ready);
        return;
    }
    if (rank < *m_winnerRank && !m_finished) {
        // Better than the winner: kept for the session to move to. Only
        // the best is kept.
        for (auto& other : m_entries) {
            if (other.get() != &entry && other->standby && other->rung
                && other->rung->rank() <= rank) {
                releaseTransport(entry, /*close=*/true);
                line.outcome = Outcome::Stopped;
                emit linesChanged();
                return;
            }
        }
        for (auto& other : m_entries) {
            if (other.get() != &entry && other->standby) {
                other->standby = false;
                releaseTransport(*other, /*close=*/true);
                m_lines[other->line].outcome = Outcome::Stopped;
            }
        }
        entry.standby = true;
        qCInfo(lcPathRacer) << "A better path to the Core is ready at" << line.address;
        emit better();
        return;
    }
    releaseTransport(entry, /*close=*/true);
    line.outcome = Outcome::Stopped;
    emit linesChanged();
}

void PathRacer::releaseTransport(Entry& entry, bool close)
{
    releaseDirectTurn(entry);
    SessionTransport* transport = entry.transport;
    entry.transport = nullptr;
    if (transport == nullptr) {
        return;
    }
    disconnect(transport, nullptr, this, nullptr);
    if (close) {
        transport->closeLink(QStringLiteral("another path connected first"));
    }
    transport->deleteLater();
}

void PathRacer::endRung(int index, Outcome outcome, const QString& reason)
{
    if (index < 0) {
        return;
    }
    Entry& entry = *m_entries[static_cast<size_t>(index)];
    if (entry.ended) {
        return;
    }
    entry.ended = true;
    entry.outcome = outcome;
    releaseDirectTurn(entry);
    if (entry.startTimer != nullptr) {
        entry.startTimer->stop();
    }
    if (entry.helloTimer != nullptr) {
        entry.helloTimer->stop();
    }
    if (entry.rung != nullptr) {
        disconnect(entry.rung, nullptr, this, nullptr);
        entry.rung->stop();
    }
    releaseTransport(entry, /*close=*/true);
    Line& line = m_lines[entry.line];
    if (entry.rung != nullptr) {
        line.kind = entry.rung->kind();
    }
    line.outcome = outcome;
    line.reason = reason;
    emit linesChanged();
    checkAllEnded();
}

void PathRacer::checkAllEnded()
{
    if (m_done || !m_started || m_winnerRank || m_lookupsRunning > 0) {
        return;
    }
    for (const auto& entry : m_entries) {
        if (!entry->ended) {
            return;
        }
    }
    m_done = true;
    // The most telling words any rung left (the service's, a Core too old
    // for it), else the general ones.
    QString reason;
    for (const Line& line : std::as_const(m_lines)) {
        if (!line.reason.isEmpty() && line.outcome != Outcome::RelayOff) {
            reason = line.reason;
            if (line.outcome == Outcome::CoreTooOld) {
                break;
            }
        }
    }
    if (reason.isEmpty()) {
        reason = QString::fromLatin1(kNoPath);
    }
    qCInfo(lcPathRacer) << "No path reached the Core";
    emit failed(reason);
}

void PathRacer::finish()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    for (size_t i = 0; i < m_entries.size(); ++i) {
        Entry& entry = *m_entries[i];
        if (!entry.ended) {
            endRung(static_cast<int>(i), Outcome::Stopped, QString());
        }
    }
    m_done = true;
    // The winner is signed in and the standby taken or let go: every turn
    // they held is free.
    for (auto& entry : m_entries) {
        if (entry->opening) {
            entry->opening = false;
            --m_directOpening;
        }
    }
}

std::optional<PathRacer::Ready> PathRacer::takeStandby()
{
    for (auto& entry : m_entries) {
        if (!entry->standby || entry->transport.isNull()) {
            continue;
        }
        entry->standby = false;
        Ready ready;
        ready.transport = entry->transport;
        ready.hello = entry->hello;
        ready.rank = entry->rung ? entry->rung->rank() : Floor;
        ready.kind = m_lines[entry->line].kind;
        ready.address = m_lines[entry->line].address;
        if (const auto* direct = qobject_cast<const DirectPathRung*>(entry->rung.data())) {
            ready.url = direct->url();
        }
        disconnect(entry->transport, nullptr, this, nullptr);
        entry->transport->setParent(nullptr);
        entry->transport = nullptr;
        return ready;
    }
    return std::nullopt;
}

QList<QPair<QString, int>> PathRacer::plannedStartsForTest() const
{
    QList<QPair<QString, int>> planned;
    for (const auto& entry : m_entries) {
        planned.append({m_lines.value(entry->line).address, entry->startDelayMs});
    }
    return planned;
}

void PathRacer::cancel()
{
    if (m_done && m_finished) {
        return;
    }
    m_done = true;
    m_finished = true;
    for (auto& entry : m_entries) {
        if (entry->startTimer != nullptr) {
            entry->startTimer->stop();
        }
        if (entry->helloTimer != nullptr) {
            entry->helloTimer->stop();
        }
        if (entry->rung != nullptr) {
            disconnect(entry->rung, nullptr, this, nullptr);
            entry->rung->stop();
        }
        entry->standby = false;
        releaseTransport(*entry, /*close=*/true);
        entry->ended = true;
    }
}

// ── The Core's WebSocket at one address ───────────────────────────────────

DirectPathRung::DirectPathRung(const QUrl& url, quint64 maxIncomingBytes, QObject* parent)
    : PathRung(parent)
    , m_url(url)
    , m_maxIncomingBytes(maxIncomingBytes)
    , m_rank(PathRacer::rankFor(url))
{
}

DirectPathRung::~DirectPathRung()
{
    stop();
}

PathRacer::PathKind DirectPathRung::kind() const
{
    return m_rank == PathRacer::ThisNetwork ? PathRacer::PathKind::ThisNetwork
                                            : PathRacer::PathKind::Direct;
}

QString DirectPathRung::address() const
{
    return hostPort(m_url);
}

void DirectPathRung::start()
{
    if (m_transport || m_done) {
        return;
    }
    // A paired Core is proved at its hello (link section 21.1): the only
    // secure scheme carries the certificate the binding is checked against.
    if (m_url.scheme().compare(QLatin1String("wss"), Qt::CaseInsensitive) != 0) {
        m_done = true;
        emit ended(PathRacer::Outcome::Failed, QString());
        return;
    }
    auto* socket = new QWebSocket();
    auto* transport = new WebSocketTransport(socket, m_maxIncomingBytes, this);
    m_transport = transport;
    connect(socket, &QWebSocket::sslErrors, this, [socket](const QList<QSslError>& errors) {
        // The identity key and certificate binding prove the Core, at its
        // hello; the chain does not (a self-signed certificate). Validity
        // dates still count, as StationClient::dialStation() keeps them.
        QList<QSslError> ignorable;
        for (const QSslError& error : errors) {
            if (error.error() == QSslError::CertificateExpired
                || error.error() == QSslError::CertificateNotYetValid) {
                continue;
            }
            ignorable.append(error);
        }
        socket->ignoreSslErrors(ignorable);
    });
    connect(socket, &QWebSocket::connected, this, [this, transport] {
        if (m_done || m_transport != transport) {
            return;
        }
        m_done = true;
        disconnect(transport->socket(), nullptr, this, nullptr);
        transport->setParent(nullptr);
        m_transport = nullptr;
        emit opened(transport);
    });
    // A proxy that demands a login: NereusSDR gives it none, so the socket
    // fails next; the rung's words say why.
    auto proxyLogin = std::make_shared<bool>(false);
    connect(socket, &QWebSocket::proxyAuthenticationRequired, this,
            [proxyLogin](const QNetworkProxy&, QAuthenticator*) { *proxyLogin = true; });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(socket, &QWebSocket::errorOccurred, this,
#else
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
#endif
            [this, transport, proxyLogin](QAbstractSocket::SocketError error) {
        if (m_done || m_transport != transport) {
            return;
        }
        m_done = true;
        m_transport = nullptr;
        transport->deleteLater();
        // Empty but for a proxy that demands a login, which says so.
        emit ended(PathRacer::Outcome::NoAnswer,
                   *proxyLogin ? NetworkTrouble::proxyNeedsLoginWords()
                               : NetworkTrouble::wordsForSocketError(error));
    });
    // Step 2b: the computer's own proxy settings (SystemProxy).
    socket->setProxy(SystemProxy::forUrl(m_url));
    socket->open(m_url);
}

void DirectPathRung::stop()
{
    m_done = true;
    if (m_transport) {
        SessionTransport* transport = m_transport;
        m_transport = nullptr;
        if (auto* ws = qobject_cast<WebSocketTransport*>(transport)) {
            disconnect(ws->socket(), nullptr, this, nullptr);
            ws->socket()->abort();
        }
        transport->deleteLater();
    }
}

// ── The rendezvous ────────────────────────────────────────────────────────

RendezvousPathRung::RendezvousPathRung(const QList<QUrl>& servers, const QString& stationId,
                                       std::shared_ptr<const ClientDeviceIdentity> device,
                                       bool allowRelay, QObject* parent)
    : PathRung(parent)
    , m_servers(servers)
    , m_stationId(stationId)
    , m_device(std::move(device))
    , m_allowRelay(allowRelay)
{
}

RendezvousPathRung::~RendezvousPathRung()
{
    stop();
}

PathRacer::PathKind RendezvousPathRung::kind() const
{
    if (m_rank == PathRacer::Floor) {
        return PathRacer::PathKind::WebRelay;
    }
    return m_rank == PathRacer::ServiceRelayed ? PathRacer::PathKind::Relay
                                               : PathRacer::PathKind::Service;
}

QString RendezvousPathRung::address() const
{
    return m_servers.isEmpty() ? QString() : m_servers.first().host();
}

void RendezvousPathRung::start()
{
    if (m_dialer || m_done) {
        return;
    }
    auto* dialer = new RendezvousDialer(this);
    m_dialer = dialer;
    dialer->setAllowRelay(m_allowRelay);
    dialer->setCoreAnswersIntroductions(m_coreAnswersIntroductions);
    if (m_dialDeadlineMs > 0) {
        dialer->setDialDeadlineMs(m_dialDeadlineMs);
    }
    if (m_answerDeadlineMs > 0) {
        dialer->setAnswerDeadlineMs(m_answerDeadlineMs);
    }
    connect(dialer, &RendezvousDialer::ready, this, [this, dialer](DataChannelTransport* transport) {
        if (m_done || m_dialer != dialer) {
            transport->closeLink(QStringLiteral("not needed"));
            transport->deleteLater();
            return;
        }
        m_done = true;
        const std::optional<MediaIcePath> path = transport->selectedPath();
        m_rank = PathRacer::rankForPath(path);
        m_dialer = nullptr;
        dialer->deleteLater();
        noteRelay(dialer);
        emit opened(transport);
    });
    connect(dialer, &RendezvousDialer::failed, this, [this, dialer](const QString& reason) {
        if (m_done || m_dialer != dialer) {
            return;
        }
        m_done = true;
        const PathRacer::Outcome outcome = dialer->coreTooOld()
            ? PathRacer::Outcome::CoreTooOld
            : PathRacer::Outcome::NoAnswer;
        m_dialer = nullptr;
        dialer->deleteLater();
        noteRelay(dialer);
        emit ended(outcome, reason);
    });
    // Step 2b: the web relay ending this attempt's leg is a line of its own.
    connect(dialer, &RendezvousDialer::webRelayEnded, this,
            [this](const QString&, const QString& words) {
        if (!words.isEmpty()) {
            emit noted(PathRacer::PathKind::WebRelay, PathRacer::Outcome::WebRelayEnded, words);
        }
    });
    dialer->dial(m_servers, m_stationId, m_device);
}

void RendezvousPathRung::noteRelay(const RendezvousDialer* dialer)
{
    // Review Minor 6: the Core answered without relay credentials though
    // this computer asked for the relay, and (re-review) no relay grant
    // came either: a service with a relay secret and no TURN secret sends
    // a grant when the Core allows the relay (rendezvous sections 10 and
    // 12.1), so without one the Core turned it off.
    if (m_allowRelay && dialer != nullptr && dialer->answered() && !dialer->relayOffered()
        && !dialer->relayGranted()) {
        emit noted(PathRacer::PathKind::Relay, PathRacer::Outcome::RelayOff,
                   QStringLiteral("The Core has the relay turned off."));
    }
}

void RendezvousPathRung::stop()
{
    m_done = true;
    if (m_dialer) {
        RendezvousDialer* dialer = m_dialer;
        m_dialer = nullptr;
        dialer->disconnect(this);
        dialer->cancel();
        dialer->deleteLater();
    }
}

} // namespace NereusSDR
