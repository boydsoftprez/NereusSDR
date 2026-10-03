// =================================================================
// src/core/audio/VaxChannelMixer.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-44; see VaxChannelMixer.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/audio/VaxChannelMixer.h"

#include <algorithm>
#include <limits>

namespace NereusSDR {

VaxChannelMixer::VaxChannelMixer()
{
    for (Slot& slot : m_slots) {
        slot.ring.assign(static_cast<std::size_t>(kRingFrames) * 2, 0.0f);
    }
}

void VaxChannelMixer::setSliceStreaming(int sliceId, bool streaming)
{
    if (!handles(sliceId)) { return; }
    Slot& slot = m_slots[static_cast<std::size_t>(sliceId)];
    slot.streaming.store(streaming, std::memory_order_release);
    if (!streaming) {
        // The DSP thread acknowledges this by leaving the channel (and
        // discarding what the slice had queued) on the slice's next block;
        // until then tryDrain() no longer counts it.
        slot.generation.fetch_add(1, std::memory_order_acq_rel);
    }
}

void VaxChannelMixer::leave(Slot& slot)
{
    slot.channel = 0;
    slot.rd = 0;
    slot.wr = 0;
    slot.avail = 0;
}

bool VaxChannelMixer::isMember(const Slot& slot, int channel) const
{
    return slot.channel == channel
        && slot.streaming.load(std::memory_order_acquire)
        && slot.seenGeneration == slot.generation.load(std::memory_order_acquire);
}

void VaxChannelMixer::accumulate(int sliceId, int channel, const float* samples,
                                 int frames, float gain)
{
    if (!handles(sliceId)) { return; }
    Slot& slot = m_slots[static_cast<std::size_t>(sliceId)];

    const std::uint32_t generation = slot.generation.load(std::memory_order_acquire);
    if (generation != slot.seenGeneration) {
        slot.seenGeneration = generation;
        leave(slot);
    }
    if (!slot.streaming.load(std::memory_order_acquire)
        || channel < 1 || channel > kChannels) {
        leave(slot);
        return;
    }
    if (samples == nullptr || frames <= 0) { return; }
    if (slot.channel != channel) {
        // Joining, or moving from another channel: nothing queued for the
        // old channel belongs to this one.
        leave(slot);
        slot.channel = channel;
    }

    const int keep = std::min(frames, kRingFrames);
    const float* src = samples + static_cast<std::size_t>(frames - keep) * 2;
    if (slot.avail + keep > kRingFrames) {
        const int overflow = slot.avail + keep - kRingFrames;
        slot.rd = (slot.rd + overflow) % kRingFrames;
        slot.avail -= overflow;
    }
    for (int i = 0; i < keep; ++i) {
        const std::size_t w = static_cast<std::size_t>(slot.wr) * 2;
        slot.ring[w] = src[static_cast<std::size_t>(i) * 2] * gain;
        slot.ring[w + 1] = src[static_cast<std::size_t>(i) * 2 + 1] * gain;
        slot.wr = (slot.wr + 1) % kRingFrames;
    }
    slot.avail += keep;
}

int VaxChannelMixer::tryDrain(int channel, float* out, int maxFrames)
{
    if (out == nullptr || maxFrames <= 0 || channel < 1 || channel > kChannels) {
        return 0;
    }
    int n = std::numeric_limits<int>::max();
    bool anyMember = false;
    for (const Slot& slot : m_slots) {
        if (!isMember(slot, channel)) { continue; }
        anyMember = true;
        n = std::min(n, slot.avail);
    }
    if (!anyMember || n <= 0) { return 0; }
    n = std::min(n, maxFrames);

    std::fill(out, out + static_cast<std::size_t>(n) * 2, 0.0f);
    for (Slot& slot : m_slots) {
        if (!isMember(slot, channel)) { continue; }
        for (int i = 0; i < n; ++i) {
            const std::size_t r = static_cast<std::size_t>(slot.rd) * 2;
            out[static_cast<std::size_t>(i) * 2] += slot.ring[r];
            out[static_cast<std::size_t>(i) * 2 + 1] += slot.ring[r + 1];
            slot.rd = (slot.rd + 1) % kRingFrames;
        }
        slot.avail -= n;
    }
    return n;
}

int VaxChannelMixer::memberCount(int channel) const
{
    int count = 0;
    for (const Slot& slot : m_slots) {
        if (isMember(slot, channel)) { ++count; }
    }
    return count;
}

} // namespace NereusSDR
