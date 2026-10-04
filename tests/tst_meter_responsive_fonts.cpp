// no-port-check: Observe native Qt paint output independently of product geometry.
#include <QtTest>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPaintEngine>
#include <QPaintDevice>
#include <QDir>
#include <QWidget>
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/HistoryGraphItem.h"
#include "gui/meters/TextOverlayItem.h"
#include "gui/meters/DialItem.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/SignalTextItem.h"
#include "gui/meters/NeedleScalePwrItem.h"
#include "gui/meters/BandButtonItem.h"
using namespace NereusSDR;
namespace {
// Observe actual QPainter text output at the paint-device boundary. This avoids
// using the renderer's private layout arithmetic as the readability oracle.
class TextPaintEngine : public QPaintEngine {
public:
    struct Text { QString value; double pixels; QRectF bounds; QFont font; };
    QVector<Text> texts;
    QVector<QPainterPath> strokes;
    explicit TextPaintEngine(int dpr):QPaintEngine(AllFeatures),m_dpr(dpr) {}
    bool begin(QPaintDevice* device) override { setPaintDevice(device); setActive(true); return true; }
    bool end() override { setActive(false); return true; }
    void updateState(const QPaintEngineState&) override {}
    Type type() const override { return User; }
    void drawPixmap(const QRectF&,const QPixmap&,const QRectF&) override {}
    void drawImage(const QRectF&,const QImage&,const QRectF&,Qt::ImageConversionFlags) override {}
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
