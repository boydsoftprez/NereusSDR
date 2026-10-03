// NereusSDR-original bounded UDP receiver for untrusted LAN Core discovery.
#include "StationLanDiscovery.h"

#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <QStringList>

namespace NereusSDR {
namespace {

constexpr int kMaximumDatagramsPerDrain = 32;

struct MembershipResult {
    bool hasEligibleInterface = false;
    bool joinFailed = false;
    bool membershipChanged = false;
};

struct EligibleTopology {
    QList<QNetworkInterface> interfaces;
    QSet<QString> identities;
};

bool eligibleInterface(const QNetworkInterface& interface)
{
    const QNetworkInterface::InterfaceFlags flags = interface.flags();
    return flags.testFlag(QNetworkInterface::IsUp)
        && flags.testFlag(QNetworkInterface::IsRunning)
        && flags.testFlag(QNetworkInterface::CanMulticast)
        && !flags.testFlag(QNetworkInterface::IsLoopBack);
}

EligibleTopology eligibleTopology(const QAbstractSocket::NetworkLayerProtocol family)
{
    EligibleTopology topology;
    for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
        if (!eligibleInterface(interface) || interface.index() <= 0) {
            continue;
        }
        bool hasFamilyAddress = false;
        for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.protocol() != family) {
                continue;
            }
            hasFamilyAddress = true;
            topology.identities.insert(QString::number(interface.index()) + QChar(0x1f)
                                       + address.toString() + QChar(0x1f) + address.scopeId());
        }
        if (!hasFamilyAddress) {
            continue;
        }
        topology.interfaces.append(interface);
    }
    return topology;
}

MembershipResult reconcileMembership(QUdpSocket* socket, const QHostAddress& group,
                                     const EligibleTopology& topology,
                                     QSet<uint>* joinedInterfaces)
{
    MembershipResult result;
    result.hasEligibleInterface = !topology.interfaces.isEmpty();
    if (socket->state() != QAbstractSocket::BoundState) {
        if (!joinedInterfaces->isEmpty()) {
            joinedInterfaces->clear();
            result.membershipChanged = true;
        }
        return result;
    }

    for (const QNetworkInterface& interface : topology.interfaces) {
        const uint interfaceIndex = static_cast<uint>(interface.index());
        if (joinedInterfaces->contains(interfaceIndex)) {
            continue;
        }
        if (socket->joinMulticastGroup(group, interface)) {
            joinedInterfaces->insert(interfaceIndex);
            result.membershipChanged = true;
        } else {
            result.joinFailed = true;
        }
    }
    return result;
}

} // namespace

StationLanDiscovery::StationLanDiscovery(QObject* parent)
    : QObject(parent)
    , m_ipv4Socket(new QUdpSocket(this))
    , m_ipv6Socket(new QUdpSocket(this))
{
    connect(m_ipv4Socket, &QUdpSocket::readyRead, this, [this]() {
        scheduleDrain(m_ipv4Socket, &m_ipv4DrainQueued);
    });
    connect(m_ipv6Socket, &QUdpSocket::readyRead, this, [this]() {
        scheduleDrain(m_ipv6Socket, &m_ipv6DrainQueued);
    });
    m_expiryTimer.setInterval(1'000);
    connect(&m_expiryTimer, &QTimer::timeout, this, &StationLanDiscovery::onExpiryTimer);
    m_clock.start();
}

StationLanDiscovery::~StationLanDiscovery()
{
    stop();
}

bool StationLanDiscovery::start(quint16 requestedPort)
{
    if (m_port != 0) {
        return true;
    }

    ++m_generation;
    m_ipv4DrainQueued = false;
    m_ipv6DrainQueued = false;
    m_ipv4JoinedInterfaces.clear();
    m_ipv6JoinedInterfaces.clear();
    m_ipv4EligibleIdentities.clear();
    m_ipv6EligibleIdentities.clear();
    m_ipv4MembershipInitialized = false;
    m_ipv6MembershipInitialized = false;
    m_membershipWarning.clear();
    m_packetError.clear();
    refreshLastError();
    const QUdpSocket::BindMode bindMode = QUdpSocket::ShareAddress
        | QUdpSocket::ReuseAddressHint;
    bool ipv4Bound = m_ipv4Socket->bind(QHostAddress::AnyIPv4, requestedPort, bindMode);
    const quint16 selectedPort = ipv4Bound ? m_ipv4Socket->localPort() : requestedPort;
    const bool ipv6Bound = m_ipv6Socket->bind(QHostAddress::AnyIPv6, selectedPort, bindMode);
    if (!ipv4Bound && !ipv6Bound) {
        m_ipv4Socket->close();
        m_ipv6Socket->close();
        setPacketError(QStringLiteral("Station LAN discovery could not bind a UDP socket."));
        emit changed();
        return false;
    }

    m_port = ipv4Bound ? m_ipv4Socket->localPort() : m_ipv6Socket->localPort();
    m_expiryTimer.start();
    refreshMulticastMembership();
    emit changed();
    return true;
}

void StationLanDiscovery::stop()
{
    const bool wasAvailable = m_port != 0 || !m_cache.endpoints().isEmpty() || !m_lastError.isEmpty();
    ++m_generation;
    m_expiryTimer.stop();
    m_ipv4DrainQueued = false;
    m_ipv6DrainQueued = false;
    m_ipv4Socket->close();
    m_ipv6Socket->close();
    m_ipv4JoinedInterfaces.clear();
    m_ipv6JoinedInterfaces.clear();
    m_ipv4EligibleIdentities.clear();
    m_ipv6EligibleIdentities.clear();
    m_ipv4MembershipInitialized = false;
    m_ipv6MembershipInitialized = false;
    m_port = 0;
    m_membershipWarning.clear();
    m_packetError.clear();
    refreshLastError();
    m_cache.clear();
    if (wasAvailable) {
        emit changed();
    }
}

void StationLanDiscovery::onExpiryTimer()
{
    const bool membershipChanged = refreshMulticastMembership();
    const bool cacheChanged = m_cache.expire(m_clock.elapsed());
    if (membershipChanged || cacheChanged) {
        emit changed();
    }
}

void StationLanDiscovery::scheduleDrain(QUdpSocket* socket, bool* queued)
{
    if (*queued || m_port == 0) {
        return;
    }
    *queued = true;
    const quint64 generation = m_generation;
    const QPointer<QUdpSocket> guardedSocket(socket);
    QTimer::singleShot(0, this, [this, guardedSocket, queued, generation]() {
        if (generation != m_generation || guardedSocket.isNull() || m_port == 0) {
            return;
        }
        drainSocket(guardedSocket.data(), queued, generation);
    });
}

void StationLanDiscovery::drainSocket(QUdpSocket* socket, bool* queued, quint64 generation)
{
    *queued = false;
    bool observationChanged = false;
    for (int count = 0; count < kMaximumDatagramsPerDrain && socket->hasPendingDatagrams(); ++count) {
        const qint64 pendingSize = socket->pendingDatagramSize();
        if (pendingSize < 0) {
            observationChanged = setPacketError(
                QStringLiteral("Station LAN discovery could not read a UDP datagram."))
                || observationChanged;
            break;
        }
        if (pendingSize > kStationLanMaxDatagramBytes) {
            socket->receiveDatagram(0);
            observationChanged = setPacketError(
                QStringLiteral("Station LAN discovery ignored an oversized datagram."))
                || observationChanged;
            continue;
        }
        const QNetworkDatagram datagram = socket->receiveDatagram(pendingSize);
        if (!datagram.isValid()) {
            observationChanged = setPacketError(
                QStringLiteral("Station LAN discovery could not read a UDP datagram."))
                || observationChanged;
            continue;
        }
        QString error;
        if (m_cache.ingest(datagram.data(), datagram.senderAddress(), datagram.interfaceIndex(),
                           m_clock.elapsed(), &error)) {
            observationChanged = true;
        }
        if (!error.isEmpty()) {
            observationChanged = setPacketError(error) || observationChanged;
        } else {
            observationChanged = setPacketError({}) || observationChanged;
        }
    }
    const QPointer<StationLanDiscovery> guardedReceiver(this);
    if (observationChanged) {
        emit changed();
    }
    if (guardedReceiver.isNull() || generation != m_generation || m_port == 0
        || (socket != m_ipv4Socket.data() && socket != m_ipv6Socket.data())) {
        return;
    }
    if (generation == m_generation && m_port != 0 && socket->hasPendingDatagrams()) {
        scheduleDrain(socket, queued);
    }
}

bool StationLanDiscovery::rebindSocket(QUdpSocket* socket, QHostAddress::SpecialAddress address,
                                       QSet<uint>* joinedInterfaces)
{
    ++m_generation;
    m_ipv4DrainQueued = false;
    m_ipv6DrainQueued = false;
    socket->close();
    joinedInterfaces->clear();
    const QUdpSocket::BindMode bindMode = QUdpSocket::ShareAddress
        | QUdpSocket::ReuseAddressHint;
    return socket->bind(address, m_port, bindMode);
}

bool StationLanDiscovery::refreshMulticastMembership()
{
    const QHostAddress ipv4Group(QString::fromLatin1(kStationLanIpv4MulticastGroup));
    const QHostAddress ipv6Group(QString::fromLatin1(kStationLanIpv6MulticastGroup));
    const EligibleTopology ipv4Topology = eligibleTopology(QAbstractSocket::IPv4Protocol);
    const EligibleTopology ipv6Topology = eligibleTopology(QAbstractSocket::IPv6Protocol);
    const bool ipv4TopologyChanged = m_ipv4MembershipInitialized
        && m_ipv4EligibleIdentities != ipv4Topology.identities;
    const bool ipv6TopologyChanged = m_ipv6MembershipInitialized
        && m_ipv6EligibleIdentities != ipv6Topology.identities;
    const bool ipv4NeedsRebind = ipv4TopologyChanged
        || m_ipv4Socket->state() != QAbstractSocket::BoundState;
    const bool ipv6NeedsRebind = ipv6TopologyChanged
        || m_ipv6Socket->state() != QAbstractSocket::BoundState;
    bool ipv4Rebound = false;
    bool ipv6Rebound = false;
    if (ipv4NeedsRebind) {
        ipv4Rebound = rebindSocket(m_ipv4Socket, QHostAddress::AnyIPv4,
                                   &m_ipv4JoinedInterfaces);
    }
    if (ipv6NeedsRebind) {
        ipv6Rebound = rebindSocket(m_ipv6Socket, QHostAddress::AnyIPv6,
                                   &m_ipv6JoinedInterfaces);
    }
    m_ipv4EligibleIdentities = ipv4Topology.identities;
    m_ipv6EligibleIdentities = ipv6Topology.identities;
    m_ipv4MembershipInitialized = true;
    m_ipv6MembershipInitialized = true;
    const MembershipResult ipv4 = reconcileMembership(
        m_ipv4Socket, ipv4Group, ipv4Topology, &m_ipv4JoinedInterfaces);
    const MembershipResult ipv6 = reconcileMembership(
        m_ipv6Socket, ipv6Group, ipv6Topology, &m_ipv6JoinedInterfaces);

    QStringList warnings;
    if (m_ipv4Socket->state() != QAbstractSocket::BoundState) {
        warnings.append(QStringLiteral("Station LAN discovery IPv4 is unavailable."));
    }
    if (m_ipv6Socket->state() != QAbstractSocket::BoundState) {
        warnings.append(QStringLiteral("Station LAN discovery IPv6 is unavailable."));
    }
    if (!ipv4.hasEligibleInterface && !ipv6.hasEligibleInterface) {
        warnings.append(QStringLiteral("Station LAN discovery has no eligible multicast interface."));
    }
    if (ipv4.joinFailed || ipv6.joinFailed) {
        warnings.append(QStringLiteral("Station LAN discovery could not join a multicast group."));
    }
    // Reset invalidates queued drains for both families. The unchanged family
    // may already have unread packets, so requeue it without requiring another
    // readyRead edge. Cache entries keep their independent expiry time.
    if (ipv4NeedsRebind || ipv6NeedsRebind) {
        if (m_ipv4Socket->hasPendingDatagrams()) {
            scheduleDrain(m_ipv4Socket, &m_ipv4DrainQueued);
        }
        if (m_ipv6Socket->hasPendingDatagrams()) {
            scheduleDrain(m_ipv6Socket, &m_ipv6DrainQueued);
        }
    }
    m_membershipWarning = warnings.join(QLatin1Char(' '));
    const bool errorChanged = refreshLastError();
    return ipv4TopologyChanged || ipv6TopologyChanged || ipv4Rebound || ipv6Rebound
        || ipv4.membershipChanged || ipv6.membershipChanged || errorChanged;
}

bool StationLanDiscovery::setPacketError(const QString& error)
{
    m_packetError = error;
    return refreshLastError();
}

bool StationLanDiscovery::refreshLastError()
{
    const QString effectiveError = m_membershipWarning.isEmpty()
        ? m_packetError
        : m_membershipWarning;
    if (m_lastError == effectiveError) {
        return false;
    }
    m_lastError = effectiveError;
    return true;
}

} // namespace NereusSDR
