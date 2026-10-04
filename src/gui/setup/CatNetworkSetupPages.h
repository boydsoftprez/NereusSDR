#pragma once
#include "core/cat/CatConfiguration.h"
#include <functional>
#include <vector>
class QFormLayout;

#include "gui/SetupPage.h"
#include "gui/setup/RemoteStationPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QVector>
#include <QWidget>

namespace NereusSDR { class TciServer; }
namespace NereusSDR { class RadioModel; class CatService; }

namespace NereusSDR {

// ---------------------------------------------------------------------------
// CAT > Serial Ports
// Four identical port sections, each with: port combo, baud combo,
// enabled state, exact format, stable bindings and actual transport status.
// ---------------------------------------------------------------------------
// Native controls call the single service configuration writer and block model echoes.
class CatChannelSetupPage : public SetupPage {
public:
    void syncFromModel() override;
protected:
    CatChannelSetupPage(RadioModel*, bool serial, QWidget*);
    bool eventFilter(QObject*, QEvent*) override;
private:
    struct Row {
        QCheckBox* enabled{}; QComboBox* device{}; QComboBox* baud{};
        QComboBox* parity{}; QComboBox* bits{}; QComboBox* stops{};
        QComboBox* primary{}; QComboBox* secondary{};
        QLineEdit* address{}; QSpinBox* port{}; QCheckBox* pty{};
        QLabel* status{}; QLabel* path{};
    };
    Row m_rows[4];
    QPointer<CatService> m_service;
    bool m_serial{false}; bool m_syncing{false};
    void apply(int);
};
class CatSerialPortsPage : public CatChannelSetupPage {
    Q_OBJECT
public:
    explicit CatSerialPortsPage(RadioModel* model = nullptr, QWidget* parent = nullptr);
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
    // Setup.cs:9491-9494 [v2.10.3.13] (TCIClientsConnectedChange setter
    // updates `grpTCIServer.Text`.
    //
    // Pass nullptr to detach (e.g. when the server is destroyed); the page
    // tracks the pointer via QPointer so a stale connection is harmless.
    //
    // Idempotent: re-calls with the same pointer just refresh the snapshot.
    void setTciServer(class NereusSDR::TciServer* server);

    // R-R3-48: the window's RadioModel, so the page can show the Core's
    // station TCI server ("Also at the station: <address>, port <port>").
    void setRadioModel(class NereusSDR::RadioModel* model);
    QString stationLineForTesting() const;
    bool switchOnForTesting() const;
    int portForTesting() const;
    QSpinBox* portSpinForTesting() const { return m_portSpin; }

signals:
    // Emitted when the operator toggles the Enable TCI Server checkbox.
    // Phase 3J-1 review P2.4: MainWindow::wireSetupDialog connects this to
    // the live start/stop path so the server starts or stops immediately
    // without a disconnect/reconnect cycle.
    // `on`:   true means start the server on the persisted port.
    // `port`: the port currently shown in the Port spinbox (persisted at emit
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
    // exposes connect/disconnect signals but not a count getter; local
    // tracking is the canonical pattern.
    QPointer<class NereusSDR::TciServer> m_tciServerRef;
    bool m_tciServerRunning{false};
    int  m_tciClientCount{0};
    void refreshTciStatusDisplay();
    // R-R3-48: the Core's station TCI server line.
    QPointer<class NereusSDR::RadioModel> m_radioModelRef;
    QLabel* m_stationLine{nullptr};
    void refreshStationLine();
    void refreshIqStreamGroup();
    void reloadSwitchFromSettings();
    QGroupBox* m_coreGroup{nullptr};
    QLabel* m_coreBind{nullptr};
    QLabel* m_coreReason{nullptr};
    QCheckBox* m_coreExpert{nullptr};
    QCheckBox* m_coreSunSdr{nullptr};
    QCheckBox* m_coreCwlu{nullptr};
    QCheckBox* m_coreInitial{nullptr};
    void refreshCoreGroup();
    void sendCoreOptions();
    // JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1): the rest of
    // the page's settings for the Core's own server, by property name.
    QHash<QByteArray, QWidget*> m_coreSettings;
    QHash<QByteArray, QString> m_coreSettingTips;
    void sendCoreSetting(const QByteArray& name, const QVariant& value);

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

    // The RX2 VFO options' captions and tooltips, shared by this window's
    // group and the Core's rows.
    static QString rx2VfoForgetLabel();
    static QString rx2VfoForgetTip();
    static QString rx2VfoUseRx1Label();
    static QString rx2VfoUseRx1Tip();
    static QString rx2VfoCopyLabel();
    static QString rx2VfoCopyTip();

    void buildUI();
    void buildServerGroup();
    void buildCoreGroup();
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
class CatTcpIpPage : public CatChannelSetupPage {
    Q_OBJECT
public:
    explicit CatTcpIpPage(RadioModel* model = nullptr, QWidget* parent = nullptr);
};
class CatGlobalSetupPage : public SetupPage {
public:
    void syncFromModel() override;
protected:
    CatGlobalSetupPage(const QString&, RadioModel*, QWidget*);
    bool eventFilter(QObject*, QEvent*) override;
    QCheckBox* addCheck(QFormLayout*, const QString&, const QString&, bool CatGlobalConfig::*);
    QComboBox* addChoice(QFormLayout*, const QString&, const QString&, const QStringList&, QString CatGlobalConfig::*);
    QLineEdit* addText(QFormLayout*, const QString&, const QString&, QString CatGlobalConfig::*);
    QSpinBox* addNumber(QFormLayout*, const QString&, const QString&, int, int, int CatGlobalConfig::*);
    QPointer<CatService> m_service;
    QLabel* m_status{};
    std::vector<std::function<void(const CatGlobalConfig&)>> m_updates;
    void applyConfiguration(const CatGlobalConfig&);
    bool m_syncing{false};
};
class CatOptionsSetupPage : public CatGlobalSetupPage {
    Q_OBJECT
public:
    explicit CatOptionsSetupPage(RadioModel*, QWidget* parent = nullptr);
signals:
    void showLogRequested();
};
class CatPttSetupPage : public CatGlobalSetupPage {
    Q_OBJECT
public:
    explicit CatPttSetupPage(RadioModel*, QWidget* parent = nullptr);
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

// ---------------------------------------------------------------------------
// Network > Peripherals
// Two-row grid: TGXL (port 9010) and PGXL (port 9008).
// Six columns per row: Name, Host IP, Port, Scan LAN, Connect, Status.
// AppSettings keys: TGXL_ManualIp, TGXL_ManualPort, PGXL_ManualIp,
//                   PGXL_ManualPort.
// Phase 3P-II Task 17; Task 63 adds live status-label signal wiring.
// ---------------------------------------------------------------------------
class PeripheralsPage : public QWidget {
    Q_OBJECT

public:
    explicit PeripheralsPage(RadioModel* model, QWidget* parent = nullptr);
    ~PeripheralsPage() override;

    // Group B fix wave (M1): whether the page still has its RadioModel.
    // False once the model is gone, so nothing asks it again.
    bool hasModelForTest() const { return !m_model.isNull(); }

protected:
    // R-R3-49 (parity Task 8): Setup closing (or the tab changing) sends a
    // remote window's unsent Tuner Genius Host or Port to the Core
    // (parity Task 9: and the Power Genius's).
    void hideEvent(QHideEvent* event) override;

private slots:
    void onScanLan(int rowIdx);
    void onConnect(int rowIdx);

private:
    // Build one device row into m_grid at the given row index.
    // name        - display label ("Tuner Genius XL" / "Power Genius XL")
    // ipKey       - AppSettings key for the host IP ("TGXL_ManualIp" / "PGXL_ManualIp")
    // portKey     - AppSettings key for the port ("TGXL_ManualPort" / "PGXL_ManualPort")
    // defaultPort - factory default (9010 / 9008)
    void buildRow(int row, const QString& name,
                  const QString& ipKey, const QString& portKey,
                  quint16 defaultPort);

    // Phase 3P-II Task 63: connect PgxlConnection + TgxlConnection signals
    // to m_statusLabels[1] (PGXL) and m_statusLabels[0] (TGXL) respectively.
    // Called at end of constructor after both rows are built.
    void wireStatusSignals();
    void refreshRemoteTgxlRow();
    // R-R3-47 / R-R3-22: the Power Genius row in a remote window, a view of
    // the Core's `amplifier` object plus the Core's PGXL commands.
    void refreshRemotePgxlRow();
    bool isRemoteMode() const;

    // Group B fix wave (M1): held weakly. RadioModel is MainWindow's first
    // child, so it goes before a Setup dialog still open at quit, and the
    // destructor's unsent-address flush must then find it gone.
    QPointer<RadioModel> m_model;
    QGridLayout*  m_grid{nullptr};

    // Per-row status labels; indexed by row (0 = TGXL, 1 = PGXL).
    QVector<QLabel*>       m_statusLabels;
    // Per-row Connect buttons; label toggles "Connect" / "Disconnect".
    QVector<QPushButton*>  m_connectBtns;

    // The endpoint Core last reported.  Keep this independently of the edit
    // controls so a phase/error/identity update cannot replace an operator's
    // unsent remote-TGXL draft.
    QString m_lastDisplayedCoreTgxlHost;
    quint16 m_lastDisplayedCoreTgxlPort{0};
    QString m_lastDisplayedCorePgxlHost;
    quint16 m_lastDisplayedCorePgxlPort{0};

    // R-R3-49 (parity Task 8, remoteTgxlControlVersion 4): a Host or Port
    // the operator typed in a remote window without pressing Connect goes
    // to the Core (setTgxlAddress) when editing finishes or Setup closes.
    void sendRemoteTgxlAddress();
    bool m_tgxlAddressEdited{false};
    bool m_fillingTgxlFromCore{false};
    // R-R3-49 (parity Task 9, remotePgxlControlVersion 4): the same for the
    // Power Genius row (setPgxlAddress).
    void sendRemotePgxlAddress();
    bool m_pgxlAddressEdited{false};
    bool m_fillingPgxlFromCore{false};
};

} // namespace NereusSDR
