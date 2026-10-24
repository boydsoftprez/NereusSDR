// =================================================================
// src/core/IqLevelMonitor.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original diagnostics; no Thetis counterpart.
//
// Modification history (NereusSDR):
//   2026-10-22 - Created by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code (diagnostics for the STRIX Core hot noise
//                 floor on slice A).
// =================================================================

#include "core/IqLevelMonitor.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// Power ratio to dB, with an all-zero interval reported as kSilentDbfs.
double powerToDb(double power)
{
    if (!(power > 0.0)) {
        return IqLevelMonitor::kSilentDbfs;
    }
    return std::max(IqLevelMonitor::kSilentDbfs, 10.0 * std::log10(power));
}

} // namespace

void IqLevelMonitor::accumulate(int ddc, const float* interleaved, qsizetype floatCount)
{
    if (ddc < 0 || ddc >= kMaxDdc || interleaved == nullptr) {
        return;
    }
    Sums& sums = m_sums[static_cast<size_t>(ddc)];
    const qsizetype pairCount = floatCount / 2;
    for (qsizetype n = 0; n < pairCount; ++n) {
        const double i = interleaved[2 * n];
        const double q = interleaved[2 * n + 1];
        if (!std::isfinite(i) || !std::isfinite(q)) {
            ++sums.nonFinite;
            continue;
        }
        const double square = i * i + q * q;
        sums.sumSquares += square;
        if (square > sums.peakSquare) {
            sums.peakSquare = square;
        }
        ++sums.pairs;
    }
}

void IqLevelMonitor::noteAdcOverflow(int adc)
{
    if (adc < 0 || adc >= 32) {
        return;
    }
    m_overloadMask.fetch_or(quint32{1} << adc, std::memory_order_relaxed);
}

std::optional<IqLevelMonitor::Report> IqLevelMonitor::takeIfDue(qint64 nowNs)
{
    if (!m_started) {
        m_started = true;
        m_intervalStartNs = nowNs;
        return std::nullopt;
    }
    if (nowNs - m_intervalStartNs < m_intervalNs) {
        return std::nullopt;
    }
    Report report;
    report.overloadMask = m_overloadMask.exchange(0, std::memory_order_relaxed);
    for (int ddc = 0; ddc < kMaxDdc; ++ddc) {
        Sums& sums = m_sums[static_cast<size_t>(ddc)];
        if (sums.pairs == 0 && sums.nonFinite == 0) {
            continue;
        }
        DdcLevel level;
        level.ddc = ddc;
        level.pairs = sums.pairs;
        level.nonFinite = sums.nonFinite;
        if (sums.pairs > 0) {
            level.rmsDbfs = powerToDb(sums.sumSquares / static_cast<double>(sums.pairs));
            level.peakDbfs = powerToDb(sums.peakSquare);
        }
        report.levels.append(level);
        sums = Sums{};
    }
    m_intervalStartNs = nowNs;
    return report;
}

} // namespace NereusSDR
