// no-port-check: NereusSDR-original native CAT loopback integration tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include "core/AppSettings.h"
#include "core/cat/CatService.h"
#include "core/cat/CatStreamFramer.h"
#include "core/SliceOwnership.h"
#include "CatFixtureHarness.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
using namespace NereusSDR;
static quint16 unusedCatPort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) { return 0; }
    return probe.serverPort();
}
class TstCatTcp : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings& settings = AppSettings::instance();
        for (const QString& key : settings.allKeys()) {
            if (key.startsWith("Cat/")) { settings.remove(key); }
        }
    }
    void framerBoundariesAndRecovery()
    {
        CatStreamFramer framer;
        QCOMPARE(framer.feed("FA"), QList<QByteArray>());
        QCOMPARE(framer.feed(";ID;"), (QList<QByteArray>{"FA;", "ID;"}));
        const QByteArray maximum = "ZZGA" + QByteArray(36, 'a') + ';';
        QCOMPARE(maximum.size(), 41);
        QCOMPARE(framer.feed(maximum), QList<QByteArray>{maximum});
        QCOMPARE(framer.feed(QByteArray(40, 'x')), QList<QByteArray>());
        QCOMPARE(framer.bufferedBytes(), 40);
        QCOMPARE(framer.feed(";"), QList<QByteArray>{QByteArray(40, 'x') + ';'});
        QCOMPARE(framer.feed(QByteArray(41, 'x')), QList<QByteArray>{QByteArray()});
        QCOMPARE(framer.bufferedBytes(), 0);
        QCOMPARE(framer.feed(QByteArray(1'000'000, 'x')), QList<QByteArray>());
        QCOMPARE(framer.bufferedBytes(), 0);
        QCOMPARE(framer.feed(";ID;"), QList<QByteArray>{"ID;"});
        QCOMPARE(framer.feed("\r\nfa;\nZZMN  MiXeD Payload ;"), (QList<QByteArray>{"fa;", "ZZMN  MiXeD Payload ;"}));
        QCOMPARE(framer.feed("ZZMNab\r\ncd;"), QList<QByteArray>{"ZZMNab\r\ncd;"});
        QCOMPARE(framer.feed("FA"), QList<QByteArray>()); framer.reset();
        QCOMPARE(framer.feed("ID;"), QList<QByteArray>{"ID;"});
        QByteArray many;
        for (int i = 0; i < 20'000; ++i) { many += "ID;"; }
        QCOMPARE(framer.feed(many).size(), 20'000); QCOMPARE(framer.bufferedBytes(), 0);
    }
    void lowLevelEphemeralAndAbortDeletion()
    {
        auto transport = std::make_unique<CatTcpTransport>();
        QVERIFY(transport->start(QHostAddress::LocalHost, 0));
        QVERIFY(transport->boundPort() > 0); QCOMPARE(transport->boundAddress(), QHostAddress(QHostAddress::LocalHost));
        QPointer<QTcpSocket> accepted;
        connect(transport.get(), &CatTcpTransport::clientAccepted, this, [&](QTcpSocket* socket) {
            accepted = socket; QVERIFY(transport->attachSession(1, socket));
        });
        QTcpSocket client; client.connectToHost(QHostAddress::LocalHost, transport->boundPort());
        QTRY_VERIFY(accepted && transport->clientCount() == 1);
        connect(accepted, &QTcpSocket::disconnected, this, [&] { transport.reset(); });
        transport->closeSession(1);
        QVERIFY(!transport);
    }
    void twoClientsFramingGuidAndLimits()
    {
        RadioModel model; model.addSlice(); model.addSlice();
        model.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        model.sliceOwnership()->setOwner(1, SliceOwnership::stationDevice());
        model.sliceById(0)->setFrequency(14'074'000);
        CatService& service = *model.catService();
        CatEndpointConfig config; config.tcpEnabled = true; config.tcpPort = unusedCatPort();
        config.binding.primarySliceId = 0; config.binding.secondarySliceId = 1;
        QVERIFY(config.tcpPort > 0); QVERIFY(service.applyChannelConfig(1, config)); service.startConfigured();
        QTcpSocket first, second;
        first.connectToHost(QHostAddress::LocalHost, config.tcpPort); second.connectToHost(QHostAddress::LocalHost, config.tcpPort);
        QTRY_COMPARE(service.clientCount(1), 2);
        first.write("FA"); second.write("ID;"); QTRY_VERIFY(second.bytesAvailable() > 0);
        QCOMPARE(second.readAll(), QByteArray("ID019;")); QCOMPARE(first.bytesAvailable(), 0);
        first.write(";fa;ID;"); QTRY_VERIFY(first.bytesAvailable() >= 34);
        QCOMPARE(first.readAll(), QByteArray("FA00014074000;FA00014074000;ID019;"));
        first.write(QByteArray(41, 'x') + QByteArray(100'000, 'x') + ";ID;");
        QTRY_VERIFY(first.bytesAvailable() >= 8); QCOMPARE(first.readAll(), QByteArray("?;ID019;"));
        const QByteArray guid = "00112233-4455-6677-8899-aabbccddeeff";
        const QByteArray other = "11223344-5566-7788-99aa-bbccddeeff00";
        first.write("zzga" + guid.toUpper() + ";ZZGA" + guid + ";ZZGA" + other + ';');
        second.write("ZZGA" + guid + ';');
        QTRY_VERIFY(first.bytesAvailable() >= 123); QTRY_VERIFY(second.bytesAvailable() >= 41);
        QCOMPARE(first.readAll(), "ZZGA" + guid + ";ZZGA" + guid + ";ZZGA" + other + ';');
        QCOMPARE(second.readAll(), "ZZGA" + guid + ';');
        service.sendToGuid(QUuid(QString::fromLatin1(guid)), "SHARED;");
        QTRY_VERIFY(first.bytesAvailable() > 0); QTRY_VERIFY(second.bytesAvailable() > 0);
        QCOMPARE(first.readAll(), QByteArray("SHARED;")); QCOMPARE(second.readAll(), QByteArray("SHARED;"));
        first.write("ZZGA" + guid + "junk;ZZGA" + QByteArray(36, 'x') + ";ZZGR" + guid + ';');
        QTRY_VERIFY(first.bytesAvailable() >= 45); QCOMPARE(first.readAll(), "?;?;ZZGR" + guid + ';');
        service.sendToGuid(QUuid(QString::fromLatin1(guid)), "SECOND;");
        QTRY_VERIFY(second.bytesAvailable() > 0); QCOMPARE(second.readAll(), QByteArray("SECOND;")); QCOMPARE(first.bytesAvailable(), 0);
        service.sendToGuid(QUuid(QString::fromLatin1(other)), "FIRST;");
        QTRY_VERIFY(first.bytesAvailable() > 0); QCOMPARE(first.readAll(), QByteArray("FIRST;"));
        first.write("ZZZZ;"); QTRY_VERIFY(first.bytesAvailable() > 0);
        QCOMPARE(first.readAll(), QByteArray("?;")); QVERIFY(service.isListening(1)); QCOMPARE(service.clientCount(1), 2);
        first.write("ZZML;"); QTRY_VERIFY(first.bytesAvailable() > 41);
        const QByteArray modes = first.readAll(); QVERIFY(modes.startsWith("ZZML")); QVERIFY(modes.endsWith(';')); QVERIFY(modes.size() > 41);
        first.abort(); QTRY_COMPARE(service.clientCount(1), 1);
        service.sendToGuid(QUuid(QString::fromLatin1(guid)), "SURVIVING;");
        QTRY_VERIFY(second.bytesAvailable() > 0); QCOMPARE(second.readAll(), QByteArray("SURVIVING;"));
        service.sendToGuid(QUuid(QString::fromLatin1(other)), "RETIRED;");
        first.connectToHost(QHostAddress::LocalHost, config.tcpPort); QTRY_COMPARE(service.clientCount(1), 2);
        service.sendToGuid(QUuid(QString::fromLatin1(guid)), "CURRENT;");
        QTRY_VERIFY(second.bytesAvailable() > 0); QCOMPARE(second.readAll(), QByteArray("CURRENT;")); QCOMPARE(first.bytesAvailable(), 0);
        quint64 firstId = 0;
        for (quint64 id : service.sessionIds(1)) { if (!service.session(id)->hasGuid(QUuid(QString::fromLatin1(guid)))) { firstId = id; } }
        QVERIFY(firstId);
        service.sendToSession(firstId, QByteArray(CatTcpTransport::kMaximumOutputBytes + 1, 'x'));
        QTRY_COMPARE(service.clientCount(1), 1);
        for (quint64 id : service.sessionIds(1)) {
            for (int i = 0; i < 6; ++i) { service.sendToSession(id, QByteArray(CatTcpTransport::kMaximumOutputBytes, 'x')); }
        }
        QCOMPARE(service.clientCount(1), 0); QVERIFY(service.sessionIds(1).isEmpty());
        service.sendToGuid(QUuid(QString::fromLatin1(guid)), "STALE;");
    }
    void occupiedBindAndWelcome()
    {
        QTcpServer occupied; QVERIFY(occupied.listen(QHostAddress::LocalHost, 0));
        RadioModel model; CatService& service = *model.catService();
        CatEndpointConfig config; config.tcpEnabled = true; config.tcpPort = occupied.serverPort();
        QVERIFY(service.applyChannelConfig(1, config)); service.startConfigured();
        QVERIFY(!service.isListening(1)); QVERIFY(service.channelState(1).startsWith("TCP error:"));
        QCOMPARE(service.boundPort(1), quint16(0)); QCOMPARE(service.clientCount(1), 0);
        service.stopAll(); occupied.close();
        CatGlobalConfig global = service.globalConfig(); global.sendWelcome = true; QVERIFY(service.applyGlobalConfig(global));
        service.startConfigured(); QVERIFY(service.isListening(1));
        QCOMPARE(service.boundPort(1), quint16(config.tcpPort)); QCOMPARE(service.boundAddress(1), QHostAddress(QHostAddress::LocalHost));
        QTcpSocket client; client.connectToHost(QHostAddress::LocalHost, config.tcpPort);
        QTRY_VERIFY(client.bytesAvailable() > 0); QCOMPARE(client.readAll(), QByteArray("#NereusSDR TCP/IP Cat#;"));
    }
    void logCallbackStopsFramesAndDeletesModel()
    {
        auto model = std::make_unique<RadioModel>();
        CatService* service = model->catService(); service->startConfigured();
        quint64 id = service->openSession(1, CatTransportKind::Tester);
        int logged = 0;
        connect(service, &CatService::messageLogged, service, [&](int, bool inbound, const QByteArray&) {
            if (inbound) { ++logged; service->stopAll(); service->startConfigured(); }
        });
        service->processBytes(id, "ID;ID;"); QCOMPARE(logged, 1); QVERIFY(service->isStarted()); QVERIFY(!service->session(id));
        model = std::make_unique<RadioModel>(); service = model->catService(); service->startConfigured();
        id = service->openSession(1, CatTransportKind::Tester);
        connect(service, &CatService::messageLogged, service, [&](int, bool, const QByteArray&) { model.reset(); });
        service->processBytes(id, "ID;ID;"); QVERIFY(!model);
    }
    void nativeLogRestartAndDeletion()
    {
        auto model=std::make_unique<RadioModel>(); CatService* service=model->catService();
        CatEndpointConfig config; config.tcpEnabled=true; config.tcpPort=unusedCatPort();
        QVERIFY(config.tcpPort>0); QVERIFY(service->applyChannelConfig(1,config)); service->startConfigured();
        QTcpSocket client; client.connectToHost(QHostAddress::LocalHost,config.tcpPort); QTRY_COMPARE(service->clientCount(1),1);
        bool restarted=false; int replies=0;
        connect(service,&CatService::messageLogged,service,[&](int,bool inbound,const QByteArray&) {
            if (!inbound) { ++replies; if (!restarted) { restarted=true; service->stopAll(); service->startConfigured(); } }
        });
        client.write("ID;ID;"); QTRY_VERIFY(restarted); QCOMPARE(replies,1); QVERIFY(service->isListening(1));
        QTRY_COMPARE(client.state(),QAbstractSocket::UnconnectedState);
        client.connectToHost(QHostAddress::LocalHost,config.tcpPort); QTRY_COMPARE(service->clientCount(1),1);
        client.write("ID;"); QTRY_COMPARE(replies,2); QTRY_VERIFY(client.bytesAvailable()>0); QCOMPARE(client.readAll(),QByteArray("ID019;"));
        const QPointer<CatService> guard(service);
        connect(service,&CatService::messageLogged,service,[&](int,bool inbound,const QByteArray&) { if (!inbound) { model.reset(); } });
        client.write(QByteArray(41,'x')+";ID;"); QTRY_VERIFY(!model); QVERIFY(!guard);
    }
    void nonTcpGuidHasNoRegistration()
    {
        CatCommandCatalog catalog; CatParser parser(catalog); CatCommandRouter router;
        const QByteArray guid = "00112233-4455-6677-8899-aabbccddeeff";
        for (CatTransportKind kind : {CatTransportKind::Serial, CatTransportKind::Pty, CatTransportKind::Tester}) {
            CatSession session(1, 1, kind, {});
            const CatValidation validation = parser.validate("zzga" + guid.toUpper() + ';'); QVERIFY(validation.request);
            const CatCommandResult result = router.execute(*validation.request, session.context()); session.applyGuidResult(*validation.request, result);
            QCOMPARE(parser.format(*catalog.find("ZZGA"), *validation.request, result, session.context()), "ZZGA" + guid + ';');
            QVERIFY(!session.hasGuid(QUuid(QString::fromLatin1(guid))));
        }
    }
    void nativeTcpPttCleanup_data()
    {
        QTest::addColumn<int>("closeKind"); QTest::addColumn<bool>("newerOperator");
        QTest::newRow("disconnect") << 0 << false;
        QTest::newRow("output error") << 1 << false;
        QTest::newRow("stop") << 2 << false;
        QTest::newRow("disconnect newer operator") << 0 << true;
        QTest::newRow("output error newer operator") << 1 << true;
        QTest::newRow("stop newer operator") << 2 << true;
    }
    void nativeTcpPttCleanup()
    {
        QFETCH(int, closeKind); QFETCH(bool, newerOperator);
        RxCatMockConnection connection; RadioModel model; model.injectConnectionForTest(&connection);
        model.addSlice(); model.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        model.sliceById(0)->setFrequency(14'200'000);
        MoxController* mox = model.moxController(); mox->setTimerIntervals(0,0,0,0,0,0);
        mox->setMoxCheck([] { return safety::BandPlanGuard::MoxCheckResult{true, {}}; });
        QVERIFY(model.txSliceArbiter()->requestHandoff(0, SliceOwnership::stationDevice()));
        CatService& service = *model.catService();
        CatEndpointConfig config; config.tcpEnabled=true; config.tcpPort=unusedCatPort(); config.binding.primarySliceId=0;
        QVERIFY(config.tcpPort>0); QVERIFY(service.applyChannelConfig(1,config)); service.startConfigured();
        QTcpSocket first,second; first.connectToHost(QHostAddress::LocalHost,config.tcpPort); second.connectToHost(QHostAddress::LocalHost,config.tcpPort);
        QTRY_COMPARE(service.clientCount(1),2);
        first.write("TX;ID;"); QTRY_VERIFY(first.bytesAvailable()>0); QCOMPARE(first.readAll(),QByteArray("ID019;")); QTRY_VERIFY(mox->isMox());
        QVERIFY(service.txCoordinator().currentRequestTag()!=0);
        second.write("ZZTX1;ID;"); QTRY_VERIFY(second.bytesAvailable()>0); QCOMPARE(second.readAll(),QByteArray("ID019;"));
        first.abort(); QTRY_COMPARE(service.clientCount(1),1); QVERIFY(mox->isMox());
        if (newerOperator) { mox->setMox(true); QVERIFY(mox->isMox()); QCOMPARE(service.txCoordinator().currentRequestTag(),quint64(0)); }
        bool observedClose = false;
        connect(&service,&CatService::sessionClosed,&service,[&](quint64) {
            observedClose=true; QCOMPARE(service.txCoordinator().currentRequestTag(),quint64(0));
            QCOMPARE(mox->isMox(),newerOperator);
        });
        if (closeKind==0) { second.abort(); }
        else if (closeKind==1) { service.sendToSession(service.sessionIds(1).first(),QByteArray(CatTcpTransport::kMaximumOutputBytes+1,'x')); }
        else { service.stopAll(); }
        QTRY_COMPARE(service.clientCount(1),0); QTRY_VERIFY(observedClose); QCOMPARE(mox->isMox(),newerOperator);
        if (newerOperator) { mox->setMox(false); }
        model.injectConnectionForTest(nullptr);
    }
    void lowLevelStopRestartAndRejectedDeletion()
    {
        CatTcpTransport transport; QVERIFY(transport.start(QHostAddress::LocalHost,0));
        const quint16 oldPort=transport.boundPort(); QPointer<QTcpSocket> accepted;
        connect(&transport,&CatTcpTransport::clientAccepted,this,[&](QTcpSocket* socket) { accepted=socket; QVERIFY(transport.attachSession(1,socket)); });
        QTcpSocket client; client.connectToHost(QHostAddress::LocalHost,oldPort); QTRY_VERIFY(accepted);
        bool restarted=false;
        connect(accepted,&QTcpSocket::disconnected,this,[&] { restarted=transport.start(QHostAddress::LocalHost,0); });
        transport.stop(); QVERIFY(restarted); QVERIFY(transport.isListening()); QCOMPARE(transport.clientCount(),0);
        transport.stop();
        auto rejected=std::make_unique<CatTcpTransport>(); QVERIFY(rejected->start(QHostAddress::LocalHost,0));
        connect(rejected.get(),&CatTcpTransport::clientAccepted,this,[&](QTcpSocket* socket) {
            connect(socket,&QTcpSocket::disconnected,this,[&] { rejected.reset(); });
        });
        QTcpSocket other; other.connectToHost(QHostAddress::LocalHost,rejected->boundPort()); QTRY_VERIFY(!rejected);
    }
    void serviceLoopbackLifecycle()
    {
        RadioModel model; model.addSlice(); model.addSlice();
        model.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        model.sliceOwnership()->setOwner(1, SliceOwnership::stationDevice());
        CatService& service = *model.catService();
        CatEndpointConfig config; config.tcpEnabled = true; config.tcpPort = unusedCatPort();
        config.binding.primarySliceId = 0; config.binding.secondarySliceId = 1;
        QVERIFY(config.tcpPort > 0); QVERIFY(service.applyChannelConfig(1, config));
        service.startConfigured(); QVERIFY(service.isListening(1));
        QTcpSocket client; client.connectToHost(QHostAddress::LocalHost, config.tcpPort);
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        client.write("ID;");
        QTRY_VERIFY(client.bytesAvailable() > 0);
        QCOMPARE(client.readAll(), QByteArray("ID019;"));
        service.stopAll(); QVERIFY(!service.isListening(1));
        QTRY_COMPARE(client.state(), QAbstractSocket::UnconnectedState);
        service.startConfigured(); QVERIFY(service.isListening(1));
    }
};
QTEST_GUILESS_MAIN(TstCatTcp)
#include "tst_cat_tcp.moc"
