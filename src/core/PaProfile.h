// =================================================================
// src/core/PaProfile.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs:23763-24046 (PAProfile class —
//     editable per-instance per-band PA gain table + 9-step drive-adjust
//     matrix + per-band max-power column + lerp-based gain compensation
//     used by SetPowerUsingTargetDBM)
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Phase 2 Agent 2A of issue #167 PA-cal
//                 safety hotfix. Editable per-instance gain profile
//                 backing the dBm-target math in TransmitModel
//                 (Phase 3) and the live editor in PaGainByBandPage
//                 (Phase 6). Constructor pulls factory rows from
//                 PaGainProfile (Phase 1A). NereusSDR-canonical 14-band
//                 layout (Thetis used Band.LAST = 42; NereusSDR has 14
//                 PA-relevant bands — 11 HF + 6m + GEN/WWV/XVTR).
//   2026-05-03 — Phase 3 Agent 3B of issue #167: getGainForBand()
//                 out-of-range return restored to Thetis-faithful
//                 1000.0f sentinel (was 0.0f Phase 2A deviation —
//                 see PaProfile.cpp note for safety rationale).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
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

#pragma once

#include <QString>

#include <array>

#include "HpsdrModel.h"
#include "models/Band.h"

namespace NereusSDR {

/// Editable per-instance per-band PA gain profile.
///
/// Faithful port of Thetis `PAProfile` (setup.cs:23763-24046 [v2.10.3.13]).
/// Each instance holds a user-customisable gain table seeded from the
/// per-model factory row in `PaGainProfile` (Phase 1A). The dBm-target
/// math (`TransmitModel::computeAudioVolume`, Phase 3) consults
/// `getGainForBand(band, sliderWatts)` to compute the audio volume that
/// corresponds to a requested watts target — which is how the K2GX
/// over-power scenario (>300 W on a 200 W radio) gets fixed: the
/// per-band PA gain table eats the dB headroom that the high-gain ANAN-
/// 8000DLE PA otherwise turns into too-much-output.
///
/// NereusSDR uses a 14-band window (vs Thetis's 42). Indices 0..13 cover
/// `Band::Band160m` through `Band::XVTR`; SWL bands (`Band::Band120m` and
/// later) are silently no-op'd by setters and return safe defaults from
/// getters. This mirrors Thetis's `(int)b > Band.FIRST && (int)b < Band.LAST`
/// guard pattern at the boundary of its larger band enum.
///
/// Serialization is NereusSDR-canonical 171-field pipe-delimited format
/// (NOT byte-compatible with Thetis 423/507-field format). One-way Thetis
/// import is out of scope for this hotfix.
class PaProfile {
public:
    /// Number of NereusSDR bands tracked by PaProfile (160m..6m + GEN/WWV/XVTR).
    /// SWL bands are outside this window — see `Band` enum (`models/Band.h`).
    static constexpr int kBandCount  = 14;

    /// Drive-step bin count. Each band carries a 9-entry gain-adjust array
    /// covering drive percentages 10/20/.../90 (Thetis setup.cs:23792
    /// — `_gainAdjust = new float[Band.LAST, 9]`). Drive 0 and 100
    /// short-circuit to 0 in `calcDriveAdjust`.
    static constexpr int kDriveSteps = 9;

    /// Total field count emitted by `dataToString`:
    ///   1 (base64 name) + 1 (model int) + 1 (default bool)
    ///   + kBandCount (gains)
    ///   + kBandCount * kDriveSteps (adjusts)
    ///   + kBandCount * 2 (max-power use + value pair)
    /// = 3 + 14 + 126 + 28 = 171.
    static constexpr int kSerializedFieldCount =
        3 + kBandCount + kBandCount * kDriveSteps + kBandCount * 2;

    /// Empty default constructor — name "", model HPSDRModel::FIRST,
    /// non-default flag, gain table all-100.0f sentinel, adjusts and
    /// max-power all zero. Useful for collection slots that get filled
    /// in later by `dataFromString`. Mirrors the setup.cs:23797-23807 init
    /// loop (every band's `_gainValues[n] = 100`, all adjusts zero, no
    /// max-power use).
    PaProfile();

    /// Construct a profile and immediately seed factory defaults from
    /// `defaultPaGainsForBand(model, band)` for every band. Faithful port of
    /// Thetis setup.cs:23786-23810 [v2.10.3.13] — the only constructor in
    /// the original class.
    PaProfile(QString name, HPSDRModel model, bool isFactoryDefault);

    // ── Accessors ────────────────────────────────────────────────────────
    QString    name() const noexcept             { return m_name; }
    HPSDRModel model() const noexcept            { return m_model; }
    bool       isFactoryDefault() const noexcept { return m_isFactoryDefault; }

    /// Per-band PA gain in dB.
    ///
    /// `driveValue == -1` (Thetis sentinel) returns the base table value
    /// only — no drive-step compensation. `driveValue` in [0, 100] applies
    /// `calcDriveAdjust(b, driveValue)` to subtract the corresponding
    /// 9-step lerp value from the base.
    ///
    /// Out-of-range Band (i.e. SWL slots `Band::Band120m`+ or any raw int
    /// cast outside [0, kBandCount)) returns the 1000.0f Thetis sentinel
    /// (setup.cs:23866 [v2.10.3.13]).  `TransmitModel::computeAudioVolume`
    /// (Phase 3B) consults `gbb >= 99.5` to short-circuit to its linear
    /// fallback, so the sentinel feeds the safety net automatically.
    float getGainForBand(Band b, int driveValue = -1) const noexcept;

    /// Read the per-band drive-step adjust matrix at `[b, stepIndex]`.
    /// `stepIndex` ranges 0..8 (covering drive percentages 10..90 in the
    /// 9-step lerp). Out-of-range Band or stepIndex returns 0.0f.
    float getAdjust(Band b, int stepIndex) const noexcept;

    /// Per-band max-power ceiling in watts. Returns 0.0f for out-of-range
    /// Band — matches Thetis setup.cs:23969-23973.
    float getMaxPower(Band b) const noexcept;

    /// Whether the per-band max-power ceiling is enforced. Returns false
    /// for out-of-range Band — matches Thetis setup.cs:23979-23983.
    bool  getMaxPowerUse(Band b) const noexcept;

    // ── Mutators (silently no-op out-of-range, matching Thetis) ──────────
    void setGainForBand(Band b, float gainDb);
    void setAdjust(Band b, int stepIndex, float gainDb);
    void setMaxPower(Band b, float watts);
    void setMaxPowerUse(Band b, bool use);

    /// Deep copy gain values + drive adjusts + max-power columns from
    /// `other`. Does NOT change `name`, `model`, or `isFactoryDefault`
    /// — matches Thetis CopySettings (setup.cs:24004-24026 [v2.10.3.13]).
    void copySettings(const PaProfile& other);

    /// Re-seed the gain table from `defaultPaGainsForBand(m, band)` for
    /// every band, zero the drive adjusts, and clear the max-power use
    /// flags. Updates `m_model` to `m`. Faithful port of Thetis
    /// `ResetGainDefaultsForModel` (setup.cs:24027-24046 [v2.10.3.13]).
    void resetGainDefaultsForModel(HPSDRModel m);

    /// Serialize to NereusSDR-canonical 171-field pipe-delimited string.
    /// Layout:
    ///   [0]      base64-encoded name
    ///   [1]      model int (HPSDRModel value)
    ///   [2]      isFactoryDefault bool ("True" / "False")
    ///   [3..16]  gain values for bands 0..13
    ///   [17..142] drive-step adjusts (band-major: band 0 steps 0..8,
    ///            band 1 steps 0..8, ...)
    ///   [143..170] max-power column (band-major: useFlag, value, useFlag,
    ///              value, ...)
    /// Mirrors Thetis DataToString pattern (setup.cs:23884-23913
    /// [v2.10.3.13]) but uses NereusSDR's 14-band layout.
    QString dataToString() const;

    /// Deserialize from a `dataToString` blob. Returns true on success.
    /// Returns false on:
    ///   - empty string
    ///   - field count != kSerializedFieldCount (171)
    /// Permissive on individual field parse failures: if a numeric field
    /// fails to parse, it's left at its default value and parsing
    /// continues. Matches Thetis tolerance to malformed numeric fields
    /// (setup.cs DataFromString silently swallows parse errors).
    bool dataFromString(const QString& data);

private:
    /// 9-step drive-adjust lerp. Faithful port of Thetis calcDriveAdjust
    /// (setup.cs:23923-23959 [v2.10.3.13]).
    float calcDriveAdjust(Band b, int driveValue) const noexcept;

    /// Linear interpolation. Thetis setup.cs:23960-23963 [v2.10.3.13]:
    ///   return a + frac * (b - a);
    static float lerp(float a, float b, float frac) noexcept;

    /// True iff `b` is within the 14-band PA window (i.e. its enum int
    /// is in [0, kBandCount)).
    static bool inRange(Band b) noexcept;

    QString    m_name;
    HPSDRModel m_model;
    bool       m_isFactoryDefault;

    std::array<float, kBandCount>                          m_gainValues;
    std::array<std::array<float, kDriveSteps>, kBandCount> m_gainAdjust;
    std::array<float, kBandCount>                          m_maxPower;
    std::array<bool,  kBandCount>                          m_useMaxPower;
};

}  // namespace NereusSDR
