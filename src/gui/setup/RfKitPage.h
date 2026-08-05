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
// =================================================================

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
};

} // namespace NereusSDR
