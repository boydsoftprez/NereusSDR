#pragma once
// no-port-check: NereusSDR-original. The radio link's datagram counters
// (UDP packets seen, packet loss, jitter, packet gap) for Network
// Diagnostics and the Core's station telemetry (R-R3-32, R-R3-49).
//
// =================================================================
// src/core/RadioLinkStats.h  (NereusSDR)
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25 - Created for the remote-window parity plan, Task 6
//                (R-R3-32, R-R3-49). One source per radio connection for
//                the local Network Diagnostics rows and the Core's
//                station telemetry. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
// =================================================================

#include <QtGlobal>

#include <array>
#include <atomic>
#include <optional>

namespace NereusSDR {

/// Counters over the datagrams a radio connection receives from its radio.
///
/// One writer: the connection's own thread, from its receive path. Each
/// call there is a handful of relaxed atomic stores and arithmetic; there
/// is no lock and no allocation. Any thread may call snapshot() (the GUI
/// thread for Network Diagnostics, the Core's telemetry collector); a
/// reader racing a bucket's reset can see that one bucket half cleared,
/// which is acceptable for a diagnostic readout.
///
/// Times are microseconds on one monotonic clock (nowUs()); the caller
/// passes them in so tests can hand-work arrivals.
class RadioLinkStats {
public:
    struct Snapshot {
        // Datagrams received from the radio since the connection started.
        quint64 udpPacketsSeen = 0;
        // Lost / (received + lost) over the last kLossWindowUs, in percent,
        // from the sequence-checked streams. Absent with no such datagram
        // in the window.
        std::optional<double> packetLossPercent;
        // RFC 3550 interarrival jitter of the lowest active receive stream,
        // in ms. Absent until two datagrams of that stream have arrived, or
        // when the stream has been silent for the loss window.
        std::optional<double> jitterMs;
        // The longest interval between two datagrams from the radio in the
        // last kGapWindowUs, counting the interval still open now, in ms.
        // Absent before the first datagram.
        std::optional<double> packetGapMs;
    };

    static constexpr qint64 kBucketUs = 250'000;
    static constexpr int kBucketCount = 24;               // 6 s of buckets
    static constexpr qint64 kLossWindowUs = 5'000'000;    // the loss window
    static constexpr qint64 kGapWindowUs = 1'000'000;     // the gap window
    // A jitter stream that has sent nothing for this long gives way to
    // any other stream.
    static constexpr qint64 kStreamIdleUs = 1'000'000;
    // A sequence step larger than this is a restart, not a run of lost
    // datagrams: the jitter estimator resynchronises instead.
    static constexpr quint32 kMaxSequenceStep = 1000;

    RadioLinkStats();

    /// The monotonic clock the connections use.
    static qint64 nowUs();

    /// Clear everything; called when a connection starts (writer thread).
    void reset();

    /// One datagram from the radio, of any kind, on any port.
    void noteDatagram(qint64 nowUs);

    /// One datagram of a sequence-checked stream, with `lost` the sequence
    /// errors its arrival revealed (Thetis counts one per mismatch).
    void noteSequenced(qint64 nowUs, quint32 lost);

    /// One datagram of receive stream `stream` (a P2 DDC, or 0 for the P1
    /// EP6 stream) carrying sequence number `sequence`, whose samples span
    /// `nominalSpacingUs` at the stream's rate. Feeds the jitter of the
    /// lowest active stream.
    void noteStreamArrival(int stream, quint32 sequence, qint64 nowUs,
                           double nominalSpacingUs);

    Snapshot snapshot(qint64 nowUs) const;

    /// One step of RFC 3550 section 6.4.1: J += (|D| - J) / 16.
    static double nextJitter(double jitter, double transitDifference);

private:
    struct Bucket {
        std::atomic<qint64> index{-1};
        std::atomic<quint32> received{0};
        std::atomic<quint32> lost{0};
        std::atomic<qint64> maxGapUs{0};
    };

    Bucket& bucketFor(qint64 nowUs);

    std::array<Bucket, kBucketCount> m_buckets;
    std::atomic<quint64> m_udpPacketsSeen{0};
    std::atomic<qint64> m_lastDatagramUs{-1};

    // Jitter state. The estimator's inputs are the writer's alone; the
    // result is published through the two atomics below.
    int m_jitterStream{-1};
    qint64 m_jitterLastArrivalUs{0};
    quint32 m_jitterLastSequence{0};
    double m_jitterUs{0.0};
    std::atomic<double> m_publishedJitterUs{0.0};
    std::atomic<bool> m_jitterValid{false};
    std::atomic<qint64> m_jitterStreamLastUs{-1};
};

} // namespace NereusSDR
