#pragma once
// =================================================================
// src/core/session/Ps3DisplayCodec.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original bounded PS3 snapshot transport.
//
// =================================================================

#include "core/dsp/Ps3Snapshot.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>

#include <cstdint>
#include <optional>

namespace NereusSDR {

class Ps3DisplayCodec {
public:
    static constexpr quint16 kSchema = 1;
    static constexpr int kHeaderBytes = 64;
    static constexpr int kMaxChunkBytes = 64 * 1024;
    static constexpr int kMaxFrameBytes = 160 * 1024;

    // Public offsets keep wire-format mutation tests independent of host byte
    // order and document the fixed schema-1 header.
    static constexpr int kChannelIdOffset = 24;
    static constexpr int kSampleCountOffset = 36;
    static constexpr int kPhaseReferenceOffset = 40;
    static constexpr int kChunkOffsetOffset = 52;

    /// Returns canonical chunks whose total size includes every fixed header.
    /// Empty means the snapshot failed validation; error is cleared on entry.
    static QList<QByteArray> encode(const Ps3Snapshot& snapshot,
                                    QString* error = nullptr);
    static bool validateSnapshot(const Ps3Snapshot& snapshot, QString* error = nullptr);
};

/// Latest-only assembler for one negotiated session generation. Incomplete
/// input returns std::nullopt with an empty error. Rejection also returns
/// std::nullopt and supplies a reason when error is non-null.
class Ps3DisplayAssembler {
public:
    explicit Ps3DisplayAssembler(std::uint64_t expectedSessionGeneration = 0);

    std::optional<Ps3Snapshot> accept(const QByteArray& chunk,
                                      QString* error = nullptr);
    void reset(std::uint64_t expectedSessionGeneration);

private:
    struct FrameIdentity {
        std::uint64_t sessionGeneration {0};
        std::uint64_t sequence {0};
        std::int64_t capturedAtUnixMilliseconds {0};
        int channelId {-1};
        int sampleCount {0};
        int correctionCount {0};
        double phaseReferenceDegrees {0.0};
        quint32 payloadBytes {0};
        quint16 chunkCount {0};
    };

    static bool sameIdentity(const FrameIdentity& left,
                             const FrameIdentity& right);
    void retirePending();

    std::uint64_t m_expectedSessionGeneration {0};
    bool m_hasCompletedSequence {false};
    std::uint64_t m_completedSequence {0};
    bool m_hasPending {false};
    FrameIdentity m_pendingIdentity;
    QVector<QByteArray> m_pendingChunks;
    QVector<bool> m_receivedChunks;
    int m_receivedCount {0};
};

} // namespace NereusSDR
