#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <cmath>

namespace NereusSDR::SettingsBackupExportWire {

// SessionMessages decodes generic commands, whose ordinal and command ID
// conversions predate this security-sensitive verb. Check the raw JSON too,
// before its narrowed MirrorUpdate/commandId values can hide a fraction.
inline bool strictEnvelope(const QByteArray& wire, bool result)
{
    if (wire.size() >= 1024 * 1024) return false;
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(wire, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QJsonObject object = doc.object();
    const QJsonValue id = object.value(QStringLiteral("id"));
    if (!id.isDouble() || !std::isfinite(id.toDouble()) || id.toDouble() < 1
        || id.toDouble() > 4294967295.0 || std::floor(id.toDouble()) != id.toDouble()) {
        return false;
    }
    if (!object.value(QStringLiteral("verb")).isString()) return false;
    if (result && (!object.value(QStringLiteral("accepted")).isBool()
        || !object.value(QStringLiteral("reason")).isString()
        || !object.value(QStringLiteral("affected")).isArray())) return false;
    const QJsonValue fields = object.value(result ? QStringLiteral("values")
                                                   : QStringLiteral("args"));
    if (!result && !fields.isArray()) return false;
    if (result && !fields.isUndefined() && !fields.isArray()) return false;
    for (const QJsonValue& value : fields.toArray()) {
        if (!value.isObject()) return false;
        const QJsonValue ordinal = value.toObject().value(QStringLiteral("ordinal"));
        if (!ordinal.isDouble() || ordinal.toDouble() != 0.0) return false;
    }
    return true;
}

} // namespace NereusSDR::SettingsBackupExportWire
