// no-port-check: NereusSDR-original complete workspace recovery integration.
// Modification history (NereusSDR):
//   2026-10-02 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QTemporaryDir>
#include <QSplitter>
#include <QJsonDocument>
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/RadioStatus.h"
#include "core/session/TransmitStateFacade.h"
#include "gui/containers/ContainerSourceAdapter.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/VoiceRecordPlayItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "core/AppSettings.h"
#include "core/settings/ISettingsBackend.h"
#include "core/settings/SettingsScope.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
using namespace NereusSDR;
namespace {
class StationRequests : public ISettingsBackend {
public:
    int requests = 0;
    bool handlesKey(const QString& key) const override { return classifySettingsKey(key)==SettingsScope::Station; }
    QVariant value(const QString&, const QVariant& fallback) const override { return fallback; }
    void setValue(const QString&, const QVariant&) override { ++requests; }
    bool contains(const QString&) const override { return false; }
    void remove(const QString&) override { ++requests; }
    QStringList handledKeys() const override { return {}; }
};
const ContentEntry* findEntry(const WorkspaceDocument& d, const QString& id) {
    for(const auto& c:d.containers) { for(const auto& e:c.contents) { if(e.id==id) { return &e; } } }
    return nullptr;
}
}
class TstContainerWorkspaceIntegration : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {AppSettings::setProfileOverride(QStringLiteral("task10-integration-%1").arg(QCoreApplication::applicationPid()));AppSettings::instance().clear();}
    void cleanupTestCase() {QFile::remove(AppSettings::instance().filePath());}
    void remoteRecoveryRebuildRejectsStaleAndForeignTelemetry() {
        QTemporaryDir dir;AppSettings settings(dir.filePath("settings"));ContainerWorkspaceStore store(settings);ContainerContentRegistry registry;
        RadioModel model(RadioModel::Role::Remote);model.setStationConnectionState(ConnectionState::Connected);QVERIFY(model.addSliceWithStationId(7)>=0);auto* slice=model.sliceById(7);QVERIFY(slice);slice->setStreamIndex(0);slice->setSignalPeakDbm(-61);slice->setSignalAverageDbm(-67);
        bool ready=true;QString session="current";MeterPoller poller;poller.setRemoteRadioModel(&model,[&]{return ready;});poller.setSessionIdSource([&]{return session;});poller.setRadioStatus(&model.radioStatus());
        TransmitState txState;poller.setRemoteTransmitState(&txState,[&]{return ready?QString():QStringLiteral("Snapshot unavailable");});
        poller.setRxReadingSource([&](const QJsonObject& context,int binding){return ContainerSourceAdapter::reading(&model,context,slice,binding,ready,true,{},session);});
        QWidget root;QSplitter splitter;ContainerManager manager(&root,&splitter);manager.setWorkspaceAdapter(&store,&registry);
        connect(&manager,&ContainerManager::meterContextReady,&poller,[&](MeterWidget* meter,const QJsonObject& context){poller.setTargetContext(meter,context);poller.addTarget(meter);});
        WorkspaceDocument d;ContainerDocument c;c.id="remote";c.layout=ContentLayout::VerticalStack;d.mainContainerId=c.id;
        auto rx=registry.makeEntry("meter.signal");auto properties=rx.config["properties"].toObject();properties["attack"]=1;properties["decay"]=1;properties["ignoreHistoryMs"]=0;rx.config["properties"]=properties;rx.context={{"sliceId",7},{"sessionId",session}};
        auto power=registry.makeEntry("meter.powerSwr");power.context=rx.context;c.contents={rx,power};d.containers={c};QCOMPARE(manager.commitWorkspace(d,0).status,CommitStatus::Saved);
        auto readRx=[&](){return qobject_cast<BarPresetItem*>(manager.contentHost("remote")->entryRows()[0].item.data());};
        auto readPower=[&](){return qobject_cast<CompositePresetItem*>(manager.contentHost("remote")->entryRows()[1].item.data());};
        QVERIFY(QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection));
        QVERIFY(readRx());QVERIFY(readRx()->hasPrimaryReading());QCOMPARE(readRx()->primaryValue(),-61.0);QCOMPARE(poller.targetCountForTest(),1);
        ready=false;model.setStationConnectionState(ConnectionState::Disconnected);QVERIFY(QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection));QVERIFY(!readRx()->hasPrimaryReading());
        manager.floatContainer("remote");QCOMPARE(poller.targetCountForTest(),1);QVERIFY(!readRx()->hasPrimaryReading());
        ready=true;model.setStationConnectionState(ConnectionState::Connected);slice->setSignalPeakDbm(-42);QVERIFY(QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection));QCOMPARE(readRx()->primaryValue(),-42.0);
        QVERIFY(txState.applyStationValue("keyed",true));QVERIFY(txState.applyStationValue("forwardPowerWatts",50.0));QVERIFY(QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection));QVERIFY(readPower()->channelHasReading(0));
        session="replacement";manager.panelDockContainer("remote");QVERIFY(!readRx()->hasPrimaryReading());QVERIFY(!readPower()->channelHasReading(0));
        QVERIFY(txState.applyStationValue("forwardPowerWatts",80.0));QVERIFY(QMetaObject::invokeMethod(&poller,"poll",Qt::DirectConnection));QVERIFY(!readPower()->channelHasReading(0));
        d=store.snapshot();for(auto& e:d.containers[0].contents){e.context["sessionId"]=session;}QCOMPARE(manager.commitWorkspace(d,d.revision).status,CommitStatus::Saved);QVERIFY(readPower()->channelHasReading(0));
        QVERIFY(txState.applyStationValue("keyed",false));QVERIFY(!readPower()->channelHasReading(0));manager.overlayDockContainer("remote");QCOMPARE(poller.targetCountForTest(),1);QVERIFY(!readPower()->channelHasReading(0));
        manager.contentHost("remote")->releaseViews();QCOMPARE(poller.targetCountForTest(),0);
    }
    void unavailableLegacyControlsRemainCommittedAcrossApplyAndRestart() {
        QTemporaryDir dir; const QString path=dir.filePath("retained.settings");
        AppSettings settings(path); TextItem text; VoiceRecordPlayItem voice;
        const QString voiceRaw=voice.serialize(); const QString discordRaw="DISCORDBTNS|0|0.5|1|0.2|0|10|3";
        const QString payload=QStringList{text.serialize(),voiceRaw,discordRaw,text.serialize()}.join(QLatin1Char('\n'));
        settings.setValue("ContainerIdList","retained");
        settings.setValue("ContainerData_retained","retained|1|17|29|341|227|true|0|0|TOPLEFT|false|true|#ff123456");
        settings.setValue("ContainerItems_retained",payload); QVERIFY(settings.save());
        ContainerContentRegistry registry; WorkspaceDocument saved;
        {
            ContainerWorkspaceStore store(settings); QWidget root; QSplitter splitter;
            ContainerManager manager(&root,&splitter); manager.setWorkspaceAdapter(&store,&registry); manager.reconcileWorkspace(store.snapshot());
            const auto original=store.snapshot(); QCOMPARE(original.containers.first().contents.size(),4);
            QCOMPARE(original.containers.first().contents[1].config.value("legacyRecord").toString(),voiceRaw);
            QCOMPARE(original.containers.first().contents[2].config.value("legacyRecord").toString(),discordRaw);
            const auto* host=manager.contentHost("retained"); QVERIFY(host); QCOMPARE(host->entryRows().size(),4);
            QVERIFY(!host->entryRows()[1].item); QVERIFY(!host->entryRows()[2].item);
            QVERIFY(!registry.isAvailable("VOICERECPLAY")); QVERIFY(!registry.isAvailable("DISCORDBTNS"));
            ContainerEditSession edit(store); auto draft=edit.draft(); draft.containers.first().name="Retained controls"; edit.setDraft(draft);
            QCOMPARE(edit.apply().status,CommitStatus::Saved); manager.floatContainer("retained"); manager.overlayDockContainer("retained");
            saved=store.snapshot(); QCOMPARE(saved.containers.first().contents,original.containers.first().contents);
        }
        AppSettings restarted(path); restarted.load(); ContainerWorkspaceStore store(restarted); QCOMPARE(store.snapshot(),saved);
        const auto backup=QJsonDocument::fromJson(restarted.value("ContainerWorkspaceBackup").toString().toUtf8()).object();
        QCOMPARE(backup["ContainerItems_retained"].toString(),payload);
    }
    void migrationDraftApplyPopRestartReturnAndLateSingleton() {
        QTemporaryDir dir;
        const QString path=dir.filePath("workspace.settings");
        const QString raw="FUTUREPLUGIN|exact customized bytes | nested\n";
        AppSettings settings(path); StationRequests station; settings.setRemoteBackend(&station);
        settings.setValue("ContainerIdList","custom");
        settings.setValue("ContainerData_custom","custom|1|17|29|341|227|true|0|0|TOPLEFT|false|true|#ff123456");
        settings.setValue("ContainerItems_custom",raw); QVERIFY(settings.save());
        ContainerContentRegistry registry;
        WorkspaceDocument saved;
        QString opaqueId, meterId, singletonId, shellId;
        QWidget originalParent, singleton(&originalParent);
        QPointer<QWidget> identity=&singleton;
        {
            ContainerWorkspaceStore store(settings); auto d=store.snapshot();
            QVERIFY(d.extensions.contains("legacySettings"));
            auto home=std::find_if(d.containers.begin(),d.containers.end(),[](const auto& c){return c.id=="custom";});
            QVERIFY(home!=d.containers.end()); QVERIFY(!home->contents.isEmpty());
            opaqueId=home->contents[0].id; const auto originalOpaque=home->contents[0];
            home->layout=ContentLayout::VerticalStack;
            auto meter=registry.makeEntry("BAR"); meterId=meter.id; meter.name="MMIO monitor";
            meter.context={{"mmioGuid","00112233-4455-6677-8899-aabbccddeeff"},{"mmioVariable","Reading"},{"future",17}};
            meter.extensions={{"plugin",QJsonObject{{"exact","retain"}}}};
            auto applet=registry.makeEntry("applet:s_meter"); singletonId=applet.id;
            home->contents.append(meter); home->contents.append(applet);
            ContainerDocument second;second.id="second";second.name="Other source";second.layout=ContentLayout::VerticalStack;second.config={{"sliceId",1}};d.containers.append(second);
            QWidget dock;QSplitter splitter;ContainerManager manager(&dock,&splitter);manager.setWorkspaceAdapter(&store,&registry);
            QCOMPARE(manager.commitWorkspace(d,d.revision).status,CommitStatus::Saved);
            const auto before=store.snapshot();
            ContainerEditSession edit(store); d=edit.draft();
            for(auto& c:d.containers) { if(c.id=="custom") {c.name="Recovered station";} if(c.id=="second") {c.name="Separate readings";} }
            edit.setDraft(d);QCOMPARE(store.snapshot(),before);QCOMPARE(edit.apply().status,CommitStatus::Saved);
            registry.attachSingleton("applet:s_meter",&singleton);QCOMPARE(registry.singletonView("applet:s_meter"),identity.data());
            ContainerArrangeController arrange(store,&manager);
            QVERIFY(arrange.move(meterId,"second",0).ok);QVERIFY(arrange.popOut(singletonId).ok);
            shellId=store.snapshot().containers.last().id;QVERIFY(store.snapshot().containers.last().popOutShell);
            QCOMPARE(*findEntry(store.snapshot(),opaqueId),originalOpaque);
            QCOMPARE(findEntry(store.snapshot(),meterId)->context,meter.context);
            QCOMPARE(registry.singletonView("applet:s_meter"),identity.data());
            saved=store.snapshot();QCOMPARE(station.requests,0);
        }
        QVERIFY(identity);QCOMPARE(registry.singletonView("applet:s_meter"),identity.data());
        {
            AppSettings restarted(path);restarted.load();restarted.setRemoteBackend(&station);
            ContainerWorkspaceStore store(restarted);QVERIFY(store.load().ok);
            QCOMPARE(store.snapshot(),saved);
            QWidget dock;QSplitter splitter;ContainerManager manager(&dock,&splitter);manager.setWorkspaceAdapter(&store,&registry);manager.reconcileWorkspace(store.snapshot());
            QCOMPARE(registry.singletonView("applet:s_meter"),identity.data());
            ContainerArrangeController arrange(store,&manager);QVERIFY(arrange.closeContainer(shellId).ok);
            const auto returned=store.snapshot();QVERIFY(findEntry(returned,singletonId));
            const auto home=std::find_if(returned.containers.cbegin(),returned.containers.cend(),[](const auto& c){return c.id=="custom";});
            QVERIFY(home!=returned.containers.cend());QCOMPARE(home->name,QString("Recovered station"));
            QCOMPARE(home->contents[0].id,opaqueId);QCOMPARE(home->contents[1].id,singletonId);
            QCOMPARE(findEntry(returned,meterId)->name,QString("MMIO monitor"));QCOMPARE(findEntry(returned,meterId)->extensions,findEntry(saved,meterId)->extensions);
            QCOMPARE(findEntry(returned,opaqueId)->config,findEntry(saved,opaqueId)->config);
            const auto backup=QJsonDocument::fromJson(restarted.value("ContainerWorkspaceBackup").toString().toUtf8()).object();QCOMPARE(backup["ContainerItems_custom"].toString(),raw);
            QCOMPARE(station.requests,0);
        }
        QVERIFY(identity);QCOMPARE(registry.singletonView("applet:s_meter"),identity.data());
        registry.returnBorrowedView(&singleton);QCOMPARE(singleton.parentWidget(),&originalParent);
    }
};
QTEST_MAIN(TstContainerWorkspaceIntegration)
#include "tst_container_workspace_integration.moc"
