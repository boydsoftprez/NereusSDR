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
//   2026-09-24 -- R-R3-47 / R-R3-22: a remote window's Power Genius XL
//                 tab is a view of the Core's `amplifier` object plus the
//                 Core's PGXL commands. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-24 -- R-R3-48: the Power Genius's band-follow line, local and
//                 remote. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-24 -- R-R3-47 / R-R3-22: in a remote window the Power Genius
//                 tab adds the Core's output limit, counters and fault
//                 history, the Tuner Genius tab shows the Core's antenna
//                 names, tune memory, counters and faults, and the General
//                 tab's interlock section shows and changes the Core's
//                 policy. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-25 -- R-R3-49 (parity Task 8): selectTab. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 -- R-R3-49 (parity Task 9): the remote Power Genius tab's
//                 Operate button asks the Core (setPgxlOperate,
//                 remotePgxlControlVersion 4), reads Operate or Standby
//                 from the amp's reported state and waits while the radio
//                 is on the air. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-26 -- iPhone app plan Task 77 fix round 3 (R-IOS-02, R-IOS-03,
//                 R-IOS-13): that Operate also waits while the Core's
//                 Tuner Genius tunes, and a faulted amp is offered Standby.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 -- R-R3-49 / R-IOS-18: Setup description version 15 ids.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "FourO3APage.h"

#include "CatNetworkSetupPages.h"   // PeripheralsPage
#include "PgxlInterlockPage.h"
#include "PgxlAdvancedPage.h"
#include "TgxlAdvancedPage.h"

#include "core/AppSettings.h"
#include "core/SmartSdrApiListener.h"
#include "core/session/IStationLink.h"
#include "gui/OperatorReasonText.h"
#include "models/AmplifierModel.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace NereusSDR {

void FourO3APage::selectTab(Tab tab)
{
    if (m_tabs) {
        m_tabs->setCurrentIndex(static_cast<int>(tab));
    }
}

FourO3APage::Tab FourO3APage::currentTabForTesting() const
{
    return static_cast<Tab>(m_tabs ? m_tabs->currentIndex() : 0);
}

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
    if (m_model && m_model->role() == RadioModel::Role::Remote) {
        // Advanced pages bind local accessory sockets. R-R3-47: the Power
        // Genius tab is a view of the Core's amp plus the Core's commands;
        // the Tuner Genius tab stays visibly unavailable.
        m_tabs->addTab(buildRemotePgxlTab(), tr("PowerGenius XL"));
        // R-R3-47: the Core's Tuner Genius records (names, tune memory,
        // counters, faults); the tuner's connection is on the General tab.
        m_tgxlAdvancedPage = new TgxlAdvancedPage(m_model);
        m_tabs->addTab(m_tgxlAdvancedPage, tr("Tuner Genius XL"));
    } else {
        m_pgxlAdvancedPage = new PgxlAdvancedPage(m_model);
        m_tabs->addTab(m_pgxlAdvancedPage, tr("PowerGenius XL"));

        // Tab 3: Tuner Genius XL.  Same pattern as Tab 2.
        m_tgxlAdvancedPage = new TgxlAdvancedPage(m_model);
        m_tabs->addTab(m_tgxlAdvancedPage, tr("Tuner Genius XL"));
    }

    // 2026-05-22 menu cleanup: Diagnostics tab removed. Connection
    // State duplicated the General tab's FlexAPI status row and the
    // per-peer labels on PGXL/TGXL tabs; Disconnect/Reconnect Log
    // duplicated the rolling NereusSDR log file. Both removed for
    // bench-driven simplification (operator can read the log file or
    // PGXL/TGXL detail tabs for the same data).

    // Apply current master-gate state to the detail tabs at
    // construction time so a cold-open with FourO3A_Enabled=False
    // shows the tabs greyed out.
    const bool initialEnabled = m_model && m_model->fourO3AEnabled();
    applyMasterGateToTabs(initialEnabled);

    // Per-radio peripherals refactor (2026-05-26): repaint the banner
    // and refresh the master toggle each time the connection state
    // changes, so the page always reflects the connected radio's
    // per-MAC scope.
    if (m_model) {
        connect(m_model, &RadioModel::connectionStateChanged,
                this, &FourO3APage::refreshConnectionBanner);
        connect(m_model, &RadioModel::stationLinkStateChanged,
                this, &FourO3APage::refreshConnectionBanner);
        connect(m_model, &RadioModel::fourO3AStatusChanged,
                this, [this]() {
                    refreshConnectionBanner();
                    refreshFlexApiStatus();
                });
        connect(m_model, &RadioModel::stationFourO3ACommandFinished,
                this, &FourO3APage::onStationFourO3ACommandFinished);
        if (m_remotePgxlTab) {
            connect(m_model->amplifierModel(), &AmplifierModel::stationConnectionChanged,
                    this, &FourO3APage::refreshRemotePgxlTab);
            connect(m_model->amplifierModel(), &AmplifierModel::statusChanged,
                    this, &FourO3APage::refreshRemotePgxlTab);
            // R-R3-49 (parity Task 9): Operate waits while the radio is on
            // the air.
            connect(m_model, &RadioModel::coreOnAirChanged,
                    this, &FourO3APage::refreshRemotePgxlTab);
            // Task 77 fix round 3: and while the Core's tuner tunes.
            connect(m_model, &RadioModel::pgxlSwitchWaitChanged,
                    this, &FourO3APage::refreshRemotePgxlTab);
            connect(m_model, &RadioModel::stationLinkStateChanged, this, [this] {
                loadRemotePgxlSettings();
                refreshRemotePgxlTab();
                applyMasterGateToTabs(m_model->fourO3AEnabled());
            });
            loadRemotePgxlSettings();
            refreshRemotePgxlTab();
        }
    }
    refreshConnectionBanner();  // initial paint

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

    // Per-radio peripherals refactor (2026-05-26): banner row at the top
    // announces whose peripheral scope is being edited.  Filled by
    // refreshConnectionBanner().
    m_connectionBanner = new QLabel(tab);
    m_connectionBanner->setWordWrap(true);
    m_connectionBanner->setStyleSheet(
        QStringLiteral("color:#ffcc66; font-size:11px; font-weight:bold;"));
    layout->addWidget(m_connectionBanner);

    // ── Master Switch ─────────────────────────────────────────────
    auto* masterBox = new QGroupBox(tr("Master Switch"), tab);
    auto* masterLayout = new QVBoxLayout(masterBox);
    m_masterToggle = new QCheckBox(tr("Enable 4O3A integration"), masterBox);
    m_masterToggle->setObjectName(QStringLiteral("fourO3AMasterToggle"));
    m_masterToggle->setProperty("nereusSetupId", "catNetwork.fourO3A.enabled");
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
           "below are grayed out."),
        masterBox);
    masterHelp->setWordWrap(true);
    masterHelp->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    masterLayout->addWidget(masterHelp);
    layout->addWidget(masterBox);

    // ── FlexAPI Listener Status ───────────────────────────────────
    auto* statusBox = new QGroupBox(tr("FlexAPI Listener"), tab);
    auto* statusLayout = new QHBoxLayout(statusBox);
    m_flexApiStatusLabel = new QLabel(tr("Status: \xE2\x97\x8B Idle"), statusBox);
    m_flexApiStatusLabel->setObjectName(QStringLiteral("fourO3AListenerStatus"));
    m_flexApiStatusLabel->setProperty("nereusSetupId", "catNetwork.fourO3A.listener");
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

    // R-R3-48: whether the Power Genius follows the radio's band (the
    // Core's `amplifier` object in a remote window).
    m_pgxlBandFollow = new QLabel(tab);
    m_pgxlBandFollow->setObjectName(QStringLiteral("pgxlBandFollowLabel"));
    m_pgxlBandFollow->setProperty("nereusSetupId", "catNetwork.fourO3A.pgxlBandFollow");
    m_pgxlBandFollow->setTextFormat(Qt::PlainText);
    m_pgxlBandFollow->setWordWrap(true);
    m_pgxlBandFollow->setStyleSheet(QStringLiteral("color: #9aa5b1; font-size: 11px;"));
    layout->addWidget(m_pgxlBandFollow);
    if (AmplifierModel* amp = m_model ? m_model->amplifierModel() : nullptr) {
        const auto refresh = [this, amp] { m_pgxlBandFollow->setText(amp->bandFollowText()); };
        connect(amp, &AmplifierModel::bandFollowChanged, m_pgxlBandFollow, refresh);
        refresh();
    }

    // ── PGXL Interlock ────────────────────────────────────────────
    // R-R3-47: in a remote window too; there it shows the Core's policy and
    // changes it through the Core.
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
    if (m_model->role() == RadioModel::Role::Remote) {
        auto* const link = m_model->stationLink();
        const bool available = link && link->remoteFourO3AControlAvailable()
            && !m_model->currentRadioMac().isEmpty();
        if (!available) {
            m_remoteMasterResultIsError = true;
            m_remoteMasterResult = tr("This Core does not offer 4O3A control to this app right now.");
            refreshConnectionBanner();
            refreshFlexApiStatus();
            return;
        }
        const IStationLink::CommandOutcome outcome = link->requestFourO3AEnabled(checked);
        if (outcome.sent) {
            m_remoteMasterPending = true;
            m_remoteMasterResultIsError = false;
            m_remoteMasterResult.clear();
        } else {
            m_remoteMasterPending = false;
            m_remoteMasterResultIsError = true;
            // Shown in user words; the raw reason is logged.
            m_remoteMasterResult = OperatorReasonText::forDisplay(outcome.reason);
        }
        // The QCheckBox has already followed the click. Restore the Core
        // snapshot immediately: no local listener or preference is changed
        // while a request is in flight.
        refreshConnectionBanner();
        refreshFlexApiStatus();
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

void FourO3APage::onStationFourO3ACommandFinished(bool accepted, const QString& reason)
{
    m_remoteMasterPending = false;
    m_remoteMasterResultIsError = !accepted;
    m_remoteMasterResult = accepted
        ? tr("Core accepted the request; waiting for its listener status.")
        : (reason.isEmpty()
               ? tr("Core refused the 4O3A request.")
               : OperatorReasonText::forDisplay(reason));
    refreshConnectionBanner();
    refreshFlexApiStatus();
}

void FourO3APage::applyMasterGateToTabs(bool enabled)
{
    if (!m_tabs) { return; }
    if (m_model && m_model->role() == RadioModel::Role::Remote) {
        for (int i = 1; i < m_tabs->count(); ++i) { m_tabs->setTabEnabled(i, false); }
        // R-R3-47: the Power Genius tab reads and asks the Core; it is
        // open whenever this Core offers it.
        if (m_remotePgxlTab) {
            const auto* const link = m_model->stationLink();
            m_tabs->setTabEnabled(1, link && link->remotePgxlControlAvailable());
            // R-R3-47: the Tuner Genius tab reads the Core's records.
            const bool tgxlData = link && link->accessoryDataAvailable();
            m_tabs->setTabEnabled(2, tgxlData);
            m_tabs->setTabToolTip(2, tgxlData ? QString()
                : tr("This Core does not share its Tuner Genius records with this app."));
        }
        // Core refuses configure when its master is off. The row still needs
        // to show that reason and let an operator cancel existing work.
        if (m_peripheralsPage) { m_peripheralsPage->setEnabled(true); }
        return;
    }
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

void FourO3APage::refreshConnectionBanner()
{
    if (!m_connectionBanner) {
        return;
    }
    if (m_model && m_model->role() == RadioModel::Role::Remote) {
        const auto* const link = m_model->stationLink();
        const bool ready = link && link->remoteFourO3AControlAvailable();
        const bool haveMac = !m_model->currentRadioMac().isEmpty();
        if (!ready || !haveMac) {
            // A result from the retired session will never arrive. Do not
            // carry its pending latch into the next authenticated session.
            m_remoteMasterPending = false;
            m_remoteMasterResult.clear();
            m_remoteMasterResultIsError = false;
        }
        m_connectionBanner->setText(ready && haveMac
            ? tr("4O3A integration is managed by Core for %1.").arg(m_model->currentRadioMac())
            : tr("4O3A integration is managed by the Core; wait until this app is connected to it."));
        if (m_masterToggle) {
            const QSignalBlocker blocker(m_masterToggle);
            m_masterToggle->setChecked(m_model->fourO3AEnabled());
            m_masterToggle->setEnabled(ready && haveMac && !m_remoteMasterPending);
            if (m_remoteMasterPending) {
                m_masterToggle->setText(tr("Enable 4O3A integration (request pending)"));
                m_masterToggle->setToolTip(tr("Waiting for the Core to confirm the change."));
            } else {
                m_masterToggle->setText(tr("Enable 4O3A integration"));
                m_masterToggle->setToolTip(ready && haveMac
                    ? tr("Ask the Core to turn its 4O3A integration on or off. The box shows the Core's own setting.")
                    : tr("Needs a connection to the Core and a radio connected at the Core."));
            }
        }
        applyMasterGateToTabs(false);
        return;
    }
    // Use the per-MAC scope as the "connected" gate so unit tests that
    // pin a MAC via setLastRadioInfoForTest + setConnectionStateForTest
    // (without a live RadioConnection object) still drive the right
    // banner state.
    const QString mac     = m_model ? m_model->currentRadioMac() : QString{};
    const bool    haveMac = !mac.isEmpty();
    if (haveMac) {
        const QString name = m_model->name();
        m_connectionBanner->setText(
            tr("Editing peripherals for %1 (%2)").arg(name, mac));
        m_connectionBanner->setStyleSheet(
            QStringLiteral("color:#7ec850; font-size:11px; font-weight:bold;"));
    } else {
        m_connectionBanner->setText(
            tr("Connect to a radio to edit its peripherals."));
        m_connectionBanner->setStyleSheet(
            QStringLiteral("color:#ffcc66; font-size:11px; font-weight:bold;"));
    }
    // Master toggle reflects the per-MAC FourO3A_Enabled flag.  Block
    // signals during the refresh so the in-progress assignment doesn't
    // re-enter setFourO3AEnabled and inadvertently flip the listener
    // state.  Also gate enabled-state on the per-MAC scope availability.
    if (m_masterToggle) {
        m_masterToggle->setEnabled(haveMac);
        m_masterToggle->blockSignals(true);
        m_masterToggle->setChecked(m_model && m_model->fourO3AEnabled());
        m_masterToggle->blockSignals(false);
    }
    // Detail tabs follow the (now possibly-changed) master flag, but
    // also grey out when no radio is connected so the operator can't
    // edit a per-MAC scope that doesn't exist yet.
    applyMasterGateToTabs(m_model && m_model->fourO3AEnabled());
    if (m_peripheralsPage) {
        m_peripheralsPage->setEnabled(haveMac
                                      && m_model->fourO3AEnabled());
    }
    if (m_pgxlInterlockPage) {
        m_pgxlInterlockPage->setEnabled(haveMac
                                        && m_model->fourO3AEnabled());
    }
}

QWidget* FourO3APage::buildRemotePgxlTab()
{
    auto* tab = new QWidget(this);
    tab->setObjectName(QStringLiteral("remotePgxlTab"));
    m_remotePgxlTab = tab;
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    auto* note = new QLabel(tr("The Core connects to this Power Genius XL, checks that it is "
                               "one, and pairs it. This window shows the Core's view."), tab);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* statusBox = new QGroupBox(tr("Power Genius XL at the Core"), tab);
    auto* statusForm = new QFormLayout(statusBox);
    m_remotePgxlStatus = new QLabel(statusBox);
    m_remotePgxlStatus->setObjectName(QStringLiteral("remotePgxlStatus"));
    m_remotePgxlStatus->setWordWrap(true);
    statusForm->addRow(tr("Status:"), m_remotePgxlStatus);
    m_remotePgxlIdentity = new QLabel(statusBox);
    m_remotePgxlIdentity->setObjectName(QStringLiteral("remotePgxlIdentity"));
    m_remotePgxlIdentity->setWordWrap(true);
    statusForm->addRow(tr("Device:"), m_remotePgxlIdentity);
    m_remotePgxlReadings = new QLabel(statusBox);
    m_remotePgxlReadings->setObjectName(QStringLiteral("remotePgxlReadings"));
    statusForm->addRow(tr("Readings:"), m_remotePgxlReadings);

    auto* buttons = new QHBoxLayout;
    m_remotePgxlConnect = new QPushButton(tr("Connect"), statusBox);
    m_remotePgxlConnect->setObjectName(QStringLiteral("remotePgxlConnectButton"));
    connect(m_remotePgxlConnect, &QPushButton::clicked,
            this, &FourO3APage::onRemotePgxlConnectClicked);
    buttons->addWidget(m_remotePgxlConnect);
    // R-R3-49 (parity Task 9): the Core puts its amp in operate or standby
    // (setPgxlOperate, remotePgxlControlVersion 4); refreshRemotePgxlTab
    // sets the words and whether it is offered. An older Core keeps it
    // disabled with the receive-only reason (R-R3-25).
    m_remotePgxlOperate = new QPushButton(tr("Operate"), statusBox);
    m_remotePgxlOperate->setObjectName(QStringLiteral("remotePgxlOperateButton"));
    m_remotePgxlOperate->setEnabled(false);
    m_remotePgxlOperate->setToolTip(
        OperatorReasonText::forDisplay(AmplifierModel::receiveOnlyOperateReason()));
    connect(m_remotePgxlOperate, &QPushButton::clicked,
            this, &FourO3APage::onRemotePgxlOperateClicked);
    buttons->addWidget(m_remotePgxlOperate);
    buttons->addStretch();
    statusForm->addRow(buttons);
    layout->addWidget(statusBox);

    auto* settingsBox = new QGroupBox(tr("Connection settings at the Core"), tab);
    auto* settingsForm = new QFormLayout(settingsBox);
    m_remotePgxlAutoReconnect = new QCheckBox(tr("Reconnect automatically after a drop"),
                                              settingsBox);
    m_remotePgxlAutoReconnect->setObjectName(QStringLiteral("remotePgxlAutoReconnect"));
    settingsForm->addRow(m_remotePgxlAutoReconnect);
    m_remotePgxlKeepalive = new QSpinBox(settingsBox);
    m_remotePgxlKeepalive->setObjectName(QStringLiteral("remotePgxlKeepalive"));
    m_remotePgxlKeepalive->setRange(RadioModel::kPgxlKeepaliveMinSec,
                                    RadioModel::kPgxlKeepaliveMaxSec);
    m_remotePgxlKeepalive->setSuffix(tr(" s"));
    m_remotePgxlKeepalive->setToolTip(tr("How often the Core checks in with the amplifier."));
    settingsForm->addRow(tr("Keepalive every:"), m_remotePgxlKeepalive);
    m_remotePgxlPing = new QSpinBox(settingsBox);
    m_remotePgxlPing->setObjectName(QStringLiteral("remotePgxlPing"));
    m_remotePgxlPing->setRange(0, RadioModel::kPgxlPingMaxSec);
    m_remotePgxlPing->setSuffix(tr(" s"));
    m_remotePgxlPing->setSpecialValueText(tr("Off"));
    m_remotePgxlPing->setToolTip(tr("How often the Core measures the amplifier's response "
                                    "time. Off sends none."));
    settingsForm->addRow(tr("Response check every:"), m_remotePgxlPing);
    m_remotePgxlApply = new QPushButton(tr("Apply"), settingsBox);
    m_remotePgxlApply->setObjectName(QStringLiteral("remotePgxlApplySettings"));
    connect(m_remotePgxlApply, &QPushButton::clicked,
            this, &FourO3APage::onRemotePgxlApplySettingsClicked);
    settingsForm->addRow(m_remotePgxlApply);
    m_remotePgxlResult = new QLabel(settingsBox);
    m_remotePgxlResult->setObjectName(QStringLiteral("remotePgxlResult"));
    m_remotePgxlResult->setWordWrap(true);
    settingsForm->addRow(m_remotePgxlResult);
    layout->addWidget(settingsBox);
    // R-R3-47 / R-R3-22: the Core's output limit, counters and fault history.
    m_pgxlAdvancedPage = new PgxlAdvancedPage(m_model, tab);
    layout->addWidget(m_pgxlAdvancedPage, 1);
    return tab;
}

void FourO3APage::loadRemotePgxlSettings()
{
    if (!m_remotePgxlTab) { return; }
    // Station-wide keys: in a remote window AppSettings reads the Core's
    // copy (SettingsProxy). Unset ping is off on the Core.
    auto& s = AppSettings::instance();
    m_remotePgxlAutoReconnect->setChecked(
        s.value(QStringLiteral("PGXL_AutoReconnect"), QStringLiteral("True")).toString()
        == QStringLiteral("True"));
    m_remotePgxlKeepalive->setValue(
        s.value(QStringLiteral("PGXL_KeepaliveSec"), QStringLiteral("30")).toInt());
    m_remotePgxlPing->setValue(
        s.value(QStringLiteral("PGXL_PingSec"), QStringLiteral("0")).toInt());
}

void FourO3APage::refreshRemotePgxlTab()
{
    if (!m_remotePgxlTab || !m_model) { return; }
    const AmplifierModel* amp = m_model->amplifierModel();
    const auto* const link = m_model->stationLink();
    const bool available = link && link->remotePgxlControlAvailable();
    if (m_tabs) {
        m_tabs->setTabEnabled(1, available);
        m_tabs->setTabToolTip(1, available ? QString()
            : tr("This Core does not offer Power Genius XL control to this app."));
    }
    using Phase = AmplifierModel::ConnectionPhase;
    const Phase phase = amp->connectionPhase();
    const bool active = phase == Phase::Discovering || phase == Phase::Connecting
        || phase == Phase::Identifying || phase == Phase::Retrying;
    const bool connected = phase == Phase::Connected;
    const QString error = amp->connectionError().isEmpty()
        ? QString() : OperatorReasonText::forDisplay(amp->connectionError());
    QString text;
    switch (phase) {
    case Phase::Disabled: text = tr("Disabled at the Core"); break;
    case Phase::Disconnected: text = tr("Disconnected"); break;
    case Phase::Discovering: text = tr("Discovering at the Core"); break;
    case Phase::Connecting: text = tr("Connecting at the Core"); break;
    case Phase::Identifying: text = tr("Identifying device"); break;
    case Phase::Retrying:
        text = error.isEmpty() ? tr("Retrying at the Core")
                               : tr("Retrying at the Core: %1").arg(error);
        break;
    case Phase::Connected: text = tr("Connected"); break;
    case Phase::Error:
        text = tr("Error: %1").arg(OperatorReasonText::forDisplay(amp->connectionError()));
        break;
    }
    if (!amp->configuredHost().isEmpty()) {
        text += tr(" (%1, port %2)").arg(amp->configuredHost()).arg(amp->configuredPort());
    }
    m_remotePgxlStatus->setText(available ? text
        : tr("This Core does not offer Power Genius XL control to this app."));

    QStringList identity;
    if (!amp->deviceModel().isEmpty()) { identity << amp->deviceModel(); }
    if (!amp->deviceSerial().isEmpty()) { identity << tr("serial %1").arg(amp->deviceSerial()); }
    if (!amp->deviceVersion().isEmpty()) { identity << tr("version %1").arg(amp->deviceVersion()); }
    if (!amp->deviceNickname().isEmpty()) { identity << tr("named %1").arg(amp->deviceNickname()); }
    m_remotePgxlIdentity->setText(identity.isEmpty() ? tr("Not identified")
                                                     : identity.join(QStringLiteral(", ")));
    m_remotePgxlReadings->setText(amp->present()
        ? tr("%1, %2 C, %3 V").arg(amp->deviceState())
              .arg(amp->temperatureC(), 0, 'f', 1).arg(amp->mainsVoltageV(), 0, 'f', 0)
        : tr("No live readings"));

    m_remotePgxlConnect->setText(connected ? tr("Disconnect")
                                 : active ? tr("Cancel") : tr("Connect"));
    const bool haveAddress = !amp->configuredHost().isEmpty() && amp->configuredPort() > 0;
    m_remotePgxlConnect->setEnabled(available && (connected || active || haveAddress));
    m_remotePgxlConnect->setToolTip(available && !haveAddress && !connected && !active
        ? tr("Enter the Power Genius address on the General tab first.") : QString());
    m_remotePgxlApply->setEnabled(available);

    // R-R3-49 (parity Task 9): Operate reads the action the amp's reported
    // state allows (Standby while it operates, Operate otherwise), and is
    // offered by a Core at version 4 connected to the amp, off the air.
    // Task 77 fix round 3: it also waits while the Core's tuner tunes (the
    // Core refuses it during its own cycle's standby wait too), and a
    // faulted amp is offered Standby.
    const bool operateOffered = link && link->pgxlFullControlAvailable();
    const bool onAir = m_model->isCoreOnAir();
    const bool tuning = m_model->pgxlSwitchWaitsForTuner();
    const bool faulted = amp->state() == AmplifierModel::State::Fault;
    m_remotePgxlOperate->setText(amp->operate() || faulted ? tr("Standby") : tr("Operate"));
    m_remotePgxlOperate->setEnabled(operateOffered && connected && !onAir && !tuning);
    m_remotePgxlOperate->setToolTip(!operateOffered
        ? OperatorReasonText::forDisplay(AmplifierModel::receiveOnlyOperateReason())
        : onAir ? RadioModel::onAirReason()
        : tuning ? RadioModel::tunerTuningReason()
        : !connected ? tr("The Core is not connected to the Power Genius.")
        : faulted ? tr("The amplifier reports a fault. Put it in standby.")
        : amp->operate() ? tr("Put the Power Genius in standby.")
                         : tr("Put the Power Genius in operate."));
}

void FourO3APage::onRemotePgxlOperateClicked()
{
    if (!m_model || !m_remotePgxlTab) { return; }
    auto* link = m_model->stationLink();
    if (!link || !link->pgxlFullControlAvailable() || m_model->isCoreOnAir()
        || m_model->pgxlSwitchWaitsForTuner()) {   // Task 77 fix round 3
        refreshRemotePgxlTab();
        return;
    }
    // The button follows the amp's report, not the click. Task 77 fix round
    // 3: a faulted amp is sent standby.
    const AmplifierModel* amp = m_model->amplifierModel();
    const bool faulted = amp->state() == AmplifierModel::State::Fault;
    const auto outcome = link->requestPgxlOperate(!amp->operate() && !faulted);
    m_remotePgxlResult->setText(outcome.sent ? QString()
                                             : OperatorReasonText::forDisplay(outcome.reason));
}

void FourO3APage::onRemotePgxlConnectClicked()
{
    if (!m_model || !m_remotePgxlTab) { return; }
    auto* link = m_model->stationLink();
    if (!link || !link->remotePgxlControlAvailable()) {
        refreshRemotePgxlTab();
        return;
    }
    const AmplifierModel* amp = m_model->amplifierModel();
    using Phase = AmplifierModel::ConnectionPhase;
    const Phase phase = amp->connectionPhase();
    const bool activeOrConnected = phase == Phase::Connected || phase == Phase::Discovering
        || phase == Phase::Connecting || phase == Phase::Identifying || phase == Phase::Retrying;
    const auto outcome = activeOrConnected
        ? link->requestDisconnectPgxl()
        : link->requestConfigurePgxl(amp->configuredHost(),
                                     static_cast<quint16>(amp->configuredPort()));
    m_remotePgxlResult->setText(outcome.sent ? QString()
                                             : OperatorReasonText::forDisplay(outcome.reason));
}

void FourO3APage::onRemotePgxlApplySettingsClicked()
{
    if (!m_model || !m_remotePgxlTab) { return; }
    auto* link = m_model->stationLink();
    if (!link || !link->remotePgxlControlAvailable()) {
        refreshRemotePgxlTab();
        return;
    }
    const auto outcome = link->requestPgxlConnectionSettings(
        m_remotePgxlAutoReconnect->isChecked(), m_remotePgxlKeepalive->value(),
        m_remotePgxlPing->value());
    m_remotePgxlResult->setText(outcome.sent ? tr("Sent to the Core.")
                                             : OperatorReasonText::forDisplay(outcome.reason));
}

void FourO3APage::refreshFlexApiStatus()
{
    if (!m_flexApiStatusLabel) { return; }
    if (m_model && m_model->role() == RadioModel::Role::Remote) {
        const auto* const link = m_model->stationLink();
        const bool ready = link && link->remoteFourO3AControlAvailable()
            && !m_model->currentRadioMac().isEmpty();
        if (!ready) {
            m_flexApiStatusLabel->setText(tr("Status: \xE2\x97\x8B Not connected to the Core"));
            m_flexApiStatusLabel->setStyleSheet(QStringLiteral("color: #888;"));
        } else if (m_model->fourO3AListening()) {
            m_flexApiStatusLabel->setText(tr("Status: \xE2\x97\x8F Core listening on TCP 4992"));
            m_flexApiStatusLabel->setStyleSheet(QStringLiteral("color: #4CAF50;"));
        } else if (!m_model->fourO3AListenerError().isEmpty()) {
            m_flexApiStatusLabel->setText(
                tr("Status: \xE2\x97\x8F Core listener error: %1")
                    .arg(OperatorReasonText::forDisplay(m_model->fourO3AListenerError())));
            m_flexApiStatusLabel->setStyleSheet(QStringLiteral("color: #E53935;"));
        } else if (m_remoteMasterResultIsError && !m_remoteMasterResult.isEmpty()) {
            m_flexApiStatusLabel->setText(tr("Status: %1").arg(m_remoteMasterResult));
            m_flexApiStatusLabel->setStyleSheet(QStringLiteral("color: #E53935;"));
        } else if (m_model->fourO3AEnabled()) {
            m_flexApiStatusLabel->setText(tr("Status: \xE2\x97\x8B Core listener starting"));
            m_flexApiStatusLabel->setStyleSheet(QStringLiteral("color: #ffcc66;"));
        } else {
            m_flexApiStatusLabel->setText(tr("Status: \xE2\x97\x8B Disabled at Core"));
            m_flexApiStatusLabel->setStyleSheet(QStringLiteral("color: #888;"));
        }
        m_flexApiStatusLabel->setToolTip(
            tr("The Core listens on TCP 4992. This window only shows the Core's status."));
        return;
    }
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
