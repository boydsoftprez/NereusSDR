// From AetherSDR SpectrumWidget.h:78-82 [@1872028c] (SpectrumRenderMode enum
// shape) plus NereusSDR-original test seams. Task 6 of the 3D stacked-trace
// spectrum plan (design doc docs/architecture/2026-08-08-3d-stacked-trace-
// spectrum-design.md, plan docs/architecture/2026-08-08-3d-stacked-trace-
// spectrum-plan.md).
//
// Pins the row-tee placement: pushDssRow() must be called from inside
// pushWaterfallRow() downstream of the stop-on-TX early return, never from
// the WaterfallTicker callback upstream of it. See
// stopOnTx_freezesBothPanesTogether below.

#include <QTest>
#include <QVector>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

namespace {

QVector<float> row(int n, float dbm) { return QVector<float>(n, dbm); }

}  // namespace

class TestDssRowTee : public QObject {
    Q_OBJECT

private slots:
    void defaultMode_is2D() {
        SpectrumWidget w;
        QCOMPARE(w.spectrumRenderMode(),
                 static_cast<int>(SpectrumRenderMode::Mode2D));
    }

    void controlDefaults_matchUpstream() {
        SpectrumWidget w;
        QCOMPARE(w.dssFloorDepth(), 6);
        QCOMPARE(w.dssGain(),      70);
        QCOMPARE(w.dssRowSpan(),  100);
        QCOMPARE(w.dssAngle(),     50);   // NereusSDR-original
        QCOMPARE(w.threeDSliceDepth(), false);
    }

    void angle50_yieldsUpstreamShape() {
        SpectrumWidget w;
        const DssShape s = w.dssShape();
        QVERIFY(std::abs(s.backWidthFrac - 0.60f) < 1e-6f);
        QVERIFY(std::abs(s.depthSpanFrac - 0.58f) < 1e-6f);
    }

    void controlsAreClamped() {
        SpectrumWidget w;
        w.setDssFloorDepth(999);  QCOMPARE(w.dssFloorDepth(), 24);
        w.setDssFloorDepth(-5);   QCOMPARE(w.dssFloorDepth(), 0);
        w.setDssGain(999);        QCOMPARE(w.dssGain(),      100);
        w.setDssRowSpan(-1);      QCOMPARE(w.dssRowSpan(),     0);
        w.setDssAngle(999);       QCOMPARE(w.dssAngle(),     100);
    }

    // The tee must sit DOWNSTREAM of the stop-on-TX gate. Teeing at the
    // ticker callback instead would let the 3D stack keep scrolling while
    // the waterfall is frozen, desyncing the two panes on every over.
    void stopOnTx_freezesBothPanesTogether() {
        SpectrumWidget w;
        // pushWaterfallRow() returns immediately while m_waterfall is null
        // (its very first guard, upstream of both the stop-on-TX gate and
        // the DSS tee under test); resize so the waterfall image actually
        // allocates, matching every other SpectrumWidget test in this file
        // that exercises push/paint internals (e.g. tst_notch_hit_test.cpp).
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setWaterfallStopOnTx(true);
        w.setTxActiveForTest(false);
        w.pushWaterfallRowForTest(row(768, -130.0f));
        const int beforeTx = w.dssRowsPushedForTest();
        QCOMPARE(beforeTx, 1);

        w.setTxActiveForTest(true);
        w.pushWaterfallRowForTest(row(768, -130.0f));
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssRowsPushedForTest(), beforeTx);   // frozen with the waterfall

        w.setTxActiveForTest(false);
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssRowsPushedForTest(), beforeTx + 1);
    }

    void stopOnTxDisabled_keepsBothPanesRunning() {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));  // see stopOnTx_freezesBothPanesTogether
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setWaterfallStopOnTx(false);
        w.setTxActiveForTest(true);
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssRowsPushedForTest(), 1);
    }

    // In 2D the ring must not be fed at all: a 2D pan allocates and does no
    // DSS work (design doc section 3.4).
    void mode2D_doesNotFeedTheRing() {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));  // see stopOnTx_freezesBothPanesTogether
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssRowsPushedForTest(), 0);
    }
};

QTEST_MAIN(TestDssRowTee)
#include "tst_dss_row_tee.moc"
