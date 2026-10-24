// no-port-check: NereusSDR-original CAT policy/lifecycle regression tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-06 Default ports for all four channels. J.J. Boyd (KG4VCF),
//            AI-assisted via Anthropic Claude Code.
// 2026-10-11 One serial device under two spellings. J.J. Boyd (KG4VCF),
//            AI-assisted via Anthropic Claude Code.
// 2026-10-12 Each refused setting names the rule it broke. J.J. Boyd (KG4VCF),
//            AI-assisted via Anthropic Claude Code.
// 2026-10-12 Invalid reported radio model refusal uses operator words.
//            J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-22: ports a test hands to a later listener come from
// unusedLoopbackPort() (UnusedLoopbackPort.h), below the kernel's ephemeral
// range, so a parallel test cannot take one between the pick and the listen.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "UnusedLoopbackPort.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QRegularExpression>
#include <functional>
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
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(configs[i].tcpPort, 13013 + i); QCOMPARE(configs[i].rigctldPort, 4532 + i);
            QCOMPARE(configs[i].tcpBindAddress, QString("127.0.0.1")); QCOMPARE(configs[i].rigctldBindAddress, QString("127.0.0.1"));
        }
        for (const CatEndpointConfig& config : configs) {
            QVERIFY(!config.tcpEnabled && !config.serialEnabled && !config.ptyEnabled && !config.rigctldEnabled);
            QCOMPARE(config.serialBaud, 115200); QCOMPARE(config.serialParity, QString("None"));
            QCOMPARE(config.serialDataBits, 8); QCOMPARE(config.serialStopBits, QString("1"));
        }
        QCOMPARE(configs[1].binding.primarySliceId, -1);
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
        const int port=unusedLoopbackPort(); QVERIFY(port != 0);
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
    void serialRefusalNamesTheDeviceOrTheFormat() {
        const QString noDevice=QStringLiteral("This serial port has no device. Choose a device, then turn the port on.");
        const QString badFormat=QStringLiteral("The serial port's baud rate, parity, data bits or stop bits are not valid.");
        CatEndpointConfig config; QString reason=QStringLiteral("left over");
        QVERIFY(CatSettings::validate(config,&reason)); QVERIFY(reason.isEmpty());
        config.serialEnabled=true; QVERIFY(!CatSettings::validate(config,&reason)); QCOMPARE(reason,noDevice);
        config.serialDevice=QStringLiteral("   "); QVERIFY(!CatSettings::validate(config,&reason)); QCOMPARE(reason,noDevice);
        // Both wrong: the missing device is said first, as it was checked first.
        config.serialParity=QStringLiteral("invented"); QVERIFY(!CatSettings::validate(config,&reason)); QCOMPARE(reason,noDevice);
        config.serialDevice=QStringLiteral("saved-device"); QVERIFY(!CatSettings::validate(config,&reason)); QCOMPARE(reason,badFormat);
        // The format is checked on a port that is off too, as before.
        config.serialEnabled=false; config.serialDevice.clear(); QVERIFY(!CatSettings::validate(config,&reason)); QCOMPARE(reason,badFormat);
        for (const auto& broken:{std::function<void(CatEndpointConfig&)>([](CatEndpointConfig& c) { c.serialBaud=0; }),std::function<void(CatEndpointConfig&)>([](CatEndpointConfig& c) { c.serialDataBits=9; }),std::function<void(CatEndpointConfig&)>([](CatEndpointConfig& c) { c.serialStopBits=QStringLiteral("3"); })}) {
            CatEndpointConfig other; broken(other); QVERIFY(!CatSettings::validate(other,&reason)); QCOMPARE(reason,badFormat);
        }
        config.serialParity=QStringLiteral("None"); QVERIFY(CatSettings::validate(config,&reason)); QVERIFY(reason.isEmpty());
        // The other reasons keep their words.
        CatEndpointConfig tcp; tcp.tcpEnabled=true; QVERIFY(!CatSettings::validate(tcp,&reason)); QCOMPARE(reason,QString("Invalid CAT listener address or port."));
        CatEndpointConfig channel; channel.channel=5; QVERIFY(!CatSettings::validate(channel,&reason)); QCOMPARE(reason,QString("Invalid CAT channel or slice selection."));
        CatEndpointConfig dialect; dialect.ptyDialect=QStringLiteral("invented"); QVERIFY(!CatSettings::validate(dialect,&reason)); QCOMPARE(reason,QString("Invalid CAT PTY dialect."));
    }
    void globalRefusalNamesTheRuleItBroke_data() {
        QTest::addColumn<CatGlobalConfig>("config"); QTest::addColumn<QString>("sentence");
        const auto row=[](const char* name,const std::function<void(CatGlobalConfig&)>& change,const char* sentence) {
            // A valid Input PTT that is on, so one change breaks one rule.
            CatGlobalConfig config; config.pttEnabled=true; config.pttDeviceSource=QStringLiteral("Physical"); config.pttUseCts=true; config.pttSerialDevice=QStringLiteral("ptt-device");
            change(config); QTest::newRow(name)<<config<<QString::fromLatin1(sentence);
        };
        row("valid",[](CatGlobalConfig&) {},"");
        row("no source",[](CatGlobalConfig& c) { c.pttDeviceSource=QStringLiteral("None"); },"Input PTT has no input source. Choose a source, then turn it on.");
        row("no input",[](CatGlobalConfig& c) { c.pttUseCts=false; c.pttUseDsr=false; },"Input PTT has no input to watch. Tick the CTS or the DSR input, then turn it on.");
        row("no device",[](CatGlobalConfig& c) { c.pttSerialDevice=QStringLiteral("  "); },"Input PTT has no physical device. Enter the device, then turn it on.");
        row("baud",[](CatGlobalConfig& c) { c.pttSerialBaud=0; },"Input PTT's baud rate, parity, data bits or stop bits are not valid.");
        row("parity",[](CatGlobalConfig& c) { c.pttSerialParity=QStringLiteral("invented"); },"Input PTT's baud rate, parity, data bits or stop bits are not valid.");
        row("data bits",[](CatGlobalConfig& c) { c.pttSerialDataBits=4; },"Input PTT's baud rate, parity, data bits or stop bits are not valid.");
        row("stop bits",[](CatGlobalConfig& c) { c.pttSerialStopBits=QStringLiteral("3"); },"Input PTT's baud rate, parity, data bits or stop bits are not valid.");
        row("DIGU offset low",[](CatGlobalConfig& c) { c.rttyDiguHz=CatDefaults::kRttyMinimumHz-1; },"The RTTY offset is out of range.");
        row("DIGU offset high",[](CatGlobalConfig& c) { c.rttyDiguHz=CatDefaults::kRttyMaximumHz+1; },"The RTTY offset is out of range.");
        row("DIGL offset low",[](CatGlobalConfig& c) { c.rttyDiglHz=CatDefaults::kRttyMinimumHz-1; },"The RTTY offset is out of range.");
        row("DIGL offset high",[](CatGlobalConfig& c) { c.rttyDiglHz=CatDefaults::kRttyMaximumHz+1; },"The RTTY offset is out of range.");
        row("identity",[](CatGlobalConfig& c) { c.rigIdentity=QStringLiteral("invented"); },"The CAT radio model is not valid.");
        row("port number low",[](CatGlobalConfig& c) { c.pttChannel=0; },"The Input PTT CAT port number is not valid.");
        row("port number high",[](CatGlobalConfig& c) { c.pttChannel=5; },"The Input PTT CAT port number is not valid.");
        row("source",[](CatGlobalConfig& c) { c.pttDeviceSource=QStringLiteral("CAT5"); },"The Input PTT source is not valid.");
    }
    void globalRefusalNamesTheRuleItBroke() {
        QFETCH(CatGlobalConfig,config); QFETCH(QString,sentence);
        QString reason=QStringLiteral("left over");
        QCOMPARE(CatSettings::validateGlobal(config,&reason),sentence.isEmpty()); QCOMPARE(reason,sentence);
        // No reason asked for: the same answer.
        QCOMPARE(CatSettings::validateGlobal(config),sentence.isEmpty());
    }
    void globalRulesAcceptAndRefuseWhatTheyDid() {
        // The serial format rule is the endpoint's, read here through validate().
        const auto formatValid=[](const CatGlobalConfig& c) {
            CatEndpointConfig endpoint; endpoint.serialBaud=c.pttSerialBaud; endpoint.serialParity=c.pttSerialParity; endpoint.serialDataBits=c.pttSerialDataBits; endpoint.serialStopBits=c.pttSerialStopBits;
            return CatSettings::validate(endpoint);
        };
        // The rule list as it stood when it was one test with no reason.
        const auto refusedBefore=[&formatValid](const CatGlobalConfig& config) {
            return config.rttyDiguHz < CatDefaults::kRttyMinimumHz || config.rttyDiguHz > CatDefaults::kRttyMaximumHz
                || config.rttyDiglHz < CatDefaults::kRttyMinimumHz || config.rttyDiglHz > CatDefaults::kRttyMaximumHz
                || !QStringList{"PowerSDR", "TS-2000", "TS-50S", "TS-480"}.contains(config.rigIdentity)
                || config.pttChannel < 1 || config.pttChannel > 4
                || !QStringList{"None", "CAT1", "CAT2", "CAT3", "CAT4", "Physical"}.contains(config.pttDeviceSource)
                || (config.pttEnabled && (config.pttDeviceSource == "None" || (!config.pttUseCts && !config.pttUseDsr)))
                || (config.pttEnabled && config.pttDeviceSource == "Physical" && config.pttSerialDevice.trimmed().isEmpty())
                || !formatValid(config);
        };
        int accepted=0; int refused=0;
        for (int digu:{-3001,-3000,2125,3000,3001}) { for (int digl:{-3001,2125,3001}) { for (const char* identity:{"TS-2000","PowerSDR","invented"}) {
        for (int channel:{0,1,4,5}) { for (const char* source:{"None","CAT1","CAT4","Physical","CAT5",""}) { for (bool enabled:{false,true}) {
        for (bool cts:{false,true}) { for (bool dsr:{false,true}) { for (const char* device:{""," ","ptt-device"}) { for (const char* parity:{"None","invented"}) {
            CatGlobalConfig config; config.rttyDiguHz=digu; config.rttyDiglHz=digl; config.rigIdentity=QString::fromLatin1(identity); config.pttChannel=channel;
            config.pttDeviceSource=QString::fromLatin1(source); config.pttEnabled=enabled; config.pttUseCts=cts; config.pttUseDsr=dsr;
            config.pttSerialDevice=QString::fromLatin1(device); config.pttSerialParity=QString::fromLatin1(parity);
            QString reason=QStringLiteral("left over"); const bool valid=CatSettings::validateGlobal(config,&reason);
            if (valid==refusedBefore(config) || valid!=reason.isEmpty()) { QFAIL(qPrintable(QStringLiteral("digu %1 digl %2 identity %3 port %4 source %5 on %6 cts %7 dsr %8 device '%9' parity %10: %11").arg(digu).arg(digl).arg(QLatin1String(identity)).arg(channel).arg(QLatin1String(source)).arg(enabled).arg(cts).arg(dsr).arg(QLatin1String(device),QLatin1String(parity),reason))); }
            ++(valid ? accepted : refused);
        } } } } } } } } } }
        QVERIFY(accepted>0); QVERIFY(refused>accepted);
    }
    void serviceHandsBackTheReasonItRefusedFor() {
        AppSettings::instance().clear(); RadioModel model; CatService& service=*model.catService(); QString reason=QStringLiteral("left over");
        CatEndpointConfig one=service.channelConfig(1); one.serialEnabled=true;
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression(QStringLiteral("This serial port has no device")));
        QVERIFY(!service.reconfigureChannel(1,one,&reason)); QCOMPARE(reason,QString("This serial port has no device. Choose a device, then turn the port on."));
        one.serialDevice=QStringLiteral("shared-device"); QVERIFY(service.reconfigureChannel(1,one,&reason)); QVERIFY(reason.isEmpty());
        // Another port asks for the device port 1 holds.
        CatEndpointConfig three=service.channelConfig(3); three.serialEnabled=true; three.serialDevice=QStringLiteral(" shared-device ");
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression(QStringLiteral("CAT 3 refused: serial device")));
        reason=QStringLiteral("left over"); QVERIFY(!service.reconfigureChannel(3,three,&reason)); QCOMPARE(reason,QString("That serial device is already used by CAT 1."));
        // Input PTT asks for it as its own physical device.
        CatGlobalConfig global=service.globalConfig(); global.pttEnabled=true; global.pttDeviceSource=QStringLiteral("Physical"); global.pttUseCts=true; global.pttSerialDevice=QStringLiteral("shared-device");
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression(QStringLiteral("CAT PTT refused: serial device")));
        reason=QStringLiteral("left over"); QVERIFY(!service.reconfigureGlobal(global,&reason)); QCOMPARE(reason,QString("That serial device is already used by CAT 1."));
        global.pttSerialDevice=QStringLiteral("ptt-device"); QVERIFY(service.reconfigureGlobal(global,&reason)); QVERIFY(reason.isEmpty());
        // A port asks for the device Input PTT now holds.
        CatEndpointConfig two=service.channelConfig(2); two.serialEnabled=true; two.serialDevice=QStringLiteral("ptt-device");
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression(QStringLiteral("CAT 2 refused: serial device")));
        reason=QStringLiteral("left over"); QVERIFY(!service.reconfigureChannel(2,two,&reason)); QCOMPARE(reason,QString("That serial device is already used by Input PTT."));
        // A rule of the global settings.
        global.pttUseCts=false;
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression(QStringLiteral("Input PTT has no input to watch")));
        reason=QStringLiteral("left over"); QVERIFY(!service.reconfigureGlobal(global,&reason)); QCOMPARE(reason,QString("Input PTT has no input to watch. Tick the CTS or the DSR input, then turn it on."));
        // Refused with no cause to name: nothing is said.
        reason=QStringLiteral("left over"); QVERIFY(!service.reconfigureChannel(5,one,&reason)); QVERIFY(reason.isEmpty());
        RadioModel window(RadioModel::Role::Remote);
        reason=QStringLiteral("left over"); QVERIFY(!window.catService()->reconfigureChannel(1,CatEndpointConfig{},&reason)); QVERIFY(reason.isEmpty());
        reason=QStringLiteral("left over"); QVERIFY(!window.catService()->reconfigureGlobal(CatGlobalConfig{},&reason)); QVERIFY(reason.isEmpty());
    }
    void serialDeviceNamesCompareAsTheSystemDoes() {
        // Windows: one device whatever the case, with or without the \\.\ prefix.
        QVERIFY(CatSettings::sameSerialDevice("COM10", "com10", true));
        QVERIFY(CatSettings::sameSerialDevice("\\\\.\\COM10", "COM10", true));
        QVERIFY(CatSettings::sameSerialDevice("\\\\.\\com10", " COM10 ", true));
        QVERIFY(!CatSettings::sameSerialDevice("COM1", "COM10", true));
        QCOMPARE(CatSettings::serialDeviceKey("\\\\.\\com10", true), QString("COM10"));
        // Elsewhere a device path is case-sensitive; only surrounding space is dropped.
        QVERIFY(!CatSettings::sameSerialDevice("/dev/ttyUSB0", "/dev/ttyusb0", false));
        QVERIFY(CatSettings::sameSerialDevice("/dev/ttyUSB0 ", "/dev/ttyUSB0", false));
        QCOMPARE(CatSettings::serialDeviceKey("\\\\.\\com10", false), QString("\\\\.\\com10"));
    }
};
QTEST_GUILESS_MAIN(TstCatSettings)
#include "tst_cat_settings.moc"
