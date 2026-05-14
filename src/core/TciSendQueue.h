// no-port-check: NereusSDR-original priority send queue. Three internal
// FIFO queues (Urgent / Binary / Control) drained in priority order with
// bounded-depth oldest-drop on each. Pattern condensed from Thetis's
// 3-queue model at TCIServer.cs:769-774 [v2.10.3.13] (queue declarations)
// and TCIServer.cs:1645-1679 [v2.10.3.13] (tryDequeueNextOutboundFrameLocked
// drain order: Urgent, Binary, Control, then coalesced). The Thetis queues
// are unbounded; the bounded-depth oldest-drop semantics are a NereusSDR-
// original safety addition to prevent unbounded memory growth under
// backpressure. The Thetis coalesced-key map (m_outboundCoalescedOrder /
// m_outboundCoalescedFrames) is implemented separately in Phase 15
// (TciVfoCoalescer); Phase 14 handles only the three priority queues.
//
// src/core/TciSendQueue.h  (NereusSDR)
// NereusSDR-original — per-client TCI outbound priority send queue.
//
// Upstream reference: Thetis TCIServer.cs:769-774 + 1645-1679 [v2.10.3.13]
//   https://github.com/ramdor/Thetis
//   Copyright (C) 2020-2025 Richard Samphire MW0LGE
//
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 14.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.

#pragma once

#include <QtCore/QMutex>
#include <QtCore/QQueue>
#include <QtCore/QString>

namespace NereusSDR {

// TciSendQueue — three-priority outbound queue with bounded-depth oldest-drop.
//
// Three internal FIFO queues mirror Thetis TCIServer.cs:769-771 [v2.10.3.13]:
//   - Urgent  (m_outboundUrgentFrames)  — ping/pong/close frames
//   - Binary  (m_outboundBinaryFrames)  — IQ + audio binary frames
//   - Control (m_outboundControlFrames) — text command frames
//
// tryPop() drains in that order, matching Thetis
// tryDequeueNextOutboundFrameLocked at TCIServer.cs:1648-1679 [v2.10.3.13].
//
// All public methods are thread-safe (QMutex-protected).
class TciSendQueue {
public:
    enum class Priority {
        Urgent  = 0,  // ping/pong/close — TCIServer.cs:769 [v2.10.3.13]
        Binary  = 1,  // IQ + audio binary — TCIServer.cs:770 [v2.10.3.13]
        Control = 2,  // text commands — TCIServer.cs:771 [v2.10.3.13]
    };

    // Construct with a per-queue capacity. When any single queue reaches
    // capacity, the OLDEST frame is dropped to make room (oldest-drop).
    // Default 1024 frames per priority bucket.
    explicit TciSendQueue(int capacityPerQueue = 1024);

    // Push a frame onto the named priority queue. If the target queue is at
    // capacity the OLDEST frame is dropped (NereusSDR-original safety
    // behavior; Thetis queues are unbounded). Thread-safe.
    void push(Priority priority, const QString& frame);

    // Pop the next frame in priority order: Urgent first, then Binary, then
    // Control (matches Thetis:1648-1679 [v2.10.3.13]). Returns true and
    // writes to *out on success; returns false when all queues are empty.
    // Thread-safe. *out is not modified on false return.
    bool tryPop(QString* out);

    // Total frames dropped due to capacity overflow since construction (or
    // last reset()). Backs the ClientChainApplet "frames dropped" counter
    // surfaced in Phase 22. Thread-safe.
    int dropCount() const;

    // Total frames currently buffered across all three queues. Thread-safe.
    int size() const;

    // Clear all queues and reset the drop counter. Test-only helper.
    void reset();

private:
    mutable QMutex  m_mutex;
    QQueue<QString> m_urgent;
    QQueue<QString> m_binary;
    QQueue<QString> m_control;
    int             m_capacityPerQueue;
    int             m_dropCount{0};
};

} // namespace NereusSDR
