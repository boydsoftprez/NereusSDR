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
#include <QTimer>
#include <QPropertyAnimation>

#include <utility>

#include "core/ConnectionState.h"

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
    void setWfColorGain(int gain) { m_wfColorGain = gain; }
    int  wfColorGain() const { return m_wfColorGain; }
    void setWfBlackLevel(int level) { m_wfBlackLevel = level; }
    int  wfBlackLevel() const { return m_wfBlackLevel; }

    // ---- Spectrum renderer controls (Phase 3G-8 commit 3) ----

    // Averaging mode applied in updateSpectrum() before drawing.
    void setAverageMode(AverageMode m);
    AverageMode averageMode() const { return m_averageMode; }

    // Smoothing time constant for Weighted / Logarithmic / TimeWindow.
    // 0.0 = no smoothing, 1.0 = infinite smoothing. Default kSmoothAlpha.
    void setAverageAlpha(float alpha);
    float averageAlpha() const { return m_averageAlpha; }

    // Peak hold: track per-bin max, decay after delay.
    void setPeakHoldEnabled(bool on);
    bool peakHoldEnabled() const { return m_peakHoldEnabled; }
    void setPeakHoldDelayMs(int ms);
    int  peakHoldDelayMs() const { return m_peakHoldDelayMs; }

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
    void setWfReverseScroll(bool on);
    bool wfReverseScroll() const { return m_wfReverseScroll; }
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
    void setZeroLineColor(const QColor& c);
    QColor zeroLineColor() const { return m_zeroLineColor; }
    void setBandEdgeColor(const QColor& c);
    QColor bandEdgeColor() const { return m_bandEdgeColor; }

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

    // ---- Per-pan settings persistence ----
    void setPanIndex(int idx) { m_panIndex = idx; }
    int  panIndex() const { return m_panIndex; }
    void loadSettings();
    void saveSettings();

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

    // Feed a new FFT frame. binsDbm are dBm values, one per frequency bin.
    // Called from the main thread after FFTEngine delivers the frame.
    void updateSpectrum(int receiverId, const QVector<float>& binsDbm);

    // ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
    // Reset the rewind ring buffer back to empty + live state. Public so
    // MainWindow can wire it to RadioModel::connectionStateChanged when
    // the radio disconnects (see plan §Task 4 Step 3-4 — NereusSDR has no
    // SpectrumWidget::clearDisplay() equivalent, so the flush is plumbed
    // through MainWindow rather than embedded in resizeEvent).
    // From AetherSDR SpectrumWidget.cpp:740-756 [@0cd4559]
    void clearWaterfallHistory();

signals:
    // Phase 3Q-8: emitted on a left-click while not Connected.
    // MainWindow wires this to showConnectionPanel().
    void disconnectedClickRequest();

    // Emitted when user clicks on spectrum/waterfall to tune
    void frequencyClicked(double hz);
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

    // ---- Coordinate helpers ----
    int    hzToX(double hz, const QRect& r) const;
    double xToHz(int x, const QRect& r) const;
    int    dbmToY(float dbm, const QRect& r) const;
    float  dbmToYf(float dbm, const QRect& r) const; // sub-pixel variant for antialiased trace

    // Returns kDbmStripW when the dBm scale strip is visible, 0 otherwise.
    // Used everywhere a rect excludes the right-edge strip so that hiding
    // the strip automatically gives the spectrum full widget width.
    int    effectiveStripW() const;

    // Returns the first and last FFT bin indices visible in the current
    // display window (m_centerHz ± m_bandwidthHz/2), mapped against
    // the DDC center and sample rate. Clamped to [0, binCount-1].
    std::pair<int, int> visibleBinRange(int binCount) const;

    // ---- Waterfall helpers ----
    void   pushWaterfallRow(const QVector<float>& bins);
    QRgb   dbmToRgb(float dbm) const;

    // ---- FFT data ----
    QVector<float> m_bins;       // latest raw FFT frame (dBm)
    QVector<float> m_smoothed;   // exponential-smoothed for display

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
    float       m_averageAlpha{0.05f};  // 0..1 exp-smoothing factor

    bool        m_peakHoldEnabled{false};
    int         m_peakHoldDelayMs{2000};
    QVector<float> m_peakHoldBins;
    QTimer*     m_peakHoldDecayTimer{nullptr};

    float       m_lineWidth{1.5f};
    bool        m_gradientEnabled{false};

    // Ported from Thetis Display.RX1DisplayCalOffset (display.cs:1372).
    float       m_dbmCalOffset{0.0f};

    // ---- Phase 3G-8 commit 4: waterfall renderer state ----

    bool  m_wfAgcEnabled{true};
    bool  m_clarityActive{false};     // Phase 3G-9c: suppresses legacy AGC when Clarity drives thresholds
    bool  m_wfReverseScroll{false};
    int   m_wfOpacity{100};           // 0..100
    int   m_wfUpdatePeriodMs{30};     // NereusSDR default per §10 divergence
    bool  m_wfUseSpectrumMinMax{false};
    AverageMode m_wfAverageMode{AverageMode::None};
    QVector<float> m_wfSmoothedBins;  // for wf averaging mode

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
    FreqLabelAlign m_freqLabelAlign{FreqLabelAlign::Center};

    NereusSDR::BandPlanManager* m_bandPlanMgr{nullptr};   // non-owning
    int                          m_bandPlanFontSize{6};   // 0 = off; AetherSDR default

    QColor m_gridColor{255, 255, 255, 40};       // vertical freq grid
    QColor m_gridFineColor{255, 255, 255, 20};   // 1/5 step fine grid
    QColor m_hGridColor{255, 255, 255, 40};      // horizontal dBm grid
    QColor m_gridTextColor{255, 255, 0};         // yellow text default
    QColor m_zeroLineColor{255, 0, 0};           // red default (Thetis)
    QColor m_bandEdgeColor{255, 0, 0};           // red default (Thetis)

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

    // ---- Coalesced settings save ----
    void scheduleSettingsSave();
    bool m_settingsSaveScheduled{false};

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
