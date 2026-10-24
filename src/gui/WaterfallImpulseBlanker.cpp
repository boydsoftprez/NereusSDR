// no-port-check: NereusSDR-original. NB Blank's row decision, without a widget.
// =================================================================
// src/gui/WaterfallImpulseBlanker.cpp  (NereusSDR)
// =================================================================
// The design and what it keeps from AetherSDR are in the header.
// Based on AetherSDR's waterfall blanker, https://github.com/ten9876/AetherSDR
// (project lead Jeremy Fielder, KK7GWY; project-level GNU GPLv3, no per-file
// header upstream).
// Modification history (NereusSDR):
//   2026-10-22  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code
//                 (Claude Opus 5.5): new. Median row level, median
//                 baseline of accepted rows, two-row hold limit.
// =================================================================

#include "gui/WaterfallImpulseBlanker.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace NereusSDR {

namespace {

// The median of [first, first + n), n > 0, reordering the range. An even
// count gives the mean of the two middle values. O(n): nth_element, then
// the largest value of the lower half.
float medianInPlace(float* first, std::size_t n)
{
    float* const mid = first + n / 2;
    std::nth_element(first, mid, first + n);
    const float upper = *mid;
    if (n % 2 == 1) {
        return upper;
    }
    const float lower = *std::max_element(first, mid);
    return 0.5f * (lower + upper);
}

} // namespace

WaterfallImpulseBlanker::Decision
WaterfallImpulseBlanker::judge(const float* rowDb, std::size_t count, float thresholdDb)
{
    constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
    Decision decision;
    decision.rowDb = kNaN;
    decision.baselineDb = kNaN;

    m_scratch.clear();
    for (std::size_t i = 0; i < count; ++i) {
        if (std::isfinite(rowDb[i])) {
            m_scratch.push_back(rowDb[i]);
        }
    }
    if (m_scratch.empty()) {
        decision.verdict = Verdict::NoFiniteBins;
        return decision;
    }
    decision.rowDb = medianInPlace(m_scratch.data(), m_scratch.size());

    // Compute rolling baseline [original inline comment from AetherSDR src/gui/SpectrumWidget.cpp:8441 [@1e0718ad]]
    // Detect impulse (need ≥8 rows of history) [original inline comment from AetherSDR src/gui/SpectrumWidget.cpp:8448 [@1e0718ad]]
    // NereusSDR: the median of the ring, not its mean.
    bool burst = false;
    if (m_ringCount >= kMinRows) {
        std::array<float, kRingRows> ring = m_ring;
        decision.baselineDb = medianInPlace(ring.data(), static_cast<std::size_t>(m_ringCount));
        burst = (decision.rowDb - decision.baselineDb) > thresholdDb;
    }

    if (!burst) {
        m_burstRun = 0;
        accept(decision.rowDb);
        decision.verdict = Verdict::Accepted;
        return decision;
    }
    if (m_burstRun < kMaxConsecutiveRows) {
        ++m_burstRun;
        decision.verdict = Verdict::Blanked;
        return decision;
    }
    // The run stays at the limit until a row that is not a burst, so every
    // further row of a lasting rise is accepted while the baseline follows.
    accept(decision.rowDb);
    decision.verdict = Verdict::AcceptedOverHold;
    return decision;
}

void WaterfallImpulseBlanker::accept(float rowDb)
{
    m_ring[static_cast<std::size_t>(m_ringIdx)] = rowDb;
    m_ringIdx = (m_ringIdx + 1) % kRingRows;
    if (m_ringCount < kRingRows) {
        ++m_ringCount;
    }
}

void WaterfallImpulseBlanker::reset()
{
    m_ringIdx = 0;
    m_ringCount = 0;
    m_burstRun = 0;
}

} // namespace NereusSDR
