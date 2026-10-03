// =================================================================
// src/core/session/StationOpeningGate.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See StationOpeningGate.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25  J.J. Boyd / KG4VCF  Original implementation (R-IOS-01,
//                                    R-R3-26). AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include "core/session/StationOpeningGate.h"

#include <QLoggingCategory>
#include <QSslSocket>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <QWebSocketServer>

Q_LOGGING_CATEGORY(lcStationOpening, "nereus.station.opening")

namespace NereusSDR {

namespace {

// What a request the Core cannot read is told. The body is for a person
// reading it in a browser or a log; clients act on the status alone.
const QByteArray kBadRequestBody =
    QByteArrayLiteral("The Core could not read this connection request.\n");

QByteArray badRequest()
{
    return QByteArrayLiteral("HTTP/1.1 400 Bad Request\r\n"
                             "Content-Type: text/plain; charset=utf-8\r\n"
                             "Content-Length: ")
           + QByteArray::number(kBadRequestBody.size())
           + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + kBadRequestBody;
}

bool isPort(const QString& text)
{
    if (text.isEmpty() || text.size() > 5) {
        return false;
    }
    for (const QChar c : text) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9')) {
            return false;
        }
    }
    return text.toInt() <= 65535;
}

// A host name as DNS and mDNS write them: labels of 1 to 63 letters,
// digits, hyphens and underscores, split by single dots, no label starting
// or ending with a hyphen, 253 characters at most. Nothing that could end
// the header line or start a path, a user name or a port. An IPv4 literal
// is such a name too. The hyphen and length rules are QUrl's (measured on
// Qt 6.8.3 and 6.11: "-core.local", "core-.local", "a.-b" and a label of
// 64 have no host), so a name that passes here is one Qt reads; the
// underscore is kept because QUrl reads it ("Shack_Core.lan"). A trailing
// dot ("core.local.") is refused: its last label is empty.
bool isHostName(const QString& text)
{
    if (text.isEmpty() || text.size() > 253) {
        return false;
    }
    const QStringList labels = text.split(QLatin1Char('.'));
    for (const QString& label : labels) {
        if (label.isEmpty() || label.size() > 63 || label.startsWith(QLatin1Char('-'))
            || label.endsWith(QLatin1Char('-'))) {
            return false;
        }
        for (const QChar c : label) {
            const bool ok = (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
                            || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'))
                            || (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
                            || c == QLatin1Char('-') || c == QLatin1Char('_');
            if (!ok) {
                return false;
            }
        }
    }
    return true;
}

// Whether Qt will find a host in this Host value: it builds the request's
// URL exactly this way (QWebSocketHandshakeRequest::readHandshake, Qt 6.8.2
// and 6.11: setAuthority on a relative URL, then requestUrl().host()), and
// a request whose URL has no host, or has a user name, is closed without
// a reply. The rules above already keep to what QUrl reads; this is the
// same check Qt makes, run by the same library, so the two cannot differ
// on a form nobody tested (an ACE label QUrl cannot decode, say).
bool qtReadsHost(const QString& canonical)
{
    QUrl url = QUrl::fromEncoded(QByteArrayLiteral("/"));
    url.setAuthority(canonical);
    return !url.host().isEmpty() && url.userName().isNull();
}

// An IPv6 literal, written without its zone: the Core reads nothing from
// it, and "fe80::1%en0" is not something QUrl reads inside brackets.
std::optional<QString> ipv6Literal(const QString& text)
{
    if (!text.contains(QLatin1Char(':'))) {
        return std::nullopt;
    }
    QHostAddress address;
    if (!address.setAddress(text) || address.protocol() != QAbstractSocket::IPv6Protocol) {
        return std::nullopt;
    }
    address.setScopeId(QString());
    return address.toString();
}

QString withPort(const QString& host, const QString& port)
{
    return port.isEmpty() ? host : host + QLatin1Char(':') + port;
}

// Case-insensitive comma list membership, for Connection: keep-alive, Upgrade.
bool listHasToken(const QByteArray& value, const QByteArray& token)
{
    const QList<QByteArray> items = value.split(',');
    for (const QByteArray& item : items) {
        if (item.trimmed().compare(token, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

// RFC 9110 section 5.6.2: a header name is a token. Qt's header parser
// refuses a line whose name is not one ("Host : x", "X@A: 1"), which
// fails the whole request without a reply.
bool isToken(const QByteArray& text)
{
    if (text.isEmpty()) {
        return false;
    }
    for (const char c : text) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                        || QByteArrayView("!#$%&'*+-.^_`|~").contains(c);
        if (!ok) {
            return false;
        }
    }
    return true;
}

// A header value Qt keeps: no control character but a tab. Qt's parser
// drops a field whose value holds one (QHttpHeaders, measured on Qt 6.8.3
// and 6.11), so an Upgrade or Host written that way would vanish.
bool isFieldValue(const QByteArray& value)
{
    for (const char c : value) {
        const auto u = static_cast<unsigned char>(c);
        if ((u < 0x20 && c != '\t') || u == 0x7F) {
            return false;
        }
    }
    return true;
}

// One Sec-WebSocket-Version line as Qt reads it: comma-separated, empty
// items skipped, every item an unsigned number (anything else makes Qt
// close the request unanswered). Adds the count of numbers to `count`.
bool versionLineIsNumbers(const QByteArray& value, int& count)
{
    const QList<QByteArray> items = value.split(',');
    for (const QByteArray& raw : items) {
        const QByteArray item = raw.trimmed();
        if (item.isEmpty()) {
            continue;
        }
        if (item.size() > 9) {
            return false; // more than an unsigned 32-bit number can hold safely
        }
        for (const char c : item) {
            if (c < '0' || c > '9') {
                return false;
            }
        }
        ++count;
    }
    return true;
}

// Qt's header parser reads at most this many header lines
// (HeaderConstants::MAX_HEADER_FIELDS, qhttpheaderparser_p.h, Qt 6.8.3 and
// 6.11) and fails the request past it.
constexpr int kMaxHeaderLines = 100;

// The gate's own socket: a QSslSocket that notices when Qt closes it
// without having written a byte, and writes the 400 first. Qt closes an
// opening it will not upgrade with pTcpSocket->close() and no reply in
// several places (QWebSocketServerPrivate::handshakeReceived, Qt 6.8.2
// and 6.11: an invalid request, an invalid response, a full queue of
// opened sockets). The gate's own reading keeps a request out of the
// first; this answers whatever is left. Armed only while Qt holds an
// opening: the gate disarms it before closing or aborting the socket
// itself (abort() also runs close()) and once the opening has opened.
class OpeningSocket final : public QSslSocket {
public:
    using QSslSocket::QSslSocket;

    void arm() { m_armed = true; }
    void disarm() { m_armed = false; }

    void close() override
    {
        if (m_armed && !m_wrote && state() == QAbstractSocket::ConnectedState) {
            m_armed = false;
            qCInfo(lcStationOpening)
                << "A connection request was closed without an answer; answered 400.";
            write(badRequest());
        }
        m_armed = false;
        // QSslSocket::close() flushes what was written before it closes,
        // the same way Qt's own 400 reaches the client.
        QSslSocket::close();
    }

protected:
    qint64 writeData(const char* data, qint64 len) override
    {
        if (len > 0) {
            m_wrote = true;
        }
        return QSslSocket::writeData(data, len);
    }

private:
    bool m_armed = false;
    bool m_wrote = false;
};

OpeningSocket* asOpening(QSslSocket* socket)
{
    // Every socket the gate holds is one it made in incomingConnection().
    return static_cast<OpeningSocket*>(socket);
}

} // namespace

// The Host value in the one form Qt reads, by the rules above alone.
static QString canonicalHostForm(const QString& raw)
{
    const QString value = raw.trimmed();
    if (value.isEmpty()) {
        return {};
    }
    // Bracketed IPv6, with or without a port.
    if (value.startsWith(QLatin1Char('['))) {
        const qsizetype close = value.indexOf(QLatin1Char(']'));
        if (close < 0) {
            return {};
        }
        const std::optional<QString> address = ipv6Literal(value.mid(1, close - 1));
        const QString rest = value.mid(close + 1);
        if (!address.has_value()) {
            return {};
        }
        if (rest.isEmpty()) {
            return QLatin1Char('[') + *address + QLatin1Char(']');
        }
        if (!rest.startsWith(QLatin1Char(':')) || !isPort(rest.mid(1))) {
            return {};
        }
        return withPort(QLatin1Char('[') + *address + QLatin1Char(']'), rest.mid(1));
    }
    const qsizetype colons = value.count(QLatin1Char(':'));
    if (colons >= 2) {
        // Unbracketed IPv6, as Apple's WebSocket API sends it: the whole
        // value when it is an address ("::1", "2001:db8::1"), otherwise an
        // address and a port after the last colon ("2001:db8::1:47910").
        // A value that reads both ways ("::1:8080") is taken whole; the
        // Core routes nothing by Host, so either reading serves.
        if (const std::optional<QString> whole = ipv6Literal(value)) {
            return QLatin1Char('[') + *whole + QLatin1Char(']');
        }
        const qsizetype last = value.lastIndexOf(QLatin1Char(':'));
        const std::optional<QString> address = ipv6Literal(value.left(last));
        const QString port = value.mid(last + 1);
        if (!address.has_value() || !isPort(port)) {
            return {};
        }
        return withPort(QLatin1Char('[') + *address + QLatin1Char(']'), port);
    }
    QString host = value;
    QString port;
    if (colons == 1) {
        const qsizetype colon = value.indexOf(QLatin1Char(':'));
        host = value.left(colon);
        port = value.mid(colon + 1);
        if (!isPort(port)) {
            return {};
        }
    }
    // An IPv4 literal or a host name; an IPv4 literal is also a valid
    // host name by these rules, so one check serves both.
    if (!isHostName(host)) {
        return {};
    }
    return withPort(host, port);
}

QString StationOpeningGate::canonicalHost(const QString& raw)
{
    const QString canonical = canonicalHostForm(raw);
    return !canonical.isEmpty() && qtReadsHost(canonical) ? canonical : QString();
}

StationOpeningGate::Rewrite StationOpeningGate::rewriteRequestHead(const QByteArray& head)
{
    // Every rule here is one Qt's own reading applies
    // (QWebSocketHandshakeRequest::readHandshake, Qt 6.8.2 and 6.11), or a
    // stricter one: a head that passes is one Qt answers, with a 101 or its
    // own 400, never one it closes without a word.
    Rewrite result;
    const qsizetype end = head.indexOf("\r\n\r\n");
    if (end < 0 || end + 4 != head.size() || head.size() > kMaxRequestHeadBytes) {
        return result;
    }
    const QList<QByteArray> lines = head.left(end).split('\n');
    const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
    if (requestLine.size() != 3 || requestLine.at(0) != "GET"
        || requestLine.at(2) != "HTTP/1.1" || !requestLine.at(1).startsWith('/')
        || !isFieldValue(requestLine.at(1))) {
        // The target is a path ("origin-form"): Qt takes the host from
        // the Host header only when the target is a relative URL.
        return result;
    }
    if (lines.size() - 1 > kMaxHeaderLines) {
        return result;
    }

    int hostCount = 0;
    int hostLine = -1;
    QString hostValue;
    // Qt reads only the FIRST Upgrade, Connection and Sec-WebSocket-Key
    // header, and every Sec-WebSocket-Version header.
    std::optional<QByteArray> upgrade;
    std::optional<QByteArray> connection;
    std::optional<QByteArray> key;
    int versions = 0;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).endsWith('\r') ? lines.at(i).chopped(1) : lines.at(i);
        const qsizetype colon = line.indexOf(':');
        // No colon, a name that is not a token (which also refuses a
        // folded line, one starting with a space or tab, that Qt would
        // join onto the header before it), or a value Qt would drop.
        if (colon <= 0 || !isToken(line.left(colon))) {
            return result;
        }
        const QByteArray name = line.left(colon).toLower();
        const QByteArray value = line.mid(colon + 1).trimmed();
        if (!isFieldValue(value)) {
            return result;
        }
        if (name == "host") {
            ++hostCount;
            hostLine = static_cast<int>(i);
            hostValue = QString::fromLatin1(value);
        } else if (name == "upgrade") {
            if (!upgrade) {
                upgrade = value;
            }
        } else if (name == "connection") {
            if (!connection) {
                connection = value;
            }
        } else if (name == "sec-websocket-key") {
            if (!key) {
                key = value;
            }
        } else if (name == "sec-websocket-version") {
            if (!versionLineIsNumbers(value, versions)) {
                return result;
            }
        }
    }
    // RFC 9112 section 3.2: a request with no Host, or more than one, is
    // answered 400.
    if (hostCount != 1 || versions == 0) {
        return result;
    }
    // Upgrade exactly "websocket", as Qt compares it; Connection a list
    // holding "upgrade".
    if (!upgrade || upgrade->compare("websocket", Qt::CaseInsensitive) != 0 || !connection
        || !listHasToken(*connection, "upgrade") || !key) {
        return result;
    }
    const auto decoded =
        QByteArray::fromBase64Encoding(*key, QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.decodingStatus != QByteArray::Base64DecodingStatus::Ok
        || decoded.decoded.size() != 16) {
        return result;
    }
    const QString canonical = canonicalHost(hostValue);
    if (canonical.isEmpty()) {
        return result;
    }

    QList<QByteArray> rewritten = lines;
    rewritten[hostLine] = QByteArrayLiteral("Host: ") + canonical.toLatin1() + '\r';
    result.head = rewritten.join('\n') + QByteArrayLiteral("\r\n\r\n");
    result.ok = true;
    return result;
}

StationOpeningGate::StationOpeningGate(QWebSocketServer* target, int maxOpenings,
                                       int maxOpeningsPerAddress, AddressKey addressKey,
                                       QObject* parent)
    : QTcpServer(parent)
    , m_target(target)
    , m_maxOpenings(maxOpenings)
    , m_maxOpeningsPerAddress(maxOpeningsPerAddress)
    , m_addressKey(std::move(addressKey))
{
}

StationOpeningGate::~StationOpeningGate()
{
    closeAll();
}

void StationOpeningGate::closeAll()
{
    close();
    // Every socket still here is one still opening, or refused and
    // closing: an opened session's socket belongs to its QWebSocket
    // (markOpened), so it is not among them. Each is cut loose from this
    // object's handlers first, so none of them runs against a
    // half-destroyed gate, then closed, and deleted from the event loop
    // rather than here, in case this runs inside one of its own signals.
    m_pending.clear();
    const QList<QSslSocket*> sockets = findChildren<QSslSocket*>(Qt::FindDirectChildrenOnly);
    for (QSslSocket* socket : sockets) {
        socket->disconnect(this);
        asOpening(socket)->disarm();
        socket->abort();
        socket->deleteLater();
    }
    const QList<QTimer*> timers = findChildren<QTimer*>(Qt::FindDirectChildrenOnly);
    for (QTimer* timer : timers) {
        timer->stop();
        timer->deleteLater();
    }
}

void StationOpeningGate::setOpeningLimits(int maxOpenings, int maxOpeningsPerAddress)
{
    if (maxOpenings > 0) {
        m_maxOpenings = maxOpenings;
    }
    if (maxOpeningsPerAddress > 0) {
        m_maxOpeningsPerAddress = maxOpeningsPerAddress;
    }
}

void StationOpeningGate::setOpeningDeadlineMs(int ms)
{
    if (ms > 0) {
        m_deadlineMs = ms;
    }
}

QSslSocket* StationOpeningGate::oldestPending(const QString& key) const
{
    QSslSocket* oldest = nullptr;
    quint64 oldestSerial = 0;
    int count = 0;
    for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it) {
        if (!key.isEmpty() && it->addressKey != key) {
            continue;
        }
        ++count;
        if (oldest == nullptr || it->serial < oldestSerial) {
            oldest = it.key();
            oldestSerial = it->serial;
        }
    }
    const int limit = key.isEmpty() ? m_maxOpenings : m_maxOpeningsPerAddress;
    return count >= limit ? oldest : nullptr;
}

void StationOpeningGate::incomingConnection(qintptr descriptor)
{
    auto* socket = new OpeningSocket(this);
    if (!socket->setSocketDescriptor(descriptor)) {
        socket->deleteLater();
        return;
    }
    const QString key = m_addressKey ? m_addressKey(socket->peerAddress().toString()) : QString();

    // At a limit, the OLDEST unfinished opening makes room, never the new
    // one: first that address's oldest, then the oldest of all. A client
    // that redials while its own abandoned dials are still opening gets
    // its newest dial through (that is the one it is waiting on), and
    // openings that never finish cannot keep a device out at all: each
    // new connection pushes the oldest of them out, long before the
    // deadline would.
    if (!key.isEmpty()) {
        if (QSslSocket* oldest = oldestPending(key)) {
            qCInfo(lcStationOpening) << "Closed an unfinished connection from"
                                     << oldest->peerAddress().toString()
                                     << "to make room for a newer one from the same address";
            release(oldest, /*close=*/true);
        }
    }
    if (QSslSocket* oldest = oldestPending(QString())) {
        qCInfo(lcStationOpening) << "Closed the oldest unfinished connection, from"
                                 << oldest->peerAddress().toString()
                                 << ", to make room for a newer one";
        release(oldest, /*close=*/true);
    }

    Pending pending;
    pending.socket = socket;
    pending.addressKey = key;
    pending.serial = ++m_serial;
    pending.deadline = new QTimer(this);
    pending.deadline->setSingleShot(true);
    pending.deadline->setInterval(m_deadlineMs);
    connect(pending.deadline, &QTimer::timeout, this, [this, socket]() {
        if (m_pending.contains(socket)) {
            qCInfo(lcStationOpening) << "A connection did not finish opening in time; closed.";
            release(socket, /*close=*/true);
        }
    });
    m_pending.insert(socket, pending);
    pending.deadline->start();

    connect(socket, &QAbstractSocket::disconnected, this, [this, socket]() {
        if (m_pending.contains(socket)) {
            release(socket, /*close=*/true);
        }
    });
    connect(socket, &QAbstractSocket::errorOccurred, this, [this, socket]() {
        if (m_pending.contains(socket) && !m_pending.value(socket).handedToQt) {
            release(socket, /*close=*/true);
        }
    });
    connect(socket, &QSslSocket::encrypted, this, [this, socket]() { onReadable(socket); });
    connect(socket, &QIODevice::readyRead, this, [this, socket]() {
        if (socket->isEncrypted()) {
            onReadable(socket);
        }
    });

    socket->setSslConfiguration(m_tls);
    socket->startServerEncryption();
}

void StationOpeningGate::onReadable(QSslSocket* socket)
{
    auto it = m_pending.find(socket);
    if (it == m_pending.end() || it->handedToQt) {
        return;
    }
    it->head += socket->readAll();
    const qsizetype end = it->head.indexOf("\r\n\r\n");
    if (end < 0) {
        if (it->head.size() > kMaxRequestHeadBytes) {
            refuse(socket);
        }
        return;
    }
    const QByteArray leftover = it->head.mid(end + 4);
    const Rewrite rewrite = rewriteRequestHead(it->head.left(end + 4));
    if (!rewrite.ok) {
        refuse(socket);
        return;
    }

    // Hand Qt the request as if it had just arrived: the rewritten head,
    // then anything the client sent after it, put back in front of
    // whatever is still unread. QIODevice::ungetChar prepends to the
    // socket's read buffer, so the bytes go back last-first.
    const QByteArray replay = rewrite.head + leftover;
    it->handedToQt = true;
    it->head.clear();
    disconnect(socket, &QIODevice::readyRead, this, nullptr);
    disconnect(socket, &QSslSocket::encrypted, this, nullptr);
    for (qsizetype i = replay.size() - 1; i >= 0; --i) {
        socket->ungetChar(replay.at(i));
    }
    // From here, a close by Qt with nothing written gets the 400 first.
    asOpening(socket)->arm();
    // handleConnection connects its own reader and, finding bytes already
    // buffered, emits readyRead for them (QWebSocketServerPrivate::
    // handleConnection, Qt 6.8.2 and 6.11). Qt then writes 101 and emits
    // newConnection, or answers 400 itself, or closes (and OpeningSocket
    // answers); the deadline above still runs until the owner calls
    // markOpened().
    m_target->handleConnection(socket);
}

void StationOpeningGate::refuse(QSslSocket* socket)
{
    release(socket, /*close=*/false);
    socket->write(badRequest());
    socket->disconnectFromHost();
    // Refused and no longer counted, but not left open either: a peer
    // that never lets the close finish is cut off at the deadline.
    QPointer<QSslSocket> guarded(socket);
    QTimer::singleShot(m_deadlineMs, this, [guarded]() {
        if (guarded) {
            guarded->abort();
            guarded->deleteLater();
        }
    });
    connect(socket, &QAbstractSocket::disconnected, socket, &QObject::deleteLater);
}

void StationOpeningGate::release(QSslSocket* socket, bool close)
{
    const auto it = m_pending.find(socket);
    if (it == m_pending.end()) {
        return;
    }
    QTimer* deadline = it->deadline;
    m_pending.erase(it);
    if (deadline != nullptr) {
        deadline->stop();
        deadline->deleteLater();
    }
    asOpening(socket)->disarm();
    if (close) {
        socket->abort();
        socket->deleteLater();
    }
}

void StationOpeningGate::markOpened(QWebSocket* webSocket)
{
    if (webSocket == nullptr) {
        return;
    }
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        QSslSocket* socket = it.key();
        if (it->handedToQt && it->socket && socket->peerPort() == webSocket->peerPort()
            && socket->localPort() == webSocket->localPort()
            && socket->peerAddress() == webSocket->peerAddress()) {
            // Opened: the socket is the QWebSocket's from here. Qt does
            // not reparent it (QWebSocketPrivate::upgradeFrom, Qt 6.8.2
            // and 6.11), so it is moved under the QWebSocket here: it is
            // released from the count and from this object's handlers,
            // never closed by closeAll(), and deleted with its QWebSocket.
            // (QWebSocket clears its pointer on the socket's destroyed
            // signal, so either may go first.)
            socket->disconnect(this);
            release(socket, /*close=*/false);
            socket->setParent(webSocket);
            return;
        }
    }
}

} // namespace NereusSDR
