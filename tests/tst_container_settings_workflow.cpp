// no-port-check: NereusSDR-original complete-entry draft workflow regressions.
#include <QtTest>
#include <QPainter>
#include "gui/meters/OtherButtonItem.h"
#include "gui/containers/meter_property_editors/OtherButtonItemEditor.h"
#include "gui/containers/ContainerContentHost.h"
#include <QTemporaryDir>
#include <QSplitter>
#include <QScrollArea>
#include <QScrollBar>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include "gui/containers/ContainerWorkspaceStore.h"
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QSignalSpy>
#include <QStyleFactory>
#include <QMouseEvent>
#include "gui/styles/AppTheme.h"
#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/MeterWidget.h"
#include "gui/containers/meter_property_editors/BaseItemEditor.h"
#include "core/AppSettings.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/CompositePresetItem.h"
using namespace NereusSDR;
class TstContainerSettingsWorkflow:public QObject {
 Q_OBJECT
private slots:
 void individualControlCatalogCreatesOrdinaryIndependentEntries() {
    ContainerContentRegistry registry;
    const QList<QPair<QString, OtherButtonItem::ButtonId>> actions = {
        {"mox",OtherButtonItem::ButtonId::Mox},{"tune",OtherButtonItem::ButtonId::Tun},
        {"monitor",OtherButtonItem::ButtonId::Mon},{"twoTone",OtherButtonItem::ButtonId::TwoTon},
        {"pureSignal",OtherButtonItem::ButtonId::PsA},{"anf",OtherButtonItem::ButtonId::Anf},
        {"snb",OtherButtonItem::ButtonId::Snb},{"mnf",OtherButtonItem::ButtonId::Mnf},
        {"peak",OtherButtonItem::ButtonId::PeakHold},{"ctun",OtherButtonItem::ButtonId::Ctun},
        {"vax1",OtherButtonItem::ButtonId::Vac1},{"vax2",OtherButtonItem::ButtonId::Vac2},
        {"mute",OtherButtonItem::ButtonId::Mute},{"binaural",OtherButtonItem::ButtonId::Bin},
        {"duplex",OtherButtonItem::ButtonId::Dup}};
    int shortcuts=0;for(const auto& descriptor:registry.descriptors()) {if(descriptor.typeId.startsWith("control.")) {++shortcuts;}}
    QCOMPARE(shortcuts,15);
    for(const auto& action:actions) {
        const auto entry=registry.makeEntry("control."+action.first);
        QCOMPARE(entry.typeId,QString("OTHERBTNS"));
        const auto copy=registry.makeEntry("control."+action.first);QVERIFY(copy.id!=entry.id);
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr));
        auto* other=qobject_cast<OtherButtonItem*>(item.get());QVERIFY(other);
        QCOMPARE(other->visibleBits(),uint32_t(1u<<int(action.second)));QCOMPARE(other->columns(),1);
        QSignalSpy command(other,&OtherButtonItem::otherButtonClicked);
        const auto captured=registry.captureMeterItem(*other);QCOMPARE(captured.id,entry.id);QCOMPARE(captured.typeId,QString("OTHERBTNS"));
        QCOMPARE(command.count(),0);QVERIFY(registry.validateEntry(captured).isEmpty());
    }
 }
 void otherEditorExplicitChoiceAndVisibilityKeepLegacyData() {
    ContainerContentRegistry registry;auto entry=registry.makeEntry("OTHERBTNS");
    const uint32_t originalBits=(1u<<int(OtherButtonItem::ButtonId::Mox))|(1u<<int(OtherButtonItem::ButtonId::Tun))|(1u<<int(OtherButtonItem::ButtonId::Rx2));
    const QString raw=QString("OTHERBTNS|0.17|0.21|0.57|0.23|2|7|6|%1|future-tail").arg(originalBits);
    entry.config["legacyRecord"]=raw;entry.config["future"]=QJsonObject{{"opaque",true}};entry.config["overrides"]=QJsonObject{{"7","3"}};
    std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr));auto* other=qobject_cast<OtherButtonItem*>(item.get());QVERIFY(other);
    OtherButtonItemEditor editor;QSignalSpy changed(&editor,&BaseItemEditor::propertyChanged);QSignalSpy command(other,&OtherButtonItem::otherButtonClicked);editor.setItem(other);
    QCOMPARE(changed.count(),0);QCOMPARE(other->visibleBits(),originalBits);QCOMPARE(other->columns(),3);
    auto* choice=editor.findChild<QComboBox*>("otherSingleControl");QVERIFY(choice);QCOMPARE(choice->currentData().toInt(),-1);
    auto* tune=editor.findChild<QCheckBox*>("otherVisible_3");QVERIFY(tune);tune->setChecked(false);
    QCOMPARE(other->visibleBits(),originalBits & ~(1u<<int(OtherButtonItem::ButtonId::Tun)));
    QVERIFY(other->visibleBits() & (1u<<int(OtherButtonItem::ButtonId::Rx2)));
    choice->setCurrentIndex(choice->findData(int(OtherButtonItem::ButtonId::Mox)));
    QCOMPARE(other->visibleBits(),1u<<int(OtherButtonItem::ButtonId::Mox));QCOMPARE(other->columns(),1);QCOMPARE(command.count(),0);
    const auto captured=registry.captureMeterItem(*other,entry);QCOMPARE(captured.config["legacyRecord"].toString(),raw);QCOMPARE(captured.config["future"],entry.config["future"]);
    std::unique_ptr<MeterItem> restored(registry.createMeterItem(captured,nullptr));auto* single=qobject_cast<OtherButtonItem*>(restored.get());QVERIFY(single);QCOMPARE(single->visibleBits(),other->visibleBits());QCOMPARE(single->columns(),1);
 }
 void individualPreviewAndLiveHitTestingUseExistingAction() {
    ContainerContentRegistry registry;const auto entry=registry.makeEntry("control.mox");
    for(const auto mode:{ContentRenderMode::Preview,ContentRenderMode::Live}) {
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr,mode));auto* other=qobject_cast<OtherButtonItem*>(item.get());QVERIFY(other);other->setRect(0,0,1,1);
        QSignalSpy command(other,&OtherButtonItem::otherButtonClicked);QSignalSpy refused(other,&ButtonBoxItem::unavailableButtonClicked);
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(55,20),QPointF(55,20),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease,QPointF(55,20),QPointF(55,20),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        other->handleMousePress(&press,112,44);other->handleMouseRelease(&release,112,44);
        QCOMPARE(command.count(),mode==ContentRenderMode::Live?1:0);
        if(mode==ContentRenderMode::Live) {QCOMPARE(command.at(0).at(0).toInt(),int(OtherButtonItem::ButtonId::Mox));other->setButtonAvailable(OtherButtonItem::ButtonId::Mox,false,"Transmit is unavailable");other->handleMousePress(&press,112,44);other->handleMouseRelease(&release,112,44);QCOMPARE(command.count(),1);QCOMPARE(refused.count(),1);}
    }
 }
 void individualCatalogDraftAddApplyCancelAndFreeGeometry() {
    QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;
    QWidget root;QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);
    WorkspaceDocument document;document.mainContainerId="A";ContainerDocument container;container.id="A";container.layout=ContentLayout::FreeCanvas;
    auto existing=registry.makeEntry("TEXT");existing.setFreeCanvasRect(QRectF(-5,10,250,30));container.contents={existing};document.containers={container};QCOMPARE(manager.commitWorkspace(document,0).status,CommitStatus::Saved);const auto original=store.snapshot();
    auto addMox=[](ContainerSettingsDialog& dialog) {
        auto* list=dialog.findChild<QListWidget*>("containerAvailableContents");if(!list) {return false;}
        for(int i=0;i<list->count();++i) {if(list->item(i)->data(Qt::UserRole).toString()=="control.mox") {list->setCurrentRow(i);for(auto* button:dialog.findChildren<QPushButton*>()) {if(button->text()==QStringLiteral("Add →")) {button->click();return true;}}return false;}}
        return false;
    };
    {ContainerSettingsDialog cancel(manager.container("A"),nullptr,&manager);QVERIFY(addMox(cancel));QCOMPARE(cancel.editSession()->draft().containers[0].contents.size(),2);cancel.reject();QCOMPARE(store.snapshot(),original);}
    ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager);QVERIFY(addMox(dialog));
    auto draft=dialog.editSession()->draft();const auto created=draft.containers[0].contents.last();QCOMPARE(created.typeId,QString("OTHERBTNS"));QCOMPARE(created.name,QString("MOX"));QCOMPARE(created.freeCanvasRect(),std::optional<QRectF>(QRectF(0,60,112,44)));QCOMPARE(draft.containers[0].contents.first(),existing);
    const QString captures=qEnvironmentVariable("TASK_CONTROL_CAPTURE_DIR");
    if(!captures.isEmpty()) {
        QVERIFY(QDir().mkpath(captures));dialog.resize(1400,1000);dialog.show();dialog.activateWindow();QCoreApplication::processEvents();
        auto* picker=dialog.findChild<QComboBox*>("otherSingleControl");QVERIFY(picker);QCOMPARE(picker->currentData().toInt(),int(OtherButtonItem::ButtonId::Mox));
        for(QWidget* ancestor=picker->parentWidget();ancestor;ancestor=ancestor->parentWidget()) {if(auto* scroll=qobject_cast<QScrollArea*>(ancestor)) {scroll->verticalScrollBar()->setValue(picker->mapTo(scroll->widget(),QPoint()).y()-20);break;}}
        auto* available=dialog.findChild<QListWidget*>("containerAvailableContents");for(int row=0;row<available->count();++row) {if(available->item(row)->data(Qt::UserRole).toString()=="control.mox") {available->setCurrentRow(row);available->scrollToItem(available->item(row));break;}}
        QCoreApplication::processEvents();QVERIFY(dialog.grab().save(captures+"/individual-control-picker.png"));
    }
    QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved);ContainerWorkspaceStore reloaded(settings);QVERIFY(reloaded.load().ok);QCOMPARE(reloaded.snapshot().containers[0].contents.last().config,created.config);
    const auto saved=store.snapshot();QVERIFY(addMox(dialog));QCOMPARE(dialog.editSession()->draft().containers[0].contents.size(),3);dialog.reject();QCOMPARE(store.snapshot(),saved);
    draft=store.snapshot();draft.containers[0].locked=true;QCOMPARE(manager.commitWorkspace(draft,draft.revision).status,CommitStatus::Saved);ContainerSettingsDialog locked(manager.container("A"),nullptr,&manager);QVERIFY(addMox(locked));QCOMPARE(locked.editSession()->draft().containers[0].contents.size(),2);
 }
 void singleControlPaintAndHitBoundsFollowViewport() {
    ContainerContentRegistry registry;auto entry=registry.makeEntry("OTHERBTNS");entry.config["legacyRecord"]=QString("OTHERBTNS|0|0|1|1|0|0|1|%1").arg(1u<<int(OtherButtonItem::ButtonId::Mox));
    std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr));auto* other=qobject_cast<OtherButtonItem*>(item.get());QVERIFY(other);QSignalSpy clicked(other,&OtherButtonItem::otherButtonClicked);
    for(const QSize& size:{QSize(64,32),QSize(112,44),QSize(220,70)}) {
        QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);other->paint(painter,size.width(),size.height());painter.end();
        QVERIFY(image.pixelColor(size.width()/2,size.height()/2).alpha()>0);
        // The native cell ends before the viewport's bottom padding; paint and hit testing use that same edge.
        QVERIFY(image.pixelColor(size.width()/2,size.height()-1).alpha()<200);
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(size.width()/2,size.height()-0.1),QPointF(size.width()/2,size.height()-0.1),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease,QPointF(size.width()/2,size.height()-0.1),QPointF(size.width()/2,size.height()-0.1),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        other->handleMousePress(&press,size.width(),size.height());other->handleMouseRelease(&release,size.width(),size.height());QCOMPARE(clicked.count(),0);
    }
 }
 void individualControlStackUsesUsableCompactViewport() {
    ContainerContentRegistry registry;ContainerContentHost host(registry);ContainerDocument document;document.id="A";document.layout=ContentLayout::VerticalStack;document.contents={registry.makeEntry("control.mox"),registry.makeEntry("control.tune")};host.reconcile(document);host.resize(420,200);host.show();QCoreApplication::processEvents();
    QCOMPARE(host.entryRows().size(),2);QCOMPARE(host.entryRows()[0].height,44);QCOMPARE(host.entryRows()[1].height,44);QCOMPARE(host.meterSurfaces().first()->height(),88);
 }
 void initTestCase() {AppSettings::setProfileOverride(QStringLiteral("task10-settings-%1").arg(QCoreApplication::applicationPid()));AppSettings::instance().clear();}
 void cleanupTestCase() {QFile::remove(AppSettings::instance().filePath());}
 void selectionRemainsReadableWithAndWithoutFocus_data() {
    QTest::addColumn<bool>("applicationTheme");
    QTest::newRow("application-theme")<<true;
    QTest::newRow("host-palette")<<false;
 }
 void selectionRemainsReadableWithAndWithoutFocus() {
    QFETCH(bool,applicationTheme);
    qApp->setStyle(QStyleFactory::create("Fusion"));
    if(applicationTheme) {applyDarkPalette(*qApp);applyAppBaselineQss(*qApp);}
    else {qApp->setPalette(qApp->style()->standardPalette());qApp->setStyleSheet({});}
    QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;
    QWidget root;QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
    WorkspaceDocument document;document.mainContainerId="A";ContainerDocument container;container.id="A";container.contents={registry.makeEntry("TEXT"),registry.makeEntry("BAR")};ContainerDocument second;second.id="B";second.name="Other container";document.containers={container,second};
    QCOMPARE(manager.commitWorkspace(document,0).status,CommitStatus::Saved);
    const auto saved=store.snapshot();
    ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager);dialog.show();dialog.activateWindow();
    const QString baseCaptures=qEnvironmentVariable("CORE_REFRESH_CAPTURE_DIR");
    const QString captures=baseCaptures.isEmpty()?QString():baseCaptures+"/"+QString::fromLatin1(QTest::currentDataTag());
    if(!captures.isEmpty()) {QVERIFY(QDir().mkpath(captures));}
    auto checkSelection=[&](QAbstractItemView* view,const QModelIndex& index,const QString& name) {
        QCoreApplication::processEvents();
        const QPixmap pixmap=view->viewport()->grab();
        const QImage image=pixmap.toImage();const qreal scale=pixmap.devicePixelRatio();
        const QRect row=view->visualRect(index);const QRect pixels(QPoint(qRound(row.left()*scale),qRound(row.top()*scale)),QSize(qRound(row.width()*scale),qRound(row.height()*scale)));
        int cyan=0,dark=0;
        for(int y=pixels.top();y<=pixels.bottom() && y<image.height();++y) {
            for(int x=pixels.left();x<=pixels.right() && x<image.width();++x) {
                const QColor color=image.pixelColor(x,y);
                cyan+=color.green()>130 && color.blue()>160 && color.red()<60;
                dark+=color.red()<70 && color.green()<70 && color.blue()<80;
            }
        }
        qInfo()<<name<<"row"<<row<<"cyan"<<cyan<<"dark"<<dark<<"area"<<pixels.width()*pixels.height()<<"palette"<<view->palette().color(QPalette::Highlight)<<view->palette().color(QPalette::HighlightedText);
        if(!captures.isEmpty()) {QVERIFY(pixmap.save(captures+"/"+name+".png"));QVERIFY(dialog.grab().save(captures+"/"+name+"-dialog.png"));}
        QVERIFY2(cyan>pixels.width()*pixels.height()/3,qPrintable(name+" must retain a distinct cyan selection"));
        QVERIFY2(dark>5,qPrintable(name+" must render contrasting dark selection text"));
    };
    for(const QString& name:{QString("containerAvailableContents"),QString("containerDraftContents")}) {
        auto* list=dialog.findChild<QListWidget*>(name);QVERIFY(list);
        int row=0;while(row<list->count() && !(list->item(row)->flags() & Qt::ItemIsSelectable)) {++row;}
        QVERIFY(row<list->count());list->setCurrentRow(row);list->setFocus();QCoreApplication::processEvents();
        checkSelection(list,list->currentIndex(),name+"-focused");
        dialog.findChild<QLineEdit*>("containerDraftTitle")->setFocus();checkSelection(list,list->currentIndex(),name+"-unfocused");
    }
    for(const QString& name:{QString("containerHeader"),QString("containerLayout"),QString("containerPlacement"),QString("containerAnchor"),QString("containerRxSource"),QString("contentSlice"),QString("containerDraftSelection")}) {
        auto* combo=dialog.findChild<QComboBox*>(name);QVERIFY(combo);
        if(!captures.isEmpty()) {QVERIFY(combo->grab().save(captures+"/"+name+"-closed.png"));}
        combo->showPopup();QCoreApplication::processEvents();
        auto* view=combo->view();view->setCurrentIndex(combo->model()->index(0,0));view->setFocus();
        checkSelection(view,view->currentIndex(),name+"-popup-focused");
        view->clearFocus();checkSelection(view,view->currentIndex(),name+"-popup-unfocused");
        const QModelIndex hover=combo->model()->index(1,0);view->scrollTo(hover);
        const QPoint position=view->visualRect(hover).center();
        QMouseEvent move(QEvent::MouseMove,position,view->viewport()->mapToGlobal(position),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(view->viewport(),&move);
        QTRY_COMPARE(view->currentIndex(),hover);
        checkSelection(view,hover,name+"-popup-hover");
        QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,position);
        QTRY_COMPARE(combo->currentIndex(),1);combo->hidePopup();
    }
    QCOMPARE(store.snapshot(),saved);QCOMPARE(poller.targetCountForTest(),0);dialog.reject();
 }
 void completeRowsPropertiesApplyReloadCancelAndInvalidImport() {
    QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;
    QWidget root;QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
    WorkspaceDocument d;d.mainContainerId="A";ContainerDocument a,b;a.id="A";a.name="Mixed readings";a.layout=ContentLayout::VerticalStack;a.config["sliceId"]=1;b.id="B";b.name="Other";
    auto bar=registry.makeEntry("meter.mic");bar.context={{"sliceId",0},{"rxSource",1},{"sessionId","foreign"},{"extension",17}};auto properties=bar.config["properties"].toObject();properties["unknown"]=QJsonObject{{"nested",true}};bar.config["properties"]=properties;
    auto opaque=registry.makeEntry("DISCORDBTNS");opaque.name="Retired legacy control";opaque.config["legacyRecord"]="DISCORDBTNS|exact customized legacy";
    auto clock=registry.makeEntry("meter.clock");clock.name="Station clocks";a.contents={bar,opaque,clock};d.containers={a,b};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);const auto original=store.snapshot();
    ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager);auto* list=dialog.findChild<QListWidget*>("containerDraftContents");QCOMPARE(list->count(),3);QVERIFY(list->item(1)->text().contains("Retired"));QVERIFY(!list->item(0)->text().startsWith('{'));
    list->setCurrentRow(0);auto* editor=dialog.findChild<ContentPropertyEditor*>();QVERIFY(editor);auto* slice=editor->findChild<QComboBox*>("contentSlice");slice->setCurrentIndex(0);
    auto entry=dialog.editSession()->draft().containers[0].contents[0];QVERIFY(!entry.context.contains("sliceId"));QVERIFY(!entry.context.contains("rxSource"));QCOMPARE(entry.context["sessionId"].toString(),QString("foreign"));QCOMPARE(entry.context["extension"].toInt(),17);
    auto* height=editor->findChild<QSpinBox*>("rowHeight");QVERIFY(height);height->setValue(96);QVERIFY(!editor->findChild<QDoubleSpinBox*>("minValue"));
    auto* title=editor->findChild<QLineEdit*>("contentName");title->setText("Custom microphone");title->textEdited(title->text());
    entry=dialog.editSession()->draft().containers[0].contents[0];QCOMPARE(entry.name,QString("Custom microphone"));std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr,ContentRenderMode::Preview));QCOMPARE(qobject_cast<BarPresetItem*>(item.get())->preferredRowHeight(),96);QVERIFY(entry.config["properties"].toObject()["unknown"].toObject()["nested"].toBool());
    dialog.findChild<QPushButton*>("containerBackground")->setProperty("draftColor",QString("#ff182b38"));
    auto* header=dialog.findChild<QComboBox*>("containerHeader");QCOMPARE(header->count(),3);header->setCurrentIndex(1);
    const auto before=dialog.editSession()->draft();QVERIFY(!dialog.importPortableEntries("{\"format\":\"nereus.entries\",\"schemaVersion\":99}"));QCOMPARE(dialog.editSession()->draft(),before);QCOMPARE(store.snapshot(),original);
    list->setCurrentRow(1);QVERIFY(dialog.findChild<ContentPropertyEditor*>());QVERIFY(QMetaObject::invokeMethod(&dialog,"onMoveItemUp"));QCOMPARE(dialog.editSession()->draft().containers[0].contents[0].id,opaque.id);
    list->setCurrentRow(0);QVERIFY(QMetaObject::invokeMethod(&dialog,"onRemoveItem"));QCOMPARE(dialog.editSession()->draft().containers[0].contents.size(),2);
    QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved);ContainerWorkspaceStore reloaded(settings);auto saved=reloaded.load();QVERIFY(saved.ok);QCOMPARE(saved.document.containers[0].header,HeaderMode::Reveal);QCOMPARE(manager.container("A")->backgroundColor(),QColor("#ff182b38"));QCOMPARE(saved.document.containers[0].contents[0].name,QString("Custom microphone"));
    QVERIFY(!dialog.editSession()->hasPendingChanges());auto* draftTitle=dialog.findChild<QLineEdit*>("containerDraftTitle");const auto savedName=draftTitle->text();draftTitle->setText("Pending after Apply");QVERIFY(dialog.editSession()->hasPendingChanges());QVERIFY(dialog.findChild<QLabel*>("containerDraftStatus")->text().contains("Pending Apply"));draftTitle->setText(savedName);QVERIFY(!dialog.editSession()->hasPendingChanges());
    const QString imported="TEXT|0|0|1|1|0|0|custom legacy";QVERIFY(dialog.importPortableEntries(imported));QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved);saved=reloaded.load();const auto importedEntry=saved.document.containers[0].contents.last();QCOMPARE(importedEntry.config["legacyRecord"].toString(),imported);QVERIFY(importedEntry.extensions.contains("nereusPortableRecovery"));
    list->setCurrentRow(0);dialog.resize(1300,950);dialog.show();QCoreApplication::processEvents();const QString captures=qEnvironmentVariable("TASK9_CAPTURE_DIR");if(!captures.isEmpty()){QVERIFY(QDir().mkpath(captures));QVERIFY(dialog.grab().save(captures+"/settings-properties.png"));}
    auto draft=dialog.editSession()->draft();draft.containers[0].name="cancelled";dialog.editSession()->setDraft(draft);dialog.reject();QCOMPARE(store.snapshot(),saved.document);
 }
 void legacyChannelHydrationValidationAndHeightReselection() {
    ContainerContentRegistry registry;auto entry=registry.makeEntry("meter.powerSwr");auto props=entry.config["properties"].toObject();auto channels=props["channels"].toArray();auto first=channels[0].toObject();first["futureNested"]=QJsonObject{{"retain",19}};channels[0]=first;props["channels"]=channels;
    entry.config["legacyRecord"]=QString::fromUtf8(QJsonDocument(props).toJson(QJsonDocument::Compact));entry.config.remove("properties");entry.typeId="PowerSwrPreset";
    const QString raw=entry.config["legacyRecord"].toString();ContentPropertyEditor editor(registry);ContentEntry edited=entry;connect(&editor,&ContentPropertyEditor::entryEdited,this,[&](const ContentEntry& e){edited=e;});editor.setEntry(entry);
    auto* attack=editor.findChild<QDoubleSpinBox*>("channel0_attack");QVERIFY(attack);attack->setValue(.35);QCOMPARE(edited.config["legacyRecord"].toString(),raw);QCOMPARE(edited.config["properties"].toObject()["channels"].toArray()[0].toObject()["futureNested"].toObject()["retain"].toInt(),19);
    auto* height=editor.findChild<QSpinBox*>("faceHeight");height->setValue(240);editor.setEntry(edited);height=editor.findChild<QSpinBox*>("faceHeight");height->setValue(144);QCOMPARE(edited.config["properties"].toObject()["faceHeight"].toInt(),144);
    auto* history=editor.findChild<QSpinBox*>("channel0_historyMs");const int previous=history->value();history->setValue(100);auto* interval=editor.findChild<QSpinBox*>("channel0_updateIntervalMs");interval->setValue(300);QVERIFY(interval->value()<=history->value());
    std::unique_ptr<MeterItem> item(registry.createMeterItem(edited,nullptr,ContentRenderMode::Preview));QVERIFY(item);QCOMPARE(qobject_cast<CompositePresetItem*>(item.get())->configuration()["channels"].toArray()[0].toObject()["attack"].toDouble(),.35);QVERIFY(previous>0);
 }
 void rendererUndoKeepsIndependentNameSourceAndMmioAxes() {
    ContainerContentRegistry registry;auto entry=registry.makeEntry("meter.mic");entry.config.remove("properties");const auto original=entry;
    ContentPropertyEditor editor(registry);ContentEntry edited=entry;connect(&editor,&ContentPropertyEditor::entryEdited,this,[&](const ContentEntry& e){edited=e;});editor.setEntry(entry);
    auto* name=editor.findChild<QLineEdit*>("contentName");name->setText("Renamed legacy face");name->textEdited(name->text());auto* slice=editor.findChild<QComboBox*>("contentSlice");slice->setCurrentIndex(slice->findData(2));
    auto* height=editor.findChild<QSpinBox*>("rowHeight");const int old=height->value();height->setValue(old+24);height->setValue(old);QCOMPARE(edited.config,original.config);QCOMPARE(edited.name,QString("Renamed legacy face"));QCOMPARE(edited.context["sliceId"].toInt(),2);
    auto primitive=registry.makeEntry("BAR");editor.setEntry(primitive);auto* guid=editor.findChild<QLineEdit*>("mmioGuid");auto* variable=editor.findChild<QLineEdit*>("mmioVariable");const QString id="00112233-4455-6677-8899-aabbccddeeff";guid->setText(id);guid->editingFinished();variable->setText("alpha");variable->editingFinished();
    auto* adapter=editor.findChild<BaseItemEditor*>();QVERIFY(adapter);auto* bar=qobject_cast<BarItem*>(adapter->item());bar->setBarColor(Qt::green);adapter->propertyChanged();QCOMPARE(edited.context["mmioGuid"].toString(),id);QCOMPARE(edited.context["mmioVariable"].toString(),QString("alpha"));
    QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);WorkspaceDocument d;ContainerDocument c;c.id="A";d.mainContainerId=c.id;c.contents={edited};d.containers={c};QCOMPARE(store.commit(d,0).status,CommitStatus::Saved);ContainerWorkspaceStore reload(settings);const auto restored=reload.load().document.containers[0].contents[0];std::unique_ptr<MeterItem> item(registry.createMeterItem(restored,nullptr,ContentRenderMode::Preview));QVERIFY(item);QCOMPARE(item->mmioGuid(),QUuid(id));QCOMPARE(item->mmioVariable(),QString("alpha"));QCOMPARE(qobject_cast<BarItem*>(item.get())->barColor(),QColor(Qt::green));
 }
 void duplicateInvalidLegacyEntryLeavesDraftUnchanged() {
    QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;
    QWidget root;QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
    WorkspaceDocument d;ContainerDocument c;c.id="A";d.mainContainerId=c.id;auto e=registry.makeEntry("meter.mic");auto props=e.config["properties"].toObject();props["style"]="invalid style";e.config["properties"]=props;c.contents={e};d.containers={c};QCOMPARE(store.commit(d,0).status,CommitStatus::Saved);
    ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager);dialog.findChild<QListWidget*>("containerDraftContents")->setCurrentRow(0);const auto original=dialog.editSession()->draft();
    dialog.findChild<QPushButton*>("duplicateContent")->click();QCOMPARE(dialog.editSession()->draft(),original);
    bool explained=false;for(auto* label:dialog.findChildren<QLabel*>()) {explained|=label->text().contains("Invalid");}QVERIFY(explained);dialog.reject();
 }
 void duplicatedShellPresentationsHaveNewHomes_data() {
    QTest::addColumn<bool>("wholeContainer");QTest::newRow("object")<<false;QTest::newRow("container")<<true;
 }
 void duplicatedShellPresentationsHaveNewHomes() {
    QFETCH(bool,wholeContainer);
    QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;
    RadioModel model(RadioModel::Role::Remote);SliceModel slice(0);QSignalSpy frequencies(&slice,&SliceModel::frequencyChanged);
    QWidget root,liveParent;RxApplet singleton(&slice,&model,&liveParent);registry.attachSingleton("applet:rx",&singleton);QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
    WorkspaceDocument d;d.mainContainerId="main";ContainerDocument main,home;main.id="main";home.id="B";home.layout=ContentLayout::VerticalStack;
    auto before=registry.makeEntry("TEXT"),original=registry.makeEntry("meter.mic"),after=registry.makeEntry("TEXT");original.name="Customized microphone";
    original.context={{"mmioGuid","00112233-4455-6677-8899-aabbccddeeff"},{"mmioVariable","alpha"},{"future",17}};original.extensions["unknown"]=QJsonObject{{"exact","keep"}};
    auto singletonEntry=registry.makeEntry("applet:rx");main.contents={singletonEntry};home.contents={before,original,after};d.containers={main,home};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
    ContainerArrangeController arrange(store,&manager);QVERIFY(arrange.popOut(original.id).ok);const QString shell=store.snapshot().containers.last().id;
    const auto popped=store.snapshot().containers.last().contents.first();QVERIFY(popped.returnLocation);QCOMPARE(popped.returnLocation->containerId,QString("B"));
    QWidget* singletonParent=singleton.parentWidget();
    const QByteArray recoverySource=wholeContainer?ContainerDocumentCodec::exportContainer(store.snapshot().containers.last()):ContainerDocumentCodec::exportEntries({popped}).toUtf8();
    ContainerSettingsDialog dialog(manager.container(shell),nullptr,&manager);dialog.findChild<QListWidget*>("containerDraftContents")->setCurrentRow(0);
    QPushButton* duplicate=dialog.findChild<QPushButton*>("duplicateContent");
    if(wholeContainer) {for(auto* button:dialog.findChildren<QPushButton*>()) {if(button->text()=="Duplicate") {duplicate=button;break;}}}
    QVERIFY(duplicate);duplicate->click();const auto draft=dialog.editSession()->draft();
    const auto copy=wholeContainer?draft.containers.last().contents.first():draft.containers.last().contents.last();
    QVERIFY(copy.id!=original.id);QVERIFY2(!copy.returnLocation,"New Settings copies must not inherit a popped object's remembered home");
    QCOMPARE(copy.config,original.config);QCOMPARE(copy.context,original.context);QCOMPARE(copy.extensions["unknown"],original.extensions["unknown"]);QVERIFY(copy.extensions.contains("nereusPortableRecovery"));
    const auto recovery=copy.extensions["nereusPortableRecovery"].toObject()["records"].toArray();QVERIFY(!recovery.isEmpty());
    const QByteArray recoveredBytes=QByteArray::fromBase64(recovery.first().toObject()["sourceBase64"].toString().toLatin1());
    if(wholeContainer) {QCOMPARE(recoveredBytes,recoverySource);}
    // exportEntries gives its envelope a fresh UUID; its original entry, including
    // the active home and anchors, must still be retained exactly in recovery.
    const auto recovered=ContainerDocumentCodec::decode(QJsonDocument(QJsonDocument::fromJson(recoveredBytes).object()["workspace"].toObject()).toJson());
    QVERIFY(recovered.ok);QCOMPARE(recovered.document.containers.first().contents.first(),popped);
    QCOMPARE(copy.name,original.name+(wholeContainer?QString():QString(" copy")));
    QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved);
    const QString copiedShell=wholeContainer?store.snapshot().containers.last().id:shell;
    QVERIFY(arrange.closeContainer(shell).ok);if(wholeContainer) {QVERIFY(arrange.closeContainer(copiedShell).ok);}
    const auto saved=store.snapshot();QCOMPARE(saved.containers[0].contents.size(),2);QCOMPARE(saved.containers[0].contents.first().id,singletonEntry.id);QCOMPARE(saved.containers[0].contents.last().id,copy.id);
    QCOMPARE(saved.containers[1].contents.size(),3);QCOMPARE(saved.containers[1].contents[0].id,before.id);QCOMPARE(saved.containers[1].contents[1].id,original.id);QCOMPARE(saved.containers[1].contents[2].id,after.id);
    auto returned=saved.containers[1].contents[1];returned.returnLocation=original.returnLocation;QCOMPARE(returned,original);
    ContainerWorkspaceStore reload(settings);QCOMPARE(reload.load().document,saved);QCOMPARE(frequencies.count(),0);QCOMPARE(poller.targetCountForTest(),0);QCOMPARE(registry.singletonView("applet:rx"),static_cast<QWidget*>(&singleton));QCOMPARE(singleton.parentWidget(),singletonParent);
    ContainerSettingsDialog singletonDialog(manager.container("main"),nullptr,&manager);const auto unchanged=singletonDialog.editSession()->draft();
    for(auto* button:singletonDialog.findChildren<QPushButton*>()) {if(button->text()=="Duplicate") {button->click();break;}}
    QCOMPARE(singletonDialog.editSession()->draft(),unchanged);QCOMPARE(registry.singletonView("applet:rx"),static_cast<QWidget*>(&singleton));
 }
 void unsupportedContainerOptionsKeepRawValues_data() {
    QTest::addColumn<QJsonValue>("minimises");QTest::addColumn<QJsonValue>("unused");
    QTest::newRow("booleans")<<QJsonValue(true)<<QJsonValue(false);
    QTest::newRow("raw")<<QJsonValue("legacy preference")<<QJsonValue(QJsonObject{{"unknown",19}});
 }
 void unsupportedContainerOptionsKeepRawValues() {
    QFETCH(QJsonValue,minimises);QFETCH(QJsonValue,unused);
    QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;
    QWidget root;QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
    WorkspaceDocument d;d.mainContainerId="A";ContainerDocument a,b;a.id="A";b.id="B";a.config={{"containerMinimises",minimises},{"hidesWhenRxNotUsed",unused}};d.containers={a,b};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
    ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager);const auto original=dialog.editSession()->draft();
    int found=0;for(auto* check:dialog.findChildren<QCheckBox*>()) {if(check->text()=="Minimizes" || check->text()=="Hide when RX unused") {++found;QVERIFY(!check->isEnabled());QVERIFY(!check->toolTip().isEmpty());check->click();}}
    QCOMPARE(found,2);dialog.selectDraftContainer("B");dialog.selectDraftContainer("A");QCOMPARE(dialog.editSession()->draft(),original);
    QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved);ContainerWorkspaceStore reload(settings);QCOMPARE(reload.load().document.containers[0].config,a.config);
    const auto portable=ContainerDocumentCodec::importContainer(ContainerDocumentCodec::exportContainer(store.snapshot().containers[0]));QVERIFY(portable.ok);QCOMPARE(portable.document.containers[0].config,a.config);
    ContainerWidget legacy;legacy.setContainerMinimises(true);legacy.setContainerHidesWhenRxNotUsed(true);ContainerSettingsDialog legacyDialog(&legacy);
    for(auto* check:legacyDialog.findChildren<QCheckBox*>()) {if(check->text()=="Minimizes" || check->text()=="Hide when RX unused") {QVERIFY(!check->isEnabled());check->click();}}
    QPushButton* apply=nullptr;for(auto* button:legacyDialog.findChildren<QPushButton*>()) {if(button->text()=="Apply") {apply=button;break;}}QVERIFY(apply);apply->click();QVERIFY(legacy.containerMinimises());QVERIFY(legacy.containerHidesWhenRxNotUsed());
 }
 void legacySignalUnitsUseGlobalPreferences() {
    ContainerContentRegistry registry;ContentPropertyEditor editor(registry);editor.setEntry(registry.makeEntry("SIGNALTEXT"));
    auto* units=editor.findChild<QComboBox*>("signalGlobalUnits");QVERIFY(units);QVERIFY(!units->isEnabled());QVERIFY(units->toolTip().contains("global multimeter"));
 }
 void retainedReturnOrderAndLockedAtomicFailure() {
    WorkspaceDocument d;d.mainContainerId="main";ContainerDocument main,home,shell;main.id="main";home.id="home";shell.id="shell";ContentEntry before,after,returned,unknown;before.id="before";after.id="after";returned.id="returned";unknown.id="unknown";before.typeId=after.typeId=returned.typeId="TEXT";unknown.typeId="future";unknown.config["raw"]="opaque";returned.returnLocation=ReturnLocation{"home","before","after",{}};unknown.returnLocation=ReturnLocation{"missing",{},{},{}};home.contents={before,after};shell.contents={returned,unknown};d.containers={main,home,shell};
    auto locked=d;locked.containers[1].locked=true;const auto original=locked;QVERIFY(!ContainerArrangeController::returnBatch(locked,2,locked.containers[2].contents).ok);QCOMPARE(locked,original);
    QVERIFY(ContainerArrangeController::returnBatch(d,2,d.containers[2].contents).ok);d.containers.removeLast();QCOMPARE(d.containers[1].contents[1].id,returned.id);QCOMPARE(d.containers[0].contents[0].config,unknown.config);
 }
 void nativeMixedPreviewHasCachedReadingActualAppletAndLegacyContent() {
    QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;
    RadioModel model(RadioModel::Role::Remote);SliceModel slice(0);QWidget root,liveParent;RxApplet applet(&slice,&model,&liveParent);applet.resize(520,300);liveParent.show();applet.show();registry.attachSingleton("applet:rx",&applet);
    QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
    WorkspaceDocument d;d.mainContainerId="A";ContainerDocument a;a.id="A";a.name="Station monitor";a.layout=ContentLayout::VerticalStack;a.header=HeaderMode::Reveal;
    auto face=registry.makeEntry("meter.mic");face.name="Microphone peak / average";auto props=face.config["properties"].toObject();props["titleColor"]="#ffff6e40";props["rowHeight"]=96;face.config["properties"]=props;
    auto legacy=registry.makeEntry("BAR");legacy.name="Customized legacy bar";std::unique_ptr<MeterItem> legacyItem(registry.createMeterItem(legacy,nullptr,ContentRenderMode::Preview));auto* bar=qobject_cast<BarItem*>(legacyItem.get());bar->setBindingId(MeterBinding::TxMic);bar->setBarColor(QColor("#33aadd"));legacy=registry.captureMeterItem(*bar,legacy);legacy.config["retainedPluginConfig"]=QJsonObject{{"future",true}};
    auto unavailable=registry.makeEntry("DISCORDBTNS");unavailable.name="Retained retired control";unavailable.extensions["unavailableReason"]="Retired — original customized record retained";
    a.contents={face,registry.makeEntry("applet:rx"),legacy,unavailable};d.containers={a};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);const auto parent=applet.parentWidget();root.resize(700,500);root.show();
    poller.setInTx(true);poller.handOutTxReadingForTest(MeterBinding::TxMicPeak,-4);poller.handOutTxReadingForTest(MeterBinding::TxMic,-8);
    ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager);dialog.findChild<QListWidget*>("containerDraftContents")->setCurrentRow(0);dialog.resize(1300,1050);dialog.show();
    auto* preview=dialog.findChild<ContainerPreviewWidget*>();QVERIFY(preview);auto* meter=preview->findChild<MeterWidget*>();QVERIFY(meter);auto* mic=qobject_cast<BarPresetItem*>(meter->items()[0]);QVERIFY(mic && mic->hasPrimaryReading());
    for(int timestamp=0;timestamp<=500;timestamp+=100) {poller.frameAdvanced(timestamp);}
    QVERIFY(mic->primaryValue()>-10);
#ifdef NEREUS_GPU_SPECTRUM
    QSignalSpy frames(meter,&QRhiWidget::frameSubmitted);meter->update();QTRY_VERIFY_WITH_TIMEOUT(frames.count()>0,3000);QVERIFY(!meter->grabFramebuffer().isNull());
#else
    QCoreApplication::processEvents();
#endif
    const QString captures=qEnvironmentVariable("TASK9_CAPTURE_DIR");if(!captures.isEmpty()) {QVERIFY(QDir().mkpath(captures));QVERIFY(dialog.grab().save(captures+"/settings-mixed-native.png"));
        auto* scroll=qobject_cast<QScrollArea*>(preview->parentWidget()->parentWidget());QVERIFY(scroll);
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        dialog.findChild<QListWidget*>("containerDraftContents")->setCurrentRow(2);QCoreApplication::processEvents();
        QVERIFY(dialog.grab().save(captures+"/settings-legacy-properties.png"));
        dialog.findChild<QListWidget*>("containerDraftContents")->setCurrentRow(3);QCoreApplication::processEvents();
        QVERIFY(dialog.grab().save(captures+"/settings-unavailable.png"));
    }
    QCOMPARE(applet.parentWidget(),parent);QCOMPARE(poller.targetCountForTest(),0);dialog.reject();
 }
 void singletonImportsMoveOnlyAndRecoveryCommits() {
    QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;MeterPoller poller;
    QWidget root,liveParent,singleton(&liveParent);registry.attachSingleton("applet:rx",&singleton);QSplitter splitter(&root);ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.setPreviewPoller(&poller);
    WorkspaceDocument d;d.mainContainerId="A";ContainerDocument a,b,c;a.id="A";b.id="B";c.id="C";a.layout=b.layout=c.layout=ContentLayout::VerticalStack;auto applet=registry.makeEntry("applet:rx");applet.extensions["portableImportedEntry"]="user metadata";auto before=registry.makeEntry("BAR"),after=registry.makeEntry("TEXT");b.contents={before,applet,after};d.containers={a,b,c};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);auto* parent=singleton.parentWidget();
    ContainerSettingsDialog dialog(manager.container("C"),nullptr,&manager);
    ContainerDocument imported;imported.id="portable";imported.layout=ContentLayout::VerticalStack;auto meter=registry.makeEntry("meter.mic");meter.returnLocation=ReturnLocation{imported.id,applet.id,"external",{}};imported.contents={applet,meter};
    QVERIFY(dialog.importPortableContainer(ContainerDocumentCodec::exportContainer(imported)));auto draft=dialog.editSession()->draft();QCOMPARE(draft.containers[1].contents.size(),2);QCOMPARE(draft.containers[2].contents[0].id,applet.id);QCOMPARE(singleton.parentWidget(),parent);QCOMPARE(store.snapshot().containers[1].contents[1].id,applet.id);
    const auto moved=draft.containers[2].contents[0];QVERIFY(moved.returnLocation);QCOMPARE(moved.returnLocation->containerId,QString("B"));QCOMPARE(moved.returnLocation->beforeId,before.id);QCOMPARE(moved.returnLocation->afterId,after.id);QCOMPARE(moved.extensions["portableImportedEntry"].toString(),QString("user metadata"));
    QCOMPARE(draft.containers[2].contents[1].returnLocation->beforeId,applet.id);QCOMPARE(draft.containers[2].contents[1].returnLocation->containerId,QString("C"));
    QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved);QVERIFY(singleton.parentWidget()!=parent);ContainerWorkspaceStore reload(settings);QVERIFY(reload.load().document.containers[2].contents[0].extensions.contains("nereusPortableRecovery"));QCOMPARE(poller.targetCountForTest(),0);
    auto locked=dialog.editSession()->draft();locked.containers[1].locked=true;dialog.editSession()->setDraft(locked);dialog.selectDraftContainer("C");const auto beforeFailed=dialog.editSession()->draft();
    QPushButton* remove=nullptr;for(auto* button:dialog.findChildren<QPushButton*>()) {if(button->text()=="Remove / return contents") {remove=button;break;}}QVERIFY(remove);remove->click();QCOMPARE(dialog.editSession()->draft(),beforeFailed);
 }

};
QTEST_MAIN(TstContainerSettingsWorkflow)
#include "tst_container_settings_workflow.moc"
