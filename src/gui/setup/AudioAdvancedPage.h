#pragma once

// =================================================================
// src/gui/setup/AudioAdvancedPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → Advanced page.
// No Thetis port; no attribution-registry row required.
//
// Sub-Phase 12 Task 12.4 (2026-04-20): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
//
// 2026-09-23 (R-R3-44): J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code. Usable in a remote window: the engine comes from
// RadioModel::localAudioDevices(), the VAX groups are this computer's, the
// DSP group (the Core's audio/DspRate and audio/DspBlockSize) follows the
// Core's settings availability, and Send IQ to VAX is refused there with
// a plain reason.
// =================================================================

#include "gui/SetupPage.h"

#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

namespace NereusSDR {

class AudioEngine;
struct DetectedCable;

// ---------------------------------------------------------------------------
// AudioAdvancedPage
//
// Sections:
//   1. DSP — sample-rate + block-size combos (persist + log deferred).
//   2. Feature Flags: SendIqToVax, TxMonitorToVax (Phase 3M deferred),
//                      MuteVaxDuringTxOnOtherSlice (active).
//   3. Detected Cables: readonly readout + Rescan button.
//   4. Reset: amber "Reset all audio to defaults" + confirm modal.
// ---------------------------------------------------------------------------
class AudioAdvancedPage : public SetupPage {
    Q_OBJECT

public:
    explicit AudioAdvancedPage(RadioModel* model, QWidget* parent = nullptr);

    // Event filter — blocks wheel events on un-focused combo boxes inside
    // the scroll area (same pattern as DeviceCard).
    bool eventFilter(QObject* obj, QEvent* event) override;

    // R-R3-44: the DSP group writes the Core's settings in a remote window.
    void setStationSettingsAvailable(bool available, const QString& reason) override;

    // The plain reason Send IQ to VAX gives in a remote window.
    static QString remoteSendIqReason();

private:
    // Section builders.
    void buildDspSection();
    void buildFeatureFlagsSection();
    void buildCablesSection();
    void buildResetSection();

    // Load/save helpers.
    void loadDspSettings();

    // DSP section.
    QComboBox* m_dspRateCombo    = nullptr;
    QComboBox* m_dspBlockCombo   = nullptr;

    // Feature-flag checkboxes.
    QCheckBox* m_sendIqToVaxCheck          = nullptr;
    QCheckBox* m_txMonitorToVaxCheck       = nullptr;
    QCheckBox* m_muteVaxDuringTxOtherCheck = nullptr;

    // Cables section.
    QLabel*      m_cablesLabel  = nullptr;
    QPushButton* m_rescanButton = nullptr;

    // Reset section.
    QPushButton* m_resetButton = nullptr;

    // Back-pointer (non-owning).
    AudioEngine* m_engine = nullptr;

    // Helpers.
    void updateCablesLabel(const QVector<DetectedCable>& cables);
    void onRescan();
    void onResetClicked();
    void installWheelFilter(QComboBox* combo);
};

} // namespace NereusSDR
