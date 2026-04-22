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
    QCheckBox*         m_agcToggle{nullptr};
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
    QCheckBox*      m_dbmScaleVisibleToggle{nullptr};
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
