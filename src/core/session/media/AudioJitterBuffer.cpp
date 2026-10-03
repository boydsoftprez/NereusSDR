// no-port-check: NereusSDR-original. Network queue policy; no DSP algorithm.
#include "core/session/media/AudioJitterBuffer.h"
#include "core/session/media/OpusAudioCodec.h"
#include <bit>
#include <algorithm>
#include <limits>

namespace NereusSDR {
AudioJitterBuffer::AudioJitterBuffer(int packetFrames, qint64 packetDurationNs)
{
    if (packetFrames > 0 && packetDurationNs > 0) {
        m_packetFrames = packetFrames;
        m_packetDurationNs = packetDurationNs;
        m_maxPackets = windowPackets(packetDurationNs);
    }
}

void AudioJitterBuffer::reset(quint32 firstTimestamp)
{
    m_packets.clear();
    m_nextIndex = 0;
    m_nextTimestamp = firstTimestamp;
    m_nextMissingDue.reset();
    m_released.clear();
}

void AudioJitterBuffer::setHold(qint64 holdNs)
{
    m_holdNs = std::clamp(holdNs, kHoldNs, kMaxHoldNs);
    m_maxPackets = windowPackets(kWindowNs + (m_holdNs - kHoldNs), m_packetDurationNs);
}

void AudioJitterBuffer::easeHold(qint64 nowNs)
{
    if (m_holdNs <= kHoldNs || !m_lastLateNs || nowNs - *m_lastLateNs < kShrinkQuietNs
        || nowNs - m_lastEaseNs < kShrinkIntervalNs) {
        return;
    }
    setHold(m_holdNs - kShrinkStepNs);
    m_lastEaseNs = nowNs;
}

qint64 AudioJitterBuffer::queuedSpanNs() const
{
    if (m_packets.empty()) { return 0; }
    return static_cast<qint64>(m_packets.rbegin()->first - m_nextIndex + 1) * m_packetDurationNs;
}

void AudioJitterBuffer::tick(qint64 nowNs)
{
    if (!m_adaptive) { return; }
    // Load findings 2: the consumer did not wake for longer than a stall.
    if (m_lastTickNs && nowNs - *m_lastTickNs > kStallNs) { m_stallArmed = true; }
    m_lastTickNs = nowNs;
    easeHold(nowNs);
    shedExcess(nowNs);
}

void AudioJitterBuffer::shedExcess(qint64 nowNs)
{
    // Only after a late packet, once the link has been quiet as long as it
    // takes the hold to start easing: the delay a stall added (a rewind's
    // replayed intervals, a backlog released into the rate matcher) is
    // then no longer wanted. Load findings 2: or after a consumer stall (a
    // gap between ticks), which can leave its backlog standing with no late
    // packet; unless a late packet is recent, whose deeper hold is still
    // wanted. A context with neither sheds nothing, so its ordinary jitter
    // never costs on-time audio.
    if ((!m_lastLateNs && !m_stallArmed)
        || (m_lastLateNs && nowNs - *m_lastLateNs < kShrinkQuietNs)) {
        m_excessSinceNs.reset();
        return;
    }
    // A standing excess only (CoDel's rule): above the target for a whole
    // interval without a dip. Any dip is jitter, and restarts the clock.
    const qint64 target = m_holdNs + kShedReserveNs;
    const qint64 content = queuedSpanNs() + m_downstreamExcessNs;
    if (m_packets.empty() || content <= target) {
        m_excessSinceNs.reset();
        if (m_holdNs <= kHoldNs) { // paid: disarm
            m_lastLateNs.reset();
            m_stallArmed = false;
        }
        return;
    }
    if (!m_excessSinceNs) {
        m_excessSinceNs = nowNs;
        return;
    }
    if (nowNs - *m_excessSinceNs < kShrinkIntervalNs) { return; }
    // The excess stood the whole interval: delay that never drained.
    // Skip kShedStepNs of it, contiguous from the head (ceil(40 ms / the
    // packet duration) intervals: one Opus interval, ten lossless ones),
    // then a fresh interval. So a lossless context sheds as fast as an
    // Opus one, 40 ms a second, twice the hold's easing, and the heard
    // delay keeps up with the readout at any packet size instead of
    // waiting on the rate matcher's slow resampling. At most one step
    // below the target, which the reserve covers. The skipped intervals
    // were never heard, so there is nothing for a late packet to find.
    const qint64 intervals = (kShedStepNs + m_packetDurationNs - 1) / m_packetDurationNs;
    const auto keep = static_cast<std::size_t>(windowPackets(kMaxWindowNs, m_packetDurationNs));
    for (qint64 i = 0; i < intervals && !m_packets.empty(); ++i) {
        const auto first = m_packets.begin();
        QByteArray shed; // empty: a missing interval
        if (first->first == m_nextIndex) {
            shed = std::move(first->second.packet);
            m_packets.erase(first);
            ++m_trimmed;
        }
        if (m_shedPackets.size() < keep) { m_shedPackets.push_back(std::move(shed)); }
        ++m_nextIndex;
        m_nextTimestamp += quint32(m_packetFrames);
        ++m_skipped;
    }
    m_released.clear();
    m_excessSinceNs = nowNs;
}

void AudioJitterBuffer::noteRelease(quint32 timestamp, qint64 releasedNs, bool concealed)
{
    m_released.push_back({timestamp, releasedNs, m_holdNs, concealed});
    // Enough to find any packet the deepest window could still call late.
    const auto bound = static_cast<std::size_t>(windowPackets(kMaxWindowNs, m_packetDurationNs));
    while (m_released.size() > bound) { m_released.pop_front(); }
}

AudioJitterBuffer::Admission AudioJitterBuffer::insert(
    const QByteArray& packet, quint32 timestamp, qint64 arrivalNs)
{
    if (packet.isEmpty() || packet.size() > OpusAudioCodecConfig::kMaxRtpPacketBytes
        || arrivalNs < 0 || arrivalNs > std::numeric_limits<qint64>::max() - kMaxHoldNs) {
        return Admission::Invalid;
    }
    const qint32 delta = std::bit_cast<qint32>(quint32(timestamp - m_nextTimestamp));
    if (delta < 0) {
        // R-R3-21: a packet whose interval was concealed arrived this much
        // too late. A late copy of a packet that did play changes nothing.
        for (auto it = m_released.rbegin(); it != m_released.rend(); ++it) {
            if (it->timestamp != timestamp) { continue; }
            if (!it->concealed || arrivalNs <= it->releasedNs) { break; }
            if (!m_adaptive) { return Admission::LateConcealed; }
            m_lastLateNs = arrivalNs;
            const qint64 lateNs = arrivalNs - it->releasedNs;
            // A rewind replays every interval released since this one, so
            // it is in order only when all of them were concealed too.
            const bool allConcealed = std::all_of(
                m_released.rbegin(), std::next(it),
                [](const Released& released) { return released.concealed; });
            const auto back = static_cast<quint64>(
                quint32(m_nextTimestamp - timestamp) / quint32(m_packetFrames));
            // Rewound, the stream runs `back` intervals later from here, so
            // the hold grows by at least that (by the lateness, if more),
            // plus the margin. Not rewound, only the lateness counts: had
            // the hold been that much deeper, the packet would have played.
            const qint64 delayNs = allConcealed
                ? std::max(lateNs, qint64(back) * m_packetDurationNs) : lateNs;
            const qint64 needed = std::min(kMaxHoldNs, it->holdNs + delayNs + kGrowMarginNs);
            if (needed > m_holdNs) { setHold(needed); }
            if (!allConcealed || it->holdNs + delayNs > kMaxHoldNs || back > m_nextIndex) {
                return Admission::LateConcealed;
            }
            m_nextIndex -= back;
            m_rewound += back;
            m_nextTimestamp = timestamp;
            m_released.erase(std::prev(it.base()), m_released.end());
            m_packets.try_emplace(m_nextIndex, Entry{packet, arrivalNs + m_holdNs});
            return Admission::Rewound;
        }
        return Admission::Late;
    }
    if (delta % m_packetFrames != 0) { return Admission::Invalid; }
    const int ahead = delta / m_packetFrames;
    if (ahead >= m_maxPackets) { return Admission::OutsideWindow; }
    const auto [unused, inserted] = m_packets.try_emplace(
        m_nextIndex + static_cast<quint64>(ahead), Entry{packet, arrivalNs + m_holdNs});
    Q_UNUSED(unused);
    return inserted ? Admission::Accepted : Admission::Duplicate;
}

std::optional<AudioJitterBuffer::Playout> AudioJitterBuffer::takeReady(qint64 nowNs)
{
    tick(nowNs);
    auto first = m_packets.begin();
    std::optional<qint64> due;
    const bool present = first != m_packets.end() && first->first == m_nextIndex;
    if (present) {
        // Preserve the producer's clock: received blocks are released at
        // their own arrival time plus the hold interval. A fixed packet-period
        // software playout clock here would hide clock drift from rmatch
        // and instead let this queue grow until it periodically dropped.
        due = first->second.dueNs;
    } else if (first != m_packets.end()) {
        due = first->second.dueNs
            - static_cast<qint64>(first->first - m_nextIndex) * m_packetDurationNs;
    } else {
        due = m_nextMissingDue;
    }
    if (!due || nowNs < *due) { return std::nullopt; }
    Playout result;
    result.timestamp = m_nextTimestamp;
    if (present) {
        result.packet = std::move(first->second.packet);
        m_packets.erase(first);
    }
    ++m_nextIndex;
    m_nextTimestamp += quint32(m_packetFrames);
    // If bounded downstream backpressure delayed a burst, do not manufacture
    // a catch-up run of PLC after its final packet. Present packets still
    // keep their arrival clock; only an empty-queue loss deadline follows
    // the actual last release.
    m_nextMissingDue = std::max(*due, nowNs) + m_packetDurationNs;
    noteRelease(result.timestamp, nowNs, !present);
    return result;
}

std::optional<AudioJitterBuffer::Playout> AudioJitterBuffer::concealExpectedNow(qint64 nowNs)
{
    // The expected packet missing: a hole (a later one queued), or an
    // empty queue while the hold is deepened and something has played. A
    // deeper hold runs playback on demand, with the matcher near empty, so
    // a gap in arrivals is concealed before the matcher underflows into a
    // restart; the loss deadline would come too late. At the base hold an
    // empty queue keeps its deadline and the matcher's own accounting (a
    // device outrunning the stream is still a clock-buffer fault). A true
    // outage is still ended by the no-packet rule.
    const auto first = m_packets.begin();
    if (first != m_packets.end() ? first->first == m_nextIndex
                                 : (!m_nextMissingDue || m_holdNs <= kHoldNs)) {
        return std::nullopt;
    }
    Playout result;
    result.timestamp = m_nextTimestamp;
    ++m_nextIndex;
    m_nextTimestamp += quint32(m_packetFrames);
    // The next loss deadline follows this concealment, as a deadline
    // release's does; a queued future packet keeps its own anchor.
    m_nextMissingDue = nowNs + m_packetDurationNs;
    noteRelease(result.timestamp, nowNs, true);
    return result;
}

int AudioJitterBuffer::advanceToFit(quint32 timestamp)
{
    const qint32 delta = std::bit_cast<qint32>(quint32(timestamp - m_nextTimestamp));
    if (delta < 0 || delta % m_packetFrames != 0) { return 0; }
    const int ahead = delta / m_packetFrames;
    if (ahead < m_maxPackets) { return 0; }
    // Nothing queued: move to the packet itself (no run of concealment for
    // intervals that were never coming).
    const auto skip = static_cast<quint64>(m_packets.empty() ? ahead : ahead - (m_maxPackets - 1));
    m_nextIndex += skip;
    m_nextTimestamp += quint32(skip) * quint32(m_packetFrames);
    int dropped = 0;
    while (!m_packets.empty() && m_packets.begin()->first < m_nextIndex) {
        m_packets.erase(m_packets.begin());
        ++dropped;
    }
    m_trimmed += quint64(dropped);
    m_skipped += skip;
    // The skipped intervals were never heard: nothing to find later.
    m_released.clear();
    return dropped;
}

std::optional<AudioJitterBuffer::Playout> AudioJitterBuffer::takeExpectedPresentEarly(
    std::optional<qint64> nowNs)
{
    auto first = m_packets.begin();
    if (first == m_packets.end() || first->first != m_nextIndex) {
        return std::nullopt;
    }

    Playout result;
    result.packet = std::move(first->second.packet);
    result.timestamp = m_nextTimestamp;
    const qint64 originalDue = first->second.dueNs;
    m_packets.erase(first);
    ++m_nextIndex;
    m_nextTimestamp += quint32(m_packetFrames);
    // Demand release changes only when usable PCM becomes available. Keep the
    // producer-derived due time as the empty-queue missing deadline; a queued
    // future packet retains the same anchor precedence as takeReady(). Using
    // the early wall-clock release here would silently create a local clock.
    // R-R3-21: never later than now plus the base hold. At the base hold
    // that is the producer-derived deadline itself (a packet due at arrival
    // plus kHoldNs cannot be taken before it arrived); only a deepened
    // hold's far deadline is brought in, so a stall right after demand
    // release is concealed before the rate matcher runs dry.
    const qint64 anchor = nowNs ? std::min(originalDue, *nowNs + kHoldNs) : originalDue;
    m_nextMissingDue = anchor + m_packetDurationNs;
    noteRelease(result.timestamp, anchor, false);
    return result;
}
} // namespace NereusSDR
