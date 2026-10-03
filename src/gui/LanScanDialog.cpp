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
//
// 2026-09-25: R-R3-49 (parity Task 8): the Core's scan for a remote
// window (LanScanDialog(RadioModel*, QWidget*)). J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
//
// 2026-09-25: R-R3-49 (parity Task 9): the Core's Power Genius scan
// (scanPgxlLan) through the same dialog. J.J. Boyd (KG4VCF), AI-assisted
// via Anthropic Claude Code.

#include "gui/LanScanDialog.h"

#include "core/LanDiscovery.h"
#include "core/session/IStationLink.h"
#include "gui/OperatorReasonText.h"
#include "models/RadioModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
    buildUi(tr("Scanning LAN for 4O3A devices (3 seconds)..."));

    // Wire up the discovery engine and start the 3-second scan.
    m_discovery = new LanDiscovery(this);

    connect(m_discovery, &LanDiscovery::deviceDiscovered,
            this, &LanScanDialog::onDeviceDiscovered);
    connect(m_discovery, &LanDiscovery::scanFinished,
            this, &LanScanDialog::onScanFinished);

    m_discovery->start(3000);
}

LanScanDialog::LanScanDialog(RadioModel* coreModel, QWidget* parent, CoreDevice device)
    : QDialog(parent)
    , m_coreModel(coreModel)
{
    const bool amp = device == CoreDevice::PowerGenius;
    buildUi(amp ? tr("The Core is listening for a Power Genius on its network (3 seconds)...")
                : tr("The Core is listening for a Tuner Genius on its network (3 seconds)..."));
    IStationLink* link = m_coreModel ? m_coreModel->stationLink() : nullptr;
    if (!link) {
        finishCoreScan(false, amp ? IStationLink::pgxlFullControlUnavailableReason()
                                  : IStationLink::tgxlFullControlUnavailableReason(),
                       QString());
        return;
    }
    const auto onAnswer = [this](quint32 commandId, bool accepted, const QString& reason,
                                 const QString& devicesJson) {
        if (m_coreScanPending && commandId == m_coreCommandId) {
            finishCoreScan(accepted, reason, devicesJson);
        }
    };
    if (amp) {
        connect(m_coreModel, &RadioModel::stationPgxlLanScanFinished, this, onAnswer);
    } else {
        connect(m_coreModel, &RadioModel::stationTgxlLanScanFinished, this, onAnswer);
    }
    connect(m_coreModel, &RadioModel::stationLinkStateChanged, this, [this]() {
        const IStationLink* current = m_coreModel->stationLink();
        if (m_coreScanPending && (!current || !current->stationLinkReady())) {
            finishCoreScan(false, tr("The link to the Core dropped before the scan finished."),
                           QString());
        }
    });
    const IStationLink::CommandOutcome outcome = amp ? link->requestPgxlLanScan()
                                                     : link->requestTgxlLanScan();
    if (!outcome.sent) {
        finishCoreScan(false, outcome.reason, QString());
        return;
    }
    m_coreCommandId = outcome.commandId;
    m_coreScanPending = true;
    // The dialog shows the Core's refusal itself (no second notice).
    m_coreModel->noteAccessoryRequestShownOnPage(outcome.commandId, this);
}

void LanScanDialog::finishCoreScan(bool accepted, const QString& reason,
                                   const QString& devicesJson)
{
    m_coreScanPending = false;
    if (!accepted) {
        m_progressBar->setValue(100);
        m_statusLabel->setText(OperatorReasonText::forDisplay(reason));
        return;
    }
    const QJsonArray devices = QJsonDocument::fromJson(devicesJson.toUtf8()).array();
    for (const QJsonValue& value : devices) {
        const QJsonObject device = value.toObject();
        const int port = device.value(QStringLiteral("port")).toInt();
        if (port < 1 || port > 65535) {
            continue;
        }
        // The Core does not say the firmware version; the column stays empty.
        onDeviceDiscovered(device.value(QStringLiteral("model")).toString(),
                           device.value(QStringLiteral("address")).toString(),
                           static_cast<quint16>(port), QString(),
                           device.value(QStringLiteral("serial")).toString(),
                           device.value(QStringLiteral("nickname")).toString());
    }
    onScanFinished();
}

int LanScanDialog::rowCountForTesting() const
{
    return m_table ? m_table->rowCount() : 0;
}

QString LanScanDialog::statusTextForTesting() const
{
    return m_statusLabel ? m_statusLabel->text() : QString();
}

void LanScanDialog::buildUi(const QString& status)
{
    setWindowTitle(tr("Scan LAN for Peripherals"));
    setMinimumWidth(640);
    setModal(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    // Status label shown above the table.
    m_statusLabel = new QLabel(status, this);
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
