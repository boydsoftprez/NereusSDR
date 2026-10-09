// =================================================================
// src/core/audio/AudioDelayProbe.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Audio delay probe (V-HW-8); see
// AudioDelayProbe.h.  No Thetis logic.
//
// Modification history (NereusSDR):
//   2026-10-08: native audio plan Task 1 (V-HW-8). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AudioDelayProbe.h"

#include <QMutexLocker>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace NereusSDR {

std::int64_t audioProbeNowNs()
{
    return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                         std::chrono::steady_clock::now().time_since_epoch())
                                         .count());
}

std::int64_t audioProbeCaptureNs(std::int64_t nowNs, double currentTime,
                                 double inputBufferAdcTime, int frames, int sampleRate,
                                 double inputLatencySeconds)
{
    if (inputBufferAdcTime != 0.0) {
        return nowNs - static_cast<std::int64_t>(
                           std::llround((currentTime - inputBufferAdcTime) * 1e9));
    }
    double ageSeconds = 0.0;
    if (sampleRate > 0 && frames > 0) {
        ageSeconds += static_cast<double>(frames) / static_cast<double>(sampleRate);
    }
    if (std::isfinite(inputLatencySeconds) && inputLatencySeconds > 0.0) {
        ageSeconds += inputLatencySeconds;
    }
    return nowNs - static_cast<std::int64_t>(std::llround(ageSeconds * 1e9));
}

// ── Clicker ────────────────────────────────────────────────────────────────

bool AudioDelayProbeClicker::process(float* interleaved, int frames, int channels)
{
    if (interleaved == nullptr || frames <= 0 || channels <= 0) {
        return false;
    }
    bool started = false;
    if (!m_started || m_framesSinceStart >= kIntervalFrames) {
        m_started = true;
        m_framesSinceStart = 0;
        m_remaining = kClickFrames;
        started = true;
    }
    const int clickFrames = std::min(m_remaining, frames);
    for (int f = 0; f < clickFrames; ++f) {
        float* frame = interleaved + static_cast<std::ptrdiff_t>(f) * channels;
        for (int c = 0; c < channels; ++c) {
            frame[c] += kClickLevel;
        }
    }
    m_remaining -= clickFrames;
    m_framesSinceStart += frames;
    return started;
}

// ── Detector ───────────────────────────────────────────────────────────────

AudioDelayProbeDetector::AudioDelayProbeDetector(int sampleRate)
    : m_sampleRate(std::max(1, sampleRate))
    , m_windowFrames(std::max(1.0, static_cast<double>(m_sampleRate) * kRmsWindowMs / 1000.0))
    , m_holdOffFrames(static_cast<std::int64_t>(m_sampleRate) * kHoldOffMs / 1000)
{
}

std::optional<std::int64_t> AudioDelayProbeDetector::process(const float* mono, int frames,
                                                              std::int64_t captureNsOfFrame0)
{
    if (mono == nullptr || frames <= 0) {
        return std::nullopt;
    }
    std::optional<std::int64_t> hit;
    const auto windowFrames = static_cast<std::int64_t>(m_windowFrames);
    for (int i = 0; i < frames; ++i) {
        const double x = std::isfinite(mono[i]) ? static_cast<double>(mono[i]) : 0.0;
        // The threshold comes from the input before this sample, and only
        // once a full RMS window has been seen.
        if (!hit && m_framesSeen >= windowFrames
            && (!m_hadHit || m_framesSinceHit >= m_holdOffFrames)) {
            const double threshold = std::max(static_cast<double>(kMinThreshold),
                                              static_cast<double>(kRmsFactor)
                                                  * std::sqrt(m_meanSquare));
            if (std::abs(x) > threshold) {
                hit = captureNsOfFrame0
                      + static_cast<std::int64_t>(std::llround(
                          static_cast<double>(i) * 1e9 / static_cast<double>(m_sampleRate)));
                m_hadHit = true;
                m_framesSinceHit = 0;
            }
        }
        // Mean over the first window, then a running mean of one window.
        const double divisor = (m_framesSeen < windowFrames)
                                   ? static_cast<double>(m_framesSeen + 1)
                                   : m_windowFrames;
        m_meanSquare += (x * x - m_meanSquare) / divisor;
        ++m_framesSeen;
        ++m_framesSinceHit;
    }
    return hit;
}

// ── Matcher ────────────────────────────────────────────────────────────────

void AudioDelayProbeMatcher::addClick(std::int64_t clickNs)
{
    QMutexLocker lock(&m_mutex);
    m_lastClickNs = clickNs;
}

void AudioDelayProbeMatcher::addHit(std::int64_t captureNs)
{
    QMutexLocker lock(&m_mutex);
    if (!m_lastClickNs) {
        return;
    }
    const std::int64_t delayNs = captureNs - *m_lastClickNs;
    if (delayNs < 0 || delayNs > kPairWindowNs) {
        return;
    }
    // One hit per click.
    m_lastClickNs.reset();
    m_pairsMs.push_back(static_cast<double>(delayNs) / 1.0e6);
}

void AudioDelayProbeMatcher::setReadoutMs(std::optional<double> ms)
{
    QMutexLocker lock(&m_mutex);
    m_readoutMs = ms;
}

QString AudioDelayProbeMatcher::takeSummary()
{
    QMutexLocker lock(&m_mutex);
    if (static_cast<int>(m_pairsMs.size()) < kSummaryEvery) {
        return {};
    }
    std::vector<double> sorted = m_pairsMs;
    m_pairsMs.clear();
    std::sort(sorted.begin(), sorted.end());
    const std::size_t n = sorted.size();
    const double median = (n % 2 == 1) ? sorted[n / 2]
                                       : (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
    const auto ms = [](double v) { return QString::number(v, 'f', 1); };
    QString line = QStringLiteral("Audio delay probe: median %1 ms, min %2, max %3 over %4 clicks")
                       .arg(ms(median), ms(sorted.front()), ms(sorted.back()))
                       .arg(n);
    if (m_readoutMs) {
        line += QStringLiteral("; readout %1 ms").arg(ms(*m_readoutMs));
    } else {
        line += QStringLiteral("; readout not available");
    }
    return line;
}

} // namespace NereusSDR
