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

#include <QGuiApplication>

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/SkuUiProfile.h"
#include "core/P2RadioConnection.h"
#include "gui/AntennaPopupBuilder.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "core/accessories/AlexController.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"
#include "gui/styles/PopupMenuStyle.h"
#include "gui/widgets/FilterPassbandWidget.h"
#include "models/PanadapterModel.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "gui/widgets/FilterPresetEditDialog.h"

#include <algorithm>

#include <QAction>
#include <QCheckBox>
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

    // Phase 3P-F Task 4: observe the first panadapter for band changes so the
    // antenna buttons repopulate when the user QSYs across a band boundary.
    // Also do an initial populate at construction time.
    if (m_model && !m_model->panadapters().isEmpty()) {
        m_pan = m_model->panadapters().first();
        connect(m_pan, &PanadapterModel::bandChanged,
                this, &RxApplet::populateAntennaButtons);
        populateAntennaButtons(m_pan->band());
    }

    // Also repopulate when AlexController antennaChanged fires for the current band.
    if (m_model) {
        connect(&m_model->alexControllerMutable(), &AlexController::antennaChanged,
                this, [this](Band band) {
            if (m_pan && band == m_pan->band()) {
                populateAntennaButtons(band);
            }
        });
    }

    // Stage C2: subscribe to FilterPresetStore so user edits/reorders/resets
    // immediately update the filter button grid.
    if (m_model && m_model->filterPresetStore()) {
        connect(m_model->filterPresetStore(), &FilterPresetStore::presetsChanged,
                this, [this](DSPMode mode) {
            // Only rebuild when the changed mode matches the currently-active mode.
            if (m_slice && mode == m_slice->dspMode()) {
                rebuildFilterButtons(mode);
                updateFilterButtons();
            }
        });
    }

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
        // §A2 one-off: #4488ff is NereusSDR-original "live blue" (lock + RX-ant accents).
        // Not the same as kBlueBg (#0070c0) or kAccent (#00b4d8). Flagged for B7/B3 review.
        m_lockBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x94\x93"), this); // 🔓
        m_lockBtn->setCheckable(true);
        m_lockBtn->setFixedSize(20, 20);
        m_lockBtn->setFlat(true);
        m_lockBtn->setStyleSheet(QStringLiteral(
            "QPushButton { font-size: 13px; padding: 0; background: transparent; border: none; }"
            "QPushButton:checked { color: #4488ff; }"  // §A2 one-off "live blue"
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
        // §A2 one-off: #4488ff/"#66aaff" are NereusSDR-original "live blue" accent.
        // Not kAccent (#00b4d8). Flagged for B7/B3 review.
        m_rxAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
        m_rxAntBtn->setObjectName(QStringLiteral("m_rxAntBtn"));
        m_rxAntBtn->setFlat(true);
        m_rxAntBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  color: #4488ff; background: transparent; border: none;"  // §A2 one-off "live blue"
            "  font-size: 10px; font-weight: bold; padding: 0 2px;"
            "}"
            "QPushButton:hover { color: #66aaff; }"  // §A2 one-off hover derived from #4488ff
        ));
        connect(m_rxAntBtn, &QPushButton::clicked, this, [this] {
            // B3: AntennaPopupBuilder — capability-gated popup (Phase 3P-I-a T22).
            QMenu menu(this);
            const QString cur = m_slice ? m_slice->rxAntenna() : QString{};
            if (m_popupCaps && m_popupSku) {
                AntennaPopupBuilder::populate(&menu, *m_popupCaps, *m_popupSku,
                    AntennaPopupBuilder::Mode::RX, cur);
            } else {
                for (const QString& ant : m_antList) {
                    QAction* act = menu.addAction(ant);
                    act->setCheckable(true);
                    act->setChecked(ant == cur);
                }
            }
            menu.setStyleSheet(QString::fromLatin1(kPopupMenu));  // Phase 3P-I-a T22 — issue #98
            const QAction* sel = menu.exec(
                m_rxAntBtn->mapToGlobal(QPoint(0, m_rxAntBtn->height())));
            if (sel && m_slice) {
                const QString text = sel->data().isValid() ? sel->data().toString()
                                                           : sel->text();
                m_slice->setRxAntenna(text);
                // Phase 3P-F Task 4: persist per-band assignment in AlexController.
                if (m_model && m_pan) {
                    // ANT1/2/3 → setRxAnt. RX-only labels → setRxOnlyAnt (position in sku).
                    // "RX out on TX" is the bypass toggle — not routed via setRxAnt.
                    if (text.startsWith(QStringLiteral("ANT"))) {
                        int antNum = 1;
                        if (text == QStringLiteral("ANT2")) { antNum = 2; }
                        else if (text == QStringLiteral("ANT3")) { antNum = 3; }
                        m_model->alexControllerMutable().setRxAnt(m_pan->band(), antNum);
                    } else if (m_popupSku && text != QStringLiteral("RX out on TX")) {
                        // RX-only label: find its position in sku.rxOnlyLabels (1-indexed)
                        const auto& lbls = m_popupSku->rxOnlyLabels;
                        for (int i = 0; i < static_cast<int>(lbls.size()); ++i) {
                            if (lbls[static_cast<size_t>(i)] == text) {
                                m_model->alexControllerMutable().setRxOnlyAnt(m_pan->band(), i + 1);
                                break;
                            }
                        }
                    }
                }
            }
        });
        row->addWidget(m_rxAntBtn);

        // Control 4: TX antenna button (flat, color #ff4444, transparent bg)
        // From AetherSDR RxApplet.cpp lines 292-311
        // §A2 one-off: #ff4444 used as semantic "TX color" (not error/danger indicator).
        // kRedBorder/kGaugeDanger are also #ff4444 but connote error — using them here
        // would misname the intent. #ff6666 is the hover. Flagged for B3 review.
        m_txAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
        m_txAntBtn->setObjectName(QStringLiteral("m_txAntBtn"));
        m_txAntBtn->setFlat(true);
        m_txAntBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  color: #ff4444; background: transparent; border: none;"  // §A2 one-off TX color
            "  font-size: 10px; font-weight: bold; padding: 0 2px;"
            "}"
            "QPushButton:hover { color: #ff6666; }"  // §A2 one-off hover derived from #ff4444
        ));
        connect(m_txAntBtn, &QPushButton::clicked, this, [this] {
            // B3: AntennaPopupBuilder TX mode — only main ANT1-3 (Phase 3P-I-a T22).
            QMenu menu(this);
            const QString cur = m_slice ? m_slice->txAntenna() : QString{};
            if (m_popupCaps && m_popupSku) {
                AntennaPopupBuilder::populate(&menu, *m_popupCaps, *m_popupSku,
                    AntennaPopupBuilder::Mode::TX, cur);
            } else {
                for (const QString& ant : m_antList) {
                    QAction* act = menu.addAction(ant);
                    act->setCheckable(true);
                    act->setChecked(ant == cur);
                }
            }
            menu.setStyleSheet(QString::fromLatin1(kPopupMenu));  // Phase 3P-I-a T22 — issue #98
            const QAction* sel = menu.exec(
                m_txAntBtn->mapToGlobal(QPoint(0, m_txAntBtn->height())));
            if (sel && m_slice) {
                const QString text = sel->data().isValid() ? sel->data().toString()
                                                           : sel->text();
                m_slice->setTxAntenna(text);
                // Phase 3P-F Task 4: persist per-band TX assignment in AlexController.
                // setTxAnt() respects blockTxAnt2/3 safety guards from Task 1.
                if (m_model && m_pan) {
                    int antNum = 1;
                    if (text == QStringLiteral("ANT2")) { antNum = 2; }
                    else if (text == QStringLiteral("ANT3")) { antNum = 3; }
                    m_model->alexControllerMutable().setTxAnt(m_pan->band(), antNum);
                }
            }
        });
        row->addWidget(m_txAntBtn);

        row->addStretch(1);

        // Control 5: Filter width label (color #00c8ff, 11px bold)
        // §A2 one-off: #00c8ff is a lighter cyan than kAccent (#00b4d8); distinct
        // intent (filter display highlight vs interactive accent). Not snapped.
        m_filterWidthLbl = new QLabel(QStringLiteral("2.9K"), this);
        m_filterWidthLbl->setAlignment(Qt::AlignCenter);
        m_filterWidthLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #00c8ff; font-size: 11px; font-weight: bold; }"  // §A2 one-off filter highlight
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
        // Seed from the slice's actual mode so a restore-from-AppSettings
        // launch (e.g. LSB / DIGL) shows the correct preset grid before the
        // first dspModeChanged event fires.  Falls back to USB if no slice
        // is bound yet.
        rebuildFilterButtons(m_slice ? m_slice->dspMode() : DSPMode::USB);
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

    // Mute button removed: VfoWidget + TitleBar are the 2 canonical surfaces.
    // AGC-T container placed here after AGC combo is constructed below.
    // (§B4 ui-polish-cross-surface — bench feedback Plan 3 review)
    // AF gain slider removed: TitleBar master volume + VfoWidget per-slice
    // AF control are the canonical 2 surfaces (§B4 ui-polish-cross-surface).

    // Control 13: Audio pan slider — L ←→ R, center = 50
    // Wired to SliceModel::setAudioPan() — §B4 ui-polish-cross-surface.
    // Slider range 0–100, center 50 → mapped to SliceModel pan range −1.0..+1.0.
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
        connect(m_panSlider, &QSlider::valueChanged, this, [this](int val) {
            if (m_updatingFromModel || !m_slice) { return; }
            m_slice->setAudioPan((val - 50) / 50.0);
        });
        row->addWidget(m_panSlider, 1);

        auto* rLbl = new QLabel(QStringLiteral("R"), this);
        rLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }"
        ).arg(Style::kTextSecondary));
        row->addWidget(rLbl);

        rightCol->addLayout(row);
    }

    // Control 14: Squelch toggle + slider
    // Wired to SliceModel::setSsqlEnabled() / setSsqlThresh() — §B4.
    // Slider 0–100 maps directly to ssqlThresh double (same units used by
    // SliceModel default of 16.0; VfoWidget uses the same 0–100 range).
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_sqlBtn = greenToggle(QStringLiteral("SQL"), 52, 20);
        connect(m_sqlBtn, &QPushButton::toggled, this, [this](bool on) {
            if (m_updatingFromModel || !m_slice) { return; }
            m_slice->setSsqlEnabled(on);
        });
        row->addWidget(m_sqlBtn);

        m_sqlSlider = new QSlider(Qt::Horizontal, this);
        m_sqlSlider->setRange(0, 100);
        m_sqlSlider->setValue(20);
        m_sqlSlider->setFixedHeight(18);
        m_sqlSlider->setStyleSheet(Style::sliderHStyle());
        connect(m_sqlSlider, &QSlider::valueChanged, this, [this](int val) {
            if (m_updatingFromModel || !m_slice) { return; }
            m_slice->setSsqlThresh(static_cast<double>(val));
        });
        row->addWidget(m_sqlSlider, 1);

        rightCol->addLayout(row);
    }

    // NB controls intentionally absent from RxApplet per strict Thetis parity:
    // Thetis never puts NB controls on a per-slice applet. The valid Thetis
    // surfaces are (1) chkNB tri-state on the VFO flag, (2) Setup → DSP →
    // NB/SNB for tuning, (3) the DSP menu bar. An earlier draft shipped an
    // NB cycling button + Thr + Lag sliders here; removed 2026-04-22 to
    // match Thetis. Per-band NB MODE persistence still lives on SliceModel
    // (not tuning) so different bands can have different NB/NB2/Off states.

    // ATT/S-ATT row — between NB and AGC
    // From Thetis console.cs: comboPreamp / udRX1StepAttData (stacked)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        row->setContentsMargins(0, 0, 0, 0);

        m_attLabel = new QLabel(QStringLiteral("ATT"), this);
        m_attLabel->setFixedWidth(34);
        m_attLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }").arg(Style::kTitleText));
        row->addWidget(m_attLabel);

        m_attStack = new QStackedWidget(this);
        m_attStack->setFixedHeight(20);

        // Page 0: Preamp combo (ATT mode — step att disabled).
        // Phase 3P-C Step 3: populate from BoardCapabilities::preampItemsForBoard()
        // at construction, not hardcoded. Matches Thetis SetComboPreampForHPSDR
        // console.cs:40755-40825 [@501e3f5] — per board at init time.
        m_preampCombo = new QComboBox(this);
        {
            const HPSDRHW initBoard = m_model
                ? m_model->hardwareProfile().effectiveBoard
                : HPSDRHW::Hermes;
            const bool initAlex = m_model
                ? m_model->boardCapabilities().hasAlexFilters
                : false;
            const auto initItems = BoardCapsTable::preampItemsForBoard(initBoard, initAlex);
            for (const auto& item : initItems) {
                m_preampCombo->addItem(QString::fromLatin1(item.label), item.modeInt);
            }
        }
        m_preampCombo->setFixedWidth(70);
        m_preampCombo->setFixedHeight(20);
        applyComboStyle(m_preampCombo);
        m_attStack->addWidget(m_preampCombo);

        // Page 1: Step att spinbox (S-ATT mode — step att enabled)
        // Phase 3P-A Task 15: initialize range from BoardCapabilities so HL2
        // (signed −28..+32 per mi0bot setup.cs:16085-16086 [v2.10.3.13-beta2])
        // is correct at widget creation, not only after connect.
        // From Thetis setup.cs:15765 [v2.10.3.13].
        {
            const bool capsPresent =
                m_model && m_model->boardCapabilities().attenuator.present;
            const int initMin = capsPresent
                ? m_model->boardCapabilities().attenuator.minDb : 0;
            const int initMax = capsPresent
                ? m_model->boardCapabilities().attenuator.maxDb : 31;
            m_stepAttSpin = new QSpinBox(this);
            m_stepAttSpin->setRange(initMin, initMax);
        }
        m_stepAttSpin->setSuffix(QStringLiteral(" dB"));
        m_stepAttSpin->setFixedWidth(70);
        m_stepAttSpin->setFixedHeight(20);
        m_attStack->addWidget(m_stepAttSpin);

        m_attStack->setCurrentIndex(0);  // default to preamp combo
        row->addWidget(m_attStack, 1);

        rightCol->addLayout(row);
    }

    // Controls 9 + 10: AGC combo + AGC threshold slider (Option B layout).
    // From AetherSDR RxApplet.cpp lines 738-768
    // Bench feedback after 39dbdd2: AGC-T slider was still cramped even with
    // its own row because auxiliary widgets (label 40 + value 32 + AUTO 30 =
    // ~102 px fixed) consumed most of the right-column width. Meanwhile the
    // AGC mode combo "Med" sat in its own row with empty space to the right.
    // Option B: combine AGC combo + AGC-T header into one header row; slider
    // gets its own full-container-width row immediately below.
    {
        m_agcTContainer = new QWidget(this);
        auto* containerLayout = new QVBoxLayout(m_agcTContainer);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(2);

        // Header row: AGC mode combo | AGC-T label | stretch | dB value | AUTO
        auto* headerRow = new QHBoxLayout;
        headerRow->setSpacing(4);

        // Control 9: AGC combo (fixedWidth 52), items: Off/Long/Slow/Med/Fast
        // Tier 1 wired → SliceModel::setAgcMode()
        m_agcCombo = new QComboBox(m_agcTContainer);
        m_agcCombo->addItem(QStringLiteral("Off"),  static_cast<int>(AGCMode::Off));
        m_agcCombo->addItem(QStringLiteral("Long"), static_cast<int>(AGCMode::Long));
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
        headerRow->addWidget(m_agcCombo);

        // Control 10 header: "AGC-T" label
        m_agcTLabelWidget = new QLabel(QStringLiteral("AGC-T"), m_agcTContainer);
        m_agcTLabelWidget->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(Style::kLabelMid));
        m_agcTLabelWidget->setFixedWidth(40);
        headerRow->addWidget(m_agcTLabelWidget);

        headerRow->addStretch(1);

        m_agcTLabel = new QLabel(QStringLiteral("-20"), m_agcTContainer);
        m_agcTLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(Style::kTextPrimary));
        m_agcTLabel->setFixedWidth(32);
        m_agcTLabel->setAlignment(Qt::AlignRight);
        headerRow->addWidget(m_agcTLabel);

        // §A2 one-offs for AUTO badge (inactive state):
        // #1a1a1a = near-black bg (darker than kDisabledBg #1a1a2a — no blue tint).
        // #445 / #556 = very dim purple-gray border/text (3-digit shorthands, off-palette).
        // #adff2f = lime-green hover accent (active-AUTO color, NereusSDR-original).
        // None of these map cleanly to a canonical constant; all kept as one-offs.
        m_agcAutoLabel = new QPushButton(QStringLiteral("AUTO"), m_agcTContainer);
        m_agcAutoLabel->setStyleSheet(
            QStringLiteral("QPushButton { background: #1a1a1a; border: 1px solid #445;"  // §A2 one-off
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
        headerRow->addWidget(m_agcAutoLabel);

        containerLayout->addLayout(headerRow);

        // Slider row: full container width — no sibling widgets.
        // From Thetis Project Files/Source/Console/console.cs:45977 — agc_thresh_point
        m_agcTSlider = new QSlider(Qt::Horizontal, m_agcTContainer);
        m_agcTSlider->setRange(-160, 0);
        m_agcTSlider->setValue(-20);
        m_agcTSlider->setFixedHeight(18);
        m_agcTSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: %1; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: %2; width: 12px; margin: -3px 0; border-radius: 6px; }")
                .arg(Style::kButtonBg, Style::kAccent));
        // From Thetis console.resx:8397 — ptbRF.ToolTip (ptbRF is the AGC-T slider)
        m_agcTSlider->setToolTip(QStringLiteral("AGC Max Gain - Operates similarly to traditional RF Gain. Right click AUTO based on noise floor."));
        containerLayout->addWidget(m_agcTSlider);

        // Second row: info sub-line (hidden by default)
        m_agcInfoLabel = new QLabel(m_agcTContainer);
        // §A2 one-off: #33aa33 is NereusSDR-original AGC-info green (one location).
        // Not in StyleConstants; distinct from kGreenBg/kGreenText. Kept as one-off.
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

        rightCol->addWidget(m_agcTContainer);
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

        // Wire XIT controls to SliceModel (B6).
        // Toggle → enable/disable XIT on the slice.
        connect(m_xitOnBtn, &QPushButton::toggled, this, [this](bool on) {
            if (m_updatingFromModel || !m_slice) { return; }
            m_slice->setXitEnabled(on);
        });
        // Minus/Plus → decrement/increment by current step, clamped to ±10000 Hz.
        connect(m_xitMinus, &QPushButton::clicked, this, [this]() {
            if (!m_slice) { return; }
            int step = m_slice->stepHz();
            m_slice->setXitHz(std::clamp(m_slice->xitHz() - step, -10000, 10000));
        });
        connect(m_xitPlus, &QPushButton::clicked, this, [this]() {
            if (!m_slice) { return; }
            int step = m_slice->stepHz();
            m_slice->setXitHz(std::clamp(m_slice->xitHz() + step, -10000, 10000));
        });
        // Zero → reset XIT offset to 0.
        connect(m_xitZero, &QPushButton::clicked, this, [this]() {
            if (!m_slice) { return; }
            m_slice->setXitHz(0);
        });
    }

    columns->addLayout(rightCol, 3);
    root->addLayout(columns);

    // Phase 3P-B Task 10: ADC OVL badge row + RX1 preamp toggle.
    //
    // Number of badges depends on BoardCapabilities::adcCount.  We gate on
    // p2PreampPerAdc (added Task 6) as the "dual-ADC board" proxy — it's
    // true for OrionMKII family (ANAN-7000DLE / 8000DLE / AnvelinaPro3) and
    // false for Saturn (single-ADC at the wire layer) and all P1 boards.
    {
        const bool dualAdc = m_model
            && m_model->boardCapabilities().p2PreampPerAdc;
        const int  adcCount = dualAdc ? 2 : 1;

        // OVL badge row ────────────────────────────────────────────────────
        m_ovlRow = new QHBoxLayout;
        m_ovlRow->setSpacing(4);
        m_ovlRow->setContentsMargins(0, 2, 0, 0);

        static const char* kOvlStyleNormal =
            "QLabel { background: rgba(255,255,255,0.06);"
            " color: rgba(255,255,255,0.45);"
            " border: 1px solid rgba(255,255,255,0.12);"
            " border-radius: 3px; font-size: 9px; font-weight: bold;"
            " padding: 1px 4px; }";

        // 2026-04-21: OVL badges are retained as backing state for the
        // StepAttenuatorController signal target and for
        // tst_rxapplet_adc_ovl's visibleOvlBadgeCountForTest() (which
        // only checks badge existence, not layout visibility). They are
        // NOT added to the visible row — the authoritative ADC-overload
        // indicator now lives on the MainWindow status bar, left of
        // STATION, mirroring Thetis's ucInfoBar Warning() placement.
        for (int i = 0; i < adcCount; ++i) {
            QString label = (adcCount == 1)
                ? QStringLiteral("OVL")
                : QStringLiteral("OVL") + QString(i == 0 ? "\u2080" : "\u2081");
            m_ovlBadges[i] = new QLabel(label, this);
            m_ovlBadges[i]->setStyleSheet(QString::fromLatin1(kOvlStyleNormal));
            m_ovlBadges[i]->setAlignment(Qt::AlignCenter);
            m_ovlBadges[i]->hide();  // not added to m_ovlRow; hidden
        }

        m_ovlRow->addStretch(1);

        // RX1 preamp toggle (dual-ADC boards only) ────────────────────────
        if (dualAdc) {
            m_rx1PreampToggle = new QCheckBox(QStringLiteral("RX1 preamp"), this);
            m_rx1PreampToggle->setStyleSheet(QStringLiteral(
                "QCheckBox { color: %1; font-size: 10px; }"
                "QCheckBox::indicator { width: 12px; height: 12px; }").arg(Style::kTitleText));
            // Phase 3P-B Task 10: RX1 preamp wires to P2RadioConnection::setRx1Preamp
            // which routes to CodecContext.p2Rx1Preamp → byte 1403 bit 1.
            connect(m_rx1PreampToggle, &QCheckBox::toggled, this, [this](bool on) {
                if (!m_model) { return; }
                auto* conn = qobject_cast<class P2RadioConnection*>(m_model->connection());
                if (!conn) { return; }
                // P2RadioConnection lives on m_connThread.  Dispatch onto its
                // event loop so the m_rx[1].preamp mutation and the resulting
                // sendCmdHighPriority() run on the connection thread, not the
                // GUI thread.  (Codex PR #94 review.)
                QMetaObject::invokeMethod(conn, [conn, on] { conn->setRx1Preamp(on); },
                                          Qt::QueuedConnection);
            });
            m_ovlRow->addWidget(m_rx1PreampToggle);
        }

        root->addLayout(m_ovlRow);
    }

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
    // m_muteBtn removed §B4 bench review — VfoWidget + TitleBar are the 2 surfaces.
    // m_afSlider removed in §B4 — TitleBar master + VfoWidget AF are the 2 surfaces.
    // From Thetis console.resx:4554 — comboAGC.ToolTip
    m_agcCombo->setToolTip(QStringLiteral("Automatic Gain Control Mode Setting"));
    // From Thetis console.resx:8397 — ptbRF.ToolTip (ptbRF is the AGC-T slider)
    m_agcTSlider->setToolTip(QStringLiteral("AGC Max Gain - Operates similarly to traditional RF Gain. Right click AUTO based on noise floor."));
    // From Thetis console.resx:4335 — chkRIT.ToolTip
    m_ritOnBtn->setToolTip(QStringLiteral("Receive Incremental Tuning - offset RX frequency by value below in Hz."));
    // From Thetis console.resx:4416 — chkXIT.ToolTip (TX gated by Phase 3M-1)
    m_xitOnBtn->setToolTip(QStringLiteral("Transmit Incremental Tuning - offset TX frequency by the value below in Hz."));
}

void RxApplet::rebuildFilterButtons(DSPMode mode)
{
    // Remove all existing buttons from grid
    for (QPushButton* btn : m_filterBtns) {
        m_filterGrid->removeWidget(btn);
        btn->deleteLater();
    }
    m_filterBtns.clear();

    // Stage C2: prefer FilterPresetStore (user overrides over Thetis defaults).
    // Fall back to SliceModel::presetsForMode if no store is available.
    // InitFilterPresets source: Thetis console.cs:5180-5575 [v2.10.3.13].
    // Up to 10 presets in 3-column grid.
    // Tier 1 wired → SliceModel::setFilter() via applyFilterPreset(low, high).
    FilterPresetStore* store = m_model ? m_model->filterPresetStore() : nullptr;

    QList<FilterPreset> storePresets;
    if (store) {
        storePresets = store->presetsForMode(mode);
        // Mirror as pairs for m_filterPresets (needed by updateFilterButtons).
        m_filterPresets.clear();
        for (const FilterPreset& fp : storePresets) {
            m_filterPresets.append({fp.low, fp.high});
        }
    } else {
        m_filterPresets = SliceModel::presetsForMode(mode);
        for (const auto& [low, high] : m_filterPresets) {
            FilterPreset fp;
            fp.name = QString();
            fp.low  = low;
            fp.high = high;
            storePresets.append(fp);
        }
    }

    static constexpr int kCols = 3;
    const int count = qMin(storePresets.size(), 10);

    for (int i = 0; i < count; ++i) {
        const FilterPreset& fp = storePresets[i];
        const int widthHz = qAbs(fp.high - fp.low);
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
        // Tooltip: show name + filter edges
        btn->setToolTip(QStringLiteral("%1: %2 Hz to %3 Hz")
            .arg(fp.name.isEmpty() ? QStringLiteral("F%1").arg(i + 1) : fp.name)
            .arg(fp.low).arg(fp.high));

        const int low  = fp.low;
        const int high = fp.high;
        connect(btn, &QPushButton::clicked, this, [this, low, high, mode] {
            applyFilterPreset(low, high);
            // Shift+click — also snap the TX passband to match the RX preset
            // (Thetis-style alignment shortcut).  Convert IQ-space preset
            // values to TX audio Hz: LSB family flips magnitude order, USB
            // family is identity, symmetric uses (0, |high|).
            if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) {
                if (!m_model) { return; }
                const bool isSymmetric =
                    mode == DSPMode::AM || mode == DSPMode::SAM
                 || mode == DSPMode::DSB || mode == DSPMode::FM
                 || mode == DSPMode::DRM;
                int audioLow, audioHigh;
                if (isSymmetric) {
                    audioLow  = 0;
                    audioHigh = qAbs(high);
                } else {
                    const int aLow  = qAbs(low);
                    const int aHigh = qAbs(high);
                    audioLow  = qMin(aLow, aHigh);
                    audioHigh = qMax(aLow, aHigh);
                }
                m_model->transmitModel().setFilterLow(audioLow);
                m_model->transmitModel().setFilterHigh(audioHigh);
            }
        });

        // Stage C2: right-click context menu → edit / reset this preset.
        btn->setContextMenuPolicy(Qt::CustomContextMenu);
        const int slot = i;
        connect(btn, &QPushButton::customContextMenuRequested, this,
                [this, slot, mode](const QPoint& pos) {
            if (!m_model || !m_model->filterPresetStore()) { return; }
            FilterPresetStore* store = m_model->filterPresetStore();
            QMenu menu(this);
            menu.setStyleSheet(QString::fromLatin1(kPopupMenu));  // Stage C2 — issue #98 parity
            QAction* editAct = menu.addAction(QStringLiteral("Edit this preset…"));
            QAction* resetAct = menu.addAction(QStringLiteral("Reset this preset"));
            QAction* chosen = menu.exec(qobject_cast<QWidget*>(sender())->mapToGlobal(pos));
            if (chosen == editAct) {
                auto* dlg = new FilterPresetEditDialog(store, mode, slot, this);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->exec();
            } else if (chosen == resetAct) {
                store->resetPreset(mode, slot);
            }
        });

        m_filterBtns.append(btn);
        m_filterGrid->addWidget(btn, i / kCols, i % kCols);
    }
}

void RxApplet::applyFilterPreset(int low, int high)
{
    if (!m_slice) { return; }
    // low/high come directly from SliceModel::presetsForMode() — no mode-switching needed.
    m_slice->setFilter(low, high);
}

void RxApplet::updateFilterButtons()
{
    if (!m_slice) { return; }

    const int curLow  = m_slice->filterLow();
    const int curHigh = m_slice->filterHigh();

    // Highlight the button matching current (low, high) within ±50 Hz tolerance per edge.
    for (int i = 0; i < m_filterBtns.size(); ++i) {
        QPushButton* btn = m_filterBtns[i];
        if (i >= m_filterPresets.size()) {
            QSignalBlocker blocker(btn);
            btn->setChecked(false);
            continue;
        }
        const auto [pLow, pHigh] = m_filterPresets[i];
        const bool match = (qAbs(curLow - pLow) <= 50) && (qAbs(curHigh - pHigh) <= 50);
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

// Phase 3P-I-a T16 — hide antenna buttons on boards without Alex.
// HL2 / Atlas / bare-ADC SKUs have no antenna relay; the buttons
// would be zombie controls (visible but no-op) and would mislead users.
// Matches VfoWidget::setBoardCapabilities (T15) one-for-one so the whole
// header row stays in sync.
void RxApplet::setBoardCapabilities(const BoardCapabilities& caps)
{
    const bool showAnt = caps.hasAlex && caps.antennaInputCount >= 3;
    if (m_rxAntBtn) { m_rxAntBtn->setVisible(showAnt); }
    if (m_txAntBtn) { m_txAntBtn->setVisible(showAnt); }

    // hermes-filter-debug Bug 1: re-range the S-ATT spinbox from caps on
    // every board change.  The construction-time setRange (line 599) runs
    // before any radio is identified and may pick up the default 0..31 if
    // boardCapabilities() is uninitialised; the post-connect block in
    // connectSlice() only fires when the slice happens to be re-set after
    // the radio is up.  This hook is the canonical board-driven path: it
    // runs on every currentRadioChanged emission (MainWindow.cpp:1400-1403)
    // so HL2's signed -28..+32 range (mi0bot setup.cs:16085-16086
    // [v2.10.3.13-beta2]) takes effect as soon as caps are known.
    if (m_stepAttSpin && caps.attenuator.present) {
        m_stepAttSpin->setRange(caps.attenuator.minDb, caps.attenuator.maxDb);
    }

    // B3: store caps for AntennaPopupBuilder (popup lambdas read this).
    m_popupCaps = caps;
}

// B3: per-SKU UI overlay for antenna popup — mirrors VfoWidget::setHpsdrSku.
// Called by MainWindow on currentRadioChanged after setBoardCapabilities.
void RxApplet::setHpsdrSku(HPSDRModel sku)
{
    m_popupSku = skuUiProfileFor(sku);
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

    // AF gain slider removed — see §B4.

    // Mute removed from RxApplet (§B4 bench review) — VfoWidget is the surface.
    // Pan / SQL — wired in §B4
    {
        QSignalBlocker bl(m_panSlider);
        // SliceModel pan: −1.0..+1.0 → slider 0..100 (center = 50)
        m_panSlider->setValue(qRound(m_slice->audioPan() * 50.0 + 50.0));
    }
    {
        QSignalBlocker bl(m_sqlBtn);
        m_sqlBtn->setChecked(m_slice->ssqlEnabled());
    }
    {
        QSignalBlocker bl(m_sqlSlider);
        m_sqlSlider->setValue(qRound(m_slice->ssqlThresh()));
    }

    // AGC-T slider initial pull (§B1 fix-up).
    if (m_agcTSlider) {
        QSignalBlocker bl(m_agcTSlider);
        const int t = m_slice->agcThreshold();
        m_agcTSlider->setValue(t);
        if (m_agcTLabel) {
            m_agcTLabel->setText(QString::number(t));
        }
    }

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

    // XIT state (B6)
    {
        QSignalBlocker bl(m_xitOnBtn);
        m_xitOnBtn->setChecked(m_slice->xitEnabled());
    }
    {
        const int hz = m_slice->xitHz();
        m_xitLabel->setText(QStringLiteral("%1%2 Hz")
            .arg(hz >= 0 ? QStringLiteral("+") : QString{})
            .arg(hz));
    }

    // Step size label (Issue #69)
    m_stepLabel->setText(QStringLiteral("%1 Hz").arg(m_slice->stepHz()));

    // NB button + NB1 tuning sliders removed from RxApplet per strict Thetis
    // parity. NB state lives on VFO flag, Setup → DSP → NB/SNB, and menu bar.

    m_updatingFromModel = false;
}

void RxApplet::connectSlice(SliceModel* s)
{
    if (!s) { return; }

    // Mode change → update combo + filter grid + passband widget
    connect(s, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
        m_updatingFromModel = true;
        const QString name = SliceModel::modeName(mode);
        const int idx = m_modeCombo->findText(name);
        if (idx >= 0) { m_modeCombo->setCurrentIndex(idx); }
        m_updatingFromModel = false;
        // Rebuild filter buttons for the new mode (canonical preset list from SliceModel)
        rebuildFilterButtons(mode);
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

    // AF gain slider removed from RxApplet (§B4) — signal handled by VfoWidget.

    // mutedChanged removed from RxApplet (§B4 bench review) — VfoWidget is the surface.
    // Pan / SQL model → UI sync (§B4)
    connect(s, &SliceModel::audioPanChanged, this, [this](double pan) {
        QSignalBlocker bl(m_panSlider);
        m_panSlider->setValue(qRound(pan * 50.0 + 50.0));
    });
    connect(s, &SliceModel::ssqlEnabledChanged, this, [this](bool on) {
        QSignalBlocker bl(m_sqlBtn);
        m_sqlBtn->setChecked(on);
    });
    connect(s, &SliceModel::ssqlThreshChanged, this, [this](double thresh) {
        QSignalBlocker bl(m_sqlSlider);
        m_sqlSlider->setValue(qRound(thresh));
    });

    // AGC-T slider model→UI sync (§B1 fix-up).
    // The B1 alignment commits added AGC-T writing (m_slice->setAgcThreshold)
    // but omitted the reverse direction, leaving VfoWidget and RxApplet sliders
    // decoupled. Use QSignalBlocker to avoid the echo loop identical to the
    // muted/audioPan/ssqlThresh connects above.
    connect(s, &SliceModel::agcThresholdChanged, this, [this](int v) {
        if (m_agcTSlider) {
            QSignalBlocker bl(m_agcTSlider);
            m_agcTSlider->setValue(v);
            if (m_agcTLabel) {
                m_agcTLabel->setText(QString::number(v));
            }
        }
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

    // XIT model → UI sync (B6)
    connect(s, &SliceModel::xitEnabledChanged, this, [this](bool on) {
        m_updatingFromModel = true;
        QSignalBlocker bl(m_xitOnBtn);
        m_xitOnBtn->setChecked(on);
        m_updatingFromModel = false;
    });
    connect(s, &SliceModel::xitHzChanged, this, [this](int hz) {
        m_xitLabel->setText(QStringLiteral("%1%2 Hz")
            .arg(hz >= 0 ? QStringLiteral("+") : QString{})
            .arg(hz));
    });

    // Step size model → label sync (Issue #69)
    connect(s, &SliceModel::stepHzChanged, this, [this](int hz) {
        m_stepLabel->setText(QStringLiteral("%1 Hz").arg(hz));
    });

    // NB controls (button + tuning sliders) removed from RxApplet per strict
    // Thetis parity (2026-04-22). NB state is managed via VFO flag chkNB,
    // Setup → DSP → NB/SNB, and the DSP menu bar — not here.

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
            // HL2 uses signed range (mi0bot setup.cs:16085-16086
            // [v2.10.3.13-beta2] Maximum=32, Minimum=-28); pull both bounds.
            const int maxDb = BoardCapsTable::stepAttMaxDb(
                info.boardType, caps.hasAlexFilters);
            const int minDb = caps.attenuator.minDb;
            m_stepAttSpin->setRange(minDb, maxDb);
            attCtrl->setMinAttenuation(minDb);
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

        // Helper: pick label text for the current (stepOn, autoOn) tuple.
        //
        // Inspired by mi0bot-Thetis console.cs:21342-21365 [v2.10.3.13-beta2]
        // AutoAttRX1 setter (HL2-only A-ATT label).  NereusSDR widens the
        // flip to all boards because StepAttenuatorController auto-att is
        // universal (not HL2-gated like mi0bot's is).  Without the widen,
        // non-HL2 users with auto-att enabled would have the same blind
        // spot issue #174 reported on HL2.
        //
        // Preserves "ATT" for preamp-combo mode (stepOn=false), which is
        // the existing NereusSDR convention across all boards.  mi0bot
        // pins HL2 into the step-att label family even in preamp mode,
        // but in NereusSDR HL2 users can flip to preamp mode via the
        // step-att toggle, so we keep "ATT" reachable on every board.
        auto attLabelText = [](bool stepOn, bool autoOn) {
            if (!stepOn) {
                return QStringLiteral("ATT");
            }
            return autoOn ? QStringLiteral("A-ATT")
                          : QStringLiteral("S-ATT");
        };

        auto refreshAttLabel = [this, attCtrl, attLabelText]() {
            const bool stepOn = attCtrl->stepAttEnabled();
            const bool autoOn = attCtrl->autoAttEnabled();
            m_attLabel->setText(attLabelText(stepOn, autoOn));
            m_attStack->setCurrentIndex(stepOn ? 1 : 0);
        };

        // React to step-att-enabled changes (ATT ↔ S-ATT/A-ATT mode switch)
        connect(attCtrl, &StepAttenuatorController::stepAttEnabledChanged,
                this, [refreshAttLabel](bool) { refreshAttLabel(); });

        // React to auto-att enable toggles — drives S-ATT ↔ A-ATT on HL2.
        // From mi0bot-Thetis console.cs:21342-21365 [v2.10.3.13-beta2].
        connect(attCtrl, &StepAttenuatorController::autoAttEnabledChanged,
                this, [refreshAttLabel](bool) { refreshAttLabel(); });

        // Sync initial state from controller
        refreshAttLabel();
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

        // Phase 3P-B Task 10: wire per-ADC OVL badges to overloadStatusChanged.
        // The signal is already per-ADC (index 0..2); we drive each badge
        // independently so dual-ADC boards show two discrete indicators.
        connect(attCtrl, &StepAttenuatorController::overloadStatusChanged,
                this, [this](int adcIndex, OverloadLevel level) {
            if (adcIndex < 0 || adcIndex >= 3) { return; }
            QLabel* badge = m_ovlBadges[adcIndex];
            if (!badge) { return; }  // not populated on single-ADC boards
            if (level == OverloadLevel::None) {
                badge->setStyleSheet(QStringLiteral(
                    "QLabel { background: rgba(255,255,255,0.06);"
                    " color: rgba(255,255,255,0.45);"
                    " border: 1px solid rgba(255,255,255,0.12);"
                    " border-radius: 3px; font-size: 9px; font-weight: bold;"
                    " padding: 1px 4px; }"));
            } else if (level == OverloadLevel::Yellow) {
                // §A2 one-off: #FFD700 is standard gold/yellow for ADC overload warning badge.
                // kAmberWarn (#ddbb00) is darker; these are distinct intents.
                badge->setStyleSheet(QStringLiteral(
                    "QLabel { background: rgba(255,200,0,0.20);"
                    " color: #FFD700;"  // §A2 one-off overload warning gold
                    " border: 1px solid rgba(255,200,0,0.40);"
                    " border-radius: 3px; font-size: 9px; font-weight: bold;"
                    " padding: 1px 4px; }"));
            } else {  // Red
                // §A2 one-off: #FF6868 is a soft red for ADC overload critical badge.
                // kGaugeDanger (#ff4444) is more saturated; distinct intent.
                badge->setStyleSheet(QStringLiteral(
                    "QLabel { background: rgba(255,90,90,0.25);"
                    " color: #FF6868;"  // §A2 one-off overload critical soft-red
                    " border: 1px solid rgba(255,90,90,0.50);"
                    " border-radius: 3px; font-size: 9px; font-weight: bold;"
                    " padding: 1px 4px; }"));
            }
        });
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
        // §A2 one-offs: #1a2a1a (dark green bg), #adff2f (lime border+text), #2a3a2a (hover).
        // NereusSDR-original active-AUTO palette; not in StyleConstants.
        if (m_agcAutoLabel) {
            m_agcAutoLabel->setStyleSheet(
                QStringLiteral("QPushButton { background: #1a2a1a; border: 1px solid #adff2f;"  // §A2 one-off
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
        // §A2 one-offs: same #1a1a1a/#445/#556/#adff2f as construction-time style; see above.
        if (m_agcAutoLabel) {
            m_agcAutoLabel->setStyleSheet(
                QStringLiteral("QPushButton { background: #1a1a1a; border: 1px solid #445;"  // §A2 one-off
                                "color: #556; font-size: 7px; padding: 0 3px; border-radius: 2px; }"
                                "QPushButton:hover { border-color: #adff2f; }"));
        }
        if (m_agcInfoLabel) {
            m_agcInfoLabel->hide();
        }
    }
}

// Phase 3P-F Task 4: Per-band antenna populate from AlexController.
//
// Reads the per-band RX and TX antenna assignments from AlexController and
// pushes them into SliceModel so the applet buttons reflect the active band.
// Called at construction and on every PanadapterModel::bandChanged() crossing.
//
// We update the button text directly (in addition to the SliceModel setter)
// because connectSlice() may not have been called yet at construction time —
// the ctor wires the panadapter and populates before setSlice() is called by
// the host widget (which is what drives connectSlice / signal subscriptions).
void RxApplet::populateAntennaButtons(Band band)
{
    if (!m_model || !m_slice) { return; }

    const AlexController& alex = m_model->alexController();

    // RX antenna: AlexController stores 1/2/3; map to "ANT1"/"ANT2"/"ANT3".
    const int rxNum = alex.rxAnt(band);
    const QString rxLabel = QStringLiteral("ANT") + QString::number(rxNum);

    // TX antenna: same mapping. setTxAnt() honours blockTxAnt2/3 safety guards.
    const int txNum = alex.txAnt(band);
    const QString txLabel = QStringLiteral("ANT") + QString::number(txNum);

    // Update button text directly — bypasses the connectSlice() signal chain
    // so the change is immediate regardless of whether the slice is "connected".
    m_rxAntBtn->setText(rxLabel);
    m_txAntBtn->setText(txLabel);

    // Also push into SliceModel so the model stays in sync with the UI.
    // Use setters only when the value actually differs to avoid spurious signals.
    if (m_slice->rxAntenna() != rxLabel) {
        m_slice->setRxAntenna(rxLabel);
    }
    if (m_slice->txAntenna() != txLabel) {
        m_slice->setTxAntenna(txLabel);
    }
}

#ifdef NEREUS_BUILD_TESTS
int RxApplet::stepAttMaxForTest() const
{
    return m_stepAttSpin ? m_stepAttSpin->maximum() : -1;
}

int RxApplet::visibleOvlBadgeCountForTest() const
{
    int count = 0;
    for (int i = 0; i < 3; ++i) {
        if (m_ovlBadges[i]) { ++count; }
    }
    return count;
}

int RxApplet::preampComboItemCountForTest() const
{
    return m_preampCombo ? m_preampCombo->count() : -1;
}

// Phase 3P-F Task 4: parse ANT<n> label from the button text and return n.
// Returns -1 if the button is null or the label does not match "ANT[123]".
int RxApplet::activeRxAntennaForTest() const
{
    if (!m_rxAntBtn) { return -1; }
    const QString text = m_rxAntBtn->text();
    if (text == QStringLiteral("ANT1")) { return 1; }
    if (text == QStringLiteral("ANT2")) { return 2; }
    if (text == QStringLiteral("ANT3")) { return 3; }
    return -1;
}

int RxApplet::activeTxAntennaForTest() const
{
    if (!m_txAntBtn) { return -1; }
    const QString text = m_txAntBtn->text();
    if (text == QStringLiteral("ANT1")) { return 1; }
    if (text == QStringLiteral("ANT2")) { return 2; }
    if (text == QStringLiteral("ANT3")) { return 3; }
    return -1;
}

QString RxApplet::attLabelTextForTest() const
{
    return m_attLabel ? m_attLabel->text() : QString();
}
#endif

} // namespace NereusSDR
