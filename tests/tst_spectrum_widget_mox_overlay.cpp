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
#include "core/AppSettings.h"

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
    void grids_are_independent_across_mox();
    void txGrid_keys_are_per_pan();
    void no_pointer_path_tunes_while_keyed();
    void keyed_save_leaves_the_receive_bandwidth_alone();
    void txGrid_load_accepts_what_the_drag_can_produce();
    void tx_low_colour_is_used_below_the_floor();
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

// 7. Receive and transmit keep independent dBm grids.
//
// From Thetis display.cs:1782-1804 [v2.10.3.15] — SpectrumGridMaxMoxModified
// returns tx_spectrum_grid_max while MOX is asserted and spectrum_grid_max
// otherwise. Two stores, one selector, nothing saved and restored.
//
// The version this replaced saved the receive grid on the MOX rise edge and
// captured whatever was live on the fall edge. That made the transmit range
// equal to "whatever the widget happened to be showing at un-key", so ANY
// writer between the edges silently redefined it -- the receive noise-floor
// tracker did exactly that on an ORION-class radio, whose receiver keeps
// running through transmit, and the next key-up came up unusable. Bench
// 2026-08-05.
void TestSpectrumWidgetMoxOverlay::grids_are_independent_across_mox()
{
    SpectrumWidget w;

    w.setDbmRange(-120.0f, -40.0f);          // a receive range
    const float rxRef   = w.refLevel();
    const float rxRange = w.dynamicRange();

    w.setMoxOverlay(true);
    // Transmit comes up on its own grid, not the receive one.
    QVERIFY2(!qFuzzyCompare(w.refLevel(), rxRef)
             || !qFuzzyCompare(w.dynamicRange(), rxRange),
             "transmit inherited the receive grid instead of its own");

    w.setDbmRange(-80.0f, 20.0f);            // adjust while transmitting
    const float txRef   = w.refLevel();
    const float txRange = w.dynamicRange();

    w.setMoxOverlay(false);
    // Receive is exactly what it was. The transmit edit did not leak.
    QCOMPARE(w.refLevel(), rxRef);
    QCOMPARE(w.dynamicRange(), rxRange);

    // And a receive-side change cannot reach the transmit grid -- this is
    // the property the save/restore version could not hold.
    w.setDbmRange(-130.0f, -30.0f);

    w.setMoxOverlay(true);
    QCOMPARE(w.refLevel(), txRef);
    QCOMPARE(w.dynamicRange(), txRange);
}

// The transmit grid keys are per pan, like every other display setting.
//
// They shipped as bare globals, which every pan read at startup AND wrote on
// every save while holding its own copy. Only the transmitting pan's copy
// ever changes, so the next save from any OTHER pan put the startup value
// back and the operator's drag silently reverted. Fails on the bare-key
// version: pan 1's save lands on pan 0's key.
void TestSpectrumWidgetMoxOverlay::txGrid_keys_are_per_pan()
{
    auto& s = AppSettings::instance();

    SpectrumWidget pan0;
    pan0.setPanIndex(0);
    pan0.setMoxOverlay(true);
    pan0.setDbmRange(-70.0f, 10.0f);     // pan 0's transmit grid
    pan0.saveSettingsForTest();

    const QString pan0Ref =
        s.value(QStringLiteral("DisplayTxGridRefLevel"), QString()).toString();
    QVERIFY2(!pan0Ref.isEmpty(), "pan 0 did not write a transmit grid at all");

    SpectrumWidget pan1;
    pan1.setPanIndex(1);
    pan1.setMoxOverlay(true);
    pan1.setDbmRange(-100.0f, -20.0f);   // a visibly different one
    pan1.saveSettingsForTest();

    // Pan 1 wrote its own key...
    QCOMPARE(s.value(QStringLiteral("DisplayTxGridRefLevel_1"), QString())
                 .toString().toFloat(),
             -20.0f);
    // ...and left pan 0's alone. This is the assertion the bare-key version
    // cannot pass.
    QCOMPARE(s.value(QStringLiteral("DisplayTxGridRefLevel"), QString())
                 .toString(),
             pan0Ref);
}

// Not one pointer path may move the radio while this pan is keyed.
//
// The original guard went on the short-pan-release branch alone, and that was
// not the only way in: a press inside the passband sets m_draggingVfo and
// mouseMoveEvent tunes on every move, a spot label tunes on press, and the
// wheel tunes on every notch. Codex found all three on PR #317.
//
// Asserted through requestTune's public effect rather than by synthesising
// mouse events: the contract being pinned is that frequencyClicked cannot
// escape while m_moxOverlay is set, whatever calls it. Every emitter is
// routed through that one function, which is checkable by grep and is what
// makes this test cover the paths it cannot drive.
void TestSpectrumWidgetMoxOverlay::no_pointer_path_tunes_while_keyed()
{
    SpectrumWidget w;
    QSignalSpy spy(&w, &SpectrumWidget::frequencyClicked);

    w.requestTuneForTest(14200000.0);
    QCOMPARE(spy.count(), 1);       // receive: tunes, as always

    w.setMoxOverlay(true);
    w.requestTuneForTest(14250000.0);
    QCOMPARE(spy.count(), 1);       // keyed: withheld

    w.setMoxOverlay(false);
    w.requestTuneForTest(14300000.0);
    QCOMPARE(spy.count(), 2);       // un-keyed: tunes again
    QCOMPARE(spy.at(1).at(0).toDouble(), 14300000.0);
}

// A save that lands mid-transmission must not write the transmit span into
// the receive bandwidth key.
//
// The direction-aware save protected the dBm pair and left DisplayBandwidth
// reading the live m_bandwidthHz, which IS the transmit span while keyed. Any
// setting change during a transmission put the transmit zoom in the receive
// key and the next launch came up with the receive pan at a transmit width.
// Found by Codex on PR #317.
void TestSpectrumWidgetMoxOverlay::keyed_save_leaves_the_receive_bandwidth_alone()
{
    auto& s = AppSettings::instance();

    SpectrumWidget w;
    w.setPanIndex(0);
    w.setFrequencyRange(14200000.0, 192000.0);   // the receive span
    w.saveSettingsForTest();
    const QString rxStored =
        s.value(QStringLiteral("DisplayBandwidth"), QString()).toString();
    QCOMPARE(rxStored.toFloat(), 192000.0f);

    w.setMoxOverlay(true);
    w.setFrequencyRange(14200000.0, 8000.0);     // a transmit span
    w.saveSettingsForTest();

    QCOMPARE(s.value(QStringLiteral("DisplayBandwidth"), QString())
                 .toString().toFloat(),
             192000.0f);
}

// The loader's bounds are the drag's bounds.
//
// They were [-160, +20] while the drag clamps to Thetis's [-200, +200], and
// +20 is exactly the transmit grid's default reference level. So an operator
// who dragged a pinned transmit scale UP saved the fix and had it silently
// discarded on the next launch. Found by Codex on PR #317.
void TestSpectrumWidgetMoxOverlay::txGrid_load_accepts_what_the_drag_can_produce()
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("DisplayTxGridRefLevel"),
               QStringLiteral("60"));            // above the old +20 ceiling
    s.setValue(QStringLiteral("DisplayTxGridDynamicRange"),
               QStringLiteral("100"));

    SpectrumWidget w;
    w.setPanIndex(0);
    w.loadSettingsForTest();
    w.setMoxOverlay(true);                       // swap the transmit grid in

    QCOMPARE(w.refLevel(), 60.0f);
}

// TX Low Color is a control the operator can set, so it has to reach a pixel.
//
// It was persisted and read back and never consulted in dbmToRgb: a bin at or
// below m_txWfLowLevel produced adjusted == 0 and fell into the palette's
// first gradient stop. Thetis pairs the two in its TX branch
// (display.cs:6424-6427 [v2.10.3.15], low_threshold and low_color set
// together) and NereusSDR ported only the threshold. Found by Codex on
// PR #317.
void TestSpectrumWidgetMoxOverlay::tx_low_colour_is_used_below_the_floor()
{
    SpectrumWidget w;
    w.setTxWfLowLevel(-100);
    w.setTxWfHighLevel(0);
    w.setTxWfLowColor(QColor(17, 34, 51));      // nothing a palette would pick

    // Receive is untouched: it has no separate low-colour control, so the
    // floor there is still the palette's own first stop.
    const QRgb rxFloor = w.dbmToRgbForTest(-140.0f);
    QVERIFY2(rxFloor != qRgb(17, 34, 51),
             "the TX low colour leaked into the receive path");

    w.setMoxOverlay(true);
    QCOMPARE(w.dbmToRgbForTest(-140.0f), qRgb(17, 34, 51));  // under the floor
    QCOMPARE(w.dbmToRgbForTest(-100.0f), qRgb(17, 34, 51));  // exactly on it

    // And a bin above the floor still comes from the gradient.
    QVERIFY2(w.dbmToRgbForTest(-20.0f) != qRgb(17, 34, 51),
             "the low colour swallowed a bin above the floor");
}

QTEST_MAIN(TestSpectrumWidgetMoxOverlay)
#include "tst_spectrum_widget_mox_overlay.moc"
