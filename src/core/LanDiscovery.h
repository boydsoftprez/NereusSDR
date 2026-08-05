// =================================================================
// src/core/LanDiscovery.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native UDP listener for PowerGeniusXL / TeragenXL
// announcements on ports 9008 and 9010. Parses device model,
// IP address, version, serial, and nickname using the official
// FlexRadio regex. Deduplicates by serial number before emitting
// the deviceDiscovered signal.
//
// Design reference: docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md (section 6.3)
// Regex pattern and wire format: FlexRadio LAN discovery protocol
//
// AI tooling: Anthropic Claude Code.

#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QSet>
#include <QString>

namespace NereusSDR {

class LanDiscovery : public QObject {
    Q_OBJECT
public:
    explicit LanDiscovery(QObject* parent = nullptr);

    void start(int timeoutMs = 3000);
    void stop();

    // Test hook: feed a raw datagram payload as if it arrived on UDP.
    void injectDatagramForTesting(const QString& payload);

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
    void parseAnnouncement(const QString& payload, quint16 port);

    QUdpSocket m_sock9008;
    QUdpSocket m_sock9010;
    QTimer     m_timeout;
    QSet<QString> m_seenSerials;
};

}  // namespace NereusSDR
