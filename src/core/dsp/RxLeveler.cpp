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
#include <array>
#include <cmath>
#include <numbers>

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

// Butterworth section Qs, 8th order (four sections).
constexpr std::array<double, 4> kButterworth8Q{0.5097955791041592, 0.6013448869350453,
                                              0.8999762231364156, 2.5629154477415055};
// Measuring band corners are kept this far inside Nyquist (share of it).
constexpr double kMaxCornerShare = 0.9;

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
    w |= static_cast<std::uint64_t>(resetCount & kResetCountMask) << 3;
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
    p.resetCount = static_cast<int>((w >> 3) & static_cast<std::uint64_t>(kResetCountMask));
    p.targetDb = static_cast<int>((w >> 8) & 0xFFu) + RxLeveler::kTargetDbMin;
    p.maxBoostDb = static_cast<int>((w >> 16) & 0xFFu) + RxLeveler::kMaxBoostDbMin;
    p.maxCutDb = static_cast<int>((w >> 24) & 0xFFu) + RxLeveler::kMaxCutDbMin;
    p.attackMs = static_cast<int>((w >> 32) & 0xFFFFu) + RxLeveler::kAttackMsMin;
    p.releaseMs = static_cast<int>((w >> 48) & 0xFFFFu) + RxLeveler::kReleaseMsMin;
    return p;
}

std::uint64_t RxLevelerParams::packBand() const noexcept
{
    const auto hz = [](int v) { return static_cast<std::uint64_t>(std::clamp(v, 0, 0xFFFF)); };
    return hz(measureLowHz) | (hz(measureHighHz) << 16);
}

void RxLevelerParams::unpackBand(std::uint64_t w, RxLevelerParams& into) noexcept
{
    into.measureLowHz = static_cast<int>(w & 0xFFFFu);
    into.measureHighHz = static_cast<int>((w >> 16) & 0xFFFFu);
}

void RxLeveler::measureBand(DSPMode mode, int filterLowHz, int filterHighHz, int& lowHz,
                            int& highHz)
{
    lowHz = 0;
    highHz = 0;
    if (filterHighHz <= filterLowHz) {
        return;
    }
    const double edge = RxPassbandVoice::kEdgeShare
        * static_cast<double>(filterHighHz - filterLowHz);
    double lo = 0.0;
    double hi = 0.0;
    switch (mode) {
    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::DSB:
        // Both sidebands. A filter about the carrier folds, the shorter
        // side's top is the audio's top; one lying to one side is read as
        // that sideband.
        if (filterLowHz >= 0) {
            lo = filterLowHz;
            hi = filterHighHz - edge;
        } else if (filterHighHz <= 0) {
            lo = -filterHighHz;
            hi = -filterLowHz - edge;
        } else {
            lo = 0.0;
            hi = std::min(-filterLowHz, filterHighHz) - edge;
        }
        break;
    case DSPMode::LSB:
    case DSPMode::CWL:
    case DSPMode::DIGL:
    case DSPMode::RADE_L:
    case DSPMode::RADE2_L:
        // Below the carrier, mirrored: the outer edge is the filter's low
        // one. A part above the carrier folds onto the bottom of the audio.
        lo = std::max(0, -filterHighHz);
        hi = -filterLowHz - edge;
        break;
    default:
        // Above the carrier (USB and the other upper modes): the outer edge
        // is the high one. A part below the carrier folds onto the bottom.
        lo = std::max(0, filterLowHz);
        hi = filterHighHz - edge;
        break;
    }
    if (hi <= lo) {
        return;
    }
    lowHz = static_cast<int>(std::lround(lo));
    highHz = static_cast<int>(std::lround(hi));
}

void RxLeveler::setMeasureBand(int lowHz, int highHz, double sampleRate)
{
    if (lowHz == m_measureLowHz && highHz == m_measureHighHz && sampleRate == m_measureRate) {
        return;
    }
    m_measureLowHz = lowHz;
    m_measureHighHz = highHz;
    m_measureRate = sampleRate;
    const double maxHz = kMaxCornerShare * sampleRate / 2.0;
    const bool band = highHz > lowHz && highHz > 0;
    // The top corner sits below the band's top (kTopCornerRatio) so the
    // reading is well down where the edge begins: a voice has little power
    // up there, a neighbor's low end has the most.
    const double topHz = static_cast<double>(highHz) / kTopCornerRatio;
    m_measureHpOn = band && lowHz > 0 && lowHz < maxHz;
    m_measureLpOn = band && topHz < maxHz && topHz > lowHz;
    // Robert Bristow-Johnson's audio EQ cookbook biquads (low- and
    // high-pass), one per Butterworth section Q.
    const auto design = [sampleRate](Section& sec, double hz, double q, bool high) {
        const double w0 = 2.0 * std::numbers::pi * hz / sampleRate;
        const double c = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        if (high) {
            sec.b0 = (1.0 + c) / 2.0 / a0;
            sec.b1 = -(1.0 + c) / a0;
        } else {
            sec.b0 = (1.0 - c) / 2.0 / a0;
            sec.b1 = (1.0 - c) / a0;
        }
        sec.b2 = sec.b0;
        sec.a1 = -2.0 * c / a0;
        sec.a2 = (1.0 - alpha) / a0;
        sec.z1 = 0.0;
        sec.z2 = 0.0;
    };
    for (size_t side = 0; side < m_measureHp.size(); ++side) {
        for (size_t i = 0; i < kButterworth8Q.size(); ++i) {
            if (m_measureHpOn) {
                design(m_measureHp[side][i], lowHz, kButterworth8Q[i], true);
            }
            if (m_measureLpOn) {
                design(m_measureLp[side][i], topHz, kButterworth8Q[i], false);
            }
        }
    }
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
    m_riseHoldLeft = 0.0;
    m_fallDb = 0.0;
    m_measureLowHz = -1;     // the filters are designed afresh
    m_measureHighHz = -1;
    m_measureRate = 0.0;
    m_hissPow = -1.0;
    m_peakRefDb = -200.0;
    m_active = false;
    m_letGo = false;
    m_hissLearnLeft = 0.0;
    m_publishedGain.store(0.0f, std::memory_order_relaxed);
    m_publishedLimiter.store(0.0f, std::memory_order_relaxed);
}

void RxLeveler::startOver()
{
    // The gain and the limiter stay where they are: process() takes them
    // back to unity at their own rates (m_letGo).
    m_loudPow = -1.0;
    m_loudSec = 0.0;
    m_quietSec = 0.0;
    m_needDb = 0.0;
    m_riseHoldLeft = 0.0;
    m_fallDb = 0.0;
    m_hissPow = -1.0;
    m_hissLearnLeft = 0.0;
    m_peakRefDb = -200.0;
    m_shortPow = 0.0;       // the old frequency's level, not this one's
    m_letGo = true;
}

void RxLeveler::resetForTransmit()
{
    m_gainDb = 0.0;
    m_limiter = 1.0;
    m_loudPow = -1.0;
    m_loudSec = 0.0;
    m_quietSec = 0.0;
    m_needDb = 0.0;
    m_riseHoldLeft = 0.0;
    m_fallDb = 0.0;
    m_shortPow = 0.0;       // the over's level is not the band's
    m_letGo = false;
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
    const double fullPower = sum / static_cast<double>(samples);

    // The loudness is read without the filter's outer edge: a neighbor whose
    // edge pokes into the passband is heard, but does not move the level.
    setMeasureBand(params.measureLowHz, params.measureHighHz, sampleRate);
    double power = fullPower;
    if (m_measureHpOn || m_measureLpOn) {
        // Each side through its own filters, and the power of both averaged
        // as fullPower averages them: a panned or BIN receiver (left and
        // right apart, or 90 degrees apart) reads what the whole block reads.
        double msum = 0.0;
        for (int n = 0; n < frames; ++n) {
            for (size_t side = 0; side < 2; ++side) {
                double y = static_cast<double>(x[2 * n + static_cast<int>(side)]);
                if (m_measureHpOn) {
                    for (Section& sec : m_measureHp[side]) {
                        y = sec.run(y);
                    }
                }
                if (m_measureLpOn) {
                    for (Section& sec : m_measureLp[side]) {
                        y = sec.run(y);
                    }
                }
                msum += y * y;
            }
        }
        power = msum / static_cast<double>(samples) * std::pow(10.0, kMeasureOffsetDb / 10.0);
        if (!std::isfinite(power)) {
            power = fullPower;
            m_measureLowHz = -1;    // start the filters over next block
        }
    }
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
    // The hiss. The first reading sets it, station or not, and a quieter
    // one lowers it quickly, station or not: the hiss is no louder than the
    // quietest moment. It rises only while no station is there, slowly, and
    // skips a level far above it: the first syllables of an over arrive
    // before the S-meter has said a station is there.
    if (!present) {
        m_hissLearnLeft = 0.0;      // heard with nobody there: learned
    }
    if (shortDb > kSilenceDbfs) {
        if (m_hissPow < 0.0) {
            m_hissPow = m_shortPow;
            // First read with a station there: its quietest moments over
            // kHissLearnSec are the hiss, before any block is speech.
            m_hissLearnLeft = present ? kHissLearnSec : 0.0;
        } else if (present && m_hissLearnLeft > 0.0) {
            m_hissPow = std::min(m_hissPow, m_shortPow);
        } else if (present) {
            // A station: a louder block is its voice and a quieter one may
            // be the AGC pulling the hiss down under it; neither is the
            // band's hiss.
        } else if (m_shortPow < m_hissPow) {
            m_hissPow += step(dt, kHissFallSec) * (m_shortPow - m_hissPow);
        } else if (shortDb < toDb(m_hissPow) + kHissSkipDb) {
            m_hissPow += step(dt, kHissRiseSec) * (m_shortPow - m_hissPow);
        } else {
            m_hissPow *= std::pow(10.0, kHissCreepDbPerSec * dt / 10.0);
        }
    }
    const double hiss = m_hissPow > 0.0 ? m_hissPow : 0.0;
    // Not until some hiss is known: over an unmeasured hiss, the hiss
    // itself would pass the peaks gate alone and be lifted as a voice.
    if (present && m_hissLearnLeft > 0.0) {
        m_hissLearnLeft = std::max(0.0, m_hissLearnLeft - dt);
    }
    const bool hissKnown = hiss > 0.0 && m_hissLearnLeft <= 0.0;
    const bool speech = present && shortDb > kSilenceDbfs
        && shortDb > m_peakRefDb - kSpeechGateDb
        && hissKnown && shortDb > toDb(hiss) + kOverHissDb
        // A voice's first reading is its own: the 50 ms level is still up
        // in the tail of a word the station check came too late for, and
        // the bare hiss of that block is no voice.
        && (m_loudPow >= 0.0 || toDb(power) > toDb(hiss) + kOverHissDb);

    m_quietSec = speech ? 0.0 : m_quietSec + dt;

    // The loudness of this station's voice, hiss taken out. Forgotten
    // when the station goes, so the next one is judged on its own.
    if (!present) {
        m_loudPow = -1.0;
        m_loudSec = 0.0;
    } else if (speech) {
        const double voice = std::max(power - hiss, power * 0.1);
        if (m_loudPow < 0.0) {
            m_loudPow = voice;
            m_loudSec = dt;
            m_letGo = false;    // a voice again: the gain follows it
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
        // No station: back to unity. A station not yet heard: hold, unless
        // the leveler is starting over (a retune), when it goes to unity.
        want = present && !m_letGo ? m_gainDb : 0.0;
    } else if (m_quietSec >= kPauseReleaseSec) {
        want = 0.0;                         // a long pause, or the over ended
    } else if (m_quietSec < kWordGapSec) {
        want = m_needDb;                    // speech, or a gap between words
    } else {
        want = std::min(m_needDb, m_gainDb); // a pause: no rise on the hiss
    }
    // After a turn-down for a louder voice, no rise until the hold runs
    // out: a neighbor keying on and off does not ride the level.
    if (!present) {
        m_riseHoldLeft = 0.0;
        m_fallDb = 0.0;
    } else if (m_riseHoldLeft > 0.0) {
        m_riseHoldLeft = std::max(0.0, m_riseHoldLeft - dt);
        want = std::min(want, m_gainDb);
    }
    // A first soft syllable is not the station: no rise until enough of
    // its voice is heard. Down, and back up to unity, go as ever.
    if (present && m_loudSec < kRiseSpeechSec) {
        want = std::min(want, std::max(m_gainDb, 0.0));
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
    // Read in the measuring band too, so a neighbor keying at the edge does
    // not snap the level down; the limiter still keeps every peak under
    // kCeiling.
    if (gainDb > 0.0 && present) {
        const double room = target + kBoostHeadroomDb - toDb(m_shortPow);
        gainDb = std::min(gainDb, std::max(room, 0.0));
    }
    // A turn-down while a voice is heard (not the let-go in a pause)
    // starts the hold once it adds up to kTurnDownDb.
    if (present && speech && gainDb < m_gainDb) {
        m_fallDb += m_gainDb - gainDb;
        if (m_fallDb >= kTurnDownDb) {
            m_riseHoldLeft = kRiseHoldSec;
            m_fallDb = 0.0;
        }
    } else if (gainDb > m_gainDb) {
        m_fallDb = 0.0;
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
