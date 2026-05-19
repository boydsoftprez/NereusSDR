// =================================================================
// src/gui/applets/TunerApplet.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR -- GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       -- per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 ss.5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18  Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout from AetherSDR src/gui/TunerApplet.{h,cpp}
//                 (ATU/tune controls + SWR progress bar). All controls
//                 NYI -- wired in later phase.
//   2026-05-18  Rewired for TGXL by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 NyiOverlay removed; QProgressBar relay bars replaced
//                 with RelayBar; 3-button mode group replaced with single
//                 cycle button; TunerModel signal connections added;
//                 antenna container added (hidden by default).
//                 From AetherSDR src/gui/TunerApplet.cpp [@0cd4559].
// =================================================================

#include "TunerApplet.h"
#include "gui/HGauge.h"
#include "gui/RelayBar.h"
#include "gui/StyleConstants.h"
#include "models/TunerModel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

namespace NereusSDR {

TunerApplet::TunerApplet(RadioModel* model, TunerModel* tunerModel, QWidget* parent)
    : AppletWidget(model, parent)
    , m_tunerModel(tunerModel)
{
    // Post-tune capture timer: after tuning=0 arrives, keep capturing SWR
    // for 400 ms so the final settled value from the TGXL has time to arrive.
    // From AetherSDR src/gui/TunerApplet.cpp:TunerApplet() [@0cd4559]
    m_postTuneTimer = new QTimer(this);
    m_postTuneTimer->setSingleShot(true);
    m_postTuneTimer->setInterval(400);
    connect(m_postTuneTimer, &QTimer::timeout, this, [this]() {
        m_postTuneCapture = false;
        float result = (m_tuneSwr < 900.0f) ? m_tuneSwr : m_swr;
        m_tuneBtn->setText(QString("SWR %1").arg(result, 0, 'f', 2));
        QTimer::singleShot(2500, this, [this]() {
            m_tuneBtn->setText(QStringLiteral("TUNE"));
        });
    });

    buildUI();

    if (m_tunerModel) {
        setTunerModel(m_tunerModel);
    }
}

void TunerApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(appletTitleBar(QStringLiteral("Tuner Genius")));

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // --- Control 1: Forward power gauge (0-200W, red@125; auto-rescaled via setPowerScale) ---
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() [@0cd4559]
    m_fwdPowerGauge = new HGauge(this);
    m_fwdPowerGauge->setRange(0.0, 200.0);
    m_fwdPowerGauge->setYellowStart(125.0);
    m_fwdPowerGauge->setRedStart(125.0);
    m_fwdPowerGauge->setTitle(QStringLiteral("Fwd Pwr"));
    m_fwdPowerGauge->setUnit(QStringLiteral("W"));
    vbox->addWidget(m_fwdPowerGauge);

    // --- Control 2: SWR gauge (1.0-3.0, red@2.5) ---
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() [@0cd4559]
    m_swrGauge = new HGauge(this);
    m_swrGauge->setRange(1.0, 3.0);
    m_swrGauge->setYellowStart(2.5);
    m_swrGauge->setRedStart(2.5);
    m_swrGauge->setTitle(QStringLiteral("SWR"));
    vbox->addWidget(m_swrGauge);

    // --- Bottom row: relay bars (left, 70%) + buttons (right, 30%) ---
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() [@0cd4559]
    auto* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(4);

    // Left column: relay bars C1 / L / C2
    auto* relayCol = new QVBoxLayout;
    relayCol->setSpacing(2);

    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() m_c1Bar / m_lBar / m_c2Bar [@0cd4559]
    m_c1Bar = new RelayBar(QStringLiteral("C1"), this);
    m_lBar  = new RelayBar(QStringLiteral("L"),  this);
    m_c2Bar = new RelayBar(QStringLiteral("C2"), this);
    relayCol->addWidget(m_c1Bar);
    relayCol->addWidget(m_lBar);
    relayCol->addWidget(m_c2Bar);
    bottomRow->addLayout(relayCol, 7);  // 70% width

    // Manual relay adjustment via mousewheel scroll.
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() relayAdjusted connections [@0cd4559]
    connect(m_c1Bar, &RelayBar::relayAdjusted, this,
            [this](int dir) { if (m_tunerModel) m_tunerModel->adjustRelay(0, dir); });
    connect(m_lBar, &RelayBar::relayAdjusted, this,
            [this](int dir) { if (m_tunerModel) m_tunerModel->adjustRelay(1, dir); });
    connect(m_c2Bar, &RelayBar::relayAdjusted, this,
            [this](int dir) { if (m_tunerModel) m_tunerModel->adjustRelay(2, dir); });

    // Right column: TUNE + OPERATE cycle buttons
    auto* btnCol = new QVBoxLayout;
    btnCol->setSpacing(2);

    // TUNE button -- non-toggle; shows "TUNING..." in red while active.
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() m_tuneBtn [@0cd4559]
    m_tuneBtn = new QPushButton(QStringLiteral("TUNE"), this);
    m_tuneBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_tuneBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
        "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
        "QPushButton:hover { background: #204060; }"));
    btnCol->addWidget(m_tuneBtn);

    connect(m_tuneBtn, &QPushButton::clicked, this, [this]() {
        if (m_tunerModel) m_tunerModel->autoTune();
    });

    // Single cycle button: OPERATE -> BYPASS -> STANDBY -> OPERATE.
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() m_operateBtn [@0cd4559]
    m_operateBtn = new QPushButton(QStringLiteral("STANDBY"), this);
    m_operateBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_operateBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
        "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
        "QPushButton:hover { background: #204060; }"));
    btnCol->addWidget(m_operateBtn);

    connect(m_operateBtn, &QPushButton::clicked, this,
            &TunerApplet::cycleOperateState);

    bottomRow->addLayout(btnCol, 3);  // 30% width
    vbox->addLayout(bottomRow);

    // --- Control 6: Antenna container (3 buttons) -- hidden until direct connection active ---
    // Gated: hasDirectConnection() && hasAntennaSwitch().
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() m_antContainer [@0cd4559]
    {
        m_antContainer = new QWidget(this);
        m_antContainer->setVisible(false);
        auto* antRow = new QHBoxLayout(m_antContainer);
        antRow->setContentsMargins(0, 0, 0, 0);
        antRow->setSpacing(2);

        // From AetherSDR src/gui/TunerApplet.cpp:buildUI() makeAntBtn [@0cd4559]
        auto makeAntBtn = [this](const QString& text) {
            auto* btn = new QPushButton(text, this);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            btn->setFixedHeight(22);
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #1a2a3a; border: 1px solid #205070; "
                "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
                "QPushButton:hover { background: #204060; }"));
            return btn;
        };

        m_ant1Btn = makeAntBtn(QStringLiteral("ANT 1"));
        m_ant2Btn = makeAntBtn(QStringLiteral("ANT 2"));
        m_ant3Btn = makeAntBtn(QStringLiteral("ANT 3"));
        antRow->addWidget(m_ant1Btn);
        antRow->addWidget(m_ant2Btn);
        antRow->addWidget(m_ant3Btn);

        // ANT buttons: 1-indexed to match AetherSDR upstream convention.
        // From AetherSDR src/gui/TunerApplet.cpp:buildUI() ant clicked [@0cd4559]
        connect(m_ant1Btn, &QPushButton::clicked, this,
                [this]() { if (m_tunerModel) m_tunerModel->setAntennaA(1); });
        connect(m_ant2Btn, &QPushButton::clicked, this,
                [this]() { if (m_tunerModel) m_tunerModel->setAntennaA(2); });
        connect(m_ant3Btn, &QPushButton::clicked, this,
                [this]() { if (m_tunerModel) m_tunerModel->setAntennaA(3); });

        vbox->addWidget(m_antContainer);
    }

    vbox->addStretch();
    root->addWidget(body);
}

void TunerApplet::setTunerModel(TunerModel* model)
{
    // From AetherSDR src/gui/TunerApplet.cpp:setTunerModel [@0cd4559]
    if (m_tunerModel == model) return;
    m_tunerModel = model;
    if (!m_tunerModel) return;

    // Relay bars: sync on any relay change.
    connect(m_tunerModel, &TunerModel::relayChanged, this, [this]() {
        if (!m_tunerModel) return;
        m_c1Bar->setValue(m_tunerModel->relayC1());
        m_lBar->setValue(m_tunerModel->relayL());
        m_c2Bar->setValue(m_tunerModel->relayC2());
    });

    // State changes -> refresh OPERATE button label + color.
    connect(m_tunerModel, &TunerModel::stateChanged, this, &TunerApplet::syncFromModel);

    // Tuning state: red button + "TUNING..." + post-tune SWR flash.
    // From AetherSDR src/gui/TunerApplet.cpp:setTunerModel() tuningChanged [@0cd4559]
    connect(m_tunerModel, &TunerModel::tuningChanged, this, [this](bool tuning) {
        if (tuning) {
            m_wasTuning = true;
            m_postTuneCapture = false;
            m_postTuneTimer->stop();
            m_tuneSwr = 999.0f;  // reset high so capture tracking works
            m_tuneBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #cc2222; border: 1px solid #ff4444; "
                "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }"));
            m_tuneBtn->setText(QStringLiteral("TUNING..."));
        } else {
            // Restore normal style.
            m_tuneBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
                "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
                "QPushButton:hover { background: #204060; }"));

            // Don't display result immediately -- the final settled SWR from the TGXL
            // often arrives after tuning=0 via TCP.  Start a short capture window.
            // From AetherSDR src/gui/TunerApplet.cpp:setTunerModel() post-tune window [@0cd4559]
            if (m_wasTuning) {
                m_wasTuning = false;
                m_postTuneCapture = true;
                m_tuneSwr = 999.0f;  // reset -- we want the post-tune value, not the sweep
                m_tuneBtn->setText(QStringLiteral("TUNING..."));
                m_postTuneTimer->start();
            } else {
                m_tuneBtn->setText(QStringLiteral("TUNE"));
            }
        }
    });

    // Forward power + SWR from direct TGXL connection.
    connect(m_tunerModel, &TunerModel::metersChanged,
            this, &TunerApplet::updateMeters);

    // Direct connection active -> enable relay bar scrolling.
    // From AetherSDR src/gui/TunerApplet.cpp:setTunerModel() directConnectionChanged [@0cd4559]
    auto updateScrollEnabled = [this]() {
        if (!m_tunerModel) return;
        const bool on = m_tunerModel->hasDirectConnection();
        m_c1Bar->setScrollEnabled(on);
        m_lBar->setScrollEnabled(on);
        m_c2Bar->setScrollEnabled(on);
    };
    connect(m_tunerModel, &TunerModel::directConnectionChanged, this, updateScrollEnabled);
    updateScrollEnabled();

    // Antenna switch: show buttons only when direct connection active AND
    // the TGXL reports antA (models without a switch never send antA).
    // From AetherSDR src/gui/TunerApplet.cpp:setTunerModel() antVisible [@0cd4559]
    auto updateAntVisible = [this]() {
        if (!m_tunerModel) return;
        m_antContainer->setVisible(m_tunerModel->hasDirectConnection()
                                   && m_tunerModel->hasAntennaSwitch());
    };
    connect(m_tunerModel, &TunerModel::directConnectionChanged, this, updateAntVisible);
    connect(m_tunerModel, &TunerModel::antennaAChanged, this,
            [this, updateAntVisible](int antA) {
                updateAntVisible();
                updateAntennaButtons(antA);
            });
    updateAntVisible();
    updateAntennaButtons(m_tunerModel->antennaA());

    // Initial full sync.
    syncFromModel();
}

void TunerApplet::setPowerScale(int maxWatts, bool hasAmplifier)
{
    // From AetherSDR src/gui/TunerApplet.cpp:setPowerScale [@0cd4559]
    if (hasAmplifier) {
        // PGXL: 0-2000 W, red > 1500 W
        m_fwdPowerGauge->setRange(0.0, 2000.0);
        m_fwdPowerGauge->setYellowStart(1500.0);
        m_fwdPowerGauge->setRedStart(1500.0);
    } else if (maxWatts > 100) {
        // Aurora (500 W): 0-600 W, red > 500 W
        m_fwdPowerGauge->setRange(0.0, 600.0);
        m_fwdPowerGauge->setYellowStart(500.0);
        m_fwdPowerGauge->setRedStart(500.0);
    } else {
        // Barefoot radio: 0-200 W, red > 125 W
        m_fwdPowerGauge->setRange(0.0, 200.0);
        m_fwdPowerGauge->setYellowStart(125.0);
        m_fwdPowerGauge->setRedStart(125.0);
    }
}

void TunerApplet::syncFromModel()
{
    // From AetherSDR src/gui/TunerApplet.cpp:syncFromModel [@0cd4559]
    if (!m_tunerModel) return;

    // Relay bars
    m_c1Bar->setValue(m_tunerModel->relayC1());
    m_lBar->setValue(m_tunerModel->relayL());
    m_c2Bar->setValue(m_tunerModel->relayC2());

    // Operate/Bypass/Standby cycle button -- 3-state display:
    //   operate=1, bypass=0  -> OPERATE  (green)
    //   operate=1, bypass=1  -> BYPASS   (orange)
    //   operate=0            -> STANDBY  (default blue)
    // From AetherSDR src/gui/TunerApplet.cpp:syncFromModel operate/bypass display [@0cd4559]
    if (m_tunerModel->isOperate() && !m_tunerModel->isBypass()) {
        m_operateBtn->setText(QStringLiteral("OPERATE"));
        m_operateBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #006030; border: 1px solid #008040; "
            "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background: #007040; }"));
    } else if (m_tunerModel->isOperate() && m_tunerModel->isBypass()) {
        m_operateBtn->setText(QStringLiteral("BYPASS"));
        m_operateBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #8a6000; border: 1px solid #a07000; "
            "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background: #9a7000; }"));
    } else {
        m_operateBtn->setText(QStringLiteral("STANDBY"));
        m_operateBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
            "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background: #204060; }"));
    }
}

void TunerApplet::cycleOperateState()
{
    // From AetherSDR src/gui/TunerApplet.cpp:cycleOperateState [@0cd4559]
    // Cycle: OPERATE -> BYPASS -> STANDBY -> OPERATE
    // (AetherSDR comment at line 184: "OPERATE -> BYPASS -> STANDBY -> OPERATE")
    if (!m_tunerModel) return;

    if (m_tunerModel->isOperate() && !m_tunerModel->isBypass()) {
        // Currently OPERATE -> go to BYPASS
        m_tunerModel->setBypass(true);
    } else if (m_tunerModel->isOperate() && m_tunerModel->isBypass()) {
        // Currently BYPASS -> go to STANDBY
        m_tunerModel->setOperate(false);
    } else {
        // Currently STANDBY -> go to OPERATE
        m_tunerModel->setBypass(false);
        m_tunerModel->setOperate(true);
    }
}

void TunerApplet::updateMeters(float fwdPower, float swr)
{
    // From AetherSDR src/gui/TunerApplet.cpp:updateMeters [@0cd4559]
    m_fwdPower = fwdPower;
    m_swr = swr;
    m_fwdPowerGauge->setValue(fwdPower);
    m_swrGauge->setValue(swr);

    // During the post-tune capture window, record the last non-idle SWR.
    // The TGXL reports the settled SWR shortly after tuning=0 arrives;
    // we take the last value > 1.01 (idle/no-RF reads ~1.00).
    // From AetherSDR src/gui/TunerApplet.cpp:updateMeters post-tune capture [@0cd4559]
    if (m_postTuneCapture && swr > 1.01f) {
        m_tuneSwr = swr;
        m_tuneBtn->setText(QString("SWR %1").arg(swr, 0, 'f', 2));
    }
}

void TunerApplet::updateAntennaButtons(int antA)
{
    // antA is 0-indexed: 0=ANT1, 1=ANT2, 2=ANT3
    // From AetherSDR src/gui/TunerApplet.cpp:updateAntennaButtons [@0cd4559]
    static constexpr const char* kDefault =
        "QPushButton { background: #1a2a3a; border: 1px solid #205070; "
        "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
        "QPushButton:hover { background: #204060; }";
    static constexpr const char* kActive =
        "QPushButton { background: #006030; border: 1px solid #008040; "
        "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }";

    m_ant1Btn->setStyleSheet(antA == 0 ? kActive : kDefault);
    m_ant2Btn->setStyleSheet(antA == 1 ? kActive : kDefault);
    m_ant3Btn->setStyleSheet(antA == 2 ? kActive : kDefault);
}

} // namespace NereusSDR
