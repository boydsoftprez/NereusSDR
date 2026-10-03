// no-port-check: NereusSDR-original. See RadioLinkStats.h.
#include "core/RadioLinkStats.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace NereusSDR {

RadioLinkStats::RadioLinkStats() = default;

qint64 RadioLinkStats::nowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void RadioLinkStats::reset()
{
    for (Bucket& bucket : m_buckets) {
        bucket.index.store(-1, std::memory_order_relaxed);
        bucket.received.store(0, std::memory_order_relaxed);
        bucket.lost.store(0, std::memory_order_relaxed);
        bucket.maxGapUs.store(0, std::memory_order_relaxed);
    }
    m_udpPacketsSeen.store(0, std::memory_order_relaxed);
    m_lastDatagramUs.store(-1, std::memory_order_relaxed);
    m_jitterStream = -1;
    m_jitterLastArrivalUs = 0;
    m_jitterLastSequence = 0;
    m_jitterUs = 0.0;
    m_publishedJitterUs.store(0.0, std::memory_order_relaxed);
    m_jitterValid.store(false, std::memory_order_relaxed);
    m_jitterStreamLastUs.store(-1, std::memory_order_relaxed);
}

RadioLinkStats::Bucket& RadioLinkStats::bucketFor(qint64 nowUs)
{
    const qint64 index = std::max<qint64>(0, nowUs) / kBucketUs;
    Bucket& bucket = m_buckets[static_cast<std::size_t>(index % kBucketCount)];
    if (bucket.index.load(std::memory_order_relaxed) != index) {
        // This slot last held a bucket from kBucketCount slots ago.
        bucket.index.store(-1, std::memory_order_relaxed);
        bucket.received.store(0, std::memory_order_relaxed);
        bucket.lost.store(0, std::memory_order_relaxed);
        bucket.maxGapUs.store(0, std::memory_order_relaxed);
        bucket.index.store(index, std::memory_order_release);
    }
    return bucket;
}

void RadioLinkStats::noteDatagram(qint64 nowUs)
{
    m_udpPacketsSeen.fetch_add(1, std::memory_order_relaxed);
    const qint64 last = m_lastDatagramUs.load(std::memory_order_relaxed);
    Bucket& bucket = bucketFor(nowUs);
    if (last >= 0 && nowUs >= last) {
        const qint64 gap = nowUs - last;
        if (gap > bucket.maxGapUs.load(std::memory_order_relaxed)) {
            bucket.maxGapUs.store(gap, std::memory_order_relaxed);
        }
    }
    m_lastDatagramUs.store(nowUs, std::memory_order_relaxed);
}

void RadioLinkStats::noteSequenced(qint64 nowUs, quint32 lost)
{
    Bucket& bucket = bucketFor(nowUs);
    bucket.received.fetch_add(1, std::memory_order_relaxed);
    if (lost > 0) {
        bucket.lost.fetch_add(lost, std::memory_order_relaxed);
    }
}

// RFC 3550 section 6.4.1, the interarrival jitter estimator:
//   J(i) = J(i-1) + (|D(i-1,i)| - J(i-1)) / 16
double RadioLinkStats::nextJitter(double jitter, double transitDifference)
{
    return jitter + (std::abs(transitDifference) - jitter) / 16.0;
}

// NereusSDR-native: Thetis measures no jitter. D is the difference between
// the arrival spacing of two datagrams of one stream and the spacing their
// sequence numbers imply at the stream's rate (RFC 3550 section 6.4.1, with
// the RTP timestamp replaced by sequence number times the datagram's
// nominal duration).
void RadioLinkStats::noteStreamArrival(int stream, quint32 sequence, qint64 nowUs,
                                       double nominalSpacingUs)
{
    const bool idle = m_jitterStream >= 0
        && nowUs - m_jitterLastArrivalUs > kStreamIdleUs;
    if (m_jitterStream < 0 || stream < m_jitterStream
        || (stream != m_jitterStream && idle)) {
        // The lowest active stream changed: start its estimate afresh.
        m_jitterStream = stream;
        m_jitterLastArrivalUs = nowUs;
        m_jitterLastSequence = sequence;
        m_jitterUs = 0.0;
        m_jitterValid.store(false, std::memory_order_relaxed);
        m_jitterStreamLastUs.store(nowUs, std::memory_order_relaxed);
        return;
    }
    if (stream != m_jitterStream) {
        return;
    }
    const quint32 step = sequence - m_jitterLastSequence;
    const qint64 arrivalSpacing = nowUs - m_jitterLastArrivalUs;
    m_jitterLastArrivalUs = nowUs;
    m_jitterLastSequence = sequence;
    m_jitterStreamLastUs.store(nowUs, std::memory_order_relaxed);
    if (step == 0 || step > kMaxSequenceStep || arrivalSpacing < 0
        || !std::isfinite(nominalSpacingUs) || nominalSpacingUs <= 0.0) {
        // A repeat, a restart or no rate: resynchronise on this datagram.
        return;
    }
    const double transitDifference =
        static_cast<double>(arrivalSpacing) - static_cast<double>(step) * nominalSpacingUs;
    m_jitterUs = nextJitter(m_jitterUs, transitDifference);
    m_publishedJitterUs.store(m_jitterUs, std::memory_order_relaxed);
    m_jitterValid.store(true, std::memory_order_relaxed);
}

RadioLinkStats::Snapshot RadioLinkStats::snapshot(qint64 nowUs) const
{
    Snapshot out;
    out.udpPacketsSeen = m_udpPacketsSeen.load(std::memory_order_relaxed);
    const qint64 nowIndex = std::max<qint64>(0, nowUs) / kBucketUs;
    const qint64 lossBuckets = kLossWindowUs / kBucketUs;
    const qint64 gapBuckets = kGapWindowUs / kBucketUs;
    quint64 received = 0;
    quint64 lost = 0;
    qint64 maxGapUs = -1;
    for (const Bucket& bucket : m_buckets) {
        const qint64 index = bucket.index.load(std::memory_order_acquire);
        if (index < 0 || index > nowIndex) {
            continue;
        }
        const qint64 age = nowIndex - index;
        if (age < lossBuckets) {
            received += bucket.received.load(std::memory_order_relaxed);
            lost += bucket.lost.load(std::memory_order_relaxed);
        }
        if (age < gapBuckets) {
            maxGapUs = std::max(maxGapUs, bucket.maxGapUs.load(std::memory_order_relaxed));
        }
    }
    if (received + lost > 0) {
        out.packetLossPercent = 100.0 * static_cast<double>(lost)
            / static_cast<double>(received + lost);
    }
    const qint64 last = m_lastDatagramUs.load(std::memory_order_relaxed);
    if (last >= 0) {
        // The interval still open now counts: a radio gone quiet shows it.
        maxGapUs = std::max(maxGapUs, std::max<qint64>(0, nowUs - last));
        out.packetGapMs = static_cast<double>(maxGapUs) / 1000.0;
    }
    const qint64 streamLast = m_jitterStreamLastUs.load(std::memory_order_relaxed);
    if (m_jitterValid.load(std::memory_order_relaxed) && streamLast >= 0
        && nowUs - streamLast <= kLossWindowUs) {
        out.jitterMs = m_publishedJitterUs.load(std::memory_order_relaxed) / 1000.0;
    }
    return out;
}

} // namespace NereusSDR
