// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - DxccWorkedStatus: per-entity / per-band / per-modeGroup
// worked-status tracker fed from AdifParser output.
//
// Ported from AetherSDR src/core/DxccWorkedStatus.h [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task C3. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". DxccStatus
//                                    enum (NewDxcc / NewBand /
//                                    NewMode / Worked / Unknown) and
//                                    the public surface (load,
//                                    clear, query, entityCount,
//                                    totalQsos) follow upstream
//                                    byte-for-byte. Internal
//                                    QHash<QString, QHash<QString,
//                                    QSet<QString>>> data layout
//                                    keyed by primaryPrefix -> band
//                                    -> set<modeGroup> preserved
//                                    verbatim. Forward declaration of
//                                    QsoRecord matches upstream;
//                                    AdifParser.h supplies the type
//                                    when the .cpp includes it.
//                                    AI tooling: Anthropic Claude
//                                    Code.

#pragma once

#include <QString>
#include <QHash>
#include <QSet>
#include <QVector>

namespace NereusSDR {

struct QsoRecord;

// From AetherSDR src/core/DxccWorkedStatus.h:12-18 [@0cd4559]
enum class DxccStatus {
    NewDxcc,   // entity never worked
    NewBand,   // entity worked but not on this band
    NewMode,   // entity worked on this band but not this mode group
    Worked,    // already worked on this band + mode group
    Unknown,   // DXCC not resolved (no cty.dat match)
};

// From AetherSDR src/core/DxccWorkedStatus.h:20-44 [@0cd4559]
//
// ---------------------------------------------------------------------------
// DxccWorkedStatus
//
// Fast O(1) lookup of worked status from a loaded ADIF log.
// Data: QHash<primaryPrefix, QHash<band, QSet<modeGroup>>>
// ---------------------------------------------------------------------------
class DxccWorkedStatus {
public:
    void load(const QVector<QsoRecord>& records);
    void clear();

    // Query worked status given the resolved DXCC primary prefix, band, and
    // normalised mode group (CW / PHONE / DATA).
    DxccStatus query(const QString& primaryPrefix,
                     const QString& band,
                     const QString& modeGroup) const;

    int entityCount() const { return m_worked.size(); }
    int totalQsos()   const { return m_totalQsos; }

private:
    // primaryPrefix -> band -> set<modeGroup>
    QHash<QString, QHash<QString, QSet<QString>>> m_worked;
    int m_totalQsos{0};
};

} // namespace NereusSDR
