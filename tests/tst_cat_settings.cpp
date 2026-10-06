// no-port-check: NereusSDR-original CAT policy/lifecycle regression tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QTemporaryDir>
#include <QTcpServer>
#include "core/AppSettings.h"
#include "core/cat/CatSettings.h"
#include "core/cat/CatService.h"
#include "models/RadioModel.h"
#include "core/SliceOwnership.h"
using namespace NereusSDR;
class TstCatSettings : public QObject {
    Q_OBJECT
private slots:
    void optionalSerialFailurePreservesConfiguration() {
        for (const QString& key : AppSettings::instance().allKeys()) {
            if (key.startsWith("Cat/")) { AppSettings::instance().remove(key); }
        }
        QTemporaryDir directory; QVERIFY(directory.isValid());
        RadioModel model; CatService& service = *model.catService();
        CatEndpointConfig config; config.serialEnabled = true; config.serialDevice = directory.filePath("absent.serial");
        config.serialBaud = 9600; config.serialDataBits = 7; config.serialParity = "Odd"; config.serialStopBits = "2";
        QVERIFY(service.applyChannelConfig(1, config)); service.startConfigured();
        QVERIFY(!service.isListening(1)); QVERIFY(service.channelState(1).startsWith("Serial error:"));
#ifndef HAVE_SERIALPORT
        QVERIFY(service.channelState(1).contains("dependency unavailable"));
#endif
        const QList<CatEndpointConfig> restored = CatSettings::load(AppSettings::instance(), model);
        QVERIFY(restored[0].serialEnabled); QCOMPARE(restored[0].serialDevice, config.serialDevice);
        QCOMPARE(restored[0].serialBaud, 9600); QCOMPARE(restored[0].serialDataBits, 7);
        QCOMPARE(restored[0].serialParity, QString("Odd")); QCOMPARE(restored[0].serialStopBits, QString("2"));
        QVERIFY(service.sessionIds(1).isEmpty()); service.stopAll();
    }
    void defaultsAndRoundTrip()
    {
        QTemporaryDir directory;
        AppSettings store(directory.filePath("cat.settings"));
        RadioModel model(RadioModel::Role::Remote);
        model.addSliceWithStationId(0); model.addSliceWithStationId(1);
        QList<CatEndpointConfig> configs = CatSettings::load(store, model);
        QCOMPARE(configs.size(), 4);
        QCOMPARE(configs[0].binding.primarySliceId, 0);
        QCOMPARE(configs[0].binding.secondarySliceId.value(), 1);
        QCOMPARE(configs[0].tcpPort, 13013);
        for (const CatEndpointConfig& config : configs) {
            QVERIFY(!config.tcpEnabled && !config.serialEnabled && !config.ptyEnabled && !config.rigctldEnabled);
            QCOMPARE(config.serialBaud, 115200); QCOMPARE(config.serialParity, QString("None"));
            QCOMPARE(config.serialDataBits, 8); QCOMPARE(config.serialStopBits, QString("1"));
        }
        QCOMPARE(configs[1].binding.primarySliceId, -1); QCOMPARE(configs[1].tcpPort, 0);
        CatSettings settings(store);
        CatGlobalConfig global = settings.global();
        QVERIFY(!global.sendWelcome && !global.allowKenwoodAi && !global.aiEnabled);
        QVERIFY(global.aiSerial1 && global.aiTcp && global.limitReportedPower);
        QCOMPARE(global.rigIdentity, QString("TS-2000")); QCOMPARE(global.rttyDiguHz, 2125);
        QVERIFY(!global.aiSerial2 && !global.aiSerial3 && !global.aiSerial4);
        QVERIFY(!global.digitalReportsSideband && !global.recenterVfo);
        QCOMPARE(global.serialNumber, QString("0000-0000"));
        QVERIFY(!global.rttyOffsetAEnabled && !global.rttyOffsetBEnabled);
        QCOMPARE(global.rttyDiglHz, 2125);
        QVERIFY(!global.pttEnabled && !global.pttUseCts && !global.pttUseDsr);
        QCOMPARE(global.pttDeviceSource, QString("None")); QVERIFY(global.pttSerialDevice.isEmpty());
        QCOMPARE(global.pttChannel, 1); QCOMPARE(global.pttSerialBaud, 115200);
        QCOMPARE(global.pttSerialParity, QString("None")); QCOMPARE(global.pttSerialDataBits, 8);
        QCOMPARE(global.pttSerialStopBits, QString("1"));
        global.aiEnabled = true; global.rttyDiglHz = -3000;
        QVERIFY(settings.setGlobal(global));
        QCOMPARE(store.value("Cat/AiEnabled").toString(), QString("True"));
        QCOMPARE(store.value("Cat/SendWelcome").toString(), QString("False"));
        global.rttyDiguHz = 3001; QVERIFY(!settings.setGlobal(global));
        configs[0].serialEnabled = true; configs[0].serialDevice = "saved-absent-device";
        configs[0].binding.primaryIncarnation = 42;
        CatSettings::save(store, configs[0]);
        QCOMPARE(store.value("Cat/Channels/1/SerialEnabled").toString(), QString("True"));
        QVERIFY(!store.contains("Cat/Channels/1/PrimaryIncarnation"));
        configs = CatSettings::load(store, model);
        QVERIFY(configs[0].serialEnabled); QCOMPARE(configs[0].serialDevice, QString("saved-absent-device"));
        QCOMPARE(configs[0].binding.primaryIncarnation, quint64(0));
        QVERIFY(store.save());
        AppSettings restored(directory.filePath("cat.settings")); restored.load();
        const QList<CatEndpointConfig> persisted = CatSettings::load(restored, model);
        QVERIFY(persisted[0].serialEnabled);
        QCOMPARE(persisted[0].serialDevice, QString("saved-absent-device"));
        QCOMPARE(restored.value("Cat/AiEnabled").toString(), QString("True"));
        CatSettings restoredGlobal(restored);
        QCOMPARE(restoredGlobal.global().rttyDiglHz, -3000);
        for (const QString& key : restored.allKeys()) {
            QVERIFY(!key.contains("Incarnation") && !key.contains("Session") && !key.contains("Guid") && !key.contains("Meter"));
        }
    }
    void runtimeGlobalPreferencesKeepPttIngressStopped()
    {
        AppSettings::instance().clear();
        RadioModel model; CatService& service=*model.catService(); service.startConfigured();
        QSignalSpy changed(&service,&CatService::globalConfigurationChanged);
        CatGlobalConfig baseline=service.globalConfig(); baseline.rttyOffsetAEnabled=true;
        QVERIFY(service.applyGlobalConfig(baseline)); QCOMPARE(changed.count(),1);
        QCOMPARE(AppSettings::instance().value("Cat/RttyOffsetAEnabled").toString(),QString("True"));
        for (int field=0; field<10; ++field) {
            CatGlobalConfig config=baseline;
            switch (field) {
            case 0: config.pttEnabled=true; break; case 1: config.pttDeviceSource="CAT1"; break;
            case 2: config.pttSerialDevice="absent-device"; break; case 3: config.pttUseCts=true; break;
            case 4: config.pttUseDsr=true; break; case 5: config.pttChannel=2; break;
            case 6: config.pttSerialBaud=9600; break; case 7: config.pttSerialParity="Odd"; break;
            case 8: config.pttSerialDataBits=7; break; case 9: config.pttSerialStopBits="2"; break;
            }
            QVERIFY(!service.applyGlobalConfig(config)); QCOMPARE(changed.count(),1);
            const CatGlobalConfig actual=service.globalConfig();
            QVERIFY(!actual.pttEnabled && actual.pttDeviceSource=="None" && actual.pttSerialDevice.isEmpty() && !actual.pttUseCts && !actual.pttUseDsr);
            QCOMPARE(actual.pttChannel,1); QCOMPARE(actual.pttSerialBaud,115200); QCOMPARE(actual.pttSerialParity,QString("None")); QCOMPARE(actual.pttSerialDataBits,8); QCOMPARE(actual.pttSerialStopBits,QString("1"));
        }
        service.beginRetirement(); baseline.rttyOffsetAEnabled=false; QVERIFY(!service.applyGlobalConfig(baseline)); QCOMPARE(changed.count(),1);
        RadioModel remote(RadioModel::Role::Remote); QVERIFY(!remote.catService()->applyGlobalConfig(baseline));
        AppSettings::instance().clear();
    }
    void nestedGlobalSaveNewestWins() {
        AppSettings& store = AppSettings::instance(); store.clear();
        RadioModel model; CatService& service = *model.catService();
        CatGlobalConfig old = service.globalConfig(); old.rigIdentity = "TS-50S"; old.rttyDiguHz = 100;
        CatGlobalConfig newer = old; newer.rigIdentity = "TS-480"; newer.rttyDiguHz = 999;
        bool nested = false;
        store.setChangeHook([&](const QString& key) {
            if (!nested && key == "Cat/SendWelcome") { nested = true; QVERIFY(service.applyGlobalConfig(newer)); }
        });
        const bool accepted = service.applyGlobalConfig(old);
        store.setChangeHook({});
        QVERIFY(nested); QVERIFY(!accepted);
        QCOMPARE(service.globalConfig().rigIdentity, newer.rigIdentity);
        QCOMPARE(store.value("Cat/RttyDiguHz").toInt(), 999);
    }
    void globalTupleIsCoherentDuringSave() {
        AppSettings& store = AppSettings::instance(); store.clear();
        RadioModel model; CatService& service = *model.catService();
        CatGlobalConfig desired = service.globalConfig(); desired.rigIdentity = "TS-480"; desired.rttyDiguHz = 999;
        bool observed = false; CatGlobalConfig seen;
        store.setChangeHook([&](const QString& key) {
            if (!observed && key == "Cat/SendWelcome") { observed = true; seen = service.globalConfig(); service.startConfigured(); }
        });
        const bool accepted = service.applyGlobalConfig(desired); store.setChangeHook({});
        QVERIFY(accepted); QVERIFY(observed); QCOMPARE(seen.rigIdentity, desired.rigIdentity); QCOMPARE(seen.rttyDiguHz, 999);
    }
    void nestedChannelSaveNewestWins() {
        AppSettings& store = AppSettings::instance(); store.clear();
        RadioModel model; CatService& service = *model.catService();
        CatEndpointConfig old; old.tcpPort = 12345; old.serialBaud = 9600;
        CatEndpointConfig newer = old; newer.tcpPort = 23456; newer.serialBaud = 38400;
        bool nested = false;
        store.setChangeHook([&](const QString& key) {
            if (!nested && key == "Cat/Channels/1/PrimarySliceId") { nested = true; QVERIFY(service.applyChannelConfig(1, newer)); }
        });
        const bool accepted = service.applyChannelConfig(1, old); store.setChangeHook({});
        QVERIFY(nested); QVERIFY(!accepted); QCOMPARE(service.channelConfig(1).tcpPort, 23456);
        QCOMPARE(store.value("Cat/Channels/1/TcpPort").toInt(), 23456);
        QCOMPARE(store.value("Cat/Channels/1/SerialBaud").toInt(), 38400);
    }
    void saveCallbackStopWinsRestart() {
        AppSettings& store=AppSettings::instance(); store.clear(); RadioModel model; CatService& service=*model.catService(); service.startConfigured();
        CatEndpointConfig desired=service.channelConfig(1); desired.tcpEnabled=true; desired.tcpPort=0;
        // An invalid proposal is rejected before hooks or teardown.
        int hooks=0; store.setChangeHook([&](const QString&) { ++hooks; service.stopAll(); });
        QVERIFY(!service.reconfigureChannel(1,desired)); QCOMPARE(hooks,0); QVERIFY(service.isStarted());
        desired.tcpEnabled=false; desired.serialBaud=19200;
        QVERIFY(service.reconfigureChannel(1,desired)); QVERIFY(hooks>0); QVERIFY(!service.isStarted()); QVERIFY(!service.isListening(1));
        QCOMPARE(store.value("Cat/Channels/1/SerialBaud").toInt(),19200); store.setChangeHook({});
    }
    void deletedServiceStopsSavingAndDoesNotNotify() {
        AppSettings& store=AppSettings::instance(); store.clear(); auto model=std::make_unique<RadioModel>();
        CatService* service=model->catService(); CatGlobalConfig desired=service->globalConfig(); desired.sendWelcome=true; desired.rigIdentity="TS-480";
        QPointer<CatService> alive(service); int writes=0;
        store.setChangeHook([&](const QString& key) { if (key.startsWith("Cat/")) { ++writes; model.reset(); } });
        QVERIFY(!service->reconfigureGlobal(desired)); store.setChangeHook({}); QVERIFY(!alive); QCOMPARE(writes,1);
        QVERIFY(store.value("Cat/RigIdentity").toString()!="TS-480");
    }
    void sameChannelStatusSupersessionDropsOldTransportSignal() {
        AppSettings::instance().clear(); RadioModel model; CatService& service=*model.catService();
        QTcpServer reservation; QVERIFY(reservation.listen(QHostAddress::LocalHost,0)); const int port=reservation.serverPort(); reservation.close();
        CatEndpointConfig config; config.tcpEnabled=true; config.tcpPort=port; QVERIFY(service.applyChannelConfig(1,config));
        bool replaced=false; int stale=0;
        connect(&service,&CatService::channelStateChanged,&model,[&](int channel,const QString& state) {
            if (channel==1 && state=="Listening" && !replaced) { replaced=true; CatEndpointConfig disabled=service.channelConfig(1); disabled.tcpEnabled=false; QVERIFY(service.reconfigureChannel(1,disabled)); }
        });
        connect(&service,&CatService::transportStateChanged,&model,[&](int channel,CatTransportKind,const QString& state) { if (channel==1 && replaced && state=="Listening") { ++stale; } });
        service.startConfigured(); QVERIFY(replaced); QCOMPARE(stale,0); QVERIFY(!service.isListening(1));
    }
    void retirementClearsInflightGlobalTuple() {
        AppSettings& store=AppSettings::instance(); store.clear(); RadioModel model; CatService& service=*model.catService();
        CatGlobalConfig desired=service.globalConfig(); desired.rigIdentity="TS-480";
        bool retired=false; store.setChangeHook([&](const QString& key) { if (!retired && key=="Cat/SendWelcome") { retired=true; service.beginRetirement(); } });
        const bool accepted=service.reconfigureGlobal(desired); store.setChangeHook({}); QVERIFY(!accepted); QVERIFY(retired);
        CatSettings persisted(store); QCOMPARE(service.globalConfig().rigIdentity,persisted.global().rigIdentity);
    }
    void transportEditDoesNotRebindReusedSlice() {
        AppSettings::instance().clear(); RadioModel model; model.addSlice(); model.addSlice(); CatService& service=*model.catService();
        CatEndpointConfig config; config.binding.primarySliceId=0; config.tcpPort=12345; QVERIFY(service.applyChannelConfig(1,config)); service.startConfigured();
        const quint64 incarnation=service.channelConfig(1).binding.primaryIncarnation;
        model.removeSlice(0); QCOMPARE(model.addSlice(),0); QVERIFY(model.sliceOwnership()->incarnation(0)!=incarnation);
        config=service.channelConfig(1); config.tcpPort=23456; QVERIFY(service.reconfigureChannel(1,config));
        QCOMPARE(service.channelConfig(1).binding.primaryIncarnation,incarnation);
        QVERIFY(!service.adapter().resolveSlice(service.channelConfig(1).binding,CatVfo::Primary));
        config=service.channelConfig(1); config.binding=service.adapter().snapshotBinding(config.binding);
        QVERIFY(service.reconfigureChannel(1,config)); QVERIFY(service.adapter().resolveSlice(service.channelConfig(1).binding,CatVfo::Primary));
    }
    void validation()
    {
        CatEndpointConfig config;
        config.tcpEnabled = true; QVERIFY(!CatSettings::validate(config));
        config.tcpPort = 65536; QVERIFY(!CatSettings::validate(config));
        config.tcpPort = 13013; QVERIFY(CatSettings::validate(config));
        config.serialEnabled = true; QVERIFY(!CatSettings::validate(config));
        config.serialDevice = "saved-device"; QVERIFY(CatSettings::validate(config));
        config.serialParity = "invented"; QVERIFY(!CatSettings::validate(config));
        config.serialParity = "None"; config.rigctldEnabled = true; QVERIFY(!CatSettings::validate(config));
        config.rigctldPort = 5555; QVERIFY(CatSettings::validate(config));
        config.tcpBindAddress = "invalid-address"; QVERIFY(!CatSettings::validate(config));
    }
};
QTEST_GUILESS_MAIN(TstCatSettings)
#include "tst_cat_settings.moc"
