// no-port-check: Independent numeric oracles and real native rendering fixtures.
#include <QtTest>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEngine>
#include <QPaintDevice>
#include <QDir>
#include <QScreen>
#include <QJsonArray>
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/CrossNeedleItem.h"
#include "gui/meters/presets/AnanMultiMeterItem.h"
#include "gui/meters/presets/PowerSwrPresetItem.h"
#include "gui/meters/presets/MagicEyePresetItem.h"
#include "gui/meters/presets/SignalTextPresetItem.h"
#include "gui/meters/presets/ContestPresetItem.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/ItemGroup.h"
#include "models/RadioModel.h"
#include "core/RadioStatus.h"
#include "core/mmio/MmioEndpoint.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "models/SliceModel.h"
#include "models/Band.h"
using namespace NereusSDR;
namespace {
// Observe actual QPainter text output at the paint-device boundary. This avoids
// using the renderer's private layout arithmetic as the readability oracle.
class TextPaintEngine : public QPaintEngine {
public:
    struct Text { QString value; int pixels; QRectF bounds; };
    QVector<Text> texts;
    struct Glyph { QSize sourceSize; QRectF bounds; };
    QVector<Glyph> glyphs;
    struct PaintedPath { QPainterPath path; QBrush brush; qreal opacity; };
    QVector<PaintedPath> paths;
    explicit TextPaintEngine(int dpr):QPaintEngine(AllFeatures),m_dpr(dpr) {}
    bool begin(QPaintDevice* device) override { setPaintDevice(device); setActive(true); return true; }
    bool end() override { setActive(false); return true; }
    void updateState(const QPaintEngineState&) override {}
    Type type() const override { return User; }
    void drawPixmap(const QRectF&,const QPixmap&,const QRectF&) override {}
    void drawImage(const QRectF& target,const QImage& image,const QRectF&,Qt::ImageConversionFlags) override {
        // Small cached source crops are face glyphs; the1855×848 art is not.
        if(image.width()<=468 && image.height()<=65) {
            const QRectF mapped=state->transform().mapRect(target);
            glyphs.append({image.size(),QRectF(mapped.topLeft()/m_dpr,mapped.size()/m_dpr)});
        }
    }
    void drawPath(const QPainterPath& path) override {
        QTransform logical=state->transform(); logical.scale(1.0/m_dpr,1.0/m_dpr);
        paths.append({logical.map(path),state->brush(),state->opacity()});
    }
    void drawPolygon(const QPointF*,int,PolygonDrawMode) override {}
    void drawTextItem(const QPointF& point,const QTextItem& item) override {
        const QRectF bounds(point.x(),point.y()-item.ascent(),item.width(),item.ascent()+item.descent());
        const QRectF mapped=state->transform().mapRect(bounds);
        texts.append({item.text(),item.font().pixelSize(),QRectF(mapped.topLeft()/m_dpr,mapped.size()/m_dpr)});
    }
private:
    int m_dpr;
};
class TextPaintDevice : public QPaintDevice {
public:
    TextPaintDevice(QSize size,int dpr):m_size(size),m_dpr(dpr),m_engine(dpr) {}
    QPaintEngine* paintEngine() const override { return &m_engine; }
    const QVector<TextPaintEngine::Text>& texts() const { return m_engine.texts; }
    const QVector<TextPaintEngine::Glyph>& glyphs() const { return m_engine.glyphs; }
    const QVector<TextPaintEngine::PaintedPath>& paths() const { return m_engine.paths; }
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
class TestCompositePresets:public QObject {
    Q_OBJECT
private:
    static QJsonArray configuredChannels(CompositePresetItem& face) {
        QJsonArray channels=face.configuration()["channels"].toArray();
        for(int i=0;i<channels.size();++i) { auto c=channels[i].toObject(); c["ignoreHistoryMs"]=0; channels[i]=c; } return channels;
    }
private slots:
    void ananIdleNeedlesRemainParkedWithoutMeasurements() {
        using Support=MeterItem::BindingSupport;
        // Independently recorded approved starting anchors, in source1855×848
        // coordinates. These are the scale starts, including10V and1SWR.
        const QPointF starts[]{QPointF(.117059794,.380134788),QPointF(.588119078,.765593954),
            QPointF(.245534088,.565355119),QPointF(.232210013,.378065912),
            QPointF(.239802545,.476981032),QPointF(.269329015,.646806982),QPointF(.225121668,.741962507)};
        const int order[]{1,6,5,2,4,3,0};
        for(Support support:{Support::Supported,Support::Unknown}) {
            AnanMultiMeterItem face;
            for(int binding:face.readingBindings()) { face.setBindingSupport(binding,support); }
            face.setAboveS9Frequency(true); face.setPowerScale(500);
            const QString saved=face.serialize();
            for(bool tx:{false,true}) {
                face.resetForTxTransition(tx);
                for(int group:{1,2,3,4,0}) {
                    QVERIFY(face.applyConfiguration({{"displayGroup",group}}));
                    for(const QSize size:{QSize(360,300),QSize(520,300),QSize(720,420)}) {
                        const QRectF skin=face.ananNeedleRect(size.width(),size.height());
                        for(int dpr:{1,2}) {
                            TextPaintDevice device(size,dpr);
                            { QPainter painter(&device); face.paintForLayer(painter,size.width(),size.height(),MeterItem::Layer::OverlayDynamic); }
                            QVector<TextPaintEngine::PaintedPath> needles;
                            for(const auto& path:device.paths()) {
                                if(path.path.elementCount()==5 && path.brush.color()!=QColor(0,0,0,150)) { needles.append(path); }
                            }
                            QCOMPARE(needles.size(),7);
                            for(int n=0;n<7;++n) {
                                const int channel=order[n]; const auto& path=needles[n];
                                const auto tip=path.path.elementAt(1),left=path.path.elementAt(0),right=path.path.elementAt(2);
                                const QPointF expected=skin.topLeft()+QPointF(starts[channel].x()*skin.width(),starts[channel].y()*skin.height());
                                const QPointF orb=channel==1?QPointF(1248,736):channel==6?QPointF(550,737):QPointF(919,740);
                                QVERIFY(QLineF(QPointF(tip.x,tip.y),expected).length()<.01);
                                QVERIFY(QLineF(QPointF((left.x+right.x)/2,(left.y+right.y)/2),skin.topLeft()+orb*skin.width()/1855).length()<.001);
                                QVERIFY(path.opacity>0 && path.opacity<1);
                                QVERIFY(!face.channelHasReading(channel));
                            }
                            for(const auto& text:device.texts()) {
                                if(text.value.startsWith("Peak ")) { QCOMPARE(text.value,QString("Peak --")); }
                                else if(!QStringList{"Signal","Volts","Amps","Power","SWR","Compression","ALC group"}.contains(text.value)) { QVERIFY(text.value.startsWith("--")); }
                            }
                        }
                    }
                }
            }
            QVERIFY(face.applyConfiguration({{"displayGroup",1}})); QCOMPARE(face.serialize(),saved);
        }
    }
    void ananIdleNeedleRasterProof() {
        using Support=MeterItem::BindingSupport;
        const QPointF starts[]{QPointF(.117059794,.380134788),QPointF(.588119078,.765593954),
            QPointF(.245534088,.565355119),QPointF(.232210013,.378065912),
            QPointF(.239802545,.476981032),QPointF(.269329015,.646806982),QPointF(.225121668,.741962507)};
        // Verify actual raster stem ink away from the hub for each role. Mode
        // gates retain default policies even in these isolated pointer fixtures.
        for(int channel=0;channel<7;++channel) {
            AnanMultiMeterItem shown,hidden;
            auto channels=shown.configuration()["channels"].toArray();
            for(int i=0;i<7;++i) { auto c=channels[i].toObject(); c["visible"]=i==channel; channels[i]=c; }
            QVERIFY(shown.applyConfiguration({{"channels",channels},{"showReadout",false}}));
            for(int binding:shown.readingBindings()) { shown.setBindingSupport(binding,Support::Supported); }
            auto c=channels[channel].toObject(); c["visible"]=false; channels[channel]=c;
            QVERIFY(hidden.applyConfiguration({{"channels",channels},{"showReadout",false}}));
            for(const QSize size:{QSize(360,300),QSize(520,300)}) {
                const QRectF skin=shown.ananNeedleRect(size.width(),size.height());
                const QPointF orb=channel==1?QPointF(1248,736):channel==6?QPointF(550,737):QPointF(919,740);
                const QPointF pivot=skin.topLeft()+orb*skin.width()/1855,tip=skin.topLeft()+QPointF(starts[channel].x()*skin.width(),starts[channel].y()*skin.height());
                for(int dpr:{1,2}) {
                    QImage present(size*dpr,QImage::Format_ARGB32_Premultiplied),absent=present;
                    present.setDevicePixelRatio(dpr); absent.setDevicePixelRatio(dpr); present.fill(Qt::transparent); absent.fill(Qt::transparent);
                    { QPainter painter(&present); shown.paintForLayer(painter,size.width(),size.height(),MeterItem::Layer::OverlayDynamic); }
                    { QPainter painter(&absent); hidden.paintForLayer(painter,size.width(),size.height(),MeterItem::Layer::OverlayDynamic); }
                    const QPoint at=((pivot+(tip-pivot)*.55)*dpr).toPoint(); int stemPixels=0;
                    for(int y=-2*dpr;y<=2*dpr;++y) { for(int x=-2*dpr;x<=2*dpr;++x) {
                        const QPoint point=at+QPoint(x,y);
                        if(present.pixelColor(point).alpha()>absent.pixelColor(point).alpha()+10) { ++stemPixels; }
                    } }
                    QVERIFY2(stemPixels>=2,qPrintable(QString("Absent parked stem channel%1 at%2px DPR%3").arg(channel).arg(size.width()).arg(dpr)));
                }
            }
        }
    }
    void ananParkedTransitionsNeverUseStaleHistory() {
        using Support=MeterItem::BindingSupport;
        AnanMultiMeterItem face; auto channels=configuredChannels(face);
        for(int i=0;i<7;++i) { auto c=channels[i].toObject(); c["attack"]=1; c["decay"]=1; c["peakHold"]=true; c["showHistory"]=true; channels[i]=c; }
        QVERIFY(face.applyConfiguration({{"channels",channels}}));
        for(int binding:face.readingBindings()) { face.setBindingSupport(binding,Support::Supported); }
        const QString saved=face.serialize();
        const auto check=[&](int expectedLive,int expectedIncluded) {
            TextPaintDevice device(QSize(520,300),1);
            { QPainter painter(&device); face.paintForLayer(painter,520,300,MeterItem::Layer::OverlayDynamic); }
            int live=0,idle=0,history=0;
            for(const auto& path:device.paths()) {
                if(path.path.elementCount()==5 && path.brush.color()!=QColor(0,0,0,150)) { if(path.opacity==1) { ++live; } else { ++idle; } }
                if(path.brush.color().alpha()==64 && path.opacity==1) { ++history; }
            }
            // Every active channel emits its real pointer and requested peak;
            // an idle channel emits exactly one pointer and no history fan.
            QCOMPARE(live,expectedLive*2); QCOMPARE(idle,expectedIncluded-expectedLive);
            QCOMPARE(history,expectedLive);
        };
        face.pushBindingValue(MeterBinding::SignalAvg,-85); face.pushBindingValue(MeterBinding::HwVolts,13.8); face.advanceMeter(0);
        check(2,7); QCOMPARE(face.channelValue(0),-85.0); QCOMPARE(face.channelPeak(0),-85.0);
        face.resetForTxTransition(true); check(0,7);
        for(int i=0;i<7;++i) { QVERIFY(!face.channelHasReading(i)); }
        TextPaintDevice absent(QSize(520,300),1);
        { QPainter painter(&absent); face.paintForLayer(painter,520,300,MeterItem::Layer::OverlayDynamic); }
        for(const auto& text:absent.texts()) { QVERIFY(!text.value.contains("-85.0") && !text.value.contains("13.8")); }
        face.pushBindingValue(MeterBinding::TxPower,70); face.pushBindingValue(MeterBinding::TxSwr,1.6); face.advanceMeter(100); check(2,7);
        // A real sample outside the selected group does not become a live
        // pointer, fan or peak. This preserves the existing group policy.
        face.pushBindingValue(MeterBinding::TxAlcGain,20); face.advanceMeter(200); QVERIFY(face.channelHasReading(5)); check(2,7);
        QVERIFY(face.applyConfiguration({{"displayGroup",2}})); check(0,7);
        face.pushBindingValue(MeterBinding::TxAlcGain,20); face.advanceMeter(300); check(1,7);
        face.setBindingSupport(MeterBinding::TxAlcGain,Support::Unsupported); check(0,6);
        face.setBindingSupport(MeterBinding::TxAlcGain,Support::Supported); check(0,7);
        auto hidden=face.configuration()["channels"].toArray(); auto c=hidden[2].toObject(); c["visible"]=false; hidden[2]=c;
        QVERIFY(face.applyConfiguration({{"channels",hidden}})); check(0,6);
        face.resetForTxTransition(false); check(0,6);
        c["visible"]=true; hidden[2]=c; QVERIFY(face.applyConfiguration({{"channels",hidden},{"displayGroup",1}}));
        face.setBindingSupport(MeterBinding::SignalAvg,Support::Unknown); face.pushBindingValue(MeterBinding::SignalAvg,-91); face.advanceMeter(400); check(1,7);
        QCOMPARE(face.channelValue(0),-91.0); QCOMPARE(face.channelPeak(0),-91.0);
        face.setBindingUnavailable(MeterBinding::SignalAvg,"offline fixture unavailable"); check(0,7);
        face.setBindingUnavailable(MeterBinding::SignalAvg,QString()); face.advanceMeter(500); check(0,7);
        QCOMPARE(face.serialize(),saved);
    }
    void ananDefaultStateNativeFrames() {
#ifdef NEREUS_GPU_SPECTRUM
        if(QGuiApplication::platformName()=="offscreen") { QSKIP("Native QRhi frames require the configured native platform"); }
#endif
        const QString destination=qEnvironmentVariable("NEREUS_METER_CAPTURE_DIR");
        for(int width:{360,520,720}) {
            MeterWidget widget; auto face=std::make_unique<AnanMultiMeterItem>();
            auto channels=configuredChannels(*face);
            for(int i=0;i<7;++i) { auto c=channels[i].toObject(); c["attack"]=1; c["decay"]=1; channels[i]=c; }
            QVERIFY(face->applyConfiguration({{"channels",channels}}));
            AnanMultiMeterItem* item=face.get(); widget.addItem(face.release()); widget.resize(width,300);
#ifdef NEREUS_GPU_SPECTRUM
            QSignalSpy submitted(&widget,&QRhiWidget::frameSubmitted);
#endif
            widget.show();
            const auto capture=[&](const QString& state) {
                QImage native;
#ifdef NEREUS_GPU_SPECTRUM
                const int before=submitted.count(); widget.update(); QTRY_VERIFY_WITH_TIMEOUT(submitted.count()>before,2000); native=widget.grabFramebuffer();
#else
                widget.update(); QCoreApplication::processEvents(); native=widget.grab().toImage();
#endif
                QVERIFY(!native.isNull());
                qInfo()<<"ANAN offline fixture"<<state<<QGuiApplication::platformName()<<width<<"DPR"<<widget.devicePixelRatioF();
                if(!destination.isEmpty()) {
                    QDir().mkpath(destination); QVERIFY(native.save(destination+QString("/anan-default-%1-native-%2-dpr%3.png").arg(state).arg(width).arg(widget.devicePixelRatioF())));
                    for(int dpr:{1,2}) {
                        QImage cpu(QSize(width,300)*dpr,QImage::Format_ARGB32_Premultiplied); cpu.setDevicePixelRatio(dpr); cpu.fill(Qt::transparent);
                        { QPainter painter(&cpu); item->paint(painter,width,300); }
                        QVERIFY(cpu.save(destination+QString("/anan-default-%1-cpu-%2-dpr%3.png").arg(state).arg(width).arg(dpr)));
                    }
                }
            };
            const QString saved=item->serialize(); capture("disconnected-unknown");
            for(int binding:item->readingBindings()) { widget.setBindingSupport(binding,MeterItem::BindingSupport::Supported); }
            widget.updateMeterValue(MeterBinding::SignalAvg,-85); widget.updateMeterValue(MeterBinding::HwVolts,13.8); widget.advanceMeters(0); capture("rx-supported");
            widget.resetForTxTransition(true); widget.advanceMeters(100); capture("tx-idle-supported");
            widget.updateMeterValue(MeterBinding::HwVolts,13.8); widget.updateMeterValue(MeterBinding::TxPower,70); widget.updateMeterValue(MeterBinding::TxSwr,1.6); widget.advanceMeters(200); capture("tx-group1-supported");
            for(int group:{2,3,4}) {
                QVERIFY(item->applyConfiguration({{"displayGroup",group}})); widget.invalidatePresentation(item);
                const int binding=group==2?MeterBinding::TxAlcGain:group==3?MeterBinding::TxAlcGroup:MeterBinding::HwAmps;
                widget.updateMeterValue(binding,group==2?20:group==3?0:8); widget.advanceMeters(200+group*100); capture(QString("tx-group%1-supported").arg(group));
            }
            widget.clearReadingCache(); widget.resetForTxTransition(false); QVERIFY(item->applyConfiguration({{"displayGroup",1}})); widget.invalidatePresentation(item); widget.advanceMeters(700); capture("rx-return-no-stale-samples");
            for(int i=0;i<7;++i) { QVERIFY(!item->channelHasReading(i)); }
            QCOMPARE(item->serialize(),saved);
        }
    }
    void ananAllTransmitGroupPreservesRoles() {
        AnanMultiMeterItem face;
        QVERIFY(face.applyConfiguration({{"displayGroup",0}}));
        QVERIFY(face.channelVisible(0));
        QVERIFY(face.channelVisible(1));
        QVERIFY(!face.channelVisible(2));
        face.resetForTxTransition(true);
        QVERIFY(!face.channelVisible(0));
        for (int channel=1;channel<7;++channel) { QVERIFY2(face.channelVisible(channel),qPrintable(QString::number(channel))); }
        auto channels=face.configuration()["channels"].toArray();
        auto current=channels[2].toObject(); current["visible"]=false; channels[2]=current;
        QVERIFY(face.applyConfiguration({{"channels",channels}})); QVERIFY(!face.channelVisible(2));
        face.takeStaticPresentationChange();
        QMouseEvent click(QEvent::MouseButtonPress,QPointF(350,10),QPointF(350,10),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        for (int expected:{1,2,3,4,0}) {
            QVERIFY(face.handleMousePress(&click,360,300)); QCOMPARE(face.configuration()["displayGroup"].toInt(),expected);
            QVERIFY(face.takeStaticPresentationChange());
        }
    }
    void ananOwnedFaceIsDarkAndComplete() {
        AnanMultiMeterItem face;
        QVERIFY(QFile::exists(":/meters/ananMM.png")); QVERIFY(QFile::exists(":/icons/NereusSDR.png"));
        const QString before=face.serialize();
        QImage image(720,300,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
        { QPainter painter(&image); face.paintForLayer(painter,720,300,MeterItem::Layer::Background); }
        int dark=0,silver=0,cyan=0;
        for (int y=40;y<250;++y) { for (int x=100;x<620;++x) {
            const QColor pixel=image.pixelColor(x,y);
            if (pixel.lightness()<80) {++dark;}
            if (pixel.lightness()>130 && qAbs(pixel.red()-pixel.green())<30) {++silver;}
            if (pixel.blue()>pixel.red()+25 && pixel.green()>pixel.red()+25) {++cyan;}
        } }
        QVERIFY2(dark>75000,"ANAN face must compose dark owned glass, not cream schematic");
        QVERIFY(silver>500); QVERIFY(cyan>50); QCOMPARE(face.serialize(),before);
        const QString destination=qEnvironmentVariable("NEREUS_METER_CAPTURE_DIR");
        if (!destination.isEmpty()) { QDir().mkpath(destination); QVERIFY(image.save(destination+"/anan-owned-background-720.png")); }
    }
    void ananSevenNeedleRasterProof() {
        // Demonstration only: override live RX/TX flags in this fixture's memory.
        // Distinct generic COMP and ALC_G values prove the ANAN binding identity.
        AnanMultiMeterItem face; auto channels=configuredChannels(face);
        for(int i=0;i<channels.size();++i) {
            auto c=channels[i].toObject(); c["onlyWhenRx"]=false; c["onlyWhenTx"]=false;
            c["attack"]=1; c["decay"]=1; c["showHistory"]=false; channels[i]=c;
        }
        QVERIFY(face.applyConfiguration({{"channels",channels},{"displayGroup",0},{"showPeakValue",false}}));
        for(int binding:face.readingBindings()) { face.setBindingSupport(binding,MeterItem::BindingSupport::Supported); }
        face.pushBindingValue(1,-85); face.pushBindingValue(200,13.8); face.pushBindingValue(201,8);
        face.pushBindingValue(100,70); face.pushBindingValue(102,1.6); face.pushBindingValue(109,20); face.pushBindingValue(110,0);
        face.pushBindingValue(104,-29); face.advanceMeter(0);
        QCOMPARE(face.channelValue(5),20.0); QCOMPARE(face.channelValue(6),0.0);
        const QString before=face.serialize(),destination=qEnvironmentVariable("NEREUS_METER_CAPTURE_DIR");
        for(const QSize size:{QSize(360,300),QSize(520,300),QSize(720,300),QSize(720,420),QSize(1440,600),QSize(1440,840),QSize(720,180),QSize(360,600)}) {
            for(int dpr:{1,2}) {
                QImage image(size*dpr,QImage::Format_ARGB32_Premultiplied); image.setDevicePixelRatio(dpr); image.fill(Qt::transparent);
                {QPainter painter(&image);face.paint(painter,size.width(),size.height());}
                QVERIFY(!image.isNull());
                if(!destination.isEmpty()) {
                    QDir().mkpath(destination);
                    QVERIFY(image.save(destination+QString("/anan-seven-SYNTHETIC-%1x%2-dpr%3.png").arg(size.width()).arg(size.height()).arg(dpr)));
                }
            }
        }
        QCOMPARE(face.serialize(),before);
    }
    void ananReadoutsKeepReadableFontsAndAllSevenRoles() {
        // Regression: proportional sizing must not emit6px names/peaks at the
        // ordinary360px minimum, and box fitting must not defeat the font floor.
        AnanMultiMeterItem face; auto channels=configuredChannels(face);
        for(int i=0;i<channels.size();++i) {
            auto c=channels[i].toObject(); c["onlyWhenRx"]=false; c["onlyWhenTx"]=false;
            c["attack"]=1; c["decay"]=1; c["showHistory"]=false; channels[i]=c;
        }
        QVERIFY(face.applyConfiguration({{"channels",channels},{"displayGroup",0},{"showPeakValue",true}}));
        for(int binding:face.readingBindings()) { face.setBindingSupport(binding,MeterItem::BindingSupport::Supported); }
        face.pushBindingValue(1,-85); face.pushBindingValue(200,13.8); face.pushBindingValue(201,8);
        face.pushBindingValue(100,70); face.pushBindingValue(102,1.6); face.pushBindingValue(109,20); face.pushBindingValue(110,0); face.advanceMeter(0);
        const QStringList names{"Signal","Volts","Amps","Power","SWR","Compression","ALC group"};
        const QStringList values{"-85.0 dBm","13.8 V","8.0 A","70.0 W","1.6 ","20.0 dB","0.0 dB"};
        const QString saved=face.serialize(),destination=qEnvironmentVariable("NEREUS_METER_CAPTURE_DIR");
        for(const QSize size:{QSize(360,300),QSize(360,600),QSize(320,300),QSize(520,300),QSize(720,180),QSize(720,420)}) {
            for(int dpr:{1,2}) {
                TextPaintDevice device(size,dpr);
                { QPainter painter(&device); face.paintForLayer(painter,size.width(),size.height(),MeterItem::Layer::OverlayDynamic); }
                QMap<QString,int> counts; int peaks=0;
                for(const auto& text:device.texts()) {
                    const bool name=names.contains(text.value),value=values.contains(text.value),peak=text.value.startsWith("Peak ");
                    QVERIFY2(name || value || peak,qPrintable("Unexpected readout text: "+text.value));
                    ++counts[text.value]; if(peak) { ++peaks; }
                    const int floor=value?12:11;
                    QVERIFY2(text.pixels>=floor,qPrintable(QString("%1x%2 DPR%3: %4 emitted%5px, minimum%6px").arg(size.width()).arg(size.height()).arg(dpr).arg(text.value).arg(text.pixels).arg(floor)));
                    QVERIFY2(QRectF(QPointF(0,0),size).adjusted(-.5,-.5,.5,.5).contains(text.bounds),qPrintable(QString("Clipped readout %1 bounds%2,%3 %4x%5").arg(text.value).arg(text.bounds.x()).arg(text.bounds.y()).arg(text.bounds.width()).arg(text.bounds.height())));
                    for(const auto& prior:device.texts()) {
                        if(&prior==&text) { break; }
                        QVERIFY2(!text.bounds.intersects(prior.bounds),qPrintable("Readout overlap: "+text.value+" / "+prior.value));
                    }
                }
                for(const QString& name:names) { QCOMPARE(counts.value(name),1); }
                for(const QString& value:values) { QCOMPARE(counts.value(value),1); }
                QCOMPARE(peaks,7); QCOMPARE(device.texts().size(),21);
                if(!destination.isEmpty()) {
                    QImage image(size*dpr,QImage::Format_ARGB32_Premultiplied); image.setDevicePixelRatio(dpr); image.fill(Qt::transparent);
                    { QPainter painter(&image); face.paint(painter,size.width(),size.height()); }
                    QDir().mkpath(destination); QVERIFY(image.save(destination+QString("/anan-seven-PEAK-SYNTHETIC-%1x%2-dpr%3.png").arg(size.width()).arg(size.height()).arg(dpr)));
                }
            }
        }
        QCOMPARE(face.serialize(),saved); QCOMPARE(face.configuration()["fontSize"].toInt(),18);
        int ordinaryValueFont=0,largerValueFont=0;
        for(int fontSize:{18,36}) {
            QVERIFY(face.applyConfiguration({{"fontSize",fontSize}}));
            // Configuration edits clear dynamics; use a fresh sample.
            face.pushBindingValue(1,-85); face.advanceMeter(100);
            TextPaintDevice device(QSize(720,600),1);
            { QPainter painter(&device); face.paintForLayer(painter,720,600,MeterItem::Layer::OverlayDynamic); }
            for(const auto& text:device.texts()) { if(text.value=="-85.0 dBm") { (fontSize==18?ordinaryValueFont:largerValueFont)=text.pixels; } }
            QCOMPARE(face.configuration()["fontSize"].toInt(),fontSize);
        }
        QVERIFY(ordinaryValueFont>=12); QVERIFY(largerValueFont>=ordinaryValueFont*1.8);
    }
    void compactAnanSourceGlyphsFitInsideRoundedGlass() {
        // Regression: enlarging compact lettering must not send the dB crop
        // outside ordinary360px glass, or crop the heading in a short face.
        AnanMultiMeterItem face;
        const QString saved=face.serialize();
        for(const QSize size:{QSize(360,300),QSize(360,600),QSize(320,300),QSize(520,300),QSize(720,180)}) {
            const QRectF skin=face.ananNeedleRect(size.width(),size.height());
            QVERIFY(skin.width()<600);
            const double scale=skin.width()/1855.0;
            // Independent approved glass oracle; includes a1logicalpx inset.
            const QRectF bounds(skin.left()+84*scale,skin.top()+76*scale,1687*scale,678*scale);
            QPainterPath glass; glass.addRoundedRect(bounds.adjusted(1,1,-1,-1),qMax(0.0,44*scale-1),qMax(0.0,44*scale-1));
            for(int dpr:{1,2}) {
                TextPaintDevice device(size,dpr);
                { QPainter painter(&device); face.paintForLayer(painter,size.width(),size.height(),MeterItem::Layer::Background); }
                QCOMPARE(device.glyphs().size(),3);
                for(const auto& glyph:device.glyphs()) {
                    QVERIFY2(glass.contains(glyph.bounds),qPrintable(QString("%1x%2 DPR%3 source%4x%5 glyph bounds%6,%7 %8x%9 escape rounded glass").arg(size.width()).arg(size.height()).arg(dpr).arg(glyph.sourceSize.width()).arg(glyph.sourceSize.height()).arg(glyph.bounds.x()).arg(glyph.bounds.y()).arg(glyph.bounds.width()).arg(glyph.bounds.height())));
                    const double sourceAspect=double(glyph.sourceSize.width())/glyph.sourceSize.height();
                    QVERIFY(qAbs(glyph.bounds.width()/glyph.bounds.height()-sourceAspect)<.0001);
                    for(const auto& prior:device.glyphs()) {
                        if(&prior==&glyph) { break; }
                        QVERIFY2(!glyph.bounds.intersects(prior.bounds),qPrintable(QString("%1x%2 DPR%3 compact glyphs overlap").arg(size.width()).arg(size.height()).arg(dpr)));
                    }
                }
            }
        }
        QCOMPARE(face.serialize(),saved);
    }
    void ananSupportMasksScaleAndNeverRevivesOldSamples() {
        using Support=MeterItem::BindingSupport;
        AnanMultiMeterItem face;
        auto channels=configuredChannels(face); auto current=channels[2].toObject(); current["future"]=QJsonObject{{"preserve",true}}; channels[2]=current;
        QVERIFY(face.applyConfiguration({{"channels",channels},{"future",QJsonObject{{"root",true}}},{"showReadout",false},{"displayGroup",0},{"x",-.125},{"w",1.125}}));
        const QString saved=face.serialize();
        face.resetForTxTransition(true); face.setBindingSupport(201,Support::Supported); face.pushBindingValue(201,0); face.advanceMeter(0);
        QVERIFY(face.channelHasReading(2)); QCOMPARE(face.channelValue(2),0.0);
        const auto background=[&] {QImage image(720,300,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter p(&image);face.paintForLayer(p,720,300,MeterItem::Layer::Background);return image;};
        const QImage supported=background(); face.takeStaticPresentationChange(); face.setBindingSupport(201,Support::Unsupported);
        QVERIFY(face.takeStaticPresentationChange()); QVERIFY(!face.channelHasReading(2)); QVERIFY(background()!=supported);
        face.pushBindingValue(201,9); face.advanceMeter(100); QVERIFY(!face.channelHasReading(2));
        face.setBindingSupport(201,Support::Supported); face.advanceMeter(200); QVERIFY(!face.channelHasReading(2));
        for(double missing:{-400.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
            face.pushBindingValue(201,missing);face.advanceMeter(300);QVERIFY(!face.channelHasReading(2));
        }
        face.pushBindingValue(201,9); face.advanceMeter(400); QVERIFY(face.channelHasReading(2));
        face.setBindingSupport(201,Support::Unknown); QVERIFY(!face.channelHasReading(2)); face.pushBindingValue(201,0);face.advanceMeter(500);QVERIFY(face.channelHasReading(2));
        QCOMPARE(face.serialize(),saved);
        // Untyped legacy clients still render finite real samples; empty Unknown stays inactive.
        AnanMultiMeterItem untyped; auto standalone=configuredChannels(untyped);
        auto signal=standalone[0].toObject();signal["showHistory"]=false; standalone[0]=signal;
        QVERIFY(untyped.applyConfiguration({{"channels",standalone},{"showReadout",false}}));
        const auto overlay=[&] {QImage image(720,300,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter p(&image);untyped.paintForLayer(p,720,300,MeterItem::Layer::OverlayDynamic);return image;};
        const QImage absent=overlay(); untyped.pushBindingValue(1,-85);untyped.advanceMeter(0);QVERIFY(overlay()!=absent);
    }
    void ananGroupControlTracksPaintedFace() {
        for(const QSize size:{QSize(360,300),QSize(720,300),QSize(1440,600),QSize(720,180),QSize(360,600)}) {
            AnanMultiMeterItem face; face.resetForTxTransition(true);
            const QRectF box=face.ananGroupControlRect(size.width(),size.height()); QVERIFY(QRectF(QPointF(),size).contains(box));
            QMouseEvent outside(QEvent::MouseButtonPress,box.topLeft()-QPointF(1,1),box.topLeft()-QPointF(1,1),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QVERIFY(!face.handleMousePress(&outside,size.width(),size.height())); QCOMPARE(face.configuration()["displayGroup"].toInt(),1);
            QMouseEvent inside(QEvent::MouseButtonPress,box.topLeft()+QPointF(1,1),box.topLeft()+QPointF(1,1),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QVERIFY(face.handleMousePress(&inside,size.width(),size.height())); QCOMPARE(face.configuration()["displayGroup"].toInt(),2);
            face.setPreviewInert(true); QVERIFY(!face.handleMousePress(&inside,size.width(),size.height())); QCOMPARE(face.configuration()["displayGroup"].toInt(),2);
        }
    }
    void ananMarksUseActualNeedleTips() {
        AnanMultiMeterItem face; QVERIFY(face.applyConfiguration({{"showReadout",false},{"showTitle",false}}));
        for(int binding:face.readingBindings()) { face.setBindingSupport(binding,MeterItem::BindingSupport::Supported); }
        struct Anchor { int channel; double raw,x,y; };
        // Oracle is the approved native1855×848 artwork/angle endpoints; independent of product geometry helpers.
        const QList<Anchor> anchors{{0,-127,0.117059794,0.380134788},{0,-13,0.821082459,0.342611449},{1,10,0.588119078,0.765593954},{1,15,0.757433483,0.765593954},{2,0,0.245534088,0.565355119},{2,20,0.780596618,0.582616549},{3,0,0.232210013,0.378065912},{3,150,0.792515383,0.397614569},{4,1,0.239802545,0.476981032},{4,10,0.783945260,0.492619957},{5,0,0.269329015,0.646806982},{5,30,0.733066814,0.652260701},{6,-30,0.225121668,0.741962507},{6,25,0.387347589,0.766773199}};
        for(int dpr:{1,2}) {
            const QRectF r=face.ananNeedleRect(720,300);
            QImage image(QSize(720,300)*dpr,QImage::Format_ARGB32_Premultiplied);image.setDevicePixelRatio(dpr);image.fill(Qt::transparent);
            {QPainter p(&image);face.paintForLayer(p,720,300,MeterItem::Layer::Background);}
            for(const Anchor& a:anchors) {
                const QPointF expected=r.topLeft()+QPointF(a.x*r.width(),a.y*r.height());
                QVERIFY(QLineF(expected,face.needleTip(a.channel,a.raw,r)).length()<.01);
                bool mark=false;const QPoint at=(expected*dpr).toPoint();
                for(int y=-2*dpr;y<=2*dpr;++y) {for(int x=-2*dpr;x<=2*dpr;++x) {const QColor ink=image.pixelColor(at+QPoint(x,y));if(ink.lightness()>85 || ink.red()>120) {mark=true;}}}
                QVERIFY2(mark,qPrintable(QString("Missing source mark channel%1 value%2").arg(a.channel).arg(a.raw)));
            }
        }
        // Source targets only establish direction; interpolation/clamping,
        // power normalization and HF offsets stay independent of face styling.
        QCOMPARE(face.calibratedPoint(0,-88),QPointF(.394,.164));
        QCOMPARE(face.calibratedPoint(1,-100),QPointF(.559,.756)); QCOMPARE(face.calibratedPoint(1,100),QPointF(.665,.784));
        face.setAboveS9Frequency(true); QCOMPARE(face.calibratedPoint(0,-93),QPointF(.501,.142));
        for(int rating:{5,100,500}) {face.setPowerScale(rating);QCOMPARE(face.calibratedPoint(3,rating*.5),QPointF(.499,.212));}
    }
    void ananNeedlesStartAtApprovedVisibleOrbs() {
        AnanMultiMeterItem face;
        const QString unchanged=face.serialize();
        for(const QSize size:{QSize(360,300),QSize(720,420),QSize(1440,840),QSize(720,180),QSize(360,600)}) {
            const QRectF skin=face.ananNeedleRect(size.width(),size.height());
            QVERIFY(skin.width()>0); QVERIFY(skin.height()>0);
            QVERIFY(qAbs(skin.height()/skin.width()-848.0/1855.0)<1e-9);
            QVERIFY(QRectF(QPointF(),size).contains(skin));
            for(int channel=0;channel<7;++channel) {
                const QPointF art=channel==1?QPointF(1248,736):channel==6?QPointF(550,737):QPointF(919,740);
                const QPointF expected=skin.topLeft()+QPointF(art.x()/1855*skin.width(),art.y()/848*skin.height());
                QVERIFY2(QLineF(expected,face.needlePivot(channel,skin)).length()<.001,qPrintable(QString("channel%1 origin differs from visible orb").arg(channel)));
            }
        }
        QCOMPARE(face.serialize(),unchanged);
        auto channels=face.configuration()["channels"].toArray();
        auto alc=channels[6].toObject(); alc["offsetX"]=.104; alc["offsetY"]=.636;
        alc["futureOrbMetadata"]=QJsonObject{{"keep",true}}; channels[6]=alc;
        QVERIFY(face.applyConfiguration({{"channels",channels}}));
        const QRectF skin=face.ananNeedleRect(720,420);
        // Edited offsets retain the original source-frame .512 aspect, independently of skin aspect.
        const QPointF expected=skin.topLeft()+QPointF((550.0/1855+.1)*skin.width(),737.0/848*skin.height()-.1*.512*skin.width());
        QVERIFY(QLineF(expected,face.needlePivot(6,skin)).length()<.001);
        const QString custom=face.serialize(); AnanMultiMeterItem loaded;
        QVERIFY(loaded.deserialize(custom)); QCOMPARE(loaded.serialize(),custom);
        QCOMPARE(loaded.configuration()["channels"].toArray()[6].toObject()["futureOrbMetadata"],alc["futureOrbMetadata"]);
    }
    void paletteRoundtrips() {
        ContainerContentRegistry registry; int complete=0;
        for(const auto& descriptor:registry.descriptors()) {
            if(!descriptor.typeId.startsWith("meter.")) { continue; }
            ContentEntry entry=registry.makeEntry(descriptor.typeId);
            if(descriptor.typeId=="meter.adcMax") { QVERIFY(!descriptor.available); QVERIFY(!registry.createMeterItem(entry,nullptr)); QVERIFY(descriptor.unavailableReason.contains("magnitude")); continue; }
            std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr)); QVERIFY2(item,qPrintable(descriptor.typeId));
            if(auto* face=qobject_cast<CompositePresetItem*>(item.get())) {
                auto edit=face->configuration(); if(face->editableFields().contains("title")) { edit["title"]="Edited | title"; } if(face->editableFields().contains("backdropColor")) { edit["backdropColor"]="#ff102030"; } edit["future"]=QJsonObject{{"keep",true}}; edit["faceHeight"]=320;
                QVERIFY(face->applyConfiguration(edit)); QCOMPARE(face->preferredFaceHeight(),320); QVERIFY(face->minimumFaceSize().width()>=260);
                const QString before=face->serialize(); QVERIFY(!face->applyConfiguration({{"w",-1}})); QCOMPARE(face->serialize(),before);
                if(!edit["channels"].toArray().isEmpty()) { auto channels=edit["channels"].toArray(); auto c=channels[0].toObject(); c["attack"]=2; channels[0]=c; QVERIFY(!face->applyConfiguration({{"channels",channels}})); QCOMPARE(face->serialize(),before); }
            } else if(auto* face=qobject_cast<BarPresetItem*>(item.get())) { QVERIFY(face->applyConfiguration({{"label","Edited | title"},{"titleColor","#ffff0000"},{"attack",.4},{"decay",.2},{"future",QJsonObject{{"keep",true}}}})); }
            ContentEntry saved=registry.captureMeterItem(*item,entry); QCOMPARE(saved.config["legacyRecord"],entry.config["legacyRecord"]);
            std::unique_ptr<MeterItem> copy(registry.createMeterItem(saved,nullptr)); QVERIFY(copy); QCOMPARE(copy->serialize(),item->serialize());
            std::unique_ptr<ItemGroup> group(ItemGroup::createCompletePreset(descriptor.typeId)); QCOMPARE(group->items().size(),1); ++complete;
        }
        QVERIFY(complete>=29);
    }
    void allBarCalibrationsAndAbsence() {
        struct Row { const char* flavor; int primary,secondary; double low,middle,high,x; const char* units; };
        // Numeric oracle transcribed from the saved source audit, never generated by product helpers.
        const QList<Row> rows{{"Comp",115,104,-30,0,12,.665,"dB"},{"Eq",116,106,-30,0,12,.665,"dB"},{"Leveler",117,107,-30,0,12,.665,"dB"},{"Cfc",118,111,-30,0,12,.665,"dB"},{"CfcGain",112,-1,0,20,25,.8,"dB"},{"LevelerGain",108,-1,0,20,25,.8,"dB"},{"AlcGain",109,-1,0,20,25,.8,"dB"},{"AlcGroup",110,-1,-30,0,25,.5,"dB"},{"Agc",5,6,-125,0,125,.5,"dB"},{"AgcGain",4,-1,-50,100,125,.857,"dB"},{"Signal",0,-1,-133,-73,-13,.5,"dBm"},{"SignalAvg",1,-1,-133,-73,-13,.5,"dBm"},{"SignalMaxBin",7,-1,-133,-73,-13,.5,"dBm"},{"Adc",2,3,-120,-20,0,.8333,"dBFS"},{"AdcMax",-1,-1,0,25000,32768,.8333,""},{"PbSnr",8,-1,0,50,60,.8333,"dB"}};
        for(const Row& row:rows) { BarPresetItem face; QVERIFY(face.configureVariant(row.flavor)); QCOMPARE(face.bindingId(),row.primary); QCOMPARE(face.configuration()["secondaryBindingId"].toInt(),row.secondary); QCOMPARE(face.calibratedPosition(row.low),0.0); QVERIFY(qAbs(face.calibratedPosition(row.middle)-row.x)<1e-10); QCOMPARE(face.calibratedPosition(row.high),.99); QCOMPARE(face.configuration()["units"].toString(),QString::fromLatin1(row.units));
            if(row.primary>=0) { face.pushBindingValue(row.primary,-400); face.advanceMeter(0); QVERIFY(!face.hasPrimaryReading()); face.pushBindingValue(row.primary,row.middle); face.advanceMeter(100); QVERIFY(face.hasPrimaryReading()); }
        }
        for(bool mic:{true,false}) { BarPresetItem face; if(mic) { face.configureAsMic(); } else { face.configureAsAlc(); } const int average=mic?103:105,peak=mic?113:114;
            face.pushBindingValue(peak,-400); face.pushBindingValue(average,-400); face.advanceMeter(0); QVERIFY(!face.hasPrimaryReading()); QVERIFY(!face.hasAverageReading());
            face.setBindingUnavailable(peak,"Remote independent peak unavailable"); face.pushBindingValue(average,-12); face.advanceMeter(100); QVERIFY(face.hasAverageReading()); QVERIFY(!face.hasPrimaryReading());
            face.resetForTxTransition(false); face.pushBindingValue(average,-400); face.advanceMeter(200); QVERIFY(!face.hasAverageReading());
        }
        BarPresetItem locked; locked.configureAsMic(); QVERIFY(locked.applyConfiguration({{"minValue",-50},{"maxValue",50}})); QCOMPARE(locked.calibratedPosition(-30),0.0); QCOMPARE(locked.calibratedPosition(0),.665); QCOMPARE(locked.calibratedPosition(12),.99);
        BarPresetItem custom; custom.configureAsCustom(999,-500,10,"Custom"); custom.pushBindingValue(999,-400); custom.advanceMeter(0); QVERIFY(custom.hasPrimaryReading());
    }
    void familySourceTraces() {
        PowerSwrPresetItem power; CrossNeedleItem cross; AnanMultiMeterItem anan; MagicEyePresetItem eye;
        QCOMPARE(power.readingBindings(),(QSet<int>{100,102})); QCOMPARE(cross.readingBindings(),(QSet<int>{100,101})); QCOMPARE(anan.readingBindings(),(QSet<int>{1,200,201,100,102,109,110}));
        for(auto* face:{static_cast<CompositePresetItem*>(&power),static_cast<CompositePresetItem*>(&cross),static_cast<CompositePresetItem*>(&anan),static_cast<CompositePresetItem*>(&eye)}) { QVERIFY(face->applyConfiguration({{"channels",configuredChannels(*face)}})); }
        power.pushBindingValue(100,100); power.pushBindingValue(102,3); power.advanceMeter(0); QCOMPARE(power.channelValue(0),80.0); QCOMPARE(power.channelValue(1),2.6);
        power.pushBindingValue(100,0); power.advanceMeter(100); QCOMPARE(power.channelValue(0),72.0); QCOMPARE(power.channelPeak(0),80.0);
        cross.pushBindingValue(100,100); cross.pushBindingValue(101,10); cross.advanceMeter(0); QCOMPARE(cross.channelValue(0),20.0); QCOMPARE(cross.channelValue(1),2.0);
        cross.pushBindingValue(100,0); cross.advanceMeter(100); QCOMPARE(cross.channelValue(0),18.0); QCOMPARE(cross.channelPeak(0),20.0);
        eye.pushBindingValue(1,-73); eye.advanceMeter(0); QVERIFY(qAbs(eye.channelValue(0)+116.2)<1e-10); eye.pushBindingValue(1,-127); eye.advanceMeter(100); QVERIFY(qAbs(eye.channelValue(0)+116.74)<1e-10);
        anan.pushBindingValue(1,-73); anan.pushBindingValue(200,15); anan.pushBindingValue(201,10); anan.pushBindingValue(100,100); anan.pushBindingValue(110,0); anan.advanceMeter(0);
        QCOMPARE(anan.channelValue(0),-83.8); QCOMPARE(anan.channelValue(1),11.0); QCOMPARE(anan.channelValue(2),2.0); QCOMPARE(anan.channelValue(3),20.0); QCOMPARE(anan.channelValue(6),-24.0);
        QVERIFY(anan.channelVisible(0)); QVERIFY(!anan.channelVisible(3)); anan.resetForTxTransition(true); QVERIFY(!anan.channelVisible(0)); QVERIFY(anan.channelVisible(3)); QVERIFY(!anan.channelHasReading(3));
        QVERIFY(anan.applyConfiguration({{"displayGroup",3}})); QVERIFY(anan.channelVisible(6)); QVERIFY(!anan.channelVisible(3)); QCOMPARE(anan.calibratedPoint(1,12.5),QPointF(.605,.772)); QCOMPARE(anan.calibratedPoint(6,-30),QPointF(.295,.804));
        for(int rating:{1,5,10,30,100,200,500}) { power.setPowerScale(rating); QCOMPARE(power.calibratedPoint(0,rating),QPointF(.75,0)); cross.setPowerScale(rating); QCOMPARE(cross.calibratedPoint(0,rating),QPointF(.662,.083)); }
        power.setPowerScale(5); QCOMPARE(power.calibratedPoint(0,.25),QPointF(.1875,0));
        cross.setPowerScale(100); const QRectF r(0,0,400,312.8); const QPointF pivot=cross.needlePivot(0,r); QCOMPARE(pivot,QPointF(328.8,347.5208));
        // Source angular algorithm with width-derived radii: independent oracle at 0W.
        const double angle=std::atan2(347.5208-.732*312.8,328.8-.052*400)+M_PI;
        const QPointF expected=pivot+QPointF(std::cos(angle)*324,std::sin(angle)*324); QVERIFY(QLineF(expected,cross.needleTip(0,0,r)).length()<1e-9);
        cross.setBindingUnavailable(101,"No reflected sample"); QVERIFY(!cross.channelHasReading(1)); QVERIFY(cross.channelHasReading(0));
    }
    void signalFrequencyAndConcreteControls() {
        SignalTextPresetItem signal; signal.setUnitMode(MeterItem::MeterUnit::S); QCOMPARE(signal.signalReadout(-73),QStringLiteral("S9")); signal.setAboveS9Frequency(true); QCOMPARE(signal.signalReadout(-93),QStringLiteral("S9")); QCOMPARE(signal.signalReadout(-66),QStringLiteral("S9+20"));
        ContestPresetItem contest; QCOMPARE(contest.internalItems().size(),4); QVERIFY(contest.vfoDisplay()->unavailableText().contains("No live")); contest.setFrequency(14200123); QCOMPARE(contest.vfoDisplay()->frequency(),qint64(14200123)); contest.setUnavailableText({});
        RadioModel model; model.addSlice(); model.addSlice(); model.setActiveSliceById(1);
        auto* a=model.sliceById(0); auto* b=model.sliceById(1); a->setFrequency(14100000); b->setFrequency(3700000);
        ContainerButtonDispatcher::Hooks hooks; bool held=false; hooks.sliceRefusal=[&](int) { return held?QStringLiteral("This slice is held by another device"):QString(); };
        ContainerButtonDispatcher dispatcher(&model,std::move(hooks)); dispatcher.applySliceAvailability(contest.bandButtons(),1); dispatcher.applySliceAvailability(contest.modeButtons(),1);
        QObject::connect(contest.bandButtons(),&BandButtonItem::bandClicked,&contest,[&](int band) { dispatcher.clickBand(band,1); });
        QObject::connect(contest.modeButtons(),&ModeButtonItem::modeClicked,&contest,[&](int mode) { if(dispatcher.sliceFor(1)) { a->setDspMode(SliceModel::modeFromName(ModeButtonItem::modeLabel(mode))); } });
        QSignalSpy bands(contest.bandButtons(),&BandButtonItem::bandClicked),modes(contest.modeButtons(),&ModeButtonItem::modeClicked);
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(35,98),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),release(QEvent::MouseButtonRelease,QPointF(35,98),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QVERIFY(contest.handleMousePress(&press,360,280)); QVERIFY(contest.handleMouseRelease(&release,360,280)); QCOMPARE(bands.count(),1); QCOMPARE(bandFromFrequency(a->frequency()),Band::Band160m); QCOMPARE(b->frequency(),3700000.0);
        held=true; dispatcher.applySliceAvailability(contest.bandButtons(),1); contest.handleMousePress(&press,360,280); contest.handleMouseRelease(&release,360,280); QCOMPARE(bands.count(),1); held=false; dispatcher.applySliceAvailability(contest.bandButtons(),1);
        QMouseEvent mp(QEvent::MouseButtonPress,QPointF(35,195),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),mr(QEvent::MouseButtonRelease,QPointF(35,195),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QVERIFY(contest.handleMousePress(&mp,360,280)); QVERIFY(contest.handleMouseRelease(&mr,360,280)); QCOMPARE(modes.count(),1); QCOMPARE(a->dspMode(),DSPMode::LSB);
        contest.setPreviewInert(true); QVERIFY(!contest.handleMousePress(&press,360,280)); contest.bandButtons()->handleMousePress(&press,360,280); contest.bandButtons()->handleMouseRelease(&release,360,280); QCOMPARE(bands.count(),1); QVERIFY(contest.bandButtons()->signalsBlocked());
    }
    void pollerReplayAbsenceAndMmio() {
        RadioModel model(RadioModel::Role::Remote); model.setStationConnectionState(ConnectionState::Connected);
        MeterPoller poller; poller.setPaReadingsModel(&model); poller.setRadioStatus(&model.radioStatus());
        MeterWidget live, replacement; auto* power=new PowerSwrPresetItem(&live); live.addItem(power); poller.addTarget(&live);
        poller.setInTx(true); model.radioStatus().setPowerReadings(50,2,1.5); QVERIFY(QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection)); live.advanceMeters(0); QVERIFY(power->channelHasReading(0));
        poller.setInTx(false); poller.removeTarget(&live);
        auto* fresh=new PowerSwrPresetItem(&replacement); replacement.addItem(fresh); auto* mic=new BarPresetItem(&replacement); mic->configureAsMic(); replacement.addItem(mic); poller.addTarget(&replacement); replacement.advanceMeters(100);
        QVERIFY(!fresh->channelHasReading(0)); QVERIFY(!fresh->channelHasReading(1)); QVERIFY(!mic->hasPrimaryReading()); QVERIFY(!mic->hasAverageReading());
        model.setStationConnectionState(ConnectionState::Disconnected); poller.replayReadings(&replacement,{}); replacement.advanceMeters(200); QVERIFY(!fresh->channelHasReading(0));
        BarPresetItem custom; custom.configureAsCustom(MeterBinding::SignalPeak,-500,100,"MMIO"); custom.setMmioBinding(QUuid::createUuid(),"Reading"); custom.pushBindingValue(MeterBinding::SignalPeak,-400); custom.advanceMeter(0); QVERIFY(custom.hasPrimaryReading()); QVERIFY(!custom.isNoReading(-400)); custom.setBindingUnavailable(MeterBinding::SignalPeak,"Missing numeric MMIO source"); QVERIFY(!custom.hasPrimaryReading());
    }
    void staticChangesAreConsumedOnce() {
        PowerSwrPresetItem power; QVERIFY(power.takeStaticPresentationChange()); QVERIFY(!power.takeStaticPresentationChange()); QVERIFY(power.applyConfiguration({{"backdropColor","#ff102030"}})); QVERIFY(power.takeStaticPresentationChange()); QVERIFY(!power.takeStaticPresentationChange());
        power.pushBindingValue(100,50); power.advanceMeter(0); QVERIFY(!power.takeStaticPresentationChange()); power.setPowerScale(5); QVERIFY(power.takeStaticPresentationChange()); power.setPowerScale(5); QVERIFY(!power.takeStaticPresentationChange());
        AnanMultiMeterItem anan; anan.takeStaticPresentationChange(); anan.setAboveS9Frequency(true); QVERIFY(anan.takeStaticPresentationChange()); anan.setAboveS9Frequency(true); QVERIFY(!anan.takeStaticPresentationChange()); QVERIFY(anan.applyConfiguration({{"displayGroup",2}})); QVERIFY(anan.takeStaticPresentationChange());
        BarPresetItem bar; bar.configureAsMic(); bar.takeStaticPresentationChange(); QVERIFY(bar.applyConfiguration({{"titleColor","#ffff0000"}})); QVERIFY(bar.takeStaticPresentationChange()); QVERIFY(!bar.takeStaticPresentationChange());
        auto channels=power.configuration()["channels"].toArray(); auto first=channels[0].toObject(); first["bindingId"]=MeterBinding::TxReversePower; channels[0]=first; QVERIFY(power.applyConfiguration({{"channels",channels}})); QCOMPARE(power.readingBindings(),(QSet<int>{101,102}));
    }
    void historySamplingOutlivesFasterSharedFrames() {
        CompositePresetItem history(CompositePresetItem::Face::History);
        auto channels=configuredChannels(history); auto c=channels[0].toObject();
        c["attack"]=1; c["decay"]=1; c["updateIntervalMs"]=250; c["color"]="#ff00ff00"; channels[0]=c;
        QVERIFY(history.applyConfiguration({{"channels",channels},{"historyMs",1200},{"historyCapacity",4},{"autoScale",false},{"minValue",-140},{"maxValue",0},{"showReadout",false},{"faceHeight",240}}));
        auto trace=[&](int right=640) {
            QImage image(640,240,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
            QPainter painter(&image); history.paintForLayer(painter,640,240,MeterItem::Layer::OverlayDynamic); painter.end();
            int pixels=0; for(int y=0;y<image.height();++y) { for(int x=0;x<right;++x) { const QColor pixel=image.pixelColor(x,y); if(pixel.green()>200 && pixel.red()<30 && pixel.blue()<30) {++pixels;} } }
            return pixels;
        };
        // Independent timeline: 250 ms sampling on 100 ms shared ticks retains
        // readings at 0,300,600,900, rather than restarting the gate every tick.
        for(int now=0;now<=900;now+=100) {
            const double value=now<300?-120:now<600?-60:now<900?-90:-30;
            history.pushBindingValue(MeterBinding::SignalAvg,value); history.advanceMeter(now);
            if(now==200) {QCOMPARE(trace(),0);}
            if(now==300) {QVERIFY2(trace()>100,"History must draw its second sample despite faster shared frames");}
            if(now==600) {QVERIFY(trace()>200);}
            if(now==900) {QVERIFY(trace()>300);}
        }
        history.setBindingUnavailable(MeterBinding::SignalAvg,"No live reading");
        history.advanceMeter(1000); QVERIFY(trace()>300); // Absence adds no invented sample.
        history.advanceMeter(1799); QVERIFY(trace()>100); // Last two real samples remain.
        history.advanceMeter(2100); QCOMPARE(trace(),0); // Exact duration expires the final sample.
        history.setBindingUnavailable(MeterBinding::SignalAvg,{});
        history.pushBindingValue(MeterBinding::SignalAvg,-80); history.advanceMeter(2200);
        history.pushBindingValue(MeterBinding::SignalAvg,-40); history.advanceMeter(2500); QVERIFY(trace()>100);
        history.resetForTxTransition(true); QCOMPARE(trace(),0); QVERIFY(!history.channelHasReading(0));
        history.pushBindingValue(MeterBinding::SignalAvg,-100); history.advanceMeter(2600);
        history.pushBindingValue(MeterBinding::SignalAvg,-40); history.advanceMeter(2900); QVERIFY(trace()>100);
        history.advanceMeter(50); QCOMPARE(trace(),0); // Rollback drops future trace points.
        history.advanceMeter(350); QVERIFY(trace()>100);
        // A source/configuration reset also starts a new history timeline.
        QVERIFY(history.applyConfiguration({{"historyCapacity",2}})); QCOMPARE(trace(),0);
        for(int now=3000;now<=3900;now+=300) {history.pushBindingValue(MeterBinding::SignalAvg,now==3900?-30:-100);history.advanceMeter(now);}
        QVERIFY(trace()>100); QCOMPARE(trace(475),0); // Capacity retains only 3600→3900, at the right edge.
    }
    void primitiveClockSharesCadence() {
        ContainerContentRegistry registry; MeterWidget widget; widget.resize(360,120); auto* item=registry.createMeterItem(registry.makeEntry("CLOCK"),&widget); QVERIFY(item); widget.addItem(item); widget.show();
#ifdef NEREUS_GPU_SPECTRUM
        QSignalSpy submitted(&widget,&QRhiWidget::frameSubmitted); widget.advanceMeters(0); const int before=submitted.count(); widget.update(); QTRY_VERIFY_WITH_TIMEOUT(submitted.count()>before,2000); const QImage first=widget.grabFramebuffer();
#else
        widget.advanceMeters(0); QTest::qWait(50); const QImage first=widget.grab().toImage();
#endif
        QVERIFY(!item->advanceMeter(100)); QTest::qWait(1100); widget.advanceMeters(1100);
#ifdef NEREUS_GPU_SPECTRUM
        const int next=submitted.count(); widget.update(); QTRY_VERIFY_WITH_TIMEOUT(submitted.count()>next,2000); const QImage last=widget.grabFramebuffer();
#else
        widget.update(); QTest::qWait(30); const QImage last=widget.grab().toImage();
#endif
        QVERIFY(!first.isNull()); QVERIFY(first!=last);
    }
    void nativeFamilyFrames() {
        const QString destination=qEnvironmentVariable("NEREUS_METER_CAPTURE_DIR"); ContainerContentRegistry registry;
        for(const QString& type:{"meter.powerSwr","meter.crossNeedle","meter.ananMulti","meter.magicEye","meter.signalText","meter.historyGraph","meter.vfoDisplay","meter.clock","meter.contest","meter.eq"}) {
            for(int width:{360,640}) {
                MeterWidget widget; ContentEntry entry=registry.makeEntry(type); auto* item=registry.createMeterItem(entry,&widget); QVERIFY(item);
                auto* face=qobject_cast<CompositePresetItem*>(item); if(face) { QVERIFY(face->applyConfiguration({{"channels",configuredChannels(*face)}})); if(type=="meter.ananMulti") { face->resetForTxTransition(true); } face->setFrequency(14200123); face->setModeLabel("USB"); face->setBandLabel("20m"); }
                widget.resize(width,face?face->preferredFaceHeight():72); widget.addItem(item); if(type=="meter.ananMulti") { widget.resetForTxTransition(true); for(int binding:item->readingBindings()) { widget.setBindingSupport(binding,MeterItem::BindingSupport::Supported); } } widget.show(); QTest::qWait(100);
#ifdef NEREUS_GPU_SPECTRUM
                QSignalSpy submitted(&widget,&QRhiWidget::frameSubmitted);
#endif
                QImage first,last;
                for(int frame=0;frame<12;++frame) {
                    for(int binding:item->readingBindings()) { const double value=binding==1 ? -110+frame*4 : binding==200 ? 13.8 : binding==201 ? 6 : binding==102 ? 1+frame*.15 : binding>=100 && binding<=101 ? frame*9 : binding==110 ? -30+frame*3 : -25+frame*3; widget.updateMeterValue(binding,value); }
                    widget.advanceMeters(frame*100);
#ifdef NEREUS_GPU_SPECTRUM
                    const int before=submitted.count(); widget.update(); QTRY_VERIFY_WITH_TIMEOUT(submitted.count()>before,2000); const QImage image=widget.grabFramebuffer();
#else
                    widget.update(); QTest::qWait(10); const QImage image=widget.grab().toImage();
#endif
                    QVERIFY(!image.isNull());
                    if(type=="meter.ananMulti" && frame==11) {
                        // A native DPR reference catches logical-resolution background upscaling.
                        QImage reference(image.size(),QImage::Format_RGBA8888); reference.setDevicePixelRatio(widget.devicePixelRatioF()); reference.fill(QColor(15,15,26));
                        { QPainter painter(&reference); face->paintForLayer(painter,width,widget.height(),MeterItem::Layer::Background); }
                        int mismatches=0,total=0; const double dpr=widget.devicePixelRatioF();
                        for(int y=0;y<qRound(22*dpr);++y) { for(int x=qRound(35*dpr);x<qRound((width-110)*dpr);++x) { const QColor actual=image.pixelColor(x,y),expected=reference.pixelColor(x,y); ++total; if(qAbs(actual.red()-expected.red())>2 || qAbs(actual.green()-expected.green())>2 || qAbs(actual.blue()-expected.blue())>2) { ++mismatches; } } }
                        QVERIFY2(mismatches<total/100,qPrintable(QString("Static title DPR mismatches %1/%2").arg(mismatches).arg(total)));
                    }
                    if(type=="meter.powerSwr" && frame==11) {
                        const double scale=double(image.width())/width;
                        // Independent .8 recurrence: inputs 0,9,...99 => 96.75000009216W.
                        const double position=.5625+(96.75000009216-50)/50*.1875;
                        const QColor marker=image.pixelColor(qRound((18+(width-36)*position)*scale),qRound(48*scale)); QVERIFY(marker.red()>200 && marker.green()>200 && marker.blue()<50);
                        const QColor band=image.pixelColor(qRound((18+(width-36)*.45)*scale),qRound(29*scale)); QVERIFY(qAbs(band.red()-144)<=3 && qAbs(band.green()-16)<=3 && qAbs(band.blue()-16)<=3);
                    }
                    if(frame==0) { first=image; } last=image;
                    if(!destination.isEmpty() && (frame==0 || frame==11)) { QDir().mkpath(destination); QVERIFY(image.save(destination+"/"+type.mid(6)+"-"+QString::number(width)+"-"+QString::number(frame)+".png")); }
                }
                if(!item->readingBindings().isEmpty()) { QVERIFY(first!=last); }
                if(face && face->vfoDisplay()) {
                    face->setFrequency(7150123); widget.advanceMeters(1300);
#ifdef NEREUS_GPU_SPECTRUM
                    const int before=submitted.count(); widget.update(); QTRY_VERIFY_WITH_TIMEOUT(submitted.count()>before,2000); const QImage changed=widget.grabFramebuffer();
#else
                    widget.update(); QTest::qWait(15); const QImage changed=widget.grab().toImage();
#endif
                    QVERIFY(changed!=last);
                }
                if(face) { QJsonObject changes; if(face->editableFields().contains("backdropColor")) { changes["backdropColor"]="#ff102030"; } if(face->editableFields().contains("titleColor")) { changes["titleColor"]="#ffff0000"; } QVERIFY(face->applyConfiguration(changes)); widget.advanceMeters(1500);
#ifdef NEREUS_GPU_SPECTRUM
                    const int before=submitted.count(); widget.update(); QTRY_VERIFY_WITH_TIMEOUT(submitted.count()>before,2000); const QImage customized=widget.grabFramebuffer();
#else
                    widget.update(); QTest::qWait(15); const QImage customized=widget.grab().toImage();
#endif
                    if(changes.contains("backdropColor")) { QCOMPARE(customized.pixelColor(1,customized.height()-2),QColor(16,32,48)); QVERIFY(customized!=last); }
                    if(!destination.isEmpty()) { QVERIFY(customized.save(destination+"/"+type.mid(6)+"-"+QString::number(width)+"-customized.png")); }
                }
                const int staticChecks=type=="meter.ananMulti" ? 2 : (type=="meter.powerSwr" || type=="meter.eq" ? 1 : 0);
                for(int check=0;check<staticChecks;++check) {
                    if(type=="meter.ananMulti" && check==1) {
                        widget.resetForTxTransition(false); widget.updateMeterValue(MeterBinding::SignalAvg,-85); widget.advanceMeters(1650);
#ifdef NEREUS_GPU_SPECTRUM
                        const int before=submitted.count(); widget.update(); QTRY_VERIFY_WITH_TIMEOUT(submitted.count()>before,2000);
#else
                        widget.update(); QTest::qWait(15);
#endif
                    }
#ifdef NEREUS_GPU_SPECTRUM
                    const QImage previous=widget.grabFramebuffer();
#else
                    const QImage previous=widget.grab().toImage();
#endif
                    if(type=="meter.powerSwr") { face->setPowerScale(5); }
                    else if(type=="meter.ananMulti" && check==0) { QVERIFY(face->applyConfiguration({{"displayGroup",2}})); }
                    else if(type=="meter.ananMulti") { face->setAboveS9Frequency(true); }
                    else { QVERIFY(qobject_cast<BarPresetItem*>(item)->applyConfiguration({{"titleColor","#ffff0000"}})); }
                    widget.advanceMeters(1600+check*100);
#ifdef NEREUS_GPU_SPECTRUM
                    const int before=submitted.count(); widget.update(); QTRY_VERIFY_WITH_TIMEOUT(submitted.count()>before,2000); const QImage changed=widget.grabFramebuffer();
#else
                    widget.update(); QTest::qWait(15); const QImage changed=widget.grab().toImage();
#endif
                    QVERIFY2(changed!=previous,qPrintable(type+" static check "+QString::number(check))); QVERIFY(!item->takeStaticPresentationChange());
                }
                qInfo()<<"Native"<<QGuiApplication::platformName()<<type<<width<<"DPR"<<widget.devicePixelRatioF();
#ifdef NEREUS_GPU_SPECTRUM
                qInfo()<<"QRhi API"<<int(widget.api())<<"frames"<<submitted.count();
#endif
            }
        }
    }
};
QTEST_MAIN(TestCompositePresets)
#include "tst_meter_composite_presets.moc"
