// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - DxccWorkedStatus: per-entity / per-band / per-modeGroup
// worked-status tracker fed from AdifParser output.
//
// Ported from AetherSDR src/core/DxccWorkedStatus.cpp [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task C3. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". load()
//                                    iterates the input QVector and
//                                    inserts (primaryPrefix, band,
//                                    modeGroup) into the nested
//                                    QHash, skipping rows where any
//                                    of the three fields is empty;
//                                    query() does an empty-prefix
//                                    early-out to Unknown then
//                                    walks entity / band / mode
//                                    in order returning NewDxcc /
//                                    NewBand / NewMode / Worked.
//                                    Behaviour preserved verbatim
//                                    (skip-on-empty gate + early
//                                    Unknown branch). AI tooling:
//                                    Anthropic Claude Code.

#include "DxccWorkedStatus.h"
#include "AdifParser.h"

namespace NereusSDR {

// From AetherSDR src/core/DxccWorkedStatus.cpp:6-16 [@0cd4559]
void DxccWorkedStatus::load(const QVector<QsoRecord>& records)
{
    m_worked.clear();
    m_totalQsos = 0;
    for (const auto& r : records) {
        if (r.dxccPrefix.isEmpty() || r.band.isEmpty() || r.modeGroup.isEmpty())
            continue;
        m_worked[r.dxccPrefix][r.band].insert(r.modeGroup);
        ++m_totalQsos;
    }
}

// From AetherSDR src/core/DxccWorkedStatus.cpp:18-22 [@0cd4559]
void DxccWorkedStatus::clear()
{
    m_worked.clear();
    m_totalQsos = 0;
}

// From AetherSDR src/core/DxccWorkedStatus.cpp:24-42 [@0cd4559]
DxccStatus DxccWorkedStatus::query(const QString& primaryPrefix,
                                   const QString& band,
                                   const QString& modeGroup) const
{
    if (primaryPrefix.isEmpty()) return DxccStatus::Unknown;

    auto entityIt = m_worked.find(primaryPrefix);
    if (entityIt == m_worked.end())
        return DxccStatus::NewDxcc;

    auto bandIt = entityIt->find(band);
    if (bandIt == entityIt->end())
        return DxccStatus::NewBand;

    if (!bandIt->contains(modeGroup))
        return DxccStatus::NewMode;

    return DxccStatus::Worked;
}

} // namespace NereusSDR
