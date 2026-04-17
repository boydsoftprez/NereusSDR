#pragma once

// =================================================================
// src/gui/applets/TxApplet.h  (NereusSDR)
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
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//                 Wiring deferred to Phase 3M.
// =================================================================

#include "AppletWidget.h"

class QPushButton;
class QSlider;
class QComboBox;
class QLabel;

namespace NereusSDR {

// TxApplet — transmit controls panel (all 15 controls, Phase 3I-1 NYI).
//
// Layout (AetherSDR TxApplet.cpp pattern):
//  1.  Forward Power gauge  — HGauge 0–120 W, red > 100 W
//  2.  SWR gauge            — HGauge 1.0–3.0, red > 2.5
//  3.  RF Power slider row  — label(62) + slider + value(22)
//  4.  Tune Power slider row
//  5.  MOX button           — checkable, red when active
//  6.  TUNE button          — checkable, red + "TUNING..." when active
//  7.  ATU button           — checkable
//  8.  MEM button           — checkable
//  9.  TX Profile combo     — "Default" item
// 10.  Tune mode combo
// 11.  2-Tone test button   — checkable
// 12.  PS-A toggle          — checkable, green when active
// 13.  DUP (full duplex)    — checkable
// 14.  xPA indicator button — checkable
// 15.  SWR protection LED   — QLabel indicator
//
// All controls disabled with NyiOverlay::markNyi("Phase 3I-1").
class TxApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit TxApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId() const override { return QStringLiteral("TX"); }
    QString appletTitle() const override { return QStringLiteral("TX"); }
    void syncFromModel() override; // NYI — empty until Phase 3I-1

private:
    void buildUI();

    // 1. Forward Power gauge
    QWidget* m_fwdPowerGauge  = nullptr;
    // 2. SWR gauge
    QWidget* m_swrGauge       = nullptr;
    // 3. RF Power
    QSlider* m_rfPowerSlider  = nullptr;
    QLabel*  m_rfPowerValue   = nullptr;
    // 4. Tune Power
    QSlider* m_tunePwrSlider  = nullptr;
    QLabel*  m_tunePwrValue   = nullptr;
    // 5. MOX
    QPushButton* m_moxBtn     = nullptr;
    // 6. TUNE
    QPushButton* m_tuneBtn    = nullptr;
    // 7. ATU
    QPushButton* m_atuBtn     = nullptr;
    // 8. MEM
    QPushButton* m_memBtn     = nullptr;
    // 9. TX Profile combo
    QComboBox*   m_profileCombo = nullptr;
    // 10. Tune mode combo
    QComboBox*   m_tuneModeCombo = nullptr;
    // 11. 2-Tone test
    QPushButton* m_twoToneBtn = nullptr;
    // 12. PS-A
    QPushButton* m_psaBtn     = nullptr;
    // 13. DUP (full duplex)
    QPushButton* m_dupBtn     = nullptr;
    // 14. xPA indicator
    QPushButton* m_xpaBtn     = nullptr;
    // 15. SWR protection LED
    QLabel*      m_swrProtLed = nullptr;
};

} // namespace NereusSDR
