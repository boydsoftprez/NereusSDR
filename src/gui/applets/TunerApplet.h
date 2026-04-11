// src/gui/applets/TunerApplet.h
#pragma once
#include "AppletWidget.h"

class QPushButton;
class QProgressBar;
class QButtonGroup;

namespace NereusSDR {

class HGauge;

// Aries ATU (Automatic Antenna Tuner) controls.
// NYI — future Aries ATU integration phase.
//
// Controls:
//   1. Fwd Power gauge      — HGauge (0-200W, yellow@125, red@125)
//   2. SWR gauge            — HGauge (1.0-3.0, yellow@2.5, red@2.5)
//   3. Relay position bars  — 3x QProgressBar ("C1", "L", "C2")
//   4. TUNE button          — QPushButton (non-toggle, sends tune command)
//   5. OPERATE/BYPASS/STANDBY — 3x QPushButton blue toggle (exclusive)
//   6. Antenna switch (1/2/3) — 3x QPushButton blue toggle (exclusive)
class TunerApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit TunerApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("tuner"); }
    QString appletTitle() const override { return QStringLiteral("ATU Control"); }
    void    syncFromModel() override;

private:
    void buildUI();

    // Control 1 — Forward power gauge (0-200W, yellow@125, red@125)
    HGauge* m_fwdPowerGauge  = nullptr;
    // Control 2 — SWR gauge (1.0-3.0, yellow@2.5, red@2.5)
    HGauge* m_swrGauge        = nullptr;

    // Control 3 — Relay position bars (C1 / L / C2)
    QProgressBar* m_c1Bar     = nullptr;
    QProgressBar* m_lBar      = nullptr;
    QProgressBar* m_c2Bar     = nullptr;

    // Control 4 — TUNE button (non-toggle)
    QPushButton* m_tuneBtn    = nullptr;

    // Control 5 — Mode buttons (blue toggle, mutually exclusive): Operate / Bypass / Standby
    QPushButton*  m_operateBtn = nullptr;
    QPushButton*  m_bypassBtn  = nullptr;
    QPushButton*  m_standbyBtn = nullptr;
    QButtonGroup* m_modeGroup  = nullptr;

    // Control 6 — Antenna switch (blue toggle, mutually exclusive): 1 / 2 / 3
    QPushButton*  m_ant1Btn    = nullptr;
    QPushButton*  m_ant2Btn    = nullptr;
    QPushButton*  m_ant3Btn    = nullptr;
    QButtonGroup* m_antGroup   = nullptr;
};

} // namespace NereusSDR
