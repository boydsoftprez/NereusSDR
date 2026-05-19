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

private slots:
    void onPgxlConnected();
    void onPgxlDisconnected();
    void onPgxlStatusUpdated(const QMap<QString, QString>& kvs);
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
    // Build each section
    void buildIdentitySection(QVBoxLayout* topLay);
    void buildHardwareSection(QVBoxLayout* topLay);
    void buildNetworkSection(QVBoxLayout* topLay);
    void buildPairingSection(QVBoxLayout* topLay);
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
    ConnectionDiagnostics* m_diagnostics{nullptr};   // owned by this
    FaultLog*              m_faultLog{nullptr};       // owned by this
    FaultLogTableModel*    m_faultTableModel{nullptr}; // owned by this

    // Pending Save & Reboot tracking
    bool m_pendingSaveReboot{false};

    // Identity section (5.6.1)
    QLineEdit* m_nickname{nullptr};
    QLabel*    m_firmwareVersion{nullptr};
    QLabel*    m_serialLabel{nullptr};
    QLabel*    m_stateBadge{nullptr};
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

    // Pairing section (5.6.4)
    QComboBox*    m_pairModeCombo{nullptr};
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

    // Footer
    QPushButton* m_revertBtn{nullptr};
    QPushButton* m_saveAndRebootBtn{nullptr};

    // Guard to prevent signal loops when we populate fields from device responses
    bool m_updatingFromDevice{false};
};

}  // namespace NereusSDR
