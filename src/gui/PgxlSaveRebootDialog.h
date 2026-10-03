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
// 2026-09-24: R-R3-47 / R-R3-22: message(), so a remote window's
// confirmation (and its test) uses the same words. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.

#pragma once

#include <QDialog>

class QPushButton;

namespace NereusSDR {

class PgxlSaveRebootDialog : public QDialog {
    Q_OBJECT
public:
    explicit PgxlSaveRebootDialog(QWidget* parent = nullptr);

    /// The dialog's question, word for word.
    static QString message();

private:
    QPushButton* m_cancelBtn{nullptr};
    QPushButton* m_saveBtn{nullptr};
};

} // namespace NereusSDR
