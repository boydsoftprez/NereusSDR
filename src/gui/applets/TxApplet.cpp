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
// Phase 3M-1b J.2: VOX toggle + VoxSettingsPopup wired.
// Phase 3M-1b J.3: MON toggle + monitor volume slider + mic-source badge wired.
// Out-of-phase controls (2-Tone, PS-A) hidden.
//
// Control inventory:
//  0.  Mic-source badge  — read-only "PC mic"/"Radio mic"  [WIRED — 3M-1b J.3]
//  1.  Fwd Power gauge   — HGauge 0–120 W, redStart 100 W
//  2.  SWR gauge         — HGauge 1.0–3.0, redStart 2.5
//  3.  RF Power slider   + label + value  [WIRED — 3M-1a H.3]
//  4.  Tune Power slider + label + value  [WIRED — 3M-1a H.3]
//      (Mic Gain slider relocated to PhoneCwApplet — 2026-04-28 relocation)
//  4b. VOX toggle button — checkable, green:checked style  [WIRED — 3M-1b J.2]
//      Right-click opens VoxSettingsPopup (threshold/gain/hang-time).
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
#include "NyiOverlay.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "gui/ComboStyle.h"
#include "gui/widgets/VoxSettingsPopup.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/MicProfileManager.h"
#include "core/MoxController.h"
#include "core/RadioStatus.h"
#include "core/TwoToneController.h"
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
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

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

    // ── 1. Forward Power gauge ── 0–120 W, redStart 100 W ───────────────────
    // Ticks: 0 / 40 / 80 / 100 / 120  (AetherSDR TxApplet.cpp:71)
    auto* fwdGauge = new HGauge(this);
    fwdGauge->setRange(0.0, 120.0);
    fwdGauge->setRedStart(100.0);
    fwdGauge->setYellowStart(100.0); // same as red — no distinct yellow zone
    fwdGauge->setTitle(QStringLiteral("RF Pwr"));
    fwdGauge->setTickLabels({QStringLiteral("0"), QStringLiteral("40"),
                              QStringLiteral("80"), QStringLiteral("100"),
                              QStringLiteral("120")});
    fwdGauge->setAccessibleName(QStringLiteral("Forward power gauge"));
    m_fwdPowerGauge = fwdGauge;
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
    NyiOverlay::markNyi(swrGauge, QStringLiteral("Phase 3I-1"));
    vbox->addWidget(swrGauge);

    // ── 3. RF Power slider row ───────────────────────────────────────────────
    // Label fixedWidth 62, value fixedWidth 22  (AetherSDR TxApplet.cpp:87–104)
    {
        auto* rfSlider = new QSlider(Qt::Horizontal, this);
        rfSlider->setRange(0, 100);
        rfSlider->setValue(100);
        rfSlider->setAccessibleName(QStringLiteral("RF power"));

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
        rfSlider->setToolTip(QStringLiteral("RF output power (0–100 W)"));
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
        tunSlider->setToolTip(QStringLiteral("Tune carrier power for current band (0–100 W)"));
        row->addWidget(tunSlider, 1);
        row->addWidget(tunValue);

        vbox->addLayout(row);
    }

    // ── 4b. VOX toggle button ─────────────────────────────────────────────────
    // Phase 3M-1b J.2: below Tune Power slider.
    // Checkable: green border when active (greenCheckedStyle()).
    // voxEnabled does NOT persist — safety: VOX always loads OFF (plan §0 row 8).
    // Right-click → showVoxSettingsPopup(pos) for threshold/gain/hang-time.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_voxBtn = new QPushButton(QStringLiteral("VOX"), this);
        m_voxBtn->setCheckable(true);
        m_voxBtn->setChecked(false);  // default: OFF — plan §0 row 8 safety rule
        m_voxBtn->setFixedHeight(22);
        m_voxBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_voxBtn->setStyleSheet(Style::buttonBaseStyle() + Style::greenCheckedStyle());
        m_voxBtn->setAccessibleName(QStringLiteral("VOX enable"));
        m_voxBtn->setToolTip(QStringLiteral(
            "Voice-operated TX (VOX). Left-click to toggle.\n"
            "Right-click for threshold/gain/hang-time settings.\n"
            "Does NOT persist across restarts (safety)."));
        // Custom context menu policy so right-click emits customContextMenuRequested.
        m_voxBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_voxBtn, 1);

        // Right-hand spacer so the button occupies ≈half the applet width
        // (matching Thetis UI density where VOX is a single small button, not
        // the full row). A stretch absorbs the remaining space.
        row->addStretch();

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
        // Blue checked style: blue border + slightly tinted bg when active.
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

    // ── Button row: TUNE + MOX + ATU + MEM (25% each) ─────────────────────
    // Matches AetherSDR TxApplet.cpp:155–203 (single 4-button row)
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

        m_atuBtn = new QPushButton(QStringLiteral("ATU"), this);
        m_atuBtn->setCheckable(true);
        m_atuBtn->setFixedHeight(22);
        m_atuBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_atuBtn->setStyleSheet(btnStyle);
        m_atuBtn->setAccessibleName(QStringLiteral("Antenna tuner"));
        NyiOverlay::markNyi(m_atuBtn, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_atuBtn, 1);

        m_memBtn = new QPushButton(QStringLiteral("MEM"), this);
        m_memBtn->setCheckable(true);
        m_memBtn->setFixedHeight(22);
        m_memBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_memBtn->setStyleSheet(btnStyle);
        m_memBtn->setAccessibleName(QStringLiteral("ATU memory"));
        NyiOverlay::markNyi(m_memBtn, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_memBtn, 1);

        vbox->addLayout(row);
    }

    // ── Profile combo row (50%) + tune mode combo (50%) ─────────────────────
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

        m_tuneModeCombo = new QComboBox(this);
        m_tuneModeCombo->addItem(QStringLiteral("Auto"));
        m_tuneModeCombo->addItem(QStringLiteral("Manual"));
        m_tuneModeCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_tuneModeCombo->setFixedHeight(22);
        applyComboStyle(m_tuneModeCombo);
        m_tuneModeCombo->setAccessibleName(QStringLiteral("Tune mode"));
        NyiOverlay::markNyi(m_tuneModeCombo, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_tuneModeCombo, 1);

        vbox->addLayout(row);
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

        // PS-A: green when checked — #006030 bg matches AetherSDR APD button
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

        m_dupBtn = new QPushButton(QStringLiteral("DUP"), this);
        m_dupBtn->setCheckable(true);
        m_dupBtn->setFixedHeight(22);
        m_dupBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_dupBtn->setAccessibleName(QStringLiteral("Full duplex"));
        NyiOverlay::markNyi(m_dupBtn, QStringLiteral("Phase 3M-3"));
        row->addWidget(m_dupBtn, 1);

        vbox->addLayout(row);
    }

    // ── APD/xPA row: xPA button + inset container ────────────────────────────
    // Inset: fixedHeight 22, bg #0a0a18, border #1e2e3e  (AetherSDR TxApplet.cpp:224–253)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_xpaBtn = new QPushButton(QStringLiteral("xPA"), this);
        m_xpaBtn->setCheckable(true);
        m_xpaBtn->setFixedHeight(22);
        m_xpaBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_xpaBtn->setStyleSheet(Style::buttonBaseStyle() + Style::greenCheckedStyle());
        m_xpaBtn->setAccessibleName(QStringLiteral("External PA indicator"));
        NyiOverlay::markNyi(m_xpaBtn, QStringLiteral("Phase 3I-1"));
        row->addWidget(m_xpaBtn, 2); // ~40%

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
        NyiOverlay::markNyi(m_swrProtLed, QStringLiteral("Phase 3I-1"));
        insetLayout->addWidget(m_swrProtLed);

        row->addWidget(inset, 3); // ~60%
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
            this, [this](double fwdW, double /*revW*/, double /*swr*/) {
        constexpr double kAlpha = 0.30;
        m_fwdPowerSmoothedW = kAlpha * fwdW + (1.0 - kAlpha) * m_fwdPowerSmoothedW;
    });
    auto* fwdGaugeRefreshTimer = new QTimer(this);
    fwdGaugeRefreshTimer->setInterval(50);   // 20 Hz UI refresh
    connect(fwdGaugeRefreshTimer, &QTimer::timeout, this, [this]() {
        if (m_fwdPowerGauge) {
            m_fwdPowerGauge->setValue(m_fwdPowerSmoothedW);
        }
    });
    fwdGaugeRefreshTimer->start();

    // ── RF Power slider → TransmitModel::setPower(int) ──────────────────────
    // From Thetis chkMOX_CheckedChanged2 power flow [v2.10.3.13]:
    //   the RF Power slider maps 0–100 to TX drive level.
    connect(m_rfPowerSlider, &QSlider::valueChanged, this, [this, &tx](int val) {
        if (m_updatingFromModel) { return; }
        m_rfPowerValue->setText(QString::number(val));
        tx.setPower(val);
    });

    // Reverse: TransmitModel::powerChanged → slider
    connect(&tx, &TransmitModel::powerChanged, this, [this](int power) {
        QSignalBlocker b(m_rfPowerSlider);
        m_updatingFromModel = true;
        m_rfPowerSlider->setValue(power);
        m_rfPowerValue->setText(QString::number(power));
        m_updatingFromModel = false;
    });

    // ── Tune Power slider → TransmitModel::setTunePowerForBand ──────────────
    // Per-band tune power, ported from Thetis console.cs:12094 [v2.10.3.13]:
    //   private int[] tunePower_by_band;
    // The current band is tracked by m_currentBand (updated by setCurrentBand).
    connect(m_tunePwrSlider, &QSlider::valueChanged, this, [this, &tx](int val) {
        if (m_updatingFromModel) { return; }
        m_tunePwrValue->setText(QString::number(val));
        tx.setTunePowerForBand(m_currentBand, val);
    });

    // Reverse: TransmitModel::tunePowerByBandChanged → slider (only for current band)
    connect(&tx, &TransmitModel::tunePowerByBandChanged,
            this, [this](Band band, int watts) {
        if (band != m_currentBand) { return; }
        QSignalBlocker b(m_tunePwrSlider);
        m_updatingFromModel = true;
        m_tunePwrSlider->setValue(watts);
        m_tunePwrValue->setText(QString::number(watts));
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

    // ── VOX toggle button ↔ TransmitModel::voxEnabled ────────────────────────
    // Phase 3M-1b J.2.
    // UI → Model: toggled → setVoxEnabled (with m_updatingFromModel guard).
    // Model → UI: voxEnabledChanged → update checked state with QSignalBlocker.
    // Right-click → showVoxSettingsPopup.
    connect(m_voxBtn, &QPushButton::toggled, this, [this, &tx](bool on) {
        if (m_updatingFromModel) { return; }
        tx.setVoxEnabled(on);
    });

    connect(&tx, &TransmitModel::voxEnabledChanged, this, [this](bool on) {
        QSignalBlocker b(m_voxBtn);
        m_updatingFromModel = true;
        m_voxBtn->setChecked(on);
        m_updatingFromModel = false;
    });

    connect(m_voxBtn, &QPushButton::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        showVoxSettingsPopup(pos);
    });

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
        m_rfPowerValue->setText(QString::number(tx.power()));
    }

    // Tune Power for current band
    {
        QSignalBlocker b(m_tunePwrSlider);
        const int tunePwr = tx.tunePowerForBand(m_currentBand);
        m_tunePwrSlider->setValue(tunePwr);
        m_tunePwrValue->setText(QString::number(tunePwr));
    }

    // VOX button state (J.2 Phase 3M-1b)
    // voxEnabled intentionally loads as OFF — plan §0 row 8 safety rule.
    // We still read the model so that if setVoxEnabled was called programmatically
    // before the applet was shown, the UI reflects the actual model state.
    if (m_voxBtn) {
        QSignalBlocker bv(m_voxBtn);
        m_voxBtn->setChecked(tx.voxEnabled());
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

// ── Phase 3M-1b J.2: showVoxSettingsPopup ────────────────────────────────────
//
// Opens a VoxSettingsPopup anchored near the VOX button. The popup is
// auto-delete (Qt::Popup + WA_DeleteOnClose) so no ownership management is
// needed here. Each slider in the popup drives the corresponding TransmitModel
// setter via a direct signal connection.
void TxApplet::showVoxSettingsPopup(const QPoint& pos)
{
    if (!m_model) { return; }

    TransmitModel& tx = m_model->transmitModel();

    // Construct the popup with the current model values so sliders start in sync.
    auto* popup = new VoxSettingsPopup(
        tx.voxThresholdDb(),
        tx.voxGainScalar(),
        tx.voxHangTimeMs(),
        nullptr);  // Qt::Popup window manages its own lifetime

    // Wire popup signals → model setters.
    connect(popup, &VoxSettingsPopup::thresholdDbChanged,
            &tx,   &TransmitModel::setVoxThresholdDb);
    connect(popup, &VoxSettingsPopup::gainScalarChanged,
            &tx,   &TransmitModel::setVoxGainScalar);
    connect(popup, &VoxSettingsPopup::hangTimeMsChanged,
            &tx,   &TransmitModel::setVoxHangTimeMs);

    // Map the button-local position to global so showAt() can position correctly.
    const QPoint globalPos = m_voxBtn
        ? m_voxBtn->mapToGlobal(pos)
        : mapToGlobal(pos);
    popup->showAt(globalPos);
}

void TxApplet::setCurrentBand(Band band)
{
    if (m_currentBand == band) { return; }
    m_currentBand = band;

    if (!m_model) { return; }

    // Update the Tune Power slider to reflect the stored value for the new band.
    const int tunePwr = m_model->transmitModel().tunePowerForBand(band);
    QSignalBlocker b(m_tunePwrSlider);
    m_updatingFromModel = true;
    m_tunePwrSlider->setValue(tunePwr);
    m_tunePwrValue->setText(QString::number(tunePwr));
    m_updatingFromModel = false;
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
