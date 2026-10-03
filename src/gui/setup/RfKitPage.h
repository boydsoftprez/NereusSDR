#pragma once

// =================================================================
// src/gui/setup/RfKitPage.h  (NereusSDR-native)
// =================================================================
//
// RF-Kit integration setup page.  Settings -> RF-Kit.
//
// Hosts a QTabWidget with two tabs:
//   1. General   -- master toggle (RfKit_Enabled), helper text,
//                   live RF2K-S connection status row.
//   2. RF2K-S    -- RF2K-S device configuration (placeholder; full
//                   content lands in Task 11).
//
// Master toggle behaviour:
//   When OFF (default on first run):
//     - Rf2ksApplet hidden in the right-column panel.
//     - RF2K-S tab disabled (greyed out).
//   When ON:
//     - RadioModel::setRfKitEnabled(true) persists the state.
//     - RF2K-S tab becomes interactive.
//     - State persisted via AppSettings key "RfKit_Enabled".
//
// Pattern mirrors src/gui/setup/FourO3APage.{h,cpp}.
// NereusSDR-original page (no Thetis upstream).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-24 -- Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-24 -- R-R3-47 / R-R3-48: in a remote window the page is a
//                 view of the Core's `rfkit` object and switch, and asks
//                 the Core to switch, connect and disconnect the amp; the
//                 band-follow line, local and remote. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 -- R-R3-49 (parity Task 10): in a remote window "Set amp to
//                 TCI mode" asks the Core (setRfKitTciMode), Save keeps a
//                 changed Host and Port on the Core without dialling
//                 (setRfKitAddress), and Live diagnostics shows the Core's
//                 connection counts; a local window's Live diagnostics
//                 gains the connected-since and last-poll readings a remote
//                 one shows. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QHash>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;

namespace NereusSDR {

class RadioModel;

class RfKitPage : public QWidget {
    Q_OBJECT

public:
    explicit RfKitPage(RadioModel* model, QWidget* parent = nullptr);

    // Testing accessors (used by tst_rfkit_page_master_gate).
    QCheckBox*   masterCheckboxForTesting() const { return m_master; }
    bool         detailTabIsEnabledForTesting() const;
    void         setHostForTesting(const QString& host);
    void         setPortForTesting(quint16 port);
    void         setAntennaLabelForTesting(int n, const QString& label);
    void         clickSaveForTesting();
    QPushButton* testConnectionButtonForTesting() const;
    QPushButton* disconnectButtonForTesting() const { return m_disconnectBtn; }
    QString      bandFollowTextForTesting() const;
    QString      liveStatusTextForTesting() const;
    QCheckBox*   autoReconnectForTesting() const { return m_autoReconnect; }
    QSpinBox*    pollIntervalForTesting() const { return m_pollIntervalSpin; }
    QLineEdit*   antennaLabelEditForTesting(int n) const
    { return n >= 1 && n <= 4 ? m_antLabelEdits[n - 1] : nullptr; }
    QPushButton* saveButtonForTesting() const { return m_saveBtn; }
    QPushButton* resetErrorButtonForTesting() const { return m_resetErrBtn; }
    // R-R3-49 (parity Task 10).
    QPushButton* setTciButtonForTesting() const { return m_setTciBtn; }
    QLineEdit*   hostEditForTesting() const { return m_hostEdit; }
    QSpinBox*    portSpinForTesting() const { return m_portSpin; }
    QString      diagnosticsTextForTesting() const;

private slots:
    // Master toggle handler.  Persists the new state via
    // RadioModel::setRfKitEnabled, then gates the RF2K-S tab.
    void onMasterToggled(bool checked);

    // Persist all RF2K-S tab fields to the per-MAC peripherals scope.
    void saveRf2ksSettings();

    // Refresh the live RF2K-S connection status row (1 Hz timer).
    void refreshLiveStatus();

    // Per-radio peripherals refactor (2026-05-26): refresh the "Editing
    // peripherals for <radio> (<mac>)" banner and the gray-out state of
    // every peripheral-bearing control on connectionStateChanged.
    void refreshConnectionBanner();

private:
    // Build the General tab content: master toggle + helper text +
    // live status row.  Returns the composite widget; caller adds it as a tab.
    QWidget* buildGeneralTab();

    // Build the RF2K-S tab placeholder.  Full content in Task 11.
    QWidget* buildRf2ksTab();

    // Enable / disable the RF2K-S detail tab based on master state.
    // Called after onMasterToggled and at construction time.
    void applyMasterGate(bool enabled);

    // Reload the per-MAC peripheral values into the RF2K-S tab widgets.
    // Called from the constructor and on connectionStateChanged so the
    // fields reflect the just-connected radio's saved values.
    void reloadFromPeripherals();

    // R-R3-47: a remote window (the amp is the Core's).
    bool isRemote() const;
    bool remoteControlAvailable() const;
    // I4 (R-R3-47): the Core takes this page's settings, names and Reset amp
    // error from a remote window (remoteRfKitControlVersion 3).
    bool remoteSettingsAvailable() const;
    void refreshRemoteSettings();
    void onResetErrorClicked();
    // Rework part 6: fields the operator changed and has not saved; the
    // Core's settings do not overwrite them.
    bool m_touchedAutoReconnect{false};
    bool m_touchedPoll{false};
    bool m_touchedLabel[4]{false, false, false, false};
    // Rework follow-up 3: values saved and not yet echoed by the Core.
    QHash<QString, QString> m_savedPending;
    void settleSaved(const QString& key);
    // R-R3-49 (parity Task 10): a remote window's TCI mode button: enabled
    // on a Core at remoteRfKitControlVersion 4 while the radio is off the
    // air and the Core is connected to the amp; otherwise disabled with the
    // reason. Parity mini-round (the operator's rulings a and b): the
    // address fields only save, so they stay enabled on the air; a local
    // window's TCI mode button waits on the air too, with the same reason.
    void refreshRemoteControls();
    void onSetTciClicked();
    void refreshBandFollow();
    void onConnectClicked();
    void onDisconnectClicked();

    RadioModel*  m_model{nullptr};

    // Tab host.
    QTabWidget*  m_tabs{nullptr};

    // General tab controls.
    QCheckBox*   m_master{nullptr};
    QLabel*      m_liveStatusLabel{nullptr};

    // Per-radio peripherals refactor (2026-05-26): banner shown at the
    // top of the General tab.  Tells the operator whose peripherals
    // they're editing (or "Connect to a radio..." when offline).
    QLabel*      m_connectionBanner{nullptr};

    // RF2K-S tab widget.  Kept so applyMasterGate can locate it by pointer.
    QWidget*     m_rf2ksTab{nullptr};

    // RF2K-S tab controls (all owned by the tab widget tree).
    QLineEdit*   m_hostEdit{nullptr};
    QSpinBox*    m_portSpin{nullptr};
    QCheckBox*   m_autoReconnect{nullptr};
    QSpinBox*    m_pollIntervalSpin{nullptr};
    QLineEdit*   m_antLabelEdits[4]{nullptr, nullptr, nullptr, nullptr};
    QLabel*      m_diagnosticsLabel{nullptr};
    QPushButton* m_testConnBtn{nullptr};
    QPushButton* m_setTciBtn{nullptr};
    QPushButton* m_resetErrBtn{nullptr};
    QPushButton* m_saveBtn{nullptr};
    QPushButton* m_disconnectBtn{nullptr};
    // R-R3-48: whether the amp follows the radio's band.
    QLabel*      m_bandFollowLabel{nullptr};
    // R-R3-47: what the Core said about the last request (remote window).
    QString      m_remoteResult;
};

} // namespace NereusSDR
