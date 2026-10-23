// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/HappyEyeballs.cpp  (NereusSDR)
// =================================================================
//
// See HappyEyeballs.h. RFC 8305 (Happy Eyeballs Version 2).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-22: no Nagle on either tunnel socket; failure statuses Qt
//               reports as the real error; the listener a QObject child.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/HappyEyeballs.h"
#include "core/session/SystemProxy.h"

#include <QHostInfo>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QWebSocket>

#include <algorithm>
#include <utility>

namespace NereusSDR {

namespace {

Q_LOGGING_CATEGORY(lcHappyEyeballs, "nereussdr.happyeyeballs")

HappyEyeballs::Resolver& resolverForTest()
{
    static HappyEyeballs::Resolver resolver;
    return resolver;
}

QString withoutBrackets(QString host)
{
    if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']'))) {
        host = host.mid(1, host.size() - 2);
    }
    return host;
}

QString addressText(const QHostAddress& address, quint16 port)
{
    return address.protocol() == QAbstractSocket::IPv6Protocol
        ? QStringLiteral("[%1]:%2").arg(address.toString()).arg(port)
        : QStringLiteral("%1:%2").arg(address.toString()).arg(port);
}

} // namespace

// ── Address order ──────────────────────────────────────────────────────

QList<QHostAddress> HappyEyeballs::interleaveFamilies(const QList<QHostAddress>& addresses)
{
    QList<QHostAddress> first;
    QList<QHostAddress> other;
    if (addresses.isEmpty()) {
        return {};
    }
    const QAbstractSocket::NetworkLayerProtocol firstFamily = addresses.first().protocol();
    for (const QHostAddress& address : addresses) {
        (address.protocol() == firstFamily ? first : other).append(address);
    }
    QList<QHostAddress> ordered;
    ordered.reserve(addresses.size());
    for (qsizetype i = 0; i < std::max(first.size(), other.size()); ++i) {
        if (i < first.size()) {
            ordered.append(first.at(i));
        }
        if (i < other.size()) {
            ordered.append(other.at(i));
        }
    }
    return ordered;
}

void HappyEyeballs::setResolverForTest(Resolver resolver)
{
    if (QStandardPaths::isTestModeEnabled()) {
        resolverForTest() = std::move(resolver);
    }
}

QNetworkProxy HappyEyeballs::proxyForWebSocket(const QUrl& url, QWebSocket* socket)
{
    const QNetworkProxy system = SystemProxy::forUrl(url);
    // A proxy the computer's settings name resolves the name itself.
    if (system.type() != QNetworkProxy::NoProxy || socket == nullptr) {
        return system;
    }
    const QString host = url.host();
    // An address literal is one address: nothing to race.
    if (host.isEmpty() || !QHostAddress(withoutBrackets(host)).isNull()) {
        return system;
    }
    const int defaultPort = url.scheme() == QLatin1String("wss") ? 443 : 80;
    auto* tunnel = new HappyEyeballsTunnel(host, static_cast<quint16>(url.port(defaultPort)), socket);
    if (!tunnel->isListening()) {
        qCWarning(lcHappyEyeballs) << "No loopback listener for" << host
                                   << "; connecting without the address race";
        tunnel->deleteLater();
        return system;
    }
    return tunnel->proxy();
}

QAbstractSocket* HappyEyeballs::upstreamOf(const QWebSocket* socket)
{
    if (socket == nullptr) {
        return nullptr;
    }
    const auto* tunnel =
        socket->findChild<HappyEyeballsTunnel*>(QString(), Qt::FindDirectChildrenOnly);
    return tunnel != nullptr ? tunnel->upstream() : nullptr;
}

QByteArray HappyEyeballs::failureStatusLine(QAbstractSocket::SocketError error)
{
    // Qt 6's HTTP proxy client (QHttpSocketEngine::slotSocketReadNotification,
    // qtbase src/network/socket/qhttpsocketengine.cpp, v6.11.0 lines
    // 640-653) maps a CONNECT reply to the socket's error: 403 and 405 to
    // SocketAccessError, 404 to HostNotFoundError ("Host not found"), 503
    // to ConnectionRefusedError ("Connection refused"), and any other
    // status to ProxyProtocolError ("Error communicating with HTTP
    // proxy"), which blames a proxy the operator never set up.
    if (error == QAbstractSocket::HostNotFoundError) {
        return QByteArrayLiteral("HTTP/1.1 404 Not Found");
    }
    // Refused, unreachable, or timed out: no address took the connection.
    // Qt maps no status to a timeout or an unreachable network, so these
    // take the nearest it has, "Connection refused" (the race's own reason
    // goes to the log).
    return QByteArrayLiteral("HTTP/1.1 503 Service Unavailable");
}

// ── HappyEyeballsConnector ─────────────────────────────────────────────

HappyEyeballsConnector::HappyEyeballsConnector(QObject* parent)
    : QObject(parent)
{
    m_attemptTimer.setSingleShot(true);
    connect(&m_attemptTimer, &QTimer::timeout, this, &HappyEyeballsConnector::startNextAttempt);
}

HappyEyeballsConnector::~HappyEyeballsConnector()
{
    if (m_lookupId >= 0) {
        QHostInfo::abortHostLookup(m_lookupId);
    }
    m_done = true;
    dropAttempts(nullptr);
}

void HappyEyeballsConnector::start(const QString& host, quint16 port)
{
    m_port = port;
    const QString name = withoutBrackets(host);
    if (QStandardPaths::isTestModeEnabled() && resolverForTest()) {
        const QList<QHostAddress> addresses = resolverForTest()(name);
        QMetaObject::invokeMethod(this, [this, addresses] { onResolved(addresses); },
                                  Qt::QueuedConnection);
        return;
    }
    m_lookupId = QHostInfo::lookupHost(name, this, [this](const QHostInfo& info) {
        m_lookupId = -1;
        onResolved(info.error() == QHostInfo::NoError ? info.addresses() : QList<QHostAddress>{});
    });
}

void HappyEyeballsConnector::onResolved(const QList<QHostAddress>& addresses)
{
    if (m_done) {
        return;
    }
    m_queue = HappyEyeballs::interleaveFamilies(addresses);
    if (m_queue.isEmpty()) {
        finishFailed(QAbstractSocket::HostNotFoundError);
        return;
    }
    startNextAttempt();
}

void HappyEyeballsConnector::startNextAttempt()
{
    m_attemptTimer.stop();
    if (m_done || m_queue.isEmpty()) {
        return;
    }
    const QHostAddress address = m_queue.takeFirst();
    auto* socket = new QTcpSocket(this);
    socket->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    m_attempts.append(socket);
    connect(socket, &QTcpSocket::connected, this, [this, socket] { onAttemptConnected(socket); });
    connect(socket, &QTcpSocket::errorOccurred, this,
            [this, socket, address](QAbstractSocket::SocketError error) {
                onAttemptFailed(socket, address, error);
            });
    qCDebug(lcHappyEyeballs).noquote() << "Trying" << addressText(address, m_port);
    // The next address after the attempt delay, or as soon as this one
    // fails (onAttemptFailed), whichever comes first.
    if (!m_queue.isEmpty()) {
        m_attemptTimer.start(HappyEyeballs::kConnectionAttemptDelayMs);
    }
    socket->connectToHost(address, m_port);
}

void HappyEyeballsConnector::onAttemptConnected(QTcpSocket* socket)
{
    if (m_done) {
        return;
    }
    m_done = true;
    m_attemptTimer.stop();
    m_queue.clear();
    socket->disconnect(this);
    m_attempts.removeAll(socket);
    dropAttempts(nullptr);
    qCInfo(lcHappyEyeballs).noquote()
        << "Connected to" << addressText(socket->peerAddress(), socket->peerPort());
    emit connected(socket);
}

void HappyEyeballsConnector::onAttemptFailed(QTcpSocket* socket, const QHostAddress& address,
                                             QAbstractSocket::SocketError error)
{
    if (m_done || !m_attempts.contains(socket)) {
        return;
    }
    qCDebug(lcHappyEyeballs).noquote()
        // The address this attempt was given: a failed socket's
        // peerAddress() is already cleared.
        << "Attempt to" << addressText(address, m_port) << "failed:"
        << socket->errorString();
    m_lastError = error;
    m_attempts.removeAll(socket);
    socket->disconnect(this);
    socket->abort();
    socket->deleteLater();
    if (!m_queue.isEmpty()) {
        startNextAttempt();
    } else if (m_attempts.isEmpty()) {
        finishFailed(m_lastError);
    }
}

void HappyEyeballsConnector::finishFailed(QAbstractSocket::SocketError error)
{
    if (m_done) {
        return;
    }
    m_done = true;
    m_attemptTimer.stop();
    dropAttempts(nullptr);
    emit failed(error);
}

void HappyEyeballsConnector::dropAttempts(QTcpSocket* keep)
{
    const QList<QTcpSocket*> attempts = std::exchange(m_attempts, {});
    for (QTcpSocket* attempt : attempts) {
        if (attempt == keep) {
            continue;
        }
        attempt->disconnect(this);
        attempt->abort();
        attempt->deleteLater();
    }
}

// ── HappyEyeballsTunnel ────────────────────────────────────────────────

HappyEyeballsTunnel::HappyEyeballsTunnel(const QString& host, quint16 port, QObject* parent)
    : QObject(parent)
    , m_host(withoutBrackets(host))
    , m_port(port)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &HappyEyeballsTunnel::onNewConnection);
    m_server->setMaxPendingConnections(1);
    m_server->listen(QHostAddress::LocalHost, 0);
}

HappyEyeballsTunnel::~HappyEyeballsTunnel()
{
    m_stage = Stage::Closed;
    for (QTcpSocket* socket : {m_client.data(), m_upstream.data()}) {
        if (socket != nullptr) {
            socket->disconnect(this);
        }
    }
}

QNetworkProxy HappyEyeballsTunnel::proxy() const
{
    return QNetworkProxy(QNetworkProxy::HttpProxy, m_server->serverAddress().toString(),
                         m_server->serverPort());
}

void HappyEyeballsTunnel::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        // One connection: the WebSocket's own. Any later one is refused.
        if (m_stage != Stage::Listening) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        socket->setParent(this);
        m_client = socket;
        m_stage = Stage::ReadingRequest;
        connect(socket, &QTcpSocket::readyRead, this, &HappyEyeballsTunnel::onClientReadable);
        connect(socket, &QTcpSocket::disconnected, this, [this] {
            if (m_stage != Stage::Piping) {
                shut();
            }
        });
    }
    m_server->close();
}

bool HappyEyeballsTunnel::targetMatches(const QByteArray& authority) const
{
    const QString text = QString::fromLatin1(authority);
    const qsizetype colon = text.lastIndexOf(QLatin1Char(':'));
    if (colon <= 0) {
        return false;
    }
    bool portOk = false;
    const uint port = text.mid(colon + 1).toUInt(&portOk);
    // Compared in ASCII form, as an internationalised name goes on the wire.
    const QByteArray asked = QUrl::toAce(withoutBrackets(text.left(colon)));
    return portOk && port == m_port && !asked.isEmpty()
        && QString::fromLatin1(asked).compare(QString::fromLatin1(QUrl::toAce(m_host)),
                                              Qt::CaseInsensitive) == 0;
}

void HappyEyeballsTunnel::onClientReadable()
{
    if (m_stage == Stage::Piping) {
        pump(m_client, m_upstream);
        return;
    }
    if (m_stage != Stage::ReadingRequest || m_client.isNull()) {
        return;
    }
    m_request += m_client->readAll();
    const qsizetype end = m_request.indexOf("\r\n\r\n");
    if (end < 0) {
        if (m_request.size() > HappyEyeballs::kMaxRequestBytes) {
            answerAndClose("HTTP/1.1 400 Bad Request");
        }
        return;
    }
    const QList<QByteArray> words = m_request.left(m_request.indexOf("\r\n")).split(' ');
    if (words.size() != 3 || words.at(0) != "CONNECT" || !targetMatches(words.at(1))) {
        qCWarning(lcHappyEyeballs) << "Tunnel refused a request that was not its CONNECT";
        answerAndClose("HTTP/1.1 403 Forbidden");
        return;
    }
    // Anything after the request (nothing, for Qt) goes to the far end.
    m_request.remove(0, end + 4);
    m_stage = Stage::Connecting;
    auto* connector = new HappyEyeballsConnector(this);
    m_connector = connector;
    connect(connector, &HappyEyeballsConnector::connected, this,
            &HappyEyeballsTunnel::onUpstreamConnected);
    connect(connector, &HappyEyeballsConnector::failed, this,
            &HappyEyeballsTunnel::onUpstreamFailed);
    connector->start(m_host, m_port);
}

void HappyEyeballsTunnel::onUpstreamConnected(QTcpSocket* socket)
{
    socket->setParent(this);
    if (m_connector) {
        m_connector->deleteLater();
    }
    if (m_stage != Stage::Connecting || m_client.isNull()) {
        socket->abort();
        socket->deleteLater();
        shut();
        return;
    }
    m_upstream = socket;
    m_stage = Stage::Piping;
    // No Nagle on either side (section 12.5 for the relay leg; every
    // tunnel alike): the loopback hop and the connection to the far end
    // send small messages at once, as the WebSocket's own socket does.
    m_client->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    // Bounded buffers each way: a side that does not read holds the other
    // back, as a direct TCP connection would.
    m_client->setReadBufferSize(HappyEyeballs::kPipeHighWaterBytes);
    socket->setReadBufferSize(HappyEyeballs::kPipeHighWaterBytes);
    connect(socket, &QTcpSocket::readyRead, this, [this] { pump(m_upstream, m_client); });
    connect(socket, &QTcpSocket::bytesWritten, this, [this] { pump(m_client, m_upstream); });
    connect(m_client, &QTcpSocket::bytesWritten, this, [this] { pump(m_upstream, m_client); });
    connect(socket, &QTcpSocket::disconnected, this,
            [this] { onSideDisconnected(m_upstream, m_client); });
    connect(m_client, &QTcpSocket::disconnected, this,
            [this] { onSideDisconnected(m_client, m_upstream); });
    for (QTcpSocket* side : {m_client.data(), socket}) {
        connect(side, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
            // A closed side ends through disconnected (onSideDisconnected).
            if (error != QAbstractSocket::RemoteHostClosedError) {
                shut();
            }
        });
    }
    m_client->write("HTTP/1.1 200 Connection established\r\n\r\n");
    if (!m_request.isEmpty()) {
        socket->write(std::exchange(m_request, {}));
    }
    pump(m_client, m_upstream);
    pump(m_upstream, m_client);
}

void HappyEyeballsTunnel::onUpstreamFailed(QAbstractSocket::SocketError error)
{
    if (m_connector) {
        m_connector->deleteLater();
    }
    if (m_stage == Stage::Connecting) {
        qCInfo(lcHappyEyeballs) << "No address of" << m_host << "answered:" << error;
        answerAndClose(HappyEyeballs::failureStatusLine(error));
    }
}

void HappyEyeballsTunnel::pump(QTcpSocket* from, QTcpSocket* to)
{
    if (from == nullptr || to == nullptr || m_stage != Stage::Piping) {
        return;
    }
    while (from->bytesAvailable() > 0
           && to->bytesToWrite() < HappyEyeballs::kPipeHighWaterBytes) {
        const qint64 room = HappyEyeballs::kPipeHighWaterBytes - to->bytesToWrite();
        const QByteArray chunk = from->read(std::min(room, from->bytesAvailable()));
        if (chunk.isEmpty()) {
            break;
        }
        to->write(chunk);
    }
}

void HappyEyeballsTunnel::onSideDisconnected(QTcpSocket* from, QTcpSocket* to)
{
    if (m_stage != Stage::Piping) {
        return;
    }
    m_stage = Stage::Closed;
    // What the closed side sent before it closed still reaches the other.
    if (from != nullptr && to != nullptr && from->bytesAvailable() > 0) {
        to->write(from->readAll());
    }
    if (to != nullptr) {
        to->disconnectFromHost();
    }
}

void HappyEyeballsTunnel::answerAndClose(const QByteArray& statusLine)
{
    m_stage = Stage::Closed;
    if (m_client) {
        m_client->write(statusLine + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        m_client->disconnectFromHost();
    }
}

void HappyEyeballsTunnel::shut()
{
    m_stage = Stage::Closed;
    m_server->close();
    if (m_connector) {
        m_connector->deleteLater();
    }
    for (QTcpSocket* socket : {m_client.data(), m_upstream.data()}) {
        if (socket != nullptr) {
            socket->disconnect(this);
            socket->abort();
        }
    }
}

} // namespace NereusSDR
