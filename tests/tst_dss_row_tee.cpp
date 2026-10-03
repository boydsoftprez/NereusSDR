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

#ifdef NEREUS_GPU_SPECTRUM
    // Once the ring wraps, two producer pushes between paints overwrite two
    // texture rows. Uploading only the latest head leaves one stale ridge.
    void skippedPaint_uploadsEveryChangedHeightRow() {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssRowDivider(1);
        for (int i = 0; i < kDssRows; ++i) {
            w.pushWaterfallRowForTest(row(768, -100.0f));
        }
        QCOMPARE(w.m_dss.rowCount(), kDssRows);

        w.m_dssUploadedRowGeneration = w.m_dss.rowGeneration();
        w.m_dssLastUploadedHead = w.m_dss.headRing();
        QVERIFY(w.dssHeightRowsToUpload().isEmpty());

        w.pushWaterfallRowForTest(row(768, -90.0f));
        QCOMPARE(w.dssHeightRowsToUpload(), QVector<int>{w.m_dss.headRing()});
        w.m_dssUploadedRowGeneration = w.m_dss.rowGeneration();
        w.m_dssLastUploadedHead = w.m_dss.headRing();

        w.pushWaterfallRowForTest(row(768, -80.0f));
        w.pushWaterfallRowForTest(row(768, -70.0f));
        QVector<int> allRows;
        for (int ring = 0; ring < kDssRows; ++ring) {
            allRows.append(ring);
        }
        QCOMPARE(w.dssHeightRowsToUpload(), allRows);

        w.m_dssUploadedRowGeneration = w.m_dss.rowGeneration();
        w.m_dssLastUploadedHead = w.m_dss.headRing();
        for (int i = 0; i < kDssRows; ++i) {
            w.pushWaterfallRowForTest(row(768, -60.0f));
        }
        QCOMPARE(w.dssHeightRowsToUpload(), allRows);
    }
#endif

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

    // Leaving 3D must clear the ring, so re-entering does not display a stack
    // of rows captured at a frequency the operator has since left. And
    // re-asserting the SAME mode must NOT clear, or an idempotent UI refresh
    // would silently wipe live history.
    //
    // Both behaviours are load-bearing for the rendering task that reads this
    // ring. Without this test, deleting the clear branch entirely leaves every
    // other test in this file green.
    void leaving3D_clearsTheRing_butSameModeDoesNot() {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));  // see stopOnTx_freezesBothPanesTogether
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.pushWaterfallRowForTest(row(768, -130.0f));
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssRowsPushedForTest(), 2);   // precondition: ring is dirty

        // Same mode again: no-op guard must fire, history survives.
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        QCOMPARE(w.dssRowsPushedForTest(), 2);

        // Genuinely leaving 3D: ring clears.
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        QCOMPARE(w.dssRowsPushedForTest(), 0);
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

    // The wide cache is populated only by receive FFT frames. A keyed row
    // must therefore stay exact-only: attaching that cache would combine the
    // current transmit centre with shoulders from the last receive frame.
    void moxRow_doesNotReuseReceiveWideCache() {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSampleRate(768000.0);
        w.setDdcCenterFrequency(14200000.0);
        w.setFrequencyRange(14200000.0, 96000.0);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setFullBinsForTest(row(4096, -130.0f));

        w.pushWaterfallRowForTest(row(768, -130.0f));
        QVERIFY(w.dssNewestRowWideBandwidthForTest() > 0.096);

        w.setMoxOverlay(true);
        w.pushWaterfallRowForTest(row(768, -50.0f));
        QCOMPARE(w.dssNewestRowWideBandwidthForTest(), 0.0);
    }
};

QTEST_MAIN(TestDssRowTee)
#include "tst_dss_row_tee.moc"
