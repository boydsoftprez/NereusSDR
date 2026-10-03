// no-port-check: NereusSDR-original. See RemoteIqCodec.h.
#include "core/session/media/RemoteIqCodec.h"

#include <QtEndian>

#include <cmath>
#include <cstring>

namespace NereusSDR {
namespace {

void put32(char* destination, quint32 value)
{
    qToLittleEndian(value, reinterpret_cast<uchar*>(destination));
}

quint32 get32(const char* source)
{
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(source));
}

} // namespace

std::optional<QByteArray> RemoteIqCodec::encode(quint32 sliceId, quint32 generation,
                                                 quint32 sequence,
                                                 const QVector<float>& samples)
{
    if (generation == 0 || samples.isEmpty() || (samples.size() & 1) != 0
        || samples.size() > 2 * kMaxPairs) {
        return std::nullopt;
    }
    for (float value : samples) {
        if (!std::isfinite(value)) { return std::nullopt; }
    }
    QByteArray message(kHeaderBytes + samples.size() * qsizetype(sizeof(float)), '\0');
    char* data = message.data();
    std::memcpy(data, "NSIQ", 4);
    data[4] = 1;
    put32(data + 8, sliceId);
    put32(data + 12, generation);
    put32(data + 16, sequence);
    put32(data + 20, static_cast<quint32>(samples.size() / 2));
    for (qsizetype i = 0; i < samples.size(); ++i) {
        quint32 bits = 0;
        std::memcpy(&bits, &samples[i], sizeof(bits));
        put32(data + kHeaderBytes + i * 4, bits);
    }
    return message;
}

std::optional<RemoteIqFrame> RemoteIqCodec::decode(const QByteArray& message)
{
    if (message.size() < kHeaderBytes || message.size() > kMaxMessageBytes
        || std::memcmp(message.constData(), "NSIQ", 4) != 0 || message.at(4) != 1
        || message.at(5) != 0 || message.at(6) != 0 || message.at(7) != 0) {
        return std::nullopt;
    }
    const char* data = message.constData();
    const quint32 count = get32(data + 20);
    const quint32 generation = get32(data + 12);
    if (generation == 0 || count == 0 || count > kMaxPairs
        || message.size() != kHeaderBytes + qsizetype(count) * 8) {
        return std::nullopt;
    }
    RemoteIqFrame frame;
    frame.sliceId = get32(data + 8);
    frame.generation = generation;
    frame.sequence = get32(data + 16);
    frame.samples.resize(qsizetype(count) * 2);
    for (qsizetype i = 0; i < frame.samples.size(); ++i) {
        const quint32 bits = get32(data + kHeaderBytes + i * 4);
        std::memcpy(&frame.samples[i], &bits, sizeof(bits));
        if (!std::isfinite(frame.samples[i])) { return std::nullopt; }
    }
    return frame;
}

} // namespace NereusSDR
