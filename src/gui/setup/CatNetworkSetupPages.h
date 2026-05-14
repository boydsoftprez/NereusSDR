#pragma once

#include "gui/SetupPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>

namespace NereusSDR { class TciServer; }

namespace NereusSDR {

// ---------------------------------------------------------------------------
// CAT > Serial Ports
// Four identical port sections, each with: port combo, baud combo,
// enable toggle, and status label. All controls are NYI / disabled.
// ---------------------------------------------------------------------------
class CatSerialPortsPage : public SetupPage {
    Q_OBJECT

public:
    explicit CatSerialPortsPage(QWidget* parent = nullptr);

private:
    struct PortRow {
        QComboBox*  portCombo{nullptr};
        QComboBox*  baudCombo{nullptr};
        QCheckBox*  enableCheck{nullptr};
        QLabel*     statusLabel{nullptr};
    };

    PortRow m_ports[4];

    void buildUI();
};

// ---------------------------------------------------------------------------
// CAT > TCI Server
// 6 group boxes: Server / Compatibility / IQ Stream / Audio Stream /
//   Sensors / VFO Quirks.  All 17 AppSettings keys bound.
// Phase 20 (Phase 3J-1): Setup → Network → TCI Server page rewrite.
// ---------------------------------------------------------------------------
class CatTciServerPage : public SetupPage {
    Q_OBJECT

public:
    explicit CatTciServerPage(QWidget* parent = nullptr);

    // ── Phase 3J-1 bench fix (2026-05-11): live status hookup ──────────────
    //
    // Connect the TciServer reference so the Server group box title shows
    // the live client count (`TCI Server (N clients)`) and the Status label
    // reflects running/stopped state.  Modeled on Thetis
    // Setup.cs:9491-9494 [v2.10.3.13] — TCIClientsConnectedChange setter
    // updates `grpTCIServer.Text`.
    //
    // Pass nullptr to detach (e.g. when the server is destroyed); the page
    // tracks the pointer via QPointer so a stale connection is harmless.
    //
    // Idempotent: re-calls with the same pointer just refresh the snapshot.
    void setTciServer(class NereusSDR::TciServer* server);

signals:
    // Emitted when the operator toggles the Enable TCI Server checkbox.
    // Phase 3J-1 review P2.4: MainWindow::wireSetupDialog connects this to
    // the live start/stop path so the server starts or stops immediately
    // without a disconnect/reconnect cycle.
    // `on`   — true: start the server on the persisted port.
    // `port` — the port currently shown in the Port spinbox (persisted at emit
    //          time; MainWindow should re-read AppSettings or accept the value
    //          directly to avoid a race with a concurrent port-spinbox change).
    void tciServerEnableToggled(bool on, quint16 port);

    // Phase 3J-1 closeout Item 1 (2026-05-12): bind-interface or port
    // changed.  MainWindow restarts the server live if it's running, so
    // the new address/port takes effect without manual toggle.  When the
    // server is stopped, the values just go into AppSettings for next
    // start.  bindAddress is the resolved string ("127.0.0.1", "0.0.0.0",
    // a specific NIC IPv4, "::", "::1", or a specific NIC IPv6).
    void tciServerBindOrPortChanged(const QString& bindAddress, quint16 port);

    // Phase 3J-1 closeout Item 2 (2026-05-12): operator clicked "Show Log...".
    // SetupDialog forwards this up to MainWindow, which owns the lazy-
    // constructed TciLogWindow so the window survives the Setup dialog
    // closing.
    void showLogRequested();

private:
    // Group 1: Server
    QGroupBox*   m_serverGroup{nullptr};  // reference for live title updates
    QCheckBox*   m_enableCheck{nullptr};
    QComboBox*   m_bindAddressCombo{nullptr};  // dropdown of bindable interfaces (Phase 3J-1 Item 1)
    QSpinBox*    m_portSpin{nullptr};
    QPushButton* m_portDefaultBtn{nullptr};
    QCheckBox*   m_sendInitialStateCheck{nullptr};
    QSpinBox*    m_rateLimitSpin{nullptr};
    QPushButton* m_showLogBtn{nullptr};
    QLabel*      m_statusLabel{nullptr};

    // Phase 3J-1 closeout Item 1 (2026-05-12): bind-interface helpers.
    // populateBindAddressCombo() reads QNetworkInterface::allInterfaces() at
    // page-construct time and adds one entry per detected non-loopback IPv4
    // (and IPv6) NIC, plus the well-known options (Loopback / Any IPv4 /
    // Any IPv6).  Each entry's user-data is the bindable address string
    // ("127.0.0.1", "0.0.0.0", "192.168.1.50", "::", "::1", etc.) that
    // matches the AppSettings TciServerBindAddress key format.
    void populateBindAddressCombo();

    // Phase 3J-1 bench fix: live status state.  Updated by setTciServer
    // signal-connected lambdas.  m_clientCount tracks via increment on
    // clientConnected and decrement on clientDisconnected (TciServer
    // exposes connect/disconnect signals but not a count getter — local
    // tracking is the canonical pattern).
    QPointer<class NereusSDR::TciServer> m_tciServerRef;
    bool m_tciServerRunning{false};
    int  m_tciClientCount{0};
    void refreshTciStatusDisplay();

    // Group 2: Compatibility
    QCheckBox*   m_emulateExpertSdr3Check{nullptr};
    QCheckBox*   m_emulateSunSdr2Check{nullptr};
    QCheckBox*   m_cwluBecomesCwCheck{nullptr};
    QCheckBox*   m_cwBecomesCwuCheck{nullptr};

    // Group 3: IQ Stream
    QCheckBox*   m_iqSwapCheck{nullptr};
    QCheckBox*   m_alwaysStreamIqCheck{nullptr};

    // Group 4: Audio Stream
    QSpinBox*    m_audioBlockSpin{nullptr};
    QComboBox*   m_txChannelCombo{nullptr};

    // Group 5: Sensors
    QSpinBox*    m_rxSensorSpin{nullptr};
    QSpinBox*    m_txSensorSpin{nullptr};

    // Group 6: VFO Quirks
    QCheckBox*   m_forgetRx2VfoBCheck{nullptr};
    QCheckBox*   m_useRx1VfoaForRx2Check{nullptr};
    QCheckBox*   m_copyRx2VfobToVfoaCheck{nullptr};

    void buildUI();
    void buildServerGroup();
    void buildCompatibilityGroup();
    void buildIqStreamGroup();
    void buildAudioStreamGroup();
    void buildSensorsGroup();
    void buildVfoQuirksGroup();
};

// ---------------------------------------------------------------------------
// CAT > TCP/IP CAT
// Enable toggle, bind IP, port spinner, status label.
// All controls are NYI / disabled.
// ---------------------------------------------------------------------------
class CatTcpIpPage : public SetupPage {
    Q_OBJECT

public:
    explicit CatTcpIpPage(QWidget* parent = nullptr);

private:
    QCheckBox* m_enableCheck{nullptr};
    QLineEdit* m_bindIpEdit{nullptr};
    QSpinBox*  m_portSpin{nullptr};
    QLabel*    m_statusLabel{nullptr};

    void buildUI();
};

// ---------------------------------------------------------------------------
// CAT > MIDI Control
// Enable toggle, device combo, mapping table placeholder, learn button.
// All controls are NYI / disabled.
// ---------------------------------------------------------------------------
class CatMidiControlPage : public SetupPage {
    Q_OBJECT

public:
    explicit CatMidiControlPage(QWidget* parent = nullptr);

private:
    QCheckBox*  m_enableCheck{nullptr};
    QComboBox*  m_deviceCombo{nullptr};
    QLabel*     m_mappingLabel{nullptr};
    QPushButton* m_learnButton{nullptr};

    void buildUI();
};

} // namespace NereusSDR
