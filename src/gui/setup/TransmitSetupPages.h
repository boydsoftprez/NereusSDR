#pragma once

#include "gui/SetupPage.h"

class QSlider;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Transmit > Power & PA
// Corresponds to Thetis setup.cs PA / Power tab.
// All controls NYI (disabled).
// ---------------------------------------------------------------------------
class PowerPaPage : public SetupPage {
    Q_OBJECT
public:
    explicit PowerPaPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Power
    QSlider* m_maxPowerSlider{nullptr};        // 0–100 W
    QSlider* m_swrProtectionSlider{nullptr};   // SWR threshold

    // Section: PA
    QLabel*    m_perBandGainLabel{nullptr};    // placeholder: future table
    QComboBox* m_fanControlCombo{nullptr};     // Off/Low/Med/High/Auto
};

// ---------------------------------------------------------------------------
// Transmit > TX Profiles
// ---------------------------------------------------------------------------
class TxProfilesPage : public SetupPage {
    Q_OBJECT
public:
    explicit TxProfilesPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Profile
    QLabel*      m_profileListLabel{nullptr};  // placeholder for future list
    QLineEdit*   m_nameEdit{nullptr};
    QPushButton* m_newBtn{nullptr};
    QPushButton* m_deleteBtn{nullptr};
    QPushButton* m_copyBtn{nullptr};

    // Section: Compression
    QCheckBox* m_compressorToggle{nullptr};
    QSlider*   m_gainSlider{nullptr};
    QCheckBox* m_cessbToggle{nullptr};
};

// ---------------------------------------------------------------------------
// Transmit > Speech Processor
// ---------------------------------------------------------------------------
class SpeechProcessorPage : public SetupPage {
    Q_OBJECT
public:
    explicit SpeechProcessorPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Compressor
    QSlider*   m_gainSlider{nullptr};
    QCheckBox* m_cessbToggle{nullptr};

    // Section: Phase Rotator
    QSpinBox* m_stagesSpin{nullptr};
    QSlider*  m_cornerFreqSlider{nullptr};

    // Section: CFC
    QCheckBox* m_cfcToggle{nullptr};
    QLabel*    m_cfcProfileLabel{nullptr};  // placeholder
};

// ---------------------------------------------------------------------------
// Transmit > PureSignal
// ---------------------------------------------------------------------------
class PureSignalPage : public SetupPage {
    Q_OBJECT
public:
    explicit PureSignalPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: PureSignal
    QCheckBox* m_enableToggle{nullptr};
    QCheckBox* m_autoCalToggle{nullptr};
    QComboBox* m_feedbackDdcCombo{nullptr};  // DDC selection
    QSlider*   m_attentionSlider{nullptr};
    QLabel*    m_infoLabel{nullptr};         // status/info placeholder
};

} // namespace NereusSDR
