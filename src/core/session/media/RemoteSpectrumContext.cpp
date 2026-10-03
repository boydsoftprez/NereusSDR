// =================================================================
// src/core/session/media/RemoteSpectrumContext.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See RemoteSpectrumContext.h.
//
// Modification history (NereusSDR):
//   2026-09-26 : Parity Task 28 (R-R3-49, A11): the `transmit` field, for a
//                 peer that declared txDisplayVersion. J.J. Boyd (KG4VCF),
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/media/RemoteSpectrumContext.h"

#include "core/FFTEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace NereusSDR {
namespace {

constexpr qsizetype kLegacyContextKeys = 19;
constexpr qsizetype kGrantKeys = 5;
constexpr double kMaxU32 = static_cast<double>(std::numeric_limits<quint32>::max());

// A finite JSON number within [low, high], integral when asked. Strings,
// booleans and non-finite values are refused, the same test the GUI's
// context parser has always applied.
bool number(const QJsonObject& object, const char* key, double low, double high,
            double& result, bool integral = false)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return false;
    }
    result = value.toDouble();
    return std::isfinite(result) && result >= low && result <= high
        && (!integral || std::floor(result) == result);
}

QString tierToWire(FftTier tier)
{
    return tier == FftTier::Fine ? QStringLiteral("fine") : QStringLiteral("wide");
}

std::optional<FftTier> tierFromWire(const QJsonValue& value)
{
    if (!value.isString()) {
        return std::nullopt;
    }
    if (value.toString() == QLatin1String("wide")) {
        return FftTier::Wide;
    }
    if (value.toString() == QLatin1String("fine")) {
        return FftTier::Fine;
    }
    return std::nullopt;
}

} // namespace

QString spectrumLimitReasonToWire(SpectrumLimitReason reason)
{
    switch (reason) {
    case SpectrumLimitReason::None:
        return QStringLiteral("none");
    case SpectrumLimitReason::LargestSize:
        return QStringLiteral("largest-size");
    case SpectrumLimitReason::SharedEngine:
        return QStringLiteral("shared");
    case SpectrumLimitReason::SourceBins:
        return QStringLiteral("source-bins");
    }
    return {};
}

std::optional<SpectrumLimitReason> spectrumLimitReasonFromWire(const QJsonValue& value)
{
    if (!value.isString()) {
        return std::nullopt;
    }
    const QString wire = value.toString();
    for (SpectrumLimitReason reason :
         {SpectrumLimitReason::None, SpectrumLimitReason::LargestSize,
          SpectrumLimitReason::SharedEngine, SpectrumLimitReason::SourceBins}) {
        if (wire == spectrumLimitReasonToWire(reason)) {
            return reason;
        }
    }
    return std::nullopt;
}

SpectrumContextGrant spectrumContextGrant(const SpectrumGrant& grant)
{
    SpectrumContextGrant reported;
    reported.grantedFftSize = grant.grantedFftSize;
    reported.grantedTier = grant.grantedTier;
    reported.requestedPixels = grant.requestedPixels;
    reported.grantedPixels = grant.grantedPixels;
    reported.limit = grant.reason;
    return reported;
}

QJsonObject encodeRemoteSpectrumContext(const SpectrumContextMessage& message,
                                        bool grantNegotiated)
{
    // The minor-8 context, key for key and number type for number type, as
    // DaemonMediaController::sendContext built it inline.
    QJsonObject payload{
        {QStringLiteral("op"), QStringLiteral("context")},
        {QStringLiteral("connectionId"), message.connectionId},
        {QStringLiteral("endpointId"), static_cast<qint64>(message.endpointId)},
        {QStringLiteral("revision"), static_cast<qint64>(message.revision)},
        {QStringLiteral("contextGeneration"), static_cast<qint64>(message.contextGeneration)},
        {QStringLiteral("sourceStream"), message.sourceStream},
        {QStringLiteral("sourceCentreHz"), message.sourceCentreHz},
        {QStringLiteral("sampleRateHz"), message.sampleRateHz},
        {QStringLiteral("centreHz"), message.centreHz},
        {QStringLiteral("spanHz"), message.spanHz},
        {QStringLiteral("wideCentreHz"), message.wideCentreHz},
        {QStringLiteral("wideSpanHz"), message.wideSpanHz},
        {QStringLiteral("traceSamples"), message.traceSamples},
        {QStringLiteral("waterfallSamples"), message.waterfallSamples},
        {QStringLiteral("wideSamples"), message.wideSamples},
        {QStringLiteral("minDbm"), message.minDbm},
        {QStringLiteral("maxDbm"), message.maxDbm},
        {QStringLiteral("fps"), message.fps},
        {QStringLiteral("framesPerLine"), message.framesPerLine},
    };
    if (message.wideband) {
        payload.insert(QStringLiteral("wideband"), message.wideband->toJson());
    }
    // Core records a grant for every configured endpoint. Without one there
    // is nothing true to report, so the context stays in the minor-8 shape,
    // which a minor-9 GUI refuses rather than trusting an invented grant.
    if (!grantNegotiated || !message.grant) {
        return payload;
    }
    const SpectrumContextGrant& grant = *message.grant;
    payload.insert(QStringLiteral("grantedFftSize"), grant.grantedFftSize);
    payload.insert(QStringLiteral("grantedTier"), tierToWire(grant.grantedTier));
    payload.insert(QStringLiteral("requestedPixels"), grant.requestedPixels);
    payload.insert(QStringLiteral("grantedPixels"), grant.grantedPixels);
    payload.insert(QStringLiteral("limit"), spectrumLimitReasonToWire(grant.limit));
    // Parity Task 28: one more field for a peer that declared the transmit
    // display (txDisplayVersion); every other peer's context is unchanged.
    if (message.transmit) {
        payload.insert(QStringLiteral("transmit"), *message.transmit);
    }
    return payload;
}

std::optional<SpectrumContextMessage> decodeRemoteSpectrumContext(const QJsonObject& payload,
                                                                  bool grantNegotiated,
                                                                  bool transmitNegotiated)
{
    const bool hasWideband = payload.contains(QStringLiteral("wideband"));
    // Parity Task 28: `transmit` rides only on the grant shape (minor 11).
    if (transmitNegotiated && !grantNegotiated) {
        return std::nullopt;
    }
    const qsizetype expectedKeys = kLegacyContextKeys + (hasWideband ? 1 : 0)
        + (grantNegotiated ? kGrantKeys : 0) + (transmitNegotiated ? 1 : 0);
    const QJsonValue op = payload.value(QStringLiteral("op"));
    const QJsonValue connectionId = payload.value(QStringLiteral("connectionId"));
    // With the key count fixed, every expected key present and valid means
    // no other key is present.
    if (payload.size() != expectedKeys || !op.isString()
        || op.toString() != QLatin1String("context") || !connectionId.isString()) {
        return std::nullopt;
    }

    SpectrumContextMessage message;
    message.connectionId = connectionId.toString();
    double endpointId = 0, revision = 0, generation = 0, stream = 0;
    if (!number(payload, "endpointId", 1, kMaxU32, endpointId, true)
        || !number(payload, "revision", 1, kMaxU32, revision, true)
        || !number(payload, "contextGeneration", 1, kMaxU32, generation, true)
        || !number(payload, "sourceStream", 0, 255, stream, true)
        || !number(payload, "sourceCentreHz", 0, 1.0e12, message.sourceCentreHz)
        || !number(payload, "sampleRateHz", 1, 1.0e8, message.sampleRateHz)) {
        return std::nullopt;
    }
    if (hasWideband) {
        const QJsonValue widebandJson = payload.value(QStringLiteral("wideband"));
        if (!widebandJson.isObject()) {
            return std::nullopt;
        }
        message.wideband = WidebandDisplayContext::fromJson(widebandJson.toObject());
        if (!message.wideband) {
            return std::nullopt;
        }
    }
    const double rate = message.sampleRateHz;
    const double maxSpan = message.wideband && message.wideband->available
        ? std::max(rate, message.wideband->adcRateHz / 2.0) : rate;
    double trace = 0, waterfall = 0, wide = 0, fps = 0, lines = 0;
    if (!number(payload, "centreHz", 0, 1.0e12, message.centreHz)
        || !number(payload, "spanHz", 0.000001, maxSpan, message.spanHz)
        || !number(payload, "wideCentreHz", 0, 1.0e12, message.wideCentreHz)
        || !number(payload, "wideSpanHz", 0, rate, message.wideSpanHz)
        || !number(payload, "traceSamples", 1, SpectrumEndpoint::kMaxPixels, trace, true)
        || !number(payload, "waterfallSamples", 1, SpectrumEndpoint::kMaxPixels, waterfall, true)
        || !number(payload, "wideSamples", 0, SpectrumEndpoint::kMaxWideSamples, wide, true)
        || !number(payload, "minDbm", kMinDbmLimit, kMaxDbmLimit, message.minDbm)
        || !number(payload, "maxDbm", kMinDbmLimit, kMaxDbmLimit, message.maxDbm)
        || message.minDbm >= message.maxDbm
        || !number(payload, "fps", 1, 60, fps, true)
        || !number(payload, "framesPerLine", 1, kMaxFramesPerLine, lines, true)
        // A wide row exists exactly when it has coverage.
        || (wide == 0) != (message.wideSpanHz == 0)) {
        return std::nullopt;
    }
    message.endpointId = static_cast<quint32>(endpointId);
    message.revision = static_cast<quint32>(revision);
    message.contextGeneration = static_cast<quint32>(generation);
    message.sourceStream = static_cast<int>(stream);
    message.traceSamples = static_cast<int>(trace);
    message.waterfallSamples = static_cast<int>(waterfall);
    message.wideSamples = static_cast<int>(wide);
    message.fps = static_cast<int>(fps);
    message.framesPerLine = static_cast<int>(lines);
    if (!grantNegotiated) {
        return message;
    }

    double fftSize = 0, requestedPixels = 0, grantedPixels = 0;
    const std::optional<FftTier> tier = tierFromWire(payload.value(QStringLiteral("grantedTier")));
    const std::optional<SpectrumLimitReason> limit =
        spectrumLimitReasonFromWire(payload.value(QStringLiteral("limit")));
    if (!tier || !limit
        || !number(payload, "grantedFftSize", 1, FFTEngine::maximumFftSize(), fftSize, true)
        || !number(payload, "requestedPixels", 1, SpectrumEndpoint::kMaxPixels,
                   requestedPixels, true)
        || !number(payload, "grantedPixels", 1, SpectrumEndpoint::kMaxPixels,
                   grantedPixels, true)
        || grantedPixels > requestedPixels) {
        return std::nullopt;
    }
    SpectrumContextGrant grant;
    grant.grantedFftSize = static_cast<int>(fftSize);
    grant.grantedTier = *tier;
    grant.requestedPixels = static_cast<int>(requestedPixels);
    grant.grantedPixels = static_cast<int>(grantedPixels);
    grant.limit = *limit;
    message.grant = grant;
    if (transmitNegotiated) {
        const QJsonValue transmit = payload.value(QStringLiteral("transmit"));
        if (!transmit.isBool()) {
            return std::nullopt;
        }
        message.transmit = transmit.toBool();
    }
    return message;
}

} // namespace NereusSDR
