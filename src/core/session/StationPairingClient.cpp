// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/StationPairingClient.cpp  (NereusSDR)
// =================================================================
//
// See StationPairingClient.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-08): pairing by code
//               through the remote access service's mailbox. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Task 28 fix wave (privacy): the mailbox's plain
//               pair.start names this computer only kMailboxPlainName; its
//               own name travels sealed. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/NetworkTrouble.h"
#include "core/session/SystemProxy.h"
#include "core/session/StationPairingClient.h"

#include "core/AppSettings.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/PairingCode.h"
#include "core/security/SpakeExchange.h"
#include "core/security/StationIdentity.h"
#include "core/session/LinkVersion.h"
#include "core/session/RendezvousClient.h"
#include "core/session/RendezvousMailboxTransport.h"
#include "core/session/SessionEndReasons.h"
#include "core/session/SessionMessages.h"
#include "core/session/SessionTransport.h"
#include "core/session/StationClient.h"

#include <QAuthenticator>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSslError>
#include <QThread>
#include <QTimer>
#include <QWebSocket>

#include <utility>

namespace NereusSDR {
namespace {
Q_LOGGING_CATEGORY(lcPairing, "nereus.pairingclient")

constexpr const char* kPeerName = "NereusSDR GUI";

// Plain words for this computer's own reasons (the Core's come in
// pair.fail and are shown as sent).
QString unreadableAnswer()
{
    return QStringLiteral("The Core answered in a way this app cannot read. Update "
                          "NereusSDR on this computer or on the Core.");
}

qint32 localSettingsSchema()
{
    // As StationClient's hello: the value this computer's own settings
    // carry, so the Core logs no difference for a pairing connection.
    return static_cast<qint32>(AppSettings::instance()
                                   .value(QStringLiteral("SettingsSchemaVersion"),
                                          QStringLiteral("0"))
                                   .toString()
                                   .toInt());
}

// The default connection: TLS to the Core, whose self-signed certificate
// is accepted here because the pairing itself proves the Core (the code),
// or is the accepted first-use risk of one tap on the Core's own network
// (the pairing design, section 4.2). The certificate is still checked
// against the Core's binding before its identity is kept. Validity dates
// are not waved through, as on StationClient's pinned links.
SessionTransport* openWebSocket(const QUrl& url)
{
    auto* socket = new QWebSocket();
    auto* transport = new WebSocketTransport(socket, StationPairingClient::kMaxIncomingMessageBytes);
    QObject::connect(socket, &QWebSocket::sslErrors, socket,
                     [socket](const QList<QSslError>& errors) {
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
    // Task 29 step 2b: the computer's own proxy settings (SystemProxy).
    socket->setProxy(SystemProxy::forUrl(url));
    socket->open(url);
    return transport;
}

} // namespace

StationPairingClient::StationPairingClient(std::shared_ptr<const ClientDeviceIdentity> identity,
                                           QString deviceName, QObject* parent)
    : QObject(parent)
    , m_identity(std::move(identity))
    , m_deviceName(std::move(deviceName))
    , m_factory(openWebSocket)
{
    m_deadlineTimer = new QTimer(this);
    m_deadlineTimer->setSingleShot(true);
    connect(m_deadlineTimer, &QTimer::timeout, this, [this] {
        fail(QStringLiteral("The Core did not finish pairing in time. Check the address "
                            "and try again."));
    });
}

StationPairingClient::~StationPairingClient()
{
    ++m_attempt;
    // The worker holds its own references to the exchange and the code;
    // waiting here only keeps a hash from outliving the process's teardown.
    if (m_worker) {
        m_worker->wait();
    }
    wipeCode();
}

void StationPairingClient::setTransportFactory(TransportFactory factory)
{
    m_factory = factory ? std::move(factory) : TransportFactory(openWebSocket);
}

void StationPairingClient::setDeadlineMs(int ms)
{
    m_deadlineMs = ms;
}

QUrl StationPairingClient::coreUrl(const QString& host, quint16 port)
{
    QUrl url;
    url.setScheme(QStringLiteral("wss"));
    url.setHost(host);   // QUrl brackets an IPv6 literal itself.
    url.setPort(port);
    return url;
}

bool StationPairingClient::parseAddress(const QString& text, QString* host, quint16* port)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.contains(QLatin1Char(' '))) {
        return false;
    }
    QString hostPart;
    int portNumber = kDefaultPort;
    const auto readPort = [&portNumber](const QString& digits) {
        bool ok = false;
        const int value = digits.toInt(&ok);
        if (!ok || value < 1 || value > 65535) {
            return false;
        }
        portNumber = value;
        return true;
    };

    if (trimmed.contains(QLatin1String("://"))) {
        const QUrl url(trimmed, QUrl::StrictMode);
        if (!url.isValid() || (url.scheme() != QLatin1String("wss")
                               && url.scheme() != QLatin1String("ws"))
            || url.host().isEmpty()) {
            return false;
        }
        hostPart = url.host();
        if (url.port() > 0) {
            portNumber = url.port();
        }
    } else if (trimmed.startsWith(QLatin1Char('['))) {
        // [IPv6] or [IPv6]:port
        const qsizetype close = trimmed.indexOf(QLatin1Char(']'));
        if (close < 0) {
            return false;
        }
        hostPart = trimmed.mid(1, close - 1);
        const QString rest = trimmed.mid(close + 1);
        if (!rest.isEmpty()) {
            if (!rest.startsWith(QLatin1Char(':')) || !readPort(rest.mid(1))) {
                return false;
            }
        }
        if (QHostAddress(hostPart).protocol() != QAbstractSocket::IPv6Protocol) {
            return false;
        }
    } else if (trimmed.count(QLatin1Char(':')) > 1) {
        // A bare IPv6 literal carries no port.
        hostPart = trimmed;
        if (QHostAddress(hostPart).protocol() != QAbstractSocket::IPv6Protocol) {
            return false;
        }
    } else {
        const qsizetype colon = trimmed.indexOf(QLatin1Char(':'));
        hostPart = colon < 0 ? trimmed : trimmed.left(colon);
        if (colon >= 0 && !readPort(trimmed.mid(colon + 1))) {
            return false;
        }
        // A name or an IPv4 address: what QUrl takes as a host.
        QUrl probe;
        probe.setHost(hostPart, QUrl::StrictMode);
        if (!probe.isValid() || probe.host().isEmpty()) {
            return false;
        }
    }
    if (hostPart.isEmpty()) {
        return false;
    }
    *host = hostPart;
    *port = static_cast<quint16>(portNumber);
    return true;
}

void StationPairingClient::pairOnThisNetwork(const QString& host, quint16 port)
{
    begin(true, QString(), host, port);
}

void StationPairingClient::pairByCode(const QString& code, const QString& host, quint16 port)
{
    // Refused before anything is sent: a mistyped word would otherwise burn
    // the Core's code (the plan's PairingCodeText rule).
    const QString normalised = PairingCode::normalise(code);
    if (normalised.isEmpty()) {
        cancel();
        emit failed(QStringLiteral("That is not a pairing code. A code is a number and two "
                                   "words, such as 7-anvil-harbor."));
        return;
    }
    begin(false, normalised, host, port);
}

void StationPairingClient::pairByCodeOverMailbox(const QString& code, SessionTransport* mailbox)
{
    const QString normalised = PairingCode::normalise(code);
    cancel();
    if (normalised.isEmpty()) {
        if (mailbox != nullptr) {
            mailbox->closeLink(QString());
            mailbox->deleteLater();
        }
        emit failed(QStringLiteral("That is not a pairing code. A code is a number and two "
                                   "words, such as 7-anvil-harbor."));
        return;
    }
    if (!readyForCode()) {
        if (mailbox != nullptr) {
            mailbox->closeLink(QString());
            mailbox->deleteLater();
        }
        return;
    }
    startMailbox(normalised, mailbox);
}

void StationPairingClient::pairByCodeFromAnywhere(const QString& code,
                                                  const QList<QUrl>& servers)
{
    // Refused before anything is sent, as on a direct connection.
    const QString normalised = PairingCode::normalise(code);
    cancel();
    if (normalised.isEmpty()) {
        emit failed(QStringLiteral("That is not a pairing code. A code is a number and two "
                                   "words, such as 7-anvil-harbor."));
        return;
    }
    if (!readyForCode()) {
        return;
    }
    // The number in the code is the Core's nameplate on the service.
    const int nameplate = normalised.section(QLatin1Char('-'), 0, 0).toInt();
    m_code = normalised;
    m_mailbox = true;
    m_state = State::OpeningMailbox;
    auto* rendezvous = new RendezvousClient(this);
    m_rendezvous = rendezvous;
    rendezvous->setServers(servers);
    const quint64 attempt = m_attempt;
    connect(rendezvous, &RendezvousClient::mailboxOpened, this, [this, attempt, rendezvous] {
        if (attempt != m_attempt || m_state != State::OpeningMailbox) {
            return;
        }
        startMailbox(m_code, new RendezvousMailboxTransport(rendezvous));
    });
    connect(rendezvous, &RendezvousClient::unreachable, this,
            [this, attempt](const QString& reason) {
        if (attempt == m_attempt) {
            fail(reason);
        }
    });
    if (m_deadlineMs > 0) {
        m_deadlineTimer->start(m_deadlineMs);
    }
    rendezvous->openMailbox(nameplate);
}

bool StationPairingClient::readyForCode()
{
    if (!m_identity || !m_identity->isValid()) {
        emit failed(QStringLiteral("This computer's own key could not be read, so it cannot "
                                   "pair with a Core."));
        return false;
    }
    if (!SpakeExchange::isAvailable()) {
        emit failed(QStringLiteral("This computer cannot pair by code."));
        return false;
    }
    return true;
}

void StationPairingClient::startMailbox(const QString& normalisedCode, SessionTransport* mailbox)
{
    if (mailbox == nullptr || !mailbox->isOpen()) {
        if (mailbox != nullptr) {
            mailbox->deleteLater();
        }
        m_state = State::AwaitStep0;
        fail(QStringLiteral("The remote access service closed the pairing before it began. "
                            "Try again."));
        return;
    }
    m_mailbox = true;
    m_lan = false;
    m_code = normalisedCode;
    m_host.clear();
    m_port = 0;
    mailbox->setParent(this);
    m_transport = mailbox;
    const quint64 attempt = m_attempt;
    connect(mailbox, &SessionTransport::textReceived, this,
            [this, attempt](const QByteArray& wire) {
        if (attempt == m_attempt) {
            onText(wire);
        }
    });
    connect(mailbox, &SessionTransport::closed, this, [this, attempt] {
        if (attempt == m_attempt) {
            onClosed();
        }
    });
    if (m_deadlineMs > 0 && !m_deadlineTimer->isActive()) {
        m_deadlineTimer->start(m_deadlineMs);
    }
    // No hellos through a mailbox (the rendezvous document, section 6.5):
    // pair.start first, in code mode, the only mode a mailbox carries. The
    // service sees it, so it names this computer only kMailboxPlainName;
    // the real name is in the sealed box (onStep2), the one the Core keeps.
    send(SessionMessages::pairStart(
        QStringLiteral("code"),
        SessionPairDevice{StationIdentity::toBase64Url(m_identity->publicKeySpki()),
                          QString::fromLatin1(kMailboxPlainName),
                          QString::fromLatin1(ClientDeviceIdentity::kKind)}));
    m_state = State::AwaitStep0;
}

void StationPairingClient::cancel()
{
    ++m_attempt;
    m_deadlineTimer->stop();
    stopTransport();
    m_mailbox = false;
    if (m_rendezvous) {
        RendezvousClient* rendezvous = m_rendezvous;
        m_rendezvous = nullptr;
        rendezvous->stop();
        rendezvous->deleteLater();
    }
    m_exchange.reset();
    wipeCode();
    m_helloIdentityKey.clear();
    m_state = State::Idle;
}

void StationPairingClient::begin(bool lan, const QString& normalisedCode, const QString& host,
                                 quint16 port)
{
    cancel();
    if (!m_identity || !m_identity->isValid()) {
        emit failed(QStringLiteral("This computer's own key could not be read, so it cannot "
                                   "pair with a Core."));
        return;
    }
    if (host.isEmpty() || port == 0) {
        emit failed(QStringLiteral("Enter the Core's address."));
        return;
    }
    if (!lan && !SpakeExchange::isAvailable()) {
        emit failed(QStringLiteral("This computer cannot pair by code."));
        return;
    }
    m_lan = lan;
    m_code = normalisedCode;
    m_host = host;
    m_port = port;
    m_state = State::AwaitHello;

    SessionTransport* transport = m_factory(coreUrl(host, port));
    if (transport == nullptr) {
        m_state = State::Idle;
        wipeCode();
        emit failed(QStringLiteral("This computer could not open a connection to the Core."));
        return;
    }
    transport->setParent(this);
    m_transport = transport;
    const quint64 attempt = m_attempt;
    connect(transport, &SessionTransport::textReceived, this,
            [this, attempt](const QByteArray& wire) {
        if (attempt == m_attempt) {
            onText(wire);
        }
    });
    connect(transport, &SessionTransport::closed, this, [this, attempt] {
        if (attempt == m_attempt) {
            onClosed();
        }
    });
    if (auto* ws = qobject_cast<WebSocketTransport*>(transport)) {
        // A proxy that demands a login: NereusSDR gives it none (JJ's
        // ruling of 2026-09-28), so pairing cannot go on; say why at once.
        connect(ws->socket(), &QWebSocket::proxyAuthenticationRequired, this,
                [this, attempt](const QNetworkProxy&, QAuthenticator*) {
            if (attempt == m_attempt && m_state != State::Idle) {
                fail(NetworkTrouble::proxyNeedsLoginWords());
            }
        });
        connect(ws->socket(), &QWebSocket::errorOccurred, this,
                [this, attempt, ws](QAbstractSocket::SocketError error) {
            if (attempt == m_attempt && m_state != State::Idle) {
                fail(StationClient::connectionFailureReason(error, ws->socket()->errorString(),
                                                            m_host));
            }
        });
    }
    if (m_deadlineMs > 0) {
        m_deadlineTimer->start(m_deadlineMs);
    }
}

void StationPairingClient::onText(const QByteArray& wire)
{
    SessionMessage message;
    if (!SessionMessages::decode(wire, &message)) {
        fail(unreadableAnswer());
        return;
    }
    // The Core's own reasons, in any state.
    if (message.kind == SessionMessageKind::PairFail) {
        fail(message.reason);
        return;
    }
    if (message.kind == SessionMessageKind::SessionEnd) {
        fail(message.reason);
        return;
    }

    switch (m_state) {
    case State::AwaitHello:
        if (message.kind == SessionMessageKind::Hello) {
            handleHello(message);
            return;
        }
        break;
    case State::AwaitAccept:
        if (message.kind == SessionMessageKind::PairAccept && message.stationIdentity) {
            complete(message.stationIdentity->publicKey, message.stationIdentity->certBinding,
                     message.pairLabel);
            return;
        }
        break;
    case State::AwaitStep0:
        if (message.kind == SessionMessageKind::PairSpake && message.pairStep == 0) {
            handleStep0(message);
            return;
        }
        break;
    case State::AwaitStep2:
        if (message.kind == SessionMessageKind::PairSpake && message.pairStep == 2) {
            handleStep2(message);
            return;
        }
        break;
    case State::AwaitConfirm:
        if (message.kind == SessionMessageKind::PairConfirm) {
            handleConfirm(message);
            return;
        }
        break;
    case State::Idle:
    case State::OpeningMailbox:
    case State::Hashing:
    case State::AwaitCoreFail:
        break;
    }
    fail(unreadableAnswer());
}

void StationPairingClient::onClosed()
{
    if (m_state == State::Idle) {
        return;
    }
    fail(QStringLiteral("The Core closed the connection before pairing finished. Try again."));
}

void StationPairingClient::handleHello(const SessionMessage& message)
{
    const QList<quint16> ours = LinkVersion::supportedMajors();
    const std::optional<quint16> agreed = LinkVersion::agreeMajor(ours, message.supportedMajors);
    if (!agreed) {
        fail(SessionEndReasons::versionRefused(message.supportedMajors, ours));
        return;
    }
    if (message.features.value(QByteArrayLiteral("pairing"), 0) < 1 || !message.stationIdentity) {
        fail(QStringLiteral("This Core cannot pair new devices. Update NereusSDR on the "
                            "Core's computer."));
        return;
    }
    bool ok = false;
    m_helloIdentityKey = StationIdentity::fromBase64Url(message.stationIdentity->publicKey, &ok);
    if (!ok) {
        m_helloIdentityKey.clear();
    }

    send(SessionMessages::hello(*agreed, kSessionProtocolMinor, localSettingsSchema(),
                                QString::fromLatin1(kPeerName), ours, {{"deviceAuth", 1}}));
    send(SessionMessages::pairStart(
        m_lan ? QStringLiteral("lan") : QStringLiteral("code"),
        SessionPairDevice{StationIdentity::toBase64Url(m_identity->publicKeySpki()), m_deviceName,
                          QString::fromLatin1(ClientDeviceIdentity::kKind)}));
    m_state = m_lan ? State::AwaitAccept : State::AwaitStep0;
}

void StationPairingClient::handleStep0(const SessionMessage& message)
{
    bool ok = false;
    const QByteArray publicData = StationIdentity::fromBase64Url(message.pairData, &ok);
    if (!ok) {
        fail(unreadableAnswer());
        return;
    }
    // Argon2id, on a worker: the window keeps drawing while it runs. The
    // worker holds its own references, so a cancel() never pulls the
    // exchange or the code out from under it.
    m_exchange = std::make_shared<SpakeExchange>(SpakeExchange::Role::Device);
    auto exchange = m_exchange;
    auto code = std::make_shared<QString>(m_code);
    wipeCode();
    auto response1 = std::make_shared<QByteArray>();
    const quint64 attempt = m_attempt;
    m_state = State::Hashing;
    QThread* worker = QThread::create([exchange, code, publicData, response1] {
        *response1 = exchange->deviceStep1(publicData, *code);
        code->fill(QChar(u'\0'));
        code->clear();
    });
    worker->setParent(this);
    m_worker = worker;
    connect(worker, &QThread::finished, this, [this, worker, attempt, response1] {
        worker->deleteLater();
        const QByteArray result = *response1;
        finishStep1(attempt, result);
    });
    worker->start();
}

void StationPairingClient::finishStep1(quint64 attempt, const QByteArray& response1)
{
    if (attempt != m_attempt || m_state != State::Hashing) {
        return;
    }
    if (response1.isEmpty()) {
        // A step 0 naming other hash settings than the fixed ones (a hostile
        // Core choosing a weak hash) is refused here, before the code is used.
        fail(unreadableAnswer());
        return;
    }
    send(SessionMessages::pairSpake(1, StationIdentity::toBase64Url(response1)));
    m_state = State::AwaitStep2;
}

void StationPairingClient::handleStep2(const SessionMessage& message)
{
    bool ok = false;
    const QByteArray response2 = StationIdentity::fromBase64Url(message.pairData, &ok);
    const QByteArray response3 = ok && m_exchange ? m_exchange->deviceStep3(response2)
                                                  : QByteArray();
    if (response3.isEmpty()) {
        // The codes differ. Say so, so the Core burns its code at once and
        // answers with when the next one appears (the link document,
        // section 3.6); its reason is what the operator reads.
        send(SessionMessages::pairFail(QStringLiteral("The pairing code did not match."), 0));
        m_state = State::AwaitCoreFail;
        return;
    }
    send(SessionMessages::pairSpake(3, StationIdentity::toBase64Url(response3)));
    const QJsonObject box{
        {QStringLiteral("publicKey"), StationIdentity::toBase64Url(m_identity->publicKeySpki())},
        {QStringLiteral("name"), m_deviceName},
        {QStringLiteral("kind"), QString::fromLatin1(ClientDeviceIdentity::kKind)},
    };
    const QByteArray sealed =
        m_exchange->sealConfirmation(QJsonDocument(box).toJson(QJsonDocument::Compact));
    if (sealed.isEmpty()) {
        fail(unreadableAnswer());
        return;
    }
    send(SessionMessages::pairConfirm(StationIdentity::toBase64Url(sealed)));
    m_state = State::AwaitConfirm;
}

void StationPairingClient::handleConfirm(const SessionMessage& message)
{
    bool ok = false;
    const QByteArray sealed = StationIdentity::fromBase64Url(message.pairBox, &ok);
    const std::optional<QByteArray> plain =
        ok && m_exchange ? m_exchange->openConfirmation(sealed) : std::nullopt;
    if (!plain) {
        fail(QStringLiteral("The Core's answer could not be confirmed with the pairing code. "
                            "Try again with the code the Core shows now."));
        return;
    }
    const QJsonObject station = QJsonDocument::fromJson(*plain).object();
    const QJsonObject identity = station.value(QStringLiteral("identity")).toObject();
    complete(identity.value(QStringLiteral("publicKey")).toString(),
             identity.value(QStringLiteral("certBinding")).toString(),
             station.value(QStringLiteral("label")).toString());
}

void StationPairingClient::complete(const QString& publicKey, const QString& certBinding,
                                    const QString& label)
{
    bool keyOk = false;
    bool bindingOk = false;
    const QByteArray spki = StationIdentity::fromBase64Url(publicKey, &keyOk);
    const QByteArray binding = StationIdentity::fromBase64Url(certBinding, &bindingOk);
    if (!keyOk || !bindingOk || !StationIdentity::isP256Spki(spki)) {
        fail(unreadableAnswer());
        return;
    }
    // Task 27: a mailbox carries no certificate. The key came in the box
    // sealed with the code's shared key, so it is the Core's; its binding is
    // checked against the certificate of this computer's first sign-in.
    if (m_mailbox) {
        if (binding.size() != StationIdentity::kSignatureBytes) {
            fail(unreadableAnswer());
            return;
        }
        PairedStationRecord record;
        record.identityKey = spki;
        record.identityFingerprint = StationIdentity::fingerprintOf(spki);
        record.label = label;
        cancel();
        qCInfo(lcPairing) << "Paired with a Core through the remote access service";
        emit paired(record);
        return;
    }
    // The key the Core paired with is the one its hello showed.
    if (!m_helloIdentityKey.isEmpty() && m_helloIdentityKey != spki) {
        fail(QStringLiteral("The Core gave two different identities while pairing, so this "
                            "computer did not keep either. Try again."));
        return;
    }
    // And it vouches for the certificate this connection presented.
    const QByteArray certificate =
        m_transport ? m_transport->peerCertificateSha256() : QByteArray();
    if (certificate.size() != 32
        || !StationIdentity::verify(spki, StationIdentity::certBindingMessage(certificate),
                                    binding)) {
        fail(QStringLiteral("The Core's certificate is not signed by the Core that paired, so "
                            "this computer did not keep the pairing."));
        return;
    }

    PairedStationRecord record;
    record.identityKey = spki;
    record.identityFingerprint = StationIdentity::fingerprintOf(spki);
    record.label = label;
    record.host = m_host;
    record.port = m_port;
    cancel();
    qCInfo(lcPairing) << "Paired with the Core at" << record.host << record.port;
    emit paired(record);
}

void StationPairingClient::fail(const QString& reason)
{
    if (m_state == State::Idle) {
        return;
    }
    cancel();
    // The reason only: never the code.
    qCWarning(lcPairing) << "Pairing did not complete:" << reason;
    emit failed(reason);
}

void StationPairingClient::send(const SessionMessage& message)
{
    if (m_transport) {
        m_transport->sendText(SessionMessages::encode(message));
    }
}

void StationPairingClient::stopTransport()
{
    if (m_transport) {
        SessionTransport* transport = m_transport;
        m_transport = nullptr;
        disconnect(transport, nullptr, this, nullptr);
        transport->closeLink(QStringLiteral("pairing finished"));
        transport->deleteLater();
    }
}

void StationPairingClient::wipeCode()
{
    m_code.fill(QChar(u'\0'));
    m_code.clear();
}

} // namespace NereusSDR
