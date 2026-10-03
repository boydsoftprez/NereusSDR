// no-port-check: NereusSDR-original actual native inert draft preview regressions.
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QDir>
#include <QLineEdit>
#include <QListWidget>
#include <QSplitter>
#include <QCheckBox>
#include <QPushButton>
#include <QScopeGuard>
#include "core/RadioDiscovery.h"
#include "core/RadioStatus.h"
#include "gui/MainWindow.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/ClockItem.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/AppSettings.h"
#include "core/mmio/MmioEndpoint.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/meter_property_editors/BaseItemEditor.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/WebImageItem.h"
using namespace NereusSDR;
class TstContainerPreview : public QObject {
    Q_OBJECT
    static ContentEntry rxFace(ContainerContentRegistry& registry) {
        auto entry=registry.makeEntry("meter.signal"); auto config=entry.config.value("properties").toObject();
        config["attack"]=1; config["decay"]=1; config["ignoreHistoryMs"]=0; entry.config["properties"]=config; return entry;
    }
private slots:
    void unpolledSourcesSharedCadenceAndNativeInertness() {
        QWidget liveParent; QWidget singleton(&liveParent); ContainerContentRegistry registry; registry.attachSingleton("applet:rx",&singleton);
        MeterPoller poller; double value=-72; poller.setRxReadingSource([&](const QJsonObject& context,int){ return context.value("sliceId").toInt()==1 ? value : -400.0; });
        ContainerPreviewWidget preview(registry,poller); ContainerDocument d; d.id="A"; d.layout=ContentLayout::VerticalStack;
        auto face=rxFace(registry); face.context["sliceId"]=1; auto applet=registry.makeEntry("applet:rx"); auto unknown=registry.makeEntry("DISCORDBTNS"); d.contents={face,applet,unknown};
        preview.setDocument(d); preview.resize(560,340); preview.show(); const auto meters=preview.findChildren<MeterWidget*>(); QCOMPARE(meters.size(),1); QCOMPARE(poller.targetCountForTest(),0); QCOMPARE(singleton.parentWidget(),&liveParent);
        auto* bar=qobject_cast<BarPresetItem*>(meters[0]->items()[0]); QVERIFY(bar); QVERIFY(bar->hasPrimaryReading()); QCOMPARE(bar->value(),-72.0);
        value=-45; poller.frameAdvanced(100); poller.frameAdvanced(200); QCOMPARE(bar->peakValue(),-45.0); value=-80; poller.frameAdvanced(300); QCOMPARE(bar->peakValue(),-45.0);
        d.name="rename"; preview.setDocument(d); QCOMPARE(preview.findChildren<MeterWidget*>()[0],meters[0]); QCOMPARE(bar->peakValue(),-45.0);
        value=-400; poller.frameAdvanced(400); QVERIFY(!bar->hasPrimaryReading());
        d.contents[0].context["sliceId"]=99; preview.setDocument(d); bar=qobject_cast<BarPresetItem*>(preview.findChild<MeterWidget*>()->items()[0]); QVERIFY(!bar->hasPrimaryReading());
        d.contents[0].context["sliceId"]=1; value=-60; preview.setDocument(d); auto* meter=preview.findChild<MeterWidget*>(); bar=qobject_cast<BarPresetItem*>(meter->items()[0]); QVERIFY(bar->hasPrimaryReading()); QCOMPARE(bar->value(),-60.0);
#ifdef NEREUS_GPU_SPECTRUM
        QSignalSpy frames(meter,&QRhiWidget::frameSubmitted); meter->update(); QTRY_VERIFY_WITH_TIMEOUT(frames.count()>0,3000); const QImage image=meter->grabFramebuffer();
#else
        const QImage image=meter->grab().toImage();
#endif
        QVERIFY(!image.isNull()); const QString capture=qEnvironmentVariable("TASK8_CAPTURE_DIR"); if(!capture.isEmpty()) { QVERIFY(QDir().mkpath(capture)); QVERIFY(image.save(capture+"/meter-preview.png")); QVERIFY(preview.grab().save(capture+"/mixed-preview.png")); }
        QCOMPARE(singleton.parentWidget(),&liveParent); QCOMPARE(poller.targetCountForTest(),0);
        auto contest=registry.makeEntry("meter.contest"); auto* view=registry.createPreview(contest,&preview); auto* faceMeter=qobject_cast<MeterWidget*>(view); QVERIFY(faceMeter);
        auto* composite=qobject_cast<CompositePresetItem*>(faceMeter->items()[0]); QVERIFY(composite); QVERIFY(composite->signalsBlocked()); for(auto* child:composite->internalItems()) { QVERIFY(child->signalsBlocked()); }
        auto web=registry.makeEntry("WEBIMAGE"); std::unique_ptr<MeterItem> item(registry.createMeterItem(web,nullptr,ContentRenderMode::Preview)); QVERIFY(item); QVERIFY(item->signalsBlocked()); QVERIFY(!qobject_cast<WebImageItem*>(item.get())->fetchEnabled());
    }
    void freshInheritedSourceUnchangedLeavesAndMmioIdentity() {
        ContainerContentRegistry registry; MeterPoller poller; int inherited=0;
        poller.setRxReadingSource([&](const QJsonObject& c,int){ return c.value("sliceId").toInt(inherited)==0?-72.0:-51.0; });
        ContainerDocument d; d.id="A"; d.layout=ContentLayout::VerticalStack;
        auto first=rxFace(registry), second=rxFace(registry); second.context["sliceId"]=1; d.contents={first,second};
        ContainerPreviewWidget preview(registry,poller); preview.setDocument(d);
        auto* untouched=preview.findChildren<MeterWidget*>()[0]; auto* bar=qobject_cast<BarPresetItem*>(untouched->items()[0]); QCOMPARE(bar->value(),-72.0);
        d.contents[1].name="changed second"; preview.setDocument(d); QCOMPARE(preview.findChildren<MeterWidget*>()[0],untouched);
        inherited=1; MeterWidget live; auto* liveBar=qobject_cast<BarPresetItem*>(registry.createMeterItem(rxFace(registry),&live,ContentRenderMode::Preview)); live.addItem(liveBar); poller.replayReadings(&live,{}); QCOMPARE(liveBar->value(),-51.0);
        poller.frameAdvanced(100); QCOMPARE(bar->value(),-51.0); QCOMPARE(poller.targetCountForTest(),0);
        MmioEndpoint endpoint; endpoint.setGuid(QUuid::createUuid()); const auto guid=endpoint.guid(); const auto prefix=guid.toString(QUuid::WithoutBraces)+".";
        endpoint.mergeBatch({{prefix+"alpha",-83.0},{prefix+"beta",-62.0}}); bool available=true;
        poller.setMmioEndpointSourceForTest([&](const QUuid& id)->MmioEndpoint*{ return available && id==guid ? &endpoint : nullptr; });
        auto mmio=registry.makeEntry("TEXT"); mmio.context["sessionId"]="other"; mmio.context["mmioGuid"]=guid.toString(); mmio.context["mmioVariable"]="alpha"; mmio.context["bindingId"]=MeterBinding::AdcPeak;
        d.contents={mmio}; preview.setDocument(d); auto* text=qobject_cast<TextItem*>(preview.findChild<MeterWidget*>()->items()[0]); QVERIFY(text); QCOMPARE(text->value(),-83.0);
        endpoint.mergeBatch({{prefix+"beta",-33.0}}); poller.frameAdvanced(200); QCOMPARE(text->value(),-83.0);
        d.contents[0].context["mmioVariable"]="beta"; preview.setDocument(d); text=qobject_cast<TextItem*>(preview.findChild<MeterWidget*>()->items()[0]); QCOMPARE(text->value(),-33.0);
        endpoint.mergeBatch({{prefix+"beta",QString("not numeric")}}); poller.frameAdvanced(300); QCOMPARE(text->value(),-400.0); QVERIFY(!text->bindingUnavailableReason(MeterBinding::AdcPeak).isEmpty());
        available=false; poller.frameAdvanced(400); QCOMPARE(text->value(),-400.0); QVERIFY(text->bindingUnavailableReason(MeterBinding::AdcPeak).contains("source"));
        QCOMPARE(poller.targetCountForTest(),0);
    }
    void foreignSessionRefusesWindowTelemetryInLiveAndPreview() {
        ContainerContentRegistry registry; MeterPoller poller; QString session="this";
        poller.setSessionIdSource([&] { return session; });
        RadioModel model(RadioModel::Role::Remote); model.setStationConnectionState(ConnectionState::Connected);
        model.applyCorePaReadings({48.0,48.0,3.0,42.0}); poller.setPaReadingsModel(&model); poller.setRadioStatus(&model.radioStatus());
        poller.setInTx(true); QVERIFY(QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection));
        model.radioStatus().setPowerReadings(50,2,1.5); poller.handOutTxReadingForTest(MeterBinding::TxMic,7);
        const auto entryFor=[&](int binding,const QString& session) {
            auto entry=registry.makeEntry("TEXT"); entry.context["bindingId"]=binding; entry.context["sliceId"]=999;
            if(!session.isEmpty()) { entry.context["sessionId"]=session; } return entry;
        };
        for(int binding:{MeterBinding::TxPower,MeterBinding::TxMic,MeterBinding::HwVolts}) {
            auto foreign=entryFor(binding,"other"); MeterWidget live;
            auto* item=registry.createMeterItem(foreign,&live,ContentRenderMode::Preview); live.addItem(item);
            poller.setTargetContext(&live,foreign.context);
            QCOMPARE(item->value(),-400.0); QVERIFY(!item->bindingUnavailableReason(binding).isEmpty());
            MeterWidget matchingLive, inheritedLive;
            auto matching=entryFor(binding,"this"), inherited=entryFor(binding,{});
            auto* matchingItem=registry.createMeterItem(matching,&matchingLive,ContentRenderMode::Preview);
            auto* inheritedItem=registry.createMeterItem(inherited,&inheritedLive,ContentRenderMode::Preview);
            matchingLive.addItem(matchingItem); inheritedLive.addItem(inheritedItem);
            poller.setTargetContext(&matchingLive,matching.context); poller.setTargetContext(&inheritedLive,inherited.context);
            ContainerDocument d; d.id="A"; d.layout=ContentLayout::VerticalStack; d.contents={foreign,entryFor(binding,{}),entryFor(binding,"this")};
            ContainerPreviewWidget preview(registry,poller); preview.setDocument(d);
            const auto meters=preview.findChildren<MeterWidget*>(); QCOMPARE(meters.size(),3);
            QCOMPARE(meters[0]->items()[0]->value(),-400.0);
            const double expected=binding==MeterBinding::TxPower?50.0:binding==MeterBinding::TxMic?7.0:48.0;
            QCOMPARE(meters[1]->items()[0]->value(),expected); QCOMPARE(meters[2]->items()[0]->value(),expected);
            QCOMPARE(matchingItem->value(),expected); QCOMPARE(inheritedItem->value(),expected);
            model.radioStatus().setPowerReadings(60,2,1.5); poller.handOutTxReadingForTest(MeterBinding::TxMic,9);
            QVERIFY(QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection)); poller.handOutTxReadingForTest(MeterBinding::TxMic,9);
            QCOMPARE(item->value(),-400.0); QCOMPARE(meters[0]->items()[0]->value(),-400.0);
            QVERIFY(!meters[0]->items()[0]->bindingUnavailableReason(binding).isEmpty());
            poller.frameAdvanced(900);
            const double refreshed=binding==MeterBinding::TxPower?60.0:binding==MeterBinding::TxMic?9.0:48.0;
            QCOMPARE(meters[1]->items()[0]->value(),refreshed); QCOMPARE(meters[2]->items()[0]->value(),refreshed);
            QCOMPARE(matchingItem->value(),refreshed); QCOMPARE(inheritedItem->value(),refreshed);
            model.radioStatus().setPowerReadings(50,2,1.5); poller.handOutTxReadingForTest(MeterBinding::TxMic,7);
        }
        session="next";
        ContainerDocument switched; switched.id="new-session"; switched.layout=ContentLayout::VerticalStack;
        auto clock=registry.makeEntry("meter.clock"); clock.context["sessionId"]="other";
        switched.contents={entryFor(MeterBinding::TxPower,"next"),entryFor(MeterBinding::HwVolts,"next"),clock};
        ContainerPreviewWidget switchedPreview(registry,poller); switchedPreview.setDocument(switched);
        const auto switchedMeters=switchedPreview.findChildren<MeterWidget*>();
        QCOMPARE(switchedMeters[0]->items()[0]->value(),-400.0); // No preceding-session seed before its first producer frame.
        QCOMPARE(switchedMeters[1]->items()[0]->value(),-400.0);
        auto* clockFace=qobject_cast<CompositePresetItem*>(switchedMeters[2]->items()[0]); QVERIFY(clockFace && clockFace->clockDisplay());
        model.radioStatus().setPowerReadings(80,3,1.8); poller.frameAdvanced(1000);
        QCOMPARE(switchedMeters[0]->items()[0]->value(),80.0);
        model.applyCorePaReadings({24.0,24.0,2.0,30.0});
        QVERIFY(QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection));
        QCOMPARE(switchedMeters[1]->items()[0]->value(),24.0);
        QVERIFY(clockFace->clockDisplay()->advanceMeter(2000));
        QVERIFY(clockFace->clockDisplay()->bindingUnavailableReason(MeterBinding::TxPower).isEmpty());
    }
    void actualMainPreviewUsesSanctionedSliceCacheAndNeverActions() {
        AppSettings::setProfileOverride(QStringLiteral("container-preview-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear(); QVERIFY(AppSettings::instance().save());
        RadioDiscovery discovery; discovery.holdOffScans(std::chrono::minutes(5));
        const auto cleanup=qScopeGuard([]{ QFile::remove(AppSettings::instance().filePath()); RadioDiscovery::clearHoldOffForTest(); });
        MainWindow window({},nullptr,MainWindow::ConnectionStartup::Deferred); window.resize(1100,780); window.show();
        auto* model=window.radioModel(); model->setBoardForTest(HPSDRHW::Saturn); model->configureStreamPool(5,5,192000); model->setConnectionStateForTest(ConnectionState::Connected);
        while(model->slices().size()<2) { QVERIFY(model->addSlice()>=0); }
        auto* b=model->sliceById(1); QVERIFY(b); b->setFrequency(7074100); b->setDspMode(DSPMode::LSB); b->setSignalPeakDbm(-67);
        auto* manager=window.findChild<ContainerManager*>(); auto* poller=window.findChild<MeterPoller*>(); QVERIFY(manager && poller);
        const int targets=poller->targetCountForTest();
        ContainerSettingsDialog dialog(manager->container(manager->workspaceStore()->snapshot().mainContainerId),nullptr,manager);
        auto draft=dialog.editSession()->draft(); auto& c=draft.containers[0]; c.layout=ContentLayout::VerticalStack; c.config["sliceId"]=1;
        auto face=manager->contentRegistry()->makeEntry("meter.contest"); c.contents={face}; dialog.editSession()->setDraft(draft);
        // Selection reload is explicit: another container selection is unnecessary.
        dialog.refreshDraftView(); auto* preview=dialog.findChild<ContainerPreviewWidget*>(); QVERIFY(preview);
        auto* meter=preview->findChild<MeterWidget*>(); QVERIFY(meter); auto* composite=qobject_cast<CompositePresetItem*>(meter->items()[0]); QVERIFY(composite);
        QCOMPARE(composite->vfoDisplay()->frequency(),int64_t(7074100)); QCOMPARE(poller->targetCountForTest(),targets);
        QSignalSpy frequency(b,&SliceModel::frequencyChanged),mode(b,&SliceModel::dspModeChanged);
        composite->vfoDisplay()->frequencyChangeRequested(100); composite->modeButtons()->modeClicked(0); QCOMPARE(frequency.count(),0); QCOMPARE(mode.count(),0);
        dialog.resize(1200,850); dialog.show();
#ifdef NEREUS_GPU_SPECTRUM
        QSignalSpy frames(meter,&QRhiWidget::frameSubmitted); meter->update(); QTRY_VERIFY_WITH_TIMEOUT(frames.count()>0,3000); QVERIFY(!meter->grabFramebuffer().isNull());
#endif
        const auto capture=qEnvironmentVariable("TASK8_CAPTURE_DIR"); if(!capture.isEmpty()) { QVERIFY(QDir().mkpath(capture)); QVERIFY(dialog.grab().save(capture+"/main-slice-b-preview.png")); }
        model->setConnectionStateForTest(ConnectionState::Disconnected); poller->frameAdvanced(500); QVERIFY(!composite->vfoDisplay()->unavailableText().isEmpty());
        QCOMPARE(frequency.count(),0); QCOMPARE(mode.count(),0); QCOMPARE(poller->targetCountForTest(),targets);
        dialog.reject();
    }
    void failedDialogApplyKeepsDraftAndNativeOwnership() {
        QTemporaryDir dir; QVERIFY(QDir(dir.path()).mkdir("sub")); AppSettings settings(dir.filePath("sub/settings")); ContainerWorkspaceStore store(settings);
        ContainerContentRegistry registry; MeterPoller poller; QWidget root; QSplitter splitter(&root); ContainerManager manager(&root,&splitter); manager.setWorkspaceAdapter(&store,&registry); manager.setPreviewPoller(&poller);
        WorkspaceDocument d; d.mainContainerId="A"; ContainerDocument a; a.id="A"; a.name="A"; a.layout=ContentLayout::VerticalStack; a.contents={registry.makeEntry("meter.clock")}; d.containers={a};
        QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved); const auto original=store.snapshot(); const auto raw=settings.value("ContainerWorkspace");
        QPointer<MeterWidget> live=manager.contentHost("A")->meterSurfaces()[0]; QWidget* parent=live->parentWidget(); QSignalSpy saved(&store,&ContainerWorkspaceStore::committed);
        ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager); dialog.findChild<QLineEdit*>()->setText("unsaved rename");
        QVERIFY(QDir(dir.path()).rename("sub","old")); QFile blocker(dir.filePath("sub")); QVERIFY(blocker.open(QIODevice::WriteOnly)); blocker.close();
        QCOMPARE(dialog.applyDraft().status,CommitStatus::StorageError); QCOMPARE(saved.count(),0); QCOMPARE(store.snapshot(),original); QCOMPARE(settings.value("ContainerWorkspace"),raw);
        QVERIFY(live); QCOMPARE(live->parentWidget(),parent); QCOMPARE(dialog.editSession()->draft().containers[0].name,QString("unsaved rename"));
        dialog.reject();
    }
    void productionDialogSwitchCancelCloseAndApply() {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings")); ContainerWorkspaceStore store(settings); ContainerContentRegistry registry; MeterPoller poller;
        QWidget root; QSplitter splitter(&root); ContainerManager manager(&root,&splitter); manager.setWorkspaceAdapter(&store,&registry); manager.setPreviewPoller(&poller);
        WorkspaceDocument d; d.mainContainerId="A"; ContainerDocument a,b; a.id="A"; b.id="B"; a.name="A"; b.name="B"; a.layout=b.layout=ContentLayout::VerticalStack; a.contents={registry.makeEntry("BAR")}; b.contents={registry.makeEntry("meter.clock")}; d.containers={a,b}; QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
        // Preserve opaque/applet row positions and avoid staging default-only edits.
        auto mixed=store.snapshot(); auto opaque=registry.makeEntry("DISCORDBTNS"); opaque.config["legacyRecord"]="exact opaque";
        mixed.containers[0].contents.append(opaque); mixed.containers[0].contents.append(registry.makeEntry("meter.customBar"));
        QCOMPARE(manager.commitWorkspace(mixed,mixed.revision).status,CommitStatus::Saved);
        const auto original=store.snapshot(); const auto raw=settings.value("ContainerWorkspace"); auto* live=manager.contentHost("A")->meterSurfaces()[0]; QPointer<MeterWidget> livePointer=live; QSignalSpy committed(&store,&ContainerWorkspaceStore::committed); QSignalSpy reconciled(&manager,&ContainerManager::workspaceReconciled);
        {
            ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager); auto* title=dialog.findChild<QLineEdit*>(); QVERIFY(title);
            dialog.findChild<QListWidget*>("containerDraftContents")->setCurrentRow(0); auto* editor=dialog.findChild<BaseItemEditor*>(); QVERIFY(editor);
            const int binding=editor->item()->bindingId(); editor->item()->setBindingId(MeterBinding::SignalPeak); editor->propertyChanged();
            QCOMPARE(committed.count(),0); QCOMPARE(store.snapshot(),original);
            QCOMPARE(dialog.editSession()->draft().containers[0].contents[0].context.value("bindingId").toInt(),MeterBinding::SignalPeak);
            editor->item()->setBindingId(binding); editor->propertyChanged(); QCOMPARE(dialog.editSession()->draft().containers[0].contents[0],original.containers[0].contents[0]);
            title->setText("draft A");
            dialog.selectDraftContainer("B"); QCOMPARE(dialog.editSession()->draft().containers[0].contents,original.containers[0].contents); title->setText("draft B"); dialog.selectDraftContainer("A"); QCOMPARE(title->text(),QString("draft A")); QCOMPARE(committed.count(),0); QCOMPARE(settings.value("ContainerWorkspace"),raw); QCOMPARE(store.snapshot(),original); QVERIFY(livePointer);
            for(auto* box:dialog.findChildren<QCheckBox*>()) { if(box->text().contains("Highlight")) { box->setChecked(true); } }
            QCOMPARE(store.snapshot(),original);
            auto* available=dialog.findChild<QListWidget*>("containerAvailableContents"); QVERIFY(available); QListWidgetItem* add=nullptr;
            for(int i=0;i<available->count();++i) { if(available->item(i)->data(Qt::UserRole).toString()=="BAR") { add=available->item(i); break; } }
            QVERIFY(add); available->setCurrentItem(add); available->itemDoubleClicked(add);
            const auto count=dialog.editSession()->draft().containers[0].contents.size(); title->setText("second draft update");
            QCOMPARE(dialog.editSession()->draft().containers[0].contents.size(),count); QCOMPARE(count,original.containers[0].contents.size()+1);
            QCOMPARE(store.snapshot(),original);
            QPushButton* duplicate=nullptr;
            for(auto* button:dialog.findChildren<QPushButton*>()) { if(button->text()=="Duplicate") { duplicate=button; break; } }
            QVERIFY(duplicate); duplicate->click(); QCOMPARE(dialog.editSession()->draft().containers.size(),original.containers.size()+1);
            QCOMPARE(manager.containerCount(),original.containers.size()); QCOMPARE(committed.count(),0); QCOMPARE(store.snapshot(),original);
            dialog.reject();
        }
        QCOMPARE(store.snapshot(),original); QCOMPARE(settings.value("ContainerWorkspace"),raw); QVERIFY(livePointer);
        {
            ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager); dialog.findChild<QLineEdit*>()->setText("close draft"); dialog.show(); dialog.close();
        }
        QCOMPARE(committed.count(),0); QCOMPARE(store.snapshot(),original); QVERIFY(livePointer);
        {
            ContainerSettingsDialog dialog(manager.container("B"),nullptr,&manager); QPushButton* remove=nullptr;
            for(auto* button:dialog.findChildren<QPushButton*>()) { if(button->text()=="Delete") { remove=button; break; } }
            QVERIFY(remove); remove->click(); QCOMPARE(dialog.editSession()->draft().containers.size(),1); QCOMPARE(manager.containerCount(),2);
            QVERIFY(manager.container("B")); QCOMPARE(store.snapshot(),original); QCOMPARE(committed.count(),0); dialog.reject();
        }
        {
            ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager); dialog.findChild<QLineEdit*>()->setText("saved A"); dialog.selectDraftContainer("B"); dialog.findChild<QLineEdit*>()->setText("saved B");
            QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved); QCOMPARE(committed.count(),1); QCOMPARE(reconciled.count(),3); // two row projections plus one completed workspace projection
            QCOMPARE(store.snapshot().containers[0].name,QString("saved A")); QCOMPARE(store.snapshot().containers[1].name,QString("saved B"));
            QCOMPARE(store.snapshot().containers[0].contents[1],original.containers[0].contents[1]);
            dialog.resize(1200,850); dialog.show(); QCoreApplication::processEvents(); const QString capture=qEnvironmentVariable("TASK8_CAPTURE_DIR"); if(!capture.isEmpty()) { QVERIFY(QDir().mkpath(capture)); QVERIFY(dialog.grab().save(capture+"/settings-preview.png")); }
        }
    }
};
QTEST_MAIN(TstContainerPreview)
#include "tst_container_preview.moc"
