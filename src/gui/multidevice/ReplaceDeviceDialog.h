#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/ReplaceDeviceDialog.h  (NereusSDR)
// =================================================================
//
// The fifth-device choice (iPhone app plan Task 78 item 7, G-53; the
// several-devices design, section 12; the link document, section 5.1
// step 4). A remote window that reaches a Core with four devices on it is
// asked which one it replaces (session.held). The list is the Core's own
// order, idle longest first; each device shows its name, how long it has
// been connected, when it was last active and what it is doing (listening
// on which slices, on the air with its transmit clock, or away). The
// desktop that runs the Core is shown and cannot be chosen. The choice
// starts on the device that took this window's place when there is one
// (Take it back), otherwise on the first that can be replaced; choosing
// the one on the air turns the button red. Replace answers
// session.takeover with the pick, Cancel with none.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 item 7 (G-53), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/RemoteDevicesState.h"

#include <QDialog>

class QLabel;
class QListWidget;
class QPushButton;

namespace NereusSDR {

class ReplaceDeviceDialog : public QDialog {
    Q_OBJECT

public:
    explicit ReplaceDeviceDialog(const RemoteHeldList& held, QWidget* parent = nullptr);

    /// A newer list from the Core: the rows change, the pick stays on the
    /// same device while it is still there and can be replaced.
    void setHeld(const RemoteHeldList& held);

    /// The picked device's wire id; empty when none is picked.
    QString pickedDeviceId() const;
    /// True when the picked device is on the air.
    bool pickedIsOnAir() const;

    /// One device's text, as the list shows it.
    static QString entryText(const RemoteHeldEntry& entry);
    /// The words above the list (what the Core said about this window's
    /// own place, when it said anything).
    static QString introText(const RemoteHeldList& held);
    static QString replaceButtonText(bool onAir);

    QListWidget* deviceList() const { return m_list; }
    QPushButton* replaceButton() const { return m_replace; }
    QPushButton* cancelButton() const { return m_cancel; }
    QLabel* intro() const { return m_intro; }

private:
    void refreshReplaceButton();

    QLabel* m_intro = nullptr;
    QListWidget* m_list = nullptr;
    QPushButton* m_replace = nullptr;
    QPushButton* m_cancel = nullptr;
};

} // namespace NereusSDR
