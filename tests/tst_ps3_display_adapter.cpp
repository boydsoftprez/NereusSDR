// no-port-check: NereusSDR-original contract tests.  Expected display
// transforms are hand-derived from the pinned TAPR calcc.c data contract.

#include <QtTest/QtTest>

#include "core/dsp/Ps3DisplayAdapter.h"

#include <cmath>
#include <limits>

using namespace NereusSDR;

namespace {

enum class FakeMode {
    ZeroCounts,
    MaxCounts,
    TooManySamples,
    TooManyCorrections,
    NegativeSampleCount,
    NegativeCorrectionCount,
    NonFiniteSample,
    NonFiniteCorrection,
    NonFinitePhaseReference,
};

FakeMode g_fakeMode = FakeMode::ZeroCounts;

void fakeGetPsDisp(int,
                   double* x,
                   double* ym,
                   double* yc,
                   double* ys,
                   double* xmCor,
                   double* ymCor,
                   double* xaCor,
                   double* yaCor,
                   int* sampleCount,
                   int* correctionCount,
                   double* phaseReference)
{
    *phaseReference = 0.0;

    if (g_fakeMode == FakeMode::ZeroCounts) {
        *sampleCount = 0;
        *correctionCount = 0;
        return;
    }
    if (g_fakeMode == FakeMode::TooManySamples) {
        *sampleCount = Ps3Snapshot::kMaxSampleCount + 1;
        *correctionCount = 1;
        return;
    }
    if (g_fakeMode == FakeMode::TooManyCorrections) {
        *sampleCount = 1;
        *correctionCount = Ps3Snapshot::kMaxCorrectionCount + 1;
        return;
    }
    if (g_fakeMode == FakeMode::NegativeSampleCount) {
        *sampleCount = -1;
        *correctionCount = 1;
        return;
    }
    if (g_fakeMode == FakeMode::NegativeCorrectionCount) {
        *sampleCount = 1;
        *correctionCount = -1;
        return;
    }

    *sampleCount = Ps3Snapshot::kMaxSampleCount;
    *correctionCount = Ps3Snapshot::kMaxCorrectionCount;
    for (int i = 0; i < *sampleCount; ++i) {
        x[i] = static_cast<double>(i) / Ps3Snapshot::kMaxSampleCount;
        ym[i] = 1.0;
        yc[i] = 1.0;
        ys[i] = 0.0;
    }
    for (int i = 0; i < *correctionCount; ++i) {
        xmCor[i] = static_cast<double>(i) / Ps3Snapshot::kMaxCorrectionCount;
        ymCor[i] = 1.0;
        xaCor[i] = xmCor[i];
        yaCor[i] = 0.0;
    }
    if (g_fakeMode == FakeMode::NonFiniteSample) {
        ys[*sampleCount - 1] = std::numeric_limits<double>::quiet_NaN();
    }
    if (g_fakeMode == FakeMode::NonFiniteCorrection) {
        yaCor[*correctionCount - 1] =
            std::numeric_limits<double>::infinity();
    }
    if (g_fakeMode == FakeMode::NonFinitePhaseReference) {
        *phaseReference = std::numeric_limits<double>::quiet_NaN();
    }
}

Ps3Snapshot transformFixture()
{
    Ps3Snapshot snapshot;
    snapshot.x = {0.5, 0.25};
    snapshot.ym = {1.5, 0.0};
    snapshot.yc = {0.0, 1.0};
    snapshot.ys = {1.0, 0.0};
    snapshot.xmCorrection = {0.25};
    snapshot.ymCorrection = {2.0};
    snapshot.xaCorrection = {0.25};
    snapshot.yaCorrection = {-45.0};
    snapshot.phaseReferenceDegrees = 30.0;
    snapshot.sampleCount = 2;
    snapshot.correctionCount = 1;
    return snapshot;
}

} // namespace

class TestPs3DisplayAdapter : public QObject {
    Q_OBJECT

private slots:
    void captureRejectsZeroCounts();
    void captureRejectsMissingReaderAndInvalidChannel();
    void captureOwnsMaximumGeometryAndMetadata();
    void captureRejectsCountsOutsideBounds();
    void captureRejectsNonFiniteValues();
    void transformRejectsMalformedSnapshotCounts();
    void transformMatchesPinnedCalccContract();
    void transformSkipsZeroMeasuredGain();
    void wrap180UsesNegative180InclusiveConvention();
};

void TestPs3DisplayAdapter::captureRejectsZeroCounts()
{
    g_fakeMode = FakeMode::ZeroCounts;
    Ps3DisplayAdapter adapter(&fakeGetPsDisp);

    QVERIFY(!adapter.capture(3, 7, 11, 1234).has_value());
}

void TestPs3DisplayAdapter::captureRejectsMissingReaderAndInvalidChannel()
{
    Ps3DisplayAdapter missingReader(nullptr);
    QVERIFY(!missingReader.capture(3, 7, 11, 1234).has_value());

    Ps3DisplayAdapter adapter(&fakeGetPsDisp);
    QVERIFY(!adapter.capture(-1, 7, 11, 1234).has_value());
}

void TestPs3DisplayAdapter::captureOwnsMaximumGeometryAndMetadata()
{
    g_fakeMode = FakeMode::MaxCounts;
    Ps3DisplayAdapter adapter(&fakeGetPsDisp);

    const std::optional<Ps3Snapshot> snapshot = adapter.capture(3, 7, 11, 1234);

    QVERIFY(snapshot.has_value());
    QCOMPARE(snapshot->channelId, 3);
    QCOMPARE(snapshot->sessionGeneration, std::uint64_t{7});
    QCOMPARE(snapshot->sequence, std::uint64_t{11});
    QCOMPARE(snapshot->capturedAtUnixMilliseconds, std::int64_t{1234});
    QCOMPARE(snapshot->sampleCount, Ps3Snapshot::kMaxSampleCount);
    QCOMPARE(snapshot->correctionCount, Ps3Snapshot::kMaxCorrectionCount);
    QCOMPARE(snapshot->x.size(), std::size_t{Ps3Snapshot::kMaxSampleCount});
    QCOMPARE(snapshot->ym.size(), snapshot->x.size());
    QCOMPARE(snapshot->yc.size(), snapshot->x.size());
    QCOMPARE(snapshot->ys.size(), snapshot->x.size());
    QCOMPARE(snapshot->xmCorrection.size(),
             std::size_t{Ps3Snapshot::kMaxCorrectionCount});
    QCOMPARE(snapshot->ymCorrection.size(), snapshot->xmCorrection.size());
    QCOMPARE(snapshot->xaCorrection.size(), snapshot->xmCorrection.size());
    QCOMPARE(snapshot->yaCorrection.size(), snapshot->xmCorrection.size());

    // The returned value owns its data; later adapter calls cannot alias it.
    const double firstOwnedValue = snapshot->x.front();
    g_fakeMode = FakeMode::NonFiniteSample;
    QVERIFY(!adapter.capture(3, 7, 12, 1235).has_value());
    QCOMPARE(snapshot->x.front(), firstOwnedValue);
}

void TestPs3DisplayAdapter::captureRejectsCountsOutsideBounds()
{
    Ps3DisplayAdapter adapter(&fakeGetPsDisp);

    g_fakeMode = FakeMode::TooManySamples;
    QVERIFY(!adapter.capture(3, 7, 11, 1234).has_value());

    g_fakeMode = FakeMode::TooManyCorrections;
    QVERIFY(!adapter.capture(3, 7, 11, 1234).has_value());

    g_fakeMode = FakeMode::NegativeSampleCount;
    QVERIFY(!adapter.capture(3, 7, 11, 1234).has_value());

    g_fakeMode = FakeMode::NegativeCorrectionCount;
    QVERIFY(!adapter.capture(3, 7, 11, 1234).has_value());
}

void TestPs3DisplayAdapter::captureRejectsNonFiniteValues()
{
    g_fakeMode = FakeMode::NonFiniteSample;
    Ps3DisplayAdapter adapter(&fakeGetPsDisp);

    QVERIFY(!adapter.capture(3, 7, 11, 1234).has_value());

    g_fakeMode = FakeMode::NonFiniteCorrection;
    QVERIFY(!adapter.capture(3, 7, 11, 1234).has_value());

    g_fakeMode = FakeMode::NonFinitePhaseReference;
    QVERIFY(!adapter.capture(3, 7, 11, 1234).has_value());
}

void TestPs3DisplayAdapter::transformRejectsMalformedSnapshotCounts()
{
    Ps3Snapshot snapshot = transformFixture();
    snapshot.sampleCount = 3;
    const Ps3PlotData plot = Ps3DisplayAdapter::transform(snapshot);
    QVERIFY(plot.measuredMagnitude.empty());
    QVERIFY(plot.correctionMagnitude.empty());
}

void TestPs3DisplayAdapter::transformMatchesPinnedCalccContract()
{
    const Ps3PlotData plot = Ps3DisplayAdapter::transform(transformFixture());

    QCOMPARE(plot.measuredMagnitude.size(), std::size_t{2});
    QCOMPARE(plot.measuredMagnitude[0].x, 0.75);
    QCOMPARE(plot.measuredMagnitude[0].y, 0.5);
    QCOMPARE(plot.measuredGain.size(), std::size_t{1});
    QVERIFY(std::abs(plot.measuredGain[0].x - 0.75) < 1.0e-12);
    QVERIFY(std::abs(plot.measuredGain[0].y - (2.0 / 3.0)) < 1.0e-12);
    QCOMPARE(plot.measuredPhase[0].x, 0.75);
    QCOMPARE(plot.measuredPhase[0].y, 60.0);

    QCOMPARE(plot.correctionMagnitude.size(), std::size_t{1});
    QCOMPARE(plot.correctionMagnitude[0].x, 0.25);
    QCOMPARE(plot.correctionMagnitude[0].y, 0.5);
    QCOMPARE(plot.correctionGain[0].x, 0.25);
    QCOMPARE(plot.correctionGain[0].y, 2.0);
    QCOMPARE(plot.correctionPhase[0].x, 0.25);
    QCOMPARE(plot.correctionPhase[0].y, -45.0);
}

void TestPs3DisplayAdapter::transformSkipsZeroMeasuredGain()
{
    const Ps3PlotData plot = Ps3DisplayAdapter::transform(transformFixture());

    QCOMPARE(plot.measuredMagnitude.size(), std::size_t{2});
    QCOMPARE(plot.measuredPhase.size(), std::size_t{2});
    QCOMPARE(plot.measuredGain.size(), std::size_t{1});
}

void TestPs3DisplayAdapter::wrap180UsesNegative180InclusiveConvention()
{
    QCOMPARE(Ps3DisplayAdapter::wrap180(180.0), -180.0);
    QCOMPARE(Ps3DisplayAdapter::wrap180(-180.0), -180.0);
    QCOMPARE(Ps3DisplayAdapter::wrap180(181.0), -179.0);
    QCOMPARE(Ps3DisplayAdapter::wrap180(-181.0), 179.0);
    QCOMPARE(Ps3DisplayAdapter::wrap180(540.0), -180.0);
}

QTEST_MAIN(TestPs3DisplayAdapter)
#include "tst_ps3_display_adapter.moc"
