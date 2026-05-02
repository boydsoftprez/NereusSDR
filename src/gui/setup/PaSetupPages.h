#pragma once

// =================================================================
// src/gui/setup/PaSetupPages.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// Setup IA reshape Phase 2 — top-level "PA" category placeholder pages.
// Mirrors Thetis tpPowerAmplifier (setup.designer.cs:47366-47371
// [v2.10.3.13]) which hosts a tab control with two sub-tabs:
//   - tpGainByBand → PaGainByBandPage (placeholder for Phase 3M-3)
//   - tpWattMeter  → PaWattMeterPage  (placeholder for Phase 3)
// NereusSDR adds a third NereusSDR-spin page (PaValuesPage) for a richer
// always-visible diagnostic readout — Thetis ships its panelPAValues block
// (setup.designer.cs:51155-51177 [v2.10.3.13]) inside the Watt Meter tab.
//
// Phase 2 ships these as placeholders only.  Phase 3+ migrates the live
// PaCalibrationGroup + Show PA Values panel into PaWattMeterPage; Phase
// 3M-3 wires the per-band gain table + auto-cal sweep into PaGainByBandPage;
// Phase 4 binds PaValuesPage to RadioStatus signals.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Original implementation.  PA top-level category for
//                 Setup IA reshape Phase 2.  AI-assisted transformation
//                 via Anthropic Claude Code.
// =================================================================

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

#include "gui/SetupPage.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// PA > PA Gain
// Mirrors Thetis tpGainByBand (setup.designer.cs:47386-47525 [v2.10.3.13]).
// Phase 2 placeholder — Phase 3M-3 wires per-band gain table + auto-cal sweep.
// ---------------------------------------------------------------------------
class PaGainByBandPage : public SetupPage {
    Q_OBJECT
public:
    explicit PaGainByBandPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// PA > Watt Meter
// Mirrors Thetis tpWattMeter (setup.designer.cs:49304-49309 [v2.10.3.13]).
// Phase 2 placeholder — Phase 3 migrates the cal spinboxes + Show PA Values
// toggle here from the existing Hardware → Calibration tab.
// ---------------------------------------------------------------------------
class PaWattMeterPage : public SetupPage {
    Q_OBJECT
public:
    explicit PaWattMeterPage(RadioModel* model, QWidget* parent = nullptr);
};

// ---------------------------------------------------------------------------
// PA > PA Values  (NereusSDR-spin)
// No direct Thetis page-level equivalent — Thetis ships panelPAValues
// (setup.designer.cs:51155-51177 [v2.10.3.13]) embedded inside the Watt
// Meter tab.  NereusSDR promotes this readout to a dedicated page so the
// live telemetry can be enriched (raw + calibrated FWD/REV, SWR, PA current,
// PA voltage, PA temperature, ADC overload state, drive byte) without
// crowding the cal-point editor.  Phase 4 binds this page to RadioStatus.
// ---------------------------------------------------------------------------
class PaValuesPage : public SetupPage {
    Q_OBJECT
public:
    explicit PaValuesPage(RadioModel* model, QWidget* parent = nullptr);
};

} // namespace NereusSDR
