//=================================================================
// CATParser.cs
//=================================================================
// Copyright (C) 2012  Bob Tracy
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
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================
// Ported from Thetis Project Files/Source/Console/CAT/CATParser.cs
// (catalogue facts; independently implemented Qt loader).
// Modification history (NereusSDR):
// 2026-10-04 - C++20/Qt6 catalogue types and metadata loading by J.J. Boyd,
//              with AI-assisted transformation via Codex.
// Upstream parser dispatch is not ported here; catalogue facts and suffix
// categories follow CATParser.cs:475-590 [v2.10.3.15].

#include "CatCommandCatalog.h"
#include "core/LogCategories.h"
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QXmlStreamReader>
#include <algorithm>
#include <limits>
namespace NereusSDR {
CatCommandCatalog::CatCommandCatalog(const QString& path)
{
    m_valid = load(path);
    if (!m_valid) {
        m_descriptors.clear();
        m_maximumRequestBytes = 0;
        qCWarning(lcCat) << "CAT catalogue initialization failed:" << m_error;
    }
}
const CatDescriptor* CatCommandCatalog::find(const QByteArray& code) const
{
    for (const CatDescriptor& descriptor : m_descriptors) {
        if (descriptor.code == code) { return &descriptor; }
    }
    return nullptr;
}
bool CatCommandCatalog::load(const QString& path)
{
    QFile metadata(QStringLiteral(":/cat/CommandContracts.json"));
    if (!metadata.open(QIODevice::ReadOnly)) {
        m_error = metadata.errorString();
        return false;
    }
    QJsonParseError jsonError;
    const QJsonDocument document = QJsonDocument::fromJson(metadata.readAll(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !document.isArray()) {
        m_error = QStringLiteral("Invalid CAT contract metadata");
        return false;
    }
    QHash<QByteArray, QJsonObject> contracts;
    for (const QJsonValue& value : document.array()) {
        const QJsonObject object = value.toObject();
        const QByteArray code = object.value(QStringLiteral("code")).toString().toLatin1();
        if (code.isEmpty() || contracts.contains(code)) {
            m_error = QStringLiteral("Missing or duplicate CAT contract code");
            return false;
        }
        contracts.insert(code, object);
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = file.errorString();
        return false;
    }
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement() || xml.name() != QStringLiteral("catstructs")) {
        m_error = QStringLiteral("Missing CAT catalogue root");
        return false;
    }
    QSet<QByteArray> codes;
    // From Thetis CAT/CATParser.cs:475-500 [v2.10.3.15]. Independently
    // implemented streaming loader; all fields are required, and activity is
    // trimmed before conversion to preserve .NET's whitespace handling (ZZHW).
    while (xml.readNextStartElement()) {
        if (xml.name() != QStringLiteral("catstruct")) {
            xml.raiseError(QStringLiteral("Unexpected catalogue element"));
            break;
        }
        CatDescriptor descriptor;
        descriptor.code = xml.attributes().value(QStringLiteral("code")).toLatin1();
        QSet<QString> fields;
        while (xml.readNextStartElement()) {
            const QString name = xml.name().toString();
            const QString text = xml.readElementText().trimmed();
            if (fields.contains(name)) { xml.raiseError(QStringLiteral("Duplicate descriptor field")); break; }
            fields.insert(name);
            if (name == QStringLiteral("active")) {
                if (text.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
                    descriptor.active = true;
                } else if (text.compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0) {
                    xml.raiseError(QStringLiteral("Invalid activity flag"));
                }
            } else if (name == QStringLiteral("nsetparms") || name == QStringLiteral("ngetparms") || name == QStringLiteral("nansparms")) {
                bool ok = false;
                // From Thetis CAT/CATParser.cs:491-497 [v2.10.3.15]: Convert.ToInt16.
                const int width = text.toInt(&ok);
                if (!ok || width < std::numeric_limits<qint16>::min() || width > std::numeric_limits<qint16>::max()) { xml.raiseError(QStringLiteral("Invalid descriptor width")); break; }
                if (name == QStringLiteral("nsetparms")) { descriptor.setWidth = width; }
                if (name == QStringLiteral("ngetparms")) { descriptor.getWidth = width; }
                if (name == QStringLiteral("nansparms")) { descriptor.answerWidth = width; }
            } else if (name != QStringLiteral("desc")) {
                xml.raiseError(QStringLiteral("Unexpected descriptor field"));
            }
        }
        const bool required = fields.contains(QStringLiteral("active")) && fields.contains(QStringLiteral("nsetparms"))
            && fields.contains(QStringLiteral("ngetparms")) && fields.contains(QStringLiteral("nansparms"));
        const bool validCode = (descriptor.code.size() == 2 || (descriptor.code.size() == 4 && descriptor.code.startsWith("ZZ")))
            && std::all_of(descriptor.code.cbegin(), descriptor.code.cend(), [](char c) { return c >= 'A' && c <= 'Z'; });
        if (xml.hasError() || !required || !validCode || codes.contains(descriptor.code) || !contracts.contains(descriptor.code)) {
            m_error = QStringLiteral("Invalid or incomplete CAT descriptor %1: %2").arg(QString::fromLatin1(descriptor.code), xml.errorString());
            return false;
        }
        const QJsonObject contract = contracts.take(descriptor.code);
        const QString outcome = contract.value(QStringLiteral("outcome")).toString();
        const QHash<QString, CatOutcome> outcomes{{QStringLiteral("Faithful"), CatOutcome::Faithful}, {QStringLiteral("Adapted"), CatOutcome::Adapted},
            {QStringLiteral("Inactive"), CatOutcome::Inactive}, {QStringLiteral("SourceInert"), CatOutcome::SourceInert}, {QStringLiteral("Unavailable"), CatOutcome::Unavailable}};
        if (!outcomes.contains(outcome)) { m_error = QStringLiteral("Invalid CAT outcome"); return false; }
        descriptor.outcome = outcomes.value(outcome);
        descriptor.family = contract.value(QStringLiteral("family")).toString();
        const QString suffix = contract.value(QStringLiteral("suffixKind")).toString();
        const QHash<QString, CatSuffixKind> suffixKinds{{QStringLiteral("Numeric"), CatSuffixKind::Numeric}, {QStringLiteral("Text"), CatSuffixKind::Text},
            {QStringLiteral("Equalizer"), CatSuffixKind::Equalizer}, {QStringLiteral("Guid"), CatSuffixKind::Guid}};
        if (!suffixKinds.contains(suffix) || descriptor.family.isEmpty() || (descriptor.active == (descriptor.outcome == CatOutcome::Inactive))) {
            m_error = QStringLiteral("Invalid CAT metadata"); return false;
        }
        descriptor.suffixKind = suffixKinds.value(suffix);
        const QString precedence = contract.value(QStringLiteral("precedence")).toString();
        if (precedence != QStringLiteral("GetFirst") && precedence != QStringLiteral("SetFirst")) {
            m_error = QStringLiteral("Invalid CAT form precedence");
            return false;
        }
        descriptor.formPrecedence = precedence == QStringLiteral("GetFirst") ? CatFormPrecedence::GetFirst : CatFormPrecedence::SetFirst;
        codes.insert(descriptor.code);
        if (descriptor.active) {
            const int suffixWidth = std::max(descriptor.setWidth, descriptor.getWidth);
            if (suffixWidth >= 0) { m_maximumRequestBytes = std::max(m_maximumRequestBytes, descriptor.code.size() + suffixWidth + 1); }
        }
        m_descriptors.append(descriptor);
    }
    while (!xml.atEnd()) { xml.readNext(); }
    if (xml.hasError() || m_descriptors.isEmpty() || !contracts.isEmpty()) {
        m_error = QStringLiteral("Incomplete CAT catalogue: %1").arg(xml.errorString());
        return false;
    }
    return true;
}
} // namespace NereusSDR
