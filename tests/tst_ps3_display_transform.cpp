// no-port-check: NereusSDR-original source-backed PS3 display transform tests.

#include <QtTest/QtTest>

#include "core/dsp/Ps3DisplayAdapter.h"

#include <cmath>
#include <limits>

using namespace NereusSDR;

namespace {

Ps3Snapshot knownSnapshot()
{
    Ps3Snapshot snapshot;
    snapshot.sampleCount = 3;
    snapshot.correctionCount = 2;
    snapshot.x = {0.2, 0.4, 0.8};
    snapshot.ym = {0.5, 0.0, 0.25};
    snapshot.yc = {1.0, -1.0, 0.0};
    snapshot.ys = {0.0, 0.0, 1.0};
    snapshot.xmCorrection = {0.15, 0.65};
    snapshot.ymCorrection = {1.5, 0.75};
    snapshot.xaCorrection = {0.35, 0.95};
    snapshot.yaCorrection = {-181.0, 181.0};
    snapshot.phaseReferenceDegrees = 10.0;
    return snapshot;
}

} // namespace

class TstPs3DisplayTransform : public QObject {
    Q_OBJECT

private slots:
    void correctionSummaryUsesMeasuredGainAndWholePhaseCurve()
    {
        const auto summary = Ps3DisplayAdapter::correctionSummary(knownSnapshot());
        QVERIFY(summary);
        QCOMPARE(summary->gainAtPeak, 0.75);
        QCOMPARE(summary->phaseSpanDegrees, 362.0);
        Ps3Snapshot snapshot = knownSnapshot();
        snapshot.xmCorrection = {0.9, 0.2};
        QCOMPARE(Ps3DisplayAdapter::correctionSummary(snapshot)->gainAtPeak, 1.5);
    }

    void correctionSummaryRejectsUnavailableOrInvalidCurves()
    {
        QVERIFY(!Ps3DisplayAdapter::correctionSummary(Ps3Snapshot{}));
        Ps3Snapshot snapshot = knownSnapshot();
        snapshot.yaCorrection.pop_back();
        QVERIFY(!Ps3DisplayAdapter::correctionSummary(snapshot));
        snapshot = knownSnapshot();
        snapshot.ymCorrection[1] = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!Ps3DisplayAdapter::correctionSummary(snapshot));
        snapshot.ymCorrection[1] = 0.0;
        QVERIFY(!Ps3DisplayAdapter::correctionSummary(snapshot));
    }

    void knownSnapshotPreservesDistinctCorrectionCoordinates()
    {
        const Ps3PlotData plot = Ps3DisplayAdapter::transform(knownSnapshot());

        QCOMPARE(plot.measuredMagnitude.size(), std::size_t{3});
        QCOMPARE(plot.measuredMagnitude.at(0).x, 0.1);
        QCOMPARE(plot.measuredMagnitude.at(0).y, 0.2);
        QCOMPARE(plot.correctionMagnitude.size(), std::size_t{2});
        QCOMPARE(plot.correctionMagnitude.at(0).x, 0.15);
        QCOMPARE(plot.correctionMagnitude.at(0).y, 0.225);
        QCOMPARE(plot.correctionPhase.size(), std::size_t{2});
        QCOMPARE(plot.correctionPhase.at(0).x, 0.35);
        QCOMPARE(plot.correctionPhase.at(1).x, 0.95);
        QVERIFY(plot.correctionMagnitude.at(0).x != plot.correctionPhase.at(0).x);
    }

    void sourcePhaseReferenceAndWrappingAreExact()
    {
        const Ps3PlotData plot = Ps3DisplayAdapter::transform(knownSnapshot());
        QCOMPARE(plot.measuredPhase.size(), std::size_t{3});
        QCOMPARE(plot.measuredPhase.at(0).y, -10.0);
        QCOMPARE(plot.measuredPhase.at(1).y, 170.0);
        QCOMPARE(plot.measuredPhase.at(2).y, 80.0);
        QCOMPARE(Ps3DisplayAdapter::wrap180(180.0), -180.0);
        QCOMPARE(Ps3DisplayAdapter::wrap180(-180.0), -180.0);
        QCOMPARE(Ps3DisplayAdapter::wrap180(540.0), -180.0);
    }

    void zeroInputNeverPublishesNonFiniteGain()
    {
        const Ps3PlotData plot = Ps3DisplayAdapter::transform(knownSnapshot());
        QCOMPARE(plot.measuredGain.size(), std::size_t{2});
        for (const Ps3PlotPoint& point : plot.measuredGain) {
            QVERIFY(std::isfinite(point.x));
            QVERIFY(std::isfinite(point.y));
        }
        for (const Ps3PlotPoint& point : plot.measuredMagnitude) {
            QVERIFY(std::isfinite(point.x));
            QVERIFY(std::isfinite(point.y));
        }
    }

    void invalidVectorCountsProduceNoPlotData()
    {
        Ps3Snapshot snapshot = knownSnapshot();
        snapshot.x.pop_back();
        const Ps3PlotData plot = Ps3DisplayAdapter::transform(snapshot);
        QVERIFY(plot.measuredMagnitude.empty());
        QVERIFY(plot.measuredGain.empty());
        QVERIFY(plot.measuredPhase.empty());
        QVERIFY(plot.correctionMagnitude.empty());
        QVERIFY(plot.correctionGain.empty());
        QVERIFY(plot.correctionPhase.empty());
    }
};

QTEST_APPLESS_MAIN(TstPs3DisplayTransform)
#include "tst_ps3_display_transform.moc"
