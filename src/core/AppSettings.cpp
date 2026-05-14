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

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

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

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString tag = xml.name().toString();
            if (tag == QStringLiteral("NereusSDR")) {
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
        if (parseSettingsXml(sanitized, m_settings, m_stationSettings, m_stationName)) {
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
        // ReadResult::Missing — first-run path. No corruption, no .bak attempt.
        qDebug() << "No settings file found at" << m_filePath << "— using defaults";
        return;
    }

    // ── Corruption path ──────────────────────────────────────────────────
    //
    // Preserve the corrupt file BEFORE doing anything else so the user
    // (or a developer) can attempt manual recovery. The next save() would
    // otherwise overwrite the only remaining copy with factory defaults
    // (the exact failure mode reported in issue #241).
    const QString stamp = QDateTime::currentDateTime().toString(
                              QStringLiteral("yyyyMMdd-HHmmss"));
    const QString corruptPath = m_filePath + QStringLiteral(".corrupt-") + stamp;
    if (QFile::rename(m_filePath, corruptPath)) {
        m_wasCorruptedOnLoad      = true;
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

    // Try the .bak fallback. If it exists and parses cleanly we restore
    // the previous good state; otherwise we leave m_settings empty and
    // proceed with defaults.
    if (QFileInfo::exists(bakPath)) {
        QString bakXml;
        const ReadResult bakRead = readFileForParse(bakPath, bakXml);
        if (bakRead == ReadResult::Ok) {
            const QString sanitized = sanitizeXmlForLoad(bakXml);
            if (parseSettingsXml(sanitized, m_settings, m_stationSettings, m_stationName)) {
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
    m_settings.clear();
    m_stationSettings.clear();
}

void AppSettings::save()
{
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
        return;
    }

    {
        QXmlStreamWriter xml(&file);
        xml.setAutoFormatting(true);
        xml.writeStartDocument();
        xml.writeStartElement(QStringLiteral("NereusSDR"));

        // Write top-level settings (encode keys so XML element names are valid)
        for (auto it = m_settings.constBegin(); it != m_settings.constEnd(); ++it) {
            xml.writeTextElement(encodeXmlKey(it.key()), it.value());
        }

        // Write station settings
        if (!m_stationSettings.isEmpty()) {
            xml.writeStartElement(m_stationName);
            xml.writeAttribute(QStringLiteral("type"), QStringLiteral("station"));
            for (auto it = m_stationSettings.constBegin(); it != m_stationSettings.constEnd(); ++it) {
                xml.writeTextElement(encodeXmlKey(it.key()), it.value());
            }
            xml.writeEndElement();
        }

        xml.writeEndElement(); // NereusSDR
        xml.writeEndDocument();
    }

    if (!file.commit()) {
        qWarning() << "Could not commit settings to" << m_filePath
                   << ":" << file.errorString();
        return;
    }

    QFile::setPermissions(m_filePath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QVariant AppSettings::value(const QString& key, const QVariant& defaultValue) const
{
    auto it = m_settings.constFind(key);
    if (it != m_settings.constEnd()) {
        return QVariant(it.value());
    }
    return defaultValue;
}

void AppSettings::setValue(const QString& key, const QVariant& val)
{
    m_settings.insert(key, val.toString());
}

void AppSettings::remove(const QString& key)
{
    m_settings.remove(key);
}

bool AppSettings::contains(const QString& key) const
{
    return m_settings.contains(key);
}

QStringList AppSettings::allKeys() const
{
    return m_settings.keys();
}

void AppSettings::clear()
{
    m_settings.clear();
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
    m_settings.insert(prefix + QStringLiteral("name"),            info.name);
    m_settings.insert(prefix + QStringLiteral("ipAddress"),       info.address.toString());
    m_settings.insert(prefix + QStringLiteral("port"),            QString::number(info.port));
    m_settings.insert(prefix + QStringLiteral("macAddress"),      info.macAddress);
    m_settings.insert(prefix + QStringLiteral("boardType"),
                      QString::number(static_cast<int>(info.boardType)));
    m_settings.insert(prefix + QStringLiteral("protocol"),
                      QString::number(static_cast<int>(info.protocol)));
    m_settings.insert(prefix + QStringLiteral("firmwareVersion"), QString::number(info.firmwareVersion));
    m_settings.insert(prefix + QStringLiteral("pinToMac"),        pinToMac   ? QStringLiteral("True") : QStringLiteral("False"));
    m_settings.insert(prefix + QStringLiteral("autoConnect"),     autoConnect ? QStringLiteral("True") : QStringLiteral("False"));
    m_settings.insert(prefix + QStringLiteral("lastSeen"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    // Model override (Phase 3I-RP). FIRST = no override.
    if (info.modelOverride != HPSDRModel::FIRST) {
        m_settings.insert(prefix + QStringLiteral("modelOverride"),
                          QString::number(static_cast<int>(info.modelOverride)));
    }
}

void AppSettings::forgetRadio(const QString& macKey)
{
    const QString prefix = radioKeyPrefix(macKey);
    // Remove all keys with this prefix
    const QStringList keys = m_settings.keys();
    for (const QString& k : keys) {
        if (k.startsWith(prefix)) {
            m_settings.remove(k);
        }
    }
}

void AppSettings::clearSavedRadios()
{
    // Remove all radios/<key>/<field> entries (but preserve lastConnected, discoveryProfile)
    const QStringList keys = m_settings.keys();
    for (const QString& k : keys) {
        if (!k.startsWith(QStringLiteral("radios/"))) {
            continue;
        }
        // Only remove keys that have a per-radio sub-path (3 segments: radios/<mac>/<field>)
        const QString rest = k.mid(7); // strip "radios/"
        if (rest.contains(QLatin1Char('/'))) {
            m_settings.remove(k);
        }
    }
}

QList<SavedRadio> AppSettings::savedRadios() const
{
    // Collect all distinct macKeys
    QSet<QString> macKeys;
    for (auto it = m_settings.constBegin(); it != m_settings.constEnd(); ++it) {
        const QString mk = macKeyFromSettingsKey(it.key());
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
    if (!m_settings.contains(nameKey)) {
        return std::nullopt;
    }

    SavedRadio sr;

    // RadioInfo fields
    sr.info.name            = m_settings.value(prefix + QStringLiteral("name"));
    sr.info.address         = QHostAddress(m_settings.value(prefix + QStringLiteral("ipAddress")));
    sr.info.port            = static_cast<quint16>(
                                m_settings.value(prefix + QStringLiteral("port"),
                                                 QStringLiteral("1024")).toUInt());
    sr.info.macAddress      = m_settings.value(prefix + QStringLiteral("macAddress"));
    sr.info.boardType       = static_cast<HPSDRHW>(
                                m_settings.value(prefix + QStringLiteral("boardType"),
                                                 QStringLiteral("999")).toInt());
    sr.info.protocol        = static_cast<ProtocolVersion>(
                                m_settings.value(prefix + QStringLiteral("protocol"),
                                                 QStringLiteral("1")).toInt());
    sr.info.firmwareVersion = m_settings.value(prefix + QStringLiteral("firmwareVersion"),
                                               QStringLiteral("0")).toInt();

    // Saved-only flags
    sr.pinToMac    = (m_settings.value(prefix + QStringLiteral("pinToMac"),
                                       QStringLiteral("False")) == QStringLiteral("True"));
    sr.autoConnect = (m_settings.value(prefix + QStringLiteral("autoConnect"),
                                       QStringLiteral("False")) == QStringLiteral("True"));

    const QString lastSeenStr = m_settings.value(prefix + QStringLiteral("lastSeen"));
    if (!lastSeenStr.isEmpty()) {
        sr.lastSeen = QDateTime::fromString(lastSeenStr, Qt::ISODate);
    }

    // Model override (Phase 3I-RP)
    const QString moStr = m_settings.value(prefix + QStringLiteral("modelOverride"),
                                            QStringLiteral("-1"));
    int moInt = moStr.toInt();
    if (moInt > static_cast<int>(HPSDRModel::FIRST) &&
        moInt < static_cast<int>(HPSDRModel::LAST)) {
        sr.info.modelOverride = static_cast<HPSDRModel>(moInt);
    }

    return sr;
}

QString AppSettings::lastConnected() const
{
    return m_settings.value(QStringLiteral("radios/lastConnected"));
}

void AppSettings::setLastConnected(const QString& macKey)
{
    if (macKey.isEmpty()) {
        m_settings.remove(QStringLiteral("radios/lastConnected"));
    } else {
        m_settings.insert(QStringLiteral("radios/lastConnected"), macKey);
    }
}

DiscoveryProfile AppSettings::discoveryProfile() const
{
    // Default to SafeDefault (4)
    const int v = m_settings.value(QStringLiteral("radios/discoveryProfile"),
                                   QStringLiteral("4")).toInt();
    return static_cast<DiscoveryProfile>(v);
}

void AppSettings::setDiscoveryProfile(DiscoveryProfile p)
{
    m_settings.insert(QStringLiteral("radios/discoveryProfile"),
                      QString::number(static_cast<int>(p)));
}

// ---------------------------------------------------------------------------
// Hardware tab persistence (Phase 3I Task 21)
// ---------------------------------------------------------------------------

void AppSettings::setHardwareValue(const QString& mac, const QString& key, const QVariant& value)
{
    const QString fullKey = QStringLiteral("hardware/%1/%2").arg(mac, key);
    m_settings.insert(fullKey, value.toString());
}

QVariant AppSettings::hardwareValue(const QString& mac, const QString& key,
                                     const QVariant& defaultValue) const
{
    const QString fullKey = QStringLiteral("hardware/%1/%2").arg(mac, key);
    auto it = m_settings.constFind(fullKey);
    if (it != m_settings.constEnd()) {
        return QVariant(it.value());
    }
    return defaultValue;
}

QMap<QString, QVariant> AppSettings::hardwareValues(const QString& mac) const
{
    const QString prefix = QStringLiteral("hardware/%1/").arg(mac);
    QMap<QString, QVariant> result;
    for (auto it = m_settings.constBegin(); it != m_settings.constEnd(); ++it) {
        if (it.key().startsWith(prefix)) {
            const QString bareKey = it.key().mid(prefix.size());
            result.insert(bareKey, QVariant(it.value()));
        }
    }
    return result;
}

void AppSettings::clearHardwareValues(const QString& mac)
{
    const QString prefix = QStringLiteral("hardware/%1/").arg(mac);
    const QStringList keys = m_settings.keys();
    for (const QString& k : keys) {
        if (k.startsWith(prefix)) {
            m_settings.remove(k);
        }
    }
}

// ---------------------------------------------------------------------------
// Model override (Phase 3I-RP)
// ---------------------------------------------------------------------------

HPSDRModel AppSettings::modelOverride(const QString& macKey) const
{
    const QString prefix = radioKeyPrefix(macKey);
    const QString val = m_settings.value(prefix + QStringLiteral("modelOverride"),
                                          QStringLiteral("-1"));
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
    m_settings.insert(prefix + QStringLiteral("modelOverride"),
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
        if (r.info.boardType != HPSDRHW::HermesLite) {
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

    setValue(versionKey, QString::number(currentVersion));
}

} // namespace NereusSDR
