// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - PgxlSaveRebootDialog: confirmation modal for PGXL save & reboot.
//
// Presents a modal dialog explaining the reboot sequence and asks the user
// to confirm before sending the save command to the Power Genius XL device.
// On accept, returns QDialog::Accepted; on reject or cancel, QDialog::Rejected.
//
// NereusSDR-native (no upstream). Design reference:
// docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md
// section 5.6 footer.
//
// Phase 3P-II Task 84.
// AI tooling: Anthropic Claude Code.

#include "PgxlSaveRebootDialog.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace NereusSDR {

PgxlSaveRebootDialog::PgxlSaveRebootDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Save & Reboot PGXL"));
    setModal(true);

    auto* layout = new QVBoxLayout(this);

    // Explanatory text per design spec section 5.6 footer.
    auto* msg = new QLabel(
        QStringLiteral("Sending `save` will persist your configuration to "
                       "flash and reboot the PGXL. The amplifier will be "
                       "offline for approximately 20 seconds. NereusSDR will "
                       "auto-reconnect when it returns. Do not transmit "
                       "during reboot."),
        this);
    msg->setWordWrap(true);
    layout->addWidget(msg);

    // Button row
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    m_saveBtn = new QPushButton(QStringLiteral("Save"), this);
    m_saveBtn->setDefault(true);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_saveBtn);
    layout->addLayout(btnRow);

    // Wire buttons
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_saveBtn,   &QPushButton::clicked, this, &QDialog::accept);

    resize(420, 200);
}

} // namespace NereusSDR
