// =================================================================
// src/gui/applets/TunerApplet.h  (NereusSDR)
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
//                 cycle button; constructor wired to TunerModel*.
//                 From AetherSDR src/gui/TunerApplet.h [@0cd4559].
// =================================================================

#pragma once
#include "AppletWidget.h"

class QPushButton;
class QWidget;
class QTimer;

namespace NereusSDR {

class HGauge;
class RelayBar;
class TunerModel;

// 4O3A Tuner Genius XL (TGXL) controls.
//
// Layout (top to bottom):
//   1. Fwd Power gauge    -- HGauge (0-200W, red@125; rescaled if PGXL present)
//   2. SWR gauge          -- HGauge (1.0-3.0, red@2.5)
//   3. Relay position bars  -- RelayBar x3 ("C1", "L", "C2"), 0-255
//   4. TUNE button        -- QPushButton (non-toggle, sends autoTune; shows
//                            "TUNING..." in red while active, then "SWR X.XX"
//                            for 2.5 s post-tune)
//   5. OPERATE cycle btn  -- Single QPushButton; click cycles OPERATE->BYPASS->
//                            STANDBY->OPERATE; label + color follows state
//   6. Antenna container  -- 3x QPushButton (ANT 1/2/3); hidden unless
//                            hasDirectConnection() && hasAntennaSwitch()
//
// From AetherSDR src/gui/TunerApplet.h [@0cd4559]
class TunerApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit TunerApplet(RadioModel* model, TunerModel* tunerModel = nullptr,
                         QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("tuner"); }
    QString appletTitle() const override { return QStringLiteral("Tuner Genius"); }
    void    syncFromModel() override;

    // Attach (or replace) the TunerModel. Safe to call with nullptr.
    void setTunerModel(TunerModel* model);

    // Switch Fwd Power gauge scale: barefoot (0-200W, red@125) vs PGXL (0-2000W, red@1500).
    // From AetherSDR src/gui/TunerApplet.h:setPowerScale [@0cd4559]
    void setPowerScale(int maxWatts, bool hasAmplifier);

public slots:
    // Feed forward power (W) and SWR into the gauges.
    // From AetherSDR src/gui/TunerApplet.h:updateMeters [@0cd4559]
    void updateMeters(float fwdPower, float swr);

private slots:
    // Cycle OPERATE -> BYPASS -> STANDBY -> OPERATE.
    // From AetherSDR src/gui/TunerApplet.cpp:cycleOperateState [@0cd4559]
    void cycleOperateState();

    // Highlight the active antenna button (antA is 0-indexed).
    // From AetherSDR src/gui/TunerApplet.cpp:updateAntennaButtons [@0cd4559]
    void updateAntennaButtons(int antA);

private:
    void buildUI();

    TunerModel* m_tunerModel = nullptr;

    // Control 1 -- Forward power gauge (0-200W, red@125; auto-rescaled)
    HGauge* m_fwdPowerGauge = nullptr;
    // Control 2 -- SWR gauge (1.0-3.0, red@2.5)
    HGauge* m_swrGauge       = nullptr;

    // Control 3 -- Relay position bars (C1 / L / C2)
    // From AetherSDR src/gui/TunerApplet.h:m_c1Bar [@0cd4559]
    RelayBar* m_c1Bar = nullptr;
    RelayBar* m_lBar  = nullptr;
    RelayBar* m_c2Bar = nullptr;

    // Control 4 -- TUNE button (non-toggle)
    QPushButton* m_tuneBtn = nullptr;

    // Control 5 -- Single cycle button (OPERATE / BYPASS / STANDBY)
    // From AetherSDR src/gui/TunerApplet.h:m_operateBtn [@0cd4559]
    QPushButton* m_operateBtn = nullptr;

    // Control 6 -- Antenna container (3 buttons); shown only when direct
    // connection active and antenna switch present.
    // From AetherSDR src/gui/TunerApplet.h:m_antContainer [@0cd4559]
    QWidget*     m_antContainer = nullptr;
    QPushButton* m_ant1Btn      = nullptr;
    QPushButton* m_ant2Btn      = nullptr;
    QPushButton* m_ant3Btn      = nullptr;

    // Meter values (updated by updateMeters)
    // From AetherSDR src/gui/TunerApplet.h:m_fwdPower [@0cd4559]
    float m_fwdPower{0.0f};
    float m_swr{1.0f};

    // Post-tune SWR capture state
    // From AetherSDR src/gui/TunerApplet.h:m_wasTuning [@0cd4559]
    bool   m_wasTuning{false};
    bool   m_postTuneCapture{false};
    float  m_tuneSwr{1.0f};
    QTimer* m_postTuneTimer{nullptr};
};

} // namespace NereusSDR
