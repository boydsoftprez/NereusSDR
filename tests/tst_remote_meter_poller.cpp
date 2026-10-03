// no-port-check: NereusSDR-original remote meter wiring regressions.
// Modification history (NereusSDR):
//   2026-09-25: iPhone app plan Task 39 (D14, R-IOS-13): a remote window's
//               transmit meters from the Core's `txState`, and the ones it
//               does not send shown disabled with the reason. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-26: trunk merge of remote transmit (R-R3-49): the ADC and AGC
//               meters shown disabled with the reason on a connected Core
//               below meterReadingsVersion 1. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: remote-window parity Task 33 (R-R3-49): the window's SWR is
//               the Core's, sent in txState; TxComp waits for
//               txReadingsVersion 1. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
#include <QTest>
#include <QSignalSpy>

#include "core/RadioStatus.h"
#include "core/HardwareProfile.h"
#include "core/RxChannel.h"
#include "gui/SMeterWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterItem.h"
#include "core/StepAttenuatorController.h"
#include "core/session/TransmitStateFacade.h"
#include "gui/HGauge.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "gui/meters/presets/AnanMultiMeterItem.h"
#include "core/session/StationCapabilities.h"

#include <memory>

using namespace NereusSDR;

class TestRemoteMeterPoller : public QObject {
    Q_OBJECT
private slots:
    void ananSupportFollowsResolvedRemoteProvider() {
        using Support=MeterItem::BindingSupport;
        RadioModel model(RadioModel::Role::Remote); MeterPoller poller; bool ready=false;
        poller.setRemoteRadioModel(&model,[&]{return ready;}); poller.setPaReadingsModel(&model);
        poller.setRemoteTxStageReadingsAvailable([&]{return model.stationTxReadingsVersion()>=3;});
        MeterWidget live; auto* face=new AnanMultiMeterItem(&live); live.addItem(face); poller.addTarget(&live);
        QCOMPARE(face->bindingSupport(201),Support::Unknown);
        StationCapabilities caps; caps.radioConnected=true; caps.board=HPSDRHW::Saturn; caps.hpsdrModel=HPSDRModel::ANAN_G2;
        caps.macAddress="AA:BB:CC:DD:EE:01"; caps.txReadingsVersion=1; ready=true; model.applyStationCapabilities(caps);
        QVERIFY(model.addSliceWithStationId(7)>=0); model.activeSlice()->setSignalAverageDbm(-85);
        poller.setRadioStatus(&model.radioStatus());
        QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection);
        QCOMPARE(face->bindingSupport(201),Support::Supported); QCOMPARE(face->bindingSupport(109),Support::Unsupported);
        QCOMPARE(face->bindingSupport(110),Support::Unsupported); QVERIFY(!face->channelHasReading(2));
        RadioModel::PaReadings readings; readings.paCurrentAmps=0; readings.paVolts=13.8; model.applyCorePaReadings(readings);
        QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection); live.advanceMeters(100); QVERIFY(face->channelHasReading(2));
        QCOMPARE(face->bindingSupport(1),Support::Supported); QCOMPARE(face->bindingSupport(200),Support::Supported);
        QVERIFY(face->channelHasReading(0)); QVERIFY(face->channelHasReading(1)); QVERIFY(face->channelVisible(0));
        live.resetForTxTransition(true); model.radioStatus().setPowerReadings(70,1,1.6); live.advanceMeters(200);
        QCOMPARE(face->bindingSupport(100),Support::Supported); QCOMPARE(face->bindingSupport(102),Support::Supported);
        QVERIFY(face->channelHasReading(3)); QVERIFY(face->channelHasReading(4)); QVERIFY(face->channelVisible(3)); QVERIFY(!face->channelVisible(0));
        const QString saved=face->serialize(); MeterWidget preview; auto* copy=new AnanMultiMeterItem(&preview); preview.addItem(copy);
        poller.replayReadings(&preview,{}); preview.advanceMeters(100); QCOMPARE(copy->bindingSupport(201),Support::Supported);
        caps.txReadingsVersion=3; model.applyStationCapabilities(caps); QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection);
        QCOMPARE(face->bindingSupport(109),Support::Supported); QVERIFY(!face->channelHasReading(5));
        // Missing samples on this same known sensor are not hardware incapability.
        readings.paCurrentAmps.reset(); model.applyCorePaReadings(readings); ready=false;
        QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection); QVERIFY(!face->channelHasReading(2)); QCOMPARE(face->bindingSupport(201),Support::Supported);
        // A different, explicitly unsupported board must not inherit old support or samples.
        caps.board=HPSDRHW::Hermes; caps.hpsdrModel=HPSDRModel::HERMES; caps.macAddress="AA:BB:CC:DD:EE:02"; ready=true;
        const HardwareProfile unsupported=profileForStation(caps.board,caps.hpsdrModel);
        QCOMPARE(unsupported.effectiveBoard,HPSDRHW::Hermes); QCOMPARE(unsupported.model,HPSDRModel::HERMES);
        QVERIFY(unsupported.caps); QVERIFY(!unsupported.caps->hasPaAmpsTelemetry); QVERIFY(!unsupported.caps->hasPaVoltsTelemetry);
        model.applyStationCapabilities(caps); QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection);
        QCOMPARE(face->bindingSupport(201),Support::Unsupported); QVERIFY(!face->channelHasReading(2));
        poller.copyCachedReadings(&preview,{}); QCOMPARE(copy->bindingSupport(201),Support::Unsupported); QVERIFY(!copy->channelHasReading(2));
        QCOMPARE(face->serialize(),saved);
        caps.board=HPSDRHW::Unknown; caps.macAddress.clear(); model.applyStationCapabilities(caps);
        QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection); QCOMPARE(face->bindingSupport(201),Support::Unknown);
    }
    // R-R3-46 fix wave: the Core adds its own calibration (its attenuator,
    // preamp and meter offset) to the S-meter readings and the spectrum
    // frames it sends. A remote window's model adds none on top, so the
    // spectrum's calibration offset and Max Bin show the Core's values;
    // a local model keeps the Thetis chain.
    void remoteModelAddsNoSecondCalibration()
    {
        StepAttenuatorController controller;
        controller.setTickTimerEnabled(false);
        controller.setStepAttEnabled(true);
        controller.setAttenuation(20);

        RadioModel remote(RadioModel::Role::Remote);
        QSignalSpy changed(&remote, &RadioModel::rxMeterOffsetChanged);
        remote.setStepAttController(&controller);
        QCOMPARE(remote.rxMeterOffsetDb(), 0.0);
        controller.setAttenuation(25);
        QCOMPARE(remote.rxMeterOffsetDb(), 0.0);
        // Whatever it tells the spectrum is 0.
        for (const QList<QVariant>& emitted : std::as_const(changed)) {
            QCOMPARE(emitted.at(0).toDouble(), 0.0);
        }
        remote.setStepAttController(nullptr);

        RadioModel local;
        local.setStepAttController(&controller);
        QVERIFY(local.rxMeterOffsetDb() != 0.0);
        local.setStepAttController(nullptr);
    }

    void selectsActualRemoteSourceAndFollowsStableActiveSlice()
    {
        RadioModel model(RadioModel::Role::Remote);
        model.setStationConnectionState(ConnectionState::Connected);
        QVERIFY(model.addSliceWithStationId(7) >= 0);
        QVERIFY(model.addSliceWithStationId(23) >= 0);
        SliceModel* first = model.sliceById(7);
        SliceModel* second = model.sliceById(23);
        QVERIFY(first && second);
        first->setSignalPeakDbm(-62);
        first->setSignalAverageDbm(-81);
        second->setSignalPeakDbm(-42);
        second->setSignalAverageDbm(-59);
        model.setActiveSlice(0);

        SMeterWidget meter;
        MeterWidget bars;
        auto* peak = new TextItem(&bars);
        auto* average = new TextItem(&bars);
        peak->setBindingId(MeterBinding::SignalPeak);
        average->setBindingId(MeterBinding::SignalAvg);
        bars.addItem(peak);
        bars.addItem(average);
        MeterPoller poller;
        poller.setSMeter(&meter);
        poller.addTarget(&bars);
        int localCalibrationCalls = 0;
        poller.setRxOffsetSource([&]() { ++localCalibrationCalls; return 37.0; });
        poller.setRemoteRadioModel(&model, []() { return true; }, [](const SliceModel* slice) {
            return slice->sliceIndex() == 7 ? -67.0 : -47.0;
        });
        QSignalSpy flags(&poller, &MeterPoller::remoteSliceLevelUpdated);
        auto tick = [&]() {
            QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        };

        meter.setRxMode("S-Meter");
        tick();
        QCOMPARE(meter.levelDbm(), -62.0f);
        QCOMPARE(peak->value(), -62.0);
        QCOMPARE(average->value(), -81.0);
        QCOMPARE(flags.count(), 2);
        QCOMPARE(flags.at(0).at(0).toInt(), 7);
        QCOMPARE(flags.at(0).at(1).toDouble(), -62.0);
        QCOMPARE(flags.at(1).at(0).toInt(), 23);
        QCOMPARE(flags.at(1).at(1).toDouble(), -42.0);

        meter.setRxMode("Sig Avg");
        tick();
        QCOMPARE(meter.levelDbm(), -81.0f);
        model.setActiveSlice(1);
        tick();
        QCOMPARE(meter.levelDbm(), -59.0f);
        meter.setRxMode("Signal Peak");
        tick();
        QCOMPARE(meter.levelDbm(), -42.0f);
        meter.setRxMode("Max Bin");
        tick();
        QCOMPARE(meter.levelDbm(), -47.0f);
        QCOMPARE(flags.constLast().at(1).toDouble(), -47.0);
        QCOMPARE(localCalibrationCalls, 0);
    }

    void disconnectDeletionAndTxCannotPollLocalDspOrKeepLiveRxReading()
    {
        auto model = std::make_unique<RadioModel>(RadioModel::Role::Remote);
        model->setStationConnectionState(ConnectionState::Connected);
        QVERIFY(model->addSliceWithStationId(12) >= 0);
        model->setActiveSlice(0);
        SliceModel* slice = model->sliceById(12);
        slice->setSignalPeakDbm(-52);
        SMeterWidget meter;
        meter.setRxMode("S-Meter");
        MeterWidget bars;
        auto* peakText = new TextItem(&bars);
        auto* averageText = new TextItem(&bars);
        peakText->setBindingId(MeterBinding::SignalPeak);
        averageText->setBindingId(MeterBinding::SignalAvg);
        bars.addItem(peakText);
        bars.addItem(averageText);
        slice->setSignalAverageDbm(-66);
        MeterPoller poller;
        poller.setSMeter(&meter);
        poller.addTarget(&bars);
        bool snapshotReady = true;
        poller.setRemoteRadioModel(model.get(), [&]() { return snapshotReady; });
        auto tick = [&]() {
            QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        };
        tick();
        QCOMPARE(meter.levelDbm(), -52.0f);
        QCOMPARE(peakText->displayText(), QStringLiteral("-52.0 dBm"));
        QCOMPARE(averageText->displayText(), QStringLiteral("-66.0 dBm"));

        model->radioStatus().setTransmitting(true);
        slice->setSignalPeakDbm(-30);
        tick();
        QCOMPARE(meter.levelDbm(), -52.0f);
        model->radioStatus().setTransmitting(false);
        tick();
        QCOMPARE(meter.levelDbm(), -30.0f);

        // R-R3-13: with no reading the meter gets the -400 dBm no-reading
        // sentinel in every RX mode and shows "--", not "-140 dBm" / "S0".
        QSignalSpy flags(&poller, &MeterPoller::remoteSliceLevelUpdated);
        model->setStationConnectionState(ConnectionState::Disconnected);
        snapshotReady = false;
        for (const char* mode : {"S-Meter", "Sig Avg", "Signal Peak", "Max Bin"}) {
            meter.setRxMode(QString::fromLatin1(mode));
            tick();
            QCOMPARE(meter.levelDbm(), -400.0f);
            QCOMPARE(meter.sUnitsText(), QStringLiteral("--"));
            QVERIFY(!flags.isEmpty());
            QCOMPARE(flags.constLast().at(1).toDouble(), -400.0);
            // The container meter items show no reading too, not -140.
            QCOMPARE(peakText->value(), -400.0);
            QCOMPARE(averageText->value(), -400.0);
            QCOMPARE(peakText->displayText(), QStringLiteral("-- dBm"));
            QCOMPARE(averageText->displayText(), QStringLiteral("-- dBm"));
        }
        meter.setRxMode("S-Meter");
        // Capabilities can mark the radio connected before the new model
        // snapshot arrives. Retained readings must not become live again.
        model->setStationConnectionState(ConnectionState::Connected);
        tick();
        QCOMPARE(meter.levelDbm(), -400.0f);
        QCOMPARE(flags.constLast().at(1).toDouble(), -400.0);
        QCOMPARE(peakText->displayText(), QStringLiteral("-- dBm"));
        slice->setSignalPeakDbm(-74);
        snapshotReady = true;
        tick();
        QCOMPARE(meter.levelDbm(), -74.0f);
        QCOMPARE(flags.constLast().at(1).toDouble(), -74.0);
        QCOMPARE(peakText->displayText(), QStringLiteral("-74.0 dBm"));
        QCOMPARE(averageText->displayText(), QStringLiteral("-66.0 dBm"));
        model.reset();
        tick();
        QCOMPARE(meter.levelDbm(), -400.0f);
        QCOMPARE(peakText->displayText(), QStringLiteral("-- dBm"));
        QCOMPARE(averageText->displayText(), QStringLiteral("-- dBm"));
    }

    // R-R3-13 / R-R3-49 (parity Task 15, B2.5): container meters bound to
    // ADC Peak, ADC Average, AGC Gain, AGC Peak and AGC Average move with
    // the Core's readings for the active slice; on a Core below
    // meterReadingsVersion 1 they show no reading, never a frozen value.
    void adcAndAgcMetersFollowTheCoresReadings()
    {
        RadioModel model(RadioModel::Role::Remote);
        model.setStationConnectionState(ConnectionState::Connected);
        QVERIFY(model.addSliceWithStationId(4) >= 0);
        QVERIFY(model.addSliceWithStationId(9) >= 0);
        model.setActiveSlice(0);
        SliceModel* first = model.sliceById(4);
        SliceModel* second = model.sliceById(9);
        QVERIFY(first && second);
        const auto seed = [](SliceModel* slice, double base) {
            slice->setAdcPeakDbfs(base);
            slice->setAdcAverageDbfs(base - 10.0);
            slice->setAgcGainDb(base + 60.0);
            slice->setAgcPeakDb(base - 20.0);
            slice->setAgcAverageDb(base - 30.0);
        };
        seed(first, -20.0);
        seed(second, -40.0);

        MeterWidget bars;
        const int bindings[] = {MeterBinding::AdcPeak, MeterBinding::AdcAvg,
                                MeterBinding::AgcGain, MeterBinding::AgcPeak,
                                MeterBinding::AgcAvg};
        QList<TextItem*> items;
        for (int binding : bindings) {
            auto* item = new TextItem(&bars);
            item->setBindingId(binding);
            bars.addItem(item);
            items.append(item);
        }
        MeterPoller poller;
        poller.addTarget(&bars);
        int localCalibrationCalls = 0;
        poller.setRxOffsetSource([&]() { ++localCalibrationCalls; return 37.0; });
        poller.setRemoteRadioModel(&model, []() { return true; });
        bool coreSendsReadings = false;
        poller.setRemoteMeterReadingsAvailable([&]() { return coreSendsReadings; });
        auto tick = [&]() {
            QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        };
        const auto expectValues = [&](const QList<double>& expected) {
            for (int i = 0; i < items.size(); ++i) {
                QCOMPARE(items.at(i)->value(), expected.at(i));
            }
        };

        // Trunk merge of remote transmit (join b): whether each binding is
        // shown disabled, and why.
        const auto expectUnavailable = [&](const QString& reason) {
            for (int binding : bindings) {
                QCOMPARE(bars.bindingUnavailableReason(binding), reason);
            }
        };

        // A Core below version 1: no reading, even with values on the slice,
        // and the five meters shown disabled with the reason.
        tick();
        expectValues({-400.0, -400.0, -400.0, -400.0, -400.0});
        for (TextItem* item : std::as_const(items)) {
            QVERIFY(item->displayText().startsWith(QStringLiteral("--")));
        }
        expectUnavailable(MeterPoller::remoteMeterReadingsNotSentText());
        QVERIFY(!MeterPoller::remoteMeterReadingsNotSentText().isEmpty());
        // The signal meters the Core always sends are not marked.
        QVERIFY(bars.bindingUnavailableReason(MeterBinding::SignalPeak).isEmpty());

        coreSendsReadings = true;
        tick();
        expectValues({-20.0, -30.0, 40.0, -40.0, -50.0});
        expectUnavailable(QString());
        // They move with the Core's readings, and follow the active slice.
        first->setAgcGainDb(47.5);
        tick();
        QCOMPARE(items.at(2)->value(), 47.5);
        model.setActiveSlice(1);
        tick();
        expectValues({-40.0, -50.0, 20.0, -60.0, -70.0});
        // The Core's values as sent: nothing added in this window.
        QCOMPARE(localCalibrationCalls, 0);

        // The Core stops sending (an older Core after a reconnect): no
        // reading, not the last value.
        coreSendsReadings = false;
        tick();
        expectValues({-400.0, -400.0, -400.0, -400.0, -400.0});

        expectUnavailable(MeterPoller::remoteMeterReadingsNotSentText());

        // No snapshot yet: no reading either, and nothing to say about a
        // Core the window has not reached.
        coreSendsReadings = true;
        poller.setRemoteRadioModel(&model, []() { return false; });
        tick();
        expectValues({-400.0, -400.0, -400.0, -400.0, -400.0});
        expectUnavailable(QString());
        coreSendsReadings = false;
        tick();
        expectUnavailable(QString());
    }

    void localPollWithoutRxChannelShowsNoReading()
    {
        // R-R3-13: a local window with no RX channel (none yet, or the
        // QPointer cleared when the channel was destroyed) feeds the
        // no-reading sentinel instead of leaving the last value frozen.
        MeterWidget bars;
        auto* peakText = new TextItem(&bars);
        auto* agcText = new TextItem(&bars);
        auto* bar = new BarItem(&bars);
        peakText->setBindingId(MeterBinding::SignalPeak);
        agcText->setBindingId(MeterBinding::AgcAvg);
        bar->setBindingId(MeterBinding::SignalAvg);
        bar->setShowValue(true);
        bars.addItem(peakText);
        bars.addItem(agcText);
        bars.addItem(bar);
        bars.updateMeterValue(MeterBinding::SignalPeak, -71.0);
        bars.updateMeterValue(MeterBinding::AgcAvg, -90.0);
        bars.updateMeterValue(MeterBinding::SignalAvg, -75.0);
        QCOMPARE(peakText->displayText(), QStringLiteral("-71.0 dBm"));

        SMeterWidget meter;
        meter.setLevel(-71.0f);
        QCOMPARE(meter.levelDbm(), -71.0f);

        MeterPoller poller;
        poller.addTarget(&bars);
        poller.setSMeter(&meter);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(peakText->displayText(), QStringLiteral("-- dBm"));
        QCOMPARE(agcText->displayText(), QStringLiteral("-- dBm"));
        QCOMPARE(bar->valueText(), QStringLiteral("--"));
        // Fix wave, Important 2: the analog S-meter header must not freeze
        // on the last reading when the channel is gone either.
        QCOMPARE(meter.levelDbm(), -400.0f);
    }

    void localLinkLostWithLiveChannelShowsNoReading()
    {
        // Fix wave, Important 2: on a local LinkLost the RX channels stay
        // alive but WDSP meters stop updating, and an inactive channel reads
        // -140 dBm, which would show as a number. While the radio link is
        // not up, the poller shows no reading; once it is up again it reads
        // the same channel.
        MeterWidget bars;
        auto* peakText = new TextItem(&bars);
        peakText->setBindingId(MeterBinding::SignalPeak);
        bars.addItem(peakText);
        SMeterWidget meter;
        RxChannel channel(0, 1024, 48000);

        MeterPoller poller;
        poller.addTarget(&bars);
        poller.setSMeter(&meter);
        poller.setRxChannel(&channel);
        auto tick = [&]() {
            QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        };
        tick();
        QCOMPARE(peakText->displayText(), QStringLiteral("-140.0 dBm"));
        QVERIFY(meter.levelDbm() > -400.0f);

        poller.setLocalRxReadingAvailable(false);
        QVERIFY(!poller.localRxReadingAvailable());
        tick();
        QCOMPARE(peakText->displayText(), QStringLiteral("-- dBm"));
        QCOMPARE(meter.levelDbm(), -400.0f);

        poller.setLocalRxReadingAvailable(true);
        tick();
        QCOMPARE(peakText->displayText(), QStringLiteral("-140.0 dBm"));
        QVERIFY(meter.levelDbm() > -400.0f);
    }

    // iPhone app plan Task 39: the remote window shows forward and reflected
    // power, SWR, ALC and MIC from the Core's `txState` by the same meter
    // items the local window uses, and the meters the Core does not send
    // are disabled with the reason, never hidden.
    void remoteTxMetersComeFromTheCoresTransmitState()
    {
        RadioModel model(RadioModel::Role::Remote);
        model.setStationConnectionState(ConnectionState::Connected);
        QVERIFY(model.addSliceWithStationId(0) >= 0);
        model.setActiveSlice(0);
        model.sliceById(0)->setSignalPeakDbm(-70);

        MeterWidget bars;
        bars.resize(200, 200);
        QHash<int, TextItem*> items;
        const QList<int> bindings{MeterBinding::TxPower, MeterBinding::TxReversePower,
                                  MeterBinding::TxSwr,   MeterBinding::TxAlc,
                                  MeterBinding::TxMic,   MeterBinding::TxComp,
                                  MeterBinding::TxEq,    MeterBinding::SignalPeak};
        for (int i = 0; i < bindings.size(); ++i) {
            auto* item = new TextItem(&bars);
            item->setBindingId(bindings.at(i));
            item->setRect(0.0f, 0.1f * i, 1.0f, 0.1f);
            bars.addItem(item);
            items.insert(bindings.at(i), item);
        }
        MeterPoller poller;
        poller.addTarget(&bars);
        poller.setRadioStatus(&model.radioStatus());
        poller.setRemoteRadioModel(&model, []() { return true; });
        TransmitState state;
        QString unavailable;
        poller.setRemoteTransmitState(&state, [&unavailable]() { return unavailable; });
        auto tick = [&]() {
            QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        };

        // The meters txState carries are available; the others are shown
        // disabled with the reason.
        for (int binding : {MeterBinding::TxPower, MeterBinding::TxReversePower,
                            MeterBinding::TxSwr, MeterBinding::TxAlc, MeterBinding::TxMic}) {
            QVERIFY2(bars.bindingUnavailableReason(binding).isEmpty(), qPrintable(QString::number(binding)));
        }
        for (int binding : MeterPoller::remoteTxBindingsNotSent()) {
            QCOMPARE(bars.bindingUnavailableReason(binding), MeterPoller::remoteTxMeterNotSentText());
        }
        QCOMPARE(bars.items().size(), bindings.size());   // none hidden
        QCOMPARE(bars.unavailableReasonAt(QPointF(10, 0.1 * 200 * 6 + 5)),
                 MeterPoller::remoteTxMeterNotSentText());  // the TxEq row
        // Parity Task 33 follow-up: TxComp waits for txReadingsVersion 1.
        QCOMPARE(bars.bindingUnavailableReason(MeterBinding::TxComp),
                 TransmitState::txReadingNotSentText());
        QVERIFY(bars.unavailableReasonAt(QPointF(10, 5)).isEmpty());  // TxPower

        // The Core keys and reads its meters.
        QVERIFY(state.applyStationValue("keyed", true));
        QVERIFY(state.applyStationValue("forwardPowerWatts", 50.0));
        QVERIFY(state.applyStationValue("reflectedPowerWatts", 2.0));
        // Parity Task 33: SWR as the Core worked it (50 W forward, 2 W
        // reflected: rho 0.2, 1.2 / 0.8).
        QVERIFY(state.applyStationValue("swr", 1.5));
        QVERIFY(state.applyStationValue("alcDb", -3.0));
        QVERIFY(state.applyStationValue("micLevelDb", -12.0));
        tick();
        QCOMPARE(items.value(MeterBinding::TxPower)->value(), 50.0);
        QCOMPARE(items.value(MeterBinding::TxReversePower)->value(), 2.0);
        // SWR from 50 W forward and 2 W reflected: rho 0.2, (1.2 / 0.8).
        QVERIFY(qAbs(items.value(MeterBinding::TxSwr)->value() - 1.5) < 1e-9);
        QCOMPARE(items.value(MeterBinding::TxAlc)->value(), -3.0);
        QCOMPARE(items.value(MeterBinding::TxMic)->value(), -12.0);
        QCOMPARE(model.radioStatus().forwardPowerWatts(), 50.0);

        // Unkeyed: the power falls to 0 with the Core's reading; the receive
        // meters come back.
        QVERIFY(state.applyStationValue("keyed", false));
        QVERIFY(state.applyStationValue("forwardPowerWatts", 0.0));
        QVERIFY(state.applyStationValue("reflectedPowerWatts", 0.0));
        QVERIFY(state.applyStationValue("swr", 1.0));
        tick();
        QCOMPARE(items.value(MeterBinding::TxPower)->value(), 0.0);
        QCOMPARE(items.value(MeterBinding::TxSwr)->value(), 1.0);
        QCOMPARE(items.value(MeterBinding::SignalPeak)->value(), -70.0);

        // A Core that does not send transmit meters: every transmit meter is
        // disabled with that reason, and nothing is fed.
        unavailable = QStringLiteral("This Core does not send transmit meters.");
        tick();
        QCOMPARE(bars.bindingUnavailableReason(MeterBinding::TxPower), unavailable);
        QCOMPARE(bars.bindingUnavailableReason(MeterBinding::TxComp), unavailable);
        unavailable.clear();
        tick();
        QVERIFY(bars.bindingUnavailableReason(MeterBinding::TxPower).isEmpty());
    }

    void aGaugeCanBeShownUnavailable()
    {
        HGauge gauge;
        QVERIFY(!gauge.isUnavailable());
        gauge.setUnavailable(true);
        QVERIFY(gauge.isUnavailable());
        gauge.setUnavailable(false);
        QVERIFY(!gauge.isUnavailable());
    }
};
QTEST_MAIN(TestRemoteMeterPoller)
#include "tst_remote_meter_poller.moc"
