// src/gui/applets/TunerApplet.cpp
#include "TunerApplet.h"
#include "NyiOverlay.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QButtonGroup>
#include <QLabel>

namespace NereusSDR {

static QProgressBar* makeRelayBar(QWidget* parent)
{
    auto* bar = new QProgressBar(parent);
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setFixedHeight(12);
    bar->setTextVisible(false);
    bar->setStyleSheet(QStringLiteral(
        "QProgressBar {"
        "  background: %1; border: 1px solid %2; border-radius: 2px;"
        "}"
        "QProgressBar::chunk {"
        "  background: %3; border-radius: 1px;"
        "}").arg(Style::kPanelBg, Style::kBorderSubtle, Style::kAccent));
    return bar;
}

TunerApplet::TunerApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

void TunerApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(appletTitleBar(QStringLiteral("ATU Control")));

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // --- Control 1: Forward power gauge (0-200W, yellow@125, red@125) ---
    m_fwdPowerGauge = new HGauge(this);
    m_fwdPowerGauge->setRange(0.0, 200.0);
    m_fwdPowerGauge->setYellowStart(125.0);
    m_fwdPowerGauge->setRedStart(125.0);
    m_fwdPowerGauge->setTitle(QStringLiteral("Fwd Power"));
    m_fwdPowerGauge->setUnit(QStringLiteral("W"));
    vbox->addWidget(m_fwdPowerGauge);
    NyiOverlay::markNyi(m_fwdPowerGauge, QStringLiteral("future"));

    // --- Control 2: SWR gauge (1.0-3.0, yellow@2.5, red@2.5) ---
    m_swrGauge = new HGauge(this);
    m_swrGauge->setRange(1.0, 3.0);
    m_swrGauge->setYellowStart(2.5);
    m_swrGauge->setRedStart(2.5);
    m_swrGauge->setTitle(QStringLiteral("SWR"));
    vbox->addWidget(m_swrGauge);
    NyiOverlay::markNyi(m_swrGauge, QStringLiteral("future"));

    // --- Control 3: Relay position bars (C1 / L / C2) ---
    {
        auto* grid = new QVBoxLayout;
        grid->setSpacing(2);

        const QString labels[3] = {
            QStringLiteral("C1"), QStringLiteral("L"), QStringLiteral("C2")
        };
        QProgressBar** bars[3] = {&m_c1Bar, &m_lBar, &m_c2Bar};

        for (int i = 0; i < 3; ++i) {
            auto* row = new QHBoxLayout;
            row->setSpacing(4);

            auto* lbl = new QLabel(labels[i], this);
            lbl->setFixedWidth(18);
            lbl->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
            row->addWidget(lbl);

            *bars[i] = makeRelayBar(this);
            row->addWidget(*bars[i], 1);

            grid->addLayout(row);
        }

        vbox->addLayout(grid);
    }

    // --- Control 4: TUNE button (non-toggle) ---
    m_tuneBtn = styledButton(QStringLiteral("TUNE"));
    vbox->addWidget(m_tuneBtn);
    NyiOverlay::markNyi(m_tuneBtn, QStringLiteral("future"));

    // --- Control 5: OPERATE/BYPASS/STANDBY — blue toggles, exclusive ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        m_operateBtn = blueToggle(QStringLiteral("Operate"));
        m_operateBtn->setCheckable(true);
        m_operateBtn->setChecked(true);

        m_bypassBtn  = blueToggle(QStringLiteral("Bypass"));
        m_bypassBtn->setCheckable(true);

        m_standbyBtn = blueToggle(QStringLiteral("Standby"));
        m_standbyBtn->setCheckable(true);

        m_modeGroup = new QButtonGroup(this);
        m_modeGroup->setExclusive(true);
        m_modeGroup->addButton(m_operateBtn);
        m_modeGroup->addButton(m_bypassBtn);
        m_modeGroup->addButton(m_standbyBtn);

        row->addWidget(m_operateBtn);
        row->addWidget(m_bypassBtn);
        row->addWidget(m_standbyBtn);

        vbox->addLayout(row);

        NyiOverlay::markNyi(m_operateBtn, QStringLiteral("future"));
        NyiOverlay::markNyi(m_bypassBtn,  QStringLiteral("future"));
        NyiOverlay::markNyi(m_standbyBtn, QStringLiteral("future"));
    }

    // --- Control 6: Antenna switch 1/2/3 — blue toggles, exclusive ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        m_ant1Btn = blueToggle(QStringLiteral("1"));
        m_ant1Btn->setCheckable(true);
        m_ant1Btn->setChecked(true);

        m_ant2Btn = blueToggle(QStringLiteral("2"));
        m_ant2Btn->setCheckable(true);

        m_ant3Btn = blueToggle(QStringLiteral("3"));
        m_ant3Btn->setCheckable(true);

        m_antGroup = new QButtonGroup(this);
        m_antGroup->setExclusive(true);
        m_antGroup->addButton(m_ant1Btn);
        m_antGroup->addButton(m_ant2Btn);
        m_antGroup->addButton(m_ant3Btn);

        row->addWidget(m_ant1Btn);
        row->addWidget(m_ant2Btn);
        row->addWidget(m_ant3Btn);
        row->addStretch();

        vbox->addLayout(row);

        NyiOverlay::markNyi(m_ant1Btn, QStringLiteral("future"));
        NyiOverlay::markNyi(m_ant2Btn, QStringLiteral("future"));
        NyiOverlay::markNyi(m_ant3Btn, QStringLiteral("future"));
    }

    vbox->addStretch();
    root->addWidget(body);
}

void TunerApplet::syncFromModel()
{
    // NYI — future Aries ATU integration phase
}

} // namespace NereusSDR
