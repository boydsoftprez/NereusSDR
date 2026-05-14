// no-port-check: NereusSDR-original priority send queue implementation.
// See TciSendQueue.h for the full cite chain and design rationale.
//
// src/core/TciSendQueue.cpp  (NereusSDR)
// NereusSDR-original — per-client TCI outbound priority send queue.
//
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 14.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.

#include "TciSendQueue.h"

#include <QtCore/QMutexLocker>

namespace NereusSDR {

TciSendQueue::TciSendQueue(int capacityPerQueue)
    : m_capacityPerQueue(capacityPerQueue)
{
}

void TciSendQueue::push(Priority priority, const QString& frame)
{
    QMutexLocker locker(&m_mutex);

    QQueue<QString>* target = nullptr;
    switch (priority) {
        case Priority::Urgent:  target = &m_urgent;  break;
        case Priority::Binary:  target = &m_binary;  break;
        case Priority::Control: target = &m_control; break;
    }
    if (!target) { return; }

    // Bounded depth + oldest-drop. Thetis queues are unbounded
    // (TCIServer.cs:769-771 [v2.10.3.13]); NereusSDR caps each queue to
    // prevent unbounded memory growth under backpressure. When the queue is
    // full, the front (oldest) frame is discarded before the new one is
    // appended — mirroring the bounded-oldest-drop contract described in the
    // design doc (Sweep C "Priority Send Queues").
    while (target->size() >= m_capacityPerQueue) {
        target->dequeue();
        ++m_dropCount;
    }
    target->enqueue(frame);
}

bool TciSendQueue::tryPop(QString* out)
{
    QMutexLocker locker(&m_mutex);

    // Drain order from Thetis tryDequeueNextOutboundFrameLocked at
    // TCIServer.cs:1648-1679 [v2.10.3.13]: Urgent first, then Binary, then
    // Control. The coalesced-key map (next tier in Thetis) is Phase 15.
    if (!m_urgent.isEmpty())  { *out = m_urgent.dequeue();  return true; }
    if (!m_binary.isEmpty())  { *out = m_binary.dequeue();  return true; }
    if (!m_control.isEmpty()) { *out = m_control.dequeue(); return true; }
    return false;
}

int TciSendQueue::dropCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_dropCount;
}

int TciSendQueue::size() const
{
    QMutexLocker locker(&m_mutex);
    return m_urgent.size() + m_binary.size() + m_control.size();
}

void TciSendQueue::reset()
{
    QMutexLocker locker(&m_mutex);
    m_urgent.clear();
    m_binary.clear();
    m_control.clear();
    m_dropCount = 0;
}

} // namespace NereusSDR
