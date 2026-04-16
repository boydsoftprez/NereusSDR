#pragma once

#include "WdspTypes.h"

#include <QObject>

#include <atomic>
#include <cstring>

namespace NereusSDR {

// Per-receiver WDSP channel wrapper.
//
// Owns one WDSP RX channel and provides Qt property access to DSP
// parameters. Each setter immediately calls the corresponding WDSP
// API function and emits a change signal.
//
// Thread safety:
//   - Main thread: create/destroy, all property setters
//   - Audio thread (future): processIq() calls fexchange2
//   - Meter timer: getMeter() — WDSP meter reads are lock-free
//
// Ported from Thetis cmaster.c create_rcvr / wdsp-integration.md section 4.
class RxChannel : public QObject {
    Q_OBJECT

    Q_PROPERTY(NereusSDR::DSPMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(NereusSDR::AGCMode agcMode READ agcMode WRITE setAgcMode NOTIFY agcModeChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)

public:
    explicit RxChannel(int channelId, int bufferSize, int sampleRate,
                       QObject* parent = nullptr);
    ~RxChannel() override;

    int channelId() const { return m_channelId; }
    int bufferSize() const { return m_bufferSize; }
    int sampleRate() const { return m_sampleRate; }

    // --- Demodulation ---

    DSPMode mode() const { return static_cast<DSPMode>(m_mode.load()); }
    void setMode(DSPMode mode);

    // --- Bandpass filter ---

    void setFilterFreqs(double lowHz, double highHz);
    double filterLow() const { return m_filterLow; }
    double filterHigh() const { return m_filterHigh; }

    // --- AGC ---

    AGCMode agcMode() const { return static_cast<AGCMode>(m_agcMode.load()); }
    void setAgcMode(AGCMode mode);
    void setAgcTop(double topdB);

    // Read back AGC top (max gain in dB) from WDSP after threshold change.
    // From Thetis console.cs:45978 — GetRXAAGCTop after SetRXAAGCThresh
    // Returns clamped value in -20..120 dB range.
    double readBackAgcTop() const;

    // Read back AGC threshold from WDSP after top/RF gain change.
    // From Thetis console.cs:50350 pattern — GetRXAAGCThresh after SetRXAAGCTop
    // Returns clamped value in -160..0 dB range.
    double readBackAgcThresh() const;

    // AGC advanced parameters
    // From Thetis Project Files/Source/Console/radio.cs:1037-1124
    // From Thetis Project Files/Source/Console/console.cs:45977
    int agcThreshold() const { return m_agcThreshold.load(); }
    void setAgcThreshold(int dBu);

    int agcHang() const { return m_agcHang.load(); }
    void setAgcHang(int ms);

    int agcSlope() const { return m_agcSlope.load(); }
    void setAgcSlope(int slope);

    int agcAttack() const { return m_agcAttack.load(); }
    void setAgcAttack(int ms);

    int agcDecay() const { return m_agcDecay.load(); }
    void setAgcDecay(int ms);

    // --- Noise blanker ---

    bool nb1Enabled() const { return m_nb1Enabled.load(); }
    void setNb1Enabled(bool enabled);

    bool nb2Enabled() const { return m_nb2Enabled.load(); }
    void setNb2Enabled(bool enabled);

    // NB2 advanced sub-parameters — set-and-forget on init from Thetis defaults.
    // Declared in Thetis Project Files/Source/Console/HPSDR/specHPSDR.cs:922-937
    // WDSP: third_party/wdsp/src/nobII.c:658,686,697,707
    //
    // mode: 0=zero, 1=sample-hold, 2=mean-hold, 3=hold-sample, 4=interpolate
    // Thetis cmaster.c default: mode=0 (zero — blank to zero on impulse)
    void setNb2Mode(int mode);
    // tau: slew time constant in seconds (advslewtime + hangslewtime)
    // Thetis cmaster.c default: 0.0001 s
    void setNb2Tau(double tau);
    // leadTime (advtime): advance time in seconds before detected impulse
    // Thetis cmaster.c default: 0.0001 s
    void setNb2LeadTime(double time);
    // hangTime: hang time in seconds after blanked impulse
    // Thetis cmaster.c default: 0.0001 s
    void setNb2HangTime(double time);

    // --- Noise reduction ---

    void setNrEnabled(bool enabled);
    void setAnfEnabled(bool enabled);

    // EMNR (NR2) — Enhanced Multiband Noise Reduction
    // From Thetis Project Files/Source/Console/radio.cs:2216-2251
    bool emnrEnabled() const { return m_emnrEnabled.load(); }
    void setEmnrEnabled(bool enabled);
    void setEmnrGainMethod(int method);
    void setEmnrNpeMethod(int method);
    void setEmnrAeRun(bool run);
    void setEmnrPosition(int position);

    // SNB — Spectral Noise Blanker
    // From Thetis Project Files/Source/Console/radio.cs (SetRXASNBARun call site)
    bool snbEnabled() const { return m_snbEnabled.load(); }
    void setSnbEnabled(bool enabled);

    // APF — Audio Peak Filter (CW narrow-peak filter via WDSP SPCW module)
    // From Thetis Project Files/Source/Console/radio.cs:1910-2008
    // WDSP: third_party/wdsp/src/apfshadow.c
    bool apfEnabled() const { return m_apfEnabled.load(); }
    void setApfEnabled(bool enabled);
    void setApfFreq(double hz);
    void setApfBandwidth(double hz);
    void setApfGain(double gain);
    void setApfSelection(int selection);

    // Squelch — SSB (syllabic squelch, WDSP SSQL module)
    // From Thetis Project Files/Source/Console/radio.cs:1185-1229
    // WDSP: third_party/wdsp/src/ssql.c:331,339
    // threshold: 0.0..1.0 linear (Thetis default _fSSqlThreshold = 0.16f)
    bool ssqlEnabled() const { return m_ssqlEnabled.load(); }
    void setSsqlEnabled(bool enabled);
    void setSsqlThresh(double threshold);  // 0.0..1.0 linear

    // Squelch — AM (WDSP AMSQ module)
    // From Thetis Project Files/Source/Console/radio.cs:1164-1178, 1293-1310
    // WDSP: third_party/wdsp/src/amsq.c — threshold in dB
    // threshold: dB domain; WDSP applies pow(10.0, t/20.0) internally
    bool amsqEnabled() const { return m_amsqEnabled.load(); }
    void setAmsqEnabled(bool enabled);
    void setAmsqThresh(double dB);

    // Squelch — FM (WDSP FMSQ module)
    // From Thetis Project Files/Source/Console/radio.cs:1274-1329
    // WDSP: third_party/wdsp/src/fmsq.c:236,244
    // threshold: linear 0..1 (Thetis fm_squelch_threshold = 1.0f, NOT dB)
    // SliceModel m_fmsqThresh{-150.0} is in dB and must be converted:
    //   linear = pow(10.0, dB / 20.0)
    bool fmsqEnabled() const { return m_fmsqEnabled.load(); }
    void setFmsqEnabled(bool enabled);
    void setFmsqThresh(double dB);  // converts dB → linear before WDSP call

    // --- Audio panel (mute / pan / binaural) ---

    // Mute: run=0 silences the audio panel output; run=1 restores it.
    // Maps to SetRXAPanelRun. Default: unmuted (panel runs).
    // From Thetis Project Files/Source/Console/dsp.cs:393-394
    // WDSP: third_party/wdsp/src/patchpanel.c:126
    bool muted() const { return m_muted.load(); }
    void setMuted(bool muted);

    // Audio pan: NereusSDR range -1.0..+1.0 (0.0 = center).
    // Converted to WDSP 0.0..1.0 via wdsp_pan = (pan + 1.0) / 2.0.
    // From Thetis Project Files/Source/Console/radio.cs:1386-1403
    //   Thetis default pan = 0.5f (center in 0..1 scale)
    // WDSP: third_party/wdsp/src/patchpanel.c:159
    void setAudioPan(double pan);  // pan in -1.0..+1.0

    // Binaural mode: enabled → I and Q carry separate headphone channels.
    // Disabled (default) → dual-mono (Q copies I).
    // From Thetis Project Files/Source/Console/radio.cs:1145-1162
    //   Thetis default bin_on = false
    // WDSP: third_party/wdsp/src/patchpanel.c:187
    bool binauralEnabled() const { return m_binauralEnabled.load(); }
    void setBinauralEnabled(bool enabled);

    // --- Frequency shift (for pan offset from VFO) ---

    void setShiftFrequency(double offsetHz);

    // --- Channel state ---

    bool isActive() const { return m_active.load(); }
    void setActive(bool active);

    // --- Audio processing (called from audio thread) ---

    // Process I/Q samples through the WDSP RX chain.
    // Input:  inI/inQ arrays of sampleCount floats (raw I/Q from radio)
    // Output: outI/outQ arrays of sampleCount floats (decoded audio L/R)
    //
    // NB1/NB2 are processed before fexchange2:
    //   xanbEXTF(id, I, Q)  -- if NB1 enabled (in-place)
    //   xnobEXTF(id, I, Q)  -- if NB2 enabled (in-place)
    //   fexchange2(channel, Iin, Qin, Iout, Qout, &error)
    //
    // From Thetis wdsp-integration.md section 4.3
    void processIq(float* inI, float* inQ,
                   float* outI, float* outQ,
                   int sampleCount);

    // --- Metering ---

    double getMeter(RxMeterType type) const;

signals:
    void modeChanged(NereusSDR::DSPMode mode);
    void agcModeChanged(NereusSDR::AGCMode mode);
    void activeChanged(bool active);
    void filterChanged(double low, double high);

private:
    const int m_channelId;
    const int m_bufferSize;
    const int m_sampleRate;

    // Atomic flags for lock-free audio thread reads
    std::atomic<int> m_mode{static_cast<int>(DSPMode::LSB)};  // Must match WdspEngine::createRxChannel init
    std::atomic<int> m_agcMode{static_cast<int>(AGCMode::Med)};
    std::atomic<bool> m_nb1Enabled{false};
    std::atomic<bool> m_nb2Enabled{false};
    std::atomic<bool> m_nrEnabled{false};
    std::atomic<bool> m_anfEnabled{false};
    // emnr: From Thetis radio.cs:2216 — rx_nr2_run default = 0
    std::atomic<bool> m_emnrEnabled{false};
    // snb: Spectral Noise Blanker — off by default
    std::atomic<bool> m_snbEnabled{false};
    // apf: Audio Peak Filter — off by default
    // From Thetis radio.cs:1910 — rx_apf_run default = false
    std::atomic<bool> m_apfEnabled{false};
    // ssql: SSB syllabic squelch — off by default
    // From Thetis radio.cs:1185 — _bSSqlOn default = false
    std::atomic<bool> m_ssqlEnabled{false};
    // amsq: AM squelch — off by default
    // From Thetis radio.cs:1293 — rx_am_squelch_on default = false
    std::atomic<bool> m_amsqEnabled{false};
    // fmsq: FM squelch — off by default
    // From Thetis radio.cs:1312 — rx_fm_squelch_on default = false
    std::atomic<bool> m_fmsqEnabled{false};
    // muted: audio panel mute — off by default (panel runs)
    // From Thetis Project Files/Source/Console/dsp.cs:393-394
    std::atomic<bool> m_muted{false};
    // binauralEnabled: binaural audio — off by default (dual-mono)
    // From Thetis radio.cs:1145-1162 — bin_on = false
    std::atomic<bool> m_binauralEnabled{false};
    std::atomic<bool> m_active{false};

    // AGC advanced parameters — atomic for thread-safe reads from audio thread
    // Defaults from Thetis Project Files/Source/Console/radio.cs:1037-1124
    // threshold default: From Thetis console.cs:45977 — agc_thresh_point = -20
    std::atomic<int> m_agcThreshold{-20};
    // hang: From Thetis radio.cs:1056-1057 — rx_agc_hang = 250 ms
    std::atomic<int> m_agcHang{250};
    // slope: From Thetis radio.cs:1107-1108 — rx_agc_slope = 0
    std::atomic<int> m_agcSlope{0};
    // attack: From WDSP wcpAGC.c create_wcpagc — tau_attack default 2 ms
    std::atomic<int> m_agcAttack{2};
    // decay: From Thetis radio.cs:1037-1038 — rx_agc_decay = 250 ms
    std::atomic<int> m_agcDecay{250};

    // Cached filter state
    double m_filterLow{150.0};
    double m_filterHigh{2850.0};
};

} // namespace NereusSDR
