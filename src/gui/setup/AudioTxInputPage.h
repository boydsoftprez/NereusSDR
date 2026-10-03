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
// R-R3-36 (2026-09-22): Backend / Device / Buffer read
// AudioEngine::txInputConfig() and write through setTxInputConfig(),
// persisted under audio/TxInput exactly as the Devices page TX Input card
// does, so both pages edit one selection. TransmitModel's pcMic* fields
// project that config (RadioModel wiring).
//
// Phase 3M-1b Task I.3 (2026-04-28): Radio Mic settings group with
// per-family layout, capability-gated. Visible only when
// MicSource::Radio AND caps.hasMicJack == true. Three sub-layouts:
//   Hermes/Atlas/HermesII/Angelia family: QGroupBox "Radio Mic (Hermes / Atlas)":
//     Row 1: Mic In / Line In radio buttons (TransmitModel::lineIn)
//     Row 2: +20 dB Mic Boost checkbox (TransmitModel::micBoost)
//     Row 3: Line In Gain slider -34..+12 dB (TransmitModel::lineInBoost)
//   Orion-MkII family (Orion/OrionMKII): QGroupBox "Radio Mic (Orion-MkII)":
//     4 checkboxes: Mic Tip-Ring, Mic Bias, Mic PTT Disabled, +20 dB Mic Boost
//   Saturn G2: QGroupBox "Radio Mic (Saturn G2)":
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
//   2026-09-22 : R-R3-36 Task 6 by J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code. PC Mic controls
//                edit the shared audio/TxInput config; Test Mic holds a
//                real capture demand; microphone status and Retry beside
//                Test Mic.
//   2026-09-23 : R-R3-36 by J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code. The PC Mic
//                controls reach this computer's engine through
//                RadioModel::localAudioDevices() and work in a remote
//                window; the mic source, Mic Gain and radio mic groups
//                follow the transmit permission (setTransmitPermitted).
//   2026-09-23 : R-R3-21 / R-R3-10 by J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code. The same held
//                controls are also disabled, with "Connect to the Core to
//                change these.", while a remote window does not have the
//                Core's settings (setStationSettingsAvailable).
//   2026-09-25 : R-R3-49 (parity Task 3) by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code. Mic
//                Gain and the radio microphone groups follow the transmit
//                settings gate (setTransmitSettingsPermittedAt, version 3)
//                and change the Core's values off the air; the mic source
//                keeps the transmit permission.
//   2026-09-30 : Radio codec lane by J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code. Radio Mic opens
//                on the Hermes Lite 2 with a note that it needs the audio
//                add-on board, and the Hermes group shows there; the Saturn
//                G2 group gains Mic Tip-Ring; the Orion group is disabled
//                on the Red Pitaya with its reason; Line In Gain moves in
//                1.5 dB steps (Setup description version 24).
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#include "gui/SetupPage.h"
#include "core/AudioDeviceConfig.h"
#include "core/audio/CaptureSupervisor.h"
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
#include <QTimer>

class QHideEvent;

namespace NereusSDR {

class AudioEngine;
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
// Test Mic implementation (R-R3-36):
//   While Test Mic is checked the page holds a CaptureSupervisor TestMic
//   lease from AudioEngine::acquireCaptureDemand(), so the selected input
//   really opens, with or without a connected radio. The lease is released
//   on Stop Test, when the page is hidden and when it is destroyed. A
//   10 ms QTimer polls AudioEngine::pcMicInputLevel(), which reads the
//   capture reader's level and is 0.0f until capture is Ready.
//
// Microphone status (R-R3-36): a label (objectName "captureStatus") shows
//   captureStatusText(AudioEngine::captureStatus()) and a "Retry microphone"
//   button (objectName "retryCapture"), enabled only in Failed, calls
//   AudioEngine::retryCapture(). Both refresh on captureStatusChanged.
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
    QLabel*      captureStatusLabel() const { return m_captureStatusLabel; }
    QPushButton* retryCaptureButton() const { return m_retryCaptureBtn; }

    // True while Test Mic holds its capture demand.
    bool hasTestMicDemand() const { return m_testMicLease.isActive(); }

    // Expose Radio Mic per-family group boxes for test introspection (I.3).
    QGroupBox* hermesRadioMicGroup() const { return m_hermesGroup; }
    QGroupBox* orionRadioMicGroup()  const { return m_orionGroup; }
    QGroupBox* saturnRadioMicGroup() const { return m_saturnGroup; }
    QGroupBox* micSourceGroup()      const { return m_micSourceGroup; }
    QRadioButton* radioMicButton()   const { return m_radioMicBtn; }
    QLabel*    radioMicNoteLabel()   const { return m_radioMicNoteLabel; }
    QSlider*   hermesLineInGainSlider() const { return m_hermesLineInGainSlider; }
    QLabel*    hermesLineInGainLabel()  const { return m_hermesLineInGainLabel; }
    QCheckBox* saturnMicTipRingCheck()  const { return m_saturnMicTipRingChk; }

    // The Line In Gain slider counts half decibels, so it moves in the
    // 1.5 dB steps of Thetis's udLineInBoost (TransmitModel::kLineInBoostStep).
    static constexpr int kLineInGainSliderScale = 2;

    // R-R3-36: the page is Mixed. The PC microphone device, backend,
    // buffer, Test Mic and Retry are this computer's and stay usable in a
    // remote window. The mic source selector, Mic Gain and the radio's own
    // microphone hardware groups are transmit settings held for the radio,
    // so they follow the negotiated transmit permission with its reason.
    // SetupDialog pushes the permission to every realized page; locally it
    // is always granted, so nothing changes there.
    void setTransmitPermitted(bool permitted, const QString& reason) override;

    // R-R3-49 (parity Task 3): Mic Gain and the radio microphone groups
    // follow the transmit settings gate for transmitSettingsVersion 3
    // instead: the Core takes them while its radio is off the air. The mic
    // source keeps setTransmitPermitted (the PC mic and VAX wait for remote
    // transmit's microphone audio).
    void setTransmitSettingsPermittedAt(int version, bool permitted,
                                        const QString& reason) override;

    // R-R3-21 / R-R3-10: the held controls are the Core's settings too, so
    // they are also disabled while those are unavailable. That reason wins
    // while it applies: connecting comes before any transmit permission.
    void setStationSettingsAvailable(bool available, const QString& reason) override;

protected:
    void hideEvent(QHideEvent* event) override;

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
    void onSaturnMicTipRingToggled(bool on);

    // Model → UI: radio mic flag changes (all families, I.3)
    void onModelLineInChanged(bool on);
    void onModelMicBoostChanged(bool on);
    void onModelLineInBoostChanged(double dB);
    void onModelMicTipRingChanged(bool on);
    void onModelMicBiasChanged(bool on);
    void onModelMicPttDisabledChanged(bool on);
    void onModelMicXlrChanged(bool on);

private:
    void buildPage(bool radioMicSelectable, HPSDRHW hw);
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
    static QString lineInBoostLabel(double dB);
    void showLineInBoost(double dB);

    // R-R3-36: shared audio/TxInput config.
    AudioEngine* engine();
    void applyTxInputConfigToControls(const AudioDeviceConfig& cfg);
    void commitTxInputConfig(const AudioDeviceConfig& cfg);
    void refreshCaptureStatus();

    // Returns the latency string for `samples` samples at 48 kHz reference.
    static QString latencyString(int samples);

    // Returns the OS-default PortAudio host API index.
    static int defaultHostApiIndex();

    // ── Source selector (I.1) ─────────────────────────────────────────────
    QGroupBox*     m_micSourceGroup{nullptr};
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
    CaptureSupervisor::Lease m_testMicLease;

    // Microphone status + Retry (R-R3-36)
    QLabel*      m_captureStatusLabel{nullptr};
    QPushButton* m_retryCaptureBtn{nullptr};

    // True while applyTxInputConfigToControls() moves the PC Mic controls,
    // so their change slots do not write the config back.
    bool m_applyingTxInputConfig{false};

    // Row 5: Mic Gain
    QSlider*     m_micGainSlider{nullptr};
    // R-R3-21: the two conditions the held controls follow (see
    // applyHeldControlGate()).
    bool    m_heldTransmitPermitted = true;
    QString m_heldTransmitReason;
    bool    m_heldStationAvailable = true;
    QString m_heldStationReason;
    // R-R3-49 (parity Task 3): the transmit settings gate (version 3).
    bool    m_heldSettingsPermitted = true;
    QString m_heldSettingsReason;
    void applyHeldControlGate();
    QLabel*      m_micGainLabel{nullptr};

    // Guard flag for both source-selector, mic-gain and radio-mic two-way sync.
    bool m_updatingFromModel{false};

    // HW family, captured at construction time for visibility logic (I.3).
    HPSDRHW m_hw{HPSDRHW::Unknown};
    // Radio codec lane: the HL2 takes the radio mic only with its audio
    // add-on board (BoardCapabilities::radioMicNeedsAddOn), and Thetis
    // greys out the Orion mic panel on the Red Pitaya
    // (RadioModel::orionMicPanelAvailable). Captured at construction.
    bool m_radioMicNeedsAddOn{false};
    bool m_orionMicPanelAvailable{true};
    QLabel* m_radioMicNoteLabel{nullptr};

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
    QCheckBox*    m_saturnMicTipRingChk{nullptr};

public:
    // Discrete buffer sizes exposed by the slider (power-of-2 steps).
    // Slider position maps to index in this list. Exposed as public so
    // tests can verify latency calculations without reimplementing the list.
    static const QVector<int> kBufferSizes;
};

} // namespace NereusSDR
