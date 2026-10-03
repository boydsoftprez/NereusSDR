// =================================================================
// src/core/audio/MasterMixer.h  (NereusSDR)
// =================================================================
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
//   2026-07-27 -- Replaced the stall-demotion heuristic with the explicit
//                 leave that upstream uses. A member is now withdrawn
//                 only by setSliceStreaming(false) or removeSlice(), so
//                 a slice that is merely late can no longer be mistaken
//                 for one that has stopped. Fixes the ANAN-G2E bench
//                 defect where two pans produced scratchy, robotic audio.
//                 Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-07-27 -- Ported the master up-slew across a membership change.
//                 The raised-cosine window is Warren Pratt's from
//                 create_aaslew (aamix.c:86-92), armed where open_mixer
//                 raises slew.uflag (aamix.c:494-496), and its 10 ms
//                 length is the RX mixer's own tslewup from
//                 cmaster.c:297-313. Fixes the ANAN-G2E bench report of a
//                 "kerplunk at the end of the unkey": a slice re-admitted
//                 after MOX resumed at full amplitude in one sample.
//                 Down-slew is NOT ported; see divergence 4 below.
//                 Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-09-21 -- Preserve queued RADE/ordinary receiver sample pairs with
//                 the upstream 4096-frame minimum ring, independent of the
//                 small DSP block size. No prefill or barrier-policy change.
//                 Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via OpenAI Codex.
//   2026-09-23 -- R-R3-45: two mixes from one barrier. Each slice is
//                 routed to the speakers OR the headphones (VAX design
//                 6.2) and both sums leave in the same drain, from the
//                 same per-slice gain, pan and mute. A route change
//                 crossfades over the anti-click ramp. NereusSDR-original.
//                 Authored by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-25 -- iPhone app Task 76 (R-IOS-31; the several-devices
//                 design, ruling 9.2): one mix per owner from the same
//                 barrier. The local sums carry only the slices a local
//                 mask names, and each owner output sums its own slices at
//                 the same per-slice gain, pan, mute, route and up-slew.
//                 NereusSDR-original. Authored by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27 -- Remote-window parity Task 32 (R-IOS-13, R-R3-49): the
//                 transmit monitor to the device that holds transmit. An
//                 owner output may take the slots outside the slice mask
//                 (the transmit monitor) into its speakers or headphones
//                 sum, and the local sums may leave them out. NereusSDR-
//                 original. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-29 -- Slice control and shared listening plan Task 6: the AF
//                 level rides into accumulate() and scales the slice in
//                 its controller's sums, and each sum may also listen to
//                 slices it does not control, at its own level, unpanned,
//                 with a continuous hand-off between the two. Every owner
//                 output is written each drain (a slot with no slice left
//                 stale audio in its buffer before). NereusSDR-original.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
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

// Phase 3O per-slice mute / volume / pan mixer. Per-slice gain, pan,
// mute and the anti-click ramp are NereusSDR-original; the ring +
// barrier structure is derived from Thetis, see below.
//
// Phase 3F: reworked from a single shared accumulator into per-slice
// rings behind a readiness barrier, so that N slices produce ONE mixed
// block per audio period instead of N separate pushes.
//
// The structure (per-producer ring + readiness barrier + one summed
// output) follows Thetis's ChannelMaster mixer, read for reference at
// v2.10.3.15:
//
//   aamix.c:237  xMixAudio()  -- per-stream ring, memcpy in, release a
//                                readiness semaphore once a full output
//                                block is queued
//   aamix.c:43   mix_main()   -- WaitForMultipleObjects(nactive, Aready,
//                                TRUE, INFINITE) then sum, then ONE
//                                outbound push
//   aamix.c:486               -- explicit semaphore release when a stream
//                                is deactivated, so a stopped stream can
//                                not wedge the barrier
//
// Three deliberate divergences from that reference, all recorded here
// because they are design decisions rather than translation slips:
//
//  1. No dedicated mixer thread. Thetis needs one because its producers
//     run on different threads. Every NereusSDR producer is already
//     serialised on the single RxDspWorker DSP thread, so the drain runs
//     inline and skips a thread handoff.
//
//  2. No per-stream resamplers. Thetis resamples inside the mixer
//     because its streams can arrive at different rates. NereusSDR sizes
//     each stream's DSP block with bufferSizeForRate(), which scales the
//     input size linearly with the sample rate, so every stream emits one
//     64-frame 48 kHz block per period whatever its DDC rate. Cadences
//     match by construction. Decoded RADE speech returns asynchronously
//     at the same long-term rate; the rings absorb that scheduling skew.
//
//  3. Barrier membership is asymmetric: a slice JOINS implicitly on its
//     first block, but LEAVES only when the slice lifecycle withdraws it
//     through setSliceStreaming(false). That mirrors SetAAudioMixState
//     (aamix.c:522), which is the only way a stream leaves the mix
//     upstream. There is no timeout anywhere, matching mix_main's
//     unbounded WaitForMultipleObjects (aamix.c:43).
//
//     Joining implicitly is safe because it can only ever SHRINK the set
//     the drain waits on, so it cannot cause a slice to be dropped from
//     a mixed block. Leaving implicitly is not safe, and an earlier
//     revision of this file proved it: it demoted a member after two
//     consecutive empty drains. Two DDCs deliver in clumps rather than
//     alternating, so a burst from one slice repeatedly tripped that
//     threshold, and the mixer emitted blocks carrying a single receiver
//     and pushed more blocks than periods had elapsed. That is the
//     2026-07-27 ANAN-G2E bench defect (audio scratchy and robotic with
//     two pans up, clean again as soon as one was closed).
//
//     The cost of this choice is that a missed setSliceStreaming(false)
//     silences the mix until the slice is withdrawn or removed. That is
//     deliberate: a wedge is loud and immediately diagnosable, whereas
//     the timeout it replaces failed quietly, degrading audio in a way
//     that took a bench session to localise.
//
//  4. Up-slew is ported; DOWN-slew is not. Thetis brackets a membership
//     change with both: close_mixer raises slew.dflag and blocks until the
//     fade-out completes before shutting the gates (aamix.c:471-478), then
//     open_mixer fades back in. It can fade out because its mixer thread
//     keeps turning independently of its producers, so it still has output
//     blocks to shape after a stream stops feeding.
//
//     Ours cannot. The drain runs inline on the producer thread
//     (divergence 1), so when the gated slice stops delivering there is no
//     further output to apply a fade-out to: the audio simply stops
//     arriving. Porting the down-slew would mean porting the mixer thread
//     with it. The audible half that is reachable, and the half the bench
//     actually reported, is the resume.
//
// Click-free join and leave is a per-slice gain ramp rather than a port
// of Thetis's upslew/downslew state machine (aamix.c:280-...), which
// gates the MIXED output on whether data is non-zero and is built for
// VAC stream start/stop, not for one slice among several coming and
// going. Ramping the per-slice gain targets our actual artifact and
// keeps the audio-thread path branch-light.
//
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace NereusSDR {

// Audio-thread-safe per-slice mixing. UI thread mutates per-slice params;
// audio thread reads via atomics. Map structural changes are guarded by a
// mutex (held only on UI thread).
class MasterMixer {
public:
    // UI thread: per-slice gain [0..1] + pan [-1..+1]. Safe to call anytime.
    void setSliceGain(int sliceId, float gain, float pan);

    // UI thread: mute / unmute a slice. The mixer ramps to and from
    // silence rather than switching abruptly, so callers should keep
    // feeding accumulate() while muted; dropping the feed instead
    // produces the click the ramp exists to avoid.
    void setSliceMuted(int sliceId, bool muted);

    // UI thread: remove a slice (e.g. slice destroyed).
    void removeSlice(int sliceId);

    // Control thread: admit a slice to the readiness barrier, or withdraw
    // it. This is the ONLY way a slice stops being waited on, short of
    // removeSlice(); there is no timeout that will do it for you. Call it
    // with false whenever a slice stops being fed while its entry lives
    // on, e.g. when its stream is deactivated or a pan is closed.
    //
    // Ports the role of SetAAudioMixState (Thetis aamix.c:522
    // [v2.10.3.15]), which clears the stream's bit in a->active, drops it
    // from the Aready wait set and shuts its accept[] gate.
    //
    // Withdrawing invalidates what the slice already queued, and
    // re-admitting does not re-enrol it on its own: it rejoins on its
    // next delivered block, fading back in over the ramp.
    void setSliceStreaming(int sliceId, bool streaming);

    // UI thread: mark a slot as an OPPORTUNISTIC contributor, i.e. one
    // that feeds only sometimes. Opportunistic slots are mixed in
    // whenever they have audio but are never barrier members, so they
    // cannot hold up a drain.
    //
    // The TX monitor is the case this exists for: it feeds only during
    // MOX, and without the opt-out it would enrol on its first block,
    // stall the mix for kStallTolerance drains when it stopped, and do
    // that again on every transition.
    void setSliceOpportunistic(int sliceId, bool opportunistic);

    // Audio thread: queue a slice's stereo block. samples is interleaved
    // L/R float32, frames = sample pairs. Enrols the slice as a barrier
    // member. Drop-oldest if the ring overflows, matching PortAudioBus's
    // overrun policy: a stall costs a brief gap, never scrambled frames.
    // `muted` rides in as an argument because the caller is the audio
    // thread: setSliceMuted() takes the slice-map mutex and must not be
    // reached from there. Mute is applied as a ramp TARGET, so a muted
    // slice fades out over the ramp rather than cutting.
    //
    // R-R3-45: `headphones` rides in the same way and for the same reason.
    // It picks which of the two sums tryDrain() builds the slice into
    // (VAX design 6.2: speakers OR headphones). A change of route is a
    // ramp too, so the slice crossfades from one output to the other.
    //
    // Slice control plan Task 6: `level` is the slice's AF level (0..1),
    // applied here rather than in WDSP, and only in the sums of the device
    // that controls the slice (the local sums through localMask, an owner's
    // through its sliceMask). A listener's sum carries its own level
    // instead (OwnerOutput::listenLevels), so the controller's AF and mute
    // never reach it.
    void accumulate(int sliceId, const float* samples, int frames,
                    bool muted = false, bool headphones = false,
                    float level = 1.0f);


    // Audio thread: sum one block if every barrier member has frames
    // ready, and return how many frames were written to out. Returns 0
    // when the barrier is not satisfied, in which case the caller must
    // NOT push -- that is the whole point, one push per audio period
    // regardless of how many slices fed it.
    //
    // out is interleaved L/R float32 and must hold maxFrames * 2 floats.
    //
    // Only the speakers sum: slices routed to the headphones are drained
    // (their rings keep time) but contribute nothing to out.
    int tryDrain(float* out, int maxFrames);

    // R-R3-45: both sums from one barrier. speakersOut carries the slices
    // routed to the speakers, headphonesOut those routed to the headphones,
    // each at the slice's own gain, pan and mute. Either may be nullptr, in
    // which case that sum is not written. Both hold maxFrames * 2 floats;
    // the return value is the frame count written to each.
    int tryDrain(float* speakersOut, float* headphonesOut, int maxFrames);

    // iPhone app Task 76 (the several-devices design, ruling 9.2): one mix
    // per owner. Each owner names its slices by a bit per slice id (0..31)
    // and gets both sums of those slices alone, each slice at its own gain,
    // pan, mute and route, with the same ramps and up-slew as the local
    // sums. A null buffer is not written; an owner with no bit set is
    // skipped whole. Owner sums never hold up or change the local drain:
    // the barrier, the cursors and the ramps are the local drain's.
    //
    // Remote-window parity Task 32: `monitor` says where the slots outside
    // 0..31 (the transmit monitor's) go for this owner: nowhere (the
    // default), its speakers sum or its headphones sum, at the slot's own
    // gain whichever local sum the slot's route builds it into. An owner
    // with no slice bit but a monitor route is not skipped.
    enum class OwnerMonitor : std::uint8_t { None, Speakers, Headphones };
    //
    // Slice control plan Task 6 (shared listening): `listenMask` names the
    // slices this owner hears without controlling them, each into its
    // speakers sum at listenLevels[id] (32 entries, 0 when muted), ramped,
    // with no pan and no route of the controller's. A slice in both masks
    // is the controller's. `listenSlot` (0..kMaxListenSlots-1) keys the
    // ramp state this owner keeps from drain to drain; -1 keeps none, so
    // the owner cannot listen. When a slice leaves an owner's sliceMask
    // and enters its listenMask in the same drain, its listen level starts
    // where the controller's gain was and ramps to the listen level, so a
    // hand-off never steps by more than the two levels differ.
    static constexpr int kMaxListenSlots = 7;
    struct OwnerOutput {
        std::uint32_t sliceMask{0};
        float* speakers{nullptr};
        float* headphones{nullptr};
        OwnerMonitor monitor{OwnerMonitor::None};
        std::uint32_t listenMask{0};
        const float* listenLevels{nullptr};
        int listenSlot{-1};
    };
    // As the two-sum tryDrain, except that speakersOut and headphonesOut
    // carry only the slices `localMask` names (slot ids outside 0..31, the
    // transmit monitor's, while `localOutOfMask`, the default), and each of
    // `owners` (ownerCount of them, may be null when 0) gets its own sums.
    // With `onlyWithoutMembers` it drains only while no slice is a barrier
    // member (only the transmit monitor is queued), and returns 0 otherwise:
    // a member's own call drains the period, so a second drain never hands
    // the outputs two blocks in one period (Task 32, the MOX-gated slice).
    //
    // Slice control plan Task 6: `localListenMask` and `localListenLevels`
    // are the local sums' listening, as an owner's listenMask and
    // listenLevels (the hosting desktop listening to another device's
    // slice plays it here at its own level).
    int tryDrain(float* speakersOut, float* headphonesOut, int maxFrames,
                 std::uint32_t localMask, OwnerOutput* owners, int ownerCount,
                 bool localOutOfMask = true, bool onlyWithoutMembers = false,
                 std::uint32_t localListenMask = 0,
                 const float* localListenLevels = nullptr,
                 float* radioOut = nullptr);
    // Radio codec (JJ's ruling 2026-09-30): `radioOut`, when not null, is
    // the radio's own speaker out, as Thetis's audio mixer 0: every
    // receiving slice whatever localMask says, each at its own gain, pan
    // and mute, both routes summed, plus the transmit monitor's slot
    // exactly while it is in the local sums (localOutOfMask). maxFrames * 2
    // floats; same ramps and up-slew as the other sums.

    // Test seam: ramp length in frames (default kDefaultRampFrames).
    void setRampFrames(int frames);

    // Control thread: slew length for THIS instance, in frames. 0 disables
    // the fade entirely.
    //
    // The speakers mixer wants the 10 ms raised cosine (Thetis
    // cmaster.c:297-313 [v2.10.3.15], tslewup 0.010). The anti-VOX mixer
    // wants none: Thetis creates its "anti-vox mixer" with 0.000 on all
    // four slew parameters (cmaster.c:159-175 [v2.10.3.15]), because the
    // DEXP reference must be amplitude-faithful from the first sample
    // after a transition.
    //
    // upSlewWindow() builds its cosine table sized for kSlewUpFrames, so an
    // arbitrary length would index past the end of it. Only 0 (disabled)
    // and kSlewUpFrames (the default) are reachable; anything else clamps
    // to kSlewUpFrames.
    void setSlewUpFrames(int frames);

    // Test seam: how many barrier members are currently enrolled.
    int producingSliceCount() const;

#ifdef NEREUS_BUILD_TESTS
    // Deterministic race seam: runs after the readiness barrier admits a
    // drain and before any ring cursor is advanced.
    void setDrainAdmissionHookForTest(std::function<void()> hook)
    {
        m_drainAdmissionHookForTest = std::move(hook);
    }
#endif

private:
    // 5 ms at 48 kHz. Long enough to be inaudible on a join or a mute,
    // short enough that it never smears a real signal.
    static constexpr int kDefaultRampFrames = 240;

    // From Thetis cmaster.c:159-168,297-306 [v2.10.3.15]: both RX and
    // anti-VOX mixers retain 4096 frames independently of the DSP block size.
    // At 48 kHz this holds 85.3 ms of queued-producer skew without dropping
    // the ordinary receiver while RADE completes its DSP/main/DSP handoff.
    // Capacity is not a prefill target: a ready pair still drains immediately.
    static constexpr int kMinimumRingFrames = 4096;
    static constexpr int kRingBlocks = 4;

    // Master up-slew across a membership change, applied to the MIXED
    // output. Without it a slice re-admitted after a transmission resumes
    // at full amplitude in a single sample, which is the "kerplunk at the
    // end of the unkey" from the 2026-07-27 G2E bench.
    //
    // Thetis fades instead: open_mixer raises the upslew flag and blocks
    // until it finishes (aamix.c:493-505 [v2.10.3.15]), and every
    // membership change runs through close_mixer / open_mixer via
    // SetAAudioMixState (aamix.c:522).
    //
    // 10 ms, from the RX mixer's own parameters. create_aamix is called
    // with tdelayup 0.000 / tslewup 0.010 / tdelaydown 0.000 /
    // tslewdown 0.010 at cmaster.c:297-313 [v2.10.3.15]. Note the VAC
    // mixer passes 0.0 for all four (ivac.c:106), so this is specifically
    // the RX path's number, not a global default.
    //
    // Frames rather than seconds because the mixed output is always
    // 48 kHz here (kBufferBaseRate), so Thetis's
    //   ntup = (int)(tslewup * outrate)          [aamix.c:79]
    // is a compile-time constant for us: 0.010 * 48000.
    static constexpr int kSlewUpFrames = 480;

    // The raised-cosine rise, built once. Ported from create_aaslew
    // (aamix.c:86-92 [v2.10.3.15]):
    //   delta = PI / (double)a->slew.ntup;
    //   theta = 0.0;
    //   for (i = 0; i <= a->slew.ntup; i++)
    //   {
    //       a->slew.cup[i] = 0.5 * (1.0 - cos (theta));
    //       theta += delta;
    //   }
    static const float* upSlewWindow();

    // Listening ramp state per sum: [0] the local sums, [1 + listenSlot]
    // each owner's.
    static constexpr int kListenLanes = 1 + kMaxListenSlots;
    using LaneLevels = std::array<float, kListenLanes>;
    static_assert(kListenLanes == 8, "SliceState::ctlSeed lists one -1 per lane");

    struct SliceState {
        std::atomic<float> gain{1.0f};
        // Slice control plan Task 6: the AF level, written by accumulate()
        // on the audio thread; applied in the controller's sums only.
        std::atomic<float> level{1.0f};
        std::atomic<float> pan{0.0f};
        std::atomic<bool>  muted{false};
        // R-R3-45: routed to the headphones sum instead of the speakers.
        // Written by accumulate() on the audio thread.
        std::atomic<bool>  headphones{false};

        // Lifecycle generation. The control thread increments the atomic
        // when it withdraws a slice; the audio thread acknowledges that
        // intent by resetting the ring before accepting a new block.
        std::atomic<std::uint32_t> streamGeneration{0};

        // Audio thread only, past this point.
        std::uint32_t ringGeneration{0};
        std::vector<float> ring;      // interleaved stereo, capacity frames*2
        int capFrames{0};
        int rd{0};
        int wr{0};
        int avail{0};

        // Barrier membership, in two parts. `streaming` is the lifecycle's
        // intent, written on the control thread by setSliceStreaming();
        // `producing` is actual membership, set on the audio thread once a
        // streaming slice has delivered a block. Both are atomic because
        // the two threads read and write them concurrently.
        //
        // `streaming` defaults true so a slice nobody explicitly manages
        // behaves as it always did: it joins on its first block. Only the
        // LEAVE needs the lifecycle's say-so.
        std::atomic<bool> streaming{true};
        std::atomic<bool> producing{false};

        // Never a barrier member; mixed in when it happens to have
        // audio. Written on the UI thread before streaming starts.
        std::atomic<bool> opportunistic{false};

        // Ramped gains. Start at silence so a slice's first block fades
        // in instead of stepping in.
        float curL{0.0f};
        float curR{0.0f};
        // The same ramped gains into the headphones sum (R-R3-45).
        float hpCurL{0.0f};
        float hpCurR{0.0f};

        // Audio-thread-only transactional drain state. tryDrain computes
        // against these snapshots and commits them only after the control
        // thread's membership epoch is still stable.
        int stagedRd{0};
        int stagedAvail{0};
        float stagedCurL{0.0f};
        float stagedCurR{0.0f};
        float stagedHpCurL{0.0f};
        float stagedHpCurR{0.0f};
        bool drainStaged{false};

        // Slice control plan Task 6, audio thread only: each sum's ramped
        // listen level, and the controller's gain in each sum the slice
        // was controlled in last drain (-1 where it was not), the start
        // of a hand-off's ramp.
        LaneLevels listenCur{};
        LaneLevels ctlSeed{-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
        LaneLevels stagedListenCur{};
        LaneLevels stagedCtlSeed{-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
    };

    // Grow (or first-allocate) a slice's ring to hold at least the upstream
    // minimum or kRingBlocks larger blocks, whichever is greater.
    static void ensureRing(SliceState& st, int frames);


    // Map guarded by m_sliceMapMutex on structural changes. Audio-thread
    // find() is a lock-free const lookup; this is safe under the invariant
    // that slice creation/removal happens only at startup/connect, not
    // mid-stream. Parameter mutation is lock-free via atomics.
    std::unordered_map<int, SliceState> m_slices;
    mutable std::mutex m_sliceMapMutex;

    int m_rampFrames{kDefaultRampFrames};

    // Slew length for this instance, in frames. Guarded by
    // m_sliceMapMutex on writes (setSlewUpFrames); read by the audio
    // thread in tryDrain() without a lock, the same plain-member pattern
    // m_rampFrames already uses. Only 0 or kSlewUpFrames is ever stored,
    // see setSlewUpFrames(), so the audio thread never has to bounds-check
    // against anything other than those two values.
    int m_slewUpFrames{kSlewUpFrames};

    // Position in the up-slew window. Starts COMPLETE, so a mixer nobody
    // has touched behaves exactly as before and only an explicit
    // membership change arms the fade. Written by the audio thread as it
    // advances, reset to 0 by setSliceStreaming() on the control thread,
    // which is Thetis's open_mixer raising the upslew flag.
    std::atomic<int> m_slewPos{kSlewUpFrames};

    // Advanced after every streaming-state publication. A drain admitted
    // under one epoch must not commit ring cursors or return audio under
    // another.
    std::atomic<std::uint64_t> m_membershipEpoch{0};

#ifdef NEREUS_BUILD_TESTS
    std::function<void()> m_drainAdmissionHookForTest;
#endif
};

} // namespace NereusSDR
