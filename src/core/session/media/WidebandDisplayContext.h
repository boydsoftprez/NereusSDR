// =================================================================
// no-port-check: NereusSDR-original. Negotiated ADC display metadata;
// separate from the DDC source and the codec's DDC-only wide history row.
// =================================================================
#pragma once

#include <QJsonObject>
#include <QtGlobal>

#include <optional>

namespace NereusSDR {

struct WidebandDisplayContext {
    bool available{false};
    bool active{false};
    int physicalAdcIndex{-1};
    int filterChainIndex{-1};
    quint32 sourceGeneration{0};
    double adcRateHz{0.0};

    bool operator==(const WidebandDisplayContext&) const = default;
    bool valid() const;
    QJsonObject toJson() const;
    static std::optional<WidebandDisplayContext> fromJson(const QJsonObject& object);
};

} // namespace NereusSDR
