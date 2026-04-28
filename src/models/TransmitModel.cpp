// =================================================================
// src/models/TransmitModel.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
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

#include "TransmitModel.h"
#include "core/AppSettings.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {
// Number of bands in the per-band array — matches Band::Count (14).
// From Thetis console.cs:12094 [v2.10.3.13]: int[] tunePower_by_band sized
// to (int)Band.LAST, which equals 14 for the Thetis Band enum.
constexpr int kBandCount = static_cast<int>(Band::Count);
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

TransmitModel::TransmitModel(QObject* parent)
    : QObject(parent)
{
    // Initialise per-band tune power to 50W.
    // From Thetis console.cs:1819-1820 [v2.10.3.13]:
    //   tunePower_by_band = new int[(int)Band.LAST];
    //   for (int i = 0; i < (int)Band.LAST; i++) tunePower_by_band[i] = 50;
    m_tunePowerByBand.fill(50);
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

void TransmitModel::setMicGainDb(int dB)
{
    // Clamp to range per Thetis console.cs:19151-19171 [v2.10.3.13].
    // Thetis runtime defaults: mic_gain_min = -40, mic_gain_max = 10.
    // NereusSDR model range [-50, 70] per plan §C.1.
    const int clamped = std::clamp(dB, kMicGainDbMin, kMicGainDbMax);
    if (clamped == m_micGainDb) { return; }  // idempotent guard

    m_micGainDb = clamped;
    // Porting from Thetis console.cs:28805-28817 [v2.10.3.13]:
    //   Audio.MicPreamp = Math.Pow(10.0, gain_db / 20.0); // convert to scalar
    m_micPreampLinear = std::pow(10.0, clamped / 20.0);

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
}

void TransmitModel::setMicBoost(bool on)
{
    if (on == m_micBoost) { return; }  // idempotent guard
    // Porting from Thetis console.cs:13237-13246 [v2.10.3.13]:
    //   mic_boost = value; ptbMic_Scroll(); SetMicGain();
    // Phase D wires the WDSP side; model just stores + signals.
    m_micBoost = on;
    emit micBoostChanged(on);
}

void TransmitModel::setMicXlr(bool on)
{
    if (on == m_micXlr) { return; }  // idempotent guard
    // Porting from Thetis console.cs:13249-13258 [v2.10.3.13]:
    //   mic_xlr = value; ptbMic_Scroll(); SetMicXlr();
    // Phase G wires the SetMicXlr() bit; model just stores + signals.
    m_micXlr = on;
    emit micXlrChanged(on);
}

void TransmitModel::setLineIn(bool on)
{
    if (on == m_lineIn) { return; }  // idempotent guard
    // Porting from Thetis console.cs:13213-13222 [v2.10.3.13]:
    //   line_in = value; ptbMic_Scroll(); SetMicGain();
    m_lineIn = on;
    emit lineInChanged(on);
}

void TransmitModel::setLineInBoost(double dB)
{
    // Clamp to Thetis range per setup.designer.cs:46898-46907 [v2.10.3.13]:
    //   udLineInBoost.Minimum = -34.5, udLineInBoost.Maximum = 12.0
    const double clamped = std::clamp(dB, kLineInBoostMin, kLineInBoostMax);
    if (clamped == m_lineInBoost) { return; }  // idempotent guard
    // Porting from Thetis console.cs:13225-13234 [v2.10.3.13]:
    //   line_in_boost = value; ptbMic_Scroll(); SetMicGain();
    m_lineInBoost = clamped;
    emit lineInBoostChanged(clamped);
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
    emit micPttDisabledChanged(disabled);
}

// ── Per-band tune power (G.3) ─────────────────────────────────────────────

int TransmitModel::tunePowerForBand(Band band) const
{
    const int idx = static_cast<int>(band);
    if (idx < 0 || idx >= kBandCount) {
        return 50;  // safe fallback for out-of-range band
    }
    return m_tunePowerByBand[static_cast<std::size_t>(idx)];
}

void TransmitModel::setTunePowerForBand(Band band, int watts)
{
    // From Thetis console.cs:12094 [v2.10.3.13]: private int[] tunePower_by_band;
    const int idx = static_cast<int>(band);
    if (idx < 0 || idx >= kBandCount) {
        return;
    }
    const int clamped = std::clamp(watts, 0, 100);
    if (m_tunePowerByBand[static_cast<std::size_t>(idx)] == clamped) {
        return;
    }
    m_tunePowerByBand[static_cast<std::size_t>(idx)] = clamped;
    emit tunePowerByBandChanged(band, clamped);
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
    for (int i = 0; i < kBandCount; ++i) {
        const QString key = prefix + QString::number(i);
        const int v = s.value(key, QStringLiteral("50")).toInt();
        m_tunePowerByBand[static_cast<std::size_t>(i)] = std::clamp(v, 0, 100);
    }
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
        s.setValue(prefix + QString::number(i),
                   QString::number(m_tunePowerByBand[static_cast<std::size_t>(i)]));
    }
}

// ── Anti-VOX properties (3M-1b C.4) ─────────────────────────────────────────
//
// Porting from Thetis audio.cs:446-454 [v2.10.3.13] (AntiVOXSourceVAC property):
//   private static bool antivox_source_VAC = false;
//   public static bool AntiVOXSourceVAC {
//     get { return antivox_source_VAC; }
//     set { antivox_source_VAC = value; cmaster.CMSetAntiVoxSourceWhat(); }
//   }
// Porting from Thetis setup.designer.cs:44699-44728 [v2.10.3.13] (udAntiVoxGain):
//   Minimum = decimal{60,0,0,-2147483648} = -60; Maximum = decimal{60,0,0,0} = 60.
// Porting from Thetis setup.cs:18986-18989 [v2.10.3.13] (udAntiVoxGain_ValueChanged):
//   cmaster.SetAntiVOXGain(0, Math.Pow(10.0, (double)udAntiVoxGain.Value / 20.0));
//
// WDSP wiring (SetAntiVOXGain + CMSetAntiVoxSourceWhat) deferred to Phase H.3.
// AppSettings persistence deferred to Phase L.2.

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
    emit antiVoxGainDbChanged(clamped);
}

void TransmitModel::setAntiVoxSourceVax(bool useVax)
{
    if (useVax == m_antiVoxSourceVax) { return; }  // idempotent guard
    // Porting from Thetis audio.cs:446-454 [v2.10.3.13]:
    //   antivox_source_VAC = value; cmaster.CMSetAntiVoxSourceWhat();
    // VAC->VAX rename: same conceptual role, NereusSDR-native design.
    // CMSetAntiVoxSourceWhat port (path-agnostic version) deferred to Phase H.3.
    m_antiVoxSourceVax = useVax;
    emit antiVoxSourceVaxChanged(useVax);
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
    emit voxHangTimeMsChanged(clamped);
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
    if (source == m_micSource) { return; }  // idempotent guard
    m_micSource = source;
    emit micSourceChanged(source);
}

// ── PC Mic session state (3M-1b I.2) ─────────────────────────────────────────
//
// NereusSDR-native transient session-state properties for the PC Mic
// configuration group (Setup → Audio → TX Input → PC Mic group box).
//
// All three setters are idempotent (no signal emitted on unchanged value).
// None of these persist across app restarts — AppSettings persistence is
// deferred to Phase L.2.  The properties survive Setup dialog close/reopen
// within the same session, stored on TransmitModel (Option B from plan §2.5).

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

} // namespace NereusSDR
