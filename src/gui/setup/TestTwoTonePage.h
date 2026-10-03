#pragma once

// =================================================================
// src/gui/setup/TestTwoTonePage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Test → Two-Tone IMD page.
// No Thetis port, no attribution headers required (per memory:
// feedback_source_first_ui_vs_dsp — Qt widgets in Setup pages are
// NereusSDR-native).
//
// Phase 3M-1c Task H (2026-04-29): Setup → Test category + Two-Tone
// page exposing the 8 TransmitModel two-tone properties (B.2 + B.3):
//
//   Group 1: Tone Frequencies
//     • udTestIMDFreq1 (QSpinBox, -20000..20000 Hz)
//     • udTestIMDFreq2 (QSpinBox, -20000..20000 Hz)
//     • Defaults preset button (sets Freq1=700 / Freq2=1900)
//     • Stealth  preset button (sets Freq1=70  / Freq2=190)
//
//   Group 2: Output Level
//     • udTwoToneLevel  (QDoubleSpinBox, -96.000..0.000 dB)
//     • udTestIMDPower  (QSpinBox, 0..100 %)
//
//   Group 3: Mode
//     • chkPulsed_TwoTone (QCheckBox)
//     • chkInvertTones    (QCheckBox)
//     • udFreq2Delay      (QSpinBox, 0..1000 ms)
//
//   Group 4: Drive Power Source (QButtonGroup with 3 radios)
//     • Drive Slider  (DrivePowerSource::DriveSlider, default)
//     • Tune Slider   (DrivePowerSource::TuneSlider)
//     • Fixed         (DrivePowerSource::Fixed)
//
// Bidirectional model↔UI sync:
//   • Each control's signal calls the corresponding TransmitModel setter.
//   • Each TransmitModel *Changed signal updates the control via QSignalBlocker
//     to prevent feedback loops.
//
// Layout matches Thetis grpTestTXIMD per setup.cs:11019..11200 [v2.10.3.13].
// Verbatim Thetis tooltip strings (preserved per project source-first protocol):
//   • chkInvertTones tooltip: "Swap F1 and F2 for lower side band modes"
//     (setup.Designer.cs:61971 [v2.10.3.13])
//   • udFreq2Delay tooltip: "Applies a wait delay before Freq#2 is enabled"
//     (setup.Designer.cs:61942 [v2.10.3.13])
//
// Run-time behavior (chkTestIMD activate handler that engages MOX, applies
// TXPostGen* setters, restores PWR slider on stop) is Phase I; it does NOT
// live on this page.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-29 — H.1/H.2/H.3 written by J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 5): every control is a setting on
//                the Core's `transmit` and follows the transmit settings
//                gate at version 5 in a remote window. The two-tone start
//                is the TX applet's 2-Tone button, not on this page.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#include "gui/SetupPage.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>

namespace NereusSDR {

class RadioModel;
enum class DrivePowerSource : int;

// ---------------------------------------------------------------------------
// TestTwoTonePage — Setup → Test → Two-Tone IMD
//
// Controls bound to TransmitModel two-tone properties.  Construction wires
// every UI signal to the corresponding model setter and every model
// *Changed signal back to the UI control (with QSignalBlocker to break
// the feedback loop).
// ---------------------------------------------------------------------------
class TestTwoTonePage : public SetupPage {
    Q_OBJECT
public:
    explicit TestTwoTonePage(RadioModel* model, QWidget* parent = nullptr);
    ~TestTwoTonePage() override = default;

    // R-R3-49 (parity Task 5): see the header's history.
    void setTransmitSettingsPermittedAt(int version, bool permitted,
                                        const QString& reason) override;

    // Test-introspection accessors (always exposed; pages pattern matches
    // AudioTxInputPage).  No NEREUS_BUILD_TESTS guard needed.
    QSpinBox*       freq1Spin()       const { return m_freq1Spin; }
    QSpinBox*       freq2Spin()       const { return m_freq2Spin; }
    QDoubleSpinBox* levelSpin()       const { return m_levelSpin; }
    QSpinBox*       powerSpin()       const { return m_powerSpin; }
    QSpinBox*       freq2DelaySpin()  const { return m_freq2DelaySpin; }
    QCheckBox*      invertCheck()     const { return m_invertCheck; }
    QCheckBox*      pulsedCheck()     const { return m_pulsedCheck; }
    QPushButton*    defaultsButton()  const { return m_defaultsBtn; }
    QPushButton*    stealthButton()   const { return m_stealthBtn; }
    QRadioButton*   driveSliderRadio() const { return m_driveSliderRadio; }
    QRadioButton*   tuneSliderRadio()  const { return m_tuneSliderRadio; }
    QRadioButton*   fixedDriveRadio()  const { return m_fixedDriveRadio; }

private slots:
    // UI → Model.
    void onFreq1Changed(int hz);
    void onFreq2Changed(int hz);
    void onLevelChanged(double db);
    void onPowerChanged(int pct);
    void onFreq2DelayChanged(int ms);
    void onInvertToggled(bool on);
    void onPulsedToggled(bool on);
    void onDrivePowerRadioToggled(int id, bool checked);

    // Preset buttons (touch only Freq1/Freq2; matches Thetis
    // btnTwoToneF_defaults_Click / btnTwoToneF_stealth_Click).
    void onDefaultsClicked();
    void onStealthClicked();

    // Model → UI.
    void onModelFreq1Changed(int hz);
    void onModelFreq2Changed(int hz);
    void onModelLevelChanged(double db);
    void onModelPowerChanged(int pct);
    void onModelFreq2DelayChanged(int ms);
    void onModelInvertChanged(bool on);
    void onModelPulsedChanged(bool on);
    void onModelDrivePowerSourceChanged(DrivePowerSource source);

private:
    void buildUi();
    void seedFromModel();
    void wireModelSignals();
    void selectDriveRadioFor(DrivePowerSource source);

    // Tone frequencies group widgets.
    QSpinBox*    m_freq1Spin{nullptr};
    QSpinBox*    m_freq2Spin{nullptr};
    QPushButton* m_defaultsBtn{nullptr};
    QPushButton* m_stealthBtn{nullptr};

    // Output level group widgets.
    QDoubleSpinBox* m_levelSpin{nullptr};
    QSpinBox*       m_powerSpin{nullptr};

    // Mode group widgets.
    QCheckBox* m_pulsedCheck{nullptr};
    QCheckBox* m_invertCheck{nullptr};
    QSpinBox*  m_freq2DelaySpin{nullptr};

    // Drive power source radios.
    QButtonGroup* m_driveButtonGroup{nullptr};
    QRadioButton* m_driveSliderRadio{nullptr};
    QRadioButton* m_tuneSliderRadio{nullptr};
    QRadioButton* m_fixedDriveRadio{nullptr};
};

} // namespace NereusSDR
