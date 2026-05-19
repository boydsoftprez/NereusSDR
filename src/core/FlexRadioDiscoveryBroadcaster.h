// =================================================================
// src/core/FlexRadioDiscoveryBroadcaster.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original — no Thetis or AetherSDR upstream.
// Wire format reverse-engineered from FLEX-8600 v4.2.18.41174 discovery
// beacon captured 2026-05-19
// (captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19 — Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QString>
#include <QByteArray>

namespace NereusSDR {

// Broadcasts a SmartSDR-format discovery beacon on UDP 4992 so that
// FlexRadio-aware accessories (PGXL, TGXL) auto-discover NereusSDR
// in their UI. Wire format reverse-engineered from a FLEX-8600 capture
// 2026-05-19 (captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng).
//
// Beacon = 28-byte VITA-49-style header + ASCII key=value payload,
// emitted once per second to 255.255.255.255:4992.
//
// Owned by RadioModel; starts when start() is called and stops on stop().
class FlexRadioDiscoveryBroadcaster : public QObject {
    Q_OBJECT
public:
    explicit FlexRadioDiscoveryBroadcaster(QObject* parent = nullptr);

    // Configurable fields. Setters update the next emitted beacon.
    void setSerial(const QString& serial);           // 16-digit dashed
    void setNickname(const QString& nickname);
    void setCallsign(const QString& callsign);
    void setVersion(const QString& version);         // e.g. "0.5.1"
    void setMacAddress(const QString& macColonForm); // "aa:bb:cc:dd:ee:ff"

    // Lifecycle
    void start();  // begins 1 Hz emission; safe to call repeatedly
    void stop();   // stops emission; safe to call repeatedly

    // Test-only: build the full packet bytes (header + ASCII payload) for a
    // given sequence count and timestamp without actually sending. Lets unit
    // tests verify the binary layout deterministically.
    QByteArray buildBeaconForTesting(quint8 packetCount,
                                     quint32 unixSeconds) const;

private slots:
    void onTick();

private:
    QByteArray buildBeacon(quint8 packetCount, quint32 unixSeconds) const;
    QString    detectLanIpv4() const; // first non-loopback IPv4 host address

    QUdpSocket m_socket;
    QTimer     m_timer;
    quint8     m_packetCount{0}; // rolling 0..15 in word 0 bits 19-16

    // Static-ish fields (set once at start, updated by setters)
    QString m_serial;
    QString m_nickname;
    QString m_callsign;
    QString m_version;
    QString m_mac;  // dashed uppercase form for radio_license_id
    QString m_ip;
};

}  // namespace NereusSDR
