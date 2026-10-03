#pragma once
// =================================================================
// src/core/session/media/DualPathAudio.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// iPhone app plan Task 29 (R-IOS-16; the pairing design, section 5.4,
// "dual-receive across a switch window, timestamp-based deduplication,
// and a defined jitter-buffer behaviour across the discontinuity"; the
// remote media control document, "Replacing the media connection"): the
// audio packets of two media connections merged into one stream across a
// replacement, as the window's jitter queue needs them.
//
// The queue plays each packet at its arrival plus its hold (the Core's
// clock, AudioJitterBuffer). A new path that is faster than the old one
// would pull that schedule earlier the moment its first packet arrives,
// and the packets only the slow old path carries (those sent before the
// new peer was ready) would then come after their time and be concealed:
// a gap as long as the difference between the two paths. So:
//
//   - While both paths carry audio, a packet from the new path waits
//     until the old path's copy of the same packet (same stream, same RTP
//     timestamp) arrives, and is dropped then as the copy it is; the old
//     path sets the schedule. How much earlier the new path brought it is
//     the new path's lead.
//   - A packet from the new path whose old copy never comes (lost, or sent
//     after the old path stopped) is handed on at its arrival plus the
//     lead, so the schedule does not move; until a lead is known it waits
//     at most kMaxWaitMs.
//   - Once the old path is done (the Core's `replace` came and the old
//     connection's packets have drained), the lead eases off by one packet
//     interval (kEaseStepMs) every kEaseIntervalMs: each step brings the
//     packets one interval earlier, which the queue sheds as one skipped
//     interval. The path's lower delay is reached in steps of 40 ms, none
//     longer.
//   - A new path that is slower than the old one has no lead: its packets
//     go on at once, and its copies of what the old path already brought
//     are dropped.
//
// Every packet is handed on once: a second copy of a (stream, timestamp)
// is dropped (RtpDuplicateFilter). A packet too short to be RTP goes on
// unchanged. The clock is the caller's, in milliseconds, so a test drives
// it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/media/RtpDuplicateFilter.h"

#include <QByteArray>
#include <QHash>
#include <QtGlobal>

#include <functional>
#include <map>
#include <optional>

namespace NereusSDR {

class DualPathAudio {
public:
    /// The longest a new path's packet waits for its old copy while no
    /// lead is known: longer than any relay's delay difference plus the
    /// deepest jitter hold (AudioJitterBuffer::kMaxHoldNs, 500 ms).
    static constexpr int kMaxWaitMs = 600;
    /// The lead eases off one Opus packet interval at a time...
    static constexpr int kEaseStepMs = 40;
    /// ...this long apart, the time the window's queue takes to see a
    /// steady link (AudioJitterBuffer::kShrinkQuietNs) and shed the excess.
    static constexpr int kEaseIntervalMs = 2000;

    using Deliver = std::function<void(const QByteArray& packet)>;

    explicit DualPathAudio(Deliver deliver) : m_deliver(std::move(deliver)) {}

    /// Dual receive begins: a new path joins the old.
    void start(qint64 nowMs);
    /// One packet from the old path (false) or the new (true).
    void submit(const QByteArray& packet, bool fromNewPath, qint64 nowMs);
    /// The old path will bring nothing more: the lead starts easing.
    void oldPathDone(qint64 nowMs);
    /// The new path is gone (the replacement failed): what it held is
    /// dropped, and the old path goes on alone.
    void newPathGone();
    /// Hands on what is due and eases the lead. Call often (every few ms)
    /// while active().
    void tick(qint64 nowMs);
    /// Held packets waiting, or a lead still to ease.
    bool active() const { return m_active; }
    qint64 leadMs() const { return m_leadMs.value_or(0); }
    bool leadKnown() const { return m_leadMs.has_value(); }
    /// Copies dropped: the new path's packets the old path's copy
    /// replaced while they waited, and every later copy.
    quint64 duplicatesDropped() const { return m_duplicates.dropped() + m_replacedWhileHeld; }
    int held() const { return static_cast<int>(m_held.size()); }

private:
    struct Held {
        QByteArray packet;
        qint64 arrivalMs = 0;
        quint64 key = 0;
    };
    static quint64 keyOf(const QByteArray& packet);
    void deliver(const QByteArray& packet);
    void finishIfIdle();

    Deliver m_deliver;
    RtpDuplicateFilter m_duplicates;
    /// Held packets by arrival order (Task 29 fix wave, review Minor 11:
    /// never by timestamp, which wraps), and each one's order by (stream,
    /// timestamp) for matching the old path's copy.
    std::map<quint64, Held> m_held;
    QHash<quint64, quint64> m_heldOrder;
    quint64 m_nextOrder = 0;
    /// When the old path brought each (stream, timestamp), the last few.
    QHash<quint64, qint64> m_oldArrivals;
    std::optional<qint64> m_leadMs;
    quint64 m_replacedWhileHeld = 0;
    bool m_active = false;
    bool m_oldDone = false;
    qint64 m_nextEaseMs = 0;
};

} // namespace NereusSDR
