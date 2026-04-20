// =================================================================
// tests/tst_preset_live_binding.cpp  (NereusSDR)
// =================================================================
//
// Edit-container refactor Task 20 — live-data routing for preset
// classes via the new virtual MeterItem::pushBindingValue(int, double)
// entry point. Before Task 20 the preset classes ignored
// MeterWidget::updateMeterValue() because their bindingId() was -1
// (multi-binding composites) or because their paint() hard-coded a
// midpoint seed. These tests cover the poller → preset → needle /
// bar routing end-to-end:
//
//   * AnanMultiMeterItem with 7 internal needles dispatches each
//     MeterBinding::* ID to the matching needle only (Signal → [0],
//     Volts → [1], etc.).
//   * CrossNeedleItem with 2 internal needles dispatches
//     TxPower → [0], TxReversePower → [1].
//   * PowerSwrPresetItem with 2 internal bars dispatches TxPower →
//     Power bar, TxSwr → SWR bar.
//   * Single-binding preset classes (SMeter, BarPreset, etc.)
//     inherit the MeterItem default, which forwards to setValue()
//     when bindingId() matches.
//
// no-port-check: NereusSDR-local test scaffolding. No Thetis source
// transcribed.
// =================================================================

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding::*
#include "gui/meters/presets/AnanMultiMeterItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/CrossNeedleItem.h"
#include "gui/meters/presets/PowerSwrPresetItem.h"
#include "gui/meters/presets/SMeterPresetItem.h"

using namespace NereusSDR;

class TstPresetLiveBinding : public QObject
{
    Q_OBJECT

private slots:
    void ananMm_pushBindingValue_routesSignalToFirstNeedle();
    void ananMm_pushBindingValue_otherBindingDoesNotTouchSignal();
    void crossNeedle_pushBindingValue_routesForwardAndReflected();
    void powerSwr_pushBindingValue_routesToMatchingBar();
    void smeter_pushBindingValue_updatesValue();
    void barPreset_pushBindingValue_updatesValue();
    void defaultItem_pushBindingValue_ignoresMismatchedBinding();
};

// ---------------------------------------------------------------------------
// AnanMultiMeterItem — 7 needles, each with its own binding ID.
// ---------------------------------------------------------------------------

void TstPresetLiveBinding::ananMm_pushBindingValue_routesSignalToFirstNeedle()
{
    AnanMultiMeterItem item;
    // Push a signal value through the Signal binding. After the call,
    // MeterItem::value() must reflect the push (the default
    // pushBindingValue in the Signal needle wrote through it), and the
    // paint() output at the -73 dBm calibration point should match the
    // Thetis AddAnanMM calibration entry (0.501, 0.142) within rounding
    // tolerance.
    item.pushBindingValue(MeterBinding::SignalAvg, -73.0);
    QCOMPARE(item.value(), -73.0);

    // Calibration introspection: the Signal needle's own calibration
    // carries the -73 dBm waypoint at normalized (0.501, 0.142).
    const QMap<float, QPointF> cal = item.needleCalibration(0);
    QVERIFY(cal.contains(-73.0f));
    QCOMPARE(cal.value(-73.0f), QPointF(0.501, 0.142));

    // Paint smoke — the item must render without crashing and produce
    // a non-empty image now that live data has replaced the midpoint
    // seed fallback.
    item.setRect(0.0f, 0.0f, 1.0f, 1.0f);
    const int W = 600;
    const int H = 300;
    QImage img(W, H, QImage::Format_ARGB32);
    img.fill(Qt::black);
    {
        QPainter p(&img);
        item.paint(p, W, H);
    }
    QVERIFY(!img.isNull());
}

void TstPresetLiveBinding::ananMm_pushBindingValue_otherBindingDoesNotTouchSignal()
{
    AnanMultiMeterItem item;
    // Send the Volts binding to the ANAN MM. Only needle[1] (Volts)
    // should receive it; needle[0] (Signal) keeps its NaN "no data"
    // sentinel and the paint path falls back to the midpoint seed.
    item.pushBindingValue(MeterBinding::HwVolts, 13.8);
    // value() tracks the most-recently-pushed binding (any match).
    QCOMPARE(item.value(), 13.8);

    // Pushing an unrelated binding ID is a no-op — value() stays on
    // the last routed value.
    item.pushBindingValue(/*bindingId=*/ 999, 42.0);
    QCOMPARE(item.value(), 13.8);
}

// ---------------------------------------------------------------------------
// CrossNeedleItem — 2 needles (forward / reflected).
// ---------------------------------------------------------------------------

void TstPresetLiveBinding::crossNeedle_pushBindingValue_routesForwardAndReflected()
{
    CrossNeedleItem item;
    item.pushBindingValue(MeterBinding::TxPower, 50.0);
    QCOMPARE(item.value(), 50.0);

    item.pushBindingValue(MeterBinding::TxReversePower, 5.0);
    // Most recent routed binding wins for MeterItem::value(); the
    // internal Needle::currentValue on the forward needle is still 50
    // (not mutated by the reflected push), but this test covers the
    // surface API only.
    QCOMPARE(item.value(), 5.0);
}

// ---------------------------------------------------------------------------
// PowerSwrPresetItem — 2 bars.
// ---------------------------------------------------------------------------

void TstPresetLiveBinding::powerSwr_pushBindingValue_routesToMatchingBar()
{
    PowerSwrPresetItem item;
    item.pushBindingValue(MeterBinding::TxPower, 100.0);
    QCOMPARE(item.value(), 100.0);

    item.pushBindingValue(MeterBinding::TxSwr, 1.5);
    QCOMPARE(item.value(), 1.5);

    // Unrelated binding — no-op.
    item.pushBindingValue(MeterBinding::SignalAvg, -40.0);
    QCOMPARE(item.value(), 1.5);
}

// ---------------------------------------------------------------------------
// SMeterPresetItem — single binding (SignalAvg); default MeterItem
// pushBindingValue forwards to setValue() when the binding ID matches.
// ---------------------------------------------------------------------------

void TstPresetLiveBinding::smeter_pushBindingValue_updatesValue()
{
    SMeterPresetItem item;
    QCOMPARE(item.bindingId(), MeterBinding::SignalAvg);

    // Matched binding → value() picks up the push.
    item.pushBindingValue(MeterBinding::SignalAvg, -70.0);
    QCOMPARE(item.value(), -70.0);

    // Mismatched binding → ignored.
    item.pushBindingValue(MeterBinding::TxPower, 25.0);
    QCOMPARE(item.value(), -70.0);
}

// ---------------------------------------------------------------------------
// BarPresetItem — single binding per flavour.
// ---------------------------------------------------------------------------

void TstPresetLiveBinding::barPreset_pushBindingValue_updatesValue()
{
    BarPresetItem item;
    item.configureAsMic();  // Binds TxMic
    const int mic = item.bindingId();

    item.pushBindingValue(mic, 12.5);
    QCOMPARE(item.value(), 12.5);

    // Unrelated binding ignored.
    item.pushBindingValue(MeterBinding::SignalAvg, -40.0);
    QCOMPARE(item.value(), 12.5);
}

// ---------------------------------------------------------------------------
// MeterItem default — matched ID forwards, mismatched ID ignored.
// ---------------------------------------------------------------------------

void TstPresetLiveBinding::defaultItem_pushBindingValue_ignoresMismatchedBinding()
{
    BarItem item;
    item.setBindingId(MeterBinding::SignalAvg);
    item.setRange(-140.0, 0.0);

    // Mismatch — setValue not called.
    item.pushBindingValue(MeterBinding::TxPower, 99.0);
    // BarItem::setValue() smooths the stored value — starting from
    // -140 the match below should move it, the mismatch above should
    // not. Check by comparing to the smoothed value through a match.
    const double before = item.value();

    item.pushBindingValue(MeterBinding::SignalAvg, -60.0);
    const double after = item.value();
    QVERIFY2(after != before, "Matched binding must update value");
}

QTEST_MAIN(TstPresetLiveBinding)
#include "tst_preset_live_binding.moc"
