// =================================================================
// src/gui/setup/RfKitPage.cpp  (NereusSDR-native)
// =================================================================
//
// See RfKitPage.h for the design overview.  Implementation notes:
//
//   - The General tab is hand-built (master toggle + helper text +
//     live status row).  The RF2K-S tab is a placeholder; its full
//     content lands in Task 11.
//
//   - Live-status refresh runs on a 1 Hz timer so the connection
//     state reflects real-time changes.
//
//   - Pattern mirrors FourO3APage.{h,cpp}.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-24 -- Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-24 -- R-R3-47 / R-R3-48: remote window through the Core
//                 (switch, connect, disconnect), band-follow line. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 -- R-R3-47: every control works from a remote window: the
//                 connection settings and antenna names as station
//                 settings, Reset amp error through the Core
//                 (remoteRfKitControlVersion 3); the page follows another
//                 window's changes to them, except fields the operator
//                 changed and the Core has not yet taken. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 -- R-R3-49 (parity Task 10): a remote window's "Set amp to
//                 TCI mode" asks the Core (setRfKitTciMode); Save keeps a
//                 changed Host and Port on the Core without dialling
//                 (setRfKitAddress); both wait while the radio is on the
//                 air. Live diagnostics shows the Core's connection counts
//                 (accessoryData's rfkit*), and a local window's gains the
//                 connected-since and last-poll readings. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 -- R-R3-49 (parity mini-round, the operator's rulings a to
//                 c): Host, Port and Save no longer wait on the air in a
//                 remote window; a local window's "Set amp to TCI mode"
//                 waits on the air, and a click refused there shows the
//                 remote window's reason. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-29 -- R-R3-49 / R-IOS-18: Setup description version 15 ids.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "RfKitPage.h"

#include "models/AccessoryDataModel.h"
#include "models/RadioModel.h"
#include "models/RfKitModel.h"
#include "core/AppSettings.h"
#include "core/Rf2ksConnection.h"
#include "core/session/IStationLink.h"
#include "gui/OperatorReasonText.h"

#include <QCheckBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {
// R-R3-49 (parity Task 10): a count's time as Live diagnostics shows it.
QString clockTime(qint64 msSinceEpoch)
{
    return msSinceEpoch > 0
        ? QDateTime::fromMSecsSinceEpoch(msSinceEpoch).toString(QStringLiteral("HH:mm:ss"))
        : QStringLiteral("--");
}
} // namespace

RfKitPage::RfKitPage(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    m_tabs = new QTabWidget(this);
    root->addWidget(m_tabs);

    m_tabs->addTab(buildGeneralTab(), tr("General"));

    m_rf2ksTab = buildRf2ksTab();
    m_tabs->addTab(m_rf2ksTab, tr("RF2K-S"));

    // Apply current master-gate state so a cold-open with RfKit_Enabled=False
    // shows the RF2K-S tab greyed out.
    applyMasterGate(m_model && m_model->rfKitEnabled());

    // Per-radio peripherals refactor (2026-05-26): repaint the banner and
    // refresh the input fields each time the connection state changes,
    // so the page always reflects the connected radio's per-MAC scope.
    if (m_model) {
        connect(m_model, &RadioModel::connectionStateChanged,
                this, &RfKitPage::refreshConnectionBanner);
        // R-R3-47: the Core's switch and link, in a remote window.
        connect(m_model, &RadioModel::stationLinkStateChanged,
                this, &RfKitPage::refreshConnectionBanner);
        connect(m_model, &RadioModel::rfKitEnabledChanged, this, [this](bool enabled) {
            if (m_master) {
                const QSignalBlocker block(m_master);
                m_master->setChecked(enabled);
            }
            applyMasterGate(enabled);
            refreshLiveStatus();
        });
        if (RfKitModel* rfKit = m_model->rfKitModel()) {
            connect(rfKit, &RfKitModel::bandFollowChanged, this, &RfKitPage::refreshBandFollow);
            connect(rfKit, &RfKitModel::stationConnectionChanged,
                    this, &RfKitPage::refreshLiveStatus);
        }
    }
    refreshConnectionBanner();  // initial paint
    refreshBandFollow();

    // Periodic live-status refresh (1 Hz).
    auto* timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &RfKitPage::refreshLiveStatus);
    timer->start();
    refreshLiveStatus();  // initial paint

    // Setup description version 15: this page's ids.
    const std::pair<QWidget*, const char*> setupIds[] = {
        {m_master, "enabled"},
        {m_liveStatusLabel, "status"},
        {m_bandFollowLabel, "bandFollow"},
        {m_hostEdit, "host"},
        {m_portSpin, "port"},
        {m_autoReconnect, "autoReconnect"},
        {m_pollIntervalSpin, "pollInterval"},
        {m_testConnBtn, "connect"},
        {m_disconnectBtn, "disconnect"},
        {m_setTciBtn, "tciMode"},
        {m_resetErrBtn, "resetError"},
        {m_antLabelEdits[0], "ant1Label"},
        {m_antLabelEdits[1], "ant2Label"},
        {m_antLabelEdits[2], "ant3Label"},
        {m_antLabelEdits[3], "ant4Label"}};
    for (const auto& [widget, id] : setupIds) {
        if (widget) {
            widget->setProperty("nereusSetupId", QStringLiteral("catNetwork.rfKit.") + QLatin1String(id));
        }
    }
    if (m_diagnosticsLabel) {
        m_diagnosticsLabel->setProperty("nereusSetupIds", QStringList{
            QStringLiteral("catNetwork.rfKit.pollsOk"), QStringLiteral("catNetwork.rfKit.pollsFailed"),
            QStringLiteral("catNetwork.rfKit.rtt"), QStringLiteral("catNetwork.rfKit.reconnects"),
            QStringLiteral("catNetwork.rfKit.connectedSince"), QStringLiteral("catNetwork.rfKit.lastPoll")});
    }
}

QWidget* RfKitPage::buildGeneralTab()
{
    auto* tab = new QWidget(this);
    auto* lay = new QVBoxLayout(tab);
    lay->setContentsMargins(0, 8, 0, 0);
    lay->setSpacing(12);

    // Per-radio peripherals refactor (2026-05-26): banner row at the top
    // of the General tab announces whose peripheral scope is being edited.
    // refreshConnectionBanner() fills the text and toggles enabled-state
    // for the controls below.
    m_connectionBanner = new QLabel(tab);
    m_connectionBanner->setWordWrap(true);
    m_connectionBanner->setStyleSheet(
        QStringLiteral("color:#ffcc66; font-size:11px; font-weight:bold;"));
    lay->addWidget(m_connectionBanner);

    m_master = new QCheckBox(tr("Enable RF-Kit Amplifier integration"), tab);
    m_master->setToolTip(
        tr("Gates the RF-Kit applet in the right-column panel and the RF2K-S "
           "configuration tab below.  Off by default; turn on only when an "
           "RF-Kit RF2K-S amplifier is connected.  Setting is scoped to the "
           "currently connected radio."));
    m_master->setChecked(m_model && m_model->rfKitEnabled());
    connect(m_master, &QCheckBox::toggled, this, &RfKitPage::onMasterToggled);
    lay->addWidget(m_master);

    auto* helper = new QLabel(tr(
        "When enabled, the RF-Kit applet appears in the right-column panel, "
        "the analog S-meter switches to 2 kW scale when the amp is in OPERATE, "
        "and TCI band tracking flows to the amp automatically. When disabled, "
        "the applet hides and the RF2K-S tab below grays out."), tab);
    helper->setWordWrap(true);
    helper->setStyleSheet(QStringLiteral("color: #9aa5b1; font-size: 11px;"));
    lay->addWidget(helper);

    m_liveStatusLabel = new QLabel(tab);
    m_liveStatusLabel->setTextFormat(Qt::RichText);
    lay->addWidget(m_liveStatusLabel);

    // R-R3-48: whether the amp follows the radio's band, and if not, the
    // TCI server address to enter on it.
    m_bandFollowLabel = new QLabel(tab);
    m_bandFollowLabel->setObjectName(QStringLiteral("rfKitPageBandFollow"));
    m_bandFollowLabel->setTextFormat(Qt::PlainText);
    m_bandFollowLabel->setWordWrap(true);
    lay->addWidget(m_bandFollowLabel);

    lay->addStretch();
    return tab;
}

QWidget* RfKitPage::buildRf2ksTab()
{
    auto* tab  = new QWidget(this);
    auto* root = new QVBoxLayout(tab);

    // --- Connection group ---
    auto* connBox = new QGroupBox(tr("Connection"), tab);
    auto* connFm  = new QFormLayout(connBox);

    // Per-radio peripherals refactor (2026-05-26): host/port live under
    // hardware/<mac>/peripherals/.  Empty when offline; reloaded from
    // the per-MAC scope on connectionStateChanged via reloadFromPeripherals.
    m_hostEdit = new QLineEdit(connBox);
    m_hostEdit->setText(m_model
        ? m_model->peripheralValue(QStringLiteral("RfKit_ManualIp"))
        : QString{});
    connFm->addRow(tr("Host:"), m_hostEdit);

    m_portSpin = new QSpinBox(connBox);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(m_model
        ? m_model->peripheralValue(QStringLiteral("RfKit_ManualPort"),
                                   QStringLiteral("8080")).toInt()
        : 8080);
    connFm->addRow(tr("Port:"), m_portSpin);

    m_autoReconnect = new QCheckBox(tr("Auto-reconnect on disconnect"), connBox);
    m_autoReconnect->setChecked(
        AppSettings::instance()
            .value(QStringLiteral("RfKit_AutoReconnect"), QStringLiteral("True"))
            .toString() == QStringLiteral("True"));
    connFm->addRow(QString(), m_autoReconnect);

    m_pollIntervalSpin = new QSpinBox(connBox);
    m_pollIntervalSpin->setRange(250, 5000);
    m_pollIntervalSpin->setSuffix(QStringLiteral(" ms"));
    m_pollIntervalSpin->setValue(AppSettings::instance()
        .value(QStringLiteral("RfKit_PollIntervalMs"), QStringLiteral("1000")).toInt());
    connFm->addRow(tr("Poll interval:"), m_pollIntervalSpin);

    m_testConnBtn = new QPushButton(tr("Test connection"), connBox);
    m_disconnectBtn = new QPushButton(tr("Disconnect"), connBox);
    m_disconnectBtn->setObjectName(QStringLiteral("rfKitDisconnectButton"));
    m_setTciBtn   = new QPushButton(tr("Set amp to TCI mode"), connBox);
    m_resetErrBtn = new QPushButton(tr("Reset amp error state"), connBox);
    auto* btnRow  = new QHBoxLayout();
    btnRow->addWidget(m_testConnBtn);
    btnRow->addWidget(m_disconnectBtn);
    btnRow->addWidget(m_setTciBtn);
    btnRow->addWidget(m_resetErrBtn);
    connFm->addRow(btnRow);

    root->addWidget(connBox);

    connect(m_testConnBtn, &QPushButton::clicked, this, &RfKitPage::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &RfKitPage::onDisconnectClicked);
    connect(m_setTciBtn, &QPushButton::clicked, this, &RfKitPage::onSetTciClicked);
    connect(m_resetErrBtn, &QPushButton::clicked, this, &RfKitPage::onResetErrorClicked);

    // --- Antenna labels group ---
    auto* labelsBox = new QGroupBox(tr("Antenna labels"), tab);
    auto* labelsFm  = new QFormLayout(labelsBox);
    auto* note      = new QLabel(tr(
        "RF2K-S firmware does not expose antenna names via REST. "
        "Labels are stored locally in NereusSDR."), labelsBox);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color:#9aa5b1; font-size:10px;"));
    labelsFm->addRow(note);
    for (int i = 0; i < 4; ++i) {
        m_antLabelEdits[i] = new QLineEdit(labelsBox);
        // Bench feedback 2026-05-25 KG4VCF: cap antenna label length so
        // the saved name fits in the applet's 4-button antenna row
        // without truncation or overrun. 12 chars covers typical names
        // ("80m dipole", "20m beam", "vertical", "Hexbeam", "MagLoop")
        // with headroom; longer labels are uncommon and would crowd the
        // button regardless.
        m_antLabelEdits[i]->setMaxLength(12);
        m_antLabelEdits[i]->setPlaceholderText(QStringLiteral("e.g. 80m dipole"));
        m_antLabelEdits[i]->setText(AppSettings::instance()
            .value(QStringLiteral("RfKit_Ant%1_Label").arg(i + 1)).toString());
        labelsFm->addRow(tr("ANT %1:").arg(i + 1), m_antLabelEdits[i]);
    }
    root->addWidget(labelsBox);

    // --- Save ---
    m_saveBtn = new QPushButton(tr("Save"), tab);
    connect(m_saveBtn, &QPushButton::clicked, this, &RfKitPage::saveRf2ksSettings);
    root->addWidget(m_saveBtn);

    // --- Live diagnostics group ---
    auto* diagBox = new QGroupBox(tr("Live diagnostics"), tab);
    auto* diagLay = new QVBoxLayout(diagBox);
    m_diagnosticsLabel = new QLabel(diagBox);
    m_diagnosticsLabel->setTextFormat(Qt::RichText);
    diagLay->addWidget(m_diagnosticsLabel);
    root->addWidget(diagBox);

    root->addStretch();

    // R-R3-47: in a remote window the amp is the Core's. Its address comes
    // from the Core's `rfkit` object and goes back with Connect; the rest
    // of this tab stays with the Core.
    if (isRemote()) {
        m_testConnBtn->setText(tr("Connect"));
        m_testConnBtn->setToolTip(tr("Ask the Core to connect to the amplifier at this address "
                                     "and save it."));
        if (RfKitModel* rfKit = m_model->rfKitModel()) {
            m_hostEdit->setText(rfKit->configuredHost());
            if (rfKit->configuredPort() > 0) {
                m_portSpin->setValue(rfKit->configuredPort());
            }
        }
        // R-R3-49 (parity Task 10): TCI mode and the address, through the
        // Core (refreshRemoteControls says when they wait and why).
        connect(m_model, &RadioModel::stationLinkStateChanged,
                this, &RfKitPage::refreshRemoteControls);
        connect(m_model, &RadioModel::coreOnAirChanged,
                this, &RfKitPage::refreshRemoteControls);
        if (RfKitModel* rfKit = m_model->rfKitModel()) {
            connect(rfKit, &RfKitModel::stationConnectionChanged,
                    this, &RfKitPage::refreshRemoteControls);
        }
        if (AccessoryDataModel* data = m_model->accessoryDataModel()) {
            connect(data, &AccessoryDataModel::rfkitDiagnosticsChanged,
                    this, &RfKitPage::refreshLiveStatus);
        }
        refreshRemoteControls();
        // I4: the Core's settings and names, and whether it takes them.
        connect(m_model, &RadioModel::stationLinkStateChanged,
                this, &RfKitPage::refreshRemoteSettings);
        // Follow-up 6: another window's change to automatic retry or the
        // poll interval on the Core shows here too.
        connect(m_model, &RadioModel::stationSettingChanged, this,
                [this](const QString& key) {
            // Rework follow-up 3: a saved value the Core now has ends its
            // field's mark (checked before the refresh reads the Core).
            settleSaved(key);
            if (key.isEmpty() || key == QLatin1String("RfKit_AutoReconnect")
                || key == QLatin1String("RfKit_PollIntervalMs")) {
                refreshRemoteSettings();
            }
        });
        if (AccessoryDataModel* data = m_model->accessoryDataModel()) {
            connect(data, &AccessoryDataModel::labelsChanged,
                    this, &RfKitPage::refreshRemoteSettings);
        }
        // A request the Core refused (connect, switch, Reset amp error):
        // its words show in the status line.
        connect(m_model, &RadioModel::accessoryRequestRefused, this,
                [this](const QString& device, const QString& reason) {
            if (device == QLatin1String("rfkit")) {
                m_remoteResult = OperatorReasonText::forDisplay(reason);
                refreshLiveStatus();
            }
        });
        // Rework part 6: what the operator changes stays until Save.
        connect(m_autoReconnect, &QCheckBox::toggled, this,
                [this] { m_touchedAutoReconnect = true; });
        connect(m_pollIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this] { m_touchedPoll = true; });
        for (int i = 0; i < 4; ++i) {
            connect(m_antLabelEdits[i], &QLineEdit::textEdited, this,
                    [this, i] { m_touchedLabel[i] = true; });
        }
        refreshRemoteSettings();
    } else if (m_model) {
        // Parity mini-round (ruling a): the local TCI mode button waits on
        // the air (refreshRemoteControls' local branch).
        connect(m_model, &RadioModel::coreOnAirChanged,
                this, &RfKitPage::refreshRemoteControls);
        refreshRemoteControls();
    }
    return tab;
}

bool RfKitPage::isRemote() const
{
    return m_model && m_model->role() == RadioModel::Role::Remote;
}

bool RfKitPage::remoteControlAvailable() const
{
    const IStationLink* link = m_model ? m_model->stationLink() : nullptr;
    return link && link->remoteRfKitControlAvailable();
}

bool RfKitPage::remoteSettingsAvailable() const
{
    const IStationLink* link = m_model ? m_model->stationLink() : nullptr;
    return link && link->rfKitSettingsAvailable();
}

void RfKitPage::refreshRemoteSettings()
{
    if (!isRemote()) {
        return;
    }
    // I4 (R-R3-47): the station's values (the Core's settings, read through
    // the station settings), unless the operator is changing one.
    const bool available = remoteSettingsAvailable();
    const QString unavailable = tr("This Core does not let this app change these settings. "
                                   "Updating the Core may help.");
    for (QWidget* w : std::initializer_list<QWidget*>{
             m_autoReconnect, m_pollIntervalSpin, m_saveBtn, m_resetErrBtn,
             m_antLabelEdits[0], m_antLabelEdits[1], m_antLabelEdits[2],
             m_antLabelEdits[3]}) {
        w->setEnabled(available);
        w->setToolTip(available ? QString() : unavailable);
    }
    if (available) {
        m_resetErrBtn->setToolTip(tr("Ask the Core to clear the amplifier's error."));
        auto& s = AppSettings::instance();
        if (!m_autoReconnect->hasFocus() && !m_touchedAutoReconnect) {
            const QSignalBlocker block(m_autoReconnect);
            m_autoReconnect->setChecked(
                s.value(QStringLiteral("RfKit_AutoReconnect"), QStringLiteral("True"))
                    .toString() == QStringLiteral("True"));
        }
        if (!m_pollIntervalSpin->hasFocus() && !m_touchedPoll) {
            const QSignalBlocker block(m_pollIntervalSpin);
            m_pollIntervalSpin->setValue(
                s.value(QStringLiteral("RfKit_PollIntervalMs"), QStringLiteral("1000")).toInt());
        }
        const QStringList labels = m_model->accessoryDataModel()
            ? m_model->accessoryDataModel()->rfkitAntennaLabels() : QStringList{};
        for (int i = 0; i < 4; ++i) {
            if (m_antLabelEdits[i] && !m_antLabelEdits[i]->hasFocus() && !m_touchedLabel[i]
                && i < labels.size()) {
                m_antLabelEdits[i]->setText(labels.at(i));
            }
        }
    }
}

void RfKitPage::refreshRemoteControls()
{
    if (!m_setTciBtn) {
        return;
    }
    if (!isRemote()) {
        // Parity mini-round (the operator's ruling a, 2026-09-25): TCI mode
        // switches the amp, so a local window's waits on the air too, with
        // the remote window's reason.
        const bool onAir = m_model && m_model->isCoreOnAir();
        m_setTciBtn->setEnabled(!onAir);
        m_setTciBtn->setToolTip(onAir ? RadioModel::onAirReason() : QString());
        return;
    }
    const IStationLink* link = m_model->stationLink();
    const bool full = link && link->rfKitFullControlAvailable();
    const bool onAir = m_model->isCoreOnAir();
    const RfKitModel* rfKit = m_model->rfKitModel();
    const bool connected = rfKit
        && rfKit->connectionPhase() == RfKitModel::ConnectionPhase::Connected;
    QString tciReason;
    if (!full) {
        // An older Core: it switches the amp to TCI mode only by itself.
        tciReason = tr("The Core puts the amplifier in TCI mode itself while the Core's TCI "
                       "server is on.");
    } else if (onAir) {
        tciReason = RadioModel::onAirReason();
    } else if (!connected) {
        tciReason = tr("The Core is not connected to the RF-Kit amplifier.");
    }
    m_setTciBtn->setEnabled(tciReason.isEmpty());
    m_setTciBtn->setToolTip(tciReason.isEmpty() ? tr("Ask the Core to put the amplifier in TCI "
                                                     "mode.")
                                                : tciReason);
    // Parity mini-round (rulings a and b): the address is only saved, so it
    // does not wait on the air (an older Core takes it only with Connect).
    for (QWidget* w : std::initializer_list<QWidget*>{m_hostEdit, m_portSpin}) {
        w->setEnabled(true);
        w->setToolTip(QString());
    }
}

void RfKitPage::onSetTciClicked()
{
    if (!m_model) { return; }
    if (isRemote()) {
        // R-R3-49 (parity Task 10): the Core sends its amp the request this
        // button sends locally.
        IStationLink* link = m_model->stationLink();
        const auto outcome = link ? link->requestRfKitTciMode()
            : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
        m_remoteResult = outcome.sent ? QString() : OperatorReasonText::forDisplay(outcome.reason);
        refreshLiveStatus();
        return;
    }
    // Parity mini-round (rulings a and c): refused on the air by the Core's
    // own rule, with the remote window's reason.
    if (m_model->refuseLocalAccessorySwitchOnAir(QStringLiteral("rfkit"))) {
        refreshRemoteControls();
        return;
    }
    if (m_model->rfKitConnection()) {
        m_model->rfKitConnection()->setOperationalInterface(QStringLiteral("TCI"));
    }
}

QString RfKitPage::diagnosticsTextForTesting() const
{
    return m_diagnosticsLabel ? m_diagnosticsLabel->text() : QString();
}

void RfKitPage::settleSaved(const QString& key)
{
    auto& s = AppSettings::instance();
    for (auto it = m_savedPending.begin(); it != m_savedPending.end();) {
        if ((key.isEmpty() || key == it.key()) && s.value(it.key()).toString() == it.value()) {
            const QString& k = it.key();
            if (k == QLatin1String("RfKit_AutoReconnect")) {
                m_touchedAutoReconnect = false;
            } else if (k == QLatin1String("RfKit_PollIntervalMs")) {
                m_touchedPoll = false;
            } else {
                const int n = k.mid(9, 1).toInt();   // RfKit_Ant<N>_Label
                if (n >= 1 && n <= 4) {
                    m_touchedLabel[n - 1] = false;
                }
            }
            it = m_savedPending.erase(it);
        } else {
            ++it;
        }
    }
}

void RfKitPage::onResetErrorClicked()
{
    if (!m_model) { return; }
    if (isRemote()) {
        // I4 (R-R3-47): the Core sends the amp the request this button sends
        // locally.
        IStationLink* link = m_model->stationLink();
        const auto outcome = link ? link->requestResetRfKitError()
            : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
        // Follow-up 3: this page shows the Core's refusal; no toast too.
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
        m_remoteResult = outcome.sent ? QString() : OperatorReasonText::forDisplay(outcome.reason);
        refreshLiveStatus();
        return;
    }
    if (m_model->rfKitConnection()) {
        m_model->rfKitConnection()->resetError();
    }
}

void RfKitPage::onConnectClicked()
{
    if (!m_model) { return; }
    if (isRemote()) {
        IStationLink* link = m_model->stationLink();
        if (!link) { return; }
        const auto outcome = link->requestConfigureRfKit(
            m_hostEdit->text().trimmed(), static_cast<quint16>(m_portSpin->value()));
        // Follow-up 3: this page shows the Core's refusal; no toast too.
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
        m_remoteResult = outcome.sent ? QString() : OperatorReasonText::forDisplay(outcome.reason);
        refreshLiveStatus();
        return;
    }
    if (m_model->rfKitConnection()) {
        m_model->rfKitConnection()->connectToAmp(
            m_hostEdit->text(),
            static_cast<quint16>(m_portSpin->value()));
    }
}

void RfKitPage::onDisconnectClicked()
{
    if (!m_model) { return; }
    if (isRemote()) {
        IStationLink* link = m_model->stationLink();
        if (!link) { return; }
        const auto outcome = link->requestDisconnectRfKit();
        // Follow-up 3: this page shows the Core's refusal; no toast too.
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
        m_remoteResult = outcome.sent ? QString() : OperatorReasonText::forDisplay(outcome.reason);
        refreshLiveStatus();
        return;
    }
    if (m_model->rfKitConnection()) {
        m_model->rfKitConnection()->disconnect();
    }
}

void RfKitPage::refreshBandFollow()
{
    if (!m_bandFollowLabel) { return; }
    RfKitModel* rfKit = m_model ? m_model->rfKitModel() : nullptr;
    m_bandFollowLabel->setText(rfKit ? rfKit->bandFollowText() : QString());
}

QString RfKitPage::bandFollowTextForTesting() const
{
    return m_bandFollowLabel ? m_bandFollowLabel->text() : QString();
}

QString RfKitPage::liveStatusTextForTesting() const
{
    return m_liveStatusLabel ? m_liveStatusLabel->text() : QString();
}

void RfKitPage::saveRf2ksSettings()
{
    if (isRemote()) {
        // I4 (R-R3-47): the connection settings and antenna names are the
        // station's; written as station settings, which the Core applies at
        // once (remoteRfKitControlVersion 3). The address goes with Connect.
        if (!remoteSettingsAvailable()) {
            return;
        }
        // R-R3-49 (parity Task 10): a Host or Port changed here is kept on
        // the Core for its radio, as a local Save keeps it, without
        // dialling (setRfKitAddress), on the air too (parity mini-round,
        // rulings a and b); an older Core takes the address only with
        // Connect.
        IStationLink* link = m_model->stationLink();
        const RfKitModel* rfKit = m_model->rfKitModel();
        const QString host = m_hostEdit->text().trimmed();
        const int port = m_portSpin->value();
        if (link && link->rfKitFullControlAvailable() && rfKit
            && (host != rfKit->configuredHost() || port != rfKit->configuredPort())) {
            const auto outcome = link->requestRfKitAddress(host, port);
            m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
            m_remoteResult = outcome.sent ? QString()
                                          : OperatorReasonText::forDisplay(outcome.reason);
            refreshLiveStatus();
        }
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("RfKit_AutoReconnect"),
                   m_autoReconnect->isChecked() ? QStringLiteral("True")
                                                : QStringLiteral("False"));
        s.setValue(QStringLiteral("RfKit_PollIntervalMs"),
                   QString::number(m_pollIntervalSpin->value()));
        for (int i = 0; i < 4; ++i) {
            s.setValue(QStringLiteral("RfKit_Ant%1_Label").arg(i + 1),
                       m_antLabelEdits[i]->text());
            m_savedPending.insert(QStringLiteral("RfKit_Ant%1_Label").arg(i + 1),
                                  m_antLabelEdits[i]->text());
        }
        // Rework follow-up 3: the marks stay until the Core has these values
        // (its settings echo them); writes that never reach it (the link
        // not ready) leave the operator's values in place.
        m_savedPending.insert(QStringLiteral("RfKit_AutoReconnect"),
                              m_autoReconnect->isChecked() ? QStringLiteral("True")
                                                           : QStringLiteral("False"));
        m_savedPending.insert(QStringLiteral("RfKit_PollIntervalMs"),
                              QString::number(m_pollIntervalSpin->value()));
        return;
    }
    // Per-radio peripherals refactor (2026-05-26): the three connection
    // keys are scoped under hardware/<mac>/peripherals/ via
    // RadioModel::setPeripheralValue.  Auto-reconnect / poll-interval
    // and antenna labels remain GLOBAL because they're operator
    // preferences that don't depend on which radio is on the air.
    if (m_model) {
        m_model->setPeripheralValue(QStringLiteral("RfKit_ManualIp"),
                                    m_hostEdit->text());
        m_model->setPeripheralValue(QStringLiteral("RfKit_ManualPort"),
                                    QString::number(m_portSpin->value()));
    }
    AppSettings::instance().setValue(
        QStringLiteral("RfKit_AutoReconnect"),
        m_autoReconnect->isChecked()
            ? QStringLiteral("True") : QStringLiteral("False"));
    AppSettings::instance().setValue(
        QStringLiteral("RfKit_PollIntervalMs"),
        QString::number(m_pollIntervalSpin->value()));
    for (int i = 0; i < 4; ++i) {
        AppSettings::instance().setValue(
            QStringLiteral("RfKit_Ant%1_Label").arg(i + 1),
            m_antLabelEdits[i]->text());
    }
    // 2026-05-25 KG4VCF bench fix: persist NOW.  AppSettings::setValue
    // only updates the in-memory dict; the disk flush waits for the
    // aboutToQuit handler.  A non-graceful exit (or a crash) between
    // Save click and quit silently loses the IP/port.  Mirror the
    // explicit flush pattern used by AudioVaxPage, DeviceCard, and
    // HardwarePage.
    AppSettings::instance().save();

    // Push the two operator preferences into the live connection.  Save
    // used to only persist them, so with RF-Kit already connected,
    // unticking Auto-reconnect still permitted the next retry and a new
    // poll interval did nothing until some later model-driven reapply
    // happened to run.  applyRfKitOperatorSettings() re-reads both keys
    // from AppSettings, so it must run after the writes above, and it
    // no-ops when there is no connection.  Codex review, PR #291.
    if (m_model) {
        m_model->applyRfKitOperatorSettings();
    }
}

void RfKitPage::reloadFromPeripherals()
{
    if (!m_model) {
        return;
    }
    if (m_hostEdit) {
        m_hostEdit->setText(m_model->peripheralValue(
            QStringLiteral("RfKit_ManualIp")));
    }
    if (m_portSpin) {
        m_portSpin->setValue(m_model->peripheralValue(
            QStringLiteral("RfKit_ManualPort"),
            QStringLiteral("8080")).toInt());
    }
    if (m_master) {
        m_master->blockSignals(true);
        m_master->setChecked(m_model->rfKitEnabled());
        m_master->blockSignals(false);
    }
}

void RfKitPage::refreshConnectionBanner()
{
    if (!m_connectionBanner) {
        return;
    }
    // Use the per-MAC scope as the "connected" gate so unit tests that
    // inject a MAC via setLastRadioInfoForTest + setConnectionStateForTest
    // (without a live RadioConnection object) still drive the right
    // banner state.
    if (isRemote()) {
        // R-R3-47: the Core's RF-Kit, switched through the Core.
        const bool available = remoteControlAvailable();
        m_connectionBanner->setText(available
            ? tr("Changes here go to the RF-Kit amplifier at the Core's station.")
            : tr("This Core does not offer RF-Kit amplifier setup to this app."));
        m_connectionBanner->setStyleSheet(available
            ? QStringLiteral("color:#7ec850; font-size:11px; font-weight:bold;")
            : QStringLiteral("color:#ffcc66; font-size:11px; font-weight:bold;"));
        if (m_master) {
            m_master->setEnabled(available);
            const QSignalBlocker block(m_master);
            m_master->setChecked(m_model->rfKitEnabled());
        }
        applyMasterGate(m_model->rfKitEnabled());
        return;
    }
    const QString mac      = m_model ? m_model->currentRadioMac() : QString{};
    const bool    haveMac  = !mac.isEmpty();
    if (haveMac) {
        const QString name = m_model->name();
        m_connectionBanner->setText(
            tr("Editing peripherals for %1 (%2)").arg(name, mac));
        m_connectionBanner->setStyleSheet(
            QStringLiteral("color:#7ec850; font-size:11px; font-weight:bold;"));
        if (m_master) { m_master->setEnabled(true); }
        // Refresh the inputs to reflect the just-connected radio's
        // per-MAC values.  Without this the page would still show the
        // previous radio's host/port until the operator clicked Save.
        reloadFromPeripherals();
    } else {
        m_connectionBanner->setText(
            tr("Connect to a radio to edit its peripherals."));
        m_connectionBanner->setStyleSheet(
            QStringLiteral("color:#ffcc66; font-size:11px; font-weight:bold;"));
        if (m_master) { m_master->setEnabled(false); }
    }
    // Apply the master gate AFTER the connection-driven enabled state
    // so the detail tab gates correctly on both axes.
    applyMasterGate(m_model && m_model->rfKitEnabled());
}

// --- Test seams ---

void RfKitPage::setHostForTesting(const QString& host)
{
    if (m_hostEdit) { m_hostEdit->setText(host); }
}

void RfKitPage::setPortForTesting(quint16 port)
{
    if (m_portSpin) { m_portSpin->setValue(static_cast<int>(port)); }
}

void RfKitPage::setAntennaLabelForTesting(int n, const QString& label)
{
    if (n >= 1 && n <= 4 && m_antLabelEdits[n - 1]) {
        m_antLabelEdits[n - 1]->setText(label);
    }
}

void RfKitPage::clickSaveForTesting()
{
    // Call the slot directly so the test is not blocked by the master gate
    // disabling the tab widget (RfKit_Enabled defaults to false on first run).
    saveRf2ksSettings();
}

QPushButton* RfKitPage::testConnectionButtonForTesting() const
{
    return m_testConnBtn;
}

void RfKitPage::onMasterToggled(bool checked)
{
    if (isRemote()) {
        // R-R3-47: the Core switches its amp; the box follows the Core's
        // answer (rfKitEnabled), so a refused request puts it back.
        IStationLink* link = m_model->stationLink();
        const auto outcome = link ? link->requestRfKitEnabled(checked)
            : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
        // Follow-up 3: this page shows the Core's refusal; no toast too.
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
        m_remoteResult = outcome.sent ? QString() : OperatorReasonText::forDisplay(outcome.reason);
        if (!outcome.sent && m_master) {
            const QSignalBlocker block(m_master);
            m_master->setChecked(m_model->rfKitEnabled());
        }
        refreshLiveStatus();
        return;
    }
    if (m_model) {
        m_model->setRfKitEnabled(checked);
    }
    applyMasterGate(checked);
    refreshLiveStatus();  // immediate paint to reflect new gate state
}

void RfKitPage::applyMasterGate(bool enabled)
{
    if (!m_tabs || !m_rf2ksTab) { return; }
    const int idx = m_tabs->indexOf(m_rf2ksTab);
    // Per-radio peripherals refactor (2026-05-26): the master gate AND
    // the peripheral scope availability must both pass for the detail
    // tab to be interactive.  Use currentRadioMac() rather than
    // isConnected() because the latter requires a live RadioConnection
    // -- unit tests that inject a MAC via setLastRadioInfoForTest +
    // setConnectionStateForTest don't stand up a connection object.
    const bool haveMac = isRemote() ? remoteControlAvailable()
                                    : m_model && !m_model->currentRadioMac().isEmpty();
    const bool gateOn  = enabled && haveMac;
    m_tabs->setTabEnabled(idx, gateOn);
    m_rf2ksTab->setEnabled(gateOn);
}

void RfKitPage::refreshLiveStatus()
{
    if (!m_liveStatusLabel || !m_model) { return; }
    if (isRemote()) {
        // R-R3-47: the Core's amp, as the `rfkit` object reports it.
        RfKitModel* rfKit = m_model->rfKitModel();
        if (!rfKit) { return; }
        using Phase = RfKitModel::ConnectionPhase;
        QString text;
        switch (rfKit->connectionPhase()) {
        case Phase::Disabled: text = tr("Off at the Core"); break;
        case Phase::Disconnected: text = tr("Disconnected"); break;
        case Phase::Discovering:
        case Phase::Connecting: text = tr("Connecting at the Core"); break;
        case Phase::Identifying: text = tr("Checking the device"); break;
        case Phase::Retrying: text = tr("Retrying at the Core"); break;
        case Phase::Connected:
            text = tr("Connected: %1 %2").arg(rfKit->deviceNickname(), rfKit->deviceVersion());
            break;
        case Phase::Error:
            text = tr("Error: %1").arg(OperatorReasonText::forDisplay(rfKit->connectionError()));
            break;
        }
        if (!m_remoteResult.isEmpty()) {
            text += QStringLiteral("\n") + m_remoteResult;
        }
        m_liveStatusLabel->setTextFormat(Qt::PlainText);
        m_liveStatusLabel->setText(tr("RF2K-S: %1").arg(text));
        if (m_diagnosticsLabel) {
            // R-R3-49 (parity Task 10): the Core's counts (accessoryData,
            // version 2); an older Core keeps them to itself.
            const IStationLink* link = m_model->stationLink();
            const AccessoryDataModel* data = m_model->accessoryDataModel();
            if (link && link->rfKitCountersAvailable() && data) {
                // Group B fix wave (M7): the response time as the local
                // line shows it, from a Core at accessoryDataVersion 3.
                const QString rtt = link->rfKitResponseTimeAvailable()
                    ? QStringLiteral("%1 ms avg").arg(data->rfkitRttAvgMs())
                    : QStringLiteral("--");
                m_diagnosticsLabel->setTextFormat(Qt::RichText);
                m_diagnosticsLabel->setText(QStringLiteral(
                    "Polls: %1 OK / %2 failed &middot; RTT %3 &middot; Reconnects %4 &middot; "
                    "Connected since %5 &middot; Last poll %6")
                    .arg(data->rfkitPollsOk()).arg(data->rfkitPollsFailed())
                    .arg(rtt).arg(data->rfkitReconnectCount())
                    .arg(clockTime(data->rfkitConnectedSinceMs()),
                         clockTime(data->rfkitLastPollMs())));
            } else {
                m_diagnosticsLabel->setTextFormat(Qt::PlainText);
                m_diagnosticsLabel->setText(tr("The Core keeps the amplifier's connection "
                                               "counts."));
            }
        }
        return;
    }
    Rf2ksConnection* conn = m_model->rfKitConnection();
    if (!conn) { return; }
    const QString status = conn->isConnected()
        ? QStringLiteral("<span style='color:#34c759;'>CONNECTED</span>")
        : QStringLiteral("<span style='color:#e64949;'>DISCONNECTED</span>");
    m_liveStatusLabel->setText(
        QStringLiteral("RF2K-S: %1 &nbsp; %2:%3 &nbsp; %4")
            .arg(status, conn->peerAddress())
            .arg(conn->peerPort())
            .arg(conn->softwareVersion()));
    if (m_diagnosticsLabel) {
        // R-R3-49 (parity Task 10): with the connected-since and last-poll
        // readings a remote window shows from the Core.
        m_diagnosticsLabel->setText(QStringLiteral(
            "Polls: %1 OK / %2 failed &middot; RTT %3 ms avg &middot; Reconnects %4 &middot; "
            "Connected since %5 &middot; Last poll %6")
            .arg(conn->pollsSucceeded()).arg(conn->pollsFailed())
            .arg(conn->rttAvgLast10Ms()).arg(conn->reconnectAttempts())
            .arg(clockTime(conn->connectedSinceMs()), clockTime(conn->lastPollMs())));
    }
}

bool RfKitPage::detailTabIsEnabledForTesting() const
{
    if (!m_tabs || !m_rf2ksTab) { return false; }
    return m_tabs->isTabEnabled(m_tabs->indexOf(m_rf2ksTab));
}

} // namespace NereusSDR
