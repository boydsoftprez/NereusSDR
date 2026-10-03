// no-port-check: NereusSDR-original VFO coalescer. Layer 3: outbound
// coalesced-map per Thetis TCIServer.cs:1722-1727 [v2.10.3.13]. Layer 1 (the
// per-app update gap) is ported in TciUpdateGap; Layer 2 (the bounded
// LinkedList) is subsumed by this coalescer.
//
// The coalesce window is implicit: between drain calls, multiple updates with
// the same key collapse to one. TciServer's drain timer (5ms) calls drainAll
// once per tick, collapsing within a 5ms window.
//
// How the three Thetis throttling layers map (TCIServer.cs:1302-1381 and
// 1722-1727 [v2.10.3.13]):
//     Layer 1: m_swVFO / m_tmVFOtimer, m_swCentre / m_tmCentretimer,
//               m_swTXFrequency / m_tmTXFrequency at TCIServer.cs:6421-6480
//               [v2.10.3.15]: the shortest gap (udTCIRateLimit, 0..1000 ms,
//               default 100) between outgoing vfo, dds and tx_frequency
//               updates to each app. Ported in TciUpdateGap (receiver and
//               transmit gaps plan, Task 10, R-R3-49); each app's
//               TciClientSession holds one, and TciServer passes every
//               broadcast line through it after this coalescer's drain.
//     Layer 2: limitList at TCIServer.cs:1302-1311: bounded LinkedList(10)
//               with oldest-drop. Not ported: this coalescer already keeps
//               one line per key, so the list cannot grow.
//     Layer 3: m_outboundCoalescedFrames at TCIServer.cs:1722-1727:
//               outbound-coalesced map keyed by command. This class; it
//               stays, and runs before Layer 1 (shared by every app).
//
// Modification history (NereusSDR):
//   2026-05-10 - Phase 3J-1 Task 15.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-24 - Receiver and transmit gaps plan, Task 10 (R-R3-49) by
//                J.J. Boyd (KG4VCF): Layer 1 is now ported (TciUpdateGap),
//                no longer subsumed by the event loop.
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 12 (R-R3-49) by
//                J.J. Boyd (KG4VCF): frames carry a tag (the update gate
//                of the event that queued them) to the drain.
//                AI-assisted transformation via Anthropic Claude Code.

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
    //
    // tag is an opaque number the caller carries with the frame to the
    // drain (TciProtocol uses it for the update gate the event that queued
    // the frame belongs to, Task 12 R-R3-49); -1 means none. A replacing
    // update replaces the tag too.
    void update(const QString& key, const QString& frame, int tag = -1);

    // One drained frame with the key and tag it was queued under.
    struct Entry {
        QString key;
        QString frame;
        int     tag{-1};
    };

    // Drain all pending frames in original arrival order into *out.
    // Clears internal state. Thread-safe. If out is nullptr, drops all frames.
    void drainAll(QStringList* out);

    // As drainAll, keeping each frame's key and tag.
    QList<Entry> drainEntries();

    // Drop all pending frames without emitting. Thread-safe.
    void clear();

    // Number of pending unique keys. Thread-safe.
    int pending() const;

private:
    mutable QMutex m_mutex;
    QQueue<QString>      m_order;   // insertion order of unique keys
    QHash<QString, QString> m_frames;  // key → latest frame
    QHash<QString, int>     m_tags;    // key → latest frame's tag
};

} // namespace NereusSDR
