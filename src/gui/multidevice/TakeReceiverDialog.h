#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/TakeReceiverDialog.h  (NereusSDR)
// =================================================================
//
// The take-a-receiver chooser (iPhone app plan Task 78, R-IOS-30; the
// several-devices design, sections 6.4 and 12 item 4): the Core asks
// `takeReceiver` (one choice per receiver, each with its devices, slices
// and frequencies) or `takeSlice` (one per slice of another device). One
// pick; a choice the Core marks not takeable is shown, disabled, with its
// `why`. Take answers confirm.proceed with the pick's `choice`, Cancel
// confirm.cancel.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/SessionMessages.h"

#include <QDialog>
#include <QJsonObject>

class QListWidget;
class QPushButton;

namespace NereusSDR {

class TakeReceiverDialog : public QDialog {
    Q_OBJECT

public:
    explicit TakeReceiverDialog(const SessionPrompt& prompt, QWidget* parent = nullptr);

    qint64 questionId() const { return m_id; }
    /// The picked choice's `choice`, -1 when none is picked.
    qint64 pickedChoice() const;

    /// One choice's text, as the list shows it.
    static QString choiceText(const QString& kind, const QJsonObject& choice);

    QListWidget* choiceList() const { return m_list; }
    QPushButton* takeButton() const { return m_take; }
    QPushButton* cancelButton() const { return m_cancel; }

private:
    void refreshTakeButton();

    qint64 m_id = 0;
    QListWidget* m_list = nullptr;
    QPushButton* m_take = nullptr;
    QPushButton* m_cancel = nullptr;
};

} // namespace NereusSDR
