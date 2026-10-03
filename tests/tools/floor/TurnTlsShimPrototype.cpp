// no-port-check: NereusSDR-original.
// =================================================================
// tests/tools/floor/TurnTlsShimPrototype.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 Step 1 (R-IOS-16): option (C), see the header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "TurnTlsShimPrototype.h"

#include <QNetworkDatagram>
#include <QSslSocket>
#include <QUdpSocket>

namespace NereusSDR::FloorPrototype {

namespace {

constexpr int kStunHeaderBytes = 20;
constexpr int kChannelDataHeaderBytes = 4;
constexpr int kMaxFrameBytes = 65535 + kStunHeaderBytes;

quint16 lengthField(const QByteArray& buffer)
{
    return static_cast<quint16>((static_cast<quint8>(buffer.at(2)) << 8)
                                | static_cast<quint8>(buffer.at(3)));
}

} // namespace

TurnTlsShimPrototype::TurnTlsShimPrototype(QString relayHost, quint16 relayPort, QObject* parent)
    : QObject(parent)
    , m_relayHost(std::move(relayHost))
    , m_relayPort(relayPort)
{
}

TurnTlsShimPrototype::~TurnTlsShimPrototype() = default;

quint16 TurnTlsShimPrototype::start()
{
    m_udp = new QUdpSocket(this);
    if (!m_udp->bind(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    connect(m_udp, &QUdpSocket::readyRead, this, &TurnTlsShimPrototype::onDatagrams);
    connectRelay();
    return m_udp->localPort();
}

void TurnTlsShimPrototype::connectRelay()
{
    if (m_tls != nullptr) {
        m_tls->disconnect(this);
        m_tls->deleteLater();
    }
    m_stream.clear();
    m_tls = new QSslSocket(this);
    // Each datagram goes out at once (TCP_NODELAY); an option set before
    // the socket exists does not reach it, so it is set on connect.
    connect(m_tls, &QSslSocket::connected, this, [this] {
        m_tls->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    });
    connect(m_tls, &QSslSocket::encrypted, this, [this] {
        ++m_connections;
        if (!m_pending.isEmpty()) {
            m_tls->write(m_pending);
            m_pending.clear();
        }
        emit encrypted();
    });
    connect(m_tls, &QSslSocket::readyRead, this, &TurnTlsShimPrototype::onRelayReadable);
    connect(m_tls, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit failed(m_tls->errorString());
    });
    m_tls->connectToHostEncrypted(m_relayHost, m_relayPort);
}

void TurnTlsShimPrototype::resetConnection()
{
    if (m_tls != nullptr) {
        m_tls->abort();
    }
    connectRelay();
}

void TurnTlsShimPrototype::onDatagrams()
{
    while (m_udp->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_udp->receiveDatagram();
        m_agentAddress = datagram.senderAddress();
        m_agentPort = static_cast<quint16>(datagram.senderPort());
        const QByteArray frame = streamFrame(datagram.data());
        if (frame.isEmpty()) {
            continue;
        }
        m_bytesToRelay += static_cast<quint64>(frame.size());
        if (m_tls != nullptr && m_tls->isEncrypted()) {
            m_tls->write(frame);
        } else {
            m_pending.append(frame);
        }
    }
}

void TurnTlsShimPrototype::onRelayReadable()
{
    const QByteArray data = m_tls->readAll();
    m_bytesFromRelay += static_cast<quint64>(data.size());
    m_stream.append(data);
    QList<QByteArray> frames;
    if (!takeFrames(m_stream, frames)) {
        emit failed(QStringLiteral("The relay sent something that is not TURN."));
        m_tls->abort();
        return;
    }
    for (const QByteArray& frame : frames) {
        if (m_agentPort != 0) {
            m_udp->writeDatagram(frame, m_agentAddress, m_agentPort);
        }
    }
}

bool TurnTlsShimPrototype::takeFrames(QByteArray& buffer, QList<QByteArray>& frames)
{
    qsizetype offset = 0;
    while (buffer.size() - offset >= kChannelDataHeaderBytes) {
        const QByteArray head = buffer.mid(offset, kChannelDataHeaderBytes);
        const quint8 first = static_cast<quint8>(head.at(0));
        const int length = lengthField(head);
        qsizetype whole = 0;
        qsizetype payload = 0;
        if ((first & 0xC0) == 0x00) {
            whole = kStunHeaderBytes + length;
            payload = whole;
        } else if ((first & 0xC0) == 0x40) {
            payload = kChannelDataHeaderBytes + length;
            whole = kChannelDataHeaderBytes + ((length + 3) & ~3);
        } else {
            return false;
        }
        if (whole > kMaxFrameBytes) {
            return false;
        }
        if (buffer.size() - offset < whole) {
            break;
        }
        frames.append(buffer.mid(offset, payload));
        offset += whole;
    }
    buffer.remove(0, offset);
    return true;
}

QByteArray TurnTlsShimPrototype::streamFrame(const QByteArray& datagram)
{
    if (datagram.size() < kChannelDataHeaderBytes) {
        return {};
    }
    const quint8 first = static_cast<quint8>(datagram.at(0));
    if ((first & 0xC0) == 0x00) {
        return datagram;
    }
    if ((first & 0xC0) != 0x40) {
        return {};
    }
    const int length = lengthField(datagram);
    if (datagram.size() < kChannelDataHeaderBytes + length) {
        return {};
    }
    QByteArray frame = datagram.left(kChannelDataHeaderBytes + length);
    frame.append(QByteArray((4 - (length % 4)) % 4, '\0'));
    return frame;
}

} // namespace NereusSDR::FloorPrototype
