// =================================================================
// src/gui/diagnostics/DiagnosticsPhaseHPages.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Implementation for the four sibling Diagnostics
// sub-tabs added in Phase 3P-H. See header for scope.
//   2026-09-27 - R-R3-49 (remote-window parity Task 22): Logs shows the
//                 Core's recent log in a remote window, and this
//                 computer's labelled; Refresh reads both again. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-20 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 - R-R3-21 / R-R3-10: Settings Validation's Reset and Forget change
//                 the radio's settings, which a remote window holds on
//                 the Core; they are disabled while it does not have them.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 - R3 Setup fix wave (R-R3-21, R-R3-10): Reset and Forget
//                 re-check that availability after their question
//                 returns. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-23 - R-R3-21: Connection Quality's "EP6 sequence gaps" row
//                 shows the EP6 sequence error count, not the throttle
//                 event count. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - R-R3-49: Connection Quality's 60 s history group is
//                 hidden until the history graph is built. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-32 (remote-window parity Task 14): the Connection
//                Quality figures from RadioModel::hl2LinkFigures(), the
//                Core's HL2 link in a remote window and said so;
//                unavailable, never 0, when absent. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Export Connected Radio is built (G-74, B6.1): it saves
//                the connected radio's own settings, the Core's in a
//                remote window, and is disabled with the reason when there
//                is no radio. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: Setup description version 15 ids on
//                the readouts. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// =================================================================

#include "DiagnosticsPhaseHPages.h"

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HermesLiteBandwidthMonitor.h"
#include "core/SettingsHygiene.h"
#include "core/session/IStationLink.h"
#include "core/settings/SettingsBackup.h"
#include "models/RadioModel.h"
#include "gui/SupportDialog.h"
#include "gui/UnbuiltFeatures.h"

#include <QFile>
#include <QFileDialog>
#include <QCloseEvent>
#include <QHideEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QShowEvent>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace NereusSDR {

// ── ConnectionQualityPage ────────────────────────────────────────────────────

ConnectionQualityPage::ConnectionQualityPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Connection Quality"), model, parent)
    , m_model(model)
{
    buildUI();
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ConnectionQualityPage::onTick);
    timer->start(500);
    onTick();
}

void ConnectionQualityPage::buildUI()
{
    // addSection() already installs a QVBoxLayout on `group` (and on
    // `histGroup` below). Creating a second `new QVBoxLayout(group)` here
    // triggers the QLayout "already has a layout" runtime warning — #272
    // captured 7 of these in the W4ORS May 16 log, all from this file.
    // Reuse the layout addSection() already installed instead.
    auto* group = addSection(QStringLiteral("Live Counters"));
    m_liveGroup = group;
    auto* form = qobject_cast<QVBoxLayout*>(group->layout());

    auto addRow = [&](const QString& label, QLabel*& out) {
        auto* row = new QHBoxLayout();
        auto* lab = new QLabel(label);
        lab->setMinimumWidth(180);
        out = new QLabel(QStringLiteral("–"));
        row->addWidget(lab);
        row->addWidget(out, 1);
        form->addLayout(row);
    };

    addRow(QStringLiteral("EP6 bytes received:"),  m_ep6BytesLabel);
    addRow(QStringLiteral("EP2 bytes sent:"),      m_ep2BytesLabel);
    addRow(QStringLiteral("LAN PHY throttle:"),    m_throttleLabel);
    addRow(QStringLiteral("EP6 sequence gaps:"),   m_seqGapLabel);
    // Setup description version 15: the readouts' ids.
    m_ep6BytesLabel->setProperty("nereusSetupId", "diagnostics.connectionQuality.ep6");
    m_ep2BytesLabel->setProperty("nereusSetupId", "diagnostics.connectionQuality.ep2");
    m_throttleLabel->setProperty("nereusSetupId", "diagnostics.connectionQuality.throttle");
    m_seqGapLabel->setProperty("nereusSetupId", "diagnostics.connectionQuality.sequenceGaps");

    auto* histGroup = addSection(QStringLiteral("60 s History"));
    auto* histLayout = qobject_cast<QVBoxLayout*>(histGroup->layout());
    m_historyPlaceholder = new QLabel(
        QStringLiteral("The 60 s history graph is not shown."));
    m_historyPlaceholder->setStyleSheet(QStringLiteral("color: #888;"));
    histLayout->addWidget(m_historyPlaceholder);
    histGroup->setObjectName(QStringLiteral("connectionHistoryGroup"));
    UnbuiltFeatures::hideUnlessBuilt(histGroup, UnbuiltFeature::ConnectionHistory);

    contentLayout()->addStretch();
}

void ConnectionQualityPage::onTick()
{
    if (m_model == nullptr) { return; }
    // R-R3-32 (parity Task 14): this window's HL2 link, or in a remote
    // window the Core's; one the Core has not sent shows as unavailable.
    const RadioModel::Hl2LinkFigures figures = m_model->hl2LinkFigures();
    const bool fromCore = m_model->hl2LinkFiguresFromCore();
    const QString unavailable = tr("Unavailable");
    if (m_liveGroup) {
        m_liveGroup->setTitle(fromCore ? tr("Live Counters, from the Core")
                                       : tr("Live Counters"));
    }
    const auto bytes = [&unavailable](std::optional<double> bps) {
        return bps ? QString::number(*bps, 'f', 0) + QStringLiteral(" B/s") : unavailable;
    };
    m_ep6BytesLabel->setText(bytes(figures.rxBytesPerSecond));
    m_ep2BytesLabel->setText(bytes(figures.txBytesPerSecond));
    m_throttleLabel->setText(!figures.throttled ? unavailable
                             : *figures.throttled ? QStringLiteral("THROTTLED")
                                                  : QStringLiteral("ok"));
    // R-R3-21: the row names EP6 sequence gaps; it showed the LAN throttle
    // event count (the row above already reports throttling).
    m_seqGapLabel->setText(figures.sequenceGaps ? QString::number(*figures.sequenceGaps)
                                                : unavailable);
    const QString source = fromCore ? tr("From the Core") : QString();
    for (QLabel* label : {m_ep6BytesLabel, m_ep2BytesLabel, m_throttleLabel, m_seqGapLabel}) {
        label->setToolTip(source);
    }
}

// ── SettingsValidationPage ───────────────────────────────────────────────────

SettingsValidationPage::SettingsValidationPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Settings Validation"), model, parent)
    , m_model(model)
{
    buildUI();
    refresh();
    if (m_model != nullptr) {
        connect(&m_model->settingsHygiene(), &SettingsHygiene::issuesChanged,
                this, &SettingsValidationPage::refresh);
    }
}

void SettingsValidationPage::buildUI()
{
    // Reuse the layout addSection() installs (see ConnectionQualityPage::buildUI
    // for context — #272).
    auto* group = addSection(QStringLiteral("Validation Issues"));
    auto* layout = qobject_cast<QVBoxLayout*>(group->layout());

    m_issueList = new QListWidget;
    m_issueList->setStyleSheet(
        QStringLiteral("QListWidget { background: #0a0a18; color: #c8d8e8; "
                       "border: 1px solid #304050; }"));
    m_issueList->setMinimumHeight(220);
    layout->addWidget(m_issueList);

    auto* btnRow = new QHBoxLayout;
    m_refreshBtn = new QPushButton(QStringLiteral("Re-validate"));
    m_repairBtn  = new QPushButton(QStringLiteral("Repair Invalid Settings"));
    m_forgetBtn  = new QPushButton(QStringLiteral("Forget This Radio"));
    btnRow->addWidget(m_refreshBtn);
    btnRow->addWidget(m_repairBtn);
    btnRow->addWidget(m_forgetBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(m_refreshBtn, &QPushButton::clicked, this, &SettingsValidationPage::onRevalidateClicked);
    connect(m_repairBtn,  &QPushButton::clicked, this, &SettingsValidationPage::onRepairClicked);
    connect(m_forgetBtn,  &QPushButton::clicked, this, &SettingsValidationPage::onForgetClicked);

    contentLayout()->addStretch();
}

void SettingsValidationPage::refresh()
{
    m_issueList->clear();
    if (m_model == nullptr) {
        m_issueList->addItem(QStringLiteral("(no model)"));
        return;
    }
    const QString unavailable = m_model->settingsHygiene().remoteUnavailableReason();
    if (!unavailable.isEmpty()) {
        m_issueList->addItem(unavailable);
        return;
    }
    const auto issues = m_model->settingsHygiene().issues();
    if (issues.isEmpty()) {
        m_issueList->addItem(QStringLiteral("✓ No issues: every setting is within this radio's range."));
        return;
    }
    for (const auto& issue : issues) {
        const QString sev =
            issue.severity == SettingsHygiene::Severity::Critical ? QStringLiteral("CRIT") :
            issue.severity == SettingsHygiene::Severity::Warning  ? QStringLiteral("WARN") :
                                                                    QStringLiteral("INFO");
        m_issueList->addItem(QStringLiteral("[%1] %2: %3")
                                 .arg(sev, issue.summary, issue.detail));
    }
}

void SettingsValidationPage::onRevalidateClicked()
{
    if (!m_model) { return; }
    const QString mac = m_model->currentRadioMac();
    if (mac.isEmpty()) { return; }
    if (m_model->ownsLocalDsp()) {
        m_model->settingsHygiene().validate(mac, m_model->boardCapabilities());
    } else if (IStationLink* link = m_model->stationLink()) {
        const auto result = link->requestSettingsHygiene("station.validateSettings", mac);
        if (!result.sent) {
            m_model->settingsHygiene().setRemoteUnavailable(result.reason);
        }
    }
}

void SettingsValidationPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    m_stationSettingsAvailable = available;
    const IStationLink* link = m_model ? m_model->stationLink() : nullptr;
    const bool hygiene = !m_model || m_model->ownsLocalDsp()
        || (link && link->settingsHygieneAvailable());
    const QString unavailable = hygiene ? reason : IStationLink::settingsHygieneUnavailableReason();
    gateStationControls({m_refreshBtn}, available && hygiene, unavailable);
    const bool paired = !m_model || m_model->ownsLocalDsp()
        || (link && link->signedInWithDeviceKey());
    gateStationControls({m_forgetBtn}, available && hygiene && paired,
        !hygiene ? unavailable : paired ? reason
            : QStringLiteral("Pair this computer with the Core to forget its radio settings."));
    // G-38: Repair runs on the Core from a remote window (station.
    // repairSettings, settingsHygieneVersion 2), with Forget's gates.
    const bool repair = !m_model || m_model->ownsLocalDsp()
        || (link && link->settingsRepairAvailable());
    gateStationControls({m_repairBtn}, available && repair && paired,
        !repair ? IStationLink::settingsRepairUnavailableReason() : paired ? reason
            : QStringLiteral("Pair this computer with the Core to repair its radio settings."));
}

void SettingsValidationPage::onRepairClicked()
{
    if (m_model == nullptr) { return; }
    const QString mac = m_model->currentRadioMac();
    const auto reply = QMessageBox::question(
        this, QStringLiteral("Repair Settings"),
        QStringLiteral("Repair the settings that are invalid for this radio? Values outside "
                       "its range are brought back into range, and settings for hardware it "
                       "does not have are removed."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    // The Core's settings can go away while the question is open; Yes then
    // changes nothing (R3 Setup fix wave, final review M2).
    if (reply == QMessageBox::Yes && m_stationSettingsAvailable && !mac.isEmpty()
        && mac == m_model->currentRadioMac()) {
        if (m_model->ownsLocalDsp()) {
            QString reason;
            if (m_model->stationOnAirRefusal(&reason)) { return; }
            m_model->settingsHygiene().resetSettingsToDefaults(mac, m_model->boardCapabilities());
        } else if (IStationLink* link = m_model->stationLink(); link && link->settingsRepairAvailable()) {
            link->requestSettingsHygiene("station.repairSettings", mac);
        }
    }
}

void SettingsValidationPage::onForgetClicked()
{
    if (m_model == nullptr) { return; }
    const QString mac = m_model->currentRadioMac();
    const auto reply = QMessageBox::question(
        this, QStringLiteral("Forget Radio"),
        QStringLiteral("Forget all settings for this radio?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    // See onRepairClicked(): re-checked after the question returns.
    if (reply == QMessageBox::Yes && m_stationSettingsAvailable && !mac.isEmpty()
        && mac == m_model->currentRadioMac()) {
        if (m_model->ownsLocalDsp()) {
            QString reason;
            if (m_model->stationOnAirRefusal(&reason)) { return; }
            m_model->settingsHygiene().forgetRadio(mac);
        } else if (IStationLink* link = m_model->stationLink(); link && link->settingsHygieneAvailable()) {
            link->requestSettingsHygiene("station.forgetSettings", mac);
        }
    }
}

// ── ExportImportConfigPage ───────────────────────────────────────────────────

ExportImportConfigPage::ExportImportConfigPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Export / Import Config"), model, parent)
    , m_model(model)
{
    buildUI();
    if (m_model) {
        connect(m_model, &RadioModel::stationSettingsBackupExportFinished, this,
                &ExportImportConfigPage::onExportCompleted);
        connect(m_model, &RadioModel::stationLinkStateChanged, this,
                &ExportImportConfigPage::onLinkStateChanged);
        // Export Connected Radio follows the radio coming and going.
        connect(m_model, &RadioModel::connectionStateChanged, this,
                [this] { refreshExportAvailability(); });
        connect(m_model, &RadioModel::currentRadioChanged, this,
                [this] { refreshExportAvailability(); });
    }
    refreshExportAvailability();
}

ExportImportConfigPage::~ExportImportConfigPage()
{
    m_shuttingDown = true;
    if (m_model) { disconnect(m_model, nullptr, this, nullptr); }
    clearPending(true);
}

void ExportImportConfigPage::closeEvent(QCloseEvent* event)
{
    const QPointer<ExportImportConfigPage> guard(this);
    clearPending(true);
    if (!guard) { return; }
    SetupPage::closeEvent(event);
}

void ExportImportConfigPage::hideEvent(QHideEvent* event)
{
    // SetupDialog hides pages through its stack and hides the whole dialog
    // on close. Neither path necessarily calls this child's closeEvent.
    ++m_visibilityGeneration;
    const QPointer<ExportImportConfigPage> guard(this);
    clearPending(true);
    if (!guard) { return; }
    SetupPage::hideEvent(event);
}

void ExportImportConfigPage::buildUI()
{
    // Reuse the layout addSection() installs (see ConnectionQualityPage::buildUI
    // for context — #272). Same pattern applies to allGroup + radioGroup below.
    auto* fileGroup = addSection(QStringLiteral("Settings File"));
    auto* fileLayout = qobject_cast<QVBoxLayout*>(fileGroup->layout());
    m_settingsPathLabel = new QLabel(
        QStringLiteral("Path: %1").arg(AppSettings::instance().filePath()));
    m_settingsPathLabel->setStyleSheet(QStringLiteral("color: #c8d8e8;"));
    m_settingsPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileLayout->addWidget(m_settingsPathLabel);

    auto* allGroup = addSection(QStringLiteral("Full Configuration"));
    auto* allLayout = qobject_cast<QVBoxLayout*>(allGroup->layout());
    auto* btnRow = new QHBoxLayout;
    m_exportAllBtn = new QPushButton(QStringLiteral("Export All Settings…"));
    m_importAllBtn = new QPushButton(QStringLiteral("Import All Settings…"));
    m_exportAllBtn->setObjectName(QStringLiteral("exportAllSettingsButton"));
    m_importAllBtn->setObjectName(QStringLiteral("importAllSettingsButton"));
    btnRow->addWidget(m_exportAllBtn);
    btnRow->addWidget(m_importAllBtn);
    btnRow->addStretch();
    allLayout->addLayout(btnRow);
    m_exportExplanation = new QLabel;
    m_exportExplanation->setObjectName(QStringLiteral("backupExportExplanation"));
    m_exportExplanation->setWordWrap(true);
    allLayout->addWidget(m_exportExplanation);
    m_importExplanation = new QLabel;
    m_importExplanation->setObjectName(QStringLiteral("backupImportExplanation"));
    m_importExplanation->setWordWrap(true);
    allLayout->addWidget(m_importExplanation);

    auto* radioGroup = addSection(QStringLiteral("Per-Radio Configuration"));
    auto* radioLayout = qobject_cast<QVBoxLayout*>(radioGroup->layout());
    radioGroup->setObjectName(QStringLiteral("exportRadioGroup"));
    m_exportRadioBtn = new QPushButton(QStringLiteral("Export Connected Radio…"));
    m_exportRadioBtn->setObjectName(QStringLiteral("exportRadioButton"));
    auto* radioRow = new QHBoxLayout;
    radioRow->addWidget(m_exportRadioBtn);
    radioRow->addStretch();
    radioLayout->addLayout(radioRow);
    m_radioSummaryLabel = new QLabel;
    m_radioSummaryLabel->setObjectName(QStringLiteral("exportRadioExplanation"));
    m_radioSummaryLabel->setWordWrap(true);
    radioLayout->addWidget(m_radioSummaryLabel);

    connect(m_exportAllBtn,   &QPushButton::clicked, this,
            &ExportImportConfigPage::onExportAllClicked);
    connect(m_importAllBtn,   &QPushButton::clicked, this,
            &ExportImportConfigPage::onImportAllClicked);
    connect(m_exportRadioBtn, &QPushButton::clicked, this,
            &ExportImportConfigPage::onExportRadioClicked);

    contentLayout()->addStretch();
}

void ExportImportConfigPage::onExportAllClicked()
{
    if (m_exportPending) { return; }
    QString reason;
    if (!exportAllowed(&reason)) {
        showExportResult(false, reason);
        return;
    }
    const bool remote = remoteWindow();
    const QPointer<ExportImportConfigPage> guard(this);
    const QPointer<RadioModel> selectedModel(m_model);
    IStationLink* const selectedLink = remote && selectedModel ? selectedModel->stationLink() : nullptr;
    const quint64 selectedGeneration = m_linkGeneration;
    const quint64 selectedVisibility = m_visibilityGeneration;
    const QString destination = chooseExportDestination(remote);
    if (!guard || destination.isEmpty()) { return; }
    // The native picker pumps events. A disconnected Core, changed
    // capability, or newly keyed radio cannot start an export afterward.
    if (!selectedModel || m_model != selectedModel || remoteWindow() != remote
        || m_visibilityGeneration != selectedVisibility
        || (remote && (m_linkGeneration != selectedGeneration
                       || m_model->stationLink() != selectedLink))) {
        showExportResult(false, tr("The connection to the Core changed. Try the backup again."));
        return;
    }
    if (!exportAllowed(&reason)) {
        showExportResult(false, reason);
        return;
    }
    const QByteArray windowXml = AppSettings::instance().exportLocalXml(&reason);
    if (windowXml.isEmpty() || !AppSettings::validateLocalXml(windowXml, &reason)) {
        showExportResult(false, reason.isEmpty()
            ? tr("Could not serialize this window's settings.") : reason);
        return;
    }
    if (!remote) {
        QSaveFile file(destination);
        if (!file.open(QIODevice::WriteOnly) || file.write(windowXml) != windowXml.size()
            || !file.commit()) {
            showExportResult(false, tr("Could not save this window's settings to %1: %2")
                .arg(destination, file.errorString()));
            return;
        }
        showExportResult(true, tr("This window's settings were exported to:\n%1")
            .arg(destination));
        return;
    }

    IStationLink* const link = m_model->stationLink();
    m_exportPending = true;
    m_requestStarting = true;
    m_ownsExport = false;
    m_pendingLink = link;
    m_pendingGeneration = m_linkGeneration;
    m_operationId = 0;
    m_destination = destination;
    m_windowXml = windowXml;
    m_earlyCompletions.clear();
    refreshExportAvailability();
    const IStationLink::CommandOutcome outcome = link->requestSettingsBackupExport();
    if (!guard) { return; }
    m_requestStarting = false;
    if (!m_exportPending) { return; } // A synchronous disconnect retired it.
    if (!selectedModel || m_model != selectedModel || m_model->stationLink() != link
        || m_linkGeneration != m_pendingGeneration
        || !link->stationLinkReady()) {
        clearPending(false);
        showExportResult(false, tr("The connection to the Core changed. Try the backup again."));
        return;
    }
    if (!outcome.sent || outcome.commandId == 0) {
        clearPending(false);
        showExportResult(false, outcome.reason.isEmpty()
            ? tr("The Core could not start the settings backup. Try again.") : outcome.reason);
        return;
    }
    m_ownsExport = true;
    m_operationId = outcome.commandId;
    for (const ExportCompletion& completion : std::as_const(m_earlyCompletions)) {
        if (completion.operationId == m_operationId) {
            const ExportCompletion matched = completion;
            finishExport(matched);
            return;
        }
    }
    m_earlyCompletions.clear();
}

bool ExportImportConfigPage::remoteWindow() const
{
    return m_model && !m_model->ownsLocalDsp();
}

bool ExportImportConfigPage::exportAllowed(QString* reason) const
{
    if (reason) { reason->clear(); }
    if (!m_model) {
        if (reason) { *reason = tr("Connect a radio model before exporting settings."); }
        return false;
    }
    if (m_model && m_model->stationOnAirRefusal(reason)) { return false; }
    if (!remoteWindow()) { return true; }
    if (!m_stationSettingsAvailable) {
        if (reason) {
            *reason = m_stationUnavailableReason.isEmpty()
                ? tr("Connect to the Core before exporting both settings stores.")
                : m_stationUnavailableReason;
        }
        return false;
    }
    IStationLink* const link = m_model->stationLink();
    if (!link || !link->stationLinkReady()) {
        if (reason) { *reason = tr("Connect to the Core before exporting both settings stores."); }
        return false;
    }
    if (!link->settingsBackupExportAvailable()) {
        if (reason) { *reason = tr("A combined backup requires an updated Core."); }
        return false;
    }
    return true;
}

void ExportImportConfigPage::refreshExportAvailability()
{
    if (!m_exportAllBtn) { return; }
    QString reason;
    const bool allowed = !m_exportPending && exportAllowed(&reason);
    m_exportAllBtn->setEnabled(allowed);
    const QString exportText = m_exportPending
        ? tr("A settings backup is in progress.")
        : !allowed ? reason
        : remoteWindow() ? tr("The backup includes this window and the paired Core.")
                         : tr("Exports this window's settings as XML.");
    m_exportExplanation->setText(exportText);
    m_exportAllBtn->setToolTip(exportText);
    m_exportAllBtn->setAccessibleDescription(exportText);

    const bool remote = remoteWindow();
    m_importAllBtn->setEnabled(!remote);
    const QString importText = remote
        ? tr("Importing a combined window and Core backup is not available.")
        : tr("Imports a local XML settings file. Restart after importing.");
    m_importExplanation->setText(importText);
    m_importAllBtn->setToolTip(importText);
    m_importAllBtn->setAccessibleDescription(importText);

    QString radioReason;
    const bool radioAllowed = radioExportAllowed(&radioReason);
    m_exportRadioBtn->setEnabled(radioAllowed);
    const QString radioText = !radioAllowed ? radioReason
        : remote ? tr("Saves only the Core's settings for its connected radio (its hardware, "
                      "antenna, filter and amplifier settings) to a file.")
                 : tr("Saves only the settings kept for the connected radio (its hardware, "
                      "antenna, filter and amplifier settings) to a file.");
    m_radioSummaryLabel->setText(radioText);
    m_exportRadioBtn->setToolTip(radioText);
    m_exportRadioBtn->setAccessibleDescription(radioText);
}

bool ExportImportConfigPage::radioExportAllowed(QString* reason) const
{
    if (reason) { reason->clear(); }
    const bool remote = remoteWindow();
    if (!m_model || m_model->currentRadioMac().isEmpty()) {
        if (reason) {
            *reason = remote ? tr("The Core has no radio connected. Connect one to export "
                                  "its settings.")
                             : tr("Connect a radio to export its settings.");
        }
        return false;
    }
    if (remote && !m_stationSettingsAvailable) {
        if (reason) {
            *reason = m_stationUnavailableReason.isEmpty()
                ? tr("The Core's settings are not available.")
                : m_stationUnavailableReason;
        }
        return false;
    }
    return true;
}

QByteArray ExportImportConfigPage::connectedRadioXml(const AppSettings& settings,
                                                     const QString& mac, QString* error)
{
    if (error) { error->clear(); }
    if (mac.isEmpty()) {
        if (error) { *error = tr("Connect a radio to export its settings."); }
        return {};
    }
    const QMap<QString, QVariant> values = settings.hardwareValues(mac);
    if (values.isEmpty()) {
        if (error) { *error = tr("No settings are saved for the connected radio."); }
        return {};
    }
    // A store that holds this radio's keys and nothing else, serialized
    // the way every settings file is. It is never saved to its path.
    AppSettings radioOnly(QString{});
    const QString prefix = QStringLiteral("hardware/%1/").arg(mac);
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        radioOnly.setValue(prefix + it.key(), it.value());
    }
    return radioOnly.exportLocalXml(error);
}

void ExportImportConfigPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    m_stationSettingsAvailable = available;
    m_stationUnavailableReason = reason;
    if (!available && m_exportPending) {
        const QPointer<ExportImportConfigPage> guard(this);
        clearPending(true);
        if (!guard) { return; }
        showExportResult(false, tr("The Core's settings became unavailable. Try again after reconnecting."));
        if (!guard) { return; }
    }
    refreshExportAvailability();
}

QString ExportImportConfigPage::chooseExportDestination(bool remote)
{
    return QFileDialog::getSaveFileName(
        this, remote ? tr("Export Window and Core Settings") : tr("Export Settings"),
        remote ? QStringLiteral("NereusSDR.nereus-settings")
               : QStringLiteral("NereusSDR.settings.xml"),
        remote ? tr("Nereus settings backup (*.nereus-settings)")
               : tr("XML (*.xml *.settings)"));
}

QString ExportImportConfigPage::chooseRadioExportDestination(const QString& mac)
{
    QString name = mac;
    name.replace(QLatin1Char(':'), QLatin1Char('-'));
    return QFileDialog::getSaveFileName(
        this, tr("Export Connected Radio"),
        QStringLiteral("NereusSDR radio %1.nereus-radio").arg(name),
        tr("Nereus radio settings (*.nereus-radio)"));
}

void ExportImportConfigPage::showExportResult(bool success, const QString& text)
{
    if (success) {
        QMessageBox::information(this, tr("Export Complete"), text);
    } else {
        QMessageBox::warning(this, tr("Export Failed"), text);
    }
}

void ExportImportConfigPage::onLinkStateChanged()
{
    ++m_linkGeneration;
    if (m_exportPending) {
        const QPointer<ExportImportConfigPage> guard(this);
        // The old session's job is retired by the client.
        clearPending(false);
        showExportResult(false, tr("The connection to the Core changed. Try the backup again."));
        if (!guard) { return; }
    }
    refreshExportAvailability();
}

void ExportImportConfigPage::onExportCompleted(quint32 operationId, bool accepted,
                                                const QString& reason,
                                                const QByteArray& coreXml)
{
    if (!m_exportPending) { return; }
    const ExportCompletion completion{operationId, accepted, reason, coreXml};
    if (m_requestStarting) {
        if (m_earlyCompletions.size() < 4) { m_earlyCompletions.append(completion); }
        return;
    }
    if (operationId == m_operationId) { finishExport(completion); }
}

void ExportImportConfigPage::finishExport(const ExportCompletion& completion)
{
    if (!m_exportPending || !m_model || m_model->stationLink() != m_pendingLink
        || m_linkGeneration != m_pendingGeneration || !m_pendingLink->stationLinkReady()
        || !m_stationSettingsAvailable) {
        clearPending(false);
        showExportResult(false, tr("The connection to the Core changed. Try the backup again."));
        return;
    }
    const QString destination = m_destination;
    const QByteArray windowXml = m_windowXml;
    clearPending(false);
    if (!completion.accepted) {
        showExportResult(false, completion.reason.isEmpty()
            ? tr("The Core refused the settings backup. Try again when it is available.")
            : completion.reason);
        return;
    }
    QString error;
    if (!AppSettings::validateLocalXml(windowXml, &error)
        || !AppSettings::validateLocalXml(completion.coreXml, &error)) {
        showExportResult(false, tr("The settings backup contained invalid XML: %1").arg(error));
        return;
    }
    if (!SettingsBackup::writeFile(destination, {windowXml, completion.coreXml}, &error)) {
        showExportResult(false, tr("Could not save the settings backup: %1").arg(error));
        return;
    }
    showExportResult(true, tr("This window and the paired Core were backed up to:\n%1")
        .arg(destination));
}

void ExportImportConfigPage::clearPending(bool cancelOwned)
{
    IStationLink* const cancelLink = cancelOwned && m_exportPending && m_ownsExport && m_model
        && m_model->stationLink() == m_pendingLink
        && m_linkGeneration == m_pendingGeneration && m_pendingLink->stationLinkReady()
        ? m_pendingLink : nullptr;
    const quint32 cancelId = m_operationId;
    m_exportPending = false;
    m_requestStarting = false;
    m_ownsExport = false;
    m_pendingLink = nullptr;
    m_operationId = 0;
    m_windowXml.clear();
    m_destination.clear();
    m_earlyCompletions.clear();
    if (!m_shuttingDown) { refreshExportAvailability(); }
    // The client may emit completion synchronously from cancellation. It
    // cannot finish a retired page job or cancel a newer job with another ID.
    // This must be the final action: the callback may destroy this page.
    if (cancelLink && cancelId != 0) { cancelLink->cancelSettingsBackupExport(cancelId); }
}

void ExportImportConfigPage::onImportAllClicked()
{
    if (remoteWindow()) {
        QMessageBox::information(this, tr("Import Unavailable"),
            tr("Importing a combined window and Core backup is not available."));
        return;
    }
    const QString src = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import Settings"), {},
        QStringLiteral("XML (*.xml *.settings)"));
    if (src.isEmpty()) { return; }
    const auto reply = QMessageBox::question(
        this, QStringLiteral("Replace Settings"),
        QStringLiteral("Replace current settings with the contents of\n%1?\n\n"
                       "A restart is required for changes to take effect.").arg(src),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) { return; }
    const QString dst = AppSettings::instance().filePath();
    QFile::remove(dst);
    if (!QFile::copy(src, dst)) {
        QMessageBox::warning(this, QStringLiteral("Import Failed"),
                             QStringLiteral("Could not write to %1").arg(dst));
        return;
    }
    QMessageBox::information(
        this, QStringLiteral("Import Complete"),
        QStringLiteral("Settings imported. Please restart NereusSDR."));
}

void ExportImportConfigPage::onExportRadioClicked()
{
    QString reason;
    if (!radioExportAllowed(&reason)) {
        showExportResult(false, reason);
        return;
    }
    const QPointer<ExportImportConfigPage> guard(this);
    const QPointer<RadioModel> selectedModel(m_model);
    const QString mac = m_model->currentRadioMac();
    const QString destination = chooseRadioExportDestination(mac);
    if (!guard || destination.isEmpty()) { return; }
    // The native picker pumps events: the radio may have gone or changed.
    if (!selectedModel || m_model != selectedModel || m_model->currentRadioMac() != mac
        || !radioExportAllowed(&reason)) {
        showExportResult(false, reason.isEmpty()
            ? tr("The connected radio changed. Try the export again.") : reason);
        return;
    }
    // In a remote window AppSettings reads the Core's values for this
    // radio through the settings proxy, which holds the connected radio's
    // settings; no command to the Core is needed.
    const QByteArray xml = connectedRadioXml(AppSettings::instance(), mac, &reason);
    if (xml.isEmpty()) {
        showExportResult(false, reason.isEmpty()
            ? tr("Could not read the connected radio's settings.") : reason);
        return;
    }
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly) || file.write(xml) != xml.size() || !file.commit()) {
        showExportResult(false, tr("Could not save the radio's settings to %1: %2")
            .arg(destination, file.errorString()));
        return;
    }
    showExportResult(true, tr("The connected radio's settings were exported to:\n%1")
        .arg(destination));
}

// ── LogsPage ─────────────────────────────────────────────────────────────────

LogsPage::LogsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Logs"), parent)
    , m_model(model)
{
    buildUI();
}

LogsPage::~LogsPage()
{
    // Setup closed with this page showing still holds the Core's log.
    if (m_holdingCoreLog && m_model != nullptr) {
        m_model->removeStationCoreLogViewer();
    }
}

bool LogsPage::isRemote() const
{
    return m_model != nullptr && m_model->role() == RadioModel::Role::Remote;
}

void LogsPage::buildUI()
{
    // Remote-window parity Task 22 (R-R3-49): the Core's recent log first,
    // then this computer's, each labelled.
    if (isRemote()) {
        auto* coreGroup = addSection(QStringLiteral("The Core's Recent Log"));
        auto* coreLayout = qobject_cast<QVBoxLayout*>(coreGroup->layout());
        m_coreLogView = new QPlainTextEdit;
        m_coreLogView->setObjectName(QStringLiteral("coreLogsView"));
        m_coreLogView->setReadOnly(true);
        m_coreLogView->setMaximumBlockCount(2000);
        m_coreLogView->setStyleSheet(QStringLiteral(
            "QPlainTextEdit { background: #0a0a18; color: #c8d8e8; "
            "border: 1px solid #304050; font-family: 'Monaco','Menlo',monospace; }"));
        m_coreLogView->setToolTip(QStringLiteral("The most recent lines of the Core's log"));
        m_coreLogView->setMinimumHeight(200);
        coreLayout->addWidget(m_coreLogView);
        connect(m_model, &RadioModel::stationCoreLogChanged, this, &LogsPage::refreshCoreLog);
        connect(m_model, &RadioModel::stationSupportAvailabilityChanged, this,
                &LogsPage::refreshCoreLog);
    }
    // Reuse the layout addSection() installs (see ConnectionQualityPage::buildUI
    // for context — #272).
    auto* group = addSection(isRemote() ? QStringLiteral("This Computer's Recent Log")
                                        : QStringLiteral("Recent Log"));
    auto* layout = qobject_cast<QVBoxLayout*>(group->layout());
    m_logView = new QPlainTextEdit;
    m_logView->setObjectName(QStringLiteral("logsView"));
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(2000);  // SupportDialog::kMaxLogViewLines
    m_logView->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: #0a0a18; color: #c8d8e8; "
        "border: 1px solid #304050; font-family: 'Monaco','Menlo',monospace; }"));
    m_logView->setToolTip(QStringLiteral("The most recent lines of the NereusSDR log file"));
    m_logView->setMinimumHeight(280);
    layout->addWidget(m_logView);

    auto* buttons = new QHBoxLayout;
    m_refreshBtn = new QPushButton(QStringLiteral("Refresh"));
    m_refreshBtn->setToolTip(isRemote() ? QStringLiteral("Read both logs again")
                                        : QStringLiteral("Read the log file again"));
    buttons->addWidget(m_refreshBtn);
    m_clearBtn = new QPushButton(QStringLiteral("Clear"));
    m_clearBtn->setToolTip(QStringLiteral("Clear this view (the log file is kept)"));
    buttons->addWidget(m_clearBtn);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    connect(m_refreshBtn, &QPushButton::clicked, this, &LogsPage::refresh);
    connect(m_clearBtn, &QPushButton::clicked, m_logView, &QPlainTextEdit::clear);
    if (m_coreLogView != nullptr) {
        m_clearBtn->setToolTip(QStringLiteral("Clear these views (the logs are kept)"));
        connect(m_clearBtn, &QPushButton::clicked, m_coreLogView, &QPlainTextEdit::clear);
    }

    contentLayout()->addStretch();
    refresh();
}

void LogsPage::refresh()
{
    m_logView->setPlainText(SupportDialog::logTailText());
    m_logView->moveCursor(QTextCursor::End);
    if (m_holdingCoreLog) {
        // Read the Core's log again: a new subscription and its backlog.
        m_model->refreshStationCoreLog();
    }
    refreshCoreLog();
}

void LogsPage::refreshCoreLog()
{
    if (m_coreLogView == nullptr) {
        return;
    }
    const QString reason = m_model->stationSupportUnavailableReason();
    if (!reason.isEmpty()) {
        m_coreLogView->setPlainText(reason);
        return;
    }
    const QStringList lines = m_model->stationCoreLog();
    m_coreLogView->setPlainText(lines.isEmpty() ? QStringLiteral("Reading the Core's log...")
                                                : lines.join(QLatin1Char('\n')));
    m_coreLogView->moveCursor(QTextCursor::End);
}

void LogsPage::showEvent(QShowEvent* event)
{
    SetupPage::showEvent(event);
    // The Core's log is followed only while this page shows.
    if (isRemote() && !m_holdingCoreLog) {
        m_holdingCoreLog = true;
        m_model->addStationCoreLogViewer();
    }
    refresh();
}

void LogsPage::hideEvent(QHideEvent* event)
{
    SetupPage::hideEvent(event);
    if (m_holdingCoreLog) {
        m_holdingCoreLog = false;
        m_model->removeStationCoreLogViewer();
    }
}

} // namespace NereusSDR
