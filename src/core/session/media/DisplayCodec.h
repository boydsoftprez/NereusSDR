#pragma once
// =================================================================
// src/core/session/media/DisplayCodec.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 Task 3.
//
// Bounded wire codec for reduced dBm trace, waterfall, and optional DSS-wide
// rows. The exact v1 layout and recovery rules are in
// docs/architecture/2026-09-20-display-codec-v1.md.
//
// =================================================================

#include <QByteArray>
#include <QVector>

namespace NereusSDR {

struct DisplayCodecContext {
    quint32 endpointId {0};
    quint32 contextGeneration {0};
    float minDbm {-160.0f};
    float maxDbm {0.0f};
    quint16 traceSamples {0};
    quint16 waterfallSamples {0};
    quint16 wideSamples {0};
};

struct DisplayCodecFrame {
    DisplayCodecContext context;
    quint32 encoderSequence {0};
    /// Sender-monotonic nanoseconds; never compare it to a receiver clock.
    quint64 producerTimestamp {0};
    bool waterfallAdvance {false};
    QVector<float> traceDbm;
    QVector<float> waterfallDbm;
    QVector<float> wideDbm;
};

enum class DisplayCodecDisposition {
    Accepted,
    NeedKeyframe,
    Rejected,
};

enum class DisplayCodecReason {
    None,
    InvalidInput,
    NoHistory,
    SequenceGap,
    StaleSequence,
    OldContext,
    ContextMismatch,
    BadMagic,
    UnsupportedVersion,
    UnknownFlags,
    Truncated,
    Oversized,
    Malformed,
};

struct DisplayCodecDecodeResult {
    DisplayCodecDisposition disposition {DisplayCodecDisposition::Rejected};
    DisplayCodecReason reason {DisplayCodecReason::InvalidInput};
    DisplayCodecFrame frame;
};

/// One NSDC v1 plane of absolute blocks only (a keyframe plane): the plane
/// prefix, then blocks of 128 samples or fewer, each value quantised to
/// [minDbm, maxDbm] as a frame's rows are. The display extras datagram
/// carries the active peak hold row this way (display extras v1). Empty for
/// a length outside 1..kMaxSamplesPerPlane, a sample that is not finite, or
/// an interval that is not finite and increasing.
QByteArray encodeDisplayCodecAbsolutePlane(const QVector<float>& samplesDbm,
                                           float minDbm, float maxDbm);

/// Reads such a plane of exactly `length` samples from `bytes` at `offset`
/// and advances `offset` past it. A residual block is malformed here: there
/// is no history to apply it to. False, with `offset` and `samplesDbm`
/// untouched, for anything malformed or truncated.
bool decodeDisplayCodecAbsolutePlane(const QByteArray& bytes, int& offset, int length,
                                     float minDbm, float maxDbm,
                                     QVector<float>& samplesDbm);

/// Stateful sender. Its history is exactly the quantised reconstruction a
/// receiver obtains, which keeps temporal residuals and dead-zone error bound
/// honest across a long delta chain.
class DisplayCodecEncoder {
public:
    static constexpr quint16 kHeaderBytes = 42;
    static constexpr int kMaxPlanes = 3;
    static constexpr int kMaxSamplesPerPlane = 4096;
    static constexpr int kMaxEncodedBytes = 16 * 1024;
    static constexpr quint32 kKeyframeInterval = 120;

    /// deadZone is measured in quantised units and is clamped to [0, 255].
    explicit DisplayCodecEncoder(int deadZone = 0);

    /// Returns empty for invalid input, stale/equal same-generation sequence,
    /// or a context-shape change without a newer generation. A first frame,
    /// context change, every 120th frame, and requestKeyframe=true keyframe.
    QByteArray encode(const DisplayCodecFrame& frame, bool requestKeyframe = false);
    /// Whether the frame the last non-empty encode() returned is a keyframe.
    bool lastEncodedKeyframe() const { return m_hasHistory && m_sinceKeyframe == 0; }
    void reset();

private:
    int m_deadZone {0};
    bool m_hasHistory {false};
    DisplayCodecContext m_context;
    quint32 m_lastSequence {0};
    quint32 m_sinceKeyframe {0};
    QVector<quint8> m_trace;
    QVector<quint8> m_waterfall;
    QVector<quint8> m_wide;
};

/// Stateful receiver. A rejected or keyframe-needed packet never changes the
/// last accepted context, frame, sequence, or reconstructed rows.
class DisplayCodecDecoder {
public:
    DisplayCodecDecodeResult decode(const QByteArray& packet);
    void reset();

private:
    bool m_hasHistory {false};
    bool m_needsKeyframe {false};
    DisplayCodecContext m_context;
    quint32 m_lastSequence {0};
    QVector<quint8> m_trace;
    QVector<quint8> m_waterfall;
    QVector<quint8> m_wide;
};

} // namespace NereusSDR
