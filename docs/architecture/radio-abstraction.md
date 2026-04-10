# Radio Abstraction Layer — Architecture Design

**Status:** Phase 2A — Design Complete
**Date:** 2026-04-08
**Author:** JJ Boyd ~KG4VCF, Co-Authored with Claude Code

---

## 1. Overview

The radio abstraction layer is the foundation of NereusSDR's hardware
interface. It handles discovery, connection, control, and I/Q data reception
for all OpenHPSDR Protocol 1 and Protocol 2 radios.

**Key architectural distinction from AetherSDR:** OpenHPSDR radios have NO
concept of slices, panadapters, or radio-side DSP. The radio is an ADC/DAC
with network transport. All intelligence — slicing, demodulation, FFT,
waterfall, metering — is client-side. This means the radio abstraction layer
is thinner than AetherSDR's but the client-side processing pipeline is
significantly more complex.

### Design Goals

- Support Protocol 1 and Protocol 2 behind a unified interface
- Handle radio discovery, connection lifecycle, and reconnection
- Manage multiple independent receivers per radio (up to 7 for P1)
- Expose a clean Qt6 signal/slot interface for model binding
- Follow AetherSDR's thread isolation pattern: auto-queued signals, no shared
  mutexes
- Keep protocol-specific details encapsulated in implementation classes

### Supported Hardware

| Board Type | Code | Protocol | ADCs | Max RX | Radios |
|-----------|------|----------|------|--------|--------|
| Metis | 0 | P1 | 1 | 3 | Metis board |
| Hermes | 1 | P1 | 1 | 4 | ANAN-10/10E/100 |
| Griffin | 2 | P1 | 1 | 4 | (deprecated) |
| Angelia | 4 | P1 | 2 | 7 | ANAN-100D |
| Orion | 5 | P1 | 2 | 7 | ANAN-200D |
| Hermes Lite | 6 | P1 | 1 | 4 | Hermes Lite 2 |
| Orion MkII | 10 | P2 | 2 | 7 | ANAN-7000DLE/8000DLE |
| Saturn | 11 | P2 | 2 | 7+ | ANAN-G2 |

---

## 2. RadioInfo Struct

`RadioInfo` is the data carrier for discovered radio metadata. It is
value-type, copyable, and protocol-agnostic.

```cpp
// src/core/RadioDiscovery.h
namespace NereusSDR {

enum class BoardType : int {
    Metis       = 0,
    Hermes      = 1,
    Griffin     = 2,
    // 3 is unused
    Angelia     = 4,
    Orion       = 5,
    HermesLite  = 6,
    // 7-9 unused
    OrionMkII   = 10,
    Saturn      = 11,
    Unknown     = -1
};

enum class ProtocolVersion : int {
    Protocol1 = 1,
    Protocol2 = 2
};

struct RadioInfo {
    // Identity
    QString name;                        // User-friendly name, e.g. "ANAN-7000DLE"
    QString macAddress;                  // MAC as "AA:BB:CC:DD:EE:FF" (primary key)
    QHostAddress address;                // IP address on local network
    quint16 port{1024};                  // Always 1024 for OpenHPSDR

    // Hardware
    BoardType boardType{BoardType::Unknown};
    int firmwareVersion{0};              // P1: byte 9 of discovery response
    int adcCount{1};                     // Derived from boardType (1 or 2)
    int maxReceivers{4};                 // Board-dependent max simultaneous RX

    // Protocol
    ProtocolVersion protocol{ProtocolVersion::Protocol1};
    bool inUse{false};                   // Another client already connected

    // Capabilities (derived from boardType at parse time)
    bool hasDiversityReceiver{false};    // Angelia+ have 2 ADCs for diversity
    bool hasPureSignal{false};           // Boards supporting PA linearization
    int maxSampleRate{384000};           // Max supported sample rate in Hz

    // Display helpers
    QString displayName() const;
    static QString boardTypeName(BoardType type);
    static int adcCountForBoard(BoardType type);
    static int maxReceiversForBoard(BoardType type);

    // Comparison — radios are identified by MAC address
    bool operator==(const RadioInfo& other) const {
        return macAddress == other.macAddress;
    }
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::RadioInfo)
```

### Board-Derived Capabilities

```cpp
// RadioDiscovery.cpp
int RadioInfo::adcCountForBoard(BoardType type) {
    switch (type) {
        case BoardType::Angelia:
        case BoardType::Orion:
        case BoardType::OrionMkII:
        case BoardType::Saturn:
            return 2;
        default:
            return 1;
    }
}

int RadioInfo::maxReceiversForBoard(BoardType type) {
    switch (type) {
        case BoardType::Metis:      return 3;
        case BoardType::Hermes:     return 4;
        case BoardType::Griffin:    return 4;
        case BoardType::HermesLite: return 4;
        case BoardType::Angelia:    return 7;
        case BoardType::Orion:      return 7;
        case BoardType::OrionMkII:  return 7;
        case BoardType::Saturn:     return 7;
        default:                    return 1;
    }
}
```

---

## 3. RadioDiscovery

### Discovery Protocol

Both P1 and P2 radios respond to the same UDP broadcast on port 1024. The
discovery packet format and response parsing differ by protocol.

#### P1 Discovery Packet (PC to Radio)

A 63-byte packet sent as UDP broadcast to `255.255.255.255:1024`:

```
Byte 0-1:   0xEF 0xFE          (Metis sync)
Byte 2:     0x02                (Discovery request)
Bytes 3-62: 0x00                (padding)
```

#### P1 Discovery Response (Radio to PC)

A 60-byte (minimum) response from the radio:

```
Byte 0-1:   0xEF 0xFE          (Metis sync)
Byte 2:     0x02                (Discovery response) — OR 0x03 if radio is in use
Byte 3-8:   MAC address         (6 bytes, big-endian)
Byte 9:     Firmware version    (single byte, decimal)
Byte 10:    Board type          (see BoardType enum)
```

If byte 2 is `0x03`, the radio is currently in use by another client.

#### P2 Discovery Response

P2 radios respond to the same broadcast but with a different response format:

```
Byte 0:     0x00                (P2 indicator — distinguishes from P1's 0xEF)
Byte 1-2:   Sequence number
Byte 3:     0x02                (Discovery response)
Byte 4-9:   MAC address         (6 bytes)
Byte 10:    Firmware version
Byte 11:    Board type          (note: offset differs from P1)
Byte 12-15: Capabilities word
```

Protocol detection is done by examining byte 0 of the response:
- `0xEF` = Protocol 1 (followed by `0xFE` sync)
- `0x00` = Protocol 2

### Class Interface

```cpp
// src/core/RadioDiscovery.h
namespace NereusSDR {

class RadioDiscovery : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool discovering READ isDiscovering NOTIFY discoveringChanged)

public:
    explicit RadioDiscovery(QObject* parent = nullptr);
    ~RadioDiscovery() override;

    void startDiscovery();
    void stopDiscovery();
    bool isDiscovering() const { return m_discovering; }

    QList<RadioInfo> discoveredRadios() const;
    std::optional<RadioInfo> radioByMac(const QString& mac) const;

signals:
    void radioDiscovered(const NereusSDR::RadioInfo& info);
    void radioUpdated(const NereusSDR::RadioInfo& info);
    void radioLost(const QString& macAddress);
    void discoveringChanged(bool discovering);
    void discoveryError(const QString& message);

private slots:
    void onReadyRead();
    void onStaleCheck();
    void sendDiscoveryPacket();

private:
    // Discovery packet construction
    QByteArray buildDiscoveryPacket() const;

    // Response parsing — returns std::nullopt if packet is not a valid response
    std::optional<RadioInfo> parseP1Response(const QByteArray& data,
                                             const QHostAddress& sender) const;
    std::optional<RadioInfo> parseP2Response(const QByteArray& data,
                                             const QHostAddress& sender) const;

    // MAC extraction from raw bytes
    static QString macToString(const char* bytes);

    // Configuration
    static constexpr quint16 kDiscoveryPort = 1024;
    static constexpr int kDiscoveryIntervalMs = 2000;  // Send broadcast every 2s
    static constexpr int kStaleTimeoutMs = 10000;      // Radio lost after 10s silence

    // State
    bool m_discovering{false};
    QUdpSocket* m_socket{nullptr};
    QTimer m_discoveryTimer;   // Periodic broadcast sender
    QTimer m_staleTimer;       // Periodic staleness checker

    // Radio tracking — keyed by MAC address
    QMap<QString, RadioInfo> m_radios;
    QMap<QString, qint64> m_lastSeen;  // MAC -> QDateTime::currentMSecsSinceEpoch()
};

} // namespace NereusSDR
```

### Discovery Lifecycle

```
startDiscovery()
  │
  ├── Bind UDP socket to 0.0.0.0:0 (ephemeral port)
  ├── Enable SO_BROADCAST on socket
  ├── Start m_discoveryTimer (2s interval)
  ├── Start m_staleTimer (5s interval)
  └── Send first discovery packet immediately
         │
         ▼
sendDiscoveryPacket()    ◄── Timer fires every 2s
  │
  └── Broadcast 63-byte packet to 255.255.255.255:1024
         │
         ▼
onReadyRead()            ◄── Radio responds
  │
  ├── Read datagram
  ├── Check byte 0: 0xEF → parseP1Response(), 0x00 → parseP2Response()
  ├── If MAC not seen before: emit radioDiscovered()
  ├── If MAC seen but data changed: emit radioUpdated()
  └── Update m_lastSeen[mac] timestamp
         │
         ▼
onStaleCheck()           ◄── Timer fires every 5s
  │
  ├── For each tracked radio:
  │   └── If (now - m_lastSeen[mac]) > kStaleTimeoutMs:
  │       ├── Remove from m_radios
  │       └── emit radioLost(mac)
  └── (repeat)
```

### P1 Discovery Packet Parsing (Detail)

```cpp
std::optional<RadioInfo> RadioDiscovery::parseP1Response(
    const QByteArray& data, const QHostAddress& sender) const
{
    // Minimum 11 bytes needed: 2 sync + 1 status + 6 MAC + 1 FW + 1 board
    if (data.size() < 11) { return std::nullopt; }

    const auto* raw = reinterpret_cast<const unsigned char*>(data.constData());

    // Verify Metis sync bytes
    if (raw[0] != 0xEF || raw[1] != 0xFE) { return std::nullopt; }

    // Byte 2: 0x02 = available, 0x03 = in use
    if (raw[2] != 0x02 && raw[2] != 0x03) { return std::nullopt; }

    RadioInfo info;
    info.address = sender;
    info.port = kDiscoveryPort;
    info.protocol = ProtocolVersion::Protocol1;
    info.inUse = (raw[2] == 0x03);
    info.macAddress = macToString(data.constData() + 3);  // Bytes 3-8
    info.firmwareVersion = raw[9];
    info.boardType = static_cast<BoardType>(raw[10]);
    info.adcCount = RadioInfo::adcCountForBoard(info.boardType);
    info.maxReceivers = RadioInfo::maxReceiversForBoard(info.boardType);
    info.name = RadioInfo::boardTypeName(info.boardType);
    info.hasDiversityReceiver = (info.adcCount >= 2);
    // PureSignal requires 2+ ADCs for feedback path (Metis and Hermes are single-ADC)
    // Corrected 2026-04-10: was (boardType != Metis) which wrongly included Hermes
    info.hasPureSignal = (info.adcCount >= 2);

    return info;
}
```

---

## 4. RadioConnection — Protocol-Agnostic Interface

`RadioConnection` is an abstract base class defining the interface for both
P1 and P2 connections. Protocol-specific behavior is implemented in
`P1RadioConnection` and `P2RadioConnection` subclasses.

### Connection State Machine

```
                     connectToRadio()
    ┌──────────────┐ ─────────────────► ┌──────────────┐
    │ Disconnected │                     │  Connecting  │
    └──────┬───────┘ ◄───────────────── └──────┬───────┘
           │          disconnect()              │
           │          timeout                   │ success
           │                                    ▼
           │                            ┌──────────────┐
           │                            │  Connected   │
           │                            └──────┬───────┘
           │                                    │
           │         ┌──────────────┐           │ error
           └──────── │    Error     │ ◄─────────┘
             auto    └──────────────┘
           reconnect   │
           (3s delay)  │ disconnect() or max retries
                       ▼
                ┌──────────────┐
                │ Disconnected │
                └──────────────┘
```

States:
- **Disconnected:** No connection. Ready to connect.
- **Discovering:** (Optional sub-state) Sending targeted discovery to confirm
  radio is still reachable before opening data streams.
- **Connecting:** P1: Sending start command and waiting for I/Q data. P2:
  TCP handshake in progress.
- **Connected:** Actively exchanging data with the radio.
- **Error:** Connection lost unexpectedly. Will auto-reconnect after 3s
  unless intentionally disconnected.

### Base Class Interface

```cpp
// src/core/RadioConnection.h
namespace NereusSDR {

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

// Abstract base class for radio connections.
// Subclasses live on the Connection worker thread.
// Call init() after moveToThread() to create sockets/timers on the worker thread.
class RadioConnection : public QObject {
    Q_OBJECT
    Q_PROPERTY(ConnectionState state READ state NOTIFY connectionStateChanged)

public:
    explicit RadioConnection(QObject* parent = nullptr);
    ~RadioConnection() override;

    ConnectionState state() const { return m_state.load(); }
    bool isConnected() const { return m_state.load() == ConnectionState::Connected; }
    const RadioInfo& radioInfo() const { return m_radioInfo; }

    // --- Factory ---
    // Creates the appropriate subclass based on RadioInfo::protocol.
    static std::unique_ptr<RadioConnection> create(const RadioInfo& info);

public slots:
    // Must be called on the worker thread after moveToThread().
    // Creates sockets, timers, and other thread-local resources.
    virtual void init() = 0;

    // Connect to the specified radio. Auto-queued from main thread.
    virtual void connectToRadio(const RadioInfo& info) = 0;

    // Graceful disconnect. Sets intentional flag to suppress auto-reconnect.
    virtual void disconnect() = 0;

    // --- Frequency Control ---
    // Set receiver frequency. Protocol handles encoding (phase word vs Hz).
    virtual void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) = 0;

    // Set TX frequency.
    virtual void setTxFrequency(quint64 frequencyHz) = 0;

    // --- Receiver Configuration ---
    // Set number of active receivers (1-7 for P1).
    // Affects I/Q sample interleaving and available bandwidth.
    virtual void setActiveReceiverCount(int count) = 0;

    // Set sample rate. Valid values: 48000, 96000, 192000, 384000.
    virtual void setSampleRate(int sampleRate) = 0;

    // --- Hardware Control ---
    // Set attenuator (0-31 dB for most boards).
    virtual void setAttenuator(int adc, int dB) = 0;

    // Set preamp/LNA enable.
    virtual void setPreamp(int adc, bool enabled) = 0;

    // Set TX drive level (0-255).
    virtual void setTxDrive(int level) = 0;

    // MOX (transmit enable).
    virtual void setMox(bool enabled) = 0;

    // Antenna selection.
    virtual void setAntenna(int rx, int antennaIndex) = 0;

signals:
    // --- State ---
    void connectionStateChanged(NereusSDR::ConnectionState state);
    void errorOccurred(const QString& message);

    // --- Data ---
    // Emitted for each receiver's I/Q block extracted from a frame.
    // receiverIndex: 0-based receiver number.
    // samples: interleaved float I/Q pairs, normalized to [-1.0, 1.0].
    // sampleCount: number of I/Q pairs in the buffer.
    void iqDataReceived(int receiverIndex, const QVector<float>& samples);

    // Emitted when mic samples are available (P1: interleaved with RX I/Q).
    void micDataReceived(const QVector<float>& samples);

    // --- Meters ---
    // Forward power, reverse power, supply voltage, PA current, temperature.
    void meterDataReceived(float forwardPower, float reversePower,
                           float supplyVoltage, float paCurrent,
                           float temperature);

    // --- Radio Status ---
    // ADC overflow detected.
    void adcOverflow(int adc);

    // Radio firmware info received (P2 command response).
    void firmwareInfoReceived(const QString& key, const QString& value);

protected:
    void setState(ConnectionState newState);

    std::atomic<ConnectionState> m_state{ConnectionState::Disconnected};
    RadioInfo m_radioInfo;
    bool m_intentionalDisconnect{false};
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::ConnectionState)
```

---

## 5. P1RadioConnection — Protocol 1 Implementation

Protocol 1 uses UDP-only communication. All control and data flows through
1032-byte Metis frames on port 1024.

### EP (Endpoint) Architecture

```
PC ──► Radio (EP2): C&C commands + TX I/Q samples
PC ──► Radio (EP4): Wideband data (not used in normal operation)

Radio ──► PC (EP6): RX I/Q samples + mic samples + C&C feedback
```

### Start/Stop Sequence

**Starting the radio (P1):**
1. Send a Metis start command (0xEF 0xFE 0x04 0x01 followed by 60 zero bytes)
2. Radio begins streaming EP6 frames to the PC's IP/port
3. PC begins sending EP2 frames with C&C data at a regular cadence

**Stopping the radio (P1):**
1. Send a Metis stop command (0xEF 0xFE 0x04 0x00 followed by 60 zero bytes)
2. Radio stops streaming
3. Close socket

### Class Interface

```cpp
// src/core/P1RadioConnection.h
namespace NereusSDR {

class MetisFrameParser;   // Forward declaration

class P1RadioConnection : public RadioConnection {
    Q_OBJECT

public:
    explicit P1RadioConnection(QObject* parent = nullptr);
    ~P1RadioConnection() override;

public slots:
    void init() override;
    void connectToRadio(const RadioInfo& info) override;
    void disconnect() override;

    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) override;
    void setTxFrequency(quint64 frequencyHz) override;
    void setActiveReceiverCount(int count) override;
    void setSampleRate(int sampleRate) override;
    void setAttenuator(int adc, int dB) override;
    void setPreamp(int adc, bool enabled) override;
    void setTxDrive(int level) override;
    void setMox(bool enabled) override;
    void setAntenna(int rx, int antennaIndex) override;

private slots:
    void onUdpReadyRead();
    void onSendCandC();         // Timer-driven C&C frame transmission
    void onWatchdogTimeout();   // No data received within timeout
    void onReconnectTimeout();

private:
    // --- Frame Construction ---
    // Build a 1032-byte Metis EP2 frame containing current C&C state.
    QByteArray buildMetisFrame();

    // Build start/stop command packets.
    QByteArray buildStartCommand() const;
    QByteArray buildStopCommand() const;

    // --- C&C State ---
    // C&C is sent as a rotating sequence of address/data pairs.
    // Each USB frame carries 5 C&C bytes: C0 (address) + C1-C4 (data).
    // The address rotates through 0x00-0x09+ to cover all control registers.
    void updateCandCRegister(int address, const QByteArray& data);
    QByteArray getCandCBytes(int address) const;

    // Frequency encoding: phase_word = freq_hz * 2^32 / 122880000
    static quint32 frequencyToPhaseWord(quint64 frequencyHz);

    // --- Protocol State ---
    QUdpSocket* m_socket{nullptr};
    std::unique_ptr<MetisFrameParser> m_frameParser;

    QTimer* m_sendTimer{nullptr};      // C&C transmission cadence (~63 Hz)
    QTimer* m_watchdogTimer{nullptr};  // Data reception watchdog (5s)
    QTimer* m_reconnectTimer{nullptr}; // 3s auto-reconnect

    quint32 m_txSequence{0};     // Metis frame sequence number (PC->Radio)
    quint32 m_rxSequence{0};     // Last received sequence (for gap detection)
    int m_candcAddress{0};       // Current C&C address in rotation (0x00-0x09+)

    // C&C register shadow — 16 registers, 4 data bytes each
    std::array<std::array<quint8, 4>, 16> m_candcRegisters{};

    // Current configuration
    int m_activeReceivers{1};
    int m_sampleRate{48000};
    bool m_mox{false};

    // Frequency correction factor (1.0 = no correction)
    double m_frequencyCorrection{1.0};
};

} // namespace NereusSDR
```

### C&C Frame Timing

P1 requires a steady stream of EP2 frames to keep the radio alive. The
send timer fires at approximately 63 Hz (15.875 ms), matching the rate at
which the radio expects C&C updates. If the radio does not receive EP2 frames
for approximately 2 seconds, it will stop transmitting EP6 data.

---

## 6. P2RadioConnection — Protocol 2 Implementation

**CORRECTION (2026-04-08):** P2 uses **UDP-only** communication on multiple
dedicated ports, NOT TCP+UDP as originally described. This was confirmed by
pcap analysis of Thetis ChannelMaster traffic and reading the Thetis
`network.c` source. A single UDP socket (`listenSock`) handles all traffic.

### Transport Architecture

```
PC ◄─── UDP ───► Radio     Single socket, commands to ports 1024-1027
                           Radio responds from ports 1025-1041
                           Dispatch by source port of incoming packets
```

### Port Map (from Thetis ChannelMaster/network.c)

| Port | Direction | Content | Size |
|------|-----------|---------|------|
| 1024 | PC→Radio | CmdGeneral (port config, watchdog) | 60 bytes |
| 1025 | PC→Radio | CmdRx (ADC, rate, enables) | 1444 bytes |
| 1025 | Radio→PC | High-priority status feedback | 60 bytes |
| 1026 | PC→Radio | CmdTx (CW, keyer, mic) | 60 bytes |
| 1026 | Radio→PC | Mic samples | 132 bytes |
| 1027 | PC→Radio | CmdHighPriority (run, freq, drive) | 1444 bytes |
| 1027-1034 | Radio→PC | Wideband ADC data | 1028 bytes |
| 1028 | PC→Radio | RX audio (L/R) to radio | 260 bytes |
| 1029 | PC→Radio | TX I/Q to radio | 1444 bytes |
| 1035-1041 | Radio→PC | DDC I/Q data (DDC0-DDC6) | 1444 bytes |

### Class Interface

```cpp
// src/core/P2RadioConnection.h
namespace NereusSDR {

class P2RadioConnection : public RadioConnection {
    Q_OBJECT

public:
    explicit P2RadioConnection(QObject* parent = nullptr);
    ~P2RadioConnection() override;

public slots:
    void init() override;
    void connectToRadio(const RadioInfo& info) override;
    void disconnect() override;

    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) override;
    void setTxFrequency(quint64 frequencyHz) override;
    void setActiveReceiverCount(int count) override;
    void setSampleRate(int sampleRate) override;
    void setAttenuator(int adc, int dB) override;
    void setPreamp(int adc, bool enabled) override;
    void setTxDrive(int level) override;
    void setMox(bool enabled) override;
    void setAntenna(int rx, int antennaIndex) override;

private slots:
    void onUdpReadyRead();
    void onReconnectTimeout();
    void onSendCommands();       // Timer-driven command packet transmission

private:
    // --- Command Packets ---
    // Build and send structured UDP command packets to radio ports 1024-1027.
    void sendCmdGeneral();       // Port 1024: config, watchdog
    void sendCmdRx();            // Port 1025: DDC enables, rates, ADC assignment
    void sendCmdTx();            // Port 1026: CW keyer, mic control, TX config
    void sendCmdHighPriority();  // Port 1027: run, freq, drive

    // --- P2 Data Parsing ---
    // Dispatch incoming datagrams by source port.
    void processDataPacket(const QByteArray& data, quint16 sourcePort);

    // P2 encodes frequency directly in Hz (no phase word conversion).
    static QByteArray encodeFrequency(quint64 frequencyHz);

    // --- Sockets ---
    // NOTE: P2 is UDP-only (corrected from original TCP+UDP assumption).
    // Single socket for all communication, matching Thetis listenSock pattern.
    QUdpSocket* m_socket{nullptr};

    QTimer* m_sendTimer{nullptr};       // Command packet cadence
    QTimer* m_reconnectTimer{nullptr};

    quint32 m_commandSequence{0};

    // Current configuration (mirrors P1 for interface compatibility)
    int m_activeReceivers{1};
    int m_sampleRate{48000};
    bool m_mox{false};
    double m_frequencyCorrection{1.0};
};

} // namespace NereusSDR
```

### P2 Connection Sequence

**CORRECTION (Phase 3A):** P2 is UDP-only on multiple ports, NOT TCP+UDP.
Confirmed by pcap analysis and Thetis ChannelMaster/network.c source.

1. Bind a single UDP socket (matching Thetis `listenSock` pattern)
2. Send CmdGeneral to port 1024 with WDT=1 (watchdog) to start radio
3. Send CmdRx to port 1025 (DDC enables, sample rates, ADC assignments)
4. Send CmdHighPriority to port 1027 (run=1, frequencies, drive level)
5. Radio begins streaming: I/Q from ports 1035-1041, status from 1025, mic from 1026
6. PC continues sending command packets periodically to maintain watchdog

### P2 vs P1 Key Differences

| Aspect | Protocol 1 | Protocol 2 |
|--------|-----------|-----------|
| Frequency encoding | Phase word: `freq * 2^32 / 122880000` | Direct Hz (32-bit integer) |
| Control transport | C&C bytes embedded in data frames | Dedicated UDP command packets to ports 1024-1027 |
| Data transport | Single UDP stream, multiplexed | Per-DDC independent UDP streams (ports 1035-1041) |
| Acknowledgment | None (fire-and-forget) | Status feedback on port 1025 |
| Bandwidth | Limited by frame rate | Higher, no inter-receiver penalty |
| Receiver independence | All RX interleaved in EP6 | Independent data streams |

---

## 7. MetisFrameParser — P1 Frame Parsing

This is the most complex class in the radio abstraction layer. It handles
the binary parsing of 1032-byte Metis frames from EP6 (Radio to PC).

### Metis Frame Structure (1032 bytes)

```
┌─────────────────────────────────────────────────────────┐
│ Metis Header (8 bytes)                                  │
│  Byte 0-1: 0xEF 0xFE (sync)                            │
│  Byte 2:   0x06 (EP6 data endpoint)                     │
│  Byte 3:   Sequence number bits [31:24]                 │
│  Byte 4:   Sequence number bits [23:16]                 │
│  Byte 5:   Sequence number bits [15:8]                  │
│  Byte 6:   Sequence number bits [7:0]                   │
│  Byte 7:   0x00 (reserved)                              │
├─────────────────────────────────────────────────────────┤
│ USB Frame 1 (512 bytes)                                 │
│  Bytes 0-2:   0x7F 0x7F 0x7F (USB sync)                │
│  Bytes 3-7:   C&C data (C0, C1, C2, C3, C4)            │
│  Bytes 8-511:  I/Q + Mic samples (504 bytes)            │
├─────────────────────────────────────────────────────────┤
│ USB Frame 2 (512 bytes)                                 │
│  Bytes 0-2:   0x7F 0x7F 0x7F (USB sync)                │
│  Bytes 3-7:   C&C data (C0, C1, C2, C3, C4)            │
│  Bytes 8-511:  I/Q + Mic samples (504 bytes)            │
└─────────────────────────────────────────────────────────┘
```

### C&C Byte Format (in USB Frame Header)

Each USB frame carries 5 C&C bytes that provide feedback from the radio:

```
C0 (byte 3): Address + PTT/Dash/Dot status
  Bits [7:1]: C&C address (0-127)
  Bit [0]:    PTT (from radio, active high)

C1-C4 (bytes 4-7): Data for the current C&C address
```

The radio rotates through C&C addresses, sending different data at each
address. Key feedback addresses:

| C0 Address | C1 | C2 | C3 | C4 |
|-----------|-----|-----|-----|-----|
| 0x00 | ADC overflow | Hermes I01-I04 | Mercury FW | Penelope/Hermes FW |
| 0x01 | Forward power [15:8] | Forward power [7:0] | Reverse power [15:8] | Reverse power [7:0] |
| 0x02 | Supply volts [15:8] | Supply volts [7:0] | AIN3 [15:8] | AIN3 [7:0] |
| 0x03 | AIN4 [15:8] | AIN4 [7:0] | AIN6 [15:8] | AIN6 [7:0] |

### I/Q Sample Extraction

After the 5-byte C&C header, the remaining 504 bytes of each USB frame
contain I/Q samples. The sample format depends on the number of active
receivers:

**Sample block structure (per receiver, per sample):**

```
3 bytes: I sample (24-bit signed, big-endian)
3 bytes: Q sample (24-bit signed, big-endian)
```

When multiple receivers are active, samples are interleaved:

```
For N receivers, each sample block is:
  RX0_I (3 bytes) + RX0_Q (3 bytes)
  RX1_I (3 bytes) + RX1_Q (3 bytes)
  ...
  RX(N-1)_I (3 bytes) + RX(N-1)_Q (3 bytes)
  Mic_L (2 bytes) + Mic_R (2 bytes)      ← 16-bit mic samples
```

**Bytes per sample block:** `(N_receivers * 6) + 4`

**Samples per USB frame:** `504 / bytes_per_block` (truncated to integer)

| Receivers | Bytes/Block | Samples/Frame | Total I/Q pairs/frame |
|-----------|-------------|---------------|----------------------|
| 1 | 10 | 50 | 50 per RX |
| 2 | 16 | 31 | 31 per RX |
| 3 | 22 | 22 | 22 per RX |
| 4 | 28 | 18 | 18 per RX |
| 5 | 34 | 14 | 14 per RX |
| 6 | 40 | 12 | 12 per RX |
| 7 | 46 | 10 | 10 per RX |

### 24-Bit Sample Conversion

```cpp
// Convert 3 big-endian bytes to a normalized float in [-1.0, 1.0]
static float decode24BitSample(const unsigned char* p) {
    // Sign-extend: if bit 7 of first byte is set, value is negative
    qint32 value = (static_cast<qint32>(p[0]) << 16)
                 | (static_cast<qint32>(p[1]) << 8)
                 | (static_cast<qint32>(p[2]));

    // Sign-extend from 24-bit to 32-bit
    if (value & 0x800000) {
        value |= 0xFF000000;
    }

    // Normalize to [-1.0, 1.0]
    return static_cast<float>(value) / 8388607.0f;  // 2^23 - 1
}
```

### Class Interface

```cpp
// src/core/MetisFrameParser.h
namespace NereusSDR {

// Feedback data extracted from C&C bytes in received frames.
struct RadioFeedback {
    bool ptt{false};
    bool adcOverflow{false};
    quint16 forwardPower{0};
    quint16 reversePower{0};
    quint16 supplyVoltage{0};    // In ADC units, needs scaling
    quint16 paCurrent{0};
    int mercuryFirmware{0};
    int penelopeFirmware{0};
};

class MetisFrameParser {
public:
    explicit MetisFrameParser();

    // Configure the parser for the expected number of receivers.
    // Must be called before parseFrame(). Changes take effect on next frame.
    void setReceiverCount(int count);
    int receiverCount() const { return m_receiverCount; }

    // Parse a 1032-byte Metis frame.
    // Returns true if the frame was valid and data was extracted.
    // After a successful parse, call samplesForReceiver() and feedback().
    bool parseFrame(const QByteArray& frame);

    // Get I/Q samples for a specific receiver from the last parsed frame.
    // Returns interleaved float I/Q pairs normalized to [-1.0, 1.0].
    // receiverIndex is 0-based.
    const QVector<float>& samplesForReceiver(int receiverIndex) const;

    // Get mic samples from the last parsed frame.
    const QVector<float>& micSamples() const { return m_micSamples; }

    // Get C&C feedback data from the last parsed frame.
    const RadioFeedback& feedback() const { return m_feedback; }

    // Sequence number from the last parsed frame.
    quint32 lastSequence() const { return m_lastSequence; }

    // Detect sequence gaps (dropped UDP packets).
    bool hasSequenceGap() const { return m_sequenceGap; }
    int droppedFrames() const { return m_droppedFrames; }

private:
    // Parse a single 512-byte USB frame starting at the given offset.
    void parseUsbFrame(const unsigned char* data);

    // Extract C&C feedback from C0-C4 bytes.
    void parseCandCFeedback(const unsigned char* cc);

    // Decode I/Q sample block for all receivers.
    void decodeSamples(const unsigned char* data, int byteCount);

    // 24-bit big-endian to float conversion.
    static float decode24Bit(const unsigned char* p);

    // 16-bit big-endian to float conversion (for mic samples).
    static float decode16Bit(const unsigned char* p);

    // Configuration
    int m_receiverCount{1};
    int m_bytesPerBlock{10};   // (receiverCount * 6) + 4

    // Parsed data — one vector per receiver
    QVector<QVector<float>> m_rxSamples;  // m_rxSamples[rx] = {I0,Q0,I1,Q1,...}
    QVector<float> m_micSamples;
    RadioFeedback m_feedback;

    // Sequence tracking
    quint32 m_lastSequence{0};
    bool m_firstFrame{true};
    bool m_sequenceGap{false};
    int m_droppedFrames{0};

    // Empty vector returned when an invalid receiverIndex is requested
    static const QVector<float> kEmptySamples;
};

} // namespace NereusSDR
```

---

## 8. C&C (Command & Control) Protocol — P1 Detail

### Outgoing C&C Registers (PC to Radio, EP2)

The PC sends C&C data embedded in Metis EP2 frames. The C0 byte contains
the register address in bits [7:1] and the MOX flag in bit [0].

#### Register Map

**C0 = 0x00: General Configuration**
```
C1[7]:   0 (reserved)
C1[6:3]: Sample rate: 0000=48k, 0001=96k, 0010=192k, 0011=384k
C1[2:0]: Number of receivers - 1 (0=1rx, 1=2rx, ..., 6=7rx)
C2[7]:   Mic source: 0=Janus, 1=Penelope
C2[6]:   Clock source: 0=Mercury, 1=Penelope (not commonly used)
C2[5:4]: 00 (reserved)
C2[3:2]: Mic rate: 00=48k
C2[1]:   0 (reserved)
C2[0]:   0 (reserved)
C3[7:0]: Board-specific config
C4[7:0]: Board-specific config
```

**C0 = 0x01: TX NCO Frequency**
```
C1-C4: 32-bit phase word for TX NCO
  phase_word = (uint32_t)(freq_hz * 4294967296.0 / 122880000.0)
```

**C0 = 0x02: RX1 NCO Frequency**
```
C1-C4: 32-bit phase word for RX1 NCO
```

**C0 = 0x03: RX2 NCO Frequency**
```
C1-C4: 32-bit phase word for RX2 NCO
```

**C0 = 0x04 through 0x08: RX3-RX7 NCO Frequencies**
```
C1-C4: 32-bit phase word for RX3-RX7 NCOs respectively
```

**C0 = 0x09: TX Drive and Mic Boost**
```
C1[7:0]: TX drive level (0-255)
C2[7]:   VNA mode (0=off, 1=on)
C2[6]:   Alex manual filter mode
C2[5:0]: Alex RX antenna select
C3[7]:   IF duplex
C3[6]:   TX relay select
C3[5:0]: Board-specific
C4[7:0]: Board-specific
```

**C0 = 0x0A: Attenuator and Preamp**
```
C1[7]:   ADC1 preamp/LNA: 0=off, 1=on
C1[6:5]: Board-specific (ADC1 attenuator on Hermes)
C1[4:0]: ADC1 attenuator (0-31 dB, 1 dB steps)
C2[7]:   ADC2 preamp/LNA: 0=off, 1=on (Angelia/Orion)
C2[4:0]: ADC2 attenuator (0-31 dB, Angelia/Orion)
C3-C4:   Board-specific (Alex filter/antenna on Orion)
```

### Frequency Phase Word Calculation

P1 uses a numerically controlled oscillator (NCO). Frequency is encoded as a
32-bit phase accumulator increment:

```
phase_word = (uint32_t)((double)freq_hz * 2^32 / clock_freq)

Where clock_freq = 122880000 Hz (122.88 MHz master clock)
```

```cpp
quint32 P1RadioConnection::frequencyToPhaseWord(quint64 frequencyHz) {
    // Apply frequency correction factor
    double correctedFreq = static_cast<double>(frequencyHz) * m_frequencyCorrection;
    double phaseWord = correctedFreq * 4294967296.0 / 122880000.0;
    return static_cast<quint32>(phaseWord);
}
```

Example: 14.200 MHz = `14200000 * 4294967296 / 122880000 = 496,741,916` = `0x1D9CC31C`

### C&C Rotation

The PC must continuously cycle through all C&C addresses, sending two USB
frames per Metis frame. Each USB frame carries one C&C address. The rotation
pattern for a typical transmission cycle:

```
Frame 0, USB Frame 0: C0=0x00 (config: sample rate, receiver count)
Frame 0, USB Frame 1: C0=0x01 (TX frequency)
Frame 1, USB Frame 0: C0=0x02 (RX1 frequency)
Frame 1, USB Frame 1: C0=0x03 (RX2 frequency)
Frame 2, USB Frame 0: C0=0x04 (RX3 frequency)
Frame 2, USB Frame 1: C0=0x05 (RX4 frequency)
... and so on ...
Frame N, USB Frame 0: C0=0x09 (TX drive)
Frame N, USB Frame 1: C0=0x0A (attenuator/preamp)
Frame N+1, USB Frame 0: C0=0x00 (back to start)
...
```

This ensures all control registers are updated approximately every
`(num_addresses / 2) * 15.875ms`. For 11 addresses, the full rotation
takes approximately 87ms.

### Board-Specific C&C Differences

| Feature | Hermes | Angelia | Orion | Hermes Lite 2 |
|---------|--------|---------|-------|---------------|
| ADC1 attenuator | C0=0x0A, C1[4:0], 0-31 dB | Same | Same | Same |
| ADC2 attenuator | N/A (single ADC) | C0=0x0A, C2[4:0] | C0=0x0A, C2[4:0] | N/A |
| Alex filters | Basic (C0=0x09) | Extended | Full Alex2 | N/A |
| Preamp | C0=0x0A, C1[7] | C0=0x0A, C1[7] + C2[7] | Same as Angelia | C0=0x0A, C1[7] |
| PureSignal FB | N/A | Dedicated RX | Dedicated RX | N/A |
| Max TX power | 255 | 255 | 255 | ~255 (board-limited) |

---

## 9. ReceiverManager — Client-Side Receiver Management

The radio has no concept of "slices" or "receivers" as logical entities.
It simply streams interleaved I/Q data for N hardware receivers.
`ReceiverManager` is the client-side abstraction that maps logical receivers
to hardware RX channels and WDSP DSP instances.

### Conceptual Model

```
┌─────────────────────────────────────────────────────────────────┐
│                        ReceiverManager                          │
├─────────┬──────────┬──────────┬──────────┬──────────┬──────────┤
│  RX 0   │  RX 1    │  RX 2    │  RX 3    │  ...     │  RX N-1  │
│ (Main)  │ (Sub/Div)│ (Second) │ (PS FB)  │          │          │
├─────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ WDSP    │ WDSP     │ WDSP     │ WDSP     │          │ WDSP     │
│ Chan 0  │ Chan 1   │ Chan 2   │ Chan 3   │  ...     │ Chan N-1 │
├─────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ Freq    │ Freq     │ Freq     │ Freq     │          │ Freq     │
│ Mode    │ Mode     │ Mode     │ Mode     │          │ Mode     │
│ Filter  │ Filter   │ Filter   │ Filter   │          │ Filter   │
│ DSP cfg │ DSP cfg  │ DSP cfg  │ DSP cfg  │          │ DSP cfg  │
└─────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
         │                       │
         ▼                       ▼
    Hardware RX 0           Hardware RX 2
  (Radio C&C address 0x02)  (Radio C&C address 0x04)
```

### Class Interface

```cpp
// src/core/ReceiverManager.h
namespace NereusSDR {

// Per-receiver configuration state.
struct ReceiverConfig {
    int receiverIndex{-1};       // Logical receiver index (0-based)
    int hardwareRx{0};           // Hardware receiver mapped in C&C (0-based)
    int wdspChannel{-1};         // WDSP channel number (assigned at creation)
    quint64 frequencyHz{14200000};
    int sampleRate{48000};
    bool active{false};
    bool isDiversity{false};     // Sub-receiver for diversity combining
    bool isPureSignalFeedback{false}; // Dedicated to PA linearization
};

class ReceiverManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(int activeReceiverCount READ activeReceiverCount
               NOTIFY activeReceiverCountChanged)
    Q_PROPERTY(int maxReceivers READ maxReceivers CONSTANT)

public:
    explicit ReceiverManager(QObject* parent = nullptr);
    ~ReceiverManager() override;

    // --- Configuration ---
    // Set the maximum based on connected radio's capability.
    void setMaxReceivers(int max);
    int maxReceivers() const { return m_maxReceivers; }

    // --- Receiver Lifecycle ---
    // Create a new receiver. Returns the receiver index, or -1 on failure.
    // The receiver is inactive until activate() is called.
    int createReceiver();

    // Destroy a receiver and release its WDSP channel.
    void destroyReceiver(int receiverIndex);

    // Activate/deactivate a receiver.
    // Activating increments the hardware receiver count sent to the radio.
    // Deactivating decrements it (the radio re-interleaves).
    void activateReceiver(int receiverIndex);
    void deactivateReceiver(int receiverIndex);

    // --- State Queries ---
    int activeReceiverCount() const;
    QList<int> activeReceiverIndices() const;
    bool isReceiverActive(int receiverIndex) const;
    const ReceiverConfig& receiverConfig(int receiverIndex) const;

    // --- Receiver Configuration ---
    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz);
    void setReceiverSampleRate(int receiverIndex, int sampleRate);

    // --- Hardware Mapping ---
    // Map a logical receiver to a hardware RX index.
    // This determines which C&C address receives the frequency command.
    void setHardwareRxMapping(int receiverIndex, int hardwareRx);

    // --- Diversity ---
    void enableDiversity(int mainReceiverIndex, int subReceiverIndex);
    void disableDiversity(int mainReceiverIndex);

    // --- PureSignal ---
    // Designate a receiver as the PureSignal feedback receiver.
    void setPureSignalFeedbackReceiver(int receiverIndex);
    int pureSignalFeedbackReceiver() const { return m_psFeedbackRx; }

    // --- I/Q Data Routing ---
    // Called by RadioConnection when I/Q data arrives for a hardware RX.
    // Routes to the correct WDSP channel.
    void feedIqData(int hardwareRx, const QVector<float>& samples);

signals:
    void activeReceiverCountChanged(int count);
    void receiverCreated(int receiverIndex);
    void receiverDestroyed(int receiverIndex);
    void receiverActivated(int receiverIndex);
    void receiverDeactivated(int receiverIndex);
    void receiverFrequencyChanged(int receiverIndex, quint64 frequencyHz);

    // Request RadioConnection to update hardware state.
    // RadioModel wires these to RadioConnection slots.
    void hardwareReceiverCountChanged(int count);
    void hardwareFrequencyChanged(int hardwareRx, quint64 frequencyHz);

    // I/Q data routed to the appropriate WDSP channel.
    // AudioEngine/WdspEngine connects to this.
    void iqDataForChannel(int wdspChannel, const QVector<float>& samples);

private:
    int m_maxReceivers{4};
    int m_nextWdspChannel{0};
    int m_psFeedbackRx{-1};

    QMap<int, ReceiverConfig> m_receivers;  // receiverIndex -> config

    // Mapping from hardware RX to logical receiver(s)
    QMap<int, QList<int>> m_hwToLogical;

    static const ReceiverConfig kInvalidConfig;
};

} // namespace NereusSDR
```

### Receiver Count vs Sample Rate Tradeoff

The number of active receivers affects available bandwidth per receiver
because all receivers share the EP6 data stream. More receivers means fewer
samples per receiver per frame, which can limit the maximum useful sample
rate:

| Active RX | Max Practical Sample Rate | Notes |
|-----------|--------------------------|-------|
| 1 | 384 kHz | Full bandwidth |
| 2 | 384 kHz | Full bandwidth |
| 3 | 192 kHz | Reduced samples/frame |
| 4 | 192 kHz | |
| 5-7 | 48 kHz | Minimum useful bandwidth |

The ReceiverManager enforces these constraints and emits warnings when
configurations approach bandwidth limits.

---

## 10. Thread Architecture

Following AetherSDR's pattern, NereusSDR uses thread isolation with
auto-queued signals for all cross-thread communication. No shared mutexes
in the data path.

### Thread Map

```
┌──────────────────────────────────────────────────────────────────────────┐
│ MAIN THREAD                                                              │
│                                                                          │
│  ┌────────────┐   ┌──────────────┐   ┌────────────────┐                 │
│  │ MainWindow │──►│  RadioModel  │──►│ ReceiverManager│                 │
│  └────────────┘   └──────┬───────┘   └────────────────┘                 │
│                          │                                               │
│         ┌────────────────┼────────────────┐                              │
│         ▼                ▼                ▼                               │
│  ┌────────────┐  ┌──────────────┐  ┌──────────────┐                     │
│  │ SliceModel │  │TransmitModel │  │  MeterModel  │                     │
│  └────────────┘  └──────────────┘  └──────────────┘                     │
└──────────────────────────────────────────────────────────────────────────┘
         │ signals (auto-queued)          ▲ signals (auto-queued)
         ▼                                │
┌──────────────────────────────────────────────────────────────────────────┐
│ CONNECTION THREAD                                                        │
│                                                                          │
│  ┌────────────────────────┐   ┌──────────────────┐                      │
│  │ P1RadioConnection      │   │ MetisFrameParser │                      │
│  │ (or P2RadioConnection) │──►│ (P1 only)        │                      │
│  └────────────────────────┘   └──────────────────┘                      │
│           │                                                              │
│     QUdpSocket (I/O — single socket for P1 and P2)                       │
└──────────────────────────────────────────────────────────────────────────┘
         │ iqDataReceived signal (auto-queued)
         ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ AUDIO THREAD                                                             │
│                                                                          │
│  ┌─────────────┐   ┌────────────┐   ┌──────────────┐                   │
│  │ AudioEngine │──►│ WdspEngine │──►│ QAudioSink   │                   │
│  │ (I/Q route) │   │ (per-chan)  │   │ (PCM output) │                   │
│  └─────────────┘   └────────────┘   └──────────────┘                   │
└──────────────────────────────────────────────────────────────────────────┘
         │ spectrumData signal (auto-queued)
         ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ SPECTRUM THREAD                                                          │
│                                                                          │
│  ┌──────────────┐   ┌──────────────────┐                                │
│  │ FFT Engine   │──►│ Waterfall Buffer │                                │
│  │ (FFTW3)      │   │ (ring buffer)    │                                │
│  └──────────────┘   └──────────────────┘                                │
└──────────────────────────────────────────────────────────────────────────┘
```

### Signal Declarations and Cross-Thread Routing

All signals that cross thread boundaries use Qt's auto-connection. Because
the sender and receiver live on different threads, Qt automatically queues
the signal invocation in the receiver's event loop.

**Connection thread to Main thread:**

```cpp
// In P1RadioConnection / P2RadioConnection:
void iqDataReceived(int receiverIndex, const QVector<float>& samples);
void connectionStateChanged(NereusSDR::ConnectionState state);
void meterDataReceived(float fwd, float rev, float voltage, float current, float temp);
void adcOverflow(int adc);
void errorOccurred(const QString& message);

// In RadioDiscovery:
void radioDiscovered(const NereusSDR::RadioInfo& info);
void radioUpdated(const NereusSDR::RadioInfo& info);
void radioLost(const QString& macAddress);
```

**Main thread to Connection thread:**

```cpp
// RadioModel invokes these on the connection via QMetaObject::invokeMethod
// or direct signal-slot connections (auto-queued because threads differ):
void setReceiverFrequency(int receiverIndex, quint64 frequencyHz);
void setTxFrequency(quint64 frequencyHz);
void setActiveReceiverCount(int count);
void setSampleRate(int sampleRate);
void setMox(bool enabled);
```

**Connection thread to Audio thread:**

```cpp
// I/Q data flows from connection to audio engine:
// RadioConnection::iqDataReceived → ReceiverManager::feedIqData →
//   emit iqDataForChannel(wdspChannel, samples) → AudioEngine (auto-queued)
```

### Worker Thread Setup Pattern

Following AetherSDR's `moveToThread` + `init()` pattern:

```cpp
// In RadioModel constructor (main thread):
void RadioModel::setupConnectionThread() {
    m_connThread = new QThread(this);
    m_connThread->setObjectName("ConnectionThread");

    // Create the connection object — do NOT parent it to this
    m_connection = RadioConnection::create(info);

    // Move to worker thread
    m_connection->moveToThread(m_connThread);

    // Wire signals BEFORE starting the thread (connections are thread-safe)
    connect(m_connection.get(), &RadioConnection::connectionStateChanged,
            this, &RadioModel::onConnectionStateChanged);
    connect(m_connection.get(), &RadioConnection::iqDataReceived,
            this, &RadioModel::onIqDataReceived);
    connect(m_connection.get(), &RadioConnection::meterDataReceived,
            this, &RadioModel::onMeterDataReceived);
    connect(m_connection.get(), &RadioConnection::errorOccurred,
            this, &RadioModel::onConnectionError);

    // Start thread, then init the connection on the worker thread
    m_connThread->start();
    QMetaObject::invokeMethod(m_connection.get(), "init", Qt::QueuedConnection);
}
```

### Data Rate Considerations

At 384 kHz sample rate with 1 receiver, the radio sends:
- 384,000 I/Q pairs/second * 6 bytes/pair = 2.304 MB/s raw I/Q
- Packed into 1032-byte frames: ~2,300 frames/second
- Each frame contains 2 USB frames * 50 samples = 100 I/Q pairs

The connection thread must process frames at this rate without blocking.
The `iqDataReceived` signal carries `QVector<float>` which is implicitly
shared (copy-on-write), so the auto-queued signal copy is efficient.

---

## 11. Class Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              NereusSDR                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────┐          ┌──────────────────────┐                  │
│  │  RadioDiscovery  │         │     RadioModel        │                  │
│  │  (QObject)       │────────►│     (QObject)         │                  │
│  │                  │ signal: │                       │                  │
│  │ +startDiscovery()│ radio   │ +connectToRadio()     │                  │
│  │ +stopDiscovery() │ Discov- │ +disconnect()         │                  │
│  │ +discoveredRadios│ ered()  │ +sendCommand()        │                  │
│  └─────────────────┘         │                       │                  │
│                               │ owns:                │                  │
│                               │ ┌──────────────────┐ │                  │
│                               │ │ ReceiverManager  │ │                  │
│                               │ │ (QObject)        │ │                  │
│                               │ │                  │ │                  │
│                               │ │ +createReceiver()│ │                  │
│                               │ │ +destroyReceiver │ │                  │
│                               │ │ +activateReceiver│ │                  │
│                               │ │ +feedIqData()    │ │                  │
│                               │ └──────────────────┘ │                  │
│                               └──────────┬───────────┘                  │
│                                          │ owns (on worker thread)      │
│                           ┌──────────────┴───────────────┐              │
│                           │                              │              │
│                           ▼                              ▼              │
│               ┌──────────────────────┐    ┌──────────────────────┐      │
│               │  RadioConnection     │    │   AudioEngine        │      │
│               │  (QObject, abstract) │    │   (QObject)          │      │
│               │                      │    │                      │      │
│               │ +init()              │    │ +processIq()         │      │
│               │ +connectToRadio()    │    │ +getAudio()          │      │
│               │ +disconnect()        │    └──────────────────────┘      │
│               │ +setReceiver         │                                  │
│               │   Frequency()        │                                  │
│               │ +setSampleRate()     │                                  │
│               │ +setMox()            │                                  │
│               └──────────┬───────────┘                                  │
│                          │                                              │
│            ┌─────────────┴─────────────┐                                │
│            │                           │                                │
│            ▼                           ▼                                │
│ ┌────────────────────┐    ┌────────────────────┐                        │
│ │ P1RadioConnection  │    │ P2RadioConnection  │                        │
│ │ (RadioConnection)  │    │ (RadioConnection)  │                        │
│ │                    │    │                    │                        │
│ │ +buildMetisFrame() │    │ +sendCommand()     │                        │
│ │ +frequencyTo       │    │ +encodeFrequency() │                        │
│ │  PhaseWord()       │    │                    │                        │
│ │                    │    │ QUdpSocket (single │                        │
│ │ QUdpSocket         │    │  socket, all I/O)  │  ← CORRECTED 2026-04-10│
│ │                    │    └────────────────────┘                        │
│ │ ┌────────────────┐ │                                                  │
│ │ │MetisFrameParser│ │                                                  │
│ │ │ (value class)  │ │                                                  │
│ │ │                │ │                                                  │
│ │ │ +parseFrame()  │ │                                                  │
│ │ │ +samplesFor    │ │                                                  │
│ │ │  Receiver()    │ │                                                  │
│ │ │ +feedback()    │ │                                                  │
│ │ └────────────────┘ │                                                  │
│ └────────────────────┘                                                  │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Ownership and Lifetime

```
RadioModel (main thread)
  ├── owns RadioDiscovery (main thread)
  ├── owns ReceiverManager (main thread, value member)
  ├── owns QThread m_connThread
  │     └── RadioConnection lives here (moved via moveToThread)
  │           └── owns MetisFrameParser (P1, value member)
  ├── owns QThread m_audioThread
  │     └── AudioEngine lives here
  │           └── owns WdspEngine (per-channel instances)
  └── owns QThread m_spectrumThread
        └── FFT engine lives here
```

---

## 12. Integration with RadioModel

`RadioModel` is the central state hub on the main thread. It owns the
connection, receiver manager, and all sub-models. It wires signals between
components.

### Key Wiring

```cpp
// RadioModel wiring (simplified)
void RadioModel::wireConnection() {
    // Discovery -> RadioModel
    connect(m_discovery, &RadioDiscovery::radioDiscovered,
            this, &RadioModel::onRadioDiscovered);

    // Connection -> RadioModel (auto-queued: connection thread -> main thread)
    connect(m_connection.get(), &RadioConnection::connectionStateChanged,
            this, &RadioModel::onConnectionStateChanged);
    connect(m_connection.get(), &RadioConnection::iqDataReceived,
            this, &RadioModel::onIqDataReceived);
    connect(m_connection.get(), &RadioConnection::meterDataReceived,
            m_meterModel, &MeterModel::updateFromHardware);

    // ReceiverManager -> Connection (auto-queued: main thread -> connection thread)
    connect(m_receiverManager, &ReceiverManager::hardwareReceiverCountChanged,
            m_connection.get(), &RadioConnection::setActiveReceiverCount);
    connect(m_receiverManager, &ReceiverManager::hardwareFrequencyChanged,
            m_connection.get(), &RadioConnection::setReceiverFrequency);

    // ReceiverManager -> AudioEngine (auto-queued: main thread -> audio thread)
    connect(m_receiverManager, &ReceiverManager::iqDataForChannel,
            m_audioEngine, &AudioEngine::processIqData);
}
```

### Command Flow Example: User Tunes VFO

```
User rotates VFO knob
  → VfoWidget::frequencyChanged(14225000)
  → SliceModel::setFrequency(14225000)
  → emit frequencyChanged(14225000)
  → RadioModel::onSliceFrequencyChanged(sliceIndex, 14225000)
  → ReceiverManager::setReceiverFrequency(rxIndex, 14225000)
  → emit hardwareFrequencyChanged(hwRx, 14225000)
  → [auto-queued to connection thread]
  → P1RadioConnection::setReceiverFrequency(hwRx, 14225000)
  → Update C&C register 0x02+hwRx with phase word
  → Phase word sent in next EP2 frame (~15ms latency)
```

### I/Q Data Flow Example: Receiving Signal

```
Radio streams EP6 UDP datagrams
  → P1RadioConnection::onUdpReadyRead()
  → MetisFrameParser::parseFrame(1032 bytes)
  → For each receiver: emit iqDataReceived(rxIndex, samples)
  → [auto-queued to main thread]
  → RadioModel::onIqDataReceived(rxIndex, samples)
  → ReceiverManager::feedIqData(hwRx, samples)
  → emit iqDataForChannel(wdspChannel, samples)
  → [auto-queued to audio thread]
  → AudioEngine::processIqData(wdspChannel, samples)
  → WdspEngine::fexchange(channel, inBuf, outBuf)
  → Demodulated audio → QAudioSink
  → FFT data → emit spectrumReady() → SpectrumWidget
```

---

## 13. Error Handling and Reconnection

### Error Categories

| Error | Source | Recovery |
|-------|--------|----------|
| No discovery response | UDP broadcast timeout | Retry discovery every 2s |
| Radio in use | Discovery byte 2 = 0x03 | Report to user, do not connect |
| UDP send failure | Socket error | Reconnect after 3s |
| No EP6 data received | Watchdog timeout (5s) | Reconnect after 3s |
| Sequence gap in EP6 | Dropped UDP packets | Log warning, continue |
| TCP disconnect (P2) | Socket error | Reconnect after 3s |
| Invalid frame data | Parse failure | Log, skip frame, continue |

### Reconnection Logic

```cpp
void RadioConnection::onWatchdogTimeout() {
    if (m_intentionalDisconnect) { return; }

    qCWarning(lcRadio) << "Watchdog: no data received for 5 seconds";
    setState(ConnectionState::Error);

    // Start reconnect timer (3s delay)
    m_reconnectTimer->start(3000);
}

void RadioConnection::onReconnectTimeout() {
    if (m_intentionalDisconnect) { return; }

    qCInfo(lcRadio) << "Attempting reconnection to" << m_radioInfo.displayName();
    connectToRadio(m_radioInfo);
}
```

### Intentional vs Unintentional Disconnect

- **User clicks Disconnect:** `m_intentionalDisconnect = true`, no reconnect.
- **Radio powers off:** Watchdog fires, auto-reconnect attempts.
- **Network glitch:** Brief data gap, watchdog fires, auto-reconnect.
- **Radio rebooted:** Discovery will re-find it, auto-reconnect succeeds.

### Sequence Gap Handling (P1)

P1 Metis frames have a 32-bit sequence number. Gaps indicate dropped UDP
packets. The MetisFrameParser tracks gaps for logging and statistics but
does not attempt retransmission (UDP is fire-and-forget).

```cpp
bool MetisFrameParser::parseFrame(const QByteArray& frame) {
    // ... header validation ...

    quint32 seq = (raw[3] << 24) | (raw[4] << 16) | (raw[5] << 8) | raw[6];

    if (!m_firstFrame) {
        quint32 expected = m_lastSequence + 1;
        if (seq != expected) {
            m_sequenceGap = true;
            m_droppedFrames += static_cast<int>(seq - expected);
            qCDebug(lcRadio) << "Sequence gap: expected" << expected
                             << "got" << seq
                             << "(" << (seq - expected) << "frames lost)";
        } else {
            m_sequenceGap = false;
        }
    }
    m_firstFrame = false;
    m_lastSequence = seq;

    // ... parse USB frames ...
}
```

---

## 14. Comparison with AetherSDR Patterns

### What We Adopt

| Pattern | AetherSDR | NereusSDR Adaptation |
|---------|-----------|---------------------|
| Worker thread + init() | RadioConnection on m_connThread | Same: RadioConnection::init() after moveToThread() |
| Auto-queued signals | All cross-thread signals | Same: iqDataReceived, connectionStateChanged, etc. |
| RadioModel central hub | Owns connection + sub-models | Same: owns RadioConnection + ReceiverManager |
| State enum | ConnectionState (4 states) | Same enum, same state machine |
| Factory pattern | N/A (single protocol) | RadioConnection::create(info) selects P1 or P2 |
| Heartbeat/watchdog | 30s heartbeat ping | EP2 send timer (63 Hz) + 5s watchdog |

### What We Replace

| AetherSDR | NereusSDR | Reason |
|-----------|-----------|--------|
| TCP 4992 command/response | UDP 1024 Metis frames (P1) / UDP multi-port (P2) | Different protocol |
| VITA-49 UDP parsing | MetisFrameParser (1032-byte frames) | Different framing |
| Radio-computed FFT/WF | Client-side FFTW3 from raw I/Q | Radio is just ADC |
| Radio-authoritative slices | Client-managed ReceiverManager | Radio has no slice concept |
| `sub slice all` subscriptions | C&C register rotation | No subscription model |
| Sequence numbers in commands | Frame sequence in Metis header | Different command model |
| PanadapterStream (VITA-49) | MetisFrameParser + ReceiverManager | Different data path |

### What We Add (No AetherSDR Equivalent)

| New Component | Purpose |
|---------------|---------|
| MetisFrameParser | Binary parsing of 1032-byte P1 Metis frames |
| C&C register management | Bidirectional hardware control via frame headers |
| Phase word calculation | P1 frequency encoding (not needed for SmartSDR) |
| ReceiverManager | Client-side receiver multiplexing (radio has no concept) |
| 24-bit sample decoding | P1 sends 24-bit big-endian I/Q (not VITA-49 float) |

---

## 15. Implementation Plan

### Phase 2A Deliverables (This Document)

- [x] RadioInfo struct with BoardType enum and capability derivation
- [x] RadioDiscovery with P1/P2 response parsing and staleness detection
- [x] RadioConnection abstract base with protocol-agnostic interface
- [x] P1RadioConnection with MetisFrameParser and C&C management
- [x] P2RadioConnection with UDP multi-port architecture
- [x] ReceiverManager for client-side receiver lifecycle
- [x] Thread architecture and signal routing
- [x] Class diagram and integration wiring

### Phase 2B Implementation Order

1. **MetisFrameParser** — Start here. Pure logic, no I/O, fully unit-testable.
   Test with captured Metis frame dumps.
2. **RadioDiscovery updates** — Expand existing stub with P1/P2 parsing.
3. **P1RadioConnection** — Build on MetisFrameParser. Test with real radio.
4. **ReceiverManager** — Client-side receiver tracking and WDSP channel mapping.
5. **RadioModel wiring** — Connect all components with signals/slots.
6. **P2RadioConnection** — Implement after P1 is stable (fewer P2 radios
   available for testing).

### Testing Strategy

- **MetisFrameParser:** Unit tests with captured frame data (hex dumps from
  Wireshark captures of Thetis sessions).
- **RadioDiscovery:** Integration test on local network with real radio.
- **P1RadioConnection:** End-to-end test: discover, connect, receive I/Q,
  verify frequency tuning.
- **ReceiverManager:** Unit tests for receiver lifecycle and mapping.
- **Full pipeline:** Discovery -> Connect -> I/Q -> WDSP -> Audio output.

---

## Appendix A: Metis Frame Quick Reference

### EP2 Frame (PC to Radio)

```
Offset  Size  Content
0       2     0xEF 0xFE (sync)
2       1     0x02 (EP2 endpoint)
3       4     Sequence number (big-endian)
7       1     0x00 (reserved)
8       3     0x7F 0x7F 0x7F (USB sync, frame 1)
11      5     C&C bytes C0-C4 (frame 1)
16      504   TX I/Q samples or zeros (frame 1)
520     3     0x7F 0x7F 0x7F (USB sync, frame 2)
523     5     C&C bytes C0-C4 (frame 2)
528     504   TX I/Q samples or zeros (frame 2)
```

### EP6 Frame (Radio to PC)

```
Offset  Size  Content
0       2     0xEF 0xFE (sync)
2       1     0x06 (EP6 endpoint)
3       4     Sequence number (big-endian)
7       1     0x00 (reserved)
8       3     0x7F 0x7F 0x7F (USB sync, frame 1)
11      5     C&C feedback bytes C0-C4 (frame 1)
16      504   RX I/Q + Mic samples (frame 1)
520     3     0x7F 0x7F 0x7F (USB sync, frame 2)
523     5     C&C feedback bytes C0-C4 (frame 2)
528     504   RX I/Q + Mic samples (frame 2)
```

### Discovery Packet

```
Offset  Size  Content
0       2     0xEF 0xFE (sync)
2       1     0x02 (discovery request)
3       60    0x00 (padding)
```

### Discovery Response (P1)

```
Offset  Size  Content
0       2     0xEF 0xFE (sync)
2       1     0x02 (available) or 0x03 (in use)
3       6     MAC address
9       1     Firmware version
10      1     Board type (see BoardType enum)
```

---

## Appendix B: Sample Rate and Receiver Count Encoding

### C&C Address 0x00, C1 Byte

```
Bits [6:3] — Sample rate:
  0000 = 48,000 Hz
  0001 = 96,000 Hz
  0010 = 192,000 Hz
  0011 = 384,000 Hz

Bits [2:0] — Number of receivers minus one:
  000 = 1 receiver
  001 = 2 receivers
  010 = 3 receivers
  011 = 4 receivers
  100 = 5 receivers
  101 = 6 receivers
  110 = 7 receivers
```

### Encoding Example

```cpp
quint8 encodeSampleRateAndReceivers(int sampleRate, int numReceivers) {
    quint8 byte = 0;

    // Sample rate in bits [6:3]
    switch (sampleRate) {
        case 48000:  byte |= (0b0000 << 3); break;
        case 96000:  byte |= (0b0001 << 3); break;
        case 192000: byte |= (0b0010 << 3); break;
        case 384000: byte |= (0b0011 << 3); break;
        default:     byte |= (0b0000 << 3); break;  // Default 48k
    }

    // Receiver count - 1 in bits [2:0]
    byte |= (static_cast<quint8>(numReceivers - 1) & 0x07);

    return byte;
}
```

---

## Appendix C: Frequency Phase Word Examples

| Frequency | Hz | Phase Word | Hex |
|-----------|------|------------|-----|
| 1.8 MHz | 1,800,000 | 63,015,304 | 0x03C14508 |
| 3.5 MHz | 3,500,000 | 122,529,478 | 0x074E68B6 |
| 7.0 MHz | 7,000,000 | 245,058,956 | 0x0E9CD16C |
| 14.0 MHz | 14,000,000 | 490,117,912 | 0x1D39A2D8 |
| 14.2 MHz | 14,200,000 | 497,117,468 | 0x1DA4750C |
| 21.0 MHz | 21,000,000 | 735,176,868 | 0x2BD67424 |
| 28.0 MHz | 28,000,000 | 980,235,824 | 0x3A734530 |
| 50.0 MHz | 50,000,000 | 1,750,421,114 | 0x684DD9EA |
| 144.0 MHz | 144,000,000 | 5,041,212,809 | 0x2C8B0A89 (wraps) |
