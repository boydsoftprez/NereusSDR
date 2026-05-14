// =================================================================
// src/gui/setup/DisplaySetupPages.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
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
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
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

#include "DisplaySetupPages.h"
#include "SetupHelpers.h"
#include "gui/SpectrumWidget.h"
#include "gui/StyleConstants.h"
#include "core/FFTEngine.h"
#include "core/ClarityController.h"
#include "core/AppSettings.h"
#include "models/Band.h"
#include "models/PanadapterModel.h"
#include "models/RadioModel.h"
#include "core/NoiseFloorTracker.h"
#include "gui/ColorSwatchButton.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QThread>
#include <QPushButton>
#include <QMessageBox>

#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Helpers (file-local)
// ---------------------------------------------------------------------------

namespace {

// Build a color swatch placeholder label (NYI — no color picker yet).
QLabel* makeColorSwatch(const QString& label, const QString& hexColor, QWidget* parent)
{
    auto* lbl = new QLabel(QStringLiteral("  %1  ").arg(label), parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: #c8d8e8; border: 1px solid #203040;"
        " border-radius: 3px; padding: 2px 6px; }").arg(hexColor));
    lbl->setFixedHeight(24);
    lbl->setEnabled(false);  // NYI
    lbl->setToolTip(QStringLiteral("Color picker — not yet implemented"));
    return lbl;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// SpectrumDefaultsPage
// ---------------------------------------------------------------------------

SpectrumDefaultsPage::SpectrumDefaultsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Spectrum Defaults"), model, parent)
{
    buildUI();
    loadFromRenderer();
}

// Maps SpectrumDefaultsPage FPS slider (10-60) to FFTEngine output FPS
// and the SpectrumWidget display timer so both stay in sync.
void SpectrumDefaultsPage::pushFps(int fps)
{
    if (!model() || !model()->fftEngine()) { return; }
    model()->fftEngine()->setOutputFps(fps);
    if (auto* sw = model()->spectrumWidget()) {
        sw->setDisplayFps(fps);
    }
    // Defensive: explicitly mirror to the spin readout.  makeSliderRow
    // wires a bidirectional slider<->spin connect that should already do
    // this, but JJ reported the spin readout sticking at 30 even when the
    // slider was driven to 60 (and the FFTEngine actually received 60
    // via this very pushFps call).  An extra setValue here is harmless
    // when the sync is working and a guaranteed fix when it isn't.
    if (m_fpSpin && m_fpSpin->value() != fps) {
        QSignalBlocker block(m_fpSpin);
        m_fpSpin->setValue(fps);
    }
}

void SpectrumDefaultsPage::loadFromRenderer()
{
    if (!model()) { return; }
    auto* sw = model()->spectrumWidget();
    auto* fe = model()->fftEngine();
    if (!sw || !fe) { return; }

    QSignalBlocker b1(m_fftSizeSlider);
    QSignalBlocker b2(m_windowCombo);
    QSignalBlocker b3(m_fpSlider);
    QSignalBlocker bHz(m_hzPerBinTargetSpin);
    QSignalBlocker b5(m_fillToggle);
    QSignalBlocker b6(m_fillAlphaSlider);
    QSignalBlocker b7(m_lineWidthSlider);
    QSignalBlocker b8(m_gradientToggle);
    QSignalBlocker b9(m_calOffsetSpin);
    QSignalBlocker b10(m_peakHoldToggle);
    QSignalBlocker b11(m_peakHoldDelaySpin);
    QSignalBlocker b12(m_threadPriorityCombo);

    // FFT size -- recover slider value from current FFT size via log2 /
    // (4096) per Thetis setup.cs:16142 [v2.10.3.13]
    //   FFTSize = 4096 * Math.Pow(2, Math.Floor(slider.Value))
    // Inverse: slider = log2(FFTSize / 4096), clamped to [0, 6].
    const int fs = fe->fftSize();
    int sliderVal = 0;
    if (fs > 4096) {
        int n = fs / 4096;
        while (n > 1 && sliderVal < 6) { n >>= 1; ++sliderVal; }
    }
    sliderVal = qBound(0, sliderVal, 6);
    m_fftSizeSlider->setValue(sliderVal);
    if (m_fftSizeReadout) {
        m_fftSizeReadout->setText(QString::number(fs));
    }
    // Bin-width prefix-style readout (Thetis-faithful).  Mirrors Thetis
    // setup.cs:16151-16152 [v2.10.3.13]: bin_width = SampleRate / FFTSize,
    // formatted "N3" (3 decimals).
    if (m_binWidthLabel) {
        const double sampleRate = fe->sampleRate();
        const double bw = (fs > 0) ? sampleRate / fs : 0.0;
        m_binWidthLabel->setText(
            bw > 0.0 ? QString::number(bw, 'f', 3) : QStringLiteral("0.000"));
    }

    // FFT window -- enum value matches combo index 1:1 (both follow WDSP
    // analyzer.c:52-173 [v2.10.3.13] case ordering).
    m_windowCombo->setCurrentIndex(static_cast<int>(fe->windowFunction()));

    m_fpSlider->setValue(fe->outputFps());
    if (m_hzPerBinTargetSpin) {
        m_hzPerBinTargetSpin->setValue(fe->hzPerBinTarget());
    }
    // Defensive: mirror to spin readout.  QSignalBlocker on m_fpSlider above
    // blocks the slider->spin sync wired by makeSliderRow (SetupHelpers.cpp:47),
    // so without this explicit setValue the spin would stick at the
    // makeSliderRow constructor default (30) on every dialog reopen even when
    // the slider correctly tracks the persisted output FPS.
    if (m_fpSpin) {
        QSignalBlocker bspin(m_fpSpin);
        m_fpSpin->setValue(fe->outputFps());
    }
    // Task 2.1: sync new split combos.
    if (m_spectrumDetectorCombo) {
        QSignalBlocker bd(m_spectrumDetectorCombo);
        m_spectrumDetectorCombo->setCurrentIndex(static_cast<int>(sw->spectrumDetector()));
    }
    if (m_spectrumAveragingCombo) {
        QSignalBlocker ba(m_spectrumAveragingCombo);
        m_spectrumAveragingCombo->setCurrentIndex(static_cast<int>(sw->spectrumAveraging()));
    }
    if (m_averagingTimeSpin) {
        QSignalBlocker bt(m_averagingTimeSpin);
        m_averagingTimeSpin->setValue(sw->spectrumAverageTimeMs());
    }
    m_fillToggle->setChecked(sw->panFillEnabled());
    m_fillAlphaSlider->setValue(static_cast<int>(sw->fillAlpha() * 100.0f));
    m_lineWidthSlider->setValue(qBound(1, static_cast<int>(sw->lineWidth()), 3));
    m_gradientToggle->setChecked(sw->gradientEnabled());
    m_calOffsetSpin->setValue(static_cast<double>(sw->dbmCalOffset()));
    m_peakHoldToggle->setChecked(sw->peakHoldEnabled());
    m_peakHoldDelaySpin->setValue(sw->peakHoldDelayMs());

    // Colour pickers (S11/S12/S13) moved to Setup → Appearance → Colors & Theme.

    // Task 2.3: sync overlay controls.
    // m_showMHzOnCursorToggle now drives the visibility flag
    // m_showCursorFreq (single source of truth, shared with the
    // SpectrumOverlayPanel button).
    if (m_showMHzOnCursorToggle) {
        QSignalBlocker b(m_showMHzOnCursorToggle);
        m_showMHzOnCursorToggle->setChecked(sw->cursorFreqVisible());
    }
    if (m_showBinWidthToggle) {
        QSignalBlocker b(m_showBinWidthToggle);
        m_showBinWidthToggle->setChecked(sw->showBinWidth());
        if (m_binWidthReadout) {
            const double bw = sw->binWidthHz();
            m_binWidthReadout->setText(
                bw > 0.0
                    ? QStringLiteral("%1 Hz/bin").arg(bw, 0, 'f', 3)
                    : QStringLiteral("— Hz/bin"));
        }
    }
    if (m_showNoiseFloorToggle) {
        QSignalBlocker b(m_showNoiseFloorToggle);
        m_showNoiseFloorToggle->setChecked(sw->showNoiseFloor());
    }
    if (m_noiseFloorPositionCombo) {
        QSignalBlocker b(m_noiseFloorPositionCombo);
        m_noiseFloorPositionCombo->setCurrentIndex(
            static_cast<int>(sw->showNoiseFloorPosition()));
    }
    if (m_nfShiftSpin) {
        QSignalBlocker b(m_nfShiftSpin);
        m_nfShiftSpin->setValue(static_cast<double>(sw->nfShiftDbm()));
    }
    if (m_nfLineWidthSpin) {
        QSignalBlocker b(m_nfLineWidthSpin);
        m_nfLineWidthSpin->setValue(static_cast<double>(sw->noiseFloorLineWidth()));
    }
    if (m_nfLineColorBtn) {
        QSignalBlocker b(m_nfLineColorBtn);
        m_nfLineColorBtn->setColor(sw->noiseFloorColor());
    }
    if (m_nfTextColorBtn) {
        QSignalBlocker b(m_nfTextColorBtn);
        m_nfTextColorBtn->setColor(sw->noiseFloorTextColor());
    }
    if (m_nfFastColorBtn) {
        QSignalBlocker b(m_nfFastColorBtn);
        m_nfFastColorBtn->setColor(sw->noiseFloorFastColor());
    }
    if (m_dispNormalizeToggle) {
        QSignalBlocker b(m_dispNormalizeToggle);
        m_dispNormalizeToggle->setChecked(sw->dispNormalize());
    }
    if (m_showPeakValueOverlayToggle) {
        QSignalBlocker b(m_showPeakValueOverlayToggle);
        m_showPeakValueOverlayToggle->setChecked(sw->showPeakValueOverlay());
    }
    if (m_peakValuePositionCombo) {
        QSignalBlocker b(m_peakValuePositionCombo);
        m_peakValuePositionCombo->setCurrentIndex(
            static_cast<int>(sw->peakValuePosition()));
    }
    if (m_peakTextDelaySpin) {
        QSignalBlocker b(m_peakTextDelaySpin);
        m_peakTextDelaySpin->setValue(sw->peakTextDelayMs());
    }
    if (m_decimationSpin && fe) {
        QSignalBlocker b(m_decimationSpin);
        m_decimationSpin->setValue(fe->decimation());
    }
}

void SpectrumDefaultsPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // Phase 3G-9b: Reset to Smooth Defaults button. Destructive — shows
    // a confirmation dialog before overwriting because it resets the
    // Spectrum / Waterfall display state.
    auto* resetBtn = new QPushButton(QStringLiteral("Reset to Smooth Defaults"), this);
    resetBtn->setToolTip(QStringLiteral(
        "Overwrite the Spectrum and Waterfall display settings with the "
        "NereusSDR smooth-default profile (Clarity Blue palette, "
        "log-recursive averaging, tight threshold gap, waterfall AGC on). "
        "Intended to recover the out-of-box look after experimentation. "
        "FFT size, frequency, band stack, and per-band grid slots are "
        "not affected."));
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        const auto rc = QMessageBox::question(
            this,
            QStringLiteral("Reset to Smooth Defaults"),
            QStringLiteral(
                "This will overwrite your current Spectrum and Waterfall "
                "display settings with the NereusSDR smooth-default profile.\n\n"
                "Your FFT size, frequency, band stack, and per-band grid "
                "slots are NOT affected.\n\n"
                "Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (rc != QMessageBox::Yes) { return; }
        if (model()) {
            model()->applyClaritySmoothDefaults();
            // Reload this page so the controls immediately reflect the
            // new model state. Sibling pages (Waterfall Defaults, Grid &
            // Scales) will update on their next SetupDialog repaint
            // because their loadFromRenderer() reads from the model.
            loadFromRenderer();
        }
    });
    contentLayout()->addWidget(resetBtn);

    // Phase 3G-9c: Clarity adaptive display tuning master toggle.
    // When on, ClarityController drives Waterfall Low/High thresholds
    // from a percentile-based noise-floor estimate. When off, thresholds
    // stay at whatever value they were last set to (no AGC fallback).
    auto* clarityToggle = new QCheckBox(
        QStringLiteral("Enable Clarity (adaptive waterfall tuning)"), this);
    clarityToggle->setToolTip(QStringLiteral(
        "Clarity keeps the waterfall thresholds centered on the actual "
        "noise floor as band conditions and tuning change. Uses a 30th-"
        "percentile estimator with 3-second EWMA smoothing and a ±2 dB "
        "deadband. When off, thresholds are fixed at their last values."));
    if (auto* cc = model() ? model()->clarityController() : nullptr) {
        clarityToggle->setChecked(cc->isEnabled());
        connect(clarityToggle, &QCheckBox::toggled, this, [this](bool on) {
            if (auto* cc2 = model() ? model()->clarityController() : nullptr) {
                cc2->setEnabled(on);
                AppSettings::instance().setValue(
                    QStringLiteral("ClarityEnabled"),
                    on ? QStringLiteral("True") : QStringLiteral("False"));
            }
            // Sync the clarityActive flag so legacy AGC knows whether
            // to stand down. Off = AGC free to run, On = AGC yields
            // once Clarity emits its first threshold update.
            if (auto* sw2 = model() ? model()->spectrumWidget() : nullptr) {
                if (!on) { sw2->setClarityActive(false); }
            }
        });
    }
    contentLayout()->addWidget(clarityToggle);

    auto* sw = model() ? model()->spectrumWidget() : nullptr;
    auto* fe = model() ? model()->fftEngine() : nullptr;

    // --- Section: Fast Fourier Transform ---
    // Layout mirrors Thetis grpDisplayRX1Pan at setup.designer.cs:34920-35054
    // [v2.10.3.13].  Group title verbatim "Fast Fourier Transform" (per
    // :34937).  Inner controls (10 total in Thetis):
    //   - "Size" centered header label (labelTS139 @ x=118)
    //   - tbDisplayFFTSize slider (Max=6, Value=5)
    //   - "Min" / "Max" endpoint labels flanking the slider (labelTS145 / 146)
    //   - "Bin Width (Hz)" prefix + bisque numeric readout (labelTS142 +
    //     lblDisplayBinWidth at y=79)
    //   - "FFT Size" prefix + bisque numeric readout (labelTS425 +
    //     lblRX1FFT_size at y=79)  -- BOTH readouts on the same row
    //   - "Window" prefix + combo (labelTS147 + comboDispWinType at y=101)
    //
    // Qt port uses QGridLayout (4 cols x 4 rows) since QFormLayout cannot
    // express the 4-cell "prefix + value + prefix + value" pattern Thetis
    // uses for the bin-width / FFT-size readout row.
    auto* fftGroup = new QGroupBox(
        QStringLiteral("Fast Fourier Transform"), this);
    auto* fftGrid = new QGridLayout(fftGroup);
    fftGrid->setSpacing(6);
    fftGrid->setColumnStretch(0, 0);
    fftGrid->setColumnStretch(1, 1);   // bin-width value cell stretches
    fftGrid->setColumnStretch(2, 0);
    fftGrid->setColumnStretch(3, 1);   // FFT-size value cell stretches

    // Stylesheet for sunken numeric readouts.  Thetis uses Bisque
    // (System.Drawing.Color.Bisque, RGB(255, 228, 196)) on a light
    // WinForms background.  NereusSDR's dark theme inverts the contrast:
    // recessed dark inset with cyan-accent text reads as "live data
    // cell" against the page's slightly-lighter group background.
    // Token sources:
    //   kInsetBg     = #0a0a18  (StyleConstants.h:53)
    //   kInsetBorder = #1e2e3e  (StyleConstants.h:54)
    //   kAccent      = #00b4d8  (StyleConstants.h:44)  -- cyan readout text
    const QString readoutStyle = QStringLiteral(
        "QLabel { background-color: #0a0a18; color: #00b4d8; "
        "border: 1px solid #1e2e3e; padding: 1px 6px; "
        "font-family: Menlo, Consolas, monospace; }");

    // Row 0: centered "Size" header label.  Mirrors Thetis labelTS139
    // ("Size") at (118, 13) [v2.10.3.13] -- centered horizontally over
    // the slider's middle.
    auto* sizeHeader = new QLabel(QStringLiteral("Size"), fftGroup);
    sizeHeader->setAlignment(Qt::AlignHCenter);
    fftGrid->addWidget(sizeHeader, 0, 1, 1, 2);  // span cols 1-2 (centered)

    // Row 1: "Min" + slider + "Max".  Min/Max are Thetis labelTS145/146
    // at (6, 41) / (227, 41) [v2.10.3.13] -- show the slider's value
    // extremes textually since the slider has no tick marks.
    auto* minLabel = new QLabel(QStringLiteral("Min"), fftGroup);
    auto* maxLabel = new QLabel(QStringLiteral("Max"), fftGroup);
    minLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    maxLabel->setAlignment(Qt::AlignLeft  | Qt::AlignVCenter);

    // Phase 2: FFT size slider, range 0..6, default 5 (=131072 bins).
    // From Thetis setup.designer.cs:35043-35053 [v2.10.3.13] tbDisplayFFTSize
    // (Maximum=6, Value=5, no tick marks).  Mapping per setup.cs:16148:
    //   FFTSize = 4096 * Math.Pow(2, Math.Floor(slider.Value))
    // i.e. slider 0..6 -> {4096, 8192, 16384, 32768, 65536, 131072, 262144}.
    m_fftSizeSlider = new QSlider(Qt::Horizontal, fftGroup);
    m_fftSizeSlider->setRange(0, 6);
    m_fftSizeSlider->setSingleStep(1);
    m_fftSizeSlider->setPageStep(1);
    m_fftSizeSlider->setTickPosition(QSlider::NoTicks);
    m_fftSizeSlider->setValue(5);
    m_fftSizeSlider->setToolTip(QStringLiteral(
        "FFT size used for spectrum analysis. Range 4096 to 262144 in "
        "powers of two. Larger = finer frequency resolution and a smaller "
        "bin width, at higher CPU cost."));
    fftGrid->addWidget(minLabel,         1, 0);
    fftGrid->addWidget(m_fftSizeSlider,  1, 1, 1, 2);  // span cols 1-2
    fftGrid->addWidget(maxLabel,         1, 3);

    // Row 2: bin-width readout + FFT-size readout, side by side per
    // Thetis (both at y=79).  Each pair is a "prefix label + bisque
    // sunken numeric cell".

    // "Bin Width (Hz)" prefix + value cell.  Thetis labelTS142
    // ("Bin Width (Hz)") at (6, 79) + lblDisplayBinWidth at (87, 79)
    // [v2.10.3.13].
    auto* binWidthPrefix =
        new QLabel(QStringLiteral("Bin Width (Hz)"), fftGroup);
    m_binWidthLabel = new QLabel(QStringLiteral("0.000"), fftGroup);
    m_binWidthLabel->setMinimumWidth(54);
    m_binWidthLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_binWidthLabel->setStyleSheet(readoutStyle);
    m_binWidthLabel->setToolTip(QStringLiteral(
        "Live bin width in Hz (sample rate / FFT size)."));

    // "FFT Size" prefix + value cell.  Thetis labelTS425 ("FFT Size") at
    // (143, 79) + lblRX1FFT_size at (196, 79) [v2.10.3.13].
    auto* fftSizePrefix = new QLabel(QStringLiteral("FFT Size"), fftGroup);
    m_fftSizeReadout = new QLabel(QStringLiteral("131072"), fftGroup);
    m_fftSizeReadout->setMinimumWidth(54);
    m_fftSizeReadout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_fftSizeReadout->setStyleSheet(readoutStyle);
    m_fftSizeReadout->setToolTip(QStringLiteral(
        "Current FFT size in bins (4096 * 2^slider)."));

    fftGrid->addWidget(binWidthPrefix,   2, 0);
    fftGrid->addWidget(m_binWidthLabel,  2, 1);
    fftGrid->addWidget(fftSizePrefix,    2, 2);
    fftGrid->addWidget(m_fftSizeReadout, 2, 3);

    // Row 3: "Window" prefix + combo.  Thetis labelTS147 ("Window") at
    // (6, 101) + comboDispWinType at (83, 101) [v2.10.3.13].
    auto* windowPrefix = new QLabel(QStringLiteral("Window"), fftGroup);
    m_windowCombo = new QComboBox(fftGroup);
    // 7 items, ordering verbatim per Thetis comboDispWinType.Items at
    // setup.designer.cs:34966-34973 [v2.10.3.13].  Combo index maps 1:1
    // to WindowFunction enum integer (both follow WDSP analyzer.c case
    // ordering, so no remap needed).
    m_windowCombo->addItems({
        QStringLiteral("Rectangular"),         // WindowFunction::Rectangular     (0)
        QStringLiteral("Blackman-Harris 4T"),  // WindowFunction::BlackmanHarris4 (1)
        QStringLiteral("Hann"),                // WindowFunction::Hann            (2)
        QStringLiteral("Flat-Top"),            // WindowFunction::FlatTop         (3)
        QStringLiteral("Hamming"),             // WindowFunction::Hamming         (4)
        QStringLiteral("Kaiser"),              // WindowFunction::Kaiser          (5)
        QStringLiteral("Blackman-Harris 7T")   // WindowFunction::BlackmanHarris7 (6)
    });
    m_windowCombo->setToolTip(QStringLiteral(
        "FFT window function. Rectangular has the narrowest main lobe but "
        "the worst sidelobes. Blackman-Harris (4T or 7T) gives strong "
        "sidelobe rejection. Flat-Top is best for amplitude calibration. "
        "Kaiser is parameterised (KaiserPi shape parameter)."));
    fftGrid->addWidget(windowPrefix,  3, 0);
    fftGrid->addWidget(m_windowCombo, 3, 1, 1, 3);  // span cols 1-3

    // Row 4: Hz/bin Target — NereusSDR-original auto-zoom override (Option
    // 3, 2026-05-08).  0 = off (bins-in-window default behaviour: FFT
    // replans on zoom to keep ~baseline bins in the visible window, Hz/bin
    // varies).  > 0 = lock Hz/bin at the given value regardless of zoom
    // (useful for hunting narrow CW/digital signals where you want the
    // same frequency resolution at any zoom).
    auto* hzPerBinPrefix = new QLabel(QStringLiteral("Hz/bin Target"), fftGroup);
    m_hzPerBinTargetSpin = new QDoubleSpinBox(fftGroup);
    m_hzPerBinTargetSpin->setRange(0.0, 200.0);
    m_hzPerBinTargetSpin->setSingleStep(0.5);
    m_hzPerBinTargetSpin->setDecimals(2);
    m_hzPerBinTargetSpin->setSpecialValueText(QStringLiteral("Off"));
    m_hzPerBinTargetSpin->setSuffix(QStringLiteral(" Hz/bin"));
    m_hzPerBinTargetSpin->setToolTip(QStringLiteral(
        "Auto-zoom override: target a constant Hz/bin regardless of zoom. "
        "Set to 0 (\"Off\") to use the default bins-in-window behaviour "
        "(FFT replans on zoom). When > 0, the FFT size is fixed at "
        "sampleRate / target so the trace delivers the requested resolution "
        "at any zoom level — handy for hunting narrow signals (CW, digital). "
        "Floor at the FFT slider value still applies (slider sets the "
        "minimum FFT size)."));
    connect(m_hzPerBinTargetSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (auto* fe = model() ? model()->fftEngine() : nullptr) {
            fe->setHzPerBinTarget(v);
        }
        AppSettings::instance().setValue(
            QStringLiteral("DisplayHzPerBinTarget"),
            QString::number(v));
        // Trigger immediate replan so the new policy applies without
        // waiting for the next zoom action.
        if (auto* sw = model() ? model()->spectrumWidget() : nullptr) {
            sw->requestAutoZoomReplan();
        }
    });
    fftGrid->addWidget(hzPerBinPrefix,        4, 0);
    fftGrid->addWidget(m_hzPerBinTargetSpin,  4, 1, 1, 3);  // span cols 1-3

    // Slider valueChanged handler.  Mirrors Thetis tbDisplayFFTSize_Scroll
    // at setup.cs:16142-16166 [v2.10.3.13] -- the nine-action sequence
    // (skip mutex/UpdateRXSpectrumDisplayVars/InitFFTFillTime per design
    // notes; everything else ported).
    connect(m_fftSizeSlider, &QSlider::valueChanged,
            this, [this](int v) {
        v = qBound(0, v, 6);
        const int newSize = 4096 << v;

        // Size readout (mirrors lblRX1FFT_size update at setup.cs:16153
        // [v2.10.3.13]).
        if (m_fftSizeReadout) {
            m_fftSizeReadout->setText(QString::number(newSize));
        }

        if (!model()) { return; }
        auto* fe = model()->fftEngine();
        auto* sw = model()->spectrumWidget();

        // FFT size set.  Atomic store + deferred replan + emits
        // spectrumSettingsChanged when oldSize != newSize.
        // Baseline is set FIRST so the auto-zoom lambda (wired to
        // bandwidthChangeRequested) treats this slider value as the new
        // K target on its next bandwidth event.  Without this the zoom
        // lambda would compute against a stale baseline.
        if (fe) {
            fe->setFftSizeBaseline(newSize);
            fe->setFftSize(newSize);
        }

        // Bin-width readout, fresh from newSize.  Format "N3" = 3 decimal
        // places per Thetis setup.cs:16152 [v2.10.3.13].
        if (m_binWidthLabel && fe) {
            const double sampleRate = fe->sampleRate();
            const double bw = (newSize > 0) ? sampleRate / newSize : 0.0;
            m_binWidthLabel->setText(
                bw > 0.0 ? QString::number(bw, 'f', 3)
                         : QStringLiteral("0.000"));
        }

        // Mirror to on-spectrum overlay (NereusSDR-original; the
        // overlay toggle gates the spectrum-corner readout).
        if (m_binWidthReadout && sw) {
            const double bw = sw->binWidthHz();
            m_binWidthReadout->setText(
                bw > 0.0 ? QStringLiteral("%1 Hz/bin").arg(bw, 0, 'f', 3)
                         : QStringLiteral("- Hz/bin"));
        }

        // FFTSizeOffset = slider.Value * 2 dB.  Mirrors Thetis
        // setup.cs:16154 [v2.10.3.13]:
        //   Display.RX1FFTSizeOffset = tbDisplayFFTSize.Value * 2;
        // Subtracted from agc_cal_offset in RadioModel auto-AGC (full
        // formula at console.cs:33304).
        if (fe) { fe->setFftSizeOffsetDb(static_cast<double>(v) * 2.0); }

        // Fast-attack noise-floor reset.  Mirrors Thetis setup.cs:16156:
        //   Display.FastAttackNoiseFloorRX1 = true;
        if (auto* nf = model()->noiseFloorTracker()) {
            nf->triggerFastAttack();
        }
    });

    // Window combo handler.
    connect(m_windowCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (!model() || !model()->fftEngine()) { return; }
        const int clamped = qBound(0,
            i,
            static_cast<int>(WindowFunction::Count) - 1);
        model()->fftEngine()->setWindowFunction(
            static_cast<WindowFunction>(clamped));
    });

    contentLayout()->addWidget(fftGroup);

    // --- Section: Rendering ---
    auto* renderGroup = new QGroupBox(QStringLiteral("Rendering"), this);
    auto* renderForm  = new QFormLayout(renderGroup);
    renderForm->setSpacing(6);

    {
        auto row = makeSliderRow(10, 60, 30, QStringLiteral(" fps"), renderGroup);
        m_fpSlider = row.slider;
        m_fpSpin   = row.spin;
        // Thetis: setup.designer.cs:33856 (udDisplayFPS) — Thetis original: "Frames Per Second (approximate)" (placeholder); rewritten
        m_fpSlider->setToolTip(QStringLiteral("Spectrum/waterfall redraw rate. Higher = smoother animation at cost of CPU."));
        row.spin->setToolTip(QStringLiteral("Spectrum/waterfall redraw rate. Higher = smoother animation at cost of CPU."));
        connect(m_fpSlider, &QSlider::valueChanged, this, [this](int v) { pushFps(v); });
        renderForm->addRow(QStringLiteral("FPS:"), row.container);
    }

    // Legacy "Averaging" combo (None / Weighted / Logarithmic / Time Window)
    // was a NereusSDR-only UI duplicate that pre-dated the Thetis-faithful
    // split below. Its setAverageMode() calls write only to m_averageMode
    // which the renderer no longer reads — m_spectrumAveraging drives the
    // actual smoothing. Combo removed to stop confusing users with two
    // averaging mode pickers; the legacy enum + setter survive for any
    // external callers and migrate via SettingsSchemaVersion=4 below.

    // Task 2.1: Detector + Averaging split (handwave fix from 3G-8).
    // Ported from Thetis comboDispPanDetector [v2.10.3.13]
    // (setup.designer.cs:34876): Peak / Rosenfell / Average / Sample / RMS.
    // RX1 scope dropped — pan-agnostic per design Section 1B.
    m_spectrumDetectorCombo = new QComboBox(renderGroup);
    m_spectrumDetectorCombo->addItems({
        QStringLiteral("Peak"),
        QStringLiteral("Rosenfell"),
        QStringLiteral("Average"),
        QStringLiteral("Sample"),
        QStringLiteral("RMS")
    });
    // Thetis: setup.designer.cs:34876 (comboDispPanDetector) [v2.10.3.13] — no upstream tooltip; rewritten
    m_spectrumDetectorCombo->setToolTip(QStringLiteral(
        "Spectrum bin-reduction policy. Peak takes the maximum bin in each display pixel. "
        "Rosenfell alternates max/min for a classic spectrum look. Average takes the "
        "arithmetic mean. Sample takes the first bin. RMS computes root-mean-square power."));
    connect(m_spectrumDetectorCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setSpectrumDetector(static_cast<SpectrumDetector>(i));
        }
    });
    renderForm->addRow(QStringLiteral("Spectrum Detector:"), m_spectrumDetectorCombo);

    // Ported from Thetis comboDispPanAveraging [v2.10.3.13]
    // (setup.designer.cs:34835): None / Recursive / Time Window / Log Recursive.
    m_spectrumAveragingCombo = new QComboBox(renderGroup);
    m_spectrumAveragingCombo->addItems({
        QStringLiteral("None"),
        QStringLiteral("Recursive"),
        QStringLiteral("Time Window"),
        QStringLiteral("Log Recursive")
    });
    // Thetis: setup.designer.cs:34835 (comboDispPanAveraging) [v2.10.3.13] — no upstream tooltip; rewritten
    m_spectrumAveragingCombo->setToolTip(QStringLiteral(
        "Spectrum frame-averaging mode. None shows raw FFT output per frame. "
        "Recursive exponentially smooths in linear power space. "
        "Time Window approximates a sliding average. "
        "Log Recursive smooths in the dB domain for a more natural look at low levels."));
    connect(m_spectrumAveragingCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setSpectrumAveraging(static_cast<SpectrumAveraging>(i));
        }
    });
    renderForm->addRow(QStringLiteral("Spectrum Averaging:"), m_spectrumAveragingCombo);

    // Spectrum (panadapter) averaging time constant.
    // From Thetis udDisplayAVGTime [v2.10.3.13] (setup.designer.cs:34902).
    // Default 30 ms matches Thetis. Range 10..9999 ms (Thetis 1..9999, but
    // the lower bound was bumped to 10 ms to keep the UI step coherent).
    // Spec widget computes α = exp(-1/(fps×τ)) internally; no hand-rolled
    // ms→alpha math here.
    m_averagingTimeSpin = new QSpinBox(renderGroup);
    m_averagingTimeSpin->setRange(10, 9999);
    m_averagingTimeSpin->setSingleStep(10);
    m_averagingTimeSpin->setSuffix(QStringLiteral(" ms"));
    m_averagingTimeSpin->setValue(30);
    m_averagingTimeSpin->setToolTip(QStringLiteral(
        "Spectrum averaging time constant. Larger = heavier smoothing, "
        "slower response. Translates to a frame-by-frame alpha via "
        "α = exp(−1 / (fps × τ)) per Thetis."));
    connect(m_averagingTimeSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int ms) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setSpectrumAverageTimeMs(ms);
        }
    });
    renderForm->addRow(QStringLiteral("Spectrum Avg Time:"), m_averagingTimeSpin);

    m_decimationSpin = new QSpinBox(renderGroup);
    m_decimationSpin->setRange(1, 32);
    m_decimationSpin->setValue(1);
    // Thetis: setup.designer.cs:33732 (udDisplayDecimation) [v2.10.3.13]
    // Thetis original: "Display decimation. Higher the number, the lower the resolution."
    m_decimationSpin->setToolTip(QStringLiteral("Display decimation. Higher the number, the lower the resolution."));
    // Task 2.3: wire to FFTEngine::setDecimation().
    connect(m_decimationSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (model() && model()->fftEngine()) {
            model()->fftEngine()->setDecimation(v);
        }
    });
    renderForm->addRow(QStringLiteral("Decimation:"), m_decimationSpin);

    m_fillToggle = new QCheckBox(QStringLiteral("Fill under trace"), renderGroup);
    // Thetis: setup.designer.cs:33749 (chkDisplayPanFill)
    m_fillToggle->setToolTip(QStringLiteral("Check to fill the panadapter display line below the data."));
    connect(m_fillToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setPanFillEnabled(on);
        }
    });
    renderForm->addRow(QString(), m_fillToggle);

    {
        auto row = makeSliderRow(0, 100, 70, QStringLiteral("%"), renderGroup);
        m_fillAlphaSlider = row.slider;
        // Thetis: setup.designer.cs:3215 (tbDataFillAlpha) — no upstream tooltip; rewritten
        // Thetis original: (none)
        m_fillAlphaSlider->setToolTip(QStringLiteral("Opacity of the fill area under the spectrum trace (0 = transparent, 100 = opaque)."));
        row.spin->setToolTip(QStringLiteral("Opacity of the fill area under the spectrum trace (0 = transparent, 100 = opaque)."));
        connect(m_fillAlphaSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setFillAlpha(v / 100.0f);
            }
        });
        renderForm->addRow(QStringLiteral("Fill Alpha:"), row.container);
    }

    {
        auto row = makeSliderRow(1, 3, 1, QStringLiteral(" px"), renderGroup);
        m_lineWidthSlider = row.slider;
        // Thetis: setup.designer.cs:3228 (udDisplayLineWidth) — no upstream tooltip; rewritten
        // Thetis original: (none)
        m_lineWidthSlider->setToolTip(QStringLiteral("Spectrum trace line width in pixels (1–3 px)."));
        row.spin->setToolTip(QStringLiteral("Spectrum trace line width in pixels (1–3 px)."));
        connect(m_lineWidthSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setLineWidth(static_cast<float>(v));
            }
        });
        renderForm->addRow(QStringLiteral("Line Width:"), row.container);
    }

    m_gradientToggle = new QCheckBox(QStringLiteral("Trace gradient"), renderGroup);
    // Thetis: setup.designer.cs:53918 (chkDataLineGradient) — rewritten (grammar fix)
    // Thetis original: "The data line is also uses the gradient if checked"
    m_gradientToggle->setToolTip(QStringLiteral("When checked, the spectrum trace line renders with the gradient color applied."));
    connect(m_gradientToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setGradientEnabled(on);
        }
    });
    renderForm->addRow(QString(), m_gradientToggle);

    contentLayout()->addWidget(renderGroup);

    // --- Section: Calibration ---
    auto* calGroup = new QGroupBox(QStringLiteral("Calibration & Peak Hold"), this);
    auto* calForm  = new QFormLayout(calGroup);
    calForm->setSpacing(6);

    m_calOffsetSpin = new QDoubleSpinBox(calGroup);
    m_calOffsetSpin->setRange(-30.0, 30.0);
    m_calOffsetSpin->setSingleStep(0.5);
    m_calOffsetSpin->setSuffix(QStringLiteral(" dBm"));
    m_calOffsetSpin->setValue(0.0);
    // Thetis: display.cs:1372 (Display.RX1DisplayCalOffset) — programmatic only, no designer tooltip; rewritten
    // Thetis original: (none — set programmatically via Display.RX1DisplayCalOffset)
    m_calOffsetSpin->setToolTip(QStringLiteral("dBm calibration offset applied to all displayed signal levels. Use to match a calibrated reference."));
    connect(m_calOffsetSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setDbmCalOffset(static_cast<float>(v));
        }
    });
    calForm->addRow(QStringLiteral("Cal Offset:"), m_calOffsetSpin);

    m_peakHoldToggle = new QCheckBox(QStringLiteral("Peak hold"), calGroup);
    // NereusSDR extension — no Thetis equivalent
    m_peakHoldToggle->setToolTip(QStringLiteral("When enabled, the highest signal level seen at each frequency bin is held on the display."));
    connect(m_peakHoldToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setPeakHoldEnabled(on);
        }
    });
    calForm->addRow(QString(), m_peakHoldToggle);

    m_peakHoldDelaySpin = new QSpinBox(calGroup);
    m_peakHoldDelaySpin->setRange(100, 10000);
    m_peakHoldDelaySpin->setSingleStep(100);
    m_peakHoldDelaySpin->setSuffix(QStringLiteral(" ms"));
    m_peakHoldDelaySpin->setValue(2000);
    // NereusSDR extension — no Thetis equivalent
    m_peakHoldDelaySpin->setToolTip(QStringLiteral("Time in milliseconds before a held peak begins to decay back toward the live trace."));
    connect(m_peakHoldDelaySpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setPeakHoldDelayMs(v);
        }
    });
    calForm->addRow(QStringLiteral("Peak Delay:"), m_peakHoldDelaySpin);

    contentLayout()->addWidget(calGroup);

    // --- Section: Spectrum Overlays (Task 2.3) ---
    // Groups the 4 corner-text overlay controls as a discrete block.
    // Helper lambda for the 4-item position combo (shared by NF and Peak).
    auto makeOverlayPositionCombo = [](QWidget* parent) -> QComboBox* {
        auto* c = new QComboBox(parent);
        c->addItems({QStringLiteral("Top Left"),    QStringLiteral("Top Right"),
                     QStringLiteral("Bottom Left"), QStringLiteral("Bottom Right")});
        return c;
    };

    auto* overlayGroup = new QGroupBox(QStringLiteral("Spectrum Overlays"), this);
    auto* overlayForm  = new QFormLayout(overlayGroup);
    overlayForm->setSpacing(6);

    // Show cursor frequency — visibility-only toggle.  Originally drove the
    // retired m_showMHzOnCursor format flag (Thetis chkShowMHzOnCursor,
    // setup.designer.cs:33043), but the Hz alternative was dropped in
    // 2026-05 and the flag retired; this checkbox now drives the same
    // visibility state as the SpectrumOverlayPanel "Cursor Freq" button
    // (m_showCursorFreq / DisplayShowCursorFreq) — single source of truth.
    m_showMHzOnCursorToggle = new QCheckBox(
        QStringLiteral("Show cursor frequency"), overlayGroup);
    m_showMHzOnCursorToggle->setToolTip(QStringLiteral(
        "Display the frequency at the cursor position (always in MHz). "
        "Same toggle as the on-spectrum overlay-panel Cursor Freq button."));
    connect(m_showMHzOnCursorToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setCursorFreqVisible(on);
        }
        AppSettings::instance().setValue(
            QStringLiteral("DisplayShowCursorFreq"),
            on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    overlayForm->addRow(QString(), m_showMHzOnCursorToggle);

    // ShowBinWidth — live bin-width readout.
    // From Thetis setup.cs:7061 [v2.10.3.13] lblDisplayBinWidth.
    m_showBinWidthToggle = new QCheckBox(
        QStringLiteral("Show bin width"), overlayGroup);
    m_showBinWidthToggle->setToolTip(QStringLiteral(
        "Display the current FFT bin width (sample rate / FFT size) in the spectrum corner."));
    m_binWidthReadout = new QLabel(QStringLiteral("— Hz/bin"), overlayGroup);
    m_binWidthReadout->setToolTip(QStringLiteral("Current FFT bin width in Hz."));
    connect(m_showBinWidthToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowBinWidth(on);
            if (on) {
                const double bw = w->binWidthHz();
                m_binWidthReadout->setText(
                    bw > 0.0
                        ? QStringLiteral("%1 Hz/bin").arg(bw, 0, 'f', 3)
                        : QStringLiteral("— Hz/bin"));
            }
        }
        AppSettings::instance().setValue(
            QStringLiteral("DisplayShowBinWidth"),
            on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    {
        auto* row = new QWidget(overlayGroup);
        auto* hl  = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(m_showBinWidthToggle);
        hl->addSpacing(8);
        hl->addWidget(m_binWidthReadout);
        hl->addStretch();
        overlayForm->addRow(QString(), row);
    }

    // ShowNoiseFloor — toggle + position (deprecated) + NF render parameters.
    // From Thetis display.cs:2304-2337 + 5400-5448 + 5763 [v2.10.3.13]:
    //   m_bShowNoiseFloorDBM   — toggle
    //   noisefloor_color       — line + box colour (default red)
    //   noisefloor_color_text  — dBm label colour (default yellow)
    //   m_fNoiseFloorLineWidth — line width (default 1.0)
    //   _fNFshiftDBM           — operator shift, [-12, +12]
    // Position combo is dead (text now anchors to the box per Thetis layout)
    // — kept for settings round-trip but disabled in UI.
    m_showNoiseFloorToggle = new QCheckBox(
        QStringLiteral("Show noise floor"), overlayGroup);
    m_showNoiseFloorToggle->setToolTip(QStringLiteral(
        "Display the noise floor as a horizontal dashed line + dBm box+text."));
    m_noiseFloorPositionCombo = makeOverlayPositionCombo(overlayGroup);
    m_noiseFloorPositionCombo->setCurrentIndex(2);  // Bottom Left default
    m_noiseFloorPositionCombo->setEnabled(false);
    m_noiseFloorPositionCombo->setToolTip(QStringLiteral(
        "Deprecated. Text now anchors to the NF box per Thetis "
        "(display.cs:5443). Setting persists but has no visual effect."));
    connect(m_showNoiseFloorToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowNoiseFloor(on);
        }
        AppSettings::instance().setValue(
            QStringLiteral("DisplayShowNoiseFloor"),
            on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    connect(m_noiseFloorPositionCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowNoiseFloorPosition(
                static_cast<SpectrumWidget::OverlayPosition>(i));
        }
        AppSettings::instance().setValue(
            QStringLiteral("DisplayShowNoiseFloorPosition"),
            QString::number(i));
    });
    {
        auto* row = new QWidget(overlayGroup);
        auto* hl  = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(m_showNoiseFloorToggle);
        hl->addSpacing(8);
        hl->addWidget(m_noiseFloorPositionCombo);
        hl->addStretch();
        overlayForm->addRow(QString(), row);
    }

    // NF Shift + Line Width — Thetis display.cs:5400 + 2310.
    {
        m_nfShiftSpin = new QDoubleSpinBox(overlayGroup);
        m_nfShiftSpin->setRange(-12.0, 12.0);
        m_nfShiftSpin->setSingleStep(0.5);
        m_nfShiftSpin->setDecimals(1);
        m_nfShiftSpin->setSuffix(QStringLiteral(" dB"));
        m_nfShiftSpin->setToolTip(QStringLiteral(
            "Operator-tunable offset added to the rendered NF level. "
            "Thetis _fNFshiftDBM, clamped to [-12, +12]."));
        connect(m_nfShiftSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setNFShiftDbm(static_cast<float>(v));
            }
            AppSettings::instance().setValue(
                QStringLiteral("DisplayNoiseFloorShiftDb"),
                QString::number(v));
        });

        m_nfLineWidthSpin = new QDoubleSpinBox(overlayGroup);
        m_nfLineWidthSpin->setRange(1.0, 5.0);
        m_nfLineWidthSpin->setSingleStep(0.5);
        m_nfLineWidthSpin->setDecimals(1);
        m_nfLineWidthSpin->setSuffix(QStringLiteral(" px"));
        m_nfLineWidthSpin->setToolTip(QStringLiteral(
            "Width of the horizontal NF dashed line. "
            "Thetis m_fNoiseFloorLineWidth, default 1.0."));
        connect(m_nfLineWidthSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setNoiseFloorLineWidth(static_cast<float>(v));
            }
            AppSettings::instance().setValue(
                QStringLiteral("DisplayNoiseFloorLineWidth"),
                QString::number(v));
        });

        auto* row = new QWidget(overlayGroup);
        auto* hl  = new QHBoxLayout(row);
        hl->setContentsMargins(20, 0, 0, 0);  // indent under the toggle
        hl->addWidget(new QLabel(QStringLiteral("NF shift:"), row));
        hl->addWidget(m_nfShiftSpin);
        hl->addSpacing(12);
        hl->addWidget(new QLabel(QStringLiteral("Line width:"), row));
        hl->addWidget(m_nfLineWidthSpin);
        hl->addStretch();
        overlayForm->addRow(QString(), row);
    }

    // NF colour pickers — Thetis display.cs:2316/2329 + NereusSDR fast-attack.
    {
        m_nfLineColorBtn = new ColorSwatchButton(Qt::red, overlayGroup);
        m_nfLineColorBtn->setToolTip(QStringLiteral(
            "Colour for the NF line + 8x8 box. Thetis default red."));
        connect(m_nfLineColorBtn, &ColorSwatchButton::colorChanged,
                this, [this](const QColor& c) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setNoiseFloorColor(c);
            }
            // HexArgb keeps NF colour storage in the same format as the
            // rest of SpectrumWidget's persisted colours.
            AppSettings::instance().setValue(
                QStringLiteral("DisplayNoiseFloorColor"),
                c.name(QColor::HexArgb));
        });

        m_nfTextColorBtn = new ColorSwatchButton(Qt::yellow, overlayGroup);
        m_nfTextColorBtn->setToolTip(QStringLiteral(
            "Colour for the NF dBm label text. Thetis default yellow."));
        connect(m_nfTextColorBtn, &ColorSwatchButton::colorChanged,
                this, [this](const QColor& c) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setNoiseFloorTextColor(c);
            }
            AppSettings::instance().setValue(
                QStringLiteral("DisplayNoiseFloorTextColor"),
                c.name(QColor::HexArgb));
        });

        m_nfFastColorBtn = new ColorSwatchButton(Qt::gray, overlayGroup);
        m_nfFastColorBtn->setToolTip(QStringLiteral(
            "Colour shown during fast-attack (band/freq/MOX change). "
            "Default gray, mirroring Thetis m_bDX2_Gray."));
        connect(m_nfFastColorBtn, &ColorSwatchButton::colorChanged,
                this, [this](const QColor& c) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setNoiseFloorFastColor(c);
            }
            AppSettings::instance().setValue(
                QStringLiteral("DisplayNoiseFloorFastColor"),
                c.name(QColor::HexArgb));
        });

        auto* row = new QWidget(overlayGroup);
        auto* hl  = new QHBoxLayout(row);
        hl->setContentsMargins(20, 0, 0, 0);  // indent under the toggle
        hl->addWidget(new QLabel(QStringLiteral("Line:"), row));
        hl->addWidget(m_nfLineColorBtn);
        hl->addSpacing(8);
        hl->addWidget(new QLabel(QStringLiteral("Text:"), row));
        hl->addWidget(m_nfTextColorBtn);
        hl->addSpacing(8);
        hl->addWidget(new QLabel(QStringLiteral("Fast-attack:"), row));
        hl->addWidget(m_nfFastColorBtn);
        hl->addStretch();
        overlayForm->addRow(QString(), row);
    }

    // DispNormalize — normalize spectrum trace to 1 Hz bandwidth.
    // From Thetis specHPSDR.cs:325 [v2.10.3.13] NormOneHzPan;
    // wired from setup.cs:18093-18099 chkDispNormalize_CheckedChanged.
    // Thetis original: "Normalize to 1 Hz"
    m_dispNormalizeToggle = new QCheckBox(
        QStringLiteral("Normalize trace"), overlayGroup);
    m_dispNormalizeToggle->setToolTip(QStringLiteral(
        "Normalize the spectrum trace to a 1 Hz reference bandwidth. "
        "Only active for Average, Sample, or RMS detector modes."));
    connect(m_dispNormalizeToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setDispNormalize(on);
        }
        AppSettings::instance().setValue(
            QStringLiteral("DisplayDispNormalize"),
            on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    overlayForm->addRow(QString(), m_dispNormalizeToggle);

    // ShowPeakValueOverlay + position + delay.
    // From Thetis console.cs:20073-20080 [v2.10.3.13] PeakTextDelay default=500ms.
    // Color default DodgerBlue from console.cs:20278 [v2.10.3.13].
    m_showPeakValueOverlayToggle = new QCheckBox(
        QStringLiteral("Show peak value overlay"), overlayGroup);
    m_showPeakValueOverlayToggle->setToolTip(QStringLiteral(
        "Display the peak signal level and frequency as a text overlay in the spectrum corner."));
    m_peakValuePositionCombo = makeOverlayPositionCombo(overlayGroup);
    m_peakValuePositionCombo->setCurrentIndex(1);  // Top Right default
    m_peakValuePositionCombo->setToolTip(QStringLiteral("Corner position for the peak value readout."));
    m_peakTextDelaySpin = new QSpinBox(overlayGroup);
    m_peakTextDelaySpin->setRange(50, 10000);
    m_peakTextDelaySpin->setSingleStep(50);
    m_peakTextDelaySpin->setSuffix(QStringLiteral(" ms"));
    // From Thetis console.cs:20073 [v2.10.3.13]: peak_text_delay = 500.
    m_peakTextDelaySpin->setValue(500);
    m_peakTextDelaySpin->setToolTip(QStringLiteral(
        "Refresh interval for the peak value overlay in milliseconds."));
    connect(m_showPeakValueOverlayToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowPeakValueOverlay(on);
        }
        AppSettings::instance().setValue(
            QStringLiteral("DisplayShowPeakValueOverlay"),
            on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    connect(m_peakValuePositionCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setPeakValuePosition(
                static_cast<SpectrumWidget::OverlayPosition>(i));
        }
        AppSettings::instance().setValue(
            QStringLiteral("DisplayPeakValuePosition"),
            QString::number(i));
    });
    connect(m_peakTextDelaySpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int ms) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setPeakTextDelayMs(ms);
        }
        AppSettings::instance().setValue(
            QStringLiteral("DisplayPeakTextDelayMs"),
            QString::number(ms));
    });
    {
        auto* row = new QWidget(overlayGroup);
        auto* hl  = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(m_showPeakValueOverlayToggle);
        hl->addSpacing(8);
        hl->addWidget(m_peakValuePositionCombo);
        hl->addSpacing(8);
        hl->addWidget(m_peakTextDelaySpin);
        hl->addStretch();
        overlayForm->addRow(QString(), row);
    }

    // GetMonitorHz button — query the screen refresh rate and snap the FPS slider.
    // From Thetis setup.designer.cs:2038 [v2.10.3.13] btnGetMonitorHz;
    // wired from setup.cs:32208 btnGetMonitorHz_Click → udDisplayFPS.Value = refreshRate.
    auto* monitorHzBtn = new QPushButton(
        QStringLiteral("Get Monitor Hz"), overlayGroup);
    monitorHzBtn->setToolTip(QStringLiteral(
        "Query the primary screen refresh rate and snap the FPS slider to the nearest valid value."));
    connect(monitorHzBtn, &QPushButton::clicked, this, [this]() {
        // From Thetis setup.cs:32208 btnGetMonitorHz_Click [v2.10.3.13]:
        //   udDisplayFPS.Value = (decimal)Display.GetCurrentMonitorRefreshRate(console);
        // Qt equivalent: QScreen::refreshRate() on the primary screen.
        if (auto* screen = QGuiApplication::primaryScreen()) {
            const int hz = qBound(10, static_cast<int>(std::round(screen->refreshRate())), 60);
            if (m_fpSlider) {
                m_fpSlider->setValue(hz);
                // pushFps is connected to m_fpSlider::valueChanged so it fires automatically.
            }
        }
    });
    overlayForm->addRow(QString(), monitorHzBtn);

    contentLayout()->addWidget(overlayGroup);

    // --- Section: Thread ---
    auto* threadGroup = new QGroupBox(QStringLiteral("Thread"), this);
    auto* threadForm  = new QFormLayout(threadGroup);
    threadForm->setSpacing(6);

    m_threadPriorityCombo = new QComboBox(threadGroup);
    m_threadPriorityCombo->addItems({
        QStringLiteral("Lowest"), QStringLiteral("Below Normal"),
        QStringLiteral("Normal"), QStringLiteral("Above Normal"),
        QStringLiteral("Highest")
    });
    m_threadPriorityCombo->setCurrentIndex(3);  // Above Normal (Thetis default)
    // Thetis: setup.designer.cs:33165 (comboDisplayThreadPriority)
    m_threadPriorityCombo->setToolTip(QStringLiteral("Set the priority of the display thread"));
    connect(m_threadPriorityCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        // Map Thetis 5-item ThreadPriority → QThread::Priority per §13 Q3.
        const QThread::Priority pri =
            (i == 0) ? QThread::LowestPriority :
            (i == 1) ? QThread::LowPriority    :
            (i == 2) ? QThread::NormalPriority :
            (i == 3) ? QThread::HighPriority   :
                       QThread::HighestPriority;
        if (model() && model()->fftEngine()) {
            if (auto* t = model()->fftEngine()->thread()) {
                t->setPriority(pri);
            }
        }
    });
    threadForm->addRow(QStringLiteral("Display Thread Priority:"), m_threadPriorityCombo);

    contentLayout()->addWidget(threadGroup);

    // Colour pickers for trace, grid, passband, and zero lines have moved to
    // Setup → Appearance → Colors & Theme (consolidated in one place).
    auto* colorHint = new QLabel(QStringLiteral(
        "Spectrum / waterfall colours: Setup → Appearance → Colors & Theme."), this);
    colorHint->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; padding: 6px; }"));
    contentLayout()->addWidget(colorHint);

    // ── Cross-links (Task 2.4) ────────────────────────────────────────────────
    // Hint lines for forward-looking moves documented in the design (Task 3.6 / 3.4).
    auto* hintVolts = new QLabel(
        QStringLiteral("ANAN-8000DLE volts/amps moved to Hardware → ANAN-8000DLE."), this);
    hintVolts->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; font-size: 10px; }"));
    contentLayout()->addWidget(hintVolts);

    auto* hintFilter = new QLabel(
        QStringLiteral("Small filter on VFOs moved to Appearance → VFO Flag."), this);
    hintFilter->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; font-size: 10px; }"));
    contentLayout()->addWidget(hintFilter);

    const QString crossLinkStyle = QStringLiteral(
        "QPushButton { background: #1a2a3a; color: #8aa8c0; border: 1px solid #203040;"
        "  border-radius: 3px; padding: 4px 10px; }"
        "QPushButton:hover { background: #203040; color: #c8d8e8; }");

    auto* crossLinkRow = new QWidget(this);
    auto* crossLinkLayout = new QHBoxLayout(crossLinkRow);
    crossLinkLayout->setContentsMargins(0, 4, 0, 0);
    crossLinkLayout->setSpacing(8);

    m_configurePeaksBtn = new QPushButton(
        QStringLiteral("Configure peaks →"), crossLinkRow);
    m_configurePeaksBtn->setToolTip(QStringLiteral(
        "Open Display → Spectrum Peaks to configure Active Peak Hold and Peak Blobs."));
    m_configurePeaksBtn->setStyleSheet(crossLinkStyle);
    connect(m_configurePeaksBtn, &QPushButton::clicked,
            this, &SpectrumDefaultsPage::navigateToSpectrumPeaksRequested);
    crossLinkLayout->addWidget(m_configurePeaksBtn);

    m_configureMultimeterBtn = new QPushButton(
        QStringLiteral("Configure multimeter →"), crossLinkRow);
    m_configureMultimeterBtn->setToolTip(QStringLiteral(
        "Open Display → Multimeter to configure the on-screen level meter. "
        "Available after Task 3.1."));
    m_configureMultimeterBtn->setStyleSheet(crossLinkStyle);
    // Multimeter page lands in Task 3.1; signal is wired in SetupDialog at that time.
    // The button is defined here so SetupDialog can connect it without touching this file again.
    connect(m_configureMultimeterBtn, &QPushButton::clicked,
            this, &SpectrumDefaultsPage::navigateToMultimeterRequested);
    crossLinkLayout->addWidget(m_configureMultimeterBtn);

    crossLinkLayout->addStretch();
    contentLayout()->addWidget(crossLinkRow);

    contentLayout()->addStretch();

    Q_UNUSED(sw);
    Q_UNUSED(fe);
}

// ---------------------------------------------------------------------------
// WaterfallDefaultsPage
// ---------------------------------------------------------------------------

WaterfallDefaultsPage::WaterfallDefaultsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Waterfall Defaults"), model, parent)
{
    buildUI();
    loadFromRenderer();
}

void WaterfallDefaultsPage::loadFromRenderer()
{
    auto* sw = model() ? model()->spectrumWidget() : nullptr;
    if (!sw) { return; }
    QSignalBlocker b1(m_highThresholdSlider);
    QSignalBlocker b2(m_lowThresholdSlider);
    QSignalBlocker b3(m_agcToggle);
    QSignalBlocker b4(m_useSpectrumMinMaxToggle);
    QSignalBlocker b5(m_updatePeriodSlider);
    QSignalBlocker b7(m_opacitySlider);
    QSignalBlocker b8(m_colorSchemeCombo);
    // m_wfAveragingCombo legacy combo removed — no signal blocker needed.
    QSignalBlocker b10(m_showRxFilterToggle);
    QSignalBlocker b11(m_showTxFilterToggle);
    QSignalBlocker b12(m_showRxZeroLineToggle);
    QSignalBlocker b13(m_showTxZeroLineToggle);
    QSignalBlocker b14(m_timestampPosCombo);
    QSignalBlocker b15(m_timestampModeCombo);

    m_highThresholdSlider->setValue(static_cast<int>(sw->wfHighThreshold()));
    m_lowThresholdSlider->setValue(static_cast<int>(sw->wfLowThreshold()));
    m_agcToggle->setChecked(sw->wfAgcEnabled());
    m_useSpectrumMinMaxToggle->setChecked(sw->wfUseSpectrumMinMax());
    m_updatePeriodSlider->setValue(sw->wfUpdatePeriodMs());
    m_opacitySlider->setValue(sw->wfOpacity());

    // Issue #230 fix: gate the low/high threshold + AGC controls when
    // "Use spectrum min/max" is on — the spectrum grid is the
    // authority for those values in that mode.  Mirrors Thetis
    // chkWaterfallUseRX1SpectrumMinMax_CheckedChanged at
    // setup.cs:19224-19232 [v2.10.3.13].
    {
        const bool useMinMax = sw->wfUseSpectrumMinMax();
        if (m_highThresholdSlider) { m_highThresholdSlider->setEnabled(!useMinMax); }
        if (m_lowThresholdSlider)  { m_lowThresholdSlider->setEnabled(!useMinMax);  }
        if (m_agcToggle)           { m_agcToggle->setEnabled(!useMinMax);           }
        if (m_wfNfAgcEnable)       { m_wfNfAgcEnable->setEnabled(!useMinMax);       }
        if (m_wfAgcOffsetDb)       { m_wfAgcOffsetDb->setEnabled(!useMinMax);       }
    }

    // Task 2.8: NF-AGC + Stop-on-TX
    if (m_wfNfAgcEnable) {
        QSignalBlocker bn(m_wfNfAgcEnable);
        m_wfNfAgcEnable->setChecked(sw->waterfallNFAGCEnabled());
    }
    if (m_wfAgcOffsetDb) {
        QSignalBlocker bo(m_wfAgcOffsetDb);
        m_wfAgcOffsetDb->setValue(sw->waterfallAGCOffsetDb());
    }
    if (m_wfStopOnTx) {
        QSignalBlocker bs(m_wfStopOnTx);
        m_wfStopOnTx->setChecked(sw->waterfallStopOnTx());
    }
    m_colorSchemeCombo->setCurrentIndex(static_cast<int>(sw->wfColorScheme()));
    // m_wfAveragingCombo legacy combo removed — sync via m_waterfallAveragingCombo below.
    // Task 2.1: sync new waterfall split combos.
    if (m_waterfallDetectorCombo) {
        QSignalBlocker bwd(m_waterfallDetectorCombo);
        m_waterfallDetectorCombo->setCurrentIndex(static_cast<int>(sw->waterfallDetector()));
    }
    if (m_waterfallAveragingCombo) {
        QSignalBlocker bwa(m_waterfallAveragingCombo);
        m_waterfallAveragingCombo->setCurrentIndex(static_cast<int>(sw->waterfallAveraging()));
    }
    if (m_waterfallAvgTimeSpin) {
        QSignalBlocker bwt(m_waterfallAvgTimeSpin);
        m_waterfallAvgTimeSpin->setValue(sw->waterfallAverageTimeMs());
    }
    m_showRxFilterToggle->setChecked(sw->showRxFilterOnWaterfall());
    m_showTxFilterToggle->setChecked(sw->showTxFilterOnRxWaterfall());
    m_showRxZeroLineToggle->setChecked(sw->showRxZeroLineOnWaterfall());
    m_showTxZeroLineToggle->setChecked(sw->showTxZeroLineOnWaterfall());
    m_timestampPosCombo->setCurrentIndex(static_cast<int>(sw->wfTimestampPosition()));
    m_timestampModeCombo->setCurrentIndex(static_cast<int>(sw->wfTimestampMode()));

    // Low Color picker moved to Setup → Appearance → Colors & Theme.

    // Sub-epic E task 11: sync history-depth dropdown to current value.
    if (m_historyDepthCombo) {
        QSignalBlocker bd(m_historyDepthCombo);
        const qint64 ms = sw->waterfallHistoryMs();
        int idx = 3;  // default: 20 minutes
        for (int i = 0; i < m_historyDepthCombo->count(); ++i) {
            if (m_historyDepthCombo->itemData(i).toLongLong() == ms) {
                idx = i;
                break;
            }
        }
        m_historyDepthCombo->setCurrentIndex(idx);
    }
    updateEffectiveDepthLabel();
    updateDelayLabel();
}

// Task 2.8: live "Delay: NN.N s" readout.
// Computes approximate time span visible in the waterfall:
//   delay_seconds = waterfall_pixel_height × update_period_ms / 1000
// The waterfall pixel height is the QImage height when available, otherwise
// we use a typical 512px stand-in so the label is always meaningful.
void WaterfallDefaultsPage::updateDelayLabel()
{
    auto* sw = model() ? model()->spectrumWidget() : nullptr;
    if (!sw || !m_delayLabel) { return; }
    const int periodMs = std::max(1, sw->wfUpdatePeriodMs());
    // Use the current SpectrumWidget pixel height as a proxy for visible rows.
    // When the widget is not yet shown, this may be 0 — fall back to 512.
    const int visibleRows = std::max(1, sw->height());
    const double delaySec = static_cast<double>(visibleRows) *
                            static_cast<double>(periodMs) / 1000.0;
    m_delayLabel->setText(
        QStringLiteral("Delay: %1 s")
            .arg(delaySec, 0, 'f', 1));
}

void WaterfallDefaultsPage::updateEffectiveDepthLabel()
{
    auto* sw = model() ? model()->spectrumWidget() : nullptr;
    if (!sw || !m_effectiveDepthLabel) { return; }
    const qint64 depthMs  = sw->waterfallHistoryMs();
    const int    periodMs = std::max(1, sw->wfUpdatePeriodMs());
    const int    capRows  = SpectrumWidget::kMaxWaterfallHistoryRows;
    const int    rows     = std::min(
        static_cast<int>((depthMs + periodMs - 1) / periodMs), capRows);
    const int    effectiveMs = rows * periodMs;
    const int    minutes = effectiveMs / 60000;
    const int    seconds = (effectiveMs / 1000) % 60;

    m_effectiveDepthLabel->setText(
        QStringLiteral("Effective rewind: %1 ms × %2 rows = %3:%4 (%5)")
            .arg(periodMs)
            .arg(rows)
            .arg(minutes)
            .arg(seconds, 2, 10, QLatin1Char('0'))
            .arg(rows == capRows ? QStringLiteral("cap reached")
                                 : QStringLiteral("uncapped")));
}

void WaterfallDefaultsPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: Levels ---
    auto* levGroup = new QGroupBox(QStringLiteral("Levels"), this);
    auto* levForm  = new QFormLayout(levGroup);
    levForm->setSpacing(6);

    {
        auto row = makeSliderRow(-200, 0, -40, QStringLiteral(" dBm"), levGroup);
        m_highThresholdSlider = row.slider;
        // Thetis: setup.designer.cs:34259 (udDisplayWaterfallHighLevel)
        m_highThresholdSlider->setToolTip(QStringLiteral("Waterfall High Signal - Show High Color above this value (gradient in between)."));
        row.spin->setToolTip(QStringLiteral("Waterfall High Signal - Show High Color above this value (gradient in between)."));
        connect(m_highThresholdSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWfHighThreshold(static_cast<float>(v));
            }
        });
        levForm->addRow(QStringLiteral("High Threshold:"), row.container);
    }

    {
        auto row = makeSliderRow(-200, 0, -130, QStringLiteral(" dBm"), levGroup);
        m_lowThresholdSlider = row.slider;
        // Thetis: setup.designer.cs:34219 (udDisplayWaterfallLowLevel)
        m_lowThresholdSlider->setToolTip(QStringLiteral("Waterfall Low Signal - Show Low Color below this value (gradient in between)."));
        row.spin->setToolTip(QStringLiteral("Waterfall Low Signal - Show Low Color below this value (gradient in between)."));
        connect(m_lowThresholdSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWfLowThreshold(static_cast<float>(v));
            }
        });
        levForm->addRow(QStringLiteral("Low Threshold:"), row.container);
    }

    m_agcToggle = new QCheckBox(QStringLiteral("AGC"), levGroup);
    // Thetis: setup.designer.cs:34069 (chkRX1WaterfallAGC)
    m_agcToggle->setToolTip(QStringLiteral("Automatically calculates Low Level Threshold for Waterfall."));
    connect(m_agcToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWfAgcEnabled(on);
        }
    });
    levForm->addRow(QString(), m_agcToggle);

    m_useSpectrumMinMaxToggle = new QCheckBox(QStringLiteral("Use spectrum min/max"), levGroup);
    // Thetis: setup.designer.cs:34054 (chkWaterfallUseRX1SpectrumMinMax)
    m_useSpectrumMinMaxToggle->setToolTip(QStringLiteral("Spectrum Grid min/max used for low and high level"));
    connect(m_useSpectrumMinMaxToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWfUseSpectrumMinMax(on);
        }
        // Issue #230 fix: mirror Thetis's
        // chkWaterfallUseRX1SpectrumMinMax_CheckedChanged at
        // setup.cs:19224-19232 [v2.10.3.13] — disable the conflicting
        // threshold and AGC controls so the user can't fight the
        // grid-driven values.
        if (m_highThresholdSlider) { m_highThresholdSlider->setEnabled(!on); }
        if (m_lowThresholdSlider)  { m_lowThresholdSlider->setEnabled(!on);  }
        if (m_agcToggle)           { m_agcToggle->setEnabled(!on);           }
        if (m_wfNfAgcEnable)       { m_wfNfAgcEnable->setEnabled(!on);       }
        if (m_wfAgcOffsetDb)       { m_wfAgcOffsetDb->setEnabled(!on);       }
    });
    levForm->addRow(QString(), m_useSpectrumMinMaxToggle);

    // Low Color (W10) moved to Setup → Appearance → Colors & Theme.

    // Task 2.8: Copy spectrum min/max → waterfall thresholds button.
    m_copySpecMinMaxBtn = new QPushButton(
        QStringLiteral("Copy spectrum min/max → waterfall thresholds"), levGroup);
    m_copySpecMinMaxBtn->setToolTip(
        QStringLiteral("Copies the current spectrum display dB max and dB min values "
                       "into the waterfall High Threshold and Low Threshold above."));
    connect(m_copySpecMinMaxBtn, &QPushButton::clicked, this, [this]() {
        auto* sw = model() ? model()->spectrumWidget() : nullptr;
        if (!sw) { return; }
        // Spectrum top = refLevel(); spectrum bottom = refLevel() - dynamicRange().
        const float high = sw->refLevel();
        const float low  = sw->refLevel() - sw->dynamicRange();
        // Apply to widget (triggers scheduleSettingsSave internally).
        sw->setWfHighThreshold(high);
        sw->setWfLowThreshold(low);
        // Sync spinboxes in this page to the new values.
        if (m_highThresholdSlider) {
            QSignalBlocker bh(m_highThresholdSlider);
            m_highThresholdSlider->setValue(static_cast<int>(high));
        }
        if (m_lowThresholdSlider) {
            QSignalBlocker bl(m_lowThresholdSlider);
            m_lowThresholdSlider->setValue(static_cast<int>(low));
        }
    });
    levForm->addRow(QString(), m_copySpecMinMaxBtn);

    contentLayout()->addWidget(levGroup);

    // --- Section: Waterfall NF-AGC (Task 2.8) ---
    auto* nfAgcGroup = new QGroupBox(QStringLiteral("Waterfall NF-AGC"), this);
    auto* nfAgcForm  = new QFormLayout(nfAgcGroup);
    nfAgcForm->setSpacing(6);

    m_wfNfAgcEnable = new QCheckBox(QStringLiteral("Enable NF-AGC"), nfAgcGroup);
    m_wfNfAgcEnable->setToolTip(
        QStringLiteral("When enabled, the waterfall low/high thresholds automatically "
                       "track the estimated noise floor. The offset below sets how far "
                       "below the noise floor the low threshold is placed."));
    connect(m_wfNfAgcEnable, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWaterfallNFAGCEnabled(on);
        }
    });
    nfAgcForm->addRow(QString(), m_wfNfAgcEnable);

    m_wfAgcOffsetDb = new QSpinBox(nfAgcGroup);
    m_wfAgcOffsetDb->setRange(-60, 60);
    m_wfAgcOffsetDb->setSuffix(QStringLiteral(" dB"));
    m_wfAgcOffsetDb->setValue(0);
    m_wfAgcOffsetDb->setToolTip(
        QStringLiteral("Offset applied above the estimated noise floor when computing "
                       "the waterfall low threshold. Negative values place the low "
                       "threshold below the noise floor (recommended)."));
    connect(m_wfAgcOffsetDb, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWaterfallAGCOffsetDb(v);
        }
    });
    nfAgcForm->addRow(QStringLiteral("NF offset:"), m_wfAgcOffsetDb);

    contentLayout()->addWidget(nfAgcGroup);

    // --- Section: Display ---
    auto* dispGroup = new QGroupBox(QStringLiteral("Display"), this);
    auto* dispForm  = new QFormLayout(dispGroup);
    dispForm->setSpacing(6);

    {
        auto row = makeSliderRow(10, 500, 50, QStringLiteral(" ms"), dispGroup);
        m_updatePeriodSlider = row.slider;
        // Thetis: setup.designer.cs:34145 (udDisplayWaterfallUpdatePeriod)
        m_updatePeriodSlider->setToolTip(QStringLiteral("How often to update (scroll another pixel line) on the waterfall display.  Note that this is tamed by the FPS setting."));
        row.spin->setToolTip(QStringLiteral("How often to update (scroll another pixel line) on the waterfall display.  Note that this is tamed by the FPS setting."));
        connect(m_updatePeriodSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWfUpdatePeriodMs(v);
            }
            updateEffectiveDepthLabel();
            updateDelayLabel();
        });
        dispForm->addRow(QStringLiteral("Update Period:"), row.container);
    }

    // Task 2.8: Calculated Delay readout — live "Delay: NN.N s".
    // Computed from waterfall height × update period; not persisted.
    m_delayLabel = new QLabel(dispGroup);
    m_delayLabel->setStyleSheet(QStringLiteral("color: #80a0b0;"));
    m_delayLabel->setToolTip(
        QStringLiteral("Approximate time covered by the visible waterfall display "
                       "(rows × update period). Longer periods or larger display heights "
                       "increase the delay span."));
    dispForm->addRow(QString(), m_delayLabel);

    // Task 2.8: Stop-on-TX.
    m_wfStopOnTx = new QCheckBox(QStringLiteral("Stop on TX"), dispGroup);
    m_wfStopOnTx->setToolTip(
        QStringLiteral("Pause the waterfall while transmitting. "
                       "Resumes automatically when TX ends."));
    connect(m_wfStopOnTx, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWaterfallStopOnTx(on);
        }
    });
    dispForm->addRow(QString(), m_wfStopOnTx);

    {
        auto row = makeSliderRow(0, 100, 100, QStringLiteral("%"), dispGroup);
        m_opacitySlider = row.slider;
        // Thetis: setup.designer.cs:2056 (tbRX1WaterfallOpacity) — rewritten
        // Thetis original: (none)
        m_opacitySlider->setToolTip(QStringLiteral("Waterfall opacity (0 = fully transparent, 100 = fully opaque). Blends the waterfall over the spectrum background."));
        row.spin->setToolTip(QStringLiteral("Waterfall opacity (0 = fully transparent, 100 = fully opaque). Blends the waterfall over the spectrum background."));
        connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWfOpacity(v);
            }
        });
        dispForm->addRow(QStringLiteral("Opacity:"), row.container);
    }

    m_colorSchemeCombo = new QComboBox(dispGroup);
    m_colorSchemeCombo->addItems({
        QStringLiteral("Default"),   QStringLiteral("Enhanced"),
        QStringLiteral("Spectran"),  QStringLiteral("BlackWhite"),
        QStringLiteral("LinLog"),    QStringLiteral("LinRad"),
        QStringLiteral("Custom"),
        QStringLiteral("Clarity Blue")   // Phase 3G-9b
    });
    // Thetis: setup.designer.cs:34110 (comboColorPalette) — rewritten
    // Thetis original: "Sets the color scheme"
    m_colorSchemeCombo->setToolTip(QStringLiteral("Waterfall color palette. Each scheme maps signal level to a different color gradient from low (dark) to high (bright)."));
    connect(m_colorSchemeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWfColorScheme(static_cast<WfColorScheme>(
                qBound(0, i, static_cast<int>(WfColorScheme::Count) - 1)));
        }
    });
    dispForm->addRow(QStringLiteral("Color Scheme:"), m_colorSchemeCombo);

    // Legacy "WF Averaging" combo (None / Weighted / Logarithmic / Time Window)
    // was a NereusSDR-only UI duplicate. Its setWfAverageMode() calls write
    // only to m_wfAverageMode which the renderer consults only when the new
    // m_waterfallAveraging is None — i.e. it's a fallback path with no
    // independent UI value. Combo removed; the legacy enum + setter survive
    // for any external callers and migrate via SettingsSchemaVersion=4.

    // Task 2.1: Detector + Averaging split for waterfall (handwave fix from 3G-8).
    // Ported from Thetis comboDispWFDetector [v2.10.3.13]
    // (setup.designer.cs:34461): Peak / Rosenfell / Average / Sample.
    // Note: WF detector has 4 items (no RMS); Pan detector has 5 (with RMS).
    // RX1 scope dropped — pan-agnostic per design Section 1B.
    m_waterfallDetectorCombo = new QComboBox(dispGroup);
    m_waterfallDetectorCombo->addItems({
        QStringLiteral("Peak"),
        QStringLiteral("Rosenfell"),
        QStringLiteral("Average"),
        QStringLiteral("Sample")
    });
    // Thetis: setup.designer.cs:34461 (comboDispWFDetector) [v2.10.3.13] — no upstream tooltip; rewritten
    m_waterfallDetectorCombo->setToolTip(QStringLiteral(
        "Waterfall bin-reduction policy. Peak takes the maximum bin per pixel. "
        "Rosenfell alternates max/min. Average takes the arithmetic mean. "
        "Sample takes the first bin."));
    connect(m_waterfallDetectorCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWaterfallDetector(static_cast<SpectrumDetector>(i));
        }
    });
    dispForm->addRow(QStringLiteral("WF Detector:"), m_waterfallDetectorCombo);

    // Ported from Thetis comboDispWFAveraging [v2.10.3.13]
    // (setup.designer.cs:34436): None / Recursive / Time Window / Log Recursive.
    m_waterfallAveragingCombo = new QComboBox(dispGroup);
    m_waterfallAveragingCombo->addItems({
        QStringLiteral("None"),
        QStringLiteral("Recursive"),
        QStringLiteral("Time Window"),
        QStringLiteral("Log Recursive")
    });
    // Thetis: setup.designer.cs:34436 (comboDispWFAveraging) [v2.10.3.13] — no upstream tooltip; rewritten
    m_waterfallAveragingCombo->setToolTip(QStringLiteral(
        "Waterfall frame-averaging mode. None shows raw FFT output per row. "
        "Recursive exponentially smooths in linear power space. "
        "Time Window approximates a sliding average. "
        "Log Recursive smooths in the dB domain for better low-signal visibility."));
    connect(m_waterfallAveragingCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWaterfallAveraging(static_cast<SpectrumAveraging>(i));
        }
    });
    dispForm->addRow(QStringLiteral("WF Averaging:"), m_waterfallAveragingCombo);

    // Waterfall averaging time constant — independent from spectrum.
    // From Thetis udDisplayAVTimeWF [v2.10.3.13] (setup.designer.cs:2086).
    // Default 120 ms matches Thetis. Range 10..9999 ms.
    m_waterfallAvgTimeSpin = new QSpinBox(dispGroup);
    m_waterfallAvgTimeSpin->setRange(10, 9999);
    m_waterfallAvgTimeSpin->setSingleStep(10);
    m_waterfallAvgTimeSpin->setSuffix(QStringLiteral(" ms"));
    m_waterfallAvgTimeSpin->setValue(120);
    m_waterfallAvgTimeSpin->setToolTip(QStringLiteral(
        "Waterfall averaging time constant. Independent from the spectrum "
        "averaging time. Larger = heavier smoothing. Translates to a "
        "frame-by-frame alpha via α = exp(−1 / (fps × τ)) per Thetis."));
    connect(m_waterfallAvgTimeSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int ms) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWaterfallAverageTimeMs(ms);
        }
    });
    dispForm->addRow(QStringLiteral("WF Avg Time:"), m_waterfallAvgTimeSpin);

    contentLayout()->addWidget(dispGroup);

    // --- Section: Overlays ---
    auto* ovGroup = new QGroupBox(QStringLiteral("Overlays"), this);
    auto* ovForm  = new QFormLayout(ovGroup);
    ovForm->setSpacing(6);

    m_showRxFilterToggle = new QCheckBox(QStringLiteral("Show RX filter on waterfall"), ovGroup);
    // Thetis: setup.designer.cs:3189 (chkShowRXFilterOnWaterfall) — rewritten
    // Thetis original: (none)
    m_showRxFilterToggle->setToolTip(QStringLiteral("Overlay the current RX passband filter boundaries on the waterfall display."));
    connect(m_showRxFilterToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowRxFilterOnWaterfall(on);
        }
    });
    ovForm->addRow(QString(), m_showRxFilterToggle);

    m_showTxFilterToggle = new QCheckBox(QStringLiteral("Show TX filter on RX waterfall"), ovGroup);
    // Thetis: setup.designer.cs:3187 (chkShowTXFilterOnRXWaterfall) — rewritten
    // Thetis original: (none)
    m_showTxFilterToggle->setToolTip(QStringLiteral("Overlay the TX passband filter boundaries on the RX waterfall display."));
    connect(m_showTxFilterToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowTxFilterOnRxWaterfall(on);
        }
    });
    ovForm->addRow(QString(), m_showTxFilterToggle);

    m_showRxZeroLineToggle = new QCheckBox(QStringLiteral("Show RX zero line on waterfall"), ovGroup);
    // Thetis: setup.designer.cs:3188 (chkShowRXZeroLineOnWaterfall) — rewritten
    // Thetis original: (none)
    m_showRxZeroLineToggle->setToolTip(QStringLiteral("Draw a line on the waterfall at the RX centre frequency (zero-beat reference)."));
    connect(m_showRxZeroLineToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowRxZeroLineOnWaterfall(on);
        }
    });
    ovForm->addRow(QString(), m_showRxZeroLineToggle);

    m_showTxZeroLineToggle = new QCheckBox(QStringLiteral("Show TX zero line on waterfall"), ovGroup);
    // Thetis: setup.designer.cs:3242 (chkShowTXZeroLineOnWaterfall) — rewritten
    // Thetis original: (none)
    m_showTxZeroLineToggle->setToolTip(QStringLiteral("Draw a line on the waterfall at the TX centre frequency (zero-beat reference)."));
    connect(m_showTxZeroLineToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowTxZeroLineOnWaterfall(on);
        }
    });
    ovForm->addRow(QString(), m_showTxZeroLineToggle);

    contentLayout()->addWidget(ovGroup);

    // ── Sub-epic E: rewind / history depth ────────────────────────────────
    auto* histGroup = new QGroupBox(QStringLiteral("Rewind history"), this);
    auto* histForm  = new QFormLayout(histGroup);
    histForm->setSpacing(6);

    m_historyDepthCombo = new QComboBox(histGroup);
    m_historyDepthCombo->addItem(QStringLiteral("60 seconds"),      60LL * 1000);
    m_historyDepthCombo->addItem(QStringLiteral("5 minutes"),  5LL * 60 * 1000);
    m_historyDepthCombo->addItem(QStringLiteral("15 minutes"), 15LL * 60 * 1000);
    m_historyDepthCombo->addItem(QStringLiteral("20 minutes"), 20LL * 60 * 1000);
    m_historyDepthCombo->setToolTip(
        QStringLiteral("Maximum amount of waterfall history kept for rewind. "
                       "Effective rewind is capped at %1 rows — slow the "
                       "update period to extend depth at fast refresh rates.")
            .arg(SpectrumWidget::kMaxWaterfallHistoryRows));
    histForm->addRow(QStringLiteral("Depth:"), m_historyDepthCombo);

    m_effectiveDepthLabel = new QLabel(histGroup);
    m_effectiveDepthLabel->setStyleSheet(QStringLiteral("color: #80a0b0;"));
    histForm->addRow(QStringLiteral(""), m_effectiveDepthLabel);

    contentLayout()->addWidget(histGroup);

    connect(m_historyDepthCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        auto* sw = model() ? model()->spectrumWidget() : nullptr;
        if (!sw) { return; }
        const qint64 ms = m_historyDepthCombo->itemData(idx).toLongLong();
        sw->setWaterfallHistoryMs(ms);
        updateEffectiveDepthLabel();
    });

    // --- Section: Time ---
    auto* timeGroup = new QGroupBox(QStringLiteral("Time"), this);
    auto* timeForm  = new QFormLayout(timeGroup);
    timeForm->setSpacing(6);

    m_timestampPosCombo = new QComboBox(timeGroup);
    m_timestampPosCombo->addItems({QStringLiteral("None"), QStringLiteral("Left"),
                                   QStringLiteral("Right")});
    // NereusSDR extension — no Thetis equivalent
    m_timestampPosCombo->setToolTip(QStringLiteral("Position of the time stamp drawn on each waterfall row. None disables timestamps; Left and Right place them at the respective edge."));
    connect(m_timestampPosCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWfTimestampPosition(
                static_cast<SpectrumWidget::TimestampPosition>(
                    qBound(0, i, static_cast<int>(SpectrumWidget::TimestampPosition::Count) - 1)));
        }
    });
    timeForm->addRow(QStringLiteral("Timestamp Position:"), m_timestampPosCombo);

    m_timestampModeCombo = new QComboBox(timeGroup);
    m_timestampModeCombo->addItems({QStringLiteral("UTC"), QStringLiteral("Local")});
    // NereusSDR extension — no Thetis equivalent
    m_timestampModeCombo->setToolTip(QStringLiteral("Time zone used for waterfall timestamps. UTC uses Coordinated Universal Time; Local uses the system clock time zone."));
    connect(m_timestampModeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWfTimestampMode(
                static_cast<SpectrumWidget::TimestampMode>(
                    qBound(0, i, static_cast<int>(SpectrumWidget::TimestampMode::Count) - 1)));
        }
    });
    timeForm->addRow(QStringLiteral("Timestamp Mode:"), m_timestampModeCombo);

    contentLayout()->addWidget(timeGroup);

    // Low Level Color (W10) has moved to Setup → Appearance → Colors & Theme.
    auto* wfColorHint = new QLabel(QStringLiteral(
        "Spectrum / waterfall colours: Setup → Appearance → Colors & Theme."), this);
    wfColorHint->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; padding: 6px; }"));
    contentLayout()->addWidget(wfColorHint);

    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// GridScalesPage
// ---------------------------------------------------------------------------

GridScalesPage::GridScalesPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Grid & Scales"), model, parent)
{
    buildUI();
    loadFromRenderer();
}

// Helper: return the first panadapter or nullptr.
static PanadapterModel* firstPan(RadioModel* m)
{
    if (!m) { return nullptr; }
    const auto pans = m->panadapters();
    return pans.isEmpty() ? nullptr : pans.first();
}

void GridScalesPage::applyBandSlot(PanadapterModel* pan)
{
    if (!pan || !m_dbMaxSpin || !m_dbMinSpin || !m_editingBandLabel) { return; }
    const Band b = pan->band();
    const BandGridSettings slot = pan->perBandGrid(b);
    const QString bandName = bandLabel(b);
    QSignalBlocker bMax(m_dbMaxSpin);
    QSignalBlocker bMin(m_dbMinSpin);
    m_dbMaxSpin->setValue(slot.dbMax);
    m_dbMinSpin->setValue(slot.dbMin);
    // Prominent header — large + bracketed so the user can spot which band
    // they're editing at a glance without having to scan row labels.
    m_editingBandLabel->setText(
        QStringLiteral("◆ Editing per-band grid: %1 ◆").arg(bandName));
    // Row-label fallback: include the band name next to each spinbox so the
    // association is unambiguous from any focal point on the page.
    if (m_dbMaxRowLabel) {
        m_dbMaxRowLabel->setText(QStringLiteral("dB Max (%1):").arg(bandName));
    }
    if (m_dbMinRowLabel) {
        m_dbMinRowLabel->setText(QStringLiteral("dB Min (%1):").arg(bandName));
    }
}

void GridScalesPage::loadFromRenderer()
{
    auto* sw  = model() ? model()->spectrumWidget() : nullptr;
    auto* pan = firstPan(model());
    if (!sw) { return; }  // pan-dependent loads gated below

    QSignalBlocker b1(m_gridToggle);
    QSignalBlocker b3(m_freqLabelAlignCombo);
    QSignalBlocker b4(m_zeroLineToggle);
    QSignalBlocker b5(m_showFpsToggle);
    QSignalBlocker b6(m_dbmScaleVisibleToggle);

    m_gridToggle->setChecked(sw->gridEnabled());
    m_dbmScaleVisibleToggle->setChecked(sw->dbmScaleVisible());
    m_freqLabelAlignCombo->setCurrentIndex(static_cast<int>(sw->freqLabelAlign()));
    m_zeroLineToggle->setChecked(sw->showZeroLine());
    m_showFpsToggle->setChecked(sw->showFps());

    // Colour pickers (G6/G9–G13) moved to Setup → Appearance → Colors & Theme.

    // Task 2.9: NF tracking controls — SpectrumWidget-only state, must
    // load even when pan is null (otherwise the bug from 2026-05-08:
    // toggle showed unchecked on dialog reopen even though disk had True
    // because the early-return on !pan skipped the entire NF block).
    const bool nfTrackingOn = sw->adjustGridMinToNoiseFloor();
    if (m_adjustGridMinToNF) {
        QSignalBlocker bNf(m_adjustGridMinToNF);
        m_adjustGridMinToNF->setChecked(nfTrackingOn);
    }
    if (m_nfOffsetGridFollow) {
        QSignalBlocker bOff(m_nfOffsetGridFollow);
        m_nfOffsetGridFollow->setValue(sw->nfOffsetGridFollow());
        // Mirror the toggle->dependent enabled-state gate from the
        // m_adjustGridMinToNF::toggled handler.  The toggle's setChecked
        // call above runs inside a QSignalBlocker so the handler doesn't
        // fire — without this re-sync the dependents stay disabled even
        // when the toggle restores to ON.
        m_nfOffsetGridFollow->setEnabled(nfTrackingOn);
    }
    if (m_maintainNFAdjustDelta) {
        QSignalBlocker bMaint(m_maintainNFAdjustDelta);
        m_maintainNFAdjustDelta->setChecked(sw->maintainNFAdjustDelta());
        m_maintainNFAdjustDelta->setEnabled(nfTrackingOn);
    }

    // Per-band loads — gated on pan availability.
    if (!pan) { return; }
    QSignalBlocker b2(m_dbStepSpin);
    m_dbStepSpin->setValue(pan->gridStep());
    applyBandSlot(pan);
}

void GridScalesPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: Grid ---
    auto* gridGroup = new QGroupBox(QStringLiteral("Grid"), this);
    auto* gridForm  = new QFormLayout(gridGroup);
    gridForm->setSpacing(6);

    m_gridToggle = new QCheckBox(QStringLiteral("Show grid"), gridGroup);
    m_gridToggle->setChecked(true);
    // Thetis: setup.designer.cs:52824 (chkGridControl)
    m_gridToggle->setToolTip(QStringLiteral("Display the Major Grid on the Panadapter including the frequency numbers"));
    connect(m_gridToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setGridEnabled(on);
        }
    });
    gridForm->addRow(QString(), m_gridToggle);

    m_dbmScaleVisibleToggle = new QCheckBox(QStringLiteral("Show dBm scale strip (right edge)"), gridGroup);
    m_dbmScaleVisibleToggle->setChecked(true);
    m_dbmScaleVisibleToggle->setToolTip(QStringLiteral("Show the reference-level scale on the right edge of the spectrum. "
                                                        "Disable to give the spectrum trace the full widget width."));
    connect(m_dbmScaleVisibleToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setDbmScaleVisible(on);
        }
    });
    gridForm->addRow(QString(), m_dbmScaleVisibleToggle);

    // Section divider above the per-band controls so the section break is
    // visually obvious — applyBandSlot updates the label text live as the
    // user tunes across a band boundary.
    auto* bandSeparator = new QFrame(gridGroup);
    bandSeparator->setFrameShape(QFrame::HLine);
    bandSeparator->setFrameShadow(QFrame::Sunken);
    bandSeparator->setStyleSheet(QStringLiteral(
        "QFrame { color: #2a3a4a; background: #2a3a4a; max-height: 1px; }"));
    gridForm->addRow(bandSeparator);

    m_editingBandLabel = new QLabel(QStringLiteral("◆ Editing per-band grid: — ◆"), gridGroup);
    m_editingBandLabel->setAlignment(Qt::AlignCenter);
    // Larger + brighter than before — section-header style instead of an
    // inline note.  Cyan picks up the existing accent palette.
    m_editingBandLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #00d4f0; font-size: 13px; font-weight: bold;"
        " padding: 4px 0; }"));
    gridForm->addRow(m_editingBandLabel);

    m_dbMaxSpin = new QSpinBox(gridGroup);
    m_dbMaxSpin->setRange(-200, 0);
    m_dbMaxSpin->setValue(-40);
    m_dbMaxSpin->setSuffix(QStringLiteral(" dB"));
    // Thetis: setup.designer.cs:34745 (udDisplayGridMax) — rewritten
    // Thetis original: "Signal level at top of display in dB."
    m_dbMaxSpin->setToolTip(QStringLiteral("Signal level at the top of the display in dB. Edits the current band's grid slot — band shown in the row label and the section header above."));
    connect(m_dbMaxSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (auto* pan = firstPan(model())) {
            pan->setPerBandDbMax(pan->band(), v);
        }
    });
    m_dbMaxRowLabel = new QLabel(QStringLiteral("dB Max (per band):"), gridGroup);
    gridForm->addRow(m_dbMaxRowLabel, m_dbMaxSpin);

    m_dbMinSpin = new QSpinBox(gridGroup);
    m_dbMinSpin->setRange(-200, 0);
    m_dbMinSpin->setValue(-140);
    m_dbMinSpin->setSuffix(QStringLiteral(" dB"));
    // Thetis: setup.designer.cs:34714 (udDisplayGridMin) — rewritten
    // Thetis original: "Signal Level at bottom of display in dB."
    m_dbMinSpin->setToolTip(QStringLiteral("Signal level at the bottom of the display in dB. Edits the current band's grid slot — band shown in the row label and the section header above."));
    connect(m_dbMinSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (auto* pan = firstPan(model())) {
            pan->setPerBandDbMin(pan->band(), v);
        }
    });
    m_dbMinRowLabel = new QLabel(QStringLiteral("dB Min (per band):"), gridGroup);
    gridForm->addRow(m_dbMinRowLabel, m_dbMinSpin);

    m_dbStepSpin = new QSpinBox(gridGroup);
    m_dbStepSpin->setRange(1, 40);
    m_dbStepSpin->setValue(10);
    m_dbStepSpin->setSuffix(QStringLiteral(" dB"));
    // Thetis: setup.designer.cs:34683 (udDisplayGridStep) — rewritten
    // Thetis original: "Horizontal Grid Step Size in dB."
    m_dbStepSpin->setToolTip(QStringLiteral("Horizontal grid step size in dB. Sets the spacing between dB grid lines across all bands (global, not per-band)."));
    connect(m_dbStepSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (auto* pan = firstPan(model())) {
            pan->setGridStep(v);
        }
    });
    gridForm->addRow(QStringLiteral("dB Step (global):"), m_dbStepSpin);

    contentLayout()->addWidget(gridGroup);

    // Connect to PanadapterModel::bandChanged so the editing-band label
    // and the dbMax/dbMin spinboxes refresh when the user tunes across a
    // band boundary (or clicks a band button).
    if (auto* pan = firstPan(model())) {
        connect(pan, &PanadapterModel::bandChanged,
                this, [this, pan](Band) { applyBandSlot(pan); });
    }

    // --- Section: Labels ---
    auto* lblGroup = new QGroupBox(QStringLiteral("Labels"), this);
    auto* lblForm  = new QFormLayout(lblGroup);
    lblForm->setSpacing(6);

    m_freqLabelAlignCombo = new QComboBox(lblGroup);
    m_freqLabelAlignCombo->addItems({
        QStringLiteral("Left"),   QStringLiteral("Center"),
        QStringLiteral("Right"),  QStringLiteral("Auto"),
        QStringLiteral("Off")
    });
    m_freqLabelAlignCombo->setCurrentIndex(1);
    // Thetis: setup.designer.cs:34649 (comboDisplayLabelAlign) — rewritten
    // Thetis original: "Sets the alignement of the grid callouts on the display."
    m_freqLabelAlignCombo->setToolTip(QStringLiteral("Sets the alignment of the frequency labels on the grid callouts on the display."));
    connect(m_freqLabelAlignCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setFreqLabelAlign(static_cast<FreqLabelAlign>(i));
        }
    });
    lblForm->addRow(QStringLiteral("Freq Label Align:"), m_freqLabelAlignCombo);

    m_zeroLineToggle = new QCheckBox(QStringLiteral("Show zero line"), lblGroup);
    // Thetis: setup.designer.cs:3221 (chkShowZeroLine) — rewritten
    // Thetis original: (none)
    m_zeroLineToggle->setToolTip(QStringLiteral("Show a horizontal line at 0 dBm on the panadapter grid."));
    connect(m_zeroLineToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowZeroLine(on);
        }
    });
    lblForm->addRow(QString(), m_zeroLineToggle);

    m_showFpsToggle = new QCheckBox(QStringLiteral("Show FPS overlay"), lblGroup);
    // Thetis: setup.designer.cs:33177 (chkShowFPS)
    m_showFpsToggle->setToolTip(QStringLiteral("Show FPS reading in top left of spectrum area"));
    connect(m_showFpsToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowFps(on);
        }
    });
    lblForm->addRow(QString(), m_showFpsToggle);

    contentLayout()->addWidget(lblGroup);

    // Grid/zero-line/band-edge colour pickers (G6/G9–G13) moved to
    // Setup → Appearance → Colors & Theme (consolidated colour panel).
    auto* gridColorHint = new QLabel(QStringLiteral(
        "Spectrum / waterfall colours: Setup → Appearance → Colors & Theme."), this);
    gridColorHint->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; padding: 6px; }"));
    contentLayout()->addWidget(gridColorHint);

    // --- Section: Noise-Floor Tracking (Task 2.9) ---
    // From Thetis setup.cs:24202-24213 [v2.10.3.13] chkAdjustGridMinToNFRX1
    // — RX1 scope dropped; NereusSDR applies as global panadapter default
    //   with per-pan override via ContainerSettings dialog (3G-6 pattern).
    auto* nfGroup = new QGroupBox(QStringLiteral("Noise-Floor Tracking"), this);
    auto* nfForm  = new QFormLayout(nfGroup);
    nfForm->setSpacing(6);

    m_adjustGridMinToNF = new QCheckBox(
        QStringLiteral("Adjust grid min to track noise floor"), nfGroup);
    // From Thetis setup.cs:24202 [v2.10.3.13] chkAdjustGridMinToNFRX1
    m_adjustGridMinToNF->setToolTip(
        QStringLiteral("When enabled, the lower grid boundary automatically follows the "
                       "live noise floor estimate. The grid min is set to NF + offset."));
    connect(m_adjustGridMinToNF, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setAdjustGridMinToNoiseFloor(on);  // calls scheduleSettingsSave internally
        }
        if (m_nfOffsetGridFollow) { m_nfOffsetGridFollow->setEnabled(on); }
        if (m_maintainNFAdjustDelta) { m_maintainNFAdjustDelta->setEnabled(on); }
    });
    nfForm->addRow(QString(), m_adjustGridMinToNF);

    m_nfOffsetGridFollow = new QSpinBox(nfGroup);
    m_nfOffsetGridFollow->setRange(-60, 60);
    m_nfOffsetGridFollow->setValue(0);
    m_nfOffsetGridFollow->setSuffix(QStringLiteral(" dB"));
    m_nfOffsetGridFollow->setEnabled(false);
    // From Thetis console.cs:46035-46040 [v2.10.3.13] _RX1NFoffsetGridFollow = 5f.
    // NereusSDR: range -60..+60, default 0. Offset is added to NF estimate.
    m_nfOffsetGridFollow->setToolTip(
        QStringLiteral("Offset added to the noise floor estimate to compute the grid min. "
                       "Use a negative value to place the grid min below the noise floor. "
                       "(Thetis default is -5 dB below NF; equivalent here as offset -5.)"));
    connect(m_nfOffsetGridFollow, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int db) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setNFOffsetGridFollow(db);  // calls scheduleSettingsSave internally
        }
    });
    nfForm->addRow(QStringLiteral("NF offset:"), m_nfOffsetGridFollow);

    m_maintainNFAdjustDelta = new QCheckBox(
        QStringLiteral("Maintain grid range (move max with min)"), nfGroup);
    m_maintainNFAdjustDelta->setEnabled(false);
    // From Thetis console.cs:46085 [v2.10.3.13] _maintainNFAdjustDeltaRX1.
    // Range delta uses std::abs() guard: abs incase //MW0LGE [2.9.0.7] [original inline comment from console.cs:46081]
    m_maintainNFAdjustDelta->setToolTip(
        QStringLiteral("When enabled, the grid max is also moved so the dB range stays "
                       "constant as the grid min tracks the noise floor."));
    connect(m_maintainNFAdjustDelta, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setMaintainNFAdjustDelta(on);  // calls scheduleSettingsSave internally
        }
    });
    nfForm->addRow(QString(), m_maintainNFAdjustDelta);

    contentLayout()->addWidget(nfGroup);

    // --- Task 2.9: Copy waterfall thresholds → spectrum min/max ---
    // Reverse direction of Task 2.8's Copy spectrum min/max → waterfall.
    auto* copyGroup = new QGroupBox(QStringLiteral("Copy"), this);
    auto* copyForm  = new QFormLayout(copyGroup);
    copyForm->setSpacing(6);

    m_copyWfToSpecBtn = new QPushButton(
        QStringLiteral("Copy waterfall thresholds → spectrum min/max"), copyGroup);
    m_copyWfToSpecBtn->setToolTip(
        QStringLiteral("Copies the current waterfall High Threshold and Low Threshold "
                       "into the spectrum dB max and dB min for the current band."));
    connect(m_copyWfToSpecBtn, &QPushButton::clicked, this, [this]() {
        auto* sw = model() ? model()->spectrumWidget() : nullptr;
        if (!sw) { return; }
        // Waterfall high → spectrum top (refLevel); waterfall low → spectrum bottom.
        const float wfHigh = sw->wfHighThreshold();
        const float wfLow  = sw->wfLowThreshold();
        // Apply to spectrum display range. setDbmRange() doesn't call save
        // internally, so explicitly request a coalesced settings save.
        sw->setDbmRange(wfLow, wfHigh);
        sw->requestSettingsSave();
        // Sync per-band grid spinboxes on this page to the new values.
        if (auto* pan = firstPan(model())) {
            pan->setPerBandDbMax(pan->band(), qRound(wfHigh));
            pan->setPerBandDbMin(pan->band(), qRound(wfLow));
            // Refresh spinbox displays without triggering extra pan writes.
            if (m_dbMaxSpin) {
                QSignalBlocker bmax(m_dbMaxSpin);
                m_dbMaxSpin->setValue(qRound(wfHigh));
            }
            if (m_dbMinSpin) {
                QSignalBlocker bmin(m_dbMinSpin);
                m_dbMinSpin->setValue(qRound(wfLow));
            }
        }
    });
    copyForm->addRow(QString(), m_copyWfToSpecBtn);

    contentLayout()->addWidget(copyGroup);

    contentLayout()->addStretch();

    // Sync enabled-state of NF sub-controls on construction after loadFromRenderer().
    // (loadFromRenderer runs before buildUI in the constructor, so the controls
    // don't exist yet when loadFromRenderer fires — re-apply the enabled gate here.)
    // Done via a post-construction deferred call: Qt will execute it after the
    // constructor returns, by which time loadFromRenderer has set m_adjustGridMinToNF.
    QTimer::singleShot(0, this, [this]() {
        bool nfOn = m_adjustGridMinToNF && m_adjustGridMinToNF->isChecked();
        if (m_nfOffsetGridFollow) { m_nfOffsetGridFollow->setEnabled(nfOn); }
        if (m_maintainNFAdjustDelta) { m_maintainNFAdjustDelta->setEnabled(nfOn); }
    });
}

// ---------------------------------------------------------------------------
// Rx2DisplayPage
// ---------------------------------------------------------------------------

Rx2DisplayPage::Rx2DisplayPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("RX2 Display"), model, parent)
{
    buildUI();
}

void Rx2DisplayPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: RX2 Spectrum ---
    auto* specGroup = new QGroupBox(QStringLiteral("RX2 Spectrum"), this);
    auto* specForm  = new QFormLayout(specGroup);
    specForm->setSpacing(6);

    m_dbMaxSpin = new QSpinBox(specGroup);
    m_dbMaxSpin->setRange(-200, 0);
    m_dbMaxSpin->setValue(-20);
    m_dbMaxSpin->setSuffix(QStringLiteral(" dB"));
    m_dbMaxSpin->setEnabled(false);  // NYI
    m_dbMaxSpin->setToolTip(QStringLiteral("RX2 spectrum dB max — not yet implemented"));
    specForm->addRow(QStringLiteral("dB Max:"), m_dbMaxSpin);

    m_dbMinSpin = new QSpinBox(specGroup);
    m_dbMinSpin->setRange(-200, 0);
    m_dbMinSpin->setValue(-160);
    m_dbMinSpin->setSuffix(QStringLiteral(" dB"));
    m_dbMinSpin->setEnabled(false);  // NYI
    m_dbMinSpin->setToolTip(QStringLiteral("RX2 spectrum dB min — not yet implemented"));
    specForm->addRow(QStringLiteral("dB Min:"), m_dbMinSpin);

    m_colorSchemeCombo = new QComboBox(specGroup);
    m_colorSchemeCombo->addItems({QStringLiteral("Enhanced"), QStringLiteral("Grayscale"),
                                  QStringLiteral("Spectrum")});
    m_colorSchemeCombo->setEnabled(false);  // NYI
    m_colorSchemeCombo->setToolTip(QStringLiteral("RX2 waterfall color scheme — not yet implemented"));
    specForm->addRow(QStringLiteral("Color Scheme:"), m_colorSchemeCombo);

    contentLayout()->addWidget(specGroup);

    // --- Section: RX2 Waterfall ---
    auto* wfGroup = new QGroupBox(QStringLiteral("RX2 Waterfall"), this);
    auto* wfForm  = new QFormLayout(wfGroup);
    wfForm->setSpacing(6);

    m_highThresholdSlider = new QSlider(Qt::Horizontal, wfGroup);
    m_highThresholdSlider->setRange(-200, 0);
    m_highThresholdSlider->setValue(-40);
    m_highThresholdSlider->setEnabled(false);  // NYI
    m_highThresholdSlider->setToolTip(QStringLiteral("RX2 waterfall high threshold — not yet implemented"));
    wfForm->addRow(QStringLiteral("High Threshold:"), m_highThresholdSlider);

    m_lowThresholdSlider = new QSlider(Qt::Horizontal, wfGroup);
    m_lowThresholdSlider->setRange(-200, 0);
    m_lowThresholdSlider->setValue(-130);
    m_lowThresholdSlider->setEnabled(false);  // NYI
    m_lowThresholdSlider->setToolTip(QStringLiteral("RX2 waterfall low threshold — not yet implemented"));
    wfForm->addRow(QStringLiteral("Low Threshold:"), m_lowThresholdSlider);

    contentLayout()->addWidget(wfGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// TxDisplayPage
// ---------------------------------------------------------------------------

TxDisplayPage::TxDisplayPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("TX Display"), model, parent)
{
    buildUI();
}

void TxDisplayPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: TX Spectrum ---
    auto* specGroup = new QGroupBox(QStringLiteral("TX Spectrum"), this);
    auto* specForm  = new QFormLayout(specGroup);
    specForm->setSpacing(6);

    m_bgColorLabel = makeColorSwatch(QStringLiteral("Background Color"), QStringLiteral("#0a0a14"), specGroup);
    specForm->addRow(QStringLiteral("Background:"), m_bgColorLabel);

    m_gridColorLabel = makeColorSwatch(QStringLiteral("Grid Color"), QStringLiteral("#203040"), specGroup);
    specForm->addRow(QStringLiteral("Grid Color:"), m_gridColorLabel);

    m_lineWidthSlider = new QSlider(Qt::Horizontal, specGroup);
    m_lineWidthSlider->setRange(1, 3);
    m_lineWidthSlider->setValue(1);
    m_lineWidthSlider->setEnabled(false);  // NYI
    m_lineWidthSlider->setToolTip(QStringLiteral("TX trace line width (1–3 px) — not yet implemented"));
    specForm->addRow(QStringLiteral("Line Width:"), m_lineWidthSlider);

    m_calOffsetSpin = new QDoubleSpinBox(specGroup);
    m_calOffsetSpin->setRange(-30.0, 30.0);
    m_calOffsetSpin->setSingleStep(0.5);
    m_calOffsetSpin->setSuffix(QStringLiteral(" dBm"));
    m_calOffsetSpin->setValue(0.0);
    m_calOffsetSpin->setEnabled(false);  // NYI
    m_calOffsetSpin->setToolTip(QStringLiteral("TX calibration offset — not yet implemented"));
    specForm->addRow(QStringLiteral("Cal Offset:"), m_calOffsetSpin);

    contentLayout()->addWidget(specGroup);

    // TX passband overlay colour picker moved to Setup → Appearance →
    // Colors & Theme alongside the RX passband picker (Plan 4 D9b follow-up).

    contentLayout()->addStretch();
}

} // namespace NereusSDR
