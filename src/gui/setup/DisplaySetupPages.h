#pragma once

// =================================================================
// src/gui/setup/DisplaySetupPages.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "gui/SetupPage.h"

class QSlider;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;

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

signals:
    /// Emitted when the user clicks "Configure peaks →".
    /// SetupDialog connects to selectPage("Spectrum Peaks").
    void navigateToSpectrumPeaksRequested();

    /// Emitted when the user clicks "Configure multimeter →".
    /// SetupDialog will connect to selectPage("Multimeter") in Task 3.1.
    void navigateToMultimeterRequested();

private:
    void buildUI();
    void loadFromRenderer();
    void pushFps(int fps);

    // Section: FFT
    // Phase 2: FFT size is a 0..6 slider (Thetis tbDisplayFFTSize per
    // setup.designer.cs:35043 [v2.10.3.13]).  Slider value maps to
    // 4096 << v -> {4096, 8192, 16384, 32768, 65536, 131072, 262144}.
    // m_fftSizeReadout shows the current size as text alongside the slider.
    QSlider*   m_fftSizeSlider{nullptr};
    QLabel*    m_fftSizeReadout{nullptr};
    // Always-visible bin-width readout in the FFT group, paired with the
    // "Bin Width (Hz)" prefix label.  Mirrors Thetis lblDisplayBinWidth at
    // setup.designer.cs:35010-35021 [v2.10.3.13].  Distinct from
    // m_binWidthReadout (Overlays group, gates the on-spectrum corner
    // overlay -- a NereusSDR-original control).
    QLabel*    m_binWidthLabel{nullptr};
    // Window combo: 7 Thetis-faithful items (Rectangular / Blackman-Harris
    // 4T / Hann / Flat-Top / Hamming / Kaiser / Blackman-Harris 7T) per
    // setup.designer.cs:34966-34973 [v2.10.3.13].  Combo index maps 1:1
    // to WindowFunction enum integer value (both follow WDSP analyzer.c
    // case codes).
    QComboBox* m_windowCombo{nullptr};

    // Hz/bin target spinbox (Option 3 — 2026-05-08 design).  0 = off
    // (use bins-in-window default); > 0 = override the auto-zoom replan
    // formula to deliver constant Hz/bin regardless of zoom level.
    QDoubleSpinBox* m_hzPerBinTargetSpin{nullptr};

    // Section: Rendering
    QSlider*   m_fpSlider{nullptr};          // 10–60 fps
    // Defensive secondary reference to the FPS spin readout cell.  The
    // bidirectional slider-spin sync wired in makeSliderRow should keep
    // them in lockstep, but pushFps explicitly sets m_fpSpin->setValue()
    // as a belt-and-braces measure -- JJ reported the spin readout
    // sticking at 30 even when slider was at 60, suggesting the
    // makeSliderRow connect doesn't fire reliably under some conditions.
    QSpinBox*  m_fpSpin{nullptr};
    // Legacy "Averaging" combo removed in v0.3.0; setAverageMode() no longer
    // drives the renderer. m_spectrumAveragingCombo + m_spectrumAvgTimeSpin
    // (Thetis-faithful split) are the canonical controls.
    // Task 2.1: Detector + Averaging split (handwave fix from 3G-8).
    // From Thetis comboDispPanDetector/comboDispPanAveraging [v2.10.3.13].
    QComboBox* m_spectrumDetectorCombo{nullptr};  // Peak/Rosenfell/Average/Sample/RMS
    QComboBox* m_spectrumAveragingCombo{nullptr}; // None/Recursive/Time Window/Log Recursive
    QSpinBox*  m_averagingTimeSpin{nullptr}; // S15 avg time (ms)
    QSpinBox*  m_decimationSpin{nullptr};    // S16 decimation
    QCheckBox* m_fillToggle{nullptr};        // Fill under spectrum trace
    QSlider*   m_fillAlphaSlider{nullptr};   // 0–100
    QSlider*   m_lineWidthSlider{nullptr};   // 1–3
    QCheckBox* m_gradientToggle{nullptr};    // S14 gradient enabled

    // Section: Colors moved to Setup → Appearance → Colors & Theme (S11/S12/S13).

    // Section: Calibration
    QDoubleSpinBox* m_calOffsetSpin{nullptr}; // Display calibration offset (dBm)
    QCheckBox*      m_peakHoldToggle{nullptr};
    QSpinBox*       m_peakHoldDelaySpin{nullptr}; // ms

    // Section: Thread (S17)
    QComboBox*      m_threadPriorityCombo{nullptr};

    // Section: Cross-links (Task 2.4)
    QPushButton*    m_configurePeaksBtn{nullptr};
    QPushButton*    m_configureMultimeterBtn{nullptr};

    // Section: Spectrum Overlays (Task 2.3)
    // From Thetis setup.designer.cs:33043 [v2.10.3.13] chkShowMHzOnCursor
    QCheckBox* m_showMHzOnCursorToggle{nullptr};
    // From Thetis setup.cs:7061 [v2.10.3.13] lblDisplayBinWidth
    QCheckBox* m_showBinWidthToggle{nullptr};
    QLabel*    m_binWidthReadout{nullptr};
    // From Thetis display.cs:2304 [v2.10.3.13] m_bShowNoiseFloorDBM
    QCheckBox* m_showNoiseFloorToggle{nullptr};
    QComboBox* m_noiseFloorPositionCombo{nullptr};
    // NF render parameters — From Thetis display.cs:2310-2337 + 5763 [v2.10.3.13].
    QDoubleSpinBox*    m_nfShiftSpin{nullptr};        // [-12, +12] dB
    QDoubleSpinBox*    m_nfLineWidthSpin{nullptr};    // [1.0, 5.0]
    ColorSwatchButton* m_nfLineColorBtn{nullptr};
    ColorSwatchButton* m_nfTextColorBtn{nullptr};
    ColorSwatchButton* m_nfFastColorBtn{nullptr};
    // From Thetis specHPSDR.cs:325 [v2.10.3.13] NormOneHzPan
    QCheckBox* m_dispNormalizeToggle{nullptr};
    // From Thetis console.cs:20073 [v2.10.3.13] PeakTextDelay
    QCheckBox* m_showPeakValueOverlayToggle{nullptr};
    QComboBox* m_peakValuePositionCombo{nullptr};
    QSpinBox*  m_peakTextDelaySpin{nullptr};
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
    void updateEffectiveDepthLabel();
    void updateDelayLabel();          // Task 2.8: live "Delay: NN.N s" readout

    // Section: Levels
    QSlider*           m_highThresholdSlider{nullptr};
    QSlider*           m_lowThresholdSlider{nullptr};
    QCheckBox*         m_agcToggle{nullptr};
    // W10 Low Color moved to Setup → Appearance → Colors & Theme.
    QCheckBox*         m_useSpectrumMinMaxToggle{nullptr};    // W15

    // Section: NF-AGC (Task 2.8)
    QCheckBox* m_wfNfAgcEnable{nullptr};   // WaterfallNFAGCEnabled
    QSpinBox*  m_wfAgcOffsetDb{nullptr};   // WaterfallAGCOffsetDb (-60..+60)

    // Section: Display
    QSlider*   m_updatePeriodSlider{nullptr};
    QLabel*    m_delayLabel{nullptr};      // Task 2.8: live "Delay: NN.N s" readout
    QCheckBox* m_wfStopOnTx{nullptr};      // Task 2.8: WaterfallStopOnTx
    QSlider*   m_opacitySlider{nullptr};
    QComboBox* m_colorSchemeCombo{nullptr};  // 7 schemes
    // W16 legacy combo removed in v0.3.0; the Thetis-faithful split combos
    // m_waterfallDetectorCombo + m_waterfallAveragingCombo (below) replace it.
    // Task 2.1: Detector + Averaging split for waterfall.
    // From Thetis comboDispWFDetector/comboDispWFAveraging [v2.10.3.13].
    QComboBox* m_waterfallDetectorCombo{nullptr};  // Peak/Rosenfell/Average/Sample
    QComboBox* m_waterfallAveragingCombo{nullptr}; // None/Recursive/Time Window/Log Recursive
    // Waterfall averaging time constant (independent from spectrum).
    // From Thetis udDisplayAVTimeWF [v2.10.3.13] (setup.designer.cs:2086).
    QSpinBox*  m_waterfallAvgTimeSpin{nullptr};

    // Task 2.8: Copy spectrum min/max → waterfall thresholds button
    QPushButton* m_copySpecMinMaxBtn{nullptr};

    // Section: Overlays
    QCheckBox* m_showRxFilterToggle{nullptr};
    QCheckBox* m_showTxFilterToggle{nullptr};
    QCheckBox* m_showRxZeroLineToggle{nullptr};
    QCheckBox* m_showTxZeroLineToggle{nullptr};

    // Section: Time
    QComboBox* m_timestampPosCombo{nullptr};  // None/Left/Right
    QComboBox* m_timestampModeCombo{nullptr}; // UTC/Local

    // Section: Rewind history (Sub-epic E task 11)
    QComboBox* m_historyDepthCombo{nullptr};
    QLabel*    m_effectiveDepthLabel{nullptr};
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
    QCheckBox*      m_dbmScaleVisibleToggle{nullptr};
    QLabel*         m_editingBandLabel{nullptr};
    // Per-band spinbox row labels — hold a reference so they update with the
    // active band name when the user tunes across a band boundary.
    QLabel*         m_dbMaxRowLabel{nullptr};
    QLabel*         m_dbMinRowLabel{nullptr};
    QSpinBox*       m_dbMaxSpin{nullptr};
    QSpinBox*       m_dbMinSpin{nullptr};
    QSpinBox*       m_dbStepSpin{nullptr};

    // Section: Labels
    QComboBox*      m_freqLabelAlignCombo{nullptr}; // Left/Center/Right/Auto/Off
    QCheckBox*      m_zeroLineToggle{nullptr};
    QCheckBox*      m_showFpsToggle{nullptr};
    // G6/G9–G13 colour pickers moved to Setup → Appearance → Colors & Theme.

    // Section: Noise-Floor Tracking (Task 2.9)
    // From Thetis setup.cs:24202-24213 [v2.10.3.13] chkAdjustGridMinToNFRX1.
    // RX1 scope dropped; NereusSDR applies as global panadapter default.
    QCheckBox*   m_adjustGridMinToNF{nullptr};  // DisplayAdjustGridMinToNoiseFloor
    QSpinBox*    m_nfOffsetGridFollow{nullptr}; // DisplayNFOffsetGridFollow (dB, -60..+60)
    QCheckBox*   m_maintainNFAdjustDelta{nullptr}; // DisplayMaintainNFAdjustDelta

    // Task 2.9: Copy waterfall thresholds → spectrum min/max (reverse of Task 2.8).
    QPushButton* m_copyWfToSpecBtn{nullptr};
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
// Forward decl for the Custom-palette gradient picker (Phase 3M-5c).
class GradientPickerWidget;

// Display > TX Display
// From Thetis tpDisplayTransmit [setup.designer.cs:36232 v2.10.3.13+501e3f51].
// 3M-5b: Waterfall Amplitude Scale group is functional.
// 3M-5c: Custom Gradient Picker (visible when palette = Custom).
// Remaining groups (FFT, Panadapter, Waterfall FFT, TX Grid Scale) are
// placeholder labels naming the sub-phase that wires them.
// ---------------------------------------------------------------------------
class TxDisplayPage : public SetupPage {
    Q_OBJECT
public:
    explicit TxDisplayPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Group 4: Waterfall Amplitude Scale (functional in 3M-5b).
    // From Thetis grpTXWFAmpScale [setup.designer.cs:36246 v2.10.3.13+501e3f51].
    QSpinBox*          m_txWfLowLevelSpin{nullptr};   // udTXWFAmpMin: range -200..200, step 5, default -70 dBm
    QSpinBox*          m_txWfHighLevelSpin{nullptr};  // udTXWFAmpMax: range -200..200, step 5, default +30 dBm
    QComboBox*         m_txWfPaletteCombo{nullptr};   // comboColorPalette_tx: 7-item Thetis list
    ColorSwatchButton* m_txWfLowColorBtn{nullptr};    // clrbtnWaterfallLow_tx: default Black

    // Group 4 (cont.): Custom Gradient Picker (functional in 3M-5c).
    // From Thetis lgLinearGradientTX_waterfall [setup.designer.cs:3283 area
    // v2.10.3.13+501e3f51]. The picker row is hidden unless palette = Custom.
    GradientPickerWidget* m_txWfGradientPicker{nullptr};
    QWidget*              m_txWfGradientRowLabel{nullptr}; // form-row label widget; toggled with picker

    // Groups 1-3 (functional in 3M-5d): FFT, Panadapter, Waterfall.
    // 9 controls total -- all bound to TxAnalyzer via lambdas.
    // From Thetis tpDisplayTransmit groups groupBoxTS8 / groupBoxTS7 /
    // groupBoxTS9 at setup.designer.cs:36511-36768 [v2.10.3.13+501e3f51].
    QSlider*    m_txFftSizeSlider{nullptr};   // tbTXDisplayFFTSize (Min=0 Max=6)
    QLabel*     m_txFftSizeReadout{nullptr};  // lblTXFFT_size  (Bisque numeric)
    QLabel*     m_txBinWidthLabel{nullptr};   // lblTXDispBinWidth (Bisque numeric)
    QComboBox*  m_txWindowCombo{nullptr};     // comboTXDispWinType (7 windows)

    QComboBox*  m_txPanDetectorCombo{nullptr};   // comboTXDispPanDetector (5 items + RMS)
    QComboBox*  m_txPanAveragingCombo{nullptr};  // comboTXDispPanAveraging (4 items)
    QSpinBox*   m_txPanAvTimeSpin{nullptr};      // udTXDisplayAVGTime ms (default 30)
    QCheckBox*  m_txPanNormalizeCheck{nullptr};  // chkDispTXNormalize (gated on PanDet>=2)

    QComboBox*  m_txWfDetectorCombo{nullptr};    // comboTXDispWFDetector (4 items, no RMS)
    QComboBox*  m_txWfAveragingCombo{nullptr};   // comboTXDispWFAveraging (4 items)
    QSpinBox*   m_txWfAvTimeSpin{nullptr};       // udTXDisplayAVTime ms (default 120)
};

} // namespace NereusSDR
