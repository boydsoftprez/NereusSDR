#pragma once
// =================================================================
// src/core/audio/TxMicWakeWatch.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. TX diagnostics lane.
//
// The longest gap between the TX pump's wakes during a key, when it
// started, and how far the radio's microphone frame sequence number moved
// across it. Measurement only: nothing here changes when the pump wakes.
//
// Threads:
//   - noteSequence: the connection's receive thread, per microphone frame.
//   - noteWake: the TX pump (TxMicSource::waitForBlock returning a block).
//   - begin / end: the connection thread, at key and unkey.
//   - stats: any thread.
// Lock-free and allocation-free: relaxed atomics, and a generation counter
// so the pump resets its own state at a new key.
//
// Modification history (NereusSDR):
//   2026-10-01: TX diagnostics lane, the wake gap and the sequence step
//               across it. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//
// =================================================================

#include <QtGlobal>

#include <atomic>

namespace NereusSDR {

class TxMicWakeWatch {
public:
    struct Stats {
        /// The longest gap between two wakes during the key, in ns; -1
        /// when fewer than two wakes came.
        qint64 longestGapNs{-1};
        /// When that gap started (the earlier wake), steady clock ns.
        qint64 gapStartSteadyNs{-1};
        /// How many microphone frame sequence numbers the radio moved
        /// across that gap (1: one frame, the next in order); -1 when no
        /// sequence number was seen on both sides.
        qint64 sequenceStep{-1};
    };

    /// A new key: clears the figures and starts recording. The pump's
    /// first wake after it only sets the baseline.
    void begin()
    {
        m_longestGapNs.store(-1, std::memory_order_relaxed);
        m_gapStartNs.store(-1, std::memory_order_relaxed);
        m_sequenceStep.store(-1, std::memory_order_relaxed);
        m_armed.store(true, std::memory_order_relaxed);
        m_generation.fetch_add(1, std::memory_order_acq_rel);
    }

    /// Unkey: stops recording; the figures stay for the unkey line.
    void end() { m_armed.store(false, std::memory_order_relaxed); }

    /// The latest microphone frame's sequence number.
    void noteSequence(quint32 sequence)
    {
        m_sequence.store(sequence, std::memory_order_relaxed);
        m_haveSequence.store(true, std::memory_order_release);
    }

    /// The pump woke with a block, at `nowSteadyNs`.
    void noteWake(qint64 nowSteadyNs)
    {
        const quint32 generation = m_generation.load(std::memory_order_acquire);
        if (generation != m_seenGeneration) {
            m_seenGeneration = generation;
            m_lastWakeNs = -1;
        }
        if (!m_armed.load(std::memory_order_relaxed)) {
            m_lastWakeNs = -1;
            return;
        }
        const bool haveSequence = m_haveSequence.load(std::memory_order_acquire);
        const quint32 sequence = m_sequence.load(std::memory_order_relaxed);
        if (m_lastWakeNs >= 0) {
            const qint64 gap = nowSteadyNs - m_lastWakeNs;
            if (gap > m_longestGapNs.load(std::memory_order_relaxed)) {
                m_longestGapNs.store(gap, std::memory_order_relaxed);
                m_gapStartNs.store(m_lastWakeNs, std::memory_order_relaxed);
                m_sequenceStep.store(haveSequence && m_lastHadSequence
                                         ? static_cast<qint64>(
                                               static_cast<quint32>(sequence - m_lastSequence))
                                         : -1,
                                     std::memory_order_relaxed);
            }
        }
        m_lastWakeNs = nowSteadyNs;
        m_lastSequence = sequence;
        m_lastHadSequence = haveSequence;
    }

    Stats stats() const
    {
        Stats out;
        out.longestGapNs = m_longestGapNs.load(std::memory_order_relaxed);
        out.gapStartSteadyNs = m_gapStartNs.load(std::memory_order_relaxed);
        out.sequenceStep = m_sequenceStep.load(std::memory_order_relaxed);
        return out;
    }

private:
    std::atomic<quint32> m_generation{0};
    std::atomic<bool> m_armed{false};
    std::atomic<quint32> m_sequence{0};
    std::atomic<bool> m_haveSequence{false};
    std::atomic<qint64> m_longestGapNs{-1};
    std::atomic<qint64> m_gapStartNs{-1};
    std::atomic<qint64> m_sequenceStep{-1};

    // The pump's own state (noteWake only).
    quint32 m_seenGeneration{0};
    qint64 m_lastWakeNs{-1};
    quint32 m_lastSequence{0};
    bool m_lastHadSequence{false};
};

} // namespace NereusSDR
