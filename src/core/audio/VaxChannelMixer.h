// =================================================================
// src/core/audio/VaxChannelMixer.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-44 (R3 receiver audio plan, fix
// wave): the local VAX tee's per-channel mix. It follows the readiness
// barrier shape MasterMixer already uses for the speakers (one summed block
// per audio period, whatever the number of slices feeding it); no upstream
// logic is translated here.
//
// Several slices may share one VAX channel. Each used to push its own block
// into the channel's device ring, so a channel carrying two slices received
// two periods of audio per real period: the ring overran and the app reading
// the channel heard garbled audio. Now each slice's block (already scaled by
// the channel gain and the slice's own 1 / AF gain) is queued on the slice's
// ring, and the channel's block leaves once every slice on the channel has
// delivered: their sum, pushed once.
//
// Membership:
//   - a slice joins a channel with its first block for that channel;
//   - it leaves when its block names another channel or none (the audio
//     thread sees the slice's VAX channel change on its next block), or
//     when the slice lifecycle withdraws it (setSliceStreaming(false), the
//     same call that withdraws it from the speakers mix: a closed pan, the
//     slice keyed for transmit). A withdrawn slice rejoins with its next
//     block after it is re-admitted.
//   - like the speakers mix there is no timeout: a slice that is late is
//     waited for, never dropped.
//
// Threads: accumulate() and tryDrain() run on the DSP thread only (every
// slice's rxBlockReady is serialised there), so the rings and membership
// need no lock. setSliceStreaming() runs on the control thread and touches
// atomics only. Nothing allocates after construction.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace NereusSDR {

class VaxChannelMixer {
public:
    static constexpr int kChannels = 4;
    // Slice ids 0..kMaxSlices-1. The slice pool never exceeds
    // WdspEngine::kMaxSliceChannels (AudioEngine.cpp checks this).
    static constexpr int kMaxSlices = 8;
    // Per-slice queue: 4096 frames (85 ms at 48 kHz), the speakers mix's
    // own minimum ring (MasterMixer::kMinimumRingFrames).
    static constexpr int kRingFrames = 4096;

    VaxChannelMixer();

    static bool handles(int sliceId) { return sliceId >= 0 && sliceId < kMaxSlices; }

    // Control thread: withdraw a slice from its channel's mix, or re-admit
    // it (it rejoins with its next block). Mirrors
    // AudioEngine::setSliceStreaming.
    void setSliceStreaming(int sliceId, bool streaming);

    // DSP thread: queue a slice's interleaved stereo block for `channel`
    // (1..kChannels), scaled by `gain`. Any other channel value takes the
    // slice out of whatever channel it was on. Drop-oldest on overflow.
    void accumulate(int sliceId, int channel, const float* samples, int frames,
                    float gain);

    // DSP thread: the channel's summed block once every slice on it has
    // frames queued; returns the frames written to `out` (interleaved
    // stereo, room for maxFrames), 0 when a slice on it is still to deliver.
    int tryDrain(int channel, float* out, int maxFrames);

    // DSP thread (or a test driving it): the slices currently on `channel`.
    int memberCount(int channel) const;

private:
    struct Slot {
        std::atomic<bool> streaming{true};
        std::atomic<std::uint32_t> generation{0};

        // DSP thread only.
        std::uint32_t seenGeneration = 0;
        int channel = 0;  // 1..kChannels while on a channel
        std::vector<float> ring;
        int rd = 0;
        int wr = 0;
        int avail = 0;
    };

    bool isMember(const Slot& slot, int channel) const;
    static void leave(Slot& slot);

    std::array<Slot, kMaxSlices> m_slots;
};

} // namespace NereusSDR
