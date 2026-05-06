#pragma once

// =================================================================
// src/gui/setup/AudioTxInputPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → TX Input page.
// No Thetis port, no attribution headers required (per memory:
// feedback_source_first_ui_vs_dsp — Qt widgets in Setup pages are
// NereusSDR-native).
//
// Phase 3M-1b Task I.1 (2026-04-28): Top-level PC Mic / Radio Mic
// radio buttons. Radio Mic disabled with tooltip
// "Radio mic jack not present on Hermes Lite 2" when
// BoardCapabilities::hasMicJack == false. Selection persists through
// TransmitModel::micSource (new property; AppSettings persistence
// deferred to L.2).
//
// Phase 3M-1b Task I.2 (2026-04-28): PC Mic group box with 5 rows:
//   Row 1 — Backend selector (CoreAudio/WASAPI/ALSA/PW etc.)
//   Row 2 — Device picker (repopulated on backend change)
//   Row 3 — Buffer-size slider with ms-latency readout
//   Row 4 — Test Mic button + live VU bar (10 ms QTimer, bus-tap)
//   Row 5 — Mic Gain slider mirroring TxApplet (bidirectional sync)
// PC Mic group is only visible when PC Mic radio button is selected.
// TransmitModel session state: pcMicHostApiIndex, pcMicDeviceName,
// pcMicBufferSamples (transient; AppSettings persistence deferred L.2).
//
// Phase 3M-1b Task I.3 (2026-04-28): Radio Mic settings group with
// per-family layout, capability-gated. Visible only when
// MicSource::Radio AND caps.hasMicJack == true. Three sub-layouts:
//   Hermes/Atlas/HermesII/Angelia family — QGroupBox "Radio Mic — Hermes / Atlas":
//     Row 1: Mic In / Line In radio buttons (TransmitModel::lineIn)
//     Row 2: +20 dB Mic Boost checkbox (TransmitModel::micBoost)
//     Row 3: Line In Gain slider -34..+12 dB (TransmitModel::lineInBoost)
//   Orion-MkII family (Orion/OrionMKII) — QGroupBox "Radio Mic — Orion-MkII":
//     4 checkboxes: Mic Tip-Ring, Mic Bias, Mic PTT Disabled, +20 dB Mic Boost
//   Saturn G2 — QGroupBox "Radio Mic — Saturn G2":
//     Row 1: 3.5 mm / XLR radio buttons (TransmitModel::micXlr)
//     Rows 2-4: Mic PTT Disabled, Mic Bias, +20 dB Mic Boost checkboxes
// HL2 hides all three groups (hasMicJack=false). HW-family discriminated
// via BoardCapabilities::board (HPSDRHW enum).
//
// Phase 3M-1b Task I.4 (2026-04-28): Mic Gain slider reads per-board
//   range from BoardCapabilities::micGainMinDb / micGainMaxDb.
//   All known boards: -40/+10 (Thetis console.cs:19151-19171 [v2.10.3.13]
//   runtime defaults). Unknown board: -50/+70 (TransmitModel fallback).
//   Initial value clamped to range at construction.
//   Cite: Pre-code review §5.4.
//
// Design spec: docs/architecture/phase3m-1b-mic-ssb-voice-plan.md
// §3 Phase I (I.1–I.4) + pre-code review §5.1 + §5.4.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-28 — I.1 written by J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code.
//   2026-04-28 — I.2 PC Mic group box written by J.J. Boyd (KG4VCF),
//                with AI-assisted implementation via Anthropic Claude Code.
//   2026-04-28 — I.3 Radio Mic per-family group boxes written by
//                J.J. Boyd (KG4VCF), with AI-assisted implementation
//                via Anthropic Claude Code.
//   2026-04-28 — I.4 Per-board mic gain range written by J.J. Boyd (KG4VCF),
//                with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#include "gui/SetupPage.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/HpsdrModel.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>

namespace NereusSDR {

class HGauge;

// ---------------------------------------------------------------------------
// AudioTxInputPage — Setup → Audio → TX Input
//
// Top-level mic-source selector (I.1):
//   • "PC Mic"    — always enabled; default.
//   • "Radio Mic" — disabled with tooltip when hasMicJack == false (HL2).
//
// PC Mic group box (I.2) — visible only when PC Mic is selected:
//   Row 1: Backend selector (CoreAudio / WASAPI / ALSA / PipeWire / …)
//   Row 2: Device picker (repopulated on backend change)
//   Row 3: Buffer-size slider + ms-latency readout (48 kHz reference)
//   Row 4: Test Mic button + HGauge VU bar (10 ms QTimer bus-tap)
//   Row 5: Mic Gain slider (bidirectional mirror with TxApplet)
//
// Selection change calls TransmitModel::setMicSource. Model changes
// (setMicSource from elsewhere) drive the radio-button check state via
// micSourceChanged signal connection. Two-way sync uses m_updatingFromModel
// guard to prevent echo loops.
//
// Capability gating is static at construction time: hasMicJack is read
// from RadioModel::boardCapabilities() once in the constructor and the
// Radio Mic button enabled-state is fixed. Dynamic capability change
// (reconnect to a different board type) is deferred.
// TODO [3M-1b I.x]: dynamic hasMicJack refresh on currentRadioChanged,
// once RadioModel emits a capability-change signal.
//
// Test Mic implementation (bus-tap approach):
//   On button press, a 10 ms QTimer polls AudioEngine::pcMicInputLevel()
//   (which reads PortAudioBus::m_txLevel, updated by the PA callback).
//   No separate capture stream is opened; the existing TX-input bus is
//   reused. If m_txInputBus is null or not open, pcMicInputLevel() returns
//   0.0f and the VU bar shows silent.
//   TODO [3M-1b I.x]: connect Test Mic to PCMicSource open/close so the
//   capture stream is started on demand when the test mic is active.
// ---------------------------------------------------------------------------
class AudioTxInputPage : public SetupPage {
    Q_OBJECT
public:
    explicit AudioTxInputPage(RadioModel* model, QWidget* parent = nullptr);
    ~AudioTxInputPage() override;

    // Expose the PC Mic group box for test introspection.
    QGroupBox* pcMicGroupBox() const { return m_pcMicGroup; }

    // Expose per-row widgets for test probes.
    QComboBox*   backendCombo()    const { return m_backendCombo; }
    QComboBox*   deviceCombo()     const { return m_deviceCombo; }
    QSlider*     bufferSlider()    const { return m_bufferSlider; }
    QLabel*      bufferLabel()     const { return m_bufferLabel; }
    QPushButton* testMicButton()   const { return m_testMicBtn; }
    HGauge*      vuBar()           const { return m_vuBar; }
    QSlider*     micGainSlider()   const { return m_micGainSlider; }

    // Expose Radio Mic per-family group boxes for test introspection (I.3).
    QGroupBox* hermesRadioMicGroup() const { return m_hermesGroup; }
    QGroupBox* orionRadioMicGroup()  const { return m_orionGroup; }
    QGroupBox* saturnRadioMicGroup() const { return m_saturnGroup; }

private slots:
    void onMicSourceButtonToggled(int id, bool checked);
    void onModelMicSourceChanged(MicSource source);

    void onBackendChanged(int comboIndex);
    void onDeviceChanged(int comboIndex);
    void onBufferSliderChanged(int value);
    void onTestMicToggled(bool checked);
    void onVuTimerTick();

    void onMicGainSliderChanged(int value);
    void onModelMicGainDbChanged(int dB);

    // ── Radio Mic per-family slots (I.3) ──────────────────────────────────────
    // Hermes/Atlas family
    void onHermesMicInputToggled(int id, bool checked);
    void onHermesMicBoostToggled(bool on);
    void onHermesLineInGainChanged(int sliderValue);

    // Orion-MkII family
    void onOrionMicTipRingToggled(bool on);
    void onOrionMicBiasToggled(bool on);
    void onOrionMicPttDisabledToggled(bool on);
    void onOrionMicBoostToggled(bool on);

    // Saturn G2 family
    void onSaturnMicInputToggled(int id, bool checked);
    void onSaturnMicPttDisabledToggled(bool on);
    void onSaturnMicBiasToggled(bool on);
    void onSaturnMicBoostToggled(bool on);

    // Model → UI: radio mic flag changes (all families, I.3)
    void onModelLineInChanged(bool on);
    void onModelMicBoostChanged(bool on);
    void onModelLineInBoostChanged(double dB);
    void onModelMicTipRingChanged(bool on);
    void onModelMicBiasChanged(bool on);
    void onModelMicPttDisabledChanged(bool on);
    void onModelMicXlrChanged(bool on);

private:
    void buildPage(bool hasMicJack, HPSDRHW hw);
    void buildPcMicGroup(QVBoxLayout* parentLayout);
    void buildHermesRadioMicGroup(QVBoxLayout* parentLayout);
    void buildOrionRadioMicGroup(QVBoxLayout* parentLayout);
    void buildSaturnRadioMicGroup(QVBoxLayout* parentLayout);
    void syncButtonsFromModel(MicSource source);
    void populateBackendCombo();
    void populateDeviceCombo(int hostApiIndex);
    void updateBufferLabel(int samples);
    void updatePcMicGroupVisibility(MicSource source);
    void updateRadioMicGroupVisibility(MicSource source, HPSDRHW hw);
    static QString lineInBoostLabel(int sliderValue);

    // Returns the latency string for `samples` samples at 48 kHz reference.
    static QString latencyString(int samples);

    // Returns the OS-default PortAudio host API index.
    static int defaultHostApiIndex();

    // ── Source selector (I.1) ─────────────────────────────────────────────
    QButtonGroup*  m_buttonGroup{nullptr};
    QRadioButton*  m_pcMicBtn{nullptr};
    QRadioButton*  m_radioMicBtn{nullptr};
    // VAX TX virtual device — added 2026-05-06 (eager-borg-d64bed).
    // Selecting routes the TX mic to /nereussdr-vax-tx shm fed by
    // 3rd-party apps writing to "NereusSDR TX" CoreAudio device.
    QRadioButton*  m_vaxMicBtn{nullptr};

    // ── PC Mic group box (I.2) ────────────────────────────────────────────
    QGroupBox*   m_pcMicGroup{nullptr};

    // Row 1: Backend
    QComboBox*   m_backendCombo{nullptr};

    // Row 2: Device
    QComboBox*   m_deviceCombo{nullptr};

    // Row 3: Buffer size
    QSlider*     m_bufferSlider{nullptr};
    QLabel*      m_bufferLabel{nullptr};

    // Row 4: Test Mic + VU bar
    QPushButton* m_testMicBtn{nullptr};
    HGauge*      m_vuBar{nullptr};
    QTimer*      m_vuTimer{nullptr};

    // Row 5: Mic Gain
    QSlider*     m_micGainSlider{nullptr};
    QLabel*      m_micGainLabel{nullptr};

    // Rows 6-7: Mic Gain Min / Max spinboxes (radio-mic-input Task 21).
    // Mirror Thetis udMicGainMin / udMicGainMax (setup.cs:9678-9694
    // [v2.10.3.13+501e3f51]); editing these updates the slider range live.
    QSpinBox*    m_micGainMinSpin{nullptr};
    QSpinBox*    m_micGainMaxSpin{nullptr};

    // Guard flag for both source-selector, mic-gain and radio-mic two-way sync.
    bool m_updatingFromModel{false};

    // HW family, captured at construction time for visibility logic (I.3).
    HPSDRHW m_hw{HPSDRHW::Unknown};

    // ── Radio Mic per-family group boxes (I.3) ────────────────────────────────
    QGroupBox* m_hermesGroup{nullptr};
    QGroupBox* m_orionGroup{nullptr};
    QGroupBox* m_saturnGroup{nullptr};

    // Hermes/Atlas family widgets
    QButtonGroup* m_hermesMicInputGroup{nullptr};  // Mic In / Line In
    QCheckBox*    m_hermesMicBoostChk{nullptr};
    QSlider*      m_hermesLineInGainSlider{nullptr};
    QLabel*       m_hermesLineInGainLabel{nullptr};

    // Orion-MkII family widgets
    QCheckBox* m_orionMicTipRingChk{nullptr};
    QCheckBox* m_orionMicBiasChk{nullptr};
    QCheckBox* m_orionMicPttDisabledChk{nullptr};
    QCheckBox* m_orionMicBoostChk{nullptr};

    // Saturn G2 family widgets
    QButtonGroup* m_saturnMicInputGroup{nullptr};   // 3.5 mm / XLR
    QCheckBox*    m_saturnMicPttDisabledChk{nullptr};
    QCheckBox*    m_saturnMicBiasChk{nullptr};
    QCheckBox*    m_saturnMicBoostChk{nullptr};

public:
    // Discrete buffer sizes exposed by the slider (power-of-2 steps).
    // Slider position maps to index in this list. Exposed as public so
    // tests can verify latency calculations without reimplementing the list.
    static const QVector<int> kBufferSizes;
};

} // namespace NereusSDR
