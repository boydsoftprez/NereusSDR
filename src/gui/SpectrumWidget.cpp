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
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
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
#include "SpectrumOverlayMenu.h"
#include "ImdOverlay.h"
#include "widgets/VfoWidget.h"
#include "ColorSwatchButton.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"   // Phase 3M-4 bench-fix Round 2: lcSpectrum
#include "dbm_strip_math.h"
#include "models/BandPlanManager.h"
#include "spectrum/SpectrumDetector.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QHoverEvent>
#include <QLabel>
#include <QPropertyAnimation>
#include <QToolTip>
#include <QUrl>

#include <QDateTime>
#include <QTimeZone>
#include <QMap>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFile>

#ifdef NEREUS_GPU_SPECTRUM
#include <rhi/qshader.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace NereusSDR {

// ---- Default waterfall gradient stops (AetherSDR style) ----
// From AetherSDR SpectrumWidget.cpp:43-51
static const WfGradientStop kDefaultStops[] = {
    {0.00f,   0,   0,   0},    // black
    {0.15f,   0,   0, 128},    // dark blue
    {0.30f,   0,  64, 255},    // blue
    {0.45f,   0, 200, 255},    // cyan
    {0.60f,   0, 220,   0},    // green
    {0.80f, 255, 255,   0},    // yellow
    {1.00f, 255,   0,   0},    // red
};

// Enhanced scheme — from Thetis display.cs:6864-6954 (9-band progression)
static const WfGradientStop kEnhancedStops[] = {
    {0.000f,   0,   0,   0},   // black
    {0.111f,   0,   0, 255},   // blue
    {0.222f,   0, 255, 255},   // cyan
    {0.333f,   0, 255,   0},   // green
    {0.444f, 128, 255,   0},   // yellow-green
    {0.556f, 255, 255,   0},   // yellow
    {0.667f, 255, 128,   0},   // orange
    {0.778f, 255,   0,   0},   // red
    {0.889f, 255,   0, 128},   // red-magenta
    {1.000f, 192,   0, 255},   // purple
};

// Spectran scheme — from Thetis display.cs:6956-7036
static const WfGradientStop kSpectranStops[] = {
    {0.00f,   0,   0,   0},    // black
    {0.10f,  32,   0,  64},    // dark purple
    {0.25f,   0,   0, 255},    // blue
    {0.40f,   0, 192,   0},    // green
    {0.55f, 255, 255,   0},    // yellow
    {0.70f, 255, 128,   0},    // orange
    {0.85f, 255,   0,   0},    // red
    {1.00f, 255, 255, 255},    // white
};

// Black-white scheme — from Thetis display.cs:7038-7075
static const WfGradientStop kBlackWhiteStops[] = {
    {0.00f,   0,   0,   0},    // black
    {1.00f, 255, 255, 255},    // white
};

// LinLog scheme — linear ramp in low region, log-shaped in high region.
// Approximation of Thetis "LinLog" combo entry (setup.cs:11904-11939).
// Phase 3G-8 commit 5 addition.
static const WfGradientStop kLinLogStops[] = {
    {0.00f,   0,   0,   0},
    {0.10f,   0,   0,  96},
    {0.25f,   0,  64, 192},
    {0.50f,   0, 192, 192},
    {0.65f,   0, 224,  64},
    {0.80f, 255, 192,   0},
    {1.00f, 255,   0,   0},
};

// LinRad scheme — LinRadiance-style cool → hot gradient. Phase 3G-8.
static const WfGradientStop kLinRadStops[] = {
    {0.00f,   0,   0,   0},
    {0.15f,  16,  16, 120},
    {0.30f,  32,  80, 200},
    {0.50f,   0, 200, 255},
    {0.70f, 200, 255, 120},
    {0.85f, 255, 200,   0},
    {1.00f, 255,  32,   0},
};

// Custom scheme — loaded from AppSettings "DisplayWfCustomStops" if set,
// otherwise falls back to Default. Phase 3G-8 commit 5. Runtime state
// is parsed lazily from the settings key when the scheme is selected.
// For now the static fallback keeps the same stops as Default.
static const WfGradientStop kCustomFallbackStops[] = {
    {0.00f,   0,   0,   0},
    {0.15f,   0,   0, 128},
    {0.30f,   0,  64, 255},
    {0.45f,   0, 200, 255},
    {0.60f,   0, 220,   0},
    {0.80f, 255, 255,   0},
    {1.00f, 255,   0,   0},
};

// Phase 3G-9b: Clarity Blue palette — full-spectrum rainbow with a
// deep-black noise floor. The "blue look" a user sees most of the time
// comes from the combination of AGC + tight thresholds compressing most
// signals into the blue/cyan range of the palette; strong signals still
// cleanly progress through green → yellow → red so peak energy remains
// distinguishable. Compare to Default/Enhanced which start at dark blue
// and spread the bright colours across the full range (producing a noisy
// noise floor). Visual target: 2026-04-14/2026-04-15 AetherSDR reference.
static const WfGradientStop kClarityBlueStops[] = {
    {0.00f,  0x00, 0x00, 0x00},  // pure black — noise floor bottom
    {0.18f,  0x02, 0x08, 0x20},  // very dark blue — noise floor top
    {0.32f,  0x08, 0x20, 0x58},  // dark blue — weak signal edge
    {0.46f,  0x10, 0x50, 0xb0},  // medium blue — weak signals
    {0.58f,  0x10, 0xa0, 0xe0},  // cyan — medium signals
    {0.70f,  0x10, 0xd0, 0x60},  // green — strong signals
    {0.80f,  0xf0, 0xe0, 0x10},  // yellow — very strong
    {0.90f,  0xff, 0x80, 0x00},  // orange — extreme
    {0.96f,  0xff, 0x20, 0x20},  // red — peak
    {1.00f,  0xff, 0x40, 0xc0},  // magenta — absolute peak
};

const WfGradientStop* wfSchemeStops(WfColorScheme scheme, int& count)
{
    switch (scheme) {
    case WfColorScheme::Enhanced:
        count = 10;
        return kEnhancedStops;
    case WfColorScheme::Spectran:
        count = 8;
        return kSpectranStops;
    case WfColorScheme::BlackWhite:
        count = 2;
        return kBlackWhiteStops;
    case WfColorScheme::LinLog:
        count = 7;
        return kLinLogStops;
    case WfColorScheme::LinRad:
        count = 7;
        return kLinRadStops;
    case WfColorScheme::Custom:
        count = 7;
        return kCustomFallbackStops;
    case WfColorScheme::ClarityBlue:
        count = 10;
        return kClarityBlueStops;
    case WfColorScheme::Default:
    default:
        count = 7;
        return kDefaultStops;
    }
}

// ---- SpectrumWidget ----

SpectrumWidget::SpectrumWidget(QWidget* parent)
    : SpectrumBaseClass(parent)
{
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
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_Hover);  // Ensure HoverMove events are delivered
#elif defined(Q_OS_WIN)
    setApi(QRhiWidget::Api::Direct3D11);
    setAttribute(Qt::WA_NativeWindow);
#endif
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

    // Timer-driven display repaint — decouples repaint rate from FFT data arrival
    // so updates are evenly spaced regardless of IQ buffer fill timing.
    m_displayTimer.setInterval(33); // 30 fps default
    m_displayTimer.setSingleShot(false);
    connect(&m_displayTimer, &QTimer::timeout, this, [this]() {
        if (m_hasNewSpectrum) {
            m_hasNewSpectrum = false;
            update();
        }
    });
    m_displayTimer.start();

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
}

SpectrumWidget::~SpectrumWidget() = default;

// ---- Settings persistence ----
// Per-pan keys use AetherSDR pattern: "DisplayFftSize" for pan 0, "DisplayFftSize_1" for pan 1
static QString settingsKey(const QString& base, int panIndex)
{
    if (panIndex == 0) {
        return base;
    }
    return QStringLiteral("%1_%2").arg(base).arg(panIndex);
}

void SpectrumWidget::loadSettings()
{
    auto& s = AppSettings::instance();

    auto readFloat = [&](const QString& key, float def) -> float {
        QString val = s.value(settingsKey(key, m_panIndex)).toString();
        if (val.isEmpty()) { return def; }
        bool ok = false;
        float v = val.toFloat(&ok);
        return ok ? v : def;
    };
    auto readInt = [&](const QString& key, int def) -> int {
        QString val = s.value(settingsKey(key, m_panIndex)).toString();
        if (val.isEmpty()) { return def; }
        bool ok = false;
        int v = val.toInt(&ok);
        return ok ? v : def;
    };

    // Ship defaults — calibrated 2026-04-30 against a live ANAN-G2 with
    // a typical residential noise floor (-115 to -120 dBm in the
    // amateur HF bands). Earlier defaults ran 12 dB hotter (Grid -36 /
    // -104, Wf -50 / -110, Wf black 98) and gave a noisy first-launch
    // experience — band noise jammed the bottom of the panadapter and
    // lit up the waterfall floor. Shifting the entire reference plane
    // down 12 dB gives a clean "noise sits low" first impression.
    // Dynamic range (68 dB grid, 60 dB waterfall) is unchanged.
    m_refLevel       = readFloat(QStringLiteral("DisplayGridMax"), -48.0f);
    m_dynamicRange   = readFloat(QStringLiteral("DisplayGridMax"), -48.0f)
                     - readFloat(QStringLiteral("DisplayGridMin"), -116.0f);
    m_spectrumFrac   = readFloat(QStringLiteral("DisplaySpectrumFrac"), 0.40f);

    // Phase 3G-12: persist the spectrum zoom level (visible bandwidth)
    // across app restarts. Center frequency is persisted indirectly via
    // SliceModel (slice frequency), so only bandwidth needs its own key.
    // Default 192000 Hz = 192 kHz matches the P1 base sample rate.
    m_bandwidthHz    = static_cast<double>(
                          readFloat(QStringLiteral("DisplayBandwidth"), 192000.0f));
    m_wfColorGain    = readInt(QStringLiteral("DisplayWfColorGain"), 45);
    m_wfBlackLevel   = readInt(QStringLiteral("DisplayWfBlackLevel"), 104);
    m_wfHighThreshold = readFloat(QStringLiteral("DisplayWfHighLevel"), -62.0f);
    m_wfLowThreshold = readFloat(QStringLiteral("DisplayWfLowLevel"), -122.0f);
    m_fillAlpha      = readFloat(QStringLiteral("DisplayFftFillAlpha"), 0.70f);
    m_panFill        = s.value(settingsKey(QStringLiteral("DisplayPanFill"), m_panIndex),
                               QStringLiteral("True")).toString() == QStringLiteral("True");

    m_ctunEnabled    = s.value(settingsKey(QStringLiteral("DisplayCtunEnabled"), m_panIndex),
                               QStringLiteral("True")).toString() == QStringLiteral("True");

    int scheme = readInt(QStringLiteral("DisplayWfColorScheme"), 0);
    m_wfColorScheme = static_cast<WfColorScheme>(qBound(0, scheme,
                          static_cast<int>(WfColorScheme::Count) - 1));

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
    const bool peakOn = s.value(settingsKey(QStringLiteral("DisplayPeakHoldEnabled"), m_panIndex),
                                QStringLiteral("False")).toString() == QStringLiteral("True");
    const bool gradOn = s.value(settingsKey(QStringLiteral("DisplayGradientEnabled"), m_panIndex),
                                QStringLiteral("False")).toString() == QStringLiteral("True");
    m_gradientEnabled = gradOn;
    // Delay the peak hold enable path until the timer infra is ready.
    if (peakOn) {
        setPeakHoldEnabled(true);
    }

    // Tasks 2.5 / 2.6 — Active Peak Hold + Peak Blobs persisted state.
    // SpectrumPeaksPage owns the UI but the renderer needs the persisted
    // values at app-start time. Without this load the peak features stay at
    // PeakBlobDetector / ActivePeakHoldTrace defaults until the user opens
    // Setup → Display → Spectrum Peaks even when the on-disk toggles are on.
    {
        // Active Peak Hold
        const bool aphOn = s.value(QStringLiteral("DisplayActivePeakHoldEnabled"),
                                   QStringLiteral("False")).toString() == QStringLiteral("True");
        const int aphDur = s.value(QStringLiteral("DisplayActivePeakHoldDurationMs"),
                                   QStringLiteral("2000")).toInt();
        const int aphDrop = s.value(QStringLiteral("DisplayActivePeakHoldDropDbPerSec"),
                                    QStringLiteral("6")).toInt();
        const bool aphFill = s.value(QStringLiteral("DisplayActivePeakHoldFill"),
                                     QStringLiteral("False")).toString() == QStringLiteral("True");
        const bool aphOnTx = s.value(QStringLiteral("DisplayActivePeakHoldOnTx"),
                                     QStringLiteral("False")).toString() == QStringLiteral("True");
        m_activePeakHold.setDurationMs(qBound(100, aphDur, 60000));
        m_activePeakHold.setDropDbPerSec(qBound(0.1, static_cast<double>(aphDrop), 120.0));
        m_activePeakHold.setFill(aphFill);
        m_activePeakHold.setOnTx(aphOnTx);
        if (aphOn) {
            m_activePeakHold.setEnabled(true);
        }
        // NereusSDR-original — distinct peak trace colour so it stays visible
        // when the data-line colour is changed (e.g. Smooth Defaults paints
        // the live trace pure white). Default gold (#FFD700FF).
        m_activePeakHoldColor = ColorSwatchButton::colorFromHex(
            s.value(QStringLiteral("DisplayActivePeakHoldColor"),
                    QStringLiteral("#FFD700FF")).toString());

        // Peak Blobs — NereusSDR ships disabled by default (deviation from
        // Thetis Display.cs:4395 [v2.10.3.13] m_bPeakBlobMaximums = true).
        const bool blobOn = s.value(QStringLiteral("DisplayPeakBlobsEnabled"),
                                    QStringLiteral("False")).toString() == QStringLiteral("True");
        const int blobCount = s.value(QStringLiteral("DisplayPeakBlobsCount"),
                                      QStringLiteral("3")).toInt();
        const bool blobInside = s.value(QStringLiteral("DisplayPeakBlobsInsideFilterOnly"),
                                        QStringLiteral("False")).toString() == QStringLiteral("True");
        const bool blobHold = s.value(QStringLiteral("DisplayPeakBlobsHoldEnabled"),
                                      QStringLiteral("False")).toString() == QStringLiteral("True");
        const int blobHoldMs = s.value(QStringLiteral("DisplayPeakBlobsHoldMs"),
                                       QStringLiteral("500")).toInt();
        const bool blobHoldDrop = s.value(QStringLiteral("DisplayPeakBlobsHoldDrop"),
                                          QStringLiteral("False")).toString() == QStringLiteral("True");
        const int blobFall = s.value(QStringLiteral("DisplayPeakBlobsFallDbPerSec"),
                                     QStringLiteral("6")).toInt();
        m_peakBlobs.setCount(qMax(1, blobCount));
        m_peakBlobs.setInsideFilterOnly(blobInside);
        m_peakBlobs.setHoldEnabled(blobHold);
        m_peakBlobs.setHoldMs(blobHoldMs);
        m_peakBlobs.setHoldDrop(blobHoldDrop);
        m_peakBlobs.setFallDbPerSec(static_cast<double>(blobFall));
        if (blobOn) {
            m_peakBlobs.setEnabled(true);
        }

        // Persisted format is "#RRGGBBAA" via ColorSwatchButton::colorToHex;
        // use the matching colorFromHex helper so alpha lands correctly.
        m_peakBlobColor = ColorSwatchButton::colorFromHex(
            s.value(QStringLiteral("DisplayPeakBlobColor"),
                    QStringLiteral("#FF4500FF")).toString());
        m_peakBlobTextColor = ColorSwatchButton::colorFromHex(
            s.value(QStringLiteral("DisplayPeakBlobTextColor"),
                    QStringLiteral("#7FFF00FF")).toString());
    }

    // Phase 3G-8 commit 4: waterfall renderer state.
    m_wfAgcEnabled = s.value(settingsKey(QStringLiteral("DisplayWfAgc"), m_panIndex),
                             QStringLiteral("True")).toString() == QStringLiteral("True");
    // Task 2.8: NF-AGC settings (DisplayWfReverseScroll key intentionally not
    // read here — W5 removed; key migration handled in Task 5.1).
    m_wfNfAgcEnabled = s.value(settingsKey(QStringLiteral("WaterfallNFAGCEnabled"), m_panIndex),
                               QStringLiteral("False")).toString() == QStringLiteral("True");
    m_wfNfAgcOffsetDb = readInt(QStringLiteral("WaterfallAGCOffsetDb"), 0);
    m_wfStopOnTx = s.value(settingsKey(QStringLiteral("WaterfallStopOnTx"), m_panIndex),
                           QStringLiteral("False")).toString() == QStringLiteral("True");
    m_wfOpacity          = readInt(QStringLiteral("DisplayWfOpacity"), 100);
    m_wfUpdatePeriodMs   = readInt(QStringLiteral("DisplayWfUpdatePeriodMs"), 30);

    // Sub-epic E: scrollback depth (default 20 min, range 60s..20min).
    m_waterfallHistoryMs = s.value(
        settingsKey(QStringLiteral("DisplayWaterfallHistoryMs"), m_panIndex),
        QString::number(static_cast<qint64>(kDefaultWaterfallHistoryMs))
    ).toLongLong();
    m_wfUseSpectrumMinMax = s.value(settingsKey(QStringLiteral("DisplayWfUseSpectrumMinMax"), m_panIndex),
                                    QStringLiteral("False")).toString() == QStringLiteral("True");
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
    m_showRxFilterOnWaterfall = s.value(settingsKey(QStringLiteral("DisplayShowRxFilterOnWaterfall"), m_panIndex),
                                        QStringLiteral("False")).toString() == QStringLiteral("True");
    // Default True — same rationale as DisplayDrawTxFilter above: the TX
    // overlay should be visible during MOX out of the box.  The waterfall
    // column is independently MOX-gated at the call site.
    m_showTxFilterOnRxWaterfall = s.value(settingsKey(QStringLiteral("DisplayShowTxFilterOnRxWaterfall"), m_panIndex),
                                          QStringLiteral("True")).toString() == QStringLiteral("True");
    // Plan 4 D9 (Cluster E): persist DrawTXFilter flag.
    // From Thetis display.cs:2481 [v2.10.3.13]: DrawTXFilter property.
    // Until the Setup → Display TX Display page has a wired checkbox,
    // the user can set this in AppSettings XML directly.
    // Default True: pairs with the MOX-gated TX overlay paint at the call
    // sites (m_txFilterVisible && m_moxOverlay).  Without this, MOX flips
    // m_moxOverlay true, the RX cyan correctly hides, but the TX orange
    // never paints — the panadapter goes "clear" during TX/TUNE.
    m_txFilterVisible = s.value(settingsKey(QStringLiteral("DisplayDrawTxFilter"), m_panIndex),
                                QStringLiteral("True")).toString() == QStringLiteral("True");
    m_showRxZeroLineOnWaterfall = s.value(settingsKey(QStringLiteral("DisplayShowRxZeroLine"), m_panIndex),
                                          QStringLiteral("False")).toString() == QStringLiteral("True");
    m_showTxZeroLineOnWaterfall = s.value(settingsKey(QStringLiteral("DisplayShowTxZeroLine"), m_panIndex),
                                          QStringLiteral("False")).toString() == QStringLiteral("True");

    // Phase 3G-8 commit 5: grid / scales state.
    m_gridEnabled = s.value(settingsKey(QStringLiteral("DisplayGridEnabled"), m_panIndex),
                            QStringLiteral("True")).toString() == QStringLiteral("True");
    m_showZeroLine = s.value(settingsKey(QStringLiteral("DisplayShowZeroLine"), m_panIndex),
                             QStringLiteral("False")).toString() == QStringLiteral("True");
    m_showFps = s.value(settingsKey(QStringLiteral("DisplayShowFps"), m_panIndex),
                        QStringLiteral("False")).toString() == QStringLiteral("True");
    // B8 Task 21: cursor frequency readout persists across restarts.
    m_showCursorFreq = s.value(settingsKey(QStringLiteral("DisplayShowCursorFreq"), m_panIndex),
                               QStringLiteral("True")).toString() == QStringLiteral("True");
    m_dbmScaleVisible = s.value(settingsKey(QStringLiteral("DisplayDbmScaleVisible"), m_panIndex),
                                QStringLiteral("True")).toString() == QStringLiteral("True");
    m_bandPlanFontSize = s.value(QStringLiteral("BandPlanFontSize"),
                                 QStringLiteral("6")).toInt();
    const int alignRaw = readInt(QStringLiteral("DisplayFreqLabelAlign"),
                                 static_cast<int>(FreqLabelAlign::Center));
    m_freqLabelAlign = static_cast<FreqLabelAlign>(qBound(0, alignRaw,
                           static_cast<int>(FreqLabelAlign::Count) - 1));

    auto readColor = [&](const QString& key, const QColor& def) -> QColor {
        const QString hex = s.value(settingsKey(key, m_panIndex)).toString();
        if (hex.isEmpty()) { return def; }
        QColor c = QColor::fromString(hex);
        return c.isValid() ? c : def;
    };
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

    writeFloat(QStringLiteral("DisplayGridMax"), m_refLevel);
    writeFloat(QStringLiteral("DisplayGridMin"), m_refLevel - m_dynamicRange);
    writeFloat(QStringLiteral("DisplaySpectrumFrac"), m_spectrumFrac);
    writeFloat(QStringLiteral("DisplayBandwidth"), static_cast<float>(m_bandwidthHz));  // Phase 3G-12
    writeInt(QStringLiteral("DisplayWfColorGain"), m_wfColorGain);
    writeInt(QStringLiteral("DisplayWfBlackLevel"), m_wfBlackLevel);
    writeFloat(QStringLiteral("DisplayWfHighLevel"), m_wfHighThreshold);
    writeFloat(QStringLiteral("DisplayWfLowLevel"), m_wfLowThreshold);
    writeFloat(QStringLiteral("DisplayFftFillAlpha"), m_fillAlpha);
    s.setValue(settingsKey(QStringLiteral("DisplayPanFill"), m_panIndex),
              m_panFill ? QStringLiteral("True") : QStringLiteral("False"));
    writeInt(QStringLiteral("DisplayWfColorScheme"), static_cast<int>(m_wfColorScheme));
    s.setValue(settingsKey(QStringLiteral("DisplayCtunEnabled"), m_panIndex),
              m_ctunEnabled ? QStringLiteral("True") : QStringLiteral("False"));

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
    s.setValue(QStringLiteral("BandPlanFontSize"),
               QString::number(m_bandPlanFontSize));
    writeInt(QStringLiteral("DisplayFreqLabelAlign"), static_cast<int>(m_freqLabelAlign));

    auto writeColor = [&](const QString& key, const QColor& c) {
        s.setValue(settingsKey(key, m_panIndex), c.name(QColor::HexArgb));
    };
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
    s.setValue(QStringLiteral("DisplayAdjustGridMinToNoiseFloor"),
               m_adjustGridMinToNF ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("DisplayNFOffsetGridFollow"),
               QString::number(m_nfOffsetGridFollow));
    s.setValue(QStringLiteral("DisplayMaintainNFAdjustDelta"),
               m_maintainNFAdjustDelta ? QStringLiteral("True") : QStringLiteral("False"));
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

    m_centerHz = centerHz;
    m_bandwidthHz = bandwidthHz;
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
    }
}

void SpectrumWidget::setSampleRate(double hz)
{
    if (!qFuzzyCompare(m_sampleRateHz, hz)) {
        m_sampleRateHz = hz;
        update();
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
    update();
    // Note: callers that invoke setDbmRange deliberately (Copy button, user drag)
    // schedule their own save. The NF-aware grid onNoiseFloorChanged() avoids
    // scheduling saves because it fires at 500ms cadence.
}

void SpectrumWidget::setWfColorScheme(WfColorScheme scheme)
{
    m_wfColorScheme = scheme;
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setWfColorGain(int gain)
{
    if (m_wfColorGain == gain) { return; }
    m_wfColorGain = gain;
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setWfBlackLevel(int level)
{
    if (m_wfBlackLevel == level) { return; }
    m_wfBlackLevel = level;
    scheduleSettingsSave();
    update();
}

// ---- Phase 3G-8 commit 3 setters ----

void SpectrumWidget::setAverageMode(AverageMode m)
{
    if (m_averageMode == m) {
        return;
    }
    m_averageMode = m;
    // Force a fresh baseline for the new mode so a switch doesn't
    // carry stale averager state forward.  Mirrors WDSP analyzer.c:
    // SetDisplayAverageMode at :1854 [v2.10.3.13] which re-init's the
    // av_sum / av_buff accumulators on mode change.
    m_spectrumAvenger.clear();
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
    m_spectrumDetector = d;
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setSpectrumAveraging(SpectrumAveraging a)
{
    if (m_spectrumAveraging == a) { return; }
    m_spectrumAveraging = a;
    // Reset avenger state so mode change doesn't carry stale history.
    // From WDSP analyzer.c:1854 [v2.10.3.13] SetDisplayAverageMode re-init.
    m_spectrumAvenger.clear();
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
    m_waterfallAvenger.clear();
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

    auto computeAlpha = [fps](int timeMs) -> float {
        const double tauSec = qMax(timeMs, 1) / 1000.0;
        // α = exp(-1 / (fps × τ)). Matches Thetis specHPSDR.cs:358 / :374.
        const double a = std::exp(-1.0 / (static_cast<double>(fps) * tauSec));
        return static_cast<float>(qBound(0.0, a, 1.0));
    };
    m_spectrumAverageAlpha  = computeAlpha(m_spectrumAverageTimeMs);
    m_waterfallAverageAlpha = computeAlpha(m_waterfallAverageTimeMs);
}

void SpectrumWidget::setSpectrumAverageTimeMs(int ms)
{
    ms = qBound(10, ms, 9999);
    if (m_spectrumAverageTimeMs == ms) {
        return;
    }
    m_spectrumAverageTimeMs = ms;
    recomputeAverageAlphas();
    scheduleSettingsSave();
}

void SpectrumWidget::setWaterfallAverageTimeMs(int ms)
{
    ms = qBound(10, ms, 9999);
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

// ---- Active Peak Hold trace setters (Task 2.5) ----
// From Thetis display.cs m_bActivePeakHold / groupBoxTS21 [v2.10.3.13].

void SpectrumWidget::setActivePeakHoldEnabled(bool on)
{
    m_activePeakHold.setEnabled(on);
    if (!on) {
        m_activePeakHold.clear();
    }
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
    update();
}

void SpectrumWidget::setPeakBlobTextColor(const QColor& c)
{
    m_peakBlobTextColor = c;
    update();
}

void SpectrumWidget::setPanFillEnabled(bool on)
{
    if (m_panFill == on) {
        return;
    }
    m_panFill = on;
    scheduleSettingsSave();
    update();  // vertex gen is next frame; render pass checks m_panFill
}

void SpectrumWidget::setFillAlpha(float a)
{
    a = qBound(0.0f, a, 1.0f);
    if (qFuzzyCompare(m_fillAlpha, a)) {
        return;
    }
    m_fillAlpha = a;
    scheduleSettingsSave();
    update();
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
    db = qBound(-30.0f, db, 30.0f);
    if (qFuzzyCompare(m_dbmCalOffset, db)) {
        return;
    }
    m_dbmCalOffset = db;
    scheduleSettingsSave();
    markOverlayDirty();  // dBm scale strip labels shift
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
    const int fftSz = m_fullLinearBins.isEmpty() ? 4096
                                                 : m_fullLinearBins.size();
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
}

void SpectrumWidget::setShowNoiseFloorPosition(OverlayPosition pos)
{
    if (m_noiseFloorPosition == pos) { return; }
    m_noiseFloorPosition = pos;
    if (m_showNoiseFloor) { markOverlayDirty(); }
}

// From Thetis display.cs:5763-5773 [v2.10.3.13] _fNFshiftDBM setter:
//   if (t < -12f) t = -12f;
//   if (t > 12f) t = 12f;
//   _fNFshiftDBM = t;
void SpectrumWidget::setNFShiftDbm(float db)
{
    const float clamped = qBound(-12.0f, db, 12.0f);
    if (m_nfShiftDbm == clamped) { return; }
    m_nfShiftDbm = clamped;
    if (m_showNoiseFloor) { markOverlayDirty(); }
}

void SpectrumWidget::setNoiseFloorColor(const QColor& c)
{
    if (!c.isValid() || m_noiseFloorColor == c) { return; }
    m_noiseFloorColor = c;
    if (m_showNoiseFloor) { markOverlayDirty(); }
}

void SpectrumWidget::setNoiseFloorTextColor(const QColor& c)
{
    if (!c.isValid() || m_noiseFloorTextColor == c) { return; }
    m_noiseFloorTextColor = c;
    if (m_showNoiseFloor) { markOverlayDirty(); }
}

void SpectrumWidget::setNoiseFloorFastColor(const QColor& c)
{
    if (!c.isValid() || m_noiseFloorFastColor == c) { return; }
    m_noiseFloorFastColor = c;
    if (m_showNoiseFloor && m_noiseFloorFastAttack) { markOverlayDirty(); }
}

void SpectrumWidget::setNoiseFloorLineWidth(float w)
{
    const float clamped = qBound(1.0f, w, 5.0f);
    if (m_noiseFloorLineWidth == clamped) { return; }
    m_noiseFloorLineWidth = clamped;
    if (m_showNoiseFloor) { markOverlayDirty(); }
}

// From Thetis display.cs:917-927 [v2.10.3.13] FastAttackNoiseFloorRX1 setter.
// Also records m_nfLastFastAttackMs — Thetis _fLastFastAttackEnabledTimeRX1
// (display.cs:4641) — so processNoiseFloor's convergence-gated auto-clear
// (display.cs:5904-5908) can measure elapsed time since the last trigger.
void SpectrumWidget::setNoiseFloorFastAttack(bool on)
{
    if (on) {
        // Stamp every trigger; consecutive band/freq/MOX events all reset
        // the elapsed clock so the gray window covers the full settling
        // period regardless of how many triggers fired.
        m_nfLastFastAttackMs = QDateTime::currentMSecsSinceEpoch();
    }
    if (m_noiseFloorFastAttack == on) { return; }
    m_noiseFloorFastAttack = on;
    if (m_showNoiseFloor) { markOverlayDirty(); }
}

// Source-first port of Thetis processNoiseFloor — display.cs:5866-5912 [v2.10.3.13].
// Iterates m_renderedPixels to count + linear-sum bins below the previous
// frame's estimate, then updates m_nfFftBinAverage (per-frame avg) and
// m_nfLerpAverage (smoothed).  Mirrors the per-pixel accumulator that lives
// in Thetis's render loop at display.cs:5253-5258 (averageSum += 10^(dB/10),
// averageCount++ when max_copy < currentAverage), folded into one helper here
// because NereusSDR's renderer doesn't share the averaging loop.
void SpectrumWidget::processNoiseFloor()
{
    const int width = m_renderedPixels.size();
    if (width <= 0) { return; }

    // Per-pixel accumulator — Thetis display.cs:5253-5258 [v2.10.3.13]:
    //   if (!mox && max_copy < currentAverage) {
    //       averageSum += fastPow10Raw(max_copy);
    //       averageCount++;
    //   }
    // currentAverage = previous-frame fftBinAverage (the running estimate);
    // bins below it are the "quiet" ones we want to characterise.
    const float currentAverage = m_nfFftBinAverage;
    double averageSum = 0.0;
    int    averageCount = 0;
    for (int i = 0; i < width; ++i) {
        const float dB = m_renderedPixels[i];
        if (dB < currentAverage) {
            averageSum += std::pow(10.0, static_cast<double>(dB) / 10.0);
            averageCount++;
        }
    }

    const int fps = qMax(1, 1000 / qMax(1, m_displayTimer.interval()));
    // Thetis default _NFsensitivity=3, clamp [0,19] (display.cs:5775+5783).
    // Match the formula exactly: int truncation of (width * sens / 20).
    const int requireSamples = qMax(1,
        (width * m_nfSensitivity) / 20);

    // Per-frame fftBinAverage update — display.cs:5883-5896.
    if (averageCount >= requireSamples) {
        const float linearAverage =
            static_cast<float>(averageSum / static_cast<double>(averageCount));
        const float oldLinear = std::pow(10.0f, m_nfFftBinAverage / 10.0f);
        const float newLinear = (linearAverage + oldLinear) * 0.5f;
        m_nfFftBinAverage = 10.0f *
            std::log10(static_cast<double>(newLinear) + 1e-60);
    } else {
        // Not enough quiet bins (signal-dense band, or estimate stuck below
        // the true floor).  Drift up by 1 dB/frame (3 dB in fast-attack to
        // re-acquire faster).  display.cs:5893.
        m_nfFftBinAverage += m_noiseFloorFastAttack ? 3.0f : 1.0f;
    }
    m_nfFftBinAverage = qBound(-200.0f, m_nfFftBinAverage, 200.0f);

    // Lerp smoothing — display.cs:5898-5902.
    int framesInAttack = m_noiseFloorFastAttack ? 0 :
        static_cast<int>((static_cast<float>(fps) / 1000.0f) * m_nfAttackTimeMs);
    framesInAttack += 1;
    const float difference = m_nfLerpAverage - m_nfFftBinAverage;
    m_nfLerpAverage -= difference / static_cast<float>(framesInAttack);

    // Fast-attack convergence-gated auto-clear — display.cs:5904-5908:
    //   if (fastAttack && abs(fft - lerp) < 1.0) {
    //       float tmpDelay = Math.Max(1000, fftFillTime + ...);
    //       if (elapsed > tmpDelay) fastAttack = false;
    //   }
    // Clears the gray flag once the smoothed estimate has caught up to the
    // per-frame estimate AND at least 1 second has passed since the trigger.
    if (m_noiseFloorFastAttack &&
        std::abs(m_nfFftBinAverage - m_nfLerpAverage) < 1.0f)
    {
        const qint64 elapsed =
            QDateTime::currentMSecsSinceEpoch() - m_nfLastFastAttackMs;
        if (elapsed > kFastAttackMinMs) {
            m_noiseFloorFastAttack = false;
            if (m_showNoiseFloor) { markOverlayDirty(); }
        }
    }
}

// ---- NF-aware grid (Task 2.9) ----
// From Thetis setup.cs:24202-24213 [v2.10.3.13]
// — RX1 scope dropped; NereusSDR applies as global panadapter default
//   with per-pan override via ContainerSettings dialog (3G-6 pattern).

void SpectrumWidget::setAdjustGridMinToNoiseFloor(bool on)
{
    if (m_adjustGridMinToNF == on) { return; }
    m_adjustGridMinToNF = on;
    scheduleSettingsSave();
}

void SpectrumWidget::setNFOffsetGridFollow(int db)
{
    db = qBound(-60, db, 60);
    if (m_nfOffsetGridFollow == db) { return; }
    m_nfOffsetGridFollow = db;
    scheduleSettingsSave();
}

void SpectrumWidget::setMaintainNFAdjustDelta(bool on)
{
    if (m_maintainNFAdjustDelta == on) { return; }
    m_maintainNFAdjustDelta = on;
    scheduleSettingsSave();
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
}

// From Thetis console.cs:20073-20080 [v2.10.3.13] PeakTextDelay / timer_peak_text.
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
}

void SpectrumWidget::setPeakValuePosition(OverlayPosition pos)
{
    if (m_peakValuePosition == pos) { return; }
    m_peakValuePosition = pos;
    if (m_showPeakValueOverlay) { markOverlayDirty(); }
}

// From Thetis console.cs:20073-20080 [v2.10.3.13] PeakTextDelay default=500.
void SpectrumWidget::setPeakTextDelayMs(int ms)
{
    ms = qBound(50, ms, 10000);
    if (m_peakTextDelayMs == ms) { return; }
    m_peakTextDelayMs = ms;
    if (m_peakTextTimer && m_peakTextTimer->isActive()) {
        m_peakTextTimer->setInterval(ms);
    }
}

// From Thetis console.cs:20278 [v2.10.3.13] peak_text_color = Color.DodgerBlue.
void SpectrumWidget::setPeakValueColor(const QColor& c)
{
    if (!c.isValid() || m_peakValueColor == c) { return; }
    m_peakValueColor = c;
    if (m_showPeakValueOverlay) { markOverlayDirty(); }
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
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setWfLowThreshold(float dbm)
{
    if (qFuzzyCompare(m_wfLowThreshold, dbm)) { return; }
    m_wfLowThreshold = dbm;
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setWfAgcEnabled(bool on)
{
    if (m_wfAgcEnabled == on) { return; }
    m_wfAgcEnabled = on;
    m_wfAgcPrimed = false;
    scheduleSettingsSave();
    update();
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
    scheduleSettingsSave();
}

void SpectrumWidget::setWaterfallAGCOffsetDb(int db)
{
    db = qBound(-60, db, 60);
    if (m_wfNfAgcOffsetDb == db) { return; }
    m_wfNfAgcOffsetDb = db;
    scheduleSettingsSave();
}

// Task 2.8: Stop-on-TX — gate pushWaterfallRow() while TX is active.
void SpectrumWidget::setWaterfallStopOnTx(bool on)
{
    if (m_wfStopOnTx == on) { return; }
    m_wfStopOnTx = on;
    scheduleSettingsSave();
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
    update();
}

void SpectrumWidget::setWfAverageMode(AverageMode m)
{
    if (m_wfAverageMode == m) { return; }
    m_wfAverageMode = m;
    // Reset waterfall avenger on mode change (analyzer.c:1854 [v2.10.3.13]).
    m_waterfallAvenger.clear();
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
    Q_UNUSED(receiverId);
    if (binsLinear.isEmpty()) { return; }

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
        m_spectrumAvenger.clear();
        m_waterfallAvenger.clear();
    }

    m_fullLinearBins = binsLinear;
    m_fftWindowEnb   = qMax(windowEnb, 1e-9);

    // Display pixel count -- spectrum panel width minus dBm strip column.
    // Per Thetis Display.cs:4970 DrawPanadapterDX2D(int W, ...) signature
    // and :4993 nDecimatedWidth = W / m_nDecimation [v2.10.3.13].  Thetis
    // S16 display-side decimation (an additional W / N reduction) is a
    // Phase 2 follow-up; for now we use the full panel width.
    const int displayWidth = qMax(width() - effectiveStripW(), 800);

    // Visible bin slice -- CTUN zoom support.  visibleBinRange() maps the
    // current m_centerHz +/- m_bandwidthHz/2 window against m_ddcCenterHz
    // + m_sampleRateHz.  When zoomed out, slice == full FFT.
    auto [firstBin, lastBin] = visibleBinRange(binsLinear.size());
    const int sliceCount = lastBin - firstBin + 1;
    if (sliceCount <= 0) { return; }

    const double pixPerBin = static_cast<double>(displayWidth) / sliceCount;
    const double binPerPix = (pixPerBin > 0.0) ? 1.0 / pixPerBin : 1.0;
    const double invEnb    = 1.0 / m_fftWindowEnb;
    // dbmOffset folded into the avenger's power-domain scale so that
    // 10·log10(linear · scale) == 10·log10(linear) + dbmOffset, matching
    // FFTEngine.cpp:348 [v2.10.3.13] (binsDbm = 10·log10 + offset).
    const double dbmScale  = std::pow(10.0, dbmOffset / 10.0);

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

    const QVector<double> noCorrection;  // per-pixel sub-band gain compensation

    // --- Spectrum plane: detector -> avenger -> m_renderedPixels (dBm) ---
    if (m_displayLinearPixels.size() != displayWidth) {
        m_displayLinearPixels.resize(displayWidth);
    }
    if (m_spectrumAvenger.numPixels() != displayWidth) {
        m_spectrumAvenger.resize(displayWidth);
    }
    NereusSDR::applySpectrumDetector(m_spectrumDetector,
                                     sliceCount,
                                     displayWidth,
                                     pixPerBin,
                                     binPerPix,
                                     m_fullLinearBins.constData() + firstBin,
                                     m_displayLinearPixels.data(),
                                     invEnb,
                                     0.0,
                                     static_cast<double>(sliceCount),
                                     0.0);
    m_spectrumAvenger.apply(m_displayLinearPixels,
                            avengerMode(m_spectrumAveraging),
                            static_cast<double>(m_spectrumAverageAlpha),
                            dbmScale,
                            noCorrection,
                            false,
                            0.0,
                            m_renderedPixels);

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

    // --- Waterfall plane: own detector + avenger -> m_wfRenderedPixels ---
    // Thetis runs separate analyzer planes for spectrum and waterfall (see
    // ANALYZER_INFO[] in analyzer.c).  DetType + AvMode are independent.
    if (m_wfDisplayLinearPixels.size() != displayWidth) {
        m_wfDisplayLinearPixels.resize(displayWidth);
    }
    if (m_waterfallAvenger.numPixels() != displayWidth) {
        m_waterfallAvenger.resize(displayWidth);
    }
    NereusSDR::applySpectrumDetector(m_waterfallDetector,
                                     sliceCount,
                                     displayWidth,
                                     pixPerBin,
                                     binPerPix,
                                     m_fullLinearBins.constData() + firstBin,
                                     m_wfDisplayLinearPixels.data(),
                                     invEnb,
                                     0.0,
                                     static_cast<double>(sliceCount),
                                     0.0);
    m_waterfallAvenger.apply(m_wfDisplayLinearPixels,
                             avengerMode(m_waterfallAveraging),
                             static_cast<double>(m_waterfallAverageAlpha),
                             dbmScale,
                             noCorrection,
                             false,
                             0.0,
                             m_wfRenderedPixels);

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
    const int fps = (intervalMs > 0) ? qMax(1, 1000 / intervalMs) : 30;

    if (m_activePeakHold.enabled()) {
        if (m_activePeakHold.size() != m_renderedPixels.size()) {
            m_activePeakHold.resize(m_renderedPixels.size());
        }
        m_activePeakHold.update(m_renderedPixels);
        m_activePeakHold.tickFrame(fps);
    }

    // Peak Blob detector -- pixel-space local maxima with hold/decay.
    // Filter-passband math becomes pixel-space: visible window is
    // m_centerHz +/- m_bandwidthHz/2 mapped across displayWidth pixels.
    // From Thetis Display.cs:5453-5508 [v2.10.3.13].
    if (m_peakBlobs.enabled() && !m_renderedPixels.isEmpty()) {
        const int n = m_renderedPixels.size();
        int filterLowPx  = 0;
        int filterHighPx = n - 1;
        if (m_peakBlobs.insideOnly() && m_bandwidthHz > 0.0) {
            const double leftHz  = m_centerHz - m_bandwidthHz / 2.0;
            const double pxWidth = m_bandwidthHz / static_cast<double>(n);
            const double loHz = m_vfoHz + m_filterLowHz;
            const double hiHz = m_vfoHz + m_filterHighHz;
            filterLowPx  = qBound(0,
                static_cast<int>(std::floor((loHz - leftHz) / pxWidth)),
                n - 1);
            filterHighPx = qBound(0,
                static_cast<int>(std::ceil((hiHz - leftHz) / pxWidth)),
                n - 1);
        }
        m_peakBlobs.update(m_renderedPixels, filterLowPx, filterHighPx);
        m_peakBlobs.tickFrame(fps, intervalMs > 0 ? intervalMs : 33);
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

    // Per-frame force-dirty for any overlay that updates every spectrum
    // frame (APH trace decay, peak blobs, NF lerp position).  GPU-only
    // optimization — CPU paint path repaints the overlay every frame
    // anyway, so the dirty flag has no analogue.
#ifdef NEREUS_GPU_SPECTRUM
    if (m_activePeakHold.enabled() || m_peakBlobs.enabled()
        || m_showNoiseFloor) {
        m_overlayStaticDirty = true;
    }
#endif

    // Push display-pixel waterfall row -- waterfall AGC + NF-AGC +
    // threshold compute now operate on display pixels per Thetis
    // Display.cs:6713-6738 [v2.10.3.13] (waterfall_data[i] indexed by
    // pixel).
    pushWaterfallRow(m_wfRenderedPixels);
    m_hasNewSpectrum = true;
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
    int w = width();
    int h = height();
#ifdef NEREUS_GPU_SPECTRUM
    // GPU mode: waterfall clips at strip border (strip is in overlay on right edge)
    int wfW = w - effectiveStripW();
#else
    int wfW = w - effectiveStripW();
#endif
    int wfH = static_cast<int>(h * (1.0f - m_spectrumFrac)) - kFreqScaleH - kDividerH;
    if (wfW > 0 && wfH > 0 && (m_waterfall.isNull() ||
        m_waterfall.width() != wfW || m_waterfall.height() != wfH)) {
        m_waterfall = QImage(wfW, wfH, QImage::Format_RGB32);
        m_waterfall.fill(QColor(0x0f, 0x0f, 0x1a));
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

    int w = width();
    int h = height();
    int specH = static_cast<int>(h * m_spectrumFrac);
    int wfTop = specH + kDividerH;
    int wfH = h - wfTop - kFreqScaleH;

    // Spectrum area (left of dBm strip — strip lives on the right edge per
    // AetherSDR convention). From AetherSDR SpectrumWidget.cpp:4858 [@0cd4559]
    QRect specRect(0, 0, w - effectiveStripW(), specH);
    // Waterfall area
    QRect wfRect(0, wfTop, w - effectiveStripW(), wfH);
    // Frequency scale bar
    QRect freqRect(0, h - kFreqScaleH, w - effectiveStripW(), kFreqScaleH);

    // Draw divider bar between spectrum and waterfall
    p.fillRect(0, specH, w, kDividerH, QColor(0x30, 0x40, 0x50));

    // Draw components
    drawGrid(p, specRect);
    drawSpectrum(p, specRect);
    drawWaterfall(p, wfRect);
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
        drawDbmScale(p, QRect(0, 0, w, specH));
    }
    drawBandPlan(p, specRect);
    // Sub-epic E: time-scale + LIVE button on the right edge of the
    // waterfall area (always painted; widens automatically when paused).
    // Use a full-width wfRect (not the clipped `wfRect` at line 1141) so the
    // strip lands in the same right-edge column as the dBm scale strip.
    const QRect wfRectFull(0, wfRect.top(), width(), wfRect.height());
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
}

// ---- Spectrum trace drawing ----
// Phase 3G-8 commit 3: honors m_lineWidth, m_gradientEnabled,
// m_peakHoldEnabled. See drawSpectrum() call site in paintEvent for
// the QPainter fallback path. GPU path uses its own vertex generation
// in renderGpuFrame() — line width and gradient wire-up there lands
// in commit 5.
void SpectrumWidget::drawSpectrum(QPainter& p, const QRect& specRect)
{
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
    QVector<QPointF> points(n);
    for (int j = 0; j < n; ++j) {
        const float x = specRect.left() + static_cast<float>(j) * xStep;
        const float y = dbmToYf(m_renderedPixels[j], specRect);
        points[j] = QPointF(x, y);
    }

    // Fill under the trace (if enabled).
    // From AetherSDR: fill alpha 0.70, cyan color.
    if (m_panFill) {
        QPainterPath fillPath;
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
        QVector<QPointF> peakPoints(n);
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
    if (m_activePeakHold.enabled() && m_activePeakHold.size() == n) {
        paintActivePeakHoldTrace(p, specRect);
    }

    // Peak Blobs render pass.  Drawn on top of the live trace line.
    // From Thetis Display.cs:5453-5508 [v2.10.3.13].
    if (m_peakBlobs.enabled() && !m_peakBlobs.blobs().isEmpty()) {
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
    QVector<QPointF> peakPoints(n);
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
        QPainterPath fillPath;
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
        p.setPen(QPen(m_peakBlobColor, 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(x, y), 3, 3);

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
    const float lerp   = m_nfLerpAverage  + m_nfShiftDbm;
    const float actual = m_nfFftBinAverage + m_nfShiftDbm;

    const int yPLerp   = dbmToY(lerp,   specRect);
    const int yPActual = dbmToY(actual, specRect);

    // Fast-attack colour swap — Thetis display.cs:5431-5432 [v2.10.3.13]:
    //   nf_colour      = bFast ? m_bDX2_Gray : m_bDX2_noisefloor;
    //   nf_colour_text = bFast ? m_bDX2_Gray : m_bDX2_noisefloor_text;
    const QColor lineCol = m_noiseFloorFastAttack
        ? m_noiseFloorFastColor : m_noiseFloorColor;
    const QColor textCol = m_noiseFloorFastAttack
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

        QRect textRect(x - 30, r.top() + 2, 60, r.height() - 2);
        p.drawText(textRect, baseFlags, label);
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

    for (float dbm = firstLabel; dbm <= m_refLevel; dbm += stepDb) {
        // Route through dbmToY() so m_dbmCalOffset is applied consistently with
        // the grid/trace/peak-hold paths — otherwise a non-zero cal offset would
        // drift strip ticks off the actual grid lines.
        const int y = dbmToY(dbm, specRect);
        if (y < labelTop || y > specRect.bottom() - 5) continue;

        // Tick mark
        p.setPen(QColor(0x50, 0x70, 0x80));
        p.drawLine(strip.left(), y, strip.left() + 4, y);

        // Label
        const QString label = QString::number(static_cast<int>(dbm));
        p.setPen(QColor(0x80, 0xa0, 0xb0));
        p.drawText(strip.left() + 6, y + fm.ascent() / 2, label);
    }
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

            QString lowestClass;
            if      (lic.contains(QLatin1Char('T'))) { lowestClass = QStringLiteral("Tech"); }
            else if (lic.contains(QLatin1Char('G'))) { lowestClass = QStringLiteral("General"); }
            else if (lic == QLatin1String("E"))      { lowestClass = QStringLiteral("Extra"); }

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
    if (binCount <= 0 || m_sampleRateHz <= 0.0) {
        return {0, -1};  // empty range — callers compute count = 0
    }

    double binWidth = m_sampleRateHz / binCount;
    double fftLowHz = m_ddcCenterHz - m_sampleRateHz / 2.0;

    double displayLowHz  = m_centerHz - m_bandwidthHz / 2.0;
    double displayHighHz = m_centerHz + m_bandwidthHz / 2.0;

    int firstBin = static_cast<int>(std::floor((displayLowHz - fftLowHz) / binWidth));
    int lastBin  = static_cast<int>(std::ceil((displayHighHz - fftLowHz) / binWidth));

    firstBin = std::clamp(firstBin, 0, binCount - 1);
    lastBin  = std::clamp(lastBin, 0, binCount - 1);

    if (firstBin > lastBin) {
        firstBin = lastBin;
    }

    return {firstBin, lastBin};
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
    if (!m_dispNormalize) { return 0.0f; }
    const double bw = binWidthHz();
    if (bw <= 0.0) { return 0.0f; }
    return -10.0f * std::log10(static_cast<float>(bw));
}

int SpectrumWidget::dbmToY(float dbm, const QRect& r) const
{
    // Phase 3G-8: apply display calibration offset before mapping to Y.
    // From Thetis display.cs:1372 Display.RX1DisplayCalOffset.
    const float calibrated = dbm + m_dbmCalOffset + normalizeShiftDb();
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
    const float calibrated = dbm + m_dbmCalOffset + normalizeShiftDb();
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
        newHistory = QImage(desiredSize, QImage::Format_RGB32);
        newHistory.fill(Qt::black);
        m_wfHistoryTimestamps = QVector<qint64>(desiredSize.height(), 0);
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
    if (m_waterfall.isNull()) {
        return;
    }

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
        std::memcpy(dst, src, rowWidthBytes);
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
    m_wfHistoryWriteRow = 0;
    m_wfHistoryRowCount = 0;
    m_wfHistoryOffsetRows = 0;
    m_wfLive = true;
    markOverlayDirty();
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

    // Task 2.8: Stop-on-TX -- skip if TX active and feature enabled.
    if (m_wfStopOnTx && m_activePeakHold.txActive()) {
        return;
    }

    // Rate-limit per configured update period.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_wfUpdatePeriodMs > 0 && m_wfLastPushMs != 0 &&
        now - m_wfLastPushMs < m_wfUpdatePeriodMs) {
        return;
    }
    m_wfLastPushMs = now;

    const int n = wfPixelsDbm.size();

    // AGC: track a slow envelope of display-pixel min/max and bias the
    // effective thresholds toward it. Simple one-pole follower.
    // Phase 3G-9c: skipped when Clarity is actively driving thresholds.
    if (m_wfAgcEnabled && !m_clarityActive) {
        float mn = wfPixelsDbm[0];
        float mx = mn;
        for (int i = 1; i < n; ++i) {
            const float v = wfPixelsDbm[i];
            if (v < mn) { mn = v; }
            if (v > mx) { mx = v; }
        }
        if (!m_wfAgcPrimed) {
            m_wfAgcRunMin = mn;
            m_wfAgcRunMax = mx;
            m_wfAgcPrimed = true;
        } else {
            constexpr float kAgcAlpha = 0.05f;
            m_wfAgcRunMin = kAgcAlpha * mn + (1.0f - kAgcAlpha) * m_wfAgcRunMin;
            m_wfAgcRunMax = kAgcAlpha * mx + (1.0f - kAgcAlpha) * m_wfAgcRunMax;
        }
        // Phase 3G-9b: 12 dB margin for palette breathing room.
        const float margin = 12.0f;
        m_wfLowThreshold  = m_wfAgcRunMin - margin;
        m_wfHighThreshold = m_wfAgcRunMax + margin;
    } else if (m_wfUseSpectrumMinMax) {
        m_wfHighThreshold = m_refLevel;
        m_wfLowThreshold  = m_refLevel - m_dynamicRange;
    }

    // Task 2.8: NF-AGC -- override thresholds from 10th-percentile noise
    // floor + configured offset.  Takes priority over spectrum-min-max
    // but yields to the existing legacy AGC (which is a different
    // feature).  Sort over the full pixel array for the percentile.
    if (m_wfNfAgcEnabled && !m_clarityActive) {
        QVector<float> sorted = wfPixelsDbm;
        std::sort(sorted.begin(), sorted.end());
        const float nf = sorted[qBound(0, sorted.size() / 10, sorted.size() - 1)];
        const float offsetF = static_cast<float>(m_wfNfAgcOffsetDb);
        m_wfLowThreshold  = nf + offsetF;
        m_wfHighThreshold = m_wfLowThreshold + 60.0f;
    }

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
        scanline[x] = dbmToRgb(wfPixelsDbm[srcPx]);
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

// ---- dBm to waterfall color ----
// Porting from Thetis display.cs:6826-6954 — waterfall color mapping.
// Thetis uses low_threshold and high_threshold (dBm) directly:
//   if (data <= low_threshold) → low_color (black)
//   if (data >= high_threshold) → max color
//   else: overall_percent = (data - low) / (high - low)  → 0.0 to 1.0
// Color gain adjusts high_threshold, black level adjusts low_threshold.
QRgb SpectrumWidget::dbmToRgb(float dbm) const
{
    // Effective thresholds adjusted by gain/black level sliders
    // Black level slider (0-125): lower = more black, higher = less black
    // Color gain slider (0-100): shifts high threshold DOWN (more color)
    // From Thetis display.cs:2522-2536 defaults: high=-80, low=-130
    float effectiveLow = m_wfLowThreshold + static_cast<float>(125 - m_wfBlackLevel) * 0.4f;
    float effectiveHigh = m_wfHighThreshold - static_cast<float>(m_wfColorGain) * 0.3f;
    if (effectiveHigh <= effectiveLow) {
        effectiveHigh = effectiveLow + 1.0f;
    }

    // From Thetis display.cs:6889-6891
    float range = effectiveHigh - effectiveLow;
    float adjusted = (dbm - effectiveLow) / range;
    adjusted = qBound(0.0f, adjusted, 1.0f);

    // Look up in gradient stops for current color scheme
    int stopCount = 0;
    const WfGradientStop* stops = wfSchemeStops(m_wfColorScheme, stopCount);

    // Find the two surrounding stops and interpolate
    for (int i = 0; i < stopCount - 1; ++i) {
        if (adjusted <= stops[i + 1].pos) {
            float t = (adjusted - stops[i].pos)
                    / (stops[i + 1].pos - stops[i].pos);
            int r = static_cast<int>(stops[i].r + t * (stops[i + 1].r - stops[i].r));
            int g = static_cast<int>(stops[i].g + t * (stops[i + 1].g - stops[i].g));
            int b = static_cast<int>(stops[i].b + t * (stops[i + 1].b - stops[i].b));
            return qRgb(r, g, b);
        }
    }
    return qRgb(stops[stopCount - 1].r,
                stops[stopCount - 1].g,
                stops[stopCount - 1].b);
}

// ---- VFO marker + filter passband overlay ----
// Ported from AetherSDR SpectrumWidget.cpp:3211-3294
// Uses per-slice colors with exact alpha values from AetherSDR.
void SpectrumWidget::drawVfoMarker(QPainter& p, const QRect& specRect, const QRect& wfRect)
{
    if (m_vfoHz <= 0.0) {
        return;
    }

    int vfoX = hzToX(m_vfoHz, specRect);

    // Per-slice color — from AetherSDR SliceColors.h:15-20
    // Slice 0 (A) = cyan, active
    static constexpr int kSliceR = 0x00, kSliceG = 0xd4, kSliceB = 0xff;

    // Filter passband rectangle
    double loHz = m_vfoHz + m_filterLowHz;
    double hiHz = m_vfoHz + m_filterHighHz;
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

    // Filter edge lines — from AetherSDR line 3237: slice color, alpha=130
    // Clip the spectrum-side edge to specBottomClipped so the line stops at
    // the bandplan strip's top edge.
    p.setPen(QPen(QColor(kSliceR, kSliceG, kSliceB, 130), 1));
    p.drawLine(xLo, specRect.top(), xLo, specBottomClipped);
    p.drawLine(xLo, wfRect.top(),   xLo, wfRect.bottom());
    p.drawLine(xHi, specRect.top(), xHi, specBottomClipped);
    p.drawLine(xHi, wfRect.top(),   xHi, wfRect.bottom());

    // VFO center line — from AetherSDR line 3281: slice color, alpha=220, width=2
    // Width narrows to 1 when filter edge is ≤4px away (CW modes)
    qreal vfoLineW = (std::abs(vfoX - xLo) <= 4 || std::abs(vfoX - xHi) <= 4) ? 1.0 : 2.0;
    p.setPen(QPen(QColor(kSliceR, kSliceG, kSliceB, 220), vfoLineW));
    p.drawLine(vfoX, specRect.top(), vfoX, wfRect.bottom());

    // VFO triangle marker — from AetherSDR line 3285-3293
    // Drawn below any VFO flag widget that may be positioned at the top.
    // If a VfoWidget exists for this slice, draw triangle at the flag's bottom edge.
    // Otherwise draw at spectrum top.
    if (vfoX >= specRect.left() && vfoX <= specRect.right()) {
        static constexpr int kTriHalf = 6;
        static constexpr int kTriH = 10;

        int triTop = specRect.top();
        // If VFO flag is present, position triangle below it
        auto it = m_vfoWidgets.constFind(0);
        if (it != m_vfoWidgets.constEnd() && it.value()->isVisible()) {
            triTop = it.value()->y() + it.value()->height();
        }
        // Clamp to spectrum area
        triTop = std::max(triTop, specRect.top());

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(kSliceR, kSliceG, kSliceB));
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
    if (m_vfoOffScreen == VfoOffScreen::None) {
        return;
    }

    // Arrow and label colors — match slice accent color
    static constexpr int kArrowW = 14;
    static constexpr int kArrowH = 20;
    QColor arrowColor(0x00, 0xb4, 0xd8);  // Cyan accent

    // Format frequency text
    double mhz = m_vfoHz / 1.0e6;
    QString label = QString::number(mhz, 'f', 4);

    QFont font = p.font();
    font.setPixelSize(11);
    font.setBold(true);
    p.setFont(font);
    QFontMetrics fm(font);

    int arrowY = specRect.top() + specRect.height() / 2 - kArrowH / 2;

    if (m_vfoOffScreen == VfoOffScreen::Left) {
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
    m_displayTimer.setInterval(1000 / clamped);
    // Averaging alphas depend on fps via Thetis α = exp(-1/(fps×τ)).
    // Recompute so the smoothing time constants stay correct after a rate change.
    recomputeAverageAlphas();
}

void SpectrumWidget::setMoxOverlay(bool isTx)
{
    if (m_moxOverlay == isTx) {
        return;  // idempotent
    }
    m_moxOverlay = isTx;
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

void SpectrumWidget::setDisplayDuplex(bool on)
{
    // From Thetis console.cs:15363-15369 [v2.10.3.13]:
    //   private bool _display_duplex = false;
    //   public bool DisplayDuplex { get; set; }
    // NereusSDR defaults this to true since the panadapter stays live
    // during MOX (see SpectrumWidget header note).  Setter is provided for
    // a potential future Setup checkbox.
    if (m_displayDuplex == on) {
        return;
    }
    m_displayDuplex = on;
    update();
}

// From Thetis display.cs:4840 [v2.10.3.13]:
//   if (!local_mox) fOffset += rx1_preamp_offset;
// The RX cal offset is only added in RX mode; during TX, the TX path uses
// its own calibration. NereusSDR models this by storing the TX ATT offset
// and applying it as an additional shift in paintEvent when m_moxOverlay.
void SpectrumWidget::setTxAttenuatorOffsetDb(float offsetDb)
{
    if (m_txAttOffsetDb == offsetDb) {
        return;
    }
    m_txAttOffsetDb = offsetDb;
    if (m_moxOverlay) {
        markOverlayDirty();  // only repaints while TX is active
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
void SpectrumWidget::setSpotMarkers(const QVector<SpotMarker>& markers)
{
    m_spotMarkers = markers;
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
            emit frequencyClicked(freqHz);
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
void SpectrumWidget::drawTxFilterOverlay(QPainter& p, const QRect& specRect)
{
    if (m_vfoHz <= 0.0) {
        return;
    }

    auto [iqLow, iqHigh] = txAudioToIq(m_txFilterLow, m_txFilterHigh, m_txMode);

    // m_txVfoOffsetHz tracks the active XIT offset so the overlay centers
    // on the actual TX frequency rather than the RX VFO.  Zero when XIT is
    // disabled or the slice has no XIT.
    const double txCenter = m_vfoHz + static_cast<double>(m_txVfoOffsetHz);
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
    // drawTxFilterOverlay (panadapter side); kept in lockstep.
    const double txCenter = m_vfoHz + static_cast<double>(m_txVfoOffsetHz);
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

// Mouse handlers must mirror the active render path's specH formula so the
// hover/click hit-tests align with where the spectrum/divider/freq-scale/
// waterfall rows are actually painted. The two render paths use DIFFERENT
// layouts:
//   - GPU path (renderGpuFrame:3313)  → contentH = h - chromeH; specH =
//     contentH * spectrumFrac; freq bar between spectrum and waterfall.
//   - QPainter path (paintEvent:1222) → specH = h * spectrumFrac;
//     freq bar at h - kFreqScaleH (BOTTOM of widget).
// Codex P1 (PR #140) fix — option B (conditional). Long-term plan is to
// unify the two render paths around the GPU layout; tracked separately so
// a non-GPU build can be verified before flipping.
static int specHFromHeight(int widgetH, float spectrumFrac, int chromeH)
{
#ifdef NEREUS_GPU_SPECTRUM
    const int contentH = widgetH - chromeH;
    return static_cast<int>(contentH * spectrumFrac);
#else
    Q_UNUSED(chromeH);
    return static_cast<int>(widgetH * spectrumFrac);
#endif
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

    int w = width();
    int h = height();
    int specH = specHFromHeight(h, m_spectrumFrac, kFreqScaleH + kDividerH);
    int dividerY = specH;
    QRect specRect(0, 0, w - effectiveStripW(), specH);
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
                            emit frequencyClicked(freqHz);
                            emit spotTriggered(spotIndex);
                        });
                } else {
                    menu.addAction(QString("Tune to %1").arg(call), this,
                        [this, freqHz]() {
                            emit frequencyClicked(freqHz);
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
            connect(m_overlayMenu, &SpectrumOverlayMenu::wfColorGainChanged,
                    this, [this](int v) { m_wfColorGain = v; update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::wfBlackLevelChanged,
                    this, [this](int v) { m_wfBlackLevel = v; update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::wfColorSchemeChanged,
                    this, [this](int v) { m_wfColorScheme = static_cast<WfColorScheme>(v); update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::fillAlphaChanged,
                    this, [this](float v) { m_fillAlpha = v; update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::panFillChanged,
                    this, [this](bool v) { m_panFill = v; update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::refLevelChanged,
                    this, [this](float v) { m_refLevel = v; update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::dynRangeChanged,
                    this, [this](float v) { m_dynamicRange = v; update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::ctunChanged,
                    this, [this](bool v) { setCtunEnabled(v); });
        }
        m_overlayMenu->setValues(m_wfColorGain, m_wfBlackLevel, false,
                                  static_cast<int>(m_wfColorScheme),
                                  m_fillAlpha, m_panFill, false,
                                  m_refLevel, m_dynamicRange, m_ctunEnabled);
        m_overlayMenu->move(event->globalPosition().toPoint());
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
                emit frequencyClicked(hr.freqMhz * 1.0e6);
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
            return;
        }

        // Below arrows: start drag-pan (existing behavior)
        m_draggingDbm = true;
        m_dragStartY = my;
        m_dragStartRef = m_refLevel;
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
    int freqBarY = dividerY + kDividerH;
    if (my >= freqBarY && my < freqBarY + kFreqScaleH) {
        // Sub-epic E: the LIVE button sits in the freq-scale row, so its
        // hit-test MUST run before the bandwidth-drag below grabs the click.
        // From AetherSDR SpectrumWidget.cpp:1655-1660 [@0cd4559]
        const int wfY = freqBarY + kFreqScaleH;
        const QRect wfRect(0, wfY, w, h - wfY);
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
    const int wfY = freqBarY + kFreqScaleH;
    const QRect wfRect(0, wfY, w, h - wfY);

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
    int w = width();
    int h = height();
    int specH = specHFromHeight(h, m_spectrumFrac, kFreqScaleH + kDividerH);
    QRect specRect(0, 0, w - effectiveStripW(), specH);

    // --- Active drag modes ---

    // Sub-epic E: time-scale drag = scrub through history
    // From AetherSDR SpectrumWidget.cpp:2122-2145 [@0cd4559]
    if (m_draggingTimeScale) {
        const int wfY = specH + kDividerH + kFreqScaleH;
        const QRect wfRect(0, wfY, w, h - wfY);
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

    if (m_draggingDbm) {
        int dy = my - m_dragStartY;
        float dbPerPixel = m_dynamicRange / static_cast<float>(specH);
        m_refLevel = m_dragStartRef + static_cast<float>(dy) * dbPerPixel;
        m_refLevel = qBound(-160.0f, m_refLevel, 20.0f);
        update();
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
        emit frequencyClicked(hz);
        return;
    }

    if (m_draggingBandwidth) {
        // Zoom bandwidth by horizontal drag
        // From AetherSDR SpectrumWidget.cpp:868-876
        // Drag right = zoom in (narrower), drag left = zoom out (wider)
        int dx = m_bwDragStartX - mx;
        double factor = 1.0 + dx * 0.003;  // 0.3% per pixel
        double newBw = m_bwDragStartBw * factor;
        newBw = std::clamp(newBw, 1000.0, m_sampleRateHz);
        m_bandwidthHz = newBw;
        // Recenter on VFO when zooming so the signal stays visible
        // Only emit centerChanged if center actually moved — avoids DDC retune per drag frame
        if (!qFuzzyCompare(m_centerHz, m_vfoHz)) {
            m_centerHz = m_vfoHz;
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
        float frac = static_cast<float>(my) / h;
        m_spectrumFrac = std::clamp(frac, 0.10f, 0.90f);
#ifdef NEREUS_GPU_SPECTRUM
        markOverlayDirty();
#else
        update();
#endif
        return;
    }

    if (m_draggingPan) {
        // Pan the view — drag changes center, not VFO
        // From AetherSDR SpectrumWidget.cpp:1230-1237
        double deltaPx = mx - m_panDragStartX;
        double deltaHz = -(deltaPx / static_cast<double>(specRect.width())) * m_bandwidthHz;
        m_centerHz = m_panDragStartCenter + deltaHz;
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

    int freqBarY = specH + kDividerH;

    // Sub-epic E: hover cursors for LIVE button + time scale
    // From AetherSDR SpectrumWidget.cpp:2200-2225 [@0cd4559]
    {
        const int wfY = specH + kDividerH + kFreqScaleH;
        if (my >= specH + kDividerH && my < wfY) {
            // In the freq-scale row — LIVE button overlaps here.
            const QRect wfRect(0, wfY, w, h - wfY);
            if (waterfallLiveButtonRect(wfRect).contains(event->position().toPoint())) {
                setCursor(Qt::PointingHandCursor);
                return;
            }
        }
        if (my >= wfY) {
            const QRect wfRect(0, wfY, w, h - wfY);
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

void SpectrumWidget::mouseReleaseEvent(QMouseEvent* event)
{
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
            if (dx <= 4) {
                int w = width();
                int specH = static_cast<int>(height() * m_spectrumFrac);
                QRect specRect(0, 0, w - effectiveStripW(), specH);
                double hz = xToHz(static_cast<int>(event->position().x()), specRect);
                hz = std::round(hz / m_stepHz) * m_stepHz;
                emit frequencyClicked(hz);
            }
        }

        // Persist display settings after drag adjustments.
        // Also emit range-change for observers (MainWindow, tests).
        // From AetherSDR SpectrumWidget.cpp:2115 [@0cd4559]
        if (m_draggingDbm) {
            emit dbmRangeChangeRequested(m_refLevel - m_dynamicRange, m_refLevel);
        }
        if (m_draggingDbm || m_draggingDivider) {
            scheduleSettingsSave();
        }

        // Hybrid zoom: replan FFT on drag-end for sharp resolution
        if (m_draggingBandwidth) {
            emit bandwidthChangeRequested(m_bandwidthHz);
        }

        m_draggingDbm = false;
        m_draggingFilter = FilterEdge::None;
        m_draggingVfo = false;
        m_draggingDivider = false;
        m_draggingPan = false;
        m_draggingBandwidth = false;
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
    QWidget::leaveEvent(event);
}

void SpectrumWidget::wheelEvent(QWheelEvent* event)
{
    // Wheel over dBm strip: adjust dynamic range in ±5 dB steps.
    // From AetherSDR SpectrumWidget.cpp:2630-2636 [@0cd4559]
    const int mx = static_cast<int>(event->position().x());
    const int my = static_cast<int>(event->position().y());
    const int specH = static_cast<int>(height() * m_spectrumFrac);
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
            emit dbmRangeChangeRequested(bottom, m_refLevel);
            update();
            scheduleSettingsSave();
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
        newBw = std::clamp(newBw, 1000.0, m_sampleRateHz);
        m_bandwidthHz = newBw;
        // Recenter on VFO when zooming
        m_centerHz = m_vfoHz;
        emit centerChanged(m_centerHz);
        emit bandwidthChangeRequested(newBw);
        updateVfoPositions();
#ifdef NEREUS_GPU_SPECTRUM
        markOverlayDirty();
#endif
    } else if (event->modifiers() & Qt::ShiftModifier) {
        // Shift+scroll: adjust ref level
        float step = (delta > 0) ? 5.0f : -5.0f;
        m_refLevel = qBound(-160.0f, m_refLevel + step, 20.0f);
        scheduleSettingsSave();
    } else {
        // Plain scroll: tune VFO by step size
        int steps = (delta > 0) ? 1 : -1;
        double newHz = m_vfoHz + steps * m_stepHz;
        newHz = std::max(newHz, 100000.0);
        emit frequencyClicked(newHz);
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
    QRhi* r = rhi();
    const int w = width();
    const int h = height();
    if (w <= 0 || h <= kFreqScaleH + kDividerH + 2) { return; }

    const int chromeH = kFreqScaleH + kDividerH;
    const int contentH = h - chromeH;
    const int specH = static_cast<int>(contentH * m_spectrumFrac);
    const int wfY = specH + kDividerH + kFreqScaleH;
    const int wfH = h - wfY;
    // Strip lives on the right edge (overlay texture paints it there).
    // Clip GPU content to w - effectiveStripW() so the trace stops at the
    // strip's border instead of being drawn under it (or fills full width
    // when the strip is hidden).
    const QRect specRect(0, 0, w - effectiveStripW(), specH);
    const QRect wfRect(0, wfY, w - effectiveStripW(), wfH);

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
    float rowOffset = (m_wfGpuTexH > 0)
        ? static_cast<float>(m_wfWriteRow) / m_wfGpuTexH : 0.0f;
    float uniforms[] = {rowOffset, 0.0f, 0.0f, 0.0f};
    batch->updateDynamicBuffer(m_wfUbo, 0, sizeof(uniforms), uniforms);

    // ---- Overlay texture (static, only on state change) ----
    {
        const qreal dpr = devicePixelRatioF();
        const int pw = static_cast<int>(w * dpr);
        const int ph = static_cast<int>(h * dpr);
        if (m_overlayStatic.size() != QSize(pw, ph)) {
            m_overlayStatic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
            m_overlayStatic.setDevicePixelRatio(dpr);
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

        if (m_overlayStaticDirty) {
            m_overlayStatic.fill(Qt::transparent);
            QPainter p(&m_overlayStatic);
            p.setRenderHint(QPainter::Antialiasing, false);

            drawGrid(p, specRect);
            if (m_dbmScaleVisible) {
                // drawDbmScale needs the FULL-WIDTH rect so the strip
                // lands in the reserved right-edge zone at x=[w-kDbmStripW..w-1].
                // Passing the clipped specRect would put the strip INSIDE the spectrum.
                drawDbmScale(p, QRect(0, 0, w, specH));
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
            const QRect wfRectFull(0, wfRect.top(), w, wfRect.height());
            p.fillRect(0, specH, w, kDividerH, QColor(0x30, 0x40, 0x50));
            drawFreqScale(p, QRect(0, specH + kDividerH, w - effectiveStripW(), kFreqScaleH));
            drawTimeScale(p, wfRectFull);
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

            // Per-frame dynamic overlays — Active Peak Hold trace + Peak
            // Blobs (Tasks 2.5/2.6) + Noise-floor line/text. The CPU
            // paintEvent path calls these from drawSpectrum() every frame;
            // the GPU path needs them baked into the overlay texture.
            // updateSpectrumLinear() forces m_overlayStaticDirty=true when
            // any of them is enabled so this block runs every frame.
            if ((m_activePeakHold.enabled() || m_peakBlobs.enabled()) &&
                !m_renderedPixels.isEmpty()) {
                if (m_activePeakHold.enabled() && m_activePeakHold.size() > 0) {
                    paintActivePeakHoldTrace(p, specRect);
                }
                if (m_peakBlobs.enabled() && !m_peakBlobs.blobs().isEmpty()) {
                    paintPeakBlobs(p, specRect);
                }
            }
            paintNoiseFloorOverlay(p, specRect);

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

            // HIGH SWR / PA safety overlay — painted last so it sits on top
            // of all other chrome. From Thetis display.cs:4183-4201 [v2.10.3.13].
            paintHighSwrOverlay(p);

            m_overlayStaticDirty = false;
            m_overlayNeedsUpload = true;
        }

        if (m_overlayNeedsUpload) {
            batch->uploadTexture(m_ovGpuTex, QRhiTextureUploadEntry(0, 0,
                QRhiTextureSubresourceUploadDescription(m_overlayStatic)));
            m_overlayNeedsUpload = false;
        }
    }

    // ---- FFT spectrum vertices ----
    // GPU vertex generation -- one vertex per display pixel.  Mirrors
    // Thetis Display.cs:5249-5378 [v2.10.3.13] per-pixel render loop
    // (DrawLine over current_display_data[i], i = 0..nDecimatedWidth-1).
    // Honours:
    //   - m_dbmCalOffset (shifts every pixel's dBm before y mapping)
    //   - m_gradientEnabled (off = flat m_fillColor, on = heatmap)
    //   - m_panFill (skips fill VBO update when disabled)
    //   - m_fillColor / m_fillAlpha (used for the flat-fill path)
    //   - m_peakHoldEnabled (generates a second line VBO for peak hold)
    if (!m_renderedPixels.isEmpty() && m_fftLineVbo && m_fftFillVbo) {
        const int n = qMin(m_renderedPixels.size(), kMaxFftBins);
        m_visibleBinCount = n;
        const float minDbm = m_refLevel - m_dynamicRange;
        const float range  = m_dynamicRange;
        const float yBot = -1.0f;
        const float yTop = 1.0f;

        const float fa = m_fillAlpha;
        const float cal = m_dbmCalOffset;

        // Flat-mode colour picked from m_fillColor.
        const float flatR = m_fillColor.redF();
        const float flatG = m_fillColor.greenF();
        const float flatB = m_fillColor.blueF();

        QVector<float> lineVerts(n * kFftVertStride);
        QVector<float> fillVerts(n * 2 * kFftVertStride);

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
        batch->updateDynamicBuffer(m_fftFillVbo, 0,
            n * 2 * kFftVertStride * sizeof(float), fillVerts.constData());

        // Peak hold VBO lives alongside the line VBO -- generated here,
        // drawn in the render pass after the main line. When peak hold
        // is off we leave the buffer stale and skip the draw call via
        // m_peakHoldHasData.  Per-display-pixel array (m_pxPeakHold) is
        // sized to m_renderedPixels in updateSpectrumLinear.
        m_peakHoldHasData = false;
        if (m_peakHoldEnabled && m_pxPeakHold.size() == m_renderedPixels.size()
            && m_fftPeakVbo) {
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
            m_peakHoldHasData = true;
        }
    }

    cb->resourceUpdate(batch);

    // ---- Begin render pass ----
    const QColor clearColor(0x0a, 0x0a, 0x14);
    cb->beginPass(renderTarget(), clearColor, {1.0f, 0});

    const QSize outputSize = renderTarget()->pixelSize();
    const float dpr = outputSize.width() / static_cast<float>(qMax(1, w));

    // Draw waterfall
    if (m_wfPipeline) {
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

    // Draw FFT spectrum
    if (m_fftFillPipeline && m_fftLinePipeline && m_visibleBinCount > 0) {
        float specVpX = static_cast<float>(specRect.x()) * dpr;
        float specVpY = static_cast<float>(h - specRect.bottom() - 1) * dpr;
        float specVpW = static_cast<float>(specRect.width()) * dpr;
        float specVpH = static_cast<float>(specRect.height()) * dpr;
        QRhiViewport specVp(specVpX, specVpY, specVpW, specVpH);

        // Fill pass — Phase 3G-8 commit 10: skip when fill is disabled.
        if (m_panFill) {
            cb->setGraphicsPipeline(m_fftFillPipeline);
            cb->setShaderResources(m_fftSrb);
            cb->setViewport(specVp);
            const QRhiCommandBuffer::VertexInput fillVbuf(m_fftFillVbo, 0);
            cb->setVertexInput(0, 1, &fillVbuf);
            cb->draw(m_visibleBinCount * 2);
        }

        // Peak hold line (drawn before main line so live trace is on top).
        if (m_peakHoldHasData && m_fftPeakVbo) {
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

    // Draw overlay
    if (m_ovPipeline) {
        cb->setGraphicsPipeline(m_ovPipeline);
        cb->setShaderResources(m_ovSrb);
        cb->setViewport({0, 0,
            static_cast<float>(outputSize.width()),
            static_cast<float>(outputSize.height())});
        const QRhiCommandBuffer::VertexInput vbuf(m_ovVbo, 0);
        cb->setVertexInput(0, 1, &vbuf);
        cb->draw(4);
    }

    cb->endPass();
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

    delete m_fftLinePipeline;  m_fftLinePipeline = nullptr;
    delete m_fftFillPipeline;  m_fftFillPipeline = nullptr;
    delete m_fftSrb;           m_fftSrb = nullptr;
    delete m_fftLineVbo;       m_fftLineVbo = nullptr;
    delete m_fftFillVbo;       m_fftFillVbo = nullptr;
    delete m_fftPeakVbo;       m_fftPeakVbo = nullptr;

    m_rhiInitialized = false;
}

#endif // NEREUS_GPU_SPECTRUM

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
            m_centerHz = hz + m_bandwidthHz / 2.0 - margin;
            needsScroll = true;
        } else if (hz > rightEdge - margin) {
            m_centerHz = hz - m_bandwidthHz / 2.0 + margin;
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
    m_centerHz = m_vfoHz;
    m_vfoOffScreen = VfoOffScreen::None;
    emit centerChanged(m_centerHz);
    updateVfoPositions();
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
#else
    update();
#endif
}

void SpectrumWidget::setCtunEnabled(bool enabled)
{
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
    w->show();
    w->raise();
    return w;
}

void SpectrumWidget::removeVfoWidget(int sliceIndex)
{
    if (auto* w = m_vfoWidgets.take(sliceIndex)) {
        delete w;
    }
}

VfoWidget* SpectrumWidget::vfoWidget(int sliceIndex) const
{
    return m_vfoWidgets.value(sliceIndex, nullptr);
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

    int specH = static_cast<int>(height() * m_spectrumFrac);
    QRect specRect(0, 0, width() - effectiveStripW(), specH);
    int vfoX = hzToX(m_vfoHz, specRect);

    for (auto it = m_vfoWidgets.begin(); it != m_vfoWidgets.end(); ++it) {
        VfoWidget* vfo = it.value();
        if (vfo->width() <= 0) {
            vfo->adjustSize();
        }
        // Hide VFO flag when off-screen (SmartSDR pattern)
        if (m_vfoOffScreen != VfoOffScreen::None) {
            vfo->hide();
        } else {
            if (!vfo->isVisible()) {
                vfo->show();
            }
            vfo->updatePosition(vfoX, 0);
            vfo->raise();
        }
    }
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
