// =================================================================
// src/core/audio/AudioRingSpsc.h  (NereusSDR)
// =================================================================
//   Copyright (C) 2026 J.J. Boyd (KG4VCF) - GPLv2-or-later.
//   2026-04-23 - created. AI-assisted via Anthropic Claude Code.
// =================================================================
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>
#include <QtGlobal>

namespace NereusSDR {

// Single-producer single-consumer lock-free byte ring.
// Capacity must be a power of two. One byte of the buffer is always
// reserved to distinguish full-vs-empty, so usable capacity is
// Capacity - 1.
//
// Release/acquire ordering: producer writes m_wr with release after
// copying bytes in; consumer reads m_wr with acquire before reading
// those bytes. Mirror for the return trip.
//
// Back-pressure: pushCopy does NOT evict from the ring in the
// producer thread — doing so would race the consumer's m_rd writes
// and corrupt the queue (the deprecated in-producer dropOldest pattern
// fails the 10^6-byte concurrent test in tst_audio_ring_spsc.cpp).
// Instead, pushCopy yields until the consumer has freed enough space.
// Callers are therefore responsible for sizing the ring with enough
// headroom that the yield loop is only ever hit under pathological
// scheduling (e.g. PipeWireStream sizes the ring for ~170 ms of
// 48 kHz stereo F32, roughly 8x a typical graph period). The single-
// shot oversize path (src > kCapacity-1 bytes) DOES drop oldest by
// advancing src — safe because it only mutates the local pointer.
template <size_t Capacity>
class AudioRingSpsc {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");
    static_assert(Capacity >= 16, "Capacity too small to be useful");

public:
    static constexpr size_t kCapacity = Capacity;
    static constexpr size_t kMask     = Capacity - 1;

    // "drop-oldest-on-overflow" has two flavours:
    //   (a) Single-shot: caller asks to push more than the ring's
    //       usable capacity (Capacity - 1) in ONE call. We advance src
    //       so only the tail fits, then write it. This path keeps us
    //       strictly single-writer (only m_wr is touched).
    //   (b) Steady-state back-pressure: when the ring is temporarily
    //       full but the caller has a normal-sized payload, we cannot
    //       evict from the ring atomically without racing the consumer
    //       on m_rd. Instead we *wait* for the consumer to free enough
    //       space. This is still lock-free under the SPSC definition —
    //       we never take a mutex — and it preserves the invariant
    //       that a successfully-accepted byte is delivered in order.
    // Non-blocking variant for RT producers that MUST NOT yield.
    // Writes whatever fits in the current free space; returns the byte
    // count actually written (0 if the ring was full). Caller is
    // responsible for tracking the dropped portion (overflow counter).
    //
    // Required for PipeWire data-thread producers (onProcessInput): the
    // PipeWire daemon SIGKILLs clients whose data thread blocks beyond
    // the per-quantum budget, so the regular pushCopy yield-loop is
    // fatal there. Use this for any producer running on a thread that
    // a real-time daemon polices.
    qint64 tryPushCopy(const uint8_t* src, qint64 bytes) {
        if (bytes <= 0) { return 0; }
        size_t want = size_t(bytes);
        if (want > kCapacity - 1) {
            src  += (want - (kCapacity - 1));
            want  = kCapacity - 1;
        }
        const size_t wr = m_wr.load(std::memory_order_relaxed);
        const size_t rd = m_rd.load(std::memory_order_acquire);
        const size_t free = (kCapacity - 1) - (wr - rd);
        if (free == 0) { return 0; }
        if (want > free) { want = free; }   // partial write
        const size_t writeIdx = wr & kMask;
        const size_t firstChunk =
            (kCapacity - writeIdx < want) ? kCapacity - writeIdx : want;
        std::memcpy(&m_buf[writeIdx], src, firstChunk);
        if (firstChunk < want) {
            std::memcpy(&m_buf[0], src + firstChunk, want - firstChunk);
        }
        m_wr.store(wr + want, std::memory_order_release);
        return qint64(want);
    }

    qint64 pushCopy(const uint8_t* src, qint64 bytes) {
        if (bytes <= 0) { return 0; }
        size_t want = size_t(bytes);

        // Single-shot input larger than the ring's usable capacity:
        // advance src so only the last kCapacity-1 bytes get written.
        if (want > kCapacity - 1) {
            src  += (want - (kCapacity - 1));
            want  = kCapacity - 1;
        }

        const size_t wr = m_wr.load(std::memory_order_relaxed);
        size_t rd = m_rd.load(std::memory_order_acquire);
        size_t free = (kCapacity - 1) - (wr - rd);

        // Back-pressure: yield until the consumer has freed enough
        // space for the full payload. Audio producers generate data at
        // a steady rate (48 kHz etc.), so this loop only runs if the
        // consumer is temporarily preempted. In practice the ring is
        // sized for ~170 ms headroom and this loop almost never spins.
        while (want > free) {
            std::this_thread::yield();
            rd = m_rd.load(std::memory_order_acquire);
            free = (kCapacity - 1) - (wr - rd);
        }

        const size_t writeIdx = wr & kMask;
        const size_t firstChunk =
            (kCapacity - writeIdx < want) ? kCapacity - writeIdx : want;
        std::memcpy(&m_buf[writeIdx], src, firstChunk);
        if (firstChunk < want) {
            std::memcpy(&m_buf[0], src + firstChunk, want - firstChunk);
        }
        m_wr.store(wr + want, std::memory_order_release);
        return qint64(want);
    }

    qint64 popInto(uint8_t* dst, qint64 maxBytes) {
        if (maxBytes <= 0) { return 0; }
        const size_t rd = m_rd.load(std::memory_order_relaxed);
        const size_t wr = m_wr.load(std::memory_order_acquire);
        // Counters are monotonic — see pushCopy for the full rationale
        // on why we must not mask (wr - rd) when computing used.
        const size_t used = wr - rd;
        const size_t toCopy =
            (size_t(maxBytes) > used) ? used : size_t(maxBytes);
        if (toCopy == 0) { return 0; }
        const size_t readIdx = rd & kMask;
        const size_t firstChunk =
            (kCapacity - readIdx < toCopy) ? kCapacity - readIdx : toCopy;
        std::memcpy(dst, &m_buf[readIdx], firstChunk);
        if (firstChunk < toCopy) {
            std::memcpy(dst + firstChunk, &m_buf[0], toCopy - firstChunk);
        }
        m_rd.store(rd + toCopy, std::memory_order_release);
        return qint64(toCopy);
    }

    // Consumer-side helper: drop the oldest `bytes` from the ring by
    // advancing m_rd. Must only be called from the consumer thread
    // (m_rd is consumer-owned under the SPSC contract — see pushCopy
    // for the reason we never touch it from the producer side).
    // No-op when ring is empty.
    void dropOldest(size_t bytes) {
        const size_t rd = m_rd.load(std::memory_order_relaxed);
        const size_t wr = m_wr.load(std::memory_order_acquire);
        const size_t used = wr - rd;
        const size_t toDrop = (bytes > used) ? used : bytes;
        if (toDrop == 0) { return; }
        m_rd.store(rd + toDrop, std::memory_order_release);
    }

    size_t usedBytes() const {
        const size_t wr = m_wr.load(std::memory_order_acquire);
        const size_t rd = m_rd.load(std::memory_order_acquire);
        return wr - rd;
    }

    static constexpr size_t capacity() { return kCapacity; }

private:
    alignas(64) std::atomic<size_t> m_wr{0};
    alignas(64) std::atomic<size_t> m_rd{0};
    std::array<uint8_t, kCapacity> m_buf{};
};

}  // namespace NereusSDR
