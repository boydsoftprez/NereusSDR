// no-port-check: NereusSDR-original Hamlib wire/authority integration tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QProcess>
#include "CatFixtureHarness.h"
#include "core/cat/CatService.h"
#include "core/cat/RigctlProtocol.h"
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <fcntl.h>
#include <unistd.h>
class RigPtyPeer {
public:
    ~RigPtyPeer() { close(); }
    bool open(const QString& path) { close(); fd = ::open(path.toLocal8Bit().constData(),O_RDWR|O_NOCTTY|O_NONBLOCK|O_CLOEXEC); return fd >= 0; }
    void close() { if (fd >= 0) { ::close(std::exchange(fd,-1)); } }
    bool send(const QByteArray& data) { return ::write(fd,data.constData(),data.size()) == data.size(); }
    QByteArray take() { QByteArray bytes; char buf[4096]; for (;;) { const ssize_t count = ::read(fd,buf,sizeof(buf)); if (count <= 0) { return bytes; } bytes.append(buf,count); } }
private: int fd{-1};
};
#endif
using namespace NereusSDR;
struct RigFixture {
    RxCatMockConnection connection;
    RadioModel model;
    CatService& service{*model.catService()};
    RigFixture() {
        model.injectConnectionForTest(&connection); model.configureStreamPool(3,3,192000);
        CatRxFixtureHarness harness; harness.slices(model);
        model.moxController()->setTimerIntervals(0,0,0,0,0,0);
        model.moxController()->setMoxCheck([] { return safety::BandPlanGuard::MoxCheckResult{true,{}}; });
    }
    ~RigFixture() { service.stopAll(); model.moxController()->setMox(false); model.injectConnectionForTest(nullptr); }
    bool configure(int channel=1, bool secondary=true, bool reverse=false) {
        QTcpServer a,b;
        if (!a.listen(QHostAddress::LocalHost,0) || !b.listen(QHostAddress::LocalHost,0)) { return false; }
        CatEndpointConfig config; config.tcpEnabled=true; config.tcpPort=a.serverPort();
        config.rigctldEnabled=true; config.rigctldPort=b.serverPort(); a.close(); b.close();
        config.binding.primarySliceId=reverse ? 2 : 0;
        if (secondary) { config.binding.secondarySliceId=reverse ? 0 : 2; }
        return service.applyChannelConfig(channel,config);
    }
};
class TstCatRigctld : public QObject {
    Q_OBJECT
    QByteArray exchange(QTcpSocket& socket,const QByteArray& bytes,const QByteArray& expected) {
        socket.write(bytes);
        if (!QTest::qWaitFor([&] { return socket.bytesAvailable()>=expected.size(); },3000)) { return "timeout: "+socket.readAll(); }
        return socket.readAll();
    }
    void connectRig(RigFixture& f,QTcpSocket& socket,int channel=1) {
        socket.connectToHost(QHostAddress::LocalHost,f.service.rigctldBoundPort(channel));
    }
private slots:
    void init() {
        for (const QString& key:AppSettings::instance().allKeys()) { if (key.startsWith("Cat/")) { AppSettings::instance().remove(key); } }
    }
    void commandsActualStateAndExtendedReset() {
        RigFixture f; QVERIFY(f.configure()); f.service.startConfigured();
        QTcpSocket socket; connectRig(f,socket); QTRY_COMPARE(f.service.rigctldClientCount(1),1);
        QCOMPARE(f.service.clientCount(1),0);
        QVERIFY(f.model.setActiveSliceById(2));
        QCOMPARE(exchange(socket,"+f\nf\n","get_freq:\nFrequency: 14074000\nRPRT 0\n14074000\n"),QByteArray("get_freq:\nFrequency: 14074000\nRPRT 0\n14074000\n"));
        QCOMPARE(exchange(socket,"|f\n","get_freq:|Frequency: 14074000|RPRT 0\n"),QByteArray("get_freq:|Frequency: 14074000|RPRT 0\n"));
        QCOMPARE(exchange(socket,"F 14201000\n\\get_freq\n","RPRT 0\n14201000\n"),QByteArray("RPRT 0\n14201000\n"));
        QCOMPARE(f.model.sliceById(0)->frequency(),14201000.0); QCOMPARE(f.model.sliceById(2)->frequency(),7074000.0);
        QCOMPARE(exchange(socket,"M LSB 2400\nm\n","RPRT 0\nLSB\n2400\n"),QByteArray("RPRT 0\nLSB\n2400\n"));
        QCOMPARE(f.model.sliceById(0)->dspMode(),DSPMode::LSB);
        QCOMPARE(exchange(socket,"V VFOB\nf\nV VFOA\n","RPRT 0\n7074000\nRPRT 0\n"),QByteArray("RPRT 0\n7074000\nRPRT 0\n"));
        QCOMPARE(exchange(socket,"I 7101000\nX USB 2200\ni\nx\n","RPRT 0\nRPRT 0\n7101000\nUSB\n2200\n"),QByteArray("RPRT 0\nRPRT 0\n7101000\nUSB\n2200\n"));
        QCOMPARE(exchange(socket,"S 1 VFOB\ns\nS 0 VFOA\ns\n","RPRT 0\n1\nVFOB\nRPRT 0\n0\nVFOA\n"),QByteArray("RPRT 0\n1\nVFOB\nRPRT 0\n0\nVFOA\n"));
        QCOMPARE(exchange(socket,"J 321\nZ -432\nj\nz\n","RPRT 0\nRPRT 0\n321\n-432\n"),QByteArray("RPRT 0\nRPRT 0\n321\n-432\n"));
        QVERIFY(!f.model.sliceById(0)->ritEnabled()); QVERIFY(!f.model.sliceById(0)->xitEnabled());
        QCOMPARE(exchange(socket,"U RIT 1\nU XIT 1\nJ 0\nZ 0\nu RIT\nu XIT\n","RPRT 0\nRPRT 0\nRPRT 0\nRPRT 0\n1\n1\n"),QByteArray("RPRT 0\nRPRT 0\nRPRT 0\nRPRT 0\n1\n1\n"));
        QCOMPARE(exchange(socket,"L AF 0.73\nl AF\nU LOCK 1\nu LOCK\n","RPRT 0\n0.73\nRPRT 0\n1\n"),QByteArray("RPRT 0\n0.73\nRPRT 0\n1\n"));
        QCOMPARE(f.model.sliceById(0)->afGain(),73); QVERIFY(f.model.sliceById(0)->locked());
        QCOMPARE(exchange(socket,"F nan\nL AF 2\nU MUTE nope\n\\get_powerstat\nP foo 1\n","RPRT -1\nRPRT -1\nRPRT -1\nRPRT -11\nRPRT -11\n"),QByteArray("RPRT -1\nRPRT -1\nRPRT -1\nRPRT -11\nRPRT -11\n"));
    }
    void officialRigctlExecutableInterop() {
        const QString binary=qEnvironmentVariable("NEREUS_HAMLIB_RIGCTL");
        if (binary.isEmpty()) { QSKIP("Official rigctl executable lane runs in the owned Linux sandbox."); }
        QProcess version; version.start(binary,{"--version"}); QTRY_COMPARE(version.state(),QProcess::NotRunning);
        QCOMPARE(version.exitCode(),0); qInfo().noquote()<<"Official client:"<<binary<<version.readAllStandardOutput();
        RigFixture f; QVERIFY(f.configure()); f.service.startConfigured(); QVERIFY(f.model.setActiveSliceById(2));
        QObject observer; connect(&f.service,&CatService::messageLogged,&observer,[](int channel,bool inbound,const QByteArray& bytes) { qInfo()<<"Client wire"<<channel<<inbound<<bytes; });
        const auto invoke=[&](const QStringList& commands) {
            QProcess process; QStringList args{"-m","2","-r",QStringLiteral("127.0.0.1:%1").arg(f.service.rigctldBoundPort(1))}; args+=commands;
            process.start(binary,args);
            if (!QTest::qWaitFor([&] { return process.state()==QProcess::NotRunning; },15000)) { process.kill(); process.waitForFinished(); qInfo()<<"Client timeout stdout"<<process.readAllStandardOutput()<<"stderr"<<process.readAllStandardError(); return QByteArray("client timeout"); }
            const QByteArray output=process.readAllStandardOutput(); const QByteArray error=process.readAllStandardError();
            qInfo().noquote()<<"rigctl arguments"<<args<<"exit"<<process.exitCode()<<"stdout"<<output<<"stderr"<<error;
            return process.exitCode()==0 && error.isEmpty() ? output : QByteArray("client failed: ")+error;
        };
        QCOMPARE(invoke({"f","VFOA"}),QByteArray("14074000\n"));
        QCOMPARE(invoke({"F","VFOA","14201000","f","VFOA","M","VFOA","LSB","2400","m","VFOA"}),QByteArray("14201000\nLSB\n2400\n"));
        QCOMPARE(f.model.sliceById(0)->frequency(),14201000.0); QCOMPARE(f.model.sliceById(0)->dspMode(),DSPMode::LSB);
        QCOMPARE(invoke({"f","VFOA","m","VFOA"}),QByteArray("14201000\nLSB\n2400\n"));
        QCOMPARE(invoke({"S","VFOA","1","VFOB","I","VFOA","7101000","i","VFOA","X","VFOA","CW","500","x","VFOA","s","VFOA"}),QByteArray("7101000\nCW\n500\n1\nVFOB\n"));
        QCOMPARE(f.model.sliceById(2)->frequency(),7101000.0); QCOMPARE(f.model.txBoundSlice(),f.model.sliceById(2));
        QCOMPARE(f.model.sliceById(2)->dspMode(),DSPMode::CWU); QCOMPARE(f.model.sliceById(2)->filterHigh()-f.model.sliceById(2)->filterLow(),500);
        QCOMPARE(invoke({"S","VFOA","0","VFOA","L","VFOA","AF","0.64","l","VFOA","AF","J","VFOA","234","j","VFOA"}),QByteArray("0.64\n234\n"));
        QCOMPARE(f.model.sliceById(0)->afGain(),64); QCOMPARE(f.model.sliceById(0)->ritHz(),234);
        QTRY_COMPARE(f.service.rigctldClientCount(1),0);
        bool sawNativeMox=false;
        connect(f.model.moxController(),&MoxController::moxChanged,&observer,[&](int,bool,bool on) { sawNativeMox=sawNativeMox || on; });
        QCOMPARE(invoke({"T","VFOA","1","t","VFOA"}),QByteArray("1\n"));
        QVERIFY(sawNativeMox);
        QTRY_COMPARE(f.service.rigctldClientCount(1),0); QTRY_VERIFY(!f.model.moxController()->isMox());
        qInfo()<<"Official client disconnect retired sessions and released native MOX";
    }
    void nativeHandshakeAndClientShapedCommands() {
        RigFixture f; QVERIFY(f.configure()); f.service.startConfigured();
        QTcpSocket socket; connectRig(f,socket); QTRY_COMPARE(f.service.rigctldClientCount(1),1);
        QCOMPARE(exchange(socket,"\\chk_vfo\n","1\n"),QByteArray("1\n"));
        QCOMPARE(exchange(socket,"\\get_lock_mode\n","0\nRPRT 0\n"),QByteArray("0\nRPRT 0\n"));
        f.model.sliceById(0)->setLocked(true);
        QCOMPARE(exchange(socket,"+\\get_lock_mode\n","get_lock_mode:\nLocked: 1\nRPRT 0\n"),QByteArray("get_lock_mode:\nLocked: 1\nRPRT 0\n"));
        f.model.sliceById(0)->setLocked(false);
        QCOMPARE(exchange(socket,"S VFOA 1 VFOA\n","RPRT -1\n"),QByteArray("RPRT -1\n"));
        socket.write("\\dump_state\n"); QTRY_VERIFY(socket.bytesAvailable()>0);
        QByteArray state;
        QVERIFY(QTest::qWaitFor([&] { state += socket.readAll(); return state.endsWith("done\n"); },3000));
        const QList<QByteArray> fields=state.split('\n');
        QCOMPARE(fields[0],QByteArray("1")); QCOMPARE(fields[1],QByteArray("2")); QCOMPARE(fields[2],QByteArray("0"));
        QCOMPARE(fields[3],QByteArray("0 0 0 0 0 0 0")); QCOMPARE(fields[4],QByteArray("0 0 0 0 0 0 0"));
        QCOMPARE(fields[5],QByteArray("0 0"));
        QVERIFY(state.contains("0x81030900\n0x81030900\n0x8\n0x8\n0\n0\n"));
        QVERIFY(state.contains("has_power2mW=0\n")); QVERIFY(!state.contains("FLEX"));
        QCOMPARE(exchange(socket,"F VFOB 7102000\nM VFOB CWR 500\nf VFOB\nm VFOB\n","RPRT 0\nRPRT 0\n7102000\nCWR\n500\n"),QByteArray("RPRT 0\nRPRT 0\n7102000\nCWR\n500\n"));
        QCOMPARE(exchange(socket,"S VFOA 1 VFOB\ns VFOA\nT VFOA 1\nt VFOA\n","RPRT 0\n1\nVFOB\nRPRT 0\n1\n"),QByteArray("RPRT 0\n1\nVFOB\nRPRT 0\n1\n"));
        QCOMPARE(exchange(socket,"T VFOA 0\n","RPRT 0\n"),QByteArray("RPRT 0\n")); QTRY_VERIFY(!f.model.moxController()->isMox());
        QCOMPARE(exchange(socket,"S VFOA 0 VFOA\n","RPRT 0\n"),QByteArray("RPRT 0\n"));
        QCOMPARE(exchange(socket,";\\get_freq VFOA\n,\\set_freq VFOA 14201000\nf\n","get_freq: VFOA;Frequency: 14074000;RPRT 0\nset_freq: VFOA 14201000,RPRT 0\n14201000\n"),QByteArray("get_freq: VFOA;Frequency: 14074000;RPRT 0\nset_freq: VFOA 14201000,RPRT 0\n14201000\n"));
        QCOMPARE(exchange(socket,"|f|F 7100000\n","get_freq: |F 7100000|RPRT -1\n"),QByteArray("get_freq: |F 7100000|RPRT -1\n"));
    }
    void separateWireFramingWelcomeGuidAndReports() {
        RigFixture f; QVERIFY(f.configure());
        CatGlobalConfig global=f.service.globalConfig(); global.sendWelcome=true; global.allowKenwoodAi=true; global.aiEnabled=true;
        QVERIFY(f.service.reconfigureGlobal(global)); f.service.startConfigured();
        QTcpSocket rig,thetis; connectRig(f,rig); thetis.connectToHost(QHostAddress::LocalHost,f.service.boundPort(1));
        QTRY_COMPARE(f.service.rigctldClientCount(1),1); QTRY_COMPARE(f.service.clientCount(1),1);
        QTRY_VERIFY(thetis.bytesAvailable()>0); QCOMPARE(thetis.readAll(),QByteArray("#NereusSDR TCP/IP Cat#;"));
        QCOMPARE(rig.bytesAvailable(),0);
        QCOMPARE(exchange(rig,"FA;\n","RPRT -1\n"),QByteArray("RPRT -1\n"));
        QCOMPARE(exchange(thetis,"f\n;","?;"),QByteArray("?;"));
        const QUuid guid("11111111-2222-3333-4444-555555555555");
        QCOMPARE(exchange(rig,"ZZGA11111111-2222-3333-4444-555555555555;\n","RPRT -1\n"),QByteArray("RPRT -1\n"));
        for (quint64 id:f.service.sessionIds(1)) { if (f.service.session(id)->dialect()==CatWireDialect::Rigctld) { QVERIFY(!f.service.session(id)->hasGuid(guid)); } }
        f.service.sendToGuid(guid,"direct;"); QCOMPARE(rig.bytesAvailable(),0);
        f.model.sliceById(0)->setFrequency(14222000); f.service.reporter().flushPending();
        QTRY_VERIFY(thetis.bytesAvailable()>0); QVERIFY(thetis.readAll().contains("FA00014222000;")); QCOMPARE(rig.bytesAvailable(),0);
        rig.write("F 14223"); QCoreApplication::processEvents(); QCOMPARE(f.model.sliceById(0)->frequency(),14222000.0);
        QCOMPARE(exchange(rig,"000\nf\n","RPRT 0\n14223000\n"),QByteArray("RPRT 0\n14223000\n"));
        QCOMPARE(exchange(rig,QByteArray(5000,'q')+"\nf\n","RPRT -1\n14223000\n"),QByteArray("RPRT -1\n14223000\n"));
    }
    void sharedClaimsConflictsAndNewerOwner_data() {
        QTest::addColumn<bool>("newer"); QTest::newRow("last-CAT-releases")<<false; QTest::newRow("new-operator-survives")<<true;
    }
    void sharedClaimsConflictsAndNewerOwner() {
        QFETCH(bool,newer); RigFixture f; QVERIFY(f.configure()); QVERIFY(f.configure(2,false,true)); f.service.startConfigured();
        QTcpSocket a,b,conflict; connectRig(f,a); connectRig(f,conflict,2); b.connectToHost(QHostAddress::LocalHost,f.service.boundPort(1));
        QTRY_COMPARE(f.service.rigctldClientCount(1),1); QTRY_COMPARE(f.service.rigctldClientCount(2),1); QTRY_COMPARE(f.service.clientCount(1),1);
        QCOMPARE(exchange(a,"T 1\n","RPRT 0\n"),QByteArray("RPRT 0\n")); QTRY_VERIFY(f.model.moxController()->isMox());
        QCOMPARE(exchange(b,"TX;ID;","ID019;"),QByteArray("ID019;"));
        QCOMPARE(exchange(conflict,"T 1\nS 0 VFOA\n","RPRT -9\nRPRT -9\n"),QByteArray("RPRT -9\nRPRT -9\n"));
        QCOMPARE(exchange(a,"F 7100000\n","RPRT -9\n"),QByteArray("RPRT -9\n")); QCOMPARE(f.model.sliceById(0)->frequency(),14074000.0);
        a.abort(); QTRY_COMPARE(f.service.rigctldClientCount(1),0); QVERIFY(f.model.moxController()->isMox());
        if (newer) { f.model.moxController()->setMox(true); }
        b.abort(); QTRY_COMPARE(f.service.clientCount(1),0); QTRY_COMPARE(f.model.moxController()->isMox(),newer);
    }
    void setterCallbackRetirementAndReconfigure_data() {
        QTest::addColumn<bool>("retire"); QTest::newRow("retirement")<<true; QTest::newRow("replacement")<<false;
    }
    void setterCallbackRetirementAndReconfigure() {
        QFETCH(bool,retire); RigFixture f; QVERIFY(f.configure()); f.service.startConfigured();
        QTcpSocket socket; connectRig(f,socket); QTRY_COMPARE(f.service.rigctldClientCount(1),1);
        const quint64 old=f.service.sessionIds(1).first(); QObject observer; int incoming=0;
        connect(&f.service,&CatService::messageLogged,&observer,[&](int,bool inbound,const QByteArray&) { if(inbound) { ++incoming; } });
        connect(f.model.sliceById(2),&SliceModel::frequencyChanged,&observer,[&] {
            if (retire) { f.service.beginRetirement(); }
            else { CatEndpointConfig config=f.service.channelConfig(1); config.ptyDialect="Rigctld"; QVERIFY(f.service.reconfigureChannel(1,config)); }
        });
        socket.write("K 7101000 LSB 2300\nT 1\n"); QTRY_COMPARE(socket.state(),QAbstractSocket::UnconnectedState);
        QCOMPARE(incoming,1); QVERIFY(!f.service.session(old)); QCOMPARE(f.model.sliceById(2)->dspMode(),DSPMode::USB);
        QVERIFY(!f.model.moxController()->isMox()); QCOMPARE(socket.bytesAvailable(),0);
    }
    void setterCallbackServiceAndModelDeletion() {
        auto model=std::make_unique<RadioModel>(); CatRxFixtureHarness harness; harness.slices(*model);
        QTcpServer reserve; QVERIFY(reserve.listen(QHostAddress::LocalHost,0)); CatEndpointConfig config;
        config.rigctldEnabled=true; config.rigctldPort=reserve.serverPort(); reserve.close(); config.binding.primarySliceId=0; config.binding.secondarySliceId=2;
        QVERIFY(model->catService()->applyChannelConfig(1,config)); model->catService()->startConfigured();
        QTcpSocket socket; socket.connectToHost(QHostAddress::LocalHost,model->catService()->rigctldBoundPort(1)); QTRY_COMPARE(model->catService()->rigctldClientCount(1),1);
        QObject observer; connect(model->sliceById(2),&SliceModel::frequencyChanged,&observer,[&] { model.reset(); });
        socket.write("K 7101000 LSB 2300\nT 1\n"); QTRY_VERIFY(!model); QTRY_COMPARE(socket.state(),QAbstractSocket::UnconnectedState); QCOMPARE(socket.bytesAvailable(),0);
    }
    void authorityAndFrozenIncarnations() {
        RigFixture f; QVERIFY(f.configure()); f.service.startConfigured(); QTcpSocket socket; connectRig(f,socket); QTRY_COMPARE(f.service.rigctldClientCount(1),1);
        f.model.sliceOwnership()->setOwner(2,"remote-owner");
        QCOMPARE(exchange(socket,"F VFOB 7101000\n","RPRT -22\n"),QByteArray("RPRT -22\n")); QCOMPARE(f.model.sliceById(2)->frequency(),7074000.0);
        f.model.removeSlice(2); f.model.addSlice(); QCOMPARE(f.model.slices().size(),2);
        QCOMPARE(exchange(socket,"F VFOB 7101000\nV VFOB\n","RPRT -12\nRPRT -12\n"),QByteArray("RPRT -12\nRPRT -12\n"));
        f.model.removeSlice(0);
        QCOMPARE(exchange(socket,"\\get_lock_mode\n","RPRT -12\n"),QByteArray("RPRT -12\n"));
    }
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    void realPtyDialectIsolationHupAndFreshReopen() {
        RigFixture f; QVERIFY(f.configure()); CatEndpointConfig config=f.service.channelConfig(1); config.ptyEnabled=true; config.ptyDialect="Rigctld";
        QVERIFY(f.service.applyChannelConfig(1,config)); CatGlobalConfig global=f.service.globalConfig(); global.allowKenwoodAi=true; global.aiEnabled=true; QVERIFY(f.service.applyGlobalConfig(global));
        f.service.startConfigured(); const QString path=f.service.ptySlavePath(1); QVERIFY(!path.isEmpty());
        RigPtyPeer peer; QVERIFY(peer.open(path)); QTRY_COMPARE(f.service.sessionIds(1).size(),1); const quint64 first=f.service.sessionIds(1).first();
        QCOMPARE(f.service.session(first)->transport(),CatTransportKind::Pty); QCOMPARE(f.service.session(first)->dialect(),CatWireDialect::Rigctld);
        QByteArray bytes; QVERIFY(peer.send("V VFOB\n+f\n"));
        QTRY_VERIFY(([&] { bytes+=peer.take(); return bytes.endsWith("RPRT 0\n"); })()); QCOMPARE(bytes,QByteArray("RPRT 0\nget_freq:\nFrequency: 7074000\nRPRT 0\n"));
        f.model.sliceById(0)->setFrequency(14222000); f.service.reporter().flushPending(); QCOMPARE(peer.take(),QByteArray());
        bytes.clear(); QVERIFY(peer.send("T 1\n")); QTRY_VERIFY(([&] { bytes+=peer.take(); return bytes.endsWith('\n'); })()); QCOMPARE(bytes,QByteArray("RPRT 0\n")); QTRY_VERIFY(f.model.moxController()->isMox());
        QVERIFY(peer.send("F 710")); peer.close(); QTRY_VERIFY(!f.service.session(first)); QTRY_VERIFY(!f.model.moxController()->isMox());
        QCOMPARE(f.service.ptySlavePath(1),path); QVERIFY(peer.open(path)); QTRY_COMPARE(f.service.sessionIds(1).size(),1);
        QVERIFY(f.service.sessionIds(1).first()>first); bytes.clear(); QVERIFY(peer.send("f\nv\n"));
        QTRY_VERIFY(([&] { bytes+=peer.take(); return bytes.endsWith("VFOA\n"); })()); QCOMPARE(bytes,QByteArray("14222000\nVFOA\n"));
        peer.close(); QTRY_VERIFY(f.service.sessionIds(1).isEmpty()); f.service.stopAll(); QVERIFY(f.service.ptySlavePath(1).isEmpty());
    }
#endif
    void missingSecondaryDoesNotCreateOrSelect() {
        RigFixture f; QVERIFY(f.configure(1,false)); f.service.startConfigured();
        QTcpSocket socket; connectRig(f,socket); QTRY_COMPARE(f.service.rigctldClientCount(1),1);
        QCOMPARE(exchange(socket,"V VFOB\nF VFOB 7100000\nS 1 VFOB\nI 7100000\n","RPRT -12\nRPRT -12\nRPRT -12\nRPRT -12\n"),QByteArray("RPRT -12\nRPRT -12\nRPRT -12\nRPRT -12\n"));
        QCOMPARE(f.model.slices().size(),2); QCOMPARE(f.model.txBoundSlice(),f.model.sliceById(0));
    }
};
QTEST_MAIN(TstCatRigctld)
#include "tst_cat_rigctld.moc"
