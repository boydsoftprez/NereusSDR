// no-port-check: NereusSDR-original native mixed-host and Main wiring evidence.
// Modification history (NereusSDR):
//   2026-10-03 — Individual control creation, scope and lifecycle regression by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QScopeGuard>
#include <QDir>
#include <QLoggingCategory>
#include <QWindow>
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "gui/MainWindow.h"
#include "gui/SMeterWidget.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/containers/ContainerArrangeController.h"
#include "models/TransmitModel.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
class TstContainerContentHostNative : public QObject {
    Q_OBJECT
    void capture(MeterWidget* meter, const QString& name)
    {
        QVERIFY(meter && meter->isVisible());
#ifdef NEREUS_GPU_SPECTRUM
        QSignalSpy frames(meter,&QRhiWidget::frameSubmitted);
        meter->update(); QTRY_VERIFY_WITH_TIMEOUT(frames.count()>0,3000);
        const QImage image=meter->grabFramebuffer();
#else
        meter->update(); QCoreApplication::processEvents(); const QImage image=meter->grab().toImage();
#endif
        QVERIFY(!image.isNull());
        const QString directory=qEnvironmentVariable("TASK6_CAPTURE_DIR");
        if (!directory.isEmpty()) { QVERIFY(QDir().mkpath(directory)); QVERIFY(image.save(directory+"/"+name+".png")); }
    }
private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules("nereus.*.debug=false");
        AppSettings::setProfileOverride(QStringLiteral("container-native-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear(); QVERIFY(AppSettings::instance().save());
        RadioDiscovery discovery; discovery.holdOffScans(std::chrono::minutes(5));
    }
    void cleanupTestCase() { QFile::remove(AppSettings::instance().filePath()); RadioDiscovery::clearHoldOffForTest(); }
    void independentControlsNativeStateScopePlacementAndLifecycle()
    {
        auto window=std::make_unique<MainWindow>(RemoteStationOptions{},nullptr,MainWindow::ConnectionStartup::Deferred);
        window->resize(1100,760);window->show();
        auto* manager=window->findChild<ContainerManager*>();QVERIFY(manager);
        auto* store=manager->workspaceStore();auto* registry=manager->contentRegistry();
        auto document=store->snapshot();const QString id=document.mainContainerId;
        document.containers.clear();ContainerDocument main;main.id=id;main.layout=ContentLayout::FreeCanvas;main.dockMode=DockMode::Floating;main.geometry=QRect(160,160,620,460);
        auto mox=registry->makeEntry("control.mox"),monitor=registry->makeEntry("control.monitor"),mute=registry->makeEntry("control.mute"),legacy=registry->makeEntry("OTHERBTNS");
        mox.setFreeCanvasRect(QRectF(20,20,112,44));monitor.setFreeCanvasRect(QRectF(152,20,112,44));mute.setFreeCanvasRect(QRectF(284,20,112,44));legacy.setFreeCanvasRect(QRectF(20,110,420,260));
        mute.context["sliceId"]=0;main.contents={mox,monitor,mute,legacy};document.containers={main};
        QCOMPARE(manager->commitWorkspace(document,document.revision).status,CommitStatus::Saved);
        auto* host=manager->contentHost(id);auto* model=window->radioModel();
        auto* mon=qobject_cast<OtherButtonItem*>(host->entryRows()[1].item.data());QVERIFY(mon);
        auto* monMeter=qobject_cast<MeterWidget*>(host->entryRows()[1].widget.data());QVERIFY(monMeter);QCOMPARE(monMeter->size(),QSize(112,44));
        const bool before=model->transmitModel().monEnabled();QSignalSpy changed(&model->transmitModel(),&TransmitModel::monEnabledChanged);
        QTest::mouseClick(monMeter,Qt::LeftButton,Qt::NoModifier,QPoint(55,20));
        QCOMPARE(changed.count(),1);QCOMPARE(model->transmitModel().monEnabled(),!before);QVERIFY(mon->buttonState(OtherButtonItem::ButtonId::Mon));
        const auto buttonPixel=[&] {
#ifdef NEREUS_GPU_SPECTRUM
            const QImage frame=monMeter->grabFramebuffer();
#else
            const QImage frame=monMeter->grab().toImage();
#endif
            return frame.pixelColor(QPoint(10,20)*monMeter->devicePixelRatioF());
        };
        QTRY_COMPARE_WITH_TIMEOUT(buttonPixel(),mon->button(mon->indexOf(OtherButtonItem::ButtonId::Mon)).onColour,500);
        capture(monMeter,"single-monitor-on");capture(qobject_cast<MeterWidget*>(host->entryRows()[0].widget.data()),"single-mox-unavailable");capture(qobject_cast<MeterWidget*>(host->entryRows()[3].widget.data()),"legacy-other-buttons");
        auto* moxItem=qobject_cast<OtherButtonItem*>(host->entryRows()[0].item.data());QVERIFY(moxItem);QVERIFY(!moxItem->isButtonAvailable(OtherButtonItem::ButtonId::Mox));QSignalSpy command(moxItem,&OtherButtonItem::otherButtonClicked);
        QTest::mouseClick(qobject_cast<MeterWidget*>(host->entryRows()[0].widget.data()),Qt::LeftButton,Qt::NoModifier,QPoint(55,20));QCOMPARE(command.count(),0);QVERIFY(!model->isTune());
        // Model-only offline slices give the real GUI dispatcher two distinguishable targets.
        model->setBoardForTest(HPSDRHW::Saturn);model->configureStreamPool(5,5,192000);model->setConnectionStateForTest(ConnectionState::Connected);while(model->slices().size()<2) {QVERIFY(model->addSlice()>=0);}
        auto* a=model->sliceById(0);auto* b=model->sliceById(1);QVERIFY(a && b);QSignalSpy aMute(a,&SliceModel::mutedChanged),bMute(b,&SliceModel::mutedChanged);
        auto* muteMeter=qobject_cast<MeterWidget*>(host->entryRows()[2].widget.data());QTest::mouseClick(muteMeter,Qt::LeftButton,Qt::NoModifier,QPoint(55,20));QCOMPARE(aMute.count(),1);QCOMPARE(bMute.count(),0);
        document=store->snapshot();document.containers[0].contents[2].context["sessionId"]="foreign-session";document.containers[0].contents[1].context["sessionId"]="foreign-session";document.containers[0].contents[3].context["sessionId"]="foreign-session";document.containers[0].contents[0].context["sessionId"]="foreign-session";QPointer<MeterItem> old=host->entryRows()[2].item;
        QCOMPARE(manager->commitWorkspace(document,document.revision).status,CommitStatus::Saved);QVERIFY(old.isNull());host=manager->contentHost(id);auto* foreign=qobject_cast<OtherButtonItem*>(host->entryRows()[2].item.data());QVERIFY(foreign);QVERIFY(!foreign->isButtonAvailable(OtherButtonItem::ButtonId::Mute));
        QTest::mouseClick(qobject_cast<MeterWidget*>(host->entryRows()[2].widget.data()),Qt::LeftButton,Qt::NoModifier,QPoint(55,20));QCOMPARE(aMute.count(),1);QCOMPARE(bMute.count(),0);
        auto* foreignMon=qobject_cast<OtherButtonItem*>(host->entryRows()[1].item.data());QVERIFY(foreignMon);QVERIFY(!foreignMon->isButtonAvailable(OtherButtonItem::ButtonId::Mon));
        QTest::mouseClick(qobject_cast<MeterWidget*>(host->entryRows()[1].widget.data()),Qt::LeftButton,Qt::NoModifier,QPoint(55,20));QCOMPARE(changed.count(),1);
        auto* foreignGroup=qobject_cast<OtherButtonItem*>(host->entryRows()[3].item.data());QVERIFY(foreignGroup);QVERIFY(!foreignGroup->isButtonAvailable(OtherButtonItem::ButtonId::Mon));QVERIFY(!foreignGroup->buttonUnavailableReason(foreignGroup->indexOf(OtherButtonItem::ButtonId::Mon)).isEmpty());
        foreignGroup->otherButtonClicked(int(OtherButtonItem::ButtonId::Mon));foreignMon->otherButtonClicked(int(OtherButtonItem::ButtonId::Mon));QCOMPARE(changed.count(),1);
        int txRequests=0;ContainerButtonDispatcher::Hooks fakeHooks;fakeHooks.desktopHosting=[]{return true;};fakeHooks.desktopMoxOn=[]{return false;};fakeHooks.desktopTuneOn=[]{return false;};
        const auto fakeRequest=[&](bool){++txRequests;};fakeHooks.requestDesktopMox=fakeRequest;fakeHooks.requestDesktopTune=fakeRequest;fakeHooks.requestDesktopTwoTone=fakeRequest;ContainerButtonDispatcher fakeDispatcher(model,std::move(fakeHooks));
        auto* foreignTx=qobject_cast<OtherButtonItem*>(host->entryRows()[0].item.data());QVERIFY(foreignTx);
        connect(foreignTx,&OtherButtonItem::otherButtonClicked,this,[&](int action){fakeDispatcher.click(static_cast<OtherButtonItem::ButtonId>(action),1);});
        QTest::mouseClick(qobject_cast<MeterWidget*>(host->entryRows()[0].widget.data()),Qt::LeftButton,Qt::NoModifier,QPoint(55,20));QCOMPARE(txRequests,0);
        connect(foreignGroup,&OtherButtonItem::otherButtonClicked,this,[&](int action){fakeDispatcher.click(static_cast<OtherButtonItem::ButtonId>(action),1);});
        auto* foreignGroupMeter=qobject_cast<MeterWidget*>(host->entryRows()[3].widget.data());QSignalSpy txRefused(foreignGroup,&ButtonBoxItem::unavailableButtonClicked);
        for(const int x:{105,175,245}) {QTest::mouseClick(foreignGroupMeter,Qt::LeftButton,Qt::NoModifier,QPoint(x,30));}QCOMPARE(txRequests,0);QCOMPARE(txRefused.count(),3);
        RadioInfo currentRadio;currentRadio.macAddress="02:00:00:00:00:01";model->setLastRadioInfoForTest(currentRadio);QVERIFY(!model->currentRadioMac().isEmpty());
        document=store->snapshot();document.containers[0].contents[3].context["sessionId"]=model->currentRadioMac();document.containers[0].contents[3].context["sliceId"]=99;document.containers[0].contents[1].context.remove("sessionId");QCOMPARE(manager->commitWorkspace(document,document.revision).status,CommitStatus::Saved);
        host=manager->contentHost(id);auto* validGroup=qobject_cast<OtherButtonItem*>(host->entryRows()[3].item.data());QVERIFY(validGroup);QVERIFY(validGroup->isButtonAvailable(OtherButtonItem::ButtonId::Mon));validGroup->otherButtonClicked(int(OtherButtonItem::ButtonId::Mon));QCOMPARE(changed.count(),2);
        // Unsupported and malformed scope refuse even direct forwarded signals.
        auto* validMon=qobject_cast<OtherButtonItem*>(host->entryRows()[1].item.data());validMon->setProperty("containerUnsupportedSource",true);validMon->setProperty("unsupportedSourceReason","Unsupported synthetic source");QSignalSpy unsupported(manager->container(id),&ContainerWidget::unavailableButtonClicked);validMon->otherButtonClicked(int(OtherButtonItem::ButtonId::Mon));QCOMPARE(changed.count(),2);QCOMPARE(unsupported.count(),1);manager->container(id)->rxSourceChanged(manager->container(id)->rxSource());QVERIFY(!validMon->isButtonAvailable(OtherButtonItem::ButtonId::Mon));
        validMon->setProperty("containerUnsupportedSource",false);validMon->setProperty("containerSourceContext",QJsonObject{{"sessionId",17}});validMon->otherButtonClicked(int(OtherButtonItem::ButtonId::Mon));QCOMPARE(changed.count(),2);manager->container(id)->rxSourceChanged(manager->container(id)->rxSource());QVERIFY(!validMon->isButtonAvailable(OtherButtonItem::ButtonId::Mon));
        model->setConnectionStateForTest(ConnectionState::Disconnected);
        ContainerArrangeController arrange(*store,manager);const auto original=store->snapshot().containers[0].contents[1];
        QVERIFY(arrange.popOut(monitor.id).ok);QCOMPARE(store->snapshot().containers.size(),2);QVERIFY(arrange.returnEntry(monitor.id).ok);
        document=store->snapshot();const auto returned=document.containers[0].contents[1];QCOMPARE(returned.id,original.id);QCOMPARE(returned.config,original.config);QCOMPARE(returned.context,original.context);QCOMPARE(returned.freeCanvasRect(),original.freeCanvasRect());
        QVERIFY(arrange.placeFreeCanvas(monitor.id,QRectF(410,20,144,48),*returned.freeCanvasRect()).ok);document=store->snapshot();document.containers[0].locked=true;QCOMPARE(manager->commitWorkspace(document,document.revision).status,CommitStatus::Saved);
        QVERIFY(!arrange.placeFreeCanvas(monitor.id,QRectF(450,20,144,48),QRectF(410,20,144,48)).ok);QCOMPARE(store->snapshot().containers[0].contents[1].freeCanvasRect(),std::optional<QRectF>(QRectF(410,20,144,48)));
        const QString captures=qEnvironmentVariable("TASK6_CAPTURE_DIR");if(!captures.isEmpty()) {QVERIFY(manager->container(id)->window()->grab().save(captures+"/independent-controls-workspace.png"));}
        ContainerWorkspaceStore reloaded(AppSettings::instance());auto loaded=reloaded.load();QVERIFY(loaded.ok);QCOMPARE(loaded.document,store->snapshot());
    }
    void productionSourcesOfflineCadenceNativeMovesAndShutdown()
    {
        auto* window=new MainWindow({},nullptr,MainWindow::ConnectionStartup::Deferred);
        const auto cleanup=qScopeGuard([&]{delete window;});
        window->resize(1200,850); window->show();
        auto* manager=window->findChild<ContainerManager*>(); auto* poller=window->findChild<MeterPoller*>(); auto* panel=window->findChild<AppletPanelWidget*>();
        QVERIFY(manager && poller && panel); auto* registry=manager->contentRegistry(); auto* store=manager->workspaceStore();
        QPointer<SMeterWidget> singleton=panel->smeterWidget(); QVERIFY(singleton); QCOMPARE(poller->smeterForTest(),singleton.data());
        auto d=store->snapshot(); const QString id=d.mainContainerId;
        d.containers.clear(); ContainerDocument c; c.id=id; c.layout=ContentLayout::VerticalStack;
        auto sm=registry->makeEntry("applet:s_meter"), clock=registry->makeEntry("meter.clock"), signal=registry->makeEntry("meter.sMeter"), contest=registry->makeEntry("meter.contest"), missing=registry->makeEntry("meter.vfoDisplay");
        signal.context["sliceId"]=0; contest.context["sliceId"]=1; missing.context["sliceId"]=99;
        c.contents={sm,clock,signal,contest,missing}; d.containers={c};
        qInfo()<<"TASK6 phase: initial materialization";
        QCOMPARE(manager->commitWorkspace(d,d.revision).status,CommitStatus::Saved);
        auto* host=manager->contentHost(id); QCOMPARE(host->meterSurfaces().size(),4); QCOMPARE(poller->targetCountForTest(),4);
        for(auto* meter:host->meterSurfaces()) { qInfo()<<"TASK6 native leaf"<<meter<<meter->windowHandle()<<"surface"<<(meter->windowHandle()?int(meter->windowHandle()->surfaceType()):-1)<<"top"<<meter->window()->windowHandle(); }
        qInfo()<<"TASK6 phase: offline frames";
        auto* clockMeter=host->meterSurfaces()[0];
        QSignalSpy cadence(poller,&MeterPoller::frameAdvanced);
#ifdef NEREUS_GPU_SPECTRUM
        QSignalSpy clockFrames(clockMeter,&QRhiWidget::frameSubmitted);
        QTRY_VERIFY_WITH_TIMEOUT(clockFrames.count()>2,2000); // driven by Main's shared poller offline
#endif
        QTRY_VERIFY_WITH_TIMEOUT(cadence.count()>2,1000);
        auto* signalFace=qobject_cast<BarPresetItem*>(host->entryRows()[2].item.data()); QVERIFY(signalFace); QVERIFY(!signalFace->hasPrimaryReading());
        capture(clockMeter,"offline-clock");
        // Native Free Canvas must preserve independent contexts and the existing
        // borrowed S-meter, including the real GUI source adapter below.
        d=store->snapshot();d.containers[0].layout=ContentLayout::FreeCanvas;
        const QRectF placements[]{QRectF(20,20,480,130),QRectF(20,170,420,180),QRectF(20,370,480,72),QRectF(20,460,560,250),QRectF(20,740,560,250)};
        for(int i=0;i<d.containers[0].contents.size();++i) {d.containers[0].contents[i].setFreeCanvasRect(placements[i]);}
        QCOMPARE(manager->commitWorkspace(d,d.revision).status,CommitStatus::Saved);
        host=manager->contentHost(id);QCOMPARE(host->meterSurfaces().size(),4);QCOMPARE(poller->targetCountForTest(),4);
        QCOMPARE(registry->singletonView("applet:s_meter"),singleton.data());
        clockMeter=host->meterSurfaces()[0];capture(clockMeter,"free-canvas-clock");
        auto* model=window->radioModel(); model->setBoardForTest(HPSDRHW::Saturn); model->configureStreamPool(5,5,192000);
        model->setConnectionStateForTest(ConnectionState::Connected); while(model->slices().size()<2) { QVERIFY(model->addSlice()>=0); }
        auto* a=model->sliceById(0); auto* b=model->sliceById(1); QVERIFY(a && b);
        a->setFrequency(14'074'000); b->setFrequency(7'074'000); b->setDspMode(DSPMode::USB);
        auto* face=qobject_cast<CompositePresetItem*>(host->entryRows()[3].item.data()); auto* absent=qobject_cast<CompositePresetItem*>(host->entryRows()[4].item.data()); QVERIFY(face && absent);
        QTRY_COMPARE(face->vfoDisplay()->frequency(),int64_t(b->frequency())); QVERIFY(!absent->vfoDisplay()->unavailableText().isEmpty());
        const double aBefore=a->frequency(), bBefore=b->frequency(); face->vfoDisplay()->frequencyChangeRequested(100);
        QCOMPARE(a->frequency(),aBefore); QCOMPARE(b->frequency(),bBefore+100);
        face->modeButtons()->modeClicked(0); QCOMPARE(b->dspMode(),DSPMode::LSB); QCOMPARE(a->dspMode(),DSPMode::USB);
        absent->vfoDisplay()->frequencyChangeRequested(100); QCOMPARE(a->frequency(),aBefore); QCOMPARE(b->frequency(),bBefore+100);
        capture(host->meterSurfaces()[2],"slice-b-contest");
        QSignalSpy aTuning(a,&SliceModel::frequencyChanged), bTuning(b,&SliceModel::frequencyChanged), aMode(a,&SliceModel::dspModeChanged), bMode(b,&SliceModel::dspModeChanged);
        for (int move=0;move<3;++move) {
            qInfo()<<"TASK6 phase: move"<<move;
            QVector<QPointer<MeterWidget>> old; for(auto* meter:host->meterSurfaces()) { old.append(meter); }
            if(move==0) { manager->floatContainer(id); }
            else if(move==1) { manager->panelDockContainer(id); }
            else { manager->overlayDockContainer(id); }
            for(const auto& previous:old) { QVERIFY(previous.isNull()); }
            host=manager->contentHost(id); QCOMPARE(host->meterSurfaces().size(),4); QCOMPARE(poller->targetCountForTest(),4);
            QVERIFY(singleton); QCOMPARE(registry->singletonView("applet:s_meter"),singleton.data()); QCOMPARE(panel->smeterWidget(),singleton.data()); QCOMPARE(poller->smeterForTest(),singleton.data());
            capture(host->meterSurfaces()[0],QStringLiteral("move-%1-clock").arg(move));
        }
        QCOMPARE(aTuning.count(),0); QCOMPARE(bTuning.count(),0); QCOMPARE(aMode.count(),0); QCOMPARE(bMode.count(),0);
        // Unsupported mixed-source legacy canvas preserves geometry without presenting or acting on the first source.
        d=store->snapshot(); d.containers[0].layout=ContentLayout::LegacyCanvas; d.containers[0].contents={signal,contest};
        d.containers[0].contents[0].canvasRect=QRectF(0,0,1,.22); d.containers[0].contents[1].canvasRect=QRectF(0,.25,1,.75);
        QCOMPARE(manager->commitWorkspace(d,d.revision).status,CommitStatus::Saved);
        host=manager->contentHost(id); face=qobject_cast<CompositePresetItem*>(host->entryRows()[1].item.data()); QVERIFY(face);
        QVERIFY(!face->vfoDisplay()->unavailableText().isEmpty()); const double finalA=a->frequency(),finalB=b->frequency();
        face->vfoDisplay()->frequencyChangeRequested(100); face->modeButtons()->modeClicked(1); QCOMPARE(a->frequency(),finalA); QCOMPARE(b->frequency(),finalB); QCOMPARE(b->dspMode(),DSPMode::LSB);
        model->setConnectionStateForTest(ConnectionState::Disconnected); const int count=cadence.count(); QTRY_VERIFY_WITH_TIMEOUT(cadence.count()>count+2,1000);
        // Final shutdown destroys borrowed applets once, and every synchronous target disappears.
        // The non-owning GUI adapters must also clear when the model outlives its window.
        model->setParent(nullptr); std::unique_ptr<RadioModel> survivingModel(model);
        QPointer<MeterPoller> deadPoller=poller; delete window; window=nullptr; QVERIFY(singleton.isNull()); QVERIFY(deadPoller.isNull());
        QVERIFY(!model->meterPoller()); QVERIFY(!model->containerManager());
    }
    void legacyFloatingAdoptsOnceAndFutureStoreRemainsReadOnly()
    {
        auto& settings=AppSettings::instance(); settings.clear();
        QWidget geometry; geometry.setGeometry(70,80,420,300);
        const QString oldGeometry=QString::fromLatin1(geometry.saveGeometry().toHex());
        ContainerWidget legacy; legacy.setId("legacy-float"); legacy.setDockMode(DockMode::Floating);
        settings.setValue("ContainerIdList","legacy-float"); settings.setValue("ContainerData_legacy-float",legacy.serialize());
        settings.setValue("MeterDisplay_legacy-float_Geometry","70,80,410,310");
        settings.setValue("AppletTXFloating","True"); settings.setValue("AppletTXFloatGeometry",oldGeometry); settings.setValue("AppletTxVisible","True"); QVERIFY(settings.save());
        QString shellId; QString entryId;
        {
            MainWindow window({},nullptr,MainWindow::ConnectionStartup::Deferred); window.resize(1200,850); window.show();
            auto* manager=window.findChild<ContainerManager*>(); QVERIFY(manager); auto d=manager->workspaceStore()->snapshot();
            int placements=0;
            for(const auto& c:d.containers) { for(const auto& e:c.contents) { if(e.typeId=="applet:TX") {
                ++placements; shellId=c.id; entryId=e.id; QCOMPARE(c.dockMode,DockMode::Floating); QVERIFY(c.popOutShell); QVERIFY(e.returnLocation); QCOMPARE(c.config["legacyAppletFloatGeometry"].toString(),oldGeometry);
            } } }
            QCOMPARE(placements,1); QVERIFY(manager->contentRegistry()->singletonView("applet:TX"));
            bool retainedGeometry=false; for(const auto& c:d.containers) { if(c.id=="legacy-float") { QCOMPARE(c.geometry,QRect(70,80,410,310)); retainedGeometry=true; } } QVERIFY(retainedGeometry);
            QCoreApplication::processEvents(); manager->saveState();
            auto* vis=window.findChild<AppletVisibilityController*>(); QVERIFY(vis); vis->setVisible("Tx",false);
            QCOMPARE(settings.value("AppletTXFloating").toString(),QString("True")); QCOMPARE(settings.value("AppletTXFloatGeometry").toString(),oldGeometry);
        }
        {
            MainWindow window({},nullptr,MainWindow::ConnectionStartup::Deferred);
            auto* manager=window.findChild<ContainerManager*>(); const auto d=manager->workspaceStore()->snapshot(); int placements=0;
            for(const auto& c:d.containers) { for(const auto& e:c.contents) { if(e.typeId=="applet:TX") { ++placements; QCOMPARE(c.id,shellId); QCOMPARE(e.id,entryId); QVERIFY(!e.visible); } } }
            QCOMPARE(placements,1); QVERIFY(!window.findChild<AppletVisibilityController*>()->isVisible("Tx"));
        }
        settings.setValue("ContainerWorkspace","{\"schemaVersion\":99}"); QVERIFY(settings.save()); const auto raw=settings.value("ContainerWorkspace");
        {
            MainWindow window({},nullptr,MainWindow::ConnectionStartup::Deferred); auto* manager=window.findChild<ContainerManager*>();
            QVERIFY(manager); QVERIFY(!manager->workspaceStore()->loadError().isEmpty()); QCOMPARE(manager->containerCount(),0); manager->saveState(); QCOMPARE(settings.value("ContainerWorkspace"),raw);
        }
        QCOMPARE(settings.value("ContainerWorkspace"),raw);
    }

};
QTEST_MAIN(TstContainerContentHostNative)
#include "tst_container_content_host_native.moc"
