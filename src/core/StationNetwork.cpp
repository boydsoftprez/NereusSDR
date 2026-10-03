// no-port-check: NereusSDR-original. R-R3-48 station network address choice;
// R-R3-22 / R-R3-47 one bind rule for every station listener (StationBind).
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
// 2026-09-24: Lane B takes integration (R-IOS-01, R-R3-21): the off-network reason
// names amplifiers and tuners and the Core's configuration file. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-26: station_bind = "::" listens on IPv4 and IPv6, through
// the remote listener's DaemonConfig::listenAddressFor. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
#include "core/StationNetwork.h"

#include "core/daemon/DaemonConfig.h"

#include <QNetworkInterface>

namespace NereusSDR::StationNetwork {

QHostAddress plainIpv4(const QHostAddress& address)
{
    bool ok = false;
    const quint32 v4 = address.toIPv4Address(&ok);
    return ok ? QHostAddress(v4) : address;
}

QHostAddress addressFacing(const QHostAddress& peer,
                           const QList<QNetworkAddressEntry>& entries)
{
    const QHostAddress target = plainIpv4(peer);
    if (target.protocol() != QAbstractSocket::IPv4Protocol || target.isLoopback()) {
        return {};
    }
    for (const QNetworkAddressEntry& entry : entries) {
        const QHostAddress ip = entry.ip();
        if (ip.protocol() != QAbstractSocket::IPv4Protocol || ip.isLoopback()) {
            continue;
        }
        const int prefix = entry.prefixLength();
        if (prefix <= 0 || prefix > 32) {
            continue;
        }
        if (target.isInSubnet(ip, prefix)) {
            return ip;
        }
    }
    return {};
}

QList<QNetworkAddressEntry> localEntries()
{
    QList<QNetworkAddressEntry> entries;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        entries.append(iface.addressEntries());
    }
    return entries;
}

QList<QNetworkAddressEntry> StationBind::entries() const
{
    return entriesForTest ? *entriesForTest : localEntries();
}

bool StationBind::everyAddress() const
{
    if (bindOverride.isEmpty()) {
        return false;
    }
    const QHostAddress address(bindOverride);
    return address == QHostAddress(QHostAddress::AnyIPv4)
        || address == QHostAddress(QHostAddress::AnyIPv6)
        || address == QHostAddress(QHostAddress::Any);
}

QHostAddress StationBind::stationAddress() const
{
    if (!bindOverride.isEmpty()) {
        return QHostAddress(bindOverride);
    }
    if (radio.isNull()) {
        return {};
    }
    return addressFacing(radio, entries());
}

QList<QHostAddress> StationBind::listenAddresses() const
{
    QList<QHostAddress> addresses;
    // The override is read as remote_bind is (R-R3-26): "::" becomes Qt's
    // dual-stack any-address, since Qt binds a parsed "::" IPv6-only and the
    // station network's IPv4 amplifiers and tuners would be refused.
    const QHostAddress station = bindOverride.isEmpty()
                                     ? stationAddress()
                                     : DaemonConfig::listenAddressFor(bindOverride);
    if (!station.isNull()) {
        addresses.append(station);
    }
    // This computer too, unless every address already covers it.
    const QHostAddress loopback(QHostAddress::LocalHost);
    if (!everyAddress() && !addresses.contains(loopback)) {
        addresses.append(loopback);
    }
    return addresses;
}

bool StationBind::acceptsPeer(const QHostAddress& peer) const
{
    const QHostAddress from = plainIpv4(peer);
    if (from.isNull()) {
        return false;
    }
    if (from.isLoopback() || everyAddress()) {
        return true;
    }
    const QHostAddress station = plainIpv4(stationAddress());
    if (station.isNull() || station.isLoopback()) {
        return false;
    }
    // The station network is the subnet of this computer's address entry
    // for the station address. An override naming no address of this
    // computer has no subnet to compare, so only that address is accepted.
    for (const QNetworkAddressEntry& entry : entries()) {
        if (plainIpv4(entry.ip()) != station) {
            continue;
        }
        const int prefix = entry.prefixLength();
        if (prefix > 0 && from.protocol() == station.protocol()) {
            return from.isInSubnet(station, prefix);
        }
    }
    return from == station;
}

QString offNetworkReason(const QString& deviceName, const QString& address)
{
    return QStringLiteral("The %1 at %2 is on a different network from the radio, and the "
                          "Core accepts amplifiers and tuners only on the radio's network. To allow "
                          "it, set station_bind in the Core's configuration file to the Core's "
                          "address on that network (or 0.0.0.0 for every network), then "
                          "restart the Core.").arg(deviceName, address);
}

} // namespace NereusSDR::StationNetwork
