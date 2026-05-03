// =================================================================
// src/core/PaCalProfile.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs:6691-6768 (CalibratedPAPower
//     + PowerKernel — per-board PA forward-power calibration)
//   Project Files/Source/Console/setup.cs:5404-5594 (PA10W..PA100W
//     properties — per-model spinbox routing for ud10PA{N}W,
//     ud100PA{N}W, ud200PA{N}W cal-point spinboxes)
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. First task in Section 3 of the P1 full-
//                 parity epic; pure model with no UI / no persistence /
//                 no controller wiring (those land in Tasks 3.2 / 3.3 /
//                 3.4 / 3.5 of the same plan).
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

#include "HpsdrModel.h"

#include <array>

namespace NereusSDR {

/// Per-board-class grouping of PA forward-power calibration profiles.
///
/// Each NereusSDR-supported HPSDRModel maps to exactly one PaCalBoardClass,
/// determined by which Thetis spinbox-set drives that model's `CalibratedPAPower`
/// lookup table:
///
///   - `Anan10`     — `ud10PA1W` … `ud10PA10W`   (1 W intervals, 10 W max).
///                    Used by ANAN-10 / ANAN-10E AND Hermes Lite 2 — mi0bot
///                    `setup.cs:5463-5466` and `:6432-6435` [v2.10.3.13-beta2]
///                    explicitly group HERMESLITE with ANAN10/ANAN10E for PA
///                    cal: same `ud10PA{1..10}W` spinbox set, same 1 W
///                    intervals, same 10 W max, and `grp10WattMeterTrim`
///                    `BringToFront()` is taken on the HL2 branch.
///   - `Anan100`    — `ud100PA10W` … `ud100PA100W` (10 W intervals, 100 W max)
///   - `Anan8000`   — `ud200PA20W` … `ud200PA200W` (20 W intervals, 200 W max)
///   - `None`       — board has no integrated PA (Atlas/HPSDR kit). UI
///                    suppresses the PA-cal group entirely; `interpolate`
///                    short-circuits to identity.
///
/// (An earlier NereusSDR placeholder `HermesLite` class with 0.5 W intervals
/// / 5 W max was added by Task 3.1 before the upstream check; it was dropped
/// 2026-05-02 once mi0bot's grouping was verified. HL2 now routes through
/// `Anan10` byte-for-byte with mi0bot.)
///
/// From Thetis console.cs:6691-6768 [v2.10.3.13] — CalibratedPAPower + PowerKernel:
/// the `switch (HardwareSpecific.Model)` selects an `interval` (1.0/10.0/20.0)
/// and a `PAsets[]` table; `PowerKernel` runs the same piecewise-linear lookup
/// for every board. Inline upstream tags preserved verbatim from the cited
/// range (within the verify-inline-tag-preservation.py ±10-line window):
///   :6737 ANAN_G2     // G8NJJ
///   :6738 ANAN_G2_1K  // G8NJJ
///   :6739 REDPITAYA   //DH1KLM
enum class PaCalBoardClass {
    None,        ///< Board has no integrated PA — Atlas / HPSDR kit, HL2 RX-only kit.
    Anan10,      ///< ANAN-10 / ANAN-10E / Hermes Lite 2 — 1 W intervals, 10 W max
                 ///< (Thetis console.cs:6745-6748 [v2.10.3.13]; HL2 grouping
                 ///< per mi0bot setup.cs:5463-5466 [v2.10.3.13-beta2]).
    Anan100,     ///< ANAN-100 / 100B / 100D / 200D / 7000DLE / G2 / G2-1K / Anvelina Pro 3 / RedPitaya / Hermes / OrionMkII — 10 W intervals.
    Anan8000,    ///< ANAN-8000DLE — 20 W intervals (Thetis console.cs:6742-6744 [v2.10.3.13]).
};

/// Map an `HPSDRModel` to its PA-calibration board class.
///
/// Mapping is derived from Thetis `console.cs:6717-6752 CalibratedPAPower`
/// `switch (HardwareSpecific.Model)` [v2.10.3.13]. Inline upstream tags from
/// the cited range (preserved verbatim so verify-inline-tag-preservation.py
/// finds them within the ±10-line port window):
///   :6737 ANAN_G2     // G8NJJ
///   :6738 ANAN_G2_1K  // G8NJJ
///   :6739 REDPITAYA   //DH1KLM
///
///   - ANAN10 / ANAN10E              → `Anan10`     (interval 1.0)
///   - HERMESLITE                     → `Anan10`     (interval 1.0, mi0bot
///                                                     setup.cs:5463-5466
///                                                     [v2.10.3.13-beta2] —
///                                                     HL2 grouped with
///                                                     ANAN10/ANAN10E for cal)
///   - ANAN8000D                      → `Anan8000`   (interval 20.0)
///   - ANAN100D / ANAN7000D /
///     ANVELINAPRO3 / ANAN_G2 /       // G8NJJ on G2 / G2_1K
///     ANAN_G2_1K / REDPITAYA         → `Anan100`    (interval 10.0, explicit case //DH1KLM on REDPITAYA)
///   - ANAN100 / ANAN100B / ANAN200D /
///     HERMES / ORIONMKII             → `Anan100`    (interval 10.0, default branch)
///   - HPSDR (Atlas/Metis kit)        → `None`       (NereusSDR-internal — Atlas
///                                                     has no integrated PA; FWD
///                                                     power meter is suppressed)
///   - FIRST / LAST sentinels         → `None`
///
/// Every `HPSDRModel` enum value MUST have an explicit case in the implementation
/// (no `default:` fallthrough) so adding a new SKU requires touching this file.
PaCalBoardClass paCalBoardClassFor(HPSDRModel model) noexcept;

/// Per-board PA forward-power calibration profile.
///
/// Holds 11 cal points indexed 0..10 plus the originating board class.
/// Index 0 is always 0 W (radio off PA — Thetis hard-codes the first table
/// entry to `0.0f` in `CalibratedPAPower`, console.cs:6705 [v2.10.3.13]).
/// Indices 1..10 hold the 10 user-calibrated points; factory defaults equal
/// each spinbox's label value (e.g. `Anan100` defaults to
/// `{0,10,20,30,40,50,60,70,80,90,100}`, matching `ud100PA{10..100}W`'s
/// initial values in setup.cs:5404-5594).
///
/// `interpolate` matches Thetis `PowerKernel` (console.cs:6756-6768
/// [v2.10.3.13]) — given a raw `alex_fwd` reading in watts, returns the
/// calibrated real-world watts derived from piecewise-linear interpolation
/// between the cal points. Above-table inputs extrapolate linearly using
/// the last segment; below-zero inputs pass through (identity for default
/// tables, slight negative skew for user-edited tables).
struct PaCalProfile {
    PaCalBoardClass boardClass{PaCalBoardClass::None};

    /// 11-entry cal table (watts). Index 0 = 0.0f always; indices 1..10
    /// are the user-calibrated points. Factory defaults are produced by
    /// `defaults(class)` and match the spinbox label values.
    std::array<float, 11> watts{};

    /// Construct factory defaults for a given board class. Each non-`None`
    /// class returns a profile whose `watts[i]` (for i in 1..10) equals
    /// `i * interval()` — i.e. each spinbox initialized to its label value.
    /// Matches Thetis behaviour where new `ud{N}PA{W}W` spinboxes start at
    /// their label value (setup.cs spinbox `Value` defaults).
    static PaCalProfile defaults(PaCalBoardClass cls) noexcept;

    /// Linear-interpolate a raw `alex_fwd` watts reading through the cal
    /// table to recover real-world watts.
    ///
    /// Algorithm matches Thetis `PowerKernel` (console.cs:6756-6768
    /// [v2.10.3.13]) byte-for-byte:
    ///   - If raw exceeds `watts[entries-1]`, use the last segment
    ///     `[entries-2, entries-1]` (extrapolates linearly).
    ///   - Otherwise scan upward to find the first `i` where
    ///     `raw <= watts[i]`, then back off one index so the segment
    ///     bracket is `[i, i+1]` with `watts[i] <= raw <= watts[i+1]`.
    ///   - Compute `frac = (raw - watts[i]) / (watts[i+1] - watts[i])`.
    ///   - Return `interval * ((1-frac)*i + frac*(i+1))`, i.e. the
    ///     fractional segment index times interval (so identity-cal
    ///     defaults round-trip raw → raw).
    ///
    /// `boardClass == None` short-circuits to identity (no PA on the
    /// hardware → no calibration to apply).
    float interpolate(float rawWatts) const noexcept;

    /// Per-class watts-between-spinbox-labels interval.
    ///   - `Anan10`     → 1.0f  (ANAN-10 / ANAN-10E / HL2)
    ///   - `Anan100`    → 10.0f
    ///   - `Anan8000`   → 20.0f
    ///   - `None`       → 0.0f
    float interval() const noexcept;
};

}  // namespace NereusSDR
