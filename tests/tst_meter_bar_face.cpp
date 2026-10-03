// no-port-check: Independent source trace and native renderer checks.
#include <QtTest>
#include <QPainter>
#include <QDir>
#include <QScreen>
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/ItemGroup.h"
#include <QJsonDocument>
#include "gui/containers/ContainerContentRegistry.h"
using namespace NereusSDR;
class TestBarFace : public QObject {
    Q_OBJECT
private slots:
    void calibrationAndTrace() {
        BarPresetItem face; face.configureAsMic();
        QCOMPARE(face.readingBindings(),(QSet<int>{103,113}));
        QCOMPARE(face.calibratedPosition(-30),0.0); QCOMPARE(face.calibratedPosition(0),.665); QCOMPARE(face.calibratedPosition(12),.99);
        QCOMPARE(face.calibratedPosition(-195),0.0); QCOMPARE(face.calibratedPosition(195),.99);
        QFile trace(QFINDTESTDATA("fixtures/meters/mic-alc-source-trace.csv")); QVERIFY(trace.open(QIODevice::ReadOnly));
        while(!trace.atEnd()) {
            const QByteArray line=trace.readLine(); if(line.startsWith('#')) { continue; }
            const QList<QByteArray> fields=line.trimmed().split(','); QCOMPARE(fields.size(),5);
            face.pushBindingValue(113,fields[1].toDouble()); face.pushBindingValue(103,fields[2].toDouble()); face.advanceMeter(fields[0].toLongLong());
            QVERIFY(qAbs(face.primaryValue()-fields[3].toDouble())<1e-8); QVERIFY(qAbs(face.averageValue()-fields[4].toDouble())<1e-8);
        }
        face.resetForTxTransition(false); QVERIFY(!face.hasPrimaryReading()); QCOMPARE(face.primaryValue(),-30.0);
        face.configureAsAlc(); QCOMPARE(face.readingBindings(),(QSet<int>{105,114}));
    }
    void invalidEditsAreAtomic() {
        BarPresetItem face; face.configureAsMic(); const QString before=face.serialize();
        for(const QJsonObject& invalid:{QJsonObject{{"label","changed"},{"style","wrong"}},QJsonObject{{"bindingId",113.5}},QJsonObject{{"w",-1}},QJsonObject{{"barColor","invalid"}},QJsonObject{{"ignoreHistoryMs",-1}}}) {
            QVERIFY(!face.applyConfiguration(invalid)); QCOMPARE(face.serialize(),before);
        }
        QVERIFY(face.applyConfiguration({{"rowHeight",90}})); QCOMPARE(face.preferredFaceHeight(),90); QCOMPARE(face.minimumFaceSize(),QSize(260,90));
    }
    void historyAndUnavailable() {
        BarPresetItem face; face.configureAsMic(); QVERIFY(face.applyConfiguration({{"ignoreHistoryMs",0},{"historyMs",200},{"peakHold",true}}));
        face.pushBindingValue(113,0); face.advanceMeter(0); face.pushBindingValue(113,-30); face.advanceMeter(100);
        QCOMPARE(face.peakValue(),-6.0); QVERIFY(face.minimumHistory()<-6);
        face.advanceMeter(300); QVERIFY(face.peakValue()<-6);
        face.setBindingUnavailable(113,"Remote has no independent peak"); QVERIFY(!face.hasPrimaryReading());
        face.pushBindingValue(103,-12); face.advanceMeter(400); QVERIFY(face.hasAverageReading());
        face.setBindingUnavailable(113,{}); face.pushBindingValue(113,0); face.advanceMeter(500); QVERIFY(face.hasPrimaryReading());
    }
    void appearancePropertiesAffectRenderer() {
        const auto render=[](QJsonObject config) {
            BarPresetItem face; face.configureAsMic(); config["ignoreHistoryMs"]=0;
            if(!face.applyConfiguration(config)) { return QImage(); }
            face.pushBindingValue(113,0); face.pushBindingValue(103,-12); face.advanceMeter(0);
            face.pushBindingValue(113,-30); face.advanceMeter(100);
            QImage image(434,72,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent); QPainter painter(&image); face.paint(painter,434,72); return image;
        };
        const QImage normal=render({}); QVERIFY(!normal.isNull());
        const QColor history=normal.pixelColor(220,30); QVERIFY(history.red()>history.green());
        const QImage noHistory=render({{"showHistory",false}}); QCOMPARE(noHistory.pixelColor(220,30),QColor(32,32,32));
        const QImage held=render({{"showHistory",false},{"peakHold",true}}); const int heldX=qRound(18+398*.532); QVERIFY(held.pixelColor(heldX,48).red()>200);
        QVERIFY(normal!=render({{"redThreshold",-2.5}}));
        QVERIFY(normal!=render({{"titleColor","#ff0000ff"}}));
        QVERIFY(normal!=render({{"style","Solid"}})); QVERIFY(normal!=render({{"style","Segments"}}));
        QVERIFY(normal!=render({{"units",""}})); QVERIFY(normal!=render({{"showValue",false},{"showPeakValue",false}}));
        QVERIFY(normal!=render({{"barColor","#ff00ffff"},{"backdropColor","#ff102030"}}));
    }
    void filledStylesUseThresholdColors() {
        for(const QString& style:{QStringLiteral("Solid"),QStringLiteral("Segments")}) {
            BarPresetItem face; face.configureAsMic(); QVERIFY(face.applyConfiguration({{"style",style},{"showHistory",false}})); face.pushBindingValue(113,12); face.advanceMeter(0); face.advanceMeter(100);
            QImage image(434,72,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent); QPainter p(&image); face.paint(p,434,72); p.end();
            int white=0, red=0;
            for(int x=30;x<180;++x) { if(image.pixelColor(x,30)==QColor(Qt::white)) { ++white; } }
            for(int x=300;x<370;++x) { if(image.pixelColor(x,30)==QColor(Qt::red)) { ++red; } }
            QVERIFY(white>100); QVERIFY(red>40);
        }
    }
    void registryJsonRoundtrip() {
        ContainerContentRegistry registry; ContentEntry entry=registry.makeEntry("meter.mic");
        entry.config["legacyRecord"]=QStringLiteral("{\"kind\":\"BarPreset\",\"flavor\":\"Mic\",\"x\":0,\"y\":0,\"w\":1,\"h\":1,\"bindingId\":113,\"minValue\":-30,\"maxValue\":12,\"label\":\"A | B\",\"future\":{\"keep\":true}}");
        entry.config.remove("properties"); // Imported historical records have no explicit property overlay.
        entry.extensions["unavailableReason"]="old unavailable";
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr)); QVERIFY(item);
        auto* face=qobject_cast<BarPresetItem*>(item.get()); QVERIFY(face);
        QVERIFY(face->applyConfiguration({{"titleColor","#ffff0000"},{"peakHold",true},{"style","Segments"}}));
        ContentEntry edited=registry.captureMeterItem(*face,entry);
        QCOMPARE(edited.config["legacyRecord"],entry.config["legacyRecord"]);
        std::unique_ptr<MeterItem> copy(registry.createMeterItem(edited,nullptr)); QVERIFY(copy);
        QCOMPARE(qobject_cast<BarPresetItem*>(copy.get())->configuration()["label"].toString(),QStringLiteral("A | B"));
        QCOMPARE(qobject_cast<BarPresetItem*>(copy.get())->configuration()["future"],face->configuration()["future"]);
        const QString raw=entry.config["legacyRecord"].toString();
        MeterWidget widget; QVERIFY(widget.deserializeItems(raw));
        auto* widgetFace=qobject_cast<BarPresetItem*>(widget.items().first()); QVERIFY(widgetFace);
        QVERIFY(widgetFace->applyConfiguration({{"label","Edited without pipe"}}));
        const QJsonDocument widgetJson=QJsonDocument::fromJson(widget.serializeItems().toUtf8()); QVERIFY(widgetJson.isObject()); QCOMPARE(widgetJson.object()["future"],face->configuration()["future"]);
        std::unique_ptr<ItemGroup> group(ItemGroup::deserialize(QStringLiteral("GROUP\nHistorical\n0\n0\n1\n1\n1\n")+raw)); QVERIFY(group); QCOMPARE(group->items().size(),1);
        auto* groupFace=qobject_cast<BarPresetItem*>(group->items().first()); QVERIFY(groupFace); QVERIFY(groupFace->applyConfiguration({{"label","Edited"}}));
        const QString groupRaw=group->serialize().split(QLatin1Char('\n')).value(7); const QJsonDocument groupJson=QJsonDocument::fromJson(groupRaw.toUtf8()); QVERIFY(groupJson.isObject()); QCOMPARE(groupJson.object()["future"],face->configuration()["future"]);

    }
    void nativeFrames() {
        const QString dump=qEnvironmentVariable("NEREUS_METER_CAPTURE_DIR");
        for(int width:{260,434,640}) {
            MeterWidget widget; widget.resize(width,144);
            auto* mic=new BarPresetItem(&widget); mic->configureAsMic(); mic->setRect(0,0,1,.5);
            auto* alc=new BarPresetItem(&widget); alc->configureAsAlc(); alc->setRect(0,.5,1,.5);
            for(auto* face:{mic,alc}) { QVERIFY(face->applyConfiguration({{"titleColor","#ffff0000"},{"peakHold",true},{"ignoreHistoryMs",0}})); widget.addItem(face); }
            widget.show(); QTest::qWait(150);
#ifdef NEREUS_GPU_SPECTRUM
            QSignalSpy submitted(&widget,&QRhiWidget::frameSubmitted);
#endif
            QImage first,last;
            const int count=65; // Temporal renderer regressions run even without capture output.
            for(int i=0;i<count;++i) {
                const double peak=i<15 ? -30+i*2.6 : i<30 ? 6 : -25;
                widget.updateMeterValue(113,peak); widget.updateMeterValue(103,peak-9);
                widget.updateMeterValue(114,peak-2); widget.updateMeterValue(105,peak-11);
                if(i>=60) { widget.setBindingUnavailable(113,"Synthetic source unavailable"); widget.setBindingUnavailable(114,"Synthetic source unavailable"); }
                widget.advanceMeters(i*100); widget.update();
#ifdef NEREUS_GPU_SPECTRUM
                const int before=submitted.count(); QTRY_VERIFY_WITH_TIMEOUT(submitted.count()>before,2000);
                QImage frame=widget.grabFramebuffer();
#else
                QTest::qWait(15); QImage frame=widget.grab().toImage();
#endif
                QVERIFY(!frame.isNull());
                const double sx=double(frame.width())/width, sy=double(frame.height())/144;
                const auto pixel=[&](double x,double y) { return frame.pixelColor(qBound(0,int(x*sx),frame.width()-1),qBound(0,int(y*sy),frame.height()-1)); };
                if(i>=15 && i<60) {
                    for(auto* face:{mic,alc}) {
                        const double low=18+(width-36)*face->calibratedPosition(face->minimumHistory());
                        const double high=18+(width-36)*face->calibratedPosition(face->peakValue());
                        if(high-low>6) {
                            int bandPixels=0, correctAlphaPixels=0; const int y=face==mic ? 30 : 102;
                            for(int x=int(low)+2;x<int(high)-2;++x) {
                                const QColor c=pixel(x,y);
                                const QColor expected=face==mic ? QColor(144,16,16) : QColor(144,141,119);
                                if(qAbs(c.red()-expected.red())<=3 && qAbs(c.green()-expected.green())<=3 && qAbs(c.blue()-expected.blue())<=3) { ++correctAlphaPixels; }
                                if(face==mic ? c.red()>60 && c.red()>c.green()*2 : c.red()>80 && c.green()>80 && c.blue()<c.green()) { ++bandPixels; }
                            }
                            QVERIFY2(correctAlphaPixels>int((high-low)*.3),qPrintable(QStringLiteral("Wrong128-alpha composition width%1 frame%2 channel%3 expectedPixels%4").arg(width).arg(i).arg(face==mic?"Mic":"ALC").arg(correctAlphaPixels)));
                            QVERIFY2(bandPixels>int((high-low)*.3),qPrintable(QStringLiteral("Missing history band width%1 frame%2 channel%3 low%4 high%5 colored%6").arg(width).arg(i).arg(face==mic?"Mic":"ALC").arg(low).arg(high).arg(bandPixels)));
                        }
                    }
                }
                // Scale endpoints and moving primary/average markers have stable geometry.
                QVERIFY(pixel(20,57).red()>180);
                if(i<60) { const double x=18+(width-36)*mic->calibratedPosition(mic->primaryValue()); const QColor c=pixel(x,48); QVERIFY(c.red()>180 && c.green()>180 && c.blue()<80); }
                if(i>=60) { QVERIFY(!mic->hasPrimaryReading()); QVERIFY(mic->hasAverageReading()); const double x=18+(width-36)*mic->calibratedPosition(mic->averageValue()); const QColor c=pixel(x,48); QVERIFY(c.red()>100 && qAbs(c.red()-c.green())<15); }
                if(i==0) { first=frame; } last=frame;
                if(!dump.isEmpty()) { QDir().mkpath(dump); QVERIFY(frame.save(QStringLiteral("%1/face-%2-%3.png").arg(dump).arg(width).arg(i,3,10,QLatin1Char('0')))); }
            }
            QVERIFY(first!=last);
            qInfo()<<"Native"<<QGuiApplication::platformName()<<"DPI"<<widget.screen()->logicalDotsPerInch()<<"DPR"<<widget.devicePixelRatioF()<<"width"<<width;
#ifdef NEREUS_GPU_SPECTRUM
            qInfo()<<"QRhi API"<<static_cast<int>(widget.api())<<"submitted"<<submitted.count();
#endif
        }
    }
};
QTEST_MAIN(TestBarFace)
#include "tst_meter_bar_face.moc"
