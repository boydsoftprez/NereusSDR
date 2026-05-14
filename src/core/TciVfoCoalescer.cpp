// no-port-check: NereusSDR-original. See TciVfoCoalescer.h for cite chain.

// src/core/TciVfoCoalescer.cpp  (NereusSDR)
// NereusSDR-original — TCI VFO coalescer implementation.
//
// Layer 3 outbound-coalesced map, per Thetis TCIServer.cs:1722-1727 [v2.10.3.13].
// Layers 1+2 subsumed by Qt event loop + 5ms TciServer drain timer (Phase 14).
//
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 15.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.

#include "TciVfoCoalescer.h"
#include <QtCore/QMutexLocker>

namespace NereusSDR {

void TciVfoCoalescer::update(const QString& key, const QString& frame)
{
    QMutexLocker locker(&m_mutex);
    if (!m_frames.contains(key)) {
        // First time we see this key — record its arrival order.
        m_order.enqueue(key);
    }
    // Latest-wins: replace (or insert) the frame for this key.
    m_frames.insert(key, frame);
}

void TciVfoCoalescer::drainAll(QStringList* out)
{
    QMutexLocker locker(&m_mutex);
    if (!out) {
        m_frames.clear();
        m_order.clear();
        return;
    }
    while (!m_order.isEmpty()) {
        const QString key = m_order.dequeue();
        const auto it = m_frames.find(key);
        if (it != m_frames.end()) {
            out->append(it.value());
            m_frames.erase(it);
        }
    }
}

void TciVfoCoalescer::clear()
{
    QMutexLocker locker(&m_mutex);
    m_order.clear();
    m_frames.clear();
}

int TciVfoCoalescer::pending() const
{
    QMutexLocker locker(&m_mutex);
    return m_order.size();
}

} // namespace NereusSDR
