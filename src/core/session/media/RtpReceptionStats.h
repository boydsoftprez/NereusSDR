// =================================================================
// src/core/session/media/RtpReceptionStats.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Remote daemon R3 Task 5a (R-R3-23).
// RFC 3550 section 6.4.1 (interarrival jitter) and appendix A.1/A.8
// reference algorithms, adapted to nanosecond arrival timestamps, for one
// remote audio context. Owns no transport, jitter-buffer, or device state.
// =================================================================

#pragma once

#include <QtGlobal>

#include <optional>

namespace NereusSDR {

/// One context's RTP reception measurements: interarrival jitter (RFC 3550
/// appendix A.8) and extended sequence-number expected/received/missing
/// accounting (appendix A.1, without the source-probation state machine:
/// every admitted packet is trusted the moment it is observed). Not
/// thread-safe; the caller owns exclusive access and publishes snapshots
/// through its own synchronization.
class RtpReceptionStats {
public:
    explicit RtpReceptionStats(int clockRateHz = 48'000);

    void reset();

    /// One call per packet the receiver admitted as Accepted or Late.
    /// `sequence`/`rtpTimestamp` come from the RTP header; `arrivalNs` is
    /// this computer's monotonic clock at the moment the packet was
    /// received, in nanoseconds.
    void observe(quint16 sequence, quint32 rtpTimestamp, qint64 arrivalNs);

    /// Every observe() call counts, including reordered ("Late") packets.
    quint64 receivedPackets() const;
    /// Extended-sequence span (highest - first + 1, wrap-aware). 0 before
    /// the first observe() call.
    quint64 expectedPackets() const;
    /// max(0, expectedPackets() - receivedPackets()).
    quint64 missingPackets() const;
    /// RFC 3550 appendix A.8 running estimate, in milliseconds. nullopt
    /// until a second packet has been observed.
    std::optional<double> jitterMs() const;

private:
    int m_clockRateHz;

    quint64 m_received = 0;

    bool m_hasFirst = false;
    quint16 m_baseSeq = 0;
    quint16 m_maxSeq = 0;
    quint64 m_cycles = 0;

    bool m_hasPrevious = false;
    qint64 m_previousArrivalNs = 0;
    quint32 m_previousTimestamp = 0;

    bool m_hasJitter = false;
    double m_jitterNs = 0.0;
};

} // namespace NereusSDR
