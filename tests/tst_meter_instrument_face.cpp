// no-port-check: NereusSDR-original regression coverage for PR #731 adaptation.
#include <QtTest>
#include <QComboBox>
#include <QLineEdit>
#include <QPainter>
#include <QDir>
#include <QJsonDocument>
#include <QListWidget>
#include <QSplitter>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QAbstractItemView>
#include "gui/meters/MeterWidget.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "core/AppSettings.h"
#include "gui/styles/AppTheme.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/PureSignalInstruments.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/containers/ContainerSettingsDialog.h"
using namespace NereusSDR;
class TestMeterInstrumentFace : public QObject {
    Q_OBJECT
private:
    static QImage render(BarPresetItem& bar, int width=320, int height=72)
    {
        QImage image(width,height,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
        QPainter painter(&image); bar.setRect(0,0,1,1); bar.paint(painter,width,height); return image;
    }
private slots:
    void captureFeedbackReference()
    {
        const QString folder=qEnvironmentVariable("NEREUS_METER_CAPTURE_DIR");
        if(folder.isEmpty()) { return; }
        QDir().mkpath(folder);
        PureSignalFeedbackMeter meter; meter.resize(320,38); meter.setLevel(150,true);
        QImage image(meter.size(),QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent); meter.render(&image);
        QVERIFY(image.save(folder+"/feedback.png"));
    }
    void instrumentUsesSettingsAndStaysInsideBounds()
    {
        WorkspacePreview::setEnabledForTest(false);
        for(const QString flavor:QStringList{"Swr","SignalAvg","Pwr","Mic","Custom"}) {
            BarPresetItem bar;
            if(flavor=="Mic") { bar.configureAsMic(); }
            else if(flavor=="Custom") { bar.configureAsCustom(1,-10,10,"Custom volts"); }
            else { QVERIFY(bar.configureVariant(flavor)); }
            QVERIFY(bar.applyConfiguration({{"face","Instrument"},{"attack",1.0},{"ignoreHistoryMs",0}}));
            const double value=flavor=="Swr"?2.5:flavor=="SignalAvg"?-60:flavor=="Pwr"?25:3;
            bar.pushBindingValue(bar.bindingId(),value); bar.advanceMeter(0);
            const auto full=render(bar);
            QVERIFY(bar.applyConfiguration({{"label","Saved operator caption"}}));
            bar.pushBindingValue(bar.bindingId(),value); bar.advanceMeter(0);
            QVERIFY(full!=render(bar));
            QVERIFY(bar.applyConfiguration({{"showTitle",false},{"showValue",false},{"showPeakValue",false},{"showHistory",false},{"showMarker",false},{"showSub",false},{"peakHold",false}}));
            bar.pushBindingValue(bar.bindingId(),value); bar.advanceMeter(0);
            const auto bare=render(bar);
            QVERIFY(bar.applyConfiguration({{"showMarker",true},{"barColor","#ff00ffff"}}));
            bar.pushBindingValue(bar.bindingId(),value); bar.advanceMeter(0);
            QVERIFY(bare!=render(bar));
            for(const QSize size:{QSize(26,20),QSize(90,30),QSize(160,48),QSize(320,72)}) {
                QImage image(size+QSize(20,20),QImage::Format_ARGB32_Premultiplied); image.fill(Qt::magenta);
                QPainter painter(&image); painter.translate(10,10);
                bar.setRect(0,0,1,1); const QPen pen(Qt::cyan,7); painter.setPen(pen); bar.paint(painter,size.width(),size.height()); QCOMPARE(painter.pen(),pen); painter.end();
                for(int y=0;y<image.height();++y) { for(int x=0;x<image.width();++x) {
                    if(x<10 || y<10 || x>=size.width()+10 || y>=size.height()+10) { QCOMPARE(image.pixelColor(x,y),QColor(Qt::magenta)); }
                } }
            }
        }
    }
    void instrumentKeepsCalibratedThresholds()
    {
        WorkspacePreview::setEnabledForTest(false);
        for(const QString flavor:QStringList{"Swr","SignalAvg"}) {
            BarPresetItem bar; QVERIFY(bar.configureVariant(flavor));
            QVERIFY(bar.applyConfiguration({{"face","Instrument"},{"lowColor","#ff00ffff"},{"highColor","#ffff00ff"},{"showHistory",false}}));
            const auto image=render(bar);
            const auto pixel=[&](double value) { return image.pixelColor(qRound(18+284*bar.calibratedPosition(value)),41); };
            QCOMPARE(pixel(flavor=="Swr"?2.5:-80),QColor(Qt::cyan));
            QCOMPARE(pixel(flavor=="Swr"?4:-60),QColor(Qt::magenta));
            QCOMPARE(bar.configuration()["redThreshold"].toDouble(),flavor=="Swr"?3.0:-73.0);
        }
    }
    void narrowInstrumentValueStaysInItsBox()
    {
        WorkspacePreview::setEnabledForTest(true);
        BarPresetItem bar; bar.configureVariant("SignalAvg");
        QVERIFY(bar.applyConfiguration({{"face","Instrument"},{"attack",1.0},{"showTitle",false},{"showPeakValue",false}}));
        bar.pushBindingValue(bar.bindingId(),-60); bar.advanceMeter(0); const auto shown=render(bar,160,48);
        QVERIFY(bar.applyConfiguration({{"showValue",false}}));
        bar.pushBindingValue(bar.bindingId(),-60); bar.advanceMeter(0); const auto hidden=render(bar,160,48);
        // At 160x48 the calibrated inset is 18*(160/260), and the
        // primary value owns only the first 32% of the remaining width.
        const double scale=160.0/260.0, left=18*scale, width=160-36*scale;
        for(int y=0;y<qFloor(24*scale);++y) { for(int x=qCeil(left+width*.32-2*scale);x<160;++x) { QCOMPARE(shown.pixel(x,y),hidden.pixel(x,y)); } }
        WorkspacePreview::setEnabledForTest(false);
    }
    void instrumentEndpointCaptionsRemainReadable()
    {
        WorkspacePreview::setEnabledForTest(true);
        BarPresetItem bar; bar.configureVariant("SignalAvg");
        QVERIFY(bar.applyConfiguration({{"face","Instrument"},{"lowColor","#ff00ffff"},{"highColor","#ffff00ff"}}));
        const auto image=render(bar);
        for(const QRect area:{QRect(18,50,26,8),QRect(274,50,26,8)}) {
            int ink=0;
            for(int y=area.top();y<=area.bottom();++y) { for(int x=area.left();x<=area.right();++x) { const auto c=image.pixelColor(x,y); if(c.green()>100 || c.blue()>100) { ++ink; } } }
            QVERIFY2(ink>10,"Endpoint tick captions must contain digits, not just a low ellipsis");
        }
        WorkspacePreview::setEnabledForTest(false);
    }
    void instrumentHonorsVisibleControls()
    {
        WorkspacePreview::setEnabledForTest(false);
        BarPresetItem original; original.configureAsMic();
        QVERIFY(original.applyConfiguration({{"face","Instrument"},{"style","Solid"},{"attack",1.0},{"decay",1.0},{"ignoreHistoryMs",0},{"peakHold",true},{"label","My mic"}}));
        const auto settings=original.configuration();
        const auto picture=[&](const QJsonObject& change) {
            BarPresetItem bar;
            if(!bar.deserialize(QString::fromUtf8(QJsonDocument(settings).toJson(QJsonDocument::Compact))) || !bar.applyConfiguration(change)) { return QImage(); }
            for(int i=0;i<3;++i) { bar.pushBindingValue(bar.bindingId(),i==0?-20:i==1?8:3); bar.setSecondaryValue(1,-5); bar.advanceMeter(i*100); }
            return render(bar);
        };
        const QImage full=picture({}); QVERIFY(!full.isNull());
        const QJsonObject alternatives{{"showTitle",false},{"showValue",false},{"showPeakValue",false},{"showMarker",false},{"showSub",false},{"showHistory",false},{"peakHold",false},
            {"label","Different title"},{"barColor","#ff00ffff"},{"titleColor","#ff00ffff"},{"backdropColor","#ff345678"},{"historyColor","#8000ffff"},{"subColor","#ff00ffff"},
            {"peakHoldColor","#ff00ffff"},{"peakValueColor","#ff00ffff"},{"segLowColor","#ff00ffff"},{"segHighColor","#ff00ffff"},{"lowColor","#ff00ffff"},{"highColor","#ff00ffff"},{"style","Segments"}};
        for(auto it=alternatives.begin();it!=alternatives.end();++it) { QVERIFY2(full!=picture({{it.key(),it.value()}}),qPrintable(it.key())); }
        BarPresetItem signal; QVERIFY(signal.configureVariant("SignalAvg")); QVERIFY(signal.applyConfiguration({{"face","Instrument"},{"attack",1.0}}));
        signal.pushBindingValue(signal.bindingId(),-60); signal.advanceMeter(0); const auto dbm=render(signal);
        signal.setUnitMode(MeterItem::MeterUnit::S); QVERIFY(dbm!=render(signal));
    }
    void captureActualFaces()
    {
        const QString folder=qEnvironmentVariable("NEREUS_METER_CAPTURE_DIR");
        if(folder.isEmpty()) { return; }
        QDir().mkpath(folder);
        for(bool workspace:{false,true}) {
            WorkspacePreview::setEnabledForTest(workspace);
            for(const QString flavor:QStringList{"Mic","Alc","Swr","SignalAvg","Pwr"}) {
                for(const QString face:QStringList{"Classic","Instrument"}) {
                    BarPresetItem bar;
                    if(flavor=="Mic") { bar.configureAsMic(); } else if(flavor=="Alc") { bar.configureAsAlc(); } else { bar.configureVariant(flavor); }
                    bar.applyNewItemDefaults();
                    QVERIFY(bar.applyConfiguration({{"face",face},{"attack",1.0},{"ignoreHistoryMs",0}}));
                    const double value=flavor=="Swr"?2.5:flavor=="SignalAvg"?-60:flavor=="Pwr"?35:-3;
                    bar.pushBindingValue(bar.bindingId(),value); bar.advanceMeter(0);
                    for(const QSize size:{QSize(320,72),QSize(160,48)}) {
                        QVERIFY(render(bar,size.width(),size.height()).save(folder+"/"+(workspace?QString("workspace-"):QString())+flavor+"-"+face+QString("-%1x%2.png").arg(size.width()).arg(size.height())));
                    }
                }
            }
        }
        WorkspacePreview::setEnabledForTest(false);
    }
    // Also run against the pre-adaptation reader in the RED phase: it treats
    // face as unknown data and must save it again unchanged.
    void readerRetainsFaceKeyAndUnknownData()
    {
        BarPresetItem original; original.configureAsMic();
        auto record=original.configuration(); record["face"]="Instrument"; record["futureSetting"]=42;
        BarPresetItem reader;
        QVERIFY(reader.deserialize(QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Compact))));
        QCOMPARE(reader.configuration()["face"].toString(),QString("Instrument"));
        QCOMPARE(reader.configuration()["futureSetting"].toInt(),42);
    }
    void switchingFacePreservesLiveMeter()
    {
        BarPresetItem bar; bar.configureVariant("Swr");
        QVERIFY(bar.applyConfiguration({{"attack", .5}, {"ignoreHistoryMs", 0}, {"label", "Antenna SWR"}}));
        bar.pushBindingValue(bar.bindingId(), 2.5); bar.advanceMeter(0);
        const double reading=bar.primaryValue(), peak=bar.peakValue(), position=bar.calibratedPosition(2.5);
        const auto before=bar.configuration();
        QVERIFY(bar.applyConfiguration({{"face", "Instrument"}}));
        QCOMPARE(bar.configuration().value("face").toString(), QString("Instrument"));
        QCOMPARE(bar.primaryValue(), reading); QCOMPARE(bar.peakValue(), peak); QVERIFY(bar.hasPrimaryReading());
        auto after=bar.configuration(); after.remove("face"); auto expected=before; expected.remove("face");
        QCOMPARE(after, expected); QCOMPARE(bar.calibratedPosition(2.5), position);
        QVERIFY(bar.applyConfiguration({{"face", "Classic"}}));
        QCOMPARE(bar.primaryValue(), reading); QVERIFY(bar.hasPrimaryReading());
        bar.pushBindingValue(bar.bindingId(),2.5); bar.advanceMeter(100);
        QCOMPARE(bar.primaryValue(),reading*.5+2.5*.5);
        QVERIFY(!bar.applyConfiguration({{"face", "invalid"}}));
    }
    void existingRecordAndRegistryRoundTrip()
    {
        ContainerContentRegistry registry;
        auto entry=registry.makeEntry("meter.swr");
        auto props=entry.config.value("properties").toObject(); props.remove("face");
        props["label"]="Saved antenna"; props["futureSetting"]=42; entry.config["properties"]=props;
        auto raw=QJsonDocument::fromJson(entry.config["legacyRecord"].toString().toUtf8()).object(); raw.remove("face");
        entry.config["legacyRecord"]=QString::fromUtf8(QJsonDocument(raw).toJson(QJsonDocument::Compact));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr)); auto* bar=qobject_cast<BarPresetItem*>(item.get()); QVERIFY(bar);
        QCOMPARE(bar->configuration().value("face").toString(), QString("Classic"));
        QVERIFY(bar->applyConfiguration({{"face", "Instrument"}}));
        const auto captured=registry.captureMeterItem(*bar, entry);
        QCOMPARE(captured.typeId, entry.typeId); QCOMPARE(captured.id, entry.id);
        std::unique_ptr<MeterItem> reloaded(registry.createMeterItem(captured,nullptr)); auto* saved=qobject_cast<BarPresetItem*>(reloaded.get()); QVERIFY(saved);
        QCOMPARE(saved->configuration(), bar->configuration());
        QCOMPARE(saved->configuration()["redThreshold"].toDouble(),3.0);
        QCOMPARE(saved->configuration()["label"].toString(),QString("Saved antenna"));
        QCOMPARE(saved->configuration()["futureSetting"].toInt(),42);
        for(const auto& d:registry.descriptors()) { QVERIFY(!d.typeId.endsWith("New")); }
    }
    void eachExistingBarKeepsItsRecordIdentity()
    {
        QStringList flavors=BarPresetItem::variants(); flavors.append({"Mic","Alc","Custom"});
        for(const QString& flavor:flavors) {
            BarPresetItem bar;
            if(flavor=="Mic") { bar.configureAsMic(); }
            else if(flavor=="Alc") { bar.configureAsAlc(); }
            else if(flavor=="Custom") { bar.configureAsCustom(1,-10,10,"Operator sensor"); }
            else { QVERIFY(bar.configureVariant(flavor)); }
            const QString type=bar.typeId(); const auto bindings=bar.readingBindings();
            QVERIFY(bar.applyConfiguration({{"face","Instrument"}}));
            QCOMPARE(bar.typeId(),type); QCOMPARE(bar.readingBindings(),bindings);
            BarPresetItem loaded; QVERIFY(loaded.deserialize(bar.serialize()));
            QCOMPARE(loaded.typeId(),type); QCOMPARE(loaded.configuration(),bar.configuration());
        }
    }
    void editorAndPasteUseExistingBar()
    {
        ContainerContentRegistry registry; ContentPropertyEditor editor(registry);
        editor.setEntry(registry.makeEntry("meter.mic"));
        auto* face=editor.findChild<QComboBox*>("face"); QVERIFY(face);
        QSignalSpy edits(&editor,&ContentPropertyEditor::entryEdited);
        face->setCurrentIndex(face->findData("Instrument")); QVERIFY(!edits.isEmpty());
        auto entry=qvariant_cast<ContentEntry>(edits.last().first());
        QCOMPARE(entry.typeId,QString("meter.mic"));
        auto* label=editor.findChild<QLineEdit*>("label"); QVERIFY(label);
        label->setText("Operator mic"); QMetaObject::invokeMethod(label,"editingFinished");
        entry=qvariant_cast<ContentEntry>(edits.last().first());
        std::unique_ptr<MeterItem> source(registry.createMeterItem(entry,nullptr)); auto* bar=qobject_cast<BarPresetItem*>(source.get()); QVERIFY(bar);
        QCOMPARE(bar->configuration()["label"].toString(),QString("Operator mic"));
        BarPresetItem target; target.configureVariant("Swr"); const int binding=target.bindingId();
        QVERIFY(ContainerSettingsDialog::pasteItemSettings(bar->serialize(),bar->typeId(),target));
        QCOMPARE(target.configuration()["face"].toString(),QString("Instrument"));
        QCOMPARE(target.typeId(),QString("meter.swr")); QCOMPARE(target.bindingId(),binding);
        QCOMPARE(target.configuration()["redThreshold"].toDouble(),3.0);
    }
    void newFollowRecordsMoveWithoutFlatteningOrResettingLiveReadings()
    {
        ContainerContentRegistry registry; auto entry=registry.makeEntry("meter.signalAvg");
        QCOMPARE(entry.config["properties"].toObject()["face"].toString(),QString("Container"));
        auto properties=entry.config["properties"].toObject(); properties["futureMeterOption"]=42; entry.config["properties"]=properties;
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings")); ContainerWorkspaceStore store(settings);
        QWidget owner; QSplitter splitter(&owner); ContainerManager manager(&owner,&splitter);
        manager.setWorkspaceAdapter(&store,&registry);
        WorkspaceDocument document; document.mainContainerId="A";
        ContainerDocument a,b; a.id="A"; a.name="A"; b.id="B"; b.name="B";
        a.layout=b.layout=ContentLayout::VerticalStack; a.contents={entry}; document.containers={a,b};
        QCOMPARE(manager.commitWorkspace(document,0).status,CommitStatus::Saved);
        auto* live=qobject_cast<BarPresetItem*>(manager.contentHost("A")->entryRows()[0].item.data()); QVERIFY(live);
        live->pushBindingValue(live->bindingId(),-60); live->advanceMeter(0);
        const auto classic=render(*live); const double reading=live->primaryValue();
        auto changed=store.snapshot(); changed.containers[0].config["meterFace"]="Instrument";
        QCOMPARE(manager.commitWorkspace(changed,changed.revision).status,CommitStatus::Saved);
        QCOMPARE(manager.contentHost("A")->entryRows()[0].item.data(),static_cast<MeterItem*>(live));
        QCOMPARE(live->primaryValue(),reading); QVERIFY(live->hasPrimaryReading()); QVERIFY(classic!=render(*live));
        QCOMPARE(live->configuration()["face"].toString(),QString("Container"));
        const auto captured=registry.captureMeterItem(*live,entry);
        QCOMPARE(captured.config["properties"].toObject()["face"].toString(),QString("Container"));
        QCOMPARE(captured.config["properties"].toObject()["futureMeterOption"].toInt(),42);
        // The same policy follows the destination after a move or copy.
        auto moved=store.snapshot(); auto follower=moved.containers[0].contents.takeFirst();
        const auto copied=ContainerDocumentCodec::importEntries(ContainerDocumentCodec::exportEntries({follower})); QVERIFY(copied.ok);
        moved.containers[1].contents={follower,registry.withNewItemIdentity(copied.document.containers.first().contents.first())};
        QCOMPARE(manager.commitWorkspace(moved,moved.revision).status,CommitStatus::Saved);
        for(const auto& row:manager.contentHost("B")->entryRows()) {
            auto* bar=qobject_cast<BarPresetItem*>(row.item.data()); QVERIFY(bar);
            bar->pushBindingValue(bar->bindingId(),-60); bar->advanceMeter(0);
            QCOMPARE(render(*bar),classic); QCOMPARE(bar->configuration()["face"].toString(),QString("Container"));
        }
        const auto encoded=ContainerDocumentCodec::encode(store.snapshot()); const auto decoded=ContainerDocumentCodec::decode(encoded);
        QVERIFY(decoded.ok); QCOMPARE(decoded.document,store.snapshot());
        ContainerWorkspaceStore reloaded(settings); QCOMPARE(reloaded.snapshot(),store.snapshot());
    }
    void realDialogContainerChoicePreviewsCancelsAppliesAndResetsOnlyBars()
    {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings")); ContainerWorkspaceStore store(settings);
        ContainerContentRegistry registry; MeterPoller poller; QWidget owner; QSplitter splitter(&owner);
        ContainerManager manager(&owner,&splitter); manager.setWorkspaceAdapter(&store,&registry); manager.setPreviewPoller(&poller);
        WorkspaceDocument document; document.mainContainerId="A"; ContainerDocument a; a.id="A"; a.layout=ContentLayout::VerticalStack;
        auto follow=registry.makeEntry("meter.signalAvg"), explicitClassic=registry.makeEntry("meter.swr"), legacy=registry.makeEntry("meter.mic"), analog=registry.makeEntry("meter.powerSwr");
        auto c=explicitClassic.config["properties"].toObject(); c["face"]="Classic"; explicitClassic.config["properties"]=c;
        c=legacy.config["properties"].toObject(); c.remove("face"); legacy.config["properties"]=c;
        auto raw=QJsonDocument::fromJson(legacy.config["legacyRecord"].toString().toUtf8()).object(); raw.remove("face");
        legacy.config["legacyRecord"]=QString::fromUtf8(QJsonDocument(raw).toJson(QJsonDocument::Compact));
        ContentEntry opaque; opaque.id=QUuid::createUuid().toString(QUuid::WithoutBraces); opaque.typeId="plugin.futureMeter";
        opaque.config["rawFutureData"]="retain exactly";
        a.config["futureContainerOption"]=77; a.contents={follow,explicitClassic,legacy,analog,opaque}; document.containers={a};
        QCOMPARE(manager.commitWorkspace(document,0).status,CommitStatus::Saved); const auto original=store.snapshot();
        const auto previewBar=[](ContainerSettingsDialog& dialog,const QString& type) -> BarPresetItem* {
            auto* preview=dialog.findChild<ContainerPreviewWidget*>();
            for(auto* bar:preview->findChildren<BarPresetItem*>()) {if(bar->typeId()==type) {return bar;}}
            return nullptr;
        };
        {
            ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager);
            dialog.findChild<QListWidget*>("containerDraftContents")->setCurrentRow(0);
            auto* choice=dialog.findChild<QComboBox*>("containerMeterFace"); QVERIFY(choice);
            auto* before=previewBar(dialog,"meter.signalAvg"); QVERIFY(before); const auto image=render(*before);
            choice->setCurrentIndex(choice->findData("Instrument"));
            auto* after=previewBar(dialog,"meter.signalAvg"); QCOMPARE(after,before); QVERIFY(image!=render(*after));
            QCOMPARE(store.snapshot(),original);
            auto* face=dialog.findChild<ContentPropertyEditor*>()->findChild<QComboBox*>("face");
            QCOMPARE(face->currentData().toString(),QString("Container")); QVERIFY(face->currentText().contains("Modern"));
            face->setCurrentIndex(face->findData("Classic"));
            QCOMPARE(dialog.editSession()->draft().containers.first().contents.first().config["properties"].toObject()["face"].toString(),QString("Classic"));
            face->setCurrentIndex(face->findData("Container"));
            QCOMPARE(dialog.editSession()->draft().containers.first().contents.first().config["properties"].toObject()["face"].toString(),QString("Container"));
            // Both a legacy omitted face and an explicit Classic override stay Classic.
            BarPresetItem swr; swr.configureVariant("Swr"); QCOMPARE(render(*previewBar(dialog,"meter.swr")),render(swr));
            BarPresetItem mic; mic.configureAsMic(); mic.applyNewItemDefaults(); mic.applyConfiguration({{"face","Classic"}});
            QCOMPARE(render(*previewBar(dialog,"meter.mic")),render(mic));
            auto* all=dialog.findChild<QPushButton*>("containerUseFaceForAll"); QVERIFY(all); all->click();
            const auto draft=dialog.editSession()->draft().containers.first(); QCOMPARE(draft.contents[3],analog); QCOMPARE(draft.contents[4],opaque);
            for(int i=0;i<3;++i) {QCOMPARE(draft.contents[i].config["properties"].toObject()["face"].toString(),QString("Container"));}
            QCOMPARE(draft.config["futureContainerOption"].toInt(),77);
            dialog.reject(); QCOMPARE(store.snapshot(),original);
        }
        {
            ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager);
            auto* choice=dialog.findChild<QComboBox*>("containerMeterFace"); QVERIFY(choice);
            choice->setCurrentIndex(choice->findData("Instrument"));
            QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved);
            QCOMPARE(store.snapshot().containers.first().config["meterFace"].toString(),QString("Instrument"));
            QCOMPARE(store.snapshot().containers.first().contents[1],explicitClassic);
            QCOMPARE(store.snapshot().containers.first().contents[2],legacy);
            dialog.reject();
        }
        {
            ContainerSettingsDialog dialog(manager.container("A"),nullptr,&manager);
            dialog.findChild<QPushButton*>("containerUseFaceForAll")->click();
            QCOMPARE(dialog.applyDraft().status,CommitStatus::Saved);
            const auto saved=store.snapshot().containers.first();
            for(int i=0;i<3;++i) {QCOMPARE(saved.contents[i].config["properties"].toObject()["face"].toString(),QString("Container"));}
            QCOMPARE(saved.contents[3],analog); QCOMPARE(saved.contents[4],opaque);
            ContainerWorkspaceStore reloaded(settings); QCOMPARE(reloaded.snapshot(),store.snapshot());
            dialog.reject();
        }
    }
    // Full production dialog with actual container/individual controls. The
    // source-default application theme is bootstrapped exactly as in main.cpp.
    void captureThemedContainerSettingsDialog()
    {
        const QString folder=qEnvironmentVariable("NEREUS_METER_DIALOG_CAPTURE_DIR");
        if(folder.isEmpty()) { return; }
        QVERIFY(QDir().mkpath(folder));
        WorkspacePreview::setEnabledForTest(true);
        qApp->setStyle(QStyleFactory::create("Fusion"));
        applyDarkPalette(*qApp); applyAppBaselineQss(*qApp); applyWorkspacePalette(*qApp);
        QTemporaryDir temporary; QVERIFY(temporary.isValid());
        AppSettings settings(temporary.filePath("settings"));
        ContainerWorkspaceStore store(settings); ContainerContentRegistry registry;
        MeterPoller poller; // Never started; no RadioModel or radio/audio source.
        QWidget owner; QSplitter splitter(&owner);
        ContainerManager manager(&owner,&splitter);
        manager.setWorkspaceAdapter(&store,&registry); manager.setPreviewPoller(&poller);
        WorkspaceDocument document; document.mainContainerId="11aa22bb-capture";
        ContainerDocument container; container.id=document.mainContainerId;
        container.name="Meter"; container.layout=ContentLayout::VerticalStack;
        const bool groupDemo=qEnvironmentVariableIsSet("NEREUS_METER_FACE_PROPOSAL");
        container.contents={registry.makeEntry("meter.signalAvg")};
        if(groupDemo) {container.contents.append(registry.makeEntry("meter.swr"));container.contents.append(registry.makeEntry("meter.mic"));}
        document.containers={container};
        QCOMPARE(manager.commitWorkspace(document,0).status,CommitStatus::Saved);
        const auto original=store.snapshot();
        ContainerSettingsDialog dialog(manager.container(container.id),&owner,&manager);
        QVERIFY(dialog.editSession()); QVERIFY(dialog.findChild<ContainerPreviewWidget*>());
        auto* contents=dialog.findChild<QListWidget*>("containerDraftContents"); QVERIFY(contents);
        contents->setCurrentRow(0); dialog.show(); dialog.resize(groupDemo?1300:1200,900);
        QCoreApplication::processEvents();
        auto* editor=dialog.findChild<ContentPropertyEditor*>(); QVERIFY(editor);
        auto* face=editor->findChild<QComboBox*>("face"); QVERIFY(face); QVERIFY(face->isVisible());
        auto* containerFace=dialog.findChild<QComboBox*>("containerMeterFace"); QVERIFY(containerFace);
        QCOMPARE(containerFace->currentData().toString(),QString("Classic"));
        QCOMPARE(face->currentData().toString(),QString("Container"));
        QVERIFY(dialog.grab().save(folder+"/container-settings-workspace-classic.png"));
        containerFace->setCurrentIndex(containerFace->findData("Instrument"));
        QCoreApplication::processEvents();
        QCOMPARE(face->currentData().toString(),QString("Container"));
        QCOMPARE(face->currentText(),QString("Use container setting (Modern)"));
        if(groupDemo) {
            // Synthetic measurements enter the actual preview bar objects.
            // There is no running poller or external measurement/device.
            auto* preview=dialog.findChild<ContainerPreviewWidget*>();
            for(auto* meter:preview->findChildren<MeterWidget*>()) {
                for(auto* item:meter->items()) {
                    auto* bar=qobject_cast<BarPresetItem*>(item); if(!bar) {continue;}
                    const double value=bar->typeId()=="meter.swr"?2.5:bar->typeId()=="meter.mic"?-3:-60;
                    QVERIFY(bar->applyConfiguration({{"attack",1.0},{"ignoreHistoryMs",0}}));
                    bar->pushBindingValue(bar->bindingId(),value);bar->setSecondaryValue(1,value-3);bar->advanceMeter(0);
                }
                meter->update();
            }
        }
        QCoreApplication::processEvents();
        const QPixmap closed=dialog.grab();
        QVERIFY(closed.save(folder+"/container-settings-workspace-modern.png"));
        const QWidget* viewport=editor->parentWidget();
        const QRect properties(viewport->mapTo(&dialog,QPoint()),viewport->size());
        QVERIFY(closed.copy(properties.intersected(dialog.rect())).save(folder+"/container-settings-properties-crop.png"));
        const QWidget* top=dialog.findChild<QComboBox*>("containerHeader")->parentWidget();
        QVERIFY(closed.copy(QRect(top->mapTo(&dialog,QPoint()),top->size())).save(folder+"/container-settings-controls-crop.png"));
        // Avoid the offscreen virtual screen clamping the actual popup left.
        dialog.move(QPoint(20,20)-face->mapTo(&dialog,QPoint()));QCoreApplication::processEvents();
        face->showPopup();QCoreApplication::processEvents();QWidget* popup=face->view()->window();
        QVERIFY(qAbs(popup->mapToGlobal(QPoint()).x()-face->mapToGlobal(QPoint()).x())<5);
        QPixmap combined=dialog.grab();
        {QPainter painter(&combined);painter.drawPixmap(popup->mapToGlobal(QPoint())-dialog.mapToGlobal(QPoint()),popup->grab());}
        QVERIFY(combined.save(folder+"/container-settings-individual-choices.png"));
        face->hidePopup();dialog.reject();QCOMPARE(store.snapshot(),original);
        WorkspacePreview::setEnabledForTest(false);
    }
};
QTEST_MAIN(TestMeterInstrumentFace)
#include "tst_meter_instrument_face.moc"
