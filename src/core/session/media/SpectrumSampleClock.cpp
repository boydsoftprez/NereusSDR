// =================================================================
// src/core/session/media/SpectrumSampleClock.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See SpectrumSampleClock.h.
//
// Modification history (NereusSDR):
//   2026-10-21: Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/media/SpectrumSampleClock.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

void SpectrumSampleClock::reset()
{
    m_anchored = false;
}

void SpectrumSampleClock::anchor(qint64 inputPosition, double sampleRateHz, qint64 nowNs)
{
    m_anchored = true;
    m_sampleRateHz = sampleRateHz;
    m_anchorNs = nowNs;
    m_anchorPosition = inputPosition;
}

qint64 SpectrumSampleClock::stamp(qint64 inputPosition, double sampleRateHz, qint64 nowNs)
{
    qint64 stampNs = nowNs;
    if (!(sampleRateHz > 0.0) || !std::isfinite(sampleRateHz)) {
        // No rate to count with: the publish time, as before this clock.
        m_anchored = false;
    } else if (!m_anchored || sampleRateHz != m_sampleRateHz
               || inputPosition < m_lastPosition) {
        anchor(inputPosition, sampleRateHz, nowNs);
    } else {
        const double elapsedNs = static_cast<double>(inputPosition - m_anchorPosition)
            * 1.0e9 / m_sampleRateHz;
        const qint64 clockNs = m_anchorNs + std::llround(elapsedNs);
        const qint64 lateNs = nowNs - clockNs;
        if (lateNs < -kReanchorErrorNs || lateNs > kMaxLateNs) {
            // Far early: the anchor was taken from a frame published far
            // later than its samples (the engine's first plan, a stall at
            // start). Late beyond jitter: the I/Q stalled, paused or lost
            // samples. Either way the frame's publish time is the anchor.
            anchor(inputPosition, sampleRateHz, nowNs);
            ++m_largeErrorReanchors;
        } else {
            // Early: the anchor was later than this frame, which is now the
            // earliest. Late: a small part of it, toward a slower radio.
            const qint64 stepNs = lateNs < 0
                ? lateNs
                : std::min(lateNs / kLateCreepDivisor, kMaxLateStepNs);
            m_anchorNs += stepNs;
            stampNs = clockNs + stepNs;
        }
    }
    m_lastPosition = inputPosition;
    if (m_hasStamp && stampNs <= m_lastStampNs) {
        stampNs = m_lastStampNs + 1;
    }
    m_hasStamp = true;
    m_lastStampNs = stampNs;
    return stampNs;
}

} // namespace NereusSDR
