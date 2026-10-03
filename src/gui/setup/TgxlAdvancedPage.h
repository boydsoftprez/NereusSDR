// =================================================================
// src/gui/setup/TgxlAdvancedPage.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native Setup -> Network -> TGXL Advanced page.
// Six-section scrolling page for Tuner Genius XL device management:
//   5.7.1 Identity & Status
//   5.7.2 Antenna Labels (TGXL-specific; no PGXL equivalent)
//   5.7.3 Network (DHCP / IP / netmask / gateway)
//   5.7.4 Tune Memory Management (TuneMemoryStore table + auto-recall toggle)
//   5.7.5 Diagnostics (8-cell 1 Hz grid)
//   5.7.6 Fault History (table + Clear All)
//
// Design reference:
//   docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md
//   section 5.7 and footer.
//
// Parallel to PgxlAdvancedPage (Task 78). Same class shape, same
// buildXxxSection methods, same status-badge styling, same
// diagnostics-grid pattern. Key differences:
//   - Section 2 is Antenna Labels (TGXL-only)
//   - Section 4 is Tune Memory Management (TGXL-only)
//   - No separate Hardware section (TGXL has no bias/fan/LED equivalents)
//   - Diagnostics binds to m_model->tgxlConnection()
//   - FaultLog uses "TGXL_FaultHistory" key
//
// AI tooling: Anthropic Claude Code.
//
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: RadioModel's counters;
//                                    a remote window's view of the Core's
//                                    antenna names, tune memory, counters and
//                                    fault history. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: every section in a
//                                    remote window: the tuner's own
//                                    settings through the Core, with the
//                                    local page's confirmations and the
//                                    tuner's answers. AI-assisted via
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
class QTableView;
class QScrollArea;
class QVBoxLayout;
class QGridLayout;

namespace NereusSDR {

class RadioModel;
class ConnectionDiagnostics;
class FaultLog;
class TuneMemoryStore;
class TgxlFaultLogTableModel;   // forward-declared; defined in .cpp
class TuneMemoryTableModel;     // forward-declared; defined in .cpp

class TgxlAdvancedPage : public QWidget {
    Q_OBJECT
public:
    explicit TgxlAdvancedPage(RadioModel* model, QWidget* parent = nullptr);
    ~TgxlAdvancedPage() override;

    // Test seams (R-R3-47).
    int faultRowCountForTesting() const;
    QString faultTextForTesting(int row) const;
    QString reconnectCountTextForTesting() const;
    QString antennaLabelForTesting(int index) const;
    int tuneMemoryRowCountForTesting() const;
    QPushButton* clearFaultsButtonForTesting() const { return m_clearFaultsBtn; }
    // R-R3-47 / R-R3-22: the tuner's own settings in a remote window.
    QLineEdit* nicknameEditForTesting() const { return m_nickname; }
    QCheckBox* dhcpCheckForTesting() const { return m_dhcpCheck; }
    QLineEdit* ipEditForTesting() const { return m_ipEdit; }
    QLineEdit* netmaskEditForTesting() const { return m_netmaskEdit; }
    QLineEdit* gatewayEditForTesting() const { return m_gatewayEdit; }
    QPushButton* applyNetworkButtonForTesting() const { return m_applyIfconfBtn; }
    QPushButton* revertButtonForTesting() const { return m_revertBtn; }
    QPushButton* saveAndRebootButtonForTesting() const { return m_saveAndRebootBtn; }
    QString firmwareTextForTesting() const;
    QString variantTextForTesting() const;
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

signals:
    // Phase 3P-II Phase 4 Task 95: emitted when the operator edits an antenna
    // label and presses Enter / moves focus.  index is 1..3; label is the new
    // text (may be empty, meaning "reset to default ANT N").
    // Forwarded by SetupDialog::tgxlAntennaLabelChanged to TunerApplet.
    void antennaLabelChanged(int index, const QString& label);

private slots:
    void onTgxlConnected();
    void onTgxlDisconnected();
    void onTgxlStatusUpdated(const QMap<QString, QString>& kvs);
    void onSetupResponse(const QMap<QString, QString>& fields);
    void onIfconfResponse(const QMap<QString, QString>& fields);
    void onDiagnosticsChanged();
    void onTuneMemoryChanged();
    void onSaveAndReboot();
    void onRevert();

    // Antenna labels section
    void onAnt1LabelEdited();
    void onAnt2LabelEdited();
    void onAnt3LabelEdited();

    // Network section
    void onDhcpToggled(bool checked);
    void onApplyIfconf();

private:
    // Setup description version 15: the described widgets' ids.
    void applySetupIds();
    // Build each section
    void buildIdentitySection(QVBoxLayout* topLay);
    void buildAntennaLabelsSection(QVBoxLayout* topLay);
    void buildNetworkSection(QVBoxLayout* topLay);
    void buildTuneMemorySection(QVBoxLayout* topLay);
    void buildDiagnosticsSection(QVBoxLayout* topLay);
    // 2026-05-22 menu cleanup: buildFaultHistorySection removed (always-
    // empty table; no producer ever populated tgxlFaultLog). R-R3-47:
    // restored for a remote window, where the Core records them.
    void buildFaultHistorySection(QVBoxLayout* topLay);
    void buildFooter(QVBoxLayout* topLay);
    // R-R3-47 / R-R3-22: a remote window.
    bool isRemote() const;
    void refreshRemote();
    bool deviceSettingsAvailable() const;
    bool remoteTunerConnected() const;
    void refreshRemoteIdentity();
    void refreshRemoteDevice();
    void updateRemoteControls();
    void showRemoteOutcome(bool sent, const QString& reason);
    bool confirmRemote(const QString& title, const QString& text);

    // Helpers
    void setPendingState(bool pending);
    void updateConnectionUi(bool connected);

    static QString formatMs(qint64 ms);
    static QString formatBytes(quint64 bytes);

    // -------------------------------------------------------
    RadioModel*            m_model{nullptr};
    // R-R3-47: RadioModel's (non-owning) when the page has a model.
    ConnectionDiagnostics* m_diagnostics{nullptr};
    // Phase 3P-II Phase 4 Task 94: non-owning when m_model != nullptr (RadioModel owns
    // the shared instance); falls back to a local QWidget-parented instance in tests.
    FaultLog*              m_faultLog{nullptr};
    // Phase 3P-II Phase 4 Task 89: TuneMemoryStore is now owned by RadioModel
    // (shared instance accessed via m_model->tuneMemoryStore()). This pointer
    // is non-owning; do not delete it.
    TuneMemoryStore*       m_tuneMemoryStore{nullptr};    // non-owning; from RadioModel
    TgxlFaultLogTableModel* m_faultTableModel{nullptr};   // owned by this
    TuneMemoryTableModel*  m_tuneMemTableModel{nullptr};  // owned by this

    // Pending Save & Reboot tracking
    bool m_pendingSaveReboot{false};

    // Identity section (5.7.1)
    QLineEdit* m_nickname{nullptr};
    QLabel*    m_firmwareVersion{nullptr};
    QLabel*    m_serialLabel{nullptr};
    QLabel*    m_stateBadge{nullptr};
    QLabel*    m_variantLabel{nullptr};    // 3x1 vs 1x1

    // Antenna labels section (5.7.2)
    QLineEdit* m_ant1Label{nullptr};
    QLineEdit* m_ant2Label{nullptr};
    QLineEdit* m_ant3Label{nullptr};

    // Network section (5.7.3)
    QCheckBox*   m_dhcpCheck{nullptr};
    QLineEdit*   m_ipEdit{nullptr};
    QLineEdit*   m_netmaskEdit{nullptr};
    QLineEdit*   m_gatewayEdit{nullptr};
    QPushButton* m_applyIfconfBtn{nullptr};
    // I5: why a network setting was not sent (both windows), else hidden.
    QLabel*      m_networkProblem{nullptr};

    // Tune memory section (5.7.4)
    QTableView* m_tuneMemTable{nullptr};
    QCheckBox*  m_autoRecallCheck{nullptr};

    // Diagnostics section (5.7.5) value labels
    QLabel* m_uptimeLabel{nullptr};
    QLabel* m_rttLabel{nullptr};
    QLabel* m_keepaliveMissedLabel{nullptr};
    QLabel* m_reconnectCountLabel{nullptr};
    QLabel* m_framesInLabel{nullptr};
    QLabel* m_framesOutLabel{nullptr};
    QLabel* m_bytesInLabel{nullptr};
    QLabel* m_bytesOutLabel{nullptr};

    // Fault history section (5.7.6)
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
