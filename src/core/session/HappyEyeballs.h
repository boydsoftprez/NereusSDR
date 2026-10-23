#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/HappyEyeballs.h  (NereusSDR)
// =================================================================
//
// Happy Eyeballs (RFC 8305) for the WebSockets this computer opens to a
// host NAME: the remote access service, the web relay, and a Core reached
// by a typed name.
//
// Why: QAbstractSocket (under QWebSocket) tries a name's addresses one at a
// time and moves on only after a connect error or its own 30 s timeout. On
// a network whose IPv6 path is silently dead (a Mac whose only global IPv6
// address is deprecated, 2026-10-22), every attempt sat in SYN_SENT to the
// service's AAAA address until our 10 s hello timer gave up, and the next
// attempt started over on IPv6. A browser on the same Mac fell back to IPv4.
//
// What: QWebSocket offers no way to choose the address it connects to, or
// to hand it a connected socket, and the service is reached by name
// through a web server, so the name must stay in the TLS handshake (SNI and
// certificate check) and in the Host header. So, when the computer's own
// proxy settings name no proxy and the host is a name, the WebSocket is
// given a proxy that lives inside this process (HappyEyeballsTunnel): a
// loopback HTTP CONNECT listener for that one host and port. Qt asks it to
// CONNECT to the NAME, keeps the name for TLS and the Host header, and
// verifies the certificate against the name exactly as before. The tunnel
// races the name's addresses (HappyEyeballsConnector): the first address,
// then the next family's 250 ms later (RFC 8305 section 5's Connection
// Attempt Delay), or at once when an attempt fails, the families
// interleaved (section 4, First Address Family Count 1); the first TCP
// connection wins, the others are aborted, and bytes are copied both ways
// with bounded buffers. An address literal, or a computer whose settings
// name a proxy (which resolves the name itself), is left as before.
//
// =================================================================
// No Nagle on either tunnel socket (the accepted loopback connection and
// the connection to the far end), so a leg that turns Nagle off on its
// WebSocket (RelayLeg, section 12.5) keeps it off end to end. A failed race
// answers the CONNECT with the status Qt maps to the real cause
// (failureStatusLine), not one it reports as a proxy fault.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-22: no Nagle on either tunnel socket; failure statuses Qt
//               reports as the real error; the listener a QObject child.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-22: both classes exported from the Core DLL (Windows: their
//               staticMetaObject, for signals seen from outside it).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QNetworkProxy>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include <functional>

class QWebSocket;

namespace NereusSDR {

namespace HappyEyeballs {

/// RFC 8305 section 5: the recommended Connection Attempt Delay.
constexpr int kConnectionAttemptDelayMs = 250;
/// The most the tunnel holds for a side that is not reading (each way).
constexpr qint64 kPipeHighWaterBytes = 256 * 1024;
/// The longest CONNECT request the tunnel reads.
constexpr int kMaxRequestBytes = 8192;

/// RFC 8305 section 4: the addresses in their resolver order, the
/// families interleaved, starting with the family of the first.
QList<QHostAddress> interleaveFamilies(const QList<QHostAddress>& addresses);

/// Process-local test seam: the addresses a name resolves to, instead of
/// the system resolver. Takes effect only in a test run (QStandardPaths
/// test mode); an empty function restores the resolver.
using Resolver = std::function<QList<QHostAddress>(const QString& host)>;
void setResolverForTest(Resolver resolver);

/// The proxy for a WebSocket `socket` to `url`: the computer's own proxy
/// (SystemProxy::forUrl) when its settings name one; otherwise, for a host
/// name, a HappyEyeballsTunnel made a child of `socket` (so it ends with
/// it), as an HTTP proxy on this computer; otherwise no proxy.
QNetworkProxy proxyForWebSocket(const QUrl& url, QWebSocket* socket);

/// The socket that carries `socket`'s bytes to its far end when it runs
/// through a tunnel (for its real local and remote addresses and its
/// options), or null.
QAbstractSocket* upstreamOf(const QWebSocket* socket);

/// The status line a tunnel answers a CONNECT with when the race failed
/// with `error`: the one Qt's HTTP proxy client reports as that error
/// rather than as a proxy fault (see the .cpp for Qt's mapping).
QByteArray failureStatusLine(QAbstractSocket::SocketError error);

} // namespace HappyEyeballs

/// Connects a TCP socket to a host name the RFC 8305 way. One use: start()
/// once, then exactly one of connected() or failed().
class NEREUS_CORE_EXPORT HappyEyeballsConnector final : public QObject {
    Q_OBJECT

public:
    explicit HappyEyeballsConnector(QObject* parent = nullptr);
    ~HappyEyeballsConnector() override;

    void start(const QString& host, quint16 port);

signals:
    /// The first attempt that connected. `socket` is still this object's
    /// child; the receiver re-parents it.
    void connected(QTcpSocket* socket);
    /// The name resolved to nothing, or every address failed.
    void failed(QAbstractSocket::SocketError error);

private:
    void onResolved(const QList<QHostAddress>& addresses);
    void startNextAttempt();
    void onAttemptFailed(QTcpSocket* socket, const QHostAddress& address,
                         QAbstractSocket::SocketError error);
    void onAttemptConnected(QTcpSocket* socket);
    void finishFailed(QAbstractSocket::SocketError error);
    void dropAttempts(QTcpSocket* keep);

    QList<QHostAddress> m_queue;
    QList<QTcpSocket*> m_attempts;
    QTimer m_attemptTimer;
    quint16 m_port = 0;
    int m_lookupId = -1;
    bool m_done = false;
    QAbstractSocket::SocketError m_lastError = QAbstractSocket::HostNotFoundError;
};

/// A loopback HTTP CONNECT proxy for one host and port, for one
/// connection: it accepts the first, answers its CONNECT once
/// HappyEyeballsConnector has a connection, and copies bytes both ways.
class NEREUS_CORE_EXPORT HappyEyeballsTunnel final : public QObject {
    Q_OBJECT

public:
    HappyEyeballsTunnel(const QString& host, quint16 port, QObject* parent = nullptr);
    ~HappyEyeballsTunnel() override;

    bool isListening() const { return m_server->isListening(); }
    /// The HTTP proxy at this tunnel's loopback listener.
    QNetworkProxy proxy() const;
    /// The connection to the far end, once one won; null before.
    QTcpSocket* upstream() const { return m_upstream.data(); }

private:
    void onNewConnection();
    void onClientReadable();
    void onUpstreamConnected(QTcpSocket* socket);
    void onUpstreamFailed(QAbstractSocket::SocketError error);
    void pump(QTcpSocket* from, QTcpSocket* to);
    void onSideDisconnected(QTcpSocket* from, QTcpSocket* to);
    void answerAndClose(const QByteArray& statusLine);
    void shut();
    bool targetMatches(const QByteArray& authority) const;

    enum class Stage { Listening, ReadingRequest, Connecting, Piping, Closed };

    QString m_host;
    quint16 m_port = 0;
    // A child, so it moves threads with the tunnel.
    QTcpServer* m_server = nullptr;
    QPointer<QTcpSocket> m_client;
    QPointer<QTcpSocket> m_upstream;
    QPointer<HappyEyeballsConnector> m_connector;
    QByteArray m_request;
    Stage m_stage = Stage::Listening;
};

} // namespace NereusSDR
