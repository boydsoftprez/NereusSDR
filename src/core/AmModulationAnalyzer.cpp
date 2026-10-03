// =================================================================
// src/core/AmModulationAnalyzer.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original file.  See header for the design.
// =================================================================
#include "core/AmModulationAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

AmModulationAnalyzer::AmModulationAnalyzer()
{
    std::lock_guard<std::mutex> lk(m_mu);
    rebuildScopeLocked();
}

void AmModulationAnalyzer::setSampleRate(int hz)
{
    std::lock_guard<std::mutex> lk(m_mu);
    if (hz <= 0 || hz == m_fs) {
        return;
    }
    m_fs = hz;
    rebuildScopeLocked();
}

int AmModulationAnalyzer::sampleRate() const
{
    std::lock_guard<std::mutex> lk(m_mu);
    return m_fs;
}

void AmModulationAnalyzer::setCarrierTimeConstantMs(double ms)
{
    std::lock_guard<std::mutex> lk(m_mu);
    m_carrierTcMs = std::max(10.0, ms);
}

void AmModulationAnalyzer::setPeakHoldMs(double ms)
{
    std::lock_guard<std::mutex> lk(m_mu);
    m_peakHoldMs = std::max(0.0, ms);
}

void AmModulationAnalyzer::setCarrierThresholds(double presentMin, double lowBelow, double highAbove)
{
    std::lock_guard<std::mutex> lk(m_mu);
    m_presentMin = presentMin;
    m_lowBelow   = lowBelow;
    m_highAbove  = highAbove;
}

void AmModulationAnalyzer::setScopeWindowSeconds(double seconds)
{
    std::lock_guard<std::mutex> lk(m_mu);
    m_scopeSeconds = std::clamp(seconds, 0.05, 5.0);
    rebuildScopeLocked();
}

void AmModulationAnalyzer::rebuildScopeLocked()
{
    // Aim for ~8 kHz scope rate: enough to draw a 5 kHz audio envelope,
    // cheap enough to repaint at 30 fps.
    m_decim = std::max(1, m_fs / 8000);
    const int scopeRate = m_fs / m_decim;
    const std::size_t n = static_cast<std::size_t>(
        std::max(64.0, m_scopeSeconds * scopeRate));
    m_scope.assign(n, 0.0f);
    m_scopeHead   = 0;
    m_scopeFilled = false;
    m_decimCount  = 0;
    m_decimAcc    = 0.0f;
}

void AmModulationAnalyzer::reset()
{
    std::lock_guard<std::mutex> lk(m_mu);
    m_carrier       = 0.0;
    m_carrierSeeded = false;
    m_seedSum       = 0.0;
    m_seedCount     = 0;
    m_winMax = m_winMin = -1.0;
    m_winValid = false;
    m_holdPos = m_holdNeg = 0.0;
    m_holdPosAt = m_holdNegAt = clock::time_point{};
    m_framesSeen = 0;
    rebuildScopeLocked();
}

double AmModulationAnalyzer::modPct(double env) const noexcept
{
    if (m_carrier <= 0.0) {
        return 0.0;
    }
    return (env / m_carrier - 1.0) * 100.0;
}

void AmModulationAnalyzer::pushScopeLocked(float pct)
{
    // Keep the largest-magnitude sample of each decimation group so
    // peaks survive decimation (a plain "every Nth sample" pick would
    // hide the very excursions the operator is looking for).
    if (m_decimCount == 0 || std::fabs(pct) > std::fabs(m_decimAcc)) {
        m_decimAcc = pct;
    }
    if (++m_decimCount >= m_decim) {
        m_scope[m_scopeHead] = m_decimAcc;
        m_scopeHead = (m_scopeHead + 1) % m_scope.size();
        if (m_scopeHead == 0) {
            m_scopeFilled = true;
        }
        m_decimCount = 0;
        m_decimAcc   = 0.0f;
    }
}

void AmModulationAnalyzer::pushIq(const float* interleaved, int frames)
{
    if (interleaved == nullptr || frames <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lk(m_mu);

    // Block statistics first (no per-sample lock churn).
    double sum = 0.0;
    double bmax = 0.0;
    double bmin = 1e300;
    for (int i = 0; i < frames; ++i) {
        const double re = interleaved[2 * i + 0];
        const double im = interleaved[2 * i + 1];
        const double env = std::sqrt(re * re + im * im);
        sum += env;
        bmax = std::max(bmax, env);
        bmin = std::min(bmin, env);
    }
    const double mean = sum / frames;

    // Carrier tracker: exponential average of the envelope.  Seed on the
    // first block after reset so the meters read sensibly within one
    // refresh instead of ramping up over the time constant.
    if (!m_carrierSeeded) {
        m_seedSum   += sum;
        m_seedCount += frames;
        m_carrier    = m_seedSum / static_cast<double>(m_seedCount);
        const long seedTarget = std::max(2048L, static_cast<long>(m_fs / 10));  // ~100 ms
        if (m_seedCount >= seedTarget) {
            m_carrierSeeded = true;
        }
    } else {
        const double tcSamples = m_carrierTcMs * 1e-3 * m_fs;
        const double a = 1.0 - std::exp(-static_cast<double>(frames) / tcSamples);
        m_carrier += (mean - m_carrier) * a;
    }

    if (!m_carrierSeeded) {
        // Reference not trustworthy yet: only feed the scope with the
        // running estimate, no window peaks / hold.
        for (int i = 0; i < frames; ++i) {
            const double re = interleaved[2 * i + 0];
            const double im = interleaved[2 * i + 1];
            pushScopeLocked(m_carrier > m_presentMin
                                ? static_cast<float>(modPct(std::sqrt(re * re + im * im)))
                                : 0.0f);
        }
        m_framesSeen += static_cast<std::uint64_t>(frames);
        return;
    }

    if (!m_winValid) {
        m_winMax = bmax;
        m_winMin = bmin;
        m_winValid = true;
    } else {
        m_winMax = std::max(m_winMax, bmax);
        m_winMin = std::min(m_winMin, bmin);
    }

    // Peak hold, in percent against the current carrier estimate.
    const bool carrierOk = m_carrier > m_presentMin;
    if (carrierOk) {
        const double pos = std::max(0.0, modPct(bmax));
        const double neg = std::max(0.0, -modPct(bmin));
        const auto now = clock::now();
        if (pos >= m_holdPos) { m_holdPos = pos; m_holdPosAt = now; }
        if (neg >= m_holdNeg) { m_holdNeg = neg; m_holdNegAt = now; }
    }

    // Scope trace.
    for (int i = 0; i < frames; ++i) {
        const double re = interleaved[2 * i + 0];
        const double im = interleaved[2 * i + 1];
        const double env = std::sqrt(re * re + im * im);
        pushScopeLocked(carrierOk ? static_cast<float>(modPct(env)) : 0.0f);
    }

    m_framesSeen += static_cast<std::uint64_t>(frames);
}

AmModulationAnalyzer::Snapshot AmModulationAnalyzer::snapshot()
{
    std::lock_guard<std::mutex> lk(m_mu);
    Snapshot s;
    s.framesSeen   = m_framesSeen;
    s.carrierLevel = m_carrier;
    s.carrierDbfs  = (m_carrier > 0.0) ? 20.0 * std::log10(m_carrier) : -120.0;
    s.carrierPresent = m_carrierSeeded && m_carrier > m_presentMin;
    s.carrierLow   = s.carrierPresent && m_carrier < m_lowBelow;
    s.carrierHigh  = s.carrierPresent && m_carrier > m_highAbove;

    if (s.carrierPresent && m_winValid) {
        s.posPeakPct = std::max(0.0, modPct(m_winMax));
        s.negPeakPct = std::max(0.0, -modPct(m_winMin));
    }
    m_winValid = false;

    // Peak hold with decay: hold flat for m_peakHoldMs, then fall at
    // 100 %/s so a stale peak clears in about a second.
    const auto now = clock::now();
    auto decayed = [&](double hold, clock::time_point at) {
        if (hold <= 0.0) return 0.0;
        const double ms = std::chrono::duration<double, std::milli>(now - at).count();
        if (ms <= m_peakHoldMs) return hold;
        return std::max(0.0, hold - (ms - m_peakHoldMs) * 0.1);
    };
    m_holdPos = decayed(m_holdPos, m_holdPosAt);
    m_holdNeg = decayed(m_holdNeg, m_holdNegAt);
    s.posHoldPct = m_holdPos;
    s.negHoldPct = m_holdNeg;

    // Scope, oldest -> newest.
    s.scopeRateHz = m_fs / m_decim;
    if (m_scopeFilled) {
        s.scope.reserve(m_scope.size());
        s.scope.insert(s.scope.end(), m_scope.begin() + static_cast<std::ptrdiff_t>(m_scopeHead), m_scope.end());
        s.scope.insert(s.scope.end(), m_scope.begin(), m_scope.begin() + static_cast<std::ptrdiff_t>(m_scopeHead));
    } else {
        s.scope.assign(m_scope.begin(), m_scope.begin() + static_cast<std::ptrdiff_t>(m_scopeHead));
    }
    return s;
}

} // namespace NereusSDR
