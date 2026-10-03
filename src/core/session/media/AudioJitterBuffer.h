#pragma once
// no-port-check: NereusSDR-original. Bounded ordering for validated audio RTP.
#include <QByteArray>
#include <QtGlobal>
#include <algorithm>
#include <deque>
#include <utility>
#include <vector>
#include <map>
#include <optional>

namespace NereusSDR {

/// Single consumer-thread queue. RTP parsing/SSRC and audio-generation checks
/// happen before insertion. It does not decode or own a device clock.
///
/// R-R3-23: the packet size is a parameter. Opus sends one 1920-frame, 40 ms
/// packet per block (the default); lossless sends 192-frame, 4 ms packets.
/// The reordering window and the hold are times, so both profiles tolerate
/// the same network delay: 8 Opus packets or 80 lossless packets.
///
/// R-R3-21: the hold adapts to the link. A packet that arrives after its
/// interval was already concealed deepens the hold by as much as it was
/// late (plus a margin), up to kMaxHoldNs, so the next stall of the same
/// length is ridden through instead of concealed. After kShrinkQuietNs
/// with no such packet the hold eases back by kShrinkStepNs a
/// kShrinkIntervalNs, never below kHoldNs. The window grows and shrinks
/// with the hold: it is always kWindowNs plus the hold's growth.
///
/// When every interval released since the late packet's own was concealed
/// too, the packet is not thrown away: the queue rewinds to it (Rewound),
/// so the concealment already heard becomes the added delay and the late
/// audio plays after it, in order. When any of them played real audio, a
/// rewind would replay out of order, so the packet stays late
/// (LateConcealed). Once the link has been quiet for kShrinkQuietNs after
/// a late packet, a standing excess is shed: when the queued span plus the
/// downstream excess has stayed above the hold plus kShedReserveNs for a
/// whole kShrinkIntervalNs (CoDel's rule: a queue that never dips below
/// its target is carrying delay, one that dips is only jittered), one
/// interval is skipped, and the interval starts again. So as the hold
/// eases the delay the operator hears comes back down with it, while
/// ordinary jitter never costs on-time audio. Shedding disarms once the
/// hold is back at kHoldNs with no excess.
///
/// Load findings 2: a consumer stall arms it too. A gap between ticks
/// longer than kStallNs (the consumer did not wake: packets kept arriving
/// on time and none was late) can leave a backlog standing that no late
/// packet announces; shedding is armed at once (the hold was not deepened,
/// so there is no easing to wait for) and sheds that excess by the same
/// rule, then disarms the same way.
///
/// A fixed-hold queue (setAdaptive(false), the PCM sink's) never grows,
/// rewinds or sheds: its consumer is paced by the hold alone, and a late
/// packet there is only late.
class AudioJitterBuffer {
public:
    /// The Opus packet, the default shape.
    static constexpr int kDefaultPacketFrames = 1920;
    static constexpr qint64 kDefaultPacketDurationNs = 40'000'000;
    /// How far ahead of the next expected packet a packet may arrive: 320 ms,
    /// the eight-packet Opus window this queue has always had.
    static constexpr qint64 kWindowNs = 320'000'000;
    /// Every packet is held this long after arrival before release: the
    /// starting hold, and its floor.
    static constexpr qint64 kHoldNs = 80'000'000;
    /// The deepest the hold grows after late packets (R-R3-21).
    static constexpr qint64 kMaxHoldNs = 500'000'000;
    /// Added to a late packet's lateness when the hold grows for it.
    static constexpr qint64 kGrowMarginNs = 20'000'000;
    /// A steady link: this long without a late packet before the hold
    /// eases back, by kShrinkStepNs every kShrinkIntervalNs.
    static constexpr qint64 kShrinkQuietNs = 2'000'000'000;
    static constexpr qint64 kShrinkStepNs = 20'000'000;
    static constexpr qint64 kShrinkIntervalNs = 1'000'000'000;
    /// Queued audio past the hold that shedding leaves alone: a packet in
    /// flight while arrival and release interleave.
    static constexpr qint64 kShedReserveNs = 40'000'000;
    /// R-R3-21: how much a standing excess sheds each time: one Opus
    /// interval, ten lossless ones.
    static constexpr qint64 kShedStepNs = 40'000'000;
    /// Load findings 2: a gap between ticks longer than this is a consumer
    /// stall, and arms shedding. The receive worker waits at most 2 ms for
    /// work, and its steady runs in the test suites wake within 14 ms at a
    /// load of 160; the stall that left 50 ms standing in
    /// tst_remote_audio_receiver_wifi was a 39 ms gap.
    static constexpr qint64 kStallNs = 20'000'000;
    /// The window at the deepest hold.
    static constexpr qint64 kMaxWindowNs = kWindowNs + (kMaxHoldNs - kHoldNs);
    /// R-R3-21. Late: behind the head (a copy of a packet that played, or
    /// one whose interval is no longer remembered). LateConcealed: its
    /// interval was concealed and it cannot be rewound to; an adaptive
    /// queue grew its hold for it. Rewound: its interval was concealed, as
    /// was every one since, and the queue rewound to play it next; the
    /// stream runs that much later from here, and the hold grew to match.
    enum class Admission { Accepted, Duplicate, Late, OutsideWindow, Invalid, Rewound,
                           LateConcealed };
    struct Playout {
        QByteArray packet; // empty means one explicit missing-packet interval
        quint32 timestamp{0};
        bool concealed() const { return packet.isEmpty(); }
    };

    /// Packets of the window a packet duration allows (at least one).
    static constexpr int windowPackets(qint64 packetDurationNs)
    {
        return windowPackets(kWindowNs, packetDurationNs);
    }
    /// Packets of a given window (at least one).
    static constexpr int windowPackets(qint64 windowNs, qint64 packetDurationNs)
    {
        return packetDurationNs > 0 && packetDurationNs <= windowNs
            ? int(windowNs / packetDurationNs) : 1;
    }

    AudioJitterBuffer() = default;
    /// A non-positive size or duration keeps the Opus default shape.
    AudioJitterBuffer(int packetFrames, qint64 packetDurationNs);

    /// Clears the queue and anchors it on firstTimestamp. The hold is the
    /// link's, not the anchor's, so it is kept.
    void reset(quint32 firstTimestamp);
    Admission insert(const QByteArray& packet, quint32 timestamp, qint64 arrivalNs);
    std::optional<Playout> takeReady(qint64 nowNs);
    /// Releases only the exact expected packet when downstream PCM is about
    /// to underrun. Future packets never conceal a missing head through this
    /// path. Its original due time remains the empty-queue missing deadline;
    /// the existing future-packet anchor rule is unchanged. R-R3-21: given
    /// the time, that deadline is anchored no later than nowNs plus the
    /// base hold, so a deepened hold cannot leave a following stall
    /// unconcealed until the rate matcher has run dry.
    std::optional<Playout> takeExpectedPresentEarly(std::optional<qint64> nowNs = std::nullopt);
    /// R-R3-21: conceals the expected interval now, when downstream PCM is
    /// about to underflow and the expected packet is not here (a hole,
    /// whose deadline a deepened hold pushes out). An underflow would cost
    /// a fresh context; one concealed interval costs one packet of PLC.
    /// Nothing when the expected packet is present (use
    /// takeExpectedPresentEarly()).
    /// R-R3-21 (re-review): also with the queue empty while the hold is
    /// deepened (playback then runs on demand) once something has been
    /// released, so a gap in arrivals is concealed before the matcher
    /// underflows. At the base hold, and before the first release, an
    /// empty queue is never concealed here.
    std::optional<Playout> concealExpectedNow(qint64 nowNs);
    /// R-R3-21: eases the hold and sheds a standing excess, as takeReady()
    /// does first; for a consumer that may not reach takeReady() on a wake
    /// (the speaker's release is held back while the matcher is full).
    /// Load findings 2: called on every wake, so a gap between calls longer
    /// than kStallNs arms shedding (see the class comment).
    void tick(qint64 nowNs);
    /// R-R3-21: the intervals shed since the last call, oldest first (at
    /// most a window's worth kept), a missing one as an empty packet, so
    /// an Opus decoder can decode (or conceal) and discard each and keep
    /// its state continuous across the skip.
    std::vector<QByteArray> takeShedPackets() { return std::exchange(m_shedPackets, {}); }
    /// Intervals skipped unheard to bound the delay, present or missing:
    /// by advanceToFit() and by shedding. Cumulative for this queue.
    quint64 skippedIntervals() const { return m_skipped; }
    /// Intervals a rewind replayed (each heard once concealed, then again
    /// as the late audio). Cumulative for this queue.
    quint64 rewoundIntervals() const { return m_rewound; }
    /// R-R3-21: a packet at `timestamp` fell outside the window. Moves the
    /// head forward just far enough for it to fit, dropping the oldest
    /// queued packets and skipping missing intervals; with nothing queued
    /// the head moves to the packet itself, so no run of concealment is
    /// made for audio that was never going to arrive. Returns the packets
    /// dropped. Nothing moves when it already fits or is behind the head.
    int advanceToFit(quint32 timestamp);
    /// R-R3-21: false for a fixed hold (see the class comment). Default true.
    void setAdaptive(bool adaptive) { m_adaptive = adaptive; }
    /// Packets dropped unheard to bound the delay: by advanceToFit() and by
    /// shedding as the hold eases. Cumulative for this queue.
    quint64 trimmedPackets() const { return m_trimmed; }
    /// The queued span in time: from the head to the newest queued packet.
    qint64 queuedSpanNs() const;
    /// R-R3-21: audio downstream (the rate matcher) holds beyond its own
    /// working level, which shedding counts with the queued span: a
    /// backlog released into the matcher is delay the same as one queued
    /// here, and skipping an interval here lets the matcher drain it. A
    /// matcher below its working level counts negative: the audio queued
    /// here is then on its way to refill it, not excess delay.
    void setDownstreamExcessNs(qint64 excessNs) { m_downstreamExcessNs = excessNs; }
    int queuedPackets() const { return static_cast<int>(m_packets.size()); }
    quint32 nextTimestamp() const { return m_nextTimestamp; }
    int packetFrames() const { return m_packetFrames; }
    qint64 packetDurationNs() const { return m_packetDurationNs; }
    /// The window in packets of this queue's size, at the current hold.
    int maxPackets() const { return m_maxPackets; }
    /// The current hold (R-R3-21): kHoldNs up to kMaxHoldNs.
    qint64 holdNs() const { return m_holdNs; }

private:
    struct Entry { QByteArray packet; qint64 dueNs; };
    // R-R3-21: a recently released interval. A late packet finds its own
    // here to learn how late it was and what the hold was then.
    struct Released { quint32 timestamp; qint64 releasedNs; qint64 holdNs; bool concealed; };
    void noteRelease(quint32 timestamp, qint64 releasedNs, bool concealed);
    void setHold(qint64 holdNs);
    void easeHold(qint64 nowNs);
    void shedExcess(qint64 nowNs);
    std::map<quint64, Entry> m_packets;
    std::deque<Released> m_released;
    qint64 m_holdNs{kHoldNs};
    std::optional<qint64> m_lastLateNs;
    // Load findings 2: the last tick, and whether a gap between ticks has
    // armed shedding.
    std::optional<qint64> m_lastTickNs;
    bool m_stallArmed{false};
    // Since when the queue has stood above its target, while armed.
    std::optional<qint64> m_excessSinceNs;
    std::vector<QByteArray> m_shedPackets;
    quint64 m_skipped{0};
    quint64 m_rewound{0};
    qint64 m_lastEaseNs{0};
    bool m_adaptive{true};
    qint64 m_downstreamExcessNs{0};
    quint64 m_trimmed{0};
    int m_packetFrames{kDefaultPacketFrames};
    qint64 m_packetDurationNs{kDefaultPacketDurationNs};
    int m_maxPackets{windowPackets(kDefaultPacketDurationNs)};
    quint64 m_nextIndex{0};
    quint32 m_nextTimestamp{0};
    std::optional<qint64> m_nextMissingDue;
};
static_assert(AudioJitterBuffer::windowPackets(AudioJitterBuffer::kDefaultPacketDurationNs) == 8,
              "the Opus window stays eight packets");
} // namespace NereusSDR
