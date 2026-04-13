#pragma once

#include "HpsdrModel.h"

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>
#include <QMetaType>

namespace NereusSDR {

// Protocol version supported by the radio.
enum class ProtocolVersion : int {
    Protocol1 = 1,
    Protocol2 = 2
};

// Information about a discovered OpenHPSDR radio.
struct RadioInfo {
    // Identity
    QString name;                        // User-friendly name, e.g. "ANAN-G2"
    QString macAddress;                  // MAC as "AA:BB:CC:DD:EE:FF" (primary key)
    QHostAddress address;                // IP address on local network
    quint16 port{1024};                  // Always 1024 for OpenHPSDR

    // Hardware
    HPSDRHW boardType{HPSDRHW::Unknown};
    int firmwareVersion{0};
    int adcCount{1};                     // Derived from boardType (1 or 2)
    int maxReceivers{4};                 // Board-dependent max simultaneous RX

    // Protocol
    ProtocolVersion protocol{ProtocolVersion::Protocol1};
    bool inUse{false};                   // Another client already connected

    // Capabilities (derived from boardType at parse time)
    bool hasDiversityReceiver{false};    // 2-ADC boards support diversity
    bool hasPureSignal{false};           // Boards supporting PA linearization
    int maxSampleRate{384000};           // Max supported sample rate in Hz

    // Display helpers
    QString displayName() const;
    static int adcCountForBoard(HPSDRHW type);
    static int maxReceiversForBoard(HPSDRHW type);

    // Comparison — radios are identified by MAC address
    bool operator==(const RadioInfo& other) const {
        return macAddress == other.macAddress;
    }
};

// Discovers OpenHPSDR radios on the local network.
// Both P1 and P2 radios respond to UDP broadcasts on port 1024.
class RadioDiscovery : public QObject {
    Q_OBJECT

public:
    explicit RadioDiscovery(QObject* parent = nullptr);
    ~RadioDiscovery() override;

    void startDiscovery();
    void stopDiscovery();

    QList<RadioInfo> discoveredRadios() const;

signals:
    void radioDiscovered(const NereusSDR::RadioInfo& info);
    void radioUpdated(const NereusSDR::RadioInfo& info);
    void radioLost(const QString& macAddress);

private slots:
    void onReadyRead();
    void onStaleCheck();
    void sendDiscoveryPacket();

private:
    static constexpr quint16 kDiscoveryPort = 1024;
    static constexpr int kDiscoveryIntervalMs = 2000;
    static constexpr int kStaleTimeoutMs = 10000;

    // MAC extraction from raw bytes
    static QString macToString(const char* bytes);

    QUdpSocket* m_socket{nullptr};
    QTimer m_discoveryTimer;
    QTimer m_staleTimer;
    QMap<QString, RadioInfo> m_radios;   // keyed by MAC address
    QMap<QString, qint64> m_lastSeen;    // MAC -> timestamp
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::RadioInfo)
Q_DECLARE_METATYPE(NereusSDR::ProtocolVersion)
