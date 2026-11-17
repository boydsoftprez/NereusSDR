// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original lock-free ring for the recording taps.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; a single-producer, single-consumer float
//             ring the recording taps copy into (spec 6.1, threads).
//             AI tooling: Anthropic Claude Code.
#pragma once

#include <QtGlobal>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <vector>

namespace NereusSDR {

// One producer thread (a DSP or transmit thread) pushes, one consumer
// thread (the recordings worker) pops. Push and pop never lock, never wait
// and never allocate; the storage is made once in the constructor. A push
// that does not fit is refused whole, so the producer can count it and move
// on rather than block.
class SpscFloatRing {
public:
    static_assert(std::atomic<std::size_t>::is_always_lock_free,
                  "the recording taps rely on a lock-free index");

    // `capacityPow2` is rounded up to a power of two.
    explicit SpscFloatRing(int capacityPow2)
    {
        std::size_t cap = 1;
        while (cap < static_cast<std::size_t>(std::max(1, capacityPow2))) {
            cap <<= 1;
        }
        m_buffer.assign(cap, 0.0f);
        m_mask = cap - 1;
    }

    SpscFloatRing(const SpscFloatRing&) = delete;
    SpscFloatRing& operator=(const SpscFloatRing&) = delete;

    int capacity() const noexcept { return static_cast<int>(m_buffer.size()); }

    // Producer. All `count` floats, or none (false).
    bool tryPush(const float* data, int count) noexcept
    {
        if (count <= 0) {
            return true;
        }
        std::size_t head = 0;
        if (!reserve(count, &head)) {
            return false;
        }
        const std::size_t n = static_cast<std::size_t>(count);
        const std::size_t index = head & m_mask;
        const std::size_t first = std::min(n, m_buffer.size() - index);
        std::memcpy(m_buffer.data() + index, data, first * sizeof(float));
        if (first < n) {
            std::memcpy(m_buffer.data(), data + first, (n - first) * sizeof(float));
        }
        m_head.store(head + n, std::memory_order_release);
        return true;
    }

    // Producer. `count` zeros, or none (false).
    bool tryPushZeros(int count) noexcept
    {
        if (count <= 0) {
            return true;
        }
        std::size_t head = 0;
        if (!reserve(count, &head)) {
            return false;
        }
        const std::size_t n = static_cast<std::size_t>(count);
        const std::size_t index = head & m_mask;
        const std::size_t first = std::min(n, m_buffer.size() - index);
        std::memset(m_buffer.data() + index, 0, first * sizeof(float));
        if (first < n) {
            std::memset(m_buffer.data(), 0, (n - first) * sizeof(float));
        }
        m_head.store(head + n, std::memory_order_release);
        return true;
    }

    // Consumer. Up to `maxCount` floats into `out`; the number taken.
    int pop(float* out, int maxCount) noexcept
    {
        if (maxCount <= 0) {
            return 0;
        }
        const std::size_t tail = m_tail.load(std::memory_order_relaxed);
        const std::size_t head = m_head.load(std::memory_order_acquire);
        const std::size_t n = std::min(head - tail, static_cast<std::size_t>(maxCount));
        if (n == 0) {
            return 0;
        }
        const std::size_t index = tail & m_mask;
        const std::size_t first = std::min(n, m_buffer.size() - index);
        std::memcpy(out, m_buffer.data() + index, first * sizeof(float));
        if (first < n) {
            std::memcpy(out + first, m_buffer.data(), (n - first) * sizeof(float));
        }
        m_tail.store(tail + n, std::memory_order_release);
        return static_cast<int>(n);
    }

    // Either side: the floats waiting.
    int available() const noexcept
    {
        return static_cast<int>(m_head.load(std::memory_order_acquire)
                                - m_tail.load(std::memory_order_acquire));
    }

private:
    bool reserve(int count, std::size_t* head) const noexcept
    {
        const std::size_t h = m_head.load(std::memory_order_relaxed);
        const std::size_t t = m_tail.load(std::memory_order_acquire);
        if (m_buffer.size() - (h - t) < static_cast<std::size_t>(count)) {
            return false;
        }
        *head = h;
        return true;
    }

    std::vector<float> m_buffer;
    std::size_t m_mask = 0;
    alignas(64) std::atomic<std::size_t> m_head{0};   // written by the producer
    alignas(64) std::atomic<std::size_t> m_tail{0};   // written by the consumer
};

}  // namespace NereusSDR
