// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - LanScanDialog implementation.
//
// NereusSDR-native (no upstream). Design reference:
// docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md
// section 5.3 (LAN scan UX).
//
// Phase 3P-II Task 18.
// AI tooling: Anthropic Claude Code.

#include "gui/LanScanDialog.h"

#include "core/LanDiscovery.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QTimer>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace NereusSDR {

LanScanDialog::LanScanDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Scan LAN for Peripherals"));
    setMinimumWidth(640);
    setModal(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    // Status label shown above the table.
    m_statusLabel = new QLabel(tr("Scanning LAN for 4O3A devices (3 seconds)..."), this);
    layout->addWidget(m_statusLabel);

    // Progress bar representing the 3-second scan window. Driven by
    // LanDiscovery::scanFinished() which fires when the timeout elapses.
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);

    // Animate the bar across the scan window using a QTimer tick every 30 ms
    // so the operator gets live feedback that the scan is running.
    auto* ticker = new QTimer(this);
    ticker->setInterval(30);
    connect(ticker, &QTimer::timeout, this, [this, ticker]() {
        const int next = m_progressBar->value() + 1;
        if (next >= 100) {
            m_progressBar->setValue(100);
            ticker->stop();
        } else {
            m_progressBar->setValue(next);
        }
    });
    ticker->start();

    layout->addWidget(m_progressBar);

    // 6-column table: Model, IP, Port, Version, Serial, Nickname.
    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({
        tr("Model"), tr("IP"), tr("Port"),
        tr("Version"), tr("Serial"), tr("Nickname")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setMinimumHeight(160);
    m_table->setToolTip(tr("Double-click a row to fill the Host and Port fields."));

    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, &LanScanDialog::onCellDoubleClicked);

    layout->addWidget(m_table);

    // Hint text below the table.
    auto* hint = new QLabel(
        tr("Double-click a row to select the device and close this dialog."), this);
    hint->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    layout->addWidget(hint);

    // Close button.
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // Wire up the discovery engine and start the 3-second scan.
    m_discovery = new LanDiscovery(this);

    connect(m_discovery, &LanDiscovery::deviceDiscovered,
            this, &LanScanDialog::onDeviceDiscovered);
    connect(m_discovery, &LanDiscovery::scanFinished,
            this, &LanScanDialog::onScanFinished);

    m_discovery->start(3000);
}

void LanScanDialog::onDeviceDiscovered(const QString& model,
                                       const QString& ip,
                                       quint16        port,
                                       const QString& version,
                                       const QString& serial,
                                       const QString& nickname)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    m_table->setItem(row, 0, new QTableWidgetItem(model));
    m_table->setItem(row, 1, new QTableWidgetItem(ip));
    m_table->setItem(row, 2, new QTableWidgetItem(QString::number(port)));
    m_table->setItem(row, 3, new QTableWidgetItem(version));
    m_table->setItem(row, 4, new QTableWidgetItem(serial));
    m_table->setItem(row, 5, new QTableWidgetItem(nickname));

    // Store the numeric port in the item's data role so we can retrieve it
    // precisely on double-click without re-parsing the text.
    m_table->item(row, 2)->setData(Qt::UserRole, static_cast<int>(port));
}

void LanScanDialog::onScanFinished()
{
    m_progressBar->setValue(100);

    const int count = m_table->rowCount();
    if (count == 0) {
        m_statusLabel->setText(tr("Scan complete. No devices found."));
    } else if (count == 1) {
        m_statusLabel->setText(tr("Scan complete. 1 device found."));
    } else {
        m_statusLabel->setText(tr("Scan complete. %1 devices found.").arg(count));
    }

    emit scanFinished();
}

void LanScanDialog::onCellDoubleClicked(int row, int /*column*/)
{
    if (row < 0 || row >= m_table->rowCount()) {
        return;
    }

    const QString ip   = m_table->item(row, 1)->text();
    const quint16 port = static_cast<quint16>(
        m_table->item(row, 2)->data(Qt::UserRole).toInt());

    emit deviceSelected(ip, port);
    accept();
}

} // namespace NereusSDR
