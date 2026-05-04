// =================================================================
// src/core/PaProfile.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs:23763-24046 (PAProfile class)
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Phase 2 Agent 2A of issue #167.
//   2026-05-03 — Phase 3 Agent 3B of issue #167: restored Thetis-faithful
//                 1000.0f sentinel return from getGainForBand() for
//                 out-of-range Band (was 0.0f from Phase 2A — that
//                 deviation silently bypassed the gbb >= 99.5 safety
//                 short-circuit in TransmitModel::computeAudioVolume
//                 and produced audio_volume = 1.0 on out-of-range
//                 Band casts).  J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

// --- From setup.cs ---

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

#include "PaProfile.h"

#include "PaGainProfile.h"

#include <QByteArray>
#include <QLatin1Char>
#include <QStringList>

namespace NereusSDR {

namespace {

// Thetis setup.cs:23811-23822 [v2.10.3.13] — base64Encode helper. UTF-8 plain
// text in, base64 ASCII out. Empty-string fallback on failure (Thetis catches
// any exception). Qt's QByteArray::toBase64 produces RFC-4648 base64 by default,
// which matches .NET Convert.ToBase64String byte-for-byte for simple input.
QString base64Encode(const QString& plainText) {
    return QString::fromLatin1(plainText.toUtf8().toBase64());
}

// Thetis setup.cs:23823-23834 [v2.10.3.13] — base64Decode helper. Returns
// the empty string on parse failure (mirrors the Thetis try/catch pattern).
QString base64Decode(const QString& base64) {
    const QByteArray decoded =
        QByteArray::fromBase64(base64.toLatin1(),
                               QByteArray::AbortOnBase64DecodingErrors);
    if (decoded.isEmpty() && !base64.isEmpty()) {
        return QString{};
    }
    return QString::fromUtf8(decoded);
}

// Format float with one decimal, matching Thetis `_gainValues[n].ToString("0.0")`
// (setup.cs:23892, 23899, 23907 [v2.10.3.13]).
QString formatFloat(float v) {
    return QString::number(static_cast<double>(v), 'f', 1);
}

}  // namespace

// ── Empty default constructor ──────────────────────────────────────────────
//
// Mirrors Thetis init loop (setup.cs:23797-23807 [v2.10.3.13]):
//   _gainValues[n] = 100; _gainAdjust[n, i] = 0; _maxPower[n] = 0;
//   _bUseMaxPower[n] = false;
PaProfile::PaProfile()
    : m_name(),
      m_model(HPSDRModel::FIRST),
      m_isFactoryDefault(false),
      m_gainValues{},
      m_gainAdjust{},
      m_maxPower{},
      m_useMaxPower{} {
    m_gainValues.fill(100.0f);
    for (auto& row : m_gainAdjust) {
        row.fill(0.0f);
    }
    m_maxPower.fill(0.0f);
    m_useMaxPower.fill(false);
}

// ── Named constructor ──────────────────────────────────────────────────────
//
// Faithful port of Thetis PAProfile(string, HPSDRModel, bool) constructor
// (setup.cs:23786-23810 [v2.10.3.13]). The init loop seeds 100.0f sentinels
// and zeros, then ResetGainDefaultsForModel overwrites the gain row with
// per-model factory values from PaGainProfile.
PaProfile::PaProfile(QString name, HPSDRModel model, bool isFactoryDefault)
    : m_name(std::move(name)),
      m_model(model),
      m_isFactoryDefault(isFactoryDefault),
      m_gainValues{},
      m_gainAdjust{},
      m_maxPower{},
      m_useMaxPower{} {
    // setup.cs:23797-23807 init loop
    m_gainValues.fill(100.0f);
    for (auto& row : m_gainAdjust) {
        row.fill(0.0f);
    }
    m_maxPower.fill(0.0f);
    m_useMaxPower.fill(false);

    // setup.cs:23809 final call — load factory rows for this model.
    resetGainDefaultsForModel(m_model);
}

// ── Accessors ──────────────────────────────────────────────────────────────

bool PaProfile::inRange(Band b) noexcept {
    const int idx = static_cast<int>(b);
    return idx >= 0 && idx < kBandCount;
}

// From Thetis setup.cs:23866 [v2.10.3.13] — GetGainForBand(Band, int):
//   if (!((int)b > (int)Band.FIRST && (int)b < (int)Band.LAST)) return 1000;
//   if (nDriveValue == -1)
//       return _gainValues[(int)b];
//   else
//       return _gainValues[(int)b] - calcDriveAdjust(b, nDriveValue);
//
// Thetis-faithful sentinel: out-of-range Band returns 1000.0f (a value
// well above any realistic PA gain).  Downstream `computeAudioVolume`
// short-circuits on `gbb >= 99.5` to its linear fallback, so the
// sentinel feeds the safety net automatically.  An earlier Phase 2A
// implementation returned 0.0f here, which silently bypassed the
// safety short-circuit and produced audio_volume = 1.0 (full rail) on
// out-of-range Band casts — a fail-loud safety regression that #167
// Phase 3B restored to Thetis-faithful behavior.
float PaProfile::getGainForBand(Band b, int driveValue) const noexcept {
    if (!inRange(b)) {
        return 1000.0f;
    }
    const int idx = static_cast<int>(b);
    if (driveValue == -1) {
        return m_gainValues[idx];
    }
    return m_gainValues[idx] - calcDriveAdjust(b, driveValue);
}

// From Thetis setup.cs:23991-23997 [v2.10.3.13] — GetAdjust(Band, int).
float PaProfile::getAdjust(Band b, int stepIndex) const noexcept {
    if (!inRange(b)) {
        return 0.0f;
    }
    if (stepIndex < 0 || stepIndex >= kDriveSteps) {
        return 0.0f;
    }
    return m_gainAdjust[static_cast<int>(b)][stepIndex];
}

// From Thetis setup.cs:23969-23973 [v2.10.3.13] — GetMaxPower(Band).
float PaProfile::getMaxPower(Band b) const noexcept {
    if (!inRange(b)) {
        return 0.0f;
    }
    return m_maxPower[static_cast<int>(b)];
}

// From Thetis setup.cs:23979-23983 [v2.10.3.13] — GetMaxPowerUse(Band).
bool PaProfile::getMaxPowerUse(Band b) const noexcept {
    if (!inRange(b)) {
        return false;
    }
    return m_useMaxPower[static_cast<int>(b)];
}

// ── Mutators ───────────────────────────────────────────────────────────────

// From Thetis setup.cs:23998-24003 [v2.10.3.13] — SetGainForBand(Band, float).
void PaProfile::setGainForBand(Band b, float gainDb) {
    if (!inRange(b)) {
        return;
    }
    m_gainValues[static_cast<int>(b)] = gainDb;
}

// From Thetis setup.cs:23984-23990 [v2.10.3.13] — SetAdjust(Band, int, float).
void PaProfile::setAdjust(Band b, int stepIndex, float gainDb) {
    if (!inRange(b)) {
        return;
    }
    if (stepIndex < 0 || stepIndex >= kDriveSteps) {
        return;
    }
    m_gainAdjust[static_cast<int>(b)][stepIndex] = gainDb;
}

// From Thetis setup.cs:23964-23968 [v2.10.3.13] — SetMaxPower(Band, float).
void PaProfile::setMaxPower(Band b, float watts) {
    if (!inRange(b)) {
        return;
    }
    m_maxPower[static_cast<int>(b)] = watts;
}

// From Thetis setup.cs:23974-23978 [v2.10.3.13] — SetMaxPowerUse(Band, bool).
void PaProfile::setMaxPowerUse(Band b, bool use) {
    if (!inRange(b)) {
        return;
    }
    m_useMaxPower[static_cast<int>(b)] = use;
}

// ── copySettings ───────────────────────────────────────────────────────────
//
// From Thetis setup.cs:24004-24026 [v2.10.3.13] — CopySettings(PAProfile).
// Copies the per-band arrays; leaves name, model, isDefault on the target
// untouched.
void PaProfile::copySettings(const PaProfile& other) {
    m_gainValues   = other.m_gainValues;
    m_gainAdjust   = other.m_gainAdjust;
    m_maxPower     = other.m_maxPower;
    m_useMaxPower  = other.m_useMaxPower;
}

// ── resetGainDefaultsForModel ──────────────────────────────────────────────
//
// From Thetis setup.cs:24027-24046 [v2.10.3.13] — ResetGainDefaultsForModel:
//   _model = model;
//   float[] pa_default_gains = HardwareSpecific.DefaultPAGainsForBands(_model);
//   for (int n = 0; n < (int)Band.LAST; n++) {
//       for (int i = 0; i < 9; i++) _gainAdjust[n, i] = 0;
//       _maxPower[n] = 0; _bUseMaxPower[n] = false;
//       SetGainForBand((Band)n, pa_default_gains[n]);
//   }
//
// NereusSDR deviation: the per-model gain row comes from the free function
// `defaultPaGainsForBand(model, band)` (PaGainProfile.h, Phase 1A) rather
// than a flat float[] indexed by Band int. Behaviour-equivalent.
void PaProfile::resetGainDefaultsForModel(HPSDRModel m) {
    m_model = m;
    for (int n = 0; n < kBandCount; ++n) {
        for (int i = 0; i < kDriveSteps; ++i) {
            m_gainAdjust[n][i] = 0.0f;
        }
        m_maxPower[n] = 0.0f;
        m_useMaxPower[n] = false;

        const Band band = static_cast<Band>(n);
        setGainForBand(band, defaultPaGainsForBand(m, band));
    }
}

// ── calcDriveAdjust ────────────────────────────────────────────────────────
//
// From Thetis setup.cs:23923-23959 [v2.10.3.13] — calcDriveAdjust(Band, int).
// Byte-for-byte port of the 9-step lerp:
//   nLIndex = nDriveValue / 10;
//   if (nDriveValue % 10 == 0) {
//       if (nLIndex == 0 || nLIndex == 10) return 0;     // 0% / 100% → 0
//       return _gainAdjust[nBand, nLIndex - 1];          // exact step
//   } else {
//       if (nLIndex == 0)      { low = 0;                              high = adj[nBand,0]; }
//       else if (nLIndex == 9) { low = adj[nBand,8];                   high = 0;             }
//       else                   { low = adj[nBand,nLIndex-1];           high = adj[nBand,nLIndex]; }
//       float frac = (nDriveValue - nLIndex*10) / 10f;
//       return lerp(low, high, frac);
//   }
float PaProfile::calcDriveAdjust(Band b, int driveValue) const noexcept {
    const int nBand = static_cast<int>(b);
    const int nLIndex = driveValue / 10;

    if (driveValue % 10 == 0) {
        if (nLIndex == 0 || nLIndex == 10) {
            return 0.0f;
        }
        // exact
        return m_gainAdjust[nBand][nLIndex - 1];
    } else {
        float low;
        float high;

        if (nLIndex == 0) {
            low = 0.0f;
            high = m_gainAdjust[nBand][0];
        } else if (nLIndex == 9) {
            low = m_gainAdjust[nBand][8];
            high = 0.0f;
        } else {
            low = m_gainAdjust[nBand][nLIndex - 1];
            high = m_gainAdjust[nBand][nLIndex];
        }

        const float frac = static_cast<float>(driveValue - (nLIndex * 10)) / 10.0f;
        return lerp(low, high, frac);
    }
}

// From Thetis setup.cs:23960-23963 [v2.10.3.13] — lerp(a, b, frac).
float PaProfile::lerp(float a, float b, float frac) noexcept {
    return a + frac * (b - a);
}

// ── dataToString ───────────────────────────────────────────────────────────
//
// From Thetis setup.cs:23884-23913 [v2.10.3.13] — DataToString. NereusSDR
// uses the 14-band layout instead of Thetis's 42-band; total field count
// 171 (vs Thetis 423/507).
QString PaProfile::dataToString() const {
    QString out;
    out.reserve(1024);

    out += base64Encode(m_name);
    out += QLatin1Char('|');
    out += QString::number(static_cast<int>(m_model));
    out += QLatin1Char('|');
    out += m_isFactoryDefault ? QStringLiteral("True") : QStringLiteral("False");
    out += QLatin1Char('|');

    // gains
    for (int n = 0; n < kBandCount; ++n) {
        out += formatFloat(m_gainValues[n]);
        out += QLatin1Char('|');
    }

    // gain adjust (band-major: band 0 steps 0..8, band 1 steps 0..8, ...)
    for (int n = 0; n < kBandCount; ++n) {
        for (int i = 0; i < kDriveSteps; ++i) {
            out += formatFloat(m_gainAdjust[n][i]);
            out += QLatin1Char('|');
        }
    }

    // max power (band-major: useFlag, value, useFlag, value, ...)
    for (int n = 0; n < kBandCount; ++n) {
        out += m_useMaxPower[n] ? QStringLiteral("True") : QStringLiteral("False");
        out += QLatin1Char('|');
        out += formatFloat(m_maxPower[n]);
        out += QLatin1Char('|');
    }

    // Thetis trims the trailing '|'. Match that.
    if (out.endsWith(QLatin1Char('|'))) {
        out.chop(1);
    }
    return out;
}

// ── dataFromString ─────────────────────────────────────────────────────────
//
// From Thetis setup.cs:23835-23883 [v2.10.3.13] — DataFromString. NereusSDR
// uses the 14-band layout exclusively; we accept exactly kSerializedFieldCount
// (171) fields. Thetis tolerates 45 / 423 / 507-field inputs as legacy
// versions; that backward-compat path is intentionally NOT ported (a fresh
// 0.3.2 install regenerates factory profiles via PaProfileManager).
bool PaProfile::dataFromString(const QString& data) {
    if (data.isEmpty()) {
        return false;
    }

    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() != kSerializedFieldCount) {
        return false;
    }

    // [0] base64-encoded name. Thetis does a round-trip check
    // (base64Encode(base64Decode(s)) == s) to detect non-base64 strings;
    // we follow the same pattern — if the string isn't valid base64, treat
    // it as the literal name (legacy un-encoded format).
    {
        const QString candidate = base64Decode(parts[0]);
        if (!candidate.isEmpty() && base64Encode(candidate) == parts[0]) {
            m_name = candidate;
        } else {
            m_name = parts[0];
        }
    }

    // [1] model int.
    {
        bool ok = false;
        const int modelInt = parts[1].toInt(&ok);
        if (ok) {
            m_model = static_cast<HPSDRModel>(modelInt);
        }
    }

    // [2] default bool. Thetis bool.Parse accepts "True" / "False"
    // case-insensitively.
    m_isFactoryDefault = (parts[2].compare(QStringLiteral("True"),
                                           Qt::CaseInsensitive) == 0);

    // [3..16] gain values.
    for (int n = 0; n < kBandCount; ++n) {
        bool ok = false;
        const float v = parts[n + 3].toFloat(&ok);
        if (ok) {
            m_gainValues[n] = v;
        }
    }

    // [17..142] gain adjust matrix.
    int idx = 0;
    constexpr int kAdjustStart = 3 + kBandCount;       // 17
    for (int n = 0; n < kBandCount; ++n) {
        for (int i = 0; i < kDriveSteps; ++i) {
            bool ok = false;
            const float v = parts[idx + kAdjustStart].toFloat(&ok);
            if (ok) {
                m_gainAdjust[n][i] = v;
            }
            ++idx;
        }
    }

    // [143..170] max-power column (useFlag, value pairs).
    idx = 0;
    constexpr int kMaxPowerStart =
        3 + kBandCount + kBandCount * kDriveSteps;     // 143
    for (int n = 0; n < kBandCount; ++n) {
        m_useMaxPower[n] = (parts[idx + kMaxPowerStart]
                                .compare(QStringLiteral("True"),
                                         Qt::CaseInsensitive) == 0);
        bool ok = false;
        const float v = parts[idx + kMaxPowerStart + 1].toFloat(&ok);
        if (ok) {
            m_maxPower[n] = v;
        }
        idx += 2;
    }

    return true;
}

}  // namespace NereusSDR
