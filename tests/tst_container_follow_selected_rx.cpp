// no-port-check: NereusSDR-original GUI selected-RX source regression tests.
#include <QtTest>
#include <QComboBox>
#include <memory>
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/containers/ContainerSourceAdapter.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/SMeterWidget.h"
#include "gui/PanadapterStack.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/DspControlThread.h"
#include "core/RxChannel.h"
using namespace NereusSDR;
namespace {
QJsonObject followContext()
{
    return {{"rxSourceMode", "followSelectedRx"}};
}
BarItem* addSignalBar(MeterWidget& widget)
{
    auto item = std::make_unique<BarItem>(&widget);
    item->setBindingId(MeterBinding::SignalPeak);
    item->setRange(-140, 0);
    item->setAttackRatio(1);
    item->setDecayRatio(0.1f);
    item->setShowHistory(true);
    BarItem* result = item.get();
    widget.addItem(item.release());
    return result;
}
}
class TestContainerFollowSelectedRx : public QObject {
    Q_OBJECT
private slots:
    void symbolicSourceOverridesInheritedFixedAndFixedOverridesFollow()
    {
        ContainerDocument document;
        document.config = {{"sliceId", 7}, {"sessionId", "station"}};
        ContentEntry entry;
        entry.context = followContext();
        const QJsonObject follow = ContainerContentHost::effectiveContext(document, entry);
        QCOMPARE(follow.value("rxSourceMode").toString(), QStringLiteral("followSelectedRx"));
        QVERIFY(!follow.contains("sliceId"));
        QCOMPARE(follow.value("sessionId").toString(), QStringLiteral("station"));
        document.config = followContext();
        entry.context = {{"sliceId", 23}};
        const QJsonObject fixed = ContainerContentHost::effectiveContext(document, entry);
        QCOMPARE(fixed.value("sliceId").toInt(), 23);
        QVERIFY(!fixed.contains("rxSourceMode"));
        entry.context = {{"rxSource", 2}};
        const QJsonObject legacy = ContainerContentHost::effectiveContext(document, entry);
        QCOMPARE(legacy.value("sliceId").toInt(), 1);
        QVERIFY(!legacy.contains("rxSourceMode"));
        entry.context = {};
        QCOMPARE(ContainerContentHost::effectiveContext(document, entry), followContext());
    }
    void dropdownPersistsSymbolicChoiceAndKeepsOtherContext()
    {
        ContainerContentRegistry registry;
        ContentPropertyEditor editor(registry);
        editor.setContainerDefaults({{"sliceId", 2}});
        ContentEntry entry = registry.makeEntry("meter.signalText");
        entry.context = {{"sessionId", "station"}, {"mmioVariable", "kept"}, {"sliceId", 1}};
        ContentEntry edited;
        connect(&editor, &ContentPropertyEditor::entryEdited, this, [&](const ContentEntry& value) { edited = value; });
        editor.setEntry(entry);
        QComboBox* source = editor.findChild<QComboBox*>("contentSlice");
        QVERIFY(source);
        const int follow = source->findText(QStringLiteral("Follow selected RX"));
        QVERIFY2(follow >= 0, "The per-object source selector must offer Follow selected RX");
        source->setCurrentIndex(follow);
        QCOMPARE(edited.context.value("rxSourceMode").toString(), QStringLiteral("followSelectedRx"));
        QVERIFY(!edited.context.contains("sliceId"));
        QVERIFY(!edited.context.contains("rxSource"));
        QCOMPARE(edited.context.value("sessionId"), entry.context.value("sessionId"));
        QCOMPARE(edited.context.value("mmioVariable"), entry.context.value("mmioVariable"));
        editor.setEntry(edited);
        source = editor.findChild<QComboBox*>("contentSlice");
        QCOMPARE(source->currentText(), QStringLiteral("Follow selected RX"));
        source->setCurrentIndex(source->findData(0));
        QCOMPARE(edited.context.value("sliceId").toInt(), 0);
        QVERIFY(!edited.context.contains("rxSourceMode"));
        source->setCurrentIndex(0);
        QVERIFY(!edited.context.contains("sliceId"));
        QVERIFY(!edited.context.contains("rxSourceMode"));
    }
    void symbolicChoiceSurvivesPortableRoundTrip()
    {
        ContainerContentRegistry registry;
        ContainerDocument document;
        document.id = "follow-container";
        document.layout = ContentLayout::VerticalStack;
        document.config = {{"sliceId", 0}};
        ContentEntry entry = registry.makeEntry("meter.signalText");
        entry.context = followContext();
        document.contents = {entry};
        const DocumentResult decoded = ContainerDocumentCodec::importContainer(ContainerDocumentCodec::exportContainer(document));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document.containers.first().contents.first().context, followContext());
        QVERIFY(!decoded.document.containers.first().contents.first().context.contains("sliceId"));
    }
    void cacheOnlyFollowUsesStableSelectedSliceAndNeverFallsBack()
    {
        RadioModel model(RadioModel::Role::Remote);
        QVERIFY(model.addSliceWithStationId(7) >= 0);
        QVERIFY(model.addSliceWithStationId(23) >= 0);
        SliceModel* a = model.sliceById(7);
        SliceModel* b = model.sliceById(23);
        a->setStreamIndex(0); b->setStreamIndex(1);
        a->setSignalPeakDbm(-35); b->setSignalPeakDbm(-95);
        QJsonObject context = followContext();
        // Explicit follow also rejects stale fixed context from older hosts.
        context["sliceId"] = 7;
        QCOMPARE(ContainerSourceAdapter::slice(&model, context, b), b);
        QCOMPARE(ContainerSourceAdapter::reading(&model, context, b, MeterBinding::SignalPeak, true, true, {}), -95.0);
        const SliceModel* maxBinSource = nullptr;
        const auto maxBin = [&](const SliceModel* source) { maxBinSource = source; return -91.0; };
        QCOMPARE(ContainerSourceAdapter::reading(&model, context, b, MeterBinding::SignalMaxBin, true, true, maxBin), -91.0);
        QCOMPARE(maxBinSource, b);
        QVERIFY(!ContainerSourceAdapter::slice(&model, context, nullptr));
        QCOMPARE(ContainerSourceAdapter::reading(&model, context, nullptr, MeterBinding::SignalPeak, true, true, {}), kNoMeterReadingDbm);
        QCOMPARE(ContainerSourceAdapter::reading(&model, {{"sliceId", 7}}, b, MeterBinding::SignalPeak, true, true, {}), -35.0);
        context["sessionId"] = "foreign";
        QVERIFY(!ContainerSourceAdapter::slice(&model, context, b, "station"));
        context.remove("sessionId"); b->setStreamIndex(-1);
        QCOMPARE(ContainerSourceAdapter::reading(&model, context, b, MeterBinding::SignalPeak, true, true, {}), kNoMeterReadingDbm);
    }
    void receiverSelectionChangesFollowButBarePanActivationDoesNot()
    {
        RadioModel model(RadioModel::Role::Remote);
        model.setStationConnectionState(ConnectionState::Connected);
        QVERIFY(model.addSliceWithStationId(7) >= 0); QVERIFY(model.addSliceWithStationId(23) >= 0);
        model.sliceById(7)->setStreamIndex(0); model.sliceById(23)->setStreamIndex(1);
        model.sliceById(7)->setSignalPeakDbm(-35); model.sliceById(23)->setSignalPeakDbm(-95);
        model.applyStationActiveSlice(7);
        MeterPoller poller;
        poller.setRemoteRadioModel(&model, [] { return true; });
        poller.setRxSourceIdentitySource([&](const QJsonObject& context) {
            return ContainerSourceAdapter::sourceIdentity(&model, context, model.activeSlice());
        });
        poller.setRxReadingSource([&](const QJsonObject& context, int binding) {
            return ContainerSourceAdapter::reading(&model, context, model.activeSlice(), binding, true, true, {});
        });
        MeterWidget meter, fixed;
        BarItem* bar = addSignalBar(meter); poller.setTargetContext(&meter, followContext());
        BarItem* fixedBar = addSignalBar(fixed); poller.setTargetContext(&fixed, {{"sliceId", 7}});
        PanadapterStack stack; QVERIFY(stack.addPanadapter("pan-1")); stack.setActivePan("pan-1");
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(bar->value(), -35.0);
        const int fixedHistory = fixedBar->historySampleCount();
        model.applyStationActiveSlice(23);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(bar->value(), -95.0); QCOMPARE(bar->peakValue(), -95.0);
        QVERIFY(fixedBar->historySampleCount() > fixedHistory);
        QCOMPARE(fixedBar->peakValue(), -35.0);
        stack.setActivePan("pan-0");
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(bar->value(), -95.0);
        model.applyStationActiveSlice(7);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(bar->value(), -35.0);
    }
    void selectedSwitchClearsLiveAndUnregisteredPreviewPeakHistory()
    {
        RadioModel model(RadioModel::Role::Remote);
        QVERIFY(model.addSliceWithStationId(7) >= 0);
        QVERIFY(model.addSliceWithStationId(23) >= 0);
        SliceModel* selected = model.sliceById(7);
        model.sliceById(7)->setStreamIndex(0); model.sliceById(23)->setStreamIndex(1);
        model.sliceById(7)->setSignalPeakDbm(-35); model.sliceById(23)->setSignalPeakDbm(-95);
        MeterPoller poller;
        poller.setRxSourceIdentitySource([&](const QJsonObject& context) {
            return ContainerSourceAdapter::sourceIdentity(&model, context, selected);
        });
        poller.setRxReadingSource([&](const QJsonObject& context, int binding) {
            return ContainerSourceAdapter::reading(&model, context, selected, binding, true, true, {});
        });
        MeterWidget live, fixed, preview;
        BarItem* follows = addSignalBar(live);
        BarItem* stays = addSignalBar(fixed);
        BarItem* copy = addSignalBar(preview);
        const QJsonObject context = followContext();
        poller.setTargetContext(&live, context);
        poller.setTargetContext(&fixed, {{"sliceId", 7}});
        poller.replayReadings(&preview, context);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        poller.copyCachedReadings(&preview, context);
        QVERIFY(follows->historySampleCount() >= 2);
        const int fixedHistory = stays->historySampleCount();
        QCOMPARE(follows->peakValue(), -35.0);
        selected = model.sliceById(23);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        poller.copyCachedReadings(&preview, context);
        QCOMPARE(follows->value(), -95.0);
        QCOMPARE(follows->peakValue(), -95.0);
        QCOMPARE(copy->peakValue(), -95.0);
        QCOMPARE(follows->historySampleCount(), 1);
        QCOMPARE(copy->historySampleCount(), 1);
        QVERIFY(stays->historySampleCount() > fixedHistory);
        QCOMPARE(stays->peakValue(), -35.0);
        selected = nullptr;
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        poller.copyCachedReadings(&preview, context);
        QCOMPARE(follows->value(), kNoMeterReadingDbm);
        QCOMPARE(copy->value(), kNoMeterReadingDbm);
        QCOMPARE(follows->historySampleCount(), 0);
        QVERIFY(follows->valueText().startsWith("--"));
        selected = model.sliceById(7);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        poller.copyCachedReadings(&preview, context);
        QCOMPARE(follows->value(), -35.0);
        QCOMPARE(copy->peakValue(), -35.0);
        QCOMPARE(poller.targetCountForTest(), 2);
    }
    void streamEpochSessionAndSameContextRecipientsResetBeforeFanout()
    {
        RadioModel model(RadioModel::Role::Remote);
        QVERIFY(model.addSliceWithStationId(7) >= 0);
        SliceModel* selected = model.sliceById(7);
        selected->setStreamIndex(0); selected->setStreamEpoch(1); selected->setSignalPeakDbm(-30);
        QString session = "one";
        int reads = 0;
        MeterPoller poller;
        poller.setRxSourceIdentitySource([&](const QJsonObject& context) {
            return ContainerSourceAdapter::sourceIdentity(&model, context, selected, session);
        });
        poller.setRxReadingSource([&](const QJsonObject& context, int binding) {
            ++reads;
            return ContainerSourceAdapter::reading(&model, context, selected, binding, true, true, {}, session);
        });
        MeterWidget one, two, preview;
        BarItem* first = addSignalBar(one);
        BarItem* second = addSignalBar(two);
        BarItem* copy = addSignalBar(preview);
        poller.setTargetContext(&one, followContext()); poller.setTargetContext(&two, followContext());
        poller.replayReadings(&preview, followContext());
        reads = 0;
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(reads, 9); // one existing cache pass shared by both targets
        selected->setStreamEpoch(2); selected->setSignalPeakDbm(-100);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        poller.copyCachedReadings(&preview, followContext());
        QCOMPARE(first->peakValue(), -100.0); QCOMPARE(second->peakValue(), -100.0);
        QCOMPARE(copy->peakValue(), -100.0);
        QCOMPARE(first->historySampleCount(), 1); QCOMPARE(second->historySampleCount(), 1);
        selected->setSignalPeakDbm(-20);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        selected->setSignalPeakDbm(-110); session = "two";
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        poller.copyCachedReadings(&preview, followContext());
        QCOMPARE(first->peakValue(), -110.0); QCOMPARE(second->peakValue(), -110.0);
        QCOMPARE(copy->peakValue(), -110.0);
        selected->setStreamIndex(1); selected->setSignalPeakDbm(-120);
        poller.copyCachedReadings(&preview, followContext());
        QCOMPARE(copy->peakValue(), -120.0); QCOMPARE(copy->historySampleCount(), 1);
    }
    void previewGraphUsesIdentityWithoutBecomingPollingTarget()
    {
        RadioModel model(RadioModel::Role::Remote);
        QVERIFY(model.addSliceWithStationId(7) >= 0);
        QVERIFY(model.addSliceWithStationId(23) >= 0);
        SliceModel* selected = model.sliceById(7);
        model.sliceById(7)->setStreamIndex(0); model.sliceById(23)->setStreamIndex(1);
        model.sliceById(7)->setSignalAverageDbm(-35); model.sliceById(23)->setSignalAverageDbm(-95);
        qint64 time = 100;
        MeterPoller poller;
        poller.setMonotonicSourceForTest([&] { return time; });
        poller.setRxSourceIdentitySource([&](const QJsonObject& context) {
            return ContainerSourceAdapter::sourceIdentity(&model, context, selected);
        });
        poller.setRxReadingSource([&](const QJsonObject& context, int binding) {
            return ContainerSourceAdapter::reading(&model, context, selected, binding, true, true, {});
        });
        ContainerContentRegistry registry;
        ContainerPreviewWidget preview(registry, poller);
        ContainerDocument document; document.id = "preview-follow"; document.layout = ContentLayout::VerticalStack;
        document.config = {{"sliceId", 7}};
        ContentEntry entry = registry.makeEntry("meter.historyGraph"); entry.context = followContext(); document.contents = {entry};
        preview.setDocument(document);
        const QList<MeterWidget*> surfaces = preview.findChildren<MeterWidget*>();
        QCOMPARE(surfaces.size(), 1);
        CompositePresetItem* graph = qobject_cast<CompositePresetItem*>(surfaces.first()->items().first());
        QVERIFY(graph);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        time += 100;
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(graph->historySampleCountForTest(), 2);
        selected = model.sliceById(23); time += 100;
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(graph->historySampleCountForTest(), 1);
        QVERIFY(graph->channelHasReading(0));
        selected = nullptr; time += 100;
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(graph->historySampleCountForTest(), 0);
        QVERIFY(!graph->channelHasReading(0));
        QCOMPARE(poller.targetCountForTest(), 0);
    }
    void rxResetPreservesMixedCompositeTxPaReplayAndMmio()
    {
        MeterWidget widget;
        auto face = std::make_unique<CompositePresetItem>(CompositePresetItem::Face::Anan, &widget);
        CompositePresetItem* mixed = face.get(); widget.addItem(face.release());
        widget.updateMeterValue(MeterBinding::SignalAvg, -35);
        widget.updateMeterValue(MeterBinding::TxPower, 70);
        widget.updateMeterValue(MeterBinding::HwVolts, 48);
        widget.advanceMeters(100);
        const double power = mixed->channelValue(3), volts = mixed->channelValue(1);
        QVERIFY(mixed->channelHasReading(0)); QVERIFY(mixed->channelHasReading(3));
        auto mmio = std::make_unique<BarItem>(&widget);
        mmio->setBindingId(MeterBinding::SignalPeak); mmio->setShowHistory(true);
        mmio->setMmioBinding(QUuid::createUuid(), "independent");
        BarItem* external = mmio.get(); widget.addItem(mmio.release()); widget.updateMmioValue(external, -55);
        const int mmioHistory = external->historySampleCount();
        widget.resetRxSource();
        QVERIFY(!mixed->channelHasReading(0));
        QVERIFY(mixed->channelHasReading(3)); QVERIFY(mixed->channelHasReading(1));
        QCOMPARE(mixed->channelValue(3), power); QCOMPARE(mixed->channelValue(1), volts);
        QCOMPARE(external->value(), -55.0); QCOMPARE(external->historySampleCount(), mmioHistory);
        auto powerText = std::make_unique<TextItem>(&widget); powerText->setBindingId(MeterBinding::TxPower);
        TextItem* powerCopy = powerText.get(); widget.addItem(powerText.release());
        QCOMPARE(powerCopy->value(), 70.0);
        BarPresetItem bar;
        bar.configureAsMic();
        QVERIFY(bar.applyConfiguration({{"secondaryBindingId", MeterBinding::SignalAvg}, {"ignoreHistoryMs", 0}}));
        // The MIC face's primary is its peak binding; the average binding was
        // replaced with RX above, so seeding TxMic cannot establish TX state.
        QCOMPARE(bar.bindingId(), MeterBinding::TxMicPeak);
        bar.pushBindingValue(bar.bindingId(), -12); bar.pushBindingValue(MeterBinding::SignalAvg, -50);
        QVERIFY(bar.hasPrimaryReading()); QVERIFY(bar.hasAverageReading());
        bar.advanceMeter(100);
        bar.pushBindingValue(bar.bindingId(), -20); bar.pushBindingValue(MeterBinding::SignalAvg, -80);
        bar.advanceMeter(200);
        const double mic = bar.primaryValue(), micPeak = bar.peakValue(), micHistory = bar.minimumHistory();
        bar.resetRxSource();
        QVERIFY(bar.hasPrimaryReading()); QVERIFY(!bar.hasAverageReading()); QCOMPARE(bar.primaryValue(), mic);
        QCOMPARE(bar.peakValue(), micPeak); QCOMPARE(bar.minimumHistory(), micHistory);
    }
    void analogFollowClearsHeldPeakOnSelectedSwitch()
    {
        MeterPoller poller; SMeterWidget smeter;
        int selected = 7; double reading = -35;
        poller.setRxSourceIdentitySource([&](const QJsonObject&) { return QByteArray::number(selected); });
        poller.setRxReadingSource([&](const QJsonObject&, int) { return reading; });
        poller.setSMeter(&smeter); poller.setSMeterContext(followContext()); smeter.setPeakHoldEnabled(true);
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(smeter.testPeakLevel(), -35.0f);
        selected = 23; reading = -95;
        QVERIFY(QMetaObject::invokeMethod(&poller, "poll", Qt::DirectConnection));
        QCOMPARE(smeter.testPeakLevel(), -95.0f);
    }
    void legacyFocusedChannelDestructionPreservesUnrelatedAdaptedFixedSource()
    {
        RadioModel model(RadioModel::Role::Remote);
        QVERIFY(model.addSliceWithStationId(7) >= 0); QVERIFY(model.addSliceWithStationId(23) >= 0);
        model.sliceById(7)->setStreamIndex(0); model.sliceById(23)->setStreamIndex(1);
        model.sliceById(7)->setSignalPeakDbm(-50); model.sliceById(23)->setSignalPeakDbm(-35);
        SliceModel* selected = model.sliceById(7);
        // Supplying an unstarted lane prevents WDSP object creation; this
        // fixture needs only the wrapper's actual QObject destruction signal.
        DspControlThread lane(DspLane::Receive);
        auto channel = std::make_unique<RxChannel>(0, 1024, 48000, &lane, nullptr);
        MeterPoller adapted, legacy;
        adapted.setRxSourceIdentitySource([&](const QJsonObject& context) {
            return ContainerSourceAdapter::sourceIdentity(&model, context, selected);
        });
        adapted.setRxReadingSource([&](const QJsonObject& context, int binding) {
            return ContainerSourceAdapter::reading(&model, context, selected, binding, true, true, {});
        });
        adapted.setRxChannel(channel.get()); legacy.setRxChannel(channel.get());
        MeterWidget fixed, follow, old;
        BarItem* fixedBar = addSignalBar(fixed);
        BarItem* followBar = addSignalBar(follow);
        BarItem* legacyBar = addSignalBar(old);
        adapted.setTargetContext(&fixed, {{"sliceId", 23}});
        adapted.setTargetContext(&follow, followContext()); legacy.addTarget(&old);
        old.updateMeterValue(MeterBinding::SignalPeak, -45);
        QVERIFY(QMetaObject::invokeMethod(&adapted, "poll", Qt::DirectConnection));
        const int history = fixedBar->historySampleCount();
        QCOMPARE(fixedBar->peakValue(), -35.0);
        selected = nullptr;
        channel.reset();
        // The unrelated fixed source is still valid, even between poll ticks.
        QCOMPARE(fixedBar->value(), -35.0);
        QCOMPARE(fixedBar->peakValue(), -35.0); QCOMPARE(fixedBar->historySampleCount(), history);
        QCOMPARE(legacyBar->value(), kNoMeterReadingDbm);
        QCOMPARE(legacyBar->historySampleCount(), 0);
        model.sliceById(23)->setSignalPeakDbm(-95);
        QVERIFY(QMetaObject::invokeMethod(&adapted, "poll", Qt::DirectConnection));
        QCOMPARE(fixedBar->value(), -95.0); QCOMPARE(fixedBar->peakValue(), -35.0);
        QVERIFY(fixedBar->historySampleCount() > history);
        QCOMPARE(followBar->value(), kNoMeterReadingDbm); QCOMPARE(followBar->historySampleCount(), 0);
        // Link-wide availability remains authoritative for every adapted RX.
        adapted.setLocalRxReadingAvailable(false);
        QCOMPARE(fixedBar->value(), kNoMeterReadingDbm); QCOMPARE(fixedBar->historySampleCount(), 0);
    }
};
QTEST_MAIN(TestContainerFollowSelectedRx)
#include "tst_container_follow_selected_rx.moc"
