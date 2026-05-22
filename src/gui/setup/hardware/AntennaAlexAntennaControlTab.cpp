// =================================================================
// src/gui/setup/hardware/AntennaAlexAntennaControlTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.designer.cs (~lines 5981-7000+,
//     grpAlexAntCtrl + panelAlexTXAntControl + panelAlexRXAntControl)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Sub-sub-tab under Hardware → Antenna/ALEX.
//                Per-band antenna assignment + Block-TX safety; backed
//                by AlexController model (Phase 3P-F Task 1).
// =================================================================

//=================================================================
// setup.designer.cs
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
//
// === Verbatim Thetis Console/setup.designer.cs header (lines 1-50) ===
// namespace Thetis { using System.Windows.Forms; partial class Setup {
//   #region Windows Form Designer generated code
//   private void InitializeComponent() {
//     this.components = new System.ComponentModel.Container();
//     System.Windows.Forms.TabPage tpAlexAntCtrl;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS3;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS4;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS6;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS9;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS10;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS12;
//     System.ComponentModel.ComponentResourceManager resources = ...;
//     this.chkForceATTwhenOutPowerChanges_decreased = new CheckBoxTS();
//     this.chkUndoAutoATTTx = new CheckBoxTS();
//     this.chkAutoATTTXPsOff = new CheckBoxTS();
//     this.lblTXattBand = new LabelTS();
//     this.chkForceATTwhenOutPowerChanges = new CheckBoxTS();
//     this.chkForceATTwhenPSAoff = new CheckBoxTS();
//     this.chkEnableXVTRHF = new CheckBoxTS();
//     this.chkBPF2Gnd = new CheckBoxTS();
//     this.chkDisableRXOut = new CheckBoxTS();
//     this.chkEXT2OutOnTx = new CheckBoxTS();
//     this.chkEXT1OutOnTx = new CheckBoxTS();
//     this.labelATTOnTX = new LabelTS();
// =================================================================

#include "AntennaAlexAntennaControlTab.h"

#include "core/accessories/AlexController.h"
#include "core/SkuUiProfile.h"
#include "core/HardwareProfile.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"
#include "models/Band.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace NereusSDR {

// ── Constructor ───────────────────────────────────────────────────────────────

AntennaAlexAntennaControlTab::AntennaAlexAntennaControlTab(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_alex(&model->alexControllerMutable())
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(6);

    // Row 1: Block-TX safety strip
    buildBlockTxStrip(outerLayout);

    // Row 2: TX + RX grids side-by-side, inside a scroll area so they don't
    // overflow when the window is short.
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* scrollContents = new QWidget(scrollArea);
    auto* gridRow = new QHBoxLayout(scrollContents);
    gridRow->setContentsMargins(0, 0, 0, 0);
    gridRow->setSpacing(12);

    buildTxGrid(gridRow);   // pass QHBoxLayout* — overloaded via QBoxLayout*
    buildRxGrid(gridRow);

    scrollArea->setWidget(scrollContents);
    outerLayout->addWidget(scrollArea, 1);

    // Row 3: TX-bypass checkboxes (Phase 3P-I-b T7).
    // Source: Thetis setup.cs:6174-6300 [v2.10.3.13 @501e3f5].
    buildTxBypassStrip(outerLayout);

    // Initial SKU sync — applies to current model state.
    applySkuProfile();

    // ── Connect model → UI ────────────────────────────────────────────────────
    connect(m_alex, &AlexController::antennaChanged,
            this, &AntennaAlexAntennaControlTab::onAntennaChanged);
    connect(m_alex, &AlexController::blockTxChanged,
            this, &AntennaAlexAntennaControlTab::onBlockTxChanged);

    // Re-sync when the connected radio changes (may switch HPSDRModel).
    connect(m_model, &RadioModel::currentRadioChanged,
            this, [this](const NereusSDR::RadioInfo&) { applySkuProfile(); });
}

// ── buildBlockTxStrip ─────────────────────────────────────────────────────────
//
// Source: Thetis grpAlexAntCtrl — chkAlexBlockTxAnt2 / chkAlexBlockTxAnt3
// (setup.designer.cs:5981-6001) [@501e3f5]
//
// NereusSDR addition: these Block-TX flags are not in Thetis's setup designer;
// they map to AlexController::blockTxAnt2/3 (Phase 3P-F Task 1 NereusSDR spin).

void AntennaAlexAntennaControlTab::buildBlockTxStrip(QVBoxLayout* outerLayout)
{
    auto* frame = new QFrame(this);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setStyleSheet(QStringLiteral(
        "QFrame { background-color: rgba(200,50,50,0.08); border: 1px solid rgba(200,50,50,0.4); border-radius: 4px; }"));

    auto* row = new QHBoxLayout(frame);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(16);

    m_blockTxAnt2 = new QCheckBox(tr("Block TX on Ant 2"), frame);
    m_blockTxAnt2->setChecked(m_alex->blockTxAnt2());
    m_blockTxAnt2->setToolTip(tr("Prevents transmit assignments to Antenna Port 2. "
                                  "Use when Ant 2 is wired for receive only."));
    row->addWidget(m_blockTxAnt2);

    m_blockTxAnt3 = new QCheckBox(tr("Block TX on Ant 3"), frame);
    m_blockTxAnt3->setChecked(m_alex->blockTxAnt3());
    m_blockTxAnt3->setToolTip(tr("Prevents transmit assignments to Antenna Port 3. "
                                  "Use when Ant 3 is wired for receive only."));
    row->addWidget(m_blockTxAnt3);

    auto* note = new QLabel(tr("<i>Safety: prevent transmit into RX-only / receive-loop antennas.</i>"), frame);
    note->setWordWrap(false);
    row->addWidget(note);
    row->addStretch();

    outerLayout->addWidget(frame);

    // ── Wire Block-TX checkboxes → controller ─────────────────────────────────
    connect(m_blockTxAnt2, &QCheckBox::toggled, this, [this](bool checked) {
        m_alex->setBlockTxAnt2(checked);
    });
    connect(m_blockTxAnt3, &QCheckBox::toggled, this, [this](bool checked) {
        m_alex->setBlockTxAnt3(checked);
    });
}

// ── buildTxGrid ───────────────────────────────────────────────────────────────
//
// Source: Thetis panelAlexTXAntControl (setup.designer.cs:~6002-6400) [@501e3f5]
// 14 rows (one per Band) × 3 radio buttons (Ant 1 / Ant 2 / Ant 3).

void AntennaAlexAntennaControlTab::buildTxGrid(QBoxLayout* outerLayout)
{
    auto* grp = new QGroupBox(tr("TX Antenna per Band"), this);
    auto* layout = new QVBoxLayout(grp);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(2);

    // Header row
    auto* hdrRow = new QHBoxLayout();
    hdrRow->setContentsMargins(0, 0, 0, 0);
    auto* hdrBand = new QLabel(tr("Band"), grp);
    hdrBand->setFixedWidth(48);
    hdrRow->addWidget(hdrBand);
    for (const char* lbl : {"Ant 1", "Ant 2", "Ant 3"}) {
        auto* h = new QLabel(tr(lbl), grp);
        h->setAlignment(Qt::AlignCenter);
        hdrRow->addWidget(h, 1);
    }
    layout->addLayout(hdrRow);

    // Band rows
    for (int b = 0; b < kBandCount; ++b) {
        auto band = static_cast<Band>(b);
        auto* rowLayout = new QHBoxLayout();
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        auto* bandLbl = new QLabel(bandLabel(band), grp);
        bandLbl->setFixedWidth(48);
        rowLayout->addWidget(bandLbl);

        auto* grpBtn = new QButtonGroup(this);
        m_txGroups[b] = grpBtn;

        const int currentAnt = m_alex->txAnt(band);  // 1-based

        for (int a = 0; a < 3; ++a) {
            auto* rb = new QRadioButton(grp);
            rb->setChecked((a + 1) == currentAnt);
            rb->setToolTip(tr("TX Ant %1 for %2").arg(a + 1).arg(bandLabel(band)));
            grpBtn->addButton(rb, a + 1);  // button id = 1-based ant number
            m_txButtons[b][a] = rb;
            rowLayout->addWidget(rb, 1, Qt::AlignCenter);

            // Wire to controller
            connect(rb, &QRadioButton::toggled, this, [this, band, antNum = a + 1](bool checked) {
                if (!checked) { return; }
                m_alex->setTxAnt(band, antNum);
            });
        }
        layout->addLayout(rowLayout);
    }

    layout->addStretch();
    outerLayout->addWidget(grp, 1);

    // Apply initial blocked state
    updateTxBlockedStates();
}

// ── buildRxGrid ───────────────────────────────────────────────────────────────
//
// Source: Thetis panelAlexRXAntControl (setup.designer.cs:~6401-7000) [@501e3f5]
// 14 rows × 6 radio buttons: RX1 (1/2/3) + visual separator + RX-only (1/2/3).

void AntennaAlexAntennaControlTab::buildRxGrid(QBoxLayout* outerLayout)
{
    auto* grp = new QGroupBox(tr("RX1 / RX2 Antenna per Band"), this);
    auto* layout = new QVBoxLayout(grp);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(2);

    // Header row
    auto* hdrRow = new QHBoxLayout();
    hdrRow->setContentsMargins(0, 0, 0, 0);
    auto* hdrBand = new QLabel(tr("Band"), grp);
    hdrBand->setFixedWidth(48);
    hdrRow->addWidget(hdrBand);
    // RX1 sub-header
    auto* rx1Hdr = new QLabel(tr("RX1"), grp);
    rx1Hdr->setAlignment(Qt::AlignCenter);
    hdrRow->addWidget(rx1Hdr, 3);
    // separator
    auto* sep = new QFrame(grp);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    hdrRow->addWidget(sep);
    // RX-only sub-header
    auto* rxOnlyHdr = new QLabel(tr("RX-only"), grp);
    rxOnlyHdr->setAlignment(Qt::AlignCenter);
    hdrRow->addWidget(rxOnlyHdr, 3);
    layout->addLayout(hdrRow);

    // Second header row with individual ant labels. Store RX-only column
    // labels so they can be retargeted per SKU (SkuUiProfile.rxOnlyLabels).
    auto* hdrRow2 = new QHBoxLayout();
    hdrRow2->setContentsMargins(0, 0, 0, 0);
    hdrRow2->addSpacing(48);  // band label space
    // RX1 column: always "1" / "2" / "3" — these stay generic.
    for (const char* lbl : {"1", "2", "3"}) {
        auto* h = new QLabel(tr(lbl), grp);
        h->setAlignment(Qt::AlignCenter);
        hdrRow2->addWidget(h, 1);
    }
    auto* sep2 = new QFrame(grp);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFrameShadow(QFrame::Sunken);
    hdrRow2->addWidget(sep2);
    // RX-only column: SKU-specific. applySkuProfile() fills these.
    for (int i = 0; i < 3; ++i) {
        auto* h = new QLabel(grp);           // text set later by applySkuProfile()
        h->setAlignment(Qt::AlignCenter);
        hdrRow2->addWidget(h, 1);
        m_rxOnlyColumnLabels[i] = h;
    }
    layout->addLayout(hdrRow2);

    // Band rows
    for (int b = 0; b < kBandCount; ++b) {
        auto band = static_cast<Band>(b);
        auto* rowLayout = new QHBoxLayout();
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        auto* bandLbl = new QLabel(bandLabel(band), grp);
        bandLbl->setFixedWidth(48);
        rowLayout->addWidget(bandLbl);

        // RX1 group
        auto* rx1Grp = new QButtonGroup(this);
        m_rx1Groups[b] = rx1Grp;
        const int currentRx1 = m_alex->rxAnt(band);  // 1-based
        for (int a = 0; a < 3; ++a) {
            auto* rb = new QRadioButton(grp);
            rb->setChecked((a + 1) == currentRx1);
            rb->setToolTip(tr("RX1 Ant %1 for %2").arg(a + 1).arg(bandLabel(band)));
            rx1Grp->addButton(rb, a + 1);
            m_rx1Buttons[b][a] = rb;
            rowLayout->addWidget(rb, 1, Qt::AlignCenter);

            connect(rb, &QRadioButton::toggled, this, [this, band, antNum = a + 1](bool checked) {
                if (!checked) { return; }
                m_alex->setRxAnt(band, antNum);
            });
        }

        // Visual separator
        auto* rowSep = new QFrame(grp);
        rowSep->setFrameShape(QFrame::VLine);
        rowSep->setFrameShadow(QFrame::Sunken);
        rowLayout->addWidget(rowSep);

        // RX-only group
        auto* rxOnlyGrp = new QButtonGroup(this);
        m_rxOnlyGroups[b] = rxOnlyGrp;
        const int currentRxOnly = m_alex->rxOnlyAnt(band);  // 1-based
        for (int a = 0; a < 3; ++a) {
            auto* rb = new QRadioButton(grp);
            rb->setChecked((a + 1) == currentRxOnly);
            // Tooltip will be set by applySkuProfile() with SKU-specific label.
            rxOnlyGrp->addButton(rb, a + 1);
            m_rxOnlyButtons[b][a] = rb;
            rowLayout->addWidget(rb, 1, Qt::AlignCenter);

            connect(rb, &QRadioButton::toggled, this, [this, band, antNum = a + 1](bool checked) {
                if (!checked) { return; }
                m_alex->setRxOnlyAnt(band, antNum);
            });
        }

        layout->addLayout(rowLayout);
    }

    layout->addStretch();
    outerLayout->addWidget(grp, 2);
}

// ── controller accessor ───────────────────────────────────────────────────────

AlexController& AntennaAlexAntennaControlTab::controller()
{
    return *m_alex;
}

// ── slots ─────────────────────────────────────────────────────────────────────

void AntennaAlexAntennaControlTab::onAntennaChanged(Band band)
{
    const int row = static_cast<int>(band);
    if (row < 0 || row >= kBandCount) { return; }
    syncTxRow(row);
    syncRxRow(row);
}

void AntennaAlexAntennaControlTab::onBlockTxChanged()
{
    // Sync Block-TX checkbox states from controller
    {
        QSignalBlocker b2(m_blockTxAnt2);
        m_blockTxAnt2->setChecked(m_alex->blockTxAnt2());
    }
    {
        QSignalBlocker b3(m_blockTxAnt3);
        m_blockTxAnt3->setChecked(m_alex->blockTxAnt3());
    }
    updateTxBlockedStates();
}

// ── private helpers ───────────────────────────────────────────────────────────

void AntennaAlexAntennaControlTab::syncTxRow(int row)
{
    auto band = static_cast<Band>(row);
    const int currentAnt = m_alex->txAnt(band);  // 1-based
    for (int a = 0; a < 3; ++a) {
        if (auto* rb = m_txButtons[row][a]) {
            QSignalBlocker sb(rb);
            rb->setChecked((a + 1) == currentAnt);
        }
    }
}

void AntennaAlexAntennaControlTab::syncRxRow(int row)
{
    auto band = static_cast<Band>(row);
    const int currentRx1     = m_alex->rxAnt(band);
    const int currentRxOnly  = m_alex->rxOnlyAnt(band);
    for (int a = 0; a < 3; ++a) {
        if (auto* rb = m_rx1Buttons[row][a]) {
            QSignalBlocker sb(rb);
            rb->setChecked((a + 1) == currentRx1);
        }
        if (auto* rb = m_rxOnlyButtons[row][a]) {
            QSignalBlocker sb(rb);
            rb->setChecked((a + 1) == currentRxOnly);
        }
    }
}

void AntennaAlexAntennaControlTab::updateTxBlockedStates()
{
    // When a TX port is blocked, grey out (disable) that column's radio buttons
    // for all bands so the user cannot select a blocked port.
    // Port columns: a == 0 → Ant 1, a == 1 → Ant 2, a == 2 → Ant 3.
    const bool blk2 = m_alex->blockTxAnt2();
    const bool blk3 = m_alex->blockTxAnt3();

    for (int b = 0; b < kBandCount; ++b) {
        if (auto* rb = m_txButtons[b][1]) { rb->setEnabled(!blk2); }  // Ant 2
        if (auto* rb = m_txButtons[b][2]) { rb->setEnabled(!blk3); }  // Ant 3
    }
}

// ── buildTxBypassStrip ───────────────────────────────────────────────────────
//
// Source: Thetis setup.cs:6174-6300 per-SKU visibility, setup.cs:19832-20405
// per-SKU label strings, setup.cs:15420-16505 handlers [v2.10.3.13 @501e3f5].
//
// Renders 5 checkboxes below the per-band grid. Individual visibility is
// driven by SkuUiProfile (set via applySkuProfile on model-change). The
// mutual-exclusion trio (RxOutOnTx / Ext1OutOnTx / Ext2OutOnTx) is handled
// model-side — AlexController clears the other two when any is set.

void AntennaAlexAntennaControlTab::buildTxBypassStrip(QVBoxLayout* outerLayout)
{
    auto* frame = new QFrame(this);
    frame->setFrameShape(QFrame::StyledPanel);
    auto* row = new QHBoxLayout(frame);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(12);

    m_chkRxOutOnTx     = new QCheckBox(tr("RX Bypass on TX"), frame);
    m_chkExt1OutOnTx   = new QCheckBox(tr("Ext 1 on TX"), frame);
    m_chkExt2OutOnTx   = new QCheckBox(tr("Ext 2 on TX"), frame);
    m_chkRxOutOverride = new QCheckBox(tr("Disable RX Bypass relay"), frame);
    m_chkUseTxAntForRx = new QCheckBox(tr("Use TX antenna for RX"), frame);

    // Tooltips — From Thetis setup.cs:6178/6198 [v2.10.3.13 @501e3f5].
    // SKU-specific tooltip for chkEXT2OutOnTx is picked in applySkuProfile().
    m_chkRxOutOnTx->setToolTip(tr("Enable RX Bypass Out relay on transmit."));
    m_chkExt1OutOnTx->setToolTip(tr("Route Ext 1 to receive path during transmit."));
    m_chkRxOutOverride->setToolTip(tr("Disable the RX Bypass Out relay (chkDisableRXOut in Thetis)."));
    m_chkUseTxAntForRx->setToolTip(tr("Use the TX antenna for RX instead of the RX antenna."));

    // Initialize state from controller.
    m_chkRxOutOnTx->setChecked(m_alex->rxOutOnTx());
    m_chkExt1OutOnTx->setChecked(m_alex->ext1OutOnTx());
    m_chkExt2OutOnTx->setChecked(m_alex->ext2OutOnTx());
    m_chkRxOutOverride->setChecked(m_alex->rxOutOverride());
    m_chkUseTxAntForRx->setChecked(m_alex->useTxAntForRx());

    row->addWidget(m_chkRxOutOnTx);
    row->addWidget(m_chkExt1OutOnTx);
    row->addWidget(m_chkExt2OutOnTx);
    row->addWidget(m_chkRxOutOverride);
    row->addWidget(m_chkUseTxAntForRx);
    row->addStretch();

    outerLayout->addWidget(frame);

    // UI → model
    connect(m_chkRxOutOnTx,     &QCheckBox::toggled, m_alex, &AlexController::setRxOutOnTx);
    connect(m_chkExt1OutOnTx,   &QCheckBox::toggled, m_alex, &AlexController::setExt1OutOnTx);
    connect(m_chkExt2OutOnTx,   &QCheckBox::toggled, m_alex, &AlexController::setExt2OutOnTx);
    connect(m_chkRxOutOverride, &QCheckBox::toggled, m_alex, &AlexController::setRxOutOverride);
    connect(m_chkUseTxAntForRx, &QCheckBox::toggled, m_alex, &AlexController::setUseTxAntForRx);

    // Model → UI (so mutual-exclusion clears reflect back into UI)
    connect(m_alex, &AlexController::rxOutOnTxChanged, this, [this](bool on) {
        QSignalBlocker b(m_chkRxOutOnTx); m_chkRxOutOnTx->setChecked(on);
    });
    connect(m_alex, &AlexController::ext1OutOnTxChanged, this, [this](bool on) {
        QSignalBlocker b(m_chkExt1OutOnTx); m_chkExt1OutOnTx->setChecked(on);
    });
    connect(m_alex, &AlexController::ext2OutOnTxChanged, this, [this](bool on) {
        QSignalBlocker b(m_chkExt2OutOnTx); m_chkExt2OutOnTx->setChecked(on);
    });
    connect(m_alex, &AlexController::rxOutOverrideChanged, this, [this](bool on) {
        QSignalBlocker b(m_chkRxOutOverride); m_chkRxOutOverride->setChecked(on);
    });
    connect(m_alex, &AlexController::useTxAntForRxChanged, this, [this](bool on) {
        QSignalBlocker b(m_chkUseTxAntForRx); m_chkUseTxAntForRx->setChecked(on);
    });
}

// ── applySkuProfile ──────────────────────────────────────────────────────────
//
// Source: Thetis setup.cs:6174-6300 + 19832-20405 [v2.10.3.13 @501e3f5].
// Called on construction and whenever RadioModel::currentRadioChanged fires.

void AntennaAlexAntennaControlTab::applySkuProfile()
{
    const HPSDRModel sku = m_model->hardwareProfile().model;
    const SkuUiProfile profile = skuUiProfileFor(sku);

    // RX-only column headers (the three "1/2/3" sub-labels become RX1/RX2/XVTR
    // or EXT2/EXT1/XVTR or BYPS/EXT1/XVTR per SKU).
    for (int i = 0; i < 3; ++i) {
        if (m_rxOnlyColumnLabels[i]) {
            m_rxOnlyColumnLabels[i]->setText(profile.rxOnlyLabels[i]);
        }
    }

    // RX-only radio-button tooltips — re-bind to SKU label.
    for (int b = 0; b < kBandCount; ++b) {
        const auto band = static_cast<Band>(b);
        for (int a = 0; a < 3; ++a) {
            if (auto* rb = m_rxOnlyButtons[b][a]) {
                rb->setToolTip(tr("RX-only %1 for %2")
                                   .arg(profile.rxOnlyLabels[a])
                                   .arg(bandLabel(band)));
            }
        }
    }

    // Checkbox visibility (gate on SkuUiProfile).
    if (m_chkRxOutOnTx)     { m_chkRxOutOnTx->setVisible(profile.hasRxOutOnTx); }
    if (m_chkExt1OutOnTx)   { m_chkExt1OutOnTx->setVisible(profile.hasExt1OutOnTx); }
    if (m_chkExt2OutOnTx)   { m_chkExt2OutOnTx->setVisible(profile.hasExt2OutOnTx); }
    if (m_chkRxOutOverride) { m_chkRxOutOverride->setVisible(profile.hasRxBypassUi); }
    // useTxAntForRx is always visible — it's a NereusSDR-native control that
    // maps to Thetis Alex.cs:66 TRxAnt (no per-SKU visibility override in Thetis).

    // Per-SKU EXT1/EXT2-on-TX button text — From Thetis setup.cs:19928-19929
    // [v2.10.3.15] — e.g. G2E re-labels chkEXT2OutOnTx to "Rx BYPASS on Tx".
    // //N1GP G2E added
    if (m_chkExt1OutOnTx) { m_chkExt1OutOnTx->setText(profile.ext1OutOnTxLabel); }
    if (m_chkExt2OutOnTx) { m_chkExt2OutOnTx->setText(profile.ext2OutOnTxLabel); }

    // SKU-specific tooltip for Ext2OutOnTx — From Thetis setup.cs:6178/6198
    // [v2.10.3.13 @501e3f5].
    if (m_chkExt2OutOnTx) {
        const bool isAnan7000Family =
            (sku == HPSDRModel::ANAN7000D)      ||
            (sku == HPSDRModel::ANAN8000D)      ||
            (sku == HPSDRModel::ANAN_G2)        ||
            (sku == HPSDRModel::ANAN_G2_1K)     ||
            (sku == HPSDRModel::ANVELINAPRO3)   ||
            (sku == HPSDRModel::REDPITAYA);
        m_chkExt2OutOnTx->setToolTip(isAnan7000Family
            ? tr("Enable RX Bypass during transmit.")
            : tr("Enable RX 1 IN on Alex or Ext 2 on ANAN during transmit."));
    }
}

} // namespace NereusSDR
