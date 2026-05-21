// =================================================================
// src/gui/setup/FourO3APage.cpp  (NereusSDR)
// =================================================================
//
// See FourO3APage.h for the design overview.  Implementation notes:
//
//   - The General tab is hand-built (master toggle + status row +
//     embedded pages).  The other 3 tabs simply addTab() an existing
//     QWidget subclass; the master toggle gates whether they're
//     interactive via QTabWidget::setTabEnabled.
//
//   - FlexAPI status refresh runs on a 1 Hz timer so the listening
//     status reflects live changes (e.g. after the master toggle
//     starts/stops the listener).
//
//   - PeripheralsPage and PgxlInterlockPage already manage their own
//     state through AppSettings + RadioModel signals.  They drop in
//     unchanged.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-21 -- Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include "FourO3APage.h"

#include "CatNetworkSetupPages.h"   // PeripheralsPage
#include "PgxlInterlockPage.h"
#include "PgxlAdvancedPage.h"
#include "TgxlAdvancedPage.h"
#include "FourO3ADiagnosticsTab.h"

#include "core/SmartSdrApiListener.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace NereusSDR {

FourO3APage::FourO3APage(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    m_tabs = new QTabWidget(this);
    root->addWidget(m_tabs);

    // Tab 1: General.  Hand-built composite that hosts the master
    // toggle, FlexAPI status row, and embedded Peripherals / PGXL
    // interlock pages.
    m_tabs->addTab(buildGeneralTab(), tr("General"));

    // Tab 2: PowerGenius XL.  Embed the existing PgxlAdvancedPage as-is;
    // its content is already structured per the mockup (identity,
    // operation, telemetry, fault history).  Page constructor takes
    // a RadioModel pointer so it can wire to PgxlConnection signals.
    m_pgxlAdvancedPage = new PgxlAdvancedPage(m_model);
    m_tabs->addTab(m_pgxlAdvancedPage, tr("PowerGenius XL"));

    // Tab 3: Tuner Genius XL.  Same pattern as Tab 2.
    m_tgxlAdvancedPage = new TgxlAdvancedPage(m_model);
    m_tabs->addTab(m_tgxlAdvancedPage, tr("Tuner Genius XL"));

    // Tab 4: Diagnostics.  New widget exposing FlexAPI listener
    // state, recent commands, and disconnect/reconnect log.
    m_diagnosticsTab = new FourO3ADiagnosticsTab(m_model);
    m_tabs->addTab(m_diagnosticsTab, tr("Diagnostics"));

    // Apply current master-gate state to the detail tabs at
    // construction time so a cold-open with FourO3A_Enabled=False
    // shows the tabs greyed out.
    const bool initialEnabled = m_model && m_model->fourO3AEnabled();
    applyMasterGateToTabs(initialEnabled);

    // Periodic FlexAPI status refresh (1 Hz).  Captures live state
    // changes including post-toggle start/stop and any external
    // listener errors.
    auto* statusTimer = new QTimer(this);
    statusTimer->setInterval(1000);
    connect(statusTimer, &QTimer::timeout,
            this, &FourO3APage::refreshFlexApiStatus);
    statusTimer->start();
    refreshFlexApiStatus();  // initial paint
}

QWidget* FourO3APage::buildGeneralTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(12);

    // ── Master Switch ─────────────────────────────────────────────
    auto* masterBox = new QGroupBox(tr("Master Switch"), tab);
    auto* masterLayout = new QVBoxLayout(masterBox);
    m_masterToggle = new QCheckBox(tr("Enable 4O3A integration"), masterBox);
    m_masterToggle->setToolTip(
        tr("Gates the FlexAPI listener on TCP 4992 and the PGXL / TGXL "
           "auto-connect paths.  Off by default; turn on only when you "
           "want NereusSDR to expose itself to 4O3A amps and tuners on "
           "your local network."));
    m_masterToggle->setChecked(m_model && m_model->fourO3AEnabled());
    connect(m_masterToggle, &QCheckBox::toggled,
            this, &FourO3APage::onMasterToggled);
    masterLayout->addWidget(m_masterToggle);

    auto* masterHelp = new QLabel(
        tr("When enabled: FlexAPI listener binds TCP 4992; PowerGenius XL "
           "and Tuner Genius XL pages become interactive; PGXL/TGXL "
           "auto-connect runs at startup.  When disabled: no port is "
           "bound, no outbound connection attempts, and the detail tabs "
           "below are greyed out."),
        masterBox);
    masterHelp->setWordWrap(true);
    masterHelp->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    masterLayout->addWidget(masterHelp);
    layout->addWidget(masterBox);

    // ── FlexAPI Listener Status ───────────────────────────────────
    auto* statusBox = new QGroupBox(tr("FlexAPI Listener"), tab);
    auto* statusLayout = new QHBoxLayout(statusBox);
    m_flexApiStatusLabel = new QLabel(tr("Status: \xE2\x97\x8B Idle"), statusBox);
    m_flexApiStatusLabel->setToolTip(
        tr("Live state of the TCP 4992 SmartSDR API listener.  Reflects "
           "the master toggle above plus any external bind errors."));
    statusLayout->addWidget(m_flexApiStatusLabel);
    statusLayout->addStretch();
    layout->addWidget(statusBox);

    // ── PGXL / TGXL Connection ────────────────────────────────────
    // PeripheralsPage already implements the per-row IP / port /
    // Connect button pattern; embedding it preserves its existing
    // AppSettings wire-up and PgxlConnection / TgxlConnection signal
    // bindings without duplication.
    m_peripheralsPage = new PeripheralsPage(m_model, tab);
    layout->addWidget(m_peripheralsPage);

    // ── PGXL Interlock ────────────────────────────────────────────
    m_pgxlInterlockPage = new PgxlInterlockPage(m_model, tab);
    layout->addWidget(m_pgxlInterlockPage);

    layout->addStretch();
    return tab;
}

void FourO3APage::onMasterToggled(bool checked)
{
    if (!m_model) {
        return;
    }
    m_model->setFourO3AEnabled(checked);
    applyMasterGateToTabs(checked);
    refreshFlexApiStatus();  // immediate paint so the dot reflects new state

    // Greying behaviour also applies to the embedded sections on the
    // General tab; the master toggle stays enabled but everything
    // below it disables when the gate is off.  Keeps the master
    // toggle reachable so the operator can flip it back on.
    if (m_peripheralsPage) { m_peripheralsPage->setEnabled(checked); }
    if (m_pgxlInterlockPage) { m_pgxlInterlockPage->setEnabled(checked); }
}

void FourO3APage::applyMasterGateToTabs(bool enabled)
{
    if (!m_tabs) { return; }
    // Tab 0 (General) stays enabled so the master toggle is always
    // reachable; tabs 1, 2, 3 (PowerGenius XL / Tuner Genius XL /
    // Diagnostics) gate on the master state.
    for (int i = 1; i < m_tabs->count(); ++i) {
        m_tabs->setTabEnabled(i, enabled);
    }
    // Also propagate to General-tab embedded sections so the IP/port
    // grid + interlock controls grey out (kept visible for context).
    if (m_peripheralsPage) { m_peripheralsPage->setEnabled(enabled); }
    if (m_pgxlInterlockPage) { m_pgxlInterlockPage->setEnabled(enabled); }
}

void FourO3APage::refreshFlexApiStatus()
{
    if (!m_flexApiStatusLabel) { return; }
    SmartSdrApiListener* listener =
        m_model ? m_model->smartSdrListener() : nullptr;
    const bool listening = listener && listener->isListening();
    if (listening) {
        m_flexApiStatusLabel->setText(
            tr("Status: \xE2\x97\x8F Listening on TCP 4992"));
        m_flexApiStatusLabel->setStyleSheet(
            QStringLiteral("color: #4CAF50;"));  // green
    } else {
        const bool gateOn = m_model && m_model->fourO3AEnabled();
        if (gateOn) {
            // Master is ON but the listener isn't bound -- bind failure
            // (e.g. port in use).  Flag it red so the operator notices.
            m_flexApiStatusLabel->setText(
                tr("Status: \xE2\x97\x8F TCP 4992 bind failed"));
            m_flexApiStatusLabel->setStyleSheet(
                QStringLiteral("color: #E53935;"));  // red
        } else {
            m_flexApiStatusLabel->setText(
                tr("Status: \xE2\x97\x8B Disabled "
                   "(toggle 'Enable 4O3A integration' above to start)"));
            m_flexApiStatusLabel->setStyleSheet(
                QStringLiteral("color: #888;"));   // grey
        }
    }
}

}  // namespace NereusSDR
