// =================================================================
// src/core/SsbModulationAnalyzer.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original file. See the header for the design.
//
// Modification history (NereusSDR):
//   2026-11-04 — Created for the SSB Mod Monitor. J.J. Boyd (KG4VCF),
//                AI-assisted via xAI Grok.
// =================================================================
#include "core/SsbModulationAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

SsbModulationAnalyzer::SsbModulationAnalyzer()
{
    std::lock_guard<std::mutex> lock(m_mu);
    rebuildScopeLocked();
}

void SsbModulationAnalyzer::setSampleRate(int hz)
{
    std::lock_guard<std::mutex> lock(m_mu);
    if (hz <= 0 || hz == m_fs) {
        return;
    }
    m_fs = hz;
    rebuildScopeLocked();
}

int SsbModulationAnalyzer::sampleRate() const
{
    std::lock_guard<std::mutex> lock(m_mu);
    return m_fs;
}

void SsbModulationAnalyzer::setAverageTimeConstantMs(double ms)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_avgTcMs = std::max(10.0, ms);
}

void SsbModulationAnalyzer::setPeakHoldMs(double ms)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_peakHoldMs = std::max(0.0, ms);
}

void SsbModulationAnalyzer::setPresentMinimum(double linear)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_presentMin = std::max(0.0, linear);
}

void SsbModulationAnalyzer::setScopeWindowSeconds(double seconds)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_scopeSeconds = std::clamp(seconds, 0.05, 5.0);
    rebuildScopeLocked();
}

void SsbModulationAnalyzer::rebuildScopeLocked()
{
    // About 8 kHz on the scope: enough to draw a voice envelope, cheap
    // enough to repaint at 30 fps. The kept sample of each group is the
    // peak, so decimation does not hide a spike.
    m_decim = std::max(1, m_fs / 8000);
    const int scopeRate = m_fs / m_decim;
    const std::size_t n = static_cast<std::size_t>(
        std::max(64.0, m_scopeSeconds * static_cast<double>(scopeRate)));
    m_scope.assign(n, 0.0f);
    m_scopeHead = 0;
    m_scopeFilled = false;
    m_decimCount = 0;
    m_decimAcc = 0.0f;
}

void SsbModulationAnalyzer::reset()
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_avg = 0.0;
    m_avgSeeded = false;
    m_seedSum = 0.0;
    m_seedCount = 0;
    m_winMax = 0.0;
    m_winValid = false;
    m_holdPeak = 0.0;
    m_holdPct = 0.0;
    m_holdAt = clock::time_point{};
    m_framesSeen = 0;
    rebuildScopeLocked();
}

void SsbModulationAnalyzer::pushScopeLocked(float pct)
{
    if (m_decimCount == 0 || pct > m_decimAcc) {
        m_decimAcc = pct;
    }
    if (++m_decimCount >= m_decim) {
        m_scope[m_scopeHead] = m_decimAcc;
        m_scopeHead = (m_scopeHead + 1) % m_scope.size();
        if (m_scopeHead == 0) {
            m_scopeFilled = true;
        }
        m_decimCount = 0;
        m_decimAcc = 0.0f;
    }
}

double SsbModulationAnalyzer::toPct(double env) noexcept
{
    return std::max(0.0, env / kFullScale * 100.0);
}

double SsbModulationAnalyzer::toDbfs(double env) noexcept
{
    return env > 0.0 ? 20.0 * std::log10(env / kFullScale) : -120.0;
}

void SsbModulationAnalyzer::pushIq(const float* interleaved, int frames)
{
    if (interleaved == nullptr || frames <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mu);

    double sum = 0.0;
    double blockMax = 0.0;
    for (int i = 0; i < frames; ++i) {
        const double re = interleaved[2 * i];
        const double im = interleaved[2 * i + 1];
        const double env = std::sqrt(re * re + im * im);
        sum += env;
        blockMax = std::max(blockMax, env);
        pushScopeLocked(static_cast<float>(toPct(env)));
    }
    const double mean = sum / static_cast<double>(frames);

    if (!m_avgSeeded) {
        m_seedSum += sum;
        m_seedCount += frames;
        m_avg = m_seedSum / static_cast<double>(m_seedCount);
        const long seedTarget = std::max(2048L, static_cast<long>(m_fs / 10));
        if (m_seedCount >= seedTarget) {
            m_avgSeeded = true;
        }
    } else {
        const double tcSamples = m_avgTcMs * 1e-3 * static_cast<double>(m_fs);
        const double a = 1.0 - std::exp(-static_cast<double>(frames) / tcSamples);
        m_avg += (mean - m_avg) * a;
    }

    if (!m_winValid) {
        m_winMax = blockMax;
        m_winValid = true;
    } else {
        m_winMax = std::max(m_winMax, blockMax);
    }

    const double blockPct = toPct(blockMax);
    if (blockPct >= m_holdPeak) {
        m_holdPeak = blockPct;
        m_holdPct = blockPct;
        m_holdAt = clock::now();
    }

    m_framesSeen += static_cast<std::uint64_t>(frames);
}

SsbModulationAnalyzer::Snapshot SsbModulationAnalyzer::snapshot()
{
    std::lock_guard<std::mutex> lock(m_mu);
    Snapshot snap;
    snap.framesSeen = m_framesSeen;

    const bool rf = m_avgSeeded && (m_avg > m_presentMin || (m_winValid && m_winMax > m_presentMin));
    snap.signalPresent = rf;
    if (rf && m_winValid) {
        snap.peakPct = toPct(m_winMax);
        snap.peakDbfs = toDbfs(m_winMax);
    }
    if (rf) {
        snap.avgPct = toPct(m_avg);
        snap.avgDbfs = toDbfs(m_avg);
    }
    m_winValid = false;

    const auto now = clock::now();
    if (m_holdPeak > 0.0) {
        const double ms = std::chrono::duration<double, std::milli>(now - m_holdAt).count();
        if (ms <= m_peakHoldMs) {
            m_holdPct = m_holdPeak;
        } else {
            // 100 percent per second after the hold, from the original peak.
            m_holdPct = std::max(0.0, m_holdPeak - (ms - m_peakHoldMs) * 0.1);
            if (m_holdPct <= 0.0) {
                m_holdPeak = 0.0;
            }
        }
    }
    snap.peakHoldPct = m_holdPct;
    snap.peakHoldDbfs = m_holdPct > 0.0 ? toDbfs(m_holdPct / 100.0 * kFullScale) : -120.0;
    if (rf && m_avg > m_presentMin && m_holdPct > 0.0) {
        snap.crestDb = 20.0 * std::log10((m_holdPct / 100.0 * kFullScale) / m_avg);
    }

    snap.scopeRateHz = m_fs / std::max(1, m_decim);
    if (m_scopeFilled) {
        snap.scope.reserve(m_scope.size());
        snap.scope.insert(snap.scope.end(),
                          m_scope.begin() + static_cast<std::ptrdiff_t>(m_scopeHead),
                          m_scope.end());
        snap.scope.insert(snap.scope.end(),
                          m_scope.begin(),
                          m_scope.begin() + static_cast<std::ptrdiff_t>(m_scopeHead));
    } else {
        snap.scope.assign(m_scope.begin(),
                          m_scope.begin() + static_cast<std::ptrdiff_t>(m_scopeHead));
    }
    return snap;
}

} // namespace NereusSDR
