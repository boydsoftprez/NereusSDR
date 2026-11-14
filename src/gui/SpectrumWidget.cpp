// =================================================================
// src/gui/SpectrumWidget.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (Longpath):
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

#include <QContextMenuEvent>

#include "widgets/NotchEditPopup.h"
#include "core/FFTEngine.h"   // kDefaultFftSize
#include "models/SpotModel.h"  // kSpotKeinSnr — nur fuer den static_assert
#include "gui/StyleConstants.h"
#include "gui/styles/Theme.h"
#include "gui/styles/ThemeQss.h"
#include "SpectrumOverlayMenu.h"
#include "ImdOverlay.h"
#include "spectrum/WaterfallTicker.h"
#include "spectrum/WaterfallTimeMarkers.h"
#include "ColorSwatchButton.h"
#include "core/AppSettings.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "core/LogCategories.h"   // Phase 3M-4 bench-fix Round 2: lcSpectrum
#include "dbm_strip_math.h"
#include "popup_placement.h"
#include "models/BandPlanManager.h"
#include "core/TransverterSetup.h"
#include "models/NotchModel.h"
#include "spectrum/SpectrumDetector.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QStyleHints>
#include <QHoverEvent>
#include <QLabel>
#include <QPropertyAnimation>
#include <QScreen>
// new QThread(this) und &QThread::finished brauchen den vollständigen
// Typ; der Header führt bei m_waterfallTickerThread nur einen Zeiger.
// Übersetzt hat es bisher allein der vorkompilierte Header — mit
// -DLONGPATH_USE_PCH=OFF, laut CMakeLists.txt:1590 die Einstellung für
// genau eine Header-Hygiene-Prüfung, fiel diese Datei um.
#include <QThread>
#include <QToolTip>
#include <QUrl>

#include <QDateTime>
#include <QElapsedTimer>
#include <QTimeZone>

// Still needed for memoryLockStats() in the perf overlay + the 1 Hz perf
// log line: the display buffers no longer take page locks, but the audio
// ring and FFT scratch still do, and that is what the readout reports.
#include "core/MemoryLock.h"
#include "core/MemoryPressure.h"
#include "core/PerfMonitor.h"
#include <QMap>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include "core/FrequenzEichung.h"
#include "core/SMeterSkala.h"
#include <QLinearGradient>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QFile>
#include <QLibraryInfo>

#ifdef LONGPATH_GPU_SPECTRUM
#include <rhi/qshader.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <utility>
#include <QTimer>

namespace Longpath {

namespace {

// ── Die Bandbreite ueber Wasserfall und 3D-Flaeche ───────────────────
//
// Hell, nicht tuerkis. Die Benutzerfarbe der Saeule (m_rxFilterColor,
// Vorgabe Tuerkis mit Alpha 80) ist fuer das schwarze 2D-Spektrum
// gemacht; auf dem bunten Wasserfall und auf der blauen 3D-Flaeche geht
// sie unter — der Betreiber am 2026-09-17: "ich sehe meine bandbreite im
// unteren bild nicht von oben bis unten." die Vorlage (live angesehen am
// selben Tag) legt dort eine weissliche, durchscheinende Saeule mit
// duennen Kanten quer durch Spektrum und Wasserfall; hell steht auf
// jeder Farbe. Leicht blaeulich, damit es zur Sprache des Hauses passt
// (Blau ist anfassbar — die Kanten zieht man).
const QColor kPassbandWash(0xdc, 0xe6, 0xf0, 60);
const QColor kPassbandEdgeLight(0xdc, 0xe6, 0xf0, 165);

// Deckkraft der beiden GPU-Baender. Standen bis 2026-10-08 als
// Zahlenliteral je zweimal im Malcode; jetzt an einer Stelle, weil
// `Kurve::baue` sie als Wert bekommt und sonst zwei Zahlen
// auseinanderlaufen koennten, die dasselbe bedeuten.
constexpr float kTraceAlpha = 0.9f;
constexpr float kPeakAlpha  = 0.55f;

} // namespace

// ── Flächen, Gitter und Rahmen kommen aus dem Theme ──────────────────
//
// 2026-08-15, OE5SOS: „der Spektrumbereich im Hauptfenster ist noch
// blaustichig, während der Wasserfall darunter neutral ist — die Naht
// ist quer durchs Fenster sichtbar."
//
// Sie war es, und zwar an genau neun Stellen. Das GITTER lief längst
// über Rollen ("grid", "grid-text"); die Werte im Klassenkopf sind tote
// Vorgaben, die loadSettings() überschreibt. Blau waren nur die
// FLÄCHEN, und die standen als QColor(0x0f, 0x0f, 0x1a) hart im
// Malcode — dieselben vier Werte, die docs/design/HAUSSTIL.md unter
// „Token" als die alte, durchgehend blaustichige Palette aufführt.
//
// Der Umweg über eine Rolle statt über eine Konstante ist der Punkt:
// eine Konstante steht beim Übersetzen fest, eine Rolle liest die
// Theme-Datei. Siehe die Notiz über pal() in SwrCurveWidget.cpp — es
// ist derselbe Fehler und dieselbe Abhilfe.
//
// Nicht hierdurch geführt: die Kurve, die Rufzeichen, die Marken — und
// die Bandkante, die {255, 0, 0} bleibt. Eine Sicherheitsmarke gehört
// nicht in eine Palette, die jemand umstellen kann.
QColor roleColor(const char* roleName, const char* fallback,
                 int alpha = 255)
{
    QColor c(Style::role(roleName, fallback));
    if (alpha != 255) { c.setAlpha(alpha); }
    return c;
}

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
// Die beiden „keine Angabe“-Werte muessen dieselbe Zahl sein: der
// Kopf von SpectrumWidget zieht das Modell nicht herein, darum steht
// sie dort noch einmal. Hier faellt es beim Uebersetzen auf, falls
// einer der beiden je geaendert wird.
static_assert(SpectrumWidget::SpotMarker::kKeineAngabe
                  == Longpath::kSpotKeinSnr,
              "SpotMarker::kKeineAngabe und kSpotKeinSnr sind "
              "auseinandergelaufen");

// Der Stoerabstand, wie ihn WSJT-X schreibt: immer mit Vorzeichen,
// damit −2 und +2 auf einen Blick zu unterscheiden sind.
static QString sSnrText(int db)
{
    return QStringLiteral("%1%2").arg(db < 0 ? QString() : QStringLiteral("+"))
                                 .arg(db);
}

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
            || a.snrDb != b.snrDb
            || a.source != b.source) {
            return false;
        }
    }
    return true;
}

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
// 2026-08-07: reworked alongside ClarityBlue, and for the same two
// reasons — but worse here. The top 22% of this ramp was red-magenta
// into purple, so a strong signal did not merely get a pink cap, it got
// a pink and purple band a fifth of the range deep. Ten stops on exact
// ninths also banded badly: 28 of 255 steps were more than twice the
// mean perceptual step.
//
// Now spaced by equal CIELAB distance and ending at red: worst step
// 1.3x the mean, no step above twice it.
static const WfGradientStop kEnhancedStops[] = {
    {0.000f,  0x00, 0x00, 0x00},  // black
    {0.107f,  0x05, 0x06, 0x4c},  // deep navy
    {0.199f,  0x08, 0x12, 0xa4},  // blue
    {0.236f,  0x0d, 0x38, 0xdc},  // bright blue
    {0.306f,  0x10, 0x6e, 0xef},  // azure
    {0.398f,  0x14, 0xa2, 0xed},  // cyan
    {0.487f,  0x18, 0xc6, 0xd2},  // cyan-teal
    {0.565f,  0x22, 0xd6, 0xa2},  // teal
    {0.635f,  0x3c, 0xde, 0x6a},  // green
    {0.684f,  0x74, 0xe3, 0x3e},  // yellow-green
    {0.731f,  0xb2, 0xe6, 0x26},  // lime
    {0.783f,  0xe2, 0xdc, 0x1a},  // yellow
    {0.846f,  0xf7, 0xb6, 0x14},  // amber
    {0.905f,  0xfc, 0x8a, 0x18},  // orange
    {0.960f,  0xff, 0x5c, 0x22},  // red-orange
    {1.000f,  0xff, 0x2a, 0x2a},  // red — peak
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
// 2026-08-07: reworked. Two problems, both visible on the bench.
//
// The last stop was magenta, one step past red. That is a hue reversal
// — the ramp runs blue → cyan → green → yellow → orange → red, all of
// it forwards through the spectrum, and then turns back on itself. It
// reads as a pink cap sitting on top of every strong signal rather than
// as "even hotter", and it was the colour the whole waterfall showed
// while the threshold arithmetic was broken. The ramp now ends at red.
//
// The stops were also placed on round numbers rather than on equal
// perceptual steps, so the ramp banded: measured in CIELAB, the worst
// single step was 4.6x the mean and eleven steps were more than twice
// it — visible as hard edges between colour zones. The visible part of
// the ramp is now spaced by equal CIELAB distance, which brings the
// worst step to 1.4x the mean and no step above twice it.
//
// The deep floor is deliberate and kept: black to very dark blue over
// the bottom 18% of the range, so the noise floor stays dark instead of
// being spread across the palette. Equalising that region too would
// have compressed the dark share from 36% to 13% and thrown away what
// makes this scheme worth having.
static const WfGradientStop kClarityBlueStops[] = {
    {0.000f,  0x00, 0x00, 0x00},  // pure black — noise floor bottom
    {0.180f,  0x02, 0x07, 0x1c},  // very dark blue — noise floor top
    {0.226f,  0x05, 0x15, 0x40},  // dark blue — weak signal edge
    {0.264f,  0x08, 0x28, 0x6c},  // deep blue
    {0.298f,  0x0c, 0x3f, 0x99},  // blue
    {0.325f,  0x11, 0x59, 0xbe},  // bright blue
    {0.355f,  0x14, 0x76, 0xd6},  // blue-cyan
    {0.397f,  0x16, 0x93, 0xdf},  // cyan — medium signals
    {0.453f,  0x18, 0xac, 0xd4},  // cyan-teal
    {0.523f,  0x1d, 0xbe, 0xb2},  // teal
    {0.588f,  0x2c, 0xca, 0x8a},  // teal-green
    {0.644f,  0x50, 0xd2, 0x60},  // green — strong signals
    {0.694f,  0x8a, 0xda, 0x3c},  // yellow-green
    {0.746f,  0xc4, 0xdd, 0x24},  // yellow — very strong
    {0.795f,  0xee, 0xd4, 0x18},  // amber
    {0.855f,  0xf8, 0xac, 0x14},  // orange — extreme
    {0.911f,  0xfb, 0x82, 0x18},  // deep orange
    {0.962f,  0xfd, 0x58, 0x22},  // red-orange
    {1.000f,  0xff, 0x2a, 0x2a},  // red — peak
};

// Counts come from the arrays, not from a number typed beside them.
// They used to be hand-maintained, which is a stop-count one edit away
// from disagreeing with its palette — and a count that is too small
// silently truncates the top of the ramp while a count that is too
// large reads past the end of the array.
template <std::size_t N>
const WfGradientStop* pick(const WfGradientStop (&arr)[N], int& count)
{
    count = static_cast<int>(N);
    return arr;
}

// ── Gedämpft: die Rampe, die dem Theme folgt ─────────────────────────
//
// Die einzige der acht, deren Farben nicht in dieser Datei stehen. Sie
// werden bei jedem Aufruf aus den Rollen geholt, damit eine geänderte
// Theme-Datei auch den Wasserfall bewegt — er ist die größte Farbfläche
// im Programm, und ihn beim Palettenwechsel stehen zu lassen wäre die
// auffälligste Lücke.
//
// Die Form der Rampe:
//
//   0.00–0.30  Hintergrund → sehr dunkles Grau. Das Grundrauschen
//              verschwindet, statt eingefärbt zu werden. Vorher lag es
//              mitten im Blau und lieferte die flimmernde Fläche, die
//              den halben Bildschirm füllte.
//   0.30–0.72  Grau → Instrumentenglimmen. Schwache Signale werden
//              sichtbar, ohne eine Farbe zu bekommen.
//   0.72–0.94  Glimmen → Messwert. Hier steht, was man tatsächlich
//              anschaut, und hier bekommt es Wärme.
//   0.94–1.00  Messwert → Gefahr. Nur die Spitzen, nur ein Sechstel der
//              obersten zehn Prozent — nicht das durchgehende rote Band
//              von vorher.
static const WfGradientStop* mutedStops(int& count)
{
    static WfGradientStop stops[7];
    static quint64 builtFor = 0;
    const quint64 gen = Style::Theme::instance().generation();
    if (builtFor != gen) {
        auto at = [](float pos, const QString& hex, WfGradientStop& s) {
            const QColor c(hex);
            s.pos = pos;
            s.r = c.red();
            s.g = c.green();
            s.b = c.blue();
        };
        // ── Wo die Stufen sitzen, entscheidet mehr als welche ────────
        //
        // Erste Fassung: 0,00 / 0,16 / 0,34 / 0,56 / 0,74 / 0,92 / 1,00.
        // Gleichmäßig verteilt, und im Betrieb (2026-08-15, 40 m) war
        // das Ergebnis ein Nebel: der Rauschteppich landete bei etwa
        // 0,4 bis 0,6 der Spanne und damit mitten im hellen Grau.
        //
        // Zweite Fassung schob den Schwarzanteil auf 0,45 — und war
        // zu weit. Im Bild blieben selbst die Träger, die im Spektrum
        // darüber deutlich herausragten, im Wasserfall fast unsichtbar.
        // Ein Wasserfall, in dem man Signale sucht, ist so falsch wie
        // einer, in dem man sie im Nebel sucht.
        //
        // Dritte Fassung, und die Begründung dahinter: das Rauschen
        // soll verschwinden, das schwächste ERKENNBARE Signal soll
        // gerade sichtbar werden. Das liegt bei diesem Gerät bei etwa
        // einem Drittel der Spanne, nicht bei der Hälfte.
        at(0.00f, Style::role("app-bg",             Style::kAppBg),        stops[0]);
        at(0.30f, Style::role("panel",              Style::kPanelBg),      stops[1]);
        at(0.48f, Style::role("border",             Style::kBorder),       stops[2]);
        at(0.64f, Style::role("text-inactive",      Style::kTextInactive), stops[3]);
        at(0.78f, Style::role("instrument-glow-hi", Style::kInstrumentGlowHi), stops[4]);
        at(0.92f, Style::role("measured",           Style::kAmberText),    stops[5]);
        // ── Oben steht nicht „Achtung“, sondern „laut“ ───────────────
        //
        // Hier stand „danger“ (#a86b6d). Im Betrieb bekam damit jeder
        // kräftige Träger einen rosa Streifen — und ein starkes Signal
        // auf 20 m ist keine Gefahr, sondern ein starkes Signal.
        //
        // Das ist genau der Fehler, der beim S-Meter schon einmal
        // ausgebaut wurde: dort war der halbe Bogen rot und kodierte
        // damit nichts. Rot bedeutet in diesem Programm „Achtung“. Wenn
        // es zusätzlich „oberes Skalenende“ bedeutet, bedeutet es bald
        // gar nichts mehr — und dann fällt es auch nicht mehr auf, wo
        // es zählt: an der Bandkante und beim Stehwellenverhältnis.
        //
        // Stattdessen das Cremeweiß der Instrumente. Bernstein, das
        // nach oben hell wird, ist das, was ein Wasserfall seit jeher
        // tut, und es sagt genau eine Sache: hier ist viel Energie.
        at(1.00f, Style::role("instrument-face",    Style::kInstrumentFace), stops[6]);
        builtFor = gen;
    }
    count = 7;
    return stops;
}

const WfGradientStop* wfSchemeStops(WfColorScheme scheme, int& count)
{
    switch (scheme) {
    case WfColorScheme::Muted:       return mutedStops(count);
    case WfColorScheme::Enhanced:    return pick(kEnhancedStops, count);
    case WfColorScheme::Spectran:    return pick(kSpectranStops, count);
    case WfColorScheme::BlackWhite:  return pick(kBlackWhiteStops, count);
    case WfColorScheme::LinLog:      return pick(kLinLogStops, count);
    case WfColorScheme::LinRad:      return pick(kLinRadStops, count);
    case WfColorScheme::Custom:      return pick(kCustomFallbackStops, count);
    case WfColorScheme::ClarityBlue: return pick(kClarityBlueStops, count);
    case WfColorScheme::Default:
    default:                         return pick(kDefaultStops, count);
    }
}

// ---- SpectrumWidget ----

SpectrumWidget::SpectrumWidget(QWidget* parent)
    : SpectrumBaseClass(parent)
{
    setMinimumSize(400, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);

#ifdef LONGPATH_GPU_SPECTRUM
    // Platform-specific QRhi backend selection.
    // Tastenbedienung braucht Fokus. ClickFocus, nicht StrongFocus:
    // wer ins Spektrum klickt, will dort auch mit den Pfeiltasten
    // arbeiten — aber die Tabulator-Reihenfolge der Eingabefelder
    // soll nicht durch den Panadapter laufen.
    setFocusPolicy(Qt::ClickFocus);

    // Order matters: setApi() first, then WA_NativeWindow, then setMouseTracking().
    // WA_NativeWindow creates a dedicated native surface (NSView on macOS, HWND on
    // Windows); setMouseTracking() must come AFTER so tracking is configured on
    // the final native surface.
#ifdef Q_OS_MAC
    setApi(QRhiWidget::Api::Metal);
    // Seit 2026-08-22 als PAAR mit der Ancestor-Isolation — siehe
    // applyNativeWindowIsolationPolicy(); die nackte Setzung liess auf
    // macOS die Vorfahren mit-nativieren (upstream #4339).
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
    pal.setColor(QPalette::Window, roleColor("app-bg", Style::kAppBg));
    setPalette(pal);
#endif

    // Hintergrund aus den Einstellungen holen, bevor der erste Rahmen
    // gemalt wird — sonst blitzt beim Start einmal die Vorgabefarbe auf.
    loadBackgroundSettings();

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
            []() { Longpath::elevateLatencyCriticalThreadPriority(); });
    // Queued connection (default for cross-thread): tick fires on the
    // ticker thread, slot runs on the main thread when the event loop
    // is free.  See WaterfallTicker.h for the cadence-isolation rationale.
    connect(m_waterfallTicker, &WaterfallTicker::tick, this, [this]() {
        // ALWAYS push on tick.  Empty-cache guard so we do nothing
        // before the very first FFT.
        m_pendingWfPixelsDbmDirty = false;
        // ── Nur EIN Schreiber auf dem Bild (2026-10-23) ─────────────
        //
        // Liefert die Maschine eigene Wasserfallzeilen (Flex,
        // Paketklasse 0x8004), schweigt der aus der FFT abgeleitete
        // Weg. Zwei Schreiber gaeben ein Flackern aus zwei Takten —
        // und der Takt der Maschine ist der richtige, sie bestimmt ihn
        // mit `LineDurationMS`.
        //
        // Zwei Sekunden Nachlauf: reisst der Kachelstrom ab, uebernimmt
        // der eigene Weg wieder, statt das Bild stehenzulassen.
        if (m_fremdeWasserfallquelle) {
            if (QDateTime::currentMSecsSinceEpoch()
                    - m_fremdeWasserfallZuletztMs < 2000) {
                return;
            }
            m_fremdeWasserfallquelle = false;
            m_fremderTeppich = std::numeric_limits<float>::quiet_NaN();
        }
        if (!m_pendingWfPixelsDbm.isEmpty()) {
            pushWaterfallRow(m_pendingWfPixelsDbm);
        }
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

    // Coalescing timer for the waterfall + overlay re-allocation itself
    // (2026-08-16).  Sibling of the history debounce above and for the
    // same reason one step earlier in the chain: a resize drag delivers a
    // new size every ~30-40 ms and only the last one is a size the
    // operator asked for.  See m_resizeSettleTimer in the header.
    m_resizeSettleTimer = new QTimer(this);
    m_resizeSettleTimer->setSingleShot(true);
    m_resizeSettleTimer->setInterval(kResizeSettleMs);
    connect(m_resizeSettleTimer, &QTimer::timeout,
            this, &SpectrumWidget::applyResizeSettled);

    // 2026-05-26 KG4VCF perf instrumentation: 1 Hz poll that updates
    // PerfMonitor's memory-pressure sample and forces the overlay
    // texture to rebuild so the perf overlay (drawn into m_overlayStatic)
    // refreshes with the current stats.  Started only when the perf
    // overlay is toggled on (setShowPerfOverlay).
    //
    // Unter dem Wächter, weil m_perfPollTimer im
    // LONGPATH_GPU_SPECTRUM-Block des Headers steht und die Überlagerung
    // selbst (paintEvent, im GPU-Zweig) ohne ihn gar nicht gemalt wird.
    // Ohne GPU-Pfad entsteht der Timer also nicht, statt einmal je
    // Sekunde für eine Anzeige zu messen, die es nicht gibt.
#ifdef LONGPATH_GPU_SPECTRUM
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
            " audio_fill %9/%10 ms underruns %11 (+%12/s)"
            " ring_under %21 (+%22/s) ring_over %23 (+%24/s)"
            " udp %13 (+%14/s)"
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
            .arg(ml.bytesLocked / (1024.0 * 1024.0), 0, 'f', 1)
            // 2026-09-04: drei Unterlaufarten getrennt, weil sie auf
            // verschiedene Schuldige zeigen. underruns = das Geraet des
            // Betriebssystems lief leer (wir zu spaet: Zeitplanung,
            // Puffergroesse, Host-API). ring_under = unser Ring war leer
            // (Erzeuger hinkt: Netz oder DSP). ring_over = unser Ring lief
            // ueber, Aeltestes verworfen (wir schieben schneller nach, als
            // das Geraet abholt — klassisch bei Ratenabweichung). Bis hier
            // waren nur die ersten sichtbar, die beiden anderen wurden
            // gezaehlt und nie ausgegeben.
            .arg(s.audioRingUnderrunsTotal).arg(s.audioRingUnderrunsDelta)
            .arg(s.audioRingOverrunsTotal).arg(s.audioRingOverrunsDelta);
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
#endif  // LONGPATH_GPU_SPECTRUM — Perf-Überlagerung

    // Phase 3Q-8: child label for the disconnect overlay. Composites in both
    // CPU and GPU paint paths (QRhi early-returns from paintEvent so a QPainter
    // overlay there would crash; a child QWidget gets stacked by Qt instead).
    m_disconnectLabel = new QLabel(QStringLiteral("DISCONNECTED"), this);
    m_disconnectLabel->setAlignment(Qt::AlignCenter);
    // ── Nicht verbunden ist ein Zustand, keine Gefahr ───────────────
    //
    // Stand in #c14848 quer ueber dem Spektrum, 36 Punkt. Rot bedeutet
    // in diesem Programm "Achtung" und markiert Grenzen -- die
    // Bandkante, ein SWR, bei dem man nicht senden sollte. Getrennt zu
    // sein ist keine davon: es geht nichts kaputt, ein Klick verbindet.
    //
    // Dieselbe Ueberlegung wie beim oberen Skalenende und beim
    // Offline-Punkt der Statuspille. Und die Statuszeile oben links sagt
    // es ohnehin schon im Klartext -- das hier ist die Wiederholung,
    // nicht die Meldung.
    // ── Und es legt sich auch nicht ueber alles ──────────────────────
    //
    // Hier stand `background-color: rgba(10, 12, 20, 200)` auf einem
    // Kindwidget, das ueber die GANZE Flaeche des Panadapters gelegt
    // wird (setGeometry(0, 0, width(), height())). Eine zu 78 %
    // deckende schwarze Decke.
    //
    // Der Betreiber, 2026-08-21, zweimal: „beim start war das
    // hintergrundbild kurz perfekt, hat sich dann aber wieder
    // abgedunkelt" und „die ersten 10 sekunden bild perfekt da und
    // danach verschwindet es". Genau das war es: die Decke erscheint,
    // sobald der Verbindungsversuch aufgibt — davor ist sie versteckt.
    // Sie verdeckte damit das Hintergrundbild UND die Einblendungen
    // (Kompass, Stehwelle), die darunter ins Spektrum gemalt werden.
    //
    // Der Kommentar darueber sagt schon, dass Getrenntsein ein Zustand
    // ist und keine Gefahr, und dass die Statuszeile es ohnehin im
    // Klartext meldet. Dann darf es auch nicht das ganze Bild
    // ausloeschen. Der Schriftzug bleibt — als Schrift, nicht als
    // Vorhang.
    m_disconnectLabel->setStyleSheet(QStringLiteral(
        "QLabel { background-color: transparent;"
        " color: %1; font-size: 36pt; font-weight: bold;"
        " letter-spacing: 8px; }")
        .arg(QLatin1String(Style::kTextScale)));
    // Klicks gehen hindurch: ein Schriftzug, der das Abstimmen
    // blockiert, waere die Decke in anderer Form.
    m_disconnectLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_disconnectLabel->hide();
    m_disconnectLabel->installEventFilter(this);

    // Phase 3M-4 Task 12 — two-tone IMD overlay analytical core.
    // Owned via QObject parenting; raw pointer mirrors m_bandPlanManager.
    m_imdOverlay = new ImdOverlay(this);
}

SpectrumWidget::~SpectrumWidget()
{
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

    // Ship defaults — calibrated 2026-04-30 against a live ANAN-G2 with
    // a typical residential noise floor (-115 to -120 dBm in the
    // amateur HF bands). Earlier defaults ran 12 dB hotter (Grid -36 /
    // -104, Wf -50 / -110, Wf black 98) and gave a noisy first-launch
    // experience — band noise jammed the bottom of the panadapter and
    // lit up the waterfall floor. Shifting the entire reference plane
    // down 12 dB gives a clean "noise sits low" first impression.
    // Dynamic range (68 dB grid, 60 dB waterfall) is unchanged.
    // Aus den Konstanten, nicht als abgeschriebene Zahl: sonst haengt der
    // Startwert davon ab, ob je etwas gespeichert wurde, und die beiden
    // Stellen laufen beim naechsten Mal wieder auseinander.
    m_refLevel       = readFloat(QStringLiteral("DisplayGridMax"),
                                 kDefaultRefLevelDbm);
    m_dynamicRange   = readFloat(QStringLiteral("DisplayGridMax"),
                                 kDefaultRefLevelDbm)
                     - readFloat(QStringLiteral("DisplayGridMin"),
                                 kDefaultFloorDbm);
    m_spectrumFrac   = readFloat(QStringLiteral("DisplaySpectrumFrac"), 0.40f);

    // Phase 3G-12: persist the spectrum zoom level (visible bandwidth)
    // across app restarts. Center frequency is persisted indirectly via
    // SliceModel (slice frequency), so only bandwidth needs its own key.
    // Default 192000 Hz = 192 kHz matches the P1 base sample rate.
    m_bandwidthHz    = static_cast<double>(
                          readFloat(QStringLiteral("DisplayBandwidth"), 192000.0f));
    m_wfColorGain    = readInt(QStringLiteral("DisplayWfColorGain"), 45);
    m_wfBlackLevel   = readInt(QStringLiteral("DisplayWfBlackLevel"), 104);
    m_overlayPanelExpanded = readBool(
        QStringLiteral("DisplayOverlayPanelExpanded"), true);
    m_wfHighThreshold = readFloat(QStringLiteral("DisplayWfHighLevel"), -62.0f);
    m_wfLowThreshold = readFloat(QStringLiteral("DisplayWfLowLevel"), -122.0f);
    // Seed render-active mirror from persistent user values — matches
    // Thetis's per-render local seed at display.cs:6575/6590
    // [v2.10.3.13]. AGC / NF-AGC / Clarity will override these at
    // composeWaterfallActiveThresholds() time; the persistent fields
    // above stay untouched (issue #230 fix).
    m_wfActiveHighThreshold = m_wfHighThreshold;
    m_wfActiveLowThreshold  = m_wfLowThreshold;
    m_fillAlpha      = readFloat(QStringLiteral("DisplayFftFillAlpha"), 0.22f);
    m_panFill        = readBool(QStringLiteral("DisplayPanFill"), true);

    m_ctunEnabled    = readBool(QStringLiteral("DisplayCtunEnabled"), true);

    int scheme = readInt(QStringLiteral("DisplayWfColorScheme"), 0);
    m_wfColorScheme = static_cast<WfColorScheme>(qBound(0, scheme,
                          static_cast<int>(WfColorScheme::Count) - 1));

    int renderMode = readInt(QStringLiteral("DisplaySpectrumRenderMode"), 0);
    m_renderMode = static_cast<SpectrumRenderMode>(qBound(0, renderMode, 1));
    // Auch das Wiederherstellen ist ein Wechsel (2026-10-30): der
    // Schnellumschalter in der Kopfleiste des Panadapters liest seine
    // Beschriftung aus diesem Wert. Ohne das Signal stand dort beim
    // Start „2D", waehrend das Raumbild lief — am Geraet gesehen.
    emit spectrumRenderModeChanged(m_renderMode);
    m_dss3dTrueScale = readBool(QStringLiteral("Display3DTrueScale"), true);
    m_dss.setBackWidthFrac(m_dss3dTrueScale ? DssRenderer::kTrueScaleWidthFrac
                                            : DssRenderer::kBackWidthFrac);

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
        readInt(QStringLiteral("DisplaySpectrumAverageTimeMs"), 350), 9999);
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

        // ── Ruhige Kurve als Vorgabe (2026-08-22) ────────────────────
        //
        // Der Betreiber, live am Geraet: „die kurve oben ist so
        // nervoes, eine katastrophe, sollte viel ruhiger sein" — und
        // dann ausdruecklich: „aendere alles so, dass die kurven
        // brauchbar sind, kannst gerne alles optimieren."
        //
        // Thetis' Vorgabe waere 30 ms (setup.designer.cs
        // udDisplayAVGTime.Value = 30 [v2.10.3.15]) — das ergibt bei
        // 30 fps ein Alpha von 0,33 und genau das Zappeln, das er
        // beanstandet hat. Auf seine ausdrueckliche Ansage weichen wir
        // hier von der Thetis-Vorgabe ab: 350 ms, Alpha etwa 0,91.
        // Glatt genug zum Ablesen, schnell genug fuer CW.
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
    // 1.0f default: operator decision, 2026-08-26 -- see m_lineWidth's
    // own declaration comment in SpectrumWidget.h for why.
    m_lineWidth        = readFloat(QStringLiteral("DisplayLineWidth"), 1.0f);
    m_dbmCalOffset     = readFloat(QStringLiteral("DisplayCalOffset"), 0.0f);
    const bool peakOn = readBool(QStringLiteral("DisplayPeakHoldEnabled"), false);
    const bool gradOn = readBool(QStringLiteral("DisplayGradientEnabled"), true);
    m_gradientEnabled = gradOn;
    // Eigener Schluessel, eigene Vorgabe (aus) — siehe die Notiz bei
    // m_heatmapEnabled. Bewusst NICHT aus DisplayGradientEnabled
    // abgeleitet: wer den alten Schalter auf "an" stehen hat, wollte auf
    // dem CPU-Pfad eine Fuellung und auf dem GPU-Pfad einen Regenbogen,
    // und aus einem gespeicherten Wert laesst sich nicht ablesen,
    // welches von beidem gemeint war.
    m_heatmapEnabled  = readBool(QStringLiteral("DisplaySpectrumHeatmap"), false);
    // Weiche Kante AN, Hof AUS -- Wahl des Betreibers vom 2026-10-08
    // nach den drei Blaettern. Siehe setTraceSoftEdge im Header.
    m_traceSoftEdge   = readBool(QStringLiteral("DisplayTraceSoftEdge"), true);
    m_traceHalo       = readBool(QStringLiteral("DisplayTraceHalo"), false);
    // Schaerfe (2026-11-08, Betreiber an den Entwurfsblaettern: „Alles
    // zusammen"): alle drei AN ab Werk, siehe setWaterfallDevicePixels.
    m_gridHairline    = readBool(QStringLiteral("DisplayGridHairline"), true);
    m_traceEdgePx     = qBound(0.25f,
                               readFloat(QStringLiteral("DisplayTraceEdgePx"), 0.5f),
                               2.0f);
    // Durchlass und Bandbreite (2026-11-08): alle vier AN ab Werk.
    m_zeigeBandbreite = readBool(QStringLiteral("DisplayPassbandLabel"), true);
    m_zeigeKlammern   = readBool(QStringLiteral("DisplaySignalBrackets"), true);
    m_zeigeLuecken    = readBool(QStringLiteral("DisplayGapFinder"), true);
    // 0 aus, 1 an; eine alte 2 ("immer") gilt als an.
    m_lupenModus      = (readInt(QStringLiteral("DisplayPassbandMagnifier"), 1) != 0) ? 1 : 0;
    m_sStufenSkala    = readBool(QStringLiteral("DisplaySUnitsScale"), false);
    {
        const bool wfDev = readBool(QStringLiteral("DisplayWaterfallDevicePixels"), true);
        if (wfDev != m_wfDevicePixels) {
            m_wfDevicePixels = wfDev;
            // Ein schon angelegtes Bild hat die alte Breite -- neu anlegen,
            // sonst gilt der Schalter erst nach der naechsten Groessen-
            // aenderung.
            if (!m_waterfall.isNull()) {
                m_waterfall = QImage();
                stelleWasserfallbildSicher();
            }
        }
    }
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
        // Longpath-original — distinct peak trace colour so it stays visible
        // when the data-line colour is changed (e.g. Smooth Defaults paints
        // the live trace pure white). Default gold (#FFD700FF).
        m_activePeakHoldColor = ColorSwatchButton::colorFromHex(
            s.value(QStringLiteral("DisplayActivePeakHoldColor"),
                    QStringLiteral("#c2924fFF")).toString());

        // Peak Blobs — Longpath ships disabled by default (deviation from
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
    m_wfAgcEnabled = readBool(QStringLiteral("DisplayWfAgc"), true);
    // Task 2.8: NF-AGC settings (DisplayWfReverseScroll key intentionally not
    // read here — W5 removed; key migration handled in Task 5.1).
    m_wfNfAgcEnabled = readBool(QStringLiteral("WaterfallNFAGCEnabled"), false);
    m_wfNfAgcOffsetDb = readInt(QStringLiteral("WaterfallAGCOffsetDb"), 0);
    m_wfStopOnTx = readBool(QStringLiteral("WaterfallStopOnTx"), false);
    m_wfOpacity          = readInt(QStringLiteral("DisplayWfOpacity"), 100);
    m_wfUpdatePeriodMs   = readInt(QStringLiteral("DisplayWfUpdatePeriodMs"), 30);

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
    {
        const int sec = readInt(QStringLiteral("DisplayWfTimeMarkerSec"), 0);
        m_wfTimeMarkerSec = wfTimeMarkerChoices().contains(sec) ? sec : 0;
    }
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
    m_gridStepDb = qBound(0, readInt(QStringLiteral("DisplayGridStepDb"), 0), 40);
    m_showZeroLine = readBool(QStringLiteral("DisplayShowZeroLine"), false);
    m_showFps = readBool(QStringLiteral("DisplayShowFps"), false);
    // B8 Task 21: cursor frequency readout persists across restarts.
    m_showCursorFreq = readBool(QStringLiteral("DisplayShowCursorFreq"), true);
    // Abstimmhilfe: aus als Vorgabe, wie ShowTuneGuides bei AetherSDR
    // (SpectrumWidget.h:1760 [@0cd4559] m_showTuneGuides{false}). Eine
    // Linie, die am Zeiger klebt, gehoert nicht ungefragt ins Bild.
    m_tuneGuideEnabled = readBool(QStringLiteral("DisplayShowTuneGuide"), false);
    // Squelch-Automatik: aus als Vorgabe. Sie schreibt eine Schwelle in
    // das Modell, und das tut nichts ungefragt.
    m_autoSquelchEnabled = readBool(QStringLiteral("DisplayAutoSquelch"), false);
    // Vorgabe true: das ist unser Istzustand seit je, nicht AetherSDRs aus.
    m_extendedFrequencyLine =
        readBool(QStringLiteral("DisplayExtendedFrequencyLine"), true);
    m_showSignalHistory =
        readBool(QStringLiteral("DisplayShowSignalHistory"), false);
    m_showSignalHistoryQrm =
        readBool(QStringLiteral("DisplayShowSignalHistoryQrm"), false);
    m_signalHistory.setQrmGateSeconds(
        readInt(QStringLiteral("DisplaySignalHistoryQrmGateS"), 6));
    m_signalHistory.setLifetimeSeconds(
        readInt(QStringLiteral("DisplaySignalHistoryLifetimeS"), 60));
    m_extendedPassband =
        readBool(QStringLiteral("DisplayExtendedPassband"), true);
    m_autoSqlMarginDb = qBound(5,
        readInt(QStringLiteral("DisplayAutoSqlMarginDb"), 10), 20);
    m_dbmScaleVisible = readBool(QStringLiteral("DisplayDbmScaleVisible"), true);
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
    // Die Kurve wird nicht unter einem eigenen Schlüssel gespeichert —
    // sie kommt bei jedem Laden frisch aus dem Theme. Ändert jemand sie
    // im Setup, gilt das für die Sitzung; das war vor dieser Änderung
    // genauso, nur mit Neonzyan als festem Ausgangspunkt.
    m_fillColor = QColor(Style::role("trace", Style::kSpectrumTrace));

    // Der eine Spot-Ton, aus derselben Rolle wie die Messwerte. Ein
    // Spot sagt dasselbe wie ein Zeigerausschlag: hier ist etwas.
    m_spotColor = QColor(Style::role("measured", Style::kAmberText));

    // Vorgabe aus dem Theme, nicht aus dem Kopf der Klasse. Wer sich im
    // Setup eine eigene Gitterfarbe ausgesucht hat, behält sie — der
    // gespeicherte Wert schlägt die Vorgabe wie zuvor.
    {
        const QColor grid(Style::role("grid", Style::kSpectrumGrid));
        auto a = [](QColor c, int v) { c.setAlpha(v); return c; };
        m_gridColor     = readColor(QStringLiteral("DisplayGridColor"),
                                    a(grid, 40));
        m_gridFineColor = readColor(QStringLiteral("DisplayGridFineColor"),
                                    a(grid, 20));
        m_hGridColor    = readColor(QStringLiteral("DisplayHGridColor"),
                                    a(grid, 40));
        m_gridTextColor = readColor(QStringLiteral("DisplayGridTextColor"),
                                    QColor(Style::role(
                                        "grid-text", Style::kSpectrumGridText)));
        m_wfLowColor    = readColor(QStringLiteral("DisplayWfLowColor"),
                                    QColor(Qt::black));
    }
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
    // RX1 scope dropped; Longpath applies as global panadapter default.
    m_adjustGridMinToNF = s.value(QStringLiteral("DisplayAdjustGridMinToNoiseFloor"),
                                  QStringLiteral("False")).toString() == QStringLiteral("True");
    m_nfOffsetGridFollow = s.value(QStringLiteral("DisplayNFOffsetGridFollow"),
                                   QStringLiteral("0")).toInt();
    m_nfOffsetGridFollow = qBound(-60, m_nfOffsetGridFollow, 60);
    m_maintainNFAdjustDelta = s.value(QStringLiteral("DisplayMaintainNFAdjustDelta"),
                                      QStringLiteral("False")).toString() == QStringLiteral("True");
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
    s.setValue(settingsKey(QStringLiteral("DisplayOverlayPanelExpanded"), m_panIndex),
              m_overlayPanelExpanded ? QStringLiteral("True") : QStringLiteral("False"));
    writeInt(QStringLiteral("DisplayWfColorScheme"), static_cast<int>(m_wfColorScheme));
    writeInt(QStringLiteral("DisplaySpectrumRenderMode"), static_cast<int>(m_renderMode));
    s.setValue(settingsKey(QStringLiteral("Display3DTrueScale"), m_panIndex),
              m_dss3dTrueScale ? QStringLiteral("True") : QStringLiteral("False"));
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
    s.setValue(settingsKey(QStringLiteral("DisplaySpectrumHeatmap"), m_panIndex),
              m_heatmapEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayTraceSoftEdge"), m_panIndex),
              m_traceSoftEdge ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayTraceHalo"), m_panIndex),
              m_traceHalo ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayWaterfallDevicePixels"), m_panIndex),
              m_wfDevicePixels ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayGridHairline"), m_panIndex),
              m_gridHairline ? QStringLiteral("True") : QStringLiteral("False"));
    writeFloat(QStringLiteral("DisplayTraceEdgePx"), m_traceEdgePx);
    s.setValue(settingsKey(QStringLiteral("DisplayPassbandLabel"), m_panIndex),
              m_zeigeBandbreite ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplaySignalBrackets"), m_panIndex),
              m_zeigeKlammern ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayGapFinder"), m_panIndex),
              m_zeigeLuecken ? QStringLiteral("True") : QStringLiteral("False"));
    writeInt(QStringLiteral("DisplayPassbandMagnifier"), m_lupenModus);
    s.setValue(settingsKey(QStringLiteral("DisplaySUnitsScale"), m_panIndex),
              m_sStufenSkala ? QStringLiteral("True") : QStringLiteral("False"));

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
    writeInt(QStringLiteral("DisplayWfTimeMarkerSec"), m_wfTimeMarkerSec);
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
    s.setValue(settingsKey(QStringLiteral("DisplayGridStepDb"), m_panIndex), m_gridStepDb);
    s.setValue(settingsKey(QStringLiteral("DisplayShowZeroLine"), m_panIndex),
              m_showZeroLine ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayShowFps"), m_panIndex),
              m_showFps ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayShowCursorFreq"), m_panIndex),
              m_showCursorFreq ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayShowTuneGuide"), m_panIndex),
              m_tuneGuideEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayAutoSquelch"), m_panIndex),
              m_autoSquelchEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    writeInt(QStringLiteral("DisplayAutoSqlMarginDb"), m_autoSqlMarginDb);
    s.setValue(settingsKey(QStringLiteral("DisplayExtendedFrequencyLine"), m_panIndex),
              m_extendedFrequencyLine ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayExtendedPassband"), m_panIndex),
              m_extendedPassband ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayShowSignalHistory"), m_panIndex),
              m_showSignalHistory ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(settingsKey(QStringLiteral("DisplayShowSignalHistoryQrm"), m_panIndex),
              m_showSignalHistoryQrm ? QStringLiteral("True") : QStringLiteral("False"));
    writeInt(QStringLiteral("DisplaySignalHistoryQrmGateS"),
             m_signalHistory.qrmGateSeconds());
    writeInt(QStringLiteral("DisplaySignalHistoryLifetimeS"),
             m_signalHistory.lifetimeSeconds());
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
    writeColor(QStringLiteral("DisplayWfLowColor"),    m_wfLowColor);
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
    // RX1 scope dropped; Longpath applies as global panadapter default.
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
    //   adapter: Longpath uses Hz throughout; threshold expressed as a
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

        // ── Longpath divergence: clear history on largeShift to keep the
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
#ifdef LONGPATH_GPU_SPECTRUM
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

    // ── Jeden gesetzten Ausschnitt melden (2026-10-27) ───────────────
    //
    // Am Geraet stand „PANADAPTER · 14.105191 MHz" ueber einer Skala
    // von 7,13 bis 7,21 MHz. Kopfzeile und Skala lesen DENSELBEN Wert
    // (`m_centerHz`, ueber hzToX bzw. ueber dieses Signal) — sie
    // koennen also nur auseinanderlaufen, wenn eine Aenderung das
    // Signal nicht erreicht. Welche, zeigt diese Zeile.
    //
    // qCInfo, nicht qCDebug: die Kategorie steht auf QtInfoMsg, eine
    // Debug-Zeile waere stumm, und eine stumme Messung ist keine. Sie
    // kostet nichts — ein Ausschnitt wird gesetzt, wenn der Betreiber
    // zoomt oder das Band wechselt, nicht je Bildrahmen.
    qCInfo(lcSpectrum).nospace()
        << "Ausschnitt gesetzt auf \"" << m_persistKey << "\": "
        << (m_centerHz / 1.0e6) << " MHz, " << (m_bandwidthHz / 1.0e3)
        << " kHz";

    emit frequencyRangeChanged(m_centerHz, m_bandwidthHz);
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

void SpectrumWidget::setFilterOffset(int lowHz, int highHz)
{
    m_filterLowHz = lowHz;
    m_filterHighHz = highHz;
#ifdef LONGPATH_GPU_SPECTRUM
    markOverlayDirty();
#else
    update();
#endif
}

void SpectrumWidget::setDbmRange(float minDbm, float maxDbm)
{
    // ── Die Spanne darf nicht auf null zulaufen ──────────────────────
    //
    // m_dynamicRange ist der Nenner jeder Umrechnung von dBm nach
    // Bildpunkt (dbmToY, dbmToYf). Wird er null, teilt die Anzeige durch
    // null; wird er negativ, steht die Skala auf dem Kopf und jede Kurve,
    // Marke und Gitterlinie sitzt falsch.
    //
    // Erreichbar war das über die NF-Gitternachführung
    // (onNoiseFloorChanged): sie schiebt das Minimum auf den
    // Rauschflur, lässt das Maximum bei maintainNFAdjustDelta=False aber
    // stehen. Auf einem lauten Band wandert das Minimum dann nach oben,
    // bis es das Maximum erreicht — ohne dass irgendetwas es aufhielt.
    // Ein ruhiges Band oder eine fehlende Antenne sind ungefährlich,
    // dort sinkt der Flur und die Spanne wird groesser.
    //
    // Der Schutz sitzt hier, weil hier alle durchkommen: Ziehen an der
    // Skala, der Copy-Knopf, Setup und die Nachführung.
    if (minDbm > maxDbm) {
        std::swap(minDbm, maxDbm);
    }
    if (maxDbm - minDbm < kMinSpanDb) {
        minDbm = maxDbm - kMinSpanDb;
    }

    m_refLevel = maxDbm;
    m_dynamicRange = maxDbm - minDbm;
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
}

void SpectrumWidget::setSpectrumRenderMode(SpectrumRenderMode mode)
{
    if (m_renderMode == mode) { return; }
    m_renderMode = mode;
    scheduleSettingsSave();
    emit spectrumRenderModeChanged(mode);

    // Start the 3D surface's ring buffer empty rather than carrying rows
    // pushed while it was off (and possibly against a since-changed
    // frequency frame -- this port has no reprojection-on-pan yet, see
    // DssRenderer.h). Also frees the CPU work of building a QImage nobody
    // sees while the mode is off.
    m_dss.clear();
    m_dss3dNeedsUpload = true;
    update();
}

void SpectrumWidget::setDss3dTrueScale(bool on)
{
    if (m_dss3dTrueScale == on) { return; }
    m_dss3dTrueScale = on;
    m_dss.setBackWidthFrac(on ? DssRenderer::kTrueScaleWidthFrac
                              : DssRenderer::kBackWidthFrac);
    scheduleSettingsSave();
    // The stored rows stay valid -- only their geometry changes -- so the
    // surface redraws from the same history instead of starting empty.
    m_dss3dNeedsUpload = true;
    update();
}

void SpectrumWidget::setWfColorScheme(WfColorScheme scheme)
{
    if (m_wfColorScheme == scheme) { return; }
    m_wfColorScheme = scheme;
    scheduleSettingsSave();

    // Die sichtbaren Zeilen neu einfärben. Ohne das galt das neue
    // Schema erst ab der nächsten Zeile, und quer über den Wasserfall
    // lief ein Streifen dort, wo umgeschaltet wurde — bis er nach ein
    // paar Minuten unten hinausgelaufen war.
    //
    // Möglich ist es erst, seit die Historie Intensität hält statt
    // Farbe. Vorher gab es nichts, woraus man hätte neu färben können.
    rebuildWaterfallViewport();
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

void SpectrumWidget::setOverlayPanelExpanded(bool expanded)
{
    if (m_overlayPanelExpanded == expanded) { return; }
    m_overlayPanelExpanded = expanded;
    scheduleSettingsSave();
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
// RX1 scope dropped — Longpath applies as global panadapter default
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
// mapped to detector descriptions. Longpath performs bin reduction in
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
    // CPU-only builds (Linux without LONGPATH_GPU_SPECTRUM).
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
    update();  // Fuellung: Verlauf statt flach, beim naechsten Rahmen
}

void SpectrumWidget::setTraceSoftEdge(bool on)
{
    if (m_traceSoftEdge == on) {
        return;
    }
    m_traceSoftEdge = on;
    scheduleSettingsSave();
    update();  // Streifenzahl aendert sich beim naechsten Rahmen
}

void SpectrumWidget::setTraceHalo(bool on)
{
    if (m_traceHalo == on) {
        return;
    }
    m_traceHalo = on;
    scheduleSettingsSave();
    update();
}

// ── Schaerfe (2026-11-08) ────────────────────────────────────────────
// Erst Entwurfsschalter fuer die Blaetter, dann vom Betreiber gewaehlt
// („Alles zusammen"): alle drei AN ab Werk und je Panadapter gespeichert
// (DisplayWaterfallDevicePixels / DisplayGridHairline / DisplayTraceEdgePx).

void SpectrumWidget::setWaterfallDevicePixels(bool on)
{
    if (m_wfDevicePixels == on) {
        return;
    }
    m_wfDevicePixels = on;
    scheduleSettingsSave();
    // Neue Bildbreite: das Bild verwerfen und beim naechsten Bedarf in
    // der richtigen Groesse anlegen (stelleWasserfallbildSicher). Der
    // Verlauf geht dabei verloren, genau wie bei einer Groessenaenderung.
    m_waterfall = QImage();
    stelleWasserfallbildSicher();
    update();
}

void SpectrumWidget::setGridHairline(bool on)
{
    if (m_gridHairline == on) {
        return;
    }
    m_gridHairline = on;
    scheduleSettingsSave();
    markOverlayDirty();
    update();
}

void SpectrumWidget::setTraceEdgePx(float px)
{
    px = qBound(0.25f, px, 2.0f);
    if (qFuzzyCompare(m_traceEdgePx, px)) {
        return;
    }
    m_traceEdgePx = px;
    scheduleSettingsSave();
    update();
}

// ── Durchlass und Bandbreite (2026-11-08) ────────────────────────────
// Begruendung und Messkern: gui/spectrum/Durchlass.h. Die Zahl am
// Durchlass liegt in der statischen Schicht (sie aendert sich mit
// VFO/Filter), Klammern, Luecken und Lupe in der dynamischen (sie
// aendern sich mit dem Spektrum).

void SpectrumWidget::setPassbandLabelShown(bool on)
{
    if (m_zeigeBandbreite == on) { return; }
    m_zeigeBandbreite = on;
    scheduleSettingsSave();
    markOverlayDirty();
    update();
}

void SpectrumWidget::setSignalBracketsShown(bool on)
{
    if (m_zeigeKlammern == on) { return; }
    m_zeigeKlammern = on;
    m_klammerHaltung.leeren();
    scheduleSettingsSave();
#ifdef LONGPATH_GPU_SPECTRUM
    m_overlayDynamicDirty = true;
#endif
    update();
}

void SpectrumWidget::setGapFinderShown(bool on)
{
    if (m_zeigeLuecken == on) { return; }
    m_zeigeLuecken = on;
    m_luecken.clear();
    m_lueckenKlick.clear();
    m_besteLuecke = -1;
    scheduleSettingsSave();
#ifdef LONGPATH_GPU_SPECTRUM
    m_overlayDynamicDirty = true;
#endif
    update();
}

void SpectrumWidget::setSUnitsScaleShown(bool on)
{
    if (m_sStufenSkala == on) { return; }
    m_sStufenSkala = on;
    scheduleSettingsSave();
    markOverlayDirty();
    update();
}

// Die Striche der S-Skala: Beschriftung und der ROHwert, den dbmToY()
// bekommt. dbmToY schlaegt den Kalibrierversatz auf; der Strich fuer die
// Stufe s soll aber dort stehen, wo ein Signal mit dem KALIBRIERTEN
// Antennenpegel dbmFuer(s) liegt -- also Rohwert = dbmFuer(s) - Versatz.
// So zeigt ein S9-Traeger im Spektrum auf den S9-Strich, wie das S-Meter.
QVector<QPair<QString, float>> SpectrumWidget::sStufenStriche() const
{
    const bool vhf = SMeter::ueberGrenze(rfFuerAnzeige(m_vfoHz) / 1.0e6);
    QVector<QPair<QString, float>> striche;
    for (int s : {1, 3, 5, 7, 9}) {
        striche.push_back({QStringLiteral("S%1").arg(s),
                           static_cast<float>(SMeter::dbmFuer(s, vhf) - m_dbmCalOffset)});
    }
    for (int plus : {10, 20, 40, 60}) {
        striche.push_back({QStringLiteral("+%1").arg(plus),
                           static_cast<float>(SMeter::dbmFuer(9.0 + plus / 6.0, vhf)
                                              - m_dbmCalOffset)});
    }
    return striche;
}

void SpectrumWidget::setPassbandMagnifier(int modus)
{
    modus = (modus != 0) ? 1 : 0;
    if (m_lupenModus == modus) { return; }
    m_lupenModus = modus;
    m_lupeGlatt.clear();
    m_lupeErster = -1;
    m_lupeHoeheDa = false;
    scheduleSettingsSave();
#ifdef LONGPATH_GPU_SPECTRUM
    m_overlayDynamicDirty = true;
#endif
    update();
}

void SpectrumWidget::setHeatmapEnabled(bool on)
{
    if (m_heatmapEnabled == on) {
        return;
    }
    m_heatmapEnabled = on;
    scheduleSettingsSave();
    update();  // Kurvenfarbe: nach Pegel statt flach, beim naechsten Rahmen
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
    // Der Rückfallwert MUSS der Vorgabe der Maschine folgen. Er stand
    // als 4096 hier fest, während die Vorgabe auf 16384 ging — und dann
    // meldet die Anzeige vor dem ersten Bild eine Bandbreite, die es
    // nicht gibt.
    const int fftSz = m_fullLinearBins.isEmpty() ? kDefaultFftSize
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
// because Longpath's renderer doesn't share the averaging loop.
void SpectrumWidget::processNoiseFloor()
{
    // The noise floor is a MEASUREMENT, so it reads the undented pixels
    // (design section 8.3).  Upstream does the same: its accumulator takes
    // max_copy from the pristine array while everything else in that loop
    // takes the dented max - display.cs:5256-5259 [v2.10.3.15].
    const QVector<float>& src = measurementPixels();
    const int width = src.size();
    if (width <= 0) { return; }

    // Per-pixel accumulator — Thetis display.cs:5253-5258 [v2.10.3.13]:
    //   if (!mox && max_copy < currentAverage) {
    //       averageSum += fastPow10Raw(max_copy);
    //       averageCount++;
    //   }
    // ── Ein Sprung ist kein Kriechen (2026-10-23) ───────────────────
    //
    // Betreiber: „dauert noch immer bis oben der wasserfall angezeigt
    // wird", und davor „oben alles schwarz" — nach dem Verbinden steht
    // die 3D-Flaeche lange in Orange, bis sie umspringt.
    //
    // Es ist nicht die Anzeige, es ist diese Schaetzung. Findet die
    // Schleife unten zu wenige ruhige Punkte, kriecht sie mit 1 dB je
    // Rahmen hoch (3 dB im Schnellanlauf) — Thetis display.cs:5893,
    // und fuer eine Abweichung von ein paar dB auch richtig. Beim
    // Verbinden ist die Abweichung aber keine Abweichung, sondern eine
    // andere Welt: der Startwert liegt auf dem Merkwert -200 dBm, und
    // bis der erste Rahmen kommt melden die leeren Rahmen -173 dBm.
    // Von dort auf einen echten Teppich um -98 sind es siebzig bis
    // hundert Rahmen. Solange liegt der Farbanker viel zu tief und
    // ALLES steht am oberen Ende der Rampe: Orange.
    //
    // Also: liegt die Schaetzung mehr als 20 dB unter dem RUHIGEN
    // Mittel dieses Rahmens, dann springen statt kriechen. 20 dB ist
    // weit mehr, als ein belegtes Band den ruhigen Teil anhebt (auf
    // 80 m gemessen rund 9 dB), und weit weniger als der Abstand beim
    // Verbinden. Die Regelung darunter bleibt unveraendert; das hier
    // setzt nur den Ausgangspunkt, von dem sie nachzieht.
    //
    // Das ruhige Mittel, nicht das volle: ein einzelnes starkes
    // Signal darf den Ausgangspunkt nicht hochziehen. Zwei Durchgaenge,
    // wie beim Teppich der Flex-Kachel.
    {
        double summe = 0.0;
        for (int i = 0; i < width; ++i) { summe += src[i]; }
        const double mittel = summe / width;
        double tiefSumme = 0.0; int tiefZahl = 0;
        for (int i = 0; i < width; ++i) {
            if (src[i] <= mittel) { tiefSumme += src[i]; ++tiefZahl; }
        }
        const float ruhig = tiefZahl > 0
            ? static_cast<float>(tiefSumme / tiefZahl)
            : static_cast<float>(mittel);
        if (std::isfinite(ruhig) && ruhig > -199.0f && ruhig < 60.0f
            && m_nfFftBinAverage < ruhig - 20.0f) {
            m_nfFftBinAverage = ruhig;
            m_nfLerpAverage   = ruhig;
        }
    }

    // currentAverage = previous-frame fftBinAverage (the running estimate);
    // bins below it are the "quiet" ones we want to characterise.
    const float currentAverage = m_nfFftBinAverage;
    double averageSum = 0.0;
    int    averageCount = 0;
    for (int i = 0; i < width; ++i) {
        const float dB = src[i];
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

    // Squelch-Automatik am geglaetteten Wert, nicht am Rahmenwert: die
    // Schwelle soll dem folgen, was als NF-Linie steht, und nicht bei
    // jedem Rahmen zappeln. m_nfLerpAverage ist derselbe Wert, den die
    // Anzeige nimmt (siehe die Notiz in onNoiseFloorChanged).
    updateAutoSquelch(m_nfLerpAverage);

    // S-Verlauf am selben Wert, aus demselben Grund: der Erkenner misst
    // „6 dB ueber dem Boden", und gemeint ist der Boden, den die NF-Linie
    // zeigt. Ausgeschaltet kehrt die Funktion sofort zurueck.
    updateSignalHistory(m_nfLerpAverage);
}

// ---- NF-aware grid (Task 2.9) ----
// From Thetis setup.cs:24202-24213 [v2.10.3.13]
// — RX1 scope dropped; Longpath applies as global panadapter default
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
// Longpath adaptation: offset is added rather than subtracted (default 0 vs Thetis
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
                // Die Achse ist die ZF, der Text nennt die Antenne: mit
                // Transverter stand hier bis 2026-11-08 „@ 28.0498 MHz"
                // unter einem 2-m-Bild. Ohne Transverter die Gleichheit.
                m_peakTextCache = QStringLiteral("Peak: ")
                    + QString::number(static_cast<double>(peakDbm), 'f', 1)
                    + QStringLiteral(" dBm @ ")
                    + QString::number(rfFuerAnzeige(peakHz) / 1.0e6, 'f', 4)
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
// Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
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
// Longpath-native (Thetis uses separate per-feature draw calls in DX2D renderer).
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
    m_wfAgcPrimed = false;
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setClarityActive(bool on)
{
    m_clarityActive = on;
    if (!on) {
        // Beim naechsten Einschalten frisch am Bild verankern, nicht mit
        // einem Boden von vor Minuten (anderes Band, anderer Zustand).
        m_wfPixelFloorDbm = std::numeric_limits<float>::quiet_NaN();
    }
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

// Issue #230 fix: Clarity is a Longpath-only override modeled on
// Thetis's AGC pattern at display.cs:6584 [v2.10.3.13], where the AGC
// running-min is a runtime field (_RX1waterfallPreviousMinValue) that
// flows into per-render locals — never the persisted user fields.
// Previously the Clarity controller called setWfLow/HighThreshold,
// which scheduled a settings save on every tick and silently
// overwrote the user's saved thresholds.
void SpectrumWidget::setClarityWaterfallThresholds(float low, float high)
{
    // Ohne Boden gibt es nichts zu verankern: die Schwellen gelten so,
    // wie Clarity sie schickt (das bisherige Verhalten).
    m_clarityLow      = low;
    m_clarityHigh     = high;
    m_clarityFloorDbm = std::numeric_limits<float>::quiet_NaN();
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

void SpectrumWidget::setClarityWaterfallThresholds(float low, float high,
                                                   float floorDbm)
{
    const float prevFloor = m_clarityFloorDbm;
    setClarityWaterfallThresholds(low, high);
    if (!std::isfinite(floorDbm)) { return; }
    // Springt Clarity weit (Bandwechsel, Neuabstimmung, Zustandswechsel
    // des Geraets), gilt der alte Bildpunkt-Boden nicht mehr: neu
    // ansetzen statt sekundenlang hinterherzugleiten.
    constexpr float kReprimeJumpDb = 6.0f;
    if (std::isfinite(prevFloor)
        && std::abs(floorDbm - prevFloor) > kReprimeJumpDb) {
        m_wfPixelFloorDbm = std::numeric_limits<float>::quiet_NaN();
    }
    m_clarityFloorDbm = floorDbm;
    // Bis die naechste Zeile den Bildpunkt-Boden kennt, die Verankerung
    // der vorigen Zeile weiterfuehren statt kurz auf die Bin-Schwellen
    // zurueckzuspringen.
    if (std::isfinite(m_wfPixelFloorDbm)) {
        const float shift = m_wfPixelFloorDbm - m_clarityFloorDbm;
        m_wfActiveLowThreshold  = m_clarityLow  + shift;
        m_wfActiveHighThreshold = m_clarityHigh + shift;
    }
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

const QVector<int>& SpectrumWidget::wfTimeMarkerChoices()
{
    static const QVector<int> kChoices{0, 15, 30, 60, 300, 600, 900};
    return kChoices;
}

void SpectrumWidget::setWfTimeMarkerSeconds(int seconds)
{
    if (!wfTimeMarkerChoices().contains(seconds)) { seconds = 0; }
    if (m_wfTimeMarkerSec == seconds) { return; }
    m_wfTimeMarkerSec = seconds;
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

void SpectrumWidget::setGridStepDb(int db)
{
    db = qBound(0, db, 40);
    if (m_gridStepDb == db) { return; }
    m_gridStepDb = db;
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

void SpectrumWidget::setShowPerfOverlay(bool on)
{
#ifndef LONGPATH_GPU_SPECTRUM
    // Ohne GPU-Pfad gibt es nichts einzuschalten: die Überlagerung wird
    // im GPU-Zweig von paintEvent gemalt, und der ist nicht übersetzt.
    // Ein stiller Nichtstuer statt eines Wächters an der Aufrufstelle
    // in MainWindow.cpp:6181 — die Menüfrage „ist das an?" beantwortet
    // showPerfOverlay() dann mit false, und beides stimmt überein.
    Q_UNUSED(on);
    return;
#else
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
#endif  // LONGPATH_GPU_SPECTRUM
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
void SpectrumWidget::setBandPlanManager(Longpath::BandPlanManager* mgr)
{
    if (m_bandPlanMgr == mgr) { return; }
    if (m_bandPlanMgr) {
        disconnect(m_bandPlanMgr, nullptr, this, nullptr);
    }
    m_bandPlanMgr = mgr;
    if (mgr) {
        connect(mgr, &Longpath::BandPlanManager::planChanged,
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

void SpectrumWidget::setWfLowColor(const QColor& c)
{
    if (!c.isValid() || m_wfLowColor == c) { return; }
    m_wfLowColor = c;
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
    // ── Kurve und Gitter kommen aus dem Theme ────────────────────────
    //
    // Vorher standen hier drei feste Werte: Neonzyan für die Kurve,
    // Weiß mit Alpha für das Gitter, Knallgelb für die Skala. Sie sind
    // die zwei größten Flächen im Fenster, und solange sie im Malcode
    // festgenagelt waren, konnte keine Theme-Datei sie erreichen.
    //
    // Style::role() nimmt den Rollennamen aus der Theme-Datei und
    // fällt auf den Wert aus StyleConstants.h zurück, wenn dort nichts
    // steht. Die Deckung des Gitters bleibt hier: fein und grob sind
    // dieselbe Farbe bei anderem Alpha.
    setFillColor(QColor(Style::role("trace", Style::kSpectrumTrace)));

    const QColor grid(Style::role("grid", Style::kSpectrumGrid));
    auto withAlpha = [](QColor c, int a) { c.setAlpha(a); return c; };
    setGridColor(withAlpha(grid, 40));                   // senkrecht, grob
    setGridFineColor(withAlpha(grid, 20));               // 1/5-Schritt
    setHGridColor(withAlpha(grid, 40));                  // waagrecht, dBm
    setGridTextColor(QColor(Style::role("grid-text",
                                        Style::kSpectrumGridText)));
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
// ── KiwiSDR als Anzeigequelle (Stufe 6, 2026-08-23) ─────────────────
//
// Begruendung des Zuschnitts steht an der Erklaerung in der Kopfdatei.
// Hier nur die zwei Stellen, an denen man beim Lesen stolpern koennte.
void SpectrumWidget::setKiwiDisplaySource(bool kiwi)
{
    if (m_kiwiDisplaySource == kiwi) { return; }
    m_kiwiDisplaySource = kiwi;

    // Beim Wechsel wird der Wasserfall geleert. Sonst stuenden im Bild
    // zwei Empfaenger untereinander, mit einer unsichtbaren Naht dort,
    // wo umgeschaltet wurde — und wer spaeter zurueckrollt, liest
    // Zeilen des einen Geraets unter der Beschriftung des anderen.
    clearWaterfallHistory();
    m_spectrumAvenger.clear();
    m_waterfallAvenger.clear();
    update();

    // Gemerkt, sobald dieser Panadapter einen Schluessel hat. Hier und
    // nicht beim Aufrufer: es gibt mehrere Bedienwege, und jeder
    // einzelne haette es sonst wissen muessen.
    if (!m_persistKey.isEmpty()) {
        AppSettings::instance().setValue(
            m_persistKey + QStringLiteral("_KiwiDisplay"),
            kiwi ? QStringLiteral("True") : QStringLiteral("False"));
    }
}

void SpectrumWidget::updateKiwiSpectrumDbm(const QVector<float>& binsDbm,
                                           double lowHz, double highHz)
{
    if (!m_kiwiDisplaySource) { return; }
    if (binsDbm.isEmpty() || highHz <= lowHz) { return; }

    // Der Kiwi-Ausschnitt tritt an die Stelle des DDC-Fensters. Damit
    // rechnet visibleBinRange() genau wie zuvor, und der Betreiber
    // behaelt Zoom und CTUN.
    setDdcCenterFrequency((lowHz + highHz) * 0.5);
    setSampleRate(highHz - lowHz);

    // dBm -> Leistung. Der Versatz ist NULL, nicht m_lastDbmOffset: der
    // Kiwi liefert bereits geeichte Pegel, ein zweiter Versatz waere
    // eine Verfaelschung. updateSpectrumLinear rechnet gleich wieder
    // zurueck; der Umweg kostet einen Durchlauf und erspart eine
    // zweite Rohrleitung durch Detektor, Mittelung und Wasserfall.
    QVector<float> linear(binsDbm.size());
    for (int i = 0; i < binsDbm.size(); ++i) {
        linear[i] = std::pow(10.0f, binsDbm[i] * 0.1f);
    }
    m_feedingFromKiwi = true;
    updateSpectrumLinear(-1, linear, 1.0, 0.0);
    m_feedingFromKiwi = false;
}

void SpectrumWidget::updateSpectrumLinear(int receiverId,
                                          const QVector<float>& binsLinear,
                                          double windowEnb,
                                          double dbmOffset)
{
    // Fuer dbmOverRange() gemerkt: die Umrechnung linear -> dBm
    // braucht denselben Versatz, den der Panadapter selbst benutzt.
    m_lastDbmOffset = dbmOffset;

    // Ankunft stempeln: der Wasserfall entscheidet daran, ob er noch
    // etwas zu zeigen hat (siehe pushWaterfallRow).
    const qint64 vorigeAnkunft = m_lastSpectrumArrivalMs;
    m_lastSpectrumArrivalMs = QDateTime::currentMSecsSinceEpoch();

    // ── Ein wieder anlaufender Strom ist ein Bandwechsel ────────────
    //
    // Der Schnellanlauf der Rauschteppich-Schaetzung haengt bisher an
    // einem Frequenzsprung ueber 500 kHz oder an MOX (MainWindow
    // :6336/:6471). Ein Geraet, das sich gerade VERBINDET, kommt in
    // keiner der beiden vor — dabei ist genau das der Fall, in dem die
    // Schaetzung von nichts auf ein ganzes Band umstellen muss.
    //
    // Nach einer Pause von ueber zwei Sekunden (dieselbe Schwelle, mit
    // der pushWaterfallRow den Strom fuer abgerissen haelt) also den
    // Schnellanlauf setzen. Beim allerersten Rahmen nicht noetig: den
    // setzt processNoiseFloor inzwischen direkt.
    if (vorigeAnkunft > 0
        && m_lastSpectrumArrivalMs - vorigeAnkunft > 2000) {
        setNoiseFloorFastAttack(true);
    }

    Q_UNUSED(receiverId);
    if (binsLinear.isEmpty()) { return; }

    // ── Die Sperre gegen die zweite Quelle ──────────────────────────
    //
    // Steht dieser Panadapter auf KiwiSDR, wird alles verworfen, was
    // NICHT vom Kiwi kommt. Ohne das schriebe der DDC des Geraets
    // weiter in dieselben Puffer, und man saehe ein Flackern zwischen
    // zwei Baendern statt eines Bildes — dasselbe Muster, das den
    // Betreiber auf 40 m an zwei gleichzeitige Frequenzen denken
    // liess.
    if (m_kiwiDisplaySource && !m_feedingFromKiwi) { return; }

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

    // Die 1-Hz-Normierung haengt an der Bin-Breite, also an der Bin-Zahl.
    // Gitter und dBm-Zahlen stehen in der statischen Ueberlagerung, die
    // nur bei Bedarf neu gezeichnet wird -- ohne diesen Anstoss blieben
    // sie nach einem Neuplan der FFT auf der alten Bin-Breite stehen,
    // waehrend die Kurve schon die neue nahm (2026-09-25).
    const bool binCountChanged = (m_fullLinearBins.size() != binsLinear.size());
    m_fullLinearBins = binsLinear;
    m_fftWindowEnb   = qMax(windowEnb, 1e-9);
    if (binCountChanged && m_dispNormalize) {
        markOverlayDirty();
    }

    // Display pixel count -- spectrum panel width minus dBm strip column,
    // in DEVICE pixels. Per Thetis Display.cs:4970 DrawPanadapterDX2D(int
    // W, ...) signature and :4993 nDecimatedWidth = W / m_nDecimation
    // [v2.10.3.13].  Thetis S16 display-side decimation (an additional
    // W / N reduction) is a Phase 2 follow-up; for now we use the full
    // panel width. "Full panel width" must mean actual screen dots, not
    // logical/CSS pixels -- devicePixelRatioF() is already applied this
    // way elsewhere in this file (renderGpuFrame's viewport math, the
    // overlay texture size). Without it, on a 2x (Retina) display this
    // stage decimates down to half as many samples as there are real
    // pixels to show them on, so every GPU trace segment has to stretch
    // across 2 device pixels -- one confirmed contributor to the trace
    // looking coarser/thicker than intended on HiDPI screens (2026-08-26).
    // Test-only override: bypasses both real widget geometry and the
    // real screen's devicePixelRatioF() for tests whose fixtures need an
    // exact, environment-independent displayWidth (see
    // setDisplayWidthOverrideForTest()'s own comment for why).
    const int displayWidth = (m_displayWidthOverrideForTest > 0)
        ? m_displayWidthOverrideForTest
        : qMax(static_cast<int>(std::lround((width() - effectiveStripW()) * devicePixelRatioF())),
               800);

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
        // Breite geaendert (Fenster gezogen, Seitenbereich zu/auf): den
        // laufenden Mittelwert mitnehmen statt neu anfangen -- sonst eine
        // Zeile weit unter dem Clarity-Fenster, die fitThresholdsToData
        // dann rot einfaerbt (WFDIAG 2026-09-25). Siehe resampleTo().
        m_spectrumAvenger.resampleTo(displayWidth);
    }
    Longpath::applySpectrumDetector(m_spectrumDetector,
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

    // ── Was kommt eigentlich herein? (2026-10-22) ────────────────────
    //
    // Alle dreissig Sekunden eine Zeile mit dem Median und dem
    // staerksten Punkt des gezeichneten Spektrums, in dBm. Fuer JEDES
    // Funkgeraet, weil beide Wege hier durchlaufen.
    //
    // Der Grund: am 2026-10-22 meldete der Betreiber „hier ist nichts,
    // es muessten hunderte stationen sein", und die Frage, ob das
    // Geraet nichts hoert oder Longpath nichts zeigt, war nur ueber
    // einen Mitschnitt und eine Auswertung von Hand zu beantworten.
    // Gemessen wurde dann: ueber 461 kHz kein Punkt ueber −98 dBm, bei
    // einem Rauschteppich von −118,5 dBm. Ein 20-m-Band am Abend hat
    // Stationen bei −80 bis −50 dBm.
    //
    // Mit dieser Zeile steht dieselbe Auskunft im Protokoll, und zwei
    // Funkgeraete lassen sich nebeneinander legen, ohne etwas
    // einzubauen.
    if (!m_renderedPixels.isEmpty()) {
        const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
        if (jetzt - m_bandBerichtZuletztMs > 30000) {
            m_bandBerichtZuletztMs = jetzt;
            QVector<float> sortiert = m_renderedPixels;
            std::sort(sortiert.begin(), sortiert.end());
            const float median = sortiert[sortiert.size() / 2];
            const float p99    = sortiert[sortiert.size() * 99 / 100];
            const float spitze = sortiert.last();
            // ── Wo steht die Spitze? (2026-10-23) ────────────────
            //
            // Betreiber: „unten rot, oben nichts … ev. verschoben?"
            // Der Wasserfall kommt bei der Flex aus der KACHEL, die
            // Kurve darueber aus der Bild-FFT — zwei Stroeme mit je
            // eigenem Frequenzbereich. Ob die beiden
            // uebereinanderliegen, sieht man nur, wenn beide sagen,
            // bei WELCHER Frequenz ihre Spitze steht. `m_renderedPixels`
            // liegt in Bildpunkten, die Umrechnung ist also dieselbe
            // wie beim Zeichnen.
            int spitzeX = 0;
            for (int i = 1; i < m_renderedPixels.size(); ++i) {
                if (m_renderedPixels[i] > m_renderedPixels[spitzeX]) {
                    spitzeX = i;
                }
            }
            const double spitzeHz = m_centerHz - m_bandwidthHz * 0.5
                + (m_renderedPixels.size() > 1
                   ? m_bandwidthHz * spitzeX / (m_renderedPixels.size() - 1)
                   : 0.0);
            qCInfo(lcSpectrum).nospace()
                << "Band: Rauschteppich " << median << " dBm, 99. Perzentil "
                << p99 << " dBm, staerkster Punkt " << spitze
                << " dBm (Hub " << (spitze - median) << " dB ueber "
                << (bandwidth() / 1.0e3) << " kHz), Spitze bei "
                << (spitzeHz / 1.0e6) << " MHz (Punkt " << spitzeX
                << " von " << m_renderedPixels.size() << ")";
        }
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
    // ANALYZER_INFO[] in analyzer.c).  DetType + AvMode are independent.
    if (m_wfDisplayLinearPixels.size() != displayWidth) {
        m_wfDisplayLinearPixels.resize(displayWidth);
    }
    if (m_waterfallAvenger.numPixels() != displayWidth) {
        // Breite geaendert (Fenster gezogen, Seitenbereich zu/auf): den
        // laufenden Mittelwert mitnehmen statt neu anfangen -- sonst eine
        // Zeile weit unter dem Clarity-Fenster, die fitThresholdsToData
        // dann rot einfaerbt (WFDIAG 2026-09-25). Siehe resampleTo().
        m_waterfallAvenger.resampleTo(displayWidth);
    }
    Longpath::applySpectrumDetector(m_waterfallDetector,
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

    // Visual notch, waterfall plane.  Thetis re-runs modifyDataForNotches on
    // the waterfall array after `data = current_waterfall_data` (:6567), under
    // the same MOX gate - display.cs:6579-6586 [v2.10.3.15].  A second
    // explicit call here because Longpath keeps the waterfall pixels in
    // their own array, so denting the spectrum plane above does not reach
    // them.
    //
    // No undented waterfall copy: upstream needs one for its per-frame
    // waterfall minimum (dataCopy at display.cs:6741, :6833, :6915, :6954,
    // :7163, :7369, each carrying //[2.10.3]MW0LGE use non notched data), and
    // Longpath has no such tracker - m_wfLowThreshold is a persisted user
    // setting, and pushWaterfallRow is the only consumer of
    // m_wfRenderedPixels.
    if (visualNotchWillDent()) {
        applyVisualNotchDent(m_wfRenderedPixels);
    }

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

    // 3D stacked-trace view -- feed the perspective ring buffer only while
    // that mode is active, so the 2D-only majority of sessions pays
    // nothing for it. Same finalized frame the flat FFT trace renders
    // (dented, avenged, at display-pixel resolution).
    if (m_renderMode == SpectrumRenderMode::Mode3D) {
        m_dss.pushRow(m_renderedPixels);
        m_dss3dNeedsUpload = true;
    }

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
    // Durchlass und Bandbreite (2026-11-08): Klammern und Luecken hoechstens
    // alle 200 ms ausgewertet, die Lupe je Spektrum geglaettet. VOR der
    // GPU-Weiche -- der QPainter-Rueckfall braucht dieselben Werte (der
    // CPU-Bau fand das am 2026-11-08: dort lief beides nie).
    werteDurchlassAus();
    fuehreLupeNach();
    fuehreEichungNach();
    meldeBandLage();

#ifdef LONGPATH_GPU_SPECTRUM
    // Mode3D: die Bandbreite liegt dort in dieser Schicht
    // (paintPassbandOverSurface) und muss mit der Flaeche Schritt
    // halten — ohne diesen Zweig wuerde sie ohne Peak-Hold/Blobs/
    // Rauschflur nie neu gemalt.
    // ── Befund 6 (2026-10-22): das Zweiton-Feld fehlte in dieser Liste ──
    //
    // Die IMD-Marken und das Messfeld werden im GPU-Weg in dieselbe
    // dynamische Schicht gemalt (siehe den Aufruf in der
    // Overlay-Neubau-Schleife). Sie standen aber nicht in dieser
    // Bedingung: ohne Spitzenhalten, Spitzenblasen, Rauschflur oder
    // 3D-Modus wurde die Schicht nie fuer veraltet erklaert -- die Marken
    // blieben also auf dem Stand des Bildes, in dem die Schicht zuletzt
    // aus einem ANDEREN Grund neu gebaut wurde. NereusSDR hat denselben
    // Befund als "GPU gate-off invalidation gap" notiert.
    const bool imdTorOffen = m_moxOverlay && m_testingIMD
                          && m_showIMDMeasurements && m_displayDuplex;
    if (m_activePeakHold.enabled() || m_peakBlobs.enabled()
        || m_showNoiseFloor
        || imdTorOffen
        || lupeSichtbar()
        || m_renderMode == SpectrumRenderMode::Mode3D) {
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

    // Push display-pixel waterfall row -- waterfall AGC + NF-AGC +
    // threshold compute now operate on display pixels per Thetis
    // Display.cs:6713-6738 [v2.10.3.13] (waterfall_data[i] indexed by
    // pixel).
    //
    // 2026-05-25 KG4VCF bench fix: cache the latest row instead of
    // pushing immediately.  m_wfPushTimer (started in the ctor) drains
    // the cache at strictly m_wfUpdatePeriodMs cadence so network-burst
    // FFT delivery does not produce visible scroll stutter.
    m_pendingWfPixelsDbm = m_wfRenderedPixels;
    m_pendingWfPixelsDbmDirty = true;
    m_hasNewSpectrum = true;

    // 2026-05-22 bench fix: signal that a fresh m_renderedPixels frame is
    // available so MainWindow can push the slice's passband peak into the
    // MaxBin detector. See peakDbmInSlicePassband for the rationale.
    emit spectrumFrameRendered();

    // ── Zweiton-Analyse: hier, nicht im Malvorgang (2026-10-22) ──────────
    //
    // Befund 3 aus NereusSDRs PureSignal-Entwurfsblatt vom 2026-10-12.
    // Dieselbe Torbedingung wie beim Malen (Thetis display.cs:5008
    // [v2.10.3.13]) -- aber einmal je Spektrumsbild statt einmal je
    // Neuzeichnung. Faellt das Tor, verfaellt das letzte Ergebnis, damit
    // der Malvorgang keine alten Marken weiterzeichnet.
    if (m_moxOverlay && m_testingIMD && m_showIMDMeasurements
            && m_displayDuplex) {
        analyseImd();
    } else {
        m_imdAnalysisValid = false;
    }
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
    // Deliberate Longpath-specific divergence from the dent-in-place rule
    // (design section 8.3, decision recorded 2026-07-28): this feeds
    // WdspEngine's MaxBin detector and therefore the analog S-Meter. Thetis
    // reads MaxBin from WDSP upstream of its display code (console.cs:46959,
    // dsp.cs:849-850 [v2.10.3.15]), so its visual notch structurally cannot
    // move its meter; ours would if this scanned the dented array. A display
    // preference must not change a measurement.
    const QVector<float>& src = measurementPixels();
    const int n = src.size();
    if (n < 2 || m_bandwidthHz <= 0.0) { return -400.0; }

    const double loHz = m_vfoHz + static_cast<double>(m_filterLowHz);
    const double hiHz = m_vfoHz + static_cast<double>(m_filterHighHz);
    if (hiHz <= loHz) { return -400.0; }

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

    // Re-allocate the size-dependent buffers — but not on every
    // intermediate size of a drag.  The first allocation runs straight
    // away (nothing to draw otherwise); after that the work is deferred
    // until the size has held still for kResizeSettleMs.  See
    // m_resizeSettleTimer for what that costs and saves.
    if (m_waterfall.isNull() || !m_resizeSettleTimer) {
        applyResizeSettled();
    } else {
        m_resizeSettleTimer->start();
    }

    // Reposition VFO flags after resize
    updateVfoPositions();
}

static int specHFromHeight(int widgetH, float spectrumFrac, int chromeH);

// ── Das Bild notfalls jetzt anlegen (2026-10-31) ────────────────────
//
// Betreiber: „warum der wasserfall dort nichts anzeigt weisz ich auch
// nicht" -- und gleich darauf der entscheidende Satz: „einmal klick in
// den panadapter und es funktioniert."
//
// Die Ursache: `m_waterfall` entstand AUSSCHLIESZLICH in
// `applyResizeSettled()`, also erst, wenn eine Groessenaenderung zur
// Ruhe gekommen ist. Kommt das Fenster in gespeicherter Groesse hoch
// und wird nie gezogen, bleibt das Bild null -- und beide
// Zeilenwege verwerfen dann JEDE ankommende Zeile mit
// `if (m_waterfall.isNull()) return;`. Der Strom lief die ganze Zeit
// (im Protokoll: 10 285 Wasserfallzeilen), nur schrieb sie niemand.
//
// Ein Klick loest irgendwann eine Neuberechnung aus, und ab da geht
// es. Genau das hat er beobachtet.
//
// `applyResizeSettled()` legt nur das Bild in der aktuellen Groesse
// an, setzt die Schreibzeile zurueck und merkt einen vollen
// Texturschub vor -- es ist gefahrlos, das beim ersten Bedarf
// nachzuholen statt auf eine Groessenaenderung zu warten.
bool SpectrumWidget::stelleWasserfallbildSicher()
{
    if (!m_waterfall.isNull()) { return true; }
    if (width() <= 0 || height() <= 0) { return false; }
    applyResizeSettled();
    return !m_waterfall.isNull();
}

void SpectrumWidget::applyResizeSettled()
{
    // Recreate waterfall image at new size
    int w = width();
    int h = height();
#ifdef LONGPATH_GPU_SPECTRUM
    // GPU mode: waterfall clips at strip border (strip is in overlay on right edge)
    int wfW = w - effectiveStripW();
#else
    int wfW = w - effectiveStripW();
#endif
    // Schaerfe-Entwurf (2026-11-08): auf Wunsch so viele Spalten, wie der
    // Schirm Punkte hat -- dieselbe Breite, die updateSpectrumLinear als
    // displayWidth rechnet. Dann faellt im Wasserfall keine Spalte mehr
    // zusammen, und die GPU streckt nichts. Die Hoehe bleibt logisch.
    if (m_wfDevicePixels) {
        wfW = static_cast<int>(std::lround(wfW * devicePixelRatioF()));
    }
    // Dieselbe Hoehe, die der Zeichenweg dem Wasserfall gibt (GPU:
    // renderGpuFrame, CPU: paintEvent) -- ueber specHFromHeight, wie dort.
    // Hier stand int(h*(1-frac)) - chrome: auf dem GPU-Weg teilt der die
    // Flaeche NACH Abzug der Leisten, also kamen z. B. bei h=620 340
    // Bildzeilen auf 353 Pixel -- eine krumme Streckung um 1,04, die jede
    // Zeile ueber zwei Pixelreihen verschmierte (2026-09-25).
    int wfH = h - specHFromHeight(h, m_spectrumFrac, kFreqScaleH + kDividerH)
              - kFreqScaleH - kDividerH;
    if (wfW > 0 && wfH > 0 && (m_waterfall.isNull() ||
        m_waterfall.width() != wfW || m_waterfall.height() != wfH)) {
        m_waterfall = QImage(wfW, wfH, QImage::Format_RGB32);
        m_waterfall.fill(m_wfBgFillColor);
        m_wfWriteRow = 0;
#ifdef LONGPATH_GPU_SPECTRUM
        m_wfTexFullUpload = true;
#endif
        // Sub-epic E: schedule history-image rebuild so the ring buffer's
        // QImage tracks m_waterfall's new dimensions. Debounced 250 ms so
        // rapid resize events don't thrash. Matches the intent of unmerged
        // AetherSDR PR #1478 [@2bb3b5c] — see plan §authoring-time #2.
        if (m_historyResizeTimer) {
            m_historyResizeTimer->start();
        }
    }
    // Unconditional, unlike the waterfall block above: the overlays follow
    // the FULL widget rect while the waterfall follows only its own pane,
    // so a drag that changes the widget without changing wfW/wfH still
    // leaves the overlays a size behind.  markOverlayDirty() also calls
    // update(), which is what runs the deferred overlay re-allocation in
    // renderGpuFrame now that resizeSettling() has gone false — and is the
    // repaint the CPU path needs here too, which is why it is not under
    // the GPU guard.
    markOverlayDirty();
}

void SpectrumWidget::paintEvent(QPaintEvent* event)
{
#ifdef LONGPATH_GPU_SPECTRUM
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
    // Derselbe Hintergrund wie im GPU-Zweig. Ein Hintergrund, der
    // nur in einem der beiden Malwege erscheint, ist schlimmer als
    // keiner: dann haengt das Bild an der Grafikkarte.
    paintBackgroundLayer(p, specRect);
    drawGrid(p, specRect);
    drawSquelchLine(p, specRect);
    drawSpectrum(p, specRect);
    drawWaterfall(p, wfRect);
    // TNF notch overlay immediately before the spots, matching upstream's
    // paint order: AetherSDR src/gui/SpectrumWidget.cpp:12903-12904
    // [@c6481cbf] draws drawTnfMarkers then drawSpotMarkers.  No visibility
    // gate: an empty marker list IS the off state, and the master TNF flag
    // recolours the markers rather than hiding them (Thetis
    // display.cs:8704-8707 [v2.10.3.15]).
    //
    // notchSpecRect() rather than the local specRect above: it is the
    // single notch geometry source the hit test also builds from, and the
    // two agree by construction on this path.
    drawNotchMarkers(p, notchSpecRect());
    // Phase 3J-2 Task E1: spot overlay between spectrum/waterfall and the
    // VFO marker so the spot label tick + pill sit on top of the trace
    // but below the slice/VFO marker chrome. Mirrors AetherSDR
    // SpectrumWidget.cpp:3787 [@0cd4559] paint ordering
    // (drawSpotMarkers between drawTnfMarkers and drawSliceMarkers).
    if (m_showSpots || m_showSignalHistory || m_showSignalHistoryQrm) {
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
    // Nach dem Bandplan, damit die Linie ueber dem Streifen liegt.
    drawCompanionMarker(p, specRect);
    // Bandstapel-Marken, auch im GPU-Weg unten (zwei Zeichenwege).
    drawBandStackMarks(p, specRect);
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
    // Danach, nicht davor: die Abstimmhilfe soll ueber der Ablesung
    // liegen, nicht unter ihr.
    drawTuneGuide(p, specRect);

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
    // Both visuals share m_showNoiseFloor in Longpath; Thetis splits this
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

    // Schaerfe-Entwurf (2026-11-08), setGridHairline: ein Geraete-Pixel
    // breit statt eines logischen, und die Lage stetig gerechnet wie die
    // Kurve (dbmToYf, Frequenz ohne Abschneiden), dann auf die MITTE eines
    // Geraete-Pixels gelegt -- mit abgeschaltetem Antialiasing faellt die
    // Linie so genau auf eine Punktreihe. Aus: Wert fuer Wert wie bisher.
    const qreal gridDpr = devicePixelRatioF();
    const qreal gridPenW = m_gridHairline ? 1.0 / gridDpr : 1.0;
    auto aufGeraetepixel = [gridDpr](double logisch) {
        return (std::floor(logisch * gridDpr) + 0.5) / gridDpr;
    };
    auto hzToXf = [this](double hz, const QRect& r) {
        const double lowHz = m_centerHz - m_bandwidthHz / 2.0;
        return r.left() + (hz - lowHz) / m_bandwidthHz * r.width();
    };
    auto senkrecht = [&](double hz) {
        if (m_gridHairline) {
            const qreal x = aufGeraetepixel(hzToXf(hz, specRect));
            p.drawLine(QLineF(x, specRect.top(), x, specRect.bottom() + 1));
        } else {
            const int x = hzToX(hz, specRect);
            p.drawLine(x, specRect.top(), x, specRect.bottom());
        }
    };

    // Horizontal dBm grid lines (major).
    p.setPen(QPen(m_hGridColor, gridPenW));

    float bottom = m_refLevel - m_dynamicRange;
    float step = 10.0f;  // 10 dB steps
    if (m_dynamicRange <= 50.0f) {
        step = 5.0f;
    }
    if (m_gridStepDb > 0) {
        step = static_cast<float>(m_gridStepDb);   // Setup: dB Step
    }

    for (float dbm = bottom + step; dbm < m_refLevel; dbm += step) {
        if (m_gridHairline) {
            const qreal y = aufGeraetepixel(dbmToYf(dbm, specRect));
            p.drawLine(QLineF(specRect.left(), y, specRect.right() + 1, y));
        } else {
            int y = dbmToY(dbm, specRect);
            p.drawLine(specRect.left(), y, specRect.right(), y);
        }
    }

    // Vertical frequency grid lines (major).
    p.setPen(QPen(m_gridColor, gridPenW));

    // Compute a nice frequency step -- dieselbe Regel wie die Skala
    // darunter, damit Linien und Zahlen uebereinander stehen.
    const double freqStep = frequenzSchritt(m_bandwidthHz, specRect.width());

    double startFreq = std::ceil((m_centerHz - m_bandwidthHz / 2.0) / freqStep) * freqStep;
    for (double f = startFreq; f < m_centerHz + m_bandwidthHz / 2.0; f += freqStep) {
        senkrecht(f);
    }

    // Fine (minor) vertical grid at 1/5 step. Drawn in m_gridFineColor.
    p.setPen(QPen(m_gridFineColor, gridPenW, Qt::DotLine));
    const double fineStep = freqStep / 5.0;
    double startFine = std::ceil((m_centerHz - m_bandwidthHz / 2.0) / fineStep) * fineStep;
    for (double f = startFine; f < m_centerHz + m_bandwidthHz / 2.0; f += fineStep) {
        senkrecht(f);
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
    if (m_activePeakHold.enabled() && m_activePeakHold.size() == n) {
        paintActivePeakHoldTrace(p, specRect);
    }

    // Peak Blobs render pass.  Drawn on top of the live trace line.
    // From Thetis Display.cs:5453-5508 [v2.10.3.13].
    if (m_peakBlobs.enabled() && !m_peakBlobs.blobs().isEmpty()) {
        paintPeakBlobs(p, specRect);
    }

    // Durchlass und Bandbreite (2026-11-08) -- im GPU-Weg in der
    // dynamischen Schicht, hier im QPainter-Rueckfall direkt.
    paintSignalKlammern(p, specRect);
    paintLueckenfinder(p, specRect);
    paintDurchlassLupe(p, specRect);

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
        // Longpath uses radius 3 (not 5) because Qt6/QPainter on HiDPI
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
    // toggle in Longpath.
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
#ifdef LONGPATH_GPU_SPECTRUM
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

    // Timestamp overlay on waterfall (Longpath extensions W8/W9).
    if (m_wfTimestampPos != TimestampPosition::None) {
        const QDateTime now = (m_wfTimestampMode == TimestampMode::UTC)
                              ? QDateTime::currentDateTimeUtc()
                              : QDateTime::currentDateTime();
        const QString stamp = now.toString(QStringLiteral("hh:mm:ss"));
        QFont f = p.font();
        f.setPixelSize(11);
        p.setFont(f);
        p.setPen(QColor(Style::kBlueText));
        const int pad = 4;
        const int textW = p.fontMetrics().horizontalAdvance(stamp);
        int x = (m_wfTimestampPos == TimestampPosition::Left)
                ? (wfRect.left() + pad)
                : (wfRect.right() - textW - pad);
        p.drawText(x, wfRect.top() + 12, stamp);
    }

    drawWaterfallTimeMarkers(p, wfRect);
}

// Clock-aligned time markers (idea: AetherSDR #5538). Screen row y of the
// waterfall shows history age (m_wfHistoryOffsetRows + y); the arithmetic
// lives in WaterfallTimeMarkers (testable against a fixed clock), this
// only gathers the visible rows' timestamps and paints.
void SpectrumWidget::drawWaterfallTimeMarkers(QPainter& p, const QRect& wfRect)
{
    if (m_wfTimeMarkerSec <= 0 || wfRect.height() <= 1
        || !m_waterfallHistory.isConfigured() || m_wfHistoryRowCount <= 0) {
        return;
    }
    const int rows = std::min(wfRect.height(), m_waterfall.height());
    if (rows < 2) { return; }
    QVector<qint64> stamps(rows, 0);
    for (int y = 0; y < rows; ++y) {
        const int rowIndex = historyRowIndexForAge(m_wfHistoryOffsetRows + y);
        if (rowIndex >= 0 && rowIndex < m_wfHistoryTimestamps.size()) {
            stamps[y] = m_wfHistoryTimestamps[rowIndex];
        }
    }
    paintWaterfallTimeMarkers(p, wfRect, stamps);
}

void SpectrumWidget::paintWaterfallTimeMarkers(QPainter& p, const QRect& wfRect,
                                               const QVector<qint64>& stamps)
{
    const QTimeZone zone = (m_wfTimestampMode == TimestampMode::UTC)
        ? QTimeZone(QTimeZone::UTC) : QTimeZone(QTimeZone::LocalTime);
    const QVector<WaterfallTimeMarker> marks = WaterfallTimeMarkers::compute(
        stamps, m_wfTimeMarkerSec, zone, Style::kFontCaption + 4);
    if (marks.isEmpty()) { return; }

    QFont f = p.font();
    f.setPixelSize(Style::kFontCaption);
    p.setFont(f);
    const QColor text(Style::kTextScale);
    QColor line = text;
    line.setAlpha(110);
    for (const WaterfallTimeMarker& m : marks) {
        const int sy = wfRect.top() + m.row;
        p.setPen(line);
        p.drawLine(wfRect.left(), sy, wfRect.right(), sy);
        if (m.labelShown) {
            p.setPen(text);
            p.drawText(wfRect.left() + 4, sy - 2, m.label);
        }
    }
}


// ── Frei waehlbarer Hintergrund ──────────────────────────────────────
//
// Port aus AetherSDR SpectrumWidget.cpp:11161-11187 [@0cd4559].
// Uebernommen sind Zuschnitt (KeepAspectRatioByExpanding + mittiger
// Ausschnitt), die Zwischenhaltung nach Groesse und die
// Deckkraft-Rechnung 1 - opacity/100.
//
// Longpath-Abweichung: die Einstellungen laufen ueber AppSettings mit
// PascalCase-Schluesseln statt AetherSDRs settingsKey()-Praefix, weil
// unser Panadapter nicht je Empfaenger persistiert.

void SpectrumWidget::setBackgroundImage(const QString& path)
{
    if (path == m_bgImagePath) { return; }
    m_bgImagePath = path;
    m_bgScaled = {};
    m_bgScaledSize = {};
    if (path.isEmpty()) {
        m_bgImage = {};
    } else {
        m_bgImage = QImage(path);
        if (m_bgImage.isNull()) {
            qCWarning(lcSpectrum) << "Hintergrundbild nicht ladbar:" << path;
        }
    }
    auto& st = AppSettings::instance();
    st.setValue("PanadapterBackgroundImage", m_bgImagePath);
    st.save();
    m_overlayStaticDirty = true;
    update();
}

void SpectrumWidget::setBackgroundBrightness(int pct)
{
    const int v = qBound(100, pct, 300);
    if (m_bgBrightnessPct == v) { return; }
    m_bgBrightnessPct = v;
    AppSettings::instance().setValue("PanadapterBackgroundBrightness",
                                     QString::number(v));
    m_overlayStaticDirty = true;
    update();
}

void SpectrumWidget::setBackgroundOpacity(int pct)
{
    const int use = qBound(0, pct, 100);
    if (use == m_bgOpacity) { return; }
    m_bgOpacity = use;
    auto& st = AppSettings::instance();
    st.setValue("PanadapterBackgroundOpacity", QString::number(m_bgOpacity));
    st.save();
    m_overlayStaticDirty = true;
    update();
}

// ── Zurueck auf den Standard ─────────────────────────────────────────
//
// Der Betreiber, 2026-08-21: „der raster inkl. blau-graue hintergrund
// gefaellt mir beim test sehr gut, bitte setze ihn als standard."
//
// Der Standard WAR schon das Blaugrau (Style::kPanadapterBg, #141e27).
// Was es nicht gab, war ein Weg zurueck: „Grundfarbe…" schreibt die
// gewaehlte Farbe in die Einstellungen, und von da an gewinnt sie —
// gegen den eingebauten Wert und gegen jedes Thema. Bei ihm stand dort
// deckendes Schwarz (#ff000000), und der Panadapter blieb schwarz, egal
// was der Standard sagte.
//
// Eine einmal getroffene Wahl soll gewinnen; nur muss man sie
// zuruecknehmen koennen. Der Schluessel wird geloescht, nicht auf einen
// Wert gesetzt: danach fragt das Laden wieder das Thema, und wer spaeter
// ein anderes Thema waehlt, bekommt dessen Panadapter-Farbe statt einer
// eingefrorenen von heute.
void SpectrumWidget::resetBackgroundFillColor()
{
    auto& st = AppSettings::instance();
    st.remove(QStringLiteral("PanadapterBackgroundFill"));
    st.save();

    m_bgFillColor = QColor(Style::role("panadapter-bg", Style::kPanadapterBg));
    if (!m_bgFillColor.isValid()) {
        m_bgFillColor = QColor(Style::kPanadapterBg);
    }
    m_overlayStaticDirty = true;
    update();
}

void SpectrumWidget::setBackgroundFillColor(const QColor& c)
{
    if (!c.isValid() || c == m_bgFillColor) { return; }
    m_bgFillColor = c;
    auto& st = AppSettings::instance();
    st.setValue("PanadapterBackgroundFill", m_bgFillColor.name(QColor::HexArgb));
    st.save();
    m_overlayStaticDirty = true;
    update();
}

// Mirrors setBackgroundFillColor()/resetBackgroundFillColor() above --
// same "own choice in AppSettings beats the theme role" precedence,
// same persisted-key-removal-not-value reset so a later theme change
// still reaches an operator who never customised this.
void SpectrumWidget::setWaterfallBackgroundFillColor(const QColor& c)
{
    if (!c.isValid() || c == m_wfBgFillColor) { return; }
    m_wfBgFillColor = c;
    auto& st = AppSettings::instance();
    st.setValue("WaterfallBackgroundFill", m_wfBgFillColor.name(QColor::HexArgb));
    st.save();
    m_waterfall.fill(m_wfBgFillColor);
    m_wfTexFullUpload = true;
    update();
}

void SpectrumWidget::resetWaterfallBackgroundFillColor()
{
    auto& st = AppSettings::instance();
    st.remove(QStringLiteral("WaterfallBackgroundFill"));
    st.save();

    m_wfBgFillColor = QColor(roleColor("app-bg", Style::kAppBg));
    if (!m_wfBgFillColor.isValid()) {
        m_wfBgFillColor = QColor(Style::kAppBg);
    }
    m_waterfall.fill(m_wfBgFillColor);
    m_wfTexFullUpload = true;
    update();
}

void SpectrumWidget::loadBackgroundSettings()
{
    auto& st = AppSettings::instance();
    const QString path = st.value("PanadapterBackgroundImage", QString()).toString();
    m_bgOpacity = qBound(0, st.value("PanadapterBackgroundOpacity", "80").toInt(), 100);
    // ── Die Grundflaeche gehoert der Palette ─────────────────────────
    //
    // Die Vorgabe kommt jetzt aus der Rolle „panadapter-bg", nicht mehr
    // direkt aus der Konstanten. Beim ersten Rendern des Hauptfensters
    // in der hellen Palette „Kreide" (2026-08-20) blieb der Panadapter
    // als einzige grosse Flaeche dunkel — ein schwarzes Loch auf hellem
    // Grund. Er ist reiner Malcode und wurde vom ThemeFilter, der nur
    // Stylesheets anfasst, nie erreicht.
    //
    // Eine selbst gewaehlte Farbe (PanadapterBackgroundFill) hat
    // weiterhin Vorrang: sie steht in den Einstellungen und gehoert
    // dem Betreiber, nicht dem Thema.
    const QColor fill(st.value("PanadapterBackgroundFill",
                               Style::role("panadapter-bg",
                                           Style::kPanadapterBg)).toString());
    if (fill.isValid()) { m_bgFillColor = fill; }

    const QColor wfFill(st.value("WaterfallBackgroundFill",
                                 Style::role("app-bg",
                                             Style::kAppBg)).toString());
    if (wfFill.isValid()) { m_wfBgFillColor = wfFill; }

    m_bgBrightnessPct = qBound(100,
        st.value("PanadapterBackgroundBrightness", "100").toInt(), 300);
    if (!path.isEmpty()) {
        m_bgImagePath = path;
        m_bgImage = QImage(path);
        m_bgScaled = {};
        m_bgScaledSize = {};
    }
    m_overlayStaticDirty = true;
}

// Malt Fuellfarbe und Bild in DIESER Reihenfolge in den uebergebenen
// Bereich. Beide Malwege rufen sie, damit GPU und CPU dasselbe zeigen —
// ein Hintergrund, der nur in einem der beiden erscheint, ist schlimmer
// als keiner.
void SpectrumWidget::paintBackgroundLayer(QPainter& p, const QRect& specRect)
{
    p.fillRect(specRect, m_bgFillColor);
    if (m_bgImage.isNull() || specRect.isEmpty()) { return; }
    if (m_bgScaledSize != specRect.size()) {
        const QImage expanded = m_bgImage.scaled(
            specRect.size(), Qt::KeepAspectRatioByExpanding,
            Qt::SmoothTransformation);
        const int cx = (expanded.width()  - specRect.width())  / 2;
        const int cy = (expanded.height() - specRect.height()) / 2;
        m_bgScaled = expanded.copy(cx, cy, specRect.width(), specRect.height());
        m_bgScaledSize = specRect.size();
    }
    const qreal keep = p.opacity();
    p.setOpacity(1.0 - m_bgOpacity / 100.0);
    p.drawImage(specRect.topLeft(), m_bgScaled);

    // ── Aufhellen ────────────────────────────────────────────────────
    //
    // Ein zweiter Durchgang in Weiss mit Plus-Verrechnung. Additiv und
    // nicht multiplikativ: so kommen die dunklen Stellen hoch, waehrend
    // die hellen nur langsam nachziehen — ein Nachtfoto wird lesbar,
    // ohne dass der Himmel ausbrennt.
    //
    // Nur nach oben. Abdunkeln kann man ueber die Fuellfarbe, dafuer
    // braucht es keinen zweiten Weg.
    if (m_bgBrightnessPct > 100) {
        const double lift = (m_bgBrightnessPct - 100) / 100.0;   // 0..1
        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.setOpacity((1.0 - m_bgOpacity / 100.0) * lift * 0.55);
        p.fillRect(specRect, Qt::white);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }
    p.setOpacity(keep);
}


// ── Squelch-Linie ────────────────────────────────────────────────────
//
// Port aus AetherSDR SpectrumWidget.cpp:4453-4506 [@0cd4559].
// Uebernommen sind die Idee (waagerechter Strich auf Schwellenhoehe,
// Beschriftung links) und das Ausblenden nach drei Sekunden.
//
// Nicht uebernommen ist AetherSDRs Stufenrechnung `kSqlMinDbm + level`:
// die stammt daher, dass FlexRadio den Schwellwert als 0..160 meldet.
// Unser SliceModel haelt ihn in dBm, also ist die Umrechnung ueberfluessig
// — und eine ueberfluessige Umrechnung ist eine Fehlerquelle ohne Nutzen.

void SpectrumWidget::setSquelchLine(bool visible, double dbm)
{
    m_squelchDbm = dbm;
    m_squelchLineVisible = visible;

    if (!m_squelchHideTimer) {
        m_squelchHideTimer = new QTimer(this);
        m_squelchHideTimer->setSingleShot(true);
        // Drei Sekunden wie bei AetherSDR. Die Linie beantwortet die
        // Frage „wo steht die Schwelle" beim EINSTELLEN; danach ist sie
        // ein Strich, der zum Raster wird und nichts mehr sagt.
        m_squelchHideTimer->setInterval(3000);
        connect(m_squelchHideTimer, &QTimer::timeout, this, [this] {
            m_squelchLineVisible = false;
            m_overlayStaticDirty = true;
            update();
        });
    }
    if (visible) {
        m_squelchHideTimer->start();
    } else {
        m_squelchHideTimer->stop();
    }
    m_overlayStaticDirty = true;
    update();
}

void SpectrumWidget::drawSquelchLine(QPainter& p, const QRect& specRect)
{
    if (!m_squelchLineVisible || specRect.isEmpty()) { return; }

    const int y = dbmToY(static_cast<float>(m_squelchDbm), specRect);
    if (y < specRect.top() || y > specRect.bottom()) { return; }

    const QColor col = roleColor("warn", Style::kAmberWarn, 220);
    p.setPen(QPen(col, 1));
    p.drawLine(specRect.left(), y, specRect.right(), y);

    QFont f = p.font();
    f.setPixelSize(Style::kFontCaption);
    f.setBold(true);
    p.setFont(f);
    p.setPen(col);
    // Links, zwei Bildpunkte ueber der Linie — wie AetherSDR. Unter der
    // Linie waere die Beschriftung im Signal.
    p.drawText(specRect.left() + 4, y - 2,
               QStringLiteral("SQL %1 dBm").arg(m_squelchDbm, 0, 'f', 0));
}

// ── Abstimmhilfe ─────────────────────────────────────────────────────
//
// Port aus AetherSDR SpectrumWidget.cpp:15046-15074 [@0cd4559]
// (Zeichnung), :9157-9164 (Sichtbarkeit in updateTrackedCursorState)
// und :4068-4090 (Schalter setShowTuneGuides).
//
// Uebernommen: senkrechte Linie am Zeiger, Beschriftung mit der
// Frequenz auf das Hertz genau, Ausblenden nach vier Sekunden Ruhe,
// Unterdrueckung solange der Zeiger ueber einer Kerbe steht.
//
// Nicht uebernommen ist die Weitergabe an Geschwister-Widgets (dort in
// setShowTuneGuides, ueber window() alle SpectrumWidgets): sie loest ein
// Problem, das wir bis Phase 3F nicht haben.

void SpectrumWidget::setTuneGuideEnabled(bool on)
{
    if (m_tuneGuideEnabled == on) { return; }
    m_tuneGuideEnabled = on;
    if (!on) {
        m_tuneGuideShowing = false;
        if (m_tuneGuideHideTimer) { m_tuneGuideHideTimer->stop(); }
    }
    scheduleSettingsSave();
    markOverlayDirty();
    update();
}

void SpectrumWidget::noteTuneGuideActivity()
{
    if (!m_tuneGuideEnabled) { return; }

    // Ueber einer Kerbe nicht: dort steht schon der Kerben-Hinweis
    // (QToolTip in mouseMoveEvent), und zwei Beschriftungen am selben
    // Zeiger ueberdecken sich. AetherSDR entscheidet genauso, dort ueber
    // m_hoveredTnfId < 0 (SpectrumWidget.cpp:9157 [@0cd4559]).
    const bool wanted = (m_hoveredNotchId < 0);

    if (!m_tuneGuideHideTimer) {
        m_tuneGuideHideTimer = new QTimer(this);
        m_tuneGuideHideTimer->setSingleShot(true);
        // Vier Sekunden wie bei AetherSDR (:1986 setInterval(4000)) —
        // laenger als die drei der Squelch-Linie, weil das Abstimmen
        // laenger dauert als das Einstellen einer Schwelle.
        m_tuneGuideHideTimer->setInterval(4000);
        connect(m_tuneGuideHideTimer, &QTimer::timeout, this, [this] {
            m_tuneGuideShowing = false;
            m_overlayStaticDirty = true;
            update();
        });
    }

    if (wanted) {
        m_tuneGuideShowing = true;
        m_tuneGuideHideTimer->start();   // jede Bewegung setzt die Zeit neu
    } else {
        m_tuneGuideShowing = false;
        m_tuneGuideHideTimer->stop();
    }
    m_overlayStaticDirty = true;
}

// ── Squelch-Automatik ────────────────────────────────────────────────
//
// Port aus AetherSDR SpectrumWidget.cpp:4508-4587 [@0cd4559]. Uebernommen
// sind die Idee (Schwelle = Rauschboden + Abstand), der Abstandsbereich
// 5..20 dB und der Vorgabewert 10 dB.
//
// Nicht uebernommen ist AetherSDRs eigener Schaetzer in
// updateAutoSquelchFromBins: wir haben den Rauschboden bereits, und zwar
// den Thetis-treuen aus processNoiseFloor, der auch die NF-Linie malt.
// Begruendung steht im Header; kurz: die Schwelle muss an der sichtbaren
// Linie ablesbar sein.

// ── Verlaengerung in den Wasserfall ──────────────────────────────────
//
// Port aus AetherSDR setExtendedFrequencyLine + setExtendedPassband
// (SpectrumWidget.cpp:4094-4130 [@0cd4559]). Vorgabe bei uns EIN, nicht
// aus — Begruendung im Header.
//
// Nicht uebernommen ist die Weitergabe an Geschwister-Widgets, aus
// demselben Grund wie bei der Abstimmhilfe: bis Phase 3F gibt es genau
// ein Panadapter.

void SpectrumWidget::setExtendedFrequencyLine(bool on)
{
    if (m_extendedFrequencyLine == on) { return; }
    m_extendedFrequencyLine = on;
    scheduleSettingsSave();
    markOverlayDirty();
    update();
}

void SpectrumWidget::setExtendedPassband(bool on)
{
    if (m_extendedPassband == on) { return; }
    m_extendedPassband = on;
    scheduleSettingsSave();
    markOverlayDirty();
    update();
}

void SpectrumWidget::setAutoSquelchEnabled(bool on)
{
    if (m_autoSquelchEnabled == on) { return; }
    m_autoSquelchEnabled = on;
    if (on) {
        // Kaltstart wie bei AetherSDR (:4515 m_lastAutoSquelchLevel = -1):
        // der erste Rauschboden nach dem Einschalten soll senden, auch
        // wenn er dem letzten von vorhin gleicht.
        m_lastAutoSqlDbm = std::numeric_limits<double>::quiet_NaN();
    }
    scheduleSettingsSave();
}

void SpectrumWidget::setAutoSqlMarginDb(int db)
{
    // 5..20 dB wie AetherSDR (:4527 std::clamp(dBm, 5, 20)).
    db = qBound(5, db, 20);
    if (m_autoSqlMarginDb == db) { return; }
    m_autoSqlMarginDb = db;
    // Neu senden, auch wenn der Rauschboden steht: der Abstand hat sich
    // geaendert, also die Schwelle. AetherSDR macht es genauso (:4528).
    m_lastAutoSqlDbm = std::numeric_limits<double>::quiet_NaN();
    scheduleSettingsSave();
}

void SpectrumWidget::updateAutoSquelch(float nfDbm)
{
    if (!m_autoSquelchEnabled) { return; }

    // Beim Senden nicht nachstellen: das eigene Signal hebt den
    // gemessenen Boden, und die Automatik wuerde die Schwelle hochziehen
    // und nach dem Loslassen alles zumachen. AetherSDR prueft dafuer
    // m_transmitting (:4533), unser Aequivalent ist m_moxOverlay.
    if (m_moxOverlay) { return; }

    // -200 dBm ist der Anfangswert von m_nfFftBinAverage / m_nfLerpAverage:
    // solange processNoiseFloor keinen Rahmen gesehen hat, steht dort kein
    // gemessener Boden, sondern die Vorbelegung.
    if (!std::isfinite(nfDbm) || nfDbm <= -199.0f) { return; }

    const double target = static_cast<double>(nfDbm)
                        + static_cast<double>(m_autoSqlMarginDb);

    // Nur bei Bewegung senden. Ohne diese Sperre laeuft bei jedem
    // FFT-Rahmen ein Modell-Setter samt Signalkette und Neuzeichnen der
    // Squelch-Linie — bei 10 bis 30 Rahmen je Sekunde.
    if (std::isfinite(m_lastAutoSqlDbm)
        && std::abs(target - m_lastAutoSqlDbm) < 1.0) {
        return;
    }
    m_lastAutoSqlDbm = target;
    emit autoSquelchThresholdSuggested(target);
}

void SpectrumWidget::drawTuneGuide(QPainter& p, const QRect& specRect)
{
    if (!m_tuneGuideEnabled || !m_tuneGuideShowing) { return; }
    if (specRect.isEmpty() || !m_mouseInWidget) { return; }

    const int cx = m_mousePos.x();
    if (cx < specRect.left() || cx > specRect.right()) { return; }

    const QColor col = roleColor("text-secondary", Style::kTextSecondary, 220);
    p.setPen(QPen(col, 1));
    p.drawLine(cx, specRect.top(), cx, specRect.bottom());

    // Punktgruppen statt MHz mit Nachkommastellen: beim Abstimmen zaehlt
    // das Hertz, und 14.270.000 liest sich schneller als 14.270000 MHz.
    // Format aus AetherSDR SpectrumWidget.cpp:15053-15062 [@0cd4559].
    const long long hz = static_cast<long long>(std::llround(xToHz(cx, specRect)));
    const QString label = QStringLiteral("%1.%2.%3")
        .arg(hz / 1000000)
        .arg((hz / 1000) % 1000, 3, 10, QChar('0'))
        .arg(hz % 1000, 3, 10, QChar('0'));

    QFont f = p.font();
    f.setPixelSize(Style::kFontCaption + 2);
    f.setBold(true);
    p.setFont(f);
    const QFontMetrics fm(f);
    const int tw = fm.horizontalAdvance(label) + 8;
    const int th = fm.height() + 4;

    // Rechts vom Zeiger, sonst links, wenn der Rand naeher ist — und
    // oberhalb, sonst unterhalb. Sonst schiebt sich die Beschriftung am
    // Bildrand aus dem Bild.
    int lx = cx + 12;
    if (lx + tw > specRect.right()) { lx = cx - tw - 4; }
    int ly = m_mousePos.y() - th - 4;
    if (ly < specRect.top()) { ly = m_mousePos.y() + 16; }

    p.fillRect(lx, ly, tw, th, roleColor("panel", Style::kPanelBg, 200));
    p.setPen(roleColor("text-primary", Style::kTextPrimary));
    p.drawText(lx + 4, ly + fm.ascent() + 2, label);
}

// ---- Frequency scale bar ----
// Beschriftung einer Frequenzmarke in MHz, mit so vielen Nachkommastellen,
// wie der Abstand der Marken braucht. Bis 2026-09-25 galten zwei Stellen fuer
// jeden Abstand ab 10 kHz -- beim 25-kHz-Raster (Spanne 100..500 kHz, z. B.
// 192 kHz Abtastrate) stand dann "14.18" an der Marke 14,175 und "14.22" an
// 14,225: die Skala zeigte Frequenzen, die dort nicht liegen.
// ── Teilstrich-Abstand der Frequenzachse (2026-11-08) ───────────────
//
// Raster und Skala hatten je eine eigene, gleich lautende Treppe:
// 5 kHz unter 50 kHz Breite, 10 kHz bis 100 kHz, 25 kHz bis 500 kHz,
// darueber 50 kHz -- und dort hoerte sie auf. Bei 4 MHz (2 m, das ganze
// Fenster des MV-2 an IFo2) waren das 80 Beschriftungen auf 1130 Punkte,
// uebereinander gedruckt und unlesbar; der Flex zeigt bis 14 MHz.
//
// Die alte Treppe bleibt der Anfang -- bis 500 kHz an einem normal
// breiten Fenster steht jede Zahl, wo sie stand. Nur wenn die Zahlen
// enger als kMindestAbstandPx (55) stuenden, geht es auf den naechsten
// runden Schritt.
double SpectrumWidget::frequenzSchritt(double bandwidthHz, int breitePx)
{
    double step = 25000.0;
    if (bandwidthHz > 500000.0) {
        step = 50000.0;
    } else if (bandwidthHz < 50000.0) {
        step = 5000.0;
    } else if (bandwidthHz < 100000.0) {
        step = 10000.0;
    }
    if (bandwidthHz <= 0.0 || breitePx <= 0) {
        return step;
    }
    // 55 Punkte: der engste Abstand, den die alte Treppe selbst ergab
    // (25 kHz bei 500 kHz Breite auf 1130 Punkte = 56,5) -- dort aendert
    // sich also nichts. Eine Zahl wie "144.25" ist bei 11 Pixeln gut 40
    // Punkte breit; es bleibt eine Luecke.
    constexpr double kMindestAbstandPx = 55.0;
    static constexpr double kLeiter[] = {
        5.0e3, 10.0e3, 25.0e3, 50.0e3, 100.0e3, 250.0e3, 500.0e3,
        1.0e6, 2.5e6, 5.0e6, 10.0e6,
    };
    for (double s : kLeiter) {
        if (s < step) { continue; }
        step = s;
        if (breitePx * s / bandwidthHz >= kMindestAbstandPx) { break; }
    }
    return step;
}

QString SpectrumWidget::freqScaleLabel(double hz, double stepHz)
{
    int decimals = 1;
    const double stepMhz = stepHz / 1.0e6;
    while (decimals < 6) {
        const double scaled = stepMhz * std::pow(10.0, decimals);
        if (std::abs(scaled - std::round(scaled)) < 1e-6) {
            break;
        }
        ++decimals;
    }
    return QString::number(hz / 1.0e6, 'f', decimals);
}

void SpectrumWidget::drawFreqScale(QPainter& p, const QRect& r)
{
    p.fillRect(r, QColor(Style::hexRole(Style::kBadgeInfoBg)));

    // Phase 3G-8 commit 5: Off alignment suppresses labels entirely.
    if (m_freqLabelAlign == FreqLabelAlign::Off) {
        return;
    }

    QFont font = p.font();
    font.setPixelSize(11);
    p.setFont(font);
    // From Thetis display.cs:2003 — grid_text_color (now configurable).
    p.setPen(m_gridTextColor);

    const double freqStep = frequenzSchritt(m_bandwidthHz, r.width());

    // Phase 3G-8 commit 5: Thetis 5-mode label alignment.
    // Left / Center / Right / Auto (center with fallback) / Off.
    const Qt::Alignment baseFlags =
        (m_freqLabelAlign == FreqLabelAlign::Left)  ? (Qt::AlignLeft  | Qt::AlignVCenter) :
        (m_freqLabelAlign == FreqLabelAlign::Right) ? (Qt::AlignRight | Qt::AlignVCenter) :
                                                       (Qt::AlignHCenter | Qt::AlignVCenter);

    double startFreq = std::ceil((m_centerHz - m_bandwidthHz / 2.0) / freqStep) * freqStep;
    for (double f = startFreq; f < m_centerHz + m_bandwidthHz / 2.0; f += freqStep) {
        int x = hzToX(f, r);
        // Die Achse beschriftet die Antenne, nicht die ZF. Der
        // Teilungsabstand bleibt in der ZF -- der Versatz verschiebt,
        // er dehnt nicht, also sitzen die Striche weiter richtig.
        const QString label = freqScaleLabel(rfFuerAnzeige(f), freqStep);

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
        // Nur ganz oder gar nicht: eine Zahl, die ueber den Rand der Leiste
        // ragt, wurde rechts vom LIVE-Knopf bzw. dem Randstreifen halb
        // verdeckt ("14.12" statt "14.125", 2026-09-25) -- abgeschnittene
        // Frequenzen sind schlechter als fehlende.
        if (lx < r.left() || lx + labelSize.width() > r.right() + 1) {
            continue;
        }
        p.drawStaticText(QPointF(lx, ly), cit.value());
    }
}

// ---- dBm scale strip ----
// From AetherSDR SpectrumWidget.cpp:4856-4925 [@0cd4559]
void SpectrumWidget::drawDbmScale(QPainter& p, const QRect& specRect)
{
    const QRect strip = Longpath::DbmStrip::stripRect(specRect, kDbmStripW);

    // Semi-opaque background
    p.fillRect(strip, roleColor("panel", Style::kPanelBg, 220));

    // Betreiber, 2026-08-28: dieselbe senkrechte Trennlinie wie im
    // Wasserfall-Zeitstreifen (drawTimeScale) -- beide zusammen wirkten
    // wie eine durchgehende Kante von oben bis unten und sollten weg.
    //
    // ── Up/Down arrows side by side at top ─────────────────────────────
    const int halfW    = kDbmStripW / 2;
    const int upCx     = strip.left() + halfW / 2;          // left half center
    const int dnCx     = strip.left() + halfW + halfW / 2;  // right half center
    const int arrowTop = specRect.top() + 2;
    const int arrowBot = specRect.top() + kDbmArrowH - 2;

    // Glas & Tiefe (2026-09-18): zwei erhabene Chips mit kleinem Pfeil
    // statt zweier grosser Dreiecke in Akzentblau — sie sind Knoepfe
    // (Bereich hoch/runter), und ein Knopf ist im Haus erhaben und
    // leise. Blau bleibt fuer die Auswahl.
    {
        const int chipH = arrowBot - arrowTop + 4;
        const int chipW = halfW - 3;
        const QRect upBox(upCx - chipW / 2, arrowTop - 2, chipW, chipH);
        const QRect dnBox(dnCx - chipW / 2, arrowTop - 2, chipW, chipH);
        Style::paintRaisedChip(p, upBox, 5);
        Style::paintRaisedChip(p, dnBox, 5);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(Style::role("text-secondary", Style::kTextSecondary)));
        const int cy = (arrowTop + arrowBot) / 2;
        QPolygon upTri;
        upTri << QPoint(upCx - 4, cy + 3) << QPoint(upCx + 4, cy + 3) << QPoint(upCx, cy - 3);
        p.drawPolygon(upTri);
        QPolygon dnTri;
        dnTri << QPoint(dnCx - 4, cy - 3) << QPoint(dnCx + 4, cy - 3) << QPoint(dnCx, cy + 3);
        p.drawPolygon(dnTri);
        p.setRenderHint(QPainter::Antialiasing, false);
    }

    // ── dBm labels ───────────────────────────────────────────────────────
    // setPixelSize statt setPointSize: der Hausstil (capsFont() in
    // StyleConstants.h) haelt Beschriftungen auf dem Graphen pixelfest,
    // nicht DPI-abhaengig -- sonst weicht diese Schrift von den
    // Frequenzachsen- und Grid-Beschriftungen ab, die schon setPixelSize
    // verwenden. Betreiber am 2026-09-24.
    QFont f = p.font();
    f.setPixelSize(Style::kFontMicro);
    p.setFont(f);
    const QFontMetrics fm(f);

    const int labelTop = specRect.top() + kDbmArrowH + 4;
    const float stepDb = Longpath::DbmStrip::adaptiveStepDb(m_dynamicRange);

    const float bottomDbm  = m_refLevel - m_dynamicRange;
    const float firstLabel = std::ceil(bottomDbm / stepDb) * stepDb;

    // drawStaticText anchors at the top-left of the glyph run; drawText
    // anchors at the baseline.  The original baseline-y was
    // (y + ascent/2); to keep visual-identical placement, drawStaticText
    // takes y = (y + ascent/2) - ascent = y - ascent/2.
    const qreal staticAscent = fm.ascent();

    if (m_sStufenSkala) {
        // S-Stufen statt dBm (2026-11-08), siehe sStufenStriche().
        for (const auto& strich : sStufenStriche()) {
            const int y = dbmToY(strich.second, specRect);
            if (y < labelTop || y > specRect.bottom() - 5) { continue; }
            p.setPen(QColor(Style::kTextTertiary));
            p.drawLine(strip.left(), y, strip.left() + 4, y);
            auto cit = m_dbmLabelCache.find(strich.first);
            if (cit == m_dbmLabelCache.end()) {
                QStaticText st(strich.first);
                st.setPerformanceHint(QStaticText::AggressiveCaching);
                cit = m_dbmLabelCache.insert(strich.first, std::move(st));
            }
            cit.value().prepare(p.transform(), f);
            // S9 hervorgehoben -- an ihr liest man Rapporte ab.
            p.setPen(strich.first == QLatin1String("S9")
                         ? QColor(Style::kAmberText)
                         : QColor(Style::kSpectrumGridText));
            p.drawStaticText(QPointF(strip.left() + 6, y - staticAscent / 2.0),
                             cit.value());
        }
        return;
    }

    for (float dbm = firstLabel; dbm <= m_refLevel; dbm += stepDb) {
        // Route through dbmToY() so m_dbmCalOffset is applied consistently with
        // the grid/trace/peak-hold paths — otherwise a non-zero cal offset would
        // drift strip ticks off the actual grid lines.
        const int y = dbmToY(dbm, specRect);
        if (y < labelTop || y > specRect.bottom() - 5) continue;

        // Tick mark
        p.setPen(QColor(Style::kTextTertiary));
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

        p.setPen(QColor(Style::kSpectrumGridText));
        p.drawStaticText(
            QPointF(strip.left() + 6, y - staticAscent / 2.0),
            cit.value());
    }
}

// ── ZF und Antenne ──────────────────────────────────────────────────

double SpectrumWidget::rfFuerAnzeige(double ifHz) const
{
    const auto& tv = Longpath::TransverterSetup::current();
    if (!tv.active()) { return ifHz; }
    return static_cast<double>(tv.rfFrequencyHz(std::llround(ifHz)));
}

double SpectrumWidget::zfFuerAchse(double rfHz) const
{
    const auto& tv = Longpath::TransverterSetup::current();
    if (!tv.active()) { return rfHz; }
    return static_cast<double>(tv.ifFrequencyHz(std::llround(rfHz)));
}

// ── Die Marke des Begleitgeraets ────────────────────────────────────

void SpectrumWidget::setCompanionMarker(bool shown, qint64 ifHz,
                                        const QString& label)
{
    if (m_companionShown == shown && m_companionIfHz == ifHz
        && m_companionLabel == label) {
        return;
    }
    m_companionShown = shown;
    m_companionIfHz  = ifHz;
    m_companionLabel = label;
    update();
}

// ── Bandstapel-Marken (2026-11-11, Entwurf D1) ──────────────────────

namespace {
// Masze aus dem gewaehlten Blatt (tests/tst_bandstapel_entwurf.cpp).
constexpr int kBsMarkHoehe = 8;      // Dreieck
constexpr int kBsMarkHalb  = 4;      // halbe Grundlinie
constexpr int kBsTreffer   = 6;      // waagrecht anklickbar, je Seite
constexpr int kBsTrefferH  = 14;     // vom unteren Rand nach oben
constexpr int kBsVfoFrei   = 4;      // die VFO-Marke gewinnt
}

void SpectrumWidget::setBandStackMarks(const QVector<BandStackMark>& marks)
{
    const bool gleich = marks.size() == m_bandStackMarks.size()
        && std::equal(marks.cbegin(), marks.cend(), m_bandStackMarks.cbegin(),
                      [](const BandStackMark& a, const BandStackMark& b) {
                          return a.id == b.id && a.hz == b.hz && a.place == b.place
                              && a.locked == b.locked && a.tip == b.tip;
                      });
    if (gleich) { return; }
    m_bandStackMarks = marks;
    // Die Marken liegen in der zwischengespeicherten Ueberlagerung des
    // GPU-Wegs; ein blosses update() zeichnete sie dort nicht neu.
    markOverlayDirty();
    update();
}

QString SpectrumWidget::bandStackMarkAt(int x, int y, const QRect& specRect) const
{
    if (m_bandStackMarks.isEmpty() || m_moxOverlay) { return {}; }
    if (y > specRect.bottom() || y < specRect.bottom() - kBsTrefferH) { return {}; }
    // Upstream V17: die Marke der Scheibe gewinnt gegen einen Strich
    // darunter, damit Ziehen weiter geht.
    if (std::abs(x - hzToX(m_vfoHz, specRect)) <= kBsVfoFrei) { return {}; }
    QString best;
    int bestAbstand = kBsTreffer + 1;
    for (const BandStackMark& m : m_bandStackMarks) {
        const int mx = hzToX(static_cast<double>(m.hz), specRect);
        const int abstand = std::abs(x - mx);
        if (abstand < bestAbstand) {
            bestAbstand = abstand;
            best = m.id;
        }
    }
    return best;
}

QString SpectrumWidget::bandStackMarkAtForTest(int x, int y) const
{
    const int specH = specHFromHeight(height(), m_spectrumFrac, kFreqScaleH + kDividerH);
    return bandStackMarkAt(x, y, QRect(0, 0, width() - effectiveStripW(), specH));
}

void SpectrumWidget::drawBandStackMarks(QPainter& p, const QRect& specRect)
{
    if (m_bandStackMarks.isEmpty() || m_moxOverlay) { return; }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor leise = roleColor("text-scale", Style::kTextScale);
    const QColor hell  = roleColor("text", Style::kTextPrimary);
    const int unten = specRect.bottom();
    for (const BandStackMark& m : m_bandStackMarks) {
        const int x = hzToX(static_cast<double>(m.hz), specRect);
        if (x < specRect.left() + kBsMarkHalb || x > specRect.right() - kBsMarkHalb) {
            continue;
        }
        const QColor farbe = m.place ? hell : leise;
        QPainterPath dreieck;
        dreieck.moveTo(x, unten - kBsMarkHoehe);
        dreieck.lineTo(x - kBsMarkHalb - 0.5, unten);
        dreieck.lineTo(x + kBsMarkHalb + 0.5, unten);
        dreieck.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(farbe);
        p.drawPath(dreieck);
        if (m.locked) {
            // Ein kleines Schloss darueber: Buegel und Koerper.
            const QPointF c(x, unten - kBsMarkHoehe - 6);
            p.setPen(QPen(leise, 1.1));
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(c.x() - 2.6, c.y() - 6.0, 5.2, 6.0), 0, 180 * 16);
            p.setPen(Qt::NoPen);
            p.setBrush(leise);
            p.drawRoundedRect(QRectF(c.x() - 3.6, c.y() - 2.6, 7.2, 5.6), 1.0, 1.0);
        }
    }
    p.restore();
}

void SpectrumWidget::drawCompanionMarker(QPainter& p, const QRect& specRect)
{
    if (!m_companionShown || m_companionIfHz <= 0) { return; }

    const int x = hzToX(static_cast<double>(m_companionIfHz), specRect);
    if (x < specRect.left() || x > specRect.right()) { return; }

    // Warm und duenn. Der Hausstil laesst Farbe hoechstens zwei Prozent
    // der Flaeche bedecken: eine Linie und ein kurzes Schild halten das
    // mit Abstand ein, ein gefuellter Streifen nicht. Warm und nicht
    // blau, weil Blau im Hausstil das Anfassbare ist -- diese Zahl ist
    // gemessen, nicht gestellt.
    const QColor warm = roleColor("companion-rig", Style::kAmberText);

    p.setPen(QPen(warm, 1, Qt::DashLine));
    p.drawLine(x, specRect.top(), x, specRect.bottom());

    // Die Zahl am Schild ist die der ANTENNE. Das ist der ganze Grund,
    // warum es den Transverter-Versatz gibt: der Betreiber liest hier
    // 144.300, waehrend die Achse darunter in der ZF laeuft. Steht kein
    // Transverter davor, sind beide gleich und niemand merkt etwas.
    QString text = m_companionLabel;
    if (text.isEmpty()) {
        text = QString::number(
            rfFuerAnzeige(static_cast<double>(m_companionIfHz)) / 1.0e6,
            'f', 3);
    }

    // 9, nicht 10: die Schriftleiter des Hausstils kennt 9 und 11, und
    // die Frequenzachse darunter laeuft auf 11. Das Schild soll
    // kleiner sein als die Achse, also die Stufe darunter -- und nicht
    // ein Zwischenwert, den verify-style-drift zu Recht anmerkt.
    QFont schrift = p.font();
    schrift.setPixelSize(9);
    p.setFont(schrift);

    const QFontMetrics fm(schrift);
    const int tw = fm.horizontalAdvance(text) + 8;
    const int th = fm.height() + 2;

    // Nach links kippen, wenn rechts kein Platz mehr ist -- sonst steht
    // das Schild halb ausserhalb und ist nicht zu lesen.
    int tx = x + 3;
    if (tx + tw > specRect.right()) { tx = x - 3 - tw; }
    const QRect schild(tx, specRect.top() + 2, tw, th);

    p.setPen(Qt::NoPen);
    p.setBrush(roleColor("app-bg", Style::kAppBg, 200));
    p.drawRect(schild);
    p.setPen(warm);
    p.drawText(schild, Qt::AlignCenter, text);
}

void SpectrumWidget::drawBandPlan(QPainter& p, const QRect& specRect)
{
    if (!m_bandPlanMgr || m_bandPlanFontSize <= 0) {
        return;
    }

    // Das Fenster, gegen das die Abschnitte geprueft werden, liegt auf
    // der ANTENNE: mit einem Transverter davor sieht der Betreiber ein
    // 2-m-Band, auch wenn dieses Widget eine 28-MHz-Achse malt. Ohne
    // Transverter sind beide Zeilen bitgleich die alten.
    const double startMhz =
        rfFuerAnzeige(m_centerHz - m_bandwidthHz / 2.0) / 1.0e6;
    const double endMhz   =
        rfFuerAnzeige(m_centerHz + m_bandwidthHz / 2.0) / 1.0e6;
    const int    bandH    = m_bandPlanFontSize + 4;
    const int    bandY    = specRect.bottom() - bandH + 1;

    // Erst die Merkpunkte, dann die Beschriftungen (2026-11-08): eine
    // Beschriftung weicht einem Punkt aus, wo sie kann; wo nicht, gewinnt
    // die Schrift. Bei 4 MHz Breite lagen die Punkte sonst auf "CW", "FM".
    const auto& spots = m_bandPlanMgr->spots();
    QVector<int> punktX;
    for (const auto& spot : spots) {
        if (spot.freqMhz < startMhz || spot.freqMhz > endMhz) {
            continue;
        }
        // Wie die Abschnitte: der Merkpunkt ist eine Antennenfrequenz, die
        // Achse eine ZF. Bis 2026-11-08 stand hier die Antennenfrequenz roh
        // -- mit Transverter lag ein 144,300-Punkt weit rechts ausserhalb
        // eines 28-MHz-Bildes. Es fiel nicht auf, weil der Plan auf 2 m bis
        // dahin keine Merkpunkte hatte.
        punktX << hzToX(zfFuerAchse(spot.freqMhz * 1.0e6), specRect);
    }
    QVector<bool> punktVerdeckt(punktX.size(), false);

    const auto& segments = m_bandPlanMgr->segments();
    for (const auto& seg : segments) {
        if (seg.highMhz <= startMhz || seg.lowMhz >= endMhz) {
            continue;
        }

        // Und zurueck: der Abschnitt ist eine Antennenfrequenz, die Achse
        // eine ZF. zfFuerAchse() ist ohne Transverter die Gleichheit,
        // der Ausdruck also derselbe wie vorher.
        const int x1 = hzToX(
            zfFuerAchse(std::max(seg.lowMhz,  startMhz) * 1.0e6), specRect);
        const int x2 = hzToX(
            zfFuerAchse(std::min(seg.highMhz, endMhz)   * 1.0e6), specRect);
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

        const QColor bg = roleColor("app-bg", Style::kAppBg);
        const QColor fill(
            static_cast<int>(seg.color.red()   * blend + bg.red()   * (1.0f - blend)),
            static_cast<int>(seg.color.green() * blend + bg.green() * (1.0f - blend)),
            static_cast<int>(seg.color.blue()  * blend + bg.blue()  * (1.0f - blend)),
            255);
        p.fillRect(x1, bandY, x2 - x1, bandH, fill);

        // Separator line at left edge of each segment.
        p.setPen(roleColor("border-subtle", Style::kBorderSubtle, 200));
        p.drawLine(x1, bandY, x1, bandY + bandH);

        // Label: mode + lowest license class allowed (only if there's room).
        if (x2 - x1 > 20) {
            QFont f = p.font();
            // setPixelSize statt setPointSize: bandH weiter oben rechnet
            // schon mit m_bandPlanFontSize als Pixelmass
            // (bandH = m_bandPlanFontSize + 4), die Schrift selbst lief
            // aber DPI-abhaengig -- beide Rechnungen meinten also nicht
            // dasselbe. Der gespeicherte Zahlenwert bleibt unveraendert,
            // nur wie er interpretiert wird.
            f.setPixelSize(m_bandPlanFontSize);
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
            const int tw = QFontMetrics(f).horizontalAdvance(label);
            const int lx = Durchlass::bandplanTextX(x1, x2, tw, punktX);
            if (lx >= 0) {
                p.drawText(QRect(lx, bandY, tw, bandH), Qt::AlignCenter, label);
            } else {
                p.drawText(QRect(x1, bandY, x2 - x1, bandH), Qt::AlignCenter, label);
                const int tl = (x1 + x2 - tw) / 2;
                for (int i = 0; i < punktX.size(); ++i) {
                    if (punktX[i] + 6 > tl && punktX[i] - 6 < tl + tw) {
                        punktVerdeckt[i] = true;
                    }
                }
            }
        }
    }

    // Spot markers (white dots for digital calling frequencies, etc.).
    // From AetherSDR SpectrumWidget.cpp:4282-4292 [@0cd4559].
    // Nicht unter einer Beschriftung (siehe oben), und Punkte, die sich
    // gegenseitig verdecken (naeher als ein Durchmesser), nur einmal.
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    int letzterX = -1000;
    for (int i = 0; i < punktX.size(); ++i) {
        const int sx = punktX[i];
        if (punktVerdeckt[i] || std::abs(sx - letzterX) < 9) {
            continue;
        }
        p.drawEllipse(QPoint(sx, bandY + bandH / 2), 4, 4);
        letzterX = sx;
    }
    p.setRenderHint(QPainter::Antialiasing, false);
}

// ─── Time scale + LIVE button (sub-epic E) ─────────────────────────────
// From AetherSDR SpectrumWidget.cpp:4929-4994 [@0cd4559]
//   adapter: Longpath computes msPerRow directly from m_wfUpdatePeriodMs
//   instead of AetherSDR's calibrated m_wfMsPerRow (we have no radio
//   tile clock to calibrate against).

void SpectrumWidget::drawTimeScale(QPainter& p, const QRect& wfRect)
{
    const QRect strip = waterfallTimeScaleRect(wfRect);
    const int stripX = strip.x();

    // Semi-opaque background so spectrum content underneath dims.
    p.fillRect(strip, roleColor("panel", Style::kPanelBg, 220));

    // Betreiber, 2026-08-28: die senkrechte Trennlinie zum Wasserfall
    // sollte weg -- das halbdurchsichtige Feld selbst grenzt den
    // Zeitstreifen schon ab, die zusaetzliche Linie war ueberfluessig.
    //
    // LIVE button — grey when live, bright red when paused.
    // Glas & Tiefe (2026-09-18): ein erhabener Chip wie jeder Knopf;
    // angehalten bleibt Rot — das ist eine Warnung, kein Zustand.
    const QRect liveRect = waterfallLiveButtonRect(wfRect);
    if (m_wfLive) {
        Style::paintRaisedChip(p, liveRect, 6);
    } else {
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QColor(Style::kRedBorder));
        p.setBrush(QColor(Style::kTxRed));
        p.drawRoundedRect(liveRect, 6, 6);
        p.setRenderHint(QPainter::Antialiasing, false);
    }

    // setPixelSize statt setPointSize: siehe die Begruendung an der
    // ersten Stelle oben (dBm-Beschriftungen).
    QFont liveFont = p.font();
    liveFont.setPixelSize(Style::kFontMicro);
    liveFont.setBold(true);
    p.setFont(liveFont);
    p.setPen(m_wfLive ? QColor(Style::role("text-secondary", Style::kTextSecondary)) : Qt::white);
    p.drawText(liveRect, Qt::AlignCenter, QStringLiteral("LIVE"));

    // Tick labels along the strip.
    const float msPerRow = std::max(1, m_wfUpdatePeriodMs);
    const QRect labelRect = strip.adjusted(0, 4, 0, 0);
    const float totalSec = labelRect.height() * msPerRow / 1000.0f;
    if (totalSec <= 0) {
        return;
    }

    // setPixelSize statt setPointSize: siehe die Begruendung bei den
    // dBm-Beschriftungen weiter oben.
    QFont f = p.font();
    f.setPixelSize(Style::kFontMicro);
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
        p.setPen(QColor(Style::kTextTertiary));
        p.drawLine(stripX, yy, stripX + 4, yy);

        // Label: elapsed seconds when live, absolute UTC when paused.
        const QString label = m_wfLive
            ? QStringLiteral("%1s").arg(static_cast<int>(sec))
            : pausedTimeLabelForAge(
                m_wfHistoryOffsetRows
                + static_cast<int>(std::round(sec * 1000.0f / msPerRow)));

        p.setPen(QColor(Style::kSpectrumGridText));
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
    if (!m_waterfallHistory.isConfigured() || ageRows < 0
        || ageRows >= m_wfHistoryRowCount) {
        return -1;
    }
    return (m_wfHistoryWriteRow + ageRows) % m_waterfallHistory.capacityRows();
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

    // Nur die Breite hat sich geändert (Teilerziehen, Fenstergröße):
    // Inhalt mitnehmen. Die Tiefe steht über
    // waterfallHistoryCapacityRows() fest, damit Zeilenindizes und
    // Zeitstempel gültig bleiben.
    if (m_waterfallHistory.isConfigured() && m_wfHistoryRowCount > 0
        && m_waterfallHistory.capacityRows() == desiredSize.height()
        && m_waterfallHistory.resizeWidth(desiredSize.width())) {
        return;
    }

    m_waterfallHistory.configure(desiredSize.width(), desiredSize.height());
    m_waterfallHistory.discardRows();
    m_wfHistoryTimestamps = QVector<qint64>(desiredSize.height(), 0);
    m_wfHistoryWriteRow = 0;
    m_wfHistoryRowCount = 0;
    m_wfHistoryOffsetRows = 0;
    m_wfLive = true;
}

// From AetherSDR SpectrumWidget.cpp:647-668 [@0cd4559]
void SpectrumWidget::appendHistoryRow(const quint8* rowData, qint64 timestampMs)
{
    ensureWaterfallHistory();
    if (!m_waterfallHistory.isConfigured() || rowData == nullptr) {
        return;
    }

    const int h = m_waterfallHistory.capacityRows();
    m_wfHistoryWriteRow = (m_wfHistoryWriteRow - 1 + h) % h;
    quint8* row = m_waterfallHistory.writableRow(m_wfHistoryWriteRow);
    if (!row) { return; }
    std::memcpy(row, rowData,
                static_cast<size_t>(m_waterfallHistory.width()));
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
    // Derselbe Ton wie die Erstfüllung. Reines Schwarz war nicht
    // blaustichig und fiel darum bei der Naht nicht auf — aber ein
    // leerer Wasserfall hätte je nach Weg zwei Gründe gehabt.
    m_waterfall.fill(m_wfBgFillColor);
    m_wfWriteRow = 0;

    if (!m_waterfallHistory.isConfigured()) {
        update();
        return;
    }

    // Hier wird gefärbt, nicht kopiert. Das ist der Gewinn aus dem
    // Umbau: die Historie hält Intensität, und jeder Neuaufbau nimmt
    // das Farbschema von JETZT. Vorher galt ein Schemawechsel erst ab
    // der nächsten Zeile, und quer durch den Wasserfall lief ein
    // Streifen dort, wo umgeschaltet wurde.
    const int w = std::min(m_waterfall.width(), m_waterfallHistory.width());
    for (int y = 0; y < m_waterfall.height(); ++y) {
        const int rowIndex = historyRowIndexForAge(m_wfHistoryOffsetRows + y);
        if (rowIndex < 0) {
            break;
        }
        const quint8* src = m_waterfallHistory.row(rowIndex);
        // nullptr heißt: dieser Block wurde nie beschrieben. Die Zeile
        // bleibt schwarz — gefüllt ist das Bild schon.
        if (!src) { continue; }
        auto* dst = reinterpret_cast<QRgb*>(m_waterfall.scanLine(y));
        for (int x = 0; x < w; ++x) {
            dst[x] = waterfallColorForIntensity(src[x], m_wfColorScheme, m_wfLowColor);
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
#ifdef LONGPATH_GPU_SPECTRUM
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
//   adapter: Longpath uses kFreqScaleH = 28 (vs AetherSDR's 20),
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
// and from setFrequencyRange's largeShift branch (Longpath divergence;
// see plan §authoring-time #3).
void SpectrumWidget::clearWaterfallHistory()
{
    // discardRows() statt „mit Schwarz füllen": die Plätze bleiben, der
    // Speicher wird zurückgegeben. Bei zwanzig Minuten Historie ist das
    // der Unterschied zwischen einem geleerten Puffer und einem, der
    // nach dem Trennen weiter Dutzende Megabyte hält.
    m_waterfallHistory.discardRows();
    // Codex P2 (PR #140): also clear the live viewport + reset its write
    // head so a disconnect-flush actually shows a clean waterfall on
    // reconnect, instead of stale rows from the previous session rolling
    // off one frame at a time. The largeShift path's reprojectWaterfall
    // already reallocates m_waterfall, so this is a no-op there; the
    // disconnect path (MainWindow → connectionStateChanged) had no
    // preceding reset.
    if (!m_waterfall.isNull()) {
        m_waterfall.fill(m_wfBgFillColor);
    }
    m_wfWriteRow = 0;
    std::fill(m_wfHistoryTimestamps.begin(), m_wfHistoryTimestamps.end(), 0);
    m_wfHistoryWriteRow = 0;
    m_wfHistoryRowCount = 0;
    m_wfHistoryOffsetRows = 0;
    m_wfLive = true;
    markOverlayDirty();
}

// Phase 3F Sub-Epic F Task 6: store the latest per-ADC wideband bins.
// Sub-Epic F polish (T7-T10) will paint these as a background layer when
// m_extendedMode is true and the pan's visible frequency range exceeds
// the active DDC bandwidth.  For now this is silent storage so the
// FFT pipeline stays warm and bench operators can confirm data is
// flowing without UI churn.
void SpectrumWidget::setWidebandBins(int adcIndex, const QVector<float>& dbmBins)
{
    if (adcIndex == 0) {
        m_widebandBinsAdc0 = dbmBins;
    } else if (adcIndex == 1) {
        m_widebandBinsAdc1 = dbmBins;
    }
    // No update() call: paint is gated behind m_extendedMode (wired in
    // F polish).  Triggering a repaint here would cost a GPU pass per
    // wideband frame on hardware that isn't yet rendering the bins.
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
    recomputeExtendedMode();
}

void SpectrumWidget::recomputeExtendedMode()
{
    const bool actual =
        m_extendedViewAllowed
        && m_sampleRateHz > 0.0
        && m_bandwidthHz > m_sampleRateHz;
    if (m_extendedMode == actual) { return; }
    m_extendedMode = actual;
    emit widebandExtensionStateChanged(actual);
    // Schedule a repaint so the future paint impl picks up the state
    // change. Today this is a no-op in the visual pipeline beyond the
    // stored bool — cheap.
    update();
}

// From AetherSDR SpectrumWidget.cpp:951-1000 [@0cd4559]
//   adapter: Longpath uses Hz throughout (upstream uses MHz). Both the
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

    // ── Die Historie beim Schwenken ──────────────────────────────────
    //
    // Hier stand derselbe Aufruf für die Historie, als sie noch ein
    // RGB32-Bild war: die alten Zeilen wurden waagrecht verschoben und
    // gestreckt, damit sie zum neuen Ausschnitt passen.
    //
    // Das ist eine Notlösung, und man sieht es: verschoben wird das
    // ganze Bild auf einmal, obwohl die Zeilen aus verschiedenen
    // Ausschnitten stammen können, und was aus dem Fenster fällt, ist
    // endgültig weg — auch wenn man gleich wieder zurückschwenkt.
    //
    // Die richtige Antwort ist Schritt 3 aus docs/design/
    // WASSERFALL-AETHER.md: jede Zeile merkt sich, bei welcher
    // Mittenfrequenz und Bandbreite sie aufgenommen wurde, und wird
    // beim Zeichnen einzeln umgerechnet. Dann ist Schwenken eine
    // Frage der Anzeige und nicht ein Eingriff in die Aufzeichnung.
    //
    // Bis dahin: die Historie wird verworfen. Lieber leer als falsch —
    // eine gestreckte alte Zeile sieht aus wie eine Messung und ist
    // keine. Der sichtbare Wasserfall (oben) wird weiter umgerechnet,
    // damit ein kleiner Schwenk nicht schwarz aufblitzt.
    if (m_waterfallHistory.isConfigured()) {
        m_waterfallHistory.discardRows();
        std::fill(m_wfHistoryTimestamps.begin(),
                  m_wfHistoryTimestamps.end(), 0);
        m_wfHistoryWriteRow = 0;
        m_wfHistoryRowCount = 0;
        m_wfHistoryOffsetRows = 0;
        m_wfLive = true;
    }

    // Two-sentinel pattern matches rebuildWaterfallViewport (see Task 3
    // review): m_wfTexFullUpload routes the GPU upload through the full
    // path, m_wfLastUploadedRow = -1 forces every scanline to be re-sent.
    // Skipping either leaves the bottom row stale on the next frame.
#ifdef LONGPATH_GPU_SPECTRUM
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
// In Longpath, "locals" become member fields (m_wfActiveLow/High) so
// external runtime layers (Clarity, between-row signal updates) can
// stick a value that survives until the next row push.  AGC's
// running-envelope state (m_wfAgcRunMin/Max) is the equivalent of
// Thetis's _RX1waterfallPreviousMinValue field.
void SpectrumWidget::composeWaterfallActiveThresholds(const QVector<float>& wfPixelsDbm)
{
    if (wfPixelsDbm.isEmpty()) { return; }
    const int n = wfPixelsDbm.size();

    // Seed from persistent user values unless Clarity is the live
    // driver — Clarity writes m_wfActive* directly via
    // setClarityWaterfallThresholds() and must survive between rows.
    // Matches the Thetis "high_threshold = waterfall_high_threshold"
    // seed at display.cs:6575 [v2.10.3.13].
    // ── Clarity ohne Boden faellt auf die eigenen Schwellen zurueck ──
    //
    // Hier gab es nur zwei Zweige: Clarity aus -> Nutzerschwellen,
    // Clarity an MIT Boden -> verschobene Clarity-Schwellen. Der
    // dritte Fall — Clarity AN, aber ohne Boden — fiel durch beide
    // hindurch und liess `m_wfActive*` auf dem Stand stehen, den das
    // letzte Geraet hinterlassen hatte.
    //
    // Genau das trat am 2026-10-22 an der FLEX-8400 ein: Clarity wird
    // ausschliesslich aus `primaryFftEngine()` gefuettert, und die
    // gibt es beim Flex nicht. Der Wasserfall war durchgehend
    // EINFARBIG ROT — bei Farbschema 0 die oberste Palettenstufe.
    //
    // Gefuettert wird Clarity jetzt auch vom Flex (MainWindow_Flex).
    // Diese Wache bleibt trotzdem: eine fremde Quelle, die den Boden
    // nicht liefert, soll ein brauchbares Bild geben und nicht das
    // Erbe des Vorgaengers.
    //
    // NACHTRAG 2026-10-23: das ist eine BEWUSSTE Vertragsaenderung
    // gegenueber dem Aufruf mit zwei Argumenten, der seine Schwellen
    // bis zum 2026-10-22 unberuehrt stehen liess. Die CI hat sie
    // gefunden (tst_waterfall_clarity_anchor fiel um, lokal gruen,
    // weil ctest die ALTE Testbinaerdatei fuhr — siehe memory
    // feedback-falsches-gruen-alte-binaerdateien). Der Vertrag bleibt
    // geaendert: ohne gemessenen Boden sind Clarity's absolute
    // Schwellen nicht belastbar, und in der Fassung bringt sie
    // niemand mehr auf diesem Weg herein — `MainWindow.cpp:6095` ruft
    // immer mit Boden. Der Pruefstand sagt jetzt den neuen Vertrag.
    const bool clarityHatBoden = std::isfinite(m_clarityFloorDbm);
    if (!m_clarityActive || !clarityHatBoden) {
        m_wfActiveLowThreshold  = m_wfLowThreshold;
        m_wfActiveHighThreshold = m_wfHighThreshold;
    }
    if (!m_clarityActive) {
        // schon gesetzt
    } else if (std::isfinite(m_clarityFloorDbm)
               && std::isfinite(m_clarityLow) && std::isfinite(m_clarityHigh)) {
        // ── Clarity am eigenen Bild verankern (2026-09-25) ───────────
        //
        // Clarity bildet low/high = Boden + Abstand, den Boden aber aus
        // den FFT-Bins (30. Perzentil). Gefaerbt werden diese Bildpunkte
        // hier — nach Wasserfall-Detektor und Mittelung, bei mehreren
        // Bins je Punkt. Beide Boeden fallen je nach Signalstatistik
        // weit auseinander: an einer SunSDR2 QRP ohne Antenne lag der
        // Bin-Boden 17 dB UNTER den Bildpunkten (echtes I/Q, spitzes
        // Rauschen: der Wasserfall wurde ganz rot) und im Einkanal-
        // Zustand 10 dB DARUEBER (ganz schwarz). Gemessen, nicht
        // geschaetzt: WFDIAG-Protokoll 06:46 und 06:49.
        //
        // Darum dasselbe Perzentil auf die Bildpunkte, langsam geglaettet
        // wie der Lauf-Minimum/Maximum-Folger darunter (0,05 je Zeile, bei
        // ~10 Zeilen/s rund 2 s), und Clarity's Fenster um den Unterschied
        // verschoben. Abstaende, Totband und Glaettung bleiben Clarity's.
        const float pixFloor = m_wfPixelFloorEstimator.estimate(wfPixelsDbm);
        if (std::isfinite(pixFloor)) {
            constexpr float kPixFloorAlpha = 0.05f;
            m_wfPixelFloorDbm = std::isfinite(m_wfPixelFloorDbm)
                ? kPixFloorAlpha * pixFloor + (1.0f - kPixFloorAlpha) * m_wfPixelFloorDbm
                : pixFloor;
            const float shift = m_wfPixelFloorDbm - m_clarityFloorDbm;
            m_wfActiveLowThreshold  = m_clarityLow  + shift;
            m_wfActiveHighThreshold = m_clarityHigh + shift;
        }
    }

    // AGC: one-pole follower on display-pixel min/max biases the
    // effective thresholds.  Skipped while Clarity is the driver.
    // Phase 3G-9c.
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
        float low  = m_wfAgcRunMin - margin;
        float high = m_wfAgcRunMax + margin;

        // …except the sliders take their cut afterwards, in dbmToRgb,
        // and at the shipped defaults the colour-gain slider removes
        // 13.5 dB from the top while this margin only added 12. The
        // effective high threshold therefore landed 1.5 dB BELOW the
        // running maximum: the loudest pixels always saturated to the
        // palette's last stop, which in ClarityBlue is magenta. On a
        // flat spectrum — a quiet band, or a radio not yet delivering
        // varied data — every pixel is the running maximum, and the
        // whole waterfall came up magenta. Reported on ANAN-7000DLE and
        // present in released 0.5.2.
        //
        // The sliders may eat into this margin — that is what they are
        // for, and peaks reaching the top colour is correct. What is
        // not correct is the palette top swallowing everything. So the
        // colour-gain slider may pull the high threshold down into the
        // loud end of the real dynamic range, capped at clipAllowance
        // below.
        //
        // 2026-08-25, OE5SOS bench: at the black-level/colour-gain
        // values actually used on a live 20m band (~55-94 / ~64-80 out
        // of range), both sliders sat past this cap and had no visible
        // effect at all -- not a bug in the cap/proportional-response
        // math itself (verified: below the cap each slider moves its
        // threshold in direct proportion, exactly as intended), just a
        // cap set tight enough that ordinary slider positions land past
        // it. Loosened on his explicit request ("bitte lockern"), not a
        // default changed on our own judgement.
        //
        // The black-level side additionally had NO cap at all: it read
        // `m_wfAgcRunMin - 1.0f - blackCut` uncapped, so for any
        // blackCut past ~11 dB (nearly the whole 0-125 slider range)
        // the +blackCut re-application in waterfallIntensityF() cancels
        // it back to a constant runMin-1 -- black level was very close
        // to a no-op across its whole usable range, worse than colour
        // gain's partial cap. Given a matching clipAllowance cap here
        // too, mirroring the high side instead of leaving it uncapped.
        const float blackCut = wfBlackLevelOffsetDb(m_wfBlackLevel);
        const float gainCut  = wfColorGainOffsetDb(m_wfColorGain);

        const float dataSpan = std::max(0.0f, m_wfAgcRunMax - m_wfAgcRunMin);
        const float clipAllowance = std::min(kWfMaxClipDb, 0.25f * dataSpan);
        low  = std::min(low,  m_wfAgcRunMin + clipAllowance - blackCut);
        high = std::max(high, m_wfAgcRunMax - clipAllowance + gainCut);

        // On a flat spectrum the allowance is zero, which would put
        // every pixel exactly at the top of the palette — the solid
        // magenta again, by a different route. Keep a usable span so a
        // quiet band looks quiet instead of looking like one colour.
        const float needed = kWfAgcPaletteSpanDb + blackCut + gainCut;
        if (high - low < needed) {
            const float mid = 0.5f * (low + high);
            low  = mid - 0.5f * needed;
            high = mid + 0.5f * needed;
        }
        m_wfActiveLowThreshold  = low;
        m_wfActiveHighThreshold = high;
    }

    // Note: "Use spectrum min/max" no longer mutates here.  Thetis
    // wires that flag via setWaterfallGainsIfLinkedToSpectrum
    // (console.cs:9094-9108 [v2.10.3.13]) — the grid-change handler
    // calls the persistent property setter once per grid change, not
    // per render frame.  Longpath's port lives in setDbmRange() +
    // setWfUseSpectrumMinMax().  Per-frame mutation here was the
    // issue #230 source.

    // Task 2.8: NF-AGC -- override thresholds from 10th-percentile
    // noise floor + configured offset.  Runs after AGC so it wins on
    // tie; defers to Clarity.
    if (m_wfNfAgcEnabled && !m_clarityActive) {
        QVector<float> sorted = wfPixelsDbm;
        std::sort(sorted.begin(), sorted.end());
        const float nf = sorted[qBound(0, sorted.size() / 10, sorted.size() - 1)];
        const float offsetF = static_cast<float>(m_wfNfAgcOffsetDb);
        m_wfActiveLowThreshold  = nf + offsetF;
        m_wfActiveHighThreshold = m_wfActiveLowThreshold + 60.0f;
    }

    // ── Letzte Sicherung: das Fenster muss die Daten schneiden ───────
    //
    // Der Klemmschutz in waterfallIntensity haelt die SPANNE offen, nicht
    // die LAGE. Ein Fenster von 30 dB, das vollstaendig unter dem
    // Rauschflur liegt, ist formal gesund und trotzdem eine einfarbige
    // Flaeche: jeder Punkt sitzt auf der Palettenspitze. Im Schema
    // Enhanced ist das ein durchgehend rotes Band, im ClarityBlue ein
    // magentafarbenes — zweimal derselbe Befund, zweimal von aussen als
    // Fehler des Wasserfalls gelesen.
    //
    // Dies ist bewusst kein Ersatz fuer die Ursache, sondern ein Netz
    // darunter: ein Fenster, das die gemessenen Werte gar nicht
    // beruehrt, ist immer ein Fehler weiter oben. Es soll ein
    // kontrastarmes Bild geben und keine Flaeche — dieselbe Haltung wie
    // bei der Spannen-Klemme.
    //
    // Absichtlich nur der voellig verfehlte Fall. Ein Fenster, das die
    // Daten teilweise abschneidet, ist genau das, wofuer die Schieber da
    // sind, und wird nicht angetastet.
    {
        float dataMin = wfPixelsDbm[0];
        float dataMax = dataMin;
        for (int i = 1; i < n; ++i) {
            const float v = wfPixelsDbm[i];
            if (!std::isfinite(v)) { continue; }
            if (v < dataMin) { dataMin = v; }
            if (v > dataMax) { dataMax = v; }
        }
        fitThresholdsToData(m_wfActiveLowThreshold,
                            m_wfActiveHighThreshold, dataMin, dataMax);
    }
}

void SpectrumWidget::fitThresholdsToData(float& low, float& high,
                                         float dataMin, float dataMax)
{
    if (!std::isfinite(dataMin) || !std::isfinite(dataMax)
        || !std::isfinite(low) || !std::isfinite(high)) {
        return;
    }
    // Nur der voellig verfehlte Fall. Beruehrt das Fenster die Daten
    // auch nur teilweise, bleibt es stehen -- genau dafuer sind die
    // Schieber da.
    if (high > dataMin && low < dataMax) {
        return;
    }
    const float span = std::max(kWfAgcPaletteSpanDb, high - low);
    const float mid  = 0.5f * (dataMin + dataMax);
    low  = mid - 0.5f * span;
    high = mid + 0.5f * span;
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
float SpectrumWidget::wfRowSourceReduce(const QVector<float>& src, int x, float scale,
                                         SpectrumDetector detector)
{
    const int n = src.size();
    if (n <= 0) { return 0.0f; }
    int first = static_cast<int>(static_cast<float>(x) * scale);
    int last  = static_cast<int>(static_cast<float>(x + 1) * scale) - 1;
    first = qBound(0, first, n - 1);
    last  = qBound(first, last, n - 1);   // Aufweiten: genau ein Punkt
    if (last == first) { return src[first]; }
    switch (detector) {
    case SpectrumDetector::Sample:
        // Der erste Punkt im Fenster -- die Wahl "Sample" heisst genau das.
        return src[first];
    case SpectrumDetector::Average:
    case SpectrumDetector::RMS: {
        // Mittlere LEISTUNG, nicht mittlere dB: ein Traeger in einem der
        // zwei Punkte zaehlt mit -3 dB, statt im dB-Mittel zu versinken.
        double sum = 0.0;
        for (int i = first; i <= last; ++i) {
            sum += std::pow(10.0, static_cast<double>(src[i]) / 10.0);
        }
        return static_cast<float>(10.0 * std::log10(sum / (last - first + 1) + 1e-60));
    }
    case SpectrumDetector::Peak:
    case SpectrumDetector::Rosenfell:
    default: {
        float m = src[first];
        for (int i = first + 1; i <= last; ++i) {
            if (src[i] > m) { m = src[i]; }
        }
        return m;
    }
    }
}

// ── Eine Wasserfallzeile der Maschine (2026-10-23) ──────────────────
//
// Die FLEX-8400 fuehrt ZWEI Bildstroeme, und AetherSDR zeichnet auch
// aus beiden (docs/protocols/flex-wasserfall-aethersdr.md):
//   0x8003  Panadapter-FFT    Bildzeile -> dBm ueber y_pixels
//   0x8004  Wasserfallkachel  qint16/128 -> INTENSITAET, mit eigenem
//           Frequenzbereich und eigenem Takt (`LineDurationMS`)
//
// Longpath zog bisher nur den ersten und leitete den Wasserfall aus
// den FFT-Bins ab — mit eigenem Zeitgeber und eigenen Schwellen. Der
// Betreiber am 2026-10-22: „das rot ist eigenartig gewesen, auch die
// zeitliche laenge, bis der wasserfall anfaengt". Beides folgt daraus.
//
// ── Die Falle, und warum diese Methode nicht pushWaterfallRow ist ──
//
// Die Kachelwerte sind KEINE dBm. Am Geraet gemessen liegen sie bei
// +88 bis +120; `pushWaterfallRow` vergleicht gegen Schwellen um
// −120…−60. Dort hineingegeben stuenden sie sofort am oberen
// Anschlag — also genau das Rot, das behoben werden soll. (Genau das
// habe ich am 2026-10-22 erst gebaut und wieder herausgenommen.)
// Longpath haelt das schon fest:
// `kachelwerteSindEinFarbmassstabKeineDbm` in tst_flex_vita_reader.
//
// Gefaerbt wird darum in der INTENSITAETSDOMAENE, nach AetherSDRs
// `intensityToWaterfallLevel` (SpectrumWidget.cpp:8393 [@d58e2b8a]),
// Fall 2 — der selbst geschaetzte. Den radio-seitigen Schwarzpunkt
// nutzt AetherSDR ausdruecklich nur auf Wunsch des Betreibers, mit
// der Begruendung, er ziele auf SmartSDRs andere Darstellung.
//
//   schwarz = Rauschteppich der Kachel
//   spanne  = max(1, 120 − Farbverstaerkung × 0,91)
//   stufe   = clamp((wert − schwarz) / spanne, 0, 1)
//
// Der Rauschteppich wird wie dort geschaetzt: Mittel ueber jeden
// achten Bin, dann Mittel der Bins UNTER diesem Mittel.
void SpectrumWidget::zeigeFremdeWasserfallzeile(const QVector<float>& intensitaet,
                                                double lowHz, double highHz)
{
    if (intensitaet.isEmpty() || !(highHz > lowHz)) { return; }
    // Siehe `stelleWasserfallbildSicher`: der Weg der Maschine traf
    // denselben leeren Speicher -- und bei der Flex ist DAS der Weg,
    // der das Bild schreibt.
    if (!stelleWasserfallbildSicher()) { return; }
    if (m_wfStopOnTx && m_activePeakHold.txActive()) { return; }

    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    m_fremdeWasserfallquelle    = true;
    m_fremdeWasserfallZuletztMs = jetzt;
    m_wfLastPushMs              = jetzt;

    // ── Zeitliche Mittelung (2026-10-23) ────────────────────────────
    //
    // Der Betreiber im Vergleich mit der Anvelina: „oben schaut es
    // nach strichen aus, flex nach punkten", und nach dem Wechsel von
    // Maximum auf Mittel je Bildpunkt: „unten ist es nach wie vor
    // sehr pixelig".
    //
    // Es war nie die Breite, sondern die ZEIT. Der abgeleitete Weg
    // laeuft durch `SpectrumAvenger` und mittelt ueber die Frames
    // (beim Betreiber Log Recursive, 400 ms) — benachbarte Zeilen
    // haengen dadurch zusammen, und daraus werden die Striche. Die
    // Flex-Kachel ging ungemittelt aufs Bild: jede Zeile ihr eigenes
    // Rauschen, also Punkte.
    //
    // Hier dieselbe Mittelung, aber ueber die GEMESSENE Zeit statt
    // ueber die Bildrate: die Kachel kommt im Takt der Maschine
    // (`line_duration`), nicht im Takt des Fensters, ein fps-basiertes
    // Alpha waere also am falschen Nenner. Alpha = exp(-dt/tau) mit
    // dem tau des Betreibers; dieselbe Form wie Thetis
    // specHPSDR.cs:358, nur mit echtem dt.
    //
    // Die Kachelwerte sind bereits logarithmisch (dB/128), die
    // Mittelung hier ist damit das Log Recursive seiner Einstellung,
    // nicht die lineare Variante.
    const bool mitteln = (m_waterfallAveraging != SpectrumAveraging::None);
    if (!mitteln
        || m_fremdeWfMittel.size() != intensitaet.size()
        || !qFuzzyCompare(m_fremdeWfMittelTiefHz, lowHz)
        || !qFuzzyCompare(m_fremdeWfMittelHochHz, highHz)
        || m_fremdeWfMittelMs <= 0
        || jetzt - m_fremdeWfMittelMs > 2000) {
        // Abgeschaltet, Aussetzer -- oder der Ausschnitt der Kachel
        // hat sich geaendert (Bandwechsel, Zoom). Dann neu anfangen:
        // weiterrechnen zoege ein altes Band ins neue hinein, und
        // weil die Kachelbreite dabei gleich bleiben kann, faengt die
        // Groessenpruefung allein das NICHT.
        m_fremdeWfMittel = intensitaet;
    } else {
        const double dtSek = std::max<qint64>(1, jetzt - m_fremdeWfMittelMs)
                             / 1000.0;
        const double tauSek = std::max(1, m_waterfallAverageTimeMs) / 1000.0;
        const float alt = static_cast<float>(
            qBound(0.0, std::exp(-dtSek / tauSek), 1.0));
        const float neu = 1.0f - alt;
        float* m = m_fremdeWfMittel.data();
        for (int i = 0; i < intensitaet.size(); ++i) {
            m[i] = neu * intensitaet[i] + alt * m[i];
        }
    }
    m_fremdeWfMittelMs     = jetzt;
    m_fremdeWfMittelTiefHz = lowHz;
    m_fremdeWfMittelHochHz = highHz;
    const QVector<float>& zeile = m_fremdeWfMittel;

    // ── Der Rauschteppich der Kachel ────────────────────────────────
    double summe = 0.0; int zahl = 0;
    for (int i = 0; i < zeile.size(); i += 8) {
        summe += zeile[i]; ++zahl;
    }
    if (zahl == 0) { return; }
    const double mittel = summe / zahl;
    double tiefSumme = 0.0; int tiefZahl = 0;
    for (int i = 0; i < zeile.size(); i += 8) {
        if (zeile[i] <= mittel) { tiefSumme += zeile[i]; ++tiefZahl; }
    }
    const float teppich = tiefZahl > 0 ? float(tiefSumme / tiefZahl)
                                       : float(mittel);
    // Langsam nachfuehren, damit eine einzelne laute Zeile das Bild
    // nicht aufhellt. 0,05 wie in der Vorlage.
    //
    // Beim SENDEN eingefroren, ebenfalls wie dort: das eigene Signal
    // wuerde den Teppich hochziehen, und nach dem Loslassen stuende
    // das Band minutenlang zu dunkel.
    constexpr float kAlpha = 0.05f;
    if (!m_activePeakHold.txActive()) {
        m_fremderTeppich = std::isfinite(m_fremderTeppich)
            ? kAlpha * teppich + (1.0f - kAlpha) * m_fremderTeppich
            : teppich;
    } else if (!std::isfinite(m_fremderTeppich)) {
        m_fremderTeppich = teppich;   // beim ersten Mal muss er stehen
    }

    const float spanne = std::max(1.0f, 120.0f - m_wfColorGain * 0.91f);

    // ── Der Teppich gehoert NICHT auf Schwarz (2026-10-23) ──────────
    //
    // Hier stand der Rauschteppich als Schwarzpunkt, wie in der
    // Vorlage („Use noise floor directly as threshold — noise maps to
    // black"). Dann liegt der ganze Teppich auf Stufe 0, und seine
    // Textur verschwindet. Der Betreiber: „der teppich sollte nicht
    // so dunkel sein", „hier ist das blau unten und oben das gleiche",
    // „jedenfalls heller".
    //
    // Die Farbtabelle ist dieselbe wie in NereusSDR (nachgesehen,
    // kDefaultStops Stufe fuer Stufe gleich) — es lag nie an ihr,
    // sondern am Anlegepunkt. Also den Schwarzpunkt um ein Stueck
    // UNTER den Teppich legen, damit er selbst im unteren Blau liegt
    // und seine Struktur sichtbar bleibt; Signale steigen von dort
    // durch Cyan, Gruen und Gelb.
    //
    // Anteilig an der Spanne, nicht in festen Einheiten: sonst haengt
    // die Helligkeit an der Farbverstaerkung.
    constexpr float kTeppichAufDerRampe = 0.18f;
    const float schwarz = m_fremderTeppich - spanne * kTeppichAufDerRampe;

    // ── Auf die Fensterbreite abbilden ──────────────────────────────
    //
    // Ausschnitt der Kachel und des Fensters muessen NICHT
    // uebereinstimmen: die Maschine schickt ihre eigene Breite
    // (gemessen 230,6 kHz gegen 200 kHz Panadapterbreite), und der
    // Betreiber darf zoomen.
    const int breite = m_waterfall.width();
    if (breite <= 0) { return; }
    const int h = m_waterfall.height();
    m_wfWriteRow = (m_wfWriteRow - 1 + h) % h;
#ifdef LONGPATH_GPU_SPECTRUM
    ++m_wfRowsSinceUpload;
#endif
    QRgb* scanline = reinterpret_cast<QRgb*>(m_waterfall.scanLine(m_wfWriteRow));
    m_wfRowIntensity.resize(breite);
    quint8* stufen = m_wfRowIntensity.data();

    const double fensterTief  = m_centerHz - m_bandwidthHz * 0.5;
    const double hzProPunkt   = m_bandwidthHz / breite;
    const double hzProBin     = (highHz - lowHz) / zeile.size();
    const float  kleinster    = *std::min_element(zeile.cbegin(),
                                                  zeile.cend());

    for (int x = 0; x < breite; ++x) {
        const double hzA = fensterTief + x * hzProPunkt;
        const double hzB = hzA + hzProPunkt;
        int a = static_cast<int>(std::floor((hzA - lowHz) / hzProBin));
        int b = static_cast<int>(std::ceil((hzB - lowHz) / hzProBin)) - 1;
        float wert;
        if (b < 0 || a >= static_cast<int>(zeile.size())) {
            // Ausserhalb dessen, was die Kachel abdeckt: der tiefste
            // Wert der Zeile. Null waere hier ein KRAEFTIGES Signal.
            wert = kleinster;
        } else {
            const int letzter = static_cast<int>(zeile.size()) - 1;
            a = std::clamp(a, 0, letzter);
            b = std::clamp(b, a, letzter);
            // ── Der Melder des Betreibers, nicht mein eigener ───
            //
            // Hier stand erst das Maximum (Rendering-Werkbank
            // 2026-09-25: ein schmaler Traeger darf nicht zwischen
            // zwei Bildpunkte fallen), dann am 2026-10-23 das Mittel
            // gegen die Koernung. Beides war meine Wahl, und beide
            // Male die falsche Baustelle: die Koernung sass in der
            // ZEIT, nicht in der Breite (siehe die Mittelung oben).
            //
            // Nachgesehen, was der abgeleitete Weg tut, den der
            // Betreiber als Vorbild nennt: er laeuft durch
            // `applySpectrumDetector` mit SEINER Einstellung —
            // `DisplayWaterfallDetector`, bei ihm Sample. Der
            // Kachelweg hat sie bisher ignoriert. Also dieselbe Wahl
            // auch hier, damit „wie bei der Anvelina" wirklich
            // dasselbe heisst.
            //
            // Kein invEnb: das ist die Fensterkorrektur UNSERER FFT.
            // Die Kachel kommt fertig von der Maschine.
            const int n = b - a + 1;
            switch (m_waterfallDetector) {
                case SpectrumDetector::Peak: {
                    wert = zeile[a];
                    for (int i = a + 1; i <= b; ++i) {
                        wert = std::max(wert, zeile[i]);
                    }
                    break;
                }
                case SpectrumDetector::Rosenfell: {
                    // Vereinfacht gegenueber SpectrumDetector.cpp:118:
                    // dort traegt `prev_maxi` ueber die Bildpunkte
                    // hinweg. Stieg UND fiel es im Fenster, nimmt der
                    // ungerade Bildpunkt das Maximum, der gerade das
                    // Minimum — so entsteht die Rosenfell-Struktur.
                    float mini = zeile[a], maxi = zeile[a];
                    bool stieg = false, fiel = false;
                    for (int i = a + 1; i <= b; ++i) {
                        mini = std::min(mini, zeile[i]);
                        maxi = std::max(maxi, zeile[i]);
                        if (zeile[i] > zeile[i - 1]) { stieg = true; }
                        if (zeile[i] < zeile[i - 1]) { fiel  = true; }
                    }
                    wert = (stieg && fiel) ? ((x & 1) ? maxi : mini) : maxi;
                    break;
                }
                case SpectrumDetector::Sample: {
                    // Die MITTE des Fensters, wie
                    // SpectrumDetector.cpp:192 (`bins[i - bcount/2]`).
                    wert = zeile[a + n / 2];
                    break;
                }
                case SpectrumDetector::Average:
                case SpectrumDetector::RMS:
                case SpectrumDetector::Count:
                default: {
                    // RMS faellt auf das Mittel: die Kachelwerte sind
                    // logarithmisch (dB/128), eine Quadratsumme darauf
                    // haette keine Bedeutung.
                    double summeBins = 0.0;
                    for (int i = a; i <= b; ++i) { summeBins += zeile[i]; }
                    wert = static_cast<float>(summeBins / n);
                    break;
                }
            }
        }
        const float stufe = std::clamp((wert - schwarz) / spanne,
                                       0.0f, 1.0f);
        stufen[x] = static_cast<quint8>(std::lround(stufe * 255.0f));
        scanline[x] = waterfallColorForIntensityF(stufe, m_wfColorScheme,
                                                  m_wfLowColor);
    }

    // ── Und wo steht die Spitze der KACHEL? (2026-10-23) ────────────
    //
    // Gegenstueck zur Bandmeldung oben, in derselben Einheit
    // (Bildpunkt und Frequenz). Stehen die beiden Spitzen an
    // verschiedenen Frequenzen, liegen die zwei Stroeme der Flex nicht
    // uebereinander — genau die Frage des Betreibers.
    if (jetzt - m_kachelBerichtZuletztMs > 30000) {
        m_kachelBerichtZuletztMs = jetzt;
        int sx = 0;
        for (int i = 1; i < breite; ++i) {
            if (stufen[i] > stufen[sx]) { sx = i; }
        }
        const double sHz = fensterTief + sx * hzProPunkt;

        // ── Liegen die beiden Stroeme uebereinander? ─────────────
        //
        // „kann es sein, dass das versetzt ist". Pixel im Bild zu
        // vergleichen ist Raten — der 3D-Flur hat Perspektive, ein
        // Kamm im Hintergrund steht nicht ueber seiner Frequenz.
        // Also die beiden Zeilen gegeneinander schieben und die
        // Verschiebung melden, bei der sie am besten passen.
        // Beide liegen in Bildpunkten derselben Flaeche, nur in
        // verschiedener Aufloesung (Bildpunktverhaeltnis).
        // Kein QStringLiteral mit deutschem Text in src/gui: den
        // zaehlt tst_oberflaeche_englisch als Beschriftung mit (die
        // Decke steht bei 9). Protokolltexte gehen darum direkt in den
        // Strom, nicht ueber eine zusammengesetzte Zeichenkette.
        bool   versatzMessbar = false;
        int    besterLag  = 0;
        int    lagLinks     = 0;
        int    lagRechts    = 0;
        double sicherLinks  = 0.0;
        double sicherRechts = 0.0;
        double faktor     = 1.0;
        double zweitRel   = 0.0;
        double beiNull    = 0.0;
        double bestWert   = 0.0;
        if (!m_renderedPixels.isEmpty() && breite > 64) {
            const int n = breite;
            const int m = m_renderedPixels.size();
            auto kurve = [&](int i) -> double {   // Pan auf Kachelbreite
                const double f = double(i) * (m - 1) / double(n - 1);
                const int a = qBound(0, int(f), m - 1);
                return m_renderedPixels[a];
            };
            // Beide auf Mittelwert null, sonst gewinnt der Gleichanteil.
            QVector<double> A(n), B(n);
            double sa = 0.0, sb = 0.0;
            for (int i = 0; i < n; ++i) {
                A[i] = stufen[i]; B[i] = kurve(i); sa += A[i]; sb += B[i];
            }
            sa /= n; sb /= n;
            for (int i = 0; i < n; ++i) { A[i] -= sa; B[i] -= sb; }
            const int maxLag = n / 10;
            double bestes = -1.0e30;
            QVector<double> werte(2 * maxLag + 1, -1.0e30);
            for (int lag = -maxLag; lag <= maxLag; ++lag) {
                double sum = 0.0; int zahl = 0;
                for (int i = qMax(0, -lag); i < qMin(n, n - lag); ++i) {
                    sum += A[i] * B[i + lag]; ++zahl;
                }
                if (zahl > 0) {
                    sum /= zahl;
                    werte[lag + maxLag] = sum;
                    if (sum > bestes) { bestes = sum; besterLag = lag; }
                }
            }
            // ── Wie deutlich ist das Ergebnis? (2026-10-23) ──────
            //
            // Dieselbe Lage ergab im Abstand von 30 s einmal -8 und
            // einmal +107 Punkte. Auf einem Band mit vielen aehnlichen
            // Signalen rastet die Kreuzkorrelation auf dem falschen
            // Nachbarn ein, und dann steht eine erfundene Zahl im
            // Protokoll, auf die ich prompt eine Behebung gebaut habe.
            //
            // Also den zweitbesten Gipfel AUSZERHALB der Umgebung des
            // besten mitmelden. Liegt er fast gleichauf, ist die
            // Messung nichts wert und sagt das auch.
            double zweitbestes = -1.0e30;
            for (int lag = -maxLag; lag <= maxLag; ++lag) {
                if (std::abs(lag - besterLag) <= 10) { continue; }
                zweitbestes = std::max(zweitbestes, werte[lag + maxLag]);
            }
            zweitRel = (bestes > 0.0 && zweitbestes > -1.0e29)
                ? zweitbestes / bestes : 0.0;
            beiNull = werte[maxLag];
            bestWert = bestes;
            versatzMessbar = true;

            // ── Verschiebung oder MASZSTAB? (2026-10-27) ─────────
            //
            // Betreiber: „leider ist das untere und das obere Signal
            // im Wasserfall nicht auf allen frequenzen gleich. oft
            // ist es nach wie vor versetzt."
            //
            // „Nicht auf allen Frequenzen gleich" ist die Auskunft,
            // auf die es ankommt: eine Verschiebung ist ueberall
            // gleich gross, ein MASZSTABSFEHLER waechst mit dem
            // Abstand von der Mitte. Ein einziger bester Versatz kann
            // die beiden nicht trennen — er mittelt sie zu einer Zahl,
            // die dann je nachdem, wo gerade die staerksten Signale
            // stehen, anders ausfaellt. Genau das habe ich am
            // 2026-10-23 gesehen (0 kHz, dann +7,4 kHz, dann +18,7)
            // und faelschlich fuer einen wandernden Versatz gehalten.
            //
            // Also BEIDE Haelften einzeln korrelieren. Dann gilt:
            //   linker ≈ rechter Versatz   -> reine Verschiebung
            //   linker ≈ MINUS rechter     -> reiner Maszstabsfehler
            // und aus dem Unterschied faellt der Faktor ab, mit dem
            // die Bild-FFT auf das Fenster gespannt wird.
            // Die Haelften brauchen ihre EIGENE Sicherheitsangabe —
            // dieselbe Lehre wie beim vollen Lauf, eine Ebene tiefer.
            // Am 2026-10-27 meldete die rechte Haelfte in zwei von
            // vier Berichten 110 bzw. 95 Punkte, waehrend der volle
            // Lauf eindeutig 0 sagte: auf einer Haelfte ohne kraeftiges
            // Signal rastet die Korrelation irgendwo ein. Ohne diese
            // Zahl haette ich daraus wieder einen Befund gemacht.
            auto halbLag = [&](int von, int bis, double* sicherheit) -> int {
                if (sicherheit) { *sicherheit = 0.0; }
                const int len = bis - von;
                if (len < 64) { return 0; }
                const int grenze = qMin(maxLag, len / 4);
                int best = 0; double bw = -1.0e30;
                QVector<double> hw(2 * grenze + 1, -1.0e30);
                for (int lag = -grenze; lag <= grenze; ++lag) {
                    double sum = 0.0; int zahl = 0;
                    for (int i = qMax(von, von - lag);
                         i < qMin(bis, bis - lag); ++i) {
                        sum += A[i] * B[i + lag]; ++zahl;
                    }
                    if (zahl > 0) {
                        sum /= zahl;
                        hw[lag + grenze] = sum;
                        if (sum > bw) { bw = sum; best = lag; }
                    }
                }
                double zweit = -1.0e30;
                for (int lag = -grenze; lag <= grenze; ++lag) {
                    if (std::abs(lag - best) <= 10) { continue; }
                    zweit = std::max(zweit, hw[lag + grenze]);
                }
                if (sicherheit && bw > 0.0 && zweit > -1.0e29) {
                    *sicherheit = zweit / bw;
                }
                return best;
            };
            lagLinks  = halbLag(0, n / 2, &sicherLinks);
            lagRechts = halbLag(n / 2, n, &sicherRechts);
            // Der Maszstab, der die beiden Haelften zur Deckung
            // braechte: die Haelften liegen im Mittel n/4 links und
            // rechts der Mitte, ihr Lag-Unterschied ist also
            // (Faktor - 1) * n/2.
            faktor = 1.0 + double(lagRechts - lagLinks) / (double(n) * 0.5);
        }

        qCInfo(lcSpectrum).nospace()
            << "Kachel: Bereich " << (lowHz / 1.0e6) << "–"
            << (highHz / 1.0e6) << " MHz ueber " << zeile.size()
            << " Bins, Fenster " << (fensterTief / 1.0e6) << "–"
            << ((fensterTief + m_bandwidthHz) / 1.0e6)
            << " MHz, Spitze bei " << (sHz / 1.0e6) << " MHz (Punkt "
            << sx << " von " << breite << ", Stufe " << stufen[sx] << ")";
        if (versatzMessbar) {
            qCInfo(lcSpectrum).nospace()
                << "Kachel gegen Spektrum: bester Versatz " << besterLag
                << " Punkte = " << (besterLag * hzProPunkt / 1.0e3)
                << " kHz; zweitbester Gipfel bei "
                << (zweitRel * 100.0) << " % davon, Wert bei Versatz 0 "
                << (bestWert > 0.0 ? beiNull / bestWert * 100.0 : 0.0)
                << " %"
                << (zweitRel > 0.8 ? " — NICHT eindeutig" : "");
            qCInfo(lcSpectrum).nospace()
                << "Kachel gegen Spektrum, haelftenweise: links "
                << lagLinks << " Punkte (zweitbester "
                << (sicherLinks * 100.0) << " %), rechts " << lagRechts
                << " Punkte (zweitbester " << (sicherRechts * 100.0)
                << " %)"
                << ((sicherLinks > 0.8 || sicherRechts > 0.8)
                        ? " — NICHT eindeutig" : "")
                << " — Maszstab " << faktor
                << " (Fenster " << (m_bandwidthHz / 1.0e3)
                << " kHz, Kachel " << ((highHz - lowHz) / 1.0e3)
                << " kHz, Verhaeltnis "
                << ((highHz - lowHz) / qMax(1.0, m_bandwidthHz)) << ")";
            // ── Und wovon die BILD-FFT ausgeht (2026-10-27) ──────
            //
            // Die fehlende Zahl in allen bisherigen Messungen: der
            // Ausschnitt, auf den die Bild-FFT gespannt wird. Er steht
            // in `m_ddcCenterHz`/`m_sampleRateHz`, gesetzt aus dem
            // Zustandsschub der Maschine — waehrend die Kachel ihren
            // eigenen Bereich in jedem Paket mitbringt. Stimmen die
            // beiden Mitten nicht ueberein, liegen die Bilder
            // auseinander, und zwar je nach Abstimmung verschieden
            // weit. Genau das meldet der Betreiber: „nicht auf allen
            // frequenzen gleich".
            qCInfo(lcSpectrum).nospace()
                << "Bild-FFT geht aus von " << ((m_ddcCenterHz
                        - m_sampleRateHz * 0.5) / 1.0e6)
                << "–" << ((m_ddcCenterHz + m_sampleRateHz * 0.5) / 1.0e6)
                << " MHz (Mitte " << (m_ddcCenterHz / 1.0e6)
                << "), Kachelmitte " << ((lowHz + highHz) * 0.5e-6)
                << " MHz, Unterschied "
                << (((lowHz + highHz) * 0.5 - m_ddcCenterHz) / 1.0e3)
                << " kHz";
        }
    }

    appendHistoryRow(stufen, jetzt);
    if (!m_wfLive) {
        rebuildWaterfallViewport();
        markOverlayDirty();
    }
}

void SpectrumWidget::pushWaterfallRow(const QVector<float>& wfPixelsDbm)
{
    if (wfPixelsDbm.isEmpty()) { return; }
    // Siehe `stelleWasserfallbildSicher`: ohne das hier blieb der
    // Wasserfall leer, bis jemand ins Bild klickte.
    if (!stelleWasserfallbildSicher()) { return; }

    // Task 2.8: Stop-on-TX -- skip if TX active and feature enabled.
    if (m_wfStopOnTx && m_activePeakHold.txActive()) {
        return;
    }

    // ── Kein Strom, keine Zeilen ────────────────────────────────────
    //
    // Der Antrieb hier ist ein ZEITGEBER (m_wfPushTimer), bewusst
    // entkoppelt von der Ankunft der FFT — das haelt den Bildlauf
    // gleichmaessig. Der Preis: reisst der Strom ab, schiebt der
    // Zeitgeber denselben alten Puffer unbegrenzt weiter. Die
    // Wasserfall-Automatik sieht dann ein voellig flaches Spektrum,
    // ihre Spanne faellt in sich zusammen, und JEDER Punkt landet auf
    // der obersten Palettenstufe. Das ist das Magenta.
    //
    // Der Betreiber am 2026-08-22: "die farbe magenta und das problem
    // hatten wir schon einmal ganz am anfang." Stimmt — zweimal, und
    // beide Male war die Ursache eine andere (eingefallenes
    // dB-Fenster, verdrehte Palette). Beide Behebungen stehen noch.
    // Dies ist die dritte Ursache desselben Bildes, und die einzige,
    // die man nur sieht, wenn das Geraet WEGFAELLT statt zu schweigen.
    //
    // Richtig ist einfrieren, nicht weiterschieben: ein stehender
    // Wasserfall sagt "hier endet der Empfang", eine magenta Flaeche
    // sagt gar nichts. Zwei Sekunden Nachlauf, damit ein Aussetzer im
    // Netz keinen Sprung ins Bild reisst.
    if (m_lastSpectrumArrivalMs > 0
        && QDateTime::currentMSecsSinceEpoch() - m_lastSpectrumArrivalMs > 2000) {
        return;
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

    const int n = wfPixelsDbm.size();
    int h = m_waterfall.height();
    // Decrement write pointer so newest row is always at m_wfWriteRow.
    m_wfWriteRow = (m_wfWriteRow - 1 + h) % h;
#ifdef LONGPATH_GPU_SPECTRUM
    ++m_wfRowsSinceUpload;
#endif

    int w = m_waterfall.width();
    QRgb* scanline = reinterpret_cast<QRgb*>(m_waterfall.scanLine(m_wfWriteRow));
    // Map source-pixel range to scanline pixels.
    //
    // ── Retina: zwei Quellpunkte je Zeilenpunkt (2026-09-25) ─────────
    //
    // Hier stand "typically 1:1". Das stimmt seit dem 2026-08-26 nicht
    // mehr: die Pipeline rechnet in GERAETE-Pixeln (displayWidth x dpr,
    // updateSpectrumLinear), das Wasserfallbild ist in LOGISCHEN Pixeln
    // angelegt (applyResizeSettled). Auf einem 2x-Schirm kommen also zwei
    // Quellpunkte auf einen Zeilenpunkt, und die alte Punktabtastung nahm
    // davon nur den ersten -- jede zweite Spalte fiel weg. Ein schmaler
    // Traeger (CW, FT8, Bake) lag in der Haelfte der Faelle genau in der
    // verworfenen Spalte: gestrichelt oder unsichtbar, waehrend das
    // Spektrum darueber ihn zeigte (Rendering-Werkbank 2026-09-25).
    //
    // Jetzt fasst die Regel des eingestellten Wasserfall-Detektors die
    // Quellpunkte zusammen, die auf den Zeilenpunkt fallen: Peak/Rosenfell
    // das Maximum, Average/RMS die mittlere Leistung, Sample der erste
    // Punkt (wie bisher -- das ist, was "Sample" verspricht). Bei 1:1 (und
    // beim Aufweiten) ist das in jedem Fall genau der alte Punkt.
    const float pxScale = static_cast<float>(n) / static_cast<float>(w);
    // Erst verdichten, dann Schwellen bilden: die Clarity-Verankerung und
    // die Automatik sollen die Werte sehen, die gleich eingefaerbt werden.
    // Mit den unverdichteten waere das Rauschen nach dem Maximum um gut
    // 1 dB heller geraten als vorher, bei gleichen Schwellen.
    m_wfRowReduced.resize(w);
    for (int x = 0; x < w; ++x) {
        m_wfRowReduced[x] = wfRowSourceReduce(wfPixelsDbm, x, pxScale,
                                              m_waterfallDetector);
    }

    // Issue #230 fix: threshold composition moved out — writes go to
    // the render-active mirror (m_wfActiveLow/High), never the
    // persisted user fields. Thetis-faithful per Thetis
    // display.cs:6575-6594 [v2.10.3.13].
    composeWaterfallActiveThresholds(m_wfRowReduced);

    // Einmal normieren, zweimal verwenden: die Intensität geht in die
    // Historie, die Farbe daraus in die sichtbare Zeile. Vorher wurde
    // gefärbt und die Farbe dann in die Historie kopiert — deshalb
    // konnte ein Schemawechsel die alten Zeilen nicht mehr erreichen.
    m_wfRowIntensity.resize(w);
    quint8* level = m_wfRowIntensity.data();
    // ── Fliesskomma statt quint8 — aber KEINE Streuung ──────────────
    //
    // Hier stand kurzzeitig eine Bayer-Streuung gegen Banding. Sie ist
    // wieder heraus, und die Messung ist der Grund.
    //
    // Die Ueberlegung war: der Betreiber wies am 2026-08-23 darauf hin,
    // dass die Anvelina mehr Dynamikumfang hat als die ANAN 10E, und
    // waterfallIntensity quantelte auf quint8 — 256 Stufen ueber die
    // ganze Spanne. Das klang nach der Ursache sichtbarer Stufen.
    //
    // tst_waterfall_banding hat nachgemessen, und das Ergebnis war
    // eindeutig: der laengste Lauf gleicher Farbe ist ACHT Bildpunkte,
    // bei 40 dB Fenster wie bei 140 dB, mit Streuung wie ohne. Bei
    // 1200 Bildpunkten Breite sind das 0,7 Prozent — kein sichtbares
    // Band. Die Grenze liegt nicht bei der Intensitaet, sondern beim
    // 8-Bit-Ausgang der Palette selbst, und dort ist sie unvermeidlich.
    //
    // Was BLEIBT, weil es nachweislich hilft: die Fliesskomma-Fassung
    // rundet, wo die alte abschnitt. Das war im Mittel eine halbe
    // Farbstufe je Kanal.
    //
    // Die Streuung waere kein Schaden gewesen — sie kostet fast nichts.
    // Aber sie waere Zierde ohne Wirkung im Weg des heissesten Pfades,
    // und ein Kommentar, der eine Wirkung behauptet, die es nicht gibt,
    // ist schlimmer als kein Kommentar.
    for (int x = 0; x < w; ++x) {
        const float f = waterfallIntensityF(m_wfRowReduced[x],
                                            m_wfActiveLowThreshold,
                                            m_wfActiveHighThreshold,
                                            m_wfBlackLevel, m_wfColorGain);
        level[x] = static_cast<quint8>(std::lround(f * 255.0f));
        scanline[x] = waterfallColorForIntensityF(f, m_wfColorScheme, m_wfLowColor);
    }

    // ── Sub-epic E: mirror the just-written row into the history ring ───
    // From AetherSDR SpectrumWidget.cpp:2808-2812 [@0cd4559]
    //   adapter: Longpath has a single FFT-derived path (no native tile
    //   path), so we always use QDateTime::currentMSecsSinceEpoch().
    {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        appendHistoryRow(level, nowMs);
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
    // Issue #230 fix: read the render-active mirror, not the
    // persistent user fields — AGC / NF-AGC / Clarity drive the
    // active mirror via composeWaterfallActiveThresholds() and
    // setClarityWaterfallThresholds().  Persistent values stay clean.
    return waterfallColor(dbm, m_wfActiveLowThreshold,
                          m_wfActiveHighThreshold,
                          m_wfBlackLevel, m_wfColorGain, m_wfColorScheme);
}

// ── Zwei Hälften statt einer ─────────────────────────────────────────
//
// Diese Funktion war ein Stück: dBm hinein, Farbe heraus. Sie ist jetzt
// in die zwei Schritte zerlegt, die ohnehin darin steckten —
// normieren, dann nachschlagen — weil die Historie nur den ersten
// braucht.
//
// Ein Byte je Punkt statt vier, und vor allem: die Historie hält damit
// eine Messung und keine Anzeige. Ein Wechsel des Farbschemas galt
// vorher erst ab der nächsten Zeile, und quer über den Wasserfall lief
// ein Streifen dort, wo umgeschaltet wurde.
//
// Die 256 Stufen sind kein Verlust, den man sieht: über eine typische
// 50-dB-Spanne ist eine Stufe 0,2 dB, und der Wasserfall hat ohnehin
// nur so viele Farben, wie die Farbtabelle hergibt.
quint8 SpectrumWidget::waterfallIntensity(float dbm, float lowDbm,
                                          float highDbm, int blackLevel,
                                          int colorGain)
{
    float effectiveLow  = lowDbm  + wfBlackLevelOffsetDb(blackLevel);
    float effectiveHigh = highDbm - wfColorGainOffsetDb(colorGain);

    // War `effectiveLow + 1.0f`, was keine Anzeige ist: ein Fenster von
    // einem Dezibel setzt jeden Punkt über dem Rauschen an die Spitze
    // der Farbtabelle, und die Spitze von ClarityBlue ist Magenta. Ein
    // zusammengefallenes Fenster ist immer ein Fehler weiter oben, aber
    // das Ergebnis soll ein kontrastarmes Bild sein und keine Fläche.
    if (effectiveHigh - effectiveLow < kWfMinSpanDb) {
        effectiveHigh = effectiveLow + kWfMinSpanDb;
    }

    const float range = effectiveHigh - effectiveLow;
    float adjusted = (dbm - effectiveLow) / range;
    // Ein NaN-Punkt ist unbekannt, nicht laut. Ohne diese Zeile fällt er
    // durch die Stufensuche — jeder Vergleich mit NaN ist falsch — und
    // nimmt die letzte Stufe: die hellste Farbe der Tabelle für fehlende
    // Daten.
    if (!std::isfinite(adjusted)) { adjusted = 0.0f; }
    adjusted = qBound(0.0f, adjusted, 1.0f);

    return static_cast<quint8>(std::lround(adjusted * 255.0f));
}

// Dieselbe Rechnung, aber OHNE die Quantelung am Ende. Begruendung an
// der Erklaerung in der Kopfdatei.
//
// Bewusst als eigene Funktion und nicht als Umbau der bestehenden: die
// quint8-Fassung wird an mehreren Stellen gerufen und von Pruefungen
// festgehalten. Zwei Funktionen mit einer gemeinsamen Rechnung waeren
// eine Doppelung, die auseinanderlaufen kann — darum ruft die alte
// jetzt die neue.
float SpectrumWidget::waterfallIntensityF(float dbm, float lowDbm,
                                          float highDbm, int blackLevel,
                                          int colorGain)
{
    float effectiveLow  = lowDbm  + wfBlackLevelOffsetDb(blackLevel);
    float effectiveHigh = highDbm - wfColorGainOffsetDb(colorGain);
    if (effectiveHigh - effectiveLow < kWfMinSpanDb) {
        effectiveHigh = effectiveLow + kWfMinSpanDb;
    }
    const float range = effectiveHigh - effectiveLow;
    float adjusted = (dbm - effectiveLow) / range;
    if (!std::isfinite(adjusted)) { adjusted = 0.0f; }
    return qBound(0.0f, adjusted, 1.0f);
}

// Die Palette, mit Fliesskomma-Eingang und RUNDUNG statt Abschneiden.
//
// static_cast<int> in der alten Fassung schneidet ab und verliert im
// Mittel eine halbe Farbstufe je Kanal — bei einem weichen Verlauf
// genau die Haelfte dessen, was man vermeiden will.
QRgb SpectrumWidget::waterfallColorForIntensityF(float level,
                                                 WfColorScheme scheme,
                                                 QColor lowColor)
{
    const float adjusted = qBound(0.0f, level, 1.0f);

    int stopCount = 0;
    const WfGradientStop* stops = wfSchemeStops(scheme, stopCount);

    // Thetis display.cs:2516/6764-6787 — waterfall_low_color replaces the
    // floor colour in the "enhanced" scheme only; every other scheme there
    // hardcodes its own floor and never reads it.
    WfGradientStop stop0 = stops[0];
    if (scheme == WfColorScheme::Enhanced) {
        stop0.r = lowColor.red();
        stop0.g = lowColor.green();
        stop0.b = lowColor.blue();
    }

    for (int i = 0; i < stopCount - 1; ++i) {
        const WfGradientStop& lo = (i == 0) ? stop0 : stops[i];
        const WfGradientStop& hi = stops[i + 1];
        if (adjusted <= hi.pos) {
            const float span = hi.pos - lo.pos;
            const float t = (span > 0.0f)
                                ? (adjusted - lo.pos) / span : 0.0f;
            const int r = int(std::lround(lo.r + t * (hi.r - lo.r)));
            const int g = int(std::lround(lo.g + t * (hi.g - lo.g)));
            const int b = int(std::lround(lo.b + t * (hi.b - lo.b)));
            return qRgb(qBound(0, r, 255), qBound(0, g, 255),
                        qBound(0, b, 255));
        }
    }
    return qRgb(stops[stopCount - 1].r,
                stops[stopCount - 1].g,
                stops[stopCount - 1].b);
}

QRgb SpectrumWidget::waterfallColorForIntensity(quint8 level,
                                                WfColorScheme scheme,
                                                QColor lowColor)
{
    const float adjusted = static_cast<float>(level) / 255.0f;

    int stopCount = 0;
    const WfGradientStop* stops = wfSchemeStops(scheme, stopCount);

    // Same low-colour substitution as waterfallColorForIntensityF() above
    // — kept in sync deliberately rather than delegating, matching how
    // this pair already duplicated the interpolation before this change.
    WfGradientStop stop0 = stops[0];
    if (scheme == WfColorScheme::Enhanced) {
        stop0.r = lowColor.red();
        stop0.g = lowColor.green();
        stop0.b = lowColor.blue();
    }

    for (int i = 0; i < stopCount - 1; ++i) {
        const WfGradientStop& lo = (i == 0) ? stop0 : stops[i];
        const WfGradientStop& hi = stops[i + 1];
        if (adjusted <= hi.pos) {
            const float t = (adjusted - lo.pos) / (hi.pos - lo.pos);
            const int r = static_cast<int>(lo.r + t * (hi.r - lo.r));
            const int g = static_cast<int>(lo.g + t * (hi.g - lo.g));
            const int b = static_cast<int>(lo.b + t * (hi.b - lo.b));
            return qRgb(r, g, b);
        }
    }
    return qRgb(stops[stopCount - 1].r,
                stops[stopCount - 1].g,
                stops[stopCount - 1].b);
}

QRgb SpectrumWidget::waterfallColor(float dbm, float lowDbm, float highDbm,
                                    int blackLevel, int colorGain,
                                    WfColorScheme scheme)
{
    // Die sichtbare Zeile geht denselben Weg wie die gespeicherte.
    // Täte sie es nicht, unterschieden sich die oberste Zeile und ihre
    // eigene Aufzeichnung eine Zeile später — ein Unterschied, den
    // niemand sucht, weil niemand ihn für möglich hält.
    return waterfallColorForIntensity(
        waterfallIntensity(dbm, lowDbm, highDbm, blackLevel, colorGain),
        scheme);
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
// CW slice. Die Werte kommen aus SliceModel
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
QVector<SpectrumWidget::SliceMarkerGeometry>
SpectrumWidget::sliceMarkerGeometry() const
{
    QVector<SliceMarkerGeometry> out;

    // Bis 2026-08-18 kam eine Marke JE FLAGGE, mit Frequenz und
    // Filterkanten aus der Flagge. Die Flaggen sind geloescht; es
    // bleibt die Marke dieses Panadapters aus seinen eigenen Werten —
    // genau der Zweig, der vorher fuer „noch keine Flagge angelegt"
    // dastand.
    //
    // Fuer Phase 3F gehoert hier eine Marke je zugeordneter Scheibe
    // her, aus SliceModel statt aus einem Widget. Das ist die bessere
    // Quelle: eine Marke, die aus der Anzeige liest, kann nicht
    // stimmen, wenn die Anzeige gerade nicht da ist.
    if (m_vfoHz > 0.0) {
        out.append(SliceMarkerGeometry{m_vfoHz, m_filterLowHz,
                                       m_filterHighHz, nullptr});
    }
    return out;
}

void SpectrumWidget::drawVfoMarker(QPainter& p, const QRect& specRect, const QRect& wfRect)
{
    for (const SliceMarkerGeometry& g : sliceMarkerGeometry()) {
        drawSliceMarker(p, specRect, wfRect, g);
    }
}

// Ported from AetherSDR SpectrumWidget.cpp:3211-3294
// Uses per-slice colors with exact alpha values from AetherSDR.
//
// Body unchanged by the Phase 3F split; it reads its centre and filter edges
// off the passed-in marker instead of the pan's m_vfoHz / m_filterLowHz /
// m_filterHighHz, so the same paint runs once per hosted slice.
void SpectrumWidget::drawSliceMarker(QPainter& p, const QRect& specRect,
                                     const QRect& wfRect,
                                     const SliceMarkerGeometry& g)
{
    int vfoX = hzToX(g.centreHz, specRect);

    // Per-slice color — from AetherSDR SliceColors.h:15-20
    // Slice 0 (A) = cyan, active
    static constexpr int kSliceR = 0x00, kSliceG = 0xd4, kSliceB = 0xff;

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

    // ── Im 3D-Modus gehoert der Spektrum-Teil NICHT hierher ──────────
    //
    // Der Betreiber am 2026-09-17, mit Bildschirmfoto im 3D-Modus:
    // "ich sehe meine bandbreite im unteren bild nicht von oben bis
    // unten." Zwei Ursachen, beide hier:
    //
    //   1. Diese Funktion malt in die STATISCHE Chrome-Schicht, und die
    //      liegt seit dem 2026-08-22 absichtlich UNTER der Kurve („die
    //      Rasterzellen muessen HINTER den Signalspitzen liegen"). Im
    //      2D-Modus ist die Kurve durchsichtig genug, die Saeule scheint
    //      durch. Die 3D-Flaeche (DssRenderer) ist aber DECKEND — sie
    //      verdeckt Saeule, Kanten und Mittellinie vollstaendig, im
    //      Spektrum blieb nur das Dreieck oben uebrig.
    //   2. Die Farbe: Tuerkis mit Alpha 80 auf schwarzem Grund ist eine
    //      Saeule; Tuerkis auf der blauen 3D-Flaeche und auf dem bunten
    //      Wasserfall ist nichts.
    //
    // Deshalb: im 3D-Modus malt paintPassbandOverSurface() den
    // Spektrum-Teil in die DYNAMISCHE Schicht, die als letzte ueber die
    // Flaeche kommt — hier bleibt dann nur der Wasserfall-Teil. Und der
    // Wasserfall bekommt eine HELLE Tuenche statt der Benutzerfarbe: so
    // macht es die Vorlage (vom 2026-09-17, live angesehen), und hell
    // steht auf jeder Wasserfallfarbe, tuerkis nur auf schwarz. Die
    // Benutzerfarbe (Setup > Display) gilt weiter fuer die Saeule im
    // 2D-Spektrum, wo sie sichtbar ist.
    const bool overSurface = (m_renderMode == SpectrumRenderMode::Mode3D);

    // Plan 4 follow-up (option A): MOX-gate the RX passband fill so it
    // disappears during TX/TUNE.  The TX filter overlay (drawTxFilterOverlay)
    // takes over in that state — gives the user a clear visual swap between
    // the cyan RX shadow and the orange TX band on MOX/TUNE engage.
    if (!m_moxOverlay) {
        // Spectrum passband fill — Plan 4 D9b: user-pickable m_rxFilterColor.
        // Previously hardcoded AetherSDR cyan alpha=35/25; now single user colour.
        if (specHeightClipped > 0 && !overSurface) {
            p.fillRect(xLo, specRect.top(), fW, specHeightClipped, m_rxFilterColor);
        }

        // Waterfall passband fill — hell, siehe oben.
        // Ab 2026-08-19 abschaltbar (m_extendedPassband, Vorgabe ein):
        // AetherSDRs setExtendedPassband, dort mit Vorgabe AUS. Wir haben
        // die Verlaengerung seit je unbedingt gemalt, also bleibt sie die
        // Vorgabe — der Schalter fuegt nur die Moeglichkeit hinzu.
        if (m_extendedPassband) {
            p.fillRect(xLo, wfRect.top(), fW, wfRect.height(), kPassbandWash);
        }
    }

    // Filter edge lines — from AetherSDR line 3237: slice color, alpha=130
    // Clip the spectrum-side edge to specBottomClipped so the line stops at
    // the bandplan strip's top edge.
    if (!overSurface) {
        p.setPen(QPen(QColor(kSliceR, kSliceG, kSliceB, 130), 1));
        p.drawLine(xLo, specRect.top(), xLo, specBottomClipped);
        p.drawLine(xHi, specRect.top(), xHi, specBottomClipped);
    }
    // Die Wasserfall-Haelfte der Kanten haengt am selben Schalter wie die
    // Mittellinie: bei AetherSDR enden die Kanten immer am Spektrum, und
    // nur die Frequenzlinie hat den Schalter. Ein dritter Schalter fuer
    // die Kanten allein waere eine Einstellung, die niemand sucht — Kante
    // und Linie gehoeren zum selben Bild. Hell wie die Tuenche, aus
    // demselben Grund.
    if (m_extendedFrequencyLine) {
        p.setPen(QPen(kPassbandEdgeLight, 1));
        p.drawLine(xLo, wfRect.top(), xLo, wfRect.bottom());
        p.drawLine(xHi, wfRect.top(), xHi, wfRect.bottom());
    }

    // VFO center line — from AetherSDR line 3281: slice color, alpha=220, width=2
    // Width narrows to 1 when filter edge is ≤4px away (CW modes)
    qreal vfoLineW = (std::abs(vfoX - xLo) <= 4 || std::abs(vfoX - xHi) <= 4) ? 1.0 : 2.0;
    p.setPen(QPen(QColor(kSliceR, kSliceG, kSliceB, 220), vfoLineW));
    // Port aus AetherSDR: drawFrequencyLine malt immer im Spektrum und nur
    // bei m_extendedFrequencyLine auch im Wasserfall
    // (SpectrumWidget.cpp:16843-16848 [@0cd4559]).
    // Im 3D-Modus nur der Wasserfall-Teil; das Spektrum uebernimmt
    // paintPassbandOverSurface().
    if (!overSurface) {
        p.drawLine(vfoX, specRect.top(), vfoX,
                   m_extendedFrequencyLine ? wfRect.bottom() : specBottomClipped);
    } else if (m_extendedFrequencyLine) {
        p.drawLine(vfoX, wfRect.top(), vfoX, wfRect.bottom());
    }

    // VFO triangle marker — from AetherSDR line 3285-3293
    // Drawn below any VFO flag widget that may be positioned at the top.
    // Ohne Flagge sitzt es am oberen Spektrumsrand. Der Zweig fuer
    // „haengt an der Unterkante einer Flagge" steht weiter unten und
    // wartet auf das, was im Zielbild an ihre Stelle tritt.
    if (vfoX >= specRect.left() && vfoX <= specRect.right()) {
        static constexpr int kTriHalf = 6;
        static constexpr int kTriH = 10;

        int triTop = specRect.top();
        // If VFO flag is present, position triangle below it.
        //
        // DIESER Marke ihre Flagge, nicht die der Scheibe 0 — solange
        // eine Marke je Panadapter gezeichnet wurde, war der Unterschied
        // folgenlos; je Scheibe haengte er sonst jedes Dreieck an die
        // Hoehe von Scheibe A.
        if (g.flag && g.flag->isVisible()) {
            triTop = g.flag->y() + g.flag->height();
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

    // Bandbreite ablesbar (2026-11-08): Zahl am Durchlass, beim Ziehen
    // die Kante und ihr Wert. Nicht waehrend des Sendens (dort steht die
    // TX-Marke mit eigener Beschriftung) und nicht im 3D-Modus.
    if (!m_moxOverlay && !overSurface) {
        paintBandbreite(p, specRect, xLo, xHi, g.centreHz, specBottomClipped,
                        g.filterLowHz, g.filterHighHz);
    }
}

// ── Durchlass und Bandbreite (2026-11-08) ────────────────────────────
//
// Die vier Entwuerfe, die der Betreiber gewaehlt hat („Alle vier
// bauen"): Bandbreite ablesbar, Signal-Klammern, Lueckenfinder,
// Durchlass-Lupe. Gemessen wird in gui/spectrum/Durchlass.*, hier wird
// nur gezeichnet. Farben ausschliesslich aus StyleConstants: Bernstein
// heisst gemessen, Akzentblau heisst anklickbar (HAUSSTIL).

namespace {

QFont durchlassSchrift(const QFont& basis, int px, bool fett)
{
    QFont f(basis);
    f.setPixelSize(px);
    f.setBold(fett);
    return f;
}

// Ein Glas-Chip mit Text. `anker` ist der Punkt, an dem `ausr` den Chip
// festmacht (wie bei QPainter::drawText mit Rechteck).
QRectF durchlassChip(QPainter& p, const QPointF& anker, Qt::Alignment ausr,
                     const QString& text, const QFont& f, const QColor& farbe)
{
    const QFontMetricsF fm(f);
    const qreal w = fm.horizontalAdvance(text) + 10.0;
    const qreal h = fm.height() + 4.0;
    qreal x = anker.x();
    qreal y = anker.y();
    if (ausr & Qt::AlignHCenter)     { x -= w / 2.0; }
    else if (ausr & Qt::AlignRight)  { x -= w; }
    if (ausr & Qt::AlignVCenter)     { y -= h / 2.0; }
    else if (ausr & Qt::AlignBottom) { y -= h; }
    const QRectF r(x, y, w, h);
    QColor grund(Style::kGlassPanelBot);
    grund.setAlpha(225);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(Style::kBorder), 1.0));
    p.setBrush(grund);
    p.drawRoundedRect(r, Style::kGlassChipRadius, Style::kGlassChipRadius);
    p.setFont(f);
    p.setPen(farbe);
    p.drawText(r, Qt::AlignCenter, text);
    p.restore();
    return r;
}

} // namespace

void SpectrumWidget::paintBandbreite(QPainter& p, const QRect& specRect,
                                     int xLo, int xHi, double centreHz,
                                     int specBottomClipped, int loHz, int hiHz)
{
    if (!m_zeigeBandbreite || specBottomClipped <= specRect.top()) {
        return;
    }
    const QFont gross = durchlassSchrift(p.font(), Style::kFontSmall, true);
    const QFont klein = durchlassSchrift(p.font(), Style::kFontCaption, false);
    const QColor text(Style::kTextPrimary);
    const QColor leise(Style::kTextSecondary);
    const QString breite = Durchlass::bandbreiteText(loHz, hiHz);
    const int fW = xHi - xLo;
    const qreal chipW = QFontMetricsF(gross).horizontalAdvance(breite) + 10.0;
    const int vfoX = hzToX(centreHz, specRect);

    if (fW >= chipW + 8.0) {
        // Breit genug: die Zahl oben im Durchlass, unter dem Dreieck,
        // und wenn auch das passt, die beiden Kanten unten.
        durchlassChip(p, QPointF((xLo + xHi) / 2.0, specRect.top() + 14),
                      Qt::AlignHCenter | Qt::AlignTop, breite, gross, text);
        const QFontMetricsF fk(klein);
        const QString lo = QString::number(std::abs(loHz));
        const QString hi = QString::number(std::abs(hiHz));
        if (fW >= fk.horizontalAdvance(lo) + fk.horizontalAdvance(hi) + 16.0) {
            p.save();
            p.setFont(klein);
            p.setPen(leise);
            const int links  = (loHz <= hiHz) ? xLo : xHi;   // wo die untere Kante steht
            const QString lt = (links == xLo) ? lo : hi;
            const QString rt = (links == xLo) ? hi : lo;
            // Ueber der Leiste des Lueckenfinders (die liegt in den
            // untersten 12 Punkten), sonst deckt ein Lueckenbalken die Zahl.
            const qreal grund = specBottomClipped - 16.0;
            p.drawText(QPointF(xLo + 4, grund), lt);
            p.drawText(QPointF(xHi - 4 - fk.horizontalAdvance(rt), grund), rt);
            p.restore();
        }
    } else {
        // Schmal (breites Bild, CW): die Zahl neben der Marke.
        durchlassChip(p, QPointF(vfoX + 10, specRect.top() + 30),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      Durchlass::bandbreiteKurz(loHz, hiHz), gross, text);
    }

    // Beim Ziehen einer Kante: die Kante hell, ihr Wert daneben.
    if (m_draggingFilter != FilterEdge::None) {
        const bool unten = (m_draggingFilter == FilterEdge::Low);
        const int wert   = unten ? loHz : hiHz;
        const int xK     = hzToX(centreHz + wert, specRect);
        p.save();
        p.setPen(QPen(text, 2.0));
        p.drawLine(xK, specRect.top(), xK, specBottomClipped);
        p.restore();
        const bool rechts = (xK >= (xLo + xHi) / 2);
        const Qt::Alignment a = (rechts ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter;
        const qreal x = rechts ? xK + 8.0 : xK - 8.0;
        const qreal y = specRect.top() + 0.45 * (specBottomClipped - specRect.top());
        durchlassChip(p, QPointF(x, y), a, QStringLiteral("%1 Hz").arg(std::abs(wert)),
                      gross, text);
        durchlassChip(p, QPointF(x, y + 24), a,
                      tr("Width %1 · %2").arg(breite,
                          m_kanteFein ? tr("fine") : tr("snap %1 Hz").arg(Durchlass::kKantenRastHz)),
                      klein, leise);
    }
}

void SpectrumWidget::werteDurchlassAus(bool sofort)
{
    if (!m_zeigeKlammern && !m_zeigeLuecken) {
        return;
    }
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    if (!sofort && jetzt - m_durchlassZuletztMs < 200) {
        return;
    }
    m_durchlassZuletztMs = jetzt;

    const int n = m_renderedPixels.size();
    if (n < 16 || m_bandwidthHz <= 0.0) {
        m_klammerHaltung.leeren();
        m_luecken.clear();
        m_besteLuecke = -1;
        return;
    }
    // Punkt i liegt bei links + i * Breite/(n-1) -- dieselbe Abbildung
    // wie die Kurve und die Spitzenanzeige.
    const double hzJePx = m_bandwidthHz / (n - 1);
    const double links  = m_centerHz - m_bandwidthHz / 2.0;
    const float boden   = Durchlass::rauschboden(m_renderedPixels);
    m_klammerBoden = boden;

    if (m_zeigeKlammern) {
        m_klammerHaltung.einspielen(
            Durchlass::erkenneSignale(m_renderedPixels, hzJePx, boden));
    }
    if (m_zeigeLuecken) {
        // Gesucht wird im Bandplan-Abschnitt, in dem der VFO steht (auf 2 m:
        // SSB/CW/DIGI 144,150-144,400); ohne Bandplan im ganzen Bild.
        int von = 0;
        int bis = n;
        if (m_bandPlanMgr) {
            const double vfoMhz = rfFuerAnzeige(m_vfoHz) / 1.0e6;
            for (const auto& seg : m_bandPlanMgr->segments()) {
                if (vfoMhz >= seg.lowMhz && vfoMhz < seg.highMhz) {
                    von = static_cast<int>(std::floor(
                        (zfFuerAchse(seg.lowMhz * 1.0e6) - links) / hzJePx));
                    bis = static_cast<int>(std::ceil(
                        (zfFuerAchse(seg.highMhz * 1.0e6) - links) / hzJePx));
                    break;
                }
            }
        }
        m_luecken = Durchlass::findeLuecken(m_renderedPixels, hzJePx, von, bis, boden);
        m_besteLuecke = -1;
        double bester = 1.0e18;
        const double vfoPx = (m_vfoHz - links) / hzJePx;
        for (int i = 0; i < m_luecken.size(); ++i) {
            const double d = std::abs(0.5 * (m_luecken[i].a + m_luecken[i].e) - vfoPx);
            if (d < bester) { bester = d; m_besteLuecke = i; }
        }
    }
#ifdef LONGPATH_GPU_SPECTRUM
    m_overlayDynamicDirty = true;
#endif
}

QImage SpectrumWidget::paintPassbandOverlaysForTest()
{
    QImage img(size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    const QRect r = spectrumRect();
    paintSignalKlammern(p, r);
    paintLueckenfinder(p, r);
    paintDurchlassLupe(p, r);
    return img;
}

// Die Lupe im Datenweg nachfuehren, nicht beim Malen: gemalt wird nur,
// wenn ein Bild entsteht, gemittelt werden muss aber jedes Spektrum --
// sonst zeigt die Lupe ein einzelnes Rohbild, waehrend das Hauptbild
// ueber die eingestellte Zeit mittelt (in der Werkbank am 2026-11-08
// so gesehen: die SSB-Station unter dem VFO fehlte in der Lupe).
void SpectrumWidget::fuehreLupeNach()
{
    if (!lupeSichtbar()) {
        return;
    }
    const int binZahl = m_fullLinearBins.size();
    const double binHz = m_sampleRateHz / binZahl;
    const double fftUnten = m_ddcCenterHz - m_sampleRateHz / 2.0;
    const Durchlass::BinBereich b = Durchlass::lupenBins(
        binZahl, binHz, fftUnten,
        m_vfoHz - Durchlass::kLupeHalbHz, m_vfoHz + Durchlass::kLupeHalbHz);
    if (b.leer()) {
        return;
    }
    // Bins in dBm wie der Detektor "Average" (Bin / ENB, mal 10^(Versatz/10),
    // dann 10 log10), ueber die Zeit geglaettet (etwa 200 ms bei 30 Bildern).
    const int m = b.letzter - b.erster + 1;
    const double skala = std::pow(10.0, m_lastDbmOffset / 10.0) / m_fftWindowEnb;
    const bool neu = (m_lupeErster != b.erster || m_lupeGlatt.size() != m);
    if (neu) {
        m_lupeGlatt.resize(m);
        m_lupeErster = b.erster;
    }
    for (int k = 0; k < m; ++k) {
        const float roh = static_cast<float>(
            10.0 * std::log10(skala * m_fullLinearBins[b.erster + k] + 1.0e-30));
        m_lupeGlatt[k] = neu ? roh : 0.85f * m_lupeGlatt[k] + 0.15f * roh;
    }
    const Durchlass::Hoehe ziel = Durchlass::lupenHoehe(m_lupeGlatt);
    m_lupeHoehe = (neu || !m_lupeHoeheDa) ? ziel : Durchlass::fuehreNach(m_lupeHoehe, ziel);
    m_lupeHoeheDa = true;
}

bool SpectrumWidget::starteTraegerMessungBei(const QPoint& widgetPos)
{
    const QRect r = notchSpecRect();
    if (!r.contains(widgetPos)) {
        return false;
    }
    return starteTraegerMessung(xToHz(widgetPos.x(), r));
}

bool SpectrumWidget::starteTraegerMessung(double geraetHz)
{
    if (geraetHz <= 0.0 || m_fullLinearBins.isEmpty() || m_sampleRateHz <= 0.0) {
        return false;
    }
    m_eichZielHz = geraetHz;
    m_eichSumme.fill(0.0, m_fullLinearBins.size());
    m_eichBilder = 0;
    return true;
}

// Im Datenweg, wie die Lupe: jedes Spektrum zaehlt, nicht nur jedes
// gemalte. Zehn Bilder (Thetis console.cs:9745), dann suchen und melden.
void SpectrumWidget::fuehreEichungNach()
{
    if (m_eichZielHz <= 0.0) {
        return;
    }
    const int n = m_fullLinearBins.size();
    if (n != m_eichSumme.size()) {          // FFT umgestellt: neu anfangen
        m_eichSumme.fill(0.0, n);
        m_eichBilder = 0;
    }
    for (int k = 0; k < n; ++k) {
        m_eichSumme[k] += static_cast<double>(m_fullLinearBins[k]);
    }
    if (++m_eichBilder < FrequenzEichung::kMittelBilder) {
        return;
    }
    const double binHz = m_sampleRateHz / n;
    const double fftUnten = m_ddcCenterHz - m_sampleRateHz / 2.0;
    const FrequenzEichung::Treffer t = FrequenzEichung::sucheTraeger(
        m_eichSumme, binHz, fftUnten, m_eichZielHz);
    m_eichZielHz = 0.0;
    m_eichSumme.clear();
    m_eichBilder = 0;
    emit traegerGemessen(t.gefunden, t.hz, t.gefunden ? rfFuerAnzeige(t.hz) : 0.0,
                         t.snrDb);
}

void SpectrumWidget::meldeBandLage()
{
    if (!m_bandLageMelden) {
        return;
    }
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    if (jetzt - m_bandLageZuletztMs < 100) {
        return;
    }
    const int n = m_renderedPixels.size();
    if (n < 16 || m_bandwidthHz <= 0.0) {
        return;
    }
    m_bandLageZuletztMs = jetzt;
    const float boden = Durchlass::rauschboden(m_renderedPixels);
    double ueber = 0.0;
    for (float v : m_renderedPixels) {
        ueber += std::max(0.0f, v - (boden + 6.0f));
    }
    const int signale = Durchlass::erkenneSignale(
        m_renderedPixels, m_bandwidthHz / (n - 1), boden).size();
    emit bandLage(boden, static_cast<float>(ueber / n), signale);
}

bool SpectrumWidget::lupeSichtbar() const
{
    if (m_lupenModus == 0 || m_moxOverlay || m_fullLinearBins.isEmpty()
        || m_sampleRateHz <= 0.0 || m_bandwidthHz <= 0.0) {
        return false;
    }
    // An heisst sichtbar, bei jeder Spanne (siehe Durchlass.h, Lupe).
    return true;
}

void SpectrumWidget::paintSignalKlammern(QPainter& p, const QRect& specRect)
{
    if (!m_zeigeKlammern || m_moxOverlay) {
        return;
    }
    const QVector<Durchlass::Signal>& sig = m_klammerHaltung.gezeigt();
    const int n = m_renderedPixels.size();
    if (sig.isEmpty() || n < 2) {
        return;
    }
    const double pxW    = static_cast<double>(specRect.width()) / (n - 1);
    const double hzJePx = m_bandwidthHz / (n - 1);
    const double links  = m_centerHz - m_bandwidthHz / 2.0;
    const bool spitzen  = m_activePeakHold.enabled() && m_activePeakHold.size() == n;

    QColor bernstein(Style::kAmberText);
    bernstein.setAlpha(235);
    const QFont f = durchlassSchrift(p.font(), Style::kFontCaption, true);
    const QFontMetricsF fm(f);

    // Die Station im Durchlass bekommt die ganze Zeile.
    const double mitteDurchlassPx =
        (m_vfoHz + 0.5 * (m_filterLowHz + m_filterHighHz) - links) / hzJePx;
    int eigen = -1;
    for (int i = 0; i < sig.size(); ++i) {
        if (mitteDurchlassPx >= sig[i].a - 2 && mitteDurchlassPx <= sig[i].e + 1) {
            eigen = i;
            break;
        }
    }

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setFont(f);
    QVector<QRectF> belegt;
    for (int i = 0; i < sig.size(); ++i) {
        const Durchlass::Signal& s = sig[i];
        // Ueber dem, was gezeichnet ist: Kurve UND Spitzenhaltelinie.
        float oben = -1.0e30f;
        for (int k = std::max(0, s.a); k < std::min(n, s.e); ++k) {
            oben = std::max(oben, m_renderedPixels[k]);
            if (spitzen) { oben = std::max(oben, m_activePeakHold.peak(k)); }
        }
        if (oben < -1.0e29f) { continue; }
        const qreal x0 = specRect.left() + (s.a - 0.5) * pxW - 1.0;
        const qreal x1 = specRect.left() + (s.e - 0.5) * pxW + 1.0;
        const qreal y  = dbmToYf(oben, specRect) - 6.0;
        if (y < specRect.top() + 4) { continue; }
        QPainterPath klammer;
        klammer.moveTo(x0, y + 5.0);
        klammer.lineTo(x0, y);
        klammer.lineTo(x1, y);
        klammer.lineTo(x1, y + 5.0);
        p.setPen(QPen(bernstein, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(klammer);

        const QString zahl = QString::number(qRound(s.snrDb));
        const qreal tw = fm.horizontalAdvance(zahl);
        QRectF r((x0 + x1 - tw) / 2.0, y - 3.0 - fm.height(), tw, fm.height());
        // Hoechstens eine Zeile ausweichen; findet die Zahl dann keinen
        // Platz, bleibt nur die Klammer. Ein Turm aus Zahlen (2-m-Contest
        // im 4-MHz-Bild, 2026-11-08) liest niemand.
        bool frei = false;
        for (int versuch = 0; versuch < 2 && !frei; ++versuch) {
            frei = true;
            for (const QRectF& b : belegt) {
                if (b.adjusted(-2, 0, 2, 0).intersects(r)) { frei = false; break; }
            }
            if (!frei) { r.translate(0, -fm.height()); }
        }
        if (!frei) { continue; }
        belegt << r;
        p.setPen(bernstein);
        p.drawText(r, Qt::AlignCenter, zahl);
    }
    p.restore();

    if (eigen >= 0) {
        const Durchlass::Signal& s = sig[eigen];
        const double mhz = rfFuerAnzeige(links + s.mittePx * hzJePx) / 1.0e6;
        const QString zeile = tr("%1 · %2 kHz · SNR %3 dB")
            .arg(mhz, 0, 'f', 4)
            .arg(s.breiteHz / 1000.0, 0, 'f', 1)
            .arg(qRound(s.snrDb));
        const int vfoX = hzToX(m_vfoHz, specRect);
        durchlassChip(p, QPointF(vfoX + 14, specRect.top() + 0.22 * specRect.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, zeile,
                      durchlassSchrift(p.font(), Style::kFontSmall, true), bernstein);
    }
}

void SpectrumWidget::paintLueckenfinder(QPainter& p, const QRect& specRect)
{
    m_lueckenKlick.clear();
    const int n = m_renderedPixels.size();
    if (!m_zeigeLuecken || m_moxOverlay || m_luecken.isEmpty() || n < 2) {
        return;
    }
    const double pxW    = static_cast<double>(specRect.width()) / (n - 1);
    const double hzJePx = m_bandwidthHz / (n - 1);
    const double links  = m_centerHz - m_bandwidthHz / 2.0;
    const qreal boden   = specRect.bottom() - bandPlanStripHeight();

    QColor blau(Style::kAccent);
    blau.setAlpha(210);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    for (int i = 0; i < m_luecken.size(); ++i) {
        const Durchlass::Luecke& l = m_luecken[i];
        const qreal x0 = specRect.left() + l.a * pxW;
        const qreal x1 = specRect.left() + l.e * pxW;
        const QRectF balken(x0, boden - 7.0, std::max<qreal>(2.0, x1 - x0), 4.0);
        p.setPen(Qt::NoPen);
        p.setBrush(blau);
        p.drawRoundedRect(balken, 2.0, 2.0);
        const double mitteZf = links + 0.5 * (l.a + l.e) * hzJePx;
        m_lueckenKlick.push_back({balken.adjusted(0, -8, 0, 5), mitteZf});

        if (i == m_besteLuecke) {
            p.setPen(QPen(QColor(Style::kTextPrimary), 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(balken.adjusted(-1, -2, 1, 2), 3.0, 3.0);
            const qreal xm = 0.5 * (x0 + x1);
            p.drawLine(QPointF(xm, boden - 10.0), QPointF(xm, boden - 30.0));
            const QString text = tr("CQ %1 · %2 kHz free")
                .arg(rfFuerAnzeige(mitteZf) / 1.0e6, 0, 'f', 3)
                .arg(qRound((l.e - l.a) * hzJePx / 1000.0));
            const QRectF chip = durchlassChip(
                p, QPointF(xm, boden - 30.0), Qt::AlignHCenter | Qt::AlignBottom, text,
                durchlassSchrift(p.font(), Style::kFontSmall, true), QColor(Style::kTextPrimary));
            m_lueckenKlick.push_back({chip, mitteZf});
        }
    }
    p.restore();
}

void SpectrumWidget::paintDurchlassLupe(QPainter& p, const QRect& specRect)
{
    if (!lupeSichtbar()) {
        return;
    }
    const int binZahl = m_fullLinearBins.size();
    const double binHz = m_sampleRateHz / binZahl;
    const double fftUnten = m_ddcCenterHz - m_sampleRateHz / 2.0;
    const double von = m_vfoHz - Durchlass::kLupeHalbHz;
    const double bis = m_vfoHz + Durchlass::kLupeHalbHz;
    const Durchlass::BinBereich b = Durchlass::lupenBins(binZahl, binHz, fftUnten, von, bis);
    const int m = b.letzter - b.erster + 1;
    // Die Werte fuehrt fuehreLupeNach() im Datenweg; passen sie nicht zum
    // Bereich (gerade umgestimmt), dieses eine Bild auslassen.
    if (b.leer() || m < 3 || m_lupeErster != b.erster || m_lupeGlatt.size() != m) {
        return;
    }

    const int bandH = bandPlanStripHeight();
    const qreal h = std::min<qreal>(170.0, (specRect.height() - bandH) * 0.42);
    const qreal w = std::min<qreal>(420.0, specRect.width() * 0.40);
    if (h < 60.0 || w < 160.0) {
        return;
    }
    // Auf die Seite, die vom VFO abgewandt ist: neben dem VFO stehen die
    // Zahl am Durchlass und die Zeile der eigenen Station.
    const bool vfoRechts = hzToX(m_vfoHz, specRect) > specRect.center().x();
    const qreal xRahmen = vfoRechts ? specRect.left() + 12.0 : specRect.right() - w - 12.0;
    const QRectF rahmen(xRahmen, specRect.top() + 28.0, w, h);
    const QRectF flaeche = rahmen.adjusted(8.0, 20.0, -8.0, -6.0);

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient verlauf(rahmen.topLeft(), rahmen.bottomLeft());
    QColor oben(Style::kGlassPanelTop);
    QColor unten(Style::kGlassPanelBot);
    // Deckend: was darunter liegt (Klammer-Zahlen), soll nicht durchscheinen.
    oben.setAlpha(255);
    unten.setAlpha(255);
    verlauf.setColorAt(0.0, oben);
    verlauf.setColorAt(1.0, unten);
    p.setPen(QPen(QColor(Style::kBorder), 1.0));
    p.setBrush(verlauf);
    p.drawRoundedRect(rahmen, Style::kGlassPanelRadius, Style::kGlassPanelRadius);

    const QFont kopf = durchlassSchrift(p.font(), Style::kFontCaption, true);
    p.setFont(kopf);
    p.setPen(QColor(Style::kTextSecondary));
    const QRectF kopfZeile(rahmen.left() + 10.0, rahmen.top() + 3.0, rahmen.width() - 20.0, 16.0);
    const QString rechts = tr("%1 Hz/bin").arg(binHz, 0, 'f', binHz < 10.0 ? 1 : 0);
    QString links = tr("MAGNIFIER ±%1 kHz · %2")
                        .arg(qRound(Durchlass::kLupeHalbHz / 1000.0))
                        .arg(rfFuerAnzeige(m_vfoHz) / 1.0e6, 0, 'f', 3);
    // Auf 2 m ist die Frequenz eine Stelle laenger, und die Zeile lief
    // in "409 Hz/bin" hinein (Werkbank z3, 2026-11-08). Dann ohne sie --
    // die Frequenz steht ohnehin am VFO.
    const QFontMetricsF kopfMass(kopf);
    if (kopfMass.horizontalAdvance(links) + kopfMass.horizontalAdvance(rechts) + 12.0
        > kopfZeile.width()) {
        links = tr("MAGNIFIER ±%1 kHz").arg(qRound(Durchlass::kLupeHalbHz / 1000.0));
    }
    p.drawText(kopfZeile, Qt::AlignLeft | Qt::AlignVCenter, links);
    p.drawText(kopfZeile, Qt::AlignRight | Qt::AlignVCenter, rechts);

    p.setClipRect(flaeche);
    auto xVon = [&](double hz) {
        return flaeche.left() + (hz - von) / (bis - von) * flaeche.width();
    };
    // Eigener Hoehenmassstab (Durchlass::lupenHoehe), nicht der des
    // Hauptbilds -- dort waren die Ausschlaege kaum zu sehen.
    const float boden  = m_lupeHoehe.unten;
    const float spanne = std::max(1.0f, m_lupeHoehe.oben - m_lupeHoehe.unten);
    auto yVon = [&](float dbm) {
        const float frac = qBound(0.0f, (dbm - boden) / spanne, 1.0f);
        return flaeche.bottom() - frac * flaeche.height();
    };
    // Alle 10 dB eine feine Linie, auf runden Werten der Anzeige
    // (mit Kalibrierversatz), damit die Hoehe ablesbar bleibt.
    {
        const float verschieb = m_dbmCalOffset + normalizeShiftDb();
        p.setPen(QPen(m_gridFineColor, 1.0));
        for (float a = std::ceil((boden + verschieb) / 10.0f) * 10.0f;
             a < boden + verschieb + spanne; a += 10.0f) {
            const qreal y = yVon(a - verschieb);
            p.drawLine(QPointF(flaeche.left(), y), QPointF(flaeche.right(), y));
        }
    }

    // Durchlass und VFO wie im Hauptbild (dieselbe Farbe des Betreibers).
    const qreal xa = xVon(m_vfoHz + std::min(m_filterLowHz, m_filterHighHz));
    const qreal xe = xVon(m_vfoHz + std::max(m_filterLowHz, m_filterHighHz));
    p.fillRect(QRectF(xa, flaeche.top(), xe - xa, flaeche.height()), m_rxFilterColor);

    // Kurve: bei mehr Bins als Punkten je Spalte das Maximum.
    QPainterPath kurve;
    const int spalten = static_cast<int>(flaeche.width());
    const bool verdichten = m > spalten;
    bool erster = true;
    auto punkt = [&](qreal x, float dbm) {
        const QPointF q(x, yVon(dbm));
        if (erster) { kurve.moveTo(q); erster = false; } else { kurve.lineTo(q); }
    };
    if (verdichten) {
        for (int c = 0; c < spalten; ++c) {
            const int k0 = c * m / spalten;
            const int k1 = std::max(k0 + 1, (c + 1) * m / spalten);
            float v = m_lupeGlatt[k0];
            for (int k = k0 + 1; k < k1 && k < m; ++k) { v = std::max(v, m_lupeGlatt[k]); }
            punkt(flaeche.left() + c + 0.5, v);
        }
    } else {
        for (int k = 0; k < m; ++k) {
            punkt(xVon(fftUnten + (b.erster + k + 0.5) * binHz), m_lupeGlatt[k]);
        }
    }
    QPainterPath fuellung = kurve;
    fuellung.lineTo(kurve.currentPosition().x(), flaeche.bottom());
    fuellung.lineTo(kurve.elementAt(0).x, flaeche.bottom());
    fuellung.closeSubpath();
    QColor fuell(m_fillColor);
    fuell.setAlpha(70);
    p.fillPath(fuellung, fuell);
    QColor linie(m_fillColor);
    linie.setAlpha(230);
    p.setPen(QPen(linie, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(kurve);

    QColor vfoFarbe(m_rxFilterColor);
    vfoFarbe.setAlpha(220);
    p.setPen(QPen(vfoFarbe, 1.0));
    const qreal xv = xVon(m_vfoHz);
    p.drawLine(QPointF(xv, flaeche.top()), QPointF(xv, flaeche.bottom()));
    p.restore();
}

// ---- Die Bandbreite UEBER der 3D-Flaeche ----
//
// Gegenstueck zu drawSliceMarker fuer den 3D-Modus: derselbe Spektrum-
// Teil (Saeule, Kanten, Mittellinie, Dreieck), aber in die dynamische
// Schicht gemalt, die als letzte ueber die deckende DSS-Flaeche kommt —
// siehe die Begruendung in drawSliceMarker. Hell statt tuerkis, weil
// die Flaeche blau ist. Der Wasserfall-Teil bleibt in drawSliceMarker.
void SpectrumWidget::paintPassbandOverSurface(QPainter& p, const QRect& specRect)
{
    if (m_renderMode != SpectrumRenderMode::Mode3D) { return; }

    const int bandPlanH = (m_bandPlanMgr && m_bandPlanFontSize > 0)
                          ? (m_bandPlanFontSize + 4) : 0;
    const int specBottomClipped = specRect.bottom() - bandPlanH;
    const int specHeightClipped = std::max(0, specBottomClipped - specRect.top());
    if (specHeightClipped <= 0) { return; }

    for (const SliceMarkerGeometry& g : sliceMarkerGeometry()) {
        const int vfoX = hzToX(g.centreHz, specRect);
        int xLo = hzToX(g.centreHz + g.filterLowHz, specRect);
        int xHi = hzToX(g.centreHz + g.filterHighHz, specRect);
        if (xLo > xHi) { std::swap(xLo, xHi); }
        const int fW = xHi - xLo;

        // MOX-Sperre wie im 2D-Zweig: waehrend TX/TUNE zeigt
        // drawTxFilterOverlay das Sendeband, nicht wir den Empfang.
        if (!m_moxOverlay) {
            p.fillRect(xLo, specRect.top(), fW, specHeightClipped, kPassbandWash);
        }
        p.setPen(QPen(kPassbandEdgeLight, 1));
        p.drawLine(xLo, specRect.top(), xLo, specBottomClipped);
        p.drawLine(xHi, specRect.top(), xHi, specBottomClipped);

        // Mittellinie: dieselbe Breitenregel wie im 2D-Zweig (schmal,
        // wenn eine Kante naeher als 4 px liegt — CW), hell statt tuerkis.
        const qreal vfoLineW =
            (std::abs(vfoX - xLo) <= 4 || std::abs(vfoX - xHi) <= 4) ? 1.0 : 2.0;
        QColor vfoLine(kPassbandEdgeLight);
        vfoLine.setAlpha(230);
        p.setPen(QPen(vfoLine, vfoLineW));
        p.drawLine(vfoX, specRect.top(), vfoX, specBottomClipped);

        // Das Dreieck oben, wie im 2D-Zweig (ohne Flaggen-Zweig: im
        // 3D-Modus gibt es keine Flagge, an die es sich haengen koennte).
        if (vfoX >= specRect.left() && vfoX <= specRect.right()) {
            static constexpr int kTriHalf = 6;
            static constexpr int kTriH = 10;
            static constexpr int kSliceR = 0x00, kSliceG = 0xd4, kSliceB = 0xff;
            const int triTop = specRect.top();
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(kSliceR, kSliceG, kSliceB));
            QPolygon tri;
            tri << QPoint(vfoX - kTriHalf, triTop)
                << QPoint(vfoX + kTriHalf, triTop)
                << QPoint(vfoX, triTop + kTriH);
            p.drawPolygon(tri);
        }
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
    // Antennenfrequenz, nicht ZF (2026-11-08) -- wie die Achse darunter.
    QString label = formatCursorFreq(rfFuerAnzeige(hz));

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
// Longpath translation: track state flag, markOverlayDirty() to trigger
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
// ── Befund 6: Entwertung der zwischengespeicherten Zweiton-Schicht ──────
//
// Im GPU-Weg liegen Marken und Messfeld in m_overlayDynamic, einer
// Textur, die nur bei gesetztem m_overlayDynamicDirty neu gebaut wird.
// update() allein macht daraus kein neues Bild. Jeder Schalter, der das
// Zweiton-Tor bewegt, muss die Schicht deshalb von Hand entwerten --
// sonst bleibt beim Torschluss das letzte Messfeld stehen, und beim
// Toroeffnen erscheint es erst, wenn irgendetwas anderes die Schicht
// erneuert.
//
// Ausserhalb des GPU-Baus ein reines update(), wie markOverlayDirty().
void SpectrumWidget::entwerteImdSchicht()
{
#ifdef LONGPATH_GPU_SPECTRUM
    m_overlayDynamicDirty = true;
    m_overlayDynamicDirtyMs = 0;  // nicht an der 30-Hz-Bremse haengen bleiben
#endif
    update();
}

void SpectrumWidget::setTestingIMD(bool on)
{
    if (m_testingIMD == on) {
        return;
    }
    m_testingIMD = on;
    if (!on && m_imdOverlay) {
        m_imdOverlay->reset();  // EMA reset, see setMoxOverlay note above.
    }
    // Befund 6 (2026-10-22): update() allein genuegt im GPU-Weg nicht --
    // die Marken liegen in einer zwischengespeicherten Textur. Ohne diese
    // Entwertung blieb beim Torschluss das alte Messfeld stehen.
    entwerteImdSchicht();
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
    entwerteImdSchicht();  // Befund 6, siehe setTestingIMD.
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
    // Longpath defaults this to true since the panadapter stays live
    // during MOX (see SpectrumWidget header note).  Setter is provided for
    // a potential future Setup checkbox.
    if (m_displayDuplex == on) {
        return;
    }
    m_displayDuplex = on;
    update();
}

// From Thetis display.cs:4840 [v2.10.3.13]:
// Upstream tags preserved: //MW0LGE (from cited upstream lines) [v2.10.3.15]
//   if (!local_mox) fOffset += rx1_preamp_offset;
// The RX cal offset is only added in RX mode; during TX, the TX path uses
// its own calibration. Longpath models this by storing the TX ATT offset
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
// Source: Longpath-original.  IQ-space conversion follows
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
// System.Drawing.Color.Gray is #828288 while Qt::gray is #9aa0a8, so the
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
    // Steht der Editor offen, bekommt er die Zahlen, die WIRKLICH
    // gelten — nicht die getippten. WDSP setzt zu schmale Breiten
    // selbst herauf (nbp.c:122-125); ohne diese Zeilen zeigte das
    // Fenster weiter den Wunsch und der Balken das Ergebnis.
    if (m_notchEditor && m_notchEditor->isVisible()) {
        const int open = m_notchEditor->notchId();
        bool stillThere = false;
        for (const NotchMarker& m : markers) {
            if (m.id != open) { continue; }
            m_notchEditor->applyFromModel(m.id, m.freqMhz * 1.0e6,
                                          m.widthHz, m.active);
            stillThere = true;
            break;
        }
        // Weg ist weg: ein Editor auf einen geloeschten Notch waere ein
        // Fenster, dessen Knoepfe ins Leere gehen.
        if (!stillThere) { m_notchEditor->close(); }
    }

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
        // Longpath coordinate mapping: hzToX(double hz, QRect) takes Hz.
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
// Algorithm preserved verbatim. Longpath divergences are local to the
// coordinate helpers (Longpath uses hzToX(double hz, QRect) over Hz-units
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

// ── S-Verlauf: die Speisung ──────────────────────────────────────────
//
// Aufbau nach AetherSDR MainWindow::onSpectrumReadyForSHistory
// (:9757-9950 [@0cd4559]) und expireSHistoryMarkers (:9732-9755), aber
// an anderer Stelle: dort haengt es an einem eigenen FFT-Abgriff, hier
// an processNoiseFloor. Begruendung im Header.
void SpectrumWidget::updateSignalHistory(float noiseFloorDbm)
{
    // Ausgeschaltet kostet das Merkmal einen Vergleich. Wer es nie
    // einschaltet, zahlt nichts — deshalb steht diese Pruefung ganz
    // vorne und nicht hinter der Drosselung.
    if (!m_showSignalHistory && !m_showSignalHistoryQrm) {
        if (!m_signalHistoryMarkers.isEmpty()) {
            m_signalHistory.clear();
            setSignalHistoryMarkers({});
        }
        return;
    }

    const QVector<float>& src = measurementPixels();
    if (src.isEmpty() || m_bandwidthHz <= 0.0) { return; }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    // Drosselung auf 10 Hz. Das Vorbild laeuft je Bild; unser Erkenner
    // sieht dieselbe Lage bei 10 Hz genauso gut, und die Belegungsrechnung
    // unten bekommt die tatsaechliche Rate gesagt, nicht die Bildrate.
    constexpr qint64 kMinIntervalMs = 100;
    if (m_shLastRunMs != 0 && (nowMs - m_shLastRunMs) < kMinIntervalMs) {
        return;
    }
    if (m_shLastRunMs != 0) {
        const float observed = 1000.0f / static_cast<float>(nowMs - m_shLastRunMs);
        // EWMA wie beim Vorbild (fpsEwma, α = 0.05).
        constexpr float kAlpha = 0.05f;
        m_shRunRateEwma = (1.0f - kAlpha) * m_shRunRateEwma + kAlpha * observed;
    }
    m_shLastRunMs = nowMs;

    // Fonie-Bereiche aus dem aktiven Bandplan. Sprachbreite Signale
    // ausserhalb zaehlen nicht; Schmalbandstoerungen schon — die halten
    // sich nicht an Bandplaene.
    QVector<QPair<double, double>> voiceRanges;
    if (m_bandPlanMgr) {
        for (const auto& seg : m_bandPlanMgr->segments()) {
            if (Longpath::isVoiceSegmentLabel(seg.label)) {
                voiceRanges.append({seg.lowMhz, seg.highMhz});
            }
        }
    }

    const double centerMhz    = m_centerHz / 1.0e6;
    const double bandwidthMhz = m_bandwidthHz / 1.0e6;

    m_signalHistory.ingest(
        Longpath::detectVoiceSignals(src, centerMhz, bandwidthMhz,
                                      voiceRanges, noiseFloorDbm),
        nowMs);

    // Altern und einordnen im Sekundentakt — auch ohne neue Treffer,
    // sonst bleiben Marken stehen, deren Zeitstempel weggealtert sind
    // (AetherSDR expireSHistoryMarkers, derselbe Takt).
    if (nowMs - m_shLastRebuildMs < 1000) { return; }
    m_shLastRebuildMs = nowMs;

    m_signalHistory.expire(nowMs);
    m_signalHistory.rebuild(nowMs, m_shRunRateEwma);

    // Bernstein fuer Sprache, Rot fuer Stoerung — die Unterscheidung wie
    // beim Vorbild (AetherSDR: #FFC800 und #FF0000), die WERTE aber aus
    // unserer Palette. Ein Panadapter mit zwei Rot- und zwei
    // Bernsteintoenen sieht nach zwei Programmen aus, und wer das Theme
    // umstellt, erwartet, dass alles mitgeht.
    //
    // roleColor() liest die Theme-Datei mit der Konstante als Rueckfall —
    // dieselben Rollen wie die Squelch-Linie („warn") und die
    // Stehwellen-Warnung.
    const QString voiceHex = roleColor("warn", Style::kAmberWarn).name();
    const QString qrmHex   = roleColor("danger", Style::kRedBorder).name();

    // Ueber der S9-Grenze (2 m) ist S9 -93 dBm: die Lesung rueckt um
    // 20 dB wie an den Instrumenten (SMeter::kVersatzUeberGrenzeDb).
    const float sVersatz =
        SMeter::ueberGrenze(rfFuerAnzeige(m_vfoHz) / 1.0e6)
            ? SMeter::kVersatzUeberGrenzeDb : 0.0f;
    QVector<SpotMarker> markers;
    for (const auto& e : m_signalHistory.visibleEntries()) {
        SpotMarker m;
        m.callsign    = Longpath::sLabel(e.peakDbm + sVersatz);
        m.freqMhz     = e.freqMhz;
        m.mode        = e.mode;
        m.source      = e.suspectQrm ? QStringLiteral("QRM")
                                     : QStringLiteral("SHistory");
        m.color       = e.suspectQrm ? qrmHex : voiceHex;
        m.timestampMs = e.lastSeenMs;
        markers.append(m);
    }
    setSignalHistoryMarkers(markers);
}

// ── S-Verlauf: der zweite Markenkanal ────────────────────────────────
//
// Dieselbe Sichtgleichheits-Bremse wie bei den DX-Spots: der Erkenner
// liefert im Sekundentakt, und in ruhiger Lage ist die Liste dabei
// unveraendert. Ohne diese Pruefung waere das ein Neuzeichnen je
// Sekunde fuer ein byteweise gleiches Bild.
void SpectrumWidget::setSignalHistoryMarkers(const QVector<SpotMarker>& markers)
{
    const bool visualChange =
        !spotMarkersVisuallyEqual(m_signalHistoryMarkers, markers);
    m_signalHistoryMarkers = markers;
    if (!visualChange) {
        return;
    }
    update();
}

void SpectrumWidget::setShowSignalHistory(bool on)
{
    if (m_showSignalHistory == on) { return; }
    m_showSignalHistory = on;
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setShowSignalHistoryQrm(bool on)
{
    if (m_showSignalHistoryQrm == on) { return; }
    m_showSignalHistoryQrm = on;
    scheduleSettingsSave();
    update();
}

void SpectrumWidget::setSignalHistoryQrmGateSeconds(int sec)
{
    if (m_signalHistory.qrmGateSeconds() == sec) { return; }
    m_signalHistory.setQrmGateSeconds(sec);   // begrenzt selbst auf 3..30
    scheduleSettingsSave();
}

int SpectrumWidget::signalHistoryQrmGateSeconds() const
{
    return m_signalHistory.qrmGateSeconds();
}

void SpectrumWidget::setSignalHistoryLifetimeSeconds(int sec)
{
    if (m_signalHistory.lifetimeSeconds() == sec) { return; }
    m_signalHistory.setLifetimeSeconds(sec);  // begrenzt selbst auf 15..300
    scheduleSettingsSave();
}

int SpectrumWidget::signalHistoryLifetimeSeconds() const
{
    return m_signalHistory.lifetimeSeconds();
}

// Phase 3J-2 + 3R M2: refresh every Spot Display knob from AppSettings.
// The producer side lives on SpotHubDialog F4 (buildDisplayTab,
// SpotHubDialog.cpp:1619-2010); every knob change writes to AppSettings
// and emits settingsChanged. MainWindow::openSpotHub wires that signal
// to this method so the live overlay tracks the dialog. Defaults
// duplicate the F4 read-side at SpotHubDialog.cpp:1714-1730.
//
// Longpath-original. AetherSDR splits these reads across a freestanding
// SpotSettingsDialog and a refreshSpots() lambda on MainWindow; the
// Longpath shape collapses both into a single helper on the consumer
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
                QStringLiteral("#c2924f")).toString()));
    setSpotBgColor(QColor(
        s.value(QStringLiteral("SpotsOverrideBgColor"),
                QStringLiteral("#0a0a14")).toString()));
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
        QStringLiteral("PSK"),           QStringLiteral("Roundtable"),
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
// Merge DX spots, Signal History markers, and QRM History markers.
// Each category is gated by its own flag.  S-History markers within 3 kHz of
// an active DX spot are suppressed — the spot carries richer callsign/DXCC info.
//
// From AetherSDR SpectrumWidget.cpp:15719-15741 [@0cd4559]. Der gemeinsame
// Weg ist der Punkt: Kollisionsstapelung, Buendelabzeichen und Klickfelder
// gelten dann fuer beide Sorten zusammen. Zwei getrennte Zeichnungen
// wuerden uebereinander malen.
//
// Als eigene Funktion und nicht inmitten der Zeichnung, damit die
// 3-kHz-Regel pruefbar ist: sie ist die Regel, die am ehesten unbemerkt
// kaputtgeht — ein falsches Vorzeichen oder eine vertauschte Einheit
// faellt im Bild erst auf, wenn zwei Etiketten uebereinander stehen.
QVector<SpectrumWidget::SpotMarker> SpectrumWidget::mergedMarkers() const
{
    QVector<SpotMarker> allMarkers;
    if (m_showSpots) { allMarkers = m_spotMarkers; }
    if (m_showSignalHistory || m_showSignalHistoryQrm) {
        constexpr double kSpotOverrideMhz = 0.003;
        for (const auto& sh : m_signalHistoryMarkers) {
            const bool isQrm = (sh.source == QStringLiteral("QRM"));
            if (isQrm  && !m_showSignalHistoryQrm) { continue; }
            if (!isQrm && !m_showSignalHistory)    { continue; }
            bool masked = false;
            for (const auto& sp : m_spotMarkers) {
                if (std::abs(sh.freqMhz - sp.freqMhz) < kSpotOverrideMhz) {
                    masked = true;
                    break;
                }
            }
            if (!masked) { allMarkers.append(sh); }
        }
    }
    return allMarkers;
}

void SpectrumWidget::drawSpotMarkers(QPainter& p, const QRect& specRect)
{
    const QVector<SpotMarker> allMarkers = mergedMarkers();

    if (allMarkers.isEmpty()) {
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

    for (const auto& spot : allMarkers) {
        // 2026-05-12 bench fix (Gap #7).  Per-source panadapter
        // visibility mask.  Missing key in m_spotSourceVisible defaults
        // to visible, so untouched sources keep the prior behaviour;
        // an explicit `false` value hides this marker entirely (no
        // label, no tick line, no hit-rect).  SpotHubDialog Display
        // tab drives this.
        if (!m_spotSourceVisible.value(spot.source, true)) {
            continue;
        }

        // Longpath coordinate mapping: hzToX(double hz, QRect) takes Hz.
        // AetherSDR upstream uses mhzToX(spot.freqMhz). Multiply by 1e6.
        //
        // ── Durch den Transverter (2026-10-31) ──────────────────────
        //
        // Ein Spot traegt die Frequenz auf der ANTENNE (144,300), die
        // Achse hier zeigt die ZF (28,300). Ohne `zfFuerAchse` landet
        // jeder 2-m-Spot weit auszerhalb des Bildes -- und ein
        // 28-MHz-Spot aus dem Cluster saesze faelschlich mitten im
        // Band. Ohne Transverter ist die Rechnung die Gleichheit.
        const int x = hzToX(zfFuerAchse(spot.freqMhz * 1.0e6), specRect);
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

        // ── Rufzeichen UND wie stark er ankommt (2026-10-31) ─────
        //
        // Betreiber: „ich will sehen, wie gut sein signal bei mir
        // ankommt. so kann ich entscheiden, ob ich es probiere oder ob
        // es schierig wird."
        //
        // Nur wo eine Zahl GEMESSEN wurde: die eigenen Dekoder
        // (FT8/FT4/RTTY) liefern sie, ein Cluster-Spot nicht. Dort
        // bleibt es beim blossen Rufzeichen -- eine erfundene Null
        // waere schlimmer als keine Angabe.
        const QString label = spot.snrDb == SpotMarker::kKeineAngabe
            ? spot.callsign
            : QStringLiteral("%1 %2").arg(spot.callsign,
                                          sSnrText(spot.snrDb));
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
        // markerIndex zeigt in m_spotMarkers und nur dorthin: an ihm haengen
        // spotTriggered, der Kerben-Hinweis beim Ueberfahren und „Remove
        // Spot" — alles Dinge, die es nur fuer echte DX-Spots gibt.
        //
        // Seit die Schleife ueber die ZUSAMMENGEFUEHRTE Liste laeuft
        // (S-Verlauf, 2026-08-19), darf der Index nicht mehr aus der
        // Zeigerdifferenz gegen m_spotMarkers entstehen: die Marke liegt in
        // einer anderen Liste, die Rechnung ist undefiniert, und bei reinen
        // Verlaufsmarken greift &m_spotMarkers[0] auf eine leere Liste zu.
        //
        // allMarkers beginnt mit den DX-Spots (falls gezeigt), danach folgen
        // die Verlaufsmarken. Die vorderen Plaetze bilden also 1:1 auf
        // m_spotMarkers ab, die hinteren bekommen -1 — „keine DX-Marke",
        // womit die Waechter an den drei Stellen oben von selbst greifen.
        const int listPos  = static_cast<int>(&spot - &allMarkers[0]);
        const int spotCount = m_showSpots ? static_cast<int>(m_spotMarkers.size()) : 0;
        const int mIdx = (listPos < spotCount) ? listPos : -1;
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
                avgX += hzToX(zfFuerAchse(s.freqMhz * 1.0e6), specRect);
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

            p.setPen(QColor(Style::kAmberText));  // amber text
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
    // Am Fenster, nicht am Panadapter (2026-09-30): der Panadapter ist
    // nativ, und ein Menue daran bekam von Qt keinen Fensterbezug („...
    // must be a top level window."). Aufgeraeumt wird weiter mit ihm.
    auto* menu = new QMenu(window());
    connect(this, &QObject::destroyed, menu, &QObject::deleteLater);
    // ── Ein Menü ist Bedienung, kein Messwert ────────────────────────
    //
    // OE5SOS, 2026-08-15: alle fünf Werte, auch die zwei Textfarben.
    // Der Aufklapper trug die alte Palette vollständig: #0f0f1a Fläche,
    // #205070 Rahmen — der Wert, den StyleConstants.h als „der umrandete
    // jeden Knopf im Programm" führt —, #204060 Auswahlfüllung und
    // #00b4d8 Auswahltext. Das Türkis ist abgeschafft; es machte zwei
    // Jobs gleichzeitig, „ausgewählt" und „gemessen", und genau diese
    // Trennung ist der Kern des Hausstils: Blau ist anfassbar, Warm ist
    // gemessen. Ein Menüeintrag ist anfassbar.
    menu->setStyleSheet(QStringLiteral(
        "QMenu {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  padding: 4px;"
        "}"
        "QMenu::item {"
        "  color: %3;"
        "  padding: 4px 12px;"
        "  font-size: 13px;"
        "}"
        "QMenu::item:selected {"
        "  background: %4;"
        "  color: %5;"
        "}")
        .arg(Style::role("app-bg",   Style::kAppBg),
             Style::role("border",   Style::kBorder),
             Style::role("text",     Style::kTextPrimary),
             Style::role("sel-bg",   Style::kBlueBg),
             Style::role("sel-text", Style::kBlueText)));

    for (const auto& spot : cluster.spots) {
        QString text = QString("%1  %2 kHz")
            .arg(spot.callsign, -10)
            .arg(spot.freqMhz * 1000.0, 0, 'f', 1);
        if (!spot.mode.isEmpty()) {
            text += "  " + spot.mode;
        }
        auto* action = menu->addAction(text);
        connect(action, &QAction::triggered, this, [this, spot] {
            // Longpath signal contract: frequencyClicked(double hz).
            // AetherSDR emits MHz; multiply by 1e6 to match the Hz signature.
            const double freqHz = spot.freqMhz * 1.0e6;
            emit frequencyClicked(freqHz);
            if (spot.source == "Memory") {
                emit spotTriggered(spot.index);
            }
        });
    }

    // Finding #3 replay guard (see mousePressEvent): stamp the close
    // so a re-delivered press can't fall through to the overlay menu.
    connect(menu, &QMenu::aboutToHide, this, [this]() {
        m_contextMenuClosedMs = QDateTime::currentMSecsSinceEpoch();
    });
    menu->popup(globalPos);
    // QMenu self-deletes on close with WA_DeleteOnClose
    menu->setAttribute(Qt::WA_DeleteOnClose);
}

// Plan 4 D9 (Cluster E).  Panadapter TX filter band fill + border lines +
// inline label.  Always drawn when m_txFilterVisible is set (called from the
// main paint sequence regardless of MOX state — Thetis shows the TX filter
// band on the spectrum even in RX mode to hint where TX will land).
//
// Source: Longpath-original rendering.  IQ-space mapping per
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
// ---------------------------------------------------------------------------
// analyseImd()
//
// ── Die Messung gehoert nicht in den Malvorgang (2026-10-22) ─────────────
//
// Bis heute stand dieser ganze Block in drawImdOverlay(), lief also mit
// der Bildrate und nur dann, wenn Qt gerade neu zeichnete. Damit hing
// sowohl der EMA-Takt als auch die Protokollzeile davon ab, WANN gemalt
// wurde -- bei verdecktem Fenster gar nicht, bei GPU-Weg anders als bei
// CPU-Weg. Das erklaert die Messreihen vom 2026-10-21, in denen Werte
// mal kamen und mal nicht.
//
// NereusSDR hat denselben Befund in seinem Entwurfsblatt vom 2026-10-12
// unter "paint-driven analysis" notiert. Behoben wie dort beschrieben:
// die Analyse laeuft jetzt genau einmal je Spektrumsbild, am Ende von
// updateSpectrumLinear(), wo m_renderedPixels endgueltig ist. Der
// Malvorgang liest nur noch das Ergebnis.
//
// Die Produkte liegen in m_imdProductDbm/m_imdProductBin in der Reihenfolge
// f0L, f0U, IMD3L, IMD3U, IMD5L, IMD5U (siehe kImdProduct* in der
// Kopfdatei) -- als nackte Zahlen, damit ImdOverlay.h nicht in die
// Kopfdatei muss (Begruendung dort bei m_imdOverlay).
// ---------------------------------------------------------------------------
void SpectrumWidget::analyseImd()
{
    // ── Warum NICHTS gemessen wird, muss auch im Protokoll stehen ────────
    //
    // Gebaut 2026-10-21, nachdem ein Messlauf 22 Sekunden lang bei offenem
    // Tor (mox/testIMD/showImd/duplex alle wahr) keine einzige Zeile
    // lieferte und ich die Ursache nur raten konnte. Ein Ausstieg, der
    // schweigt, ist von "laeuft nicht" nicht zu unterscheiden -- und man
    // sucht die Ursache dann an der falschen Stelle.
    //
    // Alle Meldungen teilen sich einen Takt von einer Sekunde mit der
    // Erfolgszeile weiter unten: diese Stelle laeuft mit der Bildrate.
    static qint64 lastImdLogMs = 0;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool logNow = (nowMs - lastImdLogMs >= 1000);
    auto melden = [&](const QString& text) {
        if (logNow) {
            lastImdLogMs = nowMs;
            qCInfo(lcSpectrum).noquote() << "IMD Zweiton: no reading --" << text;
        }
    };

    if (!m_imdOverlay || m_renderedPixels.isEmpty()) {
        melden(QStringLiteral("no spectrum rendered (window covered?)"));
        m_imdAnalysisValid = false;
        return;
    }

    // ── NICHT noch einmal zuschneiden (2026-10-21) ───────────────────────
    //
    // Hier stand `visibleBinRange(m_renderedPixels.size())` und ein
    // Ausschnitt daraus. Das war ein ZWEITER Schnitt: m_renderedPixels
    // ist laengst auf das sichtbare Fenster reduziert
    // (m_centerHz +/- m_bandwidthHz/2, in Anzeigebreite) -- der
    // Kommentar an der Spitzentext-Uhr weiter oben sagt es woertlich.
    //
    // Was der zweite Schnitt kostete, war an diesem Abend am Geraet zu
    // sehen: die Grundtoene lagen mittig und ueberlebten, die
    // IMD5-Produkte bei +/- 2d fielen heraus -- und weil Thetis'
    // Zuordnung ALLE sechs Produkte verlangt, lieferte die Messung
    // dann gar nichts. Stundenlang.
    //
    // Gefunden hat es nicht ich, sondern NereusSDR: im Upstream steht
    // derselbe Befund unter demselben Namen, mit Zahlen --
    // docs/architecture/2026-10-12-puresignal-imd-diagnosis.md,
    // „double cropping": bei 192 kHz, 48 kHz sichtbarer Spanne und 800
    // Proben blieben vom zweiten Schnitt nur die Indizes 300-500.
    //
    // Jetzt wird auf dem GANZEN sichtbaren Feld gesucht, und die
    // Markierungen werden aus denselben Koordinaten abgebildet.
    const int count = m_renderedPixels.size();
    if (count < 2) {
        melden(QStringLiteral("only %1 visible bins").arg(count));
        m_imdAnalysisValid = false;
        return;
    }

    std::vector<float> visible(m_renderedPixels.begin(),
                               m_renderedPixels.end());

    // From Thetis display.cs:5217 [v2.10.3.13]: trigger_delta = 10 dB.
    constexpr float kTriggerDelta = 10.0f;
    const auto peaks = ImdOverlay::detectPeaks(visible, kTriggerDelta);

    Maximum f0L, f0U, imd3L, imd3U, imd5L, imd5U;
    if (!ImdOverlay::labelImdProducts(peaks,
                                      f0L, f0U, imd3L, imd3U, imd5L, imd5U)) {
        // Die Zahl der Spitzen trennt die drei Faelle, die hier
        // zusammenlaufen: gar kein Signal (0-1), die zwei Grundtoene zu
        // eng beieinander (>=2, aber unter 10 Bildpunkten Abstand --
        // Thetis display.cs:5520), oder die Seitenprodukte im Rauschen.
        QString spitzen;
        for (int i = 0; i < static_cast<int>(peaks.size()) && i < 6; ++i) {
            spitzen += QStringLiteral(" x=%1/%2dBm")
                           .arg(peaks[i].x).arg(peaks[i].dBm, 0, 'f', 1);
        }
        // ── Ablesen statt Suchen (Longpath-eigen, 2026-10-21) ────────────
        //
        // Thetis' Zuordnung verlangt ALLE sechs Produkte als erkannte
        // Spitzen. Am 2026-10-21 am echten Sender gemessen: sobald
        // PureSignal wirkt, verschwinden die IMD3-Produkte aus der
        // Spitzenliste -- die Messung versagt also genau dann, wenn die
        // Korrektur ihre Arbeit tut. Dieselbe Messreihe ohne Korrektur
        // lieferte dagegen anstandslos 19,5 dB.
        //
        // Darum hier der zweite Weg: die Grundtoene sind die zwei
        // staerksten Spitzen, ihr Abstand d legt fest, WO die Produkte
        // liegen muessen (f0 -/+ d fuer IMD3, -/+ 2d fuer IMD5). Dort wird
        // der Pegel schlicht ABGELESEN. Ist da nur Rauschen, ist die Zahl
        // eine untere Schranke -- und als solche gekennzeichnet, damit
        // niemand sie fuer eine Messung des Senders haelt.
        //
        // Bewusst ohne EMA: dies ist der Notweg fuer einen einzelnen
        // Blick, nicht der geglaettete Messwert der Hauptanzeige.
        if (peaks.size() >= 2) {
            std::vector<Maximum> stark(peaks.begin(), peaks.end());
            std::sort(stark.begin(), stark.end(),
                      [](const Maximum& a, const Maximum& b) {
                          return a.dBm > b.dBm;
                      });
            const int xa = std::min(stark[0].x, stark[1].x);
            const int xb = std::max(stark[0].x, stark[1].x);
            const int d  = xb - xa;

            // Dieselbe Mindestbreite wie Thetis (display.cs:5520): unter
            // zehn Bildpunkten sind es keine zwei Toene, sondern einer.
            if (d > 10) {
                // Ein schmales Fenster um die erwartete Stelle faengt die
                // Rundung der Bin-Rechnung auf, ohne den Nachbarton
                // einzusammeln.
                auto pegelBei = [&](int bin) -> float {
                    constexpr int kFenster = 2;
                    float best = -200.0f;
                    for (int k = bin - kFenster; k <= bin + kFenster; ++k) {
                        if (k >= 0 && k < count) {
                            best = std::max(best, visible[static_cast<size_t>(k)]);
                        }
                    }
                    return best;
                };
                const float f0  = std::min(stark[0].dBm, stark[1].dBm);
                const float i3  = std::max(pegelBei(xa - d), pegelBei(xb + d));
                const float i5  = std::max(pegelBei(xa - 2 * d), pegelBei(xb + 2 * d));
                melden(QStringLiteral(
                           "read off (lower bound): f0=%1dBm "
                           "IMD3=%2dB IMD5=%3dB  [%4 peak(s), d=%5 bins]")
                           .arg(f0, 0, 'f', 1)
                           .arg(f0 - i3, 0, 'f', 1)
                           .arg(f0 - i5, 0, 'f', 1)
                           .arg(peaks.size())
                           .arg(d));
                m_imdAnalysisValid = false;
                return;
            }
        }

        melden(QStringLiteral("products not labelled; %1 peak(s) in %2 bins:%3")
                   .arg(peaks.size()).arg(count).arg(spitzen));
        // Failure modes (Thetis display.cs:5666-5673 [v2.10.3.13] fallback
        // text "Peaks not found !") are intentionally silent here — the
        // user already sees a stale readout box from the previous frame
        // until peaks return.  A future polish PR can render a hint.
        m_imdAnalysisValid = false;
        return;
    }
    m_imdOverlay->updateReadout(f0L, f0U, imd3L, imd3U, imd5L, imd5U);

    // ── Die Messung gehoert auch ins Protokoll (2026-10-21) ───────────────
    //
    // Bis hierher wurden IMD3/IMD5 nur ins Bild gemalt. Fuer die eine
    // Frage, auf die es bei PureSignal ankommt -- sinken die
    // Intermodulationsprodukte, wenn die Korrektur laeuft? -- braucht man
    // sie als ZAHLEN, zweimal: einmal mit PS aus, einmal mit PS ein.
    // Vom Bild abgelesen waere das Augenmass; hier ist es eine Reihe, die
    // man nebeneinanderlegen kann.
    //
    // Gedrosselt auf eine Zeile je Sekunde: diese Stelle laeuft im
    // Malvorgang, also mit der Bildrate. Ungedrosselt verdeckt sie jede
    // andere Meldung -- derselbe Fehler, der 2026-08-16 schon einmal am
    // PureSignal-Protokoll behoben werden musste.
    //
    // Die Werte sind die EMA-geglaetteten aus Thetis'
    // display.cs:5589-5641 [v2.10.3.13], unveraendert uebernommen: dbc /
    // dbcMin in dBm, die beiden worst-Abstaende in dB, OIP3/OIP5 in dBm.
    {
        if (logNow) {
            lastImdLogMs = nowMs;
            const ImdReadout& r = m_imdOverlay->readout();
            qCInfo(lcSpectrum).nospace()
                << "IMD Zweiton: dbc=" << r.dbc
                << " dbcMin=" << r.dbcMin
                << " worstIMD3=" << r.worstImd3DBc
                << " worstIMD5=" << r.worstImd5DBc
                << " OIP3=" << r.oip3
                << " OIP5=" << r.oip5;
        }
    }


    // Ergebnis fuer den Malvorgang ablegen.
    const Maximum* prod[6] = { &f0L, &f0U, &imd3L, &imd3U, &imd5L, &imd5U };
    for (int i = 0; i < 6; ++i) {
        m_imdProductDbm[i] = prod[i]->dBm;
        m_imdProductBin[i] = prod[i]->x;
    }
    m_imdAnalysisBins  = count;
    m_imdAnalysisValid = true;
}

// ---------------------------------------------------------------------------
// drawImdOverlay()
//
// Malt nur noch. Gemessen wird in analyseImd(), einmal je Spektrumsbild.
// ---------------------------------------------------------------------------
void SpectrumWidget::drawImdOverlay(QPainter& p, const QRect& specRect)
{
    if (!m_imdOverlay || !m_imdAnalysisValid || m_imdAnalysisBins < 2) {
        return;
    }
    const int count = m_imdAnalysisBins;
    auto produkt = [this](int i) {
        Maximum m;
        m.dBm = m_imdProductDbm[i];
        m.x   = m_imdProductBin[i];
        return m;
    };
    const Maximum f0L   = produkt(0);
    const Maximum f0U   = produkt(1);
    const Maximum imd3L = produkt(2);
    const Maximum imd3U = produkt(3);
    const Maximum imd5L = produkt(4);
    const Maximum imd5U = produkt(5);

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
    // Aus der Palette, nicht als Zahl: #c2924f war der Wert von
    // kAmberText bis zur Aufhellung am 2026-08-20. Eine abgeschriebene
    // Farbe erfaehrt von einer Aenderung an der Vorlage nichts.
    drawMarker(f0L,   QColor(QString::fromLatin1(Style::kAmberText)), 6.0f);
    drawMarker(f0U,   QColor(QString::fromLatin1(Style::kAmberText)), 6.0f);
    drawMarker(imd3L, QColor("#c25a5c"), 5.0f);
    drawMarker(imd3U, QColor("#c25a5c"), 5.0f);
    drawMarker(imd5L, QColor("#c25a5c"), 4.0f);
    drawMarker(imd5U, QColor("#c25a5c"), 4.0f);

    // From Thetis display.cs:5520 [v2.10.3.13]:
    //   RoundedRectangle rr = new RoundedRectangle();
    //   rr.Rect = new RectangleF(_two_tone_readings_X_offset, 50, 260, 180);
    //   rr.RadiusX = 14f; rr.RadiusY = 14f;
    // _two_tone_readings_X_offset starts at 50 (display.cs:4948 [v2.10.3.13]).
    // Thetis adjusts the X offset to dodge IMD5 markers (display.cs:5584-5585);
    // Longpath keeps the simple fixed-50 placement for now.
    constexpr int kBoxX = 50;
    constexpr int kBoxY = 50;
    constexpr int kBoxW = 260;
    constexpr int kBoxH = 180;
    constexpr int kBoxR = 14;
    const QRectF box(specRect.left() + kBoxX, specRect.top() + kBoxY,
                     kBoxW, kBoxH);
    p.setBrush(QColor(20, 28, 40, 240));
    p.setPen(QPen(QColor("#4a7ba8"), 1.5));
    p.drawRoundedRect(box, kBoxR, kBoxR);

    // From Thetis display.cs:5683-5687 [v2.10.3.13] DrawText calls.
    // 4 columns: "dBm dBc frequency" header (Longpath drops the freq
    // column for now — that requires hz_per_pixel + DDC center plumbing
    // which the current mockup doesn't model), then label/val1/val2.
    const auto t = m_imdOverlay->formatReadout();
    p.setPen(QColor("#c8d8e8"));
    // setPixelSize statt setPointSize: siehe die Begruendung bei den
    // dBm-Beschriftungen weiter oben.
    QFont mono(QStringLiteral("Menlo"));
    mono.setPixelSize(9);
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
// Source: Longpath-original rendering.  IQ-space mapping per
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

    // setPixelSize statt setPointSize: siehe die Begruendung bei den
    // dBm-Beschriftungen weiter oben. Der Zahlenwert 48 ist ohnehin
    // Longpaths eigene Wahl, nicht Thetis' fontDX2d_font14 (das waere
    // 14pt) -- hier wird nur die Masseinheit auf Pixel umgestellt, der
    // Wert bleibt unveraendert.
    QFont f = p.font();
    f.setPixelSize(Style::kFontAlarm);
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
// Longpath 3M-1a draws a border tint only.  Full grid re-colouring
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
            // Bis 2026-08-19 ging der Doppelklick hier als DRUCK weiter, ein
            // echter mouseDoubleClickEvent haette auf dem GPU-Weg also nie
            // gefeuert. Jetzt bekommt er ihn — und der faellt von sich aus
            // auf mousePressEvent zurueck, wenn er nichts zu tun findet.
            // Damit bleibt alles wie vorher, ausser auf einem Spot-Etikett.
            mouseDoubleClickEvent(static_cast<QMouseEvent*>(ev));
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
#ifdef LONGPATH_GPU_SPECTRUM
    const int contentH = widgetH - chromeH;
    return static_cast<int>(contentH * spectrumFrac);
#else
    Q_UNUSED(chromeH);
    return static_cast<int>(widgetH * spectrumFrac);
#endif
}

// ── EINE Rechnung fuer die Spektrumsflaeche ──────────────────────────
//
// Gezeichnet wird in `w - effectiveStripW()` — ohne die dBm-Leiste am
// rechten Rand. Meine Trefferpruefung nahm die volle Breite, und damit
// lag sie neben dem Bild: bei der rechten Einblendung um die
// Streifenbreite daneben, also genau dort, wo man NICHT klickt.
//
// Der Betreiber, 2026-08-21: „verschieben funktioniert nicht." Mein
// Test bestand trotzdem — er benutzte dieselbe falsche Rechnung wie
// der Code, den er prueft. Zwei Rechnungen fuer dieselbe Flaeche sind
// zwei Orte, an denen sie auseinanderlaufen; hier ist es die dritte
// Wiederholung desselben Musters an einem Tag.
// From AetherSDR SpectrumWidget.cpp:2264-2306 + 1857-1871 + 7138-7156
// [@0cd4559] — verbatim wo moeglich; LONGPATH_GPU_SPECTRUM statt
// AETHER_GPU_SPECTRUM, und der Frequency-Preview-Zweig entfaellt, weil
// Longpath dieses Feature (noch) nicht traegt.
void SpectrumWidget::prepareForTopLevelChange()
{
#ifdef LONGPATH_GPU_SPECTRUM
    // QRhiWidget registers a cleanup callback with the current top-level
    // backing-store QRhi. Direct splitter/floating-window reparenting can miss
    // Qt's internal notification, leaving the old QRhi with a stale callback;
    // when that QRhi is later torn down, runCleanup() fires against a stale
    // QRhiWidgetPrivate and crashes during deferred event delivery (#2495).
    //   [original comment from AetherSDR SpectrumWidget.cpp:2266-2271]
    QEvent event(QEvent::WindowAboutToChangeInternal);
    QCoreApplication::sendEvent(this, &event);
#endif
}

void SpectrumWidget::resetGpuResources()
{
#ifdef LONGPATH_GPU_SPECTRUM
    // On macOS/Windows, the GPU surface doesn't survive reparenting — tear
    // down old pipelines so initialize() rebuilds them for the new window.
    // On Linux (OpenGL), a simple update() is sufficient (#1240).
    //   [original comment from AetherSDR SpectrumWidget.cpp:7146-7148]
#ifndef Q_OS_LINUX
    releaseResources();
#endif
#endif
    if (!m_shutdownPrepared) {
        update();
    }
}

void SpectrumWidget::prepareForShutdown()
{
    if (m_shutdownPrepared) {
        return;
    }
    m_shutdownPrepared = true;

    prepareForTopLevelChange();
    setUpdatesEnabled(false);
    hide();

#ifdef LONGPATH_GPU_SPECTRUM
    releaseResources();
#ifdef Q_OS_MAC
    // Drop the native child window while its parent backing store is still
    // alive, so any remaining platform resources are gone before QWidgetWindow
    // destruction runs on app exit.
    //   [original comment from AetherSDR SpectrumWidget.cpp:2299-2301]
    destroy(true, true);
#endif
#endif
}

void SpectrumWidget::applyNativeWindowIsolationPolicy()
{
#if defined(LONGPATH_GPU_SPECTRUM) && defined(Q_OS_MAC)
    // Order matters: block ancestor promotion *before* requesting the native
    // window, so realizing the leaf's NSView can't drag its QWidget tree
    // native (redundant window-sized Core Animation backing stores, #4339).
    //   [original comment from AetherSDR SpectrumWidget.cpp:1861-1866]
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_NativeWindow);
#endif
}

QVector<float> SpectrumWidget::dbmOverRange(double loHz, double hiHz,
                                            int points) const
{
    if (points <= 1 || hiHz <= loHz || m_sampleRateHz <= 0.0) { return {}; }
    const int binCount = m_fullLinearBins.size();
    if (binCount <= 0) { return {}; }

    const double binWidth = m_sampleRateHz / binCount;
    const double fftLowHz = m_ddcCenterHz - m_sampleRateHz / 2.0;
    const double step     = (hiHz - loHz) / (points - 1);

    QVector<float> out(points, -200.0f);
    for (int i = 0; i < points; ++i) {
        const double f0 = loHz + step * (i - 0.5);
        const double f1 = loHz + step * (i + 0.5);
        int b0 = static_cast<int>(std::floor((f0 - fftLowHz) / binWidth));
        int b1 = static_cast<int>(std::ceil((f1 - fftLowHz) / binWidth));
        b0 = std::clamp(b0, 0, binCount - 1);
        b1 = std::clamp(b1, 0, binCount - 1);
        if (b1 < b0) { std::swap(b0, b1); }

        float peak = 0.0f;
        for (int b = b0; b <= b1; ++b) { peak = std::max(peak, m_fullLinearBins[b]); }
        out[i] = (peak > 0.0f)
            ? static_cast<float>(10.0 * std::log10(static_cast<double>(peak))
                                 + m_lastDbmOffset + m_dbmCalOffset)
            : -200.0f;
    }
    return out;
}

int SpectrumWidget::vfoXForTest() const
{
    const int specH = specHFromHeight(height(), m_spectrumFrac,
                                      kFreqScaleH + kDividerH);
    const QRect specRect(0, 0, width() - effectiveStripW(), specH);
    return hzToX(m_vfoHz, specRect);
}

int SpectrumWidget::freqScaleYForTest() const
{
    return specHFromHeight(height(), m_spectrumFrac,
                           kFreqScaleH + kDividerH) + kDividerH;
}

QRect SpectrumWidget::spectrumRect() const
{
    return QRect(0, 0, width() - effectiveStripW(),
                 specHFromHeight(height(), m_spectrumFrac,
                                 kFreqScaleH + kDividerH));
}
QRect SpectrumWidget::notchSpecRect() const
{
    const int specH = specHFromHeight(height(), m_spectrumFrac,
                                      kFreqScaleH + kDividerH);
    return QRect(0, 0, width() - effectiveStripW(), specH);
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
    // list is a Longpath addition and this clamp is what makes it honest.
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

    // ── Bearbeiten als Zahlen ────────────────────────────────────────
    //
    // Der Betreiber, 2026-08-21: „es waere auch gut, wenn man am
    // notchfilter klickt und dann diese bearbeite kann und auch
    // loeschen." Die Liste fester Breiten oben deckt den haeufigen
    // Fall; hier stehen Mitte und Breite frei.
    menu.addAction(tr("Edit notch…"), this,
                   [this, id]() { openNotchEditor(id, QCursor::pos()); });

    menu.addSeparator();
    // From AetherSDR src/gui/SpectrumWidget.cpp:8547 [@c6481cbf]
    // ("Remove TNF").
    menu.addAction(QStringLiteral("Remove Notch"), this,
                   [this, id]() { emit notchRemoveRequested(id); });

    // ── Alle auf einmal ──────────────────────────────────────────────
    //
    // Sechs verirrte Notches auf 40 m waren der Anlass (Bildschirmfoto
    // vom 2026-08-21). Sie entstehen leicht: Cmd- oder Strg-Klick ins
    // Spektrum legt einen an, und auf dem Mac ist ein versehentlicher
    // Cmd-Klick schnell passiert. Sechsmal zielen und rechtsklicken,
    // um sie wieder loszuwerden, ist die falsche Antwort darauf.
    //
    // Nur wenn es ueberhaupt mehr als einen gibt — bei einem einzigen
    // waere der Eintrag dasselbe wie „Remove Notch" und damit eine
    // Falle.
    if (m_notchMarkers.size() > 1) {
        menu.addAction(tr("Remove all %1 notch filters…")
                           .arg(m_notchMarkers.size()),
                       this, [this]() { emit notchRemoveAllRequested(); });
    }
}

// ── Den Editor aufmachen ─────────────────────────────────────────────
//
// Ein Fenster, nicht je Notch eines: es wird auf den gerade gemeinten
// gestellt. Die Wuensche gehen denselben Weg wie die aus dem Menue und
// vom Ziehen — ueber die notch*Requested-Signale ins Modell und von
// dort zurueck. Kein zweiter Pfad, der auseinanderlaufen koennte.
void SpectrumWidget::openNotchEditor(int id, const QPoint& globalPos)
{
    const NotchMarker* n = notchMarkerById(id);
    if (!n) { return; }

    if (!m_notchEditor) {
        m_notchEditor = new NotchEditPopup(this);
        connect(m_notchEditor, &NotchEditPopup::centreRequested,
                this, &SpectrumWidget::notchMoveRequested);
        connect(m_notchEditor, &NotchEditPopup::widthRequested,
                this, &SpectrumWidget::notchWidthRequested);
        connect(m_notchEditor, &NotchEditPopup::activeRequested,
                this, &SpectrumWidget::notchActiveRequested);
        connect(m_notchEditor, &NotchEditPopup::removeRequested,
                this, &SpectrumWidget::notchRemoveRequested);
    }
    m_notchEditor->showFor(id, n->freqMhz * 1.0e6, n->widthHz, n->active,
                           globalPos);
}

void SpectrumWidget::mousePressEvent(QMouseEvent* event)
{
    // ── Fokus AUSDRUECKLICH holen ───────────────────────────────────
    //
    // Der Betreiber am 2026-08-22: "mit dem cursor auf der tastatur
    // kann ich auch nicht nach rechts und links fahren."
    //
    // Qt::ClickFocus allein reicht nicht. Im nackten Widget klappte es
    // (dort ist es selbst das Fenster und hat den Fokus ohnehin) — im
    // vollen Hauptfenster hatte nach einem Klick ins Spektrum NIEMAND
    // den Fokus, gemessen mit QApplication::focusWidget(). Ohne
    // Fokus geht keine Taste an den Panadapter.
    //
    // setFocus() hier ist die eine Zeile, die nicht davon abhaengt,
    // auf welchem Weg der Druck ankommt.
    if (event->button() == Qt::LeftButton) {
        setFocus(Qt::MouseFocusReason);
    }

    // ── Getrennt heisst nicht tot ───────────────────────────────────
    //
    // Hier stand (Phase 3Q-8): getrennt JEDEN Linksklick schlucken und
    // stattdessen den Verbindungsdialog oeffnen. Gut gemeint, aber es
    // legt den ganzen Panadapter stumm — Abstimmen, Verschieben,
    // Zoomen, Pfeiltasten, alles.
    //
    // Der Betreiber am 2026-08-22: "test selbst, gar nichts
    // funktioniert." Nachgesehen und nachgemessen: beide Geraete
    // antworteten in dem Moment nicht, und getrennt reagierte
    // erwartungsgemaess NICHTS. Jede meiner Messungen davor lief
    // verbunden oder ohne Oberflaeche — deshalb war bei mir alles
    // gruen und bei ihm nichts brauchbar.
    //
    // Die Ansicht gehoert dem Rechner, nicht dem Geraet: Frequenz,
    // Zoom, Ausschnitt und Filterkanten sind Werte im Modell und
    // duerfen ohne Funkgeraet bedient werden. Thetis und AetherSDR
    // halten es genauso.
    //
    // Was bleibt: ein Klick auf das DISCONNECTED-Schild selbst oeffnet
    // weiter den Verbindungsdialog. Das war der eigentliche Zweck.
    if (m_connState != ConnectionState::Connected
        && event->button() == Qt::LeftButton
        && m_disconnectLabel && m_disconnectLabel->isVisible()) {
        const QRect banner(0, height() / 2 - 60, width(), 120);
        if (banner.contains(event->position().toPoint())) {
            emit disconnectedClickRequest();
            return;
        }
    }

    int w = width();
    int h = height();
    int specH = specHFromHeight(h, m_spectrumFrac, kFreqScaleH + kDividerH);
    int dividerY = specH;
    QRect specRect(0, 0, w - effectiveStripW(), specH);
    int mx = static_cast<int>(event->position().x());
    int my = static_cast<int>(event->position().y());

    // Bandstapel (2026-11-11): ein Klick auf eine Marke an der Skala
    // fuehrt zu diesem Eintrag. Vor allem anderen, sonst stimmte der
    // Klick nur auf die Frequenz ab und der Eintrag bliebe ungenutzt.
    if (event->button() == Qt::LeftButton
        && event->type() == QEvent::MouseButtonPress) {
        const QString marke = bandStackMarkAt(mx, my, specRect);
        if (!marke.isEmpty()) {
            emit bandStackMarkClicked(marke);
            return;
        }
    }

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
        // Finding #3 (rotor bench 2026-08-11): a right-press arriving
        // immediately after a spot/notch context menu closed is the
        // replayed remnant of the click that dismissed it — swallow it
        // instead of letting it open the overlay menu at the same
        // position. See the member comment in SpectrumWidget.h.
        if (m_contextMenuClosedMs != 0
            && QDateTime::currentMSecsSinceEpoch() - m_contextMenuClosedMs
                   < kContextMenuReplayGuardMs) {
            event->accept();
            return;
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

        // ── Auf einem Notch wird keiner angelegt (2026-10-30) ────────
        //
        // Betreiber: „control und klick erzeugt einen notchfilter. er
        // muss aber auch wieder entfernt werden können."
        //
        // Auf dem Mac IST Ctrl-Klick der Rechtsklick — die Geste, mit
        // der man am Trackpad ein Kontextmenue aufmacht —, und Qt
        // meldet den Ctrl-Modifier mit. Der Zweig hier kam vor dem
        // Treffertest und kehrte zurueck: jeder Ctrl-Klick legte einen
        // Notch an, auch der auf einen bestehenden, und das Menue mit
        // `Remove Notch` war von dieser Geste aus nie erreichbar.
        // Anlegen ging, entfernen nie — und wer am Trackpad sitzt, hat
        // keine zweite Taste, um daran vorbeizukommen.
        //
        // Also: liegt unter dem Zeiger schon ein Notch, gewinnt das
        // Menue. Nur auf freier Flaeche entsteht ein neuer. Die
        // Thetis-Belegung (console.cs:49614 [v2.10.3.15], Ctrl +
        // Rechtsklick legt an) bleibt damit, wo sie gemeint war.
        const bool aufNotch =
            (my < specH && mx <= specRect.right())
            && notchAtPixel(mx, specRect) >= 0;

        if (notchCtrlHeld && !aufNotch && my < specH && mx <= specRect.right()) {
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
        //   console.cs:49616 [v2.10.3.15]].  Longpath fills that suppressed
        //   slot with AetherSDR's notch menu rather than doing nothing.
        if (my < specH && mx <= specRect.right()) {
            const int hitNotch = notchAtPixel(mx, specRect);
            if (hitNotch >= 0) {
                // popup() instead of exec() — no nested event loop
                // inside mousePressEvent on the native QRhi surface
                // (finding #3; same treatment as the spot menu below).
                QMenu* menu = new QMenu(window());   // am Fenster, siehe showSpotClusterPopup()
                connect(this, &QObject::destroyed, menu, &QObject::deleteLater);
                menu->setAttribute(Qt::WA_DeleteOnClose);
                buildNotchContextMenu(hitNotch, *menu);
                connect(menu, &QMenu::aboutToHide, this, [this]() {
                    m_contextMenuClosedMs = QDateTime::currentMSecsSinceEpoch();
                });
                menu->popup(event->globalPosition().toPoint());
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
                // Der Spot nennt die Antennenfrequenz; abgestimmt wird
                // die ZF. Ohne diese Umrechnung spraenge die Scheibe
                // beim Klick auf 144 MHz -- eine Frequenz, die das
                // Geraet gar nicht kann (2026-10-31).
                const double freqHz   = zfFuerAchse(sm.freqMhz * 1.0e6);
                const QString source  = sm.source;

                // Heap + popup() + WA_DeleteOnClose instead of a
                // stack QMenu exec()'d from inside mousePressEvent
                // (finding #3, rotor bench 2026-08-11): the nested
                // exec() loop on the native QRhi surface let the
                // dismissing click re-reach this widget as a fresh
                // right-press, which fell through to the overlay
                // menu. popup() returns immediately; the lambdas
                // capture by value and use `this` as context object,
                // so they stay valid for the menu's async lifetime.
                QMenu* menu = new QMenu(window());   // am Fenster, siehe showSpotClusterPopup()
                connect(this, &QObject::destroyed, menu, &QObject::deleteLater);
                menu->setAttribute(Qt::WA_DeleteOnClose);
                connect(menu, &QMenu::aboutToHide, this, [this]() {
                    m_contextMenuClosedMs = QDateTime::currentMSecsSinceEpoch();
                });
                if (source == QStringLiteral("Memory")) {
                    const QString title = call.isEmpty()
                        ? QStringLiteral("Apply Memory")
                        : QString("Apply %1").arg(call);
                    menu->addAction(title, this,
                        [this, freqHz, spotIndex]() {
                            emit frequencyClicked(freqHz);
                            emit spotTriggered(spotIndex);
                        });
                } else {
                    menu->addAction(QString("Tune to %1").arg(call), this,
                        [this, freqHz]() {
                            emit frequencyClicked(freqHz);
                        });
                    // Same action and same downstream handler as the
                    // Spot List's right-click (2026-08-11) — the
                    // panadapter label is just the other place the
                    // operator meets the same spot.
                    menu->addAction(
                        QString("Turn rotor to %1").arg(call), this,
                        [this, call]() {
                            emit spotRotorRequested(call);
                        });
                    menu->addAction(QStringLiteral("Copy Callsign"), this,
                        [call]() {
                            QApplication::clipboard()->setText(call);
                        });
                    menu->addAction(QStringLiteral("Lookup on QRZ"), this,
                        [call]() {
                            QDesktopServices::openUrl(
                                QUrl(QStringLiteral("https://www.qrz.com/db/")
                                     + call));
                        });
                    menu->addSeparator();
                    menu->addAction(QStringLiteral("Remove Spot"), this,
                        [this, spotIndex]() {
                            emit spotRemoveRequested(spotIndex);
                        });
                }
                menu->popup(event->globalPosition().toPoint());
                event->accept();
                return;
            }
        }

        // Show overlay menu on right-click (default — not on a spot).
        if (!m_overlayMenu) {
            m_overlayMenu = new SpectrumOverlayMenu(this);
            connect(m_overlayMenu, &SpectrumOverlayMenu::openSetupPageRequested,
                    this, &SpectrumWidget::openSetupPageRequested);
            connect(m_overlayMenu, &SpectrumOverlayMenu::wfColorGainChanged,
                    this, [this](int v) { m_wfColorGain = v; update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::wfBlackLevelChanged,
                    this, [this](int v) { m_wfBlackLevel = v; update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::wfUpdatePeriodChanged,
                    this, [this](int ms) { setWfUpdatePeriodMs(ms); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::wfColorSchemeChanged,
                    this, [this](int v) { m_wfColorScheme = static_cast<WfColorScheme>(v); update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::fillAlphaChanged,
                    this, [this](float v) { m_fillAlpha = v; update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::spectrumRenderModeChanged,
                    this, [this](int idx) {
                        setSpectrumRenderMode(idx == 1 ? SpectrumRenderMode::Mode3D
                                                       : SpectrumRenderMode::Mode2D);
                    });
            connect(m_overlayMenu, &SpectrumOverlayMenu::dss3dTrueScaleChanged,
                    this, [this](bool on) { setDss3dTrueScale(on); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::panFillChanged,
                    this, [this](bool v) { m_panFill = v; update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::refLevelChanged,
                    this, [this](float v) { m_refLevel = v; update(); scheduleSettingsSave(); });
            connect(m_overlayMenu, &SpectrumOverlayMenu::dynRangeChanged,
                    this, [this](float v) { m_dynamicRange = v; update(); scheduleSettingsSave(); });
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
        }
        m_overlayMenu->setValues(m_wfColorGain, m_wfBlackLevel, false,
                                  static_cast<int>(m_wfColorScheme),
                                  m_fillAlpha, m_panFill, false,
                                  m_refLevel, m_dynamicRange, m_ctunEnabled);
        m_overlayMenu->setWfUpdatePeriodMs(m_wfUpdatePeriodMs);
        // Den gespeicherten Modus nachziehen, sonst stuende beim Oeffnen
        // immer "2D" da, auch wenn der Panadapter laengst 3D zeigt --
        // derselbe Fehler, der im SpectrumOverlayPanel schon einmal drin
        // war (siehe setSpectrumRenderModeIndex dort).
        m_overlayMenu->setSpectrumRenderModeIndex(
            m_renderMode == SpectrumRenderMode::Mode3D ? 1 : 0);
        m_overlayMenu->setDss3dTrueScale(m_dss3dTrueScale);
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
                // Fuer einen folgenden Doppelklick merken, BEVOR das
                // Abstimmen das Bild verschiebt (siehe m_spotKlickRufzeichen).
                if (hr.markerIndex >= 0 && hr.markerIndex < m_spotMarkers.size()) {
                    m_spotKlickRufzeichen =
                        m_spotMarkers[hr.markerIndex].callsign.trimmed();
                    m_spotKlickPos = pos;
                    m_spotKlickHz  = hr.freqMhz * 1.0e6;
                    m_spotKlickMs  = QDateTime::currentMSecsSinceEpoch();
                }
                // Longpath signal contract: frequencyClicked(double hz).
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
        const QRect strip    = Longpath::DbmStrip::stripRect(fullSpecRect, kDbmStripW);
        const QRect arrowRow = Longpath::DbmStrip::arrowRowRect(strip, kDbmArrowH);

        if (arrowRow.contains(mx, my)) {
            const int hit = Longpath::DbmStrip::arrowHit(mx, arrowRow);
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

        // ── Skala ziehen = ANSICHT verschieben (2026-08-22) ──────────
        //
        // Hier stand AetherSDRs Bandbreiten-Zug (Skala ziehen aendert
        // den Zoom). Der Betreiber will die Aufteilung der Vorlage, und sie
        // ist die bessere: Zoom liegt bereits auf −/+, dem Rad und den
        // S/B-Knoepfen — die Skala ist damit frei fuer das, was ihr
        // Aussehen verspricht: anfassen und die Ansicht schieben.
        //
        // Der Verschiebe-Zug sass zuerst als Rueckfall WEITER UNTEN im
        // Handler und war totes Holz: dieser Zweig hier hat jeden
        // Skala-Druck vorher gefressen. Der Test dazu war gruen AUS
        // DEM FALSCHEN GRUND — der Bandbreiten-Zug re-zentriert um den
        // Zeiger, und die Pruefung sah 'Mitte hat sich geaendert'.
        // Deshalb prueft tst_pan_drag_is_deliberate jetzt beides:
        // Mitte wandert, Bandbreite bleibt.
        m_draggingPan = true;
        m_panDragStartX = mx;
        m_panDragStartCenter = m_centerHz;
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    // 3b. Notch (TNF) grab.  Thetis runs its notch block first inside
    // case MouseButtons.Left:, ahead of every filter drag, so a notch
    // marker sitting on a filter edge still drags as a notch.  The dBm
    // strip / divider / freq-scale rows above are Longpath chrome outside
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

    // ── 2b. Schmaler Durchlass: der VFO hat Vorrang ─────────────────
    //
    // Bei 2,9 kHz Filterbreite und 100 kHz Ansicht ist der Durchlass
    // rund FUENFZEHN Punkte breit. Davon gehen zweimal kFilterGrab fuer
    // die Filterkanten ab — uebrig bleibt ein Griff von wenigen
    // Punkten, den kein Mensch zuverlaessig trifft.
    //
    // Der Betreiber am 2026-08-22, woertlich: "Du klickst auf den
    // Balken, möchtest ein bisschen nach rechts und nach links ... und
    // dann ist der Cursor irgendwo." Genau das ist die Folge: wer
    // danebentrifft, zieht eine Filterkante oder stimmt per Klick auf
    // die getroffene Stelle ab. Nachgemessen am Geraet — Druck bei
    // x=440 und x=409 im sichtbaren tuerkisen Balken, beide Male
    // passierte NICHTS, weil die Innenzone dazwischen lag.
    //
    // Also: ist der Durchlass zu schmal fuer drei Griffe, bekommt der
    // haeufigste den Vorrang. Abstimmen macht man dauernd,
    // Filterkanten zieht man selten — und fuer die gibt es die
    // Filterleiste und das Menue.
    const int xVfoNow  = hzToX(m_vfoHz, specRect);
    const int passWide = std::abs(xHi - xLo);
    //
    // Schwelle in BILDPUNKTEN, nicht in Vielfachen von kFilterGrab:
    // massgeblich ist, ob drei Griffe nebeneinander passen, und das
    // haengt an der Darstellung. 2,9 kHz auf 100 kHz Ansicht sind rund
    // 28 Punkte — zu wenig fuer drei. Auf einer 10-kHz-Ansicht sind es
    // 280, da bleiben die Filterkanten normal greifbar.
    // Gegriffen wird das BAND, nicht der Strich.
    //
    // Der Betreiber am 2026-08-22: "ich möchte, wenn ich meinen
    // bereich von 2,7 k, sprich meinen bereich wo ich höre, mit click
    // und click hold verschieben können."
    //
    // Die erste Fassung nahm einen Radius um den VFO-STRICH. Bei LSB
    // liegt der Durchlass aber LINKS davon, bei USB rechts — wer auf
    // die tuerkise Flaeche zielt, traf also je nach Betriebsart
    // daneben und begann stattdessen ein Verschieben. Jetzt zaehlt die
    // Flaeche selbst, mit acht Punkten Rand aussen herum, damit man
    // sie auch bei schmaler Ansicht bequem fasst.
    const int grabL = qMin(xLo, xVfoNow) - 8;
    const int grabR = qMax(xHi, xVfoNow) + 8;
    if (passWide < 40 && mx >= grabL && mx <= grabR) {
        m_draggingVfo    = true;
        m_vfoDragStartX  = mx;
        m_vfoDragStartHz = m_vfoHz;
        m_vfoDragHzPerPx = specRect.width() > 0
            ? m_bandwidthHz / static_cast<double>(specRect.width())
            : 0.0;
        setCursor(Qt::SizeHorCursor);
        return;
    }

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
        // Die gegriffene Kante wird hell und zeigt ihren Wert (2026-11-08).
        markOverlayDirty();
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
        // Den Bezug JETZT festhalten — siehe mouseMoveEvent.
        m_vfoDragStartX  = mx;
        m_vfoDragStartHz = m_vfoHz;
        m_vfoDragHzPerPx = specRect.width() > 0
            ? m_bandwidthHz / static_cast<double>(specRect.width())
            : 0.0;
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

    // ── 5. Verschieben NUR an der Frequenzskala (2026-08-22) ─────────
    //
    // AetherSDR machte jeden Druck ins Spektrum zum Ansicht-Verschieben
    // (SpectrumWidget.cpp:879-887). Der Betreiber ist damit live von
    // 7,07 auf 6,38 MHz gerutscht, der VFO blieb ausserhalb des Bilds
    // zurueck: „einmal unabsichtlich klick und alles verrueckt sich…
    // das anklicken und verschieben ist das problem."
    //
    // ── Ziehen im Spektrum verschiebt die Ansicht (wie im Original) ──
    //
    // Am Vormittag des 2026-08-22 habe ich das ABGESCHALTET, weil der
    // Betreiber meldete, ein unabsichtlicher Klick verrücke alles um
    // Megahertz. Die Abschaltung war die falsche Antwort auf einen
    // richtigen Befund: schuld war nicht das Verschieben, sondern die
    // absolute Rechnung im VFO-Zug (siehe mouseMoveEvent) — die
    // schaukelte sich mit der Nachzentrierung auf.
    //
    // Der Preis der Abschaltung zeigte sich am selben Abend: "wenn man
    // den balken verloren hat kann man nicht mit klick and hold den
    // bereich verändern." Genau so ist es — ohne Verschieben gibt es
    // keinen Weg zurueck zu einem VFO, der aus dem Bild gelaufen ist.
    //
    // AetherSDR sagt es an seiner Stelle woertlich
    // (SpectrumWidget.cpp:10063 [@0cd4559]):
    //
    //     // Click in FFT area → start pan drag
    //
    // und behaelt daneben das Abstimmen per Klick, mit einer
    // Bewegungsschwelle beim Loslassen (dort "single-click-to-tune
    // drag threshold", Zeile 9324). BEIDES zusammen ist die richtige
    // Aufteilung, und genau die stellen wir wieder her:
    //
    //   ziehen  -> Ansicht verschieben
    //   klicken -> abstimmen (unter 4 Punkten Bewegung)
    //   ziehen auf dem tuerkisen Balken -> abstimmen (relativ)
    m_panDragStartX = mx;
    m_panDragLastX  = mx;
    m_panDragStartCenter = m_centerHz;

    // NICHT sofort verschieben, sondern erst ab einer Schwelle.
    //
    // Der Betreiber am 2026-08-22: "bei doppelklick im padadapter
    // ruscht er herum, anstatt den balken dort hinzubringen." Genau
    // so: der erste Klick eines Doppelklicks begann sofort ein
    // Verschieben, und die winzige Handbewegung zwischen den beiden
    // Klicks rutschte die Ansicht weg — der Doppelklick landete dann
    // woanders, als man gezielt hatte. Gemessen: ZWEI Punkte Zittern
    // reichten fuer 207 Hz.
    //
    // Dieselbe Schwelle, die das Loslassen schon fuer den Klick
    // benutzt (4 Punkte). Darunter ist eine Geste ein Klick, kein
    // Zug; darueber beginnt das Verschieben in mouseMoveEvent.
    //
    // Die Frequenzskala oben behaelt ihr sofortiges Verschieben: dort
    // gibt es nichts anzuklicken, die Geste ist eindeutig.
    m_draggingPan  = false;
    m_panDragArmed = true;

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

    // Bandstapel (2026-11-11): Ueberfahren einer Marke zeigt den Eintrag.
    if (event->buttons() == Qt::NoButton && !m_bandStackMarks.isEmpty()) {
        const QString marke = bandStackMarkAt(mx, my, specRect);
        if (!marke.isEmpty()) {
            for (const BandStackMark& m : std::as_const(m_bandStackMarks)) {
                if (m.id == marke) {
                    QToolTip::showText(event->globalPosition().toPoint(), m.tip, this);
                    break;
                }
            }
        }
    }

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
        // Rast auf 50 Hz (2026-11-08, Entwurf "Bandbreite ablesbar"); mit
        // Shift fein, ohne Rast. Die Zahl an der Kante sagt, welches gilt.
        m_kanteFein = (event->modifiers() & Qt::ShiftModifier);
        if (!m_kanteFein) {
            newHz = Durchlass::raste(newHz);
        }
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
        // Die Marke samt Kantenwert liegt in der statischen Schicht.
        markOverlayDirty();
        update();
        return;
    }

    if (m_draggingVfo) {
        // ── Slide-to-tune, RELATIV gerechnet ────────────────────────
        //
        // Frueher stand hier die Fassung aus AetherSDR
        // (SpectrumWidget.cpp:1222-1228 [@0cd4559]):
        //
        //     double hz = xToHz(mx, specRect);
        //
        // Also absolut: "Zeiger bei x heisst diese Frequenz". Das ist
        // richtig, SOLANGE DIE ANSICHT STEHT. Unsere steht nicht — sie
        // zentriert sich nach jeder Frequenzaenderung neu auf den VFO.
        // Damit rutscht die Skala unter dem Zeiger weg, der naechste
        // Schritt misst wieder den vollen Abstand zur NEUEN Mitte, und
        // die Sache schaukelt sich auf: aus einer Handbewegung von
        // zwei Zentimetern wird ein Sprung ueber Hunderte Kilohertz.
        //
        // Der Betreiber hat das seit Tagen gemeldet — "einmal
        // unabsichtlich klick auf die frequenz und alles verrückt sich
        // auf 6 mhz" (2026-08-21) und wieder am 2026-08-22, als es
        // beim Probieren prompt auf 7,82 MHz davonlief. Es war nie ein
        // zu empfindlicher Klick; es war diese Rueckkopplung.
        //
        // Relativ zum Bezug beim Druck ist sie ausgeschlossen: der
        // Zeigerweg bestimmt den Frequenzweg, und was die Ansicht
        // darunter tut, spielt keine Rolle mehr.
        const double hz = std::round(
            (m_vfoDragStartHz
             + (mx - m_vfoDragStartX) * m_vfoDragHzPerPx) / m_stepHz)
            * m_stepHz;
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
        // ── Auch das ist KEIN Zug (2026-11-02) ──────────────────
        //
        // Dieselbe Fahne wie beim Randscrollen, aus demselben Grund:
        // das Fenster stellt sich SELBST auf den VFO. In CTUN nahm
        // MainWindow das fuer einen Zug und verstellte die DDC --
        // genau das Gegenteil dessen, was CTUN verspricht. Siehe
        // core/FensterRandScroll.h, mitteIstDdcBefehl.
            m_mitteFolgtDemVfo = true;
            emit centerChanged(m_centerHz);
            m_mitteFolgtDemVfo = false;
        }
        updateVfoPositions();
#ifdef LONGPATH_GPU_SPECTRUM
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
#ifdef LONGPATH_GPU_SPECTRUM
        markOverlayDirty();
#else
        update();
#endif
        return;
    }

    // Aus der Bereitschaft wird ein Zug, sobald die Schwelle
    // ueberschritten ist — siehe mousePressEvent.
    if (m_panDragArmed && !m_draggingPan
        && std::abs(mx - m_panDragStartX) > 4) {
        m_draggingPan  = true;
        m_panDragLastX = mx;
        setCursor(Qt::ClosedHandCursor);
    }

    if (m_draggingPan) {
        // ── Schrittweise rechnen, nicht gegen den Startpunkt ─────────
        //
        // Das Original rechnet gegen den Druckpunkt
        // (AetherSDR SpectrumWidget.cpp:10611-10614 [@0cd4559]):
        //
        //     dx = x - m_panDragStartX;  center = startCenter + delta;
        //
        // Das setzt voraus, dass die Ansicht waehrend des Zuges nur
        // durch den Zug selbst wandert. Bei uns wandert sie auch
        // anders: centerChanged geht ins Hauptfenster, dort setzt es
        // ohne CTUN die Slice-Frequenz — der VFO zieht mit, die
        // Ansicht zentriert nach, und der naechste Schritt misst
        // gegen einen Startwert, der nicht mehr gilt.
        //
        // Gemessen am Geraet (2026-08-22, ANAN-10/100): 100 Punkte Zug
        // ergaben 64 kHz statt der erwarteten 18 — Faktor dreieinhalb.
        // Der Betreiber sah davon nur das Ergebnis: "ich clicke und
        // fahre nach rechts, bildschirm geht nach links."
        //
        // Schrittweise ist es dagegen dicht: jeder Mausschritt bewegt
        // die Ansicht um genau seinen eigenen Weg, ausgehend davon, wo
        // sie JETZT steht. Was sonst noch an ihr zieht, geht in den
        // naechsten Schritt ein, statt sich aufzuaddieren.
        const double deltaPx = mx - m_panDragLastX;
        m_panDragLastX = mx;
        const double deltaHz =
            -(deltaPx / static_cast<double>(specRect.width())) * m_bandwidthHz;
        m_centerHz += deltaHz;
        emit centerChanged(m_centerHz);
        updateVfoPositions();
#ifdef LONGPATH_GPU_SPECTRUM
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
        // Abstimmhilfe: HIER, nicht oben am Anfang der Funktion. Sie
        // liest m_hoveredNotchId, und der Wert steht erst nach den drei
        // Zeilen darueber. Weiter oben gerufen zeigte sie sich fuer ein
        // Ereignis auch ueber einer Kerbe — sichtbar als Flackern beim
        // Darueberfahren.
        noteTuneGuideActivity();
        if (hoverNotch >= 0) {
            const NotchMarker* n = notchMarkerById(hoverNotch);
            if (n) {
                // AetherSDR renders this in a styled QLabel popup
                // (m_tnfHoverPopup, src/gui/SpectrumWidget.cpp:13591-13646
                // [@c6481cbf]); Longpath routes the same frequency +
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

#ifdef LONGPATH_GPU_SPECTRUM
    markOverlayDirty();
#else
    update();
#endif
    QWidget::mouseMoveEvent(event);
}

// ── Doppelklick auf ein Spot-Etikett: ins Logbuch ────────────────────
//
// Auf Ansage des Betreibers (2026-08-19): „wenn ein Call gespottet ist
// und ich sehe es am Panadapter, soll es mit Doppelklick sofort ins
// Logbuch übernommen werden, Frequenz usw."
//
// WAS HIER NICHT PASSIERT, und das ist Absicht: die Frequenz wird nicht
// mitgeschickt. Der ERSTE Klick des Doppelklicks stimmt schon auf den
// Spot ab (mousePressEvent, Spot-Trefferflaeche), und
// RotorLogbookPanel::buildEntry liest Frequenz, Band und Betriebsart aus
// der aktiven Scheibe. Die Frequenz zweimal zu fuehren waere eine zweite
// Wahrheit, die irgendwann von der ersten abweicht.
//
// Verlaufsmarken des S-Verlaufs sind ausgenommen: ihr Etikett ist eine
// S-Stufe („S7"), kein Rufzeichen. Sie tragen markerIndex -1, und genau
// daran erkennt man sie.
// ── Der Rechtsklick erzeugt ZWEI Ereignisse ──────────────────────────
//
// Der Betreiber, 2026-08-21, mit einem Bildschirmfoto: Rechtsklick
// genau auf einen gelben Notch-Balken — und aufgegangen ist das
// Pan-Menue („Add slice on this pan / Float this pan / Extended
// view"), nicht das Notch-Menue.
//
// Der Grund steht nicht im Notch-Code, sondern in Qt: ein Rechtsklick
// liefert einen mousePressEvent UND, davon unabhaengig, einen
// QContextMenuEvent. mousePressEvent hat unser Notch-Menue mit popup()
// aufgemacht und das Ereignis angenommen — das Kontextmenue-Ereignis
// aber blieb unbeantwortet, wanderte zum Elternteil, und
// PanadapterApplet::contextMenuEvent macht sein Menue mit exec() auf.
// exec() haelt die Runde an und legt sich obenauf. Unseres lag darunter.
//
// Genau deshalb ist der Test von gestern gruen gewesen: er hat auf
// einen freistehenden SpectrumWidget geklickt. Ohne Elternteil gibt es
// niemanden, zu dem das zweite Ereignis wandern koennte.
//
// Hier wird es beantwortet — aber nur dort, wo WIR schon etwas zeigen.
// Auf freier Flaeche bleibt es unbeantwortet und das Pan-Menue geht auf
// wie bisher.
void SpectrumWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if (hasOwnContextMenuAt(event->pos())) {
        event->accept();
        return;
    }
    event->ignore();
}

/// Zeigt der Panadapter an dieser Stelle ein eigenes Menue?
///
/// Dieselben Treffertests wie im Druckereignis — bewusst hier
/// zusammengefasst, damit die beiden nicht auseinanderlaufen koennen.
/// Liefen sie auseinander, waere das Ergebnis genau der Fehler von
/// oben: eines der beiden Menues sichtbar, das andere nicht.
bool SpectrumWidget::hasOwnContextMenuAt(const QPoint& pos) const
{
    const QRect nRect = notchSpecRect();
    if (pos.y() < nRect.height() && pos.x() <= nRect.right()) {
        if (notchAtPixel(pos.x(), nRect) >= 0) { return true; }
    }

    if (m_showSpots) {
        for (const auto& hr : m_spotClickRects) {
            if (hr.rect.contains(pos)) { return true; }
        }
    }
    return false;
}

void SpectrumWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    // ── Doppelklick auf einen Notch macht den Editor auf ─────────────
    //
    // „wenn man am notchfilter klickt und dann diese bearbeite kann"
    // (2026-08-21). Doppelklick statt einfachem Klick, weil der
    // einfache schon vergeben ist: er waehlt aus und beginnt das
    // Ziehen.
    //
    // VOR der Spot-Pruefung und vor allem VOR der Getrennt-Sperre in
    // mousePressEvent: Notches sind Einstellungen des Rechners, nicht
    // des Geraets. Sie liegen auch ohne Funkgeraet da, sie werden
    // gespeichert — und wer sie ohne Geraet nicht loeschen kann, sitzt
    // fest. Genau danach hat der Betreiber gefragt, mit geschlossener
    // App.
    if (event->button() == Qt::LeftButton) {
        const QPoint dp = event->position().toPoint();
        const QRect nRect = notchSpecRect();
        if (dp.y() < nRect.height() && dp.x() <= nRect.right()) {
            const int hit = notchAtPixel(dp.x(), nRect);
            if (hit >= 0) {
                openNotchEditor(hit, event->globalPosition().toPoint());
                event->accept();
                return;
            }
        }
    }

    if (event->button() == Qt::LeftButton && m_showSpots) {
        const QPoint pos = event->position().toPoint();
        // Zuerst der Spot des ersten Klicks: er hat abgestimmt, und wo
        // das Bild der Abstimmung folgt, liegt er jetzt woanders. Ein
        // Spot, der inzwischen unter die Maus gerutscht ist, waere der
        // falsche.
        if (!m_spotKlickRufzeichen.isEmpty()) {
            const QString call = m_spotKlickRufzeichen;
            const qint64 alter =
                QDateTime::currentMSecsSinceEpoch() - m_spotKlickMs;
            const int nah = qMax(4, QGuiApplication::styleHints()->startDragDistance());
            m_spotKlickRufzeichen.clear();
            if (alter >= 0
                && alter <= QGuiApplication::styleHints()->mouseDoubleClickInterval() + 200
                && (pos - m_spotKlickPos).manhattanLength() <= nah) {
                // Noch einmal abstimmen: lieferte das System einen
                // zweiten Druck, hat der auf leerer Flaeche abgestimmt.
                emit frequencyClicked(m_spotKlickHz);
                emit spotLogRequested(call);
                event->accept();
                return;
            }
        }
        for (const auto& hr : m_spotClickRects) {
            if (!hr.rect.contains(pos)) { continue; }
            if (hr.markerIndex < 0 || hr.markerIndex >= m_spotMarkers.size()) {
                continue;
            }
            const QString call = m_spotMarkers[hr.markerIndex].callsign.trimmed();
            if (call.isEmpty()) { continue; }
            emit spotLogRequested(call);
            event->accept();
            return;
        }
    }

    // ── Doppelklick auf freie Flaeche: SOFORT dorthin abstimmen ─────
    //
    // Der Betreiber am 2026-08-22: "wenn ich einen Doppelklick im
    // Display im Panadapter mache, dass dort automatisch die Frequenz
    // auch hinhüpft. Das heißt, wenn ich auf sieben Komma fünf stehe
    // und ich gehe auf sieben Komma zwei und mache einen Doppelklick
    // auf sieben Komma zwei, sollte auch meine Frequenz sofort auf
    // sieben Komma zwei hüpfen."
    //
    // AetherSDR haelt es genauso (SpectrumWidget.cpp:10063 [@0cd4559]:
    // "Click in FFT area → start pan drag (tune on double-click
    // only)"). Bisher fiel der Doppelklick bei uns nur auf
    // mousePressEvent durch — und seit das Ziehen wieder verschiebt,
    // hiess das: er begann ein Verschieben und stimmte NICHT ab.
    if (event->button() == Qt::LeftButton) {
        const int w = width();
        const int h = height();
        const int specH = specHFromHeight(h, m_spectrumFrac,
                                          kFreqScaleH + kDividerH);
        const QRect specRect(0, 0, w - effectiveStripW(), specH);
        const QPoint dp = event->position().toPoint();
        if (specRect.contains(dp)) {
            // Ein etwaiges Verschieben, das der erste Klick begonnen
            // hat, hier beenden — sonst zieht die Ansicht beim
            // Loslassen noch hinterher.
            m_draggingPan  = false;
            m_panDragArmed = false;
            unsetCursor();
            const double hz = std::round(xToHz(dp.x(), specRect) / m_stepHz)
                              * m_stepHz;
            emit frequencyClicked(hz);
            event->accept();
            return;
        }
    }

    // Sonst weiter wie vor 2026-08-19, als der Filter den Doppelklick
    // unbesehen als Druck weitergab.
    mousePressEvent(event);
}

void SpectrumWidget::keyPressEvent(QKeyEvent* event)
{
    // ── Links/Rechts stimmen um einen Schritt ───────────────────────
    //
    // "oder auch mit dem rechten und linken Cursortaste muss das
    // automatisch rübergezogen werden" (Betreiber, 2026-08-22).
    //
    // Schrittweite ist die eingestellte (STEP in der Befehlsleiste),
    // damit Tastatur und Mausrad dasselbe tun. Mit Umschalt zehnfach —
    // so kommt man ueber ein Band, ohne die Taste festzuhalten.
    // Auch getrennt bedienbar — siehe die Begruendung in
    // mousePressEvent.
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        const double mult = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
        const double delta = (event->key() == Qt::Key_Right ? 1.0 : -1.0)
                             * m_stepHz * mult;
        emit frequencyClicked(std::round((m_vfoHz + delta) / m_stepHz)
                              * m_stepHz);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
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
        // Nicht mehr an m_draggingPan gebunden: seit dem Umbau vom
        // 2026-08-22 verschiebt nur noch die Frequenzskala, aber der
        // Abstimmklick im Spektrum muss weiter funktionieren.
        {
            int dx = std::abs(static_cast<int>(event->position().x()) - m_panDragStartX);
            // Nur die Bewegungsschwelle entscheidet, nicht der
            // Verschieb-Modus: seit dem 2026-08-22 ist Ziehen im
            // Spektrum wieder ein Verschieben (Aether-Verhalten), und
            // waere die Bedingung noch an m_draggingPan gebunden,
            // koennte man ueberhaupt nicht mehr per Klick abstimmen.
            bool lueckeGetroffen = false;
            if (dx <= 4 && m_zeigeLuecken) {
                // Lueckenfinder (2026-11-08): ein Klick auf einen blauen
                // Balken oder den CQ-Chip stimmt in die Mitte der Luecke.
                for (const auto& k : m_lueckenKlick) {
                    if (k.first.contains(event->position())) {
                        const double ziel = (m_stepHz > 0)
                            ? std::round(k.second / m_stepHz) * m_stepHz : k.second;
                        emit frequencyClicked(ziel);
                        lueckeGetroffen = true;
                        break;
                    }
                }
            }
            if (dx <= 4 && !lueckeGetroffen) {
                int w = width();
                int specH = static_cast<int>(height() * m_spectrumFrac);
                QRect specRect(0, 0, w - effectiveStripW(), specH);
                double hz = xToHz(static_cast<int>(event->position().x()), specRect);
                hz = std::round(hz / m_stepHz) * m_stepHz;

                // Phase 3F Sub-Epic F Task 12: click-in-wing vs click-in-island
                // disambiguation. Only engages when extended-mode is on
                // (operator zoomed past the DDC's listenable range). When
                // off, this falls through to the existing single-path
                // frequencyClicked behavior — no regression to non-extended
                // pan tuning.
                if (m_extendedMode && m_sampleRateHz > 0.0) {
                    const double halfBwHz = m_sampleRateHz * 0.5;
                    if (std::abs(hz - m_ddcCenterHz) <= halfBwHz) {
                        // Click landed inside the listenable island —
                        // standard slice retune.
                        emit frequencyClicked(hz);
                    } else {
                        // Click landed in a wing — operator wants the
                        // clicked Hz to become the new DDC center.
                        // MainWindow forwards this to slice frequency,
                        // which propagates to the codec / DDC NCO retune.
                        emit ddcRetuneRequested(hz);
                    }
                } else {
                    emit frequencyClicked(hz);
                }
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
        if (m_draggingFilter != FilterEdge::None) {
            // Kante losgelassen: die helle Kante und ihr Wert verschwinden.
            m_draggingFilter = FilterEdge::None;
            markOverlayDirty();
        }
        m_draggingVfo = false;
        m_draggingDivider = false;
        m_draggingPan  = false;
        m_panDragArmed = false;
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
    // Der Zeiger ist weg, also auch seine Hilfe — ohne das blieben Linie
    // und Beschriftung bis zum Ablauf der vier Sekunden am letzten Ort
    // stehen, wo kein Zeiger mehr ist.
    if (m_tuneGuideShowing) {
        m_tuneGuideShowing = false;
        if (m_tuneGuideHideTimer) { m_tuneGuideHideTimer->stop(); }
        markOverlayDirty();
    }
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
    // Cmd/Ctrl+scroll: zoom bandwidth
    // Shift+scroll: adjust ref level
    //
    // Schritte statt Rohwerte (2026-09-27). Ein Trackpad schickt viele
    // kleine Ereignisse; jedes zaehlte als voller Schritt, und ein
    // Wischen stimmte Dutzende Schritte weit ab oder zoomte davon.
    const int steps = wheelStepsFor(event);
    if (steps == 0) {
        event->accept();
        return;
    }

    if (event->modifiers() & Qt::MetaModifier || event->modifiers() & Qt::ControlModifier) {
        // Cmd+scroll (macOS) or Ctrl+scroll: zoom bandwidth in/out
        double factor = (steps > 0) ? 0.8 : 1.25;
        double newBw = m_bandwidthHz * factor;
        newBw = std::clamp(newBw, 1000.0, m_sampleRateHz);
        // Recenter on VFO when zooming. Ueber setFrequencyRange
        // (2026-09-27): nur dort wird der Wasserfall umgerechnet, der
        // Zoom gespeichert und frequencyRangeChanged gesendet, dem die
        // Zoom-Leiste folgt -- das Rad ging an allen dreien vorbei.
        const double oldCenter = m_centerHz;
        setFrequencyRange(m_vfoHz, newBw);
        if (!qFuzzyCompare(oldCenter, m_centerHz)) {
        // ── Auch das ist KEIN Zug (2026-11-02) ──────────────────
        //
        // Dieselbe Fahne wie beim Randscrollen, aus demselben Grund:
        // das Fenster stellt sich SELBST auf den VFO. In CTUN nahm
        // MainWindow das fuer einen Zug und verstellte die DDC --
        // genau das Gegenteil dessen, was CTUN verspricht. Siehe
        // core/FensterRandScroll.h, mitteIstDdcBefehl.
            m_mitteFolgtDemVfo = true;
            emit centerChanged(m_centerHz);
            m_mitteFolgtDemVfo = false;
        }
        emit bandwidthChangeRequested(newBw);
    } else if (event->modifiers() & Qt::ShiftModifier) {
        // Shift+scroll: adjust ref level
        float step = (steps > 0) ? 5.0f : -5.0f;
        m_refLevel = qBound(-160.0f, m_refLevel + step, 20.0f);
        scheduleSettingsSave();
    } else {
        // Plain scroll: tune VFO by step size, auf das Schrittraster
        // (2026-09-27; vorher lief 14.215.050 mit 100-Hz-Schritten auf
        // ...150/...250 statt auf ...100/...200).
        emit frequencyClicked(std::max(snapTuneHz(m_vfoHz, m_stepHz, steps), 100000.0));
    }

    update();
    event->accept();
}

// From Thetis console.cs:31215-31228 [@852bf0e] -- SnapTune(freq_mhz, step_size_hz, num_steps):
//   long temp = freq_hz / step_size_hz; // do integer division to end up on a step size boundary
//   // handle when starting frequency was already on a step size boundary and tuning down
//   if (num_steps < 0 && freq_hz % step_size_hz != 0)
//       num_steps++; // off boundary -- add one as the divide takes care of one step
//   temp += num_steps; freq_hz = temp * step_size_hz;
double SpectrumWidget::snapTuneHz(double freqHz, int stepHz, int numSteps)
{
    if (stepHz <= 0) { return freqHz; }
    const qint64 f = static_cast<qint64>(std::llround(freqHz));
    qint64 temp = f / stepHz;
    if (numSteps < 0 && f % stepHz != 0) {
        numSteps++;
    }
    temp += numSteps;
    return static_cast<double>(temp * stepHz);
}

// Radereignis -> ganze Schritte (2026-09-27).
// From AetherSDR src/gui/SpectrumWidget.cpp:11529-11573 [@d58e2b8a]:
// Trackpad (pixelDelta) je ~15 px ein Schritt, Nachlauf und
// Querwischen ignoriert; Mausrad (angleDelta) je 120 ein Schritt; beides
// auf +-1 je Ereignis begrenzt und in 50 ms entprellt (#504, #556, #2150).
int SpectrumWidget::wheelStepsFor(QWheelEvent* ev)
{
    int steps = 0;
    if (!ev->pixelDelta().isNull()) {
        if (ev->phase() == Qt::ScrollMomentum) { return 0; }
        if (qAbs(ev->pixelDelta().x()) > qAbs(ev->pixelDelta().y())) { return 0; }
        m_wheelPixelAccum += ev->pixelDelta().y();
        steps = m_wheelPixelAccum / 15;
        m_wheelPixelAccum -= steps * 15;
    } else {
        m_wheelAngleAccum += ev->angleDelta().y();
        steps = m_wheelAngleAccum / 120;
        m_wheelAngleAccum -= steps * 120;
    }
    steps = qBound(-1, steps, 1);
    if (steps != 0) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastWheelStepMs < 50) {
            return 0;
        }
        m_lastWheelStepMs = now;
    }
    return steps;
}

// ============================================================================
// GPU Rendering Path (QRhiWidget)
// Ported from AetherSDR SpectrumWidget GPU pipeline
// ============================================================================

#ifdef LONGPATH_GPU_SPECTRUM

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

// ── Did that actually work? ──────────────────────────────────────────
//
// Every QRhi create() returns a bool and this file used to discard all
// twenty-five of them. That is why the magenta waterfall took three
// attempts to diagnose: when a texture or a pipeline fails to come up,
// the draw call is issued against a null or incomplete object, the
// shader samples whatever that GPU memory happened to hold, and the
// only evidence anywhere is the colour on screen. On Metal the garbage
// is very often magenta.
//
// A failure here is not recoverable and is not meant to be. What it is
// meant to be is LOUD, and named, so the next person reads one log line
// instead of bisecting three subsystems.
template <typename T>
static bool rhiCreate(T* obj, const char* what, QString* firstFailure)
{
    if (!obj) {
        qWarning() << "SpectrumWidget: could not allocate" << what;
        if (firstFailure && firstFailure->isEmpty()) {
            *firstFailure = QStringLiteral("allocation of %1")
                                .arg(QLatin1String(what));
        }
        return false;
    }
    if (!obj->create()) {
        qWarning() << "SpectrumWidget: QRhi create() FAILED for" << what
                   << "— the GPU spectrum will not draw correctly";
        if (firstFailure && firstFailure->isEmpty()) {
            *firstFailure = QLatin1String(what);
        }
        return false;
    }
    return true;
}


void SpectrumWidget::initWaterfallPipeline()
{
    QRhi* r = rhi();

    m_wfVbo = r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kQuadData));
    if (!rhiCreate(m_wfVbo, "waterfall vertex buffer", &m_gpuInitFailure)) { return; }

    m_wfUbo = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16);
    if (!rhiCreate(m_wfUbo, "waterfall uniform buffer", &m_gpuInitFailure)) { return; }

    m_wfGpuTexW = qMax(width(), 64);
    m_wfGpuTexH = qMax(m_waterfall.height(), 64);
    m_wfGpuTex = r->newTexture(QRhiTexture::RGBA8, QSize(m_wfGpuTexW, m_wfGpuTexH));
    if (!rhiCreate(m_wfGpuTex, "waterfall texture", &m_gpuInitFailure)) { return; }

    // From AetherSDR: ClampToEdge U, Repeat V (for ring buffer wrap)
    m_wfSampler = r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                 QRhiSampler::None,
                                 QRhiSampler::ClampToEdge, QRhiSampler::Repeat);
    if (!rhiCreate(m_wfSampler, "waterfall sampler", &m_gpuInitFailure)) { return; }

    m_wfSrb = r->newShaderResourceBindings();
    m_wfSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage, m_wfUbo),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_wfGpuTex, m_wfSampler),
    });
    if (!rhiCreate(m_wfSrb, "waterfall shader bindings", &m_gpuInitFailure)) { return; }

    QShader vs = loadShader(QStringLiteral(":/shaders/resources/shaders/waterfall.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/resources/shaders/waterfall.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) {
        // Reached when the .qsb is absent from the binary, or present
        // but with no variant for the running backend. The second case
        // is the nastier one: the resource loads, the file is valid, and
        // there is simply no Metal shader inside it.
        if (m_gpuInitFailure.isEmpty()) {
            m_gpuInitFailure = QStringLiteral("waterfall shaders");
        }
        return;
    }

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
    if (!rhiCreate(m_wfPipeline, "waterfall pipeline", &m_gpuInitFailure)) { return; }
}

void SpectrumWidget::initOverlayPipeline()
{
    QRhi* r = rhi();

    m_ovVbo = r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kQuadData));
    if (!rhiCreate(m_ovVbo, "overlay vertex buffer", &m_gpuInitFailure)) { return; }

    int w = qMax(width(), 64);
    int h = qMax(height(), 64);
    const qreal dpr = devicePixelRatioF();
    const int pw = static_cast<int>(w * dpr);
    const int ph = static_cast<int>(h * dpr);
    m_ovGpuTex = r->newTexture(QRhiTexture::RGBA8, QSize(pw, ph));
    if (!rhiCreate(m_ovGpuTex, "overlay texture", &m_gpuInitFailure)) { return; }

    m_ovSampler = r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                 QRhiSampler::None,
                                 QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!rhiCreate(m_ovSampler, "overlay sampler", &m_gpuInitFailure)) { return; }

    m_ovSrb = r->newShaderResourceBindings();
    m_ovSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_ovGpuTex, m_ovSampler),
    });
    if (!rhiCreate(m_ovSrb, "overlay shader bindings", &m_gpuInitFailure)) { return; }

    // ---- 3D stacked-trace texture + SRB ----
    // Reuses this same overlay pipeline/VBO/sampler -- see DssRenderer.h.
    // Sized lazily to specRect in renderGpuFrame() once that rect is known
    // (it is a sub-region of the widget, not the full window like the
    // overlay textures above), so create it 1x1 here and let the first
    // 3D-mode frame resize it.
    m_dss3dGpuTex = r->newTexture(QRhiTexture::RGBA8, QSize(1, 1));
    if (!rhiCreate(m_dss3dGpuTex, "3D spectrum texture", &m_gpuInitFailure)) { return; }
    m_dss3dGpuTexSize = QSize(1, 1);

    m_dss3dSrb = r->newShaderResourceBindings();
    m_dss3dSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_dss3dGpuTex, m_ovSampler),
    });
    if (!rhiCreate(m_dss3dSrb, "3D spectrum shader bindings", &m_gpuInitFailure)) { return; }

    QShader vs = loadShader(QStringLiteral(":/shaders/resources/shaders/overlay.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/resources/shaders/overlay.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) {
        if (m_gpuInitFailure.isEmpty()) {
            m_gpuInitFailure = QStringLiteral("overlay shaders");
        }
        return;
    }
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
    //
    // Die Ueberlagerungsbilder sind Format_RGBA8888_Premultiplied und
    // gehen roh auf die GPU: die Farbe ist also schon mit Alpha
    // multipliziert. Mit srcColor = SrcAlpha wurde ein zweites Mal
    // multipliziert -- eine Filterflaeche mit Alpha 0,3 kam mit 0,09 an,
    // und die Kantenpixel jeder Schrift wurden dunkler als QPainter sie
    // meinte. Der CPU-Pfad zeichnete dieselben Farben richtig. Fuer
    // vormultiplizierte Quellen gilt One / OneMinusSrcAlpha (2026-09-25).
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    m_ovPipeline->setTargetBlends({blend});
    if (!rhiCreate(m_ovPipeline, "overlay pipeline", &m_gpuInitFailure)) { return; }

    m_overlayStatic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
    m_overlayStatic.setDevicePixelRatio(dpr);
    // NOT page-locked, deliberately.  Until 2026-08-16 this buffer (and
    // m_overlayDynamic and m_waterfall) went through lockMemory() to keep
    // the per-paint fill + GPU upload off a compressed page under memory
    // pressure.  The reasoning was sound for the audio ring and wrong here:
    // a page fault on a display buffer costs one stuttered frame, while the
    // pin cost a full mlock/munlock of ~12 MB on every intermediate window
    // size during a resize drag.  Page locks stay on the real-time audio
    // path, where a fault is an audible dropout.  See MemoryLock.h.

    // 2026-05-26 KG4VCF dual-layer split: dynamic overlay texture.
    // Same dimensions, format, sampler, pipeline as the static
    // layer -- only the texture handle + SRB binding differ.  This
    // is the layer that carries peak-hold trace / peak blobs /
    // noise-floor overlays.  Chrome stays in the static layer.
    m_ovDynGpuTex = r->newTexture(QRhiTexture::RGBA8, QSize(pw, ph));
    if (!rhiCreate(m_ovDynGpuTex, "dynamic overlay texture", &m_gpuInitFailure)) { return; }
    m_ovDynSrb = r->newShaderResourceBindings();
    m_ovDynSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1,
            QRhiShaderResourceBinding::FragmentStage,
            m_ovDynGpuTex, m_ovSampler),
    });
    if (!rhiCreate(m_ovDynSrb, "dynamic overlay shader bindings", &m_gpuInitFailure)) { return; }
    m_overlayDynamic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
    m_overlayDynamic.setDevicePixelRatio(dpr);
    // QImage's buffer is uninitialized, and the dynamic quad is composited
    // across the WHOLE widget while the rebuild below only clears and
    // uploads the spectrum-height band.  The waterfall region therefore
    // sampled whatever the allocator handed us and could show opaque
    // garbage, most visibly right after init or a resize.  Codex review,
    // PR #291.
    m_overlayDynamic.fill(Qt::transparent);

    // ── Die Zusage gilt ab HIER, nicht erst ab dem naechsten
    //    Groessenwechsel ─────────────────────────────────────────────
    //
    // Die Teilaktualisierung weiter unten schreibt nur das Spektrumband
    // und verlaesst sich darauf, dass der Wasserfallteil dieser Textur
    // EINMAL beschrieben wurde. Gesetzt wurde diese Fahne bisher nur im
    // Groessen-Zweig (11.08.2026, „the magenta waterfall, cause number
    // six"). Eine frisch ANGELEGTE Textur kam darin nicht vor — und
    // seit der Panadapter umziehen kann, ist genau das der Normalfall:
    // resetGpuResources() gibt alles frei, initialize() legt neu an,
    // und der Wasserfallteil blieb undefinierter Metal-Speicher.
    // Deckendes Magenta, ueber einen voellig gesunden Wasserfall
    // geblendet.
    //
    // Der Betreiber hat es wiedererkannt: "wir hatten diese problem
    // schon einmal. schaue ganz am anfang. ob da nicht ein schicht
    // darüber gelegen ist." Genau die Schicht, genau dieselbe Ursache,
    // ein anderer Weg hinein.
    m_ovDynNeedsFullUpload = true;
    m_overlayDynamicDirty  = true;
}

void SpectrumWidget::initSpectrumPipeline()
{
    QRhi* r = rhi();

    // 2026-08-26: the trace line and peak-hold line are now drawn as
    // ribbons (2 vertices per point, TriangleStrip) instead of a native
    // LineStrip, same "2N vertices" shape the fill VBO already uses below
    // -- see the topology change on m_fftLinePipeline further down for
    // why. QRhi's LineStrip topology has no width control at all on this
    // project's actual GPU backends (Metal has none; the "Line Width"
    // Setup slider previously only reached the QPainter fallback path,
    // never the GPU one -- see m_lineWidth's other call sites).
    m_fftLineVbo = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                 kMaxFftBins * 2 * kFftVertStride * kLineStrips * sizeof(float));
    if (!rhiCreate(m_fftLineVbo, "spectrum line vertex buffer", &m_gpuInitFailure)) { return; }

    m_fftFillVbo = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                 kMaxFftBins * 2 * kFftVertStride * sizeof(float));
    if (!rhiCreate(m_fftFillVbo, "spectrum fill vertex buffer", &m_gpuInitFailure)) { return; }

    // Phase 3G-8 commit 10: peak hold VBO (same layout as line VBO --
    // now the ribbon layout, same reason as m_fftLineVbo above).
    m_fftPeakVbo = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                 kMaxFftBins * 2 * kFftVertStride * kLineStrips * sizeof(float));
    if (!rhiCreate(m_fftPeakVbo, "spectrum peak vertex buffer", &m_gpuInitFailure)) { return; }

    m_fftSrb = r->newShaderResourceBindings();
    m_fftSrb->setBindings({});
    if (!rhiCreate(m_fftSrb, "spectrum shader bindings", &m_gpuInitFailure)) { return; }

    QShader vs = loadShader(QStringLiteral(":/shaders/resources/shaders/spectrum.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/resources/shaders/spectrum.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) {
        if (m_gpuInitFailure.isEmpty()) {
            m_gpuInitFailure = QStringLiteral("spectrum shaders");
        }
        return;
    }
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
    if (!rhiCreate(m_fftFillPipeline, "spectrum fill pipeline", &m_gpuInitFailure)) { return; }

    // Line pipeline (triangle strip -- ribbon, not a native LineStrip; see
    // m_fftLineVbo's comment above for why. Reuses the exact same shader
    // pair, vertex layout, and blend state as the fill pipeline above --
    // both are now "2 vertices per point" ribbons, just built from
    // different per-frame geometry (see renderGpuFrame()'s vertex-gen
    // block: perpendicular-offset ribbon here vs. down-to-baseline strip
    // for fill).
    m_fftLinePipeline = r->newGraphicsPipeline();
    m_fftLinePipeline->setShaderStages({{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
    m_fftLinePipeline->setVertexInputLayout(layout);
    m_fftLinePipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_fftLinePipeline->setShaderResourceBindings(m_fftSrb);
    m_fftLinePipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_fftLinePipeline->setTargetBlends({blend});
    if (!rhiCreate(m_fftLinePipeline, "spectrum line pipeline", &m_gpuInitFailure)) { return; }
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

    // Say it once, and say what it was.
    //
    // A partial GPU setup does not fail visibly — it draws. It draws
    // whatever is in the memory it was pointed at, and the operator gets
    // a colour instead of an error. Anyone who has to debug this next
    // should be able to start from a log line naming the object, the
    // backend and the Qt build, rather than from a screenshot.
    if (!m_gpuInitFailure.isEmpty()) {
        qCritical().noquote()
            << "SpectrumWidget: GPU spectrum setup FAILED at"
            << m_gpuInitFailure
            << "\n  backend:" << r->backendName()
            << "\n  Qt:" << QLibraryInfo::version().toString()
            << "\n  The spectrum and waterfall will not render correctly."
            << "\n  A solid block of colour here is undefined GPU memory,"
            << "\n  not a palette fault — do not go looking at the palette.";
    }

    // Upload quad VBO data
    batch->uploadStaticBuffer(m_wfVbo, kQuadData);
    batch->uploadStaticBuffer(m_ovVbo, kQuadData);

    // Initial full waterfall texture upload.
    //
    // Unconditional, and at the texture's own size rather than the
    // image's. This is the magenta waterfall, reported three times and
    // wrongly blamed on the palette twice.
    //
    // A newly created QRhiTexture has undefined contents. The old code
    // uploaded m_waterfall only when it existed, and only at m_waterfall's
    // size — but at initialize() time the texture is sized from width(),
    // while the image is sized from width() minus the dBm strip, and the
    // image may not exist at all yet. Any of those leaves part or all of
    // the texture never written, and the shader then samples whatever
    // that GPU memory happened to hold. On Metal that is very often
    // magenta, which is exactly what the bench saw: a solid magenta
    // waterfall that no palette in this file can produce, because none
    // of them has a magenta stop any more.
    //
    // The two earlier fixes were both real — the collapsed dB window and
    // the palette hue reversal — and neither was this. That is worth
    // recording: the same symptom had three independent causes, and
    // fixing two of them made the third look like a regression.
    {
        QImage seed(m_wfGpuTexW, m_wfGpuTexH, QImage::Format_RGBA8888);
        // same as m_waterfall's fill — the two must not drift apart
        seed.fill(roleColor("app-bg", Style::kAppBg));
        if (!m_waterfall.isNull()) {
            QPainter sp(&seed);
            sp.drawImage(0, 0, m_waterfall);
        }
        // The seed is the ONLY thing standing between an operator with no
        // radio connected and a screenful of undefined GPU memory. With
        // nothing streaming, m_waterfall is never fed, the incremental
        // path never runs, and every pixel on screen comes from this one
        // upload. If the QImage is null — because a dimension came out
        // zero, or the allocation failed — uploadTexture silently does
        // nothing and the texture keeps whatever Metal left in it.
        //
        // Reported from the bench: magenta with the radio disconnected.
        // That is exactly this case, so this is the line that has to be
        // provably right rather than probably right.
        if (seed.isNull()) {
            qCritical() << "SpectrumWidget: waterfall seed image is NULL at"
                        << m_wfGpuTexW << "x" << m_wfGpuTexH
                        << "— the texture will never be written and will"
                        << "show undefined GPU memory.";
        } else if (qEnvironmentVariableIntValue("LONGPATH_WF_DEBUG") > 0) {
            qDebug() << "SpectrumWidget: waterfall seeded"
                     << seed.width() << "x" << seed.height()
                     << "fmt" << int(seed.format())
                     << "| tex" << m_wfGpuTexW << "x" << m_wfGpuTexH
                     << "| widget" << width() << "x" << height()
                     << "dpr" << devicePixelRatioF()
                     << "| m_waterfall null=" << m_waterfall.isNull();
        }
        QRhiTextureSubresourceUploadDescription desc(seed);
        batch->uploadTexture(m_wfGpuTex, QRhiTextureUploadEntry(0, 0, desc));
    }

    cb->resourceUpdate(batch);
    m_wfTexFullUpload = false;
    m_wfLastUploadedRow = m_wfWriteRow;
    m_wfRowsSinceUpload = 0;
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

        // Bilder je Sekunde HIER zaehlen, wo wirklich ein Bild entsteht.
        // Bis 2026-09-25 zaehlte der Zaehler in der statischen
        // Ueberlagerung mit -- die wird nur bei Aenderungen neu gebaut,
        // also zeigte er "Neuaufbauten je Sekunde" (im Leerlauf fast 0).
        // Die Zahl wird weiter dort gezeichnet; neu gebaut wird die
        // Ueberlagerung dafuer einmal je Sekunde.
        if (m_showFps) {
            ++m_fpsFrameCount;
            if (m_fpsLastUpdateMs == 0) {
                m_fpsLastUpdateMs = nowMs;
            } else if (nowMs - m_fpsLastUpdateMs >= 1000) {
                const double elapsed = (nowMs - m_fpsLastUpdateMs) / 1000.0;
                m_fpsDisplayValue = static_cast<float>(m_fpsFrameCount / elapsed);
                m_fpsFrameCount   = 0;
                m_fpsLastUpdateMs = nowMs;
                m_overlayStaticDirty = true;
            }
        }
    }

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

    // ── One measurement that halves the search space ─────────────────
    //
    // The magenta waterfall has now had five theories killed against the
    // source: missing shaders, an LTO target that never had them, an
    // RGBA/BGRA swap, a palette with a magenta stop, and custom stops
    // loaded from settings. None of them survives reading the code, and
    // the colour mapping cannot emit magenta — NaN is guarded, the
    // normalised value is qBound to 0..1, and no palette ends there.
    //
    // So either m_waterfall itself contains magenta, in which case the
    // fault is on the CPU side and above, or it does not, in which case
    // the pixels on screen never came from it and the fault is between
    // the upload and the sampler. One pixel value decides which half to
    // look in, and no amount of further reading will.
    //
    // Off unless asked for: LONGPATH_WF_DEBUG=1. A diagnostic that costs a
    // qDebug per frame in normal use is a diagnostic that gets deleted.
    static const int kWfDebugLevel =
        qEnvironmentVariableIntValue("LONGPATH_WF_DEBUG");
    static const bool kWfDebug = kWfDebugLevel > 0;

    // Level 2: the one-shot discriminator. Fill the CPU image solid
    // green and force a full upload EVERY frame. A green screen proves
    // the entire upload→texture→sampler→quad chain end to end, which
    // pins the fault in the image's CONTENT (and convicts the
    // single-pixel probe below of under-sampling); a screen that stays
    // magenta convicts the chain. One run with LONGPATH_WF_DEBUG=2
    // answers what four theories could not.
    if (kWfDebugLevel >= 2 && !m_waterfall.isNull()) {
        m_waterfall.fill(QColor(Style::kGreenText));
        m_wfTexFullUpload = true;
    }
    if (kWfDebug) {
        static int frame = 0;
        if (frame < 3 || frame % 120 == 0) {
            QString px = QStringLiteral("(no image)");
            if (!m_waterfall.isNull()) {
                const int sy = qBound(0, m_wfWriteRow,
                                      m_waterfall.height() - 1);
                const QRgb c0 = m_waterfall.pixel(m_waterfall.width() / 2, sy);
                px = QStringLiteral("newest row %1 mid-pixel #%2%3%4")
                         .arg(sy)
                         .arg(qRed(c0),   2, 16, QLatin1Char('0'))
                         .arg(qGreen(c0), 2, 16, QLatin1Char('0'))
                         .arg(qBlue(c0),  2, 16, QLatin1Char('0'));
            }
            qDebug().noquote()
                << QStringLiteral("WF[%1] tex %2x%3  img %4x%5 fmt %6 null=%7"
                                  "  writeRow=%8 lastUp=%9  full=%10  %11")
                       .arg(frame)
                       .arg(m_wfGpuTexW).arg(m_wfGpuTexH)
                       .arg(m_waterfall.width()).arg(m_waterfall.height())
                       .arg(int(m_waterfall.format()))
                       .arg(m_waterfall.isNull())
                       .arg(m_wfWriteRow).arg(m_wfLastUploadedRow)
                       .arg(m_wfTexFullUpload)
                       .arg(px);
        }
        ++frame;
    }

    // ---- Waterfall texture upload (incremental) ----
    if (!m_waterfall.isNull()) {
        if (m_waterfall.width() != m_wfGpuTexW || m_waterfall.height() != m_wfGpuTexH) {
            m_wfGpuTexW = m_waterfall.width();
            m_wfGpuTexH = m_waterfall.height();
            // FRESH OBJECTS, not an in-place recreate (2026-08-11).
            //
            // The sixth and final theory, and the one the evidence
            // finally allows: with LONGPATH_WF_DEBUG=2 the CPU image was
            // force-filled solid green and full-uploaded EVERY frame —
            // recorded into the same submitted batch whose other
            // resources demonstrably work — and the screen stayed
            // magenta. The only remaining explanation is that the draw
            // samples a texture other than the one being uploaded: the
            // original init-time 483x64 texture, never written, whose
            // undefined Metal memory is exactly that magenta. The old
            // code resized IN PLACE (setPixelSize + create on the same
            // objects) and rebuilt the same QRhiShaderResourceBindings
            // instance; on this bench's Qt 6.11.1 Metal backend the
            // draw kept using the pre-rebuild binding. A brand-new
            // texture and a brand-new SRB object cannot alias any
            // cached state — the pipeline only requires layout
            // compatibility, which is unchanged. QRhi defers the
            // native releases until the frame completes, so deleting
            // the old objects here is safe.
            QRhi* rr = rhi();
            QRhiTexture* newTex = rr->newTexture(
                QRhiTexture::RGBA8, QSize(m_wfGpuTexW, m_wfGpuTexH));
            const bool texOk = newTex && newTex->create();
            QRhiShaderResourceBindings* newSrb =
                texOk ? rr->newShaderResourceBindings() : nullptr;
            bool srbOk = false;
            if (newSrb) {
                newSrb->setBindings({
                    QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage, m_wfUbo),
                    QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, newTex, m_wfSampler),
                });
                srbOk = newSrb->create();
            }
            if (!texOk || !srbOk) {
                static qint64 lastResizeFailLog = 0;
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                if (now - lastResizeFailLog > 2000) {
                    lastResizeFailLog = now;
                    qCritical() << "SpectrumWidget: waterfall texture resize"
                                << m_wfGpuTexW << "x" << m_wfGpuTexH
                                << "FAILED (tex" << texOk << "srb" << srbOk
                                << ") — keeping the previous texture; will"
                                << "retry next frame.";
                }
                delete newSrb;
                delete newTex;
                // Old objects stay live and consistent; retry via the
                // size mismatch next frame.
                m_wfGpuTexW = -1;
                m_wfGpuTexH = -1;
            } else {
                delete m_wfSrb;
                delete m_wfGpuTex;
                m_wfGpuTex = newTex;
                m_wfSrb    = newSrb;
                if (kWfDebug) {
                    qDebug().noquote() << QStringLiteral(
                        "WF resize: fresh texture+srb %1x%2")
                        .arg(m_wfGpuTex->pixelSize().width())
                        .arg(m_wfGpuTex->pixelSize().height());
                }
                m_wfTexFullUpload = true;
            }
        }

        // ── Nach einer Pause: ganz hochladen ──────────────────────────
        //
        // Der Teil-Upload unten laeuft von m_wfLastUploadedRow rueckwaerts
        // bis m_wfWriteRow. Wurde zwischen zwei Bildern mehr als eine
        // Texturhoehe geschrieben (Fenster versteckt oder nicht
        // gezeichnet, bei 101 ms je Zeile nach gut einer halben Minute),
        // ist der Ring umgelaufen: der Abstand der beiden Zeiger nennt
        // nur noch den Rest, der Rest der Textur blieb alt -- ein
        // Wasserfall aus neuen Zeilen oben und Zeilen von VOR der Pause
        // darunter, in falscher Zeitfolge. Gefunden 2026-09-26 im
        // Pruefstand (LONGPATH_RENDER_HIDDEN=2): 400 Zeilen geschrieben,
        // 72 hochgeladen. m_waterfall hat dann ohnehin lauter neue
        // Zeilen, also einmal ganz.
        if (m_wfRowsSinceUpload >= m_wfGpuTexH) {
            m_wfTexFullUpload = true;
        }

        if (m_wfTexFullUpload) {
            // Sizes agree by construction at this point, but the
            // consequence of them ever not agreeing is a texture with
            // undefined regions — see the note in initialize(). Assert
            // it in the cheapest way available: if they disagree, fill
            // first and draw the image into it.
            QImage rgba;
            if (m_waterfall.width() == m_wfGpuTexW
                && m_waterfall.height() == m_wfGpuTexH) {
                rgba = m_waterfall.convertToFormat(QImage::Format_RGBA8888);
            } else {
                rgba = QImage(m_wfGpuTexW, m_wfGpuTexH,
                              QImage::Format_RGBA8888);
                rgba.fill(roleColor("app-bg", Style::kAppBg));
                QPainter sp(&rgba);
                sp.drawImage(0, 0, m_waterfall);
            }
            if (kWfDebug) {
                qDebug().noquote() << QStringLiteral(
                    "WF VOLLUPLOAD %1x%2 -> tex %3x%4 (null=%5)")
                    .arg(rgba.width()).arg(rgba.height())
                    .arg(m_wfGpuTexW).arg(m_wfGpuTexH)
                    .arg(rgba.isNull());
            }
            batch->uploadTexture(m_wfGpuTex, QRhiTextureUploadEntry(0, 0,
                QRhiTextureSubresourceUploadDescription(rgba)));
            m_wfLastUploadedRow = m_wfWriteRow;
            m_wfRowsSinceUpload = 0;
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
            m_wfRowsSinceUpload = 0;
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
    // Zweiter Wert: die Hoehe einer Texturzeile in UV. Der Shader klemmt
    // damit die Unterkante, damit dort nicht die neueste Zeile durch den
    // Ringumbruch hineinmischt (waterfall.frag, 2026-09-25).
    const float texelH = (m_wfGpuTexH > 0) ? 1.0f / static_cast<float>(m_wfGpuTexH) : 0.0f;
    float uniforms[] = {rowOffset, texelH, 0.0f, 0.0f};
    batch->updateDynamicBuffer(m_wfUbo, 0, sizeof(uniforms), uniforms);

    // ---- Overlay texture (static, only on state change) ----
    {
        const qreal dpr = devicePixelRatioF();
        const int pw = static_cast<int>(w * dpr);
        const int ph = static_cast<int>(h * dpr);
        if (m_overlayStatic.size() != QSize(pw, ph) && !resizeSettling()) {
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

        // 2026-05-26 KG4VCF perf instrumentation: time the overlay-
        // rebuild block (QImage fill + ~16 paint ops + GPU texture
        // upload) so the perf overlay can show avg/max cost and
        // confirm whether this is the dominant per-paint expense.
        QElapsedTimer ovlyTimer;
        // Suppressed while a resize drag is in flight.  Every draw call
        // below takes its geometry from the CURRENT widget size, while the
        // image still carries the pre-drag dimensions until the size
        // settles — so rebuilding here would paint new-size chrome into an
        // old-size buffer and clip it.  The dirty flag is left set, and the
        // GPU stretches the last complete overlay until applyResizeSettled()
        // re-allocates and this runs once.
        const bool needsOverlayRebuild = m_overlayStaticDirty && !resizeSettling();
        if (needsOverlayRebuild) {
            ovlyTimer.start();
        }
        if (needsOverlayRebuild) {
            m_overlayStatic.fill(Qt::transparent);
            QPainter p(&m_overlayStatic);
            p.setRenderHint(QPainter::Antialiasing, false);

            // Ganz unten, unter dem Raster: AetherSDR begruendet das bei
            // SpectrumWidget.cpp:13748 [@0cd4559] — die Rasterzellen
            // muessen HINTER den Signalspitzen liegen, nicht darueber.
            paintBackgroundLayer(p, specRect);
            drawGrid(p, specRect);
            drawSquelchLine(p, specRect);
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
            // Und auch hier -- genau der Fehler, vor dem der Block
            // darunter warnt: der QPainter-Weg oben ist die
            // Rueckfallebene, dieser hier malt im Betrieb.
            drawCompanionMarker(p, specRect);
            drawBandStackMarks(p, specRect);

            // ── Auch HIER, nicht nur im CPU-Weg ──────────────────────
            //
            // Die Einblendungen (Kompass, Stehwelle) standen zuerst nur
            // im QPainter-Weg oben bei drawBandPlan. Der laeuft aber
            // nur als Rueckfallebene ohne GPU — im normalen Betrieb
            // baut dieser Block die Ueberlagerungsflaeche, und dort
            // fehlten sie.
            //
            // Der Betreiber am 2026-08-21: „einblenden kann ich noch
            // immer nichts im pandapter". Die Bilder waren richtig und
            // durchsichtig (nachgemessen: Alpha 0 am Rand, 2-6 %
            // Tinte) — sie wurden nur nie in DIESE Flaeche gemalt.
            //
            // Zwei Zeichenwege sind zwei Orte, an denen etwas fehlen
            // kann. Wer den einen aendert, muss den anderen mitnehmen.

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
            // TNF notch overlay, GPU static-overlay path.  Same relative
            // ordering as the CPU paintEvent above and as AetherSDR
            // src/gui/SpectrumWidget.cpp:12013-12016 [@c6481cbf], where
            // drawTnfMarkers likewise precedes drawSpotMarkers in the
            // frequency-plane painter.  Missing THIS call site while
            // having the CPU one is a silent GPU-only regression, since
            // LONGPATH_GPU_SPECTRUM is the shipping path.
            drawNotchMarkers(p, notchSpecRect());
            // Phase 3J-2 Task E1: spot overlay before VFO marker so labels
            // sit below the slice marker chrome. Mirrors the CPU paintEvent
            // ordering and AetherSDR SpectrumWidget.cpp:3787 [@0cd4559]
            // (drawSpotMarkers between trace and drawSliceMarkers).
            if (m_showSpots || m_showSignalHistory || m_showSignalHistoryQrm) {
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
            // counter in paintEvent). Gezaehlt wird in renderGpuFrame();
            // hier steht nur die Zahl, die Ueberlagerung wird dafuer
            // einmal je Sekunde neu gebaut.
            if (m_showFps) {
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
            // Abstimmhilfe im GPU-Malweg, gleiche Reihenfolge wie oben.
            drawTuneGuide(p, specRect);

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
                      << QStringLiteral("ring   under %1 (+%2/s)")
                            .arg(stats.audioRingUnderrunsTotal)
                            .arg(stats.audioRingUnderrunsDelta)
                      << QStringLiteral("ring   over  %1 (+%2/s)")
                            .arg(stats.audioRingOverrunsTotal)
                            .arg(stats.audioRingOverrunsDelta)
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
                QFont pf = p.font();
                pf.setPixelSize(11);
                Longpath::Style::setzeMonoFamilien(pf);
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
                          // 2026-09-04: Ringpuffer-Ereignisse faerben
                          // ebenfalls rot. Ein verworfenes oder
                          // ueberblendetes Paket ist genauso hoerbar wie
                          // ein Aussetzer des Geraets — es zaehlte bisher
                          // nur an einer anderen Stelle.
                          || stats.audioRingUnderrunsDelta > 0
                          || stats.audioRingOverrunsDelta > 0
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
                const QColor border = red   ? QColor(Style::kTxRed)
                                    : amber ? QColor(Style::kAmberText)
                                            : QColor(Style::kGreenText);
                p.setPen(QPen(border, 1));
                p.setBrush(bg);
                p.drawRect(boxX, boxY, boxW, boxH);
                p.setPen(QColor(Style::kRedText));
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
        if (m_overlayDynamic.size() != QSize(pw, ph) && !resizeSettling()) {
            m_overlayDynamic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
            m_overlayDynamic.setDevicePixelRatio(dpr);
            // Same uninitialized-buffer hazard as the init path above: the
            // rebuild below only touches the spectrum-height band, while the
            // quad samples the full texture.  Codex review, PR #291.
            m_overlayDynamic.fill(Qt::transparent);
            m_ovDynGpuTex->setPixelSize(QSize(pw, ph));
            m_ovDynGpuTex->create();
            m_ovDynSrb->setBindings({
                QRhiShaderResourceBinding::sampledTexture(1,
                    QRhiShaderResourceBinding::FragmentStage,
                    m_ovDynGpuTex, m_ovSampler),
            });
            m_ovDynSrb->create();
            m_overlayDynamicDirty = true;
            // THE MAGENTA WATERFALL, cause number six — the real one
            // (2026-08-11, found by force-filling the waterfall image
            // green and watching the screen stay pink). The comment
            // above names the uninitialized-buffer hazard and the
            // partial-upload path below assumes "the waterfall portion
            // of the texture stays transparent from the first
            // full-window init" — but THIS branch just re-CREATED the
            // texture, and a recreated texture has no init upload: its
            // waterfall region is undefined Metal memory (magenta on
            // this bench), alpha-blended OVER the perfectly healthy
            // waterfall quad every frame. One full upload after every
            // recreate restores the invariant the partial path relies
            // on.
            m_ovDynNeedsFullUpload = true;
        }

        // Same resize suppression as the static layer above, and for the
        // same reason: the partial-region rebuild below clears and uploads
        // a spectrum-height band computed from the current widget size.
        if (m_overlayDynamicDirty && !resizeSettling()) {
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
                if (m_activePeakHold.enabled() && m_activePeakHold.size() > 0) {
                    paintActivePeakHoldTrace(pd, specRect);
                }
                if (m_peakBlobs.enabled() && !m_peakBlobs.blobs().isEmpty()) {
                    paintPeakBlobs(pd, specRect);
                }
            }
            paintNoiseFloorOverlay(pd, specRect);
            // Durchlass und Bandbreite (2026-11-08): Klammern, Luecken, Lupe.
            pd.setRenderHint(QPainter::Antialiasing, false);
            paintSignalKlammern(pd, specRect);
            paintLueckenfinder(pd, specRect);
            paintDurchlassLupe(pd, specRect);
            pd.setRenderHint(QPainter::Antialiasing, false);
            // Im 3D-Modus die Bandbreite UEBER die Flaeche — siehe
            // drawSliceMarker fuer den Grund.
            paintPassbandOverSurface(pd, specRect);

            // Reuse PerfMonitor's ovly metric for the *dynamic* rebuild
            // cost since that's now the per-frame variable; chrome
            // rebuilds are rare and their cost amortises out of the
            // 1 s perf window.
            PerfMonitor::instance().recordOverlayRebuild(
                static_cast<double>(dynTimer.nsecsElapsed()) / 1e6);

            if (m_ovDynNeedsFullUpload) {
                // First upload after a texture (re)create: the WHOLE
                // image, so the waterfall region is really transparent
                // instead of undefined GPU memory. See the note at the
                // recreate site — this is the magenta-waterfall fix.
                QRhiTextureSubresourceUploadDescription desc(m_overlayDynamic);
                batch->uploadTexture(m_ovDynGpuTex,
                                     QRhiTextureUploadEntry(0, 0, desc));
                m_ovDynNeedsFullUpload = false;
            } else {
                // Partial-region upload.  setSourceTopLeft / setSourceSize
                // tell QRhi to copy only the spectrum portion of the QImage
                // into the matching region of the texture; the rest of the
                // texture is untouched.  Cuts Metal command-buffer payload
                // by 30-50% depending on spectrum / waterfall split.
                // Safe ONLY because the full upload above re-established
                // the transparent waterfall region after every recreate.
                QRhiTextureSubresourceUploadDescription desc(m_overlayDynamic);
                desc.setSourceTopLeft(QPoint(0, 0));
                desc.setSourceSize(dynRectDevPx.size());
                desc.setDestinationTopLeft(QPoint(0, 0));
                batch->uploadTexture(m_ovDynGpuTex,
                                     QRhiTextureUploadEntry(0, 0, desc));
            }
            m_overlayDynamicDirty = false;
        }
    }

    // ---- FFT spectrum vertices ----
    // GPU vertex generation -- one vertex per display pixel.  Mirrors
    // Thetis Display.cs:5249-5378 [v2.10.3.13] per-pixel render loop
    // (DrawLine over current_display_data[i], i = 0..nDecimatedWidth-1).
    // Honours:
    //   - m_dbmCalOffset (shifts every pixel's dBm before y mapping)
    //   - m_heatmapEnabled (aus = flache Kurvenfarbe, an = Regenbogen;
    //     bis 2026-08-15 haftete das an m_gradientEnabled, das auf dem
    //     CPU-Pfad etwas anderes bedeutet — siehe Header)
    //   - m_panFill (skips fill VBO update when disabled)
    //   - m_fillColor / m_fillAlpha (used for the flat-fill path)
    //   - m_peakHoldEnabled (generates a second line VBO for peak hold)
    //
    // outputSize/dpr hoisted up from just before cb->beginPass() (their
    // original spot, still used there for the viewport math) so the
    // trace-line ribbon geometry below can convert its device-pixel
    // half-width to NDC using the EXACT same dpr the viewport itself is
    // built from -- one source of truth, not two independently-computed
    // values that could quietly drift apart.
    const QSize outputSize = renderTarget()->pixelSize();
    const float dpr = outputSize.width() / static_cast<float>(qMax(1, w));

    if (!m_renderedPixels.isEmpty() && m_fftLineVbo && m_fftFillVbo) {
        const int n = qMin(m_renderedPixels.size(), kMaxFftBins);
        m_visibleBinCount = n;
        const float minDbm = m_refLevel - m_dynamicRange;
        const float range  = m_dynamicRange;
        const float yBot = -1.0f;
        const float yTop = 1.0f;
        // Boden der dBm-Achse: wie dbmToYf die OBERKANTE des Bandplan-
        // Streifens, nicht der Rand des Bereichs. Bis 2026-09-25 rechnete
        // die GPU-Kurve ueber die volle Hoehe, Gitter und Ueberlagerungen
        // ohne den Streifen -- mit Bandplan lag die Kurve am Rauschboden
        // um fast die Streifenhoehe zu tief (tst_spectrum_trace_on_grid).
        // Die Fuellung reicht weiter bis yBot, wie im CPU-Pfad bis
        // specRect.bottom().
        const float stripFrac = (specRect.height() > 0)
            ? static_cast<float>(bandPlanStripHeight()) / static_cast<float>(specRect.height())
            : 0.0f;
        const float yFloor = yBot + 2.0f * qBound(0.0f, stripFrac, 1.0f);

        const float fa = m_fillAlpha;
        // Kalibrierung UND 1-Hz-Normierung, wie dbmToYf -- die Normierung
        // fehlte hier, Gitter und Ueberlagerungen hatten sie schon.
        const float cal = m_dbmCalOffset + normalizeShiftDb();

        // Flat-mode colour picked from m_fillColor.
        const float flatR = m_fillColor.redF();
        const float flatG = m_fillColor.greenF();
        const float flatB = m_fillColor.blueF();

        // ── Trace-line ribbon geometry, 2026-08-26 ──────────────────────
        //
        // The live trace and peak-hold line used to be one vertex per
        // point, drawn with QRhiGraphicsPipeline::LineStrip. That
        // topology has no width control on this project's actual GPU
        // backends (Metal in particular has none at all), so the
        // "Line Width" Setup slider (m_lineWidth) only ever reached the
        // QPainter fallback path, never the GPU one every user actually
        // sees by default -- confirmed against the code, not assumed.
        //
        // Fix: build the line as a ribbon -- 2 vertices per point,
        // offset perpendicular to the local trace direction by a
        // device-pixel half-width, drawn as a TriangleStrip -- the exact
        // same "2N vertices, alternating edges" shape the fill strip
        // below already uses and has shipped correctly for a long time.
        // No new shader: both edges of a point share ORDINARY Gouraud-
        // interpolated colour, so this needed no fragment-shader changes
        // at all.
        //
        // Deliberately NOT attempting edge feathering/antialiasing in
        // this pass (that would need genuinely new geometry/shader work,
        // e.g. AetherSDR's panscope.frag signed-distance approach) --
        // a solid, correctly-widthed ribbon already fixes the confirmed
        // bug (the slider doing nothing); soft edges are a follow-up,
        // not bundled in here blind.
        //
        // vpWpx/vpHpx are the viewport this ribbon is actually
        // rasterized into (specVp, further down) in DEVICE pixels --
        // using the same dpr the viewport itself is built from (hoisted
        // above, see its own comment) keeps this geometry and the
        // viewport it's drawn into always in agreement.
        const float vpWpx = static_cast<float>(specRect.width())  * dpr;
        const float vpHpx = static_cast<float>(specRect.height()) * dpr;

        // m_lineWidth is logical px -- same units the QPainter fallback's
        // QPen(m_fillColor, m_lineWidth) already uses (drawSpectrum()) --
        // so scale by dpr to get a device-pixel width, then halve for a
        // symmetric ribbon (offset applied on both sides of the centre).
        const float lineHalfWidthPx = (m_lineWidth * dpr) * 0.5f;

        // ── Weiche Kante und Hof ───────────────────────────────────────
        //
        // Geometrie steht in `gui/KurvenStreifen.h`, damit sie ohne
        // Fenster und ohne GPU geprueft werden kann. Mit beiden
        // Schaltern aus kommt hier GENAU EIN Streifen heraus, Wert fuer
        // Wert wie vor diesem Umbau.
        Kurve::Streifen streifen[Kurve::kStreifenHoechstens];
        const int nStrips = Kurve::baue(m_traceSoftEdge, m_traceHalo,
                                        lineHalfWidthPx, dpr,
                                        kTraceAlpha, streifen,
                                        m_traceEdgePx);
        m_lineStripCount = nStrips;

        QVector<float> lineVerts(static_cast<qsizetype>(n) * 2 * kFftVertStride * nStrips);
        QVector<float> fillVerts(n * 2 * kFftVertStride);
        // Zwei Punkte eines Streifens. `nxPx`/`nyPx` ist die Normale in
        // Geraete-Pixeln (nicht normiert in NDC) und die Teilung durch
        // die halbe Fenstergroesse passiert HIER -- so steht beim
        // Ein-Streifen-Fall dieselbe Rechenfolge wie vorher, und der
        // Puffer wird bitgleich.
        auto putStrip = [&](int strip, int j, float x, float y,
                            float nxPx, float nyPx,
                            const Kurve::Streifen& st,
                            float r, float g, float b) {
            const qsizetype k = (static_cast<qsizetype>(strip) * n + j) * 2 * kFftVertStride;
            lineVerts[k]     = x + (nxPx * st.o1) / (vpWpx * 0.5f);
            lineVerts[k + 1] = y + (nyPx * st.o1) / (vpHpx * 0.5f);
            lineVerts[k + 2] = r;
            lineVerts[k + 3] = g;
            lineVerts[k + 4] = b;
            lineVerts[k + 5] = st.a1;
            lineVerts[k + 6] = x + (nxPx * st.o2) / (vpWpx * 0.5f);
            lineVerts[k + 7] = y + (nyPx * st.o2) / (vpHpx * 0.5f);
            lineVerts[k + 8]  = r;
            lineVerts[k + 9]  = g;
            lineVerts[k + 10] = b;
            lineVerts[k + 11] = st.a2;
        };

        for (int j = 0; j < n; ++j) {
            float x = (n > 1) ? 2.0f * j / (n - 1) - 1.0f : 0.0f;
            float t = qBound(0.0f, ((m_renderedPixels[j] + cal) - minDbm) / range, 1.0f);
            float y = yFloor + t * (yTop - yFloor);

            // Ribbon perpendicular offset: central difference of the
            // neighbouring points for the local tangent (smoother than a
            // one-sided difference; clamped indices degenerate cleanly
            // at both ends of the trace instead of reading out of
            // bounds).
            const int jPrev = qMax(j - 1, 0);
            const int jNext = qMin(j + 1, n - 1);
            const float xPrev = (n > 1) ? 2.0f * jPrev / (n - 1) - 1.0f : 0.0f;
            const float xNext = (n > 1) ? 2.0f * jNext / (n - 1) - 1.0f : 0.0f;
            const float tPrev = qBound(0.0f, ((m_renderedPixels[jPrev] + cal) - minDbm) / range, 1.0f);
            const float tNext = qBound(0.0f, ((m_renderedPixels[jNext] + cal) - minDbm) / range, 1.0f);
            const float yPrev = yFloor + tPrev * (yTop - yFloor);
            const float yNext = yFloor + tNext * (yTop - yFloor);

            const float dxPx = (xNext - xPrev) * (vpWpx * 0.5f);
            const float dyPx = (yNext - yPrev) * (vpHpx * 0.5f);
            const float segLenPx = std::sqrt(dxPx * dxPx + dyPx * dyPx);
            // Degenerate (flat run, or n == 1): fall back to a level
            // ribbon rather than dividing by ~zero.
            const float uxPx = (segLenPx > 1e-6f) ? dxPx / segLenPx : 1.0f;
            const float uyPx = (segLenPx > 1e-6f) ? dyPx / segLenPx : 0.0f;
            // Die Normale ist der um 90 Grad gedrehte Einheitstangens,
            // in Geraete-Pixeln: (-uyPx, uxPx). Auf die halbe Breite
            // skaliert und von Pixeln nach NDC gerechnet wird sie in
            // `putStrip`, mit demselben vpWpx/vpHpx wie der Tangens.

            float cr, cg, cb2;
            if (m_heatmapEnabled) {
                // Heat map: blue → cyan → green → yellow → red
                // From AetherSDR SpectrumWidget.cpp:2298-2310
                // Hing bis 2026-08-15 an m_gradientEnabled, das auf dem
                // CPU-Pfad die Fuellung schaltet — siehe die Notiz im
                // Header. Eigene Fahne, Vorgabe aus.
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

            // Line vertex -- ribbon: two vertices per point (offset +/-
            // perpendicular from centre), same alternating-edge order
            // the fill strip below already uses so both form a valid
            // connected TriangleStrip.  Beide Kanten tragen dieselbe
            // Deckkraft (kTraceAlpha), solange keine weiche Kante
            // eingeschaltet ist -- dann ist es genau ein solides Band
            // wie vorher.
            for (int s = 0; s < nStrips; ++s) {
                putStrip(s, j, x, y, -uyPx, uxPx, streifen[s], cr, cg, cb2);
            }

            // ── Fuellung: dicht an der Kurve, weg zur Grundlinie ─────
            //
            // Sie war umgekehrt herum, und deshalb sah man sie nicht:
            // oben, an der Kurve, stand fa * 0.3 — fast durchsichtig —,
            // unten an der Grundlinie das volle fa, und zwar in einem
            // fest verdrahteten Dunkelblau (0.0, 0.0, 0.3). Auf fast
            // schwarzem Grund liest sich das als Hintergrund, nicht als
            // Fuellung. Es war ausserdem der letzte Blauton, den die
            // Palettenumstellung nicht erwischt hat, weil er nicht als
            // Hexwert dasteht, sondern als drei Fliesskommazahlen.
            //
            // Jetzt docs/design/HAUSSTIL.md §Weiche Uebergaenge:
            // "22 % -> 0 zur Grundlinie", in der Kurvenfarbe. Unten ist
            // die Farbe gleichgueltig, weil dort nichts mehr deckt —
            // trotzdem dieselbe, damit kein Farbstich entsteht, falls
            // jemand die Deckkraft spaeter anhebt.
            int fi = j * 2 * kFftVertStride;
            fillVerts[fi]     = x;
            fillVerts[fi + 1] = y;
            fillVerts[fi + 2] = cr;
            fillVerts[fi + 3] = cg;
            fillVerts[fi + 4] = cb2;
            fillVerts[fi + 5] = fa;
            fillVerts[fi + 6] = x;
            fillVerts[fi + 7] = yBot;
            fillVerts[fi + 8]  = cr;
            fillVerts[fi + 9]  = cg;
            fillVerts[fi + 10] = cb2;
            fillVerts[fi + 11] = 0.0f;
        }

        batch->updateDynamicBuffer(m_fftLineVbo, 0,
            static_cast<size_t>(n) * 2 * kFftVertStride * nStrips * sizeof(float), lineVerts.constData());
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
            // Same ribbon treatment as the live trace above, but
            // narrower -- 0.75x the line width, matching the CPU/
            // QPainter fallback's own convention (drawSpectrum():
            // QPen peakPen(peakCol, qMax(1.0f, m_lineWidth * 0.75f))).
            const float peakHalfWidthPx = (m_lineWidth * 0.75f * dpr) * 0.5f;
            // Weiche Kante ja, Hof nein -- siehe setTraceHalo im Header.
            Kurve::Streifen pStreifen[Kurve::kStreifenHoechstens];
            const int nPeakStrips = Kurve::baue(m_traceSoftEdge, /*hof*/ false,
                                                peakHalfWidthPx, dpr,
                                                kPeakAlpha, pStreifen,
                                                m_traceEdgePx);
            m_peakStripCount = nPeakStrips;
            QVector<float> peakVerts(
                static_cast<qsizetype>(n) * 2 * kFftVertStride * nPeakStrips);
            auto putPeakStrip = [&](int strip, int j, float x, float y,
                                    float nxPx, float nyPx,
                                    const Kurve::Streifen& st) {
                const qsizetype k =
                    (static_cast<qsizetype>(strip) * n + j) * 2 * kFftVertStride;
                peakVerts[k]     = x + (nxPx * st.o1) / (vpWpx * 0.5f);
                peakVerts[k + 1] = y + (nyPx * st.o1) / (vpHpx * 0.5f);
                peakVerts[k + 2] = flatR;
                peakVerts[k + 3] = flatG;
                peakVerts[k + 4] = flatB;
                peakVerts[k + 5] = st.a1;
                peakVerts[k + 6] = x + (nxPx * st.o2) / (vpWpx * 0.5f);
                peakVerts[k + 7] = y + (nyPx * st.o2) / (vpHpx * 0.5f);
                peakVerts[k + 8]  = flatR;
                peakVerts[k + 9]  = flatG;
                peakVerts[k + 10] = flatB;
                peakVerts[k + 11] = st.a2;
            };
            for (int j = 0; j < n; ++j) {
                float x = (n > 1) ? 2.0f * j / (n - 1) - 1.0f : 0.0f;
                float t = qBound(0.0f, ((m_pxPeakHold[j] + cal) - minDbm) / range, 1.0f);
                float y = yFloor + t * (yTop - yFloor);

                const int jPrev = qMax(j - 1, 0);
                const int jNext = qMin(j + 1, n - 1);
                const float xPrev = (n > 1) ? 2.0f * jPrev / (n - 1) - 1.0f : 0.0f;
                const float xNext = (n > 1) ? 2.0f * jNext / (n - 1) - 1.0f : 0.0f;
                const float tPrev = qBound(0.0f, ((m_pxPeakHold[jPrev] + cal) - minDbm) / range, 1.0f);
                const float tNext = qBound(0.0f, ((m_pxPeakHold[jNext] + cal) - minDbm) / range, 1.0f);
                const float yPrev = yFloor + tPrev * (yTop - yFloor);
                const float yNext = yFloor + tNext * (yTop - yFloor);

                const float dxPx = (xNext - xPrev) * (vpWpx * 0.5f);
                const float dyPx = (yNext - yPrev) * (vpHpx * 0.5f);
                const float segLenPx = std::sqrt(dxPx * dxPx + dyPx * dyPx);
                const float uxPx = (segLenPx > 1e-6f) ? dxPx / segLenPx : 1.0f;
                const float uyPx = (segLenPx > 1e-6f) ? dyPx / segLenPx : 0.0f;
                for (int s = 0; s < nPeakStrips; ++s) {
                    putPeakStrip(s, j, x, y, -uyPx, uxPx, pStreifen[s]);
                }
            }
            batch->updateDynamicBuffer(m_fftPeakVbo, 0,
                static_cast<size_t>(n) * 2 * kFftVertStride * nPeakStrips
                    * sizeof(float),
                peakVerts.constData());
            m_peakHoldHasData = true;
        }
    }

    // ---- 3D stacked-trace: build + upload the DSS surface ----
    // Must happen before beginPass -- QRhi texture uploads go through a
    // resource update batch outside the render pass, same as the overlay
    // textures' uploads above.
    if (m_renderMode == SpectrumRenderMode::Mode3D && m_dss3dGpuTex && m_dss3dSrb) {
        const QSize dssPx(qMax(1, static_cast<int>(specRect.width() * dpr)),
                          qMax(1, static_cast<int>(specRect.height() * dpr)));
        if (dssPx != m_dss3dGpuTexSize) {
            m_dss3dGpuTex->setPixelSize(dssPx);
            m_dss3dGpuTex->create();
            m_dss3dGpuTexSize = dssPx;
            m_dss3dSrb->setBindings({
                QRhiShaderResourceBinding::sampledTexture(1,
                    QRhiShaderResourceBinding::FragmentStage, m_dss3dGpuTex, m_ovSampler),
            });
            m_dss3dSrb->create();
            m_dss3dNeedsUpload = true;
        }
        if (m_dss3dNeedsUpload && m_dss.hasData()) {
            // Noise-floor-anchored, like the classic waterfall's own
            // AGC/threshold logic -- m_nfLerpAverage is the smoothed,
            // visible estimate (see processNoiseFloor). -199 is just above
            // the "not yet measured" sentinel default (-200).
            const float floorBase = (m_nfLerpAverage > -199.0f)
                ? m_nfLerpAverage : (m_refLevel - m_dynamicRange);
            // ── Der Flur gehoert UEBER den Rauschteppich ────────────
            //
            // Hier stand `- 6.0f`, nach AetherSDRs Vorgabe fuer „3D
            // Floor" (m_dssFloorOffsetDb). Dort gibt es dazu einen
            // Regler, bei uns nicht (siehe DssRenderer.h) — und mit
            // dem Flur 6 dB UNTER dem Teppich bekommt der Teppich
            // selbst rund 14 Prozent der vollen Rippenhoehe. 96
            // solcher Zeilen uebereinander ergeben einen Wald aus
            // Rippen, und der Betreiber liest ihn als Stationen:
            // „keine signale obwohl wasserfall", „hier hat es was von
            // grund auf" (2026-10-22).
            //
            // Gemessen mit einem echten Rahmen seiner FLEX-8400
            // (tests/data/flex_rahmen_20m.csv) durch denselben
            // Zeichenweg, Welligkeit der Spaltenhelligkeit:
            //   Flur −6 dB   0,30   Wald, kein Signal erkennbar
            //   Flur  0 dB   …      noch dicht
            //   Flur +3 dB   …      flacher Teppich, ~10 Signale klar
            //   Flur +6 dB   …      flach, nur noch 3 Signale
            // ── Nachgemessen am 2026-10-23: 0 dB, nicht +3 ──────────
            //
            // +3 dB stand hier einen Tag lang. Es war an einem gut
            // belegten Band gemessen, und auf einem RUHIGEN schneidet
            // es alles weg: die Bandmeldung zeigte Rauschteppich
            // −104,4 dBm und 99. Perzentil −103,8 dBm, also 0,6 dB
            // dazwischen. Der Betreiber: „die farbe veraendert sich so
            // automatisch ins dunkle, dass man dann oben gar nichts
            // mehr sieht".
            //
            // Dieselbe Messreihe gab die Antwort schon her (Hoehe der
            // Flaeche je Spalte, gegen eine ebene Flaeche = 150 px):
            //
            //   Flur     p10   p50   p90   max
            //   −6 dB    158   161   163   168   Wald
            //    0 dB    150   152   156   162   <- Teppich fast flach,
            //                                       Signale am hoechsten
            //   +3 dB    150   150   150   159
            //   +6 dB    150   150   150   154
            //
            // Bei 0 dB traegt der Teppich zwei Bildpunkte und das
            // staerkste Signal zwoelf — bei +3 nur noch neun. Null
            // behaelt also MEHR Signal bei fast demselben flachen
            // Teppich. Ein Regler dafuer fehlt weiterhin.
            const float floorDbm = std::round(floorBase * 2.0f) / 2.0f;
            const float rangeDb = m_dynamicRange;
            const quint64 paletteToken =
                (static_cast<quint64>(m_wfColorScheme) << 32)
                ^ static_cast<quint64>(m_wfLowColor.rgba());
            // Reuses the classic waterfall's own palette (not a second,
            // separately-maintained one) so the 3D surface always matches
            // whichever waterfall colour scheme is active.
            // ── Hoehe und Farbe brauchen VERSCHIEDENE Anker ──────────
            //
            // `floorDbm` taugt fuer die HOEHE: der Rauschteppich soll
            // an der Grundlinie liegen, sonst wird aus ihm ein Wald
            // von Rippen (2026-10-22). Fuer die FARBE ist derselbe
            // Anker falsch — er legt den Teppich auf Stufe 0, also auf
            // SCHWARZ, und zwischen den Signalen klafft ein Loch. Der
            // Betreiber am 2026-10-23: „schaue oben, zwischen den
            // signalen ist es schwarz".
            //
            // Es ist derselbe Fehler, den der Wasserfall einen Commit
            // vorher hatte, nur an der zweiten Stelle. Also derselbe
            // Griff und derselbe Anteil: der Farbflur liegt 18 Prozent
            // der Spanne UNTER dem Hoehenflur, damit der Teppich im
            // unteren Blau liegt und seine Struktur zeigt.
            constexpr float kTeppichAufDerRampe = 0.18f;
            const float farbFloorDbm = floorDbm - rangeDb * kTeppichAufDerRampe;
            auto palette = [this, farbFloorDbm, rangeDb](float dbm) -> QRgb {
                const float f = rangeDb > 0.0f
                    ? qBound(0.0f, (dbm - farbFloorDbm) / rangeDb, 1.0f) : 0.0f;
                return waterfallColorForIntensityF(f, m_wfColorScheme, m_wfLowColor);
            };
            // 0.70 -- AetherSDR's fixed default zCurve (m_dssZCurve).
            const QImage& dssImg = m_dss.image(dssPx, 0, floorDbm, rangeDb, 0.70f,
                                               palette, paletteToken, m_bgFillColor);
            batch->uploadTexture(m_dss3dGpuTex, QRhiTextureUploadEntry(0, 0,
                QRhiTextureSubresourceUploadDescription(dssImg)));
            m_dss3dNeedsUpload = false;
        }
    }

    cb->resourceUpdate(batch);

    // ---- Begin render pass ----
    // Die größte Fläche im Fenster. Solange sie blaustichig blieb,
    // nützte jede andere Korrektur nichts.
    const QColor clearColor = roleColor("app-bg", Style::kAppBg);
    cb->beginPass(renderTarget(), clearColor, {1.0f, 0});

    // outputSize/dpr: hoisted above the FFT vertex-generation block
    // earlier in this function (see that comment for why) -- reused
    // here unchanged.

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

    // ── Die Chrome-Schicht liegt UNTER der Kurve ─────────────────────
    //
    // Gefunden am 2026-08-22, live am Geraet: der Wasserfall lief, die
    // Kurve fehlte — seit Tagen, auf jedem Build, mit Werks- wie mit
    // Betreiber-Einstellungen. Vertices gesund (964 Punkte, Zyan,
    // Deckkraft 0.9), Zeichenbefehl lief, nichts zu sehen.
    //
    // Ursache: die Behebung von „hintergrundbild funktioniert auch
    // nicht im pandapter" (2026-08-20) malt paintBackgroundLayer in
    // die statische Overlay-Textur — und dieses Quad wurde NACH der
    // Kurve gezeichnet. Ein deckender Grund ueber der Kurve: das
    // Raster aus derselben Textur blieb sichtbar, die Kurve darunter
    // verschwand. tst_both_paint_paths hat den Zustand seither sogar
    // ERZWUNGEN — der Test verlangte paintBackgroundLayer in beiden
    // Wegen, sagte aber nichts ueber die Reihenfolge.
    //
    // AetherSDR begruendet die richtige Ordnung selbst (Zitat an der
    // Baustelle der Textur): „die Rasterzellen muessen HINTER den
    // Signalspitzen liegen." Also: Chrome zuerst, dann die Kurve,
    // dann NUR die dynamische Schicht (Peak-Hold, Rauschflur) obenauf.
    if (m_ovPipeline) {
        cb->setGraphicsPipeline(m_ovPipeline);
        cb->setViewport({0, 0,
            static_cast<float>(outputSize.width()),
            static_cast<float>(outputSize.height())});
        const QRhiCommandBuffer::VertexInput ovVbuf(m_ovVbo, 0);
        cb->setVertexInput(0, 1, &ovVbuf);
        cb->setShaderResources(m_ovSrb);
        cb->draw(4);
    }

    // Draw FFT spectrum -- or, in Mode3D, the perspective stacked-trace
    // surface instead (built + uploaded above). Either way the waterfall
    // and both overlay layers draw exactly as usual; only this block
    // changes. See DssRenderer.h.
    if (m_renderMode == SpectrumRenderMode::Mode3D) {
        if (m_dss3dSrb && m_dss.hasData()) {
            float specVpX = static_cast<float>(specRect.x()) * dpr;
            float specVpY = static_cast<float>(h - specRect.bottom() - 1) * dpr;
            float specVpW = static_cast<float>(specRect.width()) * dpr;
            float specVpH = static_cast<float>(specRect.height()) * dpr;
            cb->setGraphicsPipeline(m_ovPipeline);
            cb->setViewport({specVpX, specVpY, specVpW, specVpH});
            const QRhiCommandBuffer::VertexInput dssVbuf(m_ovVbo, 0);
            cb->setVertexInput(0, 1, &dssVbuf);
            cb->setShaderResources(m_dss3dSrb);
            cb->draw(4);
        }
    } else if (m_fftFillPipeline && m_fftLinePipeline && m_visibleBinCount > 0) {
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
            // Je Streifen ein Zug, wie bei der Kurve unten.
            for (int s = 0; s < m_peakStripCount; ++s) {
                cb->draw(m_visibleBinCount * 2, 1, s * m_visibleBinCount * 2);
            }
        }

        // Line pass
        cb->setGraphicsPipeline(m_fftLinePipeline);
        cb->setShaderResources(m_fftSrb);
        cb->setViewport(specVp);
        const QRhiCommandBuffer::VertexInput lineVbuf(m_fftLineVbo, 0);
        cb->setVertexInput(0, 1, &lineVbuf);
        // Je Streifen ein eigener Zug aus demselben Puffer: Hof,
        // Kanten, Kern -- ohne sie genau einer wie bisher.
        for (int s = 0; s < m_lineStripCount; ++s) {
            cb->draw(m_visibleBinCount * 2, 1, s * m_visibleBinCount * 2);
        }
    }

    // Draw overlay -- static chrome layer first, then dynamic
    // overlays on top.  Same pipeline + VBO; only the SRB / texture
    // binding differs.  Order matters: chrome includes the
    // freq/dBm/time scales which need to read clean from the
    // spectrum; dynamic overlays (peak blobs / peak hold / NF) sit
    // on top of chrome.
    // Nur noch die DYNAMISCHE Schicht (Peak-Hold, Blobs, Rauschflur):
    // das Chrome-Quad ist nach oben vor die Kurve gewandert — siehe die
    // Notiz dort (2026-08-22).
    if (m_ovPipeline && m_ovDynSrb) {
        cb->setGraphicsPipeline(m_ovPipeline);
        cb->setViewport({0, 0,
            static_cast<float>(outputSize.width()),
            static_cast<float>(outputSize.height())});
        const QRhiCommandBuffer::VertexInput vbuf(m_ovVbo, 0);
        cb->setVertexInput(0, 1, &vbuf);
        cb->setShaderResources(m_ovDynSrb);
        cb->draw(4);
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

    delete m_dss3dSrb;        m_dss3dSrb = nullptr;
    delete m_dss3dGpuTex;     m_dss3dGpuTex = nullptr;
    m_dss3dGpuTexSize = QSize();
    // No overlay unlock pass here any more: the display buffers stopped
    // being page-locked on 2026-08-16 (rationale in initOverlayPipeline).
    // The lock/unlock bookkeeping this used to need — one registration
    // leaked per device recreate, Codex review PR #291 — went with it.
    //
    // 2026-05-26 KG4VCF dual-layer overlay split: tear down the
    // dynamic-overlay resources.  Pipeline + VBO + sampler are
    // shared with the static layer; only SRB + texture are
    // separate.
    delete m_ovDynSrb;        m_ovDynSrb = nullptr;
    delete m_ovDynGpuTex;     m_ovDynGpuTex = nullptr;

    delete m_fftLinePipeline;  m_fftLinePipeline = nullptr;
    delete m_fftFillPipeline;  m_fftFillPipeline = nullptr;
    delete m_fftSrb;           m_fftSrb = nullptr;
    delete m_fftLineVbo;       m_fftLineVbo = nullptr;
    delete m_fftFillVbo;       m_fftFillVbo = nullptr;
    delete m_fftPeakVbo;       m_fftPeakVbo = nullptr;

    m_rhiInitialized = false;

    // ── Und die Wegmarken der Wasserfall-Textur zuruecksetzen ────────
    //
    // Ohne diese zwei Zeilen kostet jeder Umzug des Fensters (abloesen,
    // zurueckdocken) die GPU-Textur ihren Inhalt: sie wird oben
    // geloescht und beim naechsten initialize() NEU angelegt — aber der
    // Erstbefuellungs-Pfad laeuft nur, wenn m_wfTexFullUpload steht.
    // Stand er auf false, bekommt die frische Textur nie ein Byte
    // geschrieben und zeigt, was Metal gerade in dem Speicher liegen
    // hatte.
    //
    // Live gemessen am 2026-08-22: Panadapter abloesen -> die ganze
    // Flaeche magenta; zurueckdocken -> bleibt magenta. Der Betreiber
    // hatte am selben Tag ein Bild geschickt, auf dem dasselbe in Rot
    // stand. Der Erstbefuellungs-Pfad selbst war laengst richtig (der
    // Saat-Block bei "waterfall seed image") — er wurde nur nicht mehr
    // angesteuert.
    //
    // m_wfLastUploadedRow muss mit: es ist die zweite Wegmarke, und
    // eine stehengebliebene Zeilennummer laesst den Zeilen-Pfad an der
    // falschen Stelle weitermachen.
    m_wfTexFullUpload   = true;
    m_wfLastUploadedRow = -1;

    // ── Und dasselbe fuer die BEIDEN Overlay-Schichten ──────────────
    //
    // Derselbe Fehler, drei Texturen. Der Neuaufbau der Chrome-Textur
    // haengt an einem GROESSENVERGLEICH:
    //
    //     if (m_overlayStatic.size() != QSize(pw, ph)) { ... }
    //
    // Nach einem Umzug ist die GPU-Textur frisch und leer, das
    // CPU-Bild daneben aber unveraendert — die Groessen stimmen also
    // ueberein, der Zweig laeuft nicht, und die leere Textur wird ueber
    // die GANZE Flaeche gezogen. Deshalb war nach dem Abloesen alles
    // magenta, auch der Kurvenbereich: nicht der Wasserfall, sondern
    // die Chrome-Schicht darueber.
    //
    // Die Bilder wegzuwerfen ist die ehrliche Loesung: ein Nullbild hat
    // eine andere Groesse als jede gueltige, der Vergleich schlaegt an,
    // Textur und Bild werden gemeinsam neu angelegt und gefuellt.
    //
    // Der Betreiber am 2026-08-22: "die farbe magenta und das problem
    // hatten wir schon einmal ganz am anfang." Richtig — und die zwei
    // damaligen Ursachen (eingefallenes dB-Fenster, verdrehte
    // Palette) sind beide noch behoben. Das hier ist die dritte Tuer
    // zum selben Symptom, und sie geht erst auf, seit ein Panadapter
    // ueberhaupt umziehen kann.
    m_overlayStatic       = QImage();
    m_overlayStaticDirty  = true;
    m_overlayDynamic       = QImage();
    m_overlayDynamicDirty  = true;
    // Ohne diese Zeile bleibt die frisch angelegte Textur beim
    // naechsten Rahmen nur TEILWEISE beschrieben — siehe
    // initOverlayPipeline().
    m_ovDynNeedsFullUpload = true;

}

#endif // LONGPATH_GPU_SPECTRUM

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
            // ── Dieses Nachscrollen ist KEIN Zug (2026-10-30) ───────
            //
            // Die neue Mitte liegt um Breite/2 − Rand neben dem VFO —
            // bei 200 kHz genau 80 kHz. Schreibt sie jemand als
            // Abstimmbefehl in die Scheibe zurueck, zieht das Fenster
            // den VFO um diese 80 kHz hinter sich her, obwohl der VFO
            // schon richtig stand. Genau das geschah beim Verbinden
            // mit der FLEX-8400, vier Mal gemessen. Die Rechnung steht
            // in core/FensterRandScroll.h.
            //
            // Darum die Fahne: der Empfaenger des Signals kann den
            // eigenen Nachlauf vom Zug des Bedienenden unterscheiden.
            m_mitteFolgtDemVfo = true;
            emit centerChanged(m_centerHz);
            m_mitteFolgtDemVfo = false;
        }
    }

    // Update off-screen indicator state (both modes)
    //
    // 0 Hz ist keine Frequenz, sondern "kein Geraet": ohne Verbindung
    // zeigte der Zeiger "◀ 0.0000" am linken Rand — eine Null, die wie
    // eine Messung aussieht (HAUSSTIL Regel 7, 2026-09-18).
    double leftEdge = m_centerHz - m_bandwidthHz / 2.0;
    double rightEdge = m_centerHz + m_bandwidthHz / 2.0;
    if (hz <= 0.0) {
        m_vfoOffScreen = VfoOffScreen::None;
    } else if (hz < leftEdge) {
        m_vfoOffScreen = VfoOffScreen::Left;
    } else if (hz > rightEdge) {
        m_vfoOffScreen = VfoOffScreen::Right;
    } else {
        m_vfoOffScreen = VfoOffScreen::None;
    }

    updateVfoPositions();
#ifdef LONGPATH_GPU_SPECTRUM
    markOverlayDirty();
#else
    update();
#endif
}

void SpectrumWidget::recenterOnVfo()
{
    m_centerHz = m_vfoHz;
    m_vfoOffScreen = VfoOffScreen::None;
    // Der Name sagt es schon: das Fenster stellt sich auf den VFO. Kein
    // Zug. Siehe core/FensterRandScroll.h, mitteIstDdcBefehl (2026-11-02).
    m_mitteFolgtDemVfo = true;
    emit centerChanged(m_centerHz);
    m_mitteFolgtDemVfo = false;
    updateVfoPositions();
#ifdef LONGPATH_GPU_SPECTRUM
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

// Die Flaggenverwaltung (addVfoWidget / removeVfoWidget / vfoWidget /
// raiseFrontVfoWidget) ist am 2026-08-18 mit der VFO-Flagge
// weggefallen. Sie legte je Scheibe eine Flagge auf diesem
// Panadapter an, hielt sie in m_vfoWidgets und hob die vorderste bei
// jedem Bild nach oben.



// Die zweite Haelfte dieses Befunds (Sub-Epic J, 2026-07-28) war
// raiseFrontVfoWidget: die vorderste Flagge musste bei jedem Bild
// wieder nach oben, sonst ueberdeckte die naechste Positionsrunde sie.
// Die Flaggen sind am 2026-08-18 geloescht; die vorderste Scheibe wird
// weiter gefuehrt, weil das Malen sie braucht.
int SpectrumWidget::xForHz(double hz) const
{
    const QRect r = spectrumRect();
    if (r.width() <= 0) { return -1; }
    const int x = hzToX(hz, r);
    // Ausserhalb: die Flagge soll sich dann gar nicht erst setzen.
    if (x < r.left() || x > r.right()) { return -1; }
    return x;
}

void SpectrumWidget::setFrontSliceIndex(int sliceIndex)
{
    m_frontSliceIndex = sliceIndex;
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
    // Hier stand die Positionsrunde ueber alle Flaggen: Breite
    // nachziehen, ausblenden wenn ausserhalb des Fensters, sonst
    // zeigen, an die Frequenz setzen und nach oben heben — und danach
    // die vorderste noch einmal, damit nicht die hoechste Scheibenzahl
    // jedes Bild gewinnt (Befund Sub-Epic J, 2026-07-28).
    //
    // Die Flaggen sind am 2026-08-18 geloescht. Was von der Funktion
    // bleibt, ist die Rechnung darueber: sie fuehrt m_vfoOffScreen
    // nach, und das treibt weiterhin den Auslaufpfeil am Rand
    // (drawOffScreenIndicator).
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

} // namespace Longpath
