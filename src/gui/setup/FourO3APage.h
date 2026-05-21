#pragma once

// =================================================================
// src/gui/setup/FourO3APage.h  (NereusSDR)
// =================================================================
//
// 4O3A integration setup page.  Settings -> CAT & Network -> 4O3A.
//
// Hosts a QTabWidget with four tabs:
//   1. General        -- master toggle (FourO3A_Enabled), FlexAPI status
//                        row, PGXL/TGXL connection (embedded PeripheralsPage),
//                        PGXL interlock controls (embedded PgxlInterlockPage).
//   2. PowerGenius XL -- embedded PgxlAdvancedPage (identity, telemetry,
//                        fault history).
//   3. Tuner Genius XL-- embedded TgxlAdvancedPage (identity, antennas,
//                        autotune memory, telemetry).
//   4. Diagnostics    -- new FourO3ADiagnosticsTab (FlexAPI listener
//                        state, recent commands, disconnect/reconnect log).
//
// Master toggle behaviour:
//   When OFF (default on first run):
//     - TCP 4992 listener not started by RadioModel ctor.
//     - PGXL/TGXL auto-connect skipped.
//     - Tabs 2/3/4 disabled (greyed out) so the operator can still see
//       the layout but can't interact until they opt in.
//   When ON:
//     - RadioModel::setFourO3AEnabled(true) starts the listener live.
//     - Detail tabs become interactive.
//     - State persisted via AppSettings key "FourO3A_Enabled".
//
// NereusSDR-original page (no Thetis upstream).  Replaces the standalone
// "Peripherals", "PGXL Advanced", and "TGXL Advanced" tree entries that
// previously sat side-by-side under CAT & Network.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-21 -- Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include <QWidget>

class QTabWidget;
class QCheckBox;
class QLabel;

namespace NereusSDR {

class RadioModel;
class PeripheralsPage;
class PgxlInterlockPage;
class PgxlAdvancedPage;
class TgxlAdvancedPage;
class FourO3ADiagnosticsTab;

class FourO3APage : public QWidget {
    Q_OBJECT

public:
    explicit FourO3APage(RadioModel* model, QWidget* parent = nullptr);

private slots:
    // Master toggle handler.  Persists the new state to AppSettings via
    // RadioModel::setFourO3AEnabled, then updates the disabled state of
    // the detail tabs to reflect the new master gate.
    void onMasterToggled(bool checked);

private:
    // Build the General tab content as a composite widget: master toggle
    // group + FlexAPI listener status row + embedded PeripheralsPage +
    // embedded PgxlInterlockPage.  Returns the composite widget; caller
    // adds it as a tab.
    QWidget* buildGeneralTab();

    // Refresh the FlexAPI status row text (port, listening state).  Called
    // on toggle and on a periodic timer so the status stays live.
    void refreshFlexApiStatus();

    // Enable / disable the detail tabs (1, 2, 3) based on master state.
    // Called after onMasterToggled and at construction time.
    void applyMasterGateToTabs(bool enabled);

    RadioModel*           m_model{nullptr};

    // Tab host.
    QTabWidget*           m_tabs{nullptr};

    // General tab controls.
    QCheckBox*            m_masterToggle{nullptr};
    QLabel*               m_flexApiStatusLabel{nullptr};

    // Embedded existing pages.  We keep raw pointers so the master
    // toggle can enable/disable them as a unit.  Owned by the tab
    // widget once added via addTab().
    PeripheralsPage*      m_peripheralsPage{nullptr};
    PgxlInterlockPage*    m_pgxlInterlockPage{nullptr};
    PgxlAdvancedPage*     m_pgxlAdvancedPage{nullptr};
    TgxlAdvancedPage*     m_tgxlAdvancedPage{nullptr};
    FourO3ADiagnosticsTab* m_diagnosticsTab{nullptr};
};

}  // namespace NereusSDR
