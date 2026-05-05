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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
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
#ifdef Q_OS_MAC
    const QString root = QDir::homePath() + QStringLiteral("/Library/Preferences/NereusSDR");
#else
    const QString root = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                         + QStringLiteral("/NereusSDR");
#endif
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

void AppSettings::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "No settings file found at" << m_filePath << "— using defaults";
        return;
    }

    // Read the entire file as UTF-8 text and run it through the recovery
    // sanitizer before parsing. Pre-2026-04-30 builds wrote element names
    // with literal '+' (and other non-NameChars from Thetis profile names),
    // which causes QXmlStreamReader to bail mid-file. The sanitizer escapes
    // those chars structurally; well-formed files pass through unchanged.
    const QString rawXml      = QString::fromUtf8(file.readAll());
    const QString sanitizedXml = sanitizeXmlForLoad(rawXml);
    file.close();

    QXmlStreamReader xml(sanitizedXml);
    QString currentStation;
    bool inStation = false;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QString tag = xml.name().toString();
            if (tag == "NereusSDR") {
                continue;
            }
            // Check if this is a station section
            if (!inStation && xml.attributes().hasAttribute(QStringLiteral("type"))
                && xml.attributes().value(QStringLiteral("type")) == QStringLiteral("station")) {
                inStation = true;
                currentStation = tag;
                m_stationName = tag;
                continue;
            }
            // Read element text as value; decode key back to internal format
            const QString value = xml.readElementText();
            const QString key   = decodeXmlKey(tag);
            if (inStation) {
                m_stationSettings.insert(key, value);
            } else {
                m_settings.insert(key, value);
            }
        } else if (xml.isEndElement() && inStation) {
            if (xml.name().toString() == currentStation) {
                inStation = false;
            }
        }
    }

    if (xml.hasError()) {
        qWarning() << "XML parse error in settings:" << xml.errorString();
    }

    int radiosCount = 0;
    QStringList radioMacs;
    for (auto it = m_settings.constBegin(); it != m_settings.constEnd(); ++it) {
        if (it.key().startsWith(QStringLiteral("radios/"))) {
            radiosCount++;
            // Capture distinct MAC keys for the diagnostic
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
    qDebug() << "Loaded" << m_settings.size() << "settings,"
             << m_stationSettings.size() << "station settings;"
             << radiosCount << "saved-radio keys across"
             << radioMacs.size() << "MAC(s)";
}

void AppSettings::save()
{
    // Ensure directory exists
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Could not save settings to" << m_filePath;
        return;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("NereusSDR");

    // Write top-level settings (encode keys so XML element names are valid)
    for (auto it = m_settings.constBegin(); it != m_settings.constEnd(); ++it) {
        xml.writeTextElement(encodeXmlKey(it.key()), it.value());
    }

    // Write station settings
    if (!m_stationSettings.isEmpty()) {
        xml.writeStartElement(m_stationName);
        xml.writeAttribute("type", "station");
        for (auto it = m_stationSettings.constBegin(); it != m_stationSettings.constEnd(); ++it) {
            xml.writeTextElement(encodeXmlKey(it.key()), it.value());
        }
        xml.writeEndElement();
    }

    xml.writeEndElement(); // NereusSDR
    xml.writeEndDocument();

    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
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

} // namespace NereusSDR
