// no-port-check: NereusSDR-original bounded, single-producer raw-I/Q ingress.
// Modification history (NereusSDR): 2026-09-27 J.J. Boyd (KG4VCF),
// AI-assisted implementation via OpenAI Codex.
#pragma once

#include <QVector>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <optional>

namespace NereusSDR {

// One DirectConnection callback is the producer; the owner-thread timer is
// the consumer. Raw callback sizes vary, so the ring stores pairs and emits
// only full 1024-pair frames. This makes the negotiated byte charge exact
// over each complete second at supported hardware rates. An in-flight
// callback owns the retired state and sees stop atomically.
class RemoteIqIngress final {
public:
    static constexpr unsigned kPairsPerFrame = 1024;
    static constexpr unsigned kFramesCapacity = 16;
    static constexpr unsigned kPairsCapacity = kPairsPerFrame * kFramesCapacity;
    static constexpr unsigned kStorageBytes = kPairsCapacity * 2 * sizeof(float);

    bool push(const QVector<float>& samples) noexcept
    {
        if (stopped.load(std::memory_order_acquire)
            || failed.load(std::memory_order_acquire)) { return false; }
        if (samples.isEmpty() || (samples.size() & 1) != 0) {
            failed.store(true, std::memory_order_release);
            return false;
        }
        const auto pairs = static_cast<unsigned>(samples.size() / 2);
        const auto head = m_head.load(std::memory_order_relaxed);
        const auto tail = m_tail.load(std::memory_order_acquire);
        if (pairs > kPairsCapacity || head - tail > kPairsCapacity - pairs) {
            failed.store(true, std::memory_order_release);
            return false;
        }
        const unsigned offset = head % kPairsCapacity;
        const unsigned first = std::min(pairs, kPairsCapacity - offset);
        std::memcpy(m_samples.data() + 2 * offset, samples.constData(),
                    2 * first * sizeof(float));
        if (first != pairs) {
            std::memcpy(m_samples.data(), samples.constData() + 2 * first,
                        2 * (pairs - first) * sizeof(float));
        }
        m_head.store(head + pairs, std::memory_order_release);
        return true;
    }

    std::optional<QVector<float>> pop()
    {
        const auto tail = m_tail.load(std::memory_order_relaxed);
        const auto head = m_head.load(std::memory_order_acquire);
        if (head - tail < kPairsPerFrame) { return std::nullopt; }
        QVector<float> result(2 * kPairsPerFrame);
        const unsigned offset = tail % kPairsCapacity;
        const unsigned first = std::min(kPairsPerFrame, kPairsCapacity - offset);
        std::memcpy(result.data(), m_samples.data() + 2 * offset,
                    2 * first * sizeof(float));
        if (first != kPairsPerFrame) {
            std::memcpy(result.data() + 2 * first, m_samples.data(),
                        2 * (kPairsPerFrame - first) * sizeof(float));
        }
        m_tail.store(tail + kPairsPerFrame, std::memory_order_release);
        return result;
    }

    std::atomic<bool> stopped{false};
    std::atomic<bool> failed{false};

private:
    std::array<float, 2 * kPairsCapacity> m_samples {};
    std::atomic<unsigned> m_head{0};
    std::atomic<unsigned> m_tail{0};
};

} // namespace NereusSDR
