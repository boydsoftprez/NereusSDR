// =================================================================
// src/core/session/SessionTransport.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 18.
// See SessionTransport.h for the design rationale.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08  J.J. Boyd / KG4VCF  Remote daemon R2 Task 18: session
//                                    transport seam and its QWebSocket
//                                    implementation. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 12 (R-IOS-08):
//                                    peerAddress(). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-10-01  J.J. Boyd / KG4VCF  Control logging lane:
//                                    WebSocketTransport::linkDiagnostics()
//                                    (TCP_INFO and unsent bytes on Linux,
//                                    read only). AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include "core/session/SessionTransport.h"

#include <QCryptographicHash>
#include <QHostAddress>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QAbstractSocket>
#include <QWebSocket>
#include <QThread>
#include <QVariant>

#ifdef Q_OS_LINUX
#include <linux/sockios.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif

namespace NereusSDR {

std::optional<NetworkPathSnapshot> socketNetworkPathSnapshot(
    const QWebSocket* socket, NetworkPathSnapshot::Carrier carrier)
{
    if (!socket || socket->thread() != QThread::currentThread()
        || socket->state() != QAbstractSocket::ConnectedState) {
        return std::nullopt;
    }
    const auto numeric = [](QHostAddress address) {
        if (address.isNull()) { return QString(); }
        bool ipv4 = false;
        const quint32 v4 = address.toIPv4Address(&ipv4);
        if (ipv4) { address = QHostAddress(v4); }
        address.setScopeId(QString());
        return address.toString();
    };
    NetworkPathSnapshot path;
    path.kind = carrier == NetworkPathSnapshot::Carrier::WebRelay
                    ? NetworkPathSnapshot::Kind::Relayed : NetworkPathSnapshot::Kind::Direct;
    path.carrier = carrier;
    path.endpoints = NetworkPathSnapshot::Endpoints::Socket;
    path.localAddress = numeric(socket->localAddress());
    path.localPort = socket->localPort();
    path.remoteAddress = numeric(socket->peerAddress());
    path.remotePort = socket->peerPort();
    return path;
}

WebSocketTransport::WebSocketTransport(QWebSocket* socket, quint64 maxIncomingBytes,
                                       QObject* parent)
    : SessionTransport(parent)
    , m_socket(socket)
{
    if (m_socket == nullptr) {
        return;
    }
    m_socket->setParent(this);

    // Applied HERE, in the wrapper's constructor, so it is impossible to
    // hold a session transport whose socket was never capped. On the
    // daemon side this runs inside the newConnection slot, before control
    // returns to the event loop, so no inbound frame can have been
    // processed on this socket yet.
    //
    // Message AND frame, and in that order: the frame cap must not exceed
    // the message cap, and Qt's default message cap is the larger of the
    // two, so lowering the message cap first keeps the pair consistent at
    // every instant. Capping only the message would still let a single
    // oversized FRAME be buffered on the way to a message that is then
    // rejected.
    //
    // QWebSocketServer has no equivalent setter in Qt 6.11 (checked
    // against qwebsocketserver.h), which is why this is per-socket rather
    // than configured once on the listener. TciServer.cpp:1470 does the
    // same thing for the same reason.
    m_socket->setMaxAllowedIncomingMessageSize(maxIncomingBytes);
    m_socket->setMaxAllowedIncomingFrameSize(maxIncomingBytes);

    connect(m_socket, &QWebSocket::textMessageReceived, this,
            [this](const QString& message) {
        const QByteArray wire = message.toUtf8();
        m_telemetry.receivedPayloadBytes += static_cast<quint64>(wire.size());
        emit textReceived(wire);
    });

    // QWebSocket::pong carries the round-trip time and the payload we sent.
    // Elapsed time is an observation for the diagnostics UI only. Heartbeat
    // still consumes the unchanged arrival-only pongReceived signal: a slow
    // RTT must never be treated as a missing pong.
    connect(m_socket, &QWebSocket::pong, this,
            [this](quint64 elapsedMs, const QByteArray&) {
        m_telemetry.pongRttMs = elapsedMs;
        m_pongAge.start();
        emit pongReceived();
    });

    connect(m_socket, &QWebSocket::disconnected, this,
            [this]() { emit closed(); });

    // Task 29 step 2b: binary messages carry the media tunnel's datagrams
    // (the message cap above applies to them as to text).
    connect(m_socket, &QWebSocket::binaryMessageReceived, this,
            [this](const QByteArray& message) {
        emit binaryReceived(message);
    });
}

bool WebSocketTransport::sendBinary(const QByteArray& message)
{
    if (!isOpen() || m_closing) {
        return false;
    }
    if (m_socket->sendBinaryMessage(message) < 0) {
        return false;
    }
    return true;
}

qint64 WebSocketTransport::backlogBytes() const
{
    if (m_socket == nullptr) {
        return 0;
    }
    if (const auto* inner = m_socket->findChild<QAbstractSocket*>()) {
        return inner->bytesToWrite();
    }
    return 0;
}

SessionLinkDiagnostics WebSocketTransport::linkDiagnostics() const
{
    SessionLinkDiagnostics link;
    link.buffer = SessionLinkDiagnostics::Buffer::WebSocket;
    if (m_socket == nullptr) {
        return link;
    }
    // socketOption() is not const in Qt; it only reads here.
    auto* inner = m_socket->findChild<QAbstractSocket*>();
    if (inner == nullptr) {
        return link;
    }
    link.bufferedBytes = inner->bytesToWrite();
    if (inner->state() != QAbstractSocket::ConnectedState) {
        return link;
    }
    // Read, never set: the socket's options are left as they are.
    const QVariant noDelay = inner->socketOption(QAbstractSocket::LowDelayOption);
    if (noDelay.isValid()) {
        link.noDelay = noDelay.toInt() != 0;
    }
#ifdef Q_OS_LINUX
    const qintptr descriptor = inner->socketDescriptor();
    if (descriptor >= 0) {
        const int fd = static_cast<int>(descriptor);
        struct tcp_info info {};
        socklen_t length = sizeof(info);
        if (::getsockopt(fd, IPPROTO_TCP, TCP_INFO, &info, &length) == 0) {
            link.tcpRttUs = info.tcpi_rtt;
            link.tcpUnacked = info.tcpi_unacked;
            link.tcpRetransmits = info.tcpi_total_retrans;
        }
        int notSent = 0;
        if (::ioctl(fd, SIOCOUTQNSD, &notSent) == 0 && notSent >= 0) {
            link.tcpNotSentBytes = static_cast<quint32>(notSent);
        }
    }
#endif
    return link;
}

WebSocketTransport::~WebSocketTransport() = default;

void WebSocketTransport::sendText(const QByteArray& wire)
{
    if (!isOpen()) {
        return;
    }
    if (m_socket->sendTextMessage(QString::fromUtf8(wire)) >= 0) {
        m_telemetry.acceptedPayloadBytes += static_cast<quint64>(wire.size());
    }
}

void WebSocketTransport::ping()
{
    if (!isOpen()) {
        return;
    }
    // Qt6's QWebSocket::ping builds the RFC 6455 frame; the payload is
    // echoed back verbatim in the pong. Kept short and constant: it is
    // never inspected (see the pong connect above), and a payload that
    // varied per ping would only invite someone to start matching them up
    // and rebuilding a round-trip timer this design deliberately does not
    // have.
    m_socket->ping(QByteArrayLiteral("nereus"));
}

void WebSocketTransport::closeLink(const QString& reason)
{
    if (m_socket == nullptr || m_closing) {
        return;
    }
    m_closing = true;
    // CloseCodeNormal, not an error code: every close this class issues is
    // a deliberate protocol decision (version refusal, failed auth,
    // preemption, heartbeat timeout) that has already been explained to
    // the peer in a SessionEnd or AuthResult message. The reason string
    // rides along for a peer that is reading close frames rather than
    // messages.
    m_socket->close(QWebSocketProtocol::CloseCodeNormal, reason);
}

bool WebSocketTransport::isOpen() const
{
    return m_socket != nullptr && m_socket->state() == QAbstractSocket::ConnectedState;
}

QString WebSocketTransport::peerDescription() const
{
    if (m_socket == nullptr) {
        return QStringLiteral("<detached>");
    }
    return QStringLiteral("%1:%2")
        .arg(m_socket->peerAddress().toString())
        .arg(m_socket->peerPort());
}

QString WebSocketTransport::peerAddress() const
{
    if (m_socket == nullptr) {
        return {};
    }
    QHostAddress address = m_socket->peerAddress();
    // A dual-stack listener sees an IPv4 peer as ::ffff:a.b.c.d; it is one
    // address, recorded one way.
    bool isV4 = false;
    const quint32 v4 = address.toIPv4Address(&isV4);
    if (isV4) {
        address = QHostAddress(v4);
    }
    address.setScopeId(QString());
    return address.isNull() ? QString() : address.toString();
}

QByteArray WebSocketTransport::peerCertificateSha256() const
{
    if (m_socket == nullptr) {
        return {};
    }
    const QSslCertificate certificate = m_socket->sslConfiguration().peerCertificate();
    return certificate.isNull() ? QByteArray()
                                : certificate.digest(QCryptographicHash::Sha256);
}

std::optional<SessionTransportTelemetry> WebSocketTransport::telemetry() const
{
    if (!isOpen()) { return std::nullopt; }
    SessionTransportTelemetry snapshot = m_telemetry;
    if (m_pongAge.isValid()) { snapshot.pongAgeMs = m_pongAge.elapsed(); }
    return snapshot;
}

std::optional<NetworkPathSnapshot> WebSocketTransport::networkPathSnapshot() const
{
    if (thread() != QThread::currentThread() || m_closing) {
        return std::nullopt;
    }
    return socketNetworkPathSnapshot(m_socket, NetworkPathSnapshot::Carrier::WebSocket);
}

} // namespace NereusSDR
