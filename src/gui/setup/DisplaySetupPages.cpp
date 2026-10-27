// =================================================================
// src/gui/setup/DisplaySetupPages.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12 - J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//                 Keep disabled explanations stable during live level refreshes.
//   2026-10-12 - J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//                 Approved eight-page Display assembly, scoped bindings, own-row indicators,
//                 shared workspace styling, mode notes, editor placement, scoped resets,
//                 shared TX endpoints and explicit station/computer ownership.
//   2026-10-05 — J.J. Boyd (KG4VCF). Independent per-pan Clarity ownership.
//                 AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-10 - Display pill plan Task 20 (issue #296): Spectrum Defaults'
//                 FFT Size slider sets the station's baseline once, on
//                 release or after the last keyboard step, and opens on the
//                 baseline; Window, Hz/bin target, FPS, decimation and
//                 thread priority reach every stream; a line under the
//                 slider says what the FFT runs at. J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//   2026-10-11 - FFT sizing mode (design D38, issue #296): Spectrum Defaults
//                 has a "Sizing" choice (Auto, Hz/bin, Fixed); the FFT Size
//                 slider is live in Fixed only and otherwise shows the
//                 running size, the Hz/bin target is live in Hz/bin only
//                 and has no "Off", and the two cells and the line under
//                 them show what runs. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-23 - R-R3-21 / R-R3-10: Spectrum Defaults, Grid & Scales and
//                 TX Display disable only their Core controls while a
//                 remote window does not have the Core's settings.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-24 - The waterfall colour scheme names come from
//                 core/spectrum/WaterfallPalettes (iPhone app Task 19,
//                 R-IOS-06). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-26 - R-R3-01 / R-R3-11 / R-R3-41 (parity Task 17): in a remote
//                 window Spectrum Defaults opens on the Core's stored FFT
//                 size, window, Hz/bin target and FPS with readouts from
//                 the Core's grant; Cal Offset and Display Thread Priority
//                 are disabled with their reasons. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-01 (parity Task 17 follow-up): Rendering >
//                 Decimation applies to every pan's engine. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-IOS-18 / R-IOS-27 / R-R3-08: the FFT, Rendering and
//                waterfall Display controls' keys, labels, items, ranges
//                and defaults are read from core/ControlRanges.h, the
//                table the Core's catalogue sends an app. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-IOS-18 / R-IOS-27 (Display V12): the rest of Spectrum
//                Defaults, Waterfall Defaults, Grid & Scales, TX Display's
//                waterfall and 3D View carry Setup description ids; tooltips
//                that named source code, a task or an unset threshold gap
//                say what the control does; Normalize is enabled only for
//                the Average, Sample and RMS detectors, as Thetis. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-R3-49 / R-R3-21 / A12 (parity Task 30): in a remote
//                window TX Display's nine analyzer controls show the Core's
//                keys and write them, and the Core applies each to its TX
//                analyzer at once; below txDisplayVersion 2 they are
//                disabled with a reason. The waterfall levels, palette, low
//                colour and gradient stay the window's own. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Parity ruling C12: Grid & Scales' per-band dB Max and
//                dB Min show the Core's value as it arrives in a remote
//                window. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-29 - R-IOS-18: 3D Floor says what it does (how far below the
//                 noise floor the 3D surface starts; kept per band).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - "Calibration && Peak Hold" doubles its ampersand so the
//                group title draws it. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
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
#include "gui/UnbuiltFeatures.h"
#include "SetupHelpers.h"
#include "gui/SpectrumWidget.h"
#include "gui/StyleConstants.h"
#include "core/FFTEngine.h"
#include "core/spectrum/FftEnginePool.h"
#include "core/spectrum/WaterfallPalettes.h"
#include "core/ClarityController.h"
#include "core/spectrum/ClarityLevels.h"
#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/DisplayFftSizing.h"
#include "core/TxAnalyzer.h"
#include "core/WdspEngine.h"
#include "core/settings/SettingsProxy.h"
#include "models/Band.h"
#include "models/PanadapterModel.h"
#include "models/RadioModel.h"
#include "core/NoiseFloorTracker.h"
#include "gui/ColorSwatchButton.h"
#include "core/settings/PanDisplayKeys.h"
#include "gui/styles/WorkspacePreview.h"
#include <QSet>
#include <QEvent>
#include <QLineEdit>
#include "gui/widgets/GradientPickerWidget.h"
#include "models/DisplaySettingsModel.h"

#include <QScopedValueRollback>
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
#include <memory>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Helpers (file-local)
// ---------------------------------------------------------------------------

namespace {

// Build a placeholder QGroupBox with a single dimmed label inside.
// Used by TxDisplayPage for groups wired in later sub-phases.
QGroupBox* makePlaceholderGroup(const QString& title, const QString& phText, QWidget* parent)
{
    auto* group  = new QGroupBox(title, parent);
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(4);
    auto* label  = new QLabel(phText, group);
    label->setEnabled(false);
    label->setStyleSheet(QStringLiteral("QLabel { color: #607080; font-style: italic; }"));
    layout->addWidget(label);
    return group;
}

// Display pill plan Task 21: who is setting the waterfall's levels, in the
// Display pill's own words (SpectrumOverlayPanel.cpp, kClarityDrivesNote and
// its neighbours), so Setup and the pill say the same thing.
constexpr auto kSetupClarityDrivesNote =
    "Clarity is setting the levels. Turn it off to use these.";
constexpr auto kSetupMinMaxDrivesNote =
    "The spectrum's dB Max and Min are setting the levels.";
constexpr auto kSetupNfAgcNeedsAgcNote = "Turn on AGC to use NF-AGC.";
// Why Low is gray while waterfall AGC is on: AGC works the low level out
// and the stored Low is not used.
constexpr auto kSetupAgcSetsLowNote = "AGC is setting Low. Turn AGC off to set it yourself.";
// Grid & Scales with no panadapter open.
constexpr auto kSetupNoPanNote = "Open a panadapter to set its band's dB Max and dB Min.";

// Line width is stored in pixels and set in tenths of a pixel: the slider
// counts tenths, 1 (0.1 px) to 50 (5 px). The range is
// ControlRanges::kDisplayLineWidth* (from Thetis udDisplayLineWidth), which
// the Display pill's Line row uses too.
constexpr int   kLineWidthTenthsPerPx = ControlRanges::kDisplayLineWidthTenthsPerPx;
constexpr int   kLineWidthMinTenths   = ControlRanges::kDisplayLineWidthMinTenths;
constexpr int   kLineWidthMaxTenths   = ControlRanges::kDisplayLineWidthMaxTenths;

QString defaultFloatText(double value)
{
    return QString::number(value);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// SpectrumDefaultsPage
// ---------------------------------------------------------------------------

SpectrumDefaultsPage::SpectrumDefaultsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QString::fromLatin1(ControlRanges::kDisplaySpectrumPageTitle), model, parent)
{
    buildUI();
    loadFromRenderer();
}

// Maps SpectrumDefaultsPage FPS slider (10-60) to FFTEngine output FPS
// and the SpectrumWidget display timer so both stay in sync.  Persisted
// so the value survives restart — MainWindow reads
// DisplaySpectrumFps at startup and applies to both via the same path.
void SpectrumDefaultsPage::pushFps(int fps)
{
    // Display pill plan Task 20 (R-DSP-33): the window's handler reaches
    // every stream's engine, every pan and every open Display pill. A page
    // on its own reaches every engine through the pool.
    if (fftHandlers().applyFps) {
        QScopedValueRollback<bool> own(m_applyingOwnFftChange, true);
        fftHandlers().applyFps(fps);
    } else {
        if (!model() || !model()->fftEngine()) { return; }
        if (FftEnginePool* pool = model()->fftEnginePool()) {
            pool->setOutputFps(fps);
        } else {
            model()->fftEngine()->setOutputFps(fps);
        }
        if (auto* sw = model()->spectrumWidget()) {
            sw->setDisplayFps(fps);
        }
        AppSettings::instance().setValue(
            QLatin1String(ControlRanges::kDisplaySpectrumFpsKey), QString::number(fps));
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

void SpectrumDefaultsPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    // The FFT Size slider and the Hz/bin target are also held by the sizing
    // mode (design D38), so refreshFftSizingControls gates those two with
    // "Sizing", from the two reasons together.
    m_stationAvailable = available;
    m_stationReason = reason;
    gateStationControls({m_windowCombo, m_fpSlider, m_fpSpin}, available, reason);
    // Parity Task 17: the Core's stored values arrive with its settings.
    if (available && remoteWindow()) {
        loadStationSpectrumSettings();
    }
    refreshFftSizingControls();
}

void SpectrumDefaultsPage::showFftCells(int size, double binWidthHz)
{
    if (m_fftSizeReadout) {
        m_fftSizeReadout->setText(QString::number(size));
    }
    if (m_binWidthLabel) {
        m_binWidthLabel->setText(
            binWidthHz > 0.0
                ? QString::number(binWidthHz, 'f', ControlRanges::kDisplayBinWidthDecimals)
                : QStringLiteral("0.000"));
    }
}

void SpectrumDefaultsPage::refreshFftSizingControls()
{
    if (!m_fftSizingCombo || !m_fftSizeSlider || !m_hzPerBinTargetSpin) { return; }
    using ControlRanges::DisplayFftSizingMode;
    const DisplayFftSizing sizing = readDisplayFftSizing(AppSettings::instance());
    const DisplayFftReadoutFacts running = fftReadoutFacts();
    const bool fixed = sizing.mode == DisplayFftSizingMode::Fixed;
    const bool hzPerBin = sizing.mode == DisplayFftSizingMode::HzPerBin;

    {
        QSignalBlocker block(m_fftSizingCombo);
        m_fftSizingCombo->setCurrentIndex(
            m_fftSizingCombo->findData(static_cast<int>(sizing.mode)));
    }

    // One gate per control, from both reasons; the station's wins.
    gateStationControls({m_fftSizingCombo}, m_stationAvailable, m_stationReason);
    gateStationControls({m_fftSizeSlider}, m_stationAvailable && fixed,
                        m_stationAvailable ? displayFftSizeHeldTip(sizing.mode)
                                           : m_stationReason);
    gateStationControls({m_hzPerBinTargetSpin}, m_stationAvailable && hzPerBin,
                        m_stationAvailable ? displayHzPerBinHeldTip() : m_stationReason);

    // The slider: the stored size in Fixed, where it sets the size;
    // otherwise the size running now. A size still being chosen stays.
    const bool sizeBeingChosen = fixed
        && (m_fftSizeSlider->isSliderDown()
            || (m_fftSizeKeyTimer && m_fftSizeKeyTimer->isActive()));
    if (!fixed) {
        if (m_fftSizeKeyTimer) { m_fftSizeKeyTimer->stop(); }
        m_fftSizeReleasePending = false;
    }
    const int shownSize = (fixed || running.runningSize <= 0) ? sizing.storedSize
                                                              : running.runningSize;
    if (!sizeBeingChosen) {
        QSignalBlocker block(m_fftSizeSlider);
        m_fftSizeSlider->setValue(qBound(0, ControlRanges::displayFftSizePosition(shownSize),
                                         ControlRanges::kDisplayFftSizePositionMax));
    }

    // The two cells: never a bin width that is not running, except the one
    // of the size being chosen in Fixed. A remote window's cells are what
    // its Core granted.
    if (!sizeBeingChosen) {
        if (remoteWindow()) {
            refreshGrantedReadouts();
        } else if (fixed) {
            showChosenFftSize(m_fftSizeSlider->value());
        } else if (running.runningSize > 0) {
            showFftCells(running.runningSize, running.binWidthHz());
        } else {
            showChosenFftSize(m_fftSizeSlider->value());
        }
    }

    // The target: the one in force in Hz/bin; otherwise the remembered one.
    {
        QSignalBlocker block(m_hzPerBinTargetSpin);
        m_hzPerBinTargetSpin->setValue(
            hzPerBin ? sizing.hzPerBinTarget
                     : displayHzPerBinShownWhileHeld(sizing.lastHzPerBinTarget,
                                                     running.binWidthHz()));
    }
    refreshFftReadoutLine();
    emit fftSizingPresentationChanged();
}

bool SpectrumDefaultsPage::remoteWindow()
{
    return model() && !model()->ownsLocalDsp();
}

void SpectrumDefaultsPage::setFftHandlers(const SetupFftHandlers& handlers)
{
    SetupPage::setFftHandlers(handlers);
    refreshFftSizingControls();
}

int SpectrumDefaultsPage::fftBaselineInUse()
{
    // The slider's own value is the stored DisplayFftSize, in a local
    // window and a remote one alike. An engine's baseline is that size in
    // the Fixed sizing mode only (4096 in Auto and Hz/bin, design D38), so
    // it is not what the slider shows.
    return readDisplayFftSizing(AppSettings::instance()).storedSize;
}

void SpectrumDefaultsPage::showChosenFftSize(int position)
{
    position = qBound(0, position, ControlRanges::kDisplayFftSizePositionMax);
    const int newSize = ControlRanges::displayFftSizeAt(position);

    // Size readout (mirrors lblRX1FFT_size update at setup.cs:16153
    // [v2.10.3.13]).
    if (m_fftSizeReadout) {
        m_fftSizeReadout->setText(QString::number(newSize));
    }
    // Bin-width readout, fresh from newSize.  Format "N3" = 3 decimal
    // places per Thetis setup.cs:16152 [v2.10.3.13].
    // A remote window's cell shows what its Core granted
    // (refreshGrantedReadouts); it is left for the grant.
    auto* fe = model() ? model()->fftEngine() : nullptr;
    if (m_binWidthLabel && fe && !remoteWindow()) {
        // The width that size runs at: the decimation in use divides the
        // rate (FFT sizing mode, design D38: the cell never shows a width
        // the FFT would not run).
        const double sampleRate = fe->sampleRate() / double(std::max(1, fe->decimation()));
        const double bw = (newSize > 0) ? sampleRate / newSize : 0.0;
        m_binWidthLabel->setText(
            bw > 0.0 ? QString::number(bw, 'f', ControlRanges::kDisplayBinWidthDecimals)
                     : QStringLiteral("0.000"));
    }
}

void SpectrumDefaultsPage::applyChosenFftSize()
{
    if (!m_fftSizeSlider) { return; }
    // The slider sets the size in the Fixed sizing mode only (design D38).
    // In the other modes it is grayed and shows the running size, which is
    // never a size to store.
    if (readDisplayFftSizingMode(AppSettings::instance())
        != ControlRanges::DisplayFftSizingMode::Fixed) {
        refreshFftSizingControls();
        return;
    }
    const int v = qBound(0, m_fftSizeSlider->value(), ControlRanges::kDisplayFftSizePositionMax);
    const int newSize = ControlRanges::displayFftSizeAt(v);
    // The slider is back where the station already is: nothing to change.
    // A remote window's cells go back to what its Core runs.
    if (newSize == fftBaselineInUse()) {
        if (remoteWindow()) { refreshGrantedReadouts(); }
        return;
    }
    if (!model()) { return; }
    auto* fe = model()->fftEngine();
    auto* sw = model()->spectrumWidget();

    // The baseline only (design D22). The size each stream runs is planned
    // from it (ControlRanges::planAutoZoomFft): the window's handler sets
    // every stream's baseline, writes DisplayFftSize once and replans every
    // stream. This page never sets a running size itself; setting it here
    // is what made a drag replan at every position and left the slider
    // showing a zoomed size as if it were the setting (issue #296).
    if (fftHandlers().applyBaseline) {
        QScopedValueRollback<bool> own(m_applyingOwnFftChange, true);
        fftHandlers().applyBaseline(newSize);
    } else if (fe) {
        // A page on its own: the key, every engine through the pool, and a
        // request for the window's replan.
        // 2026-05-26 KG4VCF bench fix: persist the FFT size so it
        // survives restart.  Previously the slider drove the engine
        // but never wrote to AppSettings, so launch always reverted
        // to the FFTEngine ctor default (typically 4096).
        AppSettings::instance().setValue(
            QLatin1String(ControlRanges::kDisplayFftSizeKey),
            QString::number(newSize));
        // The size reaches the engines through the sizing mode: it is their
        // baseline in Fixed only (design D38).
        const DisplayFftSizing sizing = readDisplayFftSizing(AppSettings::instance());
        if (FftEnginePool* pool = model()->fftEnginePool()) {
            pool->setSizing(sizing.mode, sizing.baseline(), sizing.hzPerBinTarget);
        } else {
            fe->setFftSizeBaseline(sizing.baseline());
        }
        if (sw) {
            sw->requestAutoZoomReplan();
        }
    }

    // Mirror to on-spectrum overlay (NereusSDR-original; the
    // overlay toggle gates the spectrum-corner readout).
    if (m_binWidthReadout && sw) {
        const double bw = sw->binWidthHz();
        m_binWidthReadout->setText(
            bw > 0.0 ? QStringLiteral("%1 Hz/bin").arg(bw, 0, 'f', 3)
                     : QStringLiteral("- Hz/bin"));
    }
    // Parity Task 17: a remote window's readouts show what the Core
    // runs; they move when its new grant arrives.
    if (remoteWindow()) {
        refreshGrantedReadouts();
    }

    // Rulings wave R2: the FFT size offset and the noise-floor fast attack
    // (Thetis setup.cs:16154, 16156 [v2.10.3.13]) are no longer set here.
    // Every FFT size change sets them, this slider's included:
    // FFTEngine::setFftSizeBaseline() stores the offset and says the
    // baseline changed, and MainWindow restarts the trackers' fast attack.
    refreshFftReadoutLine();
}

DisplayFftReadoutFacts SpectrumDefaultsPage::fftReadoutFacts()
{
    // The window knows which engine the active pan shows.
    if (fftHandlers().readoutFacts) {
        return fftHandlers().readoutFacts();
    }
    DisplayFftReadoutFacts facts;
    if (!model()) { return facts; }
    auto* sw = model()->spectrumWidget();
    auto* fe = model()->fftEngine();
    FftEnginePool* pool = model()->fftEnginePool();
    facts.decimation = pool ? pool->decimation() : (fe ? fe->decimation() : 1);
    if (remoteWindow()) {
        // A pan a Core feeds: the size the Core granted it.
        facts.runningSize = sw ? sw->remoteGrantedFftSize() : 0;
        facts.sampleRateHz = sw ? sw->sampleRate() : 0.0;
    } else if (fe) {
        facts.runningSize = fe->fftSize();
        facts.sampleRateHz = fe->sampleRate();
    }
    return facts;
}

void SpectrumDefaultsPage::refreshFftReadoutLine()
{
    if (!m_fftReadoutLine) { return; }
    m_fftReadoutLine->setText(displayFftReadoutText(fftReadoutFacts()));
}

void SpectrumDefaultsPage::loadStationSpectrumSettings(bool keepChosenSize)
{
    // The station keys (SettingsScope: DisplayFftSize, DisplayFftWindow,
    // DisplayHzPerBinTarget, DisplaySpectrumFps) hold the Core's values in a
    // remote window; this window's own FFT engine is idle there.
    using namespace ControlRanges;
    auto& settings = AppSettings::instance();
    const int fftSize = settings.value(QLatin1String(kDisplayFftSizeKey),
                                       QString::number(kDisplayFftSizeDefault)).toString().toInt();
    const int sliderVal = displayFftSizePosition(fftSize);
    const int window = qBound(0, settings.value(QLatin1String(kDisplayFftWindowKey),
                                  QString::number(kDisplayFftWindowDefault))
                                  .toString().toInt(), int(WindowFunction::Count) - 1);
    const int fps = qBound(1, settings.value(QLatin1String(kDisplaySpectrumFpsKey),
                                  QString::number(kDisplaySpectrumFpsDefault))
                                  .toString().toInt(), 60);
    {
        QSignalBlocker b1(m_fftSizeSlider);
        QSignalBlocker b2(m_windowCombo);
        QSignalBlocker b4(m_fpSlider);
        QSignalBlocker b5(m_fpSpin);
        if (!keepChosenSize) {
            m_fftSizeSlider->setValue(qBound(0, sliderVal, kDisplayFftSizePositionMax));
        }
        m_windowCombo->setCurrentIndex(window);
        m_fpSlider->setValue(fps);
        m_fpSpin->setValue(fps);
    }
    // The sizing mode, the slider outside Fixed, the Hz/bin target and the
    // two cells (design D38); a size still being chosen stays.
    refreshFftSizingControls();
}

void SpectrumDefaultsPage::stationFftSettingsChanged()
{
    // This page's own change: its controls already show it.
    if (m_applyingOwnFftChange) { return; }
    if (!m_fftSizeSlider || !m_windowCombo || !m_hzPerBinTargetSpin || !m_fpSlider) { return; }
    // A size the operator is still choosing (the handle held, or keyboard
    // steps waiting to be applied) is left where it is.
    const bool sizeBeingChosen = m_fftSizeSlider->isSliderDown()
        || (m_fftSizeKeyTimer && m_fftSizeKeyTimer->isActive());
    FFTEngine* fe = model() ? model()->fftEngine() : nullptr;
    if (remoteWindow()) {
        // The station keys hold the Core's values.
        loadStationSpectrumSettings(sizeBeingChosen);
    } else if (fe) {
        // The engine holds them, as loadFromRenderer reads them.
        {
            QSignalBlocker b2(m_windowCombo);
            QSignalBlocker b4(m_fpSlider);
            m_windowCombo->setCurrentIndex(static_cast<int>(fe->windowFunction()));
            m_fpSlider->setValue(fe->outputFps());
            if (m_fpSpin) {
                QSignalBlocker b5(m_fpSpin);
                m_fpSpin->setValue(fe->outputFps());
            }
        }
    }
    if (m_decimationSpin && fe) {
        QSignalBlocker b(m_decimationSpin);
        m_decimationSpin->setValue(fe->decimation());
    }
    // The sizing mode, the size slider, the Hz/bin target and the two cells
    // (design D38); a size still being chosen stays.
    refreshFftSizingControls();
}

void SpectrumDefaultsPage::applySmoothDefaults()
{
    // The values RadioModel::applyClaritySmoothDefaults gives one pan, here
    // as Setup defaults (design D26): each is stored as the default and
    // moves every pan that has no value of its own for it.
    constexpr int scheme = static_cast<int>(WfColorScheme::ClarityBlue);
    applyDisplayDefault(QStringLiteral("DisplayWfColorScheme"), QString::number(scheme),
                        [](SpectrumWidget* pan) {
        pan->displaySettings()->setWfColorScheme(scheme);
    });
    constexpr SpectrumAveraging averaging = SpectrumAveraging::LogRecursive;
    applyDisplayDefault(QStringLiteral("DisplaySpectrumAveraging"),
                        QString::number(static_cast<int>(averaging)),
                        [](SpectrumWidget* pan) { pan->setSpectrumAveraging(averaging); });
    constexpr int averageTimeMs = 650;
    applyDisplayDefault(QStringLiteral("DisplaySpectrumAverageTimeMs"),
                        QString::number(averageTimeMs),
                        [](SpectrumWidget* pan) {
        pan->setSpectrumAverageTimeMs(averageTimeMs);
    });
    const QColor trace(0xff, 0xff, 0xff, 230);
    applyDisplayDefault(QStringLiteral("DisplayFillColor"), trace.name(QColor::HexArgb),
                        [trace](SpectrumWidget* pan) { pan->setFillColor(trace); });
    // The trace line takes the same white, as a default like the rest: a
    // trace color chosen in Setup would otherwise stay and the profile's
    // trace would not be white. A pan with a trace color of its own keeps
    // it.
    applyDisplayDefault(QStringLiteral("DisplayTraceColor"), trace.name(QColor::HexArgb),
                        [trace](SpectrumWidget* pan) { pan->setTraceColor(trace); });
    applyDisplayDefault(QStringLiteral("DisplayPanFill"), QStringLiteral("False"),
                        [](SpectrumWidget* pan) { pan->displaySettings()->setPanFill(false); });
    convertManualWaterfallModesForSmoothReset();
    constexpr int updatePeriodMs = 30;
    applyDisplayDefault(QStringLiteral("DisplayWfUpdatePeriodMs"),
                        QString::number(updatePeriodMs),
                        [](SpectrumWidget* pan) {
        pan->setWfUpdatePeriodMs(updatePeriodMs);
    });
    // The first-run profile is not applied over this on the next launch.
    // The marker is the model's key, so the model writes it.
    if (RadioModel* radio = model()) {
        radio->markDisplayProfileApplied();
    }
}

void SpectrumDefaultsPage::refreshGrantedReadouts()
{
    // What the Core runs for the active pan: its granted FFT size and the
    // bin width that gives (SpectrumWidget::binWidthHz). Before the first
    // grant, the stored size.
    auto* sw = model() ? model()->spectrumWidget() : nullptr;
    const int granted = sw ? sw->remoteGrantedFftSize() : 0;
    const int size = granted > 0 ? granted
        : AppSettings::instance().value(QLatin1String(ControlRanges::kDisplayFftSizeKey),
                                        QString::number(ControlRanges::kDisplayFftSizeDefault))
              .toString().toInt();
    if (m_fftSizeReadout) {
        m_fftSizeReadout->setText(QString::number(size));
    }
    const double bw = (sw && granted > 0) ? sw->binWidthHz() : 0.0;
    if (m_binWidthLabel) {
        m_binWidthLabel->setText(bw > 0.0 ? QString::number(bw, 'f', 3)
                                          : QStringLiteral("0.000"));
    }
    if (m_binWidthReadout) {
        m_binWidthReadout->setText(bw > 0.0 ? QStringLiteral("%1 Hz/bin").arg(bw, 0, 'f', 3)
                                            : QStringLiteral("- Hz/bin"));
    }
}

void SpectrumDefaultsPage::loadFromRenderer()
{
    // Display pill plan Task 21 (D26): the per-pan rows (detector, averaging,
    // fill, line width, gradient, calibration, peak hold, cursor frequency)
    // show the Setup default, not the active pan's own value. Each row's
    // follower (bindDefault* and followDisplayDefault in buildUI) reads it.
    showDisplayDefaults();

    if (!model()) { return; }
    auto* sw = model()->spectrumWidget();
    auto* fe = model()->fftEngine();
    if (!sw || !fe) { return; }

    QSignalBlocker b1(m_fftSizeSlider);
    QSignalBlocker b2(m_windowCombo);
    QSignalBlocker b3(m_fpSlider);
    QSignalBlocker bHz(m_hzPerBinTargetSpin);
    QSignalBlocker b12(m_threadPriorityCombo);

    // FFT size -- recover slider value from current FFT size via log2 /
    // (4096) per Thetis setup.cs:16142 [v2.10.3.13]
    //   FFTSize = 4096 * Math.Pow(2, Math.Floor(slider.Value))
    // Inverse: slider = log2(FFTSize / 4096), clamped to [0, 6].
    // Display pill plan Task 20 (D24; issue #296): the baseline, the size
    // the slider set, never fftSize(), the size the engine runs. Zoomed in,
    // the two differ (a 4096 baseline runs 16384 at a quarter of the span),
    // and loading the running size showed the slider somewhere the operator
    // never put it.
    // FFT sizing mode (design D38): that size is the stored DisplayFftSize;
    // an engine's baseline is it in Fixed only.
    const int fs = fftBaselineInUse();
    const int sliderVal = qBound(0, ControlRanges::displayFftSizePosition(fs),
                                 ControlRanges::kDisplayFftSizePositionMax);
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
            bw > 0.0 ? QString::number(bw, 'f', ControlRanges::kDisplayBinWidthDecimals)
                     : QStringLiteral("0.000"));
    }

    // FFT window -- enum value matches combo index 1:1 (both follow WDSP
    // analyzer.c:52-173 [v2.10.3.13] case ordering).
    m_windowCombo->setCurrentIndex(static_cast<int>(fe->windowFunction()));

    m_fpSlider->setValue(fe->outputFps());
    // The Hz/bin target is set with the sizing mode, at the end.
    // Defensive: mirror to spin readout.  QSignalBlocker on m_fpSlider above
    // blocks the slider->spin sync wired by makeSliderRow (SetupHelpers.cpp:47),
    // so without this explicit setValue the spin would stick at the
    // makeSliderRow constructor default (30) on every dialog reopen even when
    // the slider correctly tracks the persisted output FPS.
    if (m_fpSpin) {
        QSignalBlocker bspin(m_fpSpin);
        m_fpSpin->setValue(fe->outputFps());
    }
    // Task 2.1's split combos, the fill rows, line width, gradient,
    // calibration, peak hold and the cursor frequency are per-pan defaults:
    // showDisplayDefaults() above set them.

    // Colour pickers (S11/S12/S13) moved to Setup → Appearance → Colors & Theme.

    // The overlay picture controls now follow their selected scope through
    // showDisplayDefaults(), as the other picture controls do above.
    if (m_binWidthReadout) {
        const double width = sw->binWidthHz();
        m_binWidthReadout->setText(width > 0.0 ? QStringLiteral("%1 Hz/bin").arg(width, 0, 'f', 3)
                                             : QStringLiteral("– Hz/bin"));
    }
    if (m_decimationSpin && fe) {
        QSignalBlocker b(m_decimationSpin);
        m_decimationSpin->setValue(fe->decimation());
    }
    if (remoteWindow()) {
        loadStationSpectrumSettings();
    }
    // FFT sizing mode (design D38): the mode, and what it makes of the size
    // slider, the Hz/bin target and the two cells.
    refreshFftSizingControls();
}

QString SpectrumDefaultsPage::smoothDefaultsQuestion()
{
    return QStringLiteral(
        "This sets the smooth profile for the selected Settings for scope: "
        "Clarity Blue, log-recursive averaging with a 650 ms averaging time, "
        "a white trace without fill, and Rate at 30 ms. It also sets Scheme "
        "and Rate on Waterfall and changes Manual waterfall levels to Auto "
        "on Levels. With All pans selected, these become the defaults for "
        "every pan, including pans that are not open.\n\n"
        "Your FFT size, frequency, band stack, and per-band grid "
        "ranges are NOT affected.\n\n"
        "Continue?");
}

void SpectrumDefaultsPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // Phase 3G-9b: Reset to Smooth Defaults button. Destructive — shows
    // a confirmation dialog before overwriting because it resets the
    // Spectrum / Waterfall display state.
    auto* resetBtn = new QPushButton(QStringLiteral("Reset to Smooth Defaults"), this);
    resetBtn->setProperty("nereusSetupId", "display.spectrumDefaults.smoothDefaults");
    // R-IOS-18: the tooltip names what RadioModel::applyClaritySmoothDefaults
    // sets; it used to promise a "tight threshold gap" nothing sets.
    // Rulings wave R7: it sets the defaults of every panadapter
    // (applySmoothDefaults), and a pan with its own value keeps it; the
    // tooltip and the question used to say it overwrote one panadapter.
    // Approved scoped reset supersedes that historical own-value promise:
    // it resets these rows in Settings for and changes Manual levels to Auto.
    resetBtn->setToolTip(QStringLiteral(
        "Set the selected Settings for scope to the NereusSDR smooth profile: "
        "Clarity Blue palette, log-recursive averaging with a 650 ms averaging "
        "time, a white trace without fill, and Rate at 30 ms. Also sets Scheme "
        "and Rate on Waterfall and changes Manual waterfall levels to Auto "
        "on Levels. All pans applies these defaults to every pan, including "
        "pans that are not open. FFT size, frequency, band stack, and per-band "
        "grid ranges are not affected."));
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        const auto rc = QMessageBox::question(
            this,
            QStringLiteral("Reset to Smooth Defaults"),
            smoothDefaultsQuestion(),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (rc != QMessageBox::Yes) { return; }
        if (model()) {
            // Each value as a Setup default, so every pan without a value
            // of its own takes it (applySmoothDefaults).
            applySmoothDefaults();
            // Reload this page so the controls immediately reflect the
            // new model state. Sibling pages (Waterfall Defaults, Grid &
            // Scales) will update on their next SetupDialog repaint
            // because their loadFromRenderer() reads from the model.
            loadFromRenderer();
        }
    });
    addContent(resetBtn);

    // Phase 3G-9c: Clarity adaptive display tuning master toggle.
    // When on, ClarityController drives Waterfall Low/High thresholds
    // from a percentile-based noise-floor estimate. When off, thresholds
    // stay at whatever value they were last set to (no AGC fallback).
    auto* clarityToggle = new QCheckBox(
        QStringLiteral("Enable Clarity (adaptive waterfall tuning)"), this);
    clarityToggle->setProperty("nereusSetupId", "display.spectrumDefaults.clarity");
    clarityToggle->setToolTip(QStringLiteral(
        "Clarity keeps the waterfall's black point just above the noise "
        "floor as band conditions and tuning change. Black Level and Color "
        "Gain set how far above the noise the waterfall starts and how much "
        "range it shows. When Clarity is off, Low and High set the levels."));
    if (model()) {
        clarityToggle->setChecked(model()->clarityEnabled());
        connect(clarityToggle, &QCheckBox::toggled, model(), &RadioModel::setClarityEnabled);
        connect(model(), &RadioModel::clarityEnabledChanged, clarityToggle,
                [clarityToggle](bool enabled) {
            const QSignalBlocker blocker(clarityToggle);
            clarityToggle->setChecked(enabled);
        });
        // Sync the clarityActive flag so legacy AGC knows whether
        // to stand down. Off = AGC free to run, On = AGC yields
        // once Clarity emits its first threshold update.
    }
    addContent(clarityToggle);

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
        QString::fromLatin1(ControlRanges::kDisplayFftGroupTitle), this);
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
    // Row 0 (NereusSDR-original; FFT sizing mode, design D38): "Sizing",
    // what sets the FFT size. The Display pill's FFT tab has the same three
    // choices as buttons. Setup description version 28 describes it
    // (display.spectrumDefaults.fftSizing).
    auto* sizingPrefix = new QLabel(QStringLiteral("Sizing"), fftGroup);
    m_fftSizingCombo = new QComboBox(fftGroup);
    m_fftSizingCombo->setObjectName(QStringLiteral("setupFftSizingMode"));
    m_fftSizingCombo->setProperty("nereusSetupId",
                                  QStringLiteral("display.spectrumDefaults.fftSizing"));
    for (const DisplayFftSizingChoice& choice : kDisplayFftSizingChoices) {
        m_fftSizingCombo->addItem(QLatin1String(choice.label), static_cast<int>(choice.mode));
    }
    m_fftSizingCombo->setToolTip(displayFftSizingTip());
    m_fftSizingCombo->setAccessibleName(QStringLiteral("Sizing"));
    connect(m_fftSizingCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index < 0) { return; }
        using ControlRanges::DisplayFftSizingMode;
        const auto mode =
            static_cast<DisplayFftSizingMode>(m_fftSizingCombo->itemData(index).toInt());
        // Entering Hz/bin with no target remembered starts from the bin
        // width running now, so the size does not move until the target
        // does (design D39).
        const double running = fftReadoutFacts().binWidthHz();
        const double ifNoneRemembered =
            running > 0.0 ? ControlRanges::hzPerBinTargetForBinWidth(running) : 0.0;
        if (fftHandlers().applySizingMode) {
            QScopedValueRollback<bool> own(m_applyingOwnFftChange, true);
            fftHandlers().applySizingMode(mode, ifNoneRemembered);
        } else {
            // A page on its own: the keys, every engine through the pool,
            // and a request for the window's replan.
            auto& settings = AppSettings::instance();
            double target = 0.0;
            if (mode == DisplayFftSizingMode::HzPerBin) {
                target = readDisplayFftSizing(settings).lastHzPerBinTarget;
                if (!(target > 0.0)) {
                    target = ifNoneRemembered;
                }
            }
            writeDisplayFftSizingMode(settings, mode, target);
            const DisplayFftSizing sizing = readDisplayFftSizing(settings);
            if (FftEnginePool* pool = model() ? model()->fftEnginePool() : nullptr) {
                pool->setSizing(sizing.mode, sizing.baseline(), sizing.hzPerBinTarget);
            } else if (auto* fe = model() ? model()->fftEngine() : nullptr) {
                fe->setFftSizeBaseline(sizing.baseline());
                fe->setHzPerBinTarget(sizing.hzPerBinTarget);
            }
            if (auto* sw = model() ? model()->spectrumWidget() : nullptr) {
                sw->requestAutoZoomReplan();
            }
        }
        // What the station now holds: the mode taken, or the one it kept.
        refreshFftSizingControls();
    });
    fftGrid->addWidget(sizingPrefix,     0, 0);
    fftGrid->addWidget(m_fftSizingCombo, 0, 1, 1, 3);  // span cols 1-3

    auto* sizeHeader = new QLabel(QLatin1String(ControlRanges::kDisplayFftSizeLabel), fftGroup);
    sizeHeader->setAlignment(Qt::AlignHCenter);
    fftGrid->addWidget(sizeHeader, 1, 1, 1, 2);  // span cols 1-2 (centered)

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
    // The range, the positions' sizes and the start come from
    // ControlRanges.h (the catalogue's `display` table).
    m_fftSizeSlider = new QSlider(Qt::Horizontal, fftGroup);
    m_fftSizeSlider->setProperty("nereusSetupId", "display.spectrumDefaults.fftSize");
    m_fftSizeSlider->setRange(0, ControlRanges::kDisplayFftSizePositionMax);
    m_fftSizeSlider->setSingleStep(1);
    m_fftSizeSlider->setPageStep(1);
    m_fftSizeSlider->setTickPosition(QSlider::NoTicks);
    m_fftSizeSlider->setValue(ControlRanges::kDisplayFftSizeSliderInitial);
    // Display pill plan Task 20 (D22; R-DSP-31): the value moves when the
    // handle is let go, not at each position a drag crosses, as the Display
    // pill's Size slider does.
    m_fftSizeSlider->setTracking(false);
    m_fftSizeSlider->setToolTip(QStringLiteral(
        "FFT size used for spectrum analysis. Range 4096 to 262144 in "
        "powers of two. Larger = finer frequency resolution and a smaller "
        "bin width, at higher CPU cost."));
    // The header above the slider is its label; a screen reader gets the
    // same word, as the Sizing list does.
    m_fftSizeSlider->setAccessibleName(QLatin1String(ControlRanges::kDisplayFftSizeLabel));
    fftGrid->addWidget(minLabel,         2, 0);
    fftGrid->addWidget(m_fftSizeSlider,  2, 1, 1, 2);  // span cols 1-2
    fftGrid->addWidget(maxLabel,         2, 3);

    // Row 2: bin-width readout + FFT-size readout, side by side per
    // Thetis (both at y=79).  Each pair is a "prefix label + bisque
    // sunken numeric cell".

    // "Bin Width (Hz)" prefix + value cell.  Thetis labelTS142
    // ("Bin Width (Hz)") at (6, 79) + lblDisplayBinWidth at (87, 79)
    // [v2.10.3.13].
    auto* binWidthPrefix =
        new QLabel(QLatin1String(ControlRanges::kDisplayBinWidthLabel), fftGroup);
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
        "FFT size in bins: the size the slider is on when Sizing is Fixed, "
        "otherwise the size running now."));

    fftGrid->addWidget(binWidthPrefix,   3, 0);
    fftGrid->addWidget(m_binWidthLabel,  3, 1);
    fftGrid->addWidget(fftSizePrefix,    3, 2);
    fftGrid->addWidget(m_fftSizeReadout, 3, 3);

    // Row 3 (NereusSDR-original; display pill plan Task 20, R-DSP-61):
    // what the active pan's FFT runs at now, in the Display pill's words:
    // bin width and time per transform at the running size, or that the
    // Hz/bin target is what set the size. The two cells above are the
    // slider's own size; this is the size zoom and the target made of it.
    m_fftReadoutLine = new QLabel(fftGroup);
    m_fftReadoutLine->setObjectName(QStringLiteral("setupFftReadout"));
    m_fftReadoutLine->setProperty("nereusSetupId", "display.spectrumDefaults.fftReadout");
    m_fftReadoutLine->setWordWrap(true);
    m_fftReadoutLine->setStyleSheet(QStringLiteral("QLabel { color: #8aa8c0; }"));
    fftGrid->addWidget(m_fftReadoutLine, 4, 0, 1, 4);

    // Row 4: "Window" prefix + combo.  Thetis labelTS147 ("Window") at
    // (6, 101) + comboDispWinType at (83, 101) [v2.10.3.13].
    auto* windowPrefix = new QLabel(QLatin1String(ControlRanges::kDisplayFftWindowLabel), fftGroup);
    m_windowCombo = new QComboBox(fftGroup);
    m_windowCombo->setProperty("nereusSetupId", "display.spectrumDefaults.window");
    // 7 items, ordering verbatim per Thetis comboDispWinType.Items at
    // setup.designer.cs:34966-34973 [v2.10.3.13].  Combo index maps 1:1
    // to WindowFunction enum integer (both follow WDSP analyzer.c case
    // ordering, so no remap needed).
    // The items (ControlRanges::kDisplayFftWindows) are in Thetis's order,
    // each at its WindowFunction's index.
    for (const ControlRanges::DisplayChoiceItem& item : ControlRanges::kDisplayFftWindows) {
        m_windowCombo->addItem(QLatin1String(item.label));
    }
    m_windowCombo->setToolTip(QStringLiteral(
        "FFT window function. Rectangular has the narrowest main lobe but "
        "the worst sidelobes. Blackman-Harris (4T or 7T) gives strong "
        "sidelobe rejection. Flat-Top is best for amplitude calibration. "
        "Kaiser is parameterized (Kaiser Pi shape parameter)."));
    // The grid row is under Hz/bin Target's, so the three rows that size
    // the FFT (Sizing, Size, Hz/bin Target) sit together.
    fftGrid->addWidget(windowPrefix,  6, 0);
    fftGrid->addWidget(m_windowCombo, 6, 1, 1, 3);  // span cols 1-3

    // Row 5: Hz/bin Target — NereusSDR-original auto-zoom override (Option
    // 3, 2026-05-08).  0 = off (bins-in-window default behaviour: FFT
    // replans on zoom to keep ~baseline bins in the visible window, Hz/bin
    // varies).  > 0 = lock Hz/bin at the given value regardless of zoom
    // (useful for hunting narrow CW/digital signals where you want the
    // same frequency resolution at any zoom).
    auto* hzPerBinPrefix = new QLabel(QLatin1String(ControlRanges::kDisplayHzPerBinTargetLabel),
                                      fftGroup);
    m_hzPerBinTargetSpin = new QDoubleSpinBox(fftGroup);
    m_hzPerBinTargetSpin->setProperty("nereusSetupId", "display.spectrumDefaults.hzPerBinTarget");
    // FFT sizing mode (design D38): the target is a bin width, from one
    // step up; "Off" is no longer a value of this control, it is the Auto
    // and Fixed choices of "Sizing" above.
    m_hzPerBinTargetSpin->setRange(ControlRanges::kDisplayHzPerBinTargetStep,
                                   ControlRanges::kDisplayHzPerBinTargetMax);
    m_hzPerBinTargetSpin->setSingleStep(ControlRanges::kDisplayHzPerBinTargetStep);
    m_hzPerBinTargetSpin->setDecimals(ControlRanges::kDisplayHzPerBinTargetDecimals);
    m_hzPerBinTargetSpin->setSuffix(
        QStringLiteral(" ") + QString::fromLatin1(ControlRanges::kDisplayHzPerBinTargetUnit));
    m_hzPerBinTargetSpin->setToolTip(displayHzPerBinTip());
    m_hzPerBinTargetSpin->setAccessibleName(
        QLatin1String(ControlRanges::kDisplayHzPerBinTargetLabel));
    connect(m_hzPerBinTargetSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        // The target sets the size in the Hz/bin sizing mode only (design
        // D38). In the other modes the control is grayed and shows the
        // remembered target, which is not a request.
        if (readDisplayFftSizingMode(AppSettings::instance())
            != ControlRanges::DisplayFftSizingMode::HzPerBin) {
            refreshFftSizingControls();
            return;
        }
        // Display pill plan Task 20 (R-DSP-33): the window's handler sets
        // every stream's target, writes the key and replans every stream.
        if (fftHandlers().applyHzPerBinTarget) {
            {
                QScopedValueRollback<bool> own(m_applyingOwnFftChange, true);
                fftHandlers().applyHzPerBinTarget(v);
            }
            refreshFftSizingControls();
            return;
        }
        // A page on its own: the value through the sizing mode (above 0 is
        // the Hz/bin mode with that target, "Off" is Auto, and a Fixed mode
        // is left alone; design D38), then every engine through the pool.
        auto& settings = AppSettings::instance();
        using ControlRanges::DisplayFftSizingMode;
        if (v > 0.0) {
            writeDisplayFftSizingMode(settings, DisplayFftSizingMode::HzPerBin, v);
        } else if (readDisplayFftSizingMode(settings) == DisplayFftSizingMode::Fixed) {
            writeDisplayFftSizingMode(settings, DisplayFftSizingMode::Fixed);
        } else {
            writeDisplayFftSizingMode(settings, DisplayFftSizingMode::Auto);
        }
        const DisplayFftSizing sizing = readDisplayFftSizing(settings);
        if (FftEnginePool* pool = model() ? model()->fftEnginePool() : nullptr) {
            pool->setSizing(sizing.mode, sizing.baseline(), sizing.hzPerBinTarget);
        } else if (auto* fe = model() ? model()->fftEngine() : nullptr) {
            fe->setFftSizeBaseline(sizing.baseline());
            fe->setHzPerBinTarget(sizing.hzPerBinTarget);
        }
        // Trigger immediate replan so the new policy applies without
        // waiting for the next zoom action.
        if (auto* sw = model() ? model()->spectrumWidget() : nullptr) {
            sw->requestAutoZoomReplan();
        }
        refreshFftSizingControls();
    });
    fftGrid->addWidget(hzPerBinPrefix,        5, 0);
    fftGrid->addWidget(m_hzPerBinTargetSpin,  5, 1, 1, 3);  // span cols 1-3
    // The keyboard goes down the group as it reads.
    QWidget::setTabOrder(m_fftSizeSlider, m_hzPerBinTargetSpin);
    QWidget::setTabOrder(m_hzPerBinTargetSpin, m_windowCombo);

    // Slider handlers.  Mirrors Thetis tbDisplayFFTSize_Scroll
    // at setup.cs:16142-16166 [v2.10.3.13] -- the nine-action sequence
    // (skip mutex/UpdateRXSpectrumDisplayVars/InitFFTFillTime per design
    // notes; everything else ported).
    // NereusSDR, based on Thetis setup.cs:16142-16166 [v2.10.3.13]: Thetis
    // sets the size at every position its slider crosses. Here the size is
    // one baseline for every stream, with zoom planned on top of it, so a
    // drag acts once, as the handle is let go, and keyboard steps act once,
    // kDisplayFftSizeKeyDelayMs after the last (design D22; R-DSP-31). The
    // two cells follow the handle at once (showChosenFftSize); what is
    // applied is in applyChosenFftSize.
    m_fftSizeKeyTimer = new QTimer(this);
    m_fftSizeKeyTimer->setObjectName(QStringLiteral("setupFftSizeKeyTimer"));
    m_fftSizeKeyTimer->setSingleShot(true);
    m_fftSizeKeyTimer->setInterval(kDisplayFftSizeKeyDelayMs);
    connect(m_fftSizeKeyTimer, &QTimer::timeout, this, [this]() { applyChosenFftSize(); });
    connect(m_fftSizeSlider, &QSlider::sliderMoved,
            this, [this](int position) { showChosenFftSize(position); });
    // Qt says the handle was let go before it moves the value, so a release
    // that is about to change the value is noted here and acted on there.
    connect(m_fftSizeSlider, &QSlider::sliderReleased, this, [this]() {
        m_fftSizeReleasePending =
            m_fftSizeSlider->sliderPosition() != m_fftSizeSlider->value();
        // Let go where it started: the cells go back to the size in use.
        if (!m_fftSizeReleasePending) {
            showChosenFftSize(m_fftSizeSlider->value());
            if (remoteWindow()) { refreshGrantedReadouts(); }
        }
    });
    connect(m_fftSizeSlider, &QSlider::valueChanged,
            this, [this](int v) {
        showChosenFftSize(v);
        if (m_fftSizeReleasePending) {
            m_fftSizeReleasePending = false;
            m_fftSizeKeyTimer->stop();
            applyChosenFftSize();
            return;
        }
        // A keyboard step or a click on the groove: wait for the last.
        m_fftSizeKeyTimer->start();
    });

    // Window combo handler.
    connect(m_windowCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        const int clamped = qBound(0,
            i,
            static_cast<int>(WindowFunction::Count) - 1);
        // Display pill plan Task 20 (R-DSP-33): the window's handler sets
        // every stream's window and writes the key.
        if (fftHandlers().applyWindowType) {
            QScopedValueRollback<bool> own(m_applyingOwnFftChange, true);
            fftHandlers().applyWindowType(clamped);
            return;
        }
        if (!model() || !model()->fftEngine()) { return; }
        // A page on its own: every engine through the pool.
        if (FftEnginePool* pool = model()->fftEnginePool()) {
            pool->setWindowType(clamped);
        } else {
            model()->fftEngine()->setWindowFunction(
                static_cast<WindowFunction>(clamped));
        }
        // 2026-05-26 KG4VCF bench fix: persist the window choice so it
        // survives restart.  Previously the combo drove the engine but
        // never wrote to AppSettings, so launch always reverted to the
        // FFTEngine ctor default.
        AppSettings::instance().setValue(
            QLatin1String(ControlRanges::kDisplayFftWindowKey),
            QString::number(clamped));
    });

    addContent(fftGroup);

    // --- Section: Rendering ---
    auto* renderGroup = new QGroupBox(QString::fromLatin1(ControlRanges::kDisplayRenderingGroupTitle), this);
    auto* renderForm  = new QFormLayout(renderGroup);
    renderForm->setSpacing(6);

    {
        auto row = makeSliderRow(ControlRanges::kDisplaySpectrumFpsMin,
                                 ControlRanges::kDisplaySpectrumFpsMax,
                                 ControlRanges::kDisplaySpectrumFpsDefault,
                                 QStringLiteral(" ") + QString::fromLatin1(ControlRanges::kDisplaySpectrumFpsUnit),
                                 renderGroup);
        m_fpSlider = row.slider;
        m_fpSlider->setProperty("nereusSetupId", "display.spectrumDefaults.fps");
        m_fpSpin   = row.spin;
        // Thetis: setup.designer.cs:33856 (udDisplayFPS) — Thetis original: "Frames Per Second (approximate)" (placeholder); rewritten
        m_fpSlider->setToolTip(QStringLiteral("Spectrum/waterfall redraw rate. Higher = smoother animation at cost of CPU."));
        row.spin->setToolTip(QStringLiteral("Spectrum/waterfall redraw rate. Higher = smoother animation at cost of CPU."));
        connect(m_fpSlider, &QSlider::valueChanged, this, [this](int v) { pushFps(v); });
        renderForm->addRow(QString::fromLatin1(ControlRanges::kDisplaySpectrumFpsLabel)
                               + QLatin1Char(':'), row.container);
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
    // Items from ControlRanges::kDisplaySpectrumDetectors.
    m_spectrumDetectorCombo = new QComboBox(renderGroup);
    m_spectrumDetectorCombo->setProperty("nereusSetupId", "display.spectrumDefaults.detector");
    for (const ControlRanges::DisplayChoiceItem& item : ControlRanges::kDisplaySpectrumDetectors) {
        m_spectrumDetectorCombo->addItem(QLatin1String(item.label));
    }
    // Thetis: setup.designer.cs:34876 (comboDispPanDetector) [v2.10.3.13] — no upstream tooltip; rewritten
    m_spectrumDetectorCombo->setToolTip(QStringLiteral(
        "Spectrum bin-reduction policy. Peak takes the maximum bin in each display pixel. "
        "Rosenfell alternates max/min for a classic spectrum look. Average takes the "
        "arithmetic mean. Sample takes the first bin. RMS computes root-mean-square power."));
    bindDefaultCombo(m_spectrumDetectorCombo, QStringLiteral("DisplaySpectrumDetector"),
                     [](SpectrumWidget* pan, int i) {
        pan->setSpectrumDetector(static_cast<SpectrumDetector>(i));
    });
    connect(m_spectrumDetectorCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        // Normalize applies only to Average, Sample and RMS; Thetis
        // disables its checkbox the same way (setup.cs:18081-18082
        // [v2.10.3.15], under its //[2.10.3.5]MW0LGE note about
        // updateNormalizePan).
        if (m_dispNormalizeToggle) {
            m_dispNormalizeToggle->setEnabled(NereusSDR::normalizeAppliesToDetector(i));
        }
    });
    renderForm->addRow(QString::fromLatin1(ControlRanges::kDisplaySpectrumDetectorLabel)
                           + QLatin1Char(':'), m_spectrumDetectorCombo);

    // Ported from Thetis comboDispPanAveraging [v2.10.3.13]
    // (setup.designer.cs:34835): None / Recursive / Time Window / Log Recursive.
    // Items from ControlRanges::kDisplayAveragingModes.
    m_spectrumAveragingCombo = new QComboBox(renderGroup);
    m_spectrumAveragingCombo->setProperty("nereusSetupId", "display.spectrumDefaults.averaging");
    for (const ControlRanges::DisplayChoiceItem& item : ControlRanges::kDisplayAveragingModes) {
        m_spectrumAveragingCombo->addItem(QLatin1String(item.label));
    }
    // Thetis: setup.designer.cs:34835 (comboDispPanAveraging) [v2.10.3.13] — no upstream tooltip; rewritten
    m_spectrumAveragingCombo->setToolTip(QStringLiteral(
        "Spectrum frame-averaging mode. None shows raw FFT output per frame. "
        "Recursive exponentially smooths in linear power space. "
        "Time Window approximates a sliding average. "
        "Log Recursive smooths in the dB domain for a more natural look at low levels."));
    bindDefaultCombo(m_spectrumAveragingCombo, QStringLiteral("DisplaySpectrumAveraging"),
                     [](SpectrumWidget* pan, int i) {
        pan->setSpectrumAveraging(static_cast<SpectrumAveraging>(i));
    });
    renderForm->addRow(QString::fromLatin1(ControlRanges::kDisplaySpectrumAveragingLabel)
                           + QLatin1Char(':'), m_spectrumAveragingCombo);

    // Spectrum (panadapter) averaging time constant.
    // From Thetis udDisplayAVGTime [v2.10.3.13] (setup.designer.cs:34902).
    // Default 30 ms matches Thetis, and so do the range and step: 1..9999 ms
    // in steps of 1 (setup.designer.cs:34996-35023 [v2.10.3.15]).
    // Spec widget computes α = exp(-1/(fps×τ)) internally; no hand-rolled
    // ms→alpha math here.
    // Range, step and default from ControlRanges.h.
    m_averagingTimeSpin = new QSpinBox(renderGroup);
    m_averagingTimeSpin->setProperty("nereusSetupId", "display.spectrumDefaults.averageTime");
    m_averagingTimeSpin->setRange(ControlRanges::kDisplayAvgTimeMinMs,
                                  ControlRanges::kDisplayAvgTimeMaxMs);
    m_averagingTimeSpin->setSingleStep(ControlRanges::kDisplayAvgTimeStepMs);
    m_averagingTimeSpin->setSuffix(QStringLiteral(" ms"));
    m_averagingTimeSpin->setValue(ControlRanges::kDisplaySpectrumAvgTimeDefaultMs);
    // The alpha formula is Thetis's.
    m_averagingTimeSpin->setToolTip(QStringLiteral(
        "Spectrum averaging time constant. Larger = heavier smoothing, "
        "slower response. Translates to a frame-by-frame alpha via "
        "α = exp(−1 / (fps × τ))."));
    bindDefaultSpin(m_averagingTimeSpin, QStringLiteral("DisplaySpectrumAverageTimeMs"),
                    [](SpectrumWidget* pan, int ms) { pan->setSpectrumAverageTimeMs(ms); });
    renderForm->addRow(QString::fromLatin1(ControlRanges::kDisplaySpectrumAvgTimeLabel)
                           + QLatin1Char(':'), m_averagingTimeSpin);

    m_decimationSpin = new QSpinBox(renderGroup);
    m_decimationSpin->setProperty("nereusSetupId", "display.spectrumDefaults.decimation");
    m_decimationSpin->setRange(ControlRanges::kDisplayDecimationMin,
                               ControlRanges::kDisplayDecimationMax);
    m_decimationSpin->setSingleStep(ControlRanges::kDisplayDecimationStep);
    m_decimationSpin->setValue(ControlRanges::kDisplayDecimationDefault);
    // Thetis: setup.designer.cs:33732 (udDisplayDecimation) [v2.10.3.13]
    // Thetis original: "Display decimation. Higher the number, the lower the resolution."
    m_decimationSpin->setToolTip(QStringLiteral("Display decimation. Higher the number, the lower the resolution."));
    // Task 2.3: wire to FFTEngine::setDecimation().
    // Parity Task 17 follow-up (R-R3-01): every pan's engine, through the
    // pool, as a remote window sends it for every pan.
    connect(m_decimationSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        // Display pill plan Task 20: the window's handler, as the Display
        // pill's Decimation row uses it.
        if (fftHandlers().applyDecimation) {
            QScopedValueRollback<bool> own(m_applyingOwnFftChange, true);
            fftHandlers().applyDecimation(v);
            refreshFftReadoutLine();
            return;
        }
        if (!model()) { return; }
        if (FftEnginePool* pool = model()->fftEnginePool()) {
            pool->setDecimation(v);
        } else if (model()->fftEngine()) {
            model()->fftEngine()->setDecimation(v);
        }
        refreshFftReadoutLine();
    });
    renderForm->addRow(QString::fromLatin1(ControlRanges::kDisplayDecimationLabel)
                           + QLatin1Char(':'), m_decimationSpin);

    m_fillToggle = new QCheckBox(QStringLiteral("Fill"), renderGroup);
    m_fillToggle->setProperty("nereusSetupId", "display.spectrumDefaults.panFill");
    // 3D Stacked-Trace Spectrum Plan Task 21 test seam: see the matching
    // comment on WaterfallDefaultsPage's m_colorSchemeCombo above.
    m_fillToggle->setObjectName(QStringLiteral("specFillToggle"));
    // Thetis: setup.designer.cs:33749 (chkDisplayPanFill)
    m_fillToggle->setToolTip(QStringLiteral("Check to fill the panadapter display line below the data."));
    bindDefaultToggle(m_fillToggle, QStringLiteral("DisplayPanFill"),
                      [](SpectrumWidget* pan, bool on) {
        pan->displaySettings()->setPanFill(on);
    });
    renderForm->addRow(QString(), m_fillToggle);

    {
        auto row = makeSliderRow(0, 100, 70, QStringLiteral("%"), renderGroup);
        m_fillAlphaSlider = row.slider;
        m_fillAlphaSlider->setProperty("nereusSetupId", "display.spectrumDefaults.fillAlpha");
        // 3D Stacked-Trace Spectrum Plan Task 21 test seam: see the
        // matching comment on WaterfallDefaultsPage's m_colorSchemeCombo
        // above.
        m_fillAlphaSlider->setObjectName(QStringLiteral("specFillAlphaSlider"));
        // Thetis: setup.designer.cs:3215 (tbDataFillAlpha) — no upstream tooltip; rewritten
        // Thetis original: (none)
        m_fillAlphaSlider->setToolTip(QStringLiteral("Opacity of the fill area under the spectrum trace (0 = transparent, 100 = opaque)."));
        row.spin->setToolTip(QStringLiteral("Opacity of the fill area under the spectrum trace (0 = transparent, 100 = opaque)."));
        // The key holds 0 to 1; the slider counts percent.
        connect(m_fillAlphaSlider, &QSlider::valueChanged, this, [this](int v) {
            applyDisplayDefault(QStringLiteral("DisplayFftFillAlpha"),
                                defaultFloatText(v / 100.0),
                                [v](SpectrumWidget* pan) {
                pan->displaySettings()->setFillAlpha(v / 100.0f);
            });
        });
        QSpinBox* alphaSpin = row.spin;
        followDisplayDefault(QStringLiteral("DisplayFftFillAlpha"), [this, alphaSpin]() {
            bool ok = false;
            const double alpha =
                displayDefaultText(QStringLiteral("DisplayFftFillAlpha")).toDouble(&ok);
            if (!ok) { return; }
            QSignalBlocker blockSlider(m_fillAlphaSlider);
            QSignalBlocker blockSpin(alphaSpin);
            m_fillAlphaSlider->setValue(qRound(alpha * 100.0));
            alphaSpin->setValue(m_fillAlphaSlider->value());
        });
        renderForm->addRow(QStringLiteral("Fill Alpha:"), row.container);
    }

    {
        // Display pill plan Task 21 (S12; R-DSP-57), widened by the
        // maintainer's ruling of 2026-10-10: 0.1 to 5 px in 0.1 px steps, as
        // the Display pill's Line row has it. The row was 1 to 3 in whole
        // pixels and loaded the 1.5 px ship default as 1. The slider counts
        // tenths of a pixel; the number cell shows pixels.
        auto* row = new QWidget(renderGroup);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
        m_lineWidthSlider = new QSlider(Qt::Horizontal, row);
        m_lineWidthSlider->setObjectName(QStringLiteral("specLineWidthSlider"));
        m_lineWidthSlider->setProperty("nereusSetupId", "display.spectrumDefaults.lineWidth");
        m_lineWidthSlider->setRange(kLineWidthMinTenths, kLineWidthMaxTenths);
        m_lineWidthSlider->setSingleStep(1);
        m_lineWidthSlider->setPageStep(kLineWidthTenthsPerPx / 2);
        auto* lineSpin = new QDoubleSpinBox(row);
        lineSpin->setObjectName(QStringLiteral("specLineWidthSpin"));
        lineSpin->setDecimals(1);
        lineSpin->setRange(ControlRanges::displayLineWidthPx(kLineWidthMinTenths),
                           ControlRanges::displayLineWidthPx(kLineWidthMaxTenths));
        lineSpin->setSingleStep(1.0 / kLineWidthTenthsPerPx);
        lineSpin->setSuffix(QStringLiteral(" px"));
        lineSpin->setFixedWidth(80);
        lineSpin->setAlignment(Qt::AlignRight);
        rowLayout->addWidget(m_lineWidthSlider, 1);
        rowLayout->addWidget(lineSpin, 0);
        // Thetis: setup.designer.cs:3228 (udDisplayLineWidth) — no upstream tooltip; rewritten
        // Thetis original: (none)
        const QString lineTip =
            QStringLiteral("Spectrum trace line width in pixels (0.1 to 5 px).");
        m_lineWidthSlider->setToolTip(lineTip);
        lineSpin->setToolTip(lineTip);
        const auto showLineWidth = [this, lineSpin](double px) {
            QSignalBlocker blockSlider(m_lineWidthSlider);
            QSignalBlocker blockSpin(lineSpin);
            m_lineWidthSlider->setValue(ControlRanges::displayLineWidthTenths(px));
            lineSpin->setValue(ControlRanges::displayLineWidthPx(m_lineWidthSlider->value()));
        };
        showLineWidth(ControlRanges::kDisplayLineWidthDefaultPx);
        connect(m_lineWidthSlider, &QSlider::valueChanged, this, [this, lineSpin](int tenths) {
            const double px = ControlRanges::displayLineWidthPx(tenths);
            {
                QSignalBlocker blockSpin(lineSpin);
                lineSpin->setValue(px);
            }
            applyDisplayDefault(QStringLiteral("DisplayLineWidth"), defaultFloatText(px),
                                [px](SpectrumWidget* pan) {
                pan->setLineWidth(static_cast<float>(px));
            });
        });
        // A typed width goes to the nearest tenth of a pixel, through the
        // slider.
        connect(lineSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [this, showLineWidth](double px) {
            const int tenths = ControlRanges::displayLineWidthTenths(px);
            if (tenths == m_lineWidthSlider->value()) {
                showLineWidth(ControlRanges::displayLineWidthPx(tenths));
            } else {
                m_lineWidthSlider->setValue(tenths);
            }
        });
        followDisplayDefault(QStringLiteral("DisplayLineWidth"), [this, showLineWidth]() {
            bool ok = false;
            const double px =
                displayDefaultText(QStringLiteral("DisplayLineWidth")).toDouble(&ok);
            if (ok) { showLineWidth(px); }
        });
        renderForm->addRow(QStringLiteral("Line Width:"), row);
    }

    m_gradientToggle = new QCheckBox(QStringLiteral("Gradient"), renderGroup);
    m_gradientToggle->setProperty("nereusSetupId", "display.spectrumDefaults.gradient");
    // Thetis: setup.designer.cs:53918 (chkDataLineGradient) — rewritten (grammar fix)
    // Thetis original: "The data line is also uses the gradient if checked"
    m_gradientToggle->setToolTip(QStringLiteral("When checked, the spectrum trace line renders with the gradient color applied."));
    bindDefaultToggle(m_gradientToggle, QStringLiteral("DisplayGradientEnabled"),
                      [](SpectrumWidget* pan, bool on) { pan->setGradientEnabled(on); });
    renderForm->addRow(QString(), m_gradientToggle);

    addContent(renderGroup);

    // --- Section: Calibration ---
    auto* calGroup = new QGroupBox(QStringLiteral("Calibration && Peak Hold"), this);
    auto* calForm  = new QFormLayout(calGroup);
    calForm->setSpacing(6);

    m_calOffsetSpin = new QDoubleSpinBox(calGroup);
    m_calOffsetSpin->setProperty("nereusSetupId", "display.spectrumDefaults.calOffset");
    m_calOffsetSpin->setRange(-30.0, 30.0);
    m_calOffsetSpin->setSingleStep(0.5);
    m_calOffsetSpin->setSuffix(QStringLiteral(" dBm"));
    m_calOffsetSpin->setValue(0.0);
    // Thetis: display.cs:1372 (Display.RX1DisplayCalOffset) — programmatic only, no designer tooltip; rewritten
    // Thetis original: (none — set programmatically via Display.RX1DisplayCalOffset)
    m_calOffsetSpin->setToolTip(QStringLiteral("dBm calibration offset applied to all displayed signal levels. Use to match a calibrated reference."));
    connect(m_calOffsetSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        applyDisplayDefault(QStringLiteral("DisplayCalOffset"), defaultFloatText(v),
                            [v](SpectrumWidget* pan) {
            pan->setDbmCalOffset(static_cast<float>(v));
        });
    });
    followDisplayDefault(QStringLiteral("DisplayCalOffset"), [this]() {
        bool ok = false;
        const double offset =
            displayDefaultText(QStringLiteral("DisplayCalOffset")).toDouble(&ok);
        if (!ok) { return; }
        QSignalBlocker block(m_calOffsetSpin);
        m_calOffsetSpin->setValue(offset);
    });
    calForm->addRow(QStringLiteral("Cal Offset:"), m_calOffsetSpin);
    // Parity Task 17 (R-R3-11): a remote pan's levels already carry the
    // Core's calibration for its radio; a second local offset would count
    // it twice.
    if (remoteWindow()) {
        const QString reason = QStringLiteral("The Core calibrates the display for its radio.");
        m_calOffsetSpin->setEnabled(false);
        m_calOffsetSpin->setToolTip(reason);
        m_calOffsetSpin->setAccessibleDescription(reason);
    }

    m_peakHoldToggle = new QCheckBox(QStringLiteral("Peak Hold"), calGroup);
    m_peakHoldToggle->setProperty("nereusSetupId", "display.spectrumDefaults.peakHold");
    // NereusSDR extension — no Thetis equivalent
    m_peakHoldToggle->setToolTip(QStringLiteral("When enabled, the highest signal level seen at each frequency bin is held on the display."));
    bindDefaultToggle(m_peakHoldToggle, QStringLiteral("DisplayPeakHoldEnabled"),
                      [](SpectrumWidget* pan, bool on) { pan->setPeakHoldEnabled(on); });
    calForm->addRow(QString(), m_peakHoldToggle);

    m_peakHoldDelaySpin = new QSpinBox(calGroup);
    m_peakHoldDelaySpin->setProperty("nereusSetupId", "display.spectrumDefaults.peakDelay");
    m_peakHoldDelaySpin->setRange(100, 10000);
    m_peakHoldDelaySpin->setSingleStep(100);
    m_peakHoldDelaySpin->setSuffix(QStringLiteral(" ms"));
    m_peakHoldDelaySpin->setValue(2000);
    // NereusSDR extension — no Thetis equivalent
    m_peakHoldDelaySpin->setToolTip(QStringLiteral("Time in milliseconds before a held peak begins to decay back toward the live trace."));
    bindDefaultSpin(m_peakHoldDelaySpin, QStringLiteral("DisplayPeakHoldResetMs"),
                    [](SpectrumWidget* pan, int v) { pan->setPeakHoldDelayMs(v); });
    calForm->addRow(QStringLiteral("Peak Delay:"), m_peakHoldDelaySpin);

    addContent(calGroup);

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
        QStringLiteral("Cursor Freq"), overlayGroup);
    m_showMHzOnCursorToggle->setProperty("nereusSetupId", "display.spectrumDefaults.showCursorFreq");
    m_showMHzOnCursorToggle->setToolTip(QStringLiteral(
        "Display the frequency at the cursor position (always in MHz). "
        "Same toggle as the on-spectrum overlay-panel Cursor Freq button."));
    // Display pill plan Task 21: the bare DisplayShowCursorFreq key this
    // wrote directly is the Setup default, written by applyDisplayDefault
    // like every other per-pan row.
    bindDefaultToggle(m_showMHzOnCursorToggle, QStringLiteral("DisplayShowCursorFreq"),
                      [](SpectrumWidget* pan, bool on) { pan->setCursorFreqVisible(on); });
    overlayForm->addRow(QString(), m_showMHzOnCursorToggle);

    // ShowBinWidth — live bin-width readout.
    // From Thetis setup.cs:7061 [v2.10.3.13] lblDisplayBinWidth.
    m_showBinWidthToggle = new QCheckBox(
        QStringLiteral("Bin Width"), overlayGroup);
    m_showBinWidthToggle->setProperty("nereusSetupId", "display.spectrumDefaults.showBinWidth");
    m_showBinWidthToggle->setToolTip(QStringLiteral(
        "Display the current FFT bin width (sample rate / FFT size) in the spectrum corner."));
    m_binWidthReadout = new QLabel(QStringLiteral("– Hz/bin"), overlayGroup);
    m_binWidthReadout->setToolTip(QStringLiteral("Current FFT bin width in Hz."));
    bindDefaultToggle(m_showBinWidthToggle, QStringLiteral("DisplayShowBinWidth"),
        [](SpectrumWidget* pan, bool on) { pan->setShowBinWidth(on); });
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
        QStringLiteral("Noise Floor"), overlayGroup);
    m_showNoiseFloorToggle->setProperty("nereusSetupId", "display.spectrumDefaults.showNoiseFloor");
    m_showNoiseFloorToggle->setToolTip(QStringLiteral(
        "Display the noise floor as a horizontal dashed line + dBm box+text."));
    m_noiseFloorPositionCombo = makeOverlayPositionCombo(overlayGroup);
    m_noiseFloorPositionCombo->setProperty("nereusSetupId", "display.spectrumDefaults.noiseFloorPosition");
    m_noiseFloorPositionCombo->setCurrentIndex(2);  // Bottom Left default
    m_noiseFloorPositionCombo->setEnabled(false);
    // Deprecated: the text anchors to the NF box as Thetis draws it
    // (display.cs:5443). The source reference used to be in the tooltip.
    m_noiseFloorPositionCombo->setToolTip(QStringLiteral(
        "Not used: the noise floor text sits beside its box. "
        "The setting is kept but has no visual effect."));
    bindDefaultToggle(m_showNoiseFloorToggle, QStringLiteral("DisplayShowNoiseFloor"),
        [](SpectrumWidget* pan, bool on) { pan->setShowNoiseFloor(on); });
    bindDefaultCombo(m_noiseFloorPositionCombo, QStringLiteral("DisplayShowNoiseFloorPosition"),
        [](SpectrumWidget* pan, int value) { pan->setShowNoiseFloorPosition(static_cast<SpectrumWidget::OverlayPosition>(value)); });
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
        m_nfShiftSpin->setProperty("nereusSetupId", "display.spectrumDefaults.noiseFloorShift");
        // Thetis _fNFshiftDBM, clamped to [-12, +12] (was in the tooltip).
        m_nfShiftSpin->setToolTip(QStringLiteral(
            "Operator-tunable offset added to the rendered NF level, "
            "from -12 to +12 dB."));
        connect(m_nfShiftSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
            applyDisplayDefault(QStringLiteral("DisplayNoiseFloorShiftDb"), QString::number(value),
                [value](SpectrumWidget* pan) { pan->setNFShiftDbm(static_cast<float>(value)); });
        });
        followDisplayDefault(QStringLiteral("DisplayNoiseFloorShiftDb"), [this]() {
            const QSignalBlocker block(m_nfShiftSpin);
            m_nfShiftSpin->setValue(displayDefaultText(QStringLiteral("DisplayNoiseFloorShiftDb")).toDouble());
        });

        m_nfLineWidthSpin = new QDoubleSpinBox(overlayGroup);
        m_nfLineWidthSpin->setRange(1.0, 5.0);
        m_nfLineWidthSpin->setSingleStep(0.5);
        m_nfLineWidthSpin->setDecimals(1);
        m_nfLineWidthSpin->setSuffix(QStringLiteral(" px"));
        m_nfLineWidthSpin->setProperty("nereusSetupId", "display.spectrumDefaults.noiseFloorLineWidth");
        // Thetis m_fNoiseFloorLineWidth, default 1.0 (was in the tooltip).
        m_nfLineWidthSpin->setToolTip(QStringLiteral(
            "Width of the horizontal NF dashed line."));
        connect(m_nfLineWidthSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
            applyDisplayDefault(QStringLiteral("DisplayNoiseFloorLineWidth"), QString::number(value),
                [value](SpectrumWidget* pan) { pan->setNoiseFloorLineWidth(static_cast<float>(value)); });
        });
        followDisplayDefault(QStringLiteral("DisplayNoiseFloorLineWidth"), [this]() {
            const QSignalBlocker block(m_nfLineWidthSpin);
            m_nfLineWidthSpin->setValue(displayDefaultText(QStringLiteral("DisplayNoiseFloorLineWidth")).toDouble());
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
        // The renderer's default is magenta (SpectrumWidget m_noiseFloorColor);
        // the swatch starts there, and the tooltip no longer says red.
        m_nfLineColorBtn = new ColorSwatchButton(QColor(0xFF, 0x40, 0xFF), overlayGroup);
        m_nfLineColorBtn->setProperty("nereusSetupId", "display.spectrumDefaults.noiseFloorColor");
        m_nfLineColorBtn->setToolTip(QStringLiteral(
            "Color for the NF line + 8x8 box."));
        // HexArgb keeps NF colour storage in the same format as the
        // rest of SpectrumWidget's persisted colours.
        connect(m_nfLineColorBtn, &ColorSwatchButton::colorChanged, this, [this](const QColor& color) {
            applyDisplayDefault(QStringLiteral("DisplayNoiseFloorColor"), color.name(QColor::HexArgb),
                [color](SpectrumWidget* pan) { pan->setNoiseFloorColor(color); });
        });
        followDisplayDefault(QStringLiteral("DisplayNoiseFloorColor"), [this]() {
            const QColor color = QColor::fromString(displayDefaultText(QStringLiteral("DisplayNoiseFloorColor")));
            if (!color.isValid()) { return; }
            const QSignalBlocker block(m_nfLineColorBtn);
            m_nfLineColorBtn->setColor(color);
        });

        m_nfTextColorBtn = new ColorSwatchButton(Qt::yellow, overlayGroup);
        m_nfTextColorBtn->setProperty("nereusSetupId", "display.spectrumDefaults.noiseFloorTextColor");
        m_nfTextColorBtn->setToolTip(QStringLiteral(
            "Color for the NF dBm label text."));
        connect(m_nfTextColorBtn, &ColorSwatchButton::colorChanged, this, [this](const QColor& color) {
            applyDisplayDefault(QStringLiteral("DisplayNoiseFloorTextColor"), color.name(QColor::HexArgb),
                [color](SpectrumWidget* pan) { pan->setNoiseFloorTextColor(color); });
        });
        followDisplayDefault(QStringLiteral("DisplayNoiseFloorTextColor"), [this]() {
            const QColor color = QColor::fromString(displayDefaultText(QStringLiteral("DisplayNoiseFloorTextColor")));
            if (!color.isValid()) { return; }
            const QSignalBlocker block(m_nfTextColorBtn);
            m_nfTextColorBtn->setColor(color);
        });

        // The renderer's default (SpectrumWidget m_noiseFloorFastColor).
        m_nfFastColorBtn = new ColorSwatchButton(QColor(0xC8, 0xC8, 0xC8), overlayGroup);
        m_nfFastColorBtn->setProperty("nereusSetupId", "display.spectrumDefaults.noiseFloorFastColor");
        // Default gray, mirroring Thetis m_bDX2_Gray (was in the tooltip).
        m_nfFastColorBtn->setToolTip(QStringLiteral(
            "Color shown during fast-attack (band/freq/MOX change)."));
        connect(m_nfFastColorBtn, &ColorSwatchButton::colorChanged, this, [this](const QColor& color) {
            applyDisplayDefault(QStringLiteral("DisplayNoiseFloorFastColor"), color.name(QColor::HexArgb),
                [color](SpectrumWidget* pan) { pan->setNoiseFloorFastColor(color); });
        });
        followDisplayDefault(QStringLiteral("DisplayNoiseFloorFastColor"), [this]() {
            const QColor color = QColor::fromString(displayDefaultText(QStringLiteral("DisplayNoiseFloorFastColor")));
            if (!color.isValid()) { return; }
            const QSignalBlocker block(m_nfFastColorBtn);
            m_nfFastColorBtn->setColor(color);
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
        QStringLiteral("Normalize to 1 Hz"), overlayGroup);
    m_dispNormalizeToggle->setProperty("nereusSetupId", "display.spectrumDefaults.normalize");
    m_dispNormalizeToggle->setToolTip(QStringLiteral(
        "Normalize the spectrum trace to a 1 Hz reference bandwidth. "
        "Only active for Average, Sample, or RMS detector modes."));
    bindDefaultToggle(m_dispNormalizeToggle, QStringLiteral("DisplayDispNormalize"),
        [](SpectrumWidget* pan, bool on) { pan->setDispNormalize(on); });
    overlayForm->addRow(QString(), m_dispNormalizeToggle);

    // ShowPeakValueOverlay + position + delay.
    // From Thetis console.cs:20073-20080 [v2.10.3.13] PeakTextDelay default=500ms.
    // Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
    // Color default DodgerBlue from console.cs:20278 [v2.10.3.13].
    m_showPeakValueOverlayToggle = new QCheckBox(
        QStringLiteral("Peak Value"), overlayGroup);
    m_showPeakValueOverlayToggle->setProperty("nereusSetupId", "display.spectrumDefaults.showPeakValue");
    m_showPeakValueOverlayToggle->setToolTip(QStringLiteral(
        "Display the peak signal level and frequency as a text overlay in the spectrum corner."));
    m_peakValuePositionCombo = makeOverlayPositionCombo(overlayGroup);
    m_peakValuePositionCombo->setCurrentIndex(1);  // Top Right default
    m_peakValuePositionCombo->setProperty("nereusSetupId", "display.spectrumDefaults.peakValuePosition");
    // The row has no visible label; the accessible name is the one the
    // Setup description publishes.
    m_peakValuePositionCombo->setAccessibleName(QStringLiteral("Peak value position:"));
    m_peakValuePositionCombo->setToolTip(QStringLiteral("Corner position for the peak value readout."));
    m_peakTextDelaySpin = new QSpinBox(overlayGroup);
    m_peakTextDelaySpin->setRange(50, 10000);
    m_peakTextDelaySpin->setSingleStep(50);
    m_peakTextDelaySpin->setSuffix(QStringLiteral(" ms"));
    // From Thetis console.cs:20073 [v2.10.3.13]: peak_text_delay = 500.
    // Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
    m_peakTextDelaySpin->setValue(500);
    m_peakTextDelaySpin->setProperty("nereusSetupId", "display.spectrumDefaults.peakTextDelay");
    m_peakTextDelaySpin->setAccessibleName(QStringLiteral("Peak value refresh:"));
    m_peakTextDelaySpin->setToolTip(QStringLiteral(
        "Refresh interval for the peak value overlay in milliseconds."));
    bindDefaultToggle(m_showPeakValueOverlayToggle, QStringLiteral("DisplayShowPeakValueOverlay"),
        [](SpectrumWidget* pan, bool on) { pan->setShowPeakValueOverlay(on); });
    bindDefaultCombo(m_peakValuePositionCombo, QStringLiteral("DisplayPeakValuePosition"),
        [](SpectrumWidget* pan, int value) { pan->setPeakValuePosition(static_cast<SpectrumWidget::OverlayPosition>(value)); });
    bindDefaultSpin(m_peakTextDelaySpin, QStringLiteral("DisplayPeakTextDelayMs"),
        [](SpectrumWidget* pan, int value) { pan->setPeakTextDelayMs(value); });
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
    monitorHzBtn->setProperty("nereusSetupId", "display.spectrumDefaults.getMonitorHz");
    monitorHzBtn->setToolTip(QStringLiteral(
        "Query this window's screen refresh rate and snap the FPS slider to the nearest valid value."));
    connect(monitorHzBtn, &QPushButton::clicked, this, [this]() {
        // From Thetis setup.cs:32208 btnGetMonitorHz_Click [v2.10.3.13]:
        //   udDisplayFPS.Value = (decimal)Display.GetCurrentMonitorRefreshRate(console);
        // Qt equivalent: QScreen::refreshRate() on the primary screen.
        if (QScreen* currentScreen = this->screen()) {
            const int hz = qBound(10, static_cast<int>(std::round(currentScreen->refreshRate())), 60);
            if (m_fpSlider) {
                m_fpSlider->setValue(hz);
                // pushFps is connected to m_fpSlider::valueChanged so it fires automatically.
            }
        }
    });
    overlayForm->addRow(QString(), monitorHzBtn);

    addContent(overlayGroup);

    // --- Section: Thread ---
    auto* threadGroup = new QGroupBox(QStringLiteral("Thread"), this);
    auto* threadForm  = new QFormLayout(threadGroup);
    threadForm->setSpacing(6);

    m_threadPriorityCombo = new QComboBox(threadGroup);
    m_threadPriorityCombo->setProperty("nereusSetupId", "display.spectrumDefaults.threadPriority");
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
        // Display pill plan Task 20 (R-DSP-33): every stream's FFT thread,
        // and the threads of streams made later, through the pool.
        if (FftEnginePool* pool = model() ? model()->fftEnginePool() : nullptr) {
            pool->setThreadPriority(pri);
        } else if (model() && model()->fftEngine()) {
            if (auto* t = model()->fftEngine()->thread()) {
                t->setPriority(pri);
            }
        }
    });
    threadForm->addRow(QStringLiteral("Display Thread Priority:"), m_threadPriorityCombo);
    // Parity Task 17 (R-R3-41): a remote pan's spectrum is computed on the
    // Core, which places its own threads.
    if (remoteWindow()) {
        const QString reason = QStringLiteral("The Core sets its own display thread priority.");
        m_threadPriorityCombo->setEnabled(false);
        m_threadPriorityCombo->setToolTip(reason);
        m_threadPriorityCombo->setAccessibleDescription(reason);
    }
    // Parity Task 17 (R-R3-01, R-R3-08): the readouts follow the Core's
    // grant for the active pan (a new FFT size from the slider or a zoom).
    if (remoteWindow()) {
        if (auto* grantSource = model()->spectrumWidget()) {
            connect(grantSource, &SpectrumWidget::remoteSpectrumGrantChanged,
                    this, &SpectrumDefaultsPage::refreshGrantedReadouts);
            connect(grantSource, &SpectrumWidget::remoteSpectrumGrantChanged,
                    this, &SpectrumDefaultsPage::refreshFftReadoutLine);
            // FFT sizing mode (design D38): outside Fixed the grayed size
            // slider shows the granted size too.
            connect(grantSource, &SpectrumWidget::remoteSpectrumGrantChanged,
                    this, &SpectrumDefaultsPage::refreshFftSizingControls);
        }
    }
    // Display pill plan Task 20 (R-DSP-61): a zoom replans the running
    // size; the line under the slider follows once the window has planned.
    if (auto* zoomSource = model() ? model()->spectrumWidget() : nullptr) {
        connect(zoomSource, &SpectrumWidget::bandwidthChangeRequested, this,
                [this]() { refreshFftSizingControls(); }, Qt::QueuedConnection);
    }

    addContent(threadGroup);

    // Colour pickers for trace, grid, passband, and zero lines have moved to
    // Setup → Appearance → Colors & Theme (consolidated in one place).
    auto* colorHint = new QLabel(QStringLiteral(
        "Spectrum / waterfall colors: Setup → Appearance → Colors & Theme."), this);
    colorHint->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; padding: 6px; }"));
    addContent(colorHint);

    // ── Cross-links (Task 2.4) ────────────────────────────────────────────────
    // Hint lines for forward-looking moves documented in the design (Task 3.6 / 3.4).
    auto* hintVolts = new QLabel(
        QStringLiteral("ANAN-8000DLE volts/amps moved to Hardware → ANAN-8000DLE."), this);
    hintVolts->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; font-size: 10px; }"));
    addContent(hintVolts);

    auto* hintFilter = new QLabel(
        QStringLiteral("Small filter on VFOs moved to Appearance → VFO Flag."), this);
    hintFilter->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; font-size: 10px; }"));
    addContent(hintFilter);

    const QString crossLinkStyle = QStringLiteral(
        "QPushButton { background: #1a2a3a; color: #8aa8c0; border: 1px solid #203040;"
        "  border-radius: 3px; padding: 4px 10px; }"
        "QPushButton:hover { background: #203040; color: #c8d8e8; }");

    auto* crossLinkRow = new QWidget(this);
    crossLinkRow->setProperty("nereusSetupId", "display.spectrum.crossLinks");
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
        "Open Display → Multimeter to configure the on-screen level meter."));
    m_configureMultimeterBtn->setStyleSheet(crossLinkStyle);
    // Multimeter page lands in Task 3.1; signal is wired in SetupDialog at that time.
    // The button is defined here so SetupDialog can connect it without touching this file again.
    connect(m_configureMultimeterBtn, &QPushButton::clicked,
            this, &SpectrumDefaultsPage::navigateToMultimeterRequested);
    crossLinkLayout->addWidget(m_configureMultimeterBtn);

    crossLinkLayout->addStretch();
    addContent(crossLinkRow);

    Q_UNUSED(sw);
    Q_UNUSED(fe);
}

// ---------------------------------------------------------------------------
// WaterfallDefaultsPage
// ---------------------------------------------------------------------------

WaterfallDefaultsPage::WaterfallDefaultsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QString::fromLatin1(ControlRanges::kDisplayWaterfallPageTitle), model, parent)
{
    buildUI();
    loadFromRenderer();
    // Display pill plan Task 21: Clarity is asked of the active pan, so the
    // level rows are looked at again when another pan becomes the active one.
    if (model) {
        connect(model, &RadioModel::spectrumWidgetChanged, this,
                [this](SpectrumWidget*) { updateLevelOwner(); });
    }
}

void WaterfallDefaultsPage::loadFromRenderer()
{
    // Display pill plan Task 21 (D26): every control on this page shows the
    // Setup default (the bare key, else the ship default), not the active
    // pan's own value. Each control's follower (bindDefault* in buildUI)
    // reads it.
    showDisplayDefaults();
    updateLevelOwner();
    updateEffectiveDepthLabel();
    updateDelayLabel();
}

void WaterfallDefaultsPage::displayPansChanged()
{
    updateLevelOwner();
}

// Display pill plan Task 21 (D13, S11; R-DSP-18): who sets the waterfall's
// levels, in the Display pill's order and words. While Clarity drives the
// active pan's levels, Low, High, AGC, NF-AGC and the NF offset are greyed;
// so are they while "Use spectrum min/max" is on; NF-AGC and its offset are
// also greyed while AGC is off. Greyed, never hidden, with the reason as the
// tooltip and in the line under the Levels rows.
//
// NereusSDR, based on Thetis: chkWaterfallUseRX1SpectrumMinMax_CheckedChanged
// at setup.cs:19224-19232 [v2.10.3.13] greys the thresholds and AGC for
// "Use spectrum min/max" (issue #230); Clarity is a NereusSDR level source
// Thetis does not have, so it greys the same rows first.
void WaterfallDefaultsPage::updateLevelOwner()
{
    if (!m_highThresholdSlider || !m_wfNfAgcEnable || !m_levelOwnerNote) { return; }
    if (property("displayAssemblyOwner").toBool()) {
        SpectrumWidget* selected = nullptr;
        for (SpectrumWidget* pan : panWidgets()) { if (pan->panIndex() == displayPanIndex()) { selected = pan; break; } }
        const bool clarity = displayDefaultText(QStringLiteral("ClarityEnabled")) == QStringLiteral("True");
        const bool scale = !clarity && displayDefaultText(QStringLiteral("DisplayWfUseSpectrumMinMax")) == QStringLiteral("True");
        const bool agc = !clarity && !scale && displayDefaultText(QStringLiteral("DisplayWfAgc")) == QStringLiteral("True");
        const bool floor = agc && displayDefaultText(QStringLiteral("WaterfallNFAGCEnabled")) == QStringLiteral("True");
        const bool manual = !clarity && !scale && !agc;
        const bool keyed = selected && selected->isMoxOverlayActive();
        const QString keyedReason = QStringLiteral("This pan is transmitting. Change its receive levels when it stops.");
        gateOwnedControls({m_lowThresholdSlider, m_lowThresholdSpin}, manual && !keyed,
            keyed ? keyedReason : scale ? QStringLiteral("Low is the spectrum's dB Min. Pick Manual to set Low here.")
                : QStringLiteral("%1 is setting Low. Pick Manual to set it yourself.").arg(clarity ? QStringLiteral("Clarity") : floor ? QStringLiteral("Floor") : QStringLiteral("Auto")));
        gateOwnedControls({m_highThresholdSlider, m_highThresholdSpin}, !clarity && !scale && !keyed,
            keyed ? keyedReason : scale ? QStringLiteral("High is the spectrum's dB Max. Pick Manual to set High here.")
                : QStringLiteral("Clarity is setting High. Pick Auto, Floor or Manual to set it yourself."));
        gateOwnedControls({m_wfAgcOffsetDb}, floor && !keyed,
            keyed ? keyedReason : QStringLiteral("NF Offset is used by Floor. Pick Floor to use it."));
        const bool runKnown = selected && (selected->coreWaterfallLevelsInUse()
            ? selected->coreWaterfallLevels().has_value() : selected->waterfallAgcRunLevel().has_value());
        const bool known = manual || (selected && ((clarity && selected->clarityActive())
            || (agc && runKnown) || (scale && (!selected->adjustGridMinToNoiseFloor() || selected->acceptedGridFollowFloorDbm().has_value()))));
        for (const bool low : {true, false}) {
            QSlider* slider = low ? m_lowThresholdSlider : m_highThresholdSlider;
            QSpinBox* spin = low ? m_lowThresholdSpin : m_highThresholdSpin;
            const QSignalBlocker sliderBlock(slider);
            const QSignalBlocker spinBlock(spin);
            const QString key = low ? QStringLiteral("DisplayWfLowLevel") : QStringLiteral("DisplayWfHighLevel");
            const bool measured = low ? !manual : clarity || scale;
            spin->setSpecialValueText(QString());
            const int stored = qRound(displayDefaultText(key).toFloat());
            slider->setValue(stored);
            spin->setValue(stored);
            if (measured && !known) {
                spin->setSpecialValueText(displayPanIndex() < 0 ? QStringLiteral("each pan") : QStringLiteral("no signal"));
                spin->setValue(spin->minimum());
            } else if (measured && selected) {
                const auto pair = selected->effectiveWaterfallLevels();
                const int value = qRound(low ? pair.first : pair.second);
                slider->setValue(value);
                spin->setValue(value);
            }
        }
        gateOwnedControls({m_copySpecMinMaxBtn}, manual && !keyed && bandGridModel()
            && (displayDefaultText(QStringLiteral("DisplayAdjustGridMinToNoiseFloor")) != QStringLiteral("True")
                || (selected && selected->acceptedGridFollowFloorDbm().has_value())),
            keyed ? keyedReason : !manual
                ? QStringLiteral("Copy from spectrum scale sets Low and High. Pick Manual for waterfall levels to use it.")
                : QStringLiteral("Copy needs a spectrum scale with known dB Max and dB Min."));
        return;
    }
    SpectrumWidget* active = model() ? model()->spectrumWidget() : nullptr;
    const bool clarity = active && active->clarityActive();
    const bool minMax  = m_useSpectrumMinMaxToggle->isChecked();
    const bool agcOn   = m_agcToggle->isChecked();

    QString reason;
    if (clarity) {
        reason = QString::fromUtf8(kSetupClarityDrivesNote);
    } else if (minMax) {
        reason = QString::fromUtf8(kSetupMinMaxDrivesNote);
    }
    const bool live = reason.isEmpty();
    gateOwnedControls({m_highThresholdSlider, m_highThresholdSpin, m_agcToggle}, live, reason);
    // AGC works the low level out itself, so Low is gray while AGC is on.
    // Clarity's reason and min/max's reason come first. The gray Low keeps
    // showing its stored value.
    // From Thetis setup.cs:16365 [v2.10.3.15]: udDisplayWaterfallLowLevel.Enabled = !chkRX1WaterfallAGC.Checked;
    const bool agcSetsLow = live && agcOn;
    const QString lowReason = agcSetsLow ? QString::fromUtf8(kSetupAgcSetsLowNote) : reason;
    gateOwnedControls({m_lowThresholdSlider, m_lowThresholdSpin}, live && !agcOn, lowReason);
    // NF-AGC is an option under AGC.
    const QString nfReason = live ? QString::fromUtf8(kSetupNfAgcNeedsAgcNote) : reason;
    gateOwnedControls({m_wfNfAgcEnable, m_wfAgcOffsetDb}, live && agcOn, nfReason);
    m_levelOwnerNote->setText(lowReason);
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
        m_highThresholdSlider->setProperty("nereusSetupId", "display.waterfallDefaults.highThreshold");
        // Thetis: setup.designer.cs:34259 (udDisplayWaterfallHighLevel)
        m_highThresholdSlider->setToolTip(QStringLiteral("Waterfall High Signal - Show High Color above this value (gradient in between)."));
        row.spin->setToolTip(QStringLiteral("Waterfall High Signal - Show High Color above this value (gradient in between)."));
        m_highThresholdSpin = row.spin;
        bindDefaultSlider(m_highThresholdSlider, row.spin, QStringLiteral("DisplayWfHighLevel"),
                          [](SpectrumWidget* pan, int v) {
            pan->setWfHighThreshold(static_cast<float>(v));
        });
        levForm->addRow(QStringLiteral("High Threshold:"), row.container);
    }

    {
        auto row = makeSliderRow(-200, 0, -130, QStringLiteral(" dBm"), levGroup);
        m_lowThresholdSlider = row.slider;
        m_lowThresholdSlider->setProperty("nereusSetupId", "display.waterfallDefaults.lowThreshold");
        // Thetis: setup.designer.cs:34219 (udDisplayWaterfallLowLevel)
        m_lowThresholdSlider->setToolTip(QStringLiteral("Waterfall Low Signal - Show Low Color below this value (gradient in between)."));
        row.spin->setToolTip(QStringLiteral("Waterfall Low Signal - Show Low Color below this value (gradient in between)."));
        m_lowThresholdSpin = row.spin;
        bindDefaultSlider(m_lowThresholdSlider, row.spin, QStringLiteral("DisplayWfLowLevel"),
                          [](SpectrumWidget* pan, int v) {
            pan->setWfLowThreshold(static_cast<float>(v));
        });
        levForm->addRow(QStringLiteral("Low Threshold:"), row.container);
    }
    // Follow-up wave F4: the two readouts are as wide as their longest
    // value ("-200 dBm"), which the slider row's 80 px cut off. The width
    // is fixed, so a narrow page takes it from the sliders.
    {
        const int width = std::max({m_highThresholdSpin->width(),
                                    m_highThresholdSpin->sizeHint().width(),
                                    m_lowThresholdSpin->sizeHint().width()});
        m_highThresholdSpin->setFixedWidth(width);
        m_lowThresholdSpin->setFixedWidth(width);
    }

    m_agcToggle = new QCheckBox(QStringLiteral("AGC"), levGroup);
    m_agcToggle->setProperty("nereusSetupId", "display.waterfallDefaults.agc");
    // Thetis: setup.designer.cs:34069 (chkRX1WaterfallAGC)
    m_agcToggle->setToolTip(QStringLiteral("Automatically calculates Low Level Threshold for Waterfall."));
    bindDefaultToggle(m_agcToggle, QStringLiteral("DisplayWfAgc"),
                      [](SpectrumWidget* pan, bool on) { pan->setWfAgcEnabled(on); });
    // NF-AGC is an option under AGC: its rows follow this one.
    connect(m_agcToggle, &QCheckBox::toggled, this, [this](bool) { updateLevelOwner(); });
    levForm->addRow(QString(), m_agcToggle);

    m_useSpectrumMinMaxToggle = new QCheckBox(QStringLiteral("Use spectrum min/max"), levGroup);
    m_useSpectrumMinMaxToggle->setProperty("nereusSetupId", "display.waterfallDefaults.useSpectrumMinMax");
    // Thetis: setup.designer.cs:34054 (chkWaterfallUseRX1SpectrumMinMax)
    m_useSpectrumMinMaxToggle->setToolTip(QStringLiteral("Spectrum Grid min/max used for low and high level"));
    bindDefaultToggle(m_useSpectrumMinMaxToggle, QStringLiteral("DisplayWfUseSpectrumMinMax"),
                      [](SpectrumWidget* pan, bool on) { pan->setWfUseSpectrumMinMax(on); });
    connect(m_useSpectrumMinMaxToggle, &QCheckBox::toggled, this, [this](bool) {
        // Issue #230 fix: mirror Thetis's
        // chkWaterfallUseRX1SpectrumMinMax_CheckedChanged at
        // setup.cs:19224-19232 [v2.10.3.13] — disable the conflicting
        // threshold and AGC controls so the user can't fight the
        // grid-driven values.
        updateLevelOwner();
    });
    levForm->addRow(QString(), m_useSpectrumMinMaxToggle);

    // Display pill plan Task 21: who is setting the levels, when it is not
    // the rows above (updateLevelOwner).
    m_levelOwnerNote = new QLabel(levGroup);
    m_levelOwnerNote->setObjectName(QStringLiteral("wfLevelOwnerNote"));
    m_levelOwnerNote->setWordWrap(true);
    m_levelOwnerNote->setStyleSheet(QStringLiteral("color: #80a0b0;"));
    levForm->addRow(QString(), m_levelOwnerNote);

    // Low Color (W10) moved to Setup → Appearance → Colors & Theme.

    // Task 2.8: Copy spectrum min/max → waterfall thresholds button.
    m_copySpecMinMaxBtn = new QPushButton(
        QStringLiteral("Copy spectrum min/max → waterfall thresholds"), levGroup);
    m_copySpecMinMaxBtn->setProperty("nereusSetupId", "display.waterfallDefaults.copySpectrumMinMax");
    m_copySpecMinMaxBtn->setToolTip(
        QStringLiteral("Copies the current spectrum display dB max and dB min values "
                       "into the waterfall High Threshold and Low Threshold above."));
    connect(m_copySpecMinMaxBtn, &QPushButton::clicked, this, [this]() {
        if (property("displayAssemblyOwner").toBool()) {
            PanadapterModel* grid = bandGridModel();
            if (!grid) { return; }
            SpectrumWidget* selected = nullptr;
            for (SpectrumWidget* pan : panWidgets()) { if (pan->panIndex() == displayPanIndex()) { selected = pan; break; } }
            if (selected && selected->isMoxOverlayActive()) { return; }
            const bool tracking = displayDefaultText(QStringLiteral("DisplayAdjustGridMinToNoiseFloor")) == QStringLiteral("True");
            if (tracking && (!selected || !selected->acceptedGridFollowFloorDbm().has_value())) { return; }
            if (displayDefaultText(QStringLiteral("ClarityEnabled")) == QStringLiteral("True")
                || displayDefaultText(QStringLiteral("DisplayWfUseSpectrumMinMax")) == QStringLiteral("True")
                || displayDefaultText(QStringLiteral("DisplayWfAgc")) == QStringLiteral("True")) { return; }
            const BandGridSettings stored = grid->perBandGrid(grid->band());
            // The accepted-floor getter establishes knownness only. The
            // renderer supplies its effective range, including its clamps.
            const float low = tracking ? selected->refLevel() - selected->dynamicRange() : static_cast<float>(stored.dbMin);
            const float high = tracking ? selected->refLevel() : static_cast<float>(stored.dbMax);
            if (high <= low) { return; }
            applyDisplayDefault(QStringLiteral("DisplayWfLowLevel"), QString::number(low),
                [low](SpectrumWidget* pan) { pan->setWfLowThreshold(low); });
            applyDisplayDefault(QStringLiteral("DisplayWfHighLevel"), QString::number(high),
                [high](SpectrumWidget* pan) { pan->setWfHighThreshold(high); });
            return;
        }
        auto* sw = model() ? model()->spectrumWidget() : nullptr;
        if (!sw) { return; }
        // Spectrum top = refLevel(); spectrum bottom = refLevel() - dynamicRange().
        const float high = sw->refLevel();
        const float low  = sw->refLevel() - sw->dynamicRange();
        // Display pill plan Task 21: the two sliders carry them on as the
        // defaults for every pan (bindDefaultSlider above), in whole dB.
        if (m_highThresholdSlider) {
            m_highThresholdSlider->setValue(static_cast<int>(high));
        }
        if (m_lowThresholdSlider) {
            m_lowThresholdSlider->setValue(static_cast<int>(low));
        }
    });
    levForm->addRow(QString(), m_copySpecMinMaxBtn);

    addContent(levGroup);

    // --- Section: Waterfall NF-AGC (Task 2.8) ---
    auto* nfAgcGroup = new QGroupBox(QStringLiteral("Waterfall NF-AGC"), this);
    auto* nfAgcForm  = new QFormLayout(nfAgcGroup);
    nfAgcForm->setSpacing(6);

    m_wfNfAgcEnable = new QCheckBox(QStringLiteral("Enable NF-AGC"), nfAgcGroup);
    m_wfNfAgcEnable->setProperty("nereusSetupId", "display.waterfallDefaults.nfAgc");
    m_wfNfAgcEnable->setToolTip(
        QStringLiteral("When enabled, the waterfall low/high thresholds automatically "
                       "track the estimated noise floor. The offset below sets how far "
                       "below the noise floor the low threshold is placed."));
    bindDefaultToggle(m_wfNfAgcEnable, QStringLiteral("WaterfallNFAGCEnabled"),
                      [](SpectrumWidget* pan, bool on) { pan->setWaterfallNFAGCEnabled(on); });
    nfAgcForm->addRow(QString(), m_wfNfAgcEnable);

    m_wfAgcOffsetDb = new QSpinBox(nfAgcGroup);
    m_wfAgcOffsetDb->setProperty("nereusSetupId", "display.waterfallDefaults.nfAgcOffset");
    m_wfAgcOffsetDb->setRange(-60, 60);
    m_wfAgcOffsetDb->setSuffix(QStringLiteral(" dB"));
    m_wfAgcOffsetDb->setValue(0);
    m_wfAgcOffsetDb->setToolTip(
        QStringLiteral("Offset applied above the estimated noise floor when computing "
                       "the waterfall low threshold. Negative values place the low "
                       "threshold below the noise floor (recommended)."));
    bindDefaultSpin(m_wfAgcOffsetDb, QStringLiteral("WaterfallAGCOffsetDb"),
                    [](SpectrumWidget* pan, int v) { pan->setWaterfallAGCOffsetDb(v); });
    nfAgcForm->addRow(QStringLiteral("NF offset:"), m_wfAgcOffsetDb);

    addContent(nfAgcGroup);

    // --- Section: Display ---
    auto* dispGroup = new QGroupBox(QString::fromLatin1(ControlRanges::kDisplayWaterfallGroupTitle), this);
    auto* dispForm  = new QFormLayout(dispGroup);
    dispForm->setSpacing(6);

    {
        auto row = makeSliderRow(10, 500, 50, QStringLiteral(" ms"), dispGroup);
        m_updatePeriodSlider = row.slider;
        // Approved native Rate selector uses10ms keyboard/spin steps,
        // preserving the original10..500ms range and existing setter.
        m_updatePeriodSlider->setSingleStep(10);
        row.spin->setSingleStep(10);
        m_updatePeriodSlider->setProperty("nereusSetupId", "display.waterfallDefaults.updatePeriod");
        // Thetis: setup.designer.cs:34145 (udDisplayWaterfallUpdatePeriod)
        m_updatePeriodSlider->setToolTip(QStringLiteral("How often to update (scroll another pixel line) on the waterfall display.  Note that this is tamed by the FPS setting."));
        row.spin->setToolTip(QStringLiteral("How often to update (scroll another pixel line) on the waterfall display.  Note that this is tamed by the FPS setting."));
        bindDefaultSlider(m_updatePeriodSlider, row.spin,
                          QStringLiteral("DisplayWfUpdatePeriodMs"),
                          [](SpectrumWidget* pan, int v) { pan->setWfUpdatePeriodMs(v); });
        connect(m_updatePeriodSlider, &QSlider::valueChanged, this, [this](int) {
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
    m_wfStopOnTx->setProperty("nereusSetupId", "display.waterfallDefaults.stopOnTx");
    m_wfStopOnTx->setToolTip(
        QStringLiteral("Pause the waterfall while transmitting. "
                       "Resumes automatically when TX ends."));
    bindDefaultToggle(m_wfStopOnTx, QStringLiteral("WaterfallStopOnTx"),
                      [](SpectrumWidget* pan, bool on) { pan->setWaterfallStopOnTx(on); });
    dispForm->addRow(QString(), m_wfStopOnTx);

    {
        auto row = makeSliderRow(0, 100, 100, QStringLiteral("%"), dispGroup);
        m_opacitySlider = row.slider;
        m_opacitySlider->setProperty("nereusSetupId", "display.waterfallDefaults.opacity");
        // Thetis: setup.designer.cs:2056 (tbRX1WaterfallOpacity) — rewritten
        // Thetis original: (none)
        m_opacitySlider->setToolTip(QStringLiteral("Waterfall opacity (0 = fully transparent, 100 = fully opaque). Blends the waterfall over the spectrum background."));
        row.spin->setToolTip(QStringLiteral("Waterfall opacity (0 = fully transparent, 100 = fully opaque). Blends the waterfall over the spectrum background."));
        bindDefaultSlider(m_opacitySlider, row.spin, QStringLiteral("DisplayWfOpacity"),
                          [](SpectrumWidget* pan, int v) { pan->setWfOpacity(v); });
        dispForm->addRow(QStringLiteral("Opacity:"), row.container);
    }

    m_colorSchemeCombo = new QComboBox(dispGroup);
    // 3D Stacked-Trace Spectrum Plan Task 21 test seam: no production code
    // depends on this name, only tests/tst_display_setup_binding.cpp's
    // findChild lookup (this page is RadioModel-backed, so unlike
    // Display3DSetupPage's SpectrumWidget-first controls there is no
    // simpler headless construction path to key off instead).
    m_colorSchemeCombo->setObjectName(QStringLiteral("wfColorSchemeCombo"));
    m_colorSchemeCombo->setProperty("nereusSetupId", "display.waterfallDefaults.colorScheme");
    // Default, Enhanced, Spectran, BlackWhite, LinLog, LinRad, Custom and
    // Clarity Blue (Phase 3G-9b), in WfColorScheme order; the names live in
    // core/spectrum/WaterfallPalettes.cpp, which the Core's catalogue reads
    // too (iPhone app Task 19).
    for (int i = 0; i < static_cast<int>(WfColorScheme::Count); ++i) {
        m_colorSchemeCombo->addItem(
            QString::fromLatin1(wfSchemeName(static_cast<WfColorScheme>(i))));
    }
    // Thetis: setup.designer.cs:34110 (comboColorPalette) — rewritten
    // Thetis original: "Sets the color scheme"
    m_colorSchemeCombo->setToolTip(QStringLiteral("Waterfall color palette. Each scheme maps signal level to a different color gradient from low (dark) to high (bright)."));
    // 3D Stacked-Trace Spectrum Plan Task 21: reaches
    // DisplaySettingsModel::setWfColorScheme() instead of the widget's own
    // setWfColorScheme(), which clamps [0,7] itself (WfColorScheme::Count
    // == 8) so the local qBound() the widget-direct call needed is no
    // longer necessary here.
    bindDefaultCombo(m_colorSchemeCombo, QStringLiteral("DisplayWfColorScheme"),
                     [](SpectrumWidget* pan, int i) {
        pan->displaySettings()->setWfColorScheme(i);
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
    // Items from ControlRanges::kDisplayWaterfallDetectors.
    m_waterfallDetectorCombo = new QComboBox(dispGroup);
    m_waterfallDetectorCombo->setProperty("nereusSetupId", "display.waterfallDefaults.detector");
    for (const ControlRanges::DisplayChoiceItem& item : ControlRanges::kDisplayWaterfallDetectors) {
        m_waterfallDetectorCombo->addItem(QLatin1String(item.label));
    }
    // Thetis: setup.designer.cs:34461 (comboDispWFDetector) [v2.10.3.13] — no upstream tooltip; rewritten
    m_waterfallDetectorCombo->setToolTip(QStringLiteral(
        "Waterfall bin-reduction policy. Peak takes the maximum bin per pixel. "
        "Rosenfell alternates max/min. Average takes the arithmetic mean. "
        "Sample takes the first bin."));
    bindDefaultCombo(m_waterfallDetectorCombo, QStringLiteral("DisplayWaterfallDetector"),
                     [](SpectrumWidget* pan, int i) {
        pan->setWaterfallDetector(static_cast<SpectrumDetector>(i));
    });
    dispForm->addRow(QString::fromLatin1(ControlRanges::kDisplayWaterfallDetectorLabel)
                         + QLatin1Char(':'), m_waterfallDetectorCombo);

    // Ported from Thetis comboDispWFAveraging [v2.10.3.13]
    // (setup.designer.cs:34436): None / Recursive / Time Window / Log Recursive.
    // Items from ControlRanges::kDisplayAveragingModes.
    m_waterfallAveragingCombo = new QComboBox(dispGroup);
    m_waterfallAveragingCombo->setProperty("nereusSetupId", "display.waterfallDefaults.averaging");
    for (const ControlRanges::DisplayChoiceItem& item : ControlRanges::kDisplayAveragingModes) {
        m_waterfallAveragingCombo->addItem(QLatin1String(item.label));
    }
    // Thetis: setup.designer.cs:34436 (comboDispWFAveraging) [v2.10.3.13] — no upstream tooltip; rewritten
    m_waterfallAveragingCombo->setToolTip(QStringLiteral(
        "Waterfall frame-averaging mode. None shows raw FFT output per row. "
        "Recursive exponentially smooths in linear power space. "
        "Time Window approximates a sliding average. "
        "Log Recursive smooths in the dB domain for better low-signal visibility."));
    bindDefaultCombo(m_waterfallAveragingCombo, QStringLiteral("DisplayWaterfallAveraging"),
                     [](SpectrumWidget* pan, int i) {
        pan->setWaterfallAveraging(static_cast<SpectrumAveraging>(i));
    });
    dispForm->addRow(QString::fromLatin1(ControlRanges::kDisplayWaterfallAveragingLabel)
                         + QLatin1Char(':'), m_waterfallAveragingCombo);

    // Waterfall averaging time constant — independent from spectrum.
    // From Thetis udDisplayAVTimeWF [v2.10.3.13] (setup.designer.cs:2086).
    // Default 120 ms matches Thetis, and so do the range and step: 1..9999 ms
    // in steps of 1 (setup.designer.cs:34588-34616 [v2.10.3.15]).
    // Range, step and default from ControlRanges.h.
    m_waterfallAvgTimeSpin = new QSpinBox(dispGroup);
    m_waterfallAvgTimeSpin->setProperty("nereusSetupId", "display.waterfallDefaults.averageTime");
    m_waterfallAvgTimeSpin->setRange(ControlRanges::kDisplayAvgTimeMinMs,
                                     ControlRanges::kDisplayAvgTimeMaxMs);
    m_waterfallAvgTimeSpin->setSingleStep(ControlRanges::kDisplayAvgTimeStepMs);
    m_waterfallAvgTimeSpin->setSuffix(QStringLiteral(" ms"));
    m_waterfallAvgTimeSpin->setValue(ControlRanges::kDisplayWaterfallAvgTimeDefaultMs);
    // The alpha formula is Thetis's.
    m_waterfallAvgTimeSpin->setToolTip(QStringLiteral(
        "Waterfall averaging time constant. Independent from the spectrum "
        "averaging time. Larger = heavier smoothing. Translates to a "
        "frame-by-frame alpha via α = exp(−1 / (fps × τ))."));
    bindDefaultSpin(m_waterfallAvgTimeSpin, QStringLiteral("DisplayWaterfallAverageTimeMs"),
                    [](SpectrumWidget* pan, int ms) { pan->setWaterfallAverageTimeMs(ms); });
    dispForm->addRow(QString::fromLatin1(ControlRanges::kDisplayWaterfallAvgTimeLabel)
                         + QLatin1Char(':'), m_waterfallAvgTimeSpin);

    addContent(dispGroup);

    // --- Section: Overlays ---
    auto* ovGroup = new QGroupBox(QStringLiteral("Overlays"), this);
    auto* ovForm  = new QFormLayout(ovGroup);
    ovForm->setSpacing(6);

    m_showRxFilterToggle = new QCheckBox(QStringLiteral("RX Filter"), ovGroup);
    // Thetis: setup.designer.cs:3189 (chkShowRXFilterOnWaterfall) — rewritten
    // Thetis original: (none)
    m_showRxFilterToggle->setProperty("nereusSetupId", "display.waterfallDefaults.showRxFilter");
    m_showRxFilterToggle->setToolTip(QStringLiteral("Overlay the current RX passband filter boundaries on the waterfall display."));
    bindDefaultToggle(m_showRxFilterToggle, QStringLiteral("DisplayShowRxFilterOnWaterfall"),
                      [](SpectrumWidget* pan, bool on) { pan->setShowRxFilterOnWaterfall(on); });
    ovForm->addRow(QString(), m_showRxFilterToggle);

    m_showTxFilterToggle = new QCheckBox(QStringLiteral("TX Filter"), ovGroup);
    // Thetis: setup.designer.cs:3187 (chkShowTXFilterOnRXWaterfall) — rewritten
    // Thetis original: (none)
    m_showTxFilterToggle->setProperty("nereusSetupId", "display.waterfallDefaults.showTxFilter");
    m_showTxFilterToggle->setToolTip(QStringLiteral("Overlay the TX passband filter boundaries on the RX waterfall display."));
    bindDefaultToggle(m_showTxFilterToggle, QStringLiteral("DisplayShowTxFilterOnRxWaterfall"),
                      [](SpectrumWidget* pan, bool on) { pan->setShowTxFilterOnRxWaterfall(on); });
    ovForm->addRow(QString(), m_showTxFilterToggle);

    m_showRxZeroLineToggle = new QCheckBox(QStringLiteral("RX Zero"), ovGroup);
    // Thetis: setup.designer.cs:3188 (chkShowRXZeroLineOnWaterfall) — rewritten
    // Thetis original: (none)
    m_showRxZeroLineToggle->setProperty("nereusSetupId", "display.waterfallDefaults.showRxZeroLine");
    m_showRxZeroLineToggle->setToolTip(QStringLiteral("Draw a line on the waterfall at the RX center frequency (zero-beat reference)."));
    bindDefaultToggle(m_showRxZeroLineToggle, QStringLiteral("DisplayShowRxZeroLine"),
                      [](SpectrumWidget* pan, bool on) { pan->setShowRxZeroLineOnWaterfall(on); });
    ovForm->addRow(QString(), m_showRxZeroLineToggle);

    m_showTxZeroLineToggle = new QCheckBox(QStringLiteral("TX Zero"), ovGroup);
    // Thetis: setup.designer.cs:3242 (chkShowTXZeroLineOnWaterfall) — rewritten
    // Thetis original: (none)
    m_showTxZeroLineToggle->setProperty("nereusSetupId", "display.waterfallDefaults.showTxZeroLine");
    m_showTxZeroLineToggle->setToolTip(QStringLiteral("Draw a line on the waterfall at the TX center frequency (zero-beat reference)."));
    bindDefaultToggle(m_showTxZeroLineToggle, QStringLiteral("DisplayShowTxZeroLine"),
                      [](SpectrumWidget* pan, bool on) { pan->setShowTxZeroLineOnWaterfall(on); });
    ovForm->addRow(QString(), m_showTxZeroLineToggle);

    addContent(ovGroup);

    // ── Sub-epic E: rewind / history depth ────────────────────────────────
    auto* histGroup = new QGroupBox(QStringLiteral("Rewind history"), this);
    auto* histForm  = new QFormLayout(histGroup);
    histForm->setSpacing(6);

    m_historyDepthCombo = new QComboBox(histGroup);
    m_historyDepthCombo->setProperty("nereusSetupId", "display.waterfallDefaults.historyDepth");
    m_historyDepthCombo->addItem(QStringLiteral("60 seconds"),      60LL * 1000);
    m_historyDepthCombo->addItem(QStringLiteral("5 minutes"),  5LL * 60 * 1000);
    m_historyDepthCombo->addItem(QStringLiteral("15 minutes"), 15LL * 60 * 1000);
    m_historyDepthCombo->addItem(QStringLiteral("20 minutes"), 20LL * 60 * 1000);
    m_historyDepthCombo->setToolTip(
        QStringLiteral("Maximum amount of waterfall history kept for rewind. "
                       "Effective rewind is capped at %1 rows; slow the "
                       "update period to extend depth at fast refresh rates.")
            .arg(SpectrumWidget::kMaxWaterfallHistoryRows));
    histForm->addRow(QStringLiteral("Depth:"), m_historyDepthCombo);

    m_effectiveDepthLabel = new QLabel(histGroup);
    m_effectiveDepthLabel->setStyleSheet(QStringLiteral("color: #80a0b0;"));
    histForm->addRow(QStringLiteral(""), m_effectiveDepthLabel);

    addContent(histGroup);

    connect(m_historyDepthCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        const qint64 ms = m_historyDepthCombo->itemData(idx).toLongLong();
        applyDisplayDefault(QStringLiteral("DisplayWaterfallHistoryMs"), QString::number(ms),
                            [ms](SpectrumWidget* pan) { pan->setWaterfallHistoryMs(ms); });
        updateEffectiveDepthLabel();
    });
    followDisplayDefault(QStringLiteral("DisplayWaterfallHistoryMs"), [this]() {
        bool ok = false;
        const qint64 ms =
            displayDefaultText(QStringLiteral("DisplayWaterfallHistoryMs")).toLongLong(&ok);
        if (!ok) { return; }
        // Sub-epic E task 11: sync history-depth dropdown to current value.
        QSignalBlocker bd(m_historyDepthCombo);
        int idx = 3;  // default: 20 minutes
        for (int i = 0; i < m_historyDepthCombo->count(); ++i) {
            if (m_historyDepthCombo->itemData(i).toLongLong() == ms) {
                idx = i;
                break;
            }
        }
        m_historyDepthCombo->setCurrentIndex(idx);
    });

    // --- Section: Time ---
    auto* timeGroup = new QGroupBox(QStringLiteral("Time"), this);
    auto* timeForm  = new QFormLayout(timeGroup);
    timeForm->setSpacing(6);

    m_timestampPosCombo = new QComboBox(timeGroup);
    m_timestampPosCombo->setProperty("nereusSetupId", "display.waterfallDefaults.timestampPosition");
    m_timestampPosCombo->addItems({QStringLiteral("None"), QStringLiteral("Left"),
                                   QStringLiteral("Right")});
    // NereusSDR extension — no Thetis equivalent
    m_timestampPosCombo->setToolTip(QStringLiteral("Position of the time stamp drawn on each waterfall row. None disables timestamps; Left and Right place them at the respective edge."));
    bindDefaultCombo(m_timestampPosCombo, QStringLiteral("DisplayWfTimestampPos"),
                     [](SpectrumWidget* pan, int i) {
        pan->setWfTimestampPosition(
            static_cast<SpectrumWidget::TimestampPosition>(
                qBound(0, i, static_cast<int>(SpectrumWidget::TimestampPosition::Count) - 1)));
    });
    timeForm->addRow(QStringLiteral("Timestamp Position:"), m_timestampPosCombo);

    m_timestampModeCombo = new QComboBox(timeGroup);
    m_timestampModeCombo->setProperty("nereusSetupId", "display.waterfallDefaults.timestampMode");
    m_timestampModeCombo->addItems({QStringLiteral("UTC"), QStringLiteral("Local")});
    // NereusSDR extension — no Thetis equivalent
    m_timestampModeCombo->setToolTip(QStringLiteral("Time zone used for waterfall timestamps. UTC uses Coordinated Universal Time; Local uses the system clock time zone."));
    bindDefaultCombo(m_timestampModeCombo, QStringLiteral("DisplayWfTimestampMode"),
                     [](SpectrumWidget* pan, int i) {
        pan->setWfTimestampMode(
            static_cast<SpectrumWidget::TimestampMode>(
                qBound(0, i, static_cast<int>(SpectrumWidget::TimestampMode::Count) - 1)));
    });
    timeForm->addRow(QStringLiteral("Timestamp Mode:"), m_timestampModeCombo);

    addContent(timeGroup);

    // Low Level Color (W10) has moved to Setup → Appearance → Colors & Theme.
    auto* wfColorHint = new QLabel(QStringLiteral(
        "Spectrum / waterfall colors: Setup → Appearance → Colors & Theme."), this);
    wfColorHint->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; padding: 6px; }"));
    addContent(wfColorHint);
}

// ---------------------------------------------------------------------------
// GridScalesPage
// ---------------------------------------------------------------------------

GridScalesPage::GridScalesPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Grid & Scales"), model, parent)
{
    buildUI();
    loadFromRenderer();
    if (model) {
        // Display pill plan Task 21: the band rows edit the active pan's
        // band, so they are pointed at the new one when the active pan
        // changes.
        connect(model, &RadioModel::spectrumWidgetChanged, this,
                [this](SpectrumWidget*) { followBandGridModel(); });
        // Parity ruling C12: in a remote window the per-band values are the
        // Core's; when one arrives (or a whole snapshot) the spinboxes show
        // it. The pan has re-read the band before this signal.
        connect(model, &RadioModel::stationSettingChanged, this,
                [this](const QString& key) {
            if (key.isEmpty() || key.startsWith(QLatin1String("DisplayGridMax_"))
                || key.startsWith(QLatin1String("DisplayGridMin_"))) {
                applyBandSlot(m_bandModel);
            }
        });
    }
}

void GridScalesPage::displayPansChanged()
{
    followBandGridModel();
    refreshTrackingPresentation();
}

void GridScalesPage::refreshTrackingPresentation()
{
    SpectrumWidget* selected = nullptr;
    for (SpectrumWidget* pan : panWidgets()) { if (pan->panIndex() == displayPanIndex()) { selected = pan; break; } }
    const bool tracking = displayDefaultText(QStringLiteral("DisplayAdjustGridMinToNoiseFloor")) == QStringLiteral("True");
    const bool live = tracking && !(selected && selected->isMoxOverlayActive());
    if (m_nfOffsetGridFollow) { m_nfOffsetGridFollow->setEnabled(live); }
    if (m_maintainNFAdjustDelta) { m_maintainNFAdjustDelta->setEnabled(live); }
    // Relocated controls still have this callback owner. Refresh their
    // complete pair/caption presentation after the owner finishes seeding.
    emit trackingPresentationChanged();
}

// Display pill plan Task 21 (D27; R-DSP-36): the per-band dB Max and dB Min
// rows edit the band store of the active pan's model, the one the window
// gives through SetupDialog::setBandGridModelProvider (PanModels). Every pan
// on that band follows the store. With no pan there is no band to edit: the
// rows stay, greyed, with the reason.
void GridScalesPage::followBandGridModel()
{
    PanadapterModel* pan = bandGridModel();
    if (pan != m_bandModel) {
        if (m_bandModel) { disconnect(m_bandModel.data(), nullptr, this, nullptr); }
        m_bandModel = pan;
        if (pan) {
            // The editing-band label and the dbMax/dbMin spinboxes refresh
            // when the user tunes across a band boundary (or clicks a band
            // button), and when the store is changed from a pan.
            connect(pan, &PanadapterModel::bandChanged, this,
                    [this](Band) { applyBandSlot(m_bandModel); });
            connect(pan, &PanadapterModel::levelChanged, this,
                    [this]() { applyBandSlot(m_bandModel); });
        }
    }
    applyBandSlot(pan);
    updateBandRowGate();
}

// One gate for the band rows: the Core's settings being out of reach first
// (R-R3-21 / R-R3-10), then no pan. A control follows one helper, so the two
// conditions are combined here. The Core's reason wins when both hold: the
// values are the Core's, and opening a pan would not make them editable.
// The no-pan note below still says what to do about the pan.
void GridScalesPage::updateBandRowGate()
{
    const bool hasPan = m_bandModel != nullptr;
    const QString noPan = QString::fromUtf8(kSetupNoPanNote);
    if (property("displayAssemblyOwner").toBool()) {
        SpectrumWidget* selected = nullptr;
        for (SpectrumWidget* pan : panWidgets()) {
            if (pan->panIndex() == displayPanIndex()) { selected = pan; break; }
        }
        const bool keyed = selected && selected->isMoxOverlayActive();
        const bool tracking = displayDefaultText(QStringLiteral("DisplayAdjustGridMinToNoiseFloor")) == QStringLiteral("True");
        const bool clarity = displayDefaultText(QStringLiteral("ClarityEnabled")) == QStringLiteral("True");
        const bool scale = !clarity && displayDefaultText(QStringLiteral("DisplayWfUseSpectrumMinMax")) == QStringLiteral("True");
        const bool agc = !clarity && !scale && displayDefaultText(QStringLiteral("DisplayWfAgc")) == QStringLiteral("True");
        const bool runKnown = selected && (selected->coreWaterfallLevelsInUse()
            ? selected->coreWaterfallLevels().has_value() : selected->waterfallAgcRunLevel().has_value());
        const bool sourceKnown = (!clarity && !scale && !agc)
            || (selected && ((clarity && selected->clarityActive()) || (agc && runKnown)));
        const QString keyedReason = QStringLiteral("This pan is transmitting. Change its receive levels when it stops.");
        const QString availability = !m_stationAvailable ? m_stationReason : !hasPan ? noPan : keyedReason;
        gateStationControls({m_dbMaxSpin, m_dbMinSpin}, hasPan && m_stationAvailable && !keyed, availability);
        const bool keepRange = displayDefaultText(QStringLiteral("DisplayMaintainNFAdjustDelta")) == QStringLiteral("True");
        for (const bool minimum : {true, false}) {
            QSpinBox* spin = minimum ? m_dbMinSpin : m_dbMaxSpin;
            const QSignalBlocker block(spin);
            spin->setSpecialValueText(QString());
            if (hasPan && m_stationAvailable && !keyed) {
                gateOwnedControls({spin}, !tracking || (!minimum && !keepRange), minimum
                    ? QStringLiteral("Track floor is setting dB Min. Pick Manual to set it yourself.")
                    : QStringLiteral("Keep range moves dB Max with dB Min. Turn Keep range off to set dB Max yourself."));
            }
            if (!tracking || (!minimum && !keepRange)) { continue; }
            const bool known = selected && selected->acceptedGridFollowFloorDbm().has_value();
            if (known) {
                spin->setValue(qRound(minimum ? selected->refLevel() - selected->dynamicRange() : selected->refLevel()));
            } else {
                spin->setSpecialValueText(displayPanIndex() < 0 ? QStringLiteral("each pan") : QStringLiteral("no signal"));
                spin->setValue(spin->minimum());
            }
            if (hasPan && m_stationAvailable && !keyed) {
                gateOwnedControls({spin}, false, minimum
                    ? QStringLiteral("Track floor is setting dB Min. Pick Manual to set it yourself.")
                    : QStringLiteral("Keep range moves dB Max with dB Min. Turn Keep range off to set dB Max yourself."));
            }
        }
        QString copyReason = availability;
        if (hasPan && m_stationAvailable && !keyed) {
            if (tracking) {
                copyReason = QStringLiteral("Copy from waterfall levels sets dB Max and dB Min, and Track floor is setting dB Min. Pick Manual%1 to use it.")
                    .arg(scale ? QStringLiteral(" for the spectrum scale and for the waterfall levels") : QString());
            } else if (scale) {
                copyReason = QStringLiteral("Copy from waterfall levels sets dB Max and dB Min, and under Scale the waterfall levels come from the spectrum scale. Pick Manual for the waterfall levels to use it.");
            } else if (!sourceKnown) {
                copyReason = displayPanIndex() < 0
                    ? QStringLiteral("Low and High are set pan by pan here, so there is no one pair to copy. Pick a pan under Settings for.")
                    : QStringLiteral("Low and High have no signal, so there is no pair to copy.");
            }
        }
        gateStationControls({m_copyWfToSpecBtn}, hasPan && m_stationAvailable && !keyed
            && !tracking && !scale && sourceKnown, copyReason);
        if (m_noPanNote) { m_noPanNote->setText(hasPan ? QString() : noPan); }
        return;
    }
    gateStationControls({m_dbMaxSpin, m_dbMinSpin, m_copyWfToSpecBtn},
                        hasPan && m_stationAvailable,
                        m_stationAvailable ? noPan : m_stationReason);
    if (m_noPanNote) {
        m_noPanNote->setText(hasPan ? QString() : noPan);
    }
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

void GridScalesPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    m_stationAvailable = available;
    m_stationReason = reason;
    updateBandRowGate();
}

void GridScalesPage::loadFromRenderer()
{
    // Display pill plan Task 21 (D26): the per-pan rows show the Setup
    // default, not the active pan's own value (bindDefault* in buildUI).
    showDisplayDefaults();

    // Colour pickers (G6/G9–G13) moved to Setup → Appearance → Colors & Theme.

    // Task 2.9: NF tracking controls — SpectrumWidget-only state, must
    // load even when pan is null (otherwise the bug from 2026-05-08:
    // toggle showed unchecked on dialog reopen even though disk had True
    // because the early-return on !pan skipped the entire NF block).
    // These three are stored once for every pan, so any pan's value is the
    // value.
            // Mirror the toggle->dependent enabled-state gate from the
            // m_adjustGridMinToNF::toggled handler.  The toggle's setChecked
            // call above runs inside a QSignalBlocker so the handler doesn't
            // fire — without this re-sync the dependents stay disabled even
            // when the toggle restores to ON.
    refreshTrackingPresentation();

    // Per-band loads — gated on pan availability.
    followBandGridModel();
}

void GridScalesPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // --- Section: Grid ---
    auto* gridGroup = new QGroupBox(QStringLiteral("Grid"), this);
    auto* gridForm  = new QFormLayout(gridGroup);
    gridForm->setSpacing(6);

    m_gridToggle = new QCheckBox(QStringLiteral("Grid"), gridGroup);
    m_gridToggle->setProperty("nereusSetupId", "display.gridScales.showGrid");
    m_gridToggle->setChecked(true);
    // Thetis: setup.designer.cs:52824 (chkGridControl)
    m_gridToggle->setToolTip(QStringLiteral("Display the Major Grid on the Panadapter including the frequency numbers"));
    bindDefaultToggle(m_gridToggle, QStringLiteral("DisplayGridEnabled"),
                      [](SpectrumWidget* pan, bool on) { pan->setGridEnabled(on); });
    gridForm->addRow(QString(), m_gridToggle);

    m_dbmScaleVisibleToggle = new QCheckBox(QStringLiteral("dBm scale strip"), gridGroup);
    m_dbmScaleVisibleToggle->setProperty("nereusSetupId", "display.gridScales.dbmScale");
    m_dbmScaleVisibleToggle->setChecked(true);
    m_dbmScaleVisibleToggle->setToolTip(QStringLiteral("Show the reference-level scale on the right edge of the spectrum. "
                                                        "Disable to give the spectrum trace the full widget width."));
    bindDefaultToggle(m_dbmScaleVisibleToggle, QStringLiteral("DisplayDbmScaleVisible"),
                      [](SpectrumWidget* pan, bool on) { pan->setDbmScaleVisible(on); });
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

    m_editingBandLabel = new QLabel(QStringLiteral("◆ Editing per-band grid: – ◆"), gridGroup);
    m_editingBandLabel->setAlignment(Qt::AlignCenter);
    // Larger + brighter than before — section-header style instead of an
    // inline note.  Cyan picks up the existing accent palette.
    m_editingBandLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #00d4f0; font-size: 13px; font-weight: bold;"
        " padding: 4px 0; }"));
    gridForm->addRow(m_editingBandLabel);

    m_dbMaxSpin = new QSpinBox(gridGroup);
    m_dbMaxSpin->setProperty("nereusSetupId", "display.gridScales.dbMax");
    m_dbMaxSpin->setRange(-200, 0);
    m_dbMaxSpin->setValue(-40);
    m_dbMaxSpin->setSuffix(QStringLiteral(" dB"));
    // Thetis: setup.designer.cs:34745 (udDisplayGridMax) — rewritten
    // Thetis original: "Signal level at top of display in dB."
    m_dbMaxSpin->setToolTip(QStringLiteral("Signal level at the top of the display in dB, for the current band. Each band keeps its own value."));
    connect(m_dbMaxSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (PanadapterModel* pan = m_bandModel) {
            pan->setPerBandDbMax(pan->band(), v);
        }
    });
    m_dbMaxRowLabel = new QLabel(QStringLiteral("dB Max (per band):"), gridGroup);
    gridForm->addRow(m_dbMaxRowLabel, m_dbMaxSpin);

    m_dbMinSpin = new QSpinBox(gridGroup);
    m_dbMinSpin->setProperty("nereusSetupId", "display.gridScales.dbMin");
    m_dbMinSpin->setRange(-200, 0);
    m_dbMinSpin->setValue(-140);
    m_dbMinSpin->setSuffix(QStringLiteral(" dB"));
    // Thetis: setup.designer.cs:34714 (udDisplayGridMin) — rewritten
    // Thetis original: "Signal Level at bottom of display in dB."
    m_dbMinSpin->setToolTip(QStringLiteral("Signal level at the bottom of the display in dB, for the current band. Each band keeps its own value."));
    connect(m_dbMinSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (PanadapterModel* pan = m_bandModel) {
            pan->setPerBandDbMin(pan->band(), v);
        }
    });
    m_dbMinRowLabel = new QLabel(QStringLiteral("dB Min (per band):"), gridGroup);
    gridForm->addRow(m_dbMinRowLabel, m_dbMinSpin);

    m_dbStepSpin = new QSpinBox(gridGroup);
    m_dbStepSpin->setProperty("nereusSetupId", "display.gridScales.dbStep");
    m_dbStepSpin->setRange(1, 40);
    m_dbStepSpin->setValue(10);
    m_dbStepSpin->setSuffix(QStringLiteral(" dB"));
    // Thetis: setup.designer.cs:34683 (udDisplayGridStep) — rewritten
    // Thetis original: "Horizontal Grid Step Size in dB."
    m_dbStepSpin->setToolTip(QStringLiteral("Horizontal grid step size in dB. Sets the spacing between dB grid lines across all bands in the selected Settings for scope."));
    bindDefaultSpin(m_dbStepSpin, QStringLiteral("DisplayGridStep"),
                    [](SpectrumWidget* pan, int value) { pan->setDbGridStep(value); });
    gridForm->addRow(QStringLiteral("dB Step:"), m_dbStepSpin);

    // Display pill plan Task 21: why the three rows above are greyed when
    // no panadapter is open (updateBandRowGate).
    m_noPanNote = new QLabel(gridGroup);
    m_noPanNote->setObjectName(QStringLiteral("gridNoPanNote"));
    m_noPanNote->setWordWrap(true);
    m_noPanNote->setStyleSheet(QStringLiteral("color: #80a0b0;"));
    gridForm->addRow(QString(), m_noPanNote);

    addContent(gridGroup);

    // The band rows follow the active pan's band store: followBandGridModel()
    // connects PanadapterModel::bandChanged, and the constructor connects
    // RadioModel::stationSettingChanged.

    // --- Section: Labels ---
    auto* lblGroup = new QGroupBox(QStringLiteral("Labels"), this);
    auto* lblForm  = new QFormLayout(lblGroup);
    lblForm->setSpacing(6);

    m_freqLabelAlignCombo = new QComboBox(lblGroup);
    m_freqLabelAlignCombo->setProperty("nereusSetupId", "display.gridScales.freqLabelAlign");
    m_freqLabelAlignCombo->addItems({
        QStringLiteral("Left"),   QStringLiteral("Center"),
        QStringLiteral("Right"),  QStringLiteral("Auto"),
        QStringLiteral("Off")
    });
    m_freqLabelAlignCombo->setCurrentIndex(1);
    // Thetis: setup.designer.cs:34649 (comboDisplayLabelAlign) — rewritten
    // Thetis original: "Sets the alignement of the grid callouts on the display."
    m_freqLabelAlignCombo->setToolTip(QStringLiteral("Sets the alignment of the frequency labels on the grid callouts on the display."));
    bindDefaultCombo(m_freqLabelAlignCombo, QStringLiteral("DisplayFreqLabelAlign"),
                     [](SpectrumWidget* pan, int i) {
        pan->setFreqLabelAlign(static_cast<FreqLabelAlign>(i));
    });
    lblForm->addRow(QStringLiteral("Freq Label Align:"), m_freqLabelAlignCombo);

    m_zeroLineToggle = new QCheckBox(QStringLiteral("Show zero line"), lblGroup);
    m_zeroLineToggle->setProperty("nereusSetupId", "display.gridScales.zeroLine");
    // Thetis: setup.designer.cs:3221 (chkShowZeroLine) — rewritten
    // Thetis original: (none)
    m_zeroLineToggle->setToolTip(QStringLiteral("Show a horizontal line at 0 dBm on the panadapter grid."));
    bindDefaultToggle(m_zeroLineToggle, QStringLiteral("DisplayShowZeroLine"),
                      [](SpectrumWidget* pan, bool on) { pan->setShowZeroLine(on); });
    lblForm->addRow(QString(), m_zeroLineToggle);

    m_showFpsToggle = new QCheckBox(QStringLiteral("Show FPS overlay"), lblGroup);
    m_showFpsToggle->setProperty("nereusSetupId", "display.gridScales.showFps");
    // Thetis: setup.designer.cs:33177 (chkShowFPS)
    m_showFpsToggle->setToolTip(QStringLiteral("Show FPS reading in top left of spectrum area"));
    bindDefaultToggle(m_showFpsToggle, QStringLiteral("DisplayShowFps"),
                      [](SpectrumWidget* pan, bool on) { pan->setShowFps(on); });
    lblForm->addRow(QString(), m_showFpsToggle);

    addContent(lblGroup);

    // Grid/zero-line/band-edge colour pickers (G6/G9–G13) moved to
    // Setup → Appearance → Colors & Theme (consolidated colour panel).
    auto* gridColorHint = new QLabel(QStringLiteral(
        "Spectrum / waterfall colors: Setup → Appearance → Colors & Theme."), this);
    gridColorHint->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; padding: 6px; }"));
    addContent(gridColorHint);

    // --- Section: Noise-Floor Tracking (Task 2.9) ---
    // From Thetis setup.cs:24202-24213 [v2.10.3.13] chkAdjustGridMinToNFRX1
    // — RX1 scope dropped; NereusSDR applies as global panadapter default
    //   with per-pan override via ContainerSettings dialog (3G-6 pattern).
    auto* nfGroup = new QGroupBox(QStringLiteral("Noise-Floor Tracking"), this);
    auto* nfForm  = new QFormLayout(nfGroup);
    nfForm->setSpacing(6);

    m_adjustGridMinToNF = new QCheckBox(
        QStringLiteral("Adjust grid min to track noise floor"), nfGroup);
    m_adjustGridMinToNF->setProperty("nereusSetupId", "display.gridScales.adjustGridMinToNoiseFloor");
    // From Thetis setup.cs:24202 [v2.10.3.13] chkAdjustGridMinToNFRX1
    m_adjustGridMinToNF->setToolTip(
        QStringLiteral("When enabled, the lower grid boundary automatically follows the "
                       "live noise floor estimate. The grid min is set to NF + offset."));
    // calls scheduleSettingsSave internally
    bindDefaultToggle(m_adjustGridMinToNF, QStringLiteral("DisplayAdjustGridMinToNoiseFloor"),
        [](SpectrumWidget* pan, bool on) { pan->setAdjustGridMinToNoiseFloor(on); });
    nfForm->addRow(QString(), m_adjustGridMinToNF);

    m_nfOffsetGridFollow = new QSpinBox(nfGroup);
    m_nfOffsetGridFollow->setProperty("nereusSetupId", "display.gridScales.noiseFloorOffset");
    m_nfOffsetGridFollow->setRange(-60, 60);
    m_nfOffsetGridFollow->setValue(0);
    m_nfOffsetGridFollow->setSuffix(QStringLiteral(" dB"));
    m_nfOffsetGridFollow->setEnabled(false);
    // From Thetis console.cs:46035-46040 [v2.10.3.13] _RX1NFoffsetGridFollow = 5f.
    // NereusSDR: range -60..+60, default 0. Offset is added to NF estimate.
    // Thetis's default is 5 dB below the floor, offset -5 here; the
    // tooltip used to say so (the default stays 0, a difference for JJ).
    m_nfOffsetGridFollow->setToolTip(
        QStringLiteral("Offset added to the noise floor estimate to compute the grid min. "
                       "Use a negative value to place the grid min below the noise floor."));
    // calls scheduleSettingsSave internally
    bindDefaultSpin(m_nfOffsetGridFollow, QStringLiteral("DisplayNFOffsetGridFollow"),
        [](SpectrumWidget* pan, int value) { pan->setNFOffsetGridFollow(value); });
    nfForm->addRow(QStringLiteral("NF offset:"), m_nfOffsetGridFollow);

    m_maintainNFAdjustDelta = new QCheckBox(
        QStringLiteral("Keep range"), nfGroup);
    m_maintainNFAdjustDelta->setProperty("nereusSetupId", "display.gridScales.maintainGridRange");
    m_maintainNFAdjustDelta->setEnabled(false);
    // From Thetis console.cs:46085 [v2.10.3.13] _maintainNFAdjustDeltaRX1.
    // Range delta uses std::abs() guard: abs incase //MW0LGE [2.9.0.7] [original inline comment from console.cs:46081]
    m_maintainNFAdjustDelta->setToolTip(
        QStringLiteral("When enabled, the grid max is also moved so the dB range stays "
                       "constant as the grid min tracks the noise floor."));
    // calls scheduleSettingsSave internally
    bindDefaultToggle(m_maintainNFAdjustDelta, QStringLiteral("DisplayMaintainNFAdjustDelta"),
        [](SpectrumWidget* pan, bool on) { pan->setMaintainNFAdjustDelta(on); });
    nfForm->addRow(QString(), m_maintainNFAdjustDelta);

    followDisplayDefault(QStringLiteral("DisplayAdjustGridMinToNoiseFloor"), [this]() {
        const bool tracking = displayDefaultText(QStringLiteral("DisplayAdjustGridMinToNoiseFloor")) == QStringLiteral("True");
        const QSignalBlocker block(m_adjustGridMinToNF);
        m_adjustGridMinToNF->setChecked(tracking);
        refreshTrackingPresentation();
    });
    addContent(nfGroup);

    // --- Task 2.9: Copy waterfall thresholds → spectrum min/max ---
    // Reverse direction of Task 2.8's Copy spectrum min/max → waterfall.
    auto* copyGroup = new QGroupBox(QStringLiteral("Copy"), this);
    auto* copyForm  = new QFormLayout(copyGroup);
    copyForm->setSpacing(6);

    m_copyWfToSpecBtn = new QPushButton(
        QStringLiteral("Copy waterfall thresholds → spectrum min/max"), copyGroup);
    m_copyWfToSpecBtn->setProperty("nereusSetupId", "display.gridScales.copyWaterfallThresholds");
    m_copyWfToSpecBtn->setToolTip(
        QStringLiteral("Copies the current waterfall High Threshold and Low Threshold "
                       "into the spectrum dB max and dB min for the current band."));
    connect(m_copyWfToSpecBtn, &QPushButton::clicked, this, [this]() {
        if (property("displayAssemblyOwner").toBool()) {
            PanadapterModel* grid = bandGridModel();
            if (!grid) { return; }
            SpectrumWidget* selected = nullptr;
            for (SpectrumWidget* pan : panWidgets()) { if (pan->panIndex() == displayPanIndex()) { selected = pan; break; } }
            if (selected && selected->isMoxOverlayActive()) { return; }
            const bool clarity = displayDefaultText(QStringLiteral("ClarityEnabled")) == QStringLiteral("True");
            const bool scale = !clarity && displayDefaultText(QStringLiteral("DisplayWfUseSpectrumMinMax")) == QStringLiteral("True");
            const bool agc = !clarity && !scale && displayDefaultText(QStringLiteral("DisplayWfAgc")) == QStringLiteral("True");
            const bool runKnown = selected && (selected->coreWaterfallLevelsInUse()
                ? selected->coreWaterfallLevels().has_value() : selected->waterfallAgcRunLevel().has_value());
            if (scale || displayDefaultText(QStringLiteral("DisplayAdjustGridMinToNoiseFloor")) == QStringLiteral("True")) { return; }
            const bool known = (!clarity && !scale && !agc) || (selected && ((clarity && selected->clarityActive())
                || (agc && runKnown) || (scale && (!selected->adjustGridMinToNoiseFloor() || selected->acceptedGridFollowFloorDbm().has_value()))));
            if (!known) { return; }
            const auto pair = selected ? selected->effectiveWaterfallLevels()
                : std::pair<float, float>{displayDefaultText(QStringLiteral("DisplayWfLowLevel")).toFloat(), displayDefaultText(QStringLiteral("DisplayWfHighLevel")).toFloat()};
            if (pair.second <= pair.first) { return; }
            grid->setPerBandDbRange(grid->band(), qRound(pair.second), qRound(pair.first));
            applyBandSlot(grid);
            return;
        }
        auto* sw = model() ? model()->spectrumWidget() : nullptr;
        if (!sw) { return; }
        // Waterfall high → spectrum top (refLevel); waterfall low → spectrum bottom.
        const float wfHigh = sw->wfHighThreshold();
        const float wfLow  = sw->wfLowThreshold();
        // Display pill plan Task 21 (D27, S9): the range is the band's, so
        // it goes to the band store and nowhere else; the store moves every
        // pan on the band (this used to set the one widget's live range,
        // which the next band change threw away). Both ends go in one
        // step. An upside-down pair is not taken.
        PanadapterModel* pan = m_bandModel;
        const int dbMax = qRound(wfHigh);
        const int dbMin = qRound(wfLow);
        if (!pan || dbMax <= dbMin) { return; }
        const Band band = pan->band();
        pan->setPerBandDbRange(band, dbMax, dbMin);
        applyBandSlot(pan);
    });
    copyForm->addRow(QString(), m_copyWfToSpecBtn);

    addContent(copyGroup);

    // Sync enabled-state of NF sub-controls on construction after loadFromRenderer().
    // (loadFromRenderer runs before buildUI in the constructor, so the controls
    // don't exist yet when loadFromRenderer fires — re-apply the enabled gate here.)
    // Done via a post-construction deferred call: Qt will execute it after the
    // constructor returns, by which time loadFromRenderer has set m_adjustGridMinToNF.
    QTimer::singleShot(0, this, [this]() {
        refreshTrackingPresentation();
    });
}

// ---------------------------------------------------------------------------
// TxDisplayPage
// ---------------------------------------------------------------------------

TxDisplayPage::TxDisplayPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("TX Display"), model, parent)
{
    buildUI();
}

void TxDisplayPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    m_stationAvailable = available;
    m_stationReason = reason;
    refreshTxAnalyzerGate();
}

QList<QWidget*> TxDisplayPage::txAnalyzerControlsForTest() const
{
    return {m_txFftSizeSlider, m_txWindowCombo, m_txPanDetectorCombo, m_txPanAveragingCombo,
            m_txPanAvTimeSpin, m_txPanNormalizeCheck, m_txWfDetectorCombo,
            m_txWfAveragingCombo, m_txWfAvTimeSpin};
}

QString TxDisplayPage::coreDoesNotApplyReason()
{
    return QStringLiteral("This Core does not apply transmit display settings from this app. "
                          "Updating the Core may help.");
}

bool TxDisplayPage::remoteWindow()
{
    return model() != nullptr && !model()->ownsLocalDsp();
}

void TxDisplayPage::refreshTxAnalyzerGate()
{
    const QList<QWidget*> controls{m_txFftSizeSlider, m_txWindowCombo, m_txPanDetectorCombo,
                                   m_txPanAveragingCombo, m_txPanAvTimeSpin,
                                   m_txPanNormalizeCheck, m_txWfDetectorCombo,
                                   m_txWfAveragingCombo, m_txWfAvTimeSpin};
    // Put back each control's own state first, so Normalize's detector
    // gate below is what the gate saves and restores.
    gateStationControls(controls, true, QString());
    if (remoteWindow() && m_txPanNormalizeCheck && m_txPanDetectorCombo) {
        // From Thetis setup.cs:18152-18153 [v2.10.3.15]
        //   //[2.10.3.5]MW0LGE note: see updateNormalizePan() in specHPSDR as it only applies to pan detector type 2,3,4
        //   chkDispTXNormalize.Enabled = console.specRX.GetSpecRX(cmaster.inid(1, 0)).DetTypePan >= 2;
        m_txPanNormalizeCheck->setEnabled(m_txPanDetectorCombo->currentIndex() >= 2);
    }
    bool available = m_stationAvailable;
    QString reason = m_stationReason;
    if (available && remoteWindow() && model()->stationTxDisplayVersion() < 2) {
        available = false;
        reason = coreDoesNotApplyReason();
    }
    gateStationControls(controls, available, reason);
}

void TxDisplayPage::showTxFftReadouts(int fftSize, double binWidthHz)
{
    m_txFftSizeReadout->setText(QString::number(fftSize));
    m_txBinWidthLabel->setText(binWidthHz > 0.0 ? QString::number(binWidthHz, 'f', 3)
                                                : QStringLiteral("0.000"));
}

void TxDisplayPage::showCoreTxAnalyzerSettings()
{
    // The Core's nine keys as its TxAnalyzer::loadSettings reads them, and
    // held to what each setter takes, so a control never shows a value
    // the Core's analyzer does not run.
    auto& s = AppSettings::instance();
    const auto readInt = [&s](const char* key, int fallback) {
        bool ok = false;
        const int v = s.value(QLatin1String(key), fallback).toInt(&ok);
        return ok ? v : fallback;
    };
    const int fftSize = readInt(TxAnalyzer::kFftSizeKey, TxAnalyzer::kDefaultFftSize);
    const int position = TxAnalyzer::fftSizeSliderPositionFor(fftSize);
    const int shownSize = 4096 << position;
    {
        QSignalBlocker bSlider(m_txFftSizeSlider);
        QSignalBlocker bWin   (m_txWindowCombo);
        QSignalBlocker bPanDet(m_txPanDetectorCombo);
        QSignalBlocker bPanAvg(m_txPanAveragingCombo);
        QSignalBlocker bPanT  (m_txPanAvTimeSpin);
        QSignalBlocker bPanN  (m_txPanNormalizeCheck);
        QSignalBlocker bWfDet (m_txWfDetectorCombo);
        QSignalBlocker bWfAvg (m_txWfAveragingCombo);
        QSignalBlocker bWfT   (m_txWfAvTimeSpin);
        // Not while the operator is dragging it: the Core's echo of an
        // earlier step would pull the handle back.
        if (!m_txFftSizeSlider->isSliderDown()) {
            m_txFftSizeSlider->setValue(position);
            // The Core's analyzer runs at the TX DSP rate, 96 kHz.
            showTxFftReadouts(shownSize, static_cast<double>(WdspEngine::kTxDspSampleRate)
                                             / static_cast<double>(shownSize));
        }
        m_txWindowCombo->setCurrentIndex(
            std::clamp(readInt(TxAnalyzer::kWindowTypeKey, TxAnalyzer::kDefaultWindowType), 0, 6));
        m_txPanDetectorCombo->setCurrentIndex(std::clamp(
            readInt(TxAnalyzer::kPanDetectorKey, TxAnalyzer::kDefaultPanDetector), 0, 4));
        m_txPanAveragingCombo->setCurrentIndex(std::clamp(
            readInt(TxAnalyzer::kPanAveragingKey, TxAnalyzer::kDefaultPanAveraging), 0, 3));
        m_txPanAvTimeSpin->setValue(std::clamp(
            readInt(TxAnalyzer::kPanAvTimeMsKey, TxAnalyzer::kDefaultPanAvTimeMs), 1, 9999));
        m_txPanNormalizeCheck->setChecked(
            s.value(QLatin1String(TxAnalyzer::kPanNormalizeKey),
                    TxAnalyzer::kDefaultPanNormalize ? QStringLiteral("True")
                                                     : QStringLiteral("False"))
                .toString()
                .compare(QStringLiteral("True"), Qt::CaseInsensitive)
            == 0);
        m_txWfDetectorCombo->setCurrentIndex(std::clamp(
            readInt(TxAnalyzer::kWfDetectorKey, TxAnalyzer::kDefaultWfDetector), 0, 3));
        m_txWfAveragingCombo->setCurrentIndex(std::clamp(
            readInt(TxAnalyzer::kWfAveragingKey, TxAnalyzer::kDefaultWfAveraging), 0, 3));
        m_txWfAvTimeSpin->setValue(std::clamp(
            readInt(TxAnalyzer::kWfAvTimeMsKey, TxAnalyzer::kDefaultWfAvTimeMs), 1, 9999));
    }
    refreshTxAnalyzerGate();
}

void TxDisplayPage::showLocalTxAnalyzerSettings()
{
    auto* txa = model() ? model()->txAnalyzer() : nullptr;
    if (!txa) {
        return;
    }
    // QSignalBlocker so a setValue does not echo back into the analyzer.
    {
        QSignalBlocker bSlider(m_txFftSizeSlider);
        QSignalBlocker bWin   (m_txWindowCombo);
        QSignalBlocker bPanDet(m_txPanDetectorCombo);
        QSignalBlocker bPanAvg(m_txPanAveragingCombo);
        QSignalBlocker bPanT  (m_txPanAvTimeSpin);
        QSignalBlocker bPanN  (m_txPanNormalizeCheck);
        QSignalBlocker bWfDet (m_txWfDetectorCombo);
        QSignalBlocker bWfAvg (m_txWfAveragingCombo);
        QSignalBlocker bWfT   (m_txWfAvTimeSpin);

        // Derive slider position from current fftSize.  Defensive:
        // log2(fftSize / 4096), clamped to [0, 6].
        const int fs = txa->fftSize();
        int sliderPos = 0;
        for (int p = 0; p <= 6; ++p) {
            if ((4096 << p) >= fs) {
                sliderPos = p;
                break;
            }
        }
        m_txFftSizeSlider->setValue(sliderPos);

        m_txFftSizeReadout->setText(QString::number(txa->fftSize()));
        const double bw = txa->binWidthHz();
        m_txBinWidthLabel->setText(
            bw > 0.0 ? QString::number(bw, 'f', 3)
                     : QStringLiteral("0.000"));

        m_txWindowCombo->setCurrentIndex(
            std::clamp(txa->windowType(), 0, 6));
        m_txPanDetectorCombo->setCurrentIndex(
            std::clamp(txa->panDetector(), 0, 4));
        m_txPanAveragingCombo->setCurrentIndex(
            std::clamp(txa->panAveraging(), 0, 3));
        m_txPanAvTimeSpin->setValue(txa->panAvTimeMs());
        m_txPanNormalizeCheck->setChecked(txa->panNormalize());
        m_txPanNormalizeCheck->setEnabled(txa->panNormalizeEnabled());
        m_txWfDetectorCombo->setCurrentIndex(
            std::clamp(txa->wfDetector(), 0, 3));
        m_txWfAveragingCombo->setCurrentIndex(
            std::clamp(txa->wfAveraging(), 0, 3));
        m_txWfAvTimeSpin->setValue(txa->wfAvTimeMs());
    }
}

void TxDisplayPage::writeCoreTxAnalyzerSetting(const char* key, const QString& value)
{
    // Station scope: the window's settings proxy sends it to the Core,
    // which applies it to its TX analyzer (RadioModel::
    // applyRemoteTxDisplaySetting).
    AppSettings::instance().setValue(QLatin1String(key), value);
}

void TxDisplayPage::wireCoreTxAnalyzerControls()
{
    showCoreTxAnalyzerSettings();

    // Each control writes its key, as the local page's calls the setter
    // (setup.cs:18146-18210 [v2.10.3.15]).
    connect(m_txFftSizeSlider, &QSlider::valueChanged, this, [this](int v) {
        // From Thetis setup.cs:18179-18183 [v2.10.3.15]
        //   FFTSize = (int)(4096 * Math.Pow(2, Math.Floor((double)(tbTXDisplayFFTSize.Value))));
        //   double bin_width = (double)...SampleRate / (double)...FFTSize;
        //   lblTXDispBinWidth.Text = bin_width.ToString("N3");
        const int size = 4096 << std::clamp(v, 0, 6);
        showTxFftReadouts(size, static_cast<double>(WdspEngine::kTxDspSampleRate)
                                    / static_cast<double>(size));
        writeCoreTxAnalyzerSetting(TxAnalyzer::kFftSizeKey, QString::number(size));
    });
    connect(m_txWindowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
        writeCoreTxAnalyzerSetting(TxAnalyzer::kWindowTypeKey, QString::number(idx));
    });
    connect(m_txPanDetectorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
        writeCoreTxAnalyzerSetting(TxAnalyzer::kPanDetectorKey, QString::number(idx));
        refreshTxAnalyzerGate();
    });
    connect(m_txPanAveragingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
        writeCoreTxAnalyzerSetting(TxAnalyzer::kPanAveragingKey, QString::number(idx));
    });
    connect(m_txPanAvTimeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int ms) {
        writeCoreTxAnalyzerSetting(TxAnalyzer::kPanAvTimeMsKey, QString::number(ms));
    });
    connect(m_txPanNormalizeCheck, &QCheckBox::toggled, this, [this](bool on) {
        writeCoreTxAnalyzerSetting(TxAnalyzer::kPanNormalizeKey,
                                   on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    connect(m_txWfDetectorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
        writeCoreTxAnalyzerSetting(TxAnalyzer::kWfDetectorKey, QString::number(idx));
    });
    connect(m_txWfAveragingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
        writeCoreTxAnalyzerSetting(TxAnalyzer::kWfAveragingKey, QString::number(idx));
    });
    connect(m_txWfAvTimeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int ms) {
        writeCoreTxAnalyzerSetting(TxAnalyzer::kWfAvTimeMsKey, QString::number(ms));
    });
    // Once the slider is let go, show what the Core holds.
    connect(m_txFftSizeSlider, &QSlider::sliderReleased, this,
            &TxDisplayPage::showCoreTxAnalyzerSettings);

    // Another window's change, the Core's own (its value held to what the
    // analyzer takes), a refusal, and a new snapshot all bring the
    // controls back to the Core's keys; the Core's version opens or closes
    // them.
    connect(model(), &RadioModel::stationSettingChanged, this, [this](const QString& key) {
        if (key.isEmpty() || TxAnalyzer::isSettingsKey(key)) {
            showCoreTxAnalyzerSettings();
        }
    });
    if (auto* proxy = dynamic_cast<SettingsProxy*>(AppSettings::instance().remoteBackend())) {
        connect(proxy, &SettingsProxy::valueRejected, this,
                [this](const QString& key, const QVariant&) {
            if (TxAnalyzer::isSettingsKey(key)) {
                showCoreTxAnalyzerSettings();
            }
        });
    }
    connect(model(), &RadioModel::stationTxDisplayVersionChanged, this,
            &TxDisplayPage::refreshTxAnalyzerGate);
}

void TxDisplayPage::buildUI()
{
    // From Thetis tpDisplayTransmit [setup.designer.cs:36232 v2.10.3.13+501e3f51].
    // 5 groups matching Thetis layout order. Only group 4 (Waterfall Amplitude
    // Scale) is functional in 3M-5b. Others carry placeholder labels naming the
    // sub-phase that wires them.
    NereusSDR::Style::applyDarkPageStyle(this);

    // Sunken-readout stylesheet for the FFT size + bin width numeric cells.
    // Matches the RX-side FFT readout style introduced in 3G-8 (recessed
    // dark inset with cyan-accent text on the dark theme).
    const QString readoutStyle = QStringLiteral(
        "QLabel { background-color: #0a0a18; color: #00b4d8; "
        "border: 1px solid #1e2e3e; padding: 1px 6px; "
        "font-family: Menlo, Consolas, monospace; }");

    // ── Group 1: Fast Fourier Transform ────────────────────────────────
    // From Thetis groupBoxTS8 [setup.designer.cs:36509-36644 v2.10.3.13+501e3f51].
    // Controls (Thetis -> NereusSDR):
    //   tbTXDisplayFFTSize      QSlider, range 0..6, default 3 (Thetis ship)
    //   lblTXFFT_size           QLabel,  readout "4096" .. "262144"
    //   lblTXDispBinWidth       QLabel,  readout "0.000" .. "23.438"
    //   comboTXDispWinType      QComboBox, 7 windows (Rectangular..BH7)
    auto* fftGroup = new QGroupBox(
        QStringLiteral("Fast Fourier Transform"), this);
    auto* fftGrid = new QGridLayout(fftGroup);
    fftGrid->setSpacing(6);
    fftGrid->setColumnStretch(0, 0);
    fftGrid->setColumnStretch(1, 1);
    fftGrid->setColumnStretch(2, 0);
    fftGrid->setColumnStretch(3, 1);

    // Row 0: "Size" header centered.
    auto* txSizeHeader = new QLabel(QStringLiteral("Size"), fftGroup);
    txSizeHeader->setAlignment(Qt::AlignHCenter);
    fftGrid->addWidget(txSizeHeader, 0, 1, 1, 2);

    // Row 1: "Min" + slider + "Max".  Thetis trackbar Max=6.
    // From setup.designer.cs:36638-36642 [v2.10.3.13+501e3f51] —
    // tbTXDisplayFFTSize: Maximum=6, Value=3.  3M-5d binds slider position
    // to TxAnalyzer::setFftSizeSliderPosition (4096 * 2^position).
    auto* txMinLabel = new QLabel(QStringLiteral("Min"), fftGroup);
    auto* txMaxLabel = new QLabel(QStringLiteral("Max"), fftGroup);
    txMinLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    txMaxLabel->setAlignment(Qt::AlignLeft  | Qt::AlignVCenter);

    m_txFftSizeSlider = new QSlider(Qt::Horizontal, fftGroup);
    m_txFftSizeSlider->setProperty("nereusSetupId", "display.txDisplay.fftSize");
    m_txFftSizeSlider->setRange(0, 6);
    m_txFftSizeSlider->setSingleStep(1);
    m_txFftSizeSlider->setPageStep(1);
    m_txFftSizeSlider->setTickPosition(QSlider::NoTicks);
    m_txFftSizeSlider->setToolTip(QStringLiteral(
        "TX FFT size used during MOX. Range 4096 to 262144 in powers of "
        "two. Larger gives finer frequency resolution and a smaller bin "
        "width at higher CPU cost. RX FFT slider is independent."));
    fftGrid->addWidget(txMinLabel,         1, 0);
    fftGrid->addWidget(m_txFftSizeSlider,  1, 1, 1, 2);
    fftGrid->addWidget(txMaxLabel,         1, 3);

    // Row 2: bin-width readout + FFT-size readout, side by side.
    auto* txBinWidthPrefix = new QLabel(
        QStringLiteral("Bin Width (Hz)"), fftGroup);
    m_txBinWidthLabel = new QLabel(QStringLiteral("0.000"), fftGroup);
    m_txBinWidthLabel->setMinimumWidth(54);
    m_txBinWidthLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_txBinWidthLabel->setStyleSheet(readoutStyle);
    m_txBinWidthLabel->setToolTip(QStringLiteral(
        "Live bin width in Hz (TX sample rate / FFT size)."));

    auto* txFftSizePrefix = new QLabel(QStringLiteral("FFT Size"), fftGroup);
    m_txFftSizeReadout = new QLabel(QStringLiteral("4096"), fftGroup);
    m_txFftSizeReadout->setMinimumWidth(54);
    m_txFftSizeReadout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_txFftSizeReadout->setStyleSheet(readoutStyle);
    m_txFftSizeReadout->setToolTip(QStringLiteral(
        "Current TX FFT size in bins (4096 * 2^slider)."));

    fftGrid->addWidget(txBinWidthPrefix,    2, 0);
    fftGrid->addWidget(m_txBinWidthLabel,   2, 1);
    fftGrid->addWidget(txFftSizePrefix,     2, 2);
    fftGrid->addWidget(m_txFftSizeReadout,  2, 3);

    // Row 3: "Window" prefix + 7-item combo.
    // From setup.designer.cs:36555-36562 [v2.10.3.13+501e3f51] —
    // comboTXDispWinType.Items.AddRange order: Rectangular, BH4, Hann,
    // Flat-Top, Hamming, Kaiser, BH7.  Combo index 0..6 maps 1:1 to
    // WDSP analyzer window_type per specHPSDR.cs:134 default = 4 (Hamming).
    auto* txWindowPrefix = new QLabel(QStringLiteral("Window"), fftGroup);
    m_txWindowCombo = new QComboBox(fftGroup);
    m_txWindowCombo->setProperty("nereusSetupId", "display.txDisplay.window");
    m_txWindowCombo->addItems({
        QStringLiteral("Rectangular"),         // 0
        QStringLiteral("Blackman-Harris 4T"),  // 1
        QStringLiteral("Hann"),                // 2
        QStringLiteral("Flat-Top"),            // 3
        QStringLiteral("Hamming"),             // 4 (default)
        QStringLiteral("Kaiser"),              // 5
        QStringLiteral("Blackman-Harris 7T")   // 6
    });
    m_txWindowCombo->setToolTip(QStringLiteral(
        "TX FFT window function. Hamming (default) trades a wide main "
        "lobe for moderate sidelobe rejection. Blackman-Harris (4T or 7T) "
        "gives strong sidelobe rejection for cleaner waterfall during TX. "
        "Flat-Top is best for amplitude calibration. Kaiser is parameterized."));
    fftGrid->addWidget(txWindowPrefix,  3, 0);
    fftGrid->addWidget(m_txWindowCombo, 3, 1, 1, 3);

    addContent(fftGroup);

    // ── Group 2: Panadapter ────────────────────────────────────────────
    // From Thetis groupBoxTS7 [setup.designer.cs:36645-36768 v2.10.3.13+501e3f51].
    // Controls (Thetis -> NereusSDR):
    //   comboTXDispPanDetector    QComboBox, 5 items (Peak..RMS)
    //   comboTXDispPanAveraging   QComboBox, 4 items (None..Log Recursive)
    //   udTXDisplayAVGTime        QSpinBox, 1..9999 ms, default 30
    //   chkDispTXNormalize        QCheckBox, gated on DetTypePan >= 2
    auto* panGroup = new QGroupBox(QStringLiteral("Panadapter"), this);
    auto* panForm = new QFormLayout(panGroup);
    panForm->setSpacing(6);

    // Detector (Pan) — 5 items per setup.designer.cs:36718-36723.
    m_txPanDetectorCombo = new QComboBox(panGroup);
    m_txPanDetectorCombo->setProperty("nereusSetupId", "display.txDisplay.panDetector");
    m_txPanDetectorCombo->addItems({
        QStringLiteral("Peak"),       // 0
        QStringLiteral("Rosenfell"),  // 1
        QStringLiteral("Average"),    // 2
        QStringLiteral("Sample"),     // 3
        QStringLiteral("RMS")         // 4
    });
    m_txPanDetectorCombo->setToolTip(QStringLiteral(
        "TX panadapter trace detector. Peak shows the maximum bin value "
        "per pixel (transient-friendly). Average integrates across the "
        "bins (smoother). Sample takes one bin per pixel. RMS is true "
        "root-mean-square. Normalize is only available for Average / "
        "Sample / RMS."));
    panForm->addRow(QStringLiteral("Detector:"), m_txPanDetectorCombo);

    // Averaging (Pan) — 4 items per setup.designer.cs:36693-36697.
    m_txPanAveragingCombo = new QComboBox(panGroup);
    m_txPanAveragingCombo->setProperty("nereusSetupId", "display.txDisplay.panAveraging");
    m_txPanAveragingCombo->addItems({
        QStringLiteral("None"),           // 0
        QStringLiteral("Recursive"),      // 1
        QStringLiteral("Time Window"),    // 2
        QStringLiteral("Log Recursive")   // 3
    });
    m_txPanAveragingCombo->setToolTip(QStringLiteral(
        "TX panadapter trace averaging policy. None = instantaneous. "
        "Recursive = exponential smoothing with the time constant set "
        "below. Time Window = fixed-window N-frame average. Log "
        "Recursive = recursive in the log (dBm) domain."));
    panForm->addRow(QStringLiteral("Averaging:"), m_txPanAveragingCombo);

    // AvTime (Pan) — udTXDisplayAVGTime ms.
    // Range 1..9999 per setup.designer.cs:36738-36746.  Default 30 ms
    // per :36753 (NOT 120 — spec table was wrong; Thetis ships 30 for
    // the pan plane and 120 for the waterfall plane).
    m_txPanAvTimeSpin = new QSpinBox(panGroup);
    m_txPanAvTimeSpin->setProperty("nereusSetupId", "display.txDisplay.panAvTime");
    m_txPanAvTimeSpin->setRange(1, 9999);
    m_txPanAvTimeSpin->setSingleStep(1);
    m_txPanAvTimeSpin->setSuffix(QStringLiteral(" ms"));
    // Internally divided by 1000 to drive the WDSP exponential
    // back-multiplier.
    m_txPanAvTimeSpin->setToolTip(QStringLiteral(
        "TX panadapter averaging time constant in milliseconds. Larger "
        "values give smoother traces at the cost of slower response to "
        "transients."));
    panForm->addRow(QStringLiteral("Time:"), m_txPanAvTimeSpin);

    // Normalize (Pan) — chkDispTXNormalize.
    // Per Thetis setup.cs:18111-18112 + setup.designer.cs:36661-36673
    // [v2.10.3.13+501e3f51] the checkbox is enabled only when
    // DetTypePan >= 2 (Average / Sample / RMS).
    m_txPanNormalizeCheck = new QCheckBox(
        QStringLiteral("Normalize to 1 Hz"), panGroup);
    m_txPanNormalizeCheck->setProperty("nereusSetupId", "display.txDisplay.panNormalize");
    m_txPanNormalizeCheck->setToolTip(QStringLiteral(
        "Normalize the TX panadapter trace to a 1 Hz reference "
        "bandwidth. Only available for the Average, Sample, and RMS "
        "detectors (the Peak and Rosenfell detectors are not power-like)."));
    panForm->addRow(QStringLiteral(""), m_txPanNormalizeCheck);

    addContent(panGroup);

    // ── Group 3: Waterfall ─────────────────────────────────────────────
    // From Thetis groupBoxTS9 [setup.designer.cs:36405-36508 v2.10.3.13+501e3f51].
    // Controls (Thetis -> NereusSDR):
    //   comboTXDispWFDetector    QComboBox, 4 items (Peak..Sample, no RMS)
    //   comboTXDispWFAveraging   QComboBox, 4 items (None..Log Recursive)
    //   udTXDisplayAVTime        QSpinBox, 1..9999 ms, default 120
    auto* wfGroup = new QGroupBox(QStringLiteral("Waterfall"), this);
    auto* wfForm = new QFormLayout(wfGroup);
    wfForm->setSpacing(6);

    // Detector (WF) — 4 items per setup.designer.cs:36459-36463.  No RMS.
    m_txWfDetectorCombo = new QComboBox(wfGroup);
    m_txWfDetectorCombo->setProperty("nereusSetupId", "display.txDisplay.wfDetector");
    m_txWfDetectorCombo->addItems({
        QStringLiteral("Peak"),       // 0
        QStringLiteral("Rosenfell"),  // 1
        QStringLiteral("Average"),    // 2
        QStringLiteral("Sample")      // 3
    });
    m_txWfDetectorCombo->setToolTip(QStringLiteral(
        "TX waterfall detector. Applied independently of the pan "
        "detector. Peak suits transient visibility; Average gives a "
        "smoother waterfall floor."));
    wfForm->addRow(QStringLiteral("Detector:"), m_txWfDetectorCombo);

    // Averaging (WF) — same 4-item list as Pan.
    m_txWfAveragingCombo = new QComboBox(wfGroup);
    m_txWfAveragingCombo->setProperty("nereusSetupId", "display.txDisplay.wfAveraging");
    m_txWfAveragingCombo->addItems({
        QStringLiteral("None"),
        QStringLiteral("Recursive"),
        QStringLiteral("Time Window"),
        QStringLiteral("Log Recursive")
    });
    m_txWfAveragingCombo->setToolTip(QStringLiteral(
        "TX waterfall averaging policy. Applied independently of the "
        "pan averaging."));
    wfForm->addRow(QStringLiteral("Averaging:"), m_txWfAveragingCombo);

    // AvTime (WF) — udTXDisplayAVTime ms.
    // Range 1..9999 per setup.designer.cs:36478-36486.  Default 120 ms
    // per :36493.
    m_txWfAvTimeSpin = new QSpinBox(wfGroup);
    m_txWfAvTimeSpin->setProperty("nereusSetupId", "display.txDisplay.wfAvTime");
    m_txWfAvTimeSpin->setRange(1, 9999);
    m_txWfAvTimeSpin->setSingleStep(1);
    m_txWfAvTimeSpin->setSuffix(QStringLiteral(" ms"));
    m_txWfAvTimeSpin->setToolTip(QStringLiteral(
        "TX waterfall averaging time constant in milliseconds. Defaults "
        "to 120 ms (longer than the pan default of 30 ms) so the "
        "waterfall preserves a longer time history."));
    wfForm->addRow(QStringLiteral("Time:"), m_txWfAvTimeSpin);

    addContent(wfGroup);

    // Group 4: Waterfall Amplitude Scale (functional in 3M-5b).
    // From Thetis grpTXWFAmpScale [setup.designer.cs:36246-36379 v2.10.3.13+501e3f51].
    // Thetis label text: "Waterfall" (grpTXWFAmpScale.Text = "Waterfall").
    // NereusSDR uses the more descriptive "Waterfall Amplitude Scale" to match
    // the Thetis RX-side label wording and distinguish from Group 3.
    auto* ampGroup = new QGroupBox(QStringLiteral("Waterfall Amplitude Scale"), this);
    auto* ampForm  = new QFormLayout(ampGroup);
    ampForm->setSpacing(6);

    // Low Level: From Thetis udTXWFAmpMin
    // [setup.designer.cs:36319-36348 v2.10.3.13+501e3f51].
    // Range -200..200 step 5, default -70 dBm.
    // Thetis tooltip: "Waterfall Low Signal - Show Low Color below this value (gradient in between)."
    m_txWfLowLevelSpin = new QSpinBox(ampGroup);
    m_txWfLowLevelSpin->setProperty("nereusSetupId", "display.txDisplay.wfLowLevel");
    m_txWfLowLevelSpin->setRange(-200, 200);
    m_txWfLowLevelSpin->setSingleStep(5);
    m_txWfLowLevelSpin->setSuffix(QStringLiteral(" dBm"));
    m_txWfLowLevelSpin->setToolTip(QStringLiteral(
        "Waterfall Low Signal. Show Low Color below this value, with gradient in between."));
    ampForm->addRow(QStringLiteral("Low Level:"), m_txWfLowLevelSpin);

    // High Level: From Thetis udTXWFAmpMax
    // [setup.designer.cs:36350-36379 v2.10.3.13+501e3f51].
    // Range -200..200 step 5, default +30 dBm.
    // Thetis tooltip: "Waterfall High Signal - Show High Color above this value (gradient in between)."
    m_txWfHighLevelSpin = new QSpinBox(ampGroup);
    m_txWfHighLevelSpin->setProperty("nereusSetupId", "display.txDisplay.wfHighLevel");
    m_txWfHighLevelSpin->setRange(-200, 200);
    m_txWfHighLevelSpin->setSingleStep(5);
    m_txWfHighLevelSpin->setSuffix(QStringLiteral(" dBm"));
    m_txWfHighLevelSpin->setToolTip(QStringLiteral(
        "Waterfall High Signal. Show High Color above this value, with gradient in between."));
    ampForm->addRow(QStringLiteral("High Level:"), m_txWfHighLevelSpin);

    // Palette: From Thetis comboColorPalette_tx
    // [setup.designer.cs:36290-36307 v2.10.3.13+501e3f51].
    // Thetis 7-item list: Enhanced / Spectran / BlackWhite / LinLog / LinRad / LinAuto / Custom.
    // NereusSDR WfColorScheme: Default(0)=AetherSDR, Enhanced(1), Spectran(2),
    //   BlackWhite(3), LinLog(4), LinRad(5), Custom(6), ClarityBlue(7).
    // Thetis "LinAuto" maps to NereusSDR "Default" (AetherSDR/SmartSDR style).
    // Thetis tooltip: "Sets the color scheme"
    m_txWfPaletteCombo = new QComboBox(ampGroup);
    m_txWfPaletteCombo->setProperty("nereusSetupId", "display.txDisplay.wfPalette");
    m_txWfPaletteCombo->addItem(QStringLiteral("Enhanced"),
        QVariant::fromValue(static_cast<int>(WfColorScheme::Enhanced)));
    m_txWfPaletteCombo->addItem(QStringLiteral("Spectran"),
        QVariant::fromValue(static_cast<int>(WfColorScheme::Spectran)));
    m_txWfPaletteCombo->addItem(QStringLiteral("BlackWhite"),
        QVariant::fromValue(static_cast<int>(WfColorScheme::BlackWhite)));
    m_txWfPaletteCombo->addItem(QStringLiteral("LinLog"),
        QVariant::fromValue(static_cast<int>(WfColorScheme::LinLog)));
    m_txWfPaletteCombo->addItem(QStringLiteral("LinRad"),
        QVariant::fromValue(static_cast<int>(WfColorScheme::LinRad)));
    // Thetis "LinAuto" maps to NereusSDR Default (AetherSDR/SmartSDR style palette).
    m_txWfPaletteCombo->addItem(QStringLiteral("LinAuto"),
        QVariant::fromValue(static_cast<int>(WfColorScheme::Default)));
    m_txWfPaletteCombo->addItem(QStringLiteral("Custom"),
        QVariant::fromValue(static_cast<int>(WfColorScheme::Custom)));
    m_txWfPaletteCombo->setToolTip(QStringLiteral(
        "Sets the color scheme for the TX waterfall."));
    ampForm->addRow(QStringLiteral("Palette:"), m_txWfPaletteCombo);

    // Low Color: From Thetis clrbtnWaterfallLow_tx
    // [setup.designer.cs:36265-36279 v2.10.3.13+501e3f51].
    // Default Color.Black. Thetis tooltip: "The Color to use when the signal level
    // is at or below the low level set above."
    m_txWfLowColorBtn = new ColorSwatchButton(QColor(Qt::black), ampGroup);
    m_txWfLowColorBtn->setProperty("nereusSetupId", "display.txDisplay.wfLowColor");
    m_txWfLowColorBtn->setToolTip(QStringLiteral(
        "Color used when the signal level is at or below the Low Level set above."));
    ampForm->addRow(QStringLiteral("Low Color:"), m_txWfLowColorBtn);

    // Custom Gradient (3M-5c functional). Port of Thetis
    // lgLinearGradientTX_waterfall instance at setup.designer.cs:3283 area
    // [v2.10.3.13+501e3f51]. Visibility binds to palette = Custom; encoded
    // text persists under DisplayTxWfGradient (already reserved by 3M-5b).
    m_txWfGradientPicker = new GradientPickerWidget(ampGroup);
    m_txWfGradientPicker->setToolTip(QStringLiteral(
        "Multi-stop linear gradient editor. Click the strip body to add a "
        "stop, drag a stop to move it, double-click to recolor, right-click "
        "to delete (the two end-cap stops cannot be removed)."));
    auto* gradRowLabel = new QLabel(QStringLiteral("Custom Gradient:"), ampGroup);
    m_txWfGradientRowLabel = gradRowLabel;
    ampForm->addRow(gradRowLabel, m_txWfGradientPicker);

    addContent(ampGroup);

    // Group 5: TX Grid Scale (3M-5e)
    // From Thetis grpTXSpectrumGrid on tpDisplayTransmit
    // [setup.designer.cs:36239 v2.10.3.13+501e3f51].
    auto* txGridScale = makePlaceholderGroup(
        QStringLiteral("TX Grid Scale"),
        QStringLiteral("Max + Min + Step + Display Grid + Fill + Label Align (wired in 3M-5e)"),
        this);
    addContent(txGridScale);
    UnbuiltFeatures::hideUnlessBuilt(txGridScale, UnbuiltFeature::TxGridScale);

    // Wire the 4 functional Waterfall Amplitude Scale controls to SpectrumWidget.
    // Initial sync from current SpectrumWidget state.
    auto applyGradientVisibility = [this](WfColorScheme s) {
        const bool show = (s == WfColorScheme::Custom);
        if (m_txWfGradientPicker) {
            m_txWfGradientPicker->setVisible(show);
        }
        if (m_txWfGradientRowLabel) {
            m_txWfGradientRowLabel->setVisible(show);
        }
    };

    // Fix wave I8 (design D26, R-DSP-35): these five rows are Setup defaults
    // for every pan, like the rows on the other Display pages. Each edit is
    // stored as the default and moves every pan that has no value of its own
    // (applyDisplayDefault); each row opens on the default and follows it.

    // Low Level and High Level.
    bindDefaultSpin(m_txWfLowLevelSpin, QStringLiteral("DisplayTxWfLowLevel"),
                    [](SpectrumWidget* pan, int v) { pan->setTxWfLowLevel(v); });
    bindDefaultSpin(m_txWfHighLevelSpin, QStringLiteral("DisplayTxWfHighLevel"),
                    [](SpectrumWidget* pan, int v) { pan->setTxWfHighLevel(v); });

    // Palette: current item's data value is the WfColorScheme ordinal.
    connect(m_txWfPaletteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, applyGradientVisibility](int i) {
        const int val = m_txWfPaletteCombo->itemData(i).toInt();
        const auto scheme = static_cast<WfColorScheme>(
            qBound(0, val, static_cast<int>(WfColorScheme::Count) - 1));
        applyDisplayDefault(QStringLiteral("DisplayTxWfPalette"),
                            QString::number(static_cast<int>(scheme)),
                            [scheme](SpectrumWidget* pan) { pan->setTxWfPalette(scheme); });
        applyGradientVisibility(scheme);
    });
    followDisplayDefault(QStringLiteral("DisplayTxWfPalette"),
                         [this, applyGradientVisibility]() {
        bool ok = false;
        const int val = displayDefaultText(QStringLiteral("DisplayTxWfPalette")).toInt(&ok);
        if (!ok) { return; }
        // Sync palette combo: find item whose data matches the scheme ordinal.
        for (int i = 0; i < m_txWfPaletteCombo->count(); ++i) {
            if (m_txWfPaletteCombo->itemData(i).toInt() == val) {
                QSignalBlocker block(m_txWfPaletteCombo);
                m_txWfPaletteCombo->setCurrentIndex(i);
                // Only show the picker row when palette = Custom.
                applyGradientVisibility(static_cast<WfColorScheme>(val));
                break;
            }
        }
    });

    // Low Color: ColorSwatchButton emits colorChanged on user pick.
    connect(m_txWfLowColorBtn, &ColorSwatchButton::colorChanged,
            this, [this](const QColor& c) {
        applyDisplayDefault(QStringLiteral("DisplayTxWfLowColor"),
                            c.name(QColor::HexArgb).toUpper(),
                            [c](SpectrumWidget* pan) { pan->setTxWfLowColor(c); });
    });
    followDisplayDefault(QStringLiteral("DisplayTxWfLowColor"), [this]() {
        const QColor c =
            QColor::fromString(displayDefaultText(QStringLiteral("DisplayTxWfLowColor")));
        if (!c.isValid()) { return; }
        QSignalBlocker block(m_txWfLowColorBtn);
        m_txWfLowColorBtn->setColor(c);
    });

    // Custom Gradient picker: every mutation persists the encoded text.
    // SpectrumWidget caches the rebuilt 101-LUT from inside
    // setTxWfGradient (Step 2.12), so the live MOX render path picks up the
    // change on the next pushed waterfall row.
    if (m_txWfGradientPicker) {
        connect(m_txWfGradientPicker, &GradientPickerWidget::changed,
                this, [this]() {
            const QString encoded = m_txWfGradientPicker->encodedText();
            applyDisplayDefault(QStringLiteral("DisplayTxWfGradient"), encoded,
                                [encoded](SpectrumWidget* pan) {
                pan->setTxWfGradient(encoded);
            });
        });
        // setEncodedText is a silent no-op on empty/malformed input so a
        // fresh install retains the GradientPickerWidget default-ctor state
        // (Thetis-verbatim 8-stop grayscale).
        followDisplayDefault(QStringLiteral("DisplayTxWfGradient"), [this]() {
            const QString encoded =
                displayDefaultText(QStringLiteral("DisplayTxWfGradient"));
            if (encoded.isEmpty()) { return; }
            QSignalBlocker block(m_txWfGradientPicker);
            m_txWfGradientPicker->setEncodedText(encoded);
        });
    }

    // The picker row is hidden until the palette is Custom; then each row
    // opens on its default.
    applyGradientVisibility(WfColorScheme::Default);
    showDisplayDefaults();

    // ── 3M-5d: Groups 1-3 wiring ──────────────────────────────────────
    // Initial sync from TxAnalyzer's current state, then connect each
    // control to the corresponding setter via a lambda.  All 9 controls
    // route through model()->txAnalyzer() so the Setup page does not
    // depend on MainWindow.
    auto* txa = model() ? model()->txAnalyzer() : nullptr;
    if (txa) {
        showLocalTxAnalyzerSettings();
        // Parity both ways (Task 30): a remote window's change to this
        // Core's analyzer shows here too, signals blocked so it does not
        // echo back into the analyzer.
        connect(txa, &TxAnalyzer::settingReloaded, this,
                [this](const QString&) { showLocalTxAnalyzerSettings(); });

        // FFT size slider → setFftSizeSliderPosition + readout refresh.
        // From Thetis setup.cs:18136-18143 [v2.10.3.13+501e3f51] —
        // updates lblTXFFT_size + lblTXDispBinWidth on every scroll.
        connect(m_txFftSizeSlider, &QSlider::valueChanged,
                this, [this](int v) {
            auto* a = model() ? model()->txAnalyzer() : nullptr;
            if (!a) { return; }
            a->setFftSizeSliderPosition(v);
            m_txFftSizeReadout->setText(QString::number(a->fftSize()));
            const double bw = a->binWidthHz();
            m_txBinWidthLabel->setText(
                bw > 0.0 ? QString::number(bw, 'f', 3)
                         : QStringLiteral("0.000"));
        });

        // Window combo → setWindowType.
        // From Thetis setup.cs:18145-18150 [v2.10.3.13+501e3f51].
        connect(m_txWindowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (auto* a = model() ? model()->txAnalyzer() : nullptr) {
                a->setWindowType(idx);
            }
        });

        // Pan detector → setPanDetector + Normalize enable refresh.
        // From Thetis setup.cs:18105-18113 [v2.10.3.13+501e3f51].
        //[2.10.3.5]MW0LGE note: see updateNormalizePan() in specHPSDR as it only applies to pan detector type 2,3,4
        connect(m_txPanDetectorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            auto* a = model() ? model()->txAnalyzer() : nullptr;
            if (!a) { return; }
            a->setPanDetector(idx);
            m_txPanNormalizeCheck->setEnabled(a->panNormalizeEnabled());
        });

        // Pan averaging → setPanAveraging.
        // From Thetis setup.cs:18115-18120 [v2.10.3.13+501e3f51].
        connect(m_txPanAveragingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (auto* a = model() ? model()->txAnalyzer() : nullptr) {
                a->setPanAveraging(idx);
            }
        });

        // Pan AvTime spinbox → setPanAvTimeMs.
        // From Thetis setup.cs:18122-18127 [v2.10.3.13+501e3f51] —
        // AvTau = 0.001 * value (ms-to-seconds inside the setter).
        connect(m_txPanAvTimeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int ms) {
            if (auto* a = model() ? model()->txAnalyzer() : nullptr) {
                a->setPanAvTimeMs(ms);
            }
        });

        // Pan normalize checkbox → setPanNormalize.
        // From Thetis setup.cs:18129-18134 [v2.10.3.13+501e3f51].
        connect(m_txPanNormalizeCheck, &QCheckBox::toggled,
                this, [this](bool on) {
            if (auto* a = model() ? model()->txAnalyzer() : nullptr) {
                a->setPanNormalize(on);
            }
        });

        // WF detector → setWfDetector.
        // From Thetis setup.cs:18152-18157 [v2.10.3.13+501e3f51].
        //[2.10.3.5]MW0LGE note: see updateNormalizePan() in specHPSDR as it only applies to pan detector type 2,3,4
        //                                                 [setup.cs:18152]
        connect(m_txWfDetectorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (auto* a = model() ? model()->txAnalyzer() : nullptr) {
                a->setWfDetector(idx);
            }
        });

        // WF averaging → setWfAveraging.
        // From Thetis setup.cs:18159-18164 [v2.10.3.13+501e3f51].
        connect(m_txWfAveragingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (auto* a = model() ? model()->txAnalyzer() : nullptr) {
                a->setWfAveraging(idx);
            }
        });

        // WF AvTime spinbox → setWfAvTimeMs.
        // From Thetis setup.cs:18166-18171 [v2.10.3.13+501e3f51].
        connect(m_txWfAvTimeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int ms) {
            if (auto* a = model() ? model()->txAnalyzer() : nullptr) {
                a->setWfAvTimeMs(ms);
            }
        });
    } else if (remoteWindow()) {
        // R-R3-49 (parity Task 30): a remote window has no analyzer of its
        // own; the Core's is the one it drives.
        wireCoreTxAnalyzerControls();
    }
    refreshTxAnalyzerGate();
}

// ---------------------------------------------------------------------------
// Display3DSetupPage
// ---------------------------------------------------------------------------
// 3D Stacked-Trace Spectrum Plan Task 15. See the class-header comment in
// DisplaySetupPages.h for the full attribution and two-way-sync rationale.

Display3DSetupPage::Display3DSetupPage(SpectrumWidget* spectrumWidget, QWidget* parent)
    : SetupPage(QStringLiteral("3D View"), parent)
    , m_spectrumWidget(spectrumWidget)
{
    buildUI();

    // Display pill plan Task 21 (D26): the page sets the 3D defaults for
    // every pan and shows the defaults. Until Setup hands it the pan list,
    // the widget it was built against is the only pan it knows.
    setFallbackPan(m_spectrumWidget);
    loadFromWidget();

    if (!m_spectrumWidget) { return; }

    // DisplaySettingsModel -> this page (3D Stacked-Trace Spectrum Plan
    // Task 21). Reflects under QSignalBlocker only. Display pill plan Task
    // 21 leaves this one subscription: 3D Floor is kept per band, not as a
    // Setup default, so it still shows the pan's own value. The other six
    // rows follow their default through SetupPage::followDisplayDefault.
    DisplaySettingsModel* settings = m_spectrumWidget->displaySettings();
    connect(settings, &DisplaySettingsModel::dssFloorDepthChanged,
            this, [this](int dB) {
        if (!m_floorSlider || property("displayAssemblyOwner").toBool()) { return; }
        QSignalBlocker block(m_floorSlider);
        m_floorSlider->setValue(dB);
        if (m_floorSpin) {
            QSignalBlocker blockSpin(m_floorSpin);
            m_floorSpin->setValue(m_floorSlider->value());
        }
    });
}

void Display3DSetupPage::displayPansChanged()
{
    if (property("displayAssemblyOwner").toBool()) { showSelectedBandFloor(); }
}

void Display3DSetupPage::showSelectedBandFloor()
{
    PanadapterModel* grid = bandGridModel();
    if (m_floorBandModel != grid) {
        disconnect(m_floorBandChanged);
        disconnect(m_floorBandStored);
        m_floorBandModel = grid;
        if (grid) {
            m_floorBandChanged = connect(grid, &PanadapterModel::bandChanged, this, [this](Band) { showSelectedBandFloor(); });
            m_floorBandStored = connect(grid, &PanadapterModel::dss3DFloorDepthStored, this, [this](Band, int) { showSelectedBandFloor(); });
        }
    }
    if (!m_floorSlider || !m_floorSpin) { return; }
    gateOwnedControls({m_floorSlider, m_floorSpin}, grid != nullptr, QStringLiteral("There is no band to edit."));
    if (!grid) { return; }
    const QSignalBlocker sliderBlock(m_floorSlider);
    const QSignalBlocker spinBlock(m_floorSpin);
    const int depth = grid->dss3DFloorDepthForBand(grid->band());
    m_floorSlider->setValue(depth);
    m_floorSpin->setValue(depth);
}

void Display3DSetupPage::loadFromWidget()
{
    // Mode, Gain, Span, Angle, Speed and Slice Shadow: the Setup defaults.
    showDisplayDefaults();

    if (property("displayAssemblyOwner").toBool()) { showSelectedBandFloor(); return; }
    if (!m_spectrumWidget) { return; }
    if (m_floorSlider) {
        QSignalBlocker block(m_floorSlider);
        m_floorSlider->setValue(m_spectrumWidget->displaySettings()->dssFloorDepth());
        if (m_floorSpin) {
            QSignalBlocker blockSpin(m_floorSpin);
            m_floorSpin->setValue(m_floorSlider->value());
        }
    }
}

void Display3DSetupPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    // Display pill plan Task 21 (D26): Mode, Gain, Span, Angle, Speed and
    // Slice Shadow are Setup defaults. Each goes through applyDisplayDefault
    // (the bindDefault* helpers), which stores the default and moves every
    // pan that has no value of its own; a pan that has one keeps it.
    //
    // 3D Floor is the exception, kept per band on the pan it was built
    // against (3D Stacked-Trace Spectrum Plan Task 21's direct connect).
    // Only wired when m_spectrumWidget is non-null: it is a construction-time
    // pointer that never changes (see the class header comment), so a null
    // check at connect time is equivalent to the old per-call
    // "m_updatingFromModel || !m_spectrumWidget" guard for this half of the
    // pair -- if the pointer is null now it is null forever, and skipping
    // the connect entirely produces the same "nothing happens" outcome the
    // old runtime guard did.
    DisplaySettingsModel* settings =
        m_spectrumWidget ? m_spectrumWidget->displaySettings() : nullptr;

    // Phase 3G-9b precedent (SpectrumDefaultsPage's "Reset to Smooth
    // Defaults" button, above): a confirmation dialog gates the reset, and
    // the actual apply logic is factored into a separate method
    // (resetToDefaultsForTest here) so it is drivable without the modal
    // QMessageBox in the loop.
    auto* resetBtn = new QPushButton(QStringLiteral("Reset 3D to defaults"), this);
    resetBtn->setProperty("nereusSetupId", "display.threeD.reset");
    resetBtn->setToolTip(QStringLiteral(
        "Restore Spectrum render mode, 3D Floor, 3D Gain, 3D Span, 3D Angle, "
        "3D Speed and 3D Slice Shadow to their ship defaults (2D Waterfall / "
        "6 dB / 70% / 100% / 50% / Match / off)."));
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        const auto rc = QMessageBox::question(
            this,
            QStringLiteral("Reset 3D to defaults"),
            QStringLiteral(
                "This will restore Spectrum render mode, 3D Floor, 3D Gain, "
                "3D Span, 3D Angle, 3D Speed and 3D Slice Shadow to their "
                "ship defaults.\n\nContinue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (rc != QMessageBox::Yes) { return; }
        resetToDefaultsForTest();
    });
    addContent(resetBtn);

    auto* group = new QGroupBox(QStringLiteral("3D VIEW"), this);
    auto* form  = new QFormLayout(group);
    form->setSpacing(6);

    // ── Spectrum render mode (2D waterfall vs 3DSS) ─────────────────────
    // Labels/tooltip verbatim-matched to SpectrumOverlayMenu's "3D VIEW"
    // section (Task 13; AetherSDR SpectrumOverlayMenu.cpp:1874-1943
    // [@1872028c]) so an operator sees the same wording on both surfaces.
    m_modeCombo = new QComboBox(group);
    m_modeCombo->setObjectName(QStringLiteral("setup3DModeCombo"));
    m_modeCombo->setProperty("nereusSetupId", "display.threeD.renderMode");
    m_modeCombo->addItem(QStringLiteral("2D Waterfall"));       // SpectrumRenderMode::Mode2D
    m_modeCombo->addItem(QStringLiteral("3D Stacked Trace"));   // SpectrumRenderMode::Mode3D
    m_modeCombo->setToolTip(QStringLiteral(
        "2D: FFT trace + waterfall.\n"
        "3D: perspective stacked-trace spectrum stream."));
    bindDefaultCombo(m_modeCombo, QStringLiteral("DisplaySpectrumRenderMode"),
                     [](SpectrumWidget* pan, int mode) {
        pan->displaySettings()->setSpectrumRenderMode(mode);
    });
    form->addRow(QStringLiteral("Spectrum:"), m_modeCombo);

    // ── 3D floor depth -- how far below the noise floor to surface (dB) ──
    {
        auto row = makeSliderRow(0, 24, 6, QStringLiteral(" dB"), group);
        m_floorSlider = row.slider;
        m_floorSpin = row.spin;
        m_floorSlider->setObjectName(QStringLiteral("setup3DFloorSlider"));
        m_floorSlider->setProperty("nereusSetupId", "display.threeD.floor");
        // The surface starts at the pan's noise floor less this depth
        // (SpectrumWidget::dssFloorDbm), and each band keeps its own value
        // (PanadapterModel::dss3DFloorDepthForBand).
        const QString floorTip = QStringLiteral(
            "How far below the noise floor the surface starts. "
            "Each band keeps its own value.");
        m_floorSlider->setToolTip(floorTip);
        row.spin->setToolTip(floorTip);
        connect(m_floorSlider, &QSlider::valueChanged, this, [this, settings](int depth) {
            if (property("displayAssemblyOwner").toBool()) {
                if (PanadapterModel* grid = bandGridModel()) { grid->setDss3DFloorDepthForBand(grid->band(), depth); }
            } else if (settings) {
                settings->setDssFloorDepth(depth);
            }
        });
        form->addRow(QStringLiteral("3D Floor:"), row.container);
    }

    // ── 3D gain -- how far down the strength range the colormap reaches ──
    {
        auto row = makeSliderRow(0, 100, 70, QStringLiteral("%"), group);
        m_gainSlider = row.slider;
        m_gainSlider->setObjectName(QStringLiteral("setup3DGainSlider"));
        m_gainSlider->setProperty("nereusSetupId", "display.threeD.gain");
        const QString gainTip = QStringLiteral(
            "3D surface color gain: how far down the signal range the "
            "colormap reaches.\nHigher = color down toward the noise "
            "floor; lower = color only on the strongest signals.");
        m_gainSlider->setToolTip(gainTip);
        row.spin->setToolTip(gainTip);
        bindDefaultSlider(m_gainSlider, row.spin, QStringLiteral("Display3DGain"),
                          [](SpectrumWidget* pan, int pct) {
            pan->displaySettings()->setDssGain(pct);
        });
        form->addRow(QStringLiteral("3D Gain:"), row.container);
    }

    // ── 3D span -- how far the near rows overhang the plot edges ────────
    {
        auto row = makeSliderRow(0, 100, 100, QStringLiteral("%"), group);
        m_spanSlider = row.slider;
        m_spanSlider->setObjectName(QStringLiteral("setup3DSpanSlider"));
        m_spanSlider->setProperty("nereusSetupId", "display.threeD.span");
        const QString spanTip = QStringLiteral(
            "3D surface width: how far the nearest traces overhang the "
            "plot edges, using spectrum the radio sends from outside the "
            "panadapter.\nHigher = the empty wedges beside the surface "
            "close from the front; 0 = the classic narrowing trapezoid.");
        m_spanSlider->setToolTip(spanTip);
        row.spin->setToolTip(spanTip);
        bindDefaultSlider(m_spanSlider, row.spin, QStringLiteral("Display3DSpan"),
                          [](SpectrumWidget* pan, int pct) {
            pan->displaySettings()->setDssRowSpan(pct);
        });
        form->addRow(QStringLiteral("3D Span:"), row.container);
    }

    // ── 3D angle, viewing elevation ──────────────────────────────────────
    {
        auto row = makeSliderRow(0, 100, 50, QStringLiteral("%"), group);
        m_angleSlider = row.slider;
        m_angleSlider->setObjectName(QStringLiteral("setup3DAngleSlider"));
        m_angleSlider->setProperty("nereusSetupId", "display.threeD.angle");
        const QString angleTip = QStringLiteral(
            "Viewing angle for the 3D surface: low looks along the traces "
            "edge-on, high looks down on them.\n50 is the classic fixed "
            "angle.");
        m_angleSlider->setToolTip(angleTip);
        row.spin->setToolTip(angleTip);
        bindDefaultSlider(m_angleSlider, row.spin, QStringLiteral("Display3DAngle"),
                          [](SpectrumWidget* pan, int pct) {
            pan->displaySettings()->setDssAngle(pct);
        });
        form->addRow(QStringLiteral("3D Angle:"), row.container);
    }

    // ── 3D speed, row cadence divider (Task 24, NereusSDR-original) ─────
    // Not a makeSliderRow() (QSpinBox) pair like its siblings above: the
    // value text is "Match"/"1:N", not a plain number, so it uses the same
    // slider+QLabel shape SpectrumOverlayMenu's row does, with the same
    // tooltip verbatim on both widgets.
    {
        auto* container = new QWidget(group);
        auto* hbox = new QHBoxLayout(container);
        hbox->setContentsMargins(0, 0, 0, 0);
        m_speedSlider = new QSlider(Qt::Horizontal, container);
        m_speedSlider->setObjectName(QStringLiteral("setup3DSpeedSlider"));
        m_speedSlider->setProperty("nereusSetupId", "display.threeD.speed");
        m_speedSlider->setRange(0, 10);
        m_speedSlider->setValue(0);
        const QString speedTip = QStringLiteral(
            "How many waterfall rows each 3D row covers. Match keeps the 3D "
            "history the same length in time as the waterfall. 1:1 pushes "
            "every row, the fastest look. Higher values fold more rows into "
            "each 3D row, keeping peaks, so the surface recedes more slowly.");
        m_speedSlider->setToolTip(speedTip);
        m_speedValueLabel = new QLabel(QStringLiteral("Match"), container);
        m_speedValueLabel->setObjectName(QStringLiteral("setup3DSpeedValueLabel"));
        m_speedValueLabel->setToolTip(speedTip);
        hbox->addWidget(m_speedSlider);
        hbox->addWidget(m_speedValueLabel);
        const auto showSpeedText = [this](int v) {
            if (m_speedValueLabel) {
                m_speedValueLabel->setText(v == 0 ? QStringLiteral("Match")
                                                   : QStringLiteral("1:%1").arg(v));
            }
        };
        connect(m_speedSlider, &QSlider::valueChanged, this, [this, showSpeedText](int v) {
            showSpeedText(v);
            applyDisplayDefault(QStringLiteral("Display3DSpeed"), QString::number(v),
                                [v](SpectrumWidget* pan) {
                pan->displaySettings()->setDssRowDivider(v);
            });
        });
        followDisplayDefault(QStringLiteral("Display3DSpeed"), [this, showSpeedText]() {
            bool ok = false;
            const int v = displayDefaultText(QStringLiteral("Display3DSpeed")).toInt(&ok);
            if (!ok) { return; }
            QSignalBlocker block(m_speedSlider);
            m_speedSlider->setValue(v);
            showSpeedText(m_speedSlider->value());
        });
        form->addRow(QStringLiteral("3D Speed:"), container);
    }

    // ── 3D slice shadow, perspective decal for slice passbands ──────────
    m_sliceShadowCheck = new QCheckBox(QStringLiteral("3D Slice Shadow"), group);
    m_sliceShadowCheck->setObjectName(QStringLiteral("setup3DSliceShadowCheck"));
    m_sliceShadowCheck->setProperty("nereusSetupId", "display.threeD.sliceShadow");
    m_sliceShadowCheck->setToolTip(QStringLiteral(
        "Darken each slice's passband onto the 3D surface so it leans back\n"
        "with the perspective, instead of drawing flat on top of it."));
    bindDefaultToggle(m_sliceShadowCheck, QStringLiteral("Display3DSliceShadow"),
                      [](SpectrumWidget* pan, bool on) {
        pan->displaySettings()->setThreeDSliceDepth(on);
    });
    form->addRow(QString(), m_sliceShadowCheck);

    addContent(group);
}

void Display3DSetupPage::resetToDefaultsForTest()
{
    if (!m_spectrumWidget && !property("displayAssemblyOwner").toBool()) { return; }
    // Display pill plan Task 21: each ship value is stored as the Setup
    // default and reaches every pan that has no value of its own, whether
    // or not the page's control already showed it. A pan's own change,
    // stored or still waiting to be stored, is a value of its own and
    // stays (SetupPage::applyDisplayDefault). 3D Floor is per band.
    applyDisplayDefault(QStringLiteral("DisplaySpectrumRenderMode"),
                        QString::number(static_cast<int>(SpectrumRenderMode::Mode2D)),
                        [](SpectrumWidget* pan) {
        pan->displaySettings()->setSpectrumRenderMode(
            static_cast<int>(SpectrumRenderMode::Mode2D));
    });
    if (property("displayAssemblyOwner").toBool()) {
        if (PanadapterModel* grid = bandGridModel()) { grid->setDss3DFloorDepthForBand(grid->band(), 6); }
    } else {
        m_spectrumWidget->displaySettings()->setDssFloorDepth(6);
    }
    applyDisplayDefault(QStringLiteral("Display3DGain"), QStringLiteral("70"),
                        [](SpectrumWidget* pan) { pan->displaySettings()->setDssGain(70); });
    applyDisplayDefault(QStringLiteral("Display3DSpan"), QStringLiteral("100"),
                        [](SpectrumWidget* pan) { pan->displaySettings()->setDssRowSpan(100); });
    applyDisplayDefault(QStringLiteral("Display3DAngle"), QStringLiteral("50"),
                        [](SpectrumWidget* pan) { pan->displaySettings()->setDssAngle(50); });
    applyDisplayDefault(QStringLiteral("Display3DSpeed"), QStringLiteral("0"),
                        [](SpectrumWidget* pan) { pan->displaySettings()->setDssRowDivider(0); });
    applyDisplayDefault(QStringLiteral("Display3DSliceShadow"), QStringLiteral("False"),
                        [](SpectrumWidget* pan) {
        pan->displaySettings()->setThreeDSliceDepth(false);
    });
    showDisplayDefaults();
}


// NereusSDR-native assembly; original builders above retain all sourced
// comments, ranges and callback ownership. No second set of controls is built.
DisplayAssemblyPage::DisplayAssemblyPage(const QString& title, RadioModel* model,
    const QStringList& keys, bool panScope, QWidget* parent)
    : SetupPage(title, model, parent)
{
    setDisplayPageKeyBases(keys);
    for (const QString& key : keys) { followDisplayDefault(key, []() {}); }
    if (panScope) { enableDisplayPanScope(); }
    Style::applyDarkPageStyle(this);
    installEventFilter(this);
    QString banner;
    if (title == QStringLiteral("FFT & Engine")) {
        banner = QStringLiteral("The Core holds these. Every pan uses them, and so does every window connected to this Core.");
    } else if (title == QStringLiteral("Transmit Display")) {
        banner = QStringLiteral("The pan that transmits is drawn with these while it transmits. The Core holds the three shared groups; this computer holds the rest.");
    }
    if (!banner.isEmpty()) {
        auto* label = new QLabel(banner, this);
        label->setObjectName(QStringLiteral("setupDisplayOwnerBanner"));
        label->setWordWrap(true);
        addContent(label);
    }
}

static bool displayItemContains(QLayoutItem* item, QWidget* control)
{
    if (!item) { return false; }
    if (QWidget* widget = item->widget()) {
        return widget == control || widget->isAncestorOf(control);
    }
    if (QLayout* layout = item->layout()) {
        for (int index = 0; index < layout->count(); ++index) {
            if (displayItemContains(layout->itemAt(index), control)) { return true; }
        }
    }
    return false;
}

void DisplayAssemblyPage::takeRows(QWidget* source, const QString& title, const QStringList& ids)
{
    if (pageTitle() == QStringLiteral("Levels")) {
        if (auto* owner = qobject_cast<GridScalesPage*>(source)) {
            if (!property("gridTrackingPresentationConnected").toBool()) {
                connect(owner, &GridScalesPage::trackingPresentationChanged, this, [this]() { refreshAssemblyPresentation(); });
                setProperty("gridTrackingPresentationConnected", true);
            }
        }
    }
    QGroupBox* group = nullptr;
    for (QGroupBox* candidate : findChildren<QGroupBox*>()) {
        if (candidate->property("displayAssemblyGroupTitle").toString() != title
            || !qobject_cast<QVBoxLayout*>(candidate->layout())) { continue; }
        QWidget* owner = candidate->parentWidget();
        while (owner && !qobject_cast<SetupPage*>(owner)) { owner = owner->parentWidget(); }
        if (owner == this) { group = candidate; break; }
    }
    const bool created = group == nullptr;
    if (created) {
        group = new QGroupBox(title, this);
        if (title.isEmpty()) { group->setFlat(true); }
        group->setProperty("displayAssemblyGroupTitle", title);
        auto* column = new QVBoxLayout(group);
        column->setContentsMargins(8, 6, 8, 6);
        column->setSpacing(6);
    }
    auto* rows = qobject_cast<QVBoxLayout*>(group->layout());
    for (const QString& id : ids) {
        QWidget* control = nullptr;
        for (QWidget* candidate : source->findChildren<QWidget*>()) {
            if (candidate->property("nereusSetupId").toString() == id) { control = candidate; break; }
        }
        if (!control) { continue; }
        if (auto* button = qobject_cast<QPushButton*>(control)) {
            if (id == QStringLiteral("display.gridScales.copyWaterfallThresholds")) { button->setText(QStringLiteral("Copy from waterfall levels")); }
            if (id == QStringLiteral("display.waterfallDefaults.copySpectrumMinMax")) { button->setText(QStringLiteral("Copy from spectrum scale")); }
        }
        if (auto* toggle = qobject_cast<QCheckBox*>(control)) {
            if (id == QStringLiteral("display.gridScales.maintainGridRange")) { toggle->setText(QStringLiteral("Keep range")); }
        }
        bool moved = false;
        for (QFormLayout* form : source->findChildren<QFormLayout*>()) {
            for (int row = 0; row < form->rowCount(); ++row) {
                if (!displayItemContains(form->itemAt(row, QFormLayout::FieldRole), control)
                    && !displayItemContains(form->itemAt(row, QFormLayout::SpanningRole), control)) { continue; }
                const QFormLayout::TakeRowResult taken = form->takeRow(row);
                if (taken.labelItem) {
                    if (auto* label = qobject_cast<QLabel*>(taken.labelItem->widget())) {
                        if (id == QStringLiteral("display.waterfallDefaults.lowThreshold")) { label->setText(QStringLiteral("Low:")); }
                        if (id == QStringLiteral("display.waterfallDefaults.highThreshold")) { label->setText(QStringLiteral("High:")); }
                        if (id == QStringLiteral("display.waterfallDefaults.nfAgcOffset")) { label->setText(QStringLiteral("NF Offset:")); }
                        if (id == QStringLiteral("display.gridScales.noiseFloorOffset")) { label->setText(QStringLiteral("Floor Offset:")); }
                    }
                }
                auto* holder = new QWidget(group);
                auto* line = new QHBoxLayout(holder);
                line->setContentsMargins(0, 0, 0, 0);
                const auto attach = [line, holder](QLayoutItem* item) {
                    if (!item) { return; }
                    const auto reparent = [holder](auto&& self, QLayoutItem* child) -> void {
                        if (QWidget* widget = child->widget()) { widget->setParent(holder); widget->show(); }
                        else if (QLayout* layout = child->layout()) {
                            for (int index = 0; index < layout->count(); ++index) { self(self, layout->itemAt(index)); }
                        }
                    };
                    reparent(reparent, item);
                    if (QLayout* layout = item->layout()) { layout->setParent(nullptr); line->addLayout(layout); }
                    else { line->addItem(item); }
                };
                attach(taken.labelItem);
                attach(taken.fieldItem);
                rows->addWidget(holder);
                moved = true;
                break;
            }
            if (moved) { break; }
        }
        if (!moved) {
            for (QGridLayout* grid : source->findChildren<QGridLayout*>()) {
                const int index = grid->indexOf(control);
                if (index < 0) { continue; }
                int firstRow = 0, column = 0, rowSpan = 0, columnSpan = 0;
                grid->getItemPosition(index, &firstRow, &column, &rowSpan, &columnSpan);
                int lastRow = firstRow + rowSpan - 1;
                if (id == QStringLiteral("display.spectrumDefaults.fftSize")) {
                    // Keep Size/Min/Max, the slider, both live value cells
                    // and the readout together, as the original grid builds.
                    firstRow = 1;
                    lastRow = 4;
                }
                auto* holder = new QWidget(group);
                auto* target = new QGridLayout(holder);
                target->setContentsMargins(0, 0, 0, 0);
                target->setSpacing(grid->spacing());
                for (int itemIndex = grid->count() - 1; itemIndex >= 0; --itemIndex) {
                    int row = 0, col = 0, height = 0, width = 0;
                    grid->getItemPosition(itemIndex, &row, &col, &height, &width);
                    if (row < firstRow || row > lastRow) { continue; }
                    QLayoutItem* item = grid->takeAt(itemIndex);
                    if (QWidget* widget = item->widget()) { widget->setParent(holder); widget->show(); }
                    target->addItem(item, row - firstRow, col, height, width);
                }
                rows->addWidget(holder);
                moved = true;
                break;
            }
        }
        if (!moved) {
            if (control->parentWidget()->layout()) { control->parentWidget()->layout()->removeWidget(control); }
            rows->addWidget(control);
        }
    }
    if (created) {
        if (rows->count()) { addContent(group); }
        else { group->hide(); }
    }
}

void DisplayAssemblyPage::takeGroups(QWidget* source)
{
    const QList<QGroupBox*> groups = source->findChildren<QGroupBox*>();
    for (QGroupBox* group : groups) {
        bool nested = false;
        for (QWidget* ancestor = group->parentWidget(); ancestor && ancestor != source; ancestor = ancestor->parentWidget()) {
            if (qobject_cast<QGroupBox*>(ancestor)) { nested = true; break; }
        }
        if (nested) { continue; }
        if (group->parentWidget()->layout()) { group->parentWidget()->layout()->removeWidget(group); }
        if (pageTitle() == QStringLiteral("Transmit Display") && group->title() == QStringLiteral("Fast Fourier Transform")) { group->setTitle(QStringLiteral("FFT")); }
        if (group->title() == QStringLiteral("Active Peak Hold")) { group->setTitle(QStringLiteral("Peak trace")); }
        if (group->title() == QStringLiteral("Peak Blobs")) { group->setTitle(QStringLiteral("Peak markers")); }
        addContent(group);
    }
}

void DisplayAssemblyPage::addSpectrumScaleChooser()
{
    auto* group = new QGroupBox(QStringLiteral("Spectrum scale"), this);
    group->setProperty("displayAssemblyGroupTitle", QStringLiteral("Spectrum scale"));
    auto* column = new QVBoxLayout(group);
    auto* line = new QHBoxLayout;
    column->addLayout(line);
    auto* note = new QLabel(group);
    note->setWordWrap(true);
    column->addWidget(note);
    const QVector<QString> texts = {QStringLiteral("Manual"), QStringLiteral("Track floor")};
    QVector<QPushButton*> buttons;
    for (int index = 0; index < texts.size(); ++index) {
        auto* button = new QPushButton(texts[index], group);
        button->setCheckable(true);
        button->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
        button->setProperty("nereusSetupId", QStringLiteral("display.levels.spectrumMode.%1").arg(index));
        line->addWidget(button);
        buttons.append(button);
        if (index == 0) {
            button->setProperty("nereusDisplayKeyBase", QStringLiteral("DisplayAdjustGridMinToNoiseFloor"));
            button->setProperty("nereusDisplayRowName", QStringLiteral("Spectrum scale choice"));
        }
        connect(button, &QPushButton::clicked, this, [this, index]() {
            const bool track = index == 1;
            if ((displayDefaultText(QStringLiteral("DisplayAdjustGridMinToNoiseFloor")) == QStringLiteral("True")) == track) {
                showDisplayDefaults(); return;
            }
            applyDisplayDefault(QStringLiteral("DisplayAdjustGridMinToNoiseFloor"), track ? QStringLiteral("True") : QStringLiteral("False"),
                [track](SpectrumWidget* pan) { pan->setAdjustGridMinToNoiseFloor(track); });
        });
    }
    const auto seed = [this, buttons, note]() {
        const int selected = displayDefaultText(QStringLiteral("DisplayAdjustGridMinToNoiseFloor")) == QStringLiteral("True") ? 1 : 0;
        QString text = selected ? QStringLiteral("dB Min follows the noise floor plus Floor Offset.") : QStringLiteral("You set dB Max and dB Min.");
        if (displayDefaultText(QStringLiteral("ClarityEnabled")) != QStringLiteral("True")
            && displayDefaultText(QStringLiteral("DisplayWfUseSpectrumMinMax")) == QStringLiteral("True")) { text += QStringLiteral(" The waterfall follows this scale."); }
        SpectrumWidget* pan = nullptr;
        for (SpectrumWidget* candidate : panWidgets()) { if (candidate->panIndex() == displayPanIndex()) { pan = candidate; break; } }
        const bool keyed = pan && pan->isMoxOverlayActive();
        const QString why = QStringLiteral("Pan %1 is transmitting on the transmit scale. This chooser is a receive setting and is not used until it stops.").arg(displayPanIndex() + 1);
        note->setText(keyed ? why : text);
        QList<QWidget*> choices;
        for (QPushButton* button : buttons) { choices.append(button); }
        gateOwnedControls(choices, !keyed, why);
        for (int index = 0; index < buttons.size(); ++index) { const QSignalBlocker block(buttons[index]); buttons[index]->setChecked(index == selected); }
    };
    followDisplayDefault(QStringLiteral("DisplayAdjustGridMinToNoiseFloor"), seed);
    m_pictureRefreshers.append(seed);
    addContent(group);
    showDisplayDefaults();
}

void DisplayAssemblyPage::addWaterfallLevelsChooser()
{
    auto* group = new QGroupBox(QStringLiteral("Waterfall levels"), this);
    group->setProperty("displayAssemblyGroupTitle", QStringLiteral("Waterfall levels"));
    auto* column = new QVBoxLayout(group);
    auto* line = new QHBoxLayout;
    column->addLayout(line);
    auto* note = new QLabel(group);
    note->setWordWrap(true);
    const QStringList texts = {QStringLiteral("Clarity"), QStringLiteral("Auto"), QStringLiteral("Floor"), QStringLiteral("Manual"), QStringLiteral("Scale")};
    const QStringList keys = {QStringLiteral("ClarityEnabled"), QStringLiteral("DisplayWfAgc"), QStringLiteral("WaterfallNFAGCEnabled"), QStringLiteral("DisplayWfUseSpectrumMinMax")};
    const auto mode = [this]() {
        if (displayDefaultText(QStringLiteral("ClarityEnabled")) == QStringLiteral("True")) { return 0; }
        if (displayDefaultText(QStringLiteral("DisplayWfUseSpectrumMinMax")) == QStringLiteral("True")) { return 4; }
        if (displayDefaultText(QStringLiteral("DisplayWfAgc")) == QStringLiteral("True")) {
            return displayDefaultText(QStringLiteral("WaterfallNFAGCEnabled")) == QStringLiteral("True") ? 2 : 1;
        }
        return 3;
    };
    QVector<QPushButton*> buttons;
    for (int index = 0; index < texts.size(); ++index) {
        auto* button = new QPushButton(texts[index], group);
        button->setCheckable(true);
        button->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
        button->setProperty("nereusSetupId", QStringLiteral("display.levels.waterfallMode.%1").arg(index));
        line->addWidget(button);
        buttons.append(button);
        if (index == 0) {
            button->setProperty("nereusDisplayRowKeys", keys);
            button->setProperty("nereusDisplayRowName", QStringLiteral("Waterfall levels choice"));
        }
        connect(button, &QPushButton::clicked, this, [this, index, mode]() {
            if (mode() == index) { showDisplayDefaults(); return; }
            const auto setBool = [this](const QString& key, bool enabled, auto setter) {
                applyDisplayDefault(key, enabled ? QStringLiteral("True") : QStringLiteral("False"),
                    [setter, enabled](SpectrumWidget* pan) { (pan->*setter)(enabled); });
            };
            setBool(QStringLiteral("ClarityEnabled"), index == 0, &SpectrumWidget::setClarityEnabled);
            if (index != 0) {
                setBool(QStringLiteral("DisplayWfAgc"), index == 1 || index == 2, &SpectrumWidget::setWfAgcEnabled);
                setBool(QStringLiteral("WaterfallNFAGCEnabled"), index == 2, &SpectrumWidget::setWaterfallNFAGCEnabled);
                setBool(QStringLiteral("DisplayWfUseSpectrumMinMax"), index == 4, &SpectrumWidget::setWfUseSpectrumMinMax);
            }
        });
    }
    column->addWidget(note);
    note->setObjectName(QStringLiteral("setupWaterfallModeNote"));
    const auto seed = [this, buttons, note, mode]() {
        const int selected = mode();
        const QStringList notes = {QStringLiteral("Clarity sets Low and High from the noise floor."), QStringLiteral("Auto sets Low from the weakest signals. You set High."), QStringLiteral("Low is the noise floor plus NF Offset. You set High."), QStringLiteral("You set Low and High."), QStringLiteral("Low and High are dB Min and dB Max, above.")};
        QString text = notes[selected];
        if (selected != 0 && (displayDefaultText(QStringLiteral("DisplayWfBlackLevel")).toInt() != ClarityLevels::kBlackLevelDefault
            || displayDefaultText(QStringLiteral("DisplayWfColorGain")).toInt() != ClarityLevels::kColorGainDefault)) {
            const QStringList trimmed = {QString(), QStringLiteral("Auto sets Low, you set High."), QStringLiteral("Floor sets Low, you set High."), QStringLiteral("You set Low and High."), QStringLiteral("dB Min and dB Max set them.")};
            text = trimmed[selected] + QStringLiteral(" Black Level and Color Gain trim both.");
        } else if (selected == 4 && displayDefaultText(QStringLiteral("DisplayAdjustGridMinToNoiseFloor")) == QStringLiteral("True")) {
            text = QStringLiteral("Low and High are dB Min and dB Max. Track floor moves dB Min.");
        }
        SpectrumWidget* selectedPan = nullptr;
        for (SpectrumWidget* pan : panWidgets()) { if (pan->panIndex() == displayPanIndex()) { selectedPan = pan; break; } }
        const bool keyed = selectedPan && selectedPan->isMoxOverlayActive();
        const QString keyedReason = QStringLiteral("Pan %1 is transmitting on the transmit levels. This chooser is a receive setting and is not used until it stops.").arg(displayPanIndex() + 1);
        note->setText(keyed ? keyedReason : text);
        QList<QWidget*> choices;
        for (QPushButton* button : buttons) { choices.append(button); }
        gateOwnedControls(choices, !keyed, keyedReason);
        for (int index = 0; index < buttons.size(); ++index) { const QSignalBlocker block(buttons[index]); buttons[index]->setChecked(index == selected); }
    };
    for (const QString& key : keys) { followDisplayDefault(key, seed); }
    m_pictureRefreshers.append(seed);
    addContent(group);
    showDisplayDefaults();
}

void DisplayAssemblyPage::addViewChooser()
{
    auto* group = new QGroupBox(QStringLiteral("View"), this);
    auto* column = new QVBoxLayout(group);
    auto* line = new QHBoxLayout;
    column->addLayout(line);
    auto* note = new QLabel(group);
    note->setWordWrap(true);
    column->addWidget(note);
    QVector<QPushButton*> buttons;
    for (const QString& text : {QStringLiteral("2D Waterfall"), QStringLiteral("3D Stacked Trace")}) {
        const int index = buttons.size();
        auto* button = new QPushButton(text, group);
        button->setCheckable(true);
        button->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
        button->setProperty("nereusSetupId", QStringLiteral("display.view.mode.%1").arg(index));
        if (index == 0) {
            button->setProperty("nereusDisplayKeyBase", QStringLiteral("DisplaySpectrumRenderMode"));
            button->setProperty("nereusDisplayRowName", QStringLiteral("View choice"));
        }
        connect(button, &QPushButton::clicked, this, [this, index]() {
            if (displayDefaultText(QStringLiteral("DisplaySpectrumRenderMode")).toInt() == index) { showDisplayDefaults(); return; }
            applyDisplayDefault(QStringLiteral("DisplaySpectrumRenderMode"), QString::number(index),
                [index](SpectrumWidget* pan) { pan->setSpectrumRenderMode(index); });
        });
        buttons.append(button);
        line->addWidget(button);
    }
    followDisplayDefault(QStringLiteral("DisplaySpectrumRenderMode"), [this, buttons, note]() {
        const int selected = displayDefaultText(QStringLiteral("DisplaySpectrumRenderMode")).toInt();
        note->setText(selected == 1 ? QStringLiteral("The 3D rows are in use. The background, the trace and its fill are not drawn in 3D.")
                                    : QStringLiteral("The Background rows are used by this view. The 3D rows are used by the 3D view."));
        for (int index = 0; index < buttons.size(); ++index) { const QSignalBlocker block(buttons[index]); buttons[index]->setChecked(index == selected); }
    });
    addContent(group);
    showDisplayDefaults();
}

void DisplayAssemblyPage::displayPansChanged()
{
    for (SpectrumWidget* pan : panWidgets()) { pan->installEventFilter(this); }
    for (const std::function<void()>& refresh : std::as_const(m_pictureRefreshers)) { refresh(); }
    refreshNumberGates();
    pairNumberRows(this);
    refreshOwnRows();
}

void DisplayAssemblyPage::refreshNumberGates()
{
    const auto field = [this](const QString& id) -> QWidget* {
        for (QWidget* control : findChildren<QWidget*>()) {
            if (control->property("nereusSetupId").toString() != id) { continue; }
            QWidget* owner = control->parentWidget();
            while (owner && !qobject_cast<SetupPage*>(owner)) { owner = owner->parentWidget(); }
            if (owner == this) { return control; }
        }
        return nullptr;
    };
    const auto gate = [field](const QString& id, bool live, const QString& why) {
        QWidget* control = field(id);
        if (!control) { return; }
        // A scoped dependency must not restore an external Core/key gate.
        // These saved-state properties exist only while that gate holds.
        for (const char* external : {"SetupPageSavedStationEnabled", "SetupPageSavedTransmitEnabled", "SetupPageSavedOnAirEnabled"}) {
            if (control->property(external).isValid()) { return; }
        }
        gateOwnedControls({control}, live, why);
        // Number rows use their explicit widget flag to drive the pair and
        // label controller, so inherited-disabled parents can recover.
        if (QWidget* number = qobject_cast<QAbstractSpinBox*>(control)) {
            if (QSlider* slider = qobject_cast<QSlider*>(number->property("nereusPairedNumberSlider").value<QObject*>())) {
                gateOwnedControls({slider}, live, why);
            }
        } else if (qobject_cast<QSlider*>(control)) {
            for (QAbstractSpinBox* number : control->parentWidget()->findChildren<QAbstractSpinBox*>(QString(), Qt::FindDirectChildrenOnly)) {
                gateOwnedControls({number}, live, why);
            }
        }
        const bool rowLive = live && !control->testAttribute(Qt::WA_ForceDisabled);
        QWidget* row = control;
        while (row->parentWidget() && !qobject_cast<QGroupBox*>(row->parentWidget())) { row = row->parentWidget(); }
        if (row->parentWidget()) {
            if (auto* form = qobject_cast<QFormLayout*>(row->parentWidget()->layout())) {
                if (QWidget* label = form->labelForField(row)) { gateOwnedControls({label}, rowLive, why); }
            }
        }
        QList<QWidget*> labels;
        for (QLabel* label : row->findChildren<QLabel*>()) { labels.append(label); }
        if (row->parentWidget()) {
            for (QHBoxLayout* line : row->parentWidget()->findChildren<QHBoxLayout*>()) {
                // Only a horizontal layout that contains this exact row
                // owns its sibling caption. A group's vertical column
                // also contains the row, but its notes belong to the group.
                if (line->indexOf(row) < 0) { continue; }
                for (int index = 0; index < line->count(); ++index) {
                    if (auto* label = qobject_cast<QLabel*>(line->itemAt(index)->widget())) { labels.append(label); }
                }
            }
        }
        gateOwnedControls(labels, rowLive, why);
    };
    const auto on = [this](const char* key) { return displayDefaultText(QString::fromLatin1(key)) == QStringLiteral("True"); };
    if (pageTitle() == QStringLiteral("Spectrum")) {
        gate(QStringLiteral("display.spectrumDefaults.averageTime"),
            displayDefaultText(QStringLiteral("DisplaySpectrumAveraging")).toInt() != int(SpectrumAveraging::None),
            QStringLiteral("Averaging is None, so there is no time to set."));
        gate(QStringLiteral("display.gridScales.dbStep"), on("DisplayGridEnabled"), QStringLiteral("Grid is off, so no grid lines are drawn."));
    } else if (pageTitle() == QStringLiteral("Levels")) {
        SpectrumWidget* selected = nullptr;
        for (SpectrumWidget* pan : panWidgets()) { if (pan->panIndex() == displayPanIndex()) { selected = pan; break; } }
        // The window key edge pauses every receive follower; only the pan
        // drawing the transmit overlay uses the transmit Levels picture.
        const bool transmitting = selected && selected->isMoxOverlayActive();
        const QString why = QStringLiteral("Pan %1 is transmitting. This is a receive setting and is not used until it stops.").arg(displayPanIndex() + 1);
        gate(QStringLiteral("display.levels.blackLevel"), !transmitting, why);
        gate(QStringLiteral("display.levels.colorGain"), !transmitting, why);
        gate(QStringLiteral("display.gridScales.noiseFloorOffset"), !transmitting && on("DisplayAdjustGridMinToNoiseFloor"),
            transmitting ? why : QStringLiteral("Floor Offset is used by Track floor. Pick Track floor to use it."));
        gate(QStringLiteral("display.gridScales.maintainGridRange"), !transmitting && on("DisplayAdjustGridMinToNoiseFloor"),
            transmitting ? why : QStringLiteral("Keep range is used by Track floor. Pick Track floor to use it."));
    } else if (pageTitle() == QStringLiteral("Waterfall")) {
        gate(QStringLiteral("display.waterfallDefaults.averageTime"),
            displayDefaultText(QStringLiteral("DisplayWaterfallAveraging")).toInt() != int(SpectrumAveraging::None),
            QStringLiteral("WF Avg is None, so there is no time to set."));
        gate(QStringLiteral("display.waterfall.nbThreshold"), on("WaterfallBlankingEnabled"), QStringLiteral("NB Blank is off, so no rows are blanked."));
        gate(QStringLiteral("display.waterfallDefaults.timestampMode"), displayDefaultText(QStringLiteral("DisplayWfTimestampPos")).toInt() != 0,
            QStringLiteral("Timestamp is None."));
    } else if (pageTitle() == QStringLiteral("View")) {
        const bool threeD = displayDefaultText(QStringLiteral("DisplaySpectrumRenderMode")).toInt() == 1;
        for (const char* suffix : {"gain", "span", "angle", "speed", "sliceShadow", "floor"}) {
            gate(QStringLiteral("display.threeD.") + QString::fromLatin1(suffix), threeD,
                QStringLiteral("Used by the 3D view. Pick 3D Stacked Trace above to use it."));
        }
    } else if (pageTitle() == QStringLiteral("Peaks & Markers")) {
        for (const char* suffix : {"activePeakHoldTime", "activePeakHoldDropRate", "activePeakHoldFill", "activePeakHoldOnTx", "activePeakHoldColor"}) {
            gate(QStringLiteral("display.spectrumPeaks.") + QString::fromLatin1(suffix), on("DisplayActivePeakHoldEnabled"), QStringLiteral("Peak trace is off."));
        }
        for (const char* suffix : {"peakBlobCount", "peakBlobInsideFilter", "peakBlobHold", "peakBlobColor", "peakBlobTextColor", "peakBlobHoldTime", "peakBlobHoldDrop", "peakBlobFallRate"}) {
            const QString name = QString::fromLatin1(suffix);
            bool live = on("DisplayPeakBlobsEnabled");
            QString why = QStringLiteral("Peak markers are off.");
            if (live && (name == QStringLiteral("peakBlobHoldTime") || name == QStringLiteral("peakBlobHoldDrop") || name == QStringLiteral("peakBlobFallRate"))) {
                live = on("DisplayPeakBlobsHoldEnabled"); why = QStringLiteral("Hold before decay is off.");
            }
            if (live && name == QStringLiteral("peakBlobFallRate")) { live = on("DisplayPeakBlobsHoldDrop"); why = QStringLiteral("Decay after hold is off."); }
            gate(QStringLiteral("display.spectrumPeaks.") + name, live, why);
        }
        for (const char* suffix : {"peakValuePosition", "peakTextDelay"}) {
            gate(QStringLiteral("display.spectrumDefaults.") + QString::fromLatin1(suffix), on("DisplayShowPeakValueOverlay"), QStringLiteral("Peak Value is off."));
        }
        for (const char* suffix : {"noiseFloorShift", "noiseFloorLineWidth", "noiseFloorColor", "noiseFloorTextColor", "noiseFloorFastColor"}) {
            gate(QStringLiteral("display.spectrumDefaults.") + QString::fromLatin1(suffix), on("DisplayShowNoiseFloor"), QStringLiteral("Noise Floor is off."));
        }
        gate(QStringLiteral("display.spectrumDefaults.noiseFloorPosition"), false,
            QStringLiteral("Not used: the noise floor text sits beside its box. The setting is kept but has no visual effect."));
    } else if (pageTitle() == QStringLiteral("Transmit Display")) {
        for (const QString& prefix : {QStringLiteral("pan"), QStringLiteral("wf")}) {
            auto* combo = qobject_cast<QComboBox*>(field(QStringLiteral("display.txDisplay.") + prefix + QStringLiteral("Averaging")));
            if (combo) {
                gate(QStringLiteral("display.txDisplay.") + prefix + QStringLiteral("AvTime"), combo->currentIndex() != 0,
                     QStringLiteral("Averaging is None, so there is no time to set."));
            }
        }
    }
}

void DisplayAssemblyPage::addMissingPictureRows()
{
    const auto groupFor = [this](const QString& title) {
        for (QGroupBox* group : findChildren<QGroupBox*>()) {
            if (group->property("displayAssemblyGroupTitle").toString() == title) { return group; }
        }
        auto* group = new QGroupBox(title, this);
        group->setProperty("displayAssemblyGroupTitle", title);
        auto* column = new QVBoxLayout(group);
        column->setSpacing(6);
        addContent(group);
        return group;
    };
    if (pageTitle() == QStringLiteral("Transmit Display")) {
        QGroupBox* group = groupFor(QStringLiteral("Transmit scale"));
        auto* form = new QFormLayout;
        qobject_cast<QVBoxLayout*>(group->layout())->addLayout(form);
        for (QGroupBox* candidate : findChildren<QGroupBox*>()) {
            if (candidate->title() == QStringLiteral("Waterfall") && contentLayout()->indexOf(candidate) >= 0) {
                contentLayout()->insertWidget(contentLayout()->indexOf(candidate), group);
                break;
            }
        }
        auto* maximum = new QSpinBox(group);
        auto* minimum = new QSpinBox(group);
        maximum->setProperty("nereusSetupId", "display.txDisplay.dbMax");
        minimum->setProperty("nereusSetupId", "display.txDisplay.dbMin");
        maximum->setRange(-200, 200); minimum->setRange(-200, 200);
        maximum->setSuffix(QStringLiteral(" dBm")); minimum->setSuffix(QStringLiteral(" dBm"));
        minimum->setProperty("nativeNumberSliderMinimum", -200);
        minimum->setProperty("nativeNumberSliderMaximum", 200);
        maximum->setToolTip(QStringLiteral("This computer's transmit dB Max. The stored transmit range is 10 to 200 dB."));
        minimum->setToolTip(QStringLiteral("This computer's transmit dB Min. The stored transmit range is 10 to 200 dB."));
        form->addRow(QStringLiteral("dB Max:"), maximum);
        form->addRow(QStringLiteral("dB Min:"), minimum);
        const auto seed = [this, maximum, minimum]() {
            const QList<SpectrumWidget*> pans = panWidgets();
            const float ref = pans.isEmpty() ? AppSettings::instance().value(QStringLiteral("DisplayTxGridRefLevel"), 20).toFloat() : pans.first()->transmitRefLevel();
            const float range = pans.isEmpty() ? AppSettings::instance().value(QStringLiteral("DisplayTxGridDynamicRange"), 100).toFloat() : pans.first()->transmitDynamicRange();
            const QSignalBlocker maxBlock(maximum), minBlock(minimum);
            // Imported shared pairs remain authoritative. A derived endpoint
            // outside the HTML slider range is still shown by the number.
            minimum->setMinimum(std::min(-200, qRound(ref - range)));
            maximum->setValue(qRound(ref)); minimum->setValue(qRound(ref - range));
            if (auto* slider = minimum->parentWidget()->findChild<QSlider*>(QString(), Qt::FindDirectChildrenOnly)) {
                const QSignalBlocker block(slider); slider->setValue(minimum->value());
            }
        };
        const auto edit = [this, maximum, minimum, seed]() {
            const QList<SpectrumWidget*> pans = panWidgets();
            if (!pans.isEmpty()) { pans.first()->setTransmitDbmRange(minimum->value(), maximum->value()); }
            seed(); // Display the actual backend-clamped endpoint, no UI echo.
        };
        connect(maximum, &QSpinBox::valueChanged, this, [edit](int) { edit(); });
        connect(minimum, &QSpinBox::valueChanged, this, [edit](int) { edit(); });
        followDisplayDefault(QStringLiteral("DisplayTxGridRefLevel"), seed);
        followDisplayDefault(QStringLiteral("DisplayTxGridDynamicRange"), seed);
        m_pictureRefreshers.append(seed);
        seed();
    }
    if (pageTitle() == QStringLiteral("Spectrum")) {
        QGroupBox* group = groupFor(QStringLiteral("Grid"));
        auto* combo = new QComboBox(group);
        combo->setProperty("nereusSetupId", "display.spectrum.gridSpacing");
        combo->setProperty("nereusDisplayKeyBase", QStringLiteral("DisplayFreqGridSpacing"));
        combo->addItem(QStringLiteral("Auto"), 0);
        for (const int hz : SpectrumWidget::kFreqGridSpacingsHz) { combo->addItem(QStringLiteral("%1 kHz").arg(hz / 1000), hz); }
        addLabeledCombo(group->layout(), QStringLiteral("Grid Spacing:"), combo);
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo](int index) {
            const int hz = combo->itemData(index).toInt();
            applyDisplayDefault(QStringLiteral("DisplayFreqGridSpacing"), QString::number(hz),
                [hz](SpectrumWidget* pan) { pan->setFreqGridSpacingHz(hz); });
        });
        followDisplayDefault(QStringLiteral("DisplayFreqGridSpacing"), [this, combo]() {
            const QSignalBlocker block(combo);
            combo->setCurrentIndex(combo->findData(displayDefaultText(QStringLiteral("DisplayFreqGridSpacing")).toInt()));
        });
        followDisplayDefault(QStringLiteral("DisplayGridEnabled"), [this, combo]() {
            gateOwnedControls({combo}, displayDefaultText(QStringLiteral("DisplayGridEnabled")) == QStringLiteral("True"), QStringLiteral("Grid is off, so no grid lines are drawn."));
        });
        group = groupFor(QStringLiteral("Scales and labels"));
        auto* textSize = new QComboBox(group);
        textSize->setProperty("nereusSetupId", "display.spectrum.scaleText");
        textSize->setProperty("nereusDisplayKeyBase", QStringLiteral("DisplayFreqScaleFontPx"));
        // Same pixel range as SpectrumWidget::setFreqScaleFontPx and the pill.
        constexpr int kScaleTextMinPx = 8;
        constexpr int kScaleTextMaxPx = 14;
        for (int pixels = kScaleTextMinPx; pixels <= kScaleTextMaxPx; ++pixels) {
            textSize->addItem(QStringLiteral("%1 px").arg(pixels), pixels);
        }
        connect(textSize, &QComboBox::currentIndexChanged, this, [this, textSize](int index) {
            const int pixels = textSize->itemData(index).toInt();
            applyDisplayDefault(QStringLiteral("DisplayFreqScaleFontPx"), QString::number(pixels),
                [pixels](SpectrumWidget* pan) { pan->setFreqScaleFontPx(pixels); });
        });
        followDisplayDefault(QStringLiteral("DisplayFreqScaleFontPx"), [this, textSize]() {
            const QSignalBlocker block(textSize);
            textSize->setCurrentIndex(textSize->findData(displayDefaultText(QStringLiteral("DisplayFreqScaleFontPx")).toInt()));
        });
        addLabeledCombo(group->layout(), QStringLiteral("Scale Text:"), textSize);
    } else if (pageTitle() == QStringLiteral("Levels")) {
        QGroupBox* group = groupFor(QStringLiteral("Waterfall levels"));
        const auto addTrim = [this, group](bool black) {
            const QString key = black ? QStringLiteral("DisplayWfBlackLevel") : QStringLiteral("DisplayWfColorGain");
            auto row = makeSliderRow(black ? ClarityLevels::kBlackLevelMin : ClarityLevels::kColorGainMin,
                black ? ClarityLevels::kBlackLevelMax : ClarityLevels::kColorGainMax,
                black ? ClarityLevels::kBlackLevelDefault : ClarityLevels::kColorGainDefault, QString(), group);
            row.slider->setProperty("nereusSetupId", black ? "display.levels.blackLevel" : "display.levels.colorGain");
            bindDefaultSlider(row.slider, row.spin, key, [black](SpectrumWidget* pan, int value) {
                if (black) { pan->setWfBlackLevel(value); } else { pan->setWfColorGain(value); }
            });
            row.spin->hide();
            auto* value = new QLabel(row.container);
            qobject_cast<QHBoxLayout*>(row.container->layout())->addWidget(value);
            const auto show = [this, key, black, slider = row.slider, spin = row.spin, value]() {
                const int position = displayDefaultText(key).toInt();
                const QSignalBlocker sliderBlock(slider);
                const QSignalBlocker spinBlock(spin);
                slider->setValue(position);
                spin->setValue(position);
                const bool clarity = displayDefaultText(QStringLiteral("ClarityEnabled")) == QStringLiteral("True");
                // Approved live reference fmt(), lines768–769: a Clarity
                // offset/span, otherwise signed trims centered on57/60.
                const int db = black ? (clarity
                    ? static_cast<int>(std::floor((position - ClarityLevels::kBlackLevelAtNoise) * ClarityLevels::kBlackLevelDbPerStep))
                    : static_cast<int>(std::floor((position - ClarityLevels::kBlackLevelDefault) * ClarityLevels::kBlackLevelDbPerStep + 0.5)))
                    : (clarity ? qRound(ClarityLevels::colorSpanDb(position))
                               : static_cast<int>(std::floor((position - ClarityLevels::kColorGainDefault) * 0.3 + 0.5)));
                value->setText((black && clarity ? QStringLiteral("N") : QString())
                    + (black || !clarity ? (db >= 0 ? QStringLiteral("+") : QString()) : QString())
                    + QString::number(db) + QStringLiteral(" dB"));
            };
            connect(row.slider, &QSlider::valueChanged, value, show);
            followDisplayDefault(key, show);
            m_pictureRefreshers.append(show);
            auto* line = new QHBoxLayout;
            line->addWidget(new QLabel(black ? QStringLiteral("Black Level:") : QStringLiteral("Color Gain:"), group));
            line->addWidget(row.container, 1);
            qobject_cast<QVBoxLayout*>(group->layout())->insertLayout(2 + (black ? 0 : 1), line);
        };
        addTrim(true);
        addTrim(false);
        auto* retune = new QPushButton(QStringLiteral("Re-tune"), group);
        retune->setProperty("nereusSetupId", "display.levels.retune");
        connect(retune, &QPushButton::clicked, this, [this]() { emit clarityRetuneRequested(displayPanIndex()); });
        qobject_cast<QVBoxLayout*>(group->layout())->insertWidget(7, retune);
        const auto gate = [this, retune]() {
            SpectrumWidget* selected = nullptr;
            for (SpectrumWidget* pan : panWidgets()) { if (pan->panIndex() == displayPanIndex()) { selected = pan; break; } }
            const bool keyed = selected && selected->isMoxOverlayActive();
            const auto status = displayClarityStatus(displayPanIndex());
            retune->setProperty("clarityState", status && status->paused ? QStringLiteral("paused")
                : status && status->active ? QStringLiteral("active") : QStringLiteral("off"));
            retune->setProperty("claritySourceAvailable", status && status->sourceAvailable);
            gateOwnedControls({retune}, !keyed && displayDefaultText(QStringLiteral("ClarityEnabled")) == QStringLiteral("True"),
                keyed ? QStringLiteral("Re-tune works on receive levels. Use it when this pan stops transmitting.")
                      : QStringLiteral("Re-tune makes Clarity measure the noise floor again. Pick Clarity to use it."));
        };
        m_pictureRefreshers.append(gate);
        gate();
    } else if (pageTitle() == QStringLiteral("Waterfall")) {
        QGroupBox* group = groupFor(QStringLiteral("Drawing"));
        auto* blank = new QCheckBox(QStringLiteral("NB Blank"), group);
        blank->setProperty("nereusSetupId", "display.waterfall.nbBlank");
        bindDefaultToggle(blank, QStringLiteral("WaterfallBlankingEnabled"), [](SpectrumWidget* pan, bool enabled) { pan->setWaterfallBlankingEnabled(enabled); });
        group->layout()->addWidget(blank);
        auto* threshold = new QSpinBox(group);
        threshold->setProperty("nereusSetupId", "display.waterfall.nbThreshold");
        // SpectrumWidget.cpp kWfBlankingThresholdDbMin/Max; same as pill.
        constexpr int kThresholdMinDb = 1;
        constexpr int kThresholdMaxDb = 20;
        threshold->setRange(kThresholdMinDb, kThresholdMaxDb);
        threshold->setSuffix(QStringLiteral(" dB"));
        bindDefaultSpin(threshold, QStringLiteral("WaterfallBlankingThresholdDb"), [](SpectrumWidget* pan, int db) { pan->setWaterfallBlankingThresholdDb(db); });
        addLabeledSpinner(group->layout(), QStringLiteral("NB threshold:"), threshold);
        followDisplayDefault(QStringLiteral("WaterfallBlankingEnabled"), [this, threshold]() {
            gateOwnedControls({threshold}, displayDefaultText(QStringLiteral("WaterfallBlankingEnabled")) == QStringLiteral("True"), QStringLiteral("NB Blank is off, so no rows are blanked."));
        });
    }
    const auto noteFor = [groupFor](const QString& groupName, const QString& objectName) {
        QGroupBox* group = groupFor(groupName);
        auto* label = new QLabel(group);
        label->setObjectName(objectName);
        label->setWordWrap(true);
        group->layout()->addWidget(label);
        return label;
    };
    if (pageTitle() == QStringLiteral("Spectrum")) {
        noteFor(QStringLiteral("Peak hold"), QStringLiteral("setupPeakHoldNote"))->setText(
            QStringLiteral("Peak Hold keeps the highest level seen until Peak Delay runs out. For a peak line that falls back slowly, see Peaks & Markers →"));
        noteFor(QStringLiteral("Reset"), QStringLiteral("setupSmoothNote"))->setText(
            QStringLiteral("It also sets Scheme and Rate on Waterfall → and changes Manual waterfall levels to Auto on Levels →"));
        const auto gatePicture = [this]() {
            const bool threeD = displayDefaultText(QStringLiteral("DisplaySpectrumRenderMode")).toInt() == 1;
            const bool fill = displayDefaultText(QStringLiteral("DisplayPanFill")) == QStringLiteral("True");
            const QHash<QString, QString> drawings = {
                {"display.spectrumDefaults.lineWidth", "the trace"}, {"display.spectrum.traceColor", "the trace"},
                {"display.spectrumDefaults.panFill", "the fill"}, {"display.spectrumDefaults.fillAlpha", "the fill"},
                {"display.spectrum.fillColor", "the fill"}, {"display.spectrumDefaults.gradient", "the fill"},
                {"display.spectrumDefaults.peakHold", "the peak hold line"}, {"display.spectrumDefaults.peakDelay", "the peak hold line"},
                {"display.gridScales.zeroLine", "the zero line"}};
            for (QWidget* control : findChildren<QWidget*>()) {
                const QString id = control->property("nereusSetupId").toString();
                if (!drawings.contains(id)) { continue; }
                bool live = !threeD;
                QString why = QStringLiteral("The 3D view does not draw %1. Pick 2D Waterfall on View to use it.").arg(drawings.value(id));
                if (live && id == QStringLiteral("display.spectrumDefaults.fillAlpha") && !fill) {
                    live = false; why = QStringLiteral("Fill is off.");
                }
                if (live && id == QStringLiteral("display.spectrum.fillColor") && !fill
                    && !displayDefaultText(QStringLiteral("DisplayTraceColor")).isEmpty()) {
                    live = false; why = QStringLiteral("Fill is off and Trace Color has its own color, so nothing is drawn in Fill Color.");
                }
                if (live && id == QStringLiteral("display.spectrumDefaults.peakDelay")
                    && displayDefaultText(QStringLiteral("DisplayPeakHoldEnabled")) != QStringLiteral("True")) {
                    live = false; why = QStringLiteral("Peak Hold is off.");
                }
                gateOwnedControls({control}, live, why);
            }
        };
        m_pictureRefreshers.append(gatePicture);
    } else if (pageTitle() == QStringLiteral("Levels")) {
        QLabel* bandNote = noteFor(QStringLiteral("Spectrum scale"), QStringLiteral("setupBandScaleNote"));
        qobject_cast<QVBoxLayout*>(bandNote->parentWidget()->layout())->insertWidget(4, bandNote);
        m_pictureRefreshers.append([this, bandNote]() {
            PanadapterModel* band = bandGridModel();
            bandNote->setText(QStringLiteral("dB Max and dB Min are kept per band (%1). Manual or Track floor, Floor Offset and Keep range are the same on every band.")
                .arg(band ? bandLabel(band->band()) : QStringLiteral("no band")));
        });
        QLabel* transmitNote = noteFor(QStringLiteral("While transmitting"), QStringLiteral("setupTransmitLevelsNote"));
        transmitNote->setText(QStringLiteral("A pan that transmits is drawn with the transmit scale and the transmit levels until it stops. Transmit Display →"));
    } else if (pageTitle() == QStringLiteral("Waterfall")) {
        QLabel* delay = noteFor(QStringLiteral("Drawing"), QStringLiteral("setupWaterfallDelayNote"));
        QLabel* rewind = noteFor(QStringLiteral("Time"), QStringLiteral("setupWaterfallRewindNote"));
        qobject_cast<QVBoxLayout*>(delay->parentWidget()->layout())->insertWidget(2, delay);
        m_pictureRefreshers.append([this, delay, rewind]() {
            SpectrumWidget* selected = nullptr;
            for (SpectrumWidget* pan : panWidgets()) { if (pan->panIndex() == displayPanIndex()) { selected = pan; break; } }
            if (!selected) {
                delay->setText(displayPanIndex() < 0 ? QStringLiteral("Delay is set pan by pan by the waterfall height and Rate.")
                    : QStringLiteral("Open this pan to see the Delay for its waterfall height and Rate."));
                rewind->setText(QString());
                return;
            }
            const int rows = selected->visibleWaterfallRows();
            delay->setText(QStringLiteral("Delay: %1 s. A row takes that long to cross a waterfall %2 rows tall at this Rate.")
                .arg(double(rows) * double(selected->wfUpdatePeriodMs()) / 1000.0, 0, 'f', 1).arg(rows));
            const qint64 kept = selected->effectiveWaterfallHistorySeconds();
            const QString duration = QStringLiteral("%1:%2").arg(kept / 60).arg(kept % 60, 2, 10, QLatin1Char('0'));
            rewind->setText(kept < selected->waterfallHistoryMs() / 1000
                ? QStringLiteral("At this Rate the waterfall keeps %1 (%2 rows). Slow Rate to keep more.").arg(duration).arg(selected->waterfallHistoryCapacityRows())
                : QStringLiteral("Keeps %1 at this Rate.").arg(duration));
        });
    } else if (pageTitle() == QStringLiteral("View")) {
        QLabel* speed = noteFor(QStringLiteral("3D"), QStringLiteral("setupThreeDSpeedNote"));
        qobject_cast<QVBoxLayout*>(speed->parentWidget()->layout())->insertWidget(4, speed);
        m_pictureRefreshers.append([this, speed]() {
            const int divider = displayDefaultText(QStringLiteral("Display3DSpeed")).toInt();
            const int ms = displayDefaultText(QStringLiteral("DisplayWfUpdatePeriodMs")).toInt() * divider;
            speed->setText((divider == 0 ? QStringLiteral("Match: the 3D view covers the same time as the waterfall, which Rate sets.")
                : divider == 1 ? QStringLiteral("Every waterfall row is a 3D row: one every %1 ms at this Rate, the fastest.").arg(ms)
                : QStringLiteral("Each 3D row covers %1 waterfall rows: one every %2 ms at this Rate.").arg(divider).arg(ms))
                + QStringLiteral(" Rate is on Waterfall →"));
            const bool threeD = displayDefaultText(QStringLiteral("DisplaySpectrumRenderMode")).toInt() == 1;
            for (QWidget* control : findChildren<QWidget*>()) {
                const QString id = control->property("nereusSetupId").toString();
                if (id.startsWith(QStringLiteral("display.threeD.")) && id != QStringLiteral("display.threeD.reset")) {
                    gateOwnedControls({control}, threeD, QStringLiteral("Used by the 3D view. Pick 3D Stacked Trace above to use it."));
                }
            }
        });
        noteFor(QStringLiteral("3D"), QStringLiteral("setupThreeDResetNote"))->setText(QStringLiteral("Reset also sets the view and this band's 3D Floor."));
    }
    if (pageTitle() == QStringLiteral("Peaks & Markers")) {
        auto* back = new QPushButton(QStringLiteral("← Spectrum"), this);
        connect(back, &QPushButton::clicked, this, [this]() { emit pageRequested(QStringLiteral("Spectrum")); });
        addContent(back);
    }
    showDisplayDefaults();
    displayPansChanged();
}

void DisplayAssemblyPage::addFftSizingChooser(QWidget* source)
{
    auto* original = source->findChild<QComboBox*>(QStringLiteral("setupFftSizingMode"));
    if (!original) { return; }
    auto* holder = new QGroupBox(QStringLiteral("FFT size"), this);
    holder->setProperty("displayAssemblyGroupTitle", QStringLiteral("FFT size"));
    auto* column = new QVBoxLayout(holder);
    auto* line = new QHBoxLayout;
    column->addLayout(line);
    auto* note = new QLabel(holder);
    note->setWordWrap(true);
    column->addWidget(note);
    QVector<QPushButton*> buttons;
    for (int index = 0; index < original->count(); ++index) {
        auto* button = new QPushButton(original->itemText(index), holder);
        button->setCheckable(true);
        button->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
        line->addWidget(button);
        buttons.append(button);
        connect(button, &QPushButton::clicked, original, [original, index]() { original->setCurrentIndex(index); });
    }
    const auto seed = [buttons, original, note]() {
        const auto mode = static_cast<ControlRanges::DisplayFftSizingMode>(original->currentData().toInt());
        note->setText(mode == ControlRanges::DisplayFftSizingMode::Auto ? QStringLiteral("The zoom picks the FFT size.")
            : mode == ControlRanges::DisplayFftSizingMode::HzPerBin ? QStringLiteral("The bin width you ask for picks the FFT size.")
                : QStringLiteral("You pick the FFT size."));
        for (int index = 0; index < buttons.size(); ++index) {
            const QSignalBlocker block(buttons[index]); buttons[index]->setChecked(index == original->currentIndex()); buttons[index]->setEnabled(original->isEnabled());
        }
    };
    connect(original, &QComboBox::currentIndexChanged, holder, seed);
    if (auto* owner = qobject_cast<SpectrumDefaultsPage*>(source)) {
        connect(owner, &SpectrumDefaultsPage::fftSizingPresentationChanged, this, [this, seed]() {
            seed();
            // A blocked mode seed may enable the original number while
            // its parent pair is still disabled, which emits no event.
            // Refresh once after the complete original FFT presentation.
            refreshAssemblyPresentation();
        });
    }
    seed();
    addContent(holder);
}

} // namespace NereusSDR

namespace NereusSDR {

namespace {

class NativeNumberPairController final : public QObject {
public:
    NativeNumberPairController(QAbstractSpinBox* number, QSlider* slider, QWidget* pair,
        double scale, QWidget* field, QWidget* page)
        : QObject(pair), m_number(number), m_slider(slider), m_pair(pair), m_scale(scale)
    {
        setProperty("nativeNumberPairController", true);
        number->setProperty("nereusPairedNumberSlider", QVariant::fromValue(static_cast<QObject*>(slider)));
        number->setProperty("nereusPairedNumberScale", scale);
        connect(slider, &QObject::destroyed, number, [number]() {
            number->setProperty("nereusPairedNumberSlider", QVariant());
        });
        number->installEventFilter(this);
        slider->installEventFilter(this);
        if (auto* form = qobject_cast<QFormLayout*>(field->parentWidget()->layout())) {
            if (auto* label = qobject_cast<QLabel*>(form->labelForField(field))) { m_labels.append(label); }
        }
        for (QLabel* label : field->parentWidget()->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
            // A relocated QHBox row's label sits beside the pair; group
            // labels belong to other rows and are handled by QForm above.
            if (!qobject_cast<QGroupBox*>(field->parentWidget())) { m_labels.append(label); }
        }
        if (auto* integer = qobject_cast<QSpinBox*>(number)) {
            connect(integer, &QSpinBox::valueChanged, this, [this](int) { sync(); });
        } else if (auto* decimal = qobject_cast<QDoubleSpinBox*>(number)) {
            connect(decimal, &QDoubleSpinBox::valueChanged, this, [this](double) { sync(); });
        }
        // A blocked parent spin still quietly updates its internal editor.
        // Reading its final value here never calls a settings setter.
        if (QLineEdit* edit = number->findChild<QLineEdit*>()) {
            connect(edit, &QLineEdit::textChanged, this, [this]() { sync(); });
        }
        // Original feature callbacks run first. This also handles enabling
        // a child while its pair is inherited-disabled (Qt sends no event).
        for (QAbstractButton* choice : page->findChildren<QAbstractButton*>()) {
            connect(choice, &QAbstractButton::toggled, this, [this](bool) { sync(); });
        }
        for (QComboBox* choice : page->findChildren<QComboBox*>()) {
            connect(choice, &QComboBox::currentIndexChanged, this, [this](int) { sync(); });
        }
        sync();
    }

    void sync()
    {
        if (m_syncing || !m_number || !m_slider || !m_pair) { return; }
        const QScopedValueRollback<bool> guard(m_syncing, true);
        const double value = qobject_cast<QSpinBox*>(m_number.data())
            ? qobject_cast<QSpinBox*>(m_number.data())->value()
            : qobject_cast<QDoubleSpinBox*>(m_number.data())->value();
        const QSignalBlocker block(m_slider.data());
        m_slider->setValue(qRound(value * m_scale));
        const bool live = !m_number->testAttribute(Qt::WA_ForceDisabled)
            && !m_slider->testAttribute(Qt::WA_ForceDisabled) && !m_number->isReadOnly();
        m_pair->setEnabled(live);
        m_slider->setToolTip(m_number->toolTip());
        for (QLabel* label : std::as_const(m_labels)) { if (label) { label->setEnabled(live); } }
    }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::EnabledChange || event->type() == QEvent::Show) { sync(); }
        return QObject::eventFilter(watched, event);
    }
private:
    QPointer<QAbstractSpinBox> m_number;
    QPointer<QSlider> m_slider;
    QPointer<QWidget> m_pair;
    QList<QPointer<QLabel>> m_labels;
    double m_scale;
    bool m_syncing{false};
};

} // namespace

void DisplayAssemblyPage::pairNumberRows(QWidget* page)
{
    auto* setup = qobject_cast<SetupPage*>(page);
    const QString title = setup ? setup->pageTitle() : QString();
    for (QGroupBox* group : page->findChildren<QGroupBox*>()) {
        QWidget* owner = group->parentWidget();
        while (owner && !qobject_cast<SetupPage*>(owner)) { owner = owner->parentWidget(); }
        if (owner != page || group->property("nativeOwnerLabelReady").toBool()) { continue; }
        QString tag;
        if (title == QStringLiteral("FFT & Engine")) {
            tag = group->title() == QStringLiteral("This computer") ? QStringLiteral("THIS COMPUTER") : QStringLiteral("CORE");
        } else if (title == QStringLiteral("Transmit Display")) {
            if (group->title() == QStringLiteral("Transmit scale") || group->title() == QStringLiteral("Waterfall Amplitude Scale")) { tag = QStringLiteral("THIS COMPUTER"); }
            else if (group->title() == QStringLiteral("FFT") || group->title() == QStringLiteral("Panadapter") || group->title() == QStringLiteral("Waterfall")) { tag = QStringLiteral("CORE"); }
        } else if (title == QStringLiteral("Multimeter") && (group->title() == QStringLiteral("Signal Units") || group->title() == QStringLiteral("Signal History"))) { tag = QStringLiteral("THIS COMPUTER"); }
        if (tag.isEmpty()) { continue; }
        auto* label = new QLabel(tag, group);
        label->setObjectName(QStringLiteral("setupDisplayGroupOwner"));
        if (auto* form = qobject_cast<QFormLayout*>(group->layout())) { form->insertRow(0, label); }
        else if (auto* column = qobject_cast<QVBoxLayout*>(group->layout())) { column->insertWidget(0, label); }
        else if (auto* grid = qobject_cast<QGridLayout*>(group->layout())) { grid->addWidget(label, grid->rowCount(), 0, 1, grid->columnCount()); }
        else { group->layout()->addWidget(label); }
        group->setProperty("nativeOwnerLabelReady", true);
    }
    if (title == QStringLiteral("Multimeter")) {
        for (QWidget* control : page->findChildren<QWidget*>()) {
            if (control->property("nereusSetupId").toString() == QStringLiteral("display.multimeter.pollingDelay") && !control->property("nativeOwnerTooltipReady").toBool()) {
                control->setToolTip(control->toolTip() + QStringLiteral(" The Core holds this."));
                control->setProperty("nativeOwnerTooltipReady", true);
            }
        }
    }
    for (QAbstractSpinBox* number : page->findChildren<QAbstractSpinBox*>()) {
        QWidget* owner = number->parentWidget();
        while (owner && !qobject_cast<SetupPage*>(owner)) { owner = owner->parentWidget(); }
        if (owner != page || number->window() != page->window()
            || (number->isHidden() && number->testAttribute(Qt::WA_WState_ExplicitShowHide))
            || number->property("nativeNumberPairReady").toBool()) { continue; }
        auto* integer = qobject_cast<QSpinBox*>(number);
        auto* decimal = qobject_cast<QDoubleSpinBox*>(number);
        if (!integer && !decimal) { continue; }
        QWidget* pair = number->parentWidget();
        QWidget* field = pair;
        const QList<QSlider*> existing = pair->findChildren<QSlider*>(QString(), Qt::FindDirectChildrenOnly);
        QSlider* slider = existing.size() == 1 ? existing.first() : nullptr;
        const double scale = decimal ? std::pow(10.0, decimal->decimals()) : 1.0;
        if (!slider) {
            QWidget* originalParent = number->parentWidget();
            if (!originalParent || !originalParent->layout()) { continue; }
            pair = new QWidget(originalParent);
            pair->setObjectName(QStringLiteral("setupNumberPair.") + number->property("nereusSetupId").toString());
            auto* line = new QHBoxLayout(pair);
            line->setContentsMargins(0, 0, 0, 0);
            line->setSpacing(8); // Existing SetupHelpers slider+number row spacing.
            slider = new QSlider(Qt::Horizontal, pair);
            slider->setStyleSheet(Style::kSliderStyle);
            slider->setRange(qRound((integer ? integer->minimum() : decimal->minimum()) * scale),
                             qRound((integer ? integer->maximum() : decimal->maximum()) * scale));
            if (number->property("nativeNumberSliderMinimum").isValid()) { slider->setMinimum(number->property("nativeNumberSliderMinimum").toInt()); }
            if (number->property("nativeNumberSliderMaximum").isValid()) { slider->setMaximum(number->property("nativeNumberSliderMaximum").toInt()); }
            const QString key = number->property("nereusDisplayKeyBase").toString();
            if (key == QStringLiteral("DisplaySpectrumAverageTimeMs") || key == QStringLiteral("DisplayWaterfallAverageTimeMs")) {
                slider->setMaximum(ControlRanges::kDisplayAvgTimePillMaxMs);
            }
            slider->setSingleStep(std::max(1, qRound((integer ? integer->singleStep() : decimal->singleStep()) * scale)));
            slider->setPageStep(slider->singleStep() * 10);
            slider->setObjectName(QStringLiteral("setupNumberSlider.") + number->property("nereusSetupId").toString());
            slider->setToolTip(number->toolTip());
            // Keep the original spin and callback; the slider uses that
            // one write path rather than creating a second setting binding.
            QLayoutItem* old = originalParent->layout()->replaceWidget(number, pair);
            const std::unique_ptr<QLayoutItem> oldItem(old);
            number->setParent(pair);
            line->addWidget(slider, 1);
            line->addWidget(number);
            number->show();
            pair->show();
            field = pair;
            connect(slider, &QSlider::valueChanged, number, [integer, decimal, scale](int raw) {
                if (integer) { integer->setValue(raw); }
                else { decimal->setValue(double(raw) / scale); }
            });
        }
        number->setProperty("nativeNumberPairReady", true);
        new NativeNumberPairController(number, slider, pair, scale, field, page);
    }
    if (title == QStringLiteral("Multimeter")) {
        const QHash<QString, QString> captions = {
            {QStringLiteral("display.multimeter.pollingDelay"), QStringLiteral("the Core holds this")},
            {QStringLiteral("display.multimeter.showDecimal"), QStringLiteral("this computer holds this")}};
        for (QWidget* control : page->findChildren<QWidget*>()) {
            const QString id = control->property("nereusSetupId").toString();
            if (!captions.contains(id) || page->findChild<QLabel*>(QStringLiteral("setupDisplayRowOwner.") + id)) { continue; }
            QWidget* field = control->property("nativeNumberPairReady").toBool() ? control->parentWidget() : control;
            for (QBoxLayout* row : page->findChildren<QBoxLayout*>()) {
                const int index = row->indexOf(field);
                if (index < 0) { continue; }
                auto* label = new QLabel(captions.value(id), field->parentWidget());
                label->setObjectName(QStringLiteral("setupDisplayRowOwner.") + id);
                row->insertWidget(index + 1, label);
                break;
            }
        }
    }
    for (QObject* object : page->findChildren<QObject*>()) {
        if (object->property("nativeNumberPairController").toBool()) {
            static_cast<NativeNumberPairController*>(object)->sync();
        }
    }
}

void DisplayAssemblyPage::positionOwnRows()
{
    for (QPushButton* dot : std::as_const(m_ownDots)) {
        if (!dot) { continue; }
        QWidget* anchor = qobject_cast<QWidget*>(dot->property("setupOwnAnchor").value<QObject*>());
        if (!anchor) { continue; }
        if (dot->parentWidget() == anchor) {
            dot->move(std::max(0, anchor->width() - dot->width()), 0);
        } else {
            const QPoint origin = dot->parentWidget()->mapFromGlobal(anchor->mapToGlobal(QPoint()));
            dot->move(std::max(0, origin.x() - 8), origin.y() + (anchor->height() - dot->height()) / 2);
        }
        dot->raise();
    }
}

bool DisplayAssemblyPage::eventFilter(QObject* watched, QEvent* event)
{
    if (qobject_cast<SpectrumWidget*>(watched) && event->type() == QEvent::Resize) {
        QTimer::singleShot(0, this, [this]() { displayPansChanged(); });
    }
    if (event->type() == QEvent::Resize || event->type() == QEvent::Show
        || event->type() == QEvent::LayoutRequest) {
        positionOwnRows();
    }
    return SetupPage::eventFilter(watched, event);
}

void DisplayAssemblyPage::refreshOwnRows()
{
    if (m_refreshingOwnRows) { return; }
    const QScopedValueRollback<bool> guard(m_refreshingOwnRows, true);
    // Direct float/colour bindings retain their original formatting and
    // callbacks; this table names only their already accepted picture keys.
    static const QHash<QString, QString> directKeys = {
        {"display.spectrumDefaults.lineWidth", "DisplayLineWidth"},
        {"display.spectrumDefaults.fillAlpha", "DisplayFftFillAlpha"},
        {"display.spectrumDefaults.calOffset", "DisplayCalOffset"},
        {"display.spectrum.traceColor", "DisplayTraceColor"},
        {"display.spectrum.fillColor", "DisplayFillColor"},
        {"display.spectrumDefaults.noiseFloorShift", "DisplayNoiseFloorShiftDb"},
        {"display.spectrumDefaults.noiseFloorLineWidth", "DisplayNoiseFloorLineWidth"},
        {"display.spectrumDefaults.noiseFloorColor", "DisplayNoiseFloorColor"},
        {"display.spectrumDefaults.noiseFloorTextColor", "DisplayNoiseFloorTextColor"},
        {"display.spectrumDefaults.noiseFloorFastColor", "DisplayNoiseFloorFastColor"},
        {"display.spectrumPeaks.activePeakHoldColor", "DisplayActivePeakHoldColor"},
        {"display.spectrumPeaks.peakBlobColor", "DisplayPeakBlobColor"},
        {"display.spectrumPeaks.peakBlobTextColor", "DisplayPeakBlobTextColor"},
        {"display.view.backgroundColor", "DisplayBackgroundColor"},
        {"display.view.backgroundImage", "DisplayBackgroundImage"},
        {"display.view.backgroundOpacity", "DisplayBackgroundOpacity"}};
    SpectrumWidget* selected = nullptr;
    for (SpectrumWidget* pan : panWidgets()) {
        if (pan->panIndex() == displayPanIndex()) { selected = pan; break; }
    }
    const QStringList pageKeys = displayPageKeyBases();
    QSet<QString> seen;
    for (QWidget* control : findChildren<QWidget*>()) {
        bool hiddenOwner = false;
        for (QWidget* parent = control->parentWidget(); parent && parent != this; parent = parent->parentWidget()) {
            hiddenOwner = hiddenOwner || parent->objectName() == QStringLiteral("displayBoundSourceOwners");
        }
        if (hiddenOwner) { continue; }
        const QString id = control->property("nereusSetupId").toString();
        QStringList keys = control->property("nereusDisplayRowKeys").toStringList();
        QString logicalKey = control->property("nereusDisplayKeyBase").toString();
        if (logicalKey.isEmpty()) { logicalKey = directKeys.value(id); }
        if (keys.isEmpty() && !logicalKey.isEmpty()) { keys.append(logicalKey); }
        if (keys.isEmpty()) { continue; }
        if (logicalKey.isEmpty()) { logicalKey = QStringLiteral("WaterfallLevelsMode"); }
        bool accepted = true;
        for (const QString& key : keys) { accepted = accepted && pageKeys.contains(key); }
        if (!accepted || seen.contains(logicalKey)) { continue; }
        seen.insert(logicalKey);
        QGroupBox* group = nullptr;
        for (QWidget* parent = control->parentWidget(); parent && parent != this; parent = parent->parentWidget()) {
            if ((group = qobject_cast<QGroupBox*>(parent))) { break; }
        }
        if (!group) { continue; }
        QWidget* row = control;
        while (row->parentWidget() && row->parentWidget() != group) { row = row->parentWidget(); }
        QString name = control->property("nereusDisplayRowName").toString();
        if (name.isEmpty()) {
            if (auto* button = qobject_cast<QAbstractButton*>(control)) { name = button->text(); }
            if (name.isEmpty()) {
                for (QLabel* label : row->findChildren<QLabel*>()) {
                    if (!label->text().isEmpty()) { name = label->text(); break; }
                }
            }
        }
        if (logicalKey == QStringLiteral("DisplayLineWidth")) { name = QStringLiteral("Line Width"); }
        if (logicalKey == QStringLiteral("DisplayPeakHoldResetMs")) { name = QStringLiteral("Peak Delay"); }
        if (logicalKey == QStringLiteral("WaterfallBlankingThresholdDb")) { name = QStringLiteral("NB threshold"); }
        if (logicalKey == QStringLiteral("DisplayBackgroundImage")) { name = QStringLiteral("Picture"); }
        name.remove(QLatin1Char(':'));
        if (name.isEmpty()) { name = QStringLiteral("value"); }
        QPushButton* dot = m_ownDots.value(logicalKey);
        if (!dot) {
            const bool corner = qobject_cast<ColorSwatchButton*>(control) != nullptr
                || (qobject_cast<QPushButton*>(control) && control->property("nereusDisplayRowName").toString().isEmpty());
            dot = new QPushButton(corner ? control : group);
            dot->setObjectName(QStringLiteral("setupOwn") + logicalKey);
            dot->setProperty("setupOwnAnchor", QVariant::fromValue(static_cast<QObject*>(corner ? control : row)));
            dot->setProperty(WorkspacePreview::kSkipProperty, true);
            dot->setFixedSize(7, 7); // Approved reference's amber own-value dot.
            dot->setFocusPolicy(Qt::NoFocus);
            dot->setCursor(Qt::PointingHandCursor);
            dot->setStyleSheet(QStringLiteral("QPushButton { background: #ffb800; border: none; border-radius: 3px; padding: 0; min-width: 0; } QPushButton:disabled { background: #78623a; }"));
            row->installEventFilter(this);
            connect(dot, &QPushButton::clicked, this, [this, keys]() {
                if (displayPanIndex() < 0) { return; }
                SpectrumWidget* selected = nullptr;
                for (SpectrumWidget* pan : panWidgets()) { if (pan->panIndex() == displayPanIndex()) { selected = pan; break; } }
                if (selected) { selected->resetPanDisplayOverridesForKeys(keys); }
                else { PanDisplayKeys::clearPan(displayPanIndex(), keys); }
                emit ownRowReset();
                showDisplayDefaults();
                displayPansChanged();
            });
            m_ownDots.insert(logicalKey, dot);
        }
        bool own = false;
        if (displayPanIndex() >= 0) {
            for (const QString& key : keys) {
                if (selected) {
                    QString inherited = AppSettings::instance().value(key).toString();
                    if (inherited.isEmpty()) { inherited = selected->perPanShipDefault(key); }
                    own = own || !PanDisplayKeys::valuesEqual(selected->perPanDisplayValue(key), inherited);
                } else { own = own || PanDisplayKeys::hasOverride(key, displayPanIndex()); }
            }
        }
        dot->setVisible(own);
        dot->setEnabled(control->isEnabled());
        dot->setToolTip(QStringLiteral("Pan %1's own %2. Click the dot to follow the Setup default again.")
            .arg(displayPanIndex() + 1).arg(name));
        if (control->property("nereusDisplayRowName").toString().endsWith(QStringLiteral("choice"))) { continue; }
        QLabel* reason = m_rowReasons.value(logicalKey);
        if (!reason) {
            reason = new QLabel(group);
            reason->setObjectName(QStringLiteral("setupReason") + logicalKey);
            reason->setWordWrap(true);
            if (auto* column = qobject_cast<QVBoxLayout*>(group->layout())) {
                int rowIndex = column->indexOf(row);
                if (rowIndex < 0) {
                    for (int index = 0; index < column->count(); ++index) {
                        if (displayItemContains(column->itemAt(index), control)) { rowIndex = index; break; }
                    }
                }
                column->insertWidget(rowIndex + 1, reason);
            } else if (auto* form = qobject_cast<QFormLayout*>(group->layout())) {
                int index = -1;
                QFormLayout::ItemRole role;
                form->getWidgetPosition(control, &index, &role);
                if (index >= 0) { form->insertRow(index + 1, reason); }
                else { form->addRow(reason); }
            } else { group->layout()->addWidget(reason); }
            m_rowReasons.insert(logicalKey, reason);
        }
        reason->setText(control->isEnabled() ? QString() : control->toolTip());
    }
    for (auto dot = m_ownDots.cbegin(); dot != m_ownDots.cend(); ++dot) {
        if (!seen.contains(dot.key()) && dot.value()) { dot.value()->hide(); }
    }
    for (QWidget* control : findChildren<QWidget*>()) {
        const QString id = control->property("nereusSetupId").toString();
        if (id != QStringLiteral("display.gridScales.dbMax") && id != QStringLiteral("display.gridScales.dbMin")
            && id != QStringLiteral("display.threeD.floor") && id != QStringLiteral("display.levels.retune")
            && id != QStringLiteral("display.gridScales.copyWaterfallThresholds")
            && id != QStringLiteral("display.waterfallDefaults.copySpectrumMinMax")) { continue; }
        QGroupBox* group = nullptr;
        for (QWidget* parent = control->parentWidget(); parent && parent != this; parent = parent->parentWidget()) {
            if ((group = qobject_cast<QGroupBox*>(parent))) { break; }
        }
        if (!group || !qobject_cast<QVBoxLayout*>(group->layout())) { continue; }
        QWidget* row = control;
        while (row->parentWidget() && row->parentWidget() != group) { row = row->parentWidget(); }
        QLabel* reason = m_rowReasons.value(id);
        if (!reason) {
            reason = new QLabel(group);
            reason->setWordWrap(true);
            reason->setObjectName(QStringLiteral("setupReason.") + id);
            auto* column = qobject_cast<QVBoxLayout*>(group->layout());
            column->insertWidget(column->indexOf(row) + 1, reason);
            m_rowReasons.insert(id, reason);
        }
        reason->setText(control->isEnabled() ? QString() : control->toolTip());
    }
    // Form rows retained from the source builders may collect their reasons
    // below the group. Keep each distinct explanation once in that group.
    QHash<QWidget*, QSet<QString>> displayedReasons;
    for (QLabel* reason : findChildren<QLabel*>()) {
        if (!reason->objectName().startsWith(QStringLiteral("setupReason"))) { continue; }
        QWidget* group = reason->parentWidget();
        QSet<QString>& seenReasons = displayedReasons[group];
        const bool shown = !reason->text().isEmpty() && !seenReasons.contains(reason->text());
        // Decide the final visibility once. Showing duplicate explanations
        // before hiding them again reflows unchanged rows on live level updates.
        reason->setVisible(shown);
        if (shown) { seenReasons.insert(reason->text()); }
    }
    positionOwnRows();
}

void TxDisplayPage::retireInlineGradientEditor()
{
    if (!m_txWfGradientPicker) { return; }
    QWidget* oldParent = m_txWfGradientPicker->parentWidget();
    if (auto* form = qobject_cast<QFormLayout*>(oldParent->layout())) {
        const QFormLayout::TakeRowResult row = form->takeRow(m_txWfGradientPicker);
        const std::unique_ptr<QLayoutItem> label(row.labelItem);
        const std::unique_ptr<QLayoutItem> field(row.fieldItem);
    }
    m_txWfGradientPicker->setParent(this);
    m_txWfGradientPicker->hide();
    if (m_txWfGradientRowLabel) { m_txWfGradientRowLabel->setParent(this); m_txWfGradientRowLabel->hide(); }
}

} // namespace NereusSDR
