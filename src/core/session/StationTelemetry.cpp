// no-port-check: NereusSDR-original. See StationTelemetry.h.
#include "core/session/StationTelemetry.h"

#include <QJsonArray>

#include <array>
#include <cmath>
#include <limits>

namespace NereusSDR {
namespace {
constexpr double kMaxExactJsonInteger = 9007199254740991.0;
constexpr double kAbsoluteZeroCelsius = -273.15;

bool integer(const QJsonValue& value, double maximum, qint64* result)
{
    if (!value.isDouble()) { return false; }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0 || number > maximum
        || std::floor(number) != number) {
        return false;
    }
    *result = static_cast<qint64>(number);
    return true;
}

bool optionalInteger(const QJsonObject& object, const QString& key,
                     std::optional<qint64>* result)
{
    if (!object.contains(key)) { result->reset(); return true; }
    qint64 number = 0;
    if (!integer(object.value(key), kMaxExactJsonInteger, &number)) { return false; }
    *result = number;
    return true;
}

bool optionalRate(const QJsonObject& object, const QString& key,
                  std::optional<double>* result)
{
    if (!object.contains(key)) { result->reset(); return true; }
    const QJsonValue value = object.value(key);
    if (!value.isDouble() || !std::isfinite(value.toDouble()) || value.toDouble() < 0) {
        return false;
    }
    *result = value.toDouble();
    return true;
}

using AudioMember = std::optional<double> StationAudioTelemetry::*;
constexpr std::array<std::pair<const char*, AudioMember>, 6> kAudioRates{{
    {"sourceFramesPerSecond", &StationAudioTelemetry::sourceFramesPerSecond},
    {"sourceDropsPerSecond", &StationAudioTelemetry::sourceDropsPerSecond},
    {"encodedPacketsPerSecond", &StationAudioTelemetry::encodedPacketsPerSecond},
    {"encodeFailuresPerSecond", &StationAudioTelemetry::encodeFailuresPerSecond},
    {"sendAcceptedPerSecond", &StationAudioTelemetry::sendAcceptedPerSecond},
    {"sendRejectedPerSecond", &StationAudioTelemetry::sendRejectedPerSecond},
}};

bool optionalPercent(const QJsonObject& object, const QString& key,
                     std::optional<double>* result)
{
    if (!optionalRate(object, key, result)) { return false; }
    return !*result || **result <= 100.0;
}

bool optionalCelsius(const QJsonObject& object, const QString& key,
                     std::optional<double>* result)
{
    if (!object.contains(key)) { result->reset(); return true; }
    const QJsonValue value = object.value(key);
    if (!value.isDouble() || !std::isfinite(value.toDouble())
        || value.toDouble() < kAbsoluteZeroCelsius) {
        return false;
    }
    *result = value.toDouble();
    return true;
}

using HostKiBMember = std::optional<qint64> StationHostTelemetry::*;
constexpr std::array<std::pair<const char*, HostKiBMember>, 3> kHostKiB{{
    {"memoryAvailableKiB", &StationHostTelemetry::memoryAvailableKiB},
    {"memoryTotalKiB", &StationHostTelemetry::memoryTotalKiB},
    {"processResidentKiB", &StationHostTelemetry::processResidentKiB},
}};

// The host section is optional as a whole and field by field. An absent
// section and an empty one both mean nothing was measured.
bool decodeHost(const QJsonObject& object, StationHostTelemetry* host)
{
    const QString key = QStringLiteral("host");
    if (!object.contains(key)) { *host = {}; return true; }
    if (!object.value(key).isObject()) { return false; }
    const QJsonObject section = object.value(key).toObject();
    StationHostTelemetry decoded;
    if (!optionalPercent(section, QStringLiteral("systemCpuPercent"),
                         &decoded.systemCpuPercent)
        || !optionalPercent(section, QStringLiteral("processCpuPercent"),
                            &decoded.processCpuPercent)
        || !optionalCelsius(section, QStringLiteral("hottestZoneCelsius"),
                            &decoded.hottestZoneCelsius)) {
        return false;
    }
    for (const auto& [name, member] : kHostKiB) {
        if (!optionalInteger(section, QString::fromLatin1(name), &(decoded.*member))) {
            return false;
        }
    }
    const QString nameKey = QStringLiteral("hottestZoneName");
    if (section.contains(nameKey)) {
        const QJsonValue name = section.value(nameKey);
        // A name belongs to a measured zone; an empty name is sent absent.
        if (!name.isString() || name.toString().isEmpty()
            || name.toString().size() > kMaxHostZoneNameLength
            || !decoded.hottestZoneCelsius) {
            return false;
        }
        decoded.hottestZoneName = name.toString();
    }
    *host = decoded;
    return true;
}

bool encodeHost(const StationHostTelemetry& host, QJsonObject* object)
{
    if (host.isEmpty()) { return true; }
    QJsonObject section;
    const auto putPercent = [&](const char* key, std::optional<double> value) {
        if (!value) { return true; }
        if (!std::isfinite(*value) || *value < 0 || *value > 100.0) { return false; }
        section.insert(QString::fromLatin1(key), *value);
        return true;
    };
    if (!putPercent("systemCpuPercent", host.systemCpuPercent)
        || !putPercent("processCpuPercent", host.processCpuPercent)) {
        return false;
    }
    for (const auto& [name, member] : kHostKiB) {
        if (host.*member) {
            section.insert(QString::fromLatin1(name), *(host.*member));
        }
    }
    if (host.hottestZoneCelsius) {
        if (!std::isfinite(*host.hottestZoneCelsius)) { return false; }
        section.insert(QStringLiteral("hottestZoneCelsius"), *host.hottestZoneCelsius);
    }
    if (!host.hottestZoneName.isEmpty()) {
        section.insert(QStringLiteral("hottestZoneName"), host.hottestZoneName);
    }
    object->insert(QStringLiteral("host"), section);
    return true;
}

constexpr qint64 kMaxReceiverSliceId = 65535;

bool validReceiver(const StationReceiverTelemetry& receiver)
{
    return receiver.sliceId >= 0 && receiver.sliceId <= kMaxReceiverSliceId
        && (!receiver.loadPercent
            || (std::isfinite(*receiver.loadPercent) && *receiver.loadPercent >= 0))
        && receiver.inputDelayMs >= 0 && receiver.skippedInputMs >= 0
        && static_cast<double>(receiver.inputDelayMs) <= kMaxExactJsonInteger
        && static_cast<double>(receiver.skippedInputMs) <= kMaxExactJsonInteger;
}

bool uniqueSliceIds(const QVector<StationReceiverTelemetry>& receivers)
{
    for (qsizetype i = 0; i < receivers.size(); ++i) {
        for (qsizetype j = i + 1; j < receivers.size(); ++j) {
            if (receivers[i].sliceId == receivers[j].sliceId) { return false; }
        }
    }
    return true;
}

// The receivers section is optional as a whole. Inside it every entry names
// its slice and carries its input wait and skipped input; its load is
// absent when the receiver processed nothing in the interval.
bool decodeReceivers(const QJsonObject& object,
                     std::optional<QVector<StationReceiverTelemetry>>* receivers)
{
    const QString key = QStringLiteral("receivers");
    if (!object.contains(key)) { receivers->reset(); return true; }
    if (!object.value(key).isArray()) { return false; }
    const QJsonArray array = object.value(key).toArray();
    if (array.size() > kMaxStationReceivers) { return false; }
    QVector<StationReceiverTelemetry> decoded;
    decoded.reserve(array.size());
    for (const QJsonValue& value : array) {
        if (!value.isObject()) { return false; }
        const QJsonObject entry = value.toObject();
        StationReceiverTelemetry receiver;
        qint64 sliceId = 0;
        if (!integer(entry.value(QStringLiteral("sliceId")), kMaxReceiverSliceId, &sliceId)
            || !integer(entry.value(QStringLiteral("inputDelayMs")), kMaxExactJsonInteger,
                        &receiver.inputDelayMs)
            || !integer(entry.value(QStringLiteral("skippedInputMs")), kMaxExactJsonInteger,
                        &receiver.skippedInputMs)
            || !optionalRate(entry, QStringLiteral("loadPercent"), &receiver.loadPercent)) {
            return false;
        }
        receiver.sliceId = static_cast<int>(sliceId);
        decoded.append(receiver);
    }
    if (!uniqueSliceIds(decoded)) { return false; }
    *receivers = decoded;
    return true;
}

bool encodeReceivers(const std::optional<QVector<StationReceiverTelemetry>>& receivers,
                     QJsonObject* object)
{
    if (!receivers) { return true; }
    if (receivers->size() > kMaxStationReceivers || !uniqueSliceIds(*receivers)) {
        return false;
    }
    QJsonArray array;
    for (const StationReceiverTelemetry& receiver : *receivers) {
        if (!validReceiver(receiver)) { return false; }
        QJsonObject entry{
            {QStringLiteral("sliceId"), receiver.sliceId},
            {QStringLiteral("inputDelayMs"), receiver.inputDelayMs},
            {QStringLiteral("skippedInputMs"), receiver.skippedInputMs}};
        if (receiver.loadPercent) {
            entry.insert(QStringLiteral("loadPercent"), *receiver.loadPercent);
        }
        array.append(entry);
    }
    object->insert(QStringLiteral("receivers"), array);
    return true;
}

bool putRate(QJsonObject& object, const QString& key, std::optional<double> value)
{
    if (!value) { return true; }
    if (!std::isfinite(*value) || *value < 0) { return false; }
    object.insert(key, *value);
    return true;
}

// R-R3-32 (stationTelemetryVersion 4): the radio's PA readings and link
// quality. Volts, amps, jitter and gap are finite and not negative; loss is
// a percentage; a temperature is not below absolute zero; the counts are
// whole numbers.
using RadioRealMember = std::optional<double> StationRadioTelemetry::*;
constexpr std::array<std::pair<const char*, RadioRealMember>, 5> kRadioReals{{
    {"paVolts", &StationRadioTelemetry::paVolts},
    {"supplyVolts", &StationRadioTelemetry::supplyVolts},
    {"paCurrentAmps", &StationRadioTelemetry::paCurrentAmps},
    {"jitterMs", &StationRadioTelemetry::jitterMs},
    {"packetGapMs", &StationRadioTelemetry::packetGapMs},
}};
using RadioCountMember = std::optional<qint64> StationRadioTelemetry::*;
constexpr std::array<std::pair<const char*, RadioCountMember>, 2> kRadioCounts{{
    {"sampleRateHz", &StationRadioTelemetry::sampleRateHz},
    {"udpPacketsSeen", &StationRadioTelemetry::udpPacketsSeen},
}};

bool decodeRadioStatus(const QJsonObject& radio, StationRadioTelemetry* out)
{
    for (const auto& [name, member] : kRadioReals) {
        if (!optionalRate(radio, QString::fromLatin1(name), &(out->*member))) { return false; }
    }
    for (const auto& [name, member] : kRadioCounts) {
        if (!optionalInteger(radio, QString::fromLatin1(name), &(out->*member))) { return false; }
    }
    return optionalPercent(radio, QStringLiteral("packetLossPercent"), &out->packetLossPercent)
        && optionalCelsius(radio, QStringLiteral("paTemperatureCelsius"),
                           &out->paTemperatureCelsius);
}

// R-R3-32 (stationTelemetryVersion 5, parity Task 14): the Core's HL2
// link. The byte rates are finite and not negative, the throttle is a
// boolean and the sequence gaps a whole number.
bool decodeHl2Link(const QJsonObject& radio, StationRadioTelemetry* out)
{
    if (!optionalRate(radio, QStringLiteral("hl2RxBytesPerSecond"), &out->hl2RxBytesPerSecond)
        || !optionalRate(radio, QStringLiteral("hl2TxBytesPerSecond"),
                         &out->hl2TxBytesPerSecond)
        || !optionalInteger(radio, QStringLiteral("hl2SequenceGaps"), &out->hl2SequenceGaps)) {
        return false;
    }
    const QString throttled = QStringLiteral("hl2Throttled");
    if (!radio.contains(throttled)) {
        out->hl2Throttled.reset();
        return true;
    }
    if (!radio.value(throttled).isBool()) { return false; }
    out->hl2Throttled = radio.value(throttled).toBool();
    return true;
}

bool encodeHl2Link(const StationRadioTelemetry& in, QJsonObject* radio)
{
    if (!putRate(*radio, QStringLiteral("hl2RxBytesPerSecond"), in.hl2RxBytesPerSecond)
        || !putRate(*radio, QStringLiteral("hl2TxBytesPerSecond"), in.hl2TxBytesPerSecond)) {
        return false;
    }
    if (in.hl2Throttled) {
        radio->insert(QStringLiteral("hl2Throttled"), *in.hl2Throttled);
    }
    if (in.hl2SequenceGaps) {
        if (*in.hl2SequenceGaps < 0) { return false; }
        radio->insert(QStringLiteral("hl2SequenceGaps"), *in.hl2SequenceGaps);
    }
    return true;
}

// V6 diagnostics are optional as a group. Every present entry is bounded and
// its boolean describes a status observed within the last three seconds.
bool decodeRadioDiagnostics(const QJsonObject& radio, StationRadioTelemetry* out)
{
    if (!optionalInteger(radio, QStringLiteral("connectionAgeMs"),
                         &out->connectionAgeMs)
        || !optionalInteger(radio, QStringLiteral("radioUdpBasePort"),
                            &out->radioUdpBasePort)) {
        return false;
    }
    if (out->radioUdpBasePort && (*out->radioUdpBasePort < 1
                                  || *out->radioUdpBasePort > 65535)) {
        return false;
    }
    const QString key = QStringLiteral("adcOverloads");
    if (!radio.contains(key)) {
        out->adcOverloads.reset();
        return true;
    }
    if (!radio.value(key).isArray()) { return false; }
    const QJsonArray entries = radio.value(key).toArray();
    if (entries.size() > kMaxStationAdcOverloads) { return false; }
    QVector<StationAdcOverloadTelemetry> decoded;
    decoded.reserve(entries.size());
    for (const QJsonValue& value : entries) {
        if (!value.isObject()) { return false; }
        const QJsonObject object = value.toObject();
        qint64 adc = 0;
        StationAdcOverloadTelemetry entry;
        if (!integer(object.value(QStringLiteral("adc")), 2, &adc)
            || !integer(object.value(QStringLiteral("eventsSinceConnection")),
                        kMaxExactJsonInteger, &entry.eventsSinceConnection)
            || !optionalInteger(object, QStringLiteral("statusAgeMs"),
                                &entry.statusAgeMs)
            || !optionalInteger(object, QStringLiteral("lastOverloadAgeMs"),
                                &entry.lastOverloadAgeMs)) {
            return false;
        }
        entry.adc = static_cast<int>(adc);
        for (const StationAdcOverloadTelemetry& prior : decoded) {
            if (prior.adc == entry.adc) { return false; }
        }
        const QString overloadedKey = QStringLiteral("overloaded");
        if (object.contains(overloadedKey)) {
            if (!object.value(overloadedKey).isBool()) { return false; }
            entry.overloaded = object.value(overloadedKey).toBool();
            if (!entry.statusAgeMs || *entry.statusAgeMs > 3000
                || (*entry.overloaded && entry.eventsSinceConnection == 0)) {
                return false;
            }
        }
        if (entry.lastOverloadAgeMs
            && (entry.eventsSinceConnection == 0
                || (entry.statusAgeMs && *entry.lastOverloadAgeMs < *entry.statusAgeMs))) {
            return false;
        }
        decoded.append(entry);
    }
    out->adcOverloads = decoded;
    return true;
}

bool encodeRadioDiagnostics(const StationRadioTelemetry& in, QJsonObject* radio)
{
    if (in.connectionAgeMs) {
        radio->insert(QStringLiteral("connectionAgeMs"), *in.connectionAgeMs);
    }
    if (in.radioUdpBasePort) {
        radio->insert(QStringLiteral("radioUdpBasePort"), *in.radioUdpBasePort);
    }
    if (in.adcOverloads) {
        if (in.adcOverloads->size() > kMaxStationAdcOverloads) { return false; }
        QJsonArray entries;
        for (const StationAdcOverloadTelemetry& entry : *in.adcOverloads) {
            QJsonObject object{{QStringLiteral("adc"), entry.adc},
                               {QStringLiteral("eventsSinceConnection"),
                                entry.eventsSinceConnection}};
            if (entry.statusAgeMs) {
                object.insert(QStringLiteral("statusAgeMs"), *entry.statusAgeMs);
            }
            if (entry.overloaded) {
                object.insert(QStringLiteral("overloaded"), *entry.overloaded);
            }
            if (entry.lastOverloadAgeMs) {
                object.insert(QStringLiteral("lastOverloadAgeMs"),
                              *entry.lastOverloadAgeMs);
            }
            entries.append(object);
        }
        radio->insert(QStringLiteral("adcOverloads"), entries);
    }
    return true;
}

bool encodeRadioStatus(const StationRadioTelemetry& in, QJsonObject* radio)
{
    for (const auto& [name, member] : kRadioReals) {
        if (!putRate(*radio, QString::fromLatin1(name), in.*member)) { return false; }
    }
    for (const auto& [name, member] : kRadioCounts) {
        if (in.*member) {
            if (*(in.*member) < 0) { return false; }
            radio->insert(QString::fromLatin1(name), *(in.*member));
        }
    }
    if (in.packetLossPercent) {
        if (!std::isfinite(*in.packetLossPercent) || *in.packetLossPercent < 0
            || *in.packetLossPercent > 100.0) {
            return false;
        }
        radio->insert(QStringLiteral("packetLossPercent"), *in.packetLossPercent);
    }
    if (in.paTemperatureCelsius) {
        if (!std::isfinite(*in.paTemperatureCelsius)
            || *in.paTemperatureCelsius < kAbsoluteZeroCelsius) {
            return false;
        }
        radio->insert(QStringLiteral("paTemperatureCelsius"), *in.paTemperatureCelsius);
    }
    return true;
}
}

bool StationTelemetryCodec::decode(const QJsonObject& object,
                                   StationTelemetrySnapshot* snapshot)
{
    if (!snapshot) { return false; }
    StationTelemetrySnapshot decoded;
    qint64 sequence = 0;
    if (!integer(object.value(QStringLiteral("sequence")),
                 std::numeric_limits<quint32>::max(), &sequence)
        || sequence == 0
        || !integer(object.value(QStringLiteral("sampledElapsedMs")),
                    kMaxExactJsonInteger, &decoded.sampledElapsedMs)
        || !object.value(QStringLiteral("radio")).isObject()
        || !object.value(QStringLiteral("audio")).isObject()) {
        return false;
    }
    decoded.sequence = static_cast<quint32>(sequence);
    const QJsonObject radio = object.value(QStringLiteral("radio")).toObject();
    const QJsonObject audio = object.value(QStringLiteral("audio")).toObject();
    qint64 context = 0;
    if (!radio.value(QStringLiteral("connected")).isBool()
        || !audio.value(QStringLiteral("active")).isBool()
        || !integer(audio.value(QStringLiteral("contextGeneration")),
                    std::numeric_limits<quint32>::max(), &context)) {
        return false;
    }
    decoded.radio.connected = radio.value(QStringLiteral("connected")).toBool();
    decoded.audio.active = audio.value(QStringLiteral("active")).toBool();
    decoded.audio.contextGeneration = static_cast<quint32>(context);
    if (decoded.audio.active && context == 0) { return false; }
    if (!optionalRate(radio, QStringLiteral("rxMbps"), &decoded.radio.rxMbps)
        || !optionalRate(radio, QStringLiteral("txMbps"), &decoded.radio.txMbps)
        || !optionalInteger(radio, QStringLiteral("rttMs"), &decoded.radio.rttMs)
        || !optionalInteger(radio, QStringLiteral("rttAgeMs"), &decoded.radio.rttAgeMs)
        || decoded.radio.rttMs.has_value() != decoded.radio.rttAgeMs.has_value()) {
        return false;
    }
    if (!decodeRadioStatus(radio, &decoded.radio) || !decodeHl2Link(radio, &decoded.radio)
        || !decodeRadioDiagnostics(radio, &decoded.radio)) {
        return false;
    }
    if (!decoded.radio.connected && (decoded.radio.rxMbps || decoded.radio.txMbps
                                     || decoded.radio.rttMs
                                     || !decoded.radio.hasNoRadioStatus()
                                     || !decoded.radio.hasNoHl2Link()
                                     || !decoded.radio.hasNoRadioDiagnostics())) {
        return false;
    }
    for (const auto& [name, member] : kAudioRates) {
        if (!optionalRate(audio, QString::fromLatin1(name), &(decoded.audio.*member))
            || (!decoded.audio.active && (decoded.audio.*member).has_value())) {
            return false;
        }
    }
    if (!decodeHost(object, &decoded.host)
        || !decodeReceivers(object, &decoded.receivers)) {
        return false;
    }
    *snapshot = decoded;
    return true;
}

std::optional<QJsonObject> StationTelemetryCodec::encode(
    const StationTelemetrySnapshot& snapshot)
{
    QJsonObject radio{{QStringLiteral("connected"), snapshot.radio.connected}};
    if (!putRate(radio, QStringLiteral("rxMbps"), snapshot.radio.rxMbps)
        || !putRate(radio, QStringLiteral("txMbps"), snapshot.radio.txMbps)) {
        return std::nullopt;
    }
    if (snapshot.radio.rttMs) {
        radio.insert(QStringLiteral("rttMs"), *snapshot.radio.rttMs);
    }
    if (snapshot.radio.rttAgeMs) {
        radio.insert(QStringLiteral("rttAgeMs"), *snapshot.radio.rttAgeMs);
    }
    if (!encodeRadioStatus(snapshot.radio, &radio) || !encodeHl2Link(snapshot.radio, &radio)
        || !encodeRadioDiagnostics(snapshot.radio, &radio)) {
        return std::nullopt;
    }
    QJsonObject audio{{QStringLiteral("active"), snapshot.audio.active},
                      {QStringLiteral("contextGeneration"),
                       static_cast<qint64>(snapshot.audio.contextGeneration)}};
    for (const auto& [name, member] : kAudioRates) {
        if (!putRate(audio, QString::fromLatin1(name), snapshot.audio.*member)) {
            return std::nullopt;
        }
    }
    QJsonObject object{
        {QStringLiteral("sequence"), static_cast<qint64>(snapshot.sequence)},
        {QStringLiteral("sampledElapsedMs"), snapshot.sampledElapsedMs},
        {QStringLiteral("radio"), radio},
        {QStringLiteral("audio"), audio}};
    if (!encodeHost(snapshot.host, &object)
        || !encodeReceivers(snapshot.receivers, &object)) {
        return std::nullopt;
    }
    StationTelemetrySnapshot checked;
    if (!decode(object, &checked)) { return std::nullopt; }
    return object;
}
} // namespace NereusSDR
