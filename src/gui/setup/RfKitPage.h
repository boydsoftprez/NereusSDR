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
class QTabWidget;
class QLabel;

namespace NereusSDR {

class RadioModel;

class RfKitPage : public QWidget {
    Q_OBJECT

public:
    explicit RfKitPage(RadioModel* model, QWidget* parent = nullptr);

    // Testing accessors (used by tst_rfkit_page_master_gate).
    QCheckBox* masterCheckboxForTesting() const { return m_master; }
    bool       detailTabIsEnabledForTesting() const;

private slots:
    // Master toggle handler.  Persists the new state to AppSettings via
    // RadioModel::setRfKitEnabled, then gates the RF2K-S tab.
    void onMasterToggled(bool checked);

    // Refresh the live RF2K-S connection status row (1 Hz timer).
    void refreshLiveStatus();

private:
    // Build the General tab content: master toggle + helper text +
    // live status row.  Returns the composite widget; caller adds it as a tab.
    QWidget* buildGeneralTab();

    // Build the RF2K-S tab placeholder.  Full content in Task 11.
    QWidget* buildRf2ksTab();

    // Enable / disable the RF2K-S detail tab based on master state.
    // Called after onMasterToggled and at construction time.
    void applyMasterGate(bool enabled);

    RadioModel*  m_model{nullptr};

    // Tab host.
    QTabWidget*  m_tabs{nullptr};

    // General tab controls.
    QCheckBox*   m_master{nullptr};
    QLabel*      m_liveStatusLabel{nullptr};

    // RF2K-S tab widget.  Kept so applyMasterGate can locate it by pointer.
    QWidget*     m_rf2ksTab{nullptr};
};

} // namespace NereusSDR
