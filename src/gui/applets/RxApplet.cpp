// =================================================================
// src/gui/applets/RxApplet.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.resx (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
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

//
// Upstream source 'Project Files/Source/Console/console.resx' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

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

#include "RxApplet.h"
#include "NyiOverlay.h"
#include "core/BoardCapabilities.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/FilterPassbandWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <algorithm>

#include <QAction>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace NereusSDR {

// ─── RxApplet ─────────────────────────────────────────────────────────────────

RxApplet::RxApplet(SliceModel* slice, RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
    , m_slice(slice)
{
    buildUi();
    syncFromModel();
}

void RxApplet::buildUi()
{
    // Outer layout: zero margins, zero spacing (title bar flush).
    // Inner body: 4px sides, 2px top/bottom, 2px row spacing.
    // From AetherSDR RxApplet::buildUI() outer/inner layout pattern.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* body = new QWidget(this);
    body->setStyleSheet(QStringLiteral("background: %1;").arg(Style::kPanelBg));
    auto* root = new QVBoxLayout(body);
    root->setContentsMargins(4, 2, 4, 2);
    root->setSpacing(2);
    outer->addWidget(body);

    // ── Row 1: badge | lock | RX ant | TX ant | stretch | filter label ────
    // From AetherSDR RxApplet.cpp lines 243-336
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(3);

        // Control 1: Slice letter badge (A/B/C/D)
        // 20×20, bg #0070c0, white text, 3px radius
        m_sliceBadge = new QLabel(QStringLiteral("A"), this);
        m_sliceBadge->setFixedSize(20, 20);
        m_sliceBadge->setAlignment(Qt::AlignCenter);
        m_sliceBadge->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: %2;"
            " border-radius: 3px; font-weight: bold; font-size: 11px; }"
        ).arg(Style::kBlueBg, Style::kBlueText));
        row->addWidget(m_sliceBadge);

        // Control 2: Lock button (checkable, 20×20, emoji 🔓/🔒)
        // Live in S2.9 — wired to SliceModel::setLocked (client-side guard).
        // Checked color: #4488ff.
        m_lockBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x94\x93"), this); // 🔓
        m_lockBtn->setCheckable(true);
        m_lockBtn->setFixedSize(20, 20);
        m_lockBtn->setFlat(true);
        m_lockBtn->setStyleSheet(QStringLiteral(
            "QPushButton { font-size: 13px; padding: 0; background: transparent; border: none; }"
            "QPushButton:checked { color: #4488ff; }"
        ));
        connect(m_lockBtn, &QPushButton::toggled, this, [this](bool locked) {
            m_lockBtn->setText(locked
                ? QString::fromUtf8("\xF0\x9F\x94\x92")   // 🔒
                : QString::fromUtf8("\xF0\x9F\x94\x93")); // 🔓
            if (!m_updatingFromModel && m_slice) {
                m_slice->setLocked(locked);
            }
        });
        row->addWidget(m_lockBtn);
        // Lock is live in S2.9 — no NYI badge.

        // Control 3: RX antenna button (flat, color #4488ff, transparent bg)
        // From AetherSDR RxApplet.cpp lines 270-289
        m_rxAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
        m_rxAntBtn->setFlat(true);
        m_rxAntBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  color: #4488ff; background: transparent; border: none;"
            "  font-size: 10px; font-weight: bold; padding: 0 2px;"
            "}"
            "QPushButton:hover { color: #66aaff; }"
        ));
        connect(m_rxAntBtn, &QPushButton::clicked, this, [this] {
            QMenu menu(this);
            const QString cur = m_slice ? m_slice->rxAntenna() : QString{};
            for (const QString& ant : m_antList) {
                QAction* act = menu.addAction(ant);
                act->setCheckable(true);
                act->setChecked(ant == cur);
            }
            const QAction* sel = menu.exec(
                m_rxAntBtn->mapToGlobal(QPoint(0, m_rxAntBtn->height())));
            if (sel && m_slice) {
                m_slice->setRxAntenna(sel->text());
            }
        });
        row->addWidget(m_rxAntBtn);

        // Control 4: TX antenna button (flat, color #ff4444, transparent bg)
        // From AetherSDR RxApplet.cpp lines 292-311
        m_txAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
        m_txAntBtn->setFlat(true);
        m_txAntBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  color: #ff4444; background: transparent; border: none;"
            "  font-size: 10px; font-weight: bold; padding: 0 2px;"
            "}"
            "QPushButton:hover { color: #ff6666; }"
        ));
        connect(m_txAntBtn, &QPushButton::clicked, this, [this] {
            QMenu menu(this);
            const QString cur = m_slice ? m_slice->txAntenna() : QString{};
            for (const QString& ant : m_antList) {
                QAction* act = menu.addAction(ant);
                act->setCheckable(true);
                act->setChecked(ant == cur);
            }
            const QAction* sel = menu.exec(
                m_txAntBtn->mapToGlobal(QPoint(0, m_txAntBtn->height())));
            if (sel && m_slice) {
                m_slice->setTxAntenna(sel->text());
            }
        });
        row->addWidget(m_txAntBtn);

        row->addStretch(1);

        // Control 5: Filter width label (color #00c8ff, 11px bold)
        m_filterWidthLbl = new QLabel(QStringLiteral("2.9K"), this);
        m_filterWidthLbl->setAlignment(Qt::AlignCenter);
        m_filterWidthLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #00c8ff; font-size: 11px; font-weight: bold; }"
        ));
        row->addWidget(m_filterWidthLbl);

        row->addStretch(1);

        root->addLayout(row);
    }

    // ── Control 6: Mode combo ─────────────────────────────────────────────
    // fixedHeight 20, applyComboStyle()
    // From AetherSDR RxApplet.cpp lines 359-381
    {
        m_modeCombo = new QComboBox(this);
        m_modeCombo->setFixedHeight(20);
        m_modeCombo->addItems({
            QStringLiteral("USB"), QStringLiteral("LSB"),
            QStringLiteral("CWU"), QStringLiteral("CWL"),
            QStringLiteral("AM"),  QStringLiteral("SAM"),
            QStringLiteral("FM"),  QStringLiteral("DSB"),
            QStringLiteral("DIGU"),QStringLiteral("DIGL"),
            QStringLiteral("DRM")
        });
        applyComboStyle(m_modeCombo);

        // Tier 1 wiring: mode combo → SliceModel::setDspMode()
        connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
            if (m_updatingFromModel || !m_slice) { return; }
            const QString name = m_modeCombo->currentText();
            m_slice->setDspMode(SliceModel::modeFromName(name));
        });
        root->addWidget(m_modeCombo);
    }

    // ── Two-column area ───────────────────────────────────────────────────
    // From AetherSDR RxApplet.cpp lines 443-878
    // left:right stretch = 2:3 (same as AetherSDR)
    auto* columns = new QHBoxLayout;
    columns->setSpacing(4);

    // ── Left column ───────────────────────────────────────────────────────
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(2);

    // Control 17: Step size row — STEP: [<] [value] [>]
    // Cycles SliceModel::stepHz through kStageOneStepLadder
    // {1, 10, 100, 500, 1k, 10k}.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(0);

        auto* lbl = new QLabel(QStringLiteral("STEP:"), this);
        lbl->setFixedWidth(34);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }"
        ).arg(Style::kTextSecondary));
        row->addWidget(lbl);

        m_stepDown = new TriBtn(TriBtn::Left, this);
        row->addWidget(m_stepDown);

        m_stepLabel = new QLabel(QStringLiteral("100 Hz"), this);
        m_stepLabel->setAlignment(Qt::AlignCenter);
        m_stepLabel->setStyleSheet(Style::insetValueStyle());
        row->addWidget(m_stepLabel, 1);

        m_stepUp = new TriBtn(TriBtn::Right, this);
        row->addWidget(m_stepUp);

        leftCol->addLayout(row);

        // Step arrows cycle through kStageOneStepLadder
        // {1, 10, 100, 500, 1k, 10k}. Down = previous, Up = next.
        // Wraps at both ends. Issue #69.
        connect(m_stepDown, &QPushButton::clicked, this, [this]() {
            if (!m_slice) { return; }
            const int current = m_slice->stepHz();
            int idx = 0;
            for (int i = 0; i < kStageOneStepLadderSize; ++i) {
                if (kStageOneStepLadder[i] == current) { idx = i; break; }
            }
            const int prev = (idx - 1 + kStageOneStepLadderSize)
                             % kStageOneStepLadderSize;
            m_slice->setStepHz(kStageOneStepLadder[prev]);
        });
        connect(m_stepUp, &QPushButton::clicked, this, [this]() {
            if (!m_slice) { return; }
            const int current = m_slice->stepHz();
            int idx = 0;
            for (int i = 0; i < kStageOneStepLadderSize; ++i) {
                if (kStageOneStepLadder[i] == current) { idx = i; break; }
            }
            const int next = (idx + 1) % kStageOneStepLadderSize;
            m_slice->setStepHz(kStageOneStepLadder[next]);
        });
    }

    // Control 7: Filter preset buttons — 10 buttons in 2×5 grid
    // Rows 0-1, cols 0-4. Spacing 2px. Blue active state.
    // Tier 1 wired → SliceModel::setFilter()
    // From AetherSDR RxApplet.cpp rebuildFilterButtons()
    {
        m_filterContainer = new QWidget(this);
        m_filterGrid = new QGridLayout(m_filterContainer);
        m_filterGrid->setContentsMargins(0, 0, 0, 0);
        m_filterGrid->setSpacing(2);
        rebuildFilterButtons();
        leftCol->addWidget(m_filterContainer);
    }

    // Control 8: FilterPassband — visual filter with drag-to-adjust
    // From AetherSDR RxApplet.cpp lines 504-513
    {
        m_filterPassband = new FilterPassbandWidget(this);
        m_filterPassband->setMinimumHeight(40);
        m_filterPassband->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(m_filterPassband, &FilterPassbandWidget::filterChanged,
                this, [this](int lo, int hi) {
            if (m_slice) { m_slice->setFilter(lo, hi); }
        });
        leftCol->addWidget(m_filterPassband);
    }

    columns->addLayout(leftCol, 2);

    // ── Right column ──────────────────────────────────────────────────────
    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(2);

    // Controls 11 + 12: Mute + AF gain slider
    // From AetherSDR RxApplet.cpp lines 654-683
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 12: Mute button (18×18, emoji 🔊/🔇)
        // NYI — SliceModel has no setMuted() yet
        m_muteBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x94\x8A"), this); // 🔊
        m_muteBtn->setCheckable(true);
        m_muteBtn->setFixedSize(18, 18);
        m_muteBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: transparent; border: none; font-size: 12px; padding: 0px;"
            "}"
            "QPushButton:hover { background: %1; border-radius: 3px; }"
        ).arg(Style::kButtonAltHover));
        connect(m_muteBtn, &QPushButton::toggled, this, [this](bool muted) {
            m_muteBtn->setText(muted
                ? QString::fromUtf8("\xF0\x9F\x94\x87")    // 🔇
                : QString::fromUtf8("\xF0\x9F\x94\x8A"));  // 🔊
            // TODO Phase 3I: m_slice->setMuted(muted);
        });
        row->addWidget(m_muteBtn);
        NyiOverlay::markNyi(m_muteBtn, QStringLiteral("Phase 3I"));

        // Control 11: AF gain slider (Tier 1 wired → SliceModel::setAfGain())
        m_afSlider = new QSlider(Qt::Horizontal, this);
        m_afSlider->setRange(0, 100);
        m_afSlider->setValue(50);
        m_afSlider->setFixedHeight(18);
        m_afSlider->setStyleSheet(Style::sliderHStyle());
        connect(m_afSlider, &QSlider::valueChanged, this, [this](int v) {
            if (m_updatingFromModel || !m_slice) { return; }
            m_slice->setAfGain(v);
        });
        row->addWidget(m_afSlider, 1);

        rightCol->addLayout(row);
    }

    // Control 13: Audio pan slider (NYI)
    // L ←→ R, center = 50
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lLbl = new QLabel(QStringLiteral("L"), this);
        lLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }"
        ).arg(Style::kTextSecondary));
        row->addWidget(lLbl);

        m_panSlider = new QSlider(Qt::Horizontal, this);
        m_panSlider->setRange(0, 100);
        m_panSlider->setValue(50);
        m_panSlider->setFixedHeight(18);
        m_panSlider->setStyleSheet(Style::sliderHStyle());
        row->addWidget(m_panSlider, 1);

        auto* rLbl = new QLabel(QStringLiteral("R"), this);
        rLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }"
        ).arg(Style::kTextSecondary));
        row->addWidget(rLbl);

        rightCol->addLayout(row);
        NyiOverlay::markNyi(m_panSlider, QStringLiteral("Phase 3I"));
    }

    // Control 14: Squelch toggle + slider (NYI)
    // greenToggle(fixedWidth 52) + QSlider
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_sqlBtn = greenToggle(QStringLiteral("SQL"), 52, 20);
        row->addWidget(m_sqlBtn);

        m_sqlSlider = new QSlider(Qt::Horizontal, this);
        m_sqlSlider->setRange(0, 100);
        m_sqlSlider->setValue(20);
        m_sqlSlider->setFixedHeight(18);
        m_sqlSlider->setStyleSheet(Style::sliderHStyle());
        row->addWidget(m_sqlSlider, 1);

        rightCol->addLayout(row);
        NyiOverlay::markNyi(m_sqlBtn,    QStringLiteral("Phase 3I"));
        NyiOverlay::markNyi(m_sqlSlider, QStringLiteral("Phase 3I"));
    }

    // ATT/S-ATT row — between Squelch and AGC
    // From Thetis console.cs: comboPreamp / udRX1StepAttData (stacked)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        row->setContentsMargins(0, 0, 0, 0);

        m_attLabel = new QLabel(QStringLiteral("ATT"), this);
        m_attLabel->setFixedWidth(34);
        m_attLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #8aa8c0; font-size: 11px; }"));
        row->addWidget(m_attLabel);

        m_attStack = new QStackedWidget(this);
        m_attStack->setFixedHeight(20);

        // Page 0: Preamp combo (ATT mode — step att disabled)
        m_preampCombo = new QComboBox(this);
        m_preampCombo->addItems({QStringLiteral("0dB"), QStringLiteral("-10dB"),
                                 QStringLiteral("-20dB"), QStringLiteral("-30dB")});
        m_preampCombo->setFixedWidth(70);
        m_preampCombo->setFixedHeight(20);
        applyComboStyle(m_preampCombo);
        m_attStack->addWidget(m_preampCombo);

        // Page 1: Step att spinbox (S-ATT mode — step att enabled)
        m_stepAttSpin = new QSpinBox(this);
        m_stepAttSpin->setRange(0, 31);
        m_stepAttSpin->setSuffix(QStringLiteral(" dB"));
        m_stepAttSpin->setFixedWidth(70);
        m_stepAttSpin->setFixedHeight(20);
        m_attStack->addWidget(m_stepAttSpin);

        m_attStack->setCurrentIndex(0);  // default to preamp combo
        row->addWidget(m_attStack, 1);

        rightCol->addLayout(row);
    }

    // Controls 9 + 10: AGC combo + AGC threshold slider
    // From AetherSDR RxApplet.cpp lines 738-768
    {
        auto* agcRow = new QHBoxLayout;
        agcRow->setSpacing(4);

        // Control 9: AGC combo (fixedWidth 52), items: Off/Slow/Med/Fast
        // Tier 1 wired → SliceModel::setAgcMode()
        m_agcCombo = new QComboBox(this);
        m_agcCombo->addItem(QStringLiteral("Off"),  static_cast<int>(AGCMode::Off));
        m_agcCombo->addItem(QStringLiteral("Slow"), static_cast<int>(AGCMode::Slow));
        m_agcCombo->addItem(QStringLiteral("Med"),  static_cast<int>(AGCMode::Med));
        m_agcCombo->addItem(QStringLiteral("Fast"), static_cast<int>(AGCMode::Fast));
        m_agcCombo->setFixedWidth(52);
        m_agcCombo->setFixedHeight(20);
        applyComboStyle(m_agcCombo);

        // Tier 1 wiring
        connect(m_agcCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (m_updatingFromModel || !m_slice) { return; }
            const auto mode = static_cast<AGCMode>(
                m_agcCombo->itemData(idx).toInt());
            m_slice->setAgcMode(mode);
        });
        agcRow->addWidget(m_agcCombo);

        // Control 10: AGC threshold slider — wrapped in container with
        // AUTO badge, dB readout, info sub-line (matches VfoWidget Task 6)
        m_agcTContainer = new QWidget(this);
        auto* containerLayout = new QVBoxLayout(m_agcTContainer);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(1);

        // First row: AGC-T label + slider + dB value + AUTO badge
        auto* sliderRow = new QHBoxLayout;
        m_agcTLabelWidget = new QLabel(QStringLiteral("AGC-T"), m_agcTContainer);
        m_agcTLabelWidget->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        m_agcTLabelWidget->setFixedWidth(40);
        sliderRow->addWidget(m_agcTLabelWidget);

        // From Thetis Project Files/Source/Console/console.cs:45977 — agc_thresh_point
        m_agcTSlider = new QSlider(Qt::Horizontal, m_agcTContainer);
        m_agcTSlider->setRange(-160, 0);
        m_agcTSlider->setValue(-20);
        m_agcTSlider->setFixedHeight(18);
        m_agcTSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        // From Thetis console.resx:8397 — ptbRF.ToolTip (ptbRF is the AGC-T slider)
        m_agcTSlider->setToolTip(QStringLiteral("AGC Max Gain - Operates similarly to traditional RF Gain. Right click AUTO based on noise floor."));
        sliderRow->addWidget(m_agcTSlider);

        m_agcTLabel = new QLabel(QStringLiteral("-20"), m_agcTContainer);
        m_agcTLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
        m_agcTLabel->setFixedWidth(32);
        m_agcTLabel->setAlignment(Qt::AlignRight);
        sliderRow->addWidget(m_agcTLabel);

        m_agcAutoLabel = new QPushButton(QStringLiteral("AUTO"), m_agcTContainer);
        m_agcAutoLabel->setStyleSheet(
            QStringLiteral("QPushButton { background: #1a1a1a; border: 1px solid #445;"
                            "color: #556; font-size: 7px; padding: 0 3px; border-radius: 2px; }"
                            "QPushButton:hover { border-color: #adff2f; }"));
        m_agcAutoLabel->setFixedHeight(14);
        m_agcAutoLabel->setFixedWidth(30);
        m_agcAutoLabel->setCursor(Qt::PointingHandCursor);
        // From Thetis v2.10.3.13 setup.designer.cs:38679 — chkAutoAGCRX1.ToolTip
        m_agcAutoLabel->setToolTip(QStringLiteral("Automatically adjust AGC based on Noise Floor"));
        connect(m_agcAutoLabel, &QPushButton::clicked, this, [this]() {
            emit autoAgcToggled(!m_autoAgcActive);
        });
        sliderRow->addWidget(m_agcAutoLabel);

        containerLayout->addLayout(sliderRow);

        // Second row: info sub-line (hidden by default)
        m_agcInfoLabel = new QLabel(m_agcTContainer);
        m_agcInfoLabel->setStyleSheet(QStringLiteral("color: #33aa33; font-size: 7px; padding: 0 2px;"));
        m_agcInfoLabel->hide();
        containerLayout->addWidget(m_agcInfoLabel);

        connect(m_agcTSlider, &QSlider::valueChanged, this, [this](int v) {
            m_agcTLabel->setText(QString::number(v));
            if (m_updatingFromModel || !m_slice) { return; }
            m_slice->setAgcThreshold(v);
        });

        // Right-click on AGC-T slider → directly open Setup dialog
        m_agcTSlider->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_agcTSlider, &QWidget::customContextMenuRequested,
                this, [this](const QPoint& /*pos*/) {
            emit openSetupRequested();
        });

        agcRow->addWidget(m_agcTContainer, 1);

        rightCol->addLayout(agcRow);
    }

    rightCol->addStretch(1);

    // Control 15: RIT toggle + offset + zero
    // Live-wired in S2.8 — SliceModel::setRitEnabled/setRitHz feed the
    // existing RxChannel::setShiftFrequency path via RadioModel.
    // From AetherSDR RxApplet.cpp lines 773-823
    {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);

        m_ritOnBtn = amberToggle(QStringLiteral("RIT"), -1, 20);
        m_ritOnBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(m_ritOnBtn);

        row->addSpacing(2);

        m_ritZero = styledButton(QStringLiteral("0"), -1, 20);
        m_ritZero->setCheckable(false);
        row->addWidget(m_ritZero);

        row->addSpacing(2);

        m_ritMinus = new TriBtn(TriBtn::Left, this);
        row->addWidget(m_ritMinus);

        m_ritLabel = new QLabel(QStringLiteral("+0 Hz"), this);
        m_ritLabel->setAlignment(Qt::AlignCenter);
        m_ritLabel->setStyleSheet(Style::insetValueStyle());
        row->addWidget(m_ritLabel, 1);

        m_ritPlus = new TriBtn(TriBtn::Right, this);
        row->addWidget(m_ritPlus);

        // Wire RIT controls to SliceModel (live in S2.8).
        // Toggle → enable/disable RIT on the slice.
        connect(m_ritOnBtn, &QPushButton::toggled, this, [this](bool on) {
            if (m_updatingFromModel || !m_slice) { return; }
            m_slice->setRitEnabled(on);
        });
        // Minus/Plus → decrement/increment by current step, clamped to ±10000 Hz.
        connect(m_ritMinus, &QPushButton::clicked, this, [this]() {
            if (!m_slice) { return; }
            int step = m_slice->stepHz();
            m_slice->setRitHz(std::clamp(m_slice->ritHz() - step, -10000, 10000));
        });
        connect(m_ritPlus, &QPushButton::clicked, this, [this]() {
            if (!m_slice) { return; }
            int step = m_slice->stepHz();
            m_slice->setRitHz(std::clamp(m_slice->ritHz() + step, -10000, 10000));
        });
        // Zero → reset RIT offset to 0.
        connect(m_ritZero, &QPushButton::clicked, this, [this]() {
            if (!m_slice) { return; }
            m_slice->setRitHz(0);
        });

        rightCol->addLayout(row);

        // RIT controls are live — no NYI badge.
    }

    // Control 16: XIT toggle + offset + zero
    // Same structure as RIT. XIT stored for 3M-1 (TX phase); keep NYI badge.
    // From AetherSDR RxApplet.cpp lines 825-876
    {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);

        m_xitOnBtn = amberToggle(QStringLiteral("XIT"), -1, 20);
        m_xitOnBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(m_xitOnBtn);

        row->addSpacing(2);

        m_xitZero = styledButton(QStringLiteral("0"), -1, 20);
        m_xitZero->setCheckable(false);
        row->addWidget(m_xitZero);

        row->addSpacing(2);

        m_xitMinus = new TriBtn(TriBtn::Left, this);
        row->addWidget(m_xitMinus);

        m_xitLabel = new QLabel(QStringLiteral("+0 Hz"), this);
        m_xitLabel->setAlignment(Qt::AlignCenter);
        m_xitLabel->setStyleSheet(Style::insetValueStyle());
        row->addWidget(m_xitLabel, 1);

        m_xitPlus = new TriBtn(TriBtn::Right, this);
        row->addWidget(m_xitPlus);

        rightCol->addLayout(row);

        // XIT stored for 3M-1; keep NYI badges.
        NyiOverlay::markNyi(m_xitOnBtn, QStringLiteral("XIT — TX gated by Phase 3M-1"));
        NyiOverlay::markNyi(m_xitMinus, QStringLiteral("XIT — TX gated by Phase 3M-1"));
        NyiOverlay::markNyi(m_xitPlus,  QStringLiteral("XIT — TX gated by Phase 3M-1"));
    }

    columns->addLayout(rightCol, 3);
    root->addLayout(columns);

    // ── Tooltips ──────────────────────────────────────────────────────────
    // NereusSDR native — no Thetis per-slice badge equivalent
    m_sliceBadge->setToolTip(QStringLiteral("Slice identifier"));
    // From Thetis console.resx:5787 — chkVFOLock.ToolTip
    m_lockBtn->setToolTip(QStringLiteral("Keeps the VFO from changing while in the middle of a QSO."));
    // From Thetis console.resx:8277 — chkRxAnt.ToolTip
    m_rxAntBtn->setToolTip(QStringLiteral("Toggles receive antenna between RX and TX antennas for RX1"));
    // NereusSDR native — no single Thetis TX-antenna tooltip
    m_txAntBtn->setToolTip(QStringLiteral("Select the transmit antenna port"));
    // NereusSDR native — filter width label, no Thetis equivalent control
    m_filterWidthLbl->setToolTip(QStringLiteral("Current filter passband width"));
    // NereusSDR native — Thetis uses discrete radio buttons per mode
    m_modeCombo->setToolTip(QStringLiteral("Select operating mode"));
    // From Thetis console.resx:1560 — chkRX2Mute.ToolTip (same text for RX1)
    m_muteBtn->setToolTip(QStringLiteral("Mute - Mutes the output to the speaker."));
    // From Thetis console.resx:8433 — ptbAF.ToolTip
    m_afSlider->setToolTip(QStringLiteral("AF Gain - Monitor Volume for RX/TX"));
    // From Thetis console.resx:4554 — comboAGC.ToolTip
    m_agcCombo->setToolTip(QStringLiteral("Automatic Gain Control Mode Setting"));
    // From Thetis console.resx:8397 — ptbRF.ToolTip (ptbRF is the AGC-T slider)
    m_agcTSlider->setToolTip(QStringLiteral("AGC Max Gain - Operates similarly to traditional RF Gain. Right click AUTO based on noise floor."));
    // From Thetis console.resx:4335 — chkRIT.ToolTip
    m_ritOnBtn->setToolTip(QStringLiteral("Receive Incremental Tuning - offset RX frequency by value below in Hz."));
    // From Thetis console.resx:4416 — chkXIT.ToolTip (TX gated by Phase 3M-1)
    m_xitOnBtn->setToolTip(QStringLiteral("Transmit Incremental Tuning - offset TX frequency by the value below in Hz."));
}

void RxApplet::rebuildFilterButtons()
{
    // Remove all existing buttons from grid
    for (QPushButton* btn : m_filterBtns) {
        m_filterGrid->removeWidget(btn);
        btn->deleteLater();
    }
    m_filterBtns.clear();

    // 10 filter presets in 3-column grid (matching AetherSDR layout)
    // Blue active state when this filter is selected
    // Tier 1 wired → SliceModel::setFilter()
    static constexpr int kCols = 3;
    const int count = qMin(m_filterWidths.size(), 10);

    for (int i = 0; i < count; ++i) {
        const int widthHz = m_filterWidths.value(i, 2700);
        QString label;
        if (widthHz >= 1000) {
            label = QStringLiteral("%1K").arg(widthHz / 1000.0, 0, 'g', 2);
        } else {
            label = QStringLiteral("%1").arg(widthHz);
        }

        auto* btn = blueToggle(label, -1, 20);
        btn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        btn->setCheckable(true);
        // Smaller padding than base style to fit narrow column
        // (matches AetherSDR kButtonBase: padding 1px 2px)
        btn->setStyleSheet(btn->styleSheet() + QStringLiteral(
            "QPushButton { padding: 1px 2px; }"));

        connect(btn, &QPushButton::clicked, this, [this, widthHz] {
            applyFilterPreset(widthHz);
        });

        m_filterBtns.append(btn);
        m_filterGrid->addWidget(btn, i / kCols, i % kCols);
    }
}

void RxApplet::applyFilterPreset(int widthHz)
{
    if (!m_slice) { return; }

    // Determine low/high from width + current mode
    // For USB/CWU: low = 100, high = low + width
    // For LSB/CWL: high = -100, low = high - width
    // For AM/SAM/DSB: symmetric ±half
    // This mirrors AetherSDR RxApplet::applyFilterPreset() logic.
    const DSPMode mode = m_slice->dspMode();
    int low  = 0;
    int high = 0;

    switch (mode) {
    case DSPMode::USB:
    case DSPMode::CWU:
    case DSPMode::DIGU:
        low  = 100;
        high = low + widthHz;
        break;
    case DSPMode::LSB:
    case DSPMode::CWL:
    case DSPMode::DIGL:
        high = -100;
        low  = high - widthHz;
        break;
    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::DSB:
    case DSPMode::FM:
    case DSPMode::DRM:
        low  = -(widthHz / 2);
        high =  (widthHz / 2);
        break;
    default:
        low  = 100;
        high = low + widthHz;
        break;
    }

    m_slice->setFilter(low, high);
}

void RxApplet::updateFilterButtons()
{
    if (!m_slice) { return; }

    const int lo = m_slice->filterLow();
    const int hi = m_slice->filterHigh();
    const int width = hi - lo;

    // Highlight the button matching current filter width (±50 Hz tolerance)
    for (int i = 0; i < m_filterBtns.size(); ++i) {
        QPushButton* btn = m_filterBtns[i];
        const int bw = m_filterWidths.value(i, 0);
        const bool match = qAbs(width - bw) <= 50;
        // Suppress toggled signal to avoid echo loop
        QSignalBlocker blocker(btn);
        btn->setChecked(match);
    }
}

void RxApplet::updateFilterLabel()
{
    if (!m_slice) {
        m_filterWidthLbl->setText(QStringLiteral("---"));
        return;
    }
    m_filterWidthLbl->setText(
        formatFilterWidth(m_slice->filterLow(), m_slice->filterHigh()));
}

QString RxApplet::formatFilterWidth(int low, int high)
{
    const int width = qAbs(high - low);
    if (width >= 1000) {
        return QStringLiteral("%1K").arg(width / 1000.0, 0, 'g', 3);
    }
    return QStringLiteral("%1").arg(width);
}

void RxApplet::setSlice(SliceModel* slice)
{
    if (m_slice == slice) { return; }
    disconnectSlice(m_slice);
    m_slice = slice;
    connectSlice(m_slice);
    syncFromModel();
}

void RxApplet::setSliceIndex(int idx)
{
    static const char* kLetters[] = {"A", "B", "C", "D"};
    if (idx >= 0 && idx < 4) {
        m_sliceBadge->setText(QString::fromLatin1(kLetters[idx]));
    }
}

void RxApplet::setAntennaList(const QStringList& ants)
{
    m_antList = ants;
    // Update button labels to show current selection if changed
    if (m_slice) {
        m_rxAntBtn->setText(m_slice->rxAntenna());
        m_txAntBtn->setText(m_slice->txAntenna());
    }
}

// ── Model → UI sync ───────────────────────────────────────────────────────────

void RxApplet::syncFromModel()
{
    if (!m_slice) { return; }

    m_updatingFromModel = true;

    // Mode combo
    {
        const QString name = SliceModel::modeName(m_slice->dspMode());
        const int idx = m_modeCombo->findText(name);
        if (idx >= 0) { m_modeCombo->setCurrentIndex(idx); }
    }

    // AGC combo
    {
        const int modeVal = static_cast<int>(m_slice->agcMode());
        for (int i = 0; i < m_agcCombo->count(); ++i) {
            if (m_agcCombo->itemData(i).toInt() == modeVal) {
                m_agcCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    // AF gain slider
    m_afSlider->setValue(m_slice->afGain());

    // Antenna buttons
    m_rxAntBtn->setText(m_slice->rxAntenna());
    m_txAntBtn->setText(m_slice->txAntenna());

    // Filter label + preset buttons + passband widget
    updateFilterLabel();
    updateFilterButtons();
    m_filterPassband->setFilter(m_slice->filterLow(), m_slice->filterHigh());
    m_filterPassband->setMode(SliceModel::modeName(m_slice->dspMode()));

    // Lock state (S2.9)
    m_lockBtn->setChecked(m_slice->locked());
    m_lockBtn->setText(m_slice->locked()
        ? QString::fromUtf8("\xF0\x9F\x94\x92")   // 🔒
        : QString::fromUtf8("\xF0\x9F\x94\x93")); // 🔓

    // RIT state (S2.8)
    m_ritOnBtn->setChecked(m_slice->ritEnabled());
    {
        const int hz = m_slice->ritHz();
        m_ritLabel->setText(QStringLiteral("%1%2 Hz")
            .arg(hz >= 0 ? QStringLiteral("+") : QString{})
            .arg(hz));
    }

    // Step size label (Issue #69)
    m_stepLabel->setText(QStringLiteral("%1 Hz").arg(m_slice->stepHz()));

    m_updatingFromModel = false;
}

void RxApplet::connectSlice(SliceModel* s)
{
    if (!s) { return; }

    // Mode change → update combo + passband widget
    connect(s, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
        m_updatingFromModel = true;
        const QString name = SliceModel::modeName(mode);
        const int idx = m_modeCombo->findText(name);
        if (idx >= 0) { m_modeCombo->setCurrentIndex(idx); }
        m_updatingFromModel = false;
        updateFilterLabel();
        updateFilterButtons();
        m_filterPassband->setMode(name);
    });

    // AGC change → update combo
    connect(s, &SliceModel::agcModeChanged, this, [this](AGCMode mode) {
        m_updatingFromModel = true;
        const int modeVal = static_cast<int>(mode);
        for (int i = 0; i < m_agcCombo->count(); ++i) {
            if (m_agcCombo->itemData(i).toInt() == modeVal) {
                m_agcCombo->setCurrentIndex(i);
                break;
            }
        }
        m_updatingFromModel = false;
    });

    // AF gain change → update slider
    connect(s, &SliceModel::afGainChanged, this, [this](int gain) {
        m_updatingFromModel = true;
        m_afSlider->setValue(gain);
        m_updatingFromModel = false;
    });

    // Filter change → update label + buttons + passband widget
    connect(s, &SliceModel::filterChanged, this, [this](int lo, int hi) {
        updateFilterLabel();
        updateFilterButtons();
        m_filterPassband->setFilter(lo, hi);
    });

    // Antenna changes → update button labels
    connect(s, &SliceModel::rxAntennaChanged, this, [this](const QString& ant) {
        m_rxAntBtn->setText(ant);
    });
    connect(s, &SliceModel::txAntennaChanged, this, [this](const QString& ant) {
        m_txAntBtn->setText(ant);
    });

    // Lock model → UI sync (S2.9)
    connect(s, &SliceModel::lockedChanged, this, [this](bool locked) {
        m_updatingFromModel = true;
        m_lockBtn->setChecked(locked);
        m_lockBtn->setText(locked
            ? QString::fromUtf8("\xF0\x9F\x94\x92")   // 🔒
            : QString::fromUtf8("\xF0\x9F\x94\x93")); // 🔓
        m_updatingFromModel = false;
    });

    // RIT model → UI sync (S2.8)
    connect(s, &SliceModel::ritEnabledChanged, this, [this](bool on) {
        m_updatingFromModel = true;
        m_ritOnBtn->setChecked(on);
        m_updatingFromModel = false;
    });
    connect(s, &SliceModel::ritHzChanged, this, [this](int hz) {
        m_ritLabel->setText(QStringLiteral("%1%2 Hz")
            .arg(hz >= 0 ? QStringLiteral("+") : QString{})
            .arg(hz));
    });

    // Step size model → label sync (Issue #69)
    connect(s, &SliceModel::stepHzChanged, this, [this](int hz) {
        m_stepLabel->setText(QStringLiteral("%1 Hz").arg(hz));
    });

    // ATT/S-ATT — wire to StepAttenuatorController if available
    auto* attCtrl = m_model ? m_model->stepAttController() : nullptr;
    if (attCtrl) {
        // Populate preamp combo from board capabilities when radio is connected.
        // From Thetis console.cs:40755 SetComboPreampForHPSDR().
        if (m_model->connection() && m_model->connection()->isConnected()) {
            const auto& info = m_model->connection()->radioInfo();
            const auto& caps = BoardCapsTable::forBoard(info.boardType);
            const auto preampItems = BoardCapsTable::preampItemsForBoard(
                info.boardType, caps.hasAlexFilters);

            QSignalBlocker blk(m_preampCombo);
            m_preampCombo->clear();
            for (const auto& item : preampItems) {
                m_preampCombo->addItem(QString::fromLatin1(item.label), item.modeInt);
            }

            // Set step att spinbox range from board capabilities.
            // From Thetis setup.cs:15765 udHermesStepAttenuatorData max.
            const int maxDb = BoardCapsTable::stepAttMaxDb(
                info.boardType, caps.hasAlexFilters);
            m_stepAttSpin->setRange(0, maxDb);
            attCtrl->setMaxAttenuation(maxDb);
        }

        connect(m_stepAttSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [attCtrl](int val) {
            attCtrl->setAttenuation(val);
        });

        connect(m_preampCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, attCtrl](int idx) {
            if (idx < 0) { return; }  // guard during clear/repopulate
            int modeInt = m_preampCombo->itemData(idx).toInt();
            attCtrl->setPreampMode(static_cast<PreampMode>(modeInt));
        });

        connect(attCtrl, &StepAttenuatorController::attenuationChanged,
                this, [this](int dB) {
            QSignalBlocker blk(m_stepAttSpin);
            m_stepAttSpin->setValue(dB);
        });

        connect(attCtrl, &StepAttenuatorController::preampModeChanged,
                this, [this](PreampMode mode) {
            QSignalBlocker blk(m_preampCombo);
            int modeInt = static_cast<int>(mode);
            for (int i = 0; i < m_preampCombo->count(); ++i) {
                if (m_preampCombo->itemData(i).toInt() == modeInt) {
                    m_preampCombo->setCurrentIndex(i);
                    return;
                }
            }
        });

        // React to step-att-enabled changes (ATT ↔ S-ATT mode switch)
        connect(attCtrl, &StepAttenuatorController::stepAttEnabledChanged,
                this, [this](bool stepOn) {
            m_attLabel->setText(stepOn ? QStringLiteral("S-ATT")
                                      : QStringLiteral("ATT"));
            m_attStack->setCurrentIndex(stepOn ? 1 : 0);
        });

        // Sync initial state from controller
        const bool stepOn = attCtrl->stepAttEnabled();
        m_attLabel->setText(stepOn ? QStringLiteral("S-ATT") : QStringLiteral("ATT"));
        m_attStack->setCurrentIndex(stepOn ? 1 : 0);
        {
            QSignalBlocker blk(m_stepAttSpin);
            m_stepAttSpin->setValue(attCtrl->attenuatorDb());
        }
        {
            QSignalBlocker blk(m_preampCombo);
            int modeInt = static_cast<int>(attCtrl->preampMode());
            for (int i = 0; i < m_preampCombo->count(); ++i) {
                if (m_preampCombo->itemData(i).toInt() == modeInt) {
                    m_preampCombo->setCurrentIndex(i);
                    break;
                }
            }
        }
    }
}

void RxApplet::disconnectSlice(SliceModel* s)
{
    if (!s) { return; }
    s->disconnect(this);
}

// --- Auto AGC-T visual update (Task 7 — exact match of VfoWidget) ---
void RxApplet::updateAgcAutoVisuals(bool autoOn, float noiseFloorDbm, double offset)
{
    m_autoAgcActive = autoOn;
    m_noiseFloorDbm = noiseFloorDbm;

    if (!m_agcTSlider || !m_agcTContainer) {
        return;
    }

    if (autoOn) {
        // AUTO badge → bright green (active) — only the button illuminates
        if (m_agcAutoLabel) {
            m_agcAutoLabel->setStyleSheet(
                QStringLiteral("QPushButton { background: #1a2a1a; border: 1px solid #adff2f;"
                                "color: #adff2f; font-size: 7px; padding: 0 3px; border-radius: 2px; }"
                                "QPushButton:hover { background: #2a3a2a; }"));
        }

        // Show info sub-line
        if (m_agcInfoLabel) {
            m_agcInfoLabel->setText(
                QStringLiteral("NF %1 dB \u00b7 offset +%2")
                    .arg(static_cast<int>(noiseFloorDbm))
                    .arg(static_cast<int>(offset)));
            m_agcInfoLabel->show();
        }
    } else {
        // AUTO badge → dim gray (inactive)
        if (m_agcAutoLabel) {
            m_agcAutoLabel->setStyleSheet(
                QStringLiteral("QPushButton { background: #1a1a1a; border: 1px solid #445;"
                                "color: #556; font-size: 7px; padding: 0 3px; border-radius: 2px; }"
                                "QPushButton:hover { border-color: #adff2f; }"));
        }
        if (m_agcInfoLabel) {
            m_agcInfoLabel->hide();
        }
    }
}

} // namespace NereusSDR
