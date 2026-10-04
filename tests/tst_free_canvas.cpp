// no-port-check: NereusSDR-original free Canvas native interaction and transaction tests.
#include <QtTest>
#include <QTemporaryDir>
#include <QSplitter>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QComboBox>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QJsonArray>
#include <QMouseEvent>
#include "core/AppSettings.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/containers/FreeCanvasSurface.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/presets/BarPresetItem.h"
using namespace NereusSDR;
namespace {
void gesture(QWidget* handle,QPoint delta,bool cancel=false) {
    const QPoint start=handle->rect().center(),global=handle->mapToGlobal(start);
    QTest::mousePress(handle,Qt::LeftButton,Qt::NoModifier,start);
    QMouseEvent move(QEvent::MouseMove,start+delta,global+delta,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
    QApplication::sendEvent(handle,&move);
    if(cancel) {QTest::keyClick(handle,Qt::Key_Escape);}else {QTest::mouseRelease(handle,Qt::LeftButton,Qt::NoModifier,start+delta);}
}
ContentEntry signal(ContainerContentRegistry& registry,int source) {
    auto entry=registry.makeEntry("meter.signal");entry.context["sliceId"]=source;
    auto properties=entry.config["properties"].toObject();properties["attack"]=1;properties["decay"]=1;properties["ignoreHistoryMs"]=0;
    entry.config["properties"]=properties;return entry;
}
}
class TstFreeCanvas final : public QObject {
    Q_OBJECT
private slots:
    void importedSmallAndZeroNativeMetersStayExact() {
        ContainerContentRegistry registry;MeterPoller poller;ContainerDocument c;c.id="small";c.layout=ContentLayout::FreeCanvas;
        for(const QSizeF size:{QSizeF(40,30),QSizeF(0,0),QSizeF(460.123456789,100.987654321)}) {auto e=signal(registry,0);e.setFreeCanvasRect(QRectF(QPointF(-2.123456789,30),size));c.contents.append(e);}
        ContainerContentHost host(registry);host.reconcile(c);ContainerPreviewWidget preview(registry,poller);preview.setDocument(c);host.show();preview.show();QCoreApplication::processEvents();
        for(const auto& e:c.contents) {
            auto* live=host.findChild<FreeCanvasSurface*>();auto* draft=preview.findChild<FreeCanvasSurface*>();QVERIFY(live && draft);
            const QSize size(qRound(e.freeCanvasRect()->width()),qRound(e.freeCanvasRect()->height()));QCOMPARE(live->entryBoundary(e.id).size(),size);QCOMPARE(draft->entryBoundary(e.id).size(),size);
            auto* corner=live->findChild<QWidget*>("freeCanvasResize_"+e.id);QVERIFY(corner);QCOMPARE(corner->pos(),live->entryBoundary(e.id).bottomRight()+QPoint(1,1));
        }
        for(int i=0;i<c.contents.size();++i) {QCOMPARE(host.captureDocument().contents[i].freeCanvasRect(),c.contents[i].freeCanvasRect());QCOMPARE(host.captureDocument().contents[i].canvasRect,c.contents[i].canvasRect);}const auto parsed=ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(WorkspaceDocument{1,0,c.id,{c},{}}));QVERIFY(parsed.ok);QCOMPARE(parsed.document.containers[0],c);
        auto* resize=preview.findChild<QWidget*>("freeCanvasResize_"+c.contents[0].id);QTest::keyClick(resize,Qt::Key_Left);
        auto* face=qobject_cast<BarPresetItem*>(preview.findChildren<MeterWidget*>()[0]->items()[0]);QVERIFY(face);QVERIFY(preview.document().contents[0].freeCanvasRect()->width()>=face->minimumFaceSize().width());
    }
    void escapeRestoresExactAbsentOpaqueAndPreciseDraftGeometry() {
        for(const QJsonValue value:{QJsonValue(),QJsonValue(QJsonObject{{"future","opaque"}}),QJsonValue("opaque"),QJsonValue(QJsonArray{-10.1234567890123,20.9876543210987,460.123456789012,100.987654321098})}) {
            QTemporaryDir dir;AppSettings settings(dir.filePath("settings.xml"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;QWidget root;QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
            ContainerDocument c;c.id="A";c.layout=ContentLayout::FreeCanvas;auto e=signal(registry,0);e.canvasRect=QRectF(-.1,.2,.3,.4);e.extensions["other"]=QJsonObject{{"keep",true}};if(!value.isNull()) {e.extensions["freeCanvasRect"]=value;}c.contents={e};WorkspaceDocument d;d.mainContainerId=c.id;d.containers={c};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
            ContainerSettingsDialog dialog(manager.container(c.id),nullptr,&manager);dialog.resize(1300,1000);dialog.show();QCoreApplication::processEvents();const auto before=dialog.editSession()->draft();const auto live=store.snapshot();auto* grip=dialog.findChild<ContainerPreviewWidget*>()->findChild<QWidget*>("freeCanvasGrip_"+e.id);QVERIFY(grip);
            QTest::mousePress(grip,Qt::LeftButton);QTest::keyClick(grip,Qt::Key_Escape);QCOMPARE(dialog.editSession()->draft(),before);QVERIFY(!dialog.editSession()->hasPendingChanges());
            gesture(grip,{30,20},true);QCOMPARE(dialog.editSession()->draft(),before);QVERIFY(!dialog.editSession()->hasPendingChanges());QCOMPARE(store.snapshot(),live);
            QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved);QCOMPARE(store.snapshot().containers[0],c);dialog.reject();
        }
    }
    void numericFallbackEditsPreserveOtherResolvedAxes() {
        for(bool opaque:{false,true}) {
            QTemporaryDir dir;AppSettings settings(dir.filePath("settings.xml"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;QWidget root;QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
            ContainerDocument c;c.id="A";c.layout=ContentLayout::FreeCanvas;auto a=signal(registry,0),b=registry.makeEntry("meter.clock");b.extensions["other"]="preserved";b.canvasRect=QRectF(-.1,.2,.3,.4);if(opaque) {b.extensions["freeCanvasRect"]=QJsonObject{{"opaque",true}};}c.contents={a,b};WorkspaceDocument d;d.mainContainerId=c.id;d.containers={c};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
            ContainerSettingsDialog dialog(manager.container(c.id),nullptr,&manager);dialog.resize(1300,1000);dialog.show();QCoreApplication::processEvents();auto* preview=dialog.findChild<ContainerPreviewWidget*>();auto* scene=preview->findChild<FreeCanvasSurface*>();const QRectF resolved=scene->logicalRect(b.id);QVERIFY(resolved.y()>0);
            dialog.findChild<QListWidget*>("containerDraftContents")->setCurrentRow(1);auto* editor=dialog.findChild<ContentPropertyEditor*>();QVERIFY(editor);QCOMPARE(editor->findChild<QDoubleSpinBox*>("canvasY")->value(),resolved.y());QCOMPARE(dialog.editSession()->draft().containers[0],c);
            editor->findChild<QDoubleSpinBox*>("canvasX")->setValue(57.25);const auto changed=dialog.editSession()->draft().containers[0].contents[1];QCOMPARE(changed.freeCanvasRect()->y(),resolved.y());QCOMPARE(changed.freeCanvasRect()->width(),resolved.width());QCOMPARE(changed.freeCanvasRect()->height(),resolved.height());QCOMPARE(changed.canvasRect,b.canvasRect);QCOMPARE(changed.extensions["other"],b.extensions["other"]);QCOMPARE(dialog.editSession()->draft().containers[0].contents[0],a);dialog.reject();
        }
    }
    void nativeAppletMinimumSurvivesReconcileAndAllEditRoutes() {
        QTemporaryDir dir;AppSettings settings(dir.filePath("settings.xml"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;QWidget root,owner;auto* native=new QLabel("Native",&owner);native->setMinimumSize(220,120);native->setMaximumSize(700,400);registry.attachSingleton("applet:rx",native);QPointer<QWidget> identity=native;
        {QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);ContainerDocument c;c.id="A";c.layout=ContentLayout::FreeCanvas;auto e=registry.makeEntry("applet:rx");e.setFreeCanvasRect(QRectF(20,30,40,30));c.contents={e};WorkspaceDocument d;d.mainContainerId=c.id;d.containers={c};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);root.show();QCoreApplication::processEvents();QCOMPARE(native->size(),QSize(40,30));auto* host=manager.contentHost(c.id);auto* grip=host->findChild<QWidget*>("freeCanvasGrip_"+e.id);gesture(grip,{10,10});QCOMPARE(native->property("freeCanvasMinimum").toSizeF(),QSizeF(220,120));
            auto* corner=host->findChild<QWidget*>("freeCanvasResize_"+e.id);gesture(corner,{-100,-100});QCOMPARE(native->size(),QSize(220,120));QTest::keyClick(corner,Qt::Key_Left,Qt::ShiftModifier);QCOMPARE(native->size(),QSize(220,120));QCOMPARE(registry.singletonView("applet:rx"),identity.data());QCOMPARE(poller.targetCountForTest(),0);
            ContainerSettingsDialog dialog(manager.container(c.id),nullptr,&manager);dialog.resize(1300,1000);dialog.show();QCoreApplication::processEvents();auto* preview=dialog.findChild<ContainerPreviewWidget*>();auto* draftCorner=preview->findChild<QWidget*>("freeCanvasResize_"+e.id);gesture(draftCorner,{-100,-100});QCOMPARE(preview->document().contents[0].freeCanvasRect()->size(),QSizeF(220,120));dialog.findChild<QListWidget*>("containerDraftContents")->setCurrentRow(0);auto* editor=dialog.findChild<ContentPropertyEditor*>();QVERIFY(editor);editor->findChild<QDoubleSpinBox*>("canvasWidth")->setValue(1);editor->findChild<QDoubleSpinBox*>("canvasHeight")->setValue(1);QCOMPARE(dialog.editSession()->draft().containers[0].contents[0].freeCanvasRect()->size(),QSizeF(220,120));dialog.reject();
            auto stack=store.snapshot();stack.containers[0].layout=ContentLayout::VerticalStack;QCOMPARE(manager.commitWorkspace(stack,stack.revision).status,CommitStatus::Saved);auto free=store.snapshot();free.containers[0].layout=ContentLayout::FreeCanvas;QCOMPARE(manager.commitWorkspace(free,free.revision).status,CommitStatus::Saved);QCOMPARE(native->property("freeCanvasMinimum").toSizeF(),QSizeF(220,120));ContainerArrangeController arrange(store,&manager);QVERIFY(arrange.popOut(e.id).ok);QVERIFY(arrange.returnEntry(e.id).ok);QCOMPARE(registry.singletonView("applet:rx"),identity.data());QCOMPARE(native->property("freeCanvasMinimum").toSizeF(),QSizeF(220,120));}
        QCOMPARE(native->minimumSize(),QSize(220,120));QCOMPARE(native->maximumSize(),QSize(700,400));registry.parkSingleton(native);QCOMPARE(native->minimumSize(),QSize(220,120));registry.returnBorrowedView(native);QCOMPARE(native->parentWidget(),&owner);QCOMPARE(native->minimumSize(),QSize(220,120));QCOMPARE(native->maximumSize(),QSize(700,400));
    }
    void nativeContextsBorrowedOwnershipAndCommittedGestures() {
        QTemporaryDir dir;AppSettings settings(dir.filePath("settings.xml"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;
        QWidget root,owner;QVBoxLayout layout(&root);QSplitter splitter(&root);layout.addWidget(&splitter);root.resize(820,650);
        ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
        connect(&manager,&ContainerManager::meterContextReady,&poller,&MeterPoller::setTargetContext);
        int reads=0;poller.setRxReadingSource([&](const QJsonObject& context,int){++reads;return context["sliceId"].toInt()==0?-73.:-93.;});
        auto* native=new QLabel("One borrowed native applet",&owner);native->setMinimumSize(180,60);native->setStyleSheet("background:#203040;color:#c8d8e8;");registry.attachSingleton("applet:rx",native);QPointer<QWidget> borrowed=native;
        ContainerDocument c;c.id="A";c.layout=ContentLayout::FreeCanvas;
        auto a=signal(registry,0),b=signal(registry,1),applet=registry.makeEntry("applet:rx");
        a.setFreeCanvasRect(QRectF(10.1234567890123,20.9876543210987,460.567890123456,100.123456789012));
        b.setFreeCanvasRect(QRectF(260,180,320,100));applet.setFreeCanvasRect(QRectF(20,340,480,100));
        a.canvasRect=QRectF(-.1234567890123,.9876543210987,.665,.19);std::unique_ptr<MeterItem> raw(registry.createMeterItem(a,nullptr,ContentRenderMode::Validation));QVERIFY(raw);a.config["legacyRecord"]=raw->serialize();
        c.contents={a,b,applet};WorkspaceDocument d;d.mainContainerId=c.id;d.containers={c};
        QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);root.show();QCoreApplication::processEvents();
        auto* host=manager.contentHost(c.id);QCOMPARE(host->meterSurfaces().size(),2);QCOMPARE(poller.targetCountForTest(),2);
        auto* faceA=qobject_cast<BarPresetItem*>(host->entryRows()[0].item.data());auto* faceB=qobject_cast<BarPresetItem*>(host->entryRows()[1].item.data());QVERIFY(faceA && faceB);
        QTRY_VERIFY(faceA->hasPrimaryReading() && faceB->hasPrimaryReading());
        host->meterSurfaces()[0]->advanceMeters(100);host->meterSurfaces()[1]->advanceMeters(100);
        QCOMPARE(faceA->primaryValue(),-73.);QCOMPARE(faceB->primaryValue(),-93.);
        QCOMPARE(host->entryRows()[2].widget.data(),native);QVERIFY(host->isAncestorOf(native));
        auto* meter=host->meterSurfaces()[0];QPointer<MeterWidget> stable=meter;const int setupReads=reads;
        const auto before=store.snapshot();auto* grip=host->findChild<QWidget*>("freeCanvasGrip_"+a.id);QVERIFY(grip);
        gesture(grip,{37,19});const auto moved=store.snapshot();QCOMPARE(moved.revision,before.revision+1);
        QCOMPARE(moved.containers[0].contents[0].freeCanvasRect()->x(),a.freeCanvasRect()->x()+37);
        QCOMPARE(moved.containers[0].contents[0].freeCanvasRect()->width(),a.freeCanvasRect()->width());
        QCOMPARE(moved.containers[0].contents[1],b);QCOMPARE(moved.containers[0].contents[0].canvasRect,a.canvasRect);
        QCOMPARE(moved.containers[0].contents[0].config["legacyRecord"],a.config["legacyRecord"]);
        QCOMPARE(host->meterSurfaces()[0],stable.data());QCOMPARE(poller.targetCountForTest(),2);QCOMPARE(reads,setupReads);
        QCOMPARE(host->entryRows()[2].widget.data(),borrowed.data());
        gesture(grip,{120,60},true);QCOMPARE(store.snapshot(),moved);QCOMPARE(host->meterSurfaces()[0],stable.data());
        auto* resize=host->findChild<QWidget*>("freeCanvasResize_"+applet.id);QVERIFY(resize);gesture(resize,{23,17});
        QCOMPARE(native->size(),QSize(503,117));QCOMPARE(registry.singletonView("applet:rx"),borrowed.data());
        registry.setAvailable("applet:rx",false);QVERIFY(native->isHidden());QCOMPARE(registry.singletonView("applet:rx"),borrowed.data());
        registry.setAvailable("applet:rx",true);QVERIFY(!native->isHidden());QCOMPARE(poller.targetCountForTest(),2);
        auto locked=store.snapshot();locked.containers[0].locked=true;QCOMPARE(manager.commitWorkspace(locked,locked.revision).status,CommitStatus::Saved);
        const auto lockedSaved=store.snapshot();gesture(grip,{20,20});QCOMPARE(store.snapshot(),lockedSaved);
        QCOMPARE(host->captureDocument().contents[0].freeCanvasRect(),lockedSaved.containers[0].contents[0].freeCanvasRect());
        QCOMPARE(host->captureDocument().contents[0].canvasRect,a.canvasRect);
        const QString capture=qEnvironmentVariable("CANVAS_CAPTURE_DIR");
        if(!capture.isEmpty()) {QVERIFY(QDir().mkpath(capture));QVERIFY(root.grab().save(capture+"/committed-mixed-native.png"));}
        auto unlocked=store.snapshot();unlocked.containers[0].locked=false;QCOMPARE(manager.commitWorkspace(unlocked,unlocked.revision).status,CommitStatus::Saved);
        const auto appletRect=store.snapshot().containers[0].contents[2].freeCanvasRect();ContainerArrangeController arrange(store,&manager);
        QVERIFY(arrange.popOut(applet.id).ok);QCOMPARE(registry.singletonView("applet:rx"),borrowed.data());QCOMPARE(poller.targetCountForTest(),2);
        QVERIFY(arrange.returnEntry(applet.id).ok);QCOMPARE(registry.singletonView("applet:rx"),borrowed.data());QCOMPARE(poller.targetCountForTest(),2);
        for(const auto& entry:store.snapshot().containers[0].contents) {if(entry.id==applet.id) {QCOMPARE(entry.freeCanvasRect(),appletRect);QCOMPARE(entry.returnLocation->containerId,c.id);}}
    }
    void stackTransitionExactAxesStyleApplyCancelAndSavedPositions() {
        QTemporaryDir dir;AppSettings settings(dir.filePath("settings.xml"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;
        QWidget root;QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
        ContainerDocument c;c.id="A";c.layout=ContentLayout::VerticalStack;
        auto a=signal(registry,0),b=registry.makeEntry("meter.clock"),hidden=registry.makeEntry("TEXT"),opaque=registry.makeEntry("future.widget");
        hidden.visible=false;hidden.extensions["future"]=QJsonArray{1,2};opaque.config["legacyRecord"]="untouched opaque";
        a.canvasRect=QRectF(-.1234567890123,.9876543210987,.665,.19);b.canvasRect=QRectF(0,0,1,1);c.contents={a,b,hidden,opaque};
        WorkspaceDocument d;d.mainContainerId=c.id;d.containers={c};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);const auto original=store.snapshot();
        ContainerSettingsDialog dialog(manager.container(c.id),nullptr,&manager);dialog.resize(1300,1000);dialog.show();QCoreApplication::processEvents();
        auto* preview=dialog.findChild<ContainerPreviewWidget*>();QVERIFY(preview);
        const QRect boundaryA=preview->entryBoundary(a.id),boundaryB=preview->entryBoundary(b.id);QVERIFY(!boundaryA.isEmpty());
        auto* modes=dialog.findChild<QComboBox*>("containerLayout");QVERIFY(modes);modes->setCurrentIndex(modes->findData(int(ContentLayout::FreeCanvas)));
        auto draft=dialog.editSession()->draft();QCOMPARE(draft.containers[0].layout,ContentLayout::FreeCanvas);
        QCOMPARE(draft.containers[0].contents[0].freeCanvasRect().value(),QRectF(boundaryA));
        QCOMPARE(draft.containers[0].contents[1].freeCanvasRect().value(),QRectF(boundaryB));
        QVERIFY(boundaryB.top()>=boundaryA.bottom());QCOMPARE(draft.containers[0].contents[2],hidden);QCOMPARE(draft.containers[0].contents[3],opaque);QCOMPARE(store.snapshot(),original);
        auto* list=dialog.findChild<QListWidget*>("containerDraftContents");list->setCurrentRow(0);
        auto* grip=preview->findChild<QWidget*>("freeCanvasGrip_"+a.id);QVERIFY(grip);gesture(grip,{17,13});
        const auto placed=dialog.editSession()->draft().containers[0].contents[0];
        auto* editor=dialog.findChild<ContentPropertyEditor*>();QVERIFY(editor);auto* x=editor->findChild<QDoubleSpinBox*>("canvasX");QVERIFY(x);x->setValue(123.456789012345);
        const auto numeric=dialog.editSession()->draft().containers[0].contents[0];QCOMPARE(numeric.freeCanvasRect()->y(),placed.freeCanvasRect()->y());QCOMPARE(numeric.freeCanvasRect()->width(),placed.freeCanvasRect()->width());
        auto* rowHeight=editor->findChild<QSpinBox*>("rowHeight");QVERIFY(rowHeight);rowHeight->setValue(96);
        QCOMPARE(dialog.editSession()->draft().containers[0].contents[0].freeCanvasRect(),numeric.freeCanvasRect());QCOMPARE(dialog.editSession()->draft().containers[0].contents[0].canvasRect,a.canvasRect);
        editor->findChild<QSpinBox*>("canvasLayer")->setValue(12);rowHeight->setValue(108);QCOMPARE(dialog.editSession()->draft().containers[0].contents[0].paintOrder,12);
        QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved);const auto applied=store.snapshot();
        modes->setCurrentIndex(modes->findData(int(ContentLayout::VerticalStack)));modes->setCurrentIndex(modes->findData(int(ContentLayout::FreeCanvas)));
        QCOMPARE(dialog.editSession()->draft().containers[0].contents[0].freeCanvasRect(),applied.containers[0].contents[0].freeCanvasRect());
        grip=preview->findChild<QWidget*>("freeCanvasGrip_"+a.id);gesture(grip,{40,20});QVERIFY(dialog.editSession()->hasPendingChanges());dialog.reject();
        QCOMPARE(store.snapshot(),applied);QCOMPARE(dialog.editSession()->draft(),applied);
        ContainerSettingsDialog saved(manager.container(c.id),nullptr,&manager);saved.resize(1300,1000);saved.show();QCoreApplication::processEvents();
        auto* savedPreview=saved.findChild<ContainerPreviewWidget*>();const QSizeF reference(qMax(320,savedPreview->width()),qMax(220,savedPreview->height()));
        const QString capture=qEnvironmentVariable("CANVAS_CAPTURE_DIR");if(!capture.isEmpty()) {QVERIFY(QDir().mkpath(capture));QVERIFY(saved.grab().save(capture+"/settings-free-canvas.png"));}
        saved.findChild<QPushButton*>("convertLegacyCanvas")->click();const auto converted=saved.editSession()->draft().containers[0].contents[0];
        QCOMPARE(converted.freeCanvasRect()->x(),a.canvasRect.x()*reference.width());QCOMPARE(converted.canvasRect,a.canvasRect);QCOMPARE(converted.config["legacyRecord"],applied.containers[0].contents[0].config["legacyRecord"]);
        saved.reject();QCOMPARE(store.snapshot(),applied);
    }
    void addNativeContentReseedLockAndOpaquePrecision() {
        QTemporaryDir dir;AppSettings settings(dir.filePath("settings.xml"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;
        QWidget root,owner;QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
        auto* borrowed=new QLabel("Native applet",&owner);registry.attachSingleton("applet:rx",borrowed);
        ContainerDocument c;c.id="A";c.layout=ContentLayout::FreeCanvas;
        auto a=signal(registry,0);a.setFreeCanvasRect(QRectF(-17.1234567890123,-33.9876543210987,460.567890123456,101.123456789012));
        auto opaque=registry.makeEntry("future.widget");opaque.setFreeCanvasRect(QRectF(-9.1234567890123,27.9876543210987,0,0));opaque.config["legacyRecord"]="opaque | untouched";
        c.contents={a,opaque};WorkspaceDocument d;d.mainContainerId=c.id;d.containers={c};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
        ContainerSettingsDialog dialog(manager.container(c.id),nullptr,&manager);dialog.resize(1300,1000);dialog.show();QCoreApplication::processEvents();
        auto* available=dialog.findChild<QListWidget*>("containerAvailableContents");QVERIFY(available);
        auto add=[&](const QString& type){for(int i=0;i<available->count();++i) {if(available->item(i)->data(Qt::UserRole).toString()==type) {available->setCurrentRow(i);available->itemDoubleClicked(available->item(i));return;}}QFAIL("Expected catalog entry");};
        add("meter.clock");add("applet:rx");const auto draft=dialog.editSession()->draft();QCOMPARE(draft.containers[0].layout,ContentLayout::FreeCanvas);QCOMPARE(draft.containers[0].contents[0],a);QCOMPARE(draft.containers[0].contents[1],opaque);
        QCOMPARE(draft.containers[0].contents.size(),4);QVERIFY(draft.containers[0].contents[2].freeCanvasRect());QVERIFY(draft.containers[0].contents[3].freeCanvasRect());
        auto* contents=dialog.findChild<QListWidget*>("containerDraftContents");contents->setCurrentRow(0);
        QCheckBox* lock=nullptr;for(auto* box:dialog.findChildren<QCheckBox*>()) {if(box->text()=="Lock") {lock=box;break;}}QVERIFY(lock);lock->setChecked(true);
        auto locked=dialog.editSession()->draft();auto* seed=dialog.findChild<QPushButton*>("seedFreeCanvas");QVERIFY(!seed->isEnabled());seed->click();
        auto* numeric=dialog.findChild<QDoubleSpinBox*>("canvasX");QVERIFY(numeric && !numeric->isEnabled());numeric->setValue(400);QCOMPARE(dialog.editSession()->draft(),locked);
        lock->setChecked(false);QVERIFY(seed->isEnabled());seed->click();const auto seeded=dialog.editSession()->draft();QCOMPARE(seeded.containers[0].layout,ContentLayout::FreeCanvas);QCOMPARE(seeded.containers[0].contents[1],opaque);
        QVERIFY(seeded.containers[0].contents[0].freeCanvasRect()->x()>=0);QCOMPARE(seeded.containers[0].contents[0].canvasRect,a.canvasRect);
        QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved);ContainerWorkspaceStore reloaded(settings);QVERIFY(reloaded.load().ok);QCOMPARE(reloaded.snapshot(),store.snapshot());QCOMPARE(reloaded.snapshot().containers[0].contents[1],opaque);dialog.reject();
    }
    void placementConflictAndStorageFailureKeepLiveGeometry() {
        QTemporaryDir dir;QVERIFY(QDir(dir.path()).mkdir("profile"));const QString profile=dir.filePath("profile");AppSettings settings(profile+"/settings.xml");ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;
        QWidget root;QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);
        ContainerDocument c;c.id="A";c.layout=ContentLayout::FreeCanvas;auto a=signal(registry,0);a.setFreeCanvasRect(QRectF(10,20,460,100));c.contents={a};WorkspaceDocument d;d.mainContainerId=c.id;d.containers={c};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
        ContainerArrangeController arrange(store,&manager);const QRectF original=a.freeCanvasRect().value();
        QVERIFY(arrange.placeFreeCanvas(a.id,QRectF(20,30,460,100),original).ok);const auto before=store.snapshot();
        QVERIFY(!arrange.placeFreeCanvas(a.id,QRectF(80,90,460,100),original).ok);QCOMPARE(store.snapshot(),before);
        auto* host=manager.contentHost(c.id);host->resize(800,450);host->show();QCoreApplication::processEvents();const QRect boundary=host->entryBoundary(a.id);QPointer<MeterWidget> stable=host->meterSurfaces()[0];
        ContainerEditSession session(store);auto draft=session.draft();draft.containers[0].contents[0].setFreeCanvasRect(QRectF(80,90,460,100));session.setDraft(draft);
        QVERIFY(QDir().rename(profile,dir.filePath("saved-profile")));QFile obstruction(profile);QVERIFY(obstruction.open(QIODevice::WriteOnly));obstruction.write("obstruction");obstruction.close();
        QCOMPARE(session.apply().status,CommitStatus::StorageError);QCOMPARE(session.draft(),draft);QCOMPARE(store.snapshot(),before);
        auto* grip=host->findChild<QWidget*>("freeCanvasGrip_"+a.id);QVERIFY(grip);gesture(grip,{45,35});QCOMPARE(store.snapshot(),before);QCOMPARE(host->entryBoundary(a.id),boundary);QCOMPARE(host->meterSurfaces()[0],stable.data());
    }
    void nativeLayeringFramesAndSmallViewport() {
        ContainerContentRegistry registry;MeterPoller poller;ContainerDocument c;c.id="frames";c.layout=ContentLayout::FreeCanvas;
        auto a=registry.makeEntry("meter.ananMulti"),b=registry.makeEntry("meter.sMeter"),clock=registry.makeEntry("meter.clock");
        a.id="a";b.id="b";clock.id="c";a.paintOrder=5;b.paintOrder=2;clock.paintOrder=7;
        a.setFreeCanvasRect(QRectF(20,30,560,280));b.setFreeCanvasRect(QRectF(330,160,320,160));clock.setFreeCanvasRect(QRectF(40,370,420,180));c.contents={a,b,clock};
        QScrollArea scroll;scroll.setWidgetResizable(true);auto* preview=new ContainerPreviewWidget(registry,poller);scroll.setWidget(preview);preview->setDocument(c);scroll.resize(820,650);scroll.show();
        const auto meters=preview->findChildren<MeterWidget*>();QCOMPARE(meters.size(),3);
        const QString capture=qEnvironmentVariable("CANVAS_CAPTURE_DIR");if(!capture.isEmpty()) {QVERIFY(QDir().mkpath(capture));}
        for(auto* meter:meters) {
#ifdef NEREUS_GPU_SPECTRUM
            QSignalSpy frames(meter,&QRhiWidget::frameSubmitted);meter->update();QTRY_VERIFY_WITH_TIMEOUT(frames.count()>0,3000);const QImage image=meter->grabFramebuffer();
#else
            meter->update();QCoreApplication::processEvents();const QImage image=meter->grab().toImage();
#endif
            QVERIFY(!image.isNull());qInfo()<<"CANVAS frame"<<meter->property("freeCanvasEntryId")<<meter->size()<<image.size()<<"DPR"<<meter->devicePixelRatioF();
            if(!capture.isEmpty()) {QVERIFY(image.save(capture+"/face-"+meter->property("freeCanvasEntryId").toString()+".png"));}
        }
        if(!capture.isEmpty()) {QVERIFY(scroll.grab().save(capture+"/native-free-overlap.png"));}
        QWidget* surface=preview->findChild<QWidget*>("freeCanvasSurface");QVERIFY(surface);
        // Hit testing must use the same explicit layer order as the native frames.
        for(auto* meter:meters) {meter->setAttribute(Qt::WA_TransparentForMouseEvents,false);}
        QWidget* hit=surface->childAt(surface->mapFrom(preview,preview->entryBoundary(a.id).topLeft()+QPoint(400,180)));
        QVERIFY(hit);QCOMPARE(hit->property("freeCanvasEntryId").toString(),a.id);
        auto* grip=preview->findChild<QWidget*>("freeCanvasGrip_"+b.id);QTest::mouseClick(grip,Qt::LeftButton);
        QCOMPARE(preview->document().contents[1].paintOrder,2);
        QVector<QRect> before;for(auto* meter:meters) {before.append(meter->geometry());}
        scroll.resize(260,180);QTRY_VERIFY(scroll.verticalScrollBar()->maximum()>0);QTRY_VERIFY(scroll.horizontalScrollBar()->maximum()>0);
        for(int i=0;i<meters.size();++i) {QCOMPARE(meters[i]->geometry(),before[i]);}
        if(!capture.isEmpty()) {QVERIFY(scroll.grab().save(capture+"/native-free-small.png"));}
    }
};
QTEST_MAIN(TstFreeCanvas)
#include "tst_free_canvas.moc"
