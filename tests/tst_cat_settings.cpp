// no-port-check: NereusSDR-original CAT policy/lifecycle regression tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QTemporaryDir>
#include "core/AppSettings.h"
#include "core/cat/CatSettings.h"
#include "core/cat/CatService.h"
#include "models/RadioModel.h"
using namespace NereusSDR;
class TstCatSettings : public QObject {
    Q_OBJECT
private slots:
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
