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
//   Project Files/Source/ChannelMaster/cmaster.c [v2.10.3.15]
//     (RX and anti-VOX minimum ring capacity)
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
//   2026-09-21 -- Preserve queued RADE/ordinary receiver sample pairs with
//                 the upstream 4096-frame minimum ring, independent of the
//                 small DSP block size. No prefill or barrier-policy change.
//                 Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via OpenAI Codex.
//   2026-09-23 -- R-R3-45: speakers and headphones sums from one drain.
//                 NereusSDR-original. Authored by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25 -- iPhone app Task 76 (R-IOS-31): one mix per owner from
//                 the same drain, and a local mask for the local sums.
//                 NereusSDR-original. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-27 -- Remote-window parity Task 32 (R-IOS-13, R-R3-49): an
//                 owner's monitor route takes the transmit monitor into its
//                 speakers or headphones sum; the local sums may leave it
//                 out. NereusSDR-original. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29 -- Slice control plan Task 6: the AF level scales a slice
//                 in its controller's sums only, and every sum may listen
//                 to other slices at its own level with a continuous
//                 hand-off. NereusSDR-original. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30 -- Radio codec (JJ's ruling): tryDrain's radioOut, the
//                 radio's own speaker out, every receiving slice as
//                 Thetis's mixer 0. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
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


// --- From cmaster.c ---
/*  cmaster.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014-2019 Warren Pratt, NR0V

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
#include <array>
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
    const int want = std::max(kMinimumRingFrames, frames * kRingBlocks);
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
                             bool muted, bool headphones, float level) {
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
    st.headphones.store(headphones, std::memory_order_release);
    st.level.store(std::clamp(level, 0.0f, 1.0f), std::memory_order_release);
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
    if (out == nullptr) { return 0; }
    return tryDrain(out, nullptr, maxFrames);
}

int MasterMixer::tryDrain(float* out, float* hpOut, int maxFrames) {
    return tryDrain(out, hpOut, maxFrames, 0xFFFFFFFFu, nullptr, 0);
}

int MasterMixer::tryDrain(float* out, float* hpOut, int maxFrames,
                          std::uint32_t localMask, OwnerOutput* owners, int ownerCount,
                          bool localOutOfMask, bool onlyWithoutMembers,
                          std::uint32_t localListenMask, const float* localListenLevels,
                          float* radioOut) {
    // `out` is the speakers sum, `hpOut` the headphones sum (R-R3-45).
    if ((out == nullptr && hpOut == nullptr) || maxFrames <= 0) { return 0; }
    if (owners == nullptr) { ownerCount = 0; }
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
    } else if (onlyWithoutMembers) {
        // Task 32: a member's own call drains this period.
        return 0;
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
    if (out != nullptr) {
        std::fill(out, out + static_cast<size_t>(n) * 2, 0.0f);
    }
    if (hpOut != nullptr) {
        std::fill(hpOut, hpOut + static_cast<size_t>(n) * 2, 0.0f);
    }
    if (radioOut != nullptr) {
        std::fill(radioOut, radioOut + static_cast<size_t>(n) * 2, 0.0f);
    }
    // Task 76: each owner's sums start silent too. Slice control plan
    // Task 6: every owner's, since a listening owner may control nothing
    // (a skipped one handed its tap whatever the buffer last held).
    for (int k = 0; k < ownerCount; ++k) {
        OwnerOutput& owner = owners[k];
        if (owner.speakers != nullptr) {
            std::fill(owner.speakers, owner.speakers + static_cast<size_t>(n) * 2, 0.0f);
        }
        if (owner.headphones != nullptr) {
            std::fill(owner.headphones, owner.headphones + static_cast<size_t>(n) * 2, 0.0f);
        }
    }

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
        // Task 76: which sums this slice reaches. Slot ids outside 0..31
        // (the transmit monitor's) are local only.
        const int id = kv.first;
        const bool inMask = id >= 0 && id < 32;
        const std::uint32_t bit = inMask ? (std::uint32_t{1} << id) : 0u;
        // Task 32: the transmit monitor's slot plays locally unless the
        // caller leaves it out (a remote device holds transmit).
        const bool local = inMask ? (localMask & bit) != 0 : localOutOfMask;
        float* const sliceOut = local ? out : nullptr;
        float* const sliceHpOut = local ? hpOut : nullptr;
        // Radio codec (JJ's ruling 2026-09-30): the radio's speaker out
        // takes every receiving slice, as Thetis's mixer 0 takes RX1, RX1S
        // and RX2 whoever listens (console.cs:27650-27664 [v2.10.3.15]),
        // and the monitor slot (MON, the same mixer, audio.cs:417-418) as
        // the local sums take it.
        float* const sliceRadioOut = (inMask || localOutOfMask) ? radioOut : nullptr;

        // Target gains. Mute is a ramp target, not a hard gate, so a
        // muted slice fades out over m_rampFrames instead of clicking.
        // Linear pan law, unchanged: at pan=0 both channels pass at
        // unity, at -1 only left, at +1 only right.
        //
        // Slice control plan Task 6: the AF level too, the controller's
        // own (JJ's ruling: AF is applied here, not in WDSP).
        const float g    = st.muted.load(std::memory_order_acquire)
                               ? 0.0f
                               : st.gain.load(std::memory_order_acquire)
                                     * st.level.load(std::memory_order_acquire);
        const float pan  = st.pan.load(std::memory_order_acquire);
        const float tgtL = g * (pan <= 0.0f ? 1.0f : 1.0f - pan);
        const float tgtR = g * (pan >= 0.0f ? 1.0f : 1.0f + pan);

        // R-R3-45: the route picks which sum gets the targets; the other
        // ramps to silence, so a route change crossfades over the ramp
        // instead of stepping from one output to the other.
        const bool toHeadphones =
            st.headphones.load(std::memory_order_acquire);
        const float spkTgtL = toHeadphones ? 0.0f : tgtL;
        const float spkTgtR = toHeadphones ? 0.0f : tgtR;
        const float hpTgtL  = toHeadphones ? tgtL : 0.0f;
        const float hpTgtR  = toHeadphones ? tgtR : 0.0f;

        // Slice control plan Task 6: the sums listening to this slice. A
        // sum the slice's controller reaches takes the controller's part
        // above and no listen part; any other sum ramps its listen level
        // toward the level it asked for (0 when it stopped or is muted),
        // unpanned, into its speakers. A sum whose control just passed to
        // listening starts from the controller's gain there, so the
        // hand-off is continuous.
        struct ListenLane {
            float* dest;
            float cur;
            float target;
            int lane;
        };
        std::array<ListenLane, kListenLanes> lanes{};
        int laneCount = 0;
        std::array<bool, kListenLanes> controlled{};
        st.stagedListenCur = st.listenCur;
        st.stagedCtlSeed = st.ctlSeed;
        const auto planLane = [&](int lane, bool ctl, bool listen, float wanted, float* dest) {
            if (ctl) {
                controlled[static_cast<size_t>(lane)] = true;
                st.stagedListenCur[static_cast<size_t>(lane)] = 0.0f;
                return;
            }
            float start = st.listenCur[static_cast<size_t>(lane)];
            if (listen && st.ctlSeed[static_cast<size_t>(lane)] >= 0.0f) {
                start = st.ctlSeed[static_cast<size_t>(lane)];
            }
            st.stagedCtlSeed[static_cast<size_t>(lane)] = -1.0f;
            const float target = listen ? std::clamp(wanted, 0.0f, 1.0f) : 0.0f;
            if (start <= 0.0f && target <= 0.0f) {
                st.stagedListenCur[static_cast<size_t>(lane)] = 0.0f;
                return;
            }
            lanes[static_cast<size_t>(laneCount++)] = ListenLane{dest, start, target, lane};
        };
        if (inMask) {
            planLane(0, local, (localListenMask & bit) != 0,
                     localListenLevels != nullptr ? localListenLevels[id] : 0.0f, out);
            for (int k = 0; k < ownerCount; ++k) {
                const OwnerOutput& owner = owners[k];
                if (owner.listenSlot < 0 || owner.listenSlot >= kMaxListenSlots) {
                    continue;
                }
                planLane(1 + owner.listenSlot, (owner.sliceMask & bit) != 0,
                         (owner.listenMask & bit) != 0,
                         owner.listenLevels != nullptr ? owner.listenLevels[id] : 0.0f,
                         owner.speakers);
            }
        }

        int stagedRd = st.rd;
        float stagedCurL = st.curL;
        float stagedCurR = st.curR;
        float stagedHpCurL = st.hpCurL;
        float stagedHpCurR = st.hpCurR;
        for (int i = 0; i < take; ++i) {
            stagedCurL +=
                std::clamp(spkTgtL - stagedCurL, -step, step);
            stagedCurR +=
                std::clamp(spkTgtR - stagedCurR, -step, step);
            stagedHpCurL +=
                std::clamp(hpTgtL - stagedHpCurL, -step, step);
            stagedHpCurR +=
                std::clamp(hpTgtR - stagedHpCurR, -step, step);
            const size_t r = static_cast<size_t>(stagedRd) * 2;
            const size_t o = static_cast<size_t>(i) * 2;
            const float spkL = st.ring[r + 0] * stagedCurL;
            const float spkR = st.ring[r + 1] * stagedCurR;
            const float hpL = st.ring[r + 0] * stagedHpCurL;
            const float hpR = st.ring[r + 1] * stagedHpCurR;
            if (sliceOut != nullptr) {
                sliceOut[o + 0] += spkL;
                sliceOut[o + 1] += spkR;
            }
            if (sliceHpOut != nullptr) {
                sliceHpOut[o + 0] += hpL;
                sliceHpOut[o + 1] += hpR;
            }
            if (sliceRadioOut != nullptr) {
                sliceRadioOut[o + 0] += spkL + hpL;
                sliceRadioOut[o + 1] += spkR + hpR;
            }
            for (int k = 0; k < ownerCount; ++k) {
                OwnerOutput& owner = owners[k];
                if (!inMask) {
                    // Task 32: the transmit monitor, to the owner's chosen
                    // sum at the slot's own gain. The slot is in exactly one
                    // local sum (a route change crossfades), so the two
                    // together are that gain whichever it is.
                    float* const to = owner.monitor == OwnerMonitor::Speakers
                        ? owner.speakers
                        : owner.monitor == OwnerMonitor::Headphones ? owner.headphones
                                                                     : nullptr;
                    if (to != nullptr) {
                        to[o + 0] += spkL + hpL;
                        to[o + 1] += spkR + hpR;
                    }
                    continue;
                }
                if ((owner.sliceMask & bit) == 0) { continue; }
                if (owner.speakers != nullptr) {
                    owner.speakers[o + 0] += spkL;
                    owner.speakers[o + 1] += spkR;
                }
                if (owner.headphones != nullptr) {
                    owner.headphones[o + 0] += hpL;
                    owner.headphones[o + 1] += hpR;
                }
            }
            for (int l = 0; l < laneCount; ++l) {
                ListenLane& lane = lanes[static_cast<size_t>(l)];
                lane.cur += std::clamp(lane.target - lane.cur, -step, step);
                if (lane.dest != nullptr) {
                    lane.dest[o + 0] += st.ring[r + 0] * lane.cur;
                    lane.dest[o + 1] += st.ring[r + 1] * lane.cur;
                }
            }
            stagedRd = (stagedRd + 1) % st.capFrames;
        }
        for (int l = 0; l < laneCount; ++l) {
            const ListenLane& lane = lanes[static_cast<size_t>(l)];
            st.stagedListenCur[static_cast<size_t>(lane.lane)] = lane.cur;
        }
        const float controllerGain =
            std::max({stagedCurL, stagedCurR, stagedHpCurL, stagedHpCurR});
        for (int lane = 0; lane < kListenLanes; ++lane) {
            if (controlled[static_cast<size_t>(lane)]) {
                st.stagedCtlSeed[static_cast<size_t>(lane)] = controllerGain;
            }
        }
        st.stagedRd = stagedRd;
        st.stagedAvail = st.avail - take;
        st.stagedCurL = stagedCurL;
        st.stagedCurR = stagedCurR;
        st.stagedHpCurL = stagedHpCurL;
        st.stagedHpCurR = stagedHpCurR;
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
            const size_t o = static_cast<size_t>(i) * 2;
            if (out != nullptr) {
                out[o + 0] *= g;
                out[o + 1] *= g;
            }
            // The headphones sum resumes with the speakers (R-R3-45).
            if (hpOut != nullptr) {
                hpOut[o + 0] *= g;
                hpOut[o + 1] *= g;
            }
            if (radioOut != nullptr) {
                radioOut[o + 0] *= g;
                radioOut[o + 1] *= g;
            }
            // And every owner's sums with them (Task 76).
            for (int k = 0; k < ownerCount; ++k) {
                OwnerOutput& owner = owners[k];
                if (owner.speakers != nullptr) {
                    owner.speakers[o + 0] *= g;
                    owner.speakers[o + 1] *= g;
                }
                if (owner.headphones != nullptr) {
                    owner.headphones[o + 0] *= g;
                    owner.headphones[o + 1] *= g;
                }
            }
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
        st.hpCurL = st.stagedHpCurL;
        st.hpCurR = st.stagedHpCurR;
        st.listenCur = st.stagedListenCur;
        st.ctlSeed = st.stagedCtlSeed;
    }
    if (slewLen > 0) {
        m_slewPos.store(pos, std::memory_order_release);
    }

    return n;
}

} // namespace NereusSDR
