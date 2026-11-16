// =================================================================
// src/models/bandstack/BandStackSeeds.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/clsBandStackManager.cs [v2.10.3.15 @3759d09],
//   original licence from Thetis source is included below
//
// The band stack's factory rows for each region. See BandStackSeeds.cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

// --- From clsBandStackManager.cs ---
/*  clsBandStackManager.cs

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

#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

#include <optional>

#include "core/safety/BandPlanGuard.h"
#include "models/Band.h"

namespace NereusSDR {

/// One factory band stack row. `mode` is Thetis's mode name, as written in
/// its tables. `filterNumber` is the N of Thetis's "FN"; the row resolves to
/// low and high Hz through the per-mode preset table when the seeds load.
struct SeedRow {
    Band band;
    QString mode;
    int filterNumber;
    qint64 frequencyHz;
    bool locked;
};

/// The rows for `region`, in Thetis's order: the region's own routine, then
/// the shortwave list that every region gets.
QList<SeedRow> seedsForRegion(safety::Region region);

/// The region as General Options names it, which is also the name the band
/// stack document records.
QString regionName(safety::Region region);

/// The band a Thetis band string in the seed tables belongs to, or nullopt
/// for a string with no mapping. Deliberately not `bandFromName`, whose
/// fallback is GEN: a row must never land on GEN by accident.
std::optional<Band> seedBandFromName(const QString& name);

} // namespace NereusSDR
