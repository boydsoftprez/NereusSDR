// no-port-check: NereusSDR-original production CAT transport/model integration.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include "CatFixtureHarness.h"
#include "core/cat/CatService.h"
#include "models/RadioModel.h"

// This is an identified no-hardware connection. It records production wire
// intent only; it does not discover/connect a radio or claim RF/audio evidence.
class CatIntegrationConnection : public RxCatMockConnection {
public:
    QList<QPair<int, quint64>> receiveFrequency;
    void setReceiverFrequency(int receiver, quint64 hz) override
    {
        receiveFrequency.append({receiver, hz});
    }
};

struct CatIntegrationFixture {
    CatIntegrationConnection connection;
    RadioModel model;
    CatService& service{*model.catService()};
    qint64 clock{0};
    CatIntegrationFixture()
    {
        model.injectConnectionForTest(&connection);
        model.configureStreamPool(3, 3, 192000);
        for (int i = 0; i < 3; ++i) { model.receiverManager()->createReceiver(); }
        model.wireReceiverManagerHardwarePushesForTest();
        model.addSlice(); model.addSlice(); model.addSlice(); model.removeSlice(1);
        for (SliceModel* slice : model.slices()) {
            model.sliceOwnership()->setOwner(slice->sliceIndex(), SliceOwnership::stationDevice());
            slice->setDspMode(DSPMode::USB); slice->setFilter(100, 3000);
        }
        model.sliceById(0)->setFrequency(14074000);
        model.sliceById(2)->setFrequency(7074000);
        model.txSliceArbiter()->requestHandoff(0, SliceOwnership::stationDevice());
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        // Existing software-model admission seam. Real RF safety/bench behavior
        // is not inferred from this deterministic allowed band-plan response.
        model.moxController()->setMoxCheck([] {
            return safety::BandPlanGuard::MoxCheckResult{true, {}};
        });
        service.reporter().setClockForTest([this] { return clock; });
        connection.receiveFrequency.clear(); connection.txFreqLog.clear();
    }
    ~CatIntegrationFixture()
    {
        service.stopAll(); model.moxController()->setMox(false);
        model.injectConnectionForTest(nullptr);
    }
    bool configure(int channel = 1, bool reverse = false, bool secondary = true)
    {
        QTcpServer reservation;
        if (!reservation.listen(QHostAddress::LocalHost, 0)) { return false; }
        CatEndpointConfig config;
        config.tcpEnabled = true; config.tcpPort = reservation.serverPort(); reservation.close();
        config.binding.primarySliceId = reverse ? 2 : 0;
        if (secondary) { config.binding.secondarySliceId = reverse ? 0 : 2; }
        return service.applyChannelConfig(channel, config);
    }
    void connectClient(QTcpSocket& socket, int channel = 1)
    {
        socket.connectToHost(QHostAddress::LocalHost, service.boundPort(channel));
    }
};

class TstCatIntegration : public QObject {
    Q_OBJECT
    QByteArray exchange(QTcpSocket& socket, const QByteArray& request, qsizetype replySize)
    {
        if (socket.bytesAvailable() || socket.write(request) != request.size()) { return "unexpected socket state"; }
        if (!QTest::qWaitFor([&] { return socket.bytesAvailable() >= replySize; }, 3000)) {
            return "timeout: " + socket.readAll();
        }
        return socket.readAll();
    }
private slots:
    void init()
    {
        for (const QString& key : AppSettings::instance().allKeys()) {
            if (key.startsWith("Cat/")) { AppSettings::instance().remove(key); }
        }
    }
    void tcpParserModelsAndConnectionIntent()
    {
        CatIntegrationFixture f; QVERIFY(f.configure()); f.service.startConfigured();
        QTcpSocket client; f.connectClient(client); QTRY_COMPARE(f.service.clientCount(1), 1);
        QVERIFY(f.model.setActiveSliceById(2));
        QSignalSpy frequency(f.model.sliceById(0), &SliceModel::frequencyChanged);
        QSignalSpy mode(f.model.sliceById(0), &SliceModel::dspModeChanged);
        QSignalSpy filter(f.model.sliceById(0), &SliceModel::filterChanged);
        QSignalSpy txState(&f.model, &RadioModel::txModeAndBandpassPushed);
        QCOMPARE(exchange(client, "FA00014201000;FB00007101000;FA;FB;", 28),
                 QByteArray("FA00014201000;FB00007101000;"));
        QCOMPARE(frequency.size(), 1);
        QCOMPARE(f.model.sliceById(0)->frequency(), 14201000.0);
        QCOMPARE(f.model.sliceById(2)->frequency(), 7101000.0);
        const int primaryReceiver = f.model.receiverManager()->receiverConfig(f.model.sliceById(0)->streamIndex()).hardwareRx;
        const int secondaryReceiver = f.model.receiverManager()->receiverConfig(f.model.sliceById(2)->streamIndex()).hardwareRx;
        QVERIFY(primaryReceiver >= 0); QVERIFY(secondaryReceiver >= 0); QVERIFY(primaryReceiver != secondaryReceiver);
        QVERIFY(f.connection.receiveFrequency.contains({primaryReceiver, 14201000}));
        QVERIFY(f.connection.receiveFrequency.contains({secondaryReceiver, 7101000}));
        QVERIFY(!f.connection.receiveFrequency.contains({primaryReceiver, 7101000}));
        QVERIFY(!f.connection.receiveFrequency.contains({secondaryReceiver, 14201000}));
        QVERIFY(f.connection.txFreqLog.contains(14201000));
        QCOMPARE(exchange(client, "MD1;MD;MD2;ZZFL+0200;ZZFH+3200;ZZFL;ZZFH;", 24),
                 QByteArray("MD1;ZZFL+0200;ZZFH+3200;"));
        QCOMPARE(mode.size(), 2); QVERIFY(filter.size() >= 2);
        QCOMPARE(f.model.sliceById(0)->dspMode(), DSPMode::USB);
        QCOMPARE(f.model.sliceById(0)->filterLow(), 200);
        QCOMPARE(f.model.sliceById(0)->filterHigh(), 3200);
        QCOMPARE(f.model.sliceById(2)->dspMode(), DSPMode::USB);
        QCOMPARE(f.model.sliceById(2)->filterLow(), 100);
        QCOMPARE(exchange(client, "FT1;FT;", 4), QByteArray("FT1;"));
        QCOMPARE(f.model.txBoundSlice(), f.model.sliceById(2));
        QVERIFY(f.connection.txFreqLog.contains(7101000)); QVERIFY(!txState.isEmpty());
        const qsizetype pushes = f.connection.txFreqLog.size();
        QCOMPARE(exchange(client, "FA00014202000;FA;", 14), QByteArray("FA00014202000;"));
        QCOMPARE(f.connection.txFreqLog.size(), pushes);
        QCOMPARE(exchange(client, "FB00007102000;FB;", 14), QByteArray("FB00007102000;"));
        QCOMPARE(f.connection.txFreqLog.last(), quint64(7102000));
        QCOMPARE(f.model.slices().size(), 2);
    }
    void authorityFreezeAndStableIncarnations()
    {
        CatIntegrationFixture f; QVERIFY(f.configure()); f.service.startConfigured();
        QTcpSocket client; f.connectClient(client); QTRY_COMPARE(f.service.clientCount(1), 1);
        f.model.sliceOwnership()->setOwner(0, "foreign-device");
        QCOMPARE(exchange(client, "FA00014201000;ZZMD07;ZZFH+3200;ID;", 12), QByteArray("?;?;?;ID019;"));
        QCOMPARE(f.model.sliceById(0)->frequency(), 14074000.0);
        QCOMPARE(f.model.sliceById(0)->dspMode(), DSPMode::USB);
        f.model.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
        f.model.setRxOnly(true);
        QCOMPARE(exchange(client, "TX;ID;", 8), QByteArray("?;ID019;")); QVERIFY(!f.model.moxController()->isMox());
        f.model.setRxOnly(false);
        QCOMPARE(exchange(client, "TX;ID;", 6), QByteArray("ID019;")); QTRY_VERIFY(f.model.moxController()->isMox());
        const QList<quint64> before = f.connection.txFreqLog;
        QCOMPARE(exchange(client, "FA00014201000;MD1;ZZFH+3200;FT1;ID;", 14), QByteArray("?;?;?;?;ID019;"));
        QCOMPARE(f.connection.txFreqLog, before);
        QCOMPARE(exchange(client, "FB00007101000;FB;RX;ID;", 20), QByteArray("FB00007101000;ID019;"));
        QTRY_VERIFY(!f.model.moxController()->isMox());
        f.model.removeSlice(2); QCOMPARE(f.model.addSlice(), 1); QCOMPARE(f.model.addSlice(), 2);
        f.model.sliceOwnership()->setOwner(2, SliceOwnership::stationDevice()); f.model.sliceById(2)->setFrequency(7200000);
        QCOMPARE(exchange(client, "FB;FB00007300000;FT1;ID;", 12), QByteArray("?;?;?;ID019;"));
        QCOMPARE(f.model.sliceById(2)->frequency(), 7200000.0); QCOMPARE(f.model.slices().size(), 3);
        CatEndpointConfig config = f.service.channelConfig(1);
        config.binding.secondaryIncarnation = f.model.sliceOwnership()->refOf(2).incarnation;
        QVERIFY(f.service.reconfigureChannel(1, config));
        QTRY_COMPARE(client.state(), QAbstractSocket::UnconnectedState);
        f.connectClient(client); QTRY_COMPARE(f.service.clientCount(1), 1);
        QCOMPARE(exchange(client, "FB;", 14), QByteArray("FB00007200000;"));
    }
    void aiBindingsRoutesAndMissingSecondary()
    {
        CatIntegrationFixture f; QVERIFY(f.configure()); QVERIFY(f.configure(2, true)); QVERIFY(f.configure(3, false, false));
        CatGlobalConfig global = f.service.globalConfig(); global.allowKenwoodAi = true; global.aiEnabled = true;
        QVERIFY(f.service.applyGlobalConfig(global)); f.service.startConfigured();
        QTcpSocket first, second, third; f.connectClient(first); f.connectClient(second, 2); f.connectClient(third, 3);
        QTRY_COMPARE(f.service.clientCount(1), 1); QTRY_COMPARE(f.service.clientCount(2), 1); QTRY_COMPARE(f.service.clientCount(3), 1);
        QCOMPARE(exchange(third, "FB;ID;", 8), QByteArray("?;ID019;")); QCOMPARE(f.model.slices().size(), 2);
        const quint64 tester = f.service.openSession(1, CatTransportKind::Tester); QVERIFY(tester);
        QVERIFY(!f.service.session(tester)->context().transmitAllowed);
        QCOMPARE(first.write("FA00014201000;ID;"), qint64(QByteArray("FA00014201000;ID;").size()));
        QTRY_COMPARE(f.model.sliceById(0)->frequency(), 14201000.0); f.service.reporter().flushPending();
        QTRY_VERIFY(first.bytesAvailable() >= 20); QTRY_VERIFY(second.bytesAvailable() >= 14); QTRY_VERIFY(third.bytesAvailable() >= 14);
        const QByteArray response = first.readAll(); QVERIFY(response.contains("ID019;")); QCOMPARE(response.count("FA00014201000;"), 1);
        QCOMPARE(second.readAll(), QByteArray("FB00014201000;")); QCOMPARE(third.readAll(), QByteArray("FA00014201000;"));
        global.aiTcp = false; QVERIFY(f.service.reconfigureGlobal(global));
        QSignalSpy outbound(&f.service, &CatService::messageLogged);
        f.clock = 200; f.model.sliceById(0)->setFrequency(14202000); f.service.reporter().flushPending();
        QCOMPARE(outbound.size(), 0); QCOMPARE(first.bytesAvailable(), qint64(0));
        QCOMPARE(exchange(first, "FA;", 14), QByteArray("FA00014202000;"));
    }
    void disconnectCleanup_data()
    {
        QTest::addColumn<bool>("newerOperator"); QTest::addColumn<bool>("reconfigure");
        QTest::newRow("disconnect-last-CAT") << false << false;
        QTest::newRow("disconnect-newer-operator") << true << false;
        QTest::newRow("reconfigure-last-CAT") << false << true;
        QTest::newRow("reconfigure-newer-operator") << true << true;
    }
    void disconnectCleanup()
    {
        QFETCH(bool, newerOperator); QFETCH(bool, reconfigure);
        CatIntegrationFixture f; QVERIFY(f.configure()); QVERIFY(f.configure(2)); f.service.startConfigured();
        QTcpSocket first, second, other; f.connectClient(first); f.connectClient(second); f.connectClient(other, 2);
        QTRY_COMPARE(f.service.clientCount(1), 2); QTRY_COMPARE(f.service.clientCount(2), 1);
        QCOMPARE(exchange(first, "TX;ID;", 6), QByteArray("ID019;"));
        QCOMPARE(exchange(second, "TX;ID;", 6), QByteArray("ID019;")); QTRY_VERIFY(f.model.moxController()->isMox());
        first.abort(); QTRY_COMPARE(f.service.clientCount(1), 1); QVERIFY(f.model.moxController()->isMox());
        if (newerOperator) { f.model.moxController()->setMox(true); QCOMPARE(f.service.txCoordinator().currentRequestTag(), quint64(0)); }
        bool notice = false;
        QObject observer; // Disconnect before notice and the fixture leave scope.
        connect(&f.service, &CatService::sessionClosed, &observer, [&](quint64) {
            notice = true; QCOMPARE(f.service.txCoordinator().currentRequestTag(), quint64(0));
            QCOMPARE(f.model.moxController()->isMox(), newerOperator);
        });
        if (reconfigure) {
            CatEndpointConfig config = f.service.channelConfig(1); config.tcpEnabled = false;
            QVERIFY(f.service.reconfigureChannel(1, config));
        } else { second.abort(); }
        QTRY_VERIFY(notice); QTRY_COMPARE(f.service.clientCount(1), 0);
        QCOMPARE(f.model.moxController()->isMox(), newerOperator); QCOMPARE(f.service.clientCount(2), 1);
        QCOMPARE(exchange(other, "ID;", 6), QByteArray("ID019;"));
    }
    void parserCallbackRetirementDropsContinuation()
    {
        auto fixture = std::make_unique<CatIntegrationFixture>(); QVERIFY(fixture->configure()); fixture->service.startConfigured();
        QTcpSocket client; fixture->connectClient(client); QTRY_COMPARE(fixture->service.clientCount(1), 1);
        int incoming = 0;
        QObject observer;
        connect(&fixture->service, &CatService::messageLogged, &observer, [&](int, bool inbound, const QByteArray&) {
            if (inbound) { ++incoming; fixture->service.beginRetirement(); fixture->service.startConfigured(); }
        });
        QCOMPARE(client.write("FA00014201000;TX;ID;"), qint64(QByteArray("FA00014201000;TX;ID;").size()));
        QTRY_COMPARE(client.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(incoming, 1); QVERIFY(!fixture->service.isStarted());
        QCOMPARE(fixture->model.sliceById(0)->frequency(), 14074000.0); QVERIFY(!fixture->model.moxController()->isMox());
    }
    void remoteRoleAndStartupCannotConnectOrKey()
    {
        RadioModel remote(RadioModel::Role::Remote); CatService& service = *remote.catService();
        QSignalSpy connection(&remote, &RadioModel::connectionStateChanged);
        QSignalSpy mox(remote.moxController(), &MoxController::moxChanged);
        CatEndpointConfig config; config.tcpEnabled = true; config.tcpPort = 12345;
        QVERIFY(!service.reconfigureChannel(1, config)); service.startConfigured();
        QVERIFY(!service.isStarted()); QVERIFY(!service.isListening(1)); QVERIFY(!service.openSession(1, CatTransportKind::Tester));
        QCOMPARE(connection.size(), 0); QCOMPARE(mox.size(), 0);
        RadioModel local; QSignalSpy localConnection(&local, &RadioModel::connectionStateChanged);
        QSignalSpy localMox(local.moxController(), &MoxController::moxChanged);
        local.catService()->startConfigured(); QCOMPARE(localConnection.size(), 0); QCOMPARE(localMox.size(), 0);
    }
};
QTEST_GUILESS_MAIN(TstCatIntegration)
#include "tst_cat_integration.moc"
