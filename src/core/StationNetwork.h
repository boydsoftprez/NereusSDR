// no-port-check: NereusSDR-original. R-R3-48 station network address choice;
// R-R3-22 / R-R3-47 one bind rule for every station listener (StationBind).
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47: offNetworkReason, how to allow a device on another
// network. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-26: "::" listens on IPv4 and IPv6. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
#pragma once

#include <QHostAddress>
#include <QList>
#include <QNetworkAddressEntry>
#include <QString>

#include <optional>

namespace NereusSDR::StationNetwork {

/// This computer's address on the network that holds `peer` (the radio, or
/// an amplifier): the first IPv4 address entry whose subnet contains it.
/// Null when no entry does, or `peer` is not an IPv4 address.
QHostAddress addressFacing(const QHostAddress& peer,
                           const QList<QNetworkAddressEntry>& entries);

/// The address entries of this computer's running interfaces (loopback
/// left out), as QNetworkInterface reports them.
QList<QNetworkAddressEntry> localEntries();

/// `address` as IPv4 when it is an IPv4-mapped IPv6 address, else itself.
QHostAddress plainIpv4(const QHostAddress& address);

/// The one rule for where the Core's station listeners accept connections:
/// the SmartSDR API listener on TCP 4992, the Power Genius and Tuner Genius
/// discovery sockets on UDP 9008 and 9010, and the station TCI server.
///
/// The station network is nereusd.conf's `station_bind` when set (the older
/// name `station_tci_bind` is read too), else the network that holds the
/// radio: this computer's address on the radio's subnet, once the radio is
/// known. Every station listener also accepts this computer (127.0.0.1).
/// Before a radio connects (and with no override) only this computer is
/// accepted. A radio address change moves every listener to the new
/// network. An override of every address (`0.0.0.0` or `::`) is used as
/// given and accepts every network.
///
/// A desktop window without a Core sets none of this: its listeners bind
/// as they always have.
struct StationBind {
    /// nereusd.conf `station_bind`: an IP address, or empty to follow the radio.
    QString bindOverride;
    /// The radio's address; null before a radio connects.
    QHostAddress radio;
    /// Test seam: this computer's address entries (default: the live ones).
    std::optional<QList<QNetworkAddressEntry>> entriesForTest;

    /// This computer's address entries, the test's when given.
    QList<QNetworkAddressEntry> entries() const;
    /// The station network address: the override, else this computer's
    /// address on the radio's subnet. Null while neither is known.
    QHostAddress stationAddress() const;
    /// The override asks for every address.
    bool everyAddress() const;
    /// Where a TCP listener listens: the station address and 127.0.0.1;
    /// the every-address override alone ("::" as Qt's dual-stack
    /// any-address, as for remote_bind); 127.0.0.1 alone before the
    /// station network is known.
    QList<QHostAddress> listenAddresses() const;
    /// Whether a datagram from `peer` comes from this computer or the
    /// station network (for sockets that must open on every address to
    /// hear broadcasts, and filter instead).
    bool acceptsPeer(const QHostAddress& peer) const;
};

/// M7 (R-R3-47): why a station device (`deviceName`, "Power Genius" or
/// "Tuner Genius") heard at `address` is not admitted, in plain words, and
/// the Core setting that allows it.
QString offNetworkReason(const QString& deviceName, const QString& address);

} // namespace NereusSDR::StationNetwork
