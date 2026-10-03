// =================================================================
// src/core/session/CoreAddresses.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See CoreAddresses.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Created for the phone's direct addresses. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The kernel's own temporary and deprecated flags on Linux
//                 (/proc/net/if_inet6), beside Qt's. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/CoreAddresses.h"

#include "core/StationNetwork.h"
#include "core/session/IceConfiguration.h"
#include "core/session/StationLanAnnouncer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <utility>

namespace NereusSDR {

namespace CoreAddresses {

namespace {

struct Ipv4Block {
    quint32 base;
    int prefix;
};

// RFC 6890's IPv4 special-purpose address registry, the blocks the public
// internet does not route to one computer.
constexpr Ipv4Block kSpecialIpv4[] = {
    {0x00000000u, 8},  // 0.0.0.0/8, this network
    {0x0A000000u, 8},  // 10.0.0.0/8, private
    {0x64400000u, 10}, // 100.64.0.0/10, shared (carrier-grade NAT)
    {0x7F000000u, 8},  // 127.0.0.0/8, loopback
    {0xA9FE0000u, 16}, // 169.254.0.0/16, link-local
    {0xAC100000u, 12}, // 172.16.0.0/12, private
    {0xC0000000u, 24}, // 192.0.0.0/24, protocol assignments
    {0xC0000200u, 24}, // 192.0.2.0/24, documentation
    {0xC0586300u, 24}, // 192.88.99.0/24, 6to4 relay anycast
    {0xC0A80000u, 16}, // 192.168.0.0/16, private
    {0xC6120000u, 15}, // 198.18.0.0/15, benchmarking
    {0xC6336400u, 24}, // 198.51.100.0/24, documentation
    {0xCB007100u, 24}, // 203.0.113.0/24, documentation
    {0xE0000000u, 4},  // 224.0.0.0/4, multicast
    {0xF0000000u, 4},  // 240.0.0.0/4, reserved, and the broadcast address
};

// linux/if_addr.h: the ifa_flags bits /proc/net/if_inet6 prints.
constexpr quint32 kIfaFlagTemporary = 0x01;  // IFA_F_TEMPORARY
constexpr quint32 kIfaFlagDeprecated = 0x20; // IFA_F_DEPRECATED

bool inBlock(quint32 address, const Ipv4Block& block)
{
    const quint32 mask = block.prefix == 0 ? 0u : ~0u << (32 - block.prefix);
    return (address & mask) == block.base;
}

} // namespace

bool isStableGlobalIpv6(const QNetworkAddressEntry& entry)
{
    const QHostAddress ip = entry.ip();
    if (ip.protocol() != QAbstractSocket::IPv6Protocol) {
        return false;
    }
    // IPv4-mapped (::ffff:a.b.c.d) is outside 2000::/3, so it never passes.
    if (!IceConfiguration::isUsableLocalAddress(ip)) {
        return false;
    }
    // A temporary privacy address (RFC 8981): Linux IFA_F_TEMPORARY, macOS
    // IN6_IFF_TEMPORARY and Windows' skip-as-source addresses reach Qt as
    // not eligible for DNS.
    if (entry.dnsEligibility() == QNetworkAddressEntry::DnsIneligible) {
        return false;
    }
    // Deprecated: a renumbered prefix's old address stays on the interface
    // until its valid lifetime ends, but new connections should not use it.
    if (entry.isLifetimeKnown() && entry.preferredLifetime().hasExpired()) {
        return false;
    }
    return true;
}

bool isPublicIpv4(const QHostAddress& address)
{
    if (address.protocol() != QAbstractSocket::IPv4Protocol) {
        return false;
    }
    const quint32 value = address.toIPv4Address();
    return std::none_of(std::begin(kSpecialIpv4), std::end(kSpecialIpv4),
                        [value](const Ipv4Block& block) { return inBlock(value, block); });
}

QStringList dialable(const QList<QNetworkAddressEntry>& entries,
                     const QHostAddress& listener, quint16 port)
{
    if (port == 0) {
        return {};
    }
    QList<QHostAddress> ipv6;
    QList<QHostAddress> ipv4;
    for (const QNetworkAddressEntry& entry : entries) {
        QHostAddress ip = entry.ip();
        ip.setScopeId(QString());
        if (!stationLanListenerServesAddress(listener, ip)) {
            continue;
        }
        if (isStableGlobalIpv6(entry)) {
            if (!ipv6.contains(ip)) {
                ipv6.append(ip);
            }
        } else if (isPublicIpv4(ip)) {
            if (!ipv4.contains(ip)) {
                ipv4.append(ip);
            }
        }
    }
    std::sort(ipv6.begin(), ipv6.end(), [](const QHostAddress& a, const QHostAddress& b) {
        const Q_IPV6ADDR left = a.toIPv6Address();
        const Q_IPV6ADDR right = b.toIPv6Address();
        return std::lexicographical_compare(std::begin(left.c), std::end(left.c),
                                            std::begin(right.c), std::end(right.c));
    });
    std::sort(ipv4.begin(), ipv4.end(), [](const QHostAddress& a, const QHostAddress& b) {
        return a.toIPv4Address() < b.toIPv4Address();
    });

    QStringList out;
    const QString portText = QString::number(port);
    for (const QHostAddress& ip : std::as_const(ipv6)) {
        out.append(QStringLiteral("[%1]:%2").arg(ip.toString(), portText));
    }
    for (const QHostAddress& ip : std::as_const(ipv4)) {
        out.append(QStringLiteral("%1:%2").arg(ip.toString(), portText));
    }
    if (out.size() > kMaxAddresses) {
        out = out.mid(0, kMaxAddresses);
    }
    return out;
}

QHash<QString, KernelIpv6Flags> parseIfInet6(const QByteArray& text)
{
    QHash<QString, KernelIpv6Flags> flags;
    for (const QByteArray& line : text.split('\n')) {
        const QList<QByteArray> fields = line.simplified().split(' ');
        if (fields.size() < 6 || fields.at(0).size() != 32) {
            continue;
        }
        const QByteArray bytes = QByteArray::fromHex(fields.at(0));
        bool ok = false;
        const quint32 bits = fields.at(4).toUInt(&ok, 16);
        if (bytes.size() != 16 || !ok) {
            continue;
        }
        const QHostAddress address(reinterpret_cast<const quint8*>(bytes.constData()));
        KernelIpv6Flags entry;
        entry.temporary = (bits & kIfaFlagTemporary) != 0;
        entry.deprecated = (bits & kIfaFlagDeprecated) != 0;
        flags.insert(address.toString(), entry);
    }
    return flags;
}

QList<QNetworkAddressEntry> withKernelFlags(QList<QNetworkAddressEntry> entries,
                                            const QHash<QString, KernelIpv6Flags>& flags)
{
    for (QNetworkAddressEntry& entry : entries) {
        QHostAddress ip = entry.ip();
        ip.setScopeId(QString());
        const auto it = flags.constFind(ip.toString());
        if (it == flags.cend()) {
            continue;
        }
        if (it->temporary) {
            entry.setDnsEligibility(QNetworkAddressEntry::DnsIneligible);
        }
        if (it->deprecated) {
            const QDeadlineTimer valid =
                entry.isLifetimeKnown() ? entry.validityLifetime() : QDeadlineTimer::Forever;
            entry.setAddressLifetime(QDeadlineTimer(0), valid);
        }
    }
    return entries;
}

QList<QNetworkAddressEntry> localEntries()
{
    QList<QNetworkAddressEntry> entries = StationNetwork::localEntries();
#ifdef Q_OS_LINUX
    QFile file(QStringLiteral("/proc/net/if_inet6"));
    if (file.open(QIODevice::ReadOnly)) {
        entries = withKernelFlags(std::move(entries), parseIfInet6(file.readAll()));
    }
#endif
    return entries;
}

QString toJson(const QStringList& addresses)
{
    const QJsonObject object{{QStringLiteral("addresses"), QJsonArray::fromStringList(addresses)}};
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

} // namespace CoreAddresses

CoreAddressWatcher::CoreAddressWatcher(QObject* parent)
    : QObject(parent)
    , m_source(&CoreAddresses::localEntries)
{
    m_timer.setInterval(CoreAddresses::kRefreshIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &CoreAddressWatcher::refresh);
}

void CoreAddressWatcher::setEntrySource(EntrySource source)
{
    m_source = source ? std::move(source) : EntrySource(&CoreAddresses::localEntries);
    if (m_active) {
        refresh();
    }
}

void CoreAddressWatcher::start(const QHostAddress& listener, quint16 port)
{
    m_listener = listener;
    m_port = port;
    m_active = true;
    refresh();
    m_timer.start();
}

void CoreAddressWatcher::stop()
{
    m_timer.stop();
    m_active = false;
    m_listener = {};
    m_port = 0;
    publish({});
}

void CoreAddressWatcher::refresh()
{
    if (!m_active) {
        return;
    }
    publish(CoreAddresses::dialable(m_source(), m_listener, m_port));
}

void CoreAddressWatcher::publish(const QStringList& addresses)
{
    if (addresses == m_addresses) {
        return;
    }
    m_addresses = addresses;
    emit addressesChanged(m_addresses);
}

} // namespace NereusSDR
