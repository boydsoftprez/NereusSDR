// =================================================================
// src/gui/applets/TxApplet.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs — chkMOX_CheckedChanged2
//   (29311-29678 [v2.10.3.13]) and chkTUN_CheckedChanged (29978-30157
//   [v2.10.3.13]); original licence from Thetis source is included below.
//
// Layout from AetherSDR TxApplet.{h,cpp} (GPLv3, see AetherSDR attribution
// block below).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//                 Wiring deferred to Phase 3M.
//   2026-04-26 — Phase 3M-1a H.3: deep-wired TUNE/MOX/Tune-Power/RF-Power.
//                 Out-of-phase controls (2-Tone, PS-A) hidden.
//                 syncFromModel() implemented. setCurrentBand(Band) added.
//   2026-04-28 — Phase 3M-1b J.2: VOX toggle button added below Tune Power.
//                 Checkable, green border when active. Bidirectional with
//                 TransmitModel::voxEnabled. Right-click opens VoxSettingsPopup.
//                 (REMOVED 2026-05-03 in 3M-3a-iii Task 16 — see entry below.)
//   2026-04-28 — Phase 3M-1b J.3: MON toggle button + monitor volume slider
//                 added below VOX. Bidirectional with TransmitModel::monEnabled
//                 and monitorVolume (default 0.5f, Thetis audio.cs:417). Mic-source
//                 badge added above the gauges ("PC mic"/"Radio mic"), driven by
//                 TransmitModel::micSourceChanged. Phase J complete.
//   2026-04-28 — Phase 3M-1b (relocation): Mic Gain slider row (J.1) removed from
//                 TxApplet. Relocated to PhoneCwApplet (#5 slot) per JJ feedback.
//   2026-04-28 — Phase 3M-1b K.2: tooltipForMode(DSPMode) helper + onMoxModeChanged
//                 slot implemented. Wired to SliceModel::dspModeChanged via
//                 RadioModel active-slice accessor so the MOX button tooltip
//                 reflects the rejection reason for CW/AM/FM/etc. modes.
//                 Closes Phase K.
//   2026-04-30 — Phase 3M-3a-ii Batch 6 (Task F + A): PROC button enabled and
//                 bidirectionally wired to TransmitModel::cpdrOn.  CFC button
//                 added next to PROC, bidirectional with cfcEnabled, right-
//                 click opens modeless TxCfcDialog (single instance kept alive
//                 across opens — same pattern as TxEqDialog).
//                 requestOpenCfcDialog() public slot exposed so CfcSetupPage's
//                 [Configure CFC bands…] button routes to the same dialog
//                 instance via a MainWindow-side signal connection.
//   2026-05-02 — Plan 4 Cluster C (Task 4 / D2+D3+D9-status): TX BW spinbox
//                 row added below Profile combo — Lo/Hi QSpinBox pair
//                 bidirectional with TransmitModel::filterLow/filterHigh;
//                 orange status label shows filter description text.
//                 Wired via filterChanged(int,int) + dspModeChanged refresh.
//                 syncFromModel() extended to seed spinboxes + status label.
//   2026-05-04 — Phase 3M-3a-iii bench polish: VOX row relocated from
//                 PhoneCwApplet Phone tab (Control #10) back to TxApplet as
//                 a full row directly under TUNE/MOX (Option B - full row).
//                 VOX button + threshold slider + DexpPeakMeter strip + Hold
//                 slider all move as a unit, plus the 100 ms peak-meter
//                 poller (now TxApplet::pollVoxMeter) and the right-click
//                 → Setup → Transmit → DEXP/VOX signal handling.  DEXP row
//                 stays on PhoneCwApplet — only VOX moves.
// =================================================================

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

// =================================================================
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
// =================================================================

// TxApplet — TX control panel.
// Phase 3M-1a H.3: TUNE/MOX/Tune-Power/RF-Power deep-wired.
// Phase 3M-1b J.3: MON toggle + monitor volume slider + mic-source badge wired.
// Phase 3M-3a-iii bench polish (2026-05-04): VOX row relocated back from
//   PhoneCwApplet — full row [VOX btn][threshold + peak strip][Hold slider].
// Out-of-phase controls (2-Tone, PS-A) hidden.
//
// Control inventory:
//  0.  Mic-source badge  — read-only "PC mic"/"Radio mic"  [WIRED — 3M-1b J.3]
//  1.  Fwd Power gauge   — HGauge 0–120 W, redStart 100 W
//  2.  SWR gauge         — HGauge 1.0–3.0, redStart 2.5
//  3.  RF Power slider   + label + value  [WIRED — 3M-1a H.3]
//  4.  Tune Power slider + label + value  [WIRED — 3M-1a H.3]
//      (Mic Gain slider relocated to PhoneCwApplet — 2026-04-28 relocation)
//  4a. TUNE + MOX button row [WIRED — 3M-1a H.3]
//  4b. VOX row — [VOX btn][Threshold slider + DexpPeakMeter strip][-N dB]
//      [Hold slider 1..2000 ms][N ms]  [WIRED — 3M-3a-iii bench polish]
//      Bidirectional with voxEnabled / voxThresholdDb / voxHangTimeMs.
//      Right-click VOX → openSetupRequested("Transmit", "DEXP/VOX").
//      Relocated from PhoneCwApplet Phone tab Control #10.
//  4c. MON toggle button — checkable, blue:checked style  [WIRED — 3M-1b J.3]
//      Bidirectional with TransmitModel::monEnabled (default false).
//  4d. Monitor volume slider — 0..100 → monitorVolume 0.0..1.0  [WIRED — 3M-1b J.3]
//      Default 50 (model default 0.5f, Thetis audio.cs:417).
//  5.  MOX button        — checkable, red:checked style  [WIRED — 3M-1a H.3]
//  6.  TUNE button       — checkable, red:checked + "TUNING..." text  [WIRED — 3M-1a H.3]
//  7.  ATU button        — checkable (NYI — 3M-2/3M-3)
//  8.  MEM button        — checkable (NYI — 3M-2/3M-3)
//  9.  TX Profile combo  — (NYI — 3M-3)
// 10.  Tune mode combo   — (NYI — 3M-3)
// 11.  2-Tone test       — HIDDEN until Phase 3M-3
// 12.  PS-A toggle       — HIDDEN until Phase 3M-4
// 13.  DUP               — checkable (NYI — 3M-3)
// 14.  xPA indicator     — checkable (NYI — 3M-3)
// 15.  SWR protection LED — QLabel indicator (NYI — 3M-3)

#include "TxApplet.h"
#include "TxEqDialog.h"
#include "TxCfcDialog.h"
#include "NyiOverlay.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "gui/ComboStyle.h"
#include "gui/widgets/DexpPeakMeter.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/MicProfileManager.h"
#include "core/MoxController.h"
#include "core/RadioStatus.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QComboBox>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

TxApplet::TxApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
    wireControls();
}

void TxApplet::buildUI()
{
    // Outer layout: zero margins (title bar flush to edges)
    // Body: padded content — matches AetherSDR TxApplet.cpp outer/inner pattern
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* body = new QWidget(this);
    body->setStyleSheet(QStringLiteral("background: %1;").arg(Style::kPanelBg));
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 2);
    vbox->setSpacing(2);
    outer->addWidget(body);

    // ── 0. Mic-source badge ── read-only label above the gauges ─────────────
    // Phase 3M-1b J.3: shows "PC mic" or "Radio mic" reflecting
    // TransmitModel::micSource (default MicSource::Pc). Read-only; no interaction.
    // Updates on micSourceChanged signal (wired in wireControls()).
    {
        m_micSourceBadge = new QLabel(QStringLiteral("PC mic"), this);
        m_micSourceBadge->setAlignment(Qt::AlignCenter);
        m_micSourceBadge->setFixedHeight(16);
        m_micSourceBadge->setStyleSheet(QStringLiteral(
            "QLabel {"
            " color: %1;"
            " font-size: 9px;"
            " border: 1px solid %2;"
            " border-radius: 2px;"
            " padding: 0px 4px;"
            " background: %3;"
            "}"
        ).arg(Style::kTitleText, Style::kInsetBorder, Style::kInsetBg));
        m_micSourceBadge->setAccessibleName(QStringLiteral("Mic source indicator"));
        m_micSourceBadge->setToolTip(QStringLiteral(
            "Active microphone source: PC mic or Radio mic.\n"
            "Change via Setup → Transmit → Mic Source."));
        vbox->addWidget(m_micSourceBadge);
    }

    // ── 1. Forward Power gauge — per-SKU scale ──────────────────────────────
    // Default ticks at construction match the Hermes-class 100 W radio
    // (0 / 40 / 80 / 100 / 120 — AetherSDR TxApplet.cpp:71); rescaleFwdGaugeForModel()
    // reapplies a per-SKU range when RadioModel::currentRadioChanged fires
    // (wired in wireControls()).  Bench-reported #167 follow-up: HL2 reading
    // 1-5 W on a 0-120 W scale was a barely-visible sliver; per-SKU scaling
    // gives each radio a properly-sized meter.
    auto* fwdGauge = new HGauge(this);
    fwdGauge->setTitle(QStringLiteral("RF Pwr"));
    fwdGauge->setAccessibleName(QStringLiteral("Forward power gauge"));
    m_fwdPowerGauge = fwdGauge;
    rescaleFwdGaugeForModel(HPSDRModel::FIRST);   // sentinel default until model known
    // 3M-1a (2026-04-27): wired to RadioStatus::powerChanged in
    // wireControls() — the gauge displays radio-reported forward
    // power in watts (scaleFwdPowerWatts'd at the model side).
    // The legacy "Phase 3I-1" NYI marker has been dropped.
    vbox->addWidget(fwdGauge);

    // ── 2. SWR gauge ── 1.0–3.0, redStart 2.5 ───────────────────────────────
    // Ticks: 1 / 1.5 / 2.5 / 3  (AetherSDR TxApplet.cpp:77)
    auto* swrGauge = new HGauge(this);
    swrGauge->setRange(1.0, 3.0);
    swrGauge->setRedStart(2.5);
    swrGauge->setYellowStart(2.5);
    swrGauge->setTitle(QStringLiteral("SWR"));
    swrGauge->setTickLabels({QStringLiteral("1"), QStringLiteral("1.5"),
                              QStringLiteral("2.5"), QStringLiteral("3")});
    swrGauge->setAccessibleName(QStringLiteral("SWR gauge"));
    m_swrGauge = swrGauge;
    vbox->addWidget(swrGauge);

    // ── 3. RF Power slider row ───────────────────────────────────────────────
    // Label fixedWidth 62, value fixedWidth 22  (AetherSDR TxApplet.cpp:87–104)
    {
        auto* rfSlider = new QSlider(Qt::Horizontal, this);
        rfSlider->setRange(0, 100);
        rfSlider->setValue(100);
        rfSlider->setAccessibleName(QStringLiteral("RF power"));
        rfSlider->setObjectName(QStringLiteral("TxRfPowerSlider"));

        auto* rfValue = new QLabel(QStringLiteral("100"), this);
        rfValue->setFixedWidth(22);
        rfValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rfValue->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));

        m_rfPowerSlider = rfSlider;
        m_rfPowerValue  = rfValue;

        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("RF Power:"), this);
        lbl->setFixedWidth(62);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTitleText));
        row->addWidget(lbl);

        rfSlider->setFixedHeight(18);
        rfSlider->setEnabled(true);   // Phase 3M-1a H.3: wired
        // Tooltip set by rescalePowerSlidersForModel() (Issue #175 Task 7) -
        // Thetis-faithful "Transmit Drive - relative value" wording, applied
        // every time the active SKU changes.  No initial setToolTip here.
        row->addWidget(rfSlider, 1);
        row->addWidget(rfValue);

        vbox->addLayout(row);
    }

    // ── 4. Tune Power slider row ─────────────────────────────────────────────
    // Label fixedWidth 62, value fixedWidth 22  (AetherSDR TxApplet.cpp:107–128)
    {
        auto* tunSlider = new QSlider(Qt::Horizontal, this);
        tunSlider->setRange(0, 100);
        tunSlider->setValue(10);
        tunSlider->setAccessibleName(QStringLiteral("Tune power"));

        auto* tunValue = new QLabel(QStringLiteral("10"), this);
        tunValue->setFixedWidth(22);
        tunValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tunValue->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));

        m_tunePwrSlider = tunSlider;
        m_tunePwrValue  = tunValue;

        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Tune Pwr:"), this);
        lbl->setFixedWidth(62);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTitleText));
        row->addWidget(lbl);

        tunSlider->setFixedHeight(18);
        tunSlider->setEnabled(true);  // Phase 3M-1a H.3: wired
        // Tooltip set by rescalePowerSlidersForModel() (Issue #175 Task 7) -
        // Thetis-faithful "Tune and/or 2Tone Drive - relative value" wording,
        // applied every time the active SKU changes.  No initial setToolTip
        // here.
        row->addWidget(tunSlider, 1);
        row->addWidget(tunValue);

        vbox->addLayout(row);
    }

    // ── Button row: TUNE + MOX (50% each) ──────────────────────────────────
    // Positioned above VOX+MON for action-button prominence
    // (docs/superpowers/plans/2026-05-01-ui-polish-right-panel.md §Task 5).
    // ATU + MEM removed (ATU phase, no plan yet; MEM = channel-memory phase).
    // MOX: red active (#cc2222 bg, #ff4444 border, white text)
    // TUNE: red active when tuning, text becomes "TUNING..."
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        const QString btnStyle = Style::buttonBaseStyle()
            + QStringLiteral("QPushButton { padding: 2px; }");
        const QString redChecked = Style::redCheckedStyle();

        m_tuneBtn = new QPushButton(QStringLiteral("TUNE"), this);
        m_tuneBtn->setCheckable(true);
        m_tuneBtn->setFixedHeight(22);
        m_tuneBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_tuneBtn->setStyleSheet(btnStyle + redChecked);
        m_tuneBtn->setEnabled(true);  // Phase 3M-1a H.3: wired
        m_tuneBtn->setAccessibleName(QStringLiteral("Tune carrier"));
        m_tuneBtn->setToolTip(QStringLiteral("Enable TUNE carrier (single-tone CW)"));
        row->addWidget(m_tuneBtn, 1);

        m_moxBtn = new QPushButton(QStringLiteral("MOX"), this);
        m_moxBtn->setCheckable(true);
        m_moxBtn->setFixedHeight(22);
        m_moxBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_moxBtn->setStyleSheet(btnStyle + redChecked);
        m_moxBtn->setEnabled(true);  // Phase 3M-1a H.3: wired
        m_moxBtn->setAccessibleName(QStringLiteral("MOX transmit"));
        m_moxBtn->setToolTip(QStringLiteral("Manual transmit (MOX)"));
        row->addWidget(m_moxBtn, 1);

        vbox->addLayout(row);
    }

    // ── 4b. VOX row (3M-3a-iii bench polish 2026-05-04) ───────────────────────
    // Relocated from PhoneCwApplet (Phone tab Control #10).  Operators
    // wanted the VOX engage surface next to MOX/TUNE on the right pane
    // where they engage TX, not buried on the Phone tab.  Full row moves
    // as a unit including the live DexpPeakMeter strip + 100 ms poller.
    //
    // Layout (Option B - full row):
    //   [VOX btn 48px] | { Threshold slider top, DexpPeakMeter strip
    //   below } | [-20 dB inset] | [Hold slider 1..2000 ms] | [500 ms inset]
    //
    // Threshold slider range -80..0 dB matches Thetis ptbVOX
    // (console.Designer.cs:6018-6019 [v2.10.3.13]).  Hold slider range
    // 1..2000 ms matches Thetis udDEXPHold (setup.designer.cs:45005-45013
    // [v2.10.3.13]).  Default values mirror Thetis ptbVOX.Value=-20
    // (console.Designer.cs:6024) and udDEXPHold.Value=500
    // (setup.designer.cs:45020).
    //
    // Slider-stack: a small QVBoxLayout wraps the threshold slider (top)
    // and DexpPeakMeter (below) so the live mic peak strip sits directly
    // under the threshold knob — matches Thetis picVOX placement.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_voxBtn = new QPushButton(QStringLiteral("VOX"), this);
        m_voxBtn->setCheckable(true);
        m_voxBtn->setFixedWidth(48);
        m_voxBtn->setFixedHeight(22);
        m_voxBtn->setStyleSheet(Style::buttonBaseStyle()
                                + Style::greenCheckedStyle());
        m_voxBtn->setAccessibleName(QStringLiteral("VOX voice-operated transmit"));
        m_voxBtn->setObjectName(QStringLiteral("TxVoxButton"));
        m_voxBtn->setToolTip(QStringLiteral(
            "VOX — voice-operated transmit.  Left-click to toggle.\n"
            "Right-click to open the DEXP/VOX setup page."));
        // CustomContextMenu so right-click hits the openSetupRequested slot
        // instead of the default platform menu.
        m_voxBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_voxBtn);

        // Threshold slider-stack: slider on top, DexpPeakMeter strip below.
        auto* voxStackGroup = new QWidget(this);
        auto* voxStackVbox = new QVBoxLayout(voxStackGroup);
        voxStackVbox->setContentsMargins(0, 0, 0, 0);
        voxStackVbox->setSpacing(1);

        m_voxSlider = new QSlider(Qt::Horizontal, voxStackGroup);
        // From Thetis console.Designer.cs:6018-6019 [v2.10.3.13]:
        //   ptbVOX.Maximum = 0; ptbVOX.Minimum = -80;
        m_voxSlider->setRange(-80, 0);
        m_voxSlider->setValue(-20);
        m_voxSlider->setFixedHeight(14);
        m_voxSlider->setStyleSheet(Style::sliderHStyle());
        m_voxSlider->setAccessibleName(QStringLiteral("VOX threshold (dB)"));
        m_voxSlider->setObjectName(QStringLiteral("TxVoxThresholdSlider"));
        voxStackVbox->addWidget(m_voxSlider);

        m_voxPeakMeter = new DexpPeakMeter(voxStackGroup);
        m_voxPeakMeter->setObjectName(QStringLiteral("TxVoxPeakMeter"));
        m_voxPeakMeter->setAccessibleName(QStringLiteral("VOX live mic peak"));
        voxStackVbox->addWidget(m_voxPeakMeter);

        row->addWidget(voxStackGroup, 1);

        m_voxLvlLabel = new QLabel(QStringLiteral("-20 dB"), this);
        m_voxLvlLabel->setStyleSheet(Style::insetValueStyle());
        m_voxLvlLabel->setFixedWidth(38);
        m_voxLvlLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_voxLvlLabel);

        m_voxDlySlider = new QSlider(Qt::Horizontal, this);
        // From Thetis setup.designer.cs:45005-45013 [v2.10.3.13]:
        //   udDEXPHold.Maximum = 2000; udDEXPHold.Minimum = 1; (units: ms)
        m_voxDlySlider->setRange(1, 2000);
        m_voxDlySlider->setValue(500);
        m_voxDlySlider->setStyleSheet(Style::sliderHStyle());
        m_voxDlySlider->setAccessibleName(QStringLiteral("VOX hold time (ms)"));
        m_voxDlySlider->setObjectName(QStringLiteral("TxVoxHoldSlider"));
        row->addWidget(m_voxDlySlider, 1);

        m_voxDlyLabel = new QLabel(QStringLiteral("500 ms"), this);
        m_voxDlyLabel->setStyleSheet(Style::insetValueStyle());
        m_voxDlyLabel->setFixedWidth(42);
        m_voxDlyLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_voxDlyLabel);

        vbox->addLayout(row);
    }

    // ── 4c. MON toggle button + 4d. Monitor volume slider ─────────────────────
    // Phase 3M-1b J.3: below VOX toggle.
    // MON: checkable, blue border when active (indicates monitor on).
    //   monEnabled does NOT persist — plan §0 row 9 safety: loads OFF always.
    //   Default volume 50 (matches model default 0.5f from Thetis audio.cs:417).
    //
    // Volume slider: 0..100 integer → monitorVolume float 0.0..1.0 (value/100.0f).
    //   Inverse: monitorVolumeChanged(float) → slider position = qRound(v * 100.0f).
    {
        // MON button row
        auto* monRow = new QHBoxLayout;
        monRow->setSpacing(4);

        m_monBtn = new QPushButton(QStringLiteral("MON"), this);
        m_monBtn->setCheckable(true);
        m_monBtn->setChecked(false);  // default: OFF — plan §0 row 9 safety rule
        m_monBtn->setFixedHeight(22);
        m_monBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // MON button intentionally uses dark-navy/cyan (#001a33 bg / #3399ff border)
        // to distinguish from generic blue toggles (kBlueBg=#0070c0/kBlueBorder=#0090e0).
        // NereusSDR-original one-off — do NOT snap to Style::kBlueBg / kBlueBorder.
        m_monBtn->setStyleSheet(Style::buttonBaseStyle()
            + QStringLiteral("QPushButton:checked {"
                             " background: #001a33;"
                             " border: 1px solid #3399ff;"
                             " color: #ffffff;"
                             "}"));
        m_monBtn->setAccessibleName(QStringLiteral("Monitor enable"));
        m_monBtn->setToolTip(QStringLiteral(
            "Monitor: mix received audio into headphones during TX.\n"
            "Does NOT persist across restarts (safety)."));
        monRow->addWidget(m_monBtn, 1);
        monRow->addStretch();

        vbox->addLayout(monRow);

        // Monitor volume slider row
        auto* volRow = new QHBoxLayout;
        volRow->setSpacing(4);

        auto* volLbl = new QLabel(QStringLiteral("Mon Vol:"), this);
        volLbl->setFixedWidth(62);
        volLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTitleText));
        volRow->addWidget(volLbl);

        // Range 0..100 integer; default 50 (model default 0.5f).
        m_monitorVolumeSlider = new QSlider(Qt::Horizontal, this);
        m_monitorVolumeSlider->setRange(0, 100);
        m_monitorVolumeSlider->setValue(50);
        m_monitorVolumeSlider->setFixedHeight(18);
        m_monitorVolumeSlider->setAccessibleName(QStringLiteral("Monitor volume"));
        m_monitorVolumeSlider->setToolTip(QStringLiteral(
            "Monitor receive audio volume during TX (0–100 %)"));
        volRow->addWidget(m_monitorVolumeSlider, 1);

        m_monitorVolumeValue = new QLabel(QStringLiteral("50"), this);
        m_monitorVolumeValue->setFixedWidth(26);
        m_monitorVolumeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_monitorVolumeValue->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));
        volRow->addWidget(m_monitorVolumeValue);

        vbox->addLayout(volRow);
    }

    // ── 4e. TX-processing quick toggles: [LEV] [EQ] [CFC] ──────────────────
    // Phase 3M-3a-i Batch 2 (Task F): introduced the row of 3 (LEV/EQ/PROC).
    // Phase 3M-3a-ii Batch 6 (Task A + F, then post-bench cleanup):
    // PROC was promoted in Batch 6, then dropped here in the cleanup pass —
    // PhoneCwApplet already had a wired PROC button + slider sitting un-wired
    // since 3I-3 (NyiOverlay-marked).  Two PROC controls confused users.
    // Row is now 3 buttons (LEV / EQ / CFC); PROC lives on PhoneCwApplet.
    // All three share the same VOX/MON styling family (compact 22-px-tall,
    // expanding width, green-checked LED look).
    //
    //   LEV  — checkable, bidirectional with TransmitModel::txLevelerOn.
    //   EQ   — checkable, bidirectional with TransmitModel::txEqEnabled.
    //          Left-click toggles.  Right-click → TxEqDialog (3M-3a-i Batch 3).
    //   CFC  — checkable, bidirectional with TransmitModel::cfcEnabled.
    //          Left-click toggles.  Right-click → modeless TxCfcDialog
    //          (10-band per-band CFC editor; mirrors Thetis frmCFCConfig
    //          [v2.10.3.13]).
    //
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        const QString btnStyle = Style::buttonBaseStyle()
            + Style::greenCheckedStyle();

        m_levBtn = new QPushButton(QStringLiteral("LEV"), this);
        m_levBtn->setCheckable(true);
        m_levBtn->setFixedHeight(22);
        m_levBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_levBtn->setStyleSheet(btnStyle);
        m_levBtn->setAccessibleName(QStringLiteral("TX Leveler enable"));
        m_levBtn->setObjectName(QStringLiteral("TxLevButton"));
        m_levBtn->setToolTip(QStringLiteral(
            "TX Leveler — slow speech-leveling AGC. Improves intelligibility on weak speech."));
        row->addWidget(m_levBtn, 1);

        m_eqBtn = new QPushButton(QStringLiteral("EQ"), this);
        m_eqBtn->setCheckable(true);
        m_eqBtn->setFixedHeight(22);
        m_eqBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_eqBtn->setStyleSheet(btnStyle);
        m_eqBtn->setAccessibleName(QStringLiteral("TX EQ enable"));
        m_eqBtn->setObjectName(QStringLiteral("TxEqButton"));
        m_eqBtn->setToolTip(QStringLiteral(
            "TX 10-band graphic EQ.  Left-click to toggle.\n"
            "Right-click to open the EQ dialog."));
        // Custom context-menu policy so right-click hits a slot (not the
        // default menu).
        m_eqBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_eqBtn, 1);

        m_cfcBtn = new QPushButton(QStringLiteral("CFC"), this);
        m_cfcBtn->setCheckable(true);
        m_cfcBtn->setFixedHeight(22);
        m_cfcBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_cfcBtn->setStyleSheet(btnStyle);
        m_cfcBtn->setAccessibleName(QStringLiteral("Continuous Frequency Compressor enable"));
        m_cfcBtn->setObjectName(QStringLiteral("TxCfcButton"));
        m_cfcBtn->setToolTip(QStringLiteral(
            "CFC — 10-band continuous frequency compressor. Left-click to "
            "toggle. Right-click to open the CFC dialog."));
        // Right-click → modeless TxCfcDialog (mirrors EQ button pattern).
        m_cfcBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_cfcBtn, 1);

        vbox->addLayout(row);
    }

    // ── Profile combo row (full width) ──────────────────────────────────────
    // Tune Mode combo removed (ATU phase, no plan yet).
    // (AetherSDR TxApplet.cpp:131–153)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        // ── Phase 3M-1c J.1 ─ TX Profile combo ─────────────────────────────────
        // Populated from MicProfileManager via setMicProfileManager().
        // Right-click → emit txProfileMenuRequested (mirrors Thetis
        // comboTXProfile_MouseDown at console.cs:44519-44522 [v2.10.3.13]).
        m_profileCombo = new QComboBox(this);
        m_profileCombo->addItem(QStringLiteral("Default"));
        m_profileCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_profileCombo->setFixedHeight(22);
        applyComboStyle(m_profileCombo);
        m_profileCombo->setAccessibleName(QStringLiteral("TX profile"));
        m_profileCombo->setToolTip(QStringLiteral(
            "TX Profile — left-click to switch.  Right-click to edit "
            "(Setup → Audio → TX Profile)."));
        // Custom context-menu policy so right-click emits
        // customContextMenuRequested instead of the default popup.
        m_profileCombo->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_profileCombo, 1);

        vbox->addLayout(row);
    }

    // ── Plan 4 Cluster C (Task 4 / D2+D3+D9-status): TX BW spinbox row ──────
    // Low/High cutoff spinboxes for the TX bandpass filter.  Bidirectional with
    // TransmitModel::filterLow / filterHigh (wired in wireControls).  Defaults
    // 100 / 2900 Hz from the model (m_filterLow{100} / m_filterHigh{2900}).
    {
        auto* bwRow = new QHBoxLayout;
        bwRow->setSpacing(4);

        // "TX BW" section label — 56 px wide, bold, kTitleText colour, 10 px.
        auto* bwLbl = new QLabel(QStringLiteral("TX BW"), this);
        bwLbl->setFixedWidth(56);
        bwLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; font-weight: bold; }"
        ).arg(Style::kTitleText));
        bwRow->addWidget(bwLbl);

        // "Lo" sub-label
        auto* loLbl = new QLabel(QStringLiteral("Lo"), this);
        loLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9.5px; }"
        ).arg(Style::kTextSecondary));
        bwRow->addWidget(loLbl);

        // Low-cutoff spinbox — range [0, 5000] Hz
        m_txFilterLowSpin = new QSpinBox(this);
        m_txFilterLowSpin->setRange(0, 5000);
        m_txFilterLowSpin->setSuffix(QStringLiteral(" Hz"));
        m_txFilterLowSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
        m_txFilterLowSpin->setMinimumWidth(72);
        m_txFilterLowSpin->setAccessibleName(QStringLiteral("TX filter low cutoff"));
        m_txFilterLowSpin->setToolTip(QStringLiteral(
            "TX bandpass filter lower cutoff (Hz).  0 Hz for voice SSB modes."));
        bwRow->addWidget(m_txFilterLowSpin);

        // "Hi" sub-label
        auto* hiLbl = new QLabel(QStringLiteral("Hi"), this);
        hiLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9.5px; }"
        ).arg(Style::kTextSecondary));
        bwRow->addWidget(hiLbl);

        // High-cutoff spinbox — range [200, 10000] Hz
        m_txFilterHighSpin = new QSpinBox(this);
        m_txFilterHighSpin->setRange(200, 10000);
        m_txFilterHighSpin->setSuffix(QStringLiteral(" Hz"));
        m_txFilterHighSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
        m_txFilterHighSpin->setMinimumWidth(72);
        m_txFilterHighSpin->setAccessibleName(QStringLiteral("TX filter high cutoff"));
        m_txFilterHighSpin->setToolTip(QStringLiteral(
            "TX bandpass filter upper cutoff (Hz).  2900 Hz for typical SSB voice."));
        bwRow->addWidget(m_txFilterHighSpin);

        vbox->addLayout(bwRow);

        // D9 status label — orange tint, right-aligned, 9 px bold.
        // Displays e.g. "100-2900 Hz · 2.8k BW" (asymmetric) or "±2900 Hz · 5.8k BW"
        // (symmetric modes).  Refreshed by filterChanged + dspModeChanged.
        m_txFilterStatusLabel = new QLabel(this);
        m_txFilterStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_txFilterStatusLabel->setStyleSheet(QStringLiteral(
            // Plan 4 D9 (Cluster E): colour centralised in Style::kTxFilterOverlayLabel.
            "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
        ).arg(QLatin1String(Style::kTxFilterOverlayLabel)));
        m_txFilterStatusLabel->setAccessibleName(QStringLiteral("TX filter status"));
        vbox->addWidget(m_txFilterStatusLabel);
    }

    // ── Button row 3: 2-Tone + PS-A + DUP ───────────────────────────────────
    // Phase 3M-1a H.3: 2-Tone and PS-A are hidden until their owning phases land.
    //   2-Tone: TODO [3M-3]: visible when 2-tone test feature lands.
    //   PS-A:   TODO [3M-4]: visible when PureSignal lands.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        // ── Phase 3M-1c J.2 ─ 2-TONE button ────────────────────────────────────
        // Mirrors Thetis chk2TONE_CheckedChanged (console.cs:44728-44760
        // [v2.10.3.13]).  Wired to TwoToneController via
        // setTwoToneController().  The TUN-stop pre-step + 300 ms settle
        // delay live inside TwoToneController::setActive (Phase I.3) so the
        // button itself just dispatches setActive(checked).
        m_twoToneBtn = new QPushButton(QStringLiteral("2-Tone"), this);
        m_twoToneBtn->setCheckable(true);
        m_twoToneBtn->setFixedHeight(22);
        m_twoToneBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_twoToneBtn->setStyleSheet(Style::buttonBaseStyle() + Style::redCheckedStyle());
        m_twoToneBtn->setAccessibleName(QStringLiteral("2-tone test"));
        m_twoToneBtn->setToolTip(QStringLiteral(
            "Continuous or pulsed two-tone IMD test "
            "(configure in Setup → Test → Two-Tone)."));
        row->addWidget(m_twoToneBtn, 1);

        // PS-A: green when checked — #006030/#008040 are a darker green than kGreenBg=#006040.
        // Flagged for 3M-4 PureSignal phase review; do NOT snap to Style::kGreenBg/kGreenBorder
        // until PureSignal colors are audited against Thetis APD button palette.
        m_psaBtn = new QPushButton(QStringLiteral("PS-A"), this);
        m_psaBtn->setCheckable(true);
        m_psaBtn->setFixedHeight(22);
        m_psaBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_psaBtn->setStyleSheet(Style::buttonBaseStyle()
            + QStringLiteral("QPushButton:checked {"
                             " background: #006030; border: 1px solid #008040; color: #fff; }"));
        m_psaBtn->setAccessibleName(QStringLiteral("PS-A PureSignal"));
        m_psaBtn->setToolTip(QStringLiteral("PureSignal — not yet implemented (Phase 3M-4)"));
        m_psaBtn->setVisible(false);  // TODO [3M-4]: visible when PureSignal lands
        NyiOverlay::markNyi(m_psaBtn, QStringLiteral("Phase 3M-4"));
        row->addWidget(m_psaBtn, 1);

        vbox->addLayout(row);
    }

    // ── SWR protection LED inset ─────────────────────────────────────────────
    // xPA button removed (external-PA hardware-specific phase, no plan yet).
    // SWR Prot LED now occupies its own full-width inset row.
    // Inset: fixedHeight 22, bg #0a0a18, border #1e2e3e (AetherSDR TxApplet.cpp:224–253)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Inset container for SWR protection LED (styled like AetherSDR atuInset)
        auto* inset = new QWidget(this);
        inset->setFixedHeight(22);
        inset->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        inset->setObjectName(QStringLiteral("xpaInset"));
        inset->setStyleSheet(QStringLiteral(
            "#xpaInset { background: %1; border: 1px solid %2; border-radius: 3px; }"
            "#xpaInset QLabel { border: none; background: transparent; }"
        ).arg(Style::kInsetBg, Style::kInsetBorder));

        auto* insetLayout = new QHBoxLayout(inset);
        insetLayout->setContentsMargins(4, 0, 4, 0);
        insetLayout->setSpacing(2);

        // 15. SWR protection LED — inactive: #405060, 9px bold
        // Matches AetherSDR makeIndicator() pattern (TxApplet.cpp:22–27)
        m_swrProtLed = new QLabel(QStringLiteral("SWR Prot"), inset);
        m_swrProtLed->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
        ).arg(Style::kTextInactive));
        m_swrProtLed->setAlignment(Qt::AlignCenter);
        m_swrProtLed->setAccessibleName(QStringLiteral("SWR protection indicator"));
        insetLayout->addWidget(m_swrProtLed);

        row->addWidget(inset, 1); // full width (xPA removed)
        vbox->addLayout(row);
    }

    vbox->addStretch();
}

// ── Phase 3M-1a H.3 wiring ──────────────────────────────────────────────────
//
// wireControls: called once after buildUI(). Attaches signal/slot connections
// between the four wired controls and the model layer.
//
// Pattern follows the NereusSDR "GUI↔Model sync, no feedback loops" rule:
//   - Use QSignalBlocker (or m_updatingFromModel) to prevent echo loops.
//   - Model setters emit signals → RadioConnection sends protocol commands.
//   - UI state changes → model setters → emit back to update other UI.
void TxApplet::wireControls()
{
    if (!m_model) {
        return;
    }

    TransmitModel& tx = m_model->transmitModel();
    MoxController* mox = m_model->moxController();

    // ── Forward-power gauge ← RadioStatus::powerChanged ─────────────────────
    // 3M-1a (2026-04-27): wire the previously-NYI fwd-power gauge to the
    // radio's reported forward power.  Pipeline:
    //   P2RadioConnection → paTelemetryUpdated(fwdRaw,...)
    //     → RadioModel handler scaleFwdPowerWatts() → m_radioStatus.setForwardPower(W)
    //     → RadioStatus::powerChanged(fwd, rev, swr)  ← we listen here
    //
    // Two-stage de-jitter (Phase 3I-1 will replace with the proper Thetis
    // peak-decay filter):
    //   1.  alpha=0.10 EMA on every powerChanged emit — smooths the per-
    //       sample noise without much lag (~10 sample time-constant).
    //   2.  10 Hz QTimer reads the smoothed state into the gauge — keeps
    //       the visible digits stable even when RadioStatus::powerChanged
    //       fires twice per hardware sample (documented in
    //       RadioModel.cpp:572).  Without this throttle the digit
    //       characters update too fast to read.
    connect(&m_model->radioStatus(), &RadioStatus::powerChanged,
            this, [this](double fwdW, double /*revW*/, double swr) {
        constexpr double kAlpha = 0.30;
        m_fwdPowerSmoothedW = kAlpha * fwdW + (1.0 - kAlpha) * m_fwdPowerSmoothedW;
        if (m_swrGauge) {
            m_swrGauge->setValue(swr);
        }
    });
    auto* fwdGaugeRefreshTimer = new QTimer(this);
    fwdGaugeRefreshTimer->setInterval(50);   // 20 Hz UI refresh
    connect(fwdGaugeRefreshTimer, &QTimer::timeout, this, [this]() {
        if (m_fwdPowerGauge) {
            m_fwdPowerGauge->setValue(m_fwdPowerSmoothedW);
        }
    });
    fwdGaugeRefreshTimer->start();

    // Bench-reported #167 follow-up: TxApplet's RF Pwr / SWR HGauges have
    // their own EMA smoothing (m_fwdPowerSmoothedW, alpha=0.30) separate
    // from the MeterPanel's BarItem stack.  A single zero from
    // RadioStatus::powerChanged after un-key only walks the smoothed
    // value down by 30% (e.g. 60 → 42), then no more updates flow so the
    // gauge stays stuck at ~42.  Snap the smoothed state to 0 on MOX
    // falling-edge so the gauge clears instantly — same idea as the
    // BarItem clearSmoothing path in MeterPoller::setInTx.  Subscribed
    // to MoxController::moxStateChanged (the authoritative TX boundary,
    // covers MOX un-key AND TUNE release) rather than orphan
    // TransmitModel::moxChanged signals.
    if (m_model && m_model->moxController()) {
        connect(m_model->moxController(), &MoxController::moxStateChanged,
                this, [this](bool active) {
            if (active) { return; }      // rising-edge: smoothing takes over
            m_fwdPowerSmoothedW = 0.0;
            if (m_fwdPowerGauge) { m_fwdPowerGauge->setValue(0.0); }
            if (m_swrGauge)      { m_swrGauge->setValue(1.0); }
        });
    }

    // Per-SKU RF Pwr gauge rescale.  Bench-reported #167 follow-up: the
    // 0-120 W default scale made HL2 (5 W) and ANAN-G2-1K (1000 W) both
    // unreadable.  Subscribe to currentRadioChanged so the gauge ticks
    // and red-zone redraw whenever the active radio changes.
    //
    // Issue #175 Task 7: also rescale the RF Power and Tune Power sliders
    // (HL2 → 0..90/6 and 0..99/3 with -X.X dB labels via mi0bot formulae;
    //  every other SKU → canonical 0..100/1 with bare integer labels).
    connect(m_model, &RadioModel::currentRadioChanged, this,
            [this](const NereusSDR::RadioInfo&) {
        const auto model = m_model->hardwareProfile().model;
        rescaleFwdGaugeForModel(model);
        rescalePowerSlidersForModel(model);
    });
    // Apply the current model's scale at wireControls() time so cold-launch
    // (or reconnect-on-startup) lands a properly-sized gauge + sliders before
    // the first telemetry sample.
    {
        const auto model = m_model->hardwareProfile().model;
        rescaleFwdGaugeForModel(model);
        rescalePowerSlidersForModel(model);
    }

    // ── SWR Prot LED ← SwrProtectionController::highSwrChanged ─────────────
    // SwrProtectionController (Phase 3G-13) emits highSwrChanged(bool) when
    // the radio's SWR-protection state changes. Light the LED amber when
    // high-SWR protection is active; dim it when cleared.
    {
        auto updateSwrProtLed = [this](bool isHigh) {
            if (!m_swrProtLed) { return; }
            const QString color = isHigh ? QStringLiteral("#ffaa00")
                                         : Style::kTextInactive;
            m_swrProtLed->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
            ).arg(color));
        };
        connect(&m_model->swrProt(),
                &safety::SwrProtectionController::highSwrChanged,
                this, updateSwrProtLed);
        // Initialise to current state.
        updateSwrProtLed(m_model->swrProt().highSwr());
    }

    // ── RF Power slider → TransmitModel::setPower(int) ──────────────────────
    // From Thetis chkMOX_CheckedChanged2 power flow [v2.10.3.13]:
    //   the RF Power slider maps 0–100 to TX drive level.
    //
    // Issue #175 Task 7: label text is now driven by updatePowerSliderLabels()
    // which formats the value in -X.X dB on HL2 and bare integer on every
    // other SKU.  The old direct setText(QString::number(val)) would have
    // bypassed the dB conversion on HL2.
    connect(m_rfPowerSlider, &QSlider::valueChanged, this, [this, &tx](int val) {
        updatePowerSliderLabels();
        if (m_updatingFromModel) { return; }
        tx.setPower(val);
        // Per-band write: matches Thetis ptbPWR_Scroll at console.cs:28642
        // [v2.10.3.13] (`power_by_band[(int)_tx_band] = ptbPWR.Value;`).
        // Without this, the per-band slot only updates indirectly via the
        // setPowerUsingTargetDbm txMode-0 side-effect (TransmitModel.cpp:825),
        // which is gated on connected radio + loaded PA profile + !TUNE.
        // Result: slider moves while disconnected (or before profiles load)
        // never persist across restart.  setPowerForBand auto-persists to
        // hardware/<mac>/powerByBand/<band> when m_persistMac is non-empty.
        //
        // Source the band from the active slice (the canonical TX band per
        // RadioModel.cpp:903-905), NOT m_currentBand.  m_currentBand tracks
        // UI state and is fed by both PanadapterModel::bandChanged AND
        // SliceModel::frequencyChanged, so it can drift to the panadapter
        // band on CTUN pans without slice retune — writing through it would
        // silently corrupt other bands' stored values.  txBand() falls back
        // to m_currentBand when the active slice is unavailable.
        tx.setPowerForBand(txBand(), val);
        // Symmetric to the tune-slider auto-switch above: touching the RF
        // Power slider restores the tune source to DriveSlider so the
        // setPowerUsingTargetDbm txMode 1 branch reads tx.power() during
        // TUNE.  Last-touched-slider-wins UX. From Thetis console.cs:46553
        // [v2.10.3.13] DrivePowerSource.DRIVE_SLIDER is the canonical default.
        tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
    });

    // Reverse: TransmitModel::powerChanged → slider
    connect(&tx, &TransmitModel::powerChanged, this, [this](int power) {
        QSignalBlocker b(m_rfPowerSlider);
        m_updatingFromModel = true;
        m_rfPowerSlider->setValue(power);
        updatePowerSliderLabels();   // Issue #175 Task 7: HL2 dB / non-HL2 int
        m_updatingFromModel = false;
    });

    // ── Tune Power slider → TransmitModel::setTunePowerForBand ──────────────
    // Per-band tune power, ported from Thetis console.cs:12094 [v2.10.3.13]:
    //   private int[] tunePower_by_band;
    // The current band is tracked by m_currentBand (updated by setCurrentBand).
    //
    // Issue #175 Task 7: label text routed through updatePowerSliderLabels()
    // for the HL2 (slider/3.0 - 33.0)/2.0 dB conversion.
    connect(m_tunePwrSlider, &QSlider::valueChanged, this, [this, &tx](int val) {
        updatePowerSliderLabels();
        if (m_updatingFromModel) { return; }
        tx.setTunePowerForBand(m_currentBand, val);
        // When the user touches the tune slider, switch the tune drive
        // source so TUNE actually reads from tunePowerForBand instead of
        // the regular drive slider (the default per Thetis console.cs:46553
        // [v2.10.3.13]).  Without this the tune slider is dead UI: its
        // value persists per-band but the math kernel
        // (TransmitModel::setPowerUsingTargetDbm txMode 1) only consults
        // tunePowerForBand when m_tuneDrivePowerSource == TuneSlider.
        // NereusSDR-spin: Thetis exposes _tuneDrivePowerSource via a
        // separate Setup combo; NereusSDR follows last-touched-slider-wins
        // UX so users don't need to know the enum exists.
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
    });

    // Reverse: TransmitModel::tunePowerByBandChanged → slider (only for current band)
    connect(&tx, &TransmitModel::tunePowerByBandChanged,
            this, [this](Band band, int watts) {
        if (band != m_currentBand) { return; }
        QSignalBlocker b(m_tunePwrSlider);
        m_updatingFromModel = true;
        m_tunePwrSlider->setValue(watts);
        updatePowerSliderLabels();   // Issue #175 Task 7: HL2 dB / non-HL2 int
        m_updatingFromModel = false;
    });

    // ── TUNE button → RadioModel::setTune(bool) ──────────────────────────────
    // G.4 orchestrator: CW mode swap, tone setup, tune-power push.
    // From Thetis console.cs:29978-30157 [v2.10.3.13] chkTUN_CheckedChanged.
    // G8NJJ tell ARIES that tune is active  [original inline comment from console.cs:30153]
    // MW0LGE_22b setupTuneDriveSlider  [original inline comment from console.cs:30155]
    connect(m_tuneBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_updatingFromModel) { return; }
        if (!m_model) { return; }
        m_model->setTune(on);
        if (on) {
            m_tuneBtn->setText(QStringLiteral("TUNING..."));
        } else {
            m_tuneBtn->setText(QStringLiteral("TUNE"));
        }
    });

    // Reverse: tuneRefused → uncheck TUN button + clear text.
    // From Thetis console.cs:30076 [v2.10.3.13]: guard conditions before
    // chkTUN.Checked = true (connection + power-on checks).
    connect(m_model, &RadioModel::tuneRefused, this, [this](const QString& /*reason*/) {
        QSignalBlocker b(m_tuneBtn);
        m_updatingFromModel = true;
        m_tuneBtn->setChecked(false);
        m_tuneBtn->setText(QStringLiteral("TUNE"));
        m_updatingFromModel = false;
    });

    // ── MOX button → MoxController::setMox(bool) ────────────────────────────
    // B.5 setter: drives state machine through RX→TX or TX→RX transitions.
    // From Thetis console.cs:29311-29678 [v2.10.3.13] chkMOX_CheckedChanged2.
    // //[2.10.1.0]MW0LGE changed  [original inline comment from console.cs:29355]
    // //MW0LGE [2.9.0.7]  [original inline comment from console.cs:29400, 29561]
    // //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29567-29568, 29659]
    if (mox) {
        connect(m_moxBtn, &QPushButton::toggled, this, [this, mox](bool on) {
            if (m_updatingFromModel) { return; }
            mox->setMox(on);
        });

        // Reverse: MoxController::moxStateChanged → button checked state.
        // moxStateChanged fires at end of the timer walk (TX fully engaged
        // or fully released), not at setMox() entry — so the button reflects
        // the confirmed state, not the in-progress request.
        connect(mox, &MoxController::moxStateChanged,
                this, [this](bool on) {
            QSignalBlocker b(m_moxBtn);
            m_updatingFromModel = true;
            m_moxBtn->setChecked(on);
            m_updatingFromModel = false;
        });

        // TUNE button checked state driven by manualMoxChanged.
        // manualMoxChanged fires when setTune() sets/clears m_manualMox.
        connect(mox, &MoxController::manualMoxChanged,
                this, [this](bool isManual) {
            QSignalBlocker b(m_tuneBtn);
            m_updatingFromModel = true;
            m_tuneBtn->setChecked(isManual);
            m_tuneBtn->setText(isManual
                ? QStringLiteral("TUNING...")
                : QStringLiteral("TUNE"));
            m_updatingFromModel = false;
        });
    }

    // ── K.2: MOX button tooltip override ← SliceModel::dspModeChanged ─────────
    // Phase 3M-1b K.2: update MOX button tooltip when DSP mode changes.
    // For modes that are deferred to a later phase (CW → 3M-2, AM/FM → 3M-3)
    // the tooltip explains why MOX won't engage, matching the moxRejected reason.
    // Wired here (wireControls) rather than syncFromModel because the active
    // slice can change after construction.
    if (m_model) {
        if (SliceModel* slice = m_model->activeSlice()) {
            // Wire the active slice's dspModeChanged to onMoxModeChanged.
            connect(slice, &SliceModel::dspModeChanged,
                    this, &TxApplet::onMoxModeChanged);
            // Set initial tooltip from current mode.
            onMoxModeChanged(slice->dspMode());
        }
    }

    // ── 4b. VOX row wiring (3M-3a-iii bench polish 2026-05-04) ────────────────
    //
    // Bidirectional binds for [VOX] toggle + threshold/hold sliders, plus
    // right-click → openSetupRequested("Transmit", "DEXP/VOX") and a 100 ms
    // QTimer driving m_voxPeakMeter (TxApplet::pollVoxMeter).
    // Mirrors the wiring pattern from PhoneCwApplet (Phase 3M-3a-iii Task
    // 15) — relocated here as part of the 2026-05-04 bench polish.

    // ── VOX [ON] toggle ↔ TransmitModel::voxEnabled ──────────────────────────
    if (m_voxBtn) {
        {
            QSignalBlocker b(m_voxBtn);
            m_voxBtn->setChecked(tx.voxEnabled());
        }
        // UI → Model
        connect(m_voxBtn, &QPushButton::toggled, this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setVoxEnabled(on);
        });
        // Model → UI
        connect(&tx, &TransmitModel::voxEnabledChanged, this, [this](bool on) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_voxBtn);
                m_voxBtn->setChecked(on);
            }
            m_updatingFromModel = false;
        });
    }

    // ── VOX threshold slider ↔ TransmitModel::voxThresholdDb ─────────────────
    // Range -80..0 dB from console.Designer.cs:6018-6019 [v2.10.3.13]
    // (already set in buildUI).  Default value -20 dB matches Thetis
    // ptbVOX.Value=-20 (console.Designer.cs:6024 [v2.10.3.13]).
    if (m_voxSlider) {
        {
            QSignalBlocker b(m_voxSlider);
            m_voxSlider->setValue(tx.voxThresholdDb());
        }
        if (m_voxLvlLabel) {
            m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(tx.voxThresholdDb()));
        }
        // UI → Model + label refresh
        connect(m_voxSlider, &QSlider::valueChanged, this, [this, &tx](int dB) {
            if (m_voxLvlLabel) {
                m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(dB));
            }
            if (m_updatingFromModel) { return; }
            tx.setVoxThresholdDb(dB);
        });
        // Model → UI
        connect(&tx, &TransmitModel::voxThresholdDbChanged, this, [this](int dB) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_voxSlider);
                m_voxSlider->setValue(dB);
            }
            if (m_voxLvlLabel) {
                m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(dB));
            }
            m_updatingFromModel = false;
        });
    }

    // ── VOX Hold slider ↔ TransmitModel::voxHangTimeMs ───────────────────────
    // Range 1..2000 ms from setup.designer.cs:45005-45013 [v2.10.3.13]
    // (already set in buildUI).  Default 500 ms matches udDEXPHold.Value
    // (setup.designer.cs:45020 [v2.10.3.13]).
    if (m_voxDlySlider) {
        {
            QSignalBlocker b(m_voxDlySlider);
            m_voxDlySlider->setValue(tx.voxHangTimeMs());
        }
        if (m_voxDlyLabel) {
            m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(tx.voxHangTimeMs()));
        }
        // UI → Model + label refresh
        connect(m_voxDlySlider, &QSlider::valueChanged, this, [this, &tx](int ms) {
            if (m_voxDlyLabel) {
                m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(ms));
            }
            if (m_updatingFromModel) { return; }
            tx.setVoxHangTimeMs(ms);
        });
        // Model → UI
        connect(&tx, &TransmitModel::voxHangTimeMsChanged, this, [this](int ms) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_voxDlySlider);
                m_voxDlySlider->setValue(ms);
            }
            if (m_voxDlyLabel) {
                m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(ms));
            }
            m_updatingFromModel = false;
        });
    }

    // ── Right-click on VOX → emit openSetupRequested ─────────────────────────
    // Mirrors PhoneCwApplet's DEXP-button right-click pattern.  MainWindow
    // listens and jumps the SetupDialog to the "DEXP/VOX" leaf page.
    if (m_voxBtn) {
        connect(m_voxBtn, &QPushButton::customContextMenuRequested, this,
                [this](const QPoint&) {
            emit openSetupRequested(QStringLiteral("Transmit"),
                                    QStringLiteral("DEXP/VOX"));
        });
    }

    // ── 100 ms timer driving m_voxPeakMeter ──────────────────────────────────
    // Cadence matches Thetis UpdateNoiseGate Task.Delay(100) at
    // console.cs:25347 [v2.10.3.13].  Stops automatically when the applet
    // is destroyed (parented to `this`).  Continuous (NOT MOX-gated) since
    // GetDEXPPeakSignal is the live DEXP detector envelope, not the
    // TX-pipeline meter.
    m_voxMeterTimer = new QTimer(this);
    m_voxMeterTimer->setInterval(100);
    connect(m_voxMeterTimer, &QTimer::timeout,
            this, &TxApplet::pollVoxMeter);
    m_voxMeterTimer->start();

    // ── MON toggle button ↔ TransmitModel::monEnabled ────────────────────────
    // Phase 3M-1b J.3.
    // UI → Model: toggled → setMonEnabled (with m_updatingFromModel guard).
    // Model → UI: monEnabledChanged → update checked state with QSignalBlocker.
    connect(m_monBtn, &QPushButton::toggled, this, [this, &tx](bool on) {
        if (m_updatingFromModel) { return; }
        tx.setMonEnabled(on);
    });

    connect(&tx, &TransmitModel::monEnabledChanged, this, [this](bool on) {
        QSignalBlocker b(m_monBtn);
        m_updatingFromModel = true;
        m_monBtn->setChecked(on);
        m_updatingFromModel = false;
    });

    // ── Monitor volume slider ↔ TransmitModel::monitorVolume ─────────────────
    // Phase 3M-1b J.3.
    // UI → Model: slider valueChanged(int) → setMonitorVolume(value / 100.0f).
    // Model → UI: monitorVolumeChanged(float) → slider = qRound(v * 100.0f).
    connect(m_monitorVolumeSlider, &QSlider::valueChanged,
            this, [this, &tx](int val) {
        if (m_updatingFromModel) { return; }
        m_monitorVolumeValue->setText(QString::number(val));
        tx.setMonitorVolume(static_cast<float>(val) / 100.0f);
    });

    connect(&tx, &TransmitModel::monitorVolumeChanged,
            this, [this](float volume) {
        QSignalBlocker b(m_monitorVolumeSlider);
        m_updatingFromModel = true;
        const int uiVal = qRound(volume * 100.0f);
        m_monitorVolumeSlider->setValue(uiVal);
        m_monitorVolumeValue->setText(QString::number(uiVal));
        m_updatingFromModel = false;
    });

    // ── Phase 3M-3a-i Batch 2 (Task F): LEV / EQ quick toggles ──────────────
    //
    // LEV button ↔ TransmitModel::txLevelerOn (bidirectional, echo-guarded).
    if (m_levBtn) {
        connect(m_levBtn, &QPushButton::toggled,
                this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setTxLevelerOn(on);
        });
        connect(&tx, &TransmitModel::txLevelerOnChanged,
                this, [this](bool on) {
            QSignalBlocker b(m_levBtn);
            m_updatingFromModel = true;
            m_levBtn->setChecked(on);
            m_updatingFromModel = false;
        });
    }

    // EQ button ↔ TransmitModel::txEqEnabled (bidirectional, echo-guarded).
    // Right-click: placeholder slot for the TxEqDialog launch (Batch 3).
    if (m_eqBtn) {
        connect(m_eqBtn, &QPushButton::toggled,
                this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setTxEqEnabled(on);
        });
        connect(&tx, &TransmitModel::txEqEnabledChanged,
                this, [this](bool on) {
            QSignalBlocker b(m_eqBtn);
            m_updatingFromModel = true;
            m_eqBtn->setChecked(on);
            m_updatingFromModel = false;
        });
        // Right-click → open TxEqDialog (Phase 3M-3a-i Batch 3 A.1).
        // Modeless singleton; show + raise + activateWindow so a
        // hidden-but-alive instance is brought forward.
        connect(m_eqBtn, &QPushButton::customContextMenuRequested,
                this, [this](const QPoint& /*pos*/) {
            if (!m_model) { return; }
            TxEqDialog* dlg = TxEqDialog::instance(m_model, this);
            dlg->show();
            dlg->raise();
            dlg->activateWindow();
        });
    }

    // ── Phase 3M-3a-ii Batch 6 (Task A): CFC button ↔ TransmitModel::cfcEnabled
    // Right-click opens the modeless TxCfcDialog (lazy-create singleton kept
    // alive for fast re-opens — same pattern as TxEqDialog).
    if (m_cfcBtn) {
        connect(m_cfcBtn, &QPushButton::toggled,
                this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setCfcEnabled(on);
        });
        connect(&tx, &TransmitModel::cfcEnabledChanged,
                this, [this](bool on) {
            QSignalBlocker b(m_cfcBtn);
            m_updatingFromModel = true;
            m_cfcBtn->setChecked(on);
            m_updatingFromModel = false;
        });
        connect(m_cfcBtn, &QPushButton::customContextMenuRequested,
                this, [this](const QPoint& /*pos*/) {
            requestOpenCfcDialog();
        });
    }

    // ── Mic-source badge ← TransmitModel::micSourceChanged ───────────────────
    // Phase 3M-1b J.3. Read-only: updates badge text on signal, no user interaction.
    // "PC mic" for MicSource::Pc, "Radio mic" for MicSource::Radio.
    connect(&tx, &TransmitModel::micSourceChanged,
            this, [this](MicSource source) {
        m_micSourceBadge->setText(
            source == MicSource::Radio
                ? QStringLiteral("Radio mic")
                : QStringLiteral("PC mic"));
    });

    // ── Phase 3M-1c J.1 ─ TX Profile combo wiring ────────────────────────────
    // User-driven currentTextChanged → MicProfileManager::setActiveProfile.
    // Guarded with m_updatingFromModel so the rebuildProfileCombo() echo
    // doesn't bounce back into setActiveProfile.
    connect(m_profileCombo, &QComboBox::currentTextChanged,
            this, [this](const QString& name) {
        if (m_updatingFromModel) { return; }
        if (!m_micProfileMgr) { return; }
        if (name.isEmpty()) { return; }
        if (m_model) {
            m_micProfileMgr->setActiveProfile(name, &m_model->transmitModel());
        }
    });

    // Right-click on combo → emit txProfileMenuRequested (Thetis cite at the
    // signal declaration).
    connect(m_profileCombo, &QComboBox::customContextMenuRequested,
            this, [this](const QPoint& /*pos*/) {
        emit txProfileMenuRequested();
    });

    // ── Plan 4 Cluster C (Task 4 / D2+D3): TX BW spinbox wiring ─────────────
    //
    // UI → Model: spinbox valueChanged → setFilterLow / setFilterHigh.
    //             m_updatingFromModel guard prevents echo loops.
    // Model → UI: TransmitModel::filterChanged(int,int) → QSignalBlocker on
    //             both spinboxes, then setValue + refresh status label.
    // Status label refresh helper (shared by filterChanged and dspModeChanged).
    auto refreshFilterStatus = [this]() {
        if (!m_txFilterStatusLabel || !m_model) { return; }
        SliceModel* slice = m_model->activeSlice();
        const DSPMode mode = slice ? slice->dspMode() : DSPMode::USB;
        m_txFilterStatusLabel->setText(
            m_model->transmitModel().filterDisplayText(mode));
    };

    if (m_txFilterLowSpin) {
        connect(m_txFilterLowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int v) {
            if (m_updatingFromModel) { return; }
            m_model->transmitModel().setFilterLow(v);
        });
    }
    if (m_txFilterHighSpin) {
        connect(m_txFilterHighSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int v) {
            if (m_updatingFromModel) { return; }
            m_model->transmitModel().setFilterHigh(v);
        });
    }

    // Model → spinboxes + status label on filterChanged.
    connect(&tx, &TransmitModel::filterChanged,
            this, [this, refreshFilterStatus](int low, int high) {
        m_updatingFromModel = true;
        if (m_txFilterLowSpin) {
            QSignalBlocker bLo(m_txFilterLowSpin);
            m_txFilterLowSpin->setValue(low);
        }
        if (m_txFilterHighSpin) {
            QSignalBlocker bHi(m_txFilterHighSpin);
            m_txFilterHighSpin->setValue(high);
        }
        m_updatingFromModel = false;
        refreshFilterStatus();
    });

    // Status label refresh on DSP mode change (symmetric ↔ asymmetric format).
    // Piggybacks on the same active-slice connect block used by K.2 above.
    if (SliceModel* slice = m_model->activeSlice()) {
        connect(slice, &SliceModel::dspModeChanged,
                this, [refreshFilterStatus](DSPMode) {
            refreshFilterStatus();
        });
        // Set initial status label text.
        refreshFilterStatus();
    }

    // ── Phase 3M-1c J.2 ─ 2-TONE button wiring ───────────────────────────────
    // toggled → TwoToneController::setActive.  Echo-guarded.
    connect(m_twoToneBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_updatingFromModel) { return; }
        if (!m_twoToneCtrl) { return; }
        m_twoToneCtrl->setActive(on);
    });

    // ── Initial sync from model ──────────────────────────────────────────────
    syncFromModel();
}

void TxApplet::syncFromModel()
{
    if (!m_model) { return; }

    TransmitModel& tx = m_model->transmitModel();
    MoxController* mox = m_model->moxController();

    m_updatingFromModel = true;

    // RF Power
    {
        QSignalBlocker b(m_rfPowerSlider);
        m_rfPowerSlider->setValue(tx.power());
    }

    // Tune Power for current band
    {
        QSignalBlocker b(m_tunePwrSlider);
        const int tunePwr = tx.tunePowerForBand(m_currentBand);
        m_tunePwrSlider->setValue(tunePwr);
    }

    // Issue #175 Task 7: refresh both labels through the central
    // formatter (HL2 dB / non-HL2 int).  Replaces the two inline
    // QString::number() calls that bypassed HL2 dB conversion above.
    updatePowerSliderLabels();

    // VOX [ON] toggle + Threshold slider + Hold slider
    // (3M-3a-iii bench polish 2026-05-04 — relocated from PhoneCwApplet).
    // Bidirectional sync to TransmitModel::voxEnabled / voxThresholdDb /
    // voxHangTimeMs.  Note: voxEnabled is NEVER persisted (safety: VOX
    // always starts OFF), but we still pull whatever the current model
    // state is so any other UI surface (e.g. Setup → DEXP/VOX page) stays
    // in agreement.
    if (m_voxBtn) {
        QSignalBlocker bv(m_voxBtn);
        m_voxBtn->setChecked(tx.voxEnabled());
    }
    if (m_voxSlider) {
        {
            QSignalBlocker b(m_voxSlider);
            m_voxSlider->setValue(tx.voxThresholdDb());
        }
        if (m_voxLvlLabel) {
            m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(tx.voxThresholdDb()));
        }
    }
    if (m_voxDlySlider) {
        {
            QSignalBlocker b(m_voxDlySlider);
            m_voxDlySlider->setValue(tx.voxHangTimeMs());
        }
        if (m_voxDlyLabel) {
            m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(tx.voxHangTimeMs()));
        }
    }

    // MON button state (J.3 Phase 3M-1b)
    // monEnabled intentionally loads as OFF — plan §0 row 9 safety rule.
    if (m_monBtn) {
        QSignalBlocker bm(m_monBtn);
        m_monBtn->setChecked(tx.monEnabled());
    }

    // Monitor volume slider (J.3 Phase 3M-1b)
    // Sync slider position from model; default 0.5f → slider 50.
    if (m_monitorVolumeSlider) {
        QSignalBlocker bvol(m_monitorVolumeSlider);
        const int uiVal = qRound(tx.monitorVolume() * 100.0f);
        m_monitorVolumeSlider->setValue(uiVal);
        m_monitorVolumeValue->setText(QString::number(uiVal));
    }

    // LEV / EQ / CFC button state (3M-3a-i Batch 2 Task F + 3M-3a-ii
    // Batch 6 Task A — CFC button added; PROC moved to PhoneCwApplet
    // 3M-3a-ii post-bench cleanup).
    if (m_levBtn) {
        QSignalBlocker b(m_levBtn);
        m_levBtn->setChecked(tx.txLevelerOn());
    }
    if (m_eqBtn) {
        QSignalBlocker b(m_eqBtn);
        m_eqBtn->setChecked(tx.txEqEnabled());
    }
    if (m_cfcBtn) {
        QSignalBlocker b(m_cfcBtn);
        m_cfcBtn->setChecked(tx.cfcEnabled());
    }

    // TX BW spinboxes + status label (Plan 4 Cluster C Task 4 / D2+D3+D9)
    if (m_txFilterLowSpin) {
        QSignalBlocker bLo(m_txFilterLowSpin);
        m_txFilterLowSpin->setValue(tx.filterLow());
    }
    if (m_txFilterHighSpin) {
        QSignalBlocker bHi(m_txFilterHighSpin);
        m_txFilterHighSpin->setValue(tx.filterHigh());
    }
    if (m_txFilterStatusLabel) {
        SliceModel* slice = m_model->activeSlice();
        const DSPMode mode = slice ? slice->dspMode() : DSPMode::USB;
        m_txFilterStatusLabel->setText(tx.filterDisplayText(mode));
    }

    // Mic-source badge (J.3 Phase 3M-1b)
    if (m_micSourceBadge) {
        m_micSourceBadge->setText(
            tx.micSource() == MicSource::Radio
                ? QStringLiteral("Radio mic")
                : QStringLiteral("PC mic"));
    }

    // MOX / TUNE button state
    if (mox) {
        QSignalBlocker bm(m_moxBtn);
        m_moxBtn->setChecked(mox->isMox());

        QSignalBlocker bt(m_tuneBtn);
        const bool isManual = mox->isManualMox();
        m_tuneBtn->setChecked(isManual);
        m_tuneBtn->setText(isManual ? QStringLiteral("TUNING...") : QStringLiteral("TUNE"));
    }

    m_updatingFromModel = false;
}

// (Phase 3M-1b J.2 showVoxSettingsPopup removed in 3M-3a-iii Task 16 —
//  the wired VOX surface lives on PhoneCwApplet now, and the per-parameter
//  popup gave way to right-click → Setup → Transmit → DEXP/VOX.)

void TxApplet::rescaleFwdGaugeForModel(HPSDRModel model)
{
    if (!m_fwdPowerGauge) { return; }

    // Per-SKU PA ceiling from HpsdrModel.h paMaxWattsFor().  Bench-reported
    // #167 follow-up: 0-120 W default scale made HL2 (5 W max) and
    // ANAN-G2-1K (1000 W max) both show meaningless bar widths.
    const int maxW   = paMaxWattsFor(model);
    const double red = static_cast<double>(maxW);
    const double top = red * 1.2;   // 20% headroom past the red zone

    m_fwdPowerGauge->setRange(0.0, top);
    m_fwdPowerGauge->setRedStart(red);
    m_fwdPowerGauge->setYellowStart(red);   // no distinct yellow zone

    // Pick five ticks proportional to the new range: 0 / 1/3 / 2/3 / red / top.
    // For HL2 (red=5): 0 / 1.7 / 3.3 / 5 / 6.  For ANAN-100 (red=100):
    // 0 / 33 / 67 / 100 / 120.  For ANAN-G2-1K (red=1000): 0 / 333 / 667 / 1000 / 1200.
    auto fmt = [maxW](double v) {
        return (maxW <= 10) ? QString::number(v, 'f', 1)   // sub-watt resolution for QRP
                            : QString::number(qRound(v));
    };
    m_fwdPowerGauge->setTickLabels({
        fmt(0.0),
        fmt(red / 3.0),
        fmt(2.0 * red / 3.0),
        fmt(red),
        fmt(top),
    });
}

// ── Issue #175 Task 7: per-SKU power-slider rescale + dB labels ─────────────
//
// From mi0bot-Thetis console.cs:2098-2108 [v2.10.3.13-beta2]
//   if (HPSDRHW.HermesLite == Audio.LastRadioHardware ||
//       HPSDRModel.HERMESLITE == HardwareSpecific.Model)     // MI0BOT: Need an early indication of hardware type due to HL2 rx attenuator can be negative
//   {
//       ptbPWR.Maximum = 90;        // MI0BOT: Changes for HL2 only having a 16 step output attenuator
//       ptbPWR.Value = 0;
//       ptbPWR.LargeChange = 6;
//       ptbPWR.SmallChange = 6;
//       ptbTune.Maximum = 99;
//       ptbTune.Value = 0;
//       ptbTune.LargeChange = 3;
//       ptbTune.SmallChange = 3;
//       ...
//   }
//
// HL2: RF Power slider 0..90 step 6 (16-step output attenuator,
//      0.5 dB/step); Tune slider 0..99 step 3 (33 sub-steps).
// Other SKUs: canonical Thetis 0..100 step 1.
//
// MI0BOT: Changes for HL2 only having a 16 step output attenuator
// [original inline comment from mi0bot console.cs:2098]
void TxApplet::rescalePowerSlidersForModel(HPSDRModel model)
{
    if (!m_rfPowerSlider || !m_tunePwrSlider) { return; }

    // Cache for updatePowerSliderLabels() so the formatter sees the same
    // SKU the slider was rescaled for, even when called outside the live
    // currentRadioChanged path (unit tests, headless harness).
    m_powerSliderModel = model;

    const QSignalBlocker rfBlock(m_rfPowerSlider);
    const QSignalBlocker tunBlock(m_tunePwrSlider);

    m_rfPowerSlider->setRange(0, rfPowerSliderMaxFor(model));
    m_rfPowerSlider->setSingleStep(rfPowerSliderStepFor(model));
    m_rfPowerSlider->setPageStep(rfPowerSliderStepFor(model));

    m_tunePwrSlider->setRange(0, tuneSliderMaxFor(model));
    m_tunePwrSlider->setSingleStep(tuneSliderStepFor(model));
    m_tunePwrSlider->setPageStep(tuneSliderStepFor(model));

    // Tooltip - Thetis-faithful on every SKU.  Replaces the previous
    // "RF output power (0-100 W)" / "Tune carrier power for current band
    // (0-100 W)" wording, which contradicted Thetis semantics on every
    // SKU (the slider is a relative drive level, not watts).
    //
    // From Thetis console.resx + mi0bot-Thetis console.resx [v2.10.3.13-beta2]
    //   <data ...><value>Transmit Drive - This is a relative value and is
    //   not meant to imply watts, unless the PA profile is configured
    //   with MAX watts @ 100%</value></data>
    m_rfPowerSlider->setToolTip(QStringLiteral(
        "Transmit Drive - relative value, not watts unless the PA profile "
        "is configured with MAX watts @ 100%"));
    m_tunePwrSlider->setToolTip(QStringLiteral(
        "Tune and/or 2Tone Drive - relative value, not watts unless the "
        "PA profile is configured with MAX watts @ 100%"));

    updatePowerSliderLabels();
}

// From mi0bot-Thetis console.cs:29245-29274 [v2.10.3.13-beta2]
//   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)       // MI0BOT: HL2 has only 15 output power levels
//   {
//       ...
//       lblPWR.Text = "Drive:  " + ((Math.Round(drv / 6.0) / 2) - 7.5).ToString() + "dB";
//   }
//
// HL2 RF Power label formula:
//   dB = (round(drv/6.0)/2) - 7.5     // 0.5 dB steps in [-7.5, 0]
// HL2 Tune slider label formula (slider 0..99 -> dB -16.5..0):
//   dB = (slider/3 - 33) / 2          // inverse of the persistence formula
//
// MI0BOT: HL2 has only 15 output power levels
// [original inline comment from mi0bot console.cs:29245]
void TxApplet::updatePowerSliderLabels()
{
    if (!m_rfPowerSlider || !m_tunePwrSlider
        || !m_rfPowerValue || !m_tunePwrValue) { return; }

    // Use the cached SKU set by rescalePowerSlidersForModel() so the
    // label format always matches the slider's effective range (HL2
    // 0..90 step 6 / 0..99 step 3, all others 0..100 step 1).
    const HPSDRModel model = m_powerSliderModel;
    const int        rfVal  = m_rfPowerSlider->value();
    const int        tunVal = m_tunePwrSlider->value();

    if (model == HPSDRModel::HERMESLITE) {
        const float rfDb  = (std::round(rfVal  / 6.0f) / 2.0f) - 7.5f;
        const float tunDb = (tunVal / 3.0f - 33.0f) / 2.0f;
        m_rfPowerValue->setText(QString::number(rfDb,  'f', 1));
        m_tunePwrValue->setText(QString::number(tunDb, 'f', 1));
    } else {
        m_rfPowerValue->setText(QString::number(rfVal));
        m_tunePwrValue->setText(QString::number(tunVal));
    }
}

// Canonical TX band — derived from the active slice's frequency (which
// is what RadioModel.cpp:903-905 uses to compose the TX wire byte).
// Falls back to m_currentBand when:
//   - m_model is null (early bootstrap — TxApplet exists before
//     RadioModel pointer wired up; see TxApplet ctor parent),
//   - activeSlice() is null (no slice yet — first launch before
//     addSlice fires, or post-disconnect cleanup state).
// In both fallback cases m_currentBand is the best information we have
// and is what the pre-fix code already used.
Band TxApplet::txBand() const
{
    if (!m_model) { return m_currentBand; }
    SliceModel* slice = m_model->activeSlice();
    if (!slice) { return m_currentBand; }
    return bandFromFrequency(slice->frequency());
}

void TxApplet::setCurrentBand(Band band)
{
    // No same-band early-return: the bootstrap call from
    // MainWindow.cpp:1578 (`txApplet->setCurrentBand(pan0->band())`) fires
    // with band == m_currentBand-default (Band20m) when the panadapter
    // also opens on 20m, and we still need that call to push the loaded
    // per-band slider values into the UI on first paint.  Re-running the
    // sync on identical input is idempotent — QSlider::setValue same-value
    // is a no-op, and TransmitModel::setPower / setTunePowerForBand have
    // their own same-value early-returns — so the cost is negligible and
    // the win is correct first-paint behaviour after loadFromSettings.
    m_currentBand = band;

    if (!m_model) { return; }

    auto& tx = m_model->transmitModel();

    // Update the Tune Power slider to reflect the per-band stored value.
    {
        const int tunePwr = tx.tunePowerForBand(band);
        QSignalBlocker b(m_tunePwrSlider);
        m_updatingFromModel = true;
        m_tunePwrSlider->setValue(tunePwr);
        // Issue #175 Task 7: HL2 dB / non-HL2 integer — replaces the
        // bare m_tunePwrValue->setText(QString::number(tunePwr)) so HL2
        // operators see the dB-attenuator label rather than a raw 0..99
        // slider integer.
        updatePowerSliderLabels();
        m_updatingFromModel = false;
    }

    // Update the RF Power slider to reflect the per-band stored value —
    // ONLY when the band passed in is the canonical TX band (i.e. the
    // active slice's band).  Matches Thetis TXBand setter at
    // console.cs:17513 [v2.10.3.13] (`PWR = power_by_band[(int)value];`),
    // where `_tx_band` is single-source-of-truth for TX state.
    //
    // Why the gate: setCurrentBand is wired in MainWindow to BOTH
    // PanadapterModel::bandChanged and SliceModel::frequencyChanged, so it
    // can fire from a CTUN pan that does NOT change the slice.  Recalling
    // the panadapter band's RF power into the live slider would (a) jump
    // the displayed value off the actual TX band, and (b) leak that
    // wrong value back into the active slice's band slot via the
    // setPowerUsingTargetDbm txMode-0 side-effect on the next powerChanged
    // emission — silently corrupting per-band storage.
    //
    // Routed through setPower so the existing reverse-binding lambda
    // (TxApplet.cpp:905) paints the slider; setPower's same-value
    // early-return makes the no-band-change call free.
    if (band == txBand()) {
        tx.setPower(tx.powerForBand(band));
    }
}

// ── Phase 3M-1b K.2: tooltipForMode ──────────────────────────────────────────
//
// Returns a MOX button tooltip string matching the active DSP mode.
// For modes deferred to a later phase, the tooltip explains why MOX won't
// engage, matching the reason string emitted by moxRejected (K.2) and the
// rejection reason from BandPlanGuard::checkMoxAllowed (K.1).
//
// Mode categories:
//   Allowed (LSB/USB/DIGL/DIGU): normal "Manual transmit (MOX)" tooltip.
//   CW (CWL/CWU):                CW TX deferred to Phase 3M-2.
//   Audio (AM/SAM/DSB/FM/DRM):   AM/FM TX deferred to Phase 3M-3 (audio modes).
//   SPEC:                        Never a TX mode.
//
// This helper is static so TxApplet tests can call it directly without
// constructing a full TxApplet instance.
// ---------------------------------------------------------------------------
// static
QString TxApplet::tooltipForMode(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::USB:
    case DSPMode::DIGL:
    case DSPMode::DIGU:
        return QStringLiteral("Manual transmit (MOX)");

    case DSPMode::CWL:
    case DSPMode::CWU:
        return QStringLiteral("CW TX coming in Phase 3M-2");

    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::DSB:
    case DSPMode::FM:
    case DSPMode::DRM:
        return QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)");

    case DSPMode::SPEC:
    default:
        return QStringLiteral("Mode not supported for TX");
    }
}

// ---------------------------------------------------------------------------
// onMoxModeChanged — update MOX button tooltip when DSP mode changes.
//
// Wired to SliceModel::dspModeChanged (via RadioModel active-slice accessor)
// in wireControls(). Calls tooltipForMode(mode) to get the appropriate
// tooltip text and installs it on m_moxBtn. If the mode is an allowed SSB
// mode the tooltip reverts to the normal "Manual transmit (MOX)".
// ---------------------------------------------------------------------------
void TxApplet::onMoxModeChanged(DSPMode mode)
{
    if (m_moxBtn) {
        m_moxBtn->setToolTip(tooltipForMode(mode));
    }
}

// ── pollVoxMeter — Phase 3M-3a-iii bench polish 2026-05-04 ─────────────────
// 100 ms tick that drives m_voxPeakMeter on the TX right pane.
//
//   VOX peak (linear amplitude from TxChannel::getDexpPeakSignal()):
//     • 20 * log10(linear) → dB.
//     • Map -80..0 dB → 0..1 normalized (range matches Thetis ptbVOX scale
//       per console.Designer.cs:6018-6019 [v2.10.3.13]).
//     • Threshold marker pulled from TransmitModel::voxThresholdDb() and
//       mapped through the same -80..0 → 0..1 transform.
//
// Continuous (NOT MOX-gated) since GetDEXPPeakSignal is the live DEXP
// detector envelope, not the TX-pipeline meter.  Same rationale as the
// PhoneCwApplet::pollDexpMeters comment — see that file for the full
// narrative.  Relocated from PhoneCwApplet as part of the 2026-05-04 bench
// polish (VOX row moved to TxApplet under TUNE/MOX).
void TxApplet::pollVoxMeter()
{
    if (!m_model) { return; }
    TxChannel* ch = m_model->txChannel();
    if (!ch || !m_voxPeakMeter) { return; }

    // VOX peak: linear amplitude → dB → 0..1 over -80..0 range.
    const double linearPeak = ch->getDexpPeakSignal();
    const double voxPeakDb  = (linearPeak > 0.0)
        ? 20.0 * std::log10(linearPeak)
        : -80.0;
    const double voxPeak01  = std::clamp((voxPeakDb + 80.0) / 80.0, 0.0, 1.0);
    m_voxPeakMeter->setSignalLevel(voxPeak01);

    // VOX threshold marker: voxThresholdDb is in -80..0 dB range.
    const int thDb = m_model->transmitModel().voxThresholdDb();
    m_voxPeakMeter->setThresholdMarker(
        std::clamp((thDb + 80.0) / 80.0, 0.0, 1.0));
}

// ---------------------------------------------------------------------------
// Phase 3M-1c J.1 — setMicProfileManager
//
// Inject the per-MAC MicProfileManager.  Wires:
//   - manager.profileListChanged → rebuildProfileCombo (set membership change)
//   - manager.activeProfileChanged → combo selection update (programmatic)
// ---------------------------------------------------------------------------
void TxApplet::setMicProfileManager(MicProfileManager* mgr)
{
    if (m_micProfileMgr == mgr) { return; }

    if (m_micProfileMgr) {
        disconnect(m_micProfileMgr, nullptr, this, nullptr);
    }

    m_micProfileMgr = mgr;

    if (m_micProfileMgr) {
        // List changes → rebuild combo entries.
        connect(m_micProfileMgr, &MicProfileManager::profileListChanged,
                this, &TxApplet::rebuildProfileCombo);
        // Active changes → select the named entry without triggering a
        // setActiveProfile callback (m_updatingFromModel guards that).
        connect(m_micProfileMgr, &MicProfileManager::activeProfileChanged,
                this, [this](const QString& name) {
            if (!m_profileCombo) { return; }
            QSignalBlocker blk(m_profileCombo);
            m_updatingFromModel = true;
            const int idx = m_profileCombo->findText(name);
            if (idx >= 0) {
                m_profileCombo->setCurrentIndex(idx);
            }
            m_updatingFromModel = false;
        });
    }

    rebuildProfileCombo();
}

// ---------------------------------------------------------------------------
// Phase 3M-1c J.2 — setTwoToneController
//
// Inject the TwoToneController.  Wires the controller's
// twoToneActiveChanged signal so the button visually mirrors the
// authoritative state (covers the BandPlanGuard rejection clean-up
// from Phase I.5).
// ---------------------------------------------------------------------------
void TxApplet::setTwoToneController(TwoToneController* controller)
{
    if (m_twoToneCtrl == controller) { return; }

    if (m_twoToneCtrl) {
        disconnect(m_twoToneCtrl, nullptr, this, nullptr);
    }

    m_twoToneCtrl = controller;

    if (m_twoToneCtrl) {
        connect(m_twoToneCtrl, &TwoToneController::twoToneActiveChanged,
                this, [this](bool active) {
            if (!m_twoToneBtn) { return; }
            QSignalBlocker blk(m_twoToneBtn);
            m_updatingFromModel = true;
            m_twoToneBtn->setChecked(active);
            m_updatingFromModel = false;
        });

        // Sync initial state.
        QSignalBlocker blk(m_twoToneBtn);
        m_updatingFromModel = true;
        m_twoToneBtn->setChecked(m_twoToneCtrl->isActive());
        m_updatingFromModel = false;
    }
}

// ---------------------------------------------------------------------------
// Phase 3M-3a-ii Batch 6 (Task A) — requestOpenCfcDialog
//
// Open (or raise) the modeless TxCfcDialog.  Lazy-creates the dialog on first
// call so the construction cost is only paid when the user actually opens
// CFC settings.  The dialog is parented to this applet's top-level window so
// it floats freely; modal flag is forced false in TxCfcDialog's ctor.  We
// don't deleteLater() the dialog on close — keep it alive across opens for
// fast re-show, mirroring the TxEqDialog singleton pattern.
//
// Public-slot entry point so external surfaces can reuse the same instance:
//   - [CFC] button right-click → customContextMenuRequested → this slot.
//   - CfcSetupPage's [Configure CFC bands…] button → MainWindow connects its
//     openCfcDialogRequested signal to this slot.
//   - Future Tools menu item → connects to this slot.
// ---------------------------------------------------------------------------
void TxApplet::requestOpenCfcDialog()
{
    if (!m_model) { return; }

    if (!m_cfcDialog) {
        QWidget* host = window();
        m_cfcDialog = new TxCfcDialog(
            &m_model->transmitModel(),
            m_model->txChannel(),
            host ? host : static_cast<QWidget*>(this));
    } else {
        // Connection may have come up since the dialog was created.
        // Refresh the TxChannel pointer so the bar chart timer can poll WDSP.
        m_cfcDialog->setTxChannel(m_model->txChannel());
    }
    m_cfcDialog->show();
    m_cfcDialog->raise();
    m_cfcDialog->activateWindow();
}

// ---------------------------------------------------------------------------
// Phase 3M-1c J.1 — rebuildProfileCombo
//
// Rebuild combo entries from m_micProfileMgr->profileNames().  Preserves
// the active-profile selection where possible; otherwise the manager's
// activeProfileName() is selected.  No-op when manager is null.
// ---------------------------------------------------------------------------
void TxApplet::rebuildProfileCombo()
{
    if (!m_profileCombo) { return; }
    if (!m_micProfileMgr) {
        // No manager → leave the placeholder "Default" item alone.
        return;
    }

    QSignalBlocker blk(m_profileCombo);
    m_updatingFromModel = true;

    const QStringList names = m_micProfileMgr->profileNames();
    const QString active = m_micProfileMgr->activeProfileName();

    m_profileCombo->clear();
    m_profileCombo->addItems(names);

    const int idx = m_profileCombo->findText(active);
    if (idx >= 0) {
        m_profileCombo->setCurrentIndex(idx);
    }

    m_updatingFromModel = false;
}

} // namespace NereusSDR
