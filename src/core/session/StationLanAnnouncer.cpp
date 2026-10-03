// NereusSDR-original one-way Core LAN discovery multicast sender.
#include "StationLanAnnouncer.h"

#include <QNetworkInterface>
#include <QUdpSocket>

namespace NereusSDR {
namespace {

bool eligibleInterface(const QNetworkInterface& interface)
{
    const QNetworkInterface::InterfaceFlags flags = interface.flags();
    return flags.testFlag(QNetworkInterface::IsUp)
        && flags.testFlag(QNetworkInterface::IsRunning)
        && flags.testFlag(QNetworkInterface::CanMulticast)
        && !flags.testFlag(QNetworkInterface::IsLoopBack);
}

} // namespace

bool stationLanListenerServesAddress(const QHostAddress& listener,
                                     const QHostAddress& source)
{
    if (listener.isNull() || listener.isLoopback() || source.isNull() || source.isLoopback()
        || listener.isMulticast() || listener == QHostAddress::Broadcast
        || source == QHostAddress::Any || source == QHostAddress::AnyIPv4
        || source == QHostAddress::AnyIPv6 || source.isMulticast()
        || source == QHostAddress::Broadcast) {
        return false;
    }
    if (listener == QHostAddress::AnyIPv4) {
        return source.protocol() == QAbstractSocket::IPv4Protocol;
    }
    if (listener == QHostAddress::AnyIPv6) {
        return source.protocol() == QAbstractSocket::IPv6Protocol;
    }
    if (listener == QHostAddress::Any) {
        return source.protocol() == QAbstractSocket::IPv4Protocol
            || source.protocol() == QAbstractSocket::IPv6Protocol;
    }
    return listener == source;
}

StationLanAnnouncer::StationLanAnnouncer(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(kStationLanAnnouncementIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &StationLanAnnouncer::onTimer);
}

void StationLanAnnouncer::update(const QHostAddress& listenerAddress,
                                 const StationLanAnnouncement& announcement)
{
    QString error;
    if (listenerAddress.isNull() || listenerAddress.isLoopback()
        || listenerAddress.isMulticast() || listenerAddress == QHostAddress::Broadcast
        // iPhone app Task 16: a station sends schema 2 only. Sending schema 1
        // as well would make a desktop from before Task 16 flap between them.
        || announcement.schema != kStationLanAnnouncementSchema
        || encodeStationLanAnnouncement(announcement, &error).isEmpty()) {
        stop();
        return;
    }
    if (m_active && m_listenerAddress == listenerAddress && m_announcement == announcement) {
        return;
    }
    m_listenerAddress = listenerAddress;
    m_announcement = announcement;
    m_active = true;
    announceNow();
    m_timer.start();
}

void StationLanAnnouncer::stop()
{
    m_timer.stop();
    m_active = false;
    m_listenerAddress = {};
    m_announcement = {};
}

void StationLanAnnouncer::announceNow()
{
    if (!m_active) {
        return;
    }
    QString error;
    const QByteArray datagram = encodeStationLanAnnouncement(m_announcement, &error);
    if (datagram.isEmpty()) {
        stop();
        return;
    }
    const QHostAddress ipv4Group(QString::fromLatin1(kStationLanIpv4MulticastGroup));
    const QHostAddress ipv6Group(QString::fromLatin1(kStationLanIpv6MulticastGroup));
    for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
        if (!eligibleInterface(interface)) {
            continue;
        }
        bool usedIpv4Source = false;
        bool usedIpv6Source = false;
        for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
            QHostAddress source = entry.ip();
            if (source.protocol() == QAbstractSocket::IPv6Protocol && source.isLinkLocal()
                && source.scopeId().isEmpty()) {
                source.setScopeId(interface.name());
            }
            if (!stationLanListenerServesAddress(m_listenerAddress, source)) {
                continue;
            }
            const bool isIpv4 = source.protocol() == QAbstractSocket::IPv4Protocol;
            const bool isIpv6 = source.protocol() == QAbstractSocket::IPv6Protocol;
            if ((!isIpv4 && !isIpv6) || (isIpv4 && usedIpv4Source)
                || (isIpv6 && usedIpv6Source)) {
                continue;
            }
            if (isIpv4) {
                usedIpv4Source = true;
            } else {
                usedIpv6Source = true;
            }
            QUdpSocket socket;
            if (!socket.bind(source, 0)) {
                continue;
            }
            socket.setSocketOption(QAbstractSocket::MulticastTtlOption,
                                   kStationLanMulticastHopLimit);
            socket.setSocketOption(QAbstractSocket::MulticastLoopbackOption, 0);
            socket.setMulticastInterface(interface);
            if (isIpv4) {
                socket.writeDatagram(datagram, ipv4Group, kStationLanDiscoveryPort);
            } else {
                socket.writeDatagram(datagram, ipv6Group, kStationLanDiscoveryPort);
            }
        }
    }
}

void StationLanAnnouncer::onTimer()
{
    announceNow();
}

} // namespace NereusSDR
