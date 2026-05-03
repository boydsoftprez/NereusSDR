// =================================================================
// src/models/RadioModel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/NetworkIO.cs (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/ChannelMaster/cmaster.c, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
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

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
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

/*  wdsp.cs

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013-2017 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/
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

//
// Upstream source 'Project Files/Source/Console/HPSDR/NetworkIO.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

/*  cmaster.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014-2019 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/

#include "RadioModel.h"
#include "BandDefaults.h"
#include "RxDspWorker.h"
// 3M-1a G.1: TX-side integration — MoxController + TxChannel view.
// TxMicRouter is already included via RadioModel.h (for std::unique_ptr destructor).
#include "core/MoxController.h"
#include "core/MicProfileManager.h"
#include "core/StepAttenuatorController.h"
#include "core/TwoToneController.h"
#include "core/accessories/N2adrPreset.h"
#include "core/TxChannel.h"
// 3M-1c TX pump architecture redesign — dedicated worker thread for
// TX DSP pump (replaces D.1/E.1/L.4 chain).
#include "core/TxWorkerThread.h"
// 3M-1b L.1: concrete mic-source strategy objects.
#include "core/audio/PcMicSource.h"
#include "core/audio/RadioMicSource.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/TxMicSource.h"
#include "core/RadioConnection.h"
#include "core/RadioConnectionTeardown.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/BoardCapabilities.h"
#include "core/HardwareProfile.h"
#include "core/ReceiverManager.h"
#include "core/AudioEngine.h"
#include "core/WdspEngine.h"
#include "core/RxChannel.h"
#include "core/AppSettings.h"
#include "core/SampleRateCatalog.h"
#include "core/LogCategories.h"
#include "core/NoiseFloorTracker.h"
#include "core/ModelPaths.h"
#include "core/wdsp_api.h"
#include "gui/SpectrumWidget.h"

#include <algorithm>
#include <cmath>

#include <QDateTime>
#include <QEventLoop>
#include <QMetaObject>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QVector>

namespace NereusSDR {

// ─── Phase 3P-H Task 4: per-board PA telemetry scaling ─────────────────────
//
// The C&C / High-Priority status parsers (P1RadioConnection,
// P2RadioConnection) emit raw 16-bit ADC counts.  Per-board conversion
// to physical units (watts, volts, amps) is encoded in console.cs and
// depends on HardwareSpecific.Model — translation belongs here, not in
// the wire-protocol parsers.
//
// All formulas verbatim from Thetis console.cs [@501e3f5].  Constants
// (bridge_volt, refvoltage, adc_cal_offset, volt_div, amp_voff, amp_sens)
// preserved exactly per CLAUDE.md "Constants and Magic Numbers" rule.
namespace {

// From Thetis console.cs:25008-25072 [@501e3f5] computeAlexFwdPower():
//   float volts = (adc - adc_cal_offset) / 4095.0f * refvoltage;
//   if (volts < 0) volts = 0;
//   float watts = Math.Pow(volts, 2) / bridge_volt;
//
// Upstream inline attribution preserved verbatim:
//   :25007  case HPSDRModel.ANAN_G1: //N1GP G1 added
//   :25038  case HPSDRModel.REDPITAYA: //DH1KLM
//
// Bridge constants per HPSDRModel:
//   ANAN100/100B/100D : bridge=0.095  refV=3.3  cal=6
//   ANAN200D          : bridge=0.108  refV=5.0  cal=4
//   ANAN7000D/G2/RP   : bridge=0.12   refV=5.0  cal=32
//   ORIONMKII/8000D   : bridge=0.08   refV=5.0  cal=18
//   default           : bridge=0.09   refV=3.3  cal=6
double scaleFwdPowerWatts(quint16 adcRaw, HPSDRModel model)
{
    double bridge_volt   = 0.09;
    double refvoltage    = 3.3;
    int    adc_cal_offset = 6;

    switch (model) {
    case HPSDRModel::ANAN100:
    case HPSDRModel::ANAN100B:
    case HPSDRModel::ANAN100D:
        bridge_volt = 0.095; refvoltage = 3.3; adc_cal_offset = 6;
        break;
    case HPSDRModel::ANAN200D:
        bridge_volt = 0.108; refvoltage = 5.0; adc_cal_offset = 4;
        break;
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANVELINAPRO3:
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:             // !K will need different scaling
    case HPSDRModel::REDPITAYA: //DH1KLM
        bridge_volt = 0.12;  refvoltage = 5.0; adc_cal_offset = 32;
        break;
    case HPSDRModel::ORIONMKII:
    case HPSDRModel::ANAN8000D:
        bridge_volt = 0.08;  refvoltage = 5.0; adc_cal_offset = 18;
        break;
    default:
        // From Thetis console.cs:25049-25053 [@501e3f5] — default case
        bridge_volt = 0.09; refvoltage = 3.3; adc_cal_offset = 6;
        break;
    }

    double adc = static_cast<double>(adcRaw);
    if (adc < 0) { adc = 0; }
    double volts = (adc - adc_cal_offset) / 4095.0 * refvoltage;
    if (volts < 0) { volts = 0; }
    double watts = (volts * volts) / bridge_volt;
    if (watts < 0) { watts = 0; }
    return watts;
}

// From Thetis console.cs:24928-24996 [@501e3f5] computeRefPower():
//   identical formula shape; bridge constants differ + 6m carve-out.
//   We omit the 6m branch here because tx_band routing isn't wired
//   into RadioModel yet — the off-band scaling is conservative
//   (slightly under-reads on 6m).  TODO when TX-band tracking lands.
//
// Upstream inline attribution preserved verbatim (console.cs:24965):
//   case HPSDRModel.REDPITAYA: //DH1KLM
double scaleRevPowerWatts(quint16 adcRaw, HPSDRModel model)
{
    double bridge_volt   = 0.09;
    double refvoltage    = 3.3;
    int    adc_cal_offset = 3;

    switch (model) {
    case HPSDRModel::ANAN100:
    case HPSDRModel::ANAN100B:
    case HPSDRModel::ANAN100D:
        bridge_volt = 0.095; refvoltage = 3.3; adc_cal_offset = 3;
        break;
    case HPSDRModel::ANAN200D:
        bridge_volt = 0.108; refvoltage = 5.0; adc_cal_offset = 2;
        break;
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANVELINAPRO3:
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:                 // will need to be edited for scaling
    case HPSDRModel::REDPITAYA: //DH1KLM
        bridge_volt = 0.15;  refvoltage = 5.0; adc_cal_offset = 28;
        break;
    case HPSDRModel::ORIONMKII:
    case HPSDRModel::ANAN8000D:
        bridge_volt = 0.08;  refvoltage = 5.0; adc_cal_offset = 16;
        break;
    default:
        bridge_volt = 0.09; refvoltage = 3.3; adc_cal_offset = 3;
        break;
    }

    double adc = static_cast<double>(adcRaw);
    if (adc < 0) { adc = 0; }
    double volts = (adc - adc_cal_offset) / 4095.0 * refvoltage;
    if (volts < 0) { volts = 0; }
    double watts = (volts * volts) / bridge_volt;
    if (watts < 0) { watts = 0; }
    return watts;
}

// From Thetis console.cs:24886-24892 [@501e3f5] convertToVolts():
//   float volt_div = (22.0f + 1.0f) / 1.1f;          // R1+R2 / R2
//   float volts    = (IOreading / 4095.0f) * 5.0f;
//   volts = volts * volt_div;
//
// Applies to ORIONMKII/ANAN8000D PA volts (user_adc0 / AIN3).
// Other boards either use computeHermesDCVoltage (supply_volts AIN6)
// or don't expose PA volts at all; we return 0 for those models.
double scalePaVolts(quint16 adcRaw, HPSDRModel model)
{
    switch (model) {
    case HPSDRModel::ORIONMKII:
    case HPSDRModel::ANAN8000D:
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:
    case HPSDRModel::ANVELINAPRO3: {
        const double volt_div = (22.0 + 1.0) / 1.1;  // 20.9091
        double volts = (static_cast<double>(adcRaw) / 4095.0) * 5.0;
        volts *= volt_div;
        return volts;
    }
    default:
        return 0.0;
    }
}

// From Thetis console.cs:24916-24926 [@501e3f5] convertToAmps():
//   float voff     = _amp_voff;        // default 360.0f
//   float sens     = _amp_sens;        // default 120.0f
//   float fwdvolts = (IOreading * 5000.0f) / 4095.0f;
//   if (fwdvolts < 0) fwdvolts = 0;
//   float amps = (fwdvolts - voff) / sens;
//   if (amps < 0) amps = 0;
//
// _amp_voff and _amp_sens are user-tunable in Thetis Setup → PA Calibration;
// NereusSDR will surface them through CalibrationController in a follow-up
// (Phase 3P-G already lays the groundwork).  Defaults match Thetis 360/120.
double scalePaAmps(quint16 adcRaw, HPSDRModel model)
{
    switch (model) {
    case HPSDRModel::ORIONMKII:
    case HPSDRModel::ANAN8000D:
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:
    case HPSDRModel::ANVELINAPRO3: {
        constexpr double kAmpVoff = 360.0;   // From Thetis console.cs:24893 [@501e3f5]
        constexpr double kAmpSens = 120.0;   // From Thetis console.cs:24894 [@501e3f5]
        double fwdvolts = (static_cast<double>(adcRaw) * 5000.0) / 4095.0;
        if (fwdvolts < 0) { fwdvolts = 0; }
        double amps = (fwdvolts - kAmpVoff) / kAmpSens;
        if (amps < 0) { amps = 0; }
        return amps;
    }
    default:
        return 0.0;
    }
}

// PA temperature: Thetis does not currently surface a per-board PA temp
// scale in console.cs (no convertToTemp helper exists in v2.10.3.13).
// HL2 reports temp via I2C (Phase 3P-E IoBoardHl2 mirror); ANAN family
// PA temperature reaches Thetis only through external CAT/AmpView.
// Returning 0.0 here is honest — RadioStatusPage will show a dash for
// boards without a real source.  TODO (deferred): wire HL2 I2C temp
// register into setPaTemperature() in Phase H Task 5.
double scalePaTemperatureCelsius(quint16 /*adcRaw*/, HPSDRModel /*model*/)
{
    // no-port-check: NereusSDR-original placeholder — see comment above.
    return 0.0;
}

} // anonymous namespace

RadioModel::RadioModel(QObject* parent)
    : QObject(parent)
    , m_discovery(new RadioDiscovery(this))
    , m_receiverManager(new ReceiverManager(this))
    , m_audioEngine(new AudioEngine(this))
    , m_wdspEngine(new WdspEngine(this))
{
    // Phase 3O: give AudioEngine a non-owning back-pointer to this model
    // so rxBlockReady() can look up per-slice mute / VAX state. Wired
    // immediately after construction; AudioEngine caches the pointer and
    // treats a null as a safe no-op (tests that build AudioEngine
    // standalone).
    m_audioEngine->setRadioModel(this);

    // Phase 3P-I-a T9 — AlexController → connection pump.
    // Any per-band edit (from Setup grid, RxApplet, or VFO Flag via T12)
    // reapplies to the wire when the changed band matches the current
    // VFO band. Connect once here because m_alexController outlives each
    // connection; the helper no-ops when m_connection is null. Closes
    // issue #98's protocol-layer gap.
    connect(&m_alexController, &AlexController::antennaChanged, this,
            [this](Band b) {
        // Persist on every controller mutation so the per-band
        // selection survives app restart. Without this, AlexController
        // state lived only in memory — caught during PR #N bench
        // testing when ANT2 on 20m didn't restore across a relaunch
        // (KG4VCF 2026-04-22). Coalesced via scheduleSettingsSave so
        // load-time's 14-per-band emit burst collapses to one write.
        m_alexControllerDirty = true;
        scheduleSettingsSave();
        if (b != m_lastBand) { return; }
        applyAlexAntennaForBand(b);
        // T13 — keep the slice's cached ANT labels in sync so UI
        // surfaces reading slice->rxAntenna() see the current-band value.
        if (m_activeSlice) {
            m_activeSlice->refreshAntennasFromAlex(m_alexController, b);
        }
    });
    // Also persist the two blockTxAnt* safety toggles; they can change
    // via the Antenna Control grid even when no band crossing occurs.
    connect(&m_alexController, &AlexController::blockTxChanged, this,
            [this]() {
        m_alexControllerDirty = true;
        scheduleSettingsSave();
    });

    // Phase 3P-I-b (T6): flag changes must re-fire composition for current band.
    // The isTx arg stays false in 3P-I-b — MOX trigger wiring lands in 3M-1.
    // Uses a local lambda so all six connects share one band-lookup path.
    auto reapply = [this]() {
        Band b = m_activeSlice
                   ? bandFromFrequency(m_activeSlice->frequency())
                   : m_lastBand;
        applyAlexAntennaForBand(b);
    };
    connect(&m_alexController, &AlexController::ext1OutOnTxChanged,
            this, [reapply](bool) { reapply(); });
    connect(&m_alexController, &AlexController::ext2OutOnTxChanged,
            this, [reapply](bool) { reapply(); });
    connect(&m_alexController, &AlexController::rxOutOnTxChanged,
            this, [reapply](bool) { reapply(); });
    connect(&m_alexController, &AlexController::rxOutOverrideChanged,
            this, [reapply](bool) { reapply(); });
    connect(&m_alexController, &AlexController::useTxAntForRxChanged,
            this, [reapply](bool) { reapply(); });
    connect(&m_alexController, &AlexController::xvtrActiveChanged,
            this, [reapply](bool) { reapply(); });

    // Connection starts null — created by connectToRadio() via factory.
    //
    // Phase 3G-9b: the smooth-defaults profile is reachable only via the
    // "Reset to Smooth Defaults" button on SpectrumDefaultsPage per user
    // decision 2026-04-15 (Default should stay the out-of-box default).
    // No first-launch auto-apply here. The `DisplayProfileApplied`
    // AppSettings key is reserved for PR3 (Clarity) to repurpose.

    // Load bundled band-plan overlays from Qt resources. AppSettings is a
    // singleton available before RadioModel is constructed, so this is safe
    // here. Phase 3G RX Epic sub-epic D.
    m_bandPlanManager.loadPlans();

    // ── Phase 3M-0 Task 17: safety controller wiring ─────────────────────────
    //
    // 1. Load persisted enable / limit states so user preferences from
    //    Tasks 9-13's setup pages take effect on the first launch, not
    //    only after re-toggling each control.
    {
        auto& s = AppSettings::instance();
        m_swrProt.setEnabled(
            s.value(QStringLiteral("SwrProtectionEnabled"), QStringLiteral("False"))
             .toString() == QStringLiteral("True"));
        m_swrProt.setLimit(
            s.value(QStringLiteral("SwrProtectionLimit"), QStringLiteral("2.0"))
             .toString().toFloat());
        m_swrProt.setWindBackEnabled(
            s.value(QStringLiteral("WindBackPowerSwr"), QStringLiteral("False"))
             .toString() == QStringLiteral("True"));
        m_swrProt.setDisableOnTune(
            s.value(QStringLiteral("SwrTuneProtectionEnabled"), QStringLiteral("False"))
             .toString() == QStringLiteral("True"));
        m_swrProt.setTunePowerSwrIgnore(
            s.value(QStringLiteral("TunePowerSwrIgnore"), QStringLiteral("35"))
             .toString().toFloat());

        m_txInhibit.setEnabled(
            s.value(QStringLiteral("TxInhibitMonitorEnabled"), QStringLiteral("False"))
             .toString() == QStringLiteral("True"));
        m_txInhibit.setReverseLogic(
            s.value(QStringLiteral("TxInhibitMonitorReversed"), QStringLiteral("False"))
             .toString() == QStringLiteral("True"));
    }

    // 2. PA telemetry → SwrProtectionController::ingest is wired from the
    //    per-sample paTelemetryUpdated handler (search this file for
    //    "paTelemetryUpdated"), NOT from RadioStatus::powerChanged.
    //    RadioStatus emits powerChanged twice per hardware sample — once
    //    after setForwardPower and once after setReflectedPower — which
    //    would double-count trips and the first call would mix new fwd
    //    with stale rev. (Codex P1 follow-up to PR #139.) Routing from
    //    paTelemetryUpdated guarantees one ingest per sample with
    //    consistent fwd/rev values.

    // 3. SwrProtectionController::highSwrChanged → SpectrumWidget overlay.
    //    m_spectrumWidget may be null at construction time (set later by
    //    MainWindow::setSpectrumWidget). Guard every access.
    connect(&m_swrProt, &safety::SwrProtectionController::highSwrChanged,
            this, [this](bool isHigh) {
        if (m_spectrumWidget) {
            m_spectrumWidget->setHighSwrOverlay(isHigh, m_swrProt.windBackLatched());
        }
    });
    connect(&m_swrProt, &safety::SwrProtectionController::windBackLatchedChanged,
            this, [this](bool latched) {
        if (m_spectrumWidget && m_swrProt.highSwr()) {
            m_spectrumWidget->setHighSwrOverlay(true, latched);
        }
    });

    // ── 3M-1a G.1: TX-side integration ──────────────────────────────────────
    // Master design §5.1.1; pre-code review §1.6 + §2.5.
    //
    // MoxController: main-thread owner (QTimers fire on the main event loop).
    // Qt parent = this so RadioModel destructor cleans it up automatically.
    // From Thetis console.cs:29311-29678 [v2.10.3.13] — chkMOX_CheckedChanged2.
    //
    // Inline attribution tags preserved verbatim from the cited range:
    //[2.10.1.0]MW0LGE changed  [original inline comment from console.cs:29355]
    //MW0LGE [2.9.0.7]  [original inline comment from console.cs:29400]
    //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps  [console.cs:29561]
    //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29567]
    //[2.10.3.6]MW0LGE att_fixes NOTE: this will eventually call Display.TXAttenuatorOffset with the value  [console.cs:29568]
    // Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes  [console.cs:29576]
    // Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE  [console.cs:29603]
    //comboRX2Preamp.Enabled = true; //[2.10.3.6]MW0LGE att_fixes  [console.cs:29647]
    //udRX2StepAttData.Enabled = true; //[2.10.3.6]MW0LGE att_fixes  [console.cs:29648]
    // Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes  [console.cs:29659]
    m_moxController = new MoxController(this);

    // MoxController::hardwareFlipped → RadioModel::onMoxHardwareFlipped (F.1).
    // Qt::QueuedConnection: both live on the main thread, but QueuedConnection
    // documents the cross-component intent and ensures the slot runs after the
    // emitting call stack unwinds, matching the pre-code review §1.6 pattern.
    // From Thetis console.cs:29567-29576 [v2.10.3.13] — HdwMOXChanged call.
    //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29567-29576]
    connect(m_moxController, &MoxController::hardwareFlipped,
            this, &RadioModel::onMoxHardwareFlipped,
            Qt::QueuedConnection);

    // MoxController::txReady → TxChannel::setRunning(true) and
    // MoxController::txaFlushed → TxChannel::setRunning(false) are wired in
    // connectToRadio() once m_txChannel is live (see the "MoxController →
    // TxChannel queued connects" block inside the WDSP-init lambda).  We
    // cannot wire them here at construction time because m_txChannel is
    // nullptr until createTxChannel(1) runs inside that lambda — Qt's
    // AutoConnection thread-routing depends on the receiver having a valid
    // thread affinity (TxWorkerThread after moveToThread).

    // ── H.1: VOX run gated by voice-family mode ───────────────────────────────
    // Ports CMSetTXAVoxRun logic (cmaster.cs:1039-1052 [v2.10.3.13]):
    //   bool run = Audio.VOXEnabled && (mode in voice family)
    //   cmaster.SetDEXPRunVox(id, run);
    //
    // Signal chain:
    //   TransmitModel::voxEnabledChanged → MoxController::setVoxEnabled
    //   SliceModel::dspModeChanged       → MoxController::onModeChanged
    //   MoxController::voxRunRequested   → TxChannel::setVoxRun
    //
    // MoxController acts as the gating layer; TxChannel::setVoxRun is a
    // thin WDSP wrapper (D.3). The MoxController has already been seeded
    // with m_currentMode=DSPMode::USB (matching SliceModel default) and
    // m_voxEnabled=false so no spurious emit occurs at startup.
    //
    // Note: SliceModel wiring uses m_activeSlice (the single TX slice in
    // 3M-1b). If m_activeSlice is null at construction time the connection
    // is deferred; 3M-1b always has exactly one slice added during
    // onConnected() before any user interaction can enable VOX.
    connect(&m_transmitModel, &TransmitModel::voxEnabledChanged,
            m_moxController,  &MoxController::setVoxEnabled);

    // Active-slice mode gate: wire slice(0) dspModeChanged → MoxController.
    // The actual `connect` happens in addSlice() (when the slice exists);
    // wiring it here at construction time silently no-ops because m_slices
    // is empty at this point — the first slice gets added later via
    // addSlice() in connectToRadio(). Codex caught this on PR #149.
    // In 3M-1b there is exactly one slice; wiring via slice(0) is correct.
    // 3F multi-pan will need to re-evaluate when the active TX slice can change.
    // TODO [3F]: rewire to activeSlice() when multi-panadapter TX switching lands.

    // MoxController::voxRunRequested → TxChannel::setVoxRun is wired in
    // connectToRadio() once m_txChannel is live — same reason as txReady /
    // txaFlushed above.  Receiver=m_txChannel + AutoConnection auto-routes
    // to QueuedConnection when m_txChannel lives on TxWorkerThread, so the
    // lambda body runs on the worker thread (same-thread WDSP setter call).

    // ── H.2: VOX threshold with mic-boost-aware scaling ───────────────────────
    // Ports CMSetTXAVoxThresh (cmaster.cs:1054-1059 [v2.10.3.13]):
    //   if (Audio.console.MicBoost) thresh *= (double)Audio.VOXGain;
    //   cmaster.SetDEXPAttackThreshold(id, thresh);
    // and the dB→linear conversion from setup.cs:18911 [v2.10.3.13]:
    //   Math.Pow(10.0, (double)udDEXPThreshold.Value / 20.0)
    //
    // Signal chain:
    //   TransmitModel::voxThresholdDbChanged → MoxController::setVoxThreshold
    //   TransmitModel::micBoostChanged       → MoxController::onMicBoostChanged
    //   TransmitModel::voxGainScalarChanged  → MoxController::setVoxGainScalar
    //   MoxController::voxThresholdRequested → TxChannel::setVoxAttackThreshold
    //
    // MoxController applies both the dB→linear conversion and the mic-boost
    // scaling in computeScaledThreshold(); TxChannel::setVoxAttackThreshold
    // is a thin WDSP wrapper (D.3, TxChannel.h:473).
    connect(&m_transmitModel, &TransmitModel::voxThresholdDbChanged,
            m_moxController,  &MoxController::setVoxThreshold);
    connect(&m_transmitModel, &TransmitModel::micBoostChanged,
            m_moxController,  &MoxController::onMicBoostChanged);
    connect(&m_transmitModel, &TransmitModel::voxGainScalarChanged,
            m_moxController,  &MoxController::setVoxGainScalar);

    // MoxController::voxThresholdRequested → TxChannel::setVoxAttackThreshold
    // is wired in connectToRadio() once m_txChannel is live — same reason as
    // txReady / txaFlushed above.

    // ── H.3: VOX hang-time + anti-VOX gain + anti-VOX source path ────────────
    // Ports:
    //   ms→seconds for SetDEXPHoldTime (setup.cs:18899 [v2.10.3.13]):
    //     cmaster.SetDEXPHoldTime(0, Value / 1000.0)
    //   dB→linear for SetAntiVOXGain (setup.cs:18989 [v2.10.3.13]):
    //     cmaster.SetAntiVOXGain(0, Math.Pow(10.0, dB / 20.0))
    //   CMSetAntiVoxSourceWhat useVAC=false (cmaster.cs:937-942 [v2.10.3.13]):
    //     all RX slots (RX1, RX1S, RX2) get source=1.
    //
    // Signal chain:
    //   TransmitModel::voxHangTimeMsChanged    → MoxController::setVoxHangTime
    //   TransmitModel::antiVoxGainDbChanged    → MoxController::setAntiVoxGain
    //   TransmitModel::antiVoxSourceVaxChanged → MoxController::setAntiVoxSourceVax
    //   MoxController::voxHangTimeRequested    → TxChannel::setVoxHangTime
    //   MoxController::antiVoxGainRequested    → TxChannel::setAntiVoxGain
    //   MoxController::antiVoxSourceWhatRequested → TxChannel::setAntiVoxRun
    //
    // MoxController handles ms→seconds and dB→linear conversions; TxChannel
    // wrappers (D.3) are thin WDSP delegates.
    connect(&m_transmitModel, &TransmitModel::voxHangTimeMsChanged,
            m_moxController,  &MoxController::setVoxHangTime);
    connect(&m_transmitModel, &TransmitModel::antiVoxGainDbChanged,
            m_moxController,  &MoxController::setAntiVoxGain);
    connect(&m_transmitModel, &TransmitModel::antiVoxSourceVaxChanged,
            m_moxController,  &MoxController::setAntiVoxSourceVax);

    // MoxController::voxHangTimeRequested / antiVoxGainRequested /
    // antiVoxSourceWhatRequested → TxChannel setters are wired in
    // connectToRadio() once m_txChannel is live — same reason as txReady /
    // txaFlushed above.

    // ── H.5: P1/P2 status-frame mic_ptt → MoxController PTT-source dispatch ──
    //
    // RadioConnection::micPttFromRadio(bool) is emitted unconditionally on every
    // status frame (P1 EP6, P2 High-Priority) with the instantaneous PTT state.
    // MoxController::onMicPttFromRadio is idempotent on repeated same-state calls.
    //
    // The actual connect() is deferred to wireConnectionSignals() where
    // m_connection is live.  This block documents the wiring intent so
    // the phase-H comment block is self-contained.
    //
    //   onCatPtt: 3K — full CAT integration (rigctld / serial / network)
    //   onVoxActive: 3M-3a or via TxChannel TX-meter polling (WDSP DEXP output)
    //   onSpacePtt: 3M-3a — UI keyboard handler (MainWindow keyPressEvent)
    //   onX2Ptt: 3M-3a or later — X2 status-frame parsing in RadioConnection

    // TxMicRouter: NullMicSource for 3M-1a (zero-padded silence stream).
    // The TUNE path (gen1 PostGen) overwrites the WDSP input buffer at TXA
    // stage 22, so silence from NullMicSource is functionally inert during
    // TUNE-only TX. Replaced with PcMicSource / RadioMicSource in 3M-1b.
    // Master design §5.2 (3M-1a NullMicSource stub).
    m_txMicRouter = std::make_unique<NullMicSource>();

    // ── 3M-1c Phase L.1: MicProfileManager ────────────────────────────────────
    //
    // Per-MAC bank holding the 23 mic / VOX / MON / two-tone live keys
    // (chunk F).  Constructed once at RadioModel-ctor time; setMacAddress +
    // load() are called per-connect inside connectToRadio().  Empty MAC is
    // a silent-no-op contract per MicProfileManager.h §"All ops require a
    // per-MAC scope".  Qt parent=this so the dtor frees it.
    //
    // The user-driven setActiveProfile path is in TxApplet (J.1) and
    // TxProfileSetupPage (J.3): both call `manager->setActiveProfile(name,
    // &m_transmitModel)` directly.  No additional connect is needed at this
    // layer — MicProfileManager mutates TransmitModel via the public setter
    // API, and TransmitModel's auto-persist already routes those changes to
    // AppSettings.  The activeProfileChanged signal is consumed by the UI
    // (TxApplet J.1 + TxProfileSetupPage J.3) for combo-selection mirror.
    m_micProfileMgr = new MicProfileManager(this);

    // ── 3M-1c Phase L.2: TwoToneController ────────────────────────────────────
    //
    // Activation orchestrator for the two-tone IMD test (chunk I).  Holds
    // non-owning pointers to TransmitModel, TxChannel, MoxController, and
    // SliceModel.  The construction-time deps that DON'T require a live
    // connection (TransmitModel, MoxController) are wired here; setTxChannel
    // is called inside the WDSP-init lambda once m_txChannel is live;
    // setSliceModel is called when the active slice exists.
    //
    // Direct WDSP TXPostGen setter wiring for the 5 live-tunable two-tone
    // properties (Freq1, Freq2, Level, Power, Freq2Delay) is deferred:
    // L.2 caveat §"Recommend: keep L.2 simple — connect the signals to
    // direct WDSP setters as shown above. Live-update during active test is
    // a Phase 3M-3+ polish concern. Document the deferral."  At test-start
    // time TwoToneController reads the latest TransmitModel values directly,
    // so the user's edits ARE picked up — they just don't fire mid-test.
    //
    // The 3 control-only properties (TwoToneInvert / TwoTonePulsed /
    // TwoToneDrivePowerOrigin) are read by the controller during setActive(true)
    // and don't have WDSP-setter equivalents — they branch the activation
    // flow itself.  No connects needed for those.
    m_twoToneController = new TwoToneController(this);
    m_twoToneController->setTransmitModel(&m_transmitModel);
    m_twoToneController->setMoxController(m_moxController);

    // 3M-1a (Codex review on PR #144): wire RF-Power-slider movements to
    // the radio's drive byte.  Without this, the slider updates UI/model
    // state but `CmdHighPriority` byte 345 stays stale — users move the
    // slider expecting TX power to change, the wire-byte doesn't, and
    // the radio keeps transmitting at the prior drive level.
    //
    // Gated on `!isTune()`: while TUN is engaged, the drive byte is owned
    // by `setTune()` (which pushes `tunePowerForBand(currentBand)` and
    // restores `m_savedPowerPct` on release — Thetis console.cs:30129-30132
    // [v2.10.3.13] PreviousPWR pattern).  Mid-TUN slider movements are
    // accepted into the model but not pushed to the wire, matching
    // Thetis behaviour.
    //
    // Has no observable effect on 3M-1a's TUNE-only path (the slider has
    // no role in TUN), but keeps the UI/wire in sync for any non-TUN TX
    // path.  3M-1b voice TX will exercise this connection in earnest.
    connect(&m_transmitModel, &TransmitModel::powerChanged, this,
            [this](int power) {
        if (m_transmitModel.isTune()) { return; }
        if (!m_connection)            { return; }
        // Scale percent (0-100) → wire byte (0-255), with SWR-protection
        // foldback applied per mi0bot NetworkIO.cs:209-211 [v2.10.3.14-beta1]:
        //   int i = (int)(255 * f * _swr_protect);   // f normalised 0..1,
        //                                            // _swr_protect ≤ 1.0
        //   SetOutputPowerFactor(i);
        // setTxDrive's contract is the wire byte (0-255); m_transmitModel
        // power is 0-100 percent.  Without this scaling, 50% power was
        // reaching the wire as drive_level=50 (~20 % of max) and bench
        // RF output measured 0 W.  Default swrProtectFactor=1.0 → identity
        // to the prior `(power * 255) / 100` formula; foldback wires up
        // when SwrProtectionController drives the factor (follow-up).
        const float f          = std::clamp(power / 100.0f, 0.0f, 1.0f);
        const float swrProtect =
            std::clamp(m_transmitModel.swrProtectFactor(), 0.0f, 1.0f);
        const int wireDrive = std::clamp(int(255.0f * f * swrProtect), 0, 255);
        auto* conn = m_connection;
        QMetaObject::invokeMethod(conn, [conn, wireDrive]() {
            conn->setTxDrive(wireDrive);
        });
    });
}

RadioModel::~RadioModel()
{
    teardownConnection();
    qDeleteAll(m_slices);
    qDeleteAll(m_panadapters);
}

bool RadioModel::isConnected() const
{
    return m_connection && m_connection->isConnected();
}

const BoardCapabilities& RadioModel::boardCapabilities() const
{
#ifdef NEREUS_BUILD_TESTS
    if (m_testCapsOverride) {
        static BoardCapabilities overrideCaps{};
        overrideCaps.hasAlex     = m_testCapsHasAlex;
        overrideCaps.isRxOnlySku = m_testCapsIsRxOnly;  // 3M-1a G.2
        overrideCaps.hasMicJack  = m_testCapsHasMicJack; // 3M-1b I.1
        overrideCaps.board       = m_testCapsHw;          // 3M-1b I.3
        // 3M-1b I.4: propagate per-board mic gain range from the canonical
        // caps table for the injected board type.  This lets mic-gain range
        // tests observe the correct per-family values without a live radio.
        const BoardCapabilities& canonical = BoardCapsTable::forBoard(m_testCapsHw);
        overrideCaps.micGainMinDb = canonical.micGainMinDb;
        overrideCaps.micGainMaxDb = canonical.micGainMaxDb;
        return overrideCaps;
    }
#endif
    if (m_hardwareProfile.caps) { return *m_hardwareProfile.caps; }
    return BoardCapsTable::forBoard(HPSDRHW::Unknown);
}

// --- Slice Management ---

SliceModel* RadioModel::sliceAt(int index) const
{
    if (index >= 0 && index < m_slices.size()) {
        return m_slices.at(index);
    }
    return nullptr;
}

int RadioModel::addSlice()
{
    auto* slice = new SliceModel(this);
    int index = m_slices.size();
    slice->setSliceIndex(index);
    m_slices.append(slice);

    // 3M-1b H.1: wire VOX mode-gate from THIS slice's dspModeChanged →
    // MoxController. The construction-time wire-up at line ~677 silently
    // no-ops because m_slices is empty at that point; the first slice is
    // added here. Codex P1 fix on PR #149.
    if (m_moxController) {
        connect(slice, &SliceModel::dspModeChanged,
                m_moxController, &MoxController::onModeChanged);
    }

    if (!m_activeSlice) {
        m_activeSlice = slice;
        // Mark the first slice as active so isActiveSlice() returns true for it.
        // AudioEngine::rxBlockReady (3M-1b E.4) reads this flag to gate the
        // per-slice RX-audio push during MOX.
        slice->setActive(true);
        emit activeSliceChanged(0);
    }

    emit sliceAdded(index);
    return index;
}

void RadioModel::removeSlice(int index)
{
    if (index < 0 || index >= m_slices.size()) {
        return;
    }

    SliceModel* slice = m_slices.takeAt(index);
    if (m_activeSlice == slice) {
        // Clear the active flag before reassigning. The deleted slice's flag
        // is moot, but the new active slice needs to be marked.
        slice->setActive(false);
        m_activeSlice = m_slices.isEmpty() ? nullptr : m_slices.first();
        if (m_activeSlice) {
            m_activeSlice->setActive(true);
        }
        emit activeSliceChanged(m_activeSlice ? 0 : -1);
    }

    delete slice;
    emit sliceRemoved(index);
}

void RadioModel::setActiveSlice(int index)
{
    if (index >= 0 && index < m_slices.size()) {
        SliceModel* newActive = m_slices.at(index);
        if (m_activeSlice == newActive) {
            return;  // no change
        }
        // Clear the previous active slice flag so isActiveSlice() reflects
        // the correct single active slice. AudioEngine::rxBlockReady (3M-1b
        // E.4) reads this flag to gate the RX-audio push during MOX.
        if (m_activeSlice) {
            m_activeSlice->setActive(false);
        }
        m_activeSlice = newActive;
        m_activeSlice->setActive(true);
        emit activeSliceChanged(index);
    }
}

void RadioModel::onBandButtonClicked(Band band)
{
    SliceModel* slice = activeSlice();
    if (!slice) {
        // No active slice (pre-connection, between-slice teardown, etc.).
        // Silent — avoids log spam from UI events firing during startup.
        return;
    }

    // Use slice frequency (not PanadapterModel::band()) so that in CTUN
    // mode with an off-center panadapter, the "current band" follows the
    // VFO's actual band, not the DDC tuner's.
    const Band current = bandFromFrequency(slice->frequency());
    if (band == current) {
        // Same-band click — design decision Q1(a). Keeps UX predictable;
        // avoids yanking the VFO when the user is already in the band.
        // Silent (not emitted as "ignored") because this is expected
        // behavior, not a failed command.
        return;
    }

    if (slice->locked()) {
        // Lock → full short-circuit. Earlier design had mode still changing
        // (Thetis "lock is VFO-only" semantic), but our per-band persistence
        // model corrupted the new band's slot on a locked click: the
        // blocked setFrequency left stale freq in memory, then the tail
        // saveToSettings(newBand) baked that stale freq into the new
        // band's slot. Full short-circuit is simpler and matches the
        // common user mental model of "lock = slice is inert".
        const QString reason = QStringLiteral("Band %1 ignored: slice is locked — unlock to change bands")
                                   .arg(bandLabel(band));
        qCDebug(lcConnection) << reason;
        emit bandClickIgnored(band, reason);
        return;
    }

    // Snapshot outgoing band's full per-band DSP + session state (freq,
    // mode, filter, AGC tuple, NB, step, antennas, etc.) before we
    // overwrite the slice. See SliceModel::saveToSettings for the exact
    // key set persisted.
    slice->saveToSettings(current);

    if (slice->hasSettingsFor(band)) {
        // Second+ visit: restore last-used state for the clicked band.
        slice->restoreFromSettings(band);
        return;
    }

    // First visit: apply the seed if one exists, otherwise no-op with
    // user-visible feedback.
    BandSeed seed = BandDefaults::seedFor(band);
    if (!seed.valid) {
        // XVTR today. Becomes meaningful once the XVTR epic ships.
        const QString reason = QStringLiteral("Band %1 ignored: transverter config not yet supported")
                                   .arg(bandLabel(band));
        qCDebug(lcConnection) << reason;
        emit bandClickIgnored(band, reason);
        return;
    }

    // Order: freq before mode. NereusSDR-specific — frequencyChanged
    // triggers the per-band Alex/antenna update before mode-dependent
    // filter bandwidth applies. Note Thetis SetBand applies mode first
    // then freq (console.cs:5886/5911 [v2.10.3.13]); both orderings
    // produce the same end state, but the freq-first order exposes the
    // Alex switch earlier in the signal chain.
    slice->setFrequency(seed.frequencyHz);
    slice->setDspMode(seed.mode);
    slice->saveToSettings(band);   // Bake seed for next visit.
}

// --- Panadapter Management ---

int RadioModel::addPanadapter()
{
    auto* pan = new PanadapterModel(this);
    int index = m_panadapters.size();
    m_panadapters.append(pan);

    // PanadapterModel::bandChanged fires when the pan center crosses a band
    // boundary. In NereusSDR's design m_lastBand tracks the VFO, not the pan
    // (see comment on the frequencyChanged lambda in wireSliceSignals), so
    // there is nothing to do here on a pan-centered crossing — per-band saves
    // flow from the VFO path and the coalesced scheduleSettingsSave() timer.
    // Intentionally left as a no-op hook so the connection survives future
    // per-pan band-aware behavior without re-adding the recursion/corruption
    // path that existed in v0.2.0.
    connect(pan, &PanadapterModel::bandChanged, this, [](Band /*newBand*/) {});

    emit panadapterAdded(index);
    return index;
}

void RadioModel::removePanadapter(int index)
{
    if (index < 0 || index >= m_panadapters.size()) {
        return;
    }

    delete m_panadapters.takeAt(index);
    emit panadapterRemoved(index);
}

// --- Connection ---

void RadioModel::connectToRadio(const RadioInfo& info)
{
    // Tear down any existing connection
    if (m_connection) {
        teardownConnection();
    }

    m_lastRadioInfo = info;
    m_intentionalDisconnect = false;

    // Compute HardwareProfile from model override (Phase 3I-RP).
    HPSDRModel selectedModel = info.modelOverride;
    if (selectedModel == HPSDRModel::FIRST) {
        selectedModel = defaultModelForBoard(info.boardType);
    }
    m_hardwareProfile = profileForModel(selectedModel);

    qCDebug(lcConnection) << "HardwareProfile: model=" << displayName(m_hardwareProfile.model)
                          << "effectiveBoard=" << static_cast<int>(m_hardwareProfile.effectiveBoard)
                          << "adcCount=" << m_hardwareProfile.adcCount;

    // hermes-filter-debug Bug 1: push the connected board's attenuator range
    // into StepAttenuatorController so consumers (RxApplet S-ATT spinbox,
    // GeneralOptionsPage spinboxes) read board-correct min/max.  Default
    // controller bounds are 0..31; HL2 needs the signed -28..+32 range
    // (mi0bot setup.cs:16085-16086 [v2.10.3.13-beta2]).  Without this sync,
    // the spinbox UI clamps any negative dB the user types back to 0 even
    // though BoardCapabilities advertises the wider range.
    if (m_stepAttController) {
        const auto& atten = boardCapabilities().attenuator;
        m_stepAttController->setMinAttenuation(atten.minDb);
        m_stepAttController->setMaxAttenuation(atten.maxDb);
    }

    // Load per-MAC OC matrix state so the codec layer (P1/P2 buildCodecContext)
    // reads the correct per-band OC byte from the first C&C frame onwards.
    // Phase 3P-D Task 3.
    if (!info.macAddress.isEmpty()) {
        m_ocMatrix.setMacAddress(info.macAddress);
        m_ocMatrix.load();

        // Reconcile the OcMatrix to the persisted N2ADR Filter setting at
        // app launch.  Without this, a matrix populated by a prior session's
        // N2ADR-on toggle survives indefinitely even after the user disables
        // N2ADR — including across app restarts.  The Hl2IoBoardTab's
        // restoreSettings() does the same thing but only fires when the user
        // opens Setup; this hook ensures the matrix is always in sync from
        // the very first composeCcForBank call.
        //
        // Per-band write table lives in N2adrPreset so the toggle handler
        // and this reconcile share one source of truth.  Phase 3L also
        // added 13 SWL pin-7 RX entries via that helper — without them
        // the OcOutputsSwlTab would always render blank even after the
        // user enabled N2ADR.
        // hermes-filter-debug Bug 2: read PER-MAC, not global.  The legacy
        // global "hl2IoBoard/n2adrFilter" key has already been migrated into
        // hardware/<mac>/hl2IoBoard/n2adrFilter at app start by
        // AppSettings::migrateLegacyN2adrFilter (see main.cpp).  This read
        // matches the write side (Hl2IoBoardTab::onN2adrToggled →
        // HardwarePage::wire() → setHardwareValue).
        const QString n2adrKey = QStringLiteral("hl2IoBoard/n2adrFilter");
        const bool n2adrOn = AppSettings::instance()
                                 .hardwareValue(info.macAddress, n2adrKey,
                                                QStringLiteral("False"))
                                 .toString() == QStringLiteral("True");
        applyN2adrPreset(m_ocMatrix, n2adrOn);
        m_ocMatrix.save();

        // Load per-MAC Alex antenna controller state so Antenna Control UI
        // and future protocol codecs read the correct per-band antenna assignments.
        // Phase 3P-F Task 3. Pattern mirrors OcMatrix above.
        m_alexController.setMacAddress(info.macAddress);
        m_alexController.load();

        // Load per-MAC Apollo accessory state (present/filter/tuner bools).
        // Phase 3P-F Task 5a.
        m_apolloController.setMacAddress(info.macAddress);
        m_apolloController.load();

        // Load per-MAC PennyLane ext-ctrl master toggle.
        // Phase 3P-F Task 5b.
        m_pennyLaneController.setMacAddress(info.macAddress);
        m_pennyLaneController.load();

        // Load per-MAC HL2 Options (9 mi0bot tpHL2Options knobs).
        // Phase 3L commit #9.  Wire-format emission deferred to follow-up PR.
        m_hl2Options.setMacAddress(info.macAddress);
        m_hl2Options.load();

        // Load per-MAC calibration state (freq correction factor, level offsets, etc.).
        // Phase 3P-G. Pushed to P2RadioConnection via setCalibrationController() below.
        m_calController.setMacAddress(info.macAddress);
        m_calController.load();

        // P1 full-parity §3.2: seed PA forward-power cal profile from the
        // hardware model on first connect to this MAC. `load()` above leaves
        // `paCalProfile().boardClass == None` if no `paCalibration/boardClass`
        // key was persisted; in that case we install the factory `defaults()`
        // for the current board class. Reconnects with persisted state leave
        // the user-edited table intact.
        // Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13]
        if (m_calController.paCalProfile().boardClass == PaCalBoardClass::None) {
            m_calController.setPaCalProfile(
                PaCalProfile::defaults(paCalBoardClassFor(m_hardwareProfile.model)));
        }

        // Load per-MAC per-band tune power (50W default per band on first init).
        // Phase 3M-1a G.3. Source: Thetis console.cs:1819-1820 / :4904-4910 [v2.10.3.13].
        m_transmitModel.setMacAddress(info.macAddress);
        m_transmitModel.load();

        // Load per-MAC mic/VOX/MON properties (15 properties, 3 excluded for safety).
        // Phase 3M-1b L.2. After setMacAddress so auto-persist uses the correct MAC.
        // voxEnabled, monEnabled, micMute are NOT loaded — always start at safe defaults.
        m_transmitModel.loadFromSettings(info.macAddress);

        // ── 3M-1c L.1: per-MAC MicProfileManager scope ────────────────────────
        //
        // Set the MAC scope first, then load() seeds the "Default" profile on
        // first launch (per F.5).  Idempotent on subsequent loads under the
        // same MAC.  Constructed once at RadioModel ctor time (above);
        // setMacAddress("")  is called in teardownConnection.
        if (m_micProfileMgr) {
            m_micProfileMgr->setMacAddress(info.macAddress);
            m_micProfileMgr->load();
        }

        // L.3: HL2 force-Pc on connect.
        // HL2 has no radio-side mic jack (BoardCapabilities::hasMicJack == false).
        // Even if AppSettings persisted MicSource::Radio from a different radio
        // connected under the same MAC (extremely unlikely but possible),
        // override to Pc to keep mic-source state aligned with hardware reality.
        // The UI side (AudioTxInputPage) already disables the Radio Mic radio
        // button when !hasMicJack; this completes the model-side lock.
        // setMicSourceLocked also coerces any existing Radio state to Pc immediately.
        m_transmitModel.setMicSourceLocked(!boardCapabilities().hasMicJack);
    }

    m_name = info.displayName();
    m_model = QString::fromLatin1(m_hardwareProfile.caps->displayName);
    m_version = QString::number(info.firmwareVersion);
    emit infoChanged();

    // Configure ReceiverManager with hardware capabilities
    m_receiverManager->setMaxReceivers(info.maxReceivers);

    // Create receiver 0 with protocol-appropriate DDC mapping.
    // P2 2-ADC boards (ANAN-G2/Saturn) use DDC2 as primary RX per
    // Thetis console.cs:8216 UpdateDDCs. P1 radios deliver samples on
    // hardware receiver index 0, so leave the mapping auto-assigned
    // (which rebuildHardwareMapping resolves to 0 for the first active
    // receiver). Hardcoding DDC2 for everything dropped every P1 ep6
    // packet at ReceiverManager::feedIqData on tester hardware.
    int rxIdx = m_receiverManager->createReceiver();
    if (info.protocol == ProtocolVersion::Protocol2) {
        m_receiverManager->setDdcMapping(rxIdx, 2);   // DDC2 for 2-ADC P2 boards
    }
    m_receiverManager->setAdcForReceiver(rxIdx, 0); // ADC0

    // Create slice 0 and load persisted VFO state from AppSettings
    if (m_slices.isEmpty()) {
        addSlice();
    }
    setActiveSlice(0);
    m_activeSlice->setReceiverIndex(rxIdx);
    loadSliceState(m_activeSlice);

    // ── 3M-1c L.2: TwoToneController active-slice mode source ────────────────
    //
    // The controller reads SliceModel::dspMode() during setActive(true) for
    // the LSB-family invert-tones branch (TwoToneController.cpp step 4 /
    // setup.cs:11058-11062 [v2.10.3.13]).  Wire it to the freshly-added
    // active slice; if active slice changes later (3F multi-pan), the
    // setActiveSlice path will need to refresh this pointer too.
    if (m_twoToneController) {
        m_twoToneController->setSliceModel(m_activeSlice);
    }

    // Activate receiver (this sends hardwareReceiverCountChanged to RadioConnection)
    m_receiverManager->activateReceiver(rxIdx);

    // Initialize WDSP DSP engine (wisdom runs async — channel creation
    // is deferred until initializedChanged fires)
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    // Sample rate + active RX count come from Hardware Config (per-MAC).
    // Falls back to Thetis default (192000, setup.cs:866) when nothing
    // is persisted, and to the board-cap first-entry if 192000 isn't
    // in the allowed list. wdspInSize follows the Thetis formula
    // 64 * rate / 48000 from ChannelMaster/cmsetup.c:104-111.
    const auto& caps = *m_hardwareProfile.caps;
    const HPSDRModel model = m_hardwareProfile.model;
    const int wdspInputRate = resolveSampleRate(
        AppSettings::instance(), info.macAddress, info.protocol, caps, model);
    const int wdspInSize = bufferSizeForRate(wdspInputRate);
    const int activeRxCount = resolveActiveRxCount(
        AppSettings::instance(), info.macAddress, caps);
    qCInfo(lcConnection) << "Connecting with sampleRate=" << wdspInputRate
                         << "inSize=" << wdspInSize
                         << "activeRxCount=" << activeRxCount;

    // 3M-1a G.1 fixup: explicit disconnect in teardownConnection() prevents
    // accumulation across reconnect cycles.  Qt::UniqueConnection can't be
    // used with lambdas, so we rely on the matching disconnect there.
    // Without that disconnect, every connectToRadio() would add another copy
    // of this lambda; on the second connect, both copies would call
    // createRxChannel + createTxChannel(1) (idempotent today, but doubled work).
    connect(m_wdspEngine, &WdspEngine::initializedChanged, this,
            [this, wdspInputRate, wdspInSize](bool ok) {
        if (!ok) {
            return;
        }
        // Create primary RX channel once WDSP is ready. in_size follows
        // Thetis cmaster.c create_rcvr: 64 * input_rate / 48000. WDSP
        // decimates input_rate -> 48000 internally and outputs 64 samples
        // per fexchange2 call.
        RxChannel* rxCh = m_wdspEngine->createRxChannel(0, wdspInSize, 4096,
                                                         wdspInputRate, 48000, 48000);

        // 3M-1a G.1: create the WDSP TX channel (channel ID = 1 = WDSP.id(1, 0)).
        // Parameters match Thetis cmaster.c:177-190 [v2.10.3.13] — create_xmtr().
        // WdspEngine owns the channel via m_txChannels; we take a non-owning view.
        // The channel starts stopped (setRunning(false) is the default); txReady
        // fires setRunning(true) after MOX engage + rfDelay.
        // From Thetis dsp.cs:926-944 [v2.10.3.13] — WDSP.id(1, 0) = channel 1.
        //
        // 3M-1a bench fix: the TX channel was previously created here, but
        // this lambda fires synchronously inside m_wdspEngine->initialize()
        // BEFORE m_connection = conn.release() runs lower down in
        // connectToRadio().  That left m_txChannel with a null connection
        // pointer AND a wrong outputSampleRate (m_connection->txSampleRate()
        // returned the default 48 kHz instead of the radio's 192 kHz).
        //
        // Both prerequisites (WDSP initialised + m_connection live) are
        // guaranteed AFTER conn.release() completes, so TX channel creation
        // moved there.  See the "TX channel creation deferred" block right
        // after m_connection = conn.release().
        if (rxCh) {
            // Apply slice state to WDSP channel (no longer hardcoded)
            if (m_activeSlice) {
                rxCh->setMode(m_activeSlice->dspMode());
                rxCh->setFilterFreqs(m_activeSlice->filterLow(),
                                     m_activeSlice->filterHigh());
                rxCh->setAgcMode(m_activeSlice->agcMode());
                rxCh->setAgcTop(m_activeSlice->rfGain());
                // AGC advanced — push slice state to WDSP (Stage 2)
                rxCh->setAgcThreshold(m_activeSlice->agcThreshold());
                rxCh->setAgcHang(m_activeSlice->agcHang());
                rxCh->setAgcSlope(m_activeSlice->agcSlope());
                rxCh->setAgcAttack(m_activeSlice->agcAttack());
                rxCh->setAgcDecay(m_activeSlice->agcDecay());
                rxCh->setAgcHangThreshold(m_activeSlice->agcHangThreshold());
                rxCh->setAgcFixedGain(m_activeSlice->agcFixedGain());
                rxCh->setAgcMaxGain(m_activeSlice->agcMaxGain());
                // NB mode is per-band; tuning is global per-channel and
                // lives inside NbFamily (seeded from AppSettings at ctor,
                // live-pushed from Setup → DSP → NB/SNB). Per-slice NB
                // tuning pass-through removed 2026-04-22.
                rxCh->setNbMode(m_activeSlice->nbMode());

                // Sub-epic C-1 Task 19: push full NR config to the active slice's
                // RxChannel on radio connect.
                // Thetis console.cs:43297 SelectNR pattern [v2.10.3.13] — push
                // tuning structs first, then the active slot last so WDSP has
                // valid parameters before the run-flag is set.
                {
                    RxChannel::Nr1Tuning n1;
                    n1.taps     = m_activeSlice->nr1Taps();
                    n1.delay    = m_activeSlice->nr1Delay();
                    n1.gain     = m_activeSlice->nr1Gain();
                    n1.leakage  = m_activeSlice->nr1Leakage();
                    n1.position = m_activeSlice->nr1Position();
                    rxCh->setAnrTuning(n1);

                    RxChannel::Nr2Tuning n2;
                    n2.gainMethod  = m_activeSlice->nr2GainMethod();
                    n2.npeMethod   = m_activeSlice->nr2NpeMethod();
                    // trainT1/trainT2 are not in Nr2Tuning struct — applied via
                    // per-knob setters below (they call SetRXAEMNRtrainZetaThresh/
                    // SetRXAEMNRtrainT2 which have no struct-level path).
                    n2.aeFilter    = m_activeSlice->nr2AeFilter();
                    n2.position    = m_activeSlice->nr2Position();
                    n2.post2Run    = m_activeSlice->nr2Post2Run();
                    n2.post2Level  = m_activeSlice->nr2Post2Level();
                    n2.post2Factor = m_activeSlice->nr2Post2Factor();
                    n2.post2Rate   = m_activeSlice->nr2Post2Rate();
                    n2.post2Taper  = m_activeSlice->nr2Post2Taper();
                    rxCh->setEmnrTuning(n2);
                    // Push trainT1/trainT2 separately (not in Nr2Tuning struct)
                    rxCh->setEmnrTrainT1(m_activeSlice->nr2TrainT1());
                    rxCh->setEmnrTrainT2(m_activeSlice->nr2TrainT2());

                    RxChannel::Nr3Tuning n3;
                    n3.position       = m_activeSlice->nr3Position();
                    n3.useDefaultGain = m_activeSlice->nr3UseDefaultGain();
                    rxCh->setRnnrTuning(n3);

                    RxChannel::Nr4Tuning n4;
                    n4.reductionAmount     = m_activeSlice->nr4Reduction();
                    n4.smoothingFactor     = m_activeSlice->nr4Smoothing();
                    n4.whiteningFactor     = m_activeSlice->nr4Whitening();
                    n4.noiseRescale        = m_activeSlice->nr4Rescale();
                    n4.postFilterThreshold = m_activeSlice->nr4PostThresh();
                    n4.algo                = m_activeSlice->nr4Algo();
                    rxCh->setSbnrTuning(n4);

#ifdef HAVE_DFNR
                    rxCh->setDfnrAttenLimit(static_cast<float>(m_activeSlice->dfnrAttenLimit()));
                    rxCh->setDfnrPostFilterBeta(static_cast<float>(m_activeSlice->dfnrPostFilterBeta()));
#endif
#ifdef HAVE_MNR
                    // SliceModel already stores mnrStrength as 0.0–1.0
                    // (matches MacNRFilter::setStrength expected range).
                    // The Setup/popup slider does the ×100 / ÷100 UI↔model
                    // conversion; the model→filter path is 1:1.
                    rxCh->setMnrStrength(static_cast<float>(m_activeSlice->mnrStrength()));
                    rxCh->setMnrOversub(static_cast<float>(m_activeSlice->mnrOversub()));
                    rxCh->setMnrFloor(static_cast<float>(m_activeSlice->mnrFloor()));
                    rxCh->setMnrAlpha(static_cast<float>(m_activeSlice->mnrAlpha()));
                    rxCh->setMnrBias(static_cast<float>(m_activeSlice->mnrBias()));
                    rxCh->setMnrGsmooth(static_cast<float>(m_activeSlice->mnrGsmooth()));
#endif

                    // NR3 model — global (RNNRloadModel), not per-channel.
                    // Prefer AppSettings override; fall back to the bundled dev-path.
                    // From Thetis wdsp/rnnr.c:161-176 [v2.10.3.13]
                    {
                        const QString defaultModelPath = NereusSDR::ModelPaths::rnnoiseDefaultLargeBin();
                        const QString model = AppSettings::instance().value(
                            QStringLiteral("Nr3ModelPath"), defaultModelPath).toString();
                        if (!model.isEmpty()) {
                            qCInfo(lcDsp) << "NR3: loading rnnoise model from" << model;
#ifdef HAVE_WDSP
                            RNNRloadModel(model.toStdString().c_str());
#endif
                        } else {
                            qCWarning(lcDsp) << "NR3 model not found at expected paths;"
                                             << "NR3 will be disabled until a model is loaded.";
                        }
                    }

                    // Push the active NR slot last — parameters must be set before
                    // run-flag so WDSP gets valid defaults on first enable.
                    // From Thetis console.cs:43297 SelectNR pattern [v2.10.3.13]
                    rxCh->setActiveNr(m_activeSlice->activeNr());
                }

                rxCh->setSnbEnabled(m_activeSlice->snbEnabled());
                // APF sub-parameter defaults — From Thetis radio.cs:1986,1948,1967,1929
                // These are set-and-forget on channel creation; run flag follows slice.
                // selection=3 (bi-quad), bw=600Hz, gain=1.0, freq=600.0Hz
                rxCh->setApfSelection(3);       // radio.cs:1986 _rx_apf_type = 3 (bi-quad)
                rxCh->setApfBandwidth(600.0);   // radio.cs:1948 rx_apf_bw = 600.0 Hz
                rxCh->setApfGain(1.0);          // radio.cs:1967 rx_apf_gain = 1.0
                rxCh->setApfFreq(600.0);        // radio.cs:1929 rx_apf_freq = 600.0 Hz
                rxCh->setApfEnabled(m_activeSlice->apfEnabled());
                // Squelch initial push — From Thetis radio.cs:1185,1164,1274,1293,1312
                rxCh->setSsqlEnabled(m_activeSlice->ssqlEnabled());
                // Model stores 0–100 (slider units); WDSP expects 0.0–1.0 linear.
                rxCh->setSsqlThresh(std::clamp(m_activeSlice->ssqlThresh() / 100.0, 0.0, 1.0));
                rxCh->setAmsqEnabled(m_activeSlice->amsqEnabled());
                rxCh->setAmsqThresh(m_activeSlice->amsqThresh());
                rxCh->setFmsqEnabled(m_activeSlice->fmsqEnabled());
                rxCh->setFmsqThresh(m_activeSlice->fmsqThresh());
                // Audio panel initial push
                // Mute: From Thetis dsp.cs:393-394 — panel runs by default (unmuted)
                // Pan: From Thetis radio.cs:1386 — pan = 0.5f (center); NereusSDR 0.0 center
                // Binaural: From Thetis radio.cs:1145 — bin_on = false (dual-mono)
                rxCh->setMuted(m_activeSlice->muted());
                rxCh->setAudioPan(m_activeSlice->audioPan());
                rxCh->setBinauralEnabled(m_activeSlice->binauralEnabled());
            }
            rxCh->setActive(true);
        }
        // Apply volume from slice
        if (m_activeSlice) {
            m_audioEngine->setVolume(m_activeSlice->afGain() / 100.0f);
        }
        // Start audio output
        m_audioEngine->start();
        qCInfo(lcDsp) << "WDSP ready — RX channel 0 active, audio started";
    }, Qt::SingleShotConnection);
    m_wdspEngine->initialize(configDir);

    // WDSP wisdom now ALWAYS runs on a worker thread (WdspEngine::initialize
    // dropped its sync fast path so the user gets a progress dialog whenever
    // FFTW regenerates plans).  But the rest of this function — TX channel
    // creation at line ~1452, the SingleShot lambda above that creates the
    // RX channel, etc. — was written against the old sync contract where
    // m_initialized was true by the time initialize() returned.
    //
    // Block here while the wisdom worker finishes, pumping the Qt event loop
    // so the MainWindow wisdom progress dialog (connected to wisdomProgress)
    // renders and updates.  The dialog is Qt::ApplicationModal (see
    // MainWindow.cpp:600) so other windows are blocked from interaction
    // during the wait — no re-entrant Connect-clicks or similar.
    //
    // Order: register the listener BEFORE checking isInitialized().  Qt's
    // current threading semantics make the check-then-connect race
    // theoretically impossible (no event pump between the read and the
    // connect), but the canonical wait-for-signal idiom is connect → check
    // → exec — robust against future Qt internals changes and trivially
    // race-proof regardless of when the worker's QThread::finished posts.
    QEventLoop wisdomLoop;
    QMetaObject::Connection waitConn = QObject::connect(
        m_wdspEngine, &WdspEngine::initializedChanged,
        &wisdomLoop, [&wisdomLoop](bool ok) {
            if (ok) wisdomLoop.quit();
        });
    if (!m_wdspEngine->isInitialized()) {
        wisdomLoop.exec();
    }
    QObject::disconnect(waitConn);

    // Factory-create the connection (no parent — will be moved to thread)
    auto conn = RadioConnection::create(info);
    if (!conn) {
        qCWarning(lcConnection) << "Failed to create connection for" << info.displayName();
        return;
    }
    m_connection = conn.release();
    m_connection->setHardwareProfile(m_hardwareProfile);

    // 3M-1a bench fix: TX channel creation was previously inside the WDSP-
    // init lambda, which fires synchronously inside m_wdspEngine->initialize()
    // (above, line ~1152) — BEFORE m_connection was assigned.  Result: the
    // channel was opened with the wrong outputSampleRate (default 48 kHz
    // instead of radio's 192 kHz for P2/G2) AND TxChannel.m_connection was
    // null.  Both prerequisites (WDSP init + live m_connection) are
    // guaranteed at this point, so the TX channel is created here.
    //
    // From Thetis wdsp/cmaster.c:177-190 [v2.10.3.13] — create_xmtr() params.
    // From Thetis dsp.cs:926-944 [v2.10.3.13] — WDSP.id(1, 0) = channel 1.
    // From Thetis netInterface.c:1513 [v2.10.3.13] — P2 TX always 192 kHz.
    if (m_wdspEngine && !m_txChannel) {
        const int txOutRate = m_connection->txSampleRate();
        // Phase 3M-1c TX pump v3: inputBufferSize == 64 mirrors Thetis
        // getbuffsize(48000) at cmsetup.c:106-110 [v2.10.3.13] exactly.
        // Output buffer = 64 * txOutRate / 48000 — at 48 kHz out: 64; at
        // 192 kHz out (P2 G2): 256.
        m_txChannel = m_wdspEngine->createTxChannel(/*channelId=*/1,
                                                    /*inputBufferSize=*/64,
                                                    /*dspBufferSize=*/WdspEngine::kTxDspBufferSize,
                                                    /*inputSampleRate=*/48000,
                                                    /*dspSampleRate=*/WdspEngine::kTxDspSampleRate,
                                                    /*outputSampleRate=*/txOutRate);
        if (m_txChannel) {
            m_txChannel->setConnection(m_connection);

            // ── L.1: construct Pc + Radio mic sources + composite router ──────────
            // Construct after m_connection is live so RadioMicSource has a valid
            // connection pointer and caps are known for the hasMicJack gate.
            //
            // Ownership: RadioModel holds all three via unique_ptr (declared in
            // RadioModel.h §3M-1b L.1). CompositeTxMicRouter holds non-owning
            // raw pointers to the pc + radio sources — it must be reset FIRST
            // during teardown (see teardownConnection).
            //
            // hasMicJack gates RadioMicSource dispatch inside CompositeTxMicRouter.
            // On HL2 (hasMicJack=false) setActiveSource(Radio) is silently ignored
            // and Pc is always used.
            //
            // PcMicSource: non-QObject — no Qt parent needed.
            // RadioMicSource: QObject — parent=nullptr because unique_ptr owns
            //   the lifetime (Qt parent would cause double-free).
            //
            // Plan: 3M-1b Task L.1. Pre-code review §0.3 + master design §5.2.4.
            m_pcMicSource = std::make_unique<PcMicSource>(m_audioEngine);
            m_radioMicSource = std::make_unique<RadioMicSource>(m_connection, nullptr);
            const bool hasMicJack = m_hardwareProfile.caps
                                        ? m_hardwareProfile.caps->hasMicJack
                                        : true;  // safe default: assume mic jack present
            m_compositeMicRouter = std::make_unique<CompositeTxMicRouter>(
                m_pcMicSource.get(), m_radioMicSource.get(), hasMicJack);

            // Replace the 3M-1a NullMicSource stub with the composite router.
            m_txChannel->setMicRouter(m_compositeMicRouter.get());

            // L.1 connection 1: TxChannel siphon → AudioEngine TX monitor mix-in.
            // DirectConnection: both objects are used from the audio/DSP thread;
            // the sip1 callback must feed the monitor in-band (zero latency).
            // From pre-code review §0.3: sip1OutputReady carries post-stage-16
            // samples to the monitor bus without extra buffering.
            connect(m_txChannel, &TxChannel::sip1OutputReady,
                    m_audioEngine, &AudioEngine::txMonitorBlockReady,
                    Qt::DirectConnection);

            // L.1 connection 2 — REMOVED (Codex review on PR #149).
            // RadioMicSource::RadioMicSource subscribes to micFrameDecoded
            // itself with Qt::DirectConnection in its constructor; adding a
            // second QueuedConnection from RadioModel caused onMicFrame to
            // fire TWICE per frame from two producer threads (connection
            // thread + main thread), violating the SPSC ring's
            // single-producer assumption (m_writeIdx uses relaxed atomics).
            // The duplicated push corrupted the ring under load and was a
            // likely contributor to the audible noise floor JJ saw on the
            // bench. RadioMicSource owns the subscription; do not add a
            // second one here.

            // L.1 connection 3: TransmitModel mic preamp → TxChannel.
            // Auto (main thread → main thread); TxChannel::setMicPreamp is
            // thread-safe (atomic write per TxChannel.h E.2 notes).
            connect(&m_transmitModel, &TransmitModel::micPreampChanged,
                    m_txChannel, &TxChannel::setMicPreamp);
            // Initial-state sync: signal connections don't fire for the
            // current value. Without this push, TxChannel::m_micPreampLast
            // stays at its quiet_NaN sentinel and SetTXAPanelGain1(NaN)
            // produces silent SSB on the air. TUN uses gen-tone (different
            // gain stage) so it works without this. The mic-driven
            // fexchange2 path needs the initial preamp value to land.
            m_txChannel->setMicPreamp(m_transmitModel.micPreampLinear());

            // L.1 connection 4: TX monitor enable from TransmitModel.
            // setTxMonitorEnabled is atomic (E.3 design); auto connection.
            connect(&m_transmitModel, &TransmitModel::monEnabledChanged,
                    m_audioEngine, &AudioEngine::setTxMonitorEnabled);
            // 3M-1c K.1 — initial-state sync (mirrors the L.1 micPreamp push):
            // signal connects don't fire for the current value, so without
            // this push, AudioEngine::m_txMonitorEnabled stays at its
            // default-constructed false even if the user persisted a true
            // before disconnect. monEnabled doesn't actually persist (always
            // loads false per safety), so this push is functionally harmless
            // — but it closes the audit gap and stays robust if the
            // safety-default policy ever changes.
            m_audioEngine->setTxMonitorEnabled(m_transmitModel.monEnabled());

            // L.1 connection 5: TX monitor volume from TransmitModel.
            // setTxMonitorVolume is atomic (E.3 design); auto connection.
            connect(&m_transmitModel, &TransmitModel::monitorVolumeChanged,
                    m_audioEngine, &AudioEngine::setTxMonitorVolume);
            // 3M-1c K.2 — initial-state sync.  monitorVolume DOES persist
            // (audio.cs:417 [v2.10.3.13] literal default 0.5; user-tunable
            // and stored under hardware/<mac>/tx/MonitorVolume).  Without
            // this push, AudioEngine starts at its default 0.5 even if the
            // user saved e.g. 0.75 — first MOX cycle would be wrong volume.
            m_audioEngine->setTxMonitorVolume(m_transmitModel.monitorVolume());

            // ── L.1 (K.2 carry-forward): install MoxController BandPlanGuard check ──
            // Installs the moxCheck callback so setMox(true) consults BandPlanGuard
            // before any safety effects fire (see MoxController.cpp K.2 block).
            //
            // Closure captures: m_bandPlan, m_slices, m_hardwareProfile.
            //
            // The closure derives region from AppSettings (key "BandPlanRegion"
            // with Region2/UnitedStates as safe default matching Thetis).
            // preventDifferentBand and extended are not yet plumbed into RadioModel
            // (deferred to 3M-2+ as per the plan §L.1 TODO annotation).
            //
            // Cite: pre-code review §0.3 + MoxController.h K.2 API contract.
            if (m_moxController) {
                m_moxController->setMoxCheck([this]() -> safety::BandPlanGuard::MoxCheckResult {
                    // Derive region from AppSettings (same key used by SetupDialog).
                    // Default to Region2 (United States), matching Thetis behaviour
                    // when no region has been configured.
                    const int regionInt = AppSettings::instance()
                        .value(QStringLiteral("BandPlanRegion"),
                               QString::number(static_cast<int>(safety::Region::UnitedStates)))
                        .toInt();
                    const auto region = static_cast<safety::Region>(regionInt);

                    const SliceModel* slice = !m_slices.isEmpty() ? m_slices.first() : nullptr;
                    if (!slice) {
                        return {true, QString()};  // no slice → allow (no band context)
                    }

                    const auto freqHz = static_cast<std::int64_t>(slice->frequency());
                    const DSPMode mode = slice->dspMode();
                    // Band derived from m_lastBand (RadioModel's VFO band tracker).
                    // SliceModel has no band() accessor; m_lastBand is updated on
                    // every frequency change and reflects the current VFO band.
                    const Band rxBand  = m_lastBand;
                    // TX band: follow RX band (simplex). 3M-2/3F will separate TX band
                    // when split-VFO and cross-band TX are supported.
                    const Band txBand  = rxBand;

                    // preventDifferentBand and extended: deferred to 3M-2 / Setup TX page.
                    // TODO [3M-2]: wire to AppSettings keys "PreventDifferentBandTx" + "ExtendedTx".
                    const bool preventDifferentBand = false;
                    const bool extended = false;

                    return m_bandPlan.checkMoxAllowed(region, freqHz, mode,
                                                      rxBand, txBand,
                                                      preventDifferentBand, extended);
                });
            }

            // ── 3M-1c L.2: TwoToneController TxChannel injection ───────────────
            //
            // The controller's setTxChannel() must be called once m_txChannel is
            // live (and BEFORE any user can press the 2-TONE button — UI surfaces
            // are wired post-construction by MainWindow).  Cleared on teardown.
            // The other two deps (TransmitModel + MoxController) are wired in the
            // RadioModel ctor since they don't depend on a live connection.
            // SliceModel is wired earlier in connectToRadio() right after addSlice().
            if (m_twoToneController) {
                m_twoToneController->setTxChannel(m_txChannel);
            }

            // ── 3M-1c L.2 fixup: 5 TransmitModel two-tone signal connects + ──
            //                   initial-state pushes to TxChannel TXPostGen
            //                   wrappers (Phase L spec gap closure).
            //
            // Per pre-code review §2 + plan §L.2, the user-tunable two-tone
            // numerics (Freq1/Freq2/Level/Power/Freq2Delay) flow from the
            // model to TxChannel's TXPostGen wrapper setters in BOTH
            // continuous (TXPostGenMode=1) and pulsed (TXPostGenMode=7)
            // modes — Phase I's TwoToneController reads the values at
            // setActive(true) time, but the WDSP r2 stage still needs the
            // initial values pushed here so a fresh fexchange2 call after
            // connect doesn't see uninitialised TT params.  Mid-test
            // live-update of running TXPostGen state is deferred to 3M-3a
            // per plan caveat — these connects only push to the wrappers,
            // which are no-ops outside an active test cycle.
            //
            // Magnitude scaling (the 0.49999 * pow(10, dB/20) formula at
            // setup.cs:11056 [v2.10.3.13]) is applied INSIDE TwoToneController
            // before its WDSP setter calls; raw twoToneLevel is the dB
            // value the user set in Setup → Test → Two-Tone, NOT the linear
            // magnitude.  These L.2 connects therefore push the level as
            // a literal dB value to a separate TXPostGen path that
            // doesn't gate on the active-test flag — bench-verify in M.
            connect(&m_transmitModel, &TransmitModel::twoToneFreq1Changed,
                    m_txChannel, [this](int hz) {
                if (!m_txChannel) { return; }
                m_txChannel->setTxPostGenTTFreq1(static_cast<double>(hz));
                m_txChannel->setTxPostGenTTPulseToneFreq1(static_cast<double>(hz));
            });
            connect(&m_transmitModel, &TransmitModel::twoToneFreq2Changed,
                    m_txChannel, [this](int hz) {
                if (!m_txChannel) { return; }
                m_txChannel->setTxPostGenTTFreq2(static_cast<double>(hz));
                m_txChannel->setTxPostGenTTPulseToneFreq2(static_cast<double>(hz));
            });
            connect(&m_transmitModel, &TransmitModel::twoToneLevelChanged,
                    m_txChannel, [this](double db) {
                if (!m_txChannel) { return; }
                // Level is the dB amplitude (UI value, e.g. -6 dB).  The
                // WDSP TXPostGen mag fields expect a LINEAR magnitude in
                // [0, 0.49999] (`ttmag1` / `ttmag2` in gen.c).  Apply
                // the same conversion TwoToneController uses at activation
                // time so user-driven mid-test level changes don't push
                // an out-of-range raw dB into WDSP — that produced
                // muted / wrong-magnitude two-tone output (Codex P2 review
                // on PR #152).
                //
                // From Thetis setup.cs:11056 [v2.10.3.13]:
                //   ttmag1 = ttmag2 = 0.49999 * Math.Pow(10.0, ttmag / 20.0);
                // The literal 0.49999 MUST be preserved verbatim
                // (CLAUDE.md "Constants and Magic Numbers").
                const double mag = 0.49999 * std::pow(10.0, db / 20.0);
                m_txChannel->setTxPostGenTTMag1(mag);
                m_txChannel->setTxPostGenTTMag2(mag);
                m_txChannel->setTxPostGenTTPulseMag1(mag);
                m_txChannel->setTxPostGenTTPulseMag2(mag);
            });
            connect(&m_transmitModel, &TransmitModel::twoTonePowerChanged,
                    m_txChannel, [](int /*pct*/) {
                // TwoTonePower is consumed by TwoToneController at
                // setActive(true) when DrivePowerSource::Fixed is
                // selected — no TXPostGen analog.  Connect kept for
                // symmetry / future polish.
            });
            connect(&m_transmitModel, &TransmitModel::twoToneFreq2DelayChanged,
                    m_txChannel, [](int /*ms*/) {
                // TwoToneFreq2Delay is consumed by TwoToneController at
                // setActive(true) — no TXPostGen analog (the delay is
                // implemented as a controller-side QTimer::singleShot,
                // not a WDSP setter).  Connect kept for symmetry.
            });
            // Initial-state pushes (mirrors the L.1 micPreamp + K.1/K.2
            // pattern): signal connects don't fire for the current
            // value, so without these pushes a fresh TxChannel sees
            // uninitialised TT params.
            m_txChannel->setTxPostGenTTFreq1(static_cast<double>(m_transmitModel.twoToneFreq1()));
            m_txChannel->setTxPostGenTTFreq2(static_cast<double>(m_transmitModel.twoToneFreq2()));
            m_txChannel->setTxPostGenTTPulseToneFreq1(static_cast<double>(m_transmitModel.twoToneFreq1()));
            m_txChannel->setTxPostGenTTPulseToneFreq2(static_cast<double>(m_transmitModel.twoToneFreq2()));
            // Mirror the dB→linear conversion applied in the
            // twoToneLevelChanged lambda above — initial-state pushes
            // must use the same formula or the first activation runs
            // with raw-dB magnitudes (Codex P2 review on PR #152).
            // Source: Thetis setup.cs:11056 [v2.10.3.13].
            {
                const double initialLevelDb = m_transmitModel.twoToneLevel();
                const double initialMag = 0.49999 * std::pow(10.0, initialLevelDb / 20.0);
                m_txChannel->setTxPostGenTTMag1(initialMag);
                m_txChannel->setTxPostGenTTMag2(initialMag);
                m_txChannel->setTxPostGenTTPulseMag1(initialMag);
                m_txChannel->setTxPostGenTTPulseMag2(initialMag);
            }

            // ── 3M-1c TX pump architecture redesign: MoxController → TxChannel ──
            //                       queued connects (Phase 3M-1c spec §5.2)
            //
            // These 7 connects route MoxController emissions to TxChannel
            // setters with receiver=m_txChannel so Qt's AutoConnection
            // auto-resolves to QueuedConnection once m_txChannel is moved to
            // TxWorkerThread (a few lines below).  The lambda body then runs
            // on the worker thread, where m_txChannel->setX() is a same-
            // thread direct call — no cross-thread setter race.
            //
            // Why these are wired here (not in the RadioModel ctor):
            //   m_txChannel doesn't exist at construction time (createTxChannel
            //   runs inside this WDSP-init lambda).  Receiver thread affinity
            //   is what AutoConnection consults at signal-emission time, but
            //   the connection itself needs a non-null receiver to bind to —
            //   establishing it after m_txChannel is alive is the cleanest
            //   pattern.  Mirrors the L.2 fixup connects above.
            //
            // Why no in-lambda null guard:
            //   receiver=m_txChannel guarantees Qt auto-disconnects when
            //   m_txChannel is destroyed.  The lambda body cannot fire while
            //   m_txChannel is null.
            //
            // Source-of-truth: docs/architecture/phase3m-1c-tx-pump-architecture-plan.md
            // §5.2 last bullet (TxChannel cross-thread setter audit).

            // F.1 — txReady → setRunning(true).
            // From Thetis console.cs:29595 [v2.10.3.13] — TX-on callsite after
            // Thread.Sleep(rf_delay) in chkMOX_CheckedChanged2.
            connect(m_moxController, &MoxController::txReady,
                    m_txChannel, [this]() {
                m_txChannel->setRunning(true);
            });

            // F.1 — txaFlushed → setRunning(false).
            // From Thetis console.cs:29607 [v2.10.3.13] — TX-off callsite with
            // dmode=1 (drain) in the TX→RX branch.
            // Thread.Sleep(space_mox_delay); // default 0 // from PSDR MW0LGE  [console.cs:29603]
            connect(m_moxController, &MoxController::txaFlushed,
                    m_txChannel, [this]() {
                m_txChannel->setRunning(false);
            });

            // H.1 — voxRunRequested → setVoxRun.
            // From Thetis cmaster.cs:1039-1052 [v2.10.3.13] — CMSetTXAVoxRun.
            connect(m_moxController, &MoxController::voxRunRequested,
                    m_txChannel, [this](bool run) {
                m_txChannel->setVoxRun(run);
            });

            // H.2 — voxThresholdRequested → setVoxAttackThreshold.
            // From Thetis cmaster.cs:1054-1059 [v2.10.3.13] — CMSetTXAVoxThresh.
            connect(m_moxController, &MoxController::voxThresholdRequested,
                    m_txChannel, [this](double thresh) {
                m_txChannel->setVoxAttackThreshold(thresh);
            });

            // H.3 — voxHangTimeRequested → setVoxHangTime.
            // From Thetis setup.cs:18899 [v2.10.3.13] — SetDEXPHoldTime
            //   (ms→seconds applied in MoxController).
            connect(m_moxController, &MoxController::voxHangTimeRequested,
                    m_txChannel, [this](double seconds) {
                m_txChannel->setVoxHangTime(seconds);
            });

            // H.3 — antiVoxGainRequested → setAntiVoxGain.
            // From Thetis setup.cs:18989 [v2.10.3.13] — SetAntiVOXGain
            //   (dB→linear applied in MoxController).
            connect(m_moxController, &MoxController::antiVoxGainRequested,
                    m_txChannel, [this](double gain) {
                m_txChannel->setAntiVoxGain(gain);
            });

            // H.3 — antiVoxSourceWhatRequested → setAntiVoxRun.
            // useVax==false: per cmaster.cs:937-942 [v2.10.3.13], all RX slots
            // (RX1, RX1S, RX2) get source=1 (local-RX audio for antivox
            // reference).  3M-1b has one TxChannel paired with the active
            // slice; the three-slot iteration collapses to setAntiVoxRun(!useVax)
            // for the single-TX layout.  Full per-WDSP-channel
            // SetAntiVOXSourceWhat iteration is a 3F multi-pan concern.
            connect(m_moxController, &MoxController::antiVoxSourceWhatRequested,
                    m_txChannel, [this](bool useVax) {
                m_txChannel->setAntiVoxRun(!useVax);
            });

            // ── 3M-3 — TransmitModel → TxChannel TX processing chain wiring ─────
            //
            // 27 connects route TransmitModel setter signals into the TxChannel
            // WDSP wrappers, covering the full TX processing chain:
            //   1-13  3M-3a-i Batch 2 — TX EQ + Leveler + ALC
            //   14-17 3M-3a-ii Batch 3 — Phase Rotator (PhRot run + reverse +
            //                                            corner Hz + nstages)
            //   18-21 3M-3a-ii Batch 3 — CFC scalars (run + post-EQ run +
            //                                         pre-comp + pre-PEQ)
            //   22-24 3M-3a-ii Batch 3 — CFC profile arrays (collapsed into one
            //                                                pushCfcProfile helper)
            //   25-26 3M-3a-ii Batch 3 — CPDR (run + gain)
            //   27    3M-3a-ii Batch 3 — CESSB (run)
            // Receiver = m_txChannel so AutoConnection resolves to
            // QueuedConnection once the channel is moved onto TxWorkerThread
            // (a few lines below) — same pattern as the F.1 / H.1-H.3 / L.2
            // connects above.
            //
            // ── Initial sync — Thetis-faithful "active TX profile" restore ──
            //
            // Thetis applies the active TX profile on boot (setup.cs:9535-9541
            // [v2.10.3.13] — loadTXProfile invoked from console.cs init), which
            // pushes Lev_MaxGain=15 / ALC_MaximumGain=3 / EQ shape / etc. into
            // WDSP via the cmaster setters.  The "WDSP boot defaults stick
            // until the user moves a slider" policy that originally lived here
            // (path b — passive on initial state) was a NereusSDR-original
            // safety stance that broke persisted-state restoration: a user
            // who toggled TXEQ on, set a custom band shape, restarted, would
            // see TXEQ ON in the UI while WDSP silently ran with EQ off and
            // flat band gains.  Codex P1 review on PR #154 flagged this.
            //
            // Fix (Option C — profile-faithful): a `pushTxProcessingChain`
            // helper reads current TransmitModel state and pushes all 13
            // properties to TxChannel via the WDSP wrappers.  Called once
            // here (after the 13 connects but before moveToThread, so the
            // setter calls run on the main thread BEFORE TxWorkerThread
            // takes over — same pattern documented at line 1879-1881).  Also
            // wired to MicProfileManager::activeProfileChanged so future
            // user-driven profile picks (TxEqDialog combo, TxProfileSetupPage)
            // resync WDSP — necessary because applyValuesToModel routes through
            // TransmitModel setters, and the setters short-circuit on no-op
            // writes (value already matches), so the *Changed signal chain
            // can't be relied on alone.
            //
            // Consent for the on-boot ALC bump (0 dB WDSP boot → 3 dB Thetis
            // default) is captured at "user is running NereusSDR with the
            // shipped Default profile" — same consent model Thetis itself
            // uses.  Users who want WDSP boot defaults can save a profile
            // with ALC_MaximumGain=0 and activate it.
            //
            // ── TX EQ unified path: always SetTXAEQProfile ──
            //
            // The WDSP EQ has two write paths.  SetTXAGrphEQ10 takes 11 ints
            // (preamp + 10 band gains) and resets band centers to the fixed
            // 32/63/.../16k Hz.  SetTXAEQProfile takes a custom F[] vector
            // alongside G[] and is the only path that respects user-tuned
            // band frequencies.  NereusSDR exposes BOTH band gains AND band
            // freqs as user-tunable, so we go through the Profile path on
            // every EQ change — the Graph10 wrapper stays available for a
            // future "reset to default freqs" UX.

            auto pushEqProfile = [this]() {
                if (!m_txChannel) { return; }
                std::vector<double> freqs10(10, 0.0);
                std::vector<double> gains11(11, 0.0);
                gains11[0] = static_cast<double>(m_transmitModel.txEqPreamp());
                for (int i = 0; i < 10; ++i) {
                    freqs10[static_cast<std::size_t>(i)] =
                        static_cast<double>(m_transmitModel.txEqFreq(i));
                    gains11[static_cast<std::size_t>(i + 1)] =
                        static_cast<double>(m_transmitModel.txEqBand(i));
                }
                m_txChannel->setTxEqProfile(freqs10, gains11);
            };

            // CFC profile rebuild — mirrors pushEqProfile above.  CFC operates
            // on 10 user-visible bands.  WDSP setter signature:
            //   SetTXACFCOMPprofile(channel, nfreqs, F[], G[], E[], Qg[], Qe[])
            // We pass empty Qg / Qe vectors (translates to NULL inside the
            // wrapper), opting out of per-band Q skirts — the parametric Q
            // controls aren't yet exposed on the user surface (CFCParaEQData
            // schema column is currently an opaque blob).  cfcomp.c:669-682
            // [v2.10.3.13] documents the NULL semantic.
            auto pushCfcProfile = [this]() {
                if (!m_txChannel) { return; }
                constexpr int kCfcBands = 10;
                std::vector<double> F(kCfcBands);
                std::vector<double> G(kCfcBands);
                std::vector<double> E(kCfcBands);
                for (int i = 0; i < kCfcBands; ++i) {
                    F[static_cast<std::size_t>(i)] =
                        static_cast<double>(m_transmitModel.cfcEqFreq(i));
                    G[static_cast<std::size_t>(i)] =
                        static_cast<double>(m_transmitModel.cfcCompression(i));
                    E[static_cast<std::size_t>(i)] =
                        static_cast<double>(m_transmitModel.cfcPostEqBandGain(i));
                }
                m_txChannel->setTxCfcProfile(F, G, E, /*Qg=*/{}, /*Qe=*/{});
            };

            // Full-chain push — mirrors all 27 connect lambdas below by reading
            // current TransmitModel state and pushing to TxChannel.  Used for
            // the initial on-connect sync (loadFromSettings already fired the
            // *Changed signals before this connect block was installed, so
            // they were dropped on the floor) and for MicProfileManager::
            // activeProfileChanged (setActiveProfile's applyValuesToModel
            // setters short-circuit on no-op writes when profile values match
            // already-loaded live keys, so signal-driven sync isn't reliable).
            // Covers EQ + Leveler + ALC (3M-3a-i) AND CFC + CPDR + CESSB +
            // PhRot (3M-3a-ii Batch 3) — full 28-property TX-chain restore.
            auto pushTxProcessingChain = [this, pushEqProfile, pushCfcProfile]() {
                if (!m_txChannel) { return; }
                m_txChannel->setTxEqRunning(m_transmitModel.txEqEnabled());
                pushEqProfile();
                m_txChannel->setTxEqNc(m_transmitModel.txEqNc());
                m_txChannel->setTxEqMp(m_transmitModel.txEqMp());
                m_txChannel->setTxEqCtfmode(m_transmitModel.txEqCtfmode());
                m_txChannel->setTxEqWintype(m_transmitModel.txEqWintype());
                m_txChannel->setTxLevelerOn(m_transmitModel.txLevelerOn());
                m_txChannel->setTxLevelerTopDb(
                    static_cast<double>(m_transmitModel.txLevelerMaxGain()));
                m_txChannel->setTxLevelerDecayMs(m_transmitModel.txLevelerDecay());
                m_txChannel->setTxAlcMaxGainDb(
                    static_cast<double>(m_transmitModel.txAlcMaxGain()));
                m_txChannel->setTxAlcDecayMs(m_transmitModel.txAlcDecay());

                // ── 3M-3a-ii Batch 3 — Phase Rotator (4) ──
                m_txChannel->setStageRunning(TxChannel::Stage::PhRot,
                    m_transmitModel.phaseRotatorEnabled());
                m_txChannel->setTxPhrotReverse(m_transmitModel.phaseReverseEnabled());
                m_txChannel->setTxPhrotCornerHz(
                    static_cast<double>(m_transmitModel.phaseRotatorFreqHz()));
                m_txChannel->setTxPhrotNstages(m_transmitModel.phaseRotatorStages());

                // ── 3M-3a-ii Batch 3 — CFC scalars (4) ──
                m_txChannel->setTxCfcRunning(m_transmitModel.cfcEnabled());
                m_txChannel->setTxCfcPostEqRunning(m_transmitModel.cfcPostEqEnabled());
                m_txChannel->setTxCfcPrecompDb(
                    static_cast<double>(m_transmitModel.cfcPrecompDb()));
                m_txChannel->setTxCfcPrePeqDb(
                    static_cast<double>(m_transmitModel.cfcPostEqGainDb()));

                // ── 3M-3a-ii Batch 3 — CFC profile arrays (1 helper) ──
                pushCfcProfile();

                // ── 3M-3a-ii Batch 3 — CPDR (2) ──
                m_txChannel->setTxCpdrOn(m_transmitModel.cpdrOn());
                m_txChannel->setTxCpdrGainDb(
                    static_cast<double>(m_transmitModel.cpdrLevelDb()));

                // ── 3M-3a-ii Batch 3 — CESSB (1) ──
                m_txChannel->setTxCessbOn(m_transmitModel.cessbOn());
            };

            // 1. txEqEnabledChanged → setTxEqRunning.
            connect(&m_transmitModel, &TransmitModel::txEqEnabledChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setTxEqRunning(on);
            });

            // 2. txEqPreampChanged → rebuild full Profile (preamp lives in
            //    G[0] of the SetTXAEQProfile vector).
            connect(&m_transmitModel, &TransmitModel::txEqPreampChanged,
                    m_txChannel, [pushEqProfile](int /*dB*/) {
                pushEqProfile();
            });

            // 3. txEqBandChanged → rebuild full Profile (any single band
            //    edit pushes the whole 10-band shape).
            connect(&m_transmitModel, &TransmitModel::txEqBandChanged,
                    m_txChannel, [pushEqProfile](int /*idx*/, int /*dB*/) {
                pushEqProfile();
            });

            // 4. txEqFreqChanged → rebuild full Profile (custom-freq path).
            connect(&m_transmitModel, &TransmitModel::txEqFreqChanged,
                    m_txChannel, [pushEqProfile](int /*idx*/, int /*Hz*/) {
                pushEqProfile();
            });

            // 5. txEqNcChanged → setTxEqNc.
            connect(&m_transmitModel, &TransmitModel::txEqNcChanged,
                    m_txChannel, [this](int nc) {
                m_txChannel->setTxEqNc(nc);
            });

            // 6. txEqMpChanged → setTxEqMp.
            connect(&m_transmitModel, &TransmitModel::txEqMpChanged,
                    m_txChannel, [this](bool mp) {
                m_txChannel->setTxEqMp(mp);
            });

            // 7. txEqCtfmodeChanged → setTxEqCtfmode.
            connect(&m_transmitModel, &TransmitModel::txEqCtfmodeChanged,
                    m_txChannel, [this](int mode) {
                m_txChannel->setTxEqCtfmode(mode);
            });

            // 8. txEqWintypeChanged → setTxEqWintype.
            connect(&m_transmitModel, &TransmitModel::txEqWintypeChanged,
                    m_txChannel, [this](int wintype) {
                m_txChannel->setTxEqWintype(wintype);
            });

            // 9. txLevelerOnChanged → setTxLevelerOn.
            connect(&m_transmitModel, &TransmitModel::txLevelerOnChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setTxLevelerOn(on);
            });

            // 10. txLevelerMaxGainChanged → setTxLevelerTopDb.
            connect(&m_transmitModel, &TransmitModel::txLevelerMaxGainChanged,
                    m_txChannel, [this](int dB) {
                m_txChannel->setTxLevelerTopDb(static_cast<double>(dB));
            });

            // 11. txLevelerDecayChanged → setTxLevelerDecayMs.
            connect(&m_transmitModel, &TransmitModel::txLevelerDecayChanged,
                    m_txChannel, [this](int ms) {
                m_txChannel->setTxLevelerDecayMs(ms);
            });

            // 12. txAlcMaxGainChanged → setTxAlcMaxGainDb.
            connect(&m_transmitModel, &TransmitModel::txAlcMaxGainChanged,
                    m_txChannel, [this](int dB) {
                m_txChannel->setTxAlcMaxGainDb(static_cast<double>(dB));
            });

            // 13. txAlcDecayChanged → setTxAlcDecayMs.
            connect(&m_transmitModel, &TransmitModel::txAlcDecayChanged,
                    m_txChannel, [this](int ms) {
                m_txChannel->setTxAlcDecayMs(ms);
            });

            // ── 3M-3a-ii Batch 3 — CFC / CPDR / CESSB / PhRot routing ───────
            // 14 new connects route the 15 TransmitModel properties added in
            // 3M-3a-ii Batch 2 into the TxChannel WDSP wrappers added in
            // Batches 1 + 1.6.  3 array-changed signals collapse into a
            // shared pushCfcProfile() rebuild (matches the pushEqProfile
            // pattern at #2-#4 above).

            // 14. phaseRotatorEnabledChanged → Stage::PhRot run.
            connect(&m_transmitModel, &TransmitModel::phaseRotatorEnabledChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setStageRunning(TxChannel::Stage::PhRot, on);
            });

            // 15. phaseReverseEnabledChanged → setTxPhrotReverse.
            connect(&m_transmitModel, &TransmitModel::phaseReverseEnabledChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setTxPhrotReverse(on);
            });

            // 16. phaseRotatorFreqHzChanged → setTxPhrotCornerHz.
            connect(&m_transmitModel, &TransmitModel::phaseRotatorFreqHzChanged,
                    m_txChannel, [this](int hz) {
                m_txChannel->setTxPhrotCornerHz(static_cast<double>(hz));
            });

            // 17. phaseRotatorStagesChanged → setTxPhrotNstages.
            connect(&m_transmitModel, &TransmitModel::phaseRotatorStagesChanged,
                    m_txChannel, [this](int stages) {
                m_txChannel->setTxPhrotNstages(stages);
            });

            // 18. cfcEnabledChanged → setTxCfcRunning.
            connect(&m_transmitModel, &TransmitModel::cfcEnabledChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setTxCfcRunning(on);
            });

            // 19. cfcPostEqEnabledChanged → setTxCfcPostEqRunning.
            connect(&m_transmitModel, &TransmitModel::cfcPostEqEnabledChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setTxCfcPostEqRunning(on);
            });

            // 20. cfcPrecompDbChanged → setTxCfcPrecompDb.
            connect(&m_transmitModel, &TransmitModel::cfcPrecompDbChanged,
                    m_txChannel, [this](int dB) {
                m_txChannel->setTxCfcPrecompDb(static_cast<double>(dB));
            });

            // 21. cfcPostEqGainDbChanged → setTxCfcPrePeqDb.
            connect(&m_transmitModel, &TransmitModel::cfcPostEqGainDbChanged,
                    m_txChannel, [this](int dB) {
                m_txChannel->setTxCfcPrePeqDb(static_cast<double>(dB));
            });

            // 22. cfcEqFreqChanged → rebuild full CFC Profile (any single
            //     band edit pushes the whole 10-band F[]/G[]/E[] vector).
            connect(&m_transmitModel, &TransmitModel::cfcEqFreqChanged,
                    m_txChannel, [pushCfcProfile](int /*idx*/, int /*Hz*/) {
                pushCfcProfile();
            });

            // 23. cfcCompressionChanged → rebuild full CFC Profile (G[]).
            connect(&m_transmitModel, &TransmitModel::cfcCompressionChanged,
                    m_txChannel, [pushCfcProfile](int /*idx*/, int /*dB*/) {
                pushCfcProfile();
            });

            // 24. cfcPostEqBandGainChanged → rebuild full CFC Profile (E[]).
            connect(&m_transmitModel, &TransmitModel::cfcPostEqBandGainChanged,
                    m_txChannel, [pushCfcProfile](int /*idx*/, int /*dB*/) {
                pushCfcProfile();
            });

            // 25. cpdrOnChanged → setTxCpdrOn.
            connect(&m_transmitModel, &TransmitModel::cpdrOnChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setTxCpdrOn(on);
            });

            // 26. cpdrLevelDbChanged → setTxCpdrGainDb.
            connect(&m_transmitModel, &TransmitModel::cpdrLevelDbChanged,
                    m_txChannel, [this](int dB) {
                m_txChannel->setTxCpdrGainDb(static_cast<double>(dB));
            });

            // 27. cessbOnChanged → setTxCessbOn.
            connect(&m_transmitModel, &TransmitModel::cessbOnChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setTxCessbOn(on);
            });

            // Profile-activation resync.  Receiver = m_txChannel so this
            // becomes a QueuedConnection once moveToThread runs below; the
            // helper executes on TxWorkerThread for race-free WDSP setter
            // calls.  Triggered by user-driven profile picks (TxEqDialog,
            // TxProfileSetupPage) — see design comment above for why
            // signal-driven sync via setActiveProfile alone isn't reliable.
            if (m_micProfileMgr) {
                connect(m_micProfileMgr, &MicProfileManager::activeProfileChanged,
                        m_txChannel, [pushTxProcessingChain](const QString& /*name*/) {
                    pushTxProcessingChain();
                });
            }

            // Initial sync — push current TransmitModel state (loaded by
            // loadFromSettings at line 1106) into TxChannel before the worker
            // thread takes over.  Runs on the main thread; subsequent setter
            // calls land on TxWorkerThread via the queued connections above.
            // See line 1879-1881 for the design pattern this mirrors.
            pushTxProcessingChain();

            // ── 3M-1c TX pump architecture redesign: TxWorkerThread setup ──────
            //
            // Replaces the deleted L.4 MicReBlocker + D.1 AudioEngine
            // accumulator + bench-fix-A pumpMic timer + bench-fix-B
            // TxChannel silence-drive timer chain.  Mirrors Thetis's
            // `cm_main` worker-thread pattern (cmbuffs.c:151-168
            // [v2.10.3.13]) with NereusSDR's WDSP-r2-ring-divisibility
            // 256-sample block size end-to-end.
            //
            // Lifecycle (this block):
            //   1. Construct TxWorkerThread (RadioModel-owned via
            //      unique_ptr, parent=this for Qt cleanup safety).
            //   2. Wire deps: setTxChannel + setAudioEngine.
            //   3. Move TxChannel to the worker thread.  All connect()
            //      lambdas above already use AutoConnection, which
            //      auto-resolves to QueuedConnection now that the
            //      receiver lives on the worker thread.  The initial
            //      direct setter pushes already executed above on the
            //      main thread BEFORE this move — so the WDSP state is
            //      pre-loaded before the pump starts.
            //   4. startPump() — launches QThread, sets up the QTimer
            //      on the worker thread, enters the event loop.
            //
            // Teardown is in teardownConnection() further down.
            //
            // See plan §5.2 + §4.4 (cross-thread setter audit) and
            // src/core/TxWorkerThread.h for the full design rationale.
            if (m_audioEngine && m_txChannel) {
                // Phase 3M-1c TX pump v3: construct TxMicSource ALONGSIDE
                // TxWorkerThread.  Order matters:
                //   1. Construct TxMicSource and start() it (opens the
                //      inbound gate so the connection's parser can push
                //      mic samples even before the worker is ready).
                //   2. Hand the source to the connection so EP6/port-1026
                //      parsers route mic frames into the ring.
                //   3. Wire AudioEngine's PC mic override gate to
                //      TransmitModel::micSourceChanged.  Sync the initial
                //      value via a direct slot call (signal connections
                //      don't fire for the current value).
                //   4. Construct TxWorkerThread, attach the source as its
                //      cadence input, moveToThread + startPump.
                m_txMicSource = std::make_unique<TxMicSource>(this);
                m_txMicSource->start();

                if (auto* p1 = qobject_cast<P1RadioConnection*>(m_connection)) {
                    p1->setTxMicSource(m_txMicSource.get());
                } else if (auto* p2 = qobject_cast<P2RadioConnection*>(m_connection)) {
                    p2->setTxMicSource(m_txMicSource.get());
                }

                // PC mic override gate (Thetis cmaster.c:379 [v2.10.3.13]).
                // micSourceChanged emits MicSource enum; the slot needs a
                // bool ("is PC"), so funnel through a lambda.
                connect(&m_transmitModel, &TransmitModel::micSourceChanged,
                        m_audioEngine, [this](MicSource src) {
                            m_audioEngine->onMicSourceChanged(src == MicSource::Pc);
                        });
                m_audioEngine->onMicSourceChanged(
                    m_transmitModel.micSource() == MicSource::Pc);

                m_txWorker = std::make_unique<TxWorkerThread>(this);
                m_txWorker->setTxChannel(m_txChannel);
                m_txWorker->setAudioEngine(m_audioEngine);
                m_txWorker->setMicSource(m_txMicSource.get());
                m_txChannel->moveToThread(m_txWorker.get());
                m_txWorker->startPump();

                qCInfo(lcDsp) << "TX pump: TxWorkerThread started"
                              << "blockFrames=" << TxWorkerThread::kBlockFrames
                              << "(semaphore-wake, mic-frame-driven — 3M-1c v3)";
            }

            qCInfo(lcDsp) << "L.1: mic sources constructed (hasMicJack=" << hasMicJack
                          << "); composite router wired to TxChannel;"
                          << " 5 signal connections + K.2 moxCheck installed.";
            qCInfo(lcDsp) << "G.1: TX channel 1 created (deferred until conn live)"
                          << "outRate=" << txOutRate
                          << "— SSB voice path ready (L.1 composite router wired).";
        } else {
            qCWarning(lcDsp) << "G.1: deferred createTxChannel(1) returned nullptr —"
                                " TUNE will be unavailable until next reconnect.";
        }
    }

    // Wire the OcMatrix so P1/P2 buildCodecContext() can source ctx.ocByte
    // from maskFor(currentBand, mox) at C&C compose time.  Must be called
    // before the connection thread starts.  Phase 3P-D Task 3.
    if (auto* p1 = qobject_cast<class P1RadioConnection*>(m_connection)) {
        p1->setOcMatrix(&m_ocMatrix);
    } else if (auto* p2 = qobject_cast<class P2RadioConnection*>(m_connection)) {
        p2->setOcMatrix(&m_ocMatrix);
    }

    // Wire CalibrationController to P2RadioConnection so hzToPhaseWord()
    // applies effectiveFreqCorrectionFactor(). P1 uses raw Hz (not phase words),
    // so P1 doesn't need this. Phase 3P-G.
    if (auto* p2 = qobject_cast<class P2RadioConnection*>(m_connection)) {
        p2->setCalibrationController(&m_calController);
    }

    // Wire IoBoardHl2 so P1CodecHl2 can dequeue I2C transactions into C&C
    // frames and the ep6 read path can route responses back to the register
    // mirror.  On non-HL2 boards, setIoBoard() is a noop (selectCodec()
    // won't have installed a P1CodecHl2).  Phase 3P-E Task 2.
    if (auto* p1 = qobject_cast<class P1RadioConnection*>(m_connection)) {
        p1->setIoBoard(&m_ioBoard);
    }

    // Wire HermesLiteBandwidthMonitor so P1RadioConnection can record ep6/ep2
    // byte counts and drive the throttle-detection tick from onWatchdogTick().
    // The monitor is owned by RadioModel; the connection holds a non-owning ptr.
    // Phase 3P-E Task 3.
    if (auto* p1 = qobject_cast<class P1RadioConnection*>(m_connection)) {
        m_bwMonitor.reset();
        p1->setBandwidthMonitor(&m_bwMonitor);
    }

    // Create worker thread
    m_connThread = new QThread(this);
    m_connThread->setObjectName(QStringLiteral("ConnectionThread"));

    // Move connection to worker thread BEFORE wiring signals
    m_connection->moveToThread(m_connThread);

    // Wire signals (auto-queued across threads). Pass wdspInSize so the
    // DSP worker's accumulator drains in chunks that match the in_size
    // we just opened the WDSP channel with.
    wireConnectionSignals(wdspInSize);

    // Start thread — init() will be called on the worker thread
    connect(m_connThread, &QThread::started, m_connection, &RadioConnection::init);
    m_connThread->start();

    // CRITICAL: push sample rate + VFO frequency to the connection BEFORE
    // dispatching connectToRadio. The worker thread dequeues invokeMethod
    // calls in FIFO order, so whatever we queue first runs first. If we
    // queue connectToRadio before the setters, connectToRadio -> sendCommandFrame
    // -> composeEp2Frame reads m_rxFreqHz[0]=0 and m_sampleRate=48000 defaults
    // and sends a primed ep2 frame with phase word 0 to the radio just before
    // metis-start. Result: radio initializes DDC at freq=0 (bypass/idle state)
    // and streams ADC-pinned data with Q=0 forever. Verified against Thetis
    // NetworkIO.cs flow: Thetis always sets SetDDCRate + SetVFOfreq BEFORE
    // SendStartToMetis, so ForceCandCFrame inside SendStartToMetis reads the
    // correct freq/rate from globals.
    const int wireSampleRate = wdspInputRate;
    QMetaObject::invokeMethod(m_connection, [conn = m_connection, wireSampleRate]() {
        conn->setSampleRate(wireSampleRate);
    });
    // Push active receiver count to the connection. P1 uses this to encode
    // nrx bits in the C&C bank 0 frame. P2 DDC assignment is more complex
    // (Thetis console.cs:8216 UpdateDDCs — DDC2 is primary, not DDC0) and
    // is handled inside P2RadioConnection::connectToRadio. Calling
    // setActiveReceiverCount on P2 here would enable DDC0..N-1 on top of
    // the DDC2 enable that connectToRadio sets, leaving extra DDCs active.
    // Deferred to Phase 3F (multi-panadapter) which ports UpdateDDCs().
    if (info.protocol == ProtocolVersion::Protocol1) {
        QMetaObject::invokeMethod(m_connection, [conn = m_connection, activeRxCount]() {
            conn->setActiveReceiverCount(activeRxCount);
        });
    }
    if (m_activeSlice) {
        int hwRx = m_receiverManager->receiverConfig(0).hardwareRx;
        if (hwRx < 0) { hwRx = 0; }
        quint64 freqHz = m_activeSlice->frequency();
        QMetaObject::invokeMethod(m_connection, [conn = m_connection, hwRx, freqHz]() {
            conn->setReceiverFrequency(hwRx, freqHz);
        });
    }

    // ── Task 2.4 of P1 full-parity epic: initial push of TransmitModel state ─
    // Push lineInGain + userDigOut onto the connection BEFORE the first
    // connectToRadio dispatch so the very first C&C frame carries the
    // persisted model state instead of the connection-default 0/0.  Mirrors
    // the setSampleRate / setReceiverFrequency push pattern above (FIFO order
    // ensures these run before connectToRadio's sendCommandFrame).
    QMetaObject::invokeMethod(m_connection, [conn = m_connection,
                                              g = m_transmitModel.lineInGain()]() {
        conn->setLineInGain(g);
    });
    QMetaObject::invokeMethod(m_connection, [conn = m_connection,
                                              d = m_transmitModel.userDigOut()]() {
        conn->setUserDigOut(quint8(d & 0x0F));
    });

    // ── Task 2.5 of P1 full-parity epic: initial push of pureSig state ──────
    // Push the PureSignal user-enable toggle onto the connection BEFORE the
    // first connectToRadio dispatch so the very first C&C frame carries the
    // persisted state.  Mirrors the lineInGain/userDigOut FIFO ordering above.
    //
    // Source: Thetis ChannelMaster/networkproto1.c:599-600 [v2.10.3.13]:
    //   case 11:
    //     C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    // Until 3M-4 lights up the actual feedback DDC routing, the user's
    // PureSignal-enable toggle (PureSignalTab "Enable") is the proxy for
    // the wire bit — same semantic as Thetis PSForm.cs:240 [v2.10.3.13]
    // calling NetworkIO.SetPureSignal(1) when the user enables PS.
    QMetaObject::invokeMethod(m_connection, [conn = m_connection,
                                              ps = m_transmitModel.pureSigEnabled()]() {
        conn->setPuresignalRun(ps);
    });

    // Now dispatch connectToRadio -- it will find the correct m_rxFreqHz[0]
    // and m_sampleRate when sendCommandFrame runs inside it.
    QMetaObject::invokeMethod(m_connection, [conn = m_connection, info]() {
        conn->connectToRadio(info);
    });

    // Tell MainWindow / FFTEngine / SpectrumWidget the wire rate so bin math
    // matches the persisted hardware rate. Without this the FFT uses a stale
    // rate and compresses/expands the spectrum incorrectly.
    // Phase 3Q sub-PR-3: persist so connectionSampleRateHz() can report it.
    m_connectionSampleRateHz = wireSampleRate;
    emit wireSampleRateChanged(static_cast<double>(wireSampleRate));

    qCDebug(lcConnection) << "Connecting to" << info.displayName()
                          << "P" << static_cast<int>(info.protocol);
}

void RadioModel::disconnectFromRadio()
{
    m_intentionalDisconnect = true;
    teardownConnection();
}

void RadioModel::wireConnectionSignals(int wdspInSize)
{
    if (!m_connection) {
        return;
    }

    // Connection state → RadioModel (auto-queued: connection thread → main thread)
    connect(m_connection, &RadioConnection::connectionStateChanged,
            this, &RadioModel::onConnectionStateChanged);

    // --- Slice → WDSP + RadioConnection ---
    // Wire active slice property changes to WDSP DSP engine and radio hardware.
    wireSliceSignals();

    // --- I/Q data → ReceiverManager → DSP worker → WDSP → AudioEngine ---
    // Route through ReceiverManager for DDC-aware mapping, then dispatch
    // to RxDspWorker on its own thread for fexchange2 processing.

    // Step 1: RadioConnection I/Q → ReceiverManager (DDC routing).
    // Auto connection: m_connection is on its worker thread, this is on
    // main, so the slot is queued onto the main thread.
    connect(m_connection, &RadioConnection::iqDataReceived,
            this, [this](int ddcIndex, const QVector<float>& samples) {
        m_receiverManager->feedIqData(ddcIndex, samples);
    });

    // Step 2a: ReceiverManager → spectrum fork (main thread, fast).
    // Kept on the main thread so rawIqData → FFTEngine routing stays
    // unchanged. FFTEngine lives on its own SpectrumThread and the
    // signal already crosses threads via queued connection.
    connect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
            this, [this](int receiverIndex, const QVector<float>& samples) {
        Q_UNUSED(receiverIndex);
        emit rawIqData(samples);
    });

    // Step 2b: ReceiverManager → DSP worker (queued, off the main thread).
    // RxDspWorker accumulates samples into in_size chunks, runs each
    // chunk through RxChannel::processIq → fexchange2, then forwards
    // decoded audio to AudioEngine. fexchange2 must NOT run on the
    // main/GUI thread — see RxDspWorker.h for the deadlock rationale.
    Q_ASSERT(m_dspThread == nullptr && m_dspWorker == nullptr);
    m_dspThread = new QThread(this);
    m_dspThread->setObjectName(QStringLiteral("DspThread"));
    m_dspWorker = new RxDspWorker();   // no parent — moved to thread
    m_dspWorker->setEngines(m_wdspEngine, m_audioEngine);
    // Per-rate accumulator drain size. Must match the in_size that
    // WdspEngine::createRxChannel was called with above (line ~452),
    // otherwise fexchange2 sees the wrong sample count per call and
    // produces glitchy / jittery audio. WDSP RX output is always 64
    // samples per call (input_rate → 48000 decimation, dual-mono
    // panel via SetRXAPanelBinaural).
    m_dspWorker->setBufferSizes(wdspInSize, 64);
    m_dspWorker->moveToThread(m_dspThread);
    connect(m_dspThread, &QThread::finished,
            m_dspWorker, &QObject::deleteLater);
    connect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
            m_dspWorker, &RxDspWorker::processIqBatch,
            Qt::QueuedConnection);
    m_dspThread->start();

    // Phase 3Q-6: forward frame ticks to RadioModel::frameReceived() so
    // TitleBar::ConnectionSegment can pulse its activity LED. Using a
    // forwarding signal here means the segment never holds a raw
    // RadioConnection* that could be recreated on reconnect.
    connect(m_connection, &RadioConnection::frameReceived,
            this, &RadioModel::frameReceived);

    // Meter data → MeterModel
    connect(m_connection, &RadioConnection::meterDataReceived,
            this, [](float fwd, float rev, float voltage, float current) {
        Q_UNUSED(voltage);
        Q_UNUSED(current);
        Q_UNUSED(fwd);
        Q_UNUSED(rev);
    });

    // Phase 3P-H Task 4: PA telemetry → RadioStatus.
    // Apply per-board scaling (console.cs computeAlexFwdPower / computeRefPower
    // / convertToVolts / convertToAmps [@501e3f5]) and push the physical
    // values into the RadioStatus model owned by RadioModel. Any UI bound to
    // RadioStatus signals (Diagnostics → Radio Status page, S-meter PA tile)
    // refreshes automatically.
    //
    // P1 full-parity §3.4 (2026-05-02): the FWD reading is routed through
    // CalibrationController::calibratedFwdPowerWatts() inside
    // handlePaTelemetry — see that method for the inline cite.
    connect(m_connection, &RadioConnection::paTelemetryUpdated,
            this, [this](quint16 fwdRaw, quint16 revRaw, quint16 exciterRaw,
                         quint16 userAdc0Raw, quint16 userAdc1Raw,
                         quint16 supplyRaw) {
        handlePaTelemetry(fwdRaw, revRaw, exciterRaw,
                          userAdc0Raw, userAdc1Raw, supplyRaw);
    });

    // Error handling
    connect(m_connection, &RadioConnection::errorOccurred,
            this, [](NereusSDR::RadioConnectionError code, const QString& msg) {
        Q_UNUSED(code);
        qCWarning(lcConnection) << "Connection error:" << msg;
    });

    // Phase 3Q Task 10: auto-connect failure path.
    // When tryAutoReconnect() arms m_autoConnectInProgress, forward the
    // first connectFailed() emission as autoConnectFailed() so MainWindow
    // can open the ConnectionPanel and surface a status-bar message.
    // The flag is cleared immediately so a later user-initiated Connect
    // does not re-trigger this path.
    connect(m_connection, &RadioConnection::connectFailed,
            this, [this](NereusSDR::ConnectFailure reason, const QString& detail) {
        Q_UNUSED(detail);
        if (m_autoConnectInProgress) {
            const QString mac = m_autoConnectChosenMac;
            m_autoConnectInProgress = false;
            m_autoConnectChosenMac.clear();
            emit autoConnectFailed(mac, reason);
        }
    });

    // ReceiverManager → RadioConnection (hardware updates)
    connect(m_receiverManager, &ReceiverManager::hardwareReceiverCountChanged,
            this, [this](int count) {
        if (m_connection) {
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, count]() {
                conn->setActiveReceiverCount(count);
            });
        }
    });

    connect(m_receiverManager, &ReceiverManager::hardwareFrequencyChanged,
            this, [this](int hwIndex, quint64 freq) {
        if (m_connection) {
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, hwIndex, freq]() {
                conn->setReceiverFrequency(hwIndex, freq);
            });
        }
    });

    // H.5: P1/P2 status-frame mic_ptt → MoxController PTT-source dispatch.
    // Source: Thetis console.cs:25426 [v2.10.3.13] PollPTT:
    //   bool mic_ptt = (dotdashptt & 0x01) != 0; // PTT from radio
    // P1 bit-source: networkproto1.c:329 [v2.10.3.13] ControlBytesIn[0] & 0x1
    // P2 bit-source: network.c:689 [v2.10.3.13] ReadBufp[0] & 0x1
    //
    // m_connection lives on the connection thread; m_moxController lives on the
    // main thread.  Qt::AutoConnection would queue across threads automatically,
    // but explicit QueuedConnection documents the intent and is always correct
    // for cross-thread slot dispatch.
    if (m_moxController) {
        connect(m_connection, &RadioConnection::micPttFromRadio,
                m_moxController, &MoxController::onMicPttFromRadio,
                Qt::QueuedConnection);
    }

    // ── Task 2.4 of P1 full-parity epic: TransmitModel → RadioConnection ────
    // Wire lineInGain + userDigOut model-layer signals to the wire-bit setters
    // added in Tasks 2.1 and 2.2.  Both connection setters live on the worker
    // thread, so cross-thread dispatch goes through Qt::QueuedConnection (or
    // QMetaObject::invokeMethod for the int→quint8 adapter).
    //
    // Source: Thetis ChannelMaster/networkproto1.c:600-601 [v2.10.3.13]:
    //   case 11:
    //     C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    //     C3 = prn->user_dig_out & 0b00001111;
    //
    // userDigOut needs the lambda because Q_PROPERTY(int) doesn't directly
    // bind to setUserDigOut(quint8) — masked to low 4 bits at the bridge.
    QObject::connect(&m_transmitModel, &TransmitModel::lineInGainChanged,
                     m_connection, &RadioConnection::setLineInGain,
                     Qt::QueuedConnection);
    QObject::connect(&m_transmitModel, &TransmitModel::userDigOutChanged, m_connection,
                     [conn = m_connection](int d) {
        QMetaObject::invokeMethod(conn, [conn, d]() {
            conn->setUserDigOut(quint8(d & 0x0F));
        });
    });

    // ── Task 2.5 of P1 full-parity epic: pureSig → setPuresignalRun ─────────
    // Wire the user PureSignal-enable toggle to the wire-bit setter added in
    // Task 2.3.  Direct signal→slot bind (bool→bool, no adapter needed).
    //
    // Source: Thetis PSForm.cs:240 [v2.10.3.13]
    //   _psenabled = value;
    //   if (_psenabled) {
    //     ...
    //     NetworkIO.SetPureSignal(1);   // → prn->puresignal_run = 1
    //     ...
    //   }
    // Source: Thetis ChannelMaster/networkproto1.c:599-600 [v2.10.3.13]:
    //   case 11:
    //     C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    //
    // Until 3M-4 lights up the actual feedback DDC routing, the user's
    // PureSignal-enable toggle (PureSignalTab "Enable" checkbox) is the proxy
    // for the wire bit — same semantic as Thetis's PSEnabled property setter
    // calling NetworkIO.SetPureSignal(1).  The P2 override (Task 2.3) stores
    // the flag for symmetric API only and emits nothing on the wire until
    // 3M-4 wires up the feedback DDC routing.
    QObject::connect(&m_transmitModel, &TransmitModel::pureSigChanged,
                     m_connection, &RadioConnection::setPuresignalRun,
                     Qt::QueuedConnection);
}

// P1 full-parity §3.4: per-sample PA telemetry handler.
// Extracted from the wireConnectionSignals lambda so the test hook
// handlePaTelemetryForTest() can drive the routing without spinning up
// the full DSP-thread / RxDspWorker pipeline.
void RadioModel::handlePaTelemetry(quint16 fwdRaw, quint16 revRaw,
                                   quint16 exciterRaw, quint16 userAdc0Raw,
                                   quint16 userAdc1Raw, quint16 supplyRaw)
{
    const HPSDRModel model = m_hardwareProfile.model;
    const double fwdW   = scaleFwdPowerWatts(fwdRaw, model);
    const double revW   = scaleRevPowerWatts(revRaw, model);
    const double paV    = scalePaVolts(userAdc0Raw, model);
    const double paA    = scalePaAmps(userAdc1Raw, model);
    const double paTemp = scalePaTemperatureCelsius(0, model);
    Q_UNUSED(paV);       // RadioStatus does not expose PA volts directly (per its design header)
    Q_UNUSED(supplyRaw); // supply_volts surfaced via RadioConnection::supplyVoltsChanged signal (sub-PR-2 B.3)

    // From Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13] —
    // route raw alex_fwd through the per-board cal table before publishing
    // to RadioStatus.  Identity transform when no profile is loaded
    // (boardClass == None, see CalibrationController::calibratedFwdPowerWatts).
    // Reflected-power path is unchanged: Thetis's CalibratedPAPower is FWD-only.
    const double fwdWCal = double(
        m_calController.calibratedFwdPowerWatts(static_cast<float>(fwdW)));

    m_radioStatus.setForwardPower(fwdWCal);
    m_radioStatus.setReflectedPower(revW);
    m_radioStatus.setExciterPowerMw(static_cast<int>(exciterRaw));
    m_radioStatus.setPaCurrent(paA);
    // Only push temp when we have a real source (non-zero); leaves the
    // last-known value alone otherwise so a stale 0 doesn't overwrite a
    // good HL2 reading from another path.
    if (paTemp > 0.0) {
        m_radioStatus.setPaTemperature(paTemp);
    }

    // Phase 3M-0 Task 17 + Codex P1 follow-up: feed SwrProtectionController
    // here (one call per hardware sample with consistent fwd/rev), not
    // from RadioStatus::powerChanged (which emits twice per sample).
    // Note: SWR protection ingests the raw post-scale fwdW (not fwdWCal) —
    // the user-cal table can extrapolate above-bridge values that would
    // skew the foldback math; raw bridge watts are the canonical input
    // Thetis uses for protection (console.cs alex_fwd path is independent
    // of CalibratedPAPower).
    m_swrProt.ingest(static_cast<float>(fwdW),
                     static_cast<float>(revW),
                     m_transmitModel.isTune());
}

// Wire active slice signals to WDSP channel and radio hardware.
// Called from wireConnectionSignals after connection is established.
void RadioModel::wireSliceSignals()
{
    if (!m_activeSlice || !m_connection) {
        return;
    }

    SliceModel* slice = m_activeSlice;

    // Frequency → ReceiverManager → radio hardware
    // ReceiverManager handles DDC mapping (receiver 0 → DDC2 for ANAN-G2)
    connect(slice, &SliceModel::frequencyChanged, this, [this, slice](double freq) {
        int rxIdx = slice->receiverIndex();
        if (rxIdx >= 0) {
            m_receiverManager->setReceiverFrequency(rxIdx, static_cast<quint64>(freq));
        }
        // TX follows RX (simplex)
        if (m_connection) {
            quint64 freqHz = static_cast<quint64>(freq);
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, freqHz]() {
                conn->setTxFrequency(freqHz);
            });
        }
        // Track band from VFO frequency so per-band saves target the correct
        // band even when the panadapter center hasn't crossed the boundary.
        //
        // Do NOT recall bandstack state on a VFO-driven band crossing. From
        // Thetis console.cs:45312 handleBSFChange [@501e3f5]:
        // on an oldBand != newBand transition, Thetis only updates the old
        // and new band's LastVisited records — it does not restore saved
        // DSP state. Bandstack recall is reserved for the explicit
        // band-button press path. Trying to recall here on every wheel-tune
        // caused two bugs in v0.2.0: (1) the VFO snaps to the newBand's
        // stored frequency, breaking smooth wheel-tune across boundaries;
        // (2) saveToSettings(oldBand) wrote the current (now post-tune)
        // frequency into the oldBand slot — corrupting the stored value
        // for that band. Letting the coalesced scheduleSettingsSave() flush
        // keeps the CURRENT band's slot up to date without either bug.
        Band newBand = bandFromFrequency(freq);
        if (newBand != m_lastBand) {
            qCDebug(lcConnection) << "T10: band crossing" << bandLabel(m_lastBand)
                                  << "→" << bandLabel(newBand)
                                  << "(freq=" << freq << "Hz)";
            m_lastBand = newBand;
            // Phase 3P-I-a T10 — reapply per-band antenna on boundary
            // crossing. Thetis UpdateAlexAntSelection equivalent
            // (HPSDR/Alex.cs:310 [@501e3f5]).
            applyAlexAntennaForBand(newBand);
            // Phase 3P-I-a T10 follow-up — refresh the slice's cached
            // rxAntenna/txAntenna labels from AlexController so the
            // VFO Flag and RxApplet buttons show the new band's value.
            // Without this call the wire switched but the UI stayed
            // on the previous band's label (caught during PR #N
            // bench testing — KG4VCF 2026-04-22). Mirrors the T9
            // path at line 476-478.
            if (m_activeSlice) {
                m_activeSlice->refreshAntennasFromAlex(m_alexController, newBand);
            }
        }
        scheduleSettingsSave();
    });

    // Mode → WDSP
    connect(slice, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setMode(mode);
        }
        scheduleSettingsSave();
    });

    // Filter → WDSP
    connect(slice, &SliceModel::filterChanged, this, [this](int low, int high) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setFilterFreqs(low, high);
        }
        scheduleSettingsSave();
    });

    // AGC → WDSP
    connect(slice, &SliceModel::agcModeChanged, this, [this](AGCMode mode) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcMode(mode);
        }
        scheduleSettingsSave();
    });

    // AGC advanced → WDSP
    // From Thetis Project Files/Source/Console/console.cs:45977 — AGCThresh
    // From Thetis Project Files/Source/Console/radio.cs:1037-1124 — Decay/Hang/Slope
    // From Thetis Project Files/Source/Console/dsp.cs:116-120 — P/Invoke decls
    //
    // Bidirectional sync: SetRXAAGCThresh and SetRXAAGCTop both write max_gain
    // in WDSP wcpAGC.c. After either changes, read back the sibling value and
    // update the paired control. m_syncingAgc guards against A→B→A feedback loops.
    // From Thetis console.cs:45960-46006 — bidirectional AGC sync pattern.
    connect(slice, &SliceModel::agcThresholdChanged, this, [this](int dBu) {
        if (m_syncingAgc) { return; }

        // From Thetis v2.10.3.13 console.cs:49129-49130 — manual drag disables auto
        SliceModel* s = m_activeSlice;
        if (s && s->autoAgcEnabled()) {
            s->setAutoAgcEnabled(false);
        }

        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            m_syncingAgc = true;
            rxCh->setAgcThreshold(dBu);
            // Read back resulting AGC Top and sync RF Gain display.
            // From Thetis console.cs:45978 — GetRXAAGCTop after SetRXAAGCThresh
            double top = rxCh->readBackAgcTop();
            int rfGain = static_cast<int>(std::round(top));
            SliceModel* s = m_activeSlice;
            if (s && s->rfGain() != rfGain) {
                s->setRfGain(rfGain);
            }
            m_syncingAgc = false;
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcHangChanged, this, [this](int ms) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcHang(ms);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcSlopeChanged, this, [this](int slope) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcSlope(slope);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcAttackChanged, this, [this](int ms) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcAttack(ms);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcDecayChanged, this, [this](int ms) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcDecay(ms);
        }
        scheduleSettingsSave();
    });

    // From Thetis v2.10.3.13 setup.cs:9081 — hang threshold
    connect(slice, &SliceModel::agcHangThresholdChanged, this, [this](int val) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcHangThreshold(val);
        }
        scheduleSettingsSave();
    });

    // From Thetis v2.10.3.13 setup.cs:9001 — fixed gain
    connect(slice, &SliceModel::agcFixedGainChanged, this, [this](int dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcFixedGain(dB);
        }
        scheduleSettingsSave();
    });

    // From Thetis v2.10.3.13 setup.cs:9011 — max gain
    connect(slice, &SliceModel::agcMaxGainChanged, this, [this](int dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAgcMaxGain(dB);
        }
        scheduleSettingsSave();
    });

    // ── Auto AGC-T timer ────────────────────────────────────────────────
    // From Thetis v2.10.3.13 console.cs:46057 — tmrAutoAGC_Tick, 500ms interval
    m_autoAgcTimer = new QTimer(this);
    m_autoAgcTimer->setInterval(500);
    connect(m_autoAgcTimer, &QTimer::timeout, this, [this]() {
        SliceModel* slice = m_activeSlice;
        if (!slice || !slice->autoAgcEnabled()) {
            return;
        }
        // From Thetis v2.10.3.13 console.cs:46059 — guard: skip if not connected or MOX
        if (!m_connection || !m_connection->isConnected()) {
            return;
        }
        // From Thetis v2.10.3.13 console.cs:46059 — if (!chkPower.Checked || _mox) return;
        if (m_transmitModel.isMox()) {
            return;
        }
        if (!m_noiseFloorTracker || !m_noiseFloorTracker->isGood()) {
            return;
        }

        // From Thetis v2.10.3.13 console.cs:46107-46115
        const double noiseFloor = static_cast<double>(m_noiseFloorTracker->noiseFloor());

        // From Thetis v2.10.3.13 console.cs:33292-33319 — agcCalOffset(rx)
        // Simplified: 0.0f for FIXD (NereusSDR AGCMode::Off), 2.0f for others
        // Full formula: 2.0f + (DisplayCalOffset + PreampOffset - AlexPreampOffset - FFTSizeOffset)
        // Alex/preamp/FFT-size offsets land with spectrum knee line overlay
        const float calOffset = (slice->agcMode() == AGCMode::Off)
            ? 0.0f : 2.0f;

        // From Thetis v2.10.3.13 console.cs:45965-45968 — apply cal offset
        const double threshold = (noiseFloor + slice->autoAgcOffset())
                                 - static_cast<double>(calOffset);

        // From Thetis v2.10.3.13 console.cs:45969-45970 — clamp [-160, +2]
        const double clamped = std::clamp(threshold, -160.0, 2.0);
        const int threshInt = static_cast<int>(std::round(clamped));

        // Update both WDSP and model. m_syncingAgc prevents the
        // agcThresholdChanged handler from disabling auto mode AND from
        // re-entering the WDSP call, so we must call RxChannel directly.
        if (slice->agcThreshold() != threshInt) {
            m_syncingAgc = true;

            // Direct WDSP update — the signal handler is blocked by m_syncingAgc
            RxChannel* rxCh = m_wdspEngine ? m_wdspEngine->rxChannel(0) : nullptr;
            if (rxCh) {
                rxCh->setAgcThreshold(threshInt);
                // From Thetis v2.10.3.13 console.cs:45978 — readback AGC top
                double top = rxCh->readBackAgcTop();
                int rfGain = static_cast<int>(std::round(top));
                if (slice->rfGain() != rfGain) {
                    slice->setRfGain(rfGain);
                }
            }

            // Update model (UI sync) — handler won't re-enter WDSP
            slice->setAgcThreshold(threshInt);
            m_syncingAgc = false;
        }
    });
    m_autoAgcTimer->start();

    // ─── Sub-epic C-1 Task 19: full SliceModel → RxChannel NR tuning bridge ──
    //
    // Each tuning-knob signal is forwarded to the corresponding RxChannel
    // setter so live slider adjustments in Setup → DSP → NR and the VFO
    // popup audibly change the WDSP filter chain in real time.
    //
    // Thetis pattern: console.cs:43297 SelectNR [v2.10.3.13] — push
    // parameters before the active-slot run-flag.

    // NR1 (ANR) — 5 knobs
    // From Thetis setup.cs:8539-8566 [v2.10.3.13]
    connect(slice, &SliceModel::nr1TapsChanged, this, [this](int v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setAnrTaps(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr1DelayChanged, this, [this](int v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setAnrDelay(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr1GainChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setAnrGain(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr1LeakageChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setAnrLeakage(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr1PositionChanged, this, [this](NereusSDR::NrPosition p) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setAnrPosition(p); }
        scheduleSettingsSave();
    });

    // NR2 (EMNR) — gain-method + npe-method + AE filter + position + Post2 cascade
    // From Thetis setup.cs NR2 group [v2.10.3.13]
    connect(slice, &SliceModel::nr2GainMethodChanged, this, [this](NereusSDR::EmnrGainMethod v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setEmnrGainMethod(static_cast<int>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2NpeMethodChanged, this, [this](NereusSDR::EmnrNpeMethod v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setEmnrNpeMethod(static_cast<int>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2TrainT1Changed, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setEmnrTrainT1(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2TrainT2Changed, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setEmnrTrainT2(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2AeFilterChanged, this, [this](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setEmnrAeRun(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2PositionChanged, this, [this](NereusSDR::NrPosition p) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setEmnrPosition(static_cast<int>(p)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2Post2RunChanged, this, [this](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setEmnrPost2Run(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2Post2LevelChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setEmnrPost2Level(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2Post2FactorChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setEmnrPost2Factor(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2Post2RateChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setEmnrPost2Rate(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2Post2TaperChanged, this, [this](int v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setEmnrPost2Taper(v); }
        scheduleSettingsSave();
    });

    // NR3 (RNNR) — position + useDefaultGain
    // From Thetis setup.cs:35460-35462 [v2.10.3.13]
    connect(slice, &SliceModel::nr3PositionChanged, this, [this](NereusSDR::NrPosition p) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setRnnrPosition(p); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr3UseDefaultGainChanged, this, [this](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setRnnrUseDefaultGain(v); }
        scheduleSettingsSave();
    });

    // NR4 (SBNR) — 5 spinboxes + algo
    // From Thetis setup.cs:34511-34527 [v2.10.3.13]
    connect(slice, &SliceModel::nr4ReductionChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setSbnrReductionAmount(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr4SmoothingChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setSbnrSmoothingFactor(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr4WhiteningChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setSbnrWhiteningFactor(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr4RescaleChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setSbnrNoiseRescale(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr4PostThreshChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setSbnrPostFilterThreshold(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr4AlgoChanged, this, [this](NereusSDR::SbnrAlgo a) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setSbnrAlgo(a); }
        scheduleSettingsSave();
    });

#ifdef HAVE_DFNR
    // DFNR — AttenLimit + PostFilterBeta
    // double→float cast at the boundary (SliceModel stores double for QSpinBox compat)
    connect(slice, &SliceModel::dfnrAttenLimitChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setDfnrAttenLimit(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::dfnrPostFilterBetaChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setDfnrPostFilterBeta(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
#endif

#ifdef HAVE_MNR
    // MNR — SliceModel mnrStrength already in 0.0–1.0 (the Setup/popup
    // slider applies the ×100 / ÷100 UI↔model conversion on both sides).
    connect(slice, &SliceModel::mnrStrengthChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setMnrStrength(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::mnrOversubChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setMnrOversub(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::mnrFloorChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setMnrFloor(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::mnrAlphaChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setMnrAlpha(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::mnrBiasChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setMnrBias(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::mnrGsmoothChanged, this, [this](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) { rxCh->setMnrGsmooth(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
#endif

    // NR slot → WDSP active-run dispatch.
    // Push tuning params before the run-flag; all per-knob connects above fire
    // in real time, so the active-slot connect here just needs to switch the
    // WDSP run flags. Kept as a dedicated connect so it also fires on the
    // VFO-popup NR toggle without needing a full struct rebuild.
    // From Thetis console.cs:43297 SelectNR [v2.10.3.13]
    connect(slice, &SliceModel::activeNrChanged, this, [this](NereusSDR::NrSlot slot) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setActiveNr(slot);
        }
        scheduleSettingsSave();
    });

    // SNB → WDSP
    // From Thetis Project Files/Source/Console/console.cs:36347
    //   WDSP.SetRXASNBARun(WDSP.id(0, 0), chkDSPNB2.Checked)
    // WDSP: third_party/wdsp/src/snb.c:579
    connect(slice, &SliceModel::snbEnabledChanged, this, [this](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setSnbEnabled(on);
        }
        scheduleSettingsSave();
    });

    // NB mode (NB1 / NB2 / Off) → WDSP
    // From Thetis Project Files/Source/Console/console.cs — chkDSPNB1/chkDSPNB2 Checked
    // WDSP: third_party/wdsp/src/anb.c (SetRXAANBRun) + third_party/wdsp/src/nob.c (SetRXANOBRun)
    connect(slice, &SliceModel::nbModeChanged, this, [this](NereusSDR::NbMode m) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setNbMode(m);
        }
        scheduleSettingsSave();
    });

    // NB tuning wiring removed 2026-04-22 — no longer per-slice. All NB
    // tuning lives inside NbFamily, seeded from AppSettings at ctor and
    // live-pushed from Setup → DSP → NB/SNB handlers in DspSetupPages.cpp.

    // APF → WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1910-1927
    //   WDSP.SetRXASPCWRun(WDSP.id(thread, subrx), value)
    // WDSP: third_party/wdsp/src/apfshadow.c:93
    connect(slice, &SliceModel::apfEnabledChanged, this, [this](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setApfEnabled(on);
        }
        scheduleSettingsSave();
    });

    // APF tune offset → WDSP freq
    // From Thetis Project Files/Source/Console/setup.cs:17068-17073
    //   freq = CWPitch + tuneOffset; slider offset range -250..+250
    //   CW pitch default 600 Hz from Thetis console.cs
    // WDSP: third_party/wdsp/src/apfshadow.c:117
    connect(slice, &SliceModel::apfTuneHzChanged, this, [this](int hz) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            // From Thetis setup.cs:17071 — freq = CWPitch + tuneOffset
            // CW pitch default 600 Hz from Thetis console.cs
            static constexpr double kCwPitchHz = 600.0;
            rxCh->setApfFreq(kCwPitchHz + static_cast<double>(hz));
        }
        scheduleSettingsSave();
    });

    // Squelch — SSB → WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1185-1229
    // WDSP: third_party/wdsp/src/ssql.c:331,339
    connect(slice, &SliceModel::ssqlEnabledChanged, this, [this](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setSsqlEnabled(on);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::ssqlThreshChanged, this, [this](double threshold) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            // Model stores 0–100 (slider units); WDSP expects 0.0–1.0 linear.
            // From Thetis radio.cs:1217-1218 — clamped 0..1, default 0.16.
            double normalized = std::clamp(threshold / 100.0, 0.0, 1.0);
            rxCh->setSsqlThresh(normalized);
        }
        scheduleSettingsSave();
    });

    // Squelch — AM → WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1164-1178, 1293-1310
    // WDSP: third_party/wdsp/src/amsq.c (SetRXAAMSQRun, SetRXAAMSQThreshold)
    connect(slice, &SliceModel::amsqEnabledChanged, this, [this](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAmsqEnabled(on);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::amsqThreshChanged, this, [this](double dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAmsqThresh(dB);
        }
        scheduleSettingsSave();
    });

    // Squelch — FM → WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1274-1329
    // WDSP: third_party/wdsp/src/fmsq.c:236,244
    // SliceModel stores fmsqThresh in dB; RxChannel::setFmsqThresh converts to linear
    connect(slice, &SliceModel::fmsqEnabledChanged, this, [this](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setFmsqEnabled(on);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::fmsqThreshChanged, this, [this](double dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setFmsqThresh(dB);
        }
        scheduleSettingsSave();
    });

    // Audio panel — mute / pan / binaural → WDSP PatchPanel
    // From Thetis Project Files/Source/Console/radio.cs:1386-1403 (pan)
    // From Thetis Project Files/Source/Console/radio.cs:1145-1162 (binaural)
    // WDSP: third_party/wdsp/src/patchpanel.c:126,159,187
    connect(slice, &SliceModel::mutedChanged, this, [this](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setMuted(v);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::audioPanChanged, this, [this](double pan) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setAudioPan(pan);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::binauralEnabledChanged, this, [this](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            rxCh->setBinauralEnabled(v);
        }
        scheduleSettingsSave();
    });

    // RIT + DIG offset → WDSP shift frequency
    //
    // RIT (Receive Incremental Tuning): client-side demodulation offset that
    // does NOT retune the hardware VFO.
    // From Thetis console.cs — RIT adjusts receive demodulation without moving
    // the hardware DDC center.
    //
    // DIG offset: per-mode click-tune demodulation offset for DIGL/DIGU.
    // From Thetis console.cs:14637 (DIGUClickTuneOffset) and :14672
    // (DIGLClickTuneOffset). Both are int offsets in Hz; Thetis uses per-mode
    // filter re-centering internally, but NereusSDR implements DIG offset as
    // an additive shift on the same setShiftFrequency path as RIT.
    //
    // Combined: shift = ritOffset + digOffset (where digOffset is mode-gated).
    // For 3G-10 (single RX, no CTUN), the shift = these two terms only.
    auto updateShiftFrequency = [this, slice]() {
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (!rxCh) { return; }
        double offset = slice->ritEnabled()
                        ? static_cast<double>(slice->ritHz())
                        : 0.0;
        // DIG offset per mode — Thetis console.cs:14637,14672
        if (slice->dspMode() == DSPMode::DIGL) {
            offset += static_cast<double>(slice->diglOffsetHz());
        } else if (slice->dspMode() == DSPMode::DIGU) {
            offset += static_cast<double>(slice->diguOffsetHz());
        }
        rxCh->setShiftFrequency(offset);
    };
    connect(slice, &SliceModel::ritEnabledChanged,  this, updateShiftFrequency);
    connect(slice, &SliceModel::ritHzChanged,        this, updateShiftFrequency);
    connect(slice, &SliceModel::diglOffsetHzChanged, this, updateShiftFrequency);
    connect(slice, &SliceModel::diguOffsetHzChanged, this, updateShiftFrequency);
    connect(slice, &SliceModel::dspModeChanged,      this, updateShiftFrequency);

    // RTTY mark + shift → bandpass filter
    //
    // RTTY uses two audio tones: mark (freq1 = 2295 Hz) and space (freq0 = 2125 Hz).
    // From Thetis radio.cs:2024-2060 — rx_dolly_freq0/freq1 are stored and fed to
    // SetRXAmpeakFilFreq (the IIR audio peak filter / "dolly" filter). NereusSDR
    // does not yet implement the ampeak dolly filter (it is not in RxChannel),
    // so the mark/shift values are used to compute a bandpass window that covers
    // both tones:
    //   filterLow  = markHz − shiftHz/2 − 100
    //   filterHigh = markHz + shiftHz/2 + 100
    // The ±100 Hz guard band keeps both tones well inside the passband.
    //
    // Note: Thetis uses DIGU/DIGL DSP modes for RTTY (there is no DSPMode::RTTY
    // in WDSP — see Thetis enums.cs:252-268). The bandpass update fires whenever
    // mark or shift changes, matching Thetis setup.cs:17203 (udDSPRX1DollyF0_ValueChanged)
    // which fires unconditionally on control change.
    //
    // Full dolly-filter support (SetRXAmpeakFilFreq wiring) is deferred to a later
    // phase when the ampeak API is added to RxChannel.
    auto updateRttyFilter = [slice]() {
        const int mark  = slice->rttyMarkHz();
        const int shift = slice->rttyShiftHz();
        const int low   = mark - shift / 2 - 100;
        const int high  = mark + shift / 2 + 100;
        slice->setFilter(low, high);
    };
    connect(slice, &SliceModel::rttyMarkHzChanged,  this, updateRttyFilter);
    connect(slice, &SliceModel::rttyShiftHzChanged, this, updateRttyFilter);

    // Persistence-only wires — slice properties whose only side-effect is
    // "save the new value." Without these, changes are stored on the in-
    // memory slice but never written to AppSettings until something else
    // (a band crossing, an antenna change, etc.) happens to trigger
    // scheduleSettingsSave(). User-visible bug: tweak step (or lock, or
    // RIT, or XIT) and close the app — value reverts on next launch.
    // The dspModeChanged / filterChanged / agcModeChanged etc. handlers
    // above already call scheduleSettingsSave() as part of their main
    // job; this block covers the gaps.
    connect(slice, &SliceModel::stepHzChanged,    this, [this](int) { scheduleSettingsSave(); });
    connect(slice, &SliceModel::lockedChanged,    this, [this](bool) { scheduleSettingsSave(); });
    connect(slice, &SliceModel::ritEnabledChanged, this, [this](bool) { scheduleSettingsSave(); });
    connect(slice, &SliceModel::ritHzChanged,     this, [this](int) { scheduleSettingsSave(); });
    connect(slice, &SliceModel::xitEnabledChanged, this, [this](bool) { scheduleSettingsSave(); });
    connect(slice, &SliceModel::xitHzChanged,     this, [this](int) { scheduleSettingsSave(); });

    // XIT stored for 3M-1 (TX phase) to consume on keydown. No RX effect in 3G-10.

    // AF gain → AudioEngine volume
    connect(slice, &SliceModel::afGainChanged, this, [this](int gain) {
        m_audioEngine->setVolume(gain / 100.0f);
        scheduleSettingsSave();
    });

    // RF gain → WDSP AGC top, with bidirectional sync back to AGC-T.
    // From Thetis console.cs:50350 pattern — GetRXAAGCThresh after SetRXAAGCTop
    // Upstream inline attribution preserved verbatim (console.cs:50345):
    //   if (agc_thresh_point < -160.0) agc_thresh_point = -160.0; //[2.10.3.6]MW0LGE changed from -143
    connect(slice, &SliceModel::rfGainChanged, this, [this](int gain) {
        if (m_syncingAgc) { return; }
        RxChannel* rxCh = m_wdspEngine->rxChannel(0);
        if (rxCh) {
            m_syncingAgc = true;
            rxCh->setAgcTop(static_cast<double>(gain));
            // Read back resulting threshold and sync AGC-T display.
            double thresh = rxCh->readBackAgcThresh();
            int threshInt = static_cast<int>(std::round(thresh));
            SliceModel* s = m_activeSlice;
            if (s && s->agcThreshold() != threshInt) {
                s->setAgcThreshold(threshInt);
            }
            m_syncingAgc = false;
        }
        scheduleSettingsSave();
    });

    // Phase 3P-I-a T12 — route slice antenna writes through AlexController.
    // VFO Flag clicks land here; AlexController::setRxAnt/setTxAnt emit
    // antennaChanged(band), and T9's constructor-level connection reapplies
    // to the wire via applyAlexAntennaForBand. This makes per-band
    // persistence uniform across all UI surfaces
    // (see docs/architecture/antenna-routing-design.md §5.1).
    connect(slice, &SliceModel::rxAntennaChanged, this, [this](const QString& ant) {
        int antNum = 1;
        if (ant == QLatin1String("ANT2")) { antNum = 2; }
        else if (ant == QLatin1String("ANT3")) { antNum = 3; }
        m_alexController.setRxAnt(m_lastBand, antNum);
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::txAntennaChanged, this, [this](const QString& ant) {
        int antNum = 1;
        if (ant == QLatin1String("ANT2")) { antNum = 2; }
        else if (ant == QLatin1String("ANT3")) { antNum = 3; }
        // Note: setTxAnt respects blockTxAnt2/3 safety guards; reject is silent.
        m_alexController.setTxAnt(m_lastBand, antNum);
        scheduleSettingsSave();
    });

    // Send initial frequency to radio (after connection init completes)
    QTimer::singleShot(100, this, [this, slice]() {
        if (m_connection && m_connection->isConnected()) {
            int rxIdx = slice->receiverIndex();
            quint64 freqHz = static_cast<quint64>(slice->frequency());
            if (rxIdx >= 0) {
                m_receiverManager->setReceiverFrequency(rxIdx, freqHz);
            }
            QMetaObject::invokeMethod(m_connection, [conn = m_connection, freqHz]() {
                conn->setTxFrequency(freqHz);
            });
        }
    });
}

// Load persisted VFO state from AppSettings into a slice.
// Migrates legacy flat keys first, then restores per-band state for the
// last-used band (or, when no LastBand marker exists, falls back to the
// panadapter's center frequency band, then to the slice's default freq).
void RadioModel::loadSliceState(SliceModel* slice)
{
    if (!slice) {
        return;
    }

    // One-shot migration of legacy Vfo* flat keys. No-op if already migrated.
    SliceModel::migrateLegacyKeys();

    // Pick the band to restore. Priority order:
    //   1. Slice<N>/LastBand — written by saveToSettings on every save, so
    //      this lands on the user's actual last-used band/frequency.
    //   2. Panadapter band — only useful if the panadapter's center freq
    //      is itself restored from somewhere; today it defaults to
    //      14.225 MHz so this branch reduces to "always 20m" without (1).
    //   3. bandFromFrequency on the slice's default freq — startup fallback
    //      when neither (1) nor (2) is available (fresh install).
    Band currentBand = Band::Band20m;
    if (auto lastBand = SliceModel::loadLastBandFromSettings(slice->sliceIndex())) {
        currentBand = *lastBand;
    } else if (!m_panadapters.isEmpty()) {
        currentBand = m_panadapters.first()->band();
    } else {
        currentBand = bandFromFrequency(slice->frequency());
    }
    m_lastBand = currentBand;

    slice->restoreFromSettings(currentBand);

    // Push restored frequency to the panadapter so the spectrum display
    // lands on the same band as the slice. Without this the panadapter
    // stays parked at its 14.225 MHz default and the user sees the slice
    // jump to (say) 7.236 MHz on a panadapter still rendering 20m.
    // SpectrumWidget center freq follows from the panadapter on startup.
    if (!m_panadapters.isEmpty()) {
        m_panadapters.first()->setCenterFrequency(slice->frequency());
    }

    qCInfo(lcDsp) << "Loaded slice state for band:"
                  << bandKeyName(currentBand)
                  << SliceModel::modeName(slice->dspMode())
                  << slice->frequency() / 1e6 << "MHz"
                  << "AGC:" << static_cast<int>(slice->agcMode())
                  << "AF:" << slice->afGain() << "RF:" << slice->rfGain();
}

// Apply AlexController state to the wire. Called from three triggers:
//   T9  — AlexController::antennaChanged(band) / <flag>Changed
//   T10 — SliceModel band-crossing on current slice
//   T11 — Connection state → Connected
//
// Phase 3P-I-b (T6): full port of Thetis HPSDR/Alex.cs:310-413
// UpdateAlexAntSelection, minus MOX coupling and Aries clamp (both
// deferred to Phase 3M-1 — TX bring-up). Composition mirrors Thetis
// line-by-line with isTx branch, Ext1/Ext2OnTx mapping, xvtrActive
// gating, and rx_out_override clamp.
//
// Source: Thetis HPSDR/Alex.cs:310-413 [@501e3f5].
void RadioModel::applyAlexAntennaForBand(Band band, bool isTx)
{
    if (!m_connection || !m_connection->isConnected()) {
        qCDebug(lcConnection) << "applyAlexAntennaForBand(" << bandLabel(band)
                              << "isTx=" << isTx << ") skipped — not connected";
        return;
    }

    const BoardCapabilities& caps = boardCapabilities();

    AntennaRouting r;
    r.tx = isTx;  // Carried through for P2 MOX-aware wire reapply (3M-1 will consult).

    // From Thetis Alex.cs:312-317 [@501e3f5].
    // "if (!alex_enabled) { NetworkIO.SetAntBits(0, 0, 0, 0, false); return; }"
    if (!caps.hasAlex) {
        r.rxOnlyAnt = 0;
        r.trxAnt    = 0;
        r.txAnt     = 0;
        r.rxOut     = false;
        r.tx        = false;
        RadioConnection* conn = m_connection;
        QMetaObject::invokeMethod(conn, [conn, r]() { conn->setAntennaRouting(r); });
        return;
    }

    const int txAnt = m_alexController.txAnt(band);  // 1..3

    int  rxOnlyAnt;
    int  trxAnt;
    bool rxOut;

    if (isTx) {
        // From Thetis Alex.cs:339-347 [@501e3f5].
        if (m_alexController.ext2OutOnTx())      { rxOnlyAnt = 1; }
        else if (m_alexController.ext1OutOnTx()) { rxOnlyAnt = 2; }
        else                                      { rxOnlyAnt = 0; }

        rxOut = m_alexController.rxOutOnTx()
             || m_alexController.ext1OutOnTx()
             || m_alexController.ext2OutOnTx();

        trxAnt = txAnt;
    } else {
        // From Thetis Alex.cs:349-366 [@501e3f5].
        rxOnlyAnt = m_alexController.rxOnlyAnt(band);

        // Thetis derives `xvtr` from the current console band
        // (console.vfoa_band == Band.XVTR). Mirror that: the user is in
        // XVTR mode when the active band slot is Band::XVTR. The session
        // flag m_xvtrActive acts as a secondary override for future
        // scenarios where XVTR state isn't tied to the band enum.
        const bool xvtr = (band == Band::XVTR) || m_alexController.xvtrActive();
        if (xvtr) {
            rxOnlyAnt = (rxOnlyAnt >= 3) ? 3 : 0;
        } else if (rxOnlyAnt >= 3) {
            // "do not use XVTR ant port if not using transverter" — Alex.cs:358
            rxOnlyAnt -= 3;
        }

        rxOut = (rxOnlyAnt != 0);

        trxAnt = m_alexController.useTxAntForRx()
                   ? txAnt
                   : m_alexController.rxAnt(band);
    }

    // From Thetis Alex.cs:368-375 rx_out_override [@501e3f5].
    //G8NJJ  [Aries block adjacency — Thetis Alex.cs:376 "G8NJJ support for external Aries ATU"]
    if (m_alexController.rxOutOverride() && rxOut) {
        if (!isTx) {
            trxAnt = 4;  // Special RX-override — trx_ant=4 signals the wire layer to bypass.
        }
        if (isTx) {
            rxOut = m_alexController.rxOutOnTx()
                 || m_alexController.ext1OutOnTx()
                 || m_alexController.ext2OutOnTx();
        } else {
            rxOut = false;  // "disable Rx_Bypass_Out relay" — Alex.cs:374
        }
    }

    // MOX-coupled reapply + Aries clamp — deferred to Phase 3M-1.
    // From Thetis Alex.cs:381-382 [@501e3f5] (reference):
    //   if ((trx_ant != 4) && (LimitTXRXAntenna == true)) trx_ant = 1;
    //G8NJJ
    //
    // MW0LGE_21k9d only set bits if different — Alex.cs:394-413
    // (deduplication guard) also deferred: NereusSDR connection layer
    // can suppress redundant wire writes if needed, but we always compose
    // here for correctness.

    r.rxOnlyAnt = rxOnlyAnt;
    r.trxAnt    = trxAnt;
    r.txAnt     = txAnt;
    r.rxOut     = rxOut;

    qCDebug(lcConnection) << "applyAlexAntennaForBand(" << bandLabel(band)
                          << "isTx=" << isTx << ") → rxOnly=" << r.rxOnlyAnt
                          << "trxAnt=" << r.trxAnt << "txAnt=" << r.txAnt
                          << "rxOut=" << r.rxOut;

    // Marshal to connection worker thread — mirrors existing pattern
    // used by e.g. setReceiverFrequency.
    RadioConnection* conn = m_connection;
    QMetaObject::invokeMethod(conn, [conn, r]() {
        conn->setAntennaRouting(r);
    });
}

// Coalesce settings saves to avoid writing on every scroll tick.
void RadioModel::scheduleSettingsSave()
{
    if (m_settingsSaveScheduled) {
        return;
    }
    m_settingsSaveScheduled = true;
    QTimer::singleShot(500, this, [this]() {
        m_settingsSaveScheduled = false;
        saveSliceState(m_activeSlice);
    });
}

// Force-run any pending coalesced slice save synchronously. Without this,
// the 500 ms QTimer in scheduleSettingsSave() can't fire while the main
// thread is inside MainWindow::closeEvent → teardownConnection (synchronous,
// blocks on QThread::wait calls), so the user's last AF / step / freq /
// lock / RIT change before close gets dropped on the floor. The pending
// QTimer is left in place; if it fires after this it will redundantly
// re-save the same state, which is harmless.
void RadioModel::flushPendingSettingsSave()
{
    if (!m_settingsSaveScheduled) {
        return;
    }
    m_settingsSaveScheduled = false;
    saveSliceState(m_activeSlice);
}

// Persist current slice state to AppSettings (per-band + session state).
// Also flushes AlexController persistence if the dirty flag was set —
// see the antennaChanged / blockTxChanged handlers in wireSliceSignals.
void RadioModel::saveSliceState(SliceModel* slice)
{
    if (slice) {
        slice->saveToSettings(m_lastBand);
    }

    // Flush AlexController if any per-band antenna or block-TX toggle
    // changed since the last save. save() no-ops when MAC is empty,
    // so pre-connect dirty flags are silently dropped — that's fine
    // because load() hasn't run yet either.
    if (m_alexControllerDirty) {
        m_alexController.save();
        m_alexControllerDirty = false;
    }

    // Flush per-band tune power on every slice save (matches AlexController
    // cadence; save() no-ops when MAC is empty, so pre-connect calls are
    // harmless).
    // Phase 3M-1a G.3. Source: Thetis console.cs:3087-3091 [v2.10.3.13].
    m_transmitModel.save();
}

void RadioModel::teardownConnection()
{
    if (!m_connection) {
        return;
    }

    // Flush any pending coalesced slice save FIRST so the user's last
    // AF / step / freq / lock / RIT tweak isn't lost to the 500 ms
    // debounce in scheduleSettingsSave(). The QTimer there can't fire
    // while teardown is running on the main thread, so without this an
    // immediate close-after-tweak silently drops the change. Cheap and
    // idempotent — no-op when nothing's pending.
    flushPendingSettingsSave();

    // 3M-1a G.1 fixup: drop any prior WdspEngine::initializedChanged subscribers
    // we registered in connectToRadio(). Without this, each reconnect cycle
    // accumulates another copy of the WDSP-init lambda, causing duplicate
    // createRxChannel + createTxChannel(1) calls on the next initializedChanged.
    // Qt::UniqueConnection can't be used with lambdas, so we disconnect by hand
    // here, on the matching teardown path.
    if (m_wdspEngine != nullptr) {
        disconnect(m_wdspEngine, &WdspEngine::initializedChanged, this, nullptr);
    }

    // Flush any pending AlexController writes before the MAC-scoped
    // keys become unreachable (save() keys off m_mac, which stays set
    // across disconnect, but a crash between disconnect and next save
    // would lose the change). Cheap insurance.
    if (m_alexControllerDirty) {
        m_alexController.save();
        m_alexControllerDirty = false;
    }

    // Flush per-band tune power on disconnect.
    // Phase 3M-1a G.3. Source: Thetis console.cs:3087-3091 [v2.10.3.13].
    m_transmitModel.save();

    // Flush mic/VOX/MON properties on disconnect (defense-in-depth;
    // auto-persist should have flushed each change already).
    // Phase 3M-1b L.2.
    if (!m_lastRadioInfo.macAddress.isEmpty()) {
        m_transmitModel.persistToSettings(m_lastRadioInfo.macAddress);
    }

    // L.3: Release the HL2 mic-source lock on disconnect.
    // A subsequent connectToRadio() to a non-HL2 radio must be free to use
    // MicSource::Radio if the user selects it.  The lock is re-engaged
    // (or not) by the next connectToRadio() call based on the new radio's
    // BoardCapabilities::hasMicJack.
    m_transmitModel.setMicSourceLocked(false);

    // Disconnect signals into the DSP worker first so no new I/Q
    // batches can be posted onto the worker thread, then quit and
    // join that thread before touching WDSP. The worker is queued
    // for deletion via QThread::finished (see wireConnectionSignals),
    // so the m_dspWorker pointer may dangle after wait() returns —
    // null it out to avoid a use-after-free in any later teardown.
    if (m_dspWorker != nullptr) {
        QObject::disconnect(m_receiverManager, nullptr, m_dspWorker, nullptr);
    }
    if (m_dspThread != nullptr) {
        m_dspThread->quit();
        m_dspThread->wait();
        delete m_dspThread;
        m_dspThread = nullptr;
        m_dspWorker = nullptr;
    }

    // Stop audio output
    m_audioEngine->stop();

    // 3M-1c TX pump architecture redesign — TxWorkerThread teardown.
    //
    // ORDER MATTERS:
    //   1. stopPump() — quits the worker's event loop and waits for
    //      its QTimer/onPumpTick to finish.  Any in-flight tick
    //      completes before exit.
    //   2. Move TxChannel back to RadioModel's thread (main).  Required
    //      so TxChannel's destruction (via WdspEngine::shutdown →
    //      destroyTxChannel(1)) runs on the right thread; Qt asserts
    //      otherwise.
    //   3. unique_ptr.reset() — destroys the TxWorkerThread itself.
    //
    // Replaces the deleted L.4 MicReBlocker teardown.  See plan §5.2.
    if (m_txWorker) {
        // stopPump() internally calls m_txMicSource->stop() (the poison
        // semaphore release that breaks the worker out of waitForBlock).
        m_txWorker->stopPump();
        if (m_txChannel) {
            m_txChannel->moveToThread(this->thread());
        }
        m_txWorker.reset();
    }
    // Phase 3M-1c TX pump v3: drop the connection's view of the mic
    // source BEFORE destroying it.  Otherwise the next inbound mic
    // frame would dereference a freed TxMicSource.
    //
    // Codex P1 fix (PR #152): `setTxMicSource` is a connection-thread
    // operation (per the I3 caller-contract comment in P1/P2
    // RadioConnection::setTxMicSource — race-free with the connection-
    // thread reads in onReadyRead / onWatchdogTick / decodeMicFrame132
    // ONLY when invoked on the connection's affinity thread).  At
    // teardown, the connection has long since been moveToThread'd to
    // m_connThread (RadioModel.cpp:1842), so we must marshal the call
    // there.  BlockingQueuedConnection ensures the detach completes
    // before we proceed to `m_txMicSource->stop() + reset()` below —
    // without blocking, a queued lambda would still hold a TxMicSource*
    // when we destroy the source object.
    if (m_connection != nullptr) {
        auto* const conn = m_connection;
        auto detachMicSource = [conn]() {
            if (auto* p1 = qobject_cast<P1RadioConnection*>(conn)) {
                p1->setTxMicSource(nullptr);
            } else if (auto* p2 = qobject_cast<P2RadioConnection*>(conn)) {
                p2->setTxMicSource(nullptr);
            }
        };
        if (conn->thread() == QThread::currentThread()) {
            // Same-thread fast path — direct call.  This branch fires
            // when teardownConnection is itself running on the connection
            // thread (no production callsite today, but the guard is
            // cheap and keeps the contract explicit).
            detachMicSource();
        } else {
            // Cross-thread — block until the lambda runs on the connection
            // thread, then return.  Qt requires sender ≠ receiver thread
            // for BlockingQueuedConnection (asserts otherwise); the
            // currentThread check above guarantees this precondition.
            QMetaObject::invokeMethod(conn, detachMicSource,
                                      Qt::BlockingQueuedConnection);
        }
    }
    if (m_txMicSource) {
        m_txMicSource->stop();
        m_txMicSource.reset();
    }

    // 3M-1c L.2: drop the TwoToneController's view of the TX channel.  If a
    // user-driven setActive(true) call were to fire during teardown (mid-test
    // reconnect), the controller's null-check in setActive() short-circuits
    // safely.  Same pattern as TxChannel::setConnection(nullptr) below.
    if (m_twoToneController) {
        m_twoToneController->setTxChannel(nullptr);
        m_twoToneController->setSliceModel(nullptr);
        m_twoToneController->setPowerOn(false);
        // If a two-tone test is currently running, force it off so the
        // restored MOX-release doesn't hold over the disconnect.
        if (m_twoToneController->isActive()) {
            m_twoToneController->setActive(false);
        }
    }

    // 3M-1c L.1: drop the per-MAC scope on the profile manager so subsequent
    // mutators silently no-op until the next connectToRadio() sets a new MAC.
    if (m_micProfileMgr) {
        m_micProfileMgr->setMacAddress(QString());
    }

    // P1 full-parity §3.2: reset PA forward-power cal profile to None so a
    // subsequent connect to a different SKU (under the same MAC, or a fresh
    // MAC) gets the right `PaCalProfile::defaults(class)` applied. Without
    // this, an Anan100 profile loaded for a previous radio would survive
    // into a connect to e.g. Anan10 hardware before the next load().
    // Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13]
    m_calController.setPaCalProfile(PaCalProfile{});

    // 3M-1a G.1: detach the production loop pointers before clearing m_txChannel.
    // setConnection(nullptr) stops driveOneTxBlock() from calling sendTxIq on
    // a destroyed connection; setMicRouter(nullptr) drops the TxMicRouter ref.
    // The production timer is stopped by setRunning(false) (MoxController
    // txaFlushed path), but guard here in case TX was still active at teardown.
    if (m_txChannel) {
        m_txChannel->setConnection(nullptr);
        m_txChannel->setMicRouter(nullptr);
    }

    // 3M-1b L.1: K.2 carry-forward — uninstall the MoxCheck callback before
    // the closure's captured state (m_slices, m_bandPlan) is potentially invalid.
    // Passing an empty std::function clears the stored callback in MoxController.
    if (m_moxController) {
        m_moxController->setMoxCheck({});
    }

    // 3M-1b L.1: destroy mic-source strategy objects in reverse-construction order
    // so CompositeTxMicRouter (which holds raw pointers to pc + radio sources)
    // is released BEFORE the sources it points into.
    // After reset(), pullSamples() on the composite is unreachable (TxChannel
    // already has setMicRouter(nullptr) above).
    m_compositeMicRouter.reset();
    m_radioMicSource.reset();
    m_pcMicSource.reset();

    // Clear the non-owning TX channel view before WdspEngine::shutdown()
    // destroys the underlying WDSP channel. Any in-flight txReady / txaFlushed
    // slot calls are queued and will see m_txChannel == nullptr after this clear.
    // WdspEngine::shutdown() → destroyTxChannel(1) handles the actual WDSP teardown.
    m_txChannel = nullptr;

    // Shutdown WDSP (destroys all channels, saves cache)
    m_wdspEngine->shutdown();

    // Disconnect remaining signals (prevents new work being queued)
    QObject::disconnect(m_connection, nullptr, this, nullptr);
    QObject::disconnect(m_connection, nullptr, m_receiverManager, nullptr);
    QObject::disconnect(m_receiverManager, nullptr, this, nullptr);

    // Drop all logical receivers so the next connectToRadio() starts from
    // index 0 with a fresh wdspChannel counter. Without this, issue #75:
    // receiver 0 leaks into the next session, createReceiver() returns 1,
    // and on P2 2-ADC boards both receivers claim DDC2 — the collision in
    // rebuildHardwareMapping routes DDC2 I/Q to logical 1 whose wdspChannel
    // is 1, but only WDSP channel 0 is created in connectToRadio, so audio
    // and spectrum silently drop on the second connect.
    m_receiverManager->reset();

    // Tear down the connection on its own worker thread via the shared
    // helper. See src/core/RadioConnectionTeardown.h for why this must
    // run on the worker — short version: the RadioConnection's QTimers
    // are thread-affined to the worker and destroying them on any other
    // thread emits cross-thread warnings and can crash on Windows.
    teardownWorkerThreadedConnection(m_connection, m_connThread);

    // Phase 3Q polish: above disconnect() severed connectionStateChanged
    // before the RadioConnection's own setState(Disconnected) ran, so the
    // model's state machine never sees the transition and sticks at
    // Connected. Force it here so the panel strip + TitleBar + bottom
    // status bar all flip to Disconnected after a Radio→Disconnect.
    setConnectionState(ConnectionState::Disconnected);
}

// Phase 3G-9b — 7 smooth-default recipe values. See docs/architecture/waterfall-tuning.md.
void RadioModel::applyClaritySmoothDefaults()
{
    SpectrumWidget* sw = spectrumWidget();
    if (!sw) { return; }  // not yet wired by MainWindow — Task 3 re-invokes

    // 1. Palette — narrow-band monochrome. See docs/architecture/waterfall-tuning.md §1.
    sw->setWfColorScheme(WfColorScheme::ClarityBlue);

    // 2. Spectrum averaging mode — log-recursive for heavy smoothing.
    sw->setAverageMode(AverageMode::Logarithmic);

    // 3. Averaging alpha — very slow exponential (~500 ms perceived smoothing
    //    at 30 FPS). See waterfall-tuning.md §3.
    sw->setAverageAlpha(0.05f);

    // 4. Trace colour — pure white, thin, sits cleanly in front of the
    //    waterfall without competing. Visual target: 2026-04-14 reference.
    sw->setFillColor(QColor(0xff, 0xff, 0xff, 230));

    // 5. Pan fill OFF — trace renders as a thin line, not a filled curve.
    //    NereusSDR's default is fill-on; turn it off to match the reference.
    sw->setPanFillEnabled(false);

    // 6. Waterfall AGC — tracks band conditions automatically. With AGC on,
    sw->setWfAgcEnabled(true);

    // 7. Waterfall update period — 30 ms for smooth scroll motion.
    sw->setWfUpdatePeriodMs(30);

    // Mark the profile as applied so the gate short-circuits on next launch.
    AppSettings::instance().setValue(
        QStringLiteral("DisplayProfileApplied"),
        QStringLiteral("True"));
}

void RadioModel::setConnectionState(ConnectionState s)
{
    if (m_connectionState == s) {
        return;
    }
    m_connectionState = s;
    // Phase 3Q sub-PR-3: track when we become connected so
    // connectionUptimeText() can produce a human-readable elapsed time.
    if (s == ConnectionState::Connected) {
        m_connectionStartedAt = QDateTime::currentDateTime();
    } else {
        m_connectionStartedAt = QDateTime{}; // clear — uptime is meaningless
        m_connectionSampleRateHz = 0;
    }
    emit connectionStateChanged(s);
}

void RadioModel::onConnectionStateChanged(ConnectionState state)
{
    // Phase 3Q-1: route through setConnectionState() so m_connectionState
    // stays in sync and the signal carries the new state value.
    setConnectionState(state);

    switch (state) {
    case ConnectionState::Connected:
        qCDebug(lcConnection) << "Connected to" << m_name;
        // Phase 3Q Task 10: auto-connect succeeded — disarm the in-progress
        // flag so a later user-initiated Connect does not trip the failure path.
        if (m_autoConnectInProgress) {
            m_autoConnectInProgress = false;
            m_autoConnectChosenMac.clear();
        }
        // ── 3M-1c Phase L.2: TwoToneController power-on gate ─────────────────
        // The controller's setActive(true) refuses to engage unless powerOn
        // is true (mirrors !console.PowerOn at setup.cs:11063 [v2.10.3.13]).
        // Set on Connected, cleared on Disconnected / Error below.
        if (m_twoToneController) {
            m_twoToneController->setPowerOn(true);
        }
        // Phase 3I Task 17 — record the most recently used radio so
        // tryAutoReconnect() targets the right entry on next launch.
        if (!m_lastRadioInfo.macAddress.isEmpty()) {
            AppSettings& s = AppSettings::instance();
            s.setLastConnected(m_lastRadioInfo.macAddress);
            s.save();
            // Exempt this MAC from discovery stale-removal — once the
            // radio is streaming it stops replying to broadcasts.
            m_discovery->setConnectedMac(m_lastRadioInfo.macAddress);
        }
        // Phase 3P-H Task 4: validate persisted per-MAC settings against the
        // connected board's BoardCapabilities. Any clamp warnings, mismatch
        // alerts, or accessory mis-configurations populate
        // SettingsHygiene::issues() and surface on the Diagnostics →
        // Settings Validation sub-tab (built in Phase H Task 3).
        if (!m_lastRadioInfo.macAddress.isEmpty()) {
            m_settingsHygiene.validate(m_lastRadioInfo.macAddress, boardCapabilities());
        }
        // Phase 3I — fan out to HardwarePage so its sub-tabs populate with
        // the connected radio's fields (Radio Info labels, sample rate,
        // capability-gated tab visibility, per-MAC settings restore).
        emit currentRadioChanged(m_lastRadioInfo);
        // Phase 3P-I-a T11 — apply persisted per-band Alex antenna to the
        // fresh connection. Matches Thetis's initial UpdateAlexAntSelection
        // call path on radio startup (HPSDR/Alex.cs:310 [@501e3f5]).
        applyAlexAntennaForBand(m_lastBand);
        break;
    case ConnectionState::Disconnected:
        qCDebug(lcConnection) << "Disconnected from" << m_name;
        m_discovery->clearConnectedMac();
        // 3M-1c L.2: drop the TwoToneController power-on gate so any
        // subsequent setActive(true) is refused with a qCWarning until
        // the next Connected transition.
        if (m_twoToneController) {
            m_twoToneController->setPowerOn(false);
        }
        break;
    case ConnectionState::Connecting:
        qCDebug(lcConnection) << "Connecting to" << m_name << "...";
        break;
    case ConnectionState::Probing:
        qCDebug(lcConnection) << "Probing for" << m_name << "...";
        break;
    case ConnectionState::LinkLost:
        qCWarning(lcConnection) << "Link lost to" << m_name;
        m_discovery->clearConnectedMac();
        // 3M-1c L.2: same as Disconnected — drop the power-on gate.
        if (m_twoToneController) {
            m_twoToneController->setPowerOn(false);
        }
        break;
    }
}

// ── Phase 3M-0 Task 6: Ganymede PA-trip live state ──────────────────────────
// Porting from Thetis Andromeda/Andromeda.cs:914-948 [v2.10.3.13]
// (CATHandleAmplifierTripMessage + GanymedeResetPressed).
// G8NJJ: handlers for Ganymede 500W PA protection

// From Thetis Andromeda/Andromeda.cs:915-920 [v2.10.3.13]:
//   public void CATHandleAmplifierTripMessage(int TripState)
//   {
//       GanymedePresent = true;
//       _ganymede_pa_issue = TripState != 0; // this will also prevent MOX being re-enabled
//       if (_ganymede_pa_issue && MOX) MOX = false; //if there is a fault, undo mox if active
//   ...
// G8NJJ: handlers for Ganymede 500W PA protection
void RadioModel::handleGanymedeTrip(int tripState)
{
    // From Thetis Andromeda/Andromeda.cs:917 [v2.10.3.13]: GanymedePresent = true;
    m_ganymedePresent = true;

    // From Thetis Andromeda/Andromeda.cs:919 [v2.10.3.13]:
    //   _ganymede_pa_issue = TripState != 0; // this will also prevent MOX being re-enabled
    const bool newTripped = (tripState != 0);

    // From Thetis Andromeda/Andromeda.cs:920 [v2.10.3.13]:
    //   if (_ganymede_pa_issue && MOX) MOX = false; //if there is a fault, undo mox if active
    // G8NJJ: handlers for Ganymede 500W PA protection
    //
    // Codex P2 follow-up to PR #139: drop MOX on every asserted trip, even
    // when m_paTripped is already true. Otherwise a user manually re-keying
    // mid-fault would stay on TX after the next CAT trip message, because
    // the idempotent return below would skip the setMox(false) call.
    if (newTripped && m_transmitModel.isMox()) {
        m_transmitModel.setMox(false);
    }

    if (newTripped == m_paTripped) {
        return; // already in this trip state — no transition signal
    }

    m_paTripped = newTripped;
    emit paTrippedChanged(newTripped);
}

// From Thetis Andromeda/Andromeda.cs:950-968 [v2.10.3.13] (GanymedeResetPressed).
// G8NJJ: handlers for Ganymede 500W PA protection
void RadioModel::resetGanymedePa()
{
    if (!m_paTripped) {
        return; // already clear — idempotent
    }
    m_paTripped = false;
    emit paTrippedChanged(false);
}

// From Thetis Andromeda/Andromeda.cs:855-866 [v2.10.3.13] (GanymedePresent property setter):
//   if (!_ganymedePresent)
//   {
//       _ganymede_pa_issue = false;
//       PAStatusIndicator = PAstatusIndicatorState.NotUsed;
//   }
// G8NJJ: handlers for Ganymede 500W PA protection
void RadioModel::setGanymedePresent(bool present)
{
    m_ganymedePresent = present;

    // From Thetis Andromeda/Andromeda.cs:861-863 [v2.10.3.13]:
    //   if (!_ganymedePresent) { _ganymede_pa_issue = false; ... }
    if (!present && m_paTripped) {
        m_paTripped = false;
        emit paTrippedChanged(false);
    }
}

// ── Phase 3M-1a Task F.1: MoxController::hardwareFlipped fan-out ────────────
// Fans out hardware-flip side-effects in Thetis HdwMOXChanged step order.
// Pre-code review §2.3 (3M-1a-relevant subset):
//   Step 8  — Alex antenna routing (Thetis console.cs HdwMOXChanged step 8 [v2.10.3.13])
//   Step 12 — MOX wire bit  (P1: C0 byte 3 bit 0; P2: high-priority byte 4 bit 1)
//   Step 10 — T/R relay wire bit (P1: bank-10 C3 bit 7, active-low INVERTED)
//
// Note: pre-code review §2.5 maps HdwMOXChanged body to this slot.
// The connect() of MoxController::hardwareFlipped → this slot is G.1's job.
// G.1 MUST use Qt::QueuedConnection — see declaration in RadioModel.h.
// ── 3M-1a G.4: RadioModel::setTune ─────────────────────────────────────────
//
// Orchestrator for the TUNE function side-effects.
//
// Porting from Thetis console.cs:29978-30157 [v2.10.3.13] — chkTUN_CheckedChanged.
// This method ports the non-MoxController side-effects (see MoxController::setTune
// for the flag-management and MOX-state-machine portion, B.5).
//
// Inline attribution from Thetis:
//   //MW0LGE_21k9d  [original inline comment from console.cs:29980]
//   //MW0LGE_21a    [original inline comment from console.cs:29997]
//   //MW0LGE_22b    [original inline comment from console.cs:30033]
//   //MW0LGE_21k8   [original inline comment from console.cs:30086]
//   //MW0LGE_21j    [original inline comment from console.cs:30136]
//
// LSB-family helper: used for sign-selecting the tune-tone frequency.
// Cite: console.cs:30024-30037 [v2.10.3.13] — switch on Audio.TXDSPMode.
//   LSB, CWL, DIGL → -cw_pitch (negative side of baseband).
//   All others     → +cw_pitch (positive side of baseband).
static bool isLsbFamily(DSPMode mode) noexcept
{
    // From Thetis console.cs:30024-30037 [v2.10.3.13]:
    //   case DSPMode.LSB:
    //   case DSPMode.CWL:
    //   case DSPMode.DIGL:
    //       radio.GetDSPTX(0).TXPostGenToneFreq = -cw_pitch;
    return mode == DSPMode::LSB || mode == DSPMode::CWL || mode == DSPMode::DIGL;
}

void RadioModel::setTune(bool on)
{
    // Porting from Thetis console.cs:29978-30157 [v2.10.3.13] — chkTUN_CheckedChanged.
    //
    // 3M-1a scope: all side-effects listed in pre-code review §3.2/§3.3 except:
    //   - 2-TONE pre-stop (3M-3a)
    //   - _tune_pulse_enabled path (3M-3a)
    //   - SetPowerUsingTargetDBM full dBm-target logic (3M-3a)
    //   - ATU async tune, NetworkIO.SetUserOut*, Apollo auto-tune (deferred)
    //   - UI BackColor changes (H.3 territory)
    //   - Meter TX mode lock/restore: NereusSDR's MeterModel has no TX-mode
    //     selector yet; this is deferred to H.3 / 3M-1b when MeterModel gains
    //     a setTxDisplayMode() setter.  The save/restore slots remain below
    //     as named comments so the H.3 author knows exactly where to plug in.

    if (on) {
        // ── Power-on guard ─────────────────────────────────────────────────────
        // Cite: console.cs:29983-29991 [v2.10.3.13].
        // Thetis: "if (!PowerOn) { MessageBox.Show(...); chkTUN.Checked = false; return; }"
        // NereusSDR: PowerOn ≈ "radio connected and audio engine running".
        // Guard: if not connected, emit tuneRefused and bail out.  m_audioEngine
        // null-check mirrors Thetis's PowerOn check (power-on requires the audio
        // engine to be live, which presupposes a live connection).
        if (!isConnected() || !m_audioEngine) {
            emit tuneRefused(QStringLiteral("Power must be on to enable Tune."));
            return;
        }

        // 3M-1a G.4 fixup: set m_isTuning EARLY, matching Thetis console.cs:30010
        // [v2.10.3.13] "_tuning = true;" which precedes the tone-freq switch
        // (30022) and the PreviousPWR save (30043).  Functionally inconsequential
        // in 3M-1a (no subscriber reads m_isTuning during TUN-on setup), but
        // matches Thetis ordering for future maintainers reading side-by-side.
        m_isTuning = true;

        // ── SAVE meter mode ────────────────────────────────────────────────────
        // Cite: console.cs:30011 [v2.10.3.13]:
        //   old_meter_tx_mode_before_tune = current_meter_tx_mode;
        // NereusSDR: MeterModel does not expose a TX display-mode enum yet
        // (deferred to H.3).  No-op placeholder; the variable is declared as a
        // comment token so H.3 can fill in the real API when it exists.
        // [H.3 hook: save meterModel().txDisplayMode() here]

        // ── SWITCH to POWER meter mode ─────────────────────────────────────────
        // Cite: console.cs:30012-30015 [v2.10.3.13]:
        //   if (current_meter_tx_mode != tune_meter_tx_mode) CurrentMeterTXMode = tune_meter_tx_mode;
        //   tune_meter_tx_mode = MeterTXMode.FORWARD_POWER (console.cs:11861).
        // NereusSDR: deferred to H.3 (no MeterModel setTxDisplayMode() yet).
        // [H.3 hook: meterModel().setTxDisplayMode(MeterTxMode::ForwardPower) here]

        // ── SAVE current DSP mode ──────────────────────────────────────────────
        // Cite: console.cs:30042 [v2.10.3.13]:
        //   old_dsp_mode = radio.GetDSPTX(0).CurrentDSPMode;
        m_savedTxDspMode = m_activeSlice ? m_activeSlice->dspMode() : DSPMode::USB;

        // ── SAVE power slider value ────────────────────────────────────────────
        // Cite: console.cs:30033 [v2.10.3.13]: PreviousPWR = ptbPWR.Value;
        //   //MW0LGE_22b  [original inline comment from console.cs:30033]
        m_savedPowerPct = m_transmitModel.power();

        // ── COMPUTE tune-tone frequency (sign-selected by current DSP mode) ────
        // Cite: console.cs:30024-30037 [v2.10.3.13] — switch on Audio.TXDSPMode.
        //   NB: in Thetis the tone-freq switch runs BEFORE the CW→LSB/USB swap,
        //   so the sign is based on the ORIGINAL mode (CWL → negative, CWU → positive).
        //   Because CWL is LSB-family and CWU is not, the pre-swap mode determines
        //   the correct sideband. This ordering is preserved here.
        //
        // cw_pitch: from Thetis console.cs:18182 [v2.10.3.13] — private int cw_pitch = 600;
        static constexpr double kCwPitch = 600.0;
        const DSPMode modeBeforeSwap = m_savedTxDspMode;
        const double signedFreq = isLsbFamily(modeBeforeSwap) ? -kCwPitch : +kCwPitch;

        // ── SET TUNE TONE ──────────────────────────────────────────────────────
        // Cite: console.cs:30038-30040 [v2.10.3.13]:
        //   radio.GetDSPTX(0).TXPostGenMode = 0;
        //   radio.GetDSPTX(0).TXPostGenToneMag = MAX_TONE_MAG;
        //   radio.GetDSPTX(0).TXPostGenRun = 1;
        if (m_txChannel) {
            m_txChannel->setTuneTone(true, signedFreq, TxChannel::kMaxToneMag);
        }

        // ── CW→LSB/USB DSP MODE SWAP ───────────────────────────────────────────
        // Cite: console.cs:30043-30070 [v2.10.3.13]:
        //   switch (old_dsp_mode) { case CWL: ... TXDSPMode = LSB; break;
        //                            case CWU: ... TXDSPMode = USB; break; }
        if (m_activeSlice) {
            DSPMode swappedMode = m_savedTxDspMode;
            switch (m_savedTxDspMode) {
                case DSPMode::CWL:
                    swappedMode = DSPMode::LSB;
                    break;
                case DSPMode::CWU:
                    swappedMode = DSPMode::USB;
                    break;
                default:
                    break;  // no swap for SSB/AM/FM/DIGU/DIGL/etc.
            }
            if (swappedMode != m_savedTxDspMode) {
                m_activeSlice->setDspMode(swappedMode);
            }
        }

        // ── PUSH TUNE POWER ────────────────────────────────────────────────────
        // Cite: console.cs:30033-30037 [v2.10.3.13]:
        //   PreviousPWR = ptbPWR.Value;  //MW0LGE_22b
        //   int new_pwr = SetPowerUsingTargetDBM(..., true, true, false);
        //   if (_tuneDrivePowerSource == DrivePowerSource.FIXED) PWR = new_pwr;
        //
        // 3M-1a: uses TransmitModel::tunePowerForBand() directly.
        // Full SetPowerUsingTargetDBM dBm-target logic deferred to 3M-3a.
        const Band currentBand = m_activeSlice
                                    ? bandFromFrequency(m_activeSlice->frequency())
                                    : m_lastBand;
        const int tunePower = m_transmitModel.tunePowerForBand(currentBand);
        // Scale percent (0-100) → wire byte (0-255) with SWR-protection
        // foldback. See setTxDrive scaling comment in connection-power
        // lambda above for the mi0bot NetworkIO.cs:209-211 [v2.10.3.14-beta1]
        // citation. Default factor=1.0 → identity to prior formula.
        const float fTune          = std::clamp(tunePower / 100.0f, 0.0f, 1.0f);
        const float swrProtectTune =
            std::clamp(m_transmitModel.swrProtectFactor(), 0.0f, 1.0f);
        const int wireTuneDrive =
            std::clamp(int(255.0f * fTune * swrProtectTune), 0, 255);
        if (m_connection) {
            auto* conn = m_connection;
            QMetaObject::invokeMethod(conn, [conn, wireTuneDrive]() {
                conn->setTxDrive(wireTuneDrive);
            });
        }

        // ── PUSH TUNE-ADJUSTED TX VFO (carrier-on-dial) ────────────────────────
        // Thetis offsets the TX VFO by ±cw_pitch when TUNE is on so the
        // resulting carrier (TX_VFO + audio_tone_freq) lands exactly on dial
        // freq, not at dial±cw_pitch.
        //
        // From Thetis ChannelMaster/console.cs:31788-31810 [v2.10.3.13]:
        //   case DSPMode.USB / DIGU / DSB:
        //     if (chkTUN.Checked) tx_freq -= cw_pitch * 1e-6;
        //   case DSPMode.LSB / DIGL:
        //     if (chkTUN.Checked) tx_freq += cw_pitch * 1e-6;
        //   case DSPMode.AM / SAM / FM:
        //     if (chkTUN.Checked) tx_freq -= cw_pitch * 1e-6;
        //
        // Equivalent formula: TX_VFO = dial − signedFreq, where signedFreq is
        // the audio-rate tune tone frequency we passed to setTuneTone above:
        //   USB/DIGU/CWU/AM/SAM/FM/DSB → signedFreq = +cw_pitch → TX_VFO = dial − cw_pitch
        //   LSB/DIGL/CWL              → signedFreq = −cw_pitch → TX_VFO = dial + cw_pitch
        // After the radio's TX DDC mixes audio tone onto TX_VFO, the carrier
        // at RF = TX_VFO + signedFreq = dial.
        if (m_activeSlice && m_connection) {
            const quint64 dialHz =
                static_cast<quint64>(m_activeSlice->frequency());
            const qint64 adjustedTxHz =
                static_cast<qint64>(dialHz) - static_cast<qint64>(signedFreq);
            const quint64 wireHz =
                (adjustedTxHz < 0) ? 0 : static_cast<quint64>(adjustedTxHz);
            auto* conn = m_connection;
            QMetaObject::invokeMethod(conn, [conn, wireHz]() {
                conn->setTxFrequency(wireHz);
            });
        }

        // ── WIRE SWR PROTECTION TO LIVE TUNE POWER (F.3 final wiring) ──────────
        // F.3 ported two SwrProtectionController setters; both must be called
        // before MOX engages so the SWR controller's tune-bypass + alex_fwd
        // floor use the correct values during the impending TX.
        //
        // Cite: console.cs:26020-26057 [v2.10.3.13] — tunePowerSliderValue
        //   determines the tune-bypass condition (≤70 enables bypass).
        // Cite: console.cs:26064-26067 [v2.10.3.13] — alex_fwd_limit defaults
        //   to 5.0f, with ANAN-8000D scaling as 2.0 × ptbPWR.Value:
        //     float alex_fwd_limit = 5.0f;
        //     if (HardwareSpecific.Model == HPSDRModel.ANAN8000D)        // K2UE idea: try to determine if Hi-Z or Lo-Z load
        //         alex_fwd_limit = 2.0f * (float)ptbPWR.Value;        //    by comparing alex_fwd with power setting
        m_swrProt.setTunePowerSliderValue(tunePower);
        const float alexFwdLimit =
            (m_hardwareProfile.model == HPSDRModel::ANAN8000D)
                ? 2.0f * static_cast<float>(tunePower)
                : 5.0f;
        m_swrProt.setAlexFwdLimit(alexFwdLimit);

        // ── ENGAGE MOX via MoxController ─────────────────────────────────────
        // Cite: console.cs:30081 [v2.10.3.13]: chkMOX.Checked = true;
        //   //MW0LGE_21k8  [original inline comment from console.cs:30086]
        // MoxController::setTune(true) drives the full state machine and sets
        // _manual_mox + _current_ptt_mode = PTTMode.MANUAL (B.5).
        // Note: m_isTuning = true was moved earlier (after power-on guard) to
        // match Thetis console.cs:30010 [v2.10.3.13] ordering (G.4 fixup).
        if (m_moxController) {
            m_moxController->setTune(true);
        }

    } else {
        // ── TUN OFF path ───────────────────────────────────────────────────────

        // 3M-1a G.4 fixup: idempotent guard against double-off and cold-off.
        // Without this guard, a setTune(false) called before any setTune(true)
        // would restore m_savedPowerPct (default 100) over the user's actual
        // power setting, stomping whatever real-time value the TransmitModel holds.
        // Also matches Thetis behavior: chkTUN_CheckedChanged only runs the
        // TUN-off branch when chkTUN was checked (i.e. _tuning was true).
        // Cite: Thetis console.cs:29978 [v2.10.3.13] — if (e.NewValue == Enabled) { ... } else { ... }
        //   //MW0LGE_21k9d  [original inline comment from console.cs:29980]
        if (!m_isTuning) {
            return;
        }

        // ── RELEASE MOX via MoxController ────────────────────────────────────
        // Cite: console.cs:30105 [v2.10.3.13]: chkMOX.Checked = false;
        // MoxController::setTune(false) drives the full TX→RX walk (B.5).
        if (m_moxController) {
            m_moxController->setTune(false);
        }

        // ── RELEASE TUNE TONE ──────────────────────────────────────────────────
        // Cite: console.cs:30109 [v2.10.3.13]: radio.GetDSPTX(0).TXPostGenRun = 0;
        if (m_txChannel) {
            m_txChannel->setTuneTone(false, 0.0, 0.0);
        }

        // ── RESTORE DSP MODE if swapped ────────────────────────────────────────
        // Cite: console.cs:30112-30122 [v2.10.3.13]:
        //   switch (old_dsp_mode) { case CWL: case CWU:
        //       radio.GetDSPTX(0).CurrentDSPMode = old_dsp_mode; ... }
        if (m_activeSlice) {
            const bool wasSwapped = (m_savedTxDspMode == DSPMode::CWL ||
                                     m_savedTxDspMode == DSPMode::CWU);
            if (wasSwapped) {
                m_activeSlice->setDspMode(m_savedTxDspMode);
            }
        }

        // ── RESTORE POWER ──────────────────────────────────────────────────────
        // Cite: console.cs:30129-30132 [v2.10.3.13]:
        //   if (_tuneDrivePowerSource == DrivePowerSource.FIXED) PWR = PreviousPWR;
        //   //MW0LGE_22b  [original inline comment from console.cs:30033]
        if (m_connection) {
            auto* conn = m_connection;
            // Scale percent (0-100) → wire byte (0-255) with SWR-protection
            // foldback. See setTxDrive scaling comment in connection-power
            // lambda for the mi0bot NetworkIO.cs:209-211 [v2.10.3.14-beta1]
            // citation. Default factor=1.0 → identity to prior formula.
            const float fSaved          = std::clamp(m_savedPowerPct / 100.0f, 0.0f, 1.0f);
            const float swrProtectSaved =
                std::clamp(m_transmitModel.swrProtectFactor(), 0.0f, 1.0f);
            const int savedWireDrive =
                std::clamp(int(255.0f * fSaved * swrProtectSaved), 0, 255);
            QMetaObject::invokeMethod(conn, [conn, savedWireDrive]() {
                conn->setTxDrive(savedWireDrive);
            });
        }

        // ── RESTORE TX VFO (un-offset from cw_pitch) ───────────────────────────
        // Mirrors Thetis console.cs:31788-31810 [v2.10.3.13] which only
        // applies the ±cw_pitch tx_freq offset while chkTUN.Checked == true.
        // Once TUNE drops, txtVFOAFreq_LostFocus recomputes tx_freq without
        // the offset so the carrier returns to dial freq.
        if (m_activeSlice && m_connection) {
            const quint64 dialHz =
                static_cast<quint64>(m_activeSlice->frequency());
            auto* conn = m_connection;
            QMetaObject::invokeMethod(conn, [conn, dialHz]() {
                conn->setTxFrequency(dialHz);
            });
        }

        // ── RESTORE METER MODE ─────────────────────────────────────────────────
        // Cite: console.cs:30136-30137 [v2.10.3.13]:
        //   if (current_meter_tx_mode != old_meter_tx_mode_before_tune) //MW0LGE_21j
        //       CurrentMeterTXMode = old_meter_tx_mode_before_tune;
        // NereusSDR: deferred to H.3 (no MeterModel setTxDisplayMode() yet).
        // [H.3 hook: restore meterModel().setTxDisplayMode(savedMode) here]

        m_isTuning = false;
    }
}

void RadioModel::onMoxHardwareFlipped(bool isTx)
{
    // Step 1 — Alex antenna routing.  Resolves which TX/RX antenna ports
    // engage for the current band and tx/rx state.  AlexController state
    // is read inside applyAlexAntennaForBand; result is pushed to
    // m_connection->setAntennaRouting() internally.
    // Pre-code review §2.3 step 8 [v2.10.3.13].
    const Band band = m_activeSlice
                        ? bandFromFrequency(m_activeSlice->frequency())
                        : m_lastBand;
    applyAlexAntennaForBand(band, isTx);

    // Steps 2 + 3 — wire bits.  Guard against null connection (no radio
    // connected, or mid-teardown).  applyAlexAntennaForBand already guards
    // the same way; mirror for symmetry.  IMPORTANT: invokeMethod(nullptr, ...)
    // asserts, so this guard MUST come before the invokeMethod call below.
    if (!m_connection) {
        return;
    }

    // Steps 2 + 3 — MOX wire bit + T/R relay.
    // Both setters mutate connection-thread-owned state (m_mox /
    // m_forceBank0Next / m_trxRelay / m_forceBank10Next) and must be invoked
    // on the connection thread.  Established pattern: applyAlexAntennaForBand
    // also marshals its setAntennaRouting() call via invokeMethod (line ~2067).
    // Pre-code review §2.3 / §1.4 step 12 [v2.10.3.13] (setMox),
    // Pre-code review §2.3 step 10 [v2.10.3.13] (setTrxRelay).
    auto* conn = m_connection;
    QMetaObject::invokeMethod(conn, [conn, isTx]() {
        conn->setMox(isTx);      // Step 2 — P1 queues bank-0 flush; P2 sends immediate high-priority packet.
        conn->setTrxRelay(isTx); // Step 3 — P1 queues bank-10 flush; P2 not yet wired.
    });

    // 3M-1a bench fix: RX channel shutdown on MOX engage / restore on release.
    //
    // Porting from Thetis console.cs:29527-29543 [v2.10.3.13] — RX→TX path:
    //   if (!full_duplex)  {
    //     bool RX1_shutdown = chkVFOATX.Checked || ...;
    //     if (RX1_shutdown)
    //       WDSP.SetChannelState(WDSP.id(0, 0), 0, 1);  // off + flush
    //   }
    //
    // Porting from Thetis console.cs:29629 [v2.10.3.13] — TX→RX path:
    //   WDSP.SetChannelState(WDSP.id(0, 0), 1, 0);  // on, no flush
    //
    // 3M-1a scope: no full-duplex, no PureSignal, no VFOBTX — all currently-
    // active RX channels stop on MOX-on, restore on MOX-off.
    //
    // Ordering deviation from Thetis (acceptable for 3M-1a):
    //   - RX stop fires here on hardwareFlipped(true), which is the same
    //     moment as Alex routing / setMox wire bit — before the rfDelay.
    //     Thetis stops RX at this same point (line 29527-29543 is before
    //     HdwMOXChanged on line 29582 and the rf_delay on 29592).
    //   - RX restore fires here on hardwareFlipped(false) rather than the
    //     later rxReady phase signal.  Thetis restores at line 29629 which
    //     is after HdwMOXChanged(false) and ptt_out_delay.  The early
    //     restore is acceptable for TUN-only scope; if bench shows a click
    //     on TX→RX, wire a separate rxReady slot in a follow-up.
    if (m_wdspEngine) {
        // Only RX channel 0 is active in 3M-1a (single-RX, no RX2).
        // Thetis conditionally shuts down RX1 (id(0,0)) and sub-RX (id(0,1))
        // based on chkVFOATX/chkVFOBTX/RX2Enabled/mute_* flags.
        // For 3M-1a we unconditionally act on channel 0 (the only created channel).
        if (auto* rxCh = m_wdspEngine->rxChannel(0)) {
            if (isTx) {
                // RX off + flush.  SetChannelState(id, 0, 1) — matches
                // Thetis console.cs:29534 [v2.10.3.13].
                rxCh->setActive(false);
            } else {
                // RX on, no flush.  SetChannelState(id, 1, 0) — matches
                // Thetis console.cs:29629 [v2.10.3.13].
                rxCh->setActive(true);
            }
        }
    }
}

// ── Phase 3Q sub-PR-3: NetworkDiagnosticsDialog text accessors ──────────────
// Each accessor is thin — it reads already-held state and formats it.
// Returns "—" (em-dash) in any disconnected/unresolved case so callers
// never need to guard against null or empty strings.

QString RadioModel::connectionUptimeText() const
{
    if (!m_connectionStartedAt.isValid()) {
        return QStringLiteral("—");
    }
    const qint64 elapsedSec = m_connectionStartedAt.secsTo(QDateTime::currentDateTime());
    if (elapsedSec < 0) {
        return QStringLiteral("—");
    }
    const qint64 h  = elapsedSec / 3600;
    const qint64 m  = (elapsedSec % 3600) / 60;
    const qint64 s  = elapsedSec % 60;
    if (h > 0) {
        return QString::asprintf("%lldh %02lldm %02llds",
                                 static_cast<long long>(h),
                                 static_cast<long long>(m),
                                 static_cast<long long>(s));
    }
    return QString::asprintf("%lldm %02llds",
                             static_cast<long long>(m),
                             static_cast<long long>(s));
}

QString RadioModel::connectedRadioName() const
{
    if (!isConnected() || m_lastRadioInfo.name.isEmpty()) {
        return QStringLiteral("—");
    }
    return m_lastRadioInfo.name;
}

QString RadioModel::connectionProtocolText() const
{
    if (!isConnected()) {
        return QStringLiteral("—");
    }
    return QString::number(static_cast<int>(m_lastRadioInfo.protocol));
}

QString RadioModel::connectionFirmwareText() const
{
    if (!isConnected() || m_lastRadioInfo.firmwareVersion <= 0) {
        return QStringLiteral("—");
    }
    return QStringLiteral("v") + QString::number(m_lastRadioInfo.firmwareVersion);
}

QString RadioModel::connectionIpText() const
{
    if (!isConnected()) {
        return QStringLiteral("—");
    }
    return m_lastRadioInfo.address.toString()
           + QStringLiteral(" : ")
           + QString::number(m_lastRadioInfo.port);
}

QString RadioModel::connectionMacText() const
{
    if (!isConnected() || m_lastRadioInfo.macAddress.isEmpty()) {
        return QStringLiteral("—");
    }
    return m_lastRadioInfo.macAddress;
}

int RadioModel::connectionSampleRateHz() const
{
    return isConnected() ? m_connectionSampleRateHz : 0;
}

QString RadioModel::connectionSampleRateText() const
{
    const int rateHz = connectionSampleRateHz();
    if (rateHz <= 0) {
        return QStringLiteral("—");
    }
    if (rateHz % 1000 == 0) {
        return QString::number(rateHz / 1000) + QStringLiteral(" kHz");
    }
    return QString::number(rateHz) + QStringLiteral(" Hz");
}

// Phase 3Q Sub-PR-4 D.3 — Segment hover tooltip.
// Jitter / packet-loss / audio-backend rows omitted until those metrics
// have real sources — no NYI placeholders per the "no NYI" rule.
QString RadioModel::buildConnectionTooltip() const
{
    if (!isConnected()) {
        return tr("Disconnected. Click to connect.");
    }

    const double txMbps = m_connection ? m_connection->txByteRate(1000) : 0.0;
    const double rxMbps = m_connection ? m_connection->rxByteRate(1000) : 0.0;

    QString lines;
    lines += QStringLiteral("%1 — Connected %2\n")
                 .arg(connectedRadioName(), connectionUptimeText());
    lines += QStringLiteral("  %1 · %2\n")
                 .arg(connectionIpText(), connectionMacText());
    lines += QStringLiteral("  Protocol %1 · Firmware %2 · %3\n")
                 .arg(connectionProtocolText(),
                      connectionFirmwareText(),
                      connectionSampleRateText());
    // Build glyphs via QChar rather than UTF-8 byte-escape strings —
    // ebe9030 documented that "\xe2\x96…" sequences inside QStringLiteral
    // get misinterpreted as Latin-1 codepoints on the macOS compile
    // path, rendering as garbage. QChar(0x25B2) = ▲, QChar(0x25BC) = ▼.
    lines += QStringLiteral("  Throughput: ") + QChar(0x25B2)
           + QStringLiteral(" %1 Mbps · ").arg(QString::number(txMbps, 'f', 1))
           + QChar(0x25BC)
           + QStringLiteral(" %1 Mbps").arg(QString::number(rxMbps, 'f', 1));
    return lines;
}

} // namespace NereusSDR
