// no-port-check: Observe native Qt paint output independently of product geometry.
#include <QtTest>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPaintEngine>
#include <QPaintDevice>
#include <QDir>
#include <QWidget>
#include <QMouseEvent>
#include <limits>
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/HistoryGraphItem.h"
#include "gui/meters/TextOverlayItem.h"
#include "gui/meters/DialItem.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/SignalTextItem.h"
#include "gui/meters/NeedleScalePwrItem.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/containers/ContainerContentRegistry.h"
using namespace NereusSDR;
namespace {
// Observe actual QPainter text output at the paint-device boundary. This avoids
// using the renderer's private layout arithmetic as the readability oracle.
class TextPaintEngine : public QPaintEngine {
public:
    struct Text { QString value; double pixels; QRectF bounds; QFont font; };
    QVector<Text> texts;
    QVector<QPainterPath> strokes;
    QVector<QImage> images;
    explicit TextPaintEngine(int dpr):QPaintEngine(AllFeatures),m_dpr(dpr) {}
    bool begin(QPaintDevice* device) override { setPaintDevice(device); setActive(true); return true; }
    bool end() override { setActive(false); return true; }
    void updateState(const QPaintEngineState&) override {}
    Type type() const override { return User; }
    void drawPixmap(const QRectF&,const QPixmap&,const QRectF&) override {}
    void drawImage(const QRectF&,const QImage& image,const QRectF&,Qt::ImageConversionFlags) override {images.append(image);}
    void drawPath(const QPainterPath& path) override {recordStroke(path);}
    void drawLines(const QLineF* lines,int count) override {
        for(int i=0;i<count;++i) {QPainterPath path;path.moveTo(lines[i].p1());path.lineTo(lines[i].p2());recordStroke(path);}
    }
    void drawPolygon(const QPointF*,int,PolygonDrawMode) override {}
    void drawTextItem(const QPointF& point,const QTextItem& item) override {
        QPainterPath ink; ink.addText(point,item.font(),item.text());
        const QRectF inkMapped=state->transform().mapRect(ink.boundingRect());
        texts.append({item.text(),QFontMetricsF(item.font()).height(),QRectF(inkMapped.topLeft()/m_dpr,inkMapped.size()/m_dpr),item.font()});
    }
private:
    void recordStroke(const QPainterPath& path) {
        if(state->pen().style()==Qt::NoPen) {return;}
        QPainterPathStroker stroker;stroker.setWidth(qMax(1.0,state->pen().widthF()));
        QTransform normalise;normalise.scale(1./m_dpr,1./m_dpr);
        strokes.append(normalise.map(state->transform().map(stroker.createStroke(path))));
    }
    int m_dpr;
};
class TextPaintDevice : public QPaintDevice {
public:
    TextPaintDevice(QSize size,int dpr):m_size(size),m_dpr(dpr),m_engine(dpr) {}
    QPaintEngine* paintEngine() const override { return &m_engine; }
    const QVector<TextPaintEngine::Text>& texts() const { return m_engine.texts; }
    const QVector<QPainterPath>& strokes() const { return m_engine.strokes; }
    const QVector<QImage>& images() const { return m_engine.images; }
protected:
    int metric(PaintDeviceMetric key) const override {
        switch(key) {
        case PdmWidth: return m_size.width()*m_dpr;
        case PdmHeight: return m_size.height()*m_dpr;
        case PdmDpiX: case PdmDpiY: case PdmPhysicalDpiX: case PdmPhysicalDpiY: return 96;
        case PdmDepth: return 32;
        case PdmDevicePixelRatio: return m_dpr;
        case PdmDevicePixelRatioScaled: return m_dpr*devicePixelRatioFScale();
        default: return QPaintDevice::metric(key);
        }
    }
private:
    QSize m_size;
    int m_dpr;
    mutable TextPaintEngine m_engine;
};
}
class TestResponsiveMeterFonts : public QObject {
    Q_OBJECT
private slots:
    void nonfiniteButtonCellsAreNotPainted_data() {
        QTest::addColumn<float>("ratio");
        QTest::newRow("finite-ratio-overflow") << std::numeric_limits<float>::max();
        QTest::newRow("infinite-ratio") << std::numeric_limits<float>::infinity();
        QTest::newRow("nan-ratio") << std::numeric_limits<float>::quiet_NaN();
    }
    void nonfiniteButtonCellsAreNotPainted() {
        QFETCH(float,ratio);
        BandButtonItem buttons; buttons.setColumns(1); buttons.setVisibleBits(1);
        buttons.setHeightRatio(ratio);
        const QString saved=buttons.serialize();
        TextPaintDevice device(QSize(100,100),1); QPainter painter(&device);
        buttons.paint(painter,100,100); painter.end();
        QCOMPARE(device.texts().size(),0);
        QCOMPARE(device.strokes().size(),0);
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(10,10),QPointF(10,10),
                          Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QVERIFY(!buttons.handleMousePress(&press,100,100));
        QCOMPARE(buttons.serialize(),saved);
    }
    void zeroWidthContestDoesNotPaintButtonLabels() {
        CompositePresetItem face(CompositePresetItem::Face::Contest);
        face.setRect(0,0,0,1);
        const QString saved=face.serialize();
        TextPaintDevice device(QSize(360,280),1); QPainter painter(&device);
        face.paint(painter,360,280); painter.end();
        for(const auto& text:device.texts()) {
            QVERIFY(text.value!=face.bandButtons()->button(0).text);
        }
        QCOMPARE(face.serialize(),saved);
    }
    void fractionalLegacyButtonEdgeKeepsImportedRecord() {
        ContainerContentRegistry registry;
        auto entry=registry.makeEntry("BANDBTNS");
        const QString raw="BANDBTNS|0|0|1|1|-1|0|6|0|1|future-tail";
        entry.config["legacyRecord"]=raw;
        entry.extensions["opaque"]=QJsonObject{{"keep",true}};
        entry.canvasRect=QRectF(0,0,1.0000000000000002,1);
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr));
        auto* buttons=qobject_cast<BandButtonItem*>(item.get()); QVERIFY(buttons);
        buttons->setHeightRatio(1.5f);
        const auto before=registry.captureMeterItem(*buttons,entry);
        QSignalSpy clicked(buttons,&ButtonBoxItem::buttonClicked);
        TextPaintDevice device(QSize(21,100),1); QPainter painter(&device);
        buttons->paint(painter,21,100); painter.end();
        QVERIFY(!device.strokes().isEmpty());
        // The established float cell ends at y=6.934999942779541. These
        // neighbouring positions catch promotion of its stored geometry.
        for(const auto& hit:{QPair<double,bool>{6.93499990,true},{6.93500000,false}}) {
            const QPointF point(1.5,hit.first);
            QMouseEvent press(QEvent::MouseButtonPress,point,point,
                              Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QCOMPARE(buttons->handleMousePress(&press,21,100),hit.second);
            QMouseEvent release(QEvent::MouseButtonRelease,point,point,
                                Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            buttons->handleMouseRelease(&release,21,100);
            QCOMPARE(clicked.count(),1);
        }
        const auto after=registry.captureMeterItem(*buttons,entry);
        QCOMPARE(after,before);
        QCOMPARE(after.config["legacyRecord"].toString(),raw);
        QCOMPARE(after.extensions,entry.extensions);
        QCOMPARE(after.canvasRect.width(),entry.canvasRect.width());
        QCOMPARE(buttons->heightRatio(),1.5f);
    }
    void contestPaintPreservesImportedChildren() {
        CompositePresetItem face(CompositePresetItem::Face::Contest);
        QJsonObject config=face.configuration();
        config["vfo"]="VFO|0.123|0.234|0.345|0.456|-1|0|0";
        config["future"]=QJsonObject{{"preserve",true}};
        QVERIFY(face.applyConfiguration(config));
        const QString saved=face.serialize();
        for(QSize size:{QSize(360,280),QSize(720,560),QSize(720,120),QSize(260,360)}) {
            TextPaintDevice device(size,1);QPainter painter(&device);face.paint(painter,size.width(),size.height());painter.end();
            QVERIFY(!device.texts().isEmpty());QCOMPARE(face.serialize(),saved);
        }
    }
    void nativeRolesScaleAndFit() {
        using Face=CompositePresetItem::Face;
        const QList<Face> faces{Face::PowerSwr,Face::Cross,Face::Eye,Face::History,Face::SignalText,Face::Clock,Face::Contest};
        const QList<QSize> references{{260,144},{360,260},{260,180},{260,120},{260,120},{260,120},{360,280}};
        for(int index=0;index<faces.size();++index) {
            CompositePresetItem face(faces[index]);
            QVERIFY(face.applyConfiguration({{"future",QJsonObject{{"keep",true}}}}));
            const QString saved=face.serialize();
            QVector<TextPaintEngine::Text> reference;
            for(int dpr:{1,2}) {
                for(double factor:{.5,1.,2.}) {
                    const QSize size(qRound(references[index].width()*factor),qRound(references[index].height()*factor));
                    TextPaintDevice device(size,dpr); QPainter painter(&device); face.paint(painter,size.width(),size.height()); painter.end();
                    QVERIFY2(!device.texts().isEmpty(),qPrintable(face.typeId()));
                    for(const auto& text:device.texts()) {
                        QVERIFY2(QRectF(QPointF(0,0),size).adjusted(-1,-1,1,1).contains(text.bounds),qPrintable(face.typeId()+" "+text.value+" "+QString::number(text.pixels)));
                    }
                    if(dpr==1 && factor==1.) { reference=device.texts(); }
                    if(factor==2.) {
                        for(const auto& original:reference) {
                            auto found=std::find_if(device.texts().cbegin(),device.texts().cend(),[&](const auto& text){return text.value==original.value;});
                            // Times can cross a second; titles and dates are stable.
                            if(found!=device.texts().cend() && original.pixels>0) {
                                QVERIFY2(found->pixels>=original.pixels*1.7,qPrintable(face.typeId()+" "+original.value));
                            }
                        }
                    }
                    QCOMPARE(face.serialize(),saved);
                }
            }
        }
    }
    void aspectRatiosLongTextAndDpr() {
        using Face=CompositePresetItem::Face;
        for(Face kind:{Face::PowerSwr,Face::Cross,Face::Eye,Face::History,Face::SignalText,Face::Clock,Face::Contest}) {
            CompositePresetItem face(kind);
            QJsonObject edit{{"future",QJsonObject{{"preserve",true}}}};
            if(face.editableFields().contains("title")) { edit["title"]="A long receiver title for this meter"; }
            if(face.editableFields().contains("show24Hour")) { edit["show24Hour"]=false; }
            QVERIFY(face.applyConfiguration(edit));
            for(int binding:face.readingBindings()) { face.pushBindingValue(binding,binding==1?-123.4:2.5); }
            face.advanceMeter(0);face.advanceMeter(100);
            const QString before=face.serialize();
            for(QSize size:{QSize(720,120),QSize(260,360),QSize(520,240)}) {
                QVector<TextPaintEngine::Text> dprOne;
                for(int dpr:{1,2}) {
                    TextPaintDevice device(size,dpr);QPainter painter(&device);face.paint(painter,size.width(),size.height());painter.end();
                    QVERIFY(!device.texts().isEmpty());
                    for(const auto& text:device.texts()) {
                        QVERIFY2(QRectF(QPointF(0,0),size).adjusted(-1,-1,1,1).contains(text.bounds),qPrintable(face.typeId()+" "+text.value));
                    }
                    if(dpr==1) { dprOne=device.texts(); }
                    else {
                        QCOMPARE(device.texts().size(),dprOne.size());
                        for(int i=0;i<dprOne.size();++i) { QCOMPARE(device.texts()[i].pixels,dprOne[i].pixels); QCOMPARE(device.texts()[i].bounds,dprOne[i].bounds); }
                    }
                }
                QCOMPARE(face.serialize(),before);
            }
        }
    }
    void primaryRolesDoNotOverlap() {
        using Face=CompositePresetItem::Face;
        for(Face kind:{Face::PowerSwr,Face::Cross,Face::Eye,Face::History,Face::SignalText,Face::Clock,Face::Contest}) {
            CompositePresetItem face(kind);
            for(int binding:face.readingBindings()) { face.pushBindingValue(binding,binding==1?-93.2:2.5); }
            face.advanceMeter(0);face.advanceMeter(100);
            const QSize size=kind==Face::Cross?QSize(360,260):kind==Face::Contest?QSize(360,280):kind==Face::PowerSwr?QSize(260,144):kind==Face::Eye?QSize(260,180):QSize(260,120);
            TextPaintDevice device(size,1);QPainter painter(&device);face.paint(painter,size.width(),size.height());painter.end();
            for(int i=0;i<device.texts().size();++i) {
                for(int j=i+1;j<device.texts().size();++j) {
                    QVERIFY2(!device.texts()[i].bounds.intersects(device.texts()[j].bounds),qPrintable(face.typeId()+" "+device.texts()[i].value+" overlaps "+device.texts()[j].value));
                }
            }
        }
    }
    void crossMarksAndNeedlesClearText() {
        for(QSize size:{QSize(360,260),QSize(720,520),QSize(720,120),QSize(260,360)}) {
            CompositePresetItem cross(CompositePresetItem::Face::Cross);
            TextPaintDevice background(size,1);QPainter observe(&background);cross.paintForLayer(observe,size.width(),size.height(),MeterItem::Layer::Background);observe.end();
            QStringList values;
            for(const auto& text:background.texts()) {
                bool numeric=false;text.value.toDouble(&numeric);if(numeric) {values.append(text.value);}
            }
            values.sort();
            QCOMPARE(values,QStringList({"0","0","100","20","50"}));
            for(const auto& text:background.texts()) {
                for(const QPainterPath& stroke:background.strokes()) {
                    QVERIFY2(!stroke.intersects(text.bounds),qPrintable(QString("Cross mark%1 intersects scale at%2x%3").arg(text.value).arg(size.width()).arg(size.height())));
                }
            }
            for(int binding:cross.readingBindings()) {cross.pushBindingValue(binding,1.5);}cross.advanceMeter(0);cross.advanceMeter(100);
            QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter render(&image);cross.paint(render,size.width(),size.height());render.end();
            for(const auto& text:background.texts()) {
                if(!text.value.contains(" (")) {continue;}
                const QRect box=text.bounds.toAlignedRect().intersected(image.rect());
                int black=0;
                for(int y=box.top();y<=box.bottom();++y) {for(int x=box.left();x<=box.right();++x) {const QColor c=image.pixelColor(x,y);if(c.red()<10 && c.green()<10 && c.blue()<10) {++black;}}}
                QVERIFY2(black==0,qPrintable(text.value+" is covered by a needle"));
            }
        }
    }
    void cachedBarMajorLabelTracksItsTick() {
        for(const QString& flavor:{QString("Mic"),QString("Alc"),QString("Comp"),QString("Eq"),QString("Leveler"),QString("Cfc")}) {
            BarPresetItem bar;
            if(flavor=="Mic") {bar.configureAsMic();} else if(flavor=="Alc") {bar.configureAsAlc();} else {QVERIFY(bar.configureVariant(flavor));}
            const QString saved=bar.serialize();
            for(QSize size:{QSize(260,72),QSize(520,144),QSize(640,72)}) {
                for(int dpr:{1,2}) {
                    TextPaintDevice device(size,dpr);QPainter painter(&device);bar.paint(painter,size.width(),size.height());painter.end();
                    QCOMPARE(device.images().size(),1);const QImage& raster=device.images().first();
                    const auto white=[&](int x,int y) {const QColor c=raster.pixelColor(x,y);return c.alpha()>64 && c.red()>80 && qAbs(c.red()-c.green())<5 && qAbs(c.green()-c.blue())<5;};
                    // Locate the actual cached scale baseline and tall major tick
                    // strokes. Their raster positions, not a copied calibration
                    // formula, are the oracle for the first interior label.
                    int baseline=-1,longest=0;
                    for(int y=0;y<raster.height();++y) {
                        int count=0;for(int x=0;x<raster.width();++x) {if(white(x,y)) {++count;}}
                        if(count>longest) {longest=count;baseline=y;}
                    }
                    QVERIFY(baseline>=0);const double unit=size.height()/72.0*dpr;
                    QVector<double> ticks;int start=-1;
                    const int tickRow=baseline-qRound(8*unit);
                    for(int x=0;x<=raster.width();++x) {
                        const bool ink=x<raster.width() && white(x,tickRow);
                        if(ink && start<0) {start=x;}
                        if(!ink && start>=0) {ticks.append((start+x-1)/2.0);start=-1;}
                    }
                    QVERIFY(ticks.size()>=2);int left=raster.width(),right=-1;
                    const int stop=qRound((ticks[0]+ticks[1])/2);
                    for(int y=baseline-qRound(32*unit);y<baseline-qRound(14*unit);++y) {
                        for(int x=0;x<stop;++x) {if(white(x,y)) {left=qMin(left,x);right=qMax(right,x);}}
                    }
                    QVERIFY(right>=left);const double center=(left+right)/2.0;
                    QVERIFY2(qAbs(center-ticks[0])<=3*unit,qPrintable(QString("%1 %2x%3 DPR%4: label center%5, actual tick%6").arg(flavor).arg(size.width()).arg(size.height()).arg(dpr).arg(center).arg(ticks[0])));
                    QCOMPARE(bar.serialize(),saved);
                }
            }
        }
    }
    void clockRolesPreserveOwningFontStyle() {
        for(auto kind:{CompositePresetItem::Face::Clock,CompositePresetItem::Face::Contest}) {
            for(const QString& family:{QString("Arial"),QString("Courier")}) {
                for(auto weight:{QFont::Light,QFont::DemiBold}) {
                    CompositePresetItem face(kind);
                    QFont owning(family);owning.setWeight(weight);owning.setItalic(true);
                    QVERIFY(face.applyConfiguration({{"importedFont",QJsonObject{{"family",family},{"weight",int(weight)},{"italic",true}}}}));
                    const QString saved=face.serialize();
                    for(QSize size:{QSize(360,280),QSize(720,560),QSize(720,120),QSize(260,360)}) {
                        for(int dpr:{1,2}) {
                            TextPaintDevice device(size,dpr);QPainter painter(&device);painter.setFont(owning);face.paint(painter,size.width(),size.height());painter.end();
                            int clocks=0;
                            for(const auto& text:device.texts()) {
                                if(text.value!="Local" && text.value!="UTC" && !text.value.contains(':') && !QRegularExpression("^\\d{4}-\\d{2}-\\d{2}$").match(text.value).hasMatch()) {continue;}
                                ++clocks;QCOMPARE(text.font.family(),family);QCOMPARE(text.font.weight(),weight);QCOMPARE(text.font.italic(),true);
                            }
                            QCOMPARE(clocks,6);QCOMPARE(face.serialize(),saved);
                        }
                    }
                }
            }
        }
    }
    void cachedBarTickInkScales() {
        BarPresetItem bar;bar.configureAsMic();
        int previous=0;
        for(int factor:{1,2,4}) {
            const QSize size(130*factor,36*factor);
            QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
            QPainter painter(&image);bar.paint(painter,size.width(),size.height());painter.end();
            // The white tick lettering sits above the tick strokes. Read actual
            // cached pixels in that band, independently of the final font helper.
            int top=size.height(),bottom=-1;
            for(int y=qRound(size.height()*.34);y<qRound(size.height()*.59);++y) {
                for(int x=0;x<size.width()/2;++x) {
                    const QColor ink=image.pixelColor(x,y);
                    if(ink.red()>80 && qAbs(ink.red()-ink.green())<5 && qAbs(ink.green()-ink.blue())<5) {top=qMin(top,y);bottom=qMax(bottom,y);}
                }
            }
            const QString debug=qEnvironmentVariable("NEREUS_RESPONSIVE_DEBUG_DIR");
            if(!debug.isEmpty()) {QDir().mkpath(debug);image.save(debug+QString("/bar-tick-%1.png").arg(factor));}
            QVERIFY2(bottom>=top,qPrintable(QString("tick scale%1").arg(factor))); const int height=bottom-top+1;
            QVERIFY2(height>previous,qPrintable(QString("tick ink%1 previous%2").arg(height).arg(previous))); previous=height;
        }
    }
    void resizeCycleAndCachedBarStyle() {
        BarPresetItem bar;bar.configureAsMic();bar.pushBindingValue(bar.bindingId(),-10);bar.advanceMeter(0);
        const QString before=bar.serialize();
        const auto render=[&](QSize size,QString family,int dpr) {
            QImage image(size*dpr,QImage::Format_ARGB32_Premultiplied); image.setDevicePixelRatio(dpr);image.fill(Qt::transparent);
            QPainter painter(&image);QFont font(family);font.setItalic(true);painter.setFont(font);bar.paint(painter,size.width(),size.height());painter.end();return image;
        };
        const QImage first=render({260,72},"Arial",1);
        render({520,144},"Arial",2);
        QCOMPARE(render({260,72},"Arial",1),first);
        const QImage changed=render({260,72},"Courier New",1);QVERIFY(first!=changed);
        QCOMPARE(render({260,72},"Arial",1),first);
        QCOMPARE(bar.serialize(),before);
    }
    void barTitleScales() {
        BarPresetItem face; face.configureAsMic(); const QString before=face.serialize();
        double previous=0;
        for(QSize size:{QSize(130,36),QSize(260,72),QSize(520,144)}) {
            TextPaintDevice device(size,1); QPainter painter(&device); face.paint(painter,size.width(),size.height()); painter.end();
            const auto found=std::find_if(device.texts().cbegin(),device.texts().cend(),[](const auto& text){return text.value=="MIC";});
            QVERIFY(found!=device.texts().cend()); QVERIFY(found->pixels>previous); previous=found->pixels;
            QCOMPARE(face.serialize(),before);
        }
    }
    void primitiveFixedFontsScaleAndPreserveStyle() {
        TextOverlayItem overlay; overlay.setText1("First line"); overlay.setText2("Second line");
        overlay.setFontFamily1("Arial"); overlay.setFontBold1(true); overlay.setFontSize1(18.5f);
        HistoryGraphItem history; DialItem dial;
        for(MeterItem* item:{static_cast<MeterItem*>(&overlay),static_cast<MeterItem*>(&history),static_cast<MeterItem*>(&dial)}) {
            const QString before=item->serialize(); double previous=0;
            for(QSize size:{QSize(130,60),QSize(260,120),QSize(520,240)}) {
                TextPaintDevice device(size,1); QPainter painter(&device); item->paint(painter,size.width(),size.height()); painter.end();
                QVERIFY(!device.texts().isEmpty());
                QVERIFY2(device.texts().first().pixels>previous,qPrintable(item->serialize())); previous=device.texts().first().pixels;
                if(item==&overlay) { QVERIFY(device.texts().first().font.bold()); QCOMPARE(device.texts().first().font.family(),QString("Arial")); QVERIFY(device.texts().first().font.pointSizeF()>0); }
                QCOMPARE(item->serialize(),before);
            }
        }
    }
    void responsivePrimitiveRolesFit() {
        ClockItem clock(nullptr,false); SignalTextItem signal; NeedleScalePwrItem scale; BandButtonItem buttons;
        buttons.button(0).text="A deliberately long band button label";
        signal.setShowPeakValue(true);signal.setPeakHold(true);signal.setValue(-123.4);
        scale.setScaleCalibration({{0,{0,0}},{100,{1,1}}});scale.setMarks(2);scale.setFontSize(36);
        for(MeterItem* item:{static_cast<MeterItem*>(&clock),static_cast<MeterItem*>(&signal),static_cast<MeterItem*>(&scale),static_cast<MeterItem*>(&buttons)}) {
            const QString before=item->serialize();
            for(QSize size:{QSize(130,60),QSize(520,240),QSize(120,360)}) {
                // Legacy multi-button grids retain width-derived cells. Give
                // that primitive a full grid allocation; Contest fits its cells
                // to the parent face and is covered above at every aspect ratio.
                if(item==&buttons) {size.setHeight(qMax(size.height(),size.width()));}
                TextPaintDevice device(size,1);QPainter painter(&device);item->paint(painter,size.width(),size.height());painter.end();
                QVERIFY(!device.texts().isEmpty());
                for(const auto& text:device.texts()) {
                    QVERIFY2(QRectF(QPointF(0,0),size).adjusted(-1,-1,1,1).contains(text.bounds),qPrintable(text.value+" "+item->serialize()));
                }
                if(item!=&scale) {
                    for(int i=0;i<device.texts().size();++i) { for(int j=i+1;j<device.texts().size();++j) {
                        QVERIFY2(!device.texts()[i].bounds.intersects(device.texts()[j].bounds),qPrintable(device.texts()[i].value+" overlaps "+device.texts()[j].value));
                    } }
                }
                QCOMPARE(item->serialize(),before);
            }
        }
    }
    void objectRectangleOwnsFonts() {
        using Face=CompositePresetItem::Face;
        for(Face kind:{Face::PowerSwr,Face::Cross,Face::Eye,Face::History,Face::SignalText,Face::Clock,Face::Contest}) {
            CompositePresetItem face(kind);
            const QSize reference=kind==Face::Cross?QSize(360,260):kind==Face::Contest?QSize(360,280):kind==Face::PowerSwr?QSize(260,144):kind==Face::Eye?QSize(260,180):QSize(260,120);
            TextPaintDevice alone(reference,1);QPainter direct(&alone);face.paint(direct,reference.width(),reference.height());direct.end();
            face.setRect(.25,.25,.5,.5);const QString saved=face.serialize();
            const QSize canvas=reference*2;
            TextPaintDevice shared(canvas,1);QPainter paint(&shared);face.paint(paint,canvas.width(),canvas.height());paint.end();
            QCOMPARE(shared.texts().size(),alone.texts().size());
            for(int i=0;i<alone.texts().size();++i) { QCOMPARE(shared.texts()[i].pixels,alone.texts()[i].pixels); }
            QCOMPARE(face.serialize(),saved);
        }
    }
    void syntheticNativeCaptures() {
        const QString directory=qEnvironmentVariable("NEREUS_RESPONSIVE_CAPTURE_DIR");
        if(directory.isEmpty()) { QSKIP("Native capture requested explicitly with capture directory"); }
        QVERIFY(QDir().mkpath(directory));
        const QString family=qEnvironmentVariable("NEREUS_RESPONSIVE_CAPTURE_FAMILY");
        using Face=CompositePresetItem::Face;
        for(Face kind:{Face::PowerSwr,Face::Cross,Face::Eye,Face::History,Face::SignalText,Face::Clock,Face::Contest}) {
            CompositePresetItem face(kind);
            if(!family.isEmpty() && face.typeId()!=family) {continue;}
            const QSize base=kind==Face::Cross?QSize(360,260):kind==Face::Contest?QSize(360,280):kind==Face::PowerSwr?QSize(260,144):kind==Face::Eye?QSize(260,180):QSize(260,120);
            for(int binding:face.readingBindings()) { face.pushBindingValue(binding,binding==1?-93:1.5); }
            face.advanceMeter(0); face.advanceMeter(100);
            for(int factor:{1,2}) {
                const QSize size=base*factor;
                class FaceWidget : public QWidget {
                public: MeterItem* face=nullptr;
                protected: void paintEvent(QPaintEvent*) override {QPainter painter(this);face->paint(painter,width(),height());}
                } widget;
                widget.face=&face; widget.resize(size); widget.setWindowTitle("Synthetic offline meter font capture");widget.show(); QTest::qWait(80);
                QVERIFY(widget.grab().save(directory+"/"+face.typeId()+QString("-%1.png").arg(factor)));
                widget.hide();
            }
        }
    }
};
QTEST_MAIN(TestResponsiveMeterFonts)
#include "tst_meter_responsive_fonts.moc"
