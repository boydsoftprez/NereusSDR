#pragma once

// no-port-check: NereusSDR-original abstract base class. Inline doc comments
// reference Thetis source filenames (console.cs / networkproto1.c / network.c)
// only as pointers to where ported logic lives in the concrete subclasses
// (P1RadioConnection.cpp, P2RadioConnection.cpp); no upstream code is
// reproduced in this header.

#include "RadioDiscovery.h"
#include "HardwareProfile.h"

#include <QObject>
#include <QVector>

#include <atomic>
#include <memory>

namespace NereusSDR {

// Structured error taxonomy — design doc §6.1.
enum class RadioConnectionError {
    None,
    DiscoveryNicFailure,
    DiscoveryAllFailed,
    RadioInUse,
    FirmwareTooOld,
    FirmwareStale,           // non-fatal warning
    SocketBindFailure,
    NoDataTimeout,
    UnknownBoardType,
    ProtocolMismatch
};

// Connection state for a radio.
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

// Abstract base class for radio connections.
// Subclasses implement protocol-specific behavior (P1 or P2).
// Instances live on the Connection worker thread.
// Call init() after moveToThread() to create sockets/timers on the worker thread.
class RadioConnection : public QObject {
    Q_OBJECT

public:
    explicit RadioConnection(QObject* parent = nullptr);
    ~RadioConnection() override;

    // Factory — creates the appropriate subclass based on RadioInfo::protocol.
    static std::unique_ptr<RadioConnection> create(const RadioInfo& info);

    // State (atomic for cross-thread reads from main thread)
    ConnectionState state() const { return m_state.load(); }
    bool isConnected() const { return m_state.load() == ConnectionState::Connected; }
    const RadioInfo& radioInfo() const { return m_radioInfo; }

    void setHardwareProfile(const HardwareProfile& profile) { m_hardwareProfile = profile; }
    const HardwareProfile& hardwareProfile() const { return m_hardwareProfile; }

public slots:
    // Must be called on the worker thread after moveToThread().
    // Creates sockets, timers, and other thread-local resources.
    virtual void init() = 0;

    // Connect to the specified radio. Auto-queued from main thread.
    virtual void connectToRadio(const NereusSDR::RadioInfo& info) = 0;

    // Graceful disconnect.
    virtual void disconnect() = 0;

    // --- Frequency Control ---
    virtual void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) = 0;
    virtual void setTxFrequency(quint64 frequencyHz) = 0;

    // --- Receiver Configuration ---
    virtual void setActiveReceiverCount(int count) = 0;
    virtual void setSampleRate(int sampleRate) = 0;

    // --- Hardware Control ---
    virtual void setAttenuator(int dB) = 0;
    virtual void setPreamp(bool enabled) = 0;
    virtual void setTxDrive(int level) = 0;
    virtual void setMox(bool enabled) = 0;
    virtual void setAntenna(int antennaIndex) = 0;

    // --- ADC Mapping ---
    virtual int getAdcForDdc(int /*ddc*/) const { return 0; }

signals:
    // --- State ---
    void connectionStateChanged(NereusSDR::ConnectionState state);
    void errorOccurred(NereusSDR::RadioConnectionError code, const QString& message);

    // --- Data ---
    // Emitted for each receiver's I/Q block.
    // hwReceiverIndex: 0-based hardware receiver number.
    // samples: interleaved float I/Q pairs, normalized to [-1.0, 1.0].
    void iqDataReceived(int hwReceiverIndex, const QVector<float>& samples);

    // Emitted when mic samples are available.
    void micDataReceived(const QVector<float>& samples);

    // --- Meters ---
    void meterDataReceived(float forwardPower, float reversePower,
                           float supplyVoltage, float paCurrent);

    // --- PA telemetry (Phase 3P-H Task 4) ---
    // Raw 16-bit ADC counts read from C&C status bytes (P1) or the
    // High-Priority status packet (P2). Per-board scaling to watts /
    // volts / amps lives in RadioModel (console.cs computeAlexFwdPower /
    // computeRefPower / convertToVolts / convertToAmps), because the
    // bridge constants vary per HPSDRModel.
    //
    // Sources:
    //   P1: networkproto1.c:332-356 [@501e3f5] — C0 cases 0x08/0x10/0x18
    //   P2: network.c:711-748        [@501e3f5] — High-Priority byte offsets 2-3, 10-11, 18-19, 45-46, 51-52, 53-54
    //
    // Fields (all uint16, raw ADC counts):
    //   fwdRaw      — fwd_power      (P1 AIN1, P2 bytes 10-11)
    //   revRaw      — rev_power      (P1 AIN2, P2 bytes 18-19)
    //   exciterRaw  — exciter_power  (P1 AIN5, P2 bytes 2-3)
    //   userAdc0Raw — user_adc0      (P1 AIN3 MKII PA volts, P2 bytes 53-54)
    //   userAdc1Raw — user_adc1      (P1 AIN4 MKII PA amps,  P2 bytes 51-52)
    //   supplyRaw   — supply_volts   (P1 AIN6 Hermes volts,  P2 bytes 45-46)
    void paTelemetryUpdated(quint16 fwdRaw, quint16 revRaw, quint16 exciterRaw,
                            quint16 userAdc0Raw, quint16 userAdc1Raw,
                            quint16 supplyRaw);

    // ADC overflow detected.
    void adcOverflow(int adc);

    // Radio firmware info received during handshake.
    void firmwareInfoReceived(int version, const QString& details);

protected:
    void setState(ConnectionState newState);

    std::atomic<ConnectionState> m_state{ConnectionState::Disconnected};
    RadioInfo m_radioInfo;
    HardwareProfile m_hardwareProfile;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::ConnectionState)
Q_DECLARE_METATYPE(NereusSDR::RadioConnectionError)
