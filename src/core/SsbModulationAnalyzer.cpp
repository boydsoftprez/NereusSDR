// =================================================================
// src/core/SsbModulationAnalyzer.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original file. See the header for the design.
//
// Modification history (NereusSDR):
//   2026-11-04 — Created for the SSB Mod Monitor. Brian Sutton (W4KZO),
//                AI-assisted via xAI Grok.
//   2026-11-05 - Corrections for review findings.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
#include "core/SsbModulationAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// The one definition of crest: peak over RMS of the envelope, in dB.
double crestDbFromPeakAndRms(double peak, double rms) noexcept
{
    return 20.0 * std::log10(peak / rms);
}

} // namespace

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
    m_scopeCapacity.store(n, std::memory_order_relaxed);
    m_scopeHead = 0;
    m_scopeFilled = false;
    m_decimCount = 0;
    m_decimAcc = 0.0f;
}

void SsbModulationAnalyzer::reset()
{
    std::lock_guard<std::mutex> lock(m_mu);
    forgetInputLocked();
    m_holdPeak = 0.0;
    m_holdAt = clock::time_point{};
    m_framesSeen = 0;
}

void SsbModulationAnalyzer::forgetInputLocked()
{
    m_avg = 0.0;
    m_avgSeeded = false;
    m_seedSum = 0.0;
    m_seedCount = 0;
    m_winMax = 0.0;
    m_winValid = false;
    m_inputLive = false;
    m_crest.fill(CrestSlot{});
    rebuildScopeLocked();
}

void SsbModulationAnalyzer::advanceClockForTest(std::chrono::milliseconds step)
{
    std::lock_guard<std::mutex> lock(m_mu);
    m_nowForTest = m_nowForTest.value_or(clock::now()) + step;
}

SsbModulationAnalyzer::clock::time_point SsbModulationAnalyzer::nowLocked() const
{
    return m_nowForTest ? *m_nowForTest : clock::now();
}

double SsbModulationAnalyzer::decayedHoldPctLocked(clock::time_point now) const
{
    if (m_holdPeak <= 0.0) {
        return 0.0;
    }
    const double ms = std::chrono::duration<double, std::milli>(now - m_holdAt).count();
    if (ms <= m_peakHoldMs) {
        return m_holdPeak;
    }
    // 100 percent per second after the hold, from the original peak.
    return std::max(0.0, m_holdPeak - (ms - m_peakHoldMs) * 0.1);
}

namespace {
double crestPauseGate()
{
    static const double gate = std::pow(10.0, -SsbModulationAnalyzer::kCrestPauseGateDb / 20.0);
    return gate;
}
} // namespace

void SsbModulationAnalyzer::addCrestBlockLocked(clock::time_point now, double peak, double energy,
                                                int frames)
{
    // A pause is left out, so it cannot inflate the ratio.
    if (peak <= m_presentMin) {
        return;
    }
    const std::int64_t id = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now.time_since_epoch()).count() / kCrestSlotMs;
    CrestSlot& slot = m_crest[static_cast<std::size_t>(id % kCrestSlots)];
    if (slot.id != id) {
        slot = CrestSlot{};
        slot.id = id;
    }
    // Within one slot as well, so a pause's edge does not count.
    if (slot.frames > 0 && peak < slot.peak * crestPauseGate()) {
        return;
    }
    if (slot.frames > 0 && slot.peak < peak * crestPauseGate()) {
        slot = CrestSlot{};
        slot.id = id;
    }
    slot.peak = std::max(slot.peak, peak);
    slot.energy += energy;
    slot.frames += frames;
}

std::optional<double> SsbModulationAnalyzer::crestDbLocked(clock::time_point now) const
{
    const std::int64_t newest = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    now.time_since_epoch()).count() / kCrestSlotMs;
    const auto inWindow = [newest](const CrestSlot& slot) {
        return slot.frames > 0 && slot.id > newest - kCrestSlots && slot.id <= newest;
    };
    double peak = 0.0;
    for (const CrestSlot& slot : m_crest) {
        if (inWindow(slot)) {
            peak = std::max(peak, slot.peak);
        }
    }
    double energy = 0.0;
    std::int64_t frames = 0;
    for (const CrestSlot& slot : m_crest) {
        // A pause on a real radio is noise, not zero: far under the window's
        // peak, it is left out so it cannot inflate the ratio.
        if (inWindow(slot) && slot.peak >= peak * crestPauseGate()) {
            energy += slot.energy;
            frames += slot.frames;
        }
    }
    if (frames == 0) {
        return std::nullopt;
    }
    return crestDbFromPeakAndRms(peak, std::sqrt(energy / static_cast<double>(frames)));
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
    double energy = 0.0;
    double blockMax = 0.0;
    for (int i = 0; i < frames; ++i) {
        const double re = interleaved[2 * i];
        const double im = interleaved[2 * i + 1];
        const double power = re * re + im * im;
        const double env = std::sqrt(power);
        sum += env;
        energy += power;
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

    const auto now = nowLocked();
    m_lastPushAt = now;
    m_inputLive = true;
    addCrestBlockLocked(now, blockMax, energy, frames);

    // Against the hold as it stands now. A peak lower than the one that
    // started the hold still takes over once the hold has decayed to it.
    const double blockPct = toPct(blockMax);
    if (blockPct >= decayedHoldPctLocked(now)) {
        m_holdPeak = blockPct;
        m_holdAt = now;
    }

    m_framesSeen += static_cast<std::uint64_t>(frames);
}

SsbModulationAnalyzer::Snapshot SsbModulationAnalyzer::snapshot()
{
    // Allocated before the lock the transmit worker and the connection thread
    // take. A sample rate change landing here is the one time it grows.
    Snapshot snap;
    snap.scope.reserve(m_scopeCapacity.load(std::memory_order_relaxed));
    std::lock_guard<std::mutex> lock(m_mu);
    snap.framesSeen = m_framesSeen;

    const auto now = nowLocked();
    // Samples stopped: nothing here is live any more. The hold is not part
    // of this and finishes its decay below.
    if (m_inputLive
        && std::chrono::duration<double, std::milli>(now - m_lastPushAt).count() > kInputStaleMs) {
        forgetInputLocked();
    }

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

    const double holdPct = decayedHoldPctLocked(now);
    if (holdPct <= 0.0) {
        m_holdPeak = 0.0;
    }
    snap.peakHoldPct = holdPct;
    snap.peakHoldDbfs = holdPct > 0.0 ? toDbfs(holdPct / 100.0 * kFullScale) : -120.0;
    snap.crestDb = crestDbLocked(now);

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
