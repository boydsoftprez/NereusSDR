#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/ConfirmChangeDialog.h  (NereusSDR)
// =================================================================
//
// One dialog shape for a change the Core holds because it reaches another
// device (iPhone app plan Task 78, R-IOS-30; the several-devices design,
// sections 7.3 and 12 item 5): a shared setting (`sharedSetting`) or a
// panadapter move (`panMove`). It shows the change (`change`: label, from,
// to) and names each device it reaches and what happens to its slices
// (`affected`). Confirm answers confirm.proceed, Cancel confirm.cancel.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/SessionMessages.h"

#include <QDialog>
#include <QStringList>

class QLabel;
class QPushButton;

namespace NereusSDR {

class ConfirmChangeDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConfirmChangeDialog(const SessionPrompt& prompt, QWidget* parent = nullptr);

    qint64 questionId() const { return m_id; }

    /// "Attenuator, ADC 1: 0 dB to 20 dB".
    static QString changeText(const SessionPrompt& prompt);
    /// One line per device, then one per slice of it.
    static QStringList affectedLines(const SessionPrompt& prompt);
    /// "moves to another receiver", "closes", ...
    static QString effectText(const QString& effect);

    QPushButton* confirmButton() const { return m_confirm; }
    QPushButton* cancelButton() const { return m_cancel; }
    QLabel* changeLabel() const { return m_change; }
    QLabel* affectedLabel() const { return m_affected; }

private:
    qint64 m_id = 0;
    QLabel* m_change = nullptr;
    QLabel* m_affected = nullptr;
    QPushButton* m_confirm = nullptr;
    QPushButton* m_cancel = nullptr;
};

} // namespace NereusSDR
