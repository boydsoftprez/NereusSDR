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
// Number of bands in the per-band TX array — HF amateur + GEN/WWV/XVTR (14).
// From Thetis console.cs:12094 [v2.10.3.13]: int[] tunePower_by_band sized
// to (int)Band.LAST, which equals 14 for the Thetis Band enum.
//
// Phase 3L Note: NereusSDR's Band enum was extended to include 13 SWL bands
// (Band::SwlFirst..SwlLast) for HL2 N2ADR Filter pin assignments — but TX
// tune power is HF amateur only.  SWL bands inherit the closest ham-band
// value implicitly (no separate per-SWL persistence).
constexpr int kBandCount = static_cast<int>(Band::SwlFirst);  // 14
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
    const double clamped = std::clamp(dB, kLineInBoostMin, kLineInBoostMax);
    if (clamped == m_lineInBoost) { return; }  // idempotent guard
    // Porting from Thetis console.cs:13225-13234 [v2.10.3.13]:
    //   line_in_boost = value; ptbMic_Scroll(); SetMicGain();
    m_lineInBoost = clamped;
    persistOne(QStringLiteral("Line_Input_Level"), QString::number(m_lineInBoost));  // L.2 auto-persist
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

    // ── Anti-VOX properties ───────────────────────────────────────────────
    // antiVoxGainDb: default 0 (NereusSDR-original safe starting point)
    const int antiVoxGainDb = s.value(pfx + QLatin1String("AntiVox_Gain"),
                                       QStringLiteral("0")).toInt();
    setAntiVoxGainDb(antiVoxGainDb);
    // antiVoxSourceVax: default false (audio.cs:446 [v2.10.3.13])
    const bool antiVoxSourceVax = s.value(pfx + QLatin1String("AntiVox_Source_VAX"),
                                           QStringLiteral("False")).toString() == QLatin1String("True");
    setAntiVoxSourceVax(antiVoxSourceVax);

    // ── MON properties (monEnabled NOT loaded — safety: always false) ─────
    // monitorVolume: default 0.5f (audio.cs:417 [v2.10.3.13] literal)
    const float monitorVolume = s.value(pfx + QLatin1String("MonitorVolume"),
                                         QStringLiteral("0.5")).toFloat();
    setMonitorVolume(monitorVolume);

    // ── Mic source ────────────────────────────────────────────────────────
    // micSource: default Pc (NereusSDR-native; always safe and available)
    const QString micSourceStr = s.value(pfx + QLatin1String("Mic_Source"),
                                          QStringLiteral("Pc")).toString();
    const MicSource micSource = (micSourceStr == QLatin1String("Radio"))
                                    ? MicSource::Radio
                                    : MicSource::Pc;
    setMicSource(micSource);

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
    const double twoToneLevel = s.value(pfx + QLatin1String("TwoToneLevel"),
                                         QStringLiteral("-6")).toDouble();
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

    // ── CPDR (3M-3a-ii Batch 2) ───────────────────────────────────────────
    // cpdrOn lives at hardware/<mac>/tx/cpdr/on — outside the per-profile
    // namespace, per Thetis console.cs:36430 (global console state).
    setCpdrOn(s.value(pfx + QLatin1String("cpdr/on"),
                       QStringLiteral("False")).toString() == QLatin1String("True"));
    // CompanderLevel from database.cs:4580 [v2.10.3.13]: default 2 dB.
    setCpdrLevelDb(s.value(pfx + QLatin1String("CompanderLevel"),
                            QStringLiteral("2")).toInt());

    // ── CESSB (3M-3a-ii Batch 2) ──────────────────────────────────────────
    // Default from Thetis database.cs:4689 [v2.10.3.13]: dr["CESSB_On"] = false.
    setCessbOn(s.value(pfx + QLatin1String("CESSB_On"),
                        QStringLiteral("False")).toString() == QLatin1String("True"));
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

    // ── VOX properties (voxEnabled excluded — safety) ────────────────────
    s.setValue(pfx + QLatin1String("Dexp_Threshold"),   QString::number(m_voxThresholdDb));
    s.setValue(pfx + QLatin1String("VOX_GainScalar"),    QString::number(static_cast<double>(m_voxGainScalar)));
    s.setValue(pfx + QLatin1String("VOX_HangTime"),    QString::number(m_voxHangTimeMs));

    // ── Anti-VOX properties ───────────────────────────────────────────────
    s.setValue(pfx + QLatin1String("AntiVox_Gain"),    QString::number(m_antiVoxGainDb));
    s.setValue(pfx + QLatin1String("AntiVox_Source_VAX"), m_antiVoxSourceVax ? QStringLiteral("True") : QStringLiteral("False"));

    // ── MON properties (monEnabled excluded — safety) ─────────────────────
    s.setValue(pfx + QLatin1String("MonitorVolume"),    QString::number(static_cast<double>(m_monitorVolume)));

    // ── Mic source ────────────────────────────────────────────────────────
    s.setValue(pfx + QLatin1String("Mic_Source"),
               m_micSource == MicSource::Radio ? QStringLiteral("Radio") : QStringLiteral("Pc"));

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

    // CESSB.
    s.setValue(pfx + QLatin1String("CESSB_On"),
               m_cessbOn ? QStringLiteral("True") : QStringLiteral("False"));
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
    persistOne(QStringLiteral("AntiVox_Gain"), QString::number(m_antiVoxGainDb));  // L.2 auto-persist
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
    persistOne(QStringLiteral("AntiVox_Source_VAX"), useVax ? QStringLiteral("True") : QStringLiteral("False"));  // L.2 auto-persist
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
    persistOne(QStringLiteral("Mic_Source"),
               source == MicSource::Radio ? QStringLiteral("Radio") : QStringLiteral("Pc"));  // L.2 auto-persist
    emit micSourceChanged(source);
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
    if (clamped == m_cfcPrecompDb) { return; }
    m_cfcPrecompDb = clamped;
    persistOne(QStringLiteral("CFCPreComp"), QString::number(clamped));
    emit cfcPrecompDbChanged(clamped);
}

void TransmitModel::setCfcPostEqGainDb(int dB)
{
    // Clamp to Thetis Designer range per frmCFCConfig.Designer.cs:337-351
    // [v2.10.3.13]:  nudCFC_posteqgain.Maximum = 24, .Minimum = -24
    // (encoded via decimal sign bit in the 4th int).
    const int clamped = std::clamp(dB, kCfcPostEqGainDbMin, kCfcPostEqGainDbMax);
    if (clamped == m_cfcPostEqGainDb) { return; }
    m_cfcPostEqGainDb = clamped;
    persistOne(QStringLiteral("CFCPostEqGain"), QString::number(clamped));
    emit cfcPostEqGainDbChanged(clamped);
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
    if (clamped == m_cfcEqFreqHz[static_cast<std::size_t>(index)]) { return; }
    m_cfcEqFreqHz[static_cast<std::size_t>(index)] = clamped;
    // Thetis TXProfile keys: CFCEqFreq0..CFCEqFreq9 (database.cs:4757-4766 [v2.10.3.13]).
    persistOne(QStringLiteral("CFCEqFreq%1").arg(index), QString::number(clamped));
    emit cfcEqFreqChanged(index, clamped);
}

void TransmitModel::setCfcCompression(int index, int dB)
{
    if (index < 0 || index >= 10) { return; }
    // Clamp to Thetis Designer range per frmCFCConfig.Designer.cs:217-236
    // [v2.10.3.13]:  nudCFC_c.Maximum = 16, .Minimum = 0.
    const int clamped = std::clamp(dB, kCfcCompressionDbMin, kCfcCompressionDbMax);
    if (clamped == m_cfcCompressionDb[static_cast<std::size_t>(index)]) { return; }
    m_cfcCompressionDb[static_cast<std::size_t>(index)] = clamped;
    // Thetis TXProfile keys: CFCPreComp0..CFCPreComp9 (database.cs:4735-4744
    // [v2.10.3.13]) — note the column name says "PreComp" but these are
    // the per-band G[] compression amounts.
    persistOne(QStringLiteral("CFCPreComp%1").arg(index), QString::number(clamped));
    emit cfcCompressionChanged(index, clamped);
}

void TransmitModel::setCfcPostEqBandGain(int index, int dB)
{
    if (index < 0 || index >= 10) { return; }
    // Clamp to Thetis Designer range per frmCFCConfig.Designer.cs:564-583
    // [v2.10.3.13]:  nudCFC_gain.Maximum = 24, .Minimum = -24.
    const int clamped = std::clamp(dB, kCfcPostEqBandGainDbMin, kCfcPostEqBandGainDbMax);
    if (clamped == m_cfcPostEqBandGainDb[static_cast<std::size_t>(index)]) { return; }
    m_cfcPostEqBandGainDb[static_cast<std::size_t>(index)] = clamped;
    // Thetis TXProfile keys: CFCPostEqGain0..CFCPostEqGain9 (database.cs:4746-4755 [v2.10.3.13]).
    persistOne(QStringLiteral("CFCPostEqGain%1").arg(index), QString::number(clamped));
    emit cfcPostEqBandGainChanged(index, clamped);
}

void TransmitModel::setCfcParaEqData(const QString& data)
{
    if (data == m_cfcParaEqData) { return; }
    // From Thetis database.cs:4768 [v2.10.3.13]: dr["CFCParaEQData"] = "".
    // Stored as opaque string for forward-compat round-trip with imported
    // Thetis profiles.  No validation.
    m_cfcParaEqData = data;
    persistOne(QStringLiteral("CFCParaEQData"), data);
    emit cfcParaEqDataChanged(data);
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
    //   console.CPDRLevel = (int)dr["CompanderLevel"];
    m_cpdrLevelDb = clamped;
    persistOne(QStringLiteral("CompanderLevel"), QString::number(clamped));
    emit cpdrLevelDbChanged(clamped);
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

} // namespace NereusSDR
