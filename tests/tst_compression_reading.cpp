// no-port-check: NereusSDR regression tests (R-R3-21) for the Compression
// reading. Cites below are for the values under test.
//
// What the operator sees from the transmit Compression reading:
//   - Thetis floors the reading at -30: max(-30, TXA_COMP_AV)
//     (console.cs:46979 with dsp.cs:1056 [v2.10.3.15]; the two negations
//     cancel). With PROC off WDSP returns -400 (meter.c xmeter), which
//     reads -30.
//   - The container Compression bar (Thetis AddCompBar calibration
//     -30 / 0 / 12) sits at its left edge at rest and with PROC off, and
//     moves right as the compressed level rises.
//   - A reversed HGauge maps as AetherSDR's does ([@1e0718ad]): its
//     maximum is empty, its minimum is a full bar.

#include <QtTest/QtTest>

#include "gui/HGauge.h"
#include "gui/meters/ItemGroup.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"

#include <QSignalSpy>

#include <limits>
#include <memory>

using namespace NereusSDR;

namespace {

BarItem* compressionBar(ItemGroup* group)
{
    for (MeterItem* item : group->items()) {
        if (auto* bar = dynamic_cast<BarItem*>(item)) {
            if (bar->bindingId() == MeterBinding::TxComp) { return bar; }
        }
    }
    return nullptr;
}

} // namespace

class TestCompressionReading : public QObject
{
    Q_OBJECT

private slots:
    void readingTakesThetisFloor()
    {
        // PROC off / no transmit signal: WDSP's -400 reads -30.
        QCOMPARE(MeterPoller::compressionReading(-400.0), -30.0);
        QCOMPARE(MeterPoller::compressionReading(-31.0), -30.0);
        QCOMPARE(MeterPoller::compressionReading(
                     std::numeric_limits<double>::quiet_NaN()), -30.0);
        // A real compressed level keeps its value and its sign.
        QCOMPARE(MeterPoller::compressionReading(-10.0), -10.0);
        QCOMPARE(MeterPoller::compressionReading(0.0), 0.0);
        QCOMPARE(MeterPoller::compressionReading(3.0), 3.0);
    }

    // The poller's own hand-out (what pollTxMeters() does with each raw
    // WDSP reading) applies Thetis's floor to each reading.
    void pollerHandsOutTheFlooredReading()
    {
        MeterPoller poller;
        QSignalSpy spy(&poller, &MeterPoller::txMeterReading);
        poller.handOutTxReadingForTest(MeterBinding::TxComp, -400.0);
        poller.handOutTxReadingForTest(MeterBinding::TxComp, -10.0);
        poller.handOutTxReadingForTest(MeterBinding::TxAlc, -400.0);
        QCOMPARE(spy.size(), 3);
        QCOMPARE(spy.at(0).at(0).toInt(), MeterBinding::TxComp);
        QCOMPARE(spy.at(0).at(1).toDouble(), -30.0);
        QCOMPARE(spy.at(1).at(1).toDouble(), -10.0);
        // D14, R-R3-49: ALC takes Thetis's own -30 floor too
        // (console.cs:46982 [v2.10.3.15]); every TX reading is covered in
        // tst_tx_meter_reading.
        QCOMPARE(spy.at(2).at(1).toDouble(), -30.0);
    }

    void containerBarAtRestProcOffAndCompressing()
    {
        std::unique_ptr<ItemGroup> group(ItemGroup::createCompPreset());
        BarItem* bar = compressionBar(group.get());
        QVERIFY(bar != nullptr);

        const double rest = MeterPoller::compressionReading(-400.0);
        QCOMPARE(bar->valueToNormalizedX(rest), 0.0f);

        // A -10 dB compressed level: 20 dB above the -30 end of the
        // -30 -> 0 (at 0.665) segment.
        const float at10 = bar->valueToNormalizedX(MeterPoller::compressionReading(-10.0));
        QVERIFY(qAbs(at10 - 0.665f * 20.0f / 30.0f) < 1e-4f);

        // The bar moves right as the level rises (not the wrong way).
        QVERIFY(bar->valueToNormalizedX(-20.0) < at10);
        QVERIFY(at10 < bar->valueToNormalizedX(0.0));
    }

    void reversedGaugeMaxIsEmpty()
    {
        HGauge gauge;
        gauge.setRange(-25.0, 0.0);
        gauge.setReversed(true);
        gauge.setValue(0.0);
        QCOMPARE(gauge.filledFraction(), 0.0);
        gauge.setValue(-25.0);
        QCOMPARE(gauge.filledFraction(), 1.0);
        gauge.setValue(-10.0);
        QVERIFY(qAbs(gauge.filledFraction() - 0.4) < 1e-9);
    }

    void normalGaugeMinIsEmpty()
    {
        HGauge gauge;
        gauge.setRange(-25.0, 0.0);
        gauge.setValue(-25.0);
        QCOMPARE(gauge.filledFraction(), 0.0);
        gauge.setValue(-10.0);
        QVERIFY(qAbs(gauge.filledFraction() - 0.6) < 1e-9);
    }
};

QTEST_MAIN(TestCompressionReading)
#include "tst_compression_reading.moc"
