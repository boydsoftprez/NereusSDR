// no-port-check: NereusSDR-original receive leveler; no upstream source.
// =================================================================
// src/core/dsp/RxLeveler.cpp  (NereusSDR)
// =================================================================
//
// Receive leveler. See RxLeveler.h for what it does, where it runs and
// the modification history.
// =================================================================

#include "RxLeveler.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

constexpr double kPowerFloor = 1e-20;
// The short power reading the boost headroom check uses (s).
constexpr double kShortPowerSec = 0.05;

double toDb(double power)
{
    return 10.0 * std::log10(std::max(power, kPowerFloor));
}

double dbToGain(double dB)
{
    return std::pow(10.0, dB / 20.0);
}

// One-pole step for a block of `dt` seconds toward a time constant `tau`.
double step(double dt, double tau)
{
    if (tau <= 0.0) {
        return 1.0;
    }
    return 1.0 - std::exp(-dt / tau);
}

std::uint64_t field(int value, int lo, int hi)
{
    return static_cast<std::uint64_t>(std::clamp(value, lo, hi) - lo);
}

}  // namespace

std::uint64_t RxLevelerParams::pack() const noexcept
{
    std::uint64_t w = 0;
    w |= on ? 1u : 0u;
    w |= stationPresent ? 2u : 0u;
    w |= nearOverload ? 4u : 0u;
    w |= field(targetDb, RxLeveler::kTargetDbMin, RxLeveler::kTargetDbMax) << 8;
    w |= field(maxBoostDb, RxLeveler::kMaxBoostDbMin, RxLeveler::kMaxBoostDbMax) << 16;
    w |= field(maxCutDb, RxLeveler::kMaxCutDbMin, RxLeveler::kMaxCutDbMax) << 24;
    w |= field(attackMs, RxLeveler::kAttackMsMin, RxLeveler::kAttackMsMax) << 32;
    w |= field(releaseMs, RxLeveler::kReleaseMsMin, RxLeveler::kReleaseMsMax) << 48;
    return w;
}

RxLevelerParams RxLevelerParams::unpack(std::uint64_t w) noexcept
{
    RxLevelerParams p;
    p.on = (w & 1u) != 0;
    p.stationPresent = (w & 2u) != 0;
    p.nearOverload = (w & 4u) != 0;
    p.targetDb = static_cast<int>((w >> 8) & 0xFFu) + RxLeveler::kTargetDbMin;
    p.maxBoostDb = static_cast<int>((w >> 16) & 0xFFu) + RxLeveler::kMaxBoostDbMin;
    p.maxCutDb = static_cast<int>((w >> 24) & 0xFFu) + RxLeveler::kMaxCutDbMin;
    p.attackMs = static_cast<int>((w >> 32) & 0xFFFFu) + RxLeveler::kAttackMsMin;
    p.releaseMs = static_cast<int>((w >> 48) & 0xFFFFu) + RxLeveler::kReleaseMsMin;
    return p;
}

bool RxLeveler::modeAllows(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::USB:
    case DSPMode::DSB:
    case DSPMode::AM:
    case DSPMode::SAM:
        return true;
    default:
        return false;
    }
}

void RxLeveler::reset()
{
    m_gainDb = 0.0;
    m_limiter = 1.0;
    m_loudPow = -1.0;
    m_loudSec = 0.0;
    m_quietSec = 0.0;
    m_needDb = 0.0;
    m_shortPow = 0.0;
    m_hissPow = -1.0;
    m_peakRefDb = -200.0;
    m_active = false;
    m_publishedGain.store(0.0f, std::memory_order_relaxed);
    m_publishedLimiter.store(0.0f, std::memory_order_relaxed);
}

double RxLeveler::gainDb() const
{
    return static_cast<double>(m_publishedGain.load(std::memory_order_relaxed));
}

double RxLeveler::limiterDb() const
{
    return static_cast<double>(m_publishedLimiter.load(std::memory_order_relaxed));
}

double RxLeveler::loudnessDb() const
{
    return m_loudPow < 0.0 ? kSilenceDbfs : toDb(m_loudPow);
}

void RxLeveler::process(float* x, int frames, double sampleRate,
                        const RxLevelerParams& params)
{
    if (!params.on) {
        if (m_active) {
            reset();
        }
        return;
    }
    if (x == nullptr || frames <= 0 || sampleRate <= 0.0) {
        return;
    }
    m_active = true;
    const double dt = static_cast<double>(frames) / sampleRate;
    const int samples = frames * 2;

    // ── 1. Read the block before touching it: its power and loudest point.
    double sum = 0.0;
    double peak = 0.0;
    for (int i = 0; i < samples; ++i) {
        float s = x[i];
        if (!std::isfinite(s)) {
            s = 0.0f;
            x[i] = 0.0f;
        }
        const double v = static_cast<double>(s);
        sum += v * v;
        peak = std::max(peak, std::abs(v));
    }
    const double power = sum / static_cast<double>(samples);
    if (m_shortPow <= 0.0) {
        m_shortPow = power;     // the first block sets it, not a climb from zero
    } else {
        m_shortPow += step(dt, kShortPowerSec) * (power - m_shortPow);
    }

    // ── 2. Is this block speech, or a gap between words?
    // Judged on the 50 ms level, not the block's own: a short block of
    // plain hiss can read a few dB high, and counting it as speech would
    // ride the gain up between overs. The same level whatever the block
    // size the sound card asks for.
    const double shortDb = toDb(m_shortPow);
    if (shortDb > m_peakRefDb) {
        m_peakRefDb = shortDb;
    } else {
        m_peakRefDb = std::max(shortDb, m_peakRefDb - kPeakFallDbPerSec * dt);
    }
    const bool present = params.stationPresent;
    // The hiss between overs, learned while no station is there. It
    // follows a quieter level quickly and a louder one slowly, and skips a
    // level far above it: the first syllables of an over arrive before
    // the S-meter has said a station is there.
    if (!present && shortDb > kSilenceDbfs) {
        if (m_hissPow < 0.0) {
            m_hissPow = m_shortPow;
        } else if (m_shortPow < m_hissPow) {
            m_hissPow += step(dt, kHissFallSec) * (m_shortPow - m_hissPow);
        } else if (shortDb < toDb(m_hissPow) + kHissSkipDb) {
            m_hissPow += step(dt, kHissRiseSec) * (m_shortPow - m_hissPow);
        }
    }
    const double hiss = m_hissPow > 0.0 ? m_hissPow : 0.0;
    const bool speech = present && shortDb > kSilenceDbfs
        && shortDb > m_peakRefDb - kSpeechGateDb
        && (hiss <= 0.0 || shortDb > toDb(hiss) + kOverHissDb);

    m_quietSec = speech ? 0.0 : m_quietSec + dt;

    // The loudness of this station's voice, hiss taken out. Forgotten
    // when the station goes, so the next one is judged on its own.
    if (!present) {
        m_loudPow = -1.0;
    } else if (speech) {
        const double voice = std::max(power - hiss, power * 0.1);
        if (m_loudPow < 0.0) {
            m_loudPow = voice;
            m_loudSec = dt;
        } else {
            // A plain average of what has been heard until a time constant
            // has passed, so the first syllable does not set the reading.
            m_loudSec += dt;
            const double a = std::max(step(dt, kLoudnessSec), dt / m_loudSec);
            m_loudPow += a * (voice - m_loudPow);
        }
    }

    // ── 3. The gain this block moves toward.
    const double target = static_cast<double>(
        std::clamp(params.targetDb, kTargetDbMin, kTargetDbMax));
    const double maxCut = static_cast<double>(
        std::clamp(params.maxCutDb, kMaxCutDbMin, kMaxCutDbMax));
    const double maxBoost = params.nearOverload
        ? 0.0
        : static_cast<double>(std::clamp(params.maxBoostDb, kMaxBoostDbMin, kMaxBoostDbMax));
    // What this station needs, refreshed on each speech block.
    if (speech && m_loudPow > 0.0) {
        m_needDb = target - toDb(m_loudPow);
    }
    double want = 0.0;
    if (!present || m_loudPow < 0.0) {
        want = present ? m_gainDb : 0.0;    // no station: back to unity
    } else if (m_quietSec >= kPauseReleaseSec) {
        want = 0.0;                         // a long pause, or the over ended
    } else if (m_quietSec < kWordGapSec) {
        want = m_needDb;                    // speech, or a gap between words
    } else {
        want = std::min(m_needDb, m_gainDb); // a pause: no rise on the hiss
    }
    want = std::clamp(want, -maxCut, maxBoost);

    const double attackSec = static_cast<double>(
        std::clamp(params.attackMs, kAttackMsMin, kAttackMsMax)) / 1000.0;
    const double releaseSec = static_cast<double>(
        std::clamp(params.releaseMs, kReleaseMsMin, kReleaseMsMax)) / 1000.0;
    double gainDb = m_gainDb
        + step(dt, want > m_gainDb ? attackSec : releaseSec) * (want - m_gainDb);
    // A limit lowered while the gain was outside it takes effect at once.
    gainDb = std::clamp(gainDb, -maxCut, maxBoost);
    // A loud voice arriving while the gain is still up for a quiet one
    // loses the boost at once, not at the release rate.
    if (gainDb > 0.0 && present) {
        const double room = target + kBoostHeadroomDb - toDb(m_shortPow);
        gainDb = std::min(gainDb, std::max(room, 0.0));
    }

    // ── 4. Ramp the gain across the block, under a limiter that has
    // already seen the block's peak.
    const double g0 = dbToGain(m_gainDb);
    const double g1 = dbToGain(gainDb);
    const double gStep = (g1 - g0) / static_cast<double>(frames);
    const double gMax = std::max(g0, g1);

    // The ceiling for this block: kCeiling, or the block's own peak if it
    // came in higher. The limiter holds back what the leveler adds; it
    // never pulls a peak below where it arrived, so at or below unity it
    // does nothing.
    const double ceiling = std::max(kCeiling, peak);
    // The limiter's value at the end of the block, and the first frame
    // that would pass the ceiling at the limiter's present value.
    double limEnd = m_limiter;
    int firstOver = -1;
    if (peak * gMax * m_limiter > ceiling) {
        for (int n = 0; n < frames && firstOver < 0; ++n) {
            const double g = g0 + gStep * static_cast<double>(n + 1);
            const double a = std::max(std::abs(static_cast<double>(x[2 * n])),
                                      std::abs(static_cast<double>(x[2 * n + 1])));
            if (a * g * m_limiter > ceiling) {
                firstOver = n;
            }
        }
    }
    if (firstOver >= 0) {
        limEnd = ceiling / (peak * gMax);
    } else {
        limEnd = std::min(1.0, m_limiter
            + step(dt, kLimiterReleaseMs / 1000.0) * (1.0 - m_limiter));
        if (peak > 0.0) {
            limEnd = std::min(limEnd, ceiling / (peak * gMax));
        }
        limEnd = std::max(limEnd, m_limiter);
    }

    const int attackFrames = std::max(
        1, static_cast<int>(std::lround(kLimiterAttackMs / 1000.0 * sampleRate)));
    const int rampStart = firstOver >= 0 ? std::max(0, firstOver - attackFrames + 1) : 0;
    const int rampEnd = firstOver >= 0 ? firstOver : frames - 1;
    const double lim0 = m_limiter;
    for (int n = 0; n < frames; ++n) {
        double lim = lim0;
        if (n >= rampEnd) {
            lim = limEnd;
        } else if (n >= rampStart) {
            const double t = static_cast<double>(n - rampStart + 1)
                / static_cast<double>(rampEnd - rampStart + 1);
            lim = lim0 + (limEnd - lim0) * t;
        }
        const double g = (g0 + gStep * static_cast<double>(n + 1)) * lim;
        x[2 * n] = static_cast<float>(static_cast<double>(x[2 * n]) * g);
        x[2 * n + 1] = static_cast<float>(static_cast<double>(x[2 * n + 1]) * g);
    }

    m_gainDb = gainDb;
    m_limiter = limEnd;
    m_publishedGain.store(static_cast<float>(m_gainDb), std::memory_order_relaxed);
    m_publishedLimiter.store(static_cast<float>(20.0 * std::log10(m_limiter)),
                             std::memory_order_relaxed);
}

}  // namespace NereusSDR
