#pragma once
// =================================================================
// src/core/session/media/DisplayCodec.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 Task 3.
//
// Bounded wire codec for reduced dBm trace, waterfall, and optional DSS-wide
// rows. The exact v1 layout and recovery rules are in
// docs/architecture/2026-09-20-display-codec-v1.md; version 2, in which
// every row and trace is coded on its own, is in
// docs/architecture/2026-10-22-display-codec-v2-intra-rows.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: NSDC v2 encoder: previous-bin Rice planes, the whole or
//               split frame, and public quantize helpers.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
//   2026-10-22: NSDC v2 decoder: version dispatch, part pairing, the Held
//               disposition, up to two frames a result, and the loss
//               counters. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QByteArray>
#include <QVector>

#include <vector>

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
    /// In process only, never encoded: the row the Core's noise floor
    /// measures when it is not traceDbm (SpectrumReducer::reduce's
    /// measurementOut): while the trace's average warms up, the average
    /// with its starting value taken out, with NaN where a pixel holds no
    /// data. Empty means traceDbm.
    QVector<float> measurementDbm;
    /// NSDC v2 only: the trace is the last decoded one, carried into a frame
    /// whose own trace part was lost. Set only by the v2 decoder.
    bool traceCarried {false};
};

enum class DisplayCodecDisposition {
    Accepted,
    NeedKeyframe,
    Rejected,
    /// NSDC v2 only: a valid part of a split frame, held for its partner.
    /// Nothing is released and the reason is None.
    Held,
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
    /// The first released frame (the oldest), when frameCount >= 1.
    DisplayCodecFrame frame;
    /// Frames released, oldest first: 1 or 2 on Accepted, 0 otherwise. The
    /// v1 path releases at most one.
    int frameCount {0};
    /// The second released frame, when frameCount == 2 (v2 only).
    DisplayCodecFrame secondFrame;
};

/// The v2 decoder's loss and reject totals (display codec v2 document,
/// section 9.2). Only the v2 path and the rejects decided before the version
/// dispatch move them; they survive reset().
struct DisplayCodecDecoderCounters {
    quint64 framesLost {0};
    quint64 partsLost {0};
    quint64 staleMessages {0};
    quint64 duplicateParts {0};
    quint64 malformed {0};
};

/// The codec's quantizer, both versions: round((dbm - minDbm) * 255 /
/// (maxDbm - minDbm)), clipped to 0..255. The interval is finite and
/// increasing.
quint8 displayCodecQuantize(float dbm, float minDbm, float maxDbm);
/// The value a receiver reconstructs from `q`, clamped to the interval.
float displayCodecDequantize(quint8 q, float minDbm, float maxDbm);

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

/// One NSDC v2 plane (display codec v2 document, section 4.2): the 4-byte
/// prefix (coding 2, block size code, payload bytes), then the previous-bin
/// Rice bit stream. Deterministic: every block keeps its fewest bits (ties to
/// the smaller k, the raw escape only when strictly fewer), and the plane
/// keeps the block size with the fewest bytes (ties to the smaller size).
/// Empty for 0 samples or more than DisplayCodecEncoder::kMaxSamplesPerPlane.
QByteArray encodeDisplayCodecV2Plane(const QVector<quint8>& samples);

/// Reads one NSDC v2 plane of exactly `length` samples from `bytes` at
/// `offset` and advances `offset` past it. False, with `offset` and
/// `samples` untouched, for anything malformed or truncated: a prefix out of
/// range, a payload that runs out or overruns `bytes`, a z above 255, payload
/// bytes left after the last sample, or a nonzero pad bit.
bool decodeDisplayCodecV2Plane(const QByteArray& bytes, int& offset, int length,
                               QVector<quint8>& samples);

/// What encodeDisplayCodecV2() produced for one frame.
struct DisplayCodecV2Encoding {
    /// One message (whole) or two (row part, then trace part).
    std::vector<QByteArray> messages;
    bool split() const { return messages.size() == 2; }
    qsizetype totalBytes() const;
};

/// Encodes one frame as NSDC v2 (sections 4 and 5 of the v2 document). It
/// keeps no history. A frame that does not advance the waterfall is one
/// trace-only message. Otherwise the frame is one whole message when that
/// fits one SCTP DATA chunk (IMediaTransport::kSctpDataPayloadBytes), and
/// else a row part then a trace part when the two together stay within
/// `admittedFrameBytes`; when they would not, it is one whole message.
/// Empty `messages` for invalid input (the same rules as v1's encode()).
DisplayCodecV2Encoding encodeDisplayCodecV2(const DisplayCodecFrame& frame,
                                            quint32 admittedFrameBytes);

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
///
/// decode() chooses by the version byte (offset 4): 1 is the v1 path, 2 the
/// v2 path (display codec v2 document, section 9), anything else is
/// UnsupportedVersion. The two paths keep separate state. A v2 message never
/// depends on history: the v2 path keeps its context, the last released
/// sequence, at most one held part, and the last released planes, which only
/// fill a released frame's absent planes.
class DisplayCodecDecoder {
public:
    DisplayCodecDecodeResult decode(const QByteArray& packet);
    /// Clears both paths' state (v2: context, last sequence, the held part
    /// and the fill planes). The counters keep their values.
    void reset();
    const DisplayCodecDecoderCounters& counters() const { return m_counters; }

private:
    enum class V2Kind { TraceOnly, Whole, TracePart, RowPart };
    struct V2Message {
        V2Kind kind {V2Kind::Whole};
        DisplayCodecContext context;
        quint32 sequence {0};
        quint64 timestamp {0};
        QVector<quint8> trace;
        QVector<quint8> waterfall;
        QVector<quint8> wide;
    };

    DisplayCodecDecodeResult decodeV1(const QByteArray& packet);
    DisplayCodecDecodeResult decodeV2(const QByteArray& packet);
    static DisplayCodecReason parseV2(const QByteArray& packet, V2Message& message);
    void startV2Context(const DisplayCodecContext& context);
    void releaseV2Message(const V2Message& message, DisplayCodecDecodeResult& decoded);
    void releaseV2(const V2Message& header, const QVector<quint8>* trace,
                   const QVector<quint8>* waterfall, const QVector<quint8>* wide,
                   bool partial, DisplayCodecDecodeResult& decoded);

    bool m_hasHistory {false};
    bool m_needsKeyframe {false};
    DisplayCodecContext m_context;
    quint32 m_lastSequence {0};
    QVector<quint8> m_trace;
    QVector<quint8> m_waterfall;
    QVector<quint8> m_wide;

    // v2 path.
    bool m_v2HasContext {false};
    DisplayCodecContext m_v2Context;
    bool m_v2HasLast {false};
    quint32 m_v2Last {0};
    bool m_v2HasHeld {false};
    V2Message m_v2Held;
    QVector<quint8> m_v2FillTrace;
    QVector<quint8> m_v2FillWaterfall;
    QVector<quint8> m_v2FillWide;

    DisplayCodecDecoderCounters m_counters;
};

} // namespace NereusSDR
