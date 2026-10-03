#pragma once
// =================================================================
// src/core/session/SessionTransport.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 18.
//
// One session's byte pipe, abstracted away from what is carrying it.
// StationServer and StationClient hold this, never a QWebSocket, for two
// reasons that both turned out to be load-bearing rather than tidiness:
//
//   1. **The protocol half of this task has to be testable without TLS.**
//      QSslSocket::supportsSsl() is false on a Qt build with no working
//      TLS backend, and a suite that put every handshake, preemption and
//      ordering assertion behind a wss socket would silently QSKIP its
//      way to green on such a build. With this seam the message-order and
//      handshake assertions run over an in-process pipe unconditionally,
//      and only the genuinely TLS-specific slots skip.
//
//   2. **A silently dead peer cannot be simulated over a real socket.**
//      Task 19 step 3a exists to prove the heartbeat detects a peer that
//      stops answering WITHOUT closing -- a laptop lid, a cell handoff, a
//      NAT timeout. Over a loopback QWebSocket the peer's Qt stack answers
//      every ping automatically, so the case is unreachable; over this
//      seam a test transport simply stops emitting pongReceived(). Same
//      production code path either way -- there is no second, test-only
//      branch through StationServer.
//
// The abstraction is deliberately RFC 6455 shaped (text frames, ping,
// pong, close) rather than a generic byte stream, because that is what
// the transport under it actually is and pretending otherwise would mean
// re-inventing framing above it. R3's compact native envelope rides in
// the same text/binary frames; nothing here needs to change for it.
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
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 18 (R-IOS-08):
//                                    peerCertificateSha256(). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-10-01  J.J. Boyd / KG4VCF  Control logging lane: linkDiagnostics()
//                                    and deliveringMessageWaitUs(), read
//                                    for the Core's log only. AI-assisted
//                                    via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QString>
#include <optional>
#include "core/session/NetworkPathSnapshot.h"

QT_BEGIN_NAMESPACE
class QWebSocket;
QT_END_NAMESPACE

namespace NereusSDR {

// UTF-8 application payload totals, not TLS/WebSocket wire traffic and not
// media bandwidth. Pong RTT is observational; it never drives liveness.
struct SessionTransportTelemetry {
    quint64 receivedPayloadBytes = 0;
    quint64 acceptedPayloadBytes = 0;
    std::optional<quint64> pongRttMs;
    std::optional<qint64> pongAgeMs;
};

// Control logging lane: what a transport can say about its own link, read
// on its owner thread for the Core's log only. Nothing here changes a
// socket option or what is sent.
struct SessionLinkDiagnostics {
    enum class Buffer {
        /// A transport that does not say what its backlog is.
        Unknown,
        /// A data channel's bufferedAmount (bytes the SCTP stack holds).
        DataChannel,
        /// Bytes Qt holds for a WebSocket's TCP socket, not yet written to
        /// the kernel.
        WebSocket,
    };
    Buffer buffer = Buffer::Unknown;
    qint64 bufferedBytes = 0;
    /// TCP_NODELAY as the socket has it, where there is a TCP socket.
    std::optional<bool> noDelay;
    /// A data channel's SCTP round-trip estimate.
    std::optional<quint32> sctpRttMs;
    // The kernel's TCP figures (Linux only).
    std::optional<quint32> tcpRttUs;
    std::optional<quint32> tcpUnacked;
    std::optional<quint32> tcpRetransmits;
    std::optional<quint32> tcpNotSentBytes;
};

class SessionTransport : public QObject {
    Q_OBJECT

public:
    explicit SessionTransport(QObject* parent = nullptr) : QObject(parent) {}
    ~SessionTransport() override = default;

    /// One complete session message, already encoded (SessionMessages::
    /// encode). Dropped silently when the link is not open: a caller
    /// racing a close is normal, not an error worth propagating up into
    /// the protocol layer.
    virtual void sendText(const QByteArray& wire) = 0;

    /// RFC 6455 ping. The peer's WebSocket implementation answers it
    /// without involving its application code at all, which is precisely
    /// what makes a pong evidence about the LINK rather than about the
    /// peer's event loop having got around to us.
    virtual void ping() = 0;

    /// Close the link, telling the peer why where the transport can carry
    /// a reason. Idempotent.
    virtual void closeLink(const QString& reason) = 0;

    virtual bool isOpen() const = 0;

    /// Human-readable peer identification for logs ("127.0.0.1:54321").
    /// Never used as an identity for authorisation.
    virtual QString peerDescription() const = 0;

    /// iPhone app Task 12: the peer's address alone ("192.0.2.7",
    /// "2001:db8::7"), what device sign-in rate-limits by and records as a
    /// device's last address. Never an identity. Empty where there is no
    /// address of the peer's own to give: a relayed connection shares the
    /// relay's address, so a relay transport (Part E) returns empty.
    virtual QString peerAddress() const { return {}; }

    /// iPhone app Task 18: SHA-256 of the DER certificate the far end
    /// presented on this connection's TLS, 32 bytes; empty when the link
    /// carries no TLS (ws://, an in-process link). What a device checks
    /// the Core's certificate binding against (the link document, section
    /// 3.4) and signs into its sign-in transcript (section 3.5).
    virtual QByteArray peerCertificateSha256() const { return {}; }

    /// Read only on the transport's owner thread. Unsupported test/custom
    /// transports return absent rather than a fabricated zero measurement.
    virtual std::optional<SessionTransportTelemetry> telemetry() const { return std::nullopt; }

    /// Present route on this transport's Qt thread; unsupported or closed
    /// transports return unavailable rather than an inferred old path.
    virtual std::optional<NetworkPathSnapshot> networkPathSnapshot() const
    {
        return std::nullopt;
    }

    /// iPhone app plan Task 29 step 2b (the link document, "Paths", the
    /// media tunnel): one binary message beside the session's text ones,
    /// for the media connection's datagrams on a direct WebSocket. False
    /// where the transport carries none (a data channel), or it is not
    /// open. A peer that does not use them ignores binary messages (the
    /// station always has, section 2).
    virtual bool sendBinary(const QByteArray& message)
    {
        Q_UNUSED(message);
        return false;
    }
    virtual bool carriesBinary() const { return false; }
    /// Bytes waiting to be written, where the transport can say (the media
    /// tunnel writes only while little waits, as the web relay's leg).
    virtual qint64 backlogBytes() const { return 0; }

    /// Control logging lane: the link's buffers and, where there is one,
    /// its TCP socket's figures. For the log only.
    virtual SessionLinkDiagnostics linkDiagnostics() const
    {
        SessionLinkDiagnostics link;
        link.bufferedBytes = backlogBytes();
        return link;
    }
    /// Control logging lane: while textReceived() is being emitted, how
    /// long (microseconds) the message waited between the transport's
    /// receipt of it off the network and its delivery on this thread.
    /// Absent where the transport reads on this thread (no separate
    /// receipt) or does not measure it. For the log only.
    virtual std::optional<qint64> deliveringMessageWaitUs() const { return std::nullopt; }

signals:
    void textReceived(const QByteArray& wire);
    /// Step 2b: one binary message (sendBinary()).
    void binaryReceived(const QByteArray& message);

    /// A pong came back for one of our pings. This is the ONLY liveness
    /// evidence StationServer/StationClient accept -- see their heartbeat
    /// comments for why an arbitrary inbound frame is deliberately not
    /// treated as equivalent.
    void pongReceived();

    void closed();
};

/// The production transport: a QWebSocket, which the caller hands over and
/// this object then owns (Qt parent-ownership). Used by StationServer for
/// each accepted connection and by StationClient for its one outbound
/// connection.
class WebSocketTransport : public SessionTransport {
    Q_OBJECT

public:
    /// Takes ownership of `socket` by reparenting it onto this object, so
    /// a caller cannot accidentally outlive-or-be-outlived by the socket
    /// it just wrapped.
    ///
    /// `maxIncomingBytes` caps one inbound WebSocket message, and one
    /// inbound frame, on this socket. It is REQUIRED rather than defaulted
    /// on purpose. Qt's own defaults are roughly INT_MAX, about 2 GiB per
    /// message, and Qt buffers a whole message before it emits
    /// textMessageReceived, so an uncapped accepted socket lets a peer
    /// that has not authenticated anything allocate gigabytes on a daemon
    /// whose stated hardware floor is a Pi 4. Making the parameter
    /// mandatory means a future call site has to state a number rather
    /// than inherit a fatal default by omission. The two production
    /// numbers are StationServer::kMaxIncomingMessageBytes and
    /// StationClient::kMaxIncomingMessageBytes, which differ by two orders
    /// of magnitude because the two directions carry different traffic;
    /// see each constant for the arithmetic behind it.
    explicit WebSocketTransport(QWebSocket* socket, quint64 maxIncomingBytes,
                                QObject* parent = nullptr);
    ~WebSocketTransport() override;

    void sendText(const QByteArray& wire) override;
    void ping() override;
    void closeLink(const QString& reason) override;
    bool isOpen() const override;
    QString peerDescription() const override;
    QString peerAddress() const override;
    QByteArray peerCertificateSha256() const override;
    std::optional<SessionTransportTelemetry> telemetry() const override;
    std::optional<NetworkPathSnapshot> networkPathSnapshot() const override;
    bool sendBinary(const QByteArray& message) override;
    bool carriesBinary() const override { return true; }
    qint64 backlogBytes() const override;
    SessionLinkDiagnostics linkDiagnostics() const override;

    QWebSocket* socket() const { return m_socket; }

private:
    // Raw, not QPointer, and safe for one specific reason: the
    // constructor reparents the socket onto this object, so it cannot
    // outlive us and cannot be destroyed independently of us. A
    // QPointer here would additionally force this header to include
    // <QWebSocket> rather than forward-declaring it, because QPointer's
    // own accessors need the complete type.
    QWebSocket* m_socket = nullptr;
    bool m_closing = false;
    SessionTransportTelemetry m_telemetry;
    QElapsedTimer m_pongAge;
};

} // namespace NereusSDR
