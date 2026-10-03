#pragma once
// =================================================================
// src/core/session/StationOpeningGate.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Core WebSocket opening requests
// (R-IOS-01, R-R3-26).
//
// The front door of the Core's station listener: it accepts each TCP
// connection, runs the TLS handshake, reads the WebSocket opening request
// (the HTTP upgrade), and only then hands the connection to Qt's
// QWebSocketServer (QWebSocketServer::handleConnection), which writes the
// 101 and makes the QWebSocket StationServer adopts.
//
// Why it exists (found on the operator's phone test, 2026-09-25): Qt
// builds the request's URL from the Host header with QUrl::setAuthority
// (QWebSocketHandshakeRequest::readHandshake), and a Host holding an
// IPv6 literal without brackets ("Host: ::1", what Apple's WebSocket API
// sends for ws://[::1]:port/) is not an authority QUrl can read, so the
// request is invalid and QWebSocketServerPrivate::handshakeReceived
// writes no reply at all (read from the Qt 6.8.2 and 6.11 sources, and
// measured on Qt 6.11 and 6.8.3: the connection is closed at once with
// nothing written). The operator's Pi (Qt 6.8.2, Linux) left it open
// with nothing written. Either way no reply ever comes. The Core serves
// one listener and routes nothing by Host, so the gate:
//
//   - reads the Host itself and writes it back in the one form Qt reads
//     (an IPv6 literal in brackets, the port kept when there was one);
//     every form a real client sends opens the session: bracketed IPv6
//     with or without a port, unbracketed IPv6 with or without a port,
//     IPv4 with or without a port, and a host name (labels of 1 to 63
//     letters, digits, hyphens and underscores, none starting or ending
//     with a hyphen);
//   - answers 400 Bad Request itself, and closes, a request it cannot
//     read, or one Qt would close without a word: no Host, more than
//     one, a Host that is none of those forms or that QUrl finds no host
//     in, a request line other than GET <path> HTTP/1.1, a header line
//     Qt's parser refuses or drops (a name that is not a token, a folded
//     line, a control character in a value, more than 100 headers), a
//     first Upgrade header that is not exactly "websocket", a first
//     Connection header without "upgrade", a first Sec-WebSocket-Key
//     that is not 16 bytes, or a Sec-WebSocket-Version that is missing
//     or not a list of numbers. Qt answers the rest: 101, or its own 400
//     for a version number the Core does not speak;
//   - answers 400 itself when Qt closes a request it was handed without
//     writing anything, whatever the reason (OpeningSocket in the .cpp),
//     so no opening ends in silence;
//   - bounds the whole opening (TCP accept to Qt's 101) by one deadline,
//     kDefaultOpeningDeadlineMs, and closes a connection that has not
//     opened by then;
//   - holds a limited number of unfinished openings at once, and a
//     limited number from one address, counted until each opens, is
//     refused or reaches the deadline. A new connection at either limit
//     closes the oldest unfinished opening (that address's, then the
//     oldest of all) and takes its place, so openings that never finish
//     cannot keep a device out, and a client redialling over its own
//     abandoned dials gets its newest through. The limits are
//     StationServer::kMaxUnfinishedOpenings (64; the reasons are there)
//     and StationServer::kMaxHandshakesPerAddress (2), and the address is
//     counted the way StationServer::addressKey counts it (an IPv6
//     address by its /64);
//   - hands an opened session's socket to its QWebSocket (markOpened),
//     so closing the gate closes only what is still opening.
//
// Nothing here trusts or records the Host: it is rewritten only so Qt can
// parse the request, never read back.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25  J.J. Boyd / KG4VCF  Original implementation (R-IOS-01,
//                                    R-R3-26). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Review fixes: Qt's header rules, the
//                                    unanswered-close 400, a 64-opening
//                                    pool, opened sockets handed to their
//                                    QWebSocket. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QPointer>
#include <QSslConfiguration>
#include <QString>
#include <QTcpServer>

#include <functional>
#include <optional>

QT_BEGIN_NAMESPACE
class QSslSocket;
class QTimer;
class QWebSocket;
class QWebSocketServer;
QT_END_NAMESPACE

namespace NereusSDR {

class StationOpeningGate : public QTcpServer {
    Q_OBJECT

public:
    /// TCP accept to 101, TLS handshake included. The value follows Qt's
    /// own QWebSocketServer handshake timeout default (10000 ms in Qt 6.11,
    /// read from QWebSocketServerPrivate's constructor), the bound the
    /// listener ran under before this gate: a phone on a slow cell link
    /// finishes TLS and the request in well under it, and a connection
    /// that has not opened by then is holding a place for nothing.
    static constexpr int kDefaultOpeningDeadlineMs = 10000;
    /// The most an opening request's head may be; a longer one gets 400.
    /// Qt reads the request with lines of at most 8 KiB (handshakeReceived
    /// passes 0x2000 to readHandshake), and StationStatusPage caps its own
    /// request heads at the same 8192.
    static constexpr int kMaxRequestHeadBytes = 8192;

    /// What a request head becomes: the head Qt is given, or a refusal.
    struct Rewrite {
        bool ok = false;
        QByteArray head; // the head to hand Qt, ending "\r\n\r\n", when ok
    };

    /// Pure: reads one opening request head (through its "\r\n\r\n") and
    /// returns it with its Host written in the form Qt reads, or not ok
    /// when the Core cannot read it. Public so the table of forms is
    /// checked without a socket.
    static Rewrite rewriteRequestHead(const QByteArray& head);

    /// The Host value as Qt reads it ("[2001:db8::1]:47910", "127.0.0.1",
    /// "core.local:47910"), or empty when `value` is none of the forms.
    static QString canonicalHost(const QString& value);

    /// Groups addresses the way StationServer::addressKey does; set by the
    /// owner so the two counts cannot drift apart.
    using AddressKey = std::function<QString(const QString& address)>;

    StationOpeningGate(QWebSocketServer* target, int maxOpenings, int maxOpeningsPerAddress,
                       AddressKey addressKey, QObject* parent = nullptr);
    ~StationOpeningGate() override;

    /// The TLS settings every accepted connection runs with.
    void setTlsConfiguration(const QSslConfiguration& tls) { m_tls = tls; }
    QSslConfiguration tlsConfiguration() const { return m_tls; }

    /// The total and per-address limits; values below 1 are ignored.
    /// Applies to connections accepted after the call.
    void setOpeningLimits(int maxOpenings, int maxOpeningsPerAddress);

    /// Values below 1 are ignored. Applies to openings accepted after the
    /// call.
    void setOpeningDeadlineMs(int ms);
    int openingDeadlineMs() const { return m_deadlineMs; }

    /// Openings accepted and not yet opened, refused or timed out.
    int pendingCount() const { return static_cast<int>(m_pending.size()); }

    /// Stops listening and closes every connection still opening.
    void closeAll();

    /// The owner calls this for every QWebSocket Qt hands it, so the
    /// opening that produced it stops counting and its socket moves under
    /// the QWebSocket (closeAll() leaves it alone from then on).
    void markOpened(QWebSocket* socket);

protected:
    void incomingConnection(qintptr descriptor) override;

private:
    struct Pending {
        QPointer<QSslSocket> socket;
        QString addressKey;
        QTimer* deadline = nullptr;
        QByteArray head;
        bool handedToQt = false;
        quint64 serial = 0; // accept order; the oldest makes room first
    };

    void onReadable(QSslSocket* socket);
    void refuse(QSslSocket* socket);
    void release(QSslSocket* socket, bool close);
    /// The oldest opening from `key` (all openings when empty) when that
    /// group is at its limit, else null.
    QSslSocket* oldestPending(const QString& key) const;

    QWebSocketServer* m_target = nullptr;
    QSslConfiguration m_tls;
    int m_maxOpenings = 0;
    int m_maxOpeningsPerAddress = 0;
    AddressKey m_addressKey;
    int m_deadlineMs = kDefaultOpeningDeadlineMs;
    QHash<QSslSocket*, Pending> m_pending;
    quint64 m_serial = 0;
};

} // namespace NereusSDR
