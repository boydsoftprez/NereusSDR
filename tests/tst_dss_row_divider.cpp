// tests/tst_dss_row_divider.cpp
//
// no-port-check: NereusSDR-original. 3D Stacked-Trace Spectrum Plan Task
// 24 (design doc docs/architecture/2026-08-08-3d-stacked-trace-spectrum-
// design.md section 4.5). Upstream AetherSDR at 1872028c has no row
// divider, no 3D-specific cadence control, and ties its 3D scroll to the
// same clock as its 2D waterfall (crew workspace scout-scroll-speed.md,
// section 5), so there is no upstream source to cite for this feature.
//
// Pins the 3D Speed row divider: the 3D ring consumes one row per N
// waterfall ticks (N = effectiveDssRowDivider()), folding the N rows per
// column by peak-hold so a one-tick burst still raises a ridge, and
// stretches the glide phase over the same N ticks so the scroll stays
// continuous. N is automatic by default (waterfall pixel height over 96,
// clamped 1..64, re-read live) with a manual 1..10 override.
//
// Widget tests that push rows or take mouse events resize()+show()+
// QVERIFY(QTest::qWaitForWindowExposed(&w)) first: pushWaterfallRow()'s
// very first guard returns immediately while m_waterfall is null (see
// tst_dss_row_tee.cpp's stopOnTx_freezesBothPanesTogether, which
// documents the same trap).
//
// A 400x200 widget's waterfall image lands at 88 px tall with the
// default 0.40 spectrum split (int(200*0.6) - kFreqScaleH(28) -
// kDividerH(4) = 88), so qRound(88/96.0) clamps to an automatic divider
// of 1 -- every test below that wants a MANUAL divider therefore calls
// setDssRowDivider() explicitly rather than relying on that widget size
// to produce anything but Match's default 1:1 behaviour.

#include <QtTest/QtTest>
#include <QVector>

#include <algorithm>
#include <cmath>

#include "gui/DssGeometry.h"
#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

namespace {

QVector<float> row(int n, float dbm) { return QVector<float>(n, dbm); }

} // namespace

class TestDssRowDivider : public QObject {
    Q_OBJECT

private slots:
    // Acceptance: 3D mode, widget shown and exposed, divider 5, twelve
    // rows through pushWaterfallRowForTest: dssRowsPushedForTest() is 2
    // and dssFoldCountForTest() is 2 (rows 11 and 12 pending).
    void manualDivider_pushesOneRowPerNTicks()
    {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssRowDivider(5);

        for (int i = 0; i < 12; ++i) {
            w.pushWaterfallRowForTest(row(768, -130.0f));
        }

        QCOMPARE(w.dssRowsPushedForTest(), 2);
        QCOMPARE(w.dssFoldCountForTest(), 2);
    }

    // Acceptance: divider 1, twelve rows: twelve pushes.
    void divider1_matchesTodaysBehaviour()
    {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssRowDivider(1);

        for (int i = 0; i < 12; ++i) {
            w.pushWaterfallRowForTest(row(768, -130.0f));
        }

        QCOMPARE(w.dssRowsPushedForTest(), 12);
        QCOMPARE(w.dssFoldCountForTest(), 0); // every row pushes immediately
    }

    // Acceptance: divider 3, ring empty (fresh 3D entry), rows of -100
    // everywhere except the second row, which carries -40 across three
    // adjacent columns c-1..c+1: after the third row the ring's newest
    // row reads at least -45 at column c. Three columns, not one,
    // because smoothDssRow's 3-tap spatial blur (0.25/0.5/0.25) would
    // otherwise dilute a single-column spike toward the floor
    // (0.25*-100 + 0.5*-40 + 0.25*-100 = -70); a 3-wide plateau survives
    // the blur unchanged at its centre column.
    //
    // Goes red when the per-column maximum is replaced by "last row
    // wins": the third (and last) folded row is flat -100 everywhere, so
    // the -40 peak from row 2 would never reach the ring at all. See the
    // task report for the recorded red run and the exact observed value.
    void foldIsPeakHold()
    {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssRowDivider(3);

        const int width = kDssCols; // 768: identity resample, no bin remap
        const int c = 400;
        QVector<float> flat(width, -100.0f);
        QVector<float> spike(width, -100.0f);
        spike[c - 1] = -40.0f;
        spike[c]     = -40.0f;
        spike[c + 1] = -40.0f;

        w.pushWaterfallRowForTest(flat);  // row 1
        w.pushWaterfallRowForTest(spike); // row 2: the peak
        w.pushWaterfallRowForTest(flat);  // row 3: triggers the push

        QCOMPARE(w.dssRowsPushedForTest(), 1);
        QCOMPARE(w.dssFoldCountForTest(), 0);
        const float value = w.dssNewestRowColumnForTest(c);
        QVERIFY2(value >= -45.0f,
                 qPrintable(QStringLiteral(
                     "expected peak-hold >= -45 dBm at column %1, got %2")
                                .arg(c)
                                .arg(static_cast<double>(value))));
    }

    // Acceptance: divider 0; resize so the waterfall is tall, then
    // short; each time effectiveDssRowDividerForTest() equals
    // clamp(qRound(h / 96), 1, 64) where h is the waterfall image height
    // read through waterfallHeightForTest(). The two heights below give
    // two different dividers (12 and 1), one of them greater than 1.
    void autoDivider_tracksWaterfallHeight()
    {
        SpectrumWidget w;
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssRowDivider(0); // Match: automatic

        w.resize(400, 2000);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const int tallHeight = w.waterfallHeightForTest();
        const int tallExpected = std::clamp(
            qRound(double(tallHeight) / kDssVisibleRows),
            1, SpectrumWidget::kDssMaxAutoRowDivider);
        QCOMPARE(w.effectiveDssRowDividerForTest(), tallExpected);
        QVERIFY(tallExpected > 1);

        w.resize(400, 200);
        const int shortHeight = w.waterfallHeightForTest();
        const int shortExpected = std::clamp(
            qRound(double(shortHeight) / kDssVisibleRows),
            1, SpectrumWidget::kDssMaxAutoRowDivider);
        QCOMPARE(w.effectiveDssRowDividerForTest(), shortExpected);

        QVERIFY(tallExpected != shortExpected); // genuinely tracked the resize
    }

    // Acceptance: period 30 ms via setWfUpdatePeriodMs(30); divider 5:
    // dssScrollIncrementForTest(150) is 1.0 within 1e-6 and
    // dssScrollIncrementForTest(75) is 0.5; divider 1:
    // dssScrollIncrementForTest(30) is 1.0. Red when the divider factor
    // is dropped (period would stay 30 ms instead of scaling to 150 ms).
    void glideIncrementScalesWithDivider()
    {
        SpectrumWidget w;
        w.setWfUpdatePeriodMs(30);

        w.setDssRowDivider(5);
        QVERIFY(std::abs(w.dssScrollIncrementForTest(150) - 1.0f) < 1e-6f);
        QVERIFY(std::abs(w.dssScrollIncrementForTest(75) - 0.5f) < 1e-6f);

        w.setDssRowDivider(1);
        QVERIFY(std::abs(w.dssScrollIncrementForTest(30) - 1.0f) < 1e-6f);
    }

    // Acceptance: divider 4; two rows of width 100 then one of width
    // 120: dssFoldCountForTest() is 1 and nothing pushed.
    void partialFold_restartsOnSizeChange()
    {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssRowDivider(4);

        w.pushWaterfallRowForTest(row(100, -130.0f));
        w.pushWaterfallRowForTest(row(100, -130.0f));
        w.pushWaterfallRowForTest(row(120, -130.0f)); // size change: restarts

        QCOMPARE(w.dssFoldCountForTest(), 1);
        QCOMPARE(w.dssRowsPushedForTest(), 0);
    }

    // Acceptance: divider 10, six rows, then divider 3, one more row:
    // one push, count 0.
    void dividerLoweredMidFold_pushesOnNextTick()
    {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssRowDivider(10);

        for (int i = 0; i < 6; ++i) {
            w.pushWaterfallRowForTest(row(768, -130.0f));
        }
        QCOMPARE(w.dssRowsPushedForTest(), 0); // precondition: nothing pushed yet
        QCOMPARE(w.dssFoldCountForTest(), 6);

        w.setDssRowDivider(3);
        w.pushWaterfallRowForTest(row(768, -130.0f));

        QCOMPARE(w.dssRowsPushedForTest(), 1);
        QCOMPARE(w.dssFoldCountForTest(), 0);
    }

    // Acceptance: with stop-on-TX enabled and m_txActiveForTest true,
    // rows advance neither the waterfall write row nor the fold count.
    // Presence is proven first (TX inactive: the fold count moves), so
    // the later "frozen" assertion is not vacuous. pushWaterfallRow()'s
    // stop-on-TX gate is a single early return that sits before BOTH the
    // 3D tee (accumulateDssRow) and the waterfall pixel write that
    // follows it in program order (SpectrumWidget.cpp), so a frozen fold
    // count is proof the waterfall write never ran either -- there is no
    // second, independent gate that could diverge from this one.
    void stopOnTx_pausesFold()
    {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssRowDivider(5);
        w.setWaterfallStopOnTx(true);

        // Presence: TX inactive, the fold advances.
        w.setTxActiveForTest(false);
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssFoldCountForTest(), 1);

        // Absence: TX active, the fold is frozen across multiple pushes.
        w.setTxActiveForTest(true);
        w.pushWaterfallRowForTest(row(768, -130.0f));
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssFoldCountForTest(), 1);
        QCOMPARE(w.dssRowsPushedForTest(), 0);

        // And resumes once TX clears.
        w.setTxActiveForTest(false);
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssFoldCountForTest(), 2);
    }

    // Acceptance: divider 4, two rows, switch to 2D and back: count 0.
    void enteringAndLeaving3D_resetsFold()
    {
        SpectrumWidget w;
        w.resize(400, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssRowDivider(4);

        w.pushWaterfallRowForTest(row(768, -130.0f));
        w.pushWaterfallRowForTest(row(768, -130.0f));
        QCOMPARE(w.dssFoldCountForTest(), 2); // precondition: fold is dirty

        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        QCOMPARE(w.dssFoldCountForTest(), 0);

        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        QCOMPARE(w.dssFoldCountForTest(), 0);
    }
};

QTEST_MAIN(TestDssRowDivider)
#include "tst_dss_row_divider.moc"
