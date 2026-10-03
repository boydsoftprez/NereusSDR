// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/IceConfiguration.cpp  (NereusSDR)
// =================================================================
//
// See IceConfiguration.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Task 27 follow-up (new Minor 1): both relay hosts for a
//               one-family end that cannot tell. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/IceConfiguration.h"

#include <QHostAddress>
#include <QCoreApplication>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QObject>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <memory>

namespace NereusSDR {

namespace {

// "host[:port]" or "[v6][:port]", the part of a STUN or TURN URL after its
// scheme (RFC 7064 section 3.1, RFC 7065 section 3.1).
std::optional<IceServerAddress> parseHostPort(const QString& text)
{
    if (text.isEmpty()) {
        return std::nullopt;
    }
    QString host;
    QString portText;
    if (text.startsWith(QLatin1Char('['))) {
        const qsizetype close = text.indexOf(QLatin1Char(']'));
        if (close < 0) {
            return std::nullopt;
        }
        host = text.mid(1, close - 1);
        const QString rest = text.mid(close + 1);
        if (!rest.isEmpty()) {
            if (!rest.startsWith(QLatin1Char(':'))) {
                return std::nullopt;
            }
            portText = rest.mid(1);
        }
        if (QHostAddress(host).protocol() != QAbstractSocket::IPv6Protocol) {
            return std::nullopt;
        }
    } else {
        const qsizetype colon = text.indexOf(QLatin1Char(':'));
        if (text.count(QLatin1Char(':')) > 1) {
            // A bare IPv6 literal is not allowed: RFC 7064 writes it in
            // brackets.
            return std::nullopt;
        }
        host = colon < 0 ? text : text.left(colon);
        if (colon >= 0) {
            portText = text.mid(colon + 1);
        }
        QUrl probe;
        probe.setHost(host, QUrl::StrictMode);
        if (!probe.isValid() || probe.host().isEmpty()) {
            return std::nullopt;
        }
    }
    if (host.isEmpty()) {
        return std::nullopt;
    }
    IceServerAddress address;
    address.host = host;
    address.port = IceConfiguration::kDefaultPort;
    if (!portText.isEmpty()) {
        bool ok = false;
        const int port = portText.toInt(&ok);
        if (!ok || port < 1 || port > 65535) {
            return std::nullopt;
        }
        address.port = static_cast<quint16>(port);
    }
    return address;
}

} // namespace

IceConfiguration IceConfiguration::throughRendezvous(const QStringList& stunUrls,
                                                     bool relayAllowed,
                                                     const AddressFamilies& local,
                                                     const HostFamilies& hosts)
{
    IceConfiguration configuration;
    configuration.m_relayAllowed = relayAllowed;
    configuration.m_local = local;
    configuration.addHostFamilies(hosts);
    // The STUN URLs this build can use, in the order the service lists them
    // (section 6.1: a client uses what it needs of the list); one of them,
    // chosen by this end's address families (fix wave I2).
    QList<IceServerAddress> usable;
    QStringList names;
    for (const QString& url : stunUrls) {
        if (const auto stun = parseStunUrl(url)) {
            usable.append(*stun);
            names.append(stun->host);
        }
    }
    const int chosen = configuration.chooseByFamily(names);
    if (chosen >= 0) {
        configuration.m_stun = usable.at(chosen);
    }
    return configuration;
}

void IceConfiguration::addHostFamilies(const HostFamilies& hosts)
{
    for (auto it = hosts.cbegin(); it != hosts.cend(); ++it) {
        m_hosts.insert(it.key().toLower(), it.value());
    }
}

int IceConfiguration::setRelay(const std::optional<RendezvousWire::Turn>& turn, int families)
{
    const int wanted = std::clamp(families, 1, kMaxRelayServers);
    m_relayKnown = true;
    m_relays.clear();
    if (!m_relayAllowed || !turn) {
        return 0;
    }
    // The first usable URL of each host, in the service's order: on the
    // NereusSDR server an IPv4-only and an IPv6-only relay name.
    QList<IceRelayServer> hosts;
    QStringList names;
    for (const QString& url : turn->urls) {
        const auto address = parseTurnUrl(url);
        if (!address) {
            continue;
        }
        bool hostSeen = false;
        for (const IceRelayServer& relay : std::as_const(hosts)) {
            if (relay.host.compare(address->host, Qt::CaseInsensitive) == 0) {
                hostSeen = true;
                break;
            }
        }
        if (hostSeen) {
            continue;
        }
        hosts.append(IceRelayServer{address->host, address->port, turn->username,
                                    turn->password});
        names.append(address->host);
    }
    bool anyHostKnown = false;
    for (const QString& name : std::as_const(names)) {
        anyHostKnown = anyHostKnown || familiesOfHost(name).known();
    }
    if (wanted == 1 && m_local.known() && !m_local.both() && !anyHostKnown) {
        // This end has one family and no relay name's family is known (the
        // follow-up to the Task 27 re-review, new Minor 1): both hosts, as
        // the service's first could be the other family's. libjuice fails
        // the unreachable family's allocation (no route) without taking a
        // relay slot, so this end still allocates once.
        m_relays = hosts.mid(0, kMaxRelayServers);
    } else if (wanted == 1) {
        // One allocation: the host this end can reach (fix wave I2).
        const int chosen = chooseByFamily(names);
        if (chosen >= 0) {
            m_relays.append(hosts.at(chosen));
        }
    } else {
        m_relays = hosts.mid(0, wanted);
    }
    return static_cast<int>(m_relays.size());
}

int IceConfiguration::chooseByFamily(const QStringList& hosts) const
{
    if (hosts.isEmpty()) {
        return -1;
    }
    // Both families, or none seen: any entry is as reachable as another, so
    // the service's first.
    if (!m_local.known() || m_local.both()) {
        return 0;
    }
    for (qsizetype index = 0; index < hosts.size(); ++index) {
        if (familiesOfHost(hosts.at(index)).shares(m_local)) {
            return static_cast<int>(index);
        }
    }
    // Nothing resolved to a family this end has: it cannot tell.
    return 0;
}

AddressFamilies IceConfiguration::familiesOfHost(const QString& host) const
{
    const AddressFamilies literal = literalFamilies(host);
    return literal.known() ? literal : m_hosts.value(host.toLower());
}

bool IceConfiguration::isUsableLocalAddress(const QHostAddress& address)
{
    if (address.isNull() || address.isLoopback() || address.isLinkLocal()
        || address.isMulticast()) {
        return false;
    }
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        return address.toIPv4Address() != 0;
    }
    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        // Global unicast, 2000::/3 (RFC 4291 section 2.4): not a unique
        // local (fc00::/7) or other address that reaches no server.
        return (address.toIPv6Address()[0] & 0xE0) == 0x20;
    }
    return false;
}

AddressFamilies IceConfiguration::localAddressFamilies()
{
    AddressFamilies families;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& interface : interfaces) {
        if (!(interface.flags() & QNetworkInterface::IsUp)
            || (interface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const QList<QNetworkAddressEntry> entries = interface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            const QHostAddress address = entry.ip();
            if (!isUsableLocalAddress(address)) {
                continue;
            }
            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                families.ipv4 = true;
            } else {
                families.ipv6 = true;
            }
        }
    }
    return families;
}

AddressFamilies IceConfiguration::literalFamilies(const QString& host)
{
    return familiesOf({QHostAddress(host)});
}

AddressFamilies IceConfiguration::familiesOf(const QList<QHostAddress>& addresses)
{
    AddressFamilies families;
    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            families.ipv4 = true;
        } else if (address.protocol() == QAbstractSocket::IPv6Protocol) {
            // An IPv4-mapped IPv6 address is reached over IPv4.
            bool mapped = false;
            address.toIPv4Address(&mapped);
            if (mapped) {
                families.ipv4 = true;
            } else {
                families.ipv6 = true;
            }
        }
    }
    return families;
}

QStringList IceConfiguration::hostNames(const QStringList& urls)
{
    QStringList names;
    for (const QString& url : urls) {
        std::optional<IceServerAddress> address = parseStunUrl(url);
        if (!address) {
            address = parseTurnUrl(url);
        }
        if (!address || literalFamilies(address->host).known()) {
            continue;
        }
        const QString name = address->host.toLower();
        if (!names.contains(name)) {
            names.append(name);
        }
    }
    return names;
}

void IceConfiguration::resolveHostFamilies(const QStringList& names, QObject* context,
                                           std::function<void(const HostFamilies&)> done,
                                           int timeoutMs)
{
    struct Lookup {
        HostFamilies result;
        int pending = 0;
        bool finished = false;
        std::function<void(const HostFamilies&)> done;
        QPointer<QTimer> timer;
        QList<int> ids;
    };
    auto lookup = std::make_shared<Lookup>();
    lookup->done = std::move(done);
    const auto finish = [lookup] {
        if (lookup->finished) {
            return;
        }
        lookup->finished = true;
        for (const int id : std::as_const(lookup->ids)) {
            QHostInfo::abortHostLookup(id);
        }
        if (lookup->timer) {
            lookup->timer->stop();
            lookup->timer->deleteLater();
        }
        if (lookup->done) {
            lookup->done(lookup->result);
        }
    };

    QStringList wanted;
    for (const QString& raw : names) {
        const QString name = raw.toLower();
        if (name.isEmpty() || wanted.contains(name) || literalFamilies(name).known()) {
            continue;
        }
        // A test run never asks a resolver about a name off this computer.
        if (QStandardPaths::isTestModeEnabled() && name != QLatin1String("localhost")
            && !name.endsWith(QLatin1String(".localhost"))) {
            continue;
        }
        wanted.append(name);
    }
    if (wanted.isEmpty() || context == nullptr) {
        QMetaObject::invokeMethod(context != nullptr ? context : QCoreApplication::instance(),
                                  finish, Qt::QueuedConnection);
        return;
    }
    lookup->pending = static_cast<int>(wanted.size());
    auto* timer = new QTimer(context);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, context, finish);
    lookup->timer = timer;
    timer->start(std::max(1, timeoutMs));
    for (const QString& name : std::as_const(wanted)) {
        lookup->ids.append(QHostInfo::lookupHost(
            name, context, [lookup, name, finish](const QHostInfo& info) {
                if (lookup->finished) {
                    return;
                }
                const AddressFamilies families = familiesOf(info.addresses());
                if (info.error() == QHostInfo::NoError && families.known()) {
                    lookup->result.insert(name, families);
                }
                if (--lookup->pending == 0) {
                    finish();
                }
            }));
    }
}

namespace {
bool g_onlyLoopbackShim = false;
} // namespace

void IceConfiguration::setOnlyLoopbackShimCandidatesForTest(bool only)
{
    g_onlyLoopbackShim = only;
}

bool IceConfiguration::acceptsRemoteCandidate(const QString& candidate) const
{
    if (!RendezvousWire::isCandidate(candidate) || candidate.isEmpty()) {
        return false;
    }
    if (g_onlyLoopbackShim && !candidate.contains(QLatin1String(" 127.0.0.1 "))) {
        return false;
    }
    return m_relayAllowed || candidateType(candidate) != QLatin1String("relay");
}

std::optional<IceServerAddress> IceConfiguration::parseStunUrl(const QString& url)
{
    if (!url.startsWith(QLatin1String("stun:"))) {
        return std::nullopt;
    }
    QString rest = url.mid(5);
    if (rest.contains(QLatin1Char('?'))) {
        return std::nullopt;
    }
    return parseHostPort(rest);
}

std::optional<IceServerAddress> IceConfiguration::parseTurnUrl(const QString& url)
{
    if (!url.startsWith(QLatin1String("turn:"))) {
        // turns: is TURN over TLS, which the pinned libjuice cannot speak.
        return std::nullopt;
    }
    QString rest = url.mid(5);
    const qsizetype query = rest.indexOf(QLatin1Char('?'));
    if (query >= 0) {
        const QString parameters = rest.mid(query + 1);
        rest = rest.left(query);
        if (parameters.compare(QLatin1String("transport=udp"), Qt::CaseInsensitive) != 0) {
            return std::nullopt;
        }
    }
    return parseHostPort(rest);
}

QString IceConfiguration::candidateType(const QString& candidate)
{
    // RFC 8839 section 5.1: "... typ <type> ...".
    const QStringList fields = candidate.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (qsizetype index = 0; index + 1 < fields.size(); ++index) {
        if (fields.at(index) == QLatin1String("typ")) {
            return fields.at(index + 1);
        }
    }
    return {};
}

} // namespace NereusSDR
