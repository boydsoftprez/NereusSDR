// =================================================================
// src/gui/TelemetryHistory.cpp  (NereusSDR)
// =================================================================
//
// Ported and adapted from AetherSDR's network-diagnostics history behavior:
//   src/gui/NetworkDiagnosticsDialog.cpp, MemoryHistoryRing.h
//   @ 0dea0dd7d73e25a40c8c01d46873af5834e23921
//   https://github.com/ten9876/AetherSDR
// AetherSDR project lead: Jeremy Fielder (KK7GWY, ten9876).
// Upstream has no per-file GPL header; its project-level GNU GPLv3 LICENSE
// applies.  Nereus adds explicit segment/missing breaks and exact weights so
// remote-session lifecycle changes cannot invent a continuous measurement.
//
// Modification history (NereusSDR):
//   2026-09-21 -- R-R3-32/33 implementation by J.J. Boyd (KG4VCF), with
//                 AI-assisted adaptation via OpenAI Codex.
// =================================================================

#include "TelemetryHistory.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace NereusSDR {

std::size_t TelemetryHistory::metricIndex(Metric metric)
{
    const std::size_t index = static_cast<std::size_t>(metric);
    Q_ASSERT(index < kMetricCount);
    return index;
}

qint64 TelemetryHistory::minuteStartMs(qint64 monotonicMs)
{
    constexpr qint64 kMinuteMs = 60LL * 1000LL;
    return monotonicMs >= 0
        ? (monotonicMs / kMinuteMs) * kMinuteMs
        : ((monotonicMs - kMinuteMs + 1) / kMinuteMs) * kMinuteMs;
}

qint64 TelemetryHistory::bucketMsFor(int rangeSeconds)
{
    return rangeSeconds <= 5 * 60
        ? 1000
        : std::max<qint64>(5000, (static_cast<qint64>(rangeSeconds) * 1000) / 300);
}

double TelemetryHistory::connectGapSecondsFor(int rangeSeconds)
{
    return 3.0 * static_cast<double>(bucketMsFor(rangeSeconds)) / 1000.0;
}

void TelemetryHistory::breakMetric(Metric metric)
{
    m_metrics[metricIndex(metric)].pendingBreak = true;
}

void TelemetryHistory::append(const Sample& sample, MetricMask updated)
{
    // The contract is a monotonic GUI clock. Ignoring an old callback avoids
    // reordering an already compacted history and turning it into a false line.
    if (sample.monotonicMs < m_latestMonotonicMs) {
        return;
    }
    m_latestMonotonicMs = sample.monotonicMs;

    for (std::size_t index = 0; index < kMetricCount; ++index) {
        MetricHistory& history = m_metrics[index];
        if (!updated.test(index)) {
            compactAndPrune(history, sample.monotonicMs);
            continue;
        }
        const std::optional<double>& value = sample.values[index];
        if (!value.has_value() || !std::isfinite(*value)) {
            // Missing is not zero. The next valid reading must not join to a
            // preceding reading even if the outage was shorter than a gap rule.
            history.pendingBreak = true;
            compactAndPrune(history, sample.monotonicMs);
            continue;
        }

        const bool changedSegment = history.lastSegment.has_value()
            && *history.lastSegment != sample.sessionSegment;
        const bool sourceGap = history.lastObservationMs.has_value()
            && sample.monotonicMs - *history.lastObservationMs
                > 3 * kExpectedSampleMs;
        history.raw.push_back({sample.monotonicMs,
                               sample.sessionSegment,
                               history.pendingBreak || changedSegment || sourceGap,
                               false,
                               *value,
                               1});
        history.pendingBreak = false;
        history.lastSegment = sample.sessionSegment;
        history.lastObservationMs = sample.monotonicMs;
        compactAndPrune(history, sample.monotonicMs);
    }
}

void TelemetryHistory::compactAndPrune(MetricHistory& history, qint64 latestMs)
{
    const qint64 rawCutoff = latestMs - kRawRetentionMs;
    while (!history.raw.empty()
           && (history.raw.front().timestampMs < rawCutoff
               || static_cast<int>(history.raw.size()) > kRawObservationCapacity)) {
        compactOne(history);
    }

    const qint64 minuteCutoff = latestMs - kMinuteRetentionMs;
    while (!history.minutes.empty() && history.minutes.front().timestampMs < minuteCutoff) {
        history.minutes.pop_front();
    }
    while (static_cast<int>(history.minutes.size()) > kMinuteObservationCapacity) {
        history.minutes.pop_front();
    }
}

void TelemetryHistory::compactOne(MetricHistory& history)
{
    Q_ASSERT(!history.raw.empty());
    Observation observation = history.raw.front();
    history.raw.pop_front();
    observation.timestampMs = minuteStartMs(observation.timestampMs);
    appendMinute(history, observation);
}

void TelemetryHistory::appendMinute(MetricHistory& history, Observation observation)
{
    if (history.minutes.empty() || history.minutes.back().timestampMs != observation.timestampMs) {
        // The output after a conservatively omitted minute must begin a new
        // graph path. This is not a synthetic observation and does not grow
        // reconnect-only histories.
        if (!history.minutes.empty() && history.minutes.back().invalidMinute) {
            observation.breakBefore = true;
        }
        history.minutes.push_back(std::move(observation));
        return;
    }

    Observation& current = history.minutes.back();
    if (current.invalidMinute) {
        return;
    }

    // A minute that crosses a session or explicit break cannot be safely
    // averaged. Keep one invalid marker rather than one record per reconnect;
    // query omits it and the following minute is forced to start a new path.
    if (observation.breakBefore || current.segment != observation.segment) {
        current.mean = 0.0;
        current.weight = 0;
        current.breakBefore = true;
        current.invalidMinute = true;
        return;
    }

    // A convex interpolation retains the exact observation weights without
    // overflowing a sum of individually finite measurements.
    current.mean = std::lerp(current.mean, observation.mean,
        double(observation.weight) / double(current.weight + observation.weight));
    current.weight += observation.weight;
}

TelemetryHistory::Series TelemetryHistory::series(
    Metric metric, qint64 nowMonotonicMs, int rangeSeconds) const
{
    Series result;
    if (rangeSeconds <= 0) {
        return result;
    }
    result.maxConnectGapSeconds = connectGapSecondsFor(rangeSeconds);

    const MetricHistory& history = m_metrics[metricIndex(metric)];
    if (history.raw.empty() && history.minutes.empty()) {
        return result;
    }

    const qint64 periodMs = bucketMsFor(rangeSeconds);
    const qint64 endMs = periodMs <= 1000
        ? nowMonotonicMs
        : (nowMonotonicMs / periodMs) * periodMs;
    const qint64 cutoffMs = endMs - static_cast<qint64>(rangeSeconds) * 1000;

    std::vector<Observation> records;
    records.reserve(history.raw.size() + history.minutes.size());
    for (const Observation& observation : history.minutes) {
        const qint64 centreMs = observation.timestampMs + 30LL * 1000LL;
        if (centreMs >= cutoffMs && centreMs <= endMs) {
            Observation point = observation;
            point.timestampMs = centreMs; // minute aggregate centre
            records.push_back(point);
        }
    }
    for (const Observation& observation : history.raw) {
        if (observation.timestampMs >= cutoffMs && observation.timestampMs <= endMs) {
            records.push_back(observation);
        }
    }
    std::stable_sort(records.begin(), records.end(), [](const Observation& left, const Observation& right) {
        return left.timestampMs < right.timestampMs;
    });

    if (periodMs <= 1000) {
        result.points.reserve(static_cast<int>(records.size()));
        for (const Observation& observation : records) {
            if (observation.invalidMinute) {
                continue;
            }
            result.points.push_back({
                static_cast<double>(observation.timestampMs - cutoffMs) / 1000.0,
                observation.mean,
                observation.breakBefore,
            });
        }
        return result;
    }

    bool haveBucket = false;
    bool bucketInvalid = false;
    bool forceNextBreak = false;
    qint64 outputBucketStart = 0;
    Segment outputSegment = 0;
    bool outputBreakBefore = false;
    double mean = 0.0;
    quint64 weight = 0;
    auto flush = [&] {
        if (!haveBucket) {
            return;
        }
        if (bucketInvalid || weight == 0) {
            // Long views have one output slot per time bucket. Omitting a
            // mixed bucket avoids both a false line and rapid-reconnect
            // output growth; the next real bucket starts a new path.
            forceNextBreak = true;
        } else {
            const qint64 centreMs = outputBucketStart + periodMs / 2;
            if (centreMs < cutoffMs || centreMs > endMs) {
                // A capacity-forced minute aggregate can fall in a partial
                // edge bucket whose centre is outside this visible window.
                forceNextBreak = true;
            } else {
                result.points.push_back({
                    static_cast<double>(centreMs - cutoffMs) / 1000.0,
                    mean,
                    outputBreakBefore || forceNextBreak,
                });
                forceNextBreak = false;
            }
        }
        haveBucket = false;
        bucketInvalid = false;
        mean = 0.0;
        weight = 0;
    };

    for (const Observation& observation : records) {
        const qint64 queryBucketStart = (observation.timestampMs / periodMs) * periodMs;
        if (!haveBucket || queryBucketStart != outputBucketStart) {
            flush();
            haveBucket = true;
            outputBucketStart = queryBucketStart;
            outputSegment = observation.segment;
            outputBreakBefore = observation.breakBefore;
            bucketInvalid = observation.invalidMinute;
        } else if (observation.invalidMinute || observation.breakBefore
                   || observation.segment != outputSegment) {
            // The break belongs inside this time bucket, so no single value
            // represents it. Keep the bucket invalid until its boundary.
            bucketInvalid = true;
        }
        if (!bucketInvalid && !observation.invalidMinute) {
            mean = weight == 0 ? observation.mean : std::lerp(mean, observation.mean,
                double(observation.weight) / double(weight + observation.weight));
            weight += observation.weight;
        }
    }
    flush();
    return result;
}

int TelemetryHistory::rawObservationCount(Metric metric) const
{
    return static_cast<int>(m_metrics[metricIndex(metric)].raw.size());
}

int TelemetryHistory::minuteObservationCount(Metric metric) const
{
    return static_cast<int>(m_metrics[metricIndex(metric)].minutes.size());
}

} // namespace NereusSDR
