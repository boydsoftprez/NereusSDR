// =================================================================
// src/core/RadioDiscovery.cpp  (NereusSDR)
// =================================================================
//
// Ported from mi0bot/Thetis-HL2 source:
//   HPSDR/clsRadioDiscovery.cs
//
// Original copyright and license (preserved per GNU GPL):
//
//   Thetis / Thetis-HL2 is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — Thetis heavy modifications
//   Copyright (C) 2020-2026  Reid Campbell (MI0BOT) — Hermes-Lite fork contributions
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions in the upstream Thetis source — preserved verbatim from
// Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

// From mi0bot/Thetis@Hermes-Lite clsRadioDiscovery.cs
// Porting: NIC-walk + per-NIC synchronous poll loop with tunable timing profiles.

#include "RadioDiscovery.h"
#include "BoardCapabilities.h"
#include "LogCategories.h"

#include <QDateTime>
#include <QNetworkInterface>

#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

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

// From Thetis clsRadioDiscovery.cs formatNicMac() / equivalent MAC string helper
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
    // Stale-sweep timer only. The continuous NIC walker that a 3I-4 subagent
    // added was blocking the main thread for 15-20s every 5s — removed. Scans
    // are now one-shot, user-triggered via ConnectionPanel. Async rewrite is
    // a follow-up.
    connect(&m_staleTimer, &QTimer::timeout, this, &RadioDiscovery::onStaleCheck);
}

RadioDiscovery::~RadioDiscovery()
{
    stopDiscovery();
}

void RadioDiscovery::startDiscovery()
{
    // Phase 3I rewrote discovery to walk all NICs per scan with ephemeral
    // per-NIC sockets (mi0bot clsRadioDiscovery pattern). Main's older
    // single-persistent-m_socket path was replaced entirely in Task 4;
    // main's cross-platform socket header fix (b3c2961) is preserved at
    // the top of this file, which is what scanAllNics() needs when it
    // does its own setsockopt(SO_BROADCAST) per NIC.
    emit discoveryStarted();
    scanAllNics();                              // one-shot NIC walk
    if (!m_staleTimer.isActive()) {
        m_staleTimer.start(kStaleTimeoutMs);
    }
    emit discoveryFinished();
}

void RadioDiscovery::stopDiscovery()
{
    m_staleTimer.stop();
    emit discoveryFinished();
}

QList<RadioInfo> RadioDiscovery::discoveredRadios() const
{
    return m_radios.values();
}

// ---------------------------------------------------------------------------
// parseP1Reply — extract RadioInfo from a P1 discovery response.
// From Thetis clsRadioDiscovery.cs parseDiscoveryReply() P1 branch (line ~1155).
//
// P1 reply layout (OpenHPSDR Protocol 1):
//   [0]    0xEF
//   [1]    0xFE
//   [2]    0x02 (available) or 0x03 (in use)
//   [3..8] MAC address (6 bytes)
//   [9]    firmware version (CodeVersion)
//   [10]   board type byte (mapP1DeviceType)
//   ... (optional extra fields depending on board)
// ---------------------------------------------------------------------------
bool RadioDiscovery::parseP1Reply(const QByteArray& bytes, const QHostAddress& source, RadioInfo& out)
{
    // From Thetis: data[0]==0xef && data[1]==0xfe && (data[2]==0x2 || data[2]==0x3)
    if (bytes.size() < 11) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(bytes[0]);
    const quint8 b1 = static_cast<quint8>(bytes[1]);
    const quint8 b2 = static_cast<quint8>(bytes[2]);

    if (b0 != 0xEF || b1 != 0xFE || (b2 != 0x02 && b2 != 0x03)) {
        return false;
    }

    out = RadioInfo{};
    out.protocol  = ProtocolVersion::Protocol1;
    out.inUse     = (b2 == 0x03);
    out.port      = 1024;

    // Convert IPv6-mapped IPv4 (::ffff:x.x.x.x) to pure IPv4
    bool ok = false;
    quint32 ipv4 = source.toIPv4Address(&ok);
    out.address = ok ? QHostAddress(ipv4) : source;

    // MAC: bytes 3-8
    out.macAddress = macToString(bytes.constData() + 3);

    // From Thetis: r.CodeVersion = data[9]
    out.firmwareVersion = static_cast<quint8>(bytes[9]);

    // From Thetis: r.DeviceType = mapP1DeviceType(data[10])
    // mapP1DeviceType: 0=Atlas, 1=Hermes, 2=HermesII, 4=Angelia, 5=Orion, 6=HermesLite, 10=OrionMKII
    quint8 boardByte = static_cast<quint8>(bytes[10]);
    switch (boardByte) {
    case 0:  out.boardType = HPSDRHW::Atlas;      break;
    case 1:  out.boardType = HPSDRHW::Hermes;     break;
    case 2:  out.boardType = HPSDRHW::HermesII;   break;
    case 4:  out.boardType = HPSDRHW::Angelia;    break;
    case 5:  out.boardType = HPSDRHW::Orion;      break;
    case 6:  out.boardType = HPSDRHW::HermesLite; break;  // MI0BOT: HL2 added
    case 10: out.boardType = HPSDRHW::OrionMKII;  break;
    default: out.boardType = static_cast<HPSDRHW>(boardByte); break;
    }

    // Optional extra fields (len > 20) — From Thetis parseDiscoveryReply P1 branch
    if (bytes.size() > 20) {
        out.maxReceivers = static_cast<quint8>(bytes[20]);
        if (out.maxReceivers <= 0) {
            out.maxReceivers = RadioInfo::maxReceiversForBoard(out.boardType);
        }
    }

    // Populate derived capabilities
    out.adcCount = RadioInfo::adcCountForBoard(out.boardType);
    if (out.maxReceivers <= 0) {
        out.maxReceivers = RadioInfo::maxReceiversForBoard(out.boardType);
    }
    out.name                = QString::fromLatin1(BoardCapsTable::forBoard(out.boardType).displayName);
    out.hasDiversityReceiver = (out.adcCount >= 2);
    out.hasPureSignal        = (out.boardType != HPSDRHW::Atlas && out.boardType != HPSDRHW::Unknown);
    out.maxSampleRate        = 384000;  // P1 max

    return true;
}

// ---------------------------------------------------------------------------
// parseP2Reply — extract RadioInfo from a P2 discovery response.
// From Thetis clsRadioDiscovery.cs parseDiscoveryReply() P2 branch (line ~1201).
//
// P2 reply layout (OpenHPSDR Protocol 2):
//   [0..3] 0x00 0x00 0x00 0x00
//   [4]    0x02 (available) or 0x03 (in use)
//   [5..10] MAC address (6 bytes)
//   [11]   board type (HPSDRHW enum value directly)
//   [12]   protocol supported byte
//   [13]   firmware version (CodeVersion)
//   [14..17] Mercury versions 0-3
//   [18]   Penny version
//   [19]   Metis version
//   [20]   number of hardware receivers
// ---------------------------------------------------------------------------
bool RadioDiscovery::parseP2Reply(const QByteArray& bytes, const QHostAddress& source, RadioInfo& out)
{
    // From Thetis: data[0]==0x0 && data[1]==0x0 && data[2]==0x0 && data[3]==0x0 && (data[4]==0x2 || data[4]==0x3)
    if (bytes.size() < 21) {
        return false;
    }

    const quint8 b0 = static_cast<quint8>(bytes[0]);
    const quint8 b1 = static_cast<quint8>(bytes[1]);
    const quint8 b2 = static_cast<quint8>(bytes[2]);
    const quint8 b3 = static_cast<quint8>(bytes[3]);
    const quint8 b4 = static_cast<quint8>(bytes[4]);

    if (b0 != 0x00 || b1 != 0x00 || b2 != 0x00 || b3 != 0x00 || (b4 != 0x02 && b4 != 0x03)) {
        return false;
    }

    out = RadioInfo{};
    out.protocol  = ProtocolVersion::Protocol2;
    out.inUse     = (b4 == 0x03);
    out.port      = 1024;

    // Convert IPv6-mapped IPv4 (::ffff:x.x.x.x) to pure IPv4
    bool ok = false;
    quint32 ipv4 = source.toIPv4Address(&ok);
    out.address = ok ? QHostAddress(ipv4) : source;

    // MAC: bytes 5-10
    out.macAddress = macToString(bytes.constData() + 5);

    // From Thetis: r.DeviceType = (HPSDRHW)data[11]
    out.boardType       = static_cast<HPSDRHW>(static_cast<quint8>(bytes[11]));
    // data[12] = ProtocolSupported (not stored in RadioInfo currently)
    out.firmwareVersion = static_cast<quint8>(bytes[13]);  // CodeVersion

    // From Thetis: if (len > 20) — receivers count
    if (bytes.size() > 20) {
        int hwRx = static_cast<quint8>(bytes[20]);
        out.maxReceivers = (hwRx > 0) ? hwRx : RadioInfo::maxReceiversForBoard(out.boardType);
    }

    // Populate derived capabilities
    out.adcCount = RadioInfo::adcCountForBoard(out.boardType);
    if (out.maxReceivers <= 0) {
        out.maxReceivers = RadioInfo::maxReceiversForBoard(out.boardType);
    }
    out.name                = QString::fromLatin1(BoardCapsTable::forBoard(out.boardType).displayName);
    out.hasDiversityReceiver = (out.adcCount >= 2);
    out.hasPureSignal        = (out.boardType != HPSDRHW::Atlas && out.boardType != HPSDRHW::Unknown);
    out.maxSampleRate        = 1536000;  // P2 supports higher sample rates

    qCDebug(lcDiscovery) << "P2 response from" << out.address.toString()
                         << "board:" << BoardCapsTable::forBoard(out.boardType).displayName
                         << "fw:" << out.firmwareVersion;

    return true;
}

// ---------------------------------------------------------------------------
// scanAllNics — mi0bot NIC-walk + per-NIC synchronous poll loop.
// From Thetis clsRadioDiscovery.cs DiscoverUsingAllNics() + discoverOnNic().
//
// For each eligible NIC:
//   1. Bind a temporary QUdpSocket to that NIC's IPv4 address.
//   2. For each attempt (attemptsPerNic): send P1+P2 broadcast probes.
//   3. Poll up to quietPollsBeforeResend × pollTimeoutMs for replies.
//   4. Parse replies, de-duplicate by MAC, emit radioDiscovered / radioUpdated.
// ---------------------------------------------------------------------------
void RadioDiscovery::scanAllNics()
{
    // From Thetis clsRadioDiscovery.cs buildDiscoveryPacketP1()
    QByteArray p1Packet(63, 0);
    p1Packet[0] = static_cast<char>(0xEF);
    p1Packet[1] = static_cast<char>(0xFE);
    p1Packet[2] = static_cast<char>(0x02);

    // From Thetis clsRadioDiscovery.cs buildDiscoveryPacketP2()
    QByteArray p2Packet(60, 0);
    p2Packet[4] = static_cast<char>(0x02);

    const DiscoveryTiming timing = timingFor(m_profile);
    const int attempts        = qMax(1, timing.attemptsPerNic);
    const int quietBeforeStop = qMax(1, timing.quietPollsBeforeResend);
    const int pollMs          = qMax(10, timing.pollTimeoutMs);

    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Global MAC de-duplication across all NICs for this scan cycle
    QSet<QString> seenThisScan;

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        // From Thetis isNicCandidate(): Up/Running, not loopback
        if (!(iface.flags() & QNetworkInterface::IsUp)
            || !(iface.flags() & QNetworkInterface::IsRunning)
            || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        // Find the first IPv4 address on this NIC
        QHostAddress nicIpv4;
        QHostAddress nicBroadcast;
        const auto entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            nicIpv4 = entry.ip();
            nicBroadcast = entry.broadcast();
            break;
        }

        if (nicIpv4.isNull()) {
            continue;
        }

        // From Thetis discoverOnNic(): bind to local IPv4 on ephemeral port
        QUdpSocket sock;
        sock.setSocketOption(QAbstractSocket::MulticastLoopbackOption, 0);
        if (!sock.bind(nicIpv4, 0)) {
            qCWarning(lcDiscovery) << "Failed to bind to" << nicIpv4.toString()
                                   << "on" << iface.name() << "— skipping";
            continue;
        }

        // Enable SO_BROADCAST. Windows setsockopt takes optval as
        // `const char*`; POSIX takes `const void*`. Casting to `const char*`
        // is valid on both (main's fix b3c2961 for the legacy startDiscovery
        // path — Phase 3I's scanAllNics added a second call site that needs
        // the same treatment for Windows CI to build).
        const auto fd = sock.socketDescriptor();
        if (fd >= 0) {
            int broadcastEnable = 1;
            ::setsockopt(static_cast<int>(fd), SOL_SOCKET, SO_BROADCAST,
                         reinterpret_cast<const char*>(&broadcastEnable),
                         sizeof(broadcastEnable));
        }

        qCDebug(lcDiscovery) << "Scanning NIC" << iface.name()
                             << nicIpv4.toString() << "profile" << static_cast<int>(m_profile);

        // From Thetis discoverOnNic(): attempts × (send + quiet-poll loop)
        for (int attempt = 0; attempt < attempts; attempt++) {
            // Send P1 and P2 probes to directed subnet broadcast and 255.255.255.255
            if (!nicBroadcast.isNull()) {
                sock.writeDatagram(p1Packet, nicBroadcast, kDiscoveryPort);
                sock.writeDatagram(p2Packet, nicBroadcast, kDiscoveryPort);
            }
            sock.writeDatagram(p1Packet, QHostAddress::Broadcast, kDiscoveryPort);
            sock.writeDatagram(p2Packet, QHostAddress::Broadcast, kDiscoveryPort);

            int quietPolls = 0;
            while (quietPolls < quietBeforeStop) {
                // From Thetis: s.Poll(pollMs * 1000, SelectMode.SelectRead)
                bool readable = sock.waitForReadyRead(pollMs);
                if (!readable) {
                    quietPolls++;
                    continue;
                }
                // Reset quiet counter on activity — replies may be bursty
                quietPolls = 0;

                while (sock.hasPendingDatagrams()) {
                    QHostAddress senderAddr;
                    quint16 senderPort = 0;
                    QByteArray data;
                    data.resize(static_cast<int>(sock.pendingDatagramSize()));
                    sock.readDatagram(data.data(), data.size(), &senderAddr, &senderPort);

                    if (data.size() < 11) {
                        continue;
                    }

                    RadioInfo info;
                    bool parsed = false;

                    const quint8 firstByte = static_cast<quint8>(data[0]);
                    if (firstByte == 0xEF) {
                        parsed = parseP1Reply(data, senderAddr, info);
                    } else if (firstByte == 0x00) {
                        parsed = parseP2Reply(data, senderAddr, info);
                    }

                    if (!parsed) {
                        continue;
                    }

                    // From Thetis: MAC-based de-duplication (seen set)
                    if (info.macAddress.isEmpty()
                        || info.macAddress == "00:00:00:00:00:00") {
                        continue;
                    }

                    if (seenThisScan.contains(info.macAddress)) {
                        continue;
                    }
                    seenThisScan.insert(info.macAddress);

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
        }

        sock.close();
    }
}

void RadioDiscovery::onStaleCheck()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList stale;

    for (auto it = m_lastSeen.constBegin(); it != m_lastSeen.constEnd(); ++it) {
        if (it.key() == m_connectedMac) { continue; }
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
