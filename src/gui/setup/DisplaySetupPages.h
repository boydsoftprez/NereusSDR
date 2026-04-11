#pragma once

#include "gui/SetupPage.h"

class QSlider;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Display > Spectrum Defaults
// Corresponds to Thetis setup.cs Display tab, Spectrum section.
// All controls are NYI (disabled). Wired to persistence in a future phase.
// ---------------------------------------------------------------------------
class SpectrumDefaultsPage : public SetupPage {
    Q_OBJECT
public:
    explicit SpectrumDefaultsPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: FFT
    QComboBox* m_fftSizeCombo{nullptr};      // 1024/2048/4096/8192/16384
    QComboBox* m_windowCombo{nullptr};       // Blackman-Harris/Hann/Hamming/Flat-Top

    // Section: Rendering
    QSlider*   m_fpSlider{nullptr};          // 10–60 fps
    QComboBox* m_averagingCombo{nullptr};    // None/Weighted/Logarithmic
    QCheckBox* m_fillToggle{nullptr};        // Fill under spectrum trace
    QSlider*   m_fillAlphaSlider{nullptr};   // 0–100
    QSlider*   m_lineWidthSlider{nullptr};   // 1–3

    // Section: Calibration
    QDoubleSpinBox* m_calOffsetSpin{nullptr}; // Display calibration offset (dBm)
    QCheckBox*      m_peakHoldToggle{nullptr};
    QSpinBox*       m_peakHoldDelaySpin{nullptr}; // ms
};

// ---------------------------------------------------------------------------
// Display > Waterfall Defaults
// Corresponds to Thetis setup.cs Display tab, Waterfall section.
// ---------------------------------------------------------------------------
class WaterfallDefaultsPage : public SetupPage {
    Q_OBJECT
public:
    explicit WaterfallDefaultsPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Levels
    QSlider*   m_highThresholdSlider{nullptr};
    QSlider*   m_lowThresholdSlider{nullptr};
    QCheckBox* m_agcToggle{nullptr};

    // Section: Display
    QSlider*   m_updatePeriodSlider{nullptr};
    QCheckBox* m_reverseToggle{nullptr};
    QSlider*   m_opacitySlider{nullptr};
    QComboBox* m_colorSchemeCombo{nullptr};  // Enhanced/Grayscale/Spectrum

    // Section: Time
    QComboBox* m_timestampPosCombo{nullptr};  // None/Left/Right
    QComboBox* m_timestampModeCombo{nullptr}; // UTC/Local
};

// ---------------------------------------------------------------------------
// Display > Grid & Scales
// ---------------------------------------------------------------------------
class GridScalesPage : public SetupPage {
    Q_OBJECT
public:
    explicit GridScalesPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Grid
    QCheckBox*      m_gridToggle{nullptr};
    QSpinBox*       m_dbMaxSpin{nullptr};
    QSpinBox*       m_dbMinSpin{nullptr};
    QSpinBox*       m_dbStepSpin{nullptr};

    // Section: Labels
    QComboBox*      m_freqLabelAlignCombo{nullptr}; // Left/Center
    QLabel*         m_bandEdgeColorLabel{nullptr};  // placeholder color swatch
    QCheckBox*      m_zeroLineToggle{nullptr};
    QCheckBox*      m_showFpsToggle{nullptr};
};

// ---------------------------------------------------------------------------
// Display > RX2 Display
// ---------------------------------------------------------------------------
class Rx2DisplayPage : public SetupPage {
    Q_OBJECT
public:
    explicit Rx2DisplayPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: RX2 Spectrum
    QSpinBox*  m_dbMaxSpin{nullptr};
    QSpinBox*  m_dbMinSpin{nullptr};
    QComboBox* m_colorSchemeCombo{nullptr}; // Enhanced/Grayscale/Spectrum

    // Section: RX2 Waterfall
    QSlider*   m_highThresholdSlider{nullptr};
    QSlider*   m_lowThresholdSlider{nullptr};
};

// ---------------------------------------------------------------------------
// Display > TX Display
// ---------------------------------------------------------------------------
class TxDisplayPage : public SetupPage {
    Q_OBJECT
public:
    explicit TxDisplayPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: TX Spectrum
    QLabel*         m_bgColorLabel{nullptr};    // placeholder color swatch
    QLabel*         m_gridColorLabel{nullptr};  // placeholder color swatch
    QSlider*        m_lineWidthSlider{nullptr}; // 1–3
    QDoubleSpinBox* m_calOffsetSpin{nullptr};   // dBm offset
};

} // namespace NereusSDR
