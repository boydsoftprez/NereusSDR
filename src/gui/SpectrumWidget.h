#pragma once

// =================================================================
// src/gui/SpectrumWidget.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/enums.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
// =================================================================

/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

#include <QWidget>
#include <QVector>
#include <QImage>
#include <QColor>
#include <QPoint>
#include <QMap>
#include <QHash>
#include <QTimer>
#include <QPropertyAnimation>

#include "spectrum/ActivePeakHoldTrace.h"
#include "spectrum/PeakBlobDetector.h"
#include "spectrum/SpectrumAvenger.h"

#include <utility>

#include "core/ConnectionState.h"
#include "core/WdspTypes.h"  // DSPMode — for TX filter IQ-space mapping (Plan 4 D9)

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

// GPU spectrum: QRhiWidget base class for Metal/Vulkan/D3D12 rendering.
// CPU fallback: QWidget with QPainter.
// Note: NEREUS_GPU_SPECTRUM is set in CMakeLists.txt via target_compile_definitions.
#ifdef NEREUS_GPU_SPECTRUM
#include <QRhiWidget>
#include <rhi/qrhi.h>
using SpectrumBaseClass = QRhiWidget;
#else
using SpectrumBaseClass = QWidget;
#endif

namespace NereusSDR {

class BandPlanManager;
class SpectrumOverlayMenu;
class VfoWidget;
class ImdOverlay;  // Phase 3M-4 Task 12 — two-tone IMD overlay analytical core

// Waterfall color scheme presets.
// Default matches AetherSDR/SmartSDR style.
// From Thetis enums.cs:68-79 (ColorScheme enum). Expanded 4 → 7 in
// Phase 3G-8 commit 5 (plan §7 W17 waterfall colour schemes expansion).
enum class WfColorScheme : int {
    Default = 0,    // AetherSDR: black → blue → cyan → green → yellow → red
    Enhanced,       // Thetis enhanced (9-band progression)
    Spectran,       // SPECTRAN
    BlackWhite,     // Grayscale
    LinLog,         // Linear in low, log in high — Thetis LinLog
    LinRad,         // Linradiance-style cool → hot
    Custom,         // User-defined custom stops (reads from AppSettings)
    ClarityBlue,    // Phase 3G-9b: narrow-band monochrome (80% navy noise floor,
                    // top 20% cyan→white signals). AetherSDR-style readability.
    Count
};

// Frequency label alignment for the bottom scale bar.
// From Thetis comboDisplayLabelAlign (setup.designer.cs:34635).
// Thetis exposes 5 options (Left/Center/Right/Auto/Off); NereusSDR
// Phase 3G-8 commit 5 expands the previous 2-mode implementation to
// match.
enum class FreqLabelAlign : int {
    Left = 0,
    Center,
    Right,
    Auto,   // centered when room, otherwise left
    Off,    // suppress frequency labels entirely
    Count
};

// Spectrum averaging mode. Ported from Thetis comboDispPanAveraging
// (setup.designer.cs:34835, target console.specRX.GetSpecRX(0).AverageMode).
// Thetis options: None / Recursive / Time Window / Log Recursive.
// NereusSDR names: None / Weighted / TimeWindow / Logarithmic — the
// previous single smoothing behavior (kSmoothAlpha * new + (1-a) * prev)
// corresponds to Weighted.
enum class AverageMode : int {
    None = 0,        // pass frame through unchanged
    Weighted,        // kSmoothAlpha exponential (current NereusSDR behavior)
    Logarithmic,     // log-domain exponential (matches Thetis Log Recursive)
    TimeWindow,      // approximated as slower exponential for now
    Count
};

// Spectrum detector type. Ported from Thetis comboDispPanDetector /
// comboDispWFDetector (setup.designer.cs:34876 + setup.designer.cs:34461
// [v2.10.3.13]).  Thetis items: Peak / Rosenfell / Average / Sample / RMS
// (Pan only has RMS; WF has 4 items).
// Applied during bin reduction: when N FFT bins are mapped to M display
// pixels, this policy decides which value is chosen.
// From Thetis specHPSDR.cs:302-321 [v2.10.3.13] DetTypePan / DetTypeWF
// → SetDisplayDetectorMode(disp, pixout, mode).
enum class SpectrumDetector : int {
    Peak       = 0, // take max bin in window (Thetis "Peak")
    Rosenfell  = 1, // Rosenfell: alternate max/min per pixel (Thetis "Rosenfell")
    Average    = 2, // arithmetic mean of bins in window (Thetis "Average")
    Sample     = 3, // take first bin in window (Thetis "Sample")
    RMS        = 4, // root-mean-square of bins in window — Pan only; Thetis "RMS"
    Count
};

// Spectrum averaging mode (split from legacy AverageMode for Thetis parity).
// Ported from Thetis comboDispPanAveraging / comboDispWFAveraging
// (setup.designer.cs:34835 / setup.designer.cs:34436 [v2.10.3.13]).
// Items: None / Recursive / Time Window / Log Recursive (4 items, both combos).
// Applied across frames: each new FFT result is mixed with the running
// history buffer per the chosen policy.
// From Thetis specHPSDR.cs:383-415 [v2.10.3.13] AverageMode / AverageModeWF
// → SetDisplayAverageMode(disp, pixout, mode).
enum class SpectrumAveraging : int {
    None         = 0, // pass frame through unchanged (Thetis "None")
    Recursive    = 1, // exponential decay in linear domain (Thetis "Recursive")
    TimeWindow   = 2, // sliding time window (Thetis "Time Window")
    LogRecursive = 3, // exponential decay in log/dB domain (Thetis "Log Recursive")
    Count
};

// Gradient stop for waterfall color mapping.
struct WfGradientStop { float pos; int r, g, b; };

// Returns gradient stops for a given color scheme.
const WfGradientStop* wfSchemeStops(WfColorScheme scheme, int& count);

// CPU-rendered spectrum + waterfall display widget.
// Phase A: QPainter fallback (get something visible fast).
// Phase B: Switch to QRhiWidget for GPU rendering.
//
// Layout (top to bottom):
//   36px  - dBm scale (left strip)
//   ~40%  - spectrum trace (FFT line plot)
//   4px   - divider
//   ~60%  - waterfall (scrolling heat-map history)
//   20px  - frequency scale bar
//
// From gpu-waterfall.md lines 274-289
class SpectrumWidget : public SpectrumBaseClass {
    Q_OBJECT

    // Phase 3Q-8: animated dim factor for the disconnect overlay.
    // 1.0 = no dim (connected), 0.4 = 60% dim (disconnected, after 800 ms fade).
    Q_PROPERTY(float disconnectFade READ disconnectFade WRITE setDisconnectFade)

public:
    explicit SpectrumWidget(QWidget* parent = nullptr);
    ~SpectrumWidget() override;

    QSize sizeHint() const override { return {800, 400}; }

    // Cap on waterfall scrollback history capacity, in rows. Bounds memory use.
    // Public so unit tests (tst_waterfall_scrollback) can mirror the
    // capacity-clamp formula in their parallel shim.
    //
    // Originally 4096 (~32 MB at 2000 px wide) — ported from unmerged
    // AetherSDR PR #1478 [@2bb3b5c]. NereusSDR raised the cap to 16384
    // (~128 MB at 2000 px wide) post-merge to give ~8 min effective rewind
    // at the default 30 ms refresh and 20+ min at any period ≥ 73 ms.
    // Disk-spool tier deferred to Phase 3M (Recording).
    static constexpr int kMaxWaterfallHistoryRows = 16384;

    // ---- Frequency range ----
    void setFrequencyRange(double centerHz, double bandwidthHz);
    void setCenterFrequency(double centerHz);
    double centerFrequency() const { return m_centerHz; }
    double bandwidth() const { return m_bandwidthHz; }

    // Re-fire the auto-zoom replan with the current bandwidth.  Used by
    // setup pages (e.g. when the user changes the Hz/bin target) to kick
    // the FFTEngine into recomputing targetSize without a zoom action.
    void requestAutoZoomReplan() { emit bandwidthChangeRequested(m_bandwidthHz); }
    void setDdcCenterFrequency(double hz);
    double ddcCenterFrequency() const { return m_ddcCenterHz; }
    void setSampleRate(double hz);
    double sampleRate() const { return m_sampleRateHz; }

    // ---- Display range ----
    void setDbmRange(float minDbm, float maxDbm);
    float refLevel() const { return m_refLevel; }
    float dynamicRange() const { return m_dynamicRange; }

    // ---- Waterfall settings ----
    void setWfColorScheme(WfColorScheme scheme);
    WfColorScheme wfColorScheme() const { return m_wfColorScheme; }
    void setWfColorGain(int gain);
    int  wfColorGain() const { return m_wfColorGain; }
    void setWfBlackLevel(int level);
    int  wfBlackLevel() const { return m_wfBlackLevel; }

    // ---- Spectrum renderer controls (Phase 3G-8 commit 3) ----

    // Legacy combined averaging mode — kept for backward compat (existing
    // callers not yet migrated).  Routes internally to setSpectrumAveraging().
    // Retired key: DisplayAverageMode (migration in Task 5.1).
    void setAverageMode(AverageMode m);
    AverageMode averageMode() const { return m_averageMode; }

    // ---- Detector + Averaging split (Task 2.1, handwave fix from 3G-8) ----
    // Ported from Thetis specHPSDR.cs:302-415 [v2.10.3.13] DetTypePan /
    // DetTypeWF / AverageMode / AverageModeWF.
    // RX1 scope dropped; NereusSDR applies as global panadapter default
    // with per-pan override via ContainerSettings dialog (3G-6 pattern).

    // Spectrum (panadapter) detector — bin-reduction policy.
    // From Thetis comboDispPanDetector [v2.10.3.13] (setup.designer.cs:34876).
    void setSpectrumDetector(SpectrumDetector d);
    SpectrumDetector spectrumDetector() const { return m_spectrumDetector; }

    // Spectrum (panadapter) averaging — frame-smoothing policy.
    // From Thetis comboDispPanAveraging [v2.10.3.13] (setup.designer.cs:34835).
    void setSpectrumAveraging(SpectrumAveraging a);
    SpectrumAveraging spectrumAveraging() const { return m_spectrumAveraging; }

    // Waterfall detector — bin-reduction policy for waterfall rows.
    // From Thetis comboDispWFDetector [v2.10.3.13] (setup.designer.cs:34461).
    void setWaterfallDetector(SpectrumDetector d);
    SpectrumDetector waterfallDetector() const { return m_waterfallDetector; }

    // Waterfall averaging — frame-smoothing policy for waterfall rows.
    // From Thetis comboDispWFAveraging [v2.10.3.13] (setup.designer.cs:34436).
    void setWaterfallAveraging(SpectrumAveraging a);
    SpectrumAveraging waterfallAveraging() const { return m_waterfallAveraging; }

    // Read-only access to post-pipeline output arrays (dBm display pixels).
    // Exposed for tests that drive updateSpectrumLinear() and need to
    // assert the avenger output for a given detector + averaging combo.
    // Pipeline contract: m_renderedPixels is sized to displayWidth and
    // contains the spectrum trace data; m_wfRenderedPixels is the same
    // shape for the waterfall plane.
    const QVector<float>& renderedPixels()   const { return m_renderedPixels;   }
    const QVector<float>& wfRenderedPixels() const { return m_wfRenderedPixels; }

    // Static helper for detector math. Exposed for unit tests.
    // Note: legacy bin-reduction helper, kept for tst_detector_modes;
    // production rendering uses applySpectrumDetector() (free function in
    // spectrum/SpectrumDetector.h, verbatim WDSP analyzer.c port) instead.
    // From Thetis specHPSDR.cs:302-321 [v2.10.3.13] DetTypePan / DetTypeWF.
    static void applyDetector(const QVector<float>& input, QVector<float>& output,
                              SpectrumDetector mode, int outputBins);

    // Smoothing time constant for Weighted / Logarithmic / TimeWindow.
    // 0.0 = no smoothing, 1.0 = infinite smoothing.
    //
    // DEPRECATED: use setSpectrumAverageTimeMs / setWaterfallAverageTimeMs
    // — those compute alpha from the time constant + frame rate via the
    // Thetis formula α = exp(-1 / (fps × τ)) per specHPSDR.cs:351-380
    // [v2.10.3.13]. The bare alpha setter is kept only for callers not yet
    // migrated; it overwrites the spectrum alpha and is clobbered on the
    // next time-spin or fps change.
    void setAverageAlpha(float alpha);
    float averageAlpha() const { return m_spectrumAverageAlpha; }

    // Per-side averaging time constants (milliseconds, ms→τ via /1000).
    // Drives both spectrum and waterfall paths independently — Thetis
    // specHPSDR.cs has separate AvTau / AvTauWF setters that each compute
    // a back-multiplier α via Math.Exp(-1.0 / (frame_rate * tau)).
    // From Thetis specHPSDR.cs:351-380 [v2.10.3.13] — AvTau / AvTauWF.
    // From Thetis setup.cs udDisplayAVGTime_ValueChanged (default 30 ms)
    // and udDisplayAVTimeWF_ValueChanged (default 120 ms).
    void setSpectrumAverageTimeMs(int ms);
    int  spectrumAverageTimeMs() const { return m_spectrumAverageTimeMs; }
    void setWaterfallAverageTimeMs(int ms);
    int  waterfallAverageTimeMs() const { return m_waterfallAverageTimeMs; }
    float spectrumAverageAlpha() const { return m_spectrumAverageAlpha; }
    float waterfallAverageAlpha() const { return m_waterfallAverageAlpha; }

    // Peak hold: track per-bin max, decay after delay.
    void setPeakHoldEnabled(bool on);
    bool peakHoldEnabled() const { return m_peakHoldEnabled; }
    void setPeakHoldDelayMs(int ms);
    int  peakHoldDelayMs() const { return m_peakHoldDelayMs; }

    // ---- Active Peak Hold trace (Task 2.5) ----
    // Per-bin max tracking with configurable hold / decay / fill / TX gating.
    // Rendered as a separate pass in drawSpectrum() (Q14.1 locked decision).
    // From Thetis display.cs m_bActivePeakHold [v2.10.3.13].
    void setActivePeakHoldEnabled(bool on);
    void setActivePeakHoldDurationMs(int ms);
    void setActivePeakHoldDropDbPerSec(double r);
    void setActivePeakHoldFill(bool on);
    void setActivePeakHoldOnTx(bool on);
    // NereusSDR-original — Thetis ties the peak trace colour to the data-line
    // colour. We expose it separately so the user can keep a distinct peak
    // hold colour even when "Reset to Smooth Defaults" recolours the live
    // trace (which would otherwise hide the peak trace behind a same-coloured
    // solid line).
    void setActivePeakHoldColor(const QColor& c);
    // Called by RadioModel on MOX state change (MoxController::moxStateChanged).
    void setActivePeakHoldTxActive(bool tx);

    bool   activePeakHoldEnabled()    const { return m_activePeakHold.enabled(); }
    int    activePeakHoldDurationMs() const { return m_activePeakHold.durationMs(); }
    double activePeakHoldDropDbPerSec() const { return m_activePeakHold.dropDbPerSec(); }
    bool   activePeakHoldFill()        const { return m_activePeakHold.fill(); }
    QColor activePeakHoldColor()      const { return m_activePeakHoldColor; }

    // ---- Peak Blobs (Task 2.6) ----
    // Top-N peak markers with labeled ellipses. Rendered as a separate QPainter
    // pass in drawSpectrum() after the Active Peak Hold trace.
    // Defaults mirror Thetis Display.cs:4395-4714 [v2.10.3.13].
    void setPeakBlobsEnabled(bool e);
    void setPeakBlobsCount(int n);
    void setPeakBlobsInsideFilterOnly(bool i);
    void setPeakBlobsHoldEnabled(bool h);
    void setPeakBlobsHoldMs(int ms);
    void setPeakBlobsHoldDrop(bool d);
    void setPeakBlobsFallDbPerSec(double r);
    void setPeakBlobColor(const QColor& c);
    void setPeakBlobTextColor(const QColor& c);

    bool   peakBlobsEnabled()     const { return m_peakBlobs.enabled(); }
    int    peakBlobsCount()       const { return m_peakBlobs.count(); }
    QColor peakBlobColor()        const { return m_peakBlobColor; }
    QColor peakBlobTextColor()    const { return m_peakBlobTextColor; }

    // Trace fill (under-the-curve shaded region).
    void setPanFillEnabled(bool on);
    bool panFillEnabled() const { return m_panFill; }
    void setFillAlpha(float a);       // 0.0 .. 1.0
    float fillAlpha() const { return m_fillAlpha; }

    // Trace line width (QPainter pen width). GPU path uses line list
    // default width until commit 5 renderer additions.
    void setLineWidth(float w);
    float lineWidth() const { return m_lineWidth; }

    // Trace gradient: when enabled, the QPainter fill gradient ramps
    // from transparent at baseline to the fill color at the trace,
    // and the trace uses a vertical color gradient. GPU path keeps
    // its existing per-vertex heatmap coloring (wire-up in commit 5).
    void setGradientEnabled(bool on);
    bool gradientEnabled() const { return m_gradientEnabled; }

    // Display calibration offset added to every bin before it's mapped
    // to screen Y. Ported from Thetis Display.RX1DisplayCalOffset
    // (display.cs:1372). Range: -30 .. +30 dB.
    void setDbmCalOffset(float db);
    float dbmCalOffset() const { return m_dbmCalOffset; }

    // Trace colour (Phase 3G-8 commit 6 wiring). Single colour used for
    // both the line and the fill in the current renderer; plan §6 S11/S13
    // track splitting these if needed by future UI polish.
    void setFillColor(const QColor& c);
    QColor fillColor() const { return m_fillColor; }

    // ---- Waterfall renderer controls (Phase 3G-8 commit 4) ----

    void setWfHighThreshold(float dbm);
    float wfHighThreshold() const { return m_wfHighThreshold; }
    void setWfLowThreshold(float dbm);
    float wfLowThreshold() const { return m_wfLowThreshold; }
    void setWfAgcEnabled(bool on);
    bool wfAgcEnabled() const { return m_wfAgcEnabled; }
    void setClarityActive(bool on);
    bool clarityActive() const { return m_clarityActive; }
    // NF-AGC: auto-track waterfall thresholds to noise floor + offset.
    void setWaterfallNFAGCEnabled(bool on);
    bool waterfallNFAGCEnabled() const { return m_wfNfAgcEnabled; }
    void setWaterfallAGCOffsetDb(int db);
    int  waterfallAGCOffsetDb() const { return m_wfNfAgcOffsetDb; }
    // Stop-on-TX: pause pushWaterfallRow() while TX is active.
    void setWaterfallStopOnTx(bool on);
    bool waterfallStopOnTx() const { return m_wfStopOnTx; }
    void setWfOpacity(int percent);          // 0..100
    int  wfOpacity() const { return m_wfOpacity; }
    void setWfUpdatePeriodMs(int ms);
    int  wfUpdatePeriodMs() const { return m_wfUpdatePeriodMs; }

    qint64 waterfallHistoryMs() const  { return m_waterfallHistoryMs; }
    void   setWaterfallHistoryMs(qint64 ms);
    bool   wfLive() const              { return m_wfLive; }

    // Ported from setup.cs:7801 Display.WaterfallUseRX1SpectrumMinMax.
    void setWfUseSpectrumMinMax(bool on);
    bool wfUseSpectrumMinMax() const { return m_wfUseSpectrumMinMax; }

    // Ported from setup.designer.cs:34428 comboDispWFAveraging / AverageModeWF.
    void setWfAverageMode(AverageMode m);
    AverageMode wfAverageMode() const { return m_wfAverageMode; }

    // Timestamp overlay (NereusSDR extensions W8/W9).
    enum class TimestampPosition : int { None = 0, Left, Right, Count };
    enum class TimestampMode     : int { UTC = 0, Local, Count };
    void setWfTimestampPosition(TimestampPosition p);
    TimestampPosition wfTimestampPosition() const { return m_wfTimestampPos; }
    void setWfTimestampMode(TimestampMode m);
    TimestampMode wfTimestampMode() const { return m_wfTimestampMode; }

    // Filter / zero-line overlays on the waterfall.
    // From setup.cs:1048-1052 Display.ShowRXFilterOnWaterfall / ShowTXFilterOnRXWaterfall
    // / ShowRXZeroLineOnWaterfall / ShowTXZeroLineOnWaterfall.
    void setShowRxFilterOnWaterfall(bool on);
    bool showRxFilterOnWaterfall() const { return m_showRxFilterOnWaterfall; }
    void setShowTxFilterOnRxWaterfall(bool on);
    bool showTxFilterOnRxWaterfall() const { return m_showTxFilterOnRxWaterfall; }
    void setShowRxZeroLineOnWaterfall(bool on);
    bool showRxZeroLineOnWaterfall() const { return m_showRxZeroLineOnWaterfall; }
    void setShowTxZeroLineOnWaterfall(bool on);
    bool showTxZeroLineOnWaterfall() const { return m_showTxZeroLineOnWaterfall; }

    // ---- Grid / scales renderer controls (Phase 3G-8 commit 5) ----

    void setGridEnabled(bool on);
    bool gridEnabled() const { return m_gridEnabled; }

    // Right-edge dBm scale strip visibility. When false, the strip is
    // hidden and the spectrum fills the full widget width.
    void setDbmScaleVisible(bool on);
    bool dbmScaleVisible() const { return m_dbmScaleVisible; }

    // Bandplan overlay (Phase 3G RX Epic sub-epic D)
    void setBandPlanManager(NereusSDR::BandPlanManager* mgr);
    void setBandPlanFontSize(int pt);             // 0 = off
    int  bandPlanFontSize() const { return m_bandPlanFontSize; }
    bool bandPlanVisible() const { return m_bandPlanFontSize > 0; }

    void setShowZeroLine(bool on);
    bool showZeroLine() const { return m_showZeroLine; }

    void setShowFps(bool on);
    bool showFps() const { return m_showFps; }

    // B8 Task 21: cursor frequency readout visibility.
    // Default true (matches the previously always-on behavior).
    void setCursorFreqVisible(bool on);
    bool cursorFreqVisible() const noexcept { return m_showCursorFreq; }

    void setFreqLabelAlign(FreqLabelAlign a);
    FreqLabelAlign freqLabelAlign() const { return m_freqLabelAlign; }

    // Configurable grid/text/zero-line/band-edge colours. Previously
    // hardcoded. Ported from Thetis setup.cs:1040-1044 Display.GridColor
    // / GridPenDark / HGridColor / GridTextColor / GridZeroColor and
    // display.cs:1941 BandEdgeColor.
    void setGridColor(const QColor& c);
    QColor gridColor() const { return m_gridColor; }
    void setGridFineColor(const QColor& c);
    QColor gridFineColor() const { return m_gridFineColor; }
    void setHGridColor(const QColor& c);
    QColor hGridColor() const { return m_hGridColor; }
    void setGridTextColor(const QColor& c);
    QColor gridTextColor() const { return m_gridTextColor; }
    // Plan 4 D9c-1: zero-line color split into separate RX and TX colors.
    // RX default: red (Thetis convention).
    // TX default: amber (NereusSDR-original — distinguishes during split TX).
    void setRxZeroLineColor(const QColor& c);
    QColor rxZeroLineColor() const noexcept { return m_rxZeroLineColor; }

    void setTxZeroLineColor(const QColor& c);
    QColor txZeroLineColor() const noexcept { return m_txZeroLineColor; }

    /// Reset all user-customisable Plan 4 D9/D9c display colors to compile-time
    /// defaults.  Gives users an escape hatch from broken color combinations.
    /// Plan 4 D9c-3 — scoped to TX filter, RX filter, RX zero line, TX zero
    /// line only.  Plan 5+ may extend the scope.
    void resetDisplayColorsToDefaults();

    // Plan 4 D9c-4 — forward-compat scaffolding.  No paint code yet — these
    // colors light up only when:
    //   - TNF (Tracking Notch Filter) feature ships
    //   - SubRX (3F multi-pan / multi-RX) ships
    // Persisted now so user-customised colors survive across the version
    // that adds the feature.
    void setTnfFilterColor(const QColor& c);
    QColor tnfFilterColor() const noexcept { return m_tnfFilterColor; }

    void setSubRxFilterColor(const QColor& c);
    QColor subRxFilterColor() const noexcept { return m_subRxFilterColor; }

    void setBandEdgeColor(const QColor& c);
    QColor bandEdgeColor() const { return m_bandEdgeColor; }

    // ---- Task 2.3: Spectrum text overlays ----
    // Corner-text overlays: noise floor dBm, peak dBm @ MHz, bin width readout.
    // MHz cursor format is applied to the cursor-hover label in drawCursorInfo.

    // OverlayPosition: 4-corner placement for corner-text overlays.
    // NereusSDR-native enum; Thetis uses fixed positions (e.g. infoBar is top).
    enum class OverlayPosition { TopLeft, TopRight, BottomLeft, BottomRight };

    // formatCursorFreq — format a frequency value for the cursor label.
    // Always returns MHz format ("7.1735 MHz") — earlier integer-Hz path
    // ("7173500 Hz") was retired because it duplicated the visibility-only
    // toggle in SpectrumOverlayPanel and confused users (two controls with
    // the same name driving different state).  The Setup → Display →
    // Spectrum Defaults checkbox now controls visibility (m_showCursorFreq)
    // alongside the overlay-panel button — single source of truth.
    QString formatCursorFreq(double hz) const;

    // ShowBinWidth — toggle bin-width readout label in spectrum corner.
    // Displays sampleRate / fftSize in kHz (e.g. "11.719 Hz/bin").
    // From Thetis setup.cs:7061 lblDisplayBinWidth.Text [v2.10.3.13].
    void setShowBinWidth(bool on);
    bool showBinWidth() const { return m_showBinWidth; }
    // binWidthHz — returns current bin width; exposed for unit tests.
    double binWidthHz() const;

    // ShowNoiseFloor — render noise-floor estimate as corner text overlay.
    // From Thetis display.cs:2304-2308 ShowNoiseFloorDBM [v2.10.3.13];
    // rendered at display.cs:5440 in DrawSpectrumDX2D.
    void setShowNoiseFloor(bool on);
    bool showNoiseFloor() const { return m_showNoiseFloor; }
    void setShowNoiseFloorPosition(OverlayPosition pos);
    OverlayPosition showNoiseFloorPosition() const { return m_noiseFloorPosition; }

    // NF shift offset — From Thetis display.cs:5763 [v2.10.3.13] _fNFshiftDBM.
    // Operator-tunable shift in dB applied to the rendered NF level (line +
    // text + connector).  Clamped to [-12, +12]; default 0.
    void  setNFShiftDbm(float db);
    float nfShiftDbm() const { return m_nfShiftDbm; }

    // Fast-attack flag — From Thetis display.cs:917-927 [v2.10.3.13]
    // m_bFastAttackNoiseFloorRX1.  When true the line + text render gray
    // (m_noiseFloorFastColor) instead of red/yellow to signal the smoothed
    // average is still settling.  Caller (MainWindow) sets on band/freq
    // change and clears after attack-time elapses.
    void setNoiseFloorFastAttack(bool on);
    bool noiseFloorFastAttack() const { return m_noiseFloorFastAttack; }

    // NF colour customisation — From Thetis display.cs:2316/2329 [v2.10.3.13]:
    //   noisefloor_color      = Color.Red     (line + box)
    //   noisefloor_color_text = Color.Yellow  (dBm label)
    // Fast-attack swap colour is NereusSDR-original (Thetis hard-codes gray).
    void   setNoiseFloorColor(const QColor& c);
    QColor noiseFloorColor() const { return m_noiseFloorColor; }
    void   setNoiseFloorTextColor(const QColor& c);
    QColor noiseFloorTextColor() const { return m_noiseFloorTextColor; }
    void   setNoiseFloorFastColor(const QColor& c);
    QColor noiseFloorFastColor() const { return m_noiseFloorFastColor; }

    // NF line width — From Thetis display.cs:2310 [v2.10.3.13]
    // m_fNoiseFloorLineWidth=1.0f.  Range 1..5 in NereusSDR.
    void  setNoiseFloorLineWidth(float w);
    float noiseFloorLineWidth() const { return m_noiseFloorLineWidth; }

    // ---- NF-aware grid (Task 2.9) ----
    // When enabled, the grid lower bound auto-tracks the live noise floor
    // estimate delivered via onNoiseFloorChanged(). Ported from Thetis
    // console.cs:46074-46086 [v2.10.3.13] GridMinFollowsNFRX1 / tmrAutoAGC_Tick.
    // RX1 scope dropped; NereusSDR applies as a global panadapter default
    // with per-pan override via ContainerSettings dialog (3G-6 pattern).
    // From Thetis setup.cs:24202-24213 [v2.10.3.13]
    // — RX1 scope dropped; NereusSDR applies as global panadapter default
    //   with per-pan override via ContainerSettings dialog (3G-6 pattern).
    void setAdjustGridMinToNoiseFloor(bool on);
    bool adjustGridMinToNoiseFloor() const { return m_adjustGridMinToNF; }

    // Offset added to the NF estimate to compute the new grid min.
    // From Thetis console.cs:46035 _RX1NFoffsetGridFollow = 5f [v2.10.3.13]
    // — NereusSDR uses a range of -60..+60 with default 0 (see design 2E).
    //   Thetis subtracts the offset (setPoint = nf - offset) with default +5;
    //   NereusSDR adds the offset (proposedMin = nf + offset) with default 0,
    //   preserving equivalent semantics when the user enters a negative value.
    void setNFOffsetGridFollow(int db);
    int  nfOffsetGridFollow() const { return m_nfOffsetGridFollow; }

    // When true, move grid max by the same delta to preserve the dB range.
    // From Thetis console.cs:46085 _maintainNFAdjustDeltaRX1 [v2.10.3.13].
    // NF grid range guard: abs incase //MW0LGE [2.9.0.7] [original inline comment from console.cs:46081]
    void setMaintainNFAdjustDelta(bool on);
    bool maintainNFAdjustDelta() const { return m_maintainNFAdjustDelta; }

    // Accessors for the current grid min/max in dBm (derived from
    // m_refLevel / m_dynamicRange). Read back by tests and GridScalesPage.
    int gridMin() const { return qRound(m_refLevel - m_dynamicRange); }
    int gridMax() const { return qRound(m_refLevel); }

    // Test helper: directly fire the NF-changed handler without needing
    // a live ClarityController. Exposed in tests only; production code
    // uses the onNoiseFloorChanged() slot via signal/slot connection.
    void testApplyNoiseFloor(float nfDbm) { onNoiseFloorChanged(nfDbm); }

    // DispNormalize — normalize-to-1-Hz before display.
    // Routes to SetDisplayNormOneHz in the WDSP spectrum engine.
    // From Thetis specHPSDR.cs:291-293 [v2.10.3.13] NormOneHzPan property;
    // wired from setup.cs:18093-18099 chkDispNormalize_CheckedChanged.
    // Note: In Thetis this calls SpecHPSDRDLL.SetDisplayNormOneHz (a WDSP
    // call); NereusSDR stores the flag and propagates it to FFTEngine when
    // the WDSP spectrum engine is integrated (Task 5.x).
    void setDispNormalize(bool on);
    bool dispNormalize() const { return m_dispNormalize; }

    // ShowPeakValueOverlay — scan visible bins, render "Peak: X.X dBm @ Y.YYYY MHz"
    // as corner text. Refreshed on a timer throttled by m_peakTextDelayMs.
    // From Thetis console.cs:20073 PeakTextDelay default=500ms [v2.10.3.13].
    // PeakTextColor default DodgerBlue from console.cs:20278 [v2.10.3.13].
    void setShowPeakValueOverlay(bool on);
    bool showPeakValueOverlay() const { return m_showPeakValueOverlay; }
    void setPeakValuePosition(OverlayPosition pos);
    OverlayPosition peakValuePosition() const { return m_peakValuePosition; }
    void setPeakTextDelayMs(int ms);
    int  peakTextDelayMs() const { return m_peakTextDelayMs; }
    void setPeakValueColor(const QColor& c);
    QColor peakValueColor() const { return m_peakValueColor; }

    // ---- HIGH SWR / PA safety overlay ----
    // Ported from Thetis display.cs:4183-4201 [v2.10.3.13]
    // Mirrors the DX2D "HIGH SWR" warning block: red centred text +
    // 6 px red border around the spectrum area. When `foldback` is true,
    // "\n\nPOWER FOLD BACK" is appended to the text per display.cs:4187-4194.
    // //MW0LGE_21k8  [original inline comment from display.cs:4213]
    void setHighSwrOverlay(bool active, bool foldback) noexcept;
    bool isHighSwrOverlayActive() const noexcept { return m_highSwrActive; }
    bool isHighSwrFoldback()      const noexcept { return m_highSwrFoldback; }

    // ---- MOX / TX mode overlay (H.1, Phase 3M-1a) ----
    //
    // Ported from Thetis display.cs:1569-1593 [v2.10.3.13] Display.MOX setter.
    // When MOX is active, a red 3 px border is drawn around the spectrum
    // panel indicating TX-mode. Matches Thetis's use of tx_vgrid_pen /
    // tx_band_edge_pen (display.cs:2086, 1955 [v2.10.3.13]) which colour the
    // grid red during TX. NereusSDR renders a simpler border tint — full
    // grid colour recolouring is deferred to 3M-3.
    //
    // setTxAttenuatorOffsetDb: when ATT-on-TX is active (F.2 path) the
    // dBm scale shifts by the attenuator value so the displayed noise floor
    // stays calibrated.  Matches Thetis display.cs:4840 [v2.10.3.13]:
    //   if (!local_mox) fOffset += rx1_preamp_offset;
    // (offsets applied only in RX; TX path uses its own cal offset).
    //
    // setTxFilterVisible: stub activated from the DisplayPage
    // DrawTXFilter flag (display.cs:2481 [v2.10.3.13]).  The waterfall
    // filter overlay is already implemented (setShowTxFilterOnRxWaterfall);
    // this slot drives the spectrum-panel TX filter shadow.

    bool isMoxOverlayActive() const noexcept { return m_moxOverlay; }
    float txAttenuatorOffsetDb() const noexcept { return m_txAttOffsetDb; }
    bool txFilterVisible() const noexcept { return m_txFilterVisible; }

public slots:
    void setDisplayFps(int fps);

    // Slot wired from MoxController::moxStateChanged (MainWindow::setupModel).
    // Draws a 3 px red border around the spectrum area when isTx=true.
    // From Thetis display.cs:1569-1593 [v2.10.3.13] Display.MOX setter.
    void setMoxOverlay(bool isTx);

    // Slot fed from StepAttenuatorController::txAttenuatorOffsetDbChanged.
    // Shifts the dBm calibration display during TX-active ATT-on-TX.
    // From Thetis display.cs:4840 [v2.10.3.13].
    void setTxAttenuatorOffsetDb(float offsetDb);

    // Slot driven from DisplayPage DrawTXFilter checkbox.
    // From Thetis display.cs:2481 [v2.10.3.13].
    void setTxFilterVisible(bool on);

    // ---- Two-tone IMD overlay state slots (Phase 3M-4 Task 12) ----
    //
    // From Thetis display.cs:5008 [v2.10.3.13] show condition:
    //   show_imd_measurements = local_mox && _testing_imd
    //                           && _show_imd_measurements && displayduplex;
    //
    // Three of the four flags come from external coordinators wired by
    // MainWindow:
    //   setMoxOverlay(bool)            <- already exists; drives local_mox
    //   setTestingIMD(bool)            <- TwoToneController::twoToneActiveChanged
    //                                      (mirrors Thetis Display.TestingIMD,
    //                                      display.cs:296-302 [v2.10.3.13])
    //   setShowIMDMeasurements(bool)   <- PureSignal::show2ToneMeasurementsChanged
    //                                      (mirrors Thetis Display.ShowIMDMeasurments,
    //                                      display.cs:304-311 [v2.10.3.13])
    //   setDisplayDuplex(bool)         <- console.cs:15363-15369 [v2.10.3.13]
    //                                      DisplayDuplex; defaults to true in
    //                                      NereusSDR (panadapter stays live
    //                                      during MOX, equivalent to Thetis
    //                                      duplex mode).
    //
    // setShowIMDMeasurements(false) calls ImdOverlay::reset() to clear EMA
    // state, mirroring the Thetis display.cs:5680 [v2.10.3.13] behaviour:
    //   else if (_ema_dbc != -999) _ema_dbc = -999;
    void setTestingIMD(bool on);
    void setShowIMDMeasurements(bool on);
    void setDisplayDuplex(bool on);

    // ---- TX filter overlay (Plan 4 D9, Cluster E) ----

    /// Set the TX filter audio-Hz range.  Triggers a panadapter overlay
    /// repaint (always) and a waterfall column repaint (MOX-gated via
    /// existing m_moxOverlay).
    /// Source: NereusSDR-original glue; per-mode IQ-space mapping follows
    /// deskhpsdr/transmitter.c:2136-2186 [@120188f].
    void setTxFilterRange(int audioLowHz, int audioHighHz);

    /// Set the DSP mode so the TX filter overlay uses the correct IQ-space
    /// sign convention (USB positive, LSB negated+swapped, AM symmetric).
    /// Wired from SliceModel::dspModeChanged in MainWindow::wireSliceToSpectrum.
    void setTxMode(DSPMode mode);

    /// Signed Hz offset added to m_vfoHz when computing the TX overlay
    /// position.  Tracks the slice's active XIT offset (xitEnabled ? xitHz : 0)
    /// so the orange band centers on the actual TX frequency, not the RX VFO.
    /// Wired from SliceModel::xitEnabledChanged + xitHzChanged in MainWindow.
    void setTxVfoOffsetHz(int offsetHz);

    int txFilterLow()  const noexcept { return m_txFilterLow; }
    int txFilterHigh() const noexcept { return m_txFilterHigh; }

    // ---- TX / RX filter overlay colors (Plan 4 D9b, Cluster F) ----

    /// Set the TX passband overlay fill colour and opacity.
    /// Persists to DisplayTxFilterColor (per-pan AppSettings key).
    void setTxFilterColor(const QColor& c);
    QColor txFilterColor() const noexcept { return m_txFilterColor; }

    /// Set the RX passband overlay fill colour and opacity.
    /// Persists to DisplayRxFilterColor (per-pan AppSettings key).
    void setRxFilterColor(const QColor& c);
    QColor rxFilterColor() const noexcept { return m_rxFilterColor; }

    // ---- Per-pan settings persistence ----
    void setPanIndex(int idx) { m_panIndex = idx; }
    int  panIndex() const { return m_panIndex; }
    void loadSettings();
    void saveSettings();
    // Public coalesced-save trigger. Used by setup pages that call setDbmRange()
    // directly (which has no internal save) and need to ensure the new range
    // is persisted (e.g. Task 2.9 Copy button, per-band NF priming).
    void requestSettingsSave() { scheduleSettingsSave(); }

    // ---- VFO / filter overlay ----
    void setVfoFrequency(double hz);
    void setFilterOffset(int lowHz, int highHz);  // updates filter passband overlay

    // ---- CTUN mode (SmartSDR-style independent pan) ----
    void setCtunEnabled(bool enabled);
    bool ctunEnabled() const { return m_ctunEnabled; }
    void recenterOnVfo();

    // ---- Tuning step ----
    void setStepSize(int hz) { m_stepHz = hz; }
    int  stepSize() const { return m_stepHz; }

    // ---- VFO flag widgets (AetherSDR pattern) ----
    VfoWidget* addVfoWidget(int sliceIndex);
    void removeVfoWidget(int sliceIndex);
    VfoWidget* vfoWidget(int sliceIndex) const;
    void updateVfoPositions();

public slots:
    // Phase 3Q-8: update connection state for the disconnect overlay.
    void setConnectionState(NereusSDR::ConnectionState s);

    // Feed a new FFT frame.  binsLinear are |X[k]|² linear-power values,
    // one per frequency bin (full FFT, neg-freq first then pos-freq).
    // windowEnb is the Equivalent Noise Bandwidth of the FFT window in
    // bins; the detector applies invEnb = 1/windowEnb for Average / Sample
    // / RMS modes (analyzer.c:368-441 [v2.10.3.13]).  dbmOffset is the
    // window coherent-gain compensation -20·log10(Σw[i]) that the avenger
    // applies via scale = 10^(dbmOffset/10) so post-pipeline pixels read
    // calibrated dBm.  Called on the main thread after FFTEngine delivers
    // the frame via fftReadyLinear.
    //
    // Pipeline: visibleBinRange() slice -> applySpectrumDetector() ->
    // SpectrumAvenger::apply() -> m_renderedPixels (dBm, displayWidth).
    // Mirrors WDSP analyzer.c detector() at :283 + avenger() at :464
    // [v2.10.3.13] -- the same two-stage reduction Thetis runs per
    // display plane.  Same slice feeds the waterfall pipeline (own
    // detector + avenger) and lands in m_wfRenderedPixels.
    void updateSpectrumLinear(int receiverId, const QVector<float>& binsLinear,
                              double windowEnb, double dbmOffset);

    // NF-aware grid slot — wired to ClarityController::noiseFloorChanged in MainWindow.
    // From Thetis console.cs:46074-46086 [v2.10.3.13] tmrAutoAGC_Tick NF grid block.
    // Range delta uses std::abs() — abs incase //MW0LGE [2.9.0.7] [original inline comment from console.cs:46081]
    // NereusSDR-original: global panadapter default, no RX1/RX2 split.
    void onNoiseFloorChanged(float nfDbm);

    // ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
    // Reset the rewind ring buffer back to empty + live state. Public so
    // MainWindow can wire it to RadioModel::connectionStateChanged when
    // the radio disconnects (see plan §Task 4 Step 3-4 — NereusSDR has no
    // SpectrumWidget::clearDisplay() equivalent, so the flush is plumbed
    // through MainWindow rather than embedded in resizeEvent).
    // From AetherSDR SpectrumWidget.cpp:740-756 [@0cd4559]
    void clearWaterfallHistory();

public:
    // ── Spot overlay (Phase 3J-2 Task E1) ─────────────────────────────────
    // Public structs + setters re-declared under a fresh `public:` access
    // specifier so MOC doesn't try to interpret the nested struct as a
    // slot declaration (the enclosing block above is `public slots:`).
    //
    // Spot marker descriptor pushed into the panadapter overlay. Mirrors
    // AetherSDR's SpotMarker struct (SpectrumWidget.h:283-294 [@0cd4559])
    // verbatim so the upstream drawSpotMarkers() algorithm ports unchanged.
    // From AetherSDR src/gui/SpectrumWidget.h:283-294 [@0cd4559]
    struct SpotMarker {
        int    index{-1};
        QString callsign;
        double freqMhz{0.0};
        QString color;       // #AARRGGBB or empty for default
        QString mode;
        QColor  dxccColor;   // DXCC-aware color from DxccColorProvider (#330)
        QString source;
        QString spotterCallsign;
        QString comment;
        qint64  timestampMs{0};
    };

    // Cluster badge descriptor for spots that overflowed the level cap.
    // From AetherSDR src/gui/SpectrumWidget.h:297-300 [@0cd4559]
    struct SpotCluster {
        QRect rect;
        QVector<SpotMarker> spots;
    };

    // Click hit-test rectangle bound to a single SpotMarker. The rect is
    // the label box drawn by drawSpotMarkers; freqMhz is the click-to-tune
    // target; markerIndex points back into m_spotMarkers for tooltip data.
    // From AetherSDR src/gui/SpectrumWidget.h:635-639 [@0cd4559]
    struct SpotHitRect {
        QRect  rect;
        double freqMhz{0.0};
        int    markerIndex{-1};
    };

    void setSpotMarkers(const QVector<SpotMarker>& markers);
    // 2026-05-12 bench fix (Gap #6 — Spot List hover sync).  Driven
    // by SpotHubDialog when the user mouses over a row so the
    // matching marker on the panadapter highlights.  -1 clears.
    void setHoverSpotIndexExternal(int idx) {
        if (m_hoverSpotIndexExternal != idx) {
            m_hoverSpotIndexExternal = idx;
            update();
        }
    }
    // 2026-05-12 bench fix (Gap #7).  Per-source panadapter overlay
    // visibility.  Missing key == visible (default).  Source strings
    // match SpotMarker::source: "Cluster", "RBN", "WSJT-X",
    // "SpotCollector", "POTA", "FreeDV", "PSK", "Memory".
    void setSpotSourceVisible(const QString& source, bool visible) {
        const bool current = m_spotSourceVisible.value(source, true);
        // Phase 3J-1 closeout follow-up (2026-05-12): always INSERT the
        // mask state, even when current==visible.  Previously we'd skip
        // the insert if the new value matched the default, leaving the
        // hash empty.  That's fine until a spot arrives later whose
        // source somehow doesn't match the key string format -- the
        // mask check m_spotSourceVisible.value(...) falls back to the
        // default 'true' and shows it.  Always inserting guarantees
        // the key is present for predictable mask behaviour and avoids
        // a class of "default-true silently shows" bugs the bench
        // operator hit with FreeDV spots on 2026-05-12.
        m_spotSourceVisible.insert(source, visible);
        if (current != visible) { update(); }
    }
    bool isSpotSourceVisible(const QString& source) const {
        return m_spotSourceVisible.value(source, true);
    }
    void setShowSpots(bool on) { m_showSpots = on; update(); }
    bool showSpots() const { return m_showSpots; }
    void setSpotFontSize(int px) { m_spotFontSize = px; update(); }
    void setSpotMaxLevels(int n) { m_spotMaxLevels = n; update(); }
    void setSpotStartPct(int pct) { m_spotStartPct = pct; update(); }
    void setSpotOverrideColors(bool on) { m_spotOverrideColors = on; update(); }
    void setSpotOverrideBg(bool on) { m_spotOverrideBg = on; update(); }
    void setSpotColor(const QColor& c) { m_spotColor = c; update(); }
    void setSpotBgColor(const QColor& c) { m_spotBgColor = c; update(); }
    void setSpotBgOpacity(int pct) { m_spotBgOpacity = pct; update(); }

    // Phase 3J-2 + 3R M2: refresh every Display tab knob from AppSettings.
    // Wired to SpotHubDialog::settingsChanged in MainWindow::openSpotHub
    // so the live spectrum overlay tracks the dialog. Defaults match the
    // F4 buildDisplayTab read-side at SpotHubDialog.cpp:1714-1730.
    void loadSpotDisplaySettings();

    // Test seams (Phase 3J-2 Task E1). Public read-only views into the
    // private state drawSpotMarkers() rebuilds each frame; the test
    // suite asserts contract by inspecting these vectors after a
    // synthetic render pass.
    const QVector<SpotMarker>&   spotMarkersForTest()   const { return m_spotMarkers; }
    const QVector<SpotHitRect>&  spotClickRectsForTest() const { return m_spotClickRects; }
    const QVector<SpotCluster>&  spotClustersForTest()  const { return m_spotClusters; }
    void  drawSpotMarkersForTest(QPainter& p, const QRect& specRect) {
        drawSpotMarkers(p, specRect);
    }

    // Phase 3J-2 + 3R M2 test seams. Read-only views into the Spot
    // Display knob state so the M2 round-trip test can pin the
    // loadSpotDisplaySettings push-path without driving a paint cycle.
    int    spotFontSizeForTest()        const { return m_spotFontSize; }
    int    spotMaxLevelsForTest()       const { return m_spotMaxLevels; }
    int    spotStartPctForTest()        const { return m_spotStartPct; }
    int    spotBgOpacityForTest()       const { return m_spotBgOpacity; }
    bool   spotOverrideColorsForTest()  const { return m_spotOverrideColors; }
    bool   spotOverrideBgForTest()      const { return m_spotOverrideBg; }
    QColor spotColorForTest()           const { return m_spotColor; }
    QColor spotBgColorForTest()         const { return m_spotBgColor; }

signals:
    // Phase 3Q-8: emitted on a left-click while not Connected.
    // MainWindow wires this to showConnectionPanel().
    void disconnectedClickRequest();

    // Emitted when user clicks on spectrum/waterfall to tune
    void frequencyClicked(double hz);
    // Phase 3J-2 Task E1: emitted when the user clicks a spot label
    // (or selects a spot from a cluster badge popup). spotIndex is the
    // SpotMarker::index that was bound to the clicked label so spot
    // sources (Memory, DX cluster, RBN, etc.) can react.
    // From AetherSDR src/gui/SpectrumWidget.h:327 [@0cd4559]
    void spotTriggered(int spotIndex);

    // 2026-05-12 bench fix (Gap #3 from adversarial audit).  Emitted
    // from the right-click context menu's "Remove Spot" action so the
    // SpotModel can purge the spot.  Mirrors AetherSDR
    // SpectrumWidget.h:357 [@0cd4559].
    void spotRemoveRequested(int spotIndex);

    // 2026-05-12 bench fix (Gap #6).  Fires when the mouse enters or
    // leaves a spot label hit-rect.  -1 indicates "no spot under
    // cursor" (use to clear the Spot List highlight).  Drives the
    // panadapter <-> Spot List hover sync.
    void spotHoverIndexChanged(int spotIndex);

    // Emitted when user drags a filter edge
    void filterEdgeDragged(int lowHz, int highHz);
    // Emitted when pan center changes (drag, auto-scroll)
    void centerChanged(double centerHz);
    // Emitted when user scrolls to change bandwidth
    void bandwidthChangeRequested(double newBandwidthHz);

    // Emitted when user-visible dBm range changes via the scale strip
    // (arrow click, drag-pan on strip body, wheel zoom). Args are the
    // new floor (min) and ceiling (max) in dBm.
    // From AetherSDR SpectrumWidget.cpp:1734 [@0cd4559]
    void dbmRangeChangeRequested(float minDbm, float maxDbm);

    // Emitted when CTUN mode changes
    void ctunEnabledChanged(bool enabled);

    // Plan 4 D9 test seam: fires from drawTxFilterOverlay() after pixel
    // coordinates are computed.  Production code ignores this signal;
    // tests use QSignalSpy to verify paint was triggered with the right band.
    // Same pattern as TxChannel::txFilterApplied (Plan 4 D8).
    void txFilterOverlayPainted(int xLeft, int xRight);

protected:
#ifdef NEREUS_GPU_SPECTRUM
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;
#endif
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;  // mouse overlay forwarding
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    // ---- Phase 3Q-8: disconnect overlay state ----
    // The CPU paintEvent path can paint a QPainter overlay, but the GPU
    // (QRhi) path early-returns and refuses QPainter. To work in both modes
    // we use a child QLabel — Qt composites it on top of the QRhi surface.
    NereusSDR::ConnectionState m_connState{NereusSDR::ConnectionState::Disconnected};
    float m_disconnectFade{1.0f};  // animated; 1.0 connected, 0.4 disconnected
    QPropertyAnimation* m_fadeAnim{nullptr};
    QLabel* m_disconnectLabel{nullptr};

    float disconnectFade() const { return m_disconnectFade; }
    void  setDisconnectFade(float f) { m_disconnectFade = f; update(); }

    // ---- Drawing helpers ----
    void drawGrid(QPainter& p, const QRect& specRect);
    void drawSpectrum(QPainter& p, const QRect& specRect);
    // Active Peak Hold separate render pass (Q14.1). Called from drawSpectrum()
    // after the fill path so the peak trace sits on top of fill but below
    // the live trace line.  Iterates the per-display-pixel peak array
    // (sized to m_renderedPixels.size()).
    // From Thetis Display.cs:5341 [v2.10.3.13] -- spectralPeaks[i] indexed
    // by display pixel.
    void paintActivePeakHoldTrace(QPainter& p, const QRect& specRect);
    // Peak Blobs render pass (Task 2.6). Draws labeled ellipses at the top-N
    // local maxima. Called from drawSpectrum() after the live trace line so
    // the blobs sit on top of all spectrum content.  Blob.binIndex is now
    // a display-pixel index (0..displayWidth-1) per the pipeline migration.
    // From Thetis Display.cs:5453-5508 [v2.10.3.13].
    void paintPeakBlobs(QPainter& p, const QRect& specRect);
    // Noise-floor overlay: horizontal dashed line across the spectrum at the
    // estimated NF level + corner text label.  Called from both CPU drawSpectrum
    // and GPU renderGpuFrame overlay block so the same NF visuals appear
    // regardless of paint backend.  Reads the state maintained by
    // processNoiseFloor() (m_nfLerpAverage / m_nfFftBinAverage).
    // From Thetis display.cs:5423-5448 [v2.10.3.13] (line + text combined).
    void paintNoiseFloorOverlay(QPainter& p, const QRect& specRect);

    // Source-first port of Thetis processNoiseFloor — display.cs:5866-5912
    // [v2.10.3.13].  Called once per spectrum frame from
    // updateSpectrumLinear after m_renderedPixels is finalised; iterates
    // those pixels to accumulate (count, linear-sum) of bins below the
    // previous-frame estimate (averageCount/averageSum), then updates
    // m_nfFftBinAverage (per-frame) and m_nfLerpAverage (smoothed).
    // Also runs the fast-attack convergence-gated auto-clear from
    // display.cs:5904-5908.
    void processNoiseFloor();
    void drawWaterfall(QPainter& p, const QRect& wfRect);
    // HIGH SWR / PA safety overlay — ported from display.cs:4183-4201 [v2.10.3.13]
    void paintHighSwrOverlay(QPainter& p);
    // MOX / TX border overlay — ported from display.cs:1569-1593 [v2.10.3.13]
    // Phase 3M-1a H.1.
    void paintMoxOverlay(QPainter& p);
    // Phase 3G-8 commit 10: overlay-only waterfall chrome (filter bands,
    // zero lines, timestamp, opacity dim) split out so the GPU overlay
    // texture can render the same chrome without blitting the waterfall
    // image. Called from drawWaterfall() on the QPainter fallback path
    // and from the GPU overlay build step.
    void drawWaterfallChrome(QPainter& p, const QRect& wfRect);
    void drawFreqScale(QPainter& p, const QRect& r);
    void drawDbmScale(QPainter& p, const QRect& specRect);
    void drawBandPlan(QPainter& p, const QRect& specRect);

    // ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
    // From AetherSDR SpectrumWidget.h:402-413 [@0cd4559]
    void drawTimeScale(QPainter& p, const QRect& wfRect);
    QRect waterfallTimeScaleRect(const QRect& wfRect) const;
    QRect waterfallLiveButtonRect(const QRect& wfRect) const;
    int   waterfallStripWidth() const;
    void  ensureWaterfallHistory();
    void  rebuildWaterfallViewport();
    void  setWaterfallLive(bool live);
    void  appendHistoryRow(const QRgb* rowData, qint64 timestampMs);
    int   waterfallHistoryCapacityRows() const;
    int   maxWaterfallHistoryOffsetRows() const;
    int   historyRowIndexForAge(int ageRows) const;
    QString pausedTimeLabelForAge(int ageRows) const;
    void  reprojectWaterfall(double oldCenterHz, double oldBandwidthHz,
                             double newCenterHz, double newBandwidthHz);
    // (clearWaterfallHistory moved to public slots: in sub-epic E task 4 review.)

    void drawVfoMarker(QPainter& p, const QRect& specRect, const QRect& wfRect);
    void drawCursorInfo(QPainter& p, const QRect& specRect);

    // ---- Spot overlay (Phase 3J-2 Task E1) ----
    // From AetherSDR src/gui/SpectrumWidget.cpp:4497-4633 [@0cd4559]
    // Algorithm preserved verbatim:
    //  - Color priority: override -> DXCC -> spot color -> default cyan.
    //  - Multi-level vertical stacking with collision-induced nudge-down.
    //  - Re-scan from top after each nudge (handles cascade overlaps).
    //  - Overflow into +N cluster badges at maxBottom + 2; cluster bin
    //    width = 40 px.
    //  - Vertical dotted tick from spectrum bottom to label.
    //  - Optional background pill with configurable opacity.
    //  - Click-to-tune via frequencyClicked(hz) signal (NereusSDR Hz units;
    //    AetherSDR emits MHz, the call site multiplies by 1e6).
    //  - Cluster badge popup menu with formatted spot lines.
    void drawSpotMarkers(QPainter& p, const QRect& specRect);
    void showSpotClusterPopup(const SpotCluster& cluster, const QPoint& globalPos);

    // ---- TX filter overlay (Plan 4 D9, Cluster E) ----
    // drawTxFilterOverlay: panadapter band fill + border lines + label.
    //   Called when m_txFilterVisible is set; gating is at the call site.
    // drawTxFilterWaterfallColumn: waterfall column fill, MOX-gated.
    //   Called when m_showTxFilterOnRxWaterfall && m_moxOverlay.
    // Per deskhpsdr/transmitter.c:2136-2186 [@120188f] for IQ-space mapping.
    void drawTxFilterOverlay(QPainter& p, const QRect& specRect);
    void drawTxFilterWaterfallColumn(QPainter& p, const QRect& wfRect);

    // ---- Two-tone IMD overlay (Phase 3M-4 Task 12) ----
    // From Thetis display.cs:5008 [v2.10.3.13]:
    //   show_imd_measurements = local_mox && _testing_imd
    //                           && _show_imd_measurements && displayduplex;
    // Renders peak markers + readout box driven by ImdOverlay state.
    // Called from paintEvent (CPU path) and from the GPU overlay rebuild
    // (live, uncached because spectrum changes every frame).
    void drawImdOverlay(QPainter& p, const QRect& specRect);

    // Shared audio→IQ-space conversion used by both draw methods.
    // Returns {iqLowHz, iqHighHz} signed offsets from the VFO center.
    // Per deskhpsdr/transmitter.c:2136-2186 [@120188f].
    std::pair<int,int> txAudioToIq(int audioLow, int audioHigh, DSPMode mode) const;

    // ---- Coordinate helpers ----
    int    hzToX(double hz, const QRect& r) const;
    double xToHz(int x, const QRect& r) const;
    int    dbmToY(float dbm, const QRect& r) const;
    float  dbmToYf(float dbm, const QRect& r) const; // sub-pixel variant for antialiased trace

    // Band plan strip height — when the band plan is enabled, the bottom
    // m_bandPlanFontSize + 4 pixels of every spectrum-coordinate rect are
    // RESERVED for the strip.  dbmToY / dbmToYf subtract this from the
    // effective drawing area so the dBm floor maps to the TOP edge of the
    // band plan strip rather than the bottom of the panel.  Result: every
    // spectrum overlay (trace, NF line+box+text, peak hold, peak blobs,
    // grid lines, dBm-scale labels) automatically anchors at the band plan
    // top — they all derive their Y from these helpers.  drawBandPlan
    // itself paints below this carve-out using r.bottom() directly so the
    // strip still lands at the panel bottom.
    int bandPlanStripHeight() const;

    // 1-Hz-bandwidth normalisation shift — returns -10*log10(binWidthHz)
    // when m_dispNormalize is on, 0 otherwise.  Applied inside dbmToY /
    // dbmToYf so the entire spectrum (trace + every overlay derived from
    // these helpers) renormalises in lockstep when the toggle flips.
    // Mirrors Thetis SetDisplayNormOneHz (specHPSDR.cs:325) at render time.
    float normalizeShiftDb() const;

    // Returns kDbmStripW when the dBm scale strip is visible, 0 otherwise.
    // Used everywhere a rect excludes the right-edge strip so that hiding
    // the strip automatically gives the spectrum full widget width.
    int    effectiveStripW() const;

    // Returns the first and last FFT bin indices visible in the current
    // display window (m_centerHz ± m_bandwidthHz/2), mapped against
    // the DDC center and sample rate. Clamped to [0, binCount-1].
    std::pair<int, int> visibleBinRange(int binCount) const;

    // ---- Waterfall helpers ----
    // Feeds a post-pipeline waterfall row (display-pixel dBm, length
    // displayWidth).  Caller is updateSpectrumLinear after the waterfall
    // detector + avenger have run.  Inner AGC + NF-AGC + threshold compute
    // operate on display pixels per Thetis Display.cs:6713-6738
    // [v2.10.3.13] (waterfall_data[i] indexed by pixel).
    void   pushWaterfallRow(const QVector<float>& wfPixelsDbm);
    QRgb   dbmToRgb(float dbm) const;

    // ---- FFT pipeline state ----
    // Single Thetis-faithful pipeline: linear-power FFT bins -> visible
    // slice -> detector -> avenger -> dBm display pixels.  Spectrum and
    // waterfall keep separate detector + avenger state per WDSP per-plane
    // model (each ANALYZER_INFO[] entry has its own DetType + AvMode).
    QVector<float> m_fullLinearBins;       // FFTEngine input cache (|X[k]|²)
    QVector<float> m_displayLinearPixels;  // spectrum detector output (linear)
    QVector<float> m_renderedPixels;       // spectrum avenger output (dBm)
    QVector<float> m_wfDisplayLinearPixels; // waterfall detector output (linear)
    QVector<float> m_wfRenderedPixels;     // waterfall avenger output (dBm)

    // Equivalent Noise Bandwidth of the current FFT window, in bins.
    // Refreshed every frame via the windowEnb arg on fftReadyLinear so
    // the detector's invEnb scaling stays in lock-step with the bins it
    // just received.  No setter coordination needed.
    double m_fftWindowEnb{1.0};

    // Per-channel WDSP-style frame averagers.  See SpectrumAvenger.h for
    // the av_mode wire-format mapping (-1 peak / 0 none / 1 recursive /
    // 2 window / 3 log-recursive); analyzer.c:464-554 [v2.10.3.13] is the
    // verbatim port.
    NereusSDR::SpectrumAvenger m_spectrumAvenger;
    NereusSDR::SpectrumAvenger m_waterfallAvenger;

    // ---- Frequency range ----
    double m_centerHz{14225000.0};    // 14.225 MHz default
    double m_bandwidthHz{192000.0};   // 192 kHz default (P1 base sample rate)
    double m_ddcCenterHz{14225000.0};   // DDC hardware center frequency
    double m_sampleRateHz{768000.0};    // DDC sample rate

    // ---- Display range ----
    // From Thetis display.cs:1743-1754. Init values must match the
    // ship defaults in loadSettings() (SpectrumWidget.cpp ~line 392) —
    // any divergence shows up if a code path reads these before
    // loadSettings runs, and the slider in SetupDialog briefly shows
    // the stale value. Calibrated 2026-04-30 against a residential
    // HF noise floor; see loadSettings comment for rationale.
    float  m_refLevel{-48.0f};        // top of display (dBm)
    float  m_dynamicRange{68.0f};     // range in dB (bottom = refLevel - dynamicRange)

    // ---- Waterfall ----
    QImage m_waterfall;               // ring buffer (Format_RGB32)
    int    m_wfWriteRow{0};

    // ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
    // From AetherSDR SpectrumWidget.h:493-502 [@0cd4559]
    QImage          m_waterfallHistory;            // RGB32 ring buffer
    QVector<qint64> m_wfHistoryTimestamps;         // parallel; per-row wall-clock ms
    int             m_wfHistoryWriteRow{0};        // LIFO; index 0 = newest
    int             m_wfHistoryRowCount{0};        // saturates at capacity
    int             m_wfHistoryOffsetRows{0};      // 0 = newest visible at top
    bool            m_wfLive{true};                // pause/live state
    bool            m_draggingTimeScale{false};    // gesture flag
    int             m_timeScaleDragStartY{0};      // anchor Y at mousedown
    int             m_timeScaleDragStartOffsetRows{0};

    // Default depth (overridden at runtime by m_waterfallHistoryMs from AppSettings).
    // From AetherSDR SpectrumWidget.h:502 [@0cd4559]
    static constexpr qint64 kDefaultWaterfallHistoryMs = 20LL * 60LL * 1000LL;

    // (kMaxWaterfallHistoryRows declared at the top of the public block —
    // moved there in sub-epic E task 2 so test-shims can mirror the
    // capacity-clamp formula without befriending the class.)

    // Runtime-configurable depth; persisted as AppSettings("DisplayWaterfallHistoryMs").
    // NereusSDR-side enhancement — see plan §authoring-time #1.
    qint64          m_waterfallHistoryMs{kDefaultWaterfallHistoryMs};

    // Debounce timer for ensureWaterfallHistory() during rapid resize / slider drag.
    // From AetherSDR SpectrumWidget.h:559 [@2bb3b5c]
    // (debounce timer added by unmerged AetherSDR PR #1478 — see plan §authoring-time #2)
    QTimer*         m_historyResizeTimer{nullptr};
    QTimer          m_displayTimer;
    bool            m_hasNewSpectrum{false};

    // ---- Waterfall display controls ----
    // From AetherSDR SpectrumWidget defaults + Thetis display.cs:2522-2536
    WfColorScheme m_wfColorScheme{WfColorScheme::Default};
    int    m_wfColorGain{45};         // 0-100
    int    m_wfBlackLevel{104};       // 0-125 — keep in sync with loadSettings ship default
    // Waterfall uses its own dBm range (narrower than spectrum for better contrast).
    // Seed values; WfAgc (default on) continuously recomputes these at runtime.
    // Ship defaults — keep in sync with loadSettings (SpectrumWidget.cpp)
    float  m_wfHighThreshold{-62.0f};
    float  m_wfLowThreshold{-122.0f};

    // ---- Smoothing constant ----
    // From AetherSDR SpectrumWidget.h:417 — SMOOTH_ALPHA = 0.35f
    static constexpr float kSmoothAlpha = 0.35f;

    // ---- Layout constants ----
    // From gpu-waterfall.md:590-593
    float  m_spectrumFrac{0.40f};     // 40% spectrum, 60% waterfall
    static constexpr int kFreqScaleH = 28;  // Taller for easier grab target
    static constexpr int kDividerH = 4;
    static constexpr int kDbmStripW = 36;
    // Height of each arrow button at the top of the dBm strip.
    // From AetherSDR SpectrumWidget.h:539 [@0cd4559]
    static constexpr int kDbmArrowH = 14;

    // ---- Spectrum fill ----
    // From AetherSDR defaults
    QColor m_fillColor{0x00, 0xe5, 0xff};  // cyan
    float  m_fillAlpha{0.70f};
    bool   m_panFill{true};

    // ---- Phase 3G-8 commit 3: spectrum renderer state ----

    AverageMode m_averageMode{AverageMode::Logarithmic};
    // Spectrum + waterfall averaging time constants in milliseconds, with
    // per-side back-multiplier alphas computed via Thetis math:
    //   α = exp(-1 / (fps × τ))  [specHPSDR.cs:358 / :374, v2.10.3.13]
    // Defaults match Thetis (setup.cs udDisplayAVGTime_ValueChanged = 30 ms,
    // udDisplayAVTimeWF_ValueChanged = 120 ms).
    int         m_spectrumAverageTimeMs{30};
    int         m_waterfallAverageTimeMs{120};
    // Recomputed by recomputeAverageAlphas() whenever fps or time changes.
    float       m_spectrumAverageAlpha{0.0f};
    float       m_waterfallAverageAlpha{0.0f};

    // ---- Task 2.1: Detector + Averaging split (handwave fix from 3G-8) ----
    // Ported from Thetis specHPSDR.cs:302-415 [v2.10.3.13].
    SpectrumDetector  m_spectrumDetector{SpectrumDetector::Peak};
    SpectrumAveraging m_spectrumAveraging{SpectrumAveraging::LogRecursive};
    SpectrumDetector  m_waterfallDetector{SpectrumDetector::Peak};
    SpectrumAveraging m_waterfallAveraging{SpectrumAveraging::None};

    bool        m_peakHoldEnabled{false};
    int         m_peakHoldDelayMs{2000};
    // Per-display-pixel running max (replaces full-bin m_peakHoldBins).
    // Sized to displayWidth on first updateSpectrumLinear and re-sized on
    // resize.  Decay timer below resets it on tick.
    QVector<float> m_pxPeakHold;
    QTimer*     m_peakHoldDecayTimer{nullptr};

    // Active Peak Hold trace (Task 2.5). Separate from the legacy peak hold
    // above; rendered as a distinct pass per Q14.1.
    // From Thetis display.cs m_bActivePeakHold [v2.10.3.13].
    ActivePeakHoldTrace m_activePeakHold;
    // NereusSDR-original — distinct trace colour so the peak trace stays
    // visible even when the data-line colour is changed (e.g. "Reset to
    // Smooth Defaults" sets the data line to white). Default gold for high
    // contrast against the typical clarity-blue palette and against white.
    QColor m_activePeakHoldColor{0xFF, 0xD7, 0x00, 0xFF};

    // Peak Blobs detector (Task 2.6). Top-N local maxima with hold/decay.
    // From Thetis display.cs:4395-4714, 5453-5508 [v2.10.3.13].
    // Inline attribution from ported range — display.cs:4829 //MW0LGE [2.10.1.0] fix issue #137;
    // display.cs:4972 //[2.10.3.9]MW0LGE raw grid control option; display.cs:5109 //MW0LGE not used.
    PeakBlobDetector m_peakBlobs;
    // From Thetis display.cs:8434 [v2.10.3.13] m_bDX2_PeakBlob = Color.OrangeRed
    QColor m_peakBlobColor{0xFF, 0x45, 0x00, 0xFF};
    // From Thetis display.cs:8435 [v2.10.3.13] m_bDX2_PeakBlobText = Color.Chartreuse
    QColor m_peakBlobTextColor{0x7F, 0xFF, 0x00, 0xFF};

    float       m_lineWidth{1.5f};
    bool        m_gradientEnabled{false};

    // Ported from Thetis Display.RX1DisplayCalOffset (display.cs:1372).
    float       m_dbmCalOffset{0.0f};

    // ---- Phase 3G-8 commit 4: waterfall renderer state ----

    bool  m_wfAgcEnabled{true};
    bool  m_clarityActive{false};     // Phase 3G-9c: suppresses legacy AGC when Clarity drives thresholds
    // NF-AGC: Task 2.8 — auto-track thresholds to noise floor + offset.
    bool  m_wfNfAgcEnabled{false};
    int   m_wfNfAgcOffsetDb{0};       // offset applied above/below noise floor
    // Stop-on-TX: Task 2.8 — gate pushWaterfallRow() while TX is active.
    bool  m_wfStopOnTx{false};
    int   m_wfOpacity{100};           // 0..100
    int   m_wfUpdatePeriodMs{30};     // NereusSDR default per §10 divergence
    bool  m_wfUseSpectrumMinMax{false};
    AverageMode m_wfAverageMode{AverageMode::None};

    TimestampPosition m_wfTimestampPos{TimestampPosition::None};
    TimestampMode     m_wfTimestampMode{TimestampMode::UTC};

    bool  m_showRxFilterOnWaterfall{false};
    bool  m_showTxFilterOnRxWaterfall{false};
    bool  m_showRxZeroLineOnWaterfall{false};
    bool  m_showTxZeroLineOnWaterfall{false};

    // AGC rolling envelope (tracked across waterfall rows).
    float m_wfAgcRunMin{0.0f};
    float m_wfAgcRunMax{0.0f};
    bool  m_wfAgcPrimed{false};

    // Rate-limit waterfall pushes per m_wfUpdatePeriodMs.
    qint64 m_wfLastPushMs{0};

    // 1 Hz overlay repaint tick for the waterfall timestamp; started on
    // demand when the user selects a non-None timestamp position.
    QTimer* m_wfTimestampTicker{nullptr};

    // ---- Phase 3G-8 commit 5: grid / scales renderer state ----

    bool  m_gridEnabled{true};
    bool  m_showZeroLine{false};
    bool  m_showFps{false};
    bool  m_dbmScaleVisible{true};  // right-edge dBm strip; false → spectrum fills full width
    bool  m_showCursorFreq{true};   // B8 Task 21: cursor frequency readout; default on
    FreqLabelAlign m_freqLabelAlign{FreqLabelAlign::Center};

    NereusSDR::BandPlanManager* m_bandPlanMgr{nullptr};   // non-owning
    int                          m_bandPlanFontSize{6};   // 0 = off; AetherSDR default

    QColor m_gridColor{255, 255, 255, 40};       // vertical freq grid
    QColor m_gridFineColor{255, 255, 255, 20};   // 1/5 step fine grid
    QColor m_hGridColor{255, 255, 255, 40};      // horizontal dBm grid
    QColor m_gridTextColor{255, 255, 0};         // yellow text default
    // Plan 4 D9c-1: split zero-line color into RX + TX.
    QColor m_rxZeroLineColor{255, 0, 0};         // red default (Thetis convention)
    QColor m_txZeroLineColor{255, 184, 0};       // amber default (NereusSDR-original)
    QColor m_bandEdgeColor{255, 0, 0};           // red default (Thetis)

    // Plan 4 D9c-4: TNF + SubRX forward-compat scaffolding.  No paint yet.
    QColor m_tnfFilterColor  {255,  80,  80,  80};   // red translucent placeholder
    QColor m_subRxFilterColor{180,   0, 220,  80};   // purple translucent placeholder

    // FPS overlay tracking
    int    m_fpsFrameCount{0};
    qint64 m_fpsLastUpdateMs{0};
    float  m_fpsDisplayValue{0.0f};

    // ---- VFO / filter overlay ----
    double m_vfoHz{0.0};
    int    m_filterLowHz{-2850};    // LSB default — from Thetis
    int    m_filterHighHz{-150};
    int    m_stepHz{100};           // tuning step size

    int    m_panIndex{0};            // for per-pan settings keys

    // ---- VFO flag widgets ----
    QMap<int, VfoWidget*> m_vfoWidgets;

    // ---- CTUN mode ----
    bool   m_ctunEnabled{true};  // true = SmartSDR-style (pan independent of VFO)
    enum class VfoOffScreen { None, Left, Right };
    VfoOffScreen m_vfoOffScreen{VfoOffScreen::None};
    void drawOffScreenIndicator(QPainter& p, const QRect& specRect, const QRect& wfRect);

    // ---- Mouse tracking overlay (QRhiWidget macOS workaround) ----
    QWidget* m_mouseOverlay{nullptr};

    // ---- Overlay menu ----
    SpectrumOverlayMenu* m_overlayMenu{nullptr};

    // ---- Spot overlay state (Phase 3J-2 Task E1) ----
    // Backing store + per-frame click-rect / cluster vectors. Defaults
    // match AetherSDR src/gui/SpectrumWidget.h:634-651 [@0cd4559].
    QVector<SpotMarker>  m_spotMarkers;
    QVector<SpotHitRect> m_spotClickRects;
    QVector<SpotCluster> m_spotClusters;
    bool   m_showSpots{true};
    int    m_spotFontSize{16};
    int    m_spotMaxLevels{3};
    int    m_spotStartPct{50};       // % down from top of spectrum
    bool   m_spotOverrideColors{false};
    bool   m_spotOverrideBg{true};
    QColor m_spotColor{Qt::yellow};
    QColor m_spotBgColor{Qt::black};
    int    m_spotBgOpacity{48};

    // 2026-05-12 bench fix (Gap #6).  m_hoverSpotIndex: which spot
    // label the mouse is currently over (SpotMarker::index, -1 if
    // none).  Emitted via spotHoverIndexChanged so the Spot List can
    // highlight the matching row.  m_hoverSpotIndexExternal: opposite
    // direction — set by setHoverSpotIndexExternal() when the user
    // hovers a row in the Spot List, drives a halo around the label
    // in drawSpotMarkers.
    int    m_hoverSpotIndex{-1};
    int    m_hoverSpotIndexExternal{-1};

    // 2026-05-12 bench fix (Gap #7).  Per-source visibility mask for
    // the panadapter overlay.  Keys are SpotMarker::source strings
    // ("Cluster", "RBN", "WSJT-X", "SpotCollector", "POTA", "FreeDV",
    // "PSK", "Memory"); missing keys default to visible.  Used by
    // drawSpotMarkers to skip the per-marker draw when the source's
    // panadapter visibility toggle is off.  SpotHubDialog Display tab
    // drives this via setSpotSourceVisible.
    QHash<QString, bool> m_spotSourceVisible;

    // ---- Task 2.3: Spectrum text overlay state ----

    // (m_showMHzOnCursor retired in 2026-05 — see formatCursorFreq comment.)

    // From Thetis setup.cs:7061 [v2.10.3.13] lblDisplayBinWidth
    bool m_showBinWidth{false};

    // From Thetis display.cs:2304 [v2.10.3.13] m_bShowNoiseFloorDBM (default true in Thetis;
    // NereusSDR defaults off so the overlay is opt-in rather than on by default)
    bool            m_showNoiseFloor{false};
    OverlayPosition m_noiseFloorPosition{OverlayPosition::BottomLeft};

    // NF render colours — From Thetis display.cs:2316-2337 [v2.10.3.13]:
    //   private static Color noisefloor_color      = Color.Red;
    //   private static Color noisefloor_color_text = Color.Yellow;
    // Defaults match Thetis exactly so the line is visually distinct from
    // grid text (yellow) and from the spectrum trace.  Made configurable
    // via setNoiseFloorColor / setNoiseFloorTextColor when Setup wires them.
    // Default line + text colour — NereusSDR-tweaked from Thetis red/yellow
    // (display.cs:2316/2329 [v2.10.3.13]).  Thetis renders against a
    // configurable trace colour; NereusSDR's stock spectrum trace is cyan
    // (#00E5FF, see m_fillColor), and Thetis-default red dashes blend with
    // both cyan-trace fill and the brown band-plan strip beneath.  Default
    // tweak: bright magenta line + yellow text.  Both still match Thetis
    // semantics (warm/distinctive line, separate-colour text label) and
    // give the user clearly visible defaults that they can dial back to red
    // via the colour picker if they prefer Thetis-stock colours.
    QColor m_noiseFloorColor     {0xFF, 0x40, 0xFF};   // bright magenta
    QColor m_noiseFloorTextColor {Qt::yellow};
    // Fast-attack swap colour — From Thetis display.cs:5431-5432 [v2.10.3.13]:
    //   nf_colour      = bFast ? m_bDX2_Gray : m_bDX2_noisefloor;
    //   nf_colour_text = bFast ? m_bDX2_Gray : m_bDX2_noisefloor_text;
    // Lightened from Qt::gray (160,160,164) to #C8C8C8 so the swatch and
    // the rendered overlay both stand out against NereusSDR's dark UI
    // background; Thetis renders against a lighter-grey grid backdrop where
    // medium-grey reads fine.
    QColor m_noiseFloorFastColor {0xC8, 0xC8, 0xC8};
    // From Thetis display.cs:2310 [v2.10.3.13] m_fNoiseFloorLineWidth=1.0f.
    float  m_noiseFloorLineWidth {1.0f};

    // FFT-replan crossfade — when auto-zoom replans the FFT (or the user
    // moves the size slider), the avenger's history is cleared to avoid
    // cross-resolution ghosting.  Without smoothing, the first new frame
    // would snap into place ("trace drops in from sky").  We capture the
    // last good rendered pixels at the moment of replan and crossfade the
    // first kReplanFadeFrames frames of the new resolution against them so
    // the trace dissolves smoothly into the new layout.
    QVector<float> m_postReplanFrozenDb;
    int            m_postReplanFrameCount{0};
    static constexpr int kReplanFadeFrames = 8;

    // Per-frame + smoothed noise-floor estimates — source-first port of
    // Thetis display.cs:4633-4636 [v2.10.3.13]:
    //   m_fFFTBinAverageRX1 — current per-frame avg of bins below the
    //                         previous-frame estimate (linear-power blend)
    //   m_fLerpAverageRX1   — exponentially smoothed toward fftBinAverage
    //                         with framesInAttack-rate (display.cs:5901-5902)
    // Updated each spectrum frame in processNoiseFloor() after
    // updateSpectrumLinear finalises m_renderedPixels.  paintNoiseFloorOverlay
    // uses m_nfLerpAverage for line/box Y and m_nfFftBinAverage for actual Y.
    float m_nfFftBinAverage{-200.0f};
    float m_nfLerpAverage{-200.0f};

    // From Thetis display.cs:4638 [v2.10.3.13] m_fAttackTimeInMSForRX1=2000.
    float m_nfAttackTimeMs{2000.0f};

    // From Thetis display.cs:5775 + 5783 [v2.10.3.13]:
    //   _NFsensitivity = 3 (default), clamped to [0, 19] in setter.
    // requireSamples = (int)(width * (sensitivity / 20)) — with default 3
    // that's ~15% of pixels, achievable on any normal band.  Higher values
    // make the estimate harder to converge; values >= 20 make it unreachable
    // (requireSamples > width) and fftBinAverage perpetually drifts to +200.
    int m_nfSensitivity{3};

    // Operator-tunable NF shift — From Thetis display.cs:5763 [v2.10.3.13]:
    //   private static float _fNFshiftDBM = 0;
    // Clamped to [-12, +12] in the setter (Thetis clamps to 12 at
    // display.cs:5771).  Applied to both lerp and actual values per
    // display.cs:5400 + 5403.
    float m_nfShiftDbm{0.0f};

    // Fast-attack flag — From Thetis display.cs:917-927 [v2.10.3.13]
    // m_bFastAttackNoiseFloorRX1.  Set on band change / freq jump / MOX
    // transition.  Auto-clear is now Thetis-faithful: gated on
    // |fftBinAverage - lerpAverage| < 1.0 AND elapsed > kFastAttackMinMs
    // (display.cs:5904-5908) — the convergence check is what tells Thetis
    // the smoothed estimate has settled to the new band.
    bool   m_noiseFloorFastAttack{false};
    qint64 m_nfLastFastAttackMs{0};
    static constexpr qint64 kFastAttackMinMs = 1000;  // display.cs:5906 Math.Max(1000, ...)

    // ---- NF-aware grid (Task 2.9) ----
    // From Thetis console.cs:46025-46085 [v2.10.3.13] GridMinFollowsNFRX1,
    // _RX1NFoffsetGridFollow, _maintainNFAdjustDeltaRX1.
    // abs() guard on fDelta: abs incase //MW0LGE [2.9.0.7] [original inline comment from console.cs:46081]
    // NereusSDR-original: applied as global default; RX1-scope dropped.
    bool m_adjustGridMinToNF{false};
    int  m_nfOffsetGridFollow{0};    // dB offset added to NF estimate (default 0)
    bool m_maintainNFAdjustDelta{false};

    // From Thetis specHPSDR.cs:325 [v2.10.3.13] NormOneHzPan
    bool m_dispNormalize{false};

    // From Thetis console.cs:20073 peak_text_delay=500 [v2.10.3.13]
    // Color from console.cs:20278 Color.DodgerBlue [v2.10.3.13]
    bool            m_showPeakValueOverlay{false};
    OverlayPosition m_peakValuePosition{OverlayPosition::TopRight};
    int             m_peakTextDelayMs{500};
    // From Thetis console.cs:20278 [v2.10.3.13]: Color.DodgerBlue = #1E90FF
    QColor          m_peakValueColor{0x1E, 0x90, 0xFF};

    // Peak text overlay refresh timer — throttled by m_peakTextDelayMs.
    QTimer*         m_peakTextTimer{nullptr};
    // Current cached peak overlay text (refreshed by timer, rendered every frame).
    QString         m_peakTextCache;

    // drawTextOverlay helper — renders a text string at a corner position on
    // the spectrum rect, using the given colour.
    void drawTextOverlay(QPainter& p, const QRect& specRect,
                         OverlayPosition pos, const QString& text,
                         const QColor& color);

    // ---- Coalesced settings save ----
    void scheduleSettingsSave();
    bool m_settingsSaveScheduled{false};

    // Recompute m_spectrumAverageAlpha + m_waterfallAverageAlpha from the
    // current per-side time constants and live FPS using the Thetis formula:
    //   α = exp(-1 / (fps × τ_seconds))
    // From Thetis specHPSDR.cs:351-380 [v2.10.3.13] AvTau / AvTauWF setters.
    void recomputeAverageAlphas();

    // ---- Mouse state ----
    bool   m_draggingDbm{false};
    int    m_dragStartY{0};
    float  m_dragStartRef{0.0f};
    QPoint m_mousePos;              // for cursor frequency display
    bool   m_mouseInWidget{false};

    // Filter edge drag — from AetherSDR SpectrumWidget.h:429-432
    enum class FilterEdge { None, Low, High };
    FilterEdge m_draggingFilter{FilterEdge::None};
    int  m_filterDragStartX{0};     // pixel X at grab time
    int  m_filterDragStartHz{0};    // filter edge Hz at grab time

    // Passband center drag (slide-to-tune) — AetherSDR:434
    bool m_draggingVfo{false};

    // Divider drag (spectrum/waterfall split) — AetherSDR:419
    bool m_draggingDivider{false};

    // Pan drag (waterfall/spectrum drag to change center) — AetherSDR:425-427
    bool   m_draggingPan{false};
    int    m_panDragStartX{0};
    double m_panDragStartCenter{0.0};

    // Bandwidth drag (frequency scale bar) — AetherSDR:421-423
    bool   m_draggingBandwidth{false};
    int    m_bwDragStartX{0};
    double m_bwDragStartBw{0.0};

    // Filter edge grab zone — from AetherSDR line 1087: GRAB = 5
    static constexpr int kFilterGrab = 5;

    // ---- HIGH SWR / PA safety overlay state ----
    // Ported from Thetis display.cs:4183-4201 [v2.10.3.13]
    bool m_highSwrActive{false};
    bool m_highSwrFoldback{false};

    // ---- MOX / TX overlay state (H.1, Phase 3M-1a) ----
    // From Thetis display.cs:1568 [v2.10.3.13]: static bool _mox = false;
    bool  m_moxOverlay{false};
    // TX attenuator cal offset — applied as an additional dBm shift during TX.
    // From Thetis display.cs:4840 [v2.10.3.13]: if (!local_mox) fOffset += rx1_preamp_offset;
    float m_txAttOffsetDb{0.0f};
    // TX filter visibility in spectrum panel.
    // From Thetis display.cs:2481 [v2.10.3.13]: DrawTXFilter flag.
    bool  m_txFilterVisible{false};

    // ---- Two-tone IMD overlay state (Phase 3M-4 Task 12) ----
    // From Thetis display.cs:5008 [v2.10.3.13] show condition. Owns an
    // ImdOverlay analytical core (peak detection + EMA + readout) that
    // is invoked from drawImdOverlay() each paint cycle when all four
    // flags below are true.
    bool m_testingIMD{false};            // Display.TestingIMD mirror
    bool m_showIMDMeasurements{false};   // Display.ShowIMDMeasurments mirror
    // displayduplex defaults to true in NereusSDR — the panadapter stays
    // live during MOX (the trace switches from RX to TX feedback), which
    // is the equivalent of Thetis's "duplex" mode where DisplayDuplex=true
    // routes feedback DDC streams to the RX1 panadapter window.  Wire
    // setDisplayDuplex(false) only if a Setup checkbox is added later.
    bool m_displayDuplex{true};
    // Allocated in the constructor (init list) so the include stays in
    // the .cpp.  std::unique_ptr would require pulling ImdOverlay.h into
    // the header.  Raw pointer with QObject parenting is the established
    // pattern for SpectrumWidget owned helpers.
    ImdOverlay* m_imdOverlay{nullptr};

    // ---- TX filter overlay range + mode (Plan 4 D9, Cluster E) ----
    // Audio-Hz edges of the TX passband; updated by setTxFilterRange().
    // IQ-space conversion (per deskhpsdr/transmitter.c:2136-2186 [@120188f])
    // is applied at draw time using m_txMode.
    int     m_txFilterLow{100};   // default matches TransmitModel::m_filterLow
    int     m_txFilterHigh{2900}; // default matches TransmitModel::m_filterHigh
    DSPMode m_txMode{DSPMode::USB};
    // Signed Hz offset added to m_vfoHz for the TX overlay position so the
    // orange band tracks XIT shifts (xitEnabled ? xitHz : 0).  Updated via
    // setTxVfoOffsetHz from MainWindow on SliceModel xit signals.
    int     m_txVfoOffsetHz{0};
    QColor  m_txFilterColor{255, 120, 60, 46}; // matches kTxFilterOverlayFill default
    // Plan 4 D9b (Cluster F): user-pickable RX filter overlay color.
    // Default matches Style::kRxFilterOverlayFill = "rgba(0, 180, 216, 80)".
    QColor  m_rxFilterColor{0x00, 0xb4, 0xd8, 80};

#ifdef NEREUS_GPU_SPECTRUM
    bool m_rhiInitialized{false};

    // GPU pipeline init helpers
    void initWaterfallPipeline();
    void initOverlayPipeline();
    void initSpectrumPipeline();
    void renderGpuFrame(QRhiCommandBuffer* cb);

    // ---- Waterfall GPU resources ----
    QRhiGraphicsPipeline*       m_wfPipeline{nullptr};
    QRhiShaderResourceBindings* m_wfSrb{nullptr};
    QRhiBuffer*                 m_wfVbo{nullptr};
    QRhiBuffer*                 m_wfUbo{nullptr};
    QRhiTexture*                m_wfGpuTex{nullptr};
    QRhiSampler*                m_wfSampler{nullptr};
    int  m_wfGpuTexW{0};
    int  m_wfGpuTexH{0};
    bool m_wfTexFullUpload{true};
    int  m_wfLastUploadedRow{-1};

    // ---- Overlay GPU resources ----
    QRhiGraphicsPipeline*       m_ovPipeline{nullptr};
    QRhiShaderResourceBindings* m_ovSrb{nullptr};
    QRhiBuffer*                 m_ovVbo{nullptr};
    QRhiTexture*                m_ovGpuTex{nullptr};
    QRhiSampler*                m_ovSampler{nullptr};
    QImage m_overlayStatic;
    bool   m_overlayStaticDirty{true};
    bool   m_overlayNeedsUpload{true};

    // ---- FFT spectrum GPU resources ----
    QRhiGraphicsPipeline*       m_fftLinePipeline{nullptr};
    QRhiGraphicsPipeline*       m_fftFillPipeline{nullptr};
    QRhiShaderResourceBindings* m_fftSrb{nullptr};
    QRhiBuffer*                 m_fftLineVbo{nullptr};
    QRhiBuffer*                 m_fftFillVbo{nullptr};
    // Phase 3G-8 commit 10: peak hold VBO — same layout as line VBO,
    // generated only when peak hold is enabled. m_peakHoldHasData is
    // false between peak decay resets so we skip the draw call.
    QRhiBuffer*                 m_fftPeakVbo{nullptr};
    bool                        m_peakHoldHasData{false};
    // From AetherSDR: kMaxFftBins = 8192, kFftVertStride = 6
    static constexpr int kMaxFftBins = 65536;
    static constexpr int kFftVertStride = 6;  // x, y, r, g, b, a
    int m_visibleBinCount{0};  // bins rendered this frame (for draw call count)

#endif

    // Invalidate the GPU-path cached overlay texture so grid, labels,
    // dBm scale, waterfall filter/zero-line/timestamp overlays, and
    // other QPainter-drawn chrome re-render on next frame. Safe no-op
    // when the GPU path is disabled.
    void markOverlayDirty() {
#ifdef NEREUS_GPU_SPECTRUM
        m_overlayStaticDirty = true;
#endif
        update();
    }
};

} // namespace NereusSDR
