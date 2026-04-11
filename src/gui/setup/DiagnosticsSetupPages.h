#pragma once

#include "gui/SetupPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Diagnostics > Signal Generator
// Tone section: frequency spinner, amplitude slider, enable toggle.
// Noise section: enable toggle, level slider.
// Sweep section: enable toggle, range label placeholder.
// All controls are NYI / disabled.
// ---------------------------------------------------------------------------
class DiagSignalGeneratorPage : public SetupPage {
    Q_OBJECT

public:
    explicit DiagSignalGeneratorPage(QWidget* parent = nullptr);

private:
    // Tone
    QSpinBox*  m_toneFreqSpin{nullptr};
    QSlider*   m_toneAmpSlider{nullptr};
    QCheckBox* m_toneEnableCheck{nullptr};

    // Noise
    QCheckBox* m_noiseEnableCheck{nullptr};
    QSlider*   m_noiseLevelSlider{nullptr};

    // Sweep
    QCheckBox* m_sweepEnableCheck{nullptr};
    QLabel*    m_sweepRangeLabel{nullptr};

    void buildUI();
};

// ---------------------------------------------------------------------------
// Diagnostics > Hardware Tests
// ADC test button, result label, DDC test button, loopback test button.
// All controls are NYI / disabled.
// ---------------------------------------------------------------------------
class DiagHardwareTestsPage : public SetupPage {
    Q_OBJECT

public:
    explicit DiagHardwareTestsPage(QWidget* parent = nullptr);

private:
    QPushButton* m_adcTestButton{nullptr};
    QLabel*      m_resultLabel{nullptr};
    QPushButton* m_ddcTestButton{nullptr};
    QPushButton* m_loopbackButton{nullptr};

    void buildUI();
};

// ---------------------------------------------------------------------------
// Diagnostics > Logging
// Log section: level combo, file path label, open log button, clear log button.
// Categories section: filter placeholder label.
// All controls are NYI / disabled.
// ---------------------------------------------------------------------------
class DiagLoggingPage : public SetupPage {
    Q_OBJECT

public:
    explicit DiagLoggingPage(QWidget* parent = nullptr);

private:
    QComboBox*   m_levelCombo{nullptr};
    QLabel*      m_filePathLabel{nullptr};
    QPushButton* m_openLogButton{nullptr};
    QPushButton* m_clearLogButton{nullptr};
    QLabel*      m_filterLabel{nullptr};

    void buildUI();
};

} // namespace NereusSDR
