#pragma once

#include "gui/SetupPage.h"

class QSlider;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;

namespace NereusSDR {

class PanadapterModel;
class ColorSwatchButton;

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
    void loadFromRenderer();
    void pushFps(int fps);

    // Section: FFT
    QComboBox* m_fftSizeCombo{nullptr};      // 1024/2048/4096/8192/16384
    QComboBox* m_windowCombo{nullptr};       // Blackman-Harris/Hann/Hamming/Flat-Top

    // Section: Rendering
    QSlider*   m_fpSlider{nullptr};          // 10–60 fps
    QComboBox* m_averagingCombo{nullptr};    // None/Weighted/Logarithmic/TimeWindow
    QSpinBox*  m_averagingTimeSpin{nullptr}; // S15 avg time (ms)
    QSpinBox*  m_decimationSpin{nullptr};    // S16 decimation
    QCheckBox* m_fillToggle{nullptr};        // Fill under spectrum trace
    QSlider*   m_fillAlphaSlider{nullptr};   // 0–100
    QSlider*   m_lineWidthSlider{nullptr};   // 1–3
    QCheckBox* m_gradientToggle{nullptr};    // S14 gradient enabled

    // Section: Colors (S11/S13)
    ColorSwatchButton* m_dataLineColorBtn{nullptr};
    QSlider*           m_dataLineAlphaSlider{nullptr}; // S12
    ColorSwatchButton* m_dataFillColorBtn{nullptr};

    // Section: Calibration
    QDoubleSpinBox* m_calOffsetSpin{nullptr}; // Display calibration offset (dBm)
    QCheckBox*      m_peakHoldToggle{nullptr};
    QSpinBox*       m_peakHoldDelaySpin{nullptr}; // ms

    // Section: Thread (S17)
    QComboBox*      m_threadPriorityCombo{nullptr};
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
    void loadFromRenderer();

    // Section: Levels
    QSlider*           m_highThresholdSlider{nullptr};
    QSlider*           m_lowThresholdSlider{nullptr};
    QLabel*            m_agcDeprecationLabel{nullptr};
    ColorSwatchButton* m_lowColorBtn{nullptr};                // W10
    QCheckBox*         m_useSpectrumMinMaxToggle{nullptr};    // W15

    // Section: Display
    QSlider*   m_updatePeriodSlider{nullptr};
    QCheckBox* m_reverseToggle{nullptr};
    QSlider*   m_opacitySlider{nullptr};
    QComboBox* m_colorSchemeCombo{nullptr};  // 7 schemes
    QComboBox* m_wfAveragingCombo{nullptr};  // W16

    // Section: Overlays
    QCheckBox* m_showRxFilterToggle{nullptr};
    QCheckBox* m_showTxFilterToggle{nullptr};
    QCheckBox* m_showRxZeroLineToggle{nullptr};
    QCheckBox* m_showTxZeroLineToggle{nullptr};

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
    void loadFromRenderer();
    void applyBandSlot(PanadapterModel* pan);

    // Section: Grid
    QCheckBox*      m_gridToggle{nullptr};
    QLabel*         m_editingBandLabel{nullptr};
    QSpinBox*       m_dbMaxSpin{nullptr};
    QSpinBox*       m_dbMinSpin{nullptr};
    QSpinBox*       m_dbStepSpin{nullptr};

    // Section: Labels & Colors
    QComboBox*      m_freqLabelAlignCombo{nullptr}; // Left/Center/Right/Auto/Off
    QCheckBox*      m_zeroLineToggle{nullptr};
    QCheckBox*      m_showFpsToggle{nullptr};

    ColorSwatchButton* m_gridColorBtn{nullptr};       // G9
    ColorSwatchButton* m_gridFineColorBtn{nullptr};   // G10
    ColorSwatchButton* m_hGridColorBtn{nullptr};      // G11
    ColorSwatchButton* m_gridTextColorBtn{nullptr};   // G12
    ColorSwatchButton* m_zeroLineColorBtn{nullptr};   // G13
    ColorSwatchButton* m_bandEdgeColorBtn{nullptr};   // G6
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
