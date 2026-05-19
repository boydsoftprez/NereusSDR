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
// =================================================================

#pragma once

#include <QWidget>
#include <QMap>
#include <QString>
#include <QVector>

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
    // Build each section
    void buildIdentitySection(QVBoxLayout* topLay);
    void buildAntennaLabelsSection(QVBoxLayout* topLay);
    void buildNetworkSection(QVBoxLayout* topLay);
    void buildTuneMemorySection(QVBoxLayout* topLay);
    void buildDiagnosticsSection(QVBoxLayout* topLay);
    void buildFaultHistorySection(QVBoxLayout* topLay);
    void buildFooter(QVBoxLayout* topLay);

    // Helpers
    void setPendingState(bool pending);
    void updateConnectionUi(bool connected);

    static QString formatMs(qint64 ms);
    static QString formatBytes(quint64 bytes);

    // -------------------------------------------------------
    RadioModel*            m_model{nullptr};
    ConnectionDiagnostics* m_diagnostics{nullptr};        // owned by this
    FaultLog*              m_faultLog{nullptr};            // owned by this
    TuneMemoryStore*       m_tuneMemoryStore{nullptr};    // owned by this
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

    // Footer
    QPushButton* m_revertBtn{nullptr};
    QPushButton* m_saveAndRebootBtn{nullptr};

    // Guard to prevent signal loops when we populate fields from device responses
    bool m_updatingFromDevice{false};
};

}  // namespace NereusSDR
