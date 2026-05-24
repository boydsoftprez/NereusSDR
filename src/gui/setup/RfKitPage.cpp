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
#include "core/Rf2ksConnection.h"

#include <QCheckBox>
#include <QLabel>
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

    m_master = new QCheckBox(tr("Enable RF-Kit Amplifier integration"), tab);
    m_master->setToolTip(
        tr("Gates the Rf2ks applet in the right-column panel and the RF2K-S "
           "configuration tab below.  Off by default; turn on only when an "
           "RF-Kit RF2K-S amplifier is connected."));
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
    // Placeholder; full content lands in Task 11.
    auto* tab = new QWidget(this);
    auto* lay = new QVBoxLayout(tab);
    lay->addWidget(new QLabel(tr("(RF2K-S configuration lands in Task 11.)"), tab));
    lay->addStretch();
    return tab;
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
    m_tabs->setTabEnabled(idx, enabled);
    m_rf2ksTab->setEnabled(enabled);
}

void RfKitPage::refreshLiveStatus()
{
    if (!m_liveStatusLabel || !m_model) { return; }
    Rf2ksConnection* conn = m_model->rfKitConnection();
    if (!conn) { return; }
    const QString status = conn->isConnected()
        ? QStringLiteral("<span style='color: #34c759;'>CONNECTED</span>")
        : QStringLiteral("<span style='color: #e64949;'>DISCONNECTED</span>");
    m_liveStatusLabel->setText(
        QStringLiteral("RF2K-S: %1 &nbsp; %2:%3 &nbsp; %4")
            .arg(status, conn->peerAddress())
            .arg(conn->peerPort())
            .arg(conn->softwareVersion()));
}

bool RfKitPage::detailTabIsEnabledForTesting() const
{
    if (!m_tabs || !m_rf2ksTab) { return false; }
    return m_tabs->isTabEnabled(m_tabs->indexOf(m_rf2ksTab));
}

} // namespace NereusSDR
