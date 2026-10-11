// =================================================================
// src/core/audio/MatcherRing.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The lock-free ring the clock
// matcher (DeviceRateMatcher.cpp) writes and its device-side reader reads
// (R-AUD-15, D34).  The matching logic itself is the port; this is only the
// shared layout.
//
// One writer and one reader.  Each side owns its own counters and never
// moves the other's: the writer publishes `written`, `skipTo`, the ratio and
// the fill; the reader publishes `read`, `requested`, `readCalls`,
// `dryRuns` and `upslewPending`.  Frame counters only grow; a frame's slot
// is its counter modulo `capacityFrames` (a power of two).
//
// The header is standard layout with only always-lock-free atomics, so it
// can be built in place in shared memory (std::construct_at) and attached
// from another process.  The interleaved float frames follow the header.
//
// Modification history (NereusSDR):
//   2026-10-08: native audio plan Task 2 (R-AUD-15). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>

namespace NereusSDR {

inline constexpr std::uint32_t kMatcherRingMagic = 0x4E524D52u;   // "NRMR"
inline constexpr std::uint64_t kMatcherNoSkip = std::numeric_limits<std::uint64_t>::max();

static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<std::int64_t>::is_always_lock_free);
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(std::atomic<double>::is_always_lock_free);

struct MatcherRingHeader {
    // Fixed at construction.
    std::uint32_t magic = kMatcherRingMagic;
    std::uint32_t capacityFrames = 0;   // a power of two
    std::uint32_t channels = 2;
    std::uint32_t slewFrames = 0;       // ntslew

    // Writer side.
    std::atomic<std::uint64_t> written{0};
    std::atomic<std::uint64_t> skipTo{kMatcherNoSkip};
    std::atomic<std::uint64_t> overruns{0};
    std::atomic<std::int64_t> lastWriteNs{0};
    std::atomic<std::uint32_t> rsizeFrames{0};
    std::atomic<double> ratio{1.0};
    std::atomic<double> fillFrames{0.0};

    // Reader side.
    std::atomic<std::uint64_t> read{0};
    std::atomic<std::uint64_t> requested{0};
    std::atomic<std::uint64_t> readCalls{0};
    std::atomic<std::uint64_t> dryRuns{0};
    std::atomic<std::uint32_t> upslewPending{0};

    float* frames() { return reinterpret_cast<float*>(this + 1); }
    const float* frames() const { return reinterpret_cast<const float*>(this + 1); }
};

static_assert(std::is_standard_layout_v<MatcherRingHeader>);
static_assert(sizeof(MatcherRingHeader) % alignof(float) == 0);

inline std::size_t matcherRingBytes(std::uint32_t capacityFrames, std::uint32_t channels)
{
    return sizeof(MatcherRingHeader)
        + static_cast<std::size_t>(capacityFrames) * channels * sizeof(float);
}

// Builds the header in `memory` (aligned for MatcherRingHeader and at least
// matcherRingBytes(capacityFrames, channels) long) and zeroes the frames.
// Returns nullptr unless capacityFrames is a nonzero power of two.
inline MatcherRingHeader* constructMatcherRing(void* memory, std::uint32_t capacityFrames,
                                               std::uint32_t channels, std::uint32_t slewFrames)
{
    if (memory == nullptr || capacityFrames == 0
        || (capacityFrames & (capacityFrames - 1)) != 0 || channels == 0) {
        return nullptr;
    }
    MatcherRingHeader* ring = std::construct_at(static_cast<MatcherRingHeader*>(memory));
    ring->capacityFrames = capacityFrames;
    ring->channels = channels;
    ring->slewFrames = slewFrames;
    std::memset(ring->frames(), 0,
                static_cast<std::size_t>(capacityFrames) * channels * sizeof(float));
    return ring;
}

// Attaches to a ring another side built.  nullptr unless the magic, the
// channel count (2) and the sizes agree with `bytes`.
inline MatcherRingHeader* attachMatcherRing(void* memory, std::size_t bytes)
{
    if (memory == nullptr || bytes < sizeof(MatcherRingHeader)) {
        return nullptr;
    }
    auto* ring = static_cast<MatcherRingHeader*>(memory);
    if (ring->magic != kMatcherRingMagic || ring->channels != 2) {
        return nullptr;
    }
    const std::uint32_t capacity = ring->capacityFrames;
    if (capacity == 0 || (capacity & (capacity - 1)) != 0) {
        return nullptr;
    }
    if (matcherRingBytes(capacity, ring->channels) != bytes) {
        return nullptr;
    }
    return ring;
}

} // namespace NereusSDR
