// =================================================================
// src/gui/SpectrumWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-04  J.J. Boyd / KG4VCF. Keep 2D TX history aligned on the
//                 restored RX frequency axis using each row's capture
//                 window. Original history pixels stay intact.
//                 AI-assisted via OpenAI Codex.
//   2026-10-01  J.J. Boyd / KG4VCF. Opt-in numeric RX history diagnostics.
//                 AI-assisted via OpenAI Codex.
//   2026-10-02 J.J. Boyd / KG4VCF : issues #141/#147: CPU paint, GPU
//                 paint and interaction use the existing GPU panel layout;
//                 divider drag reserves the same chrome height.
//                 AI-assisted via OpenAI Codex.
//   2026-09-30 J.J. Boyd / KG4VCF : GUI memory leak: renderGpuFrame writes
//                 the 2D trace's line, fill and peak vertex buffers only in
//                 a frame that binds them. A 3D pan (and a 2D pan with pan
//                 fill off, for the fill) wrote them every frame unbound,
//                 and Qt's Metal backend kept every write pending: 23 GB in
//                 2.5 hours. AI-assisted via Anthropic Claude Code.
//   2026-09-30 J.J. Boyd / KG4VCF : desktop listening fix 3: the edge
//                 arrow is drawn by drawOffScreenArrow, for the pan's VFO
//                 and, one row each, for a slice shown here only as its
//                 flag (setEdgeMarkedSlice). AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-30 J.J. Boyd / KG4VCF : desktop listening lane review: a
//                 marker's click says who controls the slice and offers
//                 Take control, in the listener words; nobody's slice says
//                 so. AI-assisted via Anthropic Claude Code.
//   2026-09-30 J.J. Boyd / KG4VCF : Level Cal fix wave: while the level
//                 calibration holds the grid's noise floor follow off, the
//                 saved value stays the user's (setGridFollowSaveHold).
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 J.J. Boyd / KG4VCF : a remote window's waterfall AGC and
//                 NF-AGC colour against the Core's levels (display extras)
//                 when the Core offers them. AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF : the grid's noise-floor tracking follows
//                 the pan's own display noise floor every 500 ms, as Thetis
//                 tmrAutoAGC does, so it works with Clarity off and in a
//                 remote window; a pan Clarity feeds keeps Clarity's grid.
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF : settings stored once for every pan
//                 (overlays, normalize, peak value, grid noise-floor
//                 tracking, band plan size) reach every pan when one
//                 changes (shareWithOtherPans). AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF : setAverageMode routes to the split
//                 spectrum averaging the renderer reads (Reset to Smooth
//                 Defaults). AI-assisted via Anthropic Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF : the 1 Hz normalise applies only
//                 with the Average, Sample and RMS spectrum detectors
//                 (normalizeActive). AI-assisted via Anthropic Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF : persist the local trace/fill colour
//                 per pan and let per-pan appearance colours inherit pan 0
//                 until overridden. AI-assisted via OpenAI Codex.
//   2026-09-27 J.J. Boyd / KG4VCF : local receive waterfall rows use
//                 RX1Offset before colour and level tracking, matching
//                 Core-calibrated remote rows. AI-assisted via OpenAI Codex.
//   2026-09-26 J.J. Boyd / KG4VCF : other devices' slices drawn as markers
//                 (drawForeignMarkers), their label's click says whose
//                 they are (iPhone app plan Task 78, R-IOS-02, R-IOS-30).
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 J.J. Boyd / KG4VCF : the band-plan strip's lowest-licence-
//                 class rule moved unchanged to lowestLicenceClass() in
//                 models/BandPlan.h, which the station catalogue also
//                 calls (R-IOS-27, R-IOS-11). AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-24 J.J. Boyd / KG4VCF : processNoiseFloor, the fast-attack
//                 setter, the waterfall level composition, the normalise
//                 shift, the averaging constant, the calibration range and
//                 the peak-blob passband moved unchanged to
//                 core/spectrum/DisplayFollowers, and PeakBlobDetector and
//                 ActivePeakHoldTrace to core/spectrum (iPhone app Task 20,
//                 R-IOS-27). AI-assisted via Anthropic Claude Code.
//   2026-09-24 J.J. Boyd / KG4VCF : the waterfall palettes' gradient stops
//                 and wfSchemeStops() moved unchanged to
//                 core/spectrum/WaterfallPalettes.cpp (iPhone app Task 19,
//                 R-IOS-06). AI-assisted via Anthropic Claude Code.
//   2026-09-24 J.J. Boyd / KG4VCF : setPeakHoldEnabled emits
//                 peakHoldEnabledChanged (R-R3-49, R-R3-21). AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-22 J.J. Boyd / KG4VCF — adapt AetherSDR prepareForShutdown()
//                 [@0dea0dd7]; native child teardown only, preserving the
//                 standalone QRhi owner's lifetime (OpenAI Codex).
//   2026-09-22 J.J. Boyd / KG4VCF — share wing reference with Core (OpenAI Codex).
//   2026-09-26 J.J. Boyd / KG4VCF - R-R3-01 / R-R3-08 / R-R3-12 (parity
//                 Task 17): a remote pan's bin width, normalise shift and
//                 Hz/bin readout follow the Core's granted FFT size, and its
//                 peak hold, blob and noise floor decay follow the frame
//                 rate the Core sends. AI-assisted via Anthropic Claude Code.
//   2026-09-26 J.J. Boyd / KG4VCF - R-R3-49 / R-R3-12 (parity Task 29): the
//                 GPU path draws no stale receive trace while the MOX overlay
//                 is on without transmit pixels (drawsSpectrumTrace).
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26 J.J. Boyd / KG4VCF - R-R3-49 (parity Tasks 27-29 fix wave):
//                 a remote pan's rise and fall retire the other axis's trace,
//                 so the GPU path cannot redraw it under the new axis before
//                 a frame for it lands. AI-assisted via Anthropic Claude Code.
//   2026-09-26 J.J. Boyd / KG4VCF - R-R3-21 / R-R3-08: a remote pan's
//                 waterfall rows queue for the ticker (one a tick, two while
//                 more than two wait) with the RF window they were captured
//                 at, instead of one pending row. AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-27 J.J. Boyd / KG4VCF - A11 / R-R3-49 (parity Task 31): display
//                 duplex (DUP), off by default as Thetis. Keyed with it on,
//                 the pan keeps the receive span and bins under the transmit
//                 grid and waterfall levels, the TX filter sits against the
//                 receive span without XIT, the receive trace takes the
//                 transmit calibration (RX1Offset), and a change while keyed
//                 swaps the view and resets the blob maxima and active peaks.
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF - parity ruling C13: the performance
//                 overlay's lines come from perfOverlayLines(), which in a
//                 remote window adds the Core's drops after this computer's
//                 counters, each group headed. AI-assisted via Anthropic
//                 Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-10-01: Completed attribution for per-slice marker colors
//                 and selected-last paint order
//                 by J.J. Boyd (KG4VCF), with AI assistance via
//                 OpenAI Codex. Port introduced 2026-09-23.
//                 Source: AetherSDR src/gui/SpectrumWidget.cpp [@9f81dc00].
//                 Upstream has no per-file copyright header.
//                 Copyright (C) 2024-2026 Jeremy (KK7GWY) and
//                 AetherSDR contributors. GPLv3; project source:
//                 https://github.com/ten9876/AetherSDR
// =================================================================

// =================================================================
// Source attribution for 3D stacked-trace spectrum (AetherSDR, GPLv3):
//   Project lead: Jeremy (KK7GWY) / AetherSDR contributors
//   https://github.com/ten9876/AetherSDR
//   Upstream files at 1872028c: src/gui/SpectrumWidget.cpp,
//   src/gui/DssRenderer.cpp, resources/shaders/dss_mesh.vert, and
//   resources/shaders/dss_mesh.frag. AetherSDR has no per-file license
//   header; its project LICENSE is GPLv3.
//
// Modification history (3D stacked-trace port, NereusSDR):
//   2026-10-02 - Integrated the display bindings and speed fold with
//                landed remote capture, RX/TX and marker paths for
//                NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                integration via OpenAI Codex.
//   2026-08-08 - Adapted the AetherSDR 3D renderer and GPU upload path
//                for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code. The 3D Speed
//                row divider and peak-hold fold are NereusSDR additions.
//   2026-10-01 - Corrected frame-fold and skipped-paint GPU uploads for
//                NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via OpenAI Codex.
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

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include "SpectrumWidget.h"

#include <tuple>
#include "core/session/media/SpectrumEndpoint.h"
#include "SpectrumOverlayMenu.h"
#include "core/WidebandFftEngine.h"
#include "core/spectrum/WidebandDisplayReference.h"
#include "core/session/media/DssWideRow.h"
#include "core/spectrum/SpectrumDetector.h"
#include "ImdOverlay.h"
#include "gui/DssMeshGeometry.h"
#include "gui/SpectrumTraceFramePlan.h"
#include "StyleConstants.h"
#include "spectrum/WaterfallTicker.h"
#include "widgets/VfoWidget.h"
#include "ColorSwatchButton.h"
#include "widgets/GradientPickerWidget.h"
#include "core/AppSettings.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "core/LogCategories.h"   // Phase 3M-4 bench-fix Round 2: lcSpectrum
#include "dbm_strip_math.h"
#include "popup_placement.h"
#include "models/BandPlanManager.h"
#include "models/Band.h"
#include "models/DisplaySettingsModel.h"
#include "models/NotchModel.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QDesktopServices>
#include <QScopedValueRollback>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QLabel>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSignalBlocker>
#include <QToolTip>
#include <QUrl>

#include <QDateTime>
#include <QElapsedTimer>
#include <QTimeZone>

#include "core/MemoryLock.h"
#include "core/MemoryPressure.h"
#include "core/PerfMonitor.h"
#include <QMap>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFile>
// Pre-existing CPU-build gap (predates the 3DSS port): m_waterfallTickerThread
// calls QThread::start()/quit()/wait() and connects &QThread::started /
// &QThread::finished, all needing the complete type. The GPU build got this
// transitively through <rhi/qrhi.h>'s Qt-private includes; CPU-only never
// pulled it in, so <QThread> was only ever forward-declared and the build
// failed with -DNEREUS_GPU_SPECTRUM=OFF. Caught while verifying that
// configuration for this fix wave.
#include <QThread>

#ifdef NEREUS_GPU_SPECTRUM
#include <rhi/qshader.h>
#endif

#include <algorithm>
#include <vector>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

namespace NereusSDR {

// Visual-equality helper for the spot-marker repaint guard.  Drops a
// redundant overlay repaint whenever a spot-client poll lands on an
// unchanged set (frequent in steady state — DX cluster + WSJT-X +
// FreeDV Reporter all repeat the same active station entries every
// few seconds, and the drawSpotMarkers path is the most expensive
// item on the overlay canvas).
//
// From AetherSDR src/gui/SpectrumWidget.cpp [@a173272d] PR #2474
// ("Avoid unchanged spot overlay repaints").  Same comparison fields
// (callsign / freqMhz / color / dxccColor / source) — non-visual
// fields like spotterCallsign / comment / timestampMs intentionally
// excluded so a comment-only update does not force a repaint.
static bool spotMarkersVisuallyEqual(const QVector<SpectrumWidget::SpotMarker>& lhs,
                                     const QVector<SpectrumWidget::SpotMarker>& rhs)
{
    constexpr double kFrequencyEpsilonMhz = 1.0e-6;

    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (qsizetype i = 0; i < lhs.size(); ++i) {
        const SpectrumWidget::SpotMarker& a = lhs.at(i);
        const SpectrumWidget::SpotMarker& b = rhs.at(i);
        if (a.callsign != b.callsign
            || std::abs(a.freqMhz - b.freqMhz) > kFrequencyEpsilonMhz
            || a.color != b.color
            || a.dxccColor != b.dxccColor
            || a.source != b.source) {
            return false;
        }
    }
    return true;
}

// The waterfall palettes' gradient stops and wfSchemeStops() moved to
// core/spectrum/WaterfallPalettes.cpp (iPhone app Task 19), which the Core's
// catalogue reads too.

// Interpolate a 0..1 position across a scheme's gradient stops. Extracted
// from dbmToRgb()'s inline loop so the 3DSS palette can share the stops
// without inheriting the waterfall gain / black-level window applied above
// it. Behaviour is unchanged for dbmToRgb.
QRgb interpolateWfGradient(float t, const WfGradientStop* stops, int count)
{
    const float adjusted = qBound(0.0f, t, 1.0f);
    // Find the two surrounding stops and interpolate
    for (int i = 0; i < count - 1; ++i) {
        if (adjusted <= stops[i + 1].pos) {
            const float f = (adjusted - stops[i].pos)
                          / (stops[i + 1].pos - stops[i].pos);
            const int r = static_cast<int>(
                stops[i].r + f * (stops[i + 1].r - stops[i].r));
            const int g = static_cast<int>(
                stops[i].g + f * (stops[i + 1].g - stops[i].g));
            const int b = static_cast<int>(
                stops[i].b + f * (stops[i + 1].b - stops[i].b));
            return qRgb(r, g, b);
        }
    }
    return qRgb(stops[count - 1].r, stops[count - 1].g, stops[count - 1].b);
}

// ---- SpectrumWidget ----

// Every SpectrumWidget, for the settings every pan shares (Spectrum Peaks:
// reloadSpectrumPeaksSettingsOnAllPans; the rest: shareWithOtherPans).
// GUI thread only.
namespace {
std::vector<SpectrumWidget*>& spectrumPeaksPans()
{
    static std::vector<SpectrumWidget*> pans;
    return pans;
}
} // namespace

SpectrumWidget::SpectrumWidget(QWidget* parent)
    : SpectrumBaseClass(parent)
{
    spectrumPeaksPans().push_back(this);
    setMinimumSize(400, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);

#ifdef NEREUS_GPU_SPECTRUM
    // Platform-specific QRhi backend selection.
    // Order matters: setApi() first, then WA_NativeWindow, then setMouseTracking().
    // WA_NativeWindow creates a dedicated native surface (NSView on macOS, HWND on
    // Windows); setMouseTracking() must come AFTER so tracking is configured on
    // the final native surface.
#ifdef Q_OS_MAC
    setApi(QRhiWidget::Api::Metal);
    // Was a bare setAttribute(Qt::WA_NativeWindow) here. The pair now goes
    // through the helper so the float/dock path can re-assert both together
    // after it re-realizes the native window; see the header.
    applyNativeWindowIsolationPolicy();
    setAttribute(Qt::WA_Hover);  // Ensure HoverMove events are delivered
#elif defined(Q_OS_WIN)
    setApi(QRhiWidget::Api::Direct3D11);
    setAttribute(Qt::WA_NativeWindow);
#endif
    // 2026-05-26 KG4VCF perf polish: SpectrumWidget paints every pixel
    // of its rect every frame (GPU clears + draws spectrum + waterfall
    // + overlay + dynamic overlay; CPU fallback also paints full area).
    // Tell Qt to skip its default pre-paint alpha-channel clear --
    // small per-paint win, no visual difference.
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
#else
    // CPU fallback: dark background
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0x0f, 0x0f, 0x1a));
    setPalette(pal);
#endif

    setCursor(Qt::CrossCursor);
    setMouseTracking(true);

    // QRhiWidget on macOS Metal does not deliver mouseMoveEvent without button press.
    // Workaround: a transparent QWidget overlay that receives mouse tracking events
    // and forwards them. The overlay sits on top of the QRhiWidget, passes through
    // all clicks and drags, but captures hover movement.
    // Note: QRhiWidget with WA_NativeWindow on macOS does not support child widget
    // overlays or mouse tracking without button press. Zoom control is handled via
    // the frequency scale bar inside the QRhiWidget's own mouse press/drag events
    // (which DO work when a button is pressed).

    // Timer-driven display repaint decouples paint rate from FFT data
    // arrival, so the displayed frames are evenly spaced regardless of
    // I/Q buffer fill timing.  Default 30 fps matches the FFT engine's
    // default output rate (FFTEngine::setOutputFps(30) in MainWindow),
    // so each paint pulls exactly one fresh waterfall row in steady
    // state.  setDisplayFps() lets Setup -> Display drive this to
    // anything in [1, 60] and the persisted DisplaySpectrumFps key
    // restores it across launches.
    static constexpr int kDefaultDisplayFps = 30;
    m_displayTimer.setInterval(1000 / kDefaultDisplayFps); // 33 ms
    m_displayTimer.setTimerType(Qt::PreciseTimer);  // sub-ms accuracy
    m_displayTimer.setSingleShot(false);
    connect(&m_displayTimer, &QTimer::timeout, this, [this]() {
        // 2026-05-25 KG4VCF Option B revisit: the original Option B drop
        // of this gate ("always repaint at display cadence") looked great
        // on a healthy system but pegged a CPU core under macOS low-power
        // mode + high system load (load average ~8, 99% on one core)
        // because every display tick unconditionally pumped the GPU
        // pipeline.  Restore the gate; the sub-row interpolation still
        // helps because paint timing within a push period varies, so
        // effectiveRow varies sample-to-sample even with the gate in.
        // Trade-off: animation cadence is tied to FFT-arrival cadence
        // (not display cadence), but FFT arrival drives push too, so the
        // two stay locked in steady state.
        if (m_hasNewSpectrum) {
            m_hasNewSpectrum = false;
            update();
        }

        // 3DSS: advance the continuous scroll-progress accumulator from
        // wall clock between DSS row pushes (see m_dssScrollProgressRows'
        // header comment). Gated on Mode3D to match the "no wasted 2D
        // work" shape the rest of the 3DSS pipeline already follows
        // (rebuildDssMeshIfNeeded/uploadDssPaletteLut/uploadDssHeightRows
        // in renderGpuFrame()); m_dssLastTickMs resets to 0 on leaving 3D
        // so a stale multi-second gap can never appear as one giant jump
        // when 3D is re-entered.
        if (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) {
            const qint64 nowTickMs = QDateTime::currentMSecsSinceEpoch();
            if (m_dssLastTickMs > 0) {
                // Task 24: the increment is factored out into
                // dssScrollIncrement() so it can scale by
                // effectiveDssRowDivider() -- with divider N the glide now
                // takes N waterfall ticks (not one) to reach a pushed row,
                // matching accumulateDssRow()'s fold.
                const int deltaMs =
                    static_cast<int>(nowTickMs - m_dssLastTickMs);
                m_dssScrollProgressRows = qBound(0.0f,
                    m_dssScrollProgressRows + dssScrollIncrement(deltaMs), 1.0f);
            }
            m_dssLastTickMs = nowTickMs;
        } else {
            m_dssLastTickMs = 0;
        }
    });
    m_displayTimer.start();

    // 2026-05-25 KG4VCF bench fix: timer-driven waterfall row push.
    // Bench symptom: "waterfall scroll still appears to jitter,
    // occasional stutter".  Root cause: pushWaterfallRow was called
    // directly from updateSpectrumLinear at FFT-arrival timing.
    // Network-burst UDP delivery clusters multiple FFTs into a few ms
    // followed by a gap; the previous rate-limit-by-drop in
    // pushWaterfallRow coalesced the burst into one push, then left
    // a visible gap until the next FFT arrived.  Move the push to a
    // dedicated QTimer so the texture write happens at strictly
    // m_wfUpdatePeriodMs cadence regardless of FFT arrival pattern.
    // 2026-05-25 KG4VCF bench fix #3: PreciseTimer (vs Qt's default
    // CoarseTimer with 5% drift) keeps the push cadence within ~1 ms
    // of nominal.  CoarseTimer's ~5% slop on a 33 ms interval
    // accumulates ~50 ms = one missed frame per second, which matches
    // the operator's report of a ~1 Hz scroll hitch.
    // 2026-05-25 KG4VCF bench fix #4 (Option A): WaterfallTicker on a
    // dedicated worker thread.  The QTimer used to live here on the
    // main thread (PreciseTimer + always-push + correctly-synced cadence
    // were already in place from fixes #1-#3), but any momentary main-
    // thread block (focus event, layout pass, system notification)
    // could still delay the tick firing.  Move the timer onto its own
    // QThread so the tick fires regardless of main-thread state; the
    // queued signal then lands in the main thread's event loop and the
    // pushWaterfallRow callback runs ASAP -- if main is busy, the queued
    // ticks accumulate and drain in a burst, which the eye reads as
    // continuous scroll instead of the long-pause-then-jump pattern.
    m_waterfallTickerThread = new QThread(this);
    m_waterfallTickerThread->setObjectName(QStringLiteral("WaterfallTickerThread"));
    m_waterfallTicker = new WaterfallTicker();  // no parent -- moved to thread
    // moveToWorkerThread() moves m_timer (a value member, hence not a
    // QObject child) along with *this. moveToThread() alone strands
    // m_timer on main, which then refuses start() from the worker.
    m_waterfallTicker->moveToWorkerThread(m_waterfallTickerThread);
    connect(m_waterfallTickerThread, &QThread::finished,
            m_waterfallTicker, &QObject::deleteLater);
    // Elevate the ticker thread to USER_INTERACTIVE QoS so its event
    // loop (which carries the PreciseTimer) sits in the same scheduling
    // class as the GUI + DSP threads and is consistently preferred over
    // compile workers (DEFAULT QoS) under heavy build load.  Earlier
    // revisions used USER_INITIATED here; 2026-05-26 KG4VCF bench
    // showed that tier still let the ticker get preempted by ninja
    // workers, producing visible waterfall stutter on build kickoff.
    connect(m_waterfallTickerThread, &QThread::started,
            m_waterfallTicker,
            []() { NereusSDR::elevateLatencyCriticalThreadPriority(); });
    // Queued connection (default for cross-thread): tick fires on the
    // ticker thread, slot runs on the main thread when the event loop
    // is free.  See WaterfallTicker.h for the cadence-isolation rationale.
    connect(m_waterfallTicker, &WaterfallTicker::tick, this, [this]() {
        if (m_waterfallTickerPausedForTest) {
            return;
        }
        onWaterfallTick();
    }, Qt::QueuedConnection);
    m_waterfallTickerThread->start();
    m_waterfallTicker->setUpdatePeriodMs(m_wfUpdatePeriodMs);
    m_waterfallTicker->start();

    // Sub-epic E: debounce timer for waterfall history re-allocation
    // From AetherSDR SpectrumWidget.cpp:158-168 [@2bb3b5c]
    // (debounce timer added by unmerged AetherSDR PR #1478 — see plan §authoring-time #2)
    m_historyResizeTimer = new QTimer(this);
    m_historyResizeTimer->setSingleShot(true);
    m_historyResizeTimer->setInterval(250);
    connect(m_historyResizeTimer, &QTimer::timeout, this, [this]() {
        ensureWaterfallHistory();
        if (m_wfHistoryRowCount > 0) {
            rebuildWaterfallViewport();
        }
    });

    // 2026-05-26 KG4VCF perf instrumentation: 1 Hz poll that updates
    // PerfMonitor's memory-pressure sample and forces the overlay
    // texture to rebuild so the perf overlay (drawn into m_overlayStatic)
    // refreshes with the current stats.  Started only when the perf
    // overlay is toggled on (setShowPerfOverlay).
    m_perfPollTimer = new QTimer(this);
    m_perfPollTimer->setInterval(1000);
    connect(m_perfPollTimer, &QTimer::timeout, this, [this]() {
        const auto sample = pollMemoryPressure();
        PerfMonitor::instance().setMemoryStats(
            sample.compressing, sample.footprintMb);
        // 2026-05-26 KG4VCF: snapshot + cache + log a single line per
        // second so the operator (and anyone reading the launch log)
        // can grep "perf:" for ground-truth numbers without staring at
        // the overlay.  snapshotAndClearDeltas consumes the deltas
        // here; the overlay paint reads via lastSnapshot() to avoid
        // double-clearing.
        const auto s = PerfMonitor::instance().snapshotAndClearDeltas();
        const auto ml = memoryLockStats();
        qInfo().noquote() << QString(
            "perf: paint %1/%2 ms gap %3/%4 ms fft %5/%6 ms ovly %7/%8 ms"
            " audio_fill %9/%10 ms underruns %11 (+%12/s) udp %13 (+%14/s)"
            " tx_iq_under %15 (+%16/s)"
            " mem %17 MB%18 mlock %19 regions / %20 MB")
            .arg(s.paintMsAvg, 0, 'f', 1).arg(s.paintMsMax, 0, 'f', 1)
            .arg(s.gapMsAvg,   0, 'f', 1).arg(s.gapMsMax,   0, 'f', 1)
            .arg(s.fftMsAvg,   0, 'f', 1).arg(s.fftMsMax,   0, 'f', 1)
            .arg(s.ovlyMsAvg,  0, 'f', 1).arg(s.ovlyMsMax,  0, 'f', 1)
            .arg(s.audioFillAvgMs, 0, 'f', 1)
            .arg(s.audioFillMinMs, 0, 'f', 1)
            .arg(s.audioUnderrunsTotal).arg(s.audioUnderrunsDelta)
            .arg(s.udpDropsTotal).arg(s.udpDropsDelta)
            .arg(s.txIqUnderrunsTotal).arg(s.txIqUnderrunsDelta)
            .arg(s.memFootprintMb, 0, 'f', 0)
            .arg(s.memCompressing ? QStringLiteral(" COMPRESSING")
                                  : QString{})
            .arg(ml.regionsLocked)
            .arg(ml.bytesLocked / (1024.0 * 1024.0), 0, 'f', 1);
        markOverlayDirty();
        update();
    });
    // Restore persisted toggle (default off).  setShowPerfOverlay
    // handles the timer-start side effect.
    {
        const bool persisted =
            AppSettings::instance()
                .value(QStringLiteral("ShowPerfOverlay"),
                       QStringLiteral("False")).toString()
            == QStringLiteral("True");
        if (persisted) {
            setShowPerfOverlay(true);
        }
    }

    // Phase 3Q-8: child label for the disconnect overlay. Composites in both
    // CPU and GPU paint paths (QRhi early-returns from paintEvent so a QPainter
    // overlay there would crash; a child QWidget gets stacked by Qt instead).
    m_disconnectLabel = new QLabel(QStringLiteral("DISCONNECTED"), this);
    m_disconnectLabel->setAlignment(Qt::AlignCenter);
    m_disconnectLabel->setStyleSheet(QStringLiteral(
        "QLabel { background-color: rgba(10, 12, 20, 200);"
        " color: #c14848; font-size: 36pt; font-weight: bold;"
        " letter-spacing: 8px; }"));
    m_disconnectLabel->hide();
    m_disconnectLabel->installEventFilter(this);

    // Phase 3M-4 Task 12 — two-tone IMD overlay analytical core.
    // Owned via QObject parenting; raw pointer mirrors m_bandPlanManager.
    m_imdOverlay = new ImdOverlay(this);

    // 3D Stacked-Trace Spectrum Plan Task 18: create and bind this
    // widget's DisplaySettingsModel last, after every other member is
    // initialised. m_panIndex is already known here (its in-class
    // initializer runs before this constructor body, and no earlier
    // statement above changes it), so the model starts scoped to
    // whatever pan this widget currently claims; a caller that later
    // calls setPanIndex() re-scopes both together.
    m_displaySettings = new DisplaySettingsModel(this);
    m_displaySettings->setPanIndex(m_panIndex);
    bindDisplaySettings();
}

SpectrumWidget::~SpectrumWidget()
{
    auto& pans = spectrumPeaksPans();
    pans.erase(std::remove(pans.begin(), pans.end(), this), pans.end());
    prepareForShutdown();
    // 2026-05-25 KG4VCF bench fix #4: shut down the waterfall ticker
    // thread cleanly so its QTimer + event loop are torn down before
    // m_waterfallTicker is deleted (which happens via the finished ->
    // deleteLater wire).
    if (m_waterfallTicker) {
        m_waterfallTicker->stop();
    }
    if (m_waterfallTickerThread != nullptr) {
        m_waterfallTickerThread->quit();
        m_waterfallTickerThread->wait(2000);
    }
}

// ---- Settings persistence ----
// Per-pan keys use AetherSDR pattern: "DisplayFftSize" for pan 0, "DisplayFftSize_1" for pan 1
static QString settingsKey(const QString& base, int panIndex)
{
    if (panIndex == 0) {
        return base;
    }
    return QStringLiteral("%1_%2").arg(base).arg(panIndex);
}

// 3D Stacked-Trace Spectrum Plan Task 18: forwards to m_displaySettings
// so the owned model's per-pan keys always track this widget's own. The
// guard is defensive only -- by the time any external caller can reach
// this method the constructor has already run to completion and
// m_displaySettings is never null again after that -- but costs nothing
// to keep honest.
void SpectrumWidget::setPanIndex(int idx)
{
    m_panIndex = idx;
    if (m_displaySettings) {
        m_displaySettings->setPanIndex(idx);
    }
}

void SpectrumWidget::loadSettings()
{
    auto& s = AppSettings::instance();

    // A pan that has never been configured looks like pan 0.
    //
    // Bench report 2026-07-30 (JJ, KG4VCF): the second and later pans did
    // not honour the display settings. Two separate causes; this is the
    // second. Setup was pushing to one widget (fixed in MainWindow), and
    // separately a pan opening for the first time had no keys of its own, so
    // every read fell through to the hardcoded ship defaults and the new pan
    // came up looking nothing like the one beside it.
    //
    // Inheriting is done at read time rather than by copying pan 0's keys
    // into the new pan's namespace. Copying would make the new pan's
    // settings independent immediately, freezing whatever pan 0 happened to
    // look like at that instant; the fallback instead means an untouched pan
    // keeps following pan 0, and stops the moment the operator gives it a
    // value of its own, because that write creates the per-pan key which
    // then wins. "Follow pan 1 until you say otherwise", which is the model
    // chosen on 2026-07-30, expressed in one place.
    //
    // Pan 0 has no fallback to take, and must not: settingsKey(base, 0)
    // returns `base` itself, so recursing would just re-read the same key.
    auto rawValue = [&](const QString& key) -> QString {
        const QString own = s.value(settingsKey(key, m_panIndex)).toString();
        if (!own.isEmpty() || m_panIndex == 0) { return own; }
        return s.value(settingsKey(key, 0)).toString();
    };

    auto readFloat = [&](const QString& key, float def) -> float {
        QString val = rawValue(key);
        if (val.isEmpty()) { return def; }
        bool ok = false;
        float v = val.toFloat(&ok);
        return ok ? v : def;
    };
    auto readInt = [&](const QString& key, int def) -> int {
        QString val = rawValue(key);
        if (val.isEmpty()) { return def; }
        bool ok = false;
        int v = val.toInt(&ok);
        return ok ? v : def;
    };
    auto readBool = [&](const QString& key, bool def) -> bool {
        const QString val = rawValue(key);
        if (val.isEmpty()) { return def; }
        return val == QStringLiteral("True");
    };

    // Note for anyone adding a setting here: keys wrapped in settingsKey()
    // are per pan and inherit pan 0 through rawValue above. Keys read
    // straight off AppSettings (Active Peak Hold, Peak Blobs) are global and
    // every pan already sees the same value, which is deliberate and matches
    // Thetis keeping some display settings global while splitting others
    // per receiver (SpectrumGridMax vs RX2SpectrumGridMax, display.cs:1750
    // and :1855 [v2.10.3.15]). Pick which one a new setting is; do not read
    // a per-pan key without going through rawValue, or that setting will
    // silently stop inheriting.

    // 3D Stacked-Trace Spectrum Plan Task 23: DisplaySettingsModel is now
    // the persister for its own fourteen fields (Colour Scheme, Colour
    // Gain, Black Level, Ref Level, Dyn Range, Fill Alpha, Fill trace,
    // Spectrum mode, 3D Gain, 3D Span, 3D Angle, 3D Speed, 3D Slice
    // Shadow, split fraction) -- this function no longer reads any of
    // their AppSettings keys itself. Load the model once, up front, with
    // its own signals blocked: DisplaySettingsModel::load() calls its own
    // setters, which -- through bindDisplaySettings()'s model-to-widget
    // connections -- would otherwise fire straight back into this
    // widget's OWN appliers (setRefLevel(), setWfColorGain(), ...) for
    // every seeded value that differs from the model's just-constructed
    // ship default. Each of those appliers calls update() and
    // scheduleSettingsSave() and increments m_displaySettingsApplyCount,
    // none of which loadSettings() is allowed to do (see
    // displaySettingsApplyCountForTest()'s "no apply during load"
    // contract). Blocking the model's signals lets its own members settle
    // silently; every read of the fourteen below this point copies the
    // model's freshly loaded getter straight into this widget's member,
    // never through this widget's own setters. 3D Floor is excluded from
    // the model's load() (see DisplaySettingsModel.h's file header) and
    // stays out of this function exactly as before Task 23.
    {
        const QSignalBlocker blocker(m_displaySettings);
        m_displaySettings->load();
    }

    // Ship defaults — calibrated 2026-04-30 against a live ANAN-G2 with
    // a typical residential noise floor (-115 to -120 dBm in the
    // amateur HF bands). Earlier defaults ran 12 dB hotter (Grid -36 /
    // -104, Wf -50 / -110, Wf black 98) and gave a noisy first-launch
    // experience — band noise jammed the bottom of the panadapter and
    // lit up the waterfall floor. Shifting the entire reference plane
    // down 12 dB gives a clean "noise sits low" first impression.
    // Dynamic range (68 dB grid, 60 dB waterfall) is unchanged. Now
    // sourced from the model (Task 23), which carries the identical
    // defaults (DisplaySettingsModel.h's m_refLevel/m_dynamicRange
    // in-class initializers).
    m_refLevel       = m_displaySettings->refLevel();
    m_dynamicRange   = m_displaySettings->dynamicRange();
    m_spectrumFrac   = m_displaySettings->spectrumFrac();

    // Phase 3G-12: persist the spectrum zoom level (visible bandwidth)
    // across app restarts. Center frequency is persisted indirectly via
    // SliceModel (slice frequency), so only bandwidth needs its own key.
    // Default 192000 Hz = 192 kHz matches the P1 base sample rate.
    m_bandwidthHz    = static_cast<double>(
                          readFloat(QStringLiteral("DisplayBandwidth"), 192000.0f));
    m_wfColorGain    = m_displaySettings->wfColorGain();   // Task 23: was readInt(DisplayWfColorGain)
    m_wfBlackLevel   = m_displaySettings->wfBlackLevel();  // Task 23: was readInt(DisplayWfBlackLevel)
    m_wfHighThreshold = readFloat(QStringLiteral("DisplayWfHighLevel"), -62.0f);
    m_wfLowThreshold = readFloat(QStringLiteral("DisplayWfLowLevel"), -122.0f);
    // Seed render-active mirror from persistent user values — matches
    // Thetis's per-render local seed at display.cs:6575/6590
    // [v2.10.3.13]. AGC / NF-AGC / Clarity will override these at
    // composeWaterfallActiveThresholds() time; the persistent fields
    // above stay untouched (issue #230 fix).
    m_wfActiveHighThreshold = m_wfHighThreshold;
    m_wfActiveLowThreshold  = m_wfLowThreshold;
    m_fillAlpha      = m_displaySettings->fillAlpha();     // Task 23: was readFloat(DisplayFftFillAlpha)
    m_panFill        = m_displaySettings->panFill();       // Task 23: was readBool(DisplayPanFill)

    m_ctunEnabled    = readBool(QStringLiteral("DisplayCtunEnabled"), true);
    m_ctunPreference = m_ctunEnabled;

    // Task 23: was readInt(DisplayWfColorScheme) + qBound(0, ..., Count-1);
    // the model's own setWfColorScheme() clamps to the identical [0,7]
    // range (WfColorScheme::Count == 8).
    m_wfColorScheme = static_cast<WfColorScheme>(m_displaySettings->wfColorScheme());

    // Phase 3G-8 commit 3: spectrum renderer state.
    // DisplayAverageMode + DisplayAverageAlpha are retired keys (v0.3.0
    // migration removes the mode key; the alpha key is retired by the
    // schema-v4 migration below — alphas now derive from per-side ms time
    // constants via the Thetis α = exp(-1/(fps×τ)) formula).
    const int avgRaw = readInt(QStringLiteral("DisplayAverageMode"),
                               static_cast<int>(AverageMode::Logarithmic));
    m_averageMode = static_cast<AverageMode>(qBound(0, avgRaw,
                          static_cast<int>(AverageMode::Count) - 1));

    // Per-side averaging time constants. Defaults match Thetis (setup.cs
    // udDisplayAVGTime = 30 ms, udDisplayAVTimeWF = 120 ms). Range matches
    // Thetis: 1..9999 ms, but we clamp at 10 ms to keep the UI step sane.
    m_spectrumAverageTimeMs = qBound(10,
        readInt(QStringLiteral("DisplaySpectrumAverageTimeMs"), 30), 9999);
    m_waterfallAverageTimeMs = qBound(10,
        readInt(QStringLiteral("DisplayWaterfallAverageTimeMs"), 120), 9999);
    recomputeAverageAlphas();

    // Task 2.1: Detector + Averaging split. New keys alongside legacy.
    // Ported from Thetis specHPSDR.cs:302-415 [v2.10.3.13].
    // RX1 scope dropped — pan-agnostic naming per design Section 1B.
    {
        const int detRaw = readInt(QStringLiteral("DisplaySpectrumDetector"),
                                   static_cast<int>(SpectrumDetector::Peak));
        m_spectrumDetector = static_cast<SpectrumDetector>(
            qBound(0, detRaw, static_cast<int>(SpectrumDetector::Count) - 1));

        const int avgNewRaw = readInt(QStringLiteral("DisplaySpectrumAveraging"),
                                      static_cast<int>(SpectrumAveraging::LogRecursive));
        m_spectrumAveraging = static_cast<SpectrumAveraging>(
            qBound(0, avgNewRaw, static_cast<int>(SpectrumAveraging::Count) - 1));

        const int wfDetRaw = readInt(QStringLiteral("DisplayWaterfallDetector"),
                                     static_cast<int>(SpectrumDetector::Peak));
        m_waterfallDetector = static_cast<SpectrumDetector>(
            qBound(0, wfDetRaw, static_cast<int>(SpectrumDetector::Count) - 1));

        const int wfAvgNewRaw = readInt(QStringLiteral("DisplayWaterfallAveraging"),
                                        static_cast<int>(SpectrumAveraging::None));
        m_waterfallAveraging = static_cast<SpectrumAveraging>(
            qBound(0, wfAvgNewRaw, static_cast<int>(SpectrumAveraging::Count) - 1));
    }
    // DisplayPeakHoldDelayMs is a retired key (v0.3.0 migration removes it).
    // The legacy static peak hold uses DisplayPeakHoldResetMs (renamed to avoid
    // collision with the retired key and with DisplayActivePeakHoldDurationMs
    // which belongs to the new ActivePeakHold system introduced in Task 2.5).
    m_peakHoldDelayMs  = readInt(QStringLiteral("DisplayPeakHoldResetMs"), 2000);
    m_lineWidth        = readFloat(QStringLiteral("DisplayLineWidth"), 1.5f);
    m_dbmCalOffset     = readFloat(QStringLiteral("DisplayCalOffset"), 0.0f);
    const bool peakOn = readBool(QStringLiteral("DisplayPeakHoldEnabled"), false);
    const bool gradOn = readBool(QStringLiteral("DisplayGradientEnabled"), false);
    m_gradientEnabled = gradOn;
    // Delay the peak hold enable path until the timer infra is ready.
    if (peakOn) {
        setPeakHoldEnabled(true);
    }

    // Tasks 2.5 / 2.6 — Active Peak Hold + Peak Blobs persisted state.
    // SpectrumPeaksPage owns the UI but the renderer needs the persisted
    // values at app-start time. The keys are global (one set for every
    // pan), so the same load also serves every pan's live reload.
    loadSpectrumPeaksSettings();

    // Phase 3G-8 commit 4: waterfall renderer state.
    m_wfAgcEnabled = readBool(QStringLiteral("DisplayWfAgc"), true);
    // Task 2.8: NF-AGC settings (DisplayWfReverseScroll key intentionally not
    // read here — W5 removed; key migration handled in Task 5.1).
    m_wfNfAgcEnabled = readBool(QStringLiteral("WaterfallNFAGCEnabled"), false);
    m_wfNfAgcOffsetDb = readInt(QStringLiteral("WaterfallAGCOffsetDb"), 0);
    m_wfStopOnTx = readBool(QStringLiteral("WaterfallStopOnTx"), false);
    m_wfOpacity          = readInt(QStringLiteral("DisplayWfOpacity"), 100);
    m_wfUpdatePeriodMs   = readInt(QStringLiteral("DisplayWfUpdatePeriodMs"), 30);

    // 3M-5b: TX waterfall colormap settings.
    // From Thetis Display.cs:1911-1937 [v2.10.3.13+501e3f51] — TXWFAmpMin / TXWFAmpMax defaults.
    m_txWfLowLevel  = readInt(QStringLiteral("DisplayTxWfLowLevel"),  -70);
    m_txWfHighLevel = readInt(QStringLiteral("DisplayTxWfHighLevel"),  30);
    // From Thetis setup.cs:33314-33322 [v2.10.3.13+501e3f51] — comboColorPalette_tx default.
    m_txWfPalette = static_cast<WfColorScheme>(qBound(0,
        s.value(settingsKey(QStringLiteral("DisplayTxWfPalette"), m_panIndex),
                QString::number(static_cast<int>(WfColorScheme::Enhanced))).toInt(),
        static_cast<int>(WfColorScheme::Count) - 1));
    // From Thetis Display.cs:2516-2521 [v2.10.3.13+501e3f51] — waterfall_low_color_tx default Black.
    {
        const QString hex = s.value(
            settingsKey(QStringLiteral("DisplayTxWfLowColor"), m_panIndex),
            QStringLiteral("#FF000000")).toString();
        QColor c = QColor::fromString(hex);
        m_txWfLowColor = c.isValid() ? c : QColor(Qt::black);
    }
    // 3M-5c: Custom gradient encoded text (default empty -> picker reverts
    // to Thetis-verbatim 8-stop grayscale on first show; the cached
    // m_txCustomLut stays invalid until the user picks Custom palette and
    // mutates the gradient at least once).
    m_txWfGradient = s.value(settingsKey(QStringLiteral("DisplayTxWfGradient"), m_panIndex),
                             QString()).toString();
    m_txCustomLutValid = false;
    if (!m_txWfGradient.isEmpty()) {
        // Rebuild cached LUT from persisted encoded text. Same Thetis
        // setup.cs:33314-33322 [v2.10.3.13+501e3f51] consumer pattern as
        // setTxWfGradient() but invoked at load time so the very first
        // MOX paint after launch already has the cached 101-color LUT.
        GradientPickerWidget tempPicker;
        tempPicker.setEncodedText(m_txWfGradient);
        const QVector<QColor> lut = tempPicker.colorTable(101);
        if (lut.size() == 101) {
            for (int k = 0; k < 101; ++k) {
                m_txCustomLut[k] = lut[k].rgba();
            }
            m_txCustomLutValid = true;
        }
    }

    // Sub-epic E: scrollback depth (default 20 min, range 60s..20min).
    m_waterfallHistoryMs = s.value(
        settingsKey(QStringLiteral("DisplayWaterfallHistoryMs"), m_panIndex),
        QString::number(static_cast<qint64>(kDefaultWaterfallHistoryMs))
    ).toLongLong();
    m_wfUseSpectrumMinMax = readBool(QStringLiteral("DisplayWfUseSpectrumMinMax"), false);
    const int wfAvgRaw = readInt(QStringLiteral("DisplayWfAverageMode"),
                                 static_cast<int>(AverageMode::None));
    m_wfAverageMode = static_cast<AverageMode>(qBound(0, wfAvgRaw,
                          static_cast<int>(AverageMode::Count) - 1));
    const int tsPosRaw = readInt(QStringLiteral("DisplayWfTimestampPos"),
                                 static_cast<int>(TimestampPosition::None));
    m_wfTimestampPos = static_cast<TimestampPosition>(qBound(0, tsPosRaw,
                           static_cast<int>(TimestampPosition::Count) - 1));
    const int tsModeRaw = readInt(QStringLiteral("DisplayWfTimestampMode"),
                                  static_cast<int>(TimestampMode::UTC));
    m_wfTimestampMode = static_cast<TimestampMode>(qBound(0, tsModeRaw,
                            static_cast<int>(TimestampMode::Count) - 1));
    m_showRxFilterOnWaterfall = readBool(QStringLiteral("DisplayShowRxFilterOnWaterfall"), false);
    // Default True — same rationale as DisplayDrawTxFilter above: the TX
    // overlay should be visible during MOX out of the box.  The waterfall
    // column is independently MOX-gated at the call site.
    m_showTxFilterOnRxWaterfall = readBool(QStringLiteral("DisplayShowTxFilterOnRxWaterfall"), true);
    // Plan 4 D9 (Cluster E): persist DrawTXFilter flag.
    // From Thetis display.cs:2481 [v2.10.3.13]: DrawTXFilter property.
    // Until the Setup → Display TX Display page has a wired checkbox,
    // the user can set this in AppSettings XML directly.
    // Default True: pairs with the MOX-gated TX overlay paint at the call
    // sites (m_txFilterVisible && m_moxOverlay).  Without this, MOX flips
    // m_moxOverlay true, the RX cyan correctly hides, but the TX orange
    // never paints — the panadapter goes "clear" during TX/TUNE.
    m_txFilterVisible = readBool(QStringLiteral("DisplayDrawTxFilter"), true);
    m_showRxZeroLineOnWaterfall = readBool(QStringLiteral("DisplayShowRxZeroLine"), false);
    m_showTxZeroLineOnWaterfall = readBool(QStringLiteral("DisplayShowTxZeroLine"), false);

    // Phase 3G-8 commit 5: grid / scales state.
    m_gridEnabled = readBool(QStringLiteral("DisplayGridEnabled"), true);
    m_showZeroLine = readBool(QStringLiteral("DisplayShowZeroLine"), false);
    m_showFps = readBool(QStringLiteral("DisplayShowFps"), false);
    // B8 Task 21: cursor frequency readout persists across restarts.
    m_showCursorFreq = readBool(QStringLiteral("DisplayShowCursorFreq"), true);
    m_dbmScaleVisible = readBool(QStringLiteral("DisplayDbmScaleVisible"), true);

    // Transmit grid. Bounded on load so a value that cannot have come from a
    // deliberate drag heals to the Thetis seed instead of persisting: an
    // unusable transmit grid is self-trapping, because the strip the
    // operator would drag to fix it is the thing that stopped drawing.
    //
    // Per pan, through rawValue, like every other key in this function. These
    // three shipped as bare global keys, which every pan then read AND wrote
    // while holding its own member: a pan that never transmitted would carry
    // the value it loaded at startup and put it back on its next save,
    // silently reverting a transmit-grid drag made on the pan that did.
    {
        bool okRef = false, okRange = false;
        const float ref =
            rawValue(QStringLiteral("DisplayTxGridRefLevel")).toFloat(&okRef);
        const float rng =
            rawValue(QStringLiteral("DisplayTxGridDynamicRange")).toFloat(&okRange);
        // Bounds are the DRAG's bounds, not a second opinion about them.
        //
        // They were [-160, +20] while the drag handler clamped to Thetis's
        // [-200, +200]. The +20 half was the worse end: it is exactly the
        // transmit grid's default reference level, so an operator who dragged
        // the pinned scale UP to fix it saved a value the loader then threw
        // away, and the next launch came back to the same pinned grid with no
        // sign anything had happened. Found by Codex on PR #317.
        if (okRef && ref >= -200.0f && ref <= 200.0f)  { m_txRefLevel     = ref; }
        if (okRange && rng >= 10.0f && rng <= 200.0f)  { m_txDynamicRange = rng; }

        bool okBw = false;
        const double bw =
            rawValue(QStringLiteral("DisplayTxViewBandwidth")).toDouble(&okBw);
        // Wide enough to see, narrow enough to be a transmit view.
        if (okBw && bw >= 1000.0 && bw <= 500000.0) { m_txViewBandwidthHz = bw; }
    }
    // The receive store mirrors whatever the live pair loaded above, since
    // the widget always comes up in receive.
    m_rxRefLevel     = m_refLevel;
    m_rxDynamicRange = m_dynamicRange;
    m_bandPlanFontSize = s.value(QStringLiteral("BandPlanFontSize"),
                                 QStringLiteral("6")).toInt();
    const int alignRaw = readInt(QStringLiteral("DisplayFreqLabelAlign"),
                                 static_cast<int>(FreqLabelAlign::Center));
    m_freqLabelAlign = static_cast<FreqLabelAlign>(qBound(0, alignRaw,
                           static_cast<int>(FreqLabelAlign::Count) - 1));

    auto readColor = [&](const QString& key, const QColor& def) -> QColor {
        const QString hex = rawValue(key);
        if (hex.isEmpty()) { return def; }
        QColor c = QColor::fromString(hex);
        return c.isValid() ? c : def;
    };
    m_fillColor = readColor(QStringLiteral("DisplayFillColor"), m_fillColor);
    m_gridColor     = readColor(QStringLiteral("DisplayGridColor"), m_gridColor);
    m_gridFineColor = readColor(QStringLiteral("DisplayGridFineColor"), m_gridFineColor);
    m_hGridColor    = readColor(QStringLiteral("DisplayHGridColor"), m_hGridColor);
    m_gridTextColor = readColor(QStringLiteral("DisplayGridTextColor"), m_gridTextColor);
    // Plan 4 D9c-1: old single key "DisplayZeroLineColor" dropped (branch not
    // yet on main — no migration burden).  Load the split RX/TX keys.
    m_rxZeroLineColor = readColor(QStringLiteral("DisplayRxZeroLineColor"), m_rxZeroLineColor);
    m_txZeroLineColor = readColor(QStringLiteral("DisplayTxZeroLineColor"), m_txZeroLineColor);
    m_bandEdgeColor = readColor(QStringLiteral("DisplayBandEdgeColor"), m_bandEdgeColor);

    // Plan 4 D9b (Cluster F): TX / RX filter overlay colors.
    m_txFilterColor = readColor(QStringLiteral("DisplayTxFilterColor"), m_txFilterColor);
    m_rxFilterColor = readColor(QStringLiteral("DisplayRxFilterColor"), m_rxFilterColor);
    // Plan 4 D9c-4: TNF + SubRX scaffolding colors.
    m_tnfFilterColor   = readColor(QStringLiteral("DisplayTnfFilterColor"),   m_tnfFilterColor);
    m_subRxFilterColor = readColor(QStringLiteral("DisplaySubRxFilterColor"), m_subRxFilterColor);

    // Task 2.3: spectrum text overlay settings.
    // (DisplayShowMHzOnCursor key retired in 2026-05 — cursor is now
    // always MHz-formatted; visibility is handled by m_showCursorFreq /
    // DisplayShowCursorFreq below.)
    // From Thetis setup.cs:7061 [v2.10.3.13] lblDisplayBinWidth.
    m_showBinWidth = s.value(QStringLiteral("DisplayShowBinWidth"),
                             QStringLiteral("False")).toString() == QStringLiteral("True");
    // From Thetis display.cs:2304 [v2.10.3.13] m_bShowNoiseFloorDBM.
    m_showNoiseFloor = s.value(QStringLiteral("DisplayShowNoiseFloor"),
                               QStringLiteral("False")).toString() == QStringLiteral("True");
    {
        const int nfPos = s.value(QStringLiteral("DisplayShowNoiseFloorPosition"),
                                  QStringLiteral("2")).toInt();
        m_noiseFloorPosition = static_cast<OverlayPosition>(
            qBound(0, nfPos, static_cast<int>(OverlayPosition::BottomRight)));
    }
    // NF render parameters — From Thetis display.cs:2310-2337 + 5763 [v2.10.3.13].
    // Persisted format matches the rest of the codebase: c.name(HexArgb)
    // → "#AARRGGBB" via QColor::fromString round-trip.  The earlier
    // ColorSwatchButton::colorFromHex helper used a "#RRGGBBAA"
    // rearrangement that misinterpreted Thetis-red ("#FFFF0000" =
    // alpha=FF, R=FF, G=00, B=00) as transparent yellow (R=FF, G=FF,
    // B=00, alpha=00) — leaving the line invisible.  Stay on the
    // codebase-standard HexArgb format for consistency.
    auto readNfColor = [&s](const QString& key, const QColor& def) -> QColor {
        const QString hex = s.value(key).toString();
        if (hex.isEmpty()) { return def; }
        const QColor c = QColor::fromString(hex);
        // Reject zero-alpha colours (a previous format mismatch persisted
        // these and they render invisible).  Fallback to the default.
        if (!c.isValid() || c.alpha() == 0) { return def; }
        return c;
    };
    {
        m_noiseFloorColor     = readNfColor(
            QStringLiteral("DisplayNoiseFloorColor"),     m_noiseFloorColor);
        m_noiseFloorTextColor = readNfColor(
            QStringLiteral("DisplayNoiseFloorTextColor"), m_noiseFloorTextColor);
        m_noiseFloorFastColor = readNfColor(
            QStringLiteral("DisplayNoiseFloorFastColor"), m_noiseFloorFastColor);
        m_noiseFloorLineWidth = qBound(1.0f,
            static_cast<float>(s.value(QStringLiteral("DisplayNoiseFloorLineWidth"),
                                       QStringLiteral("1.0")).toFloat()),
            5.0f);
        m_nfShiftDbm = qBound(-12.0f,
            static_cast<float>(s.value(QStringLiteral("DisplayNoiseFloorShiftDb"),
                                       QStringLiteral("0.0")).toFloat()),
            12.0f);
    }
    // From Thetis specHPSDR.cs:325 [v2.10.3.13] NormOneHzPan.
    m_dispNormalize = s.value(QStringLiteral("DisplayDispNormalize"),
                              QStringLiteral("False")).toString() == QStringLiteral("True");
    // From Thetis console.cs:20073 [v2.10.3.13] peak_text_delay=500.
    // Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
    m_showPeakValueOverlay = s.value(QStringLiteral("DisplayShowPeakValueOverlay"),
                                     QStringLiteral("False")).toString() == QStringLiteral("True");
    {
        const int pvPos = s.value(QStringLiteral("DisplayPeakValuePosition"),
                                  QStringLiteral("1")).toInt();
        m_peakValuePosition = static_cast<OverlayPosition>(
            qBound(0, pvPos, static_cast<int>(OverlayPosition::BottomRight)));
    }
    m_peakTextDelayMs = s.value(QStringLiteral("DisplayPeakTextDelayMs"),
                                QStringLiteral("500")).toInt();
    m_peakTextDelayMs = qBound(50, m_peakTextDelayMs, 10000);
    // From Thetis console.cs:20278 [v2.10.3.13] Color.DodgerBlue (#1E90FF).
    {
        const QString pvHex = s.value(QStringLiteral("DisplayPeakValueColor")).toString();
        if (!pvHex.isEmpty()) {
            QColor c = QColor::fromString(pvHex);
            if (c.isValid()) { m_peakValueColor = c; }
        }
    }
    // If ShowPeakValueOverlay was persisted as on, restart the timer now.
    if (m_showPeakValueOverlay) {
        setShowPeakValueOverlay(false); // ensure clean start
        setShowPeakValueOverlay(true);
    }

    // Task 2.9: NF-aware grid settings.
    // From Thetis setup.cs:24202-24213 [v2.10.3.13] chkAdjustGridMinToNFRX1.
    // RX1 scope dropped; NereusSDR applies as global panadapter default.
    m_adjustGridMinToNF = s.value(QStringLiteral("DisplayAdjustGridMinToNoiseFloor"),
                                  QStringLiteral("False")).toString() == QStringLiteral("True");
    m_nfOffsetGridFollow = s.value(QStringLiteral("DisplayNFOffsetGridFollow"),
                                   QStringLiteral("0")).toInt();
    m_nfOffsetGridFollow = qBound(-60, m_nfOffsetGridFollow, 60);
    m_maintainNFAdjustDelta = s.value(QStringLiteral("DisplayMaintainNFAdjustDelta"),
                                      QStringLiteral("False")).toString() == QStringLiteral("True");
    updateGridFollowTimer();

    // 3D Stacked-Trace Spectrum Plan Task 14 (Task 24 added the seventh,
    // Display3DSpeed; Task 23 moved all seven's persistence onto the
    // model, already loaded above). 3D Floor is deliberately NOT here --
    // it is per band on PanadapterModel (see the class-header comment on
    // setDssFloorDepth()/dssFloorDepth() above); the live m_dssFloorDepth
    // mirror keeps whatever value the band-change push last set until the
    // next one arrives.
    {
        // Direct assignment, not setSpectrumRenderMode(mode): that setter's
        // ring-clear-on-leaving-3D (m_dss.clear()/m_dssRowsPushed=0),
        // markOverlayDirty()/update(), and spectrumRenderModeChanged signal
        // all assume a LIVE transition an observer might care about.
        // loadSettings() runs once at construction, before this widget has
        // painted a frame or wired any observer to that signal, so skipping
        // them here is inert today. It will stop being inert once Phase 3F
        // (multi-panadapter) can call loadSettings() again on an
        // already-live widget to re-home it onto a different pan's
        // persisted config -- migrating a 3D pan's widget onto a 2D pan's
        // settings would then leave a stale ring behind and never notify
        // whatever had been listening for the mode change. Route through
        // setSpectrumRenderMode() once that reuse path exists. Task 23:
        // the value now comes from the model (loaded, signal-blocked,
        // above), not a fresh readInt() -- the model's own
        // setSpectrumRenderMode() clamps to the identical Mode2D/Mode3D
        // range.
        m_spectrumRenderMode =
            static_cast<SpectrumRenderMode>(m_displaySettings->spectrumRenderMode());
    }
    // Task 23: were qBound(0, readInt(...), 100) / qBound(0, readInt(...), 10)
    // -- the model's own setters clamp each field to the identical range.
    m_dssGain          = m_displaySettings->dssGain();
    m_dssRowSpan       = m_displaySettings->dssRowSpan();
    m_dssAngle         = m_displaySettings->dssAngle();
    m_dssRowDivider    = m_displaySettings->dssRowDivider();
    m_threeDSliceDepth = m_displaySettings->threeDSliceDepth();

    recomputeExtendedMode();
}

void SpectrumWidget::saveSettings()
{
    auto& s = AppSettings::instance();

    auto writeFloat = [&](const QString& key, float val) {
        s.setValue(settingsKey(key, m_panIndex), QString::number(static_cast<double>(val)));
    };
    auto writeInt = [&](const QString& key, int val) {
        s.setValue(settingsKey(key, m_panIndex), QString::number(val));
    };

    // 3D Stacked-Trace Spectrum Plan Task 23: DisplaySettingsModel is now
    // the persister for its own fourteen fields (Colour Scheme,
    // Colour Gain, Black Level, Ref Level, Dyn Range, Fill Alpha, Fill
    // trace, Spectrum mode, 3D Gain, 3D Span, 3D Angle, 3D Speed, 3D
    // Slice Shadow, split fraction). The receive snapshot below preserves
    // the native keyed-save protection. syncDisplaySettingsFromWidget()
    // pushes this widget's current values into the model one more time
    // first (every write site already pushes on change, so this is
    // normally a no-op; not relied upon to be, since a caller could in
    // principle reach saveSettings() directly), then the model's own
    // save() writes its fourteen keys under this widget's panIndex(). 3D
    // Floor is excluded from both, exactly as before Task 23: it
    // persists per band on PanadapterModel (see the class-header comment
    // on setDssFloorDepth()/dssFloorDepth()).
    syncDisplaySettingsFromWidget();

    // The RECEIVE store, not the live pair. While transmitting the live pair
    // IS the transmit grid, so writing it here would put transmit values
    // into the receive keys and the operator's receive scale would be gone
    // after any save that happened to land mid-transmission.
    {
        const float rxRef   = m_moxOverlay ? m_rxRefLevel     : m_refLevel;
        const float rxRange = m_moxOverlay ? m_rxDynamicRange : m_dynamicRange;
        m_displaySettings->save(rxRef, rxRange);
    }
    // The RECEIVE span, for the same reason as the grid above. While keyed
    // m_bandwidthHz is the TRANSMIT span, so a save landing mid-transmission
    // wrote the transmit zoom into the receive key and the next launch came
    // up with the receive pan at a transmit width. The 500 ms debounce makes
    // that easy to hit: any dBm-strip release or setting change while keyed
    // is enough. Found by Codex on PR #317, alongside the grid half of it
    // that this branch had already fixed and this line had been left out of.
    writeFloat(QStringLiteral("DisplayBandwidth"),                        // Phase 3G-12
               static_cast<float>(m_txSpanLoaded ? m_rxViewBandwidthHz
                                                 : m_bandwidthHz));
    writeFloat(QStringLiteral("DisplayWfHighLevel"), m_wfHighThreshold);
    writeFloat(QStringLiteral("DisplayWfLowLevel"), m_wfLowThreshold);
    s.setValue(settingsKey(QStringLiteral("DisplayCtunEnabled"), m_panIndex),
              m_ctunPreference ? QStringLiteral("True") : QStringLiteral("False"));

    // Phase 3G-8 commit 3: spectrum renderer state.
    // DisplayAverageMode + DisplayAverageAlpha are retired keys (v0.3.0
    // and schema-v4 migrations remove them). Canonical save is the Detector +
    // Averaging split keys + the per-side averaging time constants below.
    writeInt(QStringLiteral("DisplaySpectrumAverageTimeMs"), m_spectrumAverageTimeMs);
    writeInt(QStringLiteral("DisplayWaterfallAverageTimeMs"), m_waterfallAverageTimeMs);

    // Task 2.1: Detector + Averaging split keys.
    writeInt(QStringLiteral("DisplaySpectrumDetector"),  static_cast<int>(m_spectrumDetector));
    writeInt(QStringLiteral("DisplaySpectrumAveraging"), static_cast<int>(m_spectrumAveraging));
    writeInt(QStringLiteral("DisplayWaterfallDetector"),  static_cast<int>(m_waterfallDetector));
    writeInt(QStringLiteral("DisplayWaterfallAveraging"), static_cast<int>(m_waterfallAveraging));
    writeInt(QStringLiteral("DisplayPeakHoldResetMs"), m_peakHoldDelayMs); // renamed from retired DisplayPeakHoldDelayMs
    writeFloat(QStringLiteral("DisplayLineWidth"), m_lineWidth);
    writeFloat(QStringLiteral("DisplayCalOffset"), m_dbmCalOffset);
    s.setValue(settingsKey(QStringLiteral("DisplayPeakHoldEnabled"), m_panIndex),
              m_peakHoldEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayGradientEnabled"), m_panIndex),
              m_gradientEnabled ? QStringLiteral("True") : QStringLiteral("False"));

    // Phase 3G-8 commit 4: waterfall renderer state.
    s.setValue(settingsKey(QStringLiteral("DisplayWfAgc"), m_panIndex),
              m_wfAgcEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    // Task 2.8: NF-AGC + Stop-on-TX (DisplayWfReverseScroll intentionally not
    // written; W5 removed — key migration in Task 5.1).
    s.setValue(settingsKey(QStringLiteral("WaterfallNFAGCEnabled"), m_panIndex),
              m_wfNfAgcEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    writeInt(QStringLiteral("WaterfallAGCOffsetDb"), m_wfNfAgcOffsetDb);
    s.setValue(settingsKey(QStringLiteral("WaterfallStopOnTx"), m_panIndex),
              m_wfStopOnTx ? QStringLiteral("True") : QStringLiteral("False"));
    writeInt(QStringLiteral("DisplayWfOpacity"), m_wfOpacity);
    writeInt(QStringLiteral("DisplayWfUpdatePeriodMs"), m_wfUpdatePeriodMs);

    // 3M-5b: TX waterfall colormap settings.
    // From Thetis Display.cs:1911-1937 [v2.10.3.13+501e3f51] — TXWFAmpMin / TXWFAmpMax.
    writeInt(QStringLiteral("DisplayTxWfLowLevel"),  m_txWfLowLevel);
    writeInt(QStringLiteral("DisplayTxWfHighLevel"), m_txWfHighLevel);
    // From Thetis setup.cs:33314-33322 [v2.10.3.13+501e3f51] — comboColorPalette_tx.
    s.setValue(settingsKey(QStringLiteral("DisplayTxWfPalette"), m_panIndex),
               QString::number(static_cast<int>(m_txWfPalette)));
    // From Thetis Display.cs:2516-2521 [v2.10.3.13+501e3f51] — waterfall_low_color_tx.
    s.setValue(settingsKey(QStringLiteral("DisplayTxWfLowColor"), m_panIndex),
               m_txWfLowColor.name(QColor::HexArgb).toUpper());
    // Custom gradient (3M-5c placeholder).
    s.setValue(settingsKey(QStringLiteral("DisplayTxWfGradient"), m_panIndex),
               m_txWfGradient);

    s.setValue(settingsKey(QStringLiteral("DisplayWaterfallHistoryMs"), m_panIndex),
               QString::number(m_waterfallHistoryMs));
    s.setValue(settingsKey(QStringLiteral("DisplayWfUseSpectrumMinMax"), m_panIndex),
              m_wfUseSpectrumMinMax ? QStringLiteral("True") : QStringLiteral("False"));
    writeInt(QStringLiteral("DisplayWfAverageMode"), static_cast<int>(m_wfAverageMode));
    writeInt(QStringLiteral("DisplayWfTimestampPos"), static_cast<int>(m_wfTimestampPos));
    writeInt(QStringLiteral("DisplayWfTimestampMode"), static_cast<int>(m_wfTimestampMode));
    s.setValue(settingsKey(QStringLiteral("DisplayShowRxFilterOnWaterfall"), m_panIndex),
              m_showRxFilterOnWaterfall ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayShowTxFilterOnRxWaterfall"), m_panIndex),
              m_showTxFilterOnRxWaterfall ? QStringLiteral("True") : QStringLiteral("False"));
    // Plan 4 D9 (Cluster E): persist DrawTXFilter flag.
    s.setValue(settingsKey(QStringLiteral("DisplayDrawTxFilter"), m_panIndex),
              m_txFilterVisible ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayShowRxZeroLine"), m_panIndex),
              m_showRxZeroLineOnWaterfall ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayShowTxZeroLine"), m_panIndex),
              m_showTxZeroLineOnWaterfall ? QStringLiteral("True") : QStringLiteral("False"));

    // Phase 3G-8 commit 5: grid / scales state.
    s.setValue(settingsKey(QStringLiteral("DisplayGridEnabled"), m_panIndex),
              m_gridEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayShowZeroLine"), m_panIndex),
              m_showZeroLine ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayShowFps"), m_panIndex),
              m_showFps ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayShowCursorFreq"), m_panIndex),
              m_showCursorFreq ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayDbmScaleVisible"), m_panIndex),
              m_dbmScaleVisible ? QStringLiteral("True") : QStringLiteral("False"));
    // Transmit grid persists independently of receive. While transmitting the
    // LIVE pair is the transmit one, so read the store that is not currently
    // live rather than the member directly.
    //
    // Per pan, matching the load side. A bare key here made every pan a
    // writer of one shared value, so the last pan to save won and the
    // transmitting pan's drag was the thing most likely to be overwritten.
    s.setValue(settingsKey(QStringLiteral("DisplayTxGridRefLevel"), m_panIndex),
               QString::number(m_moxOverlay ? m_refLevel : m_txRefLevel));
    s.setValue(settingsKey(QStringLiteral("DisplayTxGridDynamicRange"), m_panIndex),
               QString::number(m_moxOverlay ? m_dynamicRange : m_txDynamicRange));
    s.setValue(settingsKey(QStringLiteral("DisplayTxViewBandwidth"), m_panIndex),
               QString::number(m_txSpanLoaded ? m_bandwidthHz : m_txViewBandwidthHz));
    s.setValue(QStringLiteral("BandPlanFontSize"),
               QString::number(m_bandPlanFontSize));
    writeInt(QStringLiteral("DisplayFreqLabelAlign"), static_cast<int>(m_freqLabelAlign));

    auto writeColor = [&](const QString& key, const QColor& c) {
        s.setValue(settingsKey(key, m_panIndex), c.name(QColor::HexArgb));
    };
    writeColor(QStringLiteral("DisplayFillColor"), m_fillColor);
    writeColor(QStringLiteral("DisplayGridColor"),     m_gridColor);
    writeColor(QStringLiteral("DisplayGridFineColor"), m_gridFineColor);
    writeColor(QStringLiteral("DisplayHGridColor"),    m_hGridColor);
    writeColor(QStringLiteral("DisplayGridTextColor"), m_gridTextColor);
    // Plan 4 D9c-1: split zero-line color — write RX + TX keys.
    writeColor(QStringLiteral("DisplayRxZeroLineColor"), m_rxZeroLineColor);
    writeColor(QStringLiteral("DisplayTxZeroLineColor"), m_txZeroLineColor);
    writeColor(QStringLiteral("DisplayBandEdgeColor"), m_bandEdgeColor);
    // Plan 4 D9b (Cluster F): TX / RX filter overlay colors.
    writeColor(QStringLiteral("DisplayTxFilterColor"), m_txFilterColor);
    writeColor(QStringLiteral("DisplayRxFilterColor"), m_rxFilterColor);
    // Plan 4 D9c-4: TNF + SubRX scaffolding colors.
    writeColor(QStringLiteral("DisplayTnfFilterColor"),   m_tnfFilterColor);
    writeColor(QStringLiteral("DisplaySubRxFilterColor"), m_subRxFilterColor);

    // Task 2.3: spectrum text overlay keys.
    // (DisplayShowMHzOnCursor save retired — cursor format is always MHz now.)
    s.setValue(QStringLiteral("DisplayShowBinWidth"),
               m_showBinWidth ? QStringLiteral("True") : QStringLiteral("False"));
    // From Thetis display.cs:2304 [v2.10.3.13] m_bShowNoiseFloorDBM.
    s.setValue(QStringLiteral("DisplayShowNoiseFloor"),
               m_showNoiseFloor ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("DisplayShowNoiseFloorPosition"),
               QString::number(static_cast<int>(m_noiseFloorPosition)));
    // HexArgb format ("#AARRGGBB") matches the rest of SpectrumWidget's
    // colour persistence (m_bandEdgeColor, m_gridColor, etc.) so the file
    // stays internally consistent.
    s.setValue(QStringLiteral("DisplayNoiseFloorColor"),
               m_noiseFloorColor.name(QColor::HexArgb));
    s.setValue(QStringLiteral("DisplayNoiseFloorTextColor"),
               m_noiseFloorTextColor.name(QColor::HexArgb));
    s.setValue(QStringLiteral("DisplayNoiseFloorFastColor"),
               m_noiseFloorFastColor.name(QColor::HexArgb));
    s.setValue(QStringLiteral("DisplayNoiseFloorLineWidth"),
               QString::number(static_cast<double>(m_noiseFloorLineWidth)));
    s.setValue(QStringLiteral("DisplayNoiseFloorShiftDb"),
               QString::number(static_cast<double>(m_nfShiftDbm)));
    // From Thetis specHPSDR.cs:325 [v2.10.3.13] NormOneHzPan.
    s.setValue(QStringLiteral("DisplayDispNormalize"),
               m_dispNormalize ? QStringLiteral("True") : QStringLiteral("False"));
    // From Thetis console.cs:20073 [v2.10.3.13] peak_text_delay=500.
    // Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
    s.setValue(QStringLiteral("DisplayShowPeakValueOverlay"),
               m_showPeakValueOverlay ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("DisplayPeakValuePosition"),
               QString::number(static_cast<int>(m_peakValuePosition)));
    s.setValue(QStringLiteral("DisplayPeakTextDelayMs"),
               QString::number(m_peakTextDelayMs));
    // From Thetis console.cs:20278 [v2.10.3.13] Color.DodgerBlue.
    s.setValue(QStringLiteral("DisplayPeakValueColor"),
               m_peakValueColor.name(QColor::HexArgb));

    // Task 2.9: NF-aware grid settings.
    // From Thetis setup.cs:24202-24213 [v2.10.3.13] chkAdjustGridMinToNFRX1.
    // RX1 scope dropped; NereusSDR applies as global panadapter default.
    // Level Cal: the user's value while the level calibration holds the
    // follow off (setGridFollowSaveHold).
    const bool followToSave = gridFollowSaveHold().value_or(m_adjustGridMinToNF);
    s.setValue(QStringLiteral("DisplayAdjustGridMinToNoiseFloor"),
               followToSave ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("DisplayNFOffsetGridFollow"),
               QString::number(m_nfOffsetGridFollow));
    s.setValue(QStringLiteral("DisplayMaintainNFAdjustDelta"),
               m_maintainNFAdjustDelta ? QStringLiteral("True") : QStringLiteral("False"));

    // 3D Stacked-Trace Spectrum Plan Task 14 (Task 24 added the seventh,
    // Display3DSpeed): six of the seven 3D controls -- Spectrum render
    // mode, 3D Gain, 3D Span, 3D Angle, 3D Speed and 3D Slice Shadow --
    // are six of the model's fourteen, already written above by
    // m_displaySettings->save(). 3D Floor is the seventh and the
    // exception: it is deliberately excluded from both, persisting per
    // band on PanadapterModel instead (see the class-header comment on
    // setDssFloorDepth()/dssFloorDepth()), so no Display3DFloorDepth key
    // is written from here.
}

void SpectrumWidget::scheduleSettingsSave()
{
    if (m_settingsSaveScheduled) {
        return;
    }
    m_settingsSaveScheduled = true;
    QTimer::singleShot(500, this, [this]() {
        m_settingsSaveScheduled = false;
        saveSettings();
        AppSettings::instance().save();
    });
}

void SpectrumWidget::updateSpectrumFromTxPixels(int receiverId,
                                                const QVector<float>& binsDbm)
{
    Q_UNUSED(receiverId);
    if (binsDbm.isEmpty()) { return; }

    const int displayWidth = qMax(width() - effectiveStripW(), 800);

    // Same window slice the trace path uses, so the transmit trace and the
    // transmit waterfall agree about the X axis.
    auto [firstBin, lastBin] = visibleBinRange(binsDbm.size());
    const int sliceCount = lastBin - firstBin + 1;
    if (sliceCount <= 0) { return; }

    if (m_renderedPixels.size() != displayWidth) {
        m_renderedPixels.resize(displayWidth);
    }

    // Straight resample, deliberately NOT a detector. WDSP already reduced
    // bins to pixels using the TX Display detector and averaging; running a
    // second detector here is the defect this method exists to remove. With
    // the analyzer clipped to the display window, n_pix and displayWidth are
    // within a few percent of each other, so this is close to 1:1 and there
    // is nothing left to decimate.
    const double step = static_cast<double>(sliceCount)
                      / static_cast<double>(displayWidth);
    for (int x = 0; x < displayWidth; ++x) {
        const double src = static_cast<double>(firstBin) + step * x;
        const int    i0  = static_cast<int>(src);
        const int    i1  = qMin(i0 + 1, lastBin);
        const double f   = src - static_cast<double>(i0);
        const float  a   = binsDbm[qBound(firstBin, i0, lastBin)];
        const float  b   = binsDbm[qBound(firstBin, i1, lastBin)];
        m_renderedPixels[x] = static_cast<float>(a + (b - a) * f);
    }

    m_hasNewSpectrum = true;
    emit spectrumFrameRendered();
    update();
}

void SpectrumWidget::invalidateRemoteSpectrumFrame()
{
    traceRxHistoryEvent(RxHistoryEvent::FrameRenewal);
    m_remoteSpectrum = true;
    recomputeExtendedMode();
    m_remoteCodec = {};
    m_pxPeakHold.clear();
    // A new context is a reset, as Thetis's centre-frequency change
    // (display.cs:907-921 [v2.10.3.15]: ResetBlobMaximums(1, true);
    // ResetSpectrumPeaks(1);): both empty and wait out the 500 ms display
    // delay before they show again.
    m_activePeakHold.resize(0);
    m_peakBlobs.clearMaximums();
    m_renderedPixels.clear();
    m_undentedPixels.clear();
    m_wfRenderedPixels.clear();
    m_pendingWfPixelsDbm.clear();
    m_pendingWfPixelsDbmDirty = false;
    m_pendingRemoteWide.clear();
    m_lastFullBinsDbm.clear();
    m_hasNewSpectrum = true;
    refreshRemoteTraceProjection();
}

void SpectrumWidget::traceRxHistoryEvent(RxHistoryEvent event, int sourceStream,
                                          quint64 sourceEpoch, QSize nextImageSize,
                                          double oldCentreHz, double oldSpanHz,
                                          double newCentreHz, double newSpanHz) const
{
    if (qEnvironmentVariableIntValue("NEREUS_TRACE_RX_HISTORY") != 1) { return; }
    if (event == RxHistoryEvent::RfReproject
        && oldCentreHz == newCentreHz && oldSpanHz == newSpanHz) { return; }
    if (event == RxHistoryEvent::ViewportRebuild) {
        // Scrollback can rebuild on every frame. Keep its diagnostic sampled;
        // destructive resets and binding/resize changes are always logged.
        constexpr int kViewportTraceIntervalMs = 250;
        if (m_historyTraceViewportClock.isValid()
            && m_historyTraceViewportClock.elapsed() < kViewportTraceIntervalMs) { return; }
        m_historyTraceViewportClock.start();
    }
    const auto numeric = [](double value) { return QString::number(value, 'g', 17); };
    const bool rfChange = oldSpanHz > 0 && newSpanHz > 0;
    const bool captured = m_dss.rowCount() > 0;
    qCInfo(lcSpectrum).nospace().noquote()
        << "RX_HISTORY reason=" << static_cast<int>(event)
        << " pan=" << m_panIndex << " source=" << sourceStream << " epoch=" << sourceEpoch
        << " dssRows=" << m_dss.rowCount() << " pushed=" << m_dssRowsPushed
        << " rowGeneration=" << m_dss.rowGeneration()
        << " historyRows=" << m_wfHistoryRowCount << " historyOffset=" << m_wfHistoryOffsetRows
        << " cursor=" << m_wfWriteRow << " mode=" << static_cast<int>(m_spectrumRenderMode)
        << " widgetW=" << width() << " widgetH=" << height()
        << " liveW=" << m_waterfall.width() << " liveH=" << m_waterfall.height()
        << " historyW=" << m_waterfallHistory.width() << " historyH=" << m_waterfallHistory.height()
        << " nextW=" << nextImageSize.width() << " nextH=" << nextImageSize.height()
        << " oldCentre=" << numeric(rfChange ? oldCentreHz : m_centerHz)
        << " oldSpan=" << numeric(rfChange ? oldSpanHz : m_bandwidthHz)
        << " newCentre=" << numeric(rfChange ? newCentreHz : m_centerHz)
        << " newSpan=" << numeric(rfChange ? newSpanHz : m_bandwidthHz)
        << " capturedCentre=" << numeric(captured ? m_dss.rowCenterMhzAtAge(0) * 1.0e6 : 0.0)
        << " capturedSpan=" << numeric(captured ? m_dss.rowBandwidthMhzAtAge(0) * 1.0e6 : 0.0);
}

void SpectrumWidget::clearRemoteSpectrum()
{
    traceRxHistoryEvent(RxHistoryEvent::RemoteFullClear);
    invalidateRemoteSpectrumFrame();
    m_remoteWidebandAvailable = false;
    m_remoteWidebandActive = false;
    m_remoteWidebandAdcRateHz = 0.0;
    m_remoteFftSize = 0;
    m_remoteFps = 0;
    recomputeExtendedMode();
    m_dss.clear();
    m_dssFoldCount = 0;
    m_dssFoldRow.clear();
    m_dssFoldFullBins.clear();
    m_pendingRemoteCapture = {};
    m_dssRowsPushed = 0;
    m_dssScrollProgressRows = 0.0f;
    // R-R3-21: rows not yet drawn go with the history. A renewal
    // (invalidateRemoteSpectrumFrame) keeps them: each carries the RF window
    // it was captured at. So does the last trace.
    m_remoteRowQueue.clear();
    m_remoteTraceCaptured.clear();
    m_renderedPixels.clear();
    m_undentedPixels.clear();
    clearWaterfallHistory();
}

void SpectrumWidget::setRemoteSpectrumContext(const SpectrumEndpointContext& context,
                                               double sourceCentreHz, double sampleRateHz,
                                               int grantedFftSize)
{
    invalidateRemoteSpectrumFrame();
    const double binWidthBefore = binWidthHz();
    // NereusSDR-original remote wiring: a new codec generation retires live
    // planes, not already-painted history. Core's accepted crop can differ
    // from the requested view by FFT-bin alignment; use the existing local
    // reprojection to keep the 2D rows at their original RF frequencies.
    // 3D rows already retain their own centre/span in DssRenderer.
    if (!qFuzzyCompare(m_centerHz, context.exactCentreHz)
        || !qFuzzyCompare(m_bandwidthHz, context.exactSpanHz)) {
        reprojectWaterfall(m_centerHz, m_bandwidthHz,
                           context.exactCentreHz, context.exactSpanHz);
    }
    m_remoteCodec = context.codec;
    m_remoteCapture = {context, sourceCentreHz, sampleRateHz};
    m_remoteExactCentreHz = context.exactCentreHz;
    m_remoteExactSpanHz = context.exactSpanHz;
    m_remoteWidebandAvailable = context.wideband.available;
    m_remoteWidebandActive = context.wideband.active;
    m_remoteWidebandAdcRateHz = context.wideband.available
        ? context.wideband.adcRateHz : 0.0;
    m_remoteWideCentreHz = context.wideCentreHz;
    m_remoteWideSpanHz = context.wideSpanHz;
    // Parity Task 17: what the Core runs for this pan, not this window's
    // own (idle) engine or display timer.
    m_remoteFftSize = grantedFftSize > 0 ? grantedFftSize : 0;
    m_remoteFps = context.targetFps > 0 ? context.targetFps : 0;
    setDdcCenterFrequency(sourceCentreHz);
    setSampleRate(sampleRateHz);
    setDisplayWindowPreservingHistory(context.exactCentreHz, context.exactSpanHz);
    recomputeExtendedMode();
    if (!qFuzzyCompare(1.0 + binWidthHz(), 1.0 + binWidthBefore)) {
        // The normalise shift, the dBm scale and the Hz/bin readout move
        // with the bin width.
        markOverlayDirty();
        emit remoteSpectrumGrantChanged();
    }
}

void SpectrumWidget::onWaterfallTick()
{
    // Push on tick, EXCEPT while the TX analyzer owns the waterfall.
    //
    // "Always" was right when this was the only writer. During transmit
    // it is not: pushTxWaterfallRow feeds rows straight in at the
    // analyzer's own frame rate, and this ticker went on re-pushing the
    // last cached RX row alongside them. Two writers at two cadences
    // interleave, which paints the waterfall in horizontal bands of
    // transmit and stale receive. Bench 2026-08-05, and it is the
    // scanline artifact in that report.
    //
    // The cache write is already gated in updateSpectrumLinear, so the
    // contents here are frozen RX data during transmit -- there is
    // nothing worth drawing even if the cadence did line up.
    if (m_txExternalWaterfall) {
        // R-R3-21: receive rows queued meanwhile are superseded by the
        // transmit rows, as the one pending row was; none plays out late.
        m_remoteRowQueue.clear();
        return;
    }
    // Core controls remote waterfall cadence. Replaying a cached row
    // would invent new waterfall time during loss or a paused session;
    // R-R3-21: a remote pan draws only the rows its queue holds.
    if (m_remoteSpectrum) {
        drainRemoteWaterfallRows();
        return;
    }
    m_pendingWfPixelsDbmDirty = false;
    if (!m_pendingWfPixelsDbm.isEmpty()) {
        pushWaterfallRow(m_pendingWfPixelsDbm);
    }
}

bool SpectrumWidget::updateRemoteSpectrum(const DisplayCodecFrame& frame)
{
    return updateRemoteSpectrum(frame, m_remoteCapture);
}

bool SpectrumWidget::updateRemoteSpectrum(const DisplayCodecFrame& frame,
                                          const RemoteSpectrumCapture& capture)
{
    if (!(capture == m_remoteCapture)) { return false; }
    const auto& context = frame.context;
    if (!m_remoteSpectrum || m_remoteCodec.traceSamples == 0
        || !qFuzzyCompare(m_centerHz, m_remoteExactCentreHz)
        || !qFuzzyCompare(m_bandwidthHz, m_remoteExactSpanHz)
        || context.endpointId != m_remoteCodec.endpointId
        || context.contextGeneration != m_remoteCodec.contextGeneration
        || context.traceSamples != m_remoteCodec.traceSamples
        || context.waterfallSamples != m_remoteCodec.waterfallSamples
        || context.wideSamples != m_remoteCodec.wideSamples
        || context.minDbm != m_remoteCodec.minDbm || context.maxDbm != m_remoteCodec.maxDbm) {
        return false;
    }
    const auto valid = [](const QVector<float>& plane, int count) {
        return plane.size() == count
            && std::all_of(plane.cbegin(), plane.cend(), [](float value) {
                return std::isfinite(value);
            });
    };
    if (!valid(frame.traceDbm, context.traceSamples)
        || !valid(frame.waterfallDbm, context.waterfallSamples)
        || !valid(frame.wideDbm, context.wideSamples)) {
        return false;
    }
    m_renderedPixels = frame.traceDbm;
    m_wfRenderedPixels = frame.waterfallDbm;
    // R-R3-21: kept with its window, so a tune reprojects it, not blanks it.
    m_remoteTraceCaptured = frame.traceDbm;
    m_remoteTraceCentreHz = m_remoteExactCentreHz;
    m_remoteTraceSpanHz = m_remoteExactSpanHz;
    if (visualNotchWillDent()) {
        m_undentedPixels = m_renderedPixels;
        applyVisualNotchDent(m_renderedPixels);
        applyVisualNotchDent(m_wfRenderedPixels);
    } else {
        m_undentedPixels.clear();
    }
    updateReducedSpectrumOverlays();
    if (frame.waterfallAdvance) {
        // R-R3-21: queued, not one overwritten slot, so every row the Core
        // sent is drawn in its own place.
        m_remoteRowQueue.append({m_wfRenderedPixels, frame.wideDbm,
                                 m_remoteExactCentreHz, m_remoteExactSpanHz, capture});
        while (m_remoteRowQueue.size() > kMaxRemoteRowQueue) {
            m_remoteRowQueue.removeFirst();
            ++m_remoteRowsDropped;
        }
    }
    m_hasNewSpectrum = true;
    emit spectrumFrameRendered();
    return true;
}

bool SpectrumWidget::enqueueRemoteWaterfallRow(const QVector<float>& pixelsDbm,
                                               const QVector<float>& wideDbm,
                                               double centreHz, double spanHz)
{
    SpectrumEndpointContext context = m_remoteCapture.context();
    context.exactCentreHz = centreHz;
    context.exactSpanHz = spanHz;
    return enqueueRemoteWaterfallRow(pixelsDbm, wideDbm,
        RemoteSpectrumCapture{context, m_remoteCapture.sourceCentreHz,
                              m_remoteCapture.sourceSampleRateHz});
}

bool SpectrumWidget::enqueueRemoteWaterfallRow(const QVector<float>& pixelsDbm,
                                               const QVector<float>& wideDbm,
                                               const RemoteSpectrumCapture& capture)
{
    const double centreHz = capture.context().exactCentreHz;
    const double spanHz = capture.context().exactSpanHz;
    const auto finite = [](const QVector<float>& plane) {
        return std::all_of(plane.cbegin(), plane.cend(),
                           [](float value) { return std::isfinite(value); });
    };
    if (!m_remoteSpectrum || pixelsDbm.isEmpty() || spanHz <= 0.0
        || !finite(pixelsDbm) || !finite(wideDbm)) {
        return false;
    }
    QVector<float> pixels = pixelsDbm;
    if (visualNotchWillDent() && qFuzzyCompare(centreHz, m_centerHz)
        && qFuzzyCompare(spanHz, m_bandwidthHz)) {
        applyVisualNotchDent(pixels);
    }
    m_remoteRowQueue.append({std::move(pixels), wideDbm, centreHz, spanHz, capture});
    while (m_remoteRowQueue.size() > kMaxRemoteRowQueue) {
        m_remoteRowQueue.removeFirst();
        ++m_remoteRowsDropped;
    }
    return true;
}

QVector<float> SpectrumWidget::reprojectedToView(const QVector<float>& pixelsDbm,
                                                 double centreHz, double spanHz) const
{
    // R-R3-21: pixels captured across centreHz +- spanHz / 2, placed on the
    // current axis; where the capture did not reach, its own floor.
    const int n = pixelsDbm.size();
    if (n == 0 || spanHz <= 0.0 || m_bandwidthHz <= 0.0) {
        return pixelsDbm;
    }
    const float floorDbm = *std::min_element(pixelsDbm.cbegin(), pixelsDbm.cend());
    const double oldStartHz = centreHz - spanHz / 2.0;
    const double newStartHz = m_centerHz - m_bandwidthHz / 2.0;
    QVector<float> moved(n, floorDbm);
    for (int x = 0; x < n; ++x) {
        const double hz = newStartHz + (x + 0.5) * m_bandwidthHz / n;
        const double source = (hz - oldStartHz) / spanHz * n;
        if (source >= 0.0 && source < n) {
            moved[x] = pixelsDbm.at(int(source));
        }
    }
    return moved;
}

bool SpectrumWidget::presentRemoteTraceCaptured(const QVector<float>& traceDbm,
                                                double centreHz, double spanHz)
{
    if (!m_remoteSpectrum || showsTransmitView() || traceDbm.isEmpty() || spanHz <= 0.0
        || !std::all_of(traceDbm.cbegin(), traceDbm.cend(),
                        [](float value) { return std::isfinite(value); })) {
        return false;
    }
    m_remoteTraceCaptured = traceDbm;
    m_remoteTraceCentreHz = centreHz;
    m_remoteTraceSpanHz = spanHz;
    refreshRemoteTraceProjection();
    return true;
}

void SpectrumWidget::refreshRemoteTraceProjection()
{
    // R-R3-21: a tune, zoom or pan drag never blanks the trace. The last
    // trace the Core sent is drawn at the frequency it was captured at until
    // a frame of the new window presents, which runs an audio delay behind.
    if (!m_remoteSpectrum || showsTransmitView() || m_remoteTraceCaptured.isEmpty()) {
        return;
    }
    const bool sameWindow = qFuzzyCompare(m_remoteTraceCentreHz, m_centerHz)
        && qFuzzyCompare(m_remoteTraceSpanHz, m_bandwidthHz);
    const bool overlaps = m_remoteTraceCentreHz - m_remoteTraceSpanHz / 2.0
            < m_centerHz + m_bandwidthHz / 2.0
        && m_remoteTraceCentreHz + m_remoteTraceSpanHz / 2.0
            > m_centerHz - m_bandwidthHz / 2.0;
    if (!overlaps) {
        // Nothing the Core sent lies in this window: no trace, not an
        // invented flat line.
        m_renderedPixels.clear();
        m_undentedPixels.clear();
        m_hasNewSpectrum = true;
        update();
        return;
    }
    m_renderedPixels = sameWindow ? m_remoteTraceCaptured
        : reprojectedToView(m_remoteTraceCaptured, m_remoteTraceCentreHz, m_remoteTraceSpanHz);
    if (visualNotchWillDent()) {
        m_undentedPixels = m_renderedPixels;
        applyVisualNotchDent(m_renderedPixels);
    } else {
        m_undentedPixels.clear();
    }
    m_hasNewSpectrum = true;
    update();
}

void SpectrumWidget::drainRemoteWaterfallRows()
{
    // One row a tick keeps the Core's cadence; two while more than two wait
    // plays a late burst out over a few ticks instead of all at once.
    const int rows = m_remoteRowQueue.size() > 2 ? 2 : int(m_remoteRowQueue.size());
    for (int i = 0; i < rows; ++i) {
        RemoteWaterfallRow row = m_remoteRowQueue.takeFirst();
        if (!qFuzzyCompare(row.centreHz, m_centerHz)
            || !qFuzzyCompare(row.spanHz, m_bandwidthHz)) {
            // Captured before a tune or zoom: draw it at the frequency it
            // was captured at, as the painted history was reprojected.
            row.pixelsDbm = reprojectedToView(row.pixelsDbm, row.centreHz, row.spanHz);
            row.wideDbm.clear();
        }
        m_pendingRemoteWide = row.wideDbm;
        m_pendingRemoteCapture = row.capture;
        pushWaterfallRow(row.pixelsDbm);
        m_pendingRemoteWide.clear();
        m_pendingRemoteCapture = {};
        ++m_remoteRowsPushed;
        m_lastRemoteRowPushed = row.pixelsDbm;
    }
}

void SpectrumWidget::setDisplayWindowPreservingHistory(double centerHz,
                                                       double bandwidthHz)
{
    if (bandwidthHz <= 0.0) { return; }
    if (qFuzzyCompare(m_centerHz, centerHz)
        && qFuzzyCompare(m_bandwidthHz, bandwidthHz)) {
        return;
    }
    applyViewWindow(centerHz, bandwidthHz);
    rebuildWaterfallViewport();
    update();
}

// 3D Stacked-Trace Spectrum Plan Task 18: wires this widget's owned
// DisplaySettingsModel bidirectionally. Called once from the
// constructor, after m_displaySettings is created.
//
// Model -> widget: all fifteen model xxxChanged signals drive the
// matching widget applier, so a change made anywhere the model is
// reachable (a future popup/Setup/applet binding) lands on the live
// renderer exactly the way a direct widget call already does.
//
// Widget -> model: the seven 3D fields (six from Task 15, plus 3D Speed
// from Task 24) already have their own widget-level xxxChanged signal, so
// those seven connect straight back to the model here. The other eight
// have no per-field widget signal; those
// push through syncDisplaySettingsFromWidget(), called explicitly from
// every one of their write sites instead (see that method and its
// call sites).
// Integration: renderer reflection still emits to controls, but every
// model-to-renderer applier is suppressed while publishing native state.
void SpectrumWidget::bindDisplaySettings()
{
    // ---- Model -> widget (all fifteen) ----
    connect(m_displaySettings, &DisplaySettingsModel::wfColorSchemeChanged,
            this, [this](int v) { if (!m_reflectingDisplaySettings) { setWfColorScheme(static_cast<WfColorScheme>(v)); } });
    connect(m_displaySettings, &DisplaySettingsModel::wfColorGainChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setWfColorGain(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::wfBlackLevelChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setWfBlackLevel(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::refLevelChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setRefLevel(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::dynamicRangeChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setDynamicRange(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::fillAlphaChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setFillAlpha(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::panFillChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setPanFillEnabled(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::spectrumFracChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setSpectrumFrac(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::spectrumRenderModeChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setSpectrumRenderMode(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::dssFloorDepthChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setDssFloorDepth(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::dssGainChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setDssGain(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::dssRowSpanChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setDssRowSpan(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::dssAngleChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setDssAngle(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::dssRowDividerChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setDssRowDivider(v); } });
    connect(m_displaySettings, &DisplaySettingsModel::threeDSliceDepthChanged,
            this, [this](auto v) { if (!m_reflectingDisplaySettings) { setThreeDSliceDepth(v); } });

    // ---- Widget -> model (3D seven only; the other eight push explicitly
    //      via syncDisplaySettingsFromWidget()) ----
    connect(this, &SpectrumWidget::spectrumRenderModeChanged,
            m_displaySettings, &DisplaySettingsModel::setSpectrumRenderMode);
    connect(this, &SpectrumWidget::dssFloorDepthChanged,
            m_displaySettings, &DisplaySettingsModel::setDssFloorDepth);
    connect(this, &SpectrumWidget::dssGainChanged,
            m_displaySettings, &DisplaySettingsModel::setDssGain);
    connect(this, &SpectrumWidget::dssRowSpanChanged,
            m_displaySettings, &DisplaySettingsModel::setDssRowSpan);
    connect(this, &SpectrumWidget::dssAngleChanged,
            m_displaySettings, &DisplaySettingsModel::setDssAngle);
    connect(this, &SpectrumWidget::dssRowDividerChanged,
            m_displaySettings, &DisplaySettingsModel::setDssRowDivider);
    connect(this, &SpectrumWidget::threeDSliceDepthChanged,
            m_displaySettings, &DisplaySettingsModel::setThreeDSliceDepth);
}

// Pushes the widget's current value for all fifteen DisplaySettingsModel
// fields into the model -- see bindDisplaySettings()'s comment. Each
// model setter below carries its own equality guard, so calling all
// fifteen unconditionally on every write site is safe: only the field
// that actually changed emits, and any echo back into this widget's own
// applier is absorbed by ITS guard in turn (see the class's
// echo-termination note in DisplaySettingsModel.h).
//
// Task 20 gap fix: the seven 3D fields (spectrumRenderMode, dssFloorDepth,
// dssGain, dssRowSpan, dssAngle, dssRowDivider, threeDSliceDepth) already
// reach the model through their own widget-level xxxChanged signal
// (bindDisplaySettings()'s "Widget -> model" section), so pushing them
// here too is a no-op at every call site except one: loadSettings()
// assigns six of the seven (all but dssFloorDepth) directly, with no
// signal, so without this push the model's six fields would still hold
// ship defaults after a persisted-settings load even though the widget
// itself renders the loaded values. dssFloorDepth is pushed too even
// though loadSettings() never touches it, since this method's contract
// is "whatever the widget currently holds," which dssFloorDepth always
// has a value for (the Task 17 per-band bridge keeps it current).
void SpectrumWidget::syncDisplaySettingsFromWidget()
{
    const QScopedValueRollback<bool> reflecting(m_reflectingDisplaySettings, true);
    m_displaySettings->setWfColorScheme(static_cast<int>(m_wfColorScheme));
    m_displaySettings->setWfColorGain(m_wfColorGain);
    m_displaySettings->setWfBlackLevel(m_wfBlackLevel);
    m_displaySettings->reflectDbmRange(m_refLevel, m_dynamicRange);
    m_displaySettings->setFillAlpha(m_fillAlpha);
    m_displaySettings->setPanFill(m_panFill);
    m_displaySettings->setSpectrumFrac(m_spectrumFrac);
    m_displaySettings->setSpectrumRenderMode(static_cast<int>(m_spectrumRenderMode));
    m_displaySettings->setDssFloorDepth(m_dssFloorDepth);
    m_displaySettings->setDssGain(m_dssGain);
    m_displaySettings->setDssRowSpan(m_dssRowSpan);
    m_displaySettings->setDssAngle(m_dssAngle);
    m_displaySettings->setDssRowDivider(m_dssRowDivider);
    m_displaySettings->setThreeDSliceDepth(m_threeDSliceDepth);
}

void SpectrumWidget::setFrequencyRange(double centerHz, double bandwidthHz)
{
    const bool bwChanged = !qFuzzyCompare(m_bandwidthHz, bandwidthHz);

    // ── Sub-epic E: detect large shifts (band jumps) for history-clear ─
    // From AetherSDR SpectrumWidget.cpp:1042-1062 [@0cd4559]
    //   adapter: NereusSDR uses Hz throughout; threshold expressed as a
    //   fraction of the new half-bandwidth, same as upstream.
    const double oldCenterHz    = m_centerHz;
    const double oldBandwidthHz = m_bandwidthHz;
    const double newCenterHz    = centerHz;
    const double newBandwidthHz = bandwidthHz;
    const double halfBwHz       = newBandwidthHz / 2.0;
    const bool   largeShift     = bwChanged
        || (halfBwHz > 0.0 && std::abs(newCenterHz - oldCenterHz) > halfBwHz * 0.25);

    if (largeShift && oldBandwidthHz > 0.0 && newBandwidthHz > 0.0) {
        // Reproject the still-live image; the history will be cleared next.
        // From AetherSDR SpectrumWidget.cpp:1051 [@0cd4559]
        reprojectWaterfall(oldCenterHz, oldBandwidthHz, newCenterHz, newBandwidthHz);

        // ── NereusSDR divergence: clear history on largeShift to keep the
        //    rewind window coherent with the current band. See plan
        //    §authoring-time #3.
        clearWaterfallHistory();
    } else if (oldBandwidthHz > 0.0 && newBandwidthHz > 0.0) {
        // Small pan/zoom: reproject only — history survives.
        // From AetherSDR SpectrumWidget.cpp:1093 [@0cd4559]
        reprojectWaterfall(oldCenterHz, oldBandwidthHz, newCenterHz, newBandwidthHz);
    }

    applyViewWindow(centerHz, bandwidthHz);
    updateVfoPositions();
#ifdef NEREUS_GPU_SPECTRUM
    // Band-plan strip depends on m_centerHz/m_bandwidthHz — invalidate the
    // static overlay so the strip repositions correctly on freq/zoom changes.
    markOverlayDirty();
#else
    update();
#endif

    // Phase 3G-12: persist zoom level on every bandwidth change so the
    // visible span survives app restarts. Center frequency is not saved
    // here — it's persisted via SliceModel's own save path.
    if (bwChanged) {
        scheduleSettingsSave();
    }

    // Phase 3F Sub-Epic F Tasks 7-10: auto-derive extendedMode from zoom.
    // When the visible bandwidth exceeds the DDC sample rate (operator
    // zoomed a 192 kHz DDC out to e.g. 5 MHz visible), set extended mode
    // so the wideband ADC stream can fill the wings via the Task 11
    // chain. The "off" direction also fires here when the operator
    // zooms back inside the listenable island.
    recomputeExtendedMode();
}

void SpectrumWidget::setCenterFrequency(double centerHz)
{
    if (!qFuzzyCompare(m_centerHz, centerHz)) {
        // Route through setFrequencyRange so the Sub-epic E reproject +
        // largeShift-clear path runs on center-only band jumps too
        // (e.g. MainWindow band-jump path at MainWindow.cpp:2261).
        setFrequencyRange(centerHz, m_bandwidthHz);
    }
}

void SpectrumWidget::setDdcCenterFrequency(double hz)
{
    if (!qFuzzyCompare(m_ddcCenterHz, hz)) {
        m_ddcCenterHz = hz;
        update();
        // Notify listeners (MainWindow wires this to refresh MaxBin's
        // slice-offset so its scan window follows the slice when the
        // DDC NCO moves without a slice retune).
        emit ddcCenterFrequencyChanged(hz);
    }
}

void SpectrumWidget::setSampleRate(double hz)
{
    if (!qFuzzyCompare(m_sampleRateHz, hz)) {
        m_sampleRateHz = hz;
        update();
        // Phase 3F Sub-Epic F Tasks 7-10: sample-rate change rebases the
        // extended-mode derivation (operator may have e.g. switched a
        // radio from 192 kHz to 384 kHz, shrinking the wing).
        recomputeExtendedMode();
    }
}

// 3M-5d follow-up: setters for TX-side bin-frequency context.  See
// visibleBinRange() for usage.  Both are safe to call at any time;
// the MOX branch in visibleBinRange picks them up only while
// m_moxOverlay is true so RX-only operation is unaffected.
void SpectrumWidget::setTxCenterFrequency(double hz)
{
    if (!qFuzzyCompare(m_txCenterHz + 1.0, hz + 1.0)) {
        m_txCenterHz = hz;
        if (m_moxOverlay) {
            update();
        }
    }
}

void SpectrumWidget::setTxSampleRate(double hz)
{
    if (hz > 0.0 && !qFuzzyCompare(m_txSampleRateHz, hz)) {
        m_txSampleRateHz = hz;
        if (m_moxOverlay) {
            update();
        }
    }
}

void SpectrumWidget::setFilterOffset(int lowHz, int highHz)
{
    m_filterLowHz = lowHz;
    m_filterHighHz = highHz;
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
#else
    update();
#endif
}

void SpectrumWidget::setDbmRange(float minDbm, float maxDbm)
{
    m_refLevel = maxDbm;
    m_dynamicRange = maxDbm - minDbm;
    // Same reason as the drag: the scale is cached chrome, the trace is not.
    markOverlayDirty();
    update();
    // Note: callers that invoke setDbmRange deliberately (Copy button, user drag)
    // schedule their own save. The NF-aware grid onNoiseFloorChanged() avoids
    // scheduling saves because it fires at 500ms cadence.

    // Issue #230 fix: when "Use spectrum min/max" is on, the spectrum
    // grid drives the persistent waterfall thresholds — once per
    // grid change, not per render frame.  Mirrors Thetis
    // setWaterfallGainsIfLinkedToSpectrum at console.cs:9098-9101
    // [v2.10.3.13]:
    //     if (m_bWaterfallUseRX1SpectrumMinMax && rx == 1) {
    //         Display.WaterfallLowThreshold  = SetupForm.DisplayGridMin;
    //         Display.WaterfallHighThreshold = SetupForm.DisplayGridMax;
    //     }
    if (m_wfUseSpectrumMinMax) {
        setWfLowThreshold(minDbm);
        setWfHighThreshold(maxDbm);
    }
    // Task 18: push, so DisplaySettingsModel follows this widget's
    // ref level / dynamic range regardless of which caller (Copy button,
    // NF-aware grid follow, user drag) reached them via this shared
    // setter rather than the individually-named ones below.
    syncDisplaySettingsFromWidget();
}

// 3D Stacked-Trace Spectrum Plan Task 18: named setters for the two
// fields setDbmRange() above has always written directly. Guards use a
// fixed absolute epsilon rather than qFuzzyCompare's shape: near zero,
// qFuzzyCompare's relative tolerance breaks down, and Fill Alpha
// (guarded the same way as these two) legitimately reaches 0.0 -- the
// same hazard applies to any newly-added float guard.
//
// Bounds are each field's WIDEST existing clamp in this file, not the
// popup slider's narrower [-160,20]/[20,160]: Dyn Range matches
// wheelEvent's dBm-strip-scroll block (qBound(10.0f, ..., 200.0f)); Ref
// Level matches the Task 19 Ctrl-drag gesture a few hundred lines below
// (mouseMoveEvent's m_draggingDbmRange branch, via
// clampDbmRangeForBottom's upstream kMinDisplayDbm/kMaxDisplayDbm =
// -180.0f/80.0f), which is wider than the Shift-scroll block's
// qBound(-160.0f, ..., 20.0f) a few lines below that. Found by running
// tst_dbm_range_drag.cpp against an earlier draft that used -160..20
// here: ctrlDragRange_clampsAtMaximum (which drags refLevel to 60.0f)
// went red, because the model-to-widget round trip this task adds
// narrowed 60.0f back down to 20.0f. Same never-narrower-than-any-
// widget-write-path rule as Dyn Range.
// Integration: native editor bounds are now -200..200; reflection of
// already accepted state bypasses this editing applier entirely.
void SpectrumWidget::setRefLevel(float dBm)
{
    const float clamped = qBound(-200.0f, dBm, 200.0f);
    if (std::abs(m_refLevel - clamped) < 1e-6f) { return; }
    m_refLevel = clamped;
    markOverlayDirty();
    update();
    scheduleSettingsSave();
    ++m_displaySettingsApplyCount;
    syncDisplaySettingsFromWidget();
}

void SpectrumWidget::setDynamicRange(float dB)
{
    const float clamped = qBound(10.0f, dB, 200.0f);
    if (std::abs(m_dynamicRange - clamped) < 1e-6f) { return; }
    m_dynamicRange = clamped;
    markOverlayDirty();
    update();
    scheduleSettingsSave();
    ++m_displaySettingsApplyCount;
    syncDisplaySettingsFromWidget();
}

// Clamp matches the divider drag in mouseMoveEvent
// (m_spectrumFrac = std::clamp(frac, 0.10f, 0.90f)).
void SpectrumWidget::setSpectrumFrac(float frac)
{
    const float clamped = qBound(0.10f, frac, 0.90f);
    if (std::abs(m_spectrumFrac - clamped) < 1e-6f) { return; }
    m_spectrumFrac = clamped;
    markOverlayDirty();
    update();
    scheduleSettingsSave();
    ++m_displaySettingsApplyCount;
    syncDisplaySettingsFromWidget();
}

void SpectrumWidget::setWfColorScheme(WfColorScheme scheme)
{
    // Task 18: added -- setWfColorGain/setWfBlackLevel already had this
    // guard, this one did not (verified, not assumed).
    if (m_wfColorScheme == scheme) { return; }
    m_wfColorScheme = scheme;
    scheduleSettingsSave();
    update();
    ++m_displaySettingsApplyCount;
    syncDisplaySettingsFromWidget();
}

void SpectrumWidget::setWfColorGain(int gain)
{
    if (m_wfColorGain == gain) { return; }
    m_wfColorGain = gain;
    scheduleSettingsSave();
    update();
    ++m_displaySettingsApplyCount;
    syncDisplaySettingsFromWidget();
}

void SpectrumWidget::setWfBlackLevel(int level)
{
    if (m_wfBlackLevel == level) { return; }
    m_wfBlackLevel = level;
    scheduleSettingsSave();
    update();
    ++m_displaySettingsApplyCount;
    syncDisplaySettingsFromWidget();
}

// ---- Phase 3G-8 commit 3 setters ----

void SpectrumWidget::setAverageMode(AverageMode m)
{
    // R-IOS-18: route to the split spectrum averaging, as the header
    // promises, whether or not the legacy mode changes (it is Logarithmic
    // from the first load). The renderer reads only m_spectrumAveraging, so
    // without this Reset to Smooth Defaults' "log-recursive averaging" set
    // nothing.
    switch (m) {
    case AverageMode::None:        setSpectrumAveraging(SpectrumAveraging::None); break;
    case AverageMode::Weighted:    setSpectrumAveraging(SpectrumAveraging::Recursive); break;
    case AverageMode::Logarithmic: setSpectrumAveraging(SpectrumAveraging::LogRecursive); break;
    case AverageMode::TimeWindow:  setSpectrumAveraging(SpectrumAveraging::TimeWindow); break;
    case AverageMode::Count:       break;
    }
    if (m_averageMode == m) {
        return;
    }
    m_averageMode = m;
    // Force a fresh baseline for the new mode so a switch doesn't
    // carry stale averager state forward.  Mirrors WDSP analyzer.c:
    // SetDisplayAverageMode at :1854 [v2.10.3.13] which re-init's the
    // av_sum / av_buff accumulators on mode change.
    m_spectrumReducer.clearAveraging();
    m_extendedSpectrumAvenger.clear();
    scheduleSettingsSave();
    update();
}

// ---- Task 2.1: Detector + Averaging split setters ----
// Ported from Thetis specHPSDR.cs:302-415 [v2.10.3.13].
// RX1 scope dropped — NereusSDR applies as global panadapter default
// with per-pan override via ContainerSettings dialog (3G-6 pattern).

void SpectrumWidget::setSpectrumDetector(SpectrumDetector d)
{
    if (m_spectrumDetector == d) { return; }
    const bool normalizeWas = normalizeActive();
    m_spectrumDetector = d;
    scheduleSettingsSave();
    // The detector decides whether the 1 Hz normalise applies, and the
    // shift moves the dBm labels the overlay caches.
    if (normalizeActive() != normalizeWas) {
        markOverlayDirty();
    }
    update();
}

void SpectrumWidget::setSpectrumAveraging(SpectrumAveraging a)
{
    if (m_spectrumAveraging == a) { return; }
    m_spectrumAveraging = a;
    // Reset avenger state so mode change doesn't carry stale history.
    // From WDSP analyzer.c:1854 [v2.10.3.13] SetDisplayAverageMode re-init.
    m_spectrumReducer.clearAveraging();
    m_extendedSpectrumAvenger.clear();
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setWaterfallDetector(SpectrumDetector d)
{
    if (m_waterfallDetector == d) { return; }
    m_waterfallDetector = d;
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setWaterfallAveraging(SpectrumAveraging a)
{
    if (m_waterfallAveraging == a) { return; }
    m_waterfallAveraging = a;
    // Reset waterfall avenger state on mode change (analyzer.c:1854 [v2.10.3.13]).
    m_waterfallReducer.clearAveraging();
    m_extendedWaterfallAvenger.clear();
    scheduleSettingsSave();
    update();
}

// Static helper: apply the detector bin-reduction policy.
// Reduces the input FFT bin vector to outputBins display pixels.
// From Thetis specHPSDR.cs DetTypePan / DetTypeWF integer codes [v2.10.3.13]
// mapped to detector descriptions. NereusSDR performs bin reduction in
// software since it owns the FFTW3 spectrum computation (not the WDSP
// analyzer's SetDisplayDetectorMode DLL path).
//
// Thetis detTypePan 0=Peak 1=Rosenfell 2=Average 3=Sample 4=RMS
// From specHPSDR.cs:308  [v2.10.3.13]: SpecHPSDRDLL.SetDisplayDetectorMode(disp,0,value)
void SpectrumWidget::applyDetector(const QVector<float>& input,
                                   QVector<float>& output,
                                   SpectrumDetector mode, int outputBins)
{
    if (input.isEmpty() || outputBins <= 0) {
        output.clear();
        return;
    }
    output.resize(outputBins);
    const int inSize = input.size();
    const float ratio = static_cast<float>(inSize) / outputBins;

    for (int j = 0; j < outputBins; ++j) {
        const int firstIn = static_cast<int>(j * ratio);
        const int lastIn  = static_cast<int>((j + 1) * ratio);
        const int lo = qBound(0, firstIn, inSize - 1);
        const int hi = qBound(lo, lastIn - 1, inSize - 1);

        switch (mode) {
        case SpectrumDetector::Rosenfell: {
            // Alternate max/min bins into successive pixels (Thetis "Rosenfell").
            // Even output pixels take max, odd take min.
            float mn = input[lo], mx = input[lo];
            for (int k = lo + 1; k <= hi; ++k) {
                if (input[k] > mx) { mx = input[k]; }
                if (input[k] < mn) { mn = input[k]; }
            }
            output[j] = (j & 1) ? mn : mx;
            break;
        }
        case SpectrumDetector::Average: {
            float sum = 0.0f;
            for (int k = lo; k <= hi; ++k) { sum += input[k]; }
            output[j] = sum / static_cast<float>(hi - lo + 1);
            break;
        }
        case SpectrumDetector::Sample:
            // Take first bin in the window (Thetis "Sample").
            output[j] = input[lo];
            break;
        case SpectrumDetector::RMS: {
            // Root-mean-square (Pan only — Thetis "RMS").
            float sumSq = 0.0f;
            for (int k = lo; k <= hi; ++k) {
                // Input is in dBm; convert to linear power, square, sum.
                const float lin = std::pow(10.0f, input[k] / 10.0f);
                sumSq += lin * lin;
            }
            const float rms = std::sqrt(sumSq / static_cast<float>(hi - lo + 1));
            output[j] = (rms > 0.0f) ? 10.0f * std::log10(rms) : -200.0f;
            break;
        }
        case SpectrumDetector::Peak:
        default: {
            // Take max bin in window (Thetis "Peak").
            float mx = input[lo];
            for (int k = lo + 1; k <= hi; ++k) {
                if (input[k] > mx) { mx = input[k]; }
            }
            output[j] = mx;
            break;
        }
        }
    }
}

// Note: the previous static applyAveraging() helper (in-place exponential
// smoothing on a dBm bin array) was deleted in this commit.  Frame averaging
// is now performed by SpectrumAvenger (verbatim WDSP analyzer.c:464-554
// [v2.10.3.13] port), which operates on linear-power display pixels and
// converts to dB at the end per av_mode.  See updateSpectrumLinear().

void SpectrumWidget::setAverageAlpha(float alpha)
{
    // DEPRECATED — overrides only the spectrum alpha. Kept for callers not
    // yet migrated to the time-constant API. The next setSpectrumAverageTimeMs
    // / FPS change will recompute and overwrite this value.
    alpha = qBound(0.0f, alpha, 1.0f);
    if (qFuzzyCompare(m_spectrumAverageAlpha, alpha)) {
        return;
    }
    m_spectrumAverageAlpha = alpha;
    scheduleSettingsSave();
}

// From Thetis specHPSDR.cs:351-380 [v2.10.3.13] — AvTau / AvTauWF setters
// compute the per-side back-multiplier α via Math.Exp(-1.0 / (frame_rate * tau)).
// We mirror that exactly: τ in seconds, fps from the live display timer.
void SpectrumWidget::recomputeAverageAlphas()
{
    // Live FPS — derived from the display timer's interval. Fall back to 30
    // when the timer hasn't started yet (loadSettings runs before the timer
    // is armed). This matches the fallback used in updateSpectrum().
    const int intervalMs = m_displayTimer.interval();
    const int fps = (intervalMs > 0) ? qMax(1, 1000 / intervalMs) : 30;

    // The formula moved to core/spectrum/DisplayFollowers (iPhone app
    // Task 20) so the Core computes an app's constant the same way.
    m_spectrumAverageAlpha  = averageAlphaForTimeMs(m_spectrumAverageTimeMs, fps);
    m_waterfallAverageAlpha = averageAlphaForTimeMs(m_waterfallAverageTimeMs, fps);
}

int SpectrumWidget::overlayFrameRate() const
{
    if (m_remoteSpectrum && m_remoteFps > 0) {
        return m_remoteFps;
    }
    const int intervalMs = m_displayTimer.interval();
    return (intervalMs > 0) ? qMax(1, 1000 / intervalMs) : 30;
}

void SpectrumWidget::setSpectrumAverageTimeMs(int ms)
{
    ms = clampAverageTimeMs(ms);
    if (m_spectrumAverageTimeMs == ms) {
        return;
    }
    m_spectrumAverageTimeMs = ms;
    recomputeAverageAlphas();
    scheduleSettingsSave();
}

void SpectrumWidget::setWaterfallAverageTimeMs(int ms)
{
    ms = clampAverageTimeMs(ms);
    if (m_waterfallAverageTimeMs == ms) {
        return;
    }
    m_waterfallAverageTimeMs = ms;
    recomputeAverageAlphas();
    scheduleSettingsSave();
}

void SpectrumWidget::setPeakHoldEnabled(bool on)
{
    if (m_peakHoldEnabled == on) {
        return;
    }
    m_peakHoldEnabled = on;
    if (!on) {
        m_pxPeakHold.clear();
        if (m_peakHoldDecayTimer) {
            m_peakHoldDecayTimer->stop();
        }
    } else {
        if (!m_peakHoldDecayTimer) {
            m_peakHoldDecayTimer = new QTimer(this);
            m_peakHoldDecayTimer->setSingleShot(false);
            connect(m_peakHoldDecayTimer, &QTimer::timeout, this, [this]() {
                // Decay: reset peak hold to current rendered pixels so fresh
                // peaks start tracking again from "now".  Pixel-space tracker
                // since the spectrum trace renders from m_renderedPixels.
                if (m_peakHoldEnabled) {
                    m_pxPeakHold = m_renderedPixels;
                    update();
                }
            });
        }
        m_peakHoldDecayTimer->start(m_peakHoldDelayMs);
    }
    scheduleSettingsSave();
    update();
    emit peakHoldEnabledChanged(on);
}

void SpectrumWidget::setPeakHoldDelayMs(int ms)
{
    ms = qBound(100, ms, 60000);
    if (m_peakHoldDelayMs == ms) {
        return;
    }
    m_peakHoldDelayMs = ms;
    if (m_peakHoldDecayTimer && m_peakHoldDecayTimer->isActive()) {
        m_peakHoldDecayTimer->start(ms);
    }
    scheduleSettingsSave();
}

// ---- Spectrum Peaks: one set of settings for every pan ----
//
// The Spectrum Peaks keys are stored once, not per pan (SpectrumPeaksPage).
// Every SpectrumWidget registers itself so a Setup change reaches every pan
// at once, not just the one Setup is pointed at.

void SpectrumWidget::loadSpectrumPeaksSettings()
{
    auto& s = AppSettings::instance();
    const auto flag = [&s](const char* key) {
        return s.value(QLatin1String(key), QStringLiteral("False")).toString()
            == QStringLiteral("True");
    };

    // Active Peak Hold
    const bool aphOn = flag("DisplayActivePeakHoldEnabled");
    const int aphDur = s.value(QStringLiteral("DisplayActivePeakHoldDurationMs"),
                               QStringLiteral("2000")).toInt();
    const int aphDrop = s.value(QStringLiteral("DisplayActivePeakHoldDropDbPerSec"),
                                QStringLiteral("6")).toInt();
    m_activePeakHold.setDurationMs(qBound(100, aphDur, 60000));
    m_activePeakHold.setDropDbPerSec(qBound(0.1, static_cast<double>(aphDrop), 120.0));
    m_activePeakHold.setFill(flag("DisplayActivePeakHoldFill"));
    m_activePeakHold.setOnTx(flag("DisplayActivePeakHoldOnTx"));
    if (aphOn != m_activePeakHold.enabled()) {
        // As setActivePeakHoldEnabled: off empties the trace, on starts it
        // afresh (Thetis display.cs:460-470 [v2.10.3.15]).
        m_activePeakHold.setEnabled(aphOn);
        m_activePeakHold.clear();
    }
    // NereusSDR-original — distinct peak trace colour so it stays visible
    // when the data-line colour is changed (e.g. Smooth Defaults paints
    // the live trace pure white). Default gold (#FFD700FF).
    m_activePeakHoldColor = ColorSwatchButton::colorFromHex(
        s.value(QStringLiteral("DisplayActivePeakHoldColor"),
                QStringLiteral("#FFD700FF")).toString());

    // Peak Blobs — NereusSDR ships disabled by default (deviation from
    // Thetis Display.cs:4395 [v2.10.3.13] m_bPeakBlobMaximums = true).
    const bool blobOn = flag("DisplayPeakBlobsEnabled");
    const int blobCount = s.value(QStringLiteral("DisplayPeakBlobsCount"),
                                  QStringLiteral("3")).toInt();
    const int blobHoldMs = s.value(QStringLiteral("DisplayPeakBlobsHoldMs"),
                                   QStringLiteral("500")).toInt();
    const int blobFall = s.value(QStringLiteral("DisplayPeakBlobsFallDbPerSec"),
                                 QStringLiteral("6")).toInt();
    m_peakBlobs.setCount(qMax(1, blobCount));
    m_peakBlobs.setInsideFilterOnly(flag("DisplayPeakBlobsInsideFilterOnly"));
    m_peakBlobs.setHoldEnabled(flag("DisplayPeakBlobsHoldEnabled"));
    m_peakBlobs.setHoldMs(blobHoldMs);
    m_peakBlobs.setHoldDrop(flag("DisplayPeakBlobsHoldDrop"));
    m_peakBlobs.setFallDbPerSec(static_cast<double>(blobFall));
    if (blobOn != m_peakBlobs.enabled()) {
        m_peakBlobs.setEnabled(blobOn);
    }

    // Persisted format is "#RRGGBBAA" via ColorSwatchButton::colorToHex;
    // use the matching colorFromHex helper so alpha lands correctly.
    const QColor blobColor = ColorSwatchButton::colorFromHex(
        s.value(QStringLiteral("DisplayPeakBlobColor"),
                QStringLiteral("#FF4500FF")).toString());
    if (blobColor != m_peakBlobColor) {
        m_peakBlobColor = blobColor;
        m_blobMarkerPixmap = QPixmap();   // rebuilt at the next paint
    }
    m_peakBlobTextColor = ColorSwatchButton::colorFromHex(
        s.value(QStringLiteral("DisplayPeakBlobTextColor"),
                QStringLiteral("#7FFF00FF")).toString());
}

void SpectrumWidget::reloadSpectrumPeaksSettings()
{
    loadSpectrumPeaksSettings();
    markOverlayDirty();
    update();
}

void SpectrumWidget::reloadSpectrumPeaksSettingsOnAllPans()
{
    for (SpectrumWidget* pan : spectrumPeaksPans()) {
        pan->reloadSpectrumPeaksSettings();
    }
}

// R-IOS-18: the display settings stored once for every pan (spectrum
// overlays, normalize, the peak value readout, the grid's noise-floor
// tracking, the band plan text size). A change on one pan, from Setup or
// anywhere else, goes to every other pan, so no pan keeps an old value and
// writes it back on its next save. The setters return early on an equal
// value, which ends the fan-out.
void SpectrumWidget::shareWithOtherPans(const std::function<void(SpectrumWidget*)>& apply)
{
    const std::vector<SpectrumWidget*> pans = spectrumPeaksPans();
    for (SpectrumWidget* pan : pans) {
        if (pan != this) {
            apply(pan);
        }
    }
}

// ---- Active Peak Hold trace setters (Task 2.5) ----
// From Thetis display.cs m_bActivePeakHold / groupBoxTS21 [v2.10.3.13].

void SpectrumWidget::setActivePeakHoldEnabled(bool on)
{
    m_activePeakHold.setEnabled(on);
    // Off empties the trace; on starts it afresh, as Thetis's setter does
    // (display.cs:460-470 [v2.10.3.15]:
    //   if (m_bSpectralPeakHoldRX1) { ResetSpectrumPeaks(1); }).
    m_activePeakHold.clear();
    // Force GPU overlay rebuild now — the per-frame nudge in updateSpectrum()
    // only fires once spectrum frames arrive, leaving a stale overlay between
    // the toggle and the next frame.  markOverlayDirty() is guarded for
    // CPU-only builds (Linux without NEREUS_GPU_SPECTRUM).
    markOverlayDirty();
}

void SpectrumWidget::setActivePeakHoldDurationMs(int ms)
{
    m_activePeakHold.setDurationMs(qBound(100, ms, 60000));
}

void SpectrumWidget::setActivePeakHoldDropDbPerSec(double r)
{
    m_activePeakHold.setDropDbPerSec(qBound(0.1, r, 120.0));
}

void SpectrumWidget::setActivePeakHoldFill(bool on)
{
    m_activePeakHold.setFill(on);
    update();
}

void SpectrumWidget::setActivePeakHoldOnTx(bool on)
{
    m_activePeakHold.setOnTx(on);
}

void SpectrumWidget::setActivePeakHoldTxActive(bool tx)
{
    m_activePeakHold.setTxActive(tx);
}

void SpectrumWidget::setActivePeakHoldColor(const QColor& c)
{
    m_activePeakHoldColor = c;
    // Force GPU overlay rebuild so the new colour shows immediately.
    markOverlayDirty();
}

// ---- Peak Blobs (Task 2.6) ----
// From Thetis display.cs:4395-4714 [v2.10.3.13]

void SpectrumWidget::setPeakBlobsEnabled(bool e)
{
    m_peakBlobs.setEnabled(e);
    // Force GPU overlay rebuild now (see setActivePeakHoldEnabled comment).
    markOverlayDirty();
}

void SpectrumWidget::setPeakBlobsCount(int n)
{
    m_peakBlobs.setCount(n);
}

void SpectrumWidget::setPeakBlobsInsideFilterOnly(bool i)
{
    m_peakBlobs.setInsideFilterOnly(i);
}

void SpectrumWidget::setPeakBlobsHoldEnabled(bool h)
{
    m_peakBlobs.setHoldEnabled(h);
}

void SpectrumWidget::setPeakBlobsHoldMs(int ms)
{
    m_peakBlobs.setHoldMs(ms);
}

void SpectrumWidget::setPeakBlobsHoldDrop(bool d)
{
    m_peakBlobs.setHoldDrop(d);
}

void SpectrumWidget::setPeakBlobsFallDbPerSec(double r)
{
    m_peakBlobs.setFallDbPerSec(r);
}

void SpectrumWidget::setPeakBlobColor(const QColor& c)
{
    m_peakBlobColor = c;
    rebuildBlobMarkerPixmap();
    update();
}

void SpectrumWidget::rebuildBlobMarkerPixmap()
{
    // 2026-05-26 KG4VCF perf polish: bake the blob marker (a small
    // unfilled ellipse, 1 px stroke, radius 3 in logical pixels)
    // into a QPixmap once.  paintPeakBlobs drawPixmap()'s this at
    // each blob position instead of calling QPainter::drawEllipse.
    //
    // Why: drawEllipse routes through QRasterPaintEnginePrivate::
    // rasterize -> blend_color_generic -> QLatch::waitInternal, which
    // dispatches span-blend work to QThreadPool::globalInstance()
    // and waits.  Under build-load contention those pool workers
    // (DEFAULT QoS) get starved and the main thread sits in
    // __ulock_wait2.  Profile showed 88 samples / 8 s of main-thread
    // time stuck there from just our blob ellipses.  A pixmap blit
    // is a tight memcpy + alpha-blend that doesn't enter the parallel
    // raster path at all.
    //
    // Size: 8x8 logical pixels (radius 3 + 1 px stroke margin + 1 px
    // safety).  Drawn with the current device pixel ratio so we get
    // crisp rendering on Retina without aliasing.
    const qreal dpr = devicePixelRatioF();
    const int side = 8;  // logical pixels
    m_blobMarkerPixmap = QPixmap(side * dpr, side * dpr);
    m_blobMarkerPixmap.setDevicePixelRatio(dpr);
    m_blobMarkerPixmap.fill(Qt::transparent);
    QPainter pm(&m_blobMarkerPixmap);
    pm.setRenderHint(QPainter::Antialiasing, true);
    pm.setPen(QPen(m_peakBlobColor, 1));
    pm.setBrush(Qt::NoBrush);
    // Center at (side/2, side/2), radius 3.
    pm.drawEllipse(QPointF(side / 2.0, side / 2.0), 3.0, 3.0);
}

void SpectrumWidget::setPeakBlobTextColor(const QColor& c)
{
    m_peakBlobTextColor = c;
    update();
}

void SpectrumWidget::setPanFillEnabled(bool on)
{
    // Task 18: guard already present (verified, not assumed) -- left as-is.
    if (m_panFill == on) {
        return;
    }
    m_panFill = on;
    scheduleSettingsSave();
    update();  // vertex gen is next frame; render pass checks m_panFill
    ++m_displaySettingsApplyCount;
    syncDisplaySettingsFromWidget();
}

void SpectrumWidget::setFillAlpha(float a)
{
    a = qBound(0.0f, a, 1.0f);
    // Task 18: guard already present (verified, not assumed) -- left as-is.
    if (qFuzzyCompare(m_fillAlpha, a)) {
        return;
    }
    m_fillAlpha = a;
    scheduleSettingsSave();
    update();
    ++m_displaySettingsApplyCount;
    syncDisplaySettingsFromWidget();
}

void SpectrumWidget::setLineWidth(float w)
{
    w = qBound(0.5f, w, 8.0f);
    if (qFuzzyCompare(m_lineWidth, w)) {
        return;
    }
    m_lineWidth = w;
    scheduleSettingsSave();
    // GPU pipeline line width is 1.0 at QRhi level (portable across
    // backends); QPainter fallback path respects this immediately.
    update();
}

void SpectrumWidget::setGradientEnabled(bool on)
{
    if (m_gradientEnabled == on) {
        return;
    }
    m_gradientEnabled = on;
    scheduleSettingsSave();
    update();  // vertex gen next frame picks flat vs heatmap colours
}

void SpectrumWidget::setDbmCalOffset(float db)
{
    db = clampCalibrationOffsetDb(db);
    if (qFuzzyCompare(m_dbmCalOffset, db)) {
        return;
    }
    m_dbmCalOffset = db;
    scheduleSettingsSave();
    markOverlayDirty();  // dBm scale strip labels shift
    // 2026-05-22 calibration fix: ensure FFT vertex VBO re-runs so the
    // updated m_dbmCalOffset reaches the rendered trace position, not just
    // the axis labels.  FFT data delivery normally triggers update() on its
    // own, but this guards against the case where the cal pushes before any
    // FFT frame has arrived (e.g. controller attaches before connection).
    update();
}

void SpectrumWidget::setFillColor(const QColor& c)
{
    if (!c.isValid() || m_fillColor == c) {
        return;
    }
    m_fillColor = c;
    scheduleSettingsSave();
    update();
}

// ---- Task 2.3: Spectrum text overlay setters ----

// (setShowMHzOnCursor retired in 2026-05 — cursor format unified to always
// MHz; the Setup checkbox now drives m_showCursorFreq for visibility.)

// From Thetis setup.cs:7061 [v2.10.3.13] lblDisplayBinWidth.Text.
void SpectrumWidget::setShowBinWidth(bool on)
{
    if (m_showBinWidth == on) { return; }
    m_showBinWidth = on;
    markOverlayDirty();
    update();
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setShowBinWidth(on); });
}

double SpectrumWidget::binWidthHz() const
{
    // Bin width = sample rate / FFT size.  Plain divide; no factor of 2.
    // From Thetis setup.cs:16151 [v2.10.3.13]:
    //   bin_width = SampleRateRX1 / GetSpecRX(0).FFTSize
    // The legacy * 2 multiplier (preserved through 1A.4 as a "suspicious
    // math" follow-up) was a pre-1A.4 bug that returned half the actual
    // bin width.  Phase 2 source-first port drops it.  m_fullLinearBins
    // is sized to the full FFT (matches FFTEngine::fftSize()).
    // Parity Task 17 (R-R3-01/R-R3-08): a remote pan has no local bins;
    // its FFT runs on the Core at the size the Core granted.
    const int fftSz = (m_remoteSpectrum && m_remoteFftSize > 0) ? m_remoteFftSize
        : (m_fullLinearBins.isEmpty() ? 4096 : m_fullLinearBins.size());
    if (fftSz <= 0 || m_sampleRateHz <= 0.0) { return 0.0; }
    return m_sampleRateHz / fftSz;
}

// formatCursorFreq — always MHz format (4 decimal places, e.g. "14.2700 MHz").
// Earlier integer-Hz alternative dropped to unify with the SpectrumOverlayPanel
// "Cursor Freq" button: both UI surfaces now drive a single visibility flag
// (m_showCursorFreq), the format is fixed.
QString SpectrumWidget::formatCursorFreq(double hz) const
{
    return QString::number(hz / 1.0e6, 'f', 4) + QStringLiteral(" MHz");
}

// From Thetis display.cs:2304 [v2.10.3.13] m_bShowNoiseFloorDBM.
void SpectrumWidget::setShowNoiseFloor(bool on)
{
    if (m_showNoiseFloor == on) { return; }
    m_showNoiseFloor = on;
    // markOverlayDirty() invalidates the GPU overlay cache so the line/text
    // appears (or disappears) on toggle change.  Bare update() alone wasn't
    // sufficient: the GPU path's renderGpuFrame only re-runs the overlay
    // build block when m_overlayStaticDirty is set, so toggling NF without
    // the dirty mark left the cached overlay stale.
    markOverlayDirty();
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setShowNoiseFloor(on); });
}

void SpectrumWidget::setShowNoiseFloorPosition(OverlayPosition pos)
{
    if (m_noiseFloorPosition == pos) { return; }
    m_noiseFloorPosition = pos;
    if (m_showNoiseFloor) { markOverlayDirty(); }
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setShowNoiseFloorPosition(pos); });
}

// From Thetis display.cs:5763-5773 [v2.10.3.13] _fNFshiftDBM setter:
//   if (t < -12f) t = -12f;
//   if (t > 12f) t = 12f;
//   _fNFshiftDBM = t;
void SpectrumWidget::setNFShiftDbm(float db)
{
    const float clamped = NoiseFloorFollower::clampShiftDb(db);
    if (m_nfShiftDbm == clamped) { return; }
    m_nfShiftDbm = clamped;
    if (m_showNoiseFloor) { markOverlayDirty(); }
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setNFShiftDbm(db); });
}

void SpectrumWidget::setNoiseFloorColor(const QColor& c)
{
    if (!c.isValid() || m_noiseFloorColor == c) { return; }
    m_noiseFloorColor = c;
    if (m_showNoiseFloor) { markOverlayDirty(); }
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setNoiseFloorColor(c); });
}

void SpectrumWidget::setNoiseFloorTextColor(const QColor& c)
{
    if (!c.isValid() || m_noiseFloorTextColor == c) { return; }
    m_noiseFloorTextColor = c;
    if (m_showNoiseFloor) { markOverlayDirty(); }
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setNoiseFloorTextColor(c); });
}

void SpectrumWidget::setNoiseFloorFastColor(const QColor& c)
{
    if (!c.isValid() || m_noiseFloorFastColor == c) { return; }
    m_noiseFloorFastColor = c;
    if (m_showNoiseFloor && m_noiseFloor.fastAttack()) { markOverlayDirty(); }
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setNoiseFloorFastColor(c); });
}

void SpectrumWidget::setNoiseFloorLineWidth(float w)
{
    const float clamped = qBound(1.0f, w, 5.0f);
    if (m_noiseFloorLineWidth == clamped) { return; }
    m_noiseFloorLineWidth = clamped;
    if (m_showNoiseFloor) { markOverlayDirty(); }
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setNoiseFloorLineWidth(w); });
}

// From Thetis display.cs:917-927 [v2.10.3.13] FastAttackNoiseFloorRX1 setter,
// now in core/spectrum/DisplayFollowers (NoiseFloorFollower::setFastAttack),
// which stamps the trigger time on every call.
void SpectrumWidget::setNoiseFloorFastAttack(bool on)
{
    // Thetis's FastAttackNoiseFloorRX1 setter clears the good flag on any
    // set (display.cs:925-934 [v2.10.3.15]): m_bNoiseFloorGoodRX1 = false;
    m_gridFollowNfGood = false;
    if (!m_noiseFloor.setFastAttack(on, QDateTime::currentMSecsSinceEpoch())) { return; }
    if (m_showNoiseFloor) { markOverlayDirty(); }
}

// Source-first port of Thetis processNoiseFloor — display.cs:5866-5912 [v2.10.3.13],
// now in core/spectrum/DisplayFollowers (NoiseFloorFollower::process) so the
// Core runs the same estimate for an app's display.
void SpectrumWidget::processNoiseFloor()
{
    // The noise floor is a MEASUREMENT, so it reads the undented pixels
    // (design section 8.3).  Upstream does the same: its accumulator takes
    // max_copy from the pristine array while everything else in that loop
    // takes the dented max - display.cs:5256-5259 [v2.10.3.15].
    const QVector<float>& src = measurementPixels();
    if (src.isEmpty()) { return; }
    const int fps = overlayFrameRate();
    if (m_noiseFloor.process(src, fps, QDateTime::currentMSecsSinceEpoch())) {
        if (m_showNoiseFloor) { markOverlayDirty(); }
    }
    // The noise floor Thetis exposes outside Display, set each receive
    // frame that is not in fast attack (gridFollowTick reads it).
    // From Thetis display.cs:5398-5404 [v2.10.3.15]:
    //     if (!m_bFastAttackNoiseFloorRX1 && !bPreviousRX1)
    //     {
    //         m_fNoiseFloorRX1 = m_fLerpAverageRX1 + _fNFshiftDBM;
    //         m_bNoiseFloorGoodRX1 = true;
    //     }
    // Thetis computes it only while not local_mox.
    if (!m_moxOverlay && !m_noiseFloor.fastAttack()) {
        m_gridFollowNfDbm = m_noiseFloor.lerpAverage() + m_nfShiftDbm;
        m_gridFollowNfGood = true;
    }
}

#ifdef NEREUS_GPU_SPECTRUM
// I2 fix (final review of the 3D stacked-trace spectrum port,
// docs/architecture/2026-08-08-3d-stacked-trace-spectrum-plan.md):
// NereusSDR-original -- AetherSDR has no overlay cache to keep fresh.
//
// The 3D dBm-scale strip is baked into m_overlayStatic only when
// m_overlayStaticDirty is set (renderGpuFrame's overlay block calls
// drawDbmScale3D(..., dssFloorDbm()) inside that guard), but dssFloorDbm()
// == m_nfLerpAverage - m_dssFloorDepth re-lerps every frame in
// processNoiseFloor() above without dirtying anything. Left unchecked, the
// baked strip goes stale after any floor shift (e.g. a band change) and
// stays stale indefinitely -- nothing else re-dirties it for this reason.
//
// Dirty ONLY when the ROUNDED LABEL SET drawDbmScaleLabels() would actually
// draw has changed -- not on every sub-pixel lerp step. m_overlayStaticDirty
// is one flag for the WHOLE cached overlay (grid, band plan, freq scale,
// etc.), and the perf-fix comments around the overlay-rebuild block further
// down in this file identify that rebuild as the dominant per-paint cost;
// dirtying unconditionally here would rebuild all of it every frame and
// regress every GPU user, including 2D ones (processNoiseFloor runs
// regardless of render mode).
void SpectrumWidget::updateDssScaleOverlayFreshness()
{
    if (!m_dbmScaleVisible
            || m_spectrumRenderMode != SpectrumRenderMode::Mode3D) {
        return;
    }
    const float floorDbm = dssFloorDbm();
    const float span = dssRoundedSpanDb();
    if (m_dssLastBakedFloorDbm.has_value()) {
        const QVector<int> bakedLabels = NereusSDR::DbmStrip::dssRoundedLabelSet(
            *m_dssLastBakedFloorDbm + span, span);
        const QVector<int> liveLabels = NereusSDR::DbmStrip::dssRoundedLabelSet(
            floorDbm + span, span);
        if (bakedLabels == liveLabels) {
            return;
        }
    }
    m_dssLastBakedFloorDbm = floorDbm;
    markOverlayDirty();
}
#endif

// ---- NF-aware grid (Task 2.9) ----
// From Thetis setup.cs:24202-24213 [v2.10.3.13]
// — RX1 scope dropped; NereusSDR applies as global panadapter default
//   with per-pan override via ContainerSettings dialog (3G-6 pattern).

namespace {
std::optional<bool>& gridFollowSaveHoldStore()
{
    static std::optional<bool> hold;
    return hold;
}
} // namespace

void SpectrumWidget::setGridFollowSaveHold(std::optional<bool> saved)
{
    gridFollowSaveHoldStore() = saved;
}

std::optional<bool> SpectrumWidget::gridFollowSaveHold()
{
    return gridFollowSaveHoldStore();
}

void SpectrumWidget::setAdjustGridMinToNoiseFloor(bool on)
{
    if (m_adjustGridMinToNF == on) { return; }
    m_adjustGridMinToNF = on;
    updateGridFollowTimer();
    scheduleSettingsSave();
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setAdjustGridMinToNoiseFloor(on); });
}

void SpectrumWidget::setNFOffsetGridFollow(int db)
{
    db = qBound(-60, db, 60);
    if (m_nfOffsetGridFollow == db) { return; }
    m_nfOffsetGridFollow = db;
    scheduleSettingsSave();
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setNFOffsetGridFollow(db); });
}

void SpectrumWidget::setMaintainNFAdjustDelta(bool on)
{
    if (m_maintainNFAdjustDelta == on) { return; }
    m_maintainNFAdjustDelta = on;
    scheduleSettingsSave();
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setMaintainNFAdjustDelta(on); });
}

// From Thetis console.cs:46074-46086 [v2.10.3.13] tmrAutoAGC_Tick NF grid block:
//   float setPoint = _lastRX1NoiseFloor - _RX1NFoffsetGridFollow;
//   float fDelta = (float)Math.Abs(SetupForm.DisplayGridMax - SetupForm.DisplayGridMin); // abs incase //MW0LGE [2.9.0.7]
//   if (Math.Abs(SetupForm.DisplayGridMin - setPoint) >= 2)
//   {
//       SetupForm.DisplayGridMin = setPoint;
//       if (_maintainNFAdjustDeltaRX1) SetupForm.DisplayGridMax = setPoint + fDelta;
//   }
// NereusSDR adaptation: offset is added rather than subtracted (default 0 vs Thetis
// default +5); semantically equivalent when user enters a negative offset value.
void SpectrumWidget::onNoiseFloorChanged(float nfDbm)
{
    // ClarityController's NF estimate drives ONLY the optional grid
    // auto-tracking feature.  The NF overlay (line + box + text) uses
    // m_nfLerpAverage / m_nfFftBinAverage maintained by processNoiseFloor
    // for true Thetis parity (display.cs:5866-5912 [v2.10.3.13]) — the
    // ClarityController percentile + EWMA produces a different value than
    // Thetis's bins-below-estimate-mean, so previously the overlay line
    // floated above the visible noise on bands with non-trivial signal
    // density.
    if (!m_adjustGridMinToNF) { return; }
    // While Clarity feeds this pan its estimate, Clarity drives the grid
    // (the add-on is kept); otherwise the pan's own display noise floor does
    // (gridFollowTick, the Thetis tmrAutoAGC port).
    m_lastClarityGridNfMs = QDateTime::currentMSecsSinceEpoch();
    applyGridFollow(nfDbm);
}

// The grid rule both feeds share: Thetis tmrAutoAGC_Tick's grid block
// (quoted above onNoiseFloorChanged).
void SpectrumWidget::applyGridFollow(float nfDbm)
{
    if (!m_adjustGridMinToNF) { return; }

    // Not while transmitting. This tracks a RECEIVE noise floor, and on an
    // ORION-class radio the receiver keeps running through transmit, so the
    // tracker goes on firing at its 500 ms cadence and dragging the grid
    // underneath the transmit graticule.
    //
    // Two things break. The transmit scale the operator is looking at moves
    // on its own, and the MOX fall edge captures whatever the tracker last
    // set rather than what the operator chose -- so the next key-up comes up
    // on a noise-floor-derived range, which is how the dBm labels vanished
    // on the second TUNE at the bench on 2026-08-05.
    //
    // Same reasoning as the Clarity gate 3M-5 already added, and the same
    // reasoning as composeWaterfallActiveThresholds's early return: every
    // receive-side tracker has to stand down while the pan is showing
    // transmit. This one was missed.
    if (m_moxOverlay) { return; }

    const float oldMin = m_refLevel - m_dynamicRange;
    const float oldMax = m_refLevel;
    // abs incase //MW0LGE [2.9.0.7] [original inline comment from console.cs:46081]
    const float delta  = std::abs(oldMax - oldMin);

    const float proposedMin = nfDbm + static_cast<float>(m_nfOffsetGridFollow);

    // From Thetis console.cs:46082 [v2.10.3.13]: only move if delta >= 2 dB
    if (std::abs(oldMin - proposedMin) < 2.0f) { return; }

    const float newMin = proposedMin;
    const float newMax = m_maintainNFAdjustDelta ? (newMin + delta) : oldMax;

    // Update display range directly — do NOT call scheduleSettingsSave() here
    // because the NF-tracking loop fires at 500ms cadence and would flood
    // disk writes. The range reverts to persisted values on next app launch;
    // live NF-tracking then re-adjusts it within the first cadence cycle.
    setDbmRange(newMin, newMax);
}

// The grid's noise-floor tracking on the pan's own display noise floor,
// every 500 ms while it is on. Port of Thetis tmrAutoAGC_Tick's grid block.
// From Thetis console.cs:46136-46167 [v2.10.3.15]:
//     private void tmrAutoAGC_Tick(object sender, EventArgs e)
//     {
//         if (!chkPower.Checked || _mox) return;
//         // every 500ms
//         _lastRX1NoiseFloorGood = Display.IsNoiseFloorGoodRX1;
//         if (!_lastRX1NoiseFloorGood && !_lastRX2NoiseFloorGood) return;
//         if (_lastRX1NoiseFloorGood) _lastRX1NoiseFloor = Display.NoiseFloorRX1; // these update noisefloorgoodrx, and 'use up' the readings
//         //change the display grids if needed to follow NF
//         ... float setPoint = _lastRX1NoiseFloor - _RX1NFoffsetGridFollow;
//         ... float fDelta = (float)Math.Abs(SetupForm.DisplayGridMax - SetupForm.DisplayGridMin); // abs incase MW0LGE [2.9.0.7]
//         ... if (Math.Abs(SetupForm.DisplayGridMin - setPoint) >= 2) { ... }
// Display.NoiseFloorRX1's getter clears the good flag (display.cs:4670-4677
// [v2.10.3.15]), so each reading is used once. The radio-running check is
// the frames themselves: without frames the floor is never good.
void SpectrumWidget::gridFollowTick()
{
    if (m_moxOverlay) { return; }
    if (!m_gridFollowNfGood) { return; }
    const float nfDbm = m_gridFollowNfDbm;
    m_gridFollowNfGood = false;   // 'use up' the reading
    // Clarity keeps the pan it feeds (onNoiseFloorChanged).
    if (m_lastClarityGridNfMs > 0
        && QDateTime::currentMSecsSinceEpoch() - m_lastClarityGridNfMs < kClarityGridHoldMs) {
        return;
    }
    applyGridFollow(nfDbm);
}

void SpectrumWidget::updateGridFollowTimer()
{
    if (!m_adjustGridMinToNF) {
        if (m_gridFollowTimer) { m_gridFollowTimer->stop(); }
        return;
    }
    if (!m_gridFollowTimer) {
        m_gridFollowTimer = new QTimer(this);
        // Thetis tmrAutoAGC ticks every 500 ms ("// every 500ms",
        // console.cs:46140 [v2.10.3.15]).
        m_gridFollowTimer->setInterval(500);
        connect(m_gridFollowTimer, &QTimer::timeout, this, &SpectrumWidget::gridFollowTick);
    }
    m_gridFollowTimer->start();
}

// From Thetis specHPSDR.cs:325 [v2.10.3.13] NormOneHzPan.
// Routes to SetDisplayNormOneHz — stored here; propagated to WDSP
// spectrum engine when integrated in Task 5.x.
void SpectrumWidget::setDispNormalize(bool on)
{
    if (m_dispNormalize == on) { return; }
    m_dispNormalize = on;
    // The shift is applied at render time inside dbmToY / dbmToYf
    // (-10 * log10(binWidthHz)) so the entire spectrum trace, NF line,
    // peak hold, peak blobs, dBm-scale labels, and grid lines all
    // recompose to a 1-Hz reference bandwidth in lockstep.  The WDSP
    // SetDisplayNormOneHz path (Thetis specHPSDR.cs:325) would do the
    // same thing inside the analyzer; doing it at the rendering stage
    // keeps the FFT engine untouched and the toggle is reversible
    // without a channel rebuild.
    markOverlayDirty();
    update();
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setDispNormalize(on); });
}

// From Thetis console.cs:20073-20080 [v2.10.3.13] PeakTextDelay / timer_peak_text.
// Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
// ShowPeakValueOverlay creates or destroys the throttle timer as needed.
void SpectrumWidget::setShowPeakValueOverlay(bool on)
{
    if (m_showPeakValueOverlay == on) { return; }
    m_showPeakValueOverlay = on;
    markOverlayDirty();
    if (on) {
        if (!m_peakTextTimer) {
            m_peakTextTimer = new QTimer(this);
            m_peakTextTimer->setSingleShot(false);
            connect(m_peakTextTimer, &QTimer::timeout, this, [this]() {
                // Rebuild the cached peak overlay text from the post-pipeline
                // rendered pixels.  m_renderedPixels spans the visible window
                // (m_centerHz +/- m_bandwidthHz/2) at displayWidth resolution
                // post detector + avenger, mirroring Thetis's per-pixel scan
                // (Display.cs:5249-5316 [v2.10.3.13] inside the per-pixel
                // render loop).
                if (m_renderedPixels.isEmpty()) {
                    m_peakTextCache.clear();
                    return;
                }
                const int n = m_renderedPixels.size();
                float peakDbm = std::numeric_limits<float>::lowest();
                int   peakPx  = 0;
                for (int i = 0; i < n; ++i) {
                    if (m_renderedPixels[i] > peakDbm) {
                        peakDbm = m_renderedPixels[i];
                        peakPx  = i;
                    }
                }
                // Convert pixel index to frequency.  The visible window
                // is m_centerHz +/- m_bandwidthHz/2 mapped across n pixels.
                const double leftHz   = m_centerHz - m_bandwidthHz / 2.0;
                const double pxWidth  = (n > 1) ? m_bandwidthHz / (n - 1) : 0.0;
                const double peakHz   = leftHz + peakPx * pxWidth;
                m_peakTextCache = QStringLiteral("Peak: ")
                    + QString::number(static_cast<double>(peakDbm), 'f', 1)
                    + QStringLiteral(" dBm @ ")
                    + QString::number(peakHz / 1.0e6, 'f', 4)
                    + QStringLiteral(" MHz");
                markOverlayDirty();
                update();
            });
        }
        m_peakTextTimer->start(m_peakTextDelayMs);
    } else {
        if (m_peakTextTimer) {
            m_peakTextTimer->stop();
        }
        m_peakTextCache.clear();
        markOverlayDirty();
        update();
    }
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setShowPeakValueOverlay(on); });
}

void SpectrumWidget::setPeakValuePosition(OverlayPosition pos)
{
    if (m_peakValuePosition == pos) { return; }
    m_peakValuePosition = pos;
    if (m_showPeakValueOverlay) { markOverlayDirty(); }
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setPeakValuePosition(pos); });
}

// From Thetis console.cs:20073-20080 [v2.10.3.13] PeakTextDelay default=500.
// Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
void SpectrumWidget::setPeakTextDelayMs(int ms)
{
    ms = qBound(50, ms, 10000);
    if (m_peakTextDelayMs == ms) { return; }
    m_peakTextDelayMs = ms;
    if (m_peakTextTimer && m_peakTextTimer->isActive()) {
        m_peakTextTimer->setInterval(ms);
    }
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setPeakTextDelayMs(ms); });
}

// From Thetis console.cs:20278 [v2.10.3.13] peak_text_color = Color.DodgerBlue.
void SpectrumWidget::setPeakValueColor(const QColor& c)
{
    if (!c.isValid() || m_peakValueColor == c) { return; }
    m_peakValueColor = c;
    if (m_showPeakValueOverlay) { markOverlayDirty(); }
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setPeakValueColor(c); });
}

// drawTextOverlay — renders text at a corner of specRect with a semi-transparent
// background chip. Shared helper for NF, peak, and bin-width overlays.
// NereusSDR-native (Thetis uses separate per-feature draw calls in DX2D renderer).
void SpectrumWidget::drawTextOverlay(QPainter& p, const QRect& specRect,
                                     OverlayPosition pos, const QString& text,
                                     const QColor& color)
{
    if (text.isEmpty()) { return; }

    QFont font = p.font();
    font.setPixelSize(11);
    font.setBold(true);
    p.setFont(font);

    const QFontMetrics fm(font);
    const int pad  = 6;
    const int tw   = fm.horizontalAdvance(text) + pad * 2;
    const int th   = fm.height() + pad;

    const int margin = 6;
    int x = 0;
    int y = 0;
    switch (pos) {
        case OverlayPosition::TopLeft:
            x = specRect.left()  + margin;
            y = specRect.top()   + margin;
            break;
        case OverlayPosition::TopRight:
            x = specRect.right() - tw   - margin;
            y = specRect.top()   + margin;
            break;
        case OverlayPosition::BottomLeft:
            x = specRect.left()  + margin;
            y = specRect.bottom() - th  - margin;
            break;
        case OverlayPosition::BottomRight:
            x = specRect.right() - tw   - margin;
            y = specRect.bottom() - th  - margin;
            break;
    }

    p.fillRect(x, y, tw, th, QColor(0x10, 0x15, 0x20, 180));
    p.setPen(color);
    p.drawText(x + pad, y + fm.ascent() + pad / 2, text);
}

// ---- Phase 3G-8 commit 4: waterfall setters ----

void SpectrumWidget::setWfHighThreshold(float dbm)
{
    if (qFuzzyCompare(m_wfHighThreshold, dbm)) { return; }
    m_wfHighThreshold = dbm;
    // Mirror into render-active so the next paint reflects the new
    // user value even before composeWaterfallActiveThresholds() runs.
    // AGC / NF-AGC / Clarity will re-override active on their next
    // tick — they are the runtime layer per Thetis display.cs:6575-6594
    // [v2.10.3.13].
    m_wfActiveHighThreshold = dbm;
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setWfLowThreshold(float dbm)
{
    if (qFuzzyCompare(m_wfLowThreshold, dbm)) { return; }
    m_wfLowThreshold = dbm;
    m_wfActiveLowThreshold = dbm;
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setWfAgcEnabled(bool on)
{
    if (m_wfAgcEnabled == on) { return; }
    m_wfAgcEnabled = on;
    m_wfLevels.resetAgc();
    m_coreWfLevels.reset(); // the Core's levels were for the other mode
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setCoreWaterfallLevelsAvailable(bool available)
{
    if (m_coreWfLevelsAvailable == available) { return; }
    m_coreWfLevelsAvailable = available;
    // Levels from an earlier Core, or from before, never carry over.
    m_coreWfLevels.reset();
    m_wfLevels.resetAgc();
}

void SpectrumWidget::setCoreWaterfallLevels(float lowDbm, float highDbm)
{
    if (!std::isfinite(lowDbm) || !std::isfinite(highDbm)) { return; }
    m_coreWfLevels = std::make_pair(lowDbm, highDbm);
    if (coreWaterfallLevelsInUse() && !m_moxOverlay) {
        m_wfActiveLowThreshold = lowDbm;
        m_wfActiveHighThreshold = highDbm;
        update();
    }
}

void SpectrumWidget::setClarityActive(bool on)
{
    m_clarityActive = on;
}

// Task 2.8: NF-AGC — auto-track waterfall thresholds to noise floor + offset.
void SpectrumWidget::setWaterfallNFAGCEnabled(bool on)
{
    if (m_wfNfAgcEnabled == on) { return; }
    m_wfNfAgcEnabled = on;
    m_coreWfLevels.reset(); // the Core's levels were for the other mode
    scheduleSettingsSave();
}

void SpectrumWidget::setWaterfallAGCOffsetDb(int db)
{
    db = WaterfallLevelFollower::clampNoiseFloorAgcOffsetDb(db);
    if (m_wfNfAgcOffsetDb == db) { return; }
    m_wfNfAgcOffsetDb = db;
    scheduleSettingsSave();
}

// 3M-5b: TX waterfall colormap setters.
// From Thetis display.cs:6506-6595 [v2.10.3.13+501e3f51] -- TX thresholds,
// palette, and low-color switch inline per-frame when m_moxOverlay is active.
// MW0LGE [2.9.0.7]  [original inline comment from display.cs:6588; the
// cited 6506-6595 range spans it]
void SpectrumWidget::setTxWfLowLevel(int dbm)
{
    if (m_txWfLowLevel == dbm) { return; }
    m_txWfLowLevel = dbm;
    scheduleSettingsSave();
    update();
    emit txWfSettingsChanged();
}

void SpectrumWidget::setTxWfHighLevel(int dbm)
{
    if (m_txWfHighLevel == dbm) { return; }
    m_txWfHighLevel = dbm;
    scheduleSettingsSave();
    update();
    emit txWfSettingsChanged();
}

void SpectrumWidget::setTxWfPalette(WfColorScheme s)
{
    if (m_txWfPalette == s) { return; }
    m_txWfPalette = s;
    scheduleSettingsSave();
    update();
    emit txWfSettingsChanged();
}

void SpectrumWidget::setTxWfLowColor(const QColor& c)
{
    if (m_txWfLowColor == c) { return; }
    m_txWfLowColor = c;
    scheduleSettingsSave();
    update();
    emit txWfSettingsChanged();
}

void SpectrumWidget::setTxWfGradient(const QString& encoded)
{
    if (m_txWfGradient == encoded) { return; }
    m_txWfGradient = encoded;

    // 3M-5c: Rebuild the cached 101-entry color LUT from the encoded
    // string. Mirrors the WaterfallTXGradient() consumer at Thetis
    // setup.cs:33314-33322 [v2.10.3.13+501e3f51]:
    //   Color[] waterfall_grad = new Color[101];
    //   for (int p = 0; p <= 100; p++)
    //       waterfall_grad[p] = lgLinearGradientTX_waterfall.GetColourAtPercent(p / 100f);
    // GradientPickerWidget::setEncodedText is a silent no-op on empty /
    // malformed input, so a fresh-install empty string leaves the picker
    // at its Thetis-verbatim default 8-stop grayscale ramp.
    m_txCustomLutValid = false;
    if (!encoded.isEmpty()) {
        GradientPickerWidget tempPicker;
        tempPicker.setEncodedText(encoded);
        const QVector<QColor> lut = tempPicker.colorTable(101);
        if (lut.size() == 101) {
            for (int k = 0; k < 101; ++k) {
                m_txCustomLut[k] = lut[k].rgba();
            }
            m_txCustomLutValid = true;
        }
    }

    scheduleSettingsSave();
    update();
    emit txWfSettingsChanged();
}

// Task 2.8: Stop-on-TX — gate pushWaterfallRow() while this pan transmits.
void SpectrumWidget::setWaterfallStopOnTx(bool on)
{
    if (m_wfStopOnTx == on) { return; }
    m_wfStopOnTx = on;
    scheduleSettingsSave();
}

// 3M-5d: gate the internal pushWaterfallRow inside updateSpectrumLinear
// so the external pixout=1 stream from TxAnalyzer drives the waterfall
// plane during MOX without racing the internal path.  Flipped by
// MainWindow on MOX edges; default false leaves RX path untouched.
void SpectrumWidget::setTxExternalWaterfall(bool on)
{
    m_txExternalWaterfall = on;
}

// 3M-5d: bridge slot wired to TxAnalyzer::txWaterfallReady during MOX.
// WDSP's GetPixels(disp, 1, ...) already applied DetTypeWF +
// AverageModeWF in the analyzer.c domain.  Source array spans the
// TX baseband (96 kHz at WdspEngine::kTxDspSampleRate, centered at
// the TX channel = VFO frequency); visibleBinRange() picks up the
// TX context when m_moxOverlay is true (TX center / TX rate) so the
// slice maps to the panadapter's visible RF window correctly.  The
// slice happens here for the WF path; the trace path slices inside
// updateSpectrumLinear (same code).  Both paths therefore use
// matching X-axis math.
void SpectrumWidget::pushTxWaterfallRow(int receiverId,
                                        const QVector<float>& binsDbm)
{
    m_localSpectrumSource = receiverId;
    pushWaterfallRow(txWaterfallRow(binsDbm));
}

QVector<float> SpectrumWidget::txWaterfallRow(const QVector<float>& binsDbm) const
{
    if (binsDbm.isEmpty()) {
        return binsDbm;
    }
    auto [firstBin, lastBin] = visibleBinRange(binsDbm.size());
    if (lastBin < firstBin) {
        return binsDbm;
    }
    // Parity Task 31 (A11): Thetis's waterfall takes the same offset as the
    // trace (display.cs:6588-6601 [v2.10.3.15], //MW0LGE [2.9.0.7],
    // `max = data[i] + fOffset` with fOffset = RX1Offset): keyed, the TX
    // Display Cal Offset. A remote pan's rows carry the Core's already.
    const float offset = displayCalOffsetDb();
    QVector<float> visible(lastBin - firstBin + 1);
    for (int k = firstBin; k <= lastBin; ++k) {
        visible[k - firstBin] = binsDbm[k] + offset;
    }
    return visible;
}

// ---- 3DSS stacked-trace mode ----
// 3D Stacked-Trace Spectrum Plan Task 6 (design doc
// docs/architecture/2026-08-08-3d-stacked-trace-spectrum-design.md).
void SpectrumWidget::setSpectrumRenderMode(int mode)
{
    const SpectrumRenderMode next =
        (mode == static_cast<int>(SpectrumRenderMode::Mode3D))
            ? SpectrumRenderMode::Mode3D
            : SpectrumRenderMode::Mode2D;
    if (m_spectrumRenderMode == next) { return; }
    m_spectrumRenderMode = next;
    if (next == SpectrumRenderMode::Mode2D) {
        traceRxHistoryEvent(RxHistoryEvent::Leave3D);
        // Leaving 3D: drop the ring so re-entering starts clean rather than
        // showing a stack of rows captured at a frequency we have since left.
        m_dss.clear();
        m_dssRowsPushed = 0;
        // Task 24: also drop any in-progress peak-hold fold, so re-entering
        // 3D does not push a row built from bins captured at a frequency
        // (or row width) we have since left.
        m_dssFoldCount = 0;
        m_dssFoldRow.clear();
        m_dssFoldFullBins.clear();
    }
    m_dss.invalidate();
    markOverlayDirty();
    scheduleSettingsSave();
    update();
    // Task 15: announce the settled (normalized) value, not the raw
    // possibly-out-of-range `mode` argument, so a listener never observes
    // a value setSpectrumRenderMode() itself would have rejected.
    // Task 18: this signal is bound to DisplaySettingsModel::
    // setSpectrumRenderMode() in bindDisplaySettings(), so it IS the push
    // for this field -- no separate syncDisplaySettingsFromWidget() call
    // needed here, unlike the eight non-3D setters above.
    ++m_displaySettingsApplyCount;
    emit spectrumRenderModeChanged(static_cast<int>(m_spectrumRenderMode));
}

void SpectrumWidget::setDssFloorDepth(int dB)
{
    const int v = std::clamp(dB, 0, 24);
    if (m_dssFloorDepth == v) { return; }
    m_dssFloorDepth = v;
    m_dss.invalidate();
    markOverlayDirty();
    scheduleSettingsSave();
    update();
    // Guard shape (only emit once the resolved value actually settles)
    // borrowed from AetherSDR SpectrumWidget.cpp:4628 [@1872028c]
    // (setDssFloorDepthForSource's `if (resolvedDepth != previousResolvedDepth)
    // emit dssFloorDepthResolved(...)`), which fed the same overlay menu back
    // for a Flex/Kiwi source-dispatch reason this single-source build does
    // not have -- the early-return guard above already does the settling.
    // Task 18: bound to DisplaySettingsModel::setDssFloorDepth() in
    // bindDisplaySettings(); this emit is the push.
    ++m_displaySettingsApplyCount;
    emit dssFloorDepthChanged(m_dssFloorDepth);
}

void SpectrumWidget::setDssGain(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    if (m_dssGain == v) { return; }
    m_dssGain = v;
    m_dss.invalidate();
    scheduleSettingsSave();
    update();
    ++m_displaySettingsApplyCount;
    emit dssGainChanged(m_dssGain);
}

void SpectrumWidget::setDssRowSpan(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    if (m_dssRowSpan == v) { return; }
    m_dssRowSpan = v;
    // Deliberately no m_dss.invalidate() / markOverlayDirty() here, unlike
    // setDssFloorDepth/setDssGain/setDssAngle/setSpectrumRenderMode above.
    // rowSpanFactor (dssRowSpanTarget(), fed from m_dssRowSpan) is written
    // into the mesh UBO fresh every GPU frame from writeDssMeshUbo() --
    // there is no cached geometry or baked overlay pixel that depends on
    // its value, so the very next frame already reflects a change with no
    // invalidation needed. The CPU fallback (buildDssImage()) takes no
    // row-span input at all and always draws the classic narrowing
    // trapezoid regardless -- see dssRowSpanSupported() in
    // DssMeshGeometry.h for the same "GPU-mesh-only" fact stated from the
    // control-enablement side. Do not "fix" this by adding the calls its
    // siblings have; span is the one 3D control that genuinely does not
    // need them.
    scheduleSettingsSave();
    update();
    ++m_displaySettingsApplyCount;
    emit dssRowSpanChanged(m_dssRowSpan);
}

void SpectrumWidget::setDssAngle(int pct)
{
    const int v = std::clamp(pct, 0, 100);
    if (m_dssAngle == v) { return; }
    m_dssAngle = v;
    m_dss.invalidate();
    markOverlayDirty();
    scheduleSettingsSave();
    update();
    // Task 15: lets Display3DSetupPage follow a change made through the
    // overlay menu (or any other caller) the same way it follows its own
    // slider -- see the round-trip guard in DisplaySetupPages.cpp.
    ++m_displaySettingsApplyCount;
    emit dssAngleChanged(m_dssAngle);
}

// 3D Speed (Task 24, NereusSDR-original -- design doc section 4.5).
void SpectrumWidget::setDssRowDivider(int n)
{
    const int v = std::clamp(n, 0, 10);
    if (m_dssRowDivider == v) { return; }
    m_dssRowDivider = v;
    // Deliberately no m_dss.invalidate() / markOverlayDirty() here, same
    // reasoning as setDssRowSpan() above: the divider changes CADENCE --
    // which rows get pushed, and how fast the glide advances -- not the
    // rendered appearance of a row already sitting in the ring, so there
    // is no cached geometry or baked overlay pixel that depends on it.
    scheduleSettingsSave();
    update();
    ++m_displaySettingsApplyCount;
    emit dssRowDividerChanged(m_dssRowDivider);
}

// See the header comment for the contract. kDssVisibleRows (DssGeometry.h)
// is the ring's fixed visible-row count (96); dividing the waterfall's own
// pixel height by it, rounded and clamped, is what "match the 3D history
// to the waterfall's" means in code -- design doc section 4.5. A null or
// zero-height waterfall (never resized, or resized to nothing) clamps to
// 1 via the lower bound; no special case needed.
int SpectrumWidget::effectiveDssRowDivider() const
{
    if (m_dssRowDivider > 0) {
        return m_dssRowDivider;
    }
    return std::clamp(qRound(double(m_waterfall.height()) / kDssVisibleRows),
                       1, kDssMaxAutoRowDivider);
}

// See the header comment for the contract: period times divider, so the
// glide reaches 1.0 exactly when accumulateDssRow() is due to push the
// next row.
float SpectrumWidget::dssScrollIncrement(int deltaMs) const
{
    const int periodMs = qMax(1, m_wfUpdatePeriodMs) * effectiveDssRowDivider();
    return static_cast<float>(deltaMs) / static_cast<float>(periodMs);
}

void SpectrumWidget::setThreeDSliceDepth(bool on)
{
    if (m_threeDSliceDepth == on) { return; }
    m_threeDSliceDepth = on;
    scheduleSettingsSave();
    update();
    ++m_displaySettingsApplyCount;
    emit threeDSliceDepthChanged(m_threeDSliceDepth);
}

// Issue #230 fix: Clarity is a NereusSDR-only override modeled on
// Thetis's AGC pattern at display.cs:6584 [v2.10.3.13], where the AGC
// running-min is a runtime field (_RX1waterfallPreviousMinValue) that
// flows into per-render locals — never the persisted user fields.
// Previously the Clarity controller called setWfLow/HighThreshold,
// which scheduled a settings save on every tick and silently
// overwrote the user's saved thresholds.
void SpectrumWidget::setClarityWaterfallThresholds(float low, float high)
{
    if (qFuzzyCompare(m_wfActiveLowThreshold, low) &&
        qFuzzyCompare(m_wfActiveHighThreshold, high)) {
        return;
    }
    m_wfActiveLowThreshold  = low;
    m_wfActiveHighThreshold = high;
    update();
    // No scheduleSettingsSave() — Clarity output is runtime state, not
    // a user preference.
}

void SpectrumWidget::setWfOpacity(int percent)
{
    percent = qBound(0, percent, 100);
    if (m_wfOpacity == percent) { return; }
    m_wfOpacity = percent;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setWfUpdatePeriodMs(int ms)
{
    ms = qBound(10, ms, 500);  // matches NereusSDR UI range per plan §10
    if (m_wfUpdatePeriodMs == ms) { return; }
    m_wfUpdatePeriodMs = ms;
    scheduleSettingsSave();

    // 2026-05-25 KG4VCF bench fix: retune the timer-driven waterfall
    // push to the new period so a slider drag actually changes scroll
    // rate immediately (rather than waiting for the next ctor).
    if (m_waterfallTicker) {
        m_waterfallTicker->setUpdatePeriodMs(m_wfUpdatePeriodMs);
    }

    // Sub-epic E: capacity may have changed — debounce history rebuild
    // so slider drag doesn't trash history mid-drag.
    if (m_historyResizeTimer) {
        m_historyResizeTimer->start();
    }
}

// Sub-epic E: depth setter, called from Setup → Display dropdown.
void SpectrumWidget::setWaterfallHistoryMs(qint64 ms)
{
    ms = qBound(static_cast<qint64>(60 * 1000),     // 60 s minimum
                ms,
                static_cast<qint64>(20 * 60 * 1000)); // 20 min maximum
    if (m_waterfallHistoryMs == ms) { return; }
    m_waterfallHistoryMs = ms;

    auto& s = AppSettings::instance();
    s.setValue(settingsKey(QStringLiteral("DisplayWaterfallHistoryMs"), m_panIndex),
               QString::number(m_waterfallHistoryMs));
    s.save();

    if (m_historyResizeTimer) {
        m_historyResizeTimer->start();
    }
}

void SpectrumWidget::setWfUseSpectrumMinMax(bool on)
{
    if (m_wfUseSpectrumMinMax == on) { return; }
    m_wfUseSpectrumMinMax = on;
    scheduleSettingsSave();

    // Issue #230 fix: enabling the flag immediately syncs the
    // persistent waterfall thresholds from the current spectrum
    // range — same effect as Thetis's checkbox handler at
    // setup.cs:19221-19243 [v2.10.3.13], which routes through the
    // WaterfallUseRX1SpectrumMinMax property setter and triggers
    // setWaterfallGainsIfLinkedToSpectrum (console.cs:9098).
    if (on) {
        setWfLowThreshold(m_refLevel - m_dynamicRange);
        setWfHighThreshold(m_refLevel);
    }
    update();
}

void SpectrumWidget::setWfAverageMode(AverageMode m)
{
    if (m_wfAverageMode == m) { return; }
    m_wfAverageMode = m;
    // Reset waterfall avenger on mode change (analyzer.c:1854 [v2.10.3.13]).
    m_waterfallReducer.clearAveraging();
    m_extendedWaterfallAvenger.clear();
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setWfTimestampPosition(TimestampPosition p)
{
    if (m_wfTimestampPos == p) { return; }
    m_wfTimestampPos = p;
    // Start/stop the 1 Hz overlay refresh timer so the clock ticks live.
    if (p != TimestampPosition::None) {
        if (!m_wfTimestampTicker) {
            m_wfTimestampTicker = new QTimer(this);
            m_wfTimestampTicker->setInterval(1000);
            connect(m_wfTimestampTicker, &QTimer::timeout,
                    this, [this]() { markOverlayDirty(); });
        }
        m_wfTimestampTicker->start();
    } else if (m_wfTimestampTicker) {
        m_wfTimestampTicker->stop();
    }
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setWfTimestampMode(TimestampMode m)
{
    if (m_wfTimestampMode == m) { return; }
    m_wfTimestampMode = m;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setShowRxFilterOnWaterfall(bool on)
{
    if (m_showRxFilterOnWaterfall == on) { return; }
    m_showRxFilterOnWaterfall = on;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setShowTxFilterOnRxWaterfall(bool on)
{
    if (m_showTxFilterOnRxWaterfall == on) { return; }
    m_showTxFilterOnRxWaterfall = on;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setShowRxZeroLineOnWaterfall(bool on)
{
    if (m_showRxZeroLineOnWaterfall == on) { return; }
    m_showRxZeroLineOnWaterfall = on;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setShowTxZeroLineOnWaterfall(bool on)
{
    if (m_showTxZeroLineOnWaterfall == on) { return; }
    m_showTxZeroLineOnWaterfall = on;
    scheduleSettingsSave();
    markOverlayDirty();
}

// ---- Phase 3G-8 commit 5: grid / scales setters ----

void SpectrumWidget::setGridEnabled(bool on)
{
    if (m_gridEnabled == on) { return; }
    m_gridEnabled = on;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setShowZeroLine(bool on)
{
    if (m_showZeroLine == on) { return; }
    m_showZeroLine = on;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setShowFps(bool on)
{
    if (m_showFps == on) { return; }
    m_showFps = on;
    m_fpsFrameCount = 0;
    m_fpsLastUpdateMs = 0;
    m_fpsDisplayValue = 0.0f;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setCorePerfLinesProvider(std::function<QStringList()> provider)
{
    m_corePerfLinesProvider = std::move(provider);
    if (m_showPerfOverlay) {
        markOverlayDirty();
        update();
    }
}

QStringList SpectrumWidget::perfOverlayLines() const
{
    // Reads the cached snapshot non-destructively; the 1 Hz poll timer is
    // the sole snapshotAndClearDeltas() consumer.
    const auto stats = PerfMonitor::instance().lastSnapshot();
    QStringList lines;
    lines << QStringLiteral("paint  avg %1 max %2 ms")
                .arg(stats.paintMsAvg, 0, 'f', 1)
                .arg(stats.paintMsMax, 0, 'f', 1)
          << QStringLiteral("gap    avg %1 max %2 ms")
                .arg(stats.gapMsAvg, 0, 'f', 1)
                .arg(stats.gapMsMax, 0, 'f', 1)
          << QStringLiteral("fft    avg %1 max %2 ms")
                .arg(stats.fftMsAvg, 0, 'f', 1)
                .arg(stats.fftMsMax, 0, 'f', 1)
          << QStringLiteral("ovly   avg %1 max %2 ms")
                .arg(stats.ovlyMsAvg, 0, 'f', 1)
                .arg(stats.ovlyMsMax, 0, 'f', 1)
          << QStringLiteral("audio  fill avg %1 min %2 ms (%3 samp)")
                .arg(stats.audioFillAvgMs, 0, 'f', 1)
                .arg(stats.audioFillMinMs, 0, 'f', 1)
                .arg(stats.audioFillSamples)
          << QStringLiteral("audio  underruns %1 (+%2/s)")
                .arg(stats.audioUnderrunsTotal)
                .arg(stats.audioUnderrunsDelta)
          << QStringLiteral("udp    drops %1 (+%2/s)")
                .arg(stats.udpDropsTotal)
                .arg(stats.udpDropsDelta)
          << QStringLiteral("tx iq  underruns %1 (+%2/s)")
                .arg(stats.txIqUnderrunsTotal)
                .arg(stats.txIqUnderrunsDelta)
          << QStringLiteral("tx iq  produced  %1 (+%2/s)")
                .arg(stats.txIqProducedTotal)
                .arg(stats.txIqProducedDelta)
          << QStringLiteral("mem    %1 MB%2")
                .arg(stats.memFootprintMb, 0, 'f', 0)
                .arg(stats.memCompressing
                     ? QStringLiteral(" COMPRESSING")
                     : QString{})
          << QStringLiteral("mlock  %1 regions / %2 MB pinned")
                .arg(memoryLockStats().regionsLocked)
                .arg(memoryLockStats().bytesLocked
                     / (1024.0 * 1024.0), 0, 'f', 1);
    // Parity ruling C13: in a remote window the Core's drops follow this
    // computer's counters, each group headed.
    const QStringList core =
        m_corePerfLinesProvider ? m_corePerfLinesProvider() : QStringList{};
    if (!core.isEmpty()) {
        lines.prepend(QStringLiteral("this computer:"));
        lines << core;
    }
    return lines;
}

void SpectrumWidget::setShowPerfOverlay(bool on)
{
    if (m_showPerfOverlay == on) { return; }
    m_showPerfOverlay = on;
    // Reset the perf counters when toggled on so stats reflect the
    // operator's current question, not stale residue from before.
    if (on) {
        PerfMonitor::instance().resetAll();
    }
    if (m_perfPollTimer) {
        if (on) {
            m_perfPollTimer->start();
        } else {
            m_perfPollTimer->stop();
        }
    }
    AppSettings::instance().setValue(
        QStringLiteral("ShowPerfOverlay"),
        on ? QStringLiteral("True") : QStringLiteral("False"));
    markOverlayDirty();
}

void SpectrumWidget::setCursorFreqVisible(bool on)
{
    if (m_showCursorFreq == on) { return; }
    m_showCursorFreq = on;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setDbmScaleVisible(bool on)
{
    if (m_dbmScaleVisible == on) { return; }
    m_dbmScaleVisible = on;
    scheduleSettingsSave();
    markOverlayDirty();
}

// From AetherSDR SpectrumWidget.cpp:364-368 [@0cd4559]
void SpectrumWidget::setBandPlanManager(NereusSDR::BandPlanManager* mgr)
{
    if (m_bandPlanMgr == mgr) { return; }
    if (m_bandPlanMgr) {
        disconnect(m_bandPlanMgr, nullptr, this, nullptr);
    }
    m_bandPlanMgr = mgr;
    if (mgr) {
        connect(mgr, &NereusSDR::BandPlanManager::planChanged,
                this, [this]() {
                    markOverlayDirty();
                    update();
                });
    }
    markOverlayDirty();
    update();
}

void SpectrumWidget::setBandPlanFontSize(int pt)
{
    pt = std::clamp(pt, 0, 16);
    if (m_bandPlanFontSize == pt) { return; }
    m_bandPlanFontSize = pt;
    markOverlayDirty();
    update();
    // Stored once for every pan: every pan takes it (shareWithOtherPans).
    shareWithOtherPans([&](SpectrumWidget* pan) { pan->setBandPlanFontSize(pt); });
}

// Width of the right-edge column reserved for the dBm scale strip in the
// SPECTRUM row. The waterfall row's time-scale strip widens to 72px when
// paused but does NOT narrow the spectrum — it overlays the right edge of
// the waterfall image instead. Keeping the spectrum reservation at
// kDbmStripW means the spectrum trace doesn't shift when the user pauses.
int SpectrumWidget::effectiveStripW() const
{
    // dBm strip + paused-mode timescale-strip extension.
    // The strip is always present in the *waterfall* row (where the
    // time scale is painted); the dBm strip is in the *spectrum* row.
    // They occupy the same right-edge column.
    return m_dbmScaleVisible ? kDbmStripW : 0;
}

void SpectrumWidget::setFreqLabelAlign(FreqLabelAlign a)
{
    if (m_freqLabelAlign == a) { return; }
    m_freqLabelAlign = a;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setGridColor(const QColor& c)
{
    if (!c.isValid() || m_gridColor == c) { return; }
    m_gridColor = c;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setGridFineColor(const QColor& c)
{
    if (!c.isValid() || m_gridFineColor == c) { return; }
    m_gridFineColor = c;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setHGridColor(const QColor& c)
{
    if (!c.isValid() || m_hGridColor == c) { return; }
    m_hGridColor = c;
    scheduleSettingsSave();
    markOverlayDirty();
}

void SpectrumWidget::setGridTextColor(const QColor& c)
{
    if (!c.isValid() || m_gridTextColor == c) { return; }
    m_gridTextColor = c;
    scheduleSettingsSave();
    markOverlayDirty();
}

// Plan 4 D9c-1: zero-line color split into RX + TX.
// Old setZeroLineColor(c) replaced by setRxZeroLineColor(c) + setTxZeroLineColor(c).
// Both setters write immediately (matching D9b setTxFilterColor / setRxFilterColor)
// rather than using the 500 ms scheduleSettingsSave() — ensures persistence is
// observable synchronously in tests and after crash.
void SpectrumWidget::setRxZeroLineColor(const QColor& c)
{
    if (!c.isValid() || m_rxZeroLineColor == c) { return; }
    m_rxZeroLineColor = c;
    AppSettings::instance().setValue(
        settingsKey(QStringLiteral("DisplayRxZeroLineColor"), m_panIndex),
        c.name(QColor::HexArgb));
    markOverlayDirty();
    update();
}

void SpectrumWidget::setTxZeroLineColor(const QColor& c)
{
    if (!c.isValid() || m_txZeroLineColor == c) { return; }
    m_txZeroLineColor = c;
    AppSettings::instance().setValue(
        settingsKey(QStringLiteral("DisplayTxZeroLineColor"), m_panIndex),
        c.name(QColor::HexArgb));
    markOverlayDirty();
    update();
}

// Plan 4 D9c-3 + Colors & Theme consolidation: reset every theme colour
// exposed via Setup → Appearance → Colors & Theme.
// Original 4 Plan 4 D9/D9c colors are preserved exactly so
// tst_spectrum_tx_overlay::resetDisplayColorsToDefaults() still passes.
void SpectrumWidget::resetDisplayColorsToDefaults()
{
    // Spectrum trace & fill
    setFillColor(QColor(0x00, 0xe5, 0xff));             // default cyan trace

    // Grid colours — defaults match compile-time member initialisers in SpectrumWidget.h
    setGridColor(QColor(255, 255, 255, 40));             // m_gridColor default
    setGridFineColor(QColor(255, 255, 255, 20));         // m_gridFineColor default
    setHGridColor(QColor(255, 255, 255, 40));            // m_hGridColor default
    setGridTextColor(QColor(255, 255, 0));               // m_gridTextColor default (yellow)
    setBandEdgeColor(QColor(255, 0, 0));                 // m_bandEdgeColor default (red)

    // Zero-line colours — Plan 4 D9c-1 (unchanged values, kept for test compat)
    setRxZeroLineColor(QColor(255, 0, 0));               // red — Thetis convention
    setTxZeroLineColor(QColor(255, 184, 0));             // amber — NereusSDR-original

    // Passband overlay colours — Plan 4 D9b (unchanged values, kept for test compat)
    setRxFilterColor(QColor(0, 180, 216, 80));           // matches kRxFilterOverlayFill
    setTxFilterColor(QColor(255, 120, 60, 46));          // matches kTxFilterOverlayFill

    // Waterfall low colour — no backing setter yet; reset handled on the
    // ColorsThemePage side (resets the swatch to Qt::black).
}

// Plan 4 D9c-4: TNF + SubRX scaffolding setters.  No paint code consumes these
// yet.  Persisted so user choices survive the future feature ship.
void SpectrumWidget::setTnfFilterColor(const QColor& c)
{
    if (!c.isValid() || m_tnfFilterColor == c) { return; }
    m_tnfFilterColor = c;
    AppSettings::instance().setValue(
        settingsKey(QStringLiteral("DisplayTnfFilterColor"), m_panIndex),
        c.name(QColor::HexArgb));
    // No update() — no paint code consumes this yet.
}

void SpectrumWidget::setSubRxFilterColor(const QColor& c)
{
    if (!c.isValid() || m_subRxFilterColor == c) { return; }
    m_subRxFilterColor = c;
    AppSettings::instance().setValue(
        settingsKey(QStringLiteral("DisplaySubRxFilterColor"), m_panIndex),
        c.name(QColor::HexArgb));
    // No update() — no paint code consumes this yet.
}

void SpectrumWidget::setBandEdgeColor(const QColor& c)
{
    if (!c.isValid() || m_bandEdgeColor == c) { return; }
    m_bandEdgeColor = c;
    scheduleSettingsSave();
    markOverlayDirty();
}

// Shared overlay consumers of the final local or remote reduced trace.
void SpectrumWidget::updateReducedSpectrumOverlays()
{
    // Legacy per-pixel peak hold -- track running max in display-pixel
    // space.  Replaces the old per-bin m_peakHoldBins.
    if (m_peakHoldEnabled) {
        if (m_pxPeakHold.size() != m_renderedPixels.size()) {
            m_pxPeakHold = m_renderedPixels;
        } else {
            for (int i = 0; i < m_renderedPixels.size(); ++i) {
                if (m_renderedPixels[i] > m_pxPeakHold[i]) {
                    m_pxPeakHold[i] = m_renderedPixels[i];
                }
            }
        }
    }

    // Active Peak Hold trace -- per-display-pixel decay.  From Thetis
    // Display.cs:5341 [v2.10.3.13] spectralPeaks[i] is indexed by pixel
    // (i runs 0..nDecimatedWidth-1) and the y-mapping uses the per-pixel
    // peak.max_dBm value.  Display.cs:5356 decays peak.max_dBm by
    // dBmSpectralPeakFall per second -> /fps per frame (analyzer-adjacent).
    const int intervalMs = m_displayTimer.interval();
    // Parity Task 17: a remote pan's frames arrive at the Core's rate.
    const int fps = overlayFrameRate();

    // Thetis's frame clock (m_dElapsedFrameStart) runs whether or not a
    // feature is on, so a reset's 500 ms display delay (display.cs:859-877
    // [v2.10.3.15]) passes while it is off too: both clocks tick every frame.
    if (m_activePeakHold.enabled()) {
        if (m_activePeakHold.size() != m_renderedPixels.size()) {
            m_activePeakHold.resize(m_renderedPixels.size());
        }
        m_activePeakHold.update(m_renderedPixels);
    }
    m_activePeakHold.tickFrame(fps);

    // Peak Blob detector -- pixel-space local maxima with hold/decay.
    // Filter-passband math becomes pixel-space: visible window is
    // m_centerHz +/- m_bandwidthHz/2 mapped across displayWidth pixels.
    // From Thetis Display.cs:5453-5508 [v2.10.3.13].
    if (m_peakBlobs.enabled() && !m_renderedPixels.isEmpty()) {
        const int n = m_renderedPixels.size();
        int filterLowPx  = 0;
        int filterHighPx = n - 1;
        if (m_peakBlobs.insideOnly() && m_bandwidthHz > 0.0) {
            std::tie(filterLowPx, filterHighPx) = passbandPixels(
                n, m_centerHz, m_bandwidthHz,
                m_vfoHz + m_filterLowHz, m_vfoHz + m_filterHighHz);
        }
        m_peakBlobs.update(m_renderedPixels, filterLowPx, filterHighPx);
    }
    {
        const int frameMs = (m_remoteSpectrum && m_remoteFps > 0)
            ? qMax(1, 1000 / m_remoteFps) : (intervalMs > 0 ? intervalMs : 33);
        m_peakBlobs.tickFrame(fps, frameMs);
    }

    // Force GPU overlay texture re-render when any per-frame overlay is
    // active -- paintEvent's CPU path calls paintActivePeakHoldTrace +
    // paintPeakBlobs from drawSpectrum() every frame, but the GPU path
    // bakes overlays into m_overlayStatic.  Without this nudge, peak
    // indicators only update on Setup-driven state changes (bug from
    // 2026-05-02).
    //
    // TODO: separate m_overlayDynamic layer so static chrome (grid,
    // scales, band plan) doesn't repaint every frame.
    // Per-frame NF estimate update — Thetis display.cs:5385 [v2.10.3.13]
    // calls processNoiseFloor at the same point in its render loop (after
    // the per-pixel accumulator finishes).  Always runs (not gated on
    // m_showNoiseFloor) so the lerp/fft state stays current even when the
    // overlay is toggled off — saves a cold-start visual jump on toggle on.
    processNoiseFloor();
#ifdef NEREUS_GPU_SPECTRUM
    // I2 fix: dirty the cached 3D dBm-scale strip only when the floor just
    // updated above actually moved its rounded label set. See
    // updateDssScaleOverlayFreshness() for the full rationale.
    updateDssScaleOverlayFreshness();
#endif

    // 2026-05-25 perf fix: this block USED to force the ENTIRE GPU
    // overlay texture (freq scale, dBm strip, bandplan, time scale,
    // VFO marker, spots, waterfall chrome, peak hold trace, peak blobs,
    // NF text/line — ~16 paint ops + a full window-size QImage fill
    // and GPU texture upload) to rebuild on EVERY spectrum frame
    // whenever any of three features were enabled.  Rate-limited to
    // 10 Hz to keep CPU sane, which made blob decay / peak-hold drop
    // look chunky.
    //
    // 2026-05-26 KG4VCF first attempt: bumped 10 Hz -> 30 Hz to fix
    // chunky blob decay.  Bench: under heavy build load (parallel
    // ninja) the system became unusable -- the 30 Hz full-overlay
    // rebuild saturated the raster pool exactly as the earlier
    // measurement warned.  Reverted to 10 Hz here; the next commit
    // does the proper fix (static/dynamic layer split: chrome cached
    // on state change, dynamic overlays in a smaller spectrum-area
    // texture rebuilt every frame).
#ifdef NEREUS_GPU_SPECTRUM
    if (m_activePeakHold.enabled() || m_peakBlobs.enabled()
        || m_showNoiseFloor) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        // 2026-05-26 KG4VCF dual-layer overlay split: peak-hold trace
        // + peak blobs + noise-floor line/text live in their own GPU
        // texture (m_overlayDynamic).  We only mark *that* layer
        // dirty here -- chrome stays cached in m_overlayStatic and is
        // invalidated separately by setters that actually change
        // chrome state (band change, zoom, theme, etc.).  Rate-limit
        // raised to 30 Hz because the dynamic layer is dramatically
        // cheaper than the previous full-overlay rebuild (no chrome
        // paint ops, smaller GPU upload bandwidth).
        if (nowMs - m_overlayDynamicDirtyMs >= 33) {  // 30 Hz cap
            m_overlayDynamicDirty = true;
            m_overlayDynamicDirtyMs = nowMs;
        }
    }
#endif

}

// Feed new FFT frame -- single Thetis-faithful pipeline.
//
// Mirrors Thetis Display.cs:4970-5378 [v2.10.3.13] DrawPanadapterDX2D's
// data flow: WDSP analyzer pre-computes display-pixel data
// (current_display_data[i], indexed by pixel 0..nDecimatedWidth-1, sized
// W / m_nDecimation) and the per-pixel render loop iterates that array.
// Our split-pipeline equivalent: FFTEngine emits raw |X[k]|² linear bins;
// SpectrumWidget runs the WDSP detector + avenger ports
// (analyzer.c:283-462 / :464-554 [v2.10.3.13]) on a visible-bin slice
// and lands the result in m_renderedPixels (dBm, displayWidth).
// Spectrum and waterfall share the slice but run independent detector
// + avenger instances per WDSP per-plane model -- analyzer.c configures
// each ANALYZER_INFO[] entry with its own DetType + AvMode.
//
// All visible-window overlay consumers (legacy peak hold, ActivePeakHold,
// PeakBlobs, show-NF readout, cursor peak text) read m_renderedPixels.
// Full-band dBm chrome (ClarityController, NoiseFloorTracker) stay on
// fftReady (full-bin dBm) via independent connects in MainWindow --
// they're band-wide noise estimators by design and migrating them to
// visible-window pixels would lose information when the user zooms in.
void SpectrumWidget::updateSpectrumLinear(int receiverId,
                                          const QVector<float>& binsLinear,
                                          double windowEnb,
                                          double dbmOffset)
{
    m_localSpectrumSource = receiverId;
    if (m_remoteSpectrum || binsLinear.isEmpty()) { return; }

    // FFT size change detection.  When the FFTEngine replans (e.g. via
    // auto-zoom on bandwidth change), the per-pixel resolution shifts:
    // each output pixel now represents a different number of input bins.
    // The avenger's running per-pixel state becomes stale -- old smoothed
    // values reflect the OLD resolution and cross-blend with NEW values
    // for ~10 frames, manifesting as a "ghost smear" in the waterfall
    // and a visible step in the spectrum trace.  Clearing both avengers
    // here trades a brief 1-2 frame flicker of un-smoothed data for the
    // cleaner cross-resolution transition.
    if (!m_fullLinearBins.isEmpty()
        && m_fullLinearBins.size() != binsLinear.size()) {
        // Capture the last good trace BEFORE clearing the avenger.  The
        // first kReplanFadeFrames frames at the new resolution will blend
        // against this snapshot so the new trace dissolves in instead of
        // snapping into place.  No fade if m_renderedPixels was empty
        // (cold start — no prior frame to blend against).
        if (!m_renderedPixels.isEmpty()) {
            m_postReplanFrozenDb = m_renderedPixels;
            m_postReplanFrameCount = 0;
        } else {
            m_postReplanFrozenDb.clear();
        }
        m_spectrumReducer.clearAveraging();
        m_waterfallReducer.clearAveraging();
        m_extendedSpectrumAvenger.clear();
        m_extendedWaterfallAvenger.clear();
    }

    m_fullLinearBins = binsLinear;
    // Wideband calibration compares the survey bin bandwidth with the DDC
    // detector bandwidth, so it needs the ENBW from the same frame.
    m_fftWindowEnb = qMax(windowEnb, 1e-9);

    // Display pixel count -- spectrum panel width minus dBm strip column.
    // Per Thetis Display.cs:4970 DrawPanadapterDX2D(int W, ...) signature
    // and :4993 nDecimatedWidth = W / m_nDecimation [v2.10.3.13].  Thetis
    // S16 display-side decimation (an additional W / N reduction) is a
    // Phase 2 follow-up; for now we use the full panel width.
    //
    // R1 Task 5: this is now computed to FILL IN ReducerConfig::pixels
    // rather than being read from inside the reduction stage.  Local
    // rendering is unchanged -- same expression, same value -- but the
    // reducer no longer knows a widget exists, so nereusd can drive it
    // with a client-requested pixel count instead (design section 9.4a).
    const int displayWidth = qMax(width() - effectiveStripW(), 800);

    // Visible bin slice -- CTUN zoom support.  visibleBinRange() maps the
    // current m_centerHz +/- m_bandwidthHz/2 window against m_ddcCenterHz
    // + m_sampleRateHz.  When zoomed out, slice == full FFT.
    auto [firstBin, lastBin] = visibleBinRange(binsLinear.size());

    // Drop the DDC's filter skirt before anything is derived from the range.
    // The outermost bins sit in the decimation filter's transition band and
    // read near-nothing; at normal zoom they are off-screen, but the extended
    // pan draws the island at its true width and puts them on screen as a
    // dark band at each edge (bench 2026-08-08, "still black gaps between
    // wideband" and the DDC). Thetis discards the same 4% on its own display
    // (specHPSDR.cs:529-535 [v2.10.3.15]).
    //
    // Must happen before sliceCount, and it has to agree with the clipped
    // span listenableIslandPixels lays out, or the island's bins and pixels
    // would disagree and every signal in it would land at the wrong
    // frequency.
    if (m_extendedMode) {
        const int clip = static_cast<int>(
            std::floor(kDdcClipFraction * binsLinear.size()));
        firstBin = std::max(firstBin, clip);
        lastBin  = std::min(lastBin,
                            static_cast<int>(binsLinear.size()) - 1 - clip);
    }

    const int sliceCount = lastBin - firstBin + 1;

    // Extended pan (Sub-Epic F Task 8, finished 2026-08-08): the DDC only
    // covers part of the window once the operator zooms past its rate, so it
    // gets only the pixels it actually spans and the wideband ADC fills the
    // rest. visibleBinRange() clamps to the DDC's bin array, so before this
    // the DDC's spectrum was decimated across the FULL panel -- stretched,
    // and no longer aligned with the frequency scale under it.
    //
    // Outside extended mode listenableIslandPixels() returns the whole width,
    // which makes islandWidth == displayWidth and every line below identical
    // to what it was.

    const auto [islandFirstPx, islandLastPx] =
        listenableIslandPixels(displayWidth);
    const int islandWidth = islandLastPx - islandFirstPx + 1;

    // Pan far enough into a wing and the DDC leaves the window entirely.
    // That is a legitimate extended-view state, not an error: the wideband
    // plane still covers every pixel, so there is a full frame to draw and
    // returning here would freeze the survey on whatever was last rendered.
    // The clip above can also invert the bin range on its own once
    // visibleBinRange has clamped both ends to the same terminal bin.
    //
    // Outside extended mode an empty slice still means nothing to draw, and
    // that path returns exactly as it always did. Found by Codex on PR #318.
    const bool islandVisible = (sliceCount > 0) && (islandWidth > 0);
    if (!islandVisible && !m_extendedMode) { return; }

    const double pixPerBin = islandVisible
        ? static_cast<double>(islandWidth) / sliceCount
        : 1.0;
    const double binPerPix = (pixPerBin > 0.0) ? 1.0 / pixPerBin : 1.0;
    const double invEnb    = 1.0 / qMax(windowEnb, 1e-9);
    // dbmOffset folded into the avenger's power-domain scale so that
    // 10·log10(linear · scale) == 10·log10(linear) + dbmOffset, matching
    // FFTEngine.cpp:348 [v2.10.3.13] (binsDbm = 10·log10 + offset).
    const double dbmScale  = std::pow(10.0, dbmOffset / 10.0);
    const QVector<double> noCorrection;

    // 3DSS wide channel feed: m_fullLinearBins (this whole FFT frame,
    // unsliced) converted to dBm at full bin resolution, so
    // buildDssWideRow() can window bins outside visibleBinRange() using
    // the SAME calibration as the exact channel above (same dbmScale).
    // Deliberately skips detector reduction and avenger averaging -- both
    // are display-pixel/view-width concerns, and DssRenderer applies its
    // own independent temporal smoothing to whatever it receives (see
    // smoothDssRow() in DssRenderer.cpp), so a second averaging pass here
    // would just blur what that smoothing already handles. The conversion
    // itself mirrors SpectrumAvenger::apply()'s avMode==0 "no averaging"
    // case -- WDSP avenger() analyzer.c:464-554 [v2.10.3.13], case 0 at
    // analyzer.c:495-501: dbm = 10*log10(scale * linear) -- with the same
    // 1.0e-60 log floor SpectrumAvenger.cpp:65 uses. Gated on Mode3D:
    // nothing reads m_lastFullBinsDbm in 2D (pushDssRow() never runs).
    if (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) {
        m_lastFullBinsDbm.resize(m_fullLinearBins.size());
        for (int i = 0; i < m_fullLinearBins.size(); ++i) {
            m_lastFullBinsDbm[i] = static_cast<float>(
                10.0 * std::log10(dbmScale * m_fullLinearBins[i] + 1.0e-60));
        }
    }

    auto avengerMode = [](SpectrumAveraging m) -> int {
        // Wire-format integer codes per WDSP analyzer.c:464 [v2.10.3.13].
        switch (m) {
        case SpectrumAveraging::None:         return 0;
        case SpectrumAveraging::Recursive:    return 1;
        case SpectrumAveraging::TimeWindow:   return 2;
        case SpectrumAveraging::LogRecursive: return 3;
        default: return 0;
        }
    };

    // Crop window + stream geometry, identical for both planes.  These are
    // the four members the reducer used to read directly off the widget.
    NereusSDR::ReducerConfig cfg;
    cfg.pixels         = displayWidth;
    cfg.centreHz       = m_centerHz;
    cfg.spanHz         = m_bandwidthHz;
    cfg.streamCentreHz = m_ddcCenterHz;
    cfg.sampleRateHz   = m_sampleRateHz;

    // --- Spectrum plane: detector -> avenger -> m_renderedPixels (dBm) ---
    cfg.detector    = m_spectrumDetector;
    cfg.averageMode = avengerMode(m_spectrumAveraging);
    cfg.averageAlpha  = static_cast<double>(m_spectrumAverageAlpha);
    m_spectrumReducer.setConfig(cfg);
    if (!m_extendedMode) {
        // Keep the extracted reducer as the normal display path and as the
        // implementation shared with nereusd.
        m_spectrumReducer.reduce(binsLinear, windowEnb, dbmOffset,
                                 m_renderedPixels);
    } else {
        // Extended mode composes the DDC island and the wideband wings in
        // linear power before the common avenger runs.
        if (m_extendedSpectrumLinearPixels.size() != displayWidth) {
            m_extendedSpectrumLinearPixels.resize(displayWidth);
        }
        if (m_extendedSpectrumAvenger.numPixels() != displayWidth) {
            m_extendedSpectrumAvenger.resize(displayWidth);
        }
        if (islandVisible) {
            NereusSDR::applySpectrumDetector(
                m_spectrumDetector, sliceCount, islandWidth, pixPerBin,
                binPerPix, m_fullLinearBins.constData() + firstBin,
                m_extendedSpectrumLinearPixels.data() + islandFirstPx,
                invEnb, 0.0, static_cast<double>(sliceCount), 0.0);
        }
        fillWidebandWings(m_extendedSpectrumLinearPixels, islandFirstPx,
                          islandLastPx, dbmOffset, m_spectrumDetector);
        m_extendedSpectrumAvenger.apply(
            m_extendedSpectrumLinearPixels,
            avengerMode(m_spectrumAveraging),
            static_cast<double>(m_spectrumAverageAlpha), dbmScale,
            noCorrection, false, 0.0, m_renderedPixels);
    }

    // FFT-replan crossfade (Option A from the 2026-05-08 design).  For the
    // first kReplanFadeFrames after a resolution change, blend the
    // avenger's output with the frozen pre-replan trace using a linear
    // ramp so the new trace dissolves in instead of snapping.  The
    // avenger's own history rebuilds during these same frames; by the
    // time the fade finishes the smoothed values are real.
    if (!m_postReplanFrozenDb.isEmpty()
        && m_postReplanFrozenDb.size() == m_renderedPixels.size()
        && m_postReplanFrameCount < kReplanFadeFrames) {
        const float t = static_cast<float>(m_postReplanFrameCount + 1)
                      / static_cast<float>(kReplanFadeFrames);
        const float oneMinusT = 1.0f - t;
        for (int i = 0; i < m_renderedPixels.size(); ++i) {
            m_renderedPixels[i] = t * m_renderedPixels[i]
                                + oneMinusT * m_postReplanFrozenDb[i];
        }
        ++m_postReplanFrameCount;
        if (m_postReplanFrameCount >= kReplanFadeFrames) {
            m_postReplanFrozenDb.clear();  // free the buffer once done
        }
    }

    // Visual notch, spectrum plane.  Thetis dents in place right after the
    // analyzer hands the frame over and before its per-pixel render loop
    // (display.cs:5234-5238 [v2.10.3.15]), keeping one pristine copy that
    // only the noise-floor accumulator reads (display.cs:5259 [v2.10.3.15]).
    // Peak hold, the blob / IMD detector and the max readout deliberately see
    // the dent (display.cs:5269, :5280, :5337 [v2.10.3.15]) and so are still
    // fed from m_renderedPixels below.  Do NOT "protect" them: that would be
    // a divergence, not a fix.
    if (visualNotchWillDent()) {
        m_undentedPixels = m_renderedPixels;
        applyVisualNotchDent(m_renderedPixels);
    } else if (!m_undentedPixels.isEmpty()) {
        m_undentedPixels.clear();
    }

    // --- Waterfall plane: own detector + avenger -> m_wfRenderedPixels ---
    // Thetis runs separate analyzer planes for spectrum and waterfall (see
    // ANALYZER_INFO[] in analyzer.c).  DetType + AvMode are independent,
    // which is why this is a second reducer and not a second call on the
    // first: each carries its own avenger accumulators.
    cfg.detector    = m_waterfallDetector;
    cfg.averageMode = avengerMode(m_waterfallAveraging);
    cfg.averageAlpha  = static_cast<double>(m_waterfallAverageAlpha);
    m_waterfallReducer.setConfig(cfg);
    if (!m_extendedMode) {
        m_waterfallReducer.reduce(binsLinear, windowEnb, dbmOffset,
                                  m_wfRenderedPixels);
    } else {
        if (m_extendedWaterfallLinearPixels.size() != displayWidth) {
            m_extendedWaterfallLinearPixels.resize(displayWidth);
        }
        if (m_extendedWaterfallAvenger.numPixels() != displayWidth) {
            m_extendedWaterfallAvenger.resize(displayWidth);
        }
        if (islandVisible) {
            NereusSDR::applySpectrumDetector(
                m_waterfallDetector, sliceCount, islandWidth, pixPerBin,
                binPerPix, m_fullLinearBins.constData() + firstBin,
                m_extendedWaterfallLinearPixels.data() + islandFirstPx,
                invEnb, 0.0, static_cast<double>(sliceCount), 0.0);
        }
        fillWidebandWings(m_extendedWaterfallLinearPixels, islandFirstPx,
                          islandLastPx, dbmOffset, m_waterfallDetector);
        m_extendedWaterfallAvenger.apply(
            m_extendedWaterfallLinearPixels,
            avengerMode(m_waterfallAveraging),
            static_cast<double>(m_waterfallAverageAlpha), dbmScale,
            noCorrection, false, 0.0, m_wfRenderedPixels);
    }

    // Visual notch, waterfall plane.  Thetis re-runs modifyDataForNotches on
    // the waterfall array after `data = current_waterfall_data` (:6567), under
    // the same MOX gate - display.cs:6579-6586 [v2.10.3.15].  A second
    // explicit call here because NereusSDR keeps the waterfall pixels in
    // their own array, so denting the spectrum plane above does not reach
    // them.
    //
    // No undented waterfall copy: upstream needs one for its per-frame
    // waterfall minimum (dataCopy at display.cs:6741, :6833, :6915, :6954,
    // :7163, :7369, each carrying //[2.10.3]MW0LGE use non notched data), and
    // NereusSDR has no such tracker - m_wfLowThreshold is a persisted user
    // setting, and pushWaterfallRow is the only consumer of
    // m_wfRenderedPixels.
    if (visualNotchWillDent()) {
        applyVisualNotchDent(m_wfRenderedPixels);
    }

    updateReducedSpectrumOverlays();

    // Push display-pixel waterfall row -- waterfall AGC + NF-AGC +
    // threshold compute now operate on display pixels per Thetis
    // Display.cs:6713-6738 [v2.10.3.13] (waterfall_data[i] indexed by
    // pixel).
    //
    // 2026-05-25 KG4VCF bench fix: cache the latest row instead of
    // pushing immediately.  m_wfPushTimer (started in the ctor) drains
    // the cache at strictly m_wfUpdatePeriodMs cadence so network-burst
    // FFT delivery does not produce visible scroll stutter.
    //
    // 3M-5d: skip when the waterfall is being driven externally by
    // TxAnalyzer's pixout=1 stream (set by MainWindow on MOX-up).  This
    // prevents the WDSP's DetTypeWF + AverageModeWF pixels from being
    // overwritten by NereusSDR's own avenger output, and avoids a
    // double-push race.  RX path stays at false, no behaviour change.
    //
    // Revive merge: 3M-5d gated the direct pushWaterfallRow that used to
    // sit here; the gate now wraps the CACHE WRITE instead, which is the
    // same seam one step earlier. Gating the timer drain rather than the
    // write would have been wrong: the cache would keep the last RX row
    // and the drain would replay it under the TX rows.
    if (!m_txExternalWaterfall) {
        m_pendingWfPixelsDbm = m_wfRenderedPixels;
        m_pendingWfPixelsDbmDirty = true;
    }
    m_hasNewSpectrum = true;

    // 2026-05-22 bench fix: signal that a fresh m_renderedPixels frame is
    // available so MainWindow can push the slice's passband peak into the
    // MaxBin detector. See peakDbmInSlicePassband for the rationale.
    emit spectrumFrameRendered();
}

// 2026-05-22 bench fix: maxBin meter accuracy.
//
// Returns the strongest dBm pixel inside the active slice's IF passband,
// using m_renderedPixels (the post detector + avenger pipeline that the
// operator sees on the spectrum trace). MainWindow consumes this each
// render and pushes the value into WdspEngine's MaxBin detector so the
// analog S-meter reads what's visible on the spectrum.
//
// Hz range: [m_vfoHz + m_filterLowHz, m_vfoHz + m_filterHighHz]. For LSB
// the filter range is negative so the passband is below the VFO; for USB
// positive so the passband is above. Either way the absolute Hz bounds
// land inside the visible window when the operator is parked on a signal
// they can see.
//
// Pixel-to-Hz mapping: m_renderedPixels has displayWidth entries spanning
// [m_centerHz - m_bandwidthHz/2, m_centerHz + m_bandwidthHz/2]. Out-of-
// window pixel indices are clamped to the array bounds; if the passband
// is entirely outside the visible window the clamp degenerates and we
// return -400 sentinel.
double SpectrumWidget::peakDbmInSlicePassband() const
{
    return peakDbmInPassband(m_vfoHz + static_cast<double>(m_filterLowHz),
                             m_vfoHz + static_cast<double>(m_filterHighHz));
}

double SpectrumWidget::peakDbmInPassband(double loHz, double hiHz) const
{
    // Deliberate NereusSDR-specific divergence from the dent-in-place rule
    // (design section 8.3, decision recorded 2026-07-28): this feeds
    // WdspEngine's MaxBin detector and therefore the analog S-Meter. Thetis
    // reads MaxBin from WDSP upstream of its display code (console.cs:46959,
    // dsp.cs:849-850 [v2.10.3.15]), so its visual notch structurally cannot
    // move its meter; ours would if this scanned the dented array. A display
    // preference must not change a measurement.
    const QVector<float>& src = measurementPixels();
    const int n = src.size();
    if (n < 2 || m_bandwidthHz <= 0.0) { return -400.0; }

    if (!std::isfinite(loHz) || !std::isfinite(hiHz) || hiHz <= loHz) { return -400.0; }

    const double leftHz  = m_centerHz - m_bandwidthHz / 2.0;
    const double rightHz = m_centerHz + m_bandwidthHz / 2.0;
    if (hiHz < leftHz || loHz > rightHz) { return -400.0; }

    const double hzPerPx = m_bandwidthHz / static_cast<double>(n - 1);
    if (hzPerPx <= 0.0) { return -400.0; }

    auto hzToPx = [&](double hz) -> int {
        const double idx = (hz - leftHz) / hzPerPx;
        return qBound(0, static_cast<int>(std::round(idx)), n - 1);
    };
    const int firstPx = hzToPx(loHz);
    const int lastPx  = hzToPx(hiHz);
    if (lastPx < firstPx) { return -400.0; }

    float peak = -400.0f;
    for (int i = firstPx; i <= lastPx; ++i) {
        if (src[i] > peak) { peak = src[i]; }
    }
    return static_cast<double>(peak);
}

// ---------------------------------------------------------------------------
// Visual notch (trace dent) - design section 8.3
// ---------------------------------------------------------------------------
//
// Port of Thetis modifyDataForNotches (display.cs:4733-4817 [v2.10.3.15], the
// dent maths itself at :4790-4816) and the non-drawing arm of its
// handleNotches helper (display.cs:8677-8687 [v2.10.3.15]).  Thetis works in
// Hz-from-the-VFO and divides every pixel index by its display decimation;
// our pixel array is absolute-RF and undecimated (the m_nDecimation == 1 case,
// see updateSpectrumLinear), so a pixel index here IS Thetis's xPos and hzToX
// is the whole coordinate map.
//
// Two terms of the upstream maths are deliberately NOT ported, per section 8.3:
//
//   * handleNotches' localRit / CTUN offset (display.cs:8648-8652
//     [v2.10.3.15]).  It compensates for Thetis's VFO-label-anchored pixel
//     maths combined with its RIT-driven DDS retune.  Our x axis is absolute
//     RF, RIT never retunes the hardware, and WDSP applies shift to the
//     passband rather than to the notch, so adding it would displace every
//     dent by rit_hz.  Recorded so nobody re-adds it.
//   * cwSideToneShift (display.cs:8654 [v2.10.3.15]), dropped entirely rather
//     than threaded as a constant zero: a notch added at F in CW stores at
//     exactly F here (design section 1.2).

void SpectrumWidget::setVisualNotchEnabled(bool on)
{
    if (m_visualNotchEnabled == on) { return; }
    m_visualNotchEnabled = on;
    markOverlayDirty();
}

// From Thetis display.cs:5235 [v2.10.3.15]:
//   if (bDoVisualNotch && m_bShowVisualNotch && !local_mox)
// plus the _tnf_active half of the per-notch _Use flag at display.cs:8686
// [v2.10.3.15].  The per-notch Active half is applied inside the loop below,
// exactly as upstream skips on !nc._Use.
bool SpectrumWidget::visualNotchWillDent() const
{
    return m_visualNotchEnabled
        && !m_moxOverlay
        && m_notchGlobalEnabled
        && !m_notchMarkers.isEmpty();
}

void SpectrumWidget::applyVisualNotchDent(QVector<float>& pixels) const
{
    const int n = pixels.size();
    if (n <= 0 || m_bandwidthHz <= 0.0) { return; }

    const float fAttenuation = kNotchDentAttenuationDb;

    // A rect the width of the array, so hzToX maps onto the same columns the
    // trace is drawn from regardless of the widget's live geometry.
    const QRect r(0, 0, n, 1);

    for (const NotchMarker& nc : m_notchMarkers) {
        if (!nc.active) { continue; } // skip inactive

        // From Thetis display.cs:8679-8680 [v2.10.3.15]:
        //   double dNewWidth = n.FWidth < min_notch_wdith ? min_notch_wdith : n.FWidth; // use the min width of filter from WDSP
        //   dNewWidth += 20; // fudge factor to align better with spectrum notch
        const double dNewWidth =
            ((nc.widthHz < m_notchMinWidthHz) ? m_notchMinWidthHz : nc.widthHz)
            + kNotchDentFudgeHz;

        const double centreHz = nc.freqMhz * 1e6;
        const int cX     = hzToX(centreHz, r);
        const int leftX  = hzToX(centreHz - dNewWidth / 2.0, r);
        const int rightX = hzToX(centreHz + dNewWidth / 2.0, r);

        // do left
        int wL = cX - leftX;
        wL = qMax(1, wL);
        for (int i = cX; i > cX - wL; --i) {
            if (i < 0 || i > n - 1) { continue; }
            const int x = cX - i;
            const float fTmp = 1.0f / static_cast<float>(std::pow(
                static_cast<double>(wL) / static_cast<double>(wL - x),
                1.5)); // pow2 quite sharp
            pixels[i] -= (fAttenuation * fTmp);
        }
        // do right
        int wR = rightX - cX;
        wR = qMax(1, wR);
        for (int i = cX; i < cX + wR; ++i) {
            if (i < 0 || i > n - 1) { continue; }
            const int x = i - cX;
            const float fTmp = 1.0f / static_cast<float>(std::pow(
                static_cast<double>(wR) / static_cast<double>(wR - x),
                1.5)); // pow2 quite sharp
            pixels[i] -= (fAttenuation * fTmp);
        }
    }
}

const QVector<float>& SpectrumWidget::measurementPixels() const
{
    return (m_undentedPixels.size() == m_renderedPixels.size())
               ? m_undentedPixels
               : m_renderedPixels;
}

SpectrumWidget::SpectrumLayout SpectrumWidget::spectrumLayout() const
{
    // Preserve the GPU panel ordering and split: fixed divider/frequency
    // chrome is reserved before dividing the remaining spectrum/waterfall
    // height. Both painters and their interaction geometry use these rects.
    const int contentH = std::max(0, height() - kDividerH - kFreqScaleH);
    const int specH = static_cast<int>(contentH * m_spectrumFrac);
    const int plotW = width() - effectiveStripW();
    const int freqY = specH + kDividerH;
    const int wfY = freqY + kFreqScaleH;
    return {contentH,
            QRect(0, 0, plotW, specH),
            QRect(0, specH, width(), kDividerH),
            QRect(0, freqY, plotW, kFreqScaleH),
            QRect(0, wfY, plotW, contentH - specH),
            QRect(0, wfY, width(), contentH - specH)};
}

void SpectrumWidget::resizeEvent(QResizeEvent* event)
{
    SpectrumBaseClass::resizeEvent(event);

    // Keep mouse overlay covering entire widget
    if (m_mouseOverlay) {
        m_mouseOverlay->setGeometry(0, 0, width(), height());
        m_mouseOverlay->raise();
    }

    // Keep disconnect label sized to the widget; raise so QRhi surface
    // doesn't paint over it on the next frame.
    if (m_disconnectLabel) {
        m_disconnectLabel->setGeometry(0, 0, width(), height());
        if (m_disconnectLabel->isVisible()) {
            m_disconnectLabel->raise();
        }
    }

    // Recreate waterfall image at new size
    const SpectrumLayout layout = spectrumLayout();
    const int wfW = layout.waterfall.width();
    const int wfH = layout.waterfall.height();
    if (wfW > 0 && wfH > 0 && (m_waterfall.isNull() ||
        m_waterfall.width() != wfW || m_waterfall.height() != wfH)) {
        traceRxHistoryEvent(RxHistoryEvent::LiveImageResize, -1, 0, QSize(wfW, wfH));
        // 2026-05-26 KG4VCF: unlock the previous waterfall before
        // QImage replacement frees it.  Aligned no-op when m_waterfall
        // was null.
        if (!m_waterfall.isNull()) {
            unlockMemory(m_waterfall.constBits(),
                         m_waterfall.sizeInBytes());
        }
        m_waterfall = QImage(wfW, wfH, QImage::Format_RGB32);
        m_waterfall.fill(QColor(0x0f, 0x0f, 0x1a));
        // Pin the new waterfall buffer.  Touched every push (write
        // one row's worth of pixels) and every paint (full incremental
        // texture upload to GPU); compression stalls here cause the
        // visible waterfall stutter under build load.
        lockMemory(m_waterfall.constBits(),
                   m_waterfall.sizeInBytes(),
                   "SpectrumWidget::m_waterfall");
        m_wfWriteRow = 0;
#ifdef NEREUS_GPU_SPECTRUM
        m_wfTexFullUpload = true;
        markOverlayDirty();
#endif
        // Sub-epic E: schedule history-image rebuild so the ring buffer's
        // QImage tracks m_waterfall's new dimensions. Debounced 250 ms so
        // rapid resize events don't thrash. Matches the intent of unmerged
        // AetherSDR PR #1478 [@2bb3b5c] — see plan §authoring-time #2.
        if (m_historyResizeTimer) {
            m_historyResizeTimer->start();
        }
    }

    // Reposition VFO flags after resize
    updateVfoPositions();
}

void SpectrumWidget::paintEvent(QPaintEvent* event)
{
#ifdef NEREUS_GPU_SPECTRUM
    // GPU mode: render() handles everything via QRhi.
    // Do NOT use QPainter on QRhiWidget — it doesn't support paintEngine.
    SpectrumBaseClass::paintEvent(event);
    return;
#endif
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const SpectrumLayout layout = spectrumLayout();
    const int specH = layout.spectrum.height();

    // Spectrum area (left of dBm strip — strip lives on the right edge per
    // AetherSDR convention). From AetherSDR SpectrumWidget.cpp:4858 [@0cd4559]
    const QRect specRect = layout.spectrum;
    // Waterfall area
    const QRect wfRect = layout.waterfall;
    // Frequency scale bar
    const QRect freqRect = layout.frequency;

    // Draw divider bar between spectrum and waterfall
    p.fillRect(layout.divider, QColor(0x30, 0x40, 0x50));

    // Draw components
    drawGrid(p, specRect);
    drawSpectrum(p, specRect);
    drawWaterfall(p, wfRect);
    // TNF notch overlay immediately before the spots, matching upstream's
    // paint order: AetherSDR src/gui/SpectrumWidget.cpp:12903-12904
    // [@c6481cbf] draws drawTnfMarkers then drawSpotMarkers.  No visibility
    // gate: an empty marker list IS the off state, and the master TNF flag
    // recolours the markers rather than hiding them (Thetis
    // display.cs:8704-8707 [v2.10.3.15]).
    //
    // notchSpecRect() and the local specRect both use spectrumLayout(),
    // so the hit-test and painted marker rectangles agree.
    drawNotchMarkers(p, notchSpecRect());
    // Phase 3J-2 Task E1: spot overlay between spectrum/waterfall and the
    // VFO marker so the spot label tick + pill sit on top of the trace
    // but below the slice/VFO marker chrome. Mirrors AetherSDR
    // SpectrumWidget.cpp:3787 [@0cd4559] paint ordering
    // (drawSpotMarkers between drawTnfMarkers and drawSliceMarkers).
    if (m_showSpots) {
        drawSpotMarkers(p, specRect);
    }
    drawVfoMarker(p, specRect, wfRect);
    drawOffScreenIndicator(p, specRect, wfRect);

    // Plan 4 D9 (Cluster E) + follow-up (option A): TX filter overlay on
    // panadapter, MOX-gated.  Pairs with the !m_moxOverlay gate on the RX
    // passband fill in drawVfoMarker — the panadapter shows EITHER the
    // cyan RX shadow (RX state) OR the orange TX band (MOX/TUNE state),
    // never both.  m_txFilterVisible is the user toggle (Setup → Display);
    // m_moxOverlay is the live TX-state gate.
    if (m_txFilterVisible && m_moxOverlay) {
        drawTxFilterOverlay(p, specRect);
    }

    // Phase 3M-4 Task 12 — two-tone IMD overlay show condition.
    // From Thetis display.cs:5008 [v2.10.3.13]:
    //   show_imd_measurements = local_mox && _testing_imd
    //                           && _show_imd_measurements && displayduplex;
    if (m_moxOverlay && m_testingIMD && m_showIMDMeasurements && m_displayDuplex) {
        drawImdOverlay(p, specRect);
    }

    drawFreqScale(p, freqRect);
    if (m_dbmScaleVisible) {
        // drawDbmScale needs the FULL-WIDTH spectrum-vertical rect so the strip
        // lands in the reserved right-edge zone at x=[w-kDbmStripW..w-1].
        // Passing the clipped specRect would put the strip INSIDE the spectrum.
        // dBm strip: 2D draws a linear dBm axis; 3D maps the ticks onto the front
        // (live) trace's ridge band. Same strip chrome and click targets either way.
        // From AetherSDR SpectrumWidget.cpp:15122-15128 [@1872028c]
        if (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) {
            drawDbmScale3D(p, QRect(0, 0, w, specH), dssFloorDbm());
        } else {
            drawDbmScale(p, QRect(0, 0, w, specH));
        }
    }
    drawBandPlan(p, specRect);
    // Sub-epic E: time-scale + LIVE button on the right edge of the
    // waterfall area (always painted; widens automatically when paused).
    // Use a full-width wfRect (not the clipped `wfRect` at line 1141) so the
    // strip lands in the same right-edge column as the dBm scale strip.
    const QRect wfRectFull = layout.waterfallChrome;
    drawTimeScale(p, wfRectFull);
    // B8 Task 21: guard cursor frequency readout by m_showCursorFreq.
    if (m_showCursorFreq) {
        drawCursorInfo(p, specRect);
    }

    // FPS overlay (Phase 3G-8 commit 5 / G8 ShowFPS). Cheap rolling counter
    // updated once per second. QPainter fallback path only; GPU path prints
    // its own counter via a future commit if the feature is exposed there.
    if (m_showFps) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        m_fpsFrameCount++;
        if (m_fpsLastUpdateMs == 0) {
            m_fpsLastUpdateMs = nowMs;
        } else if (nowMs - m_fpsLastUpdateMs >= 1000) {
            const double elapsed = (nowMs - m_fpsLastUpdateMs) / 1000.0;
            m_fpsDisplayValue    = static_cast<float>(m_fpsFrameCount / elapsed);
            m_fpsFrameCount      = 0;
            m_fpsLastUpdateMs    = nowMs;
        }
        const QString fpsText = QStringLiteral("%1 fps")
                                    .arg(m_fpsDisplayValue, 0, 'f', 1);
        QFont ff = p.font();
        ff.setPixelSize(11);
        p.setFont(ff);
        p.setPen(m_gridTextColor);
        const int tw = p.fontMetrics().horizontalAdvance(fpsText);
        p.drawText(specRect.right() - tw - 8, specRect.top() + 14, fpsText);
    }

    // ---- Task 2.3: spectrum text overlays ----

    // ShowBinWidth — render bin width in corner.
    // From Thetis setup.cs:7061 [v2.10.3.13] lblDisplayBinWidth.
    if (m_showBinWidth) {
        const double bw = binWidthHz();
        if (bw > 0.0) {
            const QString bwText = QString::number(bw, 'f', 3)
                                 + QStringLiteral(" Hz/bin");
            drawTextOverlay(p, specRect, OverlayPosition::BottomRight,
                            bwText, m_gridTextColor);
        }
    }

    // ShowNoiseFloor -- render NF horizontal line + corner text overlay.
    // Both visuals share m_showNoiseFloor in NereusSDR; Thetis splits this
    // into ShowRX1NoiseFloor (line) + m_bShowNoiseFloorDBM (text), but
    // they're typically toggled together and a single flag matches the
    // existing Setup → Display → Spectrum Defaults checkbox.
    paintNoiseFloorOverlay(p, specRect);

    // ShowPeakValueOverlay — render cached peak text (refreshed by timer).
    // From Thetis console.cs:20073 PeakTextDelay=500ms [v2.10.3.13].
    // Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
    if (m_showPeakValueOverlay && !m_peakTextCache.isEmpty()) {
        drawTextOverlay(p, specRect, m_peakValuePosition,
                        m_peakTextCache, m_peakValueColor);
    }

    // HIGH SWR / PA safety overlay — painted last so it sits on top of all
    // other chrome. From Thetis display.cs:4183-4201 [v2.10.3.13].
    paintHighSwrOverlay(p);

    // MOX / TX overlay — 3 px red border drawn when transmitting.
    // From Thetis display.cs:1569-1593 [v2.10.3.13] Display.MOX setter.
    // Phase 3M-1a H.1.
    paintMoxOverlay(p);

    // Phase 3Q-8: disconnect overlay is now a child QLabel (m_disconnectLabel)
    // so it composites in both CPU and GPU paint paths. Show/hide handled in
    // setConnectionState; geometry tracked in resizeEvent.

    // Reposition VFO flag widgets every frame — ensures flag tracks marker
    // exactly with no frame delay. From AetherSDR: updatePosition called
    // from within the paint/render cycle.
    updateVfoPositions();
}

// ---- Grid drawing ----
// Adapted from Thetis display.cs grid colors:
//   grid_color = Color.FromArgb(65, 255, 255, 255)  — display.cs:2069
//   hgrid_color = Color.White — display.cs:2102
//   grid_text_color = Color.Yellow — display.cs:2003
void SpectrumWidget::drawGrid(QPainter& p, const QRect& specRect)
{
    // Phase 3G-8 commit 5: honours m_gridEnabled plus configurable grid
    // colours (m_hGridColor / m_gridColor / m_gridFineColor).
    if (!m_gridEnabled) {
        return;
    }

    // Horizontal dBm grid lines (major).
    p.setPen(QPen(m_hGridColor, 1));

    float bottom = m_refLevel - m_dynamicRange;
    float step = 10.0f;  // 10 dB steps
    if (m_dynamicRange <= 50.0f) {
        step = 5.0f;
    }

    for (float dbm = bottom + step; dbm < m_refLevel; dbm += step) {
        int y = dbmToY(dbm, specRect);
        p.drawLine(specRect.left(), y, specRect.right(), y);
    }

    // Vertical frequency grid lines (major).
    p.setPen(QPen(m_gridColor, 1));

    // Compute a nice frequency step
    double freqStep = 10000.0;  // 10 kHz default
    if (m_bandwidthHz > 500000.0) {
        freqStep = 50000.0;
    } else if (m_bandwidthHz > 100000.0) {
        freqStep = 25000.0;
    } else if (m_bandwidthHz < 50000.0) {
        freqStep = 5000.0;
    }

    double startFreq = std::ceil((m_centerHz - m_bandwidthHz / 2.0) / freqStep) * freqStep;
    for (double f = startFreq; f < m_centerHz + m_bandwidthHz / 2.0; f += freqStep) {
        int x = hzToX(f, specRect);
        p.drawLine(x, specRect.top(), x, specRect.bottom());
    }

    // Fine (minor) vertical grid at 1/5 step. Drawn in m_gridFineColor.
    p.setPen(QPen(m_gridFineColor, 1, Qt::DotLine));
    const double fineStep = freqStep / 5.0;
    double startFine = std::ceil((m_centerHz - m_bandwidthHz / 2.0) / fineStep) * fineStep;
    for (double f = startFine; f < m_centerHz + m_bandwidthHz / 2.0; f += fineStep) {
        int x = hzToX(f, specRect);
        p.drawLine(x, specRect.top(), x, specRect.bottom());
    }

    drawExtendedIslandBounds(p, specRect);
}

// Phase 3F Sub-Epic F Task 9, finished 2026-08-08: mark where the DDC's
// listenable island ends and the wideband wings begin.
//
// Without the markers the two data sources are indistinguishable on screen,
// and they are not interchangeable: inside the island the operator can hear
// what they are looking at and a click retunes the slice; outside it the
// trace is survey data off the wideband ADC and a click retunes the whole
// DDC (mousePressEvent, the Task 12 branch). A boundary the operator cannot
// see is a boundary they cross by accident.
//
// Drawn from drawGrid so it lands in the same cached static-overlay texture
// as the rest of the chrome -- the island edges only move when the pan is
// retuned or zoomed, which already invalidates that cache.
void SpectrumWidget::drawExtendedIslandBounds(QPainter& p, const QRect& specRect)
{
    if (!m_extendedMode || m_sampleRateHz <= 0.0 || m_bandwidthHz <= 0.0) {
        return;
    }

    // The CLIPPED edges, which is where the island actually stops. Drawing
    // the markers at the raw rate/2 put them 4% of the DDC rate out into the
    // wings, so they marked a boundary that is not there.
    const double ddcLowHz  = m_ddcCenterHz - ddcIslandHalfSpanHz();
    const double ddcHighHz = m_ddcCenterHz + ddcIslandHalfSpanHz();
    const double windowLow  = m_centerHz - m_bandwidthHz / 2.0;
    const double windowHigh = m_centerHz + m_bandwidthHz / 2.0;

    p.setPen(QPen(m_gridColor.lighter(160), 1, Qt::DashLine));
    for (const double edgeHz : {ddcLowHz, ddcHighHz}) {
        // An edge outside the window has no boundary to draw: the island
        // runs off that side of the panel.
        if (edgeHz < windowLow || edgeHz > windowHigh) {
            continue;
        }
        const int x = hzToX(edgeHz, specRect);
        p.drawLine(x, specRect.top(), x, specRect.bottom());
    }
}

// ---- Spectrum trace drawing ----
// Phase 3G-8 commit 3: honors m_lineWidth, m_gradientEnabled,
// m_peakHoldEnabled. See drawSpectrum() call site in paintEvent for
// the QPainter fallback path. GPU path uses its own vertex generation
// in renderGpuFrame() — line width and gradient wire-up there lands
// in commit 5.
void SpectrumWidget::drawSpectrum(QPainter& p, const QRect& specRect)
{
    // Task 10: 3DSS CPU-rendered surface replaces the classic FFT trace
    // entirely in 3D mode. This is the ONLY 3DSS surface a
    // -DNEREUS_GPU_SPECTRUM=OFF build ever draws -- that configuration never
    // reaches renderGpuFrame() (the whole function is compiled out), so this
    // early return is not merely a mesh-unavailable fallback here, it is the
    // sole rendering path for CPU-only builds. Skips the grid entirely, like
    // upstream: the opaque plot region (scaleStripPx 0, so no transparent
    // strip) would only paint over it anyway, and the earlier drawGrid(p,
    // specRect) call in paintEvent has already returned by the time this
    // runs, so nothing is wasted by leaving that call site alone.
    // From AetherSDR SpectrumWidget.cpp:14915-14933 [@1872028c] (paintEvent's
    // is3D branch).
    // Spectrum region: the 3DSS surface, or the classic bg + grid + FFT trace.
    // 3DSS replaces ONLY the spectrum trace — the divider, freq scale, waterfall,
    // overlays, and scales below run identically in both modes.
    if (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) {
        // Cap the software surface (like the GPU path) so a HiDPI/maximized
        // window doesn't rebuild a multi-megapixel QImage every frame; the
        // surface is intrinsically low-res, so stretch it on draw.
        const QImage& surf = buildDssImage(
            specRect.size().boundedTo(QSize(kDssFallbackMaxW, kDssFallbackMaxH)),
            0);
        p.fillRect(specRect, QColor(0x0a, 0x0a, 0x14));
        if (!surf.isNull()) {
            p.drawImage(specRect, surf);
        }
        return;
    }

    // Display-pixel iteration -- mirrors Thetis Display.cs:5249-5378
    // [v2.10.3.13] which loops `for (int i = 0; i < nDecimatedWidth; i++)`
    // over the post-analyzer current_display_data[] array.  Our
    // m_renderedPixels is the equivalent (post detector + avenger), sized
    // to displayWidth.
    const int n = m_renderedPixels.size();
    if (n < 2) {
        return;
    }

    const float xStep = static_cast<float>(specRect.width())
                      / static_cast<float>(n - 1);

    // Build polyline across display pixels.
    // 2026-05-26 KG4VCF perf polish: use member scratch vectors
    // (m_specPointsScratch / m_specPeakPointsScratch / m_specFillPathScratch
    // / m_specPeakPathScratch) so the per-paint allocations don't
    // churn the heap.  QVector::resize is a no-op when n hasn't
    // changed; QPainterPath::clear keeps the internal element buffer.
    QVector<QPointF>& points = m_specPointsScratch;
    points.resize(n);
    for (int j = 0; j < n; ++j) {
        const float x = specRect.left() + static_cast<float>(j) * xStep;
        const float y = dbmToYf(m_renderedPixels[j], specRect);
        points[j] = QPointF(x, y);
    }

    // Fill under the trace (if enabled).
    // From AetherSDR: fill alpha 0.70, cyan color.
    if (m_panFill) {
        QPainterPath& fillPath = m_specFillPathScratch;
        fillPath.clear();
        fillPath.moveTo(points.first().x(), specRect.bottom());
        for (const QPointF& pt : points) {
            fillPath.lineTo(pt);
        }
        fillPath.lineTo(points.last().x(), specRect.bottom());
        fillPath.closeSubpath();

        if (m_gradientEnabled) {
            QLinearGradient grad(QPointF(0, specRect.top()),
                                 QPointF(0, specRect.bottom()));
            QColor topCol = m_fillColor;
            topCol.setAlphaF(qBound(0.0f, m_fillAlpha, 1.0f));
            QColor botCol = m_fillColor;
            botCol.setAlphaF(0.0f);
            grad.setColorAt(0.0, topCol);
            grad.setColorAt(1.0, botCol);
            p.fillPath(fillPath, QBrush(grad));
        } else {
            QColor fill = m_fillColor;
            fill.setAlphaF(m_fillAlpha * 0.4f);
            p.fillPath(fillPath, fill);
        }
    }

    // Legacy per-pixel peak hold trace, drawn underneath the live trace.
    if (m_peakHoldEnabled && m_pxPeakHold.size() == n) {
        QVector<QPointF>& peakPoints = m_specPeakPointsScratch;
        peakPoints.resize(n);
        for (int j = 0; j < n; ++j) {
            const float x = specRect.left() + static_cast<float>(j) * xStep;
            const float y = dbmToYf(m_pxPeakHold[j], specRect);
            peakPoints[j] = QPointF(x, y);
        }
        QColor peakCol = m_fillColor;
        peakCol.setAlphaF(0.55f);
        QPen peakPen(peakCol, qMax(1.0f, m_lineWidth * 0.75f));
        peakPen.setStyle(Qt::DotLine);
        p.setPen(peakPen);
        p.drawPolyline(peakPoints.data(), n);
    }

    // Draw live trace line.
    // From Thetis Display.cs:5376 [v2.10.3.13] DrawLine(previousPoint, point, lineBrush, ...)
    // -- a polyline through display-pixel points.  We use m_fillColor for
    // consistency with AetherSDR style.
    QPen tracePen(m_fillColor, m_lineWidth);
    p.setPen(tracePen);
    p.drawPolyline(points.data(), n);

    // Active Peak Hold trace -- separate pass per Q14.1.  Drawn AFTER
    // the live trace so the peak line sits on top.  From Thetis
    // Display.cs:5341 [v2.10.3.13] -- per-pixel peak.max_dBm, y mapped
    // via dbmToPixel.
    // Thetis display.cs:5011 [v2.10.3.15]: while this pan transmits
    // without "Also in TX" the trace is not drawn (bSpectralPeakHold).
    if (m_activePeakHold.active() && m_activePeakHold.size() == n) {
        paintActivePeakHoldTrace(p, specRect);
    }

    // Peak Blobs render pass.  Drawn on top of the live trace line.
    // From Thetis Display.cs:5453-5508 [v2.10.3.13].
    if (m_peakBlobs.enabled() && !m_peakBlobs.displayDelayed()
        && !m_peakBlobs.blobs().isEmpty()) {
        paintPeakBlobs(p, specRect);
    }

    // Zero line (0 dBm) — Phase 3G-8 commit 5 (G7).
    // Plan 4 D9c-1: uses m_rxZeroLineColor (was m_zeroLineColor).
    if (m_showZeroLine) {
        const int zy = dbmToY(0.0f, specRect);
        if (zy >= specRect.top() && zy <= specRect.bottom()) {
            QPen zeroPen(m_rxZeroLineColor, 1, Qt::DashLine);
            p.setPen(zeroPen);
            p.drawLine(specRect.left(), zy, specRect.right(), zy);
        }
    }

    // TX zero line on panadapter — Plan 4 D9c-1.  MOX-gated so the TX
    // centre-frequency marker only appears when transmitting.
    if (m_moxOverlay && m_vfoHz > 0.0) {
        const int tx_x = hzToX(m_vfoHz, specRect);
        if (tx_x >= specRect.left() && tx_x <= specRect.right()) {
            QPen txZeroPen(m_txZeroLineColor, 1, Qt::DashLine);
            p.setPen(txZeroPen);
            p.drawLine(tx_x, specRect.top(), tx_x, specRect.bottom());
        }
    }
}

// ---- Active Peak Hold trace render pass (Q14.1 separate pass) ----
// Iterates the per-display-pixel peak array (m_activePeakHold sized to
// m_renderedPixels.size()).  Mirrors Thetis Display.cs:5341 [v2.10.3.13]
// which renders spectralPeaks[i] (i = display pixel) through the same
// dbmToPixel y-mapping as the live trace.  Optional fill shades the
// region between the peak trace and the current live trace.
void SpectrumWidget::paintActivePeakHoldTrace(QPainter& p, const QRect& specRect)
{
    const int n = m_renderedPixels.size();
    if (n < 2 || m_activePeakHold.size() != n) {
        return;
    }

    const float xStep = static_cast<float>(specRect.width())
                      / static_cast<float>(n - 1);

    // Build polyline for the peak trace (same x mapping as main trace).
    // 2026-05-26 KG4VCF perf polish: reuse member-cached scratch.
    QVector<QPointF>& peakPoints = m_specPeakPointsScratch;
    peakPoints.resize(n);
    for (int j = 0; j < n; ++j) {
        const float x = specRect.left() + static_cast<float>(j) * xStep;
        float peakDbm = m_activePeakHold.peak(j);
        // Clamp -inf to display bottom so the fill path closes cleanly.
        if (!std::isfinite(peakDbm)) {
            peakDbm = m_refLevel - m_dynamicRange;
        }
        const float y = dbmToYf(peakDbm, specRect);
        peakPoints[j] = QPointF(x, y);
    }

    // Optional fill between peak trace and current live trace.
    if (m_activePeakHold.fill()) {
        QPainterPath& fillPath = m_specPeakPathScratch;
        fillPath.clear();
        fillPath.moveTo(peakPoints.first());
        for (int j = 1; j < n; ++j) {
            fillPath.lineTo(peakPoints[j]);
        }
        for (int j = n - 1; j >= 0; --j) {
            const float x = specRect.left() + static_cast<float>(j) * xStep;
            const float y = dbmToYf(m_renderedPixels[j], specRect);
            fillPath.lineTo(QPointF(x, y));
        }
        fillPath.closeSubpath();

        QColor fillCol = m_activePeakHoldColor;
        fillCol.setAlphaF(0.18f);
        p.fillPath(fillPath, fillCol);
    }

    // Draw the peak trace line in its own colour so it stays visible
    // against the live data-line trace (which inherits m_fillColor).
    QPen peakPen(m_activePeakHoldColor, qMax(1.0f, m_lineWidth));
    peakPen.setStyle(Qt::DashLine);
    p.setPen(peakPen);
    p.drawPolyline(peakPoints.data(), n);
}

// ---- Peak Blobs render pass (Task 2.6) ----
// Ported from Thetis Display.cs:5507-5508 [v2.10.3.13] -- ellipse + text
// labels at each local-maximum blob position.  Called from drawSpectrum()
// after the live trace line so blobs sit on top of all other spectrum
// content.  Blob.binIndex is now a display-pixel index 0..n-1 (post
// pipeline migration), not a raw FFT bin -- pixel-to-x via direct
// proportional mapping across specRect.
void SpectrumWidget::paintPeakBlobs(QPainter& p, const QRect& specRect)
{
    const int n = m_renderedPixels.size();
    if (n < 2) {
        return;
    }

    const float xStep = static_cast<float>(specRect.width())
                      / static_cast<float>(n - 1);

    p.setRenderHint(QPainter::Antialiasing, true);

    // 2026-05-25 KG4VCF bench fix: PR #286's QStaticText cache for the
    // dBm and frequency scale labels stopped calling QPainter::setFont
    // (drawStaticText carries its own font); the painter is left at
    // whatever the previous section set, which under the old code was
    // 11 px from paintDbmScale's drawText calls.  Post-cache the
    // painter inherits Qt's default ~9 pt fallback, so the blob dBm
    // labels rendered visibly smaller than they used to.  Set the
    // canonical 11 px explicitly here.
    QFont blobFont = p.font();
    blobFont.setPixelSize(11);
    p.setFont(blobFont);

    for (const auto& blob : m_peakBlobs.blobs()) {
        // Skip disabled slots in the persistent blob array (post-Phase 2
        // PeakBlobDetector rewrite mirrors Thetis's fixed-size m_RX1Maximums
        // array with an Enabled flag rather than rebuilding the vector
        // every frame).
        if (!blob.enabled) {
            continue;
        }
        // Guard: only render blobs whose pixel falls within the array.
        if (blob.binIndex < 0 || blob.binIndex >= n) {
            continue;
        }

        const int x = static_cast<int>(specRect.left()
                       + static_cast<float>(blob.binIndex) * xStep);

        // dBm value -> pixel Y.
        // From Thetis Display.cs:5507 [v2.10.3.13] -- position at the peak dBm.
        const int y = dbmToY(blob.max_dBm, specRect);

        // Draw ellipse — From Thetis display.cs:5507 [v2.10.3.13]
        //   _d2dRenderTarget.DrawEllipse(m_objEllipse, m_bDX2_PeakBlob);
        // where m_objEllipse is `new SharpDX.Direct2D1.Ellipse(Vector2.Zero, 5f, 5f)`
        // (radius 5 DIPs) and DrawEllipse defaults to 1px stroke.
        //
        // NereusSDR uses radius 3 (not 5) because Qt6/QPainter on HiDPI
        // (Retina/2x) scales drawEllipse by devicePixelRatio, while Direct2D
        // on Thetis's typical Windows 1x-DPI display does not. Radius 3 in
        // logical pixels yields a visual size matching Thetis's 5-radius
        // physical-pixel render on a 1x display.
        //
        // 2026-05-26 KG4VCF perf polish: drawEllipse replaced with
        // drawPixmap(pre-rendered blob marker) to skip Qt's parallel
        // raster span-blend path.  Lazy first-build: rebuild the
        // pixmap if it's null (e.g. when peakBlobs gets enabled
        // before any color setter fires).  Subsequent calls hit the
        // cached pixmap.
        if (m_blobMarkerPixmap.isNull()) {
            rebuildBlobMarkerPixmap();
        }
        // Pixmap is 8x8 logical; center is (4,4).  Draw at top-left so
        // the center lands at (x, y).
        p.drawPixmap(x - 4, y - 4, m_blobMarkerPixmap);

        // Draw dBm text label — From Thetis display.cs:5508 [v2.10.3.13]
        //   _d2dRenderTarget.DrawText(..., new RectangleF(
        //       m_objEllipse.Point.X + 6, m_objEllipse.Point.Y - 8, ...));
        p.setPen(m_peakBlobTextColor);
        p.drawText(x + 5, y - 6, QString::number(static_cast<double>(blob.max_dBm), 'f', 1));
    }

    p.setRenderHint(QPainter::Antialiasing, false);
}

// Noise-floor overlay (8×8 box + horizontal dashed line + dBm text).
// Source-first port of Thetis display.cs:5423-5448 [v2.10.3.13]:
//   Rectangle nf_box = new Rectangle(40, 0, 8, 8);            // ctor:5219
//   ...
//   int yP = (int)yPixelLerp;
//   nf_box.Y = yP - 8;
//   drawFillRectangleDX2D(nf_colour, nf_box);                              // 5437
//   drawLineDX2D(nf_colour, 40, yP, W - 40, yP, m_styleDots,               // 5438
//                m_fNoiseFloorLineWidth);                  // horiz line
//   if (m_bShowNoiseFloorDBM) {
//       drawStringDX2D(lerp.ToString("F1"), fontDX2d_font9b, nf_colour_text,
//                      nf_box.X + nf_box.Width, nf_box.Y - 6);             // 5443
//   }
// Where:
//   noisefloor_color      = Color.Red                     // display.cs:2316
//   noisefloor_color_text = Color.Yellow                  // display.cs:2329
//   m_styleDots = StrokeStyle { DashOffset=2, DashStyle=Dash }  // 8464
//   m_fNoiseFloorLineWidth = 1.0f                         // display.cs:2310
//
// Lerp/actual/connector/fast-attack/NF-shift port complete; fast-attack
// trigger sources (band change, MOX, freq jump) wire from MainWindow via
// setNoiseFloorFastAttack — auto-clear logic still lives in NoiseFloorTracker.
void SpectrumWidget::paintNoiseFloorOverlay(QPainter& p, const QRect& specRect)
{
    if (!m_showNoiseFloor || m_renderedPixels.isEmpty()) {
        return;
    }

    // Both estimates come from processNoiseFloor — display.cs:5400 + 5403
    // [v2.10.3.13]:
    //   lerp        = m_fLerpAverageRX1   + _fNFshiftDBM;
    //   yPixelActual= dBToPixel(m_fFFTBinAverageRX1 + _fNFshiftDBM, H);
    // m_nfFftBinAverage / m_nfLerpAverage are the byte-for-byte ports of
    // those Thetis statics maintained per-frame in processNoiseFloor().
    const float lerp   = m_noiseFloor.lerpAverage()   + m_nfShiftDbm;
    const float actual = m_noiseFloor.fftBinAverage() + m_nfShiftDbm;

    const int yPLerp   = dbmToY(lerp,   specRect);
    const int yPActual = dbmToY(actual, specRect);

    // Fast-attack colour swap — Thetis display.cs:5431-5432 [v2.10.3.13]:
    //   nf_colour      = bFast ? m_bDX2_Gray : m_bDX2_noisefloor;
    //   nf_colour_text = bFast ? m_bDX2_Gray : m_bDX2_noisefloor_text;
    const QColor lineCol = m_noiseFloor.fastAttack()
        ? m_noiseFloorFastColor : m_noiseFloorColor;
    const QColor textCol = m_noiseFloor.fastAttack()
        ? m_noiseFloorFastColor : m_noiseFloorTextColor;

    // 8×8 NF box at lerp Y — Thetis display.cs:5219 + 5436-5437 [v2.10.3.13].
    constexpr int kNfBoxSize = 8;
    const int boxX = specRect.left() + 40;
    const int boxY = yPLerp - kNfBoxSize;
    const QRect nfBox(boxX, boxY, kNfBoxSize, kNfBoxSize);
    p.fillRect(nfBox, lineCol);

    // Horizontal dashed line at lerp Y — Thetis display.cs:5438 [v2.10.3.13].
    // m_styleDots = StrokeStyleProperties { DashOffset=2, DashStyle=Dash }
    // (display.cs:8464); Direct2D's Dash style is a 2-on/2-off pattern,
    // replicated via QPen dashPattern({2,2}) + dashOffset(2).
    const int x0 = specRect.left() + 40;
    const int x1 = specRect.right() - 40;
    if (x1 > x0) {
        QPen nfPen(lineCol, m_noiseFloorLineWidth);
        nfPen.setStyle(Qt::CustomDashLine);
        nfPen.setDashPattern({2.0, 2.0});
        nfPen.setDashOffset(2.0);
        nfPen.setCapStyle(Qt::FlatCap);
        p.setPen(nfPen);
        p.drawLine(x0, yPLerp, x1, yPLerp);
    }

    // Vertical connector between actual and lerp — Thetis display.cs:5442
    // [v2.10.3.13]:
    //   drawLineDX2D(nf_colour, nf_box.X - 3, (int)yPixelActual,
    //                nf_box.X - 3, yP, 2);  // direction up/down line
    // Width 2.  Solid (no dash).  Only visible when actual != lerp.  Thetis
    // gates this on m_bShowNoiseFloorDBM (the text flag) — we honour the
    // same gate via m_showNoiseFloor since both controls collapse to one
    // toggle in NereusSDR.
    if (yPActual != yPLerp) {
        QPen connectorPen(lineCol, 2);
        connectorPen.setCapStyle(Qt::FlatCap);
        p.setPen(connectorPen);
        p.drawLine(boxX - 3, yPActual, boxX - 3, yPLerp);
    }

    // NF dBm text anchored to the box — Thetis display.cs:5443:
    //   drawStringDX2D(lerp.ToString("F1"), fontDX2d_font9b, nf_colour_text,
    //                  nf_box.X + nf_box.Width, nf_box.Y - 6);
    // F1 format only; Thetis omits the "dBm" suffix (the dBm scale is
    // adjacent on the spectrum strip).
    const QString nfText = QString::number(static_cast<double>(lerp), 'f', 1);
    QFont nfFont = p.font();
    nfFont.setPixelSize(11);
    nfFont.setBold(true);
    p.setFont(nfFont);
    p.setPen(textCol);
    p.drawText(boxX + kNfBoxSize + 2, boxY - 6 + p.fontMetrics().ascent(),
               nfText);
}

// ---- Waterfall drawing ----
void SpectrumWidget::drawWaterfall(QPainter& p, const QRect& wfRect)
{
    if (m_waterfall.isNull() || wfRect.width() <= 0 || wfRect.height() <= 0) {
        return;
    }

    // Phase 3G-8 commit 4: global opacity for the waterfall image. Overlays
    // (filter bands, zero line, timestamp) are drawn afterward at full alpha.
    const float opacity = qBound(0, m_wfOpacity, 100) / 100.0f;
    const float savedOpacity = static_cast<float>(p.opacity());
    if (!qFuzzyCompare(opacity, 1.0f)) {
        p.setOpacity(opacity);
    }

    // Ring buffer display — newest row at top, oldest at bottom (normal scroll).
    // From Thetis display.cs:7719-7729: new row written at top, old content shifts down.
    // W5 (reverse scroll) removed in Task 2.8; migration in Task 5.1.
    int wfH = m_waterfall.height();

    // Part 1 (top of screen): from writeRow to end of image
    int part1Rows = wfH - m_wfWriteRow;
    if (part1Rows > 0) {
        QRect src(0, m_wfWriteRow, m_waterfall.width(), part1Rows);
        QRect dst(wfRect.left(), wfRect.top(), wfRect.width(), part1Rows);
        p.drawImage(dst, m_waterfall, src);
    }
    if (m_wfWriteRow > 0) {
        QRect src(0, 0, m_waterfall.width(), m_wfWriteRow);
        QRect dst(wfRect.left(), wfRect.top() + part1Rows, wfRect.width(), m_wfWriteRow);
        p.drawImage(dst, m_waterfall, src);
    }

    if (!qFuzzyCompare(opacity, 1.0f)) {
        p.setOpacity(savedOpacity);
    }

    drawWaterfallChrome(p, wfRect);
}

// Phase 3G-8 commit 10: waterfall chrome (filter/zero-line/timestamp
// overlays + opacity dim) factored out of drawWaterfall() so the GPU
// overlay texture can reuse it. Called with a full-opacity painter.
void SpectrumWidget::drawWaterfallChrome(QPainter& p, const QRect& wfRect)
{
    // Opacity dim for the GPU path. The QPainter fallback path dims the
    // waterfall image itself via p.setOpacity() before the blit; this
    // dim overlay matches that visual effect on GPU where the waterfall
    // texture is drawn at full alpha by the m_wfPipeline and the
    // overlay texture is layered on top.
#ifdef NEREUS_GPU_SPECTRUM
    const int op = qBound(0, m_wfOpacity, 100);
    if (op < 100) {
        const int dimAlpha = 255 - static_cast<int>(255.0 * op / 100.0);
        p.fillRect(wfRect, QColor(10, 10, 20, dimAlpha));
    }
#endif

    // RX filter passband as a translucent vertical band spanning the
    // waterfall height. Uses m_vfoHz + m_filterLowHz/m_filterHighHz.
    if (m_showRxFilterOnWaterfall && m_vfoHz > 0.0) {
        const double loHz = m_vfoHz + m_filterLowHz;
        const double hiHz = m_vfoHz + m_filterHighHz;
        const int x1 = hzToX(loHz, wfRect);
        const int x2 = hzToX(hiHz, wfRect);
        if (x2 > x1) {
            QColor band(0x00, 0xb4, 0xd8, 50);
            p.fillRect(QRect(x1, wfRect.top(), x2 - x1, wfRect.height()), band);
        }
    }

    // Plan 4 D9 (Cluster E): TX filter column on waterfall, MOX-gated.
    // m_showTxFilterOnRxWaterfall: user preference (Setup → Display).
    // m_moxOverlay: reused from H.1 (3M-1a); set by MoxController.
    if (m_showTxFilterOnRxWaterfall && m_moxOverlay) {
        drawTxFilterWaterfallColumn(p, wfRect);
    }
    // Plan 4 D9c-1: RX zero-line uses m_rxZeroLineColor (was hardcoded red 180).
    if (m_showRxZeroLineOnWaterfall && m_vfoHz > 0.0) {
        const int x = hzToX(m_vfoHz, wfRect);
        if (x >= wfRect.left() && x <= wfRect.right()) {
            QPen zeroPen(m_rxZeroLineColor, 1);
            p.setPen(zeroPen);
            p.drawLine(x, wfRect.top(), x, wfRect.bottom());
        }
    }

    // Plan 4 D9c-1: TX zero-line on waterfall — MOX-gated.
    if (m_showTxZeroLineOnWaterfall && m_moxOverlay && m_vfoHz > 0.0) {
        const int x = hzToX(m_vfoHz, wfRect);
        if (x >= wfRect.left() && x <= wfRect.right()) {
            QPen txZeroPen(m_txZeroLineColor, 1);
            p.setPen(txZeroPen);
            p.drawLine(x, wfRect.top(), x, wfRect.bottom());
        }
    }

    // Timestamp overlay on waterfall (NereusSDR extensions W8/W9).
    if (m_wfTimestampPos != TimestampPosition::None) {
        const QDateTime now = (m_wfTimestampMode == TimestampMode::UTC)
                              ? QDateTime::currentDateTimeUtc()
                              : QDateTime::currentDateTime();
        const QString stamp = now.toString(QStringLiteral("hh:mm:ss"));
        QFont f = p.font();
        f.setPixelSize(10);
        p.setFont(f);
        p.setPen(QColor(200, 220, 255));
        const int pad = 4;
        const int textW = p.fontMetrics().horizontalAdvance(stamp);
        int x = (m_wfTimestampPos == TimestampPosition::Left)
                ? (wfRect.left() + pad)
                : (wfRect.right() - textW - pad);
        p.drawText(x, wfRect.top() + 12, stamp);
    }
}

// ---- Frequency scale bar ----
void SpectrumWidget::drawFreqScale(QPainter& p, const QRect& r)
{
    p.fillRect(r, QColor(0x10, 0x15, 0x20));

    // Phase 3G-8 commit 5: Off alignment suppresses labels entirely.
    if (m_freqLabelAlign == FreqLabelAlign::Off) {
        return;
    }

    QFont font = p.font();
    font.setPixelSize(10);
    p.setFont(font);
    // From Thetis display.cs:2003 — grid_text_color (now configurable).
    p.setPen(m_gridTextColor);

    double freqStep = 25000.0;
    if (m_bandwidthHz > 500000.0) {
        freqStep = 50000.0;
    } else if (m_bandwidthHz < 50000.0) {
        freqStep = 5000.0;
    } else if (m_bandwidthHz < 100000.0) {
        freqStep = 10000.0;
    }

    // Phase 3G-8 commit 5: Thetis 5-mode label alignment.
    // Left / Center / Right / Auto (center with fallback) / Off.
    const Qt::Alignment baseFlags =
        (m_freqLabelAlign == FreqLabelAlign::Left)  ? (Qt::AlignLeft  | Qt::AlignVCenter) :
        (m_freqLabelAlign == FreqLabelAlign::Right) ? (Qt::AlignRight | Qt::AlignVCenter) :
                                                       (Qt::AlignHCenter | Qt::AlignVCenter);

    double startFreq = std::ceil((m_centerHz - m_bandwidthHz / 2.0) / freqStep) * freqStep;
    for (double f = startFreq; f < m_centerHz + m_bandwidthHz / 2.0; f += freqStep) {
        int x = hzToX(f, r);
        // Format as MHz with appropriate decimals
        double mhz = f / 1.0e6;
        QString label;
        if (freqStep >= 100000.0) {
            label = QString::number(mhz, 'f', 1);
        } else if (freqStep >= 10000.0) {
            label = QString::number(mhz, 'f', 2);
        } else {
            label = QString::number(mhz, 'f', 3);
        }

        // Cached QStaticText render — see m_freqLabelCache comment in
        // SpectrumWidget.h.  Working set grows as the user pans but
        // converges quickly because the format strings repeat.
        auto cit = m_freqLabelCache.find(label);
        if (cit == m_freqLabelCache.end()) {
            QStaticText st(label);
            st.setPerformanceHint(QStaticText::AggressiveCaching);
            cit = m_freqLabelCache.insert(label, std::move(st));
        }
        cit.value().prepare(p.transform(), font);

        // Position the cached text inside the same 60-wide rect that
        // drawText was using, honoring the configured alignment.  The
        // rect itself isn't drawn; it's only used to anchor the label.
        const QRectF textRect(x - 30, r.top() + 2, 60, r.height() - 2);
        const QSizeF labelSize = cit.value().size();
        qreal lx = textRect.left();   // AlignLeft default
        if (baseFlags & Qt::AlignHCenter) {
            lx = textRect.center().x() - labelSize.width() / 2.0;
        } else if (baseFlags & Qt::AlignRight) {
            lx = textRect.right() - labelSize.width();
        }
        const qreal ly = textRect.center().y() - labelSize.height() / 2.0;
        p.drawStaticText(QPointF(lx, ly), cit.value());
    }
}

// ---- dBm scale strip ----
// From AetherSDR SpectrumWidget.cpp:4856-4925 [@0cd4559]
void SpectrumWidget::drawDbmScale(QPainter& p, const QRect& specRect)
{
    const QRect strip = NereusSDR::DbmStrip::stripRect(specRect, kDbmStripW);

    // Semi-opaque background
    p.fillRect(strip, QColor(0x0a, 0x0a, 0x18, 220));

    // Left border line
    p.setPen(QColor(0x30, 0x40, 0x50));
    p.drawLine(strip.left(), specRect.top(), strip.left(), specRect.bottom());

    // ── Up/Down arrows side by side at top ─────────────────────────────
    const int halfW    = kDbmStripW / 2;
    const int upCx     = strip.left() + halfW / 2;          // left half center
    const int dnCx     = strip.left() + halfW + halfW / 2;  // right half center
    const int arrowTop = specRect.top() + 2;
    const int arrowBot = specRect.top() + kDbmArrowH - 2;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x60, 0x80, 0xa0));

    // Up arrow (▲) — left side
    QPolygon upTri;
    upTri << QPoint(upCx - 5, arrowBot)
          << QPoint(upCx + 5, arrowBot)
          << QPoint(upCx,     arrowTop);
    p.drawPolygon(upTri);

    // Down arrow (▼) — right side
    QPolygon dnTri;
    dnTri << QPoint(dnCx - 5, arrowTop)
          << QPoint(dnCx + 5, arrowTop)
          << QPoint(dnCx,     arrowBot);
    p.drawPolygon(dnTri);

    // ── dBm labels ───────────────────────────────────────────────────────
    QFont f = p.font();
    f.setPointSize(7);
    p.setFont(f);
    const QFontMetrics fm(f);

    const int labelTop = specRect.top() + kDbmArrowH + 4;
    const float stepDb = NereusSDR::DbmStrip::adaptiveStepDb(m_dynamicRange);

    const float bottomDbm  = m_refLevel - m_dynamicRange;
    const float firstLabel = std::ceil(bottomDbm / stepDb) * stepDb;

    // drawStaticText anchors at the top-left of the glyph run; drawText
    // anchors at the baseline.  The original baseline-y was
    // (y + ascent/2); to keep visual-identical placement, drawStaticText
    // takes y = (y + ascent/2) - ascent = y - ascent/2.
    const qreal staticAscent = fm.ascent();

    for (float dbm = firstLabel; dbm <= m_refLevel; dbm += stepDb) {
        // Route through dbmToY() so m_dbmCalOffset is applied consistently with
        // the grid/trace/peak-hold paths — otherwise a non-zero cal offset would
        // drift strip ticks off the actual grid lines.
        const int y = dbmToY(dbm, specRect);
        if (y < labelTop || y > specRect.bottom() - 5) continue;

        // Tick mark
        p.setPen(QColor(0x50, 0x70, 0x80));
        p.drawLine(strip.left(), y, strip.left() + 4, y);

        // Label — QStaticText cache: HarfBuzz shapes each unique string
        // exactly once over the widget lifetime.  See m_dbmLabelCache
        // comment in SpectrumWidget.h for rationale + AetherSDR cite.
        const QString label = QString::number(static_cast<int>(dbm));
        auto cit = m_dbmLabelCache.find(label);
        if (cit == m_dbmLabelCache.end()) {
            QStaticText st(label);
            st.setPerformanceHint(QStaticText::AggressiveCaching);
            cit = m_dbmLabelCache.insert(label, std::move(st));
        }
        // prepare() with the painter's actual transform avoids a
        // first-paint rebuild on HiDPI displays.
        cit.value().prepare(p.transform(), f);

        p.setPen(QColor(0x80, 0xa0, 0xb0));
        p.drawStaticText(
            QPointF(strip.left() + 6, y - staticAscent / 2.0),
            cit.value());
    }
}

// ---- dBm scale strip: shared chrome + labels for the 3D scale ----
// From AetherSDR SpectrumWidget.cpp:17223-17261 [@1872028c]
//
// Upstream shares this chrome (background/border/up-down arrows) between
// its own drawDbmScale() (2D) and drawDbmScale3D(). Ported fresh here as a
// NEW function rather than factored out of drawDbmScale() above: that
// function is used by the 2D path on every frame and must not change.
// The only current caller is drawDbmScale3D() below.
void SpectrumWidget::drawDbmScaleChrome(QPainter& p, const QRect& specRect)
{
    const QRect strip = NereusSDR::DbmStrip::stripRect(specRect, kDbmStripW);

    // Opaque background: since #3482 the FFT trace/waterfall end at the strip's
    // left edge, so nothing meaningful renders beneath it — a solid fill gives a
    // crisp right edge instead of letting the bg/grid bleed through and read as
    // right-side asymmetry against the hard left window border.
    p.fillRect(strip, QColor(0x0a, 0x0a, 0x18, 220));

    // Left border line
    p.setPen(QColor(0x30, 0x40, 0x50));
    p.drawLine(strip.left(), specRect.top(), strip.left(), specRect.bottom());

    // ── Up/Down arrows side by side at top ─────────────────────────────
    const int halfW = kDbmStripW / 2;
    const int upCx  = strip.left() + halfW / 2;         // left half center
    const int dnCx  = strip.left() + halfW + halfW / 2; // right half center
    const int arrowTop = specRect.top() + 2;
    const int arrowBot = specRect.top() + kDbmArrowH - 2;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x60, 0x80, 0xa0));

    // Up arrow (▲) — left side
    QPolygon upTri;
    upTri << QPoint(upCx - 5, arrowBot)
          << QPoint(upCx + 5, arrowBot)
          << QPoint(upCx,     arrowTop);
    p.drawPolygon(upTri);

    // Down arrow (▼) — right side
    QPolygon dnTri;
    dnTri << QPoint(dnCx - 5, arrowTop)
          << QPoint(dnCx + 5, arrowTop)
          << QPoint(dnCx,     arrowBot);
    p.drawPolygon(dnTri);
}

// From AetherSDR SpectrumWidget.cpp:17263-17312 [@1872028c]
void SpectrumWidget::drawDbmScaleLabels(QPainter& p, const QRect& specRect,
                                        float topDbm, float rangeDb)
{
    if (rangeDb <= 0.0f) {
        return;
    }
    const QRect strip = NereusSDR::DbmStrip::stripRect(specRect, kDbmStripW);

    // ── dBm labels — full-height LINEAR axis: topDbm at the top, topDbm-rangeDb
    //    at the baseline, evenly spaced across specRect.height(). ──────────
    QFont f = p.font();
    f.setPointSize(7);
    p.setFont(f);
    const QFontMetrics fm(f);

    const int labelTop = specRect.top() + kDbmArrowH + 4;

    // Use adaptive step: aim for ~4-6 labels
    const float stepDb = NereusSDR::DbmStrip::adaptiveStepDb(rangeDb);

    const float bottomDbm = topDbm - rangeDb;
    const float firstLabel = std::ceil(bottomDbm / stepDb) * stepDb;

    auto drawTickLabel = [&](float dbm, int y, int textBaseline) {
        p.setPen(QColor(0x50, 0x70, 0x80));
        p.drawLine(strip.left(), y, strip.left() + 4, y);

        const QString label = QString::number(static_cast<int>(std::lround(dbm)));
        p.setPen(QColor(0x80, 0xa0, 0xb0));
        p.drawText(strip.left() + 6, textBaseline, label);
    };

    for (float dbm = firstLabel; dbm <= topDbm; dbm += stepDb) {
        const float frac = (topDbm - dbm) / rangeDb;
        const int y = specRect.top() + static_cast<int>(frac * specRect.height());
        if (y < labelTop || y > specRect.bottom() - 5) { continue; }

        drawTickLabel(dbm, y, y + fm.ascent() / 2);
    }

    const int bottomY = specRect.bottom();
    if (bottomY >= labelTop) {
        drawTickLabel(bottomDbm, bottomY, bottomY - 2);
    }
}

// ─── dBm scale strip for 3D stacked-trace mode ───────────────────────────────
// From AetherSDR SpectrumWidget.cpp:17320-17335 [@1872028c]
//
// In 3D mode, a single right-side axis cannot be pixel-exact for every
// perspective row. Keep it as a full-height amplitude reference anchored to the
// 3D floor, so plain drag visibly shifts the dBm numbers and Ctrl/Meta-drag
// changes the span.
void SpectrumWidget::drawDbmScale3D(QPainter& p, const QRect& specRect,
                                    float floorDbm)
{
    drawDbmScaleChrome(p, specRect);
    // Round the span the same way the mesh/CPU surface do (buildDssImage /
    // renderGpuFrame use std::round(span*2)/2) so labels and surface agree to
    // the pixel instead of a sub-dB top/bottom skew (#3937). NereusSDR names
    // this rounding once, dssRoundedSpanDb() (Task 11 fast-follow), and all
    // three consumers call it rather than independently inlining it.
    const float span = dssRoundedSpanDb();
    drawDbmScaleLabels(p, specRect, floorDbm + span, span);
}

// ---- Band-plan strip ----
// From AetherSDR SpectrumWidget.cpp:4220-4293 [@0cd4559]
void SpectrumWidget::drawBandPlan(QPainter& p, const QRect& specRect)
{
    if (!m_bandPlanMgr || m_bandPlanFontSize <= 0) {
        return;
    }

    const double startMhz = m_centerHz / 1.0e6 - (m_bandwidthHz / 2.0) / 1.0e6;
    const double endMhz   = m_centerHz / 1.0e6 + (m_bandwidthHz / 2.0) / 1.0e6;
    const int    bandH    = m_bandPlanFontSize + 4;
    const int    bandY    = specRect.bottom() - bandH + 1;

    const auto& segments = m_bandPlanMgr->segments();
    for (const auto& seg : segments) {
        if (seg.highMhz <= startMhz || seg.lowMhz >= endMhz) {
            continue;
        }

        const int x1 = hzToX(std::max(seg.lowMhz,  startMhz) * 1.0e6, specRect);
        const int x2 = hzToX(std::min(seg.highMhz, endMhz)   * 1.0e6, specRect);
        if (x2 <= x1) {
            continue;
        }

        // License-class brightness blend: more-restrictive classes paint dimmer
        // so the eye can scan to "where I'm allowed to operate" at a glance.
        // From AetherSDR SpectrumWidget.cpp:4239-4244 [@0cd4559].
        const QString& lic = seg.license;
        float blend = 0.6f;
        if      (lic == QLatin1String("E"))          { blend = 0.20f; }
        else if (lic == QLatin1String("E,G"))         { blend = 0.40f; }
        else if (lic.contains(QLatin1Char('T')))      { blend = 0.60f; }
        else if (lic.isEmpty())                       { blend = 0.50f; }

        const QColor bg(0x0a, 0x0a, 0x14);
        const QColor fill(
            static_cast<int>(seg.color.red()   * blend + bg.red()   * (1.0f - blend)),
            static_cast<int>(seg.color.green() * blend + bg.green() * (1.0f - blend)),
            static_cast<int>(seg.color.blue()  * blend + bg.blue()  * (1.0f - blend)),
            255);
        p.fillRect(x1, bandY, x2 - x1, bandH, fill);

        // Separator line at left edge of each segment.
        p.setPen(QColor(0x0f, 0x0f, 0x1a, 200));
        p.drawLine(x1, bandY, x1, bandY + bandH);

        // Label: mode + lowest license class allowed (only if there's room).
        if (x2 - x1 > 20) {
            QFont f = p.font();
            f.setPointSize(m_bandPlanFontSize);
            f.setBold(true);
            p.setFont(f);

            const QString lowestClass = lowestLicenceClass(lic);

            QString label = seg.label;
            if (!lowestClass.isEmpty() && x2 - x1 > 60) {
                label = QStringLiteral("%1 %2").arg(seg.label, lowestClass);
            }

            p.setPen(Qt::white);
            p.drawText(QRect(x1, bandY, x2 - x1, bandH), Qt::AlignCenter, label);
        }
    }

    // Spot markers (white dots for digital calling frequencies, etc.).
    // From AetherSDR SpectrumWidget.cpp:4282-4292 [@0cd4559].
    const auto& spots = m_bandPlanMgr->spots();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    for (const auto& spot : spots) {
        if (spot.freqMhz < startMhz || spot.freqMhz > endMhz) {
            continue;
        }
        const int sx = hzToX(spot.freqMhz * 1.0e6, specRect);
        p.drawEllipse(QPoint(sx, bandY + bandH / 2), 4, 4);
    }
    p.setRenderHint(QPainter::Antialiasing, false);
}

// ─── Time scale + LIVE button (sub-epic E) ─────────────────────────────
// From AetherSDR SpectrumWidget.cpp:4929-4994 [@0cd4559]
//   adapter: NereusSDR computes msPerRow directly from m_wfUpdatePeriodMs
//   instead of AetherSDR's calibrated m_wfMsPerRow (we have no radio
//   tile clock to calibrate against).

void SpectrumWidget::drawTimeScale(QPainter& p, const QRect& wfRect)
{
    const QRect strip = waterfallTimeScaleRect(wfRect);
    const int stripX = strip.x();

    // Semi-opaque background so spectrum content underneath dims.
    p.fillRect(strip, QColor(0x0a, 0x0a, 0x18, 220));

    // Left border line — separates the strip from waterfall content.
    p.setPen(QColor(0x30, 0x40, 0x50));
    p.drawLine(stripX, wfRect.top(), stripX, wfRect.bottom());

    // LIVE button — grey when live, bright red when paused.
    const QRect liveRect = waterfallLiveButtonRect(wfRect);
    p.setPen(QColor(0x40, 0x50, 0x60));
    p.setBrush(m_wfLive ? QColor(0x45, 0x45, 0x45)
                        : QColor(0xc0, 0x20, 0x20));  // bright red when paused
    p.drawRoundedRect(liveRect, 3, 3);

    QFont liveFont = p.font();
    liveFont.setPointSize(7);
    liveFont.setBold(true);
    p.setFont(liveFont);
    p.setPen(m_wfLive ? QColor(0xb0, 0xb0, 0xb0) : Qt::white);
    p.drawText(liveRect, Qt::AlignCenter, QStringLiteral("LIVE"));

    // Tick labels along the strip.
    const float msPerRow = std::max(1, m_wfUpdatePeriodMs);
    const QRect labelRect = strip.adjusted(0, 4, 0, 0);
    const float totalSec = labelRect.height() * msPerRow / 1000.0f;
    if (totalSec <= 0) {
        return;
    }

    QFont f = p.font();
    f.setPointSize(7);
    f.setBold(false);
    p.setFont(f);
    const QFontMetrics fm(f);

    constexpr float kStepSec = 5.0f;
    for (float sec = 0; sec <= totalSec; sec += kStepSec) {
        const float frac = sec / totalSec;
        const int yy = labelRect.top()
                     + static_cast<int>(frac * labelRect.height());
        if (yy > wfRect.bottom() - 5) {
            continue;
        }

        // Tick mark
        p.setPen(QColor(0x50, 0x70, 0x80));
        p.drawLine(stripX, yy, stripX + 4, yy);

        // Label: elapsed seconds when live, absolute UTC when paused.
        const QString label = m_wfLive
            ? QStringLiteral("%1s").arg(static_cast<int>(sec))
            : pausedTimeLabelForAge(
                m_wfHistoryOffsetRows
                + static_cast<int>(std::round(sec * 1000.0f / msPerRow)));

        p.setPen(QColor(0x80, 0xa0, 0xb0));
        if (m_wfLive) {
            p.drawText(stripX + 6, yy + fm.ascent() / 2, label);
        } else {
            const QRect textRect(stripX + 6, yy - fm.height() / 2,
                                 strip.width() - 10, fm.height());
            p.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
        }
    }
}

// ---- Coordinate helpers ----

int SpectrumWidget::hzToX(double hz, const QRect& r) const
{
    double lowHz = m_centerHz - m_bandwidthHz / 2.0;
    double frac = (hz - lowHz) / m_bandwidthHz;
    return r.left() + static_cast<int>(frac * r.width());
}

double SpectrumWidget::xToHz(int x, const QRect& r) const
{
    double frac = static_cast<double>(x - r.left()) / r.width();
    return (m_centerHz - m_bandwidthHz / 2.0) + frac * m_bandwidthHz;
}

std::pair<int, int> SpectrumWidget::visibleBinRange(int binCount) const
{
    // R1 Task 5: the arithmetic moved verbatim to
    // NereusSDR::SpectrumReducer::visibleBinRange, which takes the same
    // four values as explicit config instead of reading them off a widget.
    // This overload stays as the widget-state-reading front end so both
    // in-widget callers (updateSpectrumLinear's empty-slice guard and the
    // spot-overlay mapping) keep running the one implementation.
    NereusSDR::ReducerConfig cfg;
    cfg.centreHz       = m_centerHz;
    cfg.spanHz         = m_bandwidthHz;
    // During MOX the bins come from the 96 kHz TX analyzer centered on the
    // transmit carrier. Feed that stream geometry into the same core reducer
    // calculation used for receive frames.
    // Parity Task 31: keyed with DUP on the bins are the receiver's.
    const bool useTx = showsTransmitView()
                    && m_txSampleRateHz > 0.0
                    && m_txCenterHz != 0.0;
    cfg.streamCentreHz = useTx ? m_txCenterHz : m_ddcCenterHz;
    cfg.sampleRateHz = useTx ? m_txSampleRateHz : m_sampleRateHz;
    return NereusSDR::SpectrumReducer::visibleBinRange(binCount, cfg);
}

int SpectrumWidget::bandPlanStripHeight() const
{
    // Mirrors drawBandPlan's height calc (display.cpp:3280):
    //   bandH = m_bandPlanFontSize + 4 when a bandplan manager is bound.
    return (m_bandPlanMgr && m_bandPlanFontSize > 0)
           ? (m_bandPlanFontSize + 4) : 0;
}

// 1-Hz-bandwidth normalisation shift.  When m_dispNormalize is on, every
// dB value gets shifted by -10*log10(binWidthHz) — equivalent to dividing
// the linear power per bin by the bin width before logging, so the trace
// represents power-spectral-density referenced to 1 Hz instead of per-bin.
// Mirrors Thetis SetDisplayNormOneHz (specHPSDR.cs:325) but applied at the
// Qt rendering stage so the toggle is instantly reversible without a WDSP
// channel rebuild.
float SpectrumWidget::normalizeShiftDb() const
{
    return NereusSDR::normalizeShiftDb(normalizeActive(), binWidthHz());
}

// The normalise is on and the spectrum detector is one it applies to
// (Average, Sample or RMS; normalizeAppliesToDetector). With Peak or
// Rosenfell the stored choice is kept but nothing moves, as Thetis does.
bool SpectrumWidget::normalizeActive() const
{
    return m_dispNormalize
        && NereusSDR::normalizeAppliesToDetector(static_cast<int>(m_spectrumDetector));
}

// Parity Task 31 (A11, R-R3-49): the calibration the trace is drawn with.
// From Thetis display.cs:4820-4850 [v2.10.3.15] (RX1Offset):
//     bool local_mox = localMox(1);
//     bool displayduplex = isRxDuplex(1);
//     if (local_mox)
//     {
//         fOffset = tx_display_cal_offset;
//         if (displayduplex)
//         {
//             fOffset += rx1_display_cal_offset; //[2.10.1.0] MW0LGE fix issue #137
//             fOffset += tx_attenuator_offset; //[2.10.3.6]MW0LGE att_fix // change fixes #482
//         }
//     }
//     ...
//     else fOffset = rx1_display_cal_offset;
//     if (!local_mox) fOffset += rx1_preamp_offset;
// Thetis's spectrum trace takes it at display.cs:5113 [v2.10.3.15]
// (fOffset = rx == 1 ? RX1Offset : RX2Offset). Here local_mox is the MOX
// overlay on the transmitting pan. m_dbmCalOffset is the receive
// rx1_display_cal_offset + rx1_preamp_offset (RadioModel::rxMeterOffsetDb,
// console.cs:21080-21083 [v2.10.3.15] RXOffset); m_rxPreampOffsetDb its
// preamp half, so keyed with DUP on the receive calibration alone is added,
// as Thetis adds it. The _tx_on_vfob branch has no pan of its own here: the
// pan hosting the transmit slice is the one keyed.
// A remote pan's frames, receive and transmit, carry the Core's calibration
// already (DaemonMediaController, RadioModel::keyedDisplayOffsetDb), so it
// adds none.
float SpectrumWidget::displayCalOffsetDb() const
{
    if (m_remoteSpectrum) {
        return 0.0f;
    }
    if (!m_moxOverlay) {
        // From Thetis display.cs:1372 Display.RX1DisplayCalOffset.
        return m_dbmCalOffset;
    }
    float offset = m_txDisplayCalOffsetDb;
    if (m_displayDuplex) {
        offset += m_dbmCalOffset - m_rxPreampOffsetDb;
        offset += m_txAttOffsetDb;
    }
    return offset;
}

void SpectrumWidget::setRxPreampOffsetDb(float db)
{
    // From Thetis console.cs:12310 [v2.10.3.15] (UpdateRX1DisplayOffsets):
    //   Display.RX1PreampOffset = RXPreampOffset(1);
    if (qFuzzyCompare(1.0f + m_rxPreampOffsetDb, 1.0f + db)) {
        return;
    }
    m_rxPreampOffsetDb = db;
    if (m_moxOverlay && m_displayDuplex) {
        m_hasNewSpectrum = true;
        markOverlayDirty();
        update();
    }
}

void SpectrumWidget::setTxDisplayCalOffsetDb(float db)
{
    // From Thetis setup.cs:14364 [v2.10.3.15] udTXDisplayCalOffset_ValueChanged
    // -> Display.TXDisplayCalOffset.
    if (qFuzzyCompare(1.0f + m_txDisplayCalOffsetDb, 1.0f + db)) {
        return;
    }
    m_txDisplayCalOffsetDb = db;
    if (m_moxOverlay) {
        m_hasNewSpectrum = true;
        markOverlayDirty();
        update();
    }
}

int SpectrumWidget::dbmToY(float dbm, const QRect& r) const
{
    const float calibrated = dbm + displayCalOffsetDb() + normalizeShiftDb();
    float bottom = m_refLevel - m_dynamicRange;
    float frac = (calibrated - bottom) / m_dynamicRange;
    frac = qBound(0.0f, frac, 1.0f);
    // Reserve the band plan strip height — the dBm floor must map to the
    // TOP of the band plan strip, not the panel bottom, so spectrum
    // overlays don't render on top of (or below) the strip.
    const int bandH = bandPlanStripHeight();
    const int contentBottom = r.bottom() - bandH;
    const int contentH      = qMax(1, r.height() - bandH);
    return contentBottom - static_cast<int>(frac * contentH);
}

float SpectrumWidget::dbmToYf(float dbm, const QRect& r) const
{
    const float calibrated = dbm + displayCalOffsetDb() + normalizeShiftDb();
    float bottom = m_refLevel - m_dynamicRange;
    float frac = (calibrated - bottom) / m_dynamicRange;
    frac = qBound(0.0f, frac, 1.0f);
    const int   bandH        = bandPlanStripHeight();
    const float contentBottom = static_cast<float>(r.bottom() - bandH);
    const float contentH      = static_cast<float>(qMax(1, r.height() - bandH));
    return contentBottom - frac * contentH;
}

// ─── Waterfall scrollback math helpers (sub-epic E) ───────────────────
// From AetherSDR SpectrumWidget.cpp:559-590 [@0cd4559]
//   plus 4096-row cap from [@2bb3b5c] (unmerged AetherSDR PR #1478)

int SpectrumWidget::waterfallHistoryCapacityRows() const
{
    const int msPerRow = std::max(1, m_wfUpdatePeriodMs);
    const int rows = static_cast<int>(
        (m_waterfallHistoryMs + msPerRow - 1) / msPerRow);
    return std::min(rows, kMaxWaterfallHistoryRows);
}

int SpectrumWidget::maxWaterfallHistoryOffsetRows() const
{
    return std::max(0, m_wfHistoryRowCount - m_waterfall.height());
}

int SpectrumWidget::historyRowIndexForAge(int ageRows) const
{
    if (m_waterfallHistory.isNull() || ageRows < 0
        || ageRows >= m_wfHistoryRowCount) {
        return -1;
    }
    return (m_wfHistoryWriteRow + ageRows) % m_waterfallHistory.height();
}

QString SpectrumWidget::pausedTimeLabelForAge(int ageRows) const
{
    const int rowIndex = historyRowIndexForAge(ageRows);
    if (rowIndex < 0 || rowIndex >= m_wfHistoryTimestamps.size()) {
        return QString();
    }
    const qint64 timestampMs = m_wfHistoryTimestamps[rowIndex];
    if (timestampMs <= 0) {
        return QString();
    }
    const QDateTime utc = QDateTime::fromMSecsSinceEpoch(
        timestampMs, QTimeZone::utc());
    return QStringLiteral("-") + utc.toString(QStringLiteral("HH:mm:ssZ"));
}

// From AetherSDR SpectrumWidget.cpp:594-632 [@0cd4559]
void SpectrumWidget::ensureWaterfallHistory()
{
    if (m_waterfall.isNull()) {
        return;
    }

    const QSize desiredSize(m_waterfall.width(), waterfallHistoryCapacityRows());
    if (desiredSize.width() <= 0 || desiredSize.height() <= 0) {
        return;
    }
    if (m_wfHistoryWindows.size() != desiredSize.height()) {
        m_wfHistoryWindows.resize(desiredSize.height());
    }

    if (m_waterfallHistory.size() == desiredSize) {
        return;
    }

    // Preserve rows across width changes (e.g. divider drag, manual window
    // resize) by horizontally scaling the existing history image. Height
    // capacity is fixed via waterfallHistoryCapacityRows() so row indices
    // and timestamps remain valid.
    QImage newHistory;
    if (!m_waterfallHistory.isNull() && m_wfHistoryRowCount > 0
        && m_waterfallHistory.height() == desiredSize.height()) {
        newHistory = m_waterfallHistory.scaled(
            desiredSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    if (newHistory.isNull() || newHistory.size() != desiredSize) {
        traceRxHistoryEvent(RxHistoryEvent::HistoryBufferReset, -1, 0, desiredSize);
        newHistory = QImage(desiredSize, QImage::Format_RGB32);
        newHistory.fill(Qt::black);
        m_wfHistoryTimestamps = QVector<qint64>(desiredSize.height(), 0);
        m_wfHistoryWindows = QVector<WaterfallRowWindow>(desiredSize.height());
        m_wfHistoryWriteRow = 0;
        m_wfHistoryRowCount = 0;
        m_wfHistoryOffsetRows = 0;
        m_wfLive = true;
    }
    m_waterfallHistory = newHistory;
}

// From AetherSDR SpectrumWidget.cpp:647-668 [@0cd4559]
void SpectrumWidget::appendHistoryRow(const QRgb* rowData, qint64 timestampMs)
{
    ensureWaterfallHistory();
    if (m_waterfallHistory.isNull() || rowData == nullptr) {
        return;
    }

    const int h = m_waterfallHistory.height();
    m_wfHistoryWriteRow = (m_wfHistoryWriteRow - 1 + h) % h;
    auto* row = reinterpret_cast<QRgb*>(
        m_waterfallHistory.bits()
        + m_wfHistoryWriteRow * m_waterfallHistory.bytesPerLine());
    std::memcpy(row, rowData, m_waterfallHistory.width() * sizeof(QRgb));
    if (m_wfHistoryWriteRow >= 0
        && m_wfHistoryWriteRow < m_wfHistoryTimestamps.size()) {
        m_wfHistoryTimestamps[m_wfHistoryWriteRow] = timestampMs;
        m_wfHistoryWindows[m_wfHistoryWriteRow] = {m_centerHz, m_bandwidthHz};
    }
    if (m_wfHistoryRowCount < h) {
        ++m_wfHistoryRowCount;
    }
    if (!m_wfLive) {
        // Auto-bump while paused so the displayed row stays visually fixed
        // as new live rows arrive underneath.
        m_wfHistoryOffsetRows = std::min(
            m_wfHistoryOffsetRows + 1, maxWaterfallHistoryOffsetRows());
    }
}

// From AetherSDR SpectrumWidget.cpp:670-705 [@0cd4559]
void SpectrumWidget::rebuildWaterfallViewport()
{
    rebuildWaterfallViewport(m_centerHz, m_bandwidthHz);
}

// NereusSDR-original: captured RX and TX rows can have different RF
// windows. Rebuild from their original pixels, never from an already
// projected viewport, so returning to an earlier window is lossless.
void SpectrumWidget::rebuildWaterfallViewport(double centerHz, double bandwidthHz)
{
    if (m_waterfall.isNull()) {
        return;
    }
    // resizeEvent replaces the live image before its debounced history
    // resize. Window/MOX changes can rebuild in that interval; synchronize
    // the source width before copying or indexing any history scanline.
    ensureWaterfallHistory();

    traceRxHistoryEvent(RxHistoryEvent::ViewportRebuild);
    m_wfHistoryOffsetRows = std::clamp(
        m_wfHistoryOffsetRows, 0, maxWaterfallHistoryOffsetRows());
    m_waterfall.fill(Qt::black);
    m_wfWriteRow = 0;

    if (m_waterfallHistory.isNull()) {
        update();
        return;
    }

    const int rowWidthBytes = m_waterfall.width() * static_cast<int>(sizeof(QRgb));
    for (int y = 0; y < m_waterfall.height(); ++y) {
        const int rowIndex = historyRowIndexForAge(m_wfHistoryOffsetRows + y);
        if (rowIndex < 0) {
            break;
        }
        const QRgb* src = reinterpret_cast<const QRgb*>(
            m_waterfallHistory.constScanLine(rowIndex));
        auto* dst = reinterpret_cast<QRgb*>(m_waterfall.scanLine(y));
        const WaterfallRowWindow window = m_wfHistoryWindows.value(rowIndex);
        if (window.bandwidthHz <= 0.0 || bandwidthHz <= 0.0
            || (qFuzzyCompare(window.centerHz, centerHz)
                && qFuzzyCompare(window.bandwidthHz, bandwidthHz))) {
            std::memcpy(dst, src, rowWidthBytes);
        } else {
            const double startHz = centerHz - bandwidthHz / 2.0;
            const double capturedStartHz = window.centerHz - window.bandwidthHz / 2.0;
            const int width = m_waterfall.width();
            for (int x = 0; x < width; ++x) {
                const double hz = startHz + (x + 0.5) * bandwidthHz / width;
                const double capturedX = (hz - capturedStartHz)
                                       / window.bandwidthHz * width;
                if (capturedX >= 0.0 && capturedX < width) {
                    dst[x] = src[int(capturedX)];
                }
            }
        }
    }

    // Force GPU full re-upload — the per-row delta path can't follow a
    // viewport rebuild because m_wfWriteRow no longer indexes the most
    // recent live row. Two sentinels needed: m_wfTexFullUpload routes
    // the next frame through the full-upload branch (matching upstream
    // AetherSDR's `#ifdef AETHER_GPU_SPECTRUM` path which sets
    // `m_wfTexFullUpload = true`); m_wfLastUploadedRow alone leaves the
    // bottom scanline stale because the incremental loop exits before
    // uploading row texH-1.
#ifdef NEREUS_GPU_SPECTRUM
    m_wfTexFullUpload = true;
    m_wfLastUploadedRow = -1;
#endif
    update();
}

// From AetherSDR SpectrumWidget.cpp:705-718 [@0cd4559]
void SpectrumWidget::setWaterfallLive(bool live)
{
    if (m_wfLive == live) {
        return;
    }
    if (live) {
        m_wfHistoryOffsetRows = 0;
    }
    m_wfLive = live;
    rebuildWaterfallViewport();
    markOverlayDirty();
}

int SpectrumWidget::waterfallStripWidth() const
{
    // Live: width matches the dBm strip column for visual continuity.
    // Paused: widens to fit absolute UTC labels ("-HH:mm:ssZ").
    // From AetherSDR SpectrumWidget.cpp:716-719 [@0cd4559]
    constexpr int kPausedStripW = 72;
    return m_wfLive ? kDbmStripW : kPausedStripW;
}

// From AetherSDR SpectrumWidget.cpp:720-725 [@0cd4559]
QRect SpectrumWidget::waterfallTimeScaleRect(const QRect& wfRect) const
{
    const int stripWidth = waterfallStripWidth();
    const int stripX = wfRect.right() - stripWidth + 1;
    return QRect(stripX, wfRect.top(), stripWidth, wfRect.height());
}

// From AetherSDR SpectrumWidget.cpp:728-735 [@0cd4559]
//   adapter: NereusSDR uses kFreqScaleH = 28 (vs AetherSDR's 20),
//   button Y inset adjusted accordingly.
QRect SpectrumWidget::waterfallLiveButtonRect(const QRect& wfRect) const
{
    const QRect strip = waterfallTimeScaleRect(wfRect);
    // Button width tracks the strip column so it fits without clipping:
    // 32x16 in live mode (strip is 36 px wide, matches upstream), 40x20
    // when paused (strip widens to 72 px and the user is actively trying
    // to click it to resume — bigger target is friendlier).
    const int buttonW = m_wfLive ? 32 : 40;
    const int buttonH = m_wfLive ? 16 : 20;
    const int padding = m_wfLive ? 2 : 4;
    const int buttonX = strip.right() - buttonW - 2;
    const int buttonY = wfRect.top() - kFreqScaleH + padding;
    return QRect(buttonX, buttonY, buttonW, buttonH);
}

// Sub-epic E — flush ring buffer + force live. Called from clearDisplay()
// and from setFrequencyRange's largeShift branch (NereusSDR divergence;
// see plan §authoring-time #3).
void SpectrumWidget::clearWaterfallHistory()
{
    traceRxHistoryEvent(RxHistoryEvent::WaterfallClear);
    if (!m_waterfallHistory.isNull()) {
        m_waterfallHistory.fill(Qt::black);
    }
    // Codex P2 (PR #140): also clear the live viewport + reset its write
    // head so a disconnect-flush actually shows a clean waterfall on
    // reconnect, instead of stale rows from the previous session rolling
    // off one frame at a time. The largeShift path's reprojectWaterfall
    // already reallocates m_waterfall, so this is a no-op there; the
    // disconnect path (MainWindow → connectionStateChanged) had no
    // preceding reset.
    if (!m_waterfall.isNull()) {
        m_waterfall.fill(Qt::black);
    }
    m_wfWriteRow = 0;
    std::fill(m_wfHistoryTimestamps.begin(), m_wfHistoryTimestamps.end(), 0);
    std::fill(m_wfHistoryWindows.begin(), m_wfHistoryWindows.end(), WaterfallRowWindow{});
    m_wfHistoryWriteRow = 0;
    m_wfHistoryRowCount = 0;
    m_wfHistoryOffsetRows = 0;
    m_wfLive = true;
    markOverlayDirty();
}

// Phase 3F Sub-Epic F Task 6: store the latest per-ADC wideband bins.
//
// Still no update() here. The display timer drives repaints at a fixed rate
// (m_displayTimer, 30 fps by default) and the next tick picks these up, so
// asking for a GPU pass per wideband frame would only add cost.
void SpectrumWidget::setWidebandBins(int adcIndex, const QVector<float>& dbmBins)
{
    if (adcIndex == 0) {
        m_widebandBinsAdc0 = dbmBins;
    } else if (adcIndex == 1) {
        m_widebandBinsAdc1 = dbmBins;
    }
}

// Existing public display helpers delegate to the Core reference shared
// with remote endpoint production. Arithmetic and source explanations live
// in WidebandDisplayReference.cpp; local rendering and station offset
// ownership are unchanged.
float SpectrumWidget::widebandFftNormalisationDb()
{
    return NereusSDR::widebandFftNormalisationDb();
}

float SpectrumWidget::widebandBandwidthNormalisationDb(
    SpectrumDetector detector) const
{
    return NereusSDR::widebandBandwidthNormalisationDb(
        m_widebandAdcRateHz, binWidthHz(), m_fftWindowEnb, detector);
}

float SpectrumWidget::widebandTotalCalibrationDb(SpectrumDetector detector) const
{
    return NereusSDR::widebandRelativeReferenceDb(
        m_widebandAdcRateHz, binWidthHz(), m_fftWindowEnb, detector);
}

void SpectrumWidget::setWidebandAdcIndex(int adcIndex)
{
    // Two ADC slots is the ceiling for every supported SKU; anything else is
    // a resolution failure upstream and must not silently select ADC1.
    m_widebandAdcIndex = (adcIndex == 1) ? 1 : 0;
}

void SpectrumWidget::setWidebandAdcRateHz(double rateHz)
{
    if (rateHz > 0.0) {
        m_widebandAdcRateHz = rateHz;
    }
}

// How far out the operator is allowed to zoom.
//
// Bench 2026-08-08: this is why "Extended view (wideband wings)" did nothing
// even with the paint implemented. BOTH zoom gestures clamped the visible
// bandwidth to m_sampleRateHz (the wheel at wheelEvent, the frequency-scale
// drag at mouseMoveEvent), and m_extendedMode's only trigger is
// `m_bandwidthHz > m_sampleRateHz`. The mode was unreachable from any
// operator gesture: the toggle enabled a state the UI could not enter.
//
// The extended ceiling is the wideband ADC's Nyquist because that is exactly
// the span wing data exists for -- WidebandFftEngine emits bins covering
// 0..adcRate/2 (matching Thetis wbDisplay.cs:4680-4704 [v2.10.3.15], where
// no clipping and no zoom leave low_freq = 0 and high_freq = sample_rate/2).
// Zooming past it would only add panel that can never be filled.
//
// With extended view switched off the ceiling stays exactly where it was, so
// the operator override still pins a pan to its DDC.
double SpectrumWidget::maxZoomOutBandwidthHz() const
{
    if (!m_extendedViewAllowed || m_sampleRateHz <= 0.0) {
        return m_sampleRateHz;
    }
    if (m_remoteSpectrum) {
        return m_remoteWidebandAvailable && m_remoteWidebandAdcRateHz > 0.0
            ? std::max(m_sampleRateHz, m_remoteWidebandAdcRateHz / 2.0)
            : m_sampleRateHz;
    }
    return std::max(m_sampleRateHz, m_widebandAdcRateHz / 2.0);
}

void SpectrumWidget::setDisplayWindowClamped(double centreHz,
                                             double requestedSpanHz)
{
    const double ceiling = maxZoomOutBandwidthHz();
    double span = requestedSpanHz;
    if (span <= 0.0 || (ceiling > 0.0 && span > ceiling)) {
        span = ceiling;
    }
    if (span > 0.0) {
        if (m_remoteSpectrum) {
            setDisplayWindowPreservingHistory(centreHz, span);
        } else {
            setFrequencyRange(centreHz, span);
        }
    }
}

// Phase 3F Sub-Epic F Task 7 geometry, finished 2026-08-08.
//
// The DDC covers m_ddcCenterHz +/- m_sampleRateHz/2. Mapped into the window
// m_centerHz +/- m_bandwidthHz/2, that is the listenable island; everything
// else is wing. Not extended -> the island is the whole panel, so every
// caller degrades to the pre-existing behaviour by construction.
// Clipped half-span, not the full rate: the outer kDdcClipFraction of the DDC
// is filter skirt and is not drawn. Three things have to agree on this number
// or the display contradicts itself in the 4% strips, so there is one
// definition of it. See the header.
double SpectrumWidget::ddcIslandHalfSpanHz() const
{
    return m_sampleRateHz * (0.5 - kDdcClipFraction);
}

std::pair<int, int> SpectrumWidget::listenableIslandPixels(int displayWidth) const
{
    if (displayWidth <= 0) {
        return {0, -1};
    }
    if (!m_extendedMode || m_sampleRateHz <= 0.0 || m_bandwidthHz <= 0.0) {
        return {0, displayWidth - 1};
    }

    // Has to agree with the bin range updateSpectrumLinear feeds the
    // detector, or the island's signals would land at the wrong frequencies.
    const double ddcHalfSpanHz = ddcIslandHalfSpanHz();
    const double displayLowHz = m_centerHz - m_bandwidthHz / 2.0;
    const double ddcLowHz     = m_ddcCenterHz - ddcHalfSpanHz;
    const double ddcHighHz    = m_ddcCenterHz + ddcHalfSpanHz;
    const double hzPerPixel   = m_bandwidthHz / static_cast<double>(displayWidth);

    // No overlap at all: the operator has panned far enough into a wing that
    // the DDC is off the window entirely. Say so with an empty span rather
    // than clamping both ends together, which manufactured a one-pixel island
    // at whichever edge was nearest and put DDC bins on a pixel the DDC does
    // not cover. Callers read an empty span as "the wideband plane owns every
    // pixel". Found by Codex on PR #318.
    const double displayHighHz = m_centerHz + m_bandwidthHz / 2.0;
    if (ddcHighHz < displayLowHz || ddcLowHz > displayHighHz) {
        return {0, -1};
    }

    int first = static_cast<int>(std::floor((ddcLowHz - displayLowHz) / hzPerPixel));
    int last  = static_cast<int>(std::ceil((ddcHighHz - displayLowHz) / hzPerPixel)) - 1;

    first = std::clamp(first, 0, displayWidth - 1);
    last  = std::clamp(last, 0, displayWidth - 1);
    if (last < first) {
        last = first;
    }
    return {first, last};
}

// Phase 3F Sub-Epic F Task 8, finished 2026-08-08.
//
// Frequency axis from Thetis wbDisplay.cs:4680-4704 [v2.10.3.15]: for real
// samples `bin_width = sample_rate / fft_size`, and with no clipping or zoom
// the usable span works out to 0..sample_rate/2 -- which is exactly the range
// WidebandFftEngine documents. NereusSDR's engine drops the DC bin, so
// dbmBins[i] is the FFT's bin i+1 and sits at (i+1) * bin_width.
//
// dB scale from Thetis wbDisplay.cs:2106 [v2.10.3.15]:
//   max = current_display_data[i];
//   max += rx_display_cal_offset;
//   max += preamp_offset;
void SpectrumWidget::fillWidebandWings(QVector<float>& linearPixels,
                                       int islandFirstPx, int islandLastPx,
                                       double dbmOffset,
                                       SpectrumDetector detector) const
{
    const int width = static_cast<int>(linearPixels.size());
    if (width <= 0 || m_bandwidthHz <= 0.0) {
        return;
    }
    // Ordinary (non-extended) pan: the island IS the panel, so there are no
    // wing pixels. Returning here rather than falling into a loop that skips
    // every pixel keeps the common path free of per-frame work.
    if (islandFirstPx <= 0 && islandLastPx >= width - 1) {
        return;
    }

    const QVector<float>& bins = (m_widebandAdcIndex == 1) ? m_widebandBinsAdc1
                                                           : m_widebandBinsAdc0;
    // Bin SPACING for the frequency mapping (where a bin sits). The
    // bandwidth it integrates is a separate, larger figure and lives in
    // widebandBandwidthNormalisationDb().
    const double binWidthHz =
        m_widebandAdcRateHz
        / static_cast<double>(WidebandFftEngine::kFftSize);
    const double displayLowHz = m_centerHz - m_bandwidthHz / 2.0;
    const double hzPerPixel   = m_bandwidthHz / static_cast<double>(width);

    // The avenger turns its input into dBm as 10*log10(linear * 10^(off/10)),
    // so a wing pixel that should read `db` dBm has to go in as
    // 10^((db - off)/10). Folding dbmOffset out here is what puts the wings
    // and the island on one scale across the boundary.
    const auto toLinear = [dbmOffset](double db) {
        return std::pow(10.0, (db - dbmOffset) / 10.0);
    };
    // Bottom of the current window rather than a fixed constant, so a wing
    // with no data reads as "nothing here" at any ref level / range.
    const double floorLinear = toLinear(m_refLevel - m_dynamicRange);

    const bool haveBins = !bins.isEmpty() && binWidthHz > 0.0;
    // Hoisted: it depends only on the detector and the two rates, none of
    // which move inside the loop.
    const float calDb = widebandTotalCalibrationDb(detector);

    for (int x = 0; x < width; ++x) {
        if (x >= islandFirstPx && x <= islandLastPx) {
            continue;                       // island belongs to the DDC plane
        }
        if (!haveBins) {
            linearPixels[x] = static_cast<float>(floorLinear);
            continue;
        }

        const double pixelLowHz  = displayLowHz + x * hzPerPixel;
        const double pixelHighHz = pixelLowHz + hzPerPixel;

        // -1 undoes the dropped DC bin: dbmBins[i] <-> (i+1) * binWidthHz.
        int firstBin = static_cast<int>(std::floor(pixelLowHz / binWidthHz)) - 1;
        int lastBin  = static_cast<int>(std::ceil(pixelHighHz / binWidthHz)) - 1;
        firstBin = std::max(firstBin, 0);
        lastBin  = std::min(lastBin, static_cast<int>(bins.size()) - 1);

        if (firstBin > lastBin) {
            // Window reaches past the ADC's Nyquist (or below DC): there is
            // no wideband data to show there, so say so rather than smear
            // the nearest bin across it.
            linearPixels[x] = static_cast<float>(floorLinear);
            continue;
        }

        // Peak across the bins under this pixel. Thetis's wideband analyzer
        // is configured for a peak detector too (wbDisplay.cs:4711
        // SetAnalyzer, and its per-pixel loop takes `max`), so a narrow
        // carrier survives decimation instead of averaging away.
        float peakDb = bins[firstBin];
        for (int b = firstBin + 1; b <= lastBin; ++b) {
            peakDb = std::max(peakDb, bins[b]);
        }
        linearPixels[x] = static_cast<float>(
            toLinear(static_cast<double>(peakDb + calDb)));
    }
}

// Phase 3F Sub-Epic F Tasks 7-10: extended-view policy + derived state.
// The operator toggle controls permission; zoom and DDC rate decide whether
// wideband wings are actually needed. State changes notify consumers so they
// can flip SliceModel::widebandExtensionRequested, which triggers the Task 11
// chain (Alex BPF bypass + P2 CmdGeneral byte 23 wideband-enable +
// radio starts streaming wideband packets). The actual paint hook
// for rendering the stored wideband bins as a background fill is
// deferred to a post-bench polish iteration; for now the flag drives
// the data-flow side only.
void SpectrumWidget::setExtendedViewAllowed(bool allowed)
{
    if (m_extendedViewAllowed == allowed) {
        recomputeExtendedMode();
        return;
    }
    m_extendedViewAllowed = allowed;

    // Withdrawing permission has to pull the CURRENT span back inside the
    // DDC, not merely lower the ceiling for the next gesture.
    //
    // maxZoomOutBandwidthHz is consulted by the wheel and the frequency-scale
    // drag and by nothing else, so an operator who zoomed out past the DDC
    // and then switched Extended view off kept the wide span with
    // m_extendedMode cleared. The next frame takes the ordinary path, treats
    // the whole panel as island, and stretches the DDC's bins across a much
    // wider scale with no wings to explain it: the exact stretched-island
    // defect this sub-epic set out to fix, reachable through the toggle that
    // is supposed to be the way out of it. Found by Codex on PR #318.
    if (!allowed && m_sampleRateHz > 0.0 && m_bandwidthHz > m_sampleRateHz) {
        applyViewWindowForExtendedClamp(m_sampleRateHz);
    }

    recomputeExtendedMode();
}

// The clamp above, kept separate so the write goes through one place and
// carries the repaint + history handling every other span change gets.
void SpectrumWidget::applyViewWindowForExtendedClamp(double bandwidthHz)
{
    if (m_remoteSpectrum) {
        setDisplayWindowPreservingHistory(m_centerHz, bandwidthHz);
    } else {
        setFrequencyRange(m_centerHz, bandwidthHz);
    }
    emit bandwidthChangeRequested(bandwidthHz);
}

void SpectrumWidget::applyOperatorZoom(double centreHz, double bandwidthHz)
{
    // The Ctrl+wheel branch of wheelEvent(), for a zoom button.
    const double newBw = std::clamp(bandwidthHz, 1000.0, maxZoomOutBandwidthHz());
    applyViewWindow(centreHz, newBw);
    emit centerChanged(m_centerHz);
    emit bandwidthChangeRequested(newBw);
    updateVfoPositions();
    recomputeExtendedMode();
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
#endif
    update();
}

void SpectrumWidget::zoomBy(double factor)
{
    if (!(factor > 0.0) || m_bandwidthHz <= 0.0) { return; }
    // Recentre on the VFO, as the Ctrl+wheel zoom does.
    applyOperatorZoom(m_vfoHz, m_bandwidthHz * factor);
}

void SpectrumWidget::zoomToSegment()
{
    if (!m_bandPlanMgr) { return; }
    const double vfoMhz = m_vfoHz / 1.0e6;
    for (const BandSegment& seg : m_bandPlanMgr->segments()) {
        if (vfoMhz >= seg.lowMhz && vfoMhz <= seg.highMhz && seg.highMhz > seg.lowMhz) {
            applyOperatorZoom((seg.lowMhz + seg.highMhz) * 0.5e6,
                              (seg.highMhz - seg.lowMhz) * 1.0e6);
            return;
        }
    }
}

void SpectrumWidget::zoomToBand()
{
    if (!m_bandPlanMgr) { return; }
    const Band band = bandFromFrequency(m_vfoHz);
    double lowMhz = 0.0;
    double highMhz = 0.0;
    for (const BandSegment& seg : m_bandPlanMgr->segments()) {
        const double mid = (seg.lowMhz + seg.highMhz) * 0.5e6;
        if (bandFromFrequency(mid) != band) { continue; }
        if (highMhz <= lowMhz) {
            lowMhz = seg.lowMhz;
            highMhz = seg.highMhz;
        } else {
            lowMhz = std::min(lowMhz, seg.lowMhz);
            highMhz = std::max(highMhz, seg.highMhz);
        }
    }
    const double vfoMhz = m_vfoHz / 1.0e6;
    if (highMhz <= lowMhz || vfoMhz < lowMhz || vfoMhz > highMhz) { return; }
    applyOperatorZoom((lowMhz + highMhz) * 0.5e6, (highMhz - lowMhz) * 1.0e6);
}

void SpectrumWidget::recomputeExtendedMode()
{
    const bool actual = m_remoteSpectrum
        ? m_extendedViewAllowed && m_remoteWidebandAvailable
            && m_remoteWidebandActive
        : m_extendedViewAllowed && m_sampleRateHz > 0.0
            && m_bandwidthHz > m_sampleRateHz;
    if (m_extendedMode == actual) { return; }
    m_extendedMode = actual;
    // Remote demand belongs to the authenticated subscription. Emitting this
    // signal would route the accepted Core state into the remote RadioModel's
    // local wideband intent through MainWindow.
    if (!m_remoteSpectrum) {
        emit widebandExtensionStateChanged(actual);
    }
    // Island bounds live in the cached overlay, so a context-only active
    // transition must invalidate it even when accepted geometry did not move.
    markOverlayDirty();
    update();
}

// From AetherSDR SpectrumWidget.cpp:951-1000 [@0cd4559]
//   adapter: NereusSDR uses Hz throughout (upstream uses MHz). Both the
//   live waterfall ring buffer and the long-history ring buffer are
//   reprojected so a small pan/zoom preserves the visible content.
void SpectrumWidget::reprojectWaterfall(double oldCenterHz, double oldBandwidthHz,
                                        double newCenterHz, double newBandwidthHz)
{
    if (oldBandwidthHz <= 0.0 || newBandwidthHz <= 0.0) {
        return;
    }

    traceRxHistoryEvent(RxHistoryEvent::RfReproject, -1, 0, {},
                         oldCenterHz, oldBandwidthHz, newCenterHz, newBandwidthHz);
    if (!m_waterfallHistory.isNull() && m_wfHistoryRowCount > 0) {
        // Different RX/TX windows coexist in this ring. Project each row
        // from its captured window; stretching the whole history image
        // would relabel the TX rows and irreversibly crop the RX rows.
        rebuildWaterfallViewport(newCenterHz, newBandwidthHz);
        return;
    }
    const double oldStartHz = oldCenterHz - oldBandwidthHz / 2.0;
    const double oldEndHz   = oldCenterHz + oldBandwidthHz / 2.0;
    const double newStartHz = newCenterHz - newBandwidthHz / 2.0;
    const double newEndHz   = newCenterHz + newBandwidthHz / 2.0;
    const double overlapStartHz = std::max(oldStartHz, newStartHz);
    const double overlapEndHz   = std::min(oldEndHz, newEndHz);

    auto reprojectImage = [&](QImage& image) {
        if (image.isNull()) {
            return;
        }
        const int imageWidth = image.width();
        const int imageHeight = image.height();
        if (imageWidth <= 0 || imageHeight <= 0) {
            return;
        }

        QImage reprojected(imageWidth, imageHeight, QImage::Format_RGB32);
        reprojected.fill(Qt::black);

        if (overlapEndHz > overlapStartHz) {
            const double srcLeft  = (overlapStartHz - oldStartHz) / oldBandwidthHz * imageWidth;
            const double srcRight = (overlapEndHz   - oldStartHz) / oldBandwidthHz * imageWidth;
            const double dstLeft  = (overlapStartHz - newStartHz) / newBandwidthHz * imageWidth;
            const double dstRight = (overlapEndHz   - newStartHz) / newBandwidthHz * imageWidth;

            if (srcRight > srcLeft && dstRight > dstLeft) {
                QPainter painter(&reprojected);
                painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
                painter.drawImage(QRectF(dstLeft, 0.0, dstRight - dstLeft, imageHeight),
                                  image,
                                  QRectF(srcLeft, 0.0, srcRight - srcLeft, imageHeight));
            }
        }
        image = std::move(reprojected);
    };

    reprojectImage(m_waterfall);
    reprojectImage(m_waterfallHistory);

    // Two-sentinel pattern matches rebuildWaterfallViewport (see Task 3
    // review): m_wfTexFullUpload routes the GPU upload through the full
    // path, m_wfLastUploadedRow = -1 forces every scanline to be re-sent.
    // Skipping either leaves the bottom row stale on the next frame.
#ifdef NEREUS_GPU_SPECTRUM
    m_wfLastUploadedRow = -1;
    m_wfTexFullUpload   = true;
#endif
}

// ---- Waterfall threshold composition ----
// Mirrors Thetis display.cs:6575-6594 [v2.10.3.13]: persistent user
// fields (waterfall_low/high_threshold) are seeded into per-render
// locals, then AGC / NF-AGC / Clarity override only the locals.  The
// persistent user fields stay untouched — exactly the bug that
// caused issue #230 before this split landed.
//
// In NereusSDR, "locals" become member fields (m_wfActiveLow/High) so
// external runtime layers (Clarity, between-row signal updates) can
// stick a value that survives until the next row push.  AGC's
// running-envelope state (WaterfallLevelFollower's) is the equivalent of
// Thetis's _RX1waterfallPreviousMinValue field.
void SpectrumWidget::composeWaterfallActiveThresholds(const QVector<float>& wfPixelsDbm)
{
    if (wfPixelsDbm.isEmpty()) { return; }

    // 3M-5b: AGC, NF-AGC and Clarity are RX-only. TX uses the static pair
    // from Setup -> Display -> TX; per Thetis display.cs:6506-6595
    // [v2.10.3.13+501e3f51] the TX path has no per-frame tracking at all.
    //
    // Revive merge: 3M-5b expressed this as `!isTx` on each block back when
    // the composition was inline in pushWaterfallRow. Issue #230 moved the
    // composition here, so the gate becomes one early return -- and gains
    // something the inline version did not have. Returning before the AGC
    // follower runs leaves its running envelope holding the last RX values,
    // so unkeying resumes the RX waterfall where it left off. The inline
    // gates skipped the THRESHOLD writes but the follower state lived in
    // the same block, so this is strictly the safer shape.
    //
    // dbmToRgb does not read the active mirror while m_moxOverlay is set,
    // so nothing downstream needs the values this would have written.
    if (m_moxOverlay) { return; }

    // A remote window's AGC and NF-AGC colour against the Core's levels
    // (setCoreWaterfallLevelsAvailable): the stored levels until the first
    // arrive, and the local follower does not run.
    if (coreWaterfallLevelsInUse()) {
        m_wfActiveLowThreshold = m_coreWfLevels ? m_coreWfLevels->first : m_wfLowThreshold;
        m_wfActiveHighThreshold = m_coreWfLevels ? m_coreWfLevels->second : m_wfHighThreshold;
        return;
    }

    // The composition itself (the Thetis display.cs:6575 [v2.10.3.13] seed,
    // the Phase 3G-9c AGC follower and the Task 2.8 NF-AGC) moved to
    // core/spectrum/DisplayFollowers (WaterfallLevelFollower) so the Core
    // composes an app's waterfall levels the same way.
    //
    // Note: "Use spectrum min/max" no longer mutates here.  Thetis
    // wires that flag via setWaterfallGainsIfLinkedToSpectrum
    // (console.cs:9094-9108 [v2.10.3.13]) — the grid-change handler
    // calls the persistent property setter once per grid change, not
    // per render frame.  NereusSDR's port lives in setDbmRange() +
    // setWfUseSpectrumMinMax().  Per-frame mutation here was the
    // issue #230 source.
    WaterfallLevelSettings settings;
    settings.lowDbm = m_wfLowThreshold;
    settings.highDbm = m_wfHighThreshold;
    settings.agc = m_wfAgcEnabled;
    settings.noiseFloorAgc = m_wfNfAgcEnabled;
    settings.noiseFloorAgcOffsetDb = m_wfNfAgcOffsetDb;
    settings.clarityActive = m_clarityActive;
    m_wfLevels.compose(wfPixelsDbm, settings,
                       m_wfActiveLowThreshold, m_wfActiveHighThreshold);
}

// ---- Waterfall row push ----
// From Thetis Display.cs:7719 -- new row at top, old content shifts down.
// Ring buffer equivalent: decrement write pointer so newest row is always
// at m_wfWriteRow, and display reads forward from there (wrapping).
//
// Input is the post-pipeline waterfall row (display-pixel dBm, length
// m_wfRenderedPixels.size() == displayWidth) produced by the waterfall
// detector + avenger in updateSpectrumLinear().  AGC + NF-AGC + threshold
// compute iterate display pixels per Thetis Display.cs:6713-6738
// [v2.10.3.13] (waterfall_data[i] indexed by pixel).
void SpectrumWidget::pushWaterfallRow(const QVector<float>& wfPixelsDbm)
{
    if (m_waterfall.isNull() || wfPixelsDbm.isEmpty()) {
        return;
    }

    // Task 2.8: Stop-on-TX -- skip while this pan is transmitting and the
    // feature is on. From Thetis display.cs:7601-7604 [v2.10.3.15]:
    //   bool stopWaterfallOnTx = (rx == 1 && m_bStopRX1WaterfallOnTX && local_mox) ||
    //                            (rx == 2 && m_bStopRX2WaterfallOnTX && local_mox);
    //   if (!stopWaterfallOnTx) { ... draw the row ... }
    // local_mox is this receiver transmitting: m_moxOverlay, which
    // MoxDisplayController sets on the transmitting pan for a local radio
    // (MoxController::moxStateChanged) and for a Core (txState / transmitting)
    // alike. It used to read the peak hold's transmit flag, which nothing set.
    if (m_wfStopOnTx && (m_moxOverlay || m_txActiveForTest)) {
        return;
    }

    // Thetis display.cs:6588-6605 [v2.10.3.15] adds RX1Offset to each
    // receive waterfall sample before noise-floor and colour processing.
    //MW0LGE [2.9.0.7]
    // Remote rows already carry Core calibration; txWaterfallRow() already
    // added the keyed offset. Only the local receive row needs it here.
    QVector<float> calibratedPixelsDbm = wfPixelsDbm;
    if (!m_remoteSpectrum && !m_moxOverlay) {
        const float offset = displayCalOffsetDb();
        if (offset != 0.0f) {
            for (float& dbm : calibratedPixelsDbm) {
                dbm += offset;
            }
        }
    }

    // 3DSS: feed the stacked-trace ring from the same call, downstream of the
    // stop-on-TX gate above, so the perspective stack and the flat waterfall
    // beneath it advance and freeze in lockstep. Teeing at the WaterfallTicker
    // callback instead would sit upstream of that gate and let the 3D surface
    // keep scrolling through an over. Task 24: the tee is now
    // accumulateDssRow(), which folds rows per effectiveDssRowDivider()
    // before handing a peak-held row to pushDssRow() -- see that method.
    if (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) {
        accumulateDssRow(calibratedPixelsDbm);
    }

    // 2026-05-25 KG4VCF bench fix: cadence is now driven by
    // m_wfPushTimer (set up in the ctor) at strictly m_wfUpdatePeriodMs
    // intervals, decoupled from FFT arrival.  The legacy inline rate-
    // limit ("drop if too soon since last push") is removed because it
    // could clip an occasional timer tick due to QTimer / wall-clock
    // drift, leaving a visible gap in the waterfall scroll.  m_wfLastPushMs
    // is still updated for any observability (paint debug, telemetry).
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_wfLastPushMs = now;

    // Issue #230 fix: threshold composition moved out — writes go to
    // the render-active mirror (m_wfActiveLow/High), never the
    // persisted user fields. Thetis-faithful per Thetis
    // display.cs:6575-6594 [v2.10.3.13].
    composeWaterfallActiveThresholds(calibratedPixelsDbm);

    const int n = calibratedPixelsDbm.size();
    int h = m_waterfall.height();
    // Decrement write pointer so newest row is always at m_wfWriteRow.
    m_wfWriteRow = (m_wfWriteRow - 1 + h) % h;

    int w = m_waterfall.width();
    QRgb* scanline = reinterpret_cast<QRgb*>(m_waterfall.scanLine(m_wfWriteRow));
    // Map source-pixel range to scanline pixels.  When pipeline displayWidth
    // matches m_waterfall.width() (typical case: both = panel width minus
    // strip), this is a 1:1 copy; if widths diverge (e.g. resize race),
    // proportional sampling preserves visual continuity.
    const float pxScale = static_cast<float>(n) / static_cast<float>(w);
    for (int x = 0; x < w; ++x) {
        int srcPx = static_cast<int>(static_cast<float>(x) * pxScale);
        srcPx = qBound(0, srcPx, n - 1);
        scanline[x] = dbmToRgb(calibratedPixelsDbm[srcPx]);
    }

    // ── Sub-epic E: mirror the just-written row into the history ring ───
    // From AetherSDR SpectrumWidget.cpp:2808-2812 [@0cd4559]
    //   adapter: NereusSDR has a single FFT-derived path (no native tile
    //   path), so we always use QDateTime::currentMSecsSinceEpoch().
    {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        appendHistoryRow(scanline, nowMs);
        if (!m_wfLive) {
            // Paused: don't show the new row — auto-bump in appendHistoryRow
            // already shifted offset, just rebuild the viewport.
            rebuildWaterfallViewport();
            markOverlayDirty();
        }
    }
}

// ---- 3DSS floor anchoring, wide channel feed ----

// The 3DSS surface baseline. Anchored to the measured noise floor so the
// stack keeps a constant apparent height as band conditions move, offset
// downward by the 3D Floor control to expose more or less noise texture.
float SpectrumWidget::dssFloorDbm() const
{
    // m_nfLerpAverage is this widget's smoothed measured noise floor, the
    // same quantity NoiseFloorTracker::noiseFloor() exposes (both are the
    // Thetis display.cs:4628 lerp average). m_nfFftBinAverage is the
    // per-frame value and would make the surface jitter every frame.
    return m_noiseFloor.lerpAverage() - static_cast<float>(m_dssFloorDepth);
}

float SpectrumWidget::dssSpanDb() const
{
    // The dBm display span. This widget stores the range as a top
    // (m_refLevel) plus a depth (m_dynamicRange), not as a floor/ceiling
    // pair, so the span is m_dynamicRange directly.
    return std::max(1.0f, m_dynamicRange);
}

// Task 11 fast-follow. Upstream inlines std::round(span*2.0f)/2.0f
// independently at each of drawDbmScale3D's, buildDssImage's, and
// writeDssMeshUbo's own call sites (three separate occurrences of the
// identical formula, not a shared function -- see drawDbmScale3D's own
// ported doc comment, "buildDssImage / renderGpuFrame use
// std::round(span*2)/2"). NereusSDR names it once, here, and routes all
// three consumers through it: a code review caught writeDssMeshUbo's
// rangeDb field still writing the RAW dssSpanDb() while the scale and the
// CPU fallback both rounded, so whenever the configured dBm range was not
// already an exact 0.5 dB multiple the GPU mesh's front-ridge height
// mapping (dss_mesh.vert's rangeDb) disagreed with the drawn scale by up
// to 0.25 dB -- several pixels on a tall strip at a narrow range. A single
// named accessor makes that class of drift structurally impossible to
// reintroduce at any one call site without an intentional edit.
float SpectrumWidget::dssRoundedSpanDb() const
{
    return std::round(dssSpanDb() * 2.0f) / 2.0f;
}

// From AetherSDR SpectrumWidget.cpp:14195 [@1872028c] (the writeDssMeshUbo-
// equivalent inline `std::min(rangeDb, DssRenderer::kColorSpanDb)`) and
// SpectrumWidget.cpp:11889-11890 [@1872028c] (buildDssImage()'s own local
// `colorRangeDb`, same expression against the CPU fallback's `rangeDb`).
// Review-round fix: the first pass of this task wrote kDssColorSpanDb
// (formerly kColorSpanDb) unconditionally, uncapped by the actual dBm
// span. At a narrow dBm range (e.g. 20 dB) that stretches the colormap's
// 45 dB aperture across only 20 dB of real signal, washing the surface out
// relative to upstream. The span can only ever narrow the aperture, never
// widen it past kDssColorSpanDb, hence min() rather than the span alone.
float SpectrumWidget::dssColorRangeDb() const
{
    return std::min(dssSpanDb(), kDssColorSpanDb);
}

// Build the wide channel from the off-screen DDC bins of the SAME FFT frame.
//
// The window is sized for the WIDEST angle the slider allows rather than the
// current one, so retained rows stay valid across a runtime angle change and
// moving the slider never forces a re-ingest (design doc section 4.2).
// Returns an empty vector when the view already covers the whole DDC, which
// is the correct "no span available" answer.
QVector<float> SpectrumWidget::buildDssWideRow(
    const QVector<float>& fullBins,
    double& wideCenterMhzOut,
    double& wideBandwidthMhzOut) const
{
    // The core helper is the shared local/remote crop contract. This widget
    // retains ownership of the GUI's maximum-shape choice; core receives that
    // explicit factor and remains QRhi-free.
    const DssWideRow wide = cropDssWideRow(fullBins, {
        m_centerHz,
        m_bandwidthHz,
        m_ddcCenterHz,
        m_sampleRateHz,
        dssMaxRowSpanFactor(dssShapeForAngle(0)),
    });
    wideCenterMhzOut = wide.centreHz / 1.0e6;
    wideBandwidthMhzOut = wide.spanHz / 1.0e6;
    return wide.binsDbm;
}

// From AetherSDR SpectrumWidget.cpp:12743-12784 [@1872028c], minus the
// AETHER_DSS_ROW_SPAN environment-variable override and the "age 0 is not
// authoritative" Flex/Kiwi multi-source caveats: our producer's only row
// source is pushDssRow(), which always attaches a wide slice whenever
// buildDssWideRow() finds one available, so there is no separate producer
// that could append without one while zoomed in.
float SpectrumWidget::dssRowSpanTarget(double targetBandwidthMhz) const
{
    return dssRowSpanFactorFor(
        m_dss.newestWideBandwidthMhz(targetBandwidthMhz),
        targetBandwidthMhz,
        m_dssRowSpan,
        dssShape());
}

// ---- 3DSS slice shadow decals ----

// The shader's shadowBands/shadowStyles arrays are fixed-size (dss_mesh.vert
// / dss_mesh.frag both declare [8]); the builder below truncates to this
// many rather than ever overrunning writeDssMeshUbo()'s UBO write.
// From AetherSDR SpectrumWidget.cpp:14219-14225 [@1872028c]
// (kShadowBandsOffset/kShadowStylesOffset sized off
// SpectrumWidget::kDssMeshShadowSlices [SpectrumWidget.h:2018],
// upstream's own name for this same fixed budget).
static constexpr int kDssShadowSlices = 8;

// Maps every visible slice's passband onto the [0,1] viewport-unit space
// hzToX() uses, for dss_mesh.frag's applySliceShadow() to darken directly
// onto the 3D surface.
//
// From AetherSDR SpectrumWidget.cpp:14216-14366 [@1872028c] (appendShadow /
// writeShadowSlot), simplified to what SliceMarkerGeometry actually carries:
// NereusSDR has no per-slice mode / RTTY mark-space / isActive / markerWidth
// fields at this call site, so unlike upstream this never emits a RTTY
// mark+space cue pair, never emits a band-less cue-only descriptor, and
// never dims an "inactive" slice's alpha independently of an "active" one --
// drawSliceMarker() (the flat 2D marker this decal echoes onto the surface)
// doesn't discriminate active/inactive or per-mode cues either, so none of
// this narrows anything that already existed. Every visible slice therefore
// contributes exactly one band+cue descriptor, using upstream's own
// isActive=true magnitudes (:14263, :14269) since every slice reaching this
// list is, in NereusSDR's simpler model, equally "on screen and current."
QVector<SpectrumWidget::DssShadowBand> SpectrumWidget::buildDssShadowBands() const
{
    QVector<DssShadowBand> out;
    if (!m_threeDSliceDepth || m_bandwidthHz <= 0.0) {
        return out;
    }

    const double lowHz = m_centerHz - m_bandwidthHz / 2.0;
    const auto unitForHz = [&](double hz) {
        return static_cast<float>((hz - lowHz) / m_bandwidthHz);
    };

    // Match drawSliceMarker()'s per-slice VFO cue. The AetherSDR
    // SliceColors.h:15-20 mapping starts with cyan for slice 0 (A).
    for (const SliceMarkerGeometry& g : sliceMarkerGeometry()) {
        if (out.size() >= kDssShadowSlices) {
            break;
        }
        float low  = unitForHz(g.centreHz + g.filterLowHz);
        float high = unitForHz(g.centreHz + g.filterHighHz);
        if (low > high) {
            std::swap(low, high);
        }
        if (high < 0.0f || low > 1.0f) {
            continue;   // passband never touches the visible viewport
        }
        DssShadowBand band;
        band.lowUnit    = low;
        band.highUnit   = high;
        band.centreUnit = unitForHz(g.centreHz);
        // From AetherSDR SpectrumWidget.cpp:14263 [@1872028c]
        // (writeShadowSlot's `active ? 0.42f : 0.17f` band alpha).
        band.alpha       = 0.42f;
        band.cue         = VfoWidget::sliceColor(
            g.flag ? g.flag->sliceIndex() : m_frontSliceIndex);
        // From AetherSDR SpectrumWidget.cpp:14269 [@1872028c]
        // (writeShadowSlot's `active ? 0.36f : 0.11f` cue alpha).
        band.centreAlpha = 0.36f;
        out.append(band);
    }
    return out;
}

// ---- 3DSS row tee ----
// Resamples the same post-pipeline row pushWaterfallRow() just wrote to the
// flat waterfall into the stacked-trace ring, and fills the wide (off-
// screen) channel from the cached full-DDC dBm snapshot (m_lastFullBinsDbm,
// set in updateSpectrumLinear()) via pushRowWithWide. Transmit rows are
// exact-only because updateSpectrumFromTxPixels() has no full-DDC TX frame;
// using the cache while keyed would mix fresh TX pixels with stale RX
// shoulders. Receive rows also fall back to exact-only whenever
// buildDssWideRow() has nothing to offer (not zoomed in, or no FFT frame
// cached yet).
// set in updateSpectrumLinear()) via pushRowWithWide. Falls back to the
// exact-only pushRow() whenever buildDssWideRow() has nothing to offer
// (not zoomed in, or no FFT frame cached yet).
// 3D Speed (Task 24, NereusSDR-original -- design doc section 4.5).
// Folds wfPixelsDbm (and the wide-channel source m_lastFullBinsDbm) into
// the in-progress fold by per-column maximum (peak-hold), so a burst that
// lasts a single waterfall tick still reaches the ring even when several
// ticks are folded into one 3D row -- an average was rejected for exactly
// this reason (it would shrink such a burst by a factor of N and could
// vanish from the 3D surface while still visible in the waterfall).
//
// A size or frequency-frame change on either channel discards whatever was
// accumulated and starts a fresh fold at count 1, rather than combining
// bin indices that now represent different frequencies.
// Integration: the full-DDC snapshot is cropped and calibrated per tick,
// then the actual exact and wide planes are folded. Remote wide planes
// retain their accepted capture metadata through the native presentation queue.
void SpectrumWidget::accumulateDssRow(const QVector<float>& wfPixelsDbm)
{
    // Fold the planes that this tick actually supplied, after calibration.
    // A remote wide plane already carries Core calibration and its capture axes.
    QVector<float> wide;
    double wideCenterMhz = 0.0;
    double wideBandwidthMhz = 0.0;
    const RemoteSpectrumCapture capture = m_remoteSpectrum
        ? m_pendingRemoteCapture : RemoteSpectrumCapture{};
    if (!m_moxOverlay) {
        if (m_remoteSpectrum) {
            wide = m_pendingRemoteWide;
            wideCenterMhz = capture.context().wideCentreHz / 1.0e6;
            wideBandwidthMhz = capture.context().wideSpanHz / 1.0e6;
        } else {
            wide = buildDssWideRow(m_lastFullBinsDbm, wideCenterMhz, wideBandwidthMhz);
            const float offset = displayCalOffsetDb();
            for (float& dbm : wide) { dbm += offset; }
        }
    }
    const bool incompatible = wfPixelsDbm.size() != m_dssFoldRow.size()
        || wide.size() != m_dssFoldFullBins.size()
        || m_centerHz != m_dssFoldCenterHz
        || m_bandwidthHz != m_dssFoldBandwidthHz
        || m_ddcCenterHz != m_dssFoldDdcCenterHz
        || m_sampleRateHz != m_dssFoldSampleRateHz
        || wideCenterMhz != m_dssFoldWideCenterMhz
        || wideBandwidthMhz != m_dssFoldWideBandwidthMhz
        || m_remoteSpectrum != m_dssFoldRemote || m_moxOverlay != m_dssFoldTx
        || (m_remoteSpectrum ? !(capture == m_dssFoldCapture)
                             : m_localSpectrumSource != m_dssFoldLocalSource);
    if (incompatible) {
        // Keep painted history, but do not smooth a new source against it.
        m_dss.resetInputSmoothing();
    }
    if (m_dssFoldCount == 0 || incompatible) {
        m_dssFoldRow = wfPixelsDbm;
        m_dssFoldFullBins = wide;
        m_dssFoldCount = 1;
        m_dssFoldCenterHz = m_centerHz;
        m_dssFoldBandwidthHz = m_bandwidthHz;
        m_dssFoldDdcCenterHz = m_ddcCenterHz;
        m_dssFoldSampleRateHz = m_sampleRateHz;
        m_dssFoldWideCenterMhz = wideCenterMhz;
        m_dssFoldWideBandwidthMhz = wideBandwidthMhz;
        m_dssFoldCapture = capture;
        m_dssFoldRemote = m_remoteSpectrum;
        m_dssFoldTx = m_moxOverlay;
        m_dssFoldLocalSource = m_localSpectrumSource;
    } else {
        for (int i = 0; i < m_dssFoldRow.size(); ++i) {
            m_dssFoldRow[i] = std::max(m_dssFoldRow[i], wfPixelsDbm[i]);
        }
        for (int i = 0; i < wide.size(); ++i) {
            m_dssFoldFullBins[i] = std::max(m_dssFoldFullBins[i], wide[i]);
        }
        ++m_dssFoldCount;
    }

    // A divider lowered mid-fold (down to at or below the current count)
    // pushes right here, on this very tick, rather than waiting out
    // whatever the count target was when the fold started.
    if (m_dssFoldCount >= effectiveDssRowDivider()) {
        pushDssRow(m_dssFoldRow);
        m_dssFoldCount = 0;
    }
}

void SpectrumWidget::pushDssRow(const QVector<float>& wfPixelsDbm)
{
    const double centerMhz    = m_centerHz    / 1.0e6;
    const double bandwidthMhz = m_bandwidthHz / 1.0e6;
    const double wideCenterMhz = m_dssFoldWideCenterMhz;
    const double wideBandwidthMhz = m_dssFoldWideBandwidthMhz;
    // This is the actual calibrated peak-held plane. Never crop or calibrate it again.
    const QVector<float>& wide = m_dssFoldFullBins;
    if (wide.isEmpty()) {
        m_dss.pushRow(wfPixelsDbm, centerMhz, bandwidthMhz);
    } else {
        m_dss.pushRowWithWide(wfPixelsDbm, centerMhz, bandwidthMhz,
                              wide, wideCenterMhz, wideBandwidthMhz);
    }
    ++m_dssRowsPushed;
    m_dssScrollProgressRows = 0.0f;
}

// ---- dBm to waterfall color ----
// Porting from Thetis display.cs:6826-6954 — waterfall color mapping.
// Thetis uses low_threshold and high_threshold (dBm) directly:
//   if (data <= low_threshold) → low_color (black)
//   if (data >= high_threshold) → max color
//   else: overall_percent = (data - low) / (high - low)  → 0.0 to 1.0
// Color gain adjusts high_threshold, black level adjusts low_threshold.
QRgb SpectrumWidget::dbmToRgb(float dbm) const
{
    // From Thetis display.cs:6506-6595 [v2.10.3.13+501e3f51] -- per-frame
    // MW0LGE [2.9.0.7]  [original inline comment from display.cs:6588]
    // MOX-conditional render path.  No state machine; branch is inline
    // per pixel.  When MOX is active, TX-specific thresholds + palette are
    // used instead of RX values.  Black-level / color-gain sliders are NOT
    // applied for TX (Thetis does not expose them per-direction).
    const bool isTx = m_moxOverlay;
    const WfColorScheme scheme = isTx ? m_txWfPalette : m_wfColorScheme;

    float effectiveLow;
    float effectiveHigh;
    if (isTx) {
        // TX: static thresholds from m_txWfLowLevel / m_txWfHighLevel.
        // From Thetis display.cs:6536-6539 [v2.10.3.13+501e3f51]:
        //   low_threshold  = (float)TXWFAmpMin;
        //   high_threshold = (float)TXWFAmpMax;
        //
        // Deliberately NOT the active mirror. The mirror carries AGC /
        // NF-AGC / Clarity output, all of which are RX noise-floor
        // trackers; Thetis's TX path is static thresholds with no
        // per-frame tracking at all. Reading the mirror here would let an
        // RX-derived level set the TX palette.
        effectiveLow  = static_cast<float>(m_txWfLowLevel);
        effectiveHigh = static_cast<float>(m_txWfHighLevel);
    } else {
        // RX: black-level / color-gain adjustment math.
        // Black level slider (0-125): lower = more black, higher = less black.
        // Color gain slider (0-100): shifts high threshold DOWN (more color).
        // From Thetis display.cs:2522-2536 defaults: high=-80, low=-130.
        // Issue #230 fix: read the render-active mirror, not the
        // persistent user fields — AGC / NF-AGC / Clarity drive the
        // active mirror via composeWaterfallActiveThresholds() and
        // setClarityWaterfallThresholds().  Persistent values stay clean.
        //
        // Revive merge: 3M-5b wrote this branch against m_wfLow/HighThreshold,
        // which #230 later split into persisted-vs-active. The RX branch
        // follows #230 onto the mirror; the TX branch above stays on its own
        // static pair, so the two fixes compose rather than collide.
        effectiveLow  = m_wfActiveLowThreshold  + static_cast<float>(125 - m_wfBlackLevel) * 0.4f;
        effectiveHigh = m_wfActiveHighThreshold - static_cast<float>(m_wfColorGain) * 0.3f;
    }
    if (effectiveHigh <= effectiveLow) {
        effectiveHigh = effectiveLow + 1.0f;
    }

    // Everything at or under the floor is the low colour, which on the TX
    // path is a control the operator actually set.
    //
    // From Thetis display.cs:6424-6427 [v2.10.3.15], the TX branch that pairs
    // the threshold with the colour:
    //     low_threshold  = (float)TXWFAmpMin;
    //     high_threshold = (float)TXWFAmpMax;
    //     cScheme        = _tx_color_scheme;
    //     low_color      = waterfall_low_color_tx;
    // consumed at :6766-6768 under `if (waterfall_data[i] <= low_threshold)`
    // (:6764).
    //
    // NereusSDR ported the threshold half and not the colour half. TX Low
    // Color was settable, persisted and read back, and dbmToRgb never looked
    // at it: a bin at or below m_txWfLowLevel produced adjusted == 0 and fell
    // into the palette's first gradient stop. Found by Codex on PR #317.
    //
    // TX only. The RX floor colour is the RX palette's own first stop and
    // has no separate control, so reading one here would invent behaviour
    // rather than restore it.
    if (isTx && dbm <= effectiveLow) {
        return m_txWfLowColor.rgb();
    }

    // From Thetis display.cs:6889-6891
    float range = effectiveHigh - effectiveLow;
    float adjusted = (dbm - effectiveLow) / range;
    adjusted = qBound(0.0f, adjusted, 1.0f);

    // 3M-5c: TX + Custom palette + populated 101-LUT -> direct index into
    // the GradientPickerWidget-built LUT. Mirrors the WaterfallTXGradient
    // consumer at Thetis setup.cs:33314-33322 [v2.10.3.13+501e3f51] which
    // builds Color[101] from GetColourAtPercent(p / 100f) and indexes by
    // percent-of-range. When the LUT hasn't been populated yet (fresh
    // install, user has not opened Setup -> Display -> TX -> Custom) the
    // code falls through to the existing kCustomFallbackStops path so the
    // Custom palette stays usable as a default colour scheme.
    if (isTx && scheme == WfColorScheme::Custom && m_txCustomLutValid) {
        const int idx = qBound(0, static_cast<int>(adjusted * 100.0f + 0.5f), 100);
        return m_txCustomLut[idx];
    }

    // Look up in gradient stops for current color scheme (TX or RX).
    int stopCount = 0;
    const WfGradientStop* stops = wfSchemeStops(scheme, stopCount);
    return interpolateWfGradient(adjusted, stops, stopCount);
}

// ---- 3DSS surface colour (deliberately NOT dbmToRgb) ----
// From AetherSDR SpectrumWidget.cpp:11710-11719 [@1872028c].
QRgb SpectrumWidget::dssStrengthToRgb(float s) const
{
    // gamma in [0.25 .. 4]: gain=100 -> 0.25 (colour lifted to the noise floor),
    // gain=50 -> 1.0 (linear), gain=0 -> 4 (colour only on the strongest peaks).
    const float gamma = std::pow(4.0f, (50.0f - m_dssGain) / 50.0f);
    int n = 0;
    const WfGradientStop* stops = wfSchemeStops(m_wfColorScheme, n);
    return interpolateWfGradient(
        std::pow(std::clamp(s, 0.0f, 1.0f), gamma), stops, n);
}

// From AetherSDR SpectrumWidget.cpp:12727-12728 [@1872028c] -- this is
// uploadDssPaletteLut()'s OWN inline token there (the actual GPU LUT
// re-bake gate), promoted to a named, reusable, publicly-testable method.
//
// NereusSDR divergence, precisely stated: upstream ALSO has a separately
// named 5-field `dssPaletteToken()` member (SpectrumWidget.cpp:11721-11733
// [@1872028c]: "Fold the inputs that define the 3DSS surface colour so the
// cached image recolours when any change. The surface now maps strength
// through the scheme + "3D Gain" (dssStrengthToRgb); the waterfall
// gain/black/min are kept here too since they still affect the
// 2D/waterfall colour path.") -- but that member's one call site,
// buildDssImage() (:11880-11894), belongs to the CPU-fallback cached-image
// path Task 4 explicitly deferred ("image()/rebuild()/m_cache*, deferred to
// Task 10" per docs/attribution/aethersdr-reconciliation.md). This task's
// uploadDssPaletteLut() never calls that 5-field member; it computes the
// inline 2-field token above instead, which is what this function actually
// is. dssStrengthToRgb() never reads m_wfColorGain/m_wfBlackLevel/
// m_wfMinDbm (nor does upstream's), so a 5-field token here would only
// trigger spurious re-bakes -- of a LUT that would come out byte-identical
// -- on every waterfall slider tick and every per-frame floor/range jitter.
// See waterfallKnobs_doNotMove3DColours in tests/tst_dss_palette.cpp. If/
// when Task 10 ports buildDssImage(), it should reconcile with this name
// rather than silently shadowing it with the 5-field upstream meaning.
quint64 SpectrumWidget::dssPaletteToken() const
{
    quint64 t = static_cast<quint64>(m_wfColorScheme);
    t = t * 131 + static_cast<quint64>(m_dssGain);
    return t;
}

// ---- 3DSS CPU fallback surface (Task 10) ----
// From AetherSDR SpectrumWidget.cpp:11880-11894 [@1872028c], with the
// floorDbm parameter dropped: NereusSDR has one floor source (dssFloorDbm()),
// not upstream's per-caller value (m_lastDetectDssFloor in 2D vs
// dssFloorDbm() in 3D -- a distinction that does not apply here, since this
// helper is only ever called while in 3D mode).
const QImage& SpectrumWidget::buildDssImage(const QSize& px, int scaleStripPx)
{
    const float floorDbm = dssFloorDbm();
    // dssRoundedSpanDb() (Task 11 fast-follow): named once so this CPU
    // fallback, the 3D scale, and the GPU mesh's rangeDb uniform all read
    // the identical rounded span rather than three independent inlines.
    const float rangeDb  = dssRoundedSpanDb();

    // Same mapping as the GPU mesh: a stable colour aperture independent of
    // the Ref-level height span, gamma-shaped by "3D Gain". Uses
    // dssColorRangeDb() (Task 9) rather than re-deriving std::min(rangeDb,
    // kDssColorSpanDb) inline as upstream does, so the CPU fallback and the
    // GPU mesh (writeDssMeshUbo()) read the exact same colour aperture.
    const float colorRangeDb = dssColorRangeDb();
    auto palette = [this, floorDbm, colorRangeDb](float dbm) {
        const float r = (colorRangeDb > 0.0f) ? colorRangeDb : 1.0f;
        return dssStrengthToRgb((dbm - floorDbm) / r);
    };
    // zCurve: 0.6f matches writeDssMeshUbo()'s hardcoded value (Task 9) so
    // the CPU fallback lifts the floor band identically to the GPU mesh.
    // bgFill: 0x0a0a14 matches both the pass clear colour and the mesh
    // UBO's bgFill field (writeDssMeshUbo()), for the same reason.
    return m_dss.image(px, scaleStripPx, floorDbm, rangeDb, 0.6f,
                       palette, dssPaletteToken(), QColor(0x0a, 0x0a, 0x14),
                       dssShape(), m_centerHz / 1.0e6,
                       m_bandwidthHz / 1.0e6);
}

// ---- VFO marker + filter passband overlay ----
//
// One marker per hosted slice, each from ITS OWN centre and ITS OWN filter
// edges.
//
// This used to be one marker per pan, derived from m_vfoHz plus the pan's
// single m_filterLowHz/m_filterHighHz pair. Both of those track whichever
// slice most recently reached the pan -- MainWindow pushes setVfoFrequency on
// every slice's frequencyChanged and setFilterOffset on every slice's
// filterChanged, and activating a slice re-pushes both -- so a pan hosting two
// slices shaded exactly one passband and it belonged to the last slice
// touched. Bench-reported 2026-07-28: "The pass band of the second flag
// disappears when not active. Let's keep it."
//
// Filter edges come from the flag, not the pan, because filter width is per
// slice: reusing the pan's pair would draw a 2.7 kHz SSB band around a 500 Hz
// CW slice. VfoWidget already receives both values from its SliceModel
// (MainWindow::createSliceFlag seeds setFilter and re-pushes it on
// filterChanged); it just discarded them before.
//
// Two behaviours are deliberately preserved:
//   - A pan with no flag yet still marks its own VFO. wireSliceToSpectrum
//     seeds setVfoFrequency BEFORE it builds the flag, and a paint can land in
//     that window; dropping the fallback would blank the marker there.
//   - A single-slice pan is unchanged. Its one flag carries the same frequency
//     and the same filter the pan does, so the marker is identical to the
//     pre-split one.
//
// Overlap between two slices' bands is left to whatever QPainter's default
// compositing does with the translucent fill, exactly as a single band is
// composited today. No blend rule, colour or opacity is introduced here.
//
// Marker colours. Every marker used to be drawn in slice A's cyan, so with
// two slices on one pan nothing on the band said which line belonged to which
// flag. Each marker now takes its own slice's colour, and the slices the
// operator has not selected are drawn darker, following AetherSDR's
// drawSliceMarkers, src/gui/SpectrumWidget.cpp:17454-17466 [@9f81dc00]:
//   - centre line and triangle: the slice colour when selected, its dim
//     partner otherwise (sliceColorForOverlay, :7922-7926 [@9f81dc00]);
//   - filter edges: the slice colour when selected, the theme's secondary
//     text colour otherwise.
// That split is AetherSDR's second answer. At [@0cd4559], the revision
// NereusSDR's earlier AetherSDR ports are pinned to, the edges dimmed with the
// line, and inactive passbands became hard to see; AetherSDR commit 7ca6bacf
// (#3547) moved them to the neutral grey so colour, not brightness, is what
// says "not selected". NereusSDR's secondary text colour is
// Style::kTextSecondary. The shaded passband does not come from here at all:
// it stays the operator's m_rxFilterColor (Setup > Display) for every slice.
//
// "Selected" is RadioModel's active slice, one for the whole window, carried
// on each flag (VfoWidget::isActiveSlice). A pan that does not host it draws
// every one of its slices darker, as AetherSDR does. It is deliberately not
// the pan's own front slice (m_frontSliceIndex), which would light one slice
// per pan.
//
// Paint order follows the same routine (:17710-17714 [@9f81dc00]): every
// other slice first, the selected slice last so its marker sits on top of any
// overlap.
QVector<SpectrumWidget::SliceMarkerGeometry>
SpectrumWidget::sliceMarkerGeometry() const
{
    // [original inline comment from src/gui/SpectrumWidget.cpp:17450 [@9f81dc00]]
    // Draw inactive slices first, then active slice on top
    const auto marker = [](double centreHz, int lowHz, int highHz,
                           const VfoWidget* flag, int sliceIndex, bool active) {
        const QColor lineColor = active ? VfoWidget::sliceColor(sliceIndex)
                                        : VfoWidget::sliceDimColor(sliceIndex);
        // [original inline comments from src/gui/SpectrumWidget.cpp:17455-17462 [@9f81dc00]]
        // Bandwidth affordances (passband fill + filter edges) render at full
        // brightness so they stay visible on non-active slices (#3484) — but an
        // inactive slice uses the neutral secondary colour instead of the
        // slice's own colour, so it is COLOUR (not brightness) that signals
        // inactive. That keeps the panadapter from making an inactive slice look
        // TX-selectable (the #2389 confusion concern) while fixing the
        // near-invisible passband. The VFO centre line, triangle, and RIT/XIT
        // lines keep `col` (dimmed when inactive) to preserve the focus cue.
        const QColor edgeColor = active ? VfoWidget::sliceColor(sliceIndex)
                                        : QColor(Style::kTextSecondary);
        return SliceMarkerGeometry{centreHz, lowHz, highHz, flag,
                                   sliceIndex, active, lineColor, edgeColor};
    };

    QVector<SliceMarkerGeometry> out;

    if (m_vfoWidgets.isEmpty()) {
        if (m_ownSliceMarkerPresentationAllowed && m_vfoHz > 0.0) {
            // No flag to say which slice this is. Slice A, selected, is the
            // cyan this marker has always been drawn in.
            out.append(marker(m_vfoHz, m_filterLowHz, m_filterHighHz,
                              nullptr, 0, true));
        }
        return out;
    }

    // QMap, so this walks slices in index order and the paint order is stable
    // frame to frame rather than hash-dependent.
    out.reserve(m_vfoWidgets.size());
    // [original inline comment from src/gui/SpectrumWidget.cpp:17710 [@9f81dc00]]
    // Draw all slices (active last so its marker is on top)
    QVector<SliceMarkerGeometry> selected;
    for (auto it = m_vfoWidgets.constBegin(); it != m_vfoWidgets.constEnd(); ++it) {
        const VfoWidget* flag = it.value();
        if (!flag || !flag->stationPresentationAllowed() || flag->frequency() <= 0.0) {
            continue;
        }
        const SliceMarkerGeometry g = marker(flag->frequency(), flag->filterLow(),
                                             flag->filterHigh(), flag,
                                             flag->sliceIndex(),
                                             flag->isActiveSlice());
        if (g.active) {
            selected.append(g);
        } else {
            out.append(g);
        }
    }
    out += selected;
    return out;
}

void SpectrumWidget::drawVfoMarker(QPainter& p, const QRect& specRect, const QRect& wfRect)
{
    for (const SliceMarkerGeometry& g : sliceMarkerGeometry()) {
        drawSliceMarker(p, specRect, wfRect, g);
    }
    // iPhone app plan Task 78: other devices' slices, under nothing of
    // this window's own (their labels sit at the foot, flags at the top).
    drawForeignMarkers(p, specRect, wfRect);
}

// ── iPhone app plan Task 78: other devices' slices (D46) ─────────────────
//
// NereusSDR-original, drawn as the phone draws them (the several-devices
// design, section 12 item 1): no fill, dashed, and a label at the foot of
// the spectrum instead of a flag, so nothing about them invites a drag.

void SpectrumWidget::setForeignSliceMarkers(const QVector<ForeignSliceMarker>& markers)
{
    m_foreignMarkers = markers;
    m_foreignLabelRects.clear();
    markOverlayDirty();
}

void SpectrumWidget::setOwnSliceMarkerPresentationAllowed(bool allowed)
{
    if (m_ownSliceMarkerPresentationAllowed == allowed) { return; }
    m_ownSliceMarkerPresentationAllowed = allowed;
    markOverlayDirty();
}

QString SpectrumWidget::foreignMarkerLabel(const ForeignSliceMarker& marker)
{
    const QString name = marker.ownerShortName.isEmpty() ? marker.ownerName
                                                         : marker.ownerShortName;
    QString text = name.isEmpty() ? marker.letter
                                  : QStringLiteral("%1 %2").arg(marker.letter, name);
    if (marker.tx) {
        text += QStringLiteral(" TX");
    }
    return text;
}

QString SpectrumWidget::foreignMarkerExplanation(const ForeignSliceMarker& marker)
{
    // Any slice can be taken except while it transmits, so the words are
    // the listener's (SliceAccessMirror::listenerReason, as the Core words
    // its refusal): who controls it, and Take control.
    if (marker.unowned) {
        return QStringLiteral("Nobody controls slice %1. Take control to change it.")
            .arg(marker.letter);
    }
    const QString name = marker.ownerName.isEmpty()
        ? (marker.ownerShortName.isEmpty() ? QStringLiteral("another device")
                                           : marker.ownerShortName)
        : marker.ownerName;
    return QStringLiteral("Slice %1 is controlled by %2. Take control to change it.")
        .arg(marker.letter, name);
}

void SpectrumWidget::drawForeignMarkers(QPainter& p, const QRect& specRect, const QRect& wfRect)
{
    m_foreignLabelRects.clear();
    if (m_foreignMarkers.isEmpty()) {
        return;
    }
    const int bandPlanH = (m_bandPlanMgr && m_bandPlanFontSize > 0)
                          ? (m_bandPlanFontSize + 4) : 0;
    const int specBottom = specRect.bottom() - bandPlanH;
    QFont labelFont = p.font();
    labelFont.setPixelSize(10);
    labelFont.setBold(true);
    const QFontMetrics fm(labelFont);
    static constexpr int kTriHalf = 6;
    static constexpr int kTriH = 10;
    static constexpr int kLabelPadX = 4;
    static constexpr int kLabelH = 14;
    const QColor edgeGrey(0x9a, 0xa4, 0xb0, 170);

    p.save();
    for (const ForeignSliceMarker& m : m_foreignMarkers) {
        if (m.centreHz <= 0.0) {
            continue;
        }
        const int x = hzToX(m.centreHz, specRect);
        int xLo = hzToX(m.centreHz + m.filterLowHz, specRect);
        int xHi = hzToX(m.centreHz + m.filterHighHz, specRect);
        if (xLo > xHi) {
            std::swap(xLo, xHi);
        }
        if (xHi < specRect.left() || xLo > specRect.right()) {
            continue;
        }
        const QColor colour = m.color.isValid() ? m.color : QColor(0x00, 0xd4, 0xff);
        p.setBrush(Qt::NoBrush);

        // Passband edges: dashed grey, no fill.
        QPen edges(edgeGrey, 1.0, Qt::DashLine);
        p.setPen(edges);
        p.drawLine(xLo, specRect.top(), xLo, specBottom);
        p.drawLine(xLo, wfRect.top(), xLo, wfRect.bottom());
        p.drawLine(xHi, specRect.top(), xHi, specBottom);
        p.drawLine(xHi, wfRect.top(), xHi, wfRect.bottom());

        if (x < specRect.left() || x > specRect.right()) {
            continue;
        }
        // Centre line: dashed, in the slice's colour.
        QPen centre(colour, 1.5, Qt::DashLine);
        p.setPen(centre);
        p.drawLine(x, specRect.top(), x, specBottom);
        p.drawLine(x, wfRect.top(), x, wfRect.bottom());

        // Hollow triangle at the top.
        p.setPen(QPen(colour, 1.5));
        QPolygon tri;
        tri << QPoint(x - kTriHalf, specRect.top()) << QPoint(x + kTriHalf, specRect.top())
            << QPoint(x, specRect.top() + kTriH);
        p.drawPolygon(tri);

        // The label at the foot of the spectrum.
        const QString text = foreignMarkerLabel(m);
        const int labelW = fm.horizontalAdvance(text) + 2 * kLabelPadX;
        int left = x - labelW / 2;
        left = std::clamp(left, specRect.left(), std::max(specRect.left(),
                                                           specRect.right() - labelW));
        const QRect label(left, specBottom - kLabelH - 2, labelW, kLabelH);
        p.setFont(labelFont);
        p.setPen(QPen(m.tx ? QColor(0xff, 0x44, 0x44) : colour, 1.0));
        p.setBrush(QColor(0x0f, 0x0f, 0x1a, 210));
        p.drawRoundedRect(label, 3, 3);
        p.setPen(m.away ? edgeGrey : colour);
        p.drawText(label, Qt::AlignCenter, text);
        p.setBrush(Qt::NoBrush);
        m_foreignLabelRects.insert(m.sliceId, label);
    }
    p.restore();
}

// Ported from AetherSDR SpectrumWidget.cpp:3211-3294
// Uses per-slice colors with exact alpha values from AetherSDR.
//
// Body unchanged by the Phase 3F split; it reads its centre and filter edges
// off the passed-in marker instead of the pan's m_vfoHz / m_filterLowHz /
// m_filterHighHz, so the same paint runs once per hosted slice. Its colours
// come off the marker too (see sliceMarkerGeometry for the rule); this used
// to paint every slice in slice A's cyan.
void SpectrumWidget::drawSliceMarker(QPainter& p, const QRect& specRect,
                                     const QRect& wfRect,
                                     const SliceMarkerGeometry& g)
{
    int vfoX = hzToX(g.centreHz, specRect);

    // Filter passband rectangle
    double loHz = g.centreHz + g.filterLowHz;
    double hiHz = g.centreHz + g.filterHighHz;
    int xLo = hzToX(loHz, specRect);
    int xHi = hzToX(hiHz, specRect);
    if (xLo > xHi) {
        std::swap(xLo, xHi);
    }
    int fW = xHi - xLo;

    // Reserve the bandplan strip at the bottom of specRect (drawBandPlan paints
    // colored segments there with transparent gaps; without this clip the
    // translucent passband colour bleeds through the gaps and visually sits
    // "in front of" the bandplan strip).  Same height calc as drawBandPlan:
    // bandH = m_bandPlanFontSize + 4 when a bandplan manager is bound.
    const int bandPlanH = (m_bandPlanMgr && m_bandPlanFontSize > 0)
                          ? (m_bandPlanFontSize + 4) : 0;
    const int specBottomClipped = specRect.bottom() - bandPlanH;
    const int specHeightClipped = std::max(0, specBottomClipped - specRect.top());

    // Plan 4 follow-up (option A): MOX-gate the RX passband fill so it
    // disappears during TX/TUNE.  The TX filter overlay (drawTxFilterOverlay)
    // takes over in that state — gives the user a clear visual swap between
    // the cyan RX shadow and the orange TX band on MOX/TUNE engage.
    if (!m_moxOverlay) {
        // Spectrum passband fill — Plan 4 D9b: user-pickable m_rxFilterColor.
        // Previously hardcoded AetherSDR cyan alpha=35/25; now single user colour.
        if (specHeightClipped > 0) {
            p.fillRect(xLo, specRect.top(), fW, specHeightClipped, m_rxFilterColor);
        }

        // Waterfall passband fill — Plan 4 D9b: same user-pickable colour.
        p.fillRect(xLo, wfRect.top(), fW, wfRect.height(), m_rxFilterColor);
    }

    // Filter edge lines: the marker's edge colour at alpha 130.
    // From AetherSDR src/gui/SpectrumWidget.cpp:17539-17544 [@9f81dc00].
    // Clip the spectrum-side edge to specBottomClipped so the line stops at
    // the bandplan strip's top edge.
    QColor edgeColor = g.edgeColor;
    edgeColor.setAlpha(130);
    p.setPen(QPen(edgeColor, 1));
    p.drawLine(xLo, specRect.top(), xLo, specBottomClipped);
    p.drawLine(xLo, wfRect.top(),   xLo, wfRect.bottom());
    p.drawLine(xHi, specRect.top(), xHi, specBottomClipped);
    p.drawLine(xHi, wfRect.top(),   xHi, wfRect.bottom());

    // VFO center line: the marker's line colour at alpha 220.
    // From AetherSDR src/gui/SpectrumWidget.cpp:17587 [@9f81dc00]. The width
    // is NereusSDR's own, unchanged by the colour work: 2, narrowing to 1
    // when a filter edge is ≤4px away (CW modes).
    qreal vfoLineW = (std::abs(vfoX - xLo) <= 4 || std::abs(vfoX - xHi) <= 4) ? 1.0 : 2.0;
    QColor lineColor = g.lineColor;
    lineColor.setAlpha(220);
    p.setPen(QPen(lineColor, vfoLineW));
    p.drawLine(vfoX, specRect.top(), vfoX, wfRect.bottom());

    // VFO triangle marker — from AetherSDR line 3285-3293
    // Drawn below any VFO flag widget that may be positioned at the top.
    // If a VfoWidget exists for this slice, draw triangle at the flag's bottom edge.
    // Otherwise draw at spectrum top.
    if (vfoX >= specRect.left() && vfoX <= specRect.right()) {
        static constexpr int kTriHalf = 6;
        static constexpr int kTriH = 10;

        int triTop = specRect.top();
        // If VFO flag is present, position triangle below it.
        //
        // THIS marker's flag, not m_vfoWidgets[0]. The slice-0 lookup was
        // harmless while one marker was drawn per pan; with one per slice it
        // would hang every triangle off slice A's flag height, which is only
        // ever right by coincidence.
        if (g.flag && g.flag->isVisible()) {
            triTop = g.flag->y() + g.flag->height();
        }
        // Clamp to spectrum area
        triTop = std::max(triTop, specRect.top());

        // Opaque, in the same colour as the centre line.
        // From AetherSDR src/gui/SpectrumWidget.cpp:17590-17594 [@9f81dc00].
        p.setPen(Qt::NoPen);
        p.setBrush(g.lineColor);
        QPolygon tri;
        tri << QPoint(vfoX - kTriHalf, triTop)
            << QPoint(vfoX + kTriHalf, triTop)
            << QPoint(vfoX, triTop + kTriH);
        p.drawPolygon(tri);
    }
}

// ---- Off-screen VFO indicator (AetherSDR pattern) ----
void SpectrumWidget::drawOffScreenIndicator(QPainter& p, const QRect& specRect,
                                             const QRect& wfRect)
{
    Q_UNUSED(wfRect);
    if (!m_ownSliceMarkerPresentationAllowed) {
        return;
    }

    int leftRows = 0;
    int rightRows = 0;
    if (m_vfoOffScreen != VfoOffScreen::None) {
        // Arrow and label colors — match slice accent color
        int arrowSliceId = m_frontSliceIndex;
        bool drawPanArrow = true;
        if (!m_vfoWidgets.isEmpty()) {
            // The pan-level VFO can still name a selected foreign slice. Never
            // borrow another own flag's color for that foreign frequency.
            const VfoWidget* front = m_vfoWidgets.value(arrowSliceId, nullptr);
            if (!front || !front->stationPresentationAllowed()
                || !qFuzzyCompare(front->frequency(), m_vfoHz)) {
                arrowSliceId = -1;
                for (auto it = m_vfoWidgets.cbegin(); it != m_vfoWidgets.cend(); ++it) {
                    if (it.value() && it.value()->stationPresentationAllowed()
                        && !m_edgeMarkedSlices.contains(it.key())
                        && qFuzzyCompare(it.value()->frequency(), m_vfoHz)) {
                        arrowSliceId = it.key();
                        break;
                    }
                }
            }
            if (arrowSliceId < 0) { drawPanArrow = false; }
        }
        if (drawPanArrow) {
            const bool left = (m_vfoOffScreen == VfoOffScreen::Left);
            drawOffScreenArrow(p, specRect, left, VfoWidget::sliceColor(arrowSliceId),
                               m_vfoHz, 0);
            (left ? leftRows : rightRows) = 1;
        }
    }

    // A slice this pan shows only as its flag (a listened slice a layout
    // change placed here) never moves the pan's VFO, so off the span it gets
    // its own marker, the same arrow in its own colour, one row per slice
    // below the pan's own.
    const double leftEdge = m_centerHz - m_bandwidthHz / 2.0;
    const double rightEdge = m_centerHz + m_bandwidthHz / 2.0;
    for (auto it = m_vfoWidgets.cbegin(); it != m_vfoWidgets.cend(); ++it) {
        if (!m_edgeMarkedSlices.contains(it.key()) || !it.value()
            || !it.value()->stationPresentationAllowed()) {
            continue;
        }
        const double hz = it.value()->frequency();
        if (hz >= leftEdge && hz <= rightEdge) { continue; }
        const bool left = hz < leftEdge;
        int& row = left ? leftRows : rightRows;
        drawOffScreenArrow(p, specRect, left, VfoWidget::sliceColor(it.key()), hz, row);
        ++row;
    }
}

void SpectrumWidget::drawOffScreenArrow(QPainter& p, const QRect& specRect, bool left,
                                        const QColor& arrowColor, double hz, int row)
{
    static constexpr int kArrowW = 14;
    static constexpr int kArrowH = 20;
    static constexpr int kRowGap = 6;

    // Format frequency text
    double mhz = hz / 1.0e6;
    QString label = QString::number(mhz, 'f', 4);

    QFont font = p.font();
    font.setPixelSize(11);
    font.setBold(true);
    p.setFont(font);
    QFontMetrics fm(font);

    int arrowY = specRect.top() + specRect.height() / 2 - kArrowH / 2
        + row * (kArrowH + kRowGap);

    if (left) {
        // Left arrow at left edge
        int x = specRect.left() + 4;
        QPolygon arrow;
        arrow << QPoint(x, arrowY + kArrowH / 2)
              << QPoint(x + kArrowW, arrowY)
              << QPoint(x + kArrowW, arrowY + kArrowH);
        p.setPen(Qt::NoPen);
        p.setBrush(arrowColor);
        p.drawPolygon(arrow);

        // Frequency label to the right of arrow
        p.setPen(arrowColor);
        p.drawText(x + kArrowW + 4, arrowY + kArrowH / 2 + fm.ascent() / 2, label);
    } else {
        // Right arrow at right edge
        int textW = fm.horizontalAdvance(label);
        int x = specRect.right() - 4;
        QPolygon arrow;
        arrow << QPoint(x, arrowY + kArrowH / 2)
              << QPoint(x - kArrowW, arrowY)
              << QPoint(x - kArrowW, arrowY + kArrowH);
        p.setPen(Qt::NoPen);
        p.setBrush(arrowColor);
        p.drawPolygon(arrow);

        // Frequency label to the left of arrow
        p.setPen(arrowColor);
        p.drawText(x - kArrowW - textW - 4, arrowY + kArrowH / 2 + fm.ascent() / 2, label);
    }
}

// ---- Cursor frequency display ----
void SpectrumWidget::drawCursorInfo(QPainter& p, const QRect& specRect)
{
    if (!m_mouseInWidget) {
        return;
    }

    double hz = xToHz(m_mousePos.x(), specRect);

    // formatCursorFreq always returns MHz format ("14.2700 MHz") — see its
    // doc comment for why the Hz alternative was retired.
    QString label = formatCursorFreq(hz);

    QFont font = p.font();
    font.setPixelSize(11);
    font.setBold(true);
    p.setFont(font);

    QFontMetrics fm(font);
    int textW = fm.horizontalAdvance(label) + 12;
    int textH = fm.height() + 6;

    // Position near cursor, offset to avoid covering the crosshair
    int labelX = m_mousePos.x() + 12;
    int labelY = m_mousePos.y() - textH - 4;
    if (labelX + textW > specRect.right()) {
        labelX = m_mousePos.x() - textW - 12;
    }
    if (labelY < specRect.top()) {
        labelY = m_mousePos.y() + 12;
    }

    // Background
    p.fillRect(labelX, labelY, textW, textH, QColor(0x10, 0x15, 0x20, 200));
    p.setPen(QColor(0xc8, 0xd8, 0xe8));
    p.drawText(labelX + 6, labelY + fm.ascent() + 3, label);
}

// ---- HIGH SWR / PA safety overlay ----
// Porting from display.cs:4183-4201 [v2.10.3.13] — original C# logic:
//
//   if (high_swr || _power_folded_back)
//   {
//       if (_power_folded_back)
//           drawStringDX2D("HIGH SWR\n\nPOWER FOLD BACK", fontDX2d_font14, m_bDX2_Red, 245, 20);
//       else
//           drawStringDX2D("HIGH SWR", fontDX2d_font14, m_bDX2_Red, 245, 20);
//   }
//   _d2dRenderTarget.DrawRectangle(new RectangleF(3, 3, displayTargetWidth-6, displayTargetHeight-6),
//                                  m_bDX2_Red, 6f);
//
// //MW0LGE_21k8  [original inline comment from display.cs:4213]

void SpectrumWidget::setHighSwrOverlay(bool active, bool foldback) noexcept
{
    if (m_highSwrActive == active && m_highSwrFoldback == foldback) {
        return;
    }
    m_highSwrActive   = active;
    m_highSwrFoldback = foldback;
    markOverlayDirty();
}

// ---- MOX / TX overlay slots (H.1, Phase 3M-1a) ----------------------------
//
// Porting from Thetis display.cs:1569-1593 [v2.10.3.13] — Display.MOX setter.
// Original C# logic:
//   public static bool MOX {
//     get { return _mox; }
//     set {
//       lock(_objDX2Lock) {
//         if (value != _old_mox) { PurgeBuffers(); _old_mox = value; }
//         _mox = value;
//       }
//     }
//   }
// NereusSDR translation: track state flag, markOverlayDirty() to trigger
// the border repaint on the next paint pass.

void SpectrumWidget::setDisplayFps(int fps)
{
    const int clamped = qBound(1, fps, 60);
    const int periodMs = 1000 / clamped;
    m_displayTimer.setInterval(periodMs);
    // 2026-05-26 KG4VCF bench fix: DO NOT overwrite m_wfUpdatePeriodMs
    // here.  Earlier revisions tried to "lock the waterfall throttle to
    // the same period as paint" by force-writing m_wfUpdatePeriodMs =
    // periodMs on every setDisplayFps call.  Problem: setDisplayFps
    // runs once at startup from MainWindow's persistence-restore (with
    // the persisted DisplaySpectrumFps), so the loaded
    // DisplayWfUpdatePeriodMs got clobbered to 1000/fps every launch.
    // Operator's saved waterfall period (e.g. 33 ms when FPS=20 forces
    // 50 ms) was silently lost.
    //
    // Waterfall update period now lives independently in AppSettings
    // under DisplayWfUpdatePeriodMs and is mutated only by an explicit
    // setWfUpdatePeriodMs() call (Setup -> Display slider).  If the
    // operator wants them locked, both controls expose the value and
    // either can be set to match the other.
    //
    // Averaging alphas depend on fps via Thetis α = exp(-1/(fps×τ)).
    // Recompute so the smoothing time constants stay correct after a rate change.
    recomputeAverageAlphas();
}

void SpectrumWidget::setMoxOverlay(bool isTx)
{
    if (m_moxOverlay == isTx) {
        return;  // idempotent
    }
    // Park the live grid in the store it belongs to, then load the other.
    // Doing it here rather than in MainWindow means the widget can never be
    // caught holding the wrong grid: there is exactly one place the swap
    // happens and it is the same flag every renderer already branches on.
    // Parity Task 31: the grid swaps whatever DUP says (Thetis's
    // SpectrumGridMaxMoxModified reads localMox only, display.cs:1782-1790
    // [v2.10.3.15]); the span swaps only for the transmit view. With DUP on
    // the display keeps the receive Low and High (getFilterXPositions,
    // display.cs:4564-4594 [v2.10.3.15]; getLowHighForRXn,
    // console.cs:22069-22150 [v2.10.3.15]).
    const bool spanWasLoaded = m_txSpanLoaded;
    if (isTx) {
        m_rxRefLevel        = m_refLevel;
        m_rxDynamicRange    = m_dynamicRange;
        m_refLevel          = m_txRefLevel;
        m_dynamicRange      = m_txDynamicRange;
        if (!m_displayDuplex) {
            loadTransmitSpan();
        }
    } else {
        m_txRefLevel        = m_refLevel;
        m_txDynamicRange    = m_dynamicRange;
        m_refLevel          = m_rxRefLevel;
        m_dynamicRange      = m_rxDynamicRange;
        if (m_txSpanLoaded) {
            restoreReceiveSpan();
        }
    }
    const bool axisMoved = spanWasLoaded != m_txSpanLoaded;
    // Deliberately NO scheduleSettingsSave() here.
    //
    // onNoiseFloorChanged moves the live receive grid at 500 ms cadence and
    // pointedly does not save, so its tracking reverts on restart and the
    // operator's own scale survives (SpectrumWidget.cpp, "do NOT call
    // scheduleSettingsSave() here"). Saving from the MOX edge launders that
    // drift into persistence: every transmission would bake whatever the
    // tracker last did into DisplayGridMax/Min, and the receive scale the
    // operator chose would erode away a few dB per key-up.
    //
    // Bench 2026-08-05, JJ KG4VCF: receive stopped reaching -1xx dBm and
    // bottomed out around -90 after a few transmissions. Deliberate changes
    // -- a strip drag, a Setup edit -- schedule their own save, which is
    // where both grids get written.

    m_moxOverlay = isTx;
    //-KG4VCF [v0.5.2] Retire the partial fold on either ownership edge,
    // including DUP with unchanged axes and stop-on-TX with no keyed row.
    m_dssFoldCount = 0;
    m_dssFoldRow.clear();
    m_dssFoldFullBins.clear();
    m_dss.resetInputSmoothing();
    syncDisplaySettingsFromWidget();
    // The active peak hold's transmit gate follows the same local_mox
    // (Thetis display.cs:5011 [v2.10.3.15]:
    //   bSpectralPeakHold = (!local_mox || _activePeakInTxRX1) && m_bSpectralPeakHoldRX1 && ...).
    m_activePeakHold.setTxActive(isTx);
    // Parity Task 31: keyed with DUP on the axis stays the receiver's, and
    // so does its trace.
    if (m_remoteSpectrum && axisMoved) {
        // Either edge on a remote pan: the last trace built belongs to the
        // other axis (the receive trace at the rise, the transmit display at
        // the fall). Retire it, and the vertices built from it, so nothing
        // is drawn under the new axis until a frame for it lands: the
        // Core's transmit context and frames at the rise (none at all from
        // a Core that sends no transmit display, whose pan stays blank with
        // its status line), its receive context and keyframe at the fall,
        // each a round trip away. The rise's move to the carrier cleared
        // the pixels only when the view actually moved. A local pan's
        // frames follow at once.
        m_renderedPixels.clear();
        m_undentedPixels.clear();
        m_visibleBinCount = 0;
        m_hasNewSpectrum = true;
    }
    markOverlayDirty();
    update();   // ensure QPainter path repaints immediately on MOX flip;
                // markOverlayDirty alone waits for the next natural QRhi
                // frame and was visibly laggy on bench (the panadapter went
                // "clear" briefly between cyan RX shadow + orange TX band).

    // Phase 3M-4 Task 12 — IMD overlay show condition includes local_mox.
    // When MOX flips off, clear EMA state so the next MOX-on transition
    // re-seeds from raw values (mirrors Thetis display.cs:5680 [v2.10.3.13]
    // "_ema_dbc != -999" reset path).
    if (!isTx && m_imdOverlay) {
        m_imdOverlay->reset();
    }

    // Phase 3M-4 bench-fix Round 2: log all 4 IMD-overlay gate transitions
    // so bench testers can see in the terminal exactly which gate is
    // failing when the overlay doesn't appear.  All four conditions must
    // be true at paintEvent time for drawImdOverlay to fire (gate at
    // SpectrumWidget.cpp:1438 + GPU mirror at :4369).
    qCInfo(lcSpectrum) << "IMD overlay gate: mox=" << m_moxOverlay
                       << "testIMD=" << m_testingIMD
                       << "showImd=" << m_showIMDMeasurements
                       << "duplex=" << m_displayDuplex
                       << " (after setMoxOverlay)";
}

// ── Two-tone IMD overlay state slots (Phase 3M-4 Task 12) ──────────────────
//
// Wired in MainWindow:
//   TwoToneController::twoToneActiveChanged    -> setTestingIMD
//   PureSignal::show2ToneMeasurementsChanged   -> setShowIMDMeasurements
// Mirrors Thetis Display.TestingIMD (display.cs:296-302 [v2.10.3.13]) and
// Display.ShowIMDMeasurments (display.cs:304-311 [v2.10.3.13]).
void SpectrumWidget::setTestingIMD(bool on)
{
    if (m_testingIMD == on) {
        return;
    }
    m_testingIMD = on;
    if (!on && m_imdOverlay) {
        m_imdOverlay->reset();  // EMA reset, see setMoxOverlay note above.
    }
    qCInfo(lcSpectrum) << "IMD overlay gate: mox=" << m_moxOverlay
                       << "testIMD=" << m_testingIMD
                       << "showImd=" << m_showIMDMeasurements
                       << "duplex=" << m_displayDuplex
                       << " (after setTestingIMD)";
    update();
}

void SpectrumWidget::setShowIMDMeasurements(bool on)
{
    if (m_showIMDMeasurements == on) {
        return;
    }
    m_showIMDMeasurements = on;
    if (!on && m_imdOverlay) {
        m_imdOverlay->reset();
    }
    qCInfo(lcSpectrum) << "IMD overlay gate: mox=" << m_moxOverlay
                       << "testIMD=" << m_testingIMD
                       << "showImd=" << m_showIMDMeasurements
                       << "duplex=" << m_displayDuplex
                       << " (after setShowIMDMeasurements)";
    update();
}

void SpectrumWidget::loadTransmitSpan()
{
    // Park the receive view; the centre is re-aimed at the carrier by the
    // caller, and the SPAN is what carries over, so a zoom made last
    // transmission survives.
    m_rxViewCenterHz    = m_centerHz;
    m_rxViewBandwidthHz = m_bandwidthHz;
    if (m_txViewBandwidthHz > 0.0) { m_bandwidthHz = m_txViewBandwidthHz; }
    m_txSpanLoaded = true;
    rebuildWaterfallViewport();
}

void SpectrumWidget::restoreReceiveSpan()
{
    m_txViewCenterHz    = m_centerHz;
    m_txViewBandwidthHz = m_bandwidthHz;
    if (m_rxViewBandwidthHz > 0.0) {
        m_centerHz    = m_rxViewCenterHz;
        m_bandwidthHz = m_rxViewBandwidthHz;
    }
    m_txSpanLoaded = false;
    rebuildWaterfallViewport();
}

void SpectrumWidget::resetPeaksForDuplexChange()
{
    // From Thetis display.cs:514-521 [v2.10.3.15] (Display.DisplayDuplex):
    //     if (_mox && value != display_duplex)
    //     {
    //         // just incase dup is changed whilst tx'ing
    //         ResetBlobMaximums(1, true);
    //         ResetBlobMaximums(2, true);
    //         ResetSpectrumPeaks(1);
    //         ResetSpectrumPeaks(2);
    //     }
    // Blob maxima: PeakBlobDetector; spectrum peaks: the active peak hold
    // trace (Thetis m_rx1_spectrumPeaks). One pan here, one receiver there.
    resetPeaks();
}

void SpectrumWidget::resetPeaks()
{
    // Thetis's reset pair for one receiver, as every reset site calls it
    // (display.cs:881-882 [v2.10.3.15] resetPeaksAndNoise):
    //   ResetBlobMaximums(rx, true);
    //   ResetSpectrumPeaks(rx);
    // Both empty and hold their display back 500 ms (display.cs:859-877).
    m_peakBlobs.clearMaximums();
    m_activePeakHold.clear();
    markOverlayDirty();
}

void SpectrumWidget::setDisplayDuplex(bool on)
{
    // From Thetis console.cs:15390-15395 [v2.10.3.15]:
    //   private bool _display_duplex = false;
    //   public bool DisplayDuplex { get; set; }
    // Parity Task 31: false by default, as Thetis. MoxDisplayController sets
    // it on the transmitting pan; while keyed a change swaps the pan between
    // the transmit view and the receiver's (the controller moves the source
    // and the bins, this the span), and resets the peaks.
    if (m_displayDuplex == on) {
        return;
    }
    m_displayDuplex = on;
    if (m_moxOverlay) {
        resetPeaksForDuplexChange();
        const bool spanWasLoaded = m_txSpanLoaded;
        if (on && m_txSpanLoaded) {
            restoreReceiveSpan();
        } else if (!on && !m_txSpanLoaded) {
            loadTransmitSpan();
        }
        if (m_remoteSpectrum && spanWasLoaded != m_txSpanLoaded) {
            // As at the rise and the fall: the last trace belongs to the
            // other axis.
            m_renderedPixels.clear();
            m_undentedPixels.clear();
            m_visibleBinCount = 0;
            m_hasNewSpectrum = true;
        }
    }
    markOverlayDirty();
    update();
}

// From Thetis display.cs:4840 [v2.10.3.13]:
// Upstream tags preserved: //MW0LGE (from cited upstream lines) [v2.10.3.15]
//   if (!local_mox) fOffset += rx1_preamp_offset;
// The RX cal offset is only added in RX mode; during TX, the TX path uses
// its own calibration. Parity Task 31: the TX ATT offset is added keyed
// with DUP on (displayCalOffsetDb); MainWindow feeds it from
// StepAttenuatorController::txAttenuatorOffsetDb.
void SpectrumWidget::setTxAttenuatorOffsetDb(float offsetDb)
{
    if (m_txAttOffsetDb == offsetDb) {
        return;
    }
    m_txAttOffsetDb = offsetDb;
    if (m_moxOverlay) {
        // Parity Task 31: the keyed trace moves with it (DUP on).
        m_hasNewSpectrum = true;
        markOverlayDirty();  // only repaints while TX is active
        update();
    }
}

// From Thetis display.cs:2481 [v2.10.3.13]:
//   public static bool DrawTXFilter { ... }
// Enables the TX passband overlay on the spectrum during TX.
// 3G-8 already wired setShowTxFilterOnRxWaterfall for the waterfall side;
// this slot controls the spectrum-panel TX filter shadow.
void SpectrumWidget::setTxFilterVisible(bool on)
{
    if (m_txFilterVisible == on) {
        return;
    }
    m_txFilterVisible = on;
    markOverlayDirty();
}

// ---------------------------------------------------------------------------
// setTxFilterRange()
//
// Plan 4 D9 (Cluster E).  Stores the TX audio-Hz passband edges and triggers
// a panadapter overlay repaint.  Waterfall column repaint is MOX-gated at the
// call site (see drawTxFilterWaterfallColumn).
//
// Source: NereusSDR-original.  IQ-space conversion follows
//   deskhpsdr/transmitter.c:2136-2186 [@120188f]
// which is the same mapping used by TxChannel::applyTxFilterForMode
// (TxChannel.cpp:1047-1078).
// ---------------------------------------------------------------------------
void SpectrumWidget::setTxFilterRange(int audioLowHz, int audioHighHz)
{
    if (m_txFilterLow == audioLowHz && m_txFilterHigh == audioHighHz) {
        return;
    }
    m_txFilterLow  = audioLowHz;
    m_txFilterHigh = audioHighHz;
    markOverlayDirty();
    update();
}

// ---------------------------------------------------------------------------
// setTxMode()
//
// Plan 4 D9 (Cluster E).  Records the active DSP mode so drawTxFilterOverlay
// applies the correct IQ-space sign convention (USB positive, LSB
// negated+swapped, AM/FM/DSB symmetric).
// ---------------------------------------------------------------------------
void SpectrumWidget::setTxMode(DSPMode mode)
{
    if (m_txMode == mode) {
        return;
    }
    m_txMode = mode;
    if (m_txFilterVisible) {
        markOverlayDirty();
        update();
    }
}

// ---------------------------------------------------------------------------
// setTxVfoOffsetHz()
//
// Plan 4 D9 + post-merge Codex review fix.  Signed Hz offset added to m_vfoHz
// when computing the TX overlay position.  Tracks the slice's active XIT
// offset (xitEnabled ? xitHz : 0) so the orange band centers on the actual
// transmit frequency, not the RX VFO.  Wired from SliceModel xit signals
// in MainWindow::wireSliceToSpectrum.
// ---------------------------------------------------------------------------
void SpectrumWidget::setTxVfoOffsetHz(int offsetHz)
{
    if (m_txVfoOffsetHz == offsetHz) {
        return;
    }
    m_txVfoOffsetHz = offsetHz;
    if (m_txFilterVisible) {
        markOverlayDirty();
        update();
    }
}

// ---------------------------------------------------------------------------
// setTxFilterColor()
//
// Plan 4 D9b (Cluster F).  User-pickable TX passband overlay fill colour.
// Persists per-pan to DisplayTxFilterColor (AppSettings "#RRGGBBAA").
// ---------------------------------------------------------------------------
void SpectrumWidget::setTxFilterColor(const QColor& c)
{
    if (!c.isValid() || m_txFilterColor == c) {
        return;
    }
    m_txFilterColor = c;
    auto& s = AppSettings::instance();
    s.setValue(settingsKey(QStringLiteral("DisplayTxFilterColor"), m_panIndex),
               c.name(QColor::HexArgb));
    markOverlayDirty();
    update();
}

// ---------------------------------------------------------------------------
// setRxFilterColor()
//
// Plan 4 D9b (Cluster F).  User-pickable RX passband overlay fill colour.
// Replaces the formerly hardcoded AetherSDR cyan at two fillRect sites.
// Persists per-pan to DisplayRxFilterColor (AppSettings "#RRGGBBAA").
// ---------------------------------------------------------------------------
void SpectrumWidget::setRxFilterColor(const QColor& c)
{
    if (!c.isValid() || m_rxFilterColor == c) {
        return;
    }
    m_rxFilterColor = c;
    auto& s = AppSettings::instance();
    s.setValue(settingsKey(QStringLiteral("DisplayRxFilterColor"), m_panIndex),
               c.name(QColor::HexArgb));
    markOverlayDirty();
    update();
}

// ---------------------------------------------------------------------------
// txAudioToIq()
//
// Converts audio-Hz passband [audioLow, audioHigh] to IQ-space signed offsets
// from the VFO center.  Matches TxChannel::applyTxFilterForMode exactly.
//
// Per deskhpsdr/transmitter.c:2136-2186 [@120188f] — tx_set_filter per-mode
// IQ-space sign convention.
// ---------------------------------------------------------------------------
std::pair<int,int> SpectrumWidget::txAudioToIq(int audioLow, int audioHigh,
                                                DSPMode mode) const
{
    // LSB family: bandpass sits below the carrier — negate and swap.
    auto isLsbFamily = [](DSPMode m) {
        return m == DSPMode::LSB || m == DSPMode::DIGL || m == DSPMode::CWL;
    };
    // Symmetric modes: equal sidebands around the carrier.
    auto isSymmetric = [](DSPMode m) {
        return m == DSPMode::AM  || m == DSPMode::SAM
            || m == DSPMode::DSB || m == DSPMode::FM
            || m == DSPMode::DRM;
    };

    int iqLow, iqHigh;
    if (isLsbFamily(mode)) {
        iqLow  = -audioHigh;
        iqHigh = -audioLow;
    } else if (isSymmetric(mode)) {
        iqLow  = -audioHigh;
        iqHigh = +audioHigh;
    } else {
        // USB family (USB / DIGU / CWU / SPEC / others): positive sideband.
        iqLow  = +audioLow;
        iqHigh = +audioHigh;
    }
    return {iqLow, iqHigh};
}

// ---------------------------------------------------------------------------
// TNF / notch overlay: setters, colour resolution, render
// (design sections 8.1 and 8.2).
//
// Geometry is AetherSDR's drawTnfMarkers ported unchanged
// (src/gui/SpectrumWidget.cpp:13503-13554 [@c6481cbf]).  The only two
// divergences are the ones the missing depth axis forces: the hatch spacing
// is fixed instead of depth-derived (upstream :13535) and the handle height
// is fixed instead of 8 + depthDb * 2 (upstream :13545).
//
// Colours are Thetis's, not AetherSDR's: upstream encodes permanent versus
// temporary in green/yellow and we have no permanence, while Thetis encodes
// exactly the four states we do have (display.cs:386-390 [v2.10.3.15]).
// ---------------------------------------------------------------------------

// From Thetis display.cs:389 [v2.10.3.15]: notch_active_colour = Color.Yellow.
static constexpr QRgb kNotchActiveColour = qRgb(0xFF, 0xFF, 0x00);
// From Thetis display.cs:390 [v2.10.3.15]: notch_inactive_colour = Color.Gray.
// System.Drawing.Color.Gray is #808080 while Qt::gray is #A0A0A4, so the
// literal is spelled out rather than reaching for the Qt global colour.
static constexpr QRgb kNotchInactiveColour = qRgb(0x80, 0x80, 0x80);
// From Thetis display.cs:387 [v2.10.3.15]: notch_tnf_off_colour = Color.Olive.
static constexpr QRgb kNotchTnfOffColour = qRgb(0x80, 0x80, 0x00);
// From Thetis display.cs:386 [v2.10.3.15]:
// notch_highlight_color = Color.Chartreuse.
static constexpr QRgb kNotchHighlightColour = qRgb(0x7F, 0xFF, 0x00);

// From Thetis display.cs:400-408 [v2.10.3.15]: every notch fill brush is
// changeAlpha(colour, 92); changeAlpha itself is display.cs:2939-2942.
static constexpr int kNotchFillAlpha = 92;

// Fixed, replacing AetherSDR's depth-derived
// (depthDb <= 1) ? 12 : (depthDb == 2 ? 8 : 5) at
// src/gui/SpectrumWidget.cpp:13535 [@c6481cbf].
static constexpr int kNotchHatchSpacingPx = 8;

// Fixed, replacing AetherSDR's 8 + depthDb * 2 at
// src/gui/SpectrumWidget.cpp:13545 [@c6481cbf].
static constexpr int kNotchHandleHeightPx = 10;

// From AetherSDR src/gui/SpectrumWidget.cpp:13547-13548 [@c6481cbf]:
// tri << QPoint(cx - 5, ...) << QPoint(cx + 5, ...).
static constexpr int kNotchHandleHalfWidthPx = 5;

// From AetherSDR src/gui/SpectrumWidget.cpp:13523 [@c6481cbf]:
// std::max(2, ...), so a sub-2-pixel notch stays grabbable.
static constexpr int kNotchMinHalfWidthPx = 2;

// From AetherSDR src/gui/SpectrumWidget.cpp:13551 [@c6481cbf]: the grab
// handle dims with the master flag as well as changing colour.
static constexpr int kNotchHandleAlphaOn  = 200;
static constexpr int kNotchHandleAlphaOff = 80;

// From AetherSDR src/gui/SpectrumWidget.cpp:13436-13440 [@c6481cbf]
void SpectrumWidget::setNotchMarkers(const QVector<NotchMarker>& markers)
{
    m_notchMarkers = markers;
    markOverlayDirty();
}

// From AetherSDR src/gui/SpectrumWidget.cpp:13497-13501 [@c6481cbf]
void SpectrumWidget::setNotchGlobalEnabled(bool on)
{
    m_notchGlobalEnabled = on;
    markOverlayDirty();
}

void SpectrumWidget::setNotchMinWidthHz(double hz)
{
    m_notchMinWidthHz = hz;
    markOverlayDirty();
}

// From Thetis display.cs:8691-8722 [v2.10.3.15]: handleNotches' brush
// selection, flattened to a colour because our pen and fill derive from one
// base (upstream keeps a Pen and a Brush per state and they never disagree).
QColor SpectrumWidget::notchColor(const NotchMarker& n) const
{
    // From Thetis display.cs:8710-8722 [v2.10.3.15]:
    //   //overide if highlighed  [original inline comment from display.cs:8710]
    // The highlight is applied AFTER the master-off branch upstream, so it
    // wins over every other state.  Guarded on a real id because both
    // selection members and a default-constructed marker share -1.
    if (n.id >= 0 && (n.id == m_selectedNotchId || n.id == m_hoveredNotchId)) {
        return QColor::fromRgb(kNotchHighlightColour);
    }

    // From Thetis display.cs:8704-8707 [v2.10.3.15]: master TNF off repaints
    // every marker olive rather than hiding it.
    if (!m_notchGlobalEnabled) {
        return QColor::fromRgb(kNotchTnfOffColour);
    }

    // From Thetis display.cs:8693-8702 [v2.10.3.15]
    return n.active ? QColor::fromRgb(kNotchActiveColour)
                    : QColor::fromRgb(kNotchInactiveColour);
}

// From AetherSDR src/gui/SpectrumWidget.cpp:13503-13554 [@c6481cbf]
void SpectrumWidget::drawNotchMarkers(QPainter& p, const QRect& specRect)
{
    if (m_notchMarkers.isEmpty()) {
        return;
    }

    // From AetherSDR src/gui/SpectrumWidget.cpp:13507-13519 [@c6481cbf]:
    // the drawDepthHatch lambda, renamed because the depth argument is gone.
    const auto drawHatch = [&](const QRect& rect, const QColor& colour,
                               int left, int right, int spacing) {
        if (rect.isEmpty()) {
            return;
        }
        p.save();
        p.setClipRect(rect);
        p.setPen(QPen(colour, 1));
        const int height = rect.height();
        for (int x = left - height; x < right; x += spacing) {
            p.drawLine(x, rect.bottom(), x + height, rect.top());
        }
        p.restore();
    };

    for (const NotchMarker& n : m_notchMarkers) {
        // NereusSDR coordinate mapping: hzToX(double hz, QRect) takes Hz.
        // AetherSDR upstream uses mhzToX(freqMhz) at :13522-13523; multiply
        // by 1e6, exactly as drawSpotMarkers already does.
        const double centreHz = n.freqMhz * 1.0e6;
        const int cx    = hzToX(centreHz, specRect);
        const int halfW = std::max(kNotchMinHalfWidthPx,
                                   hzToX(centreHz + n.widthHz / 2.0, specRect) - cx);
        const int left  = cx - halfW;
        const int right = cx + halfW;

        // From AetherSDR src/gui/SpectrumWidget.cpp:13527-13528 [@c6481cbf]
        // Skip if fully off-screen
        if (right < 0 || left > width()) {
            continue;
        }

        const QColor base = notchColor(n);
        QColor fill(base);
        fill.setAlpha(kNotchFillAlpha);

        const QRect notchRect(left, specRect.top(), right - left, specRect.height());
        p.fillRect(notchRect, fill);
        drawHatch(notchRect, base, left, right, kNotchHatchSpacingPx);

        // From AetherSDR src/gui/SpectrumWidget.cpp:13538-13542 [@c6481cbf]
        // Edge lines
        p.setPen(QPen(base, 1, Qt::SolidLine));
        p.drawLine(left,  specRect.top(), left,  specRect.bottom());
        p.drawLine(right, specRect.top(), right, specRect.bottom());

        // From AetherSDR src/gui/SpectrumWidget.cpp:13544-13552 [@c6481cbf]
        // Center triangle (grab handle) at top of spectrum
        QPolygon tri;
        tri << QPoint(cx - kNotchHandleHalfWidthPx, specRect.top())
            << QPoint(cx + kNotchHandleHalfWidthPx, specRect.top())
            << QPoint(cx, specRect.top() + kNotchHandleHeightPx);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(base.red(), base.green(), base.blue(),
                          m_notchGlobalEnabled ? kNotchHandleAlphaOn
                                               : kNotchHandleAlphaOff));
        p.drawPolygon(tri);
    }

    p.setBrush(Qt::NoBrush);
    p.setPen(Qt::NoPen);
}

// ---------------------------------------------------------------------------
// drawTxFilterOverlay()
//
// ---------------------------------------------------------------------------
// Spot overlay setter + render + cluster popup (Phase 3J-2 Task E1).
// Port of AetherSDR src/gui/SpectrumWidget.cpp:4303-4672 [@0cd4559].
// Algorithm preserved verbatim. NereusSDR divergences are local to the
// coordinate helpers (NereusSDR uses hzToX(double hz, QRect) over Hz-units
// and m_centerHz / m_bandwidthHz; AetherSDR's mhzToX(mhz) uses MHz units
// and m_centerMhz / m_bandwidthMhz). Visibility test and tick / label /
// cluster geometry mirror upstream byte-for-byte.
// ---------------------------------------------------------------------------

// From AetherSDR src/gui/SpectrumWidget.cpp:4303-4307 [@0cd4559]
// Repaint guard added per AetherSDR [@a173272d] PR #2474.
void SpectrumWidget::setSpotMarkers(const QVector<SpotMarker>& markers)
{
    const bool visualChange = !spotMarkersVisuallyEqual(m_spotMarkers, markers);
    m_spotMarkers = markers;
    if (!visualChange) {
        return;
    }
    update();
}

// Phase 3J-2 + 3R M2: refresh every Spot Display knob from AppSettings.
// The producer side lives on SpotHubDialog F4 (buildDisplayTab,
// SpotHubDialog.cpp:1619-2010); every knob change writes to AppSettings
// and emits settingsChanged. MainWindow::openSpotHub wires that signal
// to this method so the live overlay tracks the dialog. Defaults
// duplicate the F4 read-side at SpotHubDialog.cpp:1714-1730.
//
// NereusSDR-original. AetherSDR splits these reads across a freestanding
// SpotSettingsDialog and a refreshSpots() lambda on MainWindow; the
// NereusSDR shape collapses both into a single helper on the consumer
// widget, called once on construction (when MainWindow wires the panel)
// and again whenever the dialog raises settingsChanged.
void SpectrumWidget::loadSpotDisplaySettings()
{
    auto& s = AppSettings::instance();
    setShowSpots(
        s.value(QStringLiteral("IsSpotsEnabled"),
                QStringLiteral("True")).toString() == QStringLiteral("True"));
    setSpotFontSize(s.value(QStringLiteral("SpotFontSize"), 16).toInt());
    setSpotMaxLevels(s.value(QStringLiteral("SpotsMaxLevel"), 3).toInt());
    setSpotStartPct(
        s.value(QStringLiteral("SpotsStartingHeightPercentage"), 50).toInt());
    setSpotOverrideColors(
        s.value(QStringLiteral("IsSpotsOverrideColorsEnabled"),
                QStringLiteral("False")).toString()
            == QStringLiteral("True"));
    setSpotOverrideBg(
        s.value(QStringLiteral("IsSpotsOverrideBackgroundColorsEnabled"),
                QStringLiteral("True")).toString()
            == QStringLiteral("True"));
    setSpotColor(QColor(
        s.value(QStringLiteral("SpotsOverrideColor"),
                QStringLiteral("#FFFF00")).toString()));
    setSpotBgColor(QColor(
        s.value(QStringLiteral("SpotsOverrideBgColor"),
                QStringLiteral("#000000")).toString()));
    setSpotBgOpacity(
        s.value(QStringLiteral("SpotsBackgroundOpacity"), 48).toInt());

    // 2026-05-12 bench fix (Gap #7).  Pull per-source panadapter
    // visibility from AppSettings.  Default True (visible) so a fresh
    // install or pre-Phase-3J-2 settings file keeps showing all
    // sources.  Keys match the source strings the per-source spot
    // adapters in RadioModel stamp into SpotData::source.
    static const QStringList kSpotSources = {
        QStringLiteral("Cluster"),       QStringLiteral("RBN"),
        QStringLiteral("WSJT-X"),        QStringLiteral("SpotCollector"),
        QStringLiteral("POTA"),          QStringLiteral("FreeDV"),
        QStringLiteral("PSK"),
    };
    for (const QString& source : kSpotSources) {
        const QString key = QStringLiteral("SpotSourceVisible/") + source;
        const bool visible =
            s.value(key, QStringLiteral("True")).toString()
            == QStringLiteral("True");
        setSpotSourceVisible(source, visible);
    }
    update();
}

// From AetherSDR src/gui/SpectrumWidget.cpp:4497-4633 [@0cd4559]
void SpectrumWidget::drawSpotMarkers(QPainter& p, const QRect& specRect)
{
    if (m_spotMarkers.isEmpty()) {
        m_spotClickRects.clear();
        m_spotClusters.clear();
        return;
    }

    QFont spotFont = p.font();
    spotFont.setPixelSize(m_spotFontSize);
    spotFont.setBold(true);
    p.setFont(spotFont);
    const QFontMetrics fm(spotFont);

    // Starting Y position based on percentage setting
    const int startY = specRect.top() + specRect.height() * m_spotStartPct / 100;
    const int th = fm.height() + 2;
    const int maxBottom = startY + th * m_spotMaxLevels;

    // Track label positions to avoid overlap and for click detection
    QVector<QRect> placed;
    m_spotClickRects.clear();
    m_spotClusters.clear();

    // Track which spots overflow (can't be placed within max levels)
    // Key: x pixel position (quantized to label width), Value: list of overflowed spots
    QMap<int, QVector<SpotMarker>> overflowGroups;
    constexpr int ClusterBinWidth = 40;  // pixels — spots within this range cluster together

    // Phase 3J-1 closeout follow-up (2026-05-12): one-shot diagnostic
    // log so the bench operator can verify the per-source mask state
    // matches the spots being rendered.  Logs once per second of
    // unique state to avoid spamming the log file.  The mask check
    // is otherwise unchanged: missing key in m_spotSourceVisible
    // defaults to visible (true); explicit `false` value hides.
    static qint64 s_lastSpotMaskLogMs = 0;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!m_spotMarkers.isEmpty() && nowMs - s_lastSpotMaskLogMs > 1000) {
        QStringList parts;
        for (auto it = m_spotSourceVisible.constBegin();
             it != m_spotSourceVisible.constEnd(); ++it) {
            parts << QString("%1=%2").arg(it.key())
                                     .arg(it.value() ? "T" : "F");
        }
        QStringList sources;
        for (const auto& s : m_spotMarkers) {
            if (!s.source.isEmpty() && !sources.contains(s.source)) {
                sources << s.source;
            }
        }
        // qCInfo so the bench log captures it (lcSpectrum defaults to
        // QtInfoMsg; qCDebug would be suppressed).  Throttled to once
        // per second of unique state so it doesn't spam mid-tune.
        qCInfo(lcSpectrum) << "drawSpotMarkers: mask=" << parts
                           << "spot.sources=" << sources
                           << "total=" << m_spotMarkers.size();
        s_lastSpotMaskLogMs = nowMs;
    }

    for (const auto& spot : m_spotMarkers) {
        // 2026-05-12 bench fix (Gap #7).  Per-source panadapter
        // visibility mask.  Missing key in m_spotSourceVisible defaults
        // to visible, so untouched sources keep the prior behaviour;
        // an explicit `false` value hides this marker entirely (no
        // label, no tick line, no hit-rect).  SpotHubDialog Display
        // tab drives this.
        if (!m_spotSourceVisible.value(spot.source, true)) {
            continue;
        }

        // NereusSDR coordinate mapping: hzToX(double hz, QRect) takes Hz.
        // AetherSDR upstream uses mhzToX(spot.freqMhz). Multiply by 1e6.
        const int x = hzToX(spot.freqMhz * 1.0e6, specRect);
        if (x < 0 || x > width()) continue;

        // Color priority: override → DXCC → spot-provided → default cyan
        QColor col(0x00, 0xb4, 0xd8);  // default cyan
        if (m_spotOverrideColors) {
            col = m_spotColor;
        } else if (spot.dxccColor.isValid()) {
            col = spot.dxccColor;
        } else if (!spot.color.isEmpty() && spot.color.startsWith('#')) {
            QColor parsed(spot.color);
            if (parsed.isValid()) col = parsed;
        }

        // Draw callsign label
        const QString label = spot.callsign;
        const int tw = fm.horizontalAdvance(label) + 6;

        // Start at configured position, nudge down to avoid overlap.
        // Re-scan from the start after each nudge to handle cases where
        // nudging past label A lands on top of label B.
        QRect labelRect(x - tw / 2, startY, tw, th);
        bool collision = true;
        while (collision) {
            collision = false;
            for (const auto& r : placed) {
                if (labelRect.intersects(r)) {
                    labelRect.moveTop(r.bottom() + 1);
                    collision = true;
                    break;
                }
            }
        }
        // Overflow — collect for cluster badge
        if (labelRect.bottom() > maxBottom) {
            int bin = x / ClusterBinWidth;
            overflowGroups[bin].append(spot);
            continue;
        }

        // Draw vertical tick line from bottom of spectrum up to the label
        p.setPen(QPen(QColor(col.red(), col.green(), col.blue(), 120), 1, Qt::DotLine));
        p.drawLine(x, specRect.bottom(), x, labelRect.bottom());

        placed.append(labelRect);
        int mIdx = static_cast<int>(&spot - &m_spotMarkers[0]);
        m_spotClickRects.append({labelRect, spot.freqMhz, mIdx});

        // Background pill — only draw when override background is enabled (#768)
        if (m_spotOverrideBg) {
            int bgAlpha = m_spotBgOpacity * 255 / 100;
            QColor bgCol = m_spotBgColor;
            bgCol.setAlpha(bgAlpha);
            p.setPen(Qt::NoPen);
            p.setBrush(bgCol);
            p.drawRoundedRect(labelRect, 3, 3);
        }

        // 2026-05-12 bench fix (Gap #6 follow-on — Spot List hover halo).
        // When the user is hovering the matching row in the Spot List
        // table, SpotHubDialog calls setHoverSpotIndexExternal(idx) on
        // this widget.  Draw a bright outline so the user can map the
        // table row to the spectrum overlay at a glance.  The halo is
        // intentionally drawn AFTER the bg pill but BEFORE the text so
        // the callsign stays on top.
        if (spot.index >= 0 && spot.index == m_hoverSpotIndexExternal) {
            QColor halo(0xff, 0xff, 0x00, 220);  // bright yellow
            p.setPen(QPen(halo, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(labelRect.adjusted(-2, -2, 2, 2), 4, 4);
        }

        // Text
        p.setPen(col);
        p.drawText(labelRect, Qt::AlignCenter, label);
    }

    // Draw cluster badges for overflow groups
    if (!overflowGroups.isEmpty()) {
        QFont badgeFont = spotFont;
        badgeFont.setPixelSize(m_spotFontSize - 2);
        p.setFont(badgeFont);
        const QFontMetrics bfm(badgeFont);

        for (auto it = overflowGroups.constBegin(); it != overflowGroups.constEnd(); ++it) {
            const auto& spots = it.value();
            if (spots.isEmpty()) continue;

            // Position badge at average x of the group, at maxBottom
            int avgX = 0;
            for (const auto& s : spots) {
                avgX += hzToX(s.freqMhz * 1.0e6, specRect);
            }
            avgX /= spots.size();

            const QString badgeText = QString("+%1").arg(spots.size());
            const int bw = bfm.horizontalAdvance(badgeText) + 10;
            QRect badgeRect(avgX - bw / 2, maxBottom + 2, bw, th);

            // Nudge horizontally to avoid overlapping other badges/labels
            for (const auto& r : placed) {
                if (badgeRect.intersects(r)) {
                    badgeRect.moveLeft(r.right() + 3);
                }
            }
            placed.append(badgeRect);

            // Draw badge with distinct style
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x30, 0x50, 0x70, 200));
            p.drawRoundedRect(badgeRect, 3, 3);

            p.setPen(QColor(0xff, 0xc0, 0x40));  // amber text
            p.drawText(badgeRect, Qt::AlignCenter, badgeText);

            // Store for click detection
            SpotCluster cluster;
            cluster.rect = badgeRect;
            cluster.spots = spots;
            m_spotClusters.append(cluster);
        }

        p.setFont(spotFont);  // restore spot font
    }

    p.setFont(QFont());  // restore default
}

// From AetherSDR src/gui/SpectrumWidget.cpp:4635-4672 [@0cd4559]
void SpectrumWidget::showSpotClusterPopup(const SpotCluster& cluster, const QPoint& globalPos)
{
    auto* menu = new QMenu(this);
    menu->setStyleSheet(
        "QMenu {"
        "  background: #0f0f1a;"
        "  border: 1px solid #305070;"
        "  padding: 4px;"
        "}"
        "QMenu::item {"
        "  color: #c8d8e8;"
        "  padding: 4px 12px;"
        "  font-size: 12px;"
        "}"
        "QMenu::item:selected {"
        "  background: #1a3a5a;"
        "  color: #00b4d8;"
        "}");

    for (const auto& spot : cluster.spots) {
        QString text = QString("%1  %2 kHz")
            .arg(spot.callsign, -10)
            .arg(spot.freqMhz * 1000.0, 0, 'f', 1);
        if (!spot.mode.isEmpty()) {
            text += "  " + spot.mode;
        }
        auto* action = menu->addAction(text);
        connect(action, &QAction::triggered, this, [this, spot] {
            // NereusSDR signal contract: frequencyClicked(double hz).
            // AetherSDR emits MHz; multiply by 1e6 to match the Hz signature.
            const double freqHz = spot.freqMhz * 1.0e6;
            requestTune(freqHz);
            if (spot.source == "Memory") {
                emit spotTriggered(spot.index);
            }
        });
    }

    menu->popup(globalPos);
    // QMenu self-deletes on close with WA_DeleteOnClose
    menu->setAttribute(Qt::WA_DeleteOnClose);
}

// Plan 4 D9 (Cluster E).  Panadapter TX filter band fill + border lines +
// inline label.  Always drawn when m_txFilterVisible is set (called from the
// main paint sequence regardless of MOX state — Thetis shows the TX filter
// band on the spectrum even in RX mode to hint where TX will land).
//
// Source: NereusSDR-original rendering.  IQ-space mapping per
//   deskhpsdr/transmitter.c:2136-2186 [@120188f].
// ---------------------------------------------------------------------------
double SpectrumWidget::txFilterXitHz() const
{
    // Parity Task 31: no XIT keyed with DUP on (drawTxFilterOverlay).
    if (m_moxOverlay && m_displayDuplex) {
        return 0.0;
    }
    return static_cast<double>(m_txVfoOffsetHz);
}

void SpectrumWidget::drawTxFilterOverlay(QPainter& p, const QRect& specRect)
{
    if (m_vfoHz <= 0.0) {
        return;
    }

    auto [iqLow, iqHigh] = txAudioToIq(m_txFilterLow, m_txFilterHigh, m_txMode);

    // m_txVfoOffsetHz tracks the active XIT offset so the overlay centers
    // on the actual TX frequency rather than the RX VFO.  Zero when XIT is
    // disabled or the slice has no XIT.
    // Parity Task 31: keyed with DUP on the filter sits against the receive
    // span at the VFO, XIT left out, as Thetis's getFilterXPositions
    // (display.cs:4564-4594 [v2.10.3.15]) draws tx_filter_low/high from the
    // receive Low/High and freq_diff, and getLowHighForRXn adds XIT only
    // "when not in display duplex mode" (console.cs:22144 [v2.10.3.15]).
    const double txCenter = m_vfoHz + txFilterXitHz();
    const double absLow  = txCenter + static_cast<double>(iqLow);
    const double absHigh = txCenter + static_cast<double>(iqHigh);

    int xLow  = hzToX(absLow,  specRect);
    int xHigh = hzToX(absHigh, specRect);

    // Ensure xLow ≤ xHigh (defensive; IQ mapping should already order them).
    if (xLow > xHigh) {
        std::swap(xLow, xHigh);
    }

    // Emit test seam before any painting so QSignalSpy sees the values.
    emit txFilterOverlayPainted(xLow, xHigh);

    // Clamp to visible rect.
    const int left  = std::max(xLow,  specRect.left());
    const int right = std::min(xHigh, specRect.right());
    if (left >= right) {
        return;
    }
    const int bandW = right - left;

    // Reserve the bandplan strip at the bottom of specRect (drawBandPlan paints
    // colored segments there but leaves transparent gaps between segments;
    // without this clip the translucent filter colour would bleed through the
    // gaps and visually sit "in front of" the bandplan strip).  Mirrors the
    // height calculation in drawBandPlan: bandH = m_bandPlanFontSize + 4 when a
    // bandplan manager is bound, else 0.
    const int bandPlanH = (m_bandPlanMgr && m_bandPlanFontSize > 0)
                          ? (m_bandPlanFontSize + 4) : 0;
    const int filterBottom = specRect.bottom() - bandPlanH;
    const int filterH      = std::max(0, filterBottom - specRect.top());
    if (filterH <= 0) {
        return;
    }

    // Fill band with translucent orange.
    p.fillRect(QRect(left, specRect.top(), bandW, filterH),
               m_txFilterColor);

    // Border lines — 2 px solid orange.
    const QColor borderColor(0xff, 0x78, 0x33);  // kTxFilterOverlayBorder
    p.save();
    p.setPen(QPen(borderColor, 2));
    if (xLow >= specRect.left()) {
        p.drawLine(xLow, specRect.top(), xLow, filterBottom);
    }
    if (xHigh <= specRect.right()) {
        p.drawLine(xHigh, specRect.top(), xHigh, filterBottom);
    }
    p.restore();

    // Inline label — kTxFilterOverlayLabel (#ffaa70), 9 px, top-left inside band.
    // Format: "TX <low>-<high> Hz"
    {
        const QString label = QStringLiteral("TX %1-%2 Hz")
                                  .arg(m_txFilterLow)
                                  .arg(m_txFilterHigh);
        const QColor labelColor(0xff, 0xaa, 0x70);  // kTxFilterOverlayLabel
        p.save();
        QFont f = p.font();
        f.setPixelSize(9);
        p.setFont(f);
        p.setPen(labelColor);
        // 8 px left-pad from the left edge, 4 px top-pad.
        const int labelX = left + 8;
        const int labelY = specRect.top() + 4 + p.fontMetrics().ascent();
        if (labelX + p.fontMetrics().horizontalAdvance(label) <= right) {
            p.drawText(labelX, labelY, label);
        }
        p.restore();
    }
}

// ---------------------------------------------------------------------------
// drawImdOverlay()
//
// Phase 3M-4 Task 12.  Two-tone IMD overlay: peak markers + readout box.
// Only called when m_moxOverlay && m_testingIMD && m_showIMDMeasurements
// && m_displayDuplex.
//
// Source-first port of Thetis display.cs [v2.10.3.13]:
//   :5008       show condition (caller gates this — see paintEvent)
//   :5283-5298  peak detection (delegated to ImdOverlay::detectPeaks)
//   :5453-5475  peak ellipse markers
//   :5512-5560  IMD3/IMD5 sort + findImd labeling (delegated to
//                  ImdOverlay::labelImdProducts)
//   :5520       readout box rect (260x180 px, X=50, Y=50, R=14)
//   :5650-5685  3-column readout text (delegated to
//                  ImdOverlay::formatReadout)
// ---------------------------------------------------------------------------
void SpectrumWidget::drawImdOverlay(QPainter& p, const QRect& specRect)
{
    if (!m_imdOverlay || m_renderedPixels.isEmpty()) {
        return;
    }

    // Peak-detect on the visible bin subset so the X coordinates we get
    // back are bin offsets relative to firstBin.  Mirrors the existing
    // drawSpectrum bin-to-pixel mapping at line 1564 [SpectrumWidget.cpp].
    const auto [firstBin, lastBin] = visibleBinRange(m_renderedPixels.size());
    const int count = lastBin - firstBin + 1;
    if (count < 2) {
        return;
    }

    std::vector<float> visible;
    visible.reserve(count);
    for (int i = 0; i < count; ++i) {
        visible.push_back(m_renderedPixels[firstBin + i]);
    }

    // From Thetis display.cs:5217 [v2.10.3.13]: trigger_delta = 10 dB.
    constexpr float kTriggerDelta = 10.0f;
    const auto peaks = ImdOverlay::detectPeaks(visible, kTriggerDelta);

    Maximum f0L, f0U, imd3L, imd3U, imd5L, imd5U;
    if (!ImdOverlay::labelImdProducts(peaks,
                                      f0L, f0U, imd3L, imd3U, imd5L, imd5U)) {
        // Failure modes (Thetis display.cs:5666-5673 [v2.10.3.13] fallback
        // text "Peaks not found !") are intentionally silent here — the
        // user already sees a stale readout box from the previous frame
        // until peaks return.  A future polish PR can render a hint.
        return;
    }
    m_imdOverlay->updateReadout(f0L, f0U, imd3L, imd3U, imd5L, imd5U);

    // Map bin indices -> pixel X using the same formula as drawSpectrum.
    const float xStep = static_cast<float>(specRect.width())
                      / static_cast<float>(count - 1);
    auto binToPx = [&](int binIdx) {
        return specRect.left() + static_cast<float>(binIdx) * xStep;
    };
    auto dbmToPx = [&](float dbm) {
        return static_cast<float>(dbmToY(dbm, specRect));
    };

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // From Thetis display.cs:5453-5475 [v2.10.3.13] peak markers.
    // Ellipse colour palette matches the visual companion mockup at
    // .superpowers/brainstorm/92020-1778073191/content/imd-overlay.html
    // (Thetis uses m_bDX2_PeakBlob brush).
    auto drawMarker = [&](const Maximum& m, const QColor& c, float radius) {
        const QPointF pt(binToPx(m.x), dbmToPx(m.dBm));
        p.setPen(QPen(c, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pt, radius, radius);
    };
    drawMarker(f0L,   QColor("#ffd24a"), 6.0f);
    drawMarker(f0U,   QColor("#ffd24a"), 6.0f);
    drawMarker(imd3L, QColor("#ff8a8a"), 5.0f);
    drawMarker(imd3U, QColor("#ff8a8a"), 5.0f);
    drawMarker(imd5L, QColor("#cc6680"), 4.0f);
    drawMarker(imd5U, QColor("#cc6680"), 4.0f);

    // From Thetis display.cs:5520 [v2.10.3.13]:
    //   RoundedRectangle rr = new RoundedRectangle();
    //   rr.Rect = new RectangleF(_two_tone_readings_X_offset, 50, 260, 180);
    //   rr.RadiusX = 14f; rr.RadiusY = 14f;
    // _two_tone_readings_X_offset starts at 50 (display.cs:4948 [v2.10.3.13]).
    // Thetis adjusts the X offset to dodge IMD5 markers (display.cs:5584-5585);
    // NereusSDR keeps the simple fixed-50 placement for now.
    constexpr int kBoxX = 50;
    constexpr int kBoxY = 50;
    constexpr int kBoxW = 260;
    constexpr int kBoxH = 180;
    constexpr int kBoxR = 14;
    const QRectF box(specRect.left() + kBoxX, specRect.top() + kBoxY,
                     kBoxW, kBoxH);
    p.setBrush(QColor(20, 28, 40, 240));
    p.setPen(QPen(QColor("#5fb0d8"), 1.5));
    p.drawRoundedRect(box, kBoxR, kBoxR);

    // From Thetis display.cs:5683-5687 [v2.10.3.13] DrawText calls.
    // 4 columns: "dBm dBc frequency" header (NereusSDR drops the freq
    // column for now — that requires hz_per_pixel + DDC center plumbing
    // which the current mockup doesn't model), then label/val1/val2.
    const auto t = m_imdOverlay->formatReadout();
    p.setPen(QColor("#cfe1f0"));
    QFont mono(QStringLiteral("Menlo"));
    mono.setPointSize(9);
    mono.setStyleHint(QFont::Monospace);
    p.setFont(mono);

    // Header row mirrors display.cs:5683 "dBm        dBc           frequency"
    // (without the frequency column).
    p.drawText(QPointF(box.left() + 70, box.top() + 16),
               QStringLiteral("dBm           dBc"));

    // Label / val1 / val2 columns at the same offsets the Thetis box uses
    // (display.cs:5684-5686 +10/+64/+114 horizontal stride).
    const QRectF readingsCol(box.left() + 10, box.top() + 28,
                             box.width() - 20, box.height() - 36);
    const QRectF val1Col(box.left() + 64, box.top() + 28,
                         box.width() - 70, box.height() - 36);
    const QRectF val2Col(box.left() + 130, box.top() + 28,
                         box.width() - 130, box.height() - 36);
    p.drawText(readingsCol, Qt::AlignLeft | Qt::TextDontClip, t.readings);
    p.drawText(val1Col,     Qt::AlignLeft | Qt::TextDontClip, t.val1);
    p.drawText(val2Col,     Qt::AlignLeft | Qt::TextDontClip, t.val2);

    p.restore();
}

// ---------------------------------------------------------------------------
// drawTxFilterWaterfallColumn()
//
// Plan 4 D9 (Cluster E).  Waterfall column fill for the TX filter band.
// Only called when m_showTxFilterOnRxWaterfall && m_moxOverlay.
// Same IQ-space mapping as drawTxFilterOverlay.
//
// Source: NereusSDR-original rendering.  IQ-space mapping per
//   deskhpsdr/transmitter.c:2136-2186 [@120188f].
// ---------------------------------------------------------------------------
void SpectrumWidget::drawTxFilterWaterfallColumn(QPainter& p, const QRect& wfRect)
{
    if (m_vfoHz <= 0.0) {
        return;
    }

    auto [iqLow, iqHigh] = txAudioToIq(m_txFilterLow, m_txFilterHigh, m_txMode);

    // m_txVfoOffsetHz tracks active XIT offset — same rationale as in
    // drawTxFilterOverlay (panadapter side); kept in lockstep (parity Task
    // 31's DUP rule included).
    const double txCenter = m_vfoHz + txFilterXitHz();
    const double absLow  = txCenter + static_cast<double>(iqLow);
    const double absHigh = txCenter + static_cast<double>(iqHigh);

    int xLow  = hzToX(absLow,  wfRect);
    int xHigh = hzToX(absHigh, wfRect);
    if (xLow > xHigh) {
        std::swap(xLow, xHigh);
    }

    const int left  = std::max(xLow,  wfRect.left());
    const int right = std::min(xHigh, wfRect.right());
    if (left >= right) {
        return;
    }
    const int bandW = right - left;

    // Fill the entire current waterfall row height with translucent orange.
    p.fillRect(QRect(left, wfRect.top(), bandW, wfRect.height()), m_txFilterColor);
}

// Paint "HIGH SWR" (and optionally "POWER FOLD BACK") text centred on the
// widget, plus a 6 px red border inset by ~3 px.
// From Thetis display.cs:4183-4201 [v2.10.3.13]
void SpectrumWidget::paintHighSwrOverlay(QPainter& p)
{
    if (!m_highSwrActive) {
        return;
    }

    // 6 px red border inset 3 px from widget edges.
    // display.cs:4200 — DrawRectangle(RectangleF(3, 3, W-6, H-6), red, 6f)
    // From Thetis display.cs:4200 [v2.10.3.13]
    const QColor kRed(255, 0, 0);  // m_bDX2_Red from display.cs
    p.save();
    p.setPen(QPen(kRed, 6));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(3, 3, -3, -3));

    // Centred "HIGH SWR" text — large bold red.
    // display.cs:4189/4193 — drawStringDX2D("HIGH SWR[\n\nPOWER FOLD BACK]", fontDX2d_font14, m_bDX2_Red, 245, 20)
    // From Thetis display.cs:4187-4194 [v2.10.3.13]
    const QString text = m_highSwrFoldback
        ? QStringLiteral("HIGH SWR\n\nPOWER FOLD BACK")
        : QStringLiteral("HIGH SWR");

    QFont f = p.font();
    f.setPointSize(48);
    f.setBold(true);
    p.setFont(f);
    p.setPen(kRed);
    p.drawText(rect(), Qt::AlignCenter, text);

    p.restore();
}

// Paint a 3 px red border around the full spectrum widget when MOX is active.
//
// Porting from Thetis display.cs:1569-1593 [v2.10.3.13] — Display.MOX setter.
// Original C# sets _mox=true and (in the drawing path) switches grid pens from
// the RX colours to the TX red variants (tx_vgrid_pen [display.cs:2086],
// tx_band_edge_pen [display.cs:1955], tx_grid_zero_pen [display.cs:2053]).
// tx_band_edge_color = Color.Red  [display.cs:1955 v2.10.3.13]
//
// NereusSDR 3M-1a draws a border tint only.  Full grid re-colouring
// (switching grid pens to TX reds) is deferred to Phase 3M-3.
// Phase 3M-1a H.1.
void SpectrumWidget::paintMoxOverlay(QPainter& p)
{
    if (!m_moxOverlay) {
        return;
    }

    // 3 px red border inset 2 px from widget edges.
    // Derives from tx_band_edge_color = Color.Red [display.cs:1955 v2.10.3.13]
    // and the 6 px HIGH SWR border (display.cs:4200); TX overlay uses 3 px
    // (half-width) so it is visually distinct from the SWR alert.
    p.save();
    p.setPen(QPen(QColor(255, 0, 0, 200), 3));  // semi-transparent red
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(2, 2, -2, -2));
    p.restore();
}

// ---- Mouse event handlers ----
// From gpu-waterfall.md:1064-1076 mouse interaction table

// ---- QRhiWidget hover event workaround ----
// QRhiWidget on macOS Metal does not deliver mouseMoveEvent without a button press.
// Workaround: m_mouseOverlay (a plain QWidget child) receives mouse tracking events.
// This eventFilter forwards them to our mouseMoveEvent/mousePressEvent/etc.
bool SpectrumWidget::eventFilter(QObject* obj, QEvent* ev)
{
    // Phase 3Q-8: clicks on the disconnect-overlay label open the connection panel.
    if (obj == m_disconnectLabel) {
        if (ev->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(ev);
            if (me->button() == Qt::LeftButton) {
                emit disconnectedClickRequest();
                return true;
            }
        }
        return false;
    }

    if (obj == m_mouseOverlay) {
        switch (ev->type()) {
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(ev);
            mouseMoveEvent(me);
            // Propagate cursor from SpectrumWidget to the overlay
            m_mouseOverlay->setCursor(cursor());
            return true;
        }
        case QEvent::MouseButtonPress:
            mousePressEvent(static_cast<QMouseEvent*>(ev));
            return true;
        case QEvent::MouseButtonRelease:
            mouseReleaseEvent(static_cast<QMouseEvent*>(ev));
            return true;
        case QEvent::MouseButtonDblClick:
            mousePressEvent(static_cast<QMouseEvent*>(ev));
            return true;
        case QEvent::Wheel:
            wheelEvent(static_cast<QWheelEvent*>(ev));
            return true;
        case QEvent::Leave:
            m_mouseInWidget = false;
            update();
            return true;
        default:
            break;
        }
    }
    return SpectrumBaseClass::eventFilter(obj, ev);
}

// ---- AetherSDR panadapter interaction model ----
// Hit-test priority from AetherSDR SpectrumWidget.cpp:824-1128
// Filter edge drag, passband slide-to-tune, divider drag, dBm drag, click-to-tune

// ===========================================================================
// Notch (TNF) geometry -- design sections 8.1 and 8.2
// ===========================================================================
// Both notch paint sites and the interaction layer use the shared panel
// layout, including the same spectrum boundary in CPU and GPU builds.
QRect SpectrumWidget::notchSpecRect() const
{
    return spectrumLayout().spectrum;
}

// ===========================================================================
// Notch (TNF) interaction -- design section 7
// ===========================================================================

// From Thetis console.cs:49039 [v2.10.3.15]: if (nHpx - nLpx > 8)
static constexpr int kNotchEdgeZoneMinPx = 8;
// From Thetis console.cs:49042 [v2.10.3.15]: if (Math.Abs(e.X - nLpx) < 4)
// and console.cs:49047 [v2.10.3.15]: else if (Math.Abs(e.X - nHpx) < 4)
static constexpr int kNotchEdgeGrabPx = 4;

const SpectrumWidget::NotchMarker* SpectrumWidget::notchMarkerById(int id) const
{
    for (const NotchMarker& n : m_notchMarkers) {
        if (n.id == id) {
            return &n;
        }
    }
    return nullptr;
}

// Pixel-space port of Thetis MNotchDB.NotchThatSurroundsFrequencyInBW:
// first-found in list order, with the pad applied only when the notch is
// narrower than twice the pad.  AetherSDR's nearest-centre tnfAtPixel
// (src/gui/SpectrumWidget.cpp:13648-13681 [@c6481cbf]) is deliberately NOT
// what governs here; design section 7.3.
//
// From Thetis radio.cs:4296-4325 [v2.10.3.15]
//MW0LGE return first notch found that surrounds a given frequency in the given bandwidth
//   [original inline comment from radio.cs:4296]
//MW0LGE return list of notches in given bandwidth
//notch is included if filter width is enough to be within the BW
//   [original inline comments from radio.cs:4274-4275, the NotchesInBW
//   prefilter this folds in]
//
// The call site supplies the arguments: Thetis passes HzInNPixels(1) as the
// pad ("we pad it with 1pixel worth of hz to make it selectable at low
// zoom", console.cs:49920 [v2.10.3.15]) and widens the bandwidth window by
// _max_filter_width on both sides (console.cs:49921 [v2.10.3.15]).
int SpectrumWidget::notchAtPixel(int x, const QRect& specRect) const
{
    if (m_notchMarkers.isEmpty() || specRect.width() <= 0) {
        return -1;
    }
    // Off-screen rejection: neither hzToX nor xToHz clamps, so a pixel
    // outside the plot maps to a frequency outside the displayed span.
    if (x < specRect.left() || x > specRect.right()) {
        return -1;
    }

    const double freqHz = xToHz(x, specRect);
    const double padHz  = m_bandwidthHz / static_cast<double>(specRect.width());
    const double windowLowHz  = m_centerHz - m_bandwidthHz / 2.0
                                - NotchModel::kMaxNotchWidthHz;
    const double windowHighHz = m_centerHz + m_bandwidthHz / 2.0
                                + NotchModel::kMaxNotchWidthHz;

    for (const NotchMarker& n : m_notchMarkers) {
        const double centreHz = n.freqMhz * 1.0e6;
        const double halfHz   = n.widthHz / 2.0;

        // NotchesInBW inclusive edge overlap.
        // From Thetis radio.cs:4286 [v2.10.3.15]
        if (centreHz + halfHz < windowLowHz) {
            continue;
        }
        if (centreHz - halfHz > windowHighHz) {
            continue;
        }

        double lowHz  = centreHz - halfHz;
        double highHz = centreHz + halfHz;
        // From Thetis radio.cs:4310 [v2.10.3.15]:
        //   if (n.FWidth < (nPadWidth * 2))
        if (n.widthHz < padHz * 2.0) {
            lowHz  -= padHz;
            highHz += padHz;
        }
        if (freqHz >= lowHz && freqHz <= highHz) {
            return n.id;
        }
    }
    return -1;
}

int SpectrumWidget::notchAtPixelForTest(int x) const
{
    return notchAtPixel(x, notchSpecRect());
}

// Edge-vs-centre discrimination for a press on a notch.
// From Thetis console.cs:49024-49067 [v2.10.3.15]
//NOTCH MW0LGE  [original section marker from console.cs:48981]
SpectrumWidget::NotchGrab SpectrumWidget::notchGrabAt(
    int id, int x, bool shiftHeld, const QRect& specRect) const
{
    const NotchMarker* n = notchMarkerById(id);
    if (!n || specRect.width() <= 0) {
        return NotchGrab::None;
    }

    const double centreHz = n->freqMhz * 1.0e6;

    // upper and lower sides of the notch
    //   [original comment from console.cs:49024]
    const double dL = centreHz - (n->widthHz / 2.0);
    const double dH = centreHz + (n->widthHz / 2.0);

    // convert the upper and lower sides into pixels from left edge of pnlDisplay
    //   [original comment from console.cs:49028]
    const int nLpx = hzToX(dL, specRect);
    const int nHpx = hzToX(dH, specRect);

    bool bNearEdge = false;

    // default this based on which side of middle the mouse is
    // so that we get inuative feeling when using shift modifier to resize
    // ie we are not draggin an edge
    //   [original comments from console.cs:49034-49036]
    NotchGrab grab = (xToHz(x, specRect) >= centreHz) ? NotchGrab::HighEdge
                                                      : NotchGrab::LowEdge;

    if (nHpx - nLpx > kNotchEdgeZoneMinPx) {
        // ok, the edges are far enough appart in pixels to actually check to see if we are over low or high side
        //   [original comment from console.cs:49041]
        if (std::abs(x - nLpx) < kNotchEdgeGrabPx) {
            grab = NotchGrab::LowEdge;
            bNearEdge = true;
        } else if (std::abs(x - nHpx) < kNotchEdgeGrabPx) {
            grab = NotchGrab::HighEdge;
            bNearEdge = true;
        }
    }

    // can also hold shift drag to resize the notch
    //   [original comment from console.cs:49056]
    // near edge of notch, let us drag the width
    //   [original comment from console.cs:49058]
    // drag whole notch, as we are not near the edge
    //   [original comment from console.cs:49064]
    return (bNearEdge || shiftHeld) ? grab : NotchGrab::Centre;
}

SpectrumWidget::NotchGrab SpectrumWidget::notchGrabAtForTest(
    int id, int x, bool shiftHeld) const
{
    return notchGrabAt(id, x, shiftHeld, notchSpecRect());
}

// Notch right-click menu.  Contents from AetherSDR
// src/gui/SpectrumWidget.cpp:8517-8572 [@c6481cbf], minus the Depth
// submenu and the Permanent toggle: WDSP's NBP notch database carries
// neither, so per design section 1.2 the per-notch Active flag takes the
// Permanent slot.  Thetis has no notch menu at all: it suppresses every
// right-click action while a notch is highlighted (console.cs:49615-49616
// [v2.10.3.15]), so the whole surface is AetherSDR's.
void SpectrumWidget::buildNotchContextMenu(int id, QMenu& menu)
{
    const NotchMarker* n = notchMarkerById(id);
    if (!n) {
        return;
    }
    const double freqMhz = n->freqMhz;
    const int    widthHz = qRound(n->widthHz);
    const bool   active  = n->active;

    // Info header.  AetherSDR renders it as a disabled QWidgetAction with
    // a two-line styled label (src/gui/SpectrumWidget.cpp:8521-8545
    // [@c6481cbf]); a disabled QAction carries the same text with no
    // styling to maintain.
    QAction* info = menu.addAction(QString("%1 MHz    %2 Hz")
                                       .arg(freqMhz, 0, 'f', 6)
                                       .arg(widthHz));
    info->setEnabled(false);
    menu.addSeparator();

    // From AetherSDR src/gui/SpectrumWidget.cpp:8548-8554 [@c6481cbf]
    //
    // Presets below the filter's achievable minimum are disabled, not hidden,
    // so the floor is visible rather than mysterious.
    //
    // 2026-08-02 bench (JJ): the list was offered unconditionally, and 50 Hz
    // is below the minimum at our default nc of 4096
    // (min_width = 1600 / (nc / 256) * (rate / 48000) = 100 Hz,
    // third_party/wdsp/src/nbp.c:88). WDSP's autoincr defaults on
    // (RXA.c:105; Thetis ships chkMNFAutoIncrease.Checked = true,
    // setup.designer.cs:44197) and silently widens a sub-minimum notch back
    // to the minimum (nbp.c:122-125). So picking 50 Hz stored 50, drew a
    // 50 Hz marker and notched 100 Hz: the menu, the marker, the settings
    // table and the DSP all disagreed with no indication.
    //
    // Thetis has no preset list at all (it uses the udMNFWidth spinner) and
    // its own widths are 200 and 100, both at or above the floor, so this
    // list is a NereusSDR addition and this clamp is what makes it honest.
    // To go genuinely narrower, raise nc: 8192 gives 50 Hz, 16384 gives 25,
    // at proportional filter cost on every channel.
    const int minWidthHz = static_cast<int>(std::ceil(m_notchMinWidthHz));
    QMenu* widthMenu = menu.addMenu(QStringLiteral("Width"));
    for (int presetHz : {50, 100, 200, 500}) {
        const bool realisable = (presetHz >= minWidthHz);
        QAction* a = widthMenu->addAction(
            realisable ? QString("%1 Hz").arg(presetHz)
                       : QString("%1 Hz  (min %2 Hz)").arg(presetHz).arg(minWidthHz),
            this,
            [this, id, presetHz]() {
                emit notchWidthRequested(id, presetHz);
            });
        a->setCheckable(true);
        a->setChecked(widthHz == presetHz);
        a->setEnabled(realisable);
        if (!realisable) {
            a->setToolTip(
                QStringLiteral("Below the narrowest notch this filter can "
                               "realise (%1 Hz). Raise the DSP filter size to "
                               "go narrower.").arg(minWidthHz));
        }
    }

    menu.addSeparator();
    // Replaces AetherSDR's Make Permanent / Make Temporary pair
    // (src/gui/SpectrumWidget.cpp:8565-8571 [@c6481cbf]) with the WDSP
    // notch's own active flag (third_party/wdsp/src/nbp.c:362,
    // RXANBPAddNotch takes fcenter / fwidth / active only).
    menu.addAction(active ? QStringLiteral("Bypass Notch")
                          : QStringLiteral("Activate Notch"),
                   this, [this, id, active]() {
                       emit notchActiveRequested(id, !active);
                   });

    menu.addSeparator();
    // From AetherSDR src/gui/SpectrumWidget.cpp:8547 [@c6481cbf]
    // ("Remove TNF").
    menu.addAction(QStringLiteral("Remove Notch"), this,
                   [this, id]() { emit notchRemoveRequested(id); });
}

// ---- Task 19: Ctrl-drag dBm-range zoom (dBm strip, right edge) ----
//
// Bounds ported verbatim -- these are upstream's own numbers, not invented
// for NereusSDR. The sibling wheel-zoom gesture a few hundred lines below
// (mx >= stripX wheelEvent branch) inlines a DIFFERENT, NereusSDR-local
// bound (qBound(10.0f, ..., 200.0f)) for the same "dynamic range" field;
// that is pre-existing code this task does not touch. This task's own
// gesture uses upstream's clampDbmRangeForBottom, and upstream's bound is
// 180, not 200 -- see tst_dbm_range_drag.cpp's clampsAtMaximum test, which
// specifically pins 180 to catch a copy-paste of the wheel gesture's 200.
//
// From AetherSDR SpectrumWidget.cpp:337-340 [@1872028c]:
//   static constexpr float kMinDisplayDbm = -180.0f;
//   static constexpr float kMaxDisplayDbm = 80.0f;
//   static constexpr float kMinDisplayRangeDb = 10.0f;
//   static constexpr float kMaxDisplayRangeDb = 180.0f;
static constexpr float kMinDisplayDbm = -180.0f;
static constexpr float kMaxDisplayDbm = 80.0f;
static constexpr float kMinDisplayRangeDb = 10.0f;
static constexpr float kMaxDisplayRangeDb = 180.0f;

// From AetherSDR SpectrumWidget.cpp:389-397 [@1872028c]
static float clampDbmBottom(float bottomDbm)
{
    if (!std::isfinite(bottomDbm)) {
        return kMinDisplayDbm;
    }
    return std::clamp(bottomDbm,
                      kMinDisplayDbm,
                      kMaxDisplayDbm - kMinDisplayRangeDb);
}

// From AetherSDR SpectrumWidget.cpp:399-410 [@1872028c]
static float clampDbmRangeForBottom(float bottomDbm, float rangeDb)
{
    bottomDbm = clampDbmBottom(bottomDbm);
    if (!std::isfinite(rangeDb)) {
        rangeDb = kMinDisplayRangeDb;
    }
    const float maxRangeForBottom =
        std::min(kMaxDisplayRangeDb, kMaxDisplayDbm - bottomDbm);
    return std::clamp(rangeDb,
                      kMinDisplayRangeDb,
                      std::max(kMinDisplayRangeDb, maxRangeForBottom));
}

void SpectrumWidget::mousePressEvent(QMouseEvent* event)
{
    // Phase 3Q-8: while disconnected, swallow all left-clicks and signal
    // MainWindow to open the ConnectionPanel instead.
    if (m_connState != ConnectionState::Connected
        && event->button() == Qt::LeftButton) {
        emit disconnectedClickRequest();
        return;
    }

    // iPhone app plan Task 78: a click on another device's slice label says
    // whose it is; it tunes nothing and starts no drag.
    if (event->button() == Qt::LeftButton) {
        const QPoint at = event->position().toPoint();
        for (const ForeignSliceMarker& m : m_foreignMarkers) {
            const QRect label = m_foreignLabelRects.value(m.sliceId);
            if (label.isValid() && label.contains(at)) {
                emit foreignMarkerClicked(m.sliceId, foreignMarkerExplanation(m));
                event->accept();
                return;
            }
        }
    }

    const SpectrumLayout layout = spectrumLayout();
    const int specH = layout.spectrum.height();
    const int dividerY = layout.divider.top();
    const QRect specRect = layout.spectrum;
    int mx = static_cast<int>(event->position().x());
    int my = static_cast<int>(event->position().y());

    // Double-click on off-screen indicator → recenter pan on VFO
    if (event->type() == QEvent::MouseButtonDblClick
        && event->button() == Qt::LeftButton
        && m_vfoOffScreen != VfoOffScreen::None) {
        if ((m_vfoOffScreen == VfoOffScreen::Left && mx < specRect.left() + 60)
            || (m_vfoOffScreen == VfoOffScreen::Right && mx > specRect.right() - 60)) {
            recenterOnVfo();
            return;
        }
    }

    if (event->button() == Qt::RightButton) {
        // The dBm strip claims right-press first: on that strip a right drag
        // adjusts the RANGE (Thetis gridmaxadjust, PanDisplay.cs:4424-4431
        // [v2.10.3.15]), and the notch / spot / overlay menus below would
        // otherwise swallow it before the strip hit-test further down ever
        // ran. The strip is 36 px of the right edge above the divider, so
        // this takes nothing away from the menus.
        {
            if (isOnDbmStrip(QPoint(mx, my))) {
                m_draggingDbmRange = true;
                m_dragStartY       = my;
                m_dragStartRef     = m_refLevel;
                m_dragStartFloor   = m_refLevel - m_dynamicRange;
                setCursor(Qt::SizeVerCursor);
                event->accept();
                return;
            }
        }

        // Ctrl + right-click adds a notch at the clicked frequency; Shift
        // makes it narrow.  Checked before the spot menu and the overlay
        // menu, so plain right-click behaviour is unchanged when Ctrl is
        // not held (design section 7.3).
        //
        // From Thetis console.cs:49614-49646 [v2.10.3.15]:
        //   case MouseButtons.Right: -> if (Common.CtrlKeyDown) ->
        //   AddNotch(dFreq, rx).  The "add notch from cross hair mode with
        //   middle mouse" comment at console.cs:49633 is stale: the only
        //   MouseButtons.Middle branch (console.cs:49725) toggles active or
        //   removes on Shift, and never adds (design section 7.1).
        //
        // Both Control and Meta count: macOS swaps them, so the physical
        // Ctrl key arrives as Qt::MetaModifier.  The zoom wheel below
        // already accepts either.
        const bool notchCtrlHeld =
            (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) != 0;
        if (notchCtrlHeld && my < specH && mx <= specRect.right()) {
            const bool narrow = (event->modifiers() & Qt::ShiftModifier) != 0;
            emit notchCreateRequested(xToHz(mx, specRect), narrow);
            event->accept();
            return;
        }

        // Right-click on a notch marker opens the notch menu.  Thetis
        // suppresses every other right-click action while a notch is
        // highlighted:
        // if we have a notch highlighted, then all other right click is ignored
        //   [original comment from console.cs:49615, guarding the return at
        //   console.cs:49616 [v2.10.3.15]].  NereusSDR fills that suppressed
        //   slot with AetherSDR's notch menu rather than doing nothing.
        if (my < specH && mx <= specRect.right()) {
            const int hitNotch = notchAtPixel(mx, specRect);
            if (hitNotch >= 0) {
                QMenu menu(this);
                buildNotchContextMenu(hitNotch, menu);
                menu.exec(event->globalPosition().toPoint());
                event->accept();
                return;
            }
        }

        // 2026-05-12 bench fix (Gaps #3 + #5 — right-click context menu
        // on spot labels + memory-spot variant).  Ported from
        // AetherSDR SpectrumWidget.cpp:1779-1822 [@0cd4559].  Without
        // this the right-click always falls through to the overlay
        // menu (waterfall colors / ref level / etc.) even when the
        // user is clearly targeting a spot label.
        //
        // Menu actions (non-Memory source):
        //   - Tune to <call>         -> frequencyClicked(hz)
        //   - Copy Callsign          -> clipboard
        //   - Lookup on QRZ          -> https://www.qrz.com/db/<call>
        //   - Remove Spot            -> spotRemoveRequested(index)
        // Memory source replaces the menu with a single action that
        // applies the memory (different verb: "Apply <call>"), matching
        // upstream behaviour.
        if (m_showSpots) {
            const QPoint hitPos(mx, my);
            int hitMarkerIdx = -1;
            for (int i = 0; i < m_spotClickRects.size(); ++i) {
                if (m_spotClickRects[i].rect.contains(hitPos)) {
                    hitMarkerIdx = m_spotClickRects[i].markerIndex;
                    break;
                }
            }
            if (hitMarkerIdx >= 0 && hitMarkerIdx < m_spotMarkers.size()) {
                const auto& sm = m_spotMarkers[hitMarkerIdx];
                const int   spotIndex = sm.index;
                const QString call    = sm.callsign;
                const double freqHz   = sm.freqMhz * 1.0e6;
                const QString source  = sm.source;

                QMenu menu(this);
                if (source == QStringLiteral("Memory")) {
                    const QString title = call.isEmpty()
                        ? QStringLiteral("Apply Memory")
                        : QString("Apply %1").arg(call);
                    menu.addAction(title, this,
                        [this, freqHz, spotIndex]() {
                            requestTune(freqHz);
                            emit spotTriggered(spotIndex);
                        });
                } else {
                    menu.addAction(QString("Tune to %1").arg(call), this,
                        [this, freqHz]() {
                            requestTune(freqHz);
                        });
                    menu.addAction(QStringLiteral("Copy Callsign"), this,
                        [call]() {
                            QApplication::clipboard()->setText(call);
                        });
                    menu.addAction(QStringLiteral("Lookup on QRZ"), this,
                        [call]() {
                            QDesktopServices::openUrl(
                                QUrl(QStringLiteral("https://www.qrz.com/db/")
                                     + call));
                        });
                    menu.addSeparator();
                    menu.addAction(QStringLiteral("Remove Spot"), this,
                        [this, spotIndex]() {
                            emit spotRemoveRequested(spotIndex);
                        });
                }
                menu.exec(event->globalPosition().toPoint());
                event->accept();
                return;
            }
        }

        // Show overlay menu on right-click (default — not on a spot).
        if (!m_overlayMenu) {
            m_overlayMenu = new SpectrumOverlayMenu(this);
            // 3D Stacked-Trace Spectrum Plan Task 20: every popup signal
            // that maps onto one of DisplaySettingsModel's fifteen
            // values connects straight to the matching model setter --
            // never to this widget's own named setter, and never through
            // a lambda that touches a widget member. Task 18's own
            // model-to-widget binding (bindDisplaySettings()) is what
            // actually lands the change on the live renderer from here;
            // this popup no longer talks to SpectrumWidget at all for
            // these fourteen. ctunChanged and notchAddRequested are not
            // among the fifteen (CTUN-enabled and "add a notch" are not
            // DisplaySettingsModel fields) and keep talking to the widget
            // exactly as before.
            connect(m_overlayMenu, &SpectrumOverlayMenu::wfColorGainChanged,
                    m_displaySettings, &DisplaySettingsModel::setWfColorGain);
            connect(m_overlayMenu, &SpectrumOverlayMenu::wfBlackLevelChanged,
                    m_displaySettings, &DisplaySettingsModel::setWfBlackLevel);
            connect(m_overlayMenu, &SpectrumOverlayMenu::wfColorSchemeChanged,
                    m_displaySettings, &DisplaySettingsModel::setWfColorScheme);
            connect(m_overlayMenu, &SpectrumOverlayMenu::fillAlphaChanged,
                    m_displaySettings, &DisplaySettingsModel::setFillAlpha);
            connect(m_overlayMenu, &SpectrumOverlayMenu::panFillChanged,
                    m_displaySettings, &DisplaySettingsModel::setPanFill);
            connect(m_overlayMenu, &SpectrumOverlayMenu::refLevelChanged,
                    m_displaySettings, &DisplaySettingsModel::setRefLevel);
            connect(m_overlayMenu, &SpectrumOverlayMenu::dynRangeChanged,
                    m_displaySettings, &DisplaySettingsModel::setDynamicRange);
            connect(m_overlayMenu, &SpectrumOverlayMenu::ctunChanged,
                    this, [this](bool v) { setCtunEnabled(v); });
            // Plan decision D-e: the empty-pan "add a notch here" row.
            // The overlay-menu route always places the default width, so
            // narrow is false; Ctrl + Shift + right-click on the pan is
            // the narrow variant.
            connect(m_overlayMenu, &SpectrumOverlayMenu::notchAddRequested,
                    this, [this](double freqHz) {
                        emit notchCreateRequested(freqHz, false);
                    });
            // 3D VIEW section (Task 13, re-pointed at the model by Task
            // 20; Task 24 adds the seventh, dssRowDividerChanged): seven
            // signals wired straight to DisplaySettingsModel's
            // setters -- see the block comment above. Persistence
            // (AppSettings/PanadapterModel round-trip) is Task 14's
            // scope, not this one's; the model's own model-to-widget
            // binding still reaches the widget's setter (and its
            // scheduleSettingsSave() call) exactly as before, just one
            // hop further along.
            connect(m_overlayMenu, &SpectrumOverlayMenu::spectrumRenderModeChanged,
                    m_displaySettings, &DisplaySettingsModel::setSpectrumRenderMode);
            connect(m_overlayMenu, &SpectrumOverlayMenu::dssFloorDepthChanged,
                    m_displaySettings, &DisplaySettingsModel::setDssFloorDepth);
            connect(m_overlayMenu, &SpectrumOverlayMenu::dssGainChanged,
                    m_displaySettings, &DisplaySettingsModel::setDssGain);
            connect(m_overlayMenu, &SpectrumOverlayMenu::dssRowSpanChanged,
                    m_displaySettings, &DisplaySettingsModel::setDssRowSpan);
            connect(m_overlayMenu, &SpectrumOverlayMenu::dssAngleChanged,
                    m_displaySettings, &DisplaySettingsModel::setDssAngle);
            connect(m_overlayMenu, &SpectrumOverlayMenu::dssRowDividerChanged,
                    m_displaySettings, &DisplaySettingsModel::setDssRowDivider);
            connect(m_overlayMenu, &SpectrumOverlayMenu::dssSliceShadowChanged,
                    m_displaySettings, &DisplaySettingsModel::setThreeDSliceDepth);

            // Live refresh (Task 20): while the popup stays open, a
            // model-driven change from any OTHER bound surface (Setup ->
            // Display today; a future left-panel applet) re-seeds every
            // popup control so it never shows a stale value. The re-seed
            // reads the model's OWN current state (never a signal
            // argument) and calls setValues()/setDssValues(), which
            // block the popup's own controls' signals while seeding --
            // so this can never feed a popup signal back out, which is
            // what makes it safe to wire unconditionally: the popup's
            // OWN change already reached the model above, so the model's
            // signal fires, this lambda runs, and it reseeds the popup
            // with the same value it just sent, a Qt/QSlider no-op.
            // isVisible() additionally skips all fourteen while the
            // popup is closed, so a change made with the popup not open
            // does not do fourteen no-op reseeds on the next right-click
            // (setValues()/setDssValues() below already reseed it then).
            auto refreshOverlayMenuFromModel = [this]() {
                if (!m_overlayMenu->isVisible()) { return; }
                m_overlayMenu->setValues(m_displaySettings->wfColorGain(),
                                          m_displaySettings->wfBlackLevel(), false,
                                          m_displaySettings->wfColorScheme(),
                                          m_displaySettings->fillAlpha(),
                                          m_displaySettings->panFill(), false,
                                          m_displaySettings->refLevel(),
                                          m_displaySettings->dynamicRange(),
                                          m_ctunEnabled);
                m_overlayMenu->setDssValues(m_displaySettings->spectrumRenderMode(),
                                             m_displaySettings->dssFloorDepth(),
                                             m_displaySettings->dssGain(),
                                             m_displaySettings->dssRowSpan(),
                                             m_displaySettings->dssAngle(),
                                             m_displaySettings->threeDSliceDepth(),
                                             m_displaySettings->dssRowDivider());
            };
            connect(m_displaySettings, &DisplaySettingsModel::wfColorSchemeChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::wfColorGainChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::wfBlackLevelChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::refLevelChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::dynamicRangeChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::fillAlphaChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::panFillChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::spectrumRenderModeChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::dssFloorDepthChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::dssGainChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::dssRowSpanChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::dssAngleChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::dssRowDividerChanged,
                    this, refreshOverlayMenuFromModel);
            connect(m_displaySettings, &DisplaySettingsModel::threeDSliceDepthChanged,
                    this, refreshOverlayMenuFromModel);
        }
        m_overlayMenu->setValues(m_displaySettings->wfColorGain(),
                                  m_displaySettings->wfBlackLevel(), false,
                                  m_displaySettings->wfColorScheme(),
                                  m_displaySettings->fillAlpha(),
                                  m_displaySettings->panFill(), false,
                                  m_displaySettings->refLevel(),
                                  m_displaySettings->dynamicRange(),
                                  m_ctunEnabled);
        m_overlayMenu->setCtunAvailable(m_ctunAvailable);
        // Re-seed every popup (not just at construction): the 3D VIEW
        // section reflects whatever the operator last set, and row-span
        // support can change across the widget's lifetime if the GPU mesh
        // pipeline comes up or falls back (Task 10's RGBA16F check).
        m_overlayMenu->setDssValues(m_displaySettings->spectrumRenderMode(),
                                     m_displaySettings->dssFloorDepth(),
                                     m_displaySettings->dssGain(),
                                     m_displaySettings->dssRowSpan(),
                                     m_displaySettings->dssAngle(),
                                     m_displaySettings->threeDSliceDepth(),
                                     m_displaySettings->dssRowDivider());
        // dssMeshReady() alone is build-safe (CPU build hardcodes false), but
        // route through dssRowSpanSupported() anyway to match upstream's two
        // independent gates (compile-time GPU build + runtime mesh-ready)
        // explicitly rather than folding them together implicitly.
#ifdef NEREUS_GPU_SPECTRUM
        m_overlayMenu->setDssRowSpanSupported(
            dssRowSpanSupported(/*gpuSpectrumBuild=*/true, dssMeshReady()));
#else
        m_overlayMenu->setDssRowSpanSupported(
            dssRowSpanSupported(/*gpuSpectrumBuild=*/false, dssMeshReady()));
#endif
        // The frequency under the cursor, captured at popup time: the
        // popup outlives the press, and by the time the button is clicked
        // the pointer has moved onto the popup itself.
        m_overlayMenu->setNotchAddFrequency(xToHz(mx, specRect));

        // Clamp onto the screen before showing.
        //
        // Qt constrains a QMenu to the screen on its own but does NOT do the
        // same for a plain QWidget carrying Qt::Popup, which is what
        // SpectrumOverlayMenu is: asked for a y near the bottom edge it is
        // placed there verbatim and its body hangs off the screen. That never
        // showed while there was one full-height pan whose top sat high on the
        // display; on the LOWER pan of a 2v layout -- right-clicking its
        // waterfall, which is exactly what an operator does when they want the
        // waterfall controls -- the panel opened below the visible area.
        // Bench-reported 2026-07-28: "I have now lost the ability to adjust the
        // second RX waterfall via the right click menu."
        //
        // adjustSize() first: on the very first right-click the popup has never
        // been laid out, so size() would still be the default and the clamp
        // would be computed against the wrong height.
        const QPoint desired = event->globalPosition().toPoint();
        m_overlayMenu->adjustSize();
        const QScreen* screen = QGuiApplication::screenAt(desired);
        if (!screen) { screen = QGuiApplication::primaryScreen(); }
        m_overlayMenu->move(PopupPlacement::clampToAvailable(
            desired, m_overlayMenu->size(),
            screen ? screen->availableGeometry() : QRect()));
        m_overlayMenu->show();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // Phase 3J-2 Task E1: spot label / cluster badge hit-test.
    // Click on a spot label → tune to that frequency and notify spot
    // sources via spotTriggered(index). Click on a cluster badge (+N)
    // → show popup menu of the collapsed spots.
    // From AetherSDR src/gui/SpectrumWidget.cpp:1623-1644 [@0cd4559]
    if (m_showSpots) {
        const QPoint pos(mx, my);
        for (const auto& hr : m_spotClickRects) {
            if (hr.rect.contains(pos)) {
                // NereusSDR signal contract: frequencyClicked(double hz).
                // AetherSDR emits MHz; multiply by 1e6 to match Hz signature.
                requestTune(hr.freqMhz * 1.0e6);
                // Notify the radio that a spot was clicked (#341)
                if (hr.markerIndex >= 0 && hr.markerIndex < m_spotMarkers.size()) {
                    emit spotTriggered(m_spotMarkers[hr.markerIndex].index);
                }
                event->accept();
                return;
            }
        }
        // Click on a cluster badge → show popup with collapsed spots
        for (const auto& cluster : m_spotClusters) {
            if (cluster.rect.contains(pos)) {
                showSpotClusterPopup(cluster, mapToGlobal(pos));
                event->accept();
                return;
            }
        }
    }

    // 1. dBm scale strip — right edge. Arrow row adjusts ref level,
    // body is drag-pan. From AetherSDR SpectrumWidget.cpp:1712-1745 [@0cd4559]
    // Sub-epic E: hit-test against the actual dBm-strip width, not the
    // effectiveStripW() layout reservation (which widens to 72px when paused
    // to make room for the time-scale strip's UTC labels — but the dBm strip
    // itself stays at kDbmStripW = 36px wide).
    const int stripX = width() - kDbmStripW;
    if (mx >= stripX && effectiveStripW() > 0 && my < specH) {
        // Use FULL-WIDTH rect so stripRect() lands in the reserved zone.
        // Matches the rect passed to drawDbmScale in paintEvent.
        const QRect fullSpecRect(0, 0, width(), specH);
        const QRect strip    = NereusSDR::DbmStrip::stripRect(fullSpecRect, kDbmStripW);
        const QRect arrowRow = NereusSDR::DbmStrip::arrowRowRect(strip, kDbmArrowH);

        // Task 19: Ctrl-drag (Cmd/Meta too -- macOS swaps them, same
        // rationale as the notch-add modifier check above and the
        // Ctrl/Cmd+scroll bandwidth zoom in wheelEvent below) zooms the
        // dBm span instead of panning it. Checked ahead of the arrow-row
        // hit test, matching upstream precedence: a Ctrl-click landing on
        // the arrow row still starts a range-drag rather than nudging ref
        // level (upstream's controlClick branch runs before its
        // `if (y < DBM_ARROW_H)` arrow check).
        // From AetherSDR SpectrumWidget.cpp:9520-9550 [@1872028c]
        if (event->button() == Qt::LeftButton
            && (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier))) {
            m_draggingDbmRangeZoom = true;
            m_dbmRangeDragStartY = my;
            m_dbmRangeDragStartRange = m_dynamicRange;
            m_dbmRangeDragStartBottom = m_refLevel - m_dynamicRange;
            setCursor(Qt::SizeVerCursor);
            return;
        }

        if (arrowRow.contains(mx, my)) {
            const int hit = NereusSDR::DbmStrip::arrowHit(mx, arrowRow);
            const float bottom = m_refLevel - m_dynamicRange;
            if (hit == 0) {
                // Up arrow: raise ref level by 10 dB, keep bottom fixed
                m_refLevel += 10.0f;
            } else if (hit == 1) {
                // Down arrow: lower ref level by 10 dB, keep bottom fixed
                m_refLevel -= 10.0f;
            }
            m_dynamicRange = m_refLevel - bottom;
            if (m_dynamicRange < 10.0f) {
                m_dynamicRange = 10.0f;
                m_refLevel = bottom + m_dynamicRange;
            }
            emit dbmRangeChangeRequested(m_refLevel - m_dynamicRange, m_refLevel);
            scheduleSettingsSave();
            update();
            syncDisplaySettingsFromWidget(); // Task 18
            return;
        }

        // Below arrows: left pans the window, right stretches it.
        // From Thetis PanDisplay.cs:4105-4115 [v2.10.3.15] (left ->
        // gridminmaxadjust) and :4424-4431 (right -> gridmaxadjust).
        m_dragStartY   = my;
        m_dragStartRef = m_refLevel;
        if (event->button() == Qt::RightButton) {
            m_draggingDbmRange = true;
            m_dragStartFloor   = m_refLevel - m_dynamicRange;
        } else {
            m_draggingDbm = true;
        }
        // Task 16: captured unconditionally, same as m_dragStartRef above,
        // so mouseMoveEvent's 3D branch has a baseline regardless of which
        // mode is active when the press lands (mode cannot change mid-drag).
        m_dragStartDssFloorDepth = m_dssFloorDepth;
        setCursor(Qt::SizeVerCursor);
        return;
    }

    // 2. Divider bar (thin line) — resize spectrum/waterfall split up/down
    // Grab zone extends 6px above/below the 4px visual line for easier targeting
    static constexpr int kDividerGrab = 6;
    if (my >= dividerY - kDividerGrab && my < dividerY + kDividerH + kDividerGrab) {
        m_draggingDivider = true;
        setCursor(Qt::SplitVCursor);
        return;
    }

    // 3. Frequency scale bar (wider gray bar below divider) — zoom bandwidth left/right
    const int freqBarY = layout.frequency.top();
    if (my >= freqBarY && my < freqBarY + kFreqScaleH) {
        // Sub-epic E: the LIVE button sits in the freq-scale row, so its
        // hit-test MUST run before the bandwidth-drag below grabs the click.
        // From AetherSDR SpectrumWidget.cpp:1655-1660 [@0cd4559]
        const QRect wfRect = layout.waterfallChrome;
        if (waterfallLiveButtonRect(wfRect).contains(event->position().toPoint())
            && event->button() == Qt::LeftButton) {
            setWaterfallLive(true);
            event->accept();
            return;
        }

        m_draggingBandwidth = true;
        m_bwDragStartX = mx;
        m_bwDragStartBw = m_bandwidthHz;
        setCursor(Qt::SizeHorCursor);
        return;
    }

    // 3b. Notch (TNF) grab.  Thetis runs its notch block first inside
    // case MouseButtons.Left:, ahead of every filter drag, so a notch
    // marker sitting on a filter edge still drags as a notch.  The dBm
    // strip / divider / freq-scale rows above are NereusSDR chrome outside
    // the spectrum plot and keep their existing precedence, so the notch
    // test is guarded on my < specH.
    //
    // The press also writes m_selectedNotchId, so a press is
    // self-contained rather than relying on a prior hover, and the grab
    // latches the notch for the whole gesture: hover stops writing until
    // release, so an overlapping neighbour cannot steal the drag
    // (design section 7.3).
    //
    // From Thetis console.cs:48981-48998 [v2.10.3.15]
    //NOTCH MW0LGE  [original section marker from console.cs:48981]
    if (my < specH) {
        const int hitNotch = notchAtPixel(mx, specRect);
        if (m_selectedNotchId != hitNotch) {
            m_selectedNotchId = hitNotch;
            markOverlayDirty();
        }
        if (hitNotch >= 0) {
            const bool shiftHeld =
                (event->modifiers() & Qt::ShiftModifier) != 0;
            m_notchGrab = notchGrabAt(hitNotch, mx, shiftHeld, specRect);
            const NotchMarker* n = notchMarkerById(hitNotch);
            // the inital click point, delta is worked in mouse_move
            //   [original comment from console.cs:48997]
            m_notchDragStartX = mx;
            // From Thetis console.cs:49059 [v2.10.3.15] (width, edge drag)
            // and console.cs:49065 [v2.10.3.15] (centre, whole-notch drag).
            m_notchDragStartData = (m_notchGrab == NotchGrab::Centre)
                ? (n ? n->freqMhz * 1.0e6 : 0.0)
                : (n ? n->widthHz : 0.0);
            setCursor(Qt::SizeHorCursor);
            event->accept();
            return;
        }
    }

    // Compute filter edge pixel positions for hit-testing
    double loHz = m_vfoHz + m_filterLowHz;
    double hiHz = m_vfoHz + m_filterHighHz;
    int xLo = hzToX(loHz, specRect);
    int xHi = hzToX(hiHz, specRect);
    if (xLo > xHi) { std::swap(xLo, xHi); }

    // 3. Filter edge grab — ±5px from edge
    // From AetherSDR SpectrumWidget.cpp:1080-1109
    bool loHit = std::abs(mx - xLo) <= kFilterGrab;
    bool hiHit = std::abs(mx - xHi) <= kFilterGrab;
    if (loHit || hiHit) {
        if (loHit && hiHit) {
            // Both edges within grab range — pick closer one
            m_draggingFilter = (std::abs(mx - xLo) <= std::abs(mx - xHi))
                ? FilterEdge::Low : FilterEdge::High;
        } else {
            m_draggingFilter = loHit ? FilterEdge::Low : FilterEdge::High;
        }
        m_filterDragStartX = mx;
        m_filterDragStartHz = (m_draggingFilter == FilterEdge::Low)
            ? m_filterLowHz : m_filterHighHz;
        setCursor(Qt::SizeHorCursor);
        return;
    }

    // 4. Inside passband — slide-to-tune (VFO drag)
    // From AetherSDR SpectrumWidget.cpp:1112-1119
    int left = std::min(xLo, xHi);
    int right = std::max(xLo, xHi);
    if (mx > left + kFilterGrab && mx < right - kFilterGrab) {
        m_draggingVfo = true;
        setCursor(Qt::SizeHorCursor);
        return;
    }

    // Sub-epic E: time-scale strip + LIVE button
    // From AetherSDR SpectrumWidget.cpp:1655-1693 [@0cd4559]
    const int wfY = layout.waterfall.top();
    const QRect wfRect = layout.waterfallChrome;

    // LIVE button click (sits in the freq-scale row above the waterfall)
    if (waterfallLiveButtonRect(wfRect).contains(event->position().toPoint())
        && event->button() == Qt::LeftButton) {
        setWaterfallLive(true);
        event->accept();
        return;
    }

    // Time-scale strip drag start (right edge of waterfall)
    if (my >= wfY && event->button() == Qt::LeftButton) {
        const QRect timeScaleRect = waterfallTimeScaleRect(wfRect);
        const QPoint pos = event->position().toPoint();
        if (timeScaleRect.contains(pos)) {
            m_draggingTimeScale = true;
            m_timeScaleDragStartY = my;
            m_timeScaleDragStartOffsetRows = m_wfHistoryOffsetRows;
            setCursor(Qt::SizeVerCursor);
            event->accept();
            return;
        }
    }

    // 5. Pan drag — click in spectrum/waterfall area and drag to pan the view
    // From AetherSDR SpectrumWidget.cpp:879-887
    m_draggingPan = true;
    m_panDragStartX = mx;
    m_panDragStartCenter = m_centerHz;
    setCursor(Qt::ClosedHandCursor);
    // Don't emit click-to-tune — the release event handles that if drag distance is small

    QWidget::mousePressEvent(event);
}

void SpectrumWidget::mouseMoveEvent(QMouseEvent* event)
{
    m_mousePos = event->pos();
    m_mouseInWidget = true;
    int mx = static_cast<int>(event->position().x());
    int my = static_cast<int>(event->position().y());
    const int w = width();
    const SpectrumLayout layout = spectrumLayout();
    const int specH = layout.spectrum.height();
    const QRect specRect = layout.spectrum;

    // --- Active drag modes ---

    // Sub-epic E: time-scale drag = scrub through history
    // From AetherSDR SpectrumWidget.cpp:2122-2145 [@0cd4559]
    if (m_draggingTimeScale) {
        const QRect wfRect = layout.waterfallChrome;
        const QRect timeScaleRect = waterfallTimeScaleRect(wfRect);
        const int dragHeight = std::max(1, timeScaleRect.height());
        const int maxOffset = maxWaterfallHistoryOffsetRows();
        const int dy = m_timeScaleDragStartY - my;  // pull up = scroll back
        const int deltaRows = (maxOffset > 0)
            ? static_cast<int>(std::round(
                (static_cast<double>(dy) / dragHeight) * maxOffset))
            : 0;
        const int newOffset = std::clamp(
            m_timeScaleDragStartOffsetRows + deltaRows, 0, maxOffset);

        if (newOffset != m_wfHistoryOffsetRows) {
            m_wfHistoryOffsetRows = newOffset;
            if (newOffset > 0) {
                m_wfLive = false;  // entering paused state
            }
            rebuildWaterfallViewport();
            markOverlayDirty();
        }

        setCursor(Qt::SizeVerCursor);
        event->accept();
        return;
    }

    // Notch drag: whole-notch move, or width from the latched edge.  The
    // pixel delta is worked here from the press point recorded above; the
    // NotchModel owns every clamp (min/max centre, 0..10000 width), so
    // this layer emits the raw request.  AetherSDR's y-axis width gesture
    // (src/gui/SpectrumWidget.cpp:9051-9070 [@c6481cbf]) is deliberately
    // declined, so vertical movement during a centre drag cannot silently
    // change the width (design section 7.2).
    //
    // From Thetis console.cs:49928-49968 [v2.10.3.15] (centre)
    //MW0LGE [2.9.0.7] update on drag
    //   [original inline comment from console.cs:49967 — Thetis pushes on
    //   every mouse-move by named design; design section 6.2 says do not
    //   add throttling here.]
    // From Thetis console.cs:49971-49987 [v2.10.3.15] (width)
    if (m_notchGrab != NotchGrab::None && m_selectedNotchId >= 0
        && specRect.width() > 0) {
        const double hzPerPx = m_bandwidthHz / specRect.width();
        if (m_notchGrab == NotchGrab::Centre) {
            // drag the whole notch  [original comment from console.cs:49930]
            const double diff = (mx - m_notchDragStartX) * hzPerPx;
            emit notchMoveRequested(m_selectedNotchId,
                                    m_notchDragStartData + diff);
        } else {
            // drag the bw edges of the notch
            //   [original comment from console.cs:49973]
            const double diff = (m_notchGrab == NotchGrab::HighEdge)
                ? (mx - m_notchDragStartX) * hzPerPx
                : (m_notchDragStartX - mx) * hzPerPx;
            // we want double the diff, as we are doing 'both sides'
            //   [original comment from console.cs:49984]
            emit notchWidthRequested(m_selectedNotchId,
                                     m_notchDragStartData + (diff * 2.0));
        }
        setCursor(Qt::SizeHorCursor);
        markOverlayDirty();
        event->accept();
        return;
    }

    // A drag ends when its button is no longer held, whether or not a
    // release event ever reached us. On macOS a right-press that the window
    // system turns into a context-menu gesture can swallow the matching
    // release, and the drag flag then stays set forever: every subsequent
    // mouse move keeps dragging the scale with no button down and no way to
    // stop. Bench 2026-08-05, "i cant let go, it just goes up and down no
    // matter where the mouse is". Checking the live button state costs
    // nothing and cannot get stuck.
    //
    // A drag that ends here is a drag that ENDED, so it has to finalise the
    // same way mouseReleaseEvent does. The first version only cleared the
    // flag, so on exactly the macOS case this exists for, the scale change
    // held for the session and was thrown away at exit: no
    // dbmRangeChangeRequested for the observers, no scheduleSettingsSave.
    // The operator drags the scale into shape, quits, and comes back to the
    // old one having done nothing wrong. Found by Codex on PR #317.
    if (m_draggingDbm && !(event->buttons() & Qt::LeftButton)) {
        m_draggingDbm = false;
        setCursor(Qt::ArrowCursor);
        emit dbmRangeChangeRequested(m_refLevel - m_dynamicRange, m_refLevel);
        scheduleSettingsSave();
    }
    if (m_draggingDbmRange && !(event->buttons() & Qt::RightButton)) {
        m_draggingDbmRange = false;
        setCursor(Qt::ArrowCursor);
        emit dbmRangeChangeRequested(m_refLevel - m_dynamicRange, m_refLevel);
        scheduleSettingsSave();
    }
    if (m_draggingDbmRangeZoom && !(event->buttons() & Qt::LeftButton)) {
        m_draggingDbmRangeZoom = false;
        setCursor(Qt::ArrowCursor);
        emit dbmRangeChangeRequested(m_refLevel - m_dynamicRange, m_refLevel);
        scheduleSettingsSave();
    }

    // Ctrl-drag zooms the dBm span with the bottom pinned. This runs in
    // both render modes because m_dynamicRange also supplies the 3D span.
    if (m_draggingDbmRangeZoom) {
        const int dragHeight = std::max(1, specH);
        const int dy = m_dbmRangeDragStartY - my;
        const float deltaDb = (static_cast<float>(dy) / static_cast<float>(dragHeight))
            * m_dbmRangeDragStartRange;
        m_dynamicRange = clampDbmRangeForBottom(m_dbmRangeDragStartBottom,
                                                 m_dbmRangeDragStartRange + deltaDb);
        m_refLevel = m_dbmRangeDragStartBottom + m_dynamicRange;
        markOverlayDirty();
        syncDisplaySettingsFromWidget(); // Task 18
        return;
    }

    if (m_draggingDbm) {
        const int dy = my - m_dragStartY;

        if (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) {
            const float depthPerPixel = 24.0f / static_cast<float>(specH);
            const int newDepth = m_dragStartDssFloorDepth
                + qRound(static_cast<float>(dy) * depthPerPixel);
            setDssFloorDepth(newDepth);
            return;
        }

        // From Thetis PanDisplay.cs:3688-3699 [v2.10.3.15]:
        //     int delta_y = e.Y - grid_minmax_drag_start_point.Y;
        //     double delta_db = ((double)delta_y / 10) * 5;
        //     val += (decimal)delta_db;  min_val += (decimal)delta_db;
        //     if (val > 200) val = 200;
        //     if (min_val < -200) min_val = -200;
        //
        // A FIXED half-dB per pixel, not a fraction of the current range.
        // Ours scaled by m_dynamicRange / specH, which on a 49 dB receive
        // window came out around 0.13 dB per pixel -- roughly a quarter of
        // Thetis's, so the scale barely moved and the drag felt dead. It
        // also meant the drag's feel changed with the zoom level, which
        // Thetis's does not.
        static constexpr float kDbPerPixel = 0.5f;

        float newRef = m_dragStartRef + static_cast<float>(dy) * kDbPerPixel;

        // Thetis's own limits. The old +20 ceiling was the reason dragging
        // up did nothing at all while transmitting: the transmit grid's
        // reference level IS +20 (display.cs:1887 tx_spectrum_grid_max), so
        // the drag started already pinned against the stop.
        // Bench 2026-08-05.
        newRef = qBound(-200.0f + m_dynamicRange, newRef, 200.0f);
        m_refLevel = newRef;
        syncDisplaySettingsFromWidget();
        // The scale labels and grid lines live in the CACHED overlay texture
        // (renderGpuFrame draws them only under `if (m_overlayStaticDirty)`),
        // while the trace is vertex geometry rebuilt every frame. update()
        // alone therefore moved the trace and left the numbers frozen at
        // whatever range was live when something else last invalidated the
        // overlay -- which is the whole complaint: "the spectrum goes up and
        // down but the scale does not adjust". Bench 2026-08-05.
        markOverlayDirty();
        update();
        return;
    }

    if (m_draggingDbmRange) {
        // From Thetis PanDisplay.cs:3702-3712 [v2.10.3.15]:
        //     if (gridmaxadjust) {
        //         int delta_y = e.Y - grid_minmax_drag_start_point.Y;
        //         double delta_db = ((double)delta_y / 10) * 5;
        //         decimal val = grid_minmax_max_y;  val += (decimal)delta_db;
        //         if (val > 200) val = 200;
        //         SpectrumGridMax = (int)val;
        //     }
        // Only the ceiling moves; the floor is untouched, so the RANGE is
        // what changes. Same half-dB per pixel as the pan.
        static constexpr float kDbPerPixel = 0.5f;

        const int dy = my - m_dragStartY;
        float newRef = m_dragStartRef + static_cast<float>(dy) * kDbPerPixel;
        newRef = qMin(newRef, 200.0f);

        // The floor stays put, so guard the range rather than the ceiling:
        // a ceiling dragged to or below the floor would invert the scale.
        float newRange = newRef - m_dragStartFloor;
        if (newRange < 10.0f) {
            newRange = 10.0f;
            newRef   = m_dragStartFloor + newRange;
        } else if (newRange > 200.0f) {
            newRange = 200.0f;
            newRef   = m_dragStartFloor + newRange;
        }

        m_refLevel     = newRef;
        m_dynamicRange = newRange;
        markOverlayDirty();
        update();
        syncDisplaySettingsFromWidget(); // Task 18
        return;
    }

    if (m_draggingFilter != FilterEdge::None) {
        // Compute new filter Hz from pixel delta
        // From AetherSDR SpectrumWidget.cpp:1203-1220
        double hzPerPx = m_bandwidthHz / specRect.width();
        int newHz = m_filterDragStartHz +
            static_cast<int>(std::round((mx - m_filterDragStartX) * hzPerPx));
        int low = m_filterLowHz;
        int high = m_filterHighHz;
        if (m_draggingFilter == FilterEdge::Low) {
            low = newHz;
        } else {
            high = newHz;
        }
        // Ensure minimum 10 Hz width
        if (std::abs(high - low) >= 10) {
            m_filterLowHz = low;
            m_filterHighHz = high;
            emit filterEdgeDragged(low, high);
        }
        update();
        return;
    }

    if (m_draggingVfo) {
        // Slide-to-tune: real-time frequency update
        // From AetherSDR SpectrumWidget.cpp:1222-1228
        double hz = xToHz(mx, specRect);
        hz = std::round(hz / m_stepHz) * m_stepHz;
        requestTune(hz);
        return;
    }

    if (m_draggingBandwidth) {
        // Zoom bandwidth by horizontal drag
        // From AetherSDR SpectrumWidget.cpp:868-876
        // Drag right = zoom in (narrower), drag left = zoom out (wider)
        int dx = m_bwDragStartX - mx;
        double factor = 1.0 + dx * 0.003;  // 0.3% per pixel
        double newBw = m_bwDragStartBw * factor;
        newBw = std::clamp(newBw, 1000.0, maxZoomOutBandwidthHz());
        // Recenter on VFO when zooming so the signal stays visible.
        // Only emit centerChanged if center actually moved — avoids DDC retune per drag frame
        const bool centreMoved = !qFuzzyCompare(m_centerHz, m_vfoHz);
        applyViewWindow(centreMoved ? m_vfoHz : m_centerHz, newBw);
        if (centreMoved) {
            emit centerChanged(m_centerHz);
        }
        updateVfoPositions();
#ifdef NEREUS_GPU_SPECTRUM
        markOverlayDirty();
#else
        update();
#endif
        return;
    }

    if (m_draggingDivider) {
        // Resize spectrum/waterfall split
        const float frac = static_cast<float>(my)
            / std::max(1, layout.contentHeight);
        m_spectrumFrac = std::clamp(frac, 0.10f, 0.90f);
#ifdef NEREUS_GPU_SPECTRUM
        markOverlayDirty();
#else
        update();
#endif
        syncDisplaySettingsFromWidget(); // Task 18
        return;
    }

    if (m_draggingPan) {
        // Pan the view — drag changes center, not VFO
        // From AetherSDR SpectrumWidget.cpp:1230-1237
        double deltaPx = mx - m_panDragStartX;
        double deltaHz = -(deltaPx / static_cast<double>(specRect.width())) * m_bandwidthHz;
        applyViewWindow(m_panDragStartCenter + deltaHz, m_bandwidthHz);
        emit centerChanged(m_centerHz);
        updateVfoPositions();
#ifdef NEREUS_GPU_SPECTRUM
        markOverlayDirty();
#else
        update();
#endif
        return;
    }

    // --- Hover cursor feedback (not dragging) ---
    // From AetherSDR SpectrumWidget.cpp:1242-1344

    const int freqBarY = layout.frequency.top();

    // Sub-epic E: hover cursors for LIVE button + time scale
    // From AetherSDR SpectrumWidget.cpp:2200-2225 [@0cd4559]
    {
        const int wfY = layout.waterfall.top();
        if (my >= freqBarY && my < wfY) {
            // In the freq-scale row — LIVE button overlaps here.
            const QRect wfRect = layout.waterfallChrome;
            if (waterfallLiveButtonRect(wfRect).contains(event->position().toPoint())) {
                setCursor(Qt::PointingHandCursor);
                return;
            }
        }
        if (my >= wfY) {
            const QRect wfRect = layout.waterfallChrome;
            const QRect timeScaleRect = waterfallTimeScaleRect(wfRect);
            if (timeScaleRect.contains(event->position().toPoint())) {
                setCursor(Qt::SizeVerCursor);
                return;
            }
            // fall through to existing crosshair assignment
        }
    }

    // 2026-05-12 bench fix (Gaps #1 + #2 + #6 from the spot-overlay
    // adversarial audit): hover-cursor + tooltip + hover-index emit for
    // spot labels and cluster badges.  Ported from AetherSDR
    // SpectrumWidget.cpp:2282-2318 [@0cd4559].  Without this:
    //   - the cursor stays as a crosshair when over a spot, hiding the
    //     fact that the label is clickable;
    //   - hovering yields no information about the spot (callsign,
    //     spotter, source, comment, age);
    //   - the Spot List tab cannot highlight the row the user is
    //     pointing at on the spectrum.
    //
    // Two hit-test loops in priority order: per-spot labels first (so
    // the tooltip wins over a cluster badge that visually overlaps a
    // label edge), then cluster badges.  Both early-return after
    // setting cursor + emitting hover signal so we don't fall through
    // to the filter-edge / passband / crosshair logic below.
    if (m_showSpots && my < specH) {
        const QPoint pos(mx, my);
        for (int i = 0; i < m_spotClickRects.size(); ++i) {
            const auto& hr = m_spotClickRects[i];
            if (!hr.rect.contains(pos)) continue;
            setCursor(Qt::PointingHandCursor);
            if (hr.markerIndex >= 0 && hr.markerIndex < m_spotMarkers.size()) {
                const auto& sm = m_spotMarkers[hr.markerIndex];
                QString tip = QString("<b>%1</b>&nbsp;&nbsp;%2 MHz")
                    .arg(sm.callsign)
                    .arg(sm.freqMhz, 0, 'f', 4);
                if (!sm.mode.isEmpty()) {
                    tip += QString("<br>Mode: %1").arg(sm.mode);
                }
                if (!sm.source.isEmpty()) {
                    tip += QString("<br>Source: %1").arg(sm.source);
                }
                if (!sm.spotterCallsign.isEmpty()) {
                    tip += QString("<br>Spotter: %1").arg(sm.spotterCallsign);
                }
                if (!sm.comment.isEmpty()) {
                    tip += QString("<br>%1").arg(sm.comment.toHtmlEscaped());
                }
                if (sm.timestampMs > 0) {
                    tip += QString("<br>Spotted: %1 UTC").arg(
                        QDateTime::fromMSecsSinceEpoch(sm.timestampMs, QTimeZone::utc())
                            .toString("yyyy-MM-dd HH:mm:ss"));
                }
                QToolTip::showText(event->globalPosition().toPoint(), tip, this);
                // Gap #6 — let the Spot List tab know which row to
                // highlight.  -1 sentinel via leaveEvent / non-spot
                // hover clears the highlight.
                if (m_hoverSpotIndex != sm.index) {
                    m_hoverSpotIndex = sm.index;
                    emit spotHoverIndexChanged(sm.index);
                }
            }
            event->accept();
            return;
        }
        // No label hit — try cluster badges.
        for (const auto& cluster : m_spotClusters) {
            if (!cluster.rect.contains(pos)) continue;
            setCursor(Qt::PointingHandCursor);
            QString tip = QString("<b>%1 spot%2 at this freq</b>")
                .arg(cluster.spots.size())
                .arg(cluster.spots.size() == 1 ? "" : "s");
            const int previewN = std::min(8, static_cast<int>(cluster.spots.size()));
            for (int k = 0; k < previewN; ++k) {
                const auto& sp = cluster.spots[k];
                tip += QString("<br>%1 %2 %3")
                    .arg(sp.callsign,
                         QString::number(sp.freqMhz, 'f', 4),
                         sp.mode);
            }
            if (cluster.spots.size() > previewN) {
                tip += QString("<br>... %1 more").arg(
                    cluster.spots.size() - previewN);
            }
            QToolTip::showText(event->globalPosition().toPoint(), tip, this);
            // Clear any per-spot hover highlight when we're hovering
            // a cluster (not a specific spot).
            if (m_hoverSpotIndex != -1) {
                m_hoverSpotIndex = -1;
                emit spotHoverIndexChanged(-1);
            }
            event->accept();
            return;
        }
        // Moved off any spot/cluster -> hide tooltip and clear hover.
        if (m_hoverSpotIndex != -1) {
            m_hoverSpotIndex = -1;
            emit spotHoverIndexChanged(-1);
            QToolTip::hideText();
        }
    }

    // Notch hover: highlight, frequency/width readout, and the selection
    // the wheel resize is gated on.  Thetis re-evaluates the selected notch
    // on every non-dragging move and clears it when the cursor is not over
    // one, which is what keeps a plain scroll tuning the VFO.
    //
    // ok are we over the top of a notch?
    //   [original comment from console.cs:49919]
    // From Thetis console.cs:49917-49926 [v2.10.3.15]
    if (my < specH) {
        const int hoverNotch = notchAtPixel(mx, specRect);
        if (hoverNotch != m_hoveredNotchId) {
            m_hoveredNotchId = hoverNotch;
            if (hoverNotch < 0) {
                QToolTip::hideText();
            }
            markOverlayDirty();
        }
        if (hoverNotch != m_selectedNotchId) {
            m_selectedNotchId = hoverNotch;
            markOverlayDirty();
        }
        if (hoverNotch >= 0) {
            const NotchMarker* n = notchMarkerById(hoverNotch);
            if (n) {
                // AetherSDR renders this in a styled QLabel popup
                // (m_tnfHoverPopup, src/gui/SpectrumWidget.cpp:13591-13646
                // [@c6481cbf]); NereusSDR routes the same frequency +
                // width readout through QToolTip, the path the spot
                // overlay above already uses.
                QToolTip::showText(event->globalPosition().toPoint(),
                    QString("<b>%1 MHz</b><br>Width: %2 Hz")
                        .arg(n->freqMhz, 0, 'f', 6)
                        .arg(qRound(n->widthHz)),
                    this);
            }
            setCursor(Qt::SizeHorCursor);
            // Unconditional, not just when the hovered id changes: the
            // cursor-frequency readout is painted into the same cached
            // static overlay (drawCursorInfo), so skipping this would
            // freeze it for as long as the pointer stayed over a marker.
            markOverlayDirty();
            event->accept();
            return;
        }
    }

    // Sub-epic E: hit-test against the actual dBm-strip width, not the
    // effectiveStripW() layout reservation (which widens to 72px when paused
    // to make room for the time-scale strip's UTC labels — but the dBm strip
    // itself stays at kDbmStripW = 36px wide).
    if (mx >= w - kDbmStripW && effectiveStripW() > 0 && my < specH) {
        // Hover over dBm strip → change cursor.
        // From AetherSDR SpectrumWidget.cpp:2241-2248 [@0cd4559]
        // Strip's arrow row is the top kDbmArrowH pixels of the strip.
        // The strip's top aligns with the widget's top (y=0 in SpectrumWidget).
        if (my < kDbmArrowH) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::SizeVerCursor);
        }
    } else if (my >= specH - 6 && my < specH + kDividerH + 6) {
        setCursor(Qt::SplitVCursor);
    } else if (my >= freqBarY && my < freqBarY + kFreqScaleH) {
        setCursor(Qt::SizeHorCursor);
    } else {
        // Check filter edges and passband
        double loHz = m_vfoHz + m_filterLowHz;
        double hiHz = m_vfoHz + m_filterHighHz;
        int xLo = hzToX(loHz, specRect);
        int xHi = hzToX(hiHz, specRect);
        if (xLo > xHi) { std::swap(xLo, xHi); }

        bool onEdge = std::abs(mx - xLo) <= kFilterGrab ||
                      std::abs(mx - xHi) <= kFilterGrab;
        bool inPassband = mx > std::min(xLo, xHi) + kFilterGrab &&
                          mx < std::max(xLo, xHi) - kFilterGrab;

        if (onEdge || inPassband) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }
    }

#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
#else
    update();
#endif
    QWidget::mouseMoveEvent(event);
}

void SpectrumWidget::applyViewWindow(double centreHz, double bandwidthHz)
{
    const bool moved   = !qFuzzyCompare(m_centerHz, centreHz);
    const bool resized = !qFuzzyCompare(m_bandwidthHz, bandwidthHz);
    if (!moved && !resized) { return; }

    // The ONLY direct writes to these two in the whole class, apart from
    // setMoxOverlay's swap. Everything else routes through here.
    m_centerHz    = centreHz;
    m_bandwidthHz = bandwidthHz;

    // A moved or resized view resets this pan's peaks, local or remote.
    // From Thetis display.cs:907-921 [v2.10.3.15] (OnCentreFrequencyChanged,
    // a new display centre):
    //   ResetBlobMaximums(1, true);
    //   ResetSpectrumPeaks(1);
    // and display.cs:1214-1245 [v2.10.3.15] (RXDisplayLow / RXDisplayHigh,
    // new display edges):
    //   if (value != rx_display_low)
    //   {
    //       ResetBlobMaximums(1, true);
    //       ResetSpectrumPeaks(1);
    resetPeaks();

    if (m_remoteSpectrum) {
        // A gesture can precede the subscription observer by one tick.
        // Retire the old plane immediately so it cannot acquire new RF labels.
        m_renderedPixels.clear();
        m_undentedPixels.clear();
        m_wfRenderedPixels.clear();
        m_pendingWfPixelsDbm.clear();
        m_pendingWfPixelsDbmDirty = false;
        m_hasNewSpectrum = true;
        // R-R3-21: the last trace, drawn at the frequency it was captured
        // at, so a drag never blanks it.
        refreshRemoteTraceProjection();
    }

    // The scale and grid are cached chrome; the trace is not. Without this
    // the numbers stay frozen while the trace moves.
    markOverlayDirty();

    // Structural, not remembered: any zoom or pan path added later inherits
    // this because it cannot change the window without coming through here.
    notifyTxViewWindow();
}

void SpectrumWidget::requestTune(double hz)
{
    // See the header. One gate, one emitter, so the next tune path added
    // inherits the safety rule instead of having to remember it.
    if (m_moxOverlay) { return; }
    emit frequencyClicked(hz);
}

void SpectrumWidget::notifyTxViewWindow()
{
    if (!showsTransmitView() || m_bandwidthHz <= 0.0) { return; }
    emit txViewWindowChanged(m_centerHz, m_bandwidthHz);
}

bool SpectrumWidget::isOnDbmStrip(const QPoint& pos) const
{
    if (effectiveStripW() <= 0) { return false; }
    const int specH = spectrumLayout().spectrum.height();
    return pos.x() >= width() - kDbmStripW && pos.y() < specH;
}

void SpectrumWidget::contextMenuEvent(QContextMenuEvent* event)
{
    // A right press on the dBm strip is a range drag (Thetis gridmaxadjust,
    // PanDisplay.cs:4424-4431 [v2.10.3.15]), so swallow the context menu
    // there. Without this the pan menu opens on top of the gesture: Qt
    // synthesises this event independently of mousePressEvent, so accepting
    // the press is not enough, and it propagates to PanadapterApplet, whose
    // contextMenuEvent builds the pan menu. Bench 2026-08-05.
    if (isOnDbmStrip(event->pos())) {
        event->accept();
        return;
    }
    // Anywhere else, let it reach the pan applet as before.
    event->ignore();
    // SpectrumBaseClass, not QRhiWidget: the CPU-only build
    // (-DNEREUS_GPU_SPECTRUM=OFF, the Rock's Core) derives from QWidget.
    SpectrumBaseClass::contextMenuEvent(event);
}

void SpectrumWidget::mouseReleaseEvent(QMouseEvent* event)
{
    // Outside the LeftButton gate below, which never sees a right release.
    // Deliberately not filtered on which button was released: the point is
    // to end the gesture, and being fussy about that is how it got stuck.
    if (m_draggingDbmRange) {
        m_draggingDbmRange = false;
        setCursor(Qt::ArrowCursor);
        emit dbmRangeChangeRequested(m_refLevel - m_dynamicRange, m_refLevel);
        scheduleSettingsSave();
        event->accept();
        return;
    }

    // Sub-epic E: time-scale drag end
    // From AetherSDR SpectrumWidget.cpp:2382-2387 [@0cd4559]
    //   note: drag release does NOT auto-resume to live — m_wfLive is only
    //   flipped true by the LIVE button. Drag-to-zero would auto-bump back
    //   on the next row otherwise. This is deliberate; see plan
    //   §authoring-time decisions discussion.
    if (m_draggingTimeScale) {
        m_draggingTimeScale = false;
        setCursor(Qt::CrossCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // If pan drag was short (click, not real drag), treat as click-to-tune
        // From AetherSDR SpectrumWidget.cpp:1427-1457 — 4px Manhattan threshold
        if (m_draggingPan) {
            int dx = std::abs(static_cast<int>(event->position().x()) - m_panDragStartX);
            // NOT while transmitting. A release that moved less than the
            // 4 px threshold is read as a click-to-tune and retunes the VFO
            // -- which, keyed into an antenna and an amplifier, is not
            // something a stray click should be able to do. The panadapter
            // was effectively read-only during transmit until this branch
            // made it pannable and zoomable, so the exposure is new even
            // though the code is not.
            //
            // Panning and zooming the view stay available while keyed; it
            // is only the retune that is withheld. Bench 2026-08-05: "the
            // tune jumps when I let go".
            //
            // requestTune now carries the same gate for every tune path, so
            // the check here is redundant for the frequencyClicked branches.
            // It stays because ddcRetuneRequested below is NOT one of them
            // and moves the slice just as surely: its own comment records
            // that MainWindow forwards it to slice frequency.
            if (dx <= 4 && !m_moxOverlay) {
                const QRect specRect = spectrumLayout().spectrum;
                double hz = xToHz(static_cast<int>(event->position().x()), specRect);
                hz = std::round(hz / m_stepHz) * m_stepHz;

                // Phase 3F Sub-Epic F Task 12: click-in-wing vs click-in-island
                // disambiguation. Only engages when extended-mode is on
                // (operator zoomed past the DDC's listenable range). When
                // off, this falls through to the existing single-path
                // frequencyClicked behavior — no regression to non-extended
                // pan tuning.
                if (m_extendedMode && m_sampleRateHz > 0.0) {
                    // The CLIPPED half-span, matching what is painted. The
                    // raw rate/2 left each 4% edge strip showing wideband
                    // survey data while a click there still retuned the
                    // slice, which is the one thing the operator can see is
                    // wrong only after the radio has moved.
                    const double halfBwHz = ddcIslandHalfSpanHz();
                    if (std::abs(hz - m_ddcCenterHz) <= halfBwHz) {
                        // Click landed inside the listenable island —
                        // standard slice retune.
                        requestTune(hz);
                    } else {
                        // Click landed in a wing — operator wants the
                        // clicked Hz to become the new DDC center.
                        // MainWindow forwards this to slice frequency,
                        // which propagates to the codec / DDC NCO retune.
                        emit ddcRetuneRequested(hz);
                    }
                } else {
                    requestTune(hz);
                }
            }
        }

        // Persist display settings after drag adjustments.
        // Also emit range-change for observers (MainWindow, tests).
        // From AetherSDR SpectrumWidget.cpp:2115 [@0cd4559]
        //
        // The Ctrl-drag range zoom joins this gate rather than getting
        // its own block. Upstream's release handler (SpectrumWidget.cpp:
        // 10893-10926 [@1872028c]) does much more here -- a
        // DbmRangeTransition/beginDbmRangeTransition smoothing system,
        // refreshNoiseFloorTarget(), a dbmRangeDragFinished signal -- none
        // of which exists in NereusSDR (see the .h field comment: no
        // NereusSDR counterpart to m_dbmDragStartRef's oldMinDbm/oldMaxDbm
        // role). The settled behaviour both gestures need is the same:
        // announce the final range and persist it, which this pre-existing
        // simple gate already does correctly for m_draggingDbm.
        if (m_draggingDbm || m_draggingDbmRangeZoom) {
            emit dbmRangeChangeRequested(m_refLevel - m_dynamicRange, m_refLevel);
        }
        if (m_draggingDbm || m_draggingDbmRangeZoom || m_draggingDivider) {
            scheduleSettingsSave();
        }

        // Hybrid zoom: replan FFT on drag-end for sharp resolution
        if (m_draggingBandwidth) {
            emit bandwidthChangeRequested(m_bandwidthHz);
        }

        m_draggingDbm = false;
        m_draggingDbmRangeZoom = false;
        m_draggingDbmRange = false;
        m_draggingFilter = FilterEdge::None;
        m_draggingVfo = false;
        m_draggingDivider = false;
        m_draggingPan = false;
        m_draggingBandwidth = false;
        // Ends the notch gesture; hover resumes writing the selection.
        if (m_notchGrab != NotchGrab::None) {
            m_notchGrab = NotchGrab::None;
            emit notchDragFinished();
        }
        setCursor(Qt::CrossCursor);
    }
    QWidget::mouseReleaseEvent(event);
}

// 2026-05-12 bench fix (Gap #8 — tooltip cleanup on mouse leave).
// Ported from AetherSDR SpectrumWidget.cpp:2556-2560 [@0cd4559].  When
// the mouse exits the widget rect entirely the cursor never crosses a
// "not over a spot" boundary inside mouseMoveEvent (the move events
// stop firing), so we explicitly hide any in-flight tooltip and clear
// the hover index here.  Without this the tooltip can linger after the
// pointer has moved off the panadapter into another widget.
void SpectrumWidget::leaveEvent(QEvent* event)
{
    m_mouseInWidget = false;
    QToolTip::hideText();
    if (m_hoverSpotIndex != -1) {
        m_hoverSpotIndex = -1;
        emit spotHoverIndexChanged(-1);
    }
    // Same reasoning for the notch overlay: the cursor never crosses a
    // "not over a notch" boundary inside mouseMoveEvent when it leaves the
    // widget outright, so the Chartreuse highlight would stay lit and the
    // wheel would keep resizing a notch the operator is no longer near.
    if (m_hoveredNotchId != -1 || m_selectedNotchId != -1) {
        m_hoveredNotchId = -1;
        m_selectedNotchId = -1;
        markOverlayDirty();
    }
    QWidget::leaveEvent(event);
}

void SpectrumWidget::wheelEvent(QWheelEvent* event)
{
    // MW0LGE before all, handle the notch size change
    //   [original inline comment from console.cs:31140]
    // From Thetis console.cs:31133-31145 [v2.10.3.15] — the wheel resizes
    // the selected notch and returns before any other wheel handling.  The
    // gate is mandatory: without a selected notch a plain scroll over the
    // panadapter tunes the VFO, so an ungated resize would steal every
    // scroll (design section 7.4).  num_steps is 1 per click upstream, not
    // a raw delta, which is what makes the step constants below Hz.
    const int notchDelta = event->angleDelta().y();
    const int notchSteps = (notchDelta == 0) ? 0 : (notchDelta > 0 ? 1 : -1);
    if (m_selectedNotchId >= 0 && notchSteps != 0) {
        const NotchMarker* n = notchMarkerById(m_selectedNotchId);
        if (n) {
            // From Thetis console.cs:33299-33321 [v2.10.3.15],
            // notchMouseWheel: Shift adds the raw detent count
            // (console.cs:33306), no modifier multiplies it by 10
            // (console.cs:33309).
            //
            // Upstream's own clamps (0..._max_filter_width at
            // console.cs:33312-33313, and the "check to see if outside
            // frequency limits" pair at console.cs:33315-33318) are NOT
            // repeated here: NotchModel::setWidth carries both, so the
            // bound has one owner.
            const double step = (event->modifiers() & Qt::ShiftModifier)
                ? NotchModel::kWheelWidthStepFineHz
                : NotchModel::kWheelWidthStepHz;
            emit notchWidthRequested(m_selectedNotchId,
                                     n->widthHz + notchSteps * step);
        }
        event->accept();
        return;
    }

    // Wheel over dBm strip: adjust dynamic range in ±5 dB steps.
    // From AetherSDR SpectrumWidget.cpp:2630-2636 [@0cd4559]
    const int mx = static_cast<int>(event->position().x());
    const int my = static_cast<int>(event->position().y());
    const int specH = spectrumLayout().spectrum.height();
    // Sub-epic E: hit-test against the actual dBm-strip width, not the
    // effectiveStripW() layout reservation (which widens to 72px when paused
    // to make room for the time-scale strip's UTC labels — but the dBm strip
    // itself stays at kDbmStripW = 36px wide).
    const int stripX = width() - kDbmStripW;
    if (mx >= stripX && effectiveStripW() > 0 && my < specH) {
        const int notches = event->angleDelta().y() / 120;
        if (notches != 0) {
            const float bottom = m_refLevel - m_dynamicRange;
            m_dynamicRange = qBound(10.0f, m_dynamicRange - notches * 5.0f, 200.0f);
            m_refLevel = bottom + m_dynamicRange;
            markOverlayDirty();   // cached scale; see the drag handler
            emit dbmRangeChangeRequested(bottom, m_refLevel);
            update();
            scheduleSettingsSave();
            syncDisplaySettingsFromWidget(); // Task 18
        }
        event->accept();
        return;
    }

    // Plain scroll: tune VFO by step size (matches Thetis panadapter behavior)
    // Ctrl+scroll: adjust ref level
    // Ctrl+Shift+scroll: zoom bandwidth
    int delta = event->angleDelta().y();
    if (delta == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    if (event->modifiers() & Qt::MetaModifier || event->modifiers() & Qt::ControlModifier) {
        // Cmd+scroll (macOS) or Ctrl+scroll: zoom bandwidth in/out
        double factor = (delta > 0) ? 0.8 : 1.25;
        double newBw = m_bandwidthHz * factor;
        newBw = std::clamp(newBw, 1000.0, maxZoomOutBandwidthHz());
        // Recenter on VFO when zooming
        applyViewWindow(m_vfoHz, newBw);
        {
            const QScopedValueRollback<bool> zooming(m_wheelZoomRecentring, true);
            emit centerChanged(m_centerHz);
        }
        emit bandwidthChangeRequested(newBw);
        updateVfoPositions();
        // This path writes m_bandwidthHz directly instead of going through
        // setFrequencyRange, so it has to re-derive extended mode itself --
        // otherwise zooming past the DDC with the wheel widens the window
        // without ever asking the radio for the wideband stream that fills
        // the wings.
        recomputeExtendedMode();
#ifdef NEREUS_GPU_SPECTRUM
        markOverlayDirty();
#endif
    } else if (event->modifiers() & Qt::ShiftModifier) {
        // Shift+scroll: adjust ref level
        float step = (delta > 0) ? 5.0f : -5.0f;
        m_refLevel = qBound(-160.0f, m_refLevel + step, 20.0f);
        scheduleSettingsSave();
        syncDisplaySettingsFromWidget(); // Task 18 -- this arm only; the
                                          // other two touch none of the
                                          // eight tracked fields
    } else {
        // Plain scroll: tune VFO by step size
        int steps = (delta > 0) ? 1 : -1;
        double newHz = m_vfoHz + steps * m_stepHz;
        newHz = std::max(newHz, 100000.0);
        requestTune(newHz);
    }

    update();
    QWidget::wheelEvent(event);
}

// ============================================================================
// GPU Rendering Path (QRhiWidget)
// Ported from AetherSDR SpectrumWidget GPU pipeline
// ============================================================================

#ifdef NEREUS_GPU_SPECTRUM

// Fullscreen quad: position (x,y) + texcoord (u,v)
// From AetherSDR SpectrumWidget.cpp:1779
static const float kQuadData[] = {
    -1, -1,  0, 1,   // bottom-left
     1, -1,  1, 1,   // bottom-right
    -1,  1,  0, 0,   // top-left
     1,  1,  1, 0,   // top-right
};

static QShader loadShader(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "SpectrumWidget: failed to load shader" << path;
        return {};
    }
    QShader s = QShader::fromSerialized(f.readAll());
    if (!s.isValid()) {
        qWarning() << "SpectrumWidget: invalid shader" << path;
    }
    return s;
}

void SpectrumWidget::initWaterfallPipeline()
{
    QRhi* r = rhi();

    m_wfVbo = r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kQuadData));
    m_wfVbo->create();

    m_wfUbo = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16);
    m_wfUbo->create();

    m_wfGpuTexW = qMax(width(), 64);
    m_wfGpuTexH = qMax(m_waterfall.height(), 64);
    m_wfGpuTex = r->newTexture(QRhiTexture::RGBA8, QSize(m_wfGpuTexW, m_wfGpuTexH));
    m_wfGpuTex->create();

    // From AetherSDR: ClampToEdge U, Repeat V (for ring buffer wrap)
    m_wfSampler = r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                 QRhiSampler::None,
                                 QRhiSampler::ClampToEdge, QRhiSampler::Repeat);
    m_wfSampler->create();

    m_wfSrb = r->newShaderResourceBindings();
    m_wfSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage, m_wfUbo),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_wfGpuTex, m_wfSampler),
    });
    m_wfSrb->create();

    QShader vs = loadShader(QStringLiteral(":/shaders/resources/shaders/waterfall.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/resources/shaders/waterfall.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) { return; }

    m_wfPipeline = r->newGraphicsPipeline();
    m_wfPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vs},
        {QRhiShaderStage::Fragment, fs},
    });

    QRhiVertexInputLayout layout;
    layout.setBindings({{4 * sizeof(float)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)},
    });
    m_wfPipeline->setVertexInputLayout(layout);
    m_wfPipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_wfPipeline->setShaderResourceBindings(m_wfSrb);
    m_wfPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_wfPipeline->create();
}

void SpectrumWidget::initOverlayPipeline()
{
    QRhi* r = rhi();

    m_ovVbo = r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kQuadData));
    m_ovVbo->create();

    int w = qMax(width(), 64);
    int h = qMax(height(), 64);
    const qreal dpr = devicePixelRatioF();
    const int pw = static_cast<int>(w * dpr);
    const int ph = static_cast<int>(h * dpr);
    m_ovGpuTex = r->newTexture(QRhiTexture::RGBA8, QSize(pw, ph));
    m_ovGpuTex->create();

    m_ovSampler = r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                 QRhiSampler::None,
                                 QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_ovSampler->create();

    m_ovSrb = r->newShaderResourceBindings();
    m_ovSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_ovGpuTex, m_ovSampler),
    });
    m_ovSrb->create();

    QShader vs = loadShader(QStringLiteral(":/shaders/resources/shaders/overlay.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/resources/shaders/overlay.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) { return; }

    m_ovPipeline = r->newGraphicsPipeline();
    m_ovPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vs},
        {QRhiShaderStage::Fragment, fs},
    });

    QRhiVertexInputLayout layout;
    layout.setBindings({{4 * sizeof(float)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)},
    });
    m_ovPipeline->setVertexInputLayout(layout);
    m_ovPipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_ovPipeline->setShaderResourceBindings(m_ovSrb);
    m_ovPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());

    // Alpha blending for overlay compositing
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    m_ovPipeline->setTargetBlends({blend});
    m_ovPipeline->create();

    m_overlayStatic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
    m_overlayStatic.setDevicePixelRatio(dpr);
    // 2026-05-26 KG4VCF: pin the overlay texture so the per-paint
    // QImage::fill + 16 paint ops + GPU upload don't touch a
    // compressed page when the system is under memory pressure.
    // This is the biggest single buffer in the render path (~1.6 MB
    // at default window size) and is hit on every overlay-rebuild
    // tick (10 Hz when blob / peak-hold / NF are enabled).
    lockMemory(m_overlayStatic.constBits(),
               m_overlayStatic.sizeInBytes(),
               "SpectrumWidget::m_overlayStatic (init)");

    // 2026-05-26 KG4VCF dual-layer split: dynamic overlay texture.
    // Same dimensions, format, sampler, pipeline as the static
    // layer -- only the texture handle + SRB binding differ.  This
    // is the layer that carries peak-hold trace / peak blobs /
    // noise-floor overlays.  Chrome stays in the static layer.
    m_ovDynGpuTex = r->newTexture(QRhiTexture::RGBA8, QSize(pw, ph));
    m_ovDynGpuTex->create();
    m_ovDynSrb = r->newShaderResourceBindings();
    m_ovDynSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1,
            QRhiShaderResourceBinding::FragmentStage,
            m_ovDynGpuTex, m_ovSampler),
    });
    m_ovDynSrb->create();
    m_overlayDynamic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
    m_overlayDynamic.setDevicePixelRatio(dpr);
    // QImage's buffer is uninitialized, and the dynamic quad is composited
    // across the WHOLE widget while the rebuild below only clears and
    // uploads the spectrum-height band.  The waterfall region therefore
    // sampled whatever the allocator handed us and could show opaque
    // garbage, most visibly right after init or a resize.  Codex review,
    // PR #291.
    m_overlayDynamic.fill(Qt::transparent);
    lockMemory(m_overlayDynamic.constBits(),
               m_overlayDynamic.sizeInBytes(),
               "SpectrumWidget::m_overlayDynamic (init)");

    // 3DSS surface layer — parallel texture + SRB so the overlay pipeline can
    // paint the cached 3D image as a full-screen quad in 3D mode. The image is
    // built/uploaded on demand in renderGpuFrame().
    // 3DSS CPU-fallback quad (Task 10). Parallel texture + SRB so the same
    // overlay pipeline can paint the cached CPU 3D surface as a quad in the
    // spectrum viewport when the GPU mesh path is unavailable (RGBA16F
    // unsupported). Initially sized to the full window like m_ovGpuTex/
    // m_ovDynGpuTex above; uploadDssFallbackImage() resizes it down to the
    // actual (capped) surface size on first use. The image itself is built/
    // uploaded on demand there, called from renderGpuFrame().
    // From AetherSDR SpectrumWidget.cpp:12619-12630 [@1872028c] (upstream's
    // own m_dssGpuTex/m_dssSrb init, same shape, different field names --
    // see the .h declaration for why the names differ).
    m_dssFallbackTex = r->newTexture(QRhiTexture::RGBA8, QSize(pw, ph));
    m_dssFallbackTex->create();
    m_dssFallbackTexW = pw;
    m_dssFallbackTexH = ph;
    m_dssFallbackSrb = r->newShaderResourceBindings();
    m_dssFallbackSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1,
            QRhiShaderResourceBinding::FragmentStage,
            m_dssFallbackTex, m_ovSampler),
    });
    m_dssFallbackSrb->create();
    m_dssFallbackUploadedGen = ~0ull;
}

void SpectrumWidget::initSpectrumPipeline()
{
    QRhi* r = rhi();

    m_fftLineVbo = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                 kMaxFftBins * kFftVertStride * sizeof(float));
    m_fftLineVbo->create();

    m_fftFillVbo = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                 kMaxFftBins * 2 * kFftVertStride * sizeof(float));
    m_fftFillVbo->create();

    // Phase 3G-8 commit 10: peak hold VBO (same layout as line VBO).
    m_fftPeakVbo = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                 kMaxFftBins * kFftVertStride * sizeof(float));
    m_fftPeakVbo->create();

    m_fftSrb = r->newShaderResourceBindings();
    m_fftSrb->setBindings({});
    m_fftSrb->create();

    QShader vs = loadShader(QStringLiteral(":/shaders/resources/shaders/spectrum.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/resources/shaders/spectrum.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) { return; }

    QRhiVertexInputLayout layout;
    layout.setBindings({{kFftVertStride * sizeof(float)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float4, 2 * sizeof(float)},
    });

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

    // Fill pipeline (triangle strip)
    m_fftFillPipeline = r->newGraphicsPipeline();
    m_fftFillPipeline->setShaderStages({{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
    m_fftFillPipeline->setVertexInputLayout(layout);
    m_fftFillPipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_fftFillPipeline->setShaderResourceBindings(m_fftSrb);
    m_fftFillPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_fftFillPipeline->setTargetBlends({blend});
    m_fftFillPipeline->create();

    // Line pipeline (line strip)
    m_fftLinePipeline = r->newGraphicsPipeline();
    m_fftLinePipeline->setShaderStages({{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
    m_fftLinePipeline->setVertexInputLayout(layout);
    m_fftLinePipeline->setTopology(QRhiGraphicsPipeline::LineStrip);
    m_fftLinePipeline->setShaderResourceBindings(m_fftSrb);
    m_fftLinePipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_fftLinePipeline->setTargetBlends({blend});
    m_fftLinePipeline->create();
}

// ---- 3DSS mesh GPU resources (3D stacked-trace spectrum plan, Task 7) ----
// Pipeline/SRB/texture structure follows AetherSDR SpectrumWidget.cpp's
// initDssMeshPipeline() [@1872028c]; see per-function comments below for the
// exact cited ranges. The one behavioural divergence: the vertex buffers are
// sized from the live perspective shape (dssMeshColsFor(dssShape())) rather
// than a compile-time constant, and are reallocated only when that column
// count actually changes -- see rebuildDssMeshIfNeeded().

bool SpectrumWidget::initDssMeshPipeline()
{
    QRhi* r = rhi();
    m_dssMeshReady = false;
    if (!r) { return false; }

    // Linux takes Qt's default QRhiWidget backend (this file's setApi() only
    // covers Q_OS_MAC/Q_OS_WIN), typically OpenGL, whose driver renders the
    // ribbon outline flat/stale from a separate, identically configured
    // pipeline -- see DssMeshGeometry.h.
    m_dssOutlinePipelineMode = dssOutlinePipelineModeForBackend(
        r->backend() == QRhi::OpenGLES2);

    // R stores dBm and G stores captured-frequency coverage. The second
    // channel keeps zoom-created floor spans colour-stable without hiding
    // their lines. From AetherSDR SpectrumWidget.cpp:12793-12798 [@1872028c].
    if (!r->isTextureFormatSupported(QRhiTexture::RGBA16F, {})) {
        qCWarning(lcSpectrum) << "SpectrumWidget: RGBA16F unsupported — "
                                  "stacked-trace mesh disabled (CPU fallback)";
        return false;
    }

    QShader vs = loadShader(QStringLiteral(
        ":/shaders/resources/shaders/dss_mesh.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(
        ":/shaders/resources/shaders/dss_mesh.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh shader load "
                                  "failed — stacked-trace mesh disabled";
        return false;
    }

    m_dssMeshCols = dssMeshColsFor(dssShape());
    const int fillVerts = kDssVisibleRows * dssFillVerticesPerRow(m_dssMeshCols);
    const int lineVerts = kDssVisibleRows * dssLineVerticesPerRow(m_dssMeshCols);

    m_dssMeshVbo = r->newBuffer(QRhiBuffer::Immutable,
                                QRhiBuffer::VertexBuffer,
                                fillVerts * 3 * sizeof(float));
    m_dssMeshLineVbo = r->newBuffer(QRhiBuffer::Immutable,
                                    QRhiBuffer::VertexBuffer,
                                    lineVerts * 3 * sizeof(float));
    m_dssUbo = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                            kDssMeshUboFloats * sizeof(float));
    if (!m_dssMeshVbo->create() || !m_dssMeshLineVbo->create()
        || !m_dssUbo->create()) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh buffer create failed";
        return false;
    }

    m_dssHeightTex = r->newTexture(QRhiTexture::RGBA16F,
                                   QSize(m_dss.cols(), m_dss.rows()));
    m_dssPaletteTex = r->newTexture(QRhiTexture::RGBA8, QSize(256, 1));
    if (!m_dssHeightTex->create() || !m_dssPaletteTex->create()) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh texture create failed";
        return false;
    }

    // Height sampled in the vertex stage; Nearest is enough because the mesh
    // grid is never sparser than the texture -- the column count is sized so
    // that even at the widest rowSpanFactor the on-screen columns still cover
    // every texel, so no bin can fall between two samples. Palette is Linear
    // for a smooth floor->peak gradient.
    m_dssHeightSampler = r->newSampler(
        QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_dssPaletteSampler = r->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!m_dssHeightSampler->create() || !m_dssPaletteSampler->create()) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh sampler create failed";
        return false;
    }

    // Binding numbers 0 (UBO, both stages), 1 (height, vertex stage), 2
    // (palette, fragment stage) are load-bearing: dss_mesh.vert:63 and
    // dss_mesh.frag:47/50 declare these exact bindings.
    m_dssSrb = r->newShaderResourceBindings();
    m_dssSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0,
            QRhiShaderResourceBinding::VertexStage
                | QRhiShaderResourceBinding::FragmentStage, m_dssUbo),
        QRhiShaderResourceBinding::sampledTexture(1,
            QRhiShaderResourceBinding::VertexStage,
            m_dssHeightTex, m_dssHeightSampler),
        QRhiShaderResourceBinding::sampledTexture(2,
            QRhiShaderResourceBinding::FragmentStage,
            m_dssPaletteTex, m_dssPaletteSampler),
    });
    if (!m_dssSrb->create()) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh SRB create failed";
        return false;
    }

    QRhiVertexInputLayout layout;
    layout.setBindings({{3 * sizeof(float)}});
    layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float3, 0}});

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable   = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

    const auto makePipeline = [&]() -> QRhiGraphicsPipeline* {
        QRhiGraphicsPipeline* p = r->newGraphicsPipeline();
        p->setShaderStages({{QRhiShaderStage::Vertex, vs},
                            {QRhiShaderStage::Fragment, fs}});
        p->setVertexInputLayout(layout);
        p->setTopology(QRhiGraphicsPipeline::Triangles);
        p->setShaderResourceBindings(m_dssSrb);
        p->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        p->setTargetBlends({blend});
        return p;
    };
    m_dssFillPipeline = makePipeline();
    // QRhi's OpenGLES2 backend shares the fill pipeline for the outline draw
    // (dssOutlinePipelineFor, DssMeshGeometry.h) instead of a second,
    // identically configured one -- skip allocating it there, not just
    // skip drawing with it.
    if (m_dssOutlinePipelineMode
        == DssOutlinePipelineMode::DedicatedRibbonPipeline) {
        m_dssLinePipeline = makePipeline();
    }
    if (!m_dssFillPipeline->create()
        || (m_dssLinePipeline && !m_dssLinePipeline->create())) {
        qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh pipeline create failed";
        return false;
    }

    m_dssMeshReady = true;
    return true;
}

// Rebuilds the static mesh VBOs only when the column count derived from the
// live perspective shape actually changed (angle slider moved enough to
// cross a column boundary), never unconditionally. Sizing every panadapter
// for the widest angle unconditionally would cost 57.8 MiB instead of 33.8
// MiB at the default angle, and reallocating every frame would stall the
// render thread. NereusSDR-original: upstream sizes its VBOs once at a
// fixed viewing angle and never rebuilds them (design doc section 5.5).
void SpectrumWidget::rebuildDssMeshIfNeeded(QRhiResourceUpdateBatch* batch)
{
    if (!m_dssMeshReady || !batch) { return; }
    const int wanted = dssMeshColsFor(dssShape());
    // Skip only when the geometry currently ON THE GPU is already right.
    //
    // The condition is deliberately "have we uploaded, and is the column
    // count still the one we uploaded for", NOT a dirty flag plus a size
    // check. initDssMeshPipeline() sets m_dssMeshCols and creates a
    // correctly sized but EMPTY buffer, so a guard keyed on
    // "wanted == m_dssMeshCols && vbo->size() > 0" is satisfied on the very
    // first call and skips the only code path that ever uploads vertices.
    // The mesh would then draw undefined GPU memory forever.
    //
    // Keying on m_dssMeshUploaded also fixes the converse waste: the vertex
    // data is a pure function of the column count, so an angle change that
    // does not cross a dssMeshColsFor boundary needs no work at all. Without
    // this, a slider drag re-uploads tens of MiB of identical data per tick.
    if (m_dssMeshUploaded && wanted == m_dssMeshCols) {
        return;
    }

    QVector<float> fill;
    QVector<float> line;
    dssBuildMeshVertices(wanted, fill, line);
    const quint32 fillBytes = quint32(fill.size()) * sizeof(float);
    const quint32 lineBytes = quint32(line.size()) * sizeof(float);

    if (wanted != m_dssMeshCols) {
        m_dssMeshVbo->destroy();
        m_dssMeshVbo->setSize(fillBytes);
        m_dssMeshLineVbo->destroy();
        m_dssMeshLineVbo->setSize(lineBytes);
        if (!m_dssMeshVbo->create() || !m_dssMeshLineVbo->create()) {
            qCWarning(lcSpectrum) << "SpectrumWidget: dss_mesh resize failed";
            m_dssMeshReady = false;
            return;
        }
        m_dssMeshCols = wanted;
    }
    batch->uploadStaticBuffer(m_dssMeshVbo, 0, fillBytes, fill.constData());
    batch->uploadStaticBuffer(m_dssMeshLineVbo, 0, lineBytes, line.constData());
    m_dssMeshUploaded = true;
}

// Uploads the ring texture. Only the newest row changes per frame under
// normal scrolling, so upload one row unless the ring generation jumped by
// more than one push (mode just entered 3D, or several rows arrived between
// paints) -- in which case the whole texture is re-uploaded to avoid
// re-deriving which rows are stale from the generation delta alone.
QVector<int> SpectrumWidget::dssHeightRowsToUpload() const
{
    if (m_dss.rowCount() == 0
        || m_dss.rowGeneration() == m_dssUploadedRowGeneration) {
        return {};
    }
    if (m_dssLastUploadedHead < 0 || m_dss.rowCount() < kDssRows
        || m_dss.rowGeneration() - m_dssUploadedRowGeneration != 1) {
        QVector<int> rows;
        rows.reserve(m_dss.rows());
        for (int ring = 0; ring < m_dss.rows(); ++ring) {
            rows.append(ring);
        }
        return rows;
    }
    return {m_dss.headRing()};
}

void SpectrumWidget::uploadDssHeightRows(QRhiResourceUpdateBatch* batch)
{
    if (!m_dssMeshReady || !batch || m_dss.rowCount() == 0) { return; }
    const QVector<int> rowsToUpload = dssHeightRowsToUpload();
    if (rowsToUpload.isEmpty()) { return; }

    const int cols = m_dss.cols();
    const auto packRow = [&](int ring, QVector<qfloat16>& out) {
        const float*  exact    = m_dss.rowDataRing(ring);
        const quint8* exactCov = m_dss.rowCoverageRing(ring);
        const float*  wide     = m_dss.rowWideDataRing(ring);
        const quint8* wideCov  = m_dss.rowWideCoverageRing(ring);
        out.resize(cols * 4);
        for (int c = 0; c < cols; ++c) {
            out[c * 4 + 0] = qfloat16(exact[c]);
            out[c * 4 + 1] = qfloat16(exactCov[c] ? 1.0f : 0.0f);
            out[c * 4 + 2] = qfloat16(wide[c]);
            out[c * 4 + 3] = qfloat16(wideCov[c] ? 1.0f : 0.0f);
        }
    };

    QVector<qfloat16> packed;
    const int head = m_dss.headRing();
    for (int ring : rowsToUpload) {
        packRow(ring, packed);
        QRhiTextureSubresourceUploadDescription desc(
            packed.constData(), packed.size() * sizeof(qfloat16));
        desc.setSourceSize(QSize(cols, 1));
        desc.setDestinationTopLeft(QPoint(0, ring));
        batch->uploadTexture(m_dssHeightTex,
                             QRhiTextureUploadEntry(0, 0, desc));
    }
    m_dssLastUploadedHead = head;
    m_dssUploadedRowGeneration = m_dss.rowGeneration();
}

// From AetherSDR SpectrumWidget.cpp:12713-12741 [@1872028c], adapted to this
// file's simpler single-argument signature (Task 7's stub takes no
// floorDbm/rangeDb -- dssStrengthToRgb() operates on an already-normalised
// 0..1 strength, so this upload path never needs the per-frame dBm floor or
// range at all).
void SpectrumWidget::uploadDssPaletteLut(QRhiResourceUpdateBatch* batch)
{
    if (!m_dssPaletteTex || !batch) { return; }
    // The 3D surface maps its stable colour aperture across the FULL colormap,
    // independently of the Ref-level height span. This bypasses dbmToRgb()'s
    // waterfall black-level window while preventing a high Ref level from
    // compressing every real signal into blue. "3D Gain" gamma-shapes the LUT.
    // Colour depends only on the scheme + that control (NOT the per-frame floor/
    // range, which jitter every frame), so the LUT re-bakes only on a real change.
    const quint64 token = dssPaletteToken();
    if (token == m_dssLutToken) { return; }   // unchanged

    QImage lut(256, 1, QImage::Format_RGBA8888);   // owns its data
    for (int i = 0; i < 256; ++i) {
        const QRgb c = dssStrengthToRgb(i / 255.0f);
        lut.setPixelColor(i, 0, QColor(qRed(c), qGreen(c), qBlue(c)));
    }
    QRhiTextureSubresourceUploadDescription desc(lut);
    batch->uploadTexture(m_dssPaletteTex, QRhiTextureUploadEntry(0, 0, desc));
    m_dssLutToken = token;
}

// Writes the std140 uniform block dss_mesh.vert declares at :14-58. Field
// order matches the shader exactly: twenty-two leading float scalars, two
// explicit std140 padding floats to reach the vec4 boundary the scalar run
// rounds up to, then bgFill / shadowBands[8] / shadowStyles[8] / shadowMeta
// / rowFrames[kDssRows]. A single float out of order here shifts every
// vec4 after it and the surface renders garbage with no compile error --
// see tst_dss_shader_contract.cpp for the count half of this contract
// (parses the real shader; cannot catch a wrong order, only a wrong total).
//
// Restructured from AetherSDR SpectrumWidget.cpp:14148-14408 [@1872028c],
// which assembles the equivalent block inline inside renderGpuFrame()
// rather than as a separate function. Two upstream sections are NOT
// ported here: the row-span-factor ease-toward-target animation
// (:14164-14183, m_dssRowSpanFactor += kRowSpanAlpha * (target -
// m_dssRowSpanFactor) -- this task writes dssRowSpanTarget() straight
// through; an eased approach is a possible follow-up, not a correctness
// requirement) and the slice-shadow descriptor computation (:14216-14366,
// writeShadowSlot/appendShadow against m_sliceOverlays), left zeroed here
// per the task brief and filled in by Task 12. The rowFrames loop
// (:14373-14401) is the closest thing to a verbatim carry-over: same
// four-component-per-row shape, ages 0..kDssRows-1, oldest first.
// Upstream pre-subtracts each row's own target-relative delta at write
// time (rowCenterMhz - dssTargetCenterMhz) and always writes 0 for
// targetCenterOffsetMhz; this port writes both sides absolute (the row's
// own captured centre, and the CURRENT centre) and lets the shader's
// "(targetCenterOffsetMhz - frame.x)" subtraction do the same work --
// algebraically identical (target - row) either way, verified by hand
// against dss_mesh.vert:120-122 before porting this way, and cheaper: one
// subtraction per fragment instead of kDssRows subtractions per frame on
// the CPU whether or not that row is ever sampled.
void SpectrumWidget::writeDssMeshUbo(QRhiResourceUpdateBatch* batch,
                                     const QRect& specRect, float dpr)
{
    if (!m_dssUbo || !batch) { return; }
    const double targetBwMhz = m_bandwidthHz / 1.0e6;
    const double targetCenterMhz = m_centerHz / 1.0e6;
    const DssShape shape = dssShape();

    std::array<float, kDssMeshUboFloats> ubo{};
    int i = 0;
            // rowOffset carries a half-texel so the shader (texY = fract(rowOffset
            // + v), v = rr/rows) samples row (head+rr) CENTRES, not boundaries —
            // avoids Nearest off-by-one. Column centring uses texCols in the shader.
    // rowOffset: ring scroll plus a half texel so Nearest lands on centres.
    ubo[i++] = (m_dss.headRing() + 0.5f) / static_cast<float>(m_dss.rows());
    ubo[i++] = dssFloorDbm();
    // dssRoundedSpanDb() (Task 11 fast-follow): this field originally wrote
    // the raw dssSpanDb() here while drawDbmScale3D() and buildDssImage()
    // both rounded to the nearest 0.5 dB, so this GPU mesh's rangeDb
    // (dss_mesh.vert's front-ridge height mapping) could disagree with the
    // drawn scale and the CPU fallback by up to 0.25 dB whenever
    // m_dynamicRange was not already an exact 0.5 dB multiple. All three
    // now read the identical named, rounded value.
    ubo[i++] = dssRoundedSpanDb();
    ubo[i++] = 0.6f;                                  // zCurve: lift the floor band
    ubo[i++] = shape.backWidthFrac;
    ubo[i++] = shape.depthSpanFrac;
    ubo[i++] = shape.frontMaxRidgeFrac;
    ubo[i++] = kDssHaze;
    ubo[i++] = static_cast<float>(m_dss.cols());
    ubo[i++] = static_cast<float>(targetBwMhz);
    ubo[i++] = static_cast<float>(targetCenterMhz);
    ubo[i++] = 1.0f;                                  // rowFrequencyFrames on
    ubo[i++] = m_dssScrollProgressRows;
    ubo[i++] = static_cast<float>(m_dss.rows());
    //-KG4VCF [v0.5.3] Always one: our producer appends a single row per
    // waterfall tick. Permitted tier 1 deviation 2 of 2, design doc 2.3.
    ubo[i++] = 1.0f;                                  // scrollDistanceRows
    // colorRangeDb: dssSpanDb() capped at kDssColorSpanDb, never the
    // constant alone -- see dssColorRangeDb()'s comment (review-round fix;
    // AetherSDR SpectrumWidget.cpp:14195 [@1872028c]).
    ubo[i++] = dssColorRangeDb();
    ubo[i++] = static_cast<float>(m_dss.rowCount());
    ubo[i++] = static_cast<float>(kDssVisibleRows);
    ubo[i++] = specRect.width()  * dpr;
    ubo[i++] = specRect.height() * dpr;
    ubo[i++] = dssRowSpanTarget(targetBwMhz);
    ubo[i++] = static_cast<float>(m_dssMeshCols);
                // std140 rounds the 22-float scalar run up to bgFill's vec4
                // alignment. These two are the hole, not fields.
    ubo[i++] = 0.0f;                                  // std140 pad
    ubo[i++] = 0.0f;                                  // std140 pad

    const QColor bg(0x0a, 0x0a, 0x14);
    ubo[i++] = bg.redF();
    ubo[i++] = bg.greenF();
    ubo[i++] = bg.blueF();
    ubo[i++] = 1.0f;                                         // bgFill vec4

    // shadowBands / shadowStyles: buildDssShadowBands() already returns
    // empty when 3D Slice Shadow is off, so both loops below fall straight
    // through to the zero-padding branch and the shader's `shadowMeta.y <
    // 0.5` early-out (redundantly, but harmlessly) never even needs it.
    const QVector<DssShadowBand> shadowBands = buildDssShadowBands();
    for (int slot = 0; slot < kDssShadowSlices; ++slot) {
        if (slot < shadowBands.size()) {
            const DssShadowBand& band = shadowBands[slot];
            ubo[i++] = band.lowUnit;
            ubo[i++] = band.highUnit;
            ubo[i++] = band.centreUnit;
            ubo[i++] = band.alpha;
        } else {
            i += 4;   // std::array is zero-initialized; no descriptor here
        }
    }
    for (int slot = 0; slot < kDssShadowSlices; ++slot) {
        if (slot < shadowBands.size()) {
            const DssShadowBand& band = shadowBands[slot];
            ubo[i++] = static_cast<float>(band.cue.redF());
            ubo[i++] = static_cast<float>(band.cue.greenF());
            ubo[i++] = static_cast<float>(band.cue.blueF());
            ubo[i++] = band.centreAlpha;
        } else {
            i += 4;
        }
    }
    ubo[i++] = static_cast<float>(shadowBands.size());   // descriptor count
    ubo[i++] = m_threeDSliceDepth ? 1.0f : 0.0f;
    ubo[i++] = specRect.width() * dpr;
    ubo[i++] = 0.0f;

    // rowFrames: per-row capture frame so older rows remap correctly while
    // the operator zooms or tunes with history on screen.
    for (int age = 0; age < kDssRows; ++age) {
        ubo[i++] = static_cast<float>(m_dss.rowCenterMhzAtAge(age));
        ubo[i++] = static_cast<float>(m_dss.rowBandwidthMhzAtAge(age));
        ubo[i++] = static_cast<float>(m_dss.rowWideCenterMhzAtAge(age));
        ubo[i++] = static_cast<float>(m_dss.rowWideBandwidthMhzAtAge(age));
    }
    Q_ASSERT(i == kDssMeshUboFloats);
    batch->updateDynamicBuffer(m_dssUbo, 0,
                               kDssMeshUboFloats * sizeof(float), ubo.data());
}

// 3DSS CPU-fallback quad (Task 10). Builds the capped-resolution CPU surface
// via buildDssImage() and uploads it into m_dssFallbackTex, resizing the
// texture/SRB first if the target size changed. Called from renderGpuFrame()
// only when the mesh pipeline never came up (m_dssMeshReady false); the draw
// call that actually composites the result lives in renderGpuFrame()'s
// spectrum-region branch, reusing m_ovPipeline + m_ovVbo with this SRB.
//
// From AetherSDR SpectrumWidget.cpp:14409-14447 [@1872028c]. Two exclusions:
// dpr is a parameter here rather than recomputed from
// renderTarget()->pixelSize() locally -- the caller already has it (see
// writeDssMeshUbo()'s own dpr parameter, same existing convention); and the
// perfEnabled PerfTelemetry::recordGpuUpload() call is dropped, no
// NereusSDR PerfMonitor equivalent exists for this specific texture-upload
// event and adding one is out of this task's scope.
            // CPU cached-image fallback (no mesh pipeline). Rendered at a capped
            // resolution and stretched to the specRect viewport.
void SpectrumWidget::uploadDssFallbackImage(QRhiResourceUpdateBatch* batch,
                                            const QRect& specRect, float dpr)
{
    if (!batch) { return; }

    // Cap the software surface (like the CPU-only paint path) so a HiDPI/
    // maximized window doesn't rebuild a multi-megapixel QImage every frame;
    // the surface is intrinsically low-res, so stretch it on draw.
    const int specPwDev = qMax(1, qRound(specRect.width()  * dpr));
    const int specPhDev = qMax(1, qRound(specRect.height() * dpr));
    const double sc = qMin(1.0, qMin(double(kDssFallbackMaxW) / specPwDev,
                                     double(kDssFallbackMaxH) / specPhDev));
    const int dssW = qMax(2, static_cast<int>(specPwDev * sc));
    const int dssH = qMax(2, static_cast<int>(specPhDev * sc));
    const QImage& surf = buildDssImage(QSize(dssW, dssH), 0);

    // m_dssFallbackTex/m_dssFallbackSrb/m_ovSampler come from
    // initOverlayPipeline(); guard against a partial GPU init (OOM / device
    // loss) so the fallback never dereferences a null resource.
    if (surf.isNull() || !m_dssFallbackTex || !m_dssFallbackSrb || !m_ovSampler) {
        return;
    }
    if (m_dssFallbackTexW != dssW || m_dssFallbackTexH != dssH) {
        m_dssFallbackTexW = dssW;
        m_dssFallbackTexH = dssH;
        m_dssFallbackTex->setPixelSize(QSize(dssW, dssH));
        m_dssFallbackTex->create();
        m_dssFallbackSrb->setBindings({
            QRhiShaderResourceBinding::sampledTexture(1,
                QRhiShaderResourceBinding::FragmentStage,
                m_dssFallbackTex, m_ovSampler),
        });
        m_dssFallbackSrb->create();
        m_dssFallbackUploadedGen = ~0ull;   // force the upload below
    }
    if (m_dssFallbackUploadedGen != m_dss.generation()) {
        QRhiTextureSubresourceUploadDescription desc(surf);
        batch->uploadTexture(m_dssFallbackTex, QRhiTextureUploadEntry(0, 0, desc));
        m_dssFallbackUploadedGen = m_dss.generation();
    }
}

void SpectrumWidget::initialize(QRhiCommandBuffer* cb)
{
    if (m_rhiInitialized) { return; }

    QRhi* r = rhi();
    if (!r) {
        qWarning() << "SpectrumWidget: QRhi init failed — no GPU backend";
        return;
    }
    qDebug() << "SpectrumWidget: QRhi backend:" << r->backendName();

    auto* batch = r->nextResourceUpdateBatch();

    initWaterfallPipeline();
    initOverlayPipeline();
    initSpectrumPipeline();
    initDssMeshPipeline();

    // Upload quad VBO data
    batch->uploadStaticBuffer(m_wfVbo, kQuadData);
    batch->uploadStaticBuffer(m_ovVbo, kQuadData);

    // Initial full waterfall texture upload
    if (!m_waterfall.isNull()) {
        QImage rgba = m_waterfall.convertToFormat(QImage::Format_RGBA8888);
        QRhiTextureSubresourceUploadDescription desc(rgba);
        batch->uploadTexture(m_wfGpuTex, QRhiTextureUploadEntry(0, 0, desc));
    }

    cb->resourceUpdate(batch);
    m_wfTexFullUpload = false;
    m_wfLastUploadedRow = m_wfWriteRow;
    m_rhiInitialized = true;
}

void SpectrumWidget::renderGpuFrame(QRhiCommandBuffer* cb)
{
    // 2026-05-26 KG4VCF perf instrumentation: time the whole GPU
    // render path (texture uploads + draw-call submission) so the
    // in-spectrum overlay can show avg/max paint cost and the gap
    // between consecutive paints (proxy for main-thread starvation).
    // ns/1e6 -> ms; perf snapshot averages over the last ~1 s.
    QElapsedTimer paintTimer;
    paintTimer.start();
    {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (m_lastPaintWallMs > 0) {
            PerfMonitor::instance().recordInterFrameGap(
                static_cast<double>(nowMs - m_lastPaintWallMs));
        }
        m_lastPaintWallMs = nowMs;
    }

    QRhi* r = rhi();
    const int w = width();
    const int h = height();
    if (w <= 0 || h <= kFreqScaleH + kDividerH + 2) { return; }

    const SpectrumLayout layout = spectrumLayout();
    const int specH = layout.spectrum.height();
    // Strip lives on the right edge (overlay texture paints it there).
    // Clip GPU content to w - effectiveStripW() so the trace stops at the
    // strip's border instead of being drawn under it (or fills full width
    // when the strip is hidden).
    const QRect specRect = layout.spectrum;
    const QRect wfRect = layout.waterfall;

    auto* batch = r->nextResourceUpdateBatch();

    // ---- Waterfall texture upload (incremental) ----
    if (!m_waterfall.isNull()) {
        if (m_waterfall.width() != m_wfGpuTexW || m_waterfall.height() != m_wfGpuTexH) {
            m_wfGpuTexW = m_waterfall.width();
            m_wfGpuTexH = m_waterfall.height();
            m_wfGpuTex->setPixelSize(QSize(m_wfGpuTexW, m_wfGpuTexH));
            m_wfGpuTex->create();
            m_wfSrb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage, m_wfUbo),
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_wfGpuTex, m_wfSampler),
            });
            m_wfSrb->create();
            m_wfTexFullUpload = true;
        }

        if (m_wfTexFullUpload) {
            QImage rgba = m_waterfall.convertToFormat(QImage::Format_RGBA8888);
            batch->uploadTexture(m_wfGpuTex, QRhiTextureUploadEntry(0, 0,
                QRhiTextureSubresourceUploadDescription(rgba)));
            m_wfLastUploadedRow = m_wfWriteRow;
            m_wfTexFullUpload = false;
        } else if (m_wfWriteRow != m_wfLastUploadedRow) {
            // Incremental: upload only dirty rows
            const int texH = m_wfGpuTexH;
            int row = m_wfLastUploadedRow;
            QVector<QRhiTextureUploadEntry> entries;
            int maxRows = texH;
            while (row != m_wfWriteRow && maxRows-- > 0) {
                row = (row - 1 + texH) % texH;
                const uchar* srcLine = m_waterfall.constScanLine(row);
                QImage rowImg(srcLine, m_wfGpuTexW, 1, m_waterfall.bytesPerLine(),
                              QImage::Format_RGB32);
                QImage rowRgba = rowImg.convertToFormat(QImage::Format_RGBA8888);
                QRhiTextureSubresourceUploadDescription desc(rowRgba);
                desc.setDestinationTopLeft(QPoint(0, row));
                entries.append(QRhiTextureUploadEntry(0, 0, desc));
            }
            if (!entries.isEmpty()) {
                QRhiTextureUploadDescription uploadDesc;
                uploadDesc.setEntries(entries.begin(), entries.end());
                batch->uploadTexture(m_wfGpuTex, uploadDesc);
            }
            m_wfLastUploadedRow = m_wfWriteRow;
        }
    }

    // ---- Waterfall UBO (ring buffer offset) ----
    //
    // 2026-05-25 KG4VCF Option B: GPU sub-row scroll interpolation.
    //
    // The waterfall is a ring-buffer texture: each row push decrements
    // m_wfWriteRow and writes new dBm data at that index.  Sampling the
    // texture at v_uv.y = 0 with rowOffset = m_wfWriteRow/H places the
    // newest row at the top of the viewport.
    //
    // Before Option B, rowOffset jumped by 1/H on every push.  Even with
    // PreciseTimer + WaterfallTicker on its own thread, small drift
    // between the push cadence and the display cadence let the eye see a
    // ~1 Hz hitch in the scroll.
    //
    // Option B turns rowOffset into a CONTINUOUS function of time by
    // having the displayed top row LAG the most recent push by exactly
    // one push period.  At each display frame between push N and push
    // N+1, we blend texel m_wfWriteRow (newest, just pushed at N) with
    // texel (m_wfWriteRow + 1) mod H (the previously-newest, pushed at
    // N-1).  Both texels hold valid data, so the bilinear sampler
    // produces a clean gradient with no garbage-row artifacts.  At the
    // push moment, frac saturates to 1.0 and effectiveRow reaches
    // m_wfWriteRow exactly; immediately after the push m_wfWriteRow has
    // decremented by one and frac has reset to 0, so the formula
    // effectiveRow = m_wfWriteRow + (1 - frac) holds continuous across
    // the boundary (no visible step on push).
    //
    // The visual lag is one push period (~33 ms at the default 30 Hz
    // cadence) -- imperceptible to the operator and the price for not
    // having to invent data that doesn't exist yet.
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const float pushPeriodMs = static_cast<float>(qMax(1, m_wfUpdatePeriodMs));
    const float elapsedMs = static_cast<float>(qMax<qint64>(0, nowMs - m_wfLastPushMs));
    const float pushFrac = qBound(0.0f, elapsedMs / pushPeriodMs, 1.0f);
    const float effectiveRow = static_cast<float>(m_wfWriteRow) + (1.0f - pushFrac);
    float rowOffset = (m_wfGpuTexH > 0)
        ? effectiveRow / static_cast<float>(m_wfGpuTexH) : 0.0f;
    // Written further down, from the frame plan, only when the waterfall
    // draws (GUI-M1).
    const float wfUniforms[] = {rowOffset, 0.0f, 0.0f, 0.0f};

    // ---- Overlay texture (static, only on state change) ----
    {
        const qreal dpr = devicePixelRatioF();
        const int pw = static_cast<int>(w * dpr);
        const int ph = static_cast<int>(h * dpr);
        if (m_overlayStatic.size() != QSize(pw, ph)) {
            // 2026-05-26 KG4VCF: unlock the previous overlay before
            // replacement frees it.  Aligned no-op when null.
            if (!m_overlayStatic.isNull()) {
                unlockMemory(m_overlayStatic.constBits(),
                             m_overlayStatic.sizeInBytes());
            }
            m_overlayStatic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
            m_overlayStatic.setDevicePixelRatio(dpr);
            // Pin the resized overlay (resize trigger fires on
            // window-size change; tag for log clarity).
            lockMemory(m_overlayStatic.constBits(),
                       m_overlayStatic.sizeInBytes(),
                       "SpectrumWidget::m_overlayStatic (resize)");
            m_ovGpuTex->setPixelSize(QSize(pw, ph));
            m_ovGpuTex->create();
            m_ovSrb->setBindings({
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_ovGpuTex, m_ovSampler),
            });
            m_ovSrb->create();
            m_overlayStaticDirty = true;
        }

        // Phase 3M-4 Task 12 — IMD overlay needs live re-rendering each
        // frame because peak detection runs against the current spectrum
        // buffer. Force the otherwise-static overlay to rebuild while the
        // IMD show condition is satisfied. Cost is one QImage repaint per
        // frame; only paid during PureSignal two-tone calibration.
        if (m_moxOverlay && m_testingIMD && m_showIMDMeasurements
                && m_displayDuplex) {
            m_overlayStaticDirty = true;
        }

        // 2026-05-26 KG4VCF perf instrumentation: time the overlay-
        // rebuild block (QImage fill + ~16 paint ops + GPU texture
        // upload) so the perf overlay can show avg/max cost and
        // confirm whether this is the dominant per-paint expense.
        QElapsedTimer ovlyTimer;
        const bool needsOverlayRebuild = m_overlayStaticDirty;
        if (needsOverlayRebuild) {
            ovlyTimer.start();
        }
        if (m_overlayStaticDirty) {
            m_overlayStatic.fill(Qt::transparent);
            QPainter p(&m_overlayStatic);
            p.setRenderHint(QPainter::Antialiasing, false);

            drawGrid(p, specRect);
            if (m_dbmScaleVisible) {
                // drawDbmScale needs the FULL-WIDTH rect so the strip
                // lands in the reserved right-edge zone at x=[w-kDbmStripW..w-1].
                // Passing the clipped specRect would put the strip INSIDE the spectrum.
                // dBm strip: both render modes use a readable full-height amplitude
                // reference; the 3D surface itself is perspective-foreshortened.
                // From AetherSDR SpectrumWidget.cpp:13983-13989 [@1872028c]
                if (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) {
                    drawDbmScale3D(p, QRect(0, 0, w, specH), dssFloorDbm());
                } else {
                    drawDbmScale(p, QRect(0, 0, w, specH));
                }
            }
            // Plan 4 D9 (Cluster E) + follow-up (option A): TX filter overlay
            // on panadapter (GPU path), MOX-gated.  Painted BEFORE drawBandPlan
            // + drawFreqScale so they overpaint the translucent filter band —
            // matches the QPainter path's z-order.  Same MOX gate as the
            // QPainter call site to keep both paths in lockstep.
            if (m_txFilterVisible && m_moxOverlay) {
                drawTxFilterOverlay(p, specRect);
            }
            // Phase 3M-4 Task 12 — IMD overlay (peak markers + readout box).
            // Show condition mirrors paintEvent (CPU path) and Thetis
            // display.cs:5008 [v2.10.3.13] verbatim.
            if (m_moxOverlay && m_testingIMD && m_showIMDMeasurements
                    && m_displayDuplex) {
                drawImdOverlay(p, specRect);
            }
            drawBandPlan(p, specRect);
            // Sub-epic E: time-scale + LIVE button on the right edge of the
            // waterfall area. Same FULL-WIDTH wfRect contract as the QPainter
            // path — the clipped `wfRect` local at line 3079 cannot be reused
            // because the time-scale helpers expect a wfRect spanning the full
            // widget width (the strip lives in the dBm-strip column).
            //
            // drawTimeScale must paint AFTER drawFreqScale so the LIVE
            // button (in the freq-scale row) is on top of the freq labels —
            // when paused the 40px button extends slightly past the
            // dBm-strip column's left edge into freq-scale territory.
            const QRect wfRectFull = layout.waterfallChrome;
            p.fillRect(layout.divider, QColor(0x30, 0x40, 0x50));
            drawFreqScale(p, layout.frequency);
            drawTimeScale(p, wfRectFull);
            // TNF notch overlay, GPU static-overlay path.  Same relative
            // ordering as the CPU paintEvent above and as AetherSDR
            // src/gui/SpectrumWidget.cpp:12013-12016 [@c6481cbf], where
            // drawTnfMarkers likewise precedes drawSpotMarkers in the
            // frequency-plane painter.  Missing THIS call site while
            // having the CPU one is a silent GPU-only regression, since
            // NEREUS_GPU_SPECTRUM is the shipping path.
            drawNotchMarkers(p, notchSpecRect());
            // Phase 3J-2 Task E1: spot overlay before VFO marker so labels
            // sit below the slice marker chrome. Mirrors the CPU paintEvent
            // ordering and AetherSDR SpectrumWidget.cpp:3787 [@0cd4559]
            // (drawSpotMarkers between trace and drawSliceMarkers).
            if (m_showSpots) {
                drawSpotMarkers(p, specRect);
            }
            drawVfoMarker(p, specRect, wfRect);
            drawOffScreenIndicator(p, specRect, wfRect);

            // Phase 3G-8 commit 10: waterfall chrome (filter/zero line/
            // timestamp/opacity dim) lands in the overlay texture on GPU
            // so the same setters work in both paths. Overlay texture
            // invalidation keys track every setter that feeds this plus
            // VFO/filter changes via setVfoFrequency/setFilterOffset.
            drawWaterfallChrome(p, wfRect);

            // 2026-05-26 KG4VCF dual-layer overlay split: peak-hold
            // trace + peak blobs + noise floor are NO LONGER painted
            // here.  They live in m_overlayDynamic which rebuilds at
            // 30 Hz (cheap) -- this static texture only rebuilds on
            // state change, so the expensive chrome work (grid,
            // scales, bandplan, freq/time scale, VFO marker, spot
            // markers, waterfall chrome) amortises to ~zero per
            // frame.

            // Bin width corner readout — Thetis lblDisplayBinWidth
            // (setup.cs:7061 [v2.10.3.13]).  Mirrors the CPU drawSpectrum
            // call; GPU overlay was missing it before this commit so the
            // toggle silently did nothing in the default Metal path.
            if (m_showBinWidth) {
                const double bw = binWidthHz();
                if (bw > 0.0) {
                    const QString bwText = QString::number(bw, 'f', 3)
                                         + QStringLiteral(" Hz/bin");
                    drawTextOverlay(p, specRect, OverlayPosition::BottomRight,
                                    bwText, m_gridTextColor);
                }
            }

            // Peak value overlay — Thetis console.cs:20073 PeakTextDelay
            // (refresh interval).  m_peakTextCache is rebuilt by the
            // m_peakTextTimer slot (SpectrumWidget.cpp:1735+) when the
            // toggle is on; this just renders the cached string through
            // drawTextOverlay so the GPU path has parity with CPU.
            if (m_showPeakValueOverlay && !m_peakTextCache.isEmpty()) {
                drawTextOverlay(p, specRect, m_peakValuePosition,
                                m_peakTextCache, m_peakValueColor);
            }

            // FPS overlay for GPU mode (QPainter path draws its own
            // counter in paintEvent). Drawn into the cached overlay
            // texture means it only updates on state changes or VFO
            // tuning — good enough for a diagnostic counter and avoids
            // re-uploading every frame.
            if (m_showFps) {
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                m_fpsFrameCount++;
                if (m_fpsLastUpdateMs == 0) {
                    m_fpsLastUpdateMs = nowMs;
                } else if (nowMs - m_fpsLastUpdateMs >= 1000) {
                    const double elapsed = (nowMs - m_fpsLastUpdateMs) / 1000.0;
                    m_fpsDisplayValue = static_cast<float>(m_fpsFrameCount / elapsed);
                    m_fpsFrameCount   = 0;
                    m_fpsLastUpdateMs = nowMs;
                }
                const QString fpsText =
                    QStringLiteral("%1 fps").arg(m_fpsDisplayValue, 0, 'f', 1);
                QFont ff = p.font();
                ff.setPixelSize(11);
                p.setFont(ff);
                p.setPen(m_gridTextColor);
                const int tw = p.fontMetrics().horizontalAdvance(fpsText);
                p.drawText(specRect.right() - tw - 8, specRect.top() + 14, fpsText);
            }

            // Cursor info — guarded by m_showCursorFreq (B8 Task 21).
            if (m_showCursorFreq && m_mouseInWidget) {
                drawCursorInfo(p, specRect);
            }

            // 2026-05-26 KG4VCF perf instrumentation overlay.  Painted
            // into the static cache so the cost is only paid when the
            // 1 Hz perf-poll timer invalidates the overlay (see
            // SpectrumWidget ctor).  Operator toggles via View ->
            // Performance Overlay; persisted under "ShowPerfOverlay".
            if (m_showPerfOverlay) {
                // Read the cached snapshot (the 1 Hz poll timer is the
                // sole snapshotAndClearDeltas() consumer; reading
                // here non-destructively avoids double-clearing the
                // delta counters).
                const auto stats =
                    PerfMonitor::instance().lastSnapshot();
                const QStringList lines = perfOverlayLines();
                QFont pf = p.font();
                pf.setPixelSize(11);
                pf.setFamily(QStringLiteral("Menlo"));
                p.setFont(pf);
                const QFontMetrics fm(pf);
                const int lineH = fm.height();
                int maxW = 0;
                for (const QString& s : lines) {
                    maxW = qMax(maxW, fm.horizontalAdvance(s));
                }
                const int padX = 8;
                const int padY = 4;
                const int boxW = maxW + 2 * padX;
                const int boxH = lines.size() * lineH + 2 * padY;
                // 2026-05-26 KG4VCF: top-right of the spectrum region.
                // The SpectrumOverlayPanel (10-button panel) lives in
                // the top-left of the spectrum, so a left-anchored
                // box slips behind those controls.  Top-right is
                // shared with the FPS counter (single line); push the
                // perf overlay down by ~22 px when FPS is on so they
                // do not collide.  specRect already excludes the
                // dBm strip column so we do not need to subtract
                // effectiveStripW() again.
                const int boxX = specRect.right() - boxW - 8;
                const int fpsOffset = m_showFps ? 22 : 0;
                const int boxY = specRect.top() + 8 + fpsOffset;
                // Health-coloured background: red if underruns/drops/
                // compressing OR paint/gap exceeds 33 ms; amber if any
                // metric is hot but functional; green when clean.
                // 2026-05-26 KG4VCF: factor audio ring-fill into the
                // health colour.  audioFillMinMs < 5 ms means the
                // plugin only had 5 ms of audio left at the worst
                // point in the last window -- a hair away from
                // underrun even if the underrun counter is still 0.
                const bool audioTight = stats.audioFillSamples > 0
                                     && stats.audioFillMinMs < 5.0;
                const bool audioWarn  = stats.audioFillSamples > 0
                                     && stats.audioFillMinMs < 15.0;
                bool red   = stats.audioUnderrunsDelta > 0
                          || stats.udpDropsDelta > 0
                          || stats.txIqUnderrunsDelta > 0
                          || stats.memCompressing
                          || stats.paintMsMax > 33.0
                          || stats.gapMsMax   > 50.0
                          || audioTight;
                bool amber = !red && (stats.paintMsMax > 20.0
                                   || stats.gapMsMax   > 40.0
                                   || stats.ovlyMsMax  > 15.0
                                   || audioWarn);
                const QColor bg = red   ? QColor(80, 20, 20, 220)
                                : amber ? QColor(80, 60, 20, 220)
                                        : QColor(20, 40, 20, 220);
                const QColor border = red   ? QColor(200, 80, 80)
                                    : amber ? QColor(200, 160, 60)
                                            : QColor(80, 200, 80);
                p.setPen(QPen(border, 1));
                p.setBrush(bg);
                p.drawRect(boxX, boxY, boxW, boxH);
                p.setPen(QColor(220, 220, 220));
                for (int i = 0; i < lines.size(); ++i) {
                    p.drawText(boxX + padX,
                               boxY + padY + fm.ascent() + i * lineH,
                               lines.at(i));
                }
            }

            // HIGH SWR / PA safety overlay — painted last so it sits on top
            // of all other chrome. From Thetis display.cs:4183-4201 [v2.10.3.13].
            paintHighSwrOverlay(p);

            m_overlayStaticDirty = false;
            m_overlayNeedsUpload = true;
        }
        if (needsOverlayRebuild) {
            PerfMonitor::instance().recordOverlayRebuild(
                static_cast<double>(ovlyTimer.nsecsElapsed()) / 1e6);
        }

        if (m_overlayNeedsUpload) {
            batch->uploadTexture(m_ovGpuTex, QRhiTextureUploadEntry(0, 0,
                QRhiTextureSubresourceUploadDescription(m_overlayStatic)));
            m_overlayNeedsUpload = false;
        }

        // ---- Dynamic overlay (peak-hold trace + peak blobs + NF) ----
        // 2026-05-26 KG4VCF dual-layer split: rebuild ONLY this small
        // set of per-frame features.  Chrome stays cached in
        // m_overlayStatic.  No chrome paint ops here = cheap rebuild
        // even under heavy system load.
        //
        // Sizing follows the static layer (full window) so the GPU
        // composite can use the same quad geometry + UBO.  Bytes
        // touched per rebuild are dominated by the QImage::fill -- a
        // memset over ~1.6 MB pinned memory, which under our mlock
        // pass is deterministic-latency regardless of system memory
        // pressure.  The per-frame paint ops themselves (3 ellipses
        // + 3 small text labels + 1 polyline + 1 dashed line) are
        // < 1 ms even under contention.
        if (m_overlayDynamic.size() != QSize(pw, ph)) {
            if (!m_overlayDynamic.isNull()) {
                unlockMemory(m_overlayDynamic.constBits(),
                             m_overlayDynamic.sizeInBytes());
            }
            m_overlayDynamic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
            m_overlayDynamic.setDevicePixelRatio(dpr);
            // Same uninitialized-buffer hazard as the init path above: the
            // rebuild below only touches the spectrum-height band, while the
            // quad samples the full texture.  Codex review, PR #291.
            m_overlayDynamic.fill(Qt::transparent);
            lockMemory(m_overlayDynamic.constBits(),
                       m_overlayDynamic.sizeInBytes(),
                       "SpectrumWidget::m_overlayDynamic (resize)");
            m_ovDynGpuTex->setPixelSize(QSize(pw, ph));
            m_ovDynGpuTex->create();
            m_ovDynSrb->setBindings({
                QRhiShaderResourceBinding::sampledTexture(1,
                    QRhiShaderResourceBinding::FragmentStage,
                    m_ovDynGpuTex, m_ovSampler),
            });
            m_ovDynSrb->create();
            m_overlayDynamicDirty = true;
        }

        if (m_overlayDynamicDirty) {
            QElapsedTimer dynTimer;
            dynTimer.start();

            // 2026-05-26 KG4VCF perf polish: partial-region rebuild.
            // The dynamic-overlay features (peak hold trace, peak blobs,
            // noise floor) ONLY paint into the spectrum portion of the
            // widget; the waterfall portion below is always transparent.
            // Bench under load 279 showed 75 / 82 samples of paint cost
            // were stuck in QRhiMetal::enqueueResourceUpdates -- the
            // Metal command queue was contended.  Halving the texture
            // upload bandwidth (only spectrum area, not full window)
            // proportionally reduces Metal queue pressure.
            //
            // Clear + upload only the spectrum region in device pixels.
            // CompositionMode_Source replaces the existing pixels with
            // transparent (vs Alpha-blend which would leave them).
            // The waterfall portion of the texture stays transparent
            // from the first full-window init -- the partial upload
            // never touches it.
            const QRect dynRectDevPx(0, 0, pw,
                qMax(1, static_cast<int>(specH * dpr)));

            {
                QPainter clearP(&m_overlayDynamic);
                clearP.setCompositionMode(QPainter::CompositionMode_Source);
                clearP.fillRect(dynRectDevPx, Qt::transparent);
            }

            QPainter pd(&m_overlayDynamic);
            pd.setRenderHint(QPainter::Antialiasing, false);

            if ((m_activePeakHold.enabled() || m_peakBlobs.enabled()) &&
                !m_renderedPixels.isEmpty()) {
                if (m_activePeakHold.active() && m_activePeakHold.size() > 0) {
                    paintActivePeakHoldTrace(pd, specRect);
                }
                if (m_peakBlobs.enabled() && !m_peakBlobs.displayDelayed()
                    && !m_peakBlobs.blobs().isEmpty()) {
                    paintPeakBlobs(pd, specRect);
                }
            }
            paintNoiseFloorOverlay(pd, specRect);

            // Reuse PerfMonitor's ovly metric for the *dynamic* rebuild
            // cost since that's now the per-frame variable; chrome
            // rebuilds are rare and their cost amortises out of the
            // 1 s perf window.
            PerfMonitor::instance().recordOverlayRebuild(
                static_cast<double>(dynTimer.nsecsElapsed()) / 1e6);

            // Partial-region upload.  setSourceTopLeft / setSourceSize
            // tell QRhi to copy only the spectrum portion of the QImage
            // into the matching region of the texture; the rest of the
            // texture is untouched.  Cuts Metal command-buffer payload
            // by 30-50% depending on spectrum / waterfall split.
            QRhiTextureSubresourceUploadDescription desc(m_overlayDynamic);
            desc.setSourceTopLeft(QPoint(0, 0));
            desc.setSourceSize(dynRectDevPx.size());
            desc.setDestinationTopLeft(QPoint(0, 0));
            batch->uploadTexture(m_ovDynGpuTex, QRhiTextureUploadEntry(0, 0, desc));
            m_overlayDynamicDirty = false;
        }
    }

    // ---- 3DSS mesh resource updates (only when 3D is the active mode) ----
    // Queued alongside the FFT vertex updates below so a single
    // cb->resourceUpdate(batch) submits everything for this frame. dpr is
    // computed locally here (rather than reusing the post-beginPass
    // declaration below) because renderTarget()->pixelSize() does not
    // require an active pass, and these batch writes must be recorded
    // before cb->resourceUpdate(batch) consumes the batch.
    if (m_spectrumRenderMode == SpectrumRenderMode::Mode3D) {
        rebuildDssMeshIfNeeded(batch);
        uploadDssPaletteLut(batch);
        uploadDssHeightRows(batch);
        const QSize dssOutputSize = renderTarget()->pixelSize();
        const float dssDpr =
            dssOutputSize.width() / static_cast<float>(qMax(1, w));
        // The mesh uniforms are written below, from the frame plan, only
        // when the mesh is drawn (GUI-I2).
        // Task 10: CPU fallback surface, built/uploaded only when the mesh
        // pipeline never came up (RGBA16F unsupported at
        // initDssMeshPipeline() time -- see its warning log). Skipped
        // whenever the mesh is live: no point paying for a QImage rebuild
        // the draw-call chain below will never composite.
        if (!m_dssMeshReady) {
            uploadDssFallbackImage(batch, specRect, dssDpr);
        }
    }

    // ---- Spectrum-region draw selection ----
    // Which spectrum-region draw this frame makes, and which trace buffers
    // it writes and binds, decided once by planSpectrumTraceFrame()
    // (SpectrumTraceFramePlan.h), after the 3DSS updates above (they can
    // clear m_dssMeshReady or size the fallback texture). The trace uploads
    // below and the draw calls after beginPass both read this plan, so a
    // buffer is never written in a frame that does not bind it.
    SpectrumTraceFrameInputs traceIn;
    traceIn.mode3D = (m_spectrumRenderMode == SpectrumRenderMode::Mode3D);
    traceIn.dssMeshReady = m_dssMeshReady;
    traceIn.dssHasRows = m_dss.rowCount() > 0;
    traceIn.dssFallbackReady =
        m_ovPipeline && m_dssFallbackSrb && m_dssFallbackTexW > 0;
    traceIn.tracePipelines = m_fftFillPipeline && m_fftLinePipeline;
    traceIn.traceBuffers = m_fftLineVbo && m_fftFillVbo;
    traceIn.peakBuffer = m_fftPeakVbo != nullptr;
    traceIn.hasPixels = !m_renderedPixels.isEmpty();
    traceIn.panFill = m_panFill;
    traceIn.peakHoldReady = m_peakHoldEnabled
        && m_pxPeakHold.size() == m_renderedPixels.size();
    traceIn.showsTransmitView = showsTransmitView();
    traceIn.waterfallPipeline = m_wfPipeline != nullptr;
    traceIn.heldTrace = m_visibleBinCount > 0;
    traceIn.heldFill = m_fftFillHasData;
    traceIn.heldPeak = m_peakHoldHasData;
    const SpectrumTraceFramePlan trace = planSpectrumTraceFrame(traceIn);

    // GUI-I2 / GUI-M1 (fix wave 2026-09-30): the 3DSS mesh and waterfall
    // uniform buffers are dynamic, so a write the frame does not bind stays
    // pending in Metal's list (see SpectrumTraceFramePlan.h). Each is
    // written only in a frame that draws with it.
    if (trace.writeDssMeshUbo) {
        const QSize dssOutputSize = renderTarget()->pixelSize();
        const float dssDpr =
            dssOutputSize.width() / static_cast<float>(qMax(1, w));
        writeDssMeshUbo(batch, specRect, dssDpr);
    }
    if (trace.writeWaterfallUbo) {
        batch->updateDynamicBuffer(m_wfUbo, 0, sizeof(wfUniforms), wfUniforms);
    }

    // ---- FFT spectrum vertices ----
    // GPU vertex generation -- one vertex per display pixel.  Mirrors
    // Thetis Display.cs:5249-5378 [v2.10.3.13] per-pixel render loop
    // (DrawLine over current_display_data[i], i = 0..nDecimatedWidth-1).
    // Honours:
    //   - local m_dbmCalOffset (shifts local pixels before y mapping)
    //   - m_gradientEnabled (off = flat m_fillColor, on = heatmap)
    //   - m_panFill (skips fill VBO update when disabled)
    //   - m_fillColor / m_fillAlpha (used for the flat-fill path)
    //   - m_peakHoldEnabled (generates a second line VBO for peak hold)
    //
    // Written only when this frame binds them (the trace plan, above). Qt's
    // Metal backend keeps a partial dynamic-buffer write in the buffer's
    // pending list until the buffer is next bound
    // (QRhiMetal::executeBufferHostWritesForSlot, qrhimetal.mm, Qt 6.11);
    // a buffer written every frame and never bound -- the 2D trace under a
    // 3D pan, the fill with pan fill off -- kept every frame's vertex copy,
    // 23 GB in 2.5 hours on the desktop window.
    if (trace.writeLine) {
        const int n = qMin(m_renderedPixels.size(), kMaxFftBins);
        m_visibleBinCount = n;
        const float minDbm = m_refLevel - m_dynamicRange;
        const float range  = m_dynamicRange;
        const float yBot = -1.0f;
        const float yTop = 1.0f;

        const float fa = m_fillAlpha;
        const float cal = displayCalOffsetDb();

        // Flat-mode colour picked from m_fillColor.
        const float flatR = m_fillColor.redF();
        const float flatG = m_fillColor.greenF();
        const float flatB = m_fillColor.blueF();

        QVector<float> lineVerts(n * kFftVertStride);
        const bool buildFill = trace.writeFill;
        QVector<float> fillVerts(buildFill ? n * 2 * kFftVertStride : 0);

        for (int j = 0; j < n; ++j) {
            float x = (n > 1) ? 2.0f * j / (n - 1) - 1.0f : 0.0f;
            float t = qBound(0.0f, ((m_renderedPixels[j] + cal) - minDbm) / range, 1.0f);
            float y = yBot + t * (yTop - yBot);

            float cr, cg, cb2;
            if (m_gradientEnabled) {
                // Heat map: blue → cyan → green → yellow → red
                // From AetherSDR SpectrumWidget.cpp:2298-2310
                if (t < 0.25f) {
                    float s = t / 0.25f;
                    cr = 0.0f; cg = s; cb2 = 1.0f;
                } else if (t < 0.5f) {
                    float s = (t - 0.25f) / 0.25f;
                    cr = 0.0f; cg = 1.0f; cb2 = 1.0f - s;
                } else if (t < 0.75f) {
                    float s = (t - 0.5f) / 0.25f;
                    cr = s; cg = 1.0f; cb2 = 0.0f;
                } else {
                    float s = (t - 0.75f) / 0.25f;
                    cr = 1.0f; cg = 1.0f - s; cb2 = 0.0f;
                }
            } else {
                cr = flatR; cg = flatG; cb2 = flatB;
            }

            // Line vertex
            int li = j * kFftVertStride;
            lineVerts[li]     = x;
            lineVerts[li + 1] = y;
            lineVerts[li + 2] = cr;
            lineVerts[li + 3] = cg;
            lineVerts[li + 4] = cb2;
            lineVerts[li + 5] = 0.9f;

            // Fill vertices (top at signal, bottom at base).
            if (!buildFill) {
                continue;
            }
            int fi = j * 2 * kFftVertStride;
            fillVerts[fi]     = x;
            fillVerts[fi + 1] = y;
            fillVerts[fi + 2] = cr;
            fillVerts[fi + 3] = cg;
            fillVerts[fi + 4] = cb2;
            fillVerts[fi + 5] = fa * 0.3f;
            fillVerts[fi + 6] = x;
            fillVerts[fi + 7] = yBot;
            fillVerts[fi + 8]  = 0.0f;
            fillVerts[fi + 9]  = 0.0f;
            fillVerts[fi + 10] = 0.3f;
            fillVerts[fi + 11] = fa;
        }

        batch->updateDynamicBuffer(m_fftLineVbo, 0,
            n * kFftVertStride * sizeof(float), lineVerts.constData());
        if (buildFill) {
            batch->updateDynamicBuffer(m_fftFillVbo, 0,
                n * 2 * kFftVertStride * sizeof(float), fillVerts.constData());
        }

        // Peak hold VBO lives alongside the line VBO -- generated here,
        // drawn in the render pass after the main line. When peak hold
        // is off we leave the buffer stale and skip the draw call via
        // m_peakHoldHasData.  Per-display-pixel array (m_pxPeakHold) is
        // sized to m_renderedPixels in updateSpectrumLinear.
        if (trace.writePeak) {
            QVector<float> peakVerts(n * kFftVertStride);
            for (int j = 0; j < n; ++j) {
                float x = (n > 1) ? 2.0f * j / (n - 1) - 1.0f : 0.0f;
                float t = qBound(0.0f, ((m_pxPeakHold[j] + cal) - minDbm) / range, 1.0f);
                float y = yBot + t * (yTop - yBot);
                int li = j * kFftVertStride;
                peakVerts[li]     = x;
                peakVerts[li + 1] = y;
                peakVerts[li + 2] = flatR;
                peakVerts[li + 3] = flatG;
                peakVerts[li + 4] = flatB;
                peakVerts[li + 5] = 0.55f;
            }
            batch->updateDynamicBuffer(m_fftPeakVbo, 0,
                n * kFftVertStride * sizeof(float), peakVerts.constData());
        }
    } else if (!trace.heldTrace) {
        // Nothing this frame binds the trace buffers, so nothing was written
        // to them: they no longer hold a trace to redraw later.
        m_visibleBinCount = 0;
    }
    m_fftFillHasData = trace.heldFill;
    m_peakHoldHasData = trace.heldPeak;

    cb->resourceUpdate(batch);

    // ---- Begin render pass ----
    const QColor clearColor(0x0a, 0x0a, 0x14);
    cb->beginPass(renderTarget(), clearColor, {1.0f, 0});

    const QSize outputSize = renderTarget()->pixelSize();
    const float dpr = outputSize.width() / static_cast<float>(qMax(1, w));

    // Draw waterfall
    if (trace.drawsWaterfall) {
        cb->setGraphicsPipeline(m_wfPipeline);
        cb->setShaderResources(m_wfSrb);
        float vpX = static_cast<float>(wfRect.x()) * dpr;
        float vpY = static_cast<float>(h - wfRect.bottom() - 1) * dpr;
        float vpW = static_cast<float>(wfRect.width()) * dpr;
        float vpH = static_cast<float>(wfRect.height()) * dpr;
        cb->setViewport({vpX, vpY, vpW, vpH});
        const QRhiCommandBuffer::VertexInput vbuf(m_wfVbo, 0);
        cb->setVertexInput(0, 1, &vbuf);
        cb->draw(4);
    }

    // Spectrum region: the 3DSS surface, or the classic FFT trace.
    // 3DSS replaces only the spectrum trace: the surface fills specRect and
    // the waterfall, divider, freq scale, and all overlays keep their
    // normal 2D positions. Everything below is identical to 2D except the
    // FFT trace is swapped for the 3DSS surface quad inside specRect.
    // From AetherSDR SpectrumWidget.cpp:13304-13307 [@1872028c].
    // The region draw comes from the trace plan decided above, before the
    // resource updates.
    if (trace.drawsDssMesh) {
        const float specVpX = static_cast<float>(specRect.x()) * dpr;
        const float specVpY = static_cast<float>(h - specRect.bottom() - 1) * dpr;
        const float specVpW = static_cast<float>(specRect.width()) * dpr;
        const float specVpH = static_cast<float>(specRect.height()) * dpr;
        const QRhiViewport specVp(specVpX, specVpY, specVpW, specVpH);
        const int rows = m_dss.visibleRowCount();

        cb->setGraphicsPipeline(m_dssFillPipeline);
        cb->setShaderResources(m_dssSrb);
        cb->setViewport(specVp);
        const QRhiCommandBuffer::VertexInput fillVbuf(m_dssMeshVbo, 0);
        cb->setVertexInput(0, 1, &fillVbuf);
        cb->draw(rows * dssFillVerticesPerRow(m_dssMeshCols));

        // OpenGL binds the fill pipeline here too (dssOutlinePipelineFor) --
        // m_dssLinePipeline is null on that backend, never created above.
        cb->setGraphicsPipeline(dssOutlinePipelineFor(
            m_dssOutlinePipelineMode, m_dssFillPipeline, m_dssLinePipeline));
        cb->setShaderResources(m_dssSrb);
        cb->setViewport(specVp);
        const QRhiCommandBuffer::VertexInput lineVbuf(m_dssMeshLineVbo, 0);
        cb->setVertexInput(0, 1, &lineVbuf);
        cb->draw(rows * dssLineVerticesPerRow(m_dssMeshCols));
    } else if (trace.drawsDssFallback) {
        // Task 10: CPU cached-image fallback (mesh pipeline unavailable --
        // is3D is false here precisely because m_dssMeshReady is false, so
        // this branch is Mode3D's ONLY remaining path; it must win over the
        // classic-FFT-trace branch below or 3D mode would silently render
        // the 2D trace instead of the CPU surface it's supposed to fall
        // back to). Reuses the overlay pipeline/VBO -- the same textured
        // full-screen-quad shader every static/dynamic overlay draw already
        // uses -- with its own SRB bound to the small capped-resolution DSS
        // texture, stretched to the spectrum viewport exactly like the mesh
        // draw above. From AetherSDR SpectrumWidget.cpp:14643-14654
        // [@1872028c] (upstream's own `is3D && m_ovPipeline && m_dssSrb &&
        // m_dssTexW > 0` cached-image draw, same shape, this task's own
        // field names).
        const float specVpX = static_cast<float>(specRect.x()) * dpr;
        const float specVpY = static_cast<float>(h - specRect.bottom() - 1) * dpr;
        const float specVpW = static_cast<float>(specRect.width()) * dpr;
        const float specVpH = static_cast<float>(specRect.height()) * dpr;
        cb->setGraphicsPipeline(m_ovPipeline);
        cb->setShaderResources(m_dssFallbackSrb);
        cb->setViewport({specVpX, specVpY, specVpW, specVpH});
        const QRhiCommandBuffer::VertexInput vbuf(m_ovVbo, 0);
        cb->setVertexInput(0, 1, &vbuf);
        cb->draw(4);
    } else if (trace.drawsTrace) {
        float specVpX = static_cast<float>(specRect.x()) * dpr;
        float specVpY = static_cast<float>(h - specRect.bottom() - 1) * dpr;
        float specVpW = static_cast<float>(specRect.width()) * dpr;
        float specVpH = static_cast<float>(specRect.height()) * dpr;
        QRhiViewport specVp(specVpX, specVpY, specVpW, specVpH);

        // Fill pass — Phase 3G-8 commit 10: skip when fill is disabled.
        if (trace.bindFill) {
            cb->setGraphicsPipeline(m_fftFillPipeline);
            cb->setShaderResources(m_fftSrb);
            cb->setViewport(specVp);
            const QRhiCommandBuffer::VertexInput fillVbuf(m_fftFillVbo, 0);
            cb->setVertexInput(0, 1, &fillVbuf);
            cb->draw(m_visibleBinCount * 2);
        }

        // Peak hold line (drawn before main line so live trace is on top).
        if (trace.bindPeak) {
            cb->setGraphicsPipeline(m_fftLinePipeline);
            cb->setShaderResources(m_fftSrb);
            cb->setViewport(specVp);
            const QRhiCommandBuffer::VertexInput peakVbuf(m_fftPeakVbo, 0);
            cb->setVertexInput(0, 1, &peakVbuf);
            cb->draw(m_visibleBinCount);
        }

        // Line pass
        cb->setGraphicsPipeline(m_fftLinePipeline);
        cb->setShaderResources(m_fftSrb);
        cb->setViewport(specVp);
        const QRhiCommandBuffer::VertexInput lineVbuf(m_fftLineVbo, 0);
        cb->setVertexInput(0, 1, &lineVbuf);
        cb->draw(m_visibleBinCount);
    }

    // Draw overlay -- static chrome layer first, then dynamic
    // overlays on top.  Same pipeline + VBO; only the SRB / texture
    // binding differs.  Order matters: chrome includes the
    // freq/dBm/time scales which need to read clean from the
    // spectrum; dynamic overlays (peak blobs / peak hold / NF) sit
    // on top of chrome.
    if (m_ovPipeline) {
        cb->setGraphicsPipeline(m_ovPipeline);
        cb->setViewport({0, 0,
            static_cast<float>(outputSize.width()),
            static_cast<float>(outputSize.height())});
        const QRhiCommandBuffer::VertexInput vbuf(m_ovVbo, 0);
        cb->setVertexInput(0, 1, &vbuf);
        // Chrome layer.
        cb->setShaderResources(m_ovSrb);
        cb->draw(4);
        // Dynamic layer (peak hold / peak blobs / noise floor).
        // 2026-05-26 KG4VCF dual-layer overlay split.
        if (m_ovDynSrb) {
            cb->setShaderResources(m_ovDynSrb);
            cb->draw(4);
        }
    }

    cb->endPass();

    // 2026-05-26 KG4VCF perf instrumentation: record total render
    // cost (texture uploads + draw-call submission, both main-thread).
    // Excludes GPU-side execution because that completes asynchronously
    // after this function returns -- this is purely the CPU-side
    // command-build cost the main thread paid.
    PerfMonitor::instance().recordPaintFrame(
        static_cast<double>(paintTimer.nsecsElapsed()) / 1e6);
}

void SpectrumWidget::render(QRhiCommandBuffer* cb)
{
    if (!m_rhiInitialized) { return; }
    renderGpuFrame(cb);
}

void SpectrumWidget::releaseResources()
{
    delete m_wfPipeline;      m_wfPipeline = nullptr;
    delete m_wfSrb;           m_wfSrb = nullptr;
    delete m_wfVbo;           m_wfVbo = nullptr;
    delete m_wfUbo;           m_wfUbo = nullptr;
    delete m_wfGpuTex;        m_wfGpuTex = nullptr;
    delete m_wfSampler;       m_wfSampler = nullptr;

    delete m_ovPipeline;      m_ovPipeline = nullptr;
    delete m_ovSrb;           m_ovSrb = nullptr;
    delete m_ovVbo;           m_ovVbo = nullptr;
    delete m_ovGpuTex;        m_ovGpuTex = nullptr;
    delete m_ovSampler;       m_ovSampler = nullptr;
    // Release the static overlay's lock too.  initOverlayPipeline() locks
    // it and, on device/surface recreation, runs again and replaces the
    // QImage with a freshly locked one -- so skipping it here leaked one
    // MemoryLock registration per recreate, inflating the locked-byte
    // count and potentially pinning allocator pages for buffers that no
    // longer exist.  Codex review, PR #291.
    //
    // Deliberately NOT m_waterfall: that buffer is locked where it is
    // reallocated on a size change, not in initOverlayPipeline(), so it
    // survives this call still locked and still in use.  Unlocking it here
    // would drop a live pin that nothing re-establishes.
    if (!m_overlayStatic.isNull()) {
        unlockMemory(m_overlayStatic.constBits(),
                     m_overlayStatic.sizeInBytes());
    }
    // 2026-05-26 KG4VCF dual-layer overlay split: tear down the
    // dynamic-overlay resources.  Pipeline + VBO + sampler are
    // shared with the static layer; only SRB + texture are
    // separate.
    delete m_ovDynSrb;        m_ovDynSrb = nullptr;
    delete m_ovDynGpuTex;     m_ovDynGpuTex = nullptr;
    if (!m_overlayDynamic.isNull()) {
        unlockMemory(m_overlayDynamic.constBits(),
                     m_overlayDynamic.sizeInBytes());
    }

    delete m_fftLinePipeline;  m_fftLinePipeline = nullptr;
    delete m_fftFillPipeline;  m_fftFillPipeline = nullptr;
    delete m_fftSrb;           m_fftSrb = nullptr;
    delete m_fftLineVbo;       m_fftLineVbo = nullptr;
    delete m_fftFillVbo;       m_fftFillVbo = nullptr;
    delete m_fftPeakVbo;       m_fftPeakVbo = nullptr;

    // 3DSS mesh (3D stacked-trace spectrum plan, Task 7).
    delete m_dssFillPipeline;    m_dssFillPipeline = nullptr;
    delete m_dssLinePipeline;    m_dssLinePipeline = nullptr;
    delete m_dssSrb;             m_dssSrb = nullptr;
    delete m_dssMeshVbo;         m_dssMeshVbo = nullptr;
    delete m_dssMeshLineVbo;     m_dssMeshLineVbo = nullptr;
    delete m_dssUbo;             m_dssUbo = nullptr;
    delete m_dssHeightTex;       m_dssHeightTex = nullptr;
    delete m_dssPaletteTex;      m_dssPaletteTex = nullptr;
    delete m_dssHeightSampler;   m_dssHeightSampler = nullptr;
    delete m_dssPaletteSampler;  m_dssPaletteSampler = nullptr;
    m_dssMeshReady = false;
    m_dssMeshUploaded = false;
    m_dssMeshCols = 0;
    m_dssLutToken = ~0ull;
    m_dssUploadedRowGeneration = ~0ull;
    m_dssLastUploadedHead = -1;

    // 3DSS CPU-fallback quad (Task 10).
    delete m_dssFallbackTex;  m_dssFallbackTex = nullptr;
    delete m_dssFallbackSrb;  m_dssFallbackSrb = nullptr;
    m_dssFallbackTexW = 0;
    m_dssFallbackTexH = 0;
    m_dssFallbackUploadedGen = ~0ull;

    m_rhiInitialized = false;
}

#endif // NEREUS_GPU_SPECTRUM

// ── Detach-safe RHI lifecycle (float / dock a pan) ───────────────────────
// See SpectrumWidget.h for the bench report these three close.

// From AetherSDR src/gui/SpectrumWidget.cpp:2227-2247 [@1e0718ad]
//   adapter: NereusSDR gates on NEREUS_GPU_SPECTRUM where upstream gates on
//   AETHER_GPU_SPECTRUM. Upstream's comment records that gating this to
//   Q_OS_MAC let the identical crash through on Windows, so it stays
//   cross-platform here too.
void SpectrumWidget::prepareForTopLevelChange()
{
#ifdef NEREUS_GPU_SPECTRUM
    // QEvent::WindowAboutToChangeInternal is how QRhiWidgetPrivate
    // deregisters its cleanup callback from the outgoing QRhi, and it does
    // so identically on Metal, D3D and Vulkan.
    QEvent event(QEvent::WindowAboutToChangeInternal);
    QCoreApplication::sendEvent(this, &event);
#endif
}

// From AetherSDR src/gui/SpectrumWidget.cpp:2397-2420 [@0dea0dd7].
// Adapter: use Nereus's GPU build flag; shutdown is also invoked by the stack
// before the top-level backing store is destroyed. Only destroy native CHILD
// windows here: a standalone SpectrumWidget owns the QRhi its base destructor
// still needs, so destroying its top-level window here would free that QRhi.
void SpectrumWidget::prepareForShutdown()
{
    if (m_shutdownPrepared) { return; }
    m_shutdownPrepared = true;

    prepareForTopLevelChange();
    setUpdatesEnabled(false);
    hide();

#ifdef NEREUS_GPU_SPECTRUM
    releaseResources();
#ifdef Q_OS_MAC
    // Drop the native child window while its parent backing store is still
    // alive, so any remaining platform resources are gone before QWidgetWindow
    // destruction runs on app exit.
    if (!isWindow()) { destroy(true, true); }
#endif
#endif
}

// From AetherSDR src/gui/SpectrumWidget.cpp:7092-7110 [@1e0718ad]
//   adapter: upstream also commits/clears a frequency preview here (its
//   shutdown path shares this entry point); NereusSDR has no preview state
//   to settle, so only the GPU teardown ports.
void SpectrumWidget::resetGpuResources()
{
#ifdef NEREUS_GPU_SPECTRUM
    // On macOS/Windows the GPU surface does not survive reparenting, so the
    // pipelines have to go and be rebuilt by initialize() against the new
    // window. On Linux (OpenGL) a repaint is enough.
#ifndef Q_OS_LINUX
    releaseResources();
#endif
#endif
    update();
}

// From AetherSDR src/gui/SpectrumWidget.cpp:1843-1857 [@1e0718ad]
//   adapter: upstream gates on an env-var escape hatch
//   (nativeWindowPreferred(), AETHER_PAN_NO_NATIVE_WINDOW); NereusSDR has no
//   such override today, so the policy applies unconditionally on macOS.
void SpectrumWidget::applyNativeWindowIsolationPolicy()
{
#if defined(NEREUS_GPU_SPECTRUM) && defined(Q_OS_MAC)
    // Order matters: block ancestor promotion *before* requesting the native
    // window, so realizing the leaf's NSView cannot drag its QWidget tree
    // native (redundant window-sized Core Animation backing stores).
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_NativeWindow);
#endif
}

// ============================================================================
// VFO Flag Widget Hosting (AetherSDR pattern)
// ============================================================================

void SpectrumWidget::setVfoFrequency(double hz)
{
    m_vfoHz = hz;

    if (!m_ctunEnabled) {
        // Traditional mode: auto-scroll pan center to keep VFO visible
        // From Thetis console.cs:31371-31385
        // Upstream inline attribution preserved verbatim (console.cs:31381):
        //   //-W2PA If we tune beyond the display limits, re-center or scroll display, and keep going.  Original code above just stops tuning at edges.
        double leftEdge = m_centerHz - m_bandwidthHz / 2.0;
        double rightEdge = m_centerHz + m_bandwidthHz / 2.0;
        double margin = m_bandwidthHz * 0.10;

        bool needsScroll = false;
        if (hz < leftEdge + margin) {
            applyViewWindow(hz + m_bandwidthHz / 2.0 - margin, m_bandwidthHz);
            needsScroll = true;
        } else if (hz > rightEdge - margin) {
            applyViewWindow(hz - m_bandwidthHz / 2.0 + margin, m_bandwidthHz);
            needsScroll = true;
        }

        if (needsScroll) {
            emit centerChanged(m_centerHz);
        }
    }

    // Update off-screen indicator state (both modes)
    double leftEdge = m_centerHz - m_bandwidthHz / 2.0;
    double rightEdge = m_centerHz + m_bandwidthHz / 2.0;
    if (hz < leftEdge) {
        m_vfoOffScreen = VfoOffScreen::Left;
    } else if (hz > rightEdge) {
        m_vfoOffScreen = VfoOffScreen::Right;
    } else {
        m_vfoOffScreen = VfoOffScreen::None;
    }

    updateVfoPositions();
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
#else
    update();
#endif
}

void SpectrumWidget::recenterOnVfo()
{
    applyViewWindow(m_vfoHz, m_bandwidthHz);
    m_vfoOffScreen = VfoOffScreen::None;
    emit centerChanged(m_centerHz);
    updateVfoPositions();
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
#else
    update();
#endif
}

void SpectrumWidget::endPanDrag()
{
    if (!m_draggingPan) { return; }
    m_draggingPan = false;
    setCursor(Qt::CrossCursor);
}

void SpectrumWidget::applyRemoteCtunState(bool available, bool pinned)
{
    m_ctunAvailable = available;
    m_ctunEnabled = available && pinned;
    if (m_overlayMenu) {
        m_overlayMenu->setCtunAvailable(available);
        m_overlayMenu->setCtunState(m_ctunEnabled);
    }
    update();
}

void SpectrumWidget::setCtunEnabled(bool enabled)
{
    if (!m_ctunAvailable) { return; }
    m_ctunPreference = enabled;
    if (m_ctunEnabled == enabled) {
        return;
    }
    m_ctunEnabled = enabled;

    if (!enabled) {
        // Switching to traditional mode: recenter on VFO
        recenterOnVfo();
    }
    // Recompute off-screen state
    setVfoFrequency(m_vfoHz);

    emit ctunEnabledChanged(enabled);
    scheduleSettingsSave();
}

VfoWidget* SpectrumWidget::addVfoWidget(int sliceIndex)
{
    if (m_vfoWidgets.contains(sliceIndex)) {
        return m_vfoWidgets[sliceIndex];
    }

    auto* w = new VfoWidget(this);
    w->setSliceIndex(sliceIndex);
    m_vfoWidgets[sliceIndex] = w;
    // Whether the flag's slice is the selected one decides its marker's
    // colours (sliceMarkerGeometry). The GPU path caches the markers in the
    // static overlay, so a flip has to invalidate that cache; a bare update()
    // would redraw the old colours from it.
    connect(w, &VfoWidget::activeSliceChanged, this, [this](bool) {
        markOverlayDirty();
    });
    w->show();
    w->raise();
    return w;
}

void SpectrumWidget::removeVfoWidget(int sliceIndex)
{
    m_edgeMarkedSlices.remove(sliceIndex);
    if (auto* w = m_vfoWidgets.take(sliceIndex)) {
        // The flag's close / lock / record / play buttons are parented to THIS
        // widget, not to the flag, so deleting the flag alone orphans them and
        // they stay painted on the pan. Bench-caught 2026-07-26: creating and
        // removing slices left a stack of dead button columns behind. Cleared
        // here rather than in ~VfoWidget because doing it while both objects
        // are alive keeps the destruction order ours (issue #113).
        w->destroyFloatingButtons();
        delete w;
        update();
    }
}

VfoWidget* SpectrumWidget::vfoWidget(int sliceIndex) const
{
    return m_vfoWidgets.value(sliceIndex, nullptr);
}

void SpectrumWidget::setEdgeMarkedSlice(int sliceIndex, bool edgeMarked)
{
    const bool changed = edgeMarked ? !m_edgeMarkedSlices.contains(sliceIndex)
                                    : m_edgeMarkedSlices.contains(sliceIndex);
    if (!changed) { return; }
    if (edgeMarked) {
        m_edgeMarkedSlices.insert(sliceIndex);
    } else {
        m_edgeMarkedSlices.remove(sliceIndex);
    }
    refreshSliceFlags();
}

void SpectrumWidget::refreshSliceFlags()
{
    updateVfoPositions();
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
#else
    update();
#endif
}

// See the header for the bench defect this closes (Sub-Epic J,
// 2026-07-28). Immediate effect here is only half the fix -- see
// raiseFrontVfoWidget(), which updateVfoPositions() also calls every frame
// so the pin survives past the next position pass.
void SpectrumWidget::setFrontSliceIndex(int sliceIndex)
{
    m_frontSliceIndex = sliceIndex;
    raiseFrontVfoWidget();
}

void SpectrumWidget::raiseFrontVfoWidget()
{
    VfoWidget* w = m_vfoWidgets.value(m_frontSliceIndex, nullptr);
    if (!w) {
        // No pin, or the pinned slice has no flag on THIS pan (a different
        // pan hosts it, or this pan's flag has not been built yet) -- leave
        // this pan's own order alone.
        return;
    }
    w->raiseAboveSiblings();
}

void SpectrumWidget::updateVfoPositions()
{
    if (width() <= 0 || height() <= 0) {
        return;
    }

    // Recompute off-screen state (pan drag changes center without calling setVfoFrequency)
    double leftEdge = m_centerHz - m_bandwidthHz / 2.0;
    double rightEdge = m_centerHz + m_bandwidthHz / 2.0;
    if (m_vfoHz < leftEdge) {
        m_vfoOffScreen = VfoOffScreen::Left;
    } else if (m_vfoHz > rightEdge) {
        m_vfoOffScreen = VfoOffScreen::Right;
    } else {
        m_vfoOffScreen = VfoOffScreen::None;
    }

    const QRect specRect = spectrumLayout().spectrum;

    // Each flag is placed from ITS OWN slice frequency, not from the pan's
    // m_vfoHz.
    //
    // m_vfoHz is a single per-pan value that tracks whichever slice most
    // recently called setVfoFrequency, so deriving one vfoX from it and moving
    // every flag there stacked all the co-hosted flags on one x and made them
    // move together. The models were never wrong -- tst_radio_model_slice_
    // lifecycle pins that two slices sharing a DDC window hold independent
    // frequencies and independent shift offsets -- this was placement alone.
    // Bench-reported 2026-07-28: "If I add B flag to panadapter 1, A and B are
    // still overlaid and stuck on top of each other."
    //
    // A single-slice pan is unchanged: its one flag carries the same frequency
    // the pan does, so the x it lands on is identical to the pre-fix one.
    for (auto it = m_vfoWidgets.begin(); it != m_vfoWidgets.end(); ++it) {
        VfoWidget* vfo = it.value();
        if (vfo->width() <= 0) {
            vfo->adjustSize();
        }
        // Hide VFO flag when off-screen (SmartSDR pattern).
        //
        // Per flag, for the same reason as the placement above: the pan-level
        // m_vfoOffScreen answers "is the PAN's VFO outside the window", which
        // hid a perfectly on-window flag whenever some other slice on the same
        // pan was tuned away. m_vfoOffScreen still drives the pan's own
        // off-screen chevron (drawOffScreenIndicator) and is left alone.
        const double flagHz = vfo->frequency();
        if (!vfo->stationPresentationAllowed() || flagHz < leftEdge || flagHz > rightEdge) {
            vfo->hide();
        } else {
            // isHidden(), not isVisible(): a flag on a pan that has not been
            // shown yet reads !isVisible() forever, which turned this into an
            // unconditional show() on every pass.
            if (vfo->isHidden()) {
                vfo->show();
            }
            vfo->updatePosition(hzToX(flagHz, specRect), 0);
            vfo->raise();
        }
    }

    // The loop above just raised every visible flag once, in m_vfoWidgets'
    // ascending slice-index order -- so without this, whichever slice has
    // the HIGHEST index would land on top after every single frame,
    // regardless of which one is active. Bench-reported 2026-07-28
    // (Sub-Epic J): "slice A selected, slice B's flag covered A's, clipping
    // A's frequency readout." Re-asserting the pin here, after the loop, is
    // what makes it survive this pass instead of only the one it was set on.
    raiseFrontVfoWidget();
}

// ---- Phase 3Q-8: disconnect overlay ----------------------------------------

void SpectrumWidget::setConnectionState(ConnectionState s)
{
    if (m_connState == s) {
        return;
    }
    const bool wasConnected = (m_connState == ConnectionState::Connected);
    const bool isConnected = (s == ConnectionState::Connected);
    m_connState = s;

    if (wasConnected && !isConnected) {
        // Connected → not-Connected: fade dim factor 1.0 → 0.4 over 800 ms.
        if (!m_fadeAnim) {
            m_fadeAnim = new QPropertyAnimation(this, "disconnectFade");
            m_fadeAnim->setDuration(800);
        }
        m_fadeAnim->stop();
        m_fadeAnim->setStartValue(1.0f);
        m_fadeAnim->setEndValue(0.4f);
        m_fadeAnim->start();
    } else if (!wasConnected && isConnected) {
        // not-Connected → Connected: snap back to full opacity.
        if (m_fadeAnim) {
            m_fadeAnim->stop();
        }
        m_disconnectFade = 1.0f;
    }

    // Show/hide the child overlay label and keep it sized + on top.
    if (m_disconnectLabel) {
        m_disconnectLabel->setGeometry(0, 0, width(), height());
        m_disconnectLabel->setVisible(!isConnected);
        if (!isConnected) {
            m_disconnectLabel->raise();
        }
    }
    update();
}

} // namespace NereusSDR
