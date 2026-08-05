// =================================================================
// src/core/audio/MasterMixer.cpp  (NereusSDR)
// =================================================================
// See MasterMixer.h for contract, and for the Thetis ChannelMaster
// structure this follows plus the three divergences from it.
//
// Ported from Thetis sources (structural derivation, not a line-by-line
// translation -- the architecture is upstream's, the semantics are ours):
//   Project Files/Source/ChannelMaster/aamix.c [v2.10.3.15]
//     (per-producer ring + readiness barrier + one summed output)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-07-27 -- Per-slice mute / volume / pan mixer reworked from a
//                 single shared accumulator into per-slice rings behind
//                 a readiness barrier, so N slices produce ONE mixed
//                 block per audio period instead of N pushes. The ring
//                 + barrier + single-summed-output STRUCTURE is Warren
//                 Pratt's from aamix.c; the per-slice gain / pan / mute
//                 semantics and the anti-click gain ramp are
//                 NereusSDR-original. Three divergences from the
//                 upstream structure are argued in MasterMixer.h.
//                 Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
// =================================================================

// --- From aamix.c ---
/*  aamix.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014 Warren Pratt, NR0V

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


#include "MasterMixer.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <limits>

namespace NereusSDR {


void MasterMixer::setSliceGain(int sliceId, float gain, float pan) {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    auto& st = m_slices[sliceId];
    st.gain.store(std::clamp(gain, 0.0f, 1.0f), std::memory_order_release);
    st.pan.store(std::clamp(pan, -1.0f, 1.0f),  std::memory_order_release);
}

void MasterMixer::setSliceMuted(int sliceId, bool muted) {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    m_slices[sliceId].muted.store(muted, std::memory_order_release);
}

void MasterMixer::removeSlice(int sliceId) {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    m_slices.erase(sliceId);
}

const float* MasterMixer::upSlewWindow() {
    // Built once, then read-only, so the audio thread never allocates.
    static const std::vector<float> window = [] {
        std::vector<float> w(static_cast<size_t>(kSlewUpFrames) + 1);
        // Thetis uses M_PI via its own headers; spell it out so this does
        // not depend on _USE_MATH_DEFINES being set on MSVC.
        constexpr double kPi = 3.14159265358979323846;
        const double delta = kPi / static_cast<double>(kSlewUpFrames);
        double theta = 0.0;
        for (int i = 0; i <= kSlewUpFrames; ++i) {
            w[static_cast<size_t>(i)] =
                static_cast<float>(0.5 * (1.0 - std::cos(theta)));
            theta += delta;
        }
        return w;
    }();
    return window.data();
}

void MasterMixer::setSliceStreaming(int sliceId, bool streaming) {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    auto& st = m_slices[sliceId];
    st.streaming.store(streaming, std::memory_order_release);
    if (streaming && m_slewUpFrames > 0) {
        // Arm the master up-slew, so whatever the mix produces next fades
        // in rather than snapping to full amplitude. From Thetis
        // open_mixer (aamix.c:494-496 [v2.10.3.15]), which sets the upslew
        // flag on every membership change and then blocks until it runs:
        //   InterlockedBitTestAndSet (&a->slew.uflag, 0);
        //
        // Skipped when this instance's slew is disabled (m_slewUpFrames ==
        // 0, e.g. the anti-VOX mixer), so a disabled instance never arms a
        // fade tryDrain() will not apply anyway.
        m_slewPos.store(0, std::memory_order_release);
    }
    if (!streaming) {
        // Drop it from the wait set now rather than waiting for the audio
        // thread to notice, so a drain already in flight stops blocking on
        // it. From Thetis aamix.c:522 [v2.10.3.15], where clearing the
        // stream's a->active bit rebuilds Aready in the same call.
        st.producing.store(false, std::memory_order_release);
        // Invalidate queued audio without touching rd/wr/avail here. Those
        // indices are audio-thread-owned; accumulate acknowledges the new
        // generation and resets them before accepting a fresh block.
        st.streamGeneration.fetch_add(1, std::memory_order_acq_rel);
    }
    // Publish this boundary last. An acquire load that observes the new
    // epoch also observes streaming/producing/generation above.
    m_membershipEpoch.fetch_add(1, std::memory_order_release);
}

void MasterMixer::setSliceOpportunistic(int sliceId, bool opportunistic) {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    m_slices[sliceId].opportunistic.store(opportunistic,
                                          std::memory_order_release);
}

void MasterMixer::setRampFrames(int frames) {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    m_rampFrames = std::max(1, frames);
}

void MasterMixer::setSlewUpFrames(int frames) {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    // upSlewWindow() is built once for kSlewUpFrames entries, so only
    // "disabled" and "the default" are representable lengths; anything
    // else clamps to the default rather than indexing a table sized for a
    // different length.
    m_slewUpFrames = (frames <= 0) ? 0 : kSlewUpFrames;
    // Park the position past the end so a shortened window cannot leave a
    // drain mid-fade against a table it has outgrown.
    m_slewPos.store(m_slewUpFrames, std::memory_order_release);
}

int MasterMixer::producingSliceCount() const {
    std::lock_guard<std::mutex> lk(m_sliceMapMutex);
    int n = 0;
    for (const auto& kv : m_slices) {
        if (kv.second.producing.load(std::memory_order_acquire)) { ++n; }
    }
    return n;
}

void MasterMixer::ensureRing(SliceState& st, int frames) {
    const int want = frames * kRingBlocks;
    if (st.capFrames >= want) { return; }
    // Growing discards whatever was queued. This only happens on the
    // first block, or on a block-size change, and both are already
    // discontinuities; the ramp covers the seam.
    st.ring.assign(static_cast<size_t>(want) * 2, 0.0f);
    st.capFrames = want;
    st.rd = 0;
    st.wr = 0;
    st.avail = 0;
}

void MasterMixer::accumulate(int sliceId, const float* samples, int frames,
                             bool muted) {
    // Audio-thread hot path. No lock; rely on startup/connect-time
    // invariant that the map is stable while audio is streaming.
    auto it = m_slices.find(sliceId);
    if (it == m_slices.end()) { return; }
    if (samples == nullptr || frames <= 0) { return; }

    SliceState& st = it->second;
    const bool opportunistic =
        st.opportunistic.load(std::memory_order_acquire);
    if (!opportunistic
        && !st.streaming.load(std::memory_order_acquire)) {
        return;
    }

    const std::uint32_t generation =
        st.streamGeneration.load(std::memory_order_acquire);
    // Withdrawal stores streaming=false before advancing the generation.
    // Re-check after observing the generation so an accumulate racing between
    // those control-thread stores cannot accept a block that arrived while
    // withdrawn and expose it on a later re-admission.
    if (!opportunistic
        && !st.streaming.load(std::memory_order_acquire)) {
        return;
    }
    if (st.ringGeneration != generation) {
        // Audio-thread acknowledgment of the lifecycle invalidation. The
        // control thread changes only atomics; ring ownership stays here.
        st.rd = 0;
        st.wr = 0;
        st.avail = 0;
        st.ringGeneration = generation;
    }

    // Audio-thread write to the same atomic the UI-side setSliceMuted()
    // writes; the store is lock-free either way.
    st.muted.store(muted, std::memory_order_release);
    ensureRing(st, frames);
    if (st.capFrames <= 0) { return; }

    // Enrol as a barrier member: this slice is demonstrably delivering.
    // Opportunistic slots (the TX monitor) are mixed in but never enrol,
    // so they cannot hold up a drain. A slice the lifecycle has withdrawn
    // stays out until it is re-admitted, so a late straggler arriving
    // after withdrawal cannot silently rejoin the wait set.
    if (!opportunistic
        && st.streaming.load(std::memory_order_acquire)) {
        st.producing.store(true, std::memory_order_release);
    }

    // Drop-oldest on overflow, matching PortAudioBus's ring policy. A
    // producer that outruns the drain loses its oldest frames rather
    // than corrupting the read position.
    if (frames >= st.capFrames) {
        // Block bigger than the whole ring: keep only the newest tail.
        const int keep = st.capFrames;
        const float* tail = samples + static_cast<size_t>(frames - keep) * 2;
        std::copy(tail, tail + static_cast<size_t>(keep) * 2, st.ring.begin());
        st.rd = 0;
        st.wr = 0;
        st.avail = keep;
        return;
    }
    if (st.avail + frames > st.capFrames) {
        const int overflow = st.avail + frames - st.capFrames;
        st.rd = (st.rd + overflow) % st.capFrames;
        st.avail -= overflow;
    }
    for (int i = 0; i < frames; ++i) {
        const size_t w = static_cast<size_t>(st.wr) * 2;
        st.ring[w + 0] = samples[static_cast<size_t>(i) * 2 + 0];
        st.ring[w + 1] = samples[static_cast<size_t>(i) * 2 + 1];
        st.wr = (st.wr + 1) % st.capFrames;
    }
    st.avail += frames;
}

int MasterMixer::tryDrain(float* out, int maxFrames) {
    if (out == nullptr || maxFrames <= 0) { return 0; }
    const std::uint64_t admittedEpoch =
        m_membershipEpoch.load(std::memory_order_acquire);

    // ── Barrier ──────────────────────────────────────────────────────
    // From Thetis aamix.c:43 [v2.10.3.15]:
    //   WaitForMultipleObjects (a->nactive, a->Aready, TRUE, INFINITE);
    // Same rule, expressed as a poll because our producers are already
    // serialised on one thread and there is nothing to block on: take
    // the smallest frame count every member can satisfy.
    int  n         = std::numeric_limits<int>::max();
    int  maxAvail  = 0;
    bool anyMember = false;

    for (auto& kv : m_slices) {
        SliceState& st = kv.second;
        st.drainStaged = false;
        const bool opportunistic =
            st.opportunistic.load(std::memory_order_acquire);
        if (!opportunistic
            && !st.streaming.load(std::memory_order_acquire)) {
            continue;
        }
        if (st.ringGeneration
            != st.streamGeneration.load(std::memory_order_acquire)) {
            continue;
        }
        maxAvail = std::max(maxAvail, st.avail);
        if (!st.producing.load(std::memory_order_acquire)) { continue; }
        anyMember = true;
        n = std::min(n, st.avail);
    }

    if (!anyMember) {
        // Only opportunistic contributors (the TX monitor slot). Nothing
        // to wait for, so drain whatever is queued.
        n = maxAvail;
    }

    // A member with nothing queued is LATE, not gone, and the barrier
    // holds until it delivers. n == 0 falls through to the guard below
    // and no block leaves. Two DDCs deliver in clumps, so treating a
    // clumped-but-live slice as dead is what produced the 2026-07-27
    // G2E "scratchy with two pans" defect.
    //
    // There is deliberately no timeout here. Upstream has none either:
    // mix_main waits on every active stream with no bound, and a stream
    // leaves the mix only through an explicit state change.
    // From Thetis aamix.c:43 [v2.10.3.15]:
    //   WaitForMultipleObjects (a->nactive, a->Aready, TRUE, INFINITE);
    // The explicit leave is setSliceStreaming(), which mirrors
    // SetAAudioMixState (aamix.c:522).

    if (n <= 0) { return 0; }
    n = std::min(n, maxFrames);

#ifdef NEREUS_BUILD_TESTS
    if (m_drainAdmissionHookForTest) {
        m_drainAdmissionHookForTest();
    }
#endif

    if (m_membershipEpoch.load(std::memory_order_acquire)
        != admittedEpoch) {
        return 0;
    }

    // ── Sum ──────────────────────────────────────────────────────────
    std::fill(out, out + static_cast<size_t>(n) * 2, 0.0f);

    const float step = 1.0f / static_cast<float>(std::max(1, m_rampFrames));

    for (auto& kv : m_slices) {
        SliceState& st = kv.second;
        const bool opportunistic =
            st.opportunistic.load(std::memory_order_acquire);
        if (!opportunistic
            && !st.streaming.load(std::memory_order_acquire)) {
            continue;
        }
        if (st.ringGeneration
            != st.streamGeneration.load(std::memory_order_acquire)) {
            continue;
        }
        const int take = std::min(n, st.avail);
        if (take <= 0) { continue; }

        // Target gains. Mute is a ramp target, not a hard gate, so a
        // muted slice fades out over m_rampFrames instead of clicking.
        // Linear pan law, unchanged: at pan=0 both channels pass at
        // unity, at -1 only left, at +1 only right.
        const float g    = st.muted.load(std::memory_order_acquire)
                               ? 0.0f
                               : st.gain.load(std::memory_order_acquire);
        const float pan  = st.pan.load(std::memory_order_acquire);
        const float tgtL = g * (pan <= 0.0f ? 1.0f : 1.0f - pan);
        const float tgtR = g * (pan >= 0.0f ? 1.0f : 1.0f + pan);

        int stagedRd = st.rd;
        float stagedCurL = st.curL;
        float stagedCurR = st.curR;
        for (int i = 0; i < take; ++i) {
            stagedCurL +=
                std::clamp(tgtL - stagedCurL, -step, step);
            stagedCurR +=
                std::clamp(tgtR - stagedCurR, -step, step);
            const size_t r = static_cast<size_t>(stagedRd) * 2;
            out[static_cast<size_t>(i) * 2 + 0] +=
                st.ring[r + 0] * stagedCurL;
            out[static_cast<size_t>(i) * 2 + 1] +=
                st.ring[r + 1] * stagedCurR;
            stagedRd = (stagedRd + 1) % st.capFrames;
        }
        st.stagedRd = stagedRd;
        st.stagedAvail = st.avail - take;
        st.stagedCurL = stagedCurL;
        st.stagedCurR = stagedCurR;
        st.drainStaged = true;
    }

    // ── Master up-slew ───────────────────────────────────────────────
    // Applied to the summed output, not per slice, because that is what
    // it is protecting: the seam where the mix as a whole resumes after a
    // membership change. Thetis applies its window to a->out for the same
    // reason (upslew, aamix.c:280-284 [v2.10.3.15]).
    //
    // m_slewUpFrames is this instance's length: kSlewUpFrames for the
    // speakers mixer, 0 for the anti-VOX mixer (setSlewUpFrames() clamps
    // to just those two, since upSlewWindow() is only built for
    // kSlewUpFrames entries). Reading it as a plain int here, without the
    // mutex setSlewUpFrames() takes to write it, follows the same
    // established pattern as m_rampFrames above.
    //
    // Skipped entirely once the window has run out (or is disabled for
    // this instance), so the steady-state path costs one relaxed load and
    // a compare.
    const int slewLen = m_slewUpFrames;
    int pos = m_slewPos.load(std::memory_order_acquire);
    if (slewLen > 0 && pos < slewLen) {
        const float* w = upSlewWindow();
        for (int i = 0; i < n && pos < slewLen; ++i, ++pos) {
            const float g = w[pos];
            out[static_cast<size_t>(i) * 2 + 0] *= g;
            out[static_cast<size_t>(i) * 2 + 1] *= g;
        }
    }

    // Control-side withdrawal may race any part of barrier admission or
    // summing. Old-generation output is disposable scratch until both the
    // global membership epoch and each participating slice generation are
    // still stable. Only then advance the audio-owned cursors and gains.
    if (m_membershipEpoch.load(std::memory_order_acquire)
        != admittedEpoch) {
        return 0;
    }
    for (auto& kv : m_slices) {
        SliceState& st = kv.second;
        if (!st.drainStaged) {
            continue;
        }
        if (st.ringGeneration
            != st.streamGeneration.load(std::memory_order_acquire)) {
            return 0;
        }
        if (!st.opportunistic.load(std::memory_order_acquire)
            && !st.streaming.load(std::memory_order_acquire)) {
            return 0;
        }
    }
    for (auto& kv : m_slices) {
        SliceState& st = kv.second;
        if (!st.drainStaged) {
            continue;
        }
        st.rd = st.stagedRd;
        st.avail = st.stagedAvail;
        st.curL = st.stagedCurL;
        st.curR = st.stagedCurR;
    }
    if (slewLen > 0) {
        m_slewPos.store(pos, std::memory_order_release);
    }

    return n;
}

} // namespace NereusSDR
