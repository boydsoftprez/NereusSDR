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
//   2026-09-23 - R-R3-46: a remote window reads and writes the Core's
//                 receive antennas through the `alexAntennas` object; the transmit
//                 half follows the transmit permission. J.J. Boyd (KG4VCF), AI-
//                 assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-46 fix wave: re-reads the Core's antennas when a band
//                 edit does not take. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the Conflict policy group is hidden until the
//                 policy is read (UnbuiltFeatures).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 / R-R3-46 (parity Task 12): a remote window's TX
//                 antenna grid, Block-TX switches and TX relay switches write
//                 the Core's `alexAntennas` object and follow whether the
//                 Core takes them; no on-air rule in either window, as in
//                 Thetis. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-28 - Named native table groups, headers, rows and cells for
//                 closed version-6 Setup description parity. J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
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
#include "HardwareTransmitGate.h"
#include "gui/UnbuiltFeatures.h"

#include "core/accessories/AlexController.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/AppSettings.h"
#include "core/SkuUiProfile.h"
#include "core/HardwareProfile.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"
#include "models/Band.h"

#include <QAbstractButton>
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

namespace {
// The rows, top to bottom: 160m .. 6m, 2m, GEN, WWV, XVTR. 2 m keeps its
// own antennas, as in Thetis (Alex.cs:56-58, B160M .. B2M [v2.10.3.15];
// setup.cs:13679 ProcessAlexAntCheckBox(sender, Band.B2M)), and sits after
// 6 m as Thetis orders its bands (R-IOS-26). The button arrays are indexed
// by the per-band state slot (Band.h).
constexpr std::array<Band, kPerBandStateCount> kRowOrder{
    Band::Band160m, Band::Band80m, Band::Band60m, Band::Band40m, Band::Band30m,
    Band::Band20m,  Band::Band17m, Band::Band15m, Band::Band12m, Band::Band10m,
    Band::Band6m,   Band::Band2m,  Band::GEN,     Band::WWV,     Band::XVTR,
};
} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

AntennaAlexAntennaControlTab::AntennaAlexAntennaControlTab(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_alex(&model->alexControllerMutable())
    , m_remoteAlex(model->ownsLocalDsp() ? nullptr : model->alexAntennaFacade())
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

    // Row 4: Antenna conflict policy (Phase 3F Sub-Epic E Tasks 11-13).
    // NereusSDR-original; persisted operator preference for how add-slice
    // antenna conflicts are resolved. Consumer wire-up (RadioModel) lands
    // when the antenna-auto-switch pipeline is built.
    buildConflictPolicyGroup(outerLayout);

    // Initial SKU sync — applies to current model state.
    applySkuProfile();

    // ── Connect model → UI ────────────────────────────────────────────────────
    if (m_remoteAlex) {
        // R-R3-46: a remote window shows the Core's antennas.
        const auto resync = [this]() { syncAllFromSource(); };
        connect(m_remoteAlex, &AlexAntennaFacade::rxAntennasChanged, this, resync);
        connect(m_remoteAlex, &AlexAntennaFacade::rxOnlyAntennasChanged, this, resync);
        connect(m_remoteAlex, &AlexAntennaFacade::txAntennasChanged, this, resync);
        // A band edit that did not take leaves the Core's values: re-read
        // them over the click.
        connect(m_remoteAlex, &AlexAntennaFacade::bandEditRefused, this, resync);
        connect(m_remoteAlex, &AlexAntennaFacade::blockTxAnt2Changed, this,
                &AntennaAlexAntennaControlTab::onBlockTxChanged);
        connect(m_remoteAlex, &AlexAntennaFacade::blockTxAnt3Changed, this,
                &AntennaAlexAntennaControlTab::onBlockTxChanged);
        // Parity Task 12: the transmit half is live while the Core takes it.
        connect(m_remoteAlex, &AlexAntennaFacade::transmitEditAvailabilityChanged, this,
                &AntennaAlexAntennaControlTab::applyTransmitEditAvailability);
        applyTransmitEditAvailability();
    } else {
        connect(m_alex, &AlexController::antennaChanged,
                this, &AntennaAlexAntennaControlTab::onAntennaChanged);
        connect(m_alex, &AlexController::blockTxChanged,
                this, &AntennaAlexAntennaControlTab::onBlockTxChanged);
    }

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
    m_blockTxFrame = frame;
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setStyleSheet(QStringLiteral(
        "QFrame { background-color: rgba(200,50,50,0.08); border: 1px solid rgba(200,50,50,0.4); border-radius: 4px; }"));

    auto* row = new QHBoxLayout(frame);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(16);

    m_blockTxAnt2 = new QCheckBox(tr("Block TX on Ant 2"), frame);
    m_blockTxAnt2->setProperty("nereusSetupId", "hardware.antennaAlex.blockTxAnt2");
    m_blockTxAnt2->setChecked(blockTxAnt2Now());
    m_blockTxAnt2->setToolTip(tr("Prevents transmit assignments to Antenna Port 2. "
                                  "Use when Ant 2 is wired for receive only."));
    row->addWidget(m_blockTxAnt2);

    m_blockTxAnt3 = new QCheckBox(tr("Block TX on Ant 3"), frame);
    m_blockTxAnt3->setProperty("nereusSetupId", "hardware.antennaAlex.blockTxAnt3");
    m_blockTxAnt3->setChecked(blockTxAnt3Now());
    m_blockTxAnt3->setToolTip(tr("Prevents transmit assignments to Antenna Port 3. "
                                  "Use when Ant 3 is wired for receive only."));
    row->addWidget(m_blockTxAnt3);

    auto* note = new QLabel(tr("<i>Safety: prevent transmit into RX-only / receive-loop antennas.</i>"), frame);
    note->setWordWrap(false);
    row->addWidget(note);
    row->addStretch();

    outerLayout->addWidget(frame);

    // ── Wire Block-TX checkboxes → controller ─────────────────────────────────
    // Parity Task 12: in a remote window these go to the Core's controller
    // (radioHardwareVersion 6); the boxes then show the Core's values.
    connect(m_blockTxAnt2, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_remoteAlex) {
            m_remoteAlex->setBlockTxAnt2(checked);
            onBlockTxChanged();
            return;
        }
        m_alex->setBlockTxAnt2(checked);
    });
    connect(m_blockTxAnt3, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_remoteAlex) {
            m_remoteAlex->setBlockTxAnt3(checked);
            onBlockTxChanged();
            return;
        }
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
    grp->setProperty("nereusSetupId", "hardware.antenna.txRows");
    m_txGridGroup = grp;
    auto* layout = new QVBoxLayout(grp);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(2);

    // Header row
    auto* hdrRow = new QHBoxLayout();
    hdrRow->setContentsMargins(0, 0, 0, 0);
    auto* hdrBand = new QLabel(tr("Band"), grp);
    hdrBand->setFixedWidth(48);
    hdrRow->addWidget(hdrBand);
    int txColumn = 0;
    for (const char* lbl : {"Ant 1", "Ant 2", "Ant 3"}) {
        auto* h = new QLabel(tr(lbl), grp);
        h->setProperty("nereusAntennaColumn", QStringLiteral("tx%1").arg(++txColumn));
        h->setAlignment(Qt::AlignCenter);
        hdrRow->addWidget(h, 1);
    }
    layout->addLayout(hdrRow);

    // Band rows
    for (const Band band : kRowOrder) {
        const int b = perBandStateSlot(band);
        auto* rowLayout = new QHBoxLayout();
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        auto* bandLbl = new QLabel(bandLabel(band), grp);
        bandLbl->setProperty("nereusAntennaBand", static_cast<int>(band));
        bandLbl->setProperty("nereusAntennaRowLabel", true);
        bandLbl->setFixedWidth(48);
        rowLayout->addWidget(bandLbl);

        auto* grpBtn = new QButtonGroup(this);
        m_txGroups[b] = grpBtn;

        const int currentAnt = txAntOf(band);  // 1-based

        for (int a = 0; a < 3; ++a) {
            auto* rb = new QRadioButton(grp);
            rb->setProperty("nereusAntennaBand", static_cast<int>(band));
            rb->setProperty("nereusAntennaColumn", QStringLiteral("tx%1").arg(a + 1));
            rb->setChecked((a + 1) == currentAnt);
            rb->setToolTip(tr("TX Ant %1 for %2").arg(a + 1).arg(bandLabel(band)));
            grpBtn->addButton(rb, a + 1);  // button id = 1-based ant number
            m_txButtons[b][a] = rb;
            rowLayout->addWidget(rb, 1, Qt::AlignCenter);

            // Wire to controller
            connect(rb, &QRadioButton::toggled, this, [this, band, antNum = a + 1](bool checked) {
                if (!checked) { return; }
                if (m_remoteAlex) {
                    // Parity Task 12: the Core applies it; the row shows
                    // what the Core keeps.
                    m_remoteAlex->setTxAnt(band, antNum);
                    syncTxRow(perBandStateSlot(band));
                    return;
                }
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
    grp->setProperty("nereusSetupId", "hardware.antenna.rxRows");
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
    rx1Hdr->setProperty("nereusAntennaColumnGroup", "RX1");
    rx1Hdr->setAlignment(Qt::AlignCenter);
    hdrRow->addWidget(rx1Hdr, 3);
    // separator
    auto* sep = new QFrame(grp);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    hdrRow->addWidget(sep);
    // RX-only sub-header
    auto* rxOnlyHdr = new QLabel(tr("RX-only"), grp);
    rxOnlyHdr->setProperty("nereusAntennaColumnGroup", "RX-only");
    rxOnlyHdr->setAlignment(Qt::AlignCenter);
    hdrRow->addWidget(rxOnlyHdr, 3);
    layout->addLayout(hdrRow);

    // Second header row with individual ant labels. Store RX-only column
    // labels so they can be retargeted per SKU (SkuUiProfile.rxOnlyLabels).
    auto* hdrRow2 = new QHBoxLayout();
    hdrRow2->setContentsMargins(0, 0, 0, 0);
    hdrRow2->addSpacing(48);  // band label space
    // RX1 column: always "1" / "2" / "3" — these stay generic.
    int rxColumn = 0;
    for (const char* lbl : {"1", "2", "3"}) {
        auto* h = new QLabel(tr(lbl), grp);
        h->setProperty("nereusAntennaColumn", QStringLiteral("rx%1").arg(++rxColumn));
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
        h->setProperty("nereusAntennaColumn", QStringLiteral("rxOnly%1").arg(i + 1));
        h->setAlignment(Qt::AlignCenter);
        hdrRow2->addWidget(h, 1);
        m_rxOnlyColumnLabels[i] = h;
    }
    layout->addLayout(hdrRow2);

    // Band rows
    for (const Band band : kRowOrder) {
        const int b = perBandStateSlot(band);
        auto* rowLayout = new QHBoxLayout();
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        auto* bandLbl = new QLabel(bandLabel(band), grp);
        bandLbl->setProperty("nereusAntennaBand", static_cast<int>(band));
        bandLbl->setProperty("nereusAntennaRowLabel", true);
        bandLbl->setFixedWidth(48);
        rowLayout->addWidget(bandLbl);

        // RX1 group
        auto* rx1Grp = new QButtonGroup(this);
        m_rx1Groups[b] = rx1Grp;
        const int currentRx1 = rxAntOf(band);  // 1-based
        for (int a = 0; a < 3; ++a) {
            auto* rb = new QRadioButton(grp);
            rb->setProperty("nereusAntennaBand", static_cast<int>(band));
            rb->setProperty("nereusAntennaColumn", QStringLiteral("rx%1").arg(a + 1));
            rb->setChecked((a + 1) == currentRx1);
            rb->setToolTip(tr("RX1 Ant %1 for %2").arg(a + 1).arg(bandLabel(band)));
            rx1Grp->addButton(rb, a + 1);
            m_rx1Buttons[b][a] = rb;
            rowLayout->addWidget(rb, 1, Qt::AlignCenter);

            connect(rb, &QRadioButton::toggled, this, [this, band, antNum = a + 1](bool checked) {
                if (!checked) { return; }
                if (m_remoteAlex) {
                    // R-R3-46: the Core applies it; the row shows what the
                    // Core keeps (or keeps its value when the edit is refused).
                    m_remoteAlex->setRxAnt(band, antNum);
                    syncRxRow(perBandStateSlot(band));
                    return;
                }
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
        const int currentRxOnly = rxOnlyAntOf(band);  // 1-based
        for (int a = 0; a < 3; ++a) {
            auto* rb = new QRadioButton(grp);
            rb->setProperty("nereusAntennaBand", static_cast<int>(band));
            rb->setProperty("nereusAntennaColumn", QStringLiteral("rxOnly%1").arg(a + 1));
            rb->setChecked((a + 1) == currentRxOnly);
            // Tooltip will be set by applySkuProfile() with SKU-specific label.
            rxOnlyGrp->addButton(rb, a + 1);
            m_rxOnlyButtons[b][a] = rb;
            rowLayout->addWidget(rb, 1, Qt::AlignCenter);

            connect(rb, &QRadioButton::toggled, this, [this, band, antNum = a + 1](bool checked) {
                if (!checked) { return; }
                if (m_remoteAlex) {
                    m_remoteAlex->setRxOnlyAnt(band, antNum);
                    syncRxRow(perBandStateSlot(band));
                    return;
                }
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
    const int row = perBandStateSlot(band);
    if (row < 0 || row >= kBandCount) { return; }
    syncTxRow(row);
    syncRxRow(row);
}

void AntennaAlexAntennaControlTab::onBlockTxChanged()
{
    // Sync Block-TX checkbox states from controller
    {
        QSignalBlocker b2(m_blockTxAnt2);
        m_blockTxAnt2->setChecked(blockTxAnt2Now());
    }
    {
        QSignalBlocker b3(m_blockTxAnt3);
        m_blockTxAnt3->setChecked(blockTxAnt3Now());
    }
    updateTxBlockedStates();
}

// ── private helpers ───────────────────────────────────────────────────────────

void AntennaAlexAntennaControlTab::syncTxRow(int row)
{
    auto band = bandFromPerBandStateSlot(row);
    const int currentAnt = txAntOf(band);  // 1-based
    for (int a = 0; a < 3; ++a) {
        if (auto* rb = m_txButtons[row][a]) {
            QSignalBlocker sb(rb);
            rb->setChecked((a + 1) == currentAnt);
        }
    }
}

void AntennaAlexAntennaControlTab::syncRxRow(int row)
{
    auto band = bandFromPerBandStateSlot(row);
    const int currentRx1     = rxAntOf(band);
    const int currentRxOnly  = rxOnlyAntOf(band);
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
    const bool blk2 = blockTxAnt2Now();
    const bool blk3 = blockTxAnt3Now();

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
    m_chkRxOutOnTx->setProperty("nereusSetupId", "hardware.antennaAlex.rxOutOnTx");
    m_chkExt1OutOnTx->setProperty("nereusSetupId", "hardware.antennaAlex.ext1OutOnTx");
    m_chkExt2OutOnTx->setProperty("nereusSetupId", "hardware.antennaAlex.ext2OutOnTx");
    m_chkRxOutOverride->setProperty("nereusSetupId", "hardware.antennaAlex.rxOutOverride");
    m_chkUseTxAntForRx->setProperty("nereusSetupId", "hardware.antennaAlex.useTxAntennaForRx");

    // Tooltips — From Thetis setup.cs:6178/6198 [v2.10.3.13 @501e3f5].
    // SKU-specific tooltip for chkEXT2OutOnTx is picked in applySkuProfile().
    m_chkRxOutOnTx->setToolTip(tr("Enable RX Bypass Out relay on transmit."));
    m_chkExt1OutOnTx->setToolTip(tr("Route Ext 1 to receive path during transmit."));
    // The Thetis control for this relay is chkDisableRXOut.
    m_chkRxOutOverride->setToolTip(tr("Disable the RX Bypass Out relay."));
    m_chkUseTxAntForRx->setToolTip(tr("Use the TX antenna for RX instead of the RX antenna."));

    // Initialize state from controller.
    m_chkRxOutOnTx->setChecked(rxOutOnTxNow());
    m_chkExt1OutOnTx->setChecked(ext1OutOnTxNow());
    m_chkExt2OutOnTx->setChecked(ext2OutOnTxNow());
    m_chkRxOutOverride->setChecked(rxOutOverrideNow());
    m_chkUseTxAntForRx->setChecked(useTxAntForRxNow());

    row->addWidget(m_chkRxOutOnTx);
    row->addWidget(m_chkExt1OutOnTx);
    row->addWidget(m_chkExt2OutOnTx);
    row->addWidget(m_chkRxOutOverride);
    row->addWidget(m_chkUseTxAntForRx);
    row->addStretch();

    outerLayout->addWidget(frame);

    if (m_remoteAlex) {
        // R-R3-46: in a remote window "Use TX antenna for RX" goes to the
        // Core. Parity Task 12: so do the four TX relay switches (RX bypass
        // on TX from radioHardwareVersion 5, the other three from 6); each
        // box then shows the Core's values, so a switch the Core cleared
        // (Ext 1 on TX clears the other two) or refused shows that too.
        connect(m_chkUseTxAntForRx, &QCheckBox::toggled, this, [this](bool on) {
            m_remoteAlex->setUseTxAntennaForRx(on);
            QSignalBlocker b(m_chkUseTxAntForRx);
            m_chkUseTxAntForRx->setChecked(m_remoteAlex->useTxAntennaForRx());
        });
        connect(m_chkRxOutOnTx, &QCheckBox::toggled, this, [this](bool on) {
            m_remoteAlex->setRxOutOnTx(on);
            syncTxRelaysFromSource();
        });
        connect(m_chkExt1OutOnTx, &QCheckBox::toggled, this, [this](bool on) {
            m_remoteAlex->setExt1OutOnTx(on);
            syncTxRelaysFromSource();
        });
        connect(m_chkExt2OutOnTx, &QCheckBox::toggled, this, [this](bool on) {
            m_remoteAlex->setExt2OutOnTx(on);
            syncTxRelaysFromSource();
        });
        connect(m_chkRxOutOverride, &QCheckBox::toggled, this, [this](bool on) {
            m_remoteAlex->setRxOutOverride(on);
            syncTxRelaysFromSource();
        });
        const auto follow = [this](auto signal, QCheckBox* box) {
            connect(m_remoteAlex, signal, this, [box](bool on) {
                QSignalBlocker b(box);
                box->setChecked(on);
            });
        };
        follow(&AlexAntennaFacade::rxOutOnTxChanged, m_chkRxOutOnTx);
        follow(&AlexAntennaFacade::ext1OutOnTxChanged, m_chkExt1OutOnTx);
        follow(&AlexAntennaFacade::ext2OutOnTxChanged, m_chkExt2OutOnTx);
        follow(&AlexAntennaFacade::rxOutOverrideChanged, m_chkRxOutOverride);
        follow(&AlexAntennaFacade::useTxAntennaForRxChanged, m_chkUseTxAntForRx);
        return;
    }

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

// ── buildConflictPolicyGroup ─────────────────────────────────────────────────
//
// NereusSDR-original (Phase 3F Sub-Epic E Tasks 11-13). No upstream port.
// Persists a tri-state operator preference for how add-slice antenna
// conflicts are resolved against a slice already using the same chain.
//
// AppSettings key: "Antenna_ConflictPolicy" (int, default 0 = Auto).
//   0 = Auto    — resolve silently when safe, toast on RX-only switch
//   1 = Warn    — show TxBoundConfirmDialog before TX-bound re-route
//   2 = Block   — refuse add-slice when chain conflict would occur
//
// Consumer wire-up (RadioModel::addSliceOnPan reads this key before
// acting on chain conflicts) lands when the antenna-auto-switch pipeline
// is built; for now the persistence is operator-visible and round-trips.

void AntennaAlexAntennaControlTab::buildConflictPolicyGroup(QVBoxLayout* outerLayout)
{
    auto* group = new QGroupBox(tr("Conflict policy"), this);
    group->setObjectName(QStringLiteral("antennaConflictPolicyGroup"));
    // R-R3-49: nothing reads the policy yet; hidden until something does.
    // The saved Antenna_ConflictPolicy value stays as it is.
    UnbuiltFeatures::hideUnlessBuilt(group, UnbuiltFeature::AntennaConflict);
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);

    auto* hint = new QLabel(
        tr("How NereusSDR handles antenna conflicts when adding a slice "
           "that needs an antenna currently in use by another slice's chain:"),
        group);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* btnGroup = new QButtonGroup(this);
    // R-R3-17 / R-R3-21: user words. Warn is TxBoundConfirmDialog before a
    // TX-bound re-route; Block refuses add-slice on a chain conflict.
    auto* autoBtn  = new QRadioButton(tr("Auto - resolve it when safe, and show a notice when "
                                         "only a receive antenna changes"), group);
    auto* warnBtn  = new QRadioButton(tr("Warn - ask before moving the transmit antenna"), group);
    auto* blockBtn = new QRadioButton(tr("Block - do not add the slice while its antenna is "
                                         "in use by another slice"), group);
    btnGroup->addButton(autoBtn,  0);
    btnGroup->addButton(warnBtn,  1);
    btnGroup->addButton(blockBtn, 2);

    // Restore persisted choice (default = Auto/0).
    const int persisted =
        AppSettings::instance().value(QStringLiteral("Antenna_ConflictPolicy"), 0).toInt();
    if (auto* btn = btnGroup->button(persisted)) {
        btn->setChecked(true);
    } else {
        autoBtn->setChecked(true);
    }

    layout->addWidget(autoBtn);
    layout->addWidget(warnBtn);
    layout->addWidget(blockBtn);

    outerLayout->addWidget(group);

    // UI -> AppSettings (write-through on selection change).
    connect(btnGroup, &QButtonGroup::idClicked, this, [](int id) {
        AppSettings::instance().setValue(QStringLiteral("Antenna_ConflictPolicy"), id);
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
        const auto band = bandFromPerBandStateSlot(b);
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
        m_chkExt2OutOnTx->setToolTip(profile.ext2OutOnTxTooltip);
    }
}

// ── R-R3-46: remote window source; parity Task 12: its transmit half ─────────

int AntennaAlexAntennaControlTab::txAntOf(Band band) const
{
    return m_remoteAlex ? m_remoteAlex->txAnt(band) : m_alex->txAnt(band);
}

int AntennaAlexAntennaControlTab::rxAntOf(Band band) const
{
    return m_remoteAlex ? m_remoteAlex->rxAnt(band) : m_alex->rxAnt(band);
}

int AntennaAlexAntennaControlTab::rxOnlyAntOf(Band band) const
{
    return m_remoteAlex ? m_remoteAlex->rxOnlyAnt(band) : m_alex->rxOnlyAnt(band);
}

bool AntennaAlexAntennaControlTab::blockTxAnt2Now() const
{
    return m_remoteAlex ? m_remoteAlex->blockTxAnt2() : m_alex->blockTxAnt2();
}

bool AntennaAlexAntennaControlTab::blockTxAnt3Now() const
{
    return m_remoteAlex ? m_remoteAlex->blockTxAnt3() : m_alex->blockTxAnt3();
}

bool AntennaAlexAntennaControlTab::rxOutOnTxNow() const
{
    return m_remoteAlex ? m_remoteAlex->rxOutOnTx() : m_alex->rxOutOnTx();
}

bool AntennaAlexAntennaControlTab::ext1OutOnTxNow() const
{
    return m_remoteAlex ? m_remoteAlex->ext1OutOnTx() : m_alex->ext1OutOnTx();
}

bool AntennaAlexAntennaControlTab::ext2OutOnTxNow() const
{
    return m_remoteAlex ? m_remoteAlex->ext2OutOnTx() : m_alex->ext2OutOnTx();
}

bool AntennaAlexAntennaControlTab::rxOutOverrideNow() const
{
    return m_remoteAlex ? m_remoteAlex->rxOutOverride() : m_alex->rxOutOverride();
}

bool AntennaAlexAntennaControlTab::useTxAntForRxNow() const
{
    return m_remoteAlex ? m_remoteAlex->useTxAntennaForRx() : m_alex->useTxAntForRx();
}

void AntennaAlexAntennaControlTab::syncAllFromSource()
{
    for (int row = 0; row < kBandCount; ++row) {
        syncTxRow(row);
        syncRxRow(row);
    }
    onBlockTxChanged();
}

void AntennaAlexAntennaControlTab::syncTxRelaysFromSource()
{
    const auto show = [](QCheckBox* box, bool on) {
        if (!box) { return; }
        QSignalBlocker b(box);
        box->setChecked(on);
    };
    show(m_chkRxOutOnTx, rxOutOnTxNow());
    show(m_chkExt1OutOnTx, ext1OutOnTxNow());
    show(m_chkExt2OutOnTx, ext2OutOnTxNow());
    show(m_chkRxOutOverride, rxOutOverrideNow());
}

void AntennaAlexAntennaControlTab::applyTransmitEditAvailability()
{
    if (!m_remoteAlex) {
        return;
    }
    // Parity Task 12: the TX antenna grid, the Block-TX strip, Ext 1 and
    // Ext 2 on TX and the RX bypass relay override go to a Core at
    // radioHardwareVersion 6; RX bypass on TX to one at 5. "Use TX antenna
    // for RX" is a receive setting and follows the whole tab. No on-air
    // rule: Thetis applies each at once while transmitting (see
    // StationServer::radioHardwareVersion).
    const QString fallback = m_remoteAlex->windowUnavailableReason().isEmpty()
        ? tr("Connect to the Core to change the radio's hardware settings.")
        : m_remoteAlex->windowUnavailableReason();
    const bool txAntennas = m_remoteAlex->txAntennasEditable();
    const QString txReason = m_remoteAlex->txAntennasUnavailableReason().isEmpty()
        ? fallback : m_remoteAlex->txAntennasUnavailableReason();
    for (QWidget* w : std::initializer_list<QWidget*>{
             m_txGridGroup, m_blockTxFrame, m_chkExt1OutOnTx, m_chkExt2OutOnTx,
             m_chkRxOutOverride}) {
        HardwareTransmitGate::apply(w, txAntennas, txReason);
    }
    const bool rxBypass = m_remoteAlex->rxBypassEditable();
    const QString bypassReason = m_remoteAlex->rxBypassUnavailableReason().isEmpty()
        ? fallback : m_remoteAlex->rxBypassUnavailableReason();
    HardwareTransmitGate::apply(m_chkRxOutOnTx, rxBypass, bypassReason);
}

#ifdef NEREUS_BUILD_TESTS
QRadioButton* AntennaAlexAntennaControlTab::rxButtonForTest(Band band, int ant) const
{
    const int b = perBandStateSlot(band);
    return (b >= 0 && b < kBandCount && ant >= 1 && ant <= 3)
        ? m_rx1Buttons[static_cast<std::size_t>(b)][static_cast<std::size_t>(ant - 1)] : nullptr;
}

QRadioButton* AntennaAlexAntennaControlTab::rxOnlyButtonForTest(Band band, int ant) const
{
    const int b = perBandStateSlot(band);
    return (b >= 0 && b < kBandCount && ant >= 1 && ant <= 3)
        ? m_rxOnlyButtons[static_cast<std::size_t>(b)][static_cast<std::size_t>(ant - 1)] : nullptr;
}

QRadioButton* AntennaAlexAntennaControlTab::txButtonForTest(Band band, int ant) const
{
    const int b = perBandStateSlot(band);
    return (b >= 0 && b < kBandCount && ant >= 1 && ant <= 3)
        ? m_txButtons[static_cast<std::size_t>(b)][static_cast<std::size_t>(ant - 1)] : nullptr;
}
#endif

} // namespace NereusSDR
