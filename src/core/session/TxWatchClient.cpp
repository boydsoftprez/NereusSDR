// no-port-check: NereusSDR-original direct auxiliary watch transport.
// Modification history (NereusSDR):
//   2026-10-04: Qt 6.4 WebSocket error-signal compatibility. J.J. Boyd
//               (KG4VCF), AI-assisted via OpenAI Codex.
#include "core/session/TxWatchClient.h"

#include "core/safety/RemoteTxWatchdog.h"
#include "core/session/DataChannelTransport.h"

#include <QCryptographicHash>
#include <QPointer>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QTimer>
#include <QWebSocket>

namespace NereusSDR {
namespace {
constexpr int kTicketBytes = 32;
constexpr int kAttachBytes = 1 + kTicketBytes;
constexpr int kAckBytes = 2;
constexpr qint64 kMaxOutboundBacklog = 4096;
const QByteArray kAck = QByteArray::fromHex("0100");

void retireRelay(QPointer<DataChannelTransport> transport)
{
    if (!transport) { return; }
    transport->setParent(nullptr);
    transport->closeLink(QStringLiteral("watch closed"));
    if (transport) { transport->deleteLater(); }
}
}

TxWatchClient::TxWatchClient(QObject* parent)
    : QObject(parent)
    , m_deadline(new QTimer(this))
{
    m_deadline->setSingleShot(true);
    connect(m_deadline, &QTimer::timeout, this, [this]() {
        finish(m_ready ? QStringLiteral("watch deadline")
                       : QStringLiteral("watch attachment timed out"));
    });
}

TxWatchClient::~TxWatchClient()
{
    // Destructor must not invoke a user callback. Detach before abort(),
    // which may synchronously deliver a socket event.
    m_deadline->stop();
    if (const QPointer<QWebSocket> socket = m_socket) {
        m_socket = nullptr;
        QObject::disconnect(socket, nullptr, this, nullptr);
        // The ready handler may delete this client while QWebSocket is
        // emitting binaryMessageReceived. Keep the sender alive until that
        // emission unwinds; deleting a child in QObject's destructor would
        // destroy the sender in the middle of its own signal delivery.
        socket->setParent(nullptr);
        if (socket) { socket->deleteLater(); }
    }
    if (m_relay) {
        const QPointer<DataChannelTransport> transport = m_relay;
        m_relay = nullptr;
        QObject::disconnect(transport, nullptr, this, nullptr);
        retireRelay(transport);
    }
    m_ticket.fill('\0');
    m_ticket.clear();
    m_pin.clear();
}

void TxWatchClient::setDeadlinesForTesting(int openingMs, int acknowledgementMs)
{
    if (!m_active && openingMs > 0 && openingMs <= 10000
        && acknowledgementMs > 0 && acknowledgementMs <= 5000) {
        m_openingMs = openingMs;
        m_acknowledgementMs = acknowledgementMs;
    }
}

bool TxWatchClient::current(QWebSocket* socket, quint64 generation) const
{
    return m_active && m_socket == socket && m_generation == generation;
}

bool TxWatchClient::current(DataChannelTransport* transport, quint64 generation) const
{
    return m_active && m_relay == transport && m_generation == generation;
}

void TxWatchClient::receiveAck(const QByteArray& message)
{
    if (!m_attachSent || m_ready || message.size() != kAckBytes || message != kAck) {
        finish(QStringLiteral("unexpected watch message"));
        return;
    }
    m_telemetry.receivedPayloadBytes += quint64(message.size());
    m_deadline->stop();
    m_ready = true;
    const quint64 generation = m_generation;
    emit ready(generation);
}

bool TxWatchClient::freshPinMatches(QWebSocket* socket) const
{
    if (socket == nullptr || m_pin.size() != kTicketBytes) {
        return false;
    }
    const QSslCertificate certificate = socket->sslConfiguration().peerCertificate();
    return !certificate.isNull()
           && certificate.digest(QCryptographicHash::Sha256) == m_pin;
}

bool TxWatchClient::openDirect(const QUrl& verifiedPrimaryUrl,
                               const QByteArray& actualCorePinSha256,
                               const QByteArray& rawTicket, quint64 primaryGeneration)
{
    const QPointer<TxWatchClient> self(this);
    const quint64 revision = ++m_revision;
    if (m_active) {
        finish(QStringLiteral("watch replaced"));
    }
    if (!self || m_revision != revision || m_active) {
        return false;
    }
    m_generation = primaryGeneration;
    const bool valid = verifiedPrimaryUrl.isValid()
                       && verifiedPrimaryUrl.scheme() == QLatin1String("wss")
                       && !verifiedPrimaryUrl.host().isEmpty()
                       && !verifiedPrimaryUrl.authority(QUrl::FullyEncoded).contains('@')
                       && actualCorePinSha256.size() == kTicketBytes
                       && rawTicket.size() == kTicketBytes
                       && primaryGeneration != 0;
    if (!valid) {
        emit closed(primaryGeneration, QStringLiteral("invalid watch attachment inputs"));
        return false;
    }

    QUrl watchUrl;
    watchUrl.setScheme(QStringLiteral("wss"));
    watchUrl.setHost(verifiedPrimaryUrl.host());
    watchUrl.setPort(verifiedPrimaryUrl.port());
    watchUrl.setPath(QStringLiteral("/tx-watch/v1"));
    if (!watchUrl.isValid()) {
        emit closed(primaryGeneration, QStringLiteral("invalid watch address"));
        return false;
    }

    m_telemetry = {};
    m_pin = actualCorePinSha256;
    m_ticket = rawTicket;
    m_active = true;
    m_ready = false;
    m_attachSent = false;
    auto* socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_socket = socket;
    socket->setMaxAllowedIncomingMessageSize(kAttachBytes);
    socket->setMaxAllowedIncomingFrameSize(kAttachBytes);
    const quint64 generation = m_generation;

    connect(socket, &QWebSocket::sslErrors, this,
            [this, socket, generation](const QList<QSslError>& errors) {
        if (!current(socket, generation)) { return; }
        if (!freshPinMatches(socket)) {
            finish(QStringLiteral("watch certificate mismatch"));
            return;
        }
        QList<QSslError> ignorable;
        for (const QSslError& error : errors) {
            if (error.error() != QSslError::CertificateExpired
                && error.error() != QSslError::CertificateNotYetValid) {
                ignorable.append(error);
            }
        }
        socket->ignoreSslErrors(ignorable);
    });
    connect(socket, &QWebSocket::connected, this, [this, socket, generation]() {
        if (!current(socket, generation) || m_attachSent) { return; }
        // A successful TLS handshake may emit no sslErrors at all. Inspect
        // this socket's actual peer certificate before disclosing the ticket.
        if (!freshPinMatches(socket)) {
            finish(QStringLiteral("watch certificate mismatch"));
            return;
        }
        QByteArray attach(kAttachBytes, '\0');
        attach[0] = char(1);
        for (int i = 0; i < kTicketBytes; ++i) { attach[1 + i] = m_ticket.at(i); }
        m_ticket.fill('\0');
        m_ticket.clear();
        m_attachSent = true;
        const qsizetype attachSize = attach.size();
        const QPointer<TxWatchClient> self(this);
        const quint64 revision = m_revision;
        qint64 sent = -1;
        m_telemetry.submittedPayloadBytes += quint64(attachSize);
#ifdef NEREUS_BUILD_TESTS
        const BinaryWriterForTesting writer = m_binaryWriterForTesting;
        if (writer) {
            sent = writer(socket, attach);
        } else {
            sent = socket->sendBinaryMessage(attach);
        }
#else
        sent = socket->sendBinaryMessage(attach);
#endif
        attach.fill('\0');
        // sendBinaryMessage may synchronously emit errorOccurred, whose
        // closed handler can delete us or open a new socket. Never finish
        // that newer attempt or arm its acknowledgement deadline here.
        if (!self || self->m_revision != revision
            || !self->current(socket, generation)) {
            return;
        }
        if (sent != attachSize) {
            self->finish(QStringLiteral("watch ticket send failed"));
            return;
        }
        if (!self->m_ready) {
            self->m_deadline->start(self->m_acknowledgementMs);
        }
    });
    connect(socket, &QWebSocket::binaryMessageReceived, this,
            [this, socket, generation](const QByteArray& message) {
        if (!current(socket, generation)) { return; }
        receiveAck(message);
    });
    connect(socket, &QWebSocket::textMessageReceived, this,
            [this, socket, generation](const QString&) {
        if (current(socket, generation)) {
            finish(QStringLiteral("unexpected watch text"));
        }
    });
    connect(socket, &QWebSocket::disconnected, this, [this, socket, generation]() {
        if (current(socket, generation)) {
            finish(QStringLiteral("watch disconnected"));
        }
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(socket, &QWebSocket::errorOccurred, this,
#else
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
#endif
            [this, socket, generation](QAbstractSocket::SocketError) {
        if (current(socket, generation)) {
            finish(QStringLiteral("watch connection failed"));
        }
    });
    m_deadline->start(m_openingMs);
    socket->open(watchUrl);
    return self && self->current(socket, generation);
}

bool TxWatchClient::openRelay(DataChannelTransport* transport,
                              const QByteArray& actualCorePinSha256,
                              const QByteArray& rawTicket, quint64 primaryGeneration)
{
    if (transport != nullptr && m_relay == transport) { return false; }
    // Only a dedicated client-side watch peer can be transferred here. In
    // particular, never close a mistakenly supplied Control primary.
    if (transport != nullptr
        && (!transport->carriesBinary()
            || transport->role() != DataChannelTransport::Role::Offerer)) {
        emit closed(primaryGeneration, QStringLiteral("invalid watch transport"));
        return false;
    }
    // The supplied peer is consumed even when an old close handler replaces
    // this helper before the new attempt can start.
    QPointer<DataChannelTransport> incoming(transport);
    if (incoming) { incoming->setParent(nullptr); }
    const QPointer<TxWatchClient> self(this);
    const quint64 revision = ++m_revision;
    if (m_active) { finish(QStringLiteral("watch replaced")); }
    if (!self || self->m_revision != revision || self->m_active) {
        retireRelay(incoming);
        return false;
    }
    m_generation = primaryGeneration;
    const bool valid = incoming && actualCorePinSha256.size() == kTicketBytes
        && rawTicket.size() == kTicketBytes && primaryGeneration != 0;
    if (!valid) {
        retireRelay(incoming);
        if (self) {
            emit closed(primaryGeneration, QStringLiteral("invalid watch attachment inputs"));
        }
        return false;
    }

    m_telemetry = {};
    m_pin = actualCorePinSha256;
    m_ticket = rawTicket;
    m_active = true;
    m_ready = false;
    m_attachSent = false;
    incoming->setParent(this);
    m_relay = incoming;
    const quint64 generation = m_generation;
    connect(incoming, &DataChannelTransport::opened, this,
            [this, transport, generation]() { attachRelay(transport, generation); });
    connect(incoming, &SessionTransport::binaryReceived, this,
            [this, transport, generation](const QByteArray& message) {
        if (current(transport, generation)) { receiveAck(message); }
    });
    connect(incoming, &SessionTransport::textReceived, this,
            [this, transport, generation](const QByteArray&) {
        if (current(transport, generation)) { finish(QStringLiteral("unexpected watch text")); }
    });
    connect(incoming, &SessionTransport::closed, this,
            [this, transport, generation]() {
        if (current(transport, generation)) { finish(QStringLiteral("watch disconnected")); }
    });
    connect(incoming, &DataChannelTransport::failed, this,
            [this, transport, generation](const QString&) {
        if (current(transport, generation)) { finish(QStringLiteral("watch connection failed")); }
    });
    m_deadline->start(m_openingMs);
    if (incoming->isOpen()) { attachRelay(transport, generation); }
    return self && self->current(transport, generation);
}

void TxWatchClient::attachRelay(DataChannelTransport* transport, quint64 generation)
{
    if (!current(transport, generation) || !transport->isOpen() || m_attachSent) { return; }
    // The DTLS peer's actual presented certificate, after the handshake,
    // must match the current authenticated primary digest. SDP is not proof.
    const QByteArray presentedPin = transport->peerCertificateSha256();
    if (presentedPin.size() != kTicketBytes || presentedPin != m_pin) {
        finish(QStringLiteral("watch certificate mismatch"));
        return;
    }
    QByteArray attach(kAttachBytes, '\0');
    attach[0] = char(1);
    for (int i = 0; i < kTicketBytes; ++i) { attach[1 + i] = m_ticket.at(i); }
    m_ticket.fill('\0');
    m_ticket.clear();
    m_attachSent = true;
    const QPointer<TxWatchClient> self(this);
    const quint64 revision = m_revision;
    bool sent = false;
    m_telemetry.submittedPayloadBytes += quint64(attach.size());
#ifdef NEREUS_BUILD_TESTS
    const RelayWriterForTesting writer = m_relayWriterForTesting;
    sent = writer ? writer(transport, attach) : transport->sendBinary(attach);
#else
    sent = transport->sendBinary(attach);
#endif
    attach.fill('\0');
    if (!self || self->m_revision != revision
        || !self->current(transport, generation)) { return; }
    if (!sent) {
        self->finish(QStringLiteral("watch ticket send failed"));
        return;
    }
    if (!self->m_ready) { self->m_deadline->start(self->m_acknowledgementMs); }
}

bool TxWatchClient::sendKeepalive(quint64 sequence, quint32 epoch)
{
    const QPointer<TxWatchClient> self(this);
    DataChannelTransport* relay = m_relay.data();
    QWebSocket* socket = m_socket.data();
    if (!m_ready || (relay == nullptr && socket == nullptr)
        || (relay != nullptr && !relay->isOpen())
        || (socket != nullptr && socket->state() != QAbstractSocket::ConnectedState)) {
        finish(QStringLiteral("watch unavailable"));
        return false;
    }
    const QByteArray frame = RemoteTxWatchdog::channelKeepalive(sequence, epoch);
    qint64 backlog = relay != nullptr ? relay->backlogBytes() : socket->bytesToWrite();
#ifdef NEREUS_BUILD_TESTS
    if (m_testBacklogBytes >= 0) { backlog = m_testBacklogBytes; }
#endif
    if (backlog > kMaxOutboundBacklog - frame.size()) {
        finish(QStringLiteral("watch send backlog or failure"));
        return false;
    }
    const quint64 generation = m_generation;
    const quint64 revision = m_revision;
    qint64 sent = -1;
    m_telemetry.submittedPayloadBytes += quint64(frame.size());
    if (relay != nullptr) {
        bool accepted = false;
#ifdef NEREUS_BUILD_TESTS
        const RelayWriterForTesting writer = m_relayWriterForTesting;
        accepted = writer ? writer(relay, frame) : relay->sendBinary(frame);
#else
        accepted = relay->sendBinary(frame);
#endif
        if (!self || self->m_revision != revision
            || !self->current(relay, generation) || !self->m_ready) {
            return false;
        }
        if (!accepted) {
            self->finish(QStringLiteral("watch send backlog or failure"));
            return false;
        }
        return true;
    }
#ifdef NEREUS_BUILD_TESTS
    const BinaryWriterForTesting writer = m_binaryWriterForTesting;
    if (writer) {
        sent = writer(socket, frame);
    } else {
        sent = socket->sendBinaryMessage(frame);
    }
#else
    sent = socket->sendBinaryMessage(frame);
#endif
    if (!self || self->m_revision != revision
        || !self->current(socket, generation) || !self->m_ready) {
        return false;
    }
    if (sent != frame.size()) {
        self->finish(QStringLiteral("watch send backlog or failure"));
        return false;
    }
    return true;
}

void TxWatchClient::close()
{
    finish(QStringLiteral("watch closed"));
}

void TxWatchClient::finish(const QString& reason)
{
    if (!m_active) { return; }
    const QPointer<TxWatchClient> self(this);
    const quint64 revision = m_revision;
    const quint64 generation = m_generation;
    m_active = false;
    m_ready = false;
    m_attachSent = false;
    m_deadline->stop();
    m_ticket.fill('\0');
    m_ticket.clear();
    m_pin.clear();
    const QPointer<QWebSocket> socket = m_socket;
    m_socket = nullptr;
    const QPointer<DataChannelTransport> relay = m_relay;
    m_relay = nullptr;
    if (socket) {
        QObject::disconnect(socket, nullptr, this, nullptr);
        // A closed handler can delete this client while the socket is
        // emitting errorOccurred/disconnected. Detach the sender before
        // emitting closed, so QObject's child teardown cannot delete it
        // mid-signal.
        socket->setParent(nullptr);
        socket->abort();
        if (socket) { socket->deleteLater(); }
    }
    if (relay) {
        QObject::disconnect(relay, nullptr, this, nullptr);
        retireRelay(relay);
    }
    if (self && self->m_revision == revision && !self->m_active) {
        emit closed(generation, reason);
    }
}

} // namespace NereusSDR
