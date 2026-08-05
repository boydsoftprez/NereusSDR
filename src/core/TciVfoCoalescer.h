// no-port-check: NereusSDR-original VFO coalescer. Layer 3 only — outbound
// coalesced-map per Thetis TCIServer.cs:1722-1727 [v2.10.3.13]. Layers 1+2
// (per-event one-shot timer + bounded LinkedList) from Thetis are subsumed
// by Qt's event loop + the 5ms drain timer in TciServer (Phase 14).
//
// The coalesce window is implicit: between drain calls, multiple updates with
// the same key collapse to one. TciServer's drain timer (5ms) calls drainAll
// once per tick, collapsing within a 5ms window.
//
// Architectural divergence from Thetis (documented):
//   Thetis implements all three throttling layers at TCIServer.cs:1302-1381
//   and 1722-1727 [v2.10.3.13]:
//     Layer 1 — m_tmVFOtimer etc. at TCIServer.cs:6018-6034: per-event
//               one-shot timer defers an event by m_nRateLimit ms.
//     Layer 2 — limitList at TCIServer.cs:1302-1311: bounded LinkedList(10)
//               with oldest-drop.
//     Layer 3 — m_outboundCoalescedFrames at TCIServer.cs:1722-1727:
//               outbound-coalesced map keyed by command.
//   NereusSDR implements Layer 3 only. Layers 1+2 are upstream throttling
//   that Qt's event loop (which coalesces back-to-back timer callbacks
//   and signal deliveries) + the 5ms drain timer already approximate.

#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QHash>
#include <QtCore/QQueue>
#include <QtCore/QMutex>

namespace NereusSDR {

// TciVfoCoalescer — thread-safe latest-wins frame map with arrival-order drain.
//
// Accepts updates via update(key, frame). If the key was already pending,
// the previous frame is REPLACED (latest-wins), but the insertion-order slot
// is preserved (drain still emits in original arrival order).
//
// drainAll() copies all stored frames in arrival order into *out and clears
// internal state. Thread-safe — safe to call from the drain timer on the main
// thread while update() may be called from a protocol handler on the same
// thread (or from tests synchronously).
//
// From Thetis TCIServer.cs:1722-1727 [v2.10.3.13] — outbound-coalesced map:
//   coalesced commands: vfo, if, dds, rx_filter_band, rx_balance, agc_gain,
//   drive, tune_drive, tune, tx_frequency, tx_frequency_thetis, volume.
// Phase 15 routes only vfo: through this coalescer; other keys are added as
// their handlers are ported.
class TciVfoCoalescer {
public:
    // Insert or replace a frame for the given key. If the key was already
    // pending, the previous frame is REPLACED (latest-wins) but the
    // arrival-order slot is preserved (drain emits in original insertion order).
    // Thread-safe.
    void update(const QString& key, const QString& frame);

    // Drain all pending frames in original arrival order into *out.
    // Clears internal state. Thread-safe. If out is nullptr, drops all frames.
    void drainAll(QStringList* out);

    // Drop all pending frames without emitting. Thread-safe.
    void clear();

    // Number of pending unique keys. Thread-safe.
    int pending() const;

private:
    mutable QMutex m_mutex;
    QQueue<QString>      m_order;   // insertion order of unique keys
    QHash<QString, QString> m_frames;  // key → latest frame
};

} // namespace NereusSDR
