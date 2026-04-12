#include "UdpEndpointWorker.h"
#include "MmioEndpoint.h"
#include "FormatParser.h"
#include "../LogCategories.h"

#include <QUdpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QNetworkDatagram>

namespace NereusSDR {

UdpEndpointWorker::UdpEndpointWorker(MmioEndpoint* endpoint, QObject* parent)
    : ITransportWorker(parent)
    , m_endpoint(endpoint)
{
}

UdpEndpointWorker::~UdpEndpointWorker()
{
    stop();
}

void UdpEndpointWorker::start()
{
    // Guard against re-entrance — tear down any existing socket first.
    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }

    m_socket = new QUdpSocket(this);

    connect(m_socket, &QUdpSocket::readyRead,
            this,     &UdpEndpointWorker::onReadyRead);

    // Resolve bind address. Empty host or "0.0.0.0" → any IPv4 interface.
    // From Thetis MeterManager.cs:40419 — UdpListener binds IPAddress.Any.
    const QString hostStr = m_endpoint->host();
    QHostAddress bindAddr;
    if (hostStr.isEmpty() || hostStr == QStringLiteral("0.0.0.0")) {
        bindAddr = QHostAddress::AnyIPv4;
    } else {
        bindAddr = QHostAddress(hostStr);
    }

    const quint16 port = m_endpoint->port();

    if (!m_socket->bind(bindAddr, port)) {
        qCWarning(lcMmio) << "UDP bind failed:" << m_socket->errorString();
        setState(State::Error);
        return;
    }

    qCInfo(lcMmio) << "UDP endpoint bound to"
                   << bindAddr.toString() << "port" << port
                   << "for endpoint" << m_endpoint->name();

    setState(State::Connected);
}

void UdpEndpointWorker::stop()
{
    if (!m_socket) {
        return;
    }

    m_socket->close();
    delete m_socket;
    m_socket = nullptr;

    qCInfo(lcMmio) << "UDP endpoint stopped for endpoint" << m_endpoint->name();

    setState(State::Disconnected);
}

void UdpEndpointWorker::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        if (datagram.isNull()) {
            continue;
        }

        const QByteArray payload = datagram.data();
        const QUuid guid = m_endpoint->guid();
        const MmioEndpoint::Format fmt = m_endpoint->format();

        QHash<QString, QVariant> batch;

        if (fmt == MmioEndpoint::Format::Json) {
            batch = FormatParser::parseJson(payload, guid);
        } else if (fmt == MmioEndpoint::Format::Xml) {
            batch = FormatParser::parseXml(payload, guid);
        } else if (fmt == MmioEndpoint::Format::Raw) {
            batch = FormatParser::parseRaw(payload, guid);
        }

        if (batch.isEmpty()) {
            qCWarning(lcMmio) << "UDP parse yielded empty batch for endpoint"
                              << m_endpoint->name()
                              << "(format:" << static_cast<int>(fmt) << ")";
            continue;
        }

        emit valueBatchReceived(batch);
    }
}

} // namespace NereusSDR
