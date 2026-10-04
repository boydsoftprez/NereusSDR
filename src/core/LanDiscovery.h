// =================================================================
// src/core/LanDiscovery.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native UDP listener for PowerGeniusXL / TeragenXL
// announcements on ports 9008 and 9010. Parses device model,
// IP address, version, serial, and nickname using the official
// FlexRadio regex. Legacy users deduplicate by serial number; station-owned
// identity admission can opt into endpoint-sensitive deduplication.
//
// Design reference: docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md (section 6.3)
// Regex pattern and wire format: FlexRadio LAN discovery protocol
//
// AI tooling: Anthropic Claude Code; modified by J.J. Boyd (KG4VCF),
// September 2026, AI-assisted via OpenAI Codex. Station network filter
// (setStationBind, R-R3-22 / R-R3-47) by J.J. Boyd (KG4VCF), 2026-09-24,
// AI-assisted via Anthropic Claude Code. The announcements it ignored
// (ignoredOffNetwork, R-R3-47) by J.J. Boyd (KG4VCF), 2026-09-24,
// AI-assisted via Anthropic Claude Code.

#pragma once

#include "core/NereusCoreExport.h"
#include "core/StationNetwork.h"

#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QSet>
#include <QString>

#include <optional>

namespace NereusSDR {

class NEREUS_CORE_EXPORT LanDiscovery : public QObject {
    Q_OBJECT
public:
    explicit LanDiscovery(QObject* parent = nullptr);

    // Preserve the established serial-only behavior by default. Station-side
    // identity admission needs each endpoint candidate because filtering is
    // performed by the owner after deviceDiscovered is emitted.
    void setIdentitySensitiveDeduplication(bool enabled) {
        m_identitySensitiveDeduplication = enabled;
    }
    // R-R3-22 / R-R3-47: on the Core, hear announcements from the station
    // network only (StationNetwork::StationBind). The sockets still open on
    // every address, because the amplifier and tuner announce by broadcast
    // and a socket bound to one address hears no broadcast; an announcement
    // whose sender, or whose announced address, is not on the station
    // network (or this computer) is ignored. A desktop window never calls
    // this and hears every announcement, as before.
    void setStationBind(const StationNetwork::StationBind& bind) { m_stationBind = bind; }
    // M7 (R-R3-47): the announcements ignored as off the station network,
    // so the owner can say why when one was its own device.
    struct IgnoredAnnouncement {
        QString product;
        QString address;
        QString serial;
    };
    QList<IgnoredAnnouncement> ignoredOffNetwork() const { return m_ignoredOffNetwork; }

    void start(int timeoutMs = 3000);
    void stop();

    // Test hook: feed a raw datagram payload as if it arrived on UDP from
    // `sender` (null: unknown, which a station filter refuses).
    void injectDatagramForTesting(const QString& payload, quint16 receivedPort = 9008,
                                  const QHostAddress& sender = QHostAddress());

signals:
    void deviceDiscovered(const QString& model,
                          const QString& ip,
                          quint16        port,
                          const QString& version,
                          const QString& serial,
                          const QString& nickname);
    void scanFinished();

private slots:
    void on9008Ready();
    void on9010Ready();
    void onTimeout();

private:
    void readSocket(QUdpSocket& socket, quint16 port);
    void parseAnnouncement(const QString& payload, quint16 port, const QHostAddress& sender);

    QUdpSocket m_sock9008;
    QUdpSocket m_sock9010;
    QTimer     m_timeout;
    QSet<QString> m_seenIdentities;
    bool m_identitySensitiveDeduplication{false};
    std::optional<StationNetwork::StationBind> m_stationBind;
    QList<IgnoredAnnouncement> m_ignoredOffNetwork;
};

}  // namespace NereusSDR
