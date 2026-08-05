// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - CtyDatParser: AD1C / K1EA cty.dat country-file parser.
//
// Ported from AetherSDR src/core/CtyDatParser.h [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task C1. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". DxccEntity
//                                    fields (primaryPrefix, name,
//                                    continent, cqZone, ituZone) and
//                                    the public surface (loadFromFile,
//                                    loadFromResource,
//                                    resolvePrimaryPrefix,
//                                    entityByPrefix, entityCount,
//                                    isLoaded) follow upstream
//                                    byte-for-byte. AI tooling:
//                                    Anthropic Claude Code.

#pragma once

#include <QString>
#include <QHash>
#include <QSet>

namespace NereusSDR {

// From AetherSDR src/core/CtyDatParser.h:9-15 [@0cd4559]
struct DxccEntity {
    QString primaryPrefix;   // e.g. "G"
    QString name;            // e.g. "England"
    QString continent;       // e.g. "EU"
    int     cqZone{0};
    int     ituZone{0};
};

// From AetherSDR src/core/CtyDatParser.h:17-49 [@0cd4559]
//
// ---------------------------------------------------------------------------
// CtyDatParser
//
// Parses the AD1C cty.dat file and resolves callsigns to DXCC entities via
// longest-prefix matching.  Load once at startup; queries are O(prefix_len).
// ---------------------------------------------------------------------------
class CtyDatParser {
public:
    // Returns true on success.
    bool loadFromFile(const QString& path);
    bool loadFromResource(const QString& resourcePath);   // e.g. ":/cty.dat"

    // Resolve callsign -> primary prefix of matched entity ("G", "VK", …)
    // Returns empty string if no match.
    QString resolvePrimaryPrefix(const QString& callsign) const;

    // Look up entity details by primary prefix.
    const DxccEntity* entityByPrefix(const QString& primaryPrefix) const;

    int entityCount() const { return m_entityByPrefix.size(); }
    bool isLoaded()   const { return !m_entityByPrefix.isEmpty(); }

private:
    void parse(const QStringList& lines);

    // exact-match table:  "=VK9XX"  -> primaryPrefix
    QHash<QString, QString> m_exactMatch;
    // prefix table: "VK9X" -> primaryPrefix
    QHash<QString, QString> m_prefixTable;
    // entity details keyed by primaryPrefix
    QHash<QString, DxccEntity> m_entityByPrefix;
    int m_maxPrefixLen{0};
};

} // namespace NereusSDR
