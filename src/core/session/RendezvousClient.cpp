// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RendezvousClient.cpp  (NereusSDR)
// =================================================================
//
// See RendezvousClient.h. The wire is
// docs/architecture/2026-09-23-rendezvous-v1.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): retireIntroduction():
//               an introduction the Core has finished with is forgotten at
//               once, and never taken back. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: Task 28 tail (R-IOS-16): setPingIntervalMs(). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: iPhone app plan Task 29 fix wave (R-IOS-16): relay.grant
//               (section 12.1), from the service to a station and a client.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/RendezvousClient.h"
#include "core/session/NetworkTrouble.h"
#include "core/session/SystemProxy.h"

#include "core/security/StationIdentity.h"

#include <QAuthenticator>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTimer>
#include <QWebSocket>

#include <algorithm>

namespace NereusSDR {

namespace {

Q_LOGGING_CATEGORY(lcRendezvous, "nereussdr.rendezvous")

// Reconnect waits of a Core that lost the service: 1, 2, 5, 10, 30 and 60
// seconds, the last repeated. The same ladder the Core uses for its
// accessories, so the service is not hammered after a restart (section
// 9.3: a restart forgets every registration and stations come back).
const QList<int> kDefaultReconnectDelaysMs{1000, 2000, 5000, 10000, 30000, 60000};

// The first six characters of an id, the most a log line names (section
// 9.4's rule, kept on this side too).
QString shortId(const QString& id)
{
    return id.left(6);
}

bool isLoopbackHost(const QString& host)
{
    if (host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QHostAddress address(host);
    return !address.isNull() && address.isLoopback();
}

// Local codes (letters only, like the service's) for an end this side saw
// first: its own connection to the service went.
constexpr auto kLostCode = "lost";

} // namespace

RendezvousClient::RendezvousClient(QObject* parent)
    : QObject(parent)
    , m_reconnectDelaysMs(kDefaultReconnectDelaysMs)
{
    qRegisterMetaType<NereusSDR::RendezvousIntroduction>();
    qRegisterMetaType<NereusSDR::RendezvousWire::Turn>();
    m_helloTimer = new QTimer(this);
    m_helloTimer->setSingleShot(true);
    connect(m_helloTimer, &QTimer::timeout, this, &RendezvousClient::onHelloTimeout);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this] { connectTo(0); });
    m_pingTimer = new QTimer(this);
    m_pingTimer->setInterval(kPingIntervalMs);
    connect(m_pingTimer, &QTimer::timeout, this, &RendezvousClient::onPingTick);
}

RendezvousClient::~RendezvousClient()
{
    m_stopped = true;
    resetConnection();
}

QList<QUrl> RendezvousClient::serverUrls(const QStringList& entries, QStringList* rejected)
{
    QList<QUrl> urls;
    for (const QString& raw : entries) {
        const QString entry = raw.trimmed();
        if (entry.isEmpty()) {
            continue;
        }
        QUrl url;
        if (entry.contains(QLatin1String("://"))) {
            url = QUrl(entry, QUrl::StrictMode);
        } else {
            url = QUrl(QStringLiteral("wss://") + entry + QLatin1Char('/'), QUrl::StrictMode);
        }
        const bool secure = url.scheme() == QLatin1String("wss");
        const bool plainLoopback = url.scheme() == QLatin1String("ws") && isLoopbackHost(url.host());
        if (!url.isValid() || url.host().isEmpty() || (!secure && !plainLoopback)
            || !url.userInfo().isEmpty()) {
            if (rejected != nullptr) {
                rejected->append(entry);
            }
            continue;
        }
        if (url.path().isEmpty()) {
            url.setPath(QStringLiteral("/"));
        }
        if (!urls.contains(url)) {
            urls.append(url);
        }
    }
    return urls;
}

void RendezvousClient::setServers(const QList<QUrl>& servers)
{
    m_servers = servers;
}

void RendezvousClient::setReconnectDelaysMs(const QList<int>& delays)
{
    m_reconnectDelaysMs = delays.isEmpty() ? kDefaultReconnectDelaysMs : delays;
}

void RendezvousClient::setHelloTimeoutMs(int ms)
{
    m_helloTimeoutMs = std::max(1, ms);
}

void RendezvousClient::setPingIntervalMs(int ms)
{
    m_pingTimer->setInterval(std::max(1, ms));
}

int RendezvousClient::pingIntervalMs() const
{
    return m_pingTimer->interval();
}

QUrl RendezvousClient::currentServer() const
{
    if (!m_helloReceived || m_serverIndex < 0 || m_serverIndex >= m_servers.size()) {
        return {};
    }
    return m_servers.at(m_serverIndex);
}

// ── Station role ─────────────────────────────────────────────────────────

void RendezvousClient::registerStation(const QByteArray& stationKey, Signer sign,
                                       DeviceLookup devices)
{
    if (m_role == Role::Client) {
        qCWarning(lcRendezvous) << "A client connection cannot also register a Core";
        return;
    }
    const QString id = RendezvousWire::rendezvousId(stationKey);
    if (id.isEmpty() || !sign || !devices) {
        qCWarning(lcRendezvous) << "The Core's identity key is not usable; not registering";
        return;
    }
    m_role = Role::Station;
    m_stationKey = stationKey;
    m_stationId = id;
    m_stationSign = std::move(sign);
    m_devices = std::move(devices);
    m_stopped = false;
    m_reconnectAttempt = 0;
    m_replaced = false;
    if (m_servers.isEmpty()) {
        qCInfo(lcRendezvous) << "No remote access service is configured";
        return;
    }
    connectTo(0);
}

void RendezvousClient::claimNameplate()
{
    m_wantNameplate = true;
    if (m_role == Role::Station && m_registered) {
        RendezvousWire::Message message;
        message.kind = RendezvousWire::Kind::NameplateClaim;
        send(message);
    }
}

void RendezvousClient::releaseNameplate()
{
    m_wantNameplate = false;
    // Sent whether or not one is held: the service answers
    // nameplate.released either way (section 6.5).
    if (m_role == Role::Station && m_registered) {
        RendezvousWire::Message message;
        message.kind = RendezvousWire::Kind::NameplateRelease;
        send(message);
    }
}

bool RendezvousClient::answer(const QByteArray& introductionId, const QString& sdp)
{
    if (m_role != Role::Station || !m_registered || !m_liveIntroductions.contains(introductionId)
        || m_answered.contains(introductionId)) {
        return false;
    }
    RendezvousWire::Message message;
    message.kind = RendezvousWire::Kind::Answer;
    message.intro = introductionId;
    message.sdp = sdp;
    message.turnRequested = m_relayAllowed;
    if (!send(message)) {
        return false;
    }
    m_answered.insert(introductionId, 0);
    return true;
}

bool RendezvousClient::sendCandidate(const QByteArray& introductionId, const QString& candidate)
{
    if (m_role != Role::Station || !m_registered || !m_answered.contains(introductionId)
        || !m_liveIntroductions.contains(introductionId)) {
        return false;
    }
    const QString wire = RendezvousWire::wireCandidate(candidate);
    int& count = m_answered[introductionId];
    if (!wire.isEmpty()) {
        if (count >= kMaxCandidatesPerIntroduction) {
            return false;
        }
    }
    RendezvousWire::Message message;
    message.kind = RendezvousWire::Kind::Candidate;
    message.intro = introductionId;
    message.candidate = wire;
    if (!send(message)) {
        return false;
    }
    if (!wire.isEmpty()) {
        ++count;
    }
    return true;
}

// ── Client role ──────────────────────────────────────────────────────────

void RendezvousClient::introduce(const QString& stationId, const QByteArray& deviceKey,
                                 Signer sign, const QString& offer)
{
    if (m_role == Role::Station) {
        qCWarning(lcRendezvous) << "A Core's connection cannot introduce itself to another Core";
        return;
    }
    m_role = Role::Client;
    m_stopped = false;
    m_pending = Pending::Introduce;
    m_targetId = stationId;
    m_deviceKey = deviceKey;
    m_deviceSign = std::move(sign);
    m_offer = offer;
    m_lastRefusal.clear();
    m_networkTrouble.clear();
    if (m_socket) {
        // Connected: now. Still connecting: when the hello comes.
        if (m_helloReceived) {
            sendPending();
        }
        return;
    }
    if (m_servers.isEmpty()) {
        m_pending = Pending::None;
        emit unreachable(QStringLiteral("No remote access service is set up on this computer."));
        return;
    }
    connectTo(0);
}

void RendezvousClient::connectToService()
{
    if (m_role == Role::Station) {
        return;
    }
    m_role = Role::Client;
    m_stopped = false;
    if (m_socket) {
        if (m_helloReceived) {
            emit connected();
        }
        return;
    }
    if (m_servers.isEmpty()) {
        emit unreachable(QStringLiteral("No remote access service is set up on this computer."));
        return;
    }
    m_lastRefusal.clear();
    m_networkTrouble.clear();
    connectTo(0);
}

bool RendezvousClient::sendCandidate(const QString& candidate)
{
    if (m_role != Role::Client || !m_introductionLive) {
        return false;
    }
    RendezvousWire::Message message;
    message.kind = RendezvousWire::Kind::Candidate;
    message.candidate = RendezvousWire::wireCandidate(candidate);
    return send(message);
}

void RendezvousClient::openMailbox(int nameplate)
{
    if (m_role == Role::Station) {
        qCWarning(lcRendezvous) << "A Core's connection cannot open another Core's mailbox";
        return;
    }
    m_role = Role::Client;
    m_stopped = false;
    m_pending = Pending::OpenMailbox;
    m_mailboxNameplate = nameplate;
    m_lastRefusal.clear();
    m_networkTrouble.clear();
    if (m_socket) {
        // Connected: now. Still connecting: when the hello comes.
        if (m_helloReceived) {
            sendPending();
        }
        return;
    }
    if (m_servers.isEmpty()) {
        m_pending = Pending::None;
        emit unreachable(QStringLiteral("No remote access service is set up on this computer."));
        return;
    }
    connectTo(0);
}

// ── Both roles ───────────────────────────────────────────────────────────

bool RendezvousClient::sendMailbox(const QString& body)
{
    if (!m_mailboxOpen) {
        return false;
    }
    RendezvousWire::Message message;
    message.kind = RendezvousWire::Kind::Mailbox;
    message.body = body;
    return send(message);
}

void RendezvousClient::closeMailbox()
{
    if (!m_mailboxOpen) {
        return;
    }
    RendezvousWire::Message message;
    message.kind = RendezvousWire::Kind::MailboxClose;
    send(message);
}

void RendezvousClient::stop()
{
    m_stopped = true;
    m_reconnectTimer->stop();
    m_pending = Pending::None;
    resetConnection();
}

// ── The connection ───────────────────────────────────────────────────────

void RendezvousClient::resetConnection()
{
    ++m_generation;
    m_helloTimer->stop();
    m_pingTimer->stop();
    m_helloReceived = false;
    m_watchRelayNegotiated = false;
    m_registered = false;
    m_introductionLive = false;
    m_mailboxOpen = false;
    m_liveIntroductions.clear();
    m_answered.clear();
    m_relayGrants.clear();
    m_clientRelayGrant.reset();
    m_helloNonce.clear();
    if (m_socket) {
        QWebSocket* socket = m_socket;
        m_socket = nullptr;
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
}

void RendezvousClient::connectTo(int serverIndex)
{
    resetConnection();
    if (m_stopped || serverIndex < 0 || serverIndex >= m_servers.size()) {
        return;
    }
    m_serverIndex = serverIndex;
    // A test run never reaches a service off this computer: every test
    // binary runs in QStandardPaths test mode (tests/TestSandboxInit.cpp),
    // as AudioEngine keeps tests off real audio devices, so a test that
    // starts a Core with the default server list reaches nothing outside.
    if (QStandardPaths::isTestModeEnabled()
        && !isLoopbackHost(m_servers.at(serverIndex).host())) {
        qCInfo(lcRendezvous) << "Test run: not contacting a remote access service off this "
                                "computer";
        const quint64 generation = m_generation;
        QMetaObject::invokeMethod(this, [this, generation] {
            if (generation == m_generation) {
                tryNextServer();
            }
        }, Qt::QueuedConnection);
        return;
    }
    auto* socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    socket->setMaxAllowedIncomingMessageSize(
        static_cast<quint64>(RendezvousWire::kMaxIncomingMessageBytes));
    socket->setMaxAllowedIncomingFrameSize(
        static_cast<quint64>(RendezvousWire::kMaxIncomingMessageBytes));
    m_socket = socket;
    const quint64 generation = m_generation;
    connect(socket, &QWebSocket::connected, this, [this, generation] {
        if (generation == m_generation) {
            onConnected();
        }
    });
    connect(socket, &QWebSocket::disconnected, this, [this, generation] {
        if (generation == m_generation) {
            onDisconnected();
        }
    });
    connect(socket, &QWebSocket::errorOccurred, this,
            [this, generation](QAbstractSocket::SocketError error) {
        if (generation != m_generation) {
            return;
        }
        // A proxy that demands a login, in the operator's words (also
        // latched from its request below, whichever comes first).
        const QString words = NetworkTrouble::wordsForSocketError(error);
        if (!words.isEmpty()) {
            qCWarning(lcRendezvous).noquote() << words;
            m_networkTrouble = words;
        }
        // A refused or unreachable server may never say disconnected.
        QMetaObject::invokeMethod(this, [this, generation] {
            if (generation == m_generation) {
                onDisconnected();
            }
        }, Qt::QueuedConnection);
    });
    // A proxy that demands a login: NereusSDR gives it none (JJ's ruling of
    // 2026-09-28), so the connection fails next; the reason says why.
    connect(socket, &QWebSocket::proxyAuthenticationRequired, this,
            [this, generation](const QNetworkProxy&, QAuthenticator*) {
        if (generation == m_generation) {
            qCWarning(lcRendezvous).noquote() << NetworkTrouble::proxyNeedsLoginWords();
            m_networkTrouble = NetworkTrouble::proxyNeedsLoginWords();
        }
    });
    // Task 29 step 2b (options survey B.6, B.7): a sign-in page or an
    // inspecting network, in the operator's words. The service is not
    // pinned; the system's trust decides, as before.
    connect(socket, &QWebSocket::sslErrors, this,
            [this, generation](const QList<QSslError>& errors) {
        if (generation != m_generation) {
            return;
        }
        const QString words = NetworkTrouble::wordsForTlsErrors(errors);
        if (!words.isEmpty()) {
            qCWarning(lcRendezvous).noquote() << words;
            m_networkTrouble = words;
        }
    });
    connect(socket, &QWebSocket::textMessageReceived, this,
            [this, generation](const QString& text) {
        if (generation == m_generation) {
            onText(text);
        }
    });
    connect(socket, &QWebSocket::binaryMessageReceived, this, [this, generation] {
        if (generation == m_generation) {
            qCWarning(lcRendezvous) << "The remote access service sent a binary message; ignored";
        }
    });
    connect(socket, &QWebSocket::pong, this, [this, generation] {
        if (generation == m_generation) {
            m_missedPongs = 0;
        }
    });
    m_helloTimer->start(m_helloTimeoutMs);
    qCInfo(lcRendezvous) << "Connecting to the remote access service"
                         << m_servers.at(serverIndex).host();
    // Task 29 step 2b (options survey B.5): the computer's own proxy
    // settings, for a network that reaches the web through one.
    socket->setProxy(SystemProxy::forUrl(m_servers.at(serverIndex)));
    socket->open(m_servers.at(serverIndex));
}

void RendezvousClient::onConnected()
{
    // The hello follows at once (section 6.1); m_helloTimer still runs.
}

void RendezvousClient::onHelloTimeout()
{
    if (m_helloReceived) {
        // Fix wave I3: a service that sent its hello and then did not
        // register the Core in time (a hung backend behind its proxy keeps
        // the socket open; a lost challenge or proof) would otherwise leave
        // the Core connected, unregistered and unpinged for good. Leave the
        // connection and let the reconnect run.
        if (m_role == Role::Station && !m_registered) {
            qCInfo(lcRendezvous) << "The remote access service did not register the Core in time";
            onDisconnected();
        }
        return;
    }
    qCInfo(lcRendezvous) << "The remote access service did not answer in time";
    tryNextServer();
}

void RendezvousClient::onDisconnected()
{
    const bool hadHello = m_helloReceived;
    const bool mailboxWasOpen = m_mailboxOpen;
    const QList<QByteArray> introductions = m_liveIntroductions.values();
    const bool clientIntroductionLive = m_introductionLive;
    resetConnection();
    if (!hadHello) {
        tryNextServer();
        return;
    }
    qCInfo(lcRendezvous) << "The connection to the remote access service ended";
    // The introductions and the mailbox end with this connection; nothing
    // that runs on a connection an introduction set up is touched.
    for (const QByteArray& id : introductions) {
        emit introductionEnded(id, QString::fromLatin1(kLostCode));
    }
    if (clientIntroductionLive) {
        emit introductionEnded(QByteArray(), QString::fromLatin1(kLostCode));
    }
    if (mailboxWasOpen) {
        emit mailboxClosed(QString::fromLatin1(kLostCode));
    }
    emit connectionLost();
    if (m_role == Role::Station && !m_stopped) {
        scheduleReconnect();
    }
}

void RendezvousClient::tryNextServer()
{
    resetConnection();
    if (m_stopped) {
        return;
    }
    if (m_serverIndex + 1 < m_servers.size()) {
        connectTo(m_serverIndex + 1);
        return;
    }
    if (m_role == Role::Station) {
        scheduleReconnect();
        return;
    }
    m_pending = Pending::None;
    QString reason = m_lastRefusal;
    if (reason.isEmpty()) {
        reason = m_networkTrouble;
    }
    if (reason.isEmpty()) {
        reason = QStringLiteral("The remote access service could not be reached. Check this "
                                "computer's internet connection.");
    }
    emit unreachable(reason);
}

void RendezvousClient::scheduleReconnect()
{
    if (m_stopped || m_servers.isEmpty()) {
        return;
    }
    const int index = std::min<int>(m_reconnectAttempt,
                                    static_cast<int>(m_reconnectDelaysMs.size()) - 1);
    const int delay = m_reconnectDelaysMs.at(index);
    ++m_reconnectAttempt;
    qCInfo(lcRendezvous) << "Trying the remote access service again in" << delay << "ms";
    m_reconnectTimer->start(delay);
}

void RendezvousClient::onPingTick()
{
    if (!m_socket || !m_helloReceived) {
        return;
    }
    if (m_missedPongs >= kMaxMissedPongs) {
        qCInfo(lcRendezvous) << "The remote access service stopped answering";
        m_socket->abort();
        return;
    }
    ++m_missedPongs;
    m_socket->ping();
}

RendezvousWire::Direction RendezvousClient::outgoing() const
{
    return m_role == Role::Station ? RendezvousWire::Direction::StationToService
                                   : RendezvousWire::Direction::ClientToService;
}

RendezvousWire::Direction RendezvousClient::incoming() const
{
    return m_role == Role::Station ? RendezvousWire::Direction::ServiceToStation
                                   : RendezvousWire::Direction::ServiceToClient;
}

bool RendezvousClient::send(const RendezvousWire::Message& message)
{
    if (!m_socket || !m_helloReceived) {
        return false;
    }
    const QByteArray wire = RendezvousWire::encode(outgoing(), message);
    if (wire.isEmpty()) {
        // The sender's rule (section 2): a message that breaks a field's
        // kind or the cap is not sent.
        qCWarning(lcRendezvous) << "Not sending a" << RendezvousWire::kindName(message.kind)
                                << "message that does not fit the rendezvous wire";
        return false;
    }
    m_socket->sendTextMessage(QString::fromUtf8(wire));
    return true;
}

void RendezvousClient::onText(const QString& text)
{
    RendezvousWire::Message message;
    QString why;
    if (!RendezvousWire::decode(incoming(), text.toUtf8(), &message, &why)) {
        // Section 5.1: logged and ignored, never fatal on this side.
        qCInfo(lcRendezvous) << "Ignored a message from the remote access service:" << why;
        return;
    }
    handle(message);
}

void RendezvousClient::handle(const RendezvousWire::Message& message)
{
    using RendezvousWire::Kind;
    if (!m_helloReceived) {
        if (message.kind == Kind::Error) {
            handleError(message);
            return;
        }
        if (message.kind != Kind::Hello) {
            qCInfo(lcRendezvous) << "Ignored a message before the service's hello";
            return;
        }
        m_helloReceived = true;
        m_helloTimer->stop();
        m_helloNonce = message.nonce;
        m_stunUrls = message.stun;
        m_watchRelayNegotiated = m_watchRelayEnabled && message.version >= 2
            && message.watchRelayVersion == 1;
        emit connected();
        if (m_role == Role::Station) {
            RendezvousWire::Message registration;
            registration.kind = Kind::Register;
            registration.id = m_stationId;
            registration.publicKey = m_stationKey;
            registration.watchRelayVersion = m_watchRelayNegotiated ? 1 : 0;
            send(registration);
            // Section 3: registering must finish within the service's own
            // handshake time; the hello timer covers it on this side
            // (onHelloTimeout(), fix wave I3).
            m_helloTimer->start(m_helloTimeoutMs);
        } else {
            sendPending();
        }
        return;
    }

    switch (message.kind) {
    case Kind::Hello:
        qCInfo(lcRendezvous) << "Ignored a second hello";
        return;
    case Kind::Challenge: {
        if (m_role != Role::Station || m_registered) {
            return;
        }
        const QByteArray signature =
            m_stationSign(RendezvousWire::registerTranscript(message.nonce));
        RendezvousWire::Message prove;
        prove.kind = Kind::Prove;
        prove.signature = signature;
        send(prove);
        return;
    }
    case Kind::Registered:
        if (m_role != Role::Station || message.id != m_stationId) {
            return;
        }
        m_helloTimer->stop();
        m_registered = true;
        // Fix wave: after `replaced`, keep backing off, so two Cores with
        // one key do not take the registration from each other at the
        // first rung for ever.
        if (!m_replaced) {
            m_reconnectAttempt = 0;
        }
        m_replaced = false;
        m_missedPongs = 0;
        m_pingTimer->start();
        qCInfo(lcRendezvous) << "Registered with the remote access service as"
                             << shortId(m_stationId);
        emit registered();
        if (m_wantNameplate) {
            RendezvousWire::Message claim;
            claim.kind = Kind::NameplateClaim;
            send(claim);
        }
        return;
    case Kind::Introduction:
        handleIntroduction(message);
        return;
    case Kind::Credentials:
        if (!m_answered.contains(message.intro)) {
            return;
        }
        emit credentialsReceived(message.intro, message.turn.has_value(),
                                 message.turn.value_or(RendezvousWire::Turn{}));
        return;
    case Kind::RelayGrant: {
        // Task 29 fix wave (rendezvous section 12.1): kept for the relay
        // leg; the token is never logged.
        RendezvousWire::RelayGrant grant{message.relayUrl, message.relayToken,
                                         message.relayExpires,
                                         m_watchRelayNegotiated ? message.watchToken : QString()};
        if (m_role == Role::Station) {
            if (!m_answered.contains(message.intro)) {
                return;
            }
            m_relayGrants.insert(message.intro, grant);
            emit relayGrantReceived(message.intro);
        } else if (m_introductionLive) {
            m_clientRelayGrant = grant;
            emit relayGrantReceived(QByteArray());
        }
        return;
    }
    case Kind::Answer:
        if (m_role != Role::Client || !m_introductionLive) {
            return;
        }
        m_pending = Pending::None;
        emit answerReceived(message.sdp, message.turn.has_value(),
                            message.turn.value_or(RendezvousWire::Turn{}));
        return;
    case Kind::Candidate:
        if (m_role == Role::Station) {
            if (!m_liveIntroductions.contains(message.intro)) {
                return;
            }
            emit candidateReceived(message.intro, message.candidate);
        } else if (m_introductionLive) {
            emit candidateReceived(QByteArray(), message.candidate);
        }
        return;
    case Kind::IntroductionEnd:
        if (m_role == Role::Station) {
            if (!m_liveIntroductions.remove(message.intro)) {
                return;
            }
            m_answered.remove(message.intro);
            m_relayGrants.remove(message.intro);
            emit introductionEnded(message.intro, message.code);
        } else if (m_introductionLive) {
            m_introductionLive = false;
            m_pending = Pending::None;
            // Re-review: the grant goes with its introduction.
            m_clientRelayGrant.reset();
            emit introductionEnded(QByteArray(), message.code);
        }
        return;
    case Kind::Nameplate:
        // Only an answer to this Core's own claim (fix wave).
        if (m_role != Role::Station || !m_wantNameplate) {
            return;
        }
        emit nameplateClaimed(message.nameplate);
        return;
    case Kind::NameplateReleased:
        emit nameplateReleased();
        return;
    case Kind::MailboxOpened:
        // One mailbox at a time: a second `mailbox.opened` while one is open
        // (only a misbehaving service sends it) would start a second
        // exchange over the same bodies (fix wave).
        if (m_mailboxOpen) {
            qCInfo(lcRendezvous) << "Ignored a second mailbox while one is open";
            return;
        }
        m_mailboxOpen = true;
        if (m_role == Role::Client) {
            m_pending = Pending::None;
        }
        emit mailboxOpened(message.nameplate);
        return;
    case Kind::Mailbox:
        if (m_mailboxOpen) {
            emit mailboxReceived(message.body);
        }
        return;
    case Kind::MailboxClosed:
        if (m_mailboxOpen) {
            m_mailboxOpen = false;
            emit mailboxClosed(message.code);
        }
        return;
    case Kind::Error:
        handleError(message);
        return;
    default:
        return;
    }
}

std::optional<RendezvousWire::RelayGrant> RendezvousClient::relayGrant(
    const QByteArray& introductionId) const
{
    if (m_role == Role::Client) {
        return m_clientRelayGrant;
    }
    const auto it = m_relayGrants.constFind(introductionId);
    if (it == m_relayGrants.cend()) {
        return std::nullopt;
    }
    return it.value();
}

bool RendezvousClient::retireIntroduction(const QByteArray& introductionId)
{
    if (m_role != Role::Station || !m_liveIntroductions.remove(introductionId)) {
        return false;
    }
    m_answered.remove(introductionId);
    m_relayGrants.remove(introductionId);
    m_retiredIntroductions.append(introductionId);
    while (m_retiredIntroductions.size() > kMaxLiveIntroductions * 4) {
        m_retiredIntroductions.removeFirst();
    }
    return true;
}

void RendezvousClient::handleIntroduction(const RendezvousWire::Message& message)
{
    if (m_role != Role::Station || !m_registered || m_liveIntroductions.contains(message.intro)
        || m_retiredIntroductions.contains(message.intro)) {
        return;
    }
    // Section 4.4: the device must be one this Core paired (a revoked one
    // is no longer in the list), and its signature over this Core's id and
    // the introducing connection's nonce must verify with that device's
    // key. Anything else gets no answer at all, so the service learns
    // nothing about which devices are paired.
    const QByteArray deviceKey = m_devices ? m_devices(message.device) : QByteArray();
    const bool known = !deviceKey.isEmpty()
                       && StationIdentity::fingerprintOf(deviceKey) == message.device;
    const bool verified =
        known
        && StationIdentity::verify(deviceKey,
                                   RendezvousWire::introduceTranscript(m_stationId, message.nonce),
                                   message.deviceSignature);
    if (!verified || m_liveIntroductions.size() >= kMaxLiveIntroductions) {
        ++m_droppedIntroductions;
        qCInfo(lcRendezvous) << (verified ? "Dropped an introduction: too many are open ("
                                          : "Dropped an introduction that is not from a paired "
                                            "device (")
                             << m_droppedIntroductions << "so far)";
        return;
    }
    m_liveIntroductions.insert(message.intro);
    RendezvousIntroduction introduction;
    introduction.id = message.intro;
    introduction.deviceId = message.device;
    introduction.deviceKey = deviceKey;
    introduction.offer = message.sdp;
    qCInfo(lcRendezvous) << "A paired device asked for a connection through the remote access "
                            "service";
    emit introduced(introduction);
}

void RendezvousClient::handleError(const RendezvousWire::Message& message)
{
    qCInfo(lcRendezvous) << "The remote access service said" << message.code;
    if (m_role == Role::Station && message.code == QLatin1String("replaced")) {
        m_replaced = true;
    }
    emit serviceError(message.code, message.reason, message.retryAfterMs);
    if (m_role != Role::Client) {
        return;
    }
    const bool notHere = message.code == QLatin1String("offline")
                         || message.code == QLatin1String("nameplateUnknown");
    if (notHere && m_pending != Pending::None) {
        // The Core may be registered with a server further down the list.
        m_lastRefusal = message.reason;
        m_introductionLive = false;
        tryNextServer();
        return;
    }
    if (m_pending == Pending::OpenMailbox
        && (message.code == QLatin1String("nameplateBusy")
            || message.code == QLatin1String("rateLimited"))) {
        m_pending = Pending::None;
        emit unreachable(message.reason);
    } else if (m_pending == Pending::Introduce && message.code == QLatin1String("rateLimited")) {
        m_pending = Pending::None;
        m_introductionLive = false;
        emit unreachable(message.reason);
    }
}

void RendezvousClient::sendPending()
{
    using RendezvousWire::Kind;
    if (m_pending == Pending::Introduce) {
        RendezvousWire::Message introduce;
        introduce.kind = Kind::Introduce;
        introduce.id = m_targetId;
        introduce.device = StationIdentity::fingerprintOf(m_deviceKey);
        introduce.deviceSignature =
            m_deviceSign ? m_deviceSign(RendezvousWire::introduceTranscript(m_targetId,
                                                                            m_helloNonce))
                         : QByteArray();
        introduce.sdp = m_offer;
        introduce.watchRelayVersion = m_watchRelayNegotiated ? 1 : 0;
        if (send(introduce)) {
            m_introductionLive = true;
            m_clientRelayGrant.reset();
        } else {
            m_pending = Pending::None;
            emit unreachable(QStringLiteral("This computer could not ask the Core for a "
                                            "connection."));
        }
        return;
    }
    if (m_pending == Pending::OpenMailbox) {
        RendezvousWire::Message open;
        open.kind = Kind::MailboxOpen;
        open.nameplate = m_mailboxNameplate;
        if (!send(open)) {
            m_pending = Pending::None;
            emit unreachable(QStringLiteral("That is not a pairing code."));
        }
    }
}

} // namespace NereusSDR
