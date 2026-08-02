# WDSP Integration Architecture

**Status:** Phase 2E -- Design

> **2026-04-10 CORRECTION:** Several default values in this document were found
> to be incorrect during implementation and debugging. The code (`RxChannel.cpp`)
> has the correct Thetis-sourced values. Key discrepancies: AGC top should be
> 90.0 (not 80.0), NR adapt rate 0.0016 (not 0.0001), NR leak 1e-7 (not 0.0001),
> NB1 threshold 3.3/30.0 (not 20.0), filter defaults are mode-dependent preset
> tables (not a single -2850/-150 pair). Full correction details in
> `docs/architecture/reviews/2026-04-10-plan-review.md` section 2.1.

---

## 1. Overview

WDSP (by Warren Pratt NR0V) is the complete DSP engine for NereusSDR. Unlike
AetherSDR where the FlexRadio performs DSP internally, NereusSDR's OpenHPSDR
radio is an ADC/DAC with network transport -- the client does ALL signal
processing. WDSP provides demodulation, filtering, AGC, noise reduction,
noise blanking, auto-notch, equalization, TX compression, CESSB, VOX/DEXP,
and PureSignal PA linearization.

This document defines the C++20/Qt6 wrapper architecture around the WDSP C
API, covering channel lifecycle, per-receiver state management, thread safety,
and integration with NereusSDR's model/signal/slot architecture.

### Design Principles

1. **One WDSP channel per receiver.** Each receiver has independent DSP state.
2. **Qt properties with signal/slot notification.** Every DSP parameter is a
   Q_PROPERTY on the channel wrapper, enabling automatic UI binding.
3. **Lock-free audio path.** The audio thread calls `fexchange2()` without
   holding any mutex. Parameter updates use `std::atomic` for simple flags
   and `std::shared_mutex` for complex state changes.
4. **WDSP API calls happen immediately.** Setters call through to WDSP on
   the calling thread. WDSP handles internal locking.
5. **Graceful degradation.** When `HAVE_WDSP` is not defined, all classes
   compile as no-ops with sensible defaults.

---

## 2. Enum Definitions

All WDSP-related enums live in a single header for shared use across the
engine, channel classes, and UI layers.

```cpp
// src/core/WdspTypes.h
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace NereusSDR {

// Demodulation mode. Values match WDSP's internal mode enum.
enum class DSPMode : int {
    LSB  = 0,
    USB  = 1,
    DSB  = 2,
    CWL  = 3,
    CWU  = 4,
    FM   = 5,
    AM   = 6,
    DIGU = 7,
    SPEC = 8,
    DIGL = 9,
    SAM  = 10,
    DRM  = 11
};
Q_ENUM_NS(DSPMode)

// AGC operating mode.
enum class AGCMode : int {
    Off    = 0,
    Long   = 1,
    Slow   = 2,
    Med    = 3,
    Fast   = 4,
    Custom = 5
};
Q_ENUM_NS(AGCMode)

// RX meter types. Values match WDSP GetRXAMeter type argument.
enum class RxMeterType : int {
    SignalPeak   = 0,   // S_PK
    SignalAvg    = 1,   // S_AV
    AdcPeak      = 2,   // ADC_PK
    AdcAvg       = 3,   // ADC_AV
    AgcGain      = 4,   // AGC_GAIN
    AgcPeak      = 5,   // AGC_PK
    AgcAvg       = 6    // AGC_AV
};
Q_ENUM_NS(RxMeterType)

// TX meter types. Values match WDSP GetTXAMeter type argument.
enum class TxMeterType : int {
    MicPeak      = 0,
    MicAvg       = 1,
    EqPeak       = 2,
    EqAvg        = 3,
    CfcPeak      = 4,
    CfcAvg       = 5,
    CfcGain      = 6,
    CompPeak     = 7,
    CompAvg      = 8,
    AlcPeak      = 9,
    AlcAvg       = 10,
    AlcGain      = 11,
    OutPeak      = 12,
    OutAvg       = 13,
    LevelerPeak  = 14,
    LevelerAvg   = 15,
    LevelerGain  = 16
};
Q_ENUM_NS(TxMeterType)

// Noise reduction algorithm selection.
enum class NrAlgorithm : int {
    NR1_LMS       = 0,   // ANR -- LMS adaptive
    NR2_Spectral  = 1,   // EMNR -- spectral subtraction
    RNNR          = 2,   // RNNoise neural network
    SBNR          = 3    // Spectral bleach
};
Q_ENUM_NS(NrAlgorithm)

// Squelch type (mode-dependent).
enum class SquelchType : int {
    AM       = 0,
    FM       = 1,
    Software = 2   // General-purpose (SSB, CW, digital)
};
Q_ENUM_NS(SquelchType)

// WDSP channel type.
enum class ChannelType : int {
    RX = 0,
    TX = 1
};
Q_ENUM_NS(ChannelType)

} // namespace NereusSDR
```

---

## 3. WdspEngine Class

`WdspEngine` is the central manager for all WDSP operations. It is owned
by `RadioModel` (not a singleton) and manages the system-level WDSP
lifecycle, channel allocation, and the FFTW wisdom / impulse cache.

### 3.1 Interface

```cpp
// src/core/WdspEngine.h
#pragma once

#include "WdspTypes.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <memory>

namespace NereusSDR {

class RxChannel;
class TxChannel;

// Central WDSP manager. Owns all channel instances and manages
// system-level initialization (wisdom, impulse cache).
//
// Owned by RadioModel. Created once per radio connection.
// Thread safety: create/destroy on main thread only.
//                processIq called from audio thread via channel objects.
class WdspEngine : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool initialized READ isInitialized NOTIFY initializedChanged)
    Q_PROPERTY(QString wdspVersion READ wdspVersion CONSTANT)

public:
    explicit WdspEngine(QObject* parent = nullptr);
    ~WdspEngine() override;

    // --- System lifecycle ---

    // Initialize WDSP: load FFTW wisdom, initialize impulse cache.
    // configDir: directory for wisdom file and impulse cache.
    // Returns false if WDSP initialization fails.
    bool initialize(const QString& configDir);

    // Shutdown WDSP: save impulse cache, destroy all channels, free resources.
    void shutdown();

    bool isInitialized() const { return m_initialized; }
    QString wdspVersion() const;

    // --- RX Channel management ---

    // Create an RX channel with the given parameters.
    // Returns the new RxChannel (owned by WdspEngine) or nullptr on failure.
    // channelId: WDSP channel number (0-31). Must be unique.
    RxChannel* createRxChannel(int channelId,
                               int inputBufferSize = 1024,
                               int dspBufferSize = 2048,
                               int inputSampleRate = 48000,
                               int dspSampleRate = 48000,
                               int outputSampleRate = 48000);

    // Destroy an RX channel by ID. The RxChannel pointer becomes invalid.
    void destroyRxChannel(int channelId);

    // Look up an existing RX channel by WDSP channel ID.
    RxChannel* rxChannel(int channelId) const;

    // Return all active RX channels.
    QVector<RxChannel*> rxChannels() const;

    // --- TX Channel management ---

    // Create the TX channel. Only one TX channel is supported.
    TxChannel* createTxChannel(int channelId,
                               int inputBufferSize = 1024,
                               int dspBufferSize = 2048,
                               int inputSampleRate = 48000,
                               int dspSampleRate = 48000,
                               int outputSampleRate = 48000);

    // Destroy the TX channel.
    void destroyTxChannel();

    // Access the TX channel (nullptr if not created).
    TxChannel* txChannel() const { return m_txChannel.get(); }

    // --- Sample rate management ---

    // Change sample rates for all channels atomically.
    // Briefly deactivates channels, applies rates, reactivates.
    void setAllSampleRates(int inputRate, int dspRate, int outputRate);

    // --- Impulse cache ---

    // Save the impulse cache to disk (call on clean shutdown).
    void saveImpulseCache();

    // Load the impulse cache from disk (call during initialization).
    void loadImpulseCache();

signals:
    void initializedChanged(bool initialized);
    void channelCreated(int channelId, ChannelType type);
    void channelDestroyed(int channelId);

private:
    bool m_initialized{false};
    QString m_configDir;

    // RX channels keyed by WDSP channel ID.
    QMap<int, std::unique_ptr<RxChannel>> m_rxChannels;

    // Single TX channel.
    std::unique_ptr<TxChannel> m_txChannel;
    int m_txChannelId{-1};
};

} // namespace NereusSDR
```

### 3.2 Initialization Sequence

```
RadioModel::connectToRadio()
  |
  v
WdspEngine::initialize(configDir)
  |-- WDSPwisdom(configDir)         -- load/generate FFTW wisdom
  |-- init_impulse_cache(true)      -- enable impulse caching
  |-- read_impulse_cache(cacheFile) -- load cached filter coefficients
  +-> m_initialized = true
  |
  v
WdspEngine::createRxChannel(0, ...)  -- RX1 main
WdspEngine::createRxChannel(1, ...)  -- RX1 sub (diversity)
WdspEngine::createRxChannel(2, ...)  -- RX2 main
WdspEngine::createRxChannel(3, ...)  -- RX2 sub (diversity)
WdspEngine::createTxChannel(4, ...)  -- TX
  |
  v
Each channel: OpenChannel() -> create_anbEXT() -> create_nobEXT()
  |
  v
Configure defaults: SetRXAMode, SetRXABandpassFreqs, SetRXAAGCMode, etc.
  |
  v
Activate needed channels: SetChannelState(id, 1, 0)
  (Initially only channel 0 is activated; others on demand)
```

### 3.3 Shutdown Sequence

```
RadioModel::disconnectFromRadio()
  |
  v
WdspEngine::shutdown()
  |-- For each channel:
  |     SetChannelState(id, 0, 1)   -- deactivate with drain
  |     destroy_anbEXT(id)          -- destroy NB1
  |     destroy_nobEXT(id)          -- destroy NB2
  |     CloseChannel(id)            -- free WDSP resources
  |
  |-- save_impulse_cache(cacheFile) -- persist filter cache
  |-- destroy_impulse_cache()       -- free cache memory
  +-> m_initialized = false
```

---

## 4. RxChannel Class

`RxChannel` wraps a single WDSP RX channel. It is a `QObject` with
`Q_PROPERTY` declarations for every DSP parameter, enabling direct
binding to UI controls via signals/slots.

### 4.1 Interface

```cpp
// src/core/RxChannel.h
#pragma once

#include "WdspTypes.h"

#include <QObject>
#include <QVector>

#include <atomic>
#include <shared_mutex>

namespace NereusSDR {

// Per-receiver WDSP channel wrapper.
//
// Owns one WDSP RX channel and provides Qt property access to all
// DSP parameters. Each setter immediately calls the corresponding
// WDSP API function and emits a change signal.
//
// Thread safety:
//   - Main thread: create/destroy, all property setters, meter reads
//   - Audio thread: processIq() (calls fexchange2)
//   - Spectrum timer: getSpectrum() (calls RXAGetaSipF)
//   - Meter timer: getMeter() (calls GetRXAMeter -- lock-free)
//
// Frequently-toggled enables use std::atomic for lock-free audio
// thread reads. Complex state changes (mode, sample rate) use
// std::shared_mutex with brief exclusive locks.
class RxChannel : public QObject {
    Q_OBJECT

    // --- Demodulation ---
    Q_PROPERTY(NereusSDR::DSPMode mode READ mode WRITE setMode
               NOTIFY modeChanged)

    // --- Bandpass filter ---
    Q_PROPERTY(double filterLow READ filterLow WRITE setFilterLow
               NOTIFY filterChanged)
    Q_PROPERTY(double filterHigh READ filterHigh WRITE setFilterHigh
               NOTIFY filterChanged)
    Q_PROPERTY(int filterKernelSize READ filterKernelSize
               WRITE setFilterKernelSize NOTIFY filterChanged)
    Q_PROPERTY(bool filterMinimumPhase READ filterMinimumPhase
               WRITE setFilterMinimumPhase NOTIFY filterChanged)

    // --- AGC ---
    Q_PROPERTY(NereusSDR::AGCMode agcMode READ agcMode WRITE setAgcMode
               NOTIFY agcModeChanged)
    Q_PROPERTY(double agcTop READ agcTop WRITE setAgcTop
               NOTIFY agcTopChanged)
    Q_PROPERTY(double agcFixedGain READ agcFixedGain WRITE setAgcFixedGain
               NOTIFY agcFixedGainChanged)
    Q_PROPERTY(double agcAttack READ agcAttack WRITE setAgcAttack
               NOTIFY agcTimingChanged)
    Q_PROPERTY(double agcDecay READ agcDecay WRITE setAgcDecay
               NOTIFY agcTimingChanged)
    Q_PROPERTY(double agcHang READ agcHang WRITE setAgcHang
               NOTIFY agcTimingChanged)
    Q_PROPERTY(double agcSlope READ agcSlope WRITE setAgcSlope
               NOTIFY agcTimingChanged)

    // --- Noise blanker ---
    Q_PROPERTY(bool nb1Enabled READ nb1Enabled WRITE setNb1Enabled
               NOTIFY nb1EnabledChanged)
    Q_PROPERTY(double nb1Threshold READ nb1Threshold WRITE setNb1Threshold
               NOTIFY nb1ThresholdChanged)
    Q_PROPERTY(bool nb2Enabled READ nb2Enabled WRITE setNb2Enabled
               NOTIFY nb2EnabledChanged)
    Q_PROPERTY(double nb2Threshold READ nb2Threshold WRITE setNb2Threshold
               NOTIFY nb2ThresholdChanged)

    // --- Noise reduction ---
    Q_PROPERTY(bool nrEnabled READ nrEnabled WRITE setNrEnabled
               NOTIFY nrEnabledChanged)
    Q_PROPERTY(NereusSDR::NrAlgorithm nrAlgorithm READ nrAlgorithm
               WRITE setNrAlgorithm NOTIFY nrAlgorithmChanged)
    Q_PROPERTY(int nrTaps READ nrTaps WRITE setNrTaps
               NOTIFY nrParamsChanged)
    Q_PROPERTY(int nrDelay READ nrDelay WRITE setNrDelay
               NOTIFY nrParamsChanged)
    Q_PROPERTY(double nrGain READ nrGain WRITE setNrGain
               NOTIFY nrParamsChanged)
    Q_PROPERTY(double nrLeakage READ nrLeakage WRITE setNrLeakage
               NOTIFY nrParamsChanged)

    // --- Auto-notch filter ---
    Q_PROPERTY(bool anfEnabled READ anfEnabled WRITE setAnfEnabled
               NOTIFY anfEnabledChanged)
    Q_PROPERTY(int anfTaps READ anfTaps WRITE setAnfTaps
               NOTIFY anfParamsChanged)
    Q_PROPERTY(int anfDelay READ anfDelay WRITE setAnfDelay
               NOTIFY anfParamsChanged)
    Q_PROPERTY(double anfGain READ anfGain WRITE setAnfGain
               NOTIFY anfParamsChanged)
    Q_PROPERTY(double anfLeakage READ anfLeakage WRITE setAnfLeakage
               NOTIFY anfParamsChanged)

    // --- Squelch ---
    Q_PROPERTY(bool squelchEnabled READ squelchEnabled
               WRITE setSquelchEnabled NOTIFY squelchEnabledChanged)
    Q_PROPERTY(double squelchThreshold READ squelchThreshold
               WRITE setSquelchThreshold NOTIFY squelchThresholdChanged)

    // --- Equalization ---
    Q_PROPERTY(bool eqEnabled READ eqEnabled WRITE setEqEnabled
               NOTIFY eqEnabledChanged)

    // --- Audio panel ---
    Q_PROPERTY(double volume READ volume WRITE setVolume
               NOTIFY volumeChanged)
    Q_PROPERTY(double pan READ pan WRITE setPan
               NOTIFY panChanged)
    Q_PROPERTY(bool binaural READ binaural WRITE setBinaural
               NOTIFY binauralChanged)

    // --- Channel state ---
    Q_PROPERTY(bool active READ isActive WRITE setActive
               NOTIFY activeChanged)

public:
    explicit RxChannel(int channelId,
                       int inputBufferSize,
                       int dspBufferSize,
                       int inputSampleRate,
                       int dspSampleRate,
                       int outputSampleRate,
                       QObject* parent = nullptr);
    ~RxChannel() override;

    int channelId() const { return m_channelId; }

    // ---------------------------------------------------------------
    // Demodulation
    // ---------------------------------------------------------------

    DSPMode mode() const;
    void setMode(DSPMode mode);

    // ---------------------------------------------------------------
    // Bandpass filter
    // ---------------------------------------------------------------

    double filterLow() const { return m_filterLow; }
    double filterHigh() const { return m_filterHigh; }
    void setFilterLow(double hz);
    void setFilterHigh(double hz);

    // Set both filter edges atomically (avoids two WDSP calls).
    void setFilterFreqs(double lowHz, double highHz);

    int filterKernelSize() const { return m_filterKernelSize; }
    void setFilterKernelSize(int nc);

    bool filterMinimumPhase() const { return m_filterMinPhase; }
    void setFilterMinimumPhase(bool mp);

    // ---------------------------------------------------------------
    // AGC
    // ---------------------------------------------------------------

    AGCMode agcMode() const;
    void setAgcMode(AGCMode mode);

    double agcTop() const { return m_agcTop; }
    void setAgcTop(double top);

    double agcFixedGain() const { return m_agcFixedGain; }
    void setAgcFixedGain(double gain);

    double agcAttack() const { return m_agcAttack; }
    void setAgcAttack(double ms);

    double agcDecay() const { return m_agcDecay; }
    void setAgcDecay(double ms);

    double agcHang() const { return m_agcHang; }
    void setAgcHang(double ms);

    double agcSlope() const { return m_agcSlope; }
    void setAgcSlope(double dB);

    // ---------------------------------------------------------------
    // Noise blanker
    // ---------------------------------------------------------------

    bool nb1Enabled() const { return m_nb1Enabled.load(); }
    void setNb1Enabled(bool enabled);

    double nb1Threshold() const { return m_nb1Threshold; }
    void setNb1Threshold(double threshold);

    bool nb2Enabled() const { return m_nb2Enabled.load(); }
    void setNb2Enabled(bool enabled);

    double nb2Threshold() const { return m_nb2Threshold; }
    void setNb2Threshold(double threshold);

    // ---------------------------------------------------------------
    // Noise reduction
    // ---------------------------------------------------------------

    bool nrEnabled() const { return m_nrEnabled.load(); }
    void setNrEnabled(bool enabled);

    NrAlgorithm nrAlgorithm() const;
    void setNrAlgorithm(NrAlgorithm algo);

    int nrTaps() const { return m_nrTaps; }
    void setNrTaps(int taps);

    int nrDelay() const { return m_nrDelay; }
    void setNrDelay(int delay);

    double nrGain() const { return m_nrGain; }
    void setNrGain(double gain);

    double nrLeakage() const { return m_nrLeakage; }
    void setNrLeakage(double leakage);

    // EMNR (NR2) specific parameters
    void setEmnrGainMethod(int method);
    void setEmnrNpeMethod(int method);

    // ---------------------------------------------------------------
    // Auto-notch filter
    // ---------------------------------------------------------------

    bool anfEnabled() const { return m_anfEnabled.load(); }
    void setAnfEnabled(bool enabled);

    int anfTaps() const { return m_anfTaps; }
    void setAnfTaps(int taps);

    int anfDelay() const { return m_anfDelay; }
    void setAnfDelay(int delay);

    double anfGain() const { return m_anfGain; }
    void setAnfGain(double gain);

    double anfLeakage() const { return m_anfLeakage; }
    void setAnfLeakage(double leakage);

    // ---------------------------------------------------------------
    // Manual notch filters
    // ---------------------------------------------------------------

    // Add a manual notch filter. Returns the notch index.
    int addNotch(double centerFreqHz, double widthHz, bool active = true);

    // Remove a notch by index.
    void removeNotch(int notchIndex);

    // Edit an existing notch.
    void editNotch(int notchIndex, double centerFreqHz, double widthHz,
                   bool active);

    // Get the number of active notches.
    int notchCount() const;

    // Enable/disable the notch filter engine.
    void setNotchesEnabled(bool enabled);

    // Update the tuning reference frequency for notch offset calculation.
    void setNotchTuneFrequency(double freqHz);

    // ---------------------------------------------------------------
    // Squelch
    // ---------------------------------------------------------------

    bool squelchEnabled() const { return m_squelchEnabled.load(); }
    void setSquelchEnabled(bool enabled);

    double squelchThreshold() const { return m_squelchThreshold; }
    void setSquelchThreshold(double threshold);

    // ---------------------------------------------------------------
    // Equalization
    // ---------------------------------------------------------------

    bool eqEnabled() const { return m_eqEnabled; }
    void setEqEnabled(bool enabled);

    // Set 10-band graphic EQ gains (array of 10 doubles, dB).
    void setGraphicEq10(const double gains[10]);

    // Set parametric EQ profile.
    void setParametricEq(int numFreqs, const double* freqs,
                         const double* gains, const double* qFactors);

    // ---------------------------------------------------------------
    // Audio panel
    // ---------------------------------------------------------------

    double volume() const { return m_volume; }
    void setVolume(double gain);

    double pan() const { return m_pan; }
    void setPan(double pan);  // 0.0=left, 0.5=center, 1.0=right

    bool binaural() const { return m_binaural; }
    void setBinaural(bool enabled);

    // ---------------------------------------------------------------
    // Channel state
    // ---------------------------------------------------------------

    bool isActive() const { return m_active; }
    void setActive(bool active);

    // ---------------------------------------------------------------
    // Audio processing (called from audio thread)
    // ---------------------------------------------------------------

    // Process I/Q samples through the complete WDSP RX chain.
    // Input:  inI/inQ arrays of sampleCount floats (raw I/Q from radio)
    // Output: outI/outQ arrays of sampleCount floats (decoded audio L/R)
    //
    // This is the hot path. Called from the audio thread every buffer
    // cycle. Must not block on any mutex.
    //
    // NB1/NB2 are processed before fexchange2:
    //   xanbEXT(id, in, out)  -- if NB1 enabled
    //   xnobEXT(id, in, out)  -- if NB2 enabled
    //   fexchange2(channel, Iin, Qin, Iout, Qout, &error)
    void processIq(float* inI, float* inQ,
                   float* outI, float* outQ,
                   int sampleCount);

    // ---------------------------------------------------------------
    // Spectrum data extraction
    // ---------------------------------------------------------------

    // Get the current spectrum data from WDSP.
    // buffer: output array of `size` floats (dBFS values).
    // Returns true if new data was available.
    bool getSpectrum(float* buffer, int size);

    // Configure WDSP's internal spectrum analyzer for this channel.
    void configureSpectrum(int fftSize, int averages, int windowType);

    // ---------------------------------------------------------------
    // Meter reading (lock-free, called from GUI timer)
    // ---------------------------------------------------------------

    // Read a single RX meter value from WDSP.
    // Returns the meter value in dBFS or dB depending on type.
    // Lock-free: WDSP meter reads are atomic internally.
    double getMeter(RxMeterType type) const;

    // Read all standard meters in one batch call.
    // Returns: { signalPk, signalAvg, adcPk, agcGain }
    struct MeterValues {
        double signalPeak{-140.0};
        double signalAvg{-140.0};
        double adcPeak{-140.0};
        double agcGain{0.0};
    };
    MeterValues getMeters() const;

    // ---------------------------------------------------------------
    // FM-specific
    // ---------------------------------------------------------------

    void setFmDeviation(double deviationHz);
    void setFmLimiterEnabled(bool enabled);
    void setFmAudioFilterEnabled(bool enabled);

    // ---------------------------------------------------------------
    // CW-specific filter shapes
    // ---------------------------------------------------------------

    void setSpcwEnabled(bool enabled);
    void setSpcwFreq(double freq);
    void setSpcwBandwidth(double bw);

    // ---------------------------------------------------------------
    // Sample rate (expensive -- use sparingly)
    // ---------------------------------------------------------------

    // Change all three sample rates. Briefly deactivates the channel,
    // applies rates via SetAllRates, then reactivates.
    void setSampleRates(int inputRate, int dspRate, int outputRate);

    int inputSampleRate() const { return m_inputSampleRate; }
    int dspSampleRate() const { return m_dspSampleRate; }
    int outputSampleRate() const { return m_outputSampleRate; }

signals:
    void modeChanged(NereusSDR::DSPMode mode);
    void filterChanged(double low, double high);
    void agcModeChanged(NereusSDR::AGCMode mode);
    void agcTopChanged(double top);
    void agcFixedGainChanged(double gain);
    void agcTimingChanged();
    void nb1EnabledChanged(bool enabled);
    void nb1ThresholdChanged(double threshold);
    void nb2EnabledChanged(bool enabled);
    void nb2ThresholdChanged(double threshold);
    void nrEnabledChanged(bool enabled);
    void nrAlgorithmChanged(NereusSDR::NrAlgorithm algo);
    void nrParamsChanged();
    void anfEnabledChanged(bool enabled);
    void anfParamsChanged();
    void squelchEnabledChanged(bool enabled);
    void squelchThresholdChanged(double threshold);
    void eqEnabledChanged(bool enabled);
    void volumeChanged(double volume);
    void panChanged(double pan);
    void binauralChanged(bool enabled);
    void activeChanged(bool active);
    void spectrumReady(const QVector<float>& data);
    void metersChanged(NereusSDR::RxChannel::MeterValues values);

private:
    const int m_channelId;

    // Sample rates
    int m_inputSampleRate;
    int m_dspSampleRate;
    int m_outputSampleRate;
    int m_inputBufferSize;

    // Mutex for complex state changes (mode, sample rate).
    // Audio thread acquires shared (read) lock; main thread acquires
    // exclusive (write) lock only during mode/rate changes.
    mutable std::shared_mutex m_stateMutex;

    // --- Atomic parameters (lock-free audio thread reads) ---
    std::atomic<int> m_mode{static_cast<int>(DSPMode::USB)};
    std::atomic<int> m_agcMode{static_cast<int>(AGCMode::Med)};
    std::atomic<bool> m_nb1Enabled{false};
    std::atomic<bool> m_nb2Enabled{false};
    std::atomic<bool> m_nrEnabled{false};
    std::atomic<int> m_nrAlgorithm{static_cast<int>(NrAlgorithm::NR1_LMS)};
    std::atomic<bool> m_anfEnabled{false};
    std::atomic<bool> m_squelchEnabled{false};
    std::atomic<bool> m_active{false};

    // --- Non-atomic cached state (protected by m_stateMutex for writes) ---
    // Filter defaults are mode-dependent preset tables per Thetis console.cs:5180-5273.
    // Examples: USB F5 = +100/+3000, LSB F5 = -3000/-100
    double m_filterLow{150.0};   // From Thetis (mode-dependent)
    double m_filterHigh{2850.0}; // From Thetis (mode-dependent)
    int m_filterKernelSize{2048};
    bool m_filterMinPhase{false};

    double m_agcTop{90.0};        // From Thetis radio.cs:1018
    double m_agcFixedGain{20.0};
    double m_agcAttack{2.0};
    double m_agcDecay{250.0};
    double m_agcHang{100.0};
    double m_agcSlope{0.0};

    double m_nb1Threshold{3.3};   // From Thetis radio.cs:843
    double m_nb2Threshold{30.0};  // From Thetis cmaster.c:53

    int m_nrTaps{64};
    int m_nrDelay{16};
    double m_nrGain{16.0};
    double m_nrLeakage{1e-7};  // Corrected — was 10.0 in original design

    int m_anfTaps{64};
    int m_anfDelay{16};
    double m_anfGain{32.0};
    double m_anfLeakage{1.0};

    double m_squelchThreshold{-140.0};

    bool m_eqEnabled{false};

    double m_volume{1.0};
    double m_pan{0.5};
    bool m_binaural{false};
};

} // namespace NereusSDR
```

### 4.2 Setter Implementation Pattern

Every setter follows the same pattern: update cached state, call the
WDSP API function, and emit a change signal.

```cpp
void RxChannel::setMode(DSPMode mode)
{
    if (static_cast<int>(mode) == m_mode.load()) {
        return;  // No change
    }

    {
        // Exclusive lock: mode change may affect internal WDSP state.
        std::unique_lock lock(m_stateMutex);
        m_mode.store(static_cast<int>(mode));
#ifdef HAVE_WDSP
        SetRXAMode(m_channelId, static_cast<int>(mode));
#endif
    }

    emit modeChanged(mode);
}

void RxChannel::setNrEnabled(bool enabled)
{
    if (enabled == m_nrEnabled.load()) {
        return;
    }

    m_nrEnabled.store(enabled);

#ifdef HAVE_WDSP
    // Route to the active NR algorithm
    switch (nrAlgorithm()) {
    case NrAlgorithm::NR1_LMS:
        SetRXAANRRun(m_channelId, enabled ? 1 : 0);
        break;
    case NrAlgorithm::NR2_Spectral:
        SetRXAEMNRRun(m_channelId, enabled ? 1 : 0);
        break;
    case NrAlgorithm::RNNR:
        SetRXARNNRRun(m_channelId, enabled ? 1 : 0);
        break;
    case NrAlgorithm::SBNR:
        SetRXASBNRRun(m_channelId, enabled ? 1 : 0);
        break;
    }
#endif

    emit nrEnabledChanged(enabled);
}

void RxChannel::setAgcMode(AGCMode mode)
{
    if (static_cast<int>(mode) == m_agcMode.load()) {
        return;
    }

    m_agcMode.store(static_cast<int>(mode));

#ifdef HAVE_WDSP
    SetRXAAGCMode(m_channelId, static_cast<int>(mode));
#endif

    emit agcModeChanged(mode);
}

void RxChannel::setFilterFreqs(double lowHz, double highHz)
{
    if (m_filterLow == lowHz && m_filterHigh == highHz) {
        return;
    }

    m_filterLow = lowHz;
    m_filterHigh = highHz;

#ifdef HAVE_WDSP
    SetRXABandpassFreqs(m_channelId, lowHz, highHz);
#endif

    emit filterChanged(lowHz, highHz);
}
```

### 4.3 processIq Implementation

```cpp
void RxChannel::processIq(float* inI, float* inQ,
                           float* outI, float* outQ,
                           int sampleCount)
{
    if (!m_active.load()) {
        // Channel inactive -- zero the output buffers.
        std::memset(outI, 0, sampleCount * sizeof(float));
        std::memset(outQ, 0, sampleCount * sizeof(float));
        return;
    }

#ifdef HAVE_WDSP
    // NB1 and NB2 operate on raw I/Q BEFORE the main WDSP channel.
    // They use separate lifecycle (create_anbEXT/create_nobEXT).
    if (m_nb1Enabled.load()) {
        xanbEXT(m_channelId, inI, inI);  // in-place for I
        xanbEXT(m_channelId, inQ, inQ);  // in-place for Q
    }
    if (m_nb2Enabled.load()) {
        xnobEXT(m_channelId, inI, inI);
        xnobEXT(m_channelId, inQ, inQ);
    }

    // Main WDSP processing: demod, AGC, NR, ANF, filter, EQ, audio panel.
    int error = 0;
    fexchange2(m_channelId, inI, inQ, outI, outQ, &error);

    if (error != 0) {
        qCWarning(lcWdsp) << "fexchange2 error on channel"
                          << m_channelId << ":" << error;
    }
#else
    // WDSP not available -- passthrough (silence).
    std::memset(outI, 0, sampleCount * sizeof(float));
    std::memset(outQ, 0, sampleCount * sizeof(float));
#endif
}
```

---

## 5. TxChannel Class

`TxChannel` wraps the single WDSP TX channel. Only one TX channel exists
at a time. It manages the full TX processing chain: phase rotation,
leveler, compressor, CFCOMP, ALC, bandpass filter, and audio panel.

### 5.1 Interface

```cpp
// src/core/TxChannel.h
#pragma once

#include "WdspTypes.h"

#include <QObject>

#include <atomic>
#include <shared_mutex>

namespace NereusSDR {

// WDSP TX channel wrapper.
//
// Manages the TX DSP chain: mic input -> phase rotation -> leveler ->
// compressor -> CFCOMP -> ALC -> bandpass filter -> modulated I/Q output.
//
// Thread safety: same model as RxChannel.
//   - Main thread: property setters
//   - Audio thread: processIq()
class TxChannel : public QObject {
    Q_OBJECT

    // --- Demodulation/modulation mode ---
    Q_PROPERTY(NereusSDR::DSPMode mode READ mode WRITE setMode
               NOTIFY modeChanged)

    // --- TX bandpass filter ---
    Q_PROPERTY(double filterLow READ filterLow WRITE setFilterLow
               NOTIFY filterChanged)
    Q_PROPERTY(double filterHigh READ filterHigh WRITE setFilterHigh
               NOTIFY filterChanged)

    // --- Compressor ---
    Q_PROPERTY(bool compressorEnabled READ compressorEnabled
               WRITE setCompressorEnabled NOTIFY compressorEnabledChanged)
    Q_PROPERTY(double compressorGain READ compressorGain
               WRITE setCompressorGain NOTIFY compressorGainChanged)

    // --- Leveler ---
    Q_PROPERTY(bool levelerEnabled READ levelerEnabled
               WRITE setLevelerEnabled NOTIFY levelerEnabledChanged)
    Q_PROPERTY(double levelerTop READ levelerTop
               WRITE setLevelerTop NOTIFY levelerTopChanged)

    // --- Phase rotation ---
    Q_PROPERTY(bool phaseRotEnabled READ phaseRotEnabled
               WRITE setPhaseRotEnabled NOTIFY phaseRotEnabledChanged)

    // --- CESSB ---
    Q_PROPERTY(bool cessbEnabled READ cessbEnabled
               WRITE setCessbEnabled NOTIFY cessbEnabledChanged)

    // --- CFCOMP (spectral compressor) ---
    Q_PROPERTY(bool cfcompEnabled READ cfcompEnabled
               WRITE setCfcompEnabled NOTIFY cfcompEnabledChanged)

    // --- ALC ---
    Q_PROPERTY(bool alcEnabled READ alcEnabled
               WRITE setAlcEnabled NOTIFY alcEnabledChanged)

    // --- TX EQ ---
    Q_PROPERTY(bool eqEnabled READ eqEnabled
               WRITE setEqEnabled NOTIFY eqEnabledChanged)

    // --- Mic gain ---
    Q_PROPERTY(double micGain READ micGain
               WRITE setMicGain NOTIFY micGainChanged)

    // --- FM TX ---
    Q_PROPERTY(double fmDeviation READ fmDeviation
               WRITE setFmDeviation NOTIFY fmDeviationChanged)
    Q_PROPERTY(bool ctcssEnabled READ ctcssEnabled
               WRITE setCtcssEnabled NOTIFY ctcssChanged)
    Q_PROPERTY(double ctcssFreq READ ctcssFreq
               WRITE setCtcssFreq NOTIFY ctcssChanged)

    // --- VOX ---
    Q_PROPERTY(bool voxEnabled READ voxEnabled
               WRITE setVoxEnabled NOTIFY voxEnabledChanged)
    Q_PROPERTY(double voxThreshold READ voxThreshold
               WRITE setVoxThreshold NOTIFY voxThresholdChanged)

    // --- Channel state ---
    Q_PROPERTY(bool active READ isActive WRITE setActive
               NOTIFY activeChanged)

public:
    explicit TxChannel(int channelId,
                       int inputBufferSize,
                       int dspBufferSize,
                       int inputSampleRate,
                       int dspSampleRate,
                       int outputSampleRate,
                       QObject* parent = nullptr);
    ~TxChannel() override;

    int channelId() const { return m_channelId; }

    // --- Mode ---
    DSPMode mode() const;
    void setMode(DSPMode mode);

    // --- Filter ---
    double filterLow() const { return m_filterLow; }
    double filterHigh() const { return m_filterHigh; }
    void setFilterLow(double hz);
    void setFilterHigh(double hz);
    void setFilterFreqs(double lowHz, double highHz);

    // --- Compressor ---
    bool compressorEnabled() const { return m_compEnabled.load(); }
    void setCompressorEnabled(bool enabled);
    double compressorGain() const { return m_compGain; }
    void setCompressorGain(double gaindB);

    // --- Leveler ---
    bool levelerEnabled() const { return m_levelerEnabled.load(); }
    void setLevelerEnabled(bool enabled);
    double levelerTop() const { return m_levelerTop; }
    void setLevelerTop(double topdB);
    void setLevelerAttack(double ms);
    void setLevelerDecay(double ms);
    void setLevelerHang(double ms);

    // --- Phase rotation ---
    bool phaseRotEnabled() const { return m_phaseRotEnabled.load(); }
    void setPhaseRotEnabled(bool enabled);
    void setPhaseRotCorner(double freqHz);
    void setPhaseRotStages(int nstages);

    // --- CESSB ---
    bool cessbEnabled() const { return m_cessbEnabled.load(); }
    void setCessbEnabled(bool enabled);

    // --- CFCOMP (spectral compressor) ---
    bool cfcompEnabled() const { return m_cfcompEnabled.load(); }
    void setCfcompEnabled(bool enabled);
    void setCfcompProfile(int numFreqs, const double* freqs,
                          const double* gains, const double* eFactors);

    // --- ALC ---
    bool alcEnabled() const { return m_alcEnabled.load(); }
    void setAlcEnabled(bool enabled);
    void setAlcAttack(double ms);
    void setAlcDecay(double ms);
    void setAlcHang(double ms);
    void setAlcMaxGain(double gaindB);

    // --- TX EQ ---
    bool eqEnabled() const { return m_eqEnabled; }
    void setEqEnabled(bool enabled);
    void setGraphicEq10(const double gains[10]);

    // --- Mic gain ---
    double micGain() const { return m_micGain; }
    void setMicGain(double gain);

    // --- FM TX ---
    double fmDeviation() const { return m_fmDeviation; }
    void setFmDeviation(double deviationHz);
    bool ctcssEnabled() const { return m_ctcssEnabled; }
    void setCtcssEnabled(bool enabled);
    double ctcssFreq() const { return m_ctcssFreq; }
    void setCtcssFreq(double freqHz);

    // --- VOX ---
    bool voxEnabled() const { return m_voxEnabled.load(); }
    void setVoxEnabled(bool enabled);
    double voxThreshold() const { return m_voxThreshold; }
    void setVoxThreshold(double threshold);
    void sendAntiVoxData(const float* speakerAudio, int sampleCount);

    // --- Channel state ---
    bool isActive() const { return m_active; }
    void setActive(bool active);

    // --- Audio processing (called from audio thread) ---

    // Process mic audio through the TX DSP chain.
    // Input:  inI/inQ = mic audio (mono, duplicated to I/Q for SSB)
    // Output: outI/outQ = modulated I/Q samples for the radio DAC.
    void processIq(float* inI, float* inQ,
                   float* outI, float* outQ,
                   int sampleCount);

    // --- Metering ---
    double getMeter(TxMeterType type) const;

    struct TxMeterValues {
        double micPeak{-80.0};
        double compPeak{-80.0};
        double alcPeak{-80.0};
        double outPeak{-80.0};
        double alcGain{0.0};
        double levelerGain{0.0};
    };
    TxMeterValues getMeters() const;

    // --- Spectrum ---
    bool getSpectrum(float* buffer, int size);

signals:
    void modeChanged(NereusSDR::DSPMode mode);
    void filterChanged(double low, double high);
    void compressorEnabledChanged(bool enabled);
    void compressorGainChanged(double gain);
    void levelerEnabledChanged(bool enabled);
    void levelerTopChanged(double top);
    void phaseRotEnabledChanged(bool enabled);
    void cessbEnabledChanged(bool enabled);
    void cfcompEnabledChanged(bool enabled);
    void alcEnabledChanged(bool enabled);
    void eqEnabledChanged(bool enabled);
    void micGainChanged(double gain);
    void fmDeviationChanged(double deviation);
    void ctcssChanged();
    void voxEnabledChanged(bool enabled);
    void voxThresholdChanged(double threshold);
    void voxTriggered();
    void voxReleased();
    void activeChanged(bool active);
    void metersChanged(NereusSDR::TxChannel::TxMeterValues values);

private:
    const int m_channelId;

    int m_inputSampleRate;
    int m_dspSampleRate;
    int m_outputSampleRate;
    int m_inputBufferSize;

    mutable std::shared_mutex m_stateMutex;

    // Atomic flags for lock-free audio thread reads
    std::atomic<int> m_mode{static_cast<int>(DSPMode::USB)};
    std::atomic<bool> m_compEnabled{false};
    std::atomic<bool> m_levelerEnabled{false};
    std::atomic<bool> m_phaseRotEnabled{false};
    std::atomic<bool> m_cessbEnabled{false};
    std::atomic<bool> m_cfcompEnabled{false};
    std::atomic<bool> m_alcEnabled{true};  // ALC always active by default
    std::atomic<bool> m_voxEnabled{false};
    std::atomic<bool> m_active{false};

    // Cached state
    double m_filterLow{200.0};
    double m_filterHigh{2800.0};
    double m_compGain{0.0};
    double m_levelerTop{5.0};
    double m_micGain{1.0};
    double m_fmDeviation{5000.0};
    bool m_ctcssEnabled{false};
    double m_ctcssFreq{100.0};
    double m_voxThreshold{0.01};
    bool m_eqEnabled{false};
};

} // namespace NereusSDR
```

---

## 6. PureSignal Subsystem

PureSignal is WDSP's PA linearization system. It uses a dedicated feedback
RX channel to capture the actual PA output, compares it with the intended
TX signal, and computes pre-distortion corrections applied to subsequent
TX buffers.

### 6.1 Architecture

```
                              Radio
                           +----------+
Mic -> TxChannel -> I/Q -> |   DAC    | -> PA -> Antenna
                           |          |      |
                           |   ADC    | <- Coupler tap
                           +----------+
                                |
                         Feedback I/Q
                                |
                                v
                    Feedback RxChannel (channel 5)
                                |
                                v
                         pscc(ch, size, txRef, rxFb)
                                |
                                v
                    Correction coefficients
                                |
                                v
                    CFCOMP profile update
                                |
                                v
                    Applied to next TX buffer
```

### 6.2 PureSignal Class

```cpp
// src/core/PureSignal.h
#pragma once

#include <QObject>

namespace NereusSDR {

class RxChannel;
class TxChannel;
class WdspEngine;

// PureSignal PA linearization controller.
//
// Manages the feedback RX channel and the correction calculation loop.
// Requires a dedicated feedback RX channel (not available for normal RX)
// and the TX channel.
//
// Timing constraint: feedback path latency must be less than one buffer
// period (~20ms at 48kHz/1024 samples). Higher sample rates reduce this
// constraint proportionally.
class PureSignal : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled
               NOTIFY enabledChanged)
    Q_PROPERTY(bool calibrated READ isCalibrated
               NOTIFY calibrationStateChanged)
    Q_PROPERTY(double correctionPeak READ correctionPeak
               NOTIFY correctionUpdated)

public:
    // feedbackChannelId: WDSP channel ID for the dedicated feedback RX.
    // txChannel: pointer to the active TX channel.
    explicit PureSignal(WdspEngine* engine,
                        int feedbackChannelId,
                        TxChannel* txChannel,
                        QObject* parent = nullptr);
    ~PureSignal() override;

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    bool isCalibrated() const { return m_calibrated; }

    // Get the peak correction magnitude (for UI display).
    double correctionPeak() const { return m_correctionPeak; }

    // Initiate a single-shot calibration sequence.
    void calibrate();

    // Reset correction state (clear learned coefficients).
    void reset();

    // Called from the audio thread after each TX buffer cycle.
    // txRef: TX reference I/Q (what we intended to transmit)
    // rxFb:  RX feedback I/Q (what the PA actually transmitted)
    // size:  number of samples
    void processFeedback(const float* txRefI, const float* txRefQ,
                         const float* rxFbI, const float* rxFbQ,
                         int size);

    // Access the feedback RX channel (for sample rate / state management).
    RxChannel* feedbackChannel() const { return m_feedbackChannel; }

signals:
    void enabledChanged(bool enabled);
    void calibrationStateChanged(bool calibrated);
    void correctionUpdated(double peak);
    void calibrationStarted();
    void calibrationComplete(bool success);
    void feedbackError(const QString& message);

private:
    // Update the CFCOMP profile from PureSignal correction data.
    void updateCfcompProfile();

    WdspEngine* m_engine;
    TxChannel* m_txChannel;
    RxChannel* m_feedbackChannel{nullptr};  // Owned by WdspEngine
    int m_feedbackChannelId;

    bool m_enabled{false};
    bool m_calibrated{false};
    double m_correctionPeak{0.0};
};

} // namespace NereusSDR
```

### 6.3 PureSignal Processing Loop

```
Audio thread (every TX buffer cycle):
  |
  |-- TxChannel::processIq(micI, micQ, txOutI, txOutQ, size)
  |     // Produces modulated I/Q for the radio DAC.
  |     // Also retains a copy of txOut as the TX reference.
  |
  |-- Send txOutI/txOutQ to radio via RadioConnection
  |
  |-- (Next buffer) Receive feedback I/Q from radio via RadioConnection
  |
  |-- RxChannel::processIq(fbI, fbQ, fbOutI, fbOutQ, size)
  |     // Feedback channel processes the captured PA output.
  |
  |-- PureSignal::processFeedback(txRefI, txRefQ, fbOutI, fbOutQ, size)
  |     |
  |     |-- pscc(m_feedbackChannelId, size, txRef, rxFb)
  |     |     // WDSP computes correction coefficients.
  |     |
  |     |-- GetTXACFCOMPGainAndMask(txChId, gain, mask, size)
  |     |     // Read updated correction data.
  |     |
  |     +-- SetTXACFCOMPprofile(txChId, ...)
  |           // Apply correction to TX chain.
  |
  +-- Correction active on next TX buffer
```

### 6.4 Feedback Delay Constraint

The feedback path round-trip must be less than approximately one buffer
period:

| Sample Rate | Buffer Size | Max Feedback Delay |
|-------------|-------------|--------------------|
| 48 kHz | 1024 | ~21.3 ms |
| 96 kHz | 1024 | ~10.7 ms |
| 192 kHz | 1024 | ~5.3 ms |
| 384 kHz | 1024 | ~2.7 ms |

Higher sample rates provide faster correction convergence and tighter
linearization. The default 48 kHz rate is sufficient for most use cases.

---

## 7. DSP Parameter Flow

### 7.1 UI to WDSP (Setting a Parameter)

```
UI Control (e.g., AGC mode combo box)
  |
  v
SliceModel::setAgcMode(AGCMode::Fast)
  |-- m_agcMode = AGCMode::Fast
  |-- emit agcModeChanged(AGCMode::Fast)
  |
  v
Connection: SliceModel::agcModeChanged --> RxChannel::setAgcMode
  (wired in RadioModel during slice creation)
  |
  v
RxChannel::setAgcMode(AGCMode::Fast)
  |-- m_agcMode.store(static_cast<int>(AGCMode::Fast))
  |-- WDSP: SetRXAAGCMode(m_channelId, 4)
  |-- emit agcModeChanged(AGCMode::Fast)
  |
  v
Connection: RxChannel::agcModeChanged --> UI update
  (combo box selection reflects new mode)
```

### 7.2 Guard Against Feedback Loops

When the UI triggers a model change, the model emits a signal that
updates the WDSP channel, which in turn emits a signal that would
update the UI -- potentially causing an infinite loop.

The guard pattern (matching the existing `m_updatingFromModel`
convention):

```cpp
// In the UI widget slot connected to RxChannel::agcModeChanged:
void AgcWidget::onAgcModeChanged(AGCMode mode)
{
    QSignalBlocker blocker(m_comboAgcMode);
    m_comboAgcMode->setCurrentIndex(static_cast<int>(mode));
}
```

Or equivalently, using a guard flag:

```cpp
void AgcWidget::onAgcModeChanged(AGCMode mode)
{
    m_updatingFromModel = true;
    m_comboAgcMode->setCurrentIndex(static_cast<int>(mode));
    m_updatingFromModel = false;
}

void AgcWidget::onComboChanged(int index)
{
    if (m_updatingFromModel) {
        return;
    }
    m_rxChannel->setAgcMode(static_cast<AGCMode>(index));
}
```

---

## 8. Audio Integration

### 8.1 RX Audio Path

```
RadioConnection (I/Q data from radio)
  |
  +-> signal: iqDataReceived(int rxId, float* I, float* Q, int count)
        |
        v
  AudioEngine (runs on audio thread)
    |
    |-- Look up RxChannel for this rxId
    |-- Allocate aligned output buffers
    |
    |-- RxChannel::processIq(I, Q, outL, outR, count)
    |     |
    |     |-- xanbEXT / xnobEXT (NB1/NB2 if enabled)
    |     |-- fexchange2(channelId, I, Q, outL, outR, &error)
    |     |     // WDSP does: demod, AGC, NR, ANF, filter, EQ, audio panel
    |     +-- Returns decoded audio in outL/outR
    |
    |-- Mix output from all active RxChannels
    |-- Apply master volume
    |
    +-> QAudioSink -> speakers
```

### 8.2 TX Audio Path

```
QAudioSource (microphone)
  |
  +-> AudioEngine (audio thread)
        |
        |-- VOX detection: xdexp(voxId, micSamples, ...)
        |     If VOX triggers -> emit voxTriggered()
        |
        |-- TxChannel::processIq(micI, micQ, txI, txQ, count)
        |     |
        |     |-- fexchange2(channelId, micI, micQ, txI, txQ, &error)
        |     |     // WDSP does: phase rot, leveler, comp, CFCOMP,
        |     |     // ALC, bandpass, modulation
        |     +-- Returns modulated I/Q in txI/txQ
        |
        +-> signal: txIqReady(float* I, float* Q, int count)
              |
              v
        RadioConnection -> UDP -> Radio DAC
```

### 8.3 Buffer Alignment

WDSP and FFTW benefit from aligned memory for SIMD (AVX/AVX2).
All audio buffers are allocated on 32-byte boundaries:

```cpp
// src/core/AlignedBuffer.h
#pragma once

#include <cstdlib>
#include <cstring>
#include <memory>

namespace NereusSDR {

static constexpr size_t kBufferAlignment = 32;  // AVX2

// Custom deleter for aligned memory.
struct AlignedDeleter {
    void operator()(float* ptr) const {
        std::free(ptr);
    }
};

using AlignedBuffer = std::unique_ptr<float[], AlignedDeleter>;

// Allocate an aligned float buffer of the given size (in floats).
inline AlignedBuffer makeAlignedBuffer(int floatCount)
{
    void* ptr = std::aligned_alloc(
        kBufferAlignment,
        static_cast<size_t>(floatCount) * sizeof(float));
    std::memset(ptr, 0, static_cast<size_t>(floatCount) * sizeof(float));
    return AlignedBuffer(static_cast<float*>(ptr));
}

} // namespace NereusSDR
```

---

## 9. Spectrum Integration

### 9.1 Spectrum Data Flow

```
QTimer (30 Hz, on main thread)
  |
  v
SpectrumController::onSpectrumTimer()
  |
  |-- For each active RxChannel:
  |     |
  |     |-- AlignedBuffer buf = makeAlignedBuffer(fftSize);
  |     |-- bool ready = rxChannel->getSpectrum(buf.get(), fftSize);
  |     |
  |     |-- if (ready):
  |     |     emit spectrumReady(channelId, QVector<float>(buf, buf + fftSize))
  |     |
  |     +-> SpectrumWidget::updateSpectrum(channelId, data)
  |           |-- Update panadapter trace
  |           +-- Feed waterfall line
  |
  |-- For TX channel (if transmitting):
  |     |-- txChannel->getSpectrum(buf.get(), fftSize)
  |     +-> emit txSpectrumReady(data)
```

### 9.2 Spectrum Configuration Per Channel

```cpp
// Called once during channel setup or when FFT parameters change.
rxChannel->configureSpectrum(
    4096,     // fftSize: number of FFT bins
    6,        // averages: spectrum smoothing (higher = smoother)
    4         // windowType: 4 = Blackman-Harris 4-term
);
```

### 9.3 Primary vs. WDSP Spectrum

NereusSDR uses two spectrum computation paths:

1. **Primary (FFTW3-based):** Raw I/Q samples are fed to a separate FFTW3
   pipeline for the main panadapter/waterfall display. This provides maximum
   FFT size flexibility and is independent of WDSP buffer sizes.

2. **WDSP internal spectrum:** `RXAGetaSipF()` provides spectrum data from
   within the WDSP DSP chain (after filtering/AGC). This is useful for:
   - TX spectrum monitoring during transmit
   - Post-filter signal analysis
   - Comparison with the pre-filter spectrum

Both can run simultaneously on different timers.

---

## 10. Meter Integration

### 10.1 Meter Data Flow

```
QTimer (10 Hz, on main thread)
  |
  v
MeterController::onMeterTimer()
  |
  |-- For each active RxChannel:
  |     |
  |     |-- RxChannel::MeterValues vals = rxChannel->getMeters();
  |     |     // Internally calls:
  |     |     //   GetRXAMeter(ch, S_PK)
  |     |     //   GetRXAMeter(ch, S_AV)
  |     |     //   GetRXAMeter(ch, ADC_PK)
  |     |     //   GetRXAMeter(ch, AGC_GAIN)
  |     |
  |     |-- MeterModel::update(channelId, vals)
  |     |     // Updates S-meter, AGC gain display, ADC overload indicator
  |     |
  |     +-- emit metersChanged(channelId, vals)
  |           |
  |           +-> S-Meter widget updates
  |           +-> AGC gain indicator updates
  |           +-> ADC clip LED (if adcPeak > -1.0 dBFS)
  |
  |-- If TX active:
  |     |
  |     |-- TxChannel::TxMeterValues txVals = txChannel->getMeters();
  |     |     // Reads: MIC_PK, COMP_PK, ALC_PK, OUT_PK, ALC_GAIN, LVLR_GAIN
  |     |
  |     +-- TransmitModel::updateMeters(txVals)
  |           +-> TX meter strip updates (mic, comp, ALC, output)
```

### 10.2 Meter Reading Implementation

WDSP meter reads are lock-free (they read cached atomic values inside
WDSP). They are safe to call from any thread without synchronization.

```cpp
RxChannel::MeterValues RxChannel::getMeters() const
{
    MeterValues v;
#ifdef HAVE_WDSP
    v.signalPeak = GetRXAMeter(m_channelId,
                                static_cast<int>(RxMeterType::SignalPeak));
    v.signalAvg  = GetRXAMeter(m_channelId,
                                static_cast<int>(RxMeterType::SignalAvg));
    v.adcPeak    = GetRXAMeter(m_channelId,
                                static_cast<int>(RxMeterType::AdcPeak));
    v.agcGain    = GetRXAMeter(m_channelId,
                                static_cast<int>(RxMeterType::AgcGain));
#endif
    return v;
}
```

---

## 11. WDSP Channel Mapping

### 11.1 Standard Channel Allocation

Superseded in Phase 3F. The pre-3F table below reserved ids 1-3 for a
diversity / RX2 layout that was never built, and put TX at 4; the shipped
code put TX at 1. Phase 3F replaced both with one slice pool sized off
`BoardCapabilities::maxSlices`, so the reserved block is now derived from
the upstream `chid()` formula instead of hand-assigned.

The ids are constants on `WdspEngine` (`kMaxSliceChannels`,
`kTxChannelId`, `kPsFeedbackChannelId`); read those, not this table, when
writing code. `tests/tst_wdsp_channel_id_map.cpp` pins the invariants.

| WDSP Channel ID | NereusSDR Concept | Type | Default State | Notes |
|-----------------|-------------------|------|---------------|-------|
| 0 | Slice A | RX | Active | Primary receiver. Always created. WDSP channel id == slice index. |
| 1 | Slice B | RX | Inactive until bound | Pooled at connect; activated when a slice binds to a stream. |
| 2 | Slice C | RX | Inactive until bound | Pooled at connect. |
| 3 | Slice D | RX | Inactive until bound | Pooled at connect. |
| 4 | Slice E | RX | Inactive until bound | Pooled at connect. Top of the RX block on a 5-slice SKU. |
| 5 | TX | TX | Inactive | `kTxChannelId` = `cmSubRCVR * cmRCVR` = `1 * kMaxSliceChannels`. Activated on MOX/PTT/VOX. Single TX path. |
| 6 | PureSignal feedback | RX | Inactive | `kPsFeedbackChannelId`. NereusSDR extension; upstream runs PS feedback inside the TX channel. Not available for normal RX. |

A SKU with fewer slices (HL2 `maxSlices=1`, HermesII 2, Atlas 3, Hermes 4)
opens a shorter pool but does **not** move the TX or PS ids: the reserved
block is sized off the ceiling so those two stay compile-time constants,
the same way Thetis's `cmRCVR = 5` is a compile-time maximum rather than
the connected radio's receiver count.

### 11.2 Mapping to Thetis Thread/SubRX Convention

Thetis's `chid(stream, subrx)` (`ChannelMaster/cmsetup.c:176-191`
[v2.10.3.15]) is:

```
rx (stype 0):  ch_id = cmSubRCVR * stream + subrx
tx (stype 1):  ch_id = stream + (cmSubRCVR - 1) * cmRCVR
```

with `cmRCVR = 5`, `cmSubRCVR = 2`, `cmXMTR = 1` (`cmaster.cs:412,419,502`
[v2.10.3.15]), giving Thetis ch[0..9] = RX and ch[10] = TX. The C# mirror
is `WDSP.id(thread, subrx)` at `dsp.cs:926-944` [v2.10.3.15], whose TX case
returns `CMsubrcvr * CMrcvr`.

NereusSDR's radio structure is one WDSP channel per slice with no
sub-receivers, i.e. `cmSubRCVR = 1`, `cmRCVR = kMaxSliceChannels`,
`cmXMTR = 1`. Substituting into the same formula:

```
rx:  ch_id = 1 * slice + 0          = slice
tx:  ch_id = kMaxSliceChannels + 0  = kMaxSliceChannels
```

so the upstream formula reproduces the Phase 3F "WDSP channel id == slice
index" invariant exactly, and TX lands immediately above the RX block.

| Thetis (thread, subrx) | Thetis WDSP Channel | NereusSDR equivalent | NereusSDR WDSP Channel |
|-------------------------|--------------------|----------------------|------------------------|
| (0, 0) | 0 | Slice A | 0 |
| (0, 1) | 1 | Slice B | 1 |
| (2, 0) | 2 | Slice C | 2 |
| (2, 1) | 3 | Slice D | 3 |
| (1, 0) TX | 10 | TX | 5 |

The two columns differ because Thetis reserves two sub-receiver channels
per receiver and NereusSDR reserves one channel per slice. The formula is
the same; only `cmSubRCVR` differs.

### 11.3 SliceModel to Channel Binding

```cpp
// In RadioModel, when creating a slice:
SliceModel* slice = new SliceModel(this);
RxChannel* rx = m_wdspEngine->createRxChannel(channelId, ...);
slice->setWdspChannelId(channelId);

// Wire slice property changes to RxChannel:
connect(slice, &SliceModel::modeChanged, rx, [rx](const QString& mode) {
    rx->setMode(stringToDspMode(mode));
});
connect(slice, &SliceModel::filterChanged, rx, [rx, slice]() {
    rx->setFilterFreqs(slice->filterLow(), slice->filterHigh());
});

// Wire RxChannel signals back to slice (for external updates):
connect(rx, &RxChannel::modeChanged, slice, [slice](DSPMode mode) {
    slice->setMode(dspModeToString(mode));
});
```

---

## 12. Thread Safety Model

### 12.1 Thread Roles

| Thread | Operations | Lock Behavior |
|--------|-----------|---------------|
| **Main** | Create/destroy channels, set DSP parameters, read meters, read spectrum | Calls WDSP setters directly (WDSP has internal locks). Acquires exclusive `m_stateMutex` for mode/rate changes. |
| **Audio** | `processIq()` -- calls `fexchange2()` | Reads `std::atomic` flags. Acquires shared `m_stateMutex` during `fexchange2` (only contested during mode/rate changes). |
| **Spectrum timer** | `getSpectrum()` -- calls `RXAGetaSipF()` | No lock needed (WDSP spectrum reads are internally synchronized). |
| **Meter timer** | `getMeter()` -- calls `GetRXAMeter()` | No lock needed (WDSP meter reads are atomic). |

### 12.2 Lock Hierarchy

```
std::shared_mutex m_stateMutex (per channel)
  |
  |-- Exclusive lock: main thread, only during mode change or
  |   sample rate change (infrequent, <1ms hold time)
  |
  +-- Shared lock: audio thread, during fexchange2 call
      (prevents mode/rate change while processing)
```

The audio thread never blocks on `m_stateMutex` under normal operation.
It only contends during the brief window of a mode or sample rate change,
which happens at human interaction rates (seconds, not milliseconds).

### 12.3 Atomic Parameter Categories

**Atomic (std::atomic, lock-free):**
- Mode enable flags: `m_nrEnabled`, `m_nbEnabled`, `m_anfEnabled`,
  `m_squelchEnabled`, `m_compEnabled`, `m_voxEnabled`
- Mode enum: `m_mode`, `m_agcMode`, `m_nrAlgorithm`
- Channel active flag: `m_active`

**Non-atomic (protected by `m_stateMutex` write lock):**
- Filter frequencies: `m_filterLow`, `m_filterHigh`
- AGC timing parameters: `m_agcAttack`, `m_agcDecay`, `m_agcHang`
- Sample rates: `m_inputSampleRate`, `m_dspSampleRate`, `m_outputSampleRate`

**Rationale:** Enable flags and mode enums are toggled frequently from the
UI and read every buffer cycle by the audio thread. Using `std::atomic`
avoids any lock contention on the hot path. Filter and timing parameters
change less frequently and their values are only needed by WDSP internally
(not read by the audio thread), so they do not need atomic access.

### 12.4 Sample Rate Change Protocol

Sample rate changes are the most disruptive operation because WDSP
recomputes all filter coefficients. The protocol ensures no audio
glitches:

```
Main thread:
  1. Acquire exclusive m_stateMutex
  2. SetChannelState(ch, 0, 1)    -- deactivate with drain
  3. SetAllRates(ch, inRate, dspRate, outRate)
  4. Wait for WDSP filter recalculation (~10-50ms)
  5. SetChannelState(ch, 1, 0)    -- reactivate
  6. Release m_stateMutex

Audio thread:
  - Tries shared lock on m_stateMutex before fexchange2
  - Blocked during steps 2-5 (brief, <50ms)
  - Resumes processing with new rates after step 6
```

---

## 13. Channel Lifecycle

### 13.1 RX Channel Creation

```cpp
RxChannel* WdspEngine::createRxChannel(int channelId, ...)
{
#ifdef HAVE_WDSP
    // 1. Create the WDSP channel (type=0 for RX, state=0 for OFF)
    OpenChannel(channelId, inputBufferSize, dspBufferSize,
                inputSampleRate, dspSampleRate, outputSampleRate,
                0 /*type=RX*/, 0 /*state=OFF*/);

    // 2. Create noise blanker instances (separate lifecycle from channel)
    create_anbEXT(channelId, 0 /*run=off*/, inputBufferSize,
                  inputSampleRate, 0.1 /*tau*/, 0.1 /*hangtime*/,
                  0.1 /*advtime*/, 0.05 /*backtau*/, 3.3 /*threshold - Thetis radio.cs:843*/);

    create_nobEXT(channelId, 0 /*run=off*/, 0 /*mode*/,
                  inputBufferSize, inputSampleRate,
                  0.1, 0.1, 0.1, 0.05, 30.0 /*threshold - Thetis cmaster.c:53*/);
#endif

    // 3. Create the C++ wrapper
    auto channel = std::make_unique<RxChannel>(
        channelId, inputBufferSize, dspBufferSize,
        inputSampleRate, dspSampleRate, outputSampleRate, this);

    RxChannel* ptr = channel.get();
    m_rxChannels.insert(channelId, std::move(channel));

    emit channelCreated(channelId, ChannelType::RX);
    return ptr;
}
```

### 13.2 RX Channel Activation

```cpp
void RxChannel::setActive(bool active)
{
    if (active == m_active.load()) {
        return;
    }

    m_active.store(active);

#ifdef HAVE_WDSP
    if (active) {
        // Activate: state=1, drain=0
        SetChannelState(m_channelId, 1, 0);
    } else {
        // Deactivate: state=0, drain=1 (flush buffers)
        SetChannelState(m_channelId, 0, 1);
    }
#endif

    emit activeChanged(active);
}
```

### 13.3 RX Channel Destruction

```cpp
void WdspEngine::destroyRxChannel(int channelId)
{
    auto it = m_rxChannels.find(channelId);
    if (it == m_rxChannels.end()) {
        return;
    }

    // Deactivate first
    it->second->setActive(false);

#ifdef HAVE_WDSP
    // Destroy NB instances
    destroy_anbEXT(channelId);
    destroy_nobEXT(channelId);

    // Destroy the WDSP channel
    CloseChannel(channelId);
#endif

    m_rxChannels.erase(it);
    emit channelDestroyed(channelId);
}
```

### 13.4 Configuration After Creation

After creating a channel and before activating it, apply the default
(or restored) DSP configuration:

```cpp
// Called by RadioModel after creating a channel for a slice.
void RadioModel::configureChannelDefaults(RxChannel* rx, SliceModel* slice)
{
    rx->setMode(stringToDspMode(slice->mode()));
    rx->setFilterFreqs(slice->filterLow(), slice->filterHigh());
    rx->setAgcMode(AGCMode::Med);
    rx->setAgcTop(90.0);  // From Thetis radio.cs:1018
    rx->setVolume(1.0);
    rx->setPan(0.5);

    // NB, NR, ANF off by default
    rx->setNb1Enabled(false);
    rx->setNb2Enabled(false);
    rx->setNrEnabled(false);
    rx->setAnfEnabled(false);
    rx->setSquelchEnabled(false);

    // Configure spectrum
    rx->configureSpectrum(4096, 6, 4);

    // Now activate
    rx->setActive(true);
}
```

---

## 14. WDSP API Constraints and Quirks

These constraints are derived from the Phase 1C investigation and must
be respected by all code interacting with WDSP.

| # | Constraint | Impact | Mitigation |
|---|-----------|--------|------------|
| 1 | Maximum 32 channels | Limits exotic multi-RX configs | 6 channels (4 RX + 1 TX + 1 PS feedback) is well within limit |
| 2 | ~2-3 MB per channel | ~15 MB for 6 channels | Negligible on modern systems |
| 3 | Mode-dependent filter frequencies | LSB uses negative Hz, USB positive | `RxChannel` translates internally; UI works in positive bandwidth |
| 4 | ANF/NR1 and NR2 mutually exclusive | Cannot enable NR1 + NR2 simultaneously | `RxChannel::setNrAlgorithm()` disables the previous algorithm before enabling the new one |
| 5 | Sample rate changes recompute filters | 10-50ms stall | Deactivate channel during rate change (section 12.4) |
| 6 | Buffer alignment for SIMD | Performance degradation if unaligned | `AlignedBuffer` (section 8.3) for all audio buffers |
| 7 | Notch database shared across channels | Up to 1024 notches total | `WdspEngine` manages the shared database; per-channel notch methods delegate |
| 8 | PureSignal feedback delay <20ms | Must complete feedback loop within one buffer period | Use adequate buffer size; prefer higher sample rates for PS |
| 9 | NB1/NB2 separate lifecycle | `create_anbEXT`/`create_nobEXT` not part of `OpenChannel` | Create/destroy alongside channel in `WdspEngine` |
| 10 | VOX/DEXP separate lifecycle | `create_dexp`/`destroy_dexp` not part of `OpenChannel` | Create/destroy alongside TX channel in `WdspEngine` |
| 11 | FFTW wisdom is CPU-specific | Not portable between machines | Generate on first run; save to `{configDir}/fftw_wisdom` |
| 12 | Impulse cache version-sensitive | Invalid across WDSP version changes | Version-stamp the cache file; regenerate if stale |

---

## 15. Test Generator Integration

WDSP provides built-in test signal generators for radio alignment and
diagnostics. These are exposed through `RxChannel` and `TxChannel` for
development and testing without a physical radio.

```cpp
// Test generator control (on RxChannel or TxChannel)
void setPreGenEnabled(bool enabled);
void setPreGenMode(int mode);   // 0=tone, 1=noise, 2=sweep, 3=gaussian
void setPreGenToneFreq(double freqHz);
void setPreGenToneMag(double magdB);
void setPreGenSweepRange(double freq1Hz, double freq2Hz, double rateHz);

// TX post-generator (for two-tone IMD testing)
void setPostGenEnabled(bool enabled);
void setPostGenMode(int mode);  // 0=tone, 1=noise, 2=sweep, 3=two-tone
void setPostGenTwoTone(double freq1, double freq2, double mag1, double mag2);
```

Test generators inject signals at specific points in the DSP chain:
- **Pre-generator:** Before the DSP processing chain (tests the full chain)
- **Post-generator (TX only):** After the DSP chain (tests output path only)

---

## 16. Implementation Priority

Per the Phase 1C investigation checklist:

| Priority | Scope | WDSP Functions | Status |
|----------|-------|---------------|--------|
| **P0 (MVP)** | Channel mgmt, audio exchange, demod, bandpass, AGC, audio panel, metering, wisdom | OpenChannel, fexchange2, SetRXAMode, SetRXABandpassFreqs, SetRXAAGCMode, SetRXAPanelGain1, GetRXAMeter, WDSPwisdom | Design complete |
| **P1 (Core)** | NB1, NB2, NR1, NR2, ANF, manual notch, squelch, RX/TX EQ | ANB/NOB ext, ANR, EMNR, ANF, NBP notch, AMSQ/FMSQ/SSQL, EQ | Design complete |
| **P2 (TX)** | TX compression chain, VOX, FM TX, TX metering | Compressor, Leveler, ALC, PhaseRot, DEXP, FM deviation/CTCSS, GetTXAMeter | Design complete |
| **P3 (Advanced)** | PureSignal, CFCOMP, RNNR, SBNR, CW filters | pscc, CFCOMP, RNNoise, SBNR, SPcw/BiQuad/Gaussian/Matched | Design complete |
| **P4 (Polish)** | Test generators, spectrum via WDSP, impulse caching, filter optimization | PreGen/PostGen, RXAGetaSipF, impulse cache | Design complete |

---

## 17. File Organization

```
src/core/
  WdspTypes.h         -- Enums (DSPMode, AGCMode, MeterType, etc.)
  WdspEngine.h/.cpp   -- Central WDSP manager
  RxChannel.h/.cpp    -- Per-receiver WDSP channel wrapper
  TxChannel.h/.cpp    -- TX WDSP channel wrapper
  PureSignal.h/.cpp   -- PureSignal PA linearization
  AlignedBuffer.h     -- SIMD-aligned buffer allocation
```

All classes are in the `NereusSDR` namespace. All headers use `#pragma once`.
WDSP calls are guarded by `#ifdef HAVE_WDSP` for graceful degradation
when the WDSP library is not available.
