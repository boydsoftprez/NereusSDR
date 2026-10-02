// no-port-check: NereusSDR-original composite/source contract tests.
#include <QtTest>
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "models/RadioModel.h"
#include "core/session/TransmitStateFacade.h"
#include "core/RadioStatus.h"
#include "core/mmio/MmioEndpoint.h"
#include <memory>
using namespace NereusSDR;
class TwoChannel final : public MeterItem {
public:
    explicit TwoChannel(QObject* parent) : MeterItem(parent) {}
    QSet<int> readingBindings() const override { return bindings; }
    void pushBindingValue(int binding, double v) override { readings[binding] = v; ++pushes; }
    bool advanceMeter(qint64 ms) override { lastTime = ms; ++frames; return true; }
    void resetForTxTransition(bool tx) override { lastTx = tx; ++resets; }
    void setPowerScale(int watts) override { power = watts; }
    // Exercise the multi-layer contract even when renderLayer names its
    // static backdrop, as recovered complete faces historically did.
    Layer renderLayer() const override { return Layer::Background; }
    bool participatesIn(Layer layer) const override { return layer==Layer::Background || layer==Layer::OverlayDynamic; }
    void paint(QPainter&, int, int) override {}
    QSet<int> bindings{MeterBinding::SignalPeak, MeterBinding::SignalAvg};
    QHash<int,double> readings;
    int pushes=0, frames=0, resets=0, power=0;
    qint64 lastTime=-1;
    bool lastTx=false;
};
class TestCompositeDispatch : public QObject {
    Q_OBJECT
private slots:
    void widgetFanoutAndNewFaceReplay() {
        MeterWidget w;
        auto* face = new TwoChannel(&w); w.addItem(face);
        w.updateMeterValue(0,-42); w.updateMeterValue(1,-81);
        QCOMPARE(face->readings.value(0),-42.0); QCOMPARE(face->readings.value(1),-81.0);
        auto* added = new TwoChannel(&w); w.addItem(added);
        QCOMPARE(added->readings.value(0),-42.0); QCOMPARE(added->readings.value(1),-81.0);
        w.updateMeterValue(0,-400); QCOMPARE(added->readings.value(0),-400.0);
        w.rescalePowerMeters(5); w.resetForTxTransition(true); w.advanceMeters(700);
        QCOMPARE(added->power,5); QVERIFY(added->lastTx); QCOMPARE(added->lastTime,700);
    }
    void quietAndIrrelevantInputsPreserveCaches() {
        MeterWidget w;
        auto* bar = new BarItem(&w); bar->setBindingId(MeterBinding::SignalPeak);
        bar->setAttackRatio(1); w.addItem(bar);
        w.updateMeterValue(MeterBinding::SignalPeak,-42);
        const quint64 changes = w.readingInvalidationsForTest();
        w.updateMeterValue(MeterBinding::SignalAvg,-81);
        w.updateMeterValue(MeterBinding::SignalPeak,-42);
        QCOMPARE(w.readingInvalidationsForTest(),changes);
        bar->setDecayRatio(0.1f);
        w.updateMeterValue(MeterBinding::SignalPeak,-82);
        const double first = bar->smoothedValue();
        w.updateMeterValue(MeterBinding::SignalPeak,-82);
        QVERIFY(bar->smoothedValue()<first);
        QCOMPARE(w.readingInvalidationsForTest(),changes+2);
        bar->setShowHistory(true); w.updateMeterValue(MeterBinding::SignalPeak,-82);
        const int samples=bar->historySampleCount(); w.updateMeterValue(MeterBinding::SignalPeak,-82);
        QCOMPARE(bar->historySampleCount(),samples+1);
    }
    void radioReplayDoesNotOverwriteMmioSource() {
        MeterWidget w;
        w.updateMeterValue(MeterBinding::SignalPeak,-42);
        auto* item=new BarItem(&w); item->setBindingId(MeterBinding::SignalPeak);
        item->setMmioBinding(QUuid::createUuid(),QStringLiteral("test-cache")); w.addItem(item);
        w.updateMmioValue(item,-81);
        w.updateMeterValue(MeterBinding::SignalPeak,-42);
        QCOMPARE(item->value(),-81.0);
    }
    void mmioIdentityAvailabilityAndMissingSources() {
        RadioModel model(RadioModel::Role::Remote); model.setStationConnectionState(ConnectionState::Connected);
        MeterPoller p; p.setRemoteRadioModel(&model,[] { return true; });
        MmioEndpoint endpoint; endpoint.setGuid(QUuid::createUuid()); const QUuid guid=endpoint.guid();
        const QString prefix=guid.toString(QUuid::WithoutBraces)+QLatin1Char('.');
        endpoint.mergeBatch({{prefix+"alpha",-81.0},{prefix+"beta",-63.0}});
        bool endpointPresent=true; int lookups=0;
        p.setMmioEndpointSourceForTest([&](const QUuid& requested) -> MmioEndpoint* {
            ++lookups; return endpointPresent && requested==guid ? &endpoint : nullptr;
        });
        MeterWidget w, preview;
        const auto add=[&](MeterWidget& host,const QString& variable) {
            auto* text=new TextItem(&host); text->setBindingId(MeterBinding::AdcPeak);
            text->setMmioBinding(guid,variable); host.addItem(text); return text;
        };
        TextItem* alpha=add(w,"alpha"); TextItem* beta=add(w,"beta");
        p.addTarget(&w);
        QCOMPARE(alpha->value(),-81.0); QCOMPARE(beta->value(),-63.0);
        QVERIFY(!w.bindingUnavailableReason(MeterBinding::AdcPeak).isEmpty());
        QVERIFY(alpha->bindingUnavailableReason(MeterBinding::AdcPeak).isEmpty());
        QSignalSpy feed(&p,&MeterPoller::mmioReadingUpdated); lookups=0;
        QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection));
        QCOMPARE(lookups,2); QCOMPARE(feed.count(),2);
        QCOMPARE(feed.first().at(0).toUuid(),guid); QCOMPARE(feed.first().at(1).toString(),QStringLiteral("alpha"));
        QCOMPARE(alpha->value(),-81.0); QCOMPARE(beta->value(),-63.0);
        endpoint.mergeBatch({{prefix+"alpha",QStringLiteral("non-numeric")}});
        QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection));
        QCOMPARE(alpha->value(),-400.0); QVERIFY(alpha->displayText().startsWith("--"));
        QVERIFY(!alpha->bindingUnavailableReason(MeterBinding::AdcPeak).isEmpty()); QCOMPARE(beta->value(),-63.0);
        TextItem* copy=add(preview,"alpha"); p.replayReadings(&preview,{});
        QCOMPARE(copy->value(),-400.0); QVERIFY(!copy->bindingUnavailableReason(MeterBinding::AdcPeak).isEmpty());
        endpointPresent=false;
        p.replayReadings(&preview,{}); QVERIFY(copy->bindingUnavailableReason(MeterBinding::AdcPeak).contains("source"));
        QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection));
        QCOMPARE(beta->value(),-400.0); QVERIFY(!beta->bindingUnavailableReason(MeterBinding::AdcPeak).isEmpty());
        QCOMPARE(p.targetCountForTest(),1);
    }
    void compositeUpdatesPreserveStaticLayers() {
        MeterWidget w; auto* face=new TwoChannel(&w); w.addItem(face);
#ifdef NEREUS_GPU_SPECTRUM
        w.clearReadingLayerDirtyFlagsForTest();
        w.updateMeterValue(MeterBinding::SignalPeak,-42);
        QVERIFY(!w.backgroundDirtyForTest()); QVERIFY(!w.overlayStaticDirtyForTest()); QVERIFY(w.overlayDynamicDirtyForTest());
        w.clearReadingLayerDirtyFlagsForTest(); w.advanceMeters(100);
        QVERIFY(!w.backgroundDirtyForTest()); QVERIFY(!w.overlayStaticDirtyForTest()); QVERIFY(w.overlayDynamicDirtyForTest());
#endif
    }
    void disconnectedOrDestroyedRemoteSourceCannotReplay() {
        auto model=std::make_unique<RadioModel>(RadioModel::Role::Remote);
        model->setStationConnectionState(ConnectionState::Connected);
        MeterPoller p; bool ready=true; int reads=0;
        p.setRemoteRadioModel(model.get(),[&] { return ready; });
        p.setRxReadingSource([&](const QJsonObject&,int binding) { ++reads; return -40.0-binding; });
        const QJsonObject context{{"sliceId",7}};
        MeterWidget w, preview; auto* face=new TwoChannel(&w); w.addItem(face); p.setTargetContext(&w,context);
        auto* copy=new TwoChannel(&preview); preview.addItem(copy);
        QSignalSpy cadence(&p,&MeterPoller::frameAdvanced);
        QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection)); QCOMPARE(reads,9);
        ready=false; QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection));
        QCOMPARE(reads,9); QCOMPARE(face->readings.value(0),-400.0); p.replayReadings(&preview,context); QCOMPARE(copy->readings.value(0),-400.0);
        ready=true; QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection)); QCOMPARE(reads,18);
        model.reset(); p.replayReadings(&preview,context); QCOMPARE(copy->readings.value(0),-400.0);
        QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection)); QCOMPARE(reads,18); QCOMPARE(cadence.count(),4);
    }
    void destroyedCallbackUsesRawIdentitySafely() {
        MeterPoller p;
        auto w = std::make_unique<MeterWidget>();
        w->addItem(new TwoChannel(w.get())); p.addTarget(w.get());
        connect(w.get(), &QObject::destroyed, &p, [&](QObject* object) {
            p.removeTarget(static_cast<MeterWidget*>(object));
        });
        w.reset(); QCOMPARE(p.targetCountForTest(),0);
        QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection));
    }
    void settingsAndTxResetReplayWithoutResettingHardware() {
        MeterPoller p; MeterWidget w, replacement;
        auto* face = new TwoChannel(&w); w.addItem(face); p.addTarget(&w);
        p.setUnitMode(MeterItem::MeterUnit::uV); p.rescalePowerMeters(5); p.setInTx(true);
        auto* hardware = new BarItem(&w); hardware->setBindingId(MeterBinding::HwVolts);
        w.addItem(hardware); w.updateMeterValue(MeterBinding::HwVolts,48);
        const double volts = hardware->smoothedValue();
        p.setInTx(false); QCOMPARE(hardware->smoothedValue(),volts);
        p.setInTx(true); p.removeTarget(&w);
        auto* fresh = new TwoChannel(&replacement); replacement.addItem(fresh); p.addTarget(&replacement);
        QVERIFY(replacement.mox()); QCOMPARE(fresh->unitMode(),MeterItem::MeterUnit::uV); QCOMPARE(fresh->power,5);
    }
    void txOffEvictsPaAndAudioButRetainsHardwareReplay() {
        RadioModel model(RadioModel::Role::Remote);
        model.setStationConnectionState(ConnectionState::Connected);
        model.applyCorePaReadings({48.0,48.0,3.0,42.0});
        MeterPoller p; p.setPaReadingsModel(&model);
        p.setRadioStatus(&model.radioStatus());
        MeterWidget w, replacement;
        auto* face = new TwoChannel(&w); face->bindings={100,101,102,113,200,201,202}; w.addItem(face); p.addTarget(&w);
        p.setInTx(true); model.radioStatus().setPowerReadings(50,2,1.5);
        QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection));
        p.handOutTxReadingForTest(MeterBinding::TxMicPeak,7);
        QCOMPARE(face->readings.value(100),50.0);
        p.setInTx(false); p.removeTarget(&w);
        auto* fresh = new TwoChannel(&replacement); fresh->bindings=face->bindings; replacement.addItem(fresh); p.addTarget(&replacement);
        for (int b : {100,101,102,113}) { QCOMPARE(fresh->readings.value(b),-400.0); }
        QCOMPARE(fresh->readings.value(200),48.0); QCOMPARE(fresh->readings.value(201),3.0); QCOMPARE(fresh->readings.value(202),42.0);
        model.radioStatus().setPowerReadings(0,0,1);
        QCOMPARE(fresh->readings.value(100),0.0); QCOMPARE(fresh->readings.value(102),1.0);
    }
    void remotePeakAvailabilityKeepsAverageUseful() {
        RadioModel model(RadioModel::Role::Remote);
        model.setStationConnectionState(ConnectionState::Connected);
        TransmitState state;
        MeterPoller p; p.setRemoteRadioModel(&model, [] { return true; });
        p.setRadioStatus(&model.radioStatus()); // sanctioned continuing PA producer
        p.setRemoteTransmitState(&state, [] { return QString(); });
        MeterWidget w;
        auto* face = new TwoChannel(&w);
        face->bindings = {MeterBinding::TxMicPeak,MeterBinding::TxMic}; w.addItem(face); p.addTarget(&w);
        QVERIFY(state.applyStationValue("keyed",true));
        QVERIFY(state.applyStationValue("micLevelDb",-12.0));
        QVERIFY(state.applyStationValue("forwardPowerWatts",50.0));
        QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection));
        QCOMPARE(face->readings.value(MeterBinding::TxMic),-12.0);
        QCOMPARE(face->readings.value(MeterBinding::TxMicPeak),-400.0);
        QVERIFY(face->bindingUnavailableReason(MeterBinding::TxMic).isEmpty());
        QVERIFY(!face->bindingUnavailableReason(MeterBinding::TxMicPeak).isEmpty());
        auto* added = new TwoChannel(&w); added->bindings = face->bindings; w.addItem(added);
        QCOMPARE(added->readings.value(MeterBinding::TxMic),-12.0);
        QCOMPARE(added->bindingUnavailableReason(MeterBinding::TxMicPeak),MeterPoller::remoteTxMeterNotSentText());
        p.setRemoteTxStageReadingsAvailable([] { return true; });
        MeterWidget replacement; auto* pa=new TextItem(&replacement); pa->setBindingId(MeterBinding::TxPower); replacement.addItem(pa); p.addTarget(&replacement);
        QCOMPARE(pa->value(),50.0); // unrelated capability setter retains PA feed
        for (int peak : MeterPoller::remoteTxPeakBindingsNotSent()) {
            QCOMPARE(w.bindingUnavailableReason(peak),MeterPoller::remoteTxMeterNotSentText());
        }
    }
    void independentTxMappingAndFloors() {
        struct Pair { int average, peak; TxMeterType averageSource, peakSource; };
        const Pair pairs[]{
            {MeterBinding::TxMic,MeterBinding::TxMicPeak,TxMeterType::MicAvg,TxMeterType::MicPeak},
            {MeterBinding::TxAlc,MeterBinding::TxAlcPeak,TxMeterType::AlcAvg,TxMeterType::AlcPeak},
            {MeterBinding::TxComp,MeterBinding::TxCompPeak,TxMeterType::CompAvg,TxMeterType::CompPeak},
            {MeterBinding::TxEq,MeterBinding::TxEqPeak,TxMeterType::EqAvg,TxMeterType::EqPeak},
            {MeterBinding::TxLeveler,MeterBinding::TxLevelerPeak,TxMeterType::LevelerAvg,TxMeterType::LevelerPeak},
            {MeterBinding::TxCfc,MeterBinding::TxCfcPeak,TxMeterType::CfcAvg,TxMeterType::CfcPeak}
        };
        for (const Pair& pair : pairs) {
            const auto raw = [&](TxMeterType source) { return source==pair.peakSource ? 7.0 : source==pair.averageSource ? -12.0 : -400.0; };
            QCOMPARE(MeterPoller::txReadingForBinding(pair.peak,raw),7.0);
            QCOMPARE(MeterPoller::txReadingForBinding(pair.average,raw),-12.0);
            const double floor = pair.peak==MeterBinding::TxMicPeak || pair.peak==MeterBinding::TxAlcPeak ? -195.0 : -30.0;
            QCOMPARE(MeterPoller::txReadingForBinding(pair.peak,[](TxMeterType) { return -400.0; }),floor);
        }
        MeterPoller p; MeterWidget w;
        auto* face = new TwoChannel(&w); face->bindings={MeterBinding::TxMicPeak,MeterBinding::TxMic}; w.addItem(face); p.addTarget(&w);
        QSignalSpy feed(&p,&MeterPoller::readingUpdated);
        p.handOutTxReadingForTest(MeterBinding::TxMicPeak,7);
        p.handOutTxReadingForTest(MeterBinding::TxMic,-12);
        QCOMPARE(face->readings.value(MeterBinding::TxMicPeak),7.0);
        QCOMPARE(face->readings.value(MeterBinding::TxMic),-12.0);
        QCOMPARE(feed.count(),2);
        QVERIFY(!isNoReadingBinding(MeterBinding::TxMic));
        QVERIFY(isNoReadingBinding(MeterBinding::TxMicPeak));
    }
    void contextsAndReplacementNeverCrossFeed() {
        MeterPoller p;
        qint64 time=100; p.setMonotonicSourceForTest([&] { return time; });
        const QJsonObject a{{"sliceId",7}}, b{{"sliceId",23}};
        p.setRxReadingSource([](const QJsonObject& c,int binding) {
            return c.value("sliceId").toInt()==7 ? -40.0-binding : -90.0-binding;
        });
        MeterWidget one, two, replacement;
        auto* f1 = new TwoChannel(&one); one.addItem(f1);
        auto* f2 = new TwoChannel(&two); two.addItem(f2);
        p.setTargetContext(&one,a); p.setTargetContext(&two,b);
        QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection));
        QCOMPARE(f1->readings.value(0),-40.0); QCOMPARE(f2->readings.value(0),-90.0);
        QCOMPARE(f1->readings.value(1),-41.0); QCOMPARE(f1->frames,1);
        auto* fr = new TwoChannel(&replacement); replacement.addItem(fr);
        p.removeTarget(&one); p.setTargetContext(&replacement,a); p.addTarget(&replacement);
        QCOMPARE(p.targetCountForTest(),2); QCOMPARE(fr->readings.value(0),-40.0);
        p.setTargetContext(&replacement,b); QCOMPARE(fr->readings.value(0),-90.0);
        p.setRxReadingSource({});
        MeterWidget preview; auto* fp = new TwoChannel(&preview); preview.addItem(fp);
        p.replayReadings(&preview,a); QCOMPARE(fp->readings.value(0),-400.0);
        QCOMPARE(p.targetCountForTest(),2);
        QSignalSpy cadence(&p,&MeterPoller::frameAdvanced);
        ++time; QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection));
        p.setInTx(true); ++time; QVERIFY(QMetaObject::invokeMethod(&p,"poll",Qt::DirectConnection));
        QCOMPARE(cadence.count(),2); QCOMPARE(cadence.last().first().toLongLong(),time);
    }
};
QTEST_MAIN(TestCompositeDispatch)
#include "tst_meter_composite_dispatch.moc"
