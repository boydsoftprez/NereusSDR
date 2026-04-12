// Phase 3G-6 block 5 — TCP listener transport worker implementation.
// Porting intent from Thetis MeterManager.cs:39899-40160 (TcpListener inner
// class). Qt signal/slot replaces Thetis's polling loop.

#include "TcpListenerEndpointWorker.h"
#include "MmioEndpoint.h"
#include "FormatParser.h"
#include "../LogCategories.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

namespace NereusSDR {

TcpListenerEndpointWorker::TcpListenerEndpointWorker(MmioEndpoint* endpoint, QObject* parent)
    : ITransportWorker(parent)
    , m_endpoint(endpoint)
{
}

TcpListenerEndpointWorker::~TcpListenerEndpointWorker()
{
    stop();
}

void TcpListenerEndpointWorker::start()
{
    // Tear down any previous server or client before re-binding.
    if (m_client) {
        m_client->disconnectFromHost();
        m_client->deleteLater();
        m_client = nullptr;
    }
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_lineBuffer.clear();

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
            this, &TcpListenerEndpointWorker::onNewConnection);

    // Resolve bind address: empty or "0.0.0.0" → AnyIPv4, else parse.
    const QString hostStr = m_endpoint->host();
    QHostAddress bindAddr;
    if (hostStr.isEmpty() || hostStr == QStringLiteral("0.0.0.0")) {
        bindAddr = QHostAddress::AnyIPv4;
    } else {
        bindAddr = QHostAddress(hostStr);
    }

    const quint16 port = m_endpoint->port();

    if (!m_server->listen(bindAddr, port)) {
        qCWarning(lcMmio) << "TcpListenerEndpointWorker: failed to bind"
                          << bindAddr.toString() << ":" << port
                          << "—" << m_server->errorString();
        setState(State::Error);
        return;
    }

    setState(State::Listening);
    qCInfo(lcMmio) << "TcpListenerEndpointWorker: listening on"
                   << m_server->serverAddress().toString()
                   << ":" << m_server->serverPort();
}

void TcpListenerEndpointWorker::stop()
{
    if (m_client) {
        m_client->disconnectFromHost();
        m_client->deleteLater();
        m_client = nullptr;
    }
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_lineBuffer.clear();
    setState(State::Disconnected);
    qCInfo(lcMmio) << "TcpListenerEndpointWorker: stopped";
}

void TcpListenerEndpointWorker::onNewConnection()
{
    QTcpSocket* incoming = m_server->nextPendingConnection();
    if (!incoming) {
        return;
    }

    if (m_client != nullptr) {
        // Already have a client — reject the new one.
        qCInfo(lcMmio) << "TcpListenerEndpointWorker: rejecting additional"
                          " TCP client — only one connection supported";
        incoming->disconnectFromHost();
        incoming->deleteLater();
        return;
    }

    m_client = incoming;
    connect(m_client, &QTcpSocket::readyRead,
            this, &TcpListenerEndpointWorker::onClientReadyRead);
    connect(m_client, &QTcpSocket::disconnected,
            this, &TcpListenerEndpointWorker::onClientDisconnected);

    m_lineBuffer.clear();
    setState(State::Connected);
    qCInfo(lcMmio) << "TcpListenerEndpointWorker: client connected from"
                   << m_client->peerAddress().toString()
                   << ":" << m_client->peerPort();
}

void TcpListenerEndpointWorker::onClientReadyRead()
{
    m_lineBuffer.append(m_client->readAll());

    // Process all complete newline-delimited messages in the buffer.
    while (true) {
        const int newlinePos = m_lineBuffer.indexOf('\n');
        if (newlinePos < 0) {
            break;
        }

        // Extract the line up to (not including) the newline.
        QByteArray line = m_lineBuffer.left(newlinePos);
        // Remove the consumed bytes (including the newline) from the buffer.
        m_lineBuffer.remove(0, newlinePos + 1);

        // Strip a trailing carriage return to support CR+LF line endings.
        if (!line.isEmpty() && line.endsWith('\r')) {
            line.chop(1);
        }

        if (line.isEmpty()) {
            continue;
        }

        // Dispatch to the appropriate format parser.
        QHash<QString, QVariant> batch;
        const QUuid guid = m_endpoint->guid();

        switch (m_endpoint->format()) {
        case MmioEndpoint::Format::Json:
            batch = FormatParser::parseJson(line, guid);
            break;
        case MmioEndpoint::Format::Xml:
            batch = FormatParser::parseXml(line, guid);
            break;
        case MmioEndpoint::Format::Raw:
            batch = FormatParser::parseRaw(line, guid);
            break;
        }

        if (!batch.isEmpty()) {
            emit valueBatchReceived(batch);
        }
    }
}

void TcpListenerEndpointWorker::onClientDisconnected()
{
    qCInfo(lcMmio) << "TcpListenerEndpointWorker: TCP client disconnected";
    m_client->deleteLater();
    m_client = nullptr;
    m_lineBuffer.clear();
    // Keep the server alive — return to listening for the next client.
    setState(State::Listening);
}

} // namespace NereusSDR
