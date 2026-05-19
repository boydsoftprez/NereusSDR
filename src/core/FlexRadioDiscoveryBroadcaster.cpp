// =================================================================
// src/core/FlexRadioDiscoveryBroadcaster.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original: no Thetis or AetherSDR upstream.
// Wire format reverse-engineered from FLEX-8600 v4.2.18.41174 discovery
// beacon captured 2026-05-19
// (captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19 - Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

#include "FlexRadioDiscoveryBroadcaster.h"

#include <QDateTime>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QLoggingCategory>

namespace NereusSDR {

// Wire format reference: FLEX-8600 v4.2.18.41174 discovery beacon
// captured 2026-05-19
// (captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng).
//
// Packet structure:
//   28-byte VITA-49-style header
//   ASCII key=value payload (space-separated, padded to 4-byte multiple)
Q_LOGGING_CATEGORY(lcFlexDisc, "nereus.flex.disc", QtInfoMsg)

FlexRadioDiscoveryBroadcaster::FlexRadioDiscoveryBroadcaster(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(1000); // 1 Hz
    connect(&m_timer, &QTimer::timeout, this, &FlexRadioDiscoveryBroadcaster::onTick);
}

void FlexRadioDiscoveryBroadcaster::setSerial(const QString& serial)
{
    m_serial = serial;
}

void FlexRadioDiscoveryBroadcaster::setNickname(const QString& nickname)
{
    m_nickname = nickname;
}

void FlexRadioDiscoveryBroadcaster::setCallsign(const QString& callsign)
{
    m_callsign = callsign;
}

void FlexRadioDiscoveryBroadcaster::setVersion(const QString& version)
{
    m_version = version;
}

void FlexRadioDiscoveryBroadcaster::setMacAddress(const QString& macColonForm)
{
    // Convert "aa:bb:cc:dd:ee:ff" -> "AA-BB-CC-DD-EE-FF"
    m_mac = macColonForm;
    m_mac.replace(':', '-');
    m_mac = m_mac.toUpper();
}

void FlexRadioDiscoveryBroadcaster::start()
{
    // Refresh LAN IP on every start() call in case interface state changed.
    m_ip = detectLanIpv4();
    if (m_ip == QStringLiteral("0.0.0.0")) {
        qCWarning(lcFlexDisc) << "No LAN-facing IPv4 found; beacon ip= will be 0.0.0.0";
    }

    // Bind to 0.0.0.0:4992 with ShareAddress + ReuseAddressHint so we
    // coexist with other listeners on port 4992 (e.g. a SmartSDR instance
    // running locally). Broadcast write still works via QUdpSocket because
    // Qt sets SO_BROADCAST implicitly when writing to QHostAddress::Broadcast.
    if (!m_socket.isValid()
            || m_socket.state() != QAbstractSocket::BoundState) {
        bool bound = m_socket.bind(QHostAddress::AnyIPv4, 4992,
                                   QUdpSocket::ShareAddress
                                       | QUdpSocket::ReuseAddressHint);
        if (!bound) {
            qCWarning(lcFlexDisc)
                << "UDP 4992 bind failed:"
                << m_socket.errorString()
                << "- discovery beacon disabled";
            return;
        }
    }

    if (!m_timer.isActive()) {
        m_timer.start();
        qCInfo(lcFlexDisc) << "FlexRadio discovery beacon started on UDP 4992"
                           << "ip=" << m_ip;
    }
}

void FlexRadioDiscoveryBroadcaster::stop()
{
    if (m_timer.isActive()) {
        m_timer.stop();
        qCInfo(lcFlexDisc) << "FlexRadio discovery beacon stopped";
    }
}

void FlexRadioDiscoveryBroadcaster::onTick()
{
    const quint32 now = static_cast<quint32>(QDateTime::currentSecsSinceEpoch());
    const QByteArray pkt = buildBeacon(m_packetCount, now);

    // Bind source port to 4992 so the FLEX-style src_port=4992 is preserved.
    // QUdpSocket::writeDatagram with bound socket works for broadcast on macOS
    // provided SO_BROADCAST is set; QUdpSocket sets it implicitly when
    // writing to a broadcast address.
    qint64 written = m_socket.writeDatagram(pkt,
                                            QHostAddress::Broadcast,
                                            /*port=*/4992);
    if (written != pkt.size()) {
        qCWarning(lcFlexDisc) << "broadcast write failed:"
                              << m_socket.errorString();
    }
    m_packetCount = (m_packetCount + 1) & 0x0F; // 4-bit rolling counter 0..15
}

QByteArray FlexRadioDiscoveryBroadcaster::buildBeaconForTesting(
    quint8 packetCount, quint32 unixSeconds) const
{
    return buildBeacon(packetCount, unixSeconds);
}

// Build the full VITA-49-style discovery beacon packet.
//
// Wire format from FLEX-8600 v4.2.18.41174 discovery beacon captured
// 2026-05-19
// (captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng):
//
//   Header (28 bytes, big-endian):
//     Word 0  (4 bytes):
//       byte 0: 0x38  (Type 3 = Context Packet without Stream ID, Class ID present)
//       byte 1: 0x50 | (packetCount & 0x0F)
//               (top nibble 0x5 = TSI=01 UTC + TSF=01 real-time;
//                low nibble = rolling 4-bit sequence)
//       bytes 2-3: 16-bit big-endian packet size in 32-bit words
//     Words 1-2 (Class ID, 8 bytes): 00 00 08 00 1C 2D 53 4C (constant)
//     Word 3 (4 bytes): 32-bit big-endian UNIX epoch seconds
//     Word 4 (4 bytes): fractional timestamp hi (zero)
//     Word 5 (4 bytes): fractional timestamp lo (zero)
//     Word 6 (4 bytes): 00 00 00 00 (reserved/padding)
//
//   Payload: ASCII key=value pairs, space-separated, no terminator.
//   Total packet length must be a multiple of 4 bytes (padded with spaces).
QByteArray FlexRadioDiscoveryBroadcaster::buildBeacon(
    quint8 packetCount, quint32 unixSeconds) const
{
    // Build the ASCII payload from discovery key=value pairs.
    // Key order follows the FLEX-8600 capture verbatim.
    const QString serialStr   = m_serial.isEmpty()
                                    ? QStringLiteral("0000-0000-0000-0000")
                                    : m_serial;
    const QString versionStr  = m_version.isEmpty()
                                    ? QStringLiteral("0.5.1")
                                    : m_version;
    const QString nicknameStr = m_nickname.isEmpty()
                                    ? QStringLiteral("NereusSDR")
                                    : m_nickname;
    const QString callsignStr = m_callsign.isEmpty()
                                    ? QStringLiteral("NEREUS")
                                    : m_callsign;
    const QString ipStr       = m_ip.isEmpty()
                                    ? QStringLiteral("0.0.0.0")
                                    : m_ip;
    const QString macStr      = m_mac.isEmpty()
                                    ? QStringLiteral("00-00-00-00-00-00")
                                    : m_mac;

    QString payload;
    payload.reserve(512);
    payload += QStringLiteral("discovery_protocol_version=3.1.0.4");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("model=NereusSDR");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("serial=") + serialStr;
    payload += QLatin1Char(' ');
    payload += QStringLiteral("version=") + versionStr;
    payload += QLatin1Char(' ');
    payload += QStringLiteral("nickname=") + nicknameStr;
    payload += QLatin1Char(' ');
    payload += QStringLiteral("callsign=") + callsignStr;
    payload += QLatin1Char(' ');
    payload += QStringLiteral("ip=") + ipStr;
    payload += QLatin1Char(' ');
    payload += QStringLiteral("port=4992");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("status=Available");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("inuse_ip=");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("inuse_host=");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("max_licensed_version=v4");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("radio_license_id=") + macStr;
    payload += QLatin1Char(' ');
    payload += QStringLiteral("fpc_mac=00:00:00:00:00:00");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("wan_connected=0");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("licensed_clients=2");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("available_clients=2");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("max_panadapters=4");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("available_panadapters=4");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("max_slices=4");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("available_slices=4");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("gui_client_ips=");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("gui_client_hosts=");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("gui_client_programs=");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("gui_client_stations=");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("gui_client_handles=");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("min_software_version=2.13.0.0");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("external_port_link=1");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("license_is_unknown=0");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("is_system_model=0");
    payload += QLatin1Char(' ');
    payload += QStringLiteral("turf_region=USA");

    // Convert payload to UTF-8 bytes (ASCII only; no special chars expected).
    QByteArray payloadBytes = payload.toUtf8();

    // Pad the total packet length (header + payload) to a multiple of 4 bytes
    // by appending spaces to the payload.
    const int totalBeforePad = 28 + payloadBytes.size();
    const int remainder = totalBeforePad % 4;
    if (remainder != 0) {
        payloadBytes.append(QByteArray(4 - remainder, ' '));
    }

    const int totalBytes = 28 + payloadBytes.size();
    Q_ASSERT(totalBytes % 4 == 0);

    // Build the 28-byte header.
    // Packet size in 32-bit words (including the header).
    const quint16 totalWords = static_cast<quint16>(totalBytes / 4);

    QByteArray hdr;
    hdr.reserve(28);

    // Word 0: packet-type / TSI / TSF / packet-count / packet-size
    const quint8 byte1 = static_cast<quint8>(0x50U | (packetCount & 0x0FU));
    hdr.append(static_cast<char>(0x38));
    hdr.append(static_cast<char>(byte1));
    hdr.append(static_cast<char>((totalWords >> 8) & 0xFF));
    hdr.append(static_cast<char>(totalWords & 0xFF));

    // Words 1-2: Class ID (8 bytes, constant)
    // Value from FLEX-8600 capture: 00 00 08 00 1C 2D 53 4C
    static const quint8 kClassId[8] = {
        0x00, 0x00, 0x08, 0x00,
        0x1C, 0x2D, 0x53, 0x4C
    };
    hdr.append(reinterpret_cast<const char*>(kClassId), 8);

    // Words 3-4: Timestamp
    //   bytes 0-3: 32-bit big-endian UNIX epoch seconds (Integer Timestamp)
    //   bytes 4-7: 32-bit fractional seconds = 0
    hdr.append(static_cast<char>((unixSeconds >> 24) & 0xFF));
    hdr.append(static_cast<char>((unixSeconds >> 16) & 0xFF));
    hdr.append(static_cast<char>((unixSeconds >>  8) & 0xFF));
    hdr.append(static_cast<char>( unixSeconds        & 0xFF));
    hdr.append(QByteArray(4, '\0')); // fractional seconds hi (word 4) = 0
    hdr.append(QByteArray(4, '\0')); // fractional seconds lo (word 5) = 0
    // Word 6: reserved/padding (4 zero bytes)
    hdr.append(QByteArray(4, '\0'));

    Q_ASSERT(hdr.size() == 28);

    return hdr + payloadBytes;
}

QString FlexRadioDiscoveryBroadcaster::detectLanIpv4() const
{
    // Walk all network interfaces and return the first non-loopback,
    // IPv4, up-and-running unicast address. If none is found, return
    // "0.0.0.0" and let the caller log a warning.
    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : ifaces) {
        if (!iface.flags().testFlag(QNetworkInterface::IsUp)) { continue; }
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) { continue; }
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) { continue; }

        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            const QHostAddress addr = entry.ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol
                    && addr != QHostAddress::LocalHost) {
                return addr.toString();
            }
        }
    }
    return QStringLiteral("0.0.0.0");
}

}  // namespace NereusSDR
