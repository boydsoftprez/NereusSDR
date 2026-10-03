// no-port-check: NereusSDR-original native palette, reconstruction and workload evidence.
// Modification history (NereusSDR):
//   2026-10-02 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QDir>
#include <QPainter>
#include "gui/meters/presets/AnanMultiMeterItem.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QVBoxLayout>
#include <QTemporaryDir>
#include <QSplitter>
#include <QPushButton>
#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "gui/applets/TxApplet.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerArrangeController.h"
#include <ctime>
#include <algorithm>
#include <memory>
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/presets/BarPresetItem.h"
using namespace NereusSDR;
class TstContainerMeterNative : public QObject {
    Q_OBJECT
    QJsonArray m_captures;
    QImage frame(MeterWidget& meter) {
#ifdef NEREUS_GPU_SPECTRUM
        QSignalSpy submitted(&meter,&QRhiWidget::frameSubmitted);
        meter.update();
        if(!submitted.wait(3000) && submitted.isEmpty()) { return {}; }
        return meter.grabFramebuffer();
#else
        meter.update();QCoreApplication::processEvents();return meter.grab().toImage();
#endif
    }
    void save(const QImage& image, MeterWidget& meter, const QString& name) {
        const QString dir=qEnvironmentVariable("TASK10_CAPTURE_DIR");
        if(!dir.isEmpty()) {QVERIFY(QDir().mkpath(dir));QVERIFY(image.save(dir+"/"+name+".png"));}
        QJsonObject capture{{"name",name},{"platform",QGuiApplication::platformName()},{"dpr",meter.devicePixelRatioF()},{"width",meter.width()},{"height",meter.height()},{"pixelWidth",image.width()},{"pixelHeight",image.height()},{"nativeWindow",bool(meter.windowHandle())}};
#ifdef NEREUS_GPU_SPECTRUM
        capture["backend"]=int(meter.api());
#endif
        m_captures.append(capture);
    }
    static void seed(MeterItem& item,int step) {
        for(int binding:item.readingBindings()) {
            const double value=binding<10 ? -110+step*4 : binding==200 ? 13.8 : binding==201 ? 6 : binding==102 ? 1+step*.1 : binding==100 ? step*8 : binding==101 ? step*.8 : -25+step*2;
            item.pushBindingValue(binding,value);
        }
    }
private slots:
    void initTestCase() {AppSettings::setProfileOverride(QStringLiteral("task10-native-%1").arg(QCoreApplication::applicationPid()));AppSettings::instance().clear();}
    void nativeFaces_data() {
        QTest::addColumn<QString>("type");QTest::addColumn<int>("width");QTest::addColumn<bool>("light");
        for(const QString& type:{"meter.mic","meter.powerSwr","meter.crossNeedle","meter.ananMulti","meter.magicEye","meter.signalText","meter.historyGraph","meter.vfoDisplay","meter.clock","meter.contest"}) {
            for(int width:{260,434,640}) {for(bool light:{false,true}) {QTest::newRow(qPrintable(type+"-"+QString::number(width)+(light?"-light":"-dark")))<<type<<width<<light;}}
        }
    }
    void nativeFaces() {
        QFETCH(QString,type);QFETCH(int,width);QFETCH(bool,light);
        ContainerContentRegistry registry;QWidget host;host.setAttribute(Qt::WA_ShowWithoutActivating);host.setStyleSheet(light?"background:#e4e6ea;":"background:#121826;");
        auto* layout=new QVBoxLayout(&host);layout->setContentsMargins(12,12,12,12);
        auto meter=std::make_unique<MeterWidget>(); // Native surface choice occurs parentless.
        const ContentEntry entry=registry.makeEntry(type);auto* item=registry.createMeterItem(entry,meter.get());QVERIFY(item);
        auto* face=qobject_cast<CompositePresetItem*>(item);auto* bar=qobject_cast<BarPresetItem*>(item);
        if(face) {auto channels=face->configuration()["channels"].toArray();for(int i=0;i<channels.size();++i){auto channel=channels[i].toObject();channel["ignoreHistoryMs"]=0;channels[i]=channel;}QVERIFY(face->applyConfiguration({{"channels",channels}}));face->setFrequency(14200123);face->setModeLabel("USB");face->setBandLabel("20m");}
        if(bar) {QVERIFY(bar->applyConfiguration({{"ignoreHistoryMs",0}}));}
        const int height=face?face->preferredFaceHeight():bar->preferredRowHeight();
        if(face) {width=std::max(width,face->minimumFaceSize().width());}
        meter->setFixedSize(width,height);meter->addItem(item);layout->addWidget(meter.get());host.show();
        if(type=="meter.ananMulti") {meter->resetForTxTransition(true);}
        QImage first;
        for(int step=0;step<12;++step) {seed(*item,step);meter->advanceMeters(step*100);if(step==0){first=frame(*meter);QVERIFY(!first.isNull());}}
        const QImage peak=frame(*meter);QVERIFY(!peak.isNull());
        if(!item->readingBindings().isEmpty()) {QVERIFY(peak!=first);}
        save(peak,*meter,type.mid(6)+"-"+QString::number(width)+(light?"-light":"-dark"));
        const QString stable=item->serialize();
        seed(*item,12);meter->advanceMeters(1200);const QImage before=frame(*meter);
        if(face && face->editableFields().contains("backdropColor")) {QVERIFY(face->applyConfiguration({{"backdropColor","#ff254b35"}}));meter->advanceMeters(1300);const QImage changed=frame(*meter);QVERIFY(changed!=before);}
        // Rebuild the leaf in a different native owner from lossless item data.
        layout->removeWidget(meter.get());meter.reset();
        QWidget replacementHost;replacementHost.setAttribute(Qt::WA_ShowWithoutActivating);QVBoxLayout replacementLayout(&replacementHost);
        auto replacement=std::make_unique<MeterWidget>();auto restored=entry;restored.config["legacyRecord"]=stable;restored.config.remove("properties");
        auto* copy=registry.createMeterItem(restored,replacement.get());QVERIFY(copy);replacement->addItem(copy);replacement->setFixedSize(width,height);replacementLayout.addWidget(replacement.get());replacementHost.show();
        if(type=="meter.ananMulti") {replacement->resetForTxTransition(true);}
        for(int step=0;step<12;++step){seed(*copy,step);replacement->advanceMeters(step*100);}
        QVERIFY(!frame(*replacement).isNull());
        // Surfaces die before native hosts and QObject-owned items.
        replacementLayout.removeWidget(replacement.get());replacement.reset();
    }
    void ananGroupSelectorLivesInOneLayer() {
        AnanMultiMeterItem face;face.resetForTxTransition(true);
        QImage overlay(640,300,QImage::Format_ARGB32_Premultiplied);overlay.fill(Qt::transparent);
        {QPainter painter(&overlay);face.paintForLayer(painter,640,300,MeterItem::Layer::OverlayDynamic);}
        int alpha=0;for(int y=0;y<24;++y){for(int x=540;x<640;++x){alpha+=qAlpha(overlay.pixel(x,y));}}
        QCOMPARE(alpha,0); // Static selector must not be duplicated in the dynamic layer.
    }
    void realTxAppletMovesNeverRequestTransmit() {
        QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);
        RadioModel model(RadioModel::Role::Remote);QWidget original;TxApplet tx(&model,&original);QPointer<TxApplet> identity=&tx;
        int mox=0,tune=0,twoTone=0;tx.setDesktopKeyHandlers([&](bool){++mox;},[&](bool){++tune;},[]{return false;},[]{return false;});tx.setDesktopTwoToneHandler([&](bool){++twoTone;});
        QSignalSpy power(&model.transmitModel(),&TransmitModel::powerChanged),gain(&model.transmitModel(),&TransmitModel::micGainChanged),filter(&model.transmitModel(),&TransmitModel::filterChanged),moxState(&model.transmitModel(),&TransmitModel::moxChanged),tuneState(&model.transmitModel(),&TransmitModel::tuneChanged);
        ContainerContentRegistry registry;registry.attachSingleton("applet:TX",&tx);QWidget root;root.setAttribute(Qt::WA_ShowWithoutActivating);root.resize(950,900);QSplitter splitter(&root);splitter.resize(930,880);
        ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);
        WorkspaceDocument d;d.mainContainerId="main";ContainerDocument main,other;main.id="main";other.id="other";main.name="Transmit and microphone";main.autoHeight=true;main.layout=other.layout=ContentLayout::VerticalStack;
        auto mic=registry.makeEntry("meter.mic"),view=registry.makeEntry("applet:TX");main.contents={mic,view};d.containers={main,other};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);root.show();
        auto* content=manager.contentHost("main");QVERIFY(content);auto* meter=content->meterSurfaces()[0];meter->resetForTxTransition(true);meter->updateMeterValue(MeterBinding::TxMicPeak,-4);meter->updateMeterValue(MeterBinding::TxMic,-8);meter->advanceMeters(0);QVERIFY(!frame(*meter).isNull());
        auto* grip=content->findChild<QWidget*>("entryGrip_"+view.id);QVERIFY(grip);QTRY_VERIFY_WITH_TIMEOUT(grip->isVisible(),1000);QCoreApplication::processEvents();QVERIFY(!content->gripGeometry(view.id).isEmpty());
        QVERIFY(!content->gripGeometry(view.id).intersects(QRect(tx.mapTo(content,QPoint(0,0)),tx.size())));
        content->setFixedHeight(content->preferredContentHeight());QCoreApplication::processEvents();
        const QString directory=qEnvironmentVariable("TASK10_CAPTURE_DIR");if(!directory.isEmpty()){QVERIFY(QDir().mkpath(directory));QVERIFY(content->grab().save(directory+"/tx-mixed-container.png"));}
        auto* arrange=manager.arrangeController();QVERIFY(arrange);
        for(int repeat=0;repeat<3;++repeat) {QVERIFY(arrange->move(view.id,"other",0).ok);QVERIFY(arrange->move(view.id,"main",1).ok);QVERIFY(arrange->popOut(view.id).ok);const QString shell=store.snapshot().containers.last().id;QVERIFY(arrange->closeContainer(shell).ok);QCOMPARE(registry.singletonView("applet:TX"),identity.data());}
        manager.floatContainer("main");manager.panelDockContainer("main");
        QCOMPARE(mox,0);QCOMPARE(tune,0);QCOMPARE(twoTone,0);QCOMPARE(power.count(),0);QCOMPARE(gain.count(),0);QCOMPARE(filter.count(),0);QCOMPARE(moxState.count(),0);QCOMPARE(tuneState.count(),0);
        QVERIFY(!model.transmitModel().isMox());QVERIFY(!model.transmitModel().isTune());QVERIFY(identity);
    }
    void mixedPaletteWorkload() {
        QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);
        RadioModel model(RadioModel::Role::Remote);QWidget owner;TxApplet tx(&model,&owner);QPointer<TxApplet> identity=&tx;
        ContainerContentRegistry registry;registry.attachSingleton("applet:TX",&tx);MeterPoller poller;
        QWidget host;host.setAttribute(Qt::WA_ShowWithoutActivating);host.resize(600,1100);QSplitter splitter(&host);splitter.resize(580,1080);
        ContainerManager manager(&host,&splitter);manager.setWorkspaceAdapter(&store,&registry);
        int lookups=0;double sample=-110;
        poller.setRxReadingSource([&](const QJsonObject&,int){++lookups;return sample;});
        connect(&manager,&ContainerManager::meterContextReady,&poller,[&](MeterWidget* meter,const QJsonObject& context){poller.setTargetContext(meter,context);poller.addTarget(meter);});
        WorkspaceDocument d;ContainerDocument container;container.id="mixed";container.layout=ContentLayout::VerticalStack;d.mainContainerId=container.id;
        auto signal=registry.makeEntry("meter.signalText"),history=registry.makeEntry("meter.historyGraph"),mic=registry.makeEntry("meter.mic"),power=registry.makeEntry("meter.powerSwr");
        power.context["sliceId"]=1;container.contents={signal,history,registry.makeEntry("applet:TX"),mic,power};d.containers={container};
        QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
        auto* content=manager.contentHost("mixed");QVERIFY(content);auto surfaces=content->meterSurfaces();
        // Two contiguous RX meters share a surface; singleton and source split separate the others.
        QCOMPARE(surfaces.size(),3);QCOMPARE(poller.targetCountForTest(),3);host.show();poller.setIntervalMs(100);poller.start();
        QSignalSpy advances(&poller,&MeterPoller::frameAdvanced);
        QJsonArray submitted;
#ifdef NEREUS_GPU_SPECTRUM
        QVector<QMetaObject::Connection> observers;
        QElapsedTimer clock;clock.start();
        for(int i=0;i<surfaces.size();++i){observers.append(connect(surfaces[i],&QRhiWidget::frameSubmitted,this,[&,i]{submitted.append(QJsonObject{{"surface",i},{"ns",double(clock.nsecsElapsed())}});}));}
#endif
        bool warmupOk=false;int warmup=qEnvironmentVariableIntValue("TASK10_MIXED_WARMUP",&warmupOk);if(!warmupOk){warmup=2;}
        QTRY_VERIFY_WITH_TIMEOUT(advances.count()>=warmup,5000);
        bool ok=false;int measured=qEnvironmentVariableIntValue("TASK10_MIXED_SAMPLES",&ok);if(!ok){measured=3;}
        const int initial=advances.count(),initialLookups=lookups;quint64 invalidationsBefore=0;for(auto* meter:surfaces){invalidationsBefore+=meter->readingInvalidationsForTest();}submitted={};
#ifdef NEREUS_GPU_SPECTRUM
        clock.restart();
#endif
        QElapsedTimer interval;interval.start();const std::clock_t cpuStart=std::clock();
        while(advances.count()<initial+measured) {sample= -110 + ((advances.count()-initial)%12)*4;QTest::qWait(1);}
        const double cpuMs=1000.0*(std::clock()-cpuStart)/CLOCKS_PER_SEC;const double wallMs=interval.nsecsElapsed()/1e6;
        poller.stop();quint64 invalidationsAfter=0;for(auto* meter:surfaces){invalidationsAfter+=meter->readingInvalidationsForTest();}
        const QJsonObject result{{"kind","new-mixed-container-host-only"},{"samples",measured},{"warmupFrames",initial},{"readingInvalidations",double(invalidationsAfter-invalidationsBefore)},{"surfaceCount",surfaces.size()},{"targetCount",poller.targetCountForTest()},{"entryCount",5},{"frameAdvancedCount",advances.count()-initial},{"rxProviderCalls",lookups-initialLookups},{"processCpuMs",cpuMs},{"eventLoopWallMs",wallMs},{"frameSubmitted",submitted},{"dpr",host.devicePixelRatioF()},{"platform",QGuiApplication::platformName()},{"limits","aggregate driver/poll/render/observer CPU; submission is not GPU execution; main has no palette comparator"}};
        const QString directory=qEnvironmentVariable("TASK10_CAPTURE_DIR");if(!directory.isEmpty()){QVERIFY(QDir().mkpath(directory));QFile file(directory+"/mixed-workload.json");QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(result).toJson());}
        content->releaseViews();QCOMPARE(poller.targetCountForTest(),0);QVERIFY(identity);registry.returnBorrowedView(&tx);
    }
    void cleanupTestCase() {
        QFile::remove(AppSettings::instance().filePath());
        const QString directory=qEnvironmentVariable("TASK10_CAPTURE_DIR");if(!directory.isEmpty()){QFile file(directory+"/native-captures.json");QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(m_captures).toJson());}
    }
};
QTEST_MAIN(TstContainerMeterNative)
#include "tst_container_meter_native.moc"
