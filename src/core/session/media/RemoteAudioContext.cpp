// =================================================================
// src/core/session/media/RemoteAudioContext.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See RemoteAudioContext.h.
//
// Modification history (NereusSDR):
//   2026-09-29: iPhone app plan Task 23 (R-IOS-09, audioQualityVersion 1):
//               a device's own Opus bitrate. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-27: Remote-window parity Task 32 (R-IOS-13, R-R3-49): the
//               monitor-audio request and monitor-audio-context codec. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/media/RemoteAudioContext.h"

#include <cmath>
#include <initializer_list>
#include <limits>

namespace NereusSDR {
namespace {

constexpr qsizetype kLegacyContextKeys = 8;
constexpr qsizetype kDetailContextKeys = 9;
constexpr qsizetype kEncoderKeys = 6;
constexpr qsizetype kL16EncoderKeys = 6;
constexpr double kMaxU32 = static_cast<double>(std::numeric_limits<quint32>::max());
constexpr double kMaxSequence = static_cast<double>(std::numeric_limits<quint16>::max());
constexpr int kMinimumTargetBitrate = 6'000;
constexpr int kMaximumTargetBitrate = 510'000;

// An integral JSON number within [low, high]. Strings, booleans and
// fractional or non-finite values are refused, the same test the GUI's
// audio-context parser has always applied.
bool integral(const QJsonValue& value, double low, double high, double& result)
{
    if (!value.isDouble()) {
        return false;
    }
    result = value.toDouble();
    return std::isfinite(result) && result >= low && result <= high
        && std::floor(result) == result;
}

bool knownAudioBandwidthHz(int hz)
{
    for (int known : {4'000, 6'000, 8'000, 12'000, 20'000}) {
        if (hz == known) {
            return true;
        }
    }
    return false;
}

} // namespace

QString remoteAudioOffReasonToWire(RemoteAudioOffReason reason)
{
    switch (reason) {
    case RemoteAudioOffReason::ClientDisabled:
        return QStringLiteral("client-disabled");
    case RemoteAudioOffReason::MediaNotReady:
        return QStringLiteral("media-not-ready");
    case RemoteAudioOffReason::RadioOffline:
        return QStringLiteral("radio-offline");
    case RemoteAudioOffReason::EncoderUnavailable:
        return QStringLiteral("encoder-unavailable");
    case RemoteAudioOffReason::SliceRemoved:
        return QStringLiteral("slice-removed");
    case RemoteAudioOffReason::ReceiverLimit:
        return QStringLiteral("receiver-limit");
    case RemoteAudioOffReason::NoHeadphonesReceiver:
        return QStringLiteral("no-headphones-receiver");
    }
    return {};
}

std::optional<RemoteAudioOffReason> remoteAudioOffReasonFromWire(const QJsonValue& value)
{
    if (!value.isString()) {
        return std::nullopt;
    }
    const QString wire = value.toString();
    for (RemoteAudioOffReason reason :
         {RemoteAudioOffReason::ClientDisabled, RemoteAudioOffReason::MediaNotReady,
          RemoteAudioOffReason::RadioOffline, RemoteAudioOffReason::EncoderUnavailable}) {
        if (wire == remoteAudioOffReasonToWire(reason)) {
            return reason;
        }
    }
    return std::nullopt;
}

std::optional<RemoteAudioOffReason> receiverAudioOffReasonFromWire(const QJsonValue& value)
{
    if (const std::optional<RemoteAudioOffReason> main = remoteAudioOffReasonFromWire(value)) {
        return main;
    }
    if (!value.isString()) {
        return std::nullopt;
    }
    for (RemoteAudioOffReason reason :
         {RemoteAudioOffReason::SliceRemoved, RemoteAudioOffReason::ReceiverLimit}) {
        if (value.toString() == remoteAudioOffReasonToWire(reason)) {
            return reason;
        }
    }
    return std::nullopt;
}

std::optional<RemoteAudioOffReason> headphonesAudioOffReasonFromWire(const QJsonValue& value)
{
    if (const std::optional<RemoteAudioOffReason> main = remoteAudioOffReasonFromWire(value)) {
        return main;
    }
    if (value.isString()
        && value.toString()
            == remoteAudioOffReasonToWire(RemoteAudioOffReason::NoHeadphonesReceiver)) {
        return RemoteAudioOffReason::NoHeadphonesReceiver;
    }
    return std::nullopt;
}

QJsonObject remoteAudioEncoderToJson(const OpusEncoderProfile& profile)
{
    return {
        {QStringLiteral("codec"), QStringLiteral("opus")},
        {QStringLiteral("sampleRate"), static_cast<qint64>(profile.sampleRate)},
        {QStringLiteral("channels"), static_cast<qint64>(profile.channels)},
        {QStringLiteral("frameSamples"), static_cast<qint64>(profile.frameSamples)},
        {QStringLiteral("targetBitrate"), static_cast<qint64>(profile.targetBitrate)},
        {QStringLiteral("audioBandwidthHz"), static_cast<qint64>(profile.audioBandwidthHz)},
    };
}

std::optional<OpusEncoderProfile> remoteAudioEncoderFromJson(const QJsonValue& value)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue codec = object.value(QStringLiteral("codec"));
    double sampleRate = 0.0;
    double channels = 0.0;
    double frameSamples = 0.0;
    double targetBitrate = 0.0;
    double audioBandwidthHz = 0.0;
    // Six keys, each present and valid, is exactly the encoder key set.
    if (object.size() != kEncoderKeys || !codec.isString()
        || codec.toString() != QLatin1String("opus")
        || !integral(object.value(QStringLiteral("sampleRate")),
                     OpusAudioCodecConfig::kSampleRate, OpusAudioCodecConfig::kSampleRate,
                     sampleRate)
        || !integral(object.value(QStringLiteral("channels")),
                     OpusAudioCodecConfig::kChannels, OpusAudioCodecConfig::kChannels,
                     channels)
        || !integral(object.value(QStringLiteral("frameSamples")),
                     OpusAudioCodecConfig::kFrameSamples, OpusAudioCodecConfig::kFrameSamples,
                     frameSamples)
        || !integral(object.value(QStringLiteral("targetBitrate")),
                     kMinimumTargetBitrate, kMaximumTargetBitrate, targetBitrate)
        || !integral(object.value(QStringLiteral("audioBandwidthHz")),
                     4'000.0, 20'000.0, audioBandwidthHz)
        || !knownAudioBandwidthHz(static_cast<int>(audioBandwidthHz))) {
        return std::nullopt;
    }
    OpusEncoderProfile profile;
    profile.sampleRate = static_cast<int>(sampleRate);
    profile.channels = static_cast<int>(channels);
    profile.frameSamples = static_cast<int>(frameSamples);
    profile.targetBitrate = static_cast<int>(targetBitrate);
    profile.audioBandwidthHz = static_cast<int>(audioBandwidthHz);
    return profile;
}

QString remoteAudioProfileToWire(RemoteAudioProfile profile)
{
    switch (profile) {
    case RemoteAudioProfile::Opus:
        return QStringLiteral("opus");
    case RemoteAudioProfile::Lossless:
        return QStringLiteral("lossless");
    }
    return {};
}

std::optional<RemoteAudioProfile> remoteAudioProfileFromWire(const QJsonValue& value)
{
    if (!value.isString()) {
        return std::nullopt;
    }
    for (RemoteAudioProfile profile : {RemoteAudioProfile::Opus, RemoteAudioProfile::Lossless}) {
        if (value.toString() == remoteAudioProfileToWire(profile)) {
            return profile;
        }
    }
    return std::nullopt;
}

QString remoteAudioProfileRefusalToWire(RemoteAudioProfileRefusal refusal)
{
    switch (refusal) {
    case RemoteAudioProfileRefusal::NotAllowed:
        return QStringLiteral("lossless-not-allowed");
    case RemoteAudioProfileRefusal::Unavailable:
        return QStringLiteral("lossless-unavailable");
    }
    return {};
}

std::optional<RemoteAudioProfileRefusal> remoteAudioProfileRefusalFromWire(
    const QJsonValue& value)
{
    if (!value.isString()) {
        return std::nullopt;
    }
    for (RemoteAudioProfileRefusal refusal :
         {RemoteAudioProfileRefusal::NotAllowed, RemoteAudioProfileRefusal::Unavailable}) {
        if (value.toString() == remoteAudioProfileRefusalToWire(refusal)) {
            return refusal;
        }
    }
    return std::nullopt;
}

QJsonObject remoteAudioL16EncoderToJson(const PcmEncoderProfile& profile)
{
    return {
        {QStringLiteral("codec"), QStringLiteral("l16")},
        {QStringLiteral("sampleRate"), static_cast<qint64>(profile.sampleRate)},
        {QStringLiteral("channels"), static_cast<qint64>(profile.channels)},
        {QStringLiteral("frameSamples"), static_cast<qint64>(profile.frameSamples)},
        {QStringLiteral("bitsPerSample"), static_cast<qint64>(profile.bitsPerSample)},
        {QStringLiteral("payloadType"), static_cast<qint64>(profile.payloadType)},
    };
}

std::optional<PcmEncoderProfile> remoteAudioL16EncoderFromJson(const QJsonValue& value)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue codec = object.value(QStringLiteral("codec"));
    const PcmEncoderProfile expected = l16EncoderProfile();
    const auto exactly = [&object](const char* key, int wanted) {
        double parsed = 0.0;
        return integral(object.value(QLatin1String(key)), wanted, wanted, parsed);
    };
    // Six keys, each present with the one value this build plays.
    if (object.size() != kL16EncoderKeys || !codec.isString()
        || codec.toString() != QLatin1String("l16")
        || !exactly("sampleRate", expected.sampleRate)
        || !exactly("channels", expected.channels)
        || !exactly("frameSamples", expected.frameSamples)
        || !exactly("bitsPerSample", expected.bitsPerSample)
        || !exactly("payloadType", expected.payloadType)) {
        return std::nullopt;
    }
    return expected;
}

QString opusBitrateNotOfferedReason()
{
    return QStringLiteral("This Core does not offer that audio quality. The audio stays as it "
                          "was.");
}

QJsonObject encodeRemoteAudioContext(const RemoteAudioContextMessage& message,
                                     bool detailNegotiated, bool profileNegotiated)
{
    // The minor-7 context, key for key and number type for number type.
    QJsonObject payload{
        {QStringLiteral("op"), QStringLiteral("audio-context")},
        {QStringLiteral("connectionId"), message.connectionId},
        {QStringLiteral("revision"), static_cast<qint64>(message.revision)},
        {QStringLiteral("generation"), static_cast<qint64>(message.generation)},
        {QStringLiteral("enabled"), message.enabled},
        {QStringLiteral("ssrc"), static_cast<qint64>(message.ssrc)},
        {QStringLiteral("firstSequence"), static_cast<qint64>(message.firstSequence)},
        {QStringLiteral("firstTimestamp"), static_cast<qint64>(message.firstTimestamp)},
    };
    if (!detailNegotiated) {
        return payload;
    }
    if (profileNegotiated) {
        const RemoteAudioProfile profile = message.profile.value_or(RemoteAudioProfile::Opus);
        payload.insert(QStringLiteral("profile"), remoteAudioProfileToWire(profile));
        if (profile == RemoteAudioProfile::Opus && message.profileRefusal) {
            payload.insert(QStringLiteral("profileRefusal"),
                           remoteAudioProfileRefusalToWire(*message.profileRefusal));
        }
        // iPhone app plan Task 23: only ever set for a device that sent
        // `opusBitrate` (audioQualityVersion 1).
        if (!message.opusBitrateRefusal.isEmpty()) {
            payload.insert(QStringLiteral("opusBitrateRefusal"), message.opusBitrateRefusal);
        }
        if (profile == RemoteAudioProfile::Lossless) {
            if (message.enabled && message.losslessEncoder) {
                payload.insert(QStringLiteral("encoder"),
                               remoteAudioL16EncoderToJson(*message.losslessEncoder));
            } else if (message.enabled) {
                payload.insert(QStringLiteral("enabled"), false);
                payload.insert(QStringLiteral("reason"), remoteAudioOffReasonToWire(
                                   RemoteAudioOffReason::EncoderUnavailable));
            } else if (message.offReason) {
                payload.insert(QStringLiteral("reason"),
                               remoteAudioOffReasonToWire(*message.offReason));
            }
            return payload;
        }
    }
    if (message.enabled && !message.encoder) {
        // A minor-8 GUI refuses an enabled context without its encoder, so
        // Core never sends one: without a profile there is no audio to
        // describe. The caller stops its sender before it gets here.
        payload.insert(QStringLiteral("enabled"), false);
        payload.insert(QStringLiteral("reason"),
                       remoteAudioOffReasonToWire(RemoteAudioOffReason::EncoderUnavailable));
    } else if (message.enabled) {
        payload.insert(QStringLiteral("encoder"), remoteAudioEncoderToJson(*message.encoder));
    } else if (message.offReason) {
        payload.insert(QStringLiteral("reason"), remoteAudioOffReasonToWire(*message.offReason));
    }
    return payload;
}

namespace {

// Which context decodeContext reads.
enum class ContextKind { Main, Receiver, Headphones };

// The shared decoder. Receiver selects the receiver-audio-context: its op,
// one more key (sliceId, checked by the caller), ssrc 0 allowed while
// disabled, and all six off reasons. Headphones (R-R3-45) selects the
// headphones-audio-context: its op, the profile shape's keys, a nonzero
// ssrc, and the four main reasons plus no-headphones-receiver.
std::optional<RemoteAudioContextMessage> decodeContext(const QJsonObject& payload,
                                                       bool detailNegotiated,
                                                       bool profileNegotiated,
                                                       ContextKind kind)
{
    const bool receiver = kind == ContextKind::Receiver;
    profileNegotiated = profileNegotiated && detailNegotiated;
    // The profile shape adds "profile" and, beside profile opus only,
    // "profileRefusal"; the rest is checked as the detail shape.
    // iPhone app plan Task 23: the main context may add
    // "opusBitrateRefusal" for a device that asked for a bitrate.
    const bool bitrateRefusal = profileNegotiated && kind == ContextKind::Main
        && payload.contains(QStringLiteral("opusBitrateRefusal"));
    const qsizetype profileKeys = profileNegotiated
        ? 1 + (payload.contains(QStringLiteral("profileRefusal")) ? 1 : 0)
            + (bitrateRefusal ? 1 : 0)
        : 0;
    const QJsonValue op = payload.value(QStringLiteral("op"));
    const QJsonValue connectionId = payload.value(QStringLiteral("connectionId"));
    const QJsonValue enabled = payload.value(QStringLiteral("enabled"));
    double revision = 0.0;
    double generation = 0.0;
    double ssrc = 0.0;
    double firstSequence = 0.0;
    double firstTimestamp = 0.0;
    // With the key count fixed, the eight keys each present and valid means
    // the legacy shape has no other key, and the detail shape has one more.
    const qsizetype receiverKeys = receiver ? 1 : 0;
    // A receiver context that holds no stream id says so with ssrc 0; it
    // can only be a disabled one.
    const double minimumSsrc = receiver && enabled.isBool() && !enabled.toBool() ? 0.0 : 1.0;
    if (payload.size()
            != (detailNegotiated ? kDetailContextKeys : kLegacyContextKeys) + profileKeys
                + receiverKeys
        || !op.isString()
        || op.toString() != (receiver ? QLatin1String("receiver-audio-context")
                             : kind == ContextKind::Headphones
                                 ? QLatin1String("headphones-audio-context")
                                 : QLatin1String("audio-context"))
        || !connectionId.isString() || !enabled.isBool()
        || !integral(payload.value(QStringLiteral("revision")), 1.0, kMaxU32, revision)
        || !integral(payload.value(QStringLiteral("generation")), 1.0, kMaxU32, generation)
        || !integral(payload.value(QStringLiteral("ssrc")), minimumSsrc, kMaxU32, ssrc)
        || !integral(payload.value(QStringLiteral("firstSequence")), 0.0, kMaxSequence,
                     firstSequence)
        || !integral(payload.value(QStringLiteral("firstTimestamp")), 0.0, kMaxU32,
                     firstTimestamp)) {
        return std::nullopt;
    }
    RemoteAudioContextMessage message;
    message.connectionId = connectionId.toString();
    message.revision = static_cast<quint32>(revision);
    message.generation = static_cast<quint32>(generation);
    message.enabled = enabled.toBool();
    message.ssrc = static_cast<quint32>(ssrc);
    message.firstSequence = static_cast<quint16>(firstSequence);
    message.firstTimestamp = static_cast<quint32>(firstTimestamp);
    if (!detailNegotiated) {
        return message;
    }
    if (profileNegotiated) {
        message.profile = remoteAudioProfileFromWire(payload.value(QStringLiteral("profile")));
        if (!message.profile) {
            return std::nullopt;
        }
        if (payload.contains(QStringLiteral("profileRefusal"))) {
            message.profileRefusal = remoteAudioProfileRefusalFromWire(
                payload.value(QStringLiteral("profileRefusal")));
            if (!message.profileRefusal || *message.profile != RemoteAudioProfile::Opus) {
                return std::nullopt;
            }
        }
        if (bitrateRefusal) {
            const QJsonValue reason = payload.value(QStringLiteral("opusBitrateRefusal"));
            if (!reason.isString() || reason.toString().isEmpty()
                || reason.toString().size() > 512) {
                return std::nullopt;
            }
            message.opusBitrateRefusal = reason.toString();
        }
    }
    const bool lossless = message.profile == RemoteAudioProfile::Lossless;
    if (message.enabled) {
        if (payload.contains(QStringLiteral("reason"))) {
            return std::nullopt;
        }
        if (lossless) {
            message.losslessEncoder =
                remoteAudioL16EncoderFromJson(payload.value(QStringLiteral("encoder")));
            if (!message.losslessEncoder) {
                return std::nullopt;
            }
            return message;
        }
        message.encoder = remoteAudioEncoderFromJson(payload.value(QStringLiteral("encoder")));
        if (!message.encoder) {
            return std::nullopt;
        }
    } else {
        if (payload.contains(QStringLiteral("encoder"))) {
            return std::nullopt;
        }
        message.offReason = receiver
            ? receiverAudioOffReasonFromWire(payload.value(QStringLiteral("reason")))
            : kind == ContextKind::Headphones
                ? headphonesAudioOffReasonFromWire(payload.value(QStringLiteral("reason")))
                : remoteAudioOffReasonFromWire(payload.value(QStringLiteral("reason")));
        if (!message.offReason) {
            return std::nullopt;
        }
    }
    return message;
}

} // namespace

std::optional<RemoteAudioContextMessage> decodeRemoteAudioContext(const QJsonObject& payload,
                                                                  bool detailNegotiated,
                                                                  bool profileNegotiated)
{
    return decodeContext(payload, detailNegotiated, profileNegotiated, ContextKind::Main);
}

QJsonObject encodeReceiverAudioContext(const RemoteReceiverAudioContextMessage& message)
{
    // The audio-profile shape, key for key, then this context's op and the
    // slice it describes.
    QJsonObject payload = encodeRemoteAudioContext(message.context, /*detailNegotiated=*/true,
                                                   /*profileNegotiated=*/true);
    payload.insert(QStringLiteral("op"), QStringLiteral("receiver-audio-context"));
    payload.insert(QStringLiteral("sliceId"), static_cast<qint64>(message.sliceId));
    return payload;
}

std::optional<RemoteReceiverAudioContextMessage> decodeReceiverAudioContext(
    const QJsonObject& payload)
{
    double sliceId = 0.0;
    if (!integral(payload.value(QStringLiteral("sliceId")), 0.0,
                  static_cast<double>(std::numeric_limits<int>::max()), sliceId)) {
        return std::nullopt;
    }
    std::optional<RemoteAudioContextMessage> context =
        decodeContext(payload, /*detailNegotiated=*/true, /*profileNegotiated=*/true,
                      ContextKind::Receiver);
    if (!context) {
        return std::nullopt;
    }
    RemoteReceiverAudioContextMessage message;
    message.sliceId = static_cast<int>(sliceId);
    message.context = std::move(*context);
    return message;
}

QJsonObject encodeHeadphonesAudioContext(const RemoteAudioContextMessage& message)
{
    // The audio-profile shape, key for key, under this context's op.
    QJsonObject payload = encodeRemoteAudioContext(message, /*detailNegotiated=*/true,
                                                   /*profileNegotiated=*/true);
    payload.insert(QStringLiteral("op"), QStringLiteral("headphones-audio-context"));
    return payload;
}

std::optional<RemoteAudioContextMessage> decodeHeadphonesAudioContext(const QJsonObject& payload)
{
    return decodeContext(payload, /*detailNegotiated=*/true, /*profileNegotiated=*/true,
                         ContextKind::Headphones);
}

// ---- Transmit monitor (remote-window parity Task 32) ----

QString txMonitorRouteToWire(TxMonitorRoute route)
{
    switch (route) {
    case TxMonitorRoute::None:
        return QStringLiteral("none");
    case TxMonitorRoute::Speakers:
        return QStringLiteral("speakers");
    case TxMonitorRoute::Headphones:
        return QStringLiteral("headphones");
    }
    return {};
}

std::optional<TxMonitorRoute> txMonitorRouteFromWire(const QJsonValue& value)
{
    if (!value.isString()) {
        return std::nullopt;
    }
    const QString text = value.toString();
    for (const TxMonitorRoute route :
         {TxMonitorRoute::None, TxMonitorRoute::Speakers, TxMonitorRoute::Headphones}) {
        if (text == txMonitorRouteToWire(route)) {
            return route;
        }
    }
    return std::nullopt;
}

namespace {

QJsonObject encodeMonitorAudio(const QString& op, const MonitorAudioMessage& message)
{
    return QJsonObject{{QStringLiteral("op"), op},
                       {QStringLiteral("connectionId"), message.connectionId},
                       {QStringLiteral("revision"), static_cast<double>(message.revision)},
                       {QStringLiteral("route"), txMonitorRouteToWire(message.route)}};
}

std::optional<MonitorAudioMessage> decodeMonitorAudio(const QString& op,
                                                      const QJsonObject& payload)
{
    constexpr qsizetype kMonitorAudioKeys = 4;
    double revision = 0.0;
    const QJsonValue connectionId = payload.value(QStringLiteral("connectionId"));
    const std::optional<TxMonitorRoute> route =
        txMonitorRouteFromWire(payload.value(QStringLiteral("route")));
    if (payload.size() != kMonitorAudioKeys
        || payload.value(QStringLiteral("op")).toString() != op
        || !connectionId.isString() || connectionId.toString().isEmpty()
        || !integral(payload.value(QStringLiteral("revision")), 1.0, kMaxU32, revision)
        || !route) {
        return std::nullopt;
    }
    MonitorAudioMessage message;
    message.connectionId = connectionId.toString();
    message.revision = static_cast<quint32>(revision);
    message.route = *route;
    return message;
}

} // namespace

QJsonObject encodeMonitorAudioRequest(const MonitorAudioMessage& message)
{
    return encodeMonitorAudio(QStringLiteral("monitor-audio"), message);
}

QJsonObject encodeMonitorAudioContext(const MonitorAudioMessage& message)
{
    return encodeMonitorAudio(QStringLiteral("monitor-audio-context"), message);
}

std::optional<MonitorAudioMessage> decodeMonitorAudioRequest(const QJsonObject& payload)
{
    return decodeMonitorAudio(QStringLiteral("monitor-audio"), payload);
}

std::optional<MonitorAudioMessage> decodeMonitorAudioContext(const QJsonObject& payload)
{
    return decodeMonitorAudio(QStringLiteral("monitor-audio-context"), payload);
}

} // namespace NereusSDR
