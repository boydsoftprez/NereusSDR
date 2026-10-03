// =================================================================
// tests/tst_telemetry_history.cpp  (NereusSDR)
// =================================================================
// R-R3-32/33 GUI telemetry-history regressions. The history is deliberately
// protocol-neutral: these tests use only monotonic timestamps, segments and
// optional values, never a station transport or media receiver.
// =================================================================

#include <QtTest/QtTest>
#include <array>
#include <limits>
#include <cmath>

#include "gui/TelemetryHistory.h"

using namespace NereusSDR;

namespace {

TelemetryHistory::Sample sample(qint64 timeMs, TelemetryHistory::Segment segment,
                                TelemetryHistory::Metric metric,
                                std::optional<double> value)
{
    TelemetryHistory::Sample result;
    result.monotonicMs = timeMs;
    result.sessionSegment = segment;
    result.values[static_cast<std::size_t>(metric)] = value;
    return result;
}

const TelemetryHistory::Point* pointWithValue(const TelemetryHistory::Series& series,
                                              double value)
{
    for (const TelemetryHistory::Point& point : series.points) {
        if (qFuzzyCompare(point.value + 1.0, value + 1.0)) {
            return &point;
        }
    }
    return nullptr;
}

} // namespace

class TstTelemetryHistory : public QObject {
    Q_OBJECT

private slots:
    void independentSourcesDoNotBreakEachOthersHistory()
    {
        TelemetryHistory history;
        constexpr auto radio = TelemetryHistory::Metric::RadioRxMbps;
        constexpr auto playback = TelemetryHistory::Metric::PlaybackDecodedPacketsPerSecond;
        TelemetryHistory::MetricMask radioOnly, playbackOnly;
        radioOnly.set(static_cast<std::size_t>(radio));
        playbackOnly.set(static_cast<std::size_t>(playback));
        history.append(sample(0, 1, radio, 2.0), radioOnly);
        history.append(sample(100, 1, playback, 25.0), playbackOnly);
        history.append(sample(1000, 1, radio, 3.0), radioOnly);
        const auto series = history.series(radio, 1000, 60);
        QCOMPARE(series.points.size(), 2);
        QVERIFY(!series.points.last().breakBefore);
    }

    void finiteInputsCannotOverflowCompactedMean()
    {
        TelemetryHistory history;
        constexpr auto metric = TelemetryHistory::Metric::RadioRxMbps;
        const double high = std::numeric_limits<double>::max();
        for (int i = 0; i < 120; ++i) {
            history.append(sample(i * 1000, 1, metric, high));
        }
        const qint64 now = TelemetryHistory::kRawRetentionMs + 120000;
        history.append(sample(now, 1, metric, std::nullopt));
        const auto series = history.series(metric, now, 86400);
        QVERIFY(!series.points.isEmpty());
        for (const auto& point : series.points) {
            QVERIFY(std::isfinite(point.value));
            QCOMPARE(point.value, high);
        }
    }

    void missingDoesNotBecomeZeroAndForcesBreak()
    {
        TelemetryHistory history;
        constexpr auto metric = TelemetryHistory::Metric::RadioRxMbps;
        history.append(sample(0, 1, metric, 5.0));
        history.append(sample(1000, 1, metric, std::nullopt));
        history.append(sample(2000, 1, metric, 7.0));

        const auto series = history.series(metric, 2000, 5 * 60);
        QCOMPARE(series.points.size(), 2);
        QCOMPARE(series.points[0].value, 5.0);
        QCOMPARE(series.points[1].value, 7.0);
        QVERIFY(series.points[1].breakBefore);
    }

    void zeroIsAValidMeasuredObservation()
    {
        TelemetryHistory history;
        constexpr auto metric = TelemetryHistory::Metric::AudioSendRejectedPerSecond;
        history.append(sample(1000, 1, metric, 0.0));

        const auto series = history.series(metric, 1000, 5 * 60);
        QCOMPARE(series.points.size(), 1);
        QCOMPARE(series.points[0].value, 0.0);
    }

    void compactionKeepsObservationWeightsThroughQueryBucketing()
    {
        TelemetryHistory history;
        constexpr auto metric = TelemetryHistory::Metric::SessionPayloadRxKbps;
        // Minute zero contains sixty continuous observations whose sum is 100;
        // minute one contains one further continuous observation of 100.
        for (int second = 0; second < 60; ++second) {
            history.append(sample(second * 1000, 1, metric, second == 59 ? 100.0 : 0.0));
        }
        history.append(sample(60000, 1, metric, 100.0));
        const qint64 trigger = TelemetryHistory::kRawRetentionMs + 61000;
        history.append(sample(trigger, 1, metric, 1.0));

        const auto series = history.series(metric, trigger, 7 * 24 * 60 * 60);
        const auto* weighted = pointWithValue(series, 200.0 / 61.0);
        QVERIFY2(weighted, "query aggregation must retain the compacted observations' weights");
    }

    void reconnectStartsNewLineEvenWhenTheTimeGapIsShort()
    {
        TelemetryHistory history;
        constexpr auto metric = TelemetryHistory::Metric::PlaybackDecodedPacketsPerSecond;
        history.append(sample(0, 1, metric, 20.0));
        history.append(sample(1000, 1, metric, 21.0));
        history.append(sample(2000, 2, metric, 22.0));

        const auto series = history.series(metric, 2000, 5 * 60);
        QCOMPARE(series.points.size(), 3);
        QVERIFY(!series.points[1].breakBefore);
        QVERIFY(series.points[2].breakBefore);
    }

    void compactedMinuteCrossingBreakIsOmittedAndCannotGrowPerReconnect()
    {
        TelemetryHistory history;
        constexpr auto metric = TelemetryHistory::Metric::PlaybackLatePacketsPerSecond;
        history.append(sample(0, 1, metric, 1.0));       // valid minute 0
        history.append(sample(60000, 1, metric, 2.0));
        history.append(sample(61000, 2, metric, 3.0));   // minute 1 crosses segment
        history.append(sample(4032000, 2, metric, 4.0)); // later long-query bucket
        const qint64 trigger = TelemetryHistory::kRawRetentionMs + 4033000;
        history.append(sample(trigger, 2, metric, 5.0)); // compacts minutes 0..2

        QCOMPARE(history.minuteObservationCount(metric), 3);
        const auto series = history.series(metric, trigger, 7 * 24 * 60 * 60);
        const auto* after = pointWithValue(series, 4.0);
        QVERIFY(after);
        QVERIFY(after->breakBefore);
        QVERIFY(!pointWithValue(series, 1.0));
        QVERIFY(!pointWithValue(series, 2.0));
        QVERIFY(!pointWithValue(series, 3.0));

        for (int i = 0; i < TelemetryHistory::kRawObservationCapacity * 3; ++i) {
            history.append(sample(trigger + 1 + i,
                                  static_cast<TelemetryHistory::Segment>(100 + i), metric, 1.0));
        }
        QVERIFY(history.rawObservationCount(metric) <= TelemetryHistory::kRawObservationCapacity);
        QVERIFY(history.minuteObservationCount(metric) <= TelemetryHistory::kMinuteObservationCapacity);

        const int rangeSeconds = 7 * 24 * 60 * 60;
        const auto bounded = history.series(metric, TelemetryHistory::kMinuteRetentionMs,
                                            rangeSeconds);
        QVERIFY(bounded.points.size()
                <= rangeSeconds * 1000LL / TelemetryHistory::bucketMsFor(rangeSeconds));
    }

    void sourceGapIsRetainedThroughCompactionAndQueryAggregation()
    {
        TelemetryHistory history;
        constexpr auto metric = TelemetryHistory::Metric::PlaybackUnderflowsPerSecond;
        history.append(sample(0, 1, metric, 1.0));
        history.append(sample(65000, 1, metric, 2.0)); // No callbacks for > 3 periods.
        history.append(sample(4032000, 1, metric, 3.0));
        const qint64 trigger = TelemetryHistory::kRawRetentionMs + 4033000;
        history.append(sample(trigger, 1, metric, 4.0));

        const auto series = history.series(metric, trigger, 7 * 24 * 60 * 60);
        const auto* afterGap = pointWithValue(series, 3.0);
        QVERIFY(afterGap);
        QVERIFY(afterGap->breakBefore);
        QVERIFY(!pointWithValue(series, 1.0));
        QVERIFY(!pointWithValue(series, 2.0));
    }

    void forcedRecentMinuteCentreDoesNotEscapeTheVisibleRange()
    {
        TelemetryHistory history;
        constexpr auto metric = TelemetryHistory::Metric::RadioTxMbps;
        for (int i = 0; i <= TelemetryHistory::kRawObservationCapacity; ++i) {
            history.append(sample(i * 2, 1, metric, 1.0));
        }
        const auto series = history.series(metric, 10000, 10 * 60);
        for (const auto& point : series.points) {
            QVERIFY(point.seconds >= 0.0);
            QVERIFY(point.seconds <= 10 * 60);
        }
    }

    void retentionDropsObservationsOlderThanSevenDays()
    {
        TelemetryHistory history;
        constexpr auto metric = TelemetryHistory::Metric::SessionRttMs;
        history.append(sample(0, 1, metric, 1.0));
        const qint64 now = TelemetryHistory::kMinuteRetentionMs + 1000;
        history.append(sample(now, 1, metric, 2.0));

        QCOMPARE(history.minuteObservationCount(metric), 0);
        const auto series = history.series(metric, now, 5 * 60);
        QCOMPARE(series.points.size(), 1);
        QCOMPARE(series.points[0].value, 2.0);
    }

    void explicitPerMetricBreakStartsTheNextObservation()
    {
        TelemetryHistory history;
        constexpr auto metric = TelemetryHistory::Metric::AudioSourceDropsPerSecond;
        history.append(sample(0, 1, metric, 1.0));
        history.breakMetric(metric);
        history.append(sample(1000, 1, metric, 2.0));

        const auto series = history.series(metric, 1000, 5 * 60);
        QCOMPARE(series.points.size(), 2);
        QVERIFY(series.points[1].breakBefore);
    }

    void longWindowPointsUseBucketCentres()
    {
        TelemetryHistory history;
        constexpr auto metric = TelemetryHistory::Metric::RadioRttMs;
        history.append(sample(6000, 1, metric, 12.0));

        // 10 minutes uses max(5 seconds, range / 300) = 5 seconds. The 6000 ms
        // observation belongs to [5000,10000), whose chart point is 7.5 s.
        const auto series = history.series(metric, 600000, 10 * 60);
        QCOMPARE(series.points.size(), 1);
        QCOMPARE(series.points[0].seconds, 7.5);
        QCOMPARE(series.points[0].value, 12.0);
    }

    // R-R3-32/33: each Core computer value is its own series. A measured zero
    // is kept, an absent value breaks only its own line, and a sample that
    // updates only the Core values leaves the other series continuous.
    // R-R3-35: the measured delay, its accuracy and the delivery delay are
    // their own lines. A sample where the delay was not measured (echoes
    // stopped, a new audio context) is a gap, never zero, and a reconnect
    // (a new session segment) starts a new line even one second later.
    void audioDelayMetricsGapWhenNotMeasuredAndAcrossReconnects()
    {
        using Metric = TelemetryHistory::Metric;
        const std::array<Metric, 3> delay{Metric::AudioDelayMs, Metric::AudioDelayAccuracyMs,
                                          Metric::AudioDeliveryDelayMs};
        TelemetryHistory::MetricMask delayOnly;
        for (const Metric metric : delay) {
            delayOnly.set(static_cast<std::size_t>(metric));
        }
        const auto measured = [&](qint64 timeMs, TelemetryHistory::Segment segment,
                                  double delayMs) {
            TelemetryHistory::Sample result{timeMs, segment, {}};
            result.values[static_cast<std::size_t>(Metric::AudioDelayMs)] = delayMs;
            result.values[static_cast<std::size_t>(Metric::AudioDelayAccuracyMs)] = 0.5;
            result.values[static_cast<std::size_t>(Metric::AudioDeliveryDelayMs)] = delayMs - 20.0;
            return result;
        };
        TelemetryHistory history;
        history.append(measured(0, 1, 85.0), delayOnly);
        history.append(measured(1000, 1, 86.0), delayOnly);
        history.append({2000, 1, {}}, delayOnly);           // not measured
        history.append(measured(3000, 1, 87.0), delayOnly);
        history.append(measured(4000, 2, 88.0), delayOnly); // reconnected
        for (const Metric metric : delay) {
            const auto series = history.series(metric, 4000, 60);
            QCOMPARE(series.points.size(), 4);
            QVERIFY(!series.points[1].breakBefore);
            QVERIFY(series.points[2].breakBefore);
            QVERIFY(series.points[3].breakBefore);
        }
        QCOMPARE(history.series(Metric::AudioDelayMs, 4000, 60).points.constLast().value, 88.0);
        QCOMPARE(history.series(Metric::AudioDeliveryDelayMs, 4000, 60).points.constLast().value,
                 68.0);
        QCOMPARE(history.series(Metric::AudioDelayAccuracyMs, 4000, 60).points.constLast().value,
                 0.5);
        // Other metrics are untouched by these samples.
        QVERIFY(history.series(Metric::SpeakerBufferMs, 4000, 60).points.isEmpty());
    }

    void coreHostMetricsKeepZerosGapsAndTheirOwnLines()
    {
        using Metric = TelemetryHistory::Metric;
        const std::array<Metric, 5> host{
            Metric::CoreSystemCpuPercent, Metric::CoreProcessCpuPercent,
            Metric::CoreMemoryAvailableMiB, Metric::CoreProcessResidentMiB,
            Metric::CoreHottestZoneCelsius};
        TelemetryHistory::MetricMask hostOnly;
        for (const Metric metric : host) {
            hostOnly.set(static_cast<std::size_t>(metric));
        }
        TelemetryHistory::MetricMask radioOnly;
        radioOnly.set(static_cast<std::size_t>(Metric::RadioRxMbps));

        TelemetryHistory history;
        history.append(sample(0, 1, Metric::RadioRxMbps, 2.0), radioOnly);
        TelemetryHistory::Sample first{0, 1, {}};
        TelemetryHistory::Sample second{1000, 1, {}};
        TelemetryHistory::Sample third{2000, 1, {}};
        double value = 10.0;
        for (const Metric metric : host) {
            const auto slot = static_cast<std::size_t>(metric);
            first.values[slot] = 0.0;
            third.values[slot] = value;
            value += 10.0;
        }
        // Only the process CPU is absent in the middle sample.
        for (const Metric metric : host) {
            if (metric != Metric::CoreProcessCpuPercent) {
                second.values[static_cast<std::size_t>(metric)] = 1.0;
            }
        }
        history.append(first, hostOnly);
        history.append(second, hostOnly);
        history.append(third, hostOnly);
        history.append(sample(2000, 1, Metric::RadioRxMbps, 3.0), radioOnly);

        value = 10.0;
        for (const Metric metric : host) {
            const auto series = history.series(metric, 2000, 60);
            if (metric == Metric::CoreProcessCpuPercent) {
                QCOMPARE(series.points.size(), 2);
                QVERIFY(series.points[1].breakBefore);
            } else {
                QCOMPARE(series.points.size(), 3);
                QCOMPARE(series.points[1].value, 1.0);
                QVERIFY(!series.points[1].breakBefore);
                QVERIFY(!series.points[2].breakBefore);
            }
            QCOMPARE(series.points.constFirst().value, 0.0);
            QCOMPARE(series.points.constLast().value, value);
            value += 10.0;
        }
        const auto radio = history.series(Metric::RadioRxMbps, 2000, 60);
        QCOMPARE(radio.points.size(), 2);
        QVERIFY(!radio.points[1].breakBefore);
    }

    // R-R3-40: each receiver slot is its own series, keyed by slice ID. Loads
    // over 100 % are kept as measured, and an idle receiver (absent load)
    // breaks only its own line.
    void receiverLoadSlotsAreSeparateSeries()
    {
        using Metric = TelemetryHistory::Metric;
        QCOMPARE(TelemetryHistory::kCoreReceiverLoadSlots, 5);
        QCOMPARE(TelemetryHistory::coreReceiverLoadMetric(0),
                 Metric::CoreReceiverLoadPercentSlot0);
        QCOMPARE(TelemetryHistory::coreReceiverLoadMetric(4),
                 Metric::CoreReceiverLoadPercentSlot4);
        const Metric a = TelemetryHistory::coreReceiverLoadMetric(0);
        const Metric c = TelemetryHistory::coreReceiverLoadMetric(2);
        TelemetryHistory::MetricMask receivers;
        for (int slot = 0; slot < TelemetryHistory::kCoreReceiverLoadSlots; ++slot) {
            receivers.set(static_cast<std::size_t>(TelemetryHistory::coreReceiverLoadMetric(slot)));
        }

        TelemetryHistory history;
        TelemetryHistory::Sample first{0, 1, {}};
        first.values[static_cast<std::size_t>(a)] = 40.0;
        first.values[static_cast<std::size_t>(c)] = 90.0;
        TelemetryHistory::Sample second{1000, 1, {}};
        second.values[static_cast<std::size_t>(a)] = 45.0; // slice C idle here
        TelemetryHistory::Sample third{2000, 1, {}};
        third.values[static_cast<std::size_t>(a)] = 50.0;
        third.values[static_cast<std::size_t>(c)] = 135.0;
        history.append(first, receivers);
        history.append(second, receivers);
        history.append(third, receivers);

        const auto sliceA = history.series(a, 2000, 60);
        QCOMPARE(sliceA.points.size(), 3);
        QVERIFY(!sliceA.points[1].breakBefore);
        QVERIFY(!sliceA.points[2].breakBefore);
        const auto sliceC = history.series(c, 2000, 60);
        QCOMPARE(sliceC.points.size(), 2);
        QCOMPARE(sliceC.points[1].value, 135.0);
        QVERIFY(sliceC.points[1].breakBefore);
        QVERIFY(history.series(TelemetryHistory::coreReceiverLoadMetric(1), 2000, 60)
                    .points.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstTelemetryHistory)
#include "tst_telemetry_history.moc"
