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
// =================================================================

#include "RfKitPage.h"

#include "models/RadioModel.h"
#include "core/AppSettings.h"
#include "core/Rf2ksConnection.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace NereusSDR {

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
    }
    refreshConnectionBanner();  // initial paint

    // Periodic live-status refresh (1 Hz).
    auto* timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &RfKitPage::refreshLiveStatus);
    timer->start();
    refreshLiveStatus();  // initial paint
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
        tr("Gates the Rf2ks applet in the right-column panel and the RF2K-S "
           "configuration tab below.  Off by default; turn on only when an "
           "RF-Kit RF2K-S amplifier is connected.  Setting is scoped to the "
           "currently connected radio."));
    m_master->setChecked(m_model && m_model->rfKitEnabled());
    connect(m_master, &QCheckBox::toggled, this, &RfKitPage::onMasterToggled);
    lay->addWidget(m_master);

    auto* helper = new QLabel(tr(
        "When enabled, the Rf2ks applet appears in the right-column panel, "
        "the analog S-meter switches to 2 kW scale when the amp is in OPERATE, "
        "and TCI band tracking flows to the amp automatically. When disabled, "
        "the applet hides and the RF2K-S tab below greys out."), tab);
    helper->setWordWrap(true);
    helper->setStyleSheet(QStringLiteral("color: #9aa5b1; font-size: 11px;"));
    lay->addWidget(helper);

    m_liveStatusLabel = new QLabel(tab);
    m_liveStatusLabel->setTextFormat(Qt::RichText);
    lay->addWidget(m_liveStatusLabel);

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
    m_setTciBtn   = new QPushButton(tr("Set amp to TCI mode"), connBox);
    m_resetErrBtn = new QPushButton(tr("Reset amp error state"), connBox);
    auto* btnRow  = new QHBoxLayout();
    btnRow->addWidget(m_testConnBtn);
    btnRow->addWidget(m_setTciBtn);
    btnRow->addWidget(m_resetErrBtn);
    connFm->addRow(btnRow);

    root->addWidget(connBox);

    connect(m_testConnBtn, &QPushButton::clicked, this, [this] {
        if (m_model && m_model->rfKitConnection()) {
            m_model->rfKitConnection()->connectToAmp(
                m_hostEdit->text(),
                static_cast<quint16>(m_portSpin->value()));
        }
    });
    connect(m_setTciBtn, &QPushButton::clicked, this, [this] {
        if (m_model && m_model->rfKitConnection()) {
            m_model->rfKitConnection()->setOperationalInterface(
                QStringLiteral("TCI"));
        }
    });
    connect(m_resetErrBtn, &QPushButton::clicked, this, [this] {
        if (m_model && m_model->rfKitConnection()) {
            m_model->rfKitConnection()->resetError();
        }
    });

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
    return tab;
}

void RfKitPage::saveRf2ksSettings()
{
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
    const bool haveMac = m_model && !m_model->currentRadioMac().isEmpty();
    const bool gateOn  = enabled && haveMac;
    m_tabs->setTabEnabled(idx, gateOn);
    m_rf2ksTab->setEnabled(gateOn);
}

void RfKitPage::refreshLiveStatus()
{
    if (!m_liveStatusLabel || !m_model) { return; }
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
        m_diagnosticsLabel->setText(QStringLiteral(
            "Polls: %1 OK / %2 failed &middot; RTT %3 ms avg &middot; Reconnects %4")
            .arg(conn->pollsSucceeded()).arg(conn->pollsFailed())
            .arg(conn->rttAvgLast10Ms()).arg(conn->reconnectAttempts()));
    }
}

bool RfKitPage::detailTabIsEnabledForTesting() const
{
    if (!m_tabs || !m_rf2ksTab) { return false; }
    return m_tabs->isTabEnabled(m_tabs->indexOf(m_rf2ksTab));
}

} // namespace NereusSDR
