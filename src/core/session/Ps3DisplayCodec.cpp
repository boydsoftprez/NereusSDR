// =================================================================
// src/core/session/Ps3DisplayCodec.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original bounded PS3 snapshot transport.
//
// =================================================================

#include "core/session/Ps3DisplayCodec.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace NereusSDR {
namespace {

constexpr quint32 kMagic = 0x50533344U; // "PS3D", distinct from DisplayCodec "NSDC"
constexpr int kPayloadBytesPerChunk = Ps3DisplayCodec::kMaxChunkBytes
    - Ps3DisplayCodec::kHeaderBytes;

class Writer {
public:
    void u8(quint8 value) { m_bytes.append(static_cast<char>(value)); }
    void u16(quint16 value)
    {
        u8(static_cast<quint8>(value >> 8));
        u8(static_cast<quint8>(value));
    }
    void u32(quint32 value)
    {
        u8(static_cast<quint8>(value >> 24));
        u8(static_cast<quint8>(value >> 16));
        u8(static_cast<quint8>(value >> 8));
        u8(static_cast<quint8>(value));
    }
    void u64(quint64 value)
    {
        u32(static_cast<quint32>(value >> 32));
        u32(static_cast<quint32>(value));
    }
    void i64(qint64 value)
    {
        quint64 bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        u64(bits);
    }
    void f64(double value)
    {
        quint64 bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        u64(bits);
    }
    void bytes(const QByteArray& value) { m_bytes.append(value); }
    QByteArray take() { return std::move(m_bytes); }

private:
    QByteArray m_bytes;
};

class Reader {
public:
    explicit Reader(const QByteArray& bytes, int position = 0)
        : m_bytes(bytes), m_position(position)
    {
    }

    bool u8(quint8& value)
    {
        if (m_position >= m_bytes.size())
            return false;
        value = static_cast<quint8>(static_cast<unsigned char>(m_bytes.at(m_position++)));
        return true;
    }
    bool u16(quint16& value)
    {
        quint8 a = 0, b = 0;
        if (!u8(a) || !u8(b))
            return false;
        value = (static_cast<quint16>(a) << 8) | b;
        return true;
    }
    bool u32(quint32& value)
    {
        quint8 a = 0, b = 0, c = 0, d = 0;
        if (!u8(a) || !u8(b) || !u8(c) || !u8(d))
            return false;
        value = (static_cast<quint32>(a) << 24)
            | (static_cast<quint32>(b) << 16)
            | (static_cast<quint32>(c) << 8) | d;
        return true;
    }
    bool u64(quint64& value)
    {
        quint32 high = 0, low = 0;
        if (!u32(high) || !u32(low))
            return false;
        value = (static_cast<quint64>(high) << 32) | low;
        return true;
    }
    bool i64(qint64& value)
    {
        quint64 bits = 0;
        if (!u64(bits))
            return false;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }
    bool f64(double& value)
    {
        quint64 bits = 0;
        if (!u64(bits))
            return false;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }
    bool atEnd() const { return m_position == m_bytes.size(); }

private:
    const QByteArray& m_bytes;
    int m_position {0};
};

void setError(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

bool finite(double value)
{
    return std::isfinite(value);
}

bool validVector(const std::vector<double>& values, int expected)
{
    return values.size() == static_cast<std::size_t>(expected)
        && std::all_of(values.cbegin(), values.cend(), [](double value) {
               return finite(value);
           });
}

bool validSnapshot(const Ps3Snapshot& snapshot, QString* error)
{
    if (snapshot.channelId < 0) {
        setError(error, QStringLiteral("PS3 display channel ID is invalid"));
        return false;
    }
    if (snapshot.sampleCount < 0
        || snapshot.sampleCount > Ps3Snapshot::kMaxSampleCount
        || snapshot.correctionCount < 0
        || snapshot.correctionCount > Ps3Snapshot::kMaxCorrectionCount) {
        setError(error, QStringLiteral("PS3 display counts exceed the bounded geometry"));
        return false;
    }
    if (!finite(snapshot.phaseReferenceDegrees)) {
        setError(error, QStringLiteral("PS3 display phase reference must be finite"));
        return false;
    }
    if (!validVector(snapshot.x, snapshot.sampleCount)
        || !validVector(snapshot.ym, snapshot.sampleCount)
        || !validVector(snapshot.yc, snapshot.sampleCount)
        || !validVector(snapshot.ys, snapshot.sampleCount)
        || !validVector(snapshot.xmCorrection, snapshot.correctionCount)
        || !validVector(snapshot.ymCorrection, snapshot.correctionCount)
        || !validVector(snapshot.xaCorrection, snapshot.correctionCount)
        || !validVector(snapshot.yaCorrection, snapshot.correctionCount)) {
        setError(error, QStringLiteral("PS3 display vectors must match their counts and contain finite values"));
        return false;
    }
    return true;
}

int canonicalChunkCount(quint32 payloadBytes)
{
    const quint64 count = (static_cast<quint64>(payloadBytes)
                           + static_cast<quint64>(kPayloadBytesPerChunk) - 1)
        / static_cast<quint64>(kPayloadBytesPerChunk);
    return std::max(1, static_cast<int>(count));
}

void appendVector(Writer& writer, const std::vector<double>& values)
{
    for (double value : values)
        writer.f64(value);
}

bool readVector(Reader& reader, int count, std::vector<double>& values)
{
    values.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        double value = 0.0;
        if (!reader.f64(value) || !finite(value))
            return false;
        values.push_back(value);
    }
    return true;
}

} // namespace

bool Ps3DisplayCodec::validateSnapshot(const Ps3Snapshot& snapshot, QString* error)
{
    if (error) {
        error->clear();
    }
    return validSnapshot(snapshot, error);
}

QList<QByteArray> Ps3DisplayCodec::encode(const Ps3Snapshot& snapshot, QString* error)
{
    if (error)
        error->clear();
    if (!validSnapshot(snapshot, error))
        return {};

    const quint64 valueCount = quint64{4}
        * static_cast<quint64>(snapshot.sampleCount + snapshot.correctionCount);
    const quint64 payloadSize64 = valueCount * sizeof(double);
    if (payloadSize64 > std::numeric_limits<quint32>::max()) {
        setError(error, QStringLiteral("PS3 display payload size overflow"));
        return {};
    }
    const quint32 payloadBytes = static_cast<quint32>(payloadSize64);
    const int chunkCount = canonicalChunkCount(payloadBytes);
    if (payloadSize64 + static_cast<quint64>(chunkCount * kHeaderBytes)
        > static_cast<quint64>(kMaxFrameBytes)) {
        setError(error, QStringLiteral("PS3 display frame exceeds the 160 KiB aggregate limit"));
        return {};
    }

    Writer payloadWriter;
    appendVector(payloadWriter, snapshot.x);
    appendVector(payloadWriter, snapshot.ym);
    appendVector(payloadWriter, snapshot.yc);
    appendVector(payloadWriter, snapshot.ys);
    appendVector(payloadWriter, snapshot.xmCorrection);
    appendVector(payloadWriter, snapshot.ymCorrection);
    appendVector(payloadWriter, snapshot.xaCorrection);
    appendVector(payloadWriter, snapshot.yaCorrection);
    const QByteArray payload = payloadWriter.take();

    QList<QByteArray> chunks;
    chunks.reserve(chunkCount);
    for (int index = 0; index < chunkCount; ++index) {
        const quint32 offset = static_cast<quint32>(index * kPayloadBytesPerChunk);
        const quint32 bytes = std::min<quint32>(
            kPayloadBytesPerChunk, payloadBytes - offset);
        Writer writer;
        writer.u32(kMagic);
        writer.u16(kSchema);
        writer.u16(kHeaderBytes);
        writer.u64(snapshot.sessionGeneration);
        writer.u64(snapshot.sequence);
        writer.u32(static_cast<quint32>(snapshot.channelId));
        writer.i64(snapshot.capturedAtUnixMilliseconds);
        writer.u16(static_cast<quint16>(snapshot.sampleCount));
        writer.u16(static_cast<quint16>(snapshot.correctionCount));
        writer.f64(snapshot.phaseReferenceDegrees);
        writer.u32(payloadBytes);
        writer.u32(offset);
        writer.u32(bytes);
        writer.u16(static_cast<quint16>(index));
        writer.u16(static_cast<quint16>(chunkCount));
        writer.bytes(payload.mid(static_cast<int>(offset), static_cast<int>(bytes)));
        QByteArray chunk = writer.take();
        if (chunk.size() > kMaxChunkBytes) {
            setError(error, QStringLiteral("PS3 display chunk exceeds the 64 KiB transport limit"));
            return {};
        }
        chunks.append(std::move(chunk));
    }
    return chunks;
}

Ps3DisplayAssembler::Ps3DisplayAssembler(std::uint64_t expectedSessionGeneration)
    : m_expectedSessionGeneration(expectedSessionGeneration)
{
}

bool Ps3DisplayAssembler::sameIdentity(const FrameIdentity& left,
                                       const FrameIdentity& right)
{
    return left.sessionGeneration == right.sessionGeneration
        && left.sequence == right.sequence
        && left.capturedAtUnixMilliseconds == right.capturedAtUnixMilliseconds
        && left.channelId == right.channelId
        && left.sampleCount == right.sampleCount
        && left.correctionCount == right.correctionCount
        && left.phaseReferenceDegrees == right.phaseReferenceDegrees
        && left.payloadBytes == right.payloadBytes
        && left.chunkCount == right.chunkCount;
}

void Ps3DisplayAssembler::retirePending()
{
    m_hasPending = false;
    m_pendingChunks.clear();
    m_receivedChunks.clear();
    m_receivedCount = 0;
}

void Ps3DisplayAssembler::reset(std::uint64_t expectedSessionGeneration)
{
    m_expectedSessionGeneration = expectedSessionGeneration;
    m_hasCompletedSequence = false;
    m_completedSequence = 0;
    retirePending();
}

std::optional<Ps3Snapshot> Ps3DisplayAssembler::accept(const QByteArray& chunk,
                                                       QString* error)
{
    if (error)
        error->clear();
    if (chunk.size() > Ps3DisplayCodec::kMaxChunkBytes) {
        setError(error, QStringLiteral("PS3 display chunk is too large"));
        return std::nullopt;
    }
    if (chunk.size() < Ps3DisplayCodec::kHeaderBytes) {
        setError(error, QStringLiteral("PS3 display chunk header is truncated"));
        return std::nullopt;
    }

    Reader reader(chunk);
    quint32 magic = 0, channelId = 0, payloadBytes = 0, chunkOffset = 0,
            chunkBytes = 0;
    quint16 schema = 0, headerBytes = 0, sampleCount = 0,
            correctionCount = 0, chunkIndex = 0, chunkCount = 0;
    quint64 sessionGeneration = 0, sequence = 0;
    qint64 timestamp = 0;
    double phaseReference = 0.0;
    if (!reader.u32(magic) || !reader.u16(schema) || !reader.u16(headerBytes)
        || !reader.u64(sessionGeneration) || !reader.u64(sequence)
        || !reader.u32(channelId) || !reader.i64(timestamp)
        || !reader.u16(sampleCount) || !reader.u16(correctionCount)
        || !reader.f64(phaseReference) || !reader.u32(payloadBytes)
        || !reader.u32(chunkOffset) || !reader.u32(chunkBytes)
        || !reader.u16(chunkIndex) || !reader.u16(chunkCount)) {
        setError(error, QStringLiteral("PS3 display chunk header is truncated"));
        return std::nullopt;
    }
    if (magic != kMagic) {
        setError(error, QStringLiteral("PS3 display chunk magic is invalid"));
        return std::nullopt;
    }
    if (schema != Ps3DisplayCodec::kSchema) {
        setError(error, QStringLiteral("PS3 display schema is unsupported"));
        return std::nullopt;
    }
    if (headerBytes != Ps3DisplayCodec::kHeaderBytes) {
        setError(error, QStringLiteral("PS3 display header size is invalid"));
        return std::nullopt;
    }
    if (sessionGeneration != m_expectedSessionGeneration) {
        setError(error, QStringLiteral("PS3 display chunk belongs to the wrong session"));
        return std::nullopt;
    }
    if (channelId > static_cast<quint32>(std::numeric_limits<int>::max())) {
        setError(error, QStringLiteral("PS3 display channel ID is invalid"));
        return std::nullopt;
    }
    if (sampleCount > Ps3Snapshot::kMaxSampleCount
        || correctionCount > Ps3Snapshot::kMaxCorrectionCount) {
        setError(error, QStringLiteral("PS3 display counts exceed the bounded geometry"));
        return std::nullopt;
    }
    if (!finite(phaseReference)) {
        setError(error, QStringLiteral("PS3 display phase reference must be finite"));
        return std::nullopt;
    }

    const quint64 expectedPayloadBytes = quint64{4}
        * static_cast<quint64>(sampleCount + correctionCount) * sizeof(double);
    const int expectedChunkCount = canonicalChunkCount(payloadBytes);
    if (payloadBytes != expectedPayloadBytes
        || chunkCount != expectedChunkCount || chunkIndex >= chunkCount
        || static_cast<quint64>(payloadBytes)
                + static_cast<quint64>(chunkCount) * Ps3DisplayCodec::kHeaderBytes
            > Ps3DisplayCodec::kMaxFrameBytes) {
        setError(error, QStringLiteral("PS3 display count and frame sizes are inconsistent"));
        return std::nullopt;
    }
    const quint32 expectedOffset = static_cast<quint32>(chunkIndex)
        * static_cast<quint32>(kPayloadBytesPerChunk);
    const quint32 expectedChunkBytes = std::min<quint32>(
        kPayloadBytesPerChunk, payloadBytes - expectedOffset);
    if (chunkOffset != expectedOffset || chunkBytes != expectedChunkBytes) {
        setError(error, QStringLiteral("PS3 display chunk offset creates an overlap or gap"));
        return std::nullopt;
    }
    if (chunk.size() != Ps3DisplayCodec::kHeaderBytes + static_cast<int>(chunkBytes)) {
        setError(error, QStringLiteral("PS3 display chunk length is inconsistent"));
        return std::nullopt;
    }

    Reader payloadReader(chunk, Ps3DisplayCodec::kHeaderBytes);
    for (quint32 offset = 0; offset < chunkBytes; offset += sizeof(double)) {
        double value = 0.0;
        if (!payloadReader.f64(value) || !finite(value)) {
            setError(error, QStringLiteral("PS3 display payload values must be finite"));
            return std::nullopt;
        }
    }
    if (!payloadReader.atEnd()) {
        setError(error, QStringLiteral("PS3 display payload is malformed"));
        return std::nullopt;
    }

    FrameIdentity identity;
    identity.sessionGeneration = sessionGeneration;
    identity.sequence = sequence;
    identity.capturedAtUnixMilliseconds = timestamp;
    identity.channelId = static_cast<int>(channelId);
    identity.sampleCount = sampleCount;
    identity.correctionCount = correctionCount;
    identity.phaseReferenceDegrees = phaseReference;
    identity.payloadBytes = payloadBytes;
    identity.chunkCount = chunkCount;

    if (m_hasCompletedSequence && sequence <= m_completedSequence) {
        setError(error, QStringLiteral("PS3 display sequence is stale"));
        return std::nullopt;
    }
    if (m_hasPending) {
        if (sequence < m_pendingIdentity.sequence) {
            setError(error, QStringLiteral("PS3 display sequence is stale"));
            return std::nullopt;
        }
        if (sequence == m_pendingIdentity.sequence) {
            if (!sameIdentity(identity, m_pendingIdentity)) {
                setError(error, QStringLiteral("PS3 display chunk conflicts with the pending frame"));
                return std::nullopt;
            }
            if (m_receivedChunks.at(chunkIndex)) {
                setError(error, QStringLiteral("PS3 display chunk is a duplicate"));
                return std::nullopt;
            }
        } else {
            retirePending();
        }
    }
    if (!m_hasPending) {
        m_hasPending = true;
        m_pendingIdentity = identity;
        m_pendingChunks.resize(chunkCount);
        m_receivedChunks.fill(false, chunkCount);
    }

    m_pendingChunks[chunkIndex] = chunk.mid(Ps3DisplayCodec::kHeaderBytes,
                                            static_cast<int>(chunkBytes));
    m_receivedChunks[chunkIndex] = true;
    ++m_receivedCount;
    if (m_receivedCount != chunkCount)
        return std::nullopt;

    QByteArray payload;
    payload.reserve(static_cast<int>(payloadBytes));
    for (const auto& part : m_pendingChunks)
        payload.append(part);
    if (payload.size() != static_cast<int>(payloadBytes)) {
        setError(error, QStringLiteral("PS3 display frame contains a gap"));
        retirePending();
        return std::nullopt;
    }

    Reader frameReader(payload);
    Ps3Snapshot snapshot;
    snapshot.channelId = identity.channelId;
    snapshot.sessionGeneration = identity.sessionGeneration;
    snapshot.sequence = identity.sequence;
    snapshot.capturedAtUnixMilliseconds = identity.capturedAtUnixMilliseconds;
    snapshot.sampleCount = identity.sampleCount;
    snapshot.correctionCount = identity.correctionCount;
    snapshot.phaseReferenceDegrees = identity.phaseReferenceDegrees;
    const bool decoded = readVector(frameReader, snapshot.sampleCount, snapshot.x)
        && readVector(frameReader, snapshot.sampleCount, snapshot.ym)
        && readVector(frameReader, snapshot.sampleCount, snapshot.yc)
        && readVector(frameReader, snapshot.sampleCount, snapshot.ys)
        && readVector(frameReader, snapshot.correctionCount, snapshot.xmCorrection)
        && readVector(frameReader, snapshot.correctionCount, snapshot.ymCorrection)
        && readVector(frameReader, snapshot.correctionCount, snapshot.xaCorrection)
        && readVector(frameReader, snapshot.correctionCount, snapshot.yaCorrection)
        && frameReader.atEnd();
    if (!decoded) {
        setError(error, QStringLiteral("PS3 display frame payload is malformed"));
        retirePending();
        return std::nullopt;
    }

    m_hasCompletedSequence = true;
    m_completedSequence = identity.sequence;
    retirePending();
    return snapshot;
}

} // namespace NereusSDR
