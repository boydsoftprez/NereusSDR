// =================================================================
// src/gui/applets/AmpApplet.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5 requirements.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-18  Ported in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//                 Layout from AetherSDR src/gui/AmpApplet.{h,cpp} [@0cd4559].
//                 Changes from upstream: AppletWidget base; RadioModel* ctor;
//                 NereusSDR HGauge setter API replaces positional constructor.
// =================================================================

#include "AmpApplet.h"
#include "gui/HGauge.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>

namespace NereusSDR {

// From AetherSDR src/gui/AmpApplet.cpp:11-77 [@0cd4559]
AmpApplet::AmpApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QWidget(this);
    auto* topLayout = new QVBoxLayout(this);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);
    topLayout->addWidget(appletTitleBar(appletTitle()));
    topLayout->addWidget(root);

    auto* vbox = new QVBoxLayout(root);
    // From AetherSDR src/gui/AmpApplet.cpp:14-16 [@0cd4559]
    vbox->setContentsMargins(4, 2, 4, 2);
    vbox->setSpacing(2);

    // Fwd Power gauge: 0-2000W
    // From AetherSDR src/gui/AmpApplet.cpp:18-22 [@0cd4559]
    m_fwdGauge = new HGauge(this);
    m_fwdGauge->setRange(0.0, 2000.0);
    m_fwdGauge->setYellowStart(1000.0);
    m_fwdGauge->setRedStart(1500.0);
    m_fwdGauge->setTitle(QStringLiteral("Fwd Pwr"));
    m_fwdGauge->setTickLabels({
        QStringLiteral("0"),
        QStringLiteral("500"),
        QStringLiteral("1000"),
        QStringLiteral("1.5k"),
        QStringLiteral("2k")
    });
    vbox->addWidget(m_fwdGauge);

    // SWR gauge: 1-3
    // From AetherSDR src/gui/AmpApplet.cpp:24-28 [@0cd4559]
    m_swrGauge = new HGauge(this);
    m_swrGauge->setRange(1.0, 3.0);
    m_swrGauge->setYellowStart(2.0);
    m_swrGauge->setRedStart(2.5);
    m_swrGauge->setTitle(QStringLiteral("SWR"));
    m_swrGauge->setValue(1.0);  // start at minimum (1:1)
    m_swrGauge->setTickLabels({
        QStringLiteral("1"),
        QStringLiteral("1.5"),
        QStringLiteral("2"),
        QStringLiteral("2.5"),
        QStringLiteral("3")
    });
    vbox->addWidget(m_swrGauge);

    // Temp gauge: 0-100 deg C
    // From AetherSDR src/gui/AmpApplet.cpp:30-34 [@0cd4559]
    m_tempGauge = new HGauge(this);
    m_tempGauge->setRange(0.0, 100.0);
    m_tempGauge->setYellowStart(55.0);
    m_tempGauge->setRedStart(80.0);
    m_tempGauge->setTitle(QStringLiteral("Temp"));
    m_tempGauge->setUnit(QStringLiteral("°C"));
    m_tempGauge->setTickLabels({
        QStringLiteral("0"),
        QStringLiteral("30"),
        QStringLiteral("55"),
        QStringLiteral("80"),
        QStringLiteral("100")
    });
    vbox->addWidget(m_tempGauge);

    // From AetherSDR src/gui/AmpApplet.cpp:36 [@0cd4559]
    vbox->addSpacing(8);

    // PGXL direct telemetry -- stacked labels + OPERATE button
    // From AetherSDR src/gui/AmpApplet.cpp:37-39 [@0cd4559]
    static const char* kLabelStyle =
        "QLabel { color: #c8d8e8; font-size: 10px; }";

    // From AetherSDR src/gui/AmpApplet.cpp:41-76 [@0cd4559]
    auto* telRow = new QHBoxLayout;
    telRow->setSpacing(4);

    auto* telStack = new QVBoxLayout;
    telStack->setSpacing(0);
    telStack->setContentsMargins(0, 0, 0, 0);

    m_powerLabel = new QLabel(root);
    m_powerLabel->setStyleSheet(QLatin1String(kLabelStyle));
    m_powerLabel->hide();
    telStack->addWidget(m_powerLabel);

    m_meffLabel = new QLabel(root);
    m_meffLabel->setStyleSheet(QLatin1String(kLabelStyle));
    m_meffLabel->hide();
    telStack->addWidget(m_meffLabel);

    telRow->addLayout(telStack);

    // From AetherSDR src/gui/AmpApplet.cpp:60 [@0cd4559]
    telRow->addSpacing(30);

    // From AetherSDR src/gui/AmpApplet.cpp:62-74 [@0cd4559]
    m_operateBtn = new QPushButton(QStringLiteral("OPERATE"), root);
    m_operateBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_operateBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
            "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background: #204060; }"));
    m_operateBtn->hide();
    connect(m_operateBtn, &QPushButton::clicked, this, [this]() {
        // Toggle: if currently in OPERATE state -> request standby, else -> request operate.
        // From AetherSDR src/gui/AmpApplet.cpp:70-72 [@0cd4559]
        bool isOp = (m_operateBtn->text() == QStringLiteral("OPERATE"));
        emit operateToggled(!isOp);
    });
    telRow->addWidget(m_operateBtn, 1);

    vbox->addLayout(telRow);
}

// From AetherSDR src/gui/AmpApplet.cpp:79-82 [@0cd4559]
void AmpApplet::setFwdPower(float watts)
{
    m_fwdGauge->setValue(static_cast<double>(watts));
}

// From AetherSDR src/gui/AmpApplet.cpp:84-87 [@0cd4559]
void AmpApplet::setSwr(float swr)
{
    m_swrGauge->setValue(static_cast<double>(swr));
}

// From AetherSDR src/gui/AmpApplet.cpp:89-92 [@0cd4559]
void AmpApplet::setTemp(float degC)
{
    m_tempGauge->setValue(static_cast<double>(degC));
}

// From AetherSDR src/gui/AmpApplet.cpp:94-98 [@0cd4559]
void AmpApplet::setDrainCurrent(float amps)
{
    m_drainAmps = amps;
    updatePowerLabel();
}

// From AetherSDR src/gui/AmpApplet.cpp:100-104 [@0cd4559]
void AmpApplet::setMainsVoltage(int volts)
{
    m_mainsVolts = volts;
    updatePowerLabel();
}

// From AetherSDR src/gui/AmpApplet.cpp:106-113 [@0cd4559]
void AmpApplet::updatePowerLabel()
{
    m_powerLabel->setText(
        QStringLiteral("Volts: %1V  Amps: %2A")
            .arg(m_mainsVolts)
            .arg(static_cast<double>(m_drainAmps), 0, 'f', 1));
    m_powerLabel->show();
}

// From AetherSDR src/gui/AmpApplet.cpp:115-135 [@0cd4559]
void AmpApplet::setState(const QString& state)
{
    // Match TGXL OPERATE button style: green when operating, default when standby.
    // PGXL states: IDLE (ready), TRANSMIT_A/TRANSMIT_B (keyed), OPERATE, STANDBY, POWERUP, FAULT
    bool operating = (state == QStringLiteral("IDLE")
                      || state == QStringLiteral("OPERATE")
                      || state.startsWith(QStringLiteral("TRANSMIT")));
    if (operating) {
        m_operateBtn->setText(QStringLiteral("OPERATE"));
        m_operateBtn->setStyleSheet(
            QStringLiteral(
                "QPushButton { background: #006030; border: 1px solid #008040; "
                "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }"
                "QPushButton:hover { background: #007040; }"));
    } else {
        m_operateBtn->setText(QStringLiteral("STANDBY"));
        m_operateBtn->setStyleSheet(
            QStringLiteral(
                "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
                "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
                "QPushButton:hover { background: #204060; }"));
    }
    m_operateBtn->show();
}

// From AetherSDR src/gui/AmpApplet.cpp:137-141 [@0cd4559]
void AmpApplet::setMeff(const QString& meff)
{
    m_meffLabel->setText(QStringLiteral("MEffA:   %1").arg(meff));
    m_meffLabel->show();
}


// Phase 3P-II Phase 4 Task 88: update PGXL connected flag for context menu.
void AmpApplet::setPgxlConnected(bool connected)
{
    m_pgxlConnected = connected;
}

// Phase 3P-II Phase 4 Task 88: right-click context menu.
// Menu structure per design doc ss5.9:
//   Open PGXL Advanced...        -> navigationRequested("pgxlAdvanced")
//   (separator)
//   Disconnect / Reconnect        -> connectionToggleRequested()
//   Copy diagnostics to clipboard -> diagnosticsCopyRequested()
void AmpApplet::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu* menu = buildContextMenu(this);
    menu->exec(ev->globalPos());
    menu->deleteLater();
}

QMenu* AmpApplet::buildContextMenu(QObject* menuParent)
{
    auto* menu = new QMenu(qobject_cast<QWidget*>(menuParent));

    // Open PGXL Advanced...
    auto* openAdvancedAction = menu->addAction(QStringLiteral("Open PGXL Advanced..."));
    connect(openAdvancedAction, &QAction::triggered, this, [this]() {
        emit navigationRequested(QStringLiteral("pgxlAdvanced"));
    });

    menu->addSeparator();

    // Disconnect or Reconnect depending on current state
    const QString toggleLabel = m_pgxlConnected
        ? QStringLiteral("Disconnect")
        : QStringLiteral("Reconnect");
    auto* toggleAction = menu->addAction(toggleLabel);
    connect(toggleAction, &QAction::triggered, this, [this]() {
        emit connectionToggleRequested();
    });

    // Copy diagnostics to clipboard
    auto* copyDiagAction = menu->addAction(QStringLiteral("Copy diagnostics to clipboard"));
    connect(copyDiagAction, &QAction::triggered, this, [this]() {
        emit diagnosticsCopyRequested();
    });

    return menu;
}
} // namespace NereusSDR
