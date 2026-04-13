#include "RadioDiscovery.h"
#include "BoardCapabilities.h"
#include "LogCategories.h"

#include <QDateTime>
#include <QNetworkDatagram>
#include <QNetworkInterface>

#include <sys/socket.h>

namespace NereusSDR {

// --- RadioInfo static helpers ---

QString RadioInfo::displayName() const
{
    if (!name.isEmpty()) {
        return name;
    }
    return QString::fromLatin1(BoardCapsTable::forBoard(boardType).displayName)
           + " (" + macAddress + ")";
}

int RadioInfo::adcCountForBoard(HPSDRHW type)
{
    switch (type) {
    case HPSDRHW::Angelia:
    case HPSDRHW::Orion:
    case HPSDRHW::OrionMKII:
    case HPSDRHW::Saturn:
    case HPSDRHW::SaturnMKII:
        return 2;
    default:
        return 1;
    }
}

int RadioInfo::maxReceiversForBoard(HPSDRHW type)
{
    switch (type) {
    case HPSDRHW::Atlas:        return 3;
    case HPSDRHW::Hermes:       return 4;
    case HPSDRHW::HermesII:     return 4;
    case HPSDRHW::HermesLite:   return 4;
    case HPSDRHW::Angelia:      return 7;
    case HPSDRHW::Orion:        return 7;
    case HPSDRHW::OrionMKII:    return 7;
    case HPSDRHW::Saturn:       return 7;
    case HPSDRHW::SaturnMKII:   return 7;
    default:                    return 1;
    }
}

// --- RadioDiscovery ---

QString RadioDiscovery::macToString(const char* bytes)
{
    return QString("%1:%2:%3:%4:%5:%6")
        .arg(static_cast<quint8>(bytes[0]), 2, 16, QChar('0'))
        .arg(static_cast<quint8>(bytes[1]), 2, 16, QChar('0'))
        .arg(static_cast<quint8>(bytes[2]), 2, 16, QChar('0'))
        .arg(static_cast<quint8>(bytes[3]), 2, 16, QChar('0'))
        .arg(static_cast<quint8>(bytes[4]), 2, 16, QChar('0'))
        .arg(static_cast<quint8>(bytes[5]), 2, 16, QChar('0'))
        .toUpper();
}

RadioDiscovery::RadioDiscovery(QObject* parent)
    : QObject(parent)
{
    connect(&m_discoveryTimer, &QTimer::timeout, this, &RadioDiscovery::sendDiscoveryPacket);
    connect(&m_staleTimer, &QTimer::timeout, this, &RadioDiscovery::onStaleCheck);
}

RadioDiscovery::~RadioDiscovery()
{
    stopDiscovery();
}

void RadioDiscovery::startDiscovery()
{
    if (m_socket) {
        return;
    }

    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::Any, 0)) {
        qCWarning(lcDiscovery) << "Failed to bind UDP socket";
        delete m_socket;
        m_socket = nullptr;
        return;
    }

    // Enable SO_BROADCAST so writeDatagram to broadcast addresses works
    int fd = m_socket->socketDescriptor();
    if (fd >= 0) {
        int broadcastEnable = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &RadioDiscovery::onReadyRead);

    sendDiscoveryPacket();
    m_discoveryTimer.start(kDiscoveryIntervalMs);
    m_staleTimer.start(kStaleTimeoutMs);

    qCDebug(lcDiscovery) << "Started on port" << m_socket->localPort();
}

void RadioDiscovery::stopDiscovery()
{
    m_discoveryTimer.stop();
    m_staleTimer.stop();

    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }
}

QList<RadioInfo> RadioDiscovery::discoveredRadios() const
{
    return m_radios.values();
}

void RadioDiscovery::sendDiscoveryPacket()
{
    if (!m_socket) {
        return;
    }

    // P1 discovery packet: 63 bytes: 0xEF 0xFE 0x02 followed by 60 zero bytes
    QByteArray p1Packet(63, 0);
    p1Packet[0] = static_cast<char>(0xEF);
    p1Packet[1] = static_cast<char>(0xFE);
    p1Packet[2] = static_cast<char>(0x02);

    // P2 discovery packet: 60 bytes, byte[4] = 0x02 (rest zeros)
    // P2 radios (ANAN-G2, OrionMkII, Saturn) only respond to this format
    QByteArray p2Packet(60, 0);
    p2Packet[4] = static_cast<char>(0x02);

    // Broadcast on all network interfaces to reach radios on VPN/secondary subnets
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)
            || !(iface.flags() & QNetworkInterface::IsRunning)
            || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        const auto entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            QHostAddress broadcast = entry.broadcast();
            if (broadcast.isNull() || broadcast.protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            m_socket->writeDatagram(p1Packet, broadcast, kDiscoveryPort);
            m_socket->writeDatagram(p2Packet, broadcast, kDiscoveryPort);
        }
    }

    // Also send to global broadcast as fallback
    m_socket->writeDatagram(p1Packet, QHostAddress::Broadcast, kDiscoveryPort);
    m_socket->writeDatagram(p2Packet, QHostAddress::Broadcast, kDiscoveryPort);
}

void RadioDiscovery::onReadyRead()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();

        if (data.size() < 11) {
            continue;
        }

        // P1 responses start with 0xEF 0xFE
        // P2 responses start with 0x00 (discriminator byte)
        // Dispatch based on first byte
        const quint8 firstByte = static_cast<quint8>(data[0]);

        RadioInfo info;

        if (firstByte == 0xEF && data.size() >= 11
            && static_cast<quint8>(data[1]) == 0xFE) {
            // Protocol 1 discovery response
            quint8 status = static_cast<quint8>(data[2]);
            if (status != 0x02 && status != 0x03) {
                continue;
            }

            // Convert IPv6-mapped IPv4 (::ffff:x.x.x.x) to pure IPv4
            // so writeDatagram works correctly on IPv4-only sockets
            QHostAddress sender = datagram.senderAddress();
            bool ok = false;
            quint32 ipv4 = sender.toIPv4Address(&ok);
            info.address = ok ? QHostAddress(ipv4) : sender;
            info.port = kDiscoveryPort;
            info.protocol = ProtocolVersion::Protocol1;
            info.inUse = (status == 0x03);
            info.macAddress = macToString(data.constData() + 3);  // bytes 3-8
            info.firmwareVersion = static_cast<quint8>(data[9]);
            info.boardType = static_cast<HPSDRHW>(static_cast<quint8>(data[10]));

        } else if (firstByte == 0x00 && data.size() >= 21) {
            // Protocol 2 discovery response
            // Bytes 0-3: 0x00 (P2 discriminator)
            // Byte 4: status (0x02 = available, 0x03 = busy)
            quint8 status = static_cast<quint8>(data[4]);
            if (status != 0x02 && status != 0x03) {
                continue;
            }

            // Convert IPv6-mapped IPv4 (::ffff:x.x.x.x) to pure IPv4
            // so writeDatagram works correctly on IPv4-only sockets
            QHostAddress sender = datagram.senderAddress();
            bool ok = false;
            quint32 ipv4 = sender.toIPv4Address(&ok);
            info.address = ok ? QHostAddress(ipv4) : sender;
            info.port = kDiscoveryPort;
            info.protocol = ProtocolVersion::Protocol2;
            info.inUse = (status == 0x03);
            info.macAddress = macToString(data.constData() + 5);  // bytes 5-10
            info.boardType = static_cast<HPSDRHW>(static_cast<quint8>(data[11]));
            info.firmwareVersion = static_cast<quint8>(data[13]);

            // Byte 20: number of hardware receivers (if present)
            if (data.size() > 20) {
                int hwRx = static_cast<quint8>(data[20]);
                if (hwRx > 0) {
                    info.maxReceivers = hwRx;
                }
            }

            qCDebug(lcDiscovery) << "P2 response from" << info.address.toString()
                                 << "board:" << BoardCapsTable::forBoard(info.boardType).displayName
                                 << "fw:" << info.firmwareVersion;
        } else {
            continue;  // Unknown response format
        }

        // Populate derived capabilities
        info.adcCount = RadioInfo::adcCountForBoard(info.boardType);
        if (info.maxReceivers <= 0) {
            info.maxReceivers = RadioInfo::maxReceiversForBoard(info.boardType);
        }
        info.name = QString::fromLatin1(BoardCapsTable::forBoard(info.boardType).displayName);
        info.hasDiversityReceiver = (info.adcCount >= 2);
        info.hasPureSignal = (info.boardType != HPSDRHW::Atlas
                              && info.boardType != HPSDRHW::Unknown);

        // P2 boards support higher sample rates
        if (info.protocol == ProtocolVersion::Protocol2) {
            info.maxSampleRate = 1536000;
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_lastSeen[info.macAddress] = now;

        if (!m_radios.contains(info.macAddress)) {
            m_radios.insert(info.macAddress, info);
            qCDebug(lcDiscovery) << "Discovered:" << info.displayName()
                                 << "P" << static_cast<int>(info.protocol)
                                 << "at" << info.address.toString();
            emit radioDiscovered(info);
        } else {
            m_radios[info.macAddress] = info;
            emit radioUpdated(info);
        }
    }
}

void RadioDiscovery::onStaleCheck()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList stale;

    for (auto it = m_lastSeen.constBegin(); it != m_lastSeen.constEnd(); ++it) {
        if (now - it.value() > kStaleTimeoutMs) {
            stale.append(it.key());
        }
    }

    for (const QString& mac : stale) {
        qCDebug(lcDiscovery) << "Radio lost:" << mac;
        m_radios.remove(mac);
        m_lastSeen.remove(mac);
        emit radioLost(mac);
    }
}

} // namespace NereusSDR
