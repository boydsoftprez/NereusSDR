// tests/fakes/P2FakeRadio.cpp
//
// no-port-check: NereusSDR-original Protocol 2 loopback fixture. Wire-format
// citations describe the packets produced by the fake; no upstream control
// flow is reproduced here.

#include "P2FakeRadio.h"

#include <QNetworkDatagram>
#include <QUdpSocket>

#include <algorithm>
#include <cmath>

namespace NereusSDR::Test {

namespace {

// m_roleSockets is indexed from the host's outbound base. The radio's input
// source-role base is outbound+1, so role 0 (status) is socket offset 1 and
// role 10 (DDC0) is socket offset 11.
constexpr int kStatusSocketOffset = 1;
constexpr int kWidebandSocketOffset = 3;
constexpr int kDdcSocketOffset = 11;

void writeBe32(char* bytes, quint32 value)
{
    bytes[0] = static_cast<char>((value >> 24) & 0xff);
    bytes[1] = static_cast<char>((value >> 16) & 0xff);
    bytes[2] = static_cast<char>((value >> 8) & 0xff);
    bytes[3] = static_cast<char>(value & 0xff);
}

qint32 toInt24(float sample)
{
    const float clipped = std::clamp(sample, -1.0f, 1.0f);
    return static_cast<qint32>(std::lround(clipped * 8388607.0f));
}

} // namespace

P2FakeRadio::P2FakeRadio(QObject* parent)
    : QObject(parent)
{
}

P2FakeRadio::~P2FakeRadio()
{
    stop();
}

bool P2FakeRadio::start(const QHostAddress& address,
                        quint16 preferredOutboundBase)
{
    if (m_outboundBase != 0) {
        return true;
    }

    m_address = address;
    m_clientAddress = QHostAddress();
    m_clientPort = 0;
    m_totalEgressDatagrams = 0;
    m_highPriorityDatagrams = 0;
    m_stopCount = 0;
    m_moxAssertedCount = 0;
    m_lastHighPriorityFlags = 0;
    m_lastHighPriorityOcByte = -1;
    m_ddcSequence = 0;
    m_statusSequence = 0;
    m_widebandSequence.fill(0);
    if (preferredOutboundBase != 0) {
        return bindRoleSockets(address, preferredOutboundBase);
    }

    // Contiguous role ports are part of the actual P2 negotiation. Search a
    // test-only range instead of replacing that mapping with independent
    // ephemeral ports.
    for (int candidate = 20000; candidate <= 60000 - kRoleSocketCount;
         candidate += kRoleSocketCount + 1) {
        if (bindRoleSockets(address, static_cast<quint16>(candidate))) {
            return true;
        }
    }
    return false;
}

bool P2FakeRadio::bindRoleSockets(const QHostAddress& address,
                                  quint16 outboundBase)
{
    clearSockets();

    for (int offset = 0; offset < kRoleSocketCount; ++offset) {
        auto* socket = new QUdpSocket(this);
        if (!socket->bind(address, static_cast<quint16>(outboundBase + offset),
                          QUdpSocket::DontShareAddress)) {
            delete socket;
            clearSockets();
            return false;
        }
        m_roleSockets[static_cast<std::size_t>(offset)] = socket;
        connect(socket, &QUdpSocket::readyRead, this,
                [this, offset]() { drainRoleSocket(offset); });
    }

    m_address = address;
    m_outboundBase = outboundBase;
    m_inputRoleBase = static_cast<quint16>(outboundBase + 1);
    m_ingressEnabled = true;
    return true;
}

void P2FakeRadio::stop()
{
    clearSockets();
    m_address = QHostAddress();
    m_outboundBase = 0;
    m_inputRoleBase = 0;
    m_clientAddress = QHostAddress();
    m_clientPort = 0;
}

void P2FakeRadio::clearSockets()
{
    if (m_wrongAddressSocket) {
        m_wrongAddressSocket->close();
        delete m_wrongAddressSocket;
        m_wrongAddressSocket = nullptr;
    }
    for (QUdpSocket*& socket : m_roleSockets) {
        if (socket) {
            socket->close();
            delete socket;
            socket = nullptr;
        }
    }
}

RadioInfo P2FakeRadio::radioInfo() const
{
    RadioInfo info;
    info.name = QStringLiteral("Fake P2 Orion MkII");
    info.macAddress = QStringLiteral("02:00:00:00:02:02");
    info.address = m_address;
    info.port = m_outboundBase;
    info.boardType = HPSDRHW::OrionMKII;
    info.firmwareVersion = 110;
    info.adcCount = 2;
    info.maxReceivers = 7;
    info.protocol = ProtocolVersion::Protocol2;
    info.hasDiversityReceiver = true;
    info.hasPureSignal = true;
    info.maxSampleRate = 1536000;
    return info;
}

void P2FakeRadio::drainRoleSocket(int offset)
{
    QUdpSocket* socket = m_roleSockets[static_cast<std::size_t>(offset)];
    if (!socket) {
        return;
    }

    while (socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = socket->receiveDatagram();
        if (datagram.data().isEmpty()) {
            continue;
        }

        ++m_totalEgressDatagrams;
        learnClient(datagram.senderAddress(), datagram.senderPort());

        // Host general commands use the outbound base port, are 60 bytes
        // and carry command 0x00 in byte 4; byte 38 is the watchdog.
        if (offset == 0 && datagram.data().size() == 60
            && datagram.data().at(4) == 0x00) {
            ++m_generalDatagrams;
            m_lastGeneralWatchdog = static_cast<quint8>(datagram.data().at(38));
        }

        // Host high-priority commands use outbound base + 3 and are 1444
        // bytes. Byte 4 carries run bit 0 and MOX bit 1.
        if (offset == 3 && datagram.data().size() == 1444) {
            ++m_highPriorityDatagrams;
            const quint8 flags = static_cast<quint8>(datagram.data().at(4));
            m_lastHighPriorityFlags = flags;
            m_lastHighPriorityOcByte =
                int(static_cast<quint8>(datagram.data().at(1401)) >> 1);
            if ((flags & 0x01u) == 0) {
                ++m_stopCount;
            }
            if ((flags & 0x02u) != 0) {
                ++m_moxAssertedCount;
            }
        }
    }
}

void P2FakeRadio::learnClient(const QHostAddress& address, quint16 port)
{
    m_clientAddress = address;
    m_clientPort = port;
}

void P2FakeRadio::sendFromRole(int roleIndex, const QByteArray& datagram)
{
    if (!m_ingressEnabled || !hasClient() || roleIndex < 0
        || roleIndex >= kRoleSocketCount) {
        return;
    }
    QUdpSocket* socket = m_roleSockets[static_cast<std::size_t>(roleIndex)];
    if (socket) {
        socket->writeDatagram(datagram, m_clientAddress, m_clientPort);
    }
}

QByteArray P2FakeRadio::buildDdcPacket(quint32 sequence, float iSample,
                                       float qSample) const
{
    // The production parser consumes 1444 bytes: sequence [0..3], a 12-byte
    // header [4..15], then 238 signed I24/Q24 pairs [16..1443].
    QByteArray packet(1444, '\0');
    writeBe32(packet.data(), sequence);
    const quint32 iValue = static_cast<quint32>(toInt24(iSample));
    const quint32 qValue = static_cast<quint32>(toInt24(qSample));
    for (int sample = 0; sample < 238; ++sample) {
        const int offset = 16 + sample * 6;
        packet[offset + 0] = static_cast<char>((iValue >> 16) & 0xff);
        packet[offset + 1] = static_cast<char>((iValue >> 8) & 0xff);
        packet[offset + 2] = static_cast<char>(iValue & 0xff);
        packet[offset + 3] = static_cast<char>((qValue >> 16) & 0xff);
        packet[offset + 4] = static_cast<char>((qValue >> 8) & 0xff);
        packet[offset + 5] = static_cast<char>(qValue & 0xff);
    }
    return packet;
}

void P2FakeRadio::sendDdc(int ddc, float iSample, float qSample)
{
    if (ddc < 0 || ddc > 6) {
        return;
    }
    sendFromRole(kDdcSocketOffset + ddc,
                 buildDdcPacket(m_ddcSequence++, iSample, qSample));
}

void P2FakeRadio::sendStatus()
{
    QByteArray packet(60, '\0');
    writeBe32(packet.data(), m_statusSequence++);
    sendFromRole(kStatusSocketOffset, packet);
}

void P2FakeRadio::sendWideband(int adc)
{
    if (adc < 0 || adc >= 8) {
        return;
    }
    // Four-byte big-endian sequence followed by 512 signed 16-bit samples.
    QByteArray packet(1028, '\0');
    writeBe32(packet.data(),
              m_widebandSequence[static_cast<std::size_t>(adc)]++);
    sendFromRole(kWidebandSocketOffset + adc, packet);
}

void P2FakeRadio::sendStatusTo(const QHostAddress& clientAddress,
                               quint16 clientPort)
{
    if (!m_ingressEnabled || clientPort == 0) {
        return;
    }
    QByteArray packet(60, '\0');
    writeBe32(packet.data(), m_statusSequence++);
    QUdpSocket* socket = m_roleSockets[kStatusSocketOffset];
    if (socket) {
        socket->writeDatagram(packet, clientAddress, clientPort);
    }
}

void P2FakeRadio::sendMalformedStatus()
{
    sendFromRole(kStatusSocketOffset, QByteArray(59, '\0'));
}

void P2FakeRadio::sendWrongRoleStatus()
{
    QByteArray packet(60, '\0');
    writeBe32(packet.data(), m_statusSequence++);
    // Offset 0 is one port before the negotiated input-role base. It reaches
    // the real client socket from the selected address but must not map to any
    // current inbound role or refresh liveness.
    sendFromRole(0, packet);
}

bool P2FakeRadio::sendWrongAddressStatus()
{
    if (!m_ingressEnabled || !hasClient()) {
        return false;
    }

    if (!m_wrongAddressSocket) {
        m_wrongAddressSocket = new QUdpSocket(this);
        if (!m_wrongAddressSocket->bind(
                QHostAddress(QHostAddress::LocalHostIPv6), m_inputRoleBase,
                QUdpSocket::DontShareAddress)) {
            delete m_wrongAddressSocket;
            m_wrongAddressSocket = nullptr;
            return false;
        }
    }

    QByteArray packet(60, '\0');
    writeBe32(packet.data(), m_statusSequence++);
    return m_wrongAddressSocket->writeDatagram(
               packet, QHostAddress(QHostAddress::LocalHostIPv6),
               m_clientPort) == packet.size();
}

} // namespace NereusSDR::Test
