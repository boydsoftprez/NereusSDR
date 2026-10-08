// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - CtyDatParser: AD1C / K1EA cty.dat country-file parser.
//
// Ported from AetherSDR src/core/CtyDatParser.cpp [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task C1. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". Public API
//                                    (loadFromFile, loadFromResource,
//                                    resolvePrimaryPrefix,
//                                    entityByPrefix) and internal
//                                    helpers (cleanPrefix,
//                                    headerRe regex, longest-prefix
//                                    iteration, /P /M /MM /AM /QRP
//                                    portable-suffix handling,
//                                    /country prefix-override
//                                    fallback) follow upstream
//                                    byte-for-byte. AI tooling:
//                                    Anthropic Claude Code.
//   2026-10-07  J.J. Boyd / KG4VCF  Rotor control, bearings: the header
//                                    regex captures latitude and
//                                    longitude (longitude + west turned
//                                    to + east); cleanPrefix also strips
//                                    the <lat/long>, {continent} and
//                                    ~offset~ overrides; <lat/long> kept
//                                    per alias; resolvePrimaryPrefix's
//                                    body moves to resolveMatch so the
//                                    matched alias's override is known;
//                                    positionForCallsign. AI tooling:
//                                    Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Final review fixes: the longitude
//                                    sign comments say what is sourced
//                                    and what is assumed. Behaviour
//                                    unchanged. AI tooling: Anthropic
//                                    Claude Code.

#include "CtyDatParser.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>

namespace NereusSDR {

// From AetherSDR src/core/CtyDatParser.cpp:10-23 [@0cd4559]
//
// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Strip zone overrides "(xx)" and "[xx]" and leading "=" from a prefix token.
static QString cleanPrefix(const QString& raw)
{
    QString s = raw.trimmed();
    // remove zone overrides like (14) and [14]
    // NereusSDR: also <lat/long>, {continent} and ~offset~, the other
    // alias overrides in the cty.dat format
    // (https://www.country-files.com/cty-dat-format/).
    static const QRegularExpression zoneRe(
        R"(\([^)]*\)|\[[^\]]*\]|<[^>]*>|\{[^}]*\}|~[^~]*~)");
    s.remove(zoneRe);
    s = s.trimmed();
    return s;
}

// NereusSDR: the <lat/long> override on an alias token, if any. Its sign
// is unsourced: the format page names the override but not its sign, and
// no cty.dat to hand (ours, or those of Thetis, AetherSDR and Longpath)
// carries one; their parsers drop overrides. It is read as + west to
// match the header columns, an assumption until confirmed; returned + east.
static std::optional<GeoPosition> latLongOverride(const QString& raw)
{
    static const QRegularExpression overrideRe(
        R"(<\s*([+-]?\d+(?:\.\d+)?)\s*/\s*([+-]?\d+(?:\.\d+)?)\s*>)");
    const auto m = overrideRe.match(raw);
    if (!m.hasMatch()) {
        return std::nullopt;
    }
    return GeoPosition{m.captured(1).toDouble(), -m.captured(2).toDouble()};
}

// From AetherSDR src/core/CtyDatParser.cpp:25-53 [@0cd4559]
//
// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool CtyDatParser::loadFromFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream ts(&f);
    QStringList lines;
    while (!ts.atEnd())
        lines.append(ts.readLine());
    parse(lines);
    return isLoaded();
}

bool CtyDatParser::loadFromResource(const QString& resourcePath)
{
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream ts(&f);
    QStringList lines;
    while (!ts.atEnd())
        lines.append(ts.readLine());
    parse(lines);
    return isLoaded();
}

// From AetherSDR src/core/CtyDatParser.cpp:55-149 [@0cd4559]
//
// ---------------------------------------------------------------------------
// cty.dat parser
//
// Format:
//   Entity Name:  CQ:  ITU:  Continent:  lat:  lon:  tz:  PrimaryPrefix:
//       alias1,alias2,=EXACT1,=EXACT2;
//
// Alias tokens may have zone overrides in () or [] which we strip.
// Tokens starting with '=' are exact callsign matches.
// ---------------------------------------------------------------------------
void CtyDatParser::parse(const QStringList& lines)
{
    m_exactMatch.clear();
    m_prefixTable.clear();
    m_entityByPrefix.clear();
    m_maxPrefixLen = 0;
    m_exactPosition.clear();
    m_prefixPosition.clear();

    // We accumulate alias lines until we hit a new entity header.
    DxccEntity current;
    bool inEntity = false;
    QString aliasBuffer;

    auto commitEntity = [&]() {
        if (!inEntity || current.primaryPrefix.isEmpty())
            return;

        m_entityByPrefix.insert(current.primaryPrefix, current);

        // Register primary prefix itself
        m_prefixTable.insert(current.primaryPrefix, current.primaryPrefix);
        m_prefixPosition.remove(current.primaryPrefix);
        m_maxPrefixLen = std::max(m_maxPrefixLen, (int)current.primaryPrefix.length());

        // Parse accumulated alias buffer
        QString buf = aliasBuffer;
        buf.remove('\n');
        buf.remove('\r');
        // Remove trailing semicolon
        buf = buf.trimmed();
        if (buf.endsWith(';')) buf.chop(1);

        const QStringList tokens = buf.split(',', Qt::SkipEmptyParts);
        for (const QString& raw : tokens) {
            const QString tok = raw.trimmed();
            if (tok.isEmpty()) continue;

            if (tok.startsWith('=')) {
                // Exact match
                const QString exact = cleanPrefix(tok.mid(1)).toUpper();
                if (!exact.isEmpty()) {
                    m_exactMatch.insert(exact, current.primaryPrefix);
                    // NereusSDR: the override belongs to this alias only.
                    if (const auto pos = latLongOverride(tok)) {
                        m_exactPosition.insert(exact, *pos);
                    } else {
                        m_exactPosition.remove(exact);
                    }
                }
            } else {
                // Prefix
                const QString pfx = cleanPrefix(tok).toUpper();
                if (!pfx.isEmpty()) {
                    m_prefixTable.insert(pfx, current.primaryPrefix);
                    m_maxPrefixLen = std::max(m_maxPrefixLen, (int)pfx.length());
                    // NereusSDR: the override belongs to this alias only.
                    if (const auto pos = latLongOverride(tok)) {
                        m_prefixPosition.insert(pfx, *pos);
                    } else {
                        m_prefixPosition.remove(pfx);
                    }
                }
            }
        }
    };

    // Regex for header line: "Entity Name:  CQ:  ITU:  Cont:  lat:  lon:  tz:  Prefix:"
    // Fields are colon-separated on the first line.
    // NereusSDR: latitude and longitude are captured (groups 5 and 6); the
    // primary prefix moves from group 5 to group 7.
    static const QRegularExpression headerRe(
        R"(^([^:]+):\s*(\d+):\s*(\d+):\s*(\w+):\s*([\d\.\-]+):\s*([\d\.\-]+):\s*[\d\.\-]+:\s*([^:]+):)");

    for (const QString& line : lines) {
        // Header line — doesn't start with whitespace
        if (!line.isEmpty() && line[0] != ' ' && line[0] != '\t') {
            // Commit previous entity
            commitEntity();

            auto m = headerRe.match(line);
            if (!m.hasMatch()) {
                inEntity = false;
                continue;
            }

            current = DxccEntity{};
            current.name         = m.captured(1).trimmed();
            current.cqZone       = m.captured(2).toInt();
            current.ituZone      = m.captured(3).toInt();
            current.continent    = m.captured(4).trimmed();
            // NereusSDR: cty.dat column 5 is latitude + north, column 6
            // longitude + west. The format page does not state the sign; the
            // data does (cty.dat:532 United States 91.87, cty.dat:515 Japan
            // -138.38). Stored + east.
            current.latitude     = m.captured(5).toDouble();
            current.longitude    = -m.captured(6).toDouble();
            current.primaryPrefix = m.captured(7).trimmed().toUpper();
            // Remove trailing slash variants like "3D2/c" -> use as-is (sub-entities get own primary prefix)
            inEntity = true;
            aliasBuffer.clear();
        } else if (inEntity) {
            // Continuation line with aliases
            aliasBuffer += line;
        }
    }
    // Commit the last entity
    commitEntity();
}

// From AetherSDR src/core/CtyDatParser.cpp:151-195 [@0cd4559]
//
// ---------------------------------------------------------------------------
// Callsign resolution
// ---------------------------------------------------------------------------

// NereusSDR: upstream's resolvePrimaryPrefix body, returning the matched
// alias's <lat/long> override with the primary prefix.
QString CtyDatParser::resolvePrimaryPrefix(const QString& callsign) const
{
    return resolveMatch(callsign).primaryPrefix;
}

CtyDatParser::Match CtyDatParser::resolveMatch(const QString& callsign) const
{
    if (callsign.isEmpty()) return {};
    const QString cs = callsign.toUpper();

    // 1. Exact match first
    if (m_exactMatch.contains(cs)) {
        Match exact{m_exactMatch.value(cs), std::nullopt};
        if (auto pos = m_exactPosition.find(cs); pos != m_exactPosition.end()) {
            exact.positionOverride = pos.value();
        }
        return exact;
    }

    // Strip /P /M /MM /AM portable suffixes — use base call for prefix lookup.
    // But keep /country suffixes (e.g. G3ABC/VK4) — the part after the last /
    // is tried as a prefix override if it's 1-3 chars.
    QString base = cs;
    if (cs.contains('/')) {
        const QStringList parts = cs.split('/');
        if (parts.size() == 2) {
            const QString& suffix = parts[1];
            // If suffix looks like a DXCC prefix (not P/M/MM/AM/QRP)
            if (suffix != "P" && suffix != "M" && suffix != "MM" &&
                suffix != "AM" && suffix != "QRP" && suffix.length() <= 4) {
                // Try the suffix as a prefix
                Match r = resolveMatch(suffix);
                if (!r.primaryPrefix.isEmpty()) return r;
            }
            base = parts[0];
        } else {
            base = parts[0];
        }
    }

    // 2. Longest-prefix match (try from longest to shortest)
    const int maxLen = std::min(m_maxPrefixLen, (int)base.length());
    for (int len = maxLen; len >= 1; --len) {
        const QString pfx = base.left(len);
        auto it = m_prefixTable.find(pfx);
        if (it != m_prefixTable.end()) {
            Match prefix{it.value(), std::nullopt};
            if (auto pos = m_prefixPosition.find(pfx); pos != m_prefixPosition.end()) {
                prefix.positionOverride = pos.value();
            }
            return prefix;
        }
    }

    return {};
}

// From AetherSDR src/core/CtyDatParser.cpp:197-202 [@0cd4559]
const DxccEntity* CtyDatParser::entityByPrefix(const QString& primaryPrefix) const
{
    auto it = m_entityByPrefix.find(primaryPrefix.toUpper());
    if (it == m_entityByPrefix.end()) return nullptr;
    return &it.value();
}

// NereusSDR addition: the matched alias's <lat/long> override if it has
// one, otherwise the entity's header position.
std::optional<GeoPosition> CtyDatParser::positionForCallsign(const QString& callsign) const
{
    const Match match = resolveMatch(callsign);
    if (match.primaryPrefix.isEmpty()) {
        return std::nullopt;
    }
    if (match.positionOverride) {
        return match.positionOverride;
    }
    const DxccEntity* entity = entityByPrefix(match.primaryPrefix);
    if (entity == nullptr) {
        return std::nullopt;
    }
    return GeoPosition{entity->latitude, entity->longitude};
}

} // namespace NereusSDR
