// =================================================================
// src/gui/setup/hardware/OcOutputsTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

// OcOutputsTab.cpp
//
// Source: Thetis Setup.cs UpdateOCBits() lines 12877-12934.
// Per-band RX mask bits: chkPenOCrcv{band}{1..7} — accumulated into a 7-bit
// value via Penny.getPenny().setBandABitMask(band, val, false).
// Per-band TX mask bits: chkPenOCxmit{band}{1..7} — same pattern with true.
// BandBBitMask = 0x70 (bits 4-6 used for the "B" relay board).
//
// NereusSDR: 14 rows (Band::Count) × 7 columns. Rows = bands, columns = OC
// output bits 1–7 (0-indexed 0–6, matching bit positions 0–6 of the mask byte).

#include "OcOutputsTab.h"

#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"
#include "models/Band.h"
#include "models/RadioModel.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace NereusSDR {

static constexpr int kOcOutputCount = 7;  // From Thetis Setup.cs:12908-12914 — bits 0..6

// ── Static helper: build an empty 14×7 grid ───────────────────────────────────
QTableWidget* OcOutputsTab::makeGrid(QWidget* parent)
{
    auto* table = new QTableWidget(static_cast<int>(Band::Count), kOcOutputCount, parent);

    // Column headers: OC1 … OC7
    QStringList colHdr;
    for (int i = 1; i <= kOcOutputCount; ++i) {
        colHdr << QStringLiteral("OC%1").arg(i);
    }
    table->setHorizontalHeaderLabels(colHdr);

    // Row headers: band labels
    QStringList rowHdr;
    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        rowHdr << bandLabel(static_cast<Band>(i));
    }
    table->setVerticalHeaderLabels(rowHdr);

    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setMaximumHeight(220);

    for (int row = 0; row < static_cast<int>(Band::Count); ++row) {
        for (int col = 0; col < kOcOutputCount; ++col) {
            auto* item = new QTableWidgetItem();
            item->setCheckState(Qt::Unchecked);
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            table->setItem(row, col, item);
        }
    }
    return table;
}

// ── Constructor ───────────────────────────────────────────────────────────────

OcOutputsTab::OcOutputsTab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(6);

    // "No OC outputs" fallback label (hidden when board has OC outputs)
    m_noOcLabel = new QLabel(
        tr("This board has no Open-Collector outputs."), this);
    m_noOcLabel->setAlignment(Qt::AlignCenter);
    m_noOcLabel->setVisible(false);
    outerLayout->addWidget(m_noOcLabel);

    // ── RX mask grid ──────────────────────────────────────────────────────────
    // Source: Thetis Setup.cs chkPenOCrcv{band}{1-7} — bits 0-6 of RX mask
    auto* rxGroup = new QGroupBox(tr("RX Outputs (per band)"), this);
    auto* rxVBox  = new QVBoxLayout(rxGroup);
    m_rxGrid = makeGrid(rxGroup);
    rxVBox->addWidget(m_rxGrid);
    outerLayout->addWidget(rxGroup);

    // ── TX mask grid ──────────────────────────────────────────────────────────
    // Source: Thetis Setup.cs chkPenOCxmit{band}{1-7} — bits 0-6 of TX mask
    auto* txGroup = new QGroupBox(tr("TX Outputs (per band)"), this);
    auto* txVBox  = new QVBoxLayout(txGroup);
    m_txGrid = makeGrid(txGroup);
    txVBox->addWidget(m_txGrid);
    outerLayout->addWidget(txGroup);

    // ── Relay settle delay ────────────────────────────────────────────────────
    // No direct Thetis equivalent; stub control for relay sequencing delay.
    auto* delayGroup = new QGroupBox(tr("Relay Timing"), this);
    auto* delayForm  = new QHBoxLayout(delayGroup);
    delayForm->addWidget(new QLabel(tr("Relay settle delay (ms):"), delayGroup));
    m_settleDelaySpin = new QSpinBox(delayGroup);
    m_settleDelaySpin->setRange(0, 500);
    m_settleDelaySpin->setValue(10);
    m_settleDelaySpin->setSuffix(tr(" ms"));
    delayForm->addWidget(m_settleDelaySpin);
    delayForm->addStretch();
    outerLayout->addWidget(delayGroup);
    outerLayout->addStretch();

    // ── Wire signals ─────────────────────────────────────────────────────────
    connect(m_rxGrid, &QTableWidget::itemChanged,
            this, &OcOutputsTab::onRxMaskChanged);
    connect(m_txGrid, &QTableWidget::itemChanged,
            this, &OcOutputsTab::onTxMaskChanged);
    connect(m_settleDelaySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int ms) {
                emit settingChanged(QStringLiteral("ocOutputs/settleDelayMs"), ms);
            });
}

// ── populate ──────────────────────────────────────────────────────────────────

void OcOutputsTab::populate(const RadioInfo& /*info*/, const BoardCapabilities& caps)
{
    // Defensive: if board has no OC outputs, show the fallback label.
    const bool hasOc = caps.ocOutputCount > 0;
    m_noOcLabel->setVisible(!hasOc);
    m_rxGrid->setVisible(hasOc);
    m_txGrid->setVisible(hasOc);
    m_settleDelaySpin->setEnabled(hasOc);

    if (!hasOc) { return; }

    // Disable columns beyond the board's actual output count.
    // Source: Thetis Setup.cs — Penelope has 7 OC outputs; HL2 may have fewer.
    for (int row = 0; row < static_cast<int>(Band::Count); ++row) {
        for (int col = 0; col < kOcOutputCount; ++col) {
            bool colActive = (col < caps.ocOutputCount);
            auto enableGrid = [&](QTableWidget* grid) {
                if (auto* item = grid->item(row, col)) {
                    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
                    if (colActive) {
                        flags |= Qt::ItemIsUserCheckable;
                    } else {
                        item->setCheckState(Qt::Unchecked);
                    }
                    item->setFlags(flags);
                }
            };
            enableGrid(m_rxGrid);
            enableGrid(m_txGrid);
        }
    }
}

// ── private slots ─────────────────────────────────────────────────────────────

void OcOutputsTab::onRxMaskChanged(QTableWidgetItem* item)
{
    if (!item) { return; }
    int row = item->row();
    int col = item->column();
    Band band = static_cast<Band>(row);
    bool checked = (item->checkState() == Qt::Checked);
    emit settingChanged(
        QStringLiteral("ocOutputs/rxMask[%1][%2]")
            .arg(bandKeyName(band))
            .arg(col + 1 /* 1-based output number */),
        checked);
}

void OcOutputsTab::onTxMaskChanged(QTableWidgetItem* item)
{
    if (!item) { return; }
    int row = item->row();
    int col = item->column();
    Band band = static_cast<Band>(row);
    bool checked = (item->checkState() == Qt::Checked);
    emit settingChanged(
        QStringLiteral("ocOutputs/txMask[%1][%2]")
            .arg(bandKeyName(band))
            .arg(col + 1 /* 1-based output number */),
        checked);
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void OcOutputsTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    // Restore relay settle delay
    auto it = settings.constFind(QStringLiteral("settleDelayMs"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_settleDelaySpin);
        m_settleDelaySpin->setValue(it.value().toInt());
    }

    // Restore RX mask checkboxes: keys like "rxMask[<bandKeyName>][<col1-based>]"
    {
        QSignalBlocker blocker(m_rxGrid);
        for (auto mit = settings.constBegin(); mit != settings.constEnd(); ++mit) {
            if (!mit.key().startsWith(QStringLiteral("rxMask["))) { continue; }
            // Parse key "rxMask[<band>][<colNum>]"
            QString rest = mit.key().mid(7); // strip "rxMask["
            int closeB = rest.indexOf(QLatin1Char(']'));
            if (closeB < 0) { continue; }
            const QString bandName = rest.left(closeB);
            rest = rest.mid(closeB + 2); // skip "]["
            const int colNum = rest.left(rest.indexOf(QLatin1Char(']'))).toInt();
            for (int row = 0; row < m_rxGrid->rowCount(); ++row) {
                if (bandKeyName(static_cast<Band>(row)) != bandName) { continue; }
                if (auto* item = m_rxGrid->item(row, colNum - 1)) {
                    item->setCheckState(mit.value().toBool() ? Qt::Checked : Qt::Unchecked);
                }
                break;
            }
        }
    }

    // Restore TX mask checkboxes: keys like "txMask[<bandKeyName>][<col1-based>]"
    {
        QSignalBlocker blocker(m_txGrid);
        for (auto mit = settings.constBegin(); mit != settings.constEnd(); ++mit) {
            if (!mit.key().startsWith(QStringLiteral("txMask["))) { continue; }
            QString rest = mit.key().mid(7);
            int closeB = rest.indexOf(QLatin1Char(']'));
            if (closeB < 0) { continue; }
            const QString bandName = rest.left(closeB);
            rest = rest.mid(closeB + 2);
            const int colNum = rest.left(rest.indexOf(QLatin1Char(']'))).toInt();
            for (int row = 0; row < m_txGrid->rowCount(); ++row) {
                if (bandKeyName(static_cast<Band>(row)) != bandName) { continue; }
                if (auto* item = m_txGrid->item(row, colNum - 1)) {
                    item->setCheckState(mit.value().toBool() ? Qt::Checked : Qt::Unchecked);
                }
                break;
            }
        }
    }
}

} // namespace NereusSDR
