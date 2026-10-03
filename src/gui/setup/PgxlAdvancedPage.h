// =================================================================
// src/gui/setup/PgxlAdvancedPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native Setup -> Network -> PGXL Advanced page.
// Six-section scrolling page for Power Genius XL device management:
//   5.6.1 Identity & Status
//   5.6.2 Hardware (requires Save & Reboot)
//   5.6.3 Network (DHCP / IP / netmask / gateway)
//   5.6.4 Pairing & band source
//   5.6.5 Diagnostics (8-cell 1 Hz grid)
//   5.6.6 Fault History (table + Clear All)
//
// Design reference:
//   docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md
//   sections 5.6.1 through 5.6.6 and footer.
//
// AI tooling: Anthropic Claude Code.
//
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: RadioModel's counters;
//                                    a remote window's view of the Core's
//                                    output limit, counters and fault
//                                    history with its commands. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: every section in a
//                                    remote window: the amp's own settings
//                                    through the Core, with the local
//                                    page's confirmations and the amp's
//                                    answers. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 9, operator
//                                    amendment 2026-09-25): the local tab's
//                                    Operate button. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#pragma once

#include <QWidget>
#include <QMap>
#include <QString>
#include <QVector>

#include <functional>

class QLineEdit;
class QLabel;
class QPushButton;
class QCheckBox;
class QComboBox;
class QSlider;
class QSpinBox;
class QRadioButton;
class QTableView;
class QScrollArea;
class QVBoxLayout;
class QGridLayout;

namespace NereusSDR {

class RadioModel;
class ConnectionDiagnostics;
class FaultLog;
class FaultLogTableModel;   // forward-declared; defined in .cpp

class PgxlAdvancedPage : public QWidget {
    Q_OBJECT
public:
    explicit PgxlAdvancedPage(RadioModel* model, QWidget* parent = nullptr);
    ~PgxlAdvancedPage() override;

    // Test seams (R-R3-47).
    int faultRowCountForTesting() const;
    QString faultTextForTesting(int row) const;
    QString reconnectCountTextForTesting() const;
    QString remoteNoteForTesting() const;
    QCheckBox* powerCapCheckForTesting() const { return m_powerCapCheck; }
    QSpinBox* powerCapSpinForTesting() const { return m_powerCapSpin; }
    QPushButton* clearFaultsButtonForTesting() const { return m_clearFaultsBtn; }
    // R-R3-47 / R-R3-22: the amp's own settings in a remote window.
    QLineEdit* nicknameEditForTesting() const { return m_nickname; }
    QRadioButton* biasClassAForTesting() const { return m_biasClassA; }
    QComboBox* fanModeComboForTesting() const { return m_fanModeCombo; }
    QSlider* ledSliderForTesting() const { return m_ledSlider; }
    QCheckBox* dhcpCheckForTesting() const { return m_dhcpCheck; }
    QLineEdit* ipEditForTesting() const { return m_ipEdit; }
    QLineEdit* netmaskEditForTesting() const { return m_netmaskEdit; }
    QLineEdit* gatewayEditForTesting() const { return m_gatewayEdit; }
    QPushButton* applyNetworkButtonForTesting() const { return m_applyIfconfBtn; }
    QPushButton* revertButtonForTesting() const { return m_revertBtn; }
    QPushButton* saveAndRebootButtonForTesting() const { return m_saveAndRebootBtn; }
    QCheckBox* pairAttemptCheckForTesting() const { return m_pairAttemptCheckbox; }
    // R-R3-49 (parity Task 9): the local tab's Operate button.
    QPushButton* operateButtonForTesting() const { return m_operateBtn; }
    QString firmwareTextForTesting() const;
    QString deviceAnswerForTesting() const;
    QString networkProblemForTesting() const;
    /// Answer the page's confirmations instead of showing them; `ask`
    /// receives the title and the words the dialog would show.
    void setConfirmationForTesting(std::function<bool(const QString&, const QString&)> ask)
    { m_confirmForTesting = std::move(ask); }
    /// The Network section's warning in a local window.
    static QString networkWarningText();
    /// M4 / operator decision 2026-09-24: the question before Apply
    /// Network Settings, asked in local and remote windows alike (and the
    /// remote Network section's warning), in plain words true in both.
    static QString networkQuestionText();

private slots:
    void onPgxlConnected();
    void onPgxlDisconnected();
    void onPgxlStatusUpdated(const QMap<QString, QString>& kvs);
    // R-R3-49 (parity Task 9): the local tab's Operate (this computer's
    // PgxlConnection, the local applet's line).
    void updateOperateButton();
    void onOperateClicked();
    void onSetupResponse(const QMap<QString, QString>& fields);
    void onIfconfResponse(const QMap<QString, QString>& fields);
    void onDiagnosticsChanged();
    void onSaveAndReboot();
    void onRevert();

    // Hardware section
    void onBiasModeChanged();
    void onFanModeChanged(int index);
    void onLedSliderChanged(int value);
    void onPowerCapToggled(bool checked);
    void onPowerCapWattsChanged(int watts);

    // Network section
    void onDhcpToggled(bool checked);
    void onApplyIfconf();

    // Pairing section
    void onPairModeChanged(int index);
    void onTxAntChanged();
    void onSliceBindingChanged();

private:
    // Setup description version 15: the described widgets' ids.
    void applySetupIds();
    // Build each section
    void buildIdentitySection(QVBoxLayout* topLay);
    void buildHardwareSection(QVBoxLayout* topLay);
    void buildNetworkSection(QVBoxLayout* topLay);
    void buildPairingSection(QVBoxLayout* topLay);
    void buildDiagnosticsSection(QVBoxLayout* topLay);
    void buildFaultHistorySection(QVBoxLayout* topLay);
    void buildFooter(QVBoxLayout* topLay);

    // R-R3-47 / R-R3-22: a remote window.
    bool isRemote() const;
    bool remoteDataAvailable() const;
    void buildRemoteSections(QVBoxLayout* topLay);
    void refreshRemote();
    void sendRemotePowerCap();
    bool deviceSettingsAvailable() const;
    bool remoteAmpConnected() const;
    void refreshRemoteIdentity();
    void refreshRemoteDevice();
    void updateRemoteControls();
    void showRemoteOutcome(bool sent, const QString& reason);
    void sendRemoteHardware(const QString& setting, const QString& value);
    bool confirmRemote(const QString& title, const QString& text);

    // Helpers
    void setPendingState(bool pending);
    void updateConnectionUi(bool connected);

    static QString formatMs(qint64 ms);
    static QString formatBytes(quint64 bytes);

    // -------------------------------------------------------
    RadioModel*            m_model{nullptr};
    // R-R3-47: RadioModel's (non-owning) when the page has a model; owned
    // by this only without one.
    ConnectionDiagnostics* m_diagnostics{nullptr};
    // Phase 3P-II Phase 4 Task 94: non-owning when m_model != nullptr (RadioModel owns
    // the shared instance); falls back to a local QWidget-parented instance in tests.
    FaultLog*              m_faultLog{nullptr};
    FaultLogTableModel*    m_faultTableModel{nullptr}; // owned by this

    // Pending Save & Reboot tracking
    bool m_pendingSaveReboot{false};

    // Identity section (5.6.1)
    QLineEdit* m_nickname{nullptr};
    QLabel*    m_firmwareVersion{nullptr};
    QLabel*    m_serialLabel{nullptr};
    QLabel*    m_stateBadge{nullptr};
    QPushButton* m_operateBtn{nullptr};
    QLabel*    m_meffaLabel{nullptr};

    // Hardware section (5.6.2)
    QLabel*      m_hwPendingLabel{nullptr};
    QRadioButton* m_biasClassA{nullptr};
    QRadioButton* m_biasClassAB{nullptr};
    QComboBox*   m_fanModeCombo{nullptr};
    QSlider*     m_ledSlider{nullptr};
    QLabel*      m_ledValueLabel{nullptr};
    QCheckBox*   m_powerCapCheck{nullptr};
    QSpinBox*    m_powerCapSpin{nullptr};

    // Network section (5.6.3)
    QCheckBox* m_dhcpCheck{nullptr};
    QLineEdit* m_ipEdit{nullptr};
    QLineEdit* m_netmaskEdit{nullptr};
    QLineEdit* m_gatewayEdit{nullptr};
    QPushButton* m_applyIfconfBtn{nullptr};
    // I5: why a network setting was not sent (both windows), else hidden.
    QLabel*      m_networkProblem{nullptr};

    // Pairing section (5.6.4).
    // 2026-05-22 menu cleanup: was QComboBox with 3 entries (flexradio /
    // amplifier / none). RadioModel only ever read the derived
    // PGXL_PairAttempt boolean (lines 9654-9655) -- "flexradio" and
    // "amplifier" were functionally identical, only "none" actually did
    // anything different. Replaced with a single Auto-pair checkbox
    // backed by PGXL_PairAttempt directly. PGXL_PairMode is no longer
    // written; the stored key is left in place for backward compat (any
    // existing settings file values are ignored on read).
    QCheckBox*    m_pairAttemptCheckbox{nullptr};
    QRadioButton* m_txAntAnt1{nullptr};
    QRadioButton* m_txAntAnt2{nullptr};
    QRadioButton* m_sliceA{nullptr};
    QRadioButton* m_sliceB{nullptr};

    // Diagnostics section (5.6.5) value labels
    QLabel* m_uptimeLabel{nullptr};
    QLabel* m_rttLabel{nullptr};
    QLabel* m_keepaliveMissedLabel{nullptr};
    QLabel* m_reconnectCountLabel{nullptr};
    QLabel* m_framesInLabel{nullptr};
    QLabel* m_framesOutLabel{nullptr};
    QLabel* m_bytesInLabel{nullptr};
    QLabel* m_bytesOutLabel{nullptr};

    // Fault history section (5.6.6)
    QTableView* m_faultTable{nullptr};

    // R-R3-47: remote window only.
    QLabel*      m_remoteNote{nullptr};
    QPushButton* m_clearFaultsBtn{nullptr};
    QLabel*      m_deviceAnswer{nullptr};
    std::function<bool(const QString&, const QString&)> m_confirmForTesting;

    // Footer
    QPushButton* m_revertBtn{nullptr};
    QPushButton* m_saveAndRebootBtn{nullptr};

    // Guard to prevent signal loops when we populate fields from device responses
    bool m_updatingFromDevice{false};
};

}  // namespace NereusSDR
