#pragma once
// =================================================================
// src/gui/AudioClockEstimator.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. R-R3-35 measured audio delay: the
// Core-to-GUI clock offset from probe exchanges, and the one-way delay of
// the audio this computer plays, each with its accuracy. Pure arithmetic;
// it owns no session, timer, receiver or wording.
// =================================================================

#include "core/session/media/RemoteAudioReceiver.h"

#include <QtGlobal>

#include <cstddef>
#include <deque>
#include <optional>

namespace NereusSDR {

/// One clock-probe exchange, in nanoseconds. t0 (probe sent) and t3 (echo
/// received) are this computer's clock; t1 (probe received) and t2 (echo
/// sent) are the Core's.
struct AudioClockSample {
    qint64 t0 = 0;
    qint64 t1 = 0;
    qint64 t2 = 0;
    qint64 t3 = 0;
};

/// The Core clock minus this computer's clock, from the chosen sample.
struct AudioClockOffset {
    qint64 offsetNs = 0;
    /// The chosen sample's round trip without the Core's own time,
    /// (t3 - t0) - (t2 - t1). Half of it bounds the offset's error.
    qint64 roundTripNs = 0;
    /// When the chosen sample was taken (its t3), on this computer's clock.
    qint64 sampleNs = 0;
    /// The chosen exchange's length on this computer's clock, t3 - t0. The
    /// offset it measures belongs to some instant inside it.
    qint64 exchangeNs = 0;
    /// The offset's error bound applied at `localNs`: half the round trip
    /// plus the largest clock drift from the exchange to `localNs`.
    double boundNsAt(qint64 localNs) const;
};

/// Keeps the probe samples of the last kWindowNs and uses the one with the
/// lowest round trip. For any path, symmetric or not, the true offset lies
/// within half that round trip of the estimate:
///   offset = ((t1 - t0) + (t2 - t3)) / 2
///   round trip = (t3 - t0) - (t2 - t1)
/// Half the round trip is an accuracy, never a delay.
class AudioClockEstimator {
public:
    /// About 16 s of probes, one a second: long enough that one exchange
    /// with little queueing on either path is usually among them.
    static constexpr qint64 kWindowNs = 16'000'000'000;
    /// The largest rate difference assumed between the two clocks: each is
    /// a steady clock from a crystal oscillator within +-50 ppm, so 100 ppm
    /// covers two at opposite limits. Over a 16 s old sample it adds 1.6 ms.
    static constexpr double kMaxDriftPpm = 100.0;
    /// Echoes stop (three probes unanswered): nothing is measured.
    static constexpr qint64 kEchoStaleNs = 3'000'000'000;

    /// False (and nothing kept) for an impossible sample: an echo received
    /// before its probe was sent, or a Core that answered before it heard.
    bool addSample(const AudioClockSample& sample);
    void reset();
    /// The offset, or nothing when no sample is younger than kEchoStaleNs
    /// at `nowNs` (this computer's clock).
    std::optional<AudioClockOffset> offset(qint64 nowNs) const;
    std::size_t sampleCount() const { return m_samples.size(); }

private:
    struct Entry {
        qint64 sampleNs = 0;
        qint64 offsetNs = 0;
        qint64 roundTripNs = 0;
        qint64 exchangeNs = 0;
    };
    std::deque<Entry> m_samples;
};

/// What the Core said it captured: the end of its newest audio block, as an
/// RTP time and the Core clock's reading then, for one audio context.
struct AudioCaptureAnchor {
    quint32 generation = 0;
    quint32 rtpTimestamp = 0;
    qint64 capturedNs = 0;
};

/// The measured one-way audio delay (R-R3-35), in milliseconds.
struct AudioDelayEstimate {
    /// From the Core capturing a sample to this computer playing it out,
    /// including the device's own latency only when includesDevice.
    double delayMs = 0;
    /// The true delay lies within delayMs +- boundMs: the clock offset's
    /// bound (half the round trip plus drift) plus the playout point's own
    /// accuracy (RemoteAudioPlayoutPoint::accuracyNs()).
    double boundMs = 0;
    bool includesDevice = false;
    /// From the Core capturing a sample to its packet leaving this
    /// computer's reorder buffer for playback, with its own bound.
    std::optional<double> deliveryMs;
    std::optional<double> deliveryBoundMs;
    bool operator==(const AudioDelayEstimate&) const = default;
};

/// Everything one measurement needs, all current.
struct AudioDelayInputs {
    std::optional<AudioClockOffset> offset;
    std::optional<AudioCaptureAnchor> capture;
    /// The audio context this computer is playing (its generation).
    quint32 playingGeneration = 0;
    std::optional<RemoteAudioPlayoutPoint> playout;
    std::optional<RemoteAudioReleasePoint> release;
};

/// The delay, or nothing when any part is missing, the Core's capture
/// belongs to another audio context, or the RTP times are more than a
/// minute apart (not one continuous stream).
std::optional<AudioDelayEstimate> measureAudioDelay(const AudioDelayInputs& inputs);

/// A delay rounded for display so the shown interval still contains the
/// true value: whole milliseconds, and an accuracy rounded up by at least
/// the rounding of the value, never below 1 ms.
struct AudioDelayDisplay {
    qint64 valueMs = 0;
    qint64 accuracyMs = 1;
};
AudioDelayDisplay roundAudioDelay(double valueMs, double boundMs);

} // namespace NereusSDR
