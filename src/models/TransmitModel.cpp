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

} // namespace NereusSDR
