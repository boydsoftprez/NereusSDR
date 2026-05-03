// =================================================================
// src/core/PaCalProfile.cpp  (NereusSDR)
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
//                 Claude Code.
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

#include "PaCalProfile.h"

namespace NereusSDR {

// --- HPSDRModel → PaCalBoardClass mapping --------------------------------
//
// Every HPSDRModel value gets an explicit case so the compiler will warn
// (and CI will fail with `-Wswitch`) when a new SKU is added to HpsdrModel.h
// without a corresponding cal-class assignment here.
//
// Mapping derived from Thetis console.cs:6717-6752 CalibratedPAPower switch.

PaCalBoardClass paCalBoardClassFor(HPSDRModel model) noexcept {
    // Switch ordering mirrors the Thetis console.cs:6717-6752 switch so the
    // verbatim inline tags `//G8NJJ` (ANAN_G2, ANAN_G2_1K) and `//DH1KLM`
    // (REDPITAYA) sit in the same physical neighbourhood as the cite that
    // pulls them in (verify-inline-tag-preservation.py uses a ±10-line
    // window around each `// From Thetis` cite).
    switch (model) {
        // Atlas / HPSDR kit — discrete-board family with no integrated PA.
        // FWD power meter has no signal, so calibration is meaningless.
        // NereusSDR-internal: Thetis would route this to default 10 W
        // (no `case HPSDRModel.HPSDR` exists in console.cs:6717-6752).
        case HPSDRModel::HPSDR:        return PaCalBoardClass::None;

        // 100 W class — explicit cases in Thetis switch (10 W intervals).
        // From Thetis console.cs:6720-6741 [v2.10.3.13]:
        //   case HPSDRModel.ANAN100D:                       // explicit predetermined cal table at console.cs:6720
        //   case HPSDRModel.ANAN7000D:                      //
        //   case HPSDRModel.ANVELINAPRO3:                   //
        //   case HPSDRModel.ANAN_G2:                        // G8NJJ
        //   case HPSDRModel.ANAN_G2_1K:                     // G8NJJ
        //   case HPSDRModel.REDPITAYA:                      //DH1KLM
        //                              interval = 10.0f;
        case HPSDRModel::ANAN100D:
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANVELINAPRO3:
        case HPSDRModel::ANAN_G2:      //G8NJJ
        case HPSDRModel::ANAN_G2_1K:   //G8NJJ
        case HPSDRModel::REDPITAYA:    //DH1KLM
            return PaCalBoardClass::Anan100;

        // ANAN-8000DLE — 20 W intervals.
        // From Thetis console.cs:6742-6744 [v2.10.3.13] —
        //   case HPSDRModel.ANAN8000D: interval = 20.0f;
        // Inline tags from neighbouring lines (preserved so the
        // verify-inline-tag-preservation.py ±10-line window hits them):
        //   :6737 ANAN_G2 //G8NJJ
        //   :6738 ANAN_G2_1K //G8NJJ
        //   :6739 REDPITAYA //DH1KLM
        case HPSDRModel::ANAN8000D:    return PaCalBoardClass::Anan8000;

        // ANAN-10 / ANAN-10E — 1 W intervals.
        // From Thetis console.cs:6745-6748 [v2.10.3.13] —
        //   case HPSDRModel.ANAN10:
        //   case HPSDRModel.ANAN10E:  interval = 1.0f;
        case HPSDRModel::ANAN10:
        case HPSDRModel::ANAN10E:      return PaCalBoardClass::Anan10;

        // 100 W class — Thetis falls these through `default: interval = 10.0f`.
        // From Thetis console.cs:6749-6751 [v2.10.3.13] —
        //   default: interval = 10.0f; break;
        // The full set of HPSDRModel values that hit `default` is: HERMES,
        // ANAN100, ANAN100B, ANAN200D, ORIONMKII (everything not explicitly
        // listed in the switch and not ANAN10/10E/8000D).
        case HPSDRModel::HERMES:
        case HPSDRModel::ANAN100:
        case HPSDRModel::ANAN100B:
        case HPSDRModel::ANAN200D:
        case HPSDRModel::ORIONMKII:
            return PaCalBoardClass::Anan100;

        // Hermes Lite 2 — grouped with ANAN10/ANAN10E for PA cal.
        // From mi0bot setup.cs:5463-5466 [v2.10.3.13-beta2] — HL2 uses the
        // same `ud10PA1W..ud10PA10W` spinbox set, same 1 W intervals, same
        // 10 W max as ANAN10/ANAN10E:
        //
        //     case HPSDRModel.HERMESLITE:     // MI0BOT: HL2
        //     case HPSDRModel.ANAN10:
        //     case HPSDRModel.ANAN10E:
        //         rv = (float)ud10PA1W.Value;
        //
        // Confirmed by mi0bot setup.cs:6432-6435 [v2.10.3.13-beta2] which
        // does `grp10WattMeterTrim.BringToFront()` on the HL2 branch (i.e.
        // the 10 W meter trim group, not the 100 W). An earlier NereusSDR
        // placeholder used a separate `HermesLite` class (0.5 W intervals
        // / 5 W max) added by Task 3.1; dropped 2026-05-02 after upstream
        // verification.
        case HPSDRModel::HERMESLITE:   //MI0BOT
            return PaCalBoardClass::Anan10;

        // Sentinels — `FIRST` (-1) and `LAST` (16) are not real models.
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:
            return PaCalBoardClass::None;
    }
    // Defensive — unreachable because every enumerator above is handled
    // explicitly. Kept so a compiler that fails to prove exhaustiveness
    // (e.g. casted-int values) still returns a well-defined value.
    return PaCalBoardClass::None;
}

// --- Factory defaults ----------------------------------------------------
//
// Each non-None class returns a profile whose `watts[i]` for i in 1..10
// equals `i * interval()`. This matches Thetis spinbox initial values:
//   - `ud10PA{1..10}W`   labelled 1..10 W; default Value = label
//   - `ud100PA{10..100}W` labelled 10..100 W; default Value = label
//   - `ud200PA{20..200}W` labelled 20..200 W; default Value = label
// Index 0 is always 0.0 (Thetis hard-codes `0.0f` in PAsets[0],
// console.cs:6705 [v2.10.3.13]).

PaCalProfile PaCalProfile::defaults(PaCalBoardClass cls) noexcept {
    PaCalProfile p;
    p.boardClass = cls;
    switch (cls) {
        case PaCalBoardClass::Anan10:
            p.watts = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f,
                       6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
            break;
        case PaCalBoardClass::Anan100:
            p.watts = {0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f,
                       60.0f, 70.0f, 80.0f, 90.0f, 100.0f};
            break;
        case PaCalBoardClass::Anan8000:
            p.watts = {0.0f, 20.0f, 40.0f, 60.0f, 80.0f, 100.0f,
                       120.0f, 140.0f, 160.0f, 180.0f, 200.0f};
            break;
        case PaCalBoardClass::None:
            // value-initialized to all zeros above.
            break;
    }
    return p;
}

// --- Per-class interval --------------------------------------------------

float PaCalProfile::interval() const noexcept {
    switch (boardClass) {
        case PaCalBoardClass::Anan10:     return 1.0f;
        case PaCalBoardClass::Anan100:    return 10.0f;
        case PaCalBoardClass::Anan8000:   return 20.0f;
        case PaCalBoardClass::None:       return 0.0f;
    }
    return 0.0f;
}

// --- Linear interpolation ------------------------------------------------
//
// From Thetis console.cs:6756-6768 [v2.10.3.13] — PowerKernel (verbatim
// algorithm; variable names preserved where they don't conflict with C++):
//
//   private float PowerKernel(float watts, float interval, int entries, float[] table)
//   {
//       int idx = 0;
//       if (watts > table[entries - 1])
//           idx = entries - 2;
//       else
//       {
//           while (watts > table[idx]) idx++;
//           if (idx > 0) idx--;
//       }
//       float frac = (watts - table[idx]) / (table[idx + 1] - table[idx]);
//       return interval * ((1.0f - frac) * (float)idx + frac * (float)(idx + 1));
//   }
//
// The mathematics: `interval * ((1-frac)*idx + frac*(idx+1))` simplifies to
// `interval * (idx + frac)`, i.e. the fractional segment-index times interval.
// With identity-cal tables this returns rawWatts unchanged; with user-edited
// tables it returns the calibrated real-world watts (the spinbox label that
// the raw reading falls between, weighted by where it falls).

float PaCalProfile::interpolate(float rawWatts) const noexcept {
    // Boards with no PA short-circuit to identity — no calibration to apply.
    if (boardClass == PaCalBoardClass::None) {
        return rawWatts;
    }

    constexpr int entries = 11;  // From Thetis console.cs:6703 [v2.10.3.13] — `const int entries = 11`
    const float ivl = interval();

    int idx = 0;
    if (rawWatts > watts[entries - 1]) {
        idx = entries - 2;
    } else {
        while (rawWatts > watts[idx]) {
            idx++;
        }
        if (idx > 0) {
            idx--;
        }
    }

    // Guard against divide-by-zero in the (pathological) case where two
    // adjacent cal points are equal — Thetis doesn't guard, but the C#
    // float division would yield Inf or NaN. Returning rawWatts is the
    // graceful fallback (identity).
    const float span = watts[idx + 1] - watts[idx];
    if (span == 0.0f) {
        return rawWatts;
    }

    const float frac = (rawWatts - watts[idx]) / span;
    return ivl * ((1.0f - frac) * static_cast<float>(idx)
                  + frac * static_cast<float>(idx + 1));
}

}  // namespace NereusSDR
