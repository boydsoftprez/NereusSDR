#pragma once

#include "Band.h"
#include "core/WdspTypes.h"

#include <QObject>
#include <QString>

#include <utility>

namespace NereusSDR {

// Stage 1 stub ladder — Stage 2 replaces with Thetis tune_step_list
// (console.cs tune_step_list has 11 entries; Stage 1 uses this 5-entry
// subset only for the X/RIT STEP cycle button).
inline constexpr int kStageOneStepLadder[] = {1, 10, 100, 1000, 10000};
inline constexpr int kStageOneStepLadderSize =
    static_cast<int>(sizeof(kStageOneStepLadder) / sizeof(kStageOneStepLadder[0]));

// Represents a single receiver slice.
// In NereusSDR, slices are a client-side abstraction — the radio has
// no concept of slices. Each slice owns a WDSP channel for independent
// DSP processing.
//
// The slice is the single source of truth for VFO state (frequency,
// mode, filter, AGC, gains, antenna). Changes propagate to WDSP and
// the radio via signals wired in RadioModel.
//
// From AetherSDR SliceModel pattern: Q_PROPERTY + signals for each state.
class SliceModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(double     frequency    READ frequency    WRITE setFrequency    NOTIFY frequencyChanged)
    Q_PROPERTY(NereusSDR::DSPMode dspMode READ dspMode   WRITE setDspMode      NOTIFY dspModeChanged)
    Q_PROPERTY(int        filterLow    READ filterLow    WRITE setFilterLow    NOTIFY filterChanged)
    Q_PROPERTY(int        filterHigh   READ filterHigh   WRITE setFilterHigh   NOTIFY filterChanged)
    Q_PROPERTY(NereusSDR::AGCMode agcMode READ agcMode   WRITE setAgcMode      NOTIFY agcModeChanged)
    Q_PROPERTY(int        stepHz       READ stepHz       WRITE setStepHz       NOTIFY stepHzChanged)
    Q_PROPERTY(int        afGain       READ afGain       WRITE setAfGain       NOTIFY afGainChanged)
    Q_PROPERTY(int        rfGain       READ rfGain       WRITE setRfGain       NOTIFY rfGainChanged)
    Q_PROPERTY(QString    rxAntenna    READ rxAntenna    WRITE setRxAntenna    NOTIFY rxAntennaChanged)
    Q_PROPERTY(QString    txAntenna    READ txAntenna    WRITE setTxAntenna    NOTIFY txAntennaChanged)
    Q_PROPERTY(bool       active       READ isActive     NOTIFY activeChanged)
    Q_PROPERTY(bool       txSlice      READ isTxSlice    NOTIFY txSliceChanged)

    // ── Phase 3G-10 Stage 1 stubs (DSP state, Stage 2 wires to RxChannel) ──
    Q_PROPERTY(bool   locked          READ locked          WRITE setLocked          NOTIFY lockedChanged)
    Q_PROPERTY(bool   muted           READ muted           WRITE setMuted           NOTIFY mutedChanged)
    Q_PROPERTY(double audioPan        READ audioPan        WRITE setAudioPan        NOTIFY audioPanChanged)
    Q_PROPERTY(bool   ssqlEnabled     READ ssqlEnabled     WRITE setSsqlEnabled     NOTIFY ssqlEnabledChanged)
    Q_PROPERTY(double ssqlThresh      READ ssqlThresh      WRITE setSsqlThresh      NOTIFY ssqlThreshChanged)
    Q_PROPERTY(bool   amsqEnabled     READ amsqEnabled     WRITE setAmsqEnabled     NOTIFY amsqEnabledChanged)
    Q_PROPERTY(double amsqThresh      READ amsqThresh      WRITE setAmsqThresh      NOTIFY amsqThreshChanged)
    Q_PROPERTY(bool   fmsqEnabled     READ fmsqEnabled     WRITE setFmsqEnabled     NOTIFY fmsqEnabledChanged)
    Q_PROPERTY(double fmsqThresh      READ fmsqThresh      WRITE setFmsqThresh      NOTIFY fmsqThreshChanged)
    Q_PROPERTY(int    agcThreshold    READ agcThreshold    WRITE setAgcThreshold    NOTIFY agcThresholdChanged)
    Q_PROPERTY(int    agcHang         READ agcHang         WRITE setAgcHang         NOTIFY agcHangChanged)
    Q_PROPERTY(int    agcSlope        READ agcSlope        WRITE setAgcSlope        NOTIFY agcSlopeChanged)
    Q_PROPERTY(int    agcAttack       READ agcAttack       WRITE setAgcAttack       NOTIFY agcAttackChanged)
    Q_PROPERTY(int    agcDecay        READ agcDecay        WRITE setAgcDecay        NOTIFY agcDecayChanged)
    Q_PROPERTY(bool   ritEnabled      READ ritEnabled      WRITE setRitEnabled      NOTIFY ritEnabledChanged)
    Q_PROPERTY(int    ritHz           READ ritHz           WRITE setRitHz           NOTIFY ritHzChanged)
    Q_PROPERTY(bool   xitEnabled      READ xitEnabled      WRITE setXitEnabled      NOTIFY xitEnabledChanged)
    Q_PROPERTY(int    xitHz           READ xitHz           WRITE setXitHz           NOTIFY xitHzChanged)
    Q_PROPERTY(bool   nb2Enabled      READ nb2Enabled      WRITE setNb2Enabled      NOTIFY nb2EnabledChanged)
    Q_PROPERTY(bool   emnrEnabled     READ emnrEnabled     WRITE setEmnrEnabled     NOTIFY emnrEnabledChanged)
    Q_PROPERTY(bool   snbEnabled      READ snbEnabled      WRITE setSnbEnabled      NOTIFY snbEnabledChanged)
    Q_PROPERTY(bool   apfEnabled      READ apfEnabled      WRITE setApfEnabled      NOTIFY apfEnabledChanged)
    Q_PROPERTY(int    apfTuneHz       READ apfTuneHz       WRITE setApfTuneHz       NOTIFY apfTuneHzChanged)
    Q_PROPERTY(bool   binauralEnabled READ binauralEnabled WRITE setBinauralEnabled NOTIFY binauralEnabledChanged)
    Q_PROPERTY(int    fmCtcssMode     READ fmCtcssMode     WRITE setFmCtcssMode     NOTIFY fmCtcssModeChanged)
    Q_PROPERTY(double fmCtcssValueHz  READ fmCtcssValueHz  WRITE setFmCtcssValueHz  NOTIFY fmCtcssValueHzChanged)
    Q_PROPERTY(int    fmOffsetHz      READ fmOffsetHz      WRITE setFmOffsetHz      NOTIFY fmOffsetHzChanged)
    Q_PROPERTY(NereusSDR::FmTxMode fmTxMode READ fmTxMode WRITE setFmTxMode NOTIFY fmTxModeChanged)
    Q_PROPERTY(bool   fmReverse       READ fmReverse       WRITE setFmReverse       NOTIFY fmReverseChanged)
    Q_PROPERTY(int    diglOffsetHz    READ diglOffsetHz    WRITE setDiglOffsetHz    NOTIFY diglOffsetHzChanged)
    Q_PROPERTY(int    diguOffsetHz    READ diguOffsetHz    WRITE setDiguOffsetHz    NOTIFY diguOffsetHzChanged)
    Q_PROPERTY(int    rttyMarkHz      READ rttyMarkHz      WRITE setRttyMarkHz      NOTIFY rttyMarkHzChanged)
    Q_PROPERTY(int    rttyShiftHz     READ rttyShiftHz     WRITE setRttyShiftHz     NOTIFY rttyShiftHzChanged)

public:
    explicit SliceModel(QObject* parent = nullptr);
    ~SliceModel() override;

    // ---- Frequency ----

    double frequency() const { return m_frequency; }
    void setFrequency(double freq);

    // ---- Demodulation mode ----

    DSPMode dspMode() const { return m_dspMode; }
    // Sets mode AND applies the default filter for that mode.
    // Emits dspModeChanged + filterChanged.
    void setDspMode(DSPMode mode);

    // ---- Bandpass filter ----

    int filterLow() const { return m_filterLow; }
    void setFilterLow(int low);

    int filterHigh() const { return m_filterHigh; }
    void setFilterHigh(int high);

    // Set both filter edges atomically. Emits filterChanged once.
    void setFilter(int low, int high);

    // ---- AGC ----

    AGCMode agcMode() const { return m_agcMode; }
    void setAgcMode(AGCMode mode);

    // ---- Tuning step ----

    int stepHz() const { return m_stepHz; }
    void setStepHz(int hz);

    // ---- Gains ----

    int afGain() const { return m_afGain; }
    void setAfGain(int gain);

    int rfGain() const { return m_rfGain; }
    void setRfGain(int gain);

    // ---- Antenna selection ----
    // Per-slice RX/TX antenna (e.g., "ANT1", "ANT2", "ANT3").
    // Maps to Alex antenna register bits on OpenHPSDR radios.
    // From AetherSDR SliceModel::rxAntenna/txAntenna pattern.

    QString rxAntenna() const { return m_rxAntenna; }
    void setRxAntenna(const QString& ant);

    QString txAntenna() const { return m_txAntenna; }
    void setTxAntenna(const QString& ant);

    // ---- Slice identity ----

    bool isActive() const { return m_active; }
    void setActive(bool active);

    bool isTxSlice() const { return m_txSlice; }
    void setTxSlice(bool tx);

    int sliceIndex() const { return m_sliceIndex; }
    void setSliceIndex(int idx) { m_sliceIndex = idx; }

    // Panadapter assignment (-1 = unassigned)
    int panId() const { return m_panId; }
    void setPanId(int id) { m_panId = id; }

    // Which receiver/DDC this slice feeds (-1 = unassigned)
    int receiverIndex() const { return m_receiverIndex; }
    void setReceiverIndex(int idx) { m_receiverIndex = idx; }

    int wdspChannelId() const { return m_wdspChannelId; }
    void setWdspChannelId(int id) { m_wdspChannelId = id; }

    // ── Phase 3G-10 Stage 1 stubs (DSP state, Stage 2 wires to RxChannel) ──

    bool   locked()          const { return m_locked; }
    void   setLocked(bool v);

    bool   muted()           const { return m_muted; }
    void   setMuted(bool v);

    double audioPan()        const { return m_audioPan; }
    void   setAudioPan(double pan);

    bool   ssqlEnabled()     const { return m_ssqlEnabled; }
    void   setSsqlEnabled(bool v);

    double ssqlThresh()      const { return m_ssqlThresh; }
    void   setSsqlThresh(double dB);

    bool   amsqEnabled()     const { return m_amsqEnabled; }
    void   setAmsqEnabled(bool v);

    double amsqThresh()      const { return m_amsqThresh; }
    void   setAmsqThresh(double dB);

    bool   fmsqEnabled()     const { return m_fmsqEnabled; }
    void   setFmsqEnabled(bool v);

    double fmsqThresh()      const { return m_fmsqThresh; }
    void   setFmsqThresh(double dB);

    int    agcThreshold()    const { return m_agcThreshold; }
    void   setAgcThreshold(int dBu);

    int    agcHang()         const { return m_agcHang; }
    void   setAgcHang(int ms);

    int    agcSlope()        const { return m_agcSlope; }
    void   setAgcSlope(int dB);

    int    agcAttack()       const { return m_agcAttack; }
    void   setAgcAttack(int ms);

    int    agcDecay()        const { return m_agcDecay; }
    void   setAgcDecay(int ms);

    bool   ritEnabled()      const { return m_ritEnabled; }
    void   setRitEnabled(bool v);

    int    ritHz()           const { return m_ritHz; }
    void   setRitHz(int hz);

    bool   xitEnabled()      const { return m_xitEnabled; }
    void   setXitEnabled(bool v);

    int    xitHz()           const { return m_xitHz; }
    void   setXitHz(int hz);

    bool   nb2Enabled()      const { return m_nb2Enabled; }
    void   setNb2Enabled(bool v);

    bool   emnrEnabled()     const { return m_emnrEnabled; }
    void   setEmnrEnabled(bool v);

    bool   snbEnabled()      const { return m_snbEnabled; }
    void   setSnbEnabled(bool v);

    bool   apfEnabled()      const { return m_apfEnabled; }
    void   setApfEnabled(bool v);

    int    apfTuneHz()       const { return m_apfTuneHz; }
    void   setApfTuneHz(int hz);

    bool   binauralEnabled() const { return m_binauralEnabled; }
    void   setBinauralEnabled(bool v);

    int    fmCtcssMode()     const { return m_fmCtcssMode; }
    void   setFmCtcssMode(int mode);

    double fmCtcssValueHz()  const { return m_fmCtcssValueHz; }
    void   setFmCtcssValueHz(double hz);

    int    fmOffsetHz()      const { return m_fmOffsetHz; }
    void   setFmOffsetHz(int hz);

    FmTxMode fmTxMode()        const { return m_fmTxMode; }
    void   setFmTxMode(FmTxMode mode);

    bool   fmReverse()       const { return m_fmReverse; }
    void   setFmReverse(bool v);

    int    diglOffsetHz()    const { return m_diglOffsetHz; }
    void   setDiglOffsetHz(int hz);

    int    diguOffsetHz()    const { return m_diguOffsetHz; }
    void   setDiguOffsetHz(int hz);

    int    rttyMarkHz()      const { return m_rttyMarkHz; }
    void   setRttyMarkHz(int hz);

    int    rttyShiftHz()     const { return m_rttyShiftHz; }
    void   setRttyShiftHz(int hz);

    // ---- RIT helper ----
    // Effective receive frequency = base frequency + RIT offset (when enabled).
    // This is the frequency fed to the WDSP shift path for demodulation.
    // RIT is a demodulation offset, NOT a VFO/hardware frequency change.
    // From Thetis console.cs — RIT applies a receive-side offset without
    // retuning the hardware VFO.
    double effectiveRxFrequency() const {
        return m_frequency + (m_ritEnabled ? static_cast<double>(m_ritHz) : 0.0);
    }

    // ---- Per-mode filter defaults ----
    // Returns the F5 (default) filter low/high for a given mode.
    // Ported from Thetis console.cs:5180-5575 InitFilterPresets.
    static std::pair<int, int> defaultFilterForMode(DSPMode mode);

    // Returns human-readable mode name (e.g., DSPMode::LSB → "LSB")
    static QString modeName(DSPMode mode);

    // Returns DSPMode from name string (e.g., "LSB" → DSPMode::LSB)
    static DSPMode modeFromName(const QString& name);

    // ── Phase 3G-10 Stage 2: per-slice-per-band persistence ──────────────────
    //
    // Key namespace (see AppSettings.h §Per-slice-per-band DSP state):
    //   Per-band DSP: Slice<N>/Band<key>/AgcThreshold  etc.
    //   Session state: Slice<N>/Locked  etc.
    //
    // saveToSettings(band):
    //   Writes per-band DSP keys and session-state keys to AppSettings.
    //   Does NOT call AppSettings::save() — caller is responsible for flushing.
    //
    // restoreFromSettings(band):
    //   Reads per-band DSP keys and session-state keys from AppSettings.
    //   Missing keys fall back to current SliceModel defaults (no change).
    //
    // migrateLegacyKeys():
    //   One-shot migration. Checks for the legacy "VfoFrequency" flat key.
    //   If found, migrates to Slice0/Band<current>/... using the persisted
    //   frequency to derive the band. Then removes all legacy Vfo* keys.
    //   Call once on startup, before restoreFromSettings().
    void saveToSettings(NereusSDR::Band band);
    void restoreFromSettings(NereusSDR::Band band);
    static void migrateLegacyKeys();

signals:
    void frequencyChanged(double freq);
    void dspModeChanged(NereusSDR::DSPMode mode);
    void filterChanged(int low, int high);
    void agcModeChanged(NereusSDR::AGCMode mode);
    void stepHzChanged(int hz);
    void afGainChanged(int gain);
    void rfGainChanged(int gain);
    void rxAntennaChanged(const QString& ant);
    void txAntennaChanged(const QString& ant);
    void activeChanged(bool active);
    void txSliceChanged(bool tx);

    // ── Phase 3G-10 Stage 1 stubs ──
    void lockedChanged(bool v);
    void mutedChanged(bool v);
    void audioPanChanged(double pan);
    void ssqlEnabledChanged(bool v);
    void ssqlThreshChanged(double dB);
    void amsqEnabledChanged(bool v);
    void amsqThreshChanged(double dB);
    void fmsqEnabledChanged(bool v);
    void fmsqThreshChanged(double dB);
    void agcThresholdChanged(int dBu);
    void agcHangChanged(int ms);
    void agcSlopeChanged(int dB);
    void agcAttackChanged(int ms);
    void agcDecayChanged(int ms);
    void ritEnabledChanged(bool v);
    void ritHzChanged(int hz);
    void xitEnabledChanged(bool v);
    void xitHzChanged(int hz);
    void nb2EnabledChanged(bool v);
    void emnrEnabledChanged(bool v);
    void snbEnabledChanged(bool v);
    void apfEnabledChanged(bool v);
    void apfTuneHzChanged(int hz);
    void binauralEnabledChanged(bool v);
    void fmCtcssModeChanged(int mode);
    void fmCtcssValueHzChanged(double hz);
    void fmOffsetHzChanged(int hz);
    void fmTxModeChanged(NereusSDR::FmTxMode mode);
    void fmReverseChanged(bool v);
    void diglOffsetHzChanged(int hz);
    void diguOffsetHzChanged(int hz);
    void rttyMarkHzChanged(int hz);
    void rttyShiftHzChanged(int hz);

private:
    double  m_frequency{14225000.0};     // Default: 14.225 MHz (20m USB)
    DSPMode m_dspMode{DSPMode::USB};
    int     m_filterLow{100};            // USB default from Thetis F5
    int     m_filterHigh{3000};
    AGCMode m_agcMode{AGCMode::Med};
    int     m_stepHz{100};               // From Thetis tune_step_list[5] = 100 Hz
    int     m_afGain{50};                // 0-100, maps to 0.0-1.0 volume
    int     m_rfGain{80};                // 0-100, maps to AGC gain
    QString m_rxAntenna{QStringLiteral("ANT1")};
    QString m_txAntenna{QStringLiteral("ANT1")};
    bool    m_active{false};
    bool    m_txSlice{false};
    int     m_sliceIndex{0};
    int     m_panId{-1};
    int     m_receiverIndex{-1};
    int     m_wdspChannelId{-1};

    // ── Phase 3G-10 Stage 1 stubs (DSP state, Stage 2 wires to RxChannel) ──
    bool   m_locked{false};           // Neutral default — no Thetis citation needed
    bool   m_muted{false};            // Neutral default — no Thetis citation needed
    double m_audioPan{0.0};           // Neutral default — center pan (−1..+1), no Thetis citation needed
    bool   m_ssqlEnabled{false};      // Neutral default — feature off at start
    double m_ssqlThresh{16.0};        // Slider units 0–100; From Thetis radio.cs:1187 _fSSqlThreshold = 0.16f → 16 in slider scale
    bool   m_amsqEnabled{false};      // Neutral default — feature off at start
    double m_amsqThresh{-150.0};      // From Thetis radio.cs:1164-1165 — rx_squelch_threshold = -150.0f (AM reuses same field)
    bool   m_fmsqEnabled{false};      // Neutral default — feature off at start
    double m_fmsqThresh{-150.0};      // dB domain (plan default); Thetis radio.cs:1274-1275 stores fm_squelch_threshold = 1.0f on a different (linear 0..1) scale. Scale reconciliation deferred to Stage 2 gate.
    int    m_agcThreshold{-20};       // Thetis default — citation pending Stage 2 gate (no explicit default found in radio.cs AGCThreshold)
    int    m_agcHang{250};            // From Thetis radio.cs:1056-1057 — rx_agc_hang = 250 ms
    int    m_agcSlope{0};             // From Thetis radio.cs:1107-1108 — rx_agc_slope = 0 dB
    int    m_agcAttack{2};            // Thetis default — citation pending Stage 2 gate (AGC attack not exposed as explicit property in radio.cs)
    int    m_agcDecay{250};           // From Thetis radio.cs:1037-1038 — rx_agc_decay = 250 ms
    bool   m_ritEnabled{false};       // Neutral default — no Thetis citation needed
    int    m_ritHz{0};                // Neutral default — zero offset
    bool   m_xitEnabled{false};       // Neutral default — no Thetis citation needed
    int    m_xitHz{0};                // Neutral default — zero offset
    bool   m_nb2Enabled{false};       // Neutral default — feature off at start (S1.6 gap-fill)
    bool   m_emnrEnabled{false};      // Neutral default — feature off at start
    bool   m_snbEnabled{false};       // Neutral default — feature off at start
    bool   m_apfEnabled{false};       // Neutral default — feature off at start
    int    m_apfTuneHz{0};            // Neutral default — zero tune offset
    bool   m_binauralEnabled{false};  // Neutral default — feature off at start
    int    m_fmCtcssMode{0};          // Neutral default — Off (0 = disabled)
    double m_fmCtcssValueHz{100.0};   // From Thetis console.cs:40500 — ctcss_freq = 100.0; radio.cs:2899 — ctcss_freq_hz = 100.0
    int      m_fmOffsetHz{0};           // Neutral default — zero offset
    FmTxMode m_fmTxMode{FmTxMode::Simplex};  // From Thetis console.cs:20873 — current_fm_tx_mode = FMTXMode.Simplex
    bool     m_fmReverse{false};        // Neutral default — normal direction
    int      m_diglOffsetHz{0};         // From Thetis console.cs:14672 — DIGLClickTuneOffset default 0 Hz
    int      m_diguOffsetHz{0};         // From Thetis console.cs:14637 — DIGUClickTuneOffset default 0 Hz
    int    m_rttyMarkHz{2295};        // From Thetis setup.designer.cs:40635 — tooltip "RTTY MARK frequency" on udDSPRX1DollyF1; value 2295 (line 40637). F0=2125 is SPACE (line 40665).
    int    m_rttyShiftHz{170};        // From Thetis radio.cs:2043-2044 — rx_dolly_freq1 = 2295, rx_dolly_freq0 = 2125 → shift = 2295−2125 = 170 Hz
};

} // namespace NereusSDR
