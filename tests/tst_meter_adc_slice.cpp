// no-port-check: NereusSDR-original tests; the ADC bars read the ADC behind
// the slice each one is set to, and the ADC maximum bar names why it is empty.
#include <QtTest>
#include <memory>
#include "core/HpsdrModel.h"
#include "core/P2RadioConnection.h"
#include "core/RadioConnection.h"
#include "gui/containers/ContainerSourceAdapter.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
namespace {
// An ANAN-G2 with Slice A on ADC0 (ANT1) and Slice B on ADC1 (EXT1), the
// model's own DDC assignment putting each slice on the ADC its antenna feeds.
struct G2Station {
    P2RadioConnection conn;
    RadioModel model;
    SliceModel* sliceA{nullptr};
    SliceModel* sliceB{nullptr};
    G2Station()
    {
        conn.setBoardForTest(HPSDRHW::Saturn);
        model.injectConnectionForTest(&conn);
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        RadioInfo info;
        info.protocol = ProtocolVersion::Protocol2;
        model.setLastRadioInfoForTest(info);
        model.configureStreamPool(4, 4, 192000);
        sliceA = model.sliceById(model.addSlice());
        sliceB = model.sliceById(model.addSlice());
        if (sliceA && sliceB) {
            // Apart in frequency, so each slice gets a stream of its own.
            sliceA->setFrequency(14.200e6);
            sliceB->setFrequency(7.100e6);
            pick(sliceA, QStringLiteral("ANT1"));
            pick(sliceB, QStringLiteral("EXT1"));
        }
    }
    void pick(SliceModel* slice, const QString& antenna)
    {
        model.setActiveSlice(slice->sliceIndex());
        slice->setRxAntenna(antenna);
    }
};
BarPresetItem* addAdcBar(MeterWidget& widget, const QString& flavor = QStringLiteral("Adc"))
{
    auto item = std::make_unique<BarPresetItem>(&widget);
    if (!item->configureVariant(flavor)) { return nullptr; }
    item->setRect(0, 0, 1, 1);
    BarPresetItem* result = item.get();
    widget.addItem(item.release());
    return result;
}
QJsonObject sliceContext(const SliceModel* slice)
{
    return {{"sliceId", slice->sliceIndex()}};
}
}
class TestMeterAdcSlice : public QObject {
    Q_OBJECT
private slots:
    // Two ADC bars, one on Slice A (ADC0) and one on Slice B (ADC1), each
    // show the ADC reading of their own slice's receiver.
    void adcBarsOnTwoSlicesShowTheirOwnAdc()
    {
        G2Station s;
        QVERIFY(s.sliceA && s.sliceB);
        QVERIFY(s.sliceA->streamIndex() >= 0 && s.sliceB->streamIndex() >= 0);
        QCOMPARE(s.model.sliceAdcIndex(s.sliceA->sliceIndex()), 0);
        QCOMPARE(s.model.sliceAdcIndex(s.sliceB->sliceIndex()), 1);
        s.sliceA->setAdcPeakDbfs(-30); s.sliceA->setAdcAverageDbfs(-36);
        s.sliceB->setAdcPeakDbfs(-90); s.sliceB->setAdcAverageDbfs(-96);
        MeterPoller poller;
        poller.setRxReadingSource([&](const QJsonObject& context, int binding) {
            return ContainerSourceAdapter::reading(&s.model, context, s.model.activeSlice(), binding, true, true, {});
        });
        MeterWidget meterA, meterB;
        BarPresetItem* barA = addAdcBar(meterA);
        BarPresetItem* barB = addAdcBar(meterB);
        QVERIFY(barA && barB);
        QCOMPARE(barA->typeId(), QStringLiteral("meter.adc"));
        poller.setTargetContext(&meterA, sliceContext(s.sliceA));
        poller.setTargetContext(&meterB, sliceContext(s.sliceB));
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(barA->value(), -30.0);
        QCOMPARE(barB->value(), -90.0);
        QVERIFY(barA->hasAverageReading() && barB->hasAverageReading());
        // The average marker is smoothed (attack 0.2 from the -120 floor):
        // Slice A's moves toward -36 faster than Slice B's toward -96.
        QVERIFY(barA->averageValue() > barB->averageValue());
        // A new reading on one slice moves only that slice's bar.
        s.sliceB->setAdcPeakDbfs(-60);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(barA->value(), -30.0);
        QCOMPARE(barB->value(), -60.0);
    }
    // A Follow selected RX ADC bar reads the selected slice's ADC and
    // switches when another slice is selected.
    void followSelectedAdcBarSwitchesWithTheSelectedSlice()
    {
        G2Station s;
        QVERIFY(s.sliceA && s.sliceB);
        QCOMPARE(s.model.sliceAdcIndex(s.sliceB->sliceIndex()), 1);
        s.sliceA->setAdcPeakDbfs(-30);
        s.sliceB->setAdcPeakDbfs(-90);
        MeterPoller poller;
        poller.setRxSourceIdentitySource([&](const QJsonObject& context) {
            return ContainerSourceAdapter::sourceIdentity(&s.model, context, s.model.activeSlice());
        });
        poller.setRxReadingSource([&](const QJsonObject& context, int binding) {
            return ContainerSourceAdapter::reading(&s.model, context, s.model.activeSlice(), binding, true, true, {});
        });
        MeterWidget meter;
        BarPresetItem* bar = addAdcBar(meter);
        QVERIFY(bar);
        poller.setTargetContext(&meter, {{"rxSourceMode", "followSelectedRx"}});
        s.model.setActiveSlice(s.sliceA->sliceIndex());
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(bar->value(), -30.0);
        s.model.setActiveSlice(s.sliceB->sliceIndex());
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(bar->value(), -90.0);
        s.model.setActiveSlice(s.sliceA->sliceIndex());
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(bar->value(), -30.0);
    }
    // The ADC maximum bar has no reading provider: the meter veils it and
    // hovering it names the reason, and nothing clears that reason.
    void adcMaxBarVeilsWithItsReason()
    {
        const QString reason = QStringLiteral("ADC magnitude is not reported yet");
        MeterWidget meter;
        meter.resize(300, 80);
        BarPresetItem* bar = addAdcBar(meter, QStringLiteral("AdcMax"));
        QVERIFY(bar);
        QCOMPARE(bar->typeId(), QStringLiteral("meter.adcMax"));
        QCOMPARE(bar->bindingId(), -1);
        // The veil's rule (MeterWidget::drawUnavailableVeils): every reading
        // channel of the item carries a reason.
        const QSet<int> bindings = bar->readingBindings();
        QVERIFY(!bindings.isEmpty());
        for (int binding : bindings) { QCOMPARE(bar->bindingUnavailableReason(binding), reason); }
        QCOMPARE(meter.unavailableReasonAt(QPointF(150, 40)), reason);
        QVERIFY(!bar->hasPrimaryReading());
        // The meter clearing availability does not clear it.
        bar->setBindingUnavailable(bar->bindingId(), QString());
        QCOMPARE(meter.unavailableReasonAt(QPointF(150, 40)), reason);
        // A saved adcMax bar loads with the same reason.
        BarPresetItem loaded;
        QVERIFY(loaded.deserialize(bar->serialize()));
        QCOMPARE(loaded.bindingUnavailableReason(loaded.bindingId()), reason);
        // Another face configured on the same item drops the reason.
        QVERIFY(bar->configureVariant(QStringLiteral("Adc")));
        QVERIFY(bar->bindingUnavailableReason(-1).isEmpty());
        QVERIFY(meter.unavailableReasonAt(QPointF(150, 40)).isEmpty());
        // The ADC bar itself has its provider and no reason.
        QVERIFY(bar->bindingUnavailableReason(MeterBinding::AdcPeak).isEmpty());
    }
};
QTEST_MAIN(TestMeterAdcSlice)
#include "tst_meter_adc_slice.moc"
