// src/gui/applets/TunerApplet.h
#pragma once
#include "AppletWidget.h"

class QPushButton;
class QProgressBar;
class QButtonGroup;

namespace NereusSDR {

class HGauge;

// Aries ATU (Automatic Antenna Tuner) controls.
// NYI — Aries ATU integration phase.
class TunerApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit TunerApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("tuner"); }
    QString appletTitle() const override { return QStringLiteral("Tuner"); }
    void    syncFromModel() override;

private:
    // Gauges
    HGauge* m_fwdPowerGauge  = nullptr;  // 0–200 W, red@150
    HGauge* m_swrGauge        = nullptr;  // 1.0–3.0, red@2.5

    // Relay position bars (C1 / L / C2)
    QProgressBar* m_c1Bar     = nullptr;
    QProgressBar* m_lBar      = nullptr;
    QProgressBar* m_c2Bar     = nullptr;

    // TUNE button
    QPushButton* m_tuneBtn    = nullptr;

    // Mode buttons (mutually exclusive): Operate / Bypass / Standby
    QPushButton*  m_operateBtn = nullptr;
    QPushButton*  m_bypassBtn  = nullptr;
    QPushButton*  m_standbyBtn = nullptr;
    QButtonGroup* m_modeGroup  = nullptr;

    // Antenna switch buttons (mutually exclusive, blue active): 1 / 2 / 3
    QPushButton*  m_ant1Btn    = nullptr;
    QPushButton*  m_ant2Btn    = nullptr;
    QPushButton*  m_ant3Btn    = nullptr;
    QButtonGroup* m_antGroup   = nullptr;
};

} // namespace NereusSDR
