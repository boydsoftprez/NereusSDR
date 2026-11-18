#pragma once

// =================================================================
// src/gui/SpectrumWidget.h  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/enums.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (Longpath):
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
#include <QPainterPath>
#include <QPixmap>
#include <QColor>
#include <QString>
#include <QPoint>
#include <QMap>
#include <QHash>
#include <QStaticText>
#include <QTimer>
#include <QPropertyAnimation>

#include "gui/DssRenderer.h"
#include "gui/KurvenStreifen.h"   // Streifen der Kurve: weiche Kante, Hof
#include "gui/StyleConstants.h"   // kAmberText — Vorgabe des Spot-Tons
#include "gui/WaterfallHistoryBuffer.h"
#include "spectrum/ActivePeakHoldTrace.h"
#include "spectrum/PeakBlobDetector.h"
#include "spectrum/SpectrumAvenger.h"

#include <limits>   // quiet_NaN — Sperre der Squelch-Automatik
#include <utility>

#include "core/ConnectionState.h"
#include "core/SignalHistoryStore.h"
#include "gui/spectrum/Durchlass.h"
#include "core/WdspTypes.h"  // DSPMode — for TX filter IQ-space mapping (Plan 4 D9)
#include "core/NoiseFloorEstimator.h"  // Rauschboden der Wasserfall-Bildpunkte

QT_BEGIN_NAMESPACE
class QLabel;
class QMenu;
QT_END_NAMESPACE

// GPU spectrum: QRhiWidget base class for Metal/Vulkan/D3D12 rendering.
// CPU fallback: QWidget with QPainter.
// Note: LONGPATH_GPU_SPECTRUM is set in CMakeLists.txt via target_compile_definitions.
#ifdef LONGPATH_GPU_SPECTRUM
#include <QRhiWidget>
#include <rhi/qrhi.h>
using SpectrumBaseClass = QRhiWidget;
#else
using SpectrumBaseClass = QWidget;
#endif

namespace Longpath {

class BandPlanManager;
class SpectrumOverlayMenu;
class ImdOverlay;  // Phase 3M-4 Task 12 — two-tone IMD overlay analytical core
class WaterfallTicker;  // src/gui/spectrum/WaterfallTicker.h

// Waterfall color scheme presets.
// Default matches AetherSDR/SmartSDR style.
// From Thetis enums.cs:68-79 [v2.10.3.13] (ColorScheme enum). Expanded 4 → 7 in
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
    // ── Gedämpft ─────────────────────────────────────────────────────
    //
    // 2026-08-15. Nach dem Entblauen der Palette war der Wasserfall mit
    // Abstand das Lauteste im Fenster: ein Regenbogen über vierzig
    // Prozent der Fläche, mit einem durchgehenden roten Band dort, wo
    // das Grundrauschen oben an die Rampe stößt.
    //
    // Kein Regenbogen. Eine Rampe, die im Hintergrund beginnt — das
    // Grundrauschen verschwindet also, statt eingefärbt zu werden — über
    // Grau in den warmen Messwert-Ton läuft und erst ganz oben in die
    // Gefahrenfarbe kippt. Die Farbwerte kommen aus dem Theme, nicht aus
    // dieser Datei: siehe wfSchemeStops().
    //
    // Ans Ende gehängt, nicht eingefügt: das Schema wird als int
    // gespeichert, und eine Einfügung in der Mitte würde bei jedem
    // Anwender das Schema verschieben.
    Muted,
    Count
};

// Spectrum-trace display mode. Mode3D swaps the flat FFT trace pipeline for
// a perspective stacked-trace surface (ported from AetherSDR's "3DSS" —
// see DssRenderer.h); the waterfall and every other overlay keep rendering
// exactly as in Mode2D. Stored as int in AppSettings, so append only.
enum class SpectrumRenderMode : int {
    Mode2D = 0,
    Mode3D,
};

// Frequency label alignment for the bottom scale bar.
// From Thetis comboDisplayLabelAlign (setup.designer.cs:34635).
// Thetis exposes 5 options (Left/Center/Right/Auto/Off); Longpath
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
    // AetherSDR PR #1478 [@2bb3b5c]. Longpath raised the cap to 16384
    // (~128 MB at 2000 px wide) post-merge to give ~8 min effective rewind
    // at the default 30 ms refresh and 20+ min at any period ≥ 73 ms.
    // Disk-spool tier deferred to Phase 3M (Recording).
    static constexpr int kMaxWaterfallHistoryRows = 16384;

    // ---- Frequency range ----
    void setFrequencyRange(double centerHz, double bandwidthHz);

    // ── Das Begleitgeraet (2026-10-28) ──────────────────────────────
    //
    // Die Marke sitzt dort, wo das fremde Funkgeraet steht. Die
    // Frequenz kommt in der ZF -- also in der Achse dieses Widgets --,
    // NICHT auf der Antenne: ein Transverter gehoert in die Beschriftung,
    // nicht in die Geometrie. Wer hier eine Antennenfrequenz hineingibt,
    // malt die Marke 116 MHz neben das Signal.
    //
    // Sie fuehrt Longpaths eigene Scheibe nicht. Das waere ein Kreis,
    // sobald der Klick in die andere Richtung geht -- die Scheibe
    // schickte dem Geraet eine Frequenz, das Geraet meldete sie zurueck,
    // und beide liefen davon.
    void setCompanionMarker(bool shown, qint64 ifHz,
                            const QString& label = QString{});

    // ── Bandstapel: Marken an der Skala (2026-11-11) ───────────────
    //
    // Betreiber hat am 2026-11-11 Entwurf D1 gewaehlt: jeder Eintrag
    // des Bandstapels ein kleines Dreieck am unteren Rand des Spektrums,
    // direkt ueber der Frequenzskala; die Stelle der Scheibe heller,
    // gesperrte mit Schloss. Klick = hin (bandStackMarkClicked),
    // Ueberfahren zeigt den Eintrag. Nichts liegt ueber dem Signal.
    // Beim Senden nicht gezeichnet und nicht anklickbar.
    //
    // Die Frequenzen sind die der Scheibe (dieselbe Achse wie die
    // VFO-Marke). Eine leere Liste blendet alles aus.
    struct BandStackMark {
        QString id;
        qint64  hz{0};
        bool    place{false};
        bool    locked{false};
        QString tip;
    };
    void setBandStackMarks(const QVector<BandStackMark>& marks);
    const QVector<BandStackMark>& bandStackMarks() const { return m_bandStackMarks; }
    /// Die Marke unter (x, y), leer wenn keine.
    QString bandStackMarkAtForTest(int x, int y) const;
    bool   companionMarkerShown() const { return m_companionShown; }
    qint64 companionMarkerHz()    const { return m_companionIfHz; }
    void setCenterFrequency(double centerHz);
    double centerFrequency() const { return m_centerHz; }
    double bandwidth() const { return m_bandwidthHz; }

    /// Ob je ein Ausschnitt GESETZT wurde — nicht bloss die Vorgabe.
    /// Die Kopfzeile des Panadapters haengt daran: eine Frequenz ohne
    /// Radio waere eine Behauptung, eine FALSCHE ueber dem richtigen
    /// Bild ist schlimmer (2026-10-27).
    bool hatAusschnitt() const { return m_ausschnittGesetzt; }

    /// Width the dBm scale strip reserves along the right edge, 0 when hidden.
    ///
    /// Public so a parent laying widgets over this one can keep clear of it.
    /// PanadapterApplet's status strip is pinned to the top-right and was
    /// landing on top of the strip's range up/down arrows, which made them
    /// hard to see and hard to hit.
    int reservedRightEdgeWidth() const { return effectiveStripW(); }

    // Re-fire the auto-zoom replan with the current bandwidth.  Used by
    // setup pages (e.g. when the user changes the Hz/bin target) to kick
    // the FFTEngine into recomputing targetSize without a zoom action.
    void requestAutoZoomReplan() { emit bandwidthChangeRequested(m_bandwidthHz); }
    void setDdcCenterFrequency(double hz);
    double ddcCenterFrequency() const { return m_ddcCenterHz; }
    void setSampleRate(double hz);
    double sampleRate() const { return m_sampleRateHz; }

    /// Hoehe des Kurvenbereichs allein — ohne Wasserfall, Bandbalken
    /// und Frequenzskala. Die Knopfspalte am linken Rand richtet ihre
    /// Hoehe danach: parentWidget()->height() waere die ganze Flaeche,
    /// und genau darueber soll sie NICHT liegen.
    int spectrumAreaHeight() const {
        return static_cast<int>(static_cast<float>(height()) * m_spectrumFrac);
    }

    // ---- Display range ----

    /// Kleinste Spanne, die setDbmRange stehen lässt.
    ///
    /// m_dynamicRange ist der Nenner jeder dBm→Bildpunkt-Umrechnung.
    /// Null teilt durch null, negativ stellt die Skala auf den Kopf.
    /// Öffentlich, damit ein Test die Grenze prüfen kann, ohne sie
    /// abzuschreiben.
    static constexpr float kMinSpanDb = 10.0f;

    // ── Die Vorgabe steht an EINER Stelle ────────────────────────────
    //
    // Sie stand an dreien: hier als Member-Vorgabe, in loadSettings() als
    // Rückfallwert, und als abgeschriebene Zahl in
    // tst_pan_display_settings_inherit. Als der Bereich von −48/−116 auf
    // −30/−190 ging, fiel der Test — nicht weil etwas kaputt war,
    // sondern weil er die vierte Kopie der Zahl hielt.
    //
    // Öffentlich, damit der Test sie lesen kann statt sie noch einmal
    // hinzuschreiben. Begründung der Werte selbst: siehe m_refLevel.
    static constexpr float kDefaultRefLevelDbm = -30.0f;    // oberer Rand
    static constexpr float kDefaultRangeDb     = 160.0f;    // Spanne
    static constexpr float kDefaultFloorDbm =
        kDefaultRefLevelDbm - kDefaultRangeDb;              // −190

    /// Setzt den dargestellten Bereich. Vertauschte Grenzen werden
    /// getauscht, eine Spanne unter kMinSpanDb nach unten aufgeweitet —
    /// das Maximum bleibt dabei stehen, weil es der Bezugspegel ist.
    void setDbmRange(float minDbm, float maxDbm);
    float refLevel() const { return m_refLevel; }
    float dynamicRange() const { return m_dynamicRange; }

    // ---- 3D stacked-trace view ----
    void setSpectrumRenderMode(SpectrumRenderMode mode);

    /// Welche Puffer der 2D-Kurve in diesem Bild gefuellt werden duerfen.
    /// Nur, was im selben Bild auch gezeichnet wird: Metal haelt jede
    /// Aenderung an einem dynamischen Puffer mit einer Kopie der Daten
    /// vor, bis der Puffer in einem Zeichenbefehl gebunden wird. Ein
    /// Puffer, der gefuellt, aber nie gezeichnet wird, sammelt ueber die
    /// Sitzung an (2026-11-18 gemessen: 23 GB, Beenden 64 s).
    struct KurvenPuffer {
        bool linie{false};
        bool fuellung{false};
        bool peakHold{false};
    };
    static KurvenPuffer kurvenPufferFuellen(SpectrumRenderMode modus,
                                            bool pipelinesDa,
                                            bool fuellungAn,
                                            bool peakHoldBereit)
    {
        KurvenPuffer p;
        // Im 3D-Modus zeichnet der Durchgang die 2D-Kurve nicht.
        const bool kurve = (modus != SpectrumRenderMode::Mode3D) && pipelinesDa;
        p.linie    = kurve;
        p.fuellung = kurve && fuellungAn;
        p.peakHold = kurve && peakHoldBereit;
        return p;
    }
    SpectrumRenderMode spectrumRenderMode() const { return m_renderMode; }
    /// True: every row of the surface keeps the full width, so a ridge
    /// stands above the frequency it belongs to and matches the waterfall
    /// underneath. False: AetherSDR's perspective, which pulls older rows
    /// toward the centre (up to 31 kHz on a 200 kHz window, measured
    /// 2026-10-28) -- prettier, but the two pictures then disagree.
    void setDss3dTrueScale(bool on);
    bool dss3dTrueScale() const { return m_dss3dTrueScale; }

    // ---- Waterfall settings ----
    void setWfColorScheme(WfColorScheme scheme);
    WfColorScheme wfColorScheme() const { return m_wfColorScheme; }
    void setWfColorGain(int gain);
    int  wfColorGain() const { return m_wfColorGain; }
    void setWfBlackLevel(int level);
    int  wfBlackLevel() const { return m_wfBlackLevel; }

    // Display-Flyout auf/zu (der Pfeil-Knopf links oben im Panel) --
    // reines Chrome, keine Wasserfall-Eigenschaft, aber vom selben Fehler
    // betroffen: SpectrumOverlayPanel::collapsed() wurde nie irgendwohin
    // verdrahtet (aehnlich dem B8-Task-20-Fund bei WF Gain/Black Lvl/
    // Farbschema), der Zustand ging also nie in die Einstellungen.
    void setOverlayPanelExpanded(bool expanded);
    bool overlayPanelExpanded() const { return m_overlayPanelExpanded; }

    // ---- Spectrum renderer controls (Phase 3G-8 commit 3) ----

    // Legacy combined averaging mode — kept for backward compat (existing
    // callers not yet migrated).  Routes internally to setSpectrumAveraging().
    // Retired key: DisplayAverageMode (migration in Task 5.1).
    void setAverageMode(AverageMode m);
    AverageMode averageMode() const { return m_averageMode; }

    // ---- Detector + Averaging split (Task 2.1, handwave fix from 3G-8) ----
    // Ported from Thetis specHPSDR.cs:302-415 [v2.10.3.13] DetTypePan /
    // DetTypeWF / AverageMode / AverageModeWF.
    // RX1 scope dropped; Longpath applies as global panadapter default
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

    // 2026-05-22 bench fix for MaxBin meter accuracy.
    // Returns the strongest dBm pixel inside the active slice's IF
    // passband, computed from the undented spectrum pixels
    // (measurementPixels(), post detector + avenger pipeline -- see the
    // visual-notch note in the definition). Falls back to -400 sentinel
    // when those pixels are empty (cold start) or when the slice passband
    // falls entirely outside the visible spectrum window.
    //
    // The raw per-bin FFT power that MaxBin previously scanned (via
    // WdspEngine::onSpectrumBinsForMaxBin reading FFTEngine::fftReady)
    // can be ~12-17 dB below the spectrum's displayed pixel value
    // because the spectrum's detector + invEnb window-normalization +
    // avenger time-smoothing reconstructs the integrated signal power
    // that a single FFT bin can't show on its own. Sourcing MaxBin
    // from the display pipeline makes the meter read what the operator
    // visually sees on the trace.
    double peakDbmInSlicePassband() const;

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
    // Longpath-original — Thetis ties the peak trace colour to the data-line
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

    // ── Weiche Kante und Hof ("Kurven mit Hof", Glas & Tiefe) ────────
    //
    // Zwei Zutaten der am 2026-09-17 gewaehlten Gestaltungsrichtung, an
    // der GPU-Kurve und an der Spitzenhaltelinie.
    //
    // VORGABE: weiche Kante AN, Hof AUS. Gewaehlt vom Betreiber am
    // 2026-10-08 ("entscheide selbst") nach den drei Blaettern in
    // docs/design/2026-10-08-kurve-weiche-kante/. Die weiche Kante nimmt
    // die Treppe an steilen Flanken weg und sonst nichts; der Hof machte
    // das Band im Rauschen merklich dicker und nebliger und bleibt
    // deshalb ein Schalter, keine Vorgabe.
    //
    // Mit BEIDEN aus zeichnet die GPU weiterhin genau einen Streifen mit
    // den alten Werten, bitgleich -- Geometrie in
    // `gui/KurvenStreifen.h`, gepinnt in tst_kurven_streifen. Wer die
    // alte Kurve zurueck will, schaltet die weiche Kante ab.
    //
    // Der Hof gilt nur fuer die lebende Kurve. Die Spitzenhaltelinie
    // bekommt die weiche Kante mit (das ist die Treppenbehebung), aber
    // keinen zweiten Hof: zwei Hoefe uebereinander sind Nebel, und der
    // Hof soll die lebende Kurve heben, nicht die Merklinie.
    //
    // BEIDES WIRKT AUF DEM GPU-WEG. Der QPainter-Rueckfall (greift nur,
    // wenn QRhi nicht aufsetzt) malt die Kurve als `QPen` -- der hat
    // seine weiche Kante vom Antialiasing schon und kennt keinen Hof.
    // Dort sehen die Schalter deshalb nach nichts aus. Das ist eine
    // bewusste Luecke und keine Vergessenheit: der Rueckfall ist kein
    // Weg, auf dem jemand Gestaltung beurteilt.
    void setTraceSoftEdge(bool on);
    bool traceSoftEdge() const { return m_traceSoftEdge; }
    void setTraceHalo(bool on);
    bool traceHalo() const { return m_traceHalo; }

    // Trace gradient: when enabled, the QPainter fill gradient ramps
    // from transparent at baseline to the fill color at the trace,
    // and the trace uses a vertical color gradient. GPU path keeps
    // its existing per-vertex heatmap coloring (wire-up in commit 5).
    /// Die Fuellung unter der Kurve verlaeuft (an) oder ist flach (aus).
    void setGradientEnabled(bool on);
    bool gradientEnabled() const { return m_gradientEnabled; }

    /// Die Kurve wird nach Pegel eingefaerbt statt flach gezeichnet.
    /// Getrennt von gradientEnabled, weil beide bis 2026-08-15 an
    /// derselben Fahne hingen und auf CPU- und GPU-Pfad Verschiedenes
    /// taten — siehe die Notiz bei m_heatmapEnabled.
    void setHeatmapEnabled(bool on);
    bool heatmapEnabled() const { return m_heatmapEnabled; }

    // ── Schaerfe (2026-11-08) ───────────────────────────────────────────
    //
    // Betreiber: „was kannst du graphisch beim Panadapter noch verbessern,
    // auch in Hinsicht auf schaerfere Grafik". Drei Stellschrauben, die
    // der Zeichenweg bisher fest verdrahtet hatte. Erst als Entwurf (alle
    // aus) an den Blaettern der Rendering-Werkbank verglichen (Varianten
    // s_*), dann vom Betreiber gewaehlt: „Alles zusammen". Seitdem ALLE
    // DREI AN ab Werk und je Panadapter gespeichert. Wer das alte Bild
    // will: DisplayWaterfallDevicePixels / DisplayGridHairline = False,
    // DisplayTraceEdgePx = 1.0.
    //
    // * Wasserfall in Geraete-Pixeln. Das Wasserfallbild war in LOGISCHEN
    //   Pixeln angelegt (applyResizeSettled), die Kurve darueber rechnet
    //   seit 2026-08-26 in Geraete-Pixeln. Auf dem Retina-Schirm (dpr 2)
    //   streckte die GPU jede Wasserfallspalte linear auf zwei Punkte --
    //   weich, und schmale Traeger bekamen die halbe Aufloesung. Die
    //   HOEHE bleibt logisch: eine Zeile je Takt, also dieselbe Zeit je
    //   Bildschirmhoehe wie bisher.
    void setWaterfallDevicePixels(bool on);
    bool waterfallDevicePixels() const { return m_wfDevicePixels; }
    // * Raster als Haarlinie: ein Geraete-Pixel statt eines logischen
    //   (zwei Punkte auf Retina), und an derselben stetigen Stelle wie
    //   die Kurve statt auf ganze logische Pixel gerundet.
    void setGridHairline(bool on);
    bool gridHairline() const { return m_gridHairline; }
    // * Auslauf der weichen Kurvenkante in Geraete-Pixeln. Ab Werk 0,5
    //   (vorher fest Kurve::kKantePixel = 1,0): glaettet nur die Treppe,
    //   statt die Linie weich zu zeichnen.
    void setTraceEdgePx(float px);
    float traceEdgePx() const { return m_traceEdgePx; }
    /// Pruefstand (tst_panadapter_schaerfe): Groesse des Wasserfallbilds.
    QSize waterfallImageSizeForTest() const { return m_waterfall.size(); }

    // ── Durchlass und Bandbreite (2026-11-08) ───────────────────────────
    //
    // Betreiber an den Entwurfsblaettern: „Alle vier bauen". Messkern und
    // Begruendung in gui/spectrum/Durchlass.h. Alle vier AN ab Werk, je
    // Panadapter gespeichert, abschaltbar im Zahnrad des Panadapters.
    //
    //   Bandbreite   Zahl am Durchlass, Kantenwerte und Rast beim Ziehen
    //   Klammern     belegte Breite und SNR jedes Signals
    //   Luecken      freie Stellen zum CQ-Rufen, Klick stimmt hin
    //   Lupe         +/-10 kHz um den VFO, aus den vollen FFT-Bins;
    //                0 aus, 1 nur wenn der Durchlass schmal ist, 2 immer
    void setPassbandLabelShown(bool on);
    bool passbandLabelShown() const { return m_zeigeBandbreite; }
    void setSignalBracketsShown(bool on);
    bool signalBracketsShown() const { return m_zeigeKlammern; }
    void setGapFinderShown(bool on);
    bool gapFinderShown() const { return m_zeigeLuecken; }
    void setPassbandMagnifier(int modus);

    // ── Einmessen an einem Traeger (2026-11-08) ────────────────────────
    /// Mittelt die naechsten zehn Spektren und sucht +/-2,5 kHz um die
    /// Stelle `widgetPos` (oder `geraetHz`) den staerksten Traeger;
    /// meldet sich mit traegerGemessen. Wie Thetis' Freq Cal, aber an
    /// der angeklickten Stelle. false, wenn die Stelle nicht im Spektrum
    /// liegt oder noch keine Daten da sind.
    bool starteTraegerMessungBei(const QPoint& widgetPos);
    bool starteTraegerMessung(double geraetHz);
    bool traegerMessungLaeuft() const { return m_eichZielHz > 0.0; }

    // ── Bandlage fuer den Rundum-Lauf (2026-11-08) ─────────────────────
    /// Meldet hoechstens alle 100 ms bandLage(): Rauschboden (Median des
    /// sichtbaren Ausschnitts), Belegung (mittlerer dB-Ueberschuss ueber
    /// Boden + 6 dB je Bildpunkt) und die Zahl erkannter Signale.
    void setBandLageMelden(bool an) { m_bandLageMelden = an; }
    int  passbandMagnifier() const { return m_lupenModus; }

    // ── S-Stufen an der dBm-Skala (2026-11-08) ──────────────────────────
    //
    // Statt dBm-Zahlen S1 ... S9, +10 ... +60 -- nach Thetis' Bezug
    // (core/SMeterSkala): unter 30 MHz S9 = -73 dBm, darueber -93 dBm,
    // entschieden an der ANTENNENfrequenz des VFO (mit Transverter also
    // 2 m, nicht 28 MHz). Die Striche zeigen den kalibrierten Pegel --
    // anders als die dBm-Zahlen, die roh ablesen (Befund 2026-09-25).
    // Ab Werk aus, Haken im Zahnrad.
    void setSUnitsScaleShown(bool on);
    bool sUnitsScaleShown() const { return m_sStufenSkala; }
    /// Pruefstand: Beschriftung und Rohwert (fuer dbmToY) je Strich.
    QVector<QPair<QString, float>> sUnitTicksForTest() const { return sStufenStriche(); }

    /// Pruefstand: die Auswertung sofort, ohne die 200-ms-Bremse.
    void werteDurchlassAusForTest() { werteDurchlassAus(/*sofort=*/true); }
    QVector<Durchlass::Signal> signalBracketsForTest() const { return m_klammerHaltung.gezeigt(); }
    QVector<Durchlass::Luecke> gapsForTest() const { return m_luecken; }
    int bestGapForTest() const { return m_besteLuecke; }
    /// Die Klickflaechen der Luecken in Widget-Koordinaten (nach dem Malen).
    QVector<QRectF> gapClickRectsForTest() const
    {
        QVector<QRectF> r;
        for (const auto& k : m_lueckenKlick) { r << k.first; }
        return r;
    }
    bool magnifierVisibleForTest() const { return lupeSichtbar(); }
    /// Die Lupe in dBm je Bin, wie sie gezeichnet wuerde (geglaettet).
    QVector<float> magnifierBinsForTest() const { return m_lupeGlatt; }
    QPair<float, float> magnifierRangeForTest() const
    { return {m_lupeHoehe.unten, m_lupeHoehe.oben}; }
    /// Klammern, Luecken und Lupe in ein Bild malen. Im Pruefstand gibt es
    /// keine GPU -- und ohne Malen keine Klickflaechen der Luecken.
    QImage paintPassbandOverlaysForTest();

    // Display calibration offset added to every bin before it's mapped
    // to screen Y. Ported from Thetis Display.RX1DisplayCalOffset
    // (display.cs:1372). Range: -30 .. +30 dB.
    //
    // `merken = false` fuer den Wert, den das Geraet selbst liefert
    // (RadioModel::rxMeterOffsetChanged): er wird bei jedem Start und bei
    // jedem Wechsel neu gerechnet und muss nicht in die Einstellungsdatei.
    // Siehe setDbmCalOffset() in der .cpp fuer die Messung dazu.
    void setDbmCalOffset(float db, bool merken = true);
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

    // Issue #230 fix — Thetis-faithful split between persistent user
    // thresholds (above) and runtime render-active thresholds (below).
    // From Thetis display.cs:6575-6594 [v2.10.3.13]: the render path
    // Upstream tags preserved: //MW0LGE (from cited display.cs:6588) [v2.10.3.15]
    // seeds per-draw locals from the persistent fields then lets AGC /
    // NF-AGC / "Use spectrum min/max" / Clarity override the locals.
    // Active getters are exposed for the regression test; no public
    // setter — runtime layers set these via the dedicated paths below
    // and composeWaterfallActiveThresholds().
    float wfActiveLowThreshold()  const { return m_wfActiveLowThreshold;  }
    float wfActiveHighThreshold() const { return m_wfActiveHighThreshold; }

    // Clarity controller writes the render-active mirror only — never
    // the persisted user field. Modeled on the AGC path in
    // Thetis display.cs:6584 [v2.10.3.13] where the AGC running min
    // (_RX1waterfallPreviousMinValue) is a runtime field separate from
    // waterfall_low_threshold.
    void setClarityWaterfallThresholds(float low, float high);

    // Dasselbe mit dem Rauschboden, aus dem Clarity die Schwellen
    // gebildet hat. Dann verankert der Wasserfall sie an seinen EIGENEN
    // Bildpunkten: Clarity schaetzt den Boden aus den FFT-Bins, gefaerbt
    // werden aber die Bildpunkte nach Detektor und Mittelung — und die
    // liegen je nach Signalstatistik weit daneben (SunSDR2 QRP,
    // 2026-09-25: 17 dB darunter im I/Q-Betrieb, der Wasserfall ganz rot;
    // 10 dB darueber im Einkanal-Zustand, ganz schwarz). Clarity bleibt
    // zustaendig fuer Abstaende, Glaettung und Totband; nur der
    // Bezugspunkt wandert dorthin, wo die Farben entstehen.
    void setClarityWaterfallThresholds(float low, float high, float floorDbm);

    // Threshold composition pulled out of pushWaterfallRow() so the
    // regression test can drive it headlessly. Mutates the active
    // mirror and the AGC running-envelope state; never the persistent
    // user fields. Mirrors the Thetis local-variable composition at
    // display.cs:6575-6594 [v2.10.3.13].
    void composeWaterfallActiveThresholds(const QVector<float>& wfPixelsDbm);

    /// Zieht ein Schwellenfenster, das die gemessenen Werte GAR NICHT
    /// beruehrt, auf deren Mitte; ein teilweise ueberlappendes bleibt
    /// unangetastet.
    ///
    /// Statisch und rein, weil das die ganze Entscheidung ist: sie laesst
    /// sich mit vier Zahlen pruefen, ohne Widget, ohne Radio und ohne ein
    /// gerendertes Bild -- und genau das ist der Punkt, denn der Fehler,
    /// den sie abfaengt, ist zweimal nur als Bild aufgefallen.
    static void fitThresholdsToData(float& low, float& high,
                                    float dataMin, float dataMax);
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

    // Timestamp overlay (Longpath extensions W8/W9).
    enum class TimestampPosition : int { None = 0, Left, Right, Count };
    enum class TimestampMode     : int { UTC = 0, Local, Count };
    void setWfTimestampPosition(TimestampPosition p);
    TimestampPosition wfTimestampPosition() const { return m_wfTimestampPos; }
    void setWfTimestampMode(TimestampMode m);
    TimestampMode wfTimestampMode() const { return m_wfTimestampMode; }

    // Clock-aligned time markers across the waterfall: a thin line with a
    // time label wherever the rows cross a boundary of `seconds` on the
    // clock (15 s ... 15 min), 0 = off. Each row carries its own timestamp
    // (m_wfHistoryTimestamps), so the marks stay pinned to their rows
    // through scrolling and a paused history. The label follows the
    // timestamp mode (UTC / Local). Idea from AetherSDR #5538 (2026-09-21).
    void setWfTimeMarkerSeconds(int seconds);
    int  wfTimeMarkerSeconds() const { return m_wfTimeMarkerSec; }
    // The choices the settings page offers, in seconds (0 = off).
    static const QVector<int>& wfTimeMarkerChoices();
    // Test seam: paint the markers for a given row-timestamp vector (row 0
    // = top) into `p`, the way drawWaterfallChrome does for the live rows.
    void paintWaterfallTimeMarkersForTest(QPainter& p, const QRect& wfRect,
                                          const QVector<qint64>& stamps) {
        paintWaterfallTimeMarkers(p, wfRect, stamps);
    }

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
    // Abstand der waagrechten dB-Linien (2026-09-27). 0 = wie bisher
    // selbst (10 dB, bei <= 50 dB Bereich 5 dB). Der Regler "dB Step"
    // schrieb in das tote PanadapterModel und wirkte nie.
    // Thetis: setup.designer.cs udDisplayGridStep, "Horizontal Grid Step Size in dB."
    void setGridStepDb(int db);
    int gridStepDb() const { return m_gridStepDb; }

    // Right-edge dBm scale strip visibility. When false, the strip is
    // hidden and the spectrum fills the full widget width.
    // ── Eingeblendeter Kompass ───────────────────────────────────────
    //
    // Der Betreiber, 2026-08-20: „kann man den rotorzeiger alleine auch
    // im panadapter einblenden lassen?"
    //
    // Der Panadapter bekommt dabei KEIN Rotorwissen: er nimmt ein
    // fertiges Bild und legt es in eine Ecke. Wer es malt und wann es
    // sich aendert, ist seine Sache nicht — sonst haette das Spektrum
    // plotzlich eine Meinung zu Peilungen, Endanschlaegen und
    // Keulenbreiten, und die naechste Aenderung am Rotor muesste hier
    // nachgezogen werden.
    //
    // Als BILD und nicht als Kindwidget, weil ein nicht-natives Kind
    // auf macOS hinter dem QRhi-Inhalt verschwindet
    // (SpectrumWidget.cpp:551) und ein natives Kind keine
    // Durchsichtigkeit ueber die GPU-Flaeche bekommt. Im
    // QPainter-Durchgang der Ueberlagerung stimmt beides.
    /// Die Flaeche des Spektrums — OHNE die dBm-Leiste am rechten
    /// Rand, genau so, wie dort auch gezeichnet wird. Eine Rechnung
    /// fuer Zeichnen und Treffen.
    QRect spectrumRect() const;

    /// Wo auf dem Schirm steht diese Frequenz? Oeffentlich seit
    /// 2026-10-15 fuer die Scheibenflaggen (SliceFlag): die rechnen
    /// dieselbe Umrechnung wie das Malen, und zwei Rechnungen fuer
    /// dieselbe Sache laufen frueher oder spaeter auseinander.
    /// Gibt -1, wenn die Frequenz gerade nicht im Ausschnitt liegt.
    int xForHz(double hz) const;

    void setBackgroundImage(const QString& path);
    QString backgroundImagePath() const { return m_bgImagePath; }
    void setBackgroundOpacity(int pct);

    // ── Helligkeit des Hintergrundbildes ─────────────────────────────
    //
    // Der Betreiber, 2026-08-21: „hintergrundbild viel zu dunkel bei
    // 100 %."
    //
    // Bei 100 % Bildsichtbarkeit wird das Foto bereits voll gezeichnet
    // — mehr Deckkraft gibt es nicht. Was fehlte, war eine Moeglichkeit,
    // das BILD SELBST aufzuhellen: ein dunkles Foto bleibt ein dunkles
    // Foto, egal wie deckend man es malt.
    //
    // 100 = unveraendert, darueber heller, darunter dunkler. Aufgehellt
    // wird additiv statt ueber den Gammawert: ein dunkles Foto soll
    // sichtbar werden, ohne dass seine hellen Stellen ausbrennen.
    void setBackgroundBrightness(int pct);
    int  backgroundBrightness() const { return m_bgBrightnessPct; }
    int  backgroundOpacity() const { return m_bgOpacity; }
    void setBackgroundFillColor(const QColor& c);
    QColor backgroundFillColor() const { return m_bgFillColor; }

    /// Die eigene Grundfarbe vergessen und wieder dem Thema folgen.
    void resetBackgroundFillColor();
    void loadBackgroundSettings();

    // Dieselbe Idee wie setBackgroundFillColor(), aber fuer den
    // Wasserfall statt den Panadapter -- die Grundflaeche, die man
    // sieht, solange (noch) keine Zeile hineingemalt wurde: beim
    // ersten Start, direkt nach dem Verbinden, oder ganz ohne
    // Verbindung. War bisher an "app-bg" aus dem Thema gebunden, nicht
    // eigens waehlbar -- OE5SOS, 2026-08-25: "eine andere Farbe sollte
    // man auch aendern koennen, sprich den Hintergrund, oben und
    // unten." Oben (Panadapter) gab es schon; das hier ist unten.
    void setWaterfallBackgroundFillColor(const QColor& c);
    QColor waterfallBackgroundFillColor() const { return m_wfBgFillColor; }
    void resetWaterfallBackgroundFillColor();
    void paintBackgroundLayer(QPainter& p, const QRect& specRect);

    void setDbmScaleVisible(bool on);
    bool dbmScaleVisible() const { return m_dbmScaleVisible; }

    // Bandplan overlay (Phase 3G RX Epic sub-epic D)
    void setBandPlanManager(Longpath::BandPlanManager* mgr);
    // ── Squelch-Linie ────────────────────────────────────────────────
    //
    // Port aus AetherSDR SpectrumWidget (setSquelchLine +
    // drawSquelchLine, dort SpectrumWidget.cpp:4453-4506 [@0cd4559]).
    //
    // Longpath-Abweichung: AetherSDR rechnet aus einer 0..160-Stufe
    // (kSqlMinDbm + level), weil FlexRadio den Schwellwert so meldet.
    // Unser SliceModel haelt ihn direkt in dBm (amsqThresh), also faellt
    // die Umrechnung weg — und mit ihr die Frage, was Stufe 0 bedeutet.
    //
    // Die Linie blendet sich nach drei Sekunden aus, wie bei AetherSDR:
    // sie sagt „hier steht die Schwelle", und das muss man beim
    // Einstellen sehen, nicht dauerhaft. Ein Strich, der immer liegt,
    // wird zum Teil des Rasters.
    void setSquelchLine(bool visible, double dbm);
    bool squelchLineVisible() const { return m_squelchLineVisible; }
    double squelchLineDbm() const { return m_squelchDbm; }

    // ── Abstimmhilfe ─────────────────────────────────────────────────
    //
    // Port aus AetherSDR: Schalter setShowTuneGuides
    // (SpectrumWidget.cpp:4068-4090 [@0cd4559]), Sichtbarkeit in
    // updateTrackedCursorState (:9157-9164), Zeichnung im Malweg
    // (:15046-15074).
    //
    // Senkrechte Linie am Zeiger ueber die ganze Hoehe, dazu die
    // Frequenz auf das Hertz genau (14.270.000). Blendet nach vier
    // Sekunden Ruhe aus, wie bei AetherSDR und aus demselben Grund wie
    // die Squelch-Linie darueber: sie hilft beim ABSTIMMEN, und was
    // dauerhaft steht, wird zum Raster.
    //
    // Zwei Longpath-Abweichungen, beide mit Grund:
    //
    // 1. Keine Weitergabe an Geschwister-Widgets. AetherSDR sucht ueber
    //    window() alle SpectrumWidgets ab, weil dort mehrere
    //    Panadapter nebeneinander stehen. Bei uns gibt es bis Phase 3F
    //    genau einen — eine Schleife ueber eine Menge mit einem
    //    Element ist kein Merkmal, sondern toter Code.
    //
    // 2. Der Schluessel heisst DisplayShowTuneGuide und laeuft ueber
    //    settingsKey(..., m_panIndex), nicht ShowTuneGuides global wie
    //    bei AetherSDR: unsere Anzeigeschalter liegen samt und sonders
    //    pro Panadapter, siehe die Nachbarn DisplayShowCursorFreq und
    //    DisplayShowZeroLine. Ein einzelner globaler Schluessel mitten
    //    darin waere die Ausnahme, die spaeter jemand sucht.
    void setTuneGuideEnabled(bool on);
    bool tuneGuideEnabled() const { return m_tuneGuideEnabled; }
    bool tuneGuideShowing() const { return m_tuneGuideShowing; }

    // ── Verlaengerung in den Wasserfall ──────────────────────────────
    //
    // Port aus AetherSDR setExtendedFrequencyLine + setExtendedPassband
    // (SpectrumWidget.cpp:4094-4130 [@0cd4559], Wirkung bei :16845 und
    // :16903). Dort zwei Schalter mit Vorgabe AUS.
    //
    // Longpath-Abweichung: Vorgabe EIN. Wir haben Mittellinie,
    // Filterkanten und Durchlassflaeche seit je unbedingt bis
    // wfRect.bottom() gemalt — das ist der Istzustand, den Betreiber
    // kennen, und eine Vorgabe darf niemandem das Bild umstellen. Der
    // Schalter fuegt die Moeglichkeit hinzu, sie abzuschalten, und
    // nichts weiter.
    //
    // Zweite Abweichung: die Wasserfall-Haelfte der FILTERKANTEN haengt
    // mit an m_extendedFrequencyLine. Bei AetherSDR enden die Kanten
    // ohnehin am Spektrum; wir malen sie durch. Ein eigener dritter
    // Schalter waere eine Einstellung, die niemand sucht — Kante und
    // Linie gehoeren zum selben Bild.
    void setExtendedFrequencyLine(bool on);
    bool extendedFrequencyLine() const { return m_extendedFrequencyLine; }
    void setExtendedPassband(bool on);
    bool extendedPassband() const { return m_extendedPassband; }

    // ── Squelch-Automatik ────────────────────────────────────────────
    //
    // Port aus AetherSDR: setAutoSquelchEnable + setAutoSqlMarginDb +
    // updateAutoSquelchFromBins (SpectrumWidget.cpp:4508-4587
    // [@0cd4559]). Die Schwelle folgt dem Rauschboden mit festem
    // Abstand, statt bei jedem Bandwechsel nachgestellt zu werden.
    //
    // DREI Longpath-Abweichungen, alle aus dem hiesigen Baum begruendet:
    //
    // 1. KEIN eigener Schaetzer. AetherSDR rechnet in
    //    updateAutoSquelchFromBins einen zweiten Rauschboden aus den
    //    Bins (Zweipass-Mittel ueber jeden vierten Bin, danach EWMA
    //    0.1/0.9). Wir haben den Wert schon zweimal: processNoiseFloor
    //    haelt den Thetis-treuen m_nfLerpAverage, und das ist der Wert,
    //    den der Betreiber als NF-Linie SIEHT. Ein dritter Schaetzer
    //    ergaebe eine Schwelle, die neben der sichtbaren Linie liegt —
    //    siehe die Notiz in onNoiseFloorChanged darueber, warum die
    //    ClarityController-Schaetzung fuer die Anzeige gerade NICHT
    //    genommen wird. Wer die Linie sieht, muss die Schwelle daran
    //    ablesen koennen.
    //
    // 2. In dBm, nicht in einer 0..160-Stufe. Dieselbe Entscheidung wie
    //    bei der Squelch-Linie: FlexRadio meldet Stufen, unser
    //    SliceModel haelt dBm (amsqThresh).
    //
    // 3. Keine eigene Bodenlinie (AetherSDRs drawAutoSqlFloor, dort
    //    gestrichelt plus „Floor -120 dBm"). Unsere NF-Anzeige malt
    //    genau diesen Wert bereits, mit einstellbaren Farben und
    //    Linienbreite. Zwei Striche auf derselben Hoehe sind kein
    //    Merkmal, sondern ein Zeichenfehler.
    void setAutoSquelchEnabled(bool on);
    bool autoSquelchEnabled() const { return m_autoSquelchEnabled; }
    void setAutoSqlMarginDb(int db);           // 5..20, wie AetherSDR
    int  autoSqlMarginDb() const { return m_autoSqlMarginDb; }
    // Zuletzt vorgeschlagene Schwelle, oder NaN, solange keine steht.
    double lastAutoSqlThresholdDbm() const { return m_lastAutoSqlDbm; }
    // Fuer den Test: einen Rauschboden einspeisen, ohne FFT-Rahmen.
    void testApplyAutoSquelch(float nfDbm) { updateAutoSquelch(nfDbm); }

    void setBandPlanFontSize(int pt);             // 0 = off
    int  bandPlanFontSize() const { return m_bandPlanFontSize; }
    bool bandPlanVisible() const { return m_bandPlanFontSize > 0; }

    void setShowZeroLine(bool on);
    bool showZeroLine() const { return m_showZeroLine; }

    void setShowFps(bool on);
    bool showFps() const { return m_showFps; }
    float fpsDisplayValueForTest() const { return m_fpsDisplayValue; }

    // 2026-05-26 KG4VCF perf instrumentation: toggle the in-spectrum
    // perf overlay (paint/gap/fft/overlay timings + audio underruns
    // + UDP drops + memory pressure).  Persisted via AppSettings
    // "ShowPerfOverlay"; View -> Performance Overlay wires here.
    //
    // Die Funktion bleibt in beiden Bauarten stehen, nur ihr Rumpf
    // hängt am Wächter — dasselbe Muster wie markOverlayDirty() weiter
    // unten. m_showPerfOverlay lebt im LONGPATH_GPU_SPECTRUM-Block; stand
    // die Zugriffsfunktion davor ausserhalb, übersetzte der CPU-Bau
    // nicht. Das ist keine ausgedachte Lage: CMakeLists.txt:420 macht
    // den GPU-Pfad zur Option, Zeile 417 nennt -DLONGPATH_GPU_SPECTRUM=OFF
    // als den Weg dorthin, und ab Zeile 434 schaltet CMake ihn von
    // selbst ab, wenn Qt älter als 6.7 ist oder ShaderTools bzw.
    // GuiPrivate fehlen.
    void setShowPerfOverlay(bool on);
    bool showPerfOverlay() const
    {
#ifdef LONGPATH_GPU_SPECTRUM
        return m_showPerfOverlay;
#else
        return false;   // ohne GPU-Pfad wird die Überlagerung nie gemalt
#endif
    }

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
    // From Thetis display.cs:2516-2521 (waterfall_low_color) — the colour
    // used at/near the bottom of the waterfall gradient. Thetis wires this
    // into the "enhanced" scheme only (display.cs:6764-6787); the other
    // schemes there hardcode their own floor colour and don't read it.
    // Same scope here: only WfColorScheme::Enhanced honours this.
    void setWfLowColor(const QColor& c);
    QColor wfLowColor() const { return m_wfLowColor; }
    void setGridFineColor(const QColor& c);
    QColor gridFineColor() const { return m_gridFineColor; }
    void setHGridColor(const QColor& c);
    QColor hGridColor() const { return m_hGridColor; }
    void setGridTextColor(const QColor& c);
    QColor gridTextColor() const { return m_gridTextColor; }
    // Plan 4 D9c-1: zero-line color split into separate RX and TX colors.
    // RX default: red (Thetis convention).
    // TX default: amber (Longpath-original — distinguishes during split TX).
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
    // Longpath-native enum; Thetis uses fixed positions (e.g. infoBar is top).
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
    // Fast-attack swap colour is Longpath-original (Thetis hard-codes gray).
    void   setNoiseFloorColor(const QColor& c);
    QColor noiseFloorColor() const { return m_noiseFloorColor; }
    void   setNoiseFloorTextColor(const QColor& c);
    QColor noiseFloorTextColor() const { return m_noiseFloorTextColor; }
    void   setNoiseFloorFastColor(const QColor& c);
    QColor noiseFloorFastColor() const { return m_noiseFloorFastColor; }

    // NF line width — From Thetis display.cs:2310 [v2.10.3.13]
    // m_fNoiseFloorLineWidth=1.0f.  Range 1..5 in Longpath.
    void  setNoiseFloorLineWidth(float w);
    float noiseFloorLineWidth() const { return m_noiseFloorLineWidth; }

    // ---- NF-aware grid (Task 2.9) ----
    // When enabled, the grid lower bound auto-tracks the live noise floor
    // estimate delivered via onNoiseFloorChanged(). Ported from Thetis
    // console.cs:46074-46086 [v2.10.3.13] GridMinFollowsNFRX1 / tmrAutoAGC_Tick.
    // RX1 scope dropped; Longpath applies as a global panadapter default
    // with per-pan override via ContainerSettings dialog (3G-6 pattern).
    // From Thetis setup.cs:24202-24213 [v2.10.3.13]
    // — RX1 scope dropped; Longpath applies as global panadapter default
    //   with per-pan override via ContainerSettings dialog (3G-6 pattern).
    void setAdjustGridMinToNoiseFloor(bool on);
    bool adjustGridMinToNoiseFloor() const { return m_adjustGridMinToNF; }

    // Offset added to the NF estimate to compute the new grid min.
    // From Thetis console.cs:46035 _RX1NFoffsetGridFollow = 5f [v2.10.3.13]
    // — Longpath uses a range of -60..+60 with default 0 (see design 2E).
    //   Thetis subtracts the offset (setPoint = nf - offset) with default +5;
    //   Longpath adds the offset (proposedMin = nf + offset) with default 0,
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
    // call); Longpath stores the flag and propagates it to FFTEngine when
    // the WDSP spectrum engine is integrated (Task 5.x).
    void setDispNormalize(bool on);
    bool dispNormalize() const { return m_dispNormalize; }

    // ShowPeakValueOverlay — scan visible bins, render "Peak: X.X dBm @ Y.YYYY MHz"
    // as corner text. Refreshed on a timer throttled by m_peakTextDelayMs.
    // From Thetis console.cs:20073 PeakTextDelay default=500ms [v2.10.3.13].
    // Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
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
    // grid red during TX. Longpath renders a simpler border tint — full
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
    // Upstream tags preserved: //MW0LGE (from cited upstream lines) [v2.10.3.15]
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
    //                                      Longpath (panadapter stays live
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
    /// Source: Longpath-original glue; per-mode IQ-space mapping follows
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
    /// Die gezeigten Filterkanten. Fuer Pruefungen: was am Modell
    /// steht, muss auch im Bild stehen.
    int  filterLowHz()  const { return m_filterLowHz; }
    int  filterHighHz() const { return m_filterHighHz; }

    // ---- CTUN mode (SmartSDR-style independent pan) ----
    void setCtunEnabled(bool enabled);
    bool ctunEnabled() const { return m_ctunEnabled; }

    /// true, solange das zuletzt gesendete `centerChanged` nur davon
    /// kam, dass das Fenster dem VFO an den Rand nachgescrollt ist —
    /// also KEIN Zug des Bedienenden. Wer daraus einen Abstimmbefehl
    /// macht, baut eine Rueckkopplung von Breite/2 − Rand (bei 200 kHz
    /// genau 80 kHz). Herleitung: core/FensterRandScroll.h.
    bool mitteFolgtDemVfo() const { return m_mitteFolgtDemVfo; }
    void recenterOnVfo();

    // ---- Tuning step ----
    void setStepSize(int hz) { m_stepHz = hz; }
    int  stepSize() const { return m_stepHz; }

    // ---- VFO flag widgets (AetherSDR pattern) ----
    void updateVfoPositions();

    /// Pin sliceIndex's flag to the front of this pan's stacking order.
    ///
    /// Bench-reported 2026-07-28 (Sub-Epic J): with Slice A selected, Slice
    /// B's flag still covered A's, clipping A's frequency readout to
    /// ".955.300". z-order was creation order -- addVfoWidget's one-shot
    /// raise() -- crossed with updateVfoPositions()'s own per-frame raise()
    /// over m_vfoWidgets (a QMap sorted by slice index), which puts whichever
    /// slice has the HIGHER index on top after every single position pass,
    /// with no regard for which one the operator selected.
    ///
    /// A one-shot raise() here would not survive that: updateVfoPositions()
    /// runs every render frame (see its own comment) and would re-apply the
    /// raw ascending order on the very next pass. m_frontSliceIndex is the
    /// pin updateVfoPositions() re-asserts at the end of its own loop so the
    /// front flag survives the next frame too, and every one after it, until
    /// this is called again.
    ///
    /// A sliceIndex this pan does not host (not yet built, or hosted by a
    /// different pan) leaves the pin set but is a harmless no-op here: only
    /// the pan that actually hosts the active slice re-orders, exactly as
    /// PanadapterStack::setActiveSliceOnHostingPan already scopes the rest of
    /// the active-slice machinery to the hosting pan alone.
    void setFrontSliceIndex(int sliceIndex);

// Plain public, not public slots: the enclosing section above is a slots
// block and moc rejects a nested struct inside one.
public:
    // ---- RX marker geometry (Phase 3F) ----

    /// One RX marker's inputs: a slice centre, that slice's own signed filter
    /// edges, and the flag whose bottom edge its triangle hangs from (null
    /// when the pan is drawing its own VFO with no flag created yet).
    struct SliceMarkerGeometry {
        double centreHz{0.0};
        int    filterLowHz{0};
        int    filterHighHz{0};
        /// Frueher die VFO-Flagge, an deren Unterkante das Dreieck
        /// haengt. Die Flaggen sind am 2026-08-18 geloescht; das Feld
        /// bleibt als Anker fuer das, was im Zielbild an ihre Stelle
        /// tritt — heute immer nullptr, dann sitzt das Dreieck am
        /// oberen Spektrumsrand (der Rueckfall stand schon da).
        const QWidget* flag{nullptr};
    };

    /// Every RX marker this pan must paint, one per hosted slice, in slice
    /// order.
    ///
    /// This is drawVfoMarker()'s whole decision, split out so it is reachable
    /// without a live QPainter or a shown QRhiWidget: the harness cannot
    /// render this widget, so the geometry is what gets pinned and the pixel
    /// emission is what does not. See tests/tst_pan_flag_positions.cpp.
    QVector<SliceMarkerGeometry> sliceMarkerGeometry() const;

public slots:
    // Phase 3Q-8: update connection state for the disconnect overlay.
    void setConnectionState(Longpath::ConnectionState s);

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

    // ── KiwiSDR als Anzeigequelle (Stufe 6, 2026-08-23) ──────────────
    //
    // Ein KiwiSDR liefert seinen Wasserfall als fertige dBm-Werte ueber
    // einen eigenen Frequenzausschnitt — nicht als I/Q um eine
    // DDC-Mitte. Trotzdem braucht es dafuer KEINE zweite Rohrleitung.
    //
    // Der Grund steht in updateSpectrumLinear: die Zuordnung Bin ->
    // Frequenz laeuft dort ueber m_ddcCenterHz und m_sampleRateHz.
    // Setzt man beide auf den Kiwi-Ausschnitt, ist der Kiwi einfach
    // eine weitere Quelle von Bins — und Zoom, CTUN, Detektor,
    // Mittelung, Wasserfall und Ruecklauf arbeiten unveraendert
    // weiter. Nichts davon muss ein zweites Mal geschrieben werden.
    //
    // Die Umrechnung dBm -> "linear" geschieht mit Versatz null: der
    // Kiwi hat seine Pegel schon geeicht, ein zweiter Versatz waere
    // eine Verfaelschung.
    void updateKiwiSpectrumDbm(const QVector<float>& binsDbm,
                               double lowHz, double highHz);

    // ── Eine Wasserfallzeile der Maschine (2026-10-23) ───────────────
    //
    // Die FLEX-8400 schickt den Wasserfall als EIGENEN Strom
    // (Paketklasse 0x8004) mit eigenem Frequenzbereich und eigenem
    // Takt. Die Werte sind eine INTENSITAET (qint16/128, am Geraet +88
    // bis +120) und KEINE dBm — darum eine eigene Methode und nicht
    // `pushWaterfallRow`. Begruendung und Formel stehen dort und in
    // docs/protocols/flex-wasserfall-aethersdr.md.
    void zeigeFremdeWasserfallzeile(const QVector<float>& intensitaet,
                                    double lowHz, double highHz);

    // Zeigt dieser Panadapter das Geraet oder den KiwiSDR? Solange er
    // auf KiwiSDR steht, werden Bins vom Geraet VERWORFEN — sonst
    // uebermalten sich zwei Quellen gegenseitig, und man saehe ein
    // Flackern statt eines Bildes.
    /// Unter welchem Schluessel dieser Panadapter seine Wahl merkt.
    /// Leer = nichts merken (Pruefstaende, Einblendungen).
    ///
    /// Der Schluessel sitzt HIER und nicht beim Bedienweg, weil es
    /// mehrere gibt: das Kontextmenue des Panadapters und die
    /// KIWI-Kachel im Frequenz-Widget. Lag das Speichern beim Menue,
    /// vergaess die Kachel es — und genau das war am 2026-08-23 der
    /// Fall, gefunden von tst_settings_are_remembered.
    void setPersistKey(const QString& key) { m_persistKey = key; }

    void setKiwiDisplaySource(bool kiwi);
    bool kiwiDisplaySource() const { return m_kiwiDisplaySource; }

    // NF-aware grid slot — wired to ClarityController::noiseFloorChanged in MainWindow.
    // From Thetis console.cs:46074-46086 [v2.10.3.13] tmrAutoAGC_Tick NF grid block.
    // Range delta uses std::abs() — abs incase //MW0LGE [2.9.0.7] [original inline comment from console.cs:46081]
    // Longpath-original: global panadapter default, no RX1/RX2 split.
    void onNoiseFloorChanged(float nfDbm);

    // ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
    // Reset the rewind ring buffer back to empty + live state. Public so
    // MainWindow can wire it to RadioModel::connectionStateChanged when
    // the radio disconnects (see plan §Task 4 Step 3-4 — Longpath has no
    // SpectrumWidget::clearDisplay() equivalent, so the flush is plumbed
    // through MainWindow rather than embedded in resizeEvent).
    // From AetherSDR SpectrumWidget.cpp:740-756 [@0cd4559]
    void clearWaterfallHistory();

    /// Phase 3F Sub-Epic F Task 6: receive wideband bins for the active-ADC
    /// extended pan. Bins are stored per-ADC; actual painting wires in F
    /// polish (T7-T10). Setter alone enables Sub-Epic H bench operators to
    /// confirm the wideband data path is flowing without UI rendering.
    void setWidebandBins(int adcIndex, const QVector<float>& dbmBins);
    QVector<float> widebandBinsForTest(int adcIndex) const
    {
        return adcIndex == 0 ? m_widebandBinsAdc0 : m_widebandBinsAdc1;
    }

    /// Phase 3F Sub-Epic F Tasks 7-10: allow extended-pan rendering.
    /// The actual state is on only when allowed AND the visible bandwidth
    /// exceeds a known positive DDC sample rate. paintEvent
    /// will render wideband bins as a background fill behind the
    /// listenable DDC island. Full visual polish (dashed boundary lines,
    /// palette-aware bin draw) lands in a post-bench iteration; for now
    /// extendedMode is the derived actual state that drives the signal.
    void setExtendedViewAllowed(bool allowed);
    bool extendedViewAllowed() const { return m_extendedViewAllowed; }
    bool extendedMode() const { return m_extendedMode; }

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
        /// Wie stark er bei MIR ankommt, in dB -- `kKeineAngabe`,
        /// wenn ihn niemand gemessen hat (Cluster, RBN, POTA).
        ///
        /// Die Zahl steht hier doppelt zu `Longpath::kSpotKeinSnr`,
        /// damit dieser Kopf das Modell nicht hereinziehen muss (er
        /// haengt in sehr vielen Uebersetzungseinheiten). Ein
        /// `static_assert` in der .cpp haelt beide zusammen -- sie
        /// koennen nicht auseinanderlaufen.
        static constexpr int kKeineAngabe = -999;
        int    snrDb{kKeineAngabe};
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

    // ── S-Verlauf: Marken aus dem Signalerkenner ─────────────────────
    //
    // Zweiter Markenkanal neben den DX-Spots. Beide laufen durch
    // drawSpotMarkers, weil Kollisionsstapelung und Buendelabzeichen
    // nur dann fuer alles zusammen gelten — Port aus AetherSDR
    // (SpectrumWidget.cpp:15719-15741 [@0cd4559]).
    //
    // Eine Verlaufsmarke wird unterdrueckt, wenn binnen 3 kHz ein
    // echter Spot liegt: der traegt ein Rufzeichen, die Verlaufsmarke
    // nur eine S-Stufe.
    //
    // Quelle der Marken ist SignalHistoryStore; source == "QRM"
    // unterscheidet Stoerung von Sprache.
    void setSignalHistoryMarkers(const QVector<SpotMarker>& markers);
    QVector<SpotMarker> signalHistoryMarkersForTest() const
    {
        return m_signalHistoryMarkers;
    }

    // Was tatsaechlich gezeichnet wuerde: Spots plus Verlaufsmarken nach
    // Schaltern und 3-kHz-Regel.
    QVector<SpotMarker> mergedMarkersForTest() const { return mergedMarkers(); }

    void setShowSignalHistory(bool on);
    bool showSignalHistory() const { return m_showSignalHistory; }
    void setShowSignalHistoryQrm(bool on);
    bool showSignalHistoryQrm() const { return m_showSignalHistoryQrm; }

    // Die zwei Schwellen des S-Verlaufs. Sie lagen bis 2026-08-19 nur in
    // SignalHistoryStore und wurden von niemandem gesetzt — gebaut, an
    // keiner Flaeche, derselbe Fehler, den ich am selben Tag zweimal an
    // anderer Stelle gefunden habe.
    //
    // Die Grenzen (3..30 s und 15..300 s) stehen in der Klasse, nicht
    // hier: eine zweite Bedienflaeche wuerde sie sonst umgehen.
    void setSignalHistoryQrmGateSeconds(int s);
    int  signalHistoryQrmGateSeconds() const;
    void setSignalHistoryLifetimeSeconds(int s);
    int  signalHistoryLifetimeSeconds() const;

    // Die Speisung des S-Verlaufs, gerufen aus processNoiseFloor().
    //
    // DORT und nicht in MainWindow, aus demselben Grund wie bei der
    // Squelch-Automatik: hier liegt m_nfLerpAverage, der SICHTBARE
    // Rauschboden. Naehme der Erkenner einen anderen Schaetzer, faende
    // er Signale an Stellen, wo das Auge keine sieht.
    //
    // Bei ausgeschalteten Schaltern kostet die Funktion einen
    // Vergleich: der Erkenner laeuft nur, wenn jemand die Marken sehen
    // will. Eingeschaltet ist sie auf 10 Hz gedrosselt — die
    // Belegungsrechnung im Speicher rechnet mit der TATSAECHLICHEN
    // Aufrufrate, nicht mit der Bildrate, also bleibt die
    // Stoerungsschwelle richtig.
    void updateSignalHistory(float noiseFloorDbm);
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

    // ── Begleitgeraet: Marke, Achse, Bandplan (2026-10-28) ──────────
    //
    // Ohne #ifdef-Waechter, wie drawSpotMarkersForTest darueber. Ein
    // "...ForTest" hinter einem Waechter uebersetzt lokal durch den
    // vorkompilierten Kopf und faellt erst in der CI um -- der Fall vom
    // 2026-10-16, nachzulesen in docs.
    void drawBandStackMarksForTest(QPainter& p, const QRect& specRect) {
        drawBandStackMarks(p, specRect);
    }
    void drawCompanionMarkerForTest(QPainter& p, const QRect& specRect) {
        drawCompanionMarker(p, specRect);
    }
    void drawBandPlanForTest(QPainter& p, const QRect& specRect) {
        drawBandPlan(p, specRect);
    }
    double rfFuerAnzeigeForTest(double ifHz) const { return rfFuerAnzeige(ifHz); }
    double zfFuerAchseForTest(double rfHz)  const { return zfFuerAchse(rfHz); }
    static double frequenzSchrittForTest(double bandwidthHz, int breitePx) {
        return frequenzSchritt(bandwidthHz, breitePx);
    }
    static QString freqScaleLabelForTest(double hz, double stepHz) {
        return freqScaleLabel(hz, stepHz);
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

    // ---- TNF / notch overlay (design section 8.1) ----
    // Ported from AetherSDR's TnfMarker (src/gui/SpectrumWidget.h:575-581
    // [@c6481cbf]) with depthDb + permanent replaced by `active`: WDSP's
    // notch DB carries neither depth nor permanence, its add entry point
    // taking fcenter / fwidth / active only
    // (third_party/wdsp/src/nbp.c:362, RXANBPAddNotch).
    //
    // UNIT BOUNDARY: freqMhz is the ONLY MHz quantity in the TNF stack.
    // NotchModel::centerHz, all five notch*Requested signals,
    // setNotchMinWidthHz and the visual-notch dent maths are Hz.
    // MainWindow::refreshPanNotchMarkers is the only conversion site.
    struct NotchMarker {
        int    id{-1};
        double freqMhz{0.0};
        double widthHz{200.0};
        bool   active{true};
    };

    // From AetherSDR src/gui/SpectrumWidget.cpp:13436-13440 [@c6481cbf].
    // markOverlayDirty(), not the bare update() the spot push uses: notch
    // chrome is cached in the GPU static-overlay texture, so a dragged
    // marker would otherwise not move on the shipping path.
    void setNotchMarkers(const QVector<NotchMarker>& markers);

    /// Den Notch-Editor auf einen Filter stellen und aufmachen.
    void openNotchEditor(int id, const QPoint& globalPos);

    // From AetherSDR src/gui/SpectrumWidget.cpp:13497-13501 [@c6481cbf].
    // Master TNF flag.  Repaints every marker in the TNF-off colour rather
    // than hiding it (Thetis display.cs:8704-8707 [v2.10.3.15]).
    void setNotchGlobalEnabled(bool on);

    // WDSP's minimum notch width for the channel feeding this pan
    // (third_party/wdsp/src/nbp.c:594, RXANBPGetMinNotchWidth).  Pushed
    // rather than pulled because it varies with nc and sample rate; Thetis
    // caches it the same way (display.cs:1082 [v2.10.3.15]) and refreshes
    // it on filter-size change (console.cs:39052-39054 [v2.10.3.15],
    // UpdateMinimumNotchWidthRX).  Consumed by the visual-notch dent.
    void setNotchMinWidthHz(double hz);

    // ---- Visual notch (trace dent), design section 8.3 ----
    // Owner of the persisted state is NotchModel (key NotchVisualEnabled);
    // MainWindow::refreshPanVisualNotch pushes the same value at every pan.
    // From Thetis display.cs:1070 [v2.10.3.15]:
    //   private static bool m_bShowVisualNotch = false;
    void setVisualNotchEnabled(bool on);
    bool visualNotchEnabled() const { return m_visualNotchEnabled; }

    // Test seams, following the spotMarkersForTest convention above.
    const QVector<NotchMarker>& notchMarkersForTest() const { return m_notchMarkers; }
    bool   notchGlobalEnabledForTest() const { return m_notchGlobalEnabled; }
    double notchMinWidthHzForTest()    const { return m_notchMinWidthHz; }
    void drawNotchMarkersForTest(QPainter& p, const QRect& specRect) {
        drawNotchMarkers(p, specRect);
    }
    QRect notchSpecRectForTest() const { return notchSpecRect(); }

    // Visual-notch test seams (design section 8.3). Read-only views into the
    // state updateSpectrumLinear rebuilds each frame, so the section 11 test
    // can pin the measurement-routing contract without a paint cycle.
    float nfFftBinAverageForTest() const { return m_nfFftBinAverage; }
    const QVector<float>& undentedPixelsForTest() const {
        return measurementPixels();
    }
    const QVector<float>& activePeakHoldPeaksForTest() const {
        return m_activePeakHold.peaks();
    }
    const QVector<PeakBlob>& peakBlobsForTest() const {
        return m_peakBlobs.blobs();
    }
    // Selection and hover are written by the interaction layer (design
    // section 7.4); these also give the render tests a writer for the
    // Chartreuse highlight branch.
    void setSelectedNotchIdForTest(int id) { m_selectedNotchId = id; }
    int renderedPixelCountForTest() const { return m_renderedPixels.size(); }
    /// Die zuletzt geschriebene Wasserfallzeile als Stufen 0..255 —
    /// genau das, was gefaerbt und in die Historie gelegt wurde.
    /// Ohne diese Naht laesst sich nicht pruefen, WO ein Traeger im
    /// Wasserfall landet und ob der Teppich am Anschlag steht.
    const QVector<quint8>& wfZeilenstufenForTest() const {
        return m_wfRowIntensity;
    }
    bool fremdeWasserfallquelleForTest() const {
        return m_fremdeWasserfallquelle;
    }
    const QVector<float>& fremdeWfMittelForTest() const {
        return m_fremdeWfMittel;
    }
    int visibleBinCountForTest() const { return m_visibleBinCount; }

    // Pins updateSpectrumLinear()'s internal displayWidth (normally
    // qMax((width() - effectiveStripW()) * devicePixelRatioF(), 800))
    // to an exact value, bypassing both the real widget geometry and
    // the real screen's devicePixelRatioF() -- neither of which a test
    // can control for a never-shown widget. Added 2026-08-26 alongside
    // the devicePixelRatioF() fix: a fixture built on "800 display
    // pixels == 800 Hz span, so 1.0 Hz per pixel" needs displayWidth to
    // actually BE 800 to keep its hardcoded pixel indices meaningful,
    // and a real (non-1.0) device pixel ratio in the test environment
    // can no longer be relied on to leave that floor alone. 0 (default)
    // means "no override, use the real computation."
    void setDisplayWidthOverrideForTest(int w) { m_displayWidthOverrideForTest = w; }
    /// Oberkante der Frequenzskala — fuer Pruefungen, die die bewusste
    /// Verschiebe-Geste dort ansetzen muessen.
    int freqScaleYForTest() const;

    /// Bildpunkt-Spalte des VFO-Strichs. Wie freqScaleYForTest() nur
    /// fuer Pruefungen da: die Abbildung Frequenz->Punkt haengt an der
    /// Nutzbreite (Fenster minus Randstreifen), und eine Pruefung, die
    /// sie nachrechnet, rechnet frueher oder falsch — erlebt am
    /// 2026-08-22, als der Zug am Balken den Griff um 18 Punkte
    /// verfehlte und der Test das Verhalten faelschlich anklagte.
    int vfoXForTest() const;

    /// Das Spektrum in dBm ueber einen Frequenzbereich, gleichmaessig
    /// auf `points` Stuetzstellen abgetastet. Leer, solange keine
    /// Daten da sind.
    ///
    /// Gebaut fuer den Bandfilter (2026-08-22): der soll das Signal im
    /// Durchlass zeigen, wie es die Vorlage tut. Statt Abbildung und
    /// Kalibrierung ein zweites Mal zu bauen — mit der Aussicht, dass
    /// beide auseinanderlaufen — fragt der Bandfilter hier nach. Diese
    /// Klasse hat die Bins, die DDC-Mitte, die Abtastrate und den
    /// dBm-Versatz, und alles davon ist geprueft.
    ///
    /// Erkennung ist SPITZENWERT, wie im Panadapter: ein schmaler
    /// Traeger darf zwischen zwei Stuetzstellen nicht verschwinden.
    QVector<float> dbmOverRange(double loHz, double hiHz, int points) const;

    // ── Umzugs- und Abbau-Schutz, rueckportiert 2026-08-22 ───────────
    //
    // Diese vier Methoden standen im AetherSDR-Original
    // (SpectrumWidget.cpp [@0cd4559]) und sind bei der Portierung
    // verlorengegangen. Ohne sie: SIGSEGV beim Beenden mit abgeloestem
    // Panadapter (upstream #2495 — exakt unser Stapel in
    // QRhiWidgetPrivate::ensureRhi), eingefrorene NSResponder-Ketten
    // (#1344), doppelte Backing-Stores (#4339), schwarze Fenster nach
    // dem Umhaengen.
    void prepareForTopLevelChange();
    void resetGpuResources();
    void prepareForShutdown();
    void applyNativeWindowIsolationPolicy();
    float refLevelForTest() const { return m_refLevel; }
    float dynamicRangeForTest() const { return m_dynamicRange; }
    QColor fillColorForTest() const { return m_fillColor; }

    class NotchEditPopup* m_notchEditor {nullptr};
    void setHoveredNotchIdForTest(int id)  { m_hoveredNotchId = id; }

    // ---- TNF / notch interaction (design section 7) ----
    // Which part of a notch a press at a given pixel would grab.
    // From Thetis console.cs:49032-49067 [v2.10.3.15]: a side-of-centre
    // default, an 8 px minimum on-screen width before edge zones exist at
    // all, a +/- 4 px edge zone, and Shift as an explicit alternative to
    // being near an edge.  Centre is upstream's m_bDraggingNotch; LowEdge
    // and HighEdge are m_bDraggingNotchBW plus the side flag
    // m_BDragginNotchBWRightSide, folded into one value because they are
    // never independently meaningful.
    enum class NotchGrab { None, Centre, LowEdge, HighEdge };

    // Test seams, following the spotMarkersForTest convention above.
    // Public read-only views onto the private pixel-space logic so
    // tst_notch_hit_test can pin the Thetis rules without a real mouse.
    int       notchAtPixelForTest(int x) const;
    NotchGrab notchGrabAtForTest(int id, int x, bool shiftHeld) const;
    int       selectedNotchIdForTest() const { return m_selectedNotchId; }
    int       hoveredNotchIdForTest()  const { return m_hoveredNotchId; }
    // Populate a caller-owned QMenu with the notch actions.  Exists as a
    // seam because QMenu::exec() blocks, so the menu contents cannot be
    // asserted through a synthetic right-click.
    void buildNotchContextMenuForTest(int id, QMenu& menu) {
        buildNotchContextMenu(id, menu);
    }

    // Overlay-cache seam.  Returns false on a CPU-only build, where there
    // is no cached texture to invalidate.
    bool overlayStaticDirtyForTest() const {
#ifdef LONGPATH_GPU_SPECTRUM
        return m_overlayStaticDirty;
#else
        return false;
#endif
    }
    void clearOverlayStaticDirtyForTest() {
#ifdef LONGPATH_GPU_SPECTRUM
        m_overlayStaticDirty = false;
#endif
    }

    // ── Naehte fuer die drei Befunde vom 2026-10-22 ──────────────────────
    //
    // Befund 3: lief die Analyse? Ohne jeden Malvorgang zu erzwingen.
    bool imdAnalysisValidForTest() const { return m_imdAnalysisValid; }
    // Befund 4: steht das Zweiton-Tor auf DIESEM Pan offen?
    bool testingIMDForTest() const { return m_testingIMD; }
    // Das Messwerk selbst, damit der Lauf den EMA-Stand lesen kann. Als
    // Zeiger, damit ImdOverlay.h nicht in diese Kopfdatei muss.
    const ImdOverlay* imdOverlayForTest() const { return m_imdOverlay; }
    // Befund 6: die dynamische Schicht, in der Marken und Messfeld liegen.
    // Auf einem Bau ohne GPU gibt es keine Textur -- dann immer false,
    // genau wie bei overlayStaticDirtyForTest().
    bool overlayDynamicDirtyForTest() const {
#ifdef LONGPATH_GPU_SPECTRUM
        return m_overlayDynamicDirty;
#else
        return false;
#endif
    }
    void clearOverlayDynamicDirtyForTest() {
#ifdef LONGPATH_GPU_SPECTRUM
        m_overlayDynamicDirty = false;
#endif
    }

signals:
    /// Die Darstellungsart hat gewechselt (2D/3D).
    ///
    /// Es gab dafuer kein Signal: wer die Art von auszen umstellt —
    /// das Ueberblendmenue, der Schnellknopf in der Kopfleiste des
    /// Panadapters — konnte vom anderen Weg nichts erfahren, und die
    /// Beschriftung blieb stehen.
    void spectrumRenderModeChanged(Longpath::SpectrumRenderMode mode);

    /// Weitergereicht vom Rechtsklick-Menue (2026-08-18).
    void openSetupPageRequested(const QString& page);

    /// Mitte und Spanne haben sich geaendert.
    ///
    /// Es gab bisher kein Signal dafuer — jede Anzeige, die die Mitte
    /// zeigen wollte, haette pollen muessen. Die Kopfleiste des
    /// Panadapters ist die erste, die sie braucht.
    void frequencyRangeChanged(double centerHz, double bandwidthHz);
    // 2026-05-22 bench fix: emitted after each updateSpectrumLinear
    // completes (m_renderedPixels populated). MainWindow consumes this
    // to push peakDbmInSlicePassband() into WdspEngine's MaxBin detector
    // so the analog S-meter reads what the operator visually sees on
    // the spectrum instead of the raw per-bin FFT value (which can be
    // ~12-17 dB lower due to window-spread power and missing detector
    // pipeline). See peakDbmInSlicePassband doc.
    void spectrumFrameRendered();

    // Phase 3Q-8: emitted on a left-click while not Connected.
    // MainWindow wires this to showConnectionPanel().
    void disconnectedClickRequest();
    /// Klick auf eine Bandstapel-Marke an der Skala.
    void bandStackMarkClicked(const QString& entryId);

    // Emitted when user clicks on spectrum/waterfall to tune
    void frequencyClicked(double hz);

    // Squelch-Automatik: vorgeschlagene Schwelle in dBm. MainWindow legt
    // sie auf SliceModel::setAmsqThresh, was ueber amsqThreshChanged die
    // Squelch-Linie nachzieht — der Betreiber SIEHT die Automatik damit
    // arbeiten. AetherSDR heisst das Signal autoSquelchLevelSuggested und
    // fuehrt eine 0..160-Stufe; wir fuehren dBm.
    void autoSquelchThresholdSuggested(double dbm);
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

    // 2026-08-11: "Turn rotor to <call>" from the spot label's context
    // menu — same verb, same handler as the Spot List's right-click
    // (SpotHubDialog::rotorRequested): MainWindow raises the rotor dock
    // and hands the call to RotorLogbookPanel::workSpot, which aims and
    // turns. Carries the callsign rather than an index because the
    // rotor path wants the station, not the marker.
    void spotRotorRequested(const QString& dxCall);

    // 2026-08-19: Doppelklick auf ein Spot-Etikett — das Rufzeichen ins
    // Logbuch uebernehmen und die Peilung dazu zeigen. MainWindow holt
    // die Rotor/Log-Flaeche nach vorn und reicht das Rufzeichen an
    // RotorLogbookPanel::takeSpot.
    //
    // Ohne Frequenz: der erste Klick des Doppelklicks hat schon
    // abgestimmt, und der Logbuch-Eintrag liest Frequenz, Band und
    // Betriebsart aus der aktiven Scheibe.
    void spotLogRequested(const QString& dxCall);

    // 2026-05-12 bench fix (Gap #6).  Fires when the mouse enters or
    // leaves a spot label hit-rect.  -1 indicates "no spot under
    // cursor" (use to clear the Spot List highlight).  Drives the
    // panadapter <-> Spot List hover sync.
    void spotHoverIndexChanged(int spotIndex);

    // ---- TNF / notch overlay (design section 8.1) ----
    // Wired per pan by MainWindow::wirePanNotchHandlers, so a marker drawn
    // on a pan acts through that pan's own frequency mapping while the
    // NotchModel the handlers mutate stays global (design D1).  `narrow` is
    // the Shift-held 100 Hz add (Thetis console.cs:40269 [v2.10.3.15]).
    //
    // UNIT BOUNDARY: every frequency here is absolute RF in Hz.  The only
    // MHz quantity in the TNF stack is NotchMarker::freqMhz, converted once
    // in MainWindow::refreshPanNotchMarkers.  Emitters land with the
    // interaction layer (design sections 7.1 through 7.4).
    void notchCreateRequested(double freqHz, bool narrow);

    /// Einmessen (2026-11-08): die Messung nach starteTraegerMessung ist
    /// fertig. geraetHz = Traeger am Geraet, anzeigeHz = auf der Antenne
    /// (mit Transverter umgerechnet). Siehe core/FrequenzEichung.h.
    void traegerGemessen(bool gefunden, double geraetHz, double anzeigeHz,
                         float snrDb);
    void bandLage(float bodenDbm, float belegung, int signale);
    void notchMoveRequested(int id, double newFreqHz);
    void notchWidthRequested(int id, double widthHz);
    /// Emitted when a notch drag ends, so the coalesced DSP push can flush
    /// immediately and the final position is exact rather than up to one
    /// coalescing window stale.
    void notchDragFinished();
    void notchActiveRequested(int id, bool active);
    void notchRemoveRequested(int id);
    /// Alle Notch-Filter auf einmal — siehe die Notiz im Menuebau.
    void notchRemoveAllRequested();

    // Emitted when user drags a filter edge
    void filterEdgeDragged(int lowHz, int highHz);
    // Emitted when pan center changes (drag, auto-scroll)
    void centerChanged(double centerHz);
    // Emitted when user scrolls to change bandwidth
    void bandwidthChangeRequested(double newBandwidthHz);

    /// Phase 3F Sub-Epic F Task 10: emitted when zoom-state or operator
    /// toggle changes extended-mode. Consumers (RadioModel via MainWindow)
    /// flip the active slice's widebandExtensionRequested property,
    /// which in turn auto-bypasses Alex BPF + enables the wideband
    /// stream (Task 11 wiring already in place).
    void widebandExtensionStateChanged(bool extensionRequested);

    /// Phase 3F Sub-Epic F Task 12: emitted when the operator clicks
    /// in the extended-pan "wing" (outside the DDC listenable island).
    /// Consumer (MainWindow) retunes the active slice's frequency so
    /// the clicked Hz becomes the new DDC center. Only ever fires when
    /// m_extendedMode is true; click-inside-island routes through the
    /// existing frequencyClicked signal (slice retune in place).
    void ddcRetuneRequested(double frequencyHz);

    // Emitted when user-visible dBm range changes via the scale strip
    // (arrow click, drag-pan on strip body, wheel zoom). Args are the
    // new floor (min) and ceiling (max) in dBm.
    // From AetherSDR SpectrumWidget.cpp:1734 [@0cd4559]
    void dbmRangeChangeRequested(float minDbm, float maxDbm);

    // Emitted when CTUN mode changes
    void ctunEnabledChanged(bool enabled);

    // Emitted when m_ddcCenterHz changes (panadapter pan, band jump, etc.).
    // Used by MainWindow to re-push the CTUN slice offset into MaxBin's
    // detector so its scan window tracks the slice even when only the DDC
    // moves.  Without this, panning the spectrum leaves MaxBin scanning
    // bins at the OLD DDC-relative position until the next slice tune.
    void ddcCenterFrequencyChanged(double hz);

    // Plan 4 D9 test seam: fires from drawTxFilterOverlay() after pixel
    // coordinates are computed.  Production code ignores this signal;
    // tests use QSignalSpy to verify paint was triggered with the right band.
    // Same pattern as TxChannel::txFilterApplied (Plan 4 D8).
    void txFilterOverlayPainted(int xLeft, int xRight);

protected:
#ifdef LONGPATH_GPU_SPECTRUM
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
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

    /// Zeigt der Panadapter an dieser Stelle ein eigenes Menue?
    bool hasOwnContextMenuAt(const QPoint& pos) const;
    void wheelEvent(QWheelEvent* event) override;
    // Radereignis -> ganze Schritte (Trackpad/hochaufloesend, 2026-09-27).
    int wheelStepsFor(QWheelEvent* ev);
public:
    // Thetis SnapTune (console.cs:31215-31228): auf das Schrittraster.
    static double snapTuneHz(double freqHz, int stepHz, int numSteps);
protected:
    void leaveEvent(QEvent* event) override;

private:
    // Phase 3F Sub-Epic F Task 6: latest wideband bins per ADC.  Each entry
    // is sized 8192 (kOutputBins from WidebandFftEngine) when populated;
    // empty until the first widebandSpectrumReady arrives.  m_extendedMode
    // gates whether the bins are actually painted; the storage is silent
    // until F polish (T7-T10) wires the paint path.
    QVector<float> m_widebandBinsAdc0;
    QVector<float> m_widebandBinsAdc1;
    bool           m_extendedViewAllowed{true};
    bool           m_extendedMode{false};
    void recomputeExtendedMode();

    // ---- Phase 3Q-8: disconnect overlay state ----
    // The CPU paintEvent path can paint a QPainter overlay, but the GPU
    // (QRhi) path early-returns and refuses QPainter. To work in both modes
    // we use a child QLabel — Qt composites it on top of the QRhi surface.
    Longpath::ConnectionState m_connState{Longpath::ConnectionState::Disconnected};
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
    // measurementPixels() (the UNDENTED copy, design section 8.3) to
    // accumulate (count, linear-sum) of bins below the
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
    void drawWaterfallTimeMarkers(QPainter& p, const QRect& wfRect);
    void paintWaterfallTimeMarkers(QPainter& p, const QRect& wfRect,
                                   const QVector<qint64>& stamps);
    void drawFreqScale(QPainter& p, const QRect& r);
    void drawDbmScale(QPainter& p, const QRect& specRect);
    void drawBandPlan(QPainter& p, const QRect& specRect);

    /// Die Marke des Begleitgeraets: eine senkrechte Linie und die
    /// Frequenz daran. Warm, nicht blau -- der Hausstil haelt Blau
    /// fuer Anfassbares, und diese Zahl ist gemessen, nicht gestellt.
    void drawCompanionMarker(QPainter& p, const QRect& specRect);

    /// Bandstapel-Marken am unteren Rand des Spektrums (Entwurf D1).
    void drawBandStackMarks(QPainter& p, const QRect& specRect);
    QString bandStackMarkAt(int x, int y, const QRect& specRect) const;

    // ── ZF und Antenne (2026-10-28) ─────────────────────────────────
    //
    // Zwei Stellen in diesem Widget zeigen Zahlen, die der Betreiber
    // mit dem Band vergleicht: die Frequenzachse und die Bandplan-
    // Abschnitte. Mit einem Transverter davor liegen beide um den
    // Versatz daneben.
    //
    // Der Zweig wird nur betreten, wenn ein Transverter aktiv ist.
    // Ohne ihn kommt bitgleich dieselbe Zahl zurueck, die hereinkam --
    // kein llround, keine Umrechnung. Das ist Absicht: dieses Widget
    // malt den Panadapter eines Geraets am Mast des Betreibers, und
    // eine Aenderung, die "eigentlich nichts tut", tut im Zweifel doch
    // etwas.
    double rfFuerAnzeige(double ifHz) const;
    double zfFuerAchse(double rfHz) const;
    void drawSquelchLine(QPainter& p, const QRect& specRect);
    void drawTuneGuide(QPainter& p, const QRect& specRect);
    // Rauschboden -> Schwellenvorschlag. Ruft am Ende von
    // processNoiseFloor, mit dem sichtbaren m_nfLerpAverage.
    void updateAutoSquelch(float nfDbm);
    // Zeigerbewegung: Hilfe zeigen und die vier Sekunden neu starten.
    void noteTuneGuideActivity();

    // ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
    // From AetherSDR SpectrumWidget.h:402-413 [@0cd4559]
    void drawTimeScale(QPainter& p, const QRect& wfRect);
    QRect waterfallTimeScaleRect(const QRect& wfRect) const;
    QRect waterfallLiveButtonRect(const QRect& wfRect) const;
    int   waterfallStripWidth() const;
    void  ensureWaterfallHistory();
    void  rebuildWaterfallViewport();
    void  setWaterfallLive(bool live);
    void  appendHistoryRow(const quint8* rowData, qint64 timestampMs);
    int   waterfallHistoryCapacityRows() const;
    int   maxWaterfallHistoryOffsetRows() const;
    int   historyRowIndexForAge(int ageRows) const;
    QString pausedTimeLabelForAge(int ageRows) const;
    void  reprojectWaterfall(double oldCenterHz, double oldBandwidthHz,
                             double newCenterHz, double newBandwidthHz);
    // (clearWaterfallHistory moved to public slots: in sub-epic E task 4 review.)

    // drawVfoMarker walks sliceMarkerGeometry() and hands each entry to
    // drawSliceMarker, which paints one slice's passband fill, filter edges,
    // VFO centre line and triangle. Split in Phase 3F so a pan hosting several
    // slices paints one marker per slice instead of one per pan.
    void drawVfoMarker(QPainter& p, const QRect& specRect, const QRect& wfRect);
    void drawSliceMarker(QPainter& p, const QRect& specRect, const QRect& wfRect,
                         const SliceMarkerGeometry& g);
    /// Der Spektrum-Teil der Bandbreiten-Marke fuer den 3D-Modus, in die
    /// dynamische Schicht gemalt (ueber der deckenden DSS-Flaeche). Tut
    /// ausserhalb von Mode3D nichts. Begruendung in drawSliceMarker.
    void paintPassbandOverSurface(QPainter& p, const QRect& specRect);
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
    //  - Click-to-tune via frequencyClicked(hz) signal (Longpath Hz units;
    //    AetherSDR emits MHz, the call site multiplies by 1e6).
    //  - Cluster badge popup menu with formatted spot lines.
    void drawSpotMarkers(QPainter& p, const QRect& specRect);
    QVector<SpotMarker> mergedMarkers() const;
    void showSpotClusterPopup(const SpotCluster& cluster, const QPoint& globalPos);

    // ---- TNF / notch overlay (design section 8.2) ----
    // Geometry ported unchanged from AetherSDR drawTnfMarkers
    // (src/gui/SpectrumWidget.cpp:13503-13554 [@c6481cbf]): translucent
    // fill over the full spectrum height, diagonal hatch clipped to the
    // notch rect, 1 px edge lines at both boundaries, downward triangle
    // grab handle at the top.  Colours come from Thetis instead
    // (display.cs:386-390, 8691-8722 [v2.10.3.15]).
    void drawNotchMarkers(QPainter& p, const QRect& specRect);
    QColor notchColor(const NotchMarker& n) const;

    // The SINGLE notch geometry source.  Both paint call sites pass this
    // rect, and the interaction layer's pixel hit test builds from it, so
    // hit boxes cannot drift away from the drawn markers.  Reproduces the
    // paint sites' own specRect construction on both render paths through
    // specHFromHeight, which already encodes the GPU/CPU layout split.
    QRect notchSpecRect() const;

    // ---- TNF / notch interaction (design section 7) ----
    // notchMarkerById: linear id lookup over the render-side mirror.
    // notchAtPixel:    pixel-space port of Thetis
    //                  MNotchDB.NotchThatSurroundsFrequencyInBW.
    // notchGrabAt:     edge-vs-centre discrimination for a press.
    // buildNotchContextMenu: right-click menu over a notch marker.
    const NotchMarker* notchMarkerById(int id) const;
    int       notchAtPixel(int x, const QRect& specRect) const;
    NotchGrab notchGrabAt(int id, int x, bool shiftHeld,
                          const QRect& specRect) const;
    void      buildNotchContextMenu(int id, QMenu& menu);

    // ---- Visual notch (design section 8.3) ----
    /// True when this frame's pixels are to be dented: the toggle is on, we
    /// are not transmitting, the master TNF enable is on, and at least one
    /// marker exists. Mirrors the Thetis gate plus the _tnf_active half of
    /// its per-notch _Use flag.
    bool visualNotchWillDent() const;
    /// Subtract the notch skirts from `pixels` in place. Safe on either
    /// plane's array; the caller decides which.
    void applyVisualNotchDent(QVector<float>& pixels) const;
    /// The pristine (undented) spectrum pixels. Consumers that MEASURE
    /// rather than draw read this, never m_renderedPixels.
    const QVector<float>& measurementPixels() const;

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

    // ── Befund 3 von NereusSDRs PureSignal-Diagnose (2026-10-22) ─────────
    //
    // Die Zweiton-Analyse lief bis heute IM Malvorgang, also mit der
    // Bildrate und nur bei Neuzeichnung. analyseImd() loest sie davon:
    // sie laeuft genau einmal je Spektrumsbild, am Ende von
    // updateSpectrumLinear(), wo m_renderedPixels endgueltig ist.
    // drawImdOverlay() liest nur noch das Ergebnis.
    void analyseImd();

    // Befund 6: entwertet die zwischengespeicherte GPU-Overlay-Schicht,
    // in der Marken und Messfeld liegen. Siehe die Erklaerung in der .cpp.
    void entwerteImdSchicht();

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

public:
    // ---- Waterfall colour mapping, as pure functions ----
    //
    // Extracted from dbmToRgb so the arithmetic can be checked without
    // a running radio. It had to be: the two sliders and the AGC margin
    // were computed in two different places, and nothing noticed that
    // the colour-gain slider removed more headroom from the top of the
    // window (13.5 dB at the shipped default) than the AGC margin put
    // there (12 dB). The effective high threshold therefore sat below
    // the loudest display pixel, so the peak always clamped to the
    // palette's last stop — magenta — and on a flat spectrum, where
    // every pixel is the loudest, the whole waterfall did.

    // Decibels the black-level slider adds to the low threshold.
    static float wfBlackLevelOffsetDb(int blackLevel)
    { return static_cast<float>(125 - blackLevel) * 0.4f; }

    // Decibels the colour-gain slider takes off the high threshold.
    static float wfColorGainOffsetDb(int colorGain)
    { return static_cast<float>(colorGain) * 0.3f; }

    // Narrowest window that is still a display. A one-decibel span maps
    // everything above the floor to the top of the palette, which is a
    // solid block of one colour rather than a waterfall.
    static constexpr float kWfMinSpanDb = 6.0f;

    // Span the AGC must leave after the sliders have taken their cut.
    static constexpr float kWfAgcPaletteSpanDb = 20.0f;

    // How far into the loud/quiet end of the real dynamic range the
    // colour-gain / black-level sliders may pull the high/low
    // threshold. Peaks reaching the top colour (or the floor going
    // fully dark) is correct; the palette swallowing the whole picture
    // is not — this is the cap that keeps that from happening, shared
    // by both sliders (mirrors the AGC margin's own symmetry).
    //
    // Was 6.0f. Raised 2026-08-25 on OE5SOS's explicit request
    // ("bitte lockern") after live testing on a real 20m band showed
    // both sliders pinned against the old cap at their commonly-used
    // settings (~55-94 black level, ~64-80 colour gain out of range) —
    // past the cap each slider had no further visible effect at all.
    // A deliberate default change, not something to keep re-tuning
    // silently: touch again only on another explicit ask.
    static constexpr float kWfMaxClipDb = 20.0f;

    // dBm to colour, given already-composed thresholds. No widget state.
    static QRgb waterfallColor(float dbm, float lowDbm, float highDbm,
                               int blackLevel, int colorGain,
                               WfColorScheme scheme);

    // ── Die zwei Hälften davon ───────────────────────────────────────
    //
    // waterfallColor() ist seit dem 2026-08-15 nur noch die Verkettung
    // dieser beiden. Getrennt, weil die Historie die erste Hälfte
    // speichert und die zweite erst beim Zeichnen anwendet — dadurch
    // erreicht ein Wechsel des Farbschemas auch die alten Zeilen.

    /// dBm auf 0..255 in die Farbtabelle. Enthält die Schieberegler,
    /// den Mindestabstand der Schwellen und die NaN-Behandlung.
    // ── Fliesskomma-Fassungen gegen das Banding (2026-08-23) ────────
    //
    // Der Betreiber am 2026-08-23: "anan 10e hat glaube ich nur 14 bit,
    // avelina hat mehr." Er hat damit den Finger auf die richtige
    // Stelle gelegt — je mehr Dynamikumfang das Geraet liefert, desto
    // eher sieht man STUFEN im Wasserfall, wo ein Verlauf sein sollte.
    //
    // Die Ursache lag bei uns: waterfallIntensity quantelt auf
    // quint8 — 256 Stufen ueber die ganze Spanne. Bei 100 dB Fenster
    // sind das 0,4 dB je Stufe, und in einem weichen Verlauf sieht man
    // die Kanten.
    //
    // Yuri EU2AV geht dasselbe Problem mit einem 16-Bit-Renderziel an.
    // Das passt zu SEINEM Aufbau, wo das Tone-Mapping auf der
    // Grafikkarte NACH dem Rendern laeuft. Bei uns entsteht die Farbe
    // auf der CPU und geht als RGBA8 zur Grafikkarte — ein breiteres
    // Renderziel wuerde daran nichts aendern, weil die Quantelung
    // davor passiert.
    //
    // Also an derselben Stelle ansetzen: in Fliesskomma rechnen und
    // beim Schreiben in 8 Bit DITHERN. Das ist der uebliche Weg und
    // kostet eine Addition je Bildpunkt.
    static float waterfallIntensityF(float dbm, float lowDbm, float highDbm,
                                     int blackLevel, int colorGain);
    // Beschriftung einer Frequenzmarke (MHz) mit den Nachkommastellen, die
    // der Markenabstand stepHz braucht (25 kHz -> 3, 50 kHz -> 2, 100 kHz -> 1).
    static QString freqScaleLabel(double hz, double stepHz);
    /// Teilstrich-Abstand fuer Raster und Skala (2026-11-08), siehe .cpp.
    static double frequenzSchritt(double bandwidthHz, int breitePx);
    /// Der Wert fuer Wasserfall-Zeilenpunkt x, wenn `scale` Quellpunkte
    /// (Geraete-Pixel der Pipeline) auf einen Zeilenpunkt (logisches
    /// Pixel) fallen -- auf Retina zwei --, zusammengefasst nach der Regel
    /// des Wasserfall-Detektors (Peak: Maximum, Average/RMS: mittlere
    /// Leistung, Sample: erster Punkt). Beim Aufweiten (scale < 1) der
    /// eine naechste Punkt. Siehe pushWaterfallRow().
    static float wfRowSourceReduce(const QVector<float>& src, int x, float scale,
                                   SpectrumDetector detector);
    // lowColor: Thetis display.cs:2516/6764-6787 waterfall_low_color,
    // honoured for WfColorScheme::Enhanced only (see .cpp). A default
    // parameter, not an instance read, so this stays the pure function
    // tst_waterfall_banding.cpp already calls without a live SpectrumWidget.
    static QRgb waterfallColorForIntensityF(float level, WfColorScheme scheme,
                                            QColor lowColor = Qt::black);

    static quint8 waterfallIntensity(float dbm, float lowDbm, float highDbm,
                                     int blackLevel, int colorGain);

    /// 0..255 auf eine Farbe des gewählten Schemas.
    static QRgb waterfallColorForIntensity(quint8 level, WfColorScheme scheme,
                                           QColor lowColor = Qt::black);

private:

    // ---- FFT pipeline state ----
    // Single Thetis-faithful pipeline: linear-power FFT bins -> visible
    // slice -> detector -> avenger -> dBm display pixels.  Spectrum and
    // waterfall keep separate detector + avenger state per WDSP per-plane
    // model (each ANALYZER_INFO[] entry has its own DetType + AvMode).
    QVector<float> m_fullLinearBins;       // FFTEngine input cache (|X[k]|²)
    QVector<float> m_displayLinearPixels;  // spectrum detector output (linear)
    QVector<float> m_renderedPixels;
    /// Zeitpunkt der letzten Bandmeldung (alle 30 s, siehe
    /// updateSpectrumLinear).
    qint64 m_bandBerichtZuletztMs{0};       // spectrum avenger output (dBm)
    QVector<float> m_wfDisplayLinearPixels; // waterfall detector output (linear)
    QVector<float> m_wfRenderedPixels;     // waterfall avenger output (dBm)
    int m_displayWidthOverrideForTest{0};  // 0 = off; see setDisplayWidthOverrideForTest()

    // ---- 3D stacked-trace view (DssRenderer) ----
    // Fed from m_renderedPixels (see updateSpectrumLinear) only while
    // m_renderMode is Mode3D -- no cost to sessions that never enable it.
    SpectrumRenderMode m_renderMode{SpectrumRenderMode::Mode2D};
    bool m_dss3dTrueScale{true};   // see setDss3dTrueScale
    DssRenderer m_dss;
    bool m_dss3dNeedsUpload{false};  // set on pushRow, cleared after GPU upload

    // Equivalent Noise Bandwidth of the current FFT window, in bins.
    // Refreshed every frame via the windowEnb arg on fftReadyLinear so
    // the detector's invEnb scaling stays in lock-step with the bins it
    // just received.  No setter coordination needed.
    double m_fftWindowEnb{1.0};
    double m_lastDbmOffset{0.0};

    // Per-channel WDSP-style frame averagers.  See SpectrumAvenger.h for
    // the av_mode wire-format mapping (-1 peak / 0 none / 1 recursive /
    // 2 window / 3 log-recursive); analyzer.c:464-554 [v2.10.3.13] is the
    // verbatim port.
    Longpath::SpectrumAvenger m_spectrumAvenger;
    Longpath::SpectrumAvenger m_waterfallAvenger;

    // ---- Frequency range ----
    double m_centerHz{14225000.0};    // 14.225 MHz default
    // Ob je ein Ausschnitt GESETZT wurde — die Vorgabe oben zaehlt
    // nicht. Die Kopfzeile des Panadapters haengt daran (2026-10-27).
    bool   m_ausschnittGesetzt{false};
    double m_bandwidthHz{192000.0};   // 192 kHz default (P1 base sample rate)
    double m_ddcCenterHz{14225000.0};   // DDC hardware center frequency
    double m_sampleRateHz{768000.0};    // DDC sample rate
    bool   m_kiwiDisplaySource{false};  // Stufe 6: Geraet oder KiwiSDR
    QString m_persistKey;               // leer = nichts merken
    // Nur waehrend updateKiwiSpectrumDbm gesetzt. Ohne dieses
    // Merkmal wuerde die Sperre gegen fremde Bins auch den
    // Kiwi selbst abweisen — er geht ja durch dieselbe Tuer.
    bool   m_feedingFromKiwi{false};

    // ---- Display range ----
    // From Thetis display.cs:1743-1754. Init values must match the
    // ship defaults in loadSettings() (SpectrumWidget.cpp ~line 392) —
    // any divergence shows up if a code path reads these before
    // loadSettings runs, and the slider in SetupDialog briefly shows
    // the stale value. Calibrated 2026-04-30 against a residential
    // HF noise floor; see loadSettings comment for rationale.
    // ── Der dargestellte Bereich ─────────────────────────────────────
    //
    // War −48 / 68, also −116 bis −48. Zu eng und zu hoch: bei 16384
    // Bins auf 192 kHz sind 11,7 Hz je Bin, thermisch also
    // −174 + 10·log10(11,7) ≈ −163 dBm, und mit der Rauschzahl des
    // Empfaengers landet der Flur bei etwa −148. Der lag damit UNTER dem
    // unteren Bildrand, waehrend die 68 dB darueber vom Rauschen
    // ausgefuellt wurden.
    //
    // Jetzt −30 bis −190. Der Flur sitzt damit auf gut einem Viertel der
    // Hoehe — ein schmales Band unten statt einer gefuellten Flaeche.
    //
    // Bewusst nicht die Thetis-Werte (−40 / −140): mit einem Boden bei
    // −140 faellt derselbe Flur aus dem Bild. Eine Anzeigevorgabe gehoert
    // dem Betreiber, nicht console.cs.
    float  m_refLevel{kDefaultRefLevelDbm};     // top of display (dBm)
    float  m_dynamicRange{kDefaultRangeDb};     // bottom = refLevel - dynamicRange

    // ---- Waterfall ----
    QImage m_waterfall;               // ring buffer (Format_RGB32)
    int    m_wfWriteRow{0};

    // ── Waterfall scrollback (sub-epic E) ─────────────────────────────────
    // From AetherSDR SpectrumWidget.h:493-502 [@0cd4559]
    // Intensität, nicht Farbe — und blockweise belegt. Siehe
    // gui/WaterfallHistoryBuffer.h und docs/design/WASSERFALL-AETHER.md.
    WaterfallHistoryBuffer m_waterfallHistory;
    /// Puffer für die gerade berechnete Zeile. Als Feld, weil
    /// pushWaterfallRow() zwanzigmal in der Sekunde läuft und eine
    /// Neubelegung je Zeile reine Verschwendung wäre.
    QVector<quint8> m_wfRowIntensity;
    // Die Zeile auf Bildbreite verdichtet (Maximum je Zeilenpunkt), vor
    // Schwellen und Farbe -- siehe pushWaterfallRow().
    QVector<float> m_wfRowReduced;
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
    // Longpath-side enhancement — see plan §authoring-time #1.
    qint64          m_waterfallHistoryMs{kDefaultWaterfallHistoryMs};

    // Debounce timer for ensureWaterfallHistory() during rapid resize / slider drag.
    // From AetherSDR SpectrumWidget.h:559 [@2bb3b5c]
    // (debounce timer added by unmerged AetherSDR PR #1478 — see plan §authoring-time #2)
    QTimer*         m_historyResizeTimer{nullptr};

    // Coalesces buffer re-allocation during a resize drag (2026-08-16).
    // A drag delivers a resizeEvent every ~30-40 ms, and each intermediate
    // size used to re-allocate the waterfall image and both overlay images
    // — 45 re-allocations observed for one drag to one final size, 44 of
    // them for a size the operator never stopped at.
    //
    // While this timer is running the three buffers keep their previous
    // dimensions and the GPU stretches the last good overlay over the new
    // widget rect.  The re-allocation happens once, when the size has held
    // still for kResizeSettleMs.
    //
    // Deliberately NOT applied to the first allocation: m_waterfall starts
    // null, and deferring that would leave nothing to draw.  resizeEvent
    // calls applyResizeSettled() straight through in that case.
    QTimer*         m_resizeSettleTimer{nullptr};
    static constexpr int kResizeSettleMs = 80;

    QTimer          m_displayTimer;
    bool            m_hasNewSpectrum{false};

    // ---- Waterfall display controls ----
    // From AetherSDR SpectrumWidget defaults + Thetis display.cs:2522-2536
    WfColorScheme m_wfColorScheme{WfColorScheme::Default};
    int    m_wfColorGain{45};         // 0-100
    int    m_wfBlackLevel{104};       // 0-125 — keep in sync with loadSettings ship default
    bool   m_overlayPanelExpanded{true};
    // Waterfall uses its own dBm range (narrower than spectrum for better contrast).
    // Persistent user-configured thresholds (saved/loaded as DisplayWfHigh/LowLevel).
    // Ship defaults — keep in sync with loadSettings (SpectrumWidget.cpp)
    // From Thetis display.cs:2522 + 2536 [v2.10.3.13] waterfall_high_threshold /
    // waterfall_low_threshold (the persisted user values, distinct from
    // _RX1waterfallPreviousMinValue runtime AGC tracking state).
    float  m_wfHighThreshold{-62.0f};
    float  m_wfLowThreshold{-122.0f};

    // Render-active mirror, equivalent to Thetis's per-render
    // local high_threshold / low_threshold at display.cs:6575/6584/6590
    // [v2.10.3.13]. AGC / NF-AGC / Clarity override these; the
    // persistent fields above are never touched by runtime overrides.
    // Issue #230 fix.
    float  m_wfActiveHighThreshold{-62.0f};
    float  m_wfActiveLowThreshold{-122.0f};

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
    // 22 % an der Kurve, 0 an der Grundlinie — docs/design/HAUSSTIL.md
    // §Weiche Uebergaenge: "duenne Linie (1,6 px) plus Verlauf darunter,
    // 22 % -> 0 zur Grundlinie. Gewicht ohne Lautstaerke."
    float  m_fillAlpha{0.22f};
    bool   m_panFill{true};

    // ---- Phase 3G-8 commit 3: spectrum renderer state ----

    AverageMode m_averageMode{AverageMode::Logarithmic};
    // Spectrum + waterfall averaging time constants in milliseconds, with
    // per-side back-multiplier alphas computed via Thetis math:
    //   α = exp(-1 / (fps × τ))  [specHPSDR.cs:358 / :374, v2.10.3.13]
    // Defaults match Thetis (setup.cs udDisplayAVGTime_ValueChanged = 30 ms,
    // udDisplayAVTimeWF_ValueChanged = 120 ms).
    int         m_spectrumAverageTimeMs{350};
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
    // Longpath-original — distinct trace colour so the peak trace stays
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
    // 2026-05-26 KG4VCF perf polish: pre-rendered blob marker pixmap.
    // paintPeakBlobs blits this instead of calling QPainter::drawEllipse
    // each blob; drawEllipse went through Qt's parallel raster span
    // path and the main thread blocked in QLatch::waitInternal waiting
    // for QThreadPool workers (starved under build load).  Pixmap blit
    // is a memcpy-style operation that skips the raster engine.
    // Rebuilt when m_peakBlobColor changes.
    QPixmap m_blobMarkerPixmap;
    void rebuildBlobMarkerPixmap();

    // 2026-05-26 KG4VCF perf polish: pre-allocated scratch vectors
    // for the CPU spectrum / peak-hold paint paths.  Previously
    // drawSpectrum() allocated `QVector<QPointF> points(n)` (~16 KB)
    // and `QVector<QPointF> peakPoints(n)` (~16 KB) per paint.  GPU
    // path is hot on macOS so these don't run there, but on CPU
    // fallback they are the dominant per-paint allocation source.
    // Hoisted to members; drawSpectrum resizes (no-op when same n).
    QVector<QPointF> m_specPointsScratch;
    QVector<QPointF> m_specPeakPointsScratch;
    // QPainterPath is also re-constructed per paint in the fill +
    // peak-hold blocks; caching as a member + clear()-and-reuse
    // amortises its internal QVector allocation.
    QPainterPath m_specFillPathScratch;
    QPainterPath m_specPeakPathScratch;

    // 1.0f: operator decision (Martin, 2026-08-26), after live-testing the
    // GPU ribbon-line fix -- matches the Setup slider's own minimum (1-3 px,
    // DisplaySetupPages.cpp), which the old 1.6f default couldn't even
    // reach (the slider casts to int).
    float       m_lineWidth{1.0f};
    bool        m_traceSoftEdge{true};    // AN ab Werk, siehe setTraceSoftEdge
    bool        m_traceHalo{false};       // AUS ab Werk, siehe setTraceHalo
    // Durchlass und Bandbreite (2026-11-08), siehe setPassbandLabelShown.
    bool        m_zeigeBandbreite{true};
    bool        m_zeigeKlammern{true};
    bool        m_zeigeLuecken{true};
    int         m_lupenModus{1};            // 0 aus, 1 an (immer sichtbar)
    bool        m_kanteFein{false};         // Shift beim Ziehen: ohne Rast
    Durchlass::Haltung         m_klammerHaltung;
    float                      m_klammerBoden{0.0f};
    QVector<Durchlass::Luecke> m_luecken;
    int                        m_besteLuecke{-1};
    qint64                     m_durchlassZuletztMs{0};
    QVector<QPair<QRectF, double>> m_lueckenKlick;   // Flaeche, ZF in Hz
    QVector<float>             m_lupeGlatt;
    int                        m_lupeErster{-1};
    Durchlass::Hoehe           m_lupeHoehe;     // eigener Hoehenmassstab
    double                     m_eichZielHz{0.0};   // Einmessen: Suchmitte (Geraet)
    bool                       m_bandLageMelden{false};
    qint64                     m_bandLageZuletztMs{0};
    void meldeBandLage();
    QVector<double>            m_eichSumme;         // gemittelte Leistung je Bin
    int                        m_eichBilder{0};
    void fuehreEichungNach();
    bool                       m_lupeHoeheDa{false};
    bool                       m_sStufenSkala{false};   // S-Stufen statt dBm
    // Schaerfe (2026-11-08), siehe setWaterfallDevicePixels. AN ab Werk.
    // Vor jedem GPU-#ifdef, weil die Zugriffsmethoden ungeschuetzt sind.
    bool        m_wfDevicePixels{true};
    bool        m_gridHairline{true};
    float       m_traceEdgePx{0.5f};      // war Kurve::kKantePixel (1,0)
    // Wie viele Streifen im jeweiligen Puffer stehen. Gesetzt beim Bauen
    // der Scheitelpunkte, gelesen beim Zeichnen -- zwei Stellen, eine
    // Zahl, sonst zeichnet die GPU aus einem halb gefuellten Puffer.
    int         m_lineStripCount{1};
    int         m_peakStripCount{1};
    // ── Warum das zwei Fahnen sind und nicht eine ────────────────────
    //
    // Es war eine, und sie bedeutete auf den beiden Malwegen etwas
    // Verschiedenes:
    //
    //   CPU (drawSpectrum) — Verlauf unter der Kurve statt flacher
    //                        Fuellung
    //   GPU (render)       — Regenbogen-Einfaerbung der Kurve,
    //                        blau/cyan/gruen/gelb/rot
    //
    // Dieselbe Einstellung, dasselbe gespeicherte Feld, zwei Ergebnisse,
    // je nachdem ob der Rechner den GPU-Pfad kann. Als am 2026-08-15 die
    // Vorgabe auf "an" ging, um den Hausstil-Verlauf zu bekommen, haette
    // das auf jedem GPU-Rechner stattdessen die Kurve bunt gemacht.
    // Gerettet hat nur, dass beim Betreiber ein gespeichertes "False"
    // darueber lag — die Fahne war also seit jeher eine Falle, sie hatte
    // bloss noch niemand ausgeloest.
    //
    // Jetzt zwei Begriffe, weil es zwei Sachen sind. Wer hier einen
    // dritten Malweg ergaenzt, bedient beide oder keinen — aber nicht
    // einen davon unter dem Namen des anderen.
    //
    //   m_gradientEnabled  die Fuellung unter der Kurve verlaeuft
    //   m_heatmapEnabled   die Kurve wird nach Pegel eingefaerbt
    //
    // Die Fuellung ist der Hausstil und steht an. Der Regenbogen kommt
    // aus AetherSDR, macht Farbe zur groessten Flaeche im Fenster und
    // steht aus.
    bool        m_gradientEnabled{true};
    bool        m_heatmapEnabled{false};

    // Ported from Thetis Display.RX1DisplayCalOffset (display.cs:1372).
    float       m_dbmCalOffset{0.0f};

    // ---- Phase 3G-8 commit 4: waterfall renderer state ----

    bool  m_wfAgcEnabled{true};
    bool  m_clarityActive{false};     // Phase 3G-9c: suppresses legacy AGC when Clarity drives thresholds
    // Clarity-Schwellen samt ihrem Bin-Rauschboden, und der geglaettete
    // Rauschboden der Wasserfall-Bildpunkte (Verankerung, 2026-09-25).
    float m_clarityLow{std::numeric_limits<float>::quiet_NaN()};
    float m_clarityHigh{std::numeric_limits<float>::quiet_NaN()};
    float m_clarityFloorDbm{std::numeric_limits<float>::quiet_NaN()};
    float m_wfPixelFloorDbm{std::numeric_limits<float>::quiet_NaN()};
    NoiseFloorEstimator m_wfPixelFloorEstimator;   // 30. Perzentil wie Clarity
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
    int               m_wfTimeMarkerSec{0};

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

    // 2026-05-25 KG4VCF bench fix: timer-driven waterfall row push.
    // Decouples row push cadence from FFT arrival cadence so network-
    // burst FFT delivery (multiple FFTs in 10 ms then nothing for
    // 50 ms) does not produce visible scroll stutter.  FFT arrivals
    // overwrite m_pendingWfPixelsDbm; the ticker fires at
    // m_wfUpdatePeriodMs and the consumer slot consumes the latest
    // cached value.
    //
    // The ticker lives on its own thread (m_waterfallTickerThread) so
    // any main-thread delay (focus event, layout pass, system
    // notification) cannot delay the tick firing -- only the
    // queued-slot delivery into the main thread's event queue.  This
    // is the "first-class waterfall" option-A fix from the 2026-05-25
    // bench session.
    QVector<float>     m_pendingWfPixelsDbm;
    bool               m_pendingWfPixelsDbmDirty{false};
    /// Kommt der Wasserfall von der Maschine (Flex-Kachel)? Dann
    /// schweigt der eigene, aus der FFT abgeleitete Weg — zwei
    /// Schreiber auf demselben Bild gaeben ein Flackern aus zwei
    /// Takten, und der Takt der Maschine ist der richtige.
    bool               m_fremdeWasserfallquelle{false};
    qint64             m_fremdeWasserfallZuletztMs{0};
    /// Nachgefuehrter Rauschteppich der Kachel, in ihrer eigenen
    /// Intensitaetseinheit. NaN = noch nicht geschaetzt.
    float              m_fremderTeppich{std::numeric_limits<float>::quiet_NaN()};
    // Laufendes Mittel der Kachelzeilen (zeitliche Mittelung des
    // fremden Wasserfalls, 2026-10-23). Haelt die Groesse der
    // Kachelbreite; ein Wechsel setzt es zurueck.
    QVector<float>     m_fremdeWfMittel;
    qint64             m_fremdeWfMittelMs{0};
    qint64             m_kachelBerichtZuletztMs{0};
    double             m_fremdeWfMittelTiefHz{0.0};
    double             m_fremdeWfMittelHochHz{0.0};
    QThread*           m_waterfallTickerThread{nullptr};
    WaterfallTicker*   m_waterfallTicker{nullptr};

    // 1 Hz overlay repaint tick for the waterfall timestamp; started on
    // demand when the user selects a non-None timestamp position.
    QTimer* m_wfTimestampTicker{nullptr};

    // ---- Phase 3G-8 commit 5: grid / scales renderer state ----

    bool  m_gridEnabled{true};
    int   m_gridStepDb{0};      // 0 = automatisch
    bool  m_showZeroLine{false};
    bool  m_showFps{false};
    bool  m_dbmScaleVisible{true};  // right-edge dBm strip; false → spectrum fills full width
    bool  m_showCursorFreq{true};   // B8 Task 21: cursor frequency readout; default on
    FreqLabelAlign m_freqLabelAlign{FreqLabelAlign::Center};

    Longpath::BandPlanManager* m_bandPlanMgr{nullptr};   // non-owning

    // Begleitgeraet: wo es steht (in der ZF) und wie es heisst.
    bool    m_companionShown{false};
    qint64  m_companionIfHz{0};
    QString m_companionLabel;

    // Bandstapel-Marken (setBandStackMarks).
    QVector<BandStackMark> m_bandStackMarks;
    int                          m_bandPlanFontSize{6};   // 0 = off; AetherSDR default
    bool                         m_squelchLineVisible{false};
    double                       m_squelchDbm{-150.0};
    class QTimer*                m_squelchHideTimer{nullptr};

    // Vorgabe ein — Begruendung oben bei den Settern.
    bool                         m_extendedFrequencyLine{true};
    bool                         m_extendedPassband{true};

    bool                         m_autoSquelchEnabled{false};
    // 10 dB wie AetherSDR (SpectrumWidget.h:1414 m_autoSqlMarginDb{10}).
    int                          m_autoSqlMarginDb{10};
    // Sperre gegen Wiederholung: nur senden, wenn sich der Vorschlag um
    // mindestens 1 dB bewegt hat. AetherSDR vergleicht dafuer die
    // ganzzahlige Stufe (m_lastAutoSquelchLevel); in dBm braucht es eine
    // ausdrueckliche Schwelle, sonst faehrt der Wert bei jedem Rahmen
    // ueber die Verdrahtung.
    double                       m_lastAutoSqlDbm{
        std::numeric_limits<double>::quiet_NaN()};

    bool                         m_tuneGuideEnabled{false};
    bool                         m_tuneGuideShowing{false};
    class QTimer*                m_tuneGuideHideTimer{nullptr};

    QColor m_wfLowColor{Qt::black};              // Thetis display.cs:2516 default
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
    int    m_wheelPixelAccum{0};
    int    m_wheelAngleAccum{0};
    qint64 m_lastWheelStepMs{0};

    int    m_panIndex{0};            // for per-pan settings keys

    // ---- VFO flag widgets ----

    // Which slice's flag stays on top -- see setFrontSliceIndex(). -1 = no
    // pin; updateVfoPositions() falls back to its own ascending-index order.
    int m_frontSliceIndex{-1};

    // Re-applies the m_frontSliceIndex pin. Called by setFrontSliceIndex()
    // for an immediate effect, and again at the end of every
    // updateVfoPositions() pass, because that loop unconditionally raises
    // every visible flag once per frame and would otherwise undo the pin on
    // the very next frame.

    // ---- CTUN mode ----
    bool   m_ctunEnabled{true};  // true = SmartSDR-style (pan independent of VFO)
    enum class VfoOffScreen { None, Left, Right };
    VfoOffScreen m_vfoOffScreen{VfoOffScreen::None};
    /// Siehe mitteFolgtDemVfo(): nur waehrend einer EIGENEN Bewegung
    /// des Fensters gesetzt -- Randscrollen (2026-10-30) sowie Zoomen
    /// und recenterOnVfo (2026-11-02). Ausserhalb dieser Meldungen
    /// steht sie auf false, damit ein echter Zug ein Zug bleibt.
    bool         m_mitteFolgtDemVfo{false};
    void drawOffScreenIndicator(QPainter& p, const QRect& specRect, const QRect& wfRect);

    // ---- Mouse tracking overlay (QRhiWidget macOS workaround) ----
    QWidget* m_mouseOverlay{nullptr};

    // ---- Overlay menu ----
    SpectrumOverlayMenu* m_overlayMenu{nullptr};

    // Finding #3, rotor bench 2026-08-11: after ANY spot-menu action
    // ("Tune to", "Turn rotor") the pan overlay menu opened at the
    // same position — a re-delivered press reaching the native QRhi
    // surface once the QMenu goes away. Two-layer fix: the spot and
    // notch menus are now shown async via popup() (no nested exec()
    // inside mousePressEvent), and every right-press arriving within
    // kContextMenuReplayGuardMs of a context-menu close is swallowed
    // before it can reach the overlay-menu fallback.
    qint64 m_contextMenuClosedMs{0};
    static constexpr qint64 kContextMenuReplayGuardMs = 250;

    // ---- Spot overlay state (Phase 3J-2 Task E1) ----
    // Backing store + per-frame click-rect / cluster vectors. Defaults
    // match AetherSDR src/gui/SpectrumWidget.h:634-651 [@0cd4559].
    QVector<SpotMarker>  m_spotMarkers;
    QVector<SpotHitRect> m_spotClickRects;
    QVector<SpotCluster> m_spotClusters;
    // Der erste Klick eines Doppelklicks auf einen Spot stimmt ab. Wo das
    // Bild der Abstimmung folgt (TS-590, jedes Geraet ohne CTUN), steht
    // der Spot danach in der Mitte -- unter der Maus liegt leere Flaeche
    // oder ein ANDERER Spot. Der Doppelklick nimmt darum das Rufzeichen
    // vom ersten Klick (Martin am 12.11.: „hat nicht funktioniert").
    QString m_spotKlickRufzeichen;
    QPoint  m_spotKlickPos;
    double  m_spotKlickHz{0.0};
    qint64  m_spotKlickMs{0};
    bool   m_showSpots{true};
    // Vorgabe aus: das Merkmal ist neu, es gibt keinen Istzustand zu
    // erhalten (anders als bei der Verlaengerung in den Wasserfall).
    bool   m_showSignalHistory{false};
    bool   m_showSignalHistoryQrm{false};
    QVector<SpotMarker> m_signalHistoryMarkers;
    Longpath::SignalHistoryStore m_signalHistory;
    qint64 m_shLastRunMs{0};
    qint64 m_shLastRebuildMs{0};
    float  m_shRunRateEwma{10.0f};   // gemessene Aufrufrate des Erkenners
    int    m_spotFontSize{16};
    int    m_spotMaxLevels{3};
    int    m_spotStartPct{50};       // % down from top of spectrum
    // ── Ein Ton für alle Spots, als Vorgabe ──────────────────────────
    //
    // OE5SOS, 2026-08-15: „zu viele Farben, die nicht gut zueinander
    // passen." Am Bildschirm nachgezählt: neun Farbfamilien in einem
    // Fenster, und die lautesten davon waren die Rufzeichen — gelb aus
    // dem DX-Cluster, grün aus POTA, orange aus FreeDV, dazu die
    // DXCC-Einfärbung. Jede Quelle bringt ihre eigene Farbe mit, und
    // niemand hat sie aufeinander abgestimmt, weil niemand sie
    // zusammen gesehen hat.
    //
    // Der Schalter dafür gab es schon, er stand nur auf aus. Jetzt an:
    // ein Ton für alle Spots, und wer die Quelle an der Farbe erkennen
    // will, schaltet ihn im Spot-Hub wieder ab.
    //
    // Das LÖSCHT keine Information: welche Quelle ein Spot hat, steht
    // weiter im Marker und im Spot-Hub. Es drängt sie nur nicht mehr
    // in neun Tönen über das halbe Spektrum.
    bool   m_spotOverrideColors{true};
    bool   m_spotOverrideBg{true};
    // Der eine Ton. Bernstein statt Knallgelb — dieselbe Familie wie
    // die Messwerte im S-Meter und die Spitzen im Wasserfall, weil ein
    // Spot dasselbe sagt: hier ist etwas. Wird in loadSettings() über
    // die Rolle „measured" aus dem Theme geholt.
    QColor m_spotColor{QColor(Style::kAmberText)};
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

    // ---- TNF / notch overlay state (design section 8.1) ----
    // Main-thread only.  Both the paint path and the interaction layer run
    // there, so plain members rather than atomics; NotchModel is the
    // authoritative store and this is a render-side mirror of it.
    // Shape mirrors AetherSDR src/gui/SpectrumWidget.h:1608-1609 and
    // :1648 [@c6481cbf].
    QVector<NotchMarker> m_notchMarkers;
    // Master TNF enable, default OFF.  Matches NotchModel::globalEnabled()
    // (also false) and both upstreams: Thetis ships chkTNF unchecked, and
    // WDSP creates the notch database with master run 0
    // (third_party/wdsp/src/RXA.c:87).  A widget default of true would
    // paint every marker in the active colour for the one frame between
    // construction and the first MainWindow::refreshPanNotchMarkers push.
    bool   m_notchGlobalEnabled{false};
    // 100 Hz on this tree: nc = 4096 at a 48 kHz dsp rate through the
    // wintype-0 arm of min_notch_width (third_party/wdsp/src/nbp.c:88,
    // 1600.0 / (nc / 256) * (rate / 48000)).  Overwritten by
    // setNotchMinWidthHz once a channel is open.
    double m_notchMinWidthHz{100.0};

    // ---- Visual notch state (design section 8.3) ----
    // From Thetis display.cs:1070 [v2.10.3.15]: m_bShowVisualNotch = false.
    bool m_visualNotchEnabled{false};

    // Pristine mirror of m_renderedPixels, populated ONLY on the frames that
    // actually dent so the default-off path costs nothing. Thetis keeps a
    // permanent second array instead (current_display_data_copy, filled by
    // the memcpy at display.cs:5046-5049 and handed to the render loop at
    // :5055 [v2.10.3.15]) because its analyzer hands one over for free.
    // measurementPixels() falls back to m_renderedPixels when this is empty
    // or stale-sized.
    QVector<float> m_undentedPixels;

    // From Thetis display.cs:4778 [v2.10.3.15]: float fAttenuation = 100f;
    static constexpr float kNotchDentAttenuationDb = 100.0f;

    // From Thetis display.cs:8680 [v2.10.3.15]:
    //   dNewWidth += 20; // fudge factor to align better with spectrum notch
    static constexpr double kNotchDentFudgeHz = 20.0;

    // Written by the interaction layer (design section 7.4); drive the
    // Chartreuse highlight and the hover popup respectively.  Declared
    // here rather than in the interaction layer because notchColor() reads
    // both.
    int    m_selectedNotchId{-1};
    int    m_hoveredNotchId{-1};

    // ---- TNF / notch drag state (design section 7.2) ----
    // From Thetis console.cs:33284-33288 [v2.10.3.15], the drag-state
    // block.  m_notchGrab folds upstream's m_bDraggingNotch /
    // m_bDraggingNotchBW / m_BDragginNotchBWRightSide trio into one value;
    // m_notchDragStartX is _drag_notch_start_point.X (the press pixel, the
    // delta is worked in the move handler); m_notchDragStartData is
    // drag_notch_start_data, which carries the notch WIDTH for an edge
    // drag and the notch CENTRE for a whole-notch drag.  m_nNotchRX has no
    // analogue: the pan the drag started on is the widget receiving the
    // events.
    //NOTCH MW0LGE  [original section marker from console.cs:33283]
    NotchGrab m_notchGrab{NotchGrab::None};
    int       m_notchDragStartX{0};
    double    m_notchDragStartData{0.0};

    // ── QStaticText label cache ──────────────────────────────────────────
    // Pre-shaped (HarfBuzz-run-once) labels for the high-rate paint
    // loops in drawDbmScale and drawFreqScale.  Without this, each
    // paint reshapes every label string from scratch — AetherSDR
    // measured ~5% main-thread CPU on a single DSP curve widget from
    // shapeText, and our SpectrumWidget has 23+ drawText sites firing
    // at 20 fps.  Cache keys are the formatted label strings ("-100",
    // "14.230" etc.), values are QStaticText with AggressiveCaching.
    // Strings repeat heavily in steady state (dBm scale labels never
    // change between paints; freq labels cycle through a small set as
    // the user pans), so the cache hits >99% after the first paint.
    // mutable because drawDbm/Freq are non-const but the cache is hidden
    // state — keeping the helper signatures clean.  Lifetime: widget
    // lifetime; no invalidation needed at this font size since both
    // strips set a fixed pointSize per-paint.
    // Inspired by AetherSDR [@3503ae98] PR #2556 perf(gui): cache axis
    // labels as QStaticText.
    mutable QHash<QString, QStaticText> m_dbmLabelCache;
    mutable QHash<QString, QStaticText> m_freqLabelCache;

    // ---- Task 2.3: Spectrum text overlay state ----

    // (m_showMHzOnCursor retired in 2026-05 — see formatCursorFreq comment.)

    // From Thetis setup.cs:7061 [v2.10.3.13] lblDisplayBinWidth
    bool m_showBinWidth{false};

    // From Thetis display.cs:2304 [v2.10.3.13] m_bShowNoiseFloorDBM (default true in Thetis;
    // Longpath defaults off so the overlay is opt-in rather than on by default)
    bool            m_showNoiseFloor{false};
    OverlayPosition m_noiseFloorPosition{OverlayPosition::BottomLeft};

    // NF render colours — From Thetis display.cs:2316-2337 [v2.10.3.13]:
    //   private static Color noisefloor_color      = Color.Red;
    //   private static Color noisefloor_color_text = Color.Yellow;
    // Defaults match Thetis exactly so the line is visually distinct from
    // grid text (yellow) and from the spectrum trace.  Made configurable
    // via setNoiseFloorColor / setNoiseFloorTextColor when Setup wires them.
    // Default line + text colour — Longpath-tweaked from Thetis red/yellow
    // (display.cs:2316/2329 [v2.10.3.13]).  Thetis renders against a
    // configurable trace colour; Longpath's stock spectrum trace is cyan
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
    // Lightened from Qt::gray (160,160,164) to #c4c4c9 so the swatch and
    // the rendered overlay both stand out against Longpath's dark UI
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
    // Longpath-original: applied as global default; RX1-scope dropped.
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
    // Upstream tags preserved: //MW0LGE (from cited upstream lines) [v2.10.3.15]
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
    // displayduplex defaults to true in Longpath — the panadapter stays
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

    // Ergebnis der letzten Zweiton-Analyse, abgelegt von analyseImd() und
    // gelesen von drawImdOverlay(). Als nackte Zahlen statt als
    // ImdOverlay::Maximum, damit ImdOverlay.h nicht in diese Kopfdatei
    // muss (gleiche Begruendung wie beim Zeiger darueber).
    // Reihenfolge: f0L, f0U, IMD3L, IMD3U, IMD5L, IMD5U.
    static constexpr int kImdProductCount = 6;
    bool  m_imdAnalysisValid{false};
    int   m_imdAnalysisBins{0};
    float m_imdProductDbm[kImdProductCount]{};
    int   m_imdProductBin[kImdProductCount]{};

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

    // ── Bewusst AUSSERHALB des GPU-Gates ────────────────────────────
    //
    // Die folgenden Felder sind einfache Daten, keine QRhi-Objekte, und
    // ihre Zugriffsfunktionen weiter oben stehen ebenfalls ungeschuetzt
    // (backgroundImagePath(), backgroundOpacity(), backgroundBrightness(),
    // backgroundFillColor(), waterfallBackgroundFillColor(),
    // visibleBinCountForTest()). Lagen sie im #ifdef, liess sich Longpath
    // mit -DLONGPATH_GPU_SPECTRUM=OFF ueberhaupt nicht uebersetzen: sechs
    // Zugriffe auf Member, die es in dieser Uebersetzung nicht gibt.
    //
    // Genau daran ist der ARM-Linux-Bau von v0.6.3-rc1 gescheitert — dort
    // fehlten die Vulkan-Header, CMake hat QRhi abgeschaltet (CMakeLists
    // :434-467), und der CPU-Pfad, den Zeile 417 ausdruecklich anbietet,
    // baute nicht. Wer hier etwas hinzufuegt, das die Zugriffsfunktionen
    // sehen, laesst es ausserhalb des Gates.
    // ── Frei waehlbarer Hintergrund ──────────────────────────────────
    //
    // Port aus AetherSDR SpectrumWidget: setBackgroundImage /
    // setBackgroundOpacity / setBackgroundFillColor
    // (SpectrumWidget.cpp:11161-11187 [@0cd4559]) samt der Komposition
    // bei :13730. Die Reihenfolge ist AetherSDRs, nicht neu erfunden:
    //
    //     Fuellfarbe   volle Deckkraft
    //     Bild         Deckkraft 1 - opacity/100
    //
    // m_bgOpacity sagt also, wie stark die FUELLFARBE durchkommt, nicht
    // wie stark das Bild deckt. Der Name stammt aus AetherSDR und
    // bleibt, damit ein Vergleich der beiden Baeume nicht an einer
    // Umbenennung scheitert.
    //
    // Unconditional (not inside #ifdef LONGPATH_GPU_SPECTRUM): both the
    // GPU and the CPU-only paint path call paintBackgroundLayer(), and
    // its setters/getters above are declared unconditionally too -- these
    // members were stranded inside the GPU-only block below, which broke
    // every CPU-only build (e.g. release.yml's ubuntu-24.04-arm job,
    // which sets -DLONGPATH_GPU_SPECTRUM=OFF because that runner's Qt is
    // older than the 6.7 QRhiWidget needs).
    QImage  m_bgImage;
    QImage  m_bgScaled;
    QSize   m_bgScaledSize;
    QString m_bgImagePath;
    int     m_bgOpacity{80};
    QColor  m_bgFillColor{QColor(Style::kPanadapterBg)};
    QColor  m_wfBgFillColor{QColor(Style::kAppBg)};
    int     m_bgBrightnessPct{100};

    int m_visibleBinCount{0};  // bins rendered this frame (for draw call count)

    // Ebenfalls ausserhalb, aus demselben Grund: der CPU-Pfad in
    // SpectrumWidget.cpp fasst diese Felder ungeschuetzt an. Sie lagen
    // im Gate, weil sie dort entstanden sind — nicht, weil sie die GPU
    // braeuchten. Ein Bau ohne QRhi meldete darauf 35 Fehler. Gefunden per
    // systematischem Abgleich jedes Members in diesem Block gegen seine
    // echten Verwendungsstellen in SpectrumWidget.cpp, nachdem der
    // ubuntu-24.04-arm-Bau (-DLONGPATH_GPU_SPECTRUM=OFF, aeltere Qt-Version
    // ohne QRhiWidget) daran einen Fehler nach dem anderen meldete.

    /// Wasserfall-Textur beim naechsten Bild vollstaendig hochladen.
    /// Der CPU-Pfad setzt die Fahne beim Groessenwechsel mit.
    bool m_wfTexFullUpload{true};
    /// Wann zuletzt ECHTE Spektrumdaten ankamen (ms seit Epoche).
    /// Der Wasserfall friert ein, wenn der Strom abreisst — siehe
    /// pushWaterfallRow().
    qint64 m_lastSpectrumArrivalMs{0};

    /// Statische Einblendungen neu zeichnen (Gitter, Beschriftung,
    /// dBm-Leiste). Auf dem CPU-Pfad ohne Wirkung, aber gesetzt.
    bool   m_overlayStaticDirty{true};
    bool   m_shutdownPrepared{false};
    // ── Der VFO-Zug rechnet RELATIV zum Startpunkt ───────────────────
    // Beim Druck festgehalten: Zeigerposition, VFO und Hz-je-Punkt.
    // Warum, steht am Auswertepunkt in mouseMoveEvent.
    int    m_panDragLastX{0};
    bool   m_panDragArmed{false};
    int    m_vfoDragStartX{0};
    double m_vfoDragStartHz{0.0};
    double m_vfoDragHzPerPx{0.0};

#ifdef LONGPATH_GPU_SPECTRUM
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
    // ── Why the GPU path did not come up ─────────────────────────────
    //
    // Empty means everything created cleanly. Otherwise it names the
    // first thing that failed, and initialize() says so once, loudly.
    //
    // This exists because the magenta waterfall was diagnosed three
    // times and misattributed twice. Every QRhi create() in this file
    // returned a bool that was thrown away, so a failed texture or
    // pipeline produced no log line at all — the draw went ahead against
    // an incomplete object and the shader sampled undefined GPU memory,
    // which on Metal is very often exactly magenta. The colour on screen
    // was the entire diagnostic output of the failure.
    QString m_gpuInitFailure;

    int  m_wfGpuTexW{0};
    int  m_wfGpuTexH{0};
    int  m_wfLastUploadedRow{-1};
    // Zeilen, die pushWaterfallRow seit dem letzten Hochladen geschrieben
    // hat. Der Teil-Upload laeuft von m_wfLastUploadedRow bis
    // m_wfWriteRow -- nach mehr als einer Texturhoehe ist der Ring
    // umgelaufen, und dieser Abstand zeigt nur noch den Rest (2026-09-26).
    int  m_wfRowsSinceUpload{0};

    // ---- Overlay GPU resources ----
    QRhiGraphicsPipeline*       m_ovPipeline{nullptr};
    QRhiShaderResourceBindings* m_ovSrb{nullptr};
    QRhiBuffer*                 m_ovVbo{nullptr};
    QRhiTexture*                m_ovGpuTex{nullptr};
    QRhiSampler*                m_ovSampler{nullptr};
    /// Lage der Einblendungen als Anteil der Spektrumsflaeche.
    /// Vorgabe: links unten, rechts unten, Mitte unten — ueber dem
    /// Bandplan, wo beim Abstimmen nichts steht.
    /// Welche Einblendung gerade gezogen wird (-1 = keine).

    QImage m_overlayStatic;

    bool   m_overlayNeedsUpload{true};

    // 2026-05-26 KG4VCF dual-layer overlay split.
    //
    // The static texture above carries chrome (grid, scales, bandplan,
    // VFO marker, spot markers, freq/time scale, waterfall chrome,
    // perf overlay, etc.) that only changes on operator interaction.
    // The dynamic texture below carries the per-frame features --
    // peak hold trace, peak blobs, noise-floor line + text -- so
    // those can animate at display rate without forcing a full chrome
    // repaint each tick.
    //
    // Both textures alpha-composite onto the spectrum trace.  Same
    // pipeline + sampler + UBO + VBO as the static layer -- only the
    // SRB and texture differ.  The dynamic image is the SAME size as
    // the static one (full window) for code simplicity; in practice
    // it stays mostly transparent except for the spectrum-area
    // overlays, so the GPU sampler reads through to spectrum
    // un-tinted for the rest of the widget.
    QRhiShaderResourceBindings* m_ovDynSrb{nullptr};
    QRhiTexture*                m_ovDynGpuTex{nullptr};
    QImage m_overlayDynamic;
    bool   m_overlayDynamicDirty{true};
    bool   m_overlayDynamicNeedsUpload{true};
    // One full-image upload owed after every (re)create of
    // m_ovDynGpuTex — the partial spectrum-band upload relies on the
    // waterfall region having been written ONCE, and a recreated
    // texture is undefined until it has (the 2026-08-11 magenta
    // waterfall). True initially so the very first upload is full.
    bool   m_ovDynNeedsFullUpload{true};

    // ---- 3D stacked-trace GPU resources ----
    // No new pipeline or shader: DssRenderer paints a QImage on the CPU,
    // uploaded into this texture and drawn through the EXISTING overlay
    // pipeline/VBO (m_ovPipeline/m_ovVbo) with the viewport clipped to
    // specRect, in place of the FFT trace draws.
    QRhiShaderResourceBindings* m_dss3dSrb{nullptr};
    QRhiTexture*                m_dss3dGpuTex{nullptr};
    QSize                       m_dss3dGpuTexSize;

    // 2026-05-25 perf fix: timestamp of the last per-frame "dynamic
    // overlay" force-dirty in updateSpectrumLinear.  Rate-limits the
    // overlay rebuild for Active Peak Hold / Peak Blobs / Noise Floor
    // (all features that previously rebuilt the FULL overlay on every
    // 30 Hz spectrum frame, defeating the cache).  See the rationale
    // at SpectrumWidget.cpp around the "perf fix" comment block.
    qint64 m_overlayDynamicDirtyMs{0};

    // 2026-05-26 KG4VCF perf instrumentation: wall-clock timestamp
    // of the previous renderGpuFrame entry.  Used to feed
    // PerfMonitor::recordInterFrameGap(now - m_lastPaintWallMs) so the
    // perf overlay can show if paint events are arriving on time.
    // A gap > the display period == main thread was blocked.
    qint64 m_lastPaintWallMs{0};
    // Toggle for the in-spectrum perf overlay (drawn in a corner
    // showing paint/gap/fft/overlay-rebuild timings + audio underruns
    // + memory pressure).  Persisted via AppSettings key
    // "ShowPerfOverlay"; View menu wires the setter.
    bool m_showPerfOverlay{false};
    // 1 Hz timer that polls memory pressure + drives perf overlay
    // refresh.  Owned by SpectrumWidget via Qt parent ownership.
    QTimer* m_perfPollTimer{nullptr};

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
    // Kurve als bis zu fuenf Streifen: Hof oben/unten, Kante oben,
    // Kern, Kante unten. Ohne Kante und Hof bleibt es bei einem
    // Streifen; die Puffer sind trotzdem fuer fuenf angelegt, damit das
    // Umschalten im Betrieb keine Zuteilung auf dem Malweg braucht.
    static constexpr int kLineStrips = Kurve::kStreifenHoechstens;

#endif

    // Invalidate the GPU-path cached overlay texture so grid, labels,
    // dBm scale, waterfall filter/zero-line/timestamp overlays, and
    // other QPainter-drawn chrome re-render on next frame. Safe no-op
    // when the GPU path is disabled.
    void markOverlayDirty() {
#ifdef LONGPATH_GPU_SPECTRUM
        m_overlayStaticDirty = true;
        // Im 3D-Modus liegt die Bandbreite (Saeule, Kanten, Mittellinie)
        // in der DYNAMISCHEN Schicht ueber der Flaeche
        // (paintPassbandOverSurface) — was das Chrome veraltet, veraltet
        // dort auch sie. Ohne diese Zeile wanderte die Saeule beim
        // Filterziehen erst mit dem naechsten Spektrumbild nach.
        if (m_renderMode == SpectrumRenderMode::Mode3D) {
            m_overlayDynamicDirty = true;
        }
#endif
        update();
    }

    /// True while a resize drag is still in flight — the size has changed
    /// within the last kResizeSettleMs and is expected to change again.
    /// Callers that would re-allocate a size-dependent buffer skip while
    /// this holds; applyResizeSettled() does the work once at the end.
    ///
    /// The first allocation is exempt, but that exemption lives in
    /// resizeEvent (which never starts the timer when m_waterfall is
    /// still null) rather than here — this only reports the timer.
    bool resizeSettling() const {
        return m_resizeSettleTimer && m_resizeSettleTimer->isActive();
    }

    /// Re-allocate the waterfall image for the current widget size and
    /// let the render path re-allocate the overlays.  Called from the
    /// settle timer, and directly from resizeEvent for the first
    /// allocation.
    void applyResizeSettled();
    /// Legt `m_waterfall` an, falls es das noch nicht gibt. Gibt false,
    /// wenn das Fenster dafuer noch keine Groesse hat. Siehe den
    /// Kommentar im Rumpf (Befund des Betreibers, 2026-10-31).
    bool stelleWasserfallbildSicher();

    // Durchlass und Bandbreite (2026-11-08)
    void werteDurchlassAus(bool sofort = false);
    void fuehreLupeNach();
    bool lupeSichtbar() const;
    void paintBandbreite(QPainter& p, const QRect& specRect, int xLo, int xHi,
                         double centreHz, int specBottomClipped, int loHz, int hiHz);
    void paintSignalKlammern(QPainter& p, const QRect& specRect);
    void paintLueckenfinder(QPainter& p, const QRect& specRect);
    void paintDurchlassLupe(QPainter& p, const QRect& specRect);
    QVector<QPair<QString, float>> sStufenStriche() const;
};

} // namespace Longpath
