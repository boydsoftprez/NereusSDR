// no-port-check: NereusSDR-original.
// =================================================================
// tests/tools/floor/WsRelayPrototype.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 Step 1 (R-IOS-16): option (E), see the header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "WsRelayPrototype.h"

#include <QAbstractSocket>
#include <QNetworkDatagram>
#include <QNetworkRequest>
#include <QTimer>
#include <QUdpSocket>
#include <QUrlQuery>
#include <QWebSocket>

#include <cstdio>

namespace NereusSDR::FloorPrototype {

WsRelayPrototype::WsRelayPrototype(QUrl relayUrl, QString token, Role role, QObject* parent)
    : QObject(parent)
    , m_url(std::move(relayUrl))
    , m_token(std::move(token))
    , m_role(role)
{
}

WsRelayPrototype::~WsRelayPrototype() = default;

quint16 WsRelayPrototype::start()
{
    m_udp = new QUdpSocket(this);
    if (!m_udp->bind(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    connect(m_udp, &QUdpSocket::readyRead, this, &WsRelayPrototype::onDatagrams);
    open();
    return m_udp->localPort();
}

void WsRelayPrototype::setAgentPort(quint16 port)
{
    m_agentPort = port;
}

void WsRelayPrototype::open()
{
    if (m_socket != nullptr) {
        m_socket->disconnect(this);
        m_socket->deleteLater();
    }
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(m_socket, &QWebSocket::connected, this, [this] {
        // Each frame goes out at once (TCP_NODELAY). QWebSocket has no
        // option for it; its socket is its child.
        auto* tcp = m_socket->findChild<QAbstractSocket*>();
        if (tcp != nullptr) {
            tcp->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        }
        std::fprintf(stderr, "floor relay: connected, low delay %s\n",
                     tcp != nullptr ? "set" : "not reachable");
        ++m_connections;
        emit opened();
    });
    connect(m_socket, &QWebSocket::binaryMessageReceived, this, [this](const QByteArray& frame) {
        ++m_framesIn;
        if (m_agentPort != 0) {
            m_udp->writeDatagram(frame, m_agentAddress, m_agentPort);
        }
    });
    connect(m_socket, &QWebSocket::disconnected, this, [this] {
        // Reconnect at once under the same token (a reset, or the
        // service's restart); the prototype never gives up.
        QTimer::singleShot(0, this, &WsRelayPrototype::open);
    });
    QUrl url = m_url;
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("token"), m_token);
    query.addQueryItem(QStringLiteral("role"),
                       m_role == Role::Device ? QStringLiteral("device") : QStringLiteral("core"));
    url.setQuery(query);
    m_socket->open(QNetworkRequest(url));
}

void WsRelayPrototype::resetConnection()
{
    if (m_socket != nullptr) {
        m_socket->abort();
    }
}

void WsRelayPrototype::onDatagrams()
{
    while (m_udp->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_udp->receiveDatagram();
        if (m_role == Role::Device) {
            m_agentAddress = datagram.senderAddress();
            m_agentPort = static_cast<quint16>(datagram.senderPort());
        }
        if (m_socket != nullptr && m_socket->state() == QAbstractSocket::ConnectedState) {
            m_socket->sendBinaryMessage(datagram.data());
            ++m_framesOut;
        }
    }
}

} // namespace NereusSDR::FloorPrototype
