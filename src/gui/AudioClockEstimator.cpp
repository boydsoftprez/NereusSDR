// =================================================================
// src/gui/AudioClockEstimator.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. R-R3-35 clock offset and audio delay
// arithmetic; see AudioClockEstimator.h.
// =================================================================

#include "gui/AudioClockEstimator.h"

#include "core/session/media/PcmAudioCodec.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>

namespace NereusSDR {
namespace {

constexpr double kNsPerMs = 1'000'000.0;
// Farther apart than this, a playout RTP time and the Core's capture RTP
// time are not one continuous stream.
constexpr qint64 kMaxRtpSpanFrames = qint64(60) * PcmAudioCodecConfig::kSampleRate;

double driftNs(double elapsedNs)
{
    return std::abs(elapsedNs) * AudioClockEstimator::kMaxDriftPpm / 1'000'000.0;
}

} // namespace

double AudioClockOffset::boundNsAt(qint64 localNs) const
{
    return double(roundTripNs) / 2.0
        + driftNs(std::abs(double(localNs - sampleNs)) + double(exchangeNs));
}

bool AudioClockEstimator::addSample(const AudioClockSample& sample)
{
    const qint64 coreHeldNs = sample.t2 - sample.t1;
    const qint64 roundTripNs = (sample.t3 - sample.t0) - coreHeldNs;
    if (sample.t3 < sample.t0 || coreHeldNs < 0 || roundTripNs < 0) {
        return false;
    }
    const qint64 offsetNs = ((sample.t1 - sample.t0) + (sample.t2 - sample.t3)) / 2;
    m_samples.push_back({sample.t3, offsetNs, roundTripNs, sample.t3 - sample.t0});
    while (!m_samples.empty() && m_samples.front().sampleNs < sample.t3 - kWindowNs) {
        m_samples.pop_front();
    }
    return true;
}

void AudioClockEstimator::reset()
{
    m_samples.clear();
}

std::optional<AudioClockOffset> AudioClockEstimator::offset(qint64 nowNs) const
{
    if (m_samples.empty() || nowNs - m_samples.back().sampleNs > kEchoStaleNs) {
        return std::nullopt;
    }
    const Entry* chosen = nullptr;
    for (const Entry& entry : m_samples) {
        if (entry.sampleNs < nowNs - kWindowNs) {
            continue;
        }
        // The lowest round trip; among equals the newest, which has drifted
        // least.
        if (!chosen || entry.roundTripNs <= chosen->roundTripNs) {
            chosen = &entry;
        }
    }
    if (!chosen) {
        return std::nullopt;
    }
    return AudioClockOffset{chosen->offsetNs, chosen->roundTripNs, chosen->sampleNs,
                            chosen->exchangeNs};
}

std::optional<AudioDelayEstimate> measureAudioDelay(const AudioDelayInputs& inputs)
{
    if (!inputs.offset || !inputs.capture || !inputs.playout
        || inputs.capture->generation == 0
        || inputs.capture->generation != inputs.playingGeneration) {
        return std::nullopt;
    }
    const AudioClockOffset& offset = *inputs.offset;
    const AudioCaptureAnchor& capture = *inputs.capture;
    // The Core's capture anchor on this computer's clock, in whole ns, so
    // large clock readings lose no precision before they are subtracted.
    const qint64 anchorLocalNs = capture.capturedNs - offset.offsetNs;
    // How long before `atNs` (this computer's clock) the Core captured the
    // sample at `rtpTimestamp`, and the bound on that time.
    struct Captured {
        double sinceNs;
        double boundNs;
    };
    const auto captured = [&](quint32 rtpTimestamp, qint64 atNs) -> std::optional<Captured> {
        const qint64 spanFrames = std::bit_cast<qint32>(quint32(rtpTimestamp - capture.rtpTimestamp));
        if (std::abs(spanFrames) > kMaxRtpSpanFrames) {
            return std::nullopt;
        }
        const double spanNs = double(spanFrames) * 1e9 / double(PcmAudioCodecConfig::kSampleRate);
        // Half the round trip, plus drift from the chosen exchange to the
        // capture and over the span counted in audio frames.
        const double boundNs = offset.boundNsAt(anchorLocalNs + std::llround(spanNs))
            + driftNs(spanNs);
        return Captured{double(atNs - anchorLocalNs) - spanNs, boundNs};
    };

    const RemoteAudioPlayoutPoint& playout = *inputs.playout;
    const std::optional<Captured> heard = captured(playout.rtpTimestamp, playout.playoutNs());
    if (!heard) {
        return std::nullopt;
    }
    AudioDelayEstimate estimate;
    // The delay of the audio heard at the reading, not of the newest sample
    // behind it: while the rate matcher corrects, the two differ by its
    // stretch of the audio between them.
    estimate.delayMs = (heard->sinceNs - double(playout.matcherStretchNs())) / kNsPerMs;
    // The clock bound, plus how far the playout time itself may be off
    // (where the device callback was in its cycle, and the queue read).
    estimate.boundMs = (heard->boundNs + double(playout.accuracyNs())) / kNsPerMs;
    estimate.includesDevice = playout.deviceLatencyNs.has_value();
    if (inputs.release) {
        if (const std::optional<Captured> released =
                captured(inputs.release->rtpTimestamp, inputs.release->releasedNs)) {
            estimate.deliveryMs = released->sinceNs / kNsPerMs;
            estimate.deliveryBoundMs = released->boundNs / kNsPerMs;
        }
    }
    return estimate;
}

AudioDelayDisplay roundAudioDelay(double valueMs, double boundMs)
{
    const double rounded = std::round(valueMs);
    const double widened = std::max(0.0, boundMs) + std::abs(valueMs - rounded);
    return {static_cast<qint64>(rounded),
            std::max<qint64>(1, static_cast<qint64>(std::ceil(widened)))};
}

} // namespace NereusSDR
