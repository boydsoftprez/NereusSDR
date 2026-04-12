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
