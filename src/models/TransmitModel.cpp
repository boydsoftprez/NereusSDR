// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
// merge native EQ/CFC transactions with latest Core/remote profile ownership.
// =================================================================
// src/models/TransmitModel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// Ported from mi0bot-Thetis source:
//   Project Files/Source/Console/console.cs:47660-47673, 47666, 47775-47778
//     (HL2 setPowerUsingTargetDbm tune-slider sub-step DSP modulation;
//      setTxPostGenToneMag property/signal; computeAudioVolume HL2 branch)
//   original licence from mi0bot-Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-26 — tunePowerByBand[14] + per-MAC persistence (G.3, Phase 3M-1a)
//                 ported by J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-27 — micGainDb (int) + derived micPreampLinear (double) (C.1, Phase 3M-1b)
//                 ported by J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-27 — 8 mic-jack flag properties: micMute / micBoost / micXlr /
//                 lineIn / lineInBoost / micTipRing / micBias / micPttDisabled
//                 (C.2, Phase 3M-1b) ported by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-27 — VOX properties: voxEnabled / voxThresholdDb / voxGainScalar /
//                 voxHangTimeMs (C.3, Phase 3M-1b) ported by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//   2026-04-27 — Anti-VOX properties: antiVoxGainDb / antiVoxSourceVax
//                 (C.4, Phase 3M-1b) ported by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 (antiVoxSourceVax subsequently removed in 3M-3a-iv
//                 post-bench refactor — see 2026-05-07 entry below.)
//   2026-04-27 — MON properties: monEnabled / monitorVolume
//                 (C.5, Phase 3M-1b) ported by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-04-28 — micSource (MicSource) property (I.1, Phase 3M-1b)
//                 NereusSDR-native Setup UI property, J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//   2026-04-28 — PC Mic session state: pcMicHostApiIndex / pcMicDeviceName /
//                 pcMicBufferSamples transient properties (I.2, Phase 3M-1b)
//                 NereusSDR-native, J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-22 : R-R3-36 Task 6: the three PC Mic properties become
//                 projections of the audio/TxInput config (RadioModel
//                 wiring). NereusSDR-native, J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-04-28 — AppSettings per-MAC persistence for 15 mic/VOX/MON properties
//                 (L.2, Phase 3M-1b): loadFromSettings(mac) / persistToSettings(mac)
//                 + auto-persist on each setter via persistOne().
//                 NereusSDR-native persistence glue, J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//   2026-04-28 — setMicSourceLocked(bool) lock guard (L.3, Phase 3M-1b): HL2
//                 force-Pc-on-connect model-side lock. When locked,
//                 setMicSource(MicSource::Radio) silently coerces to Pc.
//                 NereusSDR-native, J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-04-28 — Two-tone test properties (B.2, Phase 3M-1c): 7 setter
//                 implementations + per-MAC AppSettings load/persist for
//                 TwoToneFreq1/Freq2/Level/Power/Freq2Delay/Invert/Pulsed.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-04-28 — DrivePowerSource string conversions + setter +
//                 TwoToneDrivePowerOrigin AppSettings load/persist
//                 (B.3, Phase 3M-1c).  J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-05-07 — Phase 3M-3a-iv post-bench refactor (Option A): removed
//                 setAntiVoxSourceVax / antiVoxSourceVaxChanged + the
//                 AntiVox_Source_VAX persistence read/write.  Existing
//                 user settings carrying this key will leave it as an
//                 orphan in AppSettings; ignored on load (no migration).
//                 NereusSDR-architectural divergence from Thetis
//                 chkAntiVoxSource at setup.designer.cs:44646-44657
//                 [v2.10.3.13]; see commit message for rationale.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-46 fix wave: the stored tune power stays within the
//                 model's range; follow-up 2026-09-24: clamped and saved at
//                 loadFromSettings for the radio loaded, never at
//                 setHpsdrModel (which ran before the load and saved under
//                 the previous radio). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-24 - R-R3-49 (parity Task 2): tunePowerForTxBand (the
//                 transmit band's tune power) with setTuneTxBand,
//                 setTunePowerForTxBand and a window's applyStationValue;
//                 settingRangeRefusal() for the mirrored transmit settings.
//                 NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 3): the Core's TX profiles
//                 (setStationTxProfiles, txProfileNamesFromJson, a
//                 window's applyStationValue) and the Line In gain range
//                 in settingRangeRefusal(). NereusSDR-original. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 4): txEqUseLegacy (Thetis
//                 EQUseLegacy, setup.cs:3615 and 9318 [v2.10.3.15]) with a
//                 one-time seed from the old per-computer key, the band
//                 arrays as JSON for the link, and the EQ, CFC, phase
//                 rotator, leveler and ALC ranges in settingRangeRefusal().
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 5): the per-band power and tune
//                 power as JSON objects for the link, and the DEXP,
//                 anti-VOX, two-tone and per-band power ranges in
//                 settingRangeRefusal(). NereusSDR-original. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-49 (group A fix wave, M4): settingRangeRefusal()
//                 refuses a txEqParaEqData or cfcParaEqData value the curve
//                 loader cannot read. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

// --- From console.cs (Thetis v2.10.3.13) ---

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

// --- From mi0bot-Thetis console.cs [v2.10.3.13-beta2] ---

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
// Modification history (NereusSDR) — continued:
//   2026-05-02 — filterLow / filterHigh properties + filterChanged signal
//                 + filterDisplayText + per-MAC persistence under
//                 hardware/<mac>/tx/FilterLow and FilterHigh.
//                 NereusSDR-original (Plan 4 Cluster A, Task 2/D1).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-05-03 — Phase 3 Agent 3A of issue #167 (PA-cal hotfix scaffolding):
//                 m_powerByBand[14] (default 50 W; per-band normal-mode
//                 power array parallel to m_tunePowerByBand) +
//                 powerForBand / setPowerForBand + powerByBandChanged
//                 signal; 3 Thetis ATT-on-TX-on-power-change safety
//                 properties (forceAttwhenPSAoff,
//                 forceAttwhenPowerChangesWhenPSAon, _anddecreased) +
//                 m_lastPower sentinel (-1; runtime-only; resets on
//                 forceAttwhenPowerChangesWhenPSAon toggle per Thetis
//                 console.cs:29298 [v2.10.3.13]); pureSignalActive()
//                 predicate (returns false unconditionally — 3M-4
//                 PureSignal phase wires the live PS-A check). Math
//                 kernel itself (computeAudioVolume / setPowerUsingTargetDbm)
//                 lands in Phases 3B / 3C. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-05-03 — Phase 3 Agent 3B of issue #167: computeAudioVolume()
//                 math kernel — faithful port of Thetis SetPowerUsingTargetDBM
//                 dBm-target math (console.cs:46720-46751 [v2.10.3.13])
//                 with two NereusSDR-original safety short-circuits
//                 (sliderWatts <= 0 → 0.0; gbb >= 99.5 → linear fallback).
//                 Pure function: no state mutation, no signal emission.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-05-03 — Phase 3 Agent 3C of issue #167: setPowerUsingTargetDbm()
//                 deep-parity wrapper.  Full port of Thetis
//                 SetPowerUsingTargetDBM (console.cs:46645-46762
//                 [v2.10.3.13]) integrating Phase 3A scaffolding +
//                 Phase 3B math kernel into a unified API.  Adds
//                 m_twoToneActive / m_tuneDrivePowerSource / m_tunePower
//                 state + setters + persistence; setStepAttenuatorController
//                 injection; audioVolumeChanged signal.  Routes all three
//                 txMode branches and both drive-source enums; ATT-on-TX
//                 safety gate firing via injected StepAttenuatorController.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-05-04 — Issue #175: HL2 TX mi0bot parity port.  Added
//                 setTxPostGenToneMag property + signal; HL2 sub-step
//                 DSP modulation in setPowerUsingTargetDbm tune-slider
//                 path; HL2 audio-volume formula in computeAudioVolume.
//                 Cites mi0bot-Thetis console.cs:47660-47673 +
//                 47775-47778 [v2.10.3.13-beta2].  J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-05-04 — Issue #175 review fix: setTunePower (Fixed-mode global)
//                 ceiling polymorphs on the connected SKU to match
//                 setTunePowerForBand (line 475).  load() per-band clamp
//                 also polymorphs.  Closes a code-review gap where a
//                 Fixed-mode value of 100 stored on a non-HL2 radio
//                 would survive HL2 reconnect.  Multi-source NereusSDR
//                 block above + appended mi0bot console.cs verbatim
//                 header below complete the GPL attribution.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-28 - R-IOS-13 / R-R3-49: txEqCurve, the read-only curve
//                 derived from txEqParaEqData (ParaEqCurve::txEqCurveJson)
//                 whenever the blob changes. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 (transmitSettingsVersion 15): cfcProfile,
//                 refreshed from every CFC change (refreshCfcProfile).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-29 - PA on-air gate review: the per-band FM TX offset store
//                 (setter and load) keeps only finite values in 0..50 MHz,
//                 else the band's default. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-29 - PA on-air gate re-review: the first transmit band
//                 repaints the tune power (tunePowerForTxBandChanged) even
//                 when unchanged; clearTuneTxBand() forgets it at a
//                 disconnect. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-29 - PA on-air gate re-review: an out-of-range per-band FM TX
//                 offset set keeps the previous value (console.cs:20896
//                 [v2.10.3.15]); the load still falls back to the band's
//                 default. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-29 - Two-tone PA wiring: setPowerUsingTargetDbm skips the PWR
//                 slider limit while powerSliderLimitEnabled is off
//                 (PrettyTrackBar.ConstrainAValue [v2.10.3.15]). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Radio codec: lineInGain follows lineInBoost through
//                 lineInGainIndexForBoost (Thetis SetMicGain /
//                 MakeLineInList); its default is the index for 0.0 dB.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Radio codec review: setLineInBoost holds the value on
//                 the 1.5 dB grid, the entry the radio is sent. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - CFC echo: a paired-curve ten-band or scalar write the
//                 curve already holds keeps the curve unchanged, so a late
//                 Core answer cannot overwrite a newer unsent curve.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - CFC echo fix round 1: that write goes on to the setter's
//                 own equal-value check, so an integer mirror a restore
//                 held apart from the curve still follows it. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "TransmitModel.h"
#include "core/AppSettings.h"
#include "core/ParaEqCurve.h"
#include "core/CfcProfile.h"
#include "core/PaProfile.h"
#include "core/PureSignal.h"
#include "core/StepAttenuatorController.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QScopedValueRollback>

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <vector>
#include <cmath>

namespace NereusSDR {

namespace {
// Number of bands in the per-band TX array — HF amateur + GEN/WWV/XVTR (14).
// From Thetis console.cs:12094 [v2.10.3.13]: int[] tunePower_by_band sized
// to (int)Band.LAST, which equals 14 for the Thetis Band enum.
//
// Phase 3L Note: NereusSDR's Band enum was extended to include 13 SWL bands
// (Band::SwlFirst..SwlLast) for HL2 N2ADR Filter pin assignments — but TX
// tune power is HF amateur only.  SWL bands inherit the closest ham-band
// value implicitly (no separate per-SWL persistence).
//
// 2 m (R-IOS-26, R-R3-49) keeps its own value, as Thetis's arrays sized by
// (int)Band.LAST do for B2M: the arrays hold the per-band state slots
// (Band.h, perBandStateSlot), 2 m at slot 14.
constexpr int kBandCount = kPerBandStateCount;  // 15
} // namespace

QString vaxSlotToString(VaxSlot s)
{
    switch (s) {
        case VaxSlot::None:      return QStringLiteral("None");
        case VaxSlot::MicDirect: return QStringLiteral("MicDirect");
        case VaxSlot::Vax1:      return QStringLiteral("Vax1");
        case VaxSlot::Vax2:      return QStringLiteral("Vax2");
        case VaxSlot::Vax3:      return QStringLiteral("Vax3");
        case VaxSlot::Vax4:      return QStringLiteral("Vax4");
    }
    return QStringLiteral("MicDirect");
}

VaxSlot vaxSlotFromString(const QString& s)
{
    if (s == QLatin1String("None"))      { return VaxSlot::None; }
    if (s == QLatin1String("Vax1"))      { return VaxSlot::Vax1; }
    if (s == QLatin1String("Vax2"))      { return VaxSlot::Vax2; }
    if (s == QLatin1String("Vax3"))      { return VaxSlot::Vax3; }
    if (s == QLatin1String("Vax4"))      { return VaxSlot::Vax4; }
    if (s == QLatin1String("MicDirect")) { return VaxSlot::MicDirect; }
    return VaxSlot::MicDirect;  // unknown-string fallback
}

// Drive-power source helpers (3M-1c B.3) — used by AppSettings persistence.
// Matches Thetis enums.cs:456-461 [v2.10.3.13] enum value identity.
QString drivePowerSourceToString(DrivePowerSource s)
{
    switch (s) {
        case DrivePowerSource::DriveSlider: return QStringLiteral("DriveSlider");
        case DrivePowerSource::TuneSlider:  return QStringLiteral("TuneSlider");
        case DrivePowerSource::Fixed:       return QStringLiteral("Fixed");
    }
    return QStringLiteral("DriveSlider");  // unreachable; default fallback
}

DrivePowerSource drivePowerSourceFromString(const QString& s)
{
    if (s == QLatin1String("DriveSlider")) { return DrivePowerSource::DriveSlider; }
    if (s == QLatin1String("TuneSlider"))  { return DrivePowerSource::TuneSlider; }
    if (s == QLatin1String("Fixed"))       { return DrivePowerSource::Fixed; }
    return DrivePowerSource::DriveSlider;  // unknown-string fallback
}

// R-R3-49: Thetis's default FM TX offset for a band
// (console.cs:1833-1841 [v2.10.3.15]): 1 MHz on 6 m, 0.1 MHz elsewhere.
static double defaultFmTxOffsetMhz(Band band)
{
    switch (band) {
        case Band::Band6m:  return 1.0;  // 1MHz
        case Band::Band10m: return 0.1;  // 100kHz
        default:            return 0.1;  // 100kHz
    }
}

// The per-band store keeps only what udFMOffset can hold (0..50 MHz, the
// FMTXOffsetMHz setter's check, console.cs:20891-20902 [v2.10.3.15]:
//   if (value < (double)udFMOffset.Minimum || value > (double)udFMOffset.Maximum) return; //MW0LGE_21k9
// ). NaN and infinities are outside it. A set outside it keeps the value it
// had (the setter's return); a bad stored value loads as the band's default.
static bool fmTxOffsetInRange(double mhz)
{
    return std::isfinite(mhz) && mhz >= 0.0 && mhz <= 50.0;
}

static double validFmTxOffsetMhz(Band band, double mhz)
{
    return fmTxOffsetInRange(mhz) ? mhz : defaultFmTxOffsetMhz(band);
}

TransmitModel::TransmitModel(QObject* parent)
    : QObject(parent)
{
    // Initialise per-band tune power to 50W.
    // From Thetis console.cs:1819-1820 [v2.10.3.13]:
    //   tunePower_by_band = new int[(int)Band.LAST];
    //   for (int i = 0; i < (int)Band.LAST; i++) tunePower_by_band[i] = 50;
    m_tunePowerByBand.fill(50);

    // R-IOS-13 / R-R3-49: txEqCurve for the empty blob a new model starts
    // with (the flat curve Thetis applies in its place).
    m_txEqCurve = ParaEqCurve::txEqCurveJson(m_txEqParaEqData);

    // R-R3-49 (transmitSettingsVersion 15): cfcProfile follows every CFC
    // value, once a profile restore has put them all back.
    qRegisterMetaType<CfcEditProfile>();
    refreshCfcProfile();
    for (auto signal : {&TransmitModel::cfcParaEqDataChanged,
                        &TransmitModel::cfcEqFreqJsonChanged,
                        &TransmitModel::cfcCompressionJsonChanged,
                        &TransmitModel::cfcPostEqBandGainJsonChanged}) {
        connect(this, signal, this, [this](const QString&) { refreshCfcProfile(); });
    }
    connect(this, &TransmitModel::cfcPrecompDbChanged, this, [this](int) { refreshCfcProfile(); });
    connect(this, &TransmitModel::cfcPostEqGainDbChanged, this, [this](int) { refreshCfcProfile(); });
    connect(this, &TransmitModel::cfcProfileRestored, this, &TransmitModel::refreshCfcProfile);
    connect(this, &TransmitModel::cfcSettingsReloaded, this, &TransmitModel::refreshCfcProfile);

    // Initialise per-band normal-mode power to 50W (#167 Phase 3A).
    // From Thetis console.cs:1813-1814 [v2.10.3.13]:
    //   power_by_band = new int[(int)Band.LAST];
    //   for (int i = 0; i < (int)Band.LAST; i++) power_by_band[i] = 50;
    // (Thetis safety-first default; users dial up from 50 per band.)
    m_powerByBand.fill(50);

    // R-R3-49: per-band slider limits and FM TX offsets.
    // From Thetis console.cs:1824-1841 [v2.10.3.15]:
    //   for (int i = 0; i < (int)Band.LAST; i++) limitPower_by_band[i] = 100;
    //   for (int i = 0; i < (int)Band.LAST; i++) limitTunePower_by_band[i] = 100;
    //   for (int i = 0; i < (int)Band.LAST; i++) // setup default FM offsets
    //       case Band.B6M: fm_tx_offset_by_band_mhz[i] = 1; break; // 1MHz
    //       case Band.B10M: fm_tx_offset_by_band_mhz[i] = 0.1; break; // 100kHz
    //       default: fm_tx_offset_by_band_mhz[i] = 0.1; break; // 100kHz
    m_limitPowerByBand.fill(100);
    m_limitTunePowerByBand.fill(100);
    for (int i = 0; i < kBandCount; ++i) {
        m_fmTxOffsetByBandMhz[static_cast<std::size_t>(i)] =
            defaultFmTxOffsetMhz(bandFromPerBandStateSlot(i));
    }
}

TransmitModel::~TransmitModel() = default;

void TransmitModel::setMox(bool mox)
{
    if (m_mox != mox) {
        m_mox = mox;
        emit moxChanged(mox);
    }
}

void TransmitModel::setTune(bool tune)
{
    if (m_tune != tune) {
        m_tune = tune;
        emit tuneChanged(tune);
    }
}

void TransmitModel::setPower(int power)
{
    if (m_power != power) {
        m_power = power;
        emit powerChanged(power);
    }
}

void TransmitModel::setMicGain(float gain)
{
    if (!qFuzzyCompare(m_micGain, gain)) {
        m_micGain = gain;
        emit micGainChanged(gain);
    }
}

void TransmitModel::setPureSigEnabled(bool enabled)
{
    if (m_pureSigEnabled != enabled) {
        m_pureSigEnabled = enabled;
        emit pureSigChanged(enabled);
    }
}

void TransmitModel::setSwrProtectFactor(float f)
{
    // Clamp to [0.0, 1.0]; mi0bot NetworkIO.cs:209-211 [v2.10.3.14-beta1]
    // applies _swr_protect ≤ 1.0 inside the wire-byte multiply, so any
    // value > 1.0 would over-amplify drive — clamp defensively.
    const float clamped = std::clamp(f, 0.0f, 1.0f);
    if (qFuzzyCompare(m_swrProtectFactor, clamped)) {
        return;
    }
    m_swrProtectFactor = clamped;
    emit swrProtectFactorChanged(clamped);
}

void TransmitModel::setTxOwnerSlot(VaxSlot s)
{
    const VaxSlot prev = m_txOwnerSlot.exchange(s, std::memory_order_acq_rel);
    if (prev == s) { return; }

    AppSettings::instance().setValue(
        QStringLiteral("tx/OwnerSlot"), vaxSlotToString(s));
    // No eager save() — matches TransmitModel's existing flush policy
    // (no other setters call AppSettings::instance().save() here).

    emit txOwnerSlotChanged(s);
}

void TransmitModel::loadFromSettings()
{
    const QString v = AppSettings::instance()
        .value(QStringLiteral("tx/OwnerSlot"), QStringLiteral("MicDirect"))
        .toString();
    const VaxSlot s = vaxSlotFromString(v);
    if (s != m_txOwnerSlot.load(std::memory_order_acquire)) {
        m_txOwnerSlot.store(s, std::memory_order_release);
        emit txOwnerSlotChanged(s);
    }
}

// ── Mic gain (3M-1b C.1) ──────────────────────────────────────────────────

// iPhone app plan Task 40: the whole of Thetis's setAudioMicGain, so the mic
// mute silences the mic whichever window or device sets it.
// From Thetis console.cs:28856-28868 [v2.10.3.15]:
//   private void setAudioMicGain(double gain_db)
//   {
//       if (chkMicMute.Checked) // although it is called chkMicMute, checked = mic in use
//       {
//           Audio.MicPreamp = Math.Pow(10.0, gain_db / 20.0); // convert to scalar
//           _mic_muted = false;
//       }
//       else
//       {
//           Audio.MicPreamp = 0.0;
//           _mic_muted = true;
//       }
//   }
namespace {

// The mic preamp setAudioMicGain sets: 10^(gainDb/20) with the mic in use,
// 0.0 while it is muted.
double micPreampFor(bool micInUse, int gainDb)
{
    // although it is called chkMicMute, checked = mic in use  [original inline comment from console.cs:28858]
    return micInUse ? std::pow(10.0, gainDb / 20.0) // convert to scalar
                    : 0.0;
}

} // namespace

void TransmitModel::setMicGainDb(int dB)
{
    // Clamp to range per Thetis console.cs:19151-19171 [v2.10.3.13].
    // Thetis runtime defaults: mic_gain_min = -40, mic_gain_max = 10.
    // NereusSDR model range [-50, 70] per plan §C.1.
    const int clamped = std::clamp(dB, kMicGainDbMin, kMicGainDbMax);
    if (clamped == m_micGainDb) { return; }  // idempotent guard

    m_micGainDb = clamped;
    m_micPreampLinear = micPreampFor(m_micMute, m_micGainDb);

    persistOne(QStringLiteral("MicGain"), QString::number(m_micGainDb));  // L.2 auto-persist

    emit micGainDbChanged(m_micGainDb);
    emit micPreampChanged(m_micPreampLinear);
}


// ── Mic-jack flag properties (3M-1b C.2) ─────────────────────────────────────
//
// Porting from Thetis console.cs:13213-13260 [v2.10.3.13]:
//   LineIn / LineInBoost / MicBoost / MicXlr property block.
// Porting from Thetis console.cs:28752 [v2.10.3.13] (MicMute: counter-intuitive
//   naming preserved — see header comment in TransmitModel.h).
// Porting from Thetis console.cs:19757-19766 [v2.10.3.13] (MicPTTDisabled).
// MicTipRing default from setup.designer.cs:8683 [v2.10.3.13]:
//   radOrionMicTip.Checked = true.
// MicBias default from setup.designer.cs:8779 [v2.10.3.13]:
//   radOrionBiasOff.Checked = true.
// LineInBoost range from setup.designer.cs:46898-46907 [v2.10.3.13]:
//   udLineInBoost.Minimum=-34.5, Maximum=12.0 (decoded from C# decimal int[4]).

void TransmitModel::setMicMute(bool on)
{
    if (on == m_micMute) { return; }  // idempotent guard
    m_micMute = on;
    emit micMuteChanged(on);
    // Thetis chkMicMute_CheckedChanged runs ptbMic_Scroll, which sets the
    // preamp through setAudioMicGain.
    // From Thetis console.cs:28845-28846 [v2.10.3.15] (ptbMic_Scroll):
    //   //[2.10.3.9]MW0LGE fix for when mic is disabled
    //   setAudioMicGain((double)ptbMic.Value);
    const double preamp = micPreampFor(m_micMute, m_micGainDb);
    if (preamp != m_micPreampLinear) {
        m_micPreampLinear = preamp;
        emit micPreampChanged(m_micPreampLinear);
    }
}

void TransmitModel::setMicMuted(bool muted)
{
    setMicMute(!muted);
}

void TransmitModel::setMicBoost(bool on)
{
    if (on == m_micBoost) { return; }  // idempotent guard
    // Porting from Thetis console.cs:13237-13246 [v2.10.3.13]:
    //   mic_boost = value; ptbMic_Scroll(); SetMicGain();
    // Phase D wires the WDSP side; model just stores + signals.
    m_micBoost = on;
    persistOne(QStringLiteral("Mic_Input_Boost"), on ? QStringLiteral("True") : QStringLiteral("False"));  // L.2 auto-persist
    emit micBoostChanged(on);
}

void TransmitModel::setMicXlr(bool on)
{
    if (on == m_micXlr) { return; }  // idempotent guard
    // Porting from Thetis console.cs:13249-13258 [v2.10.3.13]:
    //   mic_xlr = value; ptbMic_Scroll(); SetMicXlr();
    // Phase G wires the SetMicXlr() bit; model just stores + signals.
    m_micXlr = on;
    persistOne(QStringLiteral("Mic_XLR"), on ? QStringLiteral("True") : QStringLiteral("False"));  // L.2 auto-persist
    emit micXlrChanged(on);
}

void TransmitModel::setLineIn(bool on)
{
    if (on == m_lineIn) { return; }  // idempotent guard
    // Porting from Thetis console.cs:13213-13222 [v2.10.3.13]:
    //   line_in = value; ptbMic_Scroll(); SetMicGain();
    m_lineIn = on;
    persistOne(QStringLiteral("Line_Input_On"), on ? QStringLiteral("True") : QStringLiteral("False"));  // L.2 auto-persist
    emit lineInChanged(on);
}

void TransmitModel::setLineInBoost(double dB)
{
    // Clamp to Thetis range per setup.designer.cs:46898-46907 [v2.10.3.13]:
    //   udLineInBoost.Minimum = -34.5, udLineInBoost.Maximum = 12.0
    // Radio codec lane (2026-09-30): the value is then held on the 1.5 dB
    // grid of Thetis's udLineInBoost (setup.designer.cs:47006-47034
    // [v2.10.3.15], Increment 1.5 from -34.5, ReadOnly), taking the entry
    // lineInGainIndexForBoost sends, so an older whole-dB setting or a
    // pre-24 peer's write shows the value the radio gets (5.0 -> 4.5).
    const double clamped = std::clamp(dB, kLineInBoostMin, kLineInBoostMax);
    const double onGrid = kLineInBoostMin
        + kLineInBoostStep * static_cast<double>(lineInGainIndexForBoost(clamped));
    if (onGrid == m_lineInBoost) { return; }  // idempotent guard
    // Porting from Thetis console.cs:13225-13234 [v2.10.3.13]:
    //   line_in_boost = value; ptbMic_Scroll(); SetMicGain();
    m_lineInBoost = onGrid;
    persistOne(QStringLiteral("Line_Input_Level"), QString::number(m_lineInBoost));  // L.2 auto-persist
    emit lineInBoostChanged(onGrid);
    // Thetis SetMicGain sends the line-in gain as the index of line_in_boost
    // in its 1.5 dB table, so the wire index follows the dB value here.
    // From Thetis console.cs:40928-40932 [v2.10.3.15]:
    //   if (!lineinarrayfill) MakeLineInList();
    //   var lineboost = Array.IndexOf(lineinboost, line_in_boost.ToString());
    //   NetworkIO.SetLineBoost(lineboost);
    setLineInGain(lineInGainIndexForBoost(m_lineInBoost));
}

int TransmitModel::lineInGainIndexForBoost(double dB) noexcept
{
    // From Thetis console.cs:40900-40912 [v2.10.3.15] (MakeLineInList):
    //   for (double i = -34.5; i <= 12; i += 1.5) { lineinboost[k] = s; ++k; }
    // Entry k is -34.5 + 1.5 * k, k = 0..31. Thetis's control steps 1.5 dB
    // from -34.5 (setup.designer.cs udLineInBoost Increment 1.5, ReadOnly),
    // so its IndexOf always finds the value. A value between steps (an
    // older NereusSDR setting saved in whole dB) takes the nearest entry
    // here rather than Thetis's -1, which no Thetis control can produce.
    const double clamped = std::clamp(dB, kLineInBoostMin, kLineInBoostMax);
    const int index = static_cast<int>(
        std::lround((clamped - kLineInBoostMin) / kLineInBoostStep));
    return std::clamp(index, 0, kLineInGainIndexMax);
}

void TransmitModel::setMicTipRing(bool tipIsMic)
{
    if (tipIsMic == m_micTipRing) { return; }  // idempotent guard
    // NereusSDR model stores intuitive polarity (true = Tip is mic).
    // Wire-bit polarity inversion at RadioConnection::setMicTipRing (Phase G).
    // Thetis setup.cs:16463-16468 [v2.10.3.13]:
    //   if (radOrionMicTip.Checked) NetworkIO.SetMicTipRing(0);
    //   else NetworkIO.SetMicTipRing(1);
    m_micTipRing = tipIsMic;
    persistOne(QStringLiteral("Mic_TipRing"), tipIsMic ? QStringLiteral("True") : QStringLiteral("False"));  // L.2 auto-persist
    emit micTipRingChanged(tipIsMic);
}

void TransmitModel::setMicBias(bool on)
{
    if (on == m_micBias) { return; }  // idempotent guard
    // Porting from Thetis setup.cs:16471-16476 [v2.10.3.13]:
    //   if (radOrionBiasOn.Checked) NetworkIO.SetMicBias(1);
    //   else NetworkIO.SetMicBias(0);
    // Phase G wires the SetMicBias() bit; model just stores + signals.
    m_micBias = on;
    persistOne(QStringLiteral("Mic_Bias"), on ? QStringLiteral("True") : QStringLiteral("False"));  // L.2 auto-persist
    emit micBiasChanged(on);
}

void TransmitModel::setMicPttDisabled(bool disabled)
{
    if (disabled == m_micPttDisabled) { return; }  // idempotent guard
    // Porting from Thetis console.cs:19757-19764 [v2.10.3.13]:
    //   mic_ptt_disabled = value;
    //   NetworkIO.SetMicPTT(Convert.ToInt32(value));
    // Phase G wires the NetworkIO.SetMicPTT() call; model just stores + signals.
    m_micPttDisabled = disabled;
    persistOne(QStringLiteral("Mic_PTT_Disabled"), disabled ? QStringLiteral("True") : QStringLiteral("False"));  // L.2 auto-persist
    emit micPttDisabledChanged(disabled);
}

// ── line_in_gain + user_dig_out setters (Task 2.4 of P1 full-parity epic) ─

void TransmitModel::setLineInGain(int gain)
{
    // Clamp to bank 11 C2 low 5 bits per Thetis networkproto1.c:600 [v2.10.3.13]:
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ...
    const int clamped = std::clamp(gain, 0, 31);
    if (clamped == m_lineInGain) { return; }  // idempotent guard
    m_lineInGain = clamped;
    persistOne(QStringLiteral("LineInGain"), QString::number(m_lineInGain));  // L.2 auto-persist
    emit lineInGainChanged(clamped);
}

void TransmitModel::setUserDigOut(int dig)
{
    // Mask to bank 11 C3 low 4 bits per Thetis networkproto1.c:601 [v2.10.3.13]:
    //   C3 = prn->user_dig_out & 0b00001111;
    const int masked = dig & 0x0F;
    if (masked == m_userDigOut) { return; }  // idempotent guard
    m_userDigOut = masked;
    persistOne(QStringLiteral("UserDigOut"), QString::number(m_userDigOut));  // L.2 auto-persist
    emit userDigOutChanged(masked);
}

// ── Per-band tune power (G.3) ─────────────────────────────────────────────

int TransmitModel::tunePowerForBand(Band band) const
{
    const int idx = perBandStateSlot(band);
    if (idx < 0 || idx >= kBandCount) {
        return 50;  // safe fallback for out-of-range band
    }
    return m_tunePowerByBand[static_cast<std::size_t>(idx)];
}

void TransmitModel::setTunePowerForBand(Band band, int watts)
{
    // NereusSDR-original: per-band tune-power memory.
    //
    // Thetis (both ramdor and mi0bot) stores a single global tune_power; we
    // extend it to per-band so the operator does not have to readjust on
    // band change.  Mirrors NereusSDR's existing per-band power_by_band[]
    // pattern.
    //
    // Clamp range polymorphs on the connected radio model (#175 Task 6):
    //   HERMESLITE: [0, 99]  (mi0bot Tune slider scale, 33 sub-steps;
    //     mi0bot console.cs:47616-47666 [v2.10.3.13-beta2])
    //   others:     [0, 100] (canonical Thetis 0-100 watts target)
    const int idx = perBandStateSlot(band);
    if (idx < 0 || idx >= kBandCount) {
        return;
    }
    const int hi = (m_hpsdrModel == HPSDRModel::HERMESLITE) ? 99 : 100;
    const int clamped = std::clamp(watts, 0, hi);
    if (m_tunePowerByBand[static_cast<std::size_t>(idx)] == clamped) {
        return;
    }
    m_tunePowerByBand[static_cast<std::size_t>(idx)] = clamped;
    emit tunePowerByBandChanged(band, clamped);
    emit tunePowerByBandJsonChanged(tunePowerByBandJson());  // R-R3-49 (parity Task 5)
    if (m_tuneTxBandKnown && band == m_tuneTxBand) {
        refreshTunePowerForTxBand();
    }
}

// ── R-R3-49 (parity Task 2): tune power for the transmit band ─────────────
//
// NereusSDR-original. The TX applet's Tune Power slider sets the per-band
// tune power and the tune drive source to TuneSlider (TxApplet.cpp); a
// remote window reaches the same two through setTunePowerForTxBand on the
// Core, for the band the Core transmits on.

int TransmitModel::tunePowerMax() const noexcept
{
    return (m_hpsdrModel == HPSDRModel::HERMESLITE) ? 99 : 100;
}

void TransmitModel::refreshTunePowerForTxBand()
{
    if (!m_tuneTxBandKnown) { return; }
    const int watts = tunePowerForBand(m_tuneTxBand);
    if (watts == m_tunePowerForTxBand) { return; }
    m_tunePowerForTxBand = watts;
    emit tunePowerForTxBandChanged(watts);
}

void TransmitModel::setTuneTxBand(Band band)
{
    const bool firstKnown = !m_tuneTxBandKnown;
    m_tuneTxBand = band;
    m_tuneTxBandKnown = true;
    if (firstKnown) {
        // PA on-air gate re-review: until now the slider showed its own
        // band's tune power, so the first transmit band repaints it even
        // when its value equals the cached one.
        m_tunePowerForTxBand = tunePowerForBand(band);
        emit tunePowerForTxBandChanged(m_tunePowerForTxBand);
        return;
    }
    refreshTunePowerForTxBand();
}

void TransmitModel::clearTuneTxBand()
{
    // PA on-air gate re-review: the transmit band belonged to the radio
    // that went away (RadioModel teardown).
    m_tuneTxBandKnown = false;
}

bool TransmitModel::setTunePowerForTxBand(int watts)
{
    if (!m_tuneTxBandKnown) { return false; }
    setTunePowerForBand(m_tuneTxBand, watts);
    setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
    return true;
}

bool TransmitModel::applyStationValue(const QByteArray& propertyName, const QVariant& value)
{
    if (propertyName == "tunePowerForTxBand") {
        bool ok = false;
        const int watts = value.toInt(&ok);
        if (!ok) { return false; }
        if (watts != m_tunePowerForTxBand) {
            m_tunePowerForTxBand = watts;
            emit tunePowerForTxBandChanged(watts);
        }
        return true;
    }
    // R-R3-49 (parity Task 3): the Core's TX profiles, plain state.
    if (propertyName == "activeTxProfile") {
        const QString name = value.toString();
        if (name != m_activeTxProfile) {
            m_activeTxProfile = name;
            emit activeTxProfileChanged(name);
        }
        return true;
    }
    if (propertyName == "txProfilesJson") {
        const QString json = value.toString();
        if (json != m_txProfilesJson) {
            m_txProfilesJson = json;
            emit txProfilesJsonChanged(json);
        }
        return true;
    }
    if (propertyName == "tuneDrivePowerSource") {
        if (!value.canConvert<DrivePowerSource>()) { return false; }
        const DrivePowerSource source = value.value<DrivePowerSource>();
        if (source != m_tuneDrivePowerSource) {
            // The Core's report, not this window's choice: not saved here.
            m_tuneDrivePowerSource = source;
            emit tuneDrivePowerSourceChanged(source);
        }
        return true;
    }
    return false;
}

void TransmitModel::reportTunePowerForTxBandRefused()
{
    emit tunePowerForTxBandChanged(m_tunePowerForTxBand);
}

// ── R-R3-49 (parity Task 3): the Core's TX profiles on the link ──────────
//
// NereusSDR-original. The list goes out as a JSON array of the names, in
// the Core's MicProfileManager order, so a name holding a comma (an older
// profile saved before the comma rule) stays one name.

void TransmitModel::setStationTxProfiles(const QString& active, const QStringList& names)
{
    const QString json = QString::fromUtf8(
        QJsonDocument(QJsonArray::fromStringList(names)).toJson(QJsonDocument::Compact));
    if (json != m_txProfilesJson) {
        m_txProfilesJson = json;
        emit txProfilesJsonChanged(json);
    }
    if (active != m_activeTxProfile) {
        m_activeTxProfile = active;
        emit activeTxProfileChanged(active);
    }
}

QStringList TransmitModel::txProfileNamesFromJson(const QString& json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) {
        return {};
    }
    QStringList names;
    for (const QJsonValue& value : doc.array()) {
        if (!value.isString()) {
            return {};
        }
        names.append(value.toString());
    }
    return names;
}

// R-R3-49 (parity Task 4): the link's ten-value band arrays.
namespace {

// NereusSDR-original: ten whole numbers as the link's compact JSON array.
QString tenValuesJson(const std::function<int(int)>& value)
{
    QJsonArray array;
    for (int i = 0; i < 10; ++i) {
        array.append(value(i));
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

// The ten whole numbers in `json`, or false when it is not a JSON array of
// exactly ten whole numbers. Range is the caller's.
bool tenValuesFromJson(const QString& json, std::array<int, 10>& out)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) { return false; }
    const QJsonArray array = doc.array();
    if (array.size() != 10) { return false; }
    for (int i = 0; i < 10; ++i) {
        const QJsonValue v = array.at(i);
        if (!v.isDouble()) { return false; }
        const double d = v.toDouble();
        if (!std::isfinite(d) || d != std::floor(d)
            || d < static_cast<double>(std::numeric_limits<int>::min())
            || d > static_cast<double>(std::numeric_limits<int>::max())) {
            return false;
        }
        out[static_cast<std::size_t>(i)] = static_cast<int>(d);
    }
    return true;
}

bool tenValuesInRange(const QString& json, int lo, int hi)
{
    std::array<int, 10> values{};
    if (!tenValuesFromJson(json, values)) { return false; }
    return std::all_of(values.begin(), values.end(),
                       [lo, hi](int v) { return v >= lo && v <= hi; });
}

// R-R3-49 (parity Task 5): the link's per-band watts, a JSON object keyed by
// bandKeyName for the 15 bands 160m .. XVTR and 2m. NereusSDR-original.
// A peer built before 2 m (R-IOS-26) reads and writes the 14 without "2m"
// (BandLinkFit.h); a map without "2m" keeps 2 m's value.
constexpr int kLinkBandCount = kPerBandStateCount;  // 15

QString bandWattsJson(const std::function<int(Band)>& value)
{
    QJsonObject object;
    for (const Band band : kPerBandStateBands) {
        object.insert(bandKeyName(band), value(band));
    }
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

// The bands' watts in `json`, or false when it is not a JSON object
// holding exactly the 15 band keys (or the 14 without "2m", from a peer
// built before 2 m), each a whole number from lo to hi. The whole map,
// like the ten-value arrays: a window always sends every band it knows.
bool bandWattsFromJson(const QString& json, int lo, int hi,
                       std::vector<std::pair<Band, int>>& out)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) { return false; }
    const QJsonObject object = doc.object();
    const bool without2m = !object.contains(bandKeyName(Band::Band2m));
    if (object.size() != (without2m ? kLinkBandCount - 1 : kLinkBandCount)) { return false; }
    out.clear();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        Band found = Band::Count;
        for (const Band band : kPerBandStateBands) {
            if (bandKeyName(band) == it.key()) {
                found = band;
                break;
            }
        }
        if (found == Band::Count || !it.value().isDouble()) { return false; }
        const double d = it.value().toDouble();
        if (!std::isfinite(d) || d != std::floor(d)
            || d < static_cast<double>(lo) || d > static_cast<double>(hi)) {
            return false;
        }
        out.emplace_back(found, static_cast<int>(d));
    }
    return true;
}

} // namespace

QString TransmitModel::settingRangeRefusal(const QByteArray& propertyName,
                                           const QVariant& value) const
{
    const auto outside = [&value](qlonglong lo, qlonglong hi) {
        bool ok = false;
        const qlonglong v = value.toLongLong(&ok);
        return !ok || v < lo || v > hi;
    };
    // R-R3-49 (group A fix wave, M4): a parametric EQ curve the Core could
    // not load would leave its TX channel on the flat default curve. An
    // empty value is the saved "no curve" and loads the default, as the
    // TX profile's blank value does. The curves share the gzip envelope,
    // but CFC contains two widget JSON objects; each has its own loader.
    if (propertyName == "cfcParaEqData") {
        CfcProfile::Profile points;
        if (value.toString().isEmpty() || CfcProfile::decode(value.toString(), points)) {
            return {};
        }
        return QStringLiteral("The Core could not read that equalizer curve. Save the curve again and retry.");
    }
    if (propertyName == "txEqParaEqData") {
        const QString data = value.toString();
        ParaEqCurve::TxEqPoints points;
        if (data.isEmpty() || ParaEqCurve::loadTxEqPoints(data, points)) {
            return {};
        }
        return QStringLiteral("The Core could not read that equalizer curve. Save the curve again and retry.");
    }
    if (propertyName == "tunePower" || propertyName == "tunePowerForTxBand") {
        const int hi = tunePowerMax();
        if (!outside(0, hi)) { return {}; }
        return m_hpsdrModel == HPSDRModel::HERMESLITE
            ? QStringLiteral("Choose a tune power from 0 to %1.").arg(hi)
            : QStringLiteral("Choose a tune power from 0 to %1 W.").arg(hi);
    }
    if (propertyName == "voxThresholdDb") {
        return outside(kVoxThresholdDbMin, kVoxThresholdDbMax)
            ? QStringLiteral("Choose a VOX level from %1 to %2 dB.")
                  .arg(kVoxThresholdDbMin).arg(kVoxThresholdDbMax)
            : QString();
    }
    if (propertyName == "voxHangTimeMs") {
        return outside(kVoxHangTimeMsMin, kVoxHangTimeMsMax)
            ? QStringLiteral("Choose a VOX delay from %1 to %2 ms.")
                  .arg(kVoxHangTimeMsMin).arg(kVoxHangTimeMsMax)
            : QString();
    }
    if (propertyName == "cpdrLevelDb") {
        return outside(kCpdrLevelDbMin, kCpdrLevelDbMax)
            ? QStringLiteral("Choose a PROC level from %1 to %2 dB.")
                  .arg(kCpdrLevelDbMin).arg(kCpdrLevelDbMax)
            : QString();
    }
    if (propertyName == "amCarrierLevel") {
        return outside(kAmCarrierLevelMin, kAmCarrierLevelMax)
            ? QStringLiteral("Choose an AM carrier level from %1 to %2 percent.")
                  .arg(kAmCarrierLevelMin).arg(kAmCarrierLevelMax)
            : QString();
    }
    if (propertyName == "micGainDb") {
        return outside(kMicGainDbMin, kMicGainDbMax)
            ? QStringLiteral("Choose a mic level from %1 to %2 dB.")
                  .arg(kMicGainDbMin).arg(kMicGainDbMax)
            : QString();
    }
    // R-R3-49 (parity Task 3): Setup > Audio > TX Input's Line In gain.
    if (propertyName == "lineInBoost") {
        bool ok = false;
        const double v = value.toDouble(&ok);
        const bool inRange = ok && std::isfinite(v)
            && v >= kLineInBoostMin && v <= kLineInBoostMax;
        // The words carry kLineInBoostMin and kLineInBoostMax (-34.5, 12.0).
        return inRange ? QString()
                       : QStringLiteral("Choose a Line In gain from -34.5 to 12.0 dB.");
    }
    if (propertyName == "monitorVolume") {
        bool ok = false;
        const double v = value.toDouble(&ok);
        const bool inRange = ok && std::isfinite(v)
            && v >= static_cast<double>(kMonitorVolumeMin)
            && v <= static_cast<double>(kMonitorVolumeMax);
        return inRange ? QString()
                       : QStringLiteral("Choose a monitor level from 0.0 to 1.0.");
    }
    // R-R3-49 (parity Task 4): the TX EQ, CFC, phase rotator, leveler and
    // ALC settings, each with its setter's own range; a band array is
    // refused whole unless it holds ten whole numbers, each in range.
    const auto scalar = [&outside](qlonglong lo, qlonglong hi, const QString& words) {
        return outside(lo, hi) ? words : QString();
    };
    if (propertyName == "txEqPreamp") {
        return scalar(kTxEqPreampDbMin, kTxEqPreampDbMax,
            QStringLiteral("Choose a TX EQ preamp from %1 to %2 dB.")
                .arg(kTxEqPreampDbMin).arg(kTxEqPreampDbMax));
    }
    if (propertyName == "txEqNc") {
        return scalar(kTxEqNcMin, kTxEqNcMax,
            QStringLiteral("Choose a TX EQ Nc from %1 to %2.")
                .arg(kTxEqNcMin).arg(kTxEqNcMax));
    }
    if (propertyName == "txEqCtfmode") {
        return scalar(0, kTxEqCtfmodeMax,
            QStringLiteral("Choose a TX EQ cutoff of 0 (peaking) or 1 (notch)."));
    }
    if (propertyName == "txEqWintype") {
        return scalar(0, kTxEqWintypeMax,
            QStringLiteral("Choose a TX EQ window of 0 (Blackman-Harris) or 1 (Hann)."));
    }
    if (propertyName == "cfcPrecompDb") {
        return scalar(kCfcPrecompDbMin, kCfcPrecompDbMax,
            QStringLiteral("Choose a CFC pre-compression from %1 to %2 dB.")
                .arg(kCfcPrecompDbMin).arg(kCfcPrecompDbMax));
    }
    if (propertyName == "cfcPostEqGainDb") {
        return scalar(kCfcPostEqGainDbMin, kCfcPostEqGainDbMax,
            QStringLiteral("Choose a CFC post-EQ gain from %1 to %2 dB.")
                .arg(kCfcPostEqGainDbMin).arg(kCfcPostEqGainDbMax));
    }
    if (propertyName == "phaseRotatorFreqHz") {
        return scalar(kPhaseRotatorFreqHzMin, kPhaseRotatorFreqHzMax,
            QStringLiteral("Choose a phase rotator frequency from %1 to %2 Hz.")
                .arg(kPhaseRotatorFreqHzMin).arg(kPhaseRotatorFreqHzMax));
    }
    if (propertyName == "phaseRotatorStages") {
        return scalar(kPhaseRotatorStagesMin, kPhaseRotatorStagesMax,
            QStringLiteral("Choose from %1 to %2 phase rotator stages.")
                .arg(kPhaseRotatorStagesMin).arg(kPhaseRotatorStagesMax));
    }
    if (propertyName == "txLevelerMaxGain") {
        return scalar(kTxLevelerMaxGainDbMin, kTxLevelerMaxGainDbMax,
            QStringLiteral("Choose a leveler maximum gain from %1 to %2 dB.")
                .arg(kTxLevelerMaxGainDbMin).arg(kTxLevelerMaxGainDbMax));
    }
    if (propertyName == "txLevelerDecay") {
        return scalar(kTxLevelerDecayMsMin, kTxLevelerDecayMsMax,
            QStringLiteral("Choose a leveler decay from %1 to %2 ms.")
                .arg(kTxLevelerDecayMsMin).arg(kTxLevelerDecayMsMax));
    }
    if (propertyName == "txAlcMaxGain") {
        return scalar(kTxAlcMaxGainDbMin, kTxAlcMaxGainDbMax,
            QStringLiteral("Choose an ALC maximum gain from %1 to %2 dB.")
                .arg(kTxAlcMaxGainDbMin).arg(kTxAlcMaxGainDbMax));
    }
    if (propertyName == "txAlcDecay") {
        return scalar(kTxAlcDecayMsMin, kTxAlcDecayMsMax,
            QStringLiteral("Choose an ALC decay from %1 to %2 ms.")
                .arg(kTxAlcDecayMsMin).arg(kTxAlcDecayMsMax));
    }
    const auto bands = [&value](int lo, int hi, const QString& words) {
        return tenValuesInRange(value.toString(), lo, hi) ? QString() : words;
    };
    if (propertyName == "txEqBandsJson") {
        return bands(kTxEqBandDbMin, kTxEqBandDbMax,
            QStringLiteral("Choose ten TX EQ band levels, each from %1 to %2 dB.")
                .arg(kTxEqBandDbMin).arg(kTxEqBandDbMax));
    }
    if (propertyName == "txEqFreqsJson") {
        return bands(kTxEqFreqHzMin, kTxEqFreqHzMax,
            QStringLiteral("Choose ten TX EQ band centers, each from %1 to %2 Hz.")
                .arg(kTxEqFreqHzMin).arg(kTxEqFreqHzMax));
    }
    if (propertyName == "cfcCompressionJson") {
        const QString base = bands(kCfcCompressionDbMin, kCfcCompressionDbMax,
            QStringLiteral("Choose ten CFC compression levels, each from %1 to %2 dB.")
                .arg(kCfcCompressionDbMin).arg(kCfcCompressionDbMax));
        if (!base.isEmpty()) { return base; }
        CfcProfile::Profile p;
        if (!CfcProfile::decode(m_cfcParaEqData, p)) { return {}; }
        if (p.f.size() != 10) {
            return QStringLiteral("This CFC curve has five or eighteen bands. Update the app or use the full CFC controls.");
        }
        std::array<int, 10> values{};
        tenValuesFromJson(value.toString(), values);
        for (int i = 0; i < 10; ++i) { p.g[static_cast<std::size_t>(i)] = values[static_cast<std::size_t>(i)]; }
        return CfcProfile::encode(p).isEmpty()
            ? QStringLiteral("Those CFC values cannot form a complete curve.") : QString();
    }
    if (propertyName == "cfcEqFreqJson") {
        const QString base = bands(kCfcEqFreqHzMin, kCfcEqFreqHzMax,
            QStringLiteral("Choose ten CFC band centers, each from %1 to %2 Hz.")
                .arg(kCfcEqFreqHzMin).arg(kCfcEqFreqHzMax));
        if (!base.isEmpty()) { return base; }
        CfcProfile::Profile p;
        if (!CfcProfile::decode(m_cfcParaEqData, p)) { return {}; }
        if (p.f.size() != 10) {
            return QStringLiteral("This CFC curve has five or eighteen bands. Update the app or use the full CFC controls.");
        }
        std::array<int, 10> values{};
        tenValuesFromJson(value.toString(), values);
        for (int i = 0; i < 10; ++i) {
            p.f[static_cast<std::size_t>(i)] = values[static_cast<std::size_t>(i)];
            p.postF[static_cast<std::size_t>(i)] = values[static_cast<std::size_t>(i)];
        }
        p.minHz = p.f.front(); p.maxHz = p.f.back();
        p.postMinHz = p.postF.front(); p.postMaxHz = p.postF.back();
        return CfcProfile::encode(p).isEmpty()
            ? QStringLiteral("Choose CFC band centers in increasing order within the curve range.") : QString();
    }
    if (propertyName == "cfcPostEqBandGainJson") {
        const QString base = bands(kCfcPostEqBandGainDbMin, kCfcPostEqBandGainDbMax,
            QStringLiteral("Choose ten CFC post-EQ band levels, each from %1 to %2 dB.")
                .arg(kCfcPostEqBandGainDbMin).arg(kCfcPostEqBandGainDbMax));
        if (!base.isEmpty()) { return base; }
        CfcProfile::Profile p;
        if (!CfcProfile::decode(m_cfcParaEqData, p)) { return {}; }
        if (p.f.size() != 10) {
            return QStringLiteral("This CFC curve has five or eighteen bands. Update the app or use the full CFC controls.");
        }
        std::array<int, 10> values{};
        tenValuesFromJson(value.toString(), values);
        for (int i = 0; i < 10; ++i) { p.e[static_cast<std::size_t>(i)] = values[static_cast<std::size_t>(i)]; }
        return CfcProfile::encode(p).isEmpty()
            ? QStringLiteral("Those CFC values cannot form a complete curve.") : QString();
    }
    // R-R3-49 (parity Task 5): the per-band power and tune power, each band
    // with its per-band setter's range (setPowerForBand 0 to 100 W;
    // setTunePowerForBand 0 to 100 W, 0 to 99 on the HL2), refused whole.
    if (propertyName == "powerByBandJson" || propertyName == "tunePowerByBandJson") {
        const bool tune = propertyName == "tunePowerByBandJson";
        const int hi = tune ? tunePowerMax() : 100;
        std::vector<std::pair<Band, int>> named;
        if (bandWattsFromJson(value.toString(), 0, hi, named)) {
            return {};
        }
        if (!tune) {
            return QStringLiteral("Choose a power from 0 to 100 W for each band.");
        }
        return m_hpsdrModel == HPSDRModel::HERMESLITE
            ? QStringLiteral("Choose a tune power from 0 to %1 for each band.").arg(hi)
            : QStringLiteral("Choose a tune power from 0 to %1 W for each band.").arg(hi);
    }
    // The DEXP / VOX page's timings and filter, each with its setter's own
    // clamp range (setup.Designer.cs, cited at each constant).
    const auto real = [&value](double lo, double hi) {
        bool ok = false;
        const double v = value.toDouble(&ok);
        return ok && std::isfinite(v) && v >= lo && v <= hi;
    };
    if (propertyName == "dexpAttackTimeMs") {
        // The words carry kDexpAttackTimeMsMin and kDexpAttackTimeMsMax.
        return real(kDexpAttackTimeMsMin, kDexpAttackTimeMsMax) ? QString()
            : QStringLiteral("Choose a DEXP attack time from 2 to 100 ms.");
    }
    if (propertyName == "dexpDetectorTauMs") {
        return real(kDexpDetectorTauMsMin, kDexpDetectorTauMsMax) ? QString()
            : QStringLiteral("Choose a DEXP detector time from 1 to 100 ms.");
    }
    if (propertyName == "dexpReleaseTimeMs") {
        return real(kDexpReleaseTimeMsMin, kDexpReleaseTimeMsMax) ? QString()
            : QStringLiteral("Choose a DEXP release time from 2 to 1000 ms.");
    }
    if (propertyName == "dexpExpansionRatioDb") {
        return real(kDexpExpansionRatioDbMin, kDexpExpansionRatioDbMax) ? QString()
            : QStringLiteral("Choose a DEXP expansion ratio from 0.0 to 30.0 dB.");
    }
    if (propertyName == "dexpHysteresisRatioDb") {
        return real(kDexpHysteresisRatioDbMin, kDexpHysteresisRatioDbMax) ? QString()
            : QStringLiteral("Choose a DEXP hysteresis ratio from 0.0 to 10.0 dB.");
    }
    if (propertyName == "dexpLookAheadMs") {
        return real(kDexpLookAheadMsMin, kDexpLookAheadMsMax) ? QString()
            : QStringLiteral("Choose a look-ahead time from 10 to 999 ms.");
    }
    if (propertyName == "dexpLowCutHz" || propertyName == "dexpHighCutHz") {
        return real(kDexpFilterCutHzMin, kDexpFilterCutHzMax) ? QString()
            : QStringLiteral("Choose a VOX trigger filter cut from 100 to 10000 Hz.");
    }
    if (propertyName == "antiVoxGainDb") {
        return scalar(kAntiVoxGainDbMin, kAntiVoxGainDbMax,
            QStringLiteral("Choose an anti-VOX gain from %1 to %2 dB.")
                .arg(kAntiVoxGainDbMin).arg(kAntiVoxGainDbMax));
    }
    // The Two-Tone IMD page's settings (setup.Designer.cs ranges, cited at
    // each constant).
    if (propertyName == "twoToneFreq1") {
        return scalar(kTwoToneFreq1HzMin, kTwoToneFreq1HzMax,
            QStringLiteral("Choose a tone frequency from %1 to %2 Hz.")
                .arg(kTwoToneFreq1HzMin).arg(kTwoToneFreq1HzMax));
    }
    if (propertyName == "twoToneFreq2") {
        return scalar(kTwoToneFreq2HzMin, kTwoToneFreq2HzMax,
            QStringLiteral("Choose a tone frequency from %1 to %2 Hz.")
                .arg(kTwoToneFreq2HzMin).arg(kTwoToneFreq2HzMax));
    }
    if (propertyName == "twoToneLevel") {
        return real(kTwoToneLevelDbMin, kTwoToneLevelDbMax) ? QString()
            : QStringLiteral("Choose a two-tone level from -96 to 0 dB.");
    }
    if (propertyName == "twoTonePower") {
        return scalar(kTwoTonePowerMin, kTwoTonePowerMax,
            QStringLiteral("Choose a two-tone power from %1 to %2 percent.")
                .arg(kTwoTonePowerMin).arg(kTwoTonePowerMax));
    }
    if (propertyName == "twoToneFreq2Delay") {
        return scalar(kTwoToneFreq2DelayMsMin, kTwoToneFreq2DelayMsMax,
            QStringLiteral("Choose a second tone delay from %1 to %2 ms.")
                .arg(kTwoToneFreq2DelayMsMin).arg(kTwoToneFreq2DelayMsMax));
    }
    return {};
}

// ── Per-band normal-mode power (#167 Phase 3A) ──────────────────────────────

int TransmitModel::powerForBand(Band band) const
{
    const int idx = perBandStateSlot(band);
    if (idx < 0 || idx >= kBandCount) {
        return 100;  // safe fallback for out-of-range band
    }
    return m_powerByBand[static_cast<std::size_t>(idx)];
}

void TransmitModel::setPowerForBand(Band band, int watts)
{
    // From Thetis console.cs:1813-1814 [v2.10.3.13] — power_by_band default
    // 50 W per band (Thetis safety-first).  Used as the slider source for
    // the dBm compensator (Phase 3A scaffolding for #167 Phase 3C math
    // kernel).  Phase 3C's setPowerUsingTargetDbm txMode 0 branch writes
    // back into m_powerByBand[band] via setPower side-effect (matches
    // Thetis console.cs:46676 [v2.10.3.13] power_by_band[(int)_tx_band] =
    // new_pwr).
    const int idx = perBandStateSlot(band);
    if (idx < 0 || idx >= kBandCount) {
        return;
    }
    const int clamped = std::clamp(watts, 0, 100);
    if (m_powerByBand[static_cast<std::size_t>(idx)] == clamped) {
        return;
    }
    m_powerByBand[static_cast<std::size_t>(idx)] = clamped;
    // Auto-persist: hardware/<m_persistMac>/powerByBand/<bandKeyName>.
    if (!m_persistMac.isEmpty()) {
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/powerByBand/%2")
                .arg(m_persistMac, bandKeyName(band)),
            QString::number(clamped));
    }
    emit powerByBandChanged(band, clamped);
    emit powerByBandJsonChanged(powerByBandJson());  // R-R3-49 (parity Task 5)
}

// ── Per-band slider limits and FM TX offset (R-R3-49) ───────────────────────
//
// Thetis stores these per band (console.cs:1824-1841 [v2.10.3.15]) and the
// TXBand setter assigns them to the sliders and the FM offset on a band
// change (console.cs:17539-17550 [v2.10.3.15]).  The limit is only changed
// in Thetis by a right-drag on the slider (ptbPWR_Scroll, console.cs:28690
// [v2.10.3.15]: limitPower_by_band[(int)_tx_band] = lc.LimitValue; // store
// the adjusted limit level), which NereusSDR does not have.

int TransmitModel::limitPowerForBand(Band band) const
{
    const int idx = perBandStateSlot(band);
    if (idx < 0 || idx >= kBandCount) {
        return 100;
    }
    return m_limitPowerByBand[static_cast<std::size_t>(idx)];
}

void TransmitModel::setLimitPowerForBand(Band band, int watts)
{
    const int idx = perBandStateSlot(band);
    if (idx < 0 || idx >= kBandCount) {
        return;
    }
    const int clamped = std::clamp(watts, 0, 100);
    m_limitPowerByBand[static_cast<std::size_t>(idx)] = clamped;
    if (!m_persistMac.isEmpty()) {
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/limitPowerByBand/%2")
                .arg(m_persistMac, bandKeyName(band)),
            QString::number(clamped));
    }
}

int TransmitModel::limitTunePowerForBand(Band band) const
{
    const int idx = perBandStateSlot(band);
    if (idx < 0 || idx >= kBandCount) {
        return 100;
    }
    return m_limitTunePowerByBand[static_cast<std::size_t>(idx)];
}

void TransmitModel::setLimitTunePowerForBand(Band band, int watts)
{
    const int idx = perBandStateSlot(band);
    if (idx < 0 || idx >= kBandCount) {
        return;
    }
    const int clamped = std::clamp(watts, 0, 100);
    m_limitTunePowerByBand[static_cast<std::size_t>(idx)] = clamped;
    if (!m_persistMac.isEmpty()) {
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/limitTunePowerByBand/%2")
                .arg(m_persistMac, bandKeyName(band)),
            QString::number(clamped));
    }
}

// PrettyTrackBar LimitValue setter (PrettyTrackBar.cs [v2.10.3.15]) clamps
// the limit to the slider's Min/Max; ptbPWR and ptbTune are 0..100
// (console.Designer.cs:3686-3689, 3942-3945 [v2.10.3.15]).
void TransmitModel::setPowerLimit(int watts)
{
    m_powerLimit = std::clamp(watts, 0, 100);
}

void TransmitModel::setTunePowerLimit(int watts)
{
    m_tunePowerLimit = std::clamp(watts, 0, 100);
}

void TransmitModel::setFmTxOffsetMhz(double mhz)
{
    // From Thetis console.cs:20891-20902 [v2.10.3.15] (FMTXOffsetMHz setter):
    //   if (value < (double)udFMOffset.Minimum || value > (double)udFMOffset.Maximum) return; //MW0LGE_21k9
    // udFMOffset is 0..50 MHz.
    if (!(mhz >= 0.0 && mhz <= 50.0)) {
        return;
    }
    m_fmTxOffsetMhz = mhz;
}

double TransmitModel::fmTxOffsetForBandMhz(Band band) const
{
    const int idx = perBandStateSlot(band);
    if (idx < 0 || idx >= kBandCount) {
        return defaultFmTxOffsetMhz(band);
    }
    return m_fmTxOffsetByBandMhz[static_cast<std::size_t>(idx)];
}

void TransmitModel::setFmTxOffsetForBandMhz(Band band, double mhz)
{
    const int idx = perBandStateSlot(band);
    if (idx < 0 || idx >= kBandCount) {
        return;
    }
    // From Thetis console.cs:20896 [v2.10.3.15]: out of range keeps the
    // previous value.
    //   if (value < (double)udFMOffset.Minimum || value > (double)udFMOffset.Maximum) return; //MW0LGE_21k9
    if (!fmTxOffsetInRange(mhz)) {
        return;
    }
    m_fmTxOffsetByBandMhz[static_cast<std::size_t>(idx)] = mhz;
    if (!m_persistMac.isEmpty()) {
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/fmTxOffsetByBandMhz/%2")
                .arg(m_persistMac, bandKeyName(band)),
            QString::number(mhz, 'g', 17));
    }
}

// ── ATT-on-TX-on-power-change safety setters (#167 Phase 3A) ────────────────
//
// All 3 setters follow the existing per-MAC L.2 auto-persist pattern.
// CRITICAL: setForceAttwhenPowerChangesWhenPSAon resets m_lastPower to -1
// when the value changes — Thetis console.cs:29298 [v2.10.3.13]:
//     if (value != _forceATTwhenPowerChangesWhenPSAon) _lastPower = -1;
//     _forceATTwhenPowerChangesWhenPSAon = value;

void TransmitModel::setForceAttwhenPSAoff(bool on)
{
    if (on == m_forceAttwhenPSAoff) { return; }  // idempotent guard
    // From Thetis console.cs:29285-29290 [v2.10.3.13]:
    //   private bool _forceATTwhenPSAoff = true; //MW0LGE [2.9.0.7] added
    //   public bool ForceATTwhenPSAoff
    //   { get { return _forceATTwhenPSAoff; }
    //     set { _forceATTwhenPSAoff = value; } }
    m_forceAttwhenPSAoff = on;
    persistOne(QStringLiteral("ForceATTwhenPSAoff"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit forceAttwhenPSAoffChanged(on);
}

void TransmitModel::setForceAttwhenPowerChangesWhenPSAon(bool on)
{
    // From Thetis console.cs:29298 [v2.10.3.13] — reset on toggle:
    //   if (value != _forceATTwhenPowerChangesWhenPSAon) _lastPower = -1;
    //   _forceATTwhenPowerChangesWhenPSAon = value;
    if (on != m_forceAttwhenPowerChangesWhenPSAon) {
        m_lastPower = -1;
    }
    if (on == m_forceAttwhenPowerChangesWhenPSAon) { return; }  // idempotent guard
    m_forceAttwhenPowerChangesWhenPSAon = on;
    persistOne(QStringLiteral("ForceATTwhenOutputPowerChangesWhenPSAon"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit forceAttwhenPowerChangesWhenPSAonChanged(on);
}

void TransmitModel::setForceAttwhenPowerChangesWhenPSAonAndDecreased(bool on)
{
    if (on == m_forceAttwhenPowerChangesWhenPSAonAndDecreased) { return; }  // idempotent guard
    // From Thetis console.cs:29302-29310 [v2.10.3.13]:
    //   private bool _forceATTwhenPowerChangesWhenPSAon_anddecreased = false;
    //   public bool ForceATTwhenOutputPowerChangesWhenPSAonAndDecreased
    //   { get { return _forceATTwhenPowerChangesWhenPSAon_anddecreased; }
    //     set { _forceATTwhenPowerChangesWhenPSAon_anddecreased = value; } }
    m_forceAttwhenPowerChangesWhenPSAonAndDecreased = on;
    persistOne(QStringLiteral("ForceATTwhenOutputPowerChangesWhenPSAonAndDecreased"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit forceAttwhenPowerChangesWhenPSAonAndDecreasedChanged(on);
}

void TransmitModel::setLastPower(int value)
{
    // Mirrors Thetis `private float _lastPower = -1;` (console.cs:29292
    // [v2.10.3.13]).  Runtime-only — NOT persisted.  No signal — Phase 3C
    // is the only writer in the production path; tests use this as
    // bookkeeping for the ATT-on-TX gate semantics.
    m_lastPower = value;
}

bool TransmitModel::pureSignalActive() const noexcept
{
    // Phase 3M-4 Task 7 — live read of PureSignal::correctionsBeingApplied.
    // From Thetis console.cs:46740-46748 [v2.10.3.13]:
    //   //[2.10.3.5]MW0LGE max tx attenuation when power is increased and PS is enabled
    //   if (new_pwr != _lastPower && chkFWCATUBypass.Checked && _forceATTwhenPowerChangesWhenPSAon) ...
    // setPowerUsingTargetDbm uses chkFWCATUBypass.Checked as the predicate
    // (active when PS-A is enabled).  The Thetis predicate is "PS-A
    // enabled" (UI-state); NereusSDR uses "calcc has corrections in
    // flight" (PSForm.cs:1100-1102 [v2.10.3.13] CorrectionsBeingApplied
    // == _info[14] == 1) which is the runtime equivalent: only when
    // calcc has a valid correction set does the safety lift to 31 dB
    // make sense (the gate exists to prevent power surges destabilising
    // the live correction).
#ifdef NEREUS_BUILD_TESTS
    // Phase 3C test seam — exercise the ATT-on-TX gate without
    // constructing a full RadioModel + PureSignal coordinator.  Tri-state:
    //   -1 = no override → fall through to live read (or false if not
    //        yet wired).
    //    0 = force false.
    //    1 = force true.
    if (m_pureSignalActiveOverride >= 0) {
        return m_pureSignalActiveOverride == 1;
    }
#endif
    if (m_pureSignal != nullptr) {
        // PureSignal::correctionsBeingApplied is std::atomic<bool>::load,
        // safe to call from any thread.  noexcept-compatible.
        return m_pureSignal->correctionsBeingApplied();
    }
    return false;
}

// ── computeAudioVolume math kernel (#167 Phase 3B) ──────────────────────────
//
// From Thetis console.cs:46720-46734 [v2.10.3.13] — SetPowerUsingTargetDBM
// math kernel (ATT-on-TX-on-power-change gate at 46740-46748 lands in
// Phase 3C; sliderWatts==0 short-circuit at 46749-46751 ported below).
// The K2GX field report (>300 W output on a 200 W ANAN-8000DLE at low
// TUNE slider positions) was caused by the previous linear-only drive-
// scaling lambda in RadioModel.cpp:830-853 which had no per-band PA gain
// compensation.  This kernel ports the dBm-target math that Thetis uses
// to translate (sliderWatts, band, profile) into the audio-volume scalar
// that drives both the wire byte (audio.cs:268) and the IQ gain
// (cmaster.cs:1117).
//
// Two short-circuits run BEFORE the dBm math:
//
//   1. sliderWatts <= 0 returns 0.0 exactly.  Matches Thetis's
//      console.cs:46749-46751 branch:
//          if (new_pwr == 0) { Audio.RadioVolume = 0.0; ... }
//      Including negatives in the same branch is a NereusSDR-original safety
//      addition — Thetis's `int new_pwr` came from a clamped slider so
//      negatives weren't reachable upstream; in NereusSDR computeAudioVolume
//      can be called from tests + external callers, so failing-loud-zero is
//      the safe behavior.
//
//   2. HERMESLITE branch — full mi0bot-Thetis HL2 audio-volume formula
//      `(hl2Power * gbb/100) / 93.75` (mi0bot-Thetis console.cs:47775-47778
//      [v2.10.3.13-beta2]).  Runs BEFORE the dBm math because HL2's PA
//      attenuator topology is fundamentally different (signed dB attenuator
//      vs. analog PA gain compensation).  Non-HL2 paths fall through to
//      the canonical Thetis dBm kernel below.
//
// Removed in #202 deep-fix: a NereusSDR-original `gbb >= 99.5 → linear
// identity sliderWatts/100` short-circuit.  It inverted the Thetis semantic
// "100 = no output power" (clsHardwareSpecific.cs:463-466 [v2.10.3.13])
// into "100 = full output", which made the Bypass profile and any
// out-of-range Band cast emit wire byte 255 at slider 100.  The Thetis
// kernel at console.cs:46720-46758 always runs; with gbb=100 it produces
// audio_volume ≈ 0.0009 (essentially silent), which is the correct
// "this band has no PA gain row" behavior.
//
// The math itself is the canonical Thetis sequence:
//
//   target_dbm   = 10 * log10(sliderWatts * 1000)
//   gbb          = profile.getGainForBand(band, sliderWatts)
//   target_dbm  -= gbb
//   target_volts = sqrt(10^(target_dbm * 0.1) * 0.05)
//                = sqrt(P * R) where R=50  (E = sqrt(P*R) RMS volts on 50Ω)
//   audio_volume = min(target_volts / 0.8, 1.0)
//
// Always finite (no NaN / Inf) for any int input — even INT_MAX produces a
// finite (then-clamped) result via std::pow.
double TransmitModel::computeAudioVolume(const PaProfile& profile,
                                         Band band,
                                         int sliderWatts,
                                         HPSDRModel model) const noexcept
{
    // From Thetis console.cs:46749-46751 [v2.10.3.13] — sliderWatts == 0 path.
    // Negative slider → also returns 0 (NereusSDR-original safety).
    if (sliderWatts <= 0) {
        return 0.0;
    }

    const float gbb = profile.getGainForBand(band, sliderWatts);

    // From mi0bot-Thetis console.cs:47775-47778 [v2.10.3.13-beta2]:
    //   Audio.RadioVolume = (double)Math.Min((hl2Power * (gbb / 100)) / 93.75, 1.0);  // MI0BOT: We want to jump in steps of 16 but getting 6.
    //                                                                                 // Drive value is 0-255 but only top 4 bits used.
    //                                                                                 // Need to correct for multiplication of 1.02 in Radio volume
    //                                                                                 // Formula - 1/((16/6)/(255/1.02))
    //
    // Branch order: HL2 path runs BEFORE the gbb >= 99.5 sentinel.  On HL2
    // HF bands gbb=100 (sentinel value), but mi0bot uses
    // (hl2Power * gbb/100) / 93.75 directly — the sentinel was a
    // NereusSDR-original linear fallback for radios with no PA-gain
    // compensation; mi0bot has explicit HL2 math.  Without this ordering,
    // HL2 HF bands would short-circuit into the legacy path and never use
    // mi0bot's formula.
    //
    // mi0bot's own comment notes the divisor 93.75 has a known empirical
    // discrepancy with the derived value (~95.6 from 1/((16/6)/(255/1.02))).
    // Copying verbatim per source-first; the discrepancy is upstream-known.
    if (model == HPSDRModel::HERMESLITE) {
        const double hl2Power = static_cast<double>(sliderWatts);
        const double v = (hl2Power * (static_cast<double>(gbb) / 100.0)) / 93.75;
        return std::clamp(v, 0.0, 1.0);
    }

    // No `gbb >= 99.5` linear-identity short-circuit here.  In Thetis
    // (clsHardwareSpecific.cs:463-466 [v2.10.3.13]) the gains array is
    // initialised to 100.0f with the explicit comment:
    //   "max them out, these gains are PA attenuations, so 100 is no output power"
    // and SetPowerUsingTargetDBM (console.cs:46720-46758 [v2.10.3.13]) always
    // runs the dBm kernel — it never short-circuits to a linear identity
    // path.  With gbb=100 the kernel produces target_dbm = -50 dBm at
    // sliderWatts=100, target_volts ≈ 0.0007, audio_volume ≈ 0.0009 — i.e.
    // essentially zero output, which is the correct semantic for the
    // "this band is not handled by my PA gain row" case.
    //
    // A previous NereusSDR-original short-circuit
    //   if (gbb >= 99.5f) return std::clamp(sliderWatts / 100.0, 0.0, 1.0);
    // inverted that semantic to "100 = full output (linear identity)".
    // That made the Bypass profile (kPaGainSentinel = 100.0f every band) and
    // any out-of-range Band emit wire byte 255 at slider 100 — the issue
    // #202 trapdoor.  Removed; the Thetis kernel below now runs for all
    // non-HL2 paths.  HL2 retains its mi0bot-Thetis formula above
    // (TransmitModel.cpp:772-776).

    // From Thetis console.cs:46720-46724 [v2.10.3.13]:
    //   double target_dbm = 10 * (double)Math.Log10((double)new_pwr * 1000);
    //   ...
    //   target_dbm -= gbb;
    const double targetDbmRaw =
        10.0 * std::log10(static_cast<double>(sliderWatts) * 1000.0);
    const double targetDbm = targetDbmRaw - static_cast<double>(gbb);

    // From Thetis console.cs:46734 [v2.10.3.13]:
    //   double target_volts = Math.Sqrt(Math.Pow(10, target_dbm * 0.1) * 0.05);
    //                         // E = Sqrt(P * R)
    const double targetVolts =
        std::sqrt(std::pow(10.0, targetDbm * 0.1) * 0.05);

    // From Thetis console.cs:46758 [v2.10.3.13]:
    //   Audio.RadioVolume = (double)Math.Min((target_volts / 0.8), 1.0);
    const double audioVolume = std::min(targetVolts / 0.8, 1.0);

    // Defensive: clamp lower bound to 0.0.  std::pow / std::sqrt should
    // never return negative for finite input, but guarantee the contract.
    return std::clamp(audioVolume, 0.0, 1.0);
}

// ── Phase 3C state setters (#167) ──────────────────────────────────────────

void TransmitModel::setTwoToneActive(bool active)
{
    if (m_twoToneActive == active) { return; }  // idempotent guard
    // Mirror of TwoToneController state — Thetis chk2TONE.Checked.
    // Runtime-only mirror; not persisted (TwoToneController owns the live
    // state machine and starts OFF every session per its own contract).
    m_twoToneActive = active;
    emit twoToneActiveChanged(active);
}

void TransmitModel::setTuneDrivePowerSource(DrivePowerSource source)
{
    if (m_tuneDrivePowerSource == source) { return; }  // idempotent guard
    // Port of Thetis TuneDrivePowerOrigin setter at console.cs:46554-46575
    // [v2.10.3.13].  Persisted per-MAC under hardware/<mac>/tx/
    // TuneDrivePowerOrigin (mirrors the existing TwoToneDrivePowerOrigin
    // key — same drivePowerSourceToString/From helpers).
    m_tuneDrivePowerSource = source;
    persistOne(QStringLiteral("TuneDrivePowerOrigin"),
               drivePowerSourceToString(source));
    emit tuneDrivePowerSourceChanged(source);
}

void TransmitModel::setTunePower(int watts)
{
    // Port of Thetis tune_power setter at console.cs:17229-17242
    // [v2.10.3.13].  Range clamped to [0, 100] (matches Thetis Designer
    // FixedTunePower spinbox bounds).
    //
    // Issue #175 review fix: ceiling polymorphs on the connected radio
    // model to match setTunePowerForBand (line 475) and spec §6.  HL2
    // mi0bot Tune scale is 0..99 (33 sub-steps; mi0bot
    // console.cs:47616-47666 [v2.10.3.13-beta2]).  Without this gate, a
    // user who stored a Fixed-mode value of 100 on a non-HL2 radio,
    // then connects HL2, would bypass the spec [0, 99] HL2 ceiling.
    const int hi = (m_hpsdrModel == HPSDRModel::HERMESLITE) ? 99 : 100;
    const int clamped = std::clamp(watts, 0, hi);
    if (m_tunePower == clamped) { return; }  // idempotent guard
    m_tunePower = clamped;
    persistOne(QStringLiteral("FixedTunePower"), QString::number(clamped));
    emit tunePowerChanged(clamped);
}

void TransmitModel::setHpsdrModel(HPSDRModel m)
{
    // R-R3-46 follow-up: only the model. A connect sets it before the new
    // radio's settings load, while this object still saves under the
    // previous radio, so clamping here saved the new model's clamp under
    // the old MAC. loadFromSettings() and load() clamp the new radio's
    // values to this model (setTunePower, setTunePowerForBand's range) and
    // save them for that radio.
    m_hpsdrModel = m;
}

void TransmitModel::setTxPostGenToneMag(double mag)
{
    // From mi0bot-Thetis console.cs:47666 [v2.10.3.13-beta2]:
    //   SetTXAPostGenToneMag(0, postGenToneMag);
    // HL2 sub-step DSP audio-gain modulation.  Range 0.4..0.9999 on HL2
    // sub-step path; 1.0 = no modulation (default, non-HL2 path).
    // dedupe; matches NereusSDR setter convention
    if (m_txPostGenToneMag == mag) { return; }
    m_txPostGenToneMag = mag;
    emit txPostGenToneMagChanged(mag);
}

void TransmitModel::setStepAttenuatorController(StepAttenuatorController* ctrl)
{
    // Non-owning pointer.  RadioModel injects the controller on connect;
    // tests inject a controller they own directly.  nullptr -> ATT-on-TX
    // gate becomes a no-op (used in tests + before RadioModel wires up).
    m_stepAttCtrl = ctrl;
}

// ── setPowerUsingTargetDbm deep-parity wrapper (#167 Phase 3C) ──────────────
//
// Full deep-parity port of Thetis SetPowerUsingTargetDBM
// (line-by-line cites embedded inline; entry function header at
//  console.cs:46645 [v2.10.3.13]).  Integrates Phase 3A scaffolding
// (m_powerByBand, ATT-on-TX safety properties, m_lastPower,
// pureSignalActive) with Phase 3B's computeAudioVolume math kernel
// into the unified API used by:
//   - RadioModel drive-slider lambda (txMode 0, normal mode)
//   - RadioModel TUNE handler        (txMode 1, bFromTune=true bTwoTone=false)
//   - TwoToneController on/off       (txMode 2, bFromTune=false bTwoTone=true)
//
// Each tx mode resolves the active slider value differently:
//   - txMode 0 (normal): m_power (PWR slider).  ALSO writes m_powerByBand
//                        as a side-effect (matches Thetis console.cs:46676
//                        power_by_band[(int)_tx_band] = new_pwr).
//   - txMode 1 (tune):   m_power / tunePowerForBand / m_tunePower per
//                        m_tuneDrivePowerSource.
//   - txMode 2 (2tone):  m_power / tunePowerForBand / twoTonePower() per
//                        m_twoToneDrivePowerSource.
//
// bConstrain semantics: false ONLY on the FIXED drive source (matches
// console.cs:46689, 46705).  Caller respects bConstrain by skipping the
// slider clamp; that's the Thetis behaviour that lets a setup-page-fixed
// "10 W tune" land on the wire even if the PWR/TUN sliders disagree.
//
// XVTR translation: Thetis at console.cs:46711-46716 + 46724-46728 retunes
// to the LO band before computing gbb.  NereusSDR has only one XVTR slot
// — the sentinel fallback in computeAudioVolume catches Band::XVTR via
// PaProfile::getGainForBand returning 1000 (Phase 3B short-circuit).  Full
// XVTR LO-band translation is deferred per plan §"Open follow-ups".
TransmitModel::TxPowerResult TransmitModel::setPowerUsingTargetDbm(
    const PaProfile& activeProfile,
    Band currentBand,
    bool bSetPower,
    bool bFromTune,
    bool bTwoTone,
    HPSDRModel model)
{
    TxPowerResult result;
    result.bConstrain = true;
    int new_pwr = 0;
    // Thetis: PrettyTrackBar slider = ptbPWR; set to ptbTune on the
    // TUNE_SLIDER source (console.cs:46724+ [v2.10.3.15]).
    bool sliderIsTune = false;

    // From Thetis console.cs:46651-46669 [v2.10.3.13] — txMode determination.
    //   int txMode = 0; // 0 normal, 1 tune, 2 2tone
    //   if (!MOX && !chkTUN.Checked && !chk2TONE.Checked) {
    //       if (bFromTune) {
    //           if (!bTwoTone) txMode = 1;
    //           else           txMode = 2;
    //       }
    //   } else {
    //       if (chkTUN.Checked)        txMode = 1;
    //       else if (chk2TONE.Checked) txMode = 2;
    //   }
    int txMode = 0;
    if (!m_mox && !m_tune && !m_twoToneActive) {
        if (bFromTune) {
            txMode = bTwoTone ? 2 : 1;
        }
    } else {
        if (m_tune) {
            txMode = 1;
        } else if (m_twoToneActive) {
            txMode = 2;
        }
    }

    // Drive-slider source resolution per Thetis console.cs:46671-46709
    // [v2.10.3.13] — switch on txMode + drive-source enum.
    switch (txMode) {
        case 0:  // normal mode — Thetis console.cs:46673-46676.
            //     case 0: //normal
            //         new_pwr = ptbPWR.Value;
            //         power_by_band[(int)_tx_band] = new_pwr;
            //         break;
            new_pwr = m_power;
            // Side-effect: write back into per-band normal-mode slot.
            // Matches Thetis power_by_band[(int)_tx_band] = new_pwr.
            // setPowerForBand handles the bounds check + clamp + persist
            // + emit.
            setPowerForBand(currentBand, new_pwr);
            break;
        case 1:  // tune mode — Thetis console.cs:46677-46692.
            //     case 1: //tune
            //         switch (_tuneDrivePowerSource) {
            //             case DRIVE_SLIDER: new_pwr = ptbPWR.Value; break;
            //             case TUNE_SLIDER:  slider = ptbTune;
            //                                new_pwr = ptbTune.Value; break;
            //             case FIXED:        new_pwr = tune_power;
            //                                bConstrain = false; break;
            //         }
            //         break;
            switch (m_tuneDrivePowerSource) {
                case DrivePowerSource::DriveSlider:
                    new_pwr = m_power;
                    break;
                case DrivePowerSource::TuneSlider:
                    sliderIsTune = true;  // slider = ptbTune;
                    new_pwr = tunePowerForBand(currentBand);
                    // From mi0bot-Thetis console.cs:47660-47673 [v2.10.3.13-beta2]
                    // MI0BOT: As HL2 only has 15 step output attenuator,
                    //         reduce the level further
                    if (model == HPSDRModel::HERMESLITE) {
                        // if (bConstrain) new_pwr = slider.ConstrainAValue(ptbTune.Value);
                        // (the HL2 tune slider is 0..99, then its limit)
                        if (result.bConstrain) {
                            new_pwr = std::min(std::clamp(new_pwr, 0, 99),
                                               m_tunePowerLimit);
                        }
                        if (new_pwr <= 51) {
                            setTxPostGenToneMag((new_pwr + 40) / 100.0);
                            new_pwr = 0;
                        } else {
                            setTxPostGenToneMag(0.9999);
                            new_pwr = (new_pwr - 54) * 2;
                        }
                    }
                    break;
                case DrivePowerSource::Fixed:
                    new_pwr = m_tunePower;
                    result.bConstrain = false;
                    break;
            }
            break;
        case 2:  // 2-tone mode — Thetis console.cs:46693-46708.
            //     case 2: //2tone
            //         switch (_2ToneDrivePowerSource) {
            //             case DRIVE_SLIDER: new_pwr = ptbPWR.Value; break;
            //             case TUNE_SLIDER:  slider = ptbTune;
            //                                new_pwr = ptbTune.Value; break;
            //             case FIXED:        new_pwr = twotone_tune_power;
            //                                bConstrain = false; break;
            //         }
            //         break;
            switch (m_twoToneDrivePowerSource) {
                case DrivePowerSource::DriveSlider:
                    new_pwr = m_power;
                    break;
                case DrivePowerSource::TuneSlider:
                    sliderIsTune = true;  // slider = ptbTune;
                    new_pwr = tunePowerForBand(currentBand);
                    break;
                case DrivePowerSource::Fixed:
                    new_pwr = m_twoTonePower;
                    result.bConstrain = false;
                    break;
            }
            break;
    }

    // XVTR translation NOT ported here.  Sentinel fallback in
    // computeAudioVolume catches Band::XVTR via PaProfile::getGainForBand
    // returning 1000.  See header comment + plan §"Open follow-ups".

    // From Thetis console.cs:46797-46798 [v2.10.3.15]:
    //     //constrain power
    //     if(bConstrain) new_pwr = slider.ConstrainAValue(new_pwr);
    // PrettyTrackBar.ConstrainAValue [v2.10.3.15]:
    //     if (!_bLimitEnabled || (value <= _nLimitValue)) return value;
    //     else return _nLimitValue;
    // The slider is ptbTune on the TUNE_SLIDER source, else ptbPWR; both
    // have LimitEnabled = true (console.Designer.cs:3686, 3942
    // [v2.10.3.15]).  The two-tone start turns ptbPWR's off
    // (PWRSliderLimitEnabled = false, setup.cs:11154-11158 [v2.10.3.15])
    // around its FIXED source, so the PWR set to the two-tone power and
    // its txMode 0 scroll run past the band's limit; the stop turns it
    // back on.  The 0..100 clamp stands for the slider's own Min/Max.
    // bConstrain==false is the FIXED-drive path: the setup-page value
    // bypasses the slider.
    if (result.bConstrain) {
        new_pwr = std::clamp(new_pwr, 0, 100);
        const bool limitEnabled = sliderIsTune || m_powerSliderLimitEnabled;
        const int limit = sliderIsTune ? m_tunePowerLimit : m_powerLimit;
        if (limitEnabled && new_pwr > limit) {
            new_pwr = limit;
        }
    }

    result.newPower = new_pwr;

    // From Thetis console.cs:46722 [v2.10.3.13]:
    //   double target_dbm = 10 * (double)Math.Log10((double)new_pwr * 1000);
    //   ...
    //   target_dbm -= gbb;
    //   ...
    //   targetdBm = target_dbm;
    //
    // We pre-compute targetDbm (post-gbb-subtraction) so the result struct
    // matches the Thetis `out double targetdBm` semantic.  For the sliderWatts
    // <= 0 short-circuit case this is set to 0.0 (Thetis returns the dBm
    // value unmodified, but the new_pwr==0 branch never reads it — set 0
    // for predictability).
    if (new_pwr <= 0) {
        result.targetDbm = 0.0;
    } else {
        const float gbb = activeProfile.getGainForBand(currentBand, new_pwr);
        result.targetDbm =
            10.0 * std::log10(static_cast<double>(new_pwr) * 1000.0)
            - static_cast<double>(gbb);
    }

    // Math kernel (Phase 3B + #175 Task 5) — translates
    // (sliderWatts, band, profile, model) into [0, 1.0] audio_volume.
    // Pure function; same kernel called from every path so HL2 sentinel +
    // Bypass profile + sliderWatts==0 short-circuits + mi0bot HL2 formula
    // are uniform.  `model` threads the hardware kind through so HL2 takes
    // mi0bot's (hl2Power * gbb/100) / 93.75 path.
    result.audioVolume =
        computeAudioVolume(activeProfile, currentBand, new_pwr, model);

    // From Thetis console.cs:46738 [v2.10.3.13]:
    //   if (!bSetPower) return new_pwr;
    if (!bSetPower) { return result; }

    // ATT-on-TX-on-power-change safety gate.
    // From Thetis console.cs:46740-46748 [v2.10.3.13]:
    //   //[2.10.3.5]MW0LGE max tx attenuation when power is increased and PS is enabled
    //   if (new_pwr != _lastPower && chkFWCATUBypass.Checked && _forceATTwhenPowerChangesWhenPSAon)
    //   {
    //       if(new_pwr > _lastPower || _forceATTwhenPowerChangesWhenPSAon_anddecreased)
    //           SetupForm.ATTOnTX = 31;
    //
    //       _lastPower = new_pwr;
    //   }
    //
    //[2.10.3.5]MW0LGE max tx attenuation when power is increased and PS is enabled
    if (new_pwr != m_lastPower
        && pureSignalActive()
        && m_forceAttwhenPowerChangesWhenPSAon)
    {
        if (new_pwr > m_lastPower
            || m_forceAttwhenPowerChangesWhenPSAonAndDecreased)
        {
            // SetupForm.ATTOnTX = 31  -> StepAttenuatorController::setAttOnTxValue(31).
            // Mirrors mi0bot setup.cs:3988-4017 [v2.10.3.13] ATTOnTX setter
            // (clamps + writes the per-band TX ATT slot for the active band).
            // nullptr controller -> no-op (test seam + pre-RadioModel-wired
            // state).
            if (m_stepAttCtrl) {
                m_stepAttCtrl->setAttOnTxValue(31);
            }
        }
        m_lastPower = new_pwr;
    }

    // From Thetis console.cs:46749-46760 [v2.10.3.13]:
    //   if (new_pwr == 0) { Audio.RadioVolume = 0.0; ... }
    //   else { ... Audio.RadioVolume = (double)Math.Min((target_volts / 0.8), 1.0); }
    //
    // NereusSDR-equivalent: emit audioVolumeChanged so RadioModel can pump
    // the value to TxChannel (iq_gain) + RadioConnection (wire_byte).
    // computeAudioVolume already returns 0.0 for sliderWatts <= 0 (Phase
    // 3B short-circuit), so the same emit handles both branches uniformly.
    //
    // The TXPostGenRun = 0/1 toggle (console.cs:46752-46758) is RadioModel's
    // responsibility — TransmitModel doesn't own the post-gen run state.
    // RadioModel will gate it on bFromTune + (new_pwr > 0) at the call site.
    emit audioVolumeChanged(result.audioVolume);
    return result;
}

void TransmitModel::setMacAddress(const QString& mac)
{
    m_mac = mac;
}

void TransmitModel::load()
{
    // No-op when no MAC scope is set.
    if (m_mac.isEmpty()) {
        return;
    }
    // Cite: console.cs:4904-4910 [v2.10.3.13] — Thetis pipe-delimited restore.
    // NereusSDR uses per-band scalar keys matching the AlexController pattern.
    //
    // Author-tag preservation (CLAUDE.md GPL rule): the upstream restore loop
    // at console.cs:4906 [v2.10.3.13] carries
    //   if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE
    // This is a length-mismatch guard against the pipe-delimited string format.
    // The NereusSDR scalar-key path doesn't have a list-length to check (each
    // band's value is read independently with its own default), so the guard
    // has no direct equivalent.  The author tag is preserved here per the
    // CLAUDE.md byte-for-byte rule:
    //   //[2.10.3.5]MW0LGE  [original guard from console.cs:4906]
    auto& s = AppSettings::instance();
    const QString prefix =
        QStringLiteral("hardware/%1/tunePowerByBand/").arg(m_mac);
    // Issue #175 review fix: ceiling polymorphs on the connected radio
    // model to match setTunePowerForBand (line 475) and spec §6.  HL2
    // mi0bot Tune scale is 0..99 (33 sub-steps; mi0bot
    // console.cs:47616-47666 [v2.10.3.13-beta2]).  Without this gate, a
    // user who stored 100 on a non-HL2 radio, then connects HL2, would
    // load 100 into the array instead of being clamped to 99.  Requires
    // setHpsdrModel() to have been called before load() — the connect
    // sequence in RadioModel::connectToRadio sets m_hpsdrModel via
    // setHpsdrModel(m_hardwareProfile.model) before invoking load().
    const int hi = (m_hpsdrModel == HPSDRModel::HERMESLITE) ? 99 : 100;
    for (int i = 0; i < kBandCount; ++i) {
        // Keyed by the band's number (2 m is 27), as before for 0-13.
        const QString key =
            prefix + QString::number(static_cast<int>(bandFromPerBandStateSlot(i)));
        const int v = s.value(key, QStringLiteral("50")).toInt();
        m_tunePowerByBand[static_cast<std::size_t>(i)] = std::clamp(v, 0, hi);
    }
    refreshTunePowerForTxBand();
    // R-R3-49 (parity Task 5): the restore bypasses the per-band setter, so
    // the link's copy is told here.
    emit tunePowerByBandJsonChanged(tunePowerByBandJson());
}

void TransmitModel::save()
{
    // No-op when no MAC scope is set.
    if (m_mac.isEmpty()) {
        return;
    }
    // Cite: console.cs:3087-3091 [v2.10.3.13] — Thetis pipe-delimited save.
    // NereusSDR uses per-band scalar keys matching the AlexController pattern.
    //
    // Like AlexController::save(), this method only writes to the in-memory
    // AppSettings map; it does NOT call AppSettings::save() (full XML flush).
    // Callers schedule the disk flush at the appropriate time (teardown /
    // app-exit / explicit user-save), not on every per-band setter.
    // Calling s.save() here would trigger a full XML rewrite on every
    // saveSliceState() call (debounced 500 ms during active TX/UI use).
    auto& s = AppSettings::instance();
    const QString prefix =
        QStringLiteral("hardware/%1/tunePowerByBand/").arg(m_mac);
    for (int i = 0; i < kBandCount; ++i) {
        s.setValue(prefix + QString::number(static_cast<int>(bandFromPerBandStateSlot(i))),
                   QString::number(m_tunePowerByBand[static_cast<std::size_t>(i)]));
    }
}

// ── Per-MAC mic/VOX/MON persistence (3M-1b L.2) ─────────────────────────────
//
// NereusSDR-native persistence glue.  Key namespace: hardware/<mac>/tx/<key>.
//
// Three properties are intentionally excluded (per plan §0 rows 8 and 9):
//   - voxEnabled  → always loads false  (safety: VOX always starts OFF)
//   - monEnabled  → always loads false  (safety: MON always starts OFF)
//   - micMute     → always loads true   (safety: mic in use on startup)
//
// The auto-persist pattern mirrors CalibrationController::persist(key, value):
//   each setter calls persistOne(key, value) after updating the member, and
//   persistOne() no-ops when m_persistMac is empty (before loadFromSettings).
//
// All boolean properties are stored as "True"/"False" per the AppSettings
// convention (same as every other NereusSDR boolean persistence site).
// Numeric properties (int, double, float) are stored as decimal strings.
// MicSource is stored as "Pc" / "Radio" to match the enum naming.

void TransmitModel::persistOne(const QString& key, const QVariant& value) const
{
    if (m_persistMac.isEmpty()) {
        return;
    }
    AppSettings::instance().setValue(
        QStringLiteral("hardware/%1/tx/%2").arg(m_persistMac, key),
        value.toString());
}

void TransmitModel::loadFromSettings(const QString& mac)
{
    m_persistMac = mac;
    auto& s = AppSettings::instance();
    const QString pfx = QStringLiteral("hardware/%1/tx/").arg(mac);

    // ── micGainDb (default -6 per plan §0 row 11) ────────────────────────
    const int micGainDb = s.value(pfx + QLatin1String("MicGain"),
                                   QStringLiteral("-6")).toInt();
    setMicGainDb(micGainDb);

    // ── Mic-jack flag properties ──────────────────────────────────────────
    // micMute: NEVER loaded (safety default true = mic in use).
    // micBoost: default true (console.cs:13237 [v2.10.3.13])
    const bool micBoost = s.value(pfx + QLatin1String("Mic_Input_Boost"),
                                   QStringLiteral("True")).toString() == QLatin1String("True");
    setMicBoost(micBoost);
    // micXlr: default true (console.cs:13249 [v2.10.3.13])
    const bool micXlr = s.value(pfx + QLatin1String("Mic_XLR"),
                                  QStringLiteral("True")).toString() == QLatin1String("True");
    setMicXlr(micXlr);
    // lineIn: default false (console.cs:13213 [v2.10.3.13])
    const bool lineIn = s.value(pfx + QLatin1String("Line_Input_On"),
                                  QStringLiteral("False")).toString() == QLatin1String("True");
    setLineIn(lineIn);
    // lineInBoost: default 0.0 (console.cs:13225 [v2.10.3.13])
    const double lineInBoost = s.value(pfx + QLatin1String("Line_Input_Level"),
                                        QStringLiteral("0")).toDouble();
    setLineInBoost(lineInBoost);
    // micTipRing: default true (setup.designer.cs:8683 [v2.10.3.13])
    const bool micTipRing = s.value(pfx + QLatin1String("Mic_TipRing"),
                                     QStringLiteral("True")).toString() == QLatin1String("True");
    setMicTipRing(micTipRing);
    // micBias: default false (setup.designer.cs:8779 [v2.10.3.13])
    const bool micBias = s.value(pfx + QLatin1String("Mic_Bias"),
                                   QStringLiteral("False")).toString() == QLatin1String("True");
    setMicBias(micBias);
    // micPttDisabled: default false (console.cs:19757 [v2.10.3.13])
    const bool micPttDisabled = s.value(pfx + QLatin1String("Mic_PTT_Disabled"),
                                         QStringLiteral("False")).toString() == QLatin1String("True");
    setMicPttDisabled(micPttDisabled);

    // ── line_in_gain + user_dig_out (Task 2.4 of P1 full-parity epic) ────
    // Defaults from Thetis ChannelMaster/networkproto1.c:600-601 [v2.10.3.13]:
    //   line_in_gain default 0 (no line-in attenuation),
    //   user_dig_out default 0 (all 4 user digital pins low).
    // Radio codec lane (2026-09-30): the line-in gain index is derived
    // from lineInBoost, as Thetis SetMicGain derives it (console.cs:40928-40932
    // [v2.10.3.15]), so a stored LineInGain no longer overrides the dB value.
    setLineInGain(lineInGainIndexForBoost(m_lineInBoost));
    const int userDigOut = s.value(pfx + QLatin1String("UserDigOut"),
                                     QStringLiteral("0")).toInt();
    setUserDigOut(userDigOut);

    // ── pureSig — Phase 3M-4 Task 15: per-MAC read removed ───────────────
    // The hardware/<mac>/pureSignal/enabled key was the original Task 2.5
    // P1-full-parity proxy bridge driven by the (now-retired) Setup →
    // Hardware → PureSignal tab.  Phase 3M-4 Task 14 retired that tab; no
    // live writer of the per-MAC key remains in the codebase.
    //
    // Per Phase 3M-4 design doc §9.1 the canonical PS-enable persistence
    // path is per-TX-profile via MicProfileManager Pure_Signal_Enabled
    // (Task 7) — Thetis matches: PSEnabled is implicit-via-profile-recall,
    // not stored as a per-radio sticky.  All 19+ stock factory profiles
    // default to false, matching PSForm.cs:234 [v2.10.3.13] _psenabled =
    // false initial state.
    //
    // The model property remains the single source of truth at runtime;
    // PsForm + the PureSignal coordinator write through the model API,
    // and per-profile recall flips it via the existing setter.

    // ── VOX properties (voxEnabled NOT loaded — safety: always false) ─────
    const int voxThresholdDb = s.value(pfx + QLatin1String("Dexp_Threshold"),
                                        QStringLiteral("-40")).toInt();
    setVoxThresholdDb(voxThresholdDb);
    const float voxGainScalar = s.value(pfx + QLatin1String("VOX_GainScalar"),
                                         QStringLiteral("1")).toFloat();
    setVoxGainScalar(voxGainScalar);
    const int voxHangTimeMs = s.value(pfx + QLatin1String("VOX_HangTime"),
                                       QStringLiteral("500")).toInt();
    setVoxHangTimeMs(voxHangTimeMs);

    // ── DEXP envelope properties (3M-3a-iii Task 7) — ALL persist ─────────
    // Defaults from Thetis setup.Designer.cs [v2.10.3.13]:
    //   chkDEXPEnable: WinForms default false (line 45140-45151)
    //   udDEXPDetTau.Value=20  (line 45093)
    //   udDEXPAttack.Value=2   (line 45050)
    //   udDEXPRelease.Value=100 (line 44990)
    setDexpEnabled(s.value(pfx + QLatin1String("DEXP_Enabled"),
                            QStringLiteral("False")).toString() == QLatin1String("True"));
    setDexpDetectorTauMs(s.value(pfx + QLatin1String("DEXP_DetectorTauMs"),
                                  QStringLiteral("20")).toDouble());
    setDexpAttackTimeMs(s.value(pfx + QLatin1String("DEXP_AttackTimeMs"),
                                 QStringLiteral("2")).toDouble());
    setDexpReleaseTimeMs(s.value(pfx + QLatin1String("DEXP_ReleaseTimeMs"),
                                  QStringLiteral("100")).toDouble());

    // ── DEXP gate-ratio properties (3M-3a-iii Task 8) — both persist ──────
    // Defaults from Thetis setup.Designer.cs [v2.10.3.13]:
    //   udDEXPExpansionRatio.Value=10            (line 44900-44904)
    //   udDEXPHysteresisRatio.Value=20 -> 2.0    (line 44869-44873; scale 65536)
    setDexpExpansionRatioDb(s.value(pfx + QLatin1String("DEXP_ExpansionRatioDb"),
                                     QStringLiteral("10")).toDouble());
    setDexpHysteresisRatioDb(s.value(pfx + QLatin1String("DEXP_HysteresisRatioDb"),
                                      QStringLiteral("2")).toDouble());

    // ── DEXP look-ahead properties (3M-3a-iii Task 9) — both persist ──────
    // Defaults from Thetis setup.Designer.cs [v2.10.3.13]:
    //   chkDEXPLookAheadEnable.Checked=true (line 44808)
    //   udDEXPLookAhead.Value=60            (line 44788)
    setDexpLookAheadEnabled(s.value(pfx + QLatin1String("DEXP_LookAheadEnabled"),
                                     QStringLiteral("True")).toString() == QLatin1String("True"));
    setDexpLookAheadMs(s.value(pfx + QLatin1String("DEXP_LookAheadMs"),
                                QStringLiteral("60")).toDouble());

    // ── DEXP side-channel filter properties (3M-3a-iii Task 10) — all persist ─
    // Defaults from Thetis setup.Designer.cs [v2.10.3.13]:
    //   udSCFLowCut.Value=500     (line 45240)
    //   udSCFHighCut.Value=1500   (line 45210)
    //   chkSCFEnable.Checked=true (line 45250)
    setDexpLowCutHz(s.value(pfx + QLatin1String("DEXP_LowCutHz"),
                             QStringLiteral("500")).toDouble());
    setDexpHighCutHz(s.value(pfx + QLatin1String("DEXP_HighCutHz"),
                              QStringLiteral("1500")).toDouble());
    setDexpSideChannelFilterEnabled(
        s.value(pfx + QLatin1String("DEXP_SideChannelFilterEnabled"),
                QStringLiteral("True")).toString() == QLatin1String("True"));

    // ── Anti-VOX properties ───────────────────────────────────────────────
    // antiVoxGainDb: default 0 (NereusSDR-original safe starting point)
    const int antiVoxGainDb = s.value(pfx + QLatin1String("AntiVox_Gain"),
                                       QStringLiteral("0")).toInt();
    setAntiVoxGainDb(antiVoxGainDb);
    // 3M-3a-iv post-bench refactor (Option A): AntiVox_Source_VAX read dropped.
    // Existing user settings carrying this key will leave it as an orphan in
    // AppSettings; AppSettings ignores unknown keys on load (no migration).
    // antiVoxTauMs: default kAntiVoxTauMsDefault (=20) from Thetis
    // setup.designer.cs:44682 [v2.10.3.13] (udAntiVoxTau.Value=20).  Phase 3M-3a-iv Task 8.
    const int antiVoxTauMs = s.value(pfx + QLatin1String("AntiVox_Tau_Ms"),
                                      QString::number(kAntiVoxTauMsDefault)).toInt();
    setAntiVoxTauMs(antiVoxTauMs);
    // antiVoxRun: default false from Thetis chkAntiVoxEnable (initially
    // unchecked; no .Checked= setter at setup.designer.cs:44740-44751
    // [v2.10.3.13]).  3M-3a-iv scope-expansion.
    const bool antiVoxRun = s.value(pfx + QLatin1String("AntiVox_Enable"),
                                     QStringLiteral("False")).toString() == QLatin1String("True");
    setAntiVoxRun(antiVoxRun);
    // paSettingsBypass: default false (D4: ANAN-G2E port).
    // From Thetis setup.cs:19921 [v2.10.3.15] //N1GP G2E added —
    //   chkBypassANANPASettings.Visible = true (visibility only; no default
    //   .Checked= in Thetis v2.10.3.15, so NereusSDR defaults to false).
    const bool paSettingsBypass = s.value(pfx + QLatin1String("PaSettingsBypass"),
                                           QStringLiteral("False")).toString()
                                     == QLatin1String("True");
    setPaSettingsBypass(paSettingsBypass);

    // ── MON properties (monEnabled NOT loaded — safety: always false) ─────
    // monitorVolume: default 0.5f (audio.cs:417 [v2.10.3.13] literal)
    const float monitorVolume = s.value(pfx + QLatin1String("MonitorVolume"),
                                         QStringLiteral("0.5")).toFloat();
    setMonitorVolume(monitorVolume);

    // ── Mic source ────────────────────────────────────────────────────────
    // micSource: default Pc (NereusSDR-native; always safe and available).
    //
    // Lookup order (eager-borg-d64bed, 2026-05-06):
    //   1. Per-MAC key (hardware/<mac>/tx/Mic_Source) — explicit choice for
    //      this radio.  Always wins when present.
    //   2. Pre-connect global key (tx/preconnect/Mic_Source) — set by
    //      setMicSource() when the user picks a source before connecting
    //      to any radio.  Acts as a fallback so the choice survives an
    //      app restart and carries to the first connected radio.
    //   3. Default "Pc" — first-run, never-clicked baseline.
    //
    // Step 1 uses an empty-string sentinel rather than "Pc" so an actually-
    // missing per-MAC key falls through to step 2; an explicit per-MAC "Pc"
    // (user clicked PC Mic for this specific radio) wins over preconnect.
    const QString perMacStr = s.value(pfx + QLatin1String("Mic_Source"),
                                       QString()).toString();
    QString micSourceStr;
    if (perMacStr.isEmpty()) {
        micSourceStr = AppSettings::instance().value(
            QStringLiteral("tx/preconnect/Mic_Source"),
            QStringLiteral("Pc")).toString();
    } else {
        micSourceStr = perMacStr;
    }
    MicSource micSource = MicSource::Pc;
    if (micSourceStr == QLatin1String("Radio")) {
        micSource = MicSource::Radio;
    } else if (micSourceStr == QLatin1String("Vax")) {
        micSource = MicSource::Vax;
    }
    setMicSource(micSource);

    // ── Mic source previous (PhoneCwApplet VAX-toggle restore target) ─────
    // Lookup order matches Mic_Source: per-MAC -> preconnect -> default Pc.
    const QString perMacPreVaxStr = s.value(pfx + QLatin1String("Mic_Source_PreVax"),
                                             QString()).toString();
    QString preVaxStr;
    if (perMacPreVaxStr.isEmpty()) {
        preVaxStr = AppSettings::instance().value(
            QStringLiteral("tx/preconnect/Mic_Source_PreVax"),
            QStringLiteral("Pc")).toString();
    } else {
        preVaxStr = perMacPreVaxStr;
    }
    m_previousNonVaxMicSource = (preVaxStr == QLatin1String("Radio"))
                                    ? MicSource::Radio
                                    : MicSource::Pc;

    // ── Two-tone test properties (3M-1c B.2) ──────────────────────────────
    // Defaults per design spec §4.4 (option C):
    //   Freq1=700, Freq2=1900 — match Thetis Designer + btnTwoToneF_defaults.
    //   Level=-6, Power=50    — NereusSDR-original safer (Designer 0/10).
    //   Freq2Delay=0          — match Thetis Designer.
    //   Invert=true           — Designer chkInvertTones.Checked = true.
    //   Pulsed=false          — Designer (no Checked= line).
    const int twoToneFreq1 = s.value(pfx + QLatin1String("TwoToneFreq1"),
                                       QStringLiteral("700")).toInt();
    setTwoToneFreq1(twoToneFreq1);
    const int twoToneFreq2 = s.value(pfx + QLatin1String("TwoToneFreq2"),
                                       QStringLiteral("1900")).toInt();
    setTwoToneFreq2(twoToneFreq2);
    // From Thetis setup.Designer.cs:61994-62003 [v2.10.3.13] udTwoToneLevel
    // default 0 dB.  Phase 3M-4 Task 17: was NereusSDR-original -6 dB which
    // halved the 2-tone envelope and starved calcc LCOLLECT bin filling.
    const double twoToneLevel = s.value(pfx + QLatin1String("TwoToneLevel"),
                                         QStringLiteral("0")).toDouble();
    setTwoToneLevel(twoToneLevel);
    const int twoTonePower = s.value(pfx + QLatin1String("TwoTonePower"),
                                      QStringLiteral("50")).toInt();
    setTwoTonePower(twoTonePower);
    const int twoToneFreq2Delay = s.value(pfx + QLatin1String("TwoToneFreq2Delay"),
                                           QStringLiteral("0")).toInt();
    setTwoToneFreq2Delay(twoToneFreq2Delay);
    const bool twoToneInvert = s.value(pfx + QLatin1String("TwoToneInvert"),
                                        QStringLiteral("True")).toString() == QLatin1String("True");
    setTwoToneInvert(twoToneInvert);
    const bool twoTonePulsed = s.value(pfx + QLatin1String("TwoTonePulsed"),
                                        QStringLiteral("False")).toString() == QLatin1String("True");
    setTwoTonePulsed(twoTonePulsed);

    // ── Two-tone drive-power source (3M-1c B.3) ──────────────────────────
    // Default DriveSlider per Thetis console.cs:46553 [v2.10.3.13].
    const QString drivePowerSourceStr = s.value(pfx + QLatin1String("TwoToneDrivePowerOrigin"),
                                                 QStringLiteral("DriveSlider")).toString();
    setTwoToneDrivePowerSource(drivePowerSourceFromString(drivePowerSourceStr));

    // ── TX EQ + Leveler + ALC properties (3M-3a-i Task C) ────────────────
    // Defaults match Thetis database.cs:4552-4594 [v2.10.3.13] (TXProfile schema)
    // and WDSP TXA.c:111-128 [v2.10.3.13] (create_eqp G[]/F[] vectors).
    setTxEqEnabled(s.value(pfx + QLatin1String("TXEQEnabled"),
                            QStringLiteral("False")).toString() == QLatin1String("True"));
    setTxEqPreamp(s.value(pfx + QLatin1String("TXEQPreamp"), QStringLiteral("0")).toInt());
    // WDSP TXA.c:113 default_G[1..10] = {-12, -12, -12, -1, +1, +4, +9, +12, -10, -10}.
    static constexpr int kDefaultG[10] = {-12, -12, -12, -1, 1, 4, 9, 12, -10, -10};
    // WDSP TXA.c:112 default_F[1..10] = {32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000}.
    static constexpr int kDefaultF[10] = {32, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    for (int i = 0; i < 10; ++i) {
        const QString gKey = QStringLiteral("TXEQ%1").arg(i + 1);
        const int g = s.value(pfx + gKey, QString::number(kDefaultG[i])).toInt();
        setTxEqBand(i, g);
        const QString fKey = QStringLiteral("TxEqFreq%1").arg(i + 1);
        const int f = s.value(pfx + fKey, QString::number(kDefaultF[i])).toInt();
        setTxEqFreq(i, f);
    }

    // Leveler — defaults from database.cs:4584-4588 [v2.10.3.13].
    setTxLevelerOn(s.value(pfx + QLatin1String("Lev_On"),
                            QStringLiteral("True")).toString() == QLatin1String("True"));
    setTxLevelerMaxGain(s.value(pfx + QLatin1String("Lev_MaxGain"),
                                 QStringLiteral("15")).toInt());
    setTxLevelerDecay(s.value(pfx + QLatin1String("Lev_Decay"),
                               QStringLiteral("100")).toInt());

    // ALC — defaults from database.cs:4592-4594 [v2.10.3.13].
    setTxAlcMaxGain(s.value(pfx + QLatin1String("ALC_MaximumGain"),
                             QStringLiteral("3")).toInt());
    setTxAlcDecay(s.value(pfx + QLatin1String("ALC_Decay"),
                           QStringLiteral("10")).toInt());

    // EQ globals — defaults from WDSP TXA.c:118-127 [v2.10.3.13].
    setTxEqNc(s.value(pfx + QLatin1String("eq/nc"), QStringLiteral("2048")).toInt());
    setTxEqMp(s.value(pfx + QLatin1String("eq/mp"),
                       QStringLiteral("False")).toString() == QLatin1String("True"));
    setTxEqCtfmode(s.value(pfx + QLatin1String("eq/ctfmode"), QStringLiteral("0")).toInt());
    setTxEqWintype(s.value(pfx + QLatin1String("eq/wintype"), QStringLiteral("0")).toInt());

    // TX EQ parametric blob — opaque string round-trip.  Phase 3M-3a-ii
    // follow-up Batch 6.  No Thetis database.cs default — TXProfile column
    // ships empty until ucParametricEq populates it.
    setTxEqParaEqData(s.value(pfx + QLatin1String("TXParaEQData"),
                                QStringLiteral("")).toString());

    // R-R3-49 (parity Task 4): the Legacy EQ box, default true (Thetis
    // eqform.cs:988 [v2.10.3.15]). It used to be this computer's setting
    // (TxEqDialog/UsingLegacyEQ); a radio with no value of its own takes
    // that one once, so a user who chose the parametric EQ keeps it.
    {
        const QString key = pfx + QLatin1String("EQUseLegacy");
        const QString computerKey = QStringLiteral("TxEqDialog/UsingLegacyEQ");
        QString stored = s.value(key).toString();
        if (!s.contains(key) && s.contains(computerKey)) {
            stored = s.value(computerKey).toString();
            s.setValue(key, stored == QLatin1String("False")
                                ? QStringLiteral("False") : QStringLiteral("True"));
        }
        setTxEqUseLegacy(stored != QLatin1String("False"));
    }

    // ── Phase Rotator (3M-3a-ii Batch 2) ──────────────────────────────────
    // Defaults from Thetis database.cs:4726-4730 [v2.10.3.13].
    setPhaseRotatorEnabled(s.value(pfx + QLatin1String("CFCPhaseRotatorEnabled"),
                                    QStringLiteral("False")).toString() == QLatin1String("True"));
    setPhaseReverseEnabled(s.value(pfx + QLatin1String("CFCPhaseReverseEnabled"),
                                    QStringLiteral("False")).toString() == QLatin1String("True"));
    setPhaseRotatorFreqHz(s.value(pfx + QLatin1String("CFCPhaseRotatorFreq"),
                                   QStringLiteral("338")).toInt());
    setPhaseRotatorStages(s.value(pfx + QLatin1String("CFCPhaseRotatorStages"),
                                   QStringLiteral("8")).toInt());

    // ── CFC scalars (3M-3a-ii Batch 2) ────────────────────────────────────
    // Defaults from Thetis database.cs:4724-4733 [v2.10.3.13].
    beginCfcProfileRestore();
    setCfcEnabled(s.value(pfx + QLatin1String("CFCEnabled"),
                           QStringLiteral("False")).toString() == QLatin1String("True"));
    setCfcPostEqEnabled(s.value(pfx + QLatin1String("CFCPostEqEnabled"),
                                 QStringLiteral("False")).toString() == QLatin1String("True"));
    setCfcPrecompDb(s.value(pfx + QLatin1String("CFCPreComp"),
                             QStringLiteral("0")).toInt());
    setCfcPostEqGainDb(s.value(pfx + QLatin1String("CFCPostEqGain"),
                                QStringLiteral("0")).toInt());

    // ── CFC per-band arrays (3M-3a-ii Batch 2) ────────────────────────────
    // Defaults from Thetis database.cs:4735-4766 [v2.10.3.13]:
    //   CFCEqFreq0..9       = {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000}
    //   CFCPreComp0..9      = all 5 (per-band G[] compression amounts)
    //   CFCPostEqGain0..9   = all 0 (per-band E[] post-EQ gains)
    static constexpr int kDefaultCfcFreq[10] =
        {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000};
    for (int i = 0; i < 10; ++i) {
        const QString fKey = QStringLiteral("CFCEqFreq%1").arg(i);
        const int f = s.value(pfx + fKey, QString::number(kDefaultCfcFreq[i])).toInt();
        setCfcEqFreq(i, f);

        const QString cKey = QStringLiteral("CFCPreComp%1").arg(i);
        const int c = s.value(pfx + cKey, QStringLiteral("5")).toInt();
        setCfcCompression(i, c);

        const QString gKey = QStringLiteral("CFCPostEqGain%1").arg(i);
        const int g = s.value(pfx + gKey, QStringLiteral("0")).toInt();
        setCfcPostEqBandGain(i, g);
    }

    // CFC parametric-EQ blob — opaque string round-trip.
    setCfcParaEqData(s.value(pfx + QLatin1String("CFCParaEQData"),
                              QStringLiteral("")).toString());
    endCfcProfileRestore();
    emit cfcSettingsReloaded();

    // ── CPDR (3M-3a-ii Batch 2) ───────────────────────────────────────────
    // cpdrOn lives at hardware/<mac>/tx/cpdr/on — outside the per-profile
    // namespace, per Thetis console.cs:36430 (global console state).
    setCpdrOn(s.value(pfx + QLatin1String("cpdr/on"),
                       QStringLiteral("False")).toString() == QLatin1String("True"));
    // CompanderLevel from database.cs:4580 [v2.10.3.13]: default 2 dB.
    setCpdrLevelDb(s.value(pfx + QLatin1String("CompanderLevel"),
                            QStringLiteral("2")).toInt());
    // AM_Carrier_Level from Thetis database.cs AddTXProfileTable: default 100 %.
    setAmCarrierLevel(s.value(pfx + QLatin1String("AM_Carrier_Level"),
                              QStringLiteral("100")).toInt());

    // ── CESSB (3M-3a-ii Batch 2) ──────────────────────────────────────────
    // Default from Thetis database.cs:4689 [v2.10.3.13]: dr["CESSB_On"] = false.
    setCessbOn(s.value(pfx + QLatin1String("CESSB_On"),
                        QStringLiteral("False")).toString() == QLatin1String("True"));

    // ── TX filter bandwidth (Plan 4 D1) ───────────────────────────────────
    // Defaults 100/2900 — USB voice typical SSB (NereusSDR-original, Plan 4
    // spec §Task 2).
    setFilterLow(s.value(pfx + QLatin1String("FilterLow"),
                          QStringLiteral("100")).toInt());
    setFilterHigh(s.value(pfx + QLatin1String("FilterHigh"),
                           QStringLiteral("2900")).toInt());

    // ── PA-cal hotfix scaffolding (#167 Phase 3A) ─────────────────────────
    //
    // Per-band normal-mode power array.  Lives under a SEPARATE top-level
    // scope (hardware/<mac>/powerByBand/), parallel to tunePowerByBand —
    // not nested under tx/.  Default 50 W per band on first init.
    // From Thetis console.cs:1813-1814 [v2.10.3.13] — power_by_band default.
    {
        const QString powerPfx =
            QStringLiteral("hardware/%1/powerByBand/").arg(mac);
        for (int i = 0; i < kBandCount; ++i) {
            const Band band = bandFromPerBandStateSlot(i);
            const QString key = powerPfx + bandKeyName(band);
            const int v = s.value(key, QStringLiteral("50")).toInt();
            // Direct assignment (bypass setPowerForBand) — load is the
            // canonical state restore; setter would re-persist, emit, and
            // clamp.  We clamp here ourselves to keep AppSettings tampering
            // safe.
            m_powerByBand[static_cast<std::size_t>(i)] = std::clamp(v, 0, 100);
        }
        // R-R3-49 (parity Task 5): as for tunePowerByBand in load().
        emit powerByBandJsonChanged(powerByBandJson());
    }

    // R-R3-49: per-band slider limits and FM TX offsets, same scope.
    // From Thetis console.cs:4921-4944 [v2.10.3.15] (the pipe-delimited
    // restore; NereusSDR uses per-band scalar keys, so a missing key falls
    // back to the default per band rather than skipping the whole list):
    //   if (list.Length != (int)Band.LAST) continue; //[2.10.3.5]MW0LGE
    {
        const QString limitPfx =
            QStringLiteral("hardware/%1/limitPowerByBand/").arg(mac);
        const QString limitTunePfx =
            QStringLiteral("hardware/%1/limitTunePowerByBand/").arg(mac);
        const QString fmPfx =
            QStringLiteral("hardware/%1/fmTxOffsetByBandMhz/").arg(mac);
        for (int i = 0; i < kBandCount; ++i) {
            const Band band = bandFromPerBandStateSlot(i);
            const auto slot = static_cast<std::size_t>(i);
            m_limitPowerByBand[slot] = std::clamp(
                s.value(limitPfx + bandKeyName(band), QStringLiteral("100"))
                    .toInt(), 0, 100);
            m_limitTunePowerByBand[slot] = std::clamp(
                s.value(limitTunePfx + bandKeyName(band), QStringLiteral("100"))
                    .toInt(), 0, 100);
            bool ok = false;
            const double fm =
                s.value(fmPfx + bandKeyName(band)).toString().toDouble(&ok);
            m_fmTxOffsetByBandMhz[slot] =
                ok ? validFmTxOffsetMhz(band, fm) : defaultFmTxOffsetMhz(band);
        }
    }

    // 3 ATT-on-TX-on-power-change safety properties.
    // Defaults match Thetis console.cs:29285-29310 [v2.10.3.13]:
    //   PSAoff = true (//MW0LGE [2.9.0.7]),
    //   PSAon  = true (//MW0LGE [2.9.3.5]),
    //   PSAonAndDecreased = false.
    setForceAttwhenPSAoff(
        s.value(pfx + QLatin1String("ForceATTwhenPSAoff"),
                QStringLiteral("True")).toString() == QLatin1String("True"));
    setForceAttwhenPowerChangesWhenPSAon(
        s.value(pfx + QLatin1String("ForceATTwhenOutputPowerChangesWhenPSAon"),
                QStringLiteral("True")).toString() == QLatin1String("True"));
    setForceAttwhenPowerChangesWhenPSAonAndDecreased(
        s.value(pfx + QLatin1String("ForceATTwhenOutputPowerChangesWhenPSAonAndDecreased"),
                QStringLiteral("False")).toString() == QLatin1String("True"));

    // m_lastPower: runtime-only sentinel (-1).  NOT loaded — matches Thetis
    // ephemeral `private float _lastPower = -1` (console.cs:29292
    // [v2.10.3.13]).  Reset to -1 here so that
    //   setForceAttwhenPowerChangesWhenPSAon(...)
    // calls above couldn't land us in an unexpected state if a previous
    // session left m_lastPower at some non-sentinel value.  Belt-and-braces.
    m_lastPower = -1;

    // ── PA-cal hotfix Phase 3C state ──────────────────────────────────────
    // m_twoToneActive: runtime-only mirror of TwoToneController.  Always
    // starts false on load (TwoToneController doesn't persist its run
    // state — safety: 2-tone test always starts OFF every session).  No
    // explicit reset needed; default is false from class init.
    //
    // m_tuneDrivePowerSource: persisted per-MAC under TuneDrivePowerOrigin.
    // Default DriveSlider per Thetis console.cs:46552 [v2.10.3.13].
    setTuneDrivePowerSource(drivePowerSourceFromString(
        s.value(pfx + QLatin1String("TuneDrivePowerOrigin"),
                QStringLiteral("DriveSlider")).toString()));
    // m_tunePower: persisted per-MAC under FixedTunePower.
    // Default 10 W (NereusSDR-original safer; Thetis Designer ships 0).
    // R-R3-46: clamped to the connected model (setHpsdrModel runs first on
    // connect), and a stored value out of this model's range is saved back
    // clamped for this radio even when the value in memory already equals
    // the clamp (setTunePower's idempotent guard would skip the save).
    const QString storedTune = s.value(pfx + QLatin1String("FixedTunePower"),
                                       QStringLiteral("10")).toString();
    setTunePower(storedTune.toInt());
    if (storedTune != QString::number(m_tunePower)
        && s.contains(pfx + QLatin1String("FixedTunePower"))) {
        persistOne(QStringLiteral("FixedTunePower"), QString::number(m_tunePower));
    }
}

void TransmitModel::persistToSettings(const QString& mac) const
{
    auto& s = AppSettings::instance();
    const QString pfx = QStringLiteral("hardware/%1/tx/").arg(mac);

    // ── micGainDb ─────────────────────────────────────────────────────────
    s.setValue(pfx + QLatin1String("MicGain"),        QString::number(m_micGainDb));

    // ── Mic-jack flag properties (micMute excluded — safety) ─────────────
    s.setValue(pfx + QLatin1String("Mic_Input_Boost"),         m_micBoost        ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("Mic_XLR"),           m_micXlr          ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("Line_Input_On"),           m_lineIn          ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("Line_Input_Level"),      QString::number(m_lineInBoost));
    s.setValue(pfx + QLatin1String("Mic_TipRing"),       m_micTipRing      ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("Mic_Bias"),          m_micBias         ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("Mic_PTT_Disabled"),   m_micPttDisabled  ? QStringLiteral("True") : QStringLiteral("False"));

    // ── line_in_gain + user_dig_out (Task 2.4) ───────────────────────────
    s.setValue(pfx + QLatin1String("LineInGain"),         QString::number(m_lineInGain));
    s.setValue(pfx + QLatin1String("UserDigOut"),         QString::number(m_userDigOut));

    // ── VOX properties (voxEnabled excluded — safety) ────────────────────
    s.setValue(pfx + QLatin1String("Dexp_Threshold"),   QString::number(m_voxThresholdDb));
    s.setValue(pfx + QLatin1String("VOX_GainScalar"),    QString::number(static_cast<double>(m_voxGainScalar)));
    s.setValue(pfx + QLatin1String("VOX_HangTime"),    QString::number(m_voxHangTimeMs));

    // ── DEXP envelope properties (3M-3a-iii Task 7) — ALL persist ─────────
    s.setValue(pfx + QLatin1String("DEXP_Enabled"),
               m_dexpEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("DEXP_DetectorTauMs"), QString::number(m_dexpDetectorTauMs));
    s.setValue(pfx + QLatin1String("DEXP_AttackTimeMs"),  QString::number(m_dexpAttackTimeMs));
    s.setValue(pfx + QLatin1String("DEXP_ReleaseTimeMs"), QString::number(m_dexpReleaseTimeMs));

    // ── DEXP gate-ratio properties (3M-3a-iii Task 8) — both persist ──────
    s.setValue(pfx + QLatin1String("DEXP_ExpansionRatioDb"),  QString::number(m_dexpExpansionRatioDb));
    s.setValue(pfx + QLatin1String("DEXP_HysteresisRatioDb"), QString::number(m_dexpHysteresisRatioDb));

    // ── DEXP look-ahead properties (3M-3a-iii Task 9) — both persist ──────
    s.setValue(pfx + QLatin1String("DEXP_LookAheadEnabled"),
               m_dexpLookAheadEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("DEXP_LookAheadMs"), QString::number(m_dexpLookAheadMs));

    // ── DEXP side-channel filter properties (3M-3a-iii Task 10) — all persist ─
    s.setValue(pfx + QLatin1String("DEXP_LowCutHz"),  QString::number(m_dexpLowCutHz));
    s.setValue(pfx + QLatin1String("DEXP_HighCutHz"), QString::number(m_dexpHighCutHz));
    s.setValue(pfx + QLatin1String("DEXP_SideChannelFilterEnabled"),
               m_dexpSideChannelFilterEnabled ? QStringLiteral("True") : QStringLiteral("False"));

    // ── Anti-VOX properties ───────────────────────────────────────────────
    // 3M-3a-iv post-bench refactor (Option A): AntiVox_Source_VAX write dropped
    // alongside the antiVoxSourceVax property.
    s.setValue(pfx + QLatin1String("AntiVox_Gain"),    QString::number(m_antiVoxGainDb));
    s.setValue(pfx + QLatin1String("AntiVox_Tau_Ms"),  QString::number(m_antiVoxTauMs));
    s.setValue(pfx + QLatin1String("AntiVox_Enable"),
               m_antiVoxRun ? QStringLiteral("True") : QStringLiteral("False"));

    // ── MON properties (monEnabled excluded — safety) ─────────────────────
    s.setValue(pfx + QLatin1String("MonitorVolume"),    QString::number(static_cast<double>(m_monitorVolume)));

    // ── Mic source ────────────────────────────────────────────────────────
    {
        QString micSourceStr;
        switch (m_micSource) {
            case MicSource::Radio: micSourceStr = QStringLiteral("Radio"); break;
            case MicSource::Vax:   micSourceStr = QStringLiteral("Vax");   break;
            case MicSource::Pc:
            default:               micSourceStr = QStringLiteral("Pc");    break;
        }
        s.setValue(pfx + QLatin1String("Mic_Source"), micSourceStr);
    }

    // Mic_Source_PreVax mirrors Mic_Source persistence; tracked by setMicSource.
    {
        QString preVaxStr = (m_previousNonVaxMicSource == MicSource::Radio)
                                ? QStringLiteral("Radio")
                                : QStringLiteral("Pc");
        s.setValue(pfx + QLatin1String("Mic_Source_PreVax"), preVaxStr);
    }

    // ── Two-tone test properties (3M-1c B.2) ──────────────────────────────
    s.setValue(pfx + QLatin1String("TwoToneFreq1"),       QString::number(m_twoToneFreq1));
    s.setValue(pfx + QLatin1String("TwoToneFreq2"),       QString::number(m_twoToneFreq2));
    s.setValue(pfx + QLatin1String("TwoToneLevel"),       QString::number(m_twoToneLevel));
    s.setValue(pfx + QLatin1String("TwoTonePower"),       QString::number(m_twoTonePower));
    s.setValue(pfx + QLatin1String("TwoToneFreq2Delay"),  QString::number(m_twoToneFreq2Delay));
    s.setValue(pfx + QLatin1String("TwoToneInvert"),      m_twoToneInvert ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("TwoTonePulsed"),      m_twoTonePulsed ? QStringLiteral("True") : QStringLiteral("False"));

    // ── Two-tone drive-power source (3M-1c B.3) ─────────────────────────
    s.setValue(pfx + QLatin1String("TwoToneDrivePowerOrigin"),
               drivePowerSourceToString(m_twoToneDrivePowerSource));

    // ── TX EQ + Leveler + ALC properties (3M-3a-i Task C) ────────────────
    s.setValue(pfx + QLatin1String("TXEQEnabled"),
               m_txEqEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("TXEQPreamp"), QString::number(m_txEqPreamp));
    for (int i = 0; i < 10; ++i) {
        s.setValue(pfx + QStringLiteral("TXEQ%1").arg(i + 1),
                   QString::number(m_txEqBand[static_cast<std::size_t>(i)]));
        s.setValue(pfx + QStringLiteral("TxEqFreq%1").arg(i + 1),
                   QString::number(m_txEqFreq[static_cast<std::size_t>(i)]));
    }
    s.setValue(pfx + QLatin1String("Lev_On"),
               m_txLevelerOn ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("Lev_MaxGain"), QString::number(m_txLevelerMaxGain));
    s.setValue(pfx + QLatin1String("Lev_Decay"),   QString::number(m_txLevelerDecay));
    s.setValue(pfx + QLatin1String("ALC_MaximumGain"), QString::number(m_txAlcMaxGain));
    s.setValue(pfx + QLatin1String("ALC_Decay"),       QString::number(m_txAlcDecay));
    s.setValue(pfx + QLatin1String("eq/nc"),       QString::number(m_txEqNc));
    s.setValue(pfx + QLatin1String("eq/mp"),
               m_txEqMp ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("eq/ctfmode"),  QString::number(m_txEqCtfmode));
    s.setValue(pfx + QLatin1String("eq/wintype"),  QString::number(m_txEqWintype));

    // TX EQ parametric blob (3M-3a-ii follow-up Batch 6).
    s.setValue(pfx + QLatin1String("TXParaEQData"), m_txEqParaEqData);
    s.setValue(pfx + QLatin1String("EQUseLegacy"),
               m_txEqUseLegacy ? QStringLiteral("True") : QStringLiteral("False"));

    // ── Phase Rotator / CFC / CPDR / CESSB (3M-3a-ii Batch 2) ────────────
    s.setValue(pfx + QLatin1String("CFCPhaseRotatorEnabled"),
               m_phaseRotatorEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("CFCPhaseReverseEnabled"),
               m_phaseReverseEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("CFCPhaseRotatorFreq"),   QString::number(m_phaseRotatorFreqHz));
    s.setValue(pfx + QLatin1String("CFCPhaseRotatorStages"), QString::number(m_phaseRotatorStages));

    s.setValue(pfx + QLatin1String("CFCEnabled"),
               m_cfcEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("CFCPostEqEnabled"),
               m_cfcPostEqEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("CFCPreComp"),    QString::number(m_cfcPrecompDb));
    s.setValue(pfx + QLatin1String("CFCPostEqGain"), QString::number(m_cfcPostEqGainDb));

    for (int i = 0; i < 10; ++i) {
        s.setValue(pfx + QStringLiteral("CFCEqFreq%1").arg(i),
                   QString::number(m_cfcEqFreqHz[static_cast<std::size_t>(i)]));
        s.setValue(pfx + QStringLiteral("CFCPreComp%1").arg(i),
                   QString::number(m_cfcCompressionDb[static_cast<std::size_t>(i)]));
        s.setValue(pfx + QStringLiteral("CFCPostEqGain%1").arg(i),
                   QString::number(m_cfcPostEqBandGainDb[static_cast<std::size_t>(i)]));
    }
    s.setValue(pfx + QLatin1String("CFCParaEQData"), m_cfcParaEqData);

    // CPDR — cpdrOn outside profile namespace per Thetis console.cs:36430.
    s.setValue(pfx + QLatin1String("cpdr/on"),
               m_cpdrOn ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("CompanderLevel"), QString::number(m_cpdrLevelDb));
    s.setValue(pfx + QLatin1String("AM_Carrier_Level"), QString::number(m_amCarrierLevel));

    // CESSB.
    s.setValue(pfx + QLatin1String("CESSB_On"),
               m_cessbOn ? QStringLiteral("True") : QStringLiteral("False"));

    // ── TX filter bandwidth (Plan 4 D1) ───────────────────────────────────
    s.setValue(pfx + QLatin1String("FilterLow"),  QString::number(m_filterLow));
    s.setValue(pfx + QLatin1String("FilterHigh"), QString::number(m_filterHigh));

    // ── PA-cal hotfix scaffolding (#167 Phase 3A) ─────────────────────────
    // Per-band normal-mode power array — separate top-level scope.
    {
        const QString powerPfx =
            QStringLiteral("hardware/%1/powerByBand/").arg(mac);
        for (int i = 0; i < kBandCount; ++i) {
            const Band band = bandFromPerBandStateSlot(i);
            s.setValue(powerPfx + bandKeyName(band),
                       QString::number(
                           m_powerByBand[static_cast<std::size_t>(i)]));
        }
    }
    // R-R3-49: per-band slider limits and FM TX offsets
    // (console.cs:3101-3115 [v2.10.3.15] save).
    {
        const QString limitPfx =
            QStringLiteral("hardware/%1/limitPowerByBand/").arg(mac);
        const QString limitTunePfx =
            QStringLiteral("hardware/%1/limitTunePowerByBand/").arg(mac);
        const QString fmPfx =
            QStringLiteral("hardware/%1/fmTxOffsetByBandMhz/").arg(mac);
        for (int i = 0; i < kBandCount; ++i) {
            const Band band = bandFromPerBandStateSlot(i);
            const auto slot = static_cast<std::size_t>(i);
            s.setValue(limitPfx + bandKeyName(band),
                       QString::number(m_limitPowerByBand[slot]));
            s.setValue(limitTunePfx + bandKeyName(band),
                       QString::number(m_limitTunePowerByBand[slot]));
            s.setValue(fmPfx + bandKeyName(band),
                       QString::number(m_fmTxOffsetByBandMhz[slot], 'g', 17));
        }
    }
    // 3 ATT-on-TX safety properties (under tx/ namespace).
    s.setValue(pfx + QLatin1String("ForceATTwhenPSAoff"),
               m_forceAttwhenPSAoff ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("ForceATTwhenOutputPowerChangesWhenPSAon"),
               m_forceAttwhenPowerChangesWhenPSAon
                   ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(pfx + QLatin1String("ForceATTwhenOutputPowerChangesWhenPSAonAndDecreased"),
               m_forceAttwhenPowerChangesWhenPSAonAndDecreased
                   ? QStringLiteral("True") : QStringLiteral("False"));

    // m_lastPower: runtime-only sentinel — NOT persisted (matches Thetis
    // ephemeral _lastPower at console.cs:29292 [v2.10.3.13]).

    // ── PA-cal hotfix Phase 3C state ──────────────────────────────────────
    // m_twoToneActive: runtime-only mirror — NOT persisted.
    // m_tuneDrivePowerSource: under TuneDrivePowerOrigin.
    s.setValue(pfx + QLatin1String("TuneDrivePowerOrigin"),
               drivePowerSourceToString(m_tuneDrivePowerSource));
    // m_tunePower: under FixedTunePower.
    s.setValue(pfx + QLatin1String("FixedTunePower"),
               QString::number(m_tunePower));
}

// ── Anti-VOX properties (3M-1b C.4) ─────────────────────────────────────────
//
// Porting from Thetis setup.designer.cs:44699-44728 [v2.10.3.13] (udAntiVoxGain):
//   Minimum = decimal{60,0,0,-2147483648} = -60; Maximum = decimal{60,0,0,0} = 60.
// Porting from Thetis setup.cs:18986-18989 [v2.10.3.13] (udAntiVoxGain_ValueChanged):
//   cmaster.SetAntiVOXGain(0, Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0));
//
// WDSP wiring (SetAntiVOXGain) deferred to Phase H.3.
// AppSettings persistence deferred to Phase L.2.
//
// 3M-3a-iv post-bench refactor (Option A): setAntiVoxSourceVax(bool) and the
// antiVoxSourceVaxChanged signal have been removed.  Thetis chkAntiVoxSource
// (RX vs VAC at audio.cs:446-454 [v2.10.3.13]) does not map to NereusSDR's
// architecture: VAX is a digital-mode app bus with no mic-feedback path, so
// the audio output device is the only valid anti-VOX cancellation reference.
// See commit message and DexpVoxPage info-row for the architectural rationale.

void TransmitModel::setAntiVoxGainDb(int dB)
{
    // Clamp to Thetis udAntiVoxGain range per
    // setup.designer.cs:44708-44717 [v2.10.3.13]:
    //   Minimum = decimal{60,0,0,-2147483648} = -60
    //   Maximum = decimal{60,0,0,0}           = +60
    const int clamped = std::clamp(dB, kAntiVoxGainDbMin, kAntiVoxGainDbMax);
    if (clamped == m_antiVoxGainDb) { return; }  // idempotent guard
    // Porting from Thetis setup.cs:18986-18989 [v2.10.3.13]:
    //   cmaster.SetAntiVOXGain(0, Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0));
    // WDSP SetAntiVOXGain call deferred to Phase H.3.
    m_antiVoxGainDb = clamped;
    persistOne(QStringLiteral("AntiVox_Gain"), QString::number(m_antiVoxGainDb));  // L.2 auto-persist
    emit antiVoxGainDbChanged(clamped);
}

// ─────────────────────────────────────────────────────────────────────────────
// setAntiVoxTauMs() — Phase 3M-3a-iv Task 8.
//
// Porting from Thetis setup.designer.cs:44661-44688 [v2.10.3.13]:
//   udAntiVoxTau.Minimum   = decimal{1,0,0,0}   = 1
//   udAntiVoxTau.Maximum   = decimal{500,0,0,0} = 500
//   udAntiVoxTau.Increment = decimal{1,0,0,0}   = 1
//   udAntiVoxTau.Value     = decimal{20,0,0,0}  = 20
//
// This is the model-side property; RadioModel wires
// antiVoxTauMsChanged → MoxController::setAntiVoxTau in Task 9.
// MoxController converts to seconds and forwards to TxWorkerThread which
// calls the WDSP DEXP detector setter (RXA path; the radio's anti-VOX feed).
// ─────────────────────────────────────────────────────────────────────────────
void TransmitModel::setAntiVoxTauMs(int ms)
{
    const int clamped = std::clamp(ms, kAntiVoxTauMsMin, kAntiVoxTauMsMax);
    if (clamped == m_antiVoxTauMs) { return; }  // idempotent guard
    m_antiVoxTauMs = clamped;
    persistOne(QStringLiteral("AntiVox_Tau_Ms"), QString::number(m_antiVoxTauMs));  // auto-persist
    emit antiVoxTauMsChanged(clamped);
}

// ─────────────────────────────────────────────────────────────────────────────
// setAntiVoxRun() — 3M-3a-iv scope-expansion.
//
// Porting from Thetis setup.cs:18980-18984 [v2.10.3.13]:
//   private void chkAntiVoxEnable_CheckedChanged(object sender, EventArgs e)
//   {
//       if (initializing) return;
//       cmaster.SetAntiVOXRun(0, chkAntiVoxEnable.Checked);
//   }
//
// This is the model-side property; RadioModel wires
// antiVoxRunChanged → MoxController::setAntiVoxRun (3M-3a-iv scope-expansion),
// MoxController emits antiVoxRunRequested, TxWorkerThread::setAntiVoxRun
// forwards to TxChannel::setAntiVoxRun (the WDSP wrapper that calls
// SetAntiVOXRun) AND flips the worker-local m_antiVoxRun atomic gate.
//
// Auto-persists via persistOne; load handled in loadFromSettings(mac).
// ─────────────────────────────────────────────────────────────────────────────
void TransmitModel::setAntiVoxRun(bool run)
{
    if (run == m_antiVoxRun) { return; }  // idempotent guard
    m_antiVoxRun = run;
    persistOne(QStringLiteral("AntiVox_Enable"),
               run ? QStringLiteral("True") : QStringLiteral("False"));  // auto-persist
    emit antiVoxRunChanged(run);
}

// ── PA settings bypass setter (D4: ANAN-G2E port) ────────────────────────────
//
// From Thetis setup.cs:19921 [v2.10.3.15] //N1GP G2E added:
//   chkBypassANANPASettings.Visible = true;  (in ANAN_G2E case)
// Thetis has no CheckedChanged handler in v2.10.3.15 — the checkbox is
// UI-only, its state serialised generically.  NereusSDR persists it explicitly.
// ─────────────────────────────────────────────────────────────────────────────
void TransmitModel::setPaSettingsBypass(bool bypass)
{
    if (bypass == m_paSettingsBypass) { return; }  // idempotent guard
    m_paSettingsBypass = bypass;
    persistOne(QStringLiteral("PaSettingsBypass"),
               bypass ? QStringLiteral("True") : QStringLiteral("False"));  // auto-persist
    emit paSettingsBypassChanged(bypass);
}

// ── MON properties (3M-1b C.5) ───────────────────────────────────────────────
//
// Porting from Thetis audio.cs:406 [v2.10.3.13]:
//   private bool mon = false;
// Porting from Thetis audio.cs:417 [v2.10.3.13]:
//   cmaster.SetAAudioMixVol((void*)0, 0, WDSP.id(1, 0), 0.5);
//   The 0.5 literal is a fixed mix coefficient that NereusSDR repurposes as
//   the user-volume default for monitorVolume.
//
// AudioEngine integration (setTxMonitorEnabled / setTxMonitorVolume) deferred
// to Phase E.2-E.3.  AppSettings persistence for monitorVolume deferred to
// Phase L.2.  monEnabled intentionally NOT persisted — safety (plan §0 row 9).

void TransmitModel::setMonEnabled(bool on)
{
    if (on == m_monEnabled) { return; }  // idempotent guard
    // Porting from Thetis audio.cs:406 [v2.10.3.13]:
    //   private bool mon = false;  (default off)
    // AudioEngine integration arrives in Phase E.2.
    m_monEnabled = on;
    emit monEnabledChanged(on);
}

void TransmitModel::setMonitorVolume(float volume)
{
    // Clamp to normalized scalar range [0.0f, 1.0f].
    const float clamped = std::clamp(volume, kMonitorVolumeMin, kMonitorVolumeMax);
    // Use qFuzzyIsNull(diff) for the zero-boundary-safe idempotent guard.
    // qFuzzyCompare(0.0f, x) is unreliable when one operand is exactly zero
    // (Qt docs: both values must be non-zero).  Using diff + qFuzzyIsNull
    // avoids that pitfall (C.3 fix-up pattern).
    if (qFuzzyIsNull(clamped - m_monitorVolume)) { return; }
    // Porting from Thetis audio.cs:417 [v2.10.3.13]:
    //   cmaster.SetAAudioMixVol((void*)0, 0, WDSP.id(1, 0), 0.5);
    // SetAAudioMixVol WDSP call deferred to Phase E.3.
    m_monitorVolume = clamped;
    persistOne(QStringLiteral("MonitorVolume"), QString::number(static_cast<double>(m_monitorVolume)));  // L.2 auto-persist
    emit monitorVolumeChanged(clamped);
}

// ── VOX properties (3M-1b C.3) ───────────────────────────────────────────────
//
// Porting from Thetis audio.cs:167-192 [v2.10.3.13] (VOXEnabled setter):
//   private static bool vox_enabled = false;
//   public static bool VOXEnabled { get { return vox_enabled; } set { vox_enabled = value; ... } }
// Porting from Thetis audio.cs:194-202 [v2.10.3.13] (VOXGain):
//   private static float vox_gain = 1.0f;
//   public static float VOXGain { get { return vox_gain; } set { vox_gain = value; } }
// VOXHangTime from Thetis console.cs:14707-14716 [v2.10.3.13] /
//   setup.cs:4865-4876 [v2.10.3.13] (maps to udDEXPHold).
// voxThresholdDb range from console.Designer.cs:6018-6019 [v2.10.3.13]:
//   ptbVOX.Maximum=0, ptbVOX.Minimum=-80.
// udDEXPHold range from setup.designer.cs:45005-45013 [v2.10.3.13]:
//   Maximum=2000, Minimum=1 (ms).
//
// WDSP wiring (SetDEXPRunVox, SetDEXPAttackThreshold, SetDEXPHoldTime) deferred
// to Phase D and Phase H.  AppSettings persistence deferred to Phase L.2.
// voxEnabled intentionally NOT persisted — safety: VOX always loads OFF.

void TransmitModel::setVoxEnabled(bool on)
{
    if (on == m_voxEnabled) { return; }  // idempotent guard
    // Porting from Thetis audio.cs:167-192 [v2.10.3.13]:
    //   vox_enabled = value; cmaster.CMSetTXAVoxRun(0); ...
    // Phase H Task H.1 wires the mode-gate; model just stores + signals.
    m_voxEnabled = on;
    emit voxEnabledChanged(on);
}

void TransmitModel::setVoxThresholdDb(int dB)
{
    // Clamp to Thetis ptbVOX range per console.Designer.cs:6018-6019 [v2.10.3.13]:
    //   ptbVOX.Maximum = 0, ptbVOX.Minimum = -80
    const int clamped = std::clamp(dB, kVoxThresholdDbMin, kVoxThresholdDbMax);
    if (clamped == m_voxThresholdDb) { return; }  // idempotent guard
    // Porting from Thetis console.cs:12850-12858 [v2.10.3.13] (ptbVOX.Value setter).
    // WDSP threshold application (CMSetTXAVoxThresh mic-boost-aware scaling)
    // deferred to Phase H Task H.2.
    m_voxThresholdDb = clamped;
    persistOne(QStringLiteral("Dexp_Threshold"), QString::number(m_voxThresholdDb));  // L.2 auto-persist
    emit voxThresholdDbChanged(clamped);
}

void TransmitModel::setVoxGainScalar(float scalar)
{
    // NereusSDR sane guard [0.0f, 100.0f]; Thetis Audio.VOXGain has no explicit
    // clamp (audio.cs:194-202 [v2.10.3.13]).  0.0f disables mic-boost scaling;
    // 100.0f is an extreme upper bound that avoids silent float overflow.
    const float clamped = std::clamp(scalar, kVoxGainScalarMin, kVoxGainScalarMax);
    if (qFuzzyCompare(clamped, m_voxGainScalar)) { return; }  // idempotent guard
    // Porting from Thetis audio.cs:194-202 [v2.10.3.13]:
    //   vox_gain = value;
    // Mic-boost-aware threshold scaling wired in Phase H Task H.2.
    m_voxGainScalar = clamped;
    persistOne(QStringLiteral("VOX_GainScalar"), QString::number(static_cast<double>(m_voxGainScalar)));  // L.2 auto-persist
    emit voxGainScalarChanged(clamped);
}

void TransmitModel::setVoxHangTimeMs(int ms)
{
    // Clamp to Thetis udDEXPHold range per setup.designer.cs:45005-45013 [v2.10.3.13]:
    //   udDEXPHold.Maximum = 2000, udDEXPHold.Minimum = 1  (units: ms)
    const int clamped = std::clamp(ms, kVoxHangTimeMsMin, kVoxHangTimeMsMax);
    if (clamped == m_voxHangTimeMs) { return; }  // idempotent guard
    // Porting from Thetis console.cs:14707-14716 [v2.10.3.13]:
    //   vox_hang_time = value; if (!IsSetupFormNull) SetupForm.VOXHangTime = (int)value;
    // WDSP SetDEXPHoldTime call deferred to Phase D / Phase H.
    m_voxHangTimeMs = clamped;
    persistOne(QStringLiteral("VOX_HangTime"), QString::number(m_voxHangTimeMs));  // L.2 auto-persist
    emit voxHangTimeMsChanged(clamped);
}

// ── DEXP envelope properties (3M-3a-iii Task 7) ────────────────────────────
//
// Downward expander envelope controls.  Bound to Setup -> Audio -> VOX/DEXP
// (grpDEXPVOX on tpDSPVOXDE) per Thetis setup.Designer.cs:44820+ [v2.10.3.13].
//
// Defaults:
//   chkDEXPEnable: WinForms default false (no Checked= setter at line 45140-45151)
//   udDEXPDetTau.Value=20    (line 45093)
//   udDEXPAttack.Value=2     (line 45050)
//   udDEXPRelease.Value=100  (line 44990)
//
// Ranges:
//   udDEXPDetTau:  Min=1,    Max=100   (line 45078-45087)
//   udDEXPAttack:  Min=2,    Max=100   (line 45035-45044)
//   udDEXPRelease: Min=2,    Max=1000  (line 44975-44984)
//
// Persistence: ALL four properties persist.  Unlike voxEnabled (which is held
// off at startup for PTT safety), dexpEnabled does NOT key the radio — the
// downward expander only gates already-keyed audio.  No safety carve-out.
//
// WDSP wiring lives in TxChannel (Tasks 1-2): setDexpRun, setDexpDetectorTau,
// setDexpAttackTime, setDexpReleaseTime.  Setup-page binding lands in Task 14.

void TransmitModel::setDexpEnabled(bool on)
{
    if (on == m_dexpEnabled) { return; }  // idempotent guard
    m_dexpEnabled = on;
    persistOne(QStringLiteral("DEXP_Enabled"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit dexpEnabledChanged(on);
}

void TransmitModel::setDexpDetectorTauMs(double ms)
{
    // Clamp to udDEXPDetTau range per setup.Designer.cs:45078-45087 [v2.10.3.13]:
    //   udDEXPDetTau.Maximum = 100, udDEXPDetTau.Minimum = 1  (units: ms)
    const double clamped = std::clamp(ms, kDexpDetectorTauMsMin, kDexpDetectorTauMsMax);
    if (qFuzzyCompare(clamped, m_dexpDetectorTauMs)) { return; }  // idempotent guard
    m_dexpDetectorTauMs = clamped;
    persistOne(QStringLiteral("DEXP_DetectorTauMs"), QString::number(clamped));
    emit dexpDetectorTauMsChanged(clamped);
}

void TransmitModel::setDexpAttackTimeMs(double ms)
{
    // Clamp to udDEXPAttack range per setup.Designer.cs:45035-45044 [v2.10.3.13]:
    //   udDEXPAttack.Maximum = 100, udDEXPAttack.Minimum = 2  (units: ms)
    const double clamped = std::clamp(ms, kDexpAttackTimeMsMin, kDexpAttackTimeMsMax);
    if (qFuzzyCompare(clamped, m_dexpAttackTimeMs)) { return; }  // idempotent guard
    m_dexpAttackTimeMs = clamped;
    persistOne(QStringLiteral("DEXP_AttackTimeMs"), QString::number(clamped));
    emit dexpAttackTimeMsChanged(clamped);
}

void TransmitModel::setDexpReleaseTimeMs(double ms)
{
    // Clamp to udDEXPRelease range per setup.Designer.cs:44975-44984 [v2.10.3.13]:
    //   udDEXPRelease.Maximum = 1000, udDEXPRelease.Minimum = 2  (units: ms)
    const double clamped = std::clamp(ms, kDexpReleaseTimeMsMin, kDexpReleaseTimeMsMax);
    if (qFuzzyCompare(clamped, m_dexpReleaseTimeMs)) { return; }  // idempotent guard
    m_dexpReleaseTimeMs = clamped;
    persistOne(QStringLiteral("DEXP_ReleaseTimeMs"), QString::number(clamped));
    emit dexpReleaseTimeMsChanged(clamped);
}

// ── DEXP gate-ratio properties (3M-3a-iii Task 8) ──────────────────────────
//
// Downward-expander gate ratios.  Bound to grpDEXPVOX in Setup -> Audio ->
// VOX/DEXP per Thetis setup.Designer.cs:44820+ [v2.10.3.13].
//
// Defaults:
//   udDEXPExpansionRatio.Value=10   (line 44900-44904)
//   udDEXPHysteresisRatio.Value=20 with DecimalPlaces=1, scale=65536
//                                  -- displayed as 2.0 (line 44869-44873)
//
// Ranges:
//   udDEXPExpansionRatio:  Min=0, Max=30  (line 44885-44894)
//   udDEXPHysteresisRatio: Min=0, Max=10  (line 44854-44863)
//
// The TxChannel wrapper for hysteresis applies a NEGATIVE Math.Pow exponent
// internally (per Batch B finding); the model layer just stores the dB value.
// Wrapper conversion lives in TxChannel setDexpHysteresisRatio (Task 3).
//
// Both persist.

void TransmitModel::setDexpExpansionRatioDb(double dB)
{
    // Clamp to udDEXPExpansionRatio range per setup.Designer.cs:44885-44894 [v2.10.3.13]:
    //   udDEXPExpansionRatio.Maximum = 30, udDEXPExpansionRatio.Minimum = 0
    const double clamped = std::clamp(dB, kDexpExpansionRatioDbMin, kDexpExpansionRatioDbMax);
    if (qFuzzyCompare(clamped, m_dexpExpansionRatioDb)) { return; }  // idempotent guard
    m_dexpExpansionRatioDb = clamped;
    persistOne(QStringLiteral("DEXP_ExpansionRatioDb"), QString::number(clamped));
    emit dexpExpansionRatioDbChanged(clamped);
}

void TransmitModel::setDexpHysteresisRatioDb(double dB)
{
    // Clamp to udDEXPHysteresisRatio range per setup.Designer.cs:44854-44863 [v2.10.3.13]:
    //   udDEXPHysteresisRatio.Maximum = 10, udDEXPHysteresisRatio.Minimum = 0
    const double clamped = std::clamp(dB, kDexpHysteresisRatioDbMin, kDexpHysteresisRatioDbMax);
    if (qFuzzyCompare(clamped, m_dexpHysteresisRatioDb)) { return; }  // idempotent guard
    m_dexpHysteresisRatioDb = clamped;
    persistOne(QStringLiteral("DEXP_HysteresisRatioDb"), QString::number(clamped));
    emit dexpHysteresisRatioDbChanged(clamped);
}

// ── DEXP look-ahead properties (3M-3a-iii Task 9) ──────────────────────────
//
// Audio look-ahead controls.  Bound to grpDEXPLookAhead in Setup -> Audio ->
// VOX/DEXP per Thetis setup.Designer.cs:44755+ [v2.10.3.13].
//
// Defaults:
//   chkDEXPLookAheadEnable.Checked=true (line 44808)
//                  -- the only DEXP boolean defaulting true
//   udDEXPLookAhead.Value=60            (line 44788)
//
// Range:
//   udDEXPLookAhead: Min=10, Max=999  (line 44773-44782; units: ms)
//
// Both persist.

void TransmitModel::setDexpLookAheadEnabled(bool on)
{
    if (on == m_dexpLookAheadEnabled) { return; }  // idempotent guard
    m_dexpLookAheadEnabled = on;
    persistOne(QStringLiteral("DEXP_LookAheadEnabled"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit dexpLookAheadEnabledChanged(on);
}

void TransmitModel::setDexpLookAheadMs(double ms)
{
    // Clamp to udDEXPLookAhead range per setup.Designer.cs:44773-44782 [v2.10.3.13]:
    //   udDEXPLookAhead.Maximum = 999, udDEXPLookAhead.Minimum = 10  (units: ms)
    const double clamped = std::clamp(ms, kDexpLookAheadMsMin, kDexpLookAheadMsMax);
    if (qFuzzyCompare(clamped, m_dexpLookAheadMs)) { return; }  // idempotent guard
    m_dexpLookAheadMs = clamped;
    persistOne(QStringLiteral("DEXP_LookAheadMs"), QString::number(clamped));
    emit dexpLookAheadMsChanged(clamped);
}

// ── DEXP side-channel filter properties (3M-3a-iii Task 10) ────────────────
//
// Side-channel HP/LP filter trio used by the DEXP detector to gate which
// audio frequencies trigger VOX/DEXP.  Bound to grpSCF in Setup -> Audio ->
// VOX/DEXP per Thetis setup.Designer.cs:45153+ [v2.10.3.13].
//
// Plan scope correction (2026-05-03): originally these were planned as
// model-only / no-UI properties, but a source-first re-read by the Batch B
// agent surfaced grpSCF on tpDSPVOXDE -- so they DO get UI binding
// (lands in Task 14, the DexpVoxPage Setup-page work).  Defaults below
// therefore match the Thetis Designer values verbatim.
//
// Defaults:
//   udSCFLowCut.Value=500     (line 45240)
//   udSCFHighCut.Value=1500   (line 45210)
//   chkSCFEnable.Checked=true (line 45250)
//
// Range:
//   udSCFLowCut + udSCFHighCut both: Min=100, Max=10000 (units: Hz)
//   (lines 45195-45234)
// Range matches Task 4 wrapper clamps in TxChannel::setDexpLowCut/HighCut.
//
// All three persist.

void TransmitModel::setDexpLowCutHz(double hz)
{
    // Clamp to udSCFLowCut range per setup.Designer.cs:45225-45234 [v2.10.3.13]:
    //   udSCFLowCut.Maximum = 10000, udSCFLowCut.Minimum = 100  (units: Hz)
    const double clamped = std::clamp(hz, kDexpFilterCutHzMin, kDexpFilterCutHzMax);
    if (qFuzzyCompare(clamped, m_dexpLowCutHz)) { return; }  // idempotent guard
    m_dexpLowCutHz = clamped;
    persistOne(QStringLiteral("DEXP_LowCutHz"), QString::number(clamped));
    emit dexpLowCutHzChanged(clamped);
}

void TransmitModel::setDexpHighCutHz(double hz)
{
    // Clamp to udSCFHighCut range per setup.Designer.cs:45195-45204 [v2.10.3.13]:
    //   udSCFHighCut.Maximum = 10000, udSCFHighCut.Minimum = 100  (units: Hz)
    const double clamped = std::clamp(hz, kDexpFilterCutHzMin, kDexpFilterCutHzMax);
    if (qFuzzyCompare(clamped, m_dexpHighCutHz)) { return; }  // idempotent guard
    m_dexpHighCutHz = clamped;
    persistOne(QStringLiteral("DEXP_HighCutHz"), QString::number(clamped));
    emit dexpHighCutHzChanged(clamped);
}

void TransmitModel::setDexpSideChannelFilterEnabled(bool on)
{
    if (on == m_dexpSideChannelFilterEnabled) { return; }  // idempotent guard
    m_dexpSideChannelFilterEnabled = on;
    persistOne(QStringLiteral("DEXP_SideChannelFilterEnabled"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit dexpSideChannelFilterEnabledChanged(on);
}

// ── Two-tone test properties (3M-1c B.2) ────────────────────────────────────
//
// Per-MAC AppSettings persistence with Thetis column names per design spec
// §4.4.  WDSP setters (TXPostGenMode / TXPostGenTTFreq1/2 / TXPostGenTTMag1/2
// + pulse-profile setters) arrive in Phase E.  The two-tone activation
// handler (mode-aware invert, power-source enum, MOX engage) arrives in
// Phase I.  Setters here just store + signal + auto-persist.

void TransmitModel::setTwoToneFreq1(int hz)
{
    // Clamp to Thetis Designer range per setup.Designer.cs:62117-62126 [v2.10.3.13].
    const int clamped = std::clamp(hz, kTwoToneFreq1HzMin, kTwoToneFreq1HzMax);
    if (clamped == m_twoToneFreq1) { return; }
    m_twoToneFreq1 = clamped;
    persistOne(QStringLiteral("TwoToneFreq1"), QString::number(m_twoToneFreq1));
    emit twoToneFreq1Changed(clamped);
}

void TransmitModel::setTwoToneFreq2(int hz)
{
    // Clamp to Thetis Designer range per setup.Designer.cs:62035-62044 [v2.10.3.13].
    const int clamped = std::clamp(hz, kTwoToneFreq2HzMin, kTwoToneFreq2HzMax);
    if (clamped == m_twoToneFreq2) { return; }
    m_twoToneFreq2 = clamped;
    persistOne(QStringLiteral("TwoToneFreq2"), QString::number(m_twoToneFreq2));
    emit twoToneFreq2Changed(clamped);
}

void TransmitModel::setTwoToneFrequencies(int freq1Hz, int freq2Hz)
{
    const int first = std::clamp(freq1Hz, kTwoToneFreq1HzMin, kTwoToneFreq1HzMax);
    const int second = std::clamp(freq2Hz, kTwoToneFreq2HzMin, kTwoToneFreq2HzMax);
    const bool firstChanged = first != m_twoToneFreq1;
    const bool secondChanged = second != m_twoToneFreq2;
    if (!firstChanged && !secondChanged) { return; }
    m_twoToneFreq1 = first;
    m_twoToneFreq2 = second;
    if (firstChanged) { persistOne(QStringLiteral("TwoToneFreq1"), QString::number(first)); }
    if (secondChanged) { persistOne(QStringLiteral("TwoToneFreq2"), QString::number(second)); }
    // The combined signal pushes both DSP parameters first. Observers of
    // either ordinary property signal then see the complete new pair.
    emit twoToneFrequenciesChanged(first, second);
    if (firstChanged) { emit twoToneFreq1Changed(first); }
    if (secondChanged) { emit twoToneFreq2Changed(second); }
}

void TransmitModel::setTwoToneLevel(double db)
{
    // Clamp to Thetis Designer range per setup.Designer.cs:61994-62003 [v2.10.3.13].
    const double clamped = std::clamp(db, kTwoToneLevelDbMin, kTwoToneLevelDbMax);
    // qFuzzyIsNull(diff) zero-boundary-safe idempotent guard (matches MON pattern).
    if (qFuzzyIsNull(clamped - m_twoToneLevel)) { return; }
    m_twoToneLevel = clamped;
    persistOne(QStringLiteral("TwoToneLevel"), QString::number(m_twoToneLevel));
    emit twoToneLevelChanged(clamped);
}

void TransmitModel::setTwoTonePower(int pct)
{
    // Clamp to Thetis Designer range per setup.Designer.cs:62064-62073 [v2.10.3.13].
    const int clamped = std::clamp(pct, kTwoTonePowerMin, kTwoTonePowerMax);
    if (clamped == m_twoTonePower) { return; }
    m_twoTonePower = clamped;
    persistOne(QStringLiteral("TwoTonePower"), QString::number(m_twoTonePower));
    emit twoTonePowerChanged(clamped);
}

void TransmitModel::setTwoToneFreq2Delay(int ms)
{
    // Clamp to Thetis Designer range per setup.Designer.cs:61928-61937 [v2.10.3.13].
    const int clamped = std::clamp(ms, kTwoToneFreq2DelayMsMin, kTwoToneFreq2DelayMsMax);
    if (clamped == m_twoToneFreq2Delay) { return; }
    m_twoToneFreq2Delay = clamped;
    persistOne(QStringLiteral("TwoToneFreq2Delay"), QString::number(m_twoToneFreq2Delay));
    emit twoToneFreq2DelayChanged(clamped);
}

void TransmitModel::setTwoToneInvert(bool on)
{
    if (on == m_twoToneInvert) { return; }
    m_twoToneInvert = on;
    persistOne(QStringLiteral("TwoToneInvert"), on ? QStringLiteral("True") : QStringLiteral("False"));
    emit twoToneInvertChanged(on);
}

void TransmitModel::setTwoTonePulsed(bool on)
{
    if (on == m_twoTonePulsed) { return; }
    m_twoTonePulsed = on;
    persistOne(QStringLiteral("TwoTonePulsed"), on ? QStringLiteral("True") : QStringLiteral("False"));
    emit twoTonePulsedChanged(on);
}

// ── Two-tone drive-power source (3M-1c B.3) ────────────────────────────────
//
// Porting from Thetis console.cs:46576-46597 [v2.10.3.13] (TwoToneDrivePowerOrigin
// property — Thetis console-side; NereusSDR puts it on TransmitModel).  Phase I
// (two-tone activation handler) consumes this to decide power-source behaviour
// per setup.cs:11111-11119.  AppSettings key: "TwoToneDrivePowerOrigin".

void TransmitModel::setTwoToneDrivePowerSource(DrivePowerSource source)
{
    if (source == m_twoToneDrivePowerSource) { return; }
    m_twoToneDrivePowerSource = source;
    persistOne(QStringLiteral("TwoToneDrivePowerOrigin"),
               drivePowerSourceToString(source));
    emit twoToneDrivePowerSourceChanged(source);
}

// ── Mic source (3M-1b I.1) ────────────────────────────────────────────────────
//
// NereusSDR-native property: Thetis bakes mic-source selection into audio.cs
// directly rather than a strategy enum.  This property drives
// AudioTxInputPage (Setup → Audio → TX Input) and will be consumed by
// CompositeTxMicRouter::setActiveSource() in Phase F.3.
//
// AppSettings persistence (per-MAC) deferred to Phase L.2.

void TransmitModel::setMicSource(MicSource source)
{
    // L.3: HL2 force-Pc lock guard.
    // When m_micSourceLocked is true (hasMicJack == false), MicSource::Radio
    // is silently coerced to MicSource::Pc.  HL2 has no radio-side mic jack;
    // the UI side (AudioTxInputPage) already disables the Radio Mic radio button
    // when !hasMicJack.  This ensures the model state is consistent even if
    // any code path calls setMicSource(Radio) while the lock is active.
    if (source == MicSource::Radio && m_micSourceLocked) {
        source = MicSource::Pc;
    }

    if (source == m_micSource) { return; }  // idempotent guard
    m_micSource = source;

    // Capture every non-Vax write as the "previous" source so toggling
    // VAX off restores the user's most recent explicit choice. Updates
    // both the per-MAC and preconnect persistence keys to mirror the
    // Mic_Source two-key pattern.
    if (source != MicSource::Vax && source != m_previousNonVaxMicSource) {
        m_previousNonVaxMicSource = source;
        QString preVaxStr = (source == MicSource::Radio)
                                ? QStringLiteral("Radio")
                                : QStringLiteral("Pc");
        if (m_persistMac.isEmpty()) {
            AppSettings::instance().setValue(
                QStringLiteral("tx/preconnect/Mic_Source_PreVax"), preVaxStr);
        } else {
            persistOne(QStringLiteral("Mic_Source_PreVax"), preVaxStr);
        }
    }

    QString persistStr;
    switch (source) {
        case MicSource::Radio: persistStr = QStringLiteral("Radio"); break;
        case MicSource::Vax:   persistStr = QStringLiteral("Vax");   break;
        case MicSource::Pc:
        default:               persistStr = QStringLiteral("Pc");    break;
    }
    if (m_persistMac.isEmpty()) {
        // Pre-connect fallback (eager-borg-d64bed, 2026-05-06). When the
        // user clicks the radio button in Setup -> Audio -> TX Input
        // before connecting to a radio, persistOne early-returns (no MAC
        // bound yet) so the choice would normally be lost on app restart.
        // Write to a global "tx/preconnect/Mic_Source" key instead;
        // loadFromSettings reads it as a fallback when the per-MAC key is
        // absent so the choice carries forward to whatever radio they
        // connect next. Per-MAC values always take precedence over the
        // preconnect key once written.
        AppSettings::instance().setValue(
            QStringLiteral("tx/preconnect/Mic_Source"), persistStr);
    } else {
        persistOne(QStringLiteral("Mic_Source"), persistStr);  // L.2 auto-persist
    }
    emit micSourceChanged(source);
}

void TransmitModel::toggleVaxSource(bool on)
{
    if (on) {
        setMicSource(MicSource::Vax);
    } else {
        setMicSource(m_previousNonVaxMicSource);
    }
}

// ── Mic source lock guard (3M-1b L.3) ────────────────────────────────────────
//
// NereusSDR-native.  RadioModel::connectToRadio() calls
//   setMicSourceLocked(!boardCapabilities().hasMicJack)
// after loadFromSettings() so the lock is active for the lifetime of the HL2
// connection.  teardownConnection() calls setMicSourceLocked(false) to release
// the lock before a potential reconnect to a different (non-HL2) radio.
//
// The lock itself is NOT persisted — it is a runtime capability constraint
// derived from hardware, not a user preference.

void TransmitModel::setMicSourceLocked(bool lock)
{
    m_micSourceLocked = lock;

    // If we are engaging the lock while micSource is Radio, coerce to Pc now.
    // This handles the case where loadFromSettings already ran and set Radio
    // (from a previous non-HL2 connection's stored value), and the lock is
    // being engaged afterwards by RadioModel.
    if (lock && m_micSource == MicSource::Radio) {
        setMicSource(MicSource::Pc);  // will clamp through the lock guard above
    }
}

// ── PC Mic session state (3M-1b I.2) ─────────────────────────────────────────
//
// NereusSDR-native transient session-state properties for the PC Mic
// configuration group (Setup → Audio → TX Input → PC Mic group box).
//
// All three setters are idempotent (no signal emitted on unchanged value).
// R-R3-36 (2026-09-22): they are projections of the AudioEngine TX input
// config (audio/TxInput). RadioModel mirrors that config into them and
// forwards a setter's change signal to AudioEngine::setTxInputConfig; this
// class itself persists nothing for them.

void TransmitModel::setPcMicHostApiIndex(int index)
{
    if (index == m_pcMicHostApiIndex) { return; }  // idempotent guard
    m_pcMicHostApiIndex = index;
    emit pcMicHostApiIndexChanged(index);
}

void TransmitModel::setPcMicDeviceName(const QString& name)
{
    if (name == m_pcMicDeviceName) { return; }  // idempotent guard
    m_pcMicDeviceName = name;
    emit pcMicDeviceNameChanged(name);
}

void TransmitModel::setPcMicBufferSamples(int samples)
{
    if (samples == m_pcMicBufferSamples) { return; }  // idempotent guard
    m_pcMicBufferSamples = samples;
    emit pcMicBufferSamplesChanged(samples);
}

// ── TX EQ + Leveler + ALC properties (3M-3a-i Task C) ─────────────────────
//
// Defaults sourced from Thetis database.cs:4552-4594 [v2.10.3.13] (TXProfile
// schema) and WDSP TXA.c:111-128 [v2.10.3.13] (create_eqp G[]/F[] vectors).
//
// All setters are idempotent (skip emit + persist when value unchanged) and
// clamp to the appropriate Thetis Designer range.  Per-MAC AppSettings keys
// match Thetis TXProfile column names exactly:
//   TXEQEnabled / TXEQPreamp / TXEQ1..10 / TxEqFreq1..10 /
//   Lev_On / Lev_MaxGain / Lev_Decay / ALC_MaximumGain / ALC_Decay
// EQ globals (eq/nc, eq/mp, eq/ctfmode, eq/wintype) sit alongside the
// profile keys under hardware/<mac>/tx/, NOT under the profile namespace —
// they are radio-wide DSP settings, not part of the bundled profile.

int TransmitModel::txEqBand(int index) const noexcept
{
    if (index < 0 || index >= 10) { return 0; }
    return m_txEqBand[static_cast<std::size_t>(index)];
}

int TransmitModel::txEqFreq(int index) const noexcept
{
    if (index < 0 || index >= 10) { return 0; }
    return m_txEqFreq[static_cast<std::size_t>(index)];
}

void TransmitModel::setTxEqEnabled(bool on)
{
    if (on == m_txEqEnabled) { return; }
    // From Thetis database.cs:4553 [v2.10.3.13]: dr["TXEQEnabled"] = false;
    m_txEqEnabled = on;
    persistOne(QStringLiteral("TXEQEnabled"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit txEqEnabledChanged(on);
}

void TransmitModel::setTxEqPreamp(int dB)
{
    // NereusSDR clamp [-12, 15] dB (Thetis EQ preamp slider precedent).
    const int clamped = std::clamp(dB, kTxEqPreampDbMin, kTxEqPreampDbMax);
    if (clamped == m_txEqPreamp) { return; }
    m_txEqPreamp = clamped;
    persistOne(QStringLiteral("TXEQPreamp"), QString::number(m_txEqPreamp));
    emit txEqPreampChanged(clamped);
    publishTxEqProfile();
}

void TransmitModel::setTxEqBand(int index, int dB)
{
    if (index < 0 || index >= 10) { return; }
    const int clamped = std::clamp(dB, kTxEqBandDbMin, kTxEqBandDbMax);
    if (clamped == m_txEqBand[static_cast<std::size_t>(index)]) { return; }
    m_txEqBand[static_cast<std::size_t>(index)] = clamped;
    // Thetis TXProfile keys: TXEQ1..TXEQ10 (1-indexed, per database.cs:4316-4325 [v2.10.3.13]).
    persistOne(QStringLiteral("TXEQ%1").arg(index + 1), QString::number(clamped));
    emit txEqBandChanged(index, clamped);
    emit txEqBandsJsonChanged(txEqBandsJson());  // R-R3-49 (parity Task 4)
    publishTxEqProfile();
}

void TransmitModel::setTxEqFreq(int index, int hz)
{
    if (index < 0 || index >= 10) { return; }
    const int clamped = std::clamp(hz, kTxEqFreqHzMin, kTxEqFreqHzMax);
    if (clamped == m_txEqFreq[static_cast<std::size_t>(index)]) { return; }
    m_txEqFreq[static_cast<std::size_t>(index)] = clamped;
    // Thetis TXProfile keys: TxEqFreq1..TxEqFreq10 (mixed-case per database.cs:4326-4335 [v2.10.3.13]).
    persistOne(QStringLiteral("TxEqFreq%1").arg(index + 1), QString::number(clamped));
    emit txEqFreqChanged(index, clamped);
    emit txEqFreqsJsonChanged(txEqFreqsJson());  // R-R3-49 (parity Task 4)
    publishTxEqProfile();
}

void TransmitModel::setTxLevelerOn(bool on)
{
    if (on == m_txLevelerOn) { return; }
    // From Thetis setup.cs:9108-9123 [v2.10.3.13] — chkDSPLevelerEnabled_CheckedChanged
    // routes through DSPTX::TXLevelerOn → SetTXALevelerSt.  TxChannel side is
    // wired in 3M-3a-i Batch 2 (RadioModel signal/slot glue).
    m_txLevelerOn = on;
    persistOne(QStringLiteral("Lev_On"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit txLevelerOnChanged(on);
}

void TransmitModel::setTxLevelerMaxGain(int dB)
{
    // Clamp to Thetis Designer range per setup.Designer.cs:38718-38738 [v2.10.3.13]:
    //   udDSPLevelerThreshold.Maximum = 20, .Minimum = 0.
    const int clamped = std::clamp(dB, kTxLevelerMaxGainDbMin, kTxLevelerMaxGainDbMax);
    if (clamped == m_txLevelerMaxGain) { return; }
    // From Thetis setup.cs:9095-9099 [v2.10.3.13] — udDSPLevelerThreshold_ValueChanged
    // routes through DSPTX::TXLevelerMaxGain → SetTXALevelerTop.
    m_txLevelerMaxGain = clamped;
    persistOne(QStringLiteral("Lev_MaxGain"), QString::number(m_txLevelerMaxGain));
    emit txLevelerMaxGainChanged(clamped);
}

void TransmitModel::setTxLevelerDecay(int ms)
{
    // Clamp to Thetis Designer range per setup.Designer.cs:38744-38772 [v2.10.3.13]:
    //   udDSPLevelerDecay.Maximum = 5000, .Minimum = 1.
    const int clamped = std::clamp(ms, kTxLevelerDecayMsMin, kTxLevelerDecayMsMax);
    if (clamped == m_txLevelerDecay) { return; }
    // From Thetis setup.cs:9101-9105 [v2.10.3.13] — udDSPLevelerDecay_ValueChanged
    // routes through DSPTX::TXLevelerDecay → SetTXALevelerDecay.
    m_txLevelerDecay = clamped;
    persistOne(QStringLiteral("Lev_Decay"), QString::number(m_txLevelerDecay));
    emit txLevelerDecayChanged(clamped);
}

void TransmitModel::setTxAlcMaxGain(int dB)
{
    // Clamp to Thetis Designer range per setup.Designer.cs:38814-38833 [v2.10.3.13]:
    //   udDSPALCMaximumGain.Maximum = 120, .Minimum = 0.
    const int clamped = std::clamp(dB, kTxAlcMaxGainDbMin, kTxAlcMaxGainDbMax);
    if (clamped == m_txAlcMaxGain) { return; }
    // From Thetis setup.cs:9129-9134 [v2.10.3.13] — udDSPALCMaximumGain_ValueChanged
    // calls SetTXAALCMaxGain directly + caches WDSP.ALCGain readout.
    m_txAlcMaxGain = clamped;
    persistOne(QStringLiteral("ALC_MaximumGain"), QString::number(m_txAlcMaxGain));
    emit txAlcMaxGainChanged(clamped);
}

void TransmitModel::setTxAlcDecay(int ms)
{
    // Clamp to Thetis Designer range per setup.Designer.cs:38845-38866 [v2.10.3.13]:
    //   udDSPALCDecay.Maximum = 50, .Minimum = 1.
    const int clamped = std::clamp(ms, kTxAlcDecayMsMin, kTxAlcDecayMsMax);
    if (clamped == m_txAlcDecay) { return; }
    // From Thetis setup.cs:9136-9140 [v2.10.3.13] — udDSPALCDecay_ValueChanged
    // routes through DSPTX::TXALCDecay → SetTXAALCDecay.
    m_txAlcDecay = clamped;
    persistOne(QStringLiteral("ALC_Decay"), QString::number(m_txAlcDecay));
    emit txAlcDecayChanged(clamped);
}

void TransmitModel::setTxEqNc(int nc)
{
    if (nc == m_txEqNc) { return; }
    // From WDSP wdsp/TXA.c:118 [v2.10.3.13] — create_eqp coefficient count
    //   max(2048, ch[].dsp_size).  Defensive non-negative guard only.
    if (nc < 1) { nc = 1; }
    m_txEqNc = nc;
    persistOne(QStringLiteral("eq/nc"), QString::number(m_txEqNc));
    emit txEqNcChanged(m_txEqNc);
}

void TransmitModel::setTxEqMp(bool mp)
{
    if (mp == m_txEqMp) { return; }
    m_txEqMp = mp;
    persistOne(QStringLiteral("eq/mp"),
               mp ? QStringLiteral("True") : QStringLiteral("False"));
    emit txEqMpChanged(mp);
}

void TransmitModel::setTxEqCtfmode(int mode)
{
    if (mode == m_txEqCtfmode) { return; }
    m_txEqCtfmode = mode;
    persistOne(QStringLiteral("eq/ctfmode"), QString::number(m_txEqCtfmode));
    emit txEqCtfmodeChanged(mode);
}

void TransmitModel::setTxEqWintype(int wintype)
{
    if (wintype == m_txEqWintype) { return; }
    m_txEqWintype = wintype;
    persistOne(QStringLiteral("eq/wintype"), QString::number(m_txEqWintype));
    emit txEqWintypeChanged(wintype);
}

void TransmitModel::setTxEqParaEqData(const QString& data)
{
    if (data == m_txEqParaEqData) { return; }
    // 3M-3a-ii follow-up Batch 6 — opaque blob, no validation.  Mirror of
    // setCfcParaEqData (Batch 2).  Stored under per-MAC AppSettings key
    // "TXParaEQData" alongside the other TX EQ profile fields; the
    // ParametricEqWidget layer wraps/unwraps the inner JSON via
    // ParaEqEnvelope (gzip+base64url) so this stays a pass-through.
    m_txEqParaEqData = data;
    persistOne(QStringLiteral("TXParaEQData"), data);
    emit txEqParaEqDataChanged(data);
    // R-IOS-13 / R-R3-49: the read-only curve follows the blob.
    const QString curve = ParaEqCurve::txEqCurveJson(data);
    if (curve != m_txEqCurve) {
        m_txEqCurve = curve;
        emit txEqCurveChanged(m_txEqCurve);
    }
}

// ── R-R3-49 (parity Task 4): the Legacy EQ box and the link's arrays ─────

void TransmitModel::setTxEqUseLegacy(bool on)
{
    if (on == m_txEqUseLegacy) { return; }
    // From Thetis setup.cs:9318 [v2.10.3.15]:
    //   console.EQForm.UsingLegacyEQ = (bool)dr["EQUseLegacy"];
    // (Above it in the same restore, on the VAC lines it disables first:
    //   // diable the vacs, so we can make changes without them trying to re-init etc MW0LGE_21dk5
    //  [original inline comment from setup.cs:9313].)
    // and setup.cs:3615: dr["EQUseLegacy"] = console.EQForm.UsingLegacyEQ;
    // Thetis keeps it with the TX profile; so does NereusSDR.
    m_txEqUseLegacy = on;
    persistOne(QStringLiteral("EQUseLegacy"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit txEqUseLegacyChanged(on);
}


QString TransmitModel::txEqBandsJson() const
{
    return tenValuesJson([this](int i) { return txEqBand(i); });
}

QString TransmitModel::txEqFreqsJson() const
{
    return tenValuesJson([this](int i) { return txEqFreq(i); });
}

QString TransmitModel::cfcCompressionJson() const
{
    return tenValuesJson([this](int i) { return cfcCompression(i); });
}

QString TransmitModel::cfcEqFreqJson() const
{
    return tenValuesJson([this](int i) { return cfcEqFreq(i); });
}

QString TransmitModel::cfcPostEqBandGainJson() const
{
    return tenValuesJson([this](int i) { return cfcPostEqBandGain(i); });
}

void TransmitModel::setTxEqBandsJson(const QString& json)
{
    std::array<int, 10> values{};
    if (!tenValuesFromJson(json, values)) { return; }
    for (int i = 0; i < 10; ++i) { setTxEqBand(i, values[static_cast<std::size_t>(i)]); }
}

void TransmitModel::setTxEqFreqsJson(const QString& json)
{
    std::array<int, 10> values{};
    if (!tenValuesFromJson(json, values)) { return; }
    for (int i = 0; i < 10; ++i) { setTxEqFreq(i, values[static_cast<std::size_t>(i)]); }
}

void TransmitModel::setCfcCompressionJson(const QString& json)
{
    std::array<int, 10> values{};
    if (!tenValuesFromJson(json, values)) { return; }
    if (!cfcProfileRestoreInProgress() && !m_projectingPairedCfc
        && updatePairedCfcArray(CfcField::Compression, values)) { return; }
    // Coalesce only real fallback changes; an ordinary array edit is not an
    // authoritative restore and must not invalidate a cached exact profile.
    ++m_cfcProfileUpdateDepth;
    const auto batch = qScopeGuard([this] { endCfcProfileUpdate(); });
    for (int i = 0; i < 10; ++i) { setCfcCompression(i, values[static_cast<std::size_t>(i)]); }
}

void TransmitModel::setCfcEqFreqJson(const QString& json)
{
    std::array<int, 10> values{};
    if (!tenValuesFromJson(json, values)) { return; }
    if (!cfcProfileRestoreInProgress() && !m_projectingPairedCfc
        && updatePairedCfcArray(CfcField::Frequency, values)) { return; }
    // Coalesce only real fallback changes; an ordinary array edit is not an
    // authoritative restore and must not invalidate a cached exact profile.
    ++m_cfcProfileUpdateDepth;
    const auto batch = qScopeGuard([this] { endCfcProfileUpdate(); });
    for (int i = 0; i < 10; ++i) { setCfcEqFreq(i, values[static_cast<std::size_t>(i)]); }
}

void TransmitModel::setCfcPostEqBandGainJson(const QString& json)
{
    std::array<int, 10> values{};
    if (!tenValuesFromJson(json, values)) { return; }
    if (!cfcProfileRestoreInProgress() && !m_projectingPairedCfc
        && updatePairedCfcArray(CfcField::PostEqBandGain, values)) { return; }
    // Coalesce only real fallback changes; an ordinary array edit is not an
    // authoritative restore and must not invalidate a cached exact profile.
    ++m_cfcProfileUpdateDepth;
    const auto batch = qScopeGuard([this] { endCfcProfileUpdate(); });
    for (int i = 0; i < 10; ++i) { setCfcPostEqBandGain(i, values[static_cast<std::size_t>(i)]); }
}

namespace {
CfcProfile::Profile pairedCfcEditState(const CfcEditProfile& state)
{
    CfcProfile::Profile p;
    const auto vector = [](const QVector<double>& v) { return std::vector<double>(v.begin(), v.end()); };
    p.f = vector(state.compression.frequenciesHz); p.postF = vector(state.postEq.frequenciesHz);
    p.g = vector(state.compression.gainsDb); p.e = vector(state.postEq.gainsDb);
    p.qg = vector(state.compression.q); p.qe = vector(state.postEq.q);
    p.minHz = state.compression.frequencyMinHz; p.maxHz = state.compression.frequencyMaxHz;
    p.postMinHz = state.postEq.frequencyMinHz; p.postMaxHz = state.postEq.frequencyMaxHz;
    p.precompDb = state.compression.globalGainDb; p.postEqGainDb = state.postEq.globalGainDb;
    p.compParametric = state.compression.useQ; p.eqParametric = state.postEq.useQ;
    return p;
}
CfcEditProfile nativeCfcEditState(const CfcProfile::Profile& p)
{
    CfcEditProfile state;
    const auto vector = [](const std::vector<double>& v) { return QVector<double>(v.begin(), v.end()); };
    state.compression.frequenciesHz = vector(p.f); state.postEq.frequenciesHz = vector(p.postF);
    state.compression.gainsDb = vector(p.g); state.postEq.gainsDb = vector(p.e);
    state.compression.q = vector(p.qg); state.postEq.q = vector(p.qe);
    state.compression.frequencyMinHz = p.minHz; state.compression.frequencyMaxHz = p.maxHz;
    state.postEq.frequencyMinHz = p.postMinHz; state.postEq.frequencyMaxHz = p.postMaxHz;
    state.compression.globalGainDb = p.precompDb; state.postEq.globalGainDb = p.postEqGainDb;
    state.compression.useQ = p.compParametric; state.postEq.useQ = p.eqParametric;
    return state;
}
} // namespace

bool TransmitModel::updatePairedCfcArray(CfcField field, const std::array<int, 10>& values)
{
    CfcProfile::Profile p;
    if (!CfcProfile::decode(m_cfcParaEqData, p)) { return false; }
    if (m_activeCfcProfile) { p = pairedCfcEditState(*m_activeCfcProfile); }
    if (p.f.size() != 10) { return true; }
    // The curve already holds these values (rounded, as its ten-band
    // mirrors read them): no change to the curve, which is kept as it is.
    // Re-encoding it would change a curve nobody edited, and a late answer
    // from the Core that repeats a value would then overwrite a newer
    // curve still waiting to be sent (the cfcPhaseRotatorAndCessbRoundTrip
    // load failure). False, not true: the per-band setters then run, each
    // finds its band unchanged in the curve too, and its own equal-value
    // check decides the integer mirror, which a restored Thetis profile
    // can hold apart from the curve.
    bool unchanged = true;
    for (int i = 0; i < 10 && unchanged; ++i) {
        const auto k = static_cast<std::size_t>(i);
        const double held = field == CfcField::Frequency ? p.f[k]
            : field == CfcField::Compression             ? p.g[k]
                                                          : p.e[k];
        unchanged = std::lround(held) == values[k];
    }
    if (unchanged) { return false; }
    for (int i = 0; i < 10; ++i) {
        const double value = values[static_cast<std::size_t>(i)];
        if (field == CfcField::Frequency) {
            p.f[static_cast<std::size_t>(i)] = value;
            p.postF[static_cast<std::size_t>(i)] = value;
        }
        if (field == CfcField::Compression) { p.g[static_cast<std::size_t>(i)] = value; }
        if (field == CfcField::PostEqBandGain) { p.e[static_cast<std::size_t>(i)] = value; }
    }
    if (field == CfcField::Frequency) {
        p.minHz = p.f.front();
        p.maxHz = p.f.back();
        p.postMinHz = p.postF.front();
        p.postMaxHz = p.postF.back();
    }
    const QString encoded = CfcProfile::encode(p);
    if (!encoded.isEmpty()) {
        if (m_activeCfcProfile) { setCfcProfile(nativeCfcEditState(p)); }
        else { setCfcParaEqData(encoded); }
    }
    return true;
}

bool TransmitModel::updatePairedCfc(CfcField field, int index, double value)
{
    CfcProfile::Profile p;
    if (!CfcProfile::decode(m_cfcParaEqData, p)) { return false; }
    if (m_activeCfcProfile) { p = pairedCfcEditState(*m_activeCfcProfile); }
    // As updatePairedCfcArray: a value the curve already holds (rounded,
    // as its integer mirror reads it) is no change to the curve, which is
    // kept. False hands the write to the setter's own mirror path and its
    // equal-value check: the mirror can differ from the curve after a
    // restore, and must still follow a real write.
    const auto held = [&p, field, index]() -> std::optional<double> {
        if (field == CfcField::Precomp) { return p.precompDb; }
        if (field == CfcField::PostEqGlobal) { return p.postEqGainDb; }
        if (p.f.size() != 10 || index < 0 || index >= 10) { return std::nullopt; }
        const auto k = static_cast<std::size_t>(index);
        if (field == CfcField::Frequency) { return p.f[k]; }
        if (field == CfcField::Compression) { return p.g[k]; }
        return p.e[k];
    }();
    if (held && std::lround(*held) == std::lround(value)) { return false; }
    if (field == CfcField::Precomp) { p.precompDb = value; }
    else if (field == CfcField::PostEqGlobal) { p.postEqGainDb = value; }
    else {
        if (p.f.size() != 10 || index < 0 || index >= 10) { return true; }
        const auto k = static_cast<std::size_t>(index);
        if (field == CfcField::Frequency) {
            p.f[k] = value;
            p.postF[k] = value;
            if (index == 0) { p.minHz = value; p.postMinHz = value; }
            if (index == 9) { p.maxHz = value; p.postMaxHz = value; }
        } else if (field == CfcField::Compression) { p.g[k] = value; }
        else if (field == CfcField::PostEqBandGain) { p.e[k] = value; }
    }
    const QString encoded = CfcProfile::encode(p);
    if (!encoded.isEmpty()) {
        if (m_activeCfcProfile) { setCfcProfile(nativeCfcEditState(p)); }
        else { setCfcParaEqData(encoded); }
    }
    return true;
}

// ── R-R3-49 (parity Task 5): the per-band power and tune power ────────────

QString TransmitModel::powerByBandJson() const
{
    return bandWattsJson([this](Band band) { return powerForBand(band); });
}

QString TransmitModel::tunePowerByBandJson() const
{
    return bandWattsJson([this](Band band) { return tunePowerForBand(band); });
}

void TransmitModel::setPowerByBandJson(const QString& json)
{
    std::vector<std::pair<Band, int>> named;
    if (!bandWattsFromJson(json, std::numeric_limits<int>::min(),
                           std::numeric_limits<int>::max(), named)) {
        return;
    }
    for (const auto& [band, watts] : named) { setPowerForBand(band, watts); }
}

void TransmitModel::setTunePowerByBandJson(const QString& json)
{
    std::vector<std::pair<Band, int>> named;
    if (!bandWattsFromJson(json, std::numeric_limits<int>::min(),
                           std::numeric_limits<int>::max(), named)) {
        return;
    }
    for (const auto& [band, watts] : named) { setTunePowerForBand(band, watts); }
}

// ── CFC / CPDR / CESSB / Phase Rotator (3M-3a-ii Batch 2) ─────────────────
//
// Defaults sourced from Thetis database.cs:4724-4768 [v2.10.3.13] (TXProfile
// factory) except cpdrOn (console.cs:36430 — global console state).
//
// All setters are idempotent (skip emit + persist on unchanged value) and
// clamp to the appropriate Thetis Designer range.  Per-MAC AppSettings keys
// match Thetis TXProfile column names exactly except cpdrOn which sits at
// hardware/<mac>/tx/cpdr/on outside the per-profile namespace.

// ── Phase Rotator ─────────────────────────────────────────────────────────

void TransmitModel::setPhaseRotatorEnabled(bool on)
{
    if (on == m_phaseRotatorEnabled) { return; }
    // From Thetis database.cs:4726 [v2.10.3.13]: dr["CFCPhaseRotatorEnabled"].
    m_phaseRotatorEnabled = on;
    persistOne(QStringLiteral("CFCPhaseRotatorEnabled"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit phaseRotatorEnabledChanged(on);
}

void TransmitModel::setPhaseReverseEnabled(bool on)
{
    if (on == m_phaseReverseEnabled) { return; }
    // From Thetis database.cs:4727 [v2.10.3.13]: dr["CFCPhaseReverseEnabled"].
    m_phaseReverseEnabled = on;
    persistOne(QStringLiteral("CFCPhaseReverseEnabled"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit phaseReverseEnabledChanged(on);
}

void TransmitModel::setPhaseRotatorFreqHz(int hz)
{
    // Clamp to Thetis Designer range per setup.Designer.cs:46250-46259 [v2.10.3.13]:
    //   udPhRotFreq.Maximum = 2000, .Minimum = 10.
    const int clamped = std::clamp(hz, kPhaseRotatorFreqHzMin, kPhaseRotatorFreqHzMax);
    if (clamped == m_phaseRotatorFreqHz) { return; }
    m_phaseRotatorFreqHz = clamped;
    persistOne(QStringLiteral("CFCPhaseRotatorFreq"), QString::number(clamped));
    emit phaseRotatorFreqHzChanged(clamped);
}

void TransmitModel::setPhaseRotatorStages(int stages)
{
    // Clamp to Thetis Designer range per setup.Designer.cs:46209-46218 [v2.10.3.13]:
    //   udPHROTStages.Maximum = 16, .Minimum = 2.
    const int clamped = std::clamp(stages, kPhaseRotatorStagesMin, kPhaseRotatorStagesMax);
    if (clamped == m_phaseRotatorStages) { return; }
    m_phaseRotatorStages = clamped;
    persistOne(QStringLiteral("CFCPhaseRotatorStages"), QString::number(clamped));
    emit phaseRotatorStagesChanged(clamped);
}

// ── CFC scalars ───────────────────────────────────────────────────────────

void TransmitModel::setCfcEnabled(bool on)
{
    if (on == m_cfcEnabled) { return; }
    // From Thetis database.cs:4724 [v2.10.3.13]: dr["CFCEnabled"].
    m_cfcEnabled = on;
    persistOne(QStringLiteral("CFCEnabled"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit cfcEnabledChanged(on);
}

void TransmitModel::setCfcPostEqEnabled(bool on)
{
    if (on == m_cfcPostEqEnabled) { return; }
    // From Thetis database.cs:4725 [v2.10.3.13]: dr["CFCPostEqEnabled"].
    m_cfcPostEqEnabled = on;
    persistOne(QStringLiteral("CFCPostEqEnabled"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit cfcPostEqEnabledChanged(on);
}

void TransmitModel::setCfcPrecompDb(int dB)
{
    // Clamp to Thetis Designer range per frmCFCConfig.Designer.cs:408-422
    // [v2.10.3.13]:  nudCFC_precomp.Maximum = 16, .Minimum = 0.
    const int clamped = std::clamp(dB, kCfcPrecompDbMin, kCfcPrecompDbMax);
    if (!cfcProfileRestoreInProgress() && !m_projectingPairedCfc
        && updatePairedCfc(CfcField::Precomp, -1, clamped)) { return; }
    if (clamped == m_cfcPrecompDb) { return; }
    m_cfcPrecompDb = clamped;
    persistOne(QStringLiteral("CFCPreComp"), QString::number(clamped));
    emit cfcPrecompDbChanged(clamped);
    if (!m_projectingPairedCfc) { notifyCfcProfileChange(); }
}

void TransmitModel::setCfcPostEqGainDb(int dB)
{
    // Clamp to Thetis Designer range per frmCFCConfig.Designer.cs:337-351
    // [v2.10.3.13]:  nudCFC_posteqgain.Maximum = 24, .Minimum = -24
    // (encoded via decimal sign bit in the 4th int).
    const int clamped = std::clamp(dB, kCfcPostEqGainDbMin, kCfcPostEqGainDbMax);
    if (!cfcProfileRestoreInProgress() && !m_projectingPairedCfc
        && updatePairedCfc(CfcField::PostEqGlobal, -1, clamped)) { return; }
    if (clamped == m_cfcPostEqGainDb) { return; }
    m_cfcPostEqGainDb = clamped;
    persistOne(QStringLiteral("CFCPostEqGain"), QString::number(clamped));
    emit cfcPostEqGainDbChanged(clamped);
    if (!m_projectingPairedCfc) { notifyCfcProfileChange(); }
}

// ── CFC per-band arrays ───────────────────────────────────────────────────

int TransmitModel::cfcEqFreq(int index) const noexcept
{
    if (index < 0 || index >= 10) { return 0; }
    return m_cfcEqFreqHz[static_cast<std::size_t>(index)];
}

int TransmitModel::cfcCompression(int index) const noexcept
{
    if (index < 0 || index >= 10) { return 0; }
    return m_cfcCompressionDb[static_cast<std::size_t>(index)];
}

int TransmitModel::cfcPostEqBandGain(int index) const noexcept
{
    if (index < 0 || index >= 10) { return 0; }
    return m_cfcPostEqBandGainDb[static_cast<std::size_t>(index)];
}

void TransmitModel::setCfcEqFreq(int index, int hz)
{
    if (index < 0 || index >= 10) { return; }
    // Clamp to Thetis Designer range per frmCFCConfig.Designer.cs:267-286
    // [v2.10.3.13]:  nudCFC_f.Maximum = 20000, .Minimum = 0.
    const int clamped = std::clamp(hz, kCfcEqFreqHzMin, kCfcEqFreqHzMax);
    if (!cfcProfileRestoreInProgress() && !m_projectingPairedCfc
        && updatePairedCfc(CfcField::Frequency, index, clamped)) { return; }
    if (clamped == m_cfcEqFreqHz[static_cast<std::size_t>(index)]) { return; }
    m_cfcEqFreqHz[static_cast<std::size_t>(index)] = clamped;
    // Thetis TXProfile keys: CFCEqFreq0..CFCEqFreq9 (database.cs:4757-4766 [v2.10.3.13]).
    persistOne(QStringLiteral("CFCEqFreq%1").arg(index), QString::number(clamped));
    emit cfcEqFreqChanged(index, clamped);
    emit cfcEqFreqJsonChanged(cfcEqFreqJson());  // R-R3-49 (parity Task 4)
    if (!m_projectingPairedCfc) { notifyCfcProfileChange(); }
}

void TransmitModel::setCfcCompression(int index, int dB)
{
    if (index < 0 || index >= 10) { return; }
    // Clamp to Thetis Designer range per frmCFCConfig.Designer.cs:217-236
    // [v2.10.3.13]:  nudCFC_c.Maximum = 16, .Minimum = 0.
    const int clamped = std::clamp(dB, kCfcCompressionDbMin, kCfcCompressionDbMax);
    if (!cfcProfileRestoreInProgress() && !m_projectingPairedCfc
        && updatePairedCfc(CfcField::Compression, index, clamped)) { return; }
    if (clamped == m_cfcCompressionDb[static_cast<std::size_t>(index)]) { return; }
    m_cfcCompressionDb[static_cast<std::size_t>(index)] = clamped;
    // Thetis TXProfile keys: CFCPreComp0..CFCPreComp9 (database.cs:4735-4744
    // [v2.10.3.13]) — note the column name says "PreComp" but these are
    // the per-band G[] compression amounts.
    persistOne(QStringLiteral("CFCPreComp%1").arg(index), QString::number(clamped));
    emit cfcCompressionChanged(index, clamped);
    emit cfcCompressionJsonChanged(cfcCompressionJson());  // R-R3-49 (parity Task 4)
    if (!m_projectingPairedCfc) { notifyCfcProfileChange(); }
}

void TransmitModel::setCfcPostEqBandGain(int index, int dB)
{
    if (index < 0 || index >= 10) { return; }
    // Clamp to Thetis Designer range per frmCFCConfig.Designer.cs:564-583
    // [v2.10.3.13]:  nudCFC_gain.Maximum = 24, .Minimum = -24.
    const int clamped = std::clamp(dB, kCfcPostEqBandGainDbMin, kCfcPostEqBandGainDbMax);
    if (!cfcProfileRestoreInProgress() && !m_projectingPairedCfc
        && updatePairedCfc(CfcField::PostEqBandGain, index, clamped)) { return; }
    if (clamped == m_cfcPostEqBandGainDb[static_cast<std::size_t>(index)]) { return; }
    m_cfcPostEqBandGainDb[static_cast<std::size_t>(index)] = clamped;
    // Thetis TXProfile keys: CFCPostEqGain0..CFCPostEqGain9 (database.cs:4746-4755 [v2.10.3.13]).
    persistOne(QStringLiteral("CFCPostEqGain%1").arg(index), QString::number(clamped));
    emit cfcPostEqBandGainChanged(index, clamped);
    emit cfcPostEqBandGainJsonChanged(cfcPostEqBandGainJson());  // R-R3-49 (parity Task 4)
    if (!m_projectingPairedCfc) { notifyCfcProfileChange(); }
}

void TransmitModel::setCfcParaEqData(const QString& data)
{
    const bool cacheChanged = m_activeCfcProfile.has_value();
    m_activeCfcProfile.reset();
    if (data == m_cfcParaEqData) { if (cacheChanged) { notifyCfcProfileChange(); } return; }
    const bool nestedProjection = m_projectingPairedCfc;
    // From Thetis database.cs:4768 [v2.10.3.13]: dr["CFCParaEQData"] = "".
    // Stored as opaque string for forward-compat round-trip with imported
    // Thetis profiles.  No validation.
    m_cfcParaEqData = data;
    const quint64 generation = ++m_cfcProfileGeneration;
    persistOne(QStringLiteral("CFCParaEQData"), data);
    CfcProfile::Profile paired;
    if (!cfcProfileRestoreInProgress() && CfcProfile::decode(data, paired)) {
        // The paired blob is authoritative. Keep the older integer mirrors
        // useful to ten-band clients without writing back into the curve.
        QScopedValueRollback<bool> projecting(m_projectingPairedCfc, true);
        [&]() {
            setCfcPrecompDb(static_cast<int>(std::lround(paired.precompDb)));
            if (generation != m_cfcProfileGeneration) { return; }
            setCfcPostEqGainDb(static_cast<int>(std::lround(paired.postEqGainDb)));
            if (generation != m_cfcProfileGeneration) { return; }
            if (paired.f.size() == 10) {
                for (int i = 0; i < 10; ++i) {
                    const auto k = static_cast<std::size_t>(i);
                    setCfcEqFreq(i, static_cast<int>(std::lround(paired.f[k])));
                    if (generation != m_cfcProfileGeneration) { return; }
                    setCfcCompression(i, static_cast<int>(std::lround(paired.g[k])));
                    if (generation != m_cfcProfileGeneration) { return; }
                    setCfcPostEqBandGain(i, static_cast<int>(std::lround(paired.e[k])));
                    if (generation != m_cfcProfileGeneration) { return; }
                }
            }
        }();
    }
    // Nested writes notify only when the outer projection has released its
    // guard, so DSP and mirrors see the final curve once.
    if (!nestedProjection) { emit cfcParaEqDataChanged(m_cfcParaEqData); notifyCfcProfileChange(); }
}

// NereusSDR-original (R-R3-49, transmitSettingsVersion 15): what the CFC
// dialog shows, published. The paired blob is authoritative when the Core
// reads it (setCfcParaEqData); otherwise the ten-band values.
void TransmitModel::refreshCfcProfile()
{
    if (cfcProfileMutationInProgress()) { return; }
    CfcProfile::Profile paired;
    const QString profile = CfcProfile::decode(m_cfcParaEqData, paired)
        ? CfcProfile::publishedJson(paired, QStringLiteral("saved"))
        : CfcProfile::publishedJson(
              CfcProfile::legacyProfile(m_cfcEqFreqHz, m_cfcCompressionDb, m_cfcPostEqBandGainDb,
                                        m_cfcPrecompDb, m_cfcPostEqGainDb),
              QStringLiteral("legacy"));
    if (profile == m_cfcProfile) { return; }
    m_cfcProfile = profile;
    emit cfcProfileChanged(m_cfcProfile);
}

void TransmitModel::beginCfcProfileRestore() noexcept
{
    ++m_cfcProfileRestoreDepth;
    beginCfcProfileUpdate();
}

void TransmitModel::endCfcProfileRestore() noexcept
{
    if (m_cfcProfileRestoreDepth == 0) { return; }
    --m_cfcProfileRestoreDepth;
    endCfcProfileUpdate();
    if (m_cfcProfileRestoreDepth == 0) { emit cfcProfileRestored(); }
}

// ── CPDR ──────────────────────────────────────────────────────────────────

void TransmitModel::setCpdrOn(bool on)
{
    if (on == m_cpdrOn) { return; }
    // From Thetis console.cs:36430 [v2.10.3.13]:
    //   SetGeneralSetting(0, OtherButtonId.COMP, chkCPDR.Checked);
    // CPDR is global console state — NOT in TXProfile.  Persisted under
    // hardware/<mac>/tx/cpdr/on (outside the per-profile namespace).
    m_cpdrOn = on;
    persistOne(QStringLiteral("cpdr/on"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit cpdrOnChanged(on);
}

void TransmitModel::setCpdrLevelDb(int dB)
{
    // Clamp to Thetis Designer range per console.Designer.cs:6042-6043
    // [v2.10.3.13]:  ptbCPDR.Maximum = 20, .Minimum = 0.
    const int clamped = std::clamp(dB, kCpdrLevelDbMin, kCpdrLevelDbMax);
    if (clamped == m_cpdrLevelDb) { return; }
    // From Thetis setup.cs:9307 [v2.10.3.13]:
    // Upstream tags preserved: //MW0LGE (from cited setup.cs:9309) [v2.10.3.15]
    //   console.CPDRLevel = (int)dr["CompanderLevel"];
    m_cpdrLevelDb = clamped;
    persistOne(QStringLiteral("CompanderLevel"), QString::number(clamped));
    emit cpdrLevelDbChanged(clamped);
}

// ── AM carrier level ──────────────────────────────────────────────────────
void TransmitModel::setAmCarrierLevel(int percent)
{
    // Clamp to Thetis udTXAMCarrierLevel range (0..100 %).
    const int clamped = std::clamp(percent, kAmCarrierLevelMin, kAmCarrierLevelMax);
    if (clamped == m_amCarrierLevel) { return; }
    // From Thetis setup.cs:9628 [v2.10.3.15]:
    //   udTXAMCarrierLevel.Value = (int)dr["AM_Carrier_Level"];
    m_amCarrierLevel = clamped;
    persistOne(QStringLiteral("AM_Carrier_Level"), QString::number(clamped));
    emit amCarrierLevelChanged(clamped);
}

// ── CESSB ─────────────────────────────────────────────────────────────────

void TransmitModel::setCessbOn(bool on)
{
    if (on == m_cessbOn) { return; }
    // From Thetis database.cs:4689 [v2.10.3.13]: dr["CESSB_On"].
    m_cessbOn = on;
    persistOne(QStringLiteral("CESSB_On"),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    emit cessbOnChanged(on);
}

// ── TX filter bandwidth (Plan 4 D1) ─────────────────────────────────────────
//
// NereusSDR-original properties.  FilterLow/FilterHigh are the DSP bandpass
// filter edges (Hz) that will be fed to WDSP SetTXABandpassFreqs in Plan 4
// D8.  Defaults 100/2900 match the USB voice typical SSB range — the same
// values Thetis ships for the "Default" USB profile row in database.cs
// (Plan 4 spec §Task 2).
//
// Swap-on-commit: prevents an inverted range from reaching WDSP.  When
// setFilterLow(hz) is called with hz > m_filterHigh, the stored high value
// is swapped into the low slot and hz is stored in the high slot.  The
// converse applies to setFilterHigh.  This keeps low ≤ high at all times.
//
// Per-MAC persistence: hardware/<mac>/tx/FilterLow and FilterHigh,
// consistent with the L.2 namespace used by all other persisted TX props.

void TransmitModel::setFilterLow(int hz)
{
    // Swap-on-commit: if the new low would exceed the current high, flip them.
    if (hz > m_filterHigh) {
        std::swap(hz, m_filterHigh);
        persistOne(QStringLiteral("FilterHigh"), QString::number(m_filterHigh));
    }
    if (m_filterLow == hz) { return; }
    m_filterLow = hz;
    persistOne(QStringLiteral("FilterLow"), QString::number(m_filterLow));
    emit filterChanged(m_filterLow, m_filterHigh);
}

void TransmitModel::setFilterHigh(int hz)
{
    // Swap-on-commit: if the new high would be less than the current low, flip.
    if (hz < m_filterLow) {
        std::swap(hz, m_filterLow);
        persistOne(QStringLiteral("FilterLow"), QString::number(m_filterLow));
    }
    if (m_filterHigh == hz) { return; }
    m_filterHigh = hz;
    persistOne(QStringLiteral("FilterHigh"), QString::number(m_filterHigh));
    emit filterChanged(m_filterLow, m_filterHigh);
}

QString TransmitModel::filterDisplayText(DSPMode mode) const
{
    // Symmetric modes (AM/SAM/DSB/FM): display as ±half-bandwidth.
    // Asymmetric modes (USB/LSB/CWU/CWL/DIGU/DIGL/SPEC/DRM): display as low–high.
    const bool isSymmetric = (mode == DSPMode::AM  ||
                              mode == DSPMode::SAM  ||
                              mode == DSPMode::DSB  ||
                              mode == DSPMode::FM);

    const int bw = m_filterHigh - m_filterLow;
    const double bwKhz = bw / 1000.0;

    if (isSymmetric) {
        // Represent as ±half-bandwidth from carrier.
        const int halfBw = bw / 2;
        return QStringLiteral("±%1 Hz · %2k BW")
            .arg(halfBw)
            .arg(bwKhz, 0, 'f', 1);
    }

    return QStringLiteral("%1–%2 Hz · %3k BW")
        .arg(m_filterLow)
        .arg(m_filterHigh)
        .arg(bwKhz, 0, 'f', 1);
}

void TransmitModel::beginTxEqProfileUpdate()
{
    if (m_txEqProfileUpdateDepth++ == 0) {
        m_txEqProfileStartPreamp = m_txEqPreamp;
        m_txEqProfileStartBands = m_txEqBand;
        m_txEqProfileStartFreqs = m_txEqFreq;
    }
}

void TransmitModel::endTxEqProfileUpdate()
{
    Q_ASSERT(m_txEqProfileUpdateDepth > 0);
    if (--m_txEqProfileUpdateDepth == 0 &&
        (m_txEqPreamp != m_txEqProfileStartPreamp || m_txEqBand != m_txEqProfileStartBands ||
         m_txEqFreq != m_txEqProfileStartFreqs)) {
        publishTxEqProfile();
    }
}

void TransmitModel::publishTxEqProfile()
{
    if (m_txEqProfileUpdateDepth > 0) { return; }
    QList<int> frequencies(m_txEqFreq.begin(), m_txEqFreq.end());
    QList<int> gains{m_txEqPreamp};
    for (int gain : m_txEqBand) { gains.append(gain); }
    emit txEqProfileChanged(frequencies, gains);
}

CfcEditProfile TransmitModel::effectiveCfcProfile() const
{
    if (m_activeCfcProfile) { return *m_activeCfcProfile; }
    if (const std::optional<CfcEditProfile> decoded = decodeCfcEditProfile(m_cfcParaEqData)) {
        return *decoded;
    }
    CfcProfile::Profile mainProfile;
    if (CfcProfile::decode(m_cfcParaEqData, mainProfile)) { return nativeCfcEditState(mainProfile); }
    CfcEditProfile fallback;
    fallback.compression.globalGainDb = m_cfcPrecompDb;
    fallback.postEq.globalGainDb = m_cfcPostEqGainDb;
    for (std::size_t i = 0; i < 10; ++i) {
        fallback.compression.frequenciesHz.append(m_cfcEqFreqHz[i]);
        fallback.compression.gainsDb.append(m_cfcCompressionDb[i]);
        fallback.compression.q.append(4.0);
        fallback.postEq.gainsDb.append(m_cfcPostEqBandGainDb[i]);
        fallback.postEq.q.append(4.0);
    }
    fallback.postEq.frequenciesHz = fallback.compression.frequenciesHz;
    // Retain the existing dialog seed bounds, widening for legacy endpoints.
    fallback.compression.frequencyMaxHz = std::max(fallback.compression.frequencyMaxHz,
        static_cast<double>(*std::max_element(m_cfcEqFreqHz.begin(), m_cfcEqFreqHz.end())));
    fallback.postEq.frequencyMinHz = fallback.compression.frequencyMinHz;
    fallback.postEq.frequencyMaxHz = fallback.compression.frequencyMaxHz;
    return fallback;
}

void TransmitModel::beginCfcProfileUpdate()
{
    if (m_cfcProfileUpdateDepth++ == 0) {
        // An authoritative load must restore saved precision even for the same blob.
        m_activeCfcProfile.reset();
        m_cfcProfileDirty = true;
    }
}

void TransmitModel::endCfcProfileUpdate()
{
    Q_ASSERT(m_cfcProfileUpdateDepth > 0);
    if (--m_cfcProfileUpdateDepth == 0 && m_cfcProfileDirty) {
        m_cfcProfileDirty = false;
        emit cfcEditProfileChanged(effectiveCfcProfile());
        refreshCfcProfile();
    }
}

void TransmitModel::notifyCfcProfileChange()
{
    if (m_cfcProfileUpdateDepth > 0) { m_cfcProfileDirty = true; }
    else { emit cfcEditProfileChanged(effectiveCfcProfile()); }
}

bool TransmitModel::setCfcProfile(const CfcEditProfile& profile)
{
    if (!isValidCfcEditProfile(profile)) { return false; }
    if (m_activeCfcProfile && *m_activeCfcProfile == profile) { return true; }
    beginCfcProfileRestore();
    const auto batch = qScopeGuard([this] { endCfcProfileRestore(); });
    setCfcPrecompDb(qRound(profile.compression.globalGainDb));
    setCfcPostEqGainDb(qRound(profile.postEq.globalGainDb));
    if (profile.compression.frequenciesHz.size() == 10) {
        for (int i = 0; i < 10; ++i) {
            setCfcEqFreq(i, qRound(profile.compression.frequenciesHz[i]));
            setCfcCompression(i, qRound(profile.compression.gainsDb[i]));
            setCfcPostEqBandGain(i, qRound(profile.postEq.gainsDb[i]));
        }
    }
    setCfcParaEqData(encodeCfcEditProfile(profile));
    m_activeCfcProfile = profile;
    return true;
}

} // namespace NereusSDR
