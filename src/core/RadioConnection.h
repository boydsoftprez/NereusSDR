#pragma once

// no-port-check: NereusSDR-original abstract base class. Inline doc comments
// reference Thetis source filenames (console.cs / networkproto1.c / network.c)
// only as pointers to where ported logic lives in the concrete subclasses
// (P1RadioConnection.cpp, P2RadioConnection.cpp); no upstream code is
// reproduced in this header.

#include "ConnectionState.h"
#include "RadioDiscovery.h"
#include "HardwareProfile.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QVector>

#include <atomic>
#include <memory>

namespace NereusSDR {

// Structured failure taxonomy for the initial connect attempt — design §4.1.
// Emitted by connectFailed() when connectToRadio() cannot reach Connected state.
// The UI (TitleBar, ConnectionPanel) maps these to human-readable messages.
enum class ConnectFailure : int {
    Unreachable     = 0,  // OS reports "destination unreachable" (ICMP unreach)
    Timeout         = 1,  // No reply / no first frame within the connect budget
    MalformedReply  = 2,  // Response received but the frame parser rejected it
    ProtocolMismatch = 3, // Reply valid but P1/P2 version mismatch
    IncompatibleBoard = 4 // Reply valid but boardType is not supported
};

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

    // Wire-protocol identifier — 1 = OpenHPSDR Protocol 1 (Metis-framed UDP
    // on port 1024), 2 = OpenHPSDR Protocol 2 (multi-port UDP, higher rates).
    //
    // Mirrors Thetis NetworkIO.CurrentRadioProtocol and exists primarily so
    // protocol-conditional WDSP TXA stages (e.g. CFIR per
    // ChannelMaster/cmaster.cs:525-533 [v2.10.3.14]) can be gated without
    // dragging the concrete subclass header into the call site.
    //
    // Non-pure with a P1 default so test mocks compile unchanged; P2 must
    // override.  Override in P1RadioConnection (returns 1) is provided for
    // explicit symmetry with P2RadioConnection (returns 2).
    virtual int protocolVersion() const { return 1; }

    // Rolling-window throughput accessors. **Returns Mbps** (not bytes/sec
    // — the "ByteRate" name predates the implementation, kept for ABI
    // stability). Used by ConnectionSegment ▲▼ readouts and the network
    // diagnostics dialog. Bugs land easy here: 2026-04-30 the dialog
    // applied a second `* 8 / 1e6` conversion and TX/RX always read 0.
    // If you're computing throughput, use these values directly as Mbps.
    double txByteRate(int windowMs) const;
    double rxByteRate(int windowMs) const;

    // Hooks the protocol implementations call on each successful packet.
    // Public so subclasses (P1/P2) and tests can drive the counter.
    void recordBytesSent(qint64 n);
    void recordBytesReceived(qint64 n);

    // Ping RTT measurement via existing C&C round-trip.
    // Call notePingSent() just before emitting an outbound command and
    // notePingReceived() when the corresponding inbound status arrives.
    // A single outstanding exchange is tracked; the first valid pair
    // within 5 s emits pingRttMeasured(int rttMs). Duplicate receives
    // and receives-without-sends are silently ignored.
    // Drives the ConnectionSegment "X ms" latency readout (sub-PR-2).
    void notePingSent();
    void notePingReceived();

    // Voltage signal handlers — called by P1/P2 after extracting raw ADC counts.
    // Apply per-board scaling and emit supplyVoltsChanged / userAdc0Changed
    // when the value changes by more than 50 mV (identical-raw suppression).
    //
    // handleSupplyRaw: Hermes DC-volts formula from Thetis
    //   console.cs computeHermesDCVoltage() [v2.10.3.13].
    //   Applies to all radios (supply_volts / AIN6 / P1 case 0x18 / P2 bytes 45-46).
    //
    // handleUserAdc0Raw: MKII PA-volts formula from Thetis
    //   console.cs computeMKIIPaVoltage() [v2.10.3.13].
    //   Applies to ORIONMKII / ANAN8000D / ANAN7000D / ANAN-G2 family only
    //   (user_adc0 / AIN3 / P1 case 0x10 / P2 bytes 53-54). Callers
    //   are responsible for gating by board type before calling.
    void handleSupplyRaw(quint16 raw);
    void handleUserAdc0Raw(quint16 raw);

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

    // ── Mic-jack hardware bits (3M-1b Phase G) ────────────────────────────

    /// Hardware mic-jack 20 dB boost preamp.
    ///
    /// P1 source: Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
    ///   case 10 (C0=0x12) C2 byte: (prn->mic.mic_boost & 1) → bit 0 (0x01)
    ///
    /// P2 source: deskhpsdr src/new_protocol.c:1484-1486 [@120188f]
    ///   if (mic_boost) { transmit_specific_buffer[50] |= 0x02; }
    ///   (bit 1, mask 0x02 — different bit from P1)
    ///
    /// Polarity: 1 = boost on (no inversion).
    ///
    /// HL2 has no mic jack; the P1 implementation still writes the bit
    /// (firmware ignores it).
    virtual void setMicBoost(bool on) = 0;

    /// Hardware mic-jack line-in path (replaces front-panel mic with line input).
    ///
    /// P1 source: Thetis ChannelMaster/networkproto1.c:581 [v2.10.3.13]
    ///   case 10 (C0=0x12) C2 byte: ((prn->mic.line_in & 1) << 1) → bit 1 (0x02)
    ///
    /// P2 source: deskhpsdr src/new_protocol.c:1480-1482 [@120188f]
    ///   if (mic_linein) { transmit_specific_buffer[50] |= 0x01; }
    ///   (bit 0, mask 0x01 — different bit position from P1)
    ///
    /// Polarity: 1 = line in active (no inversion).
    ///
    /// HL2 has no mic jack; the P1 implementation still writes the bit
    /// (firmware ignores it).
    virtual void setLineIn(bool on) = 0;

    /// Hardware mic-jack Tip/Ring polarity selection.
    ///
    /// NereusSDR parameter convention: `tipHot = true` means Tip carries the
    /// mic signal (the intuitive "tip is mic" meaning).
    ///
    /// POLARITY INVERSION AT THE WIRE LAYER — both upstream sources define
    /// the field as "1 = Tip is BIAS/PTT" (i.e. Tip is NOT the mic):
    ///   Thetis field name: mic_trs  ("TRS" = Tip-Ring-Sleeve; 1 = tip is ring)
    ///   deskhpsdr field: mic_ptt_tip_bias_ring (1 = tip carries BIAS/PTT, not mic)
    /// Therefore both P1 and P2 write `!tipHot` to the wire bit.
    ///
    /// P1 source: Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
    ///   case 11 (C0=0x14) C1 byte: ((prn->mic.mic_trs & 1) << 4) → bit 4 (0x10)
    ///   (first touch of case 11 / bank 11)
    ///
    /// P2 source: deskhpsdr src/new_protocol.c:1492-1494 [@120188f]
    ///   if (mic_ptt_tip_bias_ring) { transmit_specific_buffer[50] |= 0x08; }
    ///   (bit 3, mask 0x08 — different bit position from P1)
    ///
    /// HL2 has no mic jack; the P1 implementation still writes the bit
    /// (firmware ignores it).
    virtual void setMicTipRing(bool tipHot) = 0;

    /// Hardware mic-jack phantom power (bias) enable.
    ///
    /// Polarity: 1 = bias on (no inversion — parameter maps directly to wire bit).
    ///
    /// P1 source: Thetis ChannelMaster/networkproto1.c:597 [v2.10.3.13]
    ///   case 11 (C0=0x14) C1 byte: ((prn->mic.mic_bias & 1) << 5) → bit 5 (0x20)
    ///   (same bank 11 / case 11 as G.3 mic_trs — both OR into C1)
    ///
    /// P2 source: deskhpsdr src/new_protocol.c:1496-1498 [@120188f]
    ///   if (mic_bias_enabled) { transmit_specific_buffer[50] |= 0x10; }
    ///   (bit 4, mask 0x10 in byte 50 — different bit position from P1)
    ///
    /// HL2 has no mic jack; the P1 implementation still writes the bit
    /// (firmware ignores it).
    virtual void setMicBias(bool on) = 0;

    /// Set mic line-in gain (0-31, 5 bits).  Maps to Thetis prn->mic.line_in_gain.
    /// Source: Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]
    ///   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    /// P1: bank 11 C2 low 5 bits.
    /// P2: byte 51 of CmdHighPriority (already wired pre-this-virtual via the
    ///     P2 mic struct; the P2 override bridges this virtual to that path).
    /// Default 0 = no line-in attenuation.
    virtual void setLineInGain(int gain) = 0;

    /// Set user digital outputs (0-15, low 4 bits).  Maps to Thetis prn->user_dig_out.
    /// Source: Thetis ChannelMaster/networkproto1.c:601 [v2.10.3.13]
    ///   C3 = prn->user_dig_out & 0b00001111;
    /// P1: bank 11 C3 low 4 bits.  Drives the 4 user-controllable digital pins
    ///     on the Penny/Hermes Ctrl accessory header.
    /// P2: NO EQUIVALENT — user_dig_out is a P1/Penny-only feature with no
    ///     corresponding byte in CmdHighPriority.  The P2 override stores into
    ///     base m_userDigOut for symmetric API only; it does NOT emit anything
    ///     on the wire.
    /// Default 0 = all 4 user-dig-out pins low.
    virtual void setUserDigOut(quint8 dig) = 0;

    /// Set PureSignal feedback DDC routing-active flag.  Maps to Thetis prn->puresignal_run.
    /// Source: Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]
    ///   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    /// P1: bank 11 C2 bit 6 (mask 0x40).
    /// P2: NO DIRECT WIRE-BIT — PureSignal feedback DDC routing on P2 is
    ///     handled by the feedback DDC plumbing planned for 3M-4.  The P2
    ///     override stores into base m_puresignalRun for symmetric API only;
    ///     it does NOT emit anything on the wire until 3M-4.
    ///
    /// Semantic: tracks whether PureSignal feedback DDC routing is currently
    /// *active*.  Distinct from BoardCapabilities.hasPureSignal (capability)
    /// and from TransmitModel::puresignalEnabled (user toggle).  Until 3M-4
    /// lights up the actual feedback DDC routing, the PureSignalApplet
    /// "Enable" toggle is the proxy that drives this — wiring done in
    /// Task 2.5 of the P1 full-parity epic, not here.
    /// Default false = PureSignal feedback DDC NOT routing.
    virtual void setPuresignalRun(bool run) = 0;

    /// Hardware mic-jack PTT disable flag (Orion/ANAN front-panel PTT).
    ///
    /// Parameter and wire convention match Thetis NetworkIO.SetMicPTT exactly:
    ///   disabled = true  → PTT disabled at firmware → wire bit = 1
    ///   disabled = false → PTT enabled  at firmware → wire bit = 0  (default)
    ///
    /// Source: Thetis console.cs:19757-19766 [v2.10.3.13+501e3f51]
    ///   private bool mic_ptt_disabled = false;        // default PTT enabled
    ///   public bool MicPTTDisabled {
    ///       set {
    ///           mic_ptt_disabled = value;
    ///           NetworkIO.SetMicPTT(Convert.ToInt32(value));
    ///       }
    ///   }
    ///
    /// P1 wire field: Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13+501e3f51]
    ///   case 11 (C0=0x14) C1 byte: ((prn->mic.mic_ptt & 1) << 6) → bit 6 (0x40), DIRECT
    /// P2 wire field: deskhpsdr src/new_protocol.c:1488-1490 [@120188f]
    ///   if (mic_ptt_enabled == 0) { transmit_specific_buffer[50] |= 0x04; }  // byte 50 bit 2
    /// Both upstreams use the same direct-to-disabled wire convention.
    ///
    /// HL2 has no front-panel PTT jack; the P1 implementation still writes the
    /// bit (firmware ignores it).
    virtual void setMicPTTDisabled(bool disabled) = 0;

    /// Hardware mic-jack XLR input select (Saturn G2 / ANAN-G2 only).
    ///
    /// P2-ONLY FEATURE. Saturn G2 hardware ships with an XLR balanced mic
    /// input; this bit selects between XLR (balanced) and TRS (unbalanced).
    ///
    /// P2 source: deskhpsdr src/new_protocol.c:1500-1502 [@120188f]
    ///   if (mic_input_xlr) { transmit_specific_buffer[50] |= 0x20; }
    ///   Bit 5 (mask 0x20) of byte 50. Polarity: 1 = XLR jack selected.
    ///   No inversion — parameter maps directly to wire bit.
    ///
    /// P1 implementation is STORAGE-ONLY.
    ///   P1 hardware has no XLR jack. The setter stores m_micXlr for
    ///   cross-board API consistency but does NOT emit any wire bytes.
    ///   P1 case-10 and case-11 C&C bytes are unchanged regardless of value.
    ///   Comment: "Saturn G2 P2-only feature; P1 hardware has no XLR jack."
    ///
    /// Default true — Saturn G2 ships with XLR-enabled configuration.
    virtual void setMicXlr(bool xlrJack) = 0;

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
    // --- Telemetry ---
    // Emitted when a C&C round-trip completes. rttMs is the elapsed time
    // in milliseconds between notePingSent() and notePingReceived().
    // Drives the ConnectionSegment "X ms" latency readout (sub-PR-2).
    void pingRttMeasured(int rttMs);

    // PSU supply voltage (V) from supply_volts (P1 AIN6 / P2 bytes 45-46).
    // Converted via Hermes DC-volts formula (console.cs computeHermesDCVoltage()
    // [v2.10.3.13]). Emitted at most once per 50 mV change.
    // Drives the redesigned right-side strip "PSU X.XV" metric (sub-PR-2).
    void supplyVoltsChanged(float volts);

    // PA drain voltage (V) from user_adc0 (P1 AIN3 / P2 bytes 53-54).
    // ORIONMKII / ANAN-8000D / ANAN-7000DLE / ANAN-G2 family only.
    // Converted via MKII PA-volts formula (console.cs computeMKIIPaVoltage()
    // [v2.10.3.13]). Emitted at most once per 50 mV change.
    void userAdc0Changed(float volts);

    // --- State ---
    void connectionStateChanged(NereusSDR::ConnectionState state);
    void errorOccurred(NereusSDR::RadioConnectionError code, const QString& message);

    // Emitted once per valid ep6 (P1) or DDC I/Q (P2) frame arrival.
    // The TitleBar activity LED throttles this to ≤10 Hz visible refresh —
    // the emitter fires at the full frame rate; throttling is the receiver's job.
    // Design doc §4.1.
    void frameReceived();

    // Emitted when connectToRadio() fails to reach the Connected state.
    // reason carries a typed failure code; detail is a plain-English
    // explanation suitable for display in the ConnectionPanel / TitleBar.
    // Fully-qualified type names required for Qt MOC queued-connection support.
    // Design doc §4.1.
    void connectFailed(NereusSDR::ConnectFailure reason, QString detail);

    // --- Data ---
    // Emitted for each receiver's I/Q block.
    // hwReceiverIndex: 0-based hardware receiver number.
    // samples: interleaved float I/Q pairs, normalized to [-1.0, 1.0].
    void iqDataReceived(int hwReceiverIndex, const QVector<float>& samples);

    // Emitted when mic samples are available.
    void micDataReceived(const QVector<float>& samples);

    /// Decoded mic-frame audio from the radio's mic-jack input.
    ///
    /// Emitted by P1RadioConnection on EP2 mic-byte zone arrival (Phase G);
    /// emitted by P2RadioConnection on port-1026 packet arrival (Phase G).
    /// Carries float-converted mic samples + frame count.
    ///
    /// **DirectConnection ONLY.** The pointer is only valid during the
    /// synchronous slot dispatch — Qt signals cannot safely queue raw
    /// pointers across threads. Subscribers (RadioMicSource in F.2) MUST
    /// connect with Qt::DirectConnection and immediately copy the samples
    /// into their own lock-free SPSC ring before returning.
    ///
    /// This matches the D.5 sip1OutputReady contract in TxChannel: the same
    /// raw-pointer + DirectConnection pattern is used wherever the audio
    /// thread passes data across a signal boundary.
    ///
    /// Sample format: float32 mono at the radio's mic sample rate
    /// (typically 48 kHz on HPSDR family).
    ///
    /// Plan: 3M-1b F.4. Pre-code review §6.4.
    void micFrameDecoded(const float* samples, int frames);

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

    // Mic-jack PTT input from the radio hardware, decoded from the status
    // frame on every frame receipt.  Emitted unconditionally each frame;
    // MoxController::onMicPttFromRadio() is idempotent on repeated same-state
    // calls (setPttMode(Mic) is a no-op when mode unchanged; setMox(x) only
    // advances the state machine when x differs from m_mox).
    //
    // Subscriber: RadioModel wires this to MoxController::onMicPttFromRadio
    // in setupMoxController() (H.5).  The MoxController drives MOX state when
    // mic-jack PTT changes state.
    //
    // P1 source: C0 byte (ControlBytesIn[0]) bit 0 of each EP6 sub-frame.
    //   Cite: Thetis networkproto1.c:329 [v2.10.3.13]:
    //     prn->ptt_in = ControlBytesIn[0] & 0x1;
    //   + console.cs:25426 [v2.10.3.13]:
    //     bool mic_ptt = (dotdashptt & 0x01) != 0; // PTT from radio
    //
    // P2 source: High-Priority status packet byte 4 (ReadBufp[0]) bit 0.
    //   Cite: Thetis network.c:686-689 [v2.10.3.13]:
    //     //Byte 0 - Bit [0] - PTT  1 = active, 0 = inactive
    //     prn->ptt_in = prn->ReadBufp[0] & 0x1;
    //   (ReadBufp points to raw[4] in NereusSDR — after 4-byte seq prefix.)
    void micPttFromRadio(bool pressed);

    // Radio firmware info received during handshake.
    void firmwareInfoReceived(int version, const QString& details);

private:
    struct ByteSample { qint64 ms; qint64 bytes; };
    mutable QList<ByteSample> m_txSamples;
    mutable QList<ByteSample> m_rxSamples;

    static double rateFromSamples(const QList<ByteSample>& samples, int windowMs);
    static void   pruneSamples(QList<ByteSample>& samples, qint64 nowMs, int windowMs);

    // Ping RTT state. Zero means no outstanding ping.
    qint64 m_pingSentMs{0};

    // Voltage conversion helpers.
    // convertSupplyVolts: 3.3V ADC ref + (4.7+0.82)/0.82 divider — Thetis-faithful
    //                     port of computeHermesDCVoltage (which is itself dead
    //                     code in Thetis — see source-first audit notes near the
    //                     PA volt label declaration in MainWindow.h).
    // convertMkiiPaVolts: 5.0V ADC ref + (22+1)/1.1 divider — Thetis-faithful
    //                     port of convertToVolts (the formula Thetis actually
    //                     uses to display voltage on MkII-class boards).
    static float convertSupplyVolts(quint16 raw);
    static float convertMkiiPaVolts(quint16 raw);

    // Last-emitted voltage values for identical-raw suppression (50 mV epsilon).
    // Initialised to -1 so the first call always emits.
    float m_lastSupplyVolts{-1.0f};
    float m_lastUserAdc0Volts{-1.0f};

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

    // Shared state for setMicBoost (3M-1b G.1).
    // P1: emitted to case 10 (C0=0x12) C2 bit 0 (0x01).
    // P2: emitted to transmit_specific_buffer[50] bit 1 (0x02).
    // From Thetis networkproto1.c:581 [v2.10.3.13]; deskhpsdr new_protocol.c:1484-1486 [@120188f].
    bool m_micBoost{false};

    // Shared state for setLineIn (3M-1b G.2).
    // P1: emitted to case 10 (C0=0x12) C2 bit 1 (0x02).
    // P2: emitted to transmit_specific_buffer[50] bit 0 (0x01).
    // From Thetis networkproto1.c:581 [v2.10.3.13]; deskhpsdr new_protocol.c:1480-1482 [@120188f].
    bool m_lineIn{false};

    // Shared state for setMicTipRing (3M-1b G.3).
    // Parameter convention: true = Tip is mic (intuitive).
    // POLARITY INVERSION: both P1 and P2 write !m_micTipRing to the wire bit.
    // P1: emitted to case 11 (C0=0x14) C1 bit 4 (0x10) inverted.
    // P2: emitted to transmit_specific_buffer[50] bit 3 (0x08) inverted.
    // From Thetis networkproto1.c:597 [v2.10.3.13]; deskhpsdr new_protocol.c:1492-1494 [@120188f].
    bool m_micTipRing{true};

    // Shared state for setMicBias (3M-1b G.4).
    // Polarity: 1 = bias on (no inversion — parameter maps directly to wire bit).
    // Default false — bias off (per pre-code review §2.3 / §2.7 and
    //   TransmitModel::micBias default in C.2).
    // P1: emitted to case 11 (C0=0x14) C1 bit 5 (0x20).
    // P2: emitted to transmit_specific_buffer[50] bit 4 (0x10).
    // From Thetis networkproto1.c:597 [v2.10.3.13]; deskhpsdr new_protocol.c:1496-1498 [@120188f].
    bool m_micBias{false};

    // Shared state for setLineInGain (Task 2.1 of P1 full-parity epic).
    // 5-bit field (0-31).  Default 0 = no line-in attenuation.
    // P1: emitted to case 11 (C0=0x14) C2 low 5 bits via ctx.p1LineInGain.
    // P2: bridged into m_mic.lineInGain → byte 51 of CmdHighPriority.
    // From Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]:
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    int m_lineInGain{0};

    // Shared state for setUserDigOut (Task 2.2 of P1 full-parity epic).
    // 4-bit field (0-15).  Default 0 = all 4 user-dig-out pins low.
    // P1: emitted to case 11 (C0=0x14) C3 low 4 bits via ctx.p1UserDigOut.
    // P2: STORAGE-ONLY (no wire emission — user_dig_out is P1/Penny-only).
    // From Thetis ChannelMaster/networkproto1.c:601 [v2.10.3.13]:
    //   C3 = prn->user_dig_out & 0b00001111;
    quint8 m_userDigOut{0};

    // Shared state for setPuresignalRun (Task 2.3 of P1 full-parity epic).
    // Bool flag tracking whether PureSignal feedback DDC routing is active.
    // Default false = NOT routing.
    // P1: emitted to case 11 (C0=0x14) C2 bit 6 (mask 0x40) via ctx.p1PuresignalRun.
    // P2: STORAGE-ONLY (no wire emission — PS feedback DDC routing is 3M-4 deferred).
    // From Thetis ChannelMaster/networkproto1.c:600 [v2.10.3.13]:
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    bool m_puresignalRun{false};

    // Shared state for setMicPTTDisabled (3M-1b G.5; renamed for issue #182
    // to match Thetis MicPTTDisabled / mic_ptt_disabled storage name exactly).
    // Direct polarity: m_micPTTDisabled=true means PTT is disabled at the
    // firmware (wire bit = 1). Default false — PTT enabled by default,
    // matching Thetis console.cs:19757 [v2.10.3.13+501e3f51]:
    //   private bool mic_ptt_disabled = false;
    // P1 wire: case 11 (C0=0x14) C1 bit 6 (0x40), direct.
    // P2 wire: transmit_specific_buffer[50] bit 2 (0x04), direct.
    bool m_micPTTDisabled{false};

    // Shared state for setMicXlr (3M-1b G.6).
    // P2-only wire emission. P1 stores the flag for cross-board API consistency
    //   but does NOT emit any wire bytes. "Saturn G2 P2-only feature; P1 hardware
    //   has no XLR jack." P1 case-10 and case-11 C&C bytes are UNCHANGED
    //   regardless of m_micXlr value.
    // Polarity: 1 = XLR selected (no inversion — parameter maps directly to wire bit).
    // Default true — Saturn G2 ships with XLR-enabled configuration.
    //   This matches pre-code review §2.7 and TransmitModel::micXlr default in C.2.
    // P1: STORAGE-ONLY (no wire emission).
    // P2: emitted to transmit_specific_buffer[50] bit 5 (0x20).
    // From deskhpsdr new_protocol.c:1500-1502 [@120188f]:
    //   if (mic_input_xlr) { transmit_specific_buffer[50] |= 0x20; }
    bool m_micXlr{true};
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::RadioConnectionError)
Q_DECLARE_METATYPE(NereusSDR::ConnectFailure)
