// no-port-check: NereusSDR-original native CAT AI reporting integration tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include "core/AppSettings.h"
#include "core/cat/CatService.h"
#include "core/SliceOwnership.h"
#include "core/TxSliceArbiter.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
struct CatReportingFixture {
    RadioModel model;
    CatService& service{*model.catService()};
    QTcpSocket first;
    QTcpSocket second;
    qint64 clock{0};
    QList<QPair<int, QByteArray>> output;
    CatReportingFixture()
    {
        model.addSlice(); model.addSlice();
        model.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        model.sliceOwnership()->setOwner(1, SliceOwnership::stationDevice());
        model.sliceById(0)->setFrequency(14'074'000); model.sliceById(1)->setFrequency(7'074'000);
        service.reporter().setClockForTest([this] { return clock; });
        QObject::connect(&service, &CatService::messageLogged, &service, [this](int channel, bool inbound, const QByteArray& bytes) {
            if (!inbound) { output.append({channel, bytes}); }
        });
    }
    bool configure(int channel, bool reverse = false)
    {
        QTcpServer probe;
        if (!probe.listen(QHostAddress::LocalHost, 0)) { return false; }
        CatEndpointConfig config; config.tcpEnabled = true; config.tcpPort = probe.serverPort(); probe.close();
        config.binding.primarySliceId = reverse ? 1 : 0; config.binding.secondarySliceId = reverse ? 0 : 1;
        return service.applyChannelConfig(channel, config);
    }
    bool enable()
    {
        CatGlobalConfig global = service.globalConfig(); global.allowKenwoodAi = true; global.aiEnabled = true;
        if (!service.applyGlobalConfig(global)) { return false; }
        service.startConfigured(); return service.isListening(1);
    }
    void connectClient(QTcpSocket& socket, int channel = 1) { socket.connectToHost(QHostAddress::LocalHost, service.boundPort(channel)); }
    void flush() { service.reporter().flushPending(); }
};
class TstCatReporting : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings& settings = AppSettings::instance();
        for (const QString& key : settings.allKeys()) { if (key.startsWith("Cat/")) { settings.remove(key); } }
    }
    void actualModelBroadcast()
    {
        CatReportingFixture fixture; QVERIFY(fixture.configure(1)); QVERIFY(fixture.enable());
        fixture.connectClient(fixture.first); QTRY_COMPARE(fixture.service.clientCount(1), 1);
        fixture.model.sliceById(0)->setFrequency(14'075'000); fixture.flush();
        QTRY_VERIFY(fixture.first.bytesAvailable() > 0);
        QCOMPARE(fixture.first.readAll(), QByteArray("FA00014075000;"));
    }
    void exactIntervalLatestDuplicatesAndBothVfos()
    {
        CatReportingFixture f; QVERIFY(f.configure(1)); QVERIFY(f.enable()); f.connectClient(f.first);
        QTRY_COMPARE(f.service.clientCount(1), 1);
        f.model.sliceById(0)->setFrequency(14'075'000); f.flush();
        QCOMPARE(f.output, (QList<QPair<int,QByteArray>>{{1,"FA00014075000;"}})); f.output.clear();
        f.clock = 1; f.model.sliceById(0)->setFrequency(14'076'000); f.flush(); QVERIFY(f.output.isEmpty());
        f.clock = 100; f.model.sliceById(0)->setFrequency(14'077'000); f.flush(); QVERIFY(f.output.isEmpty());
        f.clock = 199; f.flush(); QVERIFY(f.output.isEmpty());
        f.clock = 200; f.flush(); QCOMPARE(f.output, (QList<QPair<int,QByteArray>>{{1,"FA00014077000;"}})); f.output.clear();
        f.clock = 201; emit f.model.sliceById(0)->frequencyChanged(14'077'000); f.flush();
        f.clock = 400; f.flush(); QVERIFY(f.output.isEmpty());
        f.clock = 401; f.model.sliceById(0)->setFrequency(14'078'000); f.model.sliceById(1)->setFrequency(7'075'000); f.flush();
        QCOMPARE(f.output.size(), 2); QVERIFY(f.output.contains({1,"FA00014078000;"})); QVERIFY(f.output.contains({1,"FB00007075000;"}));
        f.output.clear(); f.clock = 402; f.model.sliceById(0)->setFrequency(14'079'000); f.flush();
        f.clock = 403; f.model.sliceById(0)->setFrequency(14'078'000); f.flush();
        f.clock = 601; f.flush(); QVERIFY(f.output.isEmpty());
    }
    void mappingGlobalRoutesAndSetterReply()
    {
        CatReportingFixture f; QVERIFY(f.configure(1)); QVERIFY(f.configure(2, true)); QVERIFY(f.enable());
        f.connectClient(f.first); f.connectClient(f.second, 2);
        QTRY_COMPARE(f.service.clientCount(1), 1); QTRY_COMPARE(f.service.clientCount(2), 1);
        f.first.write("FA00014076000;ID;");
        QTRY_COMPARE(f.model.sliceById(0)->frequency(), 14'076'000.0);
        f.flush();
        QTRY_VERIFY(f.first.bytesAvailable() >= 20); QTRY_VERIFY(f.second.bytesAvailable() >= 14);
        const QByteArray first = f.first.readAll(); QVERIFY(first.contains("FA00014076000;")); QVERIFY(first.contains("ID019;"));
        QCOMPARE(first.count("FA00014076000;"), 1); QCOMPARE(f.second.readAll(), QByteArray("FB00014076000;"));
        f.output.clear();
        QVERIFY(f.model.txSliceArbiter()->requestHandoff(1, SliceOwnership::stationDevice())); f.flush();
        QVERIFY(f.output.contains({1,"ZZSW1;"})); QVERIFY(f.output.contains({2,"ZZSW0;"})); f.output.clear();
        CatGlobalConfig global=f.service.globalConfig(); global.aiTcp=false; QVERIFY(f.service.applyGlobalConfig(global));
        f.clock=200; f.model.sliceById(0)->setFrequency(14'077'000); f.flush(); QVERIFY(f.output.isEmpty());
        global.aiTcp=true; global.aiEnabled=false; QVERIFY(f.service.applyGlobalConfig(global));
        f.model.sliceById(0)->setFrequency(14'078'000); f.flush(); QVERIFY(f.output.isEmpty());
        global.aiEnabled=true; global.allowKenwoodAi=false; QVERIFY(f.service.applyGlobalConfig(global));
        f.model.sliceById(0)->setFrequency(14'079'000); f.flush(); QVERIFY(f.output.isEmpty());
        global.allowKenwoodAi=true; QVERIFY(f.service.applyGlobalConfig(global));
        f.model.sliceById(0)->setFrequency(14'080'000); f.flush(); QCOMPARE(f.output.size(),2);
    }
    void globalAiCommandsAndTesterIsolation()
    {
        CatReportingFixture f; QVERIFY(f.configure(1)); QVERIFY(f.enable()); f.connectClient(f.first);
        QTRY_COMPARE(f.service.clientCount(1),1);
        const quint64 tester=f.service.openSession(1,CatTransportKind::Tester);
        QSignalSpy config(&f.service,&CatService::globalConfigurationChanged);
        QCOMPARE(f.service.processFrame(tester,"AI0;"),QByteArray()); QCOMPARE(config.size(),1);
        f.model.sliceById(0)->setFrequency(14'075'000); f.flush(); QVERIFY(f.output.isEmpty());
        QCOMPARE(f.service.processFrame(tester,"ZZAI1;"),QByteArray()); QCOMPARE(config.size(),2);
        f.model.sliceById(0)->setFrequency(14'076'000); f.flush(); QCOMPARE(f.output.size(),1);
        QCOMPARE(f.output.first().second,QByteArray("FA00014076000;"));
        QVERIFY(!f.service.session(tester)->context().transmitAllowed);
        QCOMPARE(f.service.processFrame(tester,"TX;"),QByteArray("?;")); QCOMPARE(f.service.processFrame(tester,"ZZOA1;"),QByteArray("?;"));
    }
    void pendingUsesCurrentClientsAndLastDisconnectClears()
    {
        CatReportingFixture f; QVERIFY(f.configure(1)); QVERIFY(f.enable()); f.connectClient(f.first);
        QTRY_COMPARE(f.service.clientCount(1),1);
        f.model.sliceById(0)->setFrequency(14'075'000); f.flush(); f.output.clear();
        f.clock=1; f.model.sliceById(0)->setFrequency(14'076'000); f.flush();
        f.connectClient(f.second); QTRY_COMPARE(f.service.clientCount(1),2);
        f.clock=200; f.flush(); QCOMPARE(f.output.size(),2);
        for (const auto& item:f.output) { QCOMPARE(item.second,QByteArray("FA00014076000;")); } f.output.clear();
        f.clock=201; f.model.sliceById(0)->setFrequency(14'077'000); f.flush();
        f.first.abort(); f.second.abort(); QTRY_COMPARE(f.service.clientCount(1),0);
        f.connectClient(f.first); QTRY_COMPARE(f.service.clientCount(1),1);
        f.clock=400; f.flush(); QVERIFY(f.output.isEmpty());
        f.model.sliceById(0)->setFrequency(14'078'000); f.flush(); QCOMPARE(f.output.size(),1);
    }
    void survivingTargetsAndIncarnation_data()
    {
        QTest::addColumn<int>("removed"); QTest::newRow("A removed")<<0; QTest::newRow("B removed")<<1;
    }
    void survivingTargetsAndIncarnation()
    {
        QFETCH(int,removed);
        CatReportingFixture f; QVERIFY(f.configure(1)); QVERIFY(f.enable()); f.connectClient(f.first);
        QTRY_COMPARE(f.service.clientCount(1),1);
        const int surviving=1-removed;
        f.model.sliceById(surviving)->setFrequency(14'075'000); f.flush(); QCOMPARE(f.output.size(),1); f.output.clear();
        f.clock=1; f.model.sliceById(surviving)->setFrequency(14'076'000); f.flush();
        f.model.removeSlice(removed);
        f.clock=200; f.flush();
        QVERIFY(f.output.contains({1,surviving==0?QByteArray("FA00014076000;"):QByteArray("FB00014076000;")}));
        // Removing the actual TX-bound A also causes the native arbiter to select B.
        if (removed==0) { QCOMPARE(f.output.size(),2); QVERIFY(f.output.contains({1,"ZZSW1;"})); }
        else { QCOMPARE(f.output.size(),1); }
        f.output.clear();
        QCOMPARE(f.model.addSlice(),removed); f.model.sliceOwnership()->setOwner(removed,SliceOwnership::stationDevice());
        f.model.sliceById(removed)->setFrequency(7'100'000); f.flush(); QVERIFY(f.output.isEmpty());
        f.clock=400; f.model.sliceById(surviving)->setFrequency(14'077'000); f.flush(); QCOMPARE(f.output.size(),1);
    }
    void reportCallbackRestartDropsOldRecipients()
    {
        CatReportingFixture f; QVERIFY(f.configure(1)); QVERIFY(f.enable()); f.connectClient(f.first); f.connectClient(f.second);
        QTRY_COMPARE(f.service.clientCount(1),2);
        bool restarted=false;
        connect(&f.service,&CatService::messageLogged,&f.service,[&](int,bool inbound,const QByteArray&) {
            if (!inbound && !restarted) { restarted=true; f.service.stopAll(); f.service.startConfigured(); }
        });
        f.model.sliceById(0)->setFrequency(14'075'000); f.flush(); QVERIFY(restarted); QCOMPARE(f.output.size(),1);
        QVERIFY(f.service.isListening(1)); QCOMPARE(f.service.clientCount(1),0);
        f.clock=200; f.flush(); QCOMPARE(f.output.size(),1);
    }
    void settledStateRttyAndRestart()
    {
        CatReportingFixture f; QVERIFY(f.configure(1)); QVERIFY(f.enable()); f.connectClient(f.first);
        QTRY_COMPARE(f.service.clientCount(1),1);
        CatGlobalConfig global=f.service.globalConfig(); global.rttyOffsetAEnabled=true; global.rttyDiguHz=2125;
        QVERIFY(f.service.applyGlobalConfig(global)); f.model.sliceById(0)->setDspMode(DSPMode::DIGU); f.flush(); QVERIFY(f.output.isEmpty());
        QObject::connect(f.model.sliceById(0), &SliceModel::frequencyChanged, &f.service, [&](double frequency) {
            if (frequency==14'075'000) { f.model.sliceById(0)->setFrequency(14'076'000); }
        });
        f.model.sliceById(0)->setFrequency(14'075'000); f.flush();
        QCOMPARE(f.output, (QList<QPair<int,QByteArray>>{{1,"FA00014076000;"}})); f.output.clear();
        const quint64 tester=f.service.openSession(1,CatTransportKind::Tester);
        QCOMPARE(f.service.processFrame(tester,"FA;"),QByteArray("FA00014078125;"));
        f.clock=1; f.model.sliceById(0)->setFrequency(14'077'000); f.flush();
        f.service.stopAll(); QVERIFY(f.configure(1,true)); f.service.startConfigured();
        QTRY_COMPARE(f.first.state(),QAbstractSocket::UnconnectedState); f.connectClient(f.first); QTRY_COMPARE(f.service.clientCount(1),1);
        f.clock=200; f.flush(); QVERIFY(f.output.isEmpty());
        f.model.sliceById(1)->setFrequency(7'075'000); f.flush(); QCOMPARE(f.output.first().second,QByteArray("FA00007075000;"));
    }
};
QTEST_GUILESS_MAIN(TstCatReporting)
#include "tst_cat_reporting.moc"
