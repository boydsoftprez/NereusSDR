// =================================================================
// src/gui/setup/hardware/AntennaAlexTab.cpp  (NereusSDR)
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
//   2026-04-20 — Refactored into parent QTabWidget hosting three sub-sub-tabs:
//                 Antenna Control (existing table content), Alex-1 Filters (Task 8),
//                 Alex-2 Filters (placeholder for Task 9). J.J. Boyd (KG4VCF).
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

#include "AntennaAlexTab.h"
#include "AntennaAlexAlex1Tab.h"

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "models/Band.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace NereusSDR {

// ── Band row labels (14-band) ─────────────────────────────────────────────────
static QStringList bandRowLabels()
{
    QStringList labels;
    labels.reserve(static_cast<int>(Band::Count));
    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        labels << bandLabel(static_cast<Band>(i));
    }
    return labels;
}

// ── Constructor ───────────────────────────────────────────────────────────────

AntennaAlexTab::AntennaAlexTab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    // Top-level layout holds the sub-tab widget that mirrors Thetis tcAlexControl.
    // Source: Thetis tcAlexControl (setup.designer.cs:23385-23395) [@501e3f5]
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_subTabs = new QTabWidget(this);
    m_subTabs->setTabPosition(QTabWidget::North);
    outerLayout->addWidget(m_subTabs);

    // ── Tab 0: Antenna Control ────────────────────────────────────────────────
    // Source: Thetis tpAlexAntCtrl. Hosts RX/TX antenna tables + relay options.
    // Phase F will replace the placeholder with full routing controls.
    m_antennaControlTab = new QWidget(m_subTabs);
    auto* antCtrlLayout = new QVBoxLayout(m_antennaControlTab);
    antCtrlLayout->setContentsMargins(8, 8, 8, 8);
    antCtrlLayout->setSpacing(6);

    // ── RX Antenna per-band grid ──────────────────────────────────────────────
    // Source: Thetis Setup.cs:13412-13423 — _AlexRxAntButtons[band][ant]
    // ANT1 / ANT2 / ANT3 per band. Exclusive selection per row.
    auto* rxGroup = new QGroupBox(tr("RX Antenna per Band"), m_antennaControlTab);
    auto* rxVBox  = new QVBoxLayout(rxGroup);

    m_rxAntTable = new QTableWidget(static_cast<int>(Band::Count), 3, rxGroup);
    m_rxAntTable->setHorizontalHeaderLabels({tr("ANT1"), tr("ANT2"), tr("ANT3")});
    m_rxAntTable->setVerticalHeaderLabels(bandRowLabels());
    m_rxAntTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_rxAntTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_rxAntTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rxAntTable->setSelectionMode(QAbstractItemView::NoSelection);

    for (int row = 0; row < static_cast<int>(Band::Count); ++row) {
        for (int col = 0; col < 3; ++col) {
            auto* item = new QTableWidgetItem();
            // First column (ANT1) checked by default — mirrors Thetis default
            item->setCheckState(col == 0 ? Qt::Checked : Qt::Unchecked);
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            m_rxAntTable->setItem(row, col, item);
        }
    }
    rxVBox->addWidget(m_rxAntTable);
    antCtrlLayout->addWidget(rxGroup);

    // ── TX Antenna per-band grid ──────────────────────────────────────────────
    // Source: Thetis Setup.cs:13425-13436 — _AlexTxAntButtons[band][ant]
    auto* txGroup = new QGroupBox(tr("TX Antenna per Band"), m_antennaControlTab);
    auto* txVBox  = new QVBoxLayout(txGroup);

    m_txAntTable = new QTableWidget(static_cast<int>(Band::Count), 3, txGroup);
    m_txAntTable->setHorizontalHeaderLabels({tr("ANT1"), tr("ANT2"), tr("ANT3")});
    m_txAntTable->setVerticalHeaderLabels(bandRowLabels());
    m_txAntTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_txAntTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_txAntTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_txAntTable->setSelectionMode(QAbstractItemView::NoSelection);

    for (int row = 0; row < static_cast<int>(Band::Count); ++row) {
        for (int col = 0; col < 3; ++col) {
            auto* item = new QTableWidgetItem();
            item->setCheckState(col == 0 ? Qt::Checked : Qt::Unchecked);
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            m_txAntTable->setItem(row, col, item);
        }
    }
    txVBox->addWidget(m_txAntTable);
    antCtrlLayout->addWidget(txGroup);

    // ── ALEX bypass / relay options ───────────────────────────────────────────
    // Source: Thetis Setup.cs:2892-2898 — chkRxOutOnTx, chkEXT1OutOnTx,
    //   chkEXT2OutOnTx, chkHFTRRelay, chkBPF2Gnd, chkEnableXVTRHF
    auto* optGroup = new QGroupBox(tr("ALEX Relay Options"), m_antennaControlTab);
    auto* optLayout = new QVBoxLayout(optGroup);

    m_rxOutOnTx   = new QCheckBox(tr("RX Out active during TX"), optGroup);
    m_ext1OutOnTx = new QCheckBox(tr("EXT1 Out active during TX"), optGroup);
    m_ext2OutOnTx = new QCheckBox(tr("EXT2 Out active during TX"), optGroup);
    m_hfTrRelay   = new QCheckBox(tr("HF T/R relay present"), optGroup);
    m_bpf2Gnd     = new QCheckBox(tr("BPF2 ground on TX"), optGroup);
    m_enableXvtrHf = new QCheckBox(tr("Enable XVTR HF path"), optGroup);
    m_enableXvtrHf->setToolTip(
        tr("Routes the XVTR signal through the HF path. "
           "Source: Thetis chkEnableXVTRHF (Setup.cs:18639)."));

    for (QCheckBox* chk : {m_rxOutOnTx, m_ext1OutOnTx, m_ext2OutOnTx,
                           m_hfTrRelay, m_bpf2Gnd, m_enableXvtrHf}) {
        optLayout->addWidget(chk);
    }
    antCtrlLayout->addWidget(optGroup);
    antCtrlLayout->addStretch();

    m_subTabs->addTab(m_antennaControlTab, tr("Antenna Control"));

    // ── Tab 1: Alex-1 Filters ─────────────────────────────────────────────────
    // Source: Thetis tpAlexFilterControl (setup.designer.cs:23399-25538) [@501e3f5]
    m_alex1Tab = new AntennaAlexAlex1Tab(model, m_subTabs);
    m_subTabs->addTab(m_alex1Tab, tr("Alex-1 Filters"));

    // Forward Alex-1 settingChanged under the "alex1/" prefix so HardwarePage
    // routes it to the correct AppSettings namespace.
    connect(m_alex1Tab, &AntennaAlexAlex1Tab::settingChanged,
            this, [this](const QString& key, const QVariant& value) {
                emit settingChanged(QStringLiteral("alex1/") + key, value);
            });

    // ── Tab 2: Alex-2 Filters (Task 9 placeholder) ────────────────────────────
    m_alex2PlaceholderTab = new QWidget(m_subTabs);
    auto* alex2Layout = new QVBoxLayout(m_alex2PlaceholderTab);
    alex2Layout->addWidget(new QLabel(
        tr("Alex-2 Filters — coming in Phase 3P-B Task 9"),
        m_alex2PlaceholderTab));
    alex2Layout->addStretch();
    m_subTabs->addTab(m_alex2PlaceholderTab, tr("Alex-2 Filters"));

    // ── Wire Antenna Control signals ──────────────────────────────────────────
    connect(m_rxAntTable, &QTableWidget::itemChanged,
            this, &AntennaAlexTab::onRxAntTableChanged);
    connect(m_txAntTable, &QTableWidget::itemChanged,
            this, &AntennaAlexTab::onTxAntTableChanged);

    auto wireCheckBox = [this](QCheckBox* chk, const QString& key) {
        connect(chk, &QCheckBox::toggled, this, [this, key](bool checked) {
            emit settingChanged(key, checked);
        });
    };
    wireCheckBox(m_rxOutOnTx,    QStringLiteral("antennaAlex/rxOutOnTx"));
    wireCheckBox(m_ext1OutOnTx,  QStringLiteral("antennaAlex/ext1OutOnTx"));
    wireCheckBox(m_ext2OutOnTx,  QStringLiteral("antennaAlex/ext2OutOnTx"));
    wireCheckBox(m_hfTrRelay,    QStringLiteral("antennaAlex/hfTrRelay"));
    wireCheckBox(m_bpf2Gnd,      QStringLiteral("antennaAlex/bpf2Gnd"));
    wireCheckBox(m_enableXvtrHf, QStringLiteral("antennaAlex/enableXvtrHf"));
}

// ── populate ──────────────────────────────────────────────────────────────────

void AntennaAlexTab::populate(const RadioInfo& info, const BoardCapabilities& caps)
{
    // If the board has no ALEX, HardwarePage hides this whole tab.
    // Defensive: if antennaInputCount < 3, disable extra antenna columns.
    // Source: Thetis Setup.cs:6185-6246 — radAlexR/T column enable per caps.
    for (int col = 0; col < 3; ++col) {
        bool colEnabled = (col < caps.antennaInputCount);
        for (int row = 0; row < static_cast<int>(Band::Count); ++row) {
            if (auto* item = m_rxAntTable->item(row, col)) {
                Qt::ItemFlags flags = Qt::ItemIsEnabled;
                if (colEnabled) {
                    flags |= Qt::ItemIsUserCheckable;
                } else {
                    item->setCheckState(Qt::Unchecked);
                }
                item->setFlags(flags);
            }
            if (auto* item = m_txAntTable->item(row, col)) {
                Qt::ItemFlags flags = Qt::ItemIsEnabled;
                if (colEnabled) {
                    flags |= Qt::ItemIsUserCheckable;
                } else {
                    item->setCheckState(Qt::Unchecked);
                }
                item->setFlags(flags);
            }
        }
    }
    // XVTR path only relevant when board has an XVTR jack
    m_enableXvtrHf->setEnabled(caps.xvtrJackCount > 0);

    // Gate Saturn BPF1 column on board type.
    // Saturn = ANAN-G2 / G2-1K (G8NJJ). SaturnMKII = MkII board revision.
    // From spec §7: "auto-hide on non-Saturn boards" [@501e3f5]
    const bool isSaturn = (caps.board == HPSDRHW::Saturn || caps.board == HPSDRHW::SaturnMKII);
    m_alex1Tab->updateBoardCapabilities(isSaturn);

    // Restore Alex-1 filter settings from per-MAC AppSettings.
    m_lastMac = info.macAddress;
    m_alex1Tab->restoreSettings(info.macAddress);
}

// ── private slots ─────────────────────────────────────────────────────────────

// Enforce exclusive selection per row (radio-button semantics).
// Source: Thetis _AlexRxAntButtons are RadioButtonTS — mutually exclusive per band.
void AntennaAlexTab::onRxAntTableChanged(QTableWidgetItem* changed)
{
    if (m_updating) { return; }
    if (!changed || changed->checkState() != Qt::Checked) { return; }

    int row = changed->row();
    int col = changed->column();

    m_updating = true;
    for (int c = 0; c < m_rxAntTable->columnCount(); ++c) {
        if (c == col) { continue; }
        if (auto* item = m_rxAntTable->item(row, c)) {
            item->setCheckState(Qt::Unchecked);
        }
    }
    m_updating = false;

    Band band = static_cast<Band>(row);
    emit settingChanged(
        QStringLiteral("antennaAlex/rxAnt[%1]").arg(bandKeyName(band)),
        col + 1 /* 1-based ANT number */);
}

void AntennaAlexTab::onTxAntTableChanged(QTableWidgetItem* changed)
{
    if (m_updating) { return; }
    if (!changed || changed->checkState() != Qt::Checked) { return; }

    int row = changed->row();
    int col = changed->column();

    m_updating = true;
    for (int c = 0; c < m_txAntTable->columnCount(); ++c) {
        if (c == col) { continue; }
        if (auto* item = m_txAntTable->item(row, c)) {
            item->setCheckState(Qt::Unchecked);
        }
    }
    m_updating = false;

    Band band = static_cast<Band>(row);
    emit settingChanged(
        QStringLiteral("antennaAlex/txAnt[%1]").arg(bandKeyName(band)),
        col + 1 /* 1-based ANT number */);
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void AntennaAlexTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    // Restore simple boolean checkboxes. Block signals to avoid re-emitting
    // settingChanged during restore.
    struct CheckEntry { const char* key; QCheckBox* widget; };
    const CheckEntry entries[] = {
        { "rxOutOnTx",    m_rxOutOnTx    },
        { "ext1OutOnTx",  m_ext1OutOnTx  },
        { "ext2OutOnTx",  m_ext2OutOnTx  },
        { "hfTrRelay",    m_hfTrRelay    },
        { "bpf2Gnd",      m_bpf2Gnd      },
        { "enableXvtrHf", m_enableXvtrHf },
    };
    for (const auto& e : entries) {
        auto it = settings.constFind(QString::fromLatin1(e.key));
        if (it != settings.constEnd()) {
            QSignalBlocker blocker(e.widget);
            e.widget->setChecked(it.value().toBool());
        }
    }

    // Restore per-band RX antenna selections
    {
        QSignalBlocker blocker(m_rxAntTable);
        for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
            if (!it.key().startsWith(QStringLiteral("rxAnt["))) { continue; }
            // key format: "rxAnt[<bandKeyName>]"  value = 1-based ANT number
            const QString inner = it.key().mid(6, it.key().size() - 7); // strip "rxAnt[" and "]"
            for (int row = 0; row < m_rxAntTable->rowCount(); ++row) {
                Band band = static_cast<Band>(row);
                if (bandKeyName(band) != inner) { continue; }
                const int antNum = it.value().toInt(); // 1-based
                m_updating = true;
                for (int col = 0; col < m_rxAntTable->columnCount(); ++col) {
                    if (auto* item = m_rxAntTable->item(row, col)) {
                        item->setCheckState((col + 1 == antNum) ? Qt::Checked : Qt::Unchecked);
                    }
                }
                m_updating = false;
                break;
            }
        }
    }

    // Alex-1 Filters tab uses per-MAC AppSettings directly (different key namespace).
    // The MAC is pulled from the settings map using the "mac" key if present, or
    // queried from the model. Delegate to the tab which owns its own restore path.
    // (m_alex1Tab->restoreSettings() is called separately from HardwarePage via
    // populate() → updateBoardCapabilities(), then the MAC is passed via the
    // antennaAlex/alex1 namespace when HardwarePage calls onCurrentRadioChanged.)
    // No additional action needed here for the filtered map variant.
}

} // namespace NereusSDR
