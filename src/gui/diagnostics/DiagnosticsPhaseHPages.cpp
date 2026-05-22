// =================================================================
// src/gui/diagnostics/DiagnosticsPhaseHPages.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Implementation for the four sibling Diagnostics
// sub-tabs added in Phase 3P-H. See header for scope.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-20 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include "DiagnosticsPhaseHPages.h"

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HermesLiteBandwidthMonitor.h"
#include "core/SettingsHygiene.h"
#include "models/RadioModel.h"

#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>

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
    auto* form = qobject_cast<QVBoxLayout*>(group->layout());

    auto addRow = [&](const QString& label, QLabel*& out) {
        auto* row = new QHBoxLayout();
        auto* lab = new QLabel(label);
        lab->setMinimumWidth(180);
        out = new QLabel(QStringLiteral("—"));
        row->addWidget(lab);
        row->addWidget(out, 1);
        form->addLayout(row);
    };

    addRow(QStringLiteral("EP6 bytes received:"),  m_ep6BytesLabel);
    addRow(QStringLiteral("EP2 bytes sent:"),      m_ep2BytesLabel);
    addRow(QStringLiteral("LAN PHY throttle:"),    m_throttleLabel);
    addRow(QStringLiteral("EP6 sequence gaps:"),   m_seqGapLabel);

    auto* histGroup = addSection(QStringLiteral("60 s History"));
    auto* histLayout = qobject_cast<QVBoxLayout*>(histGroup->layout());
    m_historyPlaceholder = new QLabel(
        QStringLiteral("60 s history graph — wired in follow-up phase."));
    m_historyPlaceholder->setStyleSheet(QStringLiteral("color: #888;"));
    histLayout->addWidget(m_historyPlaceholder);
    SetupPage::markNyi(m_historyPlaceholder, QStringLiteral("3P-H follow-up"));

    contentLayout()->addStretch();
}

void ConnectionQualityPage::onTick()
{
    if (m_model == nullptr) { return; }
    const HermesLiteBandwidthMonitor& bw = m_model->bwMonitor();
    m_ep6BytesLabel->setText(QString::number(bw.ep6IngressBytesPerSec(), 'f', 0)
                             + QStringLiteral(" B/s"));
    m_ep2BytesLabel->setText(QString::number(bw.ep2EgressBytesPerSec(), 'f', 0)
                             + QStringLiteral(" B/s"));
    m_throttleLabel->setText(bw.isThrottled() ? QStringLiteral("THROTTLED")
                                              : QStringLiteral("ok"));
    m_seqGapLabel->setText(QString::number(bw.throttleEventCount()));
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
    m_resetBtn   = new QPushButton(QStringLiteral("Reset to Defaults"));
    m_forgetBtn  = new QPushButton(QStringLiteral("Forget This Radio"));
    btnRow->addWidget(m_refreshBtn);
    btnRow->addWidget(m_resetBtn);
    btnRow->addWidget(m_forgetBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);

    connect(m_refreshBtn, &QPushButton::clicked, this, &SettingsValidationPage::refresh);
    connect(m_resetBtn,   &QPushButton::clicked, this, &SettingsValidationPage::onResetClicked);
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
    const auto issues = m_model->settingsHygiene().issues();
    if (issues.isEmpty()) {
        m_issueList->addItem(QStringLiteral("✓ No issues — all settings within board capability ranges."));
        return;
    }
    for (const auto& issue : issues) {
        const QString sev =
            issue.severity == SettingsHygiene::Severity::Critical ? QStringLiteral("CRIT") :
            issue.severity == SettingsHygiene::Severity::Warning  ? QStringLiteral("WARN") :
                                                                    QStringLiteral("INFO");
        m_issueList->addItem(QStringLiteral("[%1] %2 — %3")
                                 .arg(sev, issue.summary, issue.detail));
    }
}

void SettingsValidationPage::onResetClicked()
{
    if (m_model == nullptr) { return; }
    const auto reply = QMessageBox::question(
        this, QStringLiteral("Reset Settings"),
        QStringLiteral("Reset all per-board settings to defaults for this radio?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        m_model->settingsHygiene().resetSettingsToDefaults(
            QString{}, m_model->boardCapabilities());
        refresh();
    }
}

void SettingsValidationPage::onForgetClicked()
{
    if (m_model == nullptr) { return; }
    const auto reply = QMessageBox::question(
        this, QStringLiteral("Forget Radio"),
        QStringLiteral("Forget all settings for this radio?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        m_model->settingsHygiene().forgetRadio(QString{});
        refresh();
    }
}

// ── ExportImportConfigPage ───────────────────────────────────────────────────

ExportImportConfigPage::ExportImportConfigPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Export / Import Config"), model, parent)
    , m_model(model)
{
    buildUI();
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

    auto* allGroup = addSection(QStringLiteral("Full Configuration (XML)"));
    auto* allLayout = qobject_cast<QVBoxLayout*>(allGroup->layout());
    auto* btnRow = new QHBoxLayout;
    m_exportAllBtn = new QPushButton(QStringLiteral("Export All Settings…"));
    m_importAllBtn = new QPushButton(QStringLiteral("Import All Settings…"));
    btnRow->addWidget(m_exportAllBtn);
    btnRow->addWidget(m_importAllBtn);
    btnRow->addStretch();
    allLayout->addLayout(btnRow);

    auto* radioGroup = addSection(QStringLiteral("Per-Radio Configuration"));
    auto* radioLayout = qobject_cast<QVBoxLayout*>(radioGroup->layout());
    m_radioSummaryLabel = new QLabel(
        QStringLiteral("Per-radio export will copy only the hardware/<mac>/* "
                       "subtree for the connected radio."));
    m_radioSummaryLabel->setWordWrap(true);
    m_radioSummaryLabel->setStyleSheet(QStringLiteral("color: #888;"));
    radioLayout->addWidget(m_radioSummaryLabel);
    m_exportRadioBtn = new QPushButton(QStringLiteral("Export Connected Radio…"));
    radioLayout->addWidget(m_exportRadioBtn);

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
    AppSettings::instance().save();
    const QString src = AppSettings::instance().filePath();
    const QString dst = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Settings"),
        QStringLiteral("NereusSDR.settings.xml"),
        QStringLiteral("XML (*.xml *.settings)"));
    if (dst.isEmpty()) { return; }
    QFile::remove(dst);
    if (!QFile::copy(src, dst)) {
        QMessageBox::warning(this, QStringLiteral("Export Failed"),
                             QStringLiteral("Could not write to %1").arg(dst));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Export Complete"),
                             QStringLiteral("Settings exported to:\n%1").arg(dst));
}

void ExportImportConfigPage::onImportAllClicked()
{
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
    QMessageBox::information(
        this, QStringLiteral("Per-Radio Export"),
        QStringLiteral("Per-radio export filters by MAC and is wired in a "
                       "follow-up phase. Use 'Export All Settings' for now."));
}

// ── LogsPage ─────────────────────────────────────────────────────────────────

LogsPage::LogsPage(QWidget* parent)
    : SetupPage(QStringLiteral("Logs"), parent)
{
    buildUI();
}

void LogsPage::buildUI()
{
    // Reuse the layout addSection() installs (see ConnectionQualityPage::buildUI
    // for context — #272).
    auto* group = addSection(QStringLiteral("Recent Log"));
    auto* layout = qobject_cast<QVBoxLayout*>(group->layout());
    m_logView = new QPlainTextEdit;
    m_logView->setReadOnly(true);
    m_logView->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: #0a0a18; color: #c8d8e8; "
        "border: 1px solid #304050; font-family: 'Monaco','Menlo',monospace; }"));
    m_logView->setPlaceholderText(QStringLiteral(
        "qCWarning / qCDebug capture is wired in a follow-up phase. "
        "For now, run with QT_LOGGING_TO_CONSOLE=1 and read stderr."));
    m_logView->setMinimumHeight(280);
    layout->addWidget(m_logView);

    m_clearBtn = new QPushButton(QStringLiteral("Clear"));
    layout->addWidget(m_clearBtn, 0, Qt::AlignLeft);
    connect(m_clearBtn, &QPushButton::clicked, m_logView, &QPlainTextEdit::clear);
    SetupPage::markNyi(m_logView, QStringLiteral("3P-H follow-up"));

    contentLayout()->addStretch();
}

} // namespace NereusSDR
