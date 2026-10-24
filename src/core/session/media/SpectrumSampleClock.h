#pragma once
// =================================================================
// src/core/session/media/SpectrumSampleClock.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Stamps the Core's spectrum frames with
// the time of their samples instead of the moment the FFT was published.
//
// Modification history (NereusSDR):
//   2026-10-21: Created. Steady waterfall row times at the source: each
//               frame's producedAtNs follows the I/Q sample count on the
//               producer clock, so the stamps carry no FFT or thread
//               scheduling jitter. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QtGlobal>

namespace NereusSDR {

/// The time of a spectrum frame's samples on the producer clock
/// (DaemonSpectrumSource::monotonicNowNs, steady nanoseconds).
///
/// A frame is published some time after its last sample arrived: the FFT,
/// the engine thread's queue and the radio's packet batching all add a
/// varying delay. The publish time therefore jitters by milliseconds while
/// the samples themselves are evenly spaced. This clock reads time from the
/// sample count instead: stamp = anchor + (position - anchorPosition) /
/// sampleRate, where the position is how many I/Q pairs reached the engine
/// (FFTEngine::frameInputPosition). Decimation, the frame rate and the FFT
/// advance all only change which positions frames fall on, so none of them
/// needs a new anchor.
///
/// The anchor is a publish time, so the stamps sit where the publish times
/// did and the wire meaning of producerTimestamp (the Core time the frame's
/// samples were taken, displayClockVersion) is unchanged. It follows the
/// earliest publish times: a frame published before the clock's time moves
/// the anchor to itself, so no stamp is ever later than its publish, and
/// the anchor creeps up by a small part of a later publish (at most
/// kMaxLateStepNs a frame), which follows any difference between the
/// radio's sample clock and the Core's clock without taking the jitter.
/// Settled, the stamps sit within a fraction of a millisecond of the
/// earliest publish latency.
///
/// A new anchor is taken from the frame's own publish time for the first
/// frame, after reset() (the source restarted or its input history was
/// cleared, so samples were dropped), when the sample rate changes, when
/// the position runs backwards, and on an error larger than jitter: a
/// frame more than kReanchorErrorNs earlier than the clock (an anchor taken
/// from a late first frame), or more than kMaxLateNs later (a stall, a
/// pause in the I/Q, lost samples, or I/Q that arrives slower than its
/// rate). Such a frame is stamped with its publish time, as every frame was
/// before this clock. Stamps always increase, by at least a nanosecond.
///
/// Not thread-safe; DaemonSpectrumSource holds it under its frame lock.
class SpectrumSampleClock {
public:
    /// A frame this much earlier than the clock: the anchor was late.
    static constexpr qint64 kReanchorErrorNs = 100'000'000;
    /// A frame this much later than the clock is not publish jitter: the
    /// I/Q stalled, paused or lost samples, and the sample count no longer
    /// tells the time.
    static constexpr qint64 kMaxLateNs = 10'000'000;
    /// A late frame moves the anchor up by its lateness over this, and by
    /// at most kMaxLateStepNs: 50 microseconds a frame follows a clock
    /// difference of 1500 ppm at 30 fps, far beyond any radio's.
    static constexpr qint64 kLateCreepDivisor = 256;
    static constexpr qint64 kMaxLateStepNs = 50'000;

    /// The stamp for a frame whose samples end at `inputPosition` (I/Q
    /// pairs the engine has taken, counted before decimation) at
    /// `sampleRateHz`, published at `nowNs`.
    qint64 stamp(qint64 inputPosition, double sampleRateHz, qint64 nowNs);

    /// The next frame takes a new anchor. The increasing-stamp rule holds
    /// across it.
    void reset();

    /// Anchors taken for an error larger than jitter (kReanchorErrorNs,
    /// kMaxLateNs), not counting the first anchor or one after reset().
    quint64 largeErrorReanchors() const { return m_largeErrorReanchors; }

private:
    void anchor(qint64 inputPosition, double sampleRateHz, qint64 nowNs);

    bool m_anchored{false};
    double m_sampleRateHz{0.0};
    qint64 m_anchorNs{0};
    qint64 m_anchorPosition{0};
    qint64 m_lastPosition{0};
    bool m_hasStamp{false};
    qint64 m_lastStampNs{0};
    quint64 m_largeErrorReanchors{0};
};

} // namespace NereusSDR
