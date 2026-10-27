// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/containers/ContainerButtonDispatcher.cpp  (NereusSDR)
// =================================================================
//
// Maps each container button to the NereusSDR feature it fronts, on the
// container's own slice. The dispatch is NereusSDR's own; the step sizes,
// the NR cycle and the lit states are
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Band grids: the slice's band and group (setSliceBand), the
//                 2 m reason kept with its band, clicks by band number; an
//                 SWL band lights as itself (bandGridBandFromFrequency).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Fix round 1 (minor 4): MOX, TUNE and 2-TONE on a remote
//                 window give the link-down words by state. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Created (R-R3-49, R-R3-21). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2): MON follows
//                                    the transmit settings gate in a
//                                    remote window (the Core's monEnabled).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Receiver and transmit gaps plan, Task
//                                    7: the MOX button keys through
//                                    RadioModel::setMoxFromButton (a manual
//                                    key). AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app plan, desktop remote
//                                    transmit (R-IOS-13): a remote window's
//                                    MOX, TUNE and 2-TONE light from the
//                                    Core; 2-TONE goes through
//                                    RadioModel::setTwoTone. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 7): PS-A follows
//                                    the PureSignal arming gate in a remote
//                                    window (the Core arms off the air).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Receiver and transmit gaps plan, Task
//                                    16: TUNE, MOX (outside SPEC and DRM)
//                                    and 2-Tone are unavailable with the
//                                    receive-only reason. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Task 16 fix wave: MOX in every mode
//                                    (I3); both reasons in a remote window
//                                    (M6). AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  A11 / R-R3-49 (parity Task 31): DUP
//                                    (display duplex), the window's
//                                    DisplayDuplex setting through its
//                                    hooks. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 port part 2: TUN, MOX and 2TONE are unavailable
//                with the reason while a TX inhibit holds (the HL2 I/O
//                board's fault code, say), as Thetis's TXInhibit setter
//                does. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control plan Task 2: sliceFor
//                                    asks SliceAccessPolicy. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 15 fix round
//                                    1: the sliceRefusal hook refuses a
//                                    slice another device controls with
//                                    the RX applet's reason, in remote and
//                                    hosting windows. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  TX rulings (item 1): a remote
//                                    window's MOX and TUNE press toggles
//                                    against its own key. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave, hosting 2-TONE parity: a
//                                    hosting window's 2TONE asks to take
//                                    transmit, as MOX and TUNE do.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Fix wave GUI-I7: PS-A greys with the
//                                    facade's own reason (on the air, or
//                                    no radio that supports PureSignal).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  The container Power button is removed
//                                    (maintainer decision): an old Power id
//                                    falls to the unavailable default and a
//                                    click changes nothing. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Issue #357: a local window's TUN
//                                    lights from the transmit model's TUNE,
//                                    which the release press clears.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-12  J.J. Boyd / KG4VCF  The CTUN button is unavailable, with
//                                    the reason, while the receiver's
//                                    C-Tune setting is another's to change.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-26  J.J. Boyd / KG4VCF  Container item parity Task 60: 31
//                                    receive buttons on the container's
//                                    slice (NR, NR1-NR4, NB, NB1, NB2, MNF+,
//                                    RIT, XIT, RIT0, XIT0, Lock, TS+/TS-,
//                                    Mute All, Voice SQL, AF+/-, BAL+/-,
//                                    the AGC modes, AUTO and AGC+/-),
//                                    through existing setters; none keys.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-26  J.J. Boyd / KG4VCF  Container item parity Task 61: 20
//                                    transmit setting buttons (COMP, VOX,
//                                    DEXP, TX EQ, CFC, CFC EQ, Leveler,
//                                    PhaRot, MIC+/-, COMP+/-, VOX+/-, DRI+/-,
//                                    DRI-0, TUN+/-, TUN-0) through the TX
//                                    applet's setters and gates, clamped to
//                                    their sliders; none keys. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-10-26  J.J. Boyd / KG4VCF  Container item parity Task 62: 12
//                                    display and window buttons (NF, Peak
//                                    Blobs, Cur Info, Spots, Fill, ZTB,
//                                    A-Peak on the slice's panadapter;
//                                    MAF+/-; Setup, Diversity, Linearity)
//                                    through the controls' own paths; none
//                                    keys. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-10-26 - Final review fix wave (C1): the Thetis console.cs and
//                 MeterManager.cs headers for the ported step constants,
//                 NR cycle and lit states. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
// =================================================================

// --- From console.cs ---
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
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// --- From MeterManager.cs ---
/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

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

mw0lge@grange-lane.co.uk
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

#include "gui/containers/ContainerButtonDispatcher.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/BoardCapabilities.h"
#include "core/ControlRanges.h"
#include "core/HpsdrModel.h"
#include "core/NbFamily.h"
#include "core/MoxController.h"
#include "core/SliceOwnership.h"
#include "core/session/SliceAccessPolicy.h"
#include "core/TwoToneController.h"
#include "core/session/IStationLink.h"
#include "core/session/PureSignalSessionFacade.h"
#include "gui/SpectrumWidget.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/ButtonBoxItem.h"
#include "models/Band.h"
#include "models/BandDefaults.h"
#include "models/NotchModel.h"
#include "models/PureSignalSettings.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QObject>

#include <algorithm>
#include <utility>

namespace NereusSDR {

namespace {

using Id = ContainerButtonDispatcher::Id;

// The function buttons connected to a NereusSDR feature. Every other core
// button is hidden through UnbuiltFeatures (OtherButtonItem). Power is not
// one: NereusSDR has no Power button (maintainer decision 2026-09-30), and
// OtherButtonItem never draws it.
constexpr Id kConnected[] = {
    Id::Mon, Id::Tun, Id::Mox, Id::TwoTon, Id::PsA,
    Id::Anf, Id::Snb, Id::Mnf, Id::PeakHold, Id::Ctun,
    Id::Vac1, Id::Vac2, Id::Mute, Id::Bin, Id::Dup,
    // Container item parity Task 60: the receive buttons.
    Id::Nr, Id::Nr1, Id::Nr2, Id::Nr3, Id::Nr4,
    Id::Nb, Id::Nb1, Id::Nb2, Id::MnfPlus,
    Id::Rit, Id::Rit0, Id::Xit, Id::Xit0, Id::LockA,
    Id::TuneStepUp, Id::TuneStepDown, Id::MuteAll, Id::SqlVsql,
    Id::AfP5, Id::AfM5, Id::BalP5, Id::BalM5,
    Id::AgcFixed, Id::AgcLong, Id::AgcSlow, Id::AgcMedium, Id::AgcFast,
    Id::AgcCustom, Id::AgcAuto, Id::AgcP5, Id::AgcM5,
    // Container item parity Task 61: the transmit setting buttons.
    Id::Comp, Id::Vox, Id::Dexp, Id::TxEq, Id::Cfc, Id::CfcEq, Id::Leveler, Id::PhaseRot,
    Id::MicP1, Id::MicM1, Id::CompP1, Id::CompM1, Id::VoxP1, Id::VoxM1,
    Id::DriveP5, Id::DriveM5, Id::Drive0, Id::TuneP5, Id::TuneM5, Id::Tun0,
    // Container item parity Task 62: the display and window buttons.
    Id::Nf, Id::PeakBlobs, Id::CursorInfo, Id::Spots, Id::FillSpectrum, Id::Ztb,
    Id::ActivePeak, Id::MafP5, Id::MafM5,
    Id::FormSetup, Id::FormDiversity, Id::FormLinearity,
};

// From Thetis console.cs:51556-51557 [v2.10.3.15]:
//   case OtherButtonId.AF_P5: SetAF(rx, GetAF(rx) + 5); break;
//   case OtherButtonId.AF_M5: SetAF(rx, GetAF(rx) - 5); break;
constexpr int kAfStep = 5;
// From Thetis console.cs:51558-51559 [v2.10.3.15]:
//   case OtherButtonId.BAL_P5: SetBal(rx, GetBal(rx) + 5); break;
//   case OtherButtonId.BAL_M5: SetBal(rx, GetBal(rx) - 5); break;
// on Thetis's 0..100 balance; NereusSDR's pan is -1..1, the RX applet's
// slider mapping (v - 50) / 50 (RxApplet.cpp:1087), so 5 is 0.1.
constexpr int kThetisBalStep = 5;
constexpr double kThetisBalHalfRange = 50.0;
constexpr double kBalStep = kThetisBalStep / kThetisBalHalfRange;
constexpr double kPanLeft = -1.0;
constexpr double kPanRight = 1.0;
// From Thetis console.cs:51572-51573 [v2.10.3.15]:
//   case OtherButtonId.AGC_P5: SetAgcT(rx, GetAgcT(rx) + 5); break;
//   case OtherButtonId.AGC_M5: SetAgcT(rx, GetAgcT(rx) - 5); break;
constexpr int kAgcThresholdStep = 5;
// From Thetis console.cs:43334 [v2.10.3.15] (incrementNR):
//   if (_nr_selected[rx - 1] > 4) _nr_selected[rx - 1] = 0;
constexpr int kLastThetisNr = 4;
// From Thetis console.cs:51566-51571 [v2.10.3.15]:
//   case OtherButtonId.MIC_P1: Mic += 1; break;
//   case OtherButtonId.MIC_M1: Mic -= 1; break;
//   case OtherButtonId.COMP_P1: CPDRLevel += 1; break;
//   case OtherButtonId.COMP_M1: CPDRLevel -= 1; break;
//   case OtherButtonId.VOX_P1: VOXSens += 1; break;
//   case OtherButtonId.VOX_M1: VOXSens -= 1; break;
// One dB of mic gain, compression and VOX threshold here.
constexpr int kMicGainStepDb = 1;
constexpr int kCpdrLevelStepDb = 1;
constexpr int kVoxThresholdStepDb = 1;
// From Thetis console.cs:51574-51579 [v2.10.3.15]:
//   case OtherButtonId.DRIVE_P5: PWR += 5; break;
//   case OtherButtonId.DRIVE_M5: PWR -= 5; break;
//   case OtherButtonId.TUNE_P5: TunePWR += 5; break;
//   case OtherButtonId.TUNE_M5: TunePWR -= 5; break;
//   case OtherButtonId.DRIVE_0: PWR = 0; break;
//   case OtherButtonId.TUN_0: TunePWR = 0; break;
constexpr int kDriveStep = 5;
constexpr int kTunePowerStep = 5;
constexpr int kPowerZero = 0;
// The transmitSettingsVersion each control's gate needs (MainWindow's
// applyRemoteRoleGating): the RF Power slider version 1; the Phone/CW and
// TX applets' chain (COMP, DEXP, MIC, LEV, EQ, CFC, VOX level, Tune power)
// version 2; Setup > DSP > CFC's CFC EQ and phase rotator version 4.
// From Thetis console.cs:51554-51555 [v2.10.3.15]:
//   case OtherButtonId.MAF_P5: AF += 5; break;
//   case OtherButtonId.MAF_M5: AF -= 5; break;
// on Thetis's 0..100 master AF; NereusSDR's master volume is 0..1, the
// title bar slider's 0-100, so 5 is 0.05.
constexpr int kThetisMafStep = 5;
constexpr double kThetisMafRange = 100.0;
constexpr double kMasterVolumeStep = kThetisMafStep / kThetisMafRange;
// The Spot Hub's "Spots" switch (SpotHubDialog's displaySpotsToggle) and
// SpectrumWidget::loadSpotDisplaySettings's default for it.
constexpr char kSpotsEnabledKey[] = "IsSpotsEnabled";
constexpr char kSpotsEnabledDefault[] = "True";
constexpr int kRfPowerSettingsVersion = 1;
constexpr int kChainSettingsVersion = 2;
constexpr int kProcessingSettingsVersion = 4;

// Container item parity Task 61: the transmit setting buttons.
bool isTransmitSetting(Id id)
{
    switch (id) {
    case Id::Comp: case Id::Vox: case Id::Dexp: case Id::TxEq: case Id::Cfc: case Id::CfcEq:
    case Id::Leveler: case Id::PhaseRot:
    case Id::MicP1: case Id::MicM1: case Id::CompP1: case Id::CompM1:
    case Id::VoxP1: case Id::VoxM1:
    case Id::DriveP5: case Id::DriveM5: case Id::Drive0:
    case Id::TuneP5: case Id::TuneM5: case Id::Tun0:
        return true;
    default:
        return false;
    }
}

// The transmitSettingsVersion a transmit setting button's control needs.
int settingsVersionOf(Id id)
{
    switch (id) {
    case Id::CfcEq: case Id::PhaseRot:
        return kProcessingSettingsVersion;
    case Id::DriveP5: case Id::DriveM5: case Id::Drive0:
        return kRfPowerSettingsVersion;
    default:
        return kChainSettingsVersion;
    }
}

// Container item parity Task 62: the display toggles and ZTB, on the
// panadapter that shows the container's slice.
bool actsOnSlicePanadapter(Id id)
{
    switch (id) {
    case Id::Nf: case Id::PeakBlobs: case Id::CursorInfo: case Id::FillSpectrum:
    case Id::Ztb: case Id::ActivePeak:
        return true;
    default:
        return false;
    }
}

// Task 62: the display and window buttons.
bool isDisplayOrWindow(Id id)
{
    switch (id) {
    case Id::Spots: case Id::MafP5: case Id::MafM5:
    case Id::FormSetup: case Id::FormDiversity: case Id::FormLinearity:
        return true;
    default:
        return actsOnSlicePanadapter(id);
    }
}

// The receive buttons that change only the container's own slice.
bool actsOnSliceOnly(Id id)
{
    switch (id) {
    case Id::Nr: case Id::Nr1: case Id::Nr2: case Id::Nr3: case Id::Nr4:
    case Id::Nb: case Id::Nb1: case Id::Nb2:
    case Id::Rit: case Id::Rit0: case Id::Xit: case Id::Xit0: case Id::LockA:
    case Id::TuneStepUp: case Id::TuneStepDown: case Id::SqlVsql:
    case Id::AfP5: case Id::AfM5: case Id::BalP5: case Id::BalM5:
    case Id::AgcFixed: case Id::AgcLong: case Id::AgcSlow: case Id::AgcMedium:
    case Id::AgcFast: case Id::AgcCustom: case Id::AgcAuto:
    case Id::AgcP5: case Id::AgcM5:
        return true;
    default:
        return false;
    }
}

// NR1 to NR4 (Thetis SelectNR's 1-4) by button.
NrSlot nrOf(Id id)
{
    switch (id) {
    case Id::Nr1: return NrSlot::NR1;
    case Id::Nr2: return NrSlot::NR2;
    case Id::Nr3: return NrSlot::NR3;
    default:      return NrSlot::NR4;
    }
}

// The AGC mode an AGC button selects. Thetis FIXD (0) is AGCMode::Off
// (enums.cs:152-162 [v2.10.3.15]).
AGCMode agcModeOf(Id id)
{
    switch (id) {
    case Id::AgcFixed:  return AGCMode::Off;
    case Id::AgcLong:   return AGCMode::Long;
    case Id::AgcSlow:   return AGCMode::Slow;
    case Id::AgcMedium: return AGCMode::Med;
    case Id::AgcFast:   return AGCMode::Fast;
    default:            return AGCMode::Custom;
    }
}

// The lit state and caption of a receive button on `slice`, as Thetis's
// clsOtherButtons lights them (MeterManager.cs:8600-8737 [v2.10.3.15]).
void sliceButtonState(Id id, const SliceModel& slice, ContainerButtonDispatcher::State& st)
{
    switch (id) {
    case Id::Nr:
        st.on = slice.activeNr() != NrSlot::Off;
        st.label = OtherButtonItem::nrLabel(slice.activeNr());
        break;
    case Id::Nr1: case Id::Nr2: case Id::Nr3: case Id::Nr4:
        st.on = slice.activeNr() == nrOf(id);
        break;
    case Id::Nb:
        st.on = slice.nbMode() != NbMode::Off;
        st.label = OtherButtonItem::nbLabel(slice.nbMode());
        break;
    case Id::Nb1:
        st.on = slice.nbMode() == NbMode::NB;
        break;
    case Id::Nb2:
        st.on = slice.nbMode() == NbMode::NB2;
        break;
    case Id::Rit:     st.on = slice.ritEnabled(); break;
    case Id::Xit:     st.on = slice.xitEnabled(); break;
    case Id::LockA:   st.on = slice.locked(); break;
    case Id::SqlVsql: st.on = slice.ssqlEnabled(); break;
    case Id::AgcFixed: case Id::AgcLong: case Id::AgcSlow: case Id::AgcMedium:
    case Id::AgcFast: case Id::AgcCustom:
        st.on = slice.agcMode() == agcModeOf(id);
        break;
    case Id::AgcAuto: st.on = slice.autoAgcEnabled(); break;
    default:
        // RIT0, XIT0, TS+/TS-, AF+/-, BAL+/-, AGC+/-: momentary, never lit.
        break;
    }
}

// setActiveNr, with the slice's refusal (nrSelectionRefused) as the reason.
QString selectNr(SliceModel* slice, NrSlot slot)
{
    QString refusal;
    const QMetaObject::Connection refused = QObject::connect(
        slice, &SliceModel::nrSelectionRefused,
        [&refusal](const QString& reason) { refusal = reason; });
    slice->setActiveNr(slot);
    QObject::disconnect(refused);
    return refusal;
}

int vaxChannelOf(Id id)
{
    return id == Id::Vac1 ? 1 : 2;
}

QString vaxEnabledKey(int channel)
{
    // The key Setup > Audio > VAX's channel "On" switch saves.
    return QStringLiteral("audio/Vax%1/Enabled").arg(channel);
}

} // namespace

ContainerButtonDispatcher::ContainerButtonDispatcher(RadioModel* model, Hooks hooks)
    : m_model(model)
    , m_hooks(std::move(hooks))
{
}

SliceModel* ContainerButtonDispatcher::sliceFor(int rxSource) const
{
    if (!m_model) { return nullptr; }
    const int sliceId = ContainerWidget::sliceIdForRxSource(rxSource);
    SliceModel* slice = m_model->sliceById(sliceId);
    // Slice control plan Task 15 fix round 1: a slice this window listens
    // to (another device controls it) is refused as the RX applet refuses
    // it, in a remote window and a hosting one alike.
    if (slice && m_hooks.sliceRefusal && !m_hooks.sliceRefusal(sliceId).isEmpty()) {
        return nullptr;
    }
    if (slice && m_hooks.desktopHosting && m_hooks.desktopHosting()) {
        SliceOwnership* ownership = m_model->sliceOwnership();
        if (!ownership) { return nullptr; }
        // Slice control plan Task 2: a slice the station device may change
        // as its own (not one it runs held for an absent device).
        if (!SliceAccessPolicy::mayChange(*ownership, SliceOwnership::stationDevice(), sliceId)
            || ownership->mark(sliceId).isHeld()) {
            return nullptr;
        }
    }
    return slice;
}

QString ContainerButtonDispatcher::noSliceReason(int rxSource)
{
    return QStringLiteral("%1 is not open. Open it, or choose another slice in this "
                          "container's settings.")
        .arg(ContainerWidget::sliceNameForRxSource(rxSource));
}

QString ContainerButtonDispatcher::sliceUnavailableReason(int rxSource) const
{
    if (m_model && m_hooks.sliceRefusal) {
        const int sliceId = ContainerWidget::sliceIdForRxSource(rxSource);
        if (m_model->sliceById(sliceId)) {
            const QString refusal = m_hooks.sliceRefusal(sliceId);
            if (!refusal.isEmpty()) { return refusal; }
        }
    }
    if (m_model && m_hooks.desktopHosting && m_hooks.desktopHosting()) {
        const int sliceId = ContainerWidget::sliceIdForRxSource(rxSource);
        if (m_model->sliceById(sliceId)) {
            return QStringLiteral("%1 belongs to another device. Choose a Core slice in "
                                  "this container's settings.")
                .arg(ContainerWidget::sliceNameForRxSource(rxSource));
        }
    }
    return noSliceReason(rxSource);
}

QString ContainerButtonDispatcher::noRadioTransmitReason()
{
    return QStringLiteral("Connect a radio to transmit.");
}

QString ContainerButtonDispatcher::notchWhileTransmittingReason()
{
    return QStringLiteral("Notches cannot be added while transmitting.");
}

QString ContainerButtonDispatcher::noPanadapterReason()
{
    return QStringLiteral("No panadapter shows this slice.");
}

bool ContainerButtonDispatcher::spotsShownSetting()
{
    return AppSettings::instance()
               .value(QLatin1String(kSpotsEnabledKey), QLatin1String(kSpotsEnabledDefault))
               .toString()
        == QStringLiteral("True");
}

void ContainerButtonDispatcher::showSpotsOnPans(bool on, const QList<SpectrumWidget*>& pans)
{
    // SpotHubDialog's Spots switch: saved, then each pan re-reads it.
    AppSettings& settings = AppSettings::instance();
    settings.setValue(QLatin1String(kSpotsEnabledKey),
                      on ? QStringLiteral("True") : QStringLiteral("False"));
    settings.save();
    for (SpectrumWidget* pan : pans) {
        if (pan) { pan->loadSpotDisplaySettings(); }
    }
}

QList<SliceModel*> ContainerButtonDispatcher::changeableSlices() const
{
    QList<SliceModel*> result;
    if (!m_model) { return result; }
    for (SliceModel* slice : m_model->slices()) {
        // The same test as a container on that slice: open, and this
        // window's to change.
        if (slice && sliceFor(slice->sliceIndex() + 1) == slice) { result.append(slice); }
    }
    return result;
}

bool ContainerButtonDispatcher::transmitBlockedRemotely() const
{
    if (!m_model || m_model->ownsLocalDsp()) { return false; }
    return !(m_hooks.transmitPermitted && m_hooks.transmitPermitted());
}

SpectrumWidget* ContainerButtonDispatcher::spectrumOf(int rxSource) const
{
    SliceModel* slice = sliceFor(rxSource);
    if (!slice || !m_hooks.spectrumFor) { return nullptr; }
    return m_hooks.spectrumFor(slice);
}

ContainerButtonDispatcher::State
ContainerButtonDispatcher::stateOf(Id id, int rxSource) const
{
    State st;
    if (!m_model) {
        st.available = false;
        st.reason = noRadioTransmitReason();
        return st;
    }
    const auto unavailable = [&st](const QString& reason) {
        st.available = false;
        st.reason = reason;
    };
    QString remoteReason = m_hooks.remoteTransmitReasonNow ? m_hooks.remoteTransmitReasonNow()
                                                           : QString();
    if (remoteReason.isEmpty()) {
        remoteReason = m_hooks.remoteTransmitReason.isEmpty()
            ? QStringLiteral("Remote transmit controls are not available from this Core.")
            : m_hooks.remoteTransmitReason;
    }

    switch (id) {
    case Id::Mon:
        st.on = m_model->transmitModel().monEnabled();
        // R-R3-49 (parity Task 2): a transmit setting, not a key. A remote
        // window toggles the Core's monEnabled while the Core takes it.
        if (!m_model->ownsLocalDsp()
            && !(m_hooks.transmitSettingsPermitted && m_hooks.transmitSettingsPermitted())) {
            const QString reason = m_hooks.transmitSettingsReason
                ? m_hooks.transmitSettingsReason() : QString();
            unavailable(reason.isEmpty() ? remoteReason : reason);
        }
        break;
    case Id::Tun:
    case Id::Mox:
    case Id::TwoTon:
        if (m_model->role() == RadioModel::Role::Remote) {
            // Desktop remote transmit (R-IOS-13): the Core's state, as the
            // TX applet shows it; this window's own controller never keys.
            const PureSignalSessionFacade* ps = m_model->pureSignalFacade();
            st.on = id == Id::Tun ? m_model->transmitModel().isTune()
                  : id == Id::Mox ? m_model->isTransmitting()
                                  : ps && ps->twoToneOn();
        } else if (m_hooks.desktopHosting && m_hooks.desktopHosting()
                   && (id == Id::Tun || id == Id::Mox)) {
            st.on = id == Id::Tun ? (m_hooks.desktopTuneOn && m_hooks.desktopTuneOn())
                                  : (m_hooks.desktopMoxOn && m_hooks.desktopMoxOn());
        } else if (id == Id::Tun) {
            // Issue #357: the TUNE the release press clears, not isTune(),
            // which holds until the radio is back on receive.
            st.on = m_model->transmitModel().isTune();
        } else if (id == Id::Mox) {
            st.on = m_model->moxController() && m_model->moxController()->isMox();
        } else {
            st.on = m_model->twoToneController() && m_model->twoToneController()->isActive();
        }
        // Task 16: receive only disables TUN, 2TONE and MOX, as Thetis
        // console.RXOnly does (console.cs:15318-15323 [v2.10.3.15]; the
        // 2TONE line carries // MW0LGE_21a). MOX too in SPEC and DRM, where
        // Thetis leaves it alone (RadioModel::receiveOnlyDisablesMoxButton,
        // fix wave I3). First, since it holds however this window reaches
        // the radio; with a remote window's missing transmit as well, both
        // reasons show, so turning receive only off does not leave the
        // button blocked for a reason never named (fix wave M6).
        // HL2 port part 2: a TX inhibit the same way, with its reason (the
        // HL2 I/O board's fault code), as Thetis's TXInhibit setter
        // disables them (console.cs:15341-15363 [v2.10.3.15]):
        //   chkTUN.Enabled = !_tx_inhibit;
        //   chk2TONE.Enabled = !_tx_inhibit; //MW0LGE_21a
        if (m_model->transmitButtonsLocked()
            && (id != Id::Mox || m_model->transmitLockCoversMox())) {
            unavailable(m_model->transmitLockReasonAlongside(
                transmitBlockedRemotely() ? remoteReason : QString()));
        } else if (transmitBlockedRemotely()) {
            unavailable(remoteReason);
        } else if (!m_model->isConnected()) {
            // Fix round 1 (minor 4): a remote window says which link is
            // down (RadioModel::transmitLinkDownReason).
            unavailable(m_model->ownsLocalDsp() ? noRadioTransmitReason()
                                                : m_model->transmitLinkDownReason());
        } else if ((id == Id::Mox && !m_model->moxController())
                   || (id == Id::TwoTon && !m_model->twoToneController())) {
            unavailable(noRadioTransmitReason());
        }
        break;
    case Id::PsA: {
        PureSignalSessionFacade* ps = m_model->pureSignalFacade();
        st.on = ps && ps->settings() && ps->settings()->autoCalEnabled();
        // R-R3-49 (parity Task 7): arming keys nothing. A remote window
        // arms on a Core that takes it (off the air), else says why.
        const bool armingBlockedRemotely = !m_model->ownsLocalDsp()
            && !(m_hooks.pureSignalArmingPermitted && m_hooks.pureSignalArmingPermitted());
        if (armingBlockedRemotely) {
            const QString reason = m_hooks.pureSignalArmingReason
                ? m_hooks.pureSignalArmingReason() : QString();
            unavailable(reason.isEmpty() ? remoteReason : reason);
        } else if (!ps || !ps->available() || !ps->canArm()) {
            // Fix wave GUI-I7: the facade's own reason (on the air, or no
            // connected radio that supports PureSignal).
            const QString refusal = ps ? ps->armingRefusal() : QString();
            unavailable(refusal.isEmpty() ? PureSignalSessionFacade::needsRadioReason()
                                          : refusal);
        }
        break;
    }
    case Id::Anf:
    case Id::Snb:
    case Id::Mute:
    case Id::Bin: {
        SliceModel* slice = sliceFor(rxSource);
        if (!slice) {
            unavailable(sliceUnavailableReason(rxSource));
            break;
        }
        st.on = id == Id::Anf    ? slice->anfEnabled()
              : id == Id::Snb    ? slice->snbEnabled()
              : id == Id::Mute   ? slice->muted()
                                 : slice->binauralEnabled();
        break;
    }
    case Id::Mnf:
        st.on = m_model->notchModel() && m_model->notchModel()->globalEnabled();
        if (!m_model->notchModel()) {
            unavailable(QStringLiteral("Notches are not ready."));
        }
        break;
    case Id::PeakHold:
    case Id::Ctun: {
        if (!sliceFor(rxSource)) {
            unavailable(sliceUnavailableReason(rxSource));
            break;
        }
        SpectrumWidget* sw = spectrumOf(rxSource);
        if (!sw) {
            unavailable(noPanadapterReason());
            break;
        }
        if (id == Id::PeakHold) {
            st.on = sw->peakHoldEnabled();
        } else {
            st.on = sw->ctunEnabled();
            if (!sw->ctunAvailable()) {
                unavailable(QStringLiteral("C-Tune needs a connected Core that supports it."));
            } else if (!sw->ctunHeldReason().isEmpty()) {
                // Another holder's receiver, or a slice this window only
                // listens to: the Core's words for why.
                unavailable(sw->ctunHeldReason());
            }
        }
        break;
    }
    case Id::Dup: {
        // Parity Task 31 (A11): display duplex, the window's DisplayDuplex
        // (View > Display duplex (DUP) is the same setting). Thetis's
        // container DUP button toggles chkRX2SR, "chkRX2SR is the DUPlex
        // button" (console.cs:37555-37575 [v2.10.3.15];
        // ucOtherButtonsOptionsGrid.cs:540 [v2.10.3.15], "Duplex mode, view
        // the tx rx"). Not a transmit setting: it changes what this window
        // shows, so it works on and off the air.
        st.on = m_hooks.displayDuplexOn && m_hooks.displayDuplexOn();
        const QString reason = m_hooks.displayDuplexReason ? m_hooks.displayDuplexReason()
                                                           : QString();
        if (!m_hooks.setDisplayDuplex) {
            unavailable(QStringLiteral("This button does nothing in this version."));
        } else if (!reason.isEmpty()) {
            unavailable(reason);
        }
        break;
    }
    case Id::Vac1:
    case Id::Vac2:
        if (!m_hooks.vaxDevices) {
            unavailable(QStringLiteral("This computer's sound devices are not ready."));
            break;
        }
        st.on = m_hooks.vaxDevices->isVaxBusOpen(vaxChannelOf(id));
        break;
    case Id::MnfPlus:
        // Task 60: TNFAdd on the container's slice (Thetis always RX1,
        // "// no rx2", console.cs:51403 [v2.10.3.15]). Never lit. Refused
        // while transmitting, as the filter display's +MNF is.
        if (!sliceFor(rxSource)) {
            unavailable(sliceUnavailableReason(rxSource));
        } else if (!m_model->notchModel()) {
            unavailable(QStringLiteral("Notches are not ready."));
        } else if (m_model->isTransmitting()) {
            unavailable(notchWhileTransmittingReason());
        }
        break;
    case Id::MuteAll: {
        // Task 60: GetMute(0), every receiver muted (console.cs:52208-52229
        // [v2.10.3.15]), over the slices this window may change.
        if (!sliceFor(rxSource)) {
            unavailable(sliceUnavailableReason(rxSource));
            break;
        }
        const QList<SliceModel*> slices = changeableSlices();
        st.on = !slices.isEmpty()
            && std::all_of(slices.cbegin(), slices.cend(),
                           [](const SliceModel* s) { return s->muted(); });
        break;
    }
    default:
        if (isTransmitSetting(id)) {
            transmitSettingState(id, st);
            break;
        }
        if (isDisplayOrWindow(id)) {
            displayOrWindowState(id, rxSource, st);
            break;
        }
        if (actsOnSliceOnly(id)) {
            // Task 60: a receive button on the container's own slice.
            SliceModel* slice = sliceFor(rxSource);
            if (!slice) {
                unavailable(sliceUnavailableReason(rxSource));
            } else {
                sliceButtonState(id, *slice, st);
            }
            break;
        }
        // Hidden until its feature is built (OtherButtonItem).
        unavailable(QStringLiteral("This button does nothing in this version."));
        break;
    }
    return st;
}

void ContainerButtonDispatcher::apply(OtherButtonItem* item, int rxSource) const
{
    if (!item) { return; }
    for (Id id : kConnected) {
        const State st = stateOf(id, rxSource);
        item->setButtonState(id, st.on);
        item->setButtonAvailable(id, st.available, st.reason);
        item->setButtonLabel(id, st.label);
    }
}

QString ContainerButtonDispatcher::click(Id id, int rxSource)
{
    const State st = stateOf(id, rxSource);
    if (!st.available) { return st.reason; }
    const bool turnOn = !st.on;

    switch (id) {
    case Id::Mon:
        // TxApplet's MON button (TransmitModel::setMonEnabled).
        m_model->transmitModel().setMonEnabled(turnOn);
        break;
    case Id::Tun:
        // TxApplet's TUNE button (RadioModel::setTune).
        if (m_hooks.desktopHosting && m_hooks.desktopHosting()
            && m_hooks.requestDesktopTune) {
            m_hooks.requestDesktopTune(turnOn);
        } else {
            // TX rulings (item 1): against a remote window's own TUNE.
            m_model->setTune(m_model->tunePressAsksOn(turnOn));
        }
        break;
    case Id::Mox:
        // TxApplet's MOX button: a manual key, with TUN and two-tone turned
        // off on the way off (RadioModel::setMoxFromButton, Task 7).
        if (m_hooks.desktopHosting && m_hooks.desktopHosting()
            && m_hooks.requestDesktopMox) {
            m_hooks.requestDesktopMox(turnOn);
        } else {
            // TX rulings (item 1): against a remote window's own key.
            m_model->setMoxFromButton(m_model->moxPressAsksOn(turnOn));
        }
        break;
    case Id::TwoTon:
        // TxApplet's 2-TONE button (RadioModel::setTwoTone: the
        // TwoToneController here, the Core's in a remote window).
        // Fix wave (hosting 2-TONE parity): a hosting window asks to take
        // transmit first, as for MOX and TUNE.
        if (m_hooks.desktopHosting && m_hooks.desktopHosting()
            && m_hooks.requestDesktopTwoTone) {
            m_hooks.requestDesktopTwoTone(turnOn);
        } else {
            m_model->setTwoTone(turnOn);
        }
        break;
    case Id::PsA:
        // TxApplet's PS-A button: automatic calibration on, or Off/reset.
        m_model->pureSignalFacade()->requestAction(
            turnOn ? Ps3Action::StartAutomatic : Ps3Action::OffReset);
        break;
    case Id::Anf:
        sliceFor(rxSource)->setAnfEnabled(turnOn);
        break;
    case Id::Snb:
        sliceFor(rxSource)->setSnbEnabled(turnOn);
        break;
    case Id::Mute:
        sliceFor(rxSource)->setMuted(turnOn);
        break;
    case Id::Dup:
        m_hooks.setDisplayDuplex(turnOn);
        break;
    case Id::Bin:
        sliceFor(rxSource)->setBinauralEnabled(turnOn);
        break;
    case Id::Mnf:
        m_model->notchModel()->setGlobalEnabled(turnOn);
        break;
    case Id::PeakHold:
        spectrumOf(rxSource)->setPeakHoldEnabled(turnOn);
        break;
    case Id::Ctun:
        // The pan overlay's C-Tune switch; a remote window routes it to
        // the Core (RemoteMediaController follows ctunEnabledChanged).
        spectrumOf(rxSource)->setCtunEnabled(turnOn);
        break;
    case Id::Vac1:
    case Id::Vac2: {
        // Setup > Audio > VAX's channel "On" switch: saved, then the
        // channel opened or closed on this computer.
        const int channel = vaxChannelOf(id);
        AppSettings::instance().setValue(vaxEnabledKey(channel),
            turnOn ? QStringLiteral("True") : QStringLiteral("False"));
        AppSettings::instance().save();
        m_hooks.vaxDevices->setVaxEnabled(channel, turnOn);
        break;
    }
    case Id::MnfPlus:
        // RadioModel::addTnfForSlice, the pan strip's +TNF; a refused add
        // reaches the window through NotchModel::notchAddRejected.
        m_model->addTnfForSlice(sliceFor(rxSource));
        break;
    case Id::MuteAll:
        // From Thetis console.cs:51421 [v2.10.3.15]:
        //   case OtherButtonId.MUTE_ALL: SetMute(0, !GetMute(0)); break;
        for (SliceModel* slice : changeableSlices()) { slice->setMuted(turnOn); }
        break;
    default:
        if (isTransmitSetting(id)) { return clickTransmitSetting(id, rxSource); }
        if (isDisplayOrWindow(id)) {
            clickDisplayOrWindow(id, rxSource, turnOn);
            break;
        }
        if (actsOnSliceOnly(id)) { return clickSlice(id, sliceFor(rxSource)); }
        return st.reason;
    }
    return QString();
}

QString ContainerButtonDispatcher::clickSlice(Id id, SliceModel* slice)
{
    switch (id) {
    case Id::Nr: {
        // From Thetis console.cs:43333-43334 [v2.10.3.15] (incrementNR):
        //   _nr_selected[rx - 1] += 1;
        //   if (_nr_selected[rx - 1] > 4) _nr_selected[rx - 1] = 0;
        // A NereusSDR-only reducer (above NR4) goes to off the same way.
        int next = static_cast<int>(slice->activeNr()) + 1;
        if (next > kLastThetisNr) { next = static_cast<int>(NrSlot::Off); }
        return selectNr(slice, static_cast<NrSlot>(next));
    }
    case Id::Nr1: case Id::Nr2: case Id::Nr3: case Id::Nr4:
        // From Thetis console.cs:51425-51444 [v2.10.3.15]:
        //   if (GetSelectedNR(rx) == 1) { SelectNR(rx, true, 0); } else { SelectNR(rx, true, 1); }
        return selectNr(slice, slice->activeNr() == nrOf(id) ? NrSlot::Off : nrOf(id));
    case Id::Nb:
        // Thetis console.cs:51393-51400 [v2.10.3.15]: Off, NB, NB2, Off.
        slice->setNbMode(cycleNbMode(slice->nbMode()));
        break;
    case Id::Nb1:
    case Id::Nb2: {
        // From Thetis console.cs:51445-51454 [v2.10.3.15]:
        //   if (GetSelectedNB(rx) == 1) { SetSelectedNB(rx, 0); } else { SetSelectedNB(rx, 1); }
        const NbMode mode = id == Id::Nb1 ? NbMode::NB : NbMode::NB2;
        slice->setNbMode(slice->nbMode() == mode ? NbMode::Off : mode);
        break;
    }
    // Thetis console.cs:51503-51506, :52896-52907 [v2.10.3.15]: RITOn,
    // XITOn and the reset buttons (udRIT.Value = 0), per slice here.
    case Id::Rit:  slice->setRitEnabled(!slice->ritEnabled()); break;
    case Id::Xit:  slice->setXitEnabled(!slice->xitEnabled()); break;
    case Id::Rit0: slice->setRitHz(0); break;
    case Id::Xit0: slice->setXitHz(0); break;
    // Thetis console.cs:51611 [v2.10.3.15], VFOALock = !VFOALock: the
    // container's slice here.
    case Id::LockA: slice->setLocked(!slice->locked()); break;
    // Thetis console.cs:51613-51614 [v2.10.3.15], TuneStepIndex += 1 / -= 1:
    // the slice's own step here.
    case Id::TuneStepUp:   slice->changeTuneStepUp(); break;
    case Id::TuneStepDown: slice->changeTuneStepDown(); break;
    // Thetis console.cs:51510 [v2.10.3.15]: voice squelch on or off.
    case Id::SqlVsql: slice->setSsqlEnabled(!slice->ssqlEnabled()); break;
    case Id::AfP5:
    case Id::AfM5:
        slice->setAfGain(std::clamp(slice->afGain() + (id == Id::AfP5 ? kAfStep : -kAfStep),
                                    ControlRanges::kAfGainMin, ControlRanges::kAfGainMax));
        break;
    case Id::BalP5:
    case Id::BalM5:
        slice->setAudioPan(std::clamp(slice->audioPan() + (id == Id::BalP5 ? kBalStep : -kBalStep),
                                      kPanLeft, kPanRight));
        break;
    // Thetis console.cs:51477-51483 [v2.10.3.15]: SetAGCMode, SetAGCAuto.
    case Id::AgcFixed: case Id::AgcLong: case Id::AgcSlow: case Id::AgcMedium:
    case Id::AgcFast: case Id::AgcCustom:
        slice->setAgcMode(agcModeOf(id));
        break;
    case Id::AgcAuto: slice->setAutoAgcEnabled(!slice->autoAgcEnabled()); break;
    case Id::AgcP5:
    case Id::AgcM5:
        slice->setAgcThreshold(std::clamp(
            slice->agcThreshold() + (id == Id::AgcP5 ? kAgcThresholdStep : -kAgcThresholdStep),
            ControlRanges::kAgcThresholdMinDb, ControlRanges::kAgcThresholdMaxDb));
        break;
    default:
        break;
    }
    return QString();
}

QString ContainerButtonDispatcher::settingsRefusal(int version) const
{
    if (m_hooks.transmitSettingsRefusal) { return m_hooks.transmitSettingsRefusal(version); }
    return m_model->ownsLocalDsp() ? QString() : IStationLink::transmitSettingsUnavailableReason();
}

void ContainerButtonDispatcher::transmitSettingState(Id id, State& st) const
{
    const TransmitModel& tx = m_model->transmitModel();
    // As Thetis's clsOtherButtons lights them (GetGeneralSetting,
    // console.cs:52669-52700 [v2.10.3.15]); the steps never light.
    switch (id) {
    case Id::Comp:     st.on = tx.cpdrOn(); break;
    case Id::Vox:      st.on = tx.voxEnabled(); break;
    case Id::Dexp:     st.on = tx.dexpEnabled(); break;
    case Id::TxEq:     st.on = tx.txEqEnabled(); break;
    case Id::Cfc:      st.on = tx.cfcEnabled(); break;
    case Id::CfcEq:    st.on = tx.cfcPostEqEnabled(); break;
    case Id::Leveler:  st.on = tx.txLevelerOn(); break;
    case Id::PhaseRot: st.on = tx.phaseRotatorEnabled(); break;
    default: break;
    }
    QString reason;
    if (id == Id::Vox) {
        // The TX applet's VOX button: remote transmit and this computer's
        // microphone line first (voxRefusal), with receive only or a TX
        // inhibit on top (TxApplet::applyReceiveOnlyLock), then the chain
        // gate its VOX level follows.
        const QString vox = m_hooks.voxRefusal ? m_hooks.voxRefusal() : QString();
        if (m_model->transmitButtonsLocked() && m_model->transmitLockCoversVox()) {
            reason = m_model->transmitLockReasonAlongside(vox);
        } else {
            reason = vox;
        }
    }
    if (reason.isEmpty()) { reason = settingsRefusal(settingsVersionOf(id)); }
    if (!reason.isEmpty()) {
        st.available = false;
        st.reason = reason;
    }
}

void ContainerButtonDispatcher::displayOrWindowState(Id id, int rxSource, State& st) const
{
    const auto unavailable = [&st](const QString& reason) {
        st.available = false;
        st.reason = reason;
    };
    const QString noHook = QStringLiteral("This button does nothing in this version.");
    if (actsOnSlicePanadapter(id)) {
        // As Peak and CTUN: the panadapter that shows the container's slice.
        if (!sliceFor(rxSource)) {
            unavailable(sliceUnavailableReason(rxSource));
            return;
        }
        const SpectrumWidget* sw = spectrumOf(rxSource);
        if (!sw) {
            unavailable(noPanadapterReason());
            return;
        }
        // As Thetis's clsOtherButtons lights them (GetGeneralSetting,
        // console.cs:52702-52716, :52763 [v2.10.3.15]), from this pan.
        switch (id) {
        case Id::Nf:           st.on = sw->showNoiseFloor(); break;
        case Id::PeakBlobs:    st.on = sw->peakBlobsEnabled(); break;
        case Id::CursorInfo:   st.on = sw->cursorFreqVisible(); break;
        case Id::FillSpectrum: st.on = sw->panFillEnabled(); break;
        case Id::ActivePeak:   st.on = sw->activePeakHoldEnabled(); break;
        default:               break;  // ZTB: momentary, never lit.
        }
        return;
    }
    switch (id) {
    case Id::Spots:
        // Not the slice's: every panadapter shows spots or none does.
        if (!m_hooks.setSpotsShown) {
            unavailable(noHook);
            return;
        }
        st.on = m_hooks.spotsShown && m_hooks.spotsShown();
        return;
    case Id::MafP5:
    case Id::MafM5:
        // This computer's master volume, never lit. The slider is live in
        // every window, remote ones included, so the buttons are too.
        if (!m_hooks.stepMasterVolume) { unavailable(noHook); }
        return;
    case Id::FormSetup:
        // File > Settings... opens Setup in every state (createSetupDialog).
        if (!m_hooks.openSetup) { unavailable(noHook); }
        return;
    case Id::FormDiversity:
        // Tools > Diversity... is enabled in every window.
        if (!m_hooks.openDiversity) { unavailable(noHook); }
        return;
    case Id::FormLinearity: {
        // Tools > PureSignal...: disabled, with its reason, while it is.
        if (!m_hooks.openLinearity) {
            unavailable(noHook);
            return;
        }
        const QString refusal = m_hooks.linearityRefusal ? m_hooks.linearityRefusal() : QString();
        if (!refusal.isEmpty()) { unavailable(refusal); }
        return;
    }
    default:
        return;
    }
}

void ContainerButtonDispatcher::clickDisplayOrWindow(Id id, int rxSource, bool turnOn)
{
    // Thetis's are console-wide; here each acts on the panadapter that
    // shows the container's slice, through the setter its Display pill
    // (SpectrumOverlayPanel) or Setup's per-pan row calls. Peak Blobs and
    // A-Peak are saved as that pan's own value, as Setup's Peaks page does
    // for a selected pan, not as the master.
    SpectrumWidget* sw = actsOnSlicePanadapter(id) ? spectrumOf(rxSource) : nullptr;
    switch (id) {
    // From Thetis console.cs:51618 [v2.10.3.15]:
    //   case OtherButtonId.NF: SetNFEnabled(rx, !GetNFEnabled(rx)); break;
    case Id::Nf: sw->setShowNoiseFloor(turnOn); break;
    // From Thetis console.cs:52881-52895 [v2.10.3.15] (DoGeneralSettingAction):
    //   SetupForm.PeakBlobsEnabled = state;
    //   SetupForm.ShowDisplayMHzCursorInfo = state;
    //   SetupForm.ShowTCISpots = state;
    //   SetupForm.SetActivePeakHoldsEnabledRX(rx, state);
    //   SetupForm.DisplayPanFill = state;
    case Id::PeakBlobs:    sw->setPeakBlobsEnabled(turnOn); break;
    case Id::CursorInfo:   sw->setCursorFreqVisible(turnOn); break;
    case Id::Spots:        m_hooks.setSpotsShown(turnOn); break;
    case Id::ActivePeak:   sw->setActivePeakHoldEnabled(turnOn); break;
    case Id::FillSpectrum: sw->setPanFillEnabled(turnOn); break;
    // From Thetis console.cs:51548 [v2.10.3.15]:
    //   case OtherButtonId.ZTB: ZoomToBand(button == MouseButtons.Right); break;
    // A left click (a container button takes no other), the pan's ZTB.
    case Id::Ztb: sw->zoomToBand(); break;
    case Id::MafP5: m_hooks.stepMasterVolume(kMasterVolumeStep); break;
    case Id::MafM5: m_hooks.stepMasterVolume(-kMasterVolumeStep); break;
    // From Thetis console.cs:51581, :51594-51595 [v2.10.3.15]:
    //   case OtherButtonId.FORM_SETUP: setupToolStripMenuItem1_Click(this, EventArgs.Empty); break;
    //   case OtherButtonId.FORM_DIVERSITY: showHideDiversity(true); break;
    //   case OtherButtonId.FORM_LINEARITY: linearityToolStripMenuItem_Click(this, EventArgs.Empty); break;
    case Id::FormSetup:     m_hooks.openSetup(); break;
    case Id::FormDiversity: m_hooks.openDiversity(); break;
    case Id::FormLinearity: m_hooks.openLinearity(); break;
    default: break;
    }
}

Band ContainerButtonDispatcher::tunePowerBand(int rxSource) const
{
    // The TX applet's m_currentBand (a pan's band) stands in for the
    // transmit band until RadioModel knows it; here the container's slice,
    // or the active slice.
    SliceModel* slice = m_model->sliceById(ContainerWidget::sliceIdForRxSource(rxSource));
    if (!slice) { slice = m_model->activeSlice(); }
    return slice ? bandFromFrequency(slice->frequency()) : Band::Band20m;
}

QString ContainerButtonDispatcher::clickTransmitSetting(Id id, int rxSource)
{
    TransmitModel& tx = m_model->transmitModel();
    const HPSDRModel radio = m_model->hardwareProfile().model;
    switch (id) {
    // From Thetis console.cs:52848-52880 [v2.10.3.15] (DoGeneralSettingAction):
    //   CPDR = state; VOXEnable = state; NoiseGateEnabled = state;
    //   TXEQ = state; CFCEnabled = state; SetupForm.CFCPEQEnabled = state;
    //   SetupForm.TXLevelerOn = state; SetupForm.PhaseRotEnabled = state;
    // through the setters the Phone/CW applet's PROC and DEXP, the TX
    // applet's VOX, EQ, CFC and LEV, and Setup > DSP > CFC call.
    case Id::Comp:     tx.setCpdrOn(!tx.cpdrOn()); break;
    case Id::Vox:      tx.setVoxEnabled(!tx.voxEnabled()); break;
    case Id::Dexp:     tx.setDexpEnabled(!tx.dexpEnabled()); break;
    case Id::TxEq:     tx.setTxEqEnabled(!tx.txEqEnabled()); break;
    case Id::Cfc:      tx.setCfcEnabled(!tx.cfcEnabled()); break;
    case Id::CfcEq:    tx.setCfcPostEqEnabled(!tx.cfcPostEqEnabled()); break;
    case Id::Leveler:  tx.setTxLevelerOn(!tx.txLevelerOn()); break;
    case Id::PhaseRot: tx.setPhaseRotatorEnabled(!tx.phaseRotatorEnabled()); break;
    case Id::MicP1:
    case Id::MicM1: {
        // The Phone/CW applet's mic gain slider, ranged by the board.
        const BoardCapabilities& caps = m_model->boardCapabilities();
        tx.setMicGainDb(std::clamp(
            tx.micGainDb() + (id == Id::MicP1 ? kMicGainStepDb : -kMicGainStepDb),
            caps.micGainMinDb, caps.micGainMaxDb));
        break;
    }
    case Id::CompP1:
    case Id::CompM1:
        // The Phone/CW applet's PROC slider (0 to 20 dB).
        tx.setCpdrLevelDb(std::clamp(
            tx.cpdrLevelDb() + (id == Id::CompP1 ? kCpdrLevelStepDb : -kCpdrLevelStepDb),
            TransmitModel::kCpdrLevelDbMin, TransmitModel::kCpdrLevelDbMax));
        break;
    case Id::VoxP1:
    case Id::VoxM1:
        // The TX applet's VOX level slider (-80 to 0 dB).
        tx.setVoxThresholdDb(std::clamp(
            tx.voxThresholdDb() + (id == Id::VoxP1 ? kVoxThresholdStepDb : -kVoxThresholdStepDb),
            TransmitModel::kVoxThresholdDbMin, TransmitModel::kVoxThresholdDbMax));
        break;
    case Id::DriveP5:
    case Id::DriveM5:
    case Id::Drive0: {
        // The TX applet's RF Power slider (TransmitModel::setPower), ranged
        // as TxPowerSection::rescalePowerSlidersForModel ranges it.
        const int target = id == Id::Drive0 ? kPowerZero
            : tx.power() + (id == Id::DriveP5 ? kDriveStep : -kDriveStep);
        tx.setPower(std::clamp(target, kPowerZero, rfPowerSliderMaxFor(radio)));
        break;
    }
    case Id::TuneP5:
    case Id::TuneM5:
    case Id::Tun0: {
        // The TX applet's Tune Power slider: the transmit band's tune power
        // (TxApplet::shownTunePower), ranged by tuneSliderMaxFor.
        const Band band = tunePowerBand(rxSource);
        const bool remote = m_model->role() == RadioModel::Role::Remote;
        const int current = (remote || tx.tuneTxBandKnown()) ? tx.tunePowerForTxBand()
                                                             : tx.tunePowerForBand(band);
        const int target = std::clamp(
            id == Id::Tun0 ? kPowerZero
                           : current + (id == Id::TuneP5 ? kTunePowerStep : -kTunePowerStep),
            kPowerZero, tuneSliderMaxFor(radio));
        if (remote) {
            // TxApplet::requestRemoteTunePower: the Core sets its own
            // transmit band's; refused, the window shows the Core's again.
            IStationLink* link = m_model->stationLink();
            const IStationLink::CommandOutcome outcome = link
                ? link->requestTunePowerForTxBand(target)
                : IStationLink::CommandOutcome{false, IStationLink::transmitSettingsUnavailableReason()};
            if (!outcome.sent) {
                tx.reportTunePowerForTxBandRefused();
                return outcome.reason.isEmpty() ? IStationLink::transmitSettingsUnavailableReason()
                                                : outcome.reason;
            }
            break;
        }
        if (!tx.setTunePowerForTxBand(target)) { tx.setTunePowerForBand(band, target); }
        break;
    }
    default:
        break;
    }
    return QString();
}

void ContainerButtonDispatcher::applySliceAvailability(ButtonBoxItem* box, int rxSource) const
{
    if (!box) { return; }
    if (sliceFor(rxSource)) {
        box->setAllButtonsAvailable(true);
    } else {
        box->setAllButtonsAvailable(false, sliceUnavailableReason(rxSource));
    }
}

void ContainerButtonDispatcher::applyBand(BandButtonItem* item, int rxSource) const
{
    if (!item) { return; }
    applySliceAvailability(item, rxSource);
    SliceModel* slice = sliceFor(rxSource);
    // R-IOS-26: a remote window's 2 m button, disabled with the reason
    // when its Core does not have the 2 m band. The reason stays with the
    // band in whichever group shows it.
    QString reason2m;
    if (slice && m_model->stationLink() != nullptr) {
        reason2m = m_model->stationLink()->band2mUnavailableReason();
    }
    item->setBandUnavailableReason(Band::Band2m, reason2m);
    // The slice's own band group (each slice keeps its own).
    if (slice) {
        item->setSliceBand(slice->sliceIndex(),
                           BandDefaults::bandGridBandFromFrequency(slice->frequency()));
    } else {
        item->clearSliceBand();
    }
}

QString ContainerButtonDispatcher::clickBand(int band, int rxSource)
{
    SliceModel* slice = sliceFor(rxSource);
    if (!slice) { return sliceUnavailableReason(rxSource); }
    if (band < 0 || band >= static_cast<int>(Band::Count)) { return QString(); }
    m_model->onBandButtonClicked(slice, static_cast<Band>(band));
    return QString();
}

} // namespace NereusSDR
