// =================================================================
// src/core/mmio/FormatParser.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

#include "FormatParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QXmlStreamReader>

namespace NereusSDR::FormatParser {

namespace {

// Build a "<guid>.<dotted.path>" key for a given path segment list.
static QString makeKey(const QString& guidStr, const QStringList& path)
{
    if (path.isEmpty()) { return guidStr; }
    return guidStr + QLatin1Char('.') + path.join(QLatin1Char('.'));
}

// Recursive JSON walker. Each leaf writes one entry into out.
static void walkJson(const QJsonValue& value,
                     QStringList& path,
                     const QString& guidStr,
                     QHash<QString, QVariant>& out)
{
    if (value.isObject()) {
        const QJsonObject obj = value.toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            path.append(it.key());
            walkJson(it.value(), path, guidStr, out);
            path.removeLast();
        }
    } else if (value.isArray()) {
        const QJsonArray arr = value.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            path.append(QString::number(i));
            walkJson(arr.at(i), path, guidStr, out);
            path.removeLast();
        }
    } else if (value.isDouble()) {
        const double d = value.toDouble();
        // Match Thetis's behavior: prefer int if the value is a whole
        // number within int range, otherwise double.
        if (d == static_cast<double>(static_cast<qint64>(d))
            && d >= INT_MIN && d <= INT_MAX) {
            out.insert(makeKey(guidStr, path), static_cast<int>(d));
        } else {
            out.insert(makeKey(guidStr, path), d);
        }
    } else if (value.isBool()) {
        out.insert(makeKey(guidStr, path), value.toBool());
    } else if (value.isString()) {
        out.insert(makeKey(guidStr, path), value.toString());
    }
    // isNull / isUndefined → skip, matching Thetis.
}

} // namespace

QHash<QString, QVariant> parseJson(const QByteArray& payload, const QUuid& endpointGuid)
{
    QHash<QString, QVariant> out;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError) {
        return out;   // silent skip on parse failure, Thetis behavior
    }

    const QString guidStr = endpointGuid.toString(QUuid::WithoutBraces);
    QStringList path;
    if (doc.isObject()) {
        walkJson(QJsonValue(doc.object()), path, guidStr, out);
    } else if (doc.isArray()) {
        walkJson(QJsonValue(doc.array()), path, guidStr, out);
    }
    return out;
}

QHash<QString, QVariant> parseXml(const QByteArray& payload, const QUuid& endpointGuid)
{
    QHash<QString, QVariant> out;
    const QString guidStr = endpointGuid.toString(QUuid::WithoutBraces);
    QXmlStreamReader xml(payload);
    QStringList path;

    while (!xml.atEnd() && !xml.hasError()) {
        const auto token = xml.readNext();
        switch (token) {
        case QXmlStreamReader::StartElement: {
            path.append(xml.name().toString());
            // Attributes as keys under the current element path.
            const auto attrs = xml.attributes();
            for (const auto& a : attrs) {
                QStringList attrPath = path;
                attrPath.append(a.name().toString());
                out.insert(makeKey(guidStr, attrPath), a.value().toString());
            }
            break;
        }
        case QXmlStreamReader::Characters: {
            if (!xml.isWhitespace() && !path.isEmpty()) {
                const QString text = xml.text().toString().trimmed();
                if (!text.isEmpty()) {
                    out.insert(makeKey(guidStr, path), text);
                }
            }
            break;
        }
        case QXmlStreamReader::EndElement:
            if (!path.isEmpty()) { path.removeLast(); }
            break;
        default:
            break;
        }
    }
    // silent on parse errors, Thetis behavior
    return out;
}

QHash<QString, QVariant> parseRaw(const QByteArray& payload, const QUuid& endpointGuid)
{
    QHash<QString, QVariant> out;
    const QString guidStr = endpointGuid.toString(QUuid::WithoutBraces);

    // Split on colon into alternating key/value tokens.
    // Thetis MeterManager.cs:41344-41349.
    const QByteArrayList parts = payload.split(':');
    for (int i = 0; i + 1 < parts.size(); i += 2) {
        const QString key = QString::fromUtf8(parts.at(i).trimmed());
        const QString val = QString::fromUtf8(parts.at(i + 1).trimmed());
        if (key.isEmpty()) { continue; }
        out.insert(guidStr + QLatin1Char('.') + key, val);
    }
    return out;
}

} // namespace NereusSDR::FormatParser
