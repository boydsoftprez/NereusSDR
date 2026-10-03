// =================================================================
// src/core/AppSettings.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/database.cs, original licence from Thetis source is included below
//   AetherSDR src/core/AppSettings.{h,cpp} — AetherSDR has no per-file headers; project-level GPLv3 and contributor list per About dialog per https://github.com/ten9876/AetherSDR
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 AppSettings XML persistence: key/value semantics (PascalCase keys, True/False string booleans, per-StationName nesting) port Thetis database.cs SaveVarsDictionary/RestoreVarsDictionary pattern; QXmlStream file I/O skeleton follows AetherSDR `src/core/AppSettings.{h,cpp}`.
//   2026-09-23 - R-R3-21: migrateRenamedKeys() one-shot rename for keys
//                 whose writer and reader disagreed (WsjtxSpotLifetime).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: settings schema v8 drops the TCI rate limit
//                 saved in messages per second (TciRateLimitMsgsPerSec).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 16: the N2ADR
//                 filter migration covers the HL2 receive-only kit.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - Schema v9 (R-IOS-06, R-IOS-27): each slice's saved NR1
//                 values brought into Thetis's NR spinbox ranges once (old
//                 defaults to the new ones, out-of-range values clamped).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

//=================================================================
// database.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2012  FlexRadio Systems
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
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
//    USA
//=================================================================
// Modifications to the database import function to allow using files created with earlier versions.
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
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

// Upstream source 'AetherSDR src/core/AppSettings.{h,cpp}' has no top-of-file header — project-level LICENSE applies.

#include "AppSettings.h"

#include "core/ControlRanges.h"
#include "core/settings/ISettingsBackend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QBuffer>
#include <QSet>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// Profile override set by main() from --profile CLI (Issue #100).
// Scoped to the AppSettings singleton's file path + main.cpp's log dir.
// Empty string (default) preserves legacy single-profile behavior.
static QString s_profileOverride;

void AppSettings::setProfileOverride(const QString& profile)
{
    s_profileOverride = profile;
}

QString AppSettings::profileOverride()
{
    return s_profileOverride;
}

bool AppSettings::isValidProfileName(const QString& profile)
{
    if (profile.isEmpty()) {
        return false;
    }
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_-]+$"));
    return re.match(profile).hasMatch();
}

QString AppSettings::resolveConfigDir(const QString& profile)
{
    // 2026-05-12 (PR #238 follow-up): both branches now resolve through
    // QStandardPaths::writableLocation(GenericConfigLocation) so the
    // TestSandboxInit.cpp `setTestModeEnabled(true)` redirect actually
    // takes effect on every platform.  Previously the macOS branch
    // hardcoded ~/Library/Preferences/NereusSDR which bypassed the
    // test sandbox and let a test that called settings.save() (e.g.
    // tst_spothub_settings_tab) overwrite the developer's real
    // NereusSDR.settings file.  QStandardPaths returns the SAME
    // path on macOS (~/Library/Preferences) when test mode is OFF,
    // so production behavior is unchanged — only test isolation
    // gets fixed.
    const QString root = QStandardPaths::writableLocation(
                             QStandardPaths::GenericConfigLocation)
                         + QStringLiteral("/NereusSDR");
    if (!isValidProfileName(profile)) {
        return root;
    }
    return root + QStringLiteral("/profiles/") + profile;
}

QString AppSettings::resolveSettingsPath(const QString& profile)
{
    return resolveConfigDir(profile) + QStringLiteral("/NereusSDR.settings");
}

AppSettings& AppSettings::instance()
{
    static AppSettings s;
    return s;
}

AppSettings::AppSettings()
{
    initFilePath();
}

AppSettings::AppSettings(const QString& filePath)
    : m_filePath(filePath)
{
    // Used by tests — no automatic load(); caller controls load/save lifecycle.
}

void AppSettings::initFilePath()
{
    m_filePath = resolveSettingsPath(s_profileOverride);
}

// Remote Daemon R2, Task 1 -- see the doc comment on
// kDaemonProfileSeededKey (AppSettings.h) for why this exists. Mirrors
// the immediate-save() pattern used elsewhere for rare, important,
// one-shot writes (e.g. migrateVaxSchemaV1ToV2() below) rather than the
// debounced scheduleSettingsSave() timer RadioModel uses for frequent
// live changes: this writes at most once per settings store, so there is
// nothing to coalesce, and the marker must reach disk immediately so it
// is readable on the daemon's next launch even if this process is killed
// a moment later.
void AppSettings::seedDaemonProfileMarker()
{
    if (contains(QLatin1String(kDaemonProfileSeededKey))) {
        return;
    }
    setValue(QLatin1String(kDaemonProfileSeededKey), QStringLiteral("True"));
    save();
}

// ---------------------------------------------------------------------------
// XML key encoding helpers
//
// Settings keys can contain hierarchical separators and Thetis-derived
// profile names (e.g. "D-104+CPDR") whose characters are NOT valid XML 1.0
// NameChars. We per-character escape every non-NameChar to a __token__
// sentinel so the resulting element name is parseable by QXmlStreamReader.
//
// Escape table (single source of truth — encode and decode mirror it):
//   ':' '/' '[' ']' '+' ' ' '?' '&' '#' '(' ')' ',' '@' '!' '*' '\'' '"'
//   '=' ';' '<' '>' '{' '}' '|' '$' '%' '^' '~' '`' '\\'
//
// Bug fix 2026-04-30: pre-fix builds wrote element names with literal '+',
// causing QXmlStreamReader to bail mid-file, silently dropping every
// element after the first malformed one (in particular all radios/* keys
// alphabetically after hardware/...). sanitizeXmlForLoad() pre-processes
// the file text so older corrupt files round-trip cleanly on first load
// after upgrade — once the next save() runs they are well-formed.
// ---------------------------------------------------------------------------

static QString encodeXmlKey(const QString& key)
{
    QString out;
    out.reserve(key.size());
    for (QChar c : key) {
        switch (c.unicode()) {
            case ':':  out += QLatin1String("__c__");     break;
            case '/':  out += QLatin1String("__s__");     break;
            case '[':  out += QLatin1String("__lb__");    break;
            case ']':  out += QLatin1String("__rb__");    break;
            case '+':  out += QLatin1String("__plus__");  break;
            case ' ':  out += QLatin1String("__sp__");    break;
            case '?':  out += QLatin1String("__qm__");    break;
            case '&':  out += QLatin1String("__amp__");   break;
            case '#':  out += QLatin1String("__hash__");  break;
            case '(':  out += QLatin1String("__lp__");    break;
            case ')':  out += QLatin1String("__rp__");    break;
            case ',':  out += QLatin1String("__cm__");    break;
            case '@':  out += QLatin1String("__at__");    break;
            case '!':  out += QLatin1String("__excl__");  break;
            case '*':  out += QLatin1String("__ast__");   break;
            case '\'': out += QLatin1String("__sq__");    break;
            case '"':  out += QLatin1String("__dq__");    break;
            case '=':  out += QLatin1String("__eq__");    break;
            case ';':  out += QLatin1String("__sc__");    break;
            case '<':  out += QLatin1String("__lt__");    break;
            case '>':  out += QLatin1String("__gt__");    break;
            case '{':  out += QLatin1String("__lc__");    break;
            case '}':  out += QLatin1String("__rc__");    break;
            case '|':  out += QLatin1String("__pipe__");  break;
            case '$':  out += QLatin1String("__dlr__");   break;
            case '%':  out += QLatin1String("__pct__");   break;
            case '^':  out += QLatin1String("__caret__"); break;
            case '~':  out += QLatin1String("__tilde__"); break;
            case '`':  out += QLatin1String("__bt__");    break;
            case '\\': out += QLatin1String("__bs__");    break;
            default:   out += c;                          break;
        }
    }
    return out;
}

static QString decodeXmlKey(const QString& tag)
{
    QString out = tag;
    out.replace(QLatin1String("__c__"),     QLatin1String(":"));
    out.replace(QLatin1String("__s__"),     QLatin1String("/"));
    out.replace(QLatin1String("__lb__"),    QLatin1String("["));
    out.replace(QLatin1String("__rb__"),    QLatin1String("]"));
    out.replace(QLatin1String("__plus__"),  QLatin1String("+"));
    out.replace(QLatin1String("__sp__"),    QLatin1String(" "));
    out.replace(QLatin1String("__qm__"),    QLatin1String("?"));
    out.replace(QLatin1String("__amp__"),   QLatin1String("&"));
    out.replace(QLatin1String("__hash__"),  QLatin1String("#"));
    out.replace(QLatin1String("__lp__"),    QLatin1String("("));
    out.replace(QLatin1String("__rp__"),    QLatin1String(")"));
    out.replace(QLatin1String("__cm__"),    QLatin1String(","));
    out.replace(QLatin1String("__at__"),    QLatin1String("@"));
    out.replace(QLatin1String("__excl__"),  QLatin1String("!"));
    out.replace(QLatin1String("__ast__"),   QLatin1String("*"));
    out.replace(QLatin1String("__sq__"),    QLatin1String("'"));
    out.replace(QLatin1String("__dq__"),    QLatin1String("\""));
    out.replace(QLatin1String("__eq__"),    QLatin1String("="));
    out.replace(QLatin1String("__sc__"),    QLatin1String(";"));
    out.replace(QLatin1String("__lt__"),    QLatin1String("<"));
    out.replace(QLatin1String("__gt__"),    QLatin1String(">"));
    out.replace(QLatin1String("__lc__"),    QLatin1String("{"));
    out.replace(QLatin1String("__rc__"),    QLatin1String("}"));
    out.replace(QLatin1String("__pipe__"),  QLatin1String("|"));
    out.replace(QLatin1String("__dlr__"),   QLatin1String("$"));
    out.replace(QLatin1String("__pct__"),   QLatin1String("%"));
    out.replace(QLatin1String("__caret__"), QLatin1String("^"));
    out.replace(QLatin1String("__tilde__"), QLatin1String("~"));
    out.replace(QLatin1String("__bt__"),    QLatin1String("`"));
    out.replace(QLatin1String("__bs__"),    QLatin1String("\\"));
    return out;
}

// One-shot recovery for files written by pre-2026-04-30 builds. Scan the
// raw XML and escape any non-NameChar found in element-name position. The
// scanner is structural (not regex) so it correctly skips comments, PIs,
// attribute values, and CDATA — only element names are touched. Files
// already well-formed pass through unchanged.
static QString sanitizeXmlForLoad(const QString& text)
{
    QString out;
    out.reserve(text.size());
    int i = 0;
    const int n = text.size();
    while (i < n) {
        const QChar c = text.at(i);
        if (c != QLatin1Char('<')) {
            out += c;
            i++;
            continue;
        }
        // Found '<'. Determine kind: '<?' (PI), '<!' (comment/doctype),
        // '</' (end tag), or '<' (start tag).
        out += c;
        i++;
        if (i >= n) {
            break;
        }
        const QChar next = text.at(i);
        if (next == QLatin1Char('?') || next == QLatin1Char('!')) {
            // Pass through unchanged until matching '>'
            while (i < n) {
                out += text.at(i);
                if (text.at(i) == QLatin1Char('>')) {
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }
        if (next == QLatin1Char('/')) {
            out += next;
            i++;
        }
        // Now positioned at start of element name. Escape any char that
        // isn't valid in NameChar. Stop at whitespace, '/', or '>'.
        while (i < n) {
            const QChar nc = text.at(i);
            if (nc == QLatin1Char('>') || nc == QLatin1Char('/') || nc.isSpace()) {
                break;
            }
            switch (nc.unicode()) {
                case '+':  out += QLatin1String("__plus__");  break;
                case ' ':  out += QLatin1String("__sp__");    break;
                case '?':  out += QLatin1String("__qm__");    break;
                case '&':  out += QLatin1String("__amp__");   break;
                case '#':  out += QLatin1String("__hash__");  break;
                case '(':  out += QLatin1String("__lp__");    break;
                case ')':  out += QLatin1String("__rp__");    break;
                case ',':  out += QLatin1String("__cm__");    break;
                case '@':  out += QLatin1String("__at__");    break;
                case '!':  out += QLatin1String("__excl__");  break;
                case '*':  out += QLatin1String("__ast__");   break;
                case '\'': out += QLatin1String("__sq__");    break;
                case '"':  out += QLatin1String("__dq__");    break;
                case '=':  out += QLatin1String("__eq__");    break;
                case ';':  out += QLatin1String("__sc__");    break;
                case '{':  out += QLatin1String("__lc__");    break;
                case '}':  out += QLatin1String("__rc__");    break;
                case '|':  out += QLatin1String("__pipe__");  break;
                case '$':  out += QLatin1String("__dlr__");   break;
                case '%':  out += QLatin1String("__pct__");   break;
                case '^':  out += QLatin1String("__caret__"); break;
                case '~':  out += QLatin1String("__tilde__"); break;
                case '`':  out += QLatin1String("__bt__");    break;
                case '\\': out += QLatin1String("__bs__");    break;
                default:   out += nc;                         break;
            }
            i++;
        }
        // Pass through rest of open tag (attributes, '/>', '>')
        while (i < n) {
            out += text.at(i);
            if (text.at(i) == QLatin1Char('>')) {
                i++;
                break;
            }
            i++;
        }
    }
    return out;
}

// Corruption-aware load helpers (issue #241).
//
// readFileForParse: returns the raw XML text only when the file looks
//                   intact enough to attempt a parse. Returns nullopt for
//                   "file is missing", "file is unreadable", "file is
//                   empty", and "file starts with NUL bytes" (the canonical
//                   shape produced by an NTFS journal rollback over an
//                   in-flight write — see issue #241).
//
// parseSettingsXml: parses sanitized XML into the supplied QMaps and
//                   returns true only when the parser reaches the end
//                   without raising hasError(). On a clean failure both
//                   maps are left empty so the caller can fall through to
//                   the .bak path without inheriting half-loaded keys.
namespace {

enum class ReadResult {
    Ok,             // file was readable + non-empty + did not start with NULs
    Missing,        // file does not exist (first-run path — no corruption)
    Unreadable,     // file exists but open() failed (treat as corrupt)
    EmptyOrZeroed,  // file is zero bytes or starts with NUL (treat as corrupt)
};

ReadResult readFileForParse(const QString& path, QString& outXml)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return ReadResult::Missing;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return ReadResult::Unreadable;
    }
    const QByteArray bytes = file.readAll();
    file.close();
    if (bytes.isEmpty()) {
        return ReadResult::EmptyOrZeroed;
    }
    // NTFS journal rollback over an in-flight write zeroes whole sectors at
    // the head of the file (issue #241 — 28 × 4 KB = 114,688 leading NULs
    // observed). A leading NUL byte is never valid for our XML output.
    if (bytes.at(0) == '\0') {
        return ReadResult::EmptyOrZeroed;
    }
    outXml = QString::fromUtf8(bytes);
    return ReadResult::Ok;
}

bool parseSettingsXml(const QString& sanitizedXml,
                      QMap<QString, QString>& outSettings,
                      QMap<QString, QString>& outStationSettings,
                      QString& outStationName)
{
    outSettings.clear();
    outStationSettings.clear();

    QXmlStreamReader xml(sanitizedXml);
    QString currentStation;
    bool inStation = false;
    bool rootSeen = false;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString tag = xml.name().toString();
            if (!rootSeen && tag == QStringLiteral("NereusSDR")) {
                rootSeen = true;
                continue;
            }
            if (!inStation && xml.attributes().hasAttribute(QStringLiteral("type"))
                && xml.attributes().value(QStringLiteral("type")) == QStringLiteral("station")) {
                inStation = true;
                currentStation = tag;
                outStationName = tag;
                continue;
            }
            const QString value = xml.readElementText();
            const QString key   = decodeXmlKey(tag);
            if (inStation) {
                outStationSettings.insert(key, value);
            } else {
                outSettings.insert(key, value);
            }
        } else if (xml.isEndElement() && inStation) {
            if (xml.name().toString() == currentStation) {
                inStation = false;
            }
        }
    }

    if (xml.hasError()) {
        // Wipe the partially-populated maps — half-loaded settings are
        // worse than starting from .bak or defaults (silent data loss is
        // exactly what issue #241 was filed against).
        outSettings.clear();
        outStationSettings.clear();
        return false;
    }
    return true;
}

bool parseImportXml(const QByteArray& input, QMap<QString, QString>& settings,
                    QMap<QString, QString>& stationSettings, QString& stationName,
                    QString* error)
{
    auto reject = [error](const QString& reason) {
        if (error) {
            *error = reason;
        }
        return false;
    };
    if (input.isEmpty()) {
        return reject(QStringLiteral("Settings XML is empty"));
    }
    if (input.size() > 16 * 1024 * 1024) {
        return reject(QStringLiteral("Settings XML exceeds 16 MiB"));
    }
    QXmlStreamReader xml(input);
    int depth = 0;
    bool rootSeen = false;
    bool rootClosed = false;
    bool stationSeen = false;
    bool inStation = false;
    stationName = QStringLiteral("NereusSDR");
    while (!xml.atEnd()) {
        const QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::DTD || token == QXmlStreamReader::EntityReference) {
            return reject(QStringLiteral("Settings XML may not contain DTDs or entities"));
        }
        if (token == QXmlStreamReader::StartElement) {
            ++depth;
            const QString tag = xml.name().toString();
            const QXmlStreamAttributes attrs = xml.attributes();
            if (depth == 1) {
                if (rootSeen || rootClosed || tag != QStringLiteral("NereusSDR")
                    || !attrs.isEmpty()) {
                    return reject(QStringLiteral("Settings XML has the wrong root"));
                }
                rootSeen = true;
                continue;
            }
            if (depth == 2 && attrs.size() == 1
                && attrs.value(QStringLiteral("type")) == QStringLiteral("station")) {
                if (stationSeen) {
                    return reject(QStringLiteral("Settings XML has multiple Core groups"));
                }
                stationSeen = true;
                inStation = true;
                stationName = tag;
                continue;
            }
            if ((depth == 2 && !inStation) || (depth == 3 && inStation)) {
                if (!attrs.isEmpty()) {
                    return reject(QStringLiteral("Settings XML key has unexpected attributes"));
                }
                const QString key = decodeXmlKey(tag);
                QMap<QString, QString>& target = inStation ? stationSettings : settings;
                if (target.contains(key)) {
                    return reject(QStringLiteral("Settings XML has a duplicate key: %1").arg(key));
                }
                const QString value = xml.readElementText();
                if (xml.hasError()) {
                    return reject(QStringLiteral("Settings XML has unexpected nested structure: %1")
                                  .arg(xml.errorString()));
                }
                target.insert(key, value);
                --depth; // readElementText consumed this element's end token.
                continue;
            }
            return reject(QStringLiteral("Settings XML has unexpected nested structure"));
        }
        if (token == QXmlStreamReader::EndElement) {
            if (depth == 2 && inStation) {
                inStation = false;
            }
            --depth;
            if (depth == 0) {
                rootClosed = true;
            }
        } else if (token == QXmlStreamReader::Characters && !xml.isWhitespace()) {
            return reject(QStringLiteral("Settings XML has text outside a value"));
        }
    }
    if (xml.hasError() || !rootClosed || depth != 0) {
        return reject(QStringLiteral("Settings XML is malformed: %1").arg(xml.errorString()));
    }
    if (error) {
        error->clear();
    }
    return true;
}

QByteArray serializeLocalXml(const QMap<QString, QString>& settings,
                             const QMap<QString, QString>& stationSettings,
                             const QString& stationName, QString* error)
{
    QByteArray output;
    QBuffer buffer(&output);
    if (!buffer.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("Settings XML buffer could not be opened");
        }
        return {};
    }
    QXmlStreamWriter xml(&buffer);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("NereusSDR"));
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        xml.writeTextElement(encodeXmlKey(it.key()), it.value());
    }
    if (!stationSettings.isEmpty()) {
        xml.writeStartElement(stationName);
        xml.writeAttribute(QStringLiteral("type"), QStringLiteral("station"));
        for (auto it = stationSettings.constBegin(); it != stationSettings.constEnd(); ++it) {
            xml.writeTextElement(encodeXmlKey(it.key()), it.value());
        }
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
    if (xml.hasError()) {
        if (error) {
            *error = QStringLiteral("Settings XML could not be written");
        }
        return {};
    }
    if (error) {
        error->clear();
    }
    return output;
}

void logLoadedSummary(const QMap<QString, QString>& settings,
                      int stationSettingsCount)
{
    int radiosCount = 0;
    QStringList radioMacs;
    for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        if (it.key().startsWith(QStringLiteral("radios/"))) {
            radiosCount++;
            const QString rest = it.key().mid(QStringLiteral("radios/").size());
            const int slash = rest.indexOf(QLatin1Char('/'));
            if (slash > 0) {
                const QString mac = rest.left(slash);
                if (!radioMacs.contains(mac)) {
                    radioMacs.append(mac);
                }
            }
        }
    }
    qDebug() << "Loaded" << settings.size() << "settings,"
             << stationSettingsCount << "station settings;"
             << radiosCount << "saved-radio keys across"
             << radioMacs.size() << "MAC(s)";
}

} // namespace

void AppSettings::load()
{
    // Reset diagnostic state at the top of every load() so wasCorruptedOnLoad()
    // / preservedCorruptFilePath() / recoveredFromBackup() always reflect THIS
    // load attempt rather than a previous instance's history.
    m_wasCorruptedOnLoad       = false;
    m_preservedCorruptFilePath.clear();
    m_recoveredFromBackup      = false;

    const QString bakPath = m_filePath + QStringLiteral(".bak");

    // Try the main settings file first.
    QString rawXml;
    const ReadResult mainRead = readFileForParse(m_filePath, rawXml);

    if (mainRead == ReadResult::Ok) {
        const QString sanitized = sanitizeXmlForLoad(rawXml);
        // Remote Daemon R2, Task 13: this bulk populate (and its .bak-
        // recovery twin below) does not fire the change hook per key.
        // Same reasoning as the corrupt-file m_settings.clear() fallback
        // further down in this function: startup state establishment via
        // a free function writing into m_settings/m_stationSettings by
        // reference, not a series of individually-meaningful mutations
        // through setValue().
        if (parseSettingsXml(sanitized, m_settings, m_stationSettings, m_stationName)) {
            migrateLegacyNnrSettings();
            logLoadedSummary(m_settings, m_stationSettings.size());
            return;
        }
        qWarning() << "XML parse error in settings:" << m_filePath
                   << "— treating as corrupt (issue #241 recovery path)";
    } else if (mainRead == ReadResult::EmptyOrZeroed) {
        qWarning() << "Settings file" << m_filePath
                   << "is empty or starts with NUL bytes (NTFS journal rollback shape)"
                   << "— treating as corrupt (issue #241 recovery path)";
    } else if (mainRead == ReadResult::Unreadable) {
        qWarning() << "Settings file" << m_filePath
                   << "exists but could not be opened — treating as corrupt";
    } else {
        // ReadResult::Missing — could be first-run, or could be the
        // "orphan .bak" case introduced by PR #244 itself: the corrupt-
        // preserve branch below renames a corrupt main file away to
        // <file>.corrupt-<ts>, leaving the on-disk state as
        //   main: missing,   .bak: good,   .corrupt-<ts>: bad
        // If the user kills the app after recovery but before the next
        // save() runs, the next launch sees main missing and would
        // silently fall through to defaults — re-creating the exact
        // data-loss failure mode #241 was filed against.
        //
        // Discriminate first-run from orphan-.bak by checking whether
        // .bak exists at all. If it does we fall through to the .bak
        // attempt (skipping the corrupt-preserve step — there's nothing
        // to preserve when main is missing).
        if (!QFileInfo::exists(bakPath)) {
            qDebug() << "No settings file found at" << m_filePath << "— using defaults";
            return;
        }
        qDebug() << "Settings file" << m_filePath
                 << "missing but" << bakPath << "found — attempting recovery";
    }

    // ── Corrupt-preserve step (skipped when main is missing) ────────────
    //
    // Preserve the corrupt file BEFORE doing anything else so the user
    // (or a developer) can attempt manual recovery. The next save() would
    // otherwise overwrite the only remaining copy with factory defaults
    // (the exact failure mode reported in issue #241).
    if (mainRead != ReadResult::Missing) {
        const QString stamp = QDateTime::currentDateTime().toString(
                                  QStringLiteral("yyyyMMdd-HHmmss"));
        const QString corruptPath = m_filePath + QStringLiteral(".corrupt-") + stamp;
        if (QFile::rename(m_filePath, corruptPath)) {
            m_wasCorruptedOnLoad       = true;
            m_preservedCorruptFilePath = corruptPath;
            qWarning() << "Corrupt settings file preserved as" << corruptPath;
        } else {
            // Rename failed (cross-volume, permissions, etc.). Fall back to a
            // copy + remove; if even that fails just leave the corrupt file
            // alone — defaults will still write through QSaveFile and won't
            // touch it until the next save() runs.
            if (QFile::copy(m_filePath, corruptPath)) {
                QFile::remove(m_filePath);
                m_wasCorruptedOnLoad       = true;
                m_preservedCorruptFilePath = corruptPath;
                qWarning() << "Corrupt settings file copied (rename failed) to" << corruptPath;
            } else {
                qWarning() << "Could not preserve corrupt settings file at" << corruptPath
                           << "— attempting backup recovery anyway";
            }
        }
    }

    // Try the .bak fallback. If it exists and parses cleanly we restore
    // the previous good state; otherwise we leave m_settings empty and
    // proceed with defaults.
    if (QFileInfo::exists(bakPath)) {
        QString bakXml;
        const ReadResult bakRead = readFileForParse(bakPath, bakXml);
        if (bakRead == ReadResult::Ok) {
            const QString sanitized = sanitizeXmlForLoad(bakXml);
            // Same "bulk populate, no hook fire" reasoning as the
            // main-file parse above.
            if (parseSettingsXml(sanitized, m_settings, m_stationSettings, m_stationName)) {
                migrateLegacyNnrSettings();
                m_recoveredFromBackup = true;
                qWarning() << "Recovered settings from backup file" << bakPath;
                logLoadedSummary(m_settings, m_stationSettings.size());
                return;
            }
            qWarning() << "Backup settings file" << bakPath
                       << "is also corrupt — falling back to defaults";
        } else if (bakRead != ReadResult::Missing) {
            qWarning() << "Backup settings file" << bakPath
                       << "exists but is unreadable / empty / NUL-prefixed"
                       << "— falling back to defaults";
        }
    } else {
        qWarning() << "No backup settings file at" << bakPath
                   << "— falling back to defaults";
    }

    // Defaults path — leave both maps empty so first save() writes a fresh
    // factory-default file. The corrupt file (if rename succeeded) and the
    // .bak (if any) are untouched on disk for forensic inspection.
    //
    // Remote Daemon R2, Task 13: deliberately raw m_settings.clear(), not
    // the public clear() method, and does not fire the change hook. This
    // is a wholesale reset triggered by a recovery path, not a value
    // change a delegation backend needs to mirror -- the same reasoning
    // that excludes the public clear() from firing (see setChangeHook()'s
    // doc comment in AppSettings.h), and load() runs at startup before
    // any caller could plausibly have installed a hook yet regardless.
    m_settings.clear();
    m_stationSettings.clear();
}

bool AppSettings::save(QString* error)
{
    if (error) {
        error->clear();
    }
    const QByteArray localXml = serializeLocalXml(m_settings, m_stationSettings,
                                                   m_stationName, error);
    if (localXml.isEmpty()) {
        return false;
    }
    // Ensure directory exists
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    // ── .bak rotation (issue #241) ──────────────────────────────────────
    //
    // Before overwriting the live settings file we copy the current good
    // version to "<filePath>.bak" via a .bak.tmp staging step that we
    // atomically rename into place. This guarantees that if a crash takes
    // down the system mid-save, the next launch finds either:
    //   (a) the previous good main file (atomic write below has not yet
    //       run), OR
    //   (b) the new main file plus a .bak holding the previous good state.
    //
    // We never enter a state where main is corrupt AND .bak is missing —
    // that's the exact failure mode reported in issue #241 (NTFS journal
    // rollback over a non-atomic write left the user with no recovery
    // path at all).
    if (QFileInfo::exists(m_filePath)) {
        const QString bakPath    = m_filePath + QStringLiteral(".bak");
        const QString bakTmpPath = m_filePath + QStringLiteral(".bak.tmp");
        QFile::remove(bakTmpPath);  // tolerate stale .bak.tmp from a prior crash
        if (QFile::copy(m_filePath, bakTmpPath)) {
            // QFile::rename refuses to clobber an existing destination, so
            // remove the previous .bak first. The window between remove()
            // and rename() is tolerable here: even if a crash hits between
            // the two, the main file is still intact (we have not touched
            // it yet) and .bak.tmp will be cleaned up on the next save.
            QFile::remove(bakPath);
            if (!QFile::rename(bakTmpPath, bakPath)) {
                qWarning() << "Could not rotate settings backup to" << bakPath
                           << "— previous .bak (if any) lost; main save proceeding";
                QFile::remove(bakTmpPath);
            }
        } else {
            qWarning() << "Could not stage settings backup at" << bakTmpPath
                       << "— main save proceeding without rotating .bak";
        }
    }

    // ── Atomic write of the main file (issue #241) ──────────────────────
    //
    // QSaveFile writes to a hidden temp file alongside the destination,
    // calls fsync() on commit(), then performs an atomic rename over the
    // destination. On NTFS that rename uses MoveFileEx with
    // MOVEFILE_REPLACE_EXISTING which is journaled at the metadata level
    // — the destination is either entirely the previous version or
    // entirely the new version, never a partial overlay. This makes the
    // "leading 28 × 4 KB sectors zeroed" failure mode from issue #241
    // structurally impossible.
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Could not save settings to" << m_filePath
                   << ":" << file.errorString();
        if (error) {
            *error = QStringLiteral("Settings could not be saved: %1").arg(file.errorString());
        }
        return false;
    }

    if (file.write(localXml) != localXml.size()) {
        const QString reason = QStringLiteral("Settings XML could not be written: %1")
            .arg(file.errorString());
        file.cancelWriting();
        if (error) {
            *error = reason;
        }
        return false;
    }

    if (!file.commit()) {
        qWarning() << "Could not commit settings to" << m_filePath
                   << ":" << file.errorString();
        if (error) {
            *error = QStringLiteral("Settings could not be saved: %1").arg(file.errorString());
        }
        return false;
    }

    QFile::setPermissions(m_filePath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

QByteArray AppSettings::exportLocalXml(QString* error) const
{
    return serializeLocalXml(m_settings, m_stationSettings, m_stationName, error);
}

bool AppSettings::validateLocalXml(const QByteArray& input, QString* error)
{
    QMap<QString, QString> settings;
    QMap<QString, QString> stationSettings;
    QString stationName;
    return parseImportXml(input, settings, stationSettings, stationName, error);
}

bool AppSettings::importLocalXml(const QByteArray& input, QString* error)
{
    if (error) {
        error->clear();
    }
    if (m_remoteBackend || m_changeHook) {
        if (error) {
            *error = QStringLiteral("Settings owner is still active; stop its proxy and change hook before import");
        }
        return false;
    }
    QMap<QString, QString> settings;
    QMap<QString, QString> stationSettings;
    QString stationName;
    if (!parseImportXml(input, settings, stationSettings, stationName, error)) {
        return false;
    }
    AppSettings replacement(m_filePath);
    replacement.m_settings = settings;
    replacement.m_stationSettings = stationSettings;
    replacement.m_stationName = stationName;
    if (!replacement.save(error)) {
        return false;
    }
    m_settings.swap(settings);
    m_stationSettings.swap(stationSettings);
    m_stationName.swap(stationName);
    return true;
}

QVariant AppSettings::value(const QString& key, const QVariant& defaultValue) const
{
    // Remote Daemon R2, Task 15 -- one-branch delegation. See
    // setRemoteBackend()'s doc comment (AppSettings.h) for the full
    // contract; nullptr (or a backend that declines this key) falls
    // straight through to the ORIGINAL body below, unchanged.
    if (m_remoteBackend && m_remoteBackend->handlesKey(key)) {
        return m_remoteBackend->value(key, defaultValue);
    }
    auto it = m_settings.constFind(key);
    if (it != m_settings.constEnd()) {
        return QVariant(it.value());
    }
    return defaultValue;
}

void AppSettings::setValue(const QString& key, const QVariant& val)
{
    // Remote Daemon R2, Task 15 -- one-branch delegation (see value()).
    // The delegated path does not touch m_settings and does not fire
    // m_changeHook: that hook is this LOCAL instance's own "something in
    // my own map changed" signal (Task 13), and a delegated write never
    // touches this instance's own map at all.
    if (m_remoteBackend && m_remoteBackend->handlesKey(key)) {
        m_remoteBackend->setValue(key, val);
        return;
    }
    m_settings.insert(key, val.toString());
    if (m_changeHook) {
        m_changeHook(key);
    }
}

void AppSettings::remove(const QString& key)
{
    // Remote Daemon R2, Task 15 -- delegation, but NOT strictly
    // one-branch the way value()/setValue()/contains() are. Fix round 1
    // (review, Important 1): a purely-delegated remove() left a STALE
    // local m_settings entry (from a previous LOCAL session, before a
    // backend was ever installed) completely unreachable once a backend
    // claimed that key's family -- allKeys()'s own fix (above) makes such
    // an entry correctly invisible to allKeys()/contains()/value(), but
    // invisible is not the same as gone, and "forget radio"
    // (AppSettings::clearHardwareValues() / AppSettings::forgetRadio(),
    // BOTH of which fully funnel through this method) needs it actually
    // gone, not just hidden while a backend happens to be installed.
    // remove() is the one of the five delegated operations where
    // touching BOTH targets is safe: unlike setValue() (which must NEVER
    // touch m_settings for a backend-claimed key, or a remote-mode GUI's
    // own local file would end up storing another station's data -- see
    // AppSettings.h's snapshot() doc comment), removing a key that is
    // not locally present is a harmless no-op, so there is no
    // symmetrical contamination risk here.
    //
    // Fix round 2 (review, smaller item): NOT covered by this method at
    // all, despite the name collision -- SettingsHygiene::forgetRadio()
    // (SettingsHygiene.cpp:136-154, a DIFFERENT class's method sharing
    // this method's colloquial name) discovers what to remove via its
    // OWN s.allKeys() scan, not via this class's remove(). Once
    // allKeys() correctly excludes a backend-claimed local key (this
    // file's own fix, above), that scan cannot find such a key either,
    // so this method's local-cleanup half never runs for it -- the exact
    // shape of gap clearHardwareValues() had before its own fix.
    // Currently inert: both live call sites
    // (DiagnosticsPhaseHPages.cpp:184, RadioStatusPage.cpp:657) pass an
    // empty mac, so the scan's own prefix ("hardware//") never matches a
    // real key regardless. A future caller passing a real mac to
    // SettingsHygiene::forgetRadio() would resurrect this exact bug for
    // that call site; fixing it is that class's responsibility, not
    // this one's, since AppSettings has no way to know SettingsHygiene
    // exists.
    if (m_remoteBackend && m_remoteBackend->handlesKey(key)) {
        m_remoteBackend->remove(key);
        // Best-effort local cleanup, not a locally-observed value
        // change -- deliberately does NOT fire m_changeHook, matching
        // this delegated branch's existing contract (see value()'s own
        // comment: "does not touch m_settings and does not fire
        // m_changeHook") for anything reads/writes through this branch.
        m_settings.remove(key);
        return;
    }
    m_settings.remove(key);
    if (m_changeHook) {
        m_changeHook(key);
    }
}

bool AppSettings::contains(const QString& key) const
{
    // Remote Daemon R2, Task 15 -- one-branch delegation (see value()).
    if (m_remoteBackend && m_remoteBackend->handlesKey(key)) {
        return m_remoteBackend->contains(key);
    }
    return m_settings.contains(key);
}

QStringList AppSettings::allKeys() const
{
    // Remote Daemon R2, Task 15 -- additive, not a guard clause: there is
    // no single key to ask handlesKey() about, so this unions the local
    // keys with whatever the backend currently holds real values for
    // (ISettingsBackend::handledKeys()). m_remoteBackend == nullptr
    // (today's only path outside a test, and every path before this
    // task) takes the early return below, leaving the return value
    // byte-identical to `m_settings.keys()`.
    //
    // Fix round 1 (review, Important 1): a local m_settings entry the
    // backend now CLAIMS (handlesKey() true) MUST be excluded from the
    // local half of the union. Before this fix the union only ever
    // added, so a key left over in m_settings from a previous LOCAL
    // session -- the ordinary case for an operator who has used the
    // radio directly before going remote -- stayed listed here forever
    // even though contains()/value() already delegate it away to the
    // backend and report it absent. Two concrete breakages that produced:
    // hardwareValues() (below) iterates allKeys() and calls value(k) for
    // each match, so a stale local hardware/<mac>/* entry became a
    // phantom map entry whose value() came back as the caller's invalid
    // default instead of the key being absent from the result at all;
    // and clearHardwareValues() (below) called remove(k), which
    // delegates and never touches the stale local m_settings entry, so
    // "forget radio" on a remote client left those keys on disk forever
    // and they kept reappearing in allKeys(). A QSet does the membership
    // test in O(1) rather than QStringList::contains()'s O(n) scan
    // repeated for every remote key (the previous shape was O(n x m));
    // hardwareValues() calls this on every Setup-page restore.
    if (!m_remoteBackend) {
        return m_settings.keys();
    }
    QSet<QString> out;
    const QStringList localKeys = m_settings.keys();
    out.reserve(localKeys.size());
    for (const QString& k : localKeys) {
        if (!m_remoteBackend->handlesKey(k)) {
            out.insert(k);
        }
    }
    const QStringList remoteKeys = m_remoteBackend->handledKeys();
    for (const QString& k : remoteKeys) {
        out.insert(k);
    }
    return out.values();
}

void AppSettings::clear()
{
    // Deliberately does not fire the change hook -- see the doc comment
    // on setChangeHook() (AppSettings.h): a bulk test-isolation wipe has
    // no single key to report, and is not one of the ten operations the
    // R2 Task 13 brief lists as required to fire.
    m_settings.clear();
}

void AppSettings::setChangeHook(std::function<void(const QString& key)> hook)
{
    m_changeHook = std::move(hook);
}

QMap<QString, QString> AppSettings::snapshot(const QStringList& prefixes) const
{
    // Remote Daemon R2, Task 15 -- see the doc comment on this
    // declaration (AppSettings.h) for the full contract: fully-qualified
    // keys, raw QString values, a pure read over THIS instance's own
    // m_settings with no m_remoteBackend involvement at all. O(keys x
    // prefixes); a real snapshot call happens once per connect, against
    // a handful of prefixes, so this is not a hot path.
    QMap<QString, QString> out;
    for (auto it = m_settings.constBegin(); it != m_settings.constEnd(); ++it) {
        for (const QString& prefix : prefixes) {
            if (it.key().startsWith(prefix)) {
                out.insert(it.key(), it.value());
                break;
            }
        }
    }
    return out;
}

QVariant AppSettings::stationValue(const QString& key, const QVariant& defaultValue) const
{
    auto it = m_stationSettings.constFind(key);
    if (it != m_stationSettings.constEnd()) {
        return QVariant(it.value());
    }
    return defaultValue;
}

void AppSettings::setStationValue(const QString& key, const QVariant& val)
{
    m_stationSettings.insert(key, val.toString());
}

QString AppSettings::stationName() const
{
    return m_stationName;
}

void AppSettings::setStationName(const QString& name)
{
    m_stationName = name;
}

// ---------------------------------------------------------------------------
// Saved-radio helpers (Phase 3I Task 15)
// ---------------------------------------------------------------------------

// static
QString AppSettings::radioKeyPrefix(const QString& macKey)
{
    return QStringLiteral("radios/%1/").arg(macKey);
}

// static
QString AppSettings::macKeyFromSettingsKey(const QString& settingsKey)
{
    // Settings keys for per-radio fields look like: "radios/<macKey>/<field>"
    // This returns the <macKey> portion, or empty if the key doesn't match.
    static const QString kPrefix = QStringLiteral("radios/");
    if (!settingsKey.startsWith(kPrefix)) {
        return {};
    }
    const QString rest = settingsKey.mid(kPrefix.size()); // "<macKey>/<field>"
    const int slashIdx = rest.indexOf(QLatin1Char('/'));
    if (slashIdx < 0) {
        return {}; // "radios/lastConnected" etc. — top-level, not per-radio
    }
    return rest.left(slashIdx);
}

void AppSettings::saveRadio(const RadioInfo& info, bool pinToMac, bool autoConnect)
{
    const QString macKey = info.macAddress.isEmpty()
        ? QStringLiteral("manual-%1-%2")
              .arg(info.address.toString())
              .arg(info.port)
        : info.macAddress;

    const QString prefix = radioKeyPrefix(macKey);
    setValue(prefix + QStringLiteral("name"),            info.name);
    setValue(prefix + QStringLiteral("ipAddress"),       info.address.toString());
    setValue(prefix + QStringLiteral("port"),            QString::number(info.port));
    setValue(prefix + QStringLiteral("macAddress"),      info.macAddress);
    setValue(prefix + QStringLiteral("boardType"),
             QString::number(static_cast<int>(info.boardType)));
    setValue(prefix + QStringLiteral("protocol"),
             QString::number(static_cast<int>(info.protocol)));
    setValue(prefix + QStringLiteral("firmwareVersion"), QString::number(info.firmwareVersion));
    setValue(prefix + QStringLiteral("pinToMac"),        pinToMac   ? QStringLiteral("True") : QStringLiteral("False"));
    setValue(prefix + QStringLiteral("autoConnect"),     autoConnect ? QStringLiteral("True") : QStringLiteral("False"));
    setValue(prefix + QStringLiteral("lastSeen"),
             QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    // Model override (Phase 3I-RP). FIRST = no override.
    if (info.modelOverride != HPSDRModel::FIRST) {
        setValue(prefix + QStringLiteral("modelOverride"),
                 QString::number(static_cast<int>(info.modelOverride)));
    }
}

void AppSettings::setRadioAutoConnect(const QString& macKey, bool autoConnect)
{
    const QString prefix = radioKeyPrefix(macKey);
    if (!contains(prefix + QStringLiteral("macAddress"))
        && !contains(prefix + QStringLiteral("ipAddress"))) {
        return;  // not a saved radio
    }
    setValue(prefix + QStringLiteral("autoConnect"),
             autoConnect ? QStringLiteral("True") : QStringLiteral("False"));
}

void AppSettings::forgetRadio(const QString& macKey)
{
    const QString prefix = radioKeyPrefix(macKey);
    // Remove all keys with this prefix
    const QStringList keys = allKeys();
    for (const QString& k : keys) {
        if (k.startsWith(prefix)) {
            remove(k);
        }
    }
}

void AppSettings::clearSavedRadios()
{
    // Remove all radios/<key>/<field> entries (but preserve lastConnected, discoveryProfile)
    const QStringList keys = allKeys();
    for (const QString& k : keys) {
        if (!k.startsWith(QStringLiteral("radios/"))) {
            continue;
        }
        // Only remove keys that have a per-radio sub-path (3 segments: radios/<mac>/<field>)
        const QString rest = k.mid(7); // strip "radios/"
        if (rest.contains(QLatin1Char('/'))) {
            remove(k);
        }
    }
}

QList<SavedRadio> AppSettings::savedRadios() const
{
    // Collect all distinct macKeys
    QSet<QString> macKeys;
    const QStringList keys = allKeys();
    for (const QString& k : keys) {
        const QString mk = macKeyFromSettingsKey(k);
        if (!mk.isEmpty()) {
            macKeys.insert(mk);
        }
    }

    QList<SavedRadio> result;
    result.reserve(macKeys.size());
    for (const QString& mk : std::as_const(macKeys)) {
        if (auto sr = savedRadio(mk)) {
            result.append(*sr);
        }
    }
    return result;
}

std::optional<SavedRadio> AppSettings::savedRadio(const QString& macKey) const
{
    const QString prefix = radioKeyPrefix(macKey);
    const QString nameKey = prefix + QStringLiteral("name");
    if (!contains(nameKey)) {
        return std::nullopt;
    }

    SavedRadio sr;

    // RadioInfo fields
    sr.info.name            = value(prefix + QStringLiteral("name")).toString();
    sr.info.address         = QHostAddress(value(prefix + QStringLiteral("ipAddress")).toString());
    sr.info.port            = static_cast<quint16>(
                                value(prefix + QStringLiteral("port"),
                                      QStringLiteral("1024")).toUInt());
    sr.info.macAddress      = value(prefix + QStringLiteral("macAddress")).toString();
    sr.info.boardType       = static_cast<HPSDRHW>(
                                value(prefix + QStringLiteral("boardType"),
                                      QStringLiteral("999")).toInt());
    sr.info.protocol        = static_cast<ProtocolVersion>(
                                value(prefix + QStringLiteral("protocol"),
                                      QStringLiteral("1")).toInt());
    sr.info.firmwareVersion = value(prefix + QStringLiteral("firmwareVersion"),
                                    QStringLiteral("0")).toInt();

    // Saved-only flags
    sr.pinToMac    = (value(prefix + QStringLiteral("pinToMac"),
                            QStringLiteral("False")).toString() == QStringLiteral("True"));
    sr.autoConnect = (value(prefix + QStringLiteral("autoConnect"),
                            QStringLiteral("False")).toString() == QStringLiteral("True"));

    const QString lastSeenStr = value(prefix + QStringLiteral("lastSeen")).toString();
    if (!lastSeenStr.isEmpty()) {
        sr.lastSeen = QDateTime::fromString(lastSeenStr, Qt::ISODate);
    }

    // Model override (Phase 3I-RP)
    const QString moStr = value(prefix + QStringLiteral("modelOverride"),
                                 QStringLiteral("-1")).toString();
    int moInt = moStr.toInt();
    if (moInt > static_cast<int>(HPSDRModel::FIRST) &&
        moInt < static_cast<int>(HPSDRModel::LAST)) {
        sr.info.modelOverride = static_cast<HPSDRModel>(moInt);
    }

    return sr;
}

QString AppSettings::lastConnected() const
{
    return value(QStringLiteral("radios/lastConnected")).toString();
}

QString AppSettings::normalizedRadioMac(const QString& mac)
{
    QString compact = mac.trimmed().toUpper();
    compact.remove(QLatin1Char(':'));
    compact.remove(QLatin1Char('-'));
    if (compact.size() != 12)
        return {};
    for (QChar c : compact) {
        if (!((c >= QLatin1Char('0') && c <= QLatin1Char('9'))
              || (c >= QLatin1Char('A') && c <= QLatin1Char('F'))))
            return {};
    }
    QString result;
    for (int i = 0; i < compact.size(); i += 2) {
        if (!result.isEmpty())
            result += QLatin1Char(':');
        result += compact.mid(i, 2);
    }
    return result;
}

void AppSettings::migrateLegacyNnrSettings()
{
    // Only the last owner recorded in the loaded file can claim station-wide
    // Slice<N> NR selection. Run before connection mutates radios/lastConnected.
    if (m_remoteBackend)
        return;
    const QString mac = normalizedRadioMac(lastConnected());
    if (mac.isEmpty())
        return;
    const QString radioPrefix = QStringLiteral("hardware/%1/").arg(mac);
    const QString marker = radioPrefix + QStringLiteral("NnrMigrationComplete");
    if (contains(marker))
        return;

    const auto keys = allKeys();
    bool sawLegacyNr = false;
    for (const QString& key : keys) {
        if (!key.startsWith(QLatin1String("Slice")) || !key.endsWith(QLatin1String("/NrActive")))
            continue;
        const QString idText = key.mid(5, key.size() - 5 - 9);
        bool validId = false;
        const int id = idText.toInt(&validId);
        if (!validId || id < 0 || QString::number(id) != idText)
            continue;
        bool validValue = false;
        const int selected = value(key).toInt(&validValue);
        if (!validValue || selected < 0 || selected > 8)
            continue;
        sawLegacyNr = true;
        const QString target = radioPrefix + QStringLiteral("slices/%1/nnr/").arg(id);
        bool exists = false;
        for (const QString& existing : keys) {
            if (existing.startsWith(target)) {
                exists = true;
                break;
            }
        }
        if (!exists)
            setValue(target + QStringLiteral("NrActive"), selected);
    }
    if (sawLegacyNr)
        setValue(marker, QStringLiteral("True"));
}

void AppSettings::setLastConnected(const QString& macKey)
{
    if (macKey.isEmpty()) {
        remove(QStringLiteral("radios/lastConnected"));
    } else {
        setValue(QStringLiteral("radios/lastConnected"), macKey);
    }
}

DiscoveryProfile AppSettings::discoveryProfile() const
{
    // Default to SafeDefault (4)
    const int v = value(QStringLiteral("radios/discoveryProfile"),
                        QStringLiteral("4")).toInt();
    return static_cast<DiscoveryProfile>(v);
}

void AppSettings::setDiscoveryProfile(DiscoveryProfile p)
{
    setValue(QStringLiteral("radios/discoveryProfile"),
             QString::number(static_cast<int>(p)));
}

// ---------------------------------------------------------------------------
// Hardware tab persistence (Phase 3I Task 21)
// ---------------------------------------------------------------------------

void AppSettings::setHardwareValue(const QString& mac, const QString& key, const QVariant& value)
{
    const QString fullKey = QStringLiteral("hardware/%1/%2").arg(mac, key);
    setValue(fullKey, value);
}

QVariant AppSettings::hardwareValue(const QString& mac, const QString& key,
                                     const QVariant& defaultValue) const
{
    const QString fullKey = QStringLiteral("hardware/%1/%2").arg(mac, key);
    return value(fullKey, defaultValue);
}

QMap<QString, QVariant> AppSettings::hardwareValues(const QString& mac) const
{
    // Returns bare keys (prefix stripped): Task 15's snapshot() deliberately
    // does the opposite (fully-qualified keys); do not unify the two.
    const QString prefix = QStringLiteral("hardware/%1/").arg(mac);
    QMap<QString, QVariant> result;
    const QStringList keys = allKeys();
    for (const QString& k : keys) {
        if (k.startsWith(prefix)) {
            const QString bareKey = k.mid(prefix.size());
            result.insert(bareKey, value(k));
        }
    }
    return result;
}

void AppSettings::clearHardwareValues(const QString& mac)
{
    const QString prefix = QStringLiteral("hardware/%1/").arg(mac);

    // Fix round 1 (review, Important 1) -- scans m_settings.keys()
    // DIRECTLY, not the backend-aware allKeys(). allKeys() now
    // deliberately EXCLUDES a local entry the backend claims (see its
    // own comment above), which is exactly the shape "forget radio"
    // needs to find in order to delete: a STALE local leftover from a
    // previous LOCAL session that is invisible through the normal read
    // API but still physically present and still needs to actually go
    // away. Using allKeys() here (the pre-fix shape) meant this loop
    // could no longer even SEE such an entry to call remove() on it,
    // so the entry stayed on disk forever despite an explicit "forget"
    // action -- remove()'s own fix (below) can only do its job if this
    // loop still hands it the key.
    const QStringList localKeys = m_settings.keys();
    for (const QString& k : localKeys) {
        if (k.startsWith(prefix)) {
            remove(k);
        }
    }

    // Also clear anything the backend itself holds for this MAC that was
    // never sitting in m_settings at all (e.g. a value that only ever
    // arrived via a connect-time snapshot, with no local session ever
    // having cached it first).
    if (m_remoteBackend) {
        const QStringList remoteKeys = m_remoteBackend->handledKeys();
        for (const QString& k : remoteKeys) {
            if (k.startsWith(prefix)) {
                remove(k);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Model override (Phase 3I-RP)
// ---------------------------------------------------------------------------

HPSDRModel AppSettings::modelOverride(const QString& macKey) const
{
    const QString prefix = radioKeyPrefix(macKey);
    const QString val = value(prefix + QStringLiteral("modelOverride"),
                               QStringLiteral("-1")).toString();
    int v = val.toInt();
    if (v > static_cast<int>(HPSDRModel::FIRST) &&
        v < static_cast<int>(HPSDRModel::LAST)) {
        return static_cast<HPSDRModel>(v);
    }
    return HPSDRModel::FIRST;
}

void AppSettings::setModelOverride(const QString& macKey, HPSDRModel model)
{
    const QString prefix = radioKeyPrefix(macKey);
    setValue(prefix + QStringLiteral("modelOverride"),
             QString::number(static_cast<int>(model)));
}

// ---------------------------------------------------------------------------
// Radio-key migration (Phase 3Q Task 12)
// ---------------------------------------------------------------------------

void AppSettings::migrateRadioKey(const QString& oldKey, const QString& newKey)
{
    if (oldKey == newKey) {
        return;
    }

    // Read all fields stored under the old key via the typed accessor.
    // Returns std::nullopt when no entry exists — nothing to migrate.
    std::optional<SavedRadio> saved = savedRadio(oldKey);
    if (!saved.has_value()) {
        return;
    }

    // Promote the MAC address field to the real MAC so saveRadio() derives
    // the correct key (saveRadio uses info.macAddress when non-empty, else
    // falls back to "manual-<ip>-<port>").
    saved->info.macAddress = newKey;

    // Remove the old synthetic entry before writing the new one so there is
    // no window where both keys co-exist in the settings map.
    forgetRadio(oldKey);

    // Re-persist under the real-MAC key.  saveRadio() overwrites lastSeen with
    // QDateTime::currentDateTimeUtc() — acceptable for a first-probe migration.
    saveRadio(saved->info, saved->pinToMac, saved->autoConnect);
}

// ---------------------------------------------------------------------------
// Phase 3O VAX schema migration
// ---------------------------------------------------------------------------

void AppSettings::migrateVaxSchemaV1ToV2()
{
    auto& s = instance();
    if (!s.contains(QStringLiteral("audio/OutputDevice"))) { return; }
    if (s.contains(QStringLiteral("audio/Speakers/DeviceName"))) { return; }

    const QString dev = s.value(QStringLiteral("audio/OutputDevice")).toString();
    s.setValue(QStringLiteral("audio/Speakers/DeviceName"), dev);

    // Platform-default driver API. Conservative; user can tune later.
#if defined(Q_OS_WIN)
    s.setValue(QStringLiteral("audio/Speakers/DriverApi"), QStringLiteral("WASAPI"));
#elif defined(Q_OS_MAC)
    s.setValue(QStringLiteral("audio/Speakers/DriverApi"), QStringLiteral("CoreAudio"));
#else
    s.setValue(QStringLiteral("audio/Speakers/DriverApi"), QStringLiteral("Pulse"));
#endif
    s.setValue(QStringLiteral("audio/Speakers/SampleRate"),    QStringLiteral("48000"));
    s.setValue(QStringLiteral("audio/Speakers/BitDepth"),      QStringLiteral("24"));
    s.setValue(QStringLiteral("audio/Speakers/Channels"),      QStringLiteral("2"));
    s.setValue(QStringLiteral("audio/Speakers/BufferSamples"), QStringLiteral("256"));

    // Trigger first-run dialog on next launch.
    s.setValue(QStringLiteral("audio/FirstRunComplete"), QStringLiteral("False"));

    s.remove(QStringLiteral("audio/OutputDevice"));
    s.save();
}

// ---------------------------------------------------------------------------
// hermes-filter-debug Bug 2: legacy global N2ADR filter → per-MAC migration
// ---------------------------------------------------------------------------

void AppSettings::migrateLegacyN2adrFilter(AppSettings& s)
{
    static constexpr auto kLegacyKey = QLatin1String("hl2IoBoard/n2adrFilter");
    if (!s.contains(QString(kLegacyKey))) {
        return;  // no legacy key → nothing to do (also the idempotent path)
    }

    const QString legacyValue = s.value(QString(kLegacyKey)).toString();

    // Copy to per-MAC for every saved HL2.  Multiple HL2s inherit the same
    // global value as a starting point; the user can flip individual radios
    // afterwards and the per-MAC store keeps them independent.
    int hl2Count      = 0;
    int migratedCount = 0;
    for (const SavedRadio& r : s.savedRadios()) {
        // Task 16 (receiver and transmit gaps plan): the HL2 receive-only
        // kit is an HL2 (Task 15) and has the HL2's I/O board
        // (hasIoBoardHl2), so a kit saved while the global setting was in
        // use gets the value too.
        if (r.info.boardType != HPSDRHW::HermesLite
            && r.info.boardType != HPSDRHW::HermesLiteRxOnly) {
            continue;
        }
        ++hl2Count;
        // Don't overwrite an explicitly-set per-MAC value (defensive — a user
        // who has already flipped this on the new schema should win).
        if (s.hardwareValue(r.info.macAddress, QString(kLegacyKey)).isValid()) {
            continue;
        }
        s.setHardwareValue(r.info.macAddress, QString(kLegacyKey), legacyValue);
        ++migratedCount;
    }

    // Codex P1 (PR #160 review): only clear the legacy global once at least
    // one HL2 saved radio has been seen.  If no HL2 is registered yet (e.g.
    // user enabled N2ADR via the legacy code path but hasn't saved an HL2,
    // or only has a manual-IP entry whose boardType is still Unknown until
    // first probe), preserve the global so a future migration run on the
    // next launch can still pick it up.  Without this guard the legacy
    // value would be silently lost and N2ADR would come back disabled.
    if (hl2Count > 0) {
        s.remove(QString(kLegacyKey));
        qDebug() << "Migrated legacy N2ADR filter setting (" << legacyValue
                 << ") to" << migratedCount << "HL2 saved radio(s); legacy global removed";
    } else {
        qDebug() << "Migrated legacy N2ADR filter setting (" << legacyValue
                 << "): no HL2 saved radios yet; legacy global preserved for next launch";
    }
    s.save();
}

// R-R3-21: legacy global Penny Ext Control -> per-MAC migration
// ---------------------------------------------------------------------------

void AppSettings::migrateLegacyPennyExtCtrl(AppSettings& s)
{
    static constexpr auto kLegacyKey = QLatin1String("hardware/oc/pennyExtCtrl");
    static constexpr auto kRadioKey  = QLatin1String("penny/extCtrlEnabled");
    if (!s.contains(QString(kLegacyKey))) {
        return;  // nothing to carry over (also the idempotent path)
    }

    // The old checkbox stored a QVariant(bool), saved as "true"/"false";
    // PennyLaneController reads "True"/"False".
    const bool legacyOn = s.value(QString(kLegacyKey)).toString()
                              .compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
    const QString legacyValue = legacyOn ? QStringLiteral("True") : QStringLiteral("False");

    const QList<SavedRadio> radios = s.savedRadios();
    int migratedCount = 0;
    int realRadios = 0;
    bool placeholderRadios = false;
    for (const SavedRadio& r : radios) {
        // A manual radio saved without its MAC (key manual-<ip>-<port>, empty
        // macAddress) or under a MANUAL:<ip>:<port> placeholder has no key
        // PennyLaneController will read once the real MAC is known. Leave it
        // alone; the global stays so its real-MAC entry takes it later.
        const QString& mac = r.info.macAddress;
        if (mac.isEmpty() || mac.startsWith(QStringLiteral("MANUAL:"))) {
            placeholderRadios = true;
            continue;
        }
        ++realRadios;
        // A radio that already has its own value keeps it.
        if (s.hardwareValue(mac, QString(kRadioKey)).isValid()) {
            continue;
        }
        s.setHardwareValue(mac, QString(kRadioKey), legacyValue);
        ++migratedCount;
    }

    if (realRadios > 0 && !placeholderRadios) {
        s.remove(QString(kLegacyKey));
        qDebug() << "Migrated legacy Penny Ext Control setting (" << legacyValue
                 << ") to" << migratedCount << "saved radio(s); legacy global removed";
    } else {
        qDebug() << "Legacy Penny Ext Control setting (" << legacyValue
                 << "): no saved radio with a known MAC yet, or one still without it; "
                    "legacy global kept for next launch";
    }
    s.save();
}

// ---------------------------------------------------------------------------
// Issue #174: orphan-key cleanup for hardware/oc/n2adrFilter
// ---------------------------------------------------------------------------

void AppSettings::removeOrphanOcN2adrFilter(AppSettings& s)
{
    static constexpr auto kOrphanKey = QLatin1String("hardware/oc/n2adrFilter");
    if (!s.contains(QString(kOrphanKey))) {
        return;  // already clean (also the idempotent path)
    }
    s.remove(QString(kOrphanKey));
    qDebug() << "Removed orphan settings key" << QString(kOrphanKey)
             << "(issue #174 — OcOutputsHfTab N2ADR checkbox had no consumer)";
    s.save();
}

// ---------------------------------------------------------------------------
// R-R3-21: one-shot renames (see AppSettings.h)
// ---------------------------------------------------------------------------

void AppSettings::migrateRenamedKeys(AppSettings& s)
{
    struct Rename { const char* oldKey; const char* newKey; };
    static constexpr Rename kRenames[] = {
        {"WsjtxSpotLifetime", "WsjtxSpotLifetimeSec"},
    };
    bool changed = false;
    for (const Rename& r : kRenames) {
        const QString oldKey = QString::fromLatin1(r.oldKey);
        if (!s.contains(oldKey)) {
            continue;
        }
        const QString newKey = QString::fromLatin1(r.newKey);
        if (!s.contains(newKey)) {
            s.setValue(newKey, s.value(oldKey));
        }
        s.remove(oldKey);
        changed = true;
        qDebug() << "Renamed settings key" << oldKey << "to" << newKey;
    }
    if (changed) {
        s.save();
    }
}

// ---------------------------------------------------------------------------
// v0.3.0 / v0.3.x settings schema migrations
// ---------------------------------------------------------------------------

void AppSettings::ensureSettingsAtVersion(int currentVersion)
{
    const QString versionKey = QStringLiteral("SettingsSchemaVersion");
    const int storedVersion = value(versionKey, QStringLiteral("0")).toString().toInt();

    if (storedVersion >= currentVersion) {
        return;  // already at-or-past current version
    }

    // v0 → v3 migration (covers v0.2.x → v0.3.0)
    if (storedVersion < 3 && currentVersion >= 3) {
        qDebug() << "Migrating settings to schema v3 (NereusSDR v0.3.0)";

        // Retire keys whose semantics changed in v0.3.0:
        remove(QStringLiteral("DisplayAverageMode"));           // split into Detector + Averaging (Task 2.1)
        remove(QStringLiteral("DisplayPeakHold"));              // promoted to ActivePeakHold... (Task 2.5)
        remove(QStringLiteral("DisplayPeakHoldDelayMs"));       // → DisplayActivePeakHoldDurationMs (Task 2.5)
        remove(QStringLiteral("DisplayReverseWaterfallScroll")); // W5 removed (Task 2.8)

        qDebug() << "Settings migration to schema v3 complete";
    }

    // v3 → v4 migration (averaging-math fix). Retires the bare alpha key —
    // alpha is now derived per-side from millisecond time constants via
    // Thetis math (specHPSDR.cs:351-380 [v2.10.3.13]). Anyone with a v3
    // settings file gets default 30 ms / 120 ms (Thetis defaults) on next
    // load; the old alpha value is unrecoverable as a τ without knowing the
    // historical fps, and the math was wrong anyway.
    if (storedVersion < 4 && currentVersion >= 4) {
        qDebug() << "Migrating settings to schema v4 (averaging math fix)";
        remove(QStringLiteral("DisplayAverageAlpha"));
        qDebug() << "Settings migration to schema v4 complete";
    }

    // v4 → v5 migration (Thetis-faithful DSP-Options layout).
    //
    // Thetis's Display → DSP Options page exposes separate RX and TX combos
    // for buffer size and filter size on every mode that has TX (Phone, FM,
    // Digital — CW TX is firmware-handled per Thetis console.cs:38891-38897
    // [v2.10.3.13]).  NereusSDR collapsed those into single <Mode> keys
    // shared between RX and TX channels.  This migration splits them back:
    //
    //   DspOptionsBufferSize<Mode>   → <Mode>Rx + <Mode>Tx (preserved value)
    //   DspOptionsFilterSize<Mode>   → <Mode>Rx + <Mode>Tx (preserved value)
    //
    // Strategy: read old shared value, seed BOTH new keys with it, remove
    // the old key.  Existing customisation carries forward identically;
    // users can later differentiate RX vs TX in the new UI.  For CW we
    // only seed <Mode>Rx — there is no TX combo to populate.
    //
    // From Thetis radio.cs:519-574 / 2604-2662 [v2.10.3.13] — DSPRX/DSPTX
    // each persist BufferSize / FilterSize independently.
    if (storedVersion < 5 && currentVersion >= 5) {
        qDebug() << "Migrating settings to schema v5 (DSP-Options RX/TX split)";

        struct ModeSpec {
            QString modeKey;     // "Phone", "Cw", "Dig", "Fm"
            bool    hasTx;       // false for Cw — firmware-handled
        };
        const ModeSpec modes[] = {
            { QStringLiteral("Phone"), true  },
            { QStringLiteral("Cw"),    false },
            { QStringLiteral("Dig"),   true  },
            { QStringLiteral("Fm"),    true  },
        };

        auto split = [&](const QString& family, const ModeSpec& spec) {
            const QString oldKey = family + spec.modeKey;
            const QString rxKey  = family + spec.modeKey + QStringLiteral("Rx");
            const QString txKey  = family + spec.modeKey + QStringLiteral("Tx");
            if (contains(oldKey)) {
                const QString val = value(oldKey).toString();
                if (!contains(rxKey)) {
                    setValue(rxKey, val);
                }
                if (spec.hasTx && !contains(txKey)) {
                    setValue(txKey, val);
                }
                remove(oldKey);
            }
        };

        for (const auto& spec : modes) {
            split(QStringLiteral("DspOptionsBufferSize"), spec);
            split(QStringLiteral("DspOptionsFilterSize"), spec);
        }

        qDebug() << "Settings migration to schema v5 complete";
    }

    if (storedVersion < 6 && currentVersion >= 6) {
        // Phase 3F: schema v6 is additive only. No key renames, no defaults to populate.
        // New per-slice per-band keys (Slice<X>_Band_<band>_SampleRate, diversity keys, etc.)
        // are populated lazily by SliceModel::saveToSettings on first write. Operators with
        // existing v5 settings see no behavioural change until they touch the new controls.
        // See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §12.
    }

    // v6 -> v7 migration (R-R3-49). Setup > General > Options saved
    // NetworkWatchdogEnabled for years while nothing read it. R-R3-49 made
    // the key drive the radio's watchdog, so a value an operator saved
    // while the checkbox did nothing would come alive on upgrade with no
    // notice. Reset it once so every operator starts from the default (on);
    // a choice made on a v7 settings file is kept.
    if (storedVersion < 7 && currentVersion >= 7) {
        qDebug() << "Migrating settings to schema v7 (Network Watchdog reset)";
        remove(QStringLiteral("NetworkWatchdogEnabled"));
        qDebug() << "Settings migration to schema v7 complete";
    }

    // v7 -> v8 migration (R-R3-49). Setup > Network > TCI Server > Rate
    // limit was a messages-per-second box (TciRateLimitMsgsPerSec) that
    // nothing read. It is now Thetis's udTCIRateLimit, the shortest gap in
    // ms between frequency updates sent to each TCI app (TciRateLimitMs,
    // default 100). A number saved in the old unit means nothing in the new
    // one, so drop it once; every operator starts from the default.
    if (storedVersion < 8 && currentVersion >= 8) {
        qDebug() << "Migrating settings to schema v8 (TCI rate limit in ms)";
        remove(QStringLiteral("TciRateLimitMsgsPerSec"));
        qDebug() << "Settings migration to schema v8 complete";
    }

    // v8 -> v9 migration (R-IOS-06, R-IOS-27). NR1's ranges and defaults
    // became Thetis's NR spinboxes (ControlRanges.h: taps 1-1024, delay
    // 1-1023, gain 1-1000 x 1e-6, leak 1-1000 x 1e-3, defaults 64 / 16 /
    // 100 / 100). Every slice saved its NR1 values, so without this a saved
    // value outside the new range would run while the popup's slider showed
    // it pinned at an end. Once, per slice (Slice<N>/Nr1*):
    //   - a gain or leak exactly equal to the old default (16e-4, 10e-7,
    //     radio.cs's initialisers, never chosen by an operator) becomes the
    //     new default;
    //   - a value outside the range is clamped into it and written back;
    //   - a value inside the range stays as the operator set it.
    if (storedVersion < 9 && currentVersion >= 9) {
        qDebug() << "Migrating settings to schema v9 (NR1 ranges)";
        using namespace ControlRanges;
        struct Nr1Key {
            const char* suffix;
            const NrControl* control;
            double oldDefault;  // NaN where the default did not change
            bool whole;
        };
        const double none = std::nan("");
        const Nr1Key keys[] = {
            {"Nr1Taps", &kNr1Taps, none, true},
            {"Nr1Delay", &kNr1Delay, none, true},
            {"Nr1Gain", &kNr1Gain, 16e-4, false},
            {"Nr1Leakage", &kNr1Leak, 10e-7, false},
        };
        static const QRegularExpression sliceKey(
            QStringLiteral("^Slice\\d+/(Nr1Taps|Nr1Delay|Nr1Gain|Nr1Leakage)$"));
        for (const QString& key : allKeys()) {
            const QRegularExpressionMatch match = sliceKey.match(key);
            if (!match.hasMatch()) {
                continue;
            }
            for (const Nr1Key& entry : keys) {
                if (match.captured(1) != QLatin1String(entry.suffix)) {
                    continue;
                }
                bool ok = false;
                const double saved = value(key).toString().toDouble(&ok);
                const double low = entry.control->min * entry.control->scale;
                const double high = entry.control->max * entry.control->scale;
                double next = saved;
                if (!ok || !std::isfinite(saved)) {
                    next = entry.control->defaultValue;
                } else if (!std::isnan(entry.oldDefault) && saved == entry.oldDefault) {
                    next = entry.control->defaultValue;
                } else {
                    next = std::clamp(saved, low, high);
                }
                if (ok && next == saved) {
                    break;
                }
                if (entry.whole) {
                    setValue(key, static_cast<int>(std::lround(next)));
                } else {
                    setValue(key, next);
                }
                qDebug() << "Settings v9:" << key << "from" << saved << "to" << next;
                break;
            }
        }
        qDebug() << "Settings migration to schema v9 complete";
    }

    setValue(versionKey, QString::number(currentVersion));
}

} // namespace NereusSDR
