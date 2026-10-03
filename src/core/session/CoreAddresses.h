// =================================================================
// src/core/session/CoreAddresses.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The Core's own dialable control
// addresses for its signed-in paired devices (the `devices` object's
// `coreAddresses`, coreAddressesVersion 1; the station link document,
// sections 7.1 and 21.1).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Created for the phone's direct addresses. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The kernel's own temporary and deprecated flags on Linux
//                 (/proc/net/if_inet6), beside Qt's. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QNetworkAddressEntry>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include <functional>

namespace NereusSDR {

namespace CoreAddresses {

/// At most this many addresses are sent; a Core with more keeps the first
/// ones in the order dialable() gives.
inline constexpr int kMaxAddresses = 8;
/// How often the Core reads its interfaces again while it listens, so a
/// renumbered address (DHCP, SLAAC) reaches the devices. The LAN
/// announcement reads them as often (kStationLanAnnouncementIntervalMs).
inline constexpr int kRefreshIntervalMs = 5000;

/// A stable global IPv6 address: global unicast (2000::/3, the rule
/// IceConfiguration::isUsableLocalAddress applies), not a temporary privacy
/// address (the system marks those ineligible for DNS), and not deprecated
/// (its preferred lifetime has not run out). Never link-local or a unique
/// local address.
bool isStableGlobalIpv6(const QNetworkAddressEntry& entry);

/// An IPv4 address the public internet routes to this computer: none of
/// RFC 6890's special-purpose blocks (private, shared 100.64/10, loopback,
/// link-local, documentation, benchmarking, multicast, reserved).
bool isPublicIpv4(const QHostAddress& address);

/// The addresses a device can dial the Core's control listener at:
/// "[v6]:port" for each stable global IPv6 address, then "v4:port" for each
/// public IPv4 address, each family in address order, without repeats,
/// only addresses the listener serves (stationLanListenerServesAddress),
/// at most kMaxAddresses. Empty for port 0.
QStringList dialable(const QList<QNetworkAddressEntry>& entries,
                     const QHostAddress& listener, quint16 port);

/// The wire value: compact JSON {"addresses":["[2001:db8::5]:47910",...]}.
QString toJson(const QStringList& addresses);

/// What the kernel says of one IPv6 address (Linux, /proc/net/if_inet6).
struct KernelIpv6Flags {
    bool temporary = false;  ///< a privacy address (IFA_F_TEMPORARY)
    bool deprecated = false; ///< its preferred lifetime is over (IFA_F_DEPRECATED)
};

/// Reads /proc/net/if_inet6's text ("<32 hex address> <ifindex> <prefix>
/// <scope> <flags> <name>" per line, all hex) into the flags of each
/// address, keyed by QHostAddress::toString(). A line it cannot read is
/// skipped.
QHash<QString, KernelIpv6Flags> parseIfInet6(const QByteArray& text);

/// Marks each entry the kernel flags: a temporary one not eligible for DNS
/// and a deprecated one with its preferred lifetime over, so
/// isStableGlobalIpv6() drops them whatever Qt itself reported. Qt's own
/// isTemporary() is not the privacy flag (it means only "not permanent",
/// true of a stable SLAAC address too), so it is never used here.
QList<QNetworkAddressEntry> withKernelFlags(QList<QNetworkAddressEntry> entries,
                                            const QHash<QString, KernelIpv6Flags>& flags);

/// The Core's interfaces as the watcher reads them:
/// StationNetwork::localEntries(), and on Linux withKernelFlags() from
/// /proc/net/if_inet6.
QList<QNetworkAddressEntry> localEntries();

} // namespace CoreAddresses

/// Reads the Core's interfaces while its control listener is up and says
/// when the dialable addresses change.
class CoreAddressWatcher final : public QObject {
    Q_OBJECT

public:
    using EntrySource = std::function<QList<QNetworkAddressEntry>()>;

    explicit CoreAddressWatcher(QObject* parent = nullptr);

    /// Where the interfaces are read from; CoreAddresses::localEntries()
    /// (up, running, not loopback, with the kernel's flags) unless a test
    /// sets its own.
    void setEntrySource(EntrySource source);

    /// Follows a listener on `listener`:`port`: reads the interfaces now
    /// and every kRefreshIntervalMs.
    void start(const QHostAddress& listener, quint16 port);
    /// No listener: no addresses.
    void stop();
    /// Reads the interfaces again now.
    void refresh();

    QStringList addresses() const { return m_addresses; }
    bool isActive() const { return m_active; }
    /// Whether the interfaces are being read again every
    /// kRefreshIntervalMs (true while a listener is followed).
    bool isRefreshing() const { return m_timer.isActive(); }
    int refreshIntervalMs() const { return m_timer.interval(); }

signals:
    void addressesChanged(const QStringList& addresses);

private:
    void publish(const QStringList& addresses);

    EntrySource m_source;
    QTimer m_timer;
    QHostAddress m_listener;
    quint16 m_port = 0;
    bool m_active = false;
    QStringList m_addresses;
};

} // namespace NereusSDR
