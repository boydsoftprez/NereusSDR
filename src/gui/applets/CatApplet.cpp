// =================================================================
// src/gui/applets/CatApplet.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout mirrors AetherSDR `src/gui/CatApplet.{h,cpp}`
//                 (serial CAT / rigctld enable rows + PTT LEDs).
//                 All controls NYI — wired in later phase.
//   2026-05-10 — Phase 24 (Task 24.1): stripped TCI button row; TCI
//                 controls now live in TciApplet (Phase 21, 0b615a7).
// =================================================================

#include "CatApplet.h"
#include "NyiOverlay.h"
#include "models/RadioModel.h"
#include "core/cat/CatService.h"
#include <QSignalBlocker>
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>

namespace NereusSDR {

static QLabel* makeLed(const QString& name, QWidget* parent)
{
    auto* led = new QLabel(name, parent);
    led->setFixedSize(24, 14);
    led->setAlignment(Qt::AlignCenter);
    led->setStyleSheet(QStringLiteral(
        "QLabel { background: #405060; color: #c8d8e8; border-radius: 2px;"
        " font-size: 8px; font-weight: bold; }"));
    return led;
}

static QLabel* makePathLabel(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 9px; }").arg(Style::kTextSecondary));
    return lbl;
}

CatApplet::CatApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    m_service=model ? model->catService() : nullptr;
    m_localHost=model && model->ownsLocalDsp();
    buildUI();
    if (model && model->catService()) {
        const QPointer<CatService> service=m_service;
        connect(service,&CatService::configurationChanged,this,[this] { syncFromModel(); });
        connect(service,&CatService::channelStateChanged,this,[this] { syncFromModel(); });
        connect(service,&CatService::clientCountChanged,this,[this] { syncFromModel(); });
        connect(service,&CatService::ptyPathChanged,this,[this] { syncFromModel(); });
        connect(service,&CatService::transportStateChanged,this,[this] { syncFromModel(); });
        connect(m_tcpBtn,&QPushButton::toggled,this,[this,service](bool enabled) {
            if (!service) { return; }
            CatEndpointConfig config=service->channelConfig(1); config.tcpEnabled=enabled;
            const QPointer<CatApplet> lifetime(this);
            service->reconfigureChannel(1,config); if (lifetime) { syncFromModel(); }
        });
        connect(m_ptyBtn,&QPushButton::toggled,this,[this,service](bool enabled) {
            if (!service) { return; }
            CatEndpointConfig config=service->channelConfig(1); config.ptyEnabled=enabled;
            const QPointer<CatApplet> lifetime(this);
            service->reconfigureChannel(1,config); if (lifetime) { syncFromModel(); }
        });
    }
    syncFromModel();
}

void CatApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    // Do NOT add appletTitleBar() here — AppletPanelWidget::wrapWithTitleBar
    // already prepends a host-side title bar from appletTitle(). Adding our
    // own here results in a double header. Same fix in PureSignalApplet.

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // --- Control 1: CAT TCP enable + 4 status LEDs (A/B/C/D) ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_tcpBtn = greenToggle(QStringLiteral("TCP"));
        m_tcpBtn->setCheckable(true);
        row->addWidget(m_tcpBtn);

        const QString ledNames[4] = {
            QStringLiteral("A"), QStringLiteral("B"),
            QStringLiteral("C"), QStringLiteral("D")
        };
        for (int i = 0; i < 4; ++i) {
            m_tcpLed[i] = makeLed(ledNames[i], this);
            row->addWidget(m_tcpLed[i]);
        }
        row->addStretch();

        vbox->addLayout(row);
        m_tcpBtn->setObjectName("catTcpButton");
        m_tcpBtn->setToolTip(tr("Enable TCP for CAT1. Configure all four channels individually in Setup → CAT & Network."));
    }

    // --- Control 2: CAT PTY enable + 4 path labels ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_ptyBtn = greenToggle(QStringLiteral("PTY"));
        m_ptyBtn->setCheckable(true);
        row->addWidget(m_ptyBtn);

        for (int i = 0; i < 4; ++i) {
            m_ptyPath[i] = makePathLabel(
                QStringLiteral("—"), this);
            row->addWidget(m_ptyPath[i]);
        }
        row->addStretch();

        vbox->addLayout(row);
        m_ptyBtn->setObjectName("catPtyButton");
        m_ptyBtn->setToolTip(tr("Enable Thetis PTY for CAT1. Configure all four channels individually in Setup → CAT & Network."));
    }

    vbox->addWidget(divider());

    // --- Control 3: VAX enable + 4 channel status labels ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_vaxBtn = greenToggle(QStringLiteral("VAX"));
        m_vaxBtn->setCheckable(true);
        row->addWidget(m_vaxBtn);

        for (int i = 0; i < 4; ++i) {
            m_vaxStatus[i] = makeLed(QStringLiteral("Ch%1").arg(i + 1), this);
            row->addWidget(m_vaxStatus[i]);
        }
        row->addStretch();

        vbox->addLayout(row);
        NyiOverlay::markNyi(m_vaxBtn, QStringLiteral("3-VAX"));
    }

    // --- Control 4: VAX IQ enable + rate combo ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_iqBtn = greenToggle(QStringLiteral("IQ"));
        m_iqBtn->setCheckable(true);
        row->addWidget(m_iqBtn);

        m_iqRateCombo = new QComboBox(this);
        m_iqRateCombo->addItems({
            QStringLiteral("48000"),
            QStringLiteral("96000"),
            QStringLiteral("192000")
        });
        applyComboStyle(m_iqRateCombo);
        row->addWidget(m_iqRateCombo, 1);

        vbox->addLayout(row);

        NyiOverlay::markNyi(m_iqBtn,       QStringLiteral("3-VAX"));
        NyiOverlay::markNyi(m_iqRateCombo, QStringLiteral("3-VAX"));
    }

    vbox->addStretch();
    root->addWidget(body);
}

void CatApplet::syncFromModel()
{
    CatService* service=m_service;
    const bool available=m_localHost && service;
    m_tcpBtn->setEnabled(available); m_ptyBtn->setEnabled(available);
#if !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)
    m_ptyBtn->setEnabled(false); m_ptyBtn->setToolTip(tr("Native PTYs are available only on macOS and Linux."));
#endif
    if (!available) {
        const QString reason=tr("CAT setup belongs to the local host. Configure it on the computer running the Core.");
        m_tcpBtn->setToolTip(reason); m_ptyBtn->setToolTip(reason);
    }
    if (!service) { return; }
    const QSignalBlocker tcp(m_tcpBtn),pty(m_ptyBtn);
    if (available) { m_ptyBtn->setToolTip(tr("Enable %1 PTY for CAT1. Configure all four channels individually in Setup → CAT & Network.").arg(service->channelConfig(1).ptyDialect)); }
    m_tcpBtn->setChecked(service->channelConfig(1).tcpEnabled); m_ptyBtn->setChecked(service->channelConfig(1).ptyEnabled);
    for (int i=0;i<4;++i) {
        const QString state=service->transportState(i+1,CatTransportKind::Tcp);
        const bool listening=state=="Listening";
        m_tcpLed[i]->setObjectName(QStringLiteral("catTcpLed%1").arg(i+1));
        m_tcpLed[i]->setStyleSheet(QStringLiteral("QLabel { background: %1; color: #c8d8e8; border-radius: 2px; font-size: 8px; font-weight: bold; }").arg(listening ? "#208040" : state.contains("error",Qt::CaseInsensitive) ? "#a04040" : "#405060"));
        m_tcpLed[i]->setToolTip(tr("CAT%1 TCP: %2 · %3:%4 · %5 clients").arg(i+1).arg(state,service->boundAddress(i+1).toString()).arg(service->boundPort(i+1)).arg(service->clientCount(i+1)));
        const QString path=service->ptySlavePath(i+1);
        m_ptyPath[i]->setObjectName(QStringLiteral("catPtyPath%1").arg(i+1));
        m_ptyPath[i]->setText(path.isEmpty() ? QStringLiteral("—") : path);
        m_ptyPath[i]->setToolTip(tr("CAT%1 %2 PTY: %3").arg(i+1).arg(service->channelConfig(i+1).ptyDialect,service->transportState(i+1,CatTransportKind::Pty)));
        m_ptyPath[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
}

} // namespace NereusSDR
