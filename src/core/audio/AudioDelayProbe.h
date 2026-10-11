// =================================================================
// src/core/audio/AudioDelayProbe.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Audio delay probe (V-HW-8): a
// short click added to the speakers block once a second, a detector that
// finds it again in the microphone helper's input, and a matcher that
// pairs the two times and logs the delay; no Thetis logic.
//
// The probe measures the speaker-to-microphone delay through a loopback
// cable, on the build before the native audio engines and after, so the
// change can be compared with the delay each engine reports.
//
// Clock: every time here is std::chrono::steady_clock nanoseconds, the
// clock CaptureProtocol's sentMonotonicNs already uses in both processes
// (host-wide monotonic on macOS, Linux and Windows).
//
// Modification history (NereusSDR):
//   2026-10-08: native audio plan Task 1 (V-HW-8). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-17): the detector also
//               takes the stereo block an input sink receives.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QMutex>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace NereusSDR {

// std::chrono::steady_clock nanoseconds, the clock CaptureProtocol.h's
// sentMonotonicNs uses in both processes.
std::int64_t audioProbeNowNs();

// When frame 0 of a capture callback block reached the converter.  With
// PortAudio's inputBufferAdcTime nonzero:
//   nowNs - (currentTime - inputBufferAdcTime) * 1e9
// otherwise the block's own length and the stream's reported input latency:
//   nowNs - frames * 1e9 / sampleRate - inputLatencySeconds * 1e9
// Pure; no lock, no allocation.
std::int64_t audioProbeCaptureNs(std::int64_t nowNs, double currentTime,
                                 double inputBufferAdcTime, int frames, int sampleRate,
                                 double inputLatencySeconds);

// Adds the click to the speakers block.  DSP thread only.
class AudioDelayProbeClicker {
public:
    static constexpr int kClickFrames = 48;
    static constexpr float kClickLevel = 0.5f;
    static constexpr int kIntervalFrames = 48000;

    // Adds the click into an interleaved float block in place. Returns true
    // when a click starts at frame 0 of this block (the caller stamps the
    // push time).  A click starts at frame 0 of the first block, then of the
    // first block whose start is at least kIntervalFrames after the
    // previous click start; one that does not fit in its block continues
    // into the next.
    bool process(float* interleaved, int frames, int channels);

private:
    bool m_started = false;
    std::int64_t m_framesSinceStart = 0;
    int m_remaining = 0;
};

// Finds the click in mono input.  Device callback only; no lock, no
// allocation.
class AudioDelayProbeDetector {
public:
    static constexpr float kMinThreshold = 0.02f;
    static constexpr float kRmsFactor = 8.0f;
    static constexpr int kRmsWindowMs = 100;
    static constexpr int kHoldOffMs = 500;

    explicit AudioDelayProbeDetector(int sampleRate);

    // captureNsOfFrame0: when frame 0 of this buffer reached the converter.
    // Returns the capture time of a detected click, else std::nullopt.
    // A click is the first sample whose magnitude exceeds
    // max(kMinThreshold, kRmsFactor x the RMS of the input before it), at
    // least kHoldOffMs after the previous click.
    std::optional<std::int64_t> process(const float* mono, int frames,
                                        std::int64_t captureNsOfFrame0);
    // The same on interleaved stereo, as IAudioInputSink::onInput gets it,
    // reading channel 0: the picked mic channel, which the sink carries
    // in both channels (R-AUD-17).
    std::optional<std::int64_t> processStereo(const float* stereo, int frames,
                                              std::int64_t captureNsOfFrame0);

private:
    std::optional<std::int64_t> processStrided(const float* samples, int frames, int stride,
                                               std::int64_t captureNsOfFrame0);

    int m_sampleRate;
    double m_windowFrames;
    std::int64_t m_holdOffFrames;
    double m_meanSquare = 0.0;
    std::int64_t m_framesSeen = 0;
    std::int64_t m_framesSinceHit = 0;
    bool m_hadHit = false;
};

// Pairs click and capture times and makes the summary line.  Thread-safe.
class AudioDelayProbeMatcher {
public:
    static constexpr std::int64_t kPairWindowNs = 500'000'000;
    static constexpr int kSummaryEvery = 30;

    void addClick(std::int64_t clickNs);
    // Pairs with the latest click within the window.
    void addHit(std::int64_t captureNs);
    // Task 6 feeds it.
    void setReadoutMs(std::optional<double> ms);
    // The log line once every kSummaryEvery pairs, else empty.
    QString takeSummary();

private:
    mutable QMutex m_mutex;
    std::optional<std::int64_t> m_lastClickNs;
    std::vector<double> m_pairsMs;
    std::optional<double> m_readoutMs;
};

} // namespace NereusSDR
