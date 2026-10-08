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
// 2026-10-04 - Preserve native platform and remote-host PTY reasons during sync.
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-04 — Retain disabled virtual-audio/IQ controls with precise
//                 capability reasons and a dialect-neutral initial PTY tooltip.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-18 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout mirrors AetherSDR `src/gui/CatApplet.{h,cpp}`
//                 (serial CAT / rigctld enable rows + PTT LEDs).
//                 All controls NYI — wired in later phase.
//   2026-05-10 — Phase 24 (Task 24.1): stripped TCI button row; TCI
//                 controls now live in TciApplet (Phase 21, 0b615a7).
//   2026-10-06 — Remote-window reason in operator words. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 — Reads and switches CAT1 through RadioModel::catControl(),
//                 the Core's CAT in a connected desktop; syncs on the
//                 channel, availability and platform changes it shows.
//                 J.J. Boyd (KG4VCF), AI tooling: Claude Code.
// =================================================================

#include "CatApplet.h"
#include "NyiOverlay.h"
#include "models/RadioModel.h"
#include "core/cat/CatControl.h"
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
    m_control=model ? model->catControl() : nullptr;
    buildUI();
    if (m_control) {
        const auto sync=[this] { syncFromModel(); };
        connect(m_control,&CatControl::channelConfigChanged,this,sync);
        connect(m_control,&CatControl::channelStatusChanged,this,sync);
        connect(m_control,&CatControl::availabilityChanged,this,sync);
        connect(m_control,&CatControl::platformChanged,this,sync);
        connect(m_tcpBtn,&QPushButton::toggled,this,[this](bool enabled) { setCatOne(&CatEndpointConfig::tcpEnabled,enabled); });
        connect(m_ptyBtn,&QPushButton::toggled,this,[this](bool enabled) { setCatOne(&CatEndpointConfig::ptyEnabled,enabled); });
    }
    syncFromModel();
}

void CatApplet::setCatOne(bool CatEndpointConfig::* field, bool enabled)
{
    if (!m_control || !m_control->available()) { return; }
    CatEndpointConfig config=m_control->channelConfig(1); config.*field=enabled;
    const QPointer<CatApplet> lifetime(this); const bool remote=m_control->remote();
    // The Core's new settings follow its answer; a refusal is said by the window.
    m_control->reconfigureChannel(1,config,[lifetime,remote](bool accepted,const QString&) {
        if (lifetime && (!accepted || !remote)) { lifetime->syncFromModel(); }
    });
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
        m_ptyBtn->setToolTip(tr("Enable PTY for CAT1. Configure all four channels individually in Setup → CAT & Network."));
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
        m_vaxBtn->setToolTip(tr("This CAT applet does not control virtual audio."));
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
        const QString iqReason=tr("This CAT applet does not provide I/Q audio output.");
        m_iqBtn->setToolTip(iqReason);
        m_iqRateCombo->setToolTip(iqReason);
    }

    vbox->addStretch();
    root->addWidget(body);
}

void CatApplet::syncFromModel()
{
    CatControl* control=m_control;
    const bool available=control && control->available();
    const bool remote=control && control->remote();
    const CatPlatform platform=control ? control->platform() : CatPlatform{};
    m_tcpBtn->setEnabled(available); m_ptyBtn->setEnabled(available && platform.pty);
    if (!available) {
        const QString reason=control ? control->unavailableReason() : QString();
        m_tcpBtn->setToolTip(reason); m_ptyBtn->setToolTip(reason);
    } else {
        m_tcpBtn->setToolTip(tr("Enable TCP for CAT1. Configure all four channels individually in Setup → CAT & Network."));
        // PTYs are the computer running CAT's (the Core's, from a connected desktop).
        if (!platform.pty) { m_ptyBtn->setToolTip(remote ? tr("The Core's computer has no native PTYs: they are available only on macOS and Linux.") : tr("Native PTYs are available only on macOS and Linux.")); }
        else { m_ptyBtn->setToolTip(tr("Enable %1 PTY for CAT1. Configure all four channels individually in Setup → CAT & Network.").arg(control->channelConfig(1).ptyDialect)); }
    }
    if (!control) { return; }
    const QSignalBlocker tcp(m_tcpBtn),pty(m_ptyBtn);
    m_tcpBtn->setChecked(control->channelConfig(1).tcpEnabled); m_ptyBtn->setChecked(control->channelConfig(1).ptyEnabled);
    for (int i=0;i<4;++i) {
        const CatChannelStatus status=control->channelStatus(i+1);
        const QString state=status.tcp;
        const bool listening=state=="Listening";
        m_tcpLed[i]->setObjectName(QStringLiteral("catTcpLed%1").arg(i+1));
        m_tcpLed[i]->setStyleSheet(QStringLiteral("QLabel { background: %1; color: #c8d8e8; border-radius: 2px; font-size: 8px; font-weight: bold; }").arg(listening ? "#208040" : state.contains("error",Qt::CaseInsensitive) ? "#a04040" : "#405060"));
        m_tcpLed[i]->setToolTip(tr("CAT%1 TCP: %2 · %3:%4 · %5 clients").arg(i+1).arg(state,status.tcpBoundAddress).arg(status.tcpBoundPort).arg(status.tcpClients));
        const QString path=status.ptyPath;
        m_ptyPath[i]->setObjectName(QStringLiteral("catPtyPath%1").arg(i+1));
        m_ptyPath[i]->setText(path.isEmpty() ? QStringLiteral("—") : path);
        m_ptyPath[i]->setToolTip(tr("CAT%1 %2 PTY: %3").arg(i+1).arg(control->channelConfig(i+1).ptyDialect,status.pty));
        m_ptyPath[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
}

} // namespace NereusSDR
