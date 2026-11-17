#include "core/TxProfileFile.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace NereusSDR::TxProfileFile {
namespace {

void setError(QString* error, const QString& text)
{
    if (error) { *error = text; }
}

bool takeKeys(const QJsonObject& object, QHash<QString, QString>* keys, QString* error)
{
    if (!keys) {
        setError(error, QStringLiteral("The transmit profile was not understood."));
        return false;
    }
    keys->clear();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!it.value().isString()) {
            setError(error, QStringLiteral("A setting in the transmit profile is not text."));
            return false;
        }
        keys->insert(it.key(), it.value().toString());
    }
    return true;
}

} // namespace

bool parseKeys(const QString& keysJson, QHash<QString, QString>* keys, QString* error)
{
    if (keysJson.toUtf8().size() > kMaxBytes) {
        setError(error, QStringLiteral("The transmit profile is too large."));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(keysJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(error, QStringLiteral("The transmit profile was not understood."));
        return false;
    }
    return takeKeys(doc.object(), keys, error);
}

bool encode(const Document& document, QByteArray* output, QString* error)
{
    if (!output) {
        setError(error, QStringLiteral("The transmit profile was not understood."));
        return false;
    }
    if (document.name.trimmed().isEmpty()) {
        setError(error, QStringLiteral("Give the transmit profile a name."));
        return false;
    }
    QJsonObject keys;
    for (auto it = document.keys.constBegin(); it != document.keys.constEnd(); ++it) {
        keys.insert(it.key(), it.value());
    }
    const QJsonObject root{
        {QStringLiteral("nereusTxProfileVersion"), kVersion},
        {QStringLiteral("name"), document.name},
        {QStringLiteral("keys"), keys},
    };
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (bytes.size() > kMaxBytes) {
        setError(error, QStringLiteral("The transmit profile is too large."));
        return false;
    }
    *output = bytes;
    return true;
}

bool decode(const QByteArray& input, Document* output, QString* error)
{
    if (!output) {
        setError(error, QStringLiteral("The transmit profile was not understood."));
        return false;
    }
    if (input.size() > kMaxBytes) {
        setError(error, QStringLiteral("The transmit profile is too large."));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(input, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(error, QStringLiteral("This file is not a transmit profile."));
        return false;
    }
    const QJsonObject root = doc.object();
    const QJsonValue version = root.value(QStringLiteral("nereusTxProfileVersion"));
    if (!version.isDouble() || version.toInt() != kVersion || version.toDouble() != double(kVersion)) {
        setError(error, version.toInt() > kVersion
            ? QStringLiteral("This transmit profile is from a newer NereusSDR. "
                             "Update this app to import it.")
            : QStringLiteral("This file is not a transmit profile."));
        return false;
    }
    const QJsonValue name = root.value(QStringLiteral("name"));
    if (!name.isString() || name.toString().trimmed().isEmpty()) {
        setError(error, QStringLiteral("Give the transmit profile a name."));
        return false;
    }
    const QJsonValue keys = root.value(QStringLiteral("keys"));
    if (!keys.isObject()) {
        setError(error, QStringLiteral("The transmit profile has no settings."));
        return false;
    }
    QHash<QString, QString> parsed;
    if (!takeKeys(keys.toObject(), &parsed, error)) {
        return false;
    }
    output->name = name.toString();
    output->keys = parsed;
    return true;
}

} // namespace NereusSDR::TxProfileFile
