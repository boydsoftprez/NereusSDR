#pragma once
// no-port-check: NereusSDR-original typed scalar command payload adapter.
#include "MirrorSchema.h"
#include <QVariantMap>
#include <cmath>
#include <limits>
#include <optional>

namespace NereusSDR {

inline std::optional<QList<MirrorUpdate>> dspCommandValues(const QVariantMap& values)
{
    if (values.size() > 128) {
        return std::nullopt;
    }
    QList<MirrorUpdate> result;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (it.key().isEmpty() || it.key().size() > 128) {
            return std::nullopt;
        }
        MirrorWireKind kind;
        QVariant value = it.value();
        switch (value.metaType().id()) {
        case QMetaType::Bool:
            kind = MirrorWireKind::Bool;
            break;
        case QMetaType::QString:
            kind = MirrorWireKind::Utf8;
            break;
        case QMetaType::Int:
        case QMetaType::LongLong:
        case QMetaType::UInt:
            kind = MirrorWireKind::Int64;
            value = value.toLongLong();
            break;
        case QMetaType::ULongLong:
            if (value.toULongLong() > static_cast<qulonglong>(std::numeric_limits<qlonglong>::max())) {
                return std::nullopt;
            }
            kind = MirrorWireKind::Int64;
            value = value.toLongLong();
            break;
        case QMetaType::Double:
            if (!std::isfinite(value.toDouble())) {
                return std::nullopt;
            }
            kind = MirrorWireKind::Float64;
            break;
        default:
            return std::nullopt;
        }
        result.append({0, it.key().toUtf8(), kind, value});
    }
    return result;
}

inline std::optional<QVariantMap> dspCommandValues(const QList<MirrorUpdate>& values)
{
    if (values.size() > 128) {
        return std::nullopt;
    }
    QVariantMap result;
    for (const auto& update : values) {
        const QString name = QString::fromUtf8(update.name);
        if (name.isEmpty() || name.size() > 128 || name.toUtf8() != update.name || result.contains(name)
            || update.kind == MirrorWireKind::Unsupported || update.kind == MirrorWireKind::Enum) {
            return std::nullopt;
        }
        const auto typed = dspCommandValues(QVariantMap{{name, update.value}});
        if (!typed || typed->first().kind != update.kind) {
            return std::nullopt;
        }
        result.insert(name, update.value);
    }
    return result;
}

} // namespace NereusSDR
