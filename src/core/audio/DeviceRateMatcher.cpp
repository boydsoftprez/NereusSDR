// =================================================================
// src/core/audio/DeviceRateMatcher.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis (WDSP) source:
//   Project Files/Source/wdsp/rmatch.c, original licence from Thetis
//   source is included below
//   Project Files/Source/wdsp/varsamp.c, original licence from Thetis
//   source is included below
//
// The clock matcher between the DSP side and one audio device: WDSP's
// rmatch (mav, aamav, calc_rmatch, control, blend, upslew, xrmatchIN,
// dslew, xrmatchOUT and create_rmatchV's constants) split into a writer
// and a lock-free reader.  rmatch serialises both sides with cs_ring and
// cs_var; here the writer owns the variable resampler, the control and the
// write index, the reader owns the read index and the dry-run slew, and
// they share only MatcherRing's atomics.  Differences from rmatch, each
// marked where it happens:
//   * the writer replays the reads since its last write into control(),
//     with the deviation it measures, instead of each read calling it;
//   * an overrun publishes skipTo and the reader makes the jump and the
//     blend, so the writer never moves the read index;
//   * a dry run's slew tail is played by the reader from its own state
//     instead of being written back into the ring;
//   * the ring size steps through 2, 3, 5, 10, 20 and 40 ms (automatic or
//     fixed), and a dry run is followed by silence up to the new target.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08: Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted transformation via Anthropic
//               Claude Code.  Native audio plan Task 2 (R-AUD-15, D2, D7,
//               D34).
//   2026-10-09: A flush also clears the resampler's history
//               (flush_varsamp, as reset_rmatch starts from a fresh
//               varsamp), so nothing of the earlier audio plays after it.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               Native audio plan Task 6 (R-AUD-15).
//   2026-10-09: A restart starts the automatic sizing fresh (the largest
//               write gap is measured again from the restart), and the
//               fade-in after a dry run is armed from the dry-run count,
//               after its padding, never from the reader's flag alone.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               Native audio plan early-review fix wave (R-AUD-15).
//   2026-10-09: Tests only: a ratio forced to 1.0 at equal rates copies
//               the block, for the shared-memory ring's exact order check
//               (V-SW-6).  J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.  Native audio plan Task 13 (R-AUD-17).
//   2026-10-09: resamplerDelayFramesFor(), the same varsamp delay for any
//               rates, for the PC mic's window side.  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.  Native audio plan
//               Task 13 (R-AUD-18).
//   2026-10-10: A writer of whole packets (setWritePacketFrames; remote
//               playback's 1920 and 192 frames): the size is never below a
//               step whose ring holds a packet, the automatic size starts
//               at the callback plus a packet, the silence after a
//               restart, a flush or a dry run stops half a packet short of
//               the target, and stats() carries the frames queued now and
//               the fill a packet's write may reach.  A packet writer's
//               control (packetControl) steers by the fill a packet meets
//               when it did not wait for room, counts its feed-forward over
//               whole spans between such packets, and starts again after a
//               dry run.  The block writer is as it was.  Bench regression
//               (R-AUD-15).  J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================
//
// --- From rmatch.c ---
/*  rmatch.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017, 2018, 2022 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/
//
// --- From varsamp.c ---
/*  varsamp.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/

#include "core/audio/DeviceRateMatcher.h"

#include "core/LogCategories.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

#ifdef HAVE_WDSP
extern "C" {
// Exact declarations from third_party/wdsp/src/varsamp.h:61-68 (WDSP 2.10),
// flush_varsamp from varsamp.h:66.
// The struct stays opaque here, as RemoteAudioRateMatcher.cpp keeps WDSP's
// private types out of the C++ side.
typedef struct _varsamp varsamp, *VARSAMP;
VARSAMP create_varsamp(int run, int size, double* in, double* out,
                       int in_rate, int out_rate, double fc, double fc_low, int R,
                       double gain, double var, int varmode);
void destroy_varsamp(VARSAMP a);
int xvarsamp(VARSAMP a, double var);
void flush_varsamp(VARSAMP a);
}
#endif

namespace NereusSDR {

namespace {

// From Thetis Project Files/Source/wdsp/rmatch.c:501-527 [v2.10.3.15 @3759d09]
// create_rmatchV's arguments to create_rmatch.
constexpr double kFcHigh = 0.0;                 // fc_high (0.0 -> automatic)
constexpr double kFcLow = -1.0;                 // fc_low  (-1.0 -> no low cutoff)
constexpr double kGain = 1.0;                   // gain
constexpr double kStartupDelaySeconds = 3.0;    // startup delay (seconds)
constexpr int kCoefficientDensity = 1024;       // R, coefficient density
constexpr double kInitialVar = 1.0;             // initial variable ratio
constexpr int kFfMavMin = 4096;                 // feed-forward moving average min size
constexpr int kFfMavMax = 262144;               // feed-forward moving average max size - POWER OF TWO!
constexpr double kFfAlpha = 0.01;               // feed-forward exponential smoothing
constexpr int kPropRingMin = 4096;              // proportional feedback min moving av ringsize
constexpr int kPropRingMax = 16384;             // proportional feedback max moving av ringsize - POWER OF TWO!
constexpr double kPropGain = 4.0e-06;           // proportional feedback gain
constexpr int kVarMode = 1;                     // linearly interpolate cvar by sample
constexpr double kSlewSeconds = 0.003;          // slew time (seconds)

// From Thetis Project Files/Source/wdsp/rmatch.c:134 [v2.10.3.15 @3759d09]
// max_ring_insize = (int)(1.0 + (double)a->insize * (1.05 * a->nom_ratio));
constexpr double kMaxRatioMargin = 1.05;
// From Thetis Project Files/Source/wdsp/rmatch.c:147 [v2.10.3.15 @3759d09]
// a->pr_gain = a->prop_gain * 48000.0 / (double)a->nom_outrate;
constexpr double kPrGainReferenceRate = 48000.0;
// From Thetis Project Files/Source/wdsp/rmatch.c:270-271 [v2.10.3.15 @3759d09]
constexpr double kVarMax = 1.04;
constexpr double kVarMin = 0.96;
// From Thetis Project Files/Source/wdsp/varsamp.c:60 [v2.10.3.15 @3759d09]
// a->rsize = (int)(140.0 * norm_rate / min_rate);
constexpr double kVarsampTapsAtUnity = 140.0;

// NereusSDR limits on a configuration.  Outside them the matcher is
// invalid and the caller fails the open.
constexpr int kMinRate = 8000;
constexpr int kMaxRate = 768000;
constexpr int kMaxWriteBlockFrames = 8192;
constexpr int kMaxCallbackFrames = 65536;
constexpr int kChannels = 2;

// From Thetis Project Files/Source/wdsp/rmatch.c:153-162 [v2.10.3.15 @3759d09]
// The raised-cosine slew table both sides use.
std::vector<double> makeSlewTable(int ntslew)
{
    std::vector<double> cslew(static_cast<std::size_t>(ntslew) + 1, 0.0);
    double theta, dtheta;
    dtheta = std::numbers::pi / (double)ntslew;
    theta = 0.0;
    for (int m = 0; m <= ntslew; m++)
    {
        cslew[m] = 0.5 * (1.0 - std::cos (theta));
        theta += dtheta;
    }
    return cslew;
}

// From Thetis Project Files/Source/wdsp/rmatch.c:29-69 [v2.10.3.15 @3759d09]
class Mav {
public:
    Mav(int ringmin, int ringmax, double nomValue)
        : m_ringmin(ringmin)
        , m_ringmax(ringmax)
        , m_ring(static_cast<std::size_t>(ringmax), 0)
        , m_mask(ringmax - 1)
        , m_nomValue(nomValue)
    {
    }

    void flush()
    {
        std::fill(m_ring.begin(), m_ring.end(), 0);
        m_i = 0;
        m_load = 0;
        m_sum = 0;
    }

    double x(int input)
    {
        double output;
        if (m_load >= m_ringmax) {
            m_sum -= m_ring[m_i];
        }
        if (m_load < m_ringmax) {
            m_load++;
        }
        m_ring[m_i] = input;
        m_sum += m_ring[m_i];

        if (m_load >= m_ringmin) {
            output = (double)m_sum / (double)m_load;
        } else {
            output = m_nomValue;
        }
        m_i = (m_i + 1) & m_mask;
        return output;
    }

private:
    int m_ringmin;
    int m_ringmax;          // must be a power of two
    std::vector<int> m_ring;
    int m_mask;
    int m_i = 0;
    int m_load = 0;
    int m_sum = 0;
    double m_nomValue;
};

// From Thetis Project Files/Source/wdsp/rmatch.c:71-126 [v2.10.3.15 @3759d09]
// Separate positive and negative sums; the output is neg / pos.
class Aamav {
public:
    Aamav(int ringmin, int ringmax, double nomRatio)
        : m_ringmin(ringmin)
        , m_ringmax(ringmax)
        , m_ring(static_cast<std::size_t>(ringmax), 0)
        , m_mask(ringmax - 1)
        , m_nomRatio(nomRatio)
    {
    }

    void flush()
    {
        std::fill(m_ring.begin(), m_ring.end(), 0);
        m_i = 0;
        m_load = 0;
        m_pos = 0;
        m_neg = 0;
    }

    double x(int input)
    {
        double output;
        if (m_load >= m_ringmax)
        {
            if (m_ring[m_i] >= 0) {
                m_pos -= m_ring[m_i];
            } else {
                m_neg += m_ring[m_i];
            }
        }
        if (m_load <= m_ringmax) {
            m_load++;
        }
        m_ring[m_i] = input;
        if (m_ring[m_i] >= 0) {
            m_pos += m_ring[m_i];
        } else {
            m_neg -= m_ring[m_i];
        }
        if (m_load >= m_ringmin) {
            output = (double)m_neg / (double)m_pos;
        } else if (m_neg > 0 && m_pos > 0)
        {
            double frac = (double)m_load / (double)m_ringmin;
            output = (1.0 - frac) * m_nomRatio + frac * ((double)m_neg / (double)m_pos);
        }
        else {
            output = m_nomRatio;
        }
        m_i = (m_i + 1) & m_mask;
        return output;
    }

private:
    int m_ringmin;
    int m_ringmax;          // must be a power of two
    std::vector<int> m_ring;
    int m_mask;
    int m_i = 0;
    int m_load = 0;
    int m_pos = 0;
    int m_neg = 0;
    double m_nomRatio;
};

struct Sizes {
    bool valid = false;
    int maxNewsamps = 0;
    int ntslew = 0;
    int rsizeMin = 0;
    std::uint32_t capacity = 0;
    int firstStepIndex = 0;
    bool automatic = true;
};

int targetFrames(int stepMs, int outRate)
{
    return static_cast<int>(static_cast<std::int64_t>(stepMs) * outRate / 1000);
}

// From Thetis Project Files/Source/wdsp/rmatch.c:133-136 [v2.10.3.15 @3759d09]
// The ring is at least twice the largest resampler output and twice the
// output (device) block; NereusSDR adds twice the delay step's target.
int rsizeFor(int stepMs, const DeviceRateMatcher::Config& c, int maxNewsamps)
{
    int ringsize = 2 * targetFrames(stepMs, c.outRate);
    if (ringsize < 2 * maxNewsamps) {
        ringsize = 2 * maxNewsamps;
    }
    if (ringsize < 2 * c.callbackFrames) {
        ringsize = 2 * c.callbackFrames;
    }
    return ringsize;
}

// The smallest step at or above ms, or the last step.
int stepIndexAtOrAbove(double ms)
{
    const auto& steps = DeviceRateMatcher::kDelayStepsMs;
    for (std::size_t i = 0; i < steps.size(); ++i) {
        if (static_cast<double>(steps[i]) >= ms) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(steps.size()) - 1;
}

std::uint32_t nextPowerOfTwo(std::uint32_t value)
{
    std::uint32_t p = 1;
    while (p < value) {
        p <<= 1;
    }
    return p;
}

Sizes computeSizes(const DeviceRateMatcher::Config& c)
{
    Sizes s;
    if (c.inRate < kMinRate || c.inRate > kMaxRate || c.outRate < kMinRate
        || c.outRate > kMaxRate || c.writeBlockFrames <= 0
        || c.writeBlockFrames > kMaxWriteBlockFrames || c.callbackFrames <= 0
        || c.callbackFrames > kMaxCallbackFrames || c.delayMs < 0) {
        return s;
    }
    // From Thetis Project Files/Source/wdsp/rmatch.c:133-134 [v2.10.3.15 @3759d09]
    const double nomRatio = (double)c.outRate / (double)c.inRate;
    s.maxNewsamps = (int)(1.0 + (double)c.writeBlockFrames * (kMaxRatioMargin * nomRatio));
    s.rsizeMin = rsizeFor(DeviceRateMatcher::kDelayStepsMs.front(), c, s.maxNewsamps);
    // From Thetis Project Files/Source/wdsp/rmatch.c:153-154 [v2.10.3.15 @3759d09]
    // NereusSDR caps ntslew at the 2 ms step's rsize, fixed for the stream.
    s.ntslew = (int)(kSlewSeconds * c.outRate);
    if (s.ntslew + 1 > s.rsizeMin / 2) {
        s.ntslew = s.rsizeMin / 2 - 1;
    }
    const int rsizeMax = rsizeFor(DeviceRateMatcher::kDelayStepsMs.back(), c, s.maxNewsamps);
    s.capacity = nextPowerOfTwo(static_cast<std::uint32_t>(rsizeMax + s.maxNewsamps));
    if (c.delayMs == 0) {
        s.automatic = true;
        const double startMs = 1000.0 * c.callbackFrames / c.outRate
            + 1000.0 * c.writeBlockFrames / c.inRate;
        s.firstStepIndex = stepIndexAtOrAbove(startMs);
    } else {
        // A manual size not on the list takes the next step up.
        s.automatic = false;
        s.firstStepIndex = stepIndexAtOrAbove(static_cast<double>(c.delayMs));
    }
    s.valid = true;
    return s;
}

std::uint64_t subFloor(std::uint64_t a, std::uint64_t b)
{
    return a > b ? a - b : 0;
}

} // namespace

// ---------------------------------------------------------------------------
// The writer (DSP side)
// ---------------------------------------------------------------------------

struct DeviceRateMatcher::Writer {
    Writer(DeviceRateMatcher& owner, const Sizes& sizes)
        : m(owner)
        , cfg(owner.m_config)
        , maxNewsamps(sizes.maxNewsamps)
        , ntslew(sizes.ntslew)
        , cslew(makeSlewTable(sizes.ntslew))
        , in(static_cast<std::size_t>(owner.m_config.writeBlockFrames) * kChannels, 0.0)
        , resout(static_cast<std::size_t>(sizes.maxNewsamps) * kChannels, 0.0)
        , ffmav(kFfMavMin, kFfMavMax, (double)owner.m_config.outRate / (double)owner.m_config.inRate)
        , propmav(kPropRingMin, kPropRingMax, 0.0)
        , automatic(sizes.automatic)
        , stepIndex(sizes.firstStepIndex)
        , configStepIndex(sizes.firstStepIndex)
    {
        // From Thetis Project Files/Source/wdsp/rmatch.c:133-168 [v2.10.3.15 @3759d09]
        nomRatio = (double)cfg.outRate / (double)cfg.inRate;
        prGain = kPropGain * kPrGainReferenceRate / (double)cfg.outRate;	// adjust gain for rate
        invNomRatio = (double)cfg.inRate / (double)cfg.outRate;
        feedForward = 1.0;
        avDeviation = 0.0;
        var = kInitialVar;
        readStartup = (std::uint64_t)((double)cfg.outRate * kStartupDelaySeconds);
        writeStartup = (std::uint64_t)((double)cfg.inRate * kStartupDelaySeconds);
        controlFlag = false;
        rsize = rsizeFor(DeviceRateMatcher::kDelayStepsMs[stepIndex], cfg, maxNewsamps);
#ifdef HAVE_WDSP
        v = create_varsamp(1, cfg.writeBlockFrames, in.data(), resout.data(), cfg.inRate,
                           cfg.outRate, kFcHigh, kFcLow, kCoefficientDensity, kGain, var,
                           kVarMode);
#endif
    }

    ~Writer()
    {
#ifdef HAVE_WDSP
        if (v != nullptr) {
            destroy_varsamp(v);
        }
#endif
    }

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    MatcherRingHeader& ring() { return *m.m_ring; }

    std::uint64_t mask() { return static_cast<std::uint64_t>(m.m_ring->capacityFrames) - 1; }

    void setStep(int index)
    {
        stepIndex = index;
        rsize = rsizeFor(DeviceRateMatcher::kDelayStepsMs[stepIndex], cfg, maxNewsamps);
        m.m_delayStepMs.store(DeviceRateMatcher::kDelayStepsMs[stepIndex], std::memory_order_relaxed);
        ring().rsizeFrames.store(static_cast<std::uint32_t>(rsize), std::memory_order_relaxed);
        publishPacketMarks();
    }

    // NereusSDR: a packet writer's size and high-water mark for stats().
    // In steady playback the fill swings half a packet each side of the
    // target (rsize / 2), so a packet's write may reach the target plus
    // half a packet, plus the device's callback and one write block of
    // slack: nothing of a steady stream waits at that mark.  A burst is
    // written up to the mark and no further, so what a burst adds to the
    // delay while it lasts is that slack and no more.
    void publishPacketMarks()
    {
        int mark = 0;
        if (packetFrames > 0) {
            mark = std::min(rsize, rsize / 2 + packetOutMax - packetOutNominal / 2
                                       + cfg.callbackFrames + maxNewsamps);
        }
        m.m_packetFrames.store(packetFrames, std::memory_order_relaxed);
        m.m_packetOutFrames.store(packetOutMax, std::memory_order_relaxed);
        m.m_packetHighWaterFrames.store(mark, std::memory_order_relaxed);
    }

    // NereusSDR: the first size for a packet writer.  Never below the
    // smallest step whose ring holds one packet at the control's largest
    // ratio, the device's callback and one write block; a manual size
    // below that is raised to it.  The automatic size starts as the block
    // writer's does, at the device's callback plus one write, the write
    // being the packet.
    int packetStepIndex() const
    {
        const auto& steps = DeviceRateMatcher::kDelayStepsMs;
        const int last = static_cast<int>(steps.size()) - 1;
        int floorIndex = last;
        for (int i = 0; i <= last; ++i) {
            if (rsizeFor(steps[static_cast<std::size_t>(i)], cfg, maxNewsamps)
                >= packetOutMax + cfg.callbackFrames + maxNewsamps) {
                floorIndex = i;
                break;
            }
        }
        if (!automatic) {
            return std::max(floorIndex, configStepIndex);
        }
        const double startMs = 1000.0 * cfg.callbackFrames / cfg.outRate
            + 1000.0 * packetFrames / cfg.inRate;
        return std::max(floorIndex, stepIndexAtOrAbove(startMs));
    }

    // NereusSDR: the writer's packet changed (setWritePacketFrames).  The
    // stream that follows is a new one: its size is its own first size,
    // whatever the stream before grew to, and the dry runs the device made
    // while it waited for the first packet are not this stream's.
    void applyWritePacket(int frames)
    {
        packetFrames = frames;
        if (packetFrames > 0) {
            const int chunks = (packetFrames + cfg.writeBlockFrames - 1) / cfg.writeBlockFrames;
            packetOutMax = chunks * maxNewsamps;
            packetOutNominal = static_cast<int>(std::ceil(static_cast<double>(packetFrames) * nomRatio));
            setStep(packetStepIndex());
            dryRunsSeen = ring().dryRuns.load(std::memory_order_acquire);
        } else {
            packetOutMax = 0;
            packetOutNominal = 0;
            setStep(configStepIndex);
        }
    }

    // The silence a restart, a flush or a dry run leaves ahead of the next
    // audio: the target, or for a packet writer half a packet short of it,
    // where the fill stands just before a packet in steady playback.
    int padTargetFrames() const
    {
        return std::max(0, rsize / 2 - packetOutNominal / 2);
    }

    // Publishes the write index; the reader acquires it before reading.
    void publishWritten(std::uint64_t written)
    {
        ring().written.store(written, std::memory_order_release);
        writtenAtLast = written;
    }

    // Silence after `written`, no further than the physical capacity allows
    // ahead of the reader.
    void padSilence(std::uint64_t frames, std::uint64_t readNow)
    {
        std::uint64_t written = ring().written.load(std::memory_order_relaxed);
        const std::uint64_t capacity = ring().capacityFrames;
        const std::uint64_t used = written - readNow;
        const std::uint64_t room = used < capacity ? capacity - used : 0;
        const std::uint64_t n = std::min(frames, room);
        float* frames0 = ring().frames();
        for (std::uint64_t f = 0; f < n; ++f) {
            const std::uint64_t slot = (written + f) & mask();
            frames0[slot * kChannels + 0] = 0.0f;
            frames0[slot * kChannels + 1] = 0.0f;
        }
        publishWritten(written + n);
    }

    // The effective read index: a pending skip the reader has not taken
    // counts as taken.
    std::uint64_t effectiveRead(std::uint64_t readNow)
    {
        const std::uint64_t skip = ring().skipTo.load(std::memory_order_acquire);
        if (skip != kMatcherNoSkip && skip > readNow) {
            return skip;
        }
        return readNow;
    }

    void restartControl()
    {
        ffmav.flush();
        propmav.flush();
        var = kInitialVar;
        feedForward = 1.0;
        avDeviation = 0.0;
        readSamps = 0;
        writeSamps = 0;
        controlFlag = false;
        spanOpen = false;
        packetOwnTime = false;
        packetDeviation = 0;
        sinceOwnTimeFrames = 0;
        m.m_controlActive.store(false, std::memory_order_relaxed);
        ring().ratio.store(var, std::memory_order_relaxed);
        // The automatic size is chosen again from the gaps seen after the
        // restart: an idle writer's gap before it does not count (D7).
        // The size never shrinks (onControlStart only steps up).
        maxGapNs = 0;
        haveLastWrite = false;
    }

    // Drops what is queued: the reader jumps to the write index (blending
    // into it), finds silence up to the target, and the next audio fades in.
    // Nothing of the earlier audio is left: the staged frames and the
    // resampler's history go too.
    void dropQueued()
    {
        stageFrames = 0;
        // From Thetis Project Files/Source/wdsp/rmatch.c:247-254 [v2.10.3.15 @3759d09]
        // reset_rmatch rebuilds its varsamp (decalc_rmatch, calc_rmatch), so a
        // reset rmatch resamples from an empty history.  NereusSDR keeps the
        // resampler and clears its history in place, on the writer's thread.
#ifdef HAVE_WDSP
        // From WDSP varsamp.c:106
        if (v != nullptr) {
            flush_varsamp(v);
        }
#endif
        const std::uint64_t readNow = ring().read.load(std::memory_order_acquire);
        const std::uint64_t written = ring().written.load(std::memory_order_relaxed);
        ring().skipTo.store(written, std::memory_order_release);
        std::uint64_t pad = static_cast<std::uint64_t>(padTargetFrames());
        if (packetFrames > 0) {
            // NereusSDR: the packet that follows is written before the
            // reader has taken the skip, so what is dropped still stands
            // in the ring's memory: the silence leaves room there for the
            // whole packet.
            const std::uint64_t capacity = ring().capacityFrames;
            const std::uint64_t used = written - std::min(written, readNow);
            const std::uint64_t need = used + static_cast<std::uint64_t>(packetOutMax);
            pad = std::min(pad, capacity > need ? capacity - need : 0);
        }
        padSilence(pad, readNow);
        ucnt = ntslew;
    }

    void onControlStart()
    {
        m.m_controlActive.store(true, std::memory_order_relaxed);
        if (!automatic) {
            return;
        }
        const double needMs = 1000.0 * cfg.callbackFrames / cfg.outRate
            + static_cast<double>(maxGapNs) / 1.0e6;
        const int index = stepIndexAtOrAbove(needMs);
        if (index > stepIndex) {
            setStep(index);   // the fill then follows the control, with no padding
        }
    }

    // From Thetis Project Files/Source/wdsp/rmatch.c:256-273 [v2.10.3.15 @3759d09]
    // NereusSDR passes the deviation the writer measured for the event.
    void control(int change, int deviationIn)
    {
        {
            double current_ratio;
            current_ratio = ffmav.x(change);
            current_ratio *= invNomRatio;
            feedForward = kFfAlpha * current_ratio + (1.0 - kFfAlpha) * feedForward;
        }
        {
            // int deviation = a->n_ring - a->rsize / 2;  [original line from rmatch.c:265]
            int deviation = deviationIn;
            avDeviation = propmav.x(deviation);
        }
        // EnterCriticalSection (&a->cs_var);  [rmatch.c:268; the writer owns var]
        var = feedForward - prGain * avDeviation;
        if (var > kVarMax) {
            var = kVarMax;
        }
        if (var < kVarMin) {
            var = kVarMin;
        }
    }

    // One read or write event, as xrmatchOUT's and xrmatchIN's tails.
    // From Thetis Project Files/Source/wdsp/rmatch.c:353-359, 458-464 [v2.10.3.15 @3759d09]
    void controlEvent(bool isRead, int change, int deviation)
    {
        if (m.m_forceRatio.load(std::memory_order_relaxed)) {
            return;   // tests only: fixed ratio, control off
        }
        if (!controlFlag)
        {
            if (isRead) {
                readSamps += static_cast<std::uint64_t>(-change);
            } else {
                writeSamps += static_cast<std::uint64_t>(cfg.writeBlockFrames);
            }
            if ((readSamps >= readStartup) && (writeSamps >= writeStartup)) {
                controlFlag = true;
                onControlStart();
            }
        }
        if (controlFlag) {
            if (packetFrames > 0) {
                packetControl(isRead, change);
            } else {
                control(change, deviation);
            }
        }
    }

    // NereusSDR: the control for a packet writer.  control() above hears a
    // write block and a device read in turn and steers the mean fill; a
    // packet writer's blocks come a packet at a time, and a burst's packets
    // at the device's own pace (the writer keeps them until there is
    // room), so neither the mean fill nor the frames counted across a
    // burst say what the clocks do.  Both terms are control()'s, fed by
    // what does:
    //
    // The proportional term's deviation is the packet's (processChunk):
    // the fill a packet met when it came in its own time, plus half a
    // packet, against the target.
    //
    // The feed-forward counts whole spans, from one packet that came in
    // its own time to the next.  Every frame the source sent in a span was
    // written in it and the fill ends where it began, so the device's
    // frames over the span's are the clocks' ratio, whether the span is
    // one packet or a batch from the link.  A span is counted when it
    // closes, its reads and writes in turn as the block writer's come.  A
    // span longer than a second of audio is a backlog standing behind the
    // writer (after a stall): its frames were written at the device's pace
    // and only repeat the ratio in use, so it is left out and the
    // feed-forward holds.
    void packetControl(bool isRead, int change)
    {
        if (!isRead && packetBlock == 0 && packetOwnTime) {
            if (spanOpen
                && spanWriteBlocks * static_cast<std::uint64_t>(cfg.writeBlockFrames)
                    <= static_cast<std::uint64_t>(cfg.inRate)) {
                countSpan();
            }
            spanOpen = true;
            spanWriteBlocks = 0;
            spanReadCalls = 0;
            spanReadFrames = 0;
        }
        if (spanOpen) {
            if (isRead) {
                ++spanReadCalls;
                spanReadFrames += static_cast<std::uint64_t>(-change);
            } else {
                ++spanWriteBlocks;
            }
        }
        // From Thetis Project Files/Source/wdsp/rmatch.c:264-272 [v2.10.3.15 @3759d09]
        // control()'s proportional term and its limits.
        avDeviation = propmav.x(packetDeviation);
        var = feedForward - prGain * avDeviation;
        if (var > kVarMax) {
            var = kVarMax;
        }
        if (var < kVarMin) {
            var = kVarMin;
        }
    }

    // The closed span's events into the feed-forward: each write block,
    // and the reads shared out evenly among them.
    void countSpan()
    {
        const std::uint64_t per = spanReadCalls > 0 ? spanReadFrames / spanReadCalls : 0;
        std::uint64_t counted = 0;
        for (std::uint64_t block = 1; block <= spanWriteBlocks; ++block) {
            feedForwardEvent(cfg.writeBlockFrames);
            const std::uint64_t upTo = spanReadCalls * block / spanWriteBlocks;
            for (; counted < upTo; ++counted) {
                const std::uint64_t frames = (counted + 1 == spanReadCalls)
                    ? spanReadFrames - per * (spanReadCalls - 1) : per;
                feedForwardEvent(-static_cast<int>(frames));
            }
        }
    }

    // From Thetis Project Files/Source/wdsp/rmatch.c:258-263 [v2.10.3.15 @3759d09]
    // control()'s feed-forward term, for one event.
    void feedForwardEvent(int change)
    {
        double current_ratio;
        current_ratio = ffmav.x(change);
        current_ratio *= invNomRatio;
        feedForward = kFfAlpha * current_ratio + (1.0 - kFfAlpha) * feedForward;
    }

    // Replays the reads since the last write into the control.
    void replayReads(std::uint64_t readNow, std::uint64_t requestedNow, std::uint64_t readCallsNow)
    {
        const std::uint64_t k = readCallsNow - readCallsAtLast;
        const std::uint64_t r = requestedNow - requestedAtLast;
        const std::uint64_t d = subFloor(readNow, readAtLast);
        const std::int64_t half = rsize / 2;
        if (k > 0) {
            const std::uint64_t per = r / k;
            for (std::uint64_t i = 1; i <= k; ++i) {
                const std::uint64_t frames = (i == k) ? r - per * (k - 1) : per;
                const std::uint64_t readAtI = readAtLast + (d / k) * i + ((d % k) * i) / k;
                const std::int64_t fill = static_cast<std::int64_t>(writtenAtLast)
                    - static_cast<std::int64_t>(readAtI);
                controlEvent(true, -static_cast<int>(frames), static_cast<int>(fill - half));
            }
        }
        readCallsAtLast = readCallsNow;
        requestedAtLast = requestedNow;
        readAtLast = readNow;
    }

    int resample(double useVar)
    {
#ifdef NEREUS_BUILD_TESTS
        // Tests only: a ratio forced to exactly 1.0 at equal rates copies the
        // block, so a test can check frame order exactly (V-SW-6); varsamp
        // at 1.0 still filters.
        if (m.m_forceRatio.load(std::memory_order_relaxed) && useVar == 1.0
            && cfg.inRate == cfg.outRate) {
            std::copy(in.begin(), in.end(), resout.begin());
            return cfg.writeBlockFrames;
        }
#endif
#ifdef HAVE_WDSP
        return xvarsamp(v, useVar);
#else
        (void)useVar;
        std::copy(in.begin(), in.end(), resout.begin());
        return cfg.writeBlockFrames;
#endif
    }

    // Copies the resampled frames into the ring, fading them in while an
    // upslew is pending.
    void copyIn(std::uint64_t written, int newsamps)
    {
        float* frames0 = ring().frames();
        for (int f = 0; f < newsamps; ++f) {
            double left = resout[2 * f + 0];
            double right = resout[2 * f + 1];
            // From Thetis Project Files/Source/wdsp/rmatch.c:285-298 [v2.10.3.15 @3759d09]
            // upslew(): the frames after a dry run are faded in.
            if (ucnt >= 0)
            {
                left *= cslew[ntslew - ucnt];
                right *= cslew[ntslew - ucnt];
                ucnt--;
            }
            const std::uint64_t slot = (written + f) & mask();
            frames0[slot * kChannels + 0] = static_cast<float>(left);
            frames0[slot * kChannels + 1] = static_cast<float>(right);
        }
    }

    void publishReadout(std::uint64_t written, std::uint64_t readNow, double useVar)
    {
        ring().ratio.store(useVar, std::memory_order_relaxed);
        const double fill = controlFlag
            ? static_cast<double>(rsize / 2) + avDeviation
            : static_cast<double>(written - std::min(written, readNow));
        ring().fillFrames.store(fill, std::memory_order_relaxed);
    }

    // From Thetis Project Files/Source/wdsp/rmatch.c:301-362 [v2.10.3.15 @3759d09]
    // xrmatchIN for one writeBlockFrames chunk already staged in `in`.
    void processChunk(std::int64_t nowNs)
    {
        MatcherRingHeader& r = ring();
        const std::uint64_t readCallsNow = r.readCalls.load(std::memory_order_acquire);
        const std::uint64_t requestedNow = r.requested.load(std::memory_order_acquire);
        const std::uint64_t readNow = r.read.load(std::memory_order_acquire);
        const std::uint64_t dryRunsNow = r.dryRuns.load(std::memory_order_acquire);

        // A stalled reader is back: restart the control.
        if (stalled && readCallsNow != readCallsAtStall) {
            stalled = false;
            restartControl();
            // The gaps are measured from this write on.
            haveLastWrite = true;
            lastWriteNs = nowNs;
        }

        // NereusSDR: a packet writer's deviation, one for the packet and
        // the reads ahead of it (packetControl).  What the clocks move is
        // the fill a packet meets when it comes in its own time (it did
        // not wait for room, and the device has read since the packet
        // before): that, plus half a packet, is held at the target (the
        // mean fill, in steady playback).  The other packets of a burst
        // say nothing new and stand at the last such packet's deviation.
        // When no packet has come in its own time for a second of audio a
        // backlog is standing behind the writer, which holds the fill the
        // mark's slack above its working level: that slack is then the
        // deviation, so the backlog is played out.
        if (packetFrames > 0) {
            if (packetBlock == 0) {
                packetOwnTime = !packetWaited && readCallsNow != readCallsAtLast;
                if (packetOwnTime) {
                    const std::uint64_t writtenNow = r.written.load(std::memory_order_relaxed);
                    const std::int64_t met = static_cast<std::int64_t>(
                        writtenNow - std::min(writtenNow, effectiveRead(readNow)));
                    packetDeviation = static_cast<int>(met + packetOutNominal / 2 - rsize / 2);
                    sinceOwnTimeFrames = 0;
                } else if (sinceOwnTimeFrames > static_cast<std::uint64_t>(cfg.inRate)) {
                    packetDeviation = cfg.callbackFrames + maxNewsamps;
                }
            }
            sinceOwnTimeFrames += static_cast<std::uint64_t>(cfg.writeBlockFrames);
        }

        replayReads(readNow, requestedNow, readCallsNow);

        // A dry run: one step up (automatic), then silence up to the target,
        // and the audio after it fades in.  The fade-in is armed here, from
        // the count loaded above, so it always follows its padding: the
        // reader's upslewPending flag, set after the count, can be seen
        // before the count is and is not used to arm it.
        if (dryRunsNow != dryRunsSeen) {
            dryRunsSeen = dryRunsNow;
            // NereusSDR: a packet writer's dry run is its source stopping
            // (a stalled link or worker), not the clocks drifting.  The
            // frames the device asked for meanwhile would read as a faster
            // device for as long as the feed-forward average holds them,
            // and the backlog that follows is written at the device's
            // pace, so the control starts again from the audio after it.
            if (packetFrames > 0) {
                restartControl();
                haveLastWrite = true;
                lastWriteNs = nowNs;
            }
            if (automatic && stepIndex + 1 < static_cast<int>(DeviceRateMatcher::kDelayStepsMs.size())) {
                setStep(stepIndex + 1);
            }
            const std::uint64_t written = r.written.load(std::memory_order_relaxed);
            const std::uint64_t fill = written - std::min(written, effectiveRead(readNow));
            const std::uint64_t half = static_cast<std::uint64_t>(padTargetFrames());
            if (fill < half) {
                padSilence(half - fill, readNow);
            }
            r.upslewPending.store(0, std::memory_order_relaxed);
            ucnt = ntslew;
        }

        double useVar;
        // EnterCriticalSection (&a->cs_var);  [rmatch.c:309; the writer owns var]
        if (!m.m_forceRatio.load(std::memory_order_relaxed)) {
            useVar = var;
        } else {
            useVar = m.m_forcedRatio.load(std::memory_order_relaxed);
        }
        const int newsamps = resample(useVar);

        std::uint64_t written = r.written.load(std::memory_order_relaxed);
        const std::uint64_t capacity = r.capacityFrames;

        // The reader has stopped reading: a write would pass the physical
        // capacity.  The block is dropped and the reader, when it is back,
        // skips to half the ring before the write index.
        const std::uint64_t spliceFrames = droppedSinceWrite ? static_cast<std::uint64_t>(rsize / 2) : 0;
        if (written + spliceFrames + static_cast<std::uint64_t>(newsamps) - readNow > capacity) {
            r.overruns.fetch_add(1, std::memory_order_relaxed);
            r.skipTo.store(subFloor(written, static_cast<std::uint64_t>(rsize / 2)),
                           std::memory_order_release);
            if (!stalled) {
                stalled = true;
                readCallsAtStall = readCallsNow;
            }
            droppedSinceWrite = true;
            publishReadout(written, readNow, useVar);
            return;
        }

        // After dropped blocks the new audio does not continue the old: as
        // a flush, the reader blends from where it is into silence up to the
        // target, and the new audio fades in.
        if (droppedSinceWrite) {
            droppedSinceWrite = false;
            r.skipTo.store(written, std::memory_order_release);
            padSilence(spliceFrames, readNow);
            written = r.written.load(std::memory_order_relaxed);
            ucnt = ntslew;
        }

        std::uint64_t effRead = effectiveRead(readNow);
        // if ((ovfl = a->n_ring - a->rsize) > 0)  [rmatch.c:318; fill measured before the write]
        const bool overflow = (written - std::min(written, effRead))
            + static_cast<std::uint64_t>(newsamps) > static_cast<std::uint64_t>(rsize);

        copyIn(written, newsamps);
        written += static_cast<std::uint64_t>(newsamps);
        publishWritten(written);

        if (overflow)
        {
            r.overruns.fetch_add(1, std::memory_order_relaxed);
            // a->n_ring = a->rsize / 2;
            // a->n_ring = a->rsize; //  [rmatch.c:322; the reader is sent to the newest rsize frames]
            // a->iout = (a->iout + ovfl + a->rsize / 2) % a->rsize;
            // a->iout = (a->iout + ovfl) % a->rsize; //  [rmatch.c:336; the reader makes the jump and blend()]
            const std::uint64_t skip = subFloor(written, static_cast<std::uint64_t>(rsize));
            r.skipTo.store(skip, std::memory_order_release);
            effRead = skip;
        }
        r.lastWriteNs.store(nowNs, std::memory_order_release);

        const std::int64_t fill = static_cast<std::int64_t>(written)
            - static_cast<std::int64_t>(std::min(written, effRead));
        controlEvent(false, cfg.writeBlockFrames, static_cast<int>(fill - rsize / 2));
        publishReadout(written, readNow, m.m_forceRatio.load(std::memory_order_relaxed) ? useVar : var);
    }

    DeviceRateMatcher& m;
    const DeviceRateMatcher::Config& cfg;
    int maxNewsamps;
    int ntslew;
    std::vector<double> cslew;
    std::vector<double> in;       // writeBlockFrames interleaved stereo
    std::vector<double> resout;   // maxNewsamps interleaved stereo
    int stageFrames = 0;
#ifdef HAVE_WDSP
    VARSAMP v = nullptr;
#endif
    Aamav ffmav;
    Mav propmav;
    double nomRatio = 1.0;
    double invNomRatio = 1.0;
    double prGain = 0.0;
    double feedForward = 1.0;
    double avDeviation = 0.0;
    double var = kInitialVar;
    std::uint64_t readSamps = 0;
    std::uint64_t writeSamps = 0;
    std::uint64_t readStartup = 0;
    std::uint64_t writeStartup = 0;
    bool controlFlag = false;
    int ucnt = -1;
    std::uint64_t writtenAtLast = 0;
    std::uint64_t readAtLast = 0;
    std::uint64_t requestedAtLast = 0;
    std::uint64_t readCallsAtLast = 0;
    std::uint64_t dryRunsSeen = 0;
    bool stalled = false;
    bool droppedSinceWrite = false;
    std::uint64_t readCallsAtStall = 0;
    bool automatic = true;
    int stepIndex = 0;
    int configStepIndex = 0;      // the configuration's own first size
    int rsize = 0;
    // The packet writer (0: the block writer): the packet in input frames,
    // and in device frames at the nominal ratio and at the most the
    // resampler makes of it.
    std::uint32_t packetGeneration = 0;
    int packetFrames = 0;
    int packetOutNominal = 0;
    int packetOutMax = 0;
    // The write block of the packet being written.
    int packetBlock = 0;
    // Whether the packet being written waited for room, and its deviation
    // for the control.
    bool packetWaited = false;
    // Whether it came in its own time, the deviation the control hears,
    // and the input frames written since a packet last did.
    bool packetOwnTime = false;
    int packetDeviation = 0;
    std::uint64_t sinceOwnTimeFrames = 0;
    // The span being measured for the feed-forward (packetControl): its
    // write blocks, and the device's reads and the frames they asked for.
    bool spanOpen = false;
    std::uint64_t spanWriteBlocks = 0;
    std::uint64_t spanReadCalls = 0;
    std::uint64_t spanReadFrames = 0;
    bool haveLastWrite = false;
    std::int64_t lastWriteNs = 0;
    std::int64_t maxGapNs = 0;
};

// ---------------------------------------------------------------------------
// DeviceRateMatcher
// ---------------------------------------------------------------------------

std::size_t DeviceRateMatcher::ringBytes(const Config& config)
{
    const Sizes sizes = computeSizes(config);
    if (!sizes.valid) {
        return 0;
    }
    return matcherRingBytes(sizes.capacity, kChannels);
}

DeviceRateMatcher::DeviceRateMatcher(const Config& config)
    : m_config(config)
{
    const std::size_t bytes = ringBytes(config);
    if (bytes == 0) {
        qCWarning(lcAudio) << "DeviceRateMatcher: configuration out of range" << config.inRate
                           << config.outRate << config.writeBlockFrames << config.callbackFrames
                           << config.delayMs;
        return;
    }
    m_ownedMemory = std::make_unique<std::byte[]>(bytes);
    init(config, m_ownedMemory.get(), bytes);
}

DeviceRateMatcher::DeviceRateMatcher(const Config& config, void* memory, std::size_t bytes)
    : m_config(config)
{
    init(config, memory, bytes);
}

DeviceRateMatcher::~DeviceRateMatcher() = default;

void DeviceRateMatcher::init(const Config& config, void* memory, std::size_t bytes)
{
    const Sizes sizes = computeSizes(config);
    if (!sizes.valid) {
        qCWarning(lcAudio) << "DeviceRateMatcher: configuration out of range" << config.inRate
                           << config.outRate << config.writeBlockFrames << config.callbackFrames
                           << config.delayMs;
        return;
    }
#ifndef HAVE_WDSP
    if (config.inRate != config.outRate) {
        qCWarning(lcAudio) << "DeviceRateMatcher: rate conversion needs WDSP" << config.inRate
                           << config.outRate;
        return;
    }
#endif
    if (memory == nullptr || bytes < matcherRingBytes(sizes.capacity, kChannels)
        || reinterpret_cast<std::uintptr_t>(memory) % alignof(MatcherRingHeader) != 0) {
        qCWarning(lcAudio) << "DeviceRateMatcher: ring memory too small or misaligned" << bytes;
        return;
    }
    m_ring = constructMatcherRing(memory, sizes.capacity, kChannels,
                                  static_cast<std::uint32_t>(sizes.ntslew));
    if (m_ring == nullptr) {
        return;
    }
    m_writer = std::make_unique<Writer>(*this, sizes);
#ifdef HAVE_WDSP
    if (m_writer->v == nullptr) {
        m_writer.reset();
        m_ring = nullptr;
        return;
    }
#endif
    m_writer->setStep(sizes.firstStepIndex);
    m_ring->ratio.store(kInitialVar, std::memory_order_relaxed);
    m_ring->skipTo.store(kMatcherNoSkip, std::memory_order_relaxed);
    // diagnostics  [original inline comment from rmatch.c:169; dryRuns and
    // overruns are counted in the ring]
    // From Thetis Project Files/Source/wdsp/rmatch.c:139-141 [v2.10.3.15 @3759d09]
    // The ring starts holding rsize / 2 frames of silence.
    m_writer->publishWritten(static_cast<std::uint64_t>(m_writer->rsize / 2));
    m_ring->fillFrames.store(static_cast<double>(m_writer->rsize / 2), std::memory_order_relaxed);
}

bool DeviceRateMatcher::valid() const
{
    return m_ring != nullptr && m_writer != nullptr;
}

void DeviceRateMatcher::write(const float* interleavedStereo, int frames, std::int64_t nowNs)
{
    if (!valid() || interleavedStereo == nullptr || frames <= 0) {
        return;
    }
    Writer& w = *m_writer;
    // A new packet size starts the stream afresh, as a restart does.
    const std::uint32_t packetGeneration = m_writePacketGeneration.load(std::memory_order_acquire);
    const bool packetChanged = packetGeneration != w.packetGeneration;
    if (packetChanged) {
        w.packetGeneration = packetGeneration;
        w.applyWritePacket(m_writePacketRequested.load(std::memory_order_acquire));
        m_writePacketApplied.store(packetGeneration, std::memory_order_release);
    }
    // One write is one packet: its first block, and whether it waited.
    w.packetBlock = 0;
    w.packetWaited = m_writePacketWaited.load(std::memory_order_acquire);
    const bool restart = m_restartRequested.exchange(false, std::memory_order_acq_rel)
        || packetChanged;
    const bool flush = m_flushRequested.exchange(false, std::memory_order_acq_rel);
    if (restart) {
        w.restartControl();
        w.dropQueued();
    } else if (flush) {
        w.dropQueued();
    }

    // The largest gap between writes, for the automatic size; after a
    // restart, from the restart's own write on.
    if (w.haveLastWrite) {
        w.maxGapNs = std::max(w.maxGapNs, nowNs - w.lastWriteNs);
    }
    w.haveLastWrite = true;
    w.lastWriteNs = nowNs;

    const int block = m_config.writeBlockFrames;
    int pos = 0;
    while (pos < frames) {
        const int n = std::min(frames - pos, block - w.stageFrames);
        const float* src = interleavedStereo + static_cast<std::ptrdiff_t>(pos) * kChannels;
        double* dst = w.in.data() + static_cast<std::ptrdiff_t>(w.stageFrames) * kChannels;
        for (int i = 0; i < n * kChannels; ++i) {
            dst[i] = static_cast<double>(src[i]);
        }
        w.stageFrames += n;
        pos += n;
        if (w.stageFrames == block) {
            w.processChunk(nowNs);
            w.stageFrames = 0;
            if (w.packetFrames > 0) {
                ++w.packetBlock;
            }
        }
    }
}

void DeviceRateMatcher::requestFlush()
{
    m_flushRequested.store(true, std::memory_order_release);
}

void DeviceRateMatcher::requestRestart()
{
    m_restartRequested.store(true, std::memory_order_release);
}

void DeviceRateMatcher::setWritePacketFrames(int frames, bool waited)
{
    m_writePacketWaited.store(waited, std::memory_order_release);
    const int packet = std::clamp(frames, 0, kMaxWritePacketFrames);
    // The value first, then the count: a writer that sees the new count
    // reads the new value.
    if (m_writePacketRequested.exchange(packet, std::memory_order_acq_rel) != packet) {
        m_writePacketGeneration.fetch_add(1, std::memory_order_acq_rel);
    }
}

MatcherReader DeviceRateMatcher::makeReader()
{
    return MatcherReader(m_ring);
}

MatcherRingHeader* DeviceRateMatcher::ring()
{
    return m_ring;
}

double DeviceRateMatcher::ratio() const
{
    if (m_ring == nullptr) {
        return 1.0;
    }
    return m_ring->ratio.load(std::memory_order_relaxed);
}

int DeviceRateMatcher::delayStepMs() const
{
    return m_delayStepMs.load(std::memory_order_relaxed);
}

double DeviceRateMatcher::fillFrames() const
{
    if (m_ring == nullptr) {
        return 0.0;
    }
    if (m_controlActive.load(std::memory_order_relaxed)) {
        return m_ring->fillFrames.load(std::memory_order_relaxed);
    }
    const std::uint64_t read = m_ring->read.load(std::memory_order_acquire);
    const std::uint64_t written = m_ring->written.load(std::memory_order_acquire);
    return static_cast<double>(written - std::min(written, read));
}

int DeviceRateMatcher::resamplerDelayFrames() const
{
    return resamplerDelayFramesFor(m_config.inRate, m_config.outRate);
}

int DeviceRateMatcher::resamplerDelayFramesFor(int inRate, int outRate)
{
    if (inRate <= 0 || outRate <= 0) {
        return 0;
    }
    // From Thetis Project Files/Source/wdsp/varsamp.c:41-60 [v2.10.3.15 @3759d09]
    double min_rate, norm_rate;
    // double max_rate;
    if (outRate >= inRate)
    {
        min_rate = (double)inRate;
        // max_rate = (double)a->out_rate;
        norm_rate = min_rate;
    }
    else
    {
        min_rate = (double)outRate;
        // max_rate = (double)a->in_rate;
        // norm_rate = max_rate;
        norm_rate = (double)inRate;
    }
    const int rsize = (int)(kVarsampTapsAtUnity * norm_rate / min_rate);
    // As RemoteAudioRateMatcher::filterDelayFrames: half the filter, less one.
    return rsize / 2 - 1;
}

AudioDelayParts DeviceRateMatcher::delayParts(double deviceBufferMs, double deviceLatencyMs) const
{
    AudioDelayParts parts;
    parts.deviceBufferMs = deviceBufferMs;
    parts.deviceLatencyMs = deviceLatencyMs;
    if (!valid()) {
        return parts;
    }
    const double msPerFrame = 1000.0 / static_cast<double>(m_config.outRate);
    parts.matcherFillMs = fillFrames() * msPerFrame;
    parts.resamplerMs = static_cast<double>(resamplerDelayFrames()) * msPerFrame;
    return parts;
}

DeviceRateMatcherStats DeviceRateMatcher::stats() const
{
    DeviceRateMatcherStats s;
    if (m_ring == nullptr) {
        return s;
    }
    s.dryRuns = m_ring->dryRuns.load(std::memory_order_relaxed);
    s.overruns = m_ring->overruns.load(std::memory_order_relaxed);
    s.ratio = ratio();
    s.fillFrames = fillFrames();
    s.rsizeFrames = static_cast<int>(m_ring->rsizeFrames.load(std::memory_order_relaxed));
    s.capacityFrames = static_cast<int>(m_ring->capacityFrames);
    s.delayStepMs = delayStepMs();
    s.controlActive = m_controlActive.load(std::memory_order_relaxed);
    // The frames queued now; a skip the reader has not taken counts as
    // taken, as the writer counts it.
    const std::uint64_t written = m_ring->written.load(std::memory_order_acquire);
    std::uint64_t read = m_ring->read.load(std::memory_order_acquire);
    const std::uint64_t skip = m_ring->skipTo.load(std::memory_order_acquire);
    if (skip != kMatcherNoSkip && skip > read) {
        read = skip;
    }
    s.queuedFrames = static_cast<int>(std::min<std::uint64_t>(
        written - std::min(written, read), m_ring->capacityFrames));
    // A packet size the writer has not applied yet reads as none: the
    // size is then the stream's before it.
    if (m_writePacketApplied.load(std::memory_order_acquire)
        == m_writePacketGeneration.load(std::memory_order_acquire)) {
        s.packetFrames = m_packetFrames.load(std::memory_order_relaxed);
        s.packetOutFrames = m_packetOutFrames.load(std::memory_order_relaxed);
        s.packetHighWaterFrames = m_packetHighWaterFrames.load(std::memory_order_relaxed);
    }
    return s;
}

#ifdef NEREUS_BUILD_TESTS
void DeviceRateMatcher::forceRatioForTest(std::optional<double> ratio)
{
    m_forcedRatio.store(ratio.value_or(1.0), std::memory_order_relaxed);
    m_forceRatio.store(ratio.has_value(), std::memory_order_relaxed);
}
#endif

// ---------------------------------------------------------------------------
// The reader (device side)
// ---------------------------------------------------------------------------

struct MatcherReader::State {
    MatcherRingHeader* ring = nullptr;
    float* frames = nullptr;
    std::uint64_t mask = 0;
    int ntslew = 0;
    std::vector<double> cslew;
    std::vector<double> baux;   // (ntslew + 1) interleaved stereo frames

    // A dry run's slew tail still to play (dslew's second loop), from
    // dlast, at cslew[tailJ] down to cslew[0].  -1: none.
    int tailJ = -1;
    double dlast[2] = {0.0, 0.0};
    double lastOut[2] = {0.0, 0.0};

    // An overrun skip's blend still to play: the next ring frame is mixed
    // with baux[blendI].  -1: none.
    int blendI = -1;

    std::atomic<bool> fadeRequested{false};
    std::atomic<bool> faded{false};
    int fadeM = 0;          // frames of the fade played
    bool silent = false;
};

MatcherReader::MatcherReader(MatcherRingHeader* ring)
{
    if (ring == nullptr || ring->channels != kChannels || ring->capacityFrames == 0) {
        return;
    }
    m_state = std::make_unique<State>();
    State& s = *m_state;
    s.ring = ring;
    s.frames = ring->frames();
    s.mask = static_cast<std::uint64_t>(ring->capacityFrames) - 1;
    s.ntslew = static_cast<int>(ring->slewFrames);
    s.cslew = makeSlewTable(s.ntslew);
    s.baux.assign((static_cast<std::size_t>(s.ntslew) + 1) * kChannels, 0.0);
}

MatcherReader::MatcherReader() = default;
MatcherReader::~MatcherReader() = default;
MatcherReader::MatcherReader(MatcherReader&&) noexcept = default;
MatcherReader& MatcherReader::operator=(MatcherReader&&) noexcept = default;

bool MatcherReader::valid() const
{
    return m_state != nullptr;
}

void MatcherReader::requestFadeOut()
{
    if (m_state != nullptr) {
        m_state->fadeRequested.store(true, std::memory_order_release);
    }
}

bool MatcherReader::fadedOut() const
{
    return m_state != nullptr && m_state->faded.load(std::memory_order_acquire);
}

// From Thetis Project Files/Source/wdsp/rmatch.c:364-467 [v2.10.3.15 @3759d09]
// xrmatchOUT with dslew, and the reader's half of xrmatchIN's overflow
// branch with blend() (rmatch.c:275-283, 318-337).
void MatcherReader::read(float* interleaved, int frames)
{
    if (m_state == nullptr || interleaved == nullptr || frames <= 0) {
        return;
    }
    State& s = *m_state;
    MatcherRingHeader& r = *s.ring;
    const int ntslew = s.ntslew;
    std::uint64_t read = r.read.load(std::memory_order_relaxed);

    // An overrun skip: save the frames it would have played next (baux),
    // jump, and blend from them into the new ones.
    std::uint64_t skip = r.skipTo.load(std::memory_order_acquire);
    std::uint64_t written = r.written.load(std::memory_order_acquire);
    if (skip != kMatcherNoSkip
        && r.skipTo.compare_exchange_strong(skip, kMatcherNoSkip, std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
        if (skip > read && skip <= written) {
            const std::uint64_t avail = written - read;
            const int saved = static_cast<int>(std::min<std::uint64_t>(avail,
                                               static_cast<std::uint64_t>(ntslew) + 1));
            double holdL = s.lastOut[0];
            double holdR = s.lastOut[1];
            for (int i = 0; i <= ntslew; ++i) {
                if (i < saved) {
                    const std::uint64_t slot = (read + static_cast<std::uint64_t>(i)) & s.mask;
                    holdL = s.frames[slot * kChannels + 0];
                    holdR = s.frames[slot * kChannels + 1];
                }
                s.baux[2 * i + 0] = holdL;
                s.baux[2 * i + 1] = holdR;
            }
            read = skip;
            s.blendI = 0;
        }
    }

    int pos = 0;
    // A slew tail from an earlier dry run plays before any new ring frames.
    while (s.tailJ >= 0 && pos < frames)
    {
        interleaved[2 * pos + 0] = static_cast<float>(s.dlast[0] * s.cslew[s.tailJ]);
        interleaved[2 * pos + 1] = static_cast<float>(s.dlast[1] * s.cslew[s.tailJ]);
        s.tailJ--;
        pos++;
    }

    const int need = frames - pos;
    const std::uint64_t avail = written - std::min(written, read);
    const int take = static_cast<int>(std::min<std::uint64_t>(avail, static_cast<std::uint64_t>(need)));
    for (int f = 0; f < take; ++f) {
        const std::uint64_t slot = (read + static_cast<std::uint64_t>(f)) & s.mask;
        double left = s.frames[slot * kChannels + 0];
        double right = s.frames[slot * kChannels + 1];
        if (s.blendI >= 0) {
            // From Thetis Project Files/Source/wdsp/rmatch.c:275-283 [v2.10.3.15 @3759d09]
            const int i = s.blendI;
            left = s.cslew[i] * left + (1.0 - s.cslew[i]) * s.baux[2 * i + 0];
            right = s.cslew[i] * right + (1.0 - s.cslew[i]) * s.baux[2 * i + 1];
            s.blendI = (i + 1 > ntslew) ? -1 : i + 1;
        }
        interleaved[2 * (pos + f) + 0] = static_cast<float>(left);
        interleaved[2 * (pos + f) + 1] = static_cast<float>(right);
    }
    read += static_cast<std::uint64_t>(take);

    if (take < need)
    {
        // if (a->n_ring < a->outsize)  [rmatch.c:436] dslew (a);
        r.dryRuns.fetch_add(1, std::memory_order_relaxed);
        r.upslewPending.store(1, std::memory_order_release);
        int i, j, k;
        if (take > ntslew + 1)
        {
            i = pos + (take - (ntslew + 1));
            j = ntslew;
            k = ntslew + 1;
        }
        else
        {
            i = pos;
            j = ntslew;
            k = take;
            if (take == 0) {
                // a->dlast[] is the last frame of the previous output (rmatch.c:456-457).
                if (pos > 0) {
                    s.dlast[0] = interleaved[2 * (pos - 1) + 0];
                    s.dlast[1] = interleaved[2 * (pos - 1) + 1];
                } else {
                    s.dlast[0] = s.lastOut[0];
                    s.dlast[1] = s.lastOut[1];
                }
            }
        }
        while (k > 0 && j >= 0)
        {
            if (k == 1)
            {
                s.dlast[0] = interleaved[2 * i + 0];
                s.dlast[1] = interleaved[2 * i + 1];
            }
            interleaved[2 * i + 0] = static_cast<float>(interleaved[2 * i + 0] * s.cslew[j]);
            interleaved[2 * i + 1] = static_cast<float>(interleaved[2 * i + 1] * s.cslew[j]);
            i++;
            j--;
            k--;
        }
        // The rest of the slew from dlast, continuing into later reads.
        s.tailJ = j;
        int out = pos + take;
        while (s.tailJ >= 0 && out < frames)
        {
            interleaved[2 * out + 0] = static_cast<float>(s.dlast[0] * s.cslew[s.tailJ]);
            interleaved[2 * out + 1] = static_cast<float>(s.dlast[1] * s.cslew[s.tailJ]);
            s.tailJ--;
            out++;
        }
        // zeros = a->outsize + a->rsize / 2 - n;
        // if ((zeros = a->outsize - n) > 0) //  [rmatch.c:404-405; the writer pads up to the target]
        for (; out < frames; ++out) {
            interleaved[2 * out + 0] = 0.0f;
            interleaved[2 * out + 1] = 0.0f;
        }
        // a->n_ring = a->outsize + a->rsize / 2;
        // a->n_ring = n; //  [rmatch.c:421-422; the reader never writes the ring]
        // a->iin = (a->iout + a->outsize + a->rsize/2) % a->rsize;
        // a->iin = (a->iout + a->n_ring) % a->rsize; //  [rmatch.c:423-424; the writer owns the write index]
    }

    // A requested fade: down to silence over ntslew frames, then silence.
    if (s.silent) {
        std::memset(interleaved, 0, static_cast<std::size_t>(frames) * kChannels * sizeof(float));
    } else if (s.fadeRequested.load(std::memory_order_acquire)) {
        for (int f = 0; f < frames; ++f) {
            double gain = 0.0;
            if (s.fadeM < ntslew) {
                s.fadeM++;
                gain = s.cslew[ntslew - s.fadeM];
            }
            interleaved[2 * f + 0] = static_cast<float>(interleaved[2 * f + 0] * gain);
            interleaved[2 * f + 1] = static_cast<float>(interleaved[2 * f + 1] * gain);
        }
        if (s.fadeM >= ntslew) {
            s.silent = true;
            s.faded.store(true, std::memory_order_release);
        }
    }

    s.lastOut[0] = interleaved[2 * (frames - 1) + 0];
    s.lastOut[1] = interleaved[2 * (frames - 1) + 1];

    r.read.store(read, std::memory_order_release);
    r.requested.fetch_add(static_cast<std::uint64_t>(frames), std::memory_order_release);
    r.readCalls.fetch_add(1, std::memory_order_release);
}

} // namespace NereusSDR
