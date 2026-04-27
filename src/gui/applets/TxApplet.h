#pragma once

// =================================================================
// src/gui/applets/TxApplet.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs — chkMOX_CheckedChanged2
//   (29311-29678 [v2.10.3.13]) and chkTUN_CheckedChanged (29978-30157
//   [v2.10.3.13]); original licence from Thetis source is included below.
//
// Layout from AetherSDR TxApplet.{h,cpp} (GPLv3, see AetherSDR attribution
// block below).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//                 Wiring deferred to Phase 3M.
//   2026-04-26 — Phase 3M-1a H.3: TUNE/MOX/Tune-Power/RF-Power deep-wired.
//                 Out-of-phase controls hidden with TODO comments.
//                 syncFromModel() activates. setCurrentBand(Band) added for
//                 tune-power slider sync on band change.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// =================================================================
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
// =================================================================

#include "AppletWidget.h"
#include "models/Band.h"

class QPushButton;
class QSlider;
class QComboBox;
class QLabel;

namespace NereusSDR {

class HGauge;

// TxApplet — transmit controls panel.
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
// 11.  2-Tone test button   — hidden until Phase 3M-3 (out-of-phase)
// 12.  PS-A toggle          — hidden until Phase 3M-4 (out-of-phase)
// 13.  DUP (full duplex)    — checkable
// 14.  xPA indicator button — checkable
// 15.  SWR protection LED   — QLabel indicator
//
// Phase 3M-1a H.3: TUNE/MOX/Tune-Power/RF-Power are deep-wired.
// Out-of-phase controls (2-Tone, PS-A) are hidden.
class TxApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit TxApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId() const override { return QStringLiteral("TX"); }
    QString appletTitle() const override { return QStringLiteral("TX"); }
    void syncFromModel() override;

    // Called by MainWindow when band changes so the Tune Power slider
    // can reflect the stored per-band tune power.
    // Phase 3M-1a H.3.
    void setCurrentBand(Band band);

private:
    void buildUI();
    void wireControls();  // called after buildUI() — attaches signals/slots

    // 1. Forward Power gauge
    HGauge*  m_fwdPowerGauge  = nullptr;
    // 2. SWR gauge
    QWidget* m_swrGauge       = nullptr;
    // EMA smoothing state for fwd-power gauge (Thetis-style envelope detector
    // not yet ported; this is a simple alpha=0.25 exponential-moving-average
    // to keep the displayed value calm — RadioStatus::powerChanged fires
    // ~twice per hardware sample, which makes raw values visibly jittery).
    double   m_fwdPowerSmoothedW{0.0};
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

    // Current band — used to resolve per-band tune power.
    // Updated by setCurrentBand() when PanadapterModel::bandChanged fires.
    Band m_currentBand{Band::Band20m};

    // Flag preventing echo loops between the model and the UI.
    bool m_updatingFromModel{false};
};

} // namespace NereusSDR
