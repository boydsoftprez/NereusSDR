// =================================================================
// src/core/audio/CaptureProtocol.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Record framing and message codecs
// for the nereus-audio-capture helper pipe; no Thetis logic.
//
// Modification history (NereusSDR):
//   2026-10-08: native audio plan Task 1 (V-HW-8): ProbeHit and
//               ProbeEnable codecs (protocol version 2). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-17, R-AUD-18): version 3
//               codecs (AttachRing, RingAttached, the Configure identity
//               keys, device-in-use, the Status latency and buffer).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 15 (R-AUD-19 to R-AUD-22): version 4
//               ASIO codecs (AsioDescribe, AsioCaps, AsioOpen, AsioState,
//               AsioControlPanel).  J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/audio/CaptureProtocol.h"

#include "core/audio/DeviceRateMatcher.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QtEndian>

#include <cmath>
#include <cstring>
#include <limits>

namespace NereusSDR::CaptureProtocol {

namespace {

constexpr char kMagic[4] = {'N', 'C', 'A', 'P'};

bool isKnownType(quint8 raw)
{
    switch (static_cast<RecordType>(raw)) {
    case RecordType::Hello:
    case RecordType::Status:
    case RecordType::Pcm:
    case RecordType::ProbeHit:
    case RecordType::Configure:
    case RecordType::Open:
    case RecordType::Stop:
    case RecordType::Shutdown:
    case RecordType::ProbeEnable:
    case RecordType::RingAttached:
    case RecordType::AttachRing:
    case RecordType::AsioCaps:
    case RecordType::AsioState:
    case RecordType::AsioDescribe:
    case RecordType::AsioOpen:
    case RecordType::AsioControlPanel:
        return true;
    }
    return false;
}

bool isAsioType(RecordType type)
{
    return type == RecordType::AsioCaps || type == RecordType::AsioState
        || type == RecordType::AsioDescribe || type == RecordType::AsioOpen
        || type == RecordType::AsioControlPanel;
}

qsizetype payloadBound(RecordType type)
{
    if (type == RecordType::Pcm) {
        return kMaxPcmPayloadBytes;
    }
    if (isAsioType(type)) {
        return kMaxAsioJsonBytes;
    }
    return kMaxJsonBytes;
}

void appendU32(QByteArray& out, quint32 value)
{
    char bytes[4];
    qToLittleEndian<quint32>(value, bytes);
    out.append(bytes, 4);
}

void appendU64(QByteArray& out, quint64 value)
{
    char bytes[8];
    qToLittleEndian<quint64>(value, bytes);
    out.append(bytes, 8);
}

// ── JSON helpers ───────────────────────────────────────────────────────────

std::optional<QJsonObject> parseExactObject(const QByteArray& json, const QSet<QString>& keys,
                                            qsizetype maxBytes = kMaxJsonBytes)
{
    if (json.size() > maxBytes) {
        return std::nullopt;
    }
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    const QJsonObject obj = doc.object();
    if (obj.size() != keys.size()) {
        return std::nullopt;
    }
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!keys.contains(it.key())) {
            return std::nullopt;
        }
    }
    return obj;
}

// Integral JSON number within [lo, hi].
std::optional<qint64> readInteger(const QJsonObject& obj, const QString& key, qint64 lo, qint64 hi)
{
    const QJsonValue value = obj.value(key);
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double d = value.toDouble();
    if (!std::isfinite(d) || std::floor(d) != d) {
        return std::nullopt;
    }
    if (d < static_cast<double>(lo) || d > static_cast<double>(hi)) {
        return std::nullopt;
    }
    return static_cast<qint64>(d);
}

std::optional<QString> readString(const QJsonObject& obj, const QString& key)
{
    const QJsonValue value = obj.value(key);
    if (!value.isString()) {
        return std::nullopt;
    }
    QString s = value.toString();
    if (s.size() > kMaxStringChars) {
        return std::nullopt;
    }
    return s;
}

std::optional<bool> readBool(const QJsonObject& obj, const QString& key)
{
    const QJsonValue value = obj.value(key);
    if (!value.isBool()) {
        return std::nullopt;
    }
    return value.toBool();
}

std::optional<quint32> readGeneration(const QJsonObject& obj)
{
    const auto g = readInteger(obj, QStringLiteral("generation"), 1,
                               std::numeric_limits<quint32>::max());
    if (!g) {
        return std::nullopt;
    }
    return static_cast<quint32>(*g);
}

std::optional<int> readInt(const QJsonObject& obj, const QString& key)
{
    const auto v = readInteger(obj, key, std::numeric_limits<int>::min(),
                               std::numeric_limits<int>::max());
    if (!v) {
        return std::nullopt;
    }
    return static_cast<int>(*v);
}

QByteArray toJson(const QJsonObject& obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

// Encode then run the matching decoder so an encoder can never emit a
// record its peer would reject.
template <typename Decode>
QByteArray checkedRecord(RecordType type, const QByteArray& json, Decode decode)
{
    if (!decode(json)) {
        return {};
    }
    return encodeRecord(type, json);
}

struct StateName {
    HelperState state;
    const char* name;
};

constexpr StateName kStateNames[] = {
    {HelperState::Permission, "permission"},
    {HelperState::Opening, "opening"},
    {HelperState::Ready, "ready"},
    {HelperState::Failed, "failed"},
    {HelperState::Stopped, "stopped"},
};

struct ReasonName {
    FailReason reason;
    const char* name;
};

constexpr ReasonName kReasonNames[] = {
    {FailReason::None, "none"},
    {FailReason::PermissionDenied, "permission-denied"},
    {FailReason::DeviceNotFound, "device-not-found"},
    {FailReason::OpenFailed, "open-failed"},
    {FailReason::StartFailed, "start-failed"},
    {FailReason::InputLost, "input-lost"},
    {FailReason::Internal, "internal"},
    {FailReason::DeviceInUse, "device-in-use"},
};

QString stateName(HelperState state)
{
    for (const StateName& entry : kStateNames) {
        if (entry.state == state) {
            return QString::fromLatin1(entry.name);
        }
    }
    return {};
}

std::optional<HelperState> stateFromName(const QString& name)
{
    for (const StateName& entry : kStateNames) {
        if (name == QLatin1String(entry.name)) {
            return entry.state;
        }
    }
    return std::nullopt;
}

QString reasonName(FailReason reason)
{
    for (const ReasonName& entry : kReasonNames) {
        if (entry.reason == reason) {
            return QString::fromLatin1(entry.name);
        }
    }
    return {};
}

std::optional<FailReason> reasonFromName(const QString& name)
{
    for (const ReasonName& entry : kReasonNames) {
        if (name == QLatin1String(entry.name)) {
            return entry.reason;
        }
    }
    return std::nullopt;
}

} // namespace

// ── Record framing ─────────────────────────────────────────────────────────

QByteArray encodeRecord(RecordType type, const QByteArray& payload)
{
    const quint8 raw = static_cast<quint8>(type);
    if (!isKnownType(raw) || payload.size() > payloadBound(type)) {
        return {};
    }
    QByteArray out;
    out.reserve(kHeaderBytes + payload.size());
    out.append(kMagic, 4);
    out.append(static_cast<char>(kVersion));
    out.append(static_cast<char>(raw));
    out.append('\0');
    out.append('\0');
    appendU32(out, static_cast<quint32>(payload.size()));
    out.append(payload);
    return out;
}

void RecordReader::fail(Error error)
{
    m_error = error;
    m_buffer.clear();
    m_buffer.squeeze();
    m_ready.clear();
    m_expectedTotal = 0;
}

void RecordReader::append(const char* data, qsizetype size)
{
    if (m_error != Error::None || data == nullptr || size <= 0) {
        return;
    }
    qsizetype offset = 0;
    while (offset < size) {
        // Take only what the current record still needs, so the buffer
        // never holds more than one (header + bounded payload).
        const qsizetype want = (m_expectedTotal > 0 ? m_expectedTotal : kHeaderBytes)
                               - m_buffer.size();
        const qsizetype take = qMin(want, size - offset);
        m_buffer.append(data + offset, take);
        offset += take;

        if (m_expectedTotal == 0) {
            if (m_buffer.size() < kHeaderBytes) {
                continue;
            }
            const char* h = m_buffer.constData();
            if (std::memcmp(h, kMagic, 4) != 0) {
                fail(Error::BadMagic);
                return;
            }
            if (static_cast<quint8>(h[4]) != kVersion) {
                fail(Error::BadVersion);
                return;
            }
            const quint8 rawType = static_cast<quint8>(h[5]);
            if (!isKnownType(rawType)) {
                fail(Error::UnknownType);
                return;
            }
            if (h[6] != 0 || h[7] != 0) {
                fail(Error::BadReserved);
                return;
            }
            const quint32 payloadBytes = qFromLittleEndian<quint32>(h + 8);
            const RecordType type = static_cast<RecordType>(rawType);
            if (static_cast<qint64>(payloadBytes) > payloadBound(type)) {
                fail(Error::Oversize);
                return;
            }
            m_pendingType = type;
            m_expectedTotal = kHeaderBytes + static_cast<qsizetype>(payloadBytes);
        }

        if (m_buffer.size() == m_expectedTotal) {
            Record record;
            record.type = m_pendingType;
            record.payload = m_buffer.mid(kHeaderBytes);
            m_ready.push_back(std::move(record));
            m_buffer.clear();
            m_expectedTotal = 0;
        }
    }
}

std::optional<Record> RecordReader::next()
{
    if (m_error != Error::None || m_ready.empty()) {
        return std::nullopt;
    }
    Record record = std::move(m_ready.front());
    m_ready.pop_front();
    return record;
}

RecordReader::Error RecordReader::error() const
{
    return m_error;
}

qsizetype RecordReader::bufferedBytes() const
{
    return m_buffer.size();
}

// ── PCM ────────────────────────────────────────────────────────────────────

QByteArray encodePcm(quint32 generation, quint64 framePosition, quint64 sentMonotonicNs,
                     const float* samples, int frameCount)
{
    if (generation == 0 || samples == nullptr || frameCount < 1 || frameCount > kMaxPcmFrames) {
        return {};
    }
    QByteArray payload;
    payload.reserve(kPcmHeaderBytes + frameCount * 4);
    appendU32(payload, generation);
    appendU32(payload, static_cast<quint32>(frameCount));
    appendU64(payload, framePosition);
    appendU64(payload, sentMonotonicNs);
    for (int i = 0; i < frameCount; ++i) {
        if (!std::isfinite(samples[i])) {
            return {};
        }
        quint32 bits = 0;
        std::memcpy(&bits, &samples[i], sizeof(bits));
        appendU32(payload, bits);
    }
    return encodeRecord(RecordType::Pcm, payload);
}

std::optional<PcmBlock> decodePcm(const QByteArray& payload)
{
    if (payload.size() < kPcmHeaderBytes) {
        return std::nullopt;
    }
    const char* p = payload.constData();
    const quint32 generation = qFromLittleEndian<quint32>(p);
    const quint32 frameCount = qFromLittleEndian<quint32>(p + 4);
    if (generation == 0 || frameCount < 1 || frameCount > static_cast<quint32>(kMaxPcmFrames)) {
        return std::nullopt;
    }
    if (payload.size() != kPcmHeaderBytes + static_cast<qsizetype>(frameCount) * 4) {
        return std::nullopt;
    }
    PcmBlock block;
    block.generation = generation;
    block.framePosition = qFromLittleEndian<quint64>(p + 8);
    block.sentMonotonicNs = qFromLittleEndian<quint64>(p + 16);
    block.samples.resize(static_cast<int>(frameCount));
    const char* s = p + kPcmHeaderBytes;
    for (quint32 i = 0; i < frameCount; ++i) {
        const quint32 bits = qFromLittleEndian<quint32>(s + i * 4);
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value)) {
            return std::nullopt;
        }
        block.samples[static_cast<int>(i)] = value;
    }
    return block;
}

// ── Hello ──────────────────────────────────────────────────────────────────

QByteArray encodeHello(const Hello& hello)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("protocol"), hello.protocol);
    obj.insert(QStringLiteral("pid"), static_cast<double>(hello.pid));
    obj.insert(QStringLiteral("build"), hello.build);
    return checkedRecord(RecordType::Hello, toJson(obj), decodeHello);
}

std::optional<Hello> decodeHello(const QByteArray& json)
{
    static const QSet<QString> kKeys = {
        QStringLiteral("protocol"), QStringLiteral("pid"), QStringLiteral("build")};
    const auto obj = parseExactObject(json, kKeys);
    if (!obj) {
        return std::nullopt;
    }
    // The supervisor compares protocol with kVersion; the codec only
    // checks shape.  pid is bounded by exact double integers (2^53).
    const auto protocol = readInteger(*obj, QStringLiteral("protocol"), 1, 255);
    const auto pid = readInteger(*obj, QStringLiteral("pid"), 1, (qint64{1} << 53));
    const auto build = readString(*obj, QStringLiteral("build"));
    if (!protocol || !pid || !build) {
        return std::nullopt;
    }
    Hello hello;
    hello.protocol = static_cast<int>(*protocol);
    hello.pid = *pid;
    hello.build = *build;
    return hello;
}

// ── Configure ──────────────────────────────────────────────────────────────

QByteArray encodeConfigure(const Configure& configure)
{
    const AudioDeviceConfig& d = configure.device;
    QJsonObject obj;
    obj.insert(QStringLiteral("generation"), static_cast<double>(configure.generation));
    obj.insert(QStringLiteral("deviceName"), d.deviceName);
    obj.insert(QStringLiteral("sampleRate"), d.sampleRate);
    obj.insert(QStringLiteral("channels"), d.channels);
    obj.insert(QStringLiteral("bufferSamples"), d.bufferSamples);
    obj.insert(QStringLiteral("exclusiveMode"), d.exclusiveMode);
    obj.insert(QStringLiteral("hostApiIndex"), d.hostApiIndex);
    obj.insert(QStringLiteral("driverApi"), d.driverApi);
    obj.insert(QStringLiteral("bitDepth"), d.bitDepth);
    obj.insert(QStringLiteral("eventDriven"), d.eventDriven);
    obj.insert(QStringLiteral("bypassMixer"), d.bypassMixer);
    obj.insert(QStringLiteral("manualLatencyMs"), d.manualLatencyMs);
    obj.insert(QStringLiteral("engine"), d.engine ? audioEngineKey(*d.engine) : QString());
    obj.insert(QStringLiteral("deviceId"), d.deviceId);
    obj.insert(QStringLiteral("firstChannel"), d.firstChannel);
    obj.insert(QStringLiteral("micChannel"), micChannelKey(d.micChannel));
    obj.insert(QStringLiteral("delayMs"), d.delayMs);
    return checkedRecord(RecordType::Configure, toJson(obj), decodeConfigure);
}

std::optional<Configure> decodeConfigure(const QByteArray& json)
{
    static const QSet<QString> kKeys = {
        QStringLiteral("generation"),   QStringLiteral("deviceName"),
        QStringLiteral("sampleRate"),   QStringLiteral("channels"),
        QStringLiteral("bufferSamples"), QStringLiteral("exclusiveMode"),
        QStringLiteral("hostApiIndex"), QStringLiteral("driverApi"),
        QStringLiteral("bitDepth"),     QStringLiteral("eventDriven"),
        QStringLiteral("bypassMixer"),  QStringLiteral("manualLatencyMs"),
        QStringLiteral("engine"),       QStringLiteral("deviceId"),
        QStringLiteral("firstChannel"), QStringLiteral("micChannel"),
        QStringLiteral("delayMs")};
    const auto obj = parseExactObject(json, kKeys);
    if (!obj) {
        return std::nullopt;
    }
    const auto generation = readGeneration(*obj);
    const auto deviceName = readString(*obj, QStringLiteral("deviceName"));
    const auto sampleRate = readInt(*obj, QStringLiteral("sampleRate"));
    const auto channels = readInt(*obj, QStringLiteral("channels"));
    const auto bufferSamples = readInt(*obj, QStringLiteral("bufferSamples"));
    const auto exclusiveMode = readBool(*obj, QStringLiteral("exclusiveMode"));
    const auto hostApiIndex = readInt(*obj, QStringLiteral("hostApiIndex"));
    const auto driverApi = readString(*obj, QStringLiteral("driverApi"));
    const auto bitDepth = readInt(*obj, QStringLiteral("bitDepth"));
    const auto eventDriven = readBool(*obj, QStringLiteral("eventDriven"));
    const auto bypassMixer = readBool(*obj, QStringLiteral("bypassMixer"));
    const auto manualLatencyMs = readInt(*obj, QStringLiteral("manualLatencyMs"));
    const auto engineText = readString(*obj, QStringLiteral("engine"));
    const auto deviceId = readString(*obj, QStringLiteral("deviceId"));
    const auto firstChannel = readInteger(*obj, QStringLiteral("firstChannel"), 1,
                                          kMaxNativeChannels);
    const auto micText = readString(*obj, QStringLiteral("micChannel"));
    const auto delayMs = readInt(*obj, QStringLiteral("delayMs"));
    if (!generation || !deviceName || !sampleRate || !channels || !bufferSamples
        || !exclusiveMode || !hostApiIndex || !driverApi || !bitDepth || !eventDriven
        || !bypassMixer || !manualLatencyMs || !engineText || !deviceId || !firstChannel
        || !micText || !delayMs) {
        return std::nullopt;
    }
    // engine: an Engine settings key, or empty when the saved config has none.
    std::optional<AudioEngineKind> engine;
    if (!engineText->isEmpty()) {
        engine = audioEngineFromKey(*engineText);
        if (!engine) {
            return std::nullopt;
        }
    }
    const auto micChannel = micChannelFromKey(*micText);
    if (!micChannel) {
        return std::nullopt;
    }
    // delayMs: 0 (automatic) or one of the matcher's steps.
    bool delayKnown = (*delayMs == 0);
    for (const int step : DeviceRateMatcher::kDelayStepsMs) {
        delayKnown = delayKnown || (*delayMs == step);
    }
    if (!delayKnown) {
        return std::nullopt;
    }
    Configure configure;
    configure.generation = *generation;
    configure.device.deviceName = *deviceName;
    configure.device.sampleRate = *sampleRate;
    configure.device.channels = *channels;
    configure.device.bufferSamples = *bufferSamples;
    configure.device.exclusiveMode = *exclusiveMode;
    configure.device.hostApiIndex = *hostApiIndex;
    configure.device.driverApi = *driverApi;
    configure.device.bitDepth = *bitDepth;
    configure.device.eventDriven = *eventDriven;
    configure.device.bypassMixer = *bypassMixer;
    configure.device.manualLatencyMs = *manualLatencyMs;
    configure.device.engine = engine;
    configure.device.deviceId = *deviceId;
    configure.device.firstChannel = static_cast<int>(*firstChannel);
    configure.device.micChannel = *micChannel;
    configure.device.delayMs = *delayMs;
    return configure;
}

// ── Open / Stop / Shutdown ─────────────────────────────────────────────────

namespace {

QByteArray commandJson(const Command& command)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("generation"), static_cast<double>(command.generation));
    return toJson(obj);
}

} // namespace

QByteArray encodeOpen(const Command& command)
{
    return checkedRecord(RecordType::Open, commandJson(command), decodeCommand);
}

QByteArray encodeStop(const Command& command)
{
    return checkedRecord(RecordType::Stop, commandJson(command), decodeCommand);
}

std::optional<Command> decodeCommand(const QByteArray& json)
{
    static const QSet<QString> kKeys = {QStringLiteral("generation")};
    const auto obj = parseExactObject(json, kKeys);
    if (!obj) {
        return std::nullopt;
    }
    const auto generation = readGeneration(*obj);
    if (!generation) {
        return std::nullopt;
    }
    Command command;
    command.generation = *generation;
    return command;
}

QByteArray encodeRingAttached(const Command& command)
{
    return checkedRecord(RecordType::RingAttached, commandJson(command), decodeCommand);
}

// ── AttachRing (version 3) ─────────────────────────────────────────────────

QByteArray encodeAttachRing(const AttachRing& attach)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("generation"), static_cast<double>(attach.generation));
    obj.insert(QStringLiteral("memory"), attach.memory);
    obj.insert(QStringLiteral("wake"), attach.wake);
    obj.insert(QStringLiteral("bytes"), static_cast<double>(attach.bytes));
    obj.insert(QStringLiteral("inRate"), attach.inRate);
    return checkedRecord(RecordType::AttachRing, toJson(obj), decodeAttachRing);
}

std::optional<AttachRing> decodeAttachRing(const QByteArray& json)
{
    static const QSet<QString> kKeys = {
        QStringLiteral("generation"), QStringLiteral("memory"), QStringLiteral("wake"),
        QStringLiteral("bytes"), QStringLiteral("inRate")};
    const auto obj = parseExactObject(json, kKeys);
    if (!obj) {
        return std::nullopt;
    }
    const auto generation = readGeneration(*obj);
    const auto memory = readString(*obj, QStringLiteral("memory"));
    const auto wake = readString(*obj, QStringLiteral("wake"));
    const auto bytes = readInteger(*obj, QStringLiteral("bytes"), 1, kMaxRingBytes);
    const auto inRate = readInteger(*obj, QStringLiteral("inRate"), kMinNativeRate,
                                    kMaxNativeRate);
    if (!generation || !memory || !wake || !bytes || !inRate || memory->isEmpty()
        || wake->isEmpty() || *memory == *wake) {
        return std::nullopt;
    }
    AttachRing attach;
    attach.generation = *generation;
    attach.memory = *memory;
    attach.wake = *wake;
    attach.bytes = *bytes;
    attach.inRate = static_cast<int>(*inRate);
    return attach;
}

QByteArray encodeShutdown()
{
    return encodeRecord(RecordType::Shutdown, QByteArrayLiteral("{}"));
}

// ── Probe (V-HW-8) ─────────────────────────────────────────────────────────

QByteArray encodeProbeHit(std::int64_t captureNs)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("captureNs"), QString::number(static_cast<qlonglong>(captureNs)));
    return checkedRecord(RecordType::ProbeHit, toJson(obj), decodeProbeHit);
}

std::optional<std::int64_t> decodeProbeHit(const QByteArray& json)
{
    static const QSet<QString> kKeys = {QStringLiteral("captureNs")};
    const auto obj = parseExactObject(json, kKeys);
    if (!obj) {
        return std::nullopt;
    }
    const auto text = readString(*obj, QStringLiteral("captureNs"));
    if (!text) {
        return std::nullopt;
    }
    bool ok = false;
    const qlonglong value = text->toLongLong(&ok);
    // Canonical decimal only: no sign on positives, no padding, no spaces.
    if (!ok || QString::number(value) != *text) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(value);
}

QByteArray encodeProbeEnable(bool enabled)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("enabled"), enabled);
    return checkedRecord(RecordType::ProbeEnable, toJson(obj), decodeProbeEnable);
}

std::optional<bool> decodeProbeEnable(const QByteArray& json)
{
    static const QSet<QString> kKeys = {QStringLiteral("enabled")};
    const auto obj = parseExactObject(json, kKeys);
    if (!obj) {
        return std::nullopt;
    }
    return readBool(*obj, QStringLiteral("enabled"));
}

// ── Status ─────────────────────────────────────────────────────────────────

QByteArray encodeStatus(const Status& status)
{
    const QString state = stateName(status.state);
    const QString reason = reasonName(status.reason);
    if (state.isEmpty() || reason.isEmpty()) {
        return {};
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("generation"), static_cast<double>(status.generation));
    obj.insert(QStringLiteral("state"), state);
    obj.insert(QStringLiteral("actualDevice"), status.actualDevice);
    obj.insert(QStringLiteral("nativeRate"), status.nativeRate);
    obj.insert(QStringLiteral("nativeChannels"), status.nativeChannels);
    obj.insert(QStringLiteral("reason"), reason);
    obj.insert(QStringLiteral("detail"), status.detail);
    obj.insert(QStringLiteral("latencyUs"), status.latencyUs);
    obj.insert(QStringLiteral("bufferFrames"), status.bufferFrames);
    return checkedRecord(RecordType::Status, toJson(obj), decodeStatus);
}

std::optional<Status> decodeStatus(const QByteArray& json)
{
    static const QSet<QString> kKeys = {
        QStringLiteral("generation"), QStringLiteral("state"),
        QStringLiteral("actualDevice"), QStringLiteral("nativeRate"),
        QStringLiteral("nativeChannels"), QStringLiteral("reason"),
        QStringLiteral("detail"), QStringLiteral("latencyUs"),
        QStringLiteral("bufferFrames")};
    const auto obj = parseExactObject(json, kKeys);
    if (!obj) {
        return std::nullopt;
    }
    const auto latencyUs = readInteger(*obj, QStringLiteral("latencyUs"), 0, kMaxLatencyUs);
    const auto bufferFrames = readInteger(*obj, QStringLiteral("bufferFrames"), 0,
                                          kMaxBufferFrames);
    if (!latencyUs || !bufferFrames) {
        return std::nullopt;
    }
    const auto generation = readGeneration(*obj);
    const auto stateText = readString(*obj, QStringLiteral("state"));
    const auto actualDevice = readString(*obj, QStringLiteral("actualDevice"));
    const auto nativeRate = readInteger(*obj, QStringLiteral("nativeRate"), 0, kMaxNativeRate);
    const auto nativeChannels = readInteger(*obj, QStringLiteral("nativeChannels"), 0,
                                            kMaxNativeChannels);
    const auto reasonText = readString(*obj, QStringLiteral("reason"));
    const auto detail = readString(*obj, QStringLiteral("detail"));
    if (!generation || !stateText || !actualDevice || !nativeRate || !nativeChannels
        || !reasonText || !detail) {
        return std::nullopt;
    }
    const auto state = stateFromName(*stateText);
    const auto reason = reasonFromName(*reasonText);
    if (!state || !reason) {
        return std::nullopt;
    }
    const bool formatUnknown = (*nativeRate == 0 && *nativeChannels == 0);
    const bool formatValid = (*nativeRate >= kMinNativeRate && *nativeChannels >= 1);
    if (formatUnknown ? (*state == HelperState::Ready) : !formatValid) {
        return std::nullopt;
    }
    Status status;
    status.generation = *generation;
    status.state = *state;
    status.actualDevice = *actualDevice;
    status.nativeRate = static_cast<int>(*nativeRate);
    status.nativeChannels = static_cast<int>(*nativeChannels);
    status.reason = *reason;
    status.detail = *detail;
    status.latencyUs = static_cast<int>(*latencyUs);
    status.bufferFrames = static_cast<int>(*bufferFrames);
    return status;
}

// ── ASIO (version 4) ───────────────────────────────────────────────────────

namespace {

constexpr int kMaxAsioLatencyFrames = 10'000'000;
constexpr double kAsioRates[] = {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0};

struct RoleKey {
    AudioRole role;
    const char* key;
};

constexpr RoleKey kRoleKeys[] = {
    {AudioRole::Speakers, "Speakers"},   {AudioRole::Headphones, "Headphones"},
    {AudioRole::TxInput, "TxInput"},     {AudioRole::Vax1, "Vax1"},
    {AudioRole::Vax2, "Vax2"},           {AudioRole::Vax3, "Vax3"},
    {AudioRole::Vax4, "Vax4"},
};

struct SampleTypeKey {
    AsioSampleType type;
    const char* key;
};

constexpr SampleTypeKey kSampleTypeKeys[] = {
    {AsioSampleType::Int16Lsb, "Int16LSB"},     {AsioSampleType::Int24Lsb, "Int24LSB"},
    {AsioSampleType::Int32Lsb, "Int32LSB"},     {AsioSampleType::Float32Lsb, "Float32LSB"},
    {AsioSampleType::Float64Lsb, "Float64LSB"}, {AsioSampleType::Unsupported, "Unsupported"},
};

struct StateKindName {
    AsioStateKind state;
    const char* name;
};

constexpr StateKindName kAsioStateNames[] = {
    {AsioStateKind::Running, "running"}, {AsioStateKind::Restarted, "restarted"},
    {AsioStateKind::InUse, "inUse"},     {AsioStateKind::Failed, "failed"},
    {AsioStateKind::Closed, "closed"},
};

std::optional<AsioSampleType> sampleTypeFromKey(const QString& key)
{
    for (const SampleTypeKey& entry : kSampleTypeKeys) {
        if (key == QLatin1String(entry.key)) {
            return entry.type;
        }
    }
    return std::nullopt;
}

QString sampleTypeKey(AsioSampleType type)
{
    for (const SampleTypeKey& entry : kSampleTypeKeys) {
        if (entry.type == type) {
            return QString::fromLatin1(entry.key);
        }
    }
    return {};
}

bool knownAsioRate(double rate)
{
    for (double known : kAsioRates) {
        if (rate == known) {
            return true;
        }
    }
    return false;
}

// A rate: 0 (none) or a positive finite number up to kMaxNativeRate.
std::optional<double> readRate(const QJsonObject& obj, const QString& key)
{
    const QJsonValue value = obj.value(key);
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double rate = value.toDouble();
    if (!std::isfinite(rate) || rate < 0.0 || rate > static_cast<double>(kMaxNativeRate)) {
        return std::nullopt;
    }
    return rate;
}

QString directionKey(AudioDeviceDirection direction)
{
    return direction == AudioDeviceDirection::Output ? QStringLiteral("output")
                                                     : QStringLiteral("input");
}

std::optional<AudioDeviceDirection> directionFromKey(const QString& key)
{
    if (key == QLatin1String("output")) {
        return AudioDeviceDirection::Output;
    }
    if (key == QLatin1String("input")) {
        return AudioDeviceDirection::Input;
    }
    return std::nullopt;
}

bool exactKeys(const QJsonObject& obj, const QSet<QString>& keys)
{
    if (obj.size() != keys.size()) {
        return false;
    }
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!keys.contains(it.key())) {
            return false;
        }
    }
    return true;
}

QJsonObject capsJson(const AsioDriverCaps& caps)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("name"), caps.name);
    obj.insert(QStringLiteral("inputs"), caps.inputChannels);
    obj.insert(QStringLiteral("outputs"), caps.outputChannels);
    obj.insert(QStringLiteral("sampleType"), sampleTypeKey(caps.sampleType));
    obj.insert(QStringLiteral("minBuffer"), caps.minBufferFrames);
    obj.insert(QStringLiteral("maxBuffer"), caps.maxBufferFrames);
    obj.insert(QStringLiteral("preferredBuffer"), caps.preferredBufferFrames);
    obj.insert(QStringLiteral("granularity"), caps.granularity);
    QJsonArray rates;
    for (double rate : caps.sampleRates) {
        rates.append(rate);
    }
    obj.insert(QStringLiteral("rates"), rates);
    obj.insert(QStringLiteral("currentRate"), caps.currentRate);
    obj.insert(QStringLiteral("inputLatency"), static_cast<double>(caps.inputLatencyFrames));
    obj.insert(QStringLiteral("outputLatency"), static_cast<double>(caps.outputLatencyFrames));
    return obj;
}

std::optional<AsioDriverCaps> capsFromJson(const QJsonValue& value)
{
    if (!value.isObject()) {
        return std::nullopt;
    }
    static const QSet<QString> kKeys = {
        QStringLiteral("name"), QStringLiteral("inputs"), QStringLiteral("outputs"),
        QStringLiteral("sampleType"), QStringLiteral("minBuffer"), QStringLiteral("maxBuffer"),
        QStringLiteral("preferredBuffer"), QStringLiteral("granularity"),
        QStringLiteral("rates"), QStringLiteral("currentRate"),
        QStringLiteral("inputLatency"), QStringLiteral("outputLatency")};
    const QJsonObject obj = value.toObject();
    if (!exactKeys(obj, kKeys)) {
        return std::nullopt;
    }
    const auto name = readString(obj, QStringLiteral("name"));
    const auto inputs = readInteger(obj, QStringLiteral("inputs"), 0, kMaxAsioChannels);
    const auto outputs = readInteger(obj, QStringLiteral("outputs"), 0, kMaxAsioChannels);
    const auto typeText = readString(obj, QStringLiteral("sampleType"));
    const auto minBuffer = readInteger(obj, QStringLiteral("minBuffer"), 0, kMaxBufferFrames);
    const auto maxBuffer = readInteger(obj, QStringLiteral("maxBuffer"), 0, kMaxBufferFrames);
    const auto preferred = readInteger(obj, QStringLiteral("preferredBuffer"), 0, kMaxBufferFrames);
    const auto granularity = readInteger(obj, QStringLiteral("granularity"), -1, kMaxBufferFrames);
    const auto currentRate = readRate(obj, QStringLiteral("currentRate"));
    const auto inLatency = readInteger(obj, QStringLiteral("inputLatency"), 0, kMaxAsioLatencyFrames);
    const auto outLatency = readInteger(obj, QStringLiteral("outputLatency"), 0, kMaxAsioLatencyFrames);
    const QJsonValue ratesValue = obj.value(QStringLiteral("rates"));
    if (!name || name->isEmpty() || !inputs || !outputs || !typeText || !minBuffer || !maxBuffer
        || !preferred || !granularity || !currentRate || !inLatency || !outLatency
        || !ratesValue.isArray()) {
        return std::nullopt;
    }
    const auto type = sampleTypeFromKey(*typeText);
    const QJsonArray rates = ratesValue.toArray();
    if (!type || rates.size() > kMaxAsioRates) {
        return std::nullopt;
    }
    AsioDriverCaps caps;
    for (const QJsonValue& rate : rates) {
        if (!rate.isDouble() || !knownAsioRate(rate.toDouble())) {
            return std::nullopt;
        }
        caps.sampleRates.append(rate.toDouble());
    }
    caps.name = *name;
    caps.inputChannels = static_cast<int>(*inputs);
    caps.outputChannels = static_cast<int>(*outputs);
    caps.sampleType = *type;
    caps.minBufferFrames = static_cast<int>(*minBuffer);
    caps.maxBufferFrames = static_cast<int>(*maxBuffer);
    caps.preferredBufferFrames = static_cast<int>(*preferred);
    caps.granularity = static_cast<int>(*granularity);
    caps.currentRate = *currentRate;
    caps.inputLatencyFrames = *inLatency;
    caps.outputLatencyFrames = *outLatency;
    return caps;
}

} // namespace

QString asioRoleKey(AudioRole role)
{
    for (const RoleKey& entry : kRoleKeys) {
        if (entry.role == role) {
            return QString::fromLatin1(entry.key);
        }
    }
    return {};
}

std::optional<AudioRole> asioRoleFromKey(const QString& key)
{
    for (const RoleKey& entry : kRoleKeys) {
        if (key == QLatin1String(entry.key)) {
            return entry.role;
        }
    }
    return std::nullopt;
}

QByteArray encodeAsioDescribe(const AsioDescribe& describe)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("driver"), describe.driver);
    return checkedRecord(RecordType::AsioDescribe, toJson(obj), decodeAsioDescribe);
}

std::optional<AsioDescribe> decodeAsioDescribe(const QByteArray& json)
{
    static const QSet<QString> kKeys = {QStringLiteral("driver")};
    const auto obj = parseExactObject(json, kKeys, kMaxAsioJsonBytes);
    if (!obj) {
        return std::nullopt;
    }
    const auto driver = readString(*obj, QStringLiteral("driver"));
    if (!driver) {
        return std::nullopt;
    }
    return AsioDescribe{*driver};
}

QByteArray encodeAsioCaps(const AsioCapsRecord& caps)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("drivers"), QJsonArray::fromStringList(caps.drivers));
    obj.insert(QStringLiteral("driver"), caps.driver);
    obj.insert(QStringLiteral("caps"), caps.caps ? QJsonValue(capsJson(*caps.caps)) : QJsonValue());
    obj.insert(QStringLiteral("inUse"), caps.inUse);
    return checkedRecord(RecordType::AsioCaps, toJson(obj), decodeAsioCaps);
}

std::optional<AsioCapsRecord> decodeAsioCaps(const QByteArray& json)
{
    static const QSet<QString> kKeys = {QStringLiteral("drivers"), QStringLiteral("driver"),
                                        QStringLiteral("caps"), QStringLiteral("inUse")};
    const auto obj = parseExactObject(json, kKeys, kMaxAsioJsonBytes);
    if (!obj) {
        return std::nullopt;
    }
    const QJsonValue drivers = obj->value(QStringLiteral("drivers"));
    const auto driver = readString(*obj, QStringLiteral("driver"));
    const auto inUse = readBool(*obj, QStringLiteral("inUse"));
    const QJsonValue capsValue = obj->value(QStringLiteral("caps"));
    if (!drivers.isArray() || !driver || !inUse
        || drivers.toArray().size() > kMaxAsioDrivers) {
        return std::nullopt;
    }
    AsioCapsRecord record;
    for (const QJsonValue& name : drivers.toArray()) {
        if (!name.isString() || name.toString().isEmpty()
            || name.toString().size() > kMaxStringChars) {
            return std::nullopt;
        }
        record.drivers.append(name.toString());
    }
    record.driver = *driver;
    record.inUse = *inUse;
    if (!capsValue.isNull()) {
        record.caps = capsFromJson(capsValue);
        if (!record.caps || record.caps->name != record.driver) {
            return std::nullopt;
        }
    }
    return record;
}

QByteArray encodeAsioOpen(const AsioOpen& open)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("serial"), static_cast<double>(open.serial));
    obj.insert(QStringLiteral("driver"), open.driver);
    obj.insert(QStringLiteral("bufferFrames"), open.bufferFrames);
    obj.insert(QStringLiteral("rate"), open.rate);
    QJsonArray uses;
    for (const AsioOpenUse& use : open.uses) {
        QJsonObject u;
        u.insert(QStringLiteral("role"), use.role ? asioRoleKey(*use.role) : QString());
        u.insert(QStringLiteral("first"), use.pair.firstChannel);
        u.insert(QStringLiteral("count"), use.pair.channelCount);
        u.insert(QStringLiteral("direction"), directionKey(use.direction));
        u.insert(QStringLiteral("memory"), use.memory);
        u.insert(QStringLiteral("wake"), use.wake);
        u.insert(QStringLiteral("bytes"), static_cast<double>(use.bytes));
        uses.append(u);
    }
    obj.insert(QStringLiteral("uses"), uses);
    return checkedRecord(RecordType::AsioOpen, toJson(obj), decodeAsioOpen);
}

std::optional<AsioOpen> decodeAsioOpen(const QByteArray& json)
{
    static const QSet<QString> kKeys = {QStringLiteral("serial"), QStringLiteral("driver"),
                                        QStringLiteral("bufferFrames"), QStringLiteral("rate"),
                                        QStringLiteral("uses")};
    static const QSet<QString> kUseKeys = {
        QStringLiteral("role"), QStringLiteral("first"), QStringLiteral("count"),
        QStringLiteral("direction"), QStringLiteral("memory"), QStringLiteral("wake"),
        QStringLiteral("bytes")};
    const auto obj = parseExactObject(json, kKeys, kMaxAsioJsonBytes);
    if (!obj) {
        return std::nullopt;
    }
    const auto serial = readInteger(*obj, QStringLiteral("serial"), 1,
                                    std::numeric_limits<quint32>::max());
    const auto driver = readString(*obj, QStringLiteral("driver"));
    const auto buffer = readInteger(*obj, QStringLiteral("bufferFrames"), 0, kMaxBufferFrames);
    const auto rate = readRate(*obj, QStringLiteral("rate"));
    const QJsonValue usesValue = obj->value(QStringLiteral("uses"));
    if (!serial || !driver || !buffer || !rate || !usesValue.isArray()
        || usesValue.toArray().size() > kMaxAsioUses
        || (driver->isEmpty() && !usesValue.toArray().isEmpty())
        || (*rate != 0.0 && !knownAsioRate(*rate))) {
        return std::nullopt;
    }
    AsioOpen open;
    open.serial = static_cast<quint32>(*serial);
    open.driver = *driver;
    open.bufferFrames = static_cast<int>(*buffer);
    open.rate = *rate;
    for (const QJsonValue& value : usesValue.toArray()) {
        if (!value.isObject() || !exactKeys(value.toObject(), kUseKeys)) {
            return std::nullopt;
        }
        const QJsonObject u = value.toObject();
        const auto roleText = readString(u, QStringLiteral("role"));
        const auto first = readInteger(u, QStringLiteral("first"), 1, kMaxAsioChannels);
        const auto count = readInteger(u, QStringLiteral("count"), 1, 2);
        const auto directionText = readString(u, QStringLiteral("direction"));
        const auto memory = readString(u, QStringLiteral("memory"));
        const auto wake = readString(u, QStringLiteral("wake"));
        const auto bytes = readInteger(u, QStringLiteral("bytes"), 1, kMaxRingBytes);
        if (!roleText || !first || !count || !directionText || !memory || !wake || !bytes
            || memory->isEmpty() || wake->isEmpty() || *memory == *wake) {
            return std::nullopt;
        }
        const auto direction = directionFromKey(*directionText);
        if (!direction) {
            return std::nullopt;
        }
        AsioOpenUse use;
        if (!roleText->isEmpty()) {
            use.role = asioRoleFromKey(*roleText);
            if (!use.role) {
                return std::nullopt;
            }
        }
        use.pair = AudioChannelPair{static_cast<int>(*first), static_cast<int>(*count)};
        use.direction = *direction;
        use.memory = *memory;
        use.wake = *wake;
        use.bytes = *bytes;
        open.uses.append(use);
    }
    return open;
}

QByteArray encodeAsioState(const AsioState& state)
{
    QString name;
    for (const StateKindName& entry : kAsioStateNames) {
        if (entry.state == state.state) {
            name = QString::fromLatin1(entry.name);
        }
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("serial"), static_cast<double>(state.serial));
    obj.insert(QStringLiteral("state"), name);
    obj.insert(QStringLiteral("driver"), state.driver);
    obj.insert(QStringLiteral("detail"), state.detail);
    obj.insert(QStringLiteral("bufferFrames"), state.bufferFrames);
    obj.insert(QStringLiteral("rate"), state.rate);
    obj.insert(QStringLiteral("inputLatency"), state.inputLatencyFrames);
    obj.insert(QStringLiteral("outputLatency"), state.outputLatencyFrames);
    return checkedRecord(RecordType::AsioState, toJson(obj), decodeAsioState);
}

std::optional<AsioState> decodeAsioState(const QByteArray& json)
{
    static const QSet<QString> kKeys = {
        QStringLiteral("serial"), QStringLiteral("state"), QStringLiteral("driver"),
        QStringLiteral("detail"), QStringLiteral("bufferFrames"), QStringLiteral("rate"),
        QStringLiteral("inputLatency"), QStringLiteral("outputLatency")};
    const auto obj = parseExactObject(json, kKeys, kMaxAsioJsonBytes);
    if (!obj) {
        return std::nullopt;
    }
    const auto serial = readInteger(*obj, QStringLiteral("serial"), 0,
                                    std::numeric_limits<quint32>::max());
    const auto stateText = readString(*obj, QStringLiteral("state"));
    const auto driver = readString(*obj, QStringLiteral("driver"));
    const auto detail = readString(*obj, QStringLiteral("detail"));
    const auto buffer = readInteger(*obj, QStringLiteral("bufferFrames"), 0, kMaxBufferFrames);
    const auto rate = readRate(*obj, QStringLiteral("rate"));
    const auto inLatency = readInteger(*obj, QStringLiteral("inputLatency"), 0, kMaxAsioLatencyFrames);
    const auto outLatency = readInteger(*obj, QStringLiteral("outputLatency"), 0, kMaxAsioLatencyFrames);
    if (!serial || !stateText || !driver || !detail || !buffer || !rate || !inLatency
        || !outLatency) {
        return std::nullopt;
    }
    std::optional<AsioStateKind> kind;
    for (const StateKindName& entry : kAsioStateNames) {
        if (*stateText == QLatin1String(entry.name)) {
            kind = entry.state;
        }
    }
    if (!kind) {
        return std::nullopt;
    }
    // Running and restarted report what the session runs at.
    if ((*kind == AsioStateKind::Running || *kind == AsioStateKind::Restarted)
        && (*buffer == 0 || !knownAsioRate(*rate))) {
        return std::nullopt;
    }
    AsioState state;
    state.serial = static_cast<quint32>(*serial);
    state.state = *kind;
    state.driver = *driver;
    state.detail = *detail;
    state.bufferFrames = static_cast<int>(*buffer);
    state.rate = *rate;
    state.inputLatencyFrames = static_cast<int>(*inLatency);
    state.outputLatencyFrames = static_cast<int>(*outLatency);
    return state;
}

QByteArray encodeAsioControlPanel()
{
    return encodeRecord(RecordType::AsioControlPanel, QByteArrayLiteral("{}"));
}

} // namespace NereusSDR::CaptureProtocol
