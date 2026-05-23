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

#include <cstring>

// BSD socket headers are only needed for the SO_BROADCAST setsockopt() path
// gated on Q_OS_UNIX below.  Windows builds use QUdpSocket native API only
// and have no sys/socket.h equivalent in the toolchain.
#ifdef Q_OS_UNIX
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#endif

namespace NereusSDR {

// PGXL appears to validate that radio_license_id starts with FlexRadio
// Systems' OUI prefix 00-1C-2D. Bench finding 2026-05-19: using the host's
// actual MAC (e.g. 2C-CF-67-XX-XX-XX) caused PGXL to reject the beacon
// silently. We spoof the prefix while keeping the last 3 octets derived from
// the host MAC for per-install determinism.
static QString spoofFlexLicenseId(const QString& hostMacDashed)
{
    const QStringList parts = hostMacDashed.split(QLatin1Char('-'));
    if (parts.size() == 6) {
        return QStringLiteral("00-1C-2D-%1-%2-%3")
            .arg(parts[3].toUpper())
            .arg(parts[4].toUpper())
            .arg(parts[5].toUpper());
    }
    return QStringLiteral("00-1C-2D-00-00-00");
}

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

void FlexRadioDiscoveryBroadcaster::setModel(const QString& model)
{
    m_model = model;
}

void FlexRadioDiscoveryBroadcaster::start()
{
    // Refresh LAN IP on every start() call in case interface state changed.
    m_ip = detectLanIpv4();
    if (m_ip == QStringLiteral("0.0.0.0")) {
        qCWarning(lcFlexDisc) << "No LAN-facing IPv4 found; beacon ip= will be 0.0.0.0";
    }

    // Compute the subnet broadcast address for the interface owning m_ip.
    // This ensures the beacon reaches all hosts on the same subnet, even on
    // multi-interface Macs where the kernel's route for 255.255.255.255 may
    // pick the wrong interface.
    m_broadcastAddress = computeBroadcastAddress();

    // Bind to the LAN IP:4992 so the source address in the broadcast packet
    // matches the interface the beacon will egress on. Port 4992 may already
    // be claimed, so fall back to ephemeral port if needed.
    const QHostAddress bindAddr = m_ip.isEmpty()
        ? QHostAddress::AnyIPv4
        : QHostAddress(m_ip);

    if (!m_socket.isValid()
            || m_socket.state() != QAbstractSocket::BoundState) {
        // Try to set SO_BROADCAST BEFORE bind. Some systems require this order.
        // Get the socket fd from the unbound socket (may be invalid, but we try anyway).
#ifdef Q_OS_UNIX
        int sockFd = static_cast<int>(m_socket.socketDescriptor());
        if (sockFd >= 0) {
            int yes = 1;
            ::setsockopt(sockFd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
        }
#endif

        bool bound = m_socket.bind(bindAddr, 4992,
                                   QUdpSocket::ShareAddress
                                       | QUdpSocket::ReuseAddressHint);
        if (!bound) {
            // Port 4992 may already be in use (e.g. by SmartSDR instance).
            // Fall back to ephemeral source port.
            bound = m_socket.bind(bindAddr, 0,
                                  QUdpSocket::ShareAddress
                                      | QUdpSocket::ReuseAddressHint);
        }
        if (!bound) {
            qCWarning(lcFlexDisc)
                << "UDP bind failed:"
                << m_socket.errorString()
                << "- discovery beacon disabled";
            return;
        }

        // Re-set SO_BROADCAST after bind to be sure (some impls require post-bind set).
#ifdef Q_OS_UNIX
        sockFd = static_cast<int>(m_socket.socketDescriptor());
        if (sockFd >= 0) {
            int yes = 1;
            if (::setsockopt(sockFd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) != 0) {
                qCWarning(lcFlexDisc) << "setsockopt SO_BROADCAST failed on fd" << sockFd << "errno" << errno;
            } else {
                qCDebug(lcFlexDisc) << "SO_BROADCAST set on fd" << sockFd << "after bind";
            }
        } else {
            qCWarning(lcFlexDisc) << "socketDescriptor() returned invalid fd after bind" << sockFd;
        }
#endif
    }

    if (!m_timer.isActive()) {
        m_timer.start();
        qCInfo(lcFlexDisc) << "FlexRadio discovery beacon started, src=" << m_ip
                           << "dst=" << m_broadcastAddress.toString() << ":4992";
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
    // Detect if the LAN IP has changed (e.g. DHCP renewal) and rebind the socket if so.
    const QString currentIp = detectLanIpv4();
    if (!currentIp.isEmpty() && currentIp != m_ip) {
        qCInfo(lcFlexDisc) << "LAN IP changed from" << m_ip << "to" << currentIp
                           << "- rebinding broadcaster";
        m_ip = currentIp;

        // Close and rebind the socket to the new IP.
        m_socket.close();
        const QHostAddress bindAddr(m_ip);

#ifdef Q_OS_UNIX
        int fd = static_cast<int>(m_socket.socketDescriptor());
        if (fd >= 0) {
            int yes = 1;
            ::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
        }
#endif

        bool bound = m_socket.bind(bindAddr, 4992,
                                   QUdpSocket::ShareAddress
                                       | QUdpSocket::ReuseAddressHint);
        if (!bound) {
            // Port 4992 may already be in use; fall back to ephemeral port.
            bound = m_socket.bind(bindAddr, 0,
                                  QUdpSocket::ShareAddress
                                      | QUdpSocket::ReuseAddressHint);
        }

        if (bound) {
            // Re-set SO_BROADCAST after the rebind.
#ifdef Q_OS_UNIX
            fd = static_cast<int>(m_socket.socketDescriptor());
            if (fd >= 0) {
                int yes = 1;
                ::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
            }
#endif
            m_broadcastAddress = computeBroadcastAddress();
            qCInfo(lcFlexDisc) << "Broadcaster rebound to src=" << m_ip
                               << "dst=" << m_broadcastAddress.toString();
        } else {
            qCWarning(lcFlexDisc) << "Broadcaster rebind failed for new IP" << m_ip;
            return;
        }
    }

    const quint32 now = static_cast<quint32>(QDateTime::currentSecsSinceEpoch());
    const QByteArray pkt = buildBeacon(m_packetCount, now);

    const int sockFd = static_cast<int>(m_socket.socketDescriptor());
    if (sockFd < 0) {
        qCWarning(lcFlexDisc) << "broadcast: invalid socket fd";
        return;
    }

    // Try writing to 255.255.255.255 (limited broadcast) with SO_BROADCAST set.
    // This is more portable than trying to compute and use subnet broadcast addresses.
    qint64 written = m_socket.writeDatagram(pkt,
                                            QHostAddress::Broadcast,
                                            /*port=*/4992);
    if (written != pkt.size()) {
        qCWarning(lcFlexDisc) << "broadcast write failed (fd=" << sockFd << "):"
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
// Corrected wire format from FLEX-8600 v4.2.18.41174 bench diff 2026-05-19
// (captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng).
// Prior code had a 4-byte Class ID offset bug: it packed the OUI+info bytes
// at bytes 4-7 and omitted the FF FF Packet Class Code, causing PGXL to
// silently reject every beacon. TGXL accepted because it has a looser parser.
//
//   Header (28 bytes, big-endian):
//     Word 0 (bytes 0-3):
//       byte 0: 0x38  (Type 3 = Context Packet without Stream ID, Class ID present)
//       byte 1: 0x50 | (packetCount & 0x0F)
//               (top nibble 0x5 = TSI=01 UTC + TSF=01 real-time;
//                low nibble = rolling 4-bit sequence)
//       bytes 2-3: 16-bit big-endian packet size in 32-bit words
//     Word 1 (bytes 4-7):  00 00 08 00  (VITA dialect marker, standalone field)
//     Words 2-3 (bytes 8-15): 8-byte VITA-49 Class ID block:
//       bytes 8-11:  00 00 1C 2D  (pad=0x00, OUI = FlexRadio Systems 00-1C-2D)
//       bytes 12-15: 53 4C FF FF  (Info Class "SL", Packet Class 0xFFFF)
//     Word 4 (bytes 16-19): 32-bit big-endian UNIX epoch seconds
//     Word 5 (bytes 20-23): fractional timestamp (zero)
//     Word 6 (bytes 24-27): reserved (zero)
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
    // PGXL likely validates that version is in Flex's 4-part X.Y.Z.NNNNN format
    // (real radios send e.g. 4.2.18.41174). NereusSDR's actual app version
    // (0.5.1) is 3-part and looks non-Flex. Use a Flex-shaped placeholder so
    // PGXL accepts the beacon. Operator can override via setVersion().
    const QString versionStr  = m_version.isEmpty()
                                    ? QStringLiteral("4.0.0.1")
                                    : m_version;
    const QString nicknameStr = m_nickname.isEmpty()
                                    ? QStringLiteral("NereusSDR")
                                    : m_nickname;
    const QString callsignStr = m_callsign.isEmpty()
                                    ? QStringLiteral("NEREUS")
                                    : m_callsign;
    const QString modelStr    = m_model.isEmpty()
                                    ? QStringLiteral("FLEX-6400")
                                    : m_model;
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
    payload += QStringLiteral("model=") + modelStr;
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
    payload += QStringLiteral("radio_license_id=") + spoofFlexLicenseId(macStr);
    payload += QLatin1Char(' ');
    payload += QStringLiteral("fpc_mac=00:00:00:00:00:00");
    payload += QLatin1Char(' ');
    // PGXL appears to filter the FlexRadio dropdown to wan_connected=1 entries.
    // We're not actually WAN-routed but the value is metadata, not validated.
    payload += QStringLiteral("wan_connected=1");
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

    // Word 0 (bytes 0-3): packet-type / TSI / TSF / packet-count / packet-size
    const quint8 byte1 = static_cast<quint8>(0x50U | (packetCount & 0x0FU));
    hdr.append(static_cast<char>(0x38));
    hdr.append(static_cast<char>(byte1));
    hdr.append(static_cast<char>((totalWords >> 8) & 0xFF));
    hdr.append(static_cast<char>(totalWords & 0xFF));

    // Word 1 (bytes 4-7): VITA dialect marker; verbatim from FLEX reference.
    // Bench capture (2026-05-19): this word is a standalone field, NOT part of
    // the 8-byte Class ID block that follows.
    hdr.append(static_cast<char>(0x00));
    hdr.append(static_cast<char>(0x00));
    hdr.append(static_cast<char>(0x08));
    hdr.append(static_cast<char>(0x00));

    // Words 2-3 (bytes 8-15): 8-byte VITA-49 Class ID block.
    //   byte  8: Pad Bit Count = 0x00
    //   bytes 9-11: OUI = 0x00-0x1C-0x2D (FlexRadio Systems)
    //   bytes 12-13: Information Class Code = 0x53 0x4C ("SL")
    //   bytes 14-15: Packet Class Code = 0xFF 0xFF
    // Bench diff: prior code emitted only 4 bytes here (1C 2D 53 4C), omitting
    // the leading 00 00 pad+OUI prefix and the trailing FF FF class code.
    // PGXL rejects packets with a malformed Class ID; TGXL accepted them.
    static const quint8 kClassId[8] = {
        0x00, 0x00, 0x1C, 0x2D,
        0x53, 0x4C, 0xFF, 0xFF
    };
    hdr.append(reinterpret_cast<const char*>(kClassId), 8);

    // Word 4 (bytes 16-19): Integer Timestamp (Unix epoch seconds, BE uint32).
    hdr.append(static_cast<char>((unixSeconds >> 24) & 0xFF));
    hdr.append(static_cast<char>((unixSeconds >> 16) & 0xFF));
    hdr.append(static_cast<char>((unixSeconds >>  8) & 0xFF));
    hdr.append(static_cast<char>( unixSeconds        & 0xFF));

    // Words 5-6 (bytes 20-27): Fractional timestamp + reserved (all zeros).
    hdr.append(QByteArray(4, '\0')); // fractional seconds hi (word 5) = 0
    hdr.append(QByteArray(4, '\0')); // fractional seconds lo / reserved (word 6) = 0

    Q_ASSERT(hdr.size() == 28);

    return hdr + payloadBytes;
}

QString FlexRadioDiscoveryBroadcaster::detectLanIpv4() const
{
    // Primary: kernel route lookup via connected UDP socket.
    //
    // On multi-interface hosts (e.g. macOS with feth* virtual ethernets coexisting
    // with en0) iterating QNetworkInterface order can return a virtual interface
    // before the real LAN nic. That mismatches the source IP the kernel actually
    // uses for outbound TCP — including the IP we advertise in PGXL amplifier
    // create. So PGXL gets a beacon from .52 but a TCP src of .88, and silently
    // fails to TCP-connect back to us.
    //
    // The fix: ask the kernel which local IP it would use to reach a remote.
    // QUdpSocket::connectToHost() on UDP is a *local* operation; no packets are
    // sent. The kernel performs a route lookup, picks the egress interface, and
    // binds the local socket address. localAddress() then returns that source IP,
    // which by construction matches the source IP of any subsequent TCP socket
    // to the same destination (assuming the default route).
    //
    // We probe 8.8.8.8 (Google DNS) because it always resolves through the
    // default route and is reachable everywhere we'd plausibly run, but no
    // packet is ever sent so its identity is irrelevant. A peer hint can be
    // set via setPeerHint() if we need to scope to a specific subnet (e.g. when
    // PGXL is on a different routing domain than the default gateway).
    {
        QUdpSocket probe;
        const QHostAddress target = m_peerHint.isNull()
            ? QHostAddress(QStringLiteral("8.8.8.8"))
            : m_peerHint;
        probe.connectToHost(target, /*port=*/9 /* discard */);
        if (probe.waitForConnected(/*ms=*/50)) {
            const QHostAddress local = probe.localAddress();
            if (local.protocol() == QAbstractSocket::IPv4Protocol
                    && local != QHostAddress::LocalHost
                    && local != QHostAddress::AnyIPv4
                    && !local.isNull()) {
                return local.toString();
            }
        }
    }

    // Fallback: iterate interfaces but skip virtual / point-to-point types.
    // On macOS feth* (fake-ethernet) appears before en0 in QNetworkInterface
    // enumeration and is up+running with a 192.168.x.x address, which would
    // otherwise win the original first-match scan.
    static const QStringList kSkipPrefixes = {
        QStringLiteral("feth"),
        QStringLiteral("utun"),
        QStringLiteral("awdl"),
        QStringLiteral("llw"),
        QStringLiteral("bridge"),
        QStringLiteral("vmnet"),
        QStringLiteral("p2p"),
        QStringLiteral("ipsec"),
        QStringLiteral("gif"),
        QStringLiteral("stf"),
    };
    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : ifaces) {
        if (!iface.flags().testFlag(QNetworkInterface::IsUp)) { continue; }
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) { continue; }
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) { continue; }

        const QString name = iface.name();
        bool skip = false;
        for (const QString& prefix : kSkipPrefixes) {
            if (name.startsWith(prefix)) { skip = true; break; }
        }
        if (skip) { continue; }

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

QHostAddress FlexRadioDiscoveryBroadcaster::computeBroadcastAddress() const
{
    // For the LAN IP m_ip, find its associated network interface and return
    // the broadcast address for that subnet. This ensures the beacon reaches
    // all hosts on the same subnet, even on multi-interface systems where the
    // kernel's route for 255.255.255.255 may pick the wrong interface.
    //
    // On multi-interface Macs, binding the socket source to m_ip + sending to
    // the subnet broadcast address ensures the packet egresses the correct
    // interface.
    if (m_ip.isEmpty()) {
        return QHostAddress::Broadcast;
    }

    const QHostAddress lanAddr(m_ip);
    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : ifaces) {
        if (!iface.flags().testFlag(QNetworkInterface::IsUp)) { continue; }

        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip() == lanAddr) {
                const QHostAddress bcast = entry.broadcast();
                if (!bcast.isNull()) {
                    return bcast;
                }
            }
        }
    }

    // Fallback to limited broadcast if subnet broadcast not found.
    return QHostAddress::Broadcast;
}

}  // namespace NereusSDR
