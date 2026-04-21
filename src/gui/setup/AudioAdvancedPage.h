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
// =================================================================

#include "gui/SetupPage.h"

#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace NereusSDR {

class AudioEngine;
struct DetectedCable;

// ---------------------------------------------------------------------------
// AudioAdvancedPage
//
// Sections:
//   1. DSP — sample-rate + block-size combos (persist + log deferred).
//   2. VAC Feedback Tuning — per-VAX-channel gain / slew / propRing / ffRing.
//   3. Feature Flags — SendIqToVax, TxMonitorToVax (Phase 3M deferred),
//                      MuteVaxDuringTxOnOtherSlice (active).
//   4. Detected Cables — readonly readout + Rescan button.
//   5. Reset — amber "Reset all audio to defaults" + confirm modal.
// ---------------------------------------------------------------------------
class AudioAdvancedPage : public SetupPage {
    Q_OBJECT

public:
    explicit AudioAdvancedPage(RadioModel* model, QWidget* parent = nullptr);

    // Event filter — blocks wheel events on un-focused combo boxes inside
    // the scroll area (same pattern as DeviceCard).
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    // Section builders.
    void buildDspSection();
    void buildVacFeedbackSection();
    void buildFeatureFlagsSection();
    void buildCablesSection();
    void buildResetSection();

    // Load/save helpers.
    void loadDspSettings();
    void loadVacFeedbackSettings(int channel);

    // DSP section.
    QComboBox* m_dspRateCombo    = nullptr;
    QComboBox* m_dspBlockCombo   = nullptr;

    // VAC feedback section.
    QComboBox*       m_vacTargetCombo   = nullptr;
    QDoubleSpinBox*  m_vacGainSpin      = nullptr;
    QSpinBox*        m_vacSlewSpin      = nullptr;
    QSpinBox*        m_vacPropRingSpin  = nullptr;
    QSpinBox*        m_vacFfRingSpin    = nullptr;
    int              m_currentVacChannel = 1;
    bool             m_vacLoading        = false;

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
