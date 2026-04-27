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

// Antenna routing parameters — Phase 3P-I-a.
// Ports Thetis Alex.cs:310-413 UpdateAlexAntSelection output
// (HPSDR/Alex.cs [v2.10.3.13 @501e3f5]). Composed by
// RadioModel::applyAlexAntennaForBand and pushed to RadioConnection.
//
// 3P-I-a scope: trxAnt + txAnt are independent ANT1..ANT3 ports.
//     rxOnlyAnt, rxOut, tx remain zero until 3P-I-b/3M-1.
struct AntennaRouting {
    int  rxOnlyAnt {0};    // 0=none, 1=RX1, 2=RX2, 3=XVTR  (3P-I-b)
    int  trxAnt    {1};    // 1..3 — shared RX/TX port on Alex
    int  txAnt     {1};    // 1..3 — independent TX port (P2 Alex1)
    bool rxOut     {false}; // RX bypass relay active        (3P-I-b)
    bool tx        {false}; // current MOX state             (3M-1)
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
    virtual void setAntennaRouting(AntennaRouting routing) = 0;

    // Push TX-side step attenuator value to hardware.
    //
    // From Thetis ChannelMaster/netInterface.c:1006 [v2.10.3.13]
    // SetTxAttenData(int bits): broadcasts bits to all ADC tx_step_attn
    // fields and triggers CmdTx() to emit the updated frame.
    // Called by StepAttenuatorController::onMoxHardwareFlipped (F.2).
    //
    // Default no-op: subclasses that don't yet implement TX ATT (e.g. a
    // stub test connection) skip silently. P1/P2 override this method.
    virtual void setTxStepAttenuation(int /*dB*/) {}

    // ── TX path (3M-1a) ────────────────────────────────────────────────

    /// Push TX I/Q samples to the radio.
    ///
    /// `iq` points to interleaved I/Q float32 samples; `n` is the
    /// number of complex samples (so the buffer has 2*n floats).
    /// Implementations:
    ///   - P1: write to EP2 zones in the 1032-byte Metis frame
    ///     (interleaved with status bytes per the Metis spec).
    ///   - P2: write to UDP port 1029 with the per-frame layout
    ///     specified by OpenHPSDR Protocol 2.
    ///
    /// Audio-thread context: callers run this in the WDSP audio thread.
    /// The implementation must not block; it queues to the connection
    /// thread for actual UDP send.
    ///
    /// Cite: pre-code review §7.1 (P1 EP2 TX I/Q layout),
    ///        §7.6 (P2 port 1029 TX I/Q layout).
    virtual void sendTxIq(const float* iq, int n) = 0;

    /// Set the Alex T/R relay wire bit.
    ///
    /// Distinct from `setMox(bool)`:
    ///   - `setMox(true)` asserts the MOX bit on the next outbound
    ///     frame (P1 byte 3 bit 0 / P2 high-pri byte 4 bit 1).
    ///   - `setTrxRelay(true)` engages the Alex T/R relay path so
    ///     the PA can drive the antenna. P1 wire bit is C3 byte 6
    ///     bit 7 with INVERTED semantic — `1` = disabled (bypass),
    ///     `0` = enabled (normal TX). When `enabled` is true the
    ///     implementation writes `0` to bit 7 (engaged); when false
    ///     (PA disabled), writes `1` (bypass).
    ///   - P2: routed via Saturn register C0=0x24 indirect writes;
    ///     stub for 3M-1a (deferred to 3M-3).
    ///
    /// `enabled` follows caller-friendly convention (true = TX path
    /// engaged); the implementation handles the bit inversion on the
    /// wire. State is stored in base-class `m_trxRelay`.
    ///
    /// 3M-1a wires this from `MoxController::hardwareFlipped` via
    /// `RadioModel::onMoxHardwareFlipped` (Task F.1).
    ///
    /// Cite: pre-code review §7.2 + deskhpsdr/src/old_protocol.c:2909-2910
    ///       (T/R relay bit C3[6] bit 7, inverted sense).
    virtual void setTrxRelay(bool enabled) = 0;

    bool isTrxRelayEngaged() const noexcept { return m_trxRelay; }

    // DEPRECATED — call setAntennaRouting directly. Kept for one release
    // cycle as a rollback hatch per docs/architecture/antenna-routing-design.md §7.7.
    // Removed in the release following 3P-I-b.
    Q_DECL_DEPRECATED_X("Use setAntennaRouting")
    void setAntenna(int antennaIndex) {
        const int ant = (antennaIndex >= 0 && antennaIndex <= 2) ? antennaIndex + 1 : 1;
        setAntennaRouting({0, ant, ant, false, false});
    }

    // --- ADC Mapping ---
    virtual int getAdcForDdc(int /*ddc*/) const { return 0; }

    // --- TX output sample rate (bench fix round 3 — Issue B) ---
    //
    // Returns the radio's negotiated TX I/Q output sample rate in Hz.
    // This is the rate passed to OpenChannel() as outputSampleRate, and
    // therefore the rate at which WDSP's TXA rsmpout resampler delivers
    // samples to fexchange2's Iout/Qout buffers.
    //
    // P1 (HL2/Atlas/Hermes/Angelia/Orion): TX I/Q flows into EP2 zones at
    //   the radio's sample rate.  For single-RX operation this is always
    //   48000 Hz (single-rate mode).  P1RadioConnection returns 48000.
    //
    // P2 (Saturn/ANAN-G2): TX I/Q flows to UDP port 1029 at 192000 Hz.
    //   P2RadioConnection returns 192000 — derived from m_tx[0].samplingRate
    //   (always 192 kHz per Thetis netInterface.c:1513 [v2.10.3.13]).
    //
    // Default implementation returns 48000 (correct for P1 and stubs).
    // P2RadioConnection overrides to return 192000.
    //
    // From Thetis wdsp/cmaster.c:183 [v2.10.3.13] — ch_outrate parameter.
    // From Thetis netInterface.c:1513 [v2.10.3.13] — P2 tx always 192 kHz.
    virtual int txSampleRate() const { return 48000; }

    // --- Watchdog ---
    // Enable / disable the radio-side network watchdog. When enabled,
    // the radio firmware drops TX if it stops seeing C&C traffic.
    // Mirrors SetWatchdogTimer(int bits) in NetworkIOImports.cs:197-198
    // [v2.10.3.13]. Boolean only — no host-side timeout parameter.
    //
    // Wire bit emission: P1 implemented in P1RadioConnection::sendMetisStart
    // (RUNSTOP pkt[3] bit 7 per dsopenhpsdr1.v:399-400 [@7472bd1]).
    // P2 wire bit deferred to E.8 — see tracking comment in P2RadioConnection.cpp.
    virtual void setWatchdogEnabled(bool enabled) = 0;

    bool isWatchdogEnabled() const noexcept { return m_watchdogEnabled; }

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

    // Shared boolean state for setWatchdogEnabled / isWatchdogEnabled.
    // Both P1 and P2 overrides read/write this field.
    //
    // Default TRUE: HL2 firmware (dsopenhpsdr1.v:399-400) interprets RUNSTOP
    // byte bit 7 as watchdog_disable (1 = disabled, 0 = enabled). When this
    // field is true, sendMetisStart/sendMetisStop write bit 7 = 0 (watchdog
    // enabled), matching deskhpsdr's implicit behavior (buffer[3] = command
    // with no bit-7 OR → bit 7 = 0 → watchdog active by default).
    //
    // 3M-0 used false here (bug): first sendMetisStart would have written
    // bit 7 = 1 → watchdog disabled on connect. Fixed in 3M-1a Task E.5.
    // From deskhpsdr/src/old_protocol.c:3811 [@120188f]:
    //   buffer[3] = command;  // 0x01 start / 0x00 stop — bit 7 never set
    bool m_watchdogEnabled{true};

    // Shared state for setTrxRelay / isTrxRelayEngaged (3M-1a Task E.1).
    // true = TX path engaged (bit 7 of P1 C3 bank 6 written as 0 — inverted
    // sense). P2 stub; wire emit deferred to 3M-3.
    bool m_trxRelay{false};
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::ConnectionState)
Q_DECLARE_METATYPE(NereusSDR::RadioConnectionError)
