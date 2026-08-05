// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - LanScanDialog: modeless dialog that drives a LanDiscovery
// instance and shows discovered 4O3A peripheral devices in a 6-column
// table. Double-clicking a row emits deviceSelected(ip, port) and closes
// the dialog, filling the Host and Port fields in the PeripheralsPage row
// that launched the scan.
//
// NereusSDR-native (no upstream). Design reference:
// docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md
// section 5.3 (LAN scan UX).
//
// Phase 3P-II Task 18.
// AI tooling: Anthropic Claude Code.

#pragma once

#include <QDialog>

class QTableWidget;
class QProgressBar;
class QLabel;

namespace NereusSDR {

class LanDiscovery;

// Modeless dialog that runs a 3-second LAN scan (via LanDiscovery) and
// presents discovered devices in a 6-column table:
//   Model | IP | Port | Version | Serial | Nickname
//
// Usage:
//   auto* dlg = new LanScanDialog(this);
//   dlg->setAttribute(Qt::WA_DeleteOnClose);
//   connect(dlg, &LanScanDialog::deviceSelected,
//           this, [this, rowIdx](const QString& ip, quint16 port) {
//               // fill the row's IP edit and port spin
//           });
//   dlg->show();
//
// The constructor calls LanDiscovery::start(3000). When the discovery
// signals scanFinished() the progress bar reaches 100% and "Scan complete"
// is shown. The caller does not need to call stop(); the internal
// LanDiscovery timer fires automatically.
class LanScanDialog : public QDialog {
    Q_OBJECT

public:
    explicit LanScanDialog(QWidget* parent = nullptr);

signals:
    // Emitted when the user double-clicks a device row. The dialog closes
    // (accept()) immediately after emitting.
    void deviceSelected(const QString& ip, quint16 port);

    // Forwarded from LanDiscovery::scanFinished() -- useful if the caller
    // wants to know when the 3-second window has elapsed.
    void scanFinished();

private slots:
    void onDeviceDiscovered(const QString& model,
                            const QString& ip,
                            quint16        port,
                            const QString& version,
                            const QString& serial,
                            const QString& nickname);
    void onScanFinished();
    void onCellDoubleClicked(int row, int column);

private:
    LanDiscovery*   m_discovery{nullptr};
    QTableWidget*   m_table{nullptr};
    QProgressBar*   m_progressBar{nullptr};
    QLabel*         m_statusLabel{nullptr};
};

} // namespace NereusSDR
