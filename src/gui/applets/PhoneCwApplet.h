#pragma once

// =================================================================
// src/gui/applets/PhoneCwApplet.h  (NereusSDR)
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
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-04-30 — Phase 3M-3a-ii post-bench cleanup: PROC button + slider
//                 are wired bidirectionally to TransmitModel::cpdrOn /
//                 cpdrLevelDb. Slider reconfigured 0..2 (NOR/DX/DX+
//                 placeholder) → 0..20 dB (full Thetis ptbCPDR range,
//                 console.Designer.cs:6042-6043 [v2.10.3.13]).  NOR/DX/DX+
//                 labels replaced with a numeric "X dB" value display.
//                 NyiOverlay::markNyi calls dropped on PROC button + slider.
//   2026-05-03 — Phase 3M-3a-iii Task 15: VOX (#10) and DEXP (#11) rows
//                 wired to TransmitModel.  Each row gets a slider-stack
//                 (threshold slider above a thin DexpPeakMeter strip).
//                 Threshold slider ranges retuned to dB scales (VOX
//                 -80..0, DEXP -160..0).  Right-click on either [ON]
//                 button emits openSetupRequested("Transmit", "DEXP/VOX")
//                 so MainWindow can jump to the DexpVoxPage (Task 14).
//                 100 ms QTimer drives both peak meters (matches Thetis
//                 UpdateNoiseGate Task.Delay(100), console.cs:25347
//                 [v2.10.3.13]).
//   2026-05-04 — Phase 3M-3a-iii bench polish: VOX row (#10) relocated to
//                 TxApplet under TUNE/MOX (Option B - full row).  Operators
//                 wanted the VOX engage surface next to MOX/TUNE on the
//                 right pane.  Full row moved as a unit including the live
//                 DexpPeakMeter strip + 100 ms peak-meter poller +
//                 right-click → Setup → Transmit → DEXP/VOX.  DEXP row
//                 (#11) stays here — only VOX moves.  Members
//                 m_voxBtn/m_voxSlider/m_voxLvlLabel/m_voxDlySlider/
//                 m_voxDlyLabel/m_voxPeakMeter removed from this header.
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

#pragma once

#include "AppletWidget.h"

class QStackedWidget;
class QButtonGroup;
class QSlider;
class QPushButton;
class QLabel;
class QComboBox;
class QTimer;

namespace NereusSDR {

class HGauge;
class DexpPeakMeter;

// PhoneCwApplet — QStackedWidget with Phone (0) and CW (1) pages.
// FM is a separate FmApplet per the reconciled design spec.
// Phone page: 13 controls — Phase 3I-1 / 3I-3 / 3-VAX
// CW page:     9 controls — Phase 3I-2
//
// Phase 3M-1b (relocated from TxApplet J.1):
//   #5 Mic level slider is now wired to TransmitModel::micGainDb (bidirectional).
//   #1 Mic level gauge is now wired to AudioEngine::pcMicInputLevel() via 50ms timer.
class PhoneCwApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit PhoneCwApplet(RadioModel* model, QWidget* parent = nullptr);
    ~PhoneCwApplet() override;

    QString appletId()    const override { return QStringLiteral("PHCW"); }
    QString appletTitle() const override { return QStringLiteral("Phone / CW"); }
    void syncFromModel() override;

    // Switch the stacked widget page: 0=Phone, 1=CW, 2=FM
    void showPage(int index);

signals:
    /// Phase 3M-3a-iii Task 15: emitted when the user right-clicks the VOX
    /// or DEXP [ON] button.  MainWindow listens and jumps the SetupDialog
    /// to the requested leaf page.  `category` is informational (currently
    /// always "Transmit"); `page` is the SetupDialog leaf-item label
    /// (currently always "DEXP/VOX").
    void openSetupRequested(const QString& category, const QString& page);

private slots:
    /// Phase 3M-3a-iii Task 15: 100 ms timer slot — pulls live peaks from
    /// TxChannel::getDexpPeakSignal() and getTxMicMeterDb(), maps them to
    /// 0..1 normalized scales for the two DexpPeakMeter strips.  Cadence
    /// matches Thetis UpdateNoiseGate (console.cs:25347 [v2.10.3.13])
    /// which is `Task.Delay(100)`.
    void pollDexpMeters();

private:
    void buildUI();
    void buildPhonePage(QWidget* page);
    void buildCwPage(QWidget* page);
    void buildFmPage(QWidget* page);
    // Phase 3M-1b: wire mic gain slider + mic level gauge timer.
    void wireControls();
    // ── Shared ───────────────────────────────────────────────────────────────
    QStackedWidget* m_stack{nullptr};
    QButtonGroup*   m_tabGroup{nullptr};
    QPushButton*    m_phoneTabBtn{nullptr};
    QPushButton*    m_cwTabBtn{nullptr};

    // ── Mic level gauge timer (Phase 3M-1b) ──────────────────────────────────
    // Polls AudioEngine::pcMicInputLevel() at 50 ms (~20 fps) and converts the
    // linear 0..1 amplitude to dBFS for m_levelGauge. Silent (-40 floor) when
    // the TX input bus is not open (mic not configured / RX-only mode).
    QTimer*  m_micLevelTimer{nullptr};

    // ── Phone page (13 controls) ──────────────────────────────────────────────
    // #1  Mic level gauge (HGauge -40..+10 dBFS, yellow -10, red 0)
    //     Wired via m_micLevelTimer (Phase 3M-1b).
    HGauge*      m_levelGauge{nullptr};
    // #2  Compression gauge (HGauge -25..0 dB, reversed=true)
    HGauge*      m_compGauge{nullptr};
    // #3  Mic profile combo
    QComboBox*   m_micProfileCombo{nullptr};
    // #4  Mic source combo (fixed 55px)
    QComboBox*   m_micSourceCombo{nullptr};
    // #5  Mic gain slider (per-board range, default -6 dB) + value label
    //     Wired bidirectionally to TransmitModel::micGainDb (Phase 3M-1b).
    //     Greyed out when TransmitModel::micMute == false (mic muted).
    QSlider*     m_micLevelSlider{nullptr};
    QLabel*      m_micLevelLabel{nullptr};
    // #6  +ACC button (green toggle, 48px)
    QPushButton* m_accBtn{nullptr};
    // #7  PROC button (green 48px) + slider (0..20 dB CPDR level) + numeric
    //     value label "X dB".  Slider is the full Thetis ptbCPDR range
    //     (console.Designer.cs:6042-6043 [v2.10.3.13]).
    //     Bidirectional with TransmitModel::cpdrOn / cpdrLevelDb (post-bench
    //     cleanup 2026-04-30).
    QPushButton* m_procBtn{nullptr};
    QSlider*     m_procSlider{nullptr};
    QLabel*      m_procLabel{nullptr};
    QLabel*      m_procValueLabel{nullptr};
    // #8  VAX button (blue toggle, 48px)
    QPushButton* m_vaxBtn{nullptr};
    // #9  MON button (green 48px) + slider (0-100) + inset "50"
    QPushButton* m_monBtn{nullptr};
    QSlider*     m_monSlider{nullptr};
    QLabel*      m_monLabel{nullptr};
    // (Control 10 VOX row relocated to TxApplet 2026-05-04 — bench polish.
    //  See TxApplet.{h,cpp} section 4b for the new home.  Members removed
    //  from this header in the same commit.)
    // #11 DEXP toggle (36px) + threshold slider (-160..0 dB) + inset +
    //     DexpPeakMeter under threshold slider.  Toggle wired to
    //     TransmitModel::dexpEnabled.  Threshold slider is DECORATIVE
    //     (FAITHFUL Thetis quirk per console.cs:28962-28970 [v2.10.3.13]):
    //     drives the meter marker only, never pushed to WDSP.  See
    //     wireControls() for the verbatim quote.
    QPushButton*   m_dexpBtn{nullptr};
    QSlider*       m_dexpSlider{nullptr};
    QLabel*        m_dexpLabel{nullptr};
    DexpPeakMeter* m_dexpPeakMeter{nullptr};

    // Phase 3M-3a-iii Task 15: UI-only DEXP threshold marker (dB).  NOT
    // pushed to TransmitModel or TxChannel.  Stock Thetis ptbNoiseGate
    // also stores the value only for the picNoiseGate marker draw; we
    // mirror that exactly (see wireControls() inline comment).
    double m_dexpThresholdMarkerDb{-50.0};

    // Phase 3M-3a-iii Task 15: 100 ms QTimer driving both DexpPeakMeters.
    // Cadence matches Thetis UpdateNoiseGate Task.Delay(100) at
    // console.cs:25347 [v2.10.3.13].
    QTimer* m_dexpMeterTimer{nullptr};
    // #12 TX filter Low/High sliders removed — superseded by TxApplet
    // Lo/Hi spinboxes (Plan 4 Cluster C); the sliders here were NYI and
    // never reached WDSP.

    // #13 AM Carrier level slider (0-100) + inset "25"
    QSlider*     m_amCarSlider{nullptr};
    QLabel*      m_amCarLabel{nullptr};

    // ── CW page — placeholder (Phase 3M-2 deferred) ─────────────────────────
    // CW member variables removed per
    // docs/superpowers/plans/2026-05-01-ui-polish-right-panel.md §Task 7.
    // Re-add alongside the buildCwPage() implementation when 3M-2 lands:
    //   m_alcGauge, m_speedSlider, m_speedLabel, m_pitchDown, m_pitchLabel,
    //   m_pitchUp, m_delaySlider, m_delayLabel, m_sidetoneBtn,
    //   m_sidetoneSlider, m_sidetoneLabel, m_breakinBtn, m_iambicBtn,
    //   m_fwKeyerBtn, m_cwPanSlider, m_cwPanLabel.

    // ── FM page (8 controls) ─────────────────────────────────────────────────
    // #1  FM MIC slider (0-100) + inset value
    QSlider*     m_fmMicSlider{nullptr};
    QLabel*      m_fmMicLabel{nullptr};
    // #2  Deviation buttons 5.0k / 2.5k (blue toggle)
    QPushButton* m_dev5kBtn{nullptr};
    QPushButton* m_dev25kBtn{nullptr};
    // #3  CTCSS enable (green) + tone combo
    QPushButton* m_ctcssBtn{nullptr};
    QComboBox*   m_ctcssCombo{nullptr};
    // #4  Simplex toggle (green)
    QPushButton* m_simplexBtn{nullptr};
    // #5  Repeater offset slider (0-10000 kHz) + inset "600"
    QSlider*     m_rptOffsetSlider{nullptr};
    QLabel*      m_rptOffsetLabel{nullptr};
    // #6  Offset direction: [-] [+] [Rev] (blue toggles)
    QPushButton* m_offsetMinusBtn{nullptr};
    QPushButton* m_offsetPlusBtn{nullptr};
    QPushButton* m_offsetRevBtn{nullptr};
    // #7  FM TX Profile combo
    QComboBox*   m_fmProfileCombo{nullptr};
    // #8  FM Memory combo + nav
    QComboBox*   m_fmMemCombo{nullptr};
    QPushButton* m_fmMemPrev{nullptr};
    QPushButton* m_fmMemNext{nullptr};
};

} // namespace NereusSDR
