// =================================================================
// src/gui/setup/hardware/XvtrTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/xvtr.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// xvtr.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2013  Doug Wigley
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

#include "XvtrTab.h"

#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace NereusSDR {

// Thetis xvtr.cs supports up to 16 rows (chkEnable0..15).
static constexpr int kMaxXvtrRows = 16;

// Column indices — match Thetis xvtr.cs field order
enum XvtrCol : int {
    ColEnabled  = 0,
    ColName     = 1,
    ColRfStart  = 2,
    ColRfEnd    = 3,
    ColLoOffset = 4,
    ColRxOnly   = 5,
    ColPower    = 6,
    ColLoError  = 7,
    ColCount    = 8
};

// ── Constructor ───────────────────────────────────────────────────────────────

XvtrTab::XvtrTab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(6);

    // "No XVTR" fallback
    m_noXvtrLabel = new QLabel(
        tr("This board has no transverter jack."), this);
    m_noXvtrLabel->setAlignment(Qt::AlignCenter);
    m_noXvtrLabel->setVisible(false);
    outerLayout->addWidget(m_noXvtrLabel);

    // Auto-select checkbox
    // (Not a direct Thetis control but consistent with console.cs XVTR logic)
    m_autoSelectBand = new QCheckBox(tr("Auto-select active band"), this);
    m_autoSelectBand->setToolTip(
        tr("Automatically activates the matching XVTR row when the VFO "
           "frequency falls within its RF range."));
    outerLayout->addWidget(m_autoSelectBand);

    // ── Transverter table ─────────────────────────────────────────────────────
    // Source: Thetis xvtr.cs — 16 rows, columns: Enabled/Name/RF Start/RF End/
    // LO Offset/RX-only/Power/LO Error
    m_table = new QTableWidget(0, ColCount, this);
    m_table->setHorizontalHeaderLabels({
        tr("Enabled"),
        tr("Name"),
        tr("RF Start (Hz)"),
        tr("RF End (Hz)"),
        tr("LO Offset (Hz)"),
        tr("RX Only"),
        tr("Power (dB)"),
        tr("LO Error (Hz)")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    outerLayout->addWidget(m_table);
    outerLayout->addStretch();

    // ── Wire signals ─────────────────────────────────────────────────────────
    connect(m_table, &QTableWidget::itemChanged,
            this, &XvtrTab::onTableItemChanged);

    connect(m_autoSelectBand, &QCheckBox::toggled, this, [this](bool checked) {
        emit settingChanged(QStringLiteral("xvtr/autoSelectBand"), checked);
    });
}

// ── populate ──────────────────────────────────────────────────────────────────

void XvtrTab::populate(const RadioInfo& /*info*/, const BoardCapabilities& caps)
{
    const bool hasXvtr = caps.xvtrJackCount > 0;
    m_noXvtrLabel->setVisible(!hasXvtr);
    m_autoSelectBand->setVisible(hasXvtr);
    m_table->setVisible(hasXvtr);

    if (!hasXvtr) { return; }

    // Thetis xvtr.cs has up to 16 entries; cap at kMaxXvtrRows.
    const int rows = qMin(caps.xvtrJackCount, kMaxXvtrRows);
    if (rows == m_visibleRows) { return; }

    QSignalBlocker blocker(m_table);
    m_table->setRowCount(rows);
    m_visibleRows = rows;

    for (int row = 0; row < rows; ++row) {
        // ── Enabled checkbox cell (ColEnabled) ───────────────────────────────
        // Source: Thetis xvtr.cs chkEnable0..15
        if (!m_table->item(row, ColEnabled)) {
            auto* item = new QTableWidgetItem();
            item->setCheckState(Qt::Unchecked);
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_table->setItem(row, ColEnabled, item);
        }

        // ── Name (editable text) ──────────────────────────────────────────────
        // Source: Thetis xvtr.cs txtButtonText0..15
        if (!m_table->item(row, ColName)) {
            auto* item = new QTableWidgetItem(
                QStringLiteral("XVTR %1").arg(row + 1));
            item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_table->setItem(row, ColName, item);
        }

        // ── Numeric editable cells ────────────────────────────────────────────
        // Source: Thetis xvtr.cs udFreqBegin*, udFreqEnd*, udLOOffset*,
        //         udPower*, udLOError*
        auto makeNumCell = [&](int col, const QString& defaultVal) {
            if (!m_table->item(row, col)) {
                auto* item = new QTableWidgetItem(defaultVal);
                item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_table->setItem(row, col, item);
            }
        };
        makeNumCell(ColRfStart,  QStringLiteral("144000000"));
        makeNumCell(ColRfEnd,    QStringLiteral("148000000"));
        makeNumCell(ColLoOffset, QStringLiteral("116000000"));
        makeNumCell(ColPower,    QStringLiteral("0"));
        makeNumCell(ColLoError,  QStringLiteral("0"));

        // ── RX-only checkbox cell ─────────────────────────────────────────────
        // Source: Thetis xvtr.cs chkRXOnly0..15
        if (!m_table->item(row, ColRxOnly)) {
            auto* item = new QTableWidgetItem();
            item->setCheckState(Qt::Unchecked);
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_table->setItem(row, ColRxOnly, item);
        }

        // Row header label
        m_table->setVerticalHeaderItem(row,
            new QTableWidgetItem(QStringLiteral("%1").arg(row + 1)));
    }
}

// ── private slots ─────────────────────────────────────────────────────────────

void XvtrTab::onTableItemChanged(QTableWidgetItem* item)
{
    if (!item) { return; }
    int row = item->row();
    int col = item->column();

    QString field;
    QVariant value;

    switch (static_cast<XvtrCol>(col)) {
    case ColEnabled:
        field = QStringLiteral("enabled");
        value = (item->checkState() == Qt::Checked);
        break;
    case ColName:
        field = QStringLiteral("name");
        value = item->text();
        break;
    case ColRfStart:
        field = QStringLiteral("rfStart");
        value = item->text().toLongLong();
        break;
    case ColRfEnd:
        field = QStringLiteral("rfEnd");
        value = item->text().toLongLong();
        break;
    case ColLoOffset:
        field = QStringLiteral("loOffset");
        value = item->text().toLongLong();
        break;
    case ColRxOnly:
        field = QStringLiteral("rxOnly");
        value = (item->checkState() == Qt::Checked);
        break;
    case ColPower:
        field = QStringLiteral("power");
        value = item->text().toInt();
        break;
    case ColLoError:
        field = QStringLiteral("loError");
        value = item->text().toLongLong();
        break;
    default:
        return;
    }

    emit settingChanged(
        QStringLiteral("xvtr/row%1/%2").arg(row).arg(field),
        value);
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void XvtrTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    // Restore autoSelectBand checkbox
    auto it = settings.constFind(QStringLiteral("autoSelectBand"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_autoSelectBand);
        m_autoSelectBand->setChecked(it.value().toBool());
    }

    // Restore table cells: keys like "row<N>/<field>"
    {
        QSignalBlocker blocker(m_table);
        for (auto mit = settings.constBegin(); mit != settings.constEnd(); ++mit) {
            if (!mit.key().startsWith(QStringLiteral("row"))) { continue; }
            const QString rest = mit.key().mid(3); // strip "row"
            const int slash = rest.indexOf(QLatin1Char('/'));
            if (slash < 0) { continue; }
            const int row = rest.left(slash).toInt();
            if (row < 0 || row >= m_table->rowCount()) { continue; }
            const QString field = rest.mid(slash + 1);

            int col = -1;
            if (field == QLatin1String("enabled"))  { col = 0; }
            else if (field == QLatin1String("name"))      { col = 1; }
            else if (field == QLatin1String("rfStartHz")) { col = 2; }
            else if (field == QLatin1String("rfEndHz"))   { col = 3; }
            else if (field == QLatin1String("loOffsetHz")){ col = 4; }
            else if (field == QLatin1String("rxOnly"))    { col = 5; }
            else if (field == QLatin1String("powerDb"))   { col = 6; }
            else if (field == QLatin1String("loErrorHz")) { col = 7; }
            if (col < 0 || col >= m_table->columnCount()) { continue; }

            if (auto* item = m_table->item(row, col)) {
                if (col == 0 || col == 5) { // checkable columns
                    item->setCheckState(mit.value().toBool() ? Qt::Checked : Qt::Unchecked);
                } else {
                    item->setText(mit.value().toString());
                }
            }
        }
    }
}

} // namespace NereusSDR
