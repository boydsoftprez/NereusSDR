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
//   2026-05-03 — Phase 4 Agent 4A of issue #167 (PA calibration safety
//                 hotfix — K2GX field report).  Drive-slider lambda
//                 (lines ~830) and TUNE-engagement path (lines ~4280)
//                 rewritten to route through
//                 TransmitModel::setPowerUsingTargetDbm (Phase 3C deep-
//                 parity port of Thetis SetPowerUsingTargetDBM,
//                 console.cs:46645-46762 [v2.10.3.13]).  Wire-byte /
//                 IQ-scalar topology corrected per Thetis MW0LGE-canonical
//                 (audio.cs:262-271 wire NO SWR / cmaster.cs:1115-1119 IQ
//                 HAS SWR via TxChannel::setTxFixedGain).  Replaces the
//                 previous fork-specific linear formula (cited to mi0bot
//                 NetworkIO.cs:209-211 [v2.10.3.14-beta1]) that produced
//                 K2GX's >300 W output on a 200 W ANAN-8000DLE at 80m
//                 TUN slider=50.  Adds RadioModel ownership of
//                 PaProfileManager (mirrors MicProfileManager pattern);
//                 active profile passed by const-ref to setPowerUsingTargetDbm
//                 at every callsite.  Adds StepAttenuatorController
//                 propagation to TransmitModel::setStepAttenuatorController
//                 inside RadioModel::setStepAttController.  J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
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
#include "core/FFTEngine.h"
// 3M-1a G.1: TX-side integration — MoxController + TxChannel view.
// TxMicRouter is already included via RadioModel.h (for std::unique_ptr destructor).
#include "core/MoxController.h"
#include "core/MicProfileManager.h"
#include "core/PaProfile.h"
#include "core/PaProfileManager.h"
#include "core/PaTelemetryScaling.h"
#include "core/PureSignal.h"
#include "core/StepAttenuatorController.h"
#include "core/TwoToneController.h"
// Phase 3F Sub-Epic C Task 6: TxSliceArbiter integration.
#include "core/TxSliceArbiter.h"
#include "core/FFTRouter.h"  // Phase 3F Sub-Epic D Task 13
#include "models/FilterPresetStore.h"
#include "core/accessories/N2adrPreset.h"
#include "core/codec/AlexFilterMap.h"  // Phase 3F: per-ADC BPF -> HPF bits
#include "core/TxChannel.h"
// 3M-1c TX pump architecture redesign — dedicated worker thread for
// TX DSP pump (replaces D.1/E.1/L.4 chain).
#include "core/TxWorkerThread.h"
// 3M-1b L.1: concrete mic-source strategy objects.
#include "core/audio/PcMicSource.h"
#include "core/audio/RadioMicSource.h"
#include "core/audio/RealtimeAudioPriority.h"
#include "core/audio/VaxTxMicSource.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/TxMicSource.h"
#include "core/RadioConnection.h"
#include "core/RadioConnectionTeardown.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/PsccPump.h"   // Phase 3M-4 Task 17 chunk C — pscc() driver
#include "core/WidebandFftEngine.h"  // Phase 3F Sub-Epic F Task 5 — per-ADC wb FFT
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
#include "core/SkuUiProfile.h"
#include "core/wdsp_api.h"
#include "gui/SpectrumWidget.h"

// ── Phase 3J-2 H2: spot-system ownership ────────────────────────────────
#include "core/DxClusterClient.h"
#include "core/WsjtxClient.h"
#include "core/SpotCollectorClient.h"
#include "core/PotaClient.h"
#include "core/FreeDVReporterClient.h"
#include "core/FreeDVRadeReporterBridge.h"
#include "core/PskReporterClient.h"
#include "core/DxccColorProvider.h"
#include "core/DxSpot.h"
#include "core/FreeDVStation.h"
#include "models/SpotModel.h"
#include "models/SpotTableModel.h"  // for SpotTableModel::extractMode (mode guess)
#include "models/FreeDVStationModel.h"
#include "models/RxDecodeModel.h"

// Phase 3R Task I5: RadeChannel signal-graph wiring. Forward-declared in
// RadioModel.h; the .cpp pulls the full type for the connect() calls in
// wireRadeChannel().
#include "core/RadeChannel.h"

// Phase 3R K-bench: Resampler used to upsample RADE's 24 kHz baseband
// to the connection's TX I/Q rate (P1=48 kHz, P2=192 kHz).
// TxWorkerThread is already included above (line 268) for the existing
// TX pump wiring; K-bench reuses that include for setRadeChannel +
// the radeMicBlockReady signal.
#include "core/Resampler.h"

// FlexRadio UDP 4992 discovery beacon. Owned by RadioModel; configured
// and started on radio connect so PGXL/TGXL auto-discover NereusSDR.
#include "core/FlexRadioDiscoveryBroadcaster.h"

// Passive SmartSDR API listener on TCP 4992. Bench-recon stub: logs every
// line PGXL sends so we can design the response layer in a follow-up.
#include "core/SmartSdrApiListener.h"

#include <algorithm>
#include <cmath>

#include <QCryptographicHash>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMetaObject>
#include <QScopeGuard>
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

// scaleFwdPowerWatts: lifted from this anonymous namespace into the
// public PaTelemetryScaling API in Phase 1 Agent 1B of issue #167.
// See src/core/PaTelemetryScaling.{h,cpp} — same Thetis-canonical math
// (computeAlexFwdPower at console.cs:25008-25072 [v2.10.3.13 @501e3f5]),
// same per-board triplet table, same default-case fallthrough.  The
// callsite in handlePaTelemetry now reads NereusSDR::scaleFwdPowerWatts
// (Phase 4 Agent 4A migration).

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
    case HPSDRModel::ANAN_G2E: //N1GP G2E added [Thetis console.cs:25007 v2.10.3.15]
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:                 // will need to be edited for scaling
    case HPSDRModel::REDPITAYA: //DH1KLM
        bridge_volt = 0.15;  refvoltage = 5.0; adc_cal_offset = 28;
        break;
    case HPSDRModel::ORIONMKII:
    case HPSDRModel::ANAN8000D:
        bridge_volt = 0.08;  refvoltage = 5.0; adc_cal_offset = 16;
        break;
    // From mi0bot console.cs:25195-25199 [v2.10.3.13-beta2] — HL2 has its
    // own coupler scaling for the REV side, same bridge_volt as the FWD
    // side.  Bench-reported #167 follow-up: without this case, HL2 fell
    // through to the default {0.09, 3.3, 3} triplet which is 16.7×
    // wrong — the resulting refl/fwd ratio inflated by the same factor
    // pegged the SWR meter at the upper rail (>3.0) on a 50Ω dummy load.
    //   //MI0BOT: HL2  [original inline comment from mi0bot console.cs:25195]
    case HPSDRModel::HERMESLITE:
        bridge_volt = 1.5;   refvoltage = 3.3; adc_cal_offset = 6;
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
    case HPSDRModel::ANAN_G2E: //N1GP G2E added [Thetis console.cs:25007 v2.10.3.15 grouping]
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
    case HPSDRModel::ANAN_G2E: //N1GP G2E added [Thetis console.cs:25007 v2.10.3.15 grouping]
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

// scaleExciterPowerMw() is the public free function in PaTelemetryScaling.h/cpp
// (lifted there for testability — Phase F1 of the ANAN-G2E port).
// No local copy needed here; PaTelemetryScaling.h is already included above.

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
        //
        // Issue #257: pass the SkuUiProfile so the RX-only label slot
        // (EXT1 / BYPS / XVTR depending on SKU) wins over the ANT* default
        // when rxOnlyAnt(band) != 0.
        if (m_activeSlice) {
            const SkuUiProfile sku = skuUiProfileFor(m_hardwareProfile.model);
            m_activeSlice->refreshAntennasFromAlex(m_alexController, b, &sku);
        }
    });
    // Also persist the two blockTxAnt* safety toggles; they can change
    // via the Antenna Control grid even when no band crossing occurs.
    connect(&m_alexController, &AlexController::blockTxChanged, this,
            [this]() {
        m_alexControllerDirty = true;
        scheduleSettingsSave();
    });

    // Phase 3F: an effective-BPF change pushes on its OWN trigger.
    //
    // republishAlexAdcSlices is the only producer of AlexRxBpf, and it was
    // reachable only from requestDdcAssignment (a slice bound / retuned /
    // removed) and from the Connected handler. Neither of the two state
    // changes that actually flip AlexAdcState::effective went through
    // either of them:
    //   1. Wideband. SliceModel::widebandExtensionRequestedChanged ->
    //      setWidebandActive -> recomputeBpf sets WidebandLocked.
    //   2. Operator override. FilterPolicyDialog's Apply -> setBpfMode ->
    //      recomputeBpf.
    // Both repainted the bottom-bar CH label, which was bpfStateChanged's
    // only consumer, and left the preselector on the band it already had
    // until the operator happened to nudge the VFO. The UI asserted a
    // state the hardware was not in, for an unbounded time.
    //
    // Upstream does not defer. Thetis's operator bypass pushes from inside
    // its own setter:
    //   From Thetis console.cs:18793-18806 [v2.10.3.15]
    //     public bool AlexHPFBypass {
    //         set { alex_hpf_bypass = value;
    //               double freq = VFOAFreq;
    //               setAlex1HPF(freq);      // <- on the wire, right here
    //     ...
    // and setAlexHPF issues NetworkIO.SetAlexHPFBits(0x20) on the spot
    // (console.cs:6838-6855 [v2.10.3.15]). Thetis's wideband window drives
    // that same setter when it opens and wbClosing() restores it
    // (console.cs:43552-43566 [v2.10.3.15]).
    // Upstream inline attribution preserved verbatim (console.cs:43545):
    //   private bool _wb_caused_alex_hpf_bypass = false; //[2.10.3.7]MW0LGE fixes #529
    //
    // Hung off bpfStateChanged rather than off the two trigger sites on
    // purpose. recomputeBpf is the only writer of AlexAdcState::effective
    // and bpfStateChanged is its only exit, so every present and future
    // trigger has to pass through here to change the state at all. A third
    // trigger cannot forget to push; it would have to bypass the state
    // machine entirely to avoid it.
    connect(&m_alexController, &AlexController::bpfStateChanged, this,
            [this](int, const AlexController::AlexAdcState&) {
        republishAlexAdcSlices();
    });

    // Phase 3P-I-b (T6): flag changes must re-fire composition for current band.
    // The isTx arg stays false in 3P-I-b — MOX trigger wiring lands in 3M-1.
    // Uses a local lambda so all six connects share one band-lookup path.
    //
    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): also dirty+schedule-save on
    // every flag change so the per-MAC persistence in AlexController::save()
    // actually fires.  Without this the six TX-bypass checkboxes (Rx
    // BYPASS on Tx, Ext1 on Tx, Use TX antenna for RX, RX out override,
    // XVTR active) reset to default on every app reload, forcing the
    // user to re-check them every session — confirmed on the bench when
    // "Rx BYPASS on Tx" (G2E label for ext2OutOnTx) dropped after a
    // graceful close.
    auto reapplyAndPersist = [this]() {
        m_alexControllerDirty = true;
        scheduleSettingsSave();
        Band b = m_activeSlice
                   ? bandFromFrequency(m_activeSlice->frequency())
                   : m_lastBand;
        applyAlexAntennaForBand(b);
    };
    connect(&m_alexController, &AlexController::ext1OutOnTxChanged,
            this, [reapplyAndPersist](bool) { reapplyAndPersist(); });
    connect(&m_alexController, &AlexController::ext2OutOnTxChanged,
            this, [reapplyAndPersist](bool) { reapplyAndPersist(); });
    connect(&m_alexController, &AlexController::rxOutOnTxChanged,
            this, [reapplyAndPersist](bool) { reapplyAndPersist(); });
    connect(&m_alexController, &AlexController::rxOutOverrideChanged,
            this, [reapplyAndPersist](bool) { reapplyAndPersist(); });
    connect(&m_alexController, &AlexController::useTxAntForRxChanged,
            this, [reapplyAndPersist](bool) { reapplyAndPersist(); });
    connect(&m_alexController, &AlexController::xvtrActiveChanged,
            this, [reapplyAndPersist](bool) { reapplyAndPersist(); });


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

    // ── Phase 3F Sub-Epic C Task 6: TxSliceArbiter construction + wiring ──
    // Owned QObject child of RadioModel (Qt parent semantics handle the
    // destruction).  Sliced list pointer is non-owning; the arbiter reads
    // m_slices lazily via the pointer, so wiring it now while m_slices is
    // still empty is safe (the first slice gets appended later in
    // connectToRadio() → addSlice(), and the arbiter only iterates the
    // list inside requestHandoff() / load()).  MoxController is wired now
    // so the arbiter can drop MOX synchronously before flipping txSlice
    // flags on RF-safe handoff (see TxSliceArbiter::requestHandoff).
    //
    // MAC injection + load() runs later from the currentRadioChanged
    // lambda at the bottom of this constructor, since the per-MAC
    // AppSettings scope key isn't known until a radio actually connects.
    // save() runs from teardownConnection() before the connection is
    // destroyed.
    m_txSliceArbiter = new TxSliceArbiter(this);
    m_txSliceArbiter->setSliceList(&m_slices);
    m_txSliceArbiter->setMoxController(m_moxController);

    // RF-SAFETY: handing the transmitter to another slice moves the transmit
    // frequency, and with it the Alex TX low-pass. Push immediately rather
    // than waiting for the next retune, otherwise the new TX slice sits
    // behind the OLD slice's low-pass until something else happens to move
    // — which on a 10 m / 80 m pair is exactly the wrong-filter condition
    // the arbiter's RF-safe handoff exists to avoid.
    //
    // The arbiter drops MOX before it flips the binding, so this always runs
    // with the transmitter unkeyed.
    connect(m_txSliceArbiter, &TxSliceArbiter::txBoundSliceChanged,
            this, [this](int, int) {
        pushTxFrequencyFromTxSlice();
        pushTxModeAndBandpass();
        applyTxAntennaFromBoundSlice();
        if (m_moxController) {
            if (SliceModel* const bound = txBoundSlice()) {
                m_moxController->onModeChanged(bound->dspMode());
            }
        }
    });

    // Phase 3F Sub-Epic D Task 13: FFT fan-out router. NereusSDR-original
    // class (AetherSDR has no equivalent because it's a thin Flex API
    // client; we own the FFT pipeline locally). MainWindow registers
    // pan-to-receiver mappings in the sliceAdded handler; the per-
    // receiver FFTEngine pump is wired in Sub-Epic E / F polish.
    m_fftRouter = new FFTRouter(this);

    // MoxController::hardwareFlipped → RadioModel::onMoxHardwareFlipped (F.1).
    // Qt::QueuedConnection: both live on the main thread, but QueuedConnection
    // documents the cross-component intent and ensures the slot runs after the
    // emitting call stack unwinds, matching the pre-code review §1.6 pattern.
    // From Thetis console.cs:29567-29576 [v2.10.3.13] — HdwMOXChanged call.
    //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29567-29576]
    connect(m_moxController, &MoxController::hardwareFlipped,
            this, &RadioModel::onMoxHardwareFlipped,
            Qt::QueuedConnection);

    // Phase 3M-4 Task 17 chunk A — wire MOX state into ReceiverManager so
    // the per-board codec re-emits PsDdcConfig on TX/RX transitions.  The
    // codec output flow is:
    //   ReceiverManager::setMox(on)
    //     → updateDdcAssignment()
    //     → m_p2Codec->applyPureSignalDdcConfig(...)        // PsDdcConfig out
    //     → emit ddcConfigChanged(config)                   // observation only
    // Protocol 2 wire state follows the separate full-DdcAssignment request
    // made by onMoxHardwareFlipped().
    // Mirrors Thetis console.cs:8186-8538 UpdateDDCs() [v2.10.3.13] firing
    // on MOX edge: in Thetis the call goes through `chkMOX_CheckedChanged`
    // → `UpdateDDCs(false)` immediately after `mox = chkMOX.Checked`.
    connect(m_moxController, &MoxController::hardwareFlipped,
            m_receiverManager, &ReceiverManager::setMox,
            Qt::QueuedConnection);

    // Issue #177 fix — Thetis-faithful TUN-off ordering.
    //
    // setTune(false) latches m_pendingTuneOff and kicks off the MoxController
    // TX→RX walk, then returns immediately.  When MoxController emits rxReady
    // (TX→RX phase 4 of 4 — RX channels active, MOX wire bit dropped, WDSP TX
    // off), we wait an additional m_tuneOffSettleMs (default 100) and then
    // call completeTuneOff() to kill gen1, restore the DSP mode, restore the
    // power slider, and un-offset the TX VFO.
    //
    // From Thetis console.cs:30106-30109 [v2.10.3.13] — chkTUN_CheckedChanged
    // else-branch:
    //     chkMOX.Checked = false;          // synchronously walks TX→RX (~30 ms blocking)
    //     await Task.Delay(100);
    //     radio.GetDSPTX(0).TXPostGenRun = 0;
    //
    // The deferred completion prevents the gen1-off transient from reaching
    // the radio while the WDSP TX channel is still pumping (issue #177).
    connect(m_moxController, &MoxController::rxReady, this, [this]() {
        if (!m_pendingTuneOff) {
            return;
        }
        QTimer::singleShot(m_tuneOffSettleMs, this, [this]() {
            // Re-check the latch: a fresh setTune(true) (or a teardown) can
            // clear it between rxReady and the timer firing.  In that case the
            // deferred completion is a no-op because the new TUN-on path has
            // already established fresh saved state and re-engaged MOX.
            if (!m_pendingTuneOff) {
                return;
            }
            completeTuneOff();
        });
    });

    // MoxController::txReady → TxChannel::setRunning(true) and
    // MoxController::txaFlushed → TxChannel::setRunning(false) are wired in
    // connectToRadio() once m_txChannel is live (see the "MoxController →
    // TxChannel queued connects" block inside the WDSP-init lambda).  We
    // cannot wire them here at construction time because m_txChannel is
    // nullptr until createTxChannel(kTxChannelId) runs inside that lambda — Qt's
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
    // SliceModel wiring is installed from addSlice(), after a stable TX
    // binding exists. Each source lambda qualifies itself against that
    // binding before it may update the global VOX mode gate.
    connect(&m_transmitModel, &TransmitModel::voxEnabledChanged,
            m_moxController,  &MoxController::setVoxEnabled);

    // The actual per-slice connects happen in addSlice(); construction-time
    // wiring would silently no-op because m_slices is still empty here.

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
    //
    // Signal chain:
    //   TransmitModel::voxHangTimeMsChanged    → MoxController::setVoxHangTime
    //   TransmitModel::antiVoxGainDbChanged    → MoxController::setAntiVoxGain
    //   TransmitModel::antiVoxRunChanged       → MoxController::setAntiVoxRun
    //                                                (3M-3a-iv scope-expansion;
    //                                                 wired below in the
    //                                                 cancellation-feed block)
    //   MoxController::voxHangTimeRequested    → TxChannel::setVoxHangTime
    //   MoxController::antiVoxGainRequested    → TxChannel::setAntiVoxGain
    //   MoxController::antiVoxRunRequested     → TxWorkerThread::setAntiVoxRun
    //                                                (3M-3a-iv scope-expansion)
    //
    // 3M-3a-iv post-bench refactor (Option A) removed the antiVoxSourceVax
    // chain (TransmitModel::antiVoxSourceVaxChanged →
    // MoxController::setAntiVoxSourceVax → antiVoxSourceWhatRequested) entirely.
    // Thetis chkAntiVoxSource (RX vs VAC at cmaster.cs:912-943 [v2.10.3.13])
    // does not map to NereusSDR's architecture: VAX is a digital-mode app bus
    // with no mic-feedback path, so the audio output device is the only valid
    // anti-VOX cancellation reference.  See commit message and DexpVoxPage
    // info-row for the architectural rationale.
    //
    // MoxController handles ms→seconds and dB→linear conversions; TxChannel
    // wrappers (D.3) are thin WDSP delegates.
    connect(&m_transmitModel, &TransmitModel::voxHangTimeMsChanged,
            m_moxController,  &MoxController::setVoxHangTime);
    connect(&m_transmitModel, &TransmitModel::antiVoxGainDbChanged,
            m_moxController,  &MoxController::setAntiVoxGain);

    // MoxController::voxHangTimeRequested / antiVoxGainRequested →
    // TxChannel setters are wired in connectToRadio() once m_txChannel is
    // live — same reason as txReady / txaFlushed above.

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
    //   onVoxActive: WIRED in 3M-3a-iii Task 17 (bench fix, 2026-05-04) via
    //     TxChannel::voxActiveChanged — see connectToRadio() H.1 block for
    //     the connect() callsite. The wire-up was deferred from 3M-1b and
    //     omitted from the 3M-3a-iii plan; JJ's bench surfaced the gap.
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

    // ── Phase 4 Agent 4A of #167: PaProfileManager ───────────────────────────
    //
    // Per-MAC bank holding 16 "Default - <model>" factory profiles + 1 Bypass
    // profile, each carrying a 14-band x 9-drive-step PA gain table.
    // Constructed once at RadioModel-ctor time; setMacAddress +
    // load(connectedModel) are called per-connect inside connectToRadio()
    // (mirrors MicProfileManager wiring exactly).  Empty MAC is a silent
    // no-op per the PaProfileManager.h contract.  Qt parent=this so the
    // dtor frees it.
    //
    // The active profile is read at every drive-slider / TUNE / two-tone
    // callsite via paProfileManager()->activeProfile() and passed by const
    // reference to TransmitModel::setPowerUsingTargetDbm.  Per-call pass-
    // through (vs. injecting a manager pointer into TransmitModel) keeps
    // the coupling lower — TransmitModel stays a pure data + math model
    // with no manager dependencies.
    m_paProfileManager = new PaProfileManager(this);

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

    // ── Stage C2: FilterPresetStore ───────────────────────────────────────────
    // Wraps Thetis-verbatim defaults from SliceModel::presetsForMode with a
    // user-override layer persisted in AppSettings (keys: "filters/<mode>/<slot>/…").
    m_filterPresetStore = new FilterPresetStore(this);

    // ── Phase 3P-II Task 19: PGXL / TGXL / Tuner ownership ───────────────────
    // Constructed once here; accessors return non-null from this point on.
    // PgxlConnection and TgxlConnection are QObject children (parent=this).
    // TunerModel is likewise a QObject child; bindConnection() wires the
    // TGXL state/status signals immediately.
    m_pgxlConnection = new PgxlConnection(this);
    m_tgxlConnection = new TgxlConnection(this);
    m_tunerModel     = new TunerModel(this);
    m_tunerModel->bindConnection(m_tgxlConnection);

    // Phase 3P-III: RF-Kit RF2K-S connection. Constructed unconditionally;
    // the poller only starts when rfKitEnabled is set to true (reads
    // RfKit_ManualIp / RfKit_ManualPort from per-MAC peripherals scope at
    // that point).
    m_rfKitConnection = std::make_unique<Rf2ksConnection>(this);

    // Per-radio peripherals refactor (2026-05-26): the ctor-time RF-Kit
    // auto-connect from globals was removed.  The lifecycle now runs in
    // applyPeripheralsForCurrentMac(), driven from onConnectionStateChanged
    // when the radio reports Connected and m_lastRadioInfo.macAddress is
    // populated.  Existing single-radio installs are preserved by
    // migratePeripheralGlobalsIfNeeded(), which folds the legacy global
    // RfKit_* / FourO3A_Enabled / PGXL_Manual* / TGXL_Manual* keys into the
    // first-connected MAC's hardware/<mac>/peripherals/ scope and sets
    // PeripheralsMigrationDone="True" so subsequent launches skip.

    // Phase 3P-II Task 86: TxInterlockPolicy -- NereusSDR-native TX gate.
    // Loads persisted mode/grace/SWR-gate values from AppSettings in its ctor.
    // Non-null from this point; shared (non-owning) with PgxlInterlockPage
    // and MoxController (wired below after m_moxController construction).
    m_txInterlockPolicy = new TxInterlockPolicy(this);

    // Phase 3P-II Phase 4 Task 89: TuneMemoryStore -- shared TGXL relay cache.
    // Non-null from this point; shared (non-owning) with TgxlAdvancedPage and
    // TunerApplet. Lifetime: same as RadioModel (Qt parent-ownership).
    m_tuneMemoryStore = new TuneMemoryStore(this);

    // Phase 3P-II Phase 4 Task 94: FaultLog ring buffers for PGXL and TGXL.
    // Non-null from this point; shared (non-owning) with PgxlAdvancedPage and
    // TgxlAdvancedPage. Lifetime: same as RadioModel (Qt parent-ownership).
    // RadioModel captures PGXL FAULT state transitions via onPgxlStatus().
    // TGXL fault capture is bench-deferred (design doc section 4.7); the
    // instance is provided now so TgxlAdvancedPage can use the shared log.
    m_pgxlFaultLog = new FaultLog(QStringLiteral("PGXL_FaultHistory"), this);
    m_tgxlFaultLog = new FaultLog(QStringLiteral("TGXL_FaultHistory"), this);

    // FlexRadio UDP 4992 discovery beacon.
    // Constructed once; configured in connectToRadio() once the radio MAC is
    // known; stopped in teardownConnection(). Allows PGXL/TGXL to auto-discover
    // NereusSDR in their "FlexRadio" dropdown without any manual IP entry.
    // Wire format reverse-engineered from a FLEX-8600 beacon captured 2026-05-19
    // (captures/flex-pgxl-tgxl-capture_00001_20260519173452.pcapng).
    m_flexBroadcaster = new FlexRadioDiscoveryBroadcaster(this);

    // SmartSDR API responder on TCP 4992. Accepts PGXL/TGXL connections,
    // serves slice + transmit S-frames, and forwards LAN PTT requests
    // (`transmit tune on/off`) to RadioModel::setTune so a TGXL hardware
    // TUNE press actually engages the local CW tune carrier.
    //
    // Per-radio peripherals refactor (2026-05-26): construction is
    // unconditional but the listener is NOT started here.  Start/stop is
    // driven from applyPeripheralsForCurrentMac() / teardownPeripherals()
    // based on the per-MAC FourO3A_Enabled flag.  setFourO3AEnabled(bool)
    // still provides the live toggle path used by the General tab's
    // master toggle.
    m_smartSdrListener = new SmartSdrApiListener(this);
    qCInfo(lcConnection) << "SmartSDR API listener constructed; start deferred"
                          << "to Connected handler (per-MAC FourO3A_Enabled gate)";
    // LAN PTT wiring: TGXL emits `C<seq>|transmit tune on` when its
    // hardware TUNE button (or its native app TUNE button) is pressed; the
    // listener parses + ACKs the frame then emits tuneRequested(true).
    // We pipe that into the G.4 orchestrator setTune(true) so the gen1
    // PostGen tone is actually emitted. Symmetric path on tune off.
    // No latch needed here because TGXL is authoritative -- the off arrives
    // when TGXL has finished tuning regardless of who initiated.
    connect(m_smartSdrListener, &SmartSdrApiListener::tuneRequested,
            this, [this](bool on) {
        qCInfo(lcConnection) << "LAN PTT tuneRequested(" << on << ")";
        if (on) {
            // 2026-05-20 bench fix: TGXL ECHOES our outbound `tune=1` in
            // the slice/transmit S-frame back to us as `transmit tune
            // on` -- effectively saying "I acknowledge the tune state."
            // The old code treated every `transmit tune on` as a
            // TGXL-hardware-TUNE press and recursively kicked off
            // startTgxlAutotune, which sent `operate=0` to PGXL mid-TX
            // and collapsed the in-progress operator-initiated TUN.
            //
            // Guards (any one short-circuits the autotune trigger):
            //
            //  (a) m_tgxlAutotuneInProgress: a TunerApplet-initiated
            //      autotune is already running. Same guard the old code
            //      had; TGXL's `tune on` echo here is informational
            //      because m_tgxlAutotuneFromHardware was set false and
            //      we've already executed the standby + autotune cmd.
            //  (b) m_isTuning: the operator is already in a TUN cycle
            //      (TxApplet TUNE click, or any other path that called
            //      RadioModel::setTune(true)). TGXL's `tune on` here is
            //      the echo, NOT a hardware TUNE press. Without this
            //      guard, the echo aborts the user's full-beans TUN by
            //      flipping PGXL to STANDBY mid-key.
            //
            // The TGXL hardware TUNE button path (where TGXL initiates)
            // still flows through correctly: at that moment isTune() is
            // false AND m_tgxlAutotuneInProgress is false, so neither
            // guard fires and startTgxlAutotune(fromHardware=true) runs.
            if (m_tgxlAutotuneInProgress) {
                qCInfo(lcConnection)
                    << "LAN PTT tuneRequested(true) suppressed:"
                       " TunerApplet autotune already in progress"
                       " (TGXL echo)";
                return;
            }
            if (m_isTuning) {
                qCInfo(lcConnection)
                    << "LAN PTT tuneRequested(true) suppressed:"
                       " operator-initiated TUN already engaged"
                       " (TGXL echo, not a hardware-TUNE press)";
                return;
            }
            // TGXL hardware TUNE pressed (or TGXL native app TUNE). TGXL
            // is already running its own internal sweep cycle; we just
            // need to provide the carrier and put PGXL in STANDBY for
            // the duration. Same orchestration as TunerApplet TUNE click,
            // but with fromHardware=true to skip the redundant `autotune`
            // command (TGXL already started).
            startTgxlAutotune(/*fromHardware=*/true);
        } else {
            // TGXL released tune (cycle done or aborted). Drop our local
            // carrier only if WE engaged it via the autotune orchestration
            // (m_tgxlAutotuneInProgress). When the operator is running an
            // operator-initiated TUN (TxApplet TUNE click), TGXL's
            // `transmit tune off` is just an echo telling us TGXL is no
            // longer participating -- but the operator's TUN cycle is
            // separate from TGXL's view and shouldn't be aborted by an
            // echo. The operator's own TUN click decides when to drop.
            if (!m_tgxlAutotuneInProgress) {
                qCInfo(lcConnection)
                    << "LAN PTT tuneRequested(false) suppressed:"
                       " no autotune in progress (TGXL echo, operator"
                       " TUN cycle owns the drop)";
                return;
            }
            setTune(false);
        }
    });

    // Mirror local TUN / MOX state into the listener's outbound `transmit`
    // S-frame so TGXL's bandA-tracker AND its tune-detector see what we're
    // doing. Specifically: when the operator clicks the NereusSDR app TUNE
    // button (TunerApplet or TxApplet), MoxController fires
    // manualMoxChanged(true) at the end of the TUN-on walk -- we propagate
    // that to the listener as tune=1 in the next S-frame burst. TGXL reads
    // tune=1, knows the FlexRadio (us) is in tune mode, and starts its own
    // relay sweep. Without this propagation, the only path to a TGXL sweep
    // is the LAN PTT round-trip from a TGXL-initiated tune (hardware
    // button), which forced the operator to release the carrier manually
    // when our app TUNE was clicked.
    if (m_moxController) {
        connect(m_moxController, &MoxController::manualMoxChanged,
                this, [this](bool isManual) {
            if (m_smartSdrListener) {
                m_smartSdrListener->setTuneActive(isManual);
            }
            // TGXL autotune orchestration: restore PGXL when local TUN
            // drops. m_tgxlAutotuneInProgress is set by startTgxlAutotune
            // and only those cycles need the PGXL state restore. TxApplet
            // TUN drops won't trigger this branch because the flag stays
            // false (operator's full-beans TUN doesn't touch PGXL state).
            if (!isManual && m_tgxlAutotuneInProgress) {
                m_tgxlAutotuneInProgress = false;
                // Clear the interlock-grant gate too; if the cycle ends
                // before interlockGranted fires (e.g. operator hit TUN-off
                // very early, or PGXL force-tripped FAULT mid-handshake),
                // we don't want a future interlockGranted from an
                // unrelated TX to fire the (now stale) autotune.
                m_awaitingInterlockForAutotune = false;
                if (m_pgxlSavedOperate && m_pgxlConnection
                    && m_pgxlConnection->isConnected()) {
                    m_pgxlConnection->sendCommand(QStringLiteral("operate=1"));
                    qCInfo(lcConnection)
                        << "TGXL autotune complete: sent operate=1 to PGXL"
                           " (expect state edge STANDBY -> OPERATE soon)";
                } else if (m_pgxlSavedOperate) {
                    qCWarning(lcConnection)
                        << "TGXL autotune complete: PGXL was operating"
                           " before cycle but connection is down; cannot"
                           " send operate=1 -- amp will stay STANDBY";
                } else {
                    qCInfo(lcConnection)
                        << "TGXL autotune complete: PGXL was not operating"
                           " before cycle, leaving in current state";
                }
                m_pgxlSavedOperate = false;
            }
        });
        // Split engage / release across two MoxController phase signals
        // so the interlock chain runs in the right RF-safe order:
        //
        //   MOX engage (RX->TX):
        //     txAboutToBegin   -- fires BEFORE rfDelay, BEFORE any RF is
        //                         on-air. We send PTT_REQUESTED here so
        //                         PGXL has the full ~30 ms rfDelay PLUS
        //                         the time the local RF takes to ramp up
        //                         to switch its relays into the amp path.
        //                         The actual carrier doesn't flow until
        //                         interlockGranted clears the F.1 gate
        //                         below (TxChannel::setRunning(true)).
        //     [rfDelay]
        //     txReady          -- F.1 gate: deferred until interlock-
        //                         Granted fires; then audio flows.
        //
        //   MOX release (TX->RX):
        //     moxStateChanged(false) -- fires at the very end of the
        //                         TX->RX walk, AFTER txaFlushed has
        //                         already stopped TxChannel. The carrier
        //                         is already dead, so releasing the
        //                         interlock here is safe.
        //
        // 2026-05-20 bench-driven: previously BOTH engage and release
        // were on moxStateChanged, which fires at the END of the engage
        // walk -- i.e. AFTER rfDelay AND AFTER txReady. RF was flowing
        // at full power into PGXL's bypass path for ~250 ms before PGXL
        // had a chance to ACK PTT_REQUESTED and switch to the amp path,
        // causing intermittent (~5 %) high-SWR trips on PGXL.
        connect(m_moxController, &MoxController::txAboutToBegin,
                this, [this]() {
            if (!m_smartSdrListener) { return; }
            // ARM the RF-flow gate BEFORE setInterlockTransmitting may
            // synchronously emit interlockGranted (fast-ACK case where
            // an amp ACKs PTT_REQUESTED within ~100 ms; we've seen 117 ms
            // on TGXL). Without arming here, interlockGranted would land
            // BEFORE txReady arms the gate, the grant handler would find
            // m_awaitingInterlockForTx still false, do nothing, and then
            // txReady would arm the gate to wait for a grant that has
            // already happened -> stuck. m_txReadyReceived tracks the
            // matching condition and the helper below releases setRunning
            // when BOTH have fired (regardless of order).
            //
            // Reset m_txReadyReceived here too so a previous cycle's
            // value doesn't leak into this one.
            m_txReadyReceived = false;
            if (m_smartSdrListener->hasInterlockedAmp()) {
                m_awaitingInterlockForTx = true;
            } else {
                m_awaitingInterlockForTx = false;
            }
            m_smartSdrListener->setTxActive(true);
            const QString source = m_moxController->isManualMox()
                ? QStringLiteral("TUNE")
                : QStringLiteral("MOX");
            m_smartSdrListener->setInterlockTransmitting(true, source);
        });
        // 2026-05-22 bench-fix: broadcast UNKEY_REQUESTED on txAboutToEnd
        // (phase 1 of TX->RX teardown, BEFORE the WDSP TXA drain) rather
        // than on moxStateChanged(false) (phase 4, AFTER the drain has
        // already silenced the carrier). Previously the carrier dropped
        // before UNKEY_REQUESTED reached the amps, leaving PGXL in
        // state=TRANSMIT_A with no input RF, which it interpreted as a
        // fault and briefly flashed high SWR. Canonical pcap (T+168.874)
        // shows FLEX broadcasts UNKEY_REQUESTED first, then the carrier
        // drops; amps process the un-key announcement before seeing the
        // carrier vanish. Bench-confirmed 2026-05-22: JJ observed the
        // exact high-SWR flash on un-key under the prior moxStateChanged
        // ordering.
        connect(m_moxController, &MoxController::txAboutToEnd,
                this, [this]() {
            if (!m_smartSdrListener) { return; }
            m_smartSdrListener->setTxActive(false);
            const QString source = m_moxController->isManualMox()
                ? QStringLiteral("TUNE")
                : QStringLiteral("MOX");
            m_smartSdrListener->setInterlockTransmitting(false, source);
        });

        connect(m_moxController, &MoxController::moxStateChanged,
                this, [this](bool on) {
            if (on) { return; }  // engage handled by txAboutToBegin above
            // Clear the RF-flow gate so a late interlockGranted (from
            // a slow amp ACKing after we already unkeyed) doesn't fire
            // setRunning(true) on a TX channel that's already been
            // drained back to RX by the TX->RX walk above.
            // The listener side (setTxActive + setInterlockTransmitting)
            // moved to txAboutToEnd above; this slot keeps only the
            // local gate cleanup. Phase 4 fires after the drain, so it's
            // the right place to clear gates that were waiting on amp
            // ACKs from the (now completed) cycle.
            m_awaitingInterlockForTx = false;
            m_txReadyReceived = false;
        });

        // Interlock-blocked: log only, do NOT roll back MOX.
        //
        // Bench reality 14:53:30 on 2026-05-20: the spec-literal "block on
        // timeout" rollback killed an in-flight TX that was working
        // correctly (PGXL had ACKed in 177 ms and engaged TRANSMIT_A with
        // fwd=55W swr=-24.5 dB; TGXL didn't ACK in 500 ms; we rolled back
        // MOX and PGXL fell to IDLE). The user observes this as "amp said
        // high SWR then dropped PTT" -- in reality PGXL's display flashed
        // during its forced disengage.
        //
        // The wiki failsafe at:
        //   https://github.com/flexradio/smartsdr-api-docs/wiki/TCPIP-interlock
        // says the radio must emit an AMP-blocked READY and stay out of TX
        // when an amp times out. We still emit the READY (in
        // SmartSdrApiListener::onPttAckTimeout). But killing local MOX is
        // operator-hostile in this setup -- TGXL is a tuner not gating
        // voice MOX, so its silence is informational, not blocking.
        //
        // If PGXL refuses TX (its own protection circuits) it'll fall to
        // STANDBY / FAULT and stop amplifying naturally, which is the
        // correct safety path. Forcing MOX off from our side just
        // mid-transmission cuts the operator's signal arbitrarily.
        connect(m_smartSdrListener, &SmartSdrApiListener::interlockBlocked,
                this, [](const QString& reason) {
            qCWarning(lcConnection)
                << "Interlock timeout reported:" << reason
                << "-- continuing TX anyway (PGXL/TGXL self-protect if needed)";
        });

        // 2026-05-20 pcap-driven proxy: forward `amplifier set 0x<h> <k>=<v>`
        // received on :4992 to the right amp's native protocol socket
        // (PGXL :9008 or TGXL :9010). This is how TGXL coordinates with
        // PGXL during its own autotune cycle in the real FlexRadio setup
        // (e.g. flex-tgxl-direct-CONTROL.pcapng @T+172.201: TGXL sends
        // "amplifier set 0x22E8213A operate=0", FLEX forwards to PGXL :9008
        // "operate=0"). Without this proxy, NereusSDR was missing the
        // mechanism real FLEX uses for amp-to-amp orchestration, forcing
        // us into the buggier startTgxlAutotune workaround.
        connect(m_smartSdrListener, &SmartSdrApiListener::amplifierSetRequested,
                this, [this](const QString& ampHandle,
                             const QString& key,
                             const QString& value) {
            if (!m_smartSdrListener) { return; }
            const QString model = m_smartSdrListener->ampModelForHandle(ampHandle);
            if (model.isEmpty()) {
                qCWarning(lcConnection)
                    << "amplifier set proxy: unknown ampHandle=" << ampHandle
                    << "(no registered client owns it); dropping"
                    << key << "=" << value;
                return;
            }
            const QString cmd = QStringLiteral("%1=%2").arg(key).arg(value);
            if (model == QStringLiteral("PowerGeniusXL")) {
                if (m_pgxlConnection && m_pgxlConnection->isConnected()) {
                    m_pgxlConnection->sendCommand(cmd);
                    qCInfo(lcConnection)
                        << "amplifier set proxy -> PGXL:9008" << cmd
                        << "(ampHandle=0x" << ampHandle << ")";
                } else {
                    qCWarning(lcConnection)
                        << "amplifier set proxy: PGXL target but PgxlConnection"
                           " not connected; dropping" << cmd;
                }
            } else if (model == QStringLiteral("TunerGeniusXL")) {
                if (m_tgxlConnection && m_tgxlConnection->isConnected()) {
                    m_tgxlConnection->sendCommand(cmd);
                    qCInfo(lcConnection)
                        << "amplifier set proxy -> TGXL:9010" << cmd
                        << "(ampHandle=0x" << ampHandle << ")";
                } else {
                    qCWarning(lcConnection)
                        << "amplifier set proxy: TGXL target but TgxlConnection"
                           " not connected; dropping" << cmd;
                }
            } else {
                qCWarning(lcConnection)
                    << "amplifier set proxy: unknown ampModel=" << model
                    << "for ampHandle=" << ampHandle
                    << "; dropping" << cmd;
            }
        });

        // TGXL autotune orchestration: interlock-granted hook.
        //
        // When we're mid-autotune and waiting for the FlexAPI interlock
        // chain to confirm TRANSMITTING (m_awaitingInterlockForAutotune),
        // this signal is our event-driven cue that TGXL has now received
        // `S0|interlock state=TRANSMITTING` and knows PTT is live. Only
        // then is it safe to send `autotune` to TGXL on :9010 -- earlier
        // and TGXL replies "no PTT in" and aborts (~350 ms after the
        // command). Bench-observed first-press failure on cold caches
        // 2026-05-19 to 2026-05-20.
        //
        // A 150 ms settle is applied between interlockGranted and the
        // autotune command. Two things need to land at TGXL before the
        // sweep cmd:
        //   (a) the TRANSMITTING S-frame on TCP :4992 (so TGXL's internal
        //       pttA flag flips true)
        //   (b) actual RF on-air from our gen1 PostGen tone (so TGXL's
        //       directional couplers see carrier amplitude > its detection
        //       threshold)
        // The :4992 socket and the :9010 socket are independent flows;
        // without a settle TGXL can briefly see "no PTT in" because the
        // autotune lands before either (a) is fully processed or (b) has
        // ramped up to detectable levels. Bench-confirmed 2026-05-20:
        // 50 ms still caused a brief "no PTT in" flash even though the
        // sweep recovered. 150 ms eliminates the flash.
        connect(m_smartSdrListener, &SmartSdrApiListener::interlockGranted,
                this, [this](const QString& source) {
            // RF-flow gate (deck item #3, 2026-05-20 ordering fix):
            // BOTH txReady AND interlockGranted must have fired before
            // TxChannel::setRunning is called. txReady means radio is in
            // TX mode; interlockGranted means amp relays are on amp
            // path. We only call setRunning when whichever signal fires
            // SECOND lands -- the first one just records its arrival.
            //
            // The grant clears m_awaitingInterlockForTx so the symmetric
            // check in the txReady wire (when it fires later) sees the
            // gate as already released and calls setRunning then.
            if (m_awaitingInterlockForTx) {
                m_awaitingInterlockForTx = false;
                if (m_txReadyReceived) {
                    // txReady already fired (rare with fast amp ACK but
                    // possible if rfDelay is unusually short). Both
                    // conditions met: start TxChannel now.
                    if (m_txChannel) {
                        qCInfo(lcConnection)
                            << "RF-flow gate: interlock TRANSMITTING confirmed"
                               " (source=" << source
                            << ") AND txReady was already received -- starting"
                               " TxChannel now (carrier hits amp path)";
                        m_txChannel->setRunning(true);
                    }
                } else {
                    // Grant arrived first (common: fast amp ACK lands
                    // before MoxController rfDelay completes). Wait for
                    // txReady; it will start TxChannel when it sees the
                    // gate as already released.
                    qCInfo(lcConnection)
                        << "RF-flow gate: interlock TRANSMITTING confirmed"
                           " (source=" << source
                        << "), waiting for txReady (race: grant arrived first)";
                }
            }

            // Autotune gate (deck item #2): TGXL autotune cmd was held
            // for 150 ms post-grant so the TRANSMITTING frame on :4992
            // and the `autotune` cmd on :9010 land on TGXL in the right
            // order without a TCP-socket race.
            if (!m_awaitingInterlockForAutotune) { return; }
            if (!m_tgxlAutotuneInProgress) {
                // Cycle was cancelled before grant; clear the gate.
                m_awaitingInterlockForAutotune = false;
                return;
            }
            m_awaitingInterlockForAutotune = false;
            qCInfo(lcConnection)
                << "TGXL autotune: interlock TRANSMITTING confirmed (source="
                << source << "), sending autotune in 150 ms";
            QTimer::singleShot(150, this, [this]() {
                if (m_tgxlAutotuneInProgress) {
                    sendTgxlAutotuneCmd();
                }
            });
        });
    }

    // TGXL autotune orchestration: PGXL standby-confirmation hook. When
    // we're in an autotune cycle waiting for PGXL to transition to
    // STANDBY (m_pgxlStandbyPending), this signal fires from
    // onPgxlStatus() when m_ampOperate flips false. That's our event-
    // driven confirmation that PGXL has acknowledged operate=0 and is no
    // longer amplifying -- now safe to engage local TUN carrier.
    connect(this, &RadioModel::ampStateChanged, this, [this]() {
        if (m_tgxlAutotuneInProgress && m_pgxlStandbyPending
            && !m_ampOperate) {
            m_pgxlStandbyPending = false;
            qCInfo(lcConnection)
                << "TGXL autotune: PGXL confirmed STANDBY, proceeding";
            continueTgxlAutotuneAfterStandby();
        }
    });

    // Phase 3P-II Task 87: wire interlock policy into MoxController.
    //
    // setInterlockPolicy: MoxController's setMox(true) consults the policy
    // immediately after the BandPlanGuard (K.2) check.
    m_moxController->setInterlockPolicy(m_txInterlockPolicy);

    // Amp state cache: amplifierChanged(bool) and ampStateChanged() together
    // carry the m_hasAmplifier / m_ampOperate snapshot. We can't connect them
    // directly (different signatures) so use a lambda that reads current values
    // via RadioModel::hasAmplifier() / ampOperate() and forwards to the slot.
    //
    // amplifierChanged fires once (present=true) when PGXL first appears.
    // ampStateChanged fires on every OPERATE-family transition.
    // Both paths flush the same onAmpStateChanged snapshot to MoxController.
    connect(this, &RadioModel::amplifierChanged,
            this, [this](bool /*present*/) {
        m_moxController->onAmpStateChanged(m_hasAmplifier, m_ampOperate);
    });
    connect(this, &RadioModel::ampStateChanged,
            this, [this]() {
        m_moxController->onAmpStateChanged(m_hasAmplifier, m_ampOperate);
    });

    // SWR cache: forward the swr argument from ampMetersChanged.
    connect(this, &RadioModel::ampMetersChanged,
            this, [this](float /*fwd*/, float swr) {
        m_moxController->onAmpSwrUpdated(swr);
    });

    connect(m_pgxlConnection, &PgxlConnection::statusUpdated,
            this, &RadioModel::onPgxlStatus);

    // Phase 3P-II Task 62: run amplifier+pair+keepalive sequence on connect.
    connect(m_pgxlConnection, &PgxlConnection::connected,
            this, &RadioModel::onPgxlConnected);

    // Phase 3P-III Task 13: aggregate PGXL state into the cross-vendor signal.
    // onPgxlStatus() already updates m_ampOperate on every statusUpdated frame;
    // we re-emit the same operate decision through the brand-neutral signal so
    // SMeterWidget (and any future consumer) does not need to know about PGXL.
    connect(m_pgxlConnection, &PgxlConnection::statusUpdated,
            this, [this](const QMap<QString, QString>& kvs) {
        if (kvs.contains(QStringLiteral("state"))) {
            const bool inOp = kvs.value(QStringLiteral("state")) == QStringLiteral("OPERATE");
            emit externalAmpOperateChanged(inOp);
        }
    });

    // Phase 3P-III Task 13: aggregate RF-Kit operate-mode and power into the
    // cross-vendor signals. The RF-Kit REST poller emits operateModeUpdated on
    // every state poll (even if unchanged) and powerUpdated on every power poll.
    connect(m_rfKitConnection.get(), &Rf2ksConnection::operateModeUpdated,
            this, [this](const QString& mode) {
        // Phase 3P-III review fix I2: only emit on actual transitions.
        // Rf2ksConnection::parseOperateMode fires on every 1 Hz poll regardless
        // of whether the mode changed; without this guard, externalAmpOperateChanged
        // would spam once per second (conflicting with the PGXL path, which is
        // already transition-only via statusUpdated frames).
        const bool inOp = (mode == QStringLiteral("OPERATE"));
        if (inOp != m_lastRfKitInOperate) {
            m_lastRfKitInOperate = inOp;
            emit externalAmpOperateChanged(inOp);
        }
    });
    connect(m_rfKitConnection.get(), &Rf2ksConnection::powerUpdated,
            this, [this](const RfKitPowerSnapshot& snap) {
        emit externalAmpFwdSwrUpdated(snap.forwardW, snap.swr);
    });
    // Publish the OPERATE -> not-OPERATE transition when the amp drops.
    // operateModeUpdated only fires from a successful poll, so a
    // disconnect while the amp was in OPERATE left m_lastRfKitInOperate
    // latched true and no consumer ever heard otherwise: the S-Meter kept
    // the 2 kW scale indefinitely.  Codex review, PR #291.
    connect(m_rfKitConnection.get(), &Rf2ksConnection::disconnected,
            this, [this]() {
        if (m_lastRfKitInOperate) {
            m_lastRfKitInOperate = false;
            emit externalAmpOperateChanged(false);
        }
    });

    // ── Phase 3J-2 H2: spot-system construction + wiring ──────────────────────
    //
    // View models first so the per-source adapter slots have live sinks the
    // moment a client emits spotReceived. Then construct each ingest client
    // with identity / endpoint defaults from AppSettings; startConnection()
    // is left to the M3 follow-up (the AutoConnect key family wires that).
    //
    // The unique_ptrs all pass `this` as the QObject parent so the dtor
    // ordering (Qt child cleanup, reverse construction order on the
    // unique_ptr stack) drains the network sockets before the model leaves
    // scope. No raw new / delete anywhere in this block.
    auto& s = AppSettings::instance();

    m_spotModel           = std::make_unique<SpotModel>(this);
    // 2026-05-12 bench fix: SpotTableModel ownership moved from
    // SpotHubDialog so it stays populated from app start.  Prior
    // behaviour: the table only existed once the user opened
    // Tools → Spot Hub, so spots from auto-connected sources were
    // dropped on the floor until the dialog was open AND a fresh
    // connect happened.  Symptom: "auto-start spots don't appear
    // until I disconnect+reconnect every source."
    m_spotTableModel      = std::make_unique<SpotTableModel>(this);
    m_freeDvStationModel  = std::make_unique<FreeDVStationModel>(this);
    m_rxDecodeModel       = std::make_unique<RxDecodeModel>(/*maxSize*/ 200, this);
    m_dxccColorProvider   = std::make_unique<DxccColorProvider>(this);

    // 2026-05-12 bench fix: seed FreeDVStationModel::setOurGridSquare
    // from the User/GridSquare AppSettings key.  Without this the
    // model's m_ourGrid stays empty and applyDistanceHeading
    // short-circuits at `m_ourGrid.size() < 4`, zeroing every
    // station's distance + heading in the FreeDV Reporter dialog.
    // Both User/* and the legacy FreeDvReporter/GridSquare are
    // checked so existing users with the per-source key set don't
    // need to re-enter into Settings.
    {
        QString grid = s.value(QStringLiteral("User/GridSquare")).toString();
        if (grid.isEmpty()) {
            grid = s.value(QStringLiteral("FreeDvReporter/GridSquare")).toString();
        }
        if (!grid.isEmpty()) {
            m_freeDvStationModel->setOurGridSquare(grid);
        }
    }

    // DX cluster: host / port / callsign defaults from AppSettings
    // ("DxCluster/{Host,Port,Callsign}"). startConnection() is deferred to
    // M3. The same DxClusterClient class drives both this and m_rbn (which
    // tags every spot with source="RBN" because the spotter callsign has
    // an "-#" suffix; see DxClusterClient.h Modification-history block).
    m_dxCluster = std::make_unique<DxClusterClient>(this);

    // Reverse Beacon Network: second DxClusterClient instance pointing at
    // telnet.reversebeacon.net by default. Identity / port read from
    // "Rbn/{Host,Port,Callsign}".
    m_rbn = std::make_unique<DxClusterClient>(this);

    m_wsjtx          = std::make_unique<WsjtxClient>(this);
    m_spotCollector  = std::make_unique<SpotCollectorClient>(this);
    m_pota           = std::make_unique<PotaClient>(this);

    m_freeDvReporter = std::make_unique<FreeDVReporterClient>(this);
    m_freeDvReporter->setIdentity(
        s.value(QStringLiteral("FreeDvReporter/Callsign"),
                QString()).toString(),
        s.value(QStringLiteral("FreeDvReporter/GridSquare"),
                QString()).toString(),
        s.value(QStringLiteral("FreeDvReporter/Message"),
                QString()).toString(),
        QStringLiteral("NereusSDR ") + QStringLiteral(NEREUSSDR_VERSION));
    {
        const QString serverUrl = s.value(
            QStringLiteral("FreeDvReporter/ServerUrl"),
            QStringLiteral("wss://qso.freedv.org/socket.io/?EIO=4&transport=websocket")
        ).toString();
        if (!serverUrl.isEmpty()) {
            m_freeDvReporter->setServerUrl(serverUrl);
        }
    }

    // 2026-05-12 bench: FreeDV Reporter freq-publish throttle timer.
    // Single-shot, restarted on every slice frequency change.  Expiry
    // (kFreedvFreqDwellMs = 7 s) calls flushFreedvFrequencyDwell which
    // emits the cached pending freq.  See member declaration in
    // RadioModel.h for the full throttle policy.
    m_freedvFreqDwellTimer = new QTimer(this);
    m_freedvFreqDwellTimer->setSingleShot(true);
    m_freedvFreqDwellTimer->setInterval(kFreedvFreqDwellMs);
    connect(m_freedvFreqDwellTimer, &QTimer::timeout,
            this, &RadioModel::flushFreedvFrequencyDwell);

    m_pskReporter = std::make_unique<PskReporterClient>(this);
    m_pskReporter->setIdentity(
        s.value(QStringLiteral("PskReporter/Callsign"),
                QString()).toString(),
        s.value(QStringLiteral("PskReporter/GridSquare"),
                QString()).toString(),
        QStringLiteral("NereusSDR ") + QStringLiteral(NEREUSSDR_VERSION));

    // Per-source adapter slots. Auto-connection (sender + receiver both on
    // the main thread) gives DirectConnection, so the spot lands in
    // SpotModel synchronously on the emitter's call.
    connect(m_dxCluster.get(),      &DxClusterClient::spotReceived,
            this, &RadioModel::onClusterSpotReceived);
    connect(m_rbn.get(),            &DxClusterClient::spotReceived,
            this, &RadioModel::onRbnSpotReceived);
    connect(m_wsjtx.get(),          &WsjtxClient::spotReceived,
            this, &RadioModel::onWsjtxSpotReceived);
    connect(m_spotCollector.get(),  &SpotCollectorClient::spotReceived,
            this, &RadioModel::onSpotCollectorSpotReceived);
    connect(m_pota.get(),           &PotaClient::spotReceived,
            this, &RadioModel::onPotaSpotReceived);
    connect(m_freeDvReporter.get(), &FreeDVReporterClient::spotReceived,
            this, &RadioModel::onFreeDvReporterSpotReceived);
    connect(m_pskReporter.get(),    &PskReporterClient::spotReceived,
            this, &RadioModel::onPskReporterSpotReceived);

    // 2026-05-12 bench fix: also feed the shared SpotTableModel from
    // app start so the Spot List tab populates regardless of whether
    // SpotHubDialog is open.  Was previously per-dialog in
    // SpotHubDialog::buildSpotListTab's `wireClient` lambdas; moving
    // it here means auto-connected sources fill the table before the
    // user even opens the dialog.  Direct lambda capture of the
    // table model pointer keeps the addSpot call same-thread (both
    // emitter and receiver live on the main thread).
    auto wireSpotTable = [this](auto* client) {
        if (!client) { return; }
        using ClientType = std::remove_pointer_t<decltype(client)>;
        connect(client, &ClientType::spotReceived,
                this, [this](const DxSpot& spot) {
                    if (m_spotTableModel) {
                        m_spotTableModel->addSpot(spot);
                    }
                });
    };
    wireSpotTable(m_dxCluster.get());
    wireSpotTable(m_rbn.get());
    wireSpotTable(m_wsjtx.get());
    wireSpotTable(m_spotCollector.get());
    wireSpotTable(m_pota.get());
    wireSpotTable(m_freeDvReporter.get());
    wireSpotTable(m_pskReporter.get());

    // ── Phase 3R-bridge: RADE Path B (sync-only) rx_report upload ─────────
    // Ported from freedv-gui src/main.cpp:1971-1996 [@77e793a]
    // (MainFrame::OnTimer's FREEDV_MODE_RADE && syncState else-if).
    //
    // Path A (callsign-decoded via EOO text channel) is already driven
    // by onRadeTextDecoded calling FreeDVReporterClient::sendRxReport.
    // Path B handles the long stretches where RADE has sync but the
    // remote operator has not yet sent an EOO frame -- without it,
    // qso.freedv.org would not know we are receiving anything and our
    // row's "Last RX SNR" column stays blank.
    //
    // Bridge is permanently enabled at construction (the upstream gate
    // is the operator's reportingEnabled checkbox -- we model that as
    // "reporter has been started", i.e. m_freeDvReporter is connected;
    // the bridge re-checks isConnected() inside shouldEmitPathB_).
    m_radeReporterBridge = std::make_unique<FreeDVRadeReporterBridge>(
        m_freeDvReporter.get(), m_pskReporter.get(), this);
    m_radeReporterBridge->setReportingEnabled(true);
    connect(this, &RadioModel::radeSyncChanged,
            m_radeReporterBridge.get(),
            &FreeDVRadeReporterBridge::onRadeSyncChanged);
    connect(this, &RadioModel::radeSnrChanged,
            m_radeReporterBridge.get(),
            &FreeDVRadeReporterBridge::onRadeSnrChanged);
    if (m_moxController) {
        connect(m_moxController, &MoxController::moxStateChanged,
                m_radeReporterBridge.get(),
                &FreeDVRadeReporterBridge::onMoxStateChanged);
    }

    // FreeDV Reporter station signals drive FreeDVStationModel directly.
    // The model's onStationAdded/Updated/Removed slots stamp distance + heading
    // when our grid is set, then re-emit so the dialog and any other
    // subscribers see the enriched FreeDVStation.
    connect(m_freeDvReporter.get(), &FreeDVReporterClient::stationAdded,
            m_freeDvStationModel.get(), &FreeDVStationModel::onStationAdded);
    connect(m_freeDvReporter.get(), &FreeDVReporterClient::stationUpdated,
            m_freeDvStationModel.get(), &FreeDVStationModel::onStationUpdated);
    connect(m_freeDvReporter.get(), &FreeDVReporterClient::stationRemoved,
            m_freeDvStationModel.get(), &FreeDVStationModel::onStationRemoved);

    // 2026-05-12 (PR #238 bench follow-up): VFO-flag "active talker"
    // wire from FreeDV Reporter.
    //
    // The flag's RADE row shows a decoded peer callsign.  Primary
    // source is librade's EOO frame (RadeChannel::rxTextDecoded ->
    // onRadeTextDecoded -> slice->setLastRadeRxCallsign), which only
    // fires on a clean EOO at end-of-TX so the flag stays empty
    // until the speaker keys down.  Fallback source is qso.freedv.org's
    // tx_report event stream: any station that flips
    // transmitting=true on our currently-tuned dial frequency
    // surfaces on the flag immediately, replacing whatever was
    // there before.  This is the "latest transmitter wins"
    // pattern the bench operator asked for during PR #238
    // testing — "show who's actively on the channel right now".
    //
    // 2026-05-12 v2: removed the "don't overwrite if already set"
    // guard from v1.  v1 was sticky-on-first-write — if station A
    // transmitted first, the flag pinned to A and never updated
    // when station B started keying.  v2 lets every
    // transmitting=true event win so the flag tracks who's
    // actually on the air, not who first claimed the channel.
    // EOO decodes likewise overwrite (they ALSO go through
    // setLastRadeRxCallsign), so the latest authoritative source
    // is always on the flag.
    //
    // 2026-05-12 v2: tolerance bumped 1500 -> 3000 Hz to forgive
    // small VFO offsets between the remote operator's published
    // dial freq and our local VFO.  Different rigs round
    // differently and ±1.5 kHz was missing legitimate same-channel
    // pairs at the bench.
    //
    // Sticky semantics: once set, the callsign stays until
    // overwritten or setDspMode leaves RADE.  setDspMode's
    // RADE -> non-RADE branch clears m_lastRadeRxCallsign at
    // SliceModel.cpp:215-218.
    connect(m_freeDvReporter.get(), &FreeDVReporterClient::stationUpdated,
            this, [this](const QString& /*sid*/, const FreeDVStation& info) {
                if (!info.transmitting || info.callsign.isEmpty()) return;
                if (info.frequencyHz == 0) return;
                SliceModel* slice = m_activeSlice;
                if (!slice) return;
                const auto m = slice->dspMode();
                if (m != DSPMode::RADE_U && m != DSPMode::RADE_L) {
                    return;  // Flag SNR row is only visible in RADE.
                }
                const qint64 deltaHz =
                    qAbs(static_cast<qint64>(info.frequencyHz)
                         - static_cast<qint64>(slice->frequency()));
                constexpr qint64 kFreqMatchToleranceHz = 3000;
                if (deltaHz > kFreqMatchToleranceHz) return;
                // Latest-wins: always replace, regardless of whether
                // a previous fallback / EOO callsign is present.
                // SliceModel's idempotent setter early-returns when
                // the new value matches the old, so re-publishing
                // the same callsign is a no-op.
                slice->setLastRadeRxCallsign(info.callsign);
                // Off by default; enable for bench triage with
                //   QT_LOGGING_RULES="nereus.dsp.debug=true"
                qCDebug(lcDsp).noquote()
                    << QStringLiteral("FreeDV-Reporter flag fallback: "
                                      "set callsign=%1 on slice (freq=%2 Hz, "
                                      "deltaHz=%3)")
                           .arg(info.callsign)
                           .arg(slice->frequency())
                           .arg(deltaHz);
            });

    // Phase 3R K-bench: push current freq + TX state when the FreeDV
    // Reporter connects. Without this, the reporter knows our identity
    // (callsign / grid / message from setIdentity) but never our
    // operating frequency — we appear on the dashboard at freq=0 until
    // the user moves the VFO and triggers the frequencyChanged push.
    connect(m_freeDvReporter.get(), &FreeDVReporterClient::connected,
            this, [this]() {
                qCInfo(lcDsp) << "FreeDVReporter: connected signal fired";
                if (!m_freeDvReporter || !m_activeSlice) {
                    qCWarning(lcDsp)
                        << "FreeDVReporter connected but"
                        << (m_freeDvReporter ? "no active slice"
                                             : "client gone");
                    return;
                }
                const quint64 freqHz =
                    static_cast<quint64>(m_activeSlice->frequency());
                qCInfo(lcDsp) << "FreeDVReporter: pushing initial freq="
                              << freqHz << "Hz";
                m_freeDvReporter->setFrequency(freqHz);

                // Phase 3J-1 closeout follow-up (2026-05-12): hide our
                // station from the dashboard unless we connected while
                // already in RADE.  Otherwise we'd flash visible for
                // one tick on connect before the dspModeChanged handler
                // hides us.
                updateFreedvReporterVisibility();
                // 2026-05-12 bench: seed the dwell-throttle baseline so
                // subsequent slice.frequencyChanged calls measure delta
                // against the connect-time freq.  Without this seed the
                // first VFO move would always trigger the fast-path
                // (delta from 0 -> band freq is huge), bypassing the
                // dwell on what is usually a deliberate first tune.
                m_freedvLastPublishedHz = freqHz;
                if (m_freedvFreqDwellTimer) {
                    m_freedvFreqDwellTimer->stop();
                }
                const DSPMode m = m_activeSlice->dspMode();
                const QString modeStr =
                    (m == DSPMode::RADE_U || m == DSPMode::RADE_L)
                        ? QStringLiteral("RADEV1")
                        : QString();
                m_freeDvReporter->setTransmitting(false, modeStr);
            });

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
    // Phase 4 Agent 4A of issue #167 (K2GX safety hotfix) introduced the
    // Thetis-canonical dBm-target chain (replacing a previous linear
    // `wire = clamp(int(255*f*swrProtect),0,255)` formula that had no
    // per-band PA-gain compensation).  Issue #202 deep-fix reverted a
    // mistaken SWR-topology comment that previously claimed the wire
    // byte should NOT see SWR foldback.  The actual Thetis topology
    // (NetworkIO.cs:201-211 [v2.10.3.13]) puts SWR foldback on the
    // wire byte:
    //
    //   wire_byte = (int)(255 * clamp(audio_volume * 1.02, 0, 1) * _swr_protect)
    //               From Thetis audio.cs:262-271 [v2.10.3.13]
    //               `NetworkIO.SetOutputPower((float)(value * 1.02))`
    //               and NetworkIO.cs:201-211 [v2.10.3.13] which clamps
    //               and applies _swr_protect.
    //
    //   iq_gain   = audio_volume * Audio.HighSWRScale
    //               From Thetis cmaster.cs:1115-1119 [v2.10.3.13].
    // Upstream tags preserved: //MW0LGE (from cited cmaster.cs:1114) [v2.10.3.15]
    //               HighSWRScale is set to 1.0 once at console.cs:29194
    //               [v2.10.3.13] and never reassigned anywhere in
    //               baseline Thetis — IQ-side path is effectively no-op.
    //
    // Wire+IQ composition lives in RadioModel::pumpAudioVolume (one
    // helper), wired below to TransmitModel::audioVolumeChanged so every
    // setPowerUsingTargetDbm callsite + future audio_volume mutator
    // pumps both paths uniformly.
    //
    // Pre-hotfix: ANAN-8000DLE 80m TUN at slider=50 produced wire_byte=127
    // (=> ~300W on a 200W radio).  Post-hotfix: wire_byte=49 (=> ~85W).
    // Ratio matches the band's 50.5 dB PA gain compensation.
    connect(&m_transmitModel, &TransmitModel::powerChanged, this,
            [this](int /*power*/) {
        if (m_transmitModel.isTune()) { return; }
        if (!m_connection)            { return; }
        // Active-profile resolution.  Without a loaded PaProfileManager
        // (MAC scope not set, or first-launch state before factory regen),
        // activeProfile() returns nullptr and we silently no-op — same
        // contract as MicProfileManager when not yet loaded.  The user
        // sees no wire byte change until the profile bank is live.
        if (!m_paProfileManager)      { return; }
        const PaProfile* activeProfile =
            m_paProfileManager->activeProfile();
        if (!activeProfile)           { return; }

        const SliceModel* const txSlice = txBoundSlice();
        const Band currentBand = txSlice
                                    ? bandFromFrequency(txSlice->frequency())
                                    : m_lastBand;

        // Phase 3C deep-parity wrapper: computes audio_volume + applies
        // ATT-on-TX safety gate (PS-active dormant until 3M-4).  txMode 0
        // (normal): bFromTune=false, bTwoTone=false.
        // Issue #175 Task 4: thread connected model so HL2 sub-step path
        // resolves correctly (txMode 0 path is non-HL2-affected, but
        // passing the model keeps the call site uniform with TUN path).
        const auto result = m_transmitModel.setPowerUsingTargetDbm(
            *activeProfile, currentBand, /*bSetPower=*/true,
            /*bFromTune=*/false, /*bTwoTone=*/false,
            m_hardwareProfile.model);

        // Wire byte + IQ scalar pump now happens inside pumpAudioVolume,
        // wired below to TransmitModel::audioVolumeChanged.  setPowerUsingTargetDbm
        // emits that signal at TransmitModel.cpp:1129, which fires the
        // listener synchronously (Qt::AutoConnection on same thread).
        (void)result;
    });

    // ── #202 deep-fix: Audio.RadioVolume setter analogue ─────────────────────
    //
    // Connect TransmitModel::audioVolumeChanged → RadioModel::pumpAudioVolume
    // so every call to setPowerUsingTargetDbm (drive slider, TUNE-on, TUN-off
    // restore, two-tone) and any future audio_volume mutator pumps wire byte
    // + IQ scalar uniformly.  Mirrors Thetis audio.cs:262-271 [v2.10.3.13]
    // setter side-effects.
    connect(&m_transmitModel, &TransmitModel::audioVolumeChanged,
            this, &RadioModel::pumpAudioVolume);

    // Connect TransmitModel::swrProtectFactorChanged → re-pump current
    // audio_volume through the new SWR factor.  Mirrors Thetis
    // console.cs:26102-26109 [v2.10.3.13]:
    //   if (_swr_wind_back_power && swrprotection && old_swr_protect != NetworkIO.SWRProtect)
    //   {
    //       // setting SWRProtect does nothing unless power is changed,
    //       // RadioVolume is the only code that uses SWRProtect using
    //       // NetworkIO.SetOutputPower.
    //       Audio.RadioVolume = Audio.RadioVolume;  // self-assign re-emits
    //   }
    // The NereusSDR cache (m_lastAudioVolume, updated inside pumpAudioVolume)
    // stands in for Thetis's `radio_volume` backing field.
    connect(&m_transmitModel, &TransmitModel::swrProtectFactorChanged,
            this, [this](float /*factor*/) {
        pumpAudioVolume(m_lastAudioVolume);
    });

    // (Codex-flagged duplicate audioVolumeChanged listener removed in 67298ff
    // follow-up.  The single canonical pump is RadioModel::pumpAudioVolume,
    // wired at line 965 above.  pumpAudioVolume is a byte-for-byte port of
    // Thetis NetworkIO.SetOutputPower at NetworkIO.cs:201-211 [v2.10.3.13]
    // which applies SWR foldback to the wire byte — not the IQ scalar.
    // The deleted second listener inverted that topology and, because Qt
    // ran it after the first connection, won the race and removed SWR
    // foldback from the wire byte.  Test assertion in
    // tst_radio_model_drive_path::swrFoldback_appliesToIqNotWireByte
    // codifies the correct topology.)

    // Bench-reported #167 follow-up: power meters stick after un-key.
    // Root cause: handlePaTelemetry only fires while the radio is sending
    // PA telemetry (typically only during transmit).  When transmit ends
    // the telemetry pump stops and RadioStatus retains the last-known
    // forward / reflected / SWR / PA-current values, so subscribed labels
    // and meters keep displaying the last sample.  On the falling edge we
    // explicitly zero the power-related telemetry so every subscriber sees
    // a clean idle reading.  PA temperature is left alone (it's a slow
    // physical quantity and the last reading is still meaningful post-key).
    //
    // Subscribed to MoxController::moxStateChanged because that's the
    // authoritative wire-level TX boundary.  TransmitModel's m_mox / m_tune
    // flags are orphan state — never set true by any code path — so
    // subscribing to those signals would never fire.  MoxController fires
    // moxStateChanged(false) at the END of every TX→RX walk (both MOX
    // un-key and TUNE release), which is exactly when we want to zero.
    if (m_moxController) {
        connect(m_moxController, &MoxController::moxStateChanged, this,
                [this](bool active) {
            if (active) { return; }   // rising-edge: telemetry pump takes over
            m_radioStatus.setForwardPower(0.0);
            m_radioStatus.setReflectedPower(0.0);
            m_radioStatus.setExciterPowerMw(0);
            m_radioStatus.setPaCurrent(0.0);
        });

        // ── Phase 3F Sub-Epic I closeout, defect F3 ─────────────────────
        //
        // MOX is a codec input (CodecContext::mox) but nothing recomputed
        // the assignment when it moved, so on a PureSignal key-down the
        // radio stopped streaming the extra DDCs while every slice went on
        // reporting the ddcIndex it had before the key. Both edges, because
        // un-keying is what restores them.
        connect(m_moxController, &MoxController::moxStateChanged, this,
                [this](bool) { refreshDdcAssignmentForRadioState(); });

        // ── The MOX audio gate, which had never been connected ───────────
        //
        // AudioEngine has carried setMoxState() and the rxBlockReady gate
        // since 3M-1b E.4, and AudioEngine.h:80 says "Phase L (RadioModel
        // integration) wires MoxController::moxStateChanged -> setMoxState
        // via signal/slot". That wire was never made, so m_moxActive stayed
        // false for the life of the process and the gate only ever ran in
        // tst_audio_engine_rx_leak_during_mox, via setMoxStateForTest.
        //
        // The effect on the air: RX audio was never muted on key-down. It
        // is audible because PureSignal retunes DDC0 from 48 kHz to 192 kHz
        // for the duration of a transmission (the DDCAssign above), so what
        // the RX chain demodulates while keyed is not the band any more.
        // Reported on the 2026-07-27 G2E bench as noise on MOX that Thetis
        // does not produce; Thetis mutes RX on key-down
        // (console.cs:27650-27771 [v2.10.3.15] drops RX1/RX1S/RX2 from the
        // mix on every MOX transition).
        //
        // Both edges, and TUNE too: MoxController::setTune calls
        // setMox(true/false), so it emits this same signal.
        if (m_audioEngine) {
            connect(m_moxController, &MoxController::moxStateChanged,
                    m_audioEngine, &AudioEngine::setMoxState);
        }
    }

    // Active focus is listening/UI state. AudioEngine keys its MOX withdrawal
    // on the stable TX-bound slice id, so this notification deliberately has
    // no authority to move the gate while keyed.
    if (m_audioEngine) {
        connect(this, &RadioModel::activeSliceChanged,
                m_audioEngine, &AudioEngine::onActiveSliceChanged);
    }

    // ── Phase 3F Sub-Epic C Task 6: TxSliceArbiter per-MAC scope wiring ───
    // currentRadioChanged is emitted from onConnectionStateChanged once the
    // hardware profile is loaded and m_lastRadioInfo is populated (see
    // ConnectionState::Connected branch).  Push the MAC into the arbiter
    // and call load() to restore the persisted TxBoundSliceId for this
    // radio.  load() is a no-op if MAC is empty (default-constructed
    // RadioInfo from setLastRadioInfoForTest path).
    //
    // The lambda runs on the main thread (RadioModel + arbiter both live
    // here), so AppSettings access is safe.  load() may call
    // requestHandoff() which flips txSlice flags on SliceModel instances;
    // by the time currentRadioChanged fires, the slice list is already
    // populated by addSlice() in onConnected() (which runs earlier on the
    // same callstack inside onConnectionStateChanged).
    connect(this, &RadioModel::currentRadioChanged, this,
            [this](const NereusSDR::RadioInfo& info) {
        if (m_txSliceArbiter) {
            m_txSliceArbiter->setMacAddress(info.macAddress);
            m_txSliceArbiter->load();
        }
    });

    // ── Phase 3F Sub-Epic F Task 5: per-ADC WidebandFftEngine construction ─
    // One engine per ADC slot (2-ADC ceiling for current SKUs).  Default
    // 122.88 MHz ADC rate; updated when the P2 codec context updates
    // (Sub-Epic F polish T7-T10).  Parented to RadioModel so they tear
    // down with the model.
    for (int i = 0; i < 2; ++i) {
        m_widebandFftEngines[i] = new NereusSDR::WidebandFftEngine(this);
        m_widebandFftEngines[i]->setAdcSampleRateHz(122880000.0);
    }
}

RadioModel::~RadioModel()
{
    teardownConnection();
    qDeleteAll(m_slices);
    qDeleteAll(m_panadapters);
}

// ── Phase 3J-2 H2: spot-adapter slot implementations ────────────────────────
//
// Each per-source slot translates a DxSpot into the QMap<QString,QString>
// kvs shape SpotModel::applySpotStatus consumes (TCI-style sink). The kvs
// keys come from the plan task spec; SpotModel's applySpotStatus dispatches
// 12 known keys verbatim and stores callsign / rx_freq / tx_freq / mode /
// color / background_color / source / spotter_callsign / comment /
// timestamp / lifetime_seconds / priority. Each adapter reads its own
// <Source>SpotLifetimeSec AppSettings key (default 1800 s for slow sources
// like cluster / SpotCollector, 120 s for WSJT-X-style real-time decodes
// per AetherSDR DxClusterDialog.cpp:1201 [@0cd4559]) and pre-stamps the
// per-source <Source>SpotColor.
//
// Mode hint: SpotTableModel::extractMode parses comments for known mode
// tokens (CW / SSB / USB / LSB / AM / FM / FT8 / FT4 / JS8 / RTTY / PSK /
// PSK31 / PSK63 / OLIVIA / JT65 / JT9 / SAM / NFM / DIGU / DIGL). When the
// client already supplied DxSpot::source (which it does for all seven
// clients in NereusSDR), the per-source label trumps the comment heuristic
// for the kvs `source` key.
//
// The kvs map is the canonical TCI shape; the in-house adapter writes
// match AetherSDR TciProtocol.cpp:972-976 [@0cd4559] (the upstream
// reference for the same key set).

namespace {

// Build a kvs map shared across all seven adapter slots. The source label
// is taken from the DxSpot rather than the slot, because the FreeDV
// Reporter dual-feed (FreeDVReporterClient.h:124-128 [@77e793a-derived])
// synthesizes spots whose source field is already pre-stamped, and the
// DxClusterClient port (DxClusterClient.h:36-44, NereusSDR addition)
// promotes "Cluster" to "RBN" when the spotter callsign carries the
// -# suffix.
QMap<QString, QString> kvsFromSpot(const NereusSDR::DxSpot& spot,
                                   int defaultLifetimeSec,
                                   const QString& defaultColor)
{
    using NereusSDR::SpotTableModel;
    QMap<QString, QString> kvs;
    kvs[QStringLiteral("callsign")]         = spot.dxCall;
    kvs[QStringLiteral("rx_freq")]          = QString::number(spot.freqMhz, 'f', 4);
    kvs[QStringLiteral("tx_freq")]          = QString::number(spot.freqMhz, 'f', 4);
    {
        const QString mode = SpotTableModel::extractMode(spot.comment);
        if (!mode.isEmpty()) {
            kvs[QStringLiteral("mode")] = mode;
        }
    }
    kvs[QStringLiteral("source")]           = spot.source;
    kvs[QStringLiteral("spotter_callsign")] = spot.spotterCall;
    kvs[QStringLiteral("comment")]          = spot.comment;
    kvs[QStringLiteral("timestamp")]        = QString::number(
        QDateTime::currentSecsSinceEpoch());
    {
        const int life = spot.lifetimeSec > 0
                           ? spot.lifetimeSec
                           : defaultLifetimeSec;
        kvs[QStringLiteral("lifetime_seconds")] = QString::number(life);
    }
    if (!spot.color.isEmpty()) {
        kvs[QStringLiteral("color")] = spot.color;
    } else if (!defaultColor.isEmpty()) {
        kvs[QStringLiteral("color")] = defaultColor;
    }
    return kvs;
}

}  // namespace

void RadioModel::onClusterSpotReceived(const DxSpot& spot)
{
    if (!m_spotModel) { return; }
    auto& s = AppSettings::instance();
    const int lifetime = s.value(QStringLiteral("DxClusterSpotLifetimeSec"),
                                 1800).toInt();
    const QString color = s.value(QStringLiteral("DxClusterSpotColor"),
                                  QStringLiteral("#D2B48C")).toString();
    // Phase 3J-1 closeout follow-up (2026-05-12): route through SpotModel
    // dedup so re-emits of the same callsign / freq from the cluster +
    // overlapping RBN feeds don't spam the list.  60 s window default;
    // cluster lifetime stays at 30 min so the spot persists in the UI.
    const int idx = m_spotModel->dedupIndexFor(spot.dxCall, spot.freqMhz);
    m_spotModel->applySpotStatus(idx, kvsFromSpot(spot, lifetime, color));
}

void RadioModel::onRbnSpotReceived(const DxSpot& spot)
{
    if (!m_spotModel) { return; }
    auto& s = AppSettings::instance();
    const int lifetime = s.value(QStringLiteral("RbnSpotLifetimeSec"),
                                 1800).toInt();
    const QString color = s.value(QStringLiteral("RbnSpotColor"),
                                  QStringLiteral("#4488FF")).toString();
    const int idx = m_spotModel->dedupIndexFor(spot.dxCall, spot.freqMhz);
    m_spotModel->applySpotStatus(idx, kvsFromSpot(spot, lifetime, color));
}

void RadioModel::onWsjtxSpotReceived(const DxSpot& spot)
{
    if (!m_spotModel) { return; }
    auto& s = AppSettings::instance();
    // WSJT-X spots are real-time and dense; AetherSDR's
    // DxClusterDialog.cpp:1201 [@0cd4559] defaults to 120 s lifetime for
    // the dialog's UI, so reuse that here.
    const int lifetime = s.value(QStringLiteral("WsjtxSpotLifetimeSec"),
                                 120).toInt();
    const QString color = s.value(QStringLiteral("WsjtxSpotColor"),
                                  QStringLiteral("#00FF00")).toString();
    const int idx = m_spotModel->dedupIndexFor(spot.dxCall, spot.freqMhz);
    m_spotModel->applySpotStatus(idx, kvsFromSpot(spot, lifetime, color));

    // RxDecodeModel dual-feed: every WSJT-X decode also lands in the
    // "what my radio just heard" sink. WsjtxClient does not emit a separate
    // decodeReceived signal; the spotReceived payload is the source for
    // both sinks (see WsjtxClient.cpp:218-240 [v3J-2-B4]: single emit).
    if (m_rxDecodeModel) {
        RxDecode dec;
        dec.callsign = spot.dxCall;
        dec.freqMhz  = spot.freqMhz;
        dec.snr      = spot.snr;
        dec.mode     = SpotTableModel::extractMode(spot.comment);
        dec.source   = QStringLiteral("WSJT-X");
        dec.utcTime  = QDateTime::currentDateTimeUtc();
        dec.payload  = spot.comment;
        m_rxDecodeModel->addDecode(dec);
    }

    // 2026-05-12 bench fix: source-first port from freedv-gui.  Every
    // WSJT-X decode also gets queued into PSK Reporter, matching
    // upstream main.cpp:1959-1966 [@77e793a] where addReceiveRecord
    // fires on every reporter in m_reporters[] (PSK + FreeDV + CSV).
    // Gated on PskReporterClient::isAutoSendActive() (the 5-min auto-
    // send timer being armed) — analogous to freedv-gui only putting
    // PskReporter in m_reporters[] when pskReporterEnabled is true.
    // Mode string comes from the WSJT-X spot comment field (FT8/FT4/
    // JS8/JT9/etc.) parsed by SpotTableModel::extractMode.
    if (m_pskReporter && m_pskReporter->isAutoSendActive()
        && !spot.dxCall.isEmpty()) {
        const QString mode =
            SpotTableModel::extractMode(spot.comment);
        m_pskReporter->reportDecode(
            spot.dxCall,
            mode.isEmpty() ? QStringLiteral("FT8") : mode,
            spot.freqMhz,
            spot.snr);
    }
}

void RadioModel::onSpotCollectorSpotReceived(const DxSpot& spot)
{
    if (!m_spotModel) { return; }
    auto& s = AppSettings::instance();
    const int lifetime = s.value(QStringLiteral("SpotCollectorSpotLifetimeSec"),
                                 1800).toInt();
    const QString color = s.value(QStringLiteral("SpotCollectorSpotColor"),
                                  QStringLiteral("#B0C4DE")).toString();
    const int idx = m_spotModel->dedupIndexFor(spot.dxCall, spot.freqMhz);
    m_spotModel->applySpotStatus(idx, kvsFromSpot(spot, lifetime, color));
}

void RadioModel::onPotaSpotReceived(const DxSpot& spot)
{
    if (!m_spotModel) { return; }
    auto& s = AppSettings::instance();
    const int lifetime = s.value(QStringLiteral("PotaSpotLifetimeSec"),
                                 3600).toInt();
    const QString color = s.value(QStringLiteral("PotaSpotColor"),
                                  QStringLiteral("#FFFF00")).toString();
    const int idx = m_spotModel->dedupIndexFor(spot.dxCall, spot.freqMhz);
    m_spotModel->applySpotStatus(idx, kvsFromSpot(spot, lifetime, color));
}

void RadioModel::onFreeDvReporterSpotReceived(const DxSpot& spot)
{
    if (!m_spotModel) { return; }
    auto& s = AppSettings::instance();
    const int lifetime = s.value(QStringLiteral("FreeDvSpotLifetimeSec"),
                                 1800).toInt();
    const QString color = s.value(QStringLiteral("FreeDvSpotColor"),
                                  QStringLiteral("#FF8C00")).toString();
    const int idx = m_spotModel->dedupIndexFor(spot.dxCall, spot.freqMhz);
    m_spotModel->applySpotStatus(idx, kvsFromSpot(spot, lifetime, color));
}

void RadioModel::onPskReporterSpotReceived(const DxSpot& spot)
{
    if (!m_spotModel) { return; }
    auto& s = AppSettings::instance();
    const int lifetime = s.value(QStringLiteral("PskReporterSpotLifetimeSec"),
                                 1800).toInt();
    const QString color = s.value(QStringLiteral("PskReporterSpotColor"),
                                  QStringLiteral("#FF00FF")).toString();
    const int idx = m_spotModel->dedupIndexFor(spot.dxCall, spot.freqMhz);
    m_spotModel->applySpotStatus(idx, kvsFromSpot(spot, lifetime, color));
}

// ── Phase 3J-2 + 3R M3: spot-client auto-start state restore ───────────────
//
// Reads each per-source AutoConnect / AutoStart key from AppSettings and,
// when True, calls the corresponding start method with the persisted
// identity / port / interval params. MainWindow invokes this once at
// startup after RadioModel is fully wired (sibling to tryAutoReconnect
// for the radio connection itself).
//
// Key shape mirrors SpotHubDialog F2 (flat PascalCase, e.g.
// DxClusterAutoConnect / DxClusterHost / DxClusterPort / DxClusterCallsign).
// FreeDV Reporter identity / server URL is already plumbed by RadioModel's
// constructor (RadioModel.cpp:936-953); the restore here only needs to
// flip the WebSocket on. PSK Reporter is send-only; restore is a no-op.
//
// NereusSDR-original. AetherSDR splits this work between MainWindow's
// startup and per-source dialog handlers; the NereusSDR shape consolidates
// the read-and-start loop onto RadioModel so MainWindow's startup path
// stays a single call site.
void RadioModel::restoreSpotClientAutoStartState()
{
    auto& s = AppSettings::instance();
    auto isTrue = [&s](const QString& key) {
        return s.value(key, QStringLiteral("False")).toString()
               == QStringLiteral("True");
    };

    // Post-3J-2 UX fix: identity fall-back chain. The SpotHub Settings
    // tab writes a canonical User/Callsign + User/GridSquare pair. Each
    // per-source loader first checks its own legacy key, then falls
    // back to the canonical key. Loaders that need identity skip the
    // auto-start when no callsign is configured anywhere.
    const QString userCallsign =
        s.value(QStringLiteral("User/Callsign")).toString();
    const QString userGrid =
        s.value(QStringLiteral("User/GridSquare")).toString();
    auto resolveCall = [&s, &userCallsign](const QString& perSourceKey) {
        QString v = s.value(perSourceKey).toString();
        if (v.isEmpty()) v = userCallsign;
        return v;
    };

    // DxCluster
    if (m_dxCluster && isTrue(QStringLiteral("DxClusterAutoConnect"))) {
        m_dxCluster->connectToCluster(
            s.value(QStringLiteral("DxClusterHost"),
                    QStringLiteral("dxc.nc7j.com")).toString(),
            static_cast<quint16>(
                s.value(QStringLiteral("DxClusterPort"), 7300).toInt()),
            resolveCall(QStringLiteral("DxClusterCallsign")));
    }

    // RBN (same DxClusterClient class, different keys / default host).
    if (m_rbn && isTrue(QStringLiteral("RbnAutoConnect"))) {
        m_rbn->connectToCluster(
            s.value(QStringLiteral("RbnHost"),
                    QStringLiteral("telnet.reversebeacon.net")).toString(),
            static_cast<quint16>(
                s.value(QStringLiteral("RbnPort"), 7000).toInt()),
            resolveCall(QStringLiteral("RbnCallsign")));
    }

    // WSJT-X (UDP bind on the configured address / port).
    if (m_wsjtx && isTrue(QStringLiteral("WsjtxAutoStart"))) {
        m_wsjtx->startListening(
            s.value(QStringLiteral("WsjtxAddress"),
                    QStringLiteral("224.0.0.1")).toString(),
            static_cast<quint16>(
                s.value(QStringLiteral("WsjtxPort"), 2237).toInt()));
    }

    // SpotCollector (UDP bind).
    if (m_spotCollector
        && isTrue(QStringLiteral("SpotCollectorAutoStart"))) {
        m_spotCollector->startListening(
            static_cast<quint16>(
                s.value(QStringLiteral("SpotCollectorPort"), 9999).toInt()));
    }

    // POTA (HTTPS poll loop).
    if (m_pota && isTrue(QStringLiteral("PotaAutoStart"))) {
        m_pota->startPolling(
            s.value(QStringLiteral("PotaPollInterval"), 30).toInt());
    }

    // FreeDV Reporter (WebSocket connect; identity / URL already plumbed
    // in ctor at lines 936-953).
    //
    // Post-3J-2 UX fix: re-resolve identity from the User/* fall-back
    // chain and call setIdentity() before startConnection(). The ctor
    // only reads FreeDvReporter/Callsign + FreeDvReporter/GridSquare;
    // if those are empty but the user has set User/Callsign via the
    // Settings tab, the connection used to fire anonymously and the
    // qso.freedv.org server would drop it. Now: (1) re-apply identity
    // from User/* if the per-source keys are empty, (2) skip the
    // connect entirely when no callsign is configured anywhere.
    if (m_freeDvReporter && isTrue(QStringLiteral("FreeDvAutoStart"))) {
        const QString freedvCall = resolveCall(
            QStringLiteral("FreeDvReporter/Callsign"));
        QString freedvGrid =
            s.value(QStringLiteral("FreeDvReporter/GridSquare")).toString();
        if (freedvGrid.isEmpty()) freedvGrid = userGrid;
        if (freedvCall.isEmpty() || freedvGrid.isEmpty()) {
            qWarning("RadioModel: FreeDV Reporter auto-start skipped - "
                     "no identity configured. Set callsign and grid in "
                     "SpotHub > Settings tab.");
        } else {
            const QString message =
                s.value(QStringLiteral("FreeDvReporter/Message")).toString();
            const QString versionStr =
                QStringLiteral("NereusSDR ")
                    + QStringLiteral(NEREUSSDR_VERSION);
            qCInfo(lcDsp)
                << "FreeDVReporter: starting connection with identity"
                << "callsign=" << freedvCall
                << "grid=" << freedvGrid
                << "msg=" << message
                << "version=" << versionStr;
            m_freeDvReporter->setIdentity(
                freedvCall, freedvGrid, message, versionStr);
            m_freeDvReporter->startConnection();
        }
    }

    // PSK Reporter: send-only.  Identity refreshed from User/* fall-
    // back chain.  2026-05-12 bench fix: if PskReporterAutoStart is
    // True, arm the 5-minute auto-send timer now — source-first port
    // from freedv-gui main.cpp:2575-2597 [@77e793a] which adds
    // PskReporter to m_reporters[] AND starts m_pskReporterTimer at
    // audio start time.  Previously the AutoStart flag persisted but
    // had no effect (it only set identity), so users with auto-start
    // checked would never see any spots reach pskreporter.info.
    if (m_pskReporter) {
        const QString pskCall = resolveCall(
            QStringLiteral("PskReporter/Callsign"));
        QString pskGrid =
            s.value(QStringLiteral("PskReporter/GridSquare")).toString();
        if (pskGrid.isEmpty()) pskGrid = userGrid;
        if (!pskCall.isEmpty()) {
            m_pskReporter->setIdentity(pskCall, pskGrid,
                                       QStringLiteral("NereusSDR ") + QStringLiteral(NEREUSSDR_VERSION));
            if (isTrue(QStringLiteral("PskReporterAutoStart"))) {
                m_pskReporter->setAutoSendIntervalSec(
                    PskReporterClient::kReportingIntervalSec);
                qCInfo(lcDsp)
                    << "PskReporter: auto-start armed (5-min interval)"
                    << "callsign=" << pskCall;
            }
        }
    }
}

bool RadioModel::isConnected() const
{
    return m_connection && m_connection->isConnected();
}

void RadioModel::setStepAttController(StepAttenuatorController* c)
{
    // Phase 4 Agent 4A of issue #167 — propagate to TransmitModel so the
    // ATT-on-TX safety gate inside setPowerUsingTargetDbm has a live
    // controller pointer.  Mirrors the existing m_stepAttController setter
    // pattern; non-owning on both sides.
    //
    // From Thetis console.cs:46740-46748 [v2.10.3.13]:
    //   //[2.10.3.5]MW0LGE max tx attenuation when power is increased and PS is enabled
    //   if (new_pwr != _lastPower && chkFWCATUBypass.Checked && _forceATTwhenPowerChangesWhenPSAon)
    //   { ... SetupForm.ATTOnTX = 31; ... }
    //
    // The PS-active gate (chkFWCATUBypass.Checked equivalent) is dormant
    // until 3M-4 PureSignal lands; until then the structure is in place
    // but the gate never fires (TransmitModel::pureSignalActive() returns
    // false unconditionally per Phase 3A).
    m_stepAttController = c;
    m_transmitModel.setStepAttenuatorController(c);

    // 2026-05-22 spectrum-calibration fix: rxMeterOffsetDb() depends on
    // the StepAttenuatorController state (preamp mode + step-att enable +
    // attenuator dB). Wire the three controller signals through to a
    // recompute-and-emit lambda so subscribers (MeterPoller, the spectrum
    // widget's dbmCalOffset) refresh whenever any of those change. Without
    // this the spectrum was stuck at whatever offset was current at startup
    // and never tracked preamp / step-att changes.
    //
    // Idempotent first-emit guard: only emit when the computed offset
    // actually changed from the cached value, so we don't fire redundant
    // updates on every controller tick.
    if (c) {
        auto recompute = [this]() {
            const double v = rxMeterOffsetDb();
            if (!qFuzzyCompare(1.0 + v, 1.0 + m_lastEmittedRxMeterOffsetDb)) {
                m_lastEmittedRxMeterOffsetDb = v;
                emit rxMeterOffsetChanged(v);
            }
        };
        connect(c, &StepAttenuatorController::attenuationChanged,
                this, [recompute](int) { recompute(); });
        connect(c, &StepAttenuatorController::preampModeChanged,
                this, [recompute](PreampMode) { recompute(); });
        connect(c, &StepAttenuatorController::stepAttEnabledChanged,
                this, [recompute](bool) { recompute(); });
        // Initial emit so subscribers seed their cache with the current
        // value rather than waiting for the first controller change.
        recompute();
    }
}

// ── 4O3A master toggle (Settings -> CAT & Network -> 4O3A General tab) ──────
//
// Persists per-MAC under hardware/<mac>/peripherals/FourO3A_Enabled
// (True / False string).  Default OFF on first run so the TCP 4992 port
// is not bound until the operator opts in.  Live-applies: starts/stops
// the SmartSdrApiListener immediately so the UI toggle doesn't require
// an app restart.
//
// No-op when no radio is connected (no MAC scope to write under).  The
// Setup page grays out under the same condition so the operator can't
// reach this entry point.
void RadioModel::setFourO3AEnabled(bool enabled)
{
    const bool current = fourO3AEnabled();
    if (current == enabled) {
        return;  // idempotent
    }
    setPeripheralValue(QStringLiteral("FourO3A_Enabled"),
                       enabled ? QStringLiteral("True") : QStringLiteral("False"));
    AppSettings::instance().save();

    if (!m_smartSdrListener) {
        return;  // ctor should always create it; defensive null guard
    }

    if (enabled) {
        if (!m_smartSdrListener->isListening()) {
            if (m_smartSdrListener->start()) {
                qCInfo(lcConnection) << "4O3A enabled: SmartSDR API listener"
                                      << "started on TCP 4992";
            } else {
                qCWarning(lcConnection) << "4O3A enabled: SmartSDR API listener"
                                         << "failed to bind TCP 4992";
            }
        }
    } else {
        if (m_smartSdrListener->isListening()) {
            m_smartSdrListener->stop();
            qCInfo(lcConnection) << "4O3A disabled: SmartSDR API listener stopped";
        }

        // Tear down any live PGXL / TGXL TCP socket. Without this, an
        // already-connected PGXL keeps sending statusUpdated frames,
        // m_hasAmplifier stays true, and the S-Meter keeps showing the
        // 2 kW PGXL scale even though the operator just disabled 4O3A.
        if (m_pgxlConnection && m_pgxlConnection->isConnected()) {
            m_pgxlConnection->disconnect();
            qCInfo(lcConnection) << "4O3A disabled: PGXL TCP disconnected";
        }
        if (m_tgxlConnection && m_tgxlConnection->isConnected()) {
            m_tgxlConnection->disconnect();
            qCInfo(lcConnection) << "4O3A disabled: TGXL TCP disconnected";
        }

        // Reset amp-presence cache. m_hasAmplifier is sticky-true after
        // any PGXL statusUpdated (see onPgxlStatus); resetting it here
        // lets the SMeterWidget / TunerApplet revert their power scales
        // to barefoot via the amplifierChanged(false) + ampStateChanged
        // emissions below. m_ampOperate also clears so STANDBY/OPERATE
        // consumers see the amp gone.
        if (m_hasAmplifier || m_ampOperate) {
            m_hasAmplifier = false;
            const bool wasOperate = m_ampOperate;
            m_ampOperate = false;
            emit amplifierChanged(false);
            if (wasOperate) { emit ampStateChanged(); }
        }
    }

    emit fourO3AEnabledChanged(enabled);
}

bool RadioModel::fourO3AEnabled() const
{
    return peripheralValue(QStringLiteral("FourO3A_Enabled"),
                           QStringLiteral("False"))
        == QStringLiteral("True");
}

bool RadioModel::rfKitEnabled() const
{
    return peripheralValue(QStringLiteral("RfKit_Enabled"),
                           QStringLiteral("False"))
        == QStringLiteral("True");
}

// Push the operator's RF-Kit preferences into the live connection.
//
// Review blocker [P2] on PR #291: RfKitPage persisted RfKit_AutoReconnect
// and RfKit_PollIntervalMs to AppSettings, but nothing ever read them back.
// scheduleReconnect() retried unconditionally and the poll cadence stayed at
// the 1000 ms default, so both controls were inert -- the checkbox and the
// spinbox moved, saved, reloaded into the UI, and changed nothing.
//
// Called immediately before every connectToAmp() so the settings apply to
// both the Setup-toggle path and the per-MAC auto-connect path.
void RadioModel::applyRfKitOperatorSettings()
{
    if (!m_rfKitConnection) {
        return;
    }
    const bool autoRe = AppSettings::instance()
        .value(QStringLiteral("RfKit_AutoReconnect"), QStringLiteral("True"))
        .toString() == QStringLiteral("True");
    m_rfKitConnection->setAutoReconnect(autoRe);

    bool ok = false;
    const int pollMs = AppSettings::instance()
        .value(QStringLiteral("RfKit_PollIntervalMs"), QStringLiteral("1000"))
        .toString().toInt(&ok);
    if (ok) {
        // setPollIntervalMs clamps to 250..5000 itself.
        m_rfKitConnection->setPollIntervalMs(pollMs);
    }
}

void RadioModel::setRfKitEnabled(bool enabled)
{
    const bool current = rfKitEnabled();
    if (enabled == current) {
        return;
    }
    setPeripheralValue(QStringLiteral("RfKit_Enabled"),
                       enabled ? QStringLiteral("True") : QStringLiteral("False"));

    if (enabled) {
        const QString host = peripheralValue(QStringLiteral("RfKit_ManualIp"));
        const quint16 port = static_cast<quint16>(
            peripheralValue(QStringLiteral("RfKit_ManualPort"),
                            QStringLiteral("8080")).toUInt());
        if (!host.isEmpty() && m_rfKitConnection) {
            applyRfKitOperatorSettings();
            m_rfKitConnection->connectToAmp(host, port);
        }
    } else if (m_rfKitConnection) {
        m_rfKitConnection->disconnect();
    }

    emit rfKitEnabledChanged(enabled);
}

// ── Per-radio peripherals helpers ──────────────────────────────────────────
//
// Resolve the "current MAC" via m_lastRadioInfo.macAddress.  When empty
// (no radio connected, or a probe/discovery entry without MAC), peripheral
// reads return the caller's default and peripheral writes are a no-op +
// qCWarning.  Setup pages must gray themselves out under the same condition
// so the operator can't reach the write path with an unbound MAC.
QString RadioModel::currentRadioMac() const
{
    return m_lastRadioInfo.macAddress;
}

QString RadioModel::peripheralValue(const QString& key,
                                    const QString& defaultValue) const
{
    const QString mac = currentRadioMac();
    if (mac.isEmpty()) {
        return defaultValue;
    }
    return AppSettings::instance()
        .hardwareValue(mac, QStringLiteral("peripherals/") + key, defaultValue)
        .toString();
}

void RadioModel::setPeripheralValue(const QString& key, const QString& value)
{
    const QString mac = currentRadioMac();
    if (mac.isEmpty()) {
        qCWarning(lcConnection)
            << "setPeripheralValue('" << key << "',...) ignored:"
            << "no radio connected; no MAC scope to write under";
        return;
    }
    AppSettings::instance().setHardwareValue(
        mac, QStringLiteral("peripherals/") + key, value);
}

// ── Per-radio peripherals lifecycle ─────────────────────────────────────────
//
// applyPeripheralsForCurrentMac runs when the radio reports Connected and
// m_lastRadioInfo.macAddress is populated.  Reads the per-MAC enable +
// host/port slots and dials out for each accessory that's switched On.
// teardownPeripherals runs on Disconnected / LinkLost and tears every
// live socket / TCP listener down so we never leave them attached to the
// previous radio's scope when the user switches to a different rig.
void RadioModel::applyPeripheralsForCurrentMac()
{
    // One-shot fold of legacy globals into the currently connected MAC's
    // scope.  Idempotent across launches via PeripheralsMigrationDone.
    migratePeripheralGlobalsIfNeeded();

    const QString mac = currentRadioMac();
    if (mac.isEmpty()) {
        qCWarning(lcConnection)
            << "applyPeripheralsForCurrentMac: no MAC available, skipping";
        return;
    }

    int started = 0;

    // ── 4O3A SmartSDR API listener on TCP 4992 ──────────────────────────
    // Must come before PGXL/TGXL because the live socket dials are gated
    // on the same per-MAC flag.
    const bool fourO3AOn = fourO3AEnabled();
    if (fourO3AOn) {
        if (m_smartSdrListener && !m_smartSdrListener->isListening()) {
            if (m_smartSdrListener->start()) {
                qCInfo(lcConnection) << "4O3A SmartSDR API listener started"
                                      << "for MAC" << mac;
                ++started;
            } else {
                qCWarning(lcConnection)
                    << "4O3A enabled but TCP 4992 bind failed for MAC" << mac;
            }
        }
        // Re-emit fourO3AEnabledChanged so views that cache the value
        // refresh against the now-known per-MAC scope (the value may
        // differ from the previously connected radio).
        emit fourO3AEnabledChanged(true);
    } else {
        emit fourO3AEnabledChanged(false);
    }

    // ── RF-Kit RF2K-S ───────────────────────────────────────────────────
    if (rfKitEnabled() && m_rfKitConnection) {
        const QString host =
            peripheralValue(QStringLiteral("RfKit_ManualIp"));
        const quint16 port = static_cast<quint16>(
            peripheralValue(QStringLiteral("RfKit_ManualPort"),
                            QStringLiteral("8080")).toUInt());
        if (!host.isEmpty()) {
            applyRfKitOperatorSettings();
            m_rfKitConnection->connectToAmp(host, port);
            qCInfo(lcConnection)
                << "RF-Kit auto-connect for MAC" << mac
                << ":" << host << ":" << port;
            ++started;
        } else {
            qCInfo(lcConnection)
                << "RF-Kit enabled for MAC" << mac
                << "but no host configured; skipping auto-connect";
        }
        emit rfKitEnabledChanged(true);
    } else {
        emit rfKitEnabledChanged(false);
    }

    // ── PGXL / TGXL (gated on 4O3A master) ──────────────────────────────
    // Without the 4O3A gate, a saved PGXL_ManualIp would dial out even
    // with 4O3A disabled, get a statusUpdated back, flip m_hasAmplifier
    // = true, and snap the S-Meter to the 2 kW PGXL scale -- surprising
    // the operator who explicitly turned 4O3A off (see MainWindow's
    // earlier auto-connect block where this gate was first established).
    if (fourO3AOn) {
        const QString pgxlIp =
            peripheralValue(QStringLiteral("PGXL_ManualIp"));
        if (!pgxlIp.isEmpty() && m_pgxlConnection
            && !m_pgxlConnection->isConnected()) {
            const quint16 p = static_cast<quint16>(
                peripheralValue(QStringLiteral("PGXL_ManualPort"),
                                QStringLiteral("9008")).toUInt());
            m_pgxlConnection->connectToPgxl(pgxlIp, p);
            qCInfo(lcConnection) << "PGXL auto-connect for MAC" << mac
                                  << ":" << pgxlIp << ":" << p;
            ++started;
        }

        const QString tgxlIp =
            peripheralValue(QStringLiteral("TGXL_ManualIp"));
        if (!tgxlIp.isEmpty() && m_tgxlConnection
            && !m_tgxlConnection->isConnected()) {
            const quint16 p = static_cast<quint16>(
                peripheralValue(QStringLiteral("TGXL_ManualPort"),
                                QStringLiteral("9010")).toUInt());
            m_tgxlConnection->connectToTgxl(tgxlIp, p);
            qCInfo(lcConnection) << "TGXL auto-connect for MAC" << mac
                                  << ":" << tgxlIp << ":" << p;
            ++started;
        }
    }

    if (started == 0) {
        qCInfo(lcConnection)
            << "No peripherals enabled for MAC" << mac;
    }
}

void RadioModel::teardownPeripherals()
{
    // Deliberately NOT gated on isConnected(). Review blocker [P1] on PR
    // #291: when the link has dropped and a reconnect is pending,
    // isConnected() is false, so the gate skipped disconnect() and left the
    // retry armed -- it would then fire after the operator disabled RF-Kit,
    // re-issue /info and restart polling. disconnect() is idempotent: it
    // stops both timers and only emits disconnected() if it had been
    // connected.
    if (m_rfKitConnection) {
        m_rfKitConnection->disconnect();
        qCInfo(lcConnection) << "Peripherals teardown: RF-Kit disconnected";
    }
    if (m_smartSdrListener && m_smartSdrListener->isListening()) {
        m_smartSdrListener->stop();
        qCInfo(lcConnection) << "Peripherals teardown: SmartSDR API stopped";
    }
    if (m_pgxlConnection && m_pgxlConnection->isConnected()) {
        m_pgxlConnection->disconnect();
        qCInfo(lcConnection) << "Peripherals teardown: PGXL disconnected";
    }
    if (m_tgxlConnection && m_tgxlConnection->isConnected()) {
        m_tgxlConnection->disconnect();
        qCInfo(lcConnection) << "Peripherals teardown: TGXL disconnected";
    }
}

void RadioModel::migratePeripheralGlobalsIfNeeded()
{
    auto& s = AppSettings::instance();
    if (s.value(QStringLiteral("PeripheralsMigrationDone"))
            .toString() == QStringLiteral("True")) {
        return;
    }
    const QString mac = currentRadioMac();
    if (mac.isEmpty()) {
        // Defer migration until we know a MAC.  Without a target scope
        // there's nowhere to write the folded values.
        return;
    }

    static constexpr const char* kKeys[] = {
        "RfKit_Enabled", "RfKit_ManualIp", "RfKit_ManualPort",
        "FourO3A_Enabled",
        "PGXL_ManualIp", "PGXL_ManualPort",
        "TGXL_ManualIp", "TGXL_ManualPort",
    };

    int migrated = 0;
    for (const char* k : kKeys) {
        const QString key = QString::fromLatin1(k);
        if (!s.contains(key)) {
            continue;
        }
        const QString v = s.value(key).toString();
        if (v.isEmpty()) {
            // Remove empty leftovers so they don't linger as ghost keys.
            s.remove(key);
            continue;
        }
        s.setHardwareValue(mac,
                           QStringLiteral("peripherals/") + key, v);
        s.remove(key);
        ++migrated;
    }

    s.setValue(QStringLiteral("PeripheralsMigrationDone"),
               QStringLiteral("True"));
    s.save();
    qCInfo(lcConnection)
        << "Peripherals migration: folded" << migrated
        << "global key(s) into hardware/" << mac << "/peripherals/";
}

bool RadioModel::isAnyExternalAmpInOperate() const
{
    // PGXL: explicit booleans set by PgxlConnection::statusUpdated handler.
    if (m_hasAmplifier && m_ampOperate) {
        return true;
    }
    // RF-Kit: poll the last-known operate_mode from Rf2ksConnection.  The
    // connection caches it in m_operateMode and refreshes once per second.
    // Require a live connection: m_operateMode is a cache with no
    // disconnect invalidation, so an amp last seen in OPERATE kept this
    // predicate true forever after it dropped, pinning MainWindow to the
    // 2 kW scale and suppressing the radio's barefoot power updates.
    // Codex review, PR #291.
    if (m_rfKitConnection
        && m_rfKitConnection->isConnected()
        && m_rfKitConnection->operateMode() == QStringLiteral("OPERATE")) {
        return true;
    }
    return false;
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

// ── Phase 3F: maxSlices() accessor ──────────────────────────────────────────
//
// Returns 1 when disconnected (safe single-slice default so callers never
// see zero and attempt to create slices with no radio present).
//
// When connected, delegates to boardCapabilities().maxSlices for the active
// SKU.  A zero value there is also clamped to 1 — it signals that the
// BoardCapabilities row has not yet been populated for that SKU (Task 1-3),
// which should be treated as "at least one slice".
//
// NereusSDR-original — no Thetis upstream.
// Design: docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §2.
int RadioModel::maxSlices() const
{
    if (!isConnected()) {
        return 1;
    }
    const int n = boardCapabilities().maxSlices;
    return n > 0 ? n : 1;
}

// ── RX meter calibration offset (Thetis-faithful port) ──────────────────────
//
// Ported from Thetis console.cs:21040 RXOffset(rx) + :20989 RXPreampOffset
// + :21022 RXCalibrationOffset [v2.10.3.13].
//
// Returns: RXPreampOffset(1) + RXCalibrationOffset(1)
//
//   RXPreampOffset(1)  = step_att_enabled ? attenuator_data
//                                         : preamp_offset[preamp_mode]
//   RXCalibrationOffset(1) = _rx1_meter_cal_offset
//                            (+ _rx1_xvtr_gain_offset deferred to XVTR epic)
//                            (+ _rx1_6m_gain_offset   deferred to 6m epic)
//
// _rx1_meter_cal_offset defaults to rxMeterCalOffsetDefaultFor(model)
// (clsHardwareSpecific.cs:395-411 port) unless the user has saved an
// override via the AppSettings key "RX1_MeterCalOffsetDb" (matches Thetis
// RX1MeterCalOffset at console.cs:21051).  The Thetis Multimeter Setup
// page exposes the override; in NereusSDR 0.4.x the page hosts the same
// key but the UI control is queued behind the 4O3A Settings refactor
// (planned next).  Power users can edit the key directly today.
double RadioModel::rxMeterOffsetDb() const
{
    const HPSDRModel model = m_hardwareProfile.model;

    // Per-radio factory cal default + user override (AppSettings key
    // RX1_MeterCalOffsetDb).  Default = Thetis factory value per model.
    const float factoryDefault = ::NereusSDR::rxMeterCalOffsetDefaultFor(model);
    bool keyOk = false;
    const double userOverride = AppSettings::instance()
        .value(QStringLiteral("RX1_MeterCalOffsetDb"),
               QString::number(static_cast<double>(factoryDefault), 'f', 6))
        .toString()
        .toDouble(&keyOk);
    const float meterCalOffset = keyOk
        ? static_cast<float>(userOverride)
        : factoryDefault;

    // RXPreampOffset branch: step-att enabled vs preamp mode.  Both paths
    // require the StepAttenuatorController; if absent (no radio yet) the
    // chain reduces to the cal_offset alone.  From Thetis console.cs:20991:
    //   if (_ignore_attenuator_offset) return 0.0f;
    // The "ignore" toggle is not exposed in NereusSDR (Thetis Setup-only,
    // dormant in modern builds), so we always include the preamp term.
    float preampOffset = 0.0f;
    if (m_stepAttController) {
        if (m_stepAttController->stepAttEnabled()) {
            // Step-att enabled path: use raw attenuator dB.
            // From console.cs:20996: fOffset = (float)_rx1_attenuator_data;
            preampOffset = static_cast<float>(
                m_stepAttController->attenuatorDb());
        } else {
            // Preamp-mode path: lookup table per console.cs:1991-2001.
            const int modeIdx = static_cast<int>(
                m_stepAttController->preampMode());
            preampOffset = ::NereusSDR::rxPreampOffsetDbFor(modeIdx);
        }
    }

    return static_cast<double>(preampOffset + meterCalOffset);
}

// ── Phase 3F Sub-Epic I: DDC stream pool ────────────────────────────────────
//
// NereusSDR-original glue over SliceStreamAllocator. Thetis has no
// equivalent because it hard-codes RX1 -> DDC2 and RX2 -> DDC3; the pool
// concept exists here so every user DDC the SKU exposes is reachable.

void RadioModel::configureStreamPool(int userDdcCount, int maxSlices,
                                     int defaultRateHz)
{
    m_streamAllocator.configure(userDdcCount, maxSlices);
    m_streamAllocator.setDefaultSampleRateHz(defaultRateHz);
    m_streamDefaultRateHz = defaultRateHz > 0 ? defaultRateHz : 192000;

    // Phase 3F Sub-Epic I closeout, defect H1: every stream starts at the
    // connect rate, and connectToRadio hands RxDspWorker exactly this size as
    // its global default (setBufferSizes(bufferSizeForRate(wdspInputRate),
    // 64)). Recording it here is what lets applyStreamDspGeometry recognise
    // the untouched single-rate pool as already in step and do nothing at all.
    m_streamInSizePushed.clear();
    const int poolInSize = bufferSizeForRate(m_streamDefaultRateHz);
    for (int st = 0; st < m_streamAllocator.streamCount(); ++st) {
        m_streamInSizePushed.insert(st, poolInSize);
    }

    // The master mixer needs one slot per slice id for the same reason the
    // WDSP channel pool needs one channel per slice, and for the same
    // reason both are sized here rather than on demand: MasterMixer's map
    // must be structurally frozen once the DSP thread starts reading it
    // lock-free. This runs at connect, well before m_dspThread->start().
    // MasterMixer::accumulate drops unregistered ids outright, so without
    // it every slice past A is demodulated and then thrown away.
    if (m_audioEngine) {
        m_audioEngine->preregisterSlices(maxSlices);
    }
}

// ── Phase 3F Sub-Epic I: WDSP RX channel pool ───────────────────────────────
//
// One channel per slice, opened once at connect and reused. Thetis opens all
// cmRCVR * cmSubRCVR RX channels in CreateRadio (create_rcvr's OpenChannel
// loop, ChannelMaster/cmaster.c:69-85 [v2.10.3.15]); deskhpsdr opens every
// receiver in one loop (radio.c:1259 [@f3d857c]). Neither opens a channel at
// runtime, and neither do we: binding a slice to a stream only changes its
// shift offset and its I/Q source.
//
// The upper bound is not cosmetic. WDSP keeps one global channel table,
// `struct _ch ch[MAX_CHANNELS]` (third_party/wdsp/src/channel.c:29), and
// OpenChannel overwrites `ch[channel]` and calls build_channel -> start_thread
// without closing the previous occupant (channel.c:75-101). Before this clamp
// existed the pool ran to maxSlices from channel 1, so on any SKU with
// maxSlices > 1 it opened an RXA at the id createTxChannel would later reuse
// for the TXA: two wdspmain threads served the same slot, the RX iobuffs and
// critical sections leaked, and teardown was undefined. kMaxSliceChannels is
// the reserved RX block; kTxChannelId sits immediately above it.
void RadioModel::openRxChannelPool(int poolSize, int inputBufferSize,
                                   int inputSampleRateHz)
{
    if (!m_wdspEngine) {
        return;
    }

    const int requested = poolSize > 0 ? poolSize : 1;
    const int clamped = std::min(requested, WdspEngine::kMaxSliceChannels);
    if (clamped < requested) {
        qCWarning(lcDsp) << "RX channel pool request" << requested
                         << "exceeds the reserved block of"
                         << WdspEngine::kMaxSliceChannels
                         << "— clamping. Raising a SKU's maxSlices requires "
                            "raising WdspEngine::kMaxSliceChannels too, which "
                            "moves kTxChannelId and kPsFeedbackChannelId.";
    }

    for (int ch = WdspEngine::kFirstSliceChannelId; ch < clamped; ++ch) {
        if (!m_wdspEngine->rxChannel(ch)) {
            m_wdspEngine->createRxChannel(ch, inputBufferSize, 4096,
                                          inputSampleRateHz, 48000, 48000);
        }
    }

    // A channel that exists is not yet a channel that demodulates: WDSP opens
    // with state = 0 and RxChannel::m_active defaults false. Every slice that
    // is already bound when the pool comes up needs its channel switched on.
    //
    // This is the reconnect path as much as the connect path.
    // teardownConnection releases every binding and WdspEngine::shutdown
    // destroys every channel; connectToRadio then re-binds all slices BEFORE
    // WDSP finishes initialising, so those binds ran against an engine with no
    // channels at all. Without this loop only Slice A would come back audible
    // after a reconnect.
    activateBoundSliceChannels();
}

// ── Phase 3F Sub-Epic I: pooled-channel activation ──────────────────────────
//
// WDSP channels are opened stopped. create_rcvr passes `0, // initial state`
// (From Thetis ChannelMaster/cmaster.c:80 [v2.10.3.15]) and the channel is
// only switched on when the receiver it backs becomes a receiver the operator
// actually has. Thetis does that at the enable, after pushing the DSPRX's
// state:
//
//   From Thetis console.cs:37359-37361 [v2.10.3.15] — RX2 enable:
//     radio.GetDSPRX(1, 0).Active = true;
//
//     WDSP.SetChannelState(WDSP.id(2, 0), 1, 0);
//
// with the mirror at console.cs:37398-37400 on disable (Active = false, then
// SetChannelState(..., 0, 0)). The sub-receiver path does the same at
// console.cs:36577-36583 / :36616.
//
// NereusSDR's equivalent of "this receiver now exists" is a slice binding to a
// stream: before the bind the slice is not in RxDspWorker's stream -> slices
// map, so no samples reach its channel; after it, every chunk on that stream
// is fanned to it. So bind is where the channel is switched on, and unbind is
// where it is switched off.
//
// Not at creation, deliberately. m_active is what keeps a channel from
// processing before its state has been applied, and what keeps getMeter off a
// WDSP channel that has never had SetChannelState called on it (see the
// segfault note in RxChannel::getMeter). Activating the whole pool at open
// would defeat both and leave up to five wdspmain threads dispatching for
// slices that do not exist. Ordering here matches Thetis: push state, then
// switch on.
//
// Not on first use either: setActive calls into WDSP, and processIq runs on
// the DSP thread.
void RadioModel::activateBoundSliceChannels()
{
    for (SliceModel* s : std::as_const(m_slices)) {
        activateSliceChannel(s);
    }
}

void RadioModel::activateSliceChannel(SliceModel* slice)
{
    if (!m_wdspEngine || !slice || slice->streamIndex() < 0) {
        return;
    }

    // Re-admit it to the mixer's readiness barrier, the counterpart of the
    // withdrawal in deactivateSliceChannel. Ahead of the already-active
    // early return below, so a slice whose channel is still live but whose
    // barrier membership was withdrawn is admitted again rather than left
    // out of the mix. Re-admission does not enrol it on its own: it rejoins
    // on its next delivered block, fading in over the mixer's ramp.
    if (m_audioEngine) {
        m_audioEngine->setSliceStreaming(slice->sliceIndex(), true);
    }

    // Sub-Epic I invariant: WDSP RX channel id == slice index.
    RxChannel* ch = m_wdspEngine->rxChannel(slice->sliceIndex());
    if (!ch || ch->isActive()) {
        // Already live. Leave it alone: connectToRadio's WDSP-init lambda
        // gives Slice A's channel the full state push (NR, SNB, APF, squelch,
        // audio panel, the lot) and then activates it, and re-running the
        // subset below on top of that would be a downgrade dressed as a
        // refresh.
        return;
    }

    // The demodulation-critical subset. Everything here decides whether the
    // audio coming out of fexchange2 is the right signal at all; the rest of
    // the per-slice DSP surface (NR, SNB, APF, squelch, binaural, pan, and the
    // NB1 / NB2 / SNB detailed tuning) still follows the active slice and
    // lands in a later sub-epic. Note this is the INITIAL seed only: once a
    // channel is up, every one of those settings has a live per-slice push in
    // wireSliceSignals that resolves rxChannel(slice->sliceIndex()), so
    // operator changes reach the right slice either way.
    ch->setMode(slice->dspMode());
    ch->setFilterFreqs(slice->filterLow(), slice->filterHigh());
    ch->setAgcMode(slice->agcMode());
    ch->setAgcTop(slice->rfGain());
    // Seed AF gain before the channel runs a single block: WDSP initialises
    // panel.gain1 to 4.0 (+12 dB) in rxa.c:538, and setActive re-pushes
    // m_afGain anyway, but seeding first means the default never reaches the
    // mixer. Same reasoning as the Slice A block in connectToRadio.
    ch->setAfGain(slice->afGain() / 100.0);
    // The offset the allocator resolved for this slice. bindSliceToStream
    // pushes it too, but a reconnect re-opens the channel underneath an
    // already-bound slice, so it has to be re-seeded here as well.
    ch->setShiftFrequency(slice->shiftOffsetHz());

    ch->setActive(true);
}

// The disable half of the same Thetis pair (console.cs:37398-37400
// [v2.10.3.15]: Active = false, then SetChannelState(..., 0, 0)). The channel
// object stays open for whichever slice takes this id next; it just stops
// running.
void RadioModel::deactivateSliceChannel(int sliceId)
{
    // Withdraw it from the mixer's readiness barrier FIRST, before the
    // channel stops producing. The mixer waits for every member with no
    // timeout, so a slice that stops feeding while still enrolled wedges
    // the whole mix. Counterpart of the re-admission in
    // activateSliceChannel; mirrors Thetis SetAAudioMixState, which is the
    // only way a stream leaves the mix upstream (aamix.c:522 [v2.10.3.15]).
    //
    // Unconditional, and ahead of the m_wdspEngine guard: the barrier entry
    // outlives the WDSP channel, so an engine-less teardown must still
    // release it.
    if (m_audioEngine) {
        m_audioEngine->setSliceStreaming(sliceId, false);
    }

    if (!m_wdspEngine) {
        return;
    }
    if (RxChannel* ch = m_wdspEngine->rxChannel(sliceId)) {
        ch->setActive(false);
    }
}

void RadioModel::bindUnboundSlices()
{
    for (SliceModel* s : std::as_const(m_slices)) {
        if (s && s->streamIndex() < 0) {
            bindSliceToStream(s, s->frequency());
        }
    }
}

void RadioModel::republishAllStreamBindings()
{
    for (int st = 0; st < m_streamAllocator.streamCount(); ++st) {
        republishStreamBindings(st);
    }
}

void RadioModel::releaseStreamBindings()
{
    for (SliceModel* s : std::as_const(m_slices)) {
        if (!s) { continue; }
        s->setStreamIndex(-1);
        s->setShiftOffsetHz(0.0);
        // The DDC number came from the codec run for a stream that no longer
        // hosts this slice. Leaving it would let the VFO flag keep reporting
        // a DDC the radio has stopped streaming.
        s->setDdcIndex(-1);
        // An unbound slice has no stream to be fed from, so it must not go
        // on holding the mixer's readiness barrier. Whichever slices the
        // next connect re-binds are re-admitted by activateSliceChannel;
        // any that are not (reconnecting to a radio with fewer DDCs, say)
        // would otherwise wait forever and silence the whole mix, because
        // the barrier has no timeout that would release them.
        if (m_audioEngine) {
            m_audioEngine->setSliceStreaming(s->sliceIndex(), false);
        }
    }
    for (int st = 0; st < m_streamAllocator.streamCount(); ++st) {
        m_streamAllocator.deactivateStream(st);
    }
    m_streamDdc.fill(-1);
    // Defect D1: a torn-down stream is on no chain, so its last ADC must not
    // outlive it. Left behind, a slice rebound to that stream on the next
    // connection would be credited to chain 1 on a radio that never put it
    // there, and the chain-1 filter would follow it.
    m_streamAdc.fill(0);
}

int RadioModel::streamPoolSize() const
{
    return m_streamAllocator.streamCount();
}

int RadioModel::activeStreamCount() const
{
    return m_streamAllocator.activeStreamCount();
}

QVector<int> RadioModel::slicesOnStream(int streamIndex) const
{
    QVector<int> out;
    for (int i = 0; i < m_slices.size(); ++i) {
        SliceModel* s = m_slices.at(i);
        if (s && s->streamIndex() == streamIndex) {
            // sliceIndex(), not the list position. RxDspWorker consumes this
            // set as WDSP channel ids ("Invariant: WDSP channel id == slice
            // index", RxDspWorker.cpp), and bindSliceToStream looks the
            // channel up the same way. The two agree until a slice is
            // removed from the middle of m_slices: removeSlice never
            // renumbers the survivors (setSliceIndex is only ever called at
            // creation), so list position and sliceIndex diverge from that
            // point on and publishing positions would demodulate the wrong
            // channel.
            out.append(s->sliceIndex());
        }
    }
    return out;
}

// ── Phase 3F Sub-Epic J Task 11 ──────────────────────────────────────────────
//
// The only place GUI code may resolve a slice's WDSP channel. A thin
// forward to WdspEngine::rxChannel(), kept here (rather than inlined in the
// header) because RadioModel.h only forward-declares WdspEngine.
// scripts/verify-no-gui-dsp-access.py fails the build if anything under
// src/gui/ calls wdspEngine()->rxChannel() directly; this accessor is the
// sanctioned replacement for the handful of GUI sites (CTUN shift push,
// MeterPoller channel wiring, Setup-page channel-readiness gates) that
// legitimately need the pointer but were reaching straight into the engine
// to get it.
RxChannel* RadioModel::rxChannelForSlice(int sliceIndex) const
{
    return m_wdspEngine ? m_wdspEngine->rxChannel(sliceIndex) : nullptr;
}

// ── Phase 3F Sub-Epic J Task 5 ──────────────────────────────────────────────
//
// One window, one centre, N slices at their own offsets inside it. The CTUN
// drag used to move the centre and then write ONE shift (channel 0's, on the
// single-pan path), so with two slices sharing a DDC the co-host kept the
// offset it had before the drag and quietly demodulated the wrong signal --
// its flag still read the right number, which is what makes this class of
// defect expensive to spot on the air. Same hazard family as the CTUN
// stranding fix in 1058500a.
//
// Each member resolves its OWN WDSP channel through its own sliceIndex
// (Sub-Epic I invariant: WDSP RX channel id == slice index); capturing one
// channel and reusing it for the whole set would just relocate the bug.
void RadioModel::reshiftSlicesOnStream(int streamIndex, double newCentreHz)
{
    if (streamIndex < 0) {
        return;
    }
    const QVector<int> members = slicesOnStream(streamIndex);
    for (int sliceIdx : members) {
        SliceModel* s = sliceById(sliceIdx);
        if (!s) {
            continue;
        }
        const double shiftHz = s->frequency() - newCentreHz;
        s->setShiftOffsetHz(shiftHz);
        // Model and WDSP are written together on purpose. SliceModel is what
        // the flag and the settings round-trip read; the channel is what
        // actually demodulates. Leaving either behind reproduces the defect
        // in the half that was skipped.
        //
        // From Thetis radio.cs:1417 [v2.10.3.15]: SetRXAShiftFreq receives
        // +(freq - center).
        if (m_wdspEngine) {
            if (RxChannel* ch = m_wdspEngine->rxChannel(s->sliceIndex())) {
                ch->setShiftFrequency(shiftHz);
            }
        }
    }
}

int RadioModel::ddcForStream(int streamIndex) const
{
    if (streamIndex < 0 || streamIndex >= static_cast<int>(m_streamDdc.size())) {
        return -1;
    }
    return m_streamDdc[static_cast<size_t>(streamIndex)];
}

void RadioModel::republishStreamBindings(int streamIndex)
{
    const QVector<int> bound = slicesOnStream(streamIndex);
    if (m_dspWorker) {
        // Lambda rather than the string-name invokeMethod overload: the
        // slot takes QVector<int>, which Qt6 aliases to QList<int>, so a
        // by-name lookup depends on how moc spelled the parameter. The
        // capture form is compile-time checked and needs no metatype
        // registration. Matches the existing queued-call pattern in this
        // file (see connectMicPttDisabledSignal).
        QMetaObject::invokeMethod(m_dspWorker,
                                  [w = m_dspWorker, streamIndex, bound]() {
            w->setStreamSlices(streamIndex, bound);
        }, Qt::QueuedConnection);
    }
    emit streamBindingsChanged(streamIndex, bound);
}

// ── Phase 3F Sub-Epic I closeout, defect H1 ─────────────────────────────────
//
// Reconcile the DSP side of the pool with the allocator. See the declaration
// in RadioModel.h for the Thetis SetXcmInrate mapping and the fexchange2
// over-read this ordering exists to prevent.
void RadioModel::applyStreamDspGeometry()
{
    const int streamCount = m_streamAllocator.streamCount();
    if (streamCount <= 0) {
        return;
    }

    // ── Targets ──────────────────────────────────────────────────────────
    // One rate per stream, and the drain size derived from it through the
    // single copy of the Thetis formula in the tree (bufferSizeForRate,
    // SampleRateCatalog.h, from ChannelMaster cmsetup.c:106-111
    // [v2.10.3.15]). An idle stream is given the connection rate so that a
    // slice landing on it later finds the geometry already correct.
    const int fallbackRateHz = m_connectionSampleRateHz > 0
                                   ? m_connectionSampleRateHz
                                   : m_streamDefaultRateHz;
    QVector<int> rateFor(streamCount, fallbackRateHz);
    QVector<int> inSizeFor(streamCount, bufferSizeForRate(fallbackRateHz));
    for (int st = 0; st < streamCount; ++st) {
        const int rate = m_streamAllocator.isStreamActive(st)
                             ? m_streamAllocator.streamSampleRateHz(st)
                             : 0;
        if (rate > 0) {
            rateFor[st]   = rate;
            inSizeFor[st] = bufferSizeForRate(rate);
        }
    }

    // ── Early out ────────────────────────────────────────────────────────
    // Nothing below has an effect when every stream already carries the size
    // we last published and every bound slice's channel already runs at its
    // stream's rate. That is every call on a single-rate radio, so the
    // single-stream path keeps its exact previous behaviour: no feed
    // disconnect, no accumulator drop, no WDSP call.
    bool outOfStep = false;
    for (int st = 0; st < streamCount && !outOfStep; ++st) {
        outOfStep = (m_streamInSizePushed.value(st, -1) != inSizeFor[st]);
    }
    if (!outOfStep && m_wdspEngine) {
        for (SliceModel* s : std::as_const(m_slices)) {
            if (!s || s->streamIndex() < 0 || s->streamIndex() >= streamCount) {
                continue;
            }
            RxChannel* ch = m_wdspEngine->rxChannel(s->sliceIndex());
            if (ch && ch->sampleRate() != rateFor[s->streamIndex()]) {
                outOfStep = true;
                break;
            }
        }
    }
    if (!outOfStep) {
        return;
    }

    // ── Quiesce ──────────────────────────────────────────────────────────
    // Mirrors setSampleRateLive steps 2 and 10. Disconnecting the feed and
    // then reaching the worker through a BlockingQueuedConnection guarantees
    // two things: any in-flight processIqBatch has finished, and every event
    // already posted to the DSP thread (including the setStreamSlices calls
    // the rebind loop just queued) has been consumed. From here until the
    // reconnect no drain can run, so the two halves of the geometry below
    // cannot be observed half-applied.
    //
    // Ordering the two writes instead would need opposite orders for the two
    // directions -- widening must raise the drain threshold before the
    // channel's in_size, narrowing must lower the channel's in_size before
    // the threshold -- because the invariant fexchange2 imposes is one-sided:
    // the threshold may exceed ch[].in_size (it reads a prefix) but must
    // never fall below it (it would read past the end). Migration inverts the
    // direction again per slice. Quiescing removes the window rather than
    // threading it.
    //
    // The currentThread() term is not theoretical hygiene: a
    // BlockingQueuedConnection whose sender and receiver threads are the same
    // deadlocks outright. Every caller here is main-thread today, and this
    // keeps it that way if one ever is not.
    const bool quiesce = m_dspWorker != nullptr && m_receiverManager != nullptr
                         && m_dspThread != nullptr && m_dspThread->isRunning()
                         && QThread::currentThread() != m_dspThread;
    if (quiesce) {
        QObject::disconnect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
                            m_dspWorker, &RxDspWorker::processIqBatch);
        QMetaObject::invokeMethod(m_dspWorker, &RxDspWorker::resetAccumulator,
                                  Qt::BlockingQueuedConnection);
    }

    // ── Half 1: the worker's per-stream drain thresholds ─────────────────
    // From Thetis cmaster.c:461 [v2.10.3.15]:
    //   pcm->xcm_insize[in_id] = getbuffsize (rate);
    //
    // Queued, matching republishStreamBindings: the map is main-thread
    // written but DSP-thread read. Posted before the feed is reconnected, and
    // Qt delivers posted events to a thread in the order they were posted, so
    // these always land ahead of the first batch that follows the reconnect.
    //
    // The record is only updated when a worker actually received the push.
    // Claiming a delivery that went nowhere is the shape of defect F1, where
    // bindings published before wireConnectionSignals built the worker were
    // silently dropped and never re-sent. With no worker the sizes simply stay
    // out of step, and the next bind or rate change publishes them for real.
    if (m_dspWorker) {
        for (int st = 0; st < streamCount; ++st) {
            if (m_streamInSizePushed.value(st, -1) == inSizeFor[st]) {
                continue;
            }
            m_streamInSizePushed.insert(st, inSizeFor[st]);
            const int inSize = inSizeFor[st];
            QMetaObject::invokeMethod(m_dspWorker,
                                      [w = m_dspWorker, st, inSize]() {
                w->setStreamInputChunk(st, inSize);
            }, Qt::QueuedConnection);
        }
    }

    // ── Half 2: the WDSP channel of every slice bound to a stream ────────
    // From Thetis cmaster.c:473-475 [v2.10.3.15]:
    //   for (i = 0; i < pcm->cmSubRCVR; i++) {
    //       SetInputSamplerate (chid (in_id, i), rate);          // dsp channel input rate
    //       SetInputBuffsize (chid (in_id, i), pcm->xcm_insize[in_id]);  // dsp channel input size
    //   }
    //
    // Every sub-receiver on the stream, not just the first: co-hosted slices
    // are handed the same chunk and each runs its own channel over it.
    // setRxChannelRate is the live SetInputSamplerate / SetInputBuffsize path
    // (WdspEngine.cpp), NOT a rebuild -- the RxChannel wrapper stays alive, so
    // the seven raw-pointer holders that crashed PR #219 stay valid. It is
    // idempotent, so slices whose stream did not move cost one comparison.
    //
    // Main thread, deliberately: SetInputBuffsize and SetInputSamplerate each
    // go through pre_main_destroy, which sleeps 25 ms while it waits out any
    // in-flight fexchange2 (channel.c:103-111 [WDSP v1.29]). Running that on
    // the DSP thread would stall the audio feeder instead of the GUI.
    if (m_wdspEngine) {
        for (SliceModel* s : std::as_const(m_slices)) {
            if (!s) { continue; }
            const int st = s->streamIndex();
            if (st < 0 || st >= streamCount) { continue; }
            m_wdspEngine->setRxChannelRate(s->sliceIndex(), rateFor[st]);
        }
    }

    if (quiesce) {
        connect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
                m_dspWorker, &RxDspWorker::processIqBatch,
                Qt::QueuedConnection);
    }
}

void RadioModel::requestDdcAssignment()
{
    emit ddcAssignmentRequested();
    invokeCodecDdcAssignment();

    // Phase 3F: the Alex band-pass per chain is a function of which slices are
    // on which ADC, so it recomputes on exactly the events that change that
    // set. This is the coalescing point for all three of them (slice bind,
    // retune, removal) and it lines up with design §10's trigger matrix rows
    // for slice created / destroyed / retuned-across-band.
    republishAlexAdcSlices();
}

bool RadioModel::sampleRateIsRadioWide() const
{
    // Protocol 1 carries one rate for the whole radio in C&C bank 0:
    // P1RadioConnection::composeCcBank0(cc0, sampleRate, mox, activeRxCount)
    // takes a single sampleRate and encodes it as srBits, so there is no
    // per-receiver rate to set. Protocol 2 carries a per-DDC rate through
    // DdcAssignment::rate[], which the codecs populate per stream.
    return qobject_cast<NereusSDR::P1RadioConnection*>(m_connection) != nullptr;
}

QVector<int> RadioModel::allowedStreamSampleRates() const
{
    // Disconnected: no board to ask, so no list. Callers decide what to show
    // in the meantime rather than being handed a guess.
    if (m_connection == nullptr) {
        return {};
    }
    const NereusSDR::ProtocolVersion proto =
        sampleRateIsRadioWide() ? NereusSDR::ProtocolVersion::Protocol1
                                : NereusSDR::ProtocolVersion::Protocol2;
    const std::vector<int> allowed = NereusSDR::allowedSampleRates(
        proto, boardCapabilities(), m_hardwareProfile.model);
    QVector<int> out;
    out.reserve(static_cast<int>(allowed.size()));
    for (int rate : allowed) {
        out.append(rate);
    }
    return out;
}

void RadioModel::requestSliceSampleRate(int sliceId, int rateHz)
{
    // Phase 3F Sub-Epic I closeout, defect G2. Resolve by ID, not by list
    // position: VfoWidget carries SliceModel::sliceIndex(), and addSlice /
    // removeSlice never renumber survivors, so the two diverge after any
    // mid-list removal (see sliceById).
    SliceModel* slice = sliceById(sliceId);
    if (slice == nullptr) {
        return;
    }
    const int stream = slice->streamIndex();
    if (stream < 0) {
        // Not bound to a DDC yet, so there is nothing whose width to change.
        // The slice picks up its stream's rate when it binds.
        return;
    }
    if (!setStreamSampleRate(stream, rateHz)) {
        emit sliceRetuneRejected(
            sliceId,
            QStringLiteral(
                "Sample-rate change to %1 kHz was rejected; all slices "
                "stayed on their existing DDC windows.")
                .arg(rateHz / 1000));
    }
}

std::optional<RadioModel::StreamRateChangePlan>
RadioModel::planStreamSampleRateChange(int streamIndex, int rateHz) const
{
    if (rateHz <= 0
        || streamIndex < 0
        || streamIndex >= m_streamAllocator.streamCount()
        || !m_streamAllocator.isStreamActive(streamIndex)) {
        return std::nullopt;
    }

    const bool isP1 = sampleRateIsRadioWide();
    StreamRateChangePlan plan{m_streamAllocator, {}};

    if (isP1) {
        for (int st = 0; st < plan.allocator.streamCount(); ++st) {
            if (plan.allocator.isStreamActive(st)) {
                plan.allocator.activateStream(
                    st, plan.allocator.streamCentreHz(st), rateHz);
            }
        }
    } else {
        plan.allocator.activateStream(
            streamIndex, plan.allocator.streamCentreHz(streamIndex), rateHz);
    }

    struct SimulatedSlice {
        int sliceId{-1};
        double frequencyHz{0.0};
        int stream{-1};
    };
    QVector<SimulatedSlice> simulated;
    simulated.reserve(m_slices.size());
    for (SliceModel* slice : m_slices) {
        if (slice && slice->streamIndex() >= 0) {
            simulated.append({
                slice->sliceIndex(), slice->frequency(), slice->streamIndex()});
        }
    }
    plan.slices.reserve(simulated.size());

    const bool ddcPinned =
        m_receiverManager && m_receiverManager->ddcFrequencyLocked();
    using Outcome = SliceStreamAllocator::Outcome;

    for (int i = 0; i < simulated.size(); ++i) {
        SimulatedSlice& candidate = simulated[i];
        int occupantCount = 0;
        for (const SimulatedSlice& other : std::as_const(simulated)) {
            if (other.stream == candidate.stream) {
                ++occupantCount;
            }
        }

        const int previousStream = candidate.stream;
        const SliceStreamAllocator::Placement placement =
            plan.allocator.retuneSlice(
                previousStream, occupantCount == 1, ddcPinned,
                candidate.frequencyHz);
        if (placement.outcome == Outcome::Rejected) {
            return std::nullopt;
        }

        if (placement.outcome == Outcome::NewStream
            || placement.outcome == Outcome::RetunedStream) {
            const bool streamAlreadyLive =
                plan.allocator.isStreamActive(placement.streamIndex);
            const int existingRateHz =
                plan.allocator.streamSampleRateHz(placement.streamIndex);
            const int rateForStream =
                isP1
                    ? rateHz
                    : ((streamAlreadyLive && existingRateHz > 0)
                           ? existingRateHz
                           : (m_connectionSampleRateHz > 0
                                  ? m_connectionSampleRateHz
                                  : m_streamDefaultRateHz));
            plan.allocator.activateStream(
                placement.streamIndex, placement.newStreamCentreHz,
                rateForStream);
        }

        candidate.stream = placement.streamIndex;
        if (previousStream != placement.streamIndex) {
            bool previousStillOccupied = false;
            for (const SimulatedSlice& other : std::as_const(simulated)) {
                if (other.stream == previousStream) {
                    previousStillOccupied = true;
                    break;
                }
            }
            if (!previousStillOccupied) {
                plan.allocator.deactivateStream(previousStream);
            }
        }

        plan.slices.append({
            candidate.sliceId, previousStream, placement, 0});
    }

    for (PlannedSlicePlacement& planned : plan.slices) {
        planned.resolvedRateHz =
            plan.allocator.streamSampleRateHz(planned.placement.streamIndex);
        if (planned.resolvedRateHz <= 0) {
            return std::nullopt;
        }
    }

    return plan;
}

void RadioModel::commitStreamSampleRateChange(
    const StreamRateChangePlan& plan)
{
    const SliceStreamAllocator previousAllocator = m_streamAllocator;
    QSet<int> bindingsToPublish;

    m_streamAllocator = plan.allocator;

    for (int st = 0; st < m_streamAllocator.streamCount(); ++st) {
        if (!m_streamAllocator.isStreamActive(st)) {
            continue;
        }
        if (m_receiverManager) {
            m_receiverManager->setReceiverFrequency(
                st,
                static_cast<quint64>(
                    m_streamAllocator.streamCentreHz(st)));
            m_receiverManager->setReceiverSampleRate(
                st, m_streamAllocator.streamSampleRateHz(st));
        }
    }

    for (const PlannedSlicePlacement& planned : plan.slices) {
        SliceModel* slice = sliceById(planned.sliceId);
        if (!slice) {
            continue;
        }
        bindingsToPublish.insert(planned.previousStream);
        bindingsToPublish.insert(planned.placement.streamIndex);

        slice->setStreamIndex(planned.placement.streamIndex);
        slice->setShiftOffsetHz(planned.placement.shiftOffsetHz);
        slice->setSampleRateHz(planned.resolvedRateHz);

        if (m_wdspEngine) {
            if (RxChannel* channel =
                    m_wdspEngine->rxChannel(planned.sliceId)) {
                channel->setShiftFrequency(
                    planned.placement.shiftOffsetHz);
            }
        }
    }

    bindingsToPublish.remove(-1);
    for (int st : std::as_const(bindingsToPublish)) {
        republishStreamBindings(st);
    }

    if (m_receiverManager) {
        for (int st = 0; st < m_streamAllocator.streamCount(); ++st) {
            if (slicesOnStream(st).isEmpty()) {
                m_receiverManager->deactivateReceiver(st);
            } else {
                m_receiverManager->activateReceiver(st);
            }
        }
    }

    applyStreamDspGeometry();

    for (int st = 0; st < m_streamAllocator.streamCount(); ++st) {
        if (!m_streamAllocator.isStreamActive(st)) {
            continue;
        }
        if (!previousAllocator.isStreamActive(st)
            || previousAllocator.streamCentreHz(st)
                != m_streamAllocator.streamCentreHz(st)
            || previousAllocator.streamSampleRateHz(st)
                != m_streamAllocator.streamSampleRateHz(st)) {
            emit streamCentreChanged(
                st, m_streamAllocator.streamCentreHz(st),
                m_streamAllocator.streamSampleRateHz(st));
        }
    }

    requestDdcAssignment();
}

bool RadioModel::setStreamSampleRate(int streamIndex, int rateHz)
{
    const std::optional<StreamRateChangePlan> plan =
        planStreamSampleRateChange(streamIndex, rateHz);
    if (!plan.has_value()) {
        return false;
    }

    const bool isP1 = sampleRateIsRadioWide();
    if (isP1) {
        // Protocol 1 carries one radio-wide wire rate. The complete allocator
        // transition is known valid before this can stop the stream or touch
        // WDSP. A refused wire update leaves every client-side object intact.
        if (setSampleRateLive(rateHz, false) < 0) {
            return false;
        }
    } else if (m_externalDiversityRouteActive) {
        // A Protocol 2 rate is per stream. Stop the live paired route only
        // after preflight succeeds; requestDdcAssignment at commit's tail
        // recreates it from the committed geometry.
        stopExternalDiversityRoute();
    }

    commitStreamSampleRateChange(*plan);
    return true;
}

bool RadioModel::bindSliceToStream(SliceModel* slice, double frequencyHz)
{
    if (!slice) { return false; }

    // No pool yet (disconnected, or connectToRadio has not reached
    // configureStreamPool). There is no DDC to bind to, and an unsized
    // allocator would reject every placement, which would surface as a
    // spurious "all DDCs in use" toast on the very first slice.
    if (m_streamAllocator.streamCount() <= 0) { return false; }

    const int previousStream = slice->streamIndex();

    // Occupancy and the CTUN pin are handed to the allocator SEPARATELY.
    // They were AND-ed into one "may retune" permission until the 2026-07-26
    // G2E bench, where that cost a DDC: a lone slice on a pinned stream
    // tuned out of its 48 kHz window, and because the pin looked identical
    // to "someone else needs this window", the slice migrated to a fresh DDC
    // and left its old one idle -- a hole in the enable mask, and a dead
    // second pan.
    //
    // ReceiverManager::ddcFrequencyLocked is exactly the CTUN flag
    // (MainWindow sets it from SpectrumWidget::ctunEnabled). The allocator
    // applies it only while the slice stays inside the window, which is the
    // case CTUN exists for; see SliceStreamAllocator::retuneSlice.
    const bool ddcPinned =
        m_receiverManager && m_receiverManager->ddcFrequencyLocked();
    const bool soleOccupant =
        previousStream >= 0 && slicesOnStream(previousStream).size() == 1;

    const auto placement =
        (previousStream < 0)
            ? m_streamAllocator.placeSlice(frequencyHz)
            : m_streamAllocator.retuneSlice(previousStream, soleOccupant,
                                            ddcPinned, frequencyHz);

    using Outcome = NereusSDR::SliceStreamAllocator::Outcome;

    if (placement.outcome == Outcome::Rejected) {
        // Phase 3F Sub-Epic I closeout, defect F4: stash the reason for the
        // retune handler, which owns the rollback and phrases the message.
        m_lastPlacementRejectReason = placement.reason;

        // Only the FIRST bind of a slice is an "add". A slice that already
        // holds a stream is being retuned, and sliceAddRejected produces a
        // status-bar line about adding a slice when all the operator did was
        // turn the knob. The frequencyChanged handler emits
        // sliceRetuneRejected instead, after it has rolled the VFO back.
        if (previousStream < 0) {
            emit sliceAddRejected(placement.reason);
        }
        return false;
    }

    if (placement.outcome == Outcome::NewStream
        || placement.outcome == Outcome::RetunedStream) {
        // Claim or move the DDC, then centre it on the slice.
        //
        // Preserve the stream's own rate when it is already live. A
        // RetunedStream is a sole occupant dragging its existing DDC to a new
        // centre, and that DDC keeps whatever width the operator gave it
        // (Task 10). Only a freshly claimed DDC takes the connection default.
        // Without this, every sole-occupant retune silently reset the stream
        // back to the connection rate and threw away a per-stream width.
        const bool streamAlreadyLive =
            m_streamAllocator.isStreamActive(placement.streamIndex);
        const int existingRateHz =
            m_streamAllocator.streamSampleRateHz(placement.streamIndex);
        const int rateForStream =
            (streamAlreadyLive && existingRateHz > 0)
                ? existingRateHz
                : (m_connectionSampleRateHz > 0 ? m_connectionSampleRateHz
                                                : m_streamDefaultRateHz);

        m_streamAllocator.activateStream(
            placement.streamIndex, placement.newStreamCentreHz, rateForStream);

        if (m_receiverManager) {
            m_receiverManager->setReceiverFrequency(
                placement.streamIndex,
                static_cast<quint64>(placement.newStreamCentreHz));
        }
        emit streamCentreChanged(
            placement.streamIndex, placement.newStreamCentreHz,
            m_streamAllocator.streamSampleRateHz(placement.streamIndex));
    }

    slice->setStreamIndex(placement.streamIndex);
    slice->setShiftOffsetHz(placement.shiftOffsetHz);

    // Phase 3F Sub-Epic J Task 6: joining an occupied window adopts its
    // blanker state; claiming an empty one keeps this slice's own. Without
    // this, a slice migrating onto a stream that already agrees on NB2 would
    // sit there reading its own stale Off until the next time some other
    // co-host happened to touch the control. See the nbModeChanged mirror
    // above, which keeps the stream in agreement from here on.
    const QVector<int> nbPeers = slicesOnStream(placement.streamIndex);
    for (int idx : nbPeers) {
        SliceModel* peer = sliceById(idx);
        if (peer && peer != slice) {
            slice->setNbMode(peer->nbMode());
            // The NB1 / NB2 tuning knobs adopt for the same reason the mode
            // does: they configure the stream's single blanker (panb / pnob
            // per receiver, Thetis cmaster.h:74-82 [v2.10.3.15]), so a joiner
            // reporting its own stored numbers would be describing settings
            // that are not the ones in force. SNB is excluded: it is per WDSP
            // channel (rxa[channel].snba, wdsp/snb.c:621-670 [v2.10.3.15]),
            // so the joiner keeps its own.
            slice->setNb1Threshold(peer->nb1Threshold());
            slice->setNb1TransitionMs(peer->nb1TransitionMs());
            slice->setNb1LeadMs(peer->nb1LeadMs());
            slice->setNb1LagMs(peer->nb1LagMs());
            slice->setNb2Mode(peer->nb2Mode());
            break;
        }
    }

    // Phase 3F Sub-Epic I closeout, defect G2: adopt the stream's rate.
    // SliceModel::sampleRateHz is the resolved rate of whichever stream is
    // hosting the slice, so a slice that migrates (evicted by a narrowing, or
    // placed on a fresh DDC) must stop reporting the width it just left.
    const int boundRateHz =
        m_streamAllocator.streamSampleRateHz(placement.streamIndex);
    if (boundRateHz > 0) {
        slice->setSampleRateHz(boundRateHz);
    }

    // Push the offset into WDSP. RxChannel::setShiftFrequency is the Thetis
    // RXOsc port (radio.cs:1409-1420 [v2.10.3.15]): SetRXAShiftFreq +
    // RXANBPSetShiftFrequency.
    if (m_wdspEngine) {
        if (RxChannel* ch = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            ch->setShiftFrequency(placement.shiftOffsetHz);
        }
    }

    // A stream the slice just left may now be empty.
    if (previousStream >= 0 && previousStream != placement.streamIndex) {
        if (slicesOnStream(previousStream).isEmpty()) {
            m_streamAllocator.deactivateStream(previousStream);
        }
        republishStreamBindings(previousStream);
    }
    republishStreamBindings(placement.streamIndex);

    // Phase 3F Sub-Epic I closeout, defect H1: a migration re-rates the slice
    // as surely as a rate change does. Moving from a 768 kHz stream to a
    // 192 kHz one leaves the slice's WDSP channel configured for a 1024-sample
    // input while its new stream drains 256, and fexchange2 would read the
    // missing 768 off the end of the accumulator (iobuffs.c:532-536
    // [WDSP v1.29]). No-op whenever everything already agrees, which is every
    // retune on a single-rate radio.
    applyStreamDspGeometry();

    // The slice now has a stream, which means RxDspWorker is about to start
    // fanning that stream's chunks at its WDSP channel. Switch the channel on
    // (Thetis's SetChannelState(..., 1, 0) at the receiver enable) so those
    // chunks reach fexchange2 instead of the inactive-channel memset. No-op
    // for a slice whose channel is already live, i.e. every retune.
    //
    // After applyStreamDspGeometry, not before: the channel's in_size and
    // input rate must agree with the stream feeding it before it runs its
    // first block (defect H1).
    activateSliceChannel(slice);

    // The active-DDC set may have changed, so the codec must recompute.
    requestDdcAssignment();
    return true;
}

// --- Slice Management ---

// Phase 3F Sub-Epic I closeout, defect C3.
//
// Was sliceAt(index), resolving positionally with m_slices.at(index). Every
// caller passed a slice id -- addSlice's return value, a sliceAdded /
// sliceRemoved payload, a WDSP channel id from AudioEngine::rxBlockReady, or
// a literal 0 meaning "slice A" -- and removeSlice never renumbers
// survivors, so the two diverge the moment a slice is removed from the
// middle of the list. With A(0) B(1) C(2) and B removed, C's audio arrived
// as id 2, m_slices.at(2) was out of range, and C went permanently silent;
// remove A instead and B's block resolved to C's SliceModel, handing B
// C's mute state, VAX channel and MOX gating.
//
// Renamed rather than re-documented so no future reader can mistake the
// "At" for a position. Positional access is still available and used where
// it is genuinely wanted, through slices().
SliceModel* RadioModel::sliceById(int sliceId) const
{
    for (SliceModel* s : m_slices) {
        if (s && s->sliceIndex() == sliceId) {
            return s;
        }
    }
    return nullptr;
}

bool RadioModel::requestTxHandoffToSlice(int sliceId)
{
    if (m_txSliceArbiter == nullptr) { return false; }
    return m_txSliceArbiter->requestHandoff(sliceId);
}

int RadioModel::addSlice(const QString& initialPanId)
{
    auto* slice = new SliceModel(this);

    // Phase 3F Sub-Epic I closeout, defect C3: lowest id not currently in
    // use, NOT m_slices.size().
    //
    // The size stamp collided after any mid-list removal, because
    // removeSlice does not renumber survivors. A(0) B(1), remove index 0,
    // list is [B(1)]; the next addSlice stamped size() == 1 and handed out
    // 1 again. Two SliceModels then shared WDSP channel 1: slicesOnStream
    // returned {1, 1}, the RxDspWorker fan-out ran rxChannel(1)->processIq
    // twice per chunk and double-pushed its audio, and both slices wrote
    // their shift offsets into the same channel so one demodulated the
    // other's frequency.
    //
    // Lowest-free also keeps the slice-letter contract from the design doc
    // §"Slice letter assignment" -- A..E in creation order, letter freed on
    // destroy, re-creation takes the lowest available -- because the letter
    // is derived as QChar('A' + sliceIndex()) at every display site
    // (RxApplet::updateSliceButtons, VaxApplet::updateTagsLabels).
    //
    // The scan is O(n^2) over at most maxSlices (5), on a user action.
    int index = 0;
    while (sliceById(index) != nullptr) {
        ++index;
    }
    slice->setSliceIndex(index);
    // Phase 3F: stamp the owning pan id BEFORE the sliceAdded() emit below,
    // so the MainWindow handler routes the new VfoWidget to the correct
    // pan's SpectrumWidget. Without this the handler would fall back to the
    // active pan and stack every new flag on pan-0.
    //
    // panKey is the authoritative binding (a real string + panKeyChanged
    // signal so later pan migrations re-route the flag). The "initialPanId"
    // dynamic property is left in place as a transitional fallback for the
    // sliceAdded handler; both carry the same value here.
    if (!initialPanId.isEmpty()) {
        slice->setPanKey(initialPanId);
        slice->setProperty("initialPanId", initialPanId);
    }
    m_slices.append(slice);

    // ── The transmitter needs a home the moment one exists ───────────────
    //
    // TxSliceArbiter::requestHandoff is the only writer of
    // SliceModel::txSlice and it early-returns on a no-op handoff, so with
    // without an explicit initial bind nothing ever raised the flag in a
    // session where the operator did not explicitly move TX. Every consumer
    // that asks which slice transmits was reading a flag that was false on
    // every slice: the TX-bound branch of removeSlice below, the codec's
    // SliceConfig::txBound, the panadapter TX badge, the VAX TX tags, and
    // the transmit-frequency push.
    //
    // Before bindSliceToStream (so the DDC assignment this triggers already
    // carries txBound) and before the sliceAdded emit (so the VfoWidget the
    // MainWindow handler builds reads a settled binding).
    if (m_txSliceArbiter) {
        m_txSliceArbiter->syncToSliceList();
    }

    // ── Phase 3F Sub-Epic I: bind to a DDC stream ───────────────────────
    //
    // A fresh SliceModel carries the 14.225 MHz ctor default
    // (SliceModel.h m_frequency), which on the bench read as "Slice C is
    // stuck on 20 m". Seed from the active slice first so a new slice opens
    // on the band the operator is working (and therefore usually shares the
    // active slice's DDC, costing no extra hardware), then bind.
    //
    // Ordering: seed and bind both sit AFTER m_slices.append, because
    // slicesOnStream() reads m_slices and republishStreamBindings must
    // publish a set that already contains this slice. The frequencyChanged
    // lambda is wired AFTER the bind, so the seed's setFrequency does not
    // trigger a second, redundant placement.
    if (m_activeSlice && m_activeSlice != slice) {
        slice->setFrequency(m_activeSlice->frequency());
        slice->setDspMode(m_activeSlice->dspMode());
    }
    bindSliceToStream(slice, slice->frequency());

    // Retuning re-runs the allocator: the slice may stay on its stream
    // (shift only), move its stream's centre if it is the sole occupant, or
    // migrate to another DDC.
    // ── Phase 3F Sub-Epic I closeout, defect F4 ─────────────────────────
    //
    // SliceModel::setFrequency commits m_frequency and emits before this
    // handler runs, so by the time the allocator rejects the placement the
    // VFO already reads the new frequency. Nothing rolled it back: the flag
    // showed 7.150, the panadapter still showed 20 m, and WDSP kept
    // demodulating at the old shift offset. The radio was lying about where
    // it was listening.
    //
    // Rolled back rather than accepted-and-flagged. An accepted tune leaves
    // the slice unbound, and an unbound slice feeds nothing: RxDspWorker
    // never demodulates it, so the operator would be looking at a live-
    // looking VFO attached to a dead receiver, with no UI anywhere in Phase
    // 3F yet that says "unbound". Snapping back does fight the input, but it
    // fights it the way a band-edge clamp does: the number visibly refuses,
    // the status bar says why, and the constraint is learned immediately. A
    // VFO readout that disagrees with the demodulator has on-air
    // consequences; a knob that springs back does not.
    //
    // The last frequency that bound successfully is derived, not stored:
    // bindSliceToStream returns early on rejection without touching
    // streamIndex or shiftOffsetHz, so those two still describe the previous
    // successful placement, and stream centre + shift offset IS that
    // frequency by construction.
    connect(slice, &SliceModel::frequencyChanged, this,
            [this, slice](double freq) {
        // Re-entrancy: the rollback setFrequency below emits
        // frequencyChanged again. Everyone else (VFO flag, persistence)
        // must see it -- that is the point -- but re-running the allocator
        // on it would be a pointless second placement.
        if (m_rollingBackFrequency) { return; }

        const int    previousStream = slice->streamIndex();
        const double previousShift  = slice->shiftOffsetHz();

        if (bindSliceToStream(slice, freq)) { return; }

        // Not a rejection: either the pool is not sized yet (disconnected)
        // or the slice has never been bound. Nothing to roll back to.
        if (previousStream < 0
            || !m_streamAllocator.isStreamActive(previousStream)) {
            return;
        }

        const double lastGoodHz =
            m_streamAllocator.streamCentreHz(previousStream) + previousShift;

        m_rollingBackFrequency = true;
        slice->setFrequency(lastGoodHz);
        m_rollingBackFrequency = false;

        emit sliceRetuneRejected(
            slice->sliceIndex(),
            QStringLiteral("Slice %1 stayed on %2 MHz. %3")
                .arg(QChar('A' + slice->sliceIndex()))
                .arg(lastGoodHz / 1.0e6, 0, 'f', 4)
                .arg(m_lastPlacementRejectReason));
    });

    // ── Phase 3F Sub-Epic J Task 6 ───────────────────────────────────────
    //
    // The blanker belongs to the DDC stream, not the slice: ANB panb / NOB
    // pnob live in struct _rcvr beside double* audio[cmMAXSubRcvr], one
    // blanker for however many sub-receivers share the receiver (Thetis
    // cmaster.h:74-82 [v2.10.3.15]). Sub-Epic I Task 4b's processIq loop
    // (RxDspWorker.cpp) hands the stream's single blanking pass to whichever
    // slice reaches it first and runs it WITH THAT SLICE'S SETTINGS, bypassing
    // every other co-host so it cannot re-blank an already-blanked chunk.
    // Linking only the buttons would leave the actual blanking behaviour
    // depending on arrival order; mirroring the state instead means every
    // co-host always agrees, so it does not matter who wins the race.
    //
    // Wired here rather than in wireSliceSignals, and for the same reason the
    // frequencyChanged rollback handler above is: this is a contract between
    // SliceModels, not a push to hardware, so it has to hold whether or not
    // a radio is attached. wireSliceSignals's own nbModeChanged connect (the
    // WDSP push) still only runs once m_connection exists, and stays there
    // untouched.
    connect(slice, &SliceModel::nbModeChanged, this, [this, slice](NereusSDR::NbMode m) {
        if (m_mirroringNbMode) { return; }
        m_mirroringNbMode = true;
        const int stream = slice->streamIndex();
        if (stream >= 0) {
            for (int idx : slicesOnStream(stream)) {
                SliceModel* peer = sliceById(idx);
                if (peer && peer != slice) {
                    peer->setNbMode(m);
                }
            }
        }
        m_mirroringNbMode = false;
    });

    // Phase 3F Sub-Epic J follow-up: the NB1 and NB2 TUNING knobs share the
    // blanker's fate for exactly the reason nbMode does, and it is settled by
    // where WDSP keeps the state rather than by intuition:
    //
    //   SetEXTANBThreshold / Tau / Advtime / Hangtime  -> ANB a = panb[id]
    //     (Thetis wdsp/nob.c:376-423 [v2.10.3.15])
    //   SetEXTNOBMode                                  -> NOB a = pnob[id]
    //     (Thetis wdsp/nobII.c:658-663 [v2.10.3.15])
    //
    // panb and pnob are the members struct _rcvr holds ONE of per receiver,
    // beside double* audio[cmMAXSubRcvr] (cmaster.h:74-82 [v2.10.3.15]).
    // Sub-Epic I Task 4b hands the stream's single blanking pass to whichever
    // co-host reaches processIq first and runs it with THAT slice's settings,
    // so co-hosts that disagree on the tuning give a result that depends on
    // arrival order. Mirroring makes ownership of the pass irrelevant.
    //
    // SNB is deliberately absent: SetRXASNBA* writes rxa[channel].snba
    // (wdsp/snb.c:621-670 [v2.10.3.15]), one per WDSP channel, and every
    // slice has its own channel. Linking those would remove control the
    // topology actually permits.
    //
    // Wired here rather than in wireSliceSignals for the same reason the
    // nbMode mirror above is: it is a contract between SliceModels, not a
    // push to hardware, so it must hold whether or not a radio is attached.
    auto mirrorNbTuning = [this, slice](auto getter, auto setter) {
        if (m_mirroringNbTuning) { return; }
        m_mirroringNbTuning = true;
        const int stream = slice->streamIndex();
        if (stream >= 0) {
            for (int idx : slicesOnStream(stream)) {
                SliceModel* peer = sliceById(idx);
                if (peer && peer != slice) {
                    (peer->*setter)((slice->*getter)());
                }
            }
        }
        m_mirroringNbTuning = false;
    };
    connect(slice, &SliceModel::nb1ThresholdChanged, this, [mirrorNbTuning](int) {
        mirrorNbTuning(&SliceModel::nb1Threshold, &SliceModel::setNb1Threshold);
    });
    connect(slice, &SliceModel::nb1TransitionMsChanged, this, [mirrorNbTuning](double) {
        mirrorNbTuning(&SliceModel::nb1TransitionMs, &SliceModel::setNb1TransitionMs);
    });
    connect(slice, &SliceModel::nb1LeadMsChanged, this, [mirrorNbTuning](double) {
        mirrorNbTuning(&SliceModel::nb1LeadMs, &SliceModel::setNb1LeadMs);
    });
    connect(slice, &SliceModel::nb1LagMsChanged, this, [mirrorNbTuning](double) {
        mirrorNbTuning(&SliceModel::nb1LagMs, &SliceModel::setNb1LagMs);
    });
    connect(slice, &SliceModel::nb2ModeChanged, this, [mirrorNbTuning](int) {
        mirrorNbTuning(&SliceModel::nb2Mode, &SliceModel::setNb2Mode);
    });

    // RF-SAFETY: when THIS slice holds the transmitter, its frequency and
    // XIT are the transmit frequency, and the Alex TX low-pass is selected
    // from that. wireSliceSignals only ever wired the ACTIVE slice, so a
    // TX-bound slice that was not the active one could be retuned without
    // the transmitter ever hearing about it — the TX NCO and TX low-pass
    // stayed on the old band while the operator watched the VFO move.
    //
    // Thetis has the same fan-out on the transmit VFO's own handler
    // (console.cs:32866-32869 [v2.10.3.15] assigns tx_dds_freq_mhz and calls
    // UpdateTXDDSFreq from the VFO B path when B transmits).
    //
    // The gate asks txBoundSlice() rather than SliceModel::isTxSlice().
    // Since TxSliceArbiter::syncToSliceList establishes the binding at the
    // append above, the two now agree by construction whenever a binding
    // resolves -- the arbiter resolves the same position it wrote the flag
    // to. They differ only where no binding resolves at all (no arbiter, no
    // slices yet), and there the two fail in opposite directions:
    // isTxSlice() goes silent, which is the failure this chain cannot
    // afford (setTxFrequency is the only writer of m_alex.lpfBitsTx, so a
    // silent gate freezes the Alex TX low-pass wherever it last landed),
    // while txBoundSlice() falls back to the active slice and keeps the
    // low-pass following the transmit frequency. Asking txBoundSlice() also
    // gates on exactly the slice pushTxFrequencyFromTxSlice() is about to
    // read, so gate and push cannot disagree. The RF-safety property is
    // unchanged either way: a retune of a slice that is not the transmitter
    // is still a no-op here.
    connect(slice, &SliceModel::frequencyChanged, this, [this, slice]() {
        if (slice == txBoundSlice()) { pushTxFrequencyFromTxSlice(); }
    });
    connect(slice, &SliceModel::xitEnabledChanged, this, [this, slice]() {
        if (slice == txBoundSlice()) { pushTxFrequencyFromTxSlice(); }
    });
    connect(slice, &SliceModel::xitHzChanged, this, [this, slice]() {
        if (slice == txBoundSlice()) { pushTxFrequencyFromTxSlice(); }
    });

    // 3M-1b H.1: only the TX-bound slice owns the global VOX mode gate.
    // Every slice is wired because the binding can move, but an RX/UI-only
    // slice must not disable or enable VOX by changing its demod mode.
    if (m_moxController) {
        connect(slice, &SliceModel::dspModeChanged, this,
                [this, slice](DSPMode mode) {
            if (slice == txBoundSlice()) {
                m_moxController->onModeChanged(mode);
            }
        });
    }

    // Phase 3P-II Phase 4 Task 96: the TGXL follows the transmitter, not
    // whichever receiver the operator happens to tune. Preserve source
    // identity so onSliceBandChanged can enforce that authority.
    connect(slice, &SliceModel::bandChanged, this,
            [this, slice](Band band) { onSliceBandChanged(slice, band); });

    // Phase 3F Sub-Epic F Task 11: when the operator flips this slice's
    // wideband-extension flag (e.g. zoom-out past DDC bandwidth, or
    // explicit Extended-view request from F Task 13), bypass the Alex
    // BPF on the slice's ADC (Sub-Epic B Task 14-15 effective-state
    // machine drives the BpfMode::WidebandLocked branch) AND flip the
    // matching CmdGeneral byte 23 wb_enable bit on the P2 connection
    // (Sub-Epic F Task 1 wiring) so the radio starts streaming the
    // wideband packets.  Off-flip restores both.
    connect(slice, &SliceModel::widebandExtensionRequestedChanged, this,
            [this, slice](bool) {
        const int chainIdx = slice->chainIndex();
        // Codex review, PR #293: the chain's state, not this slice's edge.
        // The incoming boolean is deliberately ignored; SliceModel has already
        // committed it, so recomputing across all live slices on the chain
        // reads the new value along with everyone else's. Forwarding the edge
        // directly let one slice zooming back in switch off a chain another
        // slice was still using. See RadioModel.h on widebandActiveForChain,
        // which also applies the widebandAdcs capability gate.
        const bool on = widebandActiveForChain(chainIdx);
        m_alexController.setWidebandActive(chainIdx, on);
        //
        // ── Phase 3F Sub-Epic I closeout: marshal to the connection thread ──
        //
        // Third instance of the pattern fixed for applyDdcAssignment in
        // invokeCodecDdcAssignment, and already done correctly by the
        // setAlexRxBpf push in republishAlexAdcSlices.
        //
        // setWidebandEnabled writes m_wbEnableMask and, when connected,
        // calls sendCmdGeneral(), which writes the QUdpSocket. RadioModel
        // runs on the GUI thread and the connection was moved onto
        // m_connThread (see connectToRadio), so calling it directly tore the
        // mask against the connection thread's own frame composition -- byte
        // 23 of CmdGeneral is that mask (Thetis ChannelMaster/network.c:879
        // [v2.10.3.15]) -- and drove QUdpSocket::writeDatagram from a thread
        // that owns neither the socket nor its notifier.
        //
        // Same marshalling shape as the neighbouring pushes: the functor
        // overload of QMetaObject::invokeMethod with default
        // Qt::AutoConnection, which is a plain call when the target already
        // lives on this thread (tests, and the pre-thread window at
        // construction) and a queued QMetaCallEvent when it does not.
        //
        // No qRegisterMetaType is needed. The functor overload packages the
        // whole lambda into the event, so chainIdx and on travel as ordinary
        // by-value captures; the metatype system is only involved for the
        // Q_ARG / string-name overload or for a queued signal-slot
        // connection carrying them as parameters.
        if (auto* p2 = qobject_cast<NereusSDR::P2RadioConnection*>(m_connection)) {
            QMetaObject::invokeMethod(p2, [p2, chainIdx, on]() {
                p2->setWidebandEnabled(chainIdx, on);
            });
        }
    });

    if (!m_activeSlice) {
        m_activeSlice = slice;
        // Mark the first slice as active so isActiveSlice() returns true for it.
        // AudioEngine::rxBlockReady (3M-1b E.4) reads this flag to gate the
        // per-slice RX-audio push during MOX.
        slice->setActive(true);
        emit activeSliceChanged(0);
    }

    // Wire this slice's DSP controls to its OWN WDSP channel.
    //
    // Without this a slice created after connect has no handler at all for
    // AGC, filter, mode, NB, SNB, APF, RIT/XIT, squelch, mute, pan or any NR
    // parameter: wireSliceSignals used to read m_activeSlice and run once at
    // connect, so the entire per-slice DSP surface existed for Slice A alone.
    // Bench-caught 2026-07-26 as "AGC does not seem to be wired up" on B/C/D.
    //
    // After bindSliceToStream above, so the slice already has its stream and
    // channel; before sliceAdded, so any consumer reacting to that signal sees
    // a slice whose controls are already live.
    wireSliceSignals(slice);

    emit sliceAdded(index);
    return index;
}

// Takes a slice ID (SliceModel::sliceIndex()), not a list position — the
// counterpart of sliceById(). Every caller already passed an id: the VFO
// flag's ✕ button and its context menu both send VfoWidget::sliceIndex(),
// which MainWindow stamped from slice->sliceIndex(). sliceRemoved therefore
// carries the id too, which is what the MainWindow handler has always
// assumed (it keys m_vfoWidgetsBySlice and PanadapterApplet::associatedSlices
// by sliceIndex()).
void RadioModel::removeSlice(int sliceId)
{
    const int position = m_slices.indexOf(sliceById(sliceId));
    if (position < 0) {
        return;
    }

    // Phase 3F Sub-Epic C Task 7: never remove the last remaining slice.
    // RadioModel always carries at least one SliceModel once any have been
    // created; the AetherSDR +RX/-RX UI relies on this invariant.
    if (m_slices.size() == 1) {
        return;
    }

    // External diversity has one stable source owner: Slice A (id 0). Stop
    // while that object and its worker route are still intact, before list
    // removal can make any positional lookup observe Slice B as "first".
    if (sliceId == kExternalDiversityTargetSliceId) {
        stopExternalDiversityRoute();
    }

    // Phase 3F Sub-Epic C Task 7: if the victim is currently TX-bound, hand
    // TX off to another slice BEFORE removal so the arbiter never observes
    // a torn slice list.  Fallback target is slice 0, unless slice 0 is the
    // victim itself, in which case fall back to slice 1.
    //
    SliceModel* victim = m_slices.at(position);
    if (victim->isTxSlice() && m_txSliceArbiter) {
        const int fallbackPosition = (position == 0) ? 1 : 0;
        SliceModel* fallback = m_slices.at(fallbackPosition);
        m_txSliceArbiter->requestHandoff(fallback->sliceIndex());
    }

    SliceModel* slice = m_slices.takeAt(position);

    // Reassert the invariant after the victim leaves the list.
    if (m_txSliceArbiter) {
        m_txSliceArbiter->syncToSliceList();
    }

    // Phase 3F Sub-Epic I: free the stream if this was its last slice, so
    // the DDC drops out of the ddcEnable bitmask and the radio stops
    // streaming it. The WDSP channel stays open for reuse.
    const int freedStream = slice->streamIndex();
    slice->setStreamIndex(-1);
    // ...and stop it running. Unbind is the counterpart of the bind-time
    // activation: Thetis pairs its enable (SetChannelState(..., 1, 0)) with
    // SetChannelState(..., 0, 0) on disable (console.cs:37398-37400
    // [v2.10.3.15]). The channel object survives for whichever slice takes
    // this id next; it just stops dispatching in the meantime.
    deactivateSliceChannel(sliceId);
    if (freedStream >= 0 && slicesOnStream(freedStream).isEmpty()) {
        m_streamAllocator.deactivateStream(freedStream);
    }
    if (freedStream >= 0) {
        republishStreamBindings(freedStream);
    }
    requestDdcAssignment();

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

    // Phase 3F Sub-Epic C Task 7: deleteLater() rather than delete to keep
    // any in-flight queued signals targeting this slice safe.
    slice->deleteLater();
    emit sliceRemoved(sliceId);
}

// Phase 3F Sub-Epic C Task 7: AetherSDR-faithful +RX entry point.
// Pattern from AetherSDR MainWindow.cpp:6849-6859 [@0cd4559a]
// (+RX button handler).
void RadioModel::addSliceOnPan(const QString& panId)
{
    if (m_slices.size() >= maxSlices()) {
        // Surface a human-readable cap reason for the status-bar / toast
        // wiring landing in Sub-Epic C Tasks 8-9.  RadioInfo.name carries
        // the friendly product label (e.g. "ANAN-G2"); fall back to a
        // generic phrase when disconnected.
        const QString radioLabel = m_lastRadioInfo.name.isEmpty()
                                       ? QStringLiteral("This radio")
                                       : m_lastRadioInfo.name;
        const QString reason = QStringLiteral("%1 supports a maximum of %2 slices")
                                   .arg(radioLabel)
                                   .arg(maxSlices());
        emit sliceAddRejected(reason);
        return;
    }

    // Delegate the actual create + wire + sliceAdded emit to the existing
    // addSlice() path so we keep the MoxController VOX hookup, band-change
    // wiring, and active-slice bookkeeping in one place. Pass the panId so
    // it is stamped on the slice BEFORE sliceAdded() fires (bench fix
    // 2026-06-03; previously set after the emit -> handler saw it empty).
    addSlice(panId);
}

// ─────────────────────────────────────────────────────────────────────────
// Phase 3F closeout — Sub-Epic E Task 6 consumer-side helper.
// ─────────────────────────────────────────────────────────────────────────
//
// emitAntennaAutoSwitched provides a public emit shim for the
// antennaAutoSwitched signal. Used today by the Tools menu test entry that
// visually verifies the AntennaSwitchToast surface, and reserved for future
// AlexController conflict-detection logic to call when it auto-rewrites a
// slice's antenna assignment.
void RadioModel::emitAntennaAutoSwitched(int sliceIndex,
                                          const QString& oldAntenna,
                                          const QString& newAntenna)
{
    emit antennaAutoSwitched(sliceIndex, oldAntenna, newAntenna);
}

// ─────────────────────────────────────────────────────────────────────────
// Phase 3F closeout — Sub-Epic E Task 7 consumer-side helper.
// ─────────────────────────────────────────────────────────────────────────
//
// requestTxBoundReRoute fires the txBoundReRouteRequested signal so the
// MainWindow consumer can open TxBoundConfirmDialog. Today only the Tools
// menu test entry calls this; real emission belongs in addSliceOnPan once
// the conflict-detection state machine lands in a follow-up.
void RadioModel::requestTxBoundReRoute(const QString& proposedAntenna,
                                        const QString& existingAntenna)
{
    emit txBoundReRouteRequested(proposedAntenna, existingAntenna);
}

// ─────────────────────────────────────────────────────────────────────────
// Phase 3R Task I5: RadeChannel signal-graph wiring.
// ─────────────────────────────────────────────────────────────────────────
//
// Phase J (mode swap to RADE) constructs a RadeChannel per slice and
// calls wireRadeChannel(sliceId, channel, slice) to plumb the channel's
// signals into the per-slice slot graph below. The channel's signals
// (snrChanged / syncChanged / rxTextDecoded) do not carry a slice ID;
// the wiring captures the slice ID at wire time so the receiving slot
// knows which slice to apply the update to.

void RadioModel::wireRadeChannel(int sliceId, RadeChannel* channel,
                                 SliceModel* slice)
{
    if (channel == nullptr || slice == nullptr) {
        // Defensive no-op. Phase J always passes valid pointers in
        // production; tests use this branch to exercise wireWithNull.
        return;
    }

    // Adapt the channel's per-channel signals to the per-slice-ID
    // RadioModel slots. Captured-sliceId lambdas attach the slice
    // identity at wire time. The slot bodies look the slice up via
    // sliceById(sliceId) so a stale capture (slice removed between
    // emit and dispatch) lands as a safe no-op.
    connect(channel, &RadeChannel::snrChanged, this,
            [this, sliceId](float snr) {
                onRadeSnrChanged(sliceId, snr);
            });
    connect(channel, &RadeChannel::syncChanged, this,
            [this, sliceId](bool synced) {
                onRadeSyncChanged(sliceId, synced);
            });
    connect(channel, &RadeChannel::rxTextDecoded, this,
            [this, sliceId](const QString& callsign, const QString& grid) {
                onRadeTextDecoded(sliceId, callsign, grid);
            });
    // Phase 3R L2: freq-offset re-emit for the RadeApplet readout. The
    // codec emits only on actual offset change so no model-side de-dup
    // is needed; the captured sliceId routes the per-channel emission
    // into the multi-slice signal surface.
    connect(channel, &RadeChannel::freqOffsetChanged, this,
            [this, sliceId](float hz) {
                emit radeFreqOffsetChanged(sliceId, hz);
            });

    // Phase 3R Task J4: route decoded RADE speech into AudioEngine's
    // speakers bus through the same rxBlockReady entry point WDSP's
    // RxChannel uses (via RxDspWorker).  RadeChannel emits a QByteArray
    // of interleaved float32 stereo PCM (24 kHz from the RX path
    // upsampler at RadeChannel.cpp:513-520 [Phase 3R I2]); AudioEngine
    // expects (const float*, int frames) of interleaved stereo float
    // (AudioEngine.h:306 rxBlockReady), so the adapter lambda below
    // reinterprets the byte buffer and calls through.  The byte count
    // must be a multiple of (2 * sizeof(float)) = 8; partial blocks are
    // dropped rather than risk a half-frame push past MasterMixer.
    if (m_audioEngine != nullptr) {
        connect(channel, &RadeChannel::rxSpeechReady, this,
                [this, sliceId](const QByteArray& pcm) {
                    // One-shot first-fire tracer (off by default;
                    // enable with
                    //   QT_LOGGING_RULES="nereus.rade.debug=true").
                    // Useful for confirming RADE actually decoded
                    // anything during a bench session — without
                    // sync the codec emits nothing, so absence
                    // means "RADE never decoded", not "audio path
                    // broken".
                    static int s_rxSpeechFirstLog = 0;
                    if (s_rxSpeechFirstLog < 3) {
                        qCDebug(lcRade)
                            << "rxSpeechReady fire #"
                            << (s_rxSpeechFirstLog + 1)
                            << "sliceId=" << sliceId
                            << "bytes=" << pcm.size();
                        ++s_rxSpeechFirstLog;
                    }
                    if (m_audioEngine == nullptr) {
                        return;
                    }
                    constexpr int kBytesPerStereoFrame =
                        2 * static_cast<int>(sizeof(float));
                    const int bytes = pcm.size();
                    if (bytes <= 0 || (bytes % kBytesPerStereoFrame) != 0) {
                        return;
                    }
                    const int frames24k = bytes / kBytesPerStereoFrame;
                    const float* stereo24k =
                        reinterpret_cast<const float*>(pcm.constData());

                    // Phase 3R K-bench (bench feedback): RadeChannel
                    // emits at 24 kHz stereo float32 but AudioEngine's
                    // speakers bus runs at 48 kHz. Pushing 24 kHz
                    // samples without upsampling makes the audio play
                    // at 2x speed ("chipmunk sounding"). Upsample
                    // 24 -> 48 kHz here, one resampler per leg, so
                    // AudioEngine's MasterMixer sees the expected
                    // 48 kHz rate.
                    if (!m_radeRxSpeechL
                        || !m_radeRxSpeechR) {
                        m_radeRxSpeechL =
                            std::make_unique<Resampler>(
                                24000.0, 48000.0, 4096);
                        m_radeRxSpeechR =
                            std::make_unique<Resampler>(
                                24000.0, 48000.0, 4096);
                    }
                    // Deinterleave stereo -> two mono buffers (RADE
                    // emits L==R dual-mono anyway, but keep both legs
                    // separate so the upsampler sees a self-consistent
                    // stream per channel).
                    m_radeRxLScratch.resize(
                        static_cast<size_t>(frames24k));
                    m_radeRxRScratch.resize(
                        static_cast<size_t>(frames24k));
                    for (int i = 0; i < frames24k; ++i) {
                        m_radeRxLScratch[static_cast<size_t>(i)] =
                            stereo24k[2 * i + 0];
                        m_radeRxRScratch[static_cast<size_t>(i)] =
                            stereo24k[2 * i + 1];
                    }
                    QByteArray upL = m_radeRxSpeechL->process(
                        m_radeRxLScratch.data(), frames24k);
                    QByteArray upR = m_radeRxSpeechR->process(
                        m_radeRxRScratch.data(), frames24k);
                    const int upBytes = std::min(upL.size(),
                                                 upR.size());
                    const int frames48k =
                        upBytes / static_cast<int>(sizeof(float));
                    if (frames48k <= 0) {
                        return;  // resampler warmup
                    }
                    // Re-interleave at 48 kHz.
                    m_radeRxInterleaved48k.resize(
                        static_cast<size_t>(frames48k) * 2);
                    const float* l = reinterpret_cast<const float*>(
                        upL.constData());
                    const float* r = reinterpret_cast<const float*>(
                        upR.constData());
                    for (int i = 0; i < frames48k; ++i) {
                        m_radeRxInterleaved48k[
                            static_cast<size_t>(2 * i + 0)] = l[i];
                        m_radeRxInterleaved48k[
                            static_cast<size_t>(2 * i + 1)] = r[i];
                    }
                    m_audioEngine->rxBlockReady(
                        sliceId, m_radeRxInterleaved48k.data(),
                        frames48k);
                });
    }

    // ── Phase 3R K-bench (source-first reframe): TX modem audio ────────
    //
    // RadeChannel::txModemReady carries the RADE neural codec's
    // encoded baseband at 24 kHz stereo float32 (the upsampler
    // duplicates the 8 kHz RADE_COMP real-leg to both L and R).
    //
    // Source-first per freedv-gui src/pipeline/RADETransmitStep.cpp:
    // 196-200 [@77e793a]: take ONLY the real component of rade_tx's
    // output and treat it as audio. freedv-gui hands it to the
    // soundcard; the radio's external SSB modulator does USB/LSB.
    // NereusSDR's analogue is the WDSP TXA modulator (in USB or LSB
    // mode per TxChannel::setTxMode's RADE_U/L -> USB/LSB mapping).
    //
    // Pipeline:
    //   1. Extract L channel as mono real-valued modem baseband.
    //   2. Upsample 24 -> 48 kHz mono float32 (mic-input rate).
    //   3. Push to TxWorkerThread::setRadeAudioBlock which
    //      substitutes for the live mic in dispatchOneBlock's
    //      RADE branch. The WDSP TXA chain (with K1's RADE mic
    //      profile bypassing speech processing) modulates to
    //      proper SSB I/Q. sendTxIq runs via the normal WDSP
    //      path.
    //
    // Earlier K4 scaffolding (direct sendTxIq with I=mono / Q=0)
    // produced DSB modulation and bypassed the WDSP modulator,
    // which broke TUNE in RADE (TUNE writes PostGen + relies on
    // the modulator stage running). This reframe makes TUNE and
    // RADE TX share the same modulator path.
    //
    // 2026-05-12 (PR #238 review P1 #4 follow-up): wire the
    // txModemReady -> WDSP-modulator lambda UNCONDITIONALLY.  The
    // lambda body checks `m_txWorker` on every fire (line below),
    // so a wireRadeChannel call that lands before m_txWorker is
    // created (test harness, or any sequence where the slice mode
    // flips into RADE before connect time) still produces a live
    // connection that comes online as soon as m_txWorker is.  The
    // earlier `if (m_txWorker)` outer gate made the connect a
    // permanent no-op in that ordering, which the parity tests
    // (tst_rade_channel_model_wiring) caught.
    {
        connect(channel, &RadeChannel::txModemReady, this,
                [this](const QByteArray& iq) {
                    // One-shot first-fire tracer (off by default;
                    // enable with
                    //   QT_LOGGING_RULES="nereus.rade.debug=true").
                    // Useful during bench TX shakedown to confirm
                    // rade_tx is actually producing modem output
                    // when the operator keys up.
                    static int s_radeTxModemFirstLogged = 0;
                    if (s_radeTxModemFirstLogged < 3) {
                        qCDebug(lcRade)
                            << "txModemReady fire #"
                            << (s_radeTxModemFirstLogged + 1)
                            << "bytes=" << iq.size();
                        ++s_radeTxModemFirstLogged;
                    }
                    if (!m_txWorker) {
                        return;
                    }
                    constexpr int kBytesPerStereoFrame =
                        2 * static_cast<int>(sizeof(float));
                    const int bytes = iq.size();
                    if (bytes <= 0
                        || (bytes % kBytesPerStereoFrame) != 0) {
                        return;
                    }
                    const int frames24k = bytes / kBytesPerStereoFrame;
                    const float* stereo =
                        reinterpret_cast<const float*>(iq.constData());

                    // Step 1: extract L channel as mono modem baseband.
                    m_radeTxMonoScratch.resize(
                        static_cast<size_t>(frames24k));
                    for (int i = 0; i < frames24k; ++i) {
                        m_radeTxMonoScratch[static_cast<size_t>(i)] =
                            stereo[2 * i + 0];
                    }

                    // Step 2: lazy-build the 24 -> 48 kHz upsampler
                    // (TxWorkerThread's WDSP TXA chain runs at 48 kHz
                    // mic rate; m_radeTxResampler now feeds mic-input
                    // not the radio wire).
                    if (m_radeTxResampler == nullptr
                        || m_radeTxResamplerHwRate != 48000) {
                        m_radeTxResampler = std::make_unique<Resampler>(
                            24000.0, 48000.0,
                            /*maxBlockSamples=*/4096);
                        m_radeTxResamplerHwRate = 48000;
                    }

                    QByteArray upsampled = m_radeTxResampler->process(
                        m_radeTxMonoScratch.data(), frames24k);
                    if (upsampled.isEmpty()) {
                        return;  // resampler warm-up
                    }

                    // Step 3: hand off to the worker. Default Qt::
                    // AutoConnection resolves to QueuedConnection (the
                    // worker thread differs from this main thread);
                    // setRadeAudioBlock copies under its own mutex.
                    QMetaObject::invokeMethod(
                        m_txWorker.get(),
                        "setRadeAudioBlock",
                        Qt::QueuedConnection,
                        Q_ARG(QByteArray, upsampled));
                });
    }

    // ── Phase 3R K-bench: TX mic-feed plumbing ──────────────────────────
    //
    // TxWorkerThread emits radeMicBlockReady(QByteArray int16 mono 16k)
    // every pump tick when m_currentTxPath == TxPath::Rade.  Route
    // that into the channel's txEncode slot.  RadeChannel lives on
    // the main thread (where this RadioModel does), so Qt's
    // AutoConnection resolves to QueuedConnection (the worker emits
    // from its own thread).  Setting the worker's m_radeChannel
    // pointer makes the TxPath::Rade branch aware of the channel
    // identity for diagnostic purposes; the actual mic-block transport
    // is via the queued signal/slot which does its own thread-safe
    // delivery.
    //
    // On unwire (channel destroyed by mode swap), Qt auto-disconnects
    // the queued signal/slot (sender or receiver QObject destruction
    // tears down the connection); the worker's m_radeChannel pointer
    // is separately cleared via a channel->destroyed lambda below so
    // a stale pointer can't leak into a subsequent TxPath::Rade tick.
    if (m_txWorker) {
        m_txWorker->setRadeChannel(channel);
        connect(m_txWorker.get(), &TxWorkerThread::radeMicBlockReady,
                channel, &RadeChannel::txEncode,
                Qt::QueuedConnection);
    }

    // Phase 3R K-bench: tell RxDspWorker about the RadeChannel so it can
    // route incoming I/Q (decimated to 24 kHz) to RadeChannel::processIq
    // when WDSP RxChannel(0) is absent. Without this, RADE RX hears
    // silence — the I/Q from the radio gets dropped in RxDspWorker's
    // rxCh==null path.
    if (m_dspWorker) {
        m_dspWorker->setRadeChannel(channel);
    }
    connect(channel, &QObject::destroyed, this,
            [this]() {
                if (m_txWorker) {
                    m_txWorker->setRadeChannel(nullptr);
                }
                if (m_dspWorker) {
                    m_dspWorker->setRadeChannel(nullptr);
                }
            });

    // The slice pointer is currently unused at wire time. Slot bodies
    // dereference via sliceById(sliceId), which is the safer route because
    // it handles the slice-was-deleted race naturally. The parameter
    // remains in the signature so Phase J's call sites read with the
    // intended slice context.
    Q_UNUSED(slice);
}

bool RadioModel::radeSynced(int sliceId) const
{
    return m_radeSyncedSlices.value(sliceId, false);
}

void RadioModel::onRadeTextDecoded(int sliceId, const QString& callsign,
                                   const QString& grid)
{
    if (!m_rxDecodeModel) {
        return;
    }
    RxDecode decode;
    decode.callsign = callsign;
    decode.mode     = QStringLiteral("RADE");
    decode.source   = QStringLiteral("rade_text");
    decode.utcTime  = QDateTime::currentDateTimeUtc();

    // Pull the slice's current frequency for the freqMhz column when
    // the slice still exists. A removed slice is a safe no-op: freqMhz
    // defaults to 0.0 in the RxDecode struct.
    if (auto* slice = sliceById(sliceId)) {
        decode.freqMhz = slice->frequency() / 1.0e6;

        // 2026-05-11 bench: also pin the speaker callsign on the slice
        // so the VFO flag can paint "<call> ● <snr>dB" instead of just
        // "RADE ● <snr>dB".  Sticky semantics: stays until the next
        // EOO decode replaces it OR setDspMode leaves RADE_U/RADE_L
        // (clear-on-mode-off-RADE).  Bench design 2026-05-11 (option
        // A + D).  Empty callsign no-ops via SliceModel's idempotent
        // setter so a repeat EOO of the same call does not re-emit.
        if (!callsign.isEmpty()) {
            slice->setLastRadeRxCallsign(callsign);
        }
    }

    // I4 Option B (the third_party/rade callsign-over-EOO channel)
    // does not carry a grid square; RadeText emits textDecoded with
    // callsign only. Phase L wires RadeText::textDecoded(callsign)
    // through the channel as rxTextDecoded(callsign, "") (empty
    // grid). Future text-channel revs may add grid; the payload
    // string accommodates both forms.
    if (!grid.isEmpty()) {
        decode.payload = QStringLiteral("%1 %2").arg(callsign, grid);
    } else {
        decode.payload = callsign;
    }

    m_rxDecodeModel->addDecode(decode);

    // Phase 3R K-bench (bench feedback): pull current SNR snapshot
    // once for both reporters below.
    int snrDb = 0;
    double freqMhz = 0.0;
    if (auto* slice = sliceById(sliceId)) {
        const double snr = slice->snrDb();
        if (!std::isnan(snr)) {
            snrDb = static_cast<int>(snr);
        }
        freqMhz = slice->frequency() / 1.0e6;
    }

    // Push to FreeDV Reporter so qso.freedv.org marks our row as
    // decoding this station. freedv-gui's addReceiveRecord truncates
    // SNR to (int) so we do the same. From freedv-gui
    // src/reporting/FreeDVReporter.cpp [@77e793a].
    if (m_freeDvReporter && m_freeDvReporter->isConnected()) {
        // Wire mode "RADEV1" matches freedv-gui's FREEDV_MODE_RADE string
        // (freedv-gui src/freedv_interface.cpp:63 [@77e793a]).
        m_freeDvReporter->sendRxReport(
            callsign, QStringLiteral("RADEV1"), snrDb);
    }

    // 2026-05-12 bench fix: source-first port from freedv-gui.  Drop
    // the prior NereusSDR-specific FreeDvReporter/ReportToPsk gate
    // (which double-gated the cross-feed even when the user had
    // started PSK Reporter explicitly).  Match freedv-gui main.cpp:
    // 1959-1966 [@77e793a] which feeds every decode to every reporter
    // in m_reporters[] unconditionally; "enabled" is represented by
    // whether the reporter is in the list at all.  Our equivalent:
    // gate on PskReporterClient::isAutoSendActive(), which is true
    // iff the 5-minute auto-send timer has been armed by the Start
    // button (or PskReporterAutoStart restore).  When inactive we
    // skip the queue write so reports don't accumulate behind the
    // user's back.
    if (m_pskReporter && m_pskReporter->isAutoSendActive()) {
        m_pskReporter->reportDecode(
            callsign, QStringLiteral("RADE"), freqMhz, snrDb);
    }
}

void RadioModel::onRadeSyncChanged(int sliceId, bool synced)
{
    // Dedup repeated identical values. Without this guard the status-bar
    // SYNC indicator would flicker on every codec sync-state poll even
    // when nothing changed.
    const bool prior = m_radeSyncedSlices.value(sliceId, false);
    if (prior == synced) {
        return;
    }
    m_radeSyncedSlices[sliceId] = synced;

    // 2026-05-12 bench: clear cached speaker callsign on sync rising
    // edge IF sync was down for >= kRadeSyncDropClearDebounceMs (option
    // B debounce per bench design refinement).
    //
    // Rationale: between transmissions on a typical RADE QSO sync
    // drops for >=1-2 sec; when sync re-acquires we know a *new*
    // transmission has started but the new speaker's EOO has not
    // arrived yet (EOO decode takes 5-15 sec). Showing the previous
    // speaker's callsign during that window misattributes the new
    // transmission. Clearing flips the VFO flag back to "RADE ●"
    // until the new EOO lands.
    //
    // The debounce filters spurious sync flicker on marginal copy
    // (sub-second drops during a fade) so the user does not lose the
    // callsign mid-over.
    //
    // On falling edge (synced -> false): just record the timestamp.
    // On rising edge (false -> synced): consult the timestamp and
    // clear if elapsed >= debounce.
    if (!synced) {
        // Falling edge: record drop timestamp for the next rising edge.
        m_radeSyncDropAt[sliceId] = QDateTime::currentDateTimeUtc();
    } else {
        // Rising edge: if we have a drop timestamp AND it's been long
        // enough, treat this as a "new transmission" event and clear
        // the slice's cached speaker callsign.
        const auto it = m_radeSyncDropAt.constFind(sliceId);
        if (it != m_radeSyncDropAt.constEnd() && it.value().isValid()) {
            const qint64 elapsedMs =
                it.value().msecsTo(QDateTime::currentDateTimeUtc());
            if (elapsedMs >= kRadeSyncDropClearDebounceMs) {
                if (auto* slice = sliceById(sliceId)) {
                    if (!slice->lastRadeRxCallsign().isEmpty()) {
                        slice->setLastRadeRxCallsign(QString());
                        qCInfo(lcDsp)
                            << "RADE slice" << sliceId
                            << "sync re-acquired after" << elapsedMs
                            << "ms (>= " << kRadeSyncDropClearDebounceMs
                            << "ms debounce) — cleared speaker callsign";
                    }
                }
            }
        }
        // Clear the drop timestamp now that we've consumed it. The
        // next falling edge will record a fresh one.
        m_radeSyncDropAt.remove(sliceId);
    }

    emit radeSyncChanged(sliceId, synced);
}

void RadioModel::onRadeSnrChanged(int sliceId, float snrDb)
{
    // Forward to the slice's snrDb Q_PROPERTY (D5). SliceModel::setSnrDb
    // is NaN-aware: NaN -> NaN no-ops, numeric -> identical-numeric
    // no-ops, so no extra dedup is needed here.
    if (auto* slice = sliceById(sliceId)) {
        slice->setSnrDb(static_cast<double>(snrDb));
    }
    emit radeSnrChanged(sliceId, snrDb);
}

// ── 2026-05-12 bench: FreeDV Reporter freq-publish throttle ─────────────────
//
// Spinning the VFO across a band fires SliceModel::frequencyChanged on
// every sub-Hz movement.  Without throttling the FreeDVReporterClient
// would emit a Socket.IO freq_change packet per tick (potentially 100+
// per second on mouse-wheel acceleration), DoS'ing qso.freedv.org and
// making other operators' dashboards flicker.
//
// Policy (per bench design 2026-05-12):
//   - Trailing dwell: restart kFreedvFreqDwellMs (7000 ms) single-shot
//     timer on every call.  Timer expiry calls flushFreedvFrequencyDwell
//     which publishes m_freedvPendingHz.  Spinning publishes exactly
//     once after the user stops.
//   - Band-jump fast-path: |new - lastPublished| >= kFreedvFreqJumpHz
//     (100 kHz) bypasses the dwell and publishes immediately.  Band
//     changes don't lag the dashboard.
//   - MOX engage: flushFreedvFrequencyDwell() is also called from the
//     MoxController::txAboutToBegin subscriber so the reporter never
//     shows "TXing on stale freq" if the user keys mid-dwell.
//
// Caller (slice.frequencyChanged subscriber) has already verified the
// reporter is non-null and connected.
void RadioModel::publishFreedvFrequencyDwelled(quint64 hz)
{
    if (!m_freeDvReporter || !m_freeDvReporter->isConnected()) {
        return;
    }
    m_freedvPendingHz = hz;

    // Band-jump fast-path.  Compute delta against the *last published*
    // freq, not the most-recent-pending freq, so a slow ramp through
    // 100 kHz of band (e.g. dragging the VFO 5 kHz at a time) still
    // honours the dwell once it has crossed the threshold once.
    const quint64 last = m_freedvLastPublishedHz;
    const quint64 delta = (hz > last) ? (hz - last) : (last - hz);
    if (last == 0 || delta >= kFreedvFreqJumpHz) {
        m_freeDvReporter->setFrequency(hz);
        m_freedvLastPublishedHz = hz;
        if (m_freedvFreqDwellTimer) {
            m_freedvFreqDwellTimer->stop();
        }
        qCDebug(lcDsp) << "FreeDVReporter: fast-path published"
                       << hz << "Hz (delta=" << delta << ")";
        return;
    }

    // Trailing dwell: cache + (re)start the timer.  Expiry publishes.
    if (m_freedvFreqDwellTimer) {
        m_freedvFreqDwellTimer->start();  // restart the single-shot
    }
    qCDebug(lcDsp) << "FreeDVReporter: dwell-deferred"
                   << hz << "Hz (delta=" << delta << ")";
}

void RadioModel::flushFreedvFrequencyDwell()
{
    if (!m_freeDvReporter || !m_freeDvReporter->isConnected()) {
        return;
    }
    if (m_freedvPendingHz == 0
        || m_freedvPendingHz == m_freedvLastPublishedHz) {
        return;
    }
    m_freeDvReporter->setFrequency(m_freedvPendingHz);
    qCInfo(lcDsp) << "FreeDVReporter: dwell-published"
                  << m_freedvPendingHz << "Hz"
                  << "(delta from prior published="
                  << static_cast<qint64>(m_freedvPendingHz)
                       - static_cast<qint64>(m_freedvLastPublishedHz)
                  << "Hz)";
    m_freedvLastPublishedHz = m_freedvPendingHz;
    if (m_freedvFreqDwellTimer) {
        m_freedvFreqDwellTimer->stop();
    }
}

// Phase 3J-1 closeout follow-up (2026-05-12): show/hide our station on
// the FreeDV Reporter dashboard based on the active slice's mode.
// FreeDV Reporter is a tracker FOR FreeDV operators -- a station running
// SSB or WSJT-X has no business appearing in that list.  Mirrors freedv-
// gui's connect-and-hide-when-not-on-FreeDV behavior (FreeDVReporter.cpp
// :167-185 + :704-729 [@77e793a] -- hideFromView / showOurselves).
//
// Connection stays alive so we can still see other FreeDV stations on
// the dashboard (FreeDVReporterDialog UI works) and report decodes via
// sendRxReport when our RadeChannel pulls an EOO callsign.
void RadioModel::updateFreedvReporterVisibility()
{
    if (!m_freeDvReporter) { return; }

    const SliceModel* slice = activeSlice();
    const bool inRade = slice
        && (slice->dspMode() == DSPMode::RADE_U
         || slice->dspMode() == DSPMode::RADE_L);

    // setHiddenFromView no-ops on the network side when the requested
    // state matches the server's view, so this is safe to call on every
    // mode change without flooding qso.freedv.org with hide/show events.
    m_freeDvReporter->setHiddenFromView(!inRade);
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

bool RadioModel::setActiveSliceById(int sliceId)
{
    SliceModel* target = sliceById(sliceId);
    if (target == nullptr) { return false; }

    // The conversion this function exists for, identical in shape to
    // requestTxHandoffToSlice above.
    const int position = m_slices.indexOf(target);
    if (position < 0) { return false; }

    setActiveSlice(position);
    return true;
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
    //
    // v0.4.1 hotfix: applyHpsdrModel() also fans the resolved model out
    // to TransmitModel and ReceiverManager in one call, replacing the
    // previously-separate `m_transmitModel.setHpsdrModel(...)` push at
    // the bottom of this block (around the issue #175 review-fix
    // comment further down) AND the missing ReceiverManager push that
    // broke v0.4.0 PureSignal on Hermes / ANAN-10 / ANAN-10E /
    // ANAN-100 / ANAN-100B / AnvelinaPro3-on-P1.  See the
    // applyHpsdrModel definition for the full bug context.
    HPSDRModel selectedModel = info.modelOverride;
    if (selectedModel == HPSDRModel::FIRST) {
        selectedModel = defaultModelForBoard(info.boardType);
    }
    applyHpsdrModel(selectedModel);

    qCDebug(lcConnection) << "HardwareProfile: model=" << displayName(m_hardwareProfile.model)
                          << "effectiveBoard=" << static_cast<int>(m_hardwareProfile.effectiveBoard)
                          << "adcCount=" << m_hardwareProfile.adcCount;

    // hermes-filter-debug Bug 1: push the connected board's attenuator range
    // into StepAttenuatorController so consumers (RxApplet S-ATT spinbox,
    // GeneralOptionsPage spinboxes) read board-correct min/max.  Default
    // controller bounds are 0..31; HL2 needs the signed -28..+31 range
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
        //
        // Issue #174: default to True (key absent → enabled).  Strict
        // mi0bot port from setup.designer.cs:17466-17467 [v2.10.3.13-beta2]:
        //   this.chkHERCULES.Checked = true;
        //   this.chkHERCULES.CheckState = CheckState.Checked;
        // Users plug in the N2ADR filter board and expect it to "just work"
        // out of the box; the previous False default forced manual opt-in
        // and was a recurring support burden.
        //
        // Issue #174 (PR #188 review): gate this block on HL2 family.
        // applyN2adrPreset unconditionally wipes the entire OC matrix
        // before conditionally repopulating (N2adrPreset.cpp:73-78), so
        // running it on a non-HL2 board would destroy any user-configured
        // OC pin patterns on every connect.  N2ADR is an HL2 accessory;
        // non-HL2 boards have no business in this code path.
        if (boardCapabilities().hasIoBoardHl2) {
            const QString n2adrKey = QStringLiteral("hl2IoBoard/n2adrFilter");
            const bool n2adrOn = AppSettings::instance()
                                     .hardwareValue(info.macAddress, n2adrKey,
                                                    QStringLiteral("True"))
                                     .toString() == QStringLiteral("True");
            applyN2adrPreset(m_ocMatrix, n2adrOn);
            m_ocMatrix.save();
        }

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
        //
        // Issue #175 review fix: push the connected hardware model into
        // TransmitModel BEFORE load() so the polymorphic [0, 99] HL2
        // clamp inside TransmitModel::load() (mi0bot
        // console.cs:47616-47666 [v2.10.3.13-beta2]) sees the correct
        // SKU.  Idempotent: the second push at line ~4420 in the
        // Connected state-transition handler is a no-op when the model
        // is unchanged.
        //
        // v0.4.1 hotfix: the push is now done up-front by applyHpsdrModel()
        // (called from the model-override resolution block above), which
        // also fans out to ReceiverManager so the per-board codec sees
        // the correct model.  Both pushes still land BEFORE load(), which
        // is what the issue #175 fix required.
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

        // ── Phase 4 Agent 4A of #167: per-MAC PaProfileManager scope ─────────
        //
        // Set the MAC scope and seed the 16 "Default - <model>" + Bypass
        // factory profiles on first launch (per PaProfileManager::load
        // contract).  Active-profile-on-connect logic resolves to either the
        // stored active key, "Default - <connectedModel>", or the first
        // factory profile.  Mirrors MicProfileManager wiring above.
        // setMacAddress("") is called in teardownConnection so all mutators
        // silently no-op while no radio is selected.
        if (m_paProfileManager) {
            m_paProfileManager->setMacAddress(info.macAddress);
            m_paProfileManager->load(m_hardwareProfile.model);
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
    // P2 2-ADC boards (Angelia / Orion / OrionMKII / Saturn / ANAN-G2) use
    // DDC2 as primary RX because DDC0/DDC1 are reserved for the diversity /
    // PureSignal pair (Thetis console.cs:8556-8598 GetDDC() P2 branch
    // [v2.10.3.13]). 1-ADC P2 boards (Hermes / HermesII — ANAN-10E /
    // ANAN-100B running community P2 firmware) use DDC0 as primary
    // (console.cs:8600-8632 [v2.10.3.13]).  P1 radios deliver samples on
    // hardware receiver index 0, so leave the mapping auto-assigned (which
    // rebuildHardwareMapping resolves to 0 for the first active receiver).
    // Hardcoding DDC2 for everything dropped every P1 ep6 packet at
    // ReceiverManager::feedIqData on tester hardware; hardcoding DDC2 for
    // every P2 board left HermesII users with no I/Q stream and a 2-second
    // watchdog timeout (issue #263).
    int rxIdx = m_receiverManager->createReceiver();
    if (info.protocol == ProtocolVersion::Protocol2) {
        const int primaryDdc =
            NereusSDR::P2RadioConnection::primaryRxDdcForBoard(info.boardType);
        if (primaryDdc != 0) {
            m_receiverManager->setDdcMapping(rxIdx, primaryDdc);
        }
        // primaryDdc == 0 → leave auto-assigned; rebuildHardwareMapping
        // resolves the first active receiver to hw=0.
    }
    m_receiverManager->setAdcForReceiver(rxIdx, 0); // ADC0

    // Create slice 0 and load persisted VFO state from AppSettings.
    // Slice A always lives on the default pan "pan-0"; seed its panKey so
    // flag migration (panKeyChanged) routes symmetrically with Slice B+.
    if (m_slices.isEmpty()) {
        addSlice();
        if (!m_slices.isEmpty()) {
            m_slices.first()->setPanKey(QStringLiteral("pan-0"));
        }
    }
    setActiveSlice(0);
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

    // ── Phase 3F Sub-Epic I: open the stream pool ───────────────────────────
    //
    // Two pools with different sizes, because slices can share a DDC:
    //   streams  = caps.userDdcCount   (one per hardware DDC we may use)
    //   channels = caps.maxSlices      (one WDSP channel per slice; opened
    //                                   in the WDSP-init lambda below, once
    //                                   the engine is actually up)
    //
    // Thetis opens all 10 RX channels in CreateRadio (cmaster.cs:516
    // [v2.10.3.15]); deskhpsdr opens every receiver in one loop
    // (radio.c:1259 [@f3d857c]). Neither opens a channel at runtime.
    //
    // caps.maxSlices rather than the maxSlices() accessor: that accessor
    // returns 1 until isConnected() is true, and m_connection is not
    // assigned until further down this function.
    const int poolSlices = caps.maxSlices > 0 ? caps.maxSlices : 1;
    configureStreamPool(caps.userDdcCount, poolSlices, wdspInputRate);

    // One ReceiverManager receiver per stream. Receiver 0 was created above
    // with the board's primary-DDC mapping; the rest are auto-assigned and
    // stay inactive until a slice binds to them.
    for (int st = 1; st < caps.userDdcCount; ++st) {
        if (m_receiverManager->receiverConfig(st).receiverIndex < 0) {
            m_receiverManager->createReceiver();
        }
    }

    // Slice A is created earlier in this function, before the pool is sized,
    // so its addSlice-time bind was a no-op against an empty allocator. On a
    // reconnect, teardownConnection released EVERY slice's binding, so this
    // is also where Slice B and friends come back.
    bindUnboundSlices();

    qCInfo(lcConnection) << "Sub-Epic I: streams=" << caps.userDdcCount
                         << "channels=" << poolSlices;

    // 3M-1a G.1 fixup: explicit disconnect in teardownConnection() prevents
    // accumulation across reconnect cycles.  Qt::UniqueConnection can't be
    // used with lambdas, so we rely on the matching disconnect there.
    // Without that disconnect, every connectToRadio() would add another copy
    // of this lambda; on the second connect, both copies would call
    // createRxChannel + createTxChannel(kTxChannelId) (idempotent today, but doubled work).
    connect(m_wdspEngine, &WdspEngine::initializedChanged, this,
            [this, wdspInputRate, wdspInSize](bool ok) {
        if (!ok) {
            return;
        }
        // Create primary RX channel once WDSP is ready. in_size follows
        // Thetis cmaster.c create_rcvr: 64 * input_rate / 48000. WDSP
        // decimates input_rate -> 48000 internally and outputs 64 samples
        // per fexchange2 call.
        //
        // Phase 3R K-bench: ALWAYS create the WDSP RxChannel even when
        // slice is in RADE mode. WDSP feeds the S-meter, spectrum, AGC,
        // and ADC-overload detector — all of which the user expects to
        // keep working in RADE mode. The earlier gate that skipped this
        // creation killed the S-meter in RADE mode (bench-reported).
        // The audio-output side (AudioEngine push) is gated in
        // RxDspWorker: when the slice is in RADE the WDSP-decoded
        // audio is discarded and RADE owns the speaker path.
        RxChannel* rxCh = m_wdspEngine->createRxChannel(0, wdspInSize, 4096,
                                                         wdspInputRate, 48000, 48000);

        // 3M-1a G.1: create the WDSP TX channel (channel ID = WDSP.id(1, 0)).
        // Parameters match Thetis cmaster.c:177-190 [v2.10.3.13] — create_xmtr().
        // WdspEngine owns the channel via m_txChannels; we take a non-owning view.
        // The channel starts stopped (setRunning(false) is the default); txReady
        // fires setRunning(true) after MOX engage + rfDelay.
        // From Thetis dsp.cs:926-944 [v2.10.3.15] — WDSP.id(1, 0) returns
        // CMsubrcvr * CMrcvr, i.e. WdspEngine::kTxChannelId here.
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
            // Task 4.2: give RxChannel a handle to WdspEngine so onModeChanged()
            // can call rebuild() when the active mode's DSP-Options settings change.
            rxCh->setWdspEngine(m_wdspEngine);

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
                // NB mode is per-band, and so is the detailed tuning beside
                // it. The tuning pass-through was removed 2026-04-22 in favour
                // of NbFamily seeding from radio-global AppSettings, but the
                // Setup page that live-pushed those globals wrote channel 0
                // unconditionally, so a second receiver could never be tuned.
                // Restored per slice by the Sub-Epic J follow-up.
                //
                // Seeding here matters as much as the live pushes: a slice
                // restored from settings before its channel exists gets no
                // live push (the connect resolves no channel and no-ops), so
                // without this the channel would run on NbFamily's ctor
                // defaults while the model reported the operator's values.
                rxCh->setNbMode(m_activeSlice->nbMode());
                // From Thetis setup.cs:8606 [v2.10.3.15] for the 0.165 scale.
                rxCh->setNbThreshold(0.165 * static_cast<double>(m_activeSlice->nb1Threshold()));
                rxCh->setNbTransitionMs(m_activeSlice->nb1TransitionMs());
                rxCh->setNbLeadMs(m_activeSlice->nb1LeadMs());
                rxCh->setNbLagMs(m_activeSlice->nb1LagMs());
                rxCh->setNb2Mode(m_activeSlice->nb2Mode());
                rxCh->setSnbK1(m_activeSlice->snbK1());
                rxCh->setSnbK2(m_activeSlice->snbK2());
                rxCh->setSnbOutputBandwidthHz(m_activeSlice->snbOutputBandwidthHz());

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
                // AF Gain: route the slice slider through the WDSP RX panel
                // (SetRXAPanelGain1) — Thetis radio.cs:1077-1107 [v2.10.3.14]
                // RXOutputGain pattern — instead of multiplying it onto the
                // post-DSP master mix.  setActive(true) below will re-push
                // m_afGain regardless, but seeding it first means the channel
                // never runs even one block at WDSP's default gain1=4.0.
                rxCh->setAfGain(m_activeSlice->afGain() / 100.0);
            }
            rxCh->setActive(true);
        }

        // ── Phase 3F Sub-Epic I: open the WDSP RX channel pool ─────────────
        //
        // Runs AFTER the Slice A block above so channel 0 already carries
        // its full state; openRxChannelPool skips channels that exist and
        // leaves an already-live channel alone.
        //
        // Sized off BoardCapabilities directly, not the maxSlices()
        // accessor, which returns 1 until m_connection is assigned (further
        // down connectToRadio, after this lambda has already run).
        openRxChannelPool(boardCapabilities().maxSlices, wdspInSize,
                          wdspInputRate);

        // Master output volume (MasterOutputWidget) is the only writer to
        // AudioEngine::setVolume; the per-slice afGain seeded above lives
        // in WDSP, not in the post-DSP scalar.  Don't overwrite the master
        // value the widget restored from AppSettings here — that was the
        // distortion-at-high-volume root cause prior to 2026-05-07.
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

    // Phase B6' — per-board WDSP ChannelMaster-layer calls.
    //
    // From Thetis clsHardwareSpecific.cs:85-191 [v2.10.3.15] — called at
    // connect time per SKU.  HardwareProfile values were populated by
    // HardwareProfile::forModel() (Phase B1) from the same Thetis table.
    // Upstream inline attribution preserved per CLAUDE.md §"Inline comment preservation":
    //   :129 //N1GP G2E added
    //   :171 // G8NJJ: likely to need further changes for PA
    //   :185 //DH1KLM
    //   :187 // DH1KLM: changed for compatibility reasons for OpenHPSDR compat. DIY PA/Filter boards
    //
    // SetADCSupply: PA over-drive protection scaling in xtxgain().
    //   Case 33: ptn = 1/10^(adc_value/2730.0) — Hermes-family boards.
    //   Case 50: ptn = 1/10^(adc_value/1802.0) — OrionMKII/Saturn-family.
    //   adcSupplyVoltage == 0 sentinel → setAdcSupply skips the call.
    //
    // LRAudioSwap: L/R stereo-pair swap for the outbound P2/ETH audio stream
    //   (sendOutbound() at ChannelMaster/netInterface.c:1277 [v2.10.3.15]).
    //   Hermes-family (HERMES/ANAN10/ANAN10E/ANAN100/ANAN100B/HERMESLITE):
    //     swap=1. All modern boards (Angelia/Orion/Saturn-family): swap=0.
    m_wdspEngine->setAdcSupply(/*txid=*/0, m_hardwareProfile.adcSupplyVoltage);
    m_wdspEngine->setLRAudioSwap(m_hardwareProfile.lrAudioSwap ? 1 : 0);

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
        //
        // Issue #153 sub-bug 1 — cold-start TxChannel race fix.
        //
        // On cold start (no cached FFTW wisdom), WdspEngine::initialize
        // runs async and only emits initializedChanged(true) AFTER wisdom
        // is generated (~15 min on a fresh install).  connectToRadio()
        // runs synchronously, so the immediate createTxChannel attempt
        // returns nullptr and the previous code logged a warning and
        // gave up — TUN and MOX both produced silence for the rest of
        // the session until reconnect.
        //
        // Wrap the entire create+wire body in a captured lambda so the
        // exact same code path can run later when WdspEngine becomes
        // initialized.  Hot-cache (wisdom present): txSetup() runs and
        // succeeds inline.  Cold-start: txSetup() returns early at the
        // createTxChannel-returns-nullptr guard, the retry registration
        // below fires the same lambda once initializedChanged(true)
        // lands, and the body runs verbatim.
        auto txSetup = [this, txOutRate]() {
            if (m_txChannel) {
                return;
            }
            m_txChannel = m_wdspEngine->createTxChannel(
                /*channelId=*/WdspEngine::kTxChannelId,
                /*inputBufferSize=*/64,
                /*dspBufferSize=*/WdspEngine::kTxDspBufferSize,
                /*inputSampleRate=*/48000,
                /*dspSampleRate=*/WdspEngine::kTxDspSampleRate,
                /*outputSampleRate=*/txOutRate);
            if (!m_txChannel) {
                return;
            }
            m_txChannel->setConnection(m_connection);

            // Task 4.2: give TxChannel a handle to WdspEngine so onModeChanged()
            // can call rebuild() when the active mode's DSP-Options settings change.
            m_txChannel->setWdspEngine(m_wdspEngine);

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
            // VAX TX mic source — pulls audio from /nereussdr-vax-tx
            // shared memory (written by 3rd-party apps via the HAL
            // plugin's "NereusSDR TX" device).  Registered with the
            // composite router via setVaxSource() so MicSource::Vax
            // selection routes to it.
            m_vaxTxMicSource = std::make_unique<VaxTxMicSource>(m_audioEngine);
            const bool hasMicJack = m_hardwareProfile.caps
                                        ? m_hardwareProfile.caps->hasMicJack
                                        : true;  // safe default: assume mic jack present
            m_compositeMicRouter = std::make_unique<CompositeTxMicRouter>(
                m_pcMicSource.get(), m_radioMicSource.get(), hasMicJack);
            m_compositeMicRouter->setVaxSource(m_vaxTxMicSource.get());

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
                installBandPlanMoxCheck();
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

            // ── 3M-4 Task 7: PureSignal coordinator ────────────────────────────
            //
            // Lazy-construct here once both m_txChannel and (if available)
            // PsFeedbackChannel are live.  Both come up through WdspEngine's
            // initialization sequence (createTxChannel + openPsFeedbackChannel
            // — see WdspEngine.cpp:228 + 264).  Late-binding via setTxChannel /
            // setPsFeedbackChannel keeps the dependency wiring explicit and
            // makes teardown order safe (PureSignal::dtor draws timers down
            // before our raw TxChannel pointer dies).
            //
            // Constructor pulls construction-time deps (engine, mox, stepAtt,
            // twoTone) from RadioModel; tx + fb are passed as nullptr-safe
            // pointers and reset by setTxChannel/setPsFeedbackChannel below.
            //
            // Per-board capability application happens here — BoardCapabilities
            // is populated from m_hardwareProfile.caps by the time the
            // WDSP-init lambda runs (the discovery + hardware profile
            // exchange has completed before WDSP channels open).  Phase 3M-4
            // bench-fix Round 2: previously the doc said "happens in
            // onConnected" but no actual call site existed in the source
            // tree (grep for applyBoardCapabilities returned 0 hits before
            // this fix).  Result: SetPSHWPeak never ran, so calcc's GetPSHWPeak
            // returned 0.0 instead of the per-board default (0.6121 for
            // ANAN-G2 / 0.2899 for OrionMkII / 0.233 for HL2 / 0.4072 for
            // legacy P1 boards — Task 1 commit 1bbb85a [v2.10.3.13]).
            if (!m_pureSignal) {
                m_pureSignal = std::make_unique<PureSignal>(
                    m_wdspEngine,
                    m_txChannel,
                    m_wdspEngine ? m_wdspEngine->psFeedbackChannel() : nullptr,
                    m_moxController,
                    m_stepAttController,
                    m_twoToneController,
                    /*parent=*/nullptr);
            } else {
                // Reconnect path — pointers may have changed under us.
                m_pureSignal->setTxChannel(m_txChannel);
                m_pureSignal->setPsFeedbackChannel(
                    m_wdspEngine ? m_wdspEngine->psFeedbackChannel() : nullptr);
            }

            // From Thetis cmaster.cs:566 [v2.10.3.13-beta2] (mi0bot):
            //   puresignal.SetPSHWPeak(txch, HardwareSpecific.PSDefaultPeak);
            //   // MI0BOT: Correct for correct PS value
            // applyBoardCapabilities also pushes psSampleRate to the
            // PsFeedbackChannel + TxChannel calcc (mirrors cmaster.cs:535
            // [v2.10.3.13]: puresignal.SetPSFeedbackRate(txch, ps_rate);).
            // Inline tag preservation: //MI0BOT  [original inline comment
            // from mi0bot-Thetis cmaster.cs:566]
            m_pureSignal->applyBoardCapabilities(boardCapabilities());

            // ── Task 17 chunk A — wire PSEnabled fan-out into ReceiverManager ──
            //
            // From Thetis PSForm.cs:235-269 PSEnabled property setter
            // [v2.10.3.13]:
            //   if (_psenabled) { console.UpdateDDCs(...); NetworkIO.SetPureSignal(1);
            //                     NetworkIO.SendHighPriority(1); ... }
            // The PSEnabled property setter is THE radio/DDC fan-out, fired
            // by the cmd-state machine on every PSEnabled flip:
            //
            //   case TurnOnAutoCalibrate (PSForm.cs:646)        → PSEnabled=true
            //   case TurnOnSingleCalibrate (PSForm.cs:662)      → PSEnabled=true
            //   case IntiateRestoredCorrection (PSForm.cs:720)  → PSEnabled=true
            //   case StayON (PSForm.cs:678)                     → PSEnabled=false
            //   case TurnOFF (PSForm.cs:705)                    → PSEnabled=true
            //
            // Codex Fix C: previously these wires bound to autoCalEnabledChanged,
            // so Single Cal / Restore / Stay-on / Turn-off paths only set calcc
            // flags via setPSRunCal but never fired the radio-side fan-out.
            // Now bound to psEnabledChanged.  autoCalEnabledChanged stays live
            // for the PS-A button visual state at the UI layer.
            //
            // Qt::UniqueConnection because the WDSP-init lambda may fire on
            // reconnect (ctor branch above) without tearing down the existing
            // PureSignal — same idempotency pattern as the other late-bind
            // seams in this lambda.
            connect(m_pureSignal.get(), &PureSignal::psEnabledChanged,
                    m_receiverManager, &ReceiverManager::setPureSignalEnabled,
                    Qt::UniqueConnection);

            // ── Phase 3F Sub-Epic I closeout, defect F3 ─────────────────
            //
            // PureSignal is a codec input (CodecContext::puresignalRun) but
            // nothing recomputed the stream assignment when it moved, so a
            // slice kept reporting a DDC the radio had already reclaimed.
            //
            // Follow the effective PSEnabled state, not the auto-cal
            // preference. Single Cal and restored corrections never toggle
            // autoCalEnabled, while preference-on precedes the cmd-state
            // machine actually enabling PS. psEnabledChanged is therefore the
            // one edge that keeps codec context and wire state coherent.
            //
            // Target the member function, NOT a lambda wrapping it.
            // Qt::UniqueConnection is only implemented for pointer-to-member
            // slots: with any other callable, qobject.h:263-269 leaves pSlot
            // null, connectImpl warns "unique connections require a pointer to
            // member function of a QObject subclass" and returns an INVALID
            // connection -- the slot is never called at all, and a debug build
            // asserts outright. Both of these were lambdas, so neither
            // connection existed and the DDC assignment never refreshed when
            // PureSignal claimed or released its DDCs. The unit coverage could
            // not see it: those tests drive the model API directly rather than
            // through the signal.
            //
            // The signal carries a bool the slot does not take, which is fine --
            // a slot may accept fewer arguments than its signal.
            connect(m_pureSignal.get(), &PureSignal::psEnabledChanged,
                    this, &RadioModel::refreshDdcAssignmentForRadioState,
                    Qt::UniqueConnection);

            // Push the PS run flag through to the radio connection so
            // byte-9..16 of CmdHighPriority swap DDC0/DDC1 frequencies to TX
            // freq during MOX (Thetis network.c:936-945 [v2.10.3.13] gate is
            // (ptt_out && puresignal_run)).  Without this, DDC0/DDC1 stay
            // tuned to RX freq during TX and never see the actual TX signal.
            // From Thetis PSForm.cs:246 [v2.10.3.13]:
            //   NetworkIO.SetPureSignal(1);
            // Codex Fix C: rerouted from autoCalEnabledChanged to
            // psEnabledChanged so Single Cal also sets the wire bit.
            connect(m_pureSignal.get(), &PureSignal::psEnabledChanged,
                    m_connection, &RadioConnection::setPuresignalRun,
                    Qt::UniqueConnection);

            // Tell StepAttenuatorController that PS is active.  Without this,
            // m_psActive stays false → on every MOX-on edge,
            // onMoxHardwareFlipped sees psOff=true and forces
            // setTxStepAttenuation(31) (the "PS off, force 31 dB" Thetis
            // safety per console.cs:29562-29568 [v2.10.3.13]).  That OVERRIDES
            // PureSignal::autoAttentionTick's adjustments, pinning ATT-on-TX
            // at 31 forever and starving the PS feedback ADC so calcc never
            // converges feedbackLevel into [128, 181].
            // Codex Fix C: rerouted from autoCalEnabledChanged to
            // psEnabledChanged so Single Cal / Restore paths also lift the
            // 31 dB safety pin.
            if (m_stepAttController) {
                connect(m_pureSignal.get(),
                        &PureSignal::psEnabledChanged,
                        m_stepAttController,
                        &StepAttenuatorController::setPsActive,
                        Qt::UniqueConnection);
                // Initial state push: psEnabledChanged only fires when the
                // cmd-state machine flips PSEnabled, so a connect-time sync
                // for the controller starts from false (the cmd-state
                // machine starts in Off; PSEnabled flips on the first
                // TurnOn* visit after singleCalibrate / setAutoCalEnabled).
                m_stepAttController->setPsActive(false);
            }

            // ── Task 17 chunk C/D/E — pscc() driver (PsccPump) ─────────────────
            //
            // Without PsccPump, the WDSP calcc engine never receives any
            // paired TX-monitor + PS-feedback samples → info[16] stays at
            // zero → all PsForm Calibration Information fields, the
            // bottom-banner FB number, the IMD overlay, and GetPk are
            // blocked on info[] becoming non-zero.  PsccPump is the
            // host-side equivalent of Thetis's ChannelMaster
            // sync.c InboundBlock(id=1) call (sync.c:53-58 [v2.10.3.13]).
            //
            // Construction: same pattern as PureSignal — late-binding
            // alongside TxChannel.  Unique-pointer ordering guarantees
            // teardown drains the pump before TxChannel goes away.
            if (!m_psccPump) {
                m_psccPump = std::make_unique<PsccPump>(/*parent=*/nullptr);
                m_psccPump->setMoxController(m_moxController);
                // The TXA channel by symbol, never a literal. pscc() reads
                // txa[channel].calcc.p without a null check
                // (calcc.c:645-652), and calcc.p is only created by
                // create_txa (txa.c:405), so naming a channel that never had
                // a TXA opened segfaults the moment PureSignal pumps.
                //
                // This used to read 1, which was right when the RX pool
                // started at channel 1 and TX sat below it. Phase 3F reserved
                // [0, kMaxSliceChannels) for RX slices and moved the TXA above
                // them (RadioModel.cpp:3010), which turned the literal into an
                // RX channel and took the app down on key-down with
                // PureSignal active.
                m_psccPump->setTxChannelId(WdspEngine::kTxChannelId);

                // Chunk D — iqDataReceived is forked to PsccPump from the
                // existing wireConnectionSignals lambda (the one wired in
                // RadioModel::wireConnectionSignals around RadioConnection::
                // iqDataReceived).  PsccPump filters by ddcIndex (only acts
                // on DDC0=PS-fb and DDC1=TX-mon by default per cmaster.cs:
                // 533-534 [v2.10.3.13]); other DDCs fall through unchanged.
                //
                // The earlier separate Qt::QueuedConnection (m_connection
                // → m_psccPump.get()) was a bench-bug: Qt6 multi-listener
                // dispatch needs Q_DECLARE_METATYPE for QVector<float>,
                // which we don't have, so the second consumer silently
                // dropped packets and starved the connection thread's read
                // loop → connect watchdog timeout.  Inline call from the
                // existing lambda is metatype-free and bench-validated.

                // Chunk E — codec config tells the pump when PS DDCs go
                // live and which is which.  PsccPump activates only when
                // (psEnabled && mox) per the codec's applyPureSignalDdcConfig
                // output for OrionMkII / G2.
                //
                // ReceiverManager and PsccPump are both on the main thread
                // so AutoConnection becomes DirectConnection — no metatype
                // bootstrap needed.  PsDdcConfig is metatyped at
                // CodecContext.h:318, so even if a future thread move
                // converts this to a queued connection it will still work.
                connect(m_receiverManager,
                        &ReceiverManager::ddcConfigChanged,
                        m_psccPump.get(), &PsccPump::onDdcConfigChanged);
            }

            // Flip the TransmitModel pureSignalActive() seam from the test
            // stub default (returns false) to the live PureSignal read.
            // The ATT-on-TX-on-power-change safety gate inside
            // TransmitModel::setPowerUsingTargetDbm now fires correctly when
            // calcc has corrections in flight (#167 follow-up:
            // console.cs:46740-46748 [v2.10.3.13]).
            m_transmitModel.setPureSignal(m_pureSignal.get());

            // Phase 3M-4 Task 13: late-bound coordinator handoff for the
            // PureSignal-aware applets.  PureSignalApplet + TxApplet [PS-A]
            // listen to this signal so they can wire their controls now
            // that the coordinator is live.
            emit pureSignalCoordinatorReady(m_pureSignal.get());

            // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): per-MAC persistence
            // for PS-A enabled (autoCalEnabled).  Without this the toggle
            // lives only in memory and resets on every app launch.  Key
            // shares the same hardware/<mac>/... per-MAC scope as the
            // AlexController TX-bypass flags landed alongside this fix.
            //
            // Three subtle gotchas the bench surfaced (2026-05-23):
            //
            //   1. MAC source: m_connection->radioInfo().macAddress is
            //      populated asynchronously by the radio handshake and is
            //      still EMPTY at the moment this WDSP-init lambda runs.
            //      Use m_lastRadioInfo.macAddress (cached at the top of
            //      connectToRadio when the user picked the radio from
            //      discovery) instead.
            //
            //   2. Save flush: AppSettings::instance().setValue() updates
            //      only the in-memory map.  scheduleSettingsSave() writes
            //      per-slice + AlexController + TransmitModel state but
            //      does NOT call AppSettings::instance().save(), so this
            //      arbitrary key never reaches the XML.  Direct save() is
            //      the right pattern for rare user-initiated writes (same
            //      as SpotHubDialog / SpectrumWidget).
            //
            //   3. UniqueConnection vs lambda: Qt::UniqueConnection
            //      requires a pointer-to-member-function slot and Qt
            //      SILENTLY DROPS the connect when handed a lambda
            //      (with only a runtime warning).  Idempotency across
            //      reconnect re-wires is already safe here because
            //      m_pureSignal is reset() on disconnect (line 6760),
            //      so its outgoing connections die with it before the
            //      next connect rebuilds them — no UniqueConnection
            //      needed.
            {
                const QString mac = m_lastRadioInfo.macAddress;
                if (!mac.isEmpty()) {
                    auto& s = AppSettings::instance();
                    const QString key = QStringLiteral(
                        "hardware/%1/pureSignal/autoCalEnabled").arg(mac);
                    const bool persisted =
                        (s.value(key, QStringLiteral("False")).toString()
                         == QStringLiteral("True"));
                    if (persisted) {
                        m_pureSignal->setAutoCalEnabled(true);
                    }
                    connect(m_pureSignal.get(),
                            &PureSignal::autoCalEnabledChanged,
                            this,
                            [mac](bool on) {
                                AppSettings::instance().setValue(
                                    QStringLiteral(
                                        "hardware/%1/pureSignal/autoCalEnabled")
                                        .arg(mac),
                                    on ? QStringLiteral("True")
                                       : QStringLiteral("False"));
                                AppSettings::instance().save();
                            });
                }
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

            // F.1 — txReady → setRunning(true), GATED on interlockGranted.
            // From Thetis console.cs:29595 [v2.10.3.13] — TX-on callsite after
            // Thread.Sleep(rf_delay) in chkMOX_CheckedChanged2.
            //
            // 2026-05-20 bench fix (deck item #3 -- MOX RF-gate, then
            // 21:19 ordering refactor): if an external amp is in the
            // chain, we defer setRunning(true) until BOTH txReady AND
            // interlockGranted have fired. Whichever fires SECOND
            // triggers setRunning. We can't rely on a single arming
            // point because the two signals can race in either order
            // depending on amp ACK speed (fast TGXL ACK -> grant before
            // txReady).
            //
            // The gate is ARMED at txAboutToBegin above (before PTT_-
            // REQUESTED can fire any synchronous interlockGranted) and
            // CLEARED by the interlockGranted handler in the listener
            // wire above. Here we only flip m_txReadyReceived and call
            // setRunning if interlockGranted has already cleared the
            // gate. The grant handler does the symmetric check.
            //
            // 1500 ms failsafe armed if the grant never fires
            // (e.g. amp disconnected mid-cycle).
            connect(m_moxController, &MoxController::txReady,
                    this, [this]() {
                if (!m_txChannel) { return; }
                m_txReadyReceived = true;
                if (!m_awaitingInterlockForTx) {
                    // Either no amp in chain (gate never armed) OR the
                    // grant already cleared the gate (fast-ACK race).
                    // Either way, start TxChannel now.
                    qCInfo(lcConnection)
                        << "RF-flow gate: txReady arrived; gate already"
                           " released (or no amp). Starting TxChannel.";
                    m_txChannel->setRunning(true);
                    return;
                }
                qCInfo(lcConnection)
                    << "RF-flow gate: txReady arrived; waiting interlock"
                       "Granted before starting TxChannel";
                QTimer::singleShot(1500, this, [this]() {
                    if (m_awaitingInterlockForTx && m_txChannel) {
                        qCWarning(lcConnection)
                            << "RF-flow gate: interlockGranted didn't fire"
                               " within 1.5 s, starting TxChannel anyway"
                               " (failsafe)";
                        m_awaitingInterlockForTx = false;
                        m_txChannel->setRunning(true);
                    }
                });
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

            // Issue #153 sub-bug 2 — txAboutToBegin → pushTxModeAndBandpass.
            //
            // MoxController phase-1 signal fires synchronously BEFORE the
            // rfDelay timer that gates txReady (MoxController.cpp:505-507
            // [@501e3f5]).  pushTxModeAndBandpass dispatches setTxMode +
            // requestFilterChange to TxWorkerThread; the queued setters
            // settle well before rfDelay completes (~50 ms typical) and
            // txReady → setRunning(true) above starts the channel.
            //
            // Belt-and-suspenders re-seed at MOX-engage even though the
            // initial seed below + the dspModeChanged seed in
            // wireSliceSignals already cover the no-mode-change case.
            // Defends against any state desync caused by other code
            // paths writing TXA mode/bp0 (TUN, future PureSignal, etc.).
            connect(m_moxController, &MoxController::txAboutToBegin,
                    this, &RadioModel::pushTxModeAndBandpass);

            // 2026-05-12 bench: flush any pending FreeDV Reporter freq
            // dwell on MOX engage.  Without this, a user who tunes
            // (starts the 7 s dwell) and immediately keys would TX on
            // the new freq while the reporter dashboard still shows
            // them on the old one for up to 7 s.  Flushing here pubs
            // the cached pending freq before the radio actually starts
            // transmitting.
            connect(m_moxController, &MoxController::txAboutToBegin,
                    this, &RadioModel::flushFreedvFrequencyDwell);

            // ── Phase 3M-3a-iii Task 17 (bench fix) ───────────────────────────
            //
            // TxChannel::voxActiveChanged → MoxController::onVoxActive.
            //
            // This closes the deferred wire from 3M-1b
            // (RadioModel.cpp:756 — "onVoxActive: 3M-3a or via TxChannel
            // TX-meter polling (WDSP DEXP output)") that the 3M-3a-iii
            // implementation plan did not capture as a task.  Without it
            // [VOX] correctly enables run_vox=1 in WDSP but mic envelope
            // crossings never reach MoxController — VOX silently fails to
            // key the radio.  TxChannel registers the WDSP DEXP pushvox
            // callback in its constructor; the callback emits this signal
            // from the WDSP audio worker thread.  Qt::AutoConnection
            // promotes to QueuedConnection across the worker→main-thread
            // boundary, so MoxController::onVoxActive runs on the main
            // thread (its declared affinity — see MoxController H.5
            // comment block in this same RadioModel ctor).
            //
            // Thetis analogue: cmaster.cs:1903-1906 [v2.10.3.13] —
            //   `VOX.PushVox(int id, int active)
            //    { Audio.VOXActive = (active == 1); }`
            // wired by cmaster.cs:1125 [v2.10.3.13]
            //   `SendCBPushVox(0, PushVoxDel)`.
            // Thetis sets `Audio.VOXActive` and lets the PollPTT loop
            // notice on its next tick; NereusSDR uses direct signal-driven
            // engagement (no polling).
            connect(m_txChannel, &TxChannel::voxActiveChanged,
                    m_moxController, &MoxController::onVoxActive);

            // ── Phase 3M-3a-iii Task 18 (bench fix) ───────────────────────────
            //
            // TransmitModel::voxEnabledChanged → TxChannel::setVoxListening.
            //
            // VOX-listening mode forces the TXA pipeline pump to run when
            // VOX is enabled, so the WDSP DEXP detector can monitor mic
            // envelope even when MOX is off.  Without this gate the pump
            // only runs during MOX (driveOneTxBlock + driveOneTxBlockFromInter
            // leaved both early-return on !m_running), creating a chicken-
            // and-egg that prevents VOX from ever keying (DEXP can't fire
            // pushvox if it never sees mic).
            //
            // Wired in parallel with the existing TM::voxEnabledChanged
            // → MoxController::setVoxEnabled connect at the top of this
            // ctor (~line 669-670).  Both fire on the same TM signal:
            // MoxController gates VOX policy at the engagement layer;
            // TxChannel pumps the DSP so the policy can be evaluated.
            //
            // Receiver=m_txChannel + AutoConnection auto-routes to
            // QueuedConnection when m_txChannel lives on TxWorkerThread,
            // matching the H.1 voxRunRequested → setVoxRun pattern above.
            //
            // From Thetis wdsp/dexp.c:304 [v2.10.3.13]:
            //   "DEXP code runs continuously so it can be used to trigger
            //    VOX also."
            // Thetis's TXA pipeline pumps continuously after channel-open
            // (HPSDR EP6 audio cadence drives ChannelMaster, not MOX);
            // Audio.VOXEnabled in audio.cs:168-192 [v2.10.3.13] only
            // flips DEXP's run_vox flag via cmaster.CMSetTXAVoxRun(0).
            // NereusSDR's TxWorkerThread + m_running gate is a power-saving
            // departure from Thetis (no pumping when neither MOX nor VOX
            // is in play); this connect restores Thetis-equivalent VOX
            // detection while keeping power saving everywhere else.
            connect(&m_transmitModel, &TransmitModel::voxEnabledChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setVoxListening(on);
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

            // 3M-3a-iv: the antiVoxRun chain (TransmitModel::antiVoxRunChanged
            // -> MoxController::setAntiVoxRun -> antiVoxRunRequested ->
            // TxWorkerThread::setAntiVoxRun) is wired below near the
            // cancellation-feed connects.
            //
            // 3M-3a-iv post-bench refactor (Option A) removed the
            // antiVoxSourceWhatRequested no-op lambda that previously sat
            // here for 3F multi-pan source mux.  Thetis chkAntiVoxSource
            // (RX vs VAC at cmaster.cs:912-943 [v2.10.3.13]) does not map
            // to NereusSDR's architecture; see commit message and
            // DexpVoxPage info-row for the architectural rationale.

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

                // ── 3M-3a-iii Tasks 7-10 — DEXP (11) ──
                // Initial-sync push for the 11 DEXP TM properties so a
                // freshly-loaded profile (or a setActiveProfile invocation
                // whose setters short-circuit on no-op writes) has its DEXP
                // state reflected at WDSP. Mirrors the EQ/Lev/ALC + CFC/PhRot
                // initial-sync rationale documented above (~line 1869-1898).
                m_txChannel->setDexpRun(m_transmitModel.dexpEnabled());
                m_txChannel->setDexpDetectorTau(m_transmitModel.dexpDetectorTauMs());
                m_txChannel->setDexpAttackTime(m_transmitModel.dexpAttackTimeMs());
                m_txChannel->setDexpReleaseTime(m_transmitModel.dexpReleaseTimeMs());
                m_txChannel->setDexpExpansionRatio(m_transmitModel.dexpExpansionRatioDb());
                m_txChannel->setDexpHysteresisRatio(m_transmitModel.dexpHysteresisRatioDb());
                m_txChannel->setDexpRunAudioDelay(m_transmitModel.dexpLookAheadEnabled());
                m_txChannel->setDexpAudioDelay(m_transmitModel.dexpLookAheadMs());
                m_txChannel->setDexpLowCut(m_transmitModel.dexpLowCutHz());
                m_txChannel->setDexpHighCut(m_transmitModel.dexpHighCutHz());
                m_txChannel->setDexpRunSideChannelFilter(m_transmitModel.dexpSideChannelFilterEnabled());
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

            // ── 3M-3a-iii Tasks 7-10 — DEXP routing (11 connects) ──────────
            //
            // Routes the 11 new DEXP TransmitModel properties added in Tasks
            // 7-10 (envelope / gate ratios / look-ahead / side-channel
            // filter) into the TxChannel WDSP wrappers added in Tasks 1-5.
            // Receiver = m_txChannel so AutoConnection resolves to
            // QueuedConnection once moveToThread runs below — same pattern as
            // the F.1 / H.1-H.3 / 3M-3a-i/ii TX-chain connects above.
            //
            // No MoxController gating layer for DEXP (unlike VOX which goes
            // TM → MoxController → TxChannel for dB→linear + mic-boost
            // scaling): the DEXP TxChannel wrappers do their own ms→seconds
            // and dB→linear conversions internally (see TxChannel.h:706-914),
            // so the model layer pushes the user-visible value directly.
            //
            // Naming note: TM property names use "Ms" / "Db" / "Hz" suffixes
            // for clarity at the call-site, while TxChannel wrapper names
            // drop the unit suffix because the wrapper docstring documents
            // the unit unambiguously (e.g. setDexpDetectorTau takes ms,
            // setDexpExpansionRatio takes dB).

            // 28. dexpEnabledChanged → setDexpRun.
            connect(&m_transmitModel, &TransmitModel::dexpEnabledChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setDexpRun(on);
            });

            // 29. dexpDetectorTauMsChanged → setDexpDetectorTau.
            connect(&m_transmitModel, &TransmitModel::dexpDetectorTauMsChanged,
                    m_txChannel, [this](double tauMs) {
                m_txChannel->setDexpDetectorTau(tauMs);
            });

            // 30. dexpAttackTimeMsChanged → setDexpAttackTime.
            connect(&m_transmitModel, &TransmitModel::dexpAttackTimeMsChanged,
                    m_txChannel, [this](double attackMs) {
                m_txChannel->setDexpAttackTime(attackMs);
            });

            // 31. dexpReleaseTimeMsChanged → setDexpReleaseTime.
            connect(&m_transmitModel, &TransmitModel::dexpReleaseTimeMsChanged,
                    m_txChannel, [this](double releaseMs) {
                m_txChannel->setDexpReleaseTime(releaseMs);
            });

            // 32. dexpExpansionRatioDbChanged → setDexpExpansionRatio.
            connect(&m_transmitModel, &TransmitModel::dexpExpansionRatioDbChanged,
                    m_txChannel, [this](double dB) {
                m_txChannel->setDexpExpansionRatio(dB);
            });

            // 33. dexpHysteresisRatioDbChanged → setDexpHysteresisRatio.
            connect(&m_transmitModel, &TransmitModel::dexpHysteresisRatioDbChanged,
                    m_txChannel, [this](double dB) {
                m_txChannel->setDexpHysteresisRatio(dB);
            });

            // 34. dexpLookAheadEnabledChanged → setDexpRunAudioDelay.
            connect(&m_transmitModel, &TransmitModel::dexpLookAheadEnabledChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setDexpRunAudioDelay(on);
            });

            // 35. dexpLookAheadMsChanged → setDexpAudioDelay.
            connect(&m_transmitModel, &TransmitModel::dexpLookAheadMsChanged,
                    m_txChannel, [this](double delayMs) {
                m_txChannel->setDexpAudioDelay(delayMs);
            });

            // 36. dexpLowCutHzChanged → setDexpLowCut.
            connect(&m_transmitModel, &TransmitModel::dexpLowCutHzChanged,
                    m_txChannel, [this](double hz) {
                m_txChannel->setDexpLowCut(hz);
            });

            // 37. dexpHighCutHzChanged → setDexpHighCut.
            connect(&m_transmitModel, &TransmitModel::dexpHighCutHzChanged,
                    m_txChannel, [this](double hz) {
                m_txChannel->setDexpHighCut(hz);
            });

            // 38. dexpSideChannelFilterEnabledChanged → setDexpRunSideChannelFilter.
            connect(&m_transmitModel, &TransmitModel::dexpSideChannelFilterEnabledChanged,
                    m_txChannel, [this](bool on) {
                m_txChannel->setDexpRunSideChannelFilter(on);
            });

            // 39. txPostGenToneMagChanged → setPostGenToneMag.
            // Task 10: routes the HL2 sub-step DSP modulation value written by
            // TransmitModel::setPowerUsingTargetDbm (Task 4) into WDSP via
            // TxChannel::setPostGenToneMag → SetTXAPostGenToneMag (gen.c:800
            // [v2.10.3.13]).  Without this connect the modulation magnitude
            // computed in the HL2 path (mi0bot setup.cs:1501-1509
            // [v2.10.3.13-beta2]) never reaches the DSP engine.
            connect(&m_transmitModel, &TransmitModel::txPostGenToneMagChanged,
                    m_txChannel, [this](double mag) {
                m_txChannel->setPostGenToneMag(mag);
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

            // Plan 4 D8: per-profile TX filter → WDSP via 50 ms debounce.
            //
            // TransmitModel lives on the main thread; TxChannel lives on
            // TxWorkerThread (moved below).  We route through the intermediate
            // RadioModel::txFilterRequest signal so Qt auto-connection selects
            // QueuedConnection for TxChannel::requestFilterChange — ensuring the
            // debounce timer and WDSP call execute on the audio thread.
            //
            // Step 1: main-thread lambda captures TX-bound slice DSP mode and
            //         re-emits as txFilterRequest(low, high, mode).
            connect(&m_transmitModel, &TransmitModel::filterChanged,
                    this, [this](int audioLow, int audioHigh) {
                const SliceModel* const txSlice = txBoundSlice();
                DSPMode mode = txSlice ? txSlice->dspMode() : DSPMode::USB;
                emit txFilterRequest(audioLow, audioHigh, mode);
            });
            // Step 2: txFilterRequest (main thread sender) → requestFilterChange
            //         (audio thread slot).  Auto-connection becomes QueuedConnection
            //         after moveToThread below.
            connect(this, &RadioModel::txFilterRequest,
                    m_txChannel, &TxChannel::requestFilterChange);

            // Initial sync — push current TransmitModel state (loaded by
            // loadFromSettings at line 1106) into TxChannel before the worker
            // thread takes over.  Runs on the main thread; subsequent setter
            // calls land on TxWorkerThread via the queued connections above.
            // See line 1879-1881 for the design pattern this mirrors.
            pushTxProcessingChain();

            // ── Bench fix 2026-05-14: re-prime MoxController WDSP signals ──
            //
            // loadFromSettings (line 2631) ran BEFORE the MoxController ->
            // TxChannel connects above (lines 3604/3612/3620), so the
            // first-call NaN-sentinel emit of voxThresholdRequested /
            // voxHangTimeRequested / antiVoxGainRequested landed in a void
            // receiver and the sentinels are now consumed.  Reset them and
            // re-run the recompute helpers so the load-time values reach
            // the freshly-wired TxChannel.
            //
            // Reported bench symptom: "VOX needs juggling to prime" --
            // sliders show correct visual position on launch (model restored
            // from per-MAC AppSettings) but WDSP retains its construction-
            // time defaults until the user moves a slider.
            //
            // Thread-affinity note (PR #253 review): this call lives at the
            // initial-sync site (main thread, before m_txChannel->moveToThread
            // below), NOT inside pushTxProcessingChain.  pushTxProcessingChain
            // is reused by the activeProfileChanged connect at line ~4111
            // whose receiver is m_txChannel; after moveToThread that lambda
            // body executes on the TX worker thread, and a MoxController
            // mutation from there would race the main-thread TM -> Mox
            // setters.  Profile changes don't need the re-prime anyway: the
            // TM -> Mox -> TxChannel signal chain handles per-property
            // updates through recompute()'s computed-value guard.
            //
            // antiVoxTau and antiVoxRun are not covered here -- their TM ->
            // Mox connects are deferred to wireConnectionSignals (lines
            // 5025/5051) where an explicit re-push already happens after
            // TxWorkerThread is wired.
            if (m_moxController) {
                m_moxController->primeWdspState();
            }

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

                // ── The attach has to travel the same way the detach does ──
                //
                // Both P1RadioConnection::setTxMicSource and the P2 one carry
                // an explicit caller contract in their own bodies ("invoked on
                // this connection's affinity thread ... if a future refactor
                // reorders these RadioModel calls, this function will need
                // atomic / mutex protection"). They write m_txMicSource, which
                // the connection thread dereferences in decodeMicFrame132 /
                // the EP6 mic16 extraction, and m_lastMicAt, which the
                // connection thread reads on every keep-alive / watchdog tick.
                //
                // The contract holds on the hot path, where this runs before
                // the moveToThread further down connectToRadio. It does NOT
                // hold on the cold-start path: issue #153 sub-bug 1 captures
                // this whole txSetup lambda into a one-shot
                // WdspEngine::initializedChanged handler, and that handler
                // runs on the main thread long after the connection has been
                // moved to m_connThread and started. On a first launch with no
                // cached FFTW wisdom that window is the length of the wisdom
                // build, so it is the common case rather than a corner.
                //
                // teardownConnection already marshals its detachMicSource for
                // exactly this reason (Codex P1 fix, PR #152). Default
                // Qt::AutoConnection keeps the hot path byte-for-byte what it
                // was, because invokeMethod on an object that already lives on
                // this thread is a plain synchronous call, and only the
                // deferred retry becomes a queued QMetaCallEvent.
                //
                // Non-blocking is safe here where the detach needed
                // BlockingQueuedConnection: the detach had to complete before
                // RadioModel destroyed the TxMicSource, whereas this attach
                // hands over a source that was constructed immediately above
                // and outlives the call. The two stay correctly ordered
                // because they land in the same connection-thread FIFO.
                auto* const micSrc = m_txMicSource.get();
                if (auto* p1 = qobject_cast<P1RadioConnection*>(m_connection)) {
                    QMetaObject::invokeMethod(p1, [p1, micSrc]() {
                        p1->setTxMicSource(micSrc);
                    });
                } else if (auto* p2 = qobject_cast<P2RadioConnection*>(m_connection)) {
                    QMetaObject::invokeMethod(p2, [p2, micSrc]() {
                        p2->setTxMicSource(micSrc);
                    });
                }

                // PC mic override gate (Thetis cmaster.c:379 [v2.10.3.13]).
                // micSourceChanged emits MicSource enum; the slot needs a
                // bool ("is PC"), so funnel through a lambda.
                connect(&m_transmitModel, &TransmitModel::micSourceChanged,
                        m_audioEngine, [this](MicSource src) {
                            m_audioEngine->onMicSourceChanged(src == MicSource::Pc);
                            m_audioEngine->onMicSourceChangedVax(src == MicSource::Vax);
                        });
                m_audioEngine->onMicSourceChanged(
                    m_transmitModel.micSource() == MicSource::Pc);
                m_audioEngine->onMicSourceChangedVax(
                    m_transmitModel.micSource() == MicSource::Vax);

                m_txWorker = std::make_unique<TxWorkerThread>(this);
                m_txWorker->setTxChannel(m_txChannel);
                m_txWorker->setAudioEngine(m_audioEngine);
                m_txWorker->setMicSource(m_txMicSource.get());
                m_txChannel->moveToThread(m_txWorker.get());
                m_txWorker->startPump();

                qCInfo(lcDsp) << "TX pump: TxWorkerThread started"
                              << "blockFrames=" << TxWorkerThread::kBlockFrames
                              << "(semaphore-wake, mic-frame-driven — 3M-1c v3)";

                // ── Phase 3R K-bench: pre-RADE mic gain + leveler wiring ──
                //
                // Push the current TransmitModel state to the worker so
                // the RADE branch can apply mic gain + leveler in real
                // time. Subsequent property changes propagate via
                // queued signal/slot.
                if (m_txWorker) {
                    TxWorkerThread* w = m_txWorker.get();
                    w->setRadeMicGainDb(m_transmitModel.micGainDb());
                    w->setRadeLeveler(m_transmitModel.txLevelerOn(),
                                      m_transmitModel.txLevelerMaxGain(),
                                      m_transmitModel.txLevelerDecay());
                    qCInfo(lcDsp)
                        << "RADE pre-encode init: micGain="
                        << m_transmitModel.micGainDb() << "dB"
                        << "lev_on=" << m_transmitModel.txLevelerOn()
                        << "lev_max=" << m_transmitModel.txLevelerMaxGain()
                        << "lev_decay=" << m_transmitModel.txLevelerDecay();
                    // micGainDb has no Qt signal of its own; piggy-back on
                    // the existing micPreampChanged signal which fires on
                    // setMicPreamp(dB) and on profile loads. Lambda
                    // forwards the dB value through to the worker.
                    connect(&m_transmitModel, &TransmitModel::micPreampChanged,
                            w, [w, this](int /*dB*/) {
                                w->setRadeMicGainDb(
                                    m_transmitModel.micGainDb());
                            });
                    connect(&m_transmitModel, &TransmitModel::txLevelerOnChanged,
                            w, [w, this](bool /*on*/) {
                                w->setRadeLeveler(
                                    m_transmitModel.txLevelerOn(),
                                    m_transmitModel.txLevelerMaxGain(),
                                    m_transmitModel.txLevelerDecay());
                            });
                    connect(&m_transmitModel,
                            &TransmitModel::txLevelerMaxGainChanged,
                            w, [w, this](int /*dB*/) {
                                w->setRadeLeveler(
                                    m_transmitModel.txLevelerOn(),
                                    m_transmitModel.txLevelerMaxGain(),
                                    m_transmitModel.txLevelerDecay());
                            });
                    connect(&m_transmitModel,
                            &TransmitModel::txLevelerDecayChanged,
                            w, [w, this](int /*ms*/) {
                                w->setRadeLeveler(
                                    m_transmitModel.txLevelerOn(),
                                    m_transmitModel.txLevelerMaxGain(),
                                    m_transmitModel.txLevelerDecay());
                            });
                }

                // ── Phase 3R K-bench: retroactive RADE wire-up ─────────
                //
                // loadSliceState (called at line ~2179) runs BEFORE both
                // m_wdspEngine init AND m_txWorker creation. If the
                // persisted slice mode is RADE_U or RADE_L:
                //
                //   (a) SliceModel::setDspMode ran with engine==nullptr,
                //       so NO RadeChannel was ever created. The mode
                //       swap branch silently no-op'd.
                //   (b) wireRadeChannel was therefore never called, so
                //       NEITHER the non-TxWorker nor the TxWorker
                //       connects exist.
                //   (c) User-visible symptom: app starts in RADE mode
                //       but TX/RX silently does nothing until the user
                //       toggles to SSB then back to RADE, which triggers
                //       a fresh setDspMode with engine available.
                //
                // Fix: now that m_wdspEngine + m_txWorker are alive AND
                // the TX-bound slice already carries the RADE DSPMode,
                // synthesize the work setDspMode would have done. Create
                // RadeChannel, configure sideband + start, and call
                // wireRadeChannel which establishes all the connects.
                SliceModel* const txRadeSlice = txBoundSlice();
                if (txRadeSlice && m_wdspEngine) {
                    const DSPMode mode = txRadeSlice->dspMode();
                    if (mode == DSPMode::RADE_U || mode == DSPMode::RADE_L) {
                        const int sliceId = txRadeSlice->sliceIndex();
                        RadeChannel* radeCh =
                            m_wdspEngine->radeChannel(sliceId);
                        if (radeCh == nullptr) {
                            qCInfo(lcDsp)
                                << "RADE: creating channel" << sliceId
                                << "at WDSP-init time (persisted mode"
                                   "was RADE; setDspMode's create branch"
                                   "had no engine)";
                            radeCh = m_wdspEngine->createRadeChannel(sliceId);
                            if (radeCh != nullptr) {
                                radeCh->setSideband(
                                    mode == DSPMode::RADE_U);
                                wireRadeChannel(sliceId, radeCh,
                                                txRadeSlice);
                                // start() reads Rade/ModelPath
                                // AppSettings or falls back to "dummy"
                                // sentinel (librade has weights baked
                                // in per Phase A2b finding).
                                const QString modelPath =
                                    AppSettings::instance()
                                        .value("Rade/ModelPath",
                                               QString())
                                        .toString();
                                radeCh->start(
                                    modelPath.isEmpty()
                                        ? QStringLiteral("dummy")
                                        : modelPath);
                            }
                        } else {
                            // RadeChannel already exists (mode-swap
                            // path created it). The non-TxWorker
                            // connects also exist. Re-wire only the
                            // TxWorker-side bits.
                            qCInfo(lcDsp)
                                << "RADE: retroactive TxWorker wire-up"
                                   "for slice" << sliceId;
                            m_txWorker->setRadeChannel(radeCh);
                            connect(m_txWorker.get(),
                                    &TxWorkerThread::radeMicBlockReady,
                                    radeCh, &RadeChannel::txEncode,
                                    Qt::QueuedConnection);
                            if (m_dspWorker) {
                                m_dspWorker->setRadeChannel(radeCh);
                            }
                        }
                    }
                }

                // ── Phase 3R K-bench: FreeDV Reporter TX-state push ──
                //
                // Mirror MOX state to the FreeDV Reporter so other
                // operators see our TX indicator (red row in their
                // reporter dialog). Mode string follows freedv-gui's
                // convention: "RADE" when in either RADE_U or RADE_L,
                // empty for non-RADE modes (the reporter only cares
                // about RADE/FreeDV-mode TX events).
                if (m_moxController != nullptr && m_freeDvReporter) {
                    connect(m_moxController, &MoxController::moxStateChanged,
                            this, [this](bool active) {
                                if (!m_freeDvReporter
                                    || !m_freeDvReporter->isConnected()) {
                                    return;
                                }
                                QString mode;
                                if (const SliceModel* const txSlice =
                                        txBoundSlice()) {
                                    const DSPMode m = txSlice->dspMode();
                                    if (m == DSPMode::RADE_U
                                        || m == DSPMode::RADE_L) {
                                        // freedv-gui FREEDV_MODE_RADE
                                        // wire string [@77e793a]
                                        mode = QStringLiteral("RADEV1");
                                    }
                                }
                                m_freeDvReporter->setTransmitting(active, mode);
                            });
                }

                // ── Phase 3R Task K2: mode-aware path swap on MOX-on ──
                //
                // On every MOX-on transition, read the TX-bound slice's
                // DSPMode and post a TxPath swap to the worker.  DSPMode
                // == RADE -> TxPath::Rade (scaffolded; full integration
                // K-bench).  Anything else -> TxPath::Wdsp (the existing
                // path).  The moxStateChanged signal fires exactly once
                // per MOX transition at the END of the timer walk
                // (MoxController.h:863-865 [v2.10.3.13 conceptual]); the
                // RX path doesn't need a corresponding TxPath flip
                // because dispatchOneBlock is gated on the worker pump
                // running anyway.
                if (m_moxController != nullptr && m_txWorker) {
                    TxWorkerThread* worker = m_txWorker.get();
                    connect(m_moxController, &MoxController::moxStateChanged,
                            this, [this, worker](bool active) {
                                if (!active) {
                                    return;   // released; pump will idle anyway
                                }
                                const SliceModel* const txSlice =
                                    txBoundSlice();
                                const DSPMode mode =
                                    txSlice ? txSlice->dspMode()
                                            : DSPMode::USB;
                                const bool isRade =
                                    (mode == DSPMode::RADE_U
                                     || mode == DSPMode::RADE_L);
                                const TxWorkerThread::TxPath path =
                                    isRade
                                        ? TxWorkerThread::TxPath::Rade
                                        : TxWorkerThread::TxPath::Wdsp;
                                worker->setCurrentTxPath(path);
                            });
                }
            }

            qCInfo(lcDsp) << "L.1: mic sources constructed (hasMicJack=" << hasMicJack
                          << "); composite router wired to TxChannel;"
                          << " 5 signal connections + K.2 moxCheck installed.";
            qCInfo(lcDsp) << "G.1: TX channel 1 created (deferred until conn live)"
                          << "outRate=" << txOutRate
                          << "— SSB voice path ready (L.1 composite router wired).";

            // Issue #153 sub-bug 2 — initial TXA mode/bandpass seed.
            //
            // m_txChannel is alive and the arbiter has a stable binding.
            // Push that slice's persisted dspMode + filterLow/filterHigh so
            // SSB MOX no longer requires a prior TUN press to seed TXA
            // mode (default LSB) and bp0 cutoffs (default -5000..-100).
            //
            // Source: Thetis SetTXFilters at console.cs:8091 [v2.10.3.13]
            // + CurrentDSPMode setter at radio.cs:2670-2696 [v2.10.3.13];
            // Thetis seeds at mode-change (console.cs:33937) + at
            // chkPower → txtVFOAFreq_LostFocus path indirectly via the
            // SetupTxFilters() preamble.  NereusSDR consolidates into
            // one helper called at three triggers (this is trigger #1 of 3).
            pushTxModeAndBandpass();

            // Bench 2026-05-11: initial audioVolume seed — first MOX
            // produced no modulation until a TUN press primed the path.
            //
            // Root cause: m_lastAudioVolume defaults to 0.  pumpAudioVolume
            // (the Audio.RadioVolume setter analogue at
            // RadioModel.cpp:6458) only runs when audioVolumeChanged fires,
            // which only happens inside setPowerUsingTargetDbm.  At fresh
            // launch nothing calls setPowerUsingTargetDbm until either (a)
            // the user moves the power slider, or (b) TUNE engages, so the
            // wire drive byte and TXFixedGain IQ scalar both stay at 0
            // through the first MOX.  TUNE inadvertently primes this
            // because TUNE-on calls setPowerUsingTargetDbm(bFromTune=true)
            // and TUNE-off restores via setPowerUsingTargetDbm(bFromTune=
            // false).  Same bug class as sub-bug 2 above — Thetis does
            // not need an explicit seed because chkPower / txtVFOAFreq_
            // LostFocus already drove the chain at construction time on
            // managed-thread startup; NereusSDR's Qt signal model means
            // the construction-time setPower(default) emit is dropped
            // because connection / paProfile aren't ready yet.
            //
            // Seed by reading the user's persisted power slider value via
            // the bFromTune=false / bSetPower=true path — same code path
            // the drive-slider lambda at RadioModel.cpp:1093 takes when
            // the user moves the slider.  Emits audioVolumeChanged,
            // pumpAudioVolume runs, wire byte and IQ gain land non-zero
            // before the first MOX engage.
            //
            // Source-first cite: same chain as RadioModel.cpp:1093 —
            // setPowerUsingTargetDbm is a port of Thetis's NetworkIO.
            // SetOutputPower + cmaster.CMSetTXOutputLevel
            // (audio.cs:262-271 + NetworkIO.cs:201-211 + cmaster.cs:
            // 1115-1119 [v2.10.3.13]).
            if (m_paProfileManager) {
                const PaProfile* prof = m_paProfileManager->activeProfile();
                const SliceModel* const txSlice = txBoundSlice();
                if (prof && txSlice) {
                    const Band currentBand =
                        bandFromFrequency(txSlice->frequency());
                    (void)m_transmitModel.setPowerUsingTargetDbm(
                        *prof, currentBand, /*bSetPower=*/true,
                        /*bFromTune=*/false, /*bTwoTone=*/false,
                        m_hardwareProfile.model);
                    qCInfo(lcDsp)
                        << "Initial audioVolume seed pumped — first MOX "
                           "drive byte / IQ scalar now non-zero without "
                           "requiring TUN priming";
                }
            }
        };  // end of txSetup lambda
        txSetup();

        if (!m_txChannel) {
            // Issue #153 sub-bug 1 — cold-start retry hook.
            //
            // WDSP not yet initialized at connectToRadio time (typical
            // first-launch case with no cached FFTW wisdom — async wisdom
            // build can take ~15 minutes on a fresh install).  Register a
            // one-shot connect to WdspEngine::initializedChanged(true)
            // that re-runs the captured txSetup lambda.  receiver=this so
            // the slot runs on RadioModel's main thread; QMetaObject
            // disconnects automatically when this RadioModel is destroyed.
            //
            // The retry self-disconnects on first successful run.  If WDSP
            // never initializes (e.g. wisdom build aborted) the connection
            // remains harmlessly attached until RadioModel teardown.
            auto retry = std::make_shared<QMetaObject::Connection>();
            *retry = connect(m_wdspEngine, &WdspEngine::initializedChanged,
                             this,
                             [this, retry, txSetup = std::move(txSetup)](bool ready) {
                if (!ready || m_txChannel) {
                    return;
                }
                txSetup();
                if (m_txChannel) {
                    QObject::disconnect(*retry);
                    qCInfo(lcDsp) << "Issue #153 sub-bug 1: TxChannel deferred-create "
                                     "succeeded after WdspEngine::initializedChanged(true).";
                }
            });
            // No Qt::UniqueConnection here: it is only implemented for
            // pointer-to-member slots, and with a lambda Qt returns an INVALID
            // connection instead (qobject.h:263-269 leaves pSlot null, then
            // connectImpl warns and bails). This retry carried it, so it was
            // never connected -- meaning the whole Issue #153 sub-bug 1 fix was
            // inert and a cold-start connect never got its TxChannel.
            //
            // Nothing is lost by dropping it. The capture list makes a member
            // pointer impossible, and duplicates are already harmless: the body
            // early-returns on m_txChannel and self-disconnects on the first
            // success.
            qCWarning(lcDsp) << "Issue #153 sub-bug 1: createTxChannel(kTxChannelId) returned nullptr "
                                "at connect-time (WDSP not yet initialized — likely cold-"
                                "start with no cached wisdom).  Registered one-shot retry "
                                "on WdspEngine::initializedChanged.";
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

    // Per-MAC P1 ADC routing override (Thetis `P1_adc_cntrl`).
    //
    // Thetis stores per-DDC ADC selection in a separate 14-bit global
    // (console.cs:15120 [v2.10.3.13]) edited via Setup form's
    // radP1DDC*ADC* radio buttons. NereusSDR P1RadioConnection mirrors
    // this in m_p1AdcCntrl; applyBoardQuirks() seeds a sensible board
    // default (HL2 / 2-ADC → 4, Hermes / HermesII → 0). If the user
    // (or the future Setup → Hardware → P1 ADC Routing page) has
    // persisted a per-MAC override under hardware/<mac>/p1AdcCntrl,
    // apply it now so the first bank-4 emit goes out with the user's
    // chosen routing.
    //
    // Done before m_connection->moveToThread() below so the synchronous
    // setter is safe. The board default in applyBoardQuirks() has
    // already run during connectToRadio() preflight; this is a strict
    // override on top of that.
    if (auto* p1 = qobject_cast<class P1RadioConnection*>(m_connection)) {
        const QVariant persisted = AppSettings::instance().hardwareValue(
            info.macAddress, QStringLiteral("p1AdcCntrl"));
        if (persisted.isValid()) {
            bool ok = false;
            const int bits = persisted.toString().toInt(&ok, 0);  // base 0: accepts "0x14" too
            if (ok) {
                p1->setP1AdcCntrl(bits);
            } else {
                qCWarning(lcConnection) << "P1: hardware/" << info.macAddress
                                        << "/p1AdcCntrl = '" << persisted.toString()
                                        << "' is not a valid integer; using board default";
            }
        }
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

    // 2026-05-25 KG4VCF bench fix: elevate the connection thread.
    // It runs recvfrom() in a tight loop pulling UDP I/Q packets off
    // the wire and parsing them; if it gets preempted by a heavy
    // compile, the kernel UDP receive queue can overflow and packets
    // get dropped, producing audible glitches upstream of the DSP
    // feeder.  2026-05-26 bench: USER_INITIATED was not enough -- under
    // heavy build load compile workers still scheduled in and the
    // connect thread missed packet wake-ups long enough to drop frames.
    // Bumped to USER_INTERACTIVE so it sits in the same scheduling
    // class as the audio + GUI threads.
    connect(m_connThread, &QThread::started, m_connection,
            []() { NereusSDR::elevateLatencyCriticalThreadPriority(); });

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

    // Keep ReceiverManager's m_rx1Rate in sync so its PsDdcConfig
    // observation consumers report the same rate as the complete assignment.
    // Protocol 2 wire state itself is owned by applyDdcAssignment.
    m_receiverManager->setRx1Rate(wireSampleRate);
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
    // The user's PureSignal-enable toggle (driven from PsForm + persisted
    // under hardware/<mac>/pureSignal/enabled — Phase 3M-4 retired the
    // Setup → Hardware → PureSignal tab in favour of PsForm) is the proxy
    // for the wire bit — same semantic as Thetis PSForm.cs:240 [v2.10.3.13]
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

    // Configure and start the FlexRadio UDP 4992 discovery beacon so PGXL/TGXL
    // can auto-discover NereusSDR in their FlexRadio dropdown. The serial is
    // derived from the radio MAC via the same SHA-256 path used in
    // onPgxlConnected(); both call derivedFlexSerial() so the values match.
    if (m_flexBroadcaster) {
        const AppSettings& as = AppSettings::instance();
        const QString mac = info.macAddress.isEmpty()
                                ? QStringLiteral("00:00:00:00:00:00")
                                : info.macAddress;
        m_flexBroadcaster->setMacAddress(mac);
        m_flexBroadcaster->setSerial(derivedFlexSerial(mac));
        m_flexBroadcaster->setVersion(QStringLiteral(NEREUSSDR_VERSION));
        m_flexBroadcaster->setCallsign(
            as.value(QStringLiteral("StationCallsign"),
                     QStringLiteral("NEREUS")).toString());
        m_flexBroadcaster->setNickname(
            as.value(QStringLiteral("PGXL_BroadcastNickname"),
                     QStringLiteral("NereusSDR")).toString());
        m_flexBroadcaster->setModel(
            as.value(QStringLiteral("PGXL_DiscoveryModel"),
                     QStringLiteral("FLEX-6400")).toString());

        // Route-lookup hint: if we know PGXL's IP, ask the kernel which local
        // source IP it would use to reach PGXL, and bind the beacon to that
        // same source. This keeps the beacon's source IP consistent with the
        // amplifier-create TCP source IP. Without this, on multi-interface
        // hosts (e.g. macOS with feth* virtual ethernets alongside en0) the
        // beacon can advertise one local IP while the TCP control connection
        // uses another, and PGXL's SmartSDR-API pull silently fails.
        //
        // Per-radio peripherals refactor (2026-05-26): pull PGXL_ManualIp
        // from the per-MAC peripherals scope.  We use the raw hardwareValue
        // lookup here (not peripheralValue) because m_lastRadioInfo isn't
        // populated yet at this point in connectToRadio -- the local `info`
        // is the source of truth for this branch.
        const QString pgxlMac = info.macAddress.isEmpty()
                                    ? QStringLiteral("00:00:00:00:00:00")
                                    : info.macAddress;
        const QString pgxlIpStr = as.hardwareValue(
            pgxlMac,
            QStringLiteral("peripherals/PGXL_ManualIp"),
            QString{}).toString();
        if (!pgxlIpStr.isEmpty()) {
            m_flexBroadcaster->setPeerHint(QHostAddress(pgxlIpStr));
        }

        const bool enabled =
            as.value(QStringLiteral("PGXL_BroadcastDiscovery"),
                     QStringLiteral("True")).toString()
            == QStringLiteral("True");
        if (enabled) {
            m_flexBroadcaster->start();
        }
    }

    // Tell MainWindow / FFTEngine / SpectrumWidget the wire rate so bin math
    // matches the persisted hardware rate. Without this the FFT uses a stale
    // rate and compresses/expands the spectrum incorrectly.
    // Phase 3Q sub-PR-3: persist so connectionSampleRateHz() can report it.
    m_connectionSampleRateHz = wireSampleRate;
    emit wireSampleRateChanged(static_cast<double>(wireSampleRate));

    // Task 1.7: record active-RX count so setActiveRxCountLive() can
    // report idempotent (same-count) calls correctly.
    m_connectionActiveRxCount = activeRxCount;

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
    // Every slice, each to its own WDSP channel. Slices created later are
    // wired by addSlice; this covers the ones restored before connect.
    for (SliceModel* s : std::as_const(m_slices)) {
        wireSliceSignals(s);
    }

    // --- I/Q data → ReceiverManager → DSP worker → WDSP → AudioEngine ---
    // Route through ReceiverManager for DDC-aware mapping, then dispatch
    // to RxDspWorker on its own thread for fexchange2 processing.

    // Step 1: RadioConnection I/Q → ReceiverManager (DDC routing).
    // Auto connection: m_connection is on its worker thread, this is on
    // main, so the slot is queued onto the main thread.
    //
    // Phase 3M-4 Task 17 chunk D — receiver routing only.
    //
    // PsccPump no longer subscribes here.  As of the 2026-05-23 source-first
    // rewrite it consumes RadioConnection::psPairedIqDataReceived (a
    // packet-paired signal emitted once per multi-stream UDP packet by
    // P2RadioConnection's deinterleave loop), wired below.  The old
    // per-DDC fork into PsccPump::onIqData drove the legacy independent-
    // rings architecture and could drift by ~189 samples between TX
    // monitor and PS feedback under Qt queued-connection scheduling.
    // Lever 2 (2026-05-24): DirectConnection so this lambda runs on the
    // Connection thread, not main.  feedIqData has a recursive mutex
    // covering the receiver-map reads; the downstream emit of
    // iqDataForReceiver then drops into either a DirectConnection lambda
    // (Step 2a below where rawIqData is re-emitted) or a QueuedConnection
    // to the DSP worker (Step 2b).  Net result: I/Q packets reach FFT and
    // DSP without ever sitting in the main thread's event queue, so a
    // paint-busy main thread no longer stalls audio + waterfall together.
    connect(m_connection, &RadioConnection::iqDataReceived,
            this, [this](int ddcIndex, const QVector<float>& samples) {
        m_receiverManager->feedIqData(ddcIndex, samples);
    }, Qt::DirectConnection);

    // Phase 3M-4 bench-fix 2026-05-23 (J.J. Boyd KG4VCF): source-first
    // PS pairing.  RadioConnection emits psPairedIqDataReceived once per
    // packet that carries both PS DDCs (P2RadioConnection.cpp deinterleave
    // loop + future P1RadioConnection EP6 deinterleave).  Both buffers
    // are extracted in the same xrouter-equivalent pass, mirroring Thetis
    // sync.c:53-58 [v2.10.3.15] InboundBlock(id=1) where pscc() takes two
    // pointers that reference per-stream buffers from the same call.
    //
    // QVector<float> is NOT auto-metatyped (no Q_DECLARE_METATYPE), and
    // the connection lives on the worker thread while PsccPump lives on
    // main — so we go through a main-thread lambda for the same reason
    // the iqDataReceived path does.
    connect(m_connection, &RadioConnection::psPairedIqDataReceived,
            this, [this](int psFbDdc, const QVector<float>& psFbSamples,
                         int txMonDdc, const QVector<float>& txMonSamples) {
        if (m_psccPump) {
            m_psccPump->onPsPairedIqData(psFbDdc, psFbSamples,
                                         txMonDdc, txMonSamples);
        }
    });

    // Protocol 2 codec injection remains live for ReceiverManager's
    // ddcConfigChanged observation consumers (notably PsccPump). The P2 wire
    // deliberately does not consume that partial PsDdcConfig signal:
    // refreshDdcAssignmentForRadioState sends one full DdcAssignment instead.
    if (auto* p2 = qobject_cast<NereusSDR::P2RadioConnection*>(m_connection)) {
        // Phase 3M-4 Task 17 — feed the per-board codec into ReceiverManager
        // so updateDdcAssignment() can produce a non-empty PsDdcConfig.
        // Without this, ReceiverManager::m_p2Codec stays null and
        // applyPureSignalDdcConfig is never invoked, so ddcConfigChanged
        // observation consumers never receive the PS pair. Fires once when
        // selectCodec runs at connectToRadio time.
        connect(p2, &P2RadioConnection::p2CodecChanged, this, [this, p2]() {
            m_receiverManager->setP2Codec(p2->p2Codec());
        });
        // Race: if selectCodec already fired before this connect (the
        // codec is selected on the connection thread, signal posted via
        // queued auto-connection — should be after our connect), poll
        // once to catch up.  Cheap idempotent setter.
        if (auto* codec = p2->p2Codec()) {
            m_receiverManager->setP2Codec(codec);
        }

        // ── Phase 3F Sub-Epic F Task 5: wideband frame -> per-ADC FFT ──
        // P2RadioConnection::widebandFrameReady fires on the connection
        // thread once a 32-packet frame (16384 normalized real samples)
        // is assembled by WidebandFrameAccumulator (Sub-Epic F Task 3).
        // We hop to the main thread (auto-connection: default) so the
        // FFT runs out of the network hot path.  The 16k-pt real-to-
        // complex FFT typically completes well under one frame period
        // even at 153.6 MHz; if profiling later flags this as a stall,
        // move WidebandFftEngine to a dedicated worker thread.
        connect(p2, &P2RadioConnection::widebandFrameReady, this,
                [this](int adcIdx, const QVector<float>& samples) {
            if (adcIdx < 0 || adcIdx >= 2) { return; }
            if (!m_widebandFftEngines[adcIdx]) { return; }
            QVector<float> bins;
            m_widebandFftEngines[adcIdx]->computeFft(samples, bins);
            emit widebandSpectrumReady(adcIdx, bins);
        });
    }

    // Phase 3M-4 Task 17 P1 follow-up: P1 mirror of the P2 block above.
    //
    // For P1 boards the PureSignal DDC routing lands in a mix of bank-byte
    // updates rather than a single CmdRx — but the dispatch chain is the
    // same: ReceiverManager::ddcConfigChanged →
    // P1RadioConnection::applyPsDdcConfig writes m_adcCtrl / m_psNDdc /
    // m_activeRxCount, then arms bank 0 + bank 4 flush flags so the new
    // routing lands within ≤2 frames.
    //
    // Required for HL2 / Hermes / ANAN10 / ANAN100 (nddc=4 boards — DDC
    // routing needs cntrl1=4 ADC steering during PS-MOX) and HermesII /
    // ANAN10E / ANAN100B (nddc=2 boards — same plus the bank 2/3 freq
    // override which fires off m_psNDdc + m_mox + m_puresignalRun).
    if (auto* p1 = qobject_cast<NereusSDR::P1RadioConnection*>(m_connection)) {
        connect(m_receiverManager, &ReceiverManager::ddcConfigChanged,
                p1, &P1RadioConnection::applyPsDdcConfig,
                Qt::QueuedConnection);

        connect(p1, &P1RadioConnection::p1CodecChanged, this, [this, p1]() {
            m_receiverManager->setP1Codec(p1->p1Codec());
        });
        if (auto* codec = p1->p1Codec()) {
            m_receiverManager->setP1Codec(codec);
        }
    }

    // Step 2a: ReceiverManager → spectrum fork.
    // Lever 2 (2026-05-24): DirectConnection so this lambda runs on the
    // Connection thread (same thread that just emitted iqDataForReceiver
    // from feedIqData).  emit rawIqData() is itself thread-safe; its
    // subscribers (FFTEngine on SpectrumThread) use QueuedConnection and
    // run on their own threads, so the cross-thread queueing happens at
    // the FFT consumer boundary, not here.  Main thread never sees the
    // I/Q packet.
    //
    // Phase 3F Sub-Epic I Task 8: the logical receiver index IS the stream
    // index (plan invariant 2), so it is republished as rawIqDataForStream
    // for MainWindow's per-stream FFTEngine pool. rawIqData is kept
    // untagged for the existing single-stream subscribers.
    connect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
            this, [this](int receiverIndex, const QVector<float>& samples) {
        emit rawIqData(samples);
        emit rawIqDataForStream(receiverIndex, samples);
    }, Qt::DirectConnection);

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

    // 2026-05-25 KG4VCF bench fix: elevate the DSP thread to real-time
    // audio scheduling so heavy system load (parallel compiles, Spotlight
    // indexing, Time Machine snapshots) does not preempt the audio
    // feeder and cause ring underruns / audible jitter.  Wired on
    // started()/finished() so the elevation runs ON the DSP thread,
    // which is what pthread_set_qos / os_workgroup_join require.
    //
    // Order matters: connect onThreadFinished BEFORE the deleteLater
    // below so Qt fires onThreadFinished first (signals fire slots in
    // connect order).  deleteLater is deferred to the next event loop
    // pass so it would not actually destroy m_dspWorker before
    // onThreadFinished runs, but the explicit order makes the
    // dependency obvious to future readers.
    connect(m_dspThread, &QThread::started,
            m_dspWorker, &RxDspWorker::onThreadStarted);
    connect(m_dspThread, &QThread::finished,
            m_dspWorker, &RxDspWorker::onThreadFinished);

    connect(m_dspThread, &QThread::finished,
            m_dspWorker, &QObject::deleteLater);
    connect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
            m_dspWorker, &RxDspWorker::processIqBatch,
            Qt::QueuedConnection);

    // External diversity needs both physical DDC legs. ReceiverManager maps
    // only the designated primary onto a logical user stream; the synchronized
    // partner deliberately has no logical receiver. Fork the raw hardware-DDC
    // signal directly to the worker once, while leaving ReceiverManager's
    // ordinary fan-out above unchanged for every co-hosted slice.
    connect(m_connection, &RadioConnection::iqDataReceived,
            m_dspWorker, &RxDspWorker::processExternalDiversityIqBatch,
            Qt::QueuedConnection);
    m_dspThread->start();

    // ── Phase 3F Sub-Epic I closeout, defect F1 ─────────────────────────
    //
    // Retroactive stream-binding push, same lifecycle gotcha as the RADE
    // wire-up immediately below. connectToRadio sizes the stream pool and
    // binds every slice several thousand lines EARLIER than this function,
    // so every one of those republishStreamBindings calls hit the
    // `if (m_dspWorker)` guard while the pointer was still null. The worker
    // therefore began life knowing only its constructor seed
    // ({stream 0: [slice 0]}), and any slice on a non-zero stream was
    // accumulated, drained and then demodulated by nobody.
    //
    // First connect survived on that seed alone. Reconnect did not: the
    // slices kept their streamIndex across teardown, so connectToRadio's
    // `streamIndex() < 0` bind loop skipped all of them and nothing
    // republished at all. releaseStreamBindings() in teardownConnection is
    // the other half of this fix.
    //
    // Publishes every stream, not just the occupied ones: an idle stream
    // must be explicitly declared empty so the constructor seed cannot
    // leave slice 0 attached to a stream it no longer sits on.
    republishAllStreamBindings();

    // A persisted diversity flag may have produced its assignment before the
    // worker existed. Publish the already-computed source ownership now that
    // both the raw feed and DSP thread are live.
    reconcileExternalDiversityRoute(computeDdcAssignment());

    // Phase 3R K-bench: retroactive RADE RX wire-up.
    //
    // Same lifecycle gotcha as the TxWorker retroactive create at
    // line ~3700: wireRadeChannel ran earlier (at WDSP-init time)
    // when m_dspWorker was still nullptr, so its
    //   if (m_dspWorker) { m_dspWorker->setRadeChannel(channel); }
    // block silently no-op'd. m_dspWorker is alive now; push the
    // current RadeChannel pointer so RxDspWorker can route I/Q to
    // RadeChannel::processIq on the RADE branch.
    if (m_activeSlice && m_wdspEngine) {
        const DSPMode mode = m_activeSlice->dspMode();
        if (mode == DSPMode::RADE_U || mode == DSPMode::RADE_L) {
            RadeChannel* radeCh =
                m_wdspEngine->radeChannel(m_activeSlice->sliceIndex());
            if (radeCh != nullptr) {
                qCInfo(lcDsp)
                    << "RADE: retroactive RxDspWorker wire-up for"
                       "slice" << m_activeSlice->sliceIndex();
                m_dspWorker->setRadeChannel(radeCh);
            }
        }
    }

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

    // ── Issue #182: TransmitModel::micPttDisabled → RadioConnection wire bit ──
    // Mirror the persisted user preference onto the radio firmware and prime
    // the connection with the current model value so a fresh connect honours
    // whatever the user last saved (the Setup -> Audio -> TX Input ->
    // Mic PTT Disabled checkbox).
    //
    // Source: Thetis console.cs:19761-19764 [v2.10.3.13+501e3f51]:
    //   set {
    //       mic_ptt_disabled = value;
    //       NetworkIO.SetMicPTT(Convert.ToInt32(value));
    //   }
    // The MicPTTDisabled property setter pushes to NetworkIO unconditionally,
    // so the radio sees every UI flip.  NereusSDR mirrors that via a queued
    // signal/slot bind here, and primes once below.
    connectMicPttDisabledSignal();

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
    // The user's PureSignal-enable toggle (driven from PsForm + persisted
    // under hardware/<mac>/pureSignal/enabled — Phase 3M-4 retired the
    // Setup → Hardware → PureSignal tab in favour of PsForm) is the proxy
    // for the wire bit — same semantic as Thetis's PSEnabled property
    // setter calling NetworkIO.SetPureSignal(1).  The P2 override (Task
    // 2.3) stores the flag for symmetric API only and emits nothing on the
    // wire until the live PS coordinator wires up the feedback DDC routing.
    QObject::connect(&m_transmitModel, &TransmitModel::pureSigChanged,
                     m_connection, &RadioConnection::setPuresignalRun,
                     Qt::QueuedConnection);

    // ── Phase 3M-3a-iv Task 9: anti-VOX cancellation feed wiring ─────────
    //
    // Closes the cancellation-feed wire chain end-to-end: the mixed RX
    // audio block produced by AudioEngine is handed to TxWorkerThread,
    // which (when m_antiVoxRun is true) pumps it into
    // TxChannel::sendAntiVoxData → WDSP DEXP's anti-VOX detector.  The
    // detector then biases the VOX threshold downward so RX-bleed bursts
    // no longer trip VOX.
    //
    // Thetis equivalent: the per-transmitter aamix instance
    // (cmaster.c:159-175 [v2.10.3.15]) that mixes N sub-receivers into one
    // anti-VOX stream and calls SendAntiVOXData.  3M-3a-iv shipped without
    // the mixer, pumping slice A's block directly because only one RX was
    // ever audible; Phase 3F Sub-Epic J Task 9 put the real mixer in
    // AudioEngine and re-pointed this chain at it.
    //
    // Placement note: these connects live at the end of
    // wireConnectionSignals (rather than the txSetup lambda where the
    // existing antiVoxGainRequested connect sits) because m_dspWorker is
    // not constructed until earlier in this same wireConnectionSignals
    // method (line ~2928).  By the time we reach this point, both
    // m_dspWorker (sender) and m_txWorker (constructed in the txSetup
    // lambda before connectToRadio called us) are alive.
    if (m_dspWorker != nullptr && m_txWorker != nullptr && m_moxController != nullptr) {
        // ── Phase 3F Sub-Epic J Task 9 ───────────────────────────────────
        // AudioEngine::antiVoxBlockReady → TxWorkerThread::onAntiVoxBlockReady.
        //
        // Was RxDspWorker::antiVoxSampleReady, which forked the audio of
        // whichever stream hosted slice 0 and so let the canceller hear
        // receiver A alone.  The reference is now AudioEngine's second
        // MasterMixer, summing every audible slice, which is what Thetis
        // does: every sub-receiver is pushed into the transmitter's
        // anti-VOX mixer (cmaster.c:371-372 [v2.10.3.15]).
        //
        // **Qt::DirectConnection is load-bearing, not a preference.**
        // antiVoxBlockReady hands out a pointer into thread_local scratch
        // inside AudioEngine::rxBlockReady that the next audio period
        // overwrites.  Direct dispatch runs the slot synchronously on the
        // DSP thread while that pointer is still live; onAntiVoxBlockReady
        // copies there and does its own owned, queued hop onto the TX
        // worker's thread.  Do not "simplify" this to a queued connect:
        // Qt cannot marshal `const float*`, so it would fail at connect
        // time and silently stop feeding the detector.
        connect(m_audioEngine, &AudioEngine::antiVoxBlockReady,
                m_txWorker.get(), &TxWorkerThread::onAntiVoxBlockReady,
                Qt::DirectConnection);

        // 3M-3a-iv: RxDspWorker::bufferSizesChanged → TxWorkerThread::setAntiVoxBlockGeometry.
        //
        // Aligns DEXP's antivox_size / antivox_rate with the post-
        // decimation RX block geometry.  From Thetis cmaster.c:154-155
        // [v2.10.3.13]: audio_outsize / audio_outrate are the canonical
        // anti-VOX detector dimensions, not TX in_size / in_rate.
        connect(m_dspWorker, &RxDspWorker::bufferSizesChanged,
                m_txWorker.get(), &TxWorkerThread::setAntiVoxBlockGeometry,
                Qt::QueuedConnection);

        // 3M-3a-iv: initial push of geometry so DEXP antivox_size /
        // antivox_rate are aligned with the RX block produced by the
        // setBufferSizes() call earlier in this method (line ~2946),
        // whose emission predated the connect above.  Without this push,
        // m_antiVoxSize stays 0 and every sendAntiVoxData rejects on the
        // size-mismatch guard, defeating the cancellation feed.  Both
        // m_dspWorker and m_txWorker live on the main thread at this
        // point (moveToThread happens later for m_dspWorker, and
        // m_txWorker the QObject stays on main thread — only m_txChannel
        // is moveToThread'd into m_txWorker).  Direct call is safe.
        m_txWorker->setAntiVoxBlockGeometry(m_dspWorker->outSize(),
                                            m_dspWorker->sampleRate());

        // 3M-3a-iv: TransmitModel::antiVoxTauMsChanged → MoxController::setAntiVoxTau.
        //
        // Both objects live on main thread; direct connection.
        // Mirrors the existing antiVoxGainDbChanged → setAntiVoxGain pattern.
        connect(&m_transmitModel, &TransmitModel::antiVoxTauMsChanged,
                m_moxController,  &MoxController::setAntiVoxTau);

        // 3M-3a-iv: MoxController::antiVoxDetectorTauRequested → TxWorkerThread::setAntiVoxDetectorTau.
        //
        // MoxController emits seconds (post ms/1000.0 conversion);
        // TxWorkerThread queued slot pass-through to
        // TxChannel::setAntiVoxDetectorTau.
        //
        // From Thetis setup.cs:18992-18996 [v2.10.3.13].
        connect(m_moxController, &MoxController::antiVoxDetectorTauRequested,
                m_txWorker.get(), &TxWorkerThread::setAntiVoxDetectorTau,
                Qt::QueuedConnection);

        // 3M-3a-iv: initial push of TM tau into MoxController so the first
        // emission of antiVoxDetectorTauRequested aligns DEXP with whatever
        // AppSettings restored.  The NaN sentinel inside MoxController
        // forces the emit even if the value matches its default.
        m_moxController->setAntiVoxTau(m_transmitModel.antiVoxTauMs());

        // 3M-3a-iv scope-expansion: TransmitModel::antiVoxRunChanged ->
        // MoxController::setAntiVoxRun.
        //
        // Independent run flag wired to chkAntiVoxEnable in DexpVoxPage.
        // Mirrors the existing antiVoxGainDbChanged -> setAntiVoxGain pattern.
        // Both objects on main thread; direct connection.
        connect(&m_transmitModel, &TransmitModel::antiVoxRunChanged,
                m_moxController,  &MoxController::setAntiVoxRun);

        // 3M-3a-iv scope-expansion: MoxController::antiVoxRunRequested ->
        // TxWorkerThread::setAntiVoxRun.
        //
        // TxWorkerThread::setAntiVoxRun forwards to TxChannel::setAntiVoxRun
        // AND flips the m_antiVoxRun atomic gate that onAntiVoxSamplesReady
        // checks.  From Thetis cmaster.SetAntiVOXRun call at
        // setup.cs:18983 [v2.10.3.13].
        connect(m_moxController, &MoxController::antiVoxRunRequested,
                m_txWorker.get(), &TxWorkerThread::setAntiVoxRun,
                Qt::QueuedConnection);

        // 3M-3a-iv scope-expansion: initial push of TM antiVoxRun into
        // MoxController so the first emission of antiVoxRunRequested aligns
        // TxChannel/atomic gate with whatever AppSettings restored.  The
        // init guard inside MoxController forces the emit even if value
        // matches default.
        m_moxController->setAntiVoxRun(m_transmitModel.antiVoxRun());
    }
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
    // Phase 4 Agent 4A of issue #167 — scaleFwdPowerWatts lifted from this
    // file's anonymous namespace into the public PaTelemetryScaling API
    // (Phase 1B).  Same Thetis-canonical math, same per-board triplet
    // table; reusing the public symbol keeps the future PaValuesPage Raw
    // FWD watts label and this telemetry handler in lockstep.  Remaining
    // private helpers (scaleRevPowerWatts / scalePaVolts / scalePaAmps /
    // scalePaTemperatureCelsius) stay file-scope until they get their
    // own public surface.
    const double fwdW   = NereusSDR::scaleFwdPowerWatts(model, fwdRaw);
    const double revW   = scaleRevPowerWatts(revRaw, model);
    const double paV    = scalePaVolts(userAdc0Raw, model);
    const double paA    = scalePaAmps(userAdc1Raw, model);
    const double paTemp = scalePaTemperatureCelsius(0, model);

    // HL2 firmware overloads the C&C status frame's exciter_power AIN5
    // field to carry the FPGA on-die temperature ADC reading; the value
    // we just stored in `exciterRaw` is therefore not exciter mW on
    // HL2.  Mirror mi0bot's 100-sample averaging window before the
    // scale + push to RadioStatus.  The non-HL2 branch below keeps
    // setExciterPowerMw(exciterRaw) as before; we only divert HL2.
    //
    // From mi0bot console.cs:24937-24941 [v2.10.3.13-beta2 @c26a8a4]:
    //   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)       // MI0BOT: HL2 temperature & current
    //   {
    //       _ampsQueue.Enqueue(NetworkIO.getUserADC0());
    //       _tempQueue.Enqueue(NetworkIO.getExciterPower());
    //   }
    // and console.cs:25073-25079:
    //   float tempAverage = _tempQueue.Count > 0 ? (float)_tempQueue.Average() : 0;     // MI0BOT: HL2 temperature
    //   ...
    //   // MI0BOT: temp for HL2
    //   _MKIIHL2Temp = (3.26f * (tempAverage / 4096.0f) - 0.5f) / 0.01f;
    double hl2TempC = 0.0;
    bool   hl2TempValid = false;
    if (model == HPSDRModel::HERMESLITE) {
        m_hl2TempRing[static_cast<std::size_t>(m_hl2TempHead)] = exciterRaw;
        m_hl2TempHead = (m_hl2TempHead + 1) %
                        static_cast<int>(m_hl2TempRing.size());
        if (m_hl2TempCount < static_cast<int>(m_hl2TempRing.size())) {
            ++m_hl2TempCount;
        }
        quint64 sum = 0;
        for (int i = 0; i < m_hl2TempCount; ++i) {
            sum += m_hl2TempRing[static_cast<std::size_t>(i)];
        }
        const double avgRaw = static_cast<double>(sum) /
                              static_cast<double>(m_hl2TempCount);
        const auto avgQuantised =
            static_cast<quint16>(qBound(0.0, qRound(avgRaw) + 0.0, 65535.0));
        hl2TempC = NereusSDR::scaleHermesLiteTempCelsius(avgQuantised);
        hl2TempValid = true;
    }
    Q_UNUSED(paV);       // RadioStatus does not expose PA volts directly (per its design header)
    Q_UNUSED(supplyRaw); // supply_volts surfaced via RadioConnection::supplyVoltsChanged signal (sub-PR-2 B.3)

    // From Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13] —
    // route raw alex_fwd through the per-board cal table before publishing
    // to RadioStatus.  Identity transform when no profile is loaded
    // (boardClass == None, see CalibrationController::calibratedFwdPowerWatts).
    // Reflected-power path is unchanged: Thetis's CalibratedPAPower is FWD-only.
    const double fwdWCal = double(
        m_calController.calibratedFwdPowerWatts(static_cast<float>(fwdW)));

    // Bench-reported #167 follow-up: when not transmitting, the radio still
    // emits P2 high-priority status frames containing residue alex_fwd /
    // alex_rev values (last sample echo + directional-coupler noise floor).
    // Pushing those non-zero residue values to RadioStatus re-fills the
    // Power / SWR bars after the falling-edge handler tried to zero them.
    // Force the TX-domain readings to 0 when not transmitting so the
    // meters show the physical truth (no TX → no forward power).
    //
    // Predicate: MoxController::state() == MoxState::Tx — the authoritative
    // wire-level TX-active state.  TransmitModel's m_mox / m_tune flags are
    // orphan state in the current codebase (never set true by any code
    // path), so consulting them returned false during TUNE and force-zeroed
    // the meters mid-transmit.  MoxController is the single source of truth
    // for whether the radio is actually transmitting RF.
    //
    // PA current / temperature / supply voltage are slow physical
    // quantities valid off-air; leave those samples alone.
    // Test-seam override: handlePaTelemetryForTest sets m_forceTxForTest
    // to simulate a transmit sample without driving the full MoxController
    // state machine.  Production code paths leave the flag false.
    const bool inTx = m_forceTxForTest
                       || (m_moxController
                            && m_moxController->state() == MoxState::Tx);
    m_radioStatus.setForwardPower(inTx ? fwdWCal : 0.0);
    m_radioStatus.setReflectedPower(inTx ? revW : 0.0);
    // HL2 reuses the exciter_power C&C bytes for the FPGA temperature
    // ADC, so the same wire bytes mean different things across the
    // family.  Suppress setExciterPowerMw on HL2 so PaValuesPage /
    // RadioStatusPage don't show "exciter = 942 mW" when 942 is the
    // raw temp ADC count.  Other boards keep the existing semantic.
    if (model != HPSDRModel::HERMESLITE) {
        // From Thetis console.cs:26001-26013 [v2.10.3.15] — per-model exciter scaling.
        // ANAN_G2E and OrionMKII family use computeOrionMkIIExciterPower(); others use
        // computeExciterPower(). Logic lives in PaTelemetryScaling::scaleExciterPowerMw().
        // Inline tags preserved verbatim from upstream:
        //   console.cs:26004  case HPSDRModel.ANAN_G2E: //N1GP G2E added
        //   console.cs:26010  case HPSDRModel.REDPITAYA: //DH1KLM
        m_radioStatus.setExciterPowerMw(
            inTx ? static_cast<int>(scaleExciterPowerMw(model, exciterRaw)) : 0);
    } else if (!inTx) {
        m_radioStatus.setExciterPowerMw(0);
    }
    m_radioStatus.setPaCurrent(paA);
    // Only push temp when we have a real source (non-zero); leaves the
    // last-known value alone otherwise so a stale 0 doesn't overwrite a
    // good HL2 reading from another path.
    if (paTemp > 0.0) {
        m_radioStatus.setPaTemperature(paTemp);
    }
    if (hl2TempValid) {
        m_radioStatus.setPaTemperature(hl2TempC);
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

// Issue #182 — TransmitModel::micPttDisabled → RadioConnection wire bit.
//
// Mirrors the persisted user preference onto the radio firmware (the Setup
// -> Audio -> TX Input -> "Mic PTT Disabled" checkbox), and primes the
// connection with the current model value once so a fresh connect honours
// whatever the user last saved.  Tests reach this helper through the
// wireMicPttDisabledForTest() seam to avoid the full wireConnectionSignals
// DSP-thread pipeline.
//
// Source: Thetis console.cs:19761-19764 [v2.10.3.13+501e3f51]:
//   set {
//       mic_ptt_disabled = value;
//       NetworkIO.SetMicPTT(Convert.ToInt32(value));
//   }
// The MicPTTDisabled property setter pushes to NetworkIO unconditionally on
// every UI flip; this helper does the equivalent through the Qt signal/slot
// system (queued because the connection lives on its own worker thread).
void RadioModel::connectMicPttDisabledSignal()
{
    if (!m_connection) {
        return;
    }
    QObject::connect(&m_transmitModel, &TransmitModel::micPttDisabledChanged,
                     m_connection, &RadioConnection::setMicPTTDisabled,
                     Qt::QueuedConnection);
    // Prime: push the current model value so the wire bit reflects the user
    // preference even before the first toggle.  Queued so production callers
    // on the main thread don't synchronously block on the connection thread.
    QMetaObject::invokeMethod(m_connection, [conn = m_connection,
                                             d = m_transmitModel.micPttDisabled()]() {
        conn->setMicPTTDisabled(d);
    }, Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------
// txBoundSlice — the slice the transmitter is bound to.
//
// The transmit frequency must come from here and nowhere else. activeSlice()
// is the slice the operator is looking at, which in multi-slice is routinely
// a different slice; sourcing the transmit frequency from it means turning
// the knob on a receive-only slice retunes the transmitter, and the Alex TX
// low-pass follows it onto the wrong band.
//
// Thetis makes the same split. Its VFO A arm stands down when VFO B holds
// the transmitter:
//   From Thetis console.cs:31889-31893 [v2.10.3.15]
//     if (!chkFullDuplex.Checked && !chkVFOBTX.Checked)
//     { tx_dds_freq_mhz = tx_freq; UpdateTXDDSFreq(); }
//   From Thetis console.cs:32866-32869 [v2.10.3.15]
// Upstream inline attribution preserved verbatim (console.cs:31897):
//   if (_click_tune_display) //-W2PA This was preventing proper receiver adjustment
//     if (!rx1_sub_drag) { tx_dds_freq_mhz = tx_freq; UpdateTXDDSFreq(); }
// ---------------------------------------------------------------------------
SliceModel* RadioModel::txBoundSlice() const
{
    if (!m_txSliceArbiter) {
        return nullptr;
    }
    return sliceById(m_txSliceArbiter->txBoundSliceId());
}

void RadioModel::installBandPlanMoxCheck()
{
    if (!m_moxController) {
        return;
    }

    m_moxController->setMoxCheck([this]() -> safety::BandPlanGuard::MoxCheckResult {
        const int regionInt = AppSettings::instance()
            .value(QStringLiteral("BandPlanRegion"),
                   QString::number(static_cast<int>(safety::Region::UnitedStates)))
            .toInt();
        const auto region = static_cast<safety::Region>(regionInt);

        const SliceModel* slice = txBoundSlice();
        if (!slice) {
            return {false, QStringLiteral("No TX-bound slice")};
        }

        const auto freqHz = static_cast<std::int64_t>(slice->frequency());
        const DSPMode mode = slice->dspMode();
        const Band txBand = bandFromFrequency(slice->frequency());

        return m_bandPlan.checkMoxAllowed(region, freqHz, mode,
                                          txBand, txBand,
                                          /*preventDifferentBand=*/false,
                                          /*extended=*/false);
    });
}

// ---------------------------------------------------------------------------
// pushTxFrequencyFromTxSlice — recompute and publish the transmit frequency.
//
// Mirrors Thetis UpdateTXDDSFreq (console.cs:15464-15485 [v2.10.3.15]), which
// recomputes from tx_dds_freq_mhz and drives the Alex TX low-pass and the TX
// NCO from the same value in the same call:
//   setAlexLPF(tx_dds_freq_mhz, true);
//   ...
//   NetworkIO.VFOfreq(0, tx_dds_freq_mhz, 1);
//
// XIT is folded in, RIT is not:
//   From Thetis console.cs:31782-31784 [v2.10.3.15]
//     if (chkRIT.Checked && bRitOk) rx_freq += (int)udRIT.Value * 0.000001;
//     if (chkXIT.Checked)           tx_freq += (int)udXIT.Value * 0.000001;
// ---------------------------------------------------------------------------
void RadioModel::pushTxFrequencyFromTxSlice()
{
    if (!m_connection) { return; }

    SliceModel* slice = txBoundSlice();
    if (!slice) { return; }

    const qint64 xitOffset =
        slice->xitEnabled() ? static_cast<qint64>(slice->xitHz()) : 0LL;
    const qint64 txHz = static_cast<qint64>(slice->frequency()) + xitOffset;
    const quint64 txFreqHz = (txHz < 0) ? 0 : static_cast<quint64>(txHz);

    QMetaObject::invokeMethod(m_connection, [conn = m_connection, txFreqHz]() {
        conn->setTxFrequency(txFreqHz);
    });
}

// Wire active slice signals to WDSP channel and radio hardware.
// Called from wireConnectionSignals after connection is established.
void RadioModel::wireSliceSignals(SliceModel* slice)
{
    // Every slice, not just the active one.
    //
    // This used to open `SliceModel* slice = m_activeSlice;` and run once,
    // so the 65 per-slice DSP handlers below existed for Slice A alone --
    // AGC, filter, mode, NB, SNB, APF, RIT/XIT, squelch, mute, pan and the
    // whole NR1/NR2/NR3/NR4/MNR/DFNR parameter set were simply unwired on
    // any other slice. Each also wrote rxChannel(slice->sliceIndex()), so even where a
    // handler did fire it moved Slice A's DSP.
    if (!slice || !m_connection) {
        return;
    }

    // Phase 3F Sub-Epic I: the frequency → ReceiverManager → radio hardware
    // push that used to open this lambda has moved into bindSliceToStream,
    // which addSlice wires for EVERY slice. This one ran only for
    // m_activeSlice and only from connect time, which is why tuning Slice B
    // or later never reached the radio.
    //
    // What stays here is genuinely active-slice-only: the operator's
    // listening frequency (FreeDV Reporter), the simplex TX-follows-RX
    // push, band tracking, and the settings save.
    connect(slice, &SliceModel::frequencyChanged, this, [this, slice](double freq) {
        // Phase 3R K-bench: push the new freq to the FreeDV Reporter so
        // our station's listed freq tracks the VFO. Without this, the
        // reporter server has only the connect-time freq (or zero) and
        // we never appear on-band to other operators. Mirrors freedv-
        // gui's freqChangeImpl_ trigger pattern.
        //
        // 2026-05-12 bench: route through the dwell throttle so a VFO
        // spin doesn't DoS qso.freedv.org with one packet per wheel
        // tick.  7 s trailing dwell + 100 kHz band-jump fast-path; see
        // publishFreedvFrequencyDwelled() body for the full policy.
        if (m_freeDvReporter && m_freeDvReporter->isConnected()) {
            publishFreedvFrequencyDwelled(static_cast<quint64>(freq));
        }
        // The TX frequency fan-out lives in addSlice(), where it is wired
        // once for every slice regardless of connection lifecycle. Keeping
        // a second copy here used to publish each bound-slice retune twice.
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
            //
            // Issue #257: pass the SkuUiProfile so the new band's RX-only
            // selection (if any) gets the right SKU-specific label.
            // The slice that CHANGED band, not the active one. This handler
            // is per slice now, so refreshing m_activeSlice here would move
            // Slice A's antenna selection when Slice B crossed a band edge.
            {
                const SkuUiProfile sku = skuUiProfileFor(m_hardwareProfile.model);
                slice->refreshAntennasFromAlex(m_alexController, newBand, &sku);
            }
        }
        scheduleSettingsSave();
    });

    // Mode → WDSP
    // setMode: push the demodulation mode to WDSP immediately.
    // onModeChanged (Task 4.2): read per-mode DSP-Options AppSettings (buffer/
    // filter/filter-type) and rebuild the WDSP channel if any setting changed.
    // dspChangeMeasured is emitted with elapsed ms when a rebuild occurs.
    connect(slice, &SliceModel::dspModeChanged, this, [this, slice](DSPMode mode) {
        // Phase 3J-1 closeout follow-up (2026-05-12): re-evaluate FreeDV
        // Reporter visibility on every mode change.  Show our station on
        // the dashboard only when we're in RADE_U / RADE_L.
        updateFreedvReporterVisibility();

        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setMode(mode);
            const qint64 elapsed = rxCh->onModeChanged(mode);
            // 0 = no change / no engine; -1 = rebuild attempted but
            // channel not in engine map; > 0 = rebuild ran in N ms.
            // Only emit dspChangeMeasured when an actual rebuild
            // happened (elapsed > 0).  In-place WDSP setters that
            // finish sub-millisecond will report 1 ms via QElapsedTimer
            // since the surrounding setter calls take real time.
            if (elapsed > 0) {
                emit dspChangeMeasured(elapsed);
            }
        }
        // TX channel: live-apply per-mode filter size + filter type via the
        // in-place WDSP entry points (TXASetNC / TXASetMP — radio.cs:2628 /
        // 2647 [v2.10.3.13]).  Each setter internally quiesces the channel
        // via SetChannelState's flushflag handshake (channel.c:259-297
        // [v2.10.3.13]) — safe to call from the main thread while
        // TxWorkerThread is alive.
        //
        // The earlier 2026-05-05 hot-fix that disabled this call was
        // working around a different bug: TxChannel::onModeChanged called
        // WdspEngine::rebuildTxChannel() (close-and-reopen) which raced
        // with the running worker and SIGSEGV'd on band change.
        // commits 1ed5464/1b4ba06/fd5c807 swapped the rebuild path for
        // the in-place setters, so the live-apply is safe to restore.
        if (slice == txBoundSlice()) {
            if (m_txChannel) {
                const qint64 txElapsed = m_txChannel->onModeChanged(mode);
                // Same return-code convention as the RX path above.
                if (txElapsed > 0) {
                    emit dspChangeMeasured(txElapsed);
                }
            }

            // Issue #153 sub-bug 2 — TX-side mode + bandpass push (trigger
            // #2 of 3). A listening slice still updates its own RX WDSP
            // state above, but it has no authority over this global chain.
            pushTxModeAndBandpass();

            // 2026-05-12 bench fix (PR #238): snap TX BW to the RADE modem
            // audio passband only when the transmitter's mode changes.
            if (mode == DSPMode::RADE_U || mode == DSPMode::RADE_L) {
                m_transmitModel.setFilterLow(650);
                m_transmitModel.setFilterHigh(2350);
            } else if (m_transmitModel.filterLow() == 650
                       && m_transmitModel.filterHigh() == 2350) {
                // Leaving RADE: preserve custom voice bandwidths.
                m_transmitModel.setFilterLow(100);
                m_transmitModel.setFilterHigh(3900);
            }
        }

        scheduleSettingsSave();
    });

    // Filter → WDSP
    connect(slice, &SliceModel::filterChanged, this, [this, slice](int low, int high) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setFilterFreqs(low, high);
        }
        scheduleSettingsSave();
    });

    // AGC → WDSP
    connect(slice, &SliceModel::agcModeChanged, this, [this, slice](AGCMode mode) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
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
    connect(slice, &SliceModel::agcThresholdChanged, this, [this, slice](int dBu) {
        if (m_syncingAgc) { return; }

        // From Thetis v2.10.3.13 console.cs:49129-49130 — manual drag disables auto
        if (slice->autoAgcEnabled()) {
            slice->setAutoAgcEnabled(false);
        }

        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            m_syncingAgc = true;
            rxCh->setAgcThreshold(dBu);
            // Read back resulting AGC Top and sync RF Gain display.
            // From Thetis console.cs:45978 — GetRXAAGCTop after SetRXAAGCThresh
            double top = rxCh->readBackAgcTop();
            int rfGain = static_cast<int>(std::round(top));
            if (slice->rfGain() != rfGain) {
                slice->setRfGain(rfGain);
            }
            m_syncingAgc = false;
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcHangChanged, this, [this, slice](int ms) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAgcHang(ms);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcSlopeChanged, this, [this, slice](int slope) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAgcSlope(slope);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcAttackChanged, this, [this, slice](int ms) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAgcAttack(ms);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::agcDecayChanged, this, [this, slice](int ms) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAgcDecay(ms);
        }
        scheduleSettingsSave();
    });

    // From Thetis v2.10.3.13 setup.cs:9081 — hang threshold
    connect(slice, &SliceModel::agcHangThresholdChanged, this, [this, slice](int val) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAgcHangThreshold(val);
        }
        scheduleSettingsSave();
    });

    // From Thetis v2.10.3.13 setup.cs:9001 — fixed gain
    connect(slice, &SliceModel::agcFixedGainChanged, this, [this, slice](int dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAgcFixedGain(dB);
        }
        scheduleSettingsSave();
    });

    // From Thetis v2.10.3.13 setup.cs:9011 — max gain
    connect(slice, &SliceModel::agcMaxGainChanged, this, [this, slice](int dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAgcMaxGain(dB);
        }
        scheduleSettingsSave();
    });

    // ── Auto AGC-T timer ────────────────────────────────────────────────
    // From Thetis v2.10.3.13 console.cs:46057 — tmrAutoAGC_Tick, 500ms interval
    // One timer for the model, NOT one per slice. This function now runs for
    // every slice, so an unguarded `new QTimer` here would build and connect a
    // fresh timer per slice and fire the auto-AGC tick N times a period. The
    // tick itself is deliberately active-slice-only (it reads m_activeSlice
    // below), which is why it stays a singleton rather than being
    // parameterised like the per-slice handlers above.
    if (!m_autoAgcTimer) {
    m_autoAgcTimer = new QTimer(this);
    m_autoAgcTimer->setInterval(500);
    connect(m_autoAgcTimer, &QTimer::timeout, this, [this]() {
        // From Thetis v2.10.3.13 console.cs:46059 — guard: skip if not connected or MOX
        if (!m_connection || !m_connection->isConnected()) {
            return;
        }
        // From Thetis v2.10.3.13 console.cs:46059 — if (!chkPower.Checked || _mox) return;
        if (m_transmitModel.isMox()) {
            return;
        }

        // EVERY slice with auto-AGC on, not just the active one. The tick used
        // to open `SliceModel* slice = m_activeSlice;`, so arming AUTO on a
        // flag other than the active slice's did nothing at all -- bench-caught
        // 2026-07-26 as auto AGC not working on flags B-D.
        for (SliceModel* slice : std::as_const(m_slices)) {
        if (!slice || !slice->autoAgcEnabled()) {
            continue;
        }

        // This slice's OWN stream noise floor. A shared tracker (stream 0's)
        // would set a 20m slice's threshold from 40m's noise floor.
        NoiseFloorTracker* nfTracker = noiseFloorTrackerForSlice(slice);
        if (!nfTracker || !nfTracker->isGood()) {
            continue;
        }

        // From Thetis v2.10.3.13 console.cs:46107-46115
        const double noiseFloor = static_cast<double>(nfTracker->noiseFloor());

        // From Thetis v2.10.3.13 console.cs:33292-33319 — agcCalOffset(rx)
        // Full Thetis formula:
        //   FIXD:    0.0
        //   default: 2.0 + (DisplayCalOffset + PreampOffset - AlexPreampOffset
        //                    - FFTSizeOffset)
        //
        // FFTSizeOffset (Display.cs:1389-1397 [v2.10.3.13]) is set to
        // slider.Value * 2 dB on every FFT slider scroll (setup.cs:16154).
        // Without subtracting it, the AGC threshold drifts up to 12 dB
        // across the slider's 0..6 range (each step adds 2 dB to the
        // visible noise floor as bin width halves).
        //
        // PreampOffset / AlexPreampOffset still TBD (separate scope: lands
        // with the spectrum knee-line overlay work).  They sum to ~0 on
        // most current radios so the AGC drift was negligible until the
        // FFT slider made FFTSizeOffset user-tunable.
        float calOffset = 0.0f;
        if (slice->agcMode() != AGCMode::Off) {
            const double fftOffsetDb = m_fftEngine
                ? m_fftEngine->fftSizeOffsetDb() : 0.0;
            calOffset = 2.0f - static_cast<float>(fftOffsetDb);
        }

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
            RxChannel* rxCh = m_wdspEngine ? m_wdspEngine->rxChannel(slice->sliceIndex()) : nullptr;
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
        }  // for each slice with auto-AGC
    });
    m_autoAgcTimer->start();
    }

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
    connect(slice, &SliceModel::nr1TapsChanged, this, [this, slice](int v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setAnrTaps(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr1DelayChanged, this, [this, slice](int v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setAnrDelay(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr1GainChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setAnrGain(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr1LeakageChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setAnrLeakage(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr1PositionChanged, this, [this, slice](NereusSDR::NrPosition p) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setAnrPosition(p); }
        scheduleSettingsSave();
    });

    // NR2 (EMNR) — gain-method + npe-method + AE filter + position + Post2 cascade
    // From Thetis setup.cs NR2 group [v2.10.3.13]
    connect(slice, &SliceModel::nr2GainMethodChanged, this, [this, slice](NereusSDR::EmnrGainMethod v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setEmnrGainMethod(static_cast<int>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2NpeMethodChanged, this, [this, slice](NereusSDR::EmnrNpeMethod v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setEmnrNpeMethod(static_cast<int>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2TrainT1Changed, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setEmnrTrainT1(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2TrainT2Changed, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setEmnrTrainT2(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2AeFilterChanged, this, [this, slice](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setEmnrAeRun(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2PositionChanged, this, [this, slice](NereusSDR::NrPosition p) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setEmnrPosition(static_cast<int>(p)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2Post2RunChanged, this, [this, slice](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setEmnrPost2Run(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2Post2LevelChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setEmnrPost2Level(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2Post2FactorChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setEmnrPost2Factor(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2Post2RateChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setEmnrPost2Rate(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr2Post2TaperChanged, this, [this, slice](int v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setEmnrPost2Taper(v); }
        scheduleSettingsSave();
    });

    // NR3 (RNNR) — position + useDefaultGain
    // From Thetis setup.cs:35460-35462 [v2.10.3.13]
    connect(slice, &SliceModel::nr3PositionChanged, this, [this, slice](NereusSDR::NrPosition p) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setRnnrPosition(p); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr3UseDefaultGainChanged, this, [this, slice](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setRnnrUseDefaultGain(v); }
        scheduleSettingsSave();
    });

    // NR4 (SBNR) — 5 spinboxes + algo
    // From Thetis setup.cs:34511-34527 [v2.10.3.13]
    connect(slice, &SliceModel::nr4ReductionChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setSbnrReductionAmount(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr4SmoothingChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setSbnrSmoothingFactor(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr4WhiteningChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setSbnrWhiteningFactor(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr4RescaleChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setSbnrNoiseRescale(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr4PostThreshChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setSbnrPostFilterThreshold(v); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nr4AlgoChanged, this, [this, slice](NereusSDR::SbnrAlgo a) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setSbnrAlgo(a); }
        scheduleSettingsSave();
    });

#ifdef HAVE_DFNR
    // DFNR — AttenLimit + PostFilterBeta
    // double→float cast at the boundary (SliceModel stores double for QSpinBox compat)
    connect(slice, &SliceModel::dfnrAttenLimitChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setDfnrAttenLimit(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::dfnrPostFilterBetaChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setDfnrPostFilterBeta(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
#endif

#ifdef HAVE_MNR
    // MNR — SliceModel mnrStrength already in 0.0–1.0 (the Setup/popup
    // slider applies the ×100 / ÷100 UI↔model conversion on both sides).
    connect(slice, &SliceModel::mnrStrengthChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setMnrStrength(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::mnrOversubChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setMnrOversub(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::mnrFloorChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setMnrFloor(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::mnrAlphaChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setMnrAlpha(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::mnrBiasChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) { rxCh->setMnrBias(static_cast<float>(v)); }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::mnrGsmoothChanged, this, [this, slice](double v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
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
    connect(slice, &SliceModel::activeNrChanged, this, [this, slice](NereusSDR::NrSlot slot) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setActiveNr(slot);
        }
        scheduleSettingsSave();
    });

    // ANF is per-slice: it lives in RXA, one instance per WDSP channel.
    // Same shape as activeNrChanged above.
    connect(slice, &SliceModel::anfEnabledChanged, this, [this, slice](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAnfEnabled(on);
        }
        scheduleSettingsSave();
    });

    // SNB → WDSP
    // From Thetis Project Files/Source/Console/console.cs:36347
    //   WDSP.SetRXASNBARun(WDSP.id(0, 0), chkDSPNB2.Checked)
    // WDSP: third_party/wdsp/src/snb.c:579
    connect(slice, &SliceModel::snbEnabledChanged, this, [this, slice](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setSnbEnabled(on);
        }
        scheduleSettingsSave();
    });

    // NB mode (NB1 / NB2 / Off) → WDSP
    // From Thetis Project Files/Source/Console/console.cs — chkDSPNB1/chkDSPNB2 Checked
    // WDSP: third_party/wdsp/src/anb.c (SetRXAANBRun) + third_party/wdsp/src/nob.c (SetRXANOBRun)
    //
    // Phase 3F Sub-Epic J Task 6: the co-hosted-slice state mirror for this
    // property is wired in addSlice, not here. This connect (like the rest
    // of wireSliceSignals) only exists once m_connection is non-null, but
    // the mirror is a client-side model contract between SliceModels that
    // has to hold whether or not a radio is attached -- the same reasoning
    // that put the frequencyChanged rollback handler in addSlice instead of
    // here. See the connect beside it there for the mirror itself.
    connect(slice, &SliceModel::nbModeChanged, this, [this, slice](NereusSDR::NbMode m) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setNbMode(m);
        }
        scheduleSettingsSave();
    });

    // ── NB1 / NB2 / SNB detailed tuning → WDSP ──────────────────────────────
    // Restored as per-slice pushes by the Sub-Epic J follow-up. The wiring was
    // removed 2026-04-22 in favour of Setup → DSP → NB/SNB calling
    // SetEXTANB* / SetEXTNOB* / SetRXASNBA* directly, but every one of those
    // calls passed a hardcoded channel 0, so eight tuning controls acted on
    // receiver A no matter which receiver was selected. That bypass was
    // invisible to Sub-Epic J's rxChannel() audit because it reached WDSP by
    // another route entirely.
    //
    // The five NB1 / NB2 knobs are additionally mirrored across co-hosted
    // slices (see the mirror in addSlice); this push is what puts the agreed
    // value on each slice's own channel. SNB is per channel and unmirrored.
    connect(slice, &SliceModel::nb1ThresholdChanged, this, [this, slice](int v) {
        if (RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            // From Thetis setup.cs:8606 [v2.10.3.15]
            //   console.radio.GetDSPRX(0, 0).NBThreshold = 0.165 * (double)(udDSPNB.Value);
            // The UI value is scaled into the WDSP domain by 0.165 before it
            // reaches SetEXTANBThreshold.
            rxCh->setNbThreshold(0.165 * static_cast<double>(v));
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nb1TransitionMsChanged, this, [this, slice](double v) {
        if (RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            rxCh->setNbTransitionMs(v);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nb1LeadMsChanged, this, [this, slice](double v) {
        if (RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            rxCh->setNbLeadMs(v);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nb1LagMsChanged, this, [this, slice](double v) {
        if (RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            rxCh->setNbLagMs(v);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::nb2ModeChanged, this, [this, slice](int v) {
        if (RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            rxCh->setNb2Mode(v);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::snbK1Changed, this, [this, slice](double v) {
        if (RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            rxCh->setSnbK1(v);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::snbK2Changed, this, [this, slice](double v) {
        if (RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            rxCh->setSnbK2(v);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::snbOutputBandwidthHzChanged, this, [this, slice](int v) {
        if (RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            rxCh->setSnbOutputBandwidthHz(v);
        }
        scheduleSettingsSave();
    });

    // APF → WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1910-1927
    //   WDSP.SetRXASPCWRun(WDSP.id(thread, subrx), value)
    // WDSP: third_party/wdsp/src/apfshadow.c:93
    connect(slice, &SliceModel::apfEnabledChanged, this, [this, slice](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
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
    connect(slice, &SliceModel::apfTuneHzChanged, this, [this, slice](int hz) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
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
    connect(slice, &SliceModel::ssqlEnabledChanged, this, [this, slice](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setSsqlEnabled(on);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::ssqlThreshChanged, this, [this, slice](double threshold) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
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
    connect(slice, &SliceModel::amsqEnabledChanged, this, [this, slice](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAmsqEnabled(on);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::amsqThreshChanged, this, [this, slice](double dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAmsqThresh(dB);
        }
        scheduleSettingsSave();
    });

    // Squelch — FM → WDSP
    // From Thetis Project Files/Source/Console/radio.cs:1274-1329
    // WDSP: third_party/wdsp/src/fmsq.c:236,244
    // SliceModel stores fmsqThresh in dB; RxChannel::setFmsqThresh converts to linear
    connect(slice, &SliceModel::fmsqEnabledChanged, this, [this, slice](bool on) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setFmsqEnabled(on);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::fmsqThreshChanged, this, [this, slice](double dB) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setFmsqThresh(dB);
        }
        scheduleSettingsSave();
    });

    // Audio panel — mute / pan / binaural → WDSP PatchPanel
    // From Thetis Project Files/Source/Console/radio.cs:1386-1403 (pan)
    // From Thetis Project Files/Source/Console/radio.cs:1145-1162 (binaural)
    // WDSP: third_party/wdsp/src/patchpanel.c:126,159,187
    connect(slice, &SliceModel::mutedChanged, this, [this, slice](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setMuted(v);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::audioPanChanged, this, [this, slice](double pan) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            rxCh->setAudioPan(pan);
        }
        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::binauralEnabledChanged, this, [this, slice](bool v) {
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
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
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
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

    // XIT-to-TX fan-out is likewise wired once in addSlice(). This block
    // retains only the RX shift-frequency consumers above.

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

    // AF gain → WDSP RX panel gain1 (SetRXAPanelGain1).
    // From Thetis radio.cs:1077-1107 [v2.10.3.14] RXOutputGain setter:
    //   WDSP.SetRXAPanelGain1(WDSP.id(thread, subrx), value);
    // Per-slice AF runs INSIDE the WDSP audio panel; the post-DSP master
    // scalar (AudioEngine::setVolume) belongs to MasterOutputWidget alone.
    // Earlier wiring routed afGain to AudioEngine::setVolume too, which
    // (a) fought the master slider for the same atomic and (b) left WDSP's
    // panel.gain1 at its rxa.c:538 default of 4.0 (+12 dB), causing the
    // distortion-at-high-volume bug surfaced 2026-05-07.
    //
    // [2.10.3.5]MW0LGE wave recorder volume normalise  [original inline tag
    //   from radio.cs:1091; the wave_file_writer branch is intentionally
    //   not ported here. NereusSDR has no WaveThing recorder module yet,
    //   so there is no RecordGain to mirror. Tag preserved verbatim per
    //   CLAUDE.md inline-comment-preservation rule; restore the branch
    //   when the recorder lands.]
    connect(slice, &SliceModel::afGainChanged, this, [this, slice](int gain) {
        if (m_wdspEngine) {
            RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
            if (rxCh) {
                rxCh->setAfGain(gain / 100.0);
            }
        }
        scheduleSettingsSave();
    });

    // RF gain → WDSP AGC top, with bidirectional sync back to AGC-T.
    // From Thetis console.cs:50350 pattern — GetRXAAGCThresh after SetRXAAGCTop
    // Upstream inline attribution preserved verbatim (console.cs:50345):
    //   if (agc_thresh_point < -160.0) agc_thresh_point = -160.0; //[2.10.3.6]MW0LGE changed from -143
    connect(slice, &SliceModel::rfGainChanged, this, [this, slice](int gain) {
        if (m_syncingAgc) { return; }
        RxChannel* rxCh = m_wdspEngine->rxChannel(slice->sliceIndex());
        if (rxCh) {
            m_syncingAgc = true;
            rxCh->setAgcTop(static_cast<double>(gain));
            // Read back resulting threshold and sync AGC-T display.
            double thresh = rxCh->readBackAgcThresh();
            int threshInt = static_cast<int>(std::round(thresh));
            if (slice->agcThreshold() != threshInt) {
                slice->setAgcThreshold(threshInt);
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
    connect(slice, &SliceModel::rxAntennaChanged, this,
            [this, slice](const QString& ant) {
        // ANT1/2/3 → setRxAnt (direct hardware port). Non-ANT/non-bypass
        // labels (EXT1, EXT2, XVTR, RX1, RX2, BYPS…) → setRxOnlyAnt with
        // the 1-based position in SkuUiProfile::rxOnlyLabels, mirroring the
        // routing used by RxApplet's popup handler (RxApplet.cpp:279-293).
        // "RX out on TX" is a bypass toggle handled separately, not here.
        // Fixes SpectrumOverlayPanel antenna combo silently no-op'ing for
        // non-ANT selections (B3 fix-up).
        // Source: same routing as RxApplet popup handler (RxApplet.cpp:279-293).
        //
        // Issue #257: the antenna popup is a single mutually-exclusive
        // selection, so picking ANT1/2/3 must also clear the rx-only mux
        // (rxOnlyAnt → 0). Without this the RX-bypass relay stays engaged
        // from a prior EXT1/BYPS/XVTR pick and the radio never returns to
        // the main RX input. Mirrors Thetis ProcessAlexAntCheckBox
        // (setup.cs:13643-13705 [v2.10.3.13 @501e3f51]) where unchecking
        // every RX-only checkbox sends `setRxOnlyAnt(band, 0)`.
        if (ant.startsWith(QStringLiteral("ANT"))) {
            int antNum = 1;
            if (ant == QLatin1String("ANT2")) { antNum = 2; }
            else if (ant == QLatin1String("ANT3")) { antNum = 3; }
            const Band band = bandFromFrequency(slice->frequency());
            m_alexController.setRxAnt(band, antNum);
            m_alexController.setRxOnlyAnt(band, 0);  // issue #257 — release bypass mux
        } else if (ant != QStringLiteral("RX out on TX")) {
            // RX-only label: find 1-based position in SkuUiProfile::rxOnlyLabels.
            const SkuUiProfile sku = skuUiProfileFor(m_hardwareProfile.model);
            const auto& lbls = sku.rxOnlyLabels;
            for (int i = 0; i < static_cast<int>(lbls.size()); ++i) {
                if (lbls[static_cast<size_t>(i)] == ant) {
                    m_alexController.setRxOnlyAnt(
                        bandFromFrequency(slice->frequency()), i + 1);
                    break;
                }
            }
        }

        // Defect D1: the antenna is a codec input, so the codec has to run
        // again when it moves. SliceConfig::antennaIndex is what
        // P2CodecOrionMkII::applyDdcAssignment turns into the DDC's ADC
        // selector, and nothing here asked for a recompute, so picking EXT1
        // left the DDC on ADC0 until the operator happened to retune, rebind
        // or close a slice -- the three events that already call
        // requestDdcAssignment.
        //
        // Unconditional rather than gated on the antenna having changed
        // chains: this is the same call every VFO tick already makes
        // (frequencyChanged -> bindSliceToStream -> requestDdcAssignment; see
        // the change-gate note in ReceiverManager::setDdcMapping), and it is
        // idempotent, so one more on an antenna click costs nothing and
        // needs no second copy of the codec's antenna-to-ADC policy here to
        // decide whether to skip.
        requestDdcAssignment();

        scheduleSettingsSave();
    });
    connect(slice, &SliceModel::txAntennaChanged, this,
            [this, slice](const QString&) {
        if (slice == txBoundSlice()) {
            applyTxAntennaFromBoundSlice();
        }
        scheduleSettingsSave();
    });

    // Phase 3F Sub-Epic G Task 13: route SliceModel diversity state to
    // WdspEngine's process-wide External Diversity owner.
    //
    // CRITICAL: WDSP pdiv[] is a 2-slot array (MAX_EXT_DIVS=2) keyed by
    // External Diversity id (0 or 1), NOT the RXA channel id. For bench-
    // minimum, route ONLY the stable Slice A id through DivId 0. Slice B and
    // per-pan diversity defer to a follow-up when a proper DivId allocator
    // lands.
    connect(slice, &SliceModel::diversityEnabledChanged, this,
            [this, slice](bool on) {
        if (slice->sliceIndex() != kExternalDiversityTargetSliceId
            || sliceById(kExternalDiversityTargetSliceId) != slice) {
            return;
        }
        if (!on) {
            stopExternalDiversityRoute();
        }
        // Recompute first on enable so source resolution uses the assignment
        // that actually migrated Slice A onto DDC0/DDC1. On disable the route
        // is already clear/stopped/destroyed before the wire state reverts.
        refreshDdcAssignmentForRadioState();
    });
    connect(slice, &SliceModel::diversityPhaseDegChanged, this,
            [this, slice](double /*deg*/) {
        if (!m_externalDiversityRouteActive
            || slice->sliceIndex() != kExternalDiversityTargetSliceId
            || sliceById(kExternalDiversityTargetSliceId) != slice) {
            return;
        }
        configureExternalDiversityRotation(slice);
    });
    connect(slice, &SliceModel::diversityGainDbChanged, this,
            [this, slice](double /*db*/) {
        if (!m_externalDiversityRouteActive
            || slice->sliceIndex() != kExternalDiversityTargetSliceId
            || sliceById(kExternalDiversityTargetSliceId) != slice) {
            return;
        }
        configureExternalDiversityRotation(slice);
    });

    // Send initial frequency to radio (after connection init completes).
    // XIT offset applied here too so on-connect TX NCO matches the stored
    // XIT state without needing a separate update trigger.
    QTimer::singleShot(100, this, [this, slice]() {
        if (m_connection && m_connection->isConnected()) {
            int rxIdx = slice->streamIndex();
            quint64 freqHz = static_cast<quint64>(slice->frequency());
            if (rxIdx >= 0) {
                m_receiverManager->setReceiverFrequency(rxIdx, freqHz);
            }
            // Seed the transmit frequency from the TX-bound slice, which on
            // connect is usually but not necessarily this one.
            pushTxFrequencyFromTxSlice();
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

    // Unconditional state-restored hook for view layer.
    //
    // wireSliceToSpectrum() runs at sliceAdded() time — BEFORE this function —
    // so it seeds m_spectrumWidget->setDdcCenterFrequency() / setCenterFrequency
    // / setVfoFrequency with the slice's PRE-restore default values.  After
    // restoreFromSettings() above, the slice now holds the persisted values,
    // but the spectrum widget will only learn about them via
    // SliceModel::frequencyChanged, which is gated:
    //   (a) qFuzzyCompare guard at SliceModel.cpp:145 — no emit if equal;
    //   (b) MainWindow::wireSliceToSpectrum lambda's offScreen test — with
    //       CTUN=true (default) and persisted freq within ±halfBw of the
    //       default seed, the lambda hits the CTUN-shift branch and never
    //       calls setDdcCenterFrequency.
    //
    // This unconditional emit gives MainWindow an explicit hook to push the
    // now-correct slice freq/mode/filter into the spectrum widget and VFO
    // flag — mirroring Thetis chkPower_CheckedChanged calling
    // txtVFOAFreq_LostFocus() unconditionally at console.cs:27204
    // [v2.10.3.13] as the explicit "push state to display" step at power-on.
    emit sliceStateRestored(slice->sliceIndex());
}

// Issue #153 sub-bug 2 — push the TX-bound slice's DSPMode and the
// TransmitModel's positive audio-space bandpass to TxChannel.  SliceModel
// filter bounds are RX/IQ-space and are signed for LSB-family modes; using
// them here would make TxChannel::applyTxFilterForMode negate them twice.
// See header comment for wire
// targets and Thetis source-of-truth cites.  Called by all three
// triggers (createTxChannel post-create, SliceModel::dspModeChanged,
// MoxController::txAboutToBegin).
//
void RadioModel::pushTxModeAndBandpass()
{
    SliceModel* const slice = txBoundSlice();
    if (!slice) {
        return;
    }
    const DSPMode mode      = slice->dspMode();
    const int     audioLow  = m_transmitModel.filterLow();
    const int     audioHigh = m_transmitModel.filterHigh();

    // Diagnostic / test-observation hook fires unconditionally (m_txChannel
    // can be null during odd lifecycle moments — addSlice before
    // connectToRadio's WDSP-init lambda runs — and tests rely on the
    // emit to verify the trigger pipeline without standing up the full
    // TX pipeline).
    emit txModeAndBandpassPushed(mode, audioLow, audioHigh);

    if (!m_txChannel) {
        return;
    }

    // Queue the WDSP setter call to TxWorkerThread.  receiver=m_txChannel
    // routes via auto-queued connection; the lambda body runs on the
    // worker thread, where setTxMode / requestFilterChange are then
    // same-thread direct calls.  Mirrors the F.1 / F.2 / H.1 wires inside
    // connectToRadio's txSetup lambda (RadioModel.cpp ~1957).
    QMetaObject::invokeMethod(m_txChannel,
                              [tx = m_txChannel, mode, audioLow, audioHigh]() {
        tx->setTxMode(mode);
        tx->requestFilterChange(audioLow, audioHigh, mode);
    });
}

void RadioModel::applyTxAntennaFromBoundSlice()
{
    const SliceModel* const slice = txBoundSlice();
    if (!slice) {
        return;
    }

    const QString ant = slice->txAntenna();
    int antNum = 1;
    if (ant == QLatin1String("ANT2")) {
        antNum = 2;
    } else if (ant == QLatin1String("ANT3")) {
        antNum = 3;
    }

    // AlexController remains the single safety gate for blocked ANT2/ANT3.
    m_alexController.setTxAnt(
        bandFromFrequency(slice->frequency()), antNum);
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

// ---------------------------------------------------------------------------
// republishAlexAdcSlices — Phase 3F. Feed the per-ADC BPF analysis, then push
// its answer at the wire.
//
// Reported by CT1IQI on PR #293 (2026-05-31):
//   "the Alex control system as now coded appears to be not aware that systems
//    with dual ADCs like the Anan G2 also have dual input band pass filters
//    [...] This has to be reviewed per ADC, its assigned DDCs, and filter
//    chain."
//
// AlexController::recomputeBpf already performed exactly that review, but
// nothing called notifySlicesOnAdc outside the unit tests, so the analysis
// never ran against a live slice set and no wire byte was composed from it.
// The HPF came from whichever receiver was retuned last, so slice A on 20 m
// went deaf the moment slice B tuned 40 m.
//
// Upstream behaviour, for the record. Thetis has no bypass-on-multi-band
// concept anywhere. It avoids the collision two ways:
//   1. Two independent wire words. Alex0 (`prbpfilter`) is ADC0's chain, fed
//      from setAlex1HPF(_rx1_dds_freq); Alex1 (`prbpfilter2`) is ADC1's, fed
//      from setAlex2HPF(rx2_dds_freq_mhz).
//        From Thetis console.cs:15401 + 15435-15443 [v2.10.3.15]
//[2.10.3.13]MW0LGE
//        From Thetis ChannelMaster/network.c:1040-1050 [v2.10.3.15]
//        Upstream inline attribution preserved verbatim (console.cs:15441):
//          HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
//      That half IS a port: it is what this function makes reachable.
//   2. On boards with only one filter board (_rx2_preamp_present == false)
//      it widens the single HPF to the LOWER of the two receiver frequencies
//      rather than picking one:
//        From Thetis console.cs:15500-15510 UpdateAlexRXFilter [v2.10.3.15]
//          if (!_rx2_preamp_present && chkRX2.Checked)
//          {
//              if (rx1_dds_freq_mhz < rx2_dds_freq_mhz) setAlex1HPF(rx1_dds_freq_mhz);
//              else setAlex1HPF(rx2_dds_freq_mhz);
//          }
//
// NereusSDR DIVERGES on (2): the Auto policy bypasses instead of widening.
// Rationale, per design doc §4 and the reporter's own description of the
// hardware:
//   - Thetis's widening is only sound on a high-pass ladder, where a lower
//     corner still passes the higher slice. It runs exclusively on the legacy
//     single-filter-board radios. On the Mk II BPF boards (ANAN-7000 / 8000 /
//     G2), which the report is about, Thetis never runs it at all: those set
//     _rx2_preamp_present = true, which makes UpdateAlexRXFilter a no-op
//     (console.cs:14783-14857 [v2.10.3.15]). If those selections really are
//     band-pass, widening would leave the higher slice attenuated, which is
//     the reported bug again by another route. Bypass is correct under either
//     reading of the hardware.
//   - Thetis tops out at two receivers on two chains. NereusSDR puts up to
//     five slices on two chains, so "two receivers, two filters" does not
//     cover the case where three slices share one ADC across three bands.
// Operators who prefer a filtered chain over a wide one keep BpfMode::
// ForceBand.
// ---------------------------------------------------------------------------
void RadioModel::republishAlexAdcSlices()
{
    // Re-entrancy guard. notifySlicesOnAdc below recomputes, and a recompute
    // that lands on a new answer emits bpfStateChanged, which is wired back
    // here (see the connect in the constructor) so that an operator override
    // or a wideband toggle reaches the wire on its own trigger.
    //
    // recomputeBpf's change-only emit (AlexController.cpp:152-154) is not by
    // itself enough to stop that becoming a loop: reasonText is part of the
    // change test and it carries the band label, so in Auto mode every band
    // crossing re-emits. The re-entry arrives midway through the ADC loop,
    // where ADC1 has not been notified yet, so the nested pass would compose
    // and push ADC1 from stale state before the outer pass corrects it.
    //
    // Dropping the nested call is safe: the outer call has not read any
    // AlexController state yet at the point the emit fires. It goes on to
    // notify every remaining ADC and then reads all of them, so the single
    // push it makes is composed from state that already includes whatever
    // change triggered the emit.
    if (m_republishingAlexBpf) {
        return;
    }
    m_republishingAlexBpf = true;
    const QScopeGuard clearReentrancyGuard(
        [this]() { m_republishingAlexBpf = false; });

    // Two chains: Alex0/ADC0 and Alex1/ADC1. AlexController is sized to match.
    constexpr int kAdcCount  = 2;
    constexpr int kSliceSlots = 5;

    // How many of those two the connected board actually drives (defect D4).
    // Not adcCount: see chainForStream for the ANAN-100D / 200D counterexample
    // and the upstream cite.
    const int chainCount =
        std::clamp(boardCapabilities().rxFilterChainCount, 1, kAdcCount);

    // ── Diversity: one decision, applied to both chains ──────────────────
    //
    // Raised by CT1IQI on PR #293: under diversity Alex0 and Alex1 must be
    // set identical, and that may mean identically bypassed.
    //
    // The reason is the DDC pair itself. Diversity runs DDC0 and DDC1 as a
    // synchronous pair (P2CodecOrionMkII: ddcEnable |= 0x03, syncEnable |=
    // 0x02) with DDC0 on ADC0 and DDC1 on ADC1, sampling ONE signal through
    // TWO front ends so the combiner can weight them against each other. Two
    // different preselectors on those legs means two different amplitude and
    // group-delay responses, and the combiner has nothing coherent left to
    // work with. A filtered leg against a bypassed leg is the worst version
    // of that.
    //
    // So when the pair is engaged, every slice counts on BOTH chains. That
    // makes the two band sets identical by construction, which makes the two
    // decisions identical by construction -- one range and both chains filter
    // it, more than one and both chains bypass together. There is no separate
    // mirroring step to keep in sync.
    //
    // Thetis cannot be ported here: it has no bypass-on-multi-band concept at
    // all, and its diversity is two receivers on two chains where the chain
    // count and the receiver count are the same number. It never faces five
    // slices over two chains, so it never had to answer this. NereusSDR-
    // original policy, stated per the reporter's requirement.
    const bool diversityPair = diversityActive() && chainCount >= 2;

    std::array<std::array<Band, kSliceSlots>, kAdcCount> bands;
    for (auto& perAdc : bands) { perAdc.fill(Band::Count); }
    std::array<std::array<quint8, kSliceSlots>, kAdcCount> preselectors{};
    std::array<int, kAdcCount>    counts   {0, 0};
    std::array<double, kAdcCount> lowestHz {0.0, 0.0};

    const HPSDRHW alexBoard = boardCapabilities().board;
    auto addToChain = [&bands, &preselectors, &counts, &lowestHz, alexBoard](
                          int chain, Band band, double hz) {
        if (chain < 0 || chain >= kAdcCount) { return; }
        const quint8 physicalFilter =
            codec::alex::computeRxPreselector(hz / 1.0e6, alexBoard);

        // Compatibility is a property of the relay selection, not the Band
        // enum. Several amateur bands share one physical filter (for
        // example 20/17/15 m on the Saturn BPF1 bank). Count those as one
        // compatible range; bypass only when the chain would need two
        // different relay selections at once.
        for (int i = 0; i < counts[chain]; ++i) {
            if (preselectors[chain][i] == physicalFilter) {
                if (hz < lowestHz[chain]) { lowestHz[chain] = hz; }
                return;
            }
        }

        if (counts[chain] >= kSliceSlots) { return; }
        bands[chain][counts[chain]] = band;
        preselectors[chain][counts[chain]] = physicalFilter;
        if (counts[chain] == 0 || hz < lowestHz[chain]) { lowestHz[chain] = hz; }
        ++counts[chain];
    };

    for (SliceModel* s : std::as_const(m_slices)) {
        if (s == nullptr) { continue; }

        // An unbound slice has no DDC, so it is not on any chain and must not
        // drag a filter wide on behalf of a receiver that is not running.
        // chainForStream returns -1 for exactly that case.
        const int chain = chainForStream(s->streamIndex());
        if (chain < 0) { continue; }

        const double hz = s->frequency();
        const Band   b  = bandFromFrequency(hz);

        if (diversityPair) {
            for (int c = 0; c < chainCount; ++c) { addToChain(c, b, hz); }
        } else {
            addToChain(chain, b, hz);
        }
    }

    // Every chain AlexController models is notified, including one the board
    // does not have: the array for it is empty, which is the correct input for
    // a chain with nothing on it and which clears any state left over from a
    // previously connected two-chain radio.
    for (int adc = 0; adc < kAdcCount; ++adc) {
        m_alexController.notifySlicesOnAdc(adc, bands[adc]);
    }

    // Translate each chain's effective state into Thetis HPF bits.
    // -1 = nothing receiving on this ADC, so the connection keeps whatever it
    // had. Mirrors Thetis, which only calls setAlex2HPF when RX2 exists
    // (console.cs:15435-15442 [v2.10.3.15]) and otherwise never writes
    // prbpfilter2's HPF nibble.
    // Upstream inline attribution preserved verbatim (console.cs:15441):
    //   HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    auto hpfBitsFor = [this, &counts, &lowestHz, chainCount](int adc) -> int {
        // Defect D4. A board that does not drive this chain never gets a word
        // composed for it, whatever the slice grouping says. chainForStream
        // already folds every stream onto chain 0 on such a board, so this is
        // belt to that brace -- but it is the brace that sits next to the wire
        // write, and the invariant it states ("no word for a chain the board
        // has not got") is the one that matters if the fold is ever changed.
        if (adc >= chainCount) { return -1; }

        if (counts[adc] == 0) { return -1; }

        const AlexController::AlexAdcState& st = m_alexController.adcState(adc);
        if (st.effective == AlexController::BpfEffective::Bypass
            || st.effective == AlexController::BpfEffective::WidebandLocked) {
            // 0x20 is the bypass encoding on both chains.
            // From Thetis ChannelMaster/netInterface.c:604-651 [v2.10.3.15]:
            //   prbpfilter->_Bypass  = (bits & 0x20) != 0;
            //   prbpfilter2->_Bypass = (bits & 0x20) != 0;
            return 0x20;
        }

        // Filtered. Select from the LOWEST frequency on the chain, which is
        // Thetis's own rule for a shared filter (UpdateAlexRXFilter above) and
        // which, with a single band on the chain, returns byte-for-byte what
        // the frequency-derived path returned before this change.
        //
        // Route through the board-appropriate ladder: Orion MkII / Saturn /
        // HermesC10 carry a band-pass bank on these bits, not the legacy
        // high-pass ladder.
        // From Thetis console.cs:6827-6837 setAlex1HPF [v2.10.3.15]
        // Upstream inline attribution preserved verbatim (console.cs:6830):
        //    || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
        return int(codec::alex::computeRxPreselector(lowestHz[adc] / 1.0e6,
                                                     boardCapabilities().board));
    };

    AlexRxBpf bpf;
    bpf.hpfBitsAdc0 = hpfBitsFor(0);
    bpf.hpfBitsAdc1 = hpfBitsFor(1);

    if (m_connection == nullptr) {
        // Disconnected: AlexController's state still updated above, which is
        // what the CH indicator and FilterPolicyDialog read. Nothing to send.
        return;
    }

    // Marshal to the connection worker thread, same as applyAlexAntennaForBand.
    RadioConnection* conn = m_connection;
    QMetaObject::invokeMethod(conn, [conn, bpf]() {
        conn->setAlexRxBpf(bpf);
    });
}

// ---------------------------------------------------------------------------
// panBypassState — Phase 3F. Resolve the WIDE badge for one panadapter.
//
// NereusSDR-original; no upstream port. Thetis has no per-pan bypass
// indicator because it has no multi-pan layout: its single wideband-window
// bypass is reported by the window itself (console.cs:43545-43566
// [v2.10.3.15]). The routing below is the multi-pan generalisation, per
// design doc 2026-05-26-phase3f-multi-pan-multi-slice-design.md §16.4.
//
// The badge answers one question -- "is the RX preselector feeding THIS pan
// bypassed right now?" -- and the routing is the only honest way to answer
// it, because bypass is a property of the ADC chain while a pan is a
// property of the display:
//
//     pan -> its slices -> their stream -> that stream's ADC -> effective
//
// The chain comes from chainForStream, which is the shared resolver: it reads
// the ADC publishDdcAssignment decoded out of the codec's own assignment, and
// folds it onto chain 0 on a board with fewer filter chains than ADCs. Before
// the D1 fix this read ReceiverConfig::adcIndex inline and that field was
// pinned at 0, so a pan showing a slice the radio had moved to ADC1 reported
// chain 0's bypass state.
//
// Deliberately a pure query with no caching. It runs once per pan on the
// same triggers as rebuildFftRouting (single-digit slices, single-digit
// pans), and a cache would need invalidating on every one of the six events
// that can move the answer.
// ---------------------------------------------------------------------------
namespace {

// "20m and 40m", "20m, 40m and 6m". Plain prose: this text is read by an
// operator mid-QSO, not parsed.
QString joinRangeNames(const QStringList& names)
{
    if (names.isEmpty())    { return QString(); }
    if (names.size() == 1)  { return names.first(); }
    QStringList head = names;
    const QString last = head.takeLast();
    return QStringLiteral("%1 and %2").arg(head.join(QStringLiteral(", ")), last);
}

} // namespace

RadioModel::PanBypassState
RadioModel::panBypassState(const QSet<int>& sliceIndices) const
{
    PanBypassState result;

    // Chain count matches AlexController's own sizing (AlexController.h:217).
    constexpr int kAdcCount = 2;

    // Which chains feed this pan. A pan usually sits on one, but nothing
    // stops an operator hosting two slices from different chains on it, so
    // collect the set rather than taking the first.
    std::array<bool, kAdcCount> feeds{};
    feeds.fill(false);

    for (int sliceId : sliceIndices) {
        // sliceChainIndex is the shared resolver: the CH tag the overlay
        // paints beside this pill reads the same call, so a pan can never
        // name one chain and report the other's bypass state.
        //
        // It also returns -1 for an unbound slice, which is the case this
        // loop already wanted to skip: no DDC means no chain, so the pan
        // showing it is fed by nothing. Not wide, not filtered. Matches
        // republishAlexAdcSlices, which skips it for the same reason.
        const int adc = sliceChainIndex(sliceId);
        if (adc < 0 || adc >= kAdcCount) { continue; }
        feeds[adc] = true;
    }

    for (int adc = 0; adc < kAdcCount; ++adc) {
        if (!feeds[adc]) { continue; }

        const AlexController::AlexAdcState& st = m_alexController.adcState(adc);
        if (st.effective == AlexController::BpfEffective::Filtered) { continue; }

        result.bypassed = true;
        result.reason   = bypassReasonForAdc(adc, st);
        // First offending chain wins the tooltip. A pan straddling a wide and
        // a filtered chain is wide either way; naming the exposed one is what
        // the operator needs.
        break;
    }

    return result;
}

// ---------------------------------------------------------------------------
// sliceChainIndex: the one place slice -> stream -> ADC is resolved.
//
// Extracted from panBypassState when the per-pan status overlay needed the
// same answer for its CH tag. Two copies of this hop would have been two
// answers that could drift, and they sit side by side in the same overlay:
// a pan naming CH 0 while its WIDE pill reported chain 1's bypass state is
// worse than either being wrong alone.
//
// Resolves by slice ID, not list position. sliceById is the documented
// lookup (RadioModel.h:358-366): addSlice hands out the lowest FREE id and
// removeSlice does not renumber the survivors, so after any mid-list removal
// an id and a position name different slices. PanadapterApplet::
// associatedSlices is keyed by id, so a positional lookup here reads a
// neighbouring slice's chain -- silently, and only for operators who had
// removed a slice.
// ---------------------------------------------------------------------------
int RadioModel::sliceChainIndex(int sliceId) const
{
    SliceModel* s = sliceById(sliceId);
    if (s == nullptr) { return -1; }

    // No DDC stream means no chain. Distinct from chain 0: callers have to be
    // able to tell "fed by nothing" from "fed by the first ADC".
    return chainForStream(s->streamIndex());
}

// ---------------------------------------------------------------------------
// chainForStream: stream -> filter chain, the one place that hop is resolved.
//
// Defect D1. The ADC half of this used to be read inline in three places
// (republishAlexAdcSlices, sliceChainIndex, bypassReasonForAdc), each of them
// grouping by ReceiverConfig::adcIndex, which nothing in production ever
// wrote to anything but 0. publishDdcAssignment now records the codec's real
// answer in m_streamAdc; this is the single read of it.
//
// Defect D4 lives here too, in the fold to chain 0. adcIndex is an ADC index
// and rxFilterChainCount is a count of preselector banks, and they are not
// the same number on every SKU:
//
//   From Thetis console.cs:15435-15443 [v2.10.3.15] UpdateRX2DDSFreq:
//[2.10.3.13]MW0LGE
//   setAlex2HPF, the only writer of the chain-1 filter word, runs for
//   ORIONMKII, ANAN7000D, ANAN8000D, ANAN_G2, ANAN_G2_1K, ANVELINAPRO3 and
//   REDPITAYA and for nothing else.
//   Upstream inline attribution preserved verbatim (console.cs:15441):
//     HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
//
// ANAN-100D and ANAN-200D are absent from that list yet are both
// NetworkIO.SetRxADC(2) (clsHardwareSpecific.cs:123 and :140 [v2.10.3.15]).
// Two ADCs behind one filter bank. A stream routed to ADC1 on such a board is
// still behind chain 0's filter, so its range must be counted against chain 0
// or the chain would be filtered to a band that slice is not on and the slice
// would go deaf. Folding is the physical answer there, not a safety clamp.
//
// It is also what stops a chain-1 filter word being composed for a board with
// no chain 1: with every stream folded onto chain 0, republishAlexAdcSlices
// finds chain 1 empty and pushes -1 for it.
// ---------------------------------------------------------------------------
int RadioModel::chainForStream(int stream) const
{
    if (stream < 0 || stream >= static_cast<int>(m_streamAdc.size())) {
        return -1;
    }

    const int adc = m_streamAdc[static_cast<size_t>(stream)];
    if (adc <= 0) { return 0; }

    // adc can legitimately exceed the chain count (a 2-ADC / 1-chain board),
    // and under PureSignal the ADC field can name ADC2, the PA feedback
    // input, which is not a receive chain at all. Both fold to chain 0.
    return (adc < boardCapabilities().rxFilterChainCount) ? adc : 0;
}

// ---------------------------------------------------------------------------
// bypassReasonForAdc — the operator-facing sentence behind the WIDE badge.
//
// One sentence per cause, per design doc §16.4.4. WIDE itself is a single
// unambiguous statement about the RF path (§16.4.3): extended view is one of
// the CAUSES of bypass, not a second meaning of the word. So the badge never
// changes and the cause is named here.
//
// AlexAdcState::reasonText is not reused verbatim. It is the compact chain
// summary the bottom-bar CH label wants ("BYPASS (multi-band: 20m + 40m)");
// the badge tooltip has to tell the operator what to DO about it.
//
// Project rule: no source citations inside user-visible strings.
// ---------------------------------------------------------------------------
QString RadioModel::bypassReasonForAdc(
    int adc, const AlexController::AlexAdcState& st) const
{
    if (st.effective == AlexController::BpfEffective::WidebandLocked) {
        // Design doc §16.4.4, "extended view" row, verbatim.
        return tr("Preselector bypassed because this panadapter is showing "
                  "more spectrum than the receiver's own bandwidth. Zoom back "
                  "in, or turn off extended view for this pan, to restore "
                  "filtering.");
    }

    if (m_alexController.bpfMode(adc) == AlexController::BpfMode::ForceBypass) {
        // Design doc §16.4.4, "operator override" row, verbatim.
        return tr("Preselector bypassed by your Filter Policy setting for this "
                  "chain. Click to change it.");
    }

    // Auto policy on a multi-range chain. Name the ranges actually in
    // conflict: told only that the receiver is wide, the operator has no way
    // to work out which two slices caused it or what to change.
    //
    // Read the live slice set rather than parsing AlexAdcState::reasonText,
    // which is a display string and not a contract. Same grouping rule as
    // republishAlexAdcSlices so the two can never disagree.
    QStringList rangeNames;
    QSet<Band> seen;
    for (SliceModel* s : m_slices) {
        if (s == nullptr) { continue; }
        const int sliceChain = chainForStream(s->streamIndex());
        if (sliceChain != adc) { continue; }
        const Band b = bandFromFrequency(s->frequency());
        if (seen.contains(b)) { continue; }
        seen.insert(b);
        rangeNames << bandLabel(b);
    }

    if (rangeNames.size() < 2) {
        // Bypassed with fewer than two ranges on the chain means something
        // other than the multi-range rule put it there (a mode we do not
        // model yet). Say the true thing and stop.
        return tr("Preselector bypassed for this receiver chain. Click to "
                  "change the filter policy for this chain.");
    }

    // Design doc §16.4.4, "multi-range auto" row, with the live range names
    // substituted and the remedy the operator can act on themselves added
    // ahead of the Filter Policy pointer.
    return tr("Preselector bypassed. This receiver chain is serving %1 at "
              "once, and the band-pass filter can only pass one of them. "
              "Bypassing keeps both slices hearing. Retune or close a slice "
              "to restore filtering, or click to change the filter policy for "
              "this chain.")
        .arg(joinRangeNames(rangeNames));
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

    // The external pdiv slot is independent of RXA channel ownership. Quiesce
    // its worker feed, stop Run, and destroy it before the DSP thread or any
    // target RxChannel begins teardown. WdspEngine::shutdown's global sweep is
    // then an idempotent safety net rather than the primary lifecycle owner.
    stopExternalDiversityRoute();

    // 2026-07-27 (ANAN-G2E lockup): quiet period for discovery.  Disconnect
    // reopens the ConnectionPanel, whose ctor auto-scans — wire captures show
    // the broadcast probe burst landing 7-15 ms after our run=0 frame, and
    // both observed G2E lockups happened inside that window.  Thetis sends
    // nothing after its stop frame and never wedges the same radio.  Hold
    // discovery off (defer, not drop) until the radio's stop transition and
    // its ~2 s firmware deadman window have passed.  Applies to every
    // teardown flavor: user disconnect, failed-connect watchdog, quit.
    //
    // Armed here so nothing scans *during* teardown, and armed again after
    // the protocol disconnect completes below — that second arm is the one
    // that guarantees a full quiet period measured from the stop frame.
    if (m_discovery) {
        m_discovery->holdOffScans(kPostDisconnectScanQuietMs);
    }

    // Flush any pending coalesced slice save FIRST so the user's last
    // AF / step / freq / lock / RIT tweak isn't lost to the 500 ms
    // debounce in scheduleSettingsSave(). The QTimer there can't fire
    // while teardown is running on the main thread, so without this an
    // immediate close-after-tweak silently drops the change. Cheap and
    // idempotent — no-op when nothing's pending.
    flushPendingSettingsSave();

    // Stop the FlexRadio discovery beacon so we no longer announce ourselves
    // as "Available" after the radio disconnects.
    if (m_flexBroadcaster) {
        m_flexBroadcaster->stop();
    }

    // 3M-1a G.1 fixup: drop any prior WdspEngine::initializedChanged subscribers
    // we registered in connectToRadio(). Without this, each reconnect cycle
    // accumulates another copy of the WDSP-init lambda, causing duplicate
    // createRxChannel + createTxChannel(kTxChannelId) calls on the next initializedChanged.
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

    // Phase 3F Sub-Epic C Task 6: persist TxBoundSliceId per-MAC before
    // the connection tears down.  save() keys off the MAC the arbiter was
    // last fed (currentRadioChanged lambda in the ctor), so it stays
    // pinned across teardown for the no-op idempotent case.  Empty-MAC
    // guard inside save() makes the test-mock setLastRadioInfoForTest
    // path a no-op automatically.
    if (m_txSliceArbiter) {
        m_txSliceArbiter->save();
    }

    // Issue #259 — flush step-attenuator state to AppSettings BEFORE the
    // RadioConnection is destroyed. The MainWindow disconnect-branch in
    // onConnectionStateChanged also calls saveSettings(), but by the time
    // setConnectionState(Disconnected) fires below, m_connection has
    // already been nulled by teardownWorkerThreadedConnection() and the
    // `if (m_radioModel->connection())` guard there skips the save. Use
    // m_lastRadioInfo.macAddress (which survives teardown) to pin the
    // per-MAC keys, mirroring the TransmitModel pattern just above.
    //
    // StepAttenuatorController::saveSettings is itself gated on
    // m_loadedMac == mac, so this call is a no-op when teardown fires
    // before loadSettings (the pre-load clobber path the bench trace
    // surfaced — see StepAttenuatorController::saveSettings comment).
    // After saving, mark the controller as unloaded so a fresh connect
    // (even to the same MAC) must re-load before its teardown can save
    // again.
    if (m_stepAttController && !m_lastRadioInfo.macAddress.isEmpty()) {
        m_stepAttController->saveSettings(m_lastRadioInfo.macAddress);
        m_stepAttController->markSettingsUnloaded();
    }

    // Issue #177 — clear any in-flight TUN-off bookkeeping.  If we are
    // tearing down mid-walk, MoxController::rxReady will never fire (timers
    // are stopped) so the deferred completion would otherwise stay armed
    // across the next connect.  Clearing the latch + m_isTuning matches
    // Thetis chkTUN_CheckedChanged's _tuning=false reset (console.cs:30122
    // [v2.10.3.13]) at session end.
    m_pendingTuneOff = false;
    m_isTuning       = false;

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
        QObject::disconnect(m_connection, nullptr, m_dspWorker, nullptr);
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
    //      destroyTxChannel(kTxChannelId)) runs on the right thread; Qt asserts
    //      otherwise.
    //   3. unique_ptr.reset() — destroys the TxWorkerThread itself.
    //
    // Replaces the deleted L.4 MicReBlocker teardown.  See plan §5.2.
    if (m_txWorker) {
        // stopPump() internally calls m_txMicSource->stop() (the poison
        // semaphore release that breaks the worker out of waitForBlock).
        m_txWorker->stopPump();
        if (m_txChannel) {
            // Issue #258: TxWorkerThread::run() moves m_txChannel back to
            // this thread before returning, so by the time stopPump's
            // QThread::wait() unblocks, m_txChannel is already here.  This
            // call is therefore a defensive no-op (Qt early-returns when
            // the target thread equals the object's current thread).  Keep
            // it so the invariant is explicit at the teardown site and a
            // future refactor that breaks the run()-side move still leaves
            // teardown safe rather than introducing a hard crash.
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

    // 3M-4 Task 7: tear down PureSignal before the TxChannel pointer dies.
    // PureSignal::dtor stops its polling timers and issues a final
    // SetPSControl(1, 0, 0, 0) + SetPSMox(false) to leave the WDSP engine
    // in a known state.  Reset before the TxChannel teardown below so the
    // dtor's setPSControl call still routes to a live channel.  Detach the
    // TransmitModel seam first so any late-firing setPowerUsingTargetDbm
    // doesn't dereference a half-destructed PureSignal.
    m_transmitModel.setPureSignal(nullptr);
    m_pureSignal.reset();
    // Phase 3M-4 Task 17 chunk C: drain the pscc() driver before TxChannel
    // teardown so any in-flight pump drains cleanly.  PsccPump::~ default
    // destructor releases the rings; no explicit deactivate needed.
    m_psccPump.reset();
    // Phase 3M-4 Task 13: notify subscribers that the coordinator is gone
    // so they can disconnect their wiring cleanly (the applets re-arm on
    // the next pureSignalCoordinatorReady emit at reconnect).
    emit pureSignalCoordinatorReady(nullptr);

    // 3M-1c L.1: drop the per-MAC scope on the profile manager so subsequent
    // mutators silently no-op until the next connectToRadio() sets a new MAC.
    if (m_micProfileMgr) {
        m_micProfileMgr->setMacAddress(QString());
    }

    // Phase 4 Agent 4A of #167: drop PaProfileManager MAC scope (mirrors
    // MicProfileManager teardown above).  Subsequent activeProfile() reads
    // return nullptr until the next connectToRadio() sets a new MAC, so the
    // drive-slider / TUNE callsites silently no-op (their early-return
    // guard `if (!activeProfile)` covers this).
    if (m_paProfileManager) {
        m_paProfileManager->setMacAddress(QString());
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
    // so CompositeTxMicRouter (which holds raw pointers to pc + radio sources
    // + the VAX TX source registered via setVaxSource) is released BEFORE the
    // sources it points into.
    // After reset(), pullSamples() on the composite is unreachable (TxChannel
    // already has setMicRouter(nullptr) above).
    m_compositeMicRouter.reset();
    m_vaxTxMicSource.reset();
    m_radioMicSource.reset();
    m_pcMicSource.reset();

    // Clear the non-owning TX channel view before WdspEngine::shutdown()
    // destroys the underlying WDSP channel. Any in-flight txReady / txaFlushed
    // slot calls are queued and will see m_txChannel == nullptr after this clear.
    // WdspEngine::shutdown() → destroyTxChannel(kTxChannelId) handles the actual WDSP teardown.
    m_txChannel = nullptr;

    // Shutdown WDSP (destroys all channels, saves cache)
    m_wdspEngine->shutdown();

    // Disconnect remaining signals (prevents new work being queued)
    QObject::disconnect(m_connection, nullptr, this, nullptr);
    QObject::disconnect(m_connection, nullptr, m_receiverManager, nullptr);
    QObject::disconnect(m_receiverManager, nullptr, this, nullptr);

    // ── Phase 3F Sub-Epic I closeout, defect F1 ─────────────────────────
    //
    // Unbind every slice so the next connect actually re-binds it. The DSP
    // worker was destroyed a few dozen lines up, taking its stream->slice
    // map with it, but the SliceModels kept their streamIndex; the reconnect
    // bind loop in connectToRadio is guarded on `streamIndex() < 0`, so it
    // skipped every slice, nothing republished, and the new worker had only
    // its constructor seed. A Slice B that was on stream 1 demodulated
    // nothing until the operator happened to retune it.
    //
    // Sits beside ReceiverManager::reset() because it is the same kind of
    // step: drop the routing state so the next connect starts clean.
    releaseStreamBindings();

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

    // Re-arm the discovery quiet period now that the protocol disconnect has
    // actually completed.  The arm at the top of this function starts the
    // clock at teardown *entry*, but run=0 does not leave until
    // teardownWorkerThreadedConnection() dispatches
    // P2RadioConnection::disconnect() onto the connection thread — after the
    // audio/WDSP/TX shutdown above.  Measured on the 2026-07-27 bench that
    // cost 680 ms of the intended 3 s window (SendStop 17:31:38.149, scan
    // 17:31:40.473), and teardownWorkerThreadedConnection alone may block up
    // to kDispatchTimeoutMs (3000 ms) if the connection thread is stuck in
    // onReadyRead — which would expire the whole holdoff before the stop
    // frame is even sent, re-entering the exact window this guards.
    // holdOffScans() keeps the later deadline, so arming twice only extends.
    // Codex review, PR #306.
    if (m_discovery) {
        m_discovery->holdOffScans(kPostDisconnectScanQuietMs);
    }

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

// v0.4.1 hotfix — single point that fans the connected hardware HPSDRModel
// out to every sub-model that needs it.  Replaces three previously-separate
// sites (m_hardwareProfile = profileForModel, m_transmitModel.setHpsdrModel,
// and the missing m_receiverManager->setHpsdrModel that broke PureSignal in
// v0.4.0).
//
// Production caller: RadioModel::connectToRadio (after model-override /
// defaultModelForBoard resolution at the top of the function).
// Test caller:       setHpsdrModelForTest (test-only seam in RadioModel.h).
//
// ReceiverManager::setHpsdrModel is null-guarded because legacy test
// fixtures may construct a RadioModel with sub-models still null at the
// moment a test injects a model via setHpsdrModelForTest.  Production
// flow always has m_receiverManager non-null (constructed in RadioModel
// ctor at RadioModel.cpp:464).
//
// Bug context: v0.4.0 shipped without the ReceiverManager push, leaving
// m_hpsdrModel at the safe Atlas default HPSDRModel::HPSDR.  The codec
// layer (P1CodecStandard::applyPureSignalDdcConfig) dispatches on this
// enum — see codec/P1CodecStandard.cpp:339-391.  Without the correct
// model the switch falls through to the default branch and emits an
// empty PsDdcConfig, which keeps PsccPump inactive (its wantActive gate
// requires ddcEnable bit 0 set, syncEnable bit 1 set, and matching
// ps_rates — all zero in an empty cfg).  Result: no feedback samples
// reach calcc → state[15] stays 0 → PureSignal never converges.
// HL2 / G2 / Saturn / RedPitaya unaffected because their codecs ignore
// the model parameter (P1CodecHl2.cpp:530, P2CodecOrionMkII.cpp:436,
// P1CodecRedPitaya.cpp:77).
void RadioModel::applyHpsdrModel(HPSDRModel m)
{
    m_hardwareProfile = ::NereusSDR::profileForModel(m);
    m_transmitModel.setHpsdrModel(m_hardwareProfile.model);
    if (m_receiverManager) {
        m_receiverManager->setHpsdrModel(m_hardwareProfile.model);

        // Defect D2, second consumer. ReceiverManager keeps its own shadow of
        // the per-DDC ADC routing word for PsDdcConfig observation consumers
        // and for Protocol 1's legacy wire path.
        // setRxAdcCtrl1 / setRxAdcCtrl2 had no production caller at all, so
        // that shadow sat at 0 while the complete-assignment path was about
        // to start seeding 4, so the observation and wire views would have
        // disagreed about where DDC1 lives.
        //
        // The disagreement is currently invisible on the wire -- the PS
        // branches mask the field out with `(adcCtrl1 & 0xf3) | 0x08` and
        // ReceiverManager::setDiversityEnabled has no production caller
        // either -- but "invisible today" is exactly the condition under
        // which two copies of a value drift apart, which is the defect class
        // being closed here. Seed both from the same board-gated source.
        const quint16 seed =
            NereusSDR::defaultRxAdcCtrl(boardCapabilities().adcCount);
        m_receiverManager->setRxAdcCtrl1(static_cast<quint8>(seed & 0xff));
        m_receiverManager->setRxAdcCtrl2(static_cast<quint8>((seed >> 8) & 0x3f));
    }
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
        m_connectionActiveRxCount = 0;       // Task 1.7: reset on disconnect
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
        // Per-radio peripherals refactor (2026-05-26): now that the MAC
        // is known and settings have been validated, fire the
        // peripherals lifecycle (one-shot global migration on first run,
        // then start RF-Kit / 4O3A / PGXL / TGXL per the per-MAC flags).
        applyPeripheralsForCurrentMac();
        // Task 10 (#175): push the connected hardware model into TransmitModel
        // so the m_hpsdrModel field (added in Task 6) is non-FIRST before any
        // user TX action fires.  This activates the HL2 polymorphic clamp in
        // setTunePowerForBand (Task 7), the HL2 DSP modulation sub-step in
        // setPowerUsingTargetDbm (Task 4), and the HL2 audio-volume formula in
        // computeAudioVolume (Task 5).  Must be set BEFORE the emit so any
        // slot connected to currentRadioChanged that reads transmitModel()
        // already sees the correct model.
        m_transmitModel.setHpsdrModel(m_hardwareProfile.model);
        // Phase 3I — fan out to HardwarePage so its sub-tabs populate with
        // the connected radio's fields (Radio Info labels, sample rate,
        // capability-gated tab visibility, per-MAC settings restore).
        emit currentRadioChanged(m_lastRadioInfo);
        // Phase 3P-I-a T11 — apply persisted per-band Alex antenna to the
        // fresh connection. Matches Thetis's initial UpdateAlexAntSelection
        // call path on radio startup (HPSDR/Alex.cs:310 [@501e3f5]).
        applyAlexAntennaForBand(m_lastBand);
        // Phase 3F: same for the per-ADC band-pass. The slice set survives a
        // reconnect, so the fresh connection has to be told which chain is
        // filtered and which is wide before the first tune moves anything.
        republishAlexAdcSlices();
        // RF-SAFETY: and the transmit low-pass, for the same reason. A fresh
        // P2RadioConnection starts with m_alex.lpfBitsTx at its 6 m default
        // and only setTxFrequency ever moves it, so without a push here the
        // TX low-pass stays on the default until the operator happens to
        // turn the VFO -- the transmitter would key up on the wrong filter
        // on a radio that was connected and immediately keyed.
        //
        // Thetis re-drives the same state unconditionally rather than
        // trusting that a tune has happened, and says so in as many words:
        //   From Thetis console.cs:29095-29099 HdwMOXChanged [v2.10.3.15]
        //     // make sure TX freq has been set
        //     UpdateRX1DDSFreq();
        //     UpdateRX2DDSFreq();
        //     UpdateTXDDSFreq();
        // (the MOX-off edge repeats it at console.cs:29146-29148), and
        // UpdateTXDDSFreq is what selects the transmit low-pass:
        //   From Thetis console.cs:15464-15468 UpdateTXDDSFreq [v2.10.3.15]
        //     private void UpdateTXDDSFreq()
        //     { if (initializing) return;
        //       setAlexLPF(tx_dds_freq_mhz, true); ... }
        // Upstream inline attribution preserved verbatim (console.cs:15471):
        //   if (MOX)//[2.10.3.13]MW0LGE
        // Doing it once on Connected covers the same hole here, because
        // effectiveLpfBitsAlex0() already switches Alex0 over to the transmit
        // mask at compose time on the MOX edge, so the mask only has to be
        // correct, not re-sent.
        pushTxFrequencyFromTxSlice();
        break;
    case ConnectionState::Disconnected:
        qCDebug(lcConnection) << "Disconnected from" << m_name;
        // Per-radio peripherals refactor (2026-05-26): tear down RF-Kit /
        // 4O3A listener / PGXL / TGXL so they're not still attached to the
        // previous radio's scope when the user reconnects to a different
        // rig.  Done BEFORE clearConnectedMac so the helpers still see the
        // MAC if they need to log diagnostic context.
        teardownPeripherals();
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
        // Per-radio peripherals refactor (2026-05-26): same teardown as
        // Disconnected so peripheral sockets don't stay attached across a
        // link-loss event.
        teardownPeripherals();
        m_discovery->clearConnectedMac();
        // 3M-1c L.2: same as Disconnected — drop the power-on gate.
        if (m_twoToneController) {
            m_twoToneController->setPowerOn(false);
        }
        break;
    }
}

// ── #202 deep-fix: pumpAudioVolume — Audio.RadioVolume setter analogue ──────
//
// Direct port of the Thetis `Audio.RadioVolume` setter side-effects
// (audio.cs:262-271 [v2.10.3.13]):
//   set {
//       radio_volume = value;
//       NetworkIO.SetOutputPower((float)(value * 1.02));
//       cmaster.CMSetTXOutputLevel();
//   }
//
// Wire byte path mirrors NetworkIO.cs:201-211 [v2.10.3.13]:
//   public static void SetOutputPower(float f) {
//       if (f < 0.0) f = 0.0F;
//       if (f >= 1.0) f = 1.0F;
//       int i = (int)(255 * f * _swr_protect);
//       SetOutputPowerFactor(i);
//   }
// — note `f` is the audio_volume * 1.02 already.  SWR foldback (`_swr_protect`)
// multiplies the wire byte HERE, NOT the IQ scalar.  This is the opposite of
// what the prior NereusSDR code did (it placed swrProtect on the IQ scalar).
// The earlier "MW0LGE-canonical topology" comment was a misreading of the
// upstream source — Thetis's `Audio.HighSWRScale` (the IQ-side multiplier in
// cmaster.cs:1117) is set to 1.0 once at console.cs:29194 [v2.10.3.13] and
// never reassigned anywhere in baseline Thetis, making the IQ-side path a
// no-op.  Real SWR foldback in Thetis is wire-byte only.
//
// IQ scalar path mirrors cmaster.cs:1115-1119 [v2.10.3.13]:
//   public static void CMSetTXOutputLevel() {
//       double level = Audio.RadioVolume * Audio.HighSWRScale;
//       cmaster.SetTXFixedGain(0, level, level);
//   }
// With HighSWRScale baseline-1.0, the IQ scalar is just audio_volume.
//
// Caches the value into m_lastAudioVolume so a subsequent
// swrProtectFactorChanged emit can re-pump the same audio_volume through
// updated SWR protect (mirrors console.cs:26102-26109 [v2.10.3.13]
// `Audio.RadioVolume = Audio.RadioVolume` re-emit on _swr_protect change).
void RadioModel::pumpAudioVolume(double audioVolume)
{
    if (!m_connection) {
        // No live connection — cache the value but skip the wire write.
        // The next setPowerUsingTargetDbm after Connected will re-emit
        // and reach the wire path.
        m_lastAudioVolume = audioVolume;
        return;
    }

    m_lastAudioVolume = audioVolume;

    const float swrProtect =
        std::clamp(m_transmitModel.swrProtectFactor(), 0.0f, 1.0f);

    // Byte-for-byte port of NetworkIO.SetOutputPower(float f) at
    // NetworkIO.cs:201-211 [v2.10.3.13].  `f` is `audioVolume * 1.02`
    // (audio.cs:268 passes that argument).
    double f = audioVolume * 1.02;
    if (f < 0.0) { f = 0.0; }
    if (f >= 1.0) { f = 1.0; }
    const int wireDrive = static_cast<int>(255.0 * f
                                            * static_cast<double>(swrProtect));

    // IQ scalar — Audio.RadioVolume * Audio.HighSWRScale, with
    // HighSWRScale = 1.0 (baseline Thetis).  No SWR factor.
    const double iqGain = audioVolume;

    if (m_txChannel) {
        m_txChannel->setTxFixedGain(iqGain);
    }
    auto* conn = m_connection;
    QMetaObject::invokeMethod(conn, [conn, wireDrive]() {
        conn->setTxDrive(wireDrive);
    });
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

        // #202 deep-fix: propagate TUNE state to TransmitModel so its
        // m_tune flag (read by SetPowerUsingTargetDBM at TransmitModel.cpp:935-945)
        // tracks Thetis's `chkTUN.Checked` semantic.  Without this, a
        // power-slider movement during active TUNE would route through
        // setPowerUsingTargetDbm's txMode-0 (drive-slider) branch instead
        // of staying on the tune-power source — sending the wrong drive
        // byte mid-TUN.  Mirrors Thetis console.cs:46665 [v2.10.3.13]
        // which reads `chkTUN.Checked` directly.
        m_transmitModel.setTune(true);

        // Issue #177 — cancel any pending TUN-off completion.  If the user
        // double-clicks TUN (off → on within the rxReady + 100 ms settle
        // window) we are mid-walk: the rxReady slot has not yet fired or has
        // scheduled a singleShot that has not fired.  Clearing this flag
        // makes both the rxReady slot and the QTimer body no-op when they
        // run, leaving this fresh TUN-on path as the sole authority on saved
        // state and MOX engagement.
        m_pendingTuneOff = false;

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
        SliceModel* const txSlice = txBoundSlice();
        m_savedTxDspSliceId = txSlice ? txSlice->sliceIndex() : -1;
        m_savedTxDspMode = txSlice ? txSlice->dspMode() : DSPMode::USB;

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
        //
        // Tone gen runs by default on TUN-on; the new_pwr==0 path below
        // (after setPowerUsingTargetDbm) flips TXPostGenRun back to 0 if the
        // resolved tune power happens to be zero, mirroring Thetis
        // console.cs:46749-46758 [v2.10.3.13]:
        //   if (new_pwr == 0) {
        //       Audio.RadioVolume = 0.0;
        //       if (chkTUN.Checked) radio.GetDSPTX(0).TXPostGenRun = 0;
        //   } else {
        //       if (chkTUN.Checked) radio.GetDSPTX(0).TXPostGenRun = 1;
        //       Audio.RadioVolume = ...;
        //   }
        if (m_txChannel) {
            m_txChannel->setTuneTone(true, signedFreq, TxChannel::kMaxToneMag);
        }

        // ── CW→LSB/USB DSP MODE SWAP ───────────────────────────────────────────
        // Cite: console.cs:30043-30070 [v2.10.3.13]:
        //   switch (old_dsp_mode) { case CWL: ... TXDSPMode = LSB; break;
        //                            case CWU: ... TXDSPMode = USB; break; }
        if (txSlice) {
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
                txSlice->setDspMode(swappedMode);
            }
        }

        // ── PUSH TUNE POWER ────────────────────────────────────────────────────
        // Cite: console.cs:30033-30037 [v2.10.3.13]:
        //   PreviousPWR = ptbPWR.Value;  //MW0LGE_22b
        //   int new_pwr = SetPowerUsingTargetDBM(..., true, true, false);
        //   if (_tuneDrivePowerSource == DrivePowerSource.FIXED) PWR = new_pwr;
        //
        // Phase 4 Agent 4A of issue #167 (K2GX safety hotfix) — routed
        // through TransmitModel::setPowerUsingTargetDbm (Phase 3C deep-
        // parity wrapper).  bFromTune=true selects txMode 1 inside the
        // wrapper; the wrapper resolves the slider-source enum
        // (DriveSlider / TuneSlider / Fixed) per Thetis console.cs:46679-
        // 46692 [v2.10.3.13].
        //
        // Wire-byte vs IQ-scalar topology (matches drive-slider lambda
        // above):
        //   wire_byte = clamp(int(audio_volume * 1.02 * 255), 0, 255)
        //               From Thetis audio.cs:262-271 [v2.10.3.13]. NO SWR.
        //   iq_gain   = audio_volume * swrProtect
        //               From Thetis cmaster.cs:1115-1119 [v2.10.3.13].
        // Upstream tags preserved: //MW0LGE (from cited cmaster.cs:1114) [v2.10.3.15]
        //               SWR factor lives HERE — DO NOT add to wire byte.
        //
        // Pre-hotfix linear formula at this site:
        //   wire = clamp(int(255 * tunePower/100 * swrProtect), 0, 255)
        // shipped K2GX's >300W on 200W radio.  This rewrite is the
        // K2GX safety fix proper.
        const Band currentBand = txSlice
                                    ? bandFromFrequency(txSlice->frequency())
                                    : m_lastBand;

        // tunePower retained as a local for the SwrProtectionController
        // setters below — those setters drive the tune-bypass / alex_fwd
        // floor based on the SLIDER value (Thetis console.cs:26020-26067
        // [v2.10.3.13] reads ptbPWR.Value, not the post-PA-gain
        // audio_volume).  The wire byte itself goes through the dBm
        // wrapper; the SWR controller stays slider-driven per upstream.
        const int tunePower = m_transmitModel.tunePowerForBand(currentBand);

        if (m_paProfileManager) {
            const PaProfile* activeProfile = m_paProfileManager->activeProfile();
            if (activeProfile) {
                // Issue #175 Task 4: thread connected model so HL2
                // sub-step DSP audio-gain modulation engages on the TUN
                // path (mi0bot console.cs:47660-47673 [v2.10.3.13-beta2]).
                //
                // setPowerUsingTargetDbm emits audioVolumeChanged at
                // TransmitModel.cpp:1129; the listener wired in the
                // constructor (RadioModel::pumpAudioVolume) composes the
                // wire byte + IQ scalar Thetis-faithfully and pushes them.
                const auto result = m_transmitModel.setPowerUsingTargetDbm(
                    *activeProfile, currentBand, /*bSetPower=*/true,
                    /*bFromTune=*/true, /*bTwoTone=*/false,
                    m_hardwareProfile.model);

                // #202 deep-fix: TXPostGenRun=0 case for new_pwr==0 during TUNE.
                // Mirrors Thetis console.cs:46749-46752 [v2.10.3.13]:
                //   if (new_pwr == 0) {
                //       Audio.RadioVolume = 0.0;
                //       if (chkTUN.Checked) radio.GetDSPTX(0).TXPostGenRun = 0;
                //   }
                // setTuneTone(false, ...) maps to TXPostGenRun=0 in NereusSDR's
                // TxChannel wrapper (sets the run flag while leaving freq/mag).
                if (result.newPower == 0 && m_txChannel) {
                    m_txChannel->setTuneTone(false, signedFreq,
                                             TxChannel::kMaxToneMag);
                }
            }
            // No active profile loaded -> silently no-op the TUNE power
            // push.  The downstream MoxController / setTuneTone path still
            // engages MOX + tone, but no drive byte is sent.  Safer than
            // sending stale wire bytes from a previous radio's profile.
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
        // The dial comes from the TX-bound slice, not the active one: TUNE
        // keys the PA, so it must key on the frequency the transmitter is
        // actually bound to. Reading the active slice here would put the
        // tune carrier on whichever slice the operator was looking at.
        SliceModel* const tuneSlice = txBoundSlice();
        if (tuneSlice && m_connection) {
            const quint64 dialHz =
                static_cast<quint64>(tuneSlice->frequency());
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

        // Issue #177 fix — Thetis-faithful TUN-off ordering.
        //
        // From Thetis console.cs:30106-30109 [v2.10.3.13]:
        //   chkMOX.Checked = false;        // synchronous walk TX→RX (~30 ms)
        //   await Task.Delay(100);
        //   radio.GetDSPTX(0).TXPostGenRun = 0;
        //
        // Thetis's chkMOX setter blocks the UI thread inside chkMOX_CheckedChanged2
        // through Thread.Sleep(mox_delay=10) + Thread.Sleep(ptt_out_delay=20),
        // then waits an additional 100 ms before turning gen1 off.  By the time
        // gen1.run is set to 0, the WDSP TX channel has already been disabled
        // (line 29607) and is no longer producing samples — so the hard step at
        // gen1's output never enters a running TXA chain and there is no
        // filter-ringing transient.
        //
        // NereusSDR's MoxController is timer-based (non-blocking).  We latch
        // m_pendingTuneOff and let the rxReady → settle-timer slot wired in
        // the constructor invoke completeTuneOff() at T+30+m_tuneOffSettleMs ms.
        // Until then, the rest of the TUN-off work (gen1 off, mode restore,
        // power restore, VFO un-offset) is deferred.
        m_pendingTuneOff = true;

        // #202 deep-fix: clear TransmitModel's m_tune flag — symmetric with
        // setTune(true) in the TUN-on branch.  Mirrors Thetis user-click
        // semantic at console.cs:30106 [v2.10.3.13]: chkTUN.Checked = false
        // is the user intent that the TUN-off branch responds to.  Cleared
        // here (synchronously at user click) rather than inside
        // completeTuneOff (deferred ~130 ms later) so a power-slider event
        // arriving in the gap correctly routes through txMode-0 (drive-
        // slider) rather than txMode-1 (TUNE).
        m_transmitModel.setTune(false);

        // Capture MOX state BEFORE calling MoxController::setTune so we can
        // detect the "MOX already RX" path that would otherwise strand the
        // latch.  Codex P1 catch on PR #180: setMox(false)'s idempotent guard
        // (MoxController.cpp:461) emits no TX→RX phase signals when m_mox is
        // already false, so no rxReady fires and the deferred completion
        // never runs — m_pendingTuneOff sits latched, and a later unrelated
        // rxReady (from a normal PTT cycle) consumes the stale latch.
        //
        // This mirrors Thetis exactly. In Thetis the post-MOX work runs
        // unconditionally because `await Task.Delay(100)` at console.cs:30107
        // [v2.10.3.13] lives in the TUN handler — not in chkMOX_CheckedChanged2
        // — so it fires regardless of whether the chkMOX assignment triggered
        // a walk.  WinForms silently no-ops `chkMOX.Checked = false` when it
        // is already false, but the next line in chkTUN_CheckedChanged still
        // awaits 100 ms and then sets gen1.run = 0.
        //
        // Bug window for this guard: something has to drop MOX externally
        // while m_isTuning is still latched (e.g. PA-fault trip dropping
        // MOX, manual MOX click during TUN, or future PureSignal /
        // SwrProtectionController force-unkey paths).  Narrow but real.
        const bool moxWasOn = (m_moxController != nullptr)
                              && m_moxController->isMox();

        // ── RELEASE MOX via MoxController ────────────────────────────────────
        // Cite: console.cs:30106 [v2.10.3.13]: chkMOX.Checked = false;
        // MoxController::setTune(false) drives the full TX→RX walk (B.5)
        // when MOX is on: it fires hardwareFlipped(false) synchronously and
        // then chains keyUpDelayTimer (mox_delay) → txaFlushed →
        // pttOutDelayTimer (ptt_out_delay) → rxReady.  Always called (even
        // when MOX is already off) because it also clears m_manualMox and
        // emits manualMoxChanged(false) — Cite: console.cs:30142 [v2.10.3.13].
        if (m_moxController) {
            m_moxController->setTune(false);
        }

        if (!moxWasOn) {
            // No TX→RX walk will fire because MoxController::setMox(false)
            // hit its idempotent guard.  Schedule completeTuneOff directly
            // off a QTimer::singleShot so the deferred path still gets a
            // turn.  The settle delay matches m_tuneOffSettleMs both for
            // ordering symmetry with the walk path and because Thetis's
            // `await Task.Delay(100)` (console.cs:30107 [v2.10.3.13]) is
            // unconditional — it fires whether or not the chkMOX assignment
            // triggered a walk.  The lambda re-checks the latch in case a
            // fresh setTune(true) clears it before the timer fires.
            QTimer::singleShot(m_tuneOffSettleMs, this, [this]() {
                if (!m_pendingTuneOff) {
                    return;
                }
                completeTuneOff();
            });
        }

        // The remainder of the TUN-off work runs from completeTuneOff()
        // when MoxController::rxReady fires + m_tuneOffSettleMs elapses
        // (walk path), or directly from the singleShot above (no-walk path).
        // Wired in the RadioModel constructor next to F.1.
    }
}

// ---------------------------------------------------------------------------
// ── Phase 3J-1 follow-up: TCI Q_INVOKABLE shims (bench wire-up) ──────────────
//
// These methods are invoked by name from src/core/TciProtocol.cpp via
// QMetaObject::invokeMethod(...) when WSJT-X / ESDR3 / SunSDR clients drive
// the TCI server.  Phase 6 wired the call sites against TestMockRadioModel
// (which has matching Q_INVOKABLE methods); these production shims close the
// gap so real clients actuate the radio.
//
// Scope (WSJT-X minimum): PTT (trx), VFO (vfo), mode (modulation),
// split_enable.  Long tail (DSP toggles, AGC, SQL, RIT/XIT, balance, audio
// stream config, calibration) lands in a separate follow-up commit.
// ---------------------------------------------------------------------------

void RadioModel::setMox(bool on)
{
    // Route through MoxController when installed — that path enforces the
    // BandPlanGuard MoxCheck callback, fans out hardwareFlipped, and runs the
    // Codex P2 safety-effects-before-idempotent-guard ordering.  Without a
    // controller we fall back to the TransmitModel latch (matches the
    // pre-controller path Thetis uses during early construction).
    if (m_moxController) {
        m_moxController->setMox(on);
    } else {
        m_transmitModel.setMox(on);
    }
}

bool RadioModel::mox() const
{
    if (m_moxController) {
        return m_moxController->isMox();
    }
    return m_transmitModel.isMox();
}

void RadioModel::setVfoHz(int rx, int chan, qint64 hz)
{
    // NereusSDR has one frequency per slice.  VFO B (chan==1) maps to a
    // separate slice in this model, so per-slice VFO B writes are silently
    // ignored — TCI clients that drive VFO B should target a second slice.
    if (chan != 0) {
        return;
    }
    SliceModel* slice = sliceById(rx);
    if (!slice) {
        return;
    }
    slice->setFrequency(static_cast<double>(hz));
}

qint64 RadioModel::vfoHz(int rx, int chan) const
{
    // Both chan==0 and chan==1 return the slice frequency.  See setVfoHz note
    // — VFO B per slice is not modeled, so reads return the same value.
    (void)chan;
    const SliceModel* slice = sliceById(rx);
    if (!slice) {
        return 0;
    }
    return static_cast<qint64>(slice->frequency());
}

void RadioModel::setMode(int rx, QString modeStr)
{
    SliceModel* slice = sliceById(rx);
    if (!slice) {
        return;
    }
    const DSPMode mode = SliceModel::modeFromName(modeStr);
    slice->setDspMode(mode);
}

QString RadioModel::mode(int rx) const
{
    const SliceModel* slice = sliceById(rx);
    if (!slice) {
        return QString();
    }
    return SliceModel::modeName(slice->dspMode());
}

bool RadioModel::split(int rx) const
{
    // Phase 3F deletes the `setSplit` stub per design §3: split is replaced
    // with XIT for plus or minus 10 kHz tuning offset, or addSliceOnPan to
    // create a second slice for full retune. The query stays at false so
    // TciProtocol's init burst can still emit `split_enable:rx,false;` for
    // wire-protocol stability with WSJT-X / N1MM / Log4OM ("Split Operation:
    // None/Fake It").  See
    // docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
    // section 3 ("VFO A/B / split: not implemented").
    (void)rx;
    return false;
}

// ---------------------------------------------------------------------------
// Phase 3J-1 closeout Item 3 (2026-05-12): TCI Q_INVOKABLE long tail.
//
// Each shim routes a TciProtocol::invokeMethod call to the right model
// state.  Most are 1:1 with a SliceModel Q_PROPERTY (locked/muted/etc.);
// some are radio-global (RIT/XIT/AfLinear/etc.); a handful are stubs that
// store-and-return until their underlying feature lands (rxBin/rxApf/etc.).
//
// All slice-indexed shims sanity-check sliceById(rx) and silently no-op on
// out-of-range so a misbehaving client can't crash NereusSDR.  Getters
// return sensible defaults (false / 0 / "" / 0.0) when the slice doesn't
// exist, matching the TestMockRadioModel convention.
// ---------------------------------------------------------------------------

// ── VFO Lock ────────────────────────────────────────────────────────────────
void RadioModel::setVfoLock(int rx, int chan, bool locked)
{
    (void)chan;  // NereusSDR collapses VFOALock/VFOBLock to slice-level locked
    if (auto* s = sliceById(rx)) { s->setLocked(locked); }
}
bool RadioModel::vfoLock(int rx, int chan) const
{
    (void)chan;
    if (const auto* s = sliceById(rx)) { return s->locked(); }
    return false;
}
void RadioModel::setLock(int rx, bool locked)
{
    if (auto* s = sliceById(rx)) { s->setLocked(locked); }
}
bool RadioModel::lock(int rx) const
{
    if (const auto* s = sliceById(rx)) { return s->locked(); }
    return false;
}

// ── Mute ────────────────────────────────────────────────────────────────────
void RadioModel::setGlobalMute(bool on) { m_tciGlobalMute = on; }
bool RadioModel::globalMute() const     { return m_tciGlobalMute; }
void RadioModel::setRxMute(int rx, bool on)
{
    if (auto* s = sliceById(rx)) { s->setMuted(on); }
}
bool RadioModel::rxMute(int rx) const
{
    if (const auto* s = sliceById(rx)) { return s->muted(); }
    return false;
}

// ── Filter ──────────────────────────────────────────────────────────────────
//
// Review P2 #4 fix (2026-05-22): use the atomic SliceModel::setFilter(low,
// high) instead of the separate setFilterLow then setFilterHigh calls.  The
// split path emitted filterChanged twice -- once with (newLow, oldHigh) and
// again with (newLow, newHigh) -- so TCI clients tracking
// SliceModel::filterChanged via the local-broadcast path received a stale
// intermediate frame like "rx_filter_band:0,400,<oldHigh>;" before the
// final value.  Atomic setFilter emits filterChanged exactly once with the
// final pair, matching Thetis FilterChangedHandlers semantics
// (TCIServer.cs:6732 [v2.10.3.15] -- single OnFilterChanged event per
// FilterChanged delegate fire).
void RadioModel::setFilterBand(int rx, int lowHz, int highHz)
{
    if (auto* s = sliceById(rx)) {
        s->setFilter(lowHz, highHz);
    }
}
int RadioModel::filterLow(int rx) const
{
    if (const auto* s = sliceById(rx)) { return s->filterLow(); }
    return 0;
}
int RadioModel::filterHigh(int rx) const
{
    if (const auto* s = sliceById(rx)) { return s->filterHigh(); }
    return 0;
}

// ── AGC mode ────────────────────────────────────────────────────────────────
void RadioModel::setAgcMode(int rx, const QString& mode)
{
    auto* s = sliceById(rx);
    if (!s) { return; }
    const QString upper = mode.toUpper();
    AGCMode m = AGCMode::Med;
    if      (upper == QLatin1String("OFF"))    { m = AGCMode::Off;    }
    else if (upper == QLatin1String("LONG"))   { m = AGCMode::Long;   }
    else if (upper == QLatin1String("SLOW"))   { m = AGCMode::Slow;   }
    else if (upper == QLatin1String("MED")
          || upper == QLatin1String("MEDIUM")) { m = AGCMode::Med;    }
    else if (upper == QLatin1String("FAST"))   { m = AGCMode::Fast;   }
    else if (upper == QLatin1String("CUSTOM")) { m = AGCMode::Custom; }
    s->setAgcMode(m);
}
QString RadioModel::agcMode(int rx) const
{
    const auto* s = sliceById(rx);
    if (!s) { return QString(); }
    switch (s->agcMode()) {
        case AGCMode::Off:    return QStringLiteral("OFF");
        case AGCMode::Long:   return QStringLiteral("LONG");
        case AGCMode::Slow:   return QStringLiteral("SLOW");
        case AGCMode::Med:    return QStringLiteral("MED");
        case AGCMode::Fast:   return QStringLiteral("FAST");
        case AGCMode::Custom: return QStringLiteral("CUSTOM");
    }
    return QStringLiteral("MED");
}

// ── AGC gain (threshold) ────────────────────────────────────────────────────
void RadioModel::setAgcGain(int rx, int gain)
{
    if (auto* s = sliceById(rx)) { s->setAgcThreshold(gain); }
}
int RadioModel::agcGain(int rx) const
{
    if (const auto* s = sliceById(rx)) { return s->agcThreshold(); }
    return 0;
}

// ── Squelch ─────────────────────────────────────────────────────────────────
void RadioModel::setSqlEnable(int rx, bool on)
{
    if (auto* s = sliceById(rx)) { s->setSsqlEnabled(on); }
}
bool RadioModel::sqlEnable(int rx) const
{
    if (const auto* s = sliceById(rx)) { return s->ssqlEnabled(); }
    return false;
}
void RadioModel::setSqlLevel(int rx, int level)
{
    if (auto* s = sliceById(rx)) { s->setSsqlThresh(static_cast<double>(level)); }
}
int RadioModel::sqlLevel(int rx) const
{
    if (const auto* s = sliceById(rx)) {
        return static_cast<int>(s->ssqlThresh());
    }
    return 0;
}

// ── RIT / XIT (active slice) ────────────────────────────────────────────────
void RadioModel::setRitEnable(bool on)
{
    if (auto* s = activeSlice()) { s->setRitEnabled(on); }
}
bool RadioModel::ritEnable() const
{
    if (const auto* s = activeSlice()) { return s->ritEnabled(); }
    return false;
}
void RadioModel::setRitOffset(int hz)
{
    if (auto* s = activeSlice()) { s->setRitHz(hz); }
}
int RadioModel::ritOffset() const
{
    if (const auto* s = activeSlice()) { return s->ritHz(); }
    return 0;
}
void RadioModel::setXitEnable(bool on)
{
    if (auto* s = activeSlice()) { s->setXitEnabled(on); }
}
bool RadioModel::xitEnable() const
{
    if (const auto* s = activeSlice()) { return s->xitEnabled(); }
    return false;
}
void RadioModel::setXitOffset(int hz)
{
    if (auto* s = activeSlice()) { s->setXitHz(hz); }
}
int RadioModel::xitOffset() const
{
    if (const auto* s = activeSlice()) { return s->xitHz(); }
    return 0;
}

// ── RX balance / audio pan ──────────────────────────────────────────────────
void RadioModel::setRxBalance(int rx, int chan, double balance)
{
    (void)chan;
    if (auto* s = sliceById(rx)) { s->setAudioPan(balance); }
}
double RadioModel::rxBalance(int rx, int chan) const
{
    (void)chan;
    if (const auto* s = sliceById(rx)) { return s->audioPan(); }
    return 0.0;
}

// ── CTUN (stub until model lands) ───────────────────────────────────────────
void RadioModel::setRxCtun(int rx, bool on)
{
    if (rx >= 0 && rx < kTciStubSliceMax) { m_tciStubRxCtun[rx] = on; }
}
bool RadioModel::rxCtun(int rx) const
{
    if (rx >= 0 && rx < kTciStubSliceMax) { return m_tciStubRxCtun[rx]; }
    return false;
}

// ── NB / NR / ANF ───────────────────────────────────────────────────────────
void RadioModel::setRxNb(int rx, bool on)
{
    if (auto* s = sliceById(rx)) {
        s->setNbMode(on ? NbMode::NB : NbMode::Off);
    }
}
bool RadioModel::rxNb(int rx) const
{
    if (const auto* s = sliceById(rx)) { return s->nbMode() != NbMode::Off; }
    return false;
}
void RadioModel::setRxNr(int rx, bool on, int nrIndex)
{
    auto* s = sliceById(rx);
    if (!s) { return; }
    if (!on) {
        s->setActiveNr(NrSlot::Off);
        return;
    }
    NrSlot slot = NrSlot::NR1;
    switch (nrIndex) {
        case 0: slot = NrSlot::NR1;  break;
        case 1: slot = NrSlot::NR2;  break;
        case 2: slot = NrSlot::NR3;  break;
        case 3: slot = NrSlot::NR4;  break;
        case 4: slot = NrSlot::DFNR; break;
        case 5: slot = NrSlot::BNR;  break;
        case 6: slot = NrSlot::MNR;  break;
        default: slot = NrSlot::NR1; break;
    }
    s->setActiveNr(slot);
}
bool RadioModel::rxNr(int rx) const
{
    if (const auto* s = sliceById(rx)) { return s->activeNr() != NrSlot::Off; }
    return false;
}
int RadioModel::rxNrIndex(int rx) const
{
    if (const auto* s = sliceById(rx)) {
        switch (s->activeNr()) {
            case NrSlot::Off:  return 0;
            case NrSlot::NR1:  return 0;
            case NrSlot::NR2:  return 1;
            case NrSlot::NR3:  return 2;
            case NrSlot::NR4:  return 3;
            case NrSlot::DFNR: return 4;
            case NrSlot::BNR:  return 5;
            case NrSlot::MNR:  return 6;
        }
    }
    return 0;
}
// ANF: Thetis's auto-notch is a WDSP RXA stage independent of the NR slot
// system.  Phase 3F Sub-Epic J Task 1 gave it its own SliceModel Q_PROPERTY
// (anfEnabled / setAnfEnabled / anfEnabledChanged), already wired to
// RxChannel::setAnfEnabled in the per-slice connect block above.  This shim
// used to store ANF state in m_tciStubRxApf -- the APF stub array -- so a
// TCI client's rx_anf_enable set (a) silently flipped APF's stored bit too
// (rxAnf(rx) and rxApf(rx) read the identical bool) and (b) never touched
// real WDSP ANF at all, since nothing read m_tciStubRxApf back out into the
// DSP chain.  Routed through sliceById(rx) now, matching every other
// per-slice shim in this section.  Phase 3F chip task_c1e6fbad finished the
// job: APF and BIN moved to their own SliceModel properties too and the stub
// arrays are gone, so nothing aliases anything here any more.
void RadioModel::setRxAnf(int rx, bool on)
{
    if (auto* s = sliceById(rx)) { s->setAnfEnabled(on); }
}
bool RadioModel::rxAnf(int rx) const
{
    if (const auto* s = sliceById(rx)) { return s->anfEnabled(); }
    return false;
}

// BIN: Thetis's binaural toggle is per receiver, handleRxBinEnable at
// TCIServer.cs:1854-1869 [v2.10.3.15] -- consoleThreadSafe.SetBin(rx + 1,
// enabled) on the set path, GetBin(rx + 1) on the query path.  NereusSDR's
// analog is SliceModel::binauralEnabled, which RadioModel already wires to
// RxChannel::setBinauralEnabled in the per-slice connect block above.
//
// Phase 3F chip task_c1e6fbad: this pair used to store into m_tciStubRxBin
// and read straight back out, so a TCI client set BIN, was told it took
// effect, and nothing in the DSP chain moved.  Same defect the ANF shim had
// before Task 10, and the same fix: route through sliceById(rx).
void RadioModel::setRxBin(int rx, bool on)
{
    if (auto* s = sliceById(rx)) { s->setBinauralEnabled(on); }
}
bool RadioModel::rxBin(int rx) const
{
    if (const auto* s = sliceById(rx)) { return s->binauralEnabled(); }
    return false;
}

// APF: also per receiver upstream, handleRxApfEnable at TCIServer.cs:1870-1894
// [v2.10.3.15] -- SetupForm.RX1APFEnable / RX2APFEnable.  Thetis holds the
// state on the Setup form and bails when that form is null; NereusSDR keeps it
// on the slice (SliceModel::apfEnabled, wired to RxChannel::setApfEnabled), so
// there is no equivalent null-form gate to port.  Same stub-to-real routing as
// BIN above.
void RadioModel::setRxApf(int rx, bool on)
{
    if (auto* s = sliceById(rx)) { s->setApfEnabled(on); }
}
bool RadioModel::rxApf(int rx) const
{
    if (const auto* s = sliceById(rx)) { return s->apfEnabled(); }
    return false;
}

// ── Stub DSP toggles (no model state yet) ───────────────────────────────────
//
// NF stays a stub, and deliberately so: upstream is asymmetric here.
// handleRxNfEnable (TCIServer.cs:1895-1910 [v2.10.3.15]) answers a query with
// the PER-RX notch state, consoleThreadSafe.GetMNF(rx + 1), but a set writes
// the RADIO-GLOBAL consoleThreadSafe.TNFActive = enabled.  NereusSDR has no
// MNF/TNF model to route either half to yet, and guessing which of the two
// semantics to adopt would bake in a choice the upstream itself does not make
// consistently.  Left storing and reading its own value so a round-trip still
// returns what the operator last set.
void RadioModel::setRxNf(int rx, bool on)
{
    if (rx >= 0 && rx < kTciStubSliceMax) { m_tciStubRxNf[rx] = on; }
}
bool RadioModel::rxNf(int rx) const
{
    if (rx >= 0 && rx < kTciStubSliceMax) { return m_tciStubRxNf[rx]; }
    return false;
}
void RadioModel::setRxEnable(int rx, bool on)
{
    if (rx >= 0 && rx < kTciStubSliceMax) { m_tciStubRxEnable[rx] = on; }
}
bool RadioModel::rxEnable(int rx) const
{
    if (rx >= 0 && rx < kTciStubSliceMax) { return m_tciStubRxEnable[rx]; }
    return false;
}

// ── Per-slice AF gain (rx_volume: query source) ─────────────────────────────
//
// Phase 3F Sub-Epic J Task 10: TCI receiver rx -> slice id rx via
// sliceById(rx), the same convention every other per-rx shim in this
// section already uses (setMode/mode, setFilterBand/filterLow, setAgcMode/
// agcMode, setRxNb/rxNb, etc.).  Falls back to the active slice when no
// slice with that id exists, rather than 0 (which a TCI client would read
// as "muted") or silently aliasing whatever sliceById(0) happens to return
// -- see TciProtocol.cpp's rx_volume block in buildInitialRadioStateLines
// for the full receiver -> slice id writeup and the Thetis citations this
// replaces.
int RadioModel::afGain(int rx) const
{
    const SliceModel* s = sliceById(rx);
    if (!s) {
        s = activeSlice();
    }
    if (!s) {
        return 0;
    }
    return s->afGain();
}

// ── Volume (radio-global) ───────────────────────────────────────────────────
//
// Review P1 #1 fix (2026-05-22): forward to live AudioEngine / TransmitModel
// state instead of the decoupled m_tciAfLinear / m_tciMonLinear caches.  The
// caches defaulted to 0, so a fresh real-client connect saw "volume:-60.0;"
// (muted) and "rx_volume:0,0,-60.00;" -- bench-bogus.  Thetis reads
// consoleThreadSafe.AF / TXAF for the same fields (TCIServer.cs:2652+2655
// [v2.10.3.15]), which are the actual UI slider positions.  NereusSDR's
// equivalents:
//   AF        -> AudioEngine::volume() (float [0..1]) scaled to int [0..100]
//   TXAF/MON  -> TransmitModel::monitorVolume() (float [0..1]) same scale
// The legacy m_tciAfLinear / m_tciMonLinear caches are still written by
// setAfLinear / setMonLinear so test code that pokes them keeps working,
// but they are NOT read back -- the live source is authoritative.  Setters
// also forward to the live model so TCI clients writing AF/MON actually
// affect the radio (parity with Thetis handleVolume / handleMONVolume).
void RadioModel::setAfLinear(int v)
{
    m_tciAfLinear = v;
    if (m_audioEngine) {
        m_audioEngine->setVolume(static_cast<float>(qBound(0, v, 100)) / 100.0f);
    }
}
int  RadioModel::afLinear() const
{
    if (m_audioEngine) {
        const float vol = m_audioEngine->volume();
        return qBound(0, static_cast<int>(vol * 100.0f + 0.5f), 100);
    }
    return m_tciAfLinear;
}
void RadioModel::setMonLinear(int v)
{
    m_tciMonLinear = v;
    m_transmitModel.setMonitorVolume(
        static_cast<float>(qBound(0, v, 100)) / 100.0f);
}
int  RadioModel::monLinear() const
{
    const float vol = m_transmitModel.monitorVolume();
    return qBound(0, static_cast<int>(vol * 100.0f + 0.5f), 100);
}

// ── IQ rate ─────────────────────────────────────────────────────────────────
//
// Review P1 #1 fix (2026-05-22): prefer the live connection sample rate.
// Thetis reads consoleThreadSafe.SampleRateRX1 via getPublishedIQSampleRate
// (TCIServer.cs:2642 [v2.10.3.15]).  NereusSDR exposes the same via
// connectionSampleRateHz().  Falls back to the cached m_tciIqSampleRate
// only when no connection is active (matches Thetis behaviour: pre-
// connect probes see whatever the user/setup defaults left in the field).
void RadioModel::setIqSampleRate(int sr) { m_tciIqSampleRate = sr; }
int  RadioModel::iqSampleRate() const
{
    const int liveRate = connectionSampleRateHz();
    if (liveRate > 0) { return liveRate; }
    return m_tciIqSampleRate;
}

// ── Audio stream config (parity-only; TciServer intercepts) ─────────────────
void RadioModel::setAudioSampleRate(int sr)          { m_tciAudioSampleRate = sr; }
int  RadioModel::audioSampleRate() const             { return m_tciAudioSampleRate; }
void RadioModel::setAudioStreamSampleType(const QString& t) { m_tciAudioStreamSampleType = t; }
QString RadioModel::audioStreamSampleType() const    { return m_tciAudioStreamSampleType; }
void RadioModel::setAudioStreamChannels(int n)       { m_tciAudioStreamChannels = n; }
int  RadioModel::audioStreamChannels() const         { return m_tciAudioStreamChannels; }
void RadioModel::setAudioStreamSamples(int n)        { m_tciAudioStreamSamples = n; }
int  RadioModel::audioStreamSamples() const          { return m_tciAudioStreamSamples; }

// ── TX profile (MicProfileManager) ──────────────────────────────────────────
// MicProfileManager::setActiveProfile takes (name, TransmitModel*) -- pass
// our owned m_transmitModel reference so the profile's settings actually
// fan out to the model + WDSP.
void RadioModel::setTxProfile(const QString& name)
{
    if (m_micProfileMgr) {
        m_micProfileMgr->setActiveProfile(name, &m_transmitModel);
    }
}
QString RadioModel::txProfile() const
{
    if (m_micProfileMgr) {
        return m_micProfileMgr->activeProfileName();
    }
    return QString();
}
QStringList RadioModel::txProfilesList() const
{
    if (m_micProfileMgr) {
        return m_micProfileMgr->profileNames();
    }
    return {};
}

// ── Calibration (getter-only stubs) ─────────────────────────────────────────
// No calibration model exists yet.  All getters return 0.0 = "no calibration
// applied".  Real implementation lands when CalibrationModel + per-slice
// persistence are added.
double RadioModel::calibrationMeter(int rx) const     { (void)rx; return 0.0; }
double RadioModel::calibrationDisplay(int rx) const   { (void)rx; return 0.0; }
double RadioModel::calibrationXvtr(int rx) const      { (void)rx; return 0.0; }
double RadioModel::calibrationSixMeter(int rx) const  { (void)rx; return 0.0; }
double RadioModel::calibrationTxDisplay(int rx) const { (void)rx; return 0.0; }

// ── Init-burst live-state shims (Phase 3J-1 closeout 2026-05-22) ────────────
// Documentation lives in RadioModel.h alongside the declarations; cite
// summaries inline here for diff-readability.

// rx2Enabled -- From Thetis RX2Enabled at console.cs:37278 [v2.10.3.15].
// Derived from m_connectionActiveRxCount; setActiveRxCountLive is the
// authoritative state writer.
bool RadioModel::rx2Enabled() const
{
    return m_connectionActiveRxCount >= 2;
}

// monEnabled -- From Thetis MON at console.cs:18656-18663 [v2.10.3.15].
// Forwards to TransmitModel::monEnabled (default false, never persisted).
bool RadioModel::monEnabled() const
{
    return m_transmitModel.monEnabled();
}

// tune -- From Thetis TUN at console.cs:18677-18684 [v2.10.3.15].
// Same backing field as the existing isTune() accessor; separate Q_INVOKABLE
// surface because isTune() is noexcept and cannot carry Q_INVOKABLE.
bool RadioModel::tune() const
{
    return m_isTuning;
}

// powerOn -- From Thetis PowerOn at console.cs:19799-19803 [v2.10.3.15].
// Architectural divergence: NereusSDR has no separate Power button, so
// connection IS power.  TCI clients see powerOn = isConnected().
bool RadioModel::powerOn() const
{
    return isConnected();
}

// diglOffset -- From Thetis DIGLClickTuneOffset at console.cs:14693
// [v2.10.3.15].  Architectural divergence: per-slice in NereusSDR; expose
// active slice value (falls back to 0 when no active slice -- e.g. pre-
// connect TCI client probe).
int RadioModel::diglOffset() const
{
    if (m_activeSlice) {
        return m_activeSlice->diglOffsetHz();
    }
    return 0;
}

// diguOffset -- From Thetis DIGUClickTuneOffset at console.cs:14658
// [v2.10.3.15].  Same divergence as diglOffset.
int RadioModel::diguOffset() const
{
    if (m_activeSlice) {
        return m_activeSlice->diguOffsetHz();
    }
    return 0;
}

// ---------------------------------------------------------------------------
// completeTuneOff — Thetis-faithful TUN-off completion (issue #177).
//
// Invoked from a QTimer::singleShot(m_tuneOffSettleMs) chained off
// MoxController::rxReady.  By this point the TX→RX walk has finished, the
// MOX wire bit is off, the WDSP TX channel has been drained and stopped
// (txaFlushed → setRunning(false)), and the radio's PA is no longer
// transmitting.  Cutting gen1 here cannot cause a filter-ringing transient
// to reach the wire because the TXA chain has stopped processing.
//
// Mirrors Thetis console.cs:30109-30134 [v2.10.3.13]:
//   radio.GetDSPTX(0).TXPostGenRun = 0;     // 30109 — gen1 OFF
//   ...
//   switch (old_dsp_mode) { case CWL/CWU: restore }   // 30113-30121
//   _tuning = false;                        // 30122
//   updateVFOFreqs(false, true);            // 30124 — un-offset TX VFO
//   if (_tuneDrivePowerSource == FIXED) PWR = PreviousPWR;   // 30130-30134
//   //MW0LGE_22b  [original inline comment from console.cs:30033]
//
// Idempotent: re-checks m_pendingTuneOff and bails if a fresh setTune(true)
// or a teardown has cleared it.  The constructor lambda also guards before
// dispatching here, but a defense-in-depth check makes the contract
// explicit at this entry point.
// ---------------------------------------------------------------------------
void RadioModel::completeTuneOff()
{
    if (!m_pendingTuneOff) {
        return;
    }
    m_pendingTuneOff = false;

    // ── RELEASE TUNE TONE ──────────────────────────────────────────────────
    // Cite: console.cs:30109 [v2.10.3.13]: radio.GetDSPTX(0).TXPostGenRun = 0;
    // The TX channel has already been stopped by F.1 txaFlushed → setRunning(false),
    // so this gen1 update lands on an idle TXA chain — no transient.
    if (m_txChannel) {
        m_txChannel->setTuneTone(false, 0.0, 0.0);
    }

    // ── RESTORE DSP MODE if swapped ────────────────────────────────────────
    // Cite: console.cs:30112-30122 [v2.10.3.13]:
    //   switch (old_dsp_mode) { case CWL: case CWU:
    //       radio.GetDSPTX(0).CurrentDSPMode = old_dsp_mode; ... }
    if (SliceModel* const savedTxSlice =
            sliceById(m_savedTxDspSliceId)) {
        const bool wasSwapped = (m_savedTxDspMode == DSPMode::CWL ||
                                 m_savedTxDspMode == DSPMode::CWU);
        if (wasSwapped) {
            savedTxSlice->setDspMode(m_savedTxDspMode);
        }
    }
    m_savedTxDspSliceId = -1;

    // ── RESTORE POWER ──────────────────────────────────────────────────────
    // Cite: console.cs:30129-30132 [v2.10.3.13]:
    //   if (_tuneDrivePowerSource == DrivePowerSource.FIXED) PWR = PreviousPWR;
    //   //MW0LGE_22b  [original inline comment from console.cs:30033]
    //
    // Codex P1 follow-up to PR #178 — route the restore through the
    // calibrated dBm path, NOT the old linear formula.  Previously
    // this site computed wire_byte = clamp(int(255 * pct/100 * swr),
    // 0, 255) and wrote it directly via setTxDrive(), which left the
    // radio holding a pre-hotfix linear byte after TUN-off.  In the
    // common flow "TUN on → TUN off → MOX without moving slider",
    // MOX would engage with that stale linear byte → K2GX-class
    // over-drive on high-gain PAs.
    //
    // Same composition as the drive-slider lambda + TUNE-on rewrite:
    //   wire_byte = clamp(int(audio_volume * 1.02 * 255), 0, 255)
    //               From audio.cs:262-271 [v2.10.3.13]; NO SWR factor.
    //   iq_gain   = audio_volume * swrProtect
    //               From cmaster.cs:1115-1119 [v2.10.3.13]; SWR HERE.
    // bFromTune=false routes through txMode 0 (drive-slider source)
    // since TUN is now off and the user's saved drive-slider value
    // is the canonical post-restore source.
    m_transmitModel.setPower(m_savedPowerPct);
    const SliceModel* const txSlice = txBoundSlice();
    const Band offBand = txSlice
                            ? bandFromFrequency(txSlice->frequency())
                            : m_lastBand;
    if (m_paProfileManager) {
        const PaProfile* activeProfile = m_paProfileManager->activeProfile();
        if (activeProfile) {
            // Issue #175 Task 4: thread connected model so the TUN-off
            // restore (txMode 0 path back to drive slider) is uniform
            // with the TUN-on path; non-HL2 SKUs unaffected.
            //
            // setPowerUsingTargetDbm emits audioVolumeChanged at
            // TransmitModel.cpp:1129; the listener wired in the
            // constructor (RadioModel::pumpAudioVolume) composes the wire
            // byte + IQ scalar Thetis-faithfully and pushes them.
            const auto result = m_transmitModel.setPowerUsingTargetDbm(
                *activeProfile, offBand, /*bSetPower=*/true,
                /*bFromTune=*/false, /*bTwoTone=*/false,
                m_hardwareProfile.model);
            (void)result;
        }
    }

    // ── RESTORE TX VFO (un-offset from cw_pitch) ───────────────────────────
    // Mirrors Thetis console.cs:31788-31810 [v2.10.3.13] which only
    // applies the ±cw_pitch tx_freq offset while chkTUN.Checked == true.
    // Once TUNE drops, txtVFOAFreq_LostFocus recomputes tx_freq without
    // the offset so the carrier returns to dial freq.
    // Same TX-bound source as the setTune arm above — the carrier returns to
    // the transmitter's own dial, not to whichever slice is on screen.
    if (SliceModel* const tuneSlice = txBoundSlice(); tuneSlice && m_connection) {
        const quint64 dialHz =
            static_cast<quint64>(tuneSlice->frequency());
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

void RadioModel::onMoxHardwareFlipped(bool isTx)
{
    // Step 1 — Alex antenna routing.  Resolves which TX/RX antenna ports
    // engage for the current band and tx/rx state.  AlexController state
    // is read inside applyAlexAntennaForBand; result is pushed to
    // m_connection->setAntennaRouting() internally.
    // Pre-code review §2.3 step 8 [v2.10.3.13].
    SliceModel* const txSlice = txBoundSlice();
    if (isTx && txSlice == nullptr) {
        return;
    }
    const Band band = txSlice
                        ? bandFromFrequency(txSlice->frequency())
                        : m_lastBand;
    if (isTx) {
        // Defensive authority reconciliation: a listening slice may have
        // changed its stored TX antenna before becoming TX-bound.
        applyTxAntennaFromBoundSlice();
    }
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
        if (isTx) {
            if (m_moxStoppedRxChannel < 0 && txSlice != nullptr) {
                const int channelId = txSlice->sliceIndex();
                if (auto* const rxCh = m_wdspEngine->rxChannel(channelId)) {
                    // RX off + flush. SetChannelState(id, 0, 1), matching
                    // Thetis console.cs:29534 [v2.10.3.13].
                    rxCh->setActive(false);
                    m_moxStoppedRxChannel = channelId;
                }
            }
        } else {
            const int channelId = m_moxStoppedRxChannel;
            m_moxStoppedRxChannel = -1;
            if (channelId >= 0) {
                if (auto* const rxCh = m_wdspEngine->rxChannel(channelId)) {
                    // RX on, no flush. SetChannelState(id, 1, 0), matching
                    // Thetis console.cs:29629 [v2.10.3.13].
                    rxCh->setActive(true);
                }
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

// ---------------------------------------------------------------------------
// setSampleRateLive — Task 1.6
//
// Sample-rate live-apply coordinator.  Implements the 7-step sequence
// described in the design doc (thetis-display-dsp-parity-design.md §5C).
//
// NereusSDR-original infrastructure — no Thetis source ported here.
// The P1 restart pattern mirrors the onReconnectTimeout() sequence in
// P1RadioConnection (itself ported from networkproto1.c SendStopToMetis /
// SendStartToMetis [v2.10.3.13]).
// ---------------------------------------------------------------------------
qint64 RadioModel::setSampleRateLive(int newRateHz,
                                     bool reconcileDiversity)
{
    QElapsedTimer t;
    t.start();

    // Idempotent check first — safe even when disconnected, avoids the
    // spurious warning log on redundant calls from the settings-restore path.
    if (newRateHz == m_connectionSampleRateHz) {
        return 0;
    }

    // Guard: nothing to do if disconnected or WDSP not initialized.
    if (!m_connection || !m_wdspEngine || !m_wdspEngine->isInitialized()) {
        qCWarning(lcConnection) << "setSampleRateLive: no active connection "
                                   "or WDSP not initialized — ignoring";
        return -1;
    }

    qCInfo(lcConnection) << "setSampleRateLive:" << m_connectionSampleRateHz
                         << "Hz ->" << newRateHz << "Hz";

    // External diversity owns reusable buffers and a WDSP slot sized to the
    // target channel's input geometry. Stop its raw-DDC feed before changing
    // that geometry; it is recreated on the control path below, so the DSP hot
    // loop never has to allocate in response to a rate mismatch.
    const bool restartExternalDiversity =
        m_externalDiversityRouteActive;
    if (restartExternalDiversity) {
        stopExternalDiversityRoute();
    }

    // Source-first port of Thetis setup.cs::comboAudioSampleRate1_SelectedIndexChanged
    // [v2.10.3.13:7003-7159].  The Thetis path mutates the running WDSP
    // channel via cmaster.SetXcmInrate (cmaster.c:453-507) — the channel
    // object stays alive across the call.  This replaces the post-v0.3.2
    // destroy-and-recreate path that invalidated 7+ raw-pointer holders
    // (RadioModel::m_txChannel, TxWorkerThread, PureSignal, MeterPoller,
    // TwoToneController, TxCfcDialog, TxChannel::s_voxKeyInstance) and
    // crashed when step 7 moved the dangling m_txChannel back to its
    // worker thread.  TX channel is intentionally untouched here:
    // audio.cs::SampleRate1 setter [v2.10.3.13:637-649] only calls
    // SetXcmInrate(0, ...) for RX1 and SetXcmInrate(1, ...) for RX2;
    // SampleRateTX setter (lines 663-672) does NOT call SetXcmInrate.

    const int newInSize = bufferSizeForRate(newRateHz);

    // ── Step 1: Drain the RX channel ──────────────────────────────────────
    // Thetis setup.cs:7010 / 7081 [v2.10.3.13]: SetChannelState(id, 0, 1)
    // — off + drain to flush the slew envelope cleanly.  RxChannel is owned
    // by WdspEngine; look it up by channel ID rather than caching a raw
    // pointer (the previous pattern's failure mode is exactly what this
    // fix replaces).
    RxChannel* rxCh = m_wdspEngine->rxChannel(0);
    if (rxCh && rxCh->isActive()) {
        rxCh->setActive(false);
    }
    QThread::msleep(10);  // setup.cs:7011 / 7082 [v2.10.3.13]: Thread.Sleep(10)

    // ── Step 2: Quiesce DSP worker ────────────────────────────────────────
    // Disconnect the I/Q feed so no new batches land while the WDSP channel
    // is being reconfigured.  resetAccumulator() via BlockingQueuedConnection
    // ensures any in-flight batch completes before we proceed.
    if (m_dspWorker && m_receiverManager) {
        QObject::disconnect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
                            m_dspWorker, &RxDspWorker::processIqBatch);
        if (m_dspThread && m_dspThread->isRunning()) {
            QMetaObject::invokeMethod(m_dspWorker,
                                      &RxDspWorker::resetAccumulator,
                                      Qt::BlockingQueuedConnection);
        }
    }

    // ── Step 3: Pause AudioEngine ─────────────────────────────────────────
    m_audioEngine->pauseInput();

    // ── Step 4: Stop radio data flow ──────────────────────────────────────
    // Thetis setup.cs:7020-7022 (P2 EnableRx) / 7092 (P1 SendStopToMetis)
    // [v2.10.3.13].  In NereusSDR the stop+set-rate+start cycle is wrapped
    // by P1RadioConnection::restartStreamWithRate (P1) or atomic-rate-update
    // inside RadioConnection::setSampleRate (P2).  Both paths are queued
    // to the connection thread; we wait below for inflight packets to drain.
    if (auto* p1 = qobject_cast<P1RadioConnection*>(m_connection)) {
        QMetaObject::invokeMethod(p1, [p1, newRateHz]() {
            p1->restartStreamWithRate(newRateHz);
        }, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(m_connection,
                                  [conn = m_connection, newRateHz]() {
            conn->setSampleRate(newRateHz);
        }, Qt::QueuedConnection);
    }

    // ── Step 5: Wait for inflight I/Q packets to clear ─────────────────────
    // Thetis setup.cs:7025 / 7095 [v2.10.3.13]:
    //   Thread.Sleep(20);   // P2 (ETH)
    //   Thread.Sleep(25);   // P1 (USB)
    QThread::msleep(25);  // P1 conservative bound covers both protocols

    // ── Step 6: Update the live WDSP channel rate ─────────────────────────
    // Thetis cmaster.c:473-474 [v2.10.3.13] via WdspEngine::setRxChannelRate.
    // No destroy-and-recreate — the RxChannel C++ wrapper stays alive,
    // m_rxChannel raw pointer (and every other holder) remains valid.
    m_wdspEngine->setRxChannelRate(0, newRateHz);

    // Phase 3F Sub-Epic I closeout, defect H1: every slice's channel, not
    // just channel 0. This is a radio-wide rate, and step 7 below gives the
    // whole worker one drain size, so a channel left at the old rate would be
    // handed a chunk sized for the new one. Upstream loops the same way:
    //   From Thetis cmaster.c:473-475 [v2.10.3.15]
    //     for (i = 0; i < pcm->cmSubRCVR; i++) {
    //         SetInputSamplerate (chid (in_id, i), rate);
    //         SetInputBuffsize (chid (in_id, i), pcm->xcm_insize[in_id]);
    //     }
    // Idempotent for channel 0, which the line above already moved.
    for (SliceModel* s : std::as_const(m_slices)) {
        if (s) {
            m_wdspEngine->setRxChannelRate(s->sliceIndex(), newRateHz);
        }
    }

    // ── Step 7: Reconfigure AudioEngine and DSP worker for new rate ───────
    // WDSP always outputs 64 samples @ 48 kHz; AudioEngine's speakers bus
    // doesn't need reopening but the input geometry follows the wire rate.
    m_audioEngine->reinitForSampleRate(newRateHz);
    if (m_dspWorker) {
        m_dspWorker->setBufferSizes(newInSize, 64);
        // Phase 3F Sub-Epic I closeout, defect H1: this is the radio-wide
        // control, so it resets every stream's width. Per-stream overrides
        // were published against the rate that just went away; leaving them
        // would keep a stream draining a chunk size no channel is configured
        // for any more. Queued to land on the DSP thread, and posted while
        // the feed is still disconnected (step 2) so it is consumed before
        // the first batch that follows the reconnect in step 10.
        QMetaObject::invokeMethod(m_dspWorker,
                                  &RxDspWorker::clearStreamInputChunks,
                                  Qt::QueuedConnection);
    }

    // Keep the allocator and the published-size record agreeing with what was
    // just pushed. Without this the next retune's applyStreamDspGeometry would
    // read the allocator's stale per-stream rates and drag every channel back
    // to the rate this call just left.
    for (int st = 0; st < m_streamAllocator.streamCount(); ++st) {
        if (m_streamAllocator.isStreamActive(st)) {
            m_streamAllocator.activateStream(
                st, m_streamAllocator.streamCentreHz(st), newRateHz);
        }
        m_streamInSizePushed.insert(st, newInSize);
    }
    for (SliceModel* s : std::as_const(m_slices)) {
        if (s && s->streamIndex() >= 0) {
            s->setSampleRateHz(newRateHz);
        }
    }

    // ── Step 8: Brief wait for samples at the new rate to arrive ─────────
    // Thetis setup.cs:7046 / 7129 [v2.10.3.13]:
    //   Thread.Sleep(1);  // P2
    //   Thread.Sleep(5);  // P1
    QThread::msleep(5);

    // ── Step 9: Re-enable the RX channel ─────────────────────────────────
    // Thetis setup.cs:7056 / 7141 [v2.10.3.13]: SetChannelState(id, 1, 0).
    // Re-look-up rather than reuse rxCh in case the engine state shifted.
    if (RxChannel* rx = m_wdspEngine->rxChannel(0)) {
        rx->setActive(true);
    }

    // ── Step 10: Reconnect I/Q feed ──────────────────────────────────────
    if (m_dspWorker && m_receiverManager) {
        connect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
                m_dspWorker, &RxDspWorker::processIqBatch,
                Qt::QueuedConnection);
    }

    // ── Step 11: Resume audio ────────────────────────────────────────────
    m_audioEngine->resumeInput();

    // ── Step 12: Update state and emit ───────────────────────────────────
    m_connectionSampleRateHz = newRateHz;
    emit wireSampleRateChanged(static_cast<double>(newRateHz));

    if (restartExternalDiversity && reconcileDiversity) {
        reconcileExternalDiversityRoute(computeDdcAssignment());
    }

    // Persist the new rate per-MAC so the next connect picks it up.
    if (!m_lastRadioInfo.macAddress.isEmpty()) {
        AppSettings::instance().setHardwareValue(
            m_lastRadioInfo.macAddress,
            QStringLiteral("radioInfo/sampleRate"),
            newRateHz);
    }

    const qint64 elapsedMs = t.elapsed();
    qCInfo(lcConnection) << "setSampleRateLive: done in" << elapsedMs << "ms";

    emit dspChangeMeasured(elapsedMs);
    return elapsedMs;
}

// ---------------------------------------------------------------------------
// setActiveRxCountLive — Task 1.7
//
// Active-RX-count live-apply coordinator.  Enables/disables the secondary
// receiver without disconnect/reconnect.  Strategy A (both P1 and P2):
//
//   P1 note: The plan (design §5D) flagged a potential need to rework
//   "MetisFrameParser" for mid-stream count changes.  Investigation found no
//   separate MetisFrameParser class — EP6 parsing lives in P1RadioConnection::
//   parseEp6Frame(frame, numRx, ...) which accepts numRx as a parameter on
//   every call and reads m_activeRxCount from the instance overload.  There is
//   no per-receiver cache to invalidate.  Full live-apply (Strategy A) is
//   therefore possible without any parser rework.
//
//   P2 note: setActiveReceiverCount() already calls sendCmdRx() when running,
//   which re-encodes the DDC enable bits in the next CmdRx packet.  No
//   additional stop/start cycle is needed on P2.
//
// NereusSDR-original infrastructure — no Thetis source ported here.
// Mirrors setSampleRateLive() (Task 1.6) in structure.
// ---------------------------------------------------------------------------
qint64 RadioModel::setActiveRxCountLive(int newCount)
{
    QElapsedTimer t;
    t.start();

    // Idempotent — safe when disconnected; avoids spurious warning on redundant
    // calls from the settings-restore path.
    if (newCount == m_connectionActiveRxCount) {
        return 0;
    }

    // Guard: nothing to do if disconnected or WDSP not initialized.
    if (!m_connection || !m_wdspEngine || !m_wdspEngine->isInitialized()) {
        qCWarning(lcConnection) << "setActiveRxCountLive: no active connection "
                                   "or WDSP not initialized — ignoring";
        return -1;
    }

    // Clamp to board capability.
    const int maxRx = m_hardwareProfile.caps ? m_hardwareProfile.caps->maxReceivers : 1;
    const int clamped = qBound(1, newCount, maxRx);
    qCInfo(lcConnection) << "setActiveRxCountLive:" << m_connectionActiveRxCount
                         << "->" << clamped;

    // ── Step 1: Quiesce DSP worker ────────────────────────────────────────────
    // Same pattern as setSampleRateLive step 1: disconnect I/Q feed and flush.
    if (m_dspWorker && m_receiverManager) {
        QObject::disconnect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
                            m_dspWorker, &RxDspWorker::processIqBatch);
        if (m_dspThread && m_dspThread->isRunning()) {
            QMetaObject::invokeMethod(m_dspWorker,
                                      &RxDspWorker::resetAccumulator,
                                      Qt::BlockingQueuedConnection);
        }
    }

    // Stop TX pump — defensive; setActiveRxCountLive shouldn't be called
    // while transmitting, but guard here as in setSampleRateLive.
    if (m_txWorker) {
        m_txWorker->stopPump();
        if (m_txChannel) {
            // Issue #258: TxWorkerThread::run() moves m_txChannel back to
            // this thread before returning, so this call is a defensive
            // no-op (Qt early-returns when target == current).  See
            // teardownConnection for the full rationale.
            m_txChannel->moveToThread(this->thread());
        }
    }

    // ── Step 2: Pause AudioEngine ─────────────────────────────────────────────
    m_audioEngine->pauseInput();

    // ── Step 3: Create / destroy WDSP RX channels ────────────────────────────
    // For each newly-needed receiver (index 1..clamped-1): create RxChannel.
    // For each receiver being removed (index clamped..m_connectionActiveRxCount-1):
    // destroy RxChannel.
    //
    // Channel 0 always exists and is never touched here.
    const int wdspRate   = m_connectionSampleRateHz > 0 ? m_connectionSampleRateHz : 48000;
    const int wdspInSize = bufferSizeForRate(wdspRate);

    if (clamped > m_connectionActiveRxCount) {
        // Adding receivers.
        for (int ch = m_connectionActiveRxCount; ch < clamped; ++ch) {
            if (!m_wdspEngine->rxChannel(ch)) {
                m_wdspEngine->createRxChannel(ch, wdspInSize, 4096,
                                              wdspRate, 48000, 48000);
                qCInfo(lcConnection) << "setActiveRxCountLive: created WDSP RX channel" << ch;
            }
        }
    } else {
        // Removing receivers.
        for (int ch = m_connectionActiveRxCount - 1; ch >= clamped; --ch) {
            if (ch > 0 && m_wdspEngine->rxChannel(ch)) {
                m_wdspEngine->destroyRxChannel(ch);
                qCInfo(lcConnection) << "setActiveRxCountLive: destroyed WDSP RX channel" << ch;
            }
        }
    }

    // ── Step 4: Reconfigure ReceiverManager DDC mapping ──────────────────────
    if (m_receiverManager) {
        if (clamped > m_connectionActiveRxCount) {
            // Activate receivers 1..clamped-1.  Create them if they don't exist.
            for (int rx = m_connectionActiveRxCount; rx < clamped; ++rx) {
                if (m_receiverManager->receiverConfig(rx).receiverIndex < 0) {
                    int created = m_receiverManager->createReceiver();
                    Q_UNUSED(created)
                }
                m_receiverManager->activateReceiver(rx);
            }
        } else {
            // Deactivate receivers clamped..m_connectionActiveRxCount-1.
            for (int rx = m_connectionActiveRxCount - 1; rx >= clamped; --rx) {
                m_receiverManager->deactivateReceiver(rx);
            }
        }
    }

    // ── Step 5: Update hardware ───────────────────────────────────────────────
    if (auto* p1 = qobject_cast<P1RadioConnection*>(m_connection)) {
        // P1: update m_activeRxCount and restart the EP6 stream so the radio
        // re-arms with the new per-frame slot count.  restartStreamWithCount()
        // mirrors restartStreamWithRate(): stop + prime(3) + start + prime(3).
        // Must run on the connection thread.
        QMetaObject::invokeMethod(p1, [p1, clamped]() {
            p1->restartStreamWithCount(clamped);
        }, Qt::QueuedConnection);
    } else {
        // P2 (and future protocol variants): setActiveReceiverCount() calls
        // sendCmdRx() when running — no stop/start cycle needed.
        QMetaObject::invokeMethod(m_connection,
                                  [conn = m_connection, clamped]() {
            conn->setActiveReceiverCount(clamped);
        }, Qt::QueuedConnection);
    }

    // ── Step 6: Restart TX pump ───────────────────────────────────────────────
    if (m_txWorker && m_txChannel) {
        m_txChannel->moveToThread(m_txWorker.get());
        m_txWorker->startPump();
    }

    // ── Step 7: Reconnect DSP worker I/Q feed ─────────────────────────────────
    if (m_dspWorker && m_receiverManager) {
        connect(m_receiverManager, &ReceiverManager::iqDataForReceiver,
                m_dspWorker, &RxDspWorker::processIqBatch,
                Qt::QueuedConnection);
    }

    // Resume AudioEngine.
    m_audioEngine->resumeInput();

    // ── Step 8: Update state, persist, emit ──────────────────────────────────
    m_connectionActiveRxCount = clamped;
    emit activeRxCountChanged(clamped);

    if (!m_lastRadioInfo.macAddress.isEmpty()) {
        AppSettings::instance().setHardwareValue(
            m_lastRadioInfo.macAddress,
            QStringLiteral("radioInfo/activeRxCount"),
            clamped);
    }

    const qint64 elapsedMs = t.elapsed();
    qCInfo(lcConnection) << "setActiveRxCountLive: done in" << elapsedMs << "ms";

    emit dspChangeMeasured(elapsedMs);
    return elapsedMs;
}

// ---------------------------------------------------------------------------
// Task 4.2 — rebuildDspOptionsForMode
//
// Called from DspOptionsPage when a per-mode combo changes and the combo's
// mode matches the current active slice mode (design Section 4B).
// Delegates to RxChannel::onModeChanged() and TxChannel::onModeChanged(),
// then emits dspChangeMeasured(ms) if a rebuild occurred.
//
// No-op guard: returns immediately if WDSP is not initialized or no
// RxChannel exists (e.g., disconnected, during teardown).
//
// NereusSDR-original infrastructure — no Thetis source ported here.
// ---------------------------------------------------------------------------
void RadioModel::rebuildDspOptionsForMode(DSPMode forMode)
{
    if (!m_wdspEngine || !m_wdspEngine->isInitialized()) {
        return;
    }

    // -1 = no change; 0+ = applied (in-place WDSP setters routinely finish
    // sub-millisecond, so 0 ms is a legitimate elapsed time).
    if (RxChannel* rxCh = m_wdspEngine->rxChannel(0)) {
        const qint64 elapsed = rxCh->onModeChanged(forMode);
        if (elapsed >= 0) {
            emit dspChangeMeasured(elapsed);
        }
    }

    // TX channel — guard: may be null (not created until radio connects).
    if (m_txChannel) {
        const qint64 txElapsed = m_txChannel->onModeChanged(forMode);
        if (txElapsed >= 0) {
            emit dspChangeMeasured(txElapsed);
        }
    }
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

// ── Phase 3P-II Task 19: PGXL status update handler ─────────────────────────
//
// Called on every PgxlConnection::statusUpdated. Behaviour:
//   1. First call (m_hasAmplifier still false): set m_hasAmplifier=true
//      and emit amplifierChanged(true).
//   2. Parse "state" key. The PGXL is considered in operate mode when the
//      state string is one of: IDLE, OPERATE, TRANSMIT_A, TRANSMIT_B
//      (the four states where the amp is on-air ready or transmitting).
//      If m_ampOperate changed, emit ampStateChanged().
//   3. Parse "peakfwd" (dBm float) and "swr" (signed dB return-loss float).
//      Convert: watts = 10^(dbm/10) / 1000
//               |reflection_coeff| = 10^(rl_db/20)   (rl_db is negative
//                                                    on a good match; e.g.
//                                                    swr=-24.5 -> |G|=0.0596)
//               SWR = (1 + |G|) / (1 - |G|)          (-> 1.13 for the
//                                                    example above)
//      Clamp |G| -> SWR at 99 when |G| approaches 1.0 (effectively
//      infinite SWR; open or short). Emit ampMetersChanged(watts, swr)
//      when both keys are present.
//
//      2026-05-20 bench fix: prior formula was `10^(-rl_db/20)` which
//      inverts the math -- for swr=-24.5 (well-matched antenna) it
//      produced ratio = 16.78 instead of 1.13, so the SMeterWidget TX
//      display showed a wildly wrong high-SWR readout the whole time
//      PGXL was actually amplifying into a low-SWR load. User report:
//      "always high SWR readout on PGXL".
void RadioModel::onPgxlStatus(const QMap<QString, QString>& kvs)
{
    // 1. First-presence detection.
    if (!m_hasAmplifier) {
        m_hasAmplifier = true;
        emit amplifierChanged(true);
    }

    // 2. Operate-state parse.
    if (kvs.contains(QStringLiteral("state"))) {
        const QString& st = kvs.value(QStringLiteral("state"));
        const bool nowOperate =
            (st == QStringLiteral("IDLE")        ||
             st == QStringLiteral("OPERATE")     ||
             st == QStringLiteral("TRANSMIT_A")  ||
             st == QStringLiteral("TRANSMIT_B"));
        // Surface PGXL state edges in the log so the autotune
        // standby/restore cycle and the TX-engagement (OPERATE ->
        // TRANSMIT_A) handshake are visible without a debugger.
        // Bench-noisy at most a few transitions per minute under normal
        // ops; not a logspam risk.
        if (st != m_lastPgxlState) {
            qCInfo(lcConnection)
                << "PGXL state edge:" << m_lastPgxlState << "->" << st
                << "(ampOperate=" << (nowOperate ? "true" : "false") << ")";
        }
        if (nowOperate != m_ampOperate) {
            m_ampOperate = nowOperate;
            emit ampStateChanged();
        }

        // Phase 3P-II Phase 4 Task 94: capture FAULT state *transitions* only.
        // A repeated FAULT push (same FAULT state, no edge) is not re-captured
        // so the ring buffer does not fill with duplicate events during a
        // sustained fault condition.
        if (st.startsWith(QStringLiteral("FAULT"))
                && !m_lastPgxlState.startsWith(QStringLiteral("FAULT"))) {
            // Phase 3P-II review fix C3: convert PGXL wire values to the
            // units expected by FaultEvent (watts / ratio) and
            // FaultLog::likelyCauseFor.  The 'fwd' key is dBm (same wire
            // encoding as 'peakfwd' in the meter path below) and 'swr' is
            // signed dB return loss (negative on a good match per the
            // PGXL Ethernet API; e.g. swr=-24.5 -> RL 24.5 dB -> SWR 1.13).
            //
            // 2026-05-20 bench fix: the prior formula `10^(-rl_db/20)`
            // inverted the math and produced ~16.8 for a well-matched
            // antenna. Replaced with the canonical |reflection| -> SWR
            // calculation. Mirrors the same conversion in the meter
            // path below.
            const float fwdDbm   = kvs.value(QStringLiteral("fwd")).toFloat();
            const float rlDbWire = kvs.value(QStringLiteral("swr")).toFloat();
            const float temp     = kvs.value(QStringLiteral("temp")).toFloat();
            const float fwdW     = std::pow(10.0f, fwdDbm / 10.0f) / 1000.0f;
            float swrRatio;
            if (rlDbWire >= 0.0f) {
                // RL >= 0 dB is physically open/short or measurement
                // glitch; cap to 99 for display.
                swrRatio = 99.0f;
            } else {
                const float gamma = std::pow(10.0f, rlDbWire / 20.0f);
                swrRatio = (gamma >= 0.999f)
                    ? 99.0f
                    : (1.0f + gamma) / (1.0f - gamma);
            }
            FaultEvent ev{
                QDateTime::currentMSecsSinceEpoch(),
                st,
                fwdW,
                swrRatio,
                temp,
                FaultLog::likelyCauseFor(fwdW, swrRatio, temp)
            };
            m_pgxlFaultLog->capture(ev);
        }

        m_lastPgxlState = st;
    }

    // 3. Power + SWR meter conversion (header doc-block above explains
    // the math + the 2026-05-20 bench-driven sign fix).
    const bool hasFwd = kvs.contains(QStringLiteral("peakfwd"));
    const bool hasSwr = kvs.contains(QStringLiteral("swr"));
    if (hasFwd && hasSwr) {
        const float dbm      = kvs.value(QStringLiteral("peakfwd")).toFloat();
        const float rlDbWire = kvs.value(QStringLiteral("swr")).toFloat();
        const float watts    = std::pow(10.0f, dbm / 10.0f) / 1000.0f;
        float ratio;
        if (rlDbWire >= 0.0f) {
            ratio = 99.0f;  // RL=0 -> infinite SWR; cap for display
        } else {
            const float gamma = std::pow(10.0f, rlDbWire / 20.0f);
            ratio = (gamma >= 0.999f)
                ? 99.0f
                : (1.0f + gamma) / (1.0f - gamma);
        }
        emit ampMetersChanged(watts, ratio);
    }
}

// ---------------------------------------------------------------------------
// derivedFlexSerial: extract the serial derivation so both the PGXL
// pairing flow and the FlexRadio discovery beacon use the same value.
// ---------------------------------------------------------------------------

QString RadioModel::derivedFlexSerial(const QString& mac) const
{
    // Phase 3P-II bench-discovered: PGXL's FlexRadio tab expects a 16-digit
    // dashed serial in 4-4-4-4 groups (e.g. 2923-1104-6600-8823). The earlier
    // "NereusSDR-<mac>" format was silently dropped by PGXL's regex validator.
    // SHA-256 the MAC + a salt, take 8 bytes -> 64-bit number -> mod 10^16 ->
    // dash-format. Deterministic per (host + radio MAC) install. Operator can
    // override via PGXL_FlexRadioSerial AppSettings key when a collision is
    // suspected.
    const AppSettings& s = AppSettings::instance();
    QString serial = s.value(QStringLiteral("PGXL_FlexRadioSerial"),
                             QString()).toString().trimmed();
    if (!serial.isEmpty()) {
        return serial;
    }

    const QByteArray salt = QByteArrayLiteral("NereusSDR-PGXL-v1");
    QByteArray hash = QCryptographicHash::hash(
        (mac.toUtf8() + salt), QCryptographicHash::Sha256);
    // Take first 8 bytes -> uint64 -> mod 10^16.
    quint64 n = 0;
    for (int i = 0; i < 8; ++i) {
        n = (n << 8) | static_cast<quint8>(hash[i]);
    }
    constexpr quint64 mod16 = 10000000000000000ULL;  // 10^16
    n %= mod16;
    const QString d = QString::number(n).rightJustified(16, '0');
    serial = QStringLiteral("%1-%2-%3-%4")
                 .arg(d.mid(0, 4))
                 .arg(d.mid(4, 4))
                 .arg(d.mid(8, 4))
                 .arg(d.mid(12, 4));
    qCInfo(lcConnection) << "FlexRadio serial derived from MAC:" << serial
                         << "(override via PGXL_FlexRadioSerial key)";
    return serial;
}

// ---------------------------------------------------------------------------
// Phase 3P-II Task 62: PGXL pairing-flow runner
// ---------------------------------------------------------------------------

void RadioModel::onPgxlConnected()
{
    if (!m_pgxlConnection) { return; }
    auto& s = AppSettings::instance();

    // Serial number derivation.
    // m_lastRadioInfo.macAddress may be empty before a radio connects;
    // fall back to a fixed placeholder so amplifier create still works.
    const QString mac = m_lastRadioInfo.macAddress.isEmpty()
                            ? QStringLiteral("00:00:00:00:00:00")
                            : m_lastRadioInfo.macAddress;

    const QString ourSerial = derivedFlexSerial(mac);

    const QString antMap = s.value(QStringLiteral("PGXL_AntMap"),
                                   QStringLiteral("ANT1:PORTA,ANT2:PORTB")).toString();
    // 2026-05-21 bench-confirmed: PGXL native protocol (TCP 9008) rejects
    // model=NereusSDR with error 50000015 (parse/validation) and closes the
    // socket, producing a tight reconnect loop on RadioModel's PgxlConnection.
    // Canonical FLEX-8600 pcap (flex-tgxl-direct-CONTROL.pcapng @ T+206)
    // advertises model="FLEX-8600M". PGXL whitelists known FlexRadio model
    // strings; "NereusSDR" is not on that list. Claim FLEX-8600M (the closest
    // multi-slice match for our P1/P2 support surface) so PGXL accepts the
    // pairing handshake. Override via PGXL_PairModel for future SKU work.
    const QString pgxlPairModel = s.value(QStringLiteral("PGXL_PairModel"),
                                          QStringLiteral("FLEX-8600M")).toString();
    m_pgxlConnection->amplifierCreate(ourSerial, pgxlPairModel, antMap);

    // Optional flexradio pairing (enabled by default via PGXL_PairAttempt).
    if (s.value(QStringLiteral("PGXL_PairAttempt"), QStringLiteral("True")).toString()
            == QStringLiteral("True")) {
        QChar slice = s.value(QStringLiteral("PGXL_FlexAmpSlice"),
                              QStringLiteral("A")).toString().at(0);
        const QString txAnt = s.value(QStringLiteral("PGXL_TxAnt"),
                                      QStringLiteral("ANT1")).toString();
        m_pgxlConnection->flexradioPair(slice, ourSerial, txAnt,
                                        /*pttOverLan=*/true, /*active=*/true);
    }

    // Always enable keepalive after pairing attempt.
    m_pgxlConnection->enableKeepalive();
}

// ---------------------------------------------------------------------------
// startTgxlAutotune - PGXL standby + TGXL relay sweep + PGXL restore.
// ---------------------------------------------------------------------------
//
// Bench-driven 2026-05-20: TGXL refuses to sweep relays when PGXL is in
// OPERATE because PGXL amplifies the radio's tune carrier and TGXL can't
// calibrate against the amplified signal -- TGXL reports "no PTT" and
// aborts. The correct operator workflow with real FlexRadio (per 4O3A's
// PGXL/TGXL User Guides and community guidance):
//   1. Put PGXL in STANDBY (amp bypassed)
//   2. Run TGXL autotune at radio tunepower (~10-25 W, low enough that
//      tuning a high-SWR load can't damage the amp/antenna)
//   3. Re-arm PGXL to OPERATE when tune completes
//
// fromHardware==true means the TGXL hardware TUNE button was pressed;
// TGXL is already running its own internal sweep and just needs the
// radio's carrier. We skip the explicit `autotune` command in that case.
//
// fromHardware==false means TunerApplet TUNE was clicked; we send
// `autotune` to TGXL on :9010 to start the sweep.
//
// PGXL state restore happens via the m_moxController->manualMoxChanged
// wire that detects local TUN dropping (handled in ctor). When that
// fires AND m_tgxlAutotuneInProgress is true, we send `operate=1` to
// restore PGXL to OPERATE (if it was operating before the tune cycle).
void RadioModel::startTgxlAutotune(bool fromHardware)
{
    if (!m_tgxlConnection || !m_tgxlConnection->isConnected()) {
        qCWarning(lcConnection)
            << "TGXL autotune requested but TGXL not connected; ignoring";
        return;
    }

    // Suppress recursive call. When WE initiate (fromHardware=false), our
    // `autotune` command causes TGXL to send back `transmit tune on` (LAN
    // PTT request -- TGXL telling the radio "please key up so I can sweep").
    // That arrives at SmartSdrApiListener and would re-enter this method
    // with fromHardware=true. The recursive entry would re-send operate=0
    // (already in flight) and any subsequent `transmit tune off` from TGXL
    // would trigger a premature carrier drop via manualMoxChanged(false).
    // Treat the LAN PTT during an in-progress cycle as confirmation only.
    if (m_tgxlAutotuneInProgress && fromHardware) {
        qCInfo(lcConnection)
            << "TGXL autotune: LAN PTT ack received during in-progress"
               " cycle, ignoring (we initiated and are already engaged)";
        return;
    }

    // 2026-05-22 reverted from commit 3a8662c5: hardware-initiated TUNE
    // path also needs to pre-standby PGXL.
    //
    // The earlier pcap-driven removal (3a8662c5) read canonical FLEX as
    // leaving PGXL in OPERATE during TGXL hardware TUNE. JJ's bench
    // proved this read wrong: at 2026-05-22 16:08-ish, with PGXL
    // staying in OPERATE during a TGXL hardware-TUNE-button press, PGXL
    // tripped on "high power" because TGXL was sweeping its relays
    // through PGXL's amplified output. The pcap capture we read may
    // have been from a different rig setup (lower drive, isolator,
    // or other path that's not in our bench config).
    //
    // Restored behavior: both fromHardware=true (TGXL hardware press)
    // and fromHardware=false (TunerApplet TUNE click) standby PGXL for
    // the duration of the TGXL relay sweep, then restore on completion.
    // Matches TunerApplet autotune's existing safe behavior that JJ
    // confirmed works correctly.

    // Snapshot PGXL state. m_ampOperate is the radio's view of PGXL's
    // operate-family state (IDLE / OPERATE / TRANSMIT_A / TRANSMIT_B).
    m_pgxlSavedOperate = m_hasAmplifier && m_ampOperate;
    m_tgxlAutotuneInProgress = true;
    m_tgxlAutotuneFromHardware = fromHardware;
    m_pgxlStandbyPending = m_pgxlSavedOperate;  // need to await STANDBY confirm?

    qCInfo(lcConnection)
        << "TGXL autotune starting (app TUNE). fromHardware=" << fromHardware
        << "pgxlSavedOperate=" << m_pgxlSavedOperate;

    if (!m_pgxlSavedOperate) {
        // PGXL already in non-operate state (STANDBY / FAULT / POWERUP /
        // not present). No need to wait for transition -- proceed.
        continueTgxlAutotuneAfterStandby();
        return;
    }

    // Send `operate=0` and wait for PGXL to broadcast `state=STANDBY` in
    // its next R-frame status update. The wait is event-driven via the
    // ampStateChanged signal (subscribed in the ctor). Mirrors the
    // PTT_REQUESTED / interlock-ready handshake pattern: send request,
    // wait for confirmation, then proceed -- no fragile fixed delay.
    if (m_pgxlConnection && m_pgxlConnection->isConnected()) {
        m_pgxlConnection->sendCommand(QStringLiteral("operate=0"));
        qCInfo(lcConnection)
            << "TGXL autotune: PGXL operate=0 sent, awaiting STANDBY ack";
    }

    // Failsafe: PGXL polls its state at ~10 Hz so the ampStateChanged
    // signal usually fires within 100-200 ms. If 1500 ms elapses without
    // confirmation (amp disconnected mid-cycle, status poll stuck), we
    // proceed anyway with a warning so the operator's TUNE isn't stranded.
    QTimer::singleShot(1500, this, [this]() {
        if (m_tgxlAutotuneInProgress && m_pgxlStandbyPending) {
            qCWarning(lcConnection)
                << "TGXL autotune: PGXL didn't confirm STANDBY within"
                   " 1.5 s, proceeding anyway (failsafe)";
            m_pgxlStandbyPending = false;
            continueTgxlAutotuneAfterStandby();
        }
    });
}

// Second half of the TGXL autotune orchestration: engage local TUN
// carrier and (if WE initiated) wait for the FlexAPI interlock chain
// to broadcast TRANSMITTING before sending `autotune` to TGXL. Called
// either directly from startTgxlAutotune (PGXL already in standby) or
// from the ampStateChanged signal handler (PGXL just transitioned to
// standby) or from the 1.5 s standby failsafe timer.
//
// 2026-05-20 fix: previously this used a fixed 200 ms QTimer::singleShot
// to delay the `autotune` command, on the theory that 200 ms was enough
// for the carrier to settle. On cold caches the FlexAPI interlock chain
// (MoxController walk -> moxStateChanged -> PTT_REQUESTED broadcast ->
// amp ACK round-trip -> TRANSMITTING broadcast) takes longer than 200 ms
// for the first key-up, so `autotune` arrived at TGXL on :9010 BEFORE
// TGXL had seen `S0|interlock state=TRANSMITTING` on :4992. TGXL then
// aborted ~350 ms later with "no PTT in". Bench timing 2026-05-20:
// first press cycle 547 ms (aborted), second 520 ms (aborted), third
// 2.98 s (successful). User: "first press of the tgxl tune failed with
// no ptt in message second tune worked".
//
// The fix is the same event-driven pattern we use for the PGXL standby
// confirmation: set a flag, wait for the signal, then proceed. The flag
// is m_awaitingInterlockForAutotune, the signal is interlockGranted from
// SmartSdrApiListener (wired in the ctor). A 1.5 s failsafe sends the
// autotune anyway if interlockGranted never fires (e.g. amps all
// disconnected mid-cycle before they could ACK).
void RadioModel::continueTgxlAutotuneAfterStandby()
{
    if (!m_tgxlAutotuneInProgress) {
        // Cycle was cancelled (e.g. operator hit TUN-off) before we got
        // here. Bail out -- don't engage TUN and don't send autotune.
        return;
    }
    qCInfo(lcConnection)
        << "TGXL autotune: PGXL standby ready, engaging local TUN carrier";
    setTune(true);

    if (m_tgxlAutotuneFromHardware) {
        // TGXL initiated this cycle via LAN PTT (`transmit tune on`); it's
        // already sweeping its own relays and just needed the carrier.
        // No `autotune` command to send and no interlock wait to gate on.
        return;
    }

    // Arm the gate: when interlockGranted fires (TRANSMITTING was just
    // broadcast to all clients including TGXL), the lambda in the ctor
    // sends the autotune command after a small 50 ms TCP socket settle.
    m_awaitingInterlockForAutotune = true;

    // Failsafe: amps usually ACK in <100 ms (and the listener's lenient
    // 500 ms timeout grants anyway). If 1500 ms elapses without
    // interlockGranted firing -- e.g. every amp disconnected mid-cycle
    // before it could ACK -- send the autotune anyway so the operator's
    // TUNE isn't stranded. Mirrors the standby failsafe in startTgxl-
    // Autotune (same 1.5 s budget, same "proceed with warning" semantic).
    QTimer::singleShot(1500, this, [this]() {
        if (m_awaitingInterlockForAutotune && m_tgxlAutotuneInProgress) {
            qCWarning(lcConnection)
                << "TGXL autotune: interlockGranted didn't fire within"
                   " 1.5 s, sending autotune anyway (failsafe)";
            m_awaitingInterlockForAutotune = false;
            sendTgxlAutotuneCmd();
        }
    });
}

// Send the `autotune` command on the TGXL :9010 control socket. Called
// from the interlockGranted handler (event-driven path, normal case) or
// from the 1.5 s failsafe timer in continueTgxlAutotuneAfterStandby
// (degraded path, no interlock confirmation arrived).
void RadioModel::sendTgxlAutotuneCmd()
{
    if (m_tgxlConnection && m_tgxlConnection->isConnected()
        && m_tgxlAutotuneInProgress) {
        m_tgxlConnection->sendCommand(QStringLiteral("autotune"));
        qCInfo(lcConnection) << "TGXL autotune: sent autotune cmd";
    }
}

// Phase 3P-II Phase 4 Task 96: auto-recall TGXL tune memory on band change.
//
// Design reference: docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md
// Task 96 / design ss4.8 "Recall flow".
//
// Bench caveat (design ss4.8): the TGXL absolute-relay-position write verb is
// not in AetherSDR's command set and has not been bench-confirmed.  Until a
// confirmed "relay set" API lands, recall issues a fresh "tune start" so the
// TGXL auto-tunes from its current position.  The stored memory acts as a UX
// hint (the table shows what was tuned last time).  The fall-back is noted in
// the log so a future implementer can see the placeholder clearly.
//
// Connected from addSlice() to every SliceModel::bandChanged. Only the
// stable TX binding may propagate a band transition to the global TGXL.
void RadioModel::onSliceBandChanged(SliceModel* source, NereusSDR::Band band)
{
    if (source != txBoundSlice()) { return; }
    if (!m_tuneMemoryStore || !m_tgxlConnection) { return; }

    const bool autoRecall = AppSettings::instance()
        .value(QStringLiteral("TGXL_AutoTuneMemoryRecall"), QStringLiteral("False"))
        .toString() == QStringLiteral("True");
    if (!autoRecall) { return; }

    // Determine the active antenna (1-indexed; TunerModel is 0-indexed).
    int antenna = 1;
    if (m_tunerModel && m_tunerModel->hasAntennaSwitch()) {
        antenna = m_tunerModel->antennaA() + 1;
        if (antenna < 1 || antenna > 3) { antenna = 1; }
    }

    const auto rec = m_tuneMemoryStore->recall(antenna, band);
    if (!rec.has_value()) {
        qCDebug(lcMeter) << "TGXL auto-recall: no stored memory for ant="
                         << antenna << "band=" << bandLabel(band) << "- no-op";
        return;
    }

    if (!m_tgxlConnection->isConnected()) {
        qCDebug(lcMeter) << "TGXL auto-recall: TGXL not connected, skipping for band="
                         << bandLabel(band);
        return;
    }

    // Bench-caveat placeholder: absolute relay-position write not yet confirmed.
    // Issue "autotune" so the TGXL re-tunes from its current position.
    // Replace with a direct relay-set command once the TGXL API is confirmed.
    // Bench-fix 2026-05-19: was `tune start`; pcap analysis shows real
    // PGXL/TGXL workflow uses `autotune` on the :9010 control channel.
    m_tgxlConnection->sendCommand(QStringLiteral("autotune"));
    qCInfo(lcMeter) << "TGXL auto-recall: triggered autotune for"
                    << "ant=" << antenna
                    << "band=" << bandLabel(band)
                    << "(stored relay C1=" << rec->c1
                    << "L=" << rec->l
                    << "C2=" << rec->c2
                    << "savedAt=" << rec->savedAtMs << ");"
                    << "absolute relay-write deferred pending bench confirmation";
}

// ── Phase 3F Sub-Epic B Task 16 ────────────────────────────────────────────

std::array<NereusSDR::SliceConfig, 5>
RadioModel::buildStreamConfigsForCodec() const
{
    // NereusSDR-original: assembles the input array the codec's
    // applyDdcAssignment() needs. No Thetis equivalent; Thetis builds
    // UpdateDDCs inputs inline in console.cs:8186-8538 [v2.10.3.15].
    //
    // Phase 3F Sub-Epic I Task 7b: indexed by DDC STREAM, not by slice. A
    // DDC belongs to a stream and slices bind to streams many-to-one
    // (ChannelMaster cmaster.h:75-82 [v2.10.3.15]: one `_rcvr` carries one
    // noise blanker and one panadapter but `double* audio[cmMAXSubRcvr]`,
    // so one receiver is one DDC is one stream). Indexing by slice handed
    // two co-hosted slices DDC2 and DDC3, contradicting the sharing model
    // they had just been bound under.
    std::array<NereusSDR::SliceConfig, 5> configs{};

    const int streams = std::min(m_streamAllocator.streamCount(), 5);
    for (int st = 0; st < streams; ++st) {
        if (!m_streamAllocator.isStreamActive(st)) { continue; }

        NereusSDR::SliceConfig& cfg = configs[st];
        cfg.live = true;

        // The DDC tunes to the window centre, not to any one slice: slices
        // sit at shift offsets inside it (SliceModel::shiftOffsetHz, pushed
        // into WDSP by RxChannel::setShiftFrequency).
        const double centreHz = m_streamAllocator.streamCentreHz(st);
        cfg.frequencyHz  = static_cast<qint64>(centreHz);
        cfg.bandIndex    = static_cast<int>(NereusSDR::bandFromFrequency(centreHz));
        cfg.sampleRateHz = m_streamAllocator.streamSampleRateHz(st);

        // Fold the per-slice flags across the stream's members. Iterating
        // m_slices directly rather than slicesOnStream(st): that returns
        // sliceIndex() values (WDSP channel ids), which stop matching list
        // positions once a slice is removed from the middle of the list.
        bool first = true;
        for (SliceModel* s : m_slices) {
            if (!s || s->streamIndex() != st) { continue; }

            // Any slice on the stream being TX-bound makes the stream's
            // chain TX-bound; likewise for a diversity request.
            cfg.txBound            = cfg.txBound || s->isTxSlice();
            cfg.diversityRequested = cfg.diversityRequested || s->diversityEnabled();

            if (first) {
                first = false;
                // Antenna comes from the first slice on the stream: they
                // share one RF chain, so they share its antenna.
                //
                // Map rxAntenna() string to the integer index used by
                // CodecContext:
                //   ANT1=1, ANT2=2, ANT3=3, EXT1=4, EXT2=5, BYPS=6, fallback=1.
                const QString ant = s->rxAntenna();
                if      (ant == QLatin1String("ANT1")) { cfg.antennaIndex = 1; }
                else if (ant == QLatin1String("ANT2")) { cfg.antennaIndex = 2; }
                else if (ant == QLatin1String("ANT3")) { cfg.antennaIndex = 3; }
                else if (ant == QLatin1String("EXT1")) { cfg.antennaIndex = 4; }
                else if (ant == QLatin1String("EXT2")) { cfg.antennaIndex = 5; }
                else if (ant == QLatin1String("BYPS")) { cfg.antennaIndex = 6; }
                else                                   { cfg.antennaIndex = 1; }
            }
        }
    }

    return configs;
}

NereusSDR::CodecContext RadioModel::currentCodecContext() const
{
    // Phase 3F Sub-Epic I closeout, defect F3: single read of the radio-state
    // inputs, so computeDdcAssignment and describeSuspendedStreams cannot
    // disagree about whether PureSignal is transmitting.
    NereusSDR::CodecContext ctx{};

    // Defect D2: the per-DDC ADC routing word. Thetis's fresh-install value
    // is rx_adc_ctrl1 = 4 (console.cs:15099 [v2.10.3.15]) with
    // rx_adc_ctrl2 = 0 (console.cs:15135), and setup.cs:16934 [v2.10.3.15]
    // states what the 4 encodes: "bits 3 & 2 set to 01 => DDC1 to ADC1".
    //
    // NereusSDR never assigned this on Protocol 2 -- P2RadioConnection's
    // buildCodecContext does not touch it, and this function builds a bare
    // CodecContext rather than calling that -- so the codecs' `a.adcCtrl1 =
    // ctx.adcCtrl & 0xff` faithfully copied a zero. The visible consequence
    // was in diversity: the DDC0/DDC1 sync pair both landed on ADC0, one
    // physical input sampled twice, which is not diversity at all.
    //
    // Seeded before the test seam below, not after, so the forced-state
    // path (setDdcContextForTest, which exists to reach the PureSignal and
    // diversity branches without a live radio) exercises the same ADC map
    // production does. defaultRxAdcCtrl gates on the board's ADC count so a
    // 1-ADC SKU is never handed an ADC1 selector.
    ctx.adcCtrl = NereusSDR::defaultRxAdcCtrl(boardCapabilities().adcCount);

    if (m_ddcCtxForTest) {
        ctx.mox           = m_ddcCtxMoxForTest;
        ctx.puresignalRun = m_ddcCtxPsForTest;
        ctx.diversity     = diversityActive();
        return ctx;
    }
    ctx.mox           = m_moxController ? m_moxController->isMox() : false;
    ctx.puresignalRun = (m_pureSignal && m_pureSignal->isPsEnabled());
    ctx.diversity     = diversityActive();
    return ctx;
}

#ifdef NEREUS_BUILD_TESTS
PureSignal* RadioModel::installPureSignalForTest(TxChannel* tx)
{
    m_pureSignal = std::make_unique<PureSignal>(
        /*engine=*/nullptr, tx, /*fb=*/nullptr, /*mox=*/nullptr,
        /*stepAtt=*/nullptr, /*twoTone=*/nullptr, /*parent=*/nullptr);
    connect(m_pureSignal.get(), &PureSignal::psEnabledChanged,
            this, &RadioModel::refreshDdcAssignmentForRadioState,
            Qt::UniqueConnection);
    return m_pureSignal.get();
}
#endif

bool RadioModel::diversityActive() const
{
    // Extracted from currentCodecContext with the D1 fix, so the Alex
    // per-chain decision and the DDC assignment read one answer. They must
    // agree: the diversity pair is DDC0 on ADC0 and DDC1 on ADC1 sampling one
    // signal through two front ends, and if the codec engages the pair while
    // the filter analysis thinks it did not, the two legs get different
    // preselectors and there is nothing coherent left to combine.
    //
    // Slice A only, and that is a known limitation rather than a choice here:
    // WDSP's pdiv[] is a 2-slot array keyed by External Diversity id, so
    // Sub-Epic G gates the whole path to Slice A (RadioModel.cpp guard in
    // wireSliceSignals). Resolve the stable Slice A id, not list position:
    // removing A must not silently promote B into ownership.
    if (m_ddcCtxForTest) { return m_ddcCtxDivForTest; }
    const SliceModel* target =
        sliceById(kExternalDiversityTargetSliceId);
    return target && target->diversityEnabled();
}

bool RadioModel::resolveExternalDiversitySources(
    const NereusSDR::DdcAssignment& assignment,
    const SliceModel* target, int& primaryDdc, int& secondaryDdc) const
{
    primaryDdc = -1;
    secondaryDdc = -1;
    if (!target) {
        return false;
    }

    const int stream = target->streamIndex();
    if (stream < 0 || stream >= 5) {
        return false;
    }

    primaryDdc = assignment.streamDdc[stream];

    // DdcAssignment::syncEnable names DDCs synchronized to DDC0. A valid
    // diversity assignment therefore publishes the designated stream on DDC0
    // and one equal-rate partner in that mask. The Hermes-class assignment
    // activates DDC1 through syncEnable without repeating it in ddcEnable.
    // During MOX+PureSignal
    // the designated stream remains on its user DDC while DDC0/DDC1 belong to
    // PS; rejecting any non-zero primary prevents that pair from being fed into
    // the diversity combiner.
    if (primaryDdc != 0
        || (assignment.ddcEnable & (1 << primaryDdc)) == 0
        || assignment.rate[primaryDdc] <= 0) {
        return false;
    }

    for (int ddc = 1; ddc < 8; ++ddc) {
        const int bit = 1 << ddc;
        if ((assignment.syncEnable & bit) == 0
            || assignment.rate[ddc] != assignment.rate[primaryDdc]) {
            continue;
        }
        secondaryDdc = ddc;
        return true;
    }

    return false;
}

void RadioModel::configureExternalDiversityRotation(
    const SliceModel* target)
{
    if (!target || !m_wdspEngine) {
        return;
    }

    // I/Q rotation: input 0 is the identity; input 1 carries the operator's
    // phase and gain correction. Output == nr selects WDSP's mixed result.
    const double gainLin =
        std::pow(10.0, target->diversityGainDb() / 20.0);
    const double rad = target->diversityPhaseDeg() * M_PI / 180.0;
    double iRot[2] = {1.0, gainLin * std::cos(rad)};
    double qRot[2] = {0.0, gainLin * std::sin(rad)};
    m_wdspEngine->configureExternalDiversity(
        kExternalDiversityId, 2, iRot, qRot, 2);
}

void RadioModel::reconcileExternalDiversityRoute(
    const NereusSDR::DdcAssignment& assignment)
{
    SliceModel* target =
        sliceById(kExternalDiversityTargetSliceId);
    int primaryDdc = -1;
    int secondaryDdc = -1;
    const bool sourcesReady =
        target && target->diversityEnabled()
        && m_wdspEngine && m_dspWorker
        && resolveExternalDiversitySources(
            assignment, target, primaryDdc, secondaryDdc);

    int chunkSize = 0;
    if (sourcesReady) {
        if (RxChannel* channel =
                m_wdspEngine->rxChannel(target->sliceIndex())) {
            chunkSize = channel->bufferSize();
        }
        // Unit/model-only construction has no open RxChannel. The codec rate
        // is the same source used to size that channel in production, so it is
        // also the deterministic fallback for partial startup and tests.
        if (chunkSize <= 0 && assignment.rate[primaryDdc] > 0) {
            chunkSize = bufferSizeForRate(assignment.rate[primaryDdc]);
        }
    }

    if (!sourcesReady || chunkSize <= 0
        || chunkSize > RxDspWorker::kMaxSaneExternalDiversityChunk) {
        if (m_externalDiversityRouteActive) {
            stopExternalDiversityRoute();
        }
        return;
    }

    const bool unchanged =
        m_externalDiversityRouteActive
        && m_externalDiversityPrimaryDdc == primaryDdc
        && m_externalDiversitySecondaryDdc == secondaryDdc
        && m_externalDiversityChunkSize == chunkSize;
    if (unchanged) {
        return;
    }

    if (m_externalDiversityRouteActive) {
        stopExternalDiversityRoute();
    }

    // Upstream ordering: CreateRadio creates stopped; the console configures
    // it; InboundBlock may only enter after the paired source route exists.
    if (!m_wdspEngine->createExternalDiversity(
            kExternalDiversityId, 2, chunkSize)) {
        return;
    }
    configureExternalDiversityRotation(target);

    auto publishRoute = [this, target, primaryDdc, secondaryDdc]() {
        m_dspWorker->setExternalDiversityRoute(
            kExternalDiversityId, target->sliceIndex(),
            primaryDdc, secondaryDdc);
    };
    const bool workerThreadRunning =
        m_dspThread && m_dspThread->isRunning()
        && m_dspWorker->thread() != QThread::currentThread();
    if (workerThreadRunning) {
        QMetaObject::invokeMethod(
            m_dspWorker, publishRoute, Qt::BlockingQueuedConnection);
    } else {
        publishRoute();
    }

    m_wdspEngine->setExternalDiversityRunning(
        kExternalDiversityId, true);
    m_externalDiversityRouteActive = true;
    m_externalDiversityPrimaryDdc = primaryDdc;
    m_externalDiversitySecondaryDdc = secondaryDdc;
    m_externalDiversityChunkSize = chunkSize;
}

void RadioModel::stopExternalDiversityRoute()
{
    // Clear the worker first. A BlockingQueuedConnection is the barrier that
    // prevents any raw I/Q event already ahead of this control call from
    // reaching WDSP after slot 0 is stopped or destroyed.
    if (m_dspWorker) {
        const bool workerThreadRunning =
            m_dspThread && m_dspThread->isRunning()
            && QThread::currentThread() != m_dspThread;
        if (workerThreadRunning) {
            QMetaObject::invokeMethod(
                m_dspWorker, &RxDspWorker::clearExternalDiversityRoute,
                Qt::BlockingQueuedConnection);
        } else {
            m_dspWorker->clearExternalDiversityRoute();
        }
    }

    if (m_wdspEngine) {
        m_wdspEngine->setExternalDiversityRunning(
            kExternalDiversityId, false);
        m_wdspEngine->destroyExternalDiversity(
            kExternalDiversityId);
    }

    m_externalDiversityRouteActive = false;
    m_externalDiversityPrimaryDdc = -1;
    m_externalDiversitySecondaryDdc = -1;
    m_externalDiversityChunkSize = 0;
}

// Codex review, PR #293. See RadioModel.h for the defect and for why this
// rehomes instead of removing.
int RadioModel::rehomeSlicesToPans(const QStringList& livePanIds)
{
    if (livePanIds.isEmpty()) {
        return 0;
    }

    const QString survivor = livePanIds.first();
    int moved = 0;
    for (SliceModel* s : std::as_const(m_slices)) {
        if (!s) { continue; }
        if (livePanIds.contains(s->panKey())) { continue; }
        // setPanKey emits panKeyChanged, which is what MainWindow needs in
        // order to move the slice's VfoWidget onto the surviving pan. Its
        // absence was half the defect: even re-expanding the layout left the
        // recreated pane with no flag on it.
        s->setPanKey(survivor);
        ++moved;
    }
    return moved;
}

// Codex review, PR #293. See RadioModel.h for why this is a chain property
// rather than a slice one.
bool RadioModel::widebandActiveForChain(int chainIdx) const
{
    if (chainIdx < 0) {
        return false;
    }

    // The capability gate. widebandAdcs is the number of ADCs on this board
    // that can carry a wideband stream; 0 means the board has no such
    // mechanism at all, which is every Protocol 1 SKU in the table
    // ("wideband mechanism differs; deferred to 3F-W", BoardCapabilities.cpp).
    //
    // Gated on the count being zero rather than on chainIdx < widebandAdcs,
    // deliberately. A chain index is not an ADC index: ANAN-100D and 200D
    // report two ADCs behind one preselector chain, so comparing one against
    // the other is the exact ADC-count-versus-chain-count confusion that has
    // already produced defects on this branch. The zero test is the part that
    // is unambiguous and it covers the reported case. Narrowing further needs
    // the chain-to-ADC mapping to be settled first; that is recorded as a
    // follow-up rather than guessed at here.
    if (boardCapabilities().widebandAdcs <= 0) {
        return false;
    }

    // Any live slice on this chain still asking is enough to hold it on.
    for (const SliceModel* s : m_slices) {
        if (!s) { continue; }
        if (s->chainIndex() != chainIdx) { continue; }
        if (s->widebandExtensionRequested()) { return true; }
    }
    return false;
}

NereusSDR::DdcAssignment RadioModel::computeDdcAssignment() const
{
    // NereusSDR-original glue. No Thetis equivalent at this abstraction layer.
    const NereusSDR::CodecContext ctx = currentCodecContext();

    const std::array<NereusSDR::SliceConfig, 5> streams = buildStreamConfigsForCodec();

    // Codec source. The RadioConnection owns the codec and is authoritative
    // whenever a connection object exists. ReceiverManager holds the same
    // non-owning pointer (wired at connect from p1CodecChanged /
    // p2CodecChanged, cleared in reset()) and is the fallback when there is
    // no connection to ask. That is what lets the mapping be computed, and
    // unit-tested, without standing up a UDP socket.
    if (auto* p2conn = qobject_cast<P2RadioConnection*>(m_connection)) {
        if (NereusSDR::IP2Codec* codec = p2conn->p2Codec()) {
            return codec->applyDdcAssignment(ctx, streams);
        }
    } else if (auto* p1conn = qobject_cast<NereusSDR::P1RadioConnection*>(m_connection)) {
        if (NereusSDR::IP1Codec* codec = p1conn->p1Codec()) {
            return codec->applyDdcAssignment(ctx, streams);
        }
    } else if (m_receiverManager) {
        if (NereusSDR::IP2Codec* codec = m_receiverManager->p2Codec()) {
            return codec->applyDdcAssignment(ctx, streams);
        }
        if (NereusSDR::IP1Codec* codec = m_receiverManager->p1Codec()) {
            return codec->applyDdcAssignment(ctx, streams);
        }
    }

    // No codec selected yet: no board, so no DDC numbers exist. Every
    // streamDdc entry stays at the -1 idle sentinel.
    return NereusSDR::DdcAssignment{};
}

void RadioModel::publishDdcAssignment(const NereusSDR::DdcAssignment& assignment)
{
    // Phase 3F Sub-Epic I Task 7b: cache the codec's per-stream choice.
    // Idle streams keep the -1 sentinel, so an emptied stream leaves no
    // stale DDC behind for ddcForStream() to report.
    m_streamDdc = {{assignment.streamDdc[0], assignment.streamDdc[1],
                    assignment.streamDdc[2], assignment.streamDdc[3],
                    assignment.streamDdc[4]}};

    // Route each stream's hardware DDC to its logical receiver. Without
    // this, ReceiverManager::rebuildHardwareMapping's fallback auto-assign
    // hands stream 1 whatever nextAutoHw reaches (DDC0 on a G2, reserved for
    // the PureSignal / diversity pair) while the codec enables DDC3, so
    // every packet for that stream is dropped in feedIqData for want of an
    // m_hwToLogical entry. setDdcMapping re-runs rebuildHardwareMapping for
    // an already-active receiver; for an inactive one the activation
    // reconcile below re-runs it, so the mapping is live either way.
    //
    // PROTOCOL 1 IS EXCLUDED, and this is not an optimisation. The codec's
    // DDC number is the ReceiverManager routing key on Protocol 2 only:
    // P2RadioConnection emits iqDataReceived keyed by the real DDC index
    // (P2RadioConnection.cpp:2736 + :2809), but Protocol 1 packs the ACTIVE
    // receivers sequentially into the EP6 frame and emits their frame-slot
    // index (P1RadioConnection.cpp:2999-3007). Publishing DDC numbers onto a
    // P1 receiver would route stream 0 to hw index 2 on Anvelina Pro 3 /
    // RedPitaya and drop every EP6 packet: the exact regression recorded in
    // connectToRadio's "P1 radios deliver samples on hardware receiver index
    // 0" comment (issue #263). The sequential auto-assign that
    // rebuildHardwareMapping already performs IS the correct P1 answer,
    // because nth-active-receiver maps to nth frame slot by construction.
    // The slice-level publish below still carries the codec's DDC number on
    // P1: that is the wire-level truth, just not a routing key.
    const bool protocol1 =
        (qobject_cast<NereusSDR::P1RadioConnection*>(m_connection) != nullptr)
        || (m_connection == nullptr && m_receiverManager
            && m_receiverManager->p1Codec() != nullptr);

    // Idle streams are skipped rather than cleared to -1: -1 restores the
    // auto-assign fallback that caused the drop in the first place, and a
    // deactivated receiver is excluded from m_hwToLogical anyway, so the
    // last-known explicit DDC is the safer thing to leave behind.
    if (m_receiverManager && !protocol1) {
        const int streams = std::min(m_streamAllocator.streamCount(), 5);
        for (int st = 0; st < streams; ++st) {
            const int ddc = assignment.streamDdc[st];
            if (ddc >= 0) {
                m_receiverManager->setDdcMapping(st, ddc);
            }
        }
    }

    // ── Defect D1: publish the ADC the codec actually chose ──────────────
    //
    // ReceiverConfig::adcIndex is what the Alex per-chain analysis groups by
    // (chainForStream -> republishAlexAdcSlices / sliceChainIndex /
    // bypassReasonForAdc). Before this loop its only writer in the whole tree
    // was setAdcForReceiver, called once, with 0, for receiver 0. Every slice
    // therefore reported ADC0 no matter what the radio had been told.
    //
    // That was harmless while nothing moved a DDC off ADC0. Commit 99709649
    // ended that by routing a slice on an RX-only antenna to ADC1, and
    // 7cc35f20 made it reach the ANAN-G2. From then on the wire and the
    // filter analysis disagreed: the radio moved the DDC, AlexController
    // still counted the slice on chain 0, saw two bands on one chain, and
    // bypassed BOTH chains when one of them should have been filtered.
    //
    // Read back rather than re-derive. adcForDdc decodes the same two ADC
    // control bytes the codec just composed and P2RadioConnection is about to
    // send, so the model reports what the radio was told. Deriving the ADC
    // here from the antenna a second time would be a second copy of the
    // codec's policy, free to drift from it.
    //
    // NOT gated on protocol: unlike setDdcMapping above, adcIndex is not a
    // routing key, so publishing it on P1 cannot misroute a packet. It is
    // also inert there in practice, because every P1 SKU in the table has
    // rxFilterChainCount == 1 and chainForStream folds the whole radio onto
    // chain 0.
    //
    // The stored value is the true ADC, not the chain. Callers that want the
    // chain go through chainForStream, which is where the one-chain fold
    // lives; ReceiverConfig::adcIndex keeps meaning what its name says.
    //
    // Written to m_streamAdc AND mirrored into ReceiverManager. One writer,
    // one source (this assignment), two destinations -- the same fan-out
    // m_streamDdc / setDdcMapping does a few lines up. m_streamAdc is the one
    // chainForStream reads, because a ReceiverConfig only exists once
    // connectToRadio has created a receiver for that stream; the
    // ReceiverManager copy is there so its long-standing adcIndex field stops
    // reporting 0 for a DDC the radio moved, which is the lie D1 was built on.
    {
        const int streams = std::min(m_streamAllocator.streamCount(), 5);
        for (int st = 0; st < streams; ++st) {
            const int ddc = assignment.streamDdc[st];
            if (ddc < 0) { continue; }   // suspended: keep the last known ADC
            const size_t streamIndex = static_cast<size_t>(st);
            m_streamAdc[streamIndex] =
                NereusSDR::adcForDdc(assignment, ddc);
            if (m_receiverManager) {
                // Mirror the already-decoded physical map. Do not decode the
                // control bytes again here: SliceModel, ReceiverManager, and
                // the Alex chain analysis must all consume one answer.
                m_receiverManager->setAdcForReceiver(st,
                                                     m_streamAdc[streamIndex]);
            }
        }
    }

    // Stamp every slice with both physical-routing coordinates of the stream
    // hosting it, so co-hosted slices agree. chainForStream reads the same
    // m_streamAdc entry just mirrored into ReceiverManager above and performs
    // the board-specific ADC-to-filter-chain fold in exactly one place.
    for (SliceModel* s : std::as_const(m_slices)) {
        if (!s) { continue; }
        const int st = s->streamIndex();
        const bool validStream = st >= 0 && st < 5;
        s->setDdcIndex(validStream ? assignment.streamDdc[st] : -1);
        s->setChainIndex(validStream ? chainForStream(st) : -1);

        // Codex review, PR #293: psPaused is what greys the pan and raises the
        // PS HOLD pill (PanadapterApplet reads slice->psPaused() to drive
        // SpectrumStatusOverlay), and nothing in production ever wrote it. The
        // only callers were tests, so the property round-tripped perfectly
        // while the overlay went on presenting a slice the radio had stopped
        // streaming as live.
        //
        // Driven from the assignment rather than from the PureSignal
        // coordinator, which is what its declaration originally proposed. The
        // codec is the component that decides whether this slice still has a
        // DDC, so reading its answer cannot drift from it; a parallel signal
        // out of the PS coordinator would be a second opinion free to disagree,
        // and disagreeing copies of one fact are most of what this branch has
        // spent its time fixing.
        //
        // The condition is exactly the suspended-stream test below: a stream
        // that still hosts slices and came back with no DDC. That set is not
        // PureSignal-specific, so a diversity reclaim greys the pan too. The
        // radio really has stopped streaming it in both cases, so greying is
        // right; only the pill's wording is narrower than the state it shows.
        s->setPsPaused(validStream && assignment.streamDdc[st] < 0);
    }

    // ── Phase 3F Sub-Epic I closeout, defect F3 ─────────────────────────
    //
    // A stream that still hosts slices but came back from the codec with no
    // DDC has been suspended: the radio has stopped streaming it. Announce
    // it, because until now it was completely silent.
    //
    // The suspension itself is CORRECT and stays. It is what Thetis does.
    // On the 1-ADC HERMES class -- the family P2CodecHermes and
    // P1CodecStandard implement -- UpdateDDCs collapses to a single synced
    // pair the moment PureSignal transmits or diversity engages, dropping
    // every user receiver including RX1:
    //
    //   From Thetis console.cs:8448-8456 [v2.10.3.15]:
    //     else // transmitting and PS is ON
    //     {
    //         P1_DDCConfig = 6; DDCEnable = DDC0; SyncEnable = DDC1;
    //         Rate[0] = ps_rate; Rate[1] = ps_rate; cntrl1 = 4; cntrl2 = 0;
    //     }
    //
    // and there is no trailing `if (rx2_enabled) DDCEnable += DDC1;` on that
    // branch, unlike the ORION class at console.cs:8299-8303 which keeps RX2
    // on DDC3 through every PS and diversity state. Thetis's own GetDDC
    // agrees: for Hermes / HermesII / HermesC10 on P2 the MOX+PS cases are
    // literally empty, so rx1 and rx2 both come back -1
    // (console.cs:8635-8636 and 8641-8642 [v2.10.3.15]).
    //
    // What Thetis does NOT do is tell the operator. Nothing unchecks RX2,
    // nothing greys it, and the only trace is a label that quietly fails to
    // repaint on the Setup ADC tab. That is the part worth improving on: the
    // behaviour is upstream-faithful, the silence is not.
    QVector<int> suspended;
    {
        const int streams = std::min(m_streamAllocator.streamCount(), 5);
        for (int st = 0; st < streams; ++st) {
            if (assignment.streamDdc[st] < 0 && !slicesOnStream(st).isEmpty()) {
                suspended.append(st);
            }
        }
    }
    if (suspended != m_suspendedStreams) {
        m_suspendedStreams = suspended;
        emit streamsSuspended(suspended, describeSuspendedStreams(suspended));
    }

    // Phase 3F Sub-Epic I Task 7: reconcile ReceiverManager activation
    // against the current slice bindings. bindSliceToStream / removeSlice
    // (Task 6) maintain SliceStreamAllocator's own stream-active
    // bookkeeping and call setReceiverFrequency, but neither touches
    // ReceiverManager's per-receiver active flag, so a freshly-claimed
    // stream's receiver stayed inactive forever and
    // ReceiverManager::setReceiverFrequency silently stored the frequency
    // without pushing hardwareFrequencyChanged: it only emits when active,
    // and rebuildHardwareMapping() only assigns a hardwareRx to active
    // receivers (ReceiverManager.cpp). activateReceiver() re-runs
    // rebuildHardwareMapping(), which re-emits hardwareFrequencyChanged
    // from the already-stored frequency, so activating here is sufficient
    // even though setReceiverFrequency already ran earlier in
    // bindSliceToStream. Runs for both P1 and P2: receiver activation is
    // client-side bookkeeping, independent of whether the P1 DDC wire-byte
    // path (deferred to Sub-Epic C, see invokeCodecDdcAssignment below) is
    // implemented. Both activateReceiver/deactivateReceiver no-op when
    // already in the target state, so this is cheap to run on every
    // assignment change.
    if (m_receiverManager) {
        for (int st = 0; st < streamPoolSize(); ++st) {
            if (slicesOnStream(st).isEmpty()) {
                m_receiverManager->deactivateReceiver(st);
            } else {
                m_receiverManager->activateReceiver(st);
            }
        }
    }

    // The same complete assignment that selected the hardware DDCs owns the
    // paired worker route. Keeping this at the publish boundary prevents a
    // second source-selection policy from drifting from the wire state.
    reconcileExternalDiversityRoute(assignment);
}

QString RadioModel::describeSuspendedStreams(const QVector<int>& streams) const
{
    if (streams.isEmpty()) {
        return QString();
    }

    // Slice letters, not stream numbers: the operator sees A/B/C/D on the
    // flags and the RX applet, never a stream index.
    QStringList letters;
    for (SliceModel* s : m_slices) {
        if (!s || !streams.contains(s->streamIndex())) { continue; }
        letters.append(QString(QChar('A' + s->sliceIndex())));
    }
    letters.sort();
    if (letters.isEmpty()) {
        return QString();
    }

    const QString who = letters.size() == 1
                            ? QStringLiteral("Slice %1 has").arg(letters.first())
                            : QStringLiteral("Slices %1 have").arg(letters.join(
                                  QStringLiteral(", ")));

    const NereusSDR::CodecContext ctx = currentCodecContext();
    const bool mox = ctx.mox;
    const bool ps  = ctx.puresignalRun;
    const bool div = ctx.diversity;

    if (ps && mox) {
        return QStringLiteral("%1 no receiver while PureSignal is transmitting "
                              "on this radio. Unkey to restore.").arg(who);
    }
    if (div) {
        return QStringLiteral("%1 no receiver while diversity is on for this "
                              "radio.").arg(who);
    }
    return QStringLiteral("%1 no receiver: this radio has no DDC free for "
                          "them right now.").arg(who);
}

void RadioModel::refreshDdcAssignmentForRadioState()
{
    // MOX, effective PureSignal run state, and diversity are codec inputs.
    // Use the same complete request as slice binding so Protocol 2 has one
    // owner for enable/rate/ADC/sync wire state. Protocol 1's invocation is
    // client-side only; ReceiverManager::ddcConfigChanged retains its P1
    // applyPsDdcConfig wire path.
    requestDdcAssignment();
}

void RadioModel::invokeCodecDdcAssignment()
{
    const NereusSDR::DdcAssignment assignment = computeDdcAssignment();

    // Wire push. P2 only: the P1 codec's DdcAssignment is computed above but
    // the existing applyPsDdcConfig flow handles P1 wire writes, and full P1
    // integration is deferred to Phase 3F Sub-Epic C. Gated on a live
    // connection; the client-side publish below is not, because the mapping
    // is model bookkeeping that has to be correct before the first packet
    // arrives.
    //
    // ── Phase 3F Sub-Epic I closeout: marshal to the connection thread ────
    //
    // applyDdcAssignment rewrites m_rx[] and calls sendCmdRx(), which writes
    // the QUdpSocket. RadioModel runs on the GUI thread and the connection
    // was moved onto m_connThread (see connectToRadio), so calling it
    // directly tore m_rx[] against the connection thread's own frame
    // composition and drove QUdpSocket::writeDatagram from a thread that
    // does not own the socket or its notifier. requestDdcAssignment is wired
    // to every slice's frequencyChanged, so this ran on every VFO tick.
    //
    // Same marshalling shape as the hardwareReceiverCountChanged and
    // hardwareFrequencyChanged pushes in wireConnectionSignals: the functor
    // overload of QMetaObject::invokeMethod with default Qt::AutoConnection,
    // which is a plain call when the target already lives on this thread and
    // a queued QMetaCallEvent when it does not.
    //
    // No qRegisterMetaType is needed. The functor overload packages the whole
    // lambda into the event, so `assignment` travels as an ordinary by-value
    // capture of a trivially copyable aggregate; the metatype system is only
    // involved for the Q_ARG / string-name overload or for a queued
    // signal-slot connection carrying DdcAssignment as a parameter.
    if (isConnected()) {
        if (auto* p2conn = qobject_cast<P2RadioConnection*>(m_connection)) {
            if (p2conn->p2Codec()) {
                QMetaObject::invokeMethod(p2conn, [p2conn, assignment]() {
                    p2conn->applyDdcAssignment(assignment);
                });
            }
        }
    }

    publishDdcAssignment(assignment);
}

} // namespace NereusSDR
