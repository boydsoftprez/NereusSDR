// =================================================================
// src/core/audio/CaptureProtocol.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Record framing and message codecs
// for the nereus-audio-capture helper pipe; no Thetis logic.
// =================================================================

#include "core/audio/CaptureProtocol.h"

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
    case RecordType::Configure:
    case RecordType::Open:
    case RecordType::Stop:
    case RecordType::Shutdown:
        return true;
    }
    return false;
}

qsizetype payloadBound(RecordType type)
{
    if (type == RecordType::Pcm) {
        return kMaxPcmPayloadBytes;
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

std::optional<QJsonObject> parseExactObject(const QByteArray& json, const QSet<QString>& keys)
{
    if (json.size() > kMaxJsonBytes) {
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
        QStringLiteral("bypassMixer"),  QStringLiteral("manualLatencyMs")};
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
    if (!generation || !deviceName || !sampleRate || !channels || !bufferSamples
        || !exclusiveMode || !hostApiIndex || !driverApi || !bitDepth || !eventDriven
        || !bypassMixer || !manualLatencyMs) {
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

QByteArray encodeShutdown()
{
    return encodeRecord(RecordType::Shutdown, QByteArrayLiteral("{}"));
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
    return checkedRecord(RecordType::Status, toJson(obj), decodeStatus);
}

std::optional<Status> decodeStatus(const QByteArray& json)
{
    static const QSet<QString> kKeys = {
        QStringLiteral("generation"), QStringLiteral("state"),
        QStringLiteral("actualDevice"), QStringLiteral("nativeRate"),
        QStringLiteral("nativeChannels"), QStringLiteral("reason"),
        QStringLiteral("detail")};
    const auto obj = parseExactObject(json, kKeys);
    if (!obj) {
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
    return status;
}

} // namespace NereusSDR::CaptureProtocol
