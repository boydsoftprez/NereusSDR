// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RelayLeg.cpp  (NereusSDR)
// =================================================================
//
// See RelayLeg.h. The wire is docs/architecture/2026-09-23-rendezvous-v1.md,
// section 12.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-04: Qt 6.4 WebSocket error-signal compatibility. J.J. Boyd
//               (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: dropSocket() sends a close frame only on an open
//               WebSocket and aborts one still opening. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: LINK minor 11: a replaced leg drops its socket and says
//               ended(). LINK minor 14: legs are owned from
//               construction, no raw delete. J.J. Boyd (KG4VCF), AI-
//               assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/RelayLeg.h"

#include "core/session/NetworkTrouble.h"
#include "core/session/SystemProxy.h"

#include <QAuthenticator>
#include <QAbstractSocket>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QStandardPaths>
#include <QTimer>
#include <QUdpSocket>
#include <QUuid>
#include <QWebSocket>
#include <QThread>

#include <algorithm>

Q_LOGGING_CATEGORY(lcRelayLeg, "nereus.session.relayleg")

namespace NereusSDR {

namespace {

bool isLoopbackHost(const QString& host)
{
    const QHostAddress address(host);
    return host == QLatin1String("localhost") || (!address.isNull() && address.isLoopback());
}

bool isLetters(const QByteArray& bytes)
{
    if (bytes.isEmpty() || bytes.size() > 64) {
        return false;
    }
    return std::all_of(bytes.cbegin(), bytes.cend(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    });
}

} // namespace

// One ICE connection's use of a lane (IceConfiguration::CandidateSource).
class RelayLaneSource final : public IceConfiguration::CandidateSource {
public:
    RelayLaneSource(RelayLeg* leg, int lane, QByteArray id = {})
        : m_ownerThread(leg ? leg->thread() : nullptr), m_leg(leg), m_lane(lane), m_id(std::move(id)) {}
    ~RelayLaneSource() override { stop(); }

    void start(std::function<void(const QString&)> add) override
    {
        if (m_leg.isNull() || m_started) {
            return;
        }
        quint16 port = m_id.isEmpty() ? m_leg->lanePort(m_lane) : 0;
        const quint64 claim = m_id.isEmpty() ? m_leg->claimLane(m_lane)
                                              : m_leg->claimRoute(m_id, port);
        if (port == 0 || claim == 0) {
            return;
        }
        m_started = true;
        m_claim = claim;
        add(RelayLeg::candidateLine(m_lane, port));
    }

    void stop() override
    {
        if (m_started && !m_leg.isNull()) {
            if (m_id.isEmpty()) {
                m_leg->releaseLane(m_lane, m_claim);
            } else {
                m_leg->releaseRoute(m_id, m_claim);
            }
        }
        m_started = false;
    }

    std::optional<NetworkPathSnapshot> networkPathSnapshot() const override
    {
        if (m_ownerThread != QThread::currentThread() || !m_started || m_claim == 0 || !m_leg
            || m_leg->m_state != RelayLeg::State::Joined
            || !m_leg->m_peerPresent || m_leg->m_endSeen) {
            return std::nullopt;
        }
        return socketNetworkPathSnapshot(m_leg->m_socket,
                                         NetworkPathSnapshot::Carrier::WebRelay);
    }

private:
    QThread* m_ownerThread = nullptr;
    QPointer<RelayLeg> m_leg;
    int m_lane = 0;
    bool m_started = false;
    quint64 m_claim = 0;
    QByteArray m_id;
};

RelayLeg::RelayLeg(QObject* parent)
    : QObject(parent)
{
    m_connectTimer = new QTimer(this);
    m_connectTimer->setSingleShot(true);
    connect(m_connectTimer, &QTimer::timeout, this, &RelayLeg::connectNow);
}

RelayLeg::RelayLeg(Purpose purpose)
    : RelayLeg(static_cast<QObject*>(nullptr))
{
    m_purpose = purpose;
}

RelayLeg::~RelayLeg()
{
    m_state = State::Ended;
    dropSocket();
}

std::shared_ptr<RelayLeg> RelayLeg::create()
{
    // LINK minor 14: owned from the start (the constructor is private, so
    // no make_shared); a leg whose lanes do not bind goes with its owner.
    std::shared_ptr<RelayLeg> leg(new RelayLeg(), [](RelayLeg* gone) {
        gone->close();
        gone->deleteLater();
    });
    if (!leg->bindLanes()) {
        return nullptr;
    }
    return leg;
}

std::shared_ptr<RelayLeg> RelayLeg::createWatch()
{
    // LINK minor 14: owned from the start (the constructor is private, so
    // no make_shared); a leg whose lanes do not bind goes with its owner.
    std::shared_ptr<RelayLeg> leg(new RelayLeg(Purpose::Watch), [](RelayLeg* gone) {
        gone->close();
        gone->deleteLater();
    });
    if (!leg->bindLanes()) {
        return nullptr;
    }
    return leg;
}

IceConfiguration::CandidateSourceFactory RelayLeg::factoryFor(std::shared_ptr<RelayLeg> leg)
{
    return [leg](int lane, const QString& connectionId,
                 bool routed) -> std::shared_ptr<IceConfiguration::CandidateSource> {
        return leg ? leg->sourceFor(lane, connectionId, routed) : nullptr;
    };
}

bool RelayLeg::bindLanes()
{
    const size_t count = m_purpose == Purpose::Watch ? 1 : m_lanes.size();
    for (size_t i = 0; i < count; ++i) {
        Lane& lane = m_lanes[i];
        if (lane.socket != nullptr) {
            continue;
        }
        auto* socket = new QUdpSocket(this);
        if (!socket->bind(QHostAddress::LocalHost, 0)) {
            qCWarning(lcRelayLeg) << "The web relay's local socket could not be opened:"
                                  << socket->errorString();
            delete socket;
            return false;
        }
        lane.socket = socket;
        const int laneNumber = static_cast<int>(i) + 1;
        connect(socket, &QUdpSocket::readyRead, this, [this, laneNumber] { readLane(laneNumber); });
    }
    return true;
}

quint16 RelayLeg::lanePort(int lane) const
{
    if (lane < 1 || lane > static_cast<int>(m_lanes.size())
        || (m_purpose == Purpose::Watch && lane != kTagControl)) {
        return 0;
    }
    const Lane& entry = m_lanes[static_cast<size_t>(lane - 1)];
    return entry.socket != nullptr ? entry.socket->localPort() : 0;
}

QString RelayLeg::candidateLine(int lane, quint16 port)
{
    // A host candidate at the lane socket, with the lowest priority: every
    // pair it makes is below every other, so ICE takes it last.
    return QStringLiteral("candidate:wsrelay%1 1 UDP %2 127.0.0.1 %3 typ host")
        .arg(lane)
        .arg(kCandidatePriority)
        .arg(port);
}

std::shared_ptr<IceConfiguration::CandidateSource> RelayLeg::sourceFor(
    int lane, const QString& connectionId, bool routed)
{
    if (m_purpose == Purpose::Watch
        && (lane != kTagControl || routed || !connectionId.isEmpty())) {
        return nullptr;
    }
    if (lane < 1 || lane > static_cast<int>(m_lanes.size())) {
        return nullptr;
    }
    if (lane == kTagMedia && routed) {
        const QUuid uuid = QUuid::fromString(connectionId);
        if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != connectionId) {
            return nullptr;
        }
        return std::make_shared<RelayLaneSource>(this, lane, uuid.toRfc4122());
    }
    return std::make_shared<RelayLaneSource>(this, lane);
}

quint64 RelayLeg::claimRoute(const QByteArray& id, quint16& port)
{
    if (m_purpose == Purpose::Watch || id.size() != 16 || m_routes.contains(id) || m_routes.size() >= 3
        || (m_mediaModeChosen && !m_mediaRouted)) {
        return 0;
    }
    auto* socket = new QUdpSocket(this);
    if (!socket->bind(QHostAddress::LocalHost, 0)) {
        delete socket;
        return 0;
    }
    port = socket->localPort();
    const quint64 claim = m_nextClaim++;
    m_routes.insert(id, Route{socket, {}, 0, claim});
    m_mediaModeChosen = true;
    m_mediaRouted = true;
    connect(socket, &QUdpSocket::readyRead, this, [this, id] { readRoute(id); });
    return claim;
}

void RelayLeg::releaseRoute(const QByteArray& id, quint64 claim)
{
    auto it = m_routes.find(id);
    if (it == m_routes.end() || it->claim != claim) {
        return;
    }
    it->claim = 0;
    it->agentAddress.clear();
    it->agentPort = 0;
    Lane& media = m_lanes[static_cast<size_t>(kTagMedia - 1)];
    std::erase_if(media.queue, [&id](const QByteArray& frame) {
        return frame.size() >= 17 && frame.mid(1, 16) == id;
    });
    media.queuedBytes = 0;
    for (const QByteArray& frame : media.queue) {
        media.queuedBytes += static_cast<int>(frame.size());
    }
    QUdpSocket* const socket = it->socket;
    socket->disconnect(this);
    socket->close();
    socket->deleteLater();
    m_routes.erase(it);
}

void RelayLeg::readRoute(const QByteArray& id)
{
    auto it = m_routes.find(id);
    if (it == m_routes.end()) {
        return;
    }
    QUdpSocket* socket = it->socket;
    while (socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = socket->receiveDatagram(kMaxDatagramBytes + 1);
        if (it->claim == 0) {
            continue;
        }
        const QByteArray payload = datagram.data();
        if (!datagram.senderAddress().isLoopback() || payload.isEmpty()) {
            continue;
        }
        if (payload.size() > kMaxDatagramBytes - 16) {
            ++m_droppedOversize;
            continue;
        }
        if (m_state == State::Idle || m_state == State::Ended) {
            continue;
        }
        const QHostAddress sender = datagram.senderAddress();
        const quint16 senderPort = static_cast<quint16>(datagram.senderPort());
        if (it->agentPort != 0
            && (it->agentAddress != sender || it->agentPort != senderPort)) {
            ++m_droppedWrongSender;
            continue;
        }
        if (it->agentPort == 0) {
            it->agentAddress = sender;
            it->agentPort = senderPort;
        }
        QByteArray frame;
        frame.reserve(1 + 16 + payload.size());
        frame.append(static_cast<char>(kTagMedia));
        frame.append(id);
        frame.append(payload);
        enqueue(kTagMedia, frame);
    }
    flush();
}

RelayLeg::Lane* RelayLeg::laneFor(int lane)
{
    if (lane < 1 || lane > static_cast<int>(m_lanes.size())) {
        return nullptr;
    }
    return &m_lanes[static_cast<size_t>(lane - 1)];
}

quint64 RelayLeg::claimLane(int lane)
{
    if (m_purpose == Purpose::Watch && lane != kTagControl) {
        return 0;
    }
    Lane* entry = laneFor(lane);
    if (entry == nullptr || entry->claim != 0
        || (lane == kTagMedia && m_mediaModeChosen && m_mediaRouted)) {
        return 0;
    }
    if (lane == kTagMedia) {
        m_mediaModeChosen = true;
    }
    // A new connection on this lane: its agent is learned afresh.
    entry->claim = m_nextClaim++;
    entry->agentAddress.clear();
    entry->agentPort = 0;
    return entry->claim;
}

void RelayLeg::releaseLane(int lane, quint64 claim)
{
    Lane* entry = laneFor(lane);
    if (entry == nullptr || entry->claim != claim) {
        return;
    }
    entry->claim = 0;
    entry->agentAddress.clear();
    entry->agentPort = 0;
    entry->queue.clear();
    entry->queuedBytes = 0;
}

void RelayLeg::setAgentForTest(int lane, const QHostAddress& address, quint16 port)
{
    if (m_purpose == Purpose::Watch && lane != kTagControl) {
        return;
    }
    if (Lane* entry = laneFor(lane)) {
        entry->agentAddress = address;
        entry->agentPort = port;
    }
}

namespace {
QUrl& relayUrlForTest()
{
    static QUrl url;
    return url;
}
} // namespace

void RelayLeg::setRelayUrlForTest(const QUrl& url)
{
    relayUrlForTest() = url;
}

void RelayLeg::open(const QUrl& grantUrl, const QString& token)
{
    const QUrl url = relayUrlForTest().isEmpty() ? grantUrl : relayUrlForTest();
    if (m_state != State::Idle || token.isEmpty() || !url.isValid()) {
        return;
    }
    const bool secure = url.scheme() == QLatin1String("wss");
    // Plain ws:// only to this computer (the conformance runner).
    if (!secure && !(url.scheme() == QLatin1String("ws") && isLoopbackHost(url.host()))) {
        qCInfo(lcRelayLeg) << "The web relay's address is not usable";
        return;
    }
    // A test run never reaches a relay off this computer (as
    // RendezvousClient).
    if (QStandardPaths::isTestModeEnabled() && !isLoopbackHost(url.host())) {
        qCInfo(lcRelayLeg) << "Test run: not contacting a web relay off this computer";
        return;
    }
    m_url = url;
    m_token = token;
    m_state = State::Connecting;
    connectNow();
}

void RelayLeg::close()
{
    m_connectTimer->stop();
    m_state = State::Ended;
    dropSocket();
}

void RelayLeg::dropSocket()
{
    if (m_socket.isNull()) {
        return;
    }
    QWebSocket* socket = m_socket.data();
    m_socket = nullptr;
    socket->disconnect(this);
    // A close frame belongs only on an open WebSocket (RFC 6455 section
    // 7.1.2). Still opening (TCP connecting, or the upgrade unanswered),
    // close() would write one anyway: into the upgrade exchange, or into a
    // TCP socket not yet connected (QNativeSocketEngine's "write() was not
    // called in QAbstractSocket::ConnectedState"). Abort that instead, as
    // StationClient::onHandshakeDeadline does for its own socket.
    if (socket->state() == QAbstractSocket::ConnectedState) {
        socket->close();
    } else {
        socket->abort();
    }
    socket->deleteLater();
}

void RelayLeg::scheduleConnect(int delayMs)
{
    m_state = State::Waiting;
    m_connectTimer->start(std::max(0, delayMs));
}

void RelayLeg::connectNow()
{
    if (m_state == State::Ended || m_token.isEmpty()) {
        return;
    }
    dropSocket();
    m_state = State::Connecting;
    m_endSeen = false;
    m_afterClose = AfterClose::Rejoin;
    auto* socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    socket->setMaxAllowedIncomingMessageSize(kMaxDatagramBytes + 1 + 64);
    socket->setMaxAllowedIncomingFrameSize(kMaxDatagramBytes + 1 + 64);
    socket->setProxy(SystemProxy::forUrl(m_url));
    m_socket = socket;
    ++m_connections;
    connect(socket, &QWebSocket::connected, this, &RelayLeg::onConnected);
    connect(socket, &QWebSocket::disconnected, this, &RelayLeg::onDisconnected);
    connect(socket, &QWebSocket::binaryMessageReceived, this, &RelayLeg::onMessage);
    // Options survey B.6 and B.7: the reason in plain words when the leg
    // gives up (the relay is not pinned; the system's trust decides).
    connect(socket, &QWebSocket::sslErrors, this, [this](const QList<QSslError>& errors) {
        const QString words = NetworkTrouble::wordsForTlsErrors(errors);
        if (!words.isEmpty()) {
            qCWarning(lcRelayLeg).noquote() << words;
            m_networkTrouble = words;
        }
    });
    // A proxy that demands a login: said plainly when the leg gives up.
    connect(socket, &QWebSocket::proxyAuthenticationRequired, this,
            [this](const QNetworkProxy&, QAuthenticator*) {
        qCWarning(lcRelayLeg).noquote() << NetworkTrouble::proxyNeedsLoginWords();
        m_networkTrouble = NetworkTrouble::proxyNeedsLoginWords();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(socket, &QWebSocket::errorOccurred, this,
#else
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
#endif
            [this](QAbstractSocket::SocketError error) {
        const QString words = NetworkTrouble::wordsForSocketError(error);
        if (!words.isEmpty()) {
            qCWarning(lcRelayLeg).noquote() << words;
            m_networkTrouble = words;
        }
    });
    connect(socket, &QWebSocket::bytesWritten, this, [this](qint64) { flush(); });
    socket->open(m_url);
}

void RelayLeg::onConnected()
{
    if (m_socket.isNull()) {
        return;
    }
    // Section 12.5: no Nagle on the leg.
    if (auto* inner = m_socket->findChild<QAbstractSocket*>()) {
        inner->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    }
    // JOIN first (section 12.3), datagrams right after it.
    QByteArray join;
    join.reserve(1 + m_token.size());
    join.append(static_cast<char>(kTagJoin));
    join.append(m_token.toLatin1());
    m_socket->sendBinaryMessage(join);
    qCInfo(lcRelayLeg) << "Joining the web relay";
    flush();
}

void RelayLeg::onDisconnected()
{
    if (m_state == State::Ended) {
        return;
    }
    dropSocket();
    m_peerPresent = false;
    if (m_endSeen) {
        switch (m_afterClose) {
        case AfterClose::Stop:
            return; // finish() already said why
        case AfterClose::RetryShortly:
            if (!m_sinceRunning) {
                m_since.start();
                m_sinceRunning = true;
            }
            if (m_since.elapsed() >= kRetryShortlyForMs) {
                finish(QStringLiteral("lost"));
                return;
            }
            scheduleConnect(kRetryShortlyMs);
            return;
        case AfterClose::Rejoin:
            break;
        }
    }
    // A close without an END (or an END that says join again): join again
    // with the same token within the window, the loopback sockets kept.
    if (!m_sinceRunning) {
        m_since.start();
        m_sinceRunning = true;
        m_rejoinDelayMs = 0;
    }
    if (m_since.elapsed() >= kRejoinWindowMs) {
        finish(QStringLiteral("lost"));
        return;
    }
    qCInfo(lcRelayLeg) << "The web relay's connection closed; joining again";
    scheduleConnect(m_rejoinDelayMs);
    m_rejoinDelayMs = m_rejoinDelayMs == 0 ? 500 : std::min(m_rejoinDelayMs * 2, 4000);
}

void RelayLeg::onMessage(const QByteArray& message)
{
    if (m_endSeen || message.isEmpty()) {
        return;
    }
    const auto tag = static_cast<quint8>(message.at(0));
    if ((m_purpose == Purpose::Primary && (tag == kTagControl || tag == kTagMedia))
        || (m_purpose == Purpose::Watch && tag == kTagWatch)) {
        if (message.size() > kMaxDatagramBytes + 1) {
            ++m_droppedOversize;
            return;
        }
        const QByteArray payload = message.mid(1);
        if (tag == kTagMedia && m_mediaRouted) {
            if (payload.size() <= 16) {
                ++m_droppedNoAgent;
                return;
            }
            const auto it = m_routes.constFind(payload.left(16));
            if (it == m_routes.cend() || it->claim == 0 || it->agentPort == 0) {
                ++m_droppedNoAgent;
                return;
            }
            it->socket->writeDatagram(payload.constData() + 16, payload.size() - 16,
                                      it->agentAddress, it->agentPort);
            ++m_delivered;
            return;
        }
        Lane* lane = laneFor(tag == kTagWatch ? kTagControl : tag);
        if (payload.isEmpty() || payload.size() > kMaxDatagramBytes
            || lane == nullptr || lane->socket == nullptr) {
            if (tag == kTagMedia && m_mediaRouted) {
                ++m_droppedNoAgent;
            }
            return;
        }
        if (lane->agentPort == 0) {
            ++m_droppedNoAgent;
            return;
        }
        lane->socket->writeDatagram(payload, lane->agentAddress, lane->agentPort);
        ++m_delivered;
        return;
    }
    if (tag < kTagJoin) {
        // 0x00 and the data tags this end does not know: dropped.
        ++m_droppedUnknownTag;
        return;
    }
    switch (tag) {
    case kTagReady: {
        // Two bytes after the tag (version, other leg present); more are
        // ignored, and a later version is read as version 1's.
        const bool present = message.size() > 2 && static_cast<quint8>(message.at(2)) == 1;
        m_state = State::Joined;
        m_peerPresent = present;
        m_sinceRunning = false;
        m_rejoinDelayMs = 0;
        qCInfo(lcRelayLeg) << "Joined the web relay" << (present ? "(the other end is there)" : "");
        emit joined(present);
        flush();
        return;
    }
    case kTagPeer: {
        const bool present = message.size() > 1 && static_cast<quint8>(message.at(1)) == 1;
        if (present != m_peerPresent) {
            m_peerPresent = present;
            emit peerChanged(present);
        }
        return;
    }
    case kTagEnd: {
        const QByteArray code = message.mid(1);
        onEnd(isLetters(code) ? QString::fromLatin1(code) : QStringLiteral("unknown"));
        return;
    }
    default:
        // JOIN from the relay, and 0x84 up: ignored.
        return;
    }
}

void RelayLeg::onEnd(const QString& code)
{
    m_endSeen = true;
    qCInfo(lcRelayLeg).noquote() << QStringLiteral("The web relay ended the leg: %1").arg(code);
    if (code == QLatin1String("timeout") || code == QLatin1String("shuttingDown")) {
        m_afterClose = AfterClose::Rejoin;
        if (!m_sinceRunning) {
            m_since.start();
            m_sinceRunning = true;
            m_rejoinDelayMs = code == QLatin1String("shuttingDown") ? 1000 : 0;
        }
        return;
    }
    if (code == QLatin1String("full") || code == QLatin1String("tooManyConnections")
        || code == QLatin1String("tooManySessions")) {
        m_afterClose = AfterClose::RetryShortly;
        return;
    }
    if (code == QLatin1String("replaced")) {
        // The newer connection is this leg's own (after a reset this end
        // no longer holds): nothing more here.
        // LINK minor 11: nothing more on the wire, but the leg still ends
        // as every other end does: its timer stopped, its socket dropped,
        // and ended() said (with no words, section 12.4's table) so its
        // owner does not hold a leg that is gone.
        m_afterClose = AfterClose::Stop;
        dropSocket();
        finish(code);
        return;
    }
    m_afterClose = AfterClose::Stop;
    finish(code);
}

void RelayLeg::finish(const QString& code)
{
    m_connectTimer->stop();
    m_state = State::Ended;
    m_endCode = code;
    // A leg lost on a network that breaks its secure connection says so.
    const QString words = code == QLatin1String("lost") && !m_networkTrouble.isEmpty()
        ? m_networkTrouble : wordsFor(code);
    emit ended(code, words);
}

QString RelayLeg::wordsFor(const QString& code)
{
    // Section 12.4's table.
    if (code == QLatin1String("protocolError")) {
        return QStringLiteral("The connection through the web relay failed. Updating the app or "
                              "the Core may help.");
    }
    if (code == QLatin1String("timeout")) {
        return QStringLiteral("The web relay did not answer in time. Trying again.");
    }
    if (code == QLatin1String("badToken")) {
        return QStringLiteral("The web relay did not accept this connection. Try connecting "
                              "again.");
    }
    if (code == QLatin1String("expired")) {
        return QStringLiteral("The web relay's permission ran out. Try connecting again.");
    }
    if (code == QLatin1String("ended")) {
        return QStringLiteral("That relayed connection has ended. Try connecting again.");
    }
    if (code == QLatin1String("full")) {
        return QStringLiteral("The web relay is busy. Trying again shortly.");
    }
    if (code == QLatin1String("tooManyConnections")) {
        return QStringLiteral("Too many connections from this network to the web relay. Trying "
                              "again shortly.");
    }
    if (code == QLatin1String("tooManySessions")) {
        return QStringLiteral("This Core already has as many connections through the web relay as "
                              "it can. Try again shortly.");
    }
    if (code == QLatin1String("peerGone")) {
        return QStringLiteral("The other end left the web relay.");
    }
    if (code == QLatin1String("shuttingDown")) {
        return QStringLiteral("The web relay is restarting. Trying again.");
    }
    if (code == QLatin1String("replaced") || code == QLatin1String("idle")) {
        return QString();
    }
    if (code == QLatin1String("lost")) {
        return QStringLiteral("The connection to the web relay was lost.");
    }
    // An END code this end does not know: the session is over (12.4).
    return QStringLiteral("That relayed connection has ended. Try connecting again.");
}

void RelayLeg::readLane(int lane)
{
    Lane* entry = laneFor(lane);
    if (entry == nullptr || entry->socket == nullptr) {
        return;
    }
    while (entry->socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = entry->socket->receiveDatagram(kMaxDatagramBytes + 1);
        const QHostAddress sender = datagram.senderAddress();
        // Only this computer's own agents write to a lane socket.
        if (!sender.isLoopback()) {
            continue;
        }
        const QByteArray payload = datagram.data();
        if (payload.isEmpty()) {
            continue;
        }
        if (payload.size() > kMaxDatagramBytes) {
            // The relay would close the leg (1009): never sent.
            ++m_droppedOversize;
            continue;
        }
        if (m_state == State::Idle || m_state == State::Ended) {
            continue;
        }
        if (lane == kTagMedia && m_mediaModeChosen && entry->claim == 0) {
            continue;
        }
        const quint16 senderPort = static_cast<quint16>(datagram.senderPort());
        if (entry->agentPort != 0
            && (entry->agentAddress != sender || entry->agentPort != senderPort)) {
            ++m_droppedWrongSender;
            continue;
        }
        if (entry->agentPort == 0) {
            entry->agentAddress = sender;
            entry->agentPort = senderPort;
        }
        QByteArray frame;
        frame.reserve(1 + payload.size());
        frame.append(static_cast<char>(m_purpose == Purpose::Watch ? kTagWatch : lane));
        frame.append(payload);
        enqueue(lane, frame);
    }
    flush();
}

void RelayLeg::enqueue(int lane, const QByteArray& frame)
{
    Lane* entry = laneFor(lane);
    if (entry == nullptr) {
        return;
    }
    entry->queue.push_back(frame);
    entry->queuedBytes += static_cast<int>(frame.size());
    // Bounded, the oldest dropped first (sections 12.5 and 12.9).
    const int frameLimit = m_purpose == Purpose::Watch ? kWatchQueueFrames : kQueueFrames;
    const int byteLimit = m_purpose == Purpose::Watch ? kWatchQueueBytes : kQueueBytes;
    while (static_cast<int>(entry->queue.size()) > frameLimit
           || entry->queuedBytes > byteLimit) {
        entry->queuedBytes -= static_cast<int>(entry->queue.front().size());
        entry->queue.pop_front();
        ++m_droppedQueue;
    }
}

qint64 RelayLeg::socketBacklog() const
{
    if (m_socket.isNull()) {
        return 0;
    }
    if (const auto* inner = m_socket->findChild<QAbstractSocket*>()) {
        return inner->bytesToWrite();
    }
    return 0;
}

void RelayLeg::flush()
{
    if (m_socket.isNull() || m_endSeen || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    while (socketBacklog() < kWriteLimitBytes) {
        // The lanes in turn.
        Lane* chosen = nullptr;
        for (size_t tries = 0; tries < m_lanes.size(); ++tries) {
            Lane& candidate = m_lanes[static_cast<size_t>(m_nextLane)];
            m_nextLane = (m_nextLane + 1) % static_cast<int>(m_lanes.size());
            if (!candidate.queue.empty()) {
                chosen = &candidate;
                break;
            }
        }
        if (chosen == nullptr) {
            return;
        }
        const QByteArray frame = chosen->queue.front();
        chosen->queue.pop_front();
        chosen->queuedBytes -= static_cast<int>(frame.size());
        m_socket->sendBinaryMessage(frame);
        ++m_sent;
    }
}

} // namespace NereusSDR
