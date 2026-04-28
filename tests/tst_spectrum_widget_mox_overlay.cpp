// no-port-check: unit tests for SpectrumWidget MOX overlay boolean state.
// Phase 3M-1a H.1.
//
// Tests verify:
//   1. Fresh widget has MOX overlay off.
//   2. setMoxOverlay(true) stores state.
//   3. setMoxOverlay(false) clears state.
//   4. setMoxOverlay is idempotent (same-value calls don't crash).
//   5. setTxAttenuatorOffsetDb stores value.
//   6. setTxFilterVisible stores state.
//
// No pixel-diff — visual verification is deferred until the GPU-path render
// loop is unit-testable.  State accessors confirm the slot set the field.

#include <QtTest/QtTest>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestSpectrumWidgetMoxOverlay : public QObject
{
    Q_OBJECT

private slots:
    void overlay_disabledByDefault();
    void setMoxOverlay_trueStoresState();
    void setMoxOverlay_falseStoresState();
    void setMoxOverlay_idempotent();
    void setTxAttenuatorOffsetDb_storesValue();
    void setTxFilterVisible_storesState();
};

// 1. Fresh widget has MOX overlay off
void TestSpectrumWidgetMoxOverlay::overlay_disabledByDefault()
{
    SpectrumWidget w;
    QVERIFY(!w.isMoxOverlayActive());
    QCOMPARE(w.txAttenuatorOffsetDb(), 0.0f);
    QVERIFY(!w.txFilterVisible());
}

// 2. setMoxOverlay(true) stores state
void TestSpectrumWidgetMoxOverlay::setMoxOverlay_trueStoresState()
{
    SpectrumWidget w;
    w.setMoxOverlay(true);
    QVERIFY(w.isMoxOverlayActive());
}

// 3. setMoxOverlay(false) clears state
void TestSpectrumWidgetMoxOverlay::setMoxOverlay_falseStoresState()
{
    SpectrumWidget w;
    w.setMoxOverlay(true);
    QVERIFY(w.isMoxOverlayActive());
    w.setMoxOverlay(false);
    QVERIFY(!w.isMoxOverlayActive());
}

// 4. setMoxOverlay is idempotent (calling with same value must not crash)
void TestSpectrumWidgetMoxOverlay::setMoxOverlay_idempotent()
{
    SpectrumWidget w;
    w.setMoxOverlay(false);  // default, should be no-op
    QVERIFY(!w.isMoxOverlayActive());

    w.setMoxOverlay(true);
    w.setMoxOverlay(true);   // second call: idempotent
    QVERIFY(w.isMoxOverlayActive());

    w.setMoxOverlay(false);
    w.setMoxOverlay(false);  // second call: idempotent
    QVERIFY(!w.isMoxOverlayActive());
}

// 5. setTxAttenuatorOffsetDb stores value
void TestSpectrumWidgetMoxOverlay::setTxAttenuatorOffsetDb_storesValue()
{
    SpectrumWidget w;
    w.setTxAttenuatorOffsetDb(31.0f);
    QCOMPARE(w.txAttenuatorOffsetDb(), 31.0f);

    w.setTxAttenuatorOffsetDb(0.0f);
    QCOMPARE(w.txAttenuatorOffsetDb(), 0.0f);
}

// 6. setTxFilterVisible stores state
void TestSpectrumWidgetMoxOverlay::setTxFilterVisible_storesState()
{
    SpectrumWidget w;
    QVERIFY(!w.txFilterVisible());

    w.setTxFilterVisible(true);
    QVERIFY(w.txFilterVisible());

    w.setTxFilterVisible(false);
    QVERIFY(!w.txFilterVisible());
}

QTEST_MAIN(TestSpectrumWidgetMoxOverlay)
#include "tst_spectrum_widget_mox_overlay.moc"
