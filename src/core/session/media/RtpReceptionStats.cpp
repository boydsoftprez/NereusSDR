// =================================================================
// src/core/session/media/RtpReceptionStats.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Remote daemon R3 Task 5a (R-R3-23).
// RFC 3550 section 6.4.1 (interarrival jitter) and appendix A.1/A.8
// reference algorithms, adapted to nanosecond arrival timestamps.
// =================================================================

#include "core/session/media/RtpReceptionStats.h"

#include <cmath>

namespace NereusSDR {

RtpReceptionStats::RtpReceptionStats(int clockRateHz) : m_clockRateHz(clockRateHz) { }

void RtpReceptionStats::reset()
{
    m_received = 0;
    m_hasFirst = false;
    m_baseSeq = 0;
    m_maxSeq = 0;
    m_cycles = 0;
    m_hasPrevious = false;
    m_previousArrivalNs = 0;
    m_previousTimestamp = 0;
    m_hasJitter = false;
    m_jitterNs = 0.0;
}

void RtpReceptionStats::observe(quint16 sequence, quint32 rtpTimestamp, qint64 arrivalNs)
{
    // RFC 3550 appendix A.1, simplified: no source-probation state machine.
    // The first observed packet seeds base/max; a later packet extends max
    // (and its 65536-wrap cycle count) only when it is not older than the
    // current max. An older (reordered) packet never moves max, but it is
    // still an observed, received packet.
    if (!m_hasFirst) {
        m_baseSeq = sequence;
        m_maxSeq = sequence;
        m_hasFirst = true;
    } else {
        const quint16 forwardDelta = static_cast<quint16>(sequence - m_maxSeq);
        if (forwardDelta < 0x8000) {
            if (sequence < m_maxSeq) {
                m_cycles += 65536;
            }
            m_maxSeq = sequence;
        }
    }
    ++m_received;

    // RFC 3550 appendix A.8, adapted to nanosecond arrival timestamps: the
    // signed 32-bit RTP timestamp delta absorbs timestamp wraparound, and D
    // is the arrival-time deviation from the sender's clock, in ns.
    if (m_hasPrevious) {
        const qint32 tsDeltaTicks = static_cast<qint32>(rtpTimestamp - m_previousTimestamp);
        const double expectedArrivalDeltaNs =
            double(tsDeltaTicks) * 1'000'000'000.0 / double(m_clockRateHz);
        const double arrivalDeltaNs = double(arrivalNs - m_previousArrivalNs);
        const double deviationNs = arrivalDeltaNs - expectedArrivalDeltaNs;
        m_jitterNs += (std::abs(deviationNs) - m_jitterNs) / 16.0;
        m_hasJitter = true;
    }
    m_previousArrivalNs = arrivalNs;
    m_previousTimestamp = rtpTimestamp;
    m_hasPrevious = true;
}

quint64 RtpReceptionStats::receivedPackets() const
{
    return m_received;
}

quint64 RtpReceptionStats::expectedPackets() const
{
    if (!m_hasFirst) {
        return 0;
    }
    return m_cycles + quint64(m_maxSeq) - quint64(m_baseSeq) + 1;
}

quint64 RtpReceptionStats::missingPackets() const
{
    const quint64 expected = expectedPackets();
    return expected > m_received ? expected - m_received : 0;
}

std::optional<double> RtpReceptionStats::jitterMs() const
{
    if (!m_hasJitter) {
        return std::nullopt;
    }
    return m_jitterNs / 1'000'000.0;
}

} // namespace NereusSDR
