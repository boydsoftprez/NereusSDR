// no-port-check: NereusSDR-original actual native inert draft preview regressions.
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QDir>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QCheckBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QLayout>
#include <QPushButton>
#include <QScopeGuard>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include "core/RadioDiscovery.h"
#include "core/RadioStatus.h"
#include "gui/MainWindow.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/ClockItem.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/AppSettings.h"
#include "core/UnbuiltFeatureList.h"
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
namespace {
// Observe actual glyph ink rather than duplicating private button-grid geometry.
class ButtonTextPaintDevice : public QPaintDevice {
public:
    struct Ink { QString label; QRectF bounds; };
    class Engine : public QPaintEngine {
    public:
        QVector<Ink> inks;
        Engine() : QPaintEngine(AllFeatures) {}
        bool begin(QPaintDevice* device) override { setPaintDevice(device); setActive(true); return true; }
        bool end() override { setActive(false); return true; }
        void updateState(const QPaintEngineState&) override {}
        Type type() const override { return User; }
        void drawPixmap(const QRectF&, const QPixmap&, const QRectF&) override {}
        void drawPath(const QPainterPath&) override {}
        void drawPolygon(const QPointF*, int, PolygonDrawMode) override {}
        void drawTextItem(const QPointF& point, const QTextItem& text) override {
            QPainterPath path; path.addText(point, text.font(), text.text());
            inks.append({text.text(), state->transform().mapRect(path.boundingRect())});
        }
    };
    explicit ButtonTextPaintDevice(QSize size) : m_size(size) {}
    QPaintEngine* paintEngine() const override { return &m_engine; }
    const QVector<Ink>& inks() const { return m_engine.inks; }
protected:
    int metric(PaintDeviceMetric key) const override {
        switch (key) {
        case PdmWidth: return m_size.width();
        case PdmHeight: return m_size.height();
        case PdmDpiX: case PdmDpiY: case PdmPhysicalDpiX: case PdmPhysicalDpiY: return 96;
        case PdmDepth: return 32;
        case PdmDevicePixelRatio: return 1;
        case PdmDevicePixelRatioScaled: return devicePixelRatioFScale();
        default: return QPaintDevice::metric(key);
        }
    }
private:
    QSize m_size;
    mutable Engine m_engine;
};
}
class TstContainerPreview : public QObject {
    Q_OBJECT
    static ContentEntry rxFace(ContainerContentRegistry& registry) {
        auto entry=registry.makeEntry("meter.signal"); auto config=entry.config.value("properties").toObject();
        config["attack"]=1; config["decay"]=1; config["ignoreHistoryMs"]=0; entry.config["properties"]=config; return entry;
    }
private slots:
    void stackButtonInkStaysInsideMiddleSharedRow_data() {
        QTest::addColumn<QString>("type"); QTest::addColumn<uint>("bits");
        QTest::addColumn<int>("columns"); QTest::addColumn<int>("width");
        for (int width : {1280,1920}) {
            QTest::newRow(qPrintable("other-one-"+QString::number(width))) << QString("OTHERBTNS") << uint(16) << 2 << width;
            QTest::newRow(qPrintable("other-two-"+QString::number(width))) << QString("OTHERBTNS") << uint(24) << 2 << width;
            QTest::newRow(qPrintable("mode-one-"+QString::number(width))) << QString("MODEBTNS") << uint(1) << 2 << width;
            QTest::newRow(qPrintable("mode-two-"+QString::number(width))) << QString("MODEBTNS") << uint(3) << 2 << width;
            QTest::newRow(qPrintable("recognized-single-"+QString::number(width))) << QString("OTHERBTNS") << uint(16) << 1 << width;
        }
    }
    void stackButtonInkStaysInsideMiddleSharedRow() {
        QFETCH(QString,type); QFETCH(uint,bits); QFETCH(int,columns); QFETCH(int,width);
        ContainerContentRegistry registry; auto entry=registry.makeEntry(type);
        QStringList fields=entry.config["legacyRecord"].toString().split('|');
        const int visibilityField=type=="OTHERBTNS" ? 8 : 9; QVERIFY(fields.size()>visibilityField);
        fields[7]=QString::number(columns); fields[visibilityField]=QString::number(bits);
        entry.config["legacyRecord"]=fields.join('|');
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr,ContentRenderMode::Live));
        auto* box=qobject_cast<ButtonBoxItem*>(item.get()); QVERIFY(box);
        box->setProperty("containerStackGrid",true);
        // A real shared surface normalizes its middle 44-pixel row within 132 pixels.
        box->setRect(0,44.f/132.f,1,44.f/132.f);
        const QRectF entryRect(0,44,width,44);
        ButtonTextPaintDevice device(QSize(width,132)); QPainter textPainter(&device);
        box->paint(textPainter,width,132); textPainter.end();
        QList<int> shown;
        for (int i=0;i<box->buttonCount();++i) { if (box->isButtonShown(i)) { shown.append(i); } }
        QCOMPARE(device.inks().size(),shown.size()); QSignalSpy commands(box,&ButtonBoxItem::buttonClicked);
        for (int i=0;i<shown.size();++i) {
            const auto& ink=device.inks()[i]; QVERIFY(entryRect.contains(ink.bounds));
            const QPointF point=ink.bounds.center();
            QMouseEvent press(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QVERIFY(box->handleMousePress(&press,width,132)); QVERIFY(box->handleMouseRelease(&release,width,132));
            QCOMPARE(commands.count(),i+1); QCOMPARE(commands.last().first().toInt(),shown[i]);
        }
        QImage image(width,132,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
        QPainter painter(&image);
        // Intersect the row with the caller's clip and restore that clip for later items.
        const QRect callerClip(0,0,width/2,132); painter.setClipRect(callerClip);
        const QRegion originalClip=painter.clipRegion(); const QRectF originalClipBounds=painter.clipBoundingRect();
        const bool originallyClipped=painter.hasClipping(); box->paint(painter,width,132);
        QCOMPARE(painter.hasClipping(),originallyClipped); QCOMPARE(painter.clipBoundingRect(),originalClipBounds);
        QCOMPARE(painter.clipRegion(),originalClip);
        painter.fillRect(QRect(4,100,8,8),Qt::cyan); painter.end();
        QCOMPARE(image.pixelColor(7,103),QColor(Qt::cyan));
        int leakedPixels=0;
        for (int y=0;y<image.height();++y) {
            for (int x=0;x<image.width();++x) {
                const bool sentinel=x>=4 && x<12 && y>=100 && y<108;
                if (!sentinel && (y<44 || y>=88 || !callerClip.contains(x,y))) {
                    leakedPixels+=int(image.pixelColor(x,y).alpha()!=0);
                }
            }
        }
        QCOMPARE(leakedPixels,0);
    }
    void primitiveStackPreviewUsesLiveRowHeight_data() {
        QTest::addColumn<QString>("type");
        for (const QString& type : QStringList{"CLOCK", "VFO", "TEXT", "HISTORY", "IMAGE"}) {
            QTest::newRow(qPrintable(type)) << type;
        }
    }
    void primitiveStackPreviewUsesLiveRowHeight() {
        QFETCH(QString,type);
        ContainerContentRegistry registry; MeterPoller poller;
        auto entry=registry.makeEntry(type); entry.canvasRect=QRectF(.1234567890123,.25,.5678901234567,.125);
        QTemporaryDir imageDirectory;
        if (type=="IMAGE") {
            QVERIFY(imageDirectory.isValid());
            QImage image(4,4,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::blue);
            const QString path=imageDirectory.filePath("fixture.png"); QVERIFY(image.save(path));
            QStringList fields=entry.config["legacyRecord"].toString().split('|'); QVERIFY(fields.size()>7);
            fields[7]=path; entry.config["legacyRecord"]=fields.join('|');
        }
        entry.config["future"]=QJsonObject{{"keep",true}}; entry.extensions["opaque"]=QJsonObject{{"keep",true}};
        ContainerDocument document; document.id="primitive-parity"; document.layout=ContentLayout::VerticalStack; document.contents={entry};
        ContainerPreviewWidget preview(registry,poller); preview.setDocument(document); preview.resize(640,300); preview.show();
        ContainerContentHost live(registry); live.reconcile(document); live.resize(640,300); live.show(); QCoreApplication::processEvents();
        QCOMPARE(preview.entryBoundary(entry.id).height(),live.entryBoundary(entry.id).height());
        const auto captured=live.captureDocument().contents.first();
        QCOMPARE(captured.canvasRect,entry.canvasRect); QCOMPARE(captured.config["legacyRecord"],entry.config["legacyRecord"]);
        QCOMPARE(captured.config["future"],entry.config["future"]); QCOMPARE(captured.extensions,entry.extensions);
        QCOMPARE(preview.document(),document); QCOMPARE(poller.targetCountForTest(),0);
    }
    void emptyOrInvalidGroupedStackKeepsRecoverableData_data() {
        QTest::addColumn<QString>("raw");
        QTest::newRow("empty-mask") << QString("OTHERBTNS|.1|.2|.3|.4|0|0|2|0|future-tail");
        QTest::newRow("invalid-columns") << QString("OTHERBTNS|.1|.2|.3|.4|0|0|0|1052|future-tail");
        QTest::newRow("extreme-columns") << QString("OTHERBTNS|.1|.2|.3|.4|0|0|2147483647|1052|future-tail");
    }
    void emptyOrInvalidGroupedStackKeepsRecoverableData() {
        QFETCH(QString,raw); ContainerContentRegistry registry; MeterPoller poller;
        auto entry=registry.makeEntry("OTHERBTNS"); entry.config["legacyRecord"]=raw;
        ContainerDocument document; document.id="safe-group"; document.layout=ContentLayout::VerticalStack; document.contents={entry};
        ContainerPreviewWidget preview(registry,poller); preview.setDocument(document); preview.resize(640,300); preview.show();
        ContainerContentHost live(registry); live.reconcile(document); live.resize(640,300); live.show(); QCoreApplication::processEvents();
        QVERIFY(preview.entryBoundary(entry.id).height()>0); QVERIFY(live.entryBoundary(entry.id).height()>0);
        QVERIFY(preview.width()<4096); QVERIFY(live.meterSurfaces().first()->minimumWidth()<4096);
        QCOMPARE(preview.document(),document); QCOMPARE(live.captureDocument().contents.first().config["legacyRecord"].toString(),raw);
    }
    void groupedStackControlsKeepLabelsAndHitsInsideTheirRows_data() {
        QTest::addColumn<QString>("type"); QTest::addColumn<int>("width");
        for (const QString& type : QStringList{"OTHERBTNS", "BANDBTNS", "MODEBTNS", "FILTERBTNS", "ANTENNABTNS", "TUNESTEPBTNS", "VOICERECPLAY"}) {
            for (int width : {160, 1280}) {
                QTest::newRow(qPrintable(type + QString::number(width))) << type << width;
            }
        }
    }
    void groupedStackControlsKeepLabelsAndHitsInsideTheirRows() {
        QFETCH(QString,type); QFETCH(int,width);
        if (type=="VOICERECPLAY") { UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::Voice,true); }
        const auto restoreFeatures=qScopeGuard([] { UnbuiltFeatures::resetForTest(); });
        ContainerContentRegistry registry; MeterPoller poller;
        auto entry=registry.makeEntry(type);
        QStringList fields=entry.config["legacyRecord"].toString().split('|');
        const int visibilityField=type=="VOICERECPLAY" ? -1 : (type=="OTHERBTNS" || type=="ANTENNABTNS" ? 8 : 9);
        QVERIFY(fields.size()>qMax(7,visibilityField));
        fields[7]="2";
        if (type=="OTHERBTNS") { fields[8]="1052"; }
        else if (type=="ANTENNABTNS") { fields[8]="7"; }
        else if (type!="VOICERECPLAY") { fields[9]="7"; }
        fields.append("future-tail"); entry.config["legacyRecord"]=fields.join('|');
        entry.canvasRect=QRectF(.1234567890123,.2345678901234,.5678901234567,.3456789012345);
        entry.extensions["opaque"]=QJsonObject{{"keep",true}}; entry.config["future"]=QJsonObject{{"keep",true}};
        ContainerDocument document; document.id="grouped-stack"; document.layout=ContentLayout::VerticalStack;
        document.contents={entry,registry.makeEntry("TEXT")};
        QScrollArea viewport; viewport.setWidgetResizable(true);
        auto* preview=new ContainerPreviewWidget(registry,poller); viewport.setWidget(preview);
        preview->setDocument(document); viewport.resize(width,300); viewport.show();
        ContainerContentHost live(registry); live.reconcile(document); live.resize(width,300); live.show();
        QCoreApplication::processEvents();
        auto* previewMeter=preview->findChildren<MeterWidget*>().first();
        auto* previewBox=qobject_cast<ButtonBoxItem*>(previewMeter->items().first()); QVERIFY(previewBox);
        auto* liveBox=qobject_cast<ButtonBoxItem*>(live.entryRows().first().item.data()); QVERIFY(liveBox);
        const int rows=type=="VOICERECPLAY" ? 3 : 2;
        auto* liveMeter=qobject_cast<MeterWidget*>(live.entryRows().first().widget.data()); QVERIFY(liveMeter);
        QCOMPARE(previewBox->visibleBits(),liveBox->visibleBits());
        const QString capture=qEnvironmentVariable("PREVIEW_STACK_CAPTURE_DIR");
        if (!capture.isEmpty()) {
            const QString path=capture+"/"+QString::fromLatin1(QTest::currentDataTag()); QVERIFY(QDir().mkpath(path));
            QImage painted(previewMeter->size(),QImage::Format_ARGB32_Premultiplied); painted.fill(QColor("#0f0f1a"));
            QPainter painter(&painted); previewBox->paint(painter,previewMeter->width(),previewMeter->height()); painter.end();
            QVERIFY(painted.save(path+"/group-paint-before-assertions.png"));
        }
        const QRect liveBoundary=live.entryBoundary(entry.id);
        QVERIFY(live.entryBoundary(document.contents[1].id).top()>=liveBoundary.bottom());
        for (const auto& pair : {QPair<ButtonBoxItem*,MeterWidget*>{previewBox,previewMeter},{liveBox,liveMeter}}) {
            auto* box=pair.first; auto* meter=pair.second;
            ButtonTextPaintDevice device(meter->size()); QPainter painter(&device);
            box->paint(painter,meter->width(),meter->height()); painter.end();
            QList<int> shown;
            for (int i=0;i<box->buttonCount();++i) { if (box->isButtonShown(i)) { shown.append(i); } }
            QCOMPARE(device.inks().size(),shown.size());
            const int rowHeight=box==previewBox ? previewMeter->height() : live.entryBoundary(entry.id).height();
            const QRectF row(0,0,meter->width(),rowHeight);
            QSignalSpy commands(box,&ButtonBoxItem::buttonClicked);
            for (int i=0;i<shown.size();++i) {
                const auto& ink=device.inks()[i]; QCOMPARE(ink.label,box->button(shown[i]).text);
                QVERIFY2(row.contains(ink.bounds),qPrintable(QString("%1 ink outside entry: %2,%3 %4x%5").arg(ink.label).arg(ink.bounds.x()).arg(ink.bounds.y()).arg(ink.bounds.width()).arg(ink.bounds.height())));
                const QPointF point(ink.bounds.center());
                QMouseEvent press(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QVERIFY(box->handleMousePress(&press,meter->width(),meter->height()));
                QVERIFY(box->handleMouseRelease(&release,meter->width(),meter->height()));
                if (box==liveBox) { QCOMPARE(commands.count(),i+1); QCOMPARE(commands.last().first().toInt(),shown[i]); }
                else { QCOMPARE(commands.count(),0); }
            }
        }
        QCOMPARE(preview->entryBoundary(entry.id).top(),0);
        QCOMPARE(preview->entryBoundary(entry.id).height(),44*rows);
        QCOMPARE(live.entryBoundary(entry.id).height(),44*rows);
        QVERIFY(previewMeter->minimumWidth()>=128); QVERIFY(liveMeter->minimumWidth()>=128);
        QCOMPARE(preview->document(),document); QCOMPARE(poller.targetCountForTest(),0);
        const auto captured=live.captureDocument().contents.first();
        QCOMPARE(captured.canvasRect,entry.canvasRect); QCOMPARE(captured.config["legacyRecord"],entry.config["legacyRecord"]);
        QCOMPARE(captured.config["future"],entry.config["future"]); QCOMPARE(captured.extensions,entry.extensions);
        if (!capture.isEmpty()) {
            const QString path=capture+"/"+QString::fromLatin1(QTest::currentDataTag()); QVERIFY(QDir().mkpath(path));
#ifdef NEREUS_GPU_SPECTRUM
            QSignalSpy previewFrames(previewMeter,&QRhiWidget::frameSubmitted),liveFrames(liveMeter,&QRhiWidget::frameSubmitted);
            previewMeter->update(); liveMeter->update();
            QTRY_VERIFY_WITH_TIMEOUT(previewFrames.count()>0 && liveFrames.count()>0,3000);
            QVERIFY(previewMeter->grabFramebuffer().save(path+"/group-preview.png"));
            QVERIFY(liveMeter->grabFramebuffer().save(path+"/group-live.png"));
#else
            QVERIFY(previewMeter->grab().save(path+"/group-preview.png")); QVERIFY(liveMeter->grab().save(path+"/group-live.png"));
#endif
            QVERIFY(viewport.grab().save(path+"/viewport.png"));
        }
    }
    void nativeFreeLeafCompositionPreflight() {
        ContainerContentRegistry registry;
        QWidget root;
        root.resize(800,520);
        auto a=registry.makeEntry("meter.ananMulti");
        auto b=registry.makeEntry("meter.sMeter");
        a.context["sliceId"]=0; b.context["sliceId"]=1;
        auto* first=qobject_cast<MeterWidget*>(registry.createPreview(a,&root));
        auto* second=qobject_cast<MeterWidget*>(registry.createPreview(b,&root));
        QVERIFY(first && second);
        for(auto* meter:{first,second}) {meter->setAttribute(Qt::WA_TransparentForMouseEvents,false);for(auto* item:meter->items()) {item->clearStackMetadata();item->setRect(0,0,1,1);}}
        first->setGeometry(20,20,560,280); second->setGeometry(260,160,320,160);
        auto* native=new QLabel("Borrowed native QWidget fixture",&root);native->setStyleSheet("background:#203040;color:white;");native->setGeometry(480,300,280,100);
        second->raise();native->raise();root.show();
#ifdef NEREUS_GPU_SPECTRUM
        QSignalSpy framesA(first,&QRhiWidget::frameSubmitted),framesB(second,&QRhiWidget::frameSubmitted);
        first->update();second->update();
        QTRY_VERIFY_WITH_TIMEOUT(framesA.count()>0 && framesB.count()>0,4000);
        const QImage faceA=first->grabFramebuffer(),faceB=second->grabFramebuffer();
#else
        const QImage faceA=first->grab().toImage(),faceB=second->grab().toImage();
#endif
        QVERIFY(!faceA.isNull() && !faceB.isNull());
        QCOMPARE(root.childAt(300,210),static_cast<QWidget*>(second));
        QCOMPARE(root.childAt(500,330),static_cast<QWidget*>(native));
        const QString capture=qEnvironmentVariable("CANVAS_CAPTURE_DIR");
        if(!capture.isEmpty()) {QVERIFY(QDir().mkpath(capture));QVERIFY(faceA.save(capture+"/preflight-anan.png"));QVERIFY(faceB.save(capture+"/preflight-s.png"));QVERIFY(root.grab().save(capture+"/preflight-overlap.png"));}
    }
    void freeCanvasKeepsNativeLeafViewportsAndScrolls()
    {
        ContainerContentRegistry registry; MeterPoller poller;
        ContainerDocument d; d.id="free-native"; d.layout=static_cast<ContentLayout>(2);
        auto a=registry.makeEntry("meter.signalText"),b=registry.makeEntry("meter.sMeter");
        a.context["sliceId"]=0; b.context["sliceId"]=1;
        a.extensions["freeCanvasRect"]=QJsonArray{20.125,30.25,460.5,180.75};
        b.extensions["freeCanvasRect"]=QJsonArray{270.25,330.5,320.75,160.5};
        d.contents={a,b};
        QScrollArea viewport; viewport.setWidgetResizable(true);
        auto* preview=new ContainerPreviewWidget(registry,poller); viewport.setWidget(preview);
        preview->setDocument(d); viewport.resize(300,220);viewport.show();
        auto meters=preview->findChildren<MeterWidget*>();QCOMPARE(meters.size(),2);
        if(meters[0]->property("freeCanvasEntryId").toString()!=a.id) {std::swap(meters[0],meters[1]);}
        QTRY_VERIFY(viewport.verticalScrollBar()->maximum()>0);
        QVERIFY(qAbs(meters[0]->width()-460.5)<=1);QVERIFY(qAbs(meters[0]->height()-180.75)<=1);
        QVERIFY(qAbs(meters[1]->height()-160.5)<=1);
        const auto before=meters[0]->geometry();viewport.resize(220,180);QCoreApplication::processEvents();
        QCOMPARE(meters[0]->geometry(),before);
        QCOMPARE(preview->document(),d);QCOMPARE(poller.targetCountForTest(),0);
    }
    void freeCanvasGripResizeEscapeAndLock()
    {
        ContainerContentRegistry registry;MeterPoller poller;ContainerPreviewWidget preview(registry,poller);
        ContainerDocument d;d.id="gestures";d.layout=static_cast<ContentLayout>(2);
        auto a=registry.makeEntry("meter.signal"),b=registry.makeEntry("meter.sMeter");
        const QJsonArray exact{10.1234567890123,20.9876543210987,460.567890123456,180.123456789012};
        a.extensions["freeCanvasRect"]=exact;b.extensions["freeCanvasRect"]=QJsonArray{30.,260.,320.,160.};d.contents={a,b};
        preview.setDocument(d);preview.resize(800,600);preview.show();QCoreApplication::processEvents();
        auto* grip=preview.findChild<QWidget*>("freeCanvasGrip_"+a.id);QVERIFY(grip);
        auto* corner=preview.findChild<QWidget*>("freeCanvasResize_"+a.id);QVERIFY(corner);
        auto* meter=preview.findChildren<MeterWidget*>().first();QPointer<MeterWidget> stable=meter;
        auto drag=[](QWidget* handle,QPoint delta,bool cancel=false) {
            const QPoint start=handle->rect().center();const QPoint global=handle->mapToGlobal(start);
            QTest::mousePress(handle,Qt::LeftButton,Qt::NoModifier,start);
            QMouseEvent move(QEvent::MouseMove,start+delta,global+delta,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(handle,&move);
            if(cancel) {QTest::keyClick(handle,Qt::Key_Escape);}
            else {QTest::mouseRelease(handle,Qt::LeftButton,Qt::NoModifier,start+delta);}
        };
        drag(grip,{37,19});
        auto rect=preview.document().contents[0].extensions["freeCanvasRect"].toArray();
        QCOMPARE(rect[0].toDouble(),exact[0].toDouble()+37);QCOMPARE(rect[1].toDouble(),exact[1].toDouble()+19);
        QCOMPARE(rect[2],exact[2]);QCOMPARE(rect[3],exact[3]);QCOMPARE(preview.document().contents[1],b);
        QCOMPARE(preview.findChildren<MeterWidget*>().first(),stable.data());
        const auto moved=preview.document();drag(corner,{43,29});
        rect=preview.document().contents[0].extensions["freeCanvasRect"].toArray();
        QCOMPARE(rect[0],moved.contents[0].extensions["freeCanvasRect"].toArray()[0]);
        QCOMPARE(rect[2].toDouble(),exact[2].toDouble()+43);QCOMPARE(rect[3].toDouble(),exact[3].toDouble()+29);
        const auto resized=preview.document();drag(grip,{-300,12},true);QCOMPARE(preview.document(),resized);
        auto locked=resized;locked.locked=true;preview.setDocument(locked);drag(grip,{20,20});drag(corner,{20,20});QCOMPARE(preview.document(),locked);
        QCOMPARE(poller.targetCountForTest(),0);
    }
    void largeSignalFontFitsTitleReadingAndPeak() {
        ContainerContentRegistry registry; auto entry=registry.makeEntry("meter.signalText");
        auto properties=entry.config.value("properties").toObject(); properties["fontSize"]=56; properties["faceHeight"]=120; entry.config["properties"]=properties;
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr)); auto* face=qobject_cast<CompositePresetItem*>(item.get()); QVERIFY(face);
        QFont font; font.setPixelSize(56); QFont peakFont; peakFont.setPixelSize(14);
        const int textHeight=2*QFontMetrics(font).height()+QFontMetrics(peakFont).height();
        qInfo()<<"STACK font-aware minimum"<<textHeight<<"preferred"<<face->preferredFaceHeight();
        QVERIFY2(face->preferredFaceHeight()>=textHeight,"Supported Signal font must have distinct full-height title, reading and peak lines");
        QVERIFY2(face->minimumFaceSize().width()>=QFontMetrics(font).horizontalAdvance("-140.0 dBm")+16,"The chosen Signal font must fit the supported reading at minimum width");
        QCOMPARE(face->configuration()["fontSize"].toInt(),56); QCOMPARE(face->configuration()["faceHeight"].toInt(),120);
    }
    void verticalStackKeepsConfiguredRowsInSmallViewport_data() {
        QTest::addColumn<QStringList>("types"); QTest::addColumn<int>("width");
        const QStringList bars{"meter.signalText","meter.comp","meter.eq","meter.leveler","meter.cfc","meter.cfcGain","meter.alcGain"};
        const QStringList mixed{"meter.signalText","meter.historyGraph","meter.clock","meter.sMeter","meter.vfoDisplay","meter.contest"};
        QTest::newRow("bars-640")<<bars<<640; QTest::newRow("bars-360")<<bars<<360; QTest::newRow("bars-260")<<bars<<260;
        QTest::newRow("mixed-640")<<mixed<<640; QTest::newRow("mixed-360")<<mixed<<360;
    }
    void verticalStackKeepsConfiguredRowsInSmallViewport() {
        QFETCH(QStringList,types); QFETCH(int,width);
        ContainerContentRegistry registry; MeterPoller poller; int sourceReads=0;
        poller.setRxReadingSource([&](const QJsonObject&,int){ ++sourceReads; return -73.0; });
        ContainerDocument d; d.id="stack-regression"; d.layout=ContentLayout::VerticalStack; d.autoHeight=true;
        for(const QString& type : types) {
            auto entry=registry.makeEntry(type); auto properties=entry.config.value("properties").toObject();
            if(type=="meter.signalText") { properties["fontSize"]=56; properties["faceHeight"]=120; }
            else if(types.size()==7 || type=="meter.sMeter") { properties["rowHeight"]=72; }
            properties["futureGeometryNote"]="retained"; entry.config["properties"]=properties;
            std::unique_ptr<MeterItem> imported(registry.createMeterItem(entry,nullptr,ContentRenderMode::Validation)); QVERIFY(imported);
            entry.config["legacyRecord"]=imported->serialize(); entry.extensions["futurePlacement"]=QJsonObject{{"x",999}};
            entry.canvasRect=QRectF(.12,.23,.67,.19); d.contents.append(entry);
        }
        QScrollArea viewport; viewport.setWidgetResizable(true); auto* preview=new ContainerPreviewWidget(registry,poller); viewport.setWidget(preview);
        connect(preview,&ContainerPreviewWidget::presentationRequested,preview,[](MeterWidget* meter,const QJsonObject&) {
            for(auto* item:meter->items()) { if(auto* face=qobject_cast<CompositePresetItem*>(item)) { face->setFrequency(14225000); face->setModeLabel("USB"); face->setBandLabel("20m"); face->setUnavailableText({}); } }
        });
        preview->setDocument(d); poller.frameAdvanced(100); const int setupReads=sourceReads; viewport.resize(width,260); viewport.show(); QTest::qWait(150);
        const auto meters=preview->findChildren<MeterWidget*>(); QCOMPARE(meters.size(),types.size());
        const QString baseCapture=qEnvironmentVariable("PREVIEW_STACK_CAPTURE_DIR");
        const QString capture=baseCapture.isEmpty()?QString():baseCapture+"/"+QString::fromLatin1(QTest::currentDataTag());
        if(!capture.isEmpty()) { QVERIFY(QDir().mkpath(capture)); QVERIFY(viewport.grab().save(capture+"/short-top.png")); }
        for(auto* meter:meters) { qInfo()<<"STACK row"<<meter->geometry()<<"item"<<meter->items()[0]->x()<<meter->items()[0]->y()<<meter->items()[0]->itemWidth()<<meter->items()[0]->itemHeight(); }
        qInfo()<<"STACK preview"<<preview->size()<<"minimum"<<preview->minimumSize()<<"scroll"<<viewport.verticalScrollBar()->maximum();
        QVERIFY(viewport.verticalScrollBar()->maximum()>0);
        if(width==260) { QVERIFY(viewport.horizontalScrollBar()->maximum()>0); }
        QVERIFY2(preview->height()>=552,"The viewport must scroll the full configured stack instead of clipping rows");
        QCOMPARE(preview->document(),d); QCOMPARE(poller.targetCountForTest(),0);
        for(int i=1;i<meters.size();++i) { QVERIFY(meters[i]->geometry().top()>=meters[i-1]->geometry().bottom()); }
        ContainerContentHost live(registry); live.reconcile(d); live.resize(width,preview->height()+8); live.show(); QTest::qWait(100);
        QCOMPARE(live.meterSurfaces().size(),1);
        const auto captured=live.captureDocument();
        for(int i=0;i<d.contents.size();++i) {
            QCOMPARE(captured.contents[i].canvasRect,d.contents[i].canvasRect);
            QCOMPARE(captured.contents[i].extensions,d.contents[i].extensions);
            QCOMPARE(captured.contents[i].config["legacyRecord"],d.contents[i].config["legacyRecord"]);
            QCOMPARE(captured.contents[i].config["properties"].toObject()["futureGeometryNote"],QJsonValue("retained"));
            for(const QString& key:{QString("fontSize"),QString("faceHeight"),QString("rowHeight")}) {
                if(d.contents[i].config["properties"].toObject().contains(key)) { QCOMPARE(captured.contents[i].config["properties"].toObject()[key],d.contents[i].config["properties"].toObject()[key]); }
            }
        }
        auto* liveViewport = live.findChild<QScrollArea*>();
        QVERIFY(liveViewport);
        QVERIFY(liveViewport->widget()->layout());
        const QMargins liveMargins = liveViewport->widget()->layout()->contentsMargins();
        const int liveUsableWidth = liveViewport->viewport()->width() - liveMargins.left() - liveMargins.right();
        for(int i=0;i<live.entryRows().size();++i) {
            const auto& row=live.entryRows()[i]; const QRect boundary=live.entryBoundary(row.entryId);
            qInfo()<<"STACK live"<<boundary<<"usable viewport width"<<liveUsableWidth;
            if (auto* bar = qobject_cast<BarPresetItem*>(row.item.data())) {
                // The draft preview keeps configured row heights. Approved live
                // auto-height bars instead use natural width-derived allocation;
                // both retain the same document and renderer calibration.
                QCOMPARE(meters[i]->height(), bar->preferredRowHeight());
                const int naturalHeight = qRound(double(bar->preferredRowHeight()) * liveUsableWidth / 260);
                QVERIFY(qAbs(boundary.height() - naturalHeight) <= 1); // cumulative rounding across rows
            } else {
                QVERIFY(qAbs(boundary.height()-meters[i]->height())<=1);
            }
            if(i>0) { QVERIFY(boundary.top()>=live.entryBoundary(live.entryRows()[i-1].entryId).bottom()); }
            for(int binding:row.item->readingBindings()) { row.item->pushBindingValue(binding,-73); } row.item->advanceMeter(100);
            if(auto* face=qobject_cast<CompositePresetItem*>(row.item.data())) { face->setFrequency(14225000); face->setModeLabel("USB"); face->setBandLabel("20m"); face->setUnavailableText({}); }
        }
        viewport.verticalScrollBar()->setValue(viewport.verticalScrollBar()->maximum()); QTest::qWait(100);
        QVERIFY(viewport.viewport()->rect().intersects(QRect(meters.last()->mapTo(viewport.viewport(),QPoint()),meters.last()->size())));
        if(!capture.isEmpty()) { QVERIFY(viewport.grab().save(capture+"/short-bottom.png")); }
        viewport.resize(width,preview->minimumHeight()+2); viewport.verticalScrollBar()->setValue(0); QTest::qWait(150);
        for(int i=0;i<meters.size();++i) {
#ifdef NEREUS_GPU_SPECTRUM
            QSignalSpy frames(meters[i],&QRhiWidget::frameSubmitted); meters[i]->update(); QTRY_VERIFY_WITH_TIMEOUT(frames.count()>0,3000);
            const QImage image=meters[i]->grabFramebuffer();
#else
            const QImage image=meters[i]->grab().toImage();
#endif
            QVERIFY(!image.isNull()); QVERIFY(meters[i]->items()[0]->signalsBlocked());
            if(auto* face=qobject_cast<CompositePresetItem*>(meters[i]->items()[0])) { for(auto* child:face->internalItems()) { QVERIFY(child->signalsBlocked()); } }
            if(types.size()==6 && i==4) {
                int clippedFrequencyPixels=0; const qreal scale=qreal(image.width())/meters[i]->width();
                for(int y=4;y<int(image.height()*.6);++y) { for(int x=image.width()-qRound(9*scale);x<image.width()-qRound(6*scale);++x) { const QColor c=image.pixelColor(x,y); clippedFrequencyPixels+=int(c.red()>180 && c.green()>70 && c.green()<230 && c.blue()<50); } }
                qInfo()<<"STACK VFO frequency pixels at inner right edge"<<clippedFrequencyPixels;
                QCOMPARE(clippedFrequencyPixels,0);
            }
            if(!capture.isEmpty()) { QVERIFY(image.save(capture+QString("/preview-row-%1.png").arg(i))); }
        }
#ifdef NEREUS_GPU_SPECTRUM
        auto* surface=live.meterSurfaces().first(); QSignalSpy frames(surface,&QRhiWidget::frameSubmitted); surface->update(); QTRY_VERIFY_WITH_TIMEOUT(frames.count()>0,3000);
        const QImage liveImage=surface->grabFramebuffer();
#else
        const QImage liveImage=live.meterSurfaces().first()->grab().toImage();
#endif
        if(!capture.isEmpty()) { QVERIFY(preview->grab().save(capture+"/full-preview.png")); QVERIFY(liveImage.save(capture+"/live-run.png")); }
        viewport.resize(width+80,260); QTest::qWait(100);
        QCOMPARE(meters[0]->height(),qobject_cast<CompositePresetItem*>(meters[0]->items()[0])->preferredFaceHeight());
        QCOMPARE(preview->document(),d); QCOMPARE(poller.targetCountForTest(),0); QCOMPARE(sourceReads,setupReads);
        d.layout=ContentLayout::LegacyCanvas; preview->setDocument(d); const auto legacy=preview->findChild<MeterWidget*>()->items();
        for(auto* item:legacy) { QVERIFY(qAbs(item->x()-.12f)<1e-6); QVERIFY(qAbs(item->y()-.23f)<1e-6); QVERIFY(qAbs(item->itemHeight()-.19f)<1e-6); }
    }
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
            for(auto* button:dialog.findChildren<QPushButton*>()) { if(button->text()=="Remove / return contents") { remove=button; break; } }
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
