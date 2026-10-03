// NereusSDR-original LAN Core discovery UDP receiver. Discovery is untrusted.
#pragma once

#include "StationLanCache.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>

class QUdpSocket;
class TstStationLanTransport;

namespace NereusSDR {

class StationLanDiscovery : public QObject {
    Q_OBJECT

public:
    explicit StationLanDiscovery(QObject* parent = nullptr);
    ~StationLanDiscovery() override;

    bool start(quint16 port = kStationLanDiscoveryPort);
    void stop();

    QList<StationLanEndpoint> endpoints() const { return m_cache.endpoints(); }
    quint16 port() const { return m_port; }
    QString lastError() const { return m_lastError; }

signals:
    void changed();

private slots:
    void onExpiryTimer();

private:
    friend class ::TstStationLanTransport;
    void scheduleDrain(QUdpSocket* socket, bool* queued);
    void drainSocket(QUdpSocket* socket, bool* queued, quint64 generation);
    bool rebindSocket(QUdpSocket* socket, QHostAddress::SpecialAddress address,
                      QSet<uint>* joinedInterfaces);
    bool refreshMulticastMembership();
    bool setPacketError(const QString& error);
    bool refreshLastError();

    QPointer<QUdpSocket> m_ipv4Socket;
    QPointer<QUdpSocket> m_ipv6Socket;
    QTimer m_expiryTimer;
    QElapsedTimer m_clock;
    StationLanCache m_cache;
    QString m_lastError;
    QString m_membershipWarning;
    QString m_packetError;
    QSet<uint> m_ipv4JoinedInterfaces;
    QSet<uint> m_ipv6JoinedInterfaces;
    QSet<QString> m_ipv4EligibleIdentities;
    QSet<QString> m_ipv6EligibleIdentities;
    quint16 m_port = 0;
    quint64 m_generation = 0;
    bool m_ipv4DrainQueued = false;
    bool m_ipv6DrainQueued = false;
    bool m_ipv4MembershipInitialized = false;
    bool m_ipv6MembershipInitialized = false;
};

} // namespace NereusSDR
