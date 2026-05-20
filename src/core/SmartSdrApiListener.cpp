// =================================================================
// src/core/SmartSdrApiListener.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native class. No upstream port.
//
// AI tooling: Anthropic Claude Code.

#include "SmartSdrApiListener.h"

#include <QLoggingCategory>
#include <QHostAddress>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcSmartSdr, "nereus.smartsdr")

SmartSdrApiListener::SmartSdrApiListener(QObject* parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection,
            this, &SmartSdrApiListener::onNewConnection);
}

bool SmartSdrApiListener::start()
{
    if (m_server.isListening()) {
        m_server.close();
    }
    bool ok = m_server.listen(QHostAddress::AnyIPv4, 4992);
    if (!ok) {
        qCWarning(lcSmartSdr) << "failed to bind TCP 4992:"
                               << m_server.errorString();
    } else {
        qCInfo(lcSmartSdr) << "SmartSDR API listener listening on"
                           << m_server.serverAddress().toString()
                           << ":" << m_server.serverPort();
    }
    return ok;
}

void SmartSdrApiListener::stop()
{
    m_server.close();
}

bool SmartSdrApiListener::isListening() const
{
    return m_server.isListening();
}

void SmartSdrApiListener::onNewConnection()
{
    while (m_server.hasPendingConnections()) {
        QTcpSocket* sock = m_server.nextPendingConnection();
        if (!sock) {
            continue;
        }
        connect(sock, &QTcpSocket::readyRead,
                this, &SmartSdrApiListener::onClientDataReady);
        connect(sock, &QTcpSocket::disconnected,
                this, &SmartSdrApiListener::onClientDisconnected);

        const QString host = sock->peerAddress().toString();
        const quint16 port = sock->peerPort();
        qCInfo(lcSmartSdr) << "client connected from" << host << ":" << port;
        m_readBuffers[sock] = {};
        emit clientConnected(host, port);
    }
}

void SmartSdrApiListener::onClientDataReady()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) {
        return;
    }

    const QString peerHost = sock->peerAddress().toString();
    const quint16 peerPort = sock->peerPort();

    QByteArray chunk = sock->readAll();
    if (!chunk.isEmpty()) {
        qCInfo(lcSmartSdr) << "RX raw from" << peerHost << ":" << peerPort
                           << "(" << chunk.size() << "bytes):"
                           << chunk.toHex(' ').left(120)
                           << "| ascii:" << QString::fromUtf8(chunk).left(80);
    }
    m_readBuffers[sock].append(chunk);

    // SmartSDR API uses CR-terminated lines (matching PgxlConnection convention).
    QByteArray& buf = m_readBuffers[sock];
    int idx;
    while ((idx = buf.indexOf('\r')) != -1) {
        QByteArray rawLine = buf.left(idx);
        buf.remove(0, idx + 1);
        // Skip bare LF that may follow a CR.
        if (!buf.isEmpty() && buf.at(0) == '\n') {
            buf.remove(0, 1);
        }
        const QString line = QString::fromUtf8(rawLine).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        qCInfo(lcSmartSdr) << "RX from" << peerHost << ":" << peerPort
                            << "line:" << line;
        emit lineReceived(peerHost, peerPort, line);
    }
}

void SmartSdrApiListener::onClientDisconnected()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) {
        return;
    }
    qCInfo(lcSmartSdr) << "client disconnected from"
                        << sock->peerAddress().toString() << ":" << sock->peerPort()
                        << "total buffered:" << m_readBuffers.value(sock).size() << "bytes";
    m_readBuffers.remove(sock);
    sock->deleteLater();
}

}  // namespace NereusSDR
