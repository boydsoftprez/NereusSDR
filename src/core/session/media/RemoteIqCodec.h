// no-port-check: NereusSDR-original bounded raw-I/Q media framing.
// Modification history (NereusSDR):
//   2026-09-27: original implementation by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via OpenAI Codex.
#pragma once

#include <QByteArray>
#include <QVector>

#include <optional>

namespace NereusSDR {

struct RemoteIqFrame {
    quint32 sliceId = 0;
    quint32 generation = 0;
    quint32 sequence = 0;
    QVector<float> samples; // Interleaved I,Q; never swapped on the Core.
};

class RemoteIqCodec final {
public:
    static constexpr int kHeaderBytes = 24;
    static constexpr int kMaxPairs = 1024;
    static constexpr int kMaxMessageBytes = kHeaderBytes + 2 * kMaxPairs * sizeof(float);

    static std::optional<QByteArray> encode(quint32 sliceId, quint32 generation,
                                            quint32 sequence, const QVector<float>& samples);
    static std::optional<RemoteIqFrame> decode(const QByteArray& message);
};

} // namespace NereusSDR
