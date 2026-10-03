// no-port-check: NereusSDR-original mixed host integration invariants.
// Modification history (NereusSDR):
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QSplitter>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QAction>
#include <QLabel>
#include "gui/meters/MeterItem.h"
#include "core/AppSettings.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerSourceAdapter.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/SMeterWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
class TstContainerContentHost : public QObject {
    Q_OBJECT
private slots:
    void orderedRunsKeepContextGeometryAndHiddenBoundaries()
    {
        ContainerContentRegistry registry;
        ContainerContentHost host(registry);
        ContainerDocument d; d.id = "main"; d.layout = ContentLayout::VerticalStack;
        auto a = registry.makeEntry("meter.mic"); a.id = "a"; a.canvasRect = QRectF(.13,.24,.61,.18); a.context["sliceId"] = 2;
        auto b = registry.makeEntry("meter.alc"); b.id = "b"; b.context["sliceId"] = 2;
        auto c = registry.makeEntry("meter.mic"); c.id = "c"; c.context["sliceId"] = 7;
        auto hidden = registry.makeEntry("meter.alc"); hidden.id = "hidden"; hidden.visible = false;
        auto late = registry.makeEntry("applet:rx"); late.id = "late";
        d.contents = {a,b,c,hidden,late};
        host.reconcile(d);
        QCOMPARE(host.meterSurfaces().size(), 2);
        QCOMPARE(host.meterSurfaces()[0]->items().size(), 2);
        QCOMPARE(host.sourceContext(host.meterSurfaces()[1]).value("sliceId").toInt(), 7);
        QCOMPARE(host.entryRows().size(), 5);
        QCOMPARE(host.entryRows()[3].entryId, QString("hidden"));
        QVERIFY(!host.entryRows()[3].effectiveVisible);
        QCOMPARE(host.captureDocument().contents[0].canvasRect, a.canvasRect);
        const auto* original = host.meterSurfaces()[0];
        host.reconcile(d); QCOMPARE(host.meterSurfaces()[0], original);
        auto* widget = new QWidget(&host); widget->setObjectName("one live model view");
        registry.attachSingleton("applet:rx", widget); host.reconcile(d);
        QCOMPARE(host.entryRows()[4].widget.data(), widget);
        registry.setAvailable("applet:rx", false); host.reconcile(d);
        QCOMPARE(host.entryRows()[4].widget.data(), widget);
        QVERIFY(widget->isHidden()); QVERIFY(d.contents[4].visible);
        registry.setAvailable("applet:rx", true); host.reconcile(d);
        QCOMPARE(registry.singletonView("applet:rx"), widget);
    }
    void committedMoveKeepsSingletonAndReplacesTargetsSynchronously()
    {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        QWidget dock; QSplitter splitter; ContainerManager manager(&dock, &splitter);
        MeterPoller poller;
        manager.setWorkspaceAdapter(&store, &registry);
        connect(&manager, &ContainerManager::meterContextReady, &poller, &MeterPoller::setTargetContext);
        WorkspaceDocument document; document.mainContainerId = "main";
        ContainerDocument main; main.id = "main"; main.layout = ContentLayout::VerticalStack;
        auto sm = registry.makeEntry("applet:s_meter"); auto bar = registry.makeEntry("meter.mic");
        main.contents = {sm,bar};
        ContainerDocument second; second.id = "second"; second.layout = ContentLayout::VerticalStack; second.dockMode = DockMode::Floating;
        document.containers = {main,second};
        QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);
        auto* meter = new SMeterWidget(); registry.attachSingleton("applet:s_meter", meter); poller.setSMeter(meter);
        QPointer<SMeterWidget> stable(meter);
        QPointer<MeterWidget> old(manager.contentHost("main")->meterSurfaces().first());
        QCOMPARE(poller.targetCountForTest(), 1);
        document = store.snapshot(); document.containers[0].contents.removeAt(0); document.containers[1].contents.append(sm);
        QCOMPARE(manager.commitWorkspace(document, document.revision).status, CommitStatus::Saved);
        QVERIFY(stable); QCOMPARE(registry.singletonView("applet:s_meter"), stable.data());
        QVERIFY(manager.contentHost("second")->isAncestorOf(stable));
        QVERIFY(old.isNull()); QCOMPARE(poller.targetCountForTest(), 1);
        manager.reconcileWorkspace(store.snapshot()); QCOMPARE(poller.targetCountForTest(), 1);
        manager.floatContainer("main"); QCOMPARE(poller.targetCountForTest(), 1);
        manager.panelDockContainer("main"); QCOMPARE(poller.targetCountForTest(), 1);
        document = store.snapshot(); document.containers[0].contents.append(sm); document.containers[0].contents.last().id = "duplicate";
        QCOMPARE(manager.commitWorkspace(document, document.revision).status, CommitStatus::Invalid);
        QVERIFY(stable);
    }
    void structuredVisibilityWinsLegacyAndFailedSaveIsInert()
    {
        QTemporaryDir dir; const QString path = dir.filePath("settings.xml"); AppSettings settings(path);
        ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        WorkspaceDocument d; d.mainContainerId = "main"; ContainerDocument c; c.id = "main";
        auto a = registry.makeEntry("applet:rx"); a.visible = false; c.contents = {a}; d.containers = {c};
        QCOMPARE(store.commit(d,0).status, CommitStatus::Saved);
        auto& global = AppSettings::instance();
        const bool hadLegacy = global.contains("AppletRxVisible"); const QVariant legacy = global.value("AppletRxVisible");
        const auto restoreLegacy = qScopeGuard([&] { if (hadLegacy) { global.setValue("AppletRxVisible",legacy); } else { global.remove("AppletRxVisible"); } });
        global.setValue("AppletRxVisible", "True");
        AppletVisibilityController visibility; visibility.setWorkspaceAdapter(&store, &registry);
        visibility.registerApplet("Rx","RX",true); QVERIFY(!visibility.isVisible("Rx"));
        visibility.setAvailable("Rx",false); visibility.setVisible("Rx",true);
        QVERIFY(visibility.isVisible("Rx")); QVERIFY(!visibility.isEffectivelyVisible("Rx"));
        QCOMPARE(store.snapshot().containers[0].contents[0].visible, true);
        visibility.setAvailable("Rx",true);
        // A future/corrupt workspace remains read-only, with original bytes intact.
        settings.setValue("ContainerWorkspace","{\"schemaVersion\":99}"); QVERIFY(!store.load().ok);
        const auto before = store.snapshot(); visibility.setVisible("Rx",false);
        QVERIFY(visibility.isVisible("Rx")); QCOMPARE(store.snapshot(),before);
        QCOMPARE(settings.value("ContainerWorkspace").toString(),QString("{\"schemaVersion\":99}"));
        QVERIFY(!visibility.storageError().isEmpty());
    }
    void saveFailureKeepsCommittedUiAndBytes()
    {
        QTemporaryDir dir; QVERIFY(QDir(dir.path()).mkdir("profile"));
        const QString profile = dir.filePath("profile"); const QString path = profile + "/settings.xml";
        AppSettings settings(path); ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        QWidget owner, dock; QSplitter splitter; ContainerManager manager(&dock,&splitter);
        manager.setWorkspaceAdapter(&store,&registry);
        auto* view = new QWidget(&owner); registry.attachSingleton("applet:rx",view);
        WorkspaceDocument d; d.mainContainerId="main"; ContainerDocument c; c.id="main";
        auto entry=registry.makeEntry("applet:rx"); c.contents={entry}; d.containers={c};
        QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
        AppletVisibilityController controller; controller.setWorkspaceAdapter(&store,&registry); controller.registerApplet("Rx","RX",true);
        QAction menu; menu.setCheckable(true); menu.setChecked(controller.isVisible("Rx"));
        connect(&menu,&QAction::toggled,&controller,[&](bool v){controller.setVisible("Rx",v);});
        connect(&controller,&AppletVisibilityController::visibilityChanged,&menu,[&](const QString&,bool v){menu.setChecked(v);});
        const auto before=store.snapshot(); const QVariant raw=settings.value("ContainerWorkspace");
        const auto parent=view->parentWidget(); const bool hidden=view->isHidden();
        QFile original(path); QVERIFY(original.open(QIODevice::ReadOnly)); const QByteArray bytes=original.readAll(); original.close();
        QVERIFY(QDir().rename(profile,dir.filePath("saved-profile")));
        QFile obstruction(profile); QVERIFY(obstruction.open(QIODevice::WriteOnly)); obstruction.write("obstruction"); obstruction.close();
        auto draft=before; draft.containers[0].contents[0].visible=false;
        QCOMPARE(store.commit(draft,before.revision).status,CommitStatus::StorageError);
        QSignalSpy failures(&controller,&AppletVisibilityController::persistenceFailed);
        menu.setChecked(false);
        QCOMPARE(failures.count(),1); QVERIFY(controller.isVisible("Rx")); QVERIFY(menu.isChecked());
        QCOMPARE(view->parentWidget(),parent); QCOMPARE(view->isHidden(),hidden);
        QCOMPARE(store.snapshot(),before); QCOMPARE(settings.value("ContainerWorkspace"),raw);
        QFile preserved(dir.filePath("saved-profile/settings.xml")); QVERIFY(preserved.open(QIODevice::ReadOnly)); QCOMPARE(preserved.readAll(),bytes);
    }
    void selectiveReconcilePreservesHistoryAndDestroyedSingletonPlaceholder()
    {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        QWidget owner,dock; QSplitter splitter; ContainerManager manager(&dock,&splitter); MeterPoller poller;
        manager.setWorkspaceAdapter(&store,&registry);
        connect(&manager,&ContainerManager::meterContextReady,&poller,&MeterPoller::setTargetContext);
        WorkspaceDocument d; d.mainContainerId="main";
        ContainerDocument a; a.id="main"; a.layout=ContentLayout::VerticalStack;
        auto face=registry.makeEntry("meter.mic"); auto properties=face.config["properties"].toObject(); properties["ignoreHistoryMs"]=0; face.config["properties"]=properties; auto applet=registry.makeEntry("applet:rx"); a.contents={face,applet};
        ContainerDocument b; b.id="other"; b.layout=ContentLayout::VerticalStack; b.contents={registry.makeEntry("meter.alc")}; d.containers={a,b};
        QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
        QPointer<MeterWidget> first=manager.contentHost("main")->meterSurfaces().first(), other=manager.contentHost("other")->meterSurfaces().first();
        auto* item=qobject_cast<BarPresetItem*>(first->items().first()); QVERIFY(item);
        item->setValue(-25); first->advanceMeters(0); item->setValue(-10); first->advanceMeters(100); const double history=item->minimumHistory();
        QVERIFY(item->hasPrimaryReading()); QVERIFY(history>-30); QVERIFY(history<item->peakValue());
        const auto contents=store.snapshot().containers[0].contents;
        d=store.snapshot(); d.containers[0].geometry=QRect(12,18,400,350); d.containers[0].name="new name"; d.containers[1].visible=false;
        QCOMPARE(manager.commitWorkspace(d,d.revision).status,CommitStatus::Saved);
        QCOMPARE(manager.contentHost("main")->meterSurfaces().first(),first.data()); QCOMPARE(manager.contentHost("other")->meterSurfaces().first(),other.data());
        QCOMPARE(item->minimumHistory(),history); QCOMPARE(poller.targetCountForTest(),2);
        manager.saveState(); QCOMPARE(store.snapshot().containers[0].contents,contents); QCOMPARE(manager.contentHost("main")->meterSurfaces().first(),first.data());
        auto* view=new QWidget(&owner); registry.attachSingleton("applet:rx",view);
        QCOMPARE(manager.contentHost("main")->entryRows()[1].widget.data(),view);
        d=store.snapshot(); d.containers[0].contents[1].visible=false;
        QCOMPARE(manager.commitWorkspace(d,d.revision).status,CommitStatus::Saved);
        QCOMPARE(manager.contentHost("main")->meterSurfaces().first(),first.data());
        d=store.snapshot(); d.containers[0].contents[1].visible=true;
        QCOMPARE(manager.commitWorkspace(d,d.revision).status,CommitStatus::Saved);
        registry.setAvailable("applet:rx",false); registry.setAvailable("applet:rx",true);
        QCOMPARE(manager.contentHost("main")->meterSurfaces().first(),first.data()); QCOMPARE(item->minimumHistory(),history);
        delete view; QVERIFY(qobject_cast<QLabel*>(manager.contentHost("main")->entryRows()[1].widget.data()));
        auto* replacement=new QWidget(&owner); registry.attachSingleton("applet:rx",replacement);
        QCOMPARE(manager.contentHost("main")->entryRows()[1].widget.data(),replacement);
        registry.attachSingleton("applet:rx",nullptr); QCOMPARE(replacement->parentWidget(),&owner);
        auto* finalView=new QWidget(&owner); registry.attachSingleton("applet:rx",finalView); delete replacement;
        QCOMPARE(registry.singletonView("applet:rx"),finalView); QCOMPARE(manager.contentHost("main")->entryRows()[1].widget.data(),finalView);
        QCOMPARE(first->items().first(),item); QCOMPARE(item->minimumHistory(),history); QCOMPARE(poller.targetCountForTest(),2);
    }
    void retainedDuplicateUsesFirstPlacementPreference()
    {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings.xml")); ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        WorkspaceDocument d; d.mainContainerId="main"; ContainerDocument a,b; a.id="main"; b.id="duplicate-container";
        auto first=registry.makeEntry("applet:TX"), duplicate=first; duplicate.id="retained-duplicate"; duplicate.visible=false; duplicate.extensions["opaque"]="retain";
        a.contents={first}; b.contents={duplicate}; d.containers={a,b}; QCOMPARE(store.commit(d,0).status,CommitStatus::Saved);
        AppletVisibilityController controller; controller.setWorkspaceAdapter(&store,&registry); controller.registerApplet("Tx","TX",false);
        QVERIFY(controller.isVisible("Tx")); controller.setVisible("Tx",false); QVERIFY(!controller.isVisible("Tx"));
        QCOMPARE(store.snapshot().containers[1].contents[0],duplicate);
        controller.setVisible("Tx",true); QVERIFY(store.snapshot().containers[0].contents[0].visible); QCOMPARE(store.snapshot().containers[1].contents[0],duplicate);
    }
    void minimumWidthAndLegacyMixedSourcePreservation()
    {
        ContainerContentRegistry registry; ContainerContentHost host(registry); ContainerDocument d; d.id="main"; d.layout=ContentLayout::VerticalStack;
        d.contents={registry.makeEntry("meter.ananMulti"),registry.makeEntry("meter.mic")}; host.reconcile(d);
        QCOMPARE(host.meterSurfaces().size(),1); QVERIFY(host.meterSurfaces().first()->minimumWidth()>=360);
        d.layout=ContentLayout::LegacyCanvas; d.contents[0].context["sliceId"]=7; d.contents[1].context["sliceId"]=23;
        d.contents[0].canvasRect=QRectF(.2,.15,.4,.3); d.contents[1].canvasRect=QRectF(.1,.62,.7,.11); host.reconcile(d);
        QCOMPARE(host.meterSurfaces().size(),1); QVERIFY(!host.entryRows()[0].item->property("containerUnsupportedSource").toBool());
        QVERIFY(host.entryRows()[1].item->property("containerUnsupportedSource").toBool());
        QCOMPARE(host.captureDocument().contents[0].canvasRect,d.contents[0].canvasRect); QCOMPARE(host.captureDocument().contents[1].canvasRect,d.contents[1].canvasRect);
        host.meterSurfaces().first()->updateMeterValue(MeterBinding::TxMic,-20); QVERIFY(!qobject_cast<BarPresetItem*>(host.entryRows()[1].item.data())->hasPrimaryReading());
    }
    void malformedKnownEntryCreatesOnlyExplanationAndValidRun()
    {
        ContainerContentRegistry registry; ContainerContentHost host(registry); MeterPoller poller;
        int ready=0; connect(&host,&ContainerContentHost::meterSurfaceReady,this,[&](MeterWidget*,const QJsonObject&){++ready;});
        connect(&host,&ContainerContentHost::meterSurfaceReady,&poller,&MeterPoller::setTargetContext);
        ContainerDocument d; d.id="main"; d.layout=ContentLayout::VerticalStack;
        auto broken=registry.makeEntry("meter.mic"); broken.config["legacyRecord"]="retained future or damaged record"; broken.context["sliceId"]=2;
        auto valid=registry.makeEntry("meter.alc"); valid.context["sliceId"]=7; d.contents={broken,valid};
        host.reconcile(d); QCOMPARE(host.meterSurfaces().size(),1); QCOMPARE(ready,1); QCOMPARE(poller.targetCountForTest(),1);
        QCOMPARE(host.entryRows().size(),2); QVERIFY(qobject_cast<QLabel*>(host.entryRows()[0].widget.data())); QVERIFY(!host.entryRows()[0].item);
        QVERIFY(host.entryRows()[1].item); QCOMPARE(host.sourceContext(host.meterSurfaces().first()).value("sliceId").toInt(),7);
        QCOMPARE(host.captureDocument().contents[0],broken);
    }
    void borrowedStackViewReturnsToConstructionOwner()
    {
        QWidget owner; QWidget stackView(&owner); QPointer<QWidget> stable=&stackView;
        {
            ContainerContentRegistry registry;
            {
                ContainerContentHost host(registry); registry.attachSingleton("applet:rx",&stackView);
                ContainerDocument d; d.id="main"; d.layout=ContentLayout::VerticalStack; d.contents={registry.makeEntry("applet:rx")};
                host.reconcile(d); QVERIFY(host.isAncestorOf(&stackView));
            }
            QVERIFY(stable); QCOMPARE(registry.singletonView("applet:rx"),&stackView);
        }
        QVERIFY(stable); QCOMPARE(stackView.parentWidget(),&owner);
    }
    void detachedHeaderKeepsSmeterLookupAndPointer()
    {
        AppletPanelWidget panel; QPointer<SMeterWidget> sm(panel.smeterWidget());
        panel.clearHeaderWidget(); QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QVERIFY(sm); QCOMPARE(panel.smeterWidget(),sm.data());
        ContainerContentRegistry registry; registry.attachSingleton("applet:s_meter",sm);
        ContainerContentHost host(registry); ContainerDocument d; d.id="main"; d.layout=ContentLayout::VerticalStack; d.contents={registry.makeEntry("applet:s_meter")};
        host.reconcile(d); QCOMPARE(panel.smeterWidget(),sm.data());
        host.releaseViews(); QVERIFY(sm); QCOMPARE(panel.smeterWidget(),sm.data());
    }
    void stableSourceNeverFallsBackAndReadsOnlyCache()
    {
        RadioModel model(RadioModel::Role::Remote); model.setStationConnectionState(ConnectionState::Connected);
        QVERIFY(model.addSliceWithStationId(7)>=0); QVERIFY(model.addSliceWithStationId(23)>=0);
        auto* first=model.sliceById(7); auto* second=model.sliceById(23); first->setStreamIndex(0); second->setStreamIndex(1);
        first->setSignalPeakDbm(-65); second->setSignalPeakDbm(-43);
        const QJsonObject explicitSource{{"sliceId",23}};
        QCOMPARE(ContainerSourceAdapter::slice(&model,explicitSource,first),second);
        QCOMPARE(ContainerSourceAdapter::reading(&model,explicitSource,first,MeterBinding::SignalPeak,true,true,{}),-43.0);
        QCOMPARE(ContainerSourceAdapter::reading(&model,{{"sliceId",99}},first,MeterBinding::SignalPeak,true,true,{}),kNoMeterReadingDbm);
        QCOMPARE(ContainerSourceAdapter::reading(&model,{},first,MeterBinding::SignalPeak,false,true,{}),kNoMeterReadingDbm);
        QCOMPARE(ContainerSourceAdapter::reading(&model,explicitSource,first,MeterBinding::PbSnr,true,true,{}),kNoMeterReadingDbm);
        QCOMPARE(ContainerSourceAdapter::reading(&model,explicitSource,first,MeterBinding::AdcPeak,true,false,{}),kNoMeterReadingDbm);
        const auto maxBin=[](const SliceModel* source){return source->sliceIndex()==23?-41.0:-71.0;};
        QCOMPARE(ContainerSourceAdapter::reading(&model,explicitSource,first,MeterBinding::SignalMaxBin,true,true,maxBin),-41.0);
        QCOMPARE(ContainerSourceAdapter::slice(&model,{{"sessionId","station"},{"sliceId",23}},first,"station"),second);
        QVERIFY(!ContainerSourceAdapter::slice(&model,{{"sessionId","elsewhere"},{"sliceId",23}},first,"station"));
        MeterPoller poller; QSignalSpy frames(&poller,&MeterPoller::frameAdvanced); poller.start();
        QTRY_VERIFY_WITH_TIMEOUT(frames.count()>0,500); poller.stop();
    }
};
QTEST_MAIN(TstContainerContentHost)
#include "tst_container_content_host.moc"
