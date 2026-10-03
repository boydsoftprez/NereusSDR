// =================================================================
// no-port-check: NereusSDR-original. Strict negotiated ADC display context.
// =================================================================
#include "core/session/media/WidebandDisplayContext.h"

#include <cmath>
#include <limits>

namespace NereusSDR {
namespace {
bool number(const QJsonObject& object, const char* key, double& result)
{
    const auto value = object.value(QLatin1String(key));
    if (!value.isDouble()) { return false; }
    result = value.toDouble();
    return std::isfinite(result);
}

bool integer(const QJsonObject& object, const char* key, double maximum, double& result)
{
    return number(object, key, result) && result >= 0.0 && result <= maximum
        && result == std::floor(result);
}
} // namespace

bool WidebandDisplayContext::valid() const
{
    if (!available) {
        return !active && physicalAdcIndex == -1 && filterChainIndex == -1
            && sourceGeneration == 0 && adcRateHz == 0.0;
    }
    return physicalAdcIndex >= 0 && physicalAdcIndex < 2
        && filterChainIndex >= 0 && filterChainIndex < 2
        && std::isfinite(adcRateHz) && adcRateHz > 0.0 && adcRateHz / 2.0 > 0.0
        && (active ? sourceGeneration != 0 : sourceGeneration == 0);
}

QJsonObject WidebandDisplayContext::toJson() const
{
    if (!valid()) { return {}; }
    QJsonObject result{{QStringLiteral("version"), 1},
                       {QStringLiteral("available"), available},
                       {QStringLiteral("active"), active}};
    if (available) {
        result.insert(QStringLiteral("physicalAdcIndex"), physicalAdcIndex);
        result.insert(QStringLiteral("filterChainIndex"), filterChainIndex);
        result.insert(QStringLiteral("sourceGeneration"), qint64(sourceGeneration));
        result.insert(QStringLiteral("adcRateHz"), adcRateHz);
        result.insert(QStringLiteral("geometryRateBasis"), QStringLiteral("thetisLocalReference"));
        result.insert(QStringLiteral("lowHz"), 0.0);
        result.insert(QStringLiteral("highHz"), adcRateHz / 2.0);
        result.insert(QStringLiteral("levelReference"), QStringLiteral("localWingRelativeWithStationRxOffset"));
    }
    return result;
}

std::optional<WidebandDisplayContext> WidebandDisplayContext::fromJson(const QJsonObject& object)
{
    double version = 0.0;
    if (!integer(object, "version", 1, version) || version != 1
        || !object.value(QStringLiteral("available")).isBool()
        || !object.value(QStringLiteral("active")).isBool()) { return std::nullopt; }
    WidebandDisplayContext context;
    context.available = object.value(QStringLiteral("available")).toBool();
    context.active = object.value(QStringLiteral("active")).toBool();
    if (!context.available) {
        return object.size() == 3 && context.valid()
            ? std::optional(context) : std::nullopt;
    }
    double adc = 0.0, chain = 0.0, generation = 0.0, low = 0.0, high = 0.0;
    if (object.size() != 11 || !integer(object, "physicalAdcIndex", 1, adc)
        || !integer(object, "filterChainIndex", 1, chain)
        || !integer(object, "sourceGeneration", std::numeric_limits<quint32>::max(), generation)
        || !number(object, "adcRateHz", context.adcRateHz)
        || !number(object, "lowHz", low) || !number(object, "highHz", high)
        || low != 0.0 || high != context.adcRateHz / 2.0
        || object.value(QStringLiteral("geometryRateBasis")) != QStringLiteral("thetisLocalReference")
        || object.value(QStringLiteral("levelReference")) != QStringLiteral("localWingRelativeWithStationRxOffset")) {
        return std::nullopt;
    }
    context.physicalAdcIndex = int(adc);
    context.filterChainIndex = int(chain);
    context.sourceGeneration = quint32(generation);
    return context.valid() ? std::optional(context) : std::nullopt;
}
} // namespace NereusSDR
