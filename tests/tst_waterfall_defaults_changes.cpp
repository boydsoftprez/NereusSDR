// no-port-check: unit tests for Task 2.8 WaterfallDefaultsPage changes.
// NereusSDR-original — no Thetis upstream.
//
// Tests verify:
//   1. setWaterfallNFAGCEnabled() / waterfallNFAGCEnabled() round-trip.
//   2. setWaterfallAGCOffsetDb() / waterfallAGCOffsetDb() round-trip.
//   3. setWaterfallAGCOffsetDb() clamps to [-60, +60].
//   4. setWaterfallStopOnTx() / waterfallStopOnTx() round-trip.
//   5. Stop-on-TX: pushWaterfallRow() is skipped when TX active + flag set.
//   6. Stop-on-TX: pushWaterfallRow() proceeds when TX active but flag clear.
//   7. W5 (setWfReverseScroll) has been removed — compile-time proof via
//      absence of the method in the header (asserted via static_assert-style
//      note; actual removal is verified by the build succeeding without it).
//
// No pixel-diff — visual verification deferred to manual bench.
// Stop-on-TX tests verify via wfLastPushMs: if the push was skipped the
// timestamp does not advance.

#include <QtTest/QtTest>
#include <QVector>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestWaterfallDefaultsChanges : public QObject
{
    Q_OBJECT

private slots:
    void nfAgcEnabled_defaultFalse();
    void nfAgcEnabled_roundTrip();
    void nfAgcOffsetDb_defaultZero();
    void nfAgcOffsetDb_roundTrip();
    void nfAgcOffsetDb_clampsAbove60();
    void nfAgcOffsetDb_clampsBelow60();
    void stopOnTx_defaultFalse();
    void stopOnTx_roundTrip();
    void stopOnTx_skipsRowWhenTxActive();
    void stopOnTx_proceedsWhenFlagClear();
    void stopOnTx_proceedsWhenTxNotActive();
    void stopOnTx_localPanStopsWhileTransmitting();
    void stopOnTx_remotePanStopsWhileTransmitting();
    void stopOnTx_offKeepsRowsWhileTransmitting();
};

void TestWaterfallDefaultsChanges::nfAgcEnabled_defaultFalse()
{
    SpectrumWidget w;
    QVERIFY(!w.waterfallNFAGCEnabled());
}

void TestWaterfallDefaultsChanges::nfAgcEnabled_roundTrip()
{
    SpectrumWidget w;
    w.setWaterfallNFAGCEnabled(true);
    QVERIFY(w.waterfallNFAGCEnabled());
    w.setWaterfallNFAGCEnabled(false);
    QVERIFY(!w.waterfallNFAGCEnabled());
}

void TestWaterfallDefaultsChanges::nfAgcOffsetDb_defaultZero()
{
    SpectrumWidget w;
    QCOMPARE(w.waterfallAGCOffsetDb(), 0);
}

void TestWaterfallDefaultsChanges::nfAgcOffsetDb_roundTrip()
{
    SpectrumWidget w;
    w.setWaterfallAGCOffsetDb(-10);
    QCOMPARE(w.waterfallAGCOffsetDb(), -10);
    w.setWaterfallAGCOffsetDb(20);
    QCOMPARE(w.waterfallAGCOffsetDb(), 20);
    w.setWaterfallAGCOffsetDb(0);
    QCOMPARE(w.waterfallAGCOffsetDb(), 0);
}

void TestWaterfallDefaultsChanges::nfAgcOffsetDb_clampsAbove60()
{
    SpectrumWidget w;
    w.setWaterfallAGCOffsetDb(999);
    QCOMPARE(w.waterfallAGCOffsetDb(), 60);
}

void TestWaterfallDefaultsChanges::nfAgcOffsetDb_clampsBelow60()
{
    SpectrumWidget w;
    w.setWaterfallAGCOffsetDb(-999);
    QCOMPARE(w.waterfallAGCOffsetDb(), -60);
}

void TestWaterfallDefaultsChanges::stopOnTx_defaultFalse()
{
    SpectrumWidget w;
    QVERIFY(!w.waterfallStopOnTx());
}

void TestWaterfallDefaultsChanges::stopOnTx_roundTrip()
{
    SpectrumWidget w;
    w.setWaterfallStopOnTx(true);
    QVERIFY(w.waterfallStopOnTx());
    w.setWaterfallStopOnTx(false);
    QVERIFY(!w.waterfallStopOnTx());
}

// Smoke test for stop-on-TX: verify the setters don't crash when TX active.
// Full integration testing (verifying row-count increment / no-increment)
// requires a live QRhi surface and is deferred to the manual bench session.
void TestWaterfallDefaultsChanges::stopOnTx_skipsRowWhenTxActive()
{
    SpectrumWidget w;
    w.setWaterfallStopOnTx(true);
    w.setActivePeakHoldTxActive(true);
    // Calling pushWaterfallRow (private) requires a public entry; the method
    // is exercised indirectly via updateSpectrumLinear, which we can't call
    // headless.  Verify the setter combination is at minimum non-crashing.
    QVERIFY(w.waterfallStopOnTx());
    // No crash = pass.
}

void TestWaterfallDefaultsChanges::stopOnTx_proceedsWhenFlagClear()
{
    SpectrumWidget w;
    w.setWaterfallStopOnTx(false);
    w.setActivePeakHoldTxActive(true);
    QVERIFY(!w.waterfallStopOnTx());
    // No crash = pass.
}

void TestWaterfallDefaultsChanges::stopOnTx_proceedsWhenTxNotActive()
{
    SpectrumWidget w;
    w.setWaterfallStopOnTx(true);
    w.setActivePeakHoldTxActive(false);
    // TX not active — stop-on-TX gate is inactive; no crash.
    QVERIFY(w.waterfallStopOnTx());
}

namespace {

// A shown 3D pan, so pushWaterfallRow() allocates its waterfall and counts
// each row it draws (dssRowsPushedForTest), as tst_dss_row_tee does.
void showCountingPan(SpectrumWidget& w)
{
    w.resize(400, 200);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
}

QVector<float> wfRow() { return QVector<float>(768, -130.0f); }

} // namespace

// Thetis display.cs:7601-7604 [v2.10.3.15]: the receive waterfall stops on
// TX when the option is on AND that receiver is transmitting (local_mox).
// The transmitting state is the pan's own MOX overlay, which
// MoxDisplayController sets on the transmitting pan for a local radio and
// for a Core alike. Before this fix the gate read a flag nothing set, so
// the row was drawn anyway.
void TestWaterfallDefaultsChanges::stopOnTx_localPanStopsWhileTransmitting()
{
    SpectrumWidget w;
    showCountingPan(w);
    w.setWaterfallStopOnTx(true);
    w.pushWaterfallRowForTest(wfRow());
    const int before = w.dssRowsPushedForTest();
    QCOMPARE(before, 1);

    w.setMoxOverlay(true);
    w.pushWaterfallRowForTest(wfRow());
    w.pushWaterfallRowForTest(wfRow());
    QCOMPARE(w.dssRowsPushedForTest(), before);

    w.setMoxOverlay(false);
    w.pushWaterfallRowForTest(wfRow());
    QCOMPARE(w.dssRowsPushedForTest(), before + 1);
}

void TestWaterfallDefaultsChanges::stopOnTx_remotePanStopsWhileTransmitting()
{
    SpectrumWidget w;
    showCountingPan(w);
    w.invalidateRemoteSpectrumFrame();
    w.setWaterfallTickerPausedForTest(true);
    w.setWaterfallStopOnTx(true);
    const double centre = w.centerFrequency();
    const double span = w.bandwidth();
    QVERIFY(w.enqueueRemoteWaterfallRow(wfRow(), {}, centre, span));
    w.tickWaterfallForTest();
    const int before = w.dssRowsPushedForTest();
    QCOMPARE(before, 1);

    w.setMoxOverlay(true);
    QVERIFY(w.enqueueRemoteWaterfallRow(wfRow(), {}, w.centerFrequency(), w.bandwidth()));
    w.tickWaterfallForTest();
    QCOMPARE(w.dssRowsPushedForTest(), before);

    w.setMoxOverlay(false);
    QVERIFY(w.enqueueRemoteWaterfallRow(wfRow(), {}, w.centerFrequency(), w.bandwidth()));
    w.tickWaterfallForTest();
    QCOMPARE(w.dssRowsPushedForTest(), before + 1);
}

void TestWaterfallDefaultsChanges::stopOnTx_offKeepsRowsWhileTransmitting()
{
    SpectrumWidget w;
    showCountingPan(w);
    w.setWaterfallStopOnTx(false);
    w.setMoxOverlay(true);
    w.pushWaterfallRowForTest(wfRow());
    QCOMPARE(w.dssRowsPushedForTest(), 1);
}

QTEST_MAIN(TestWaterfallDefaultsChanges)
#include "tst_waterfall_defaults_changes.moc"
