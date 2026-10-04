// no-port-check: NereusSDR-original functional CAT setup, diagnostics and sandbox visual tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <memory>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QScrollArea>
#include <QScrollBar>
#include "gui/setup/CatNetworkSetupPages.h"
#include "gui/setup/CatLogWindow.h"
#include "gui/applets/CatApplet.h"
#include "gui/SetupDialog.h"
#include "core/AppSettings.h"
#include "core/cat/CatService.h"
#include "core/SliceOwnership.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
using namespace NereusSDR;
namespace {
int unusedPort() { QTcpServer reservation; if (!reservation.listen(QHostAddress::LocalHost,0)) { return 0; } return reservation.serverPort(); }
template<class T> T* control(QWidget& page,const char* name) { return page.findChild<T*>(QString::fromLatin1(name)); }
}
class TstCatSetup : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::setProfileOverride(QStringLiteral("cat-setup-%1").arg(QCoreApplication::applicationPid())); qInfo()<<"CAT test settings sandbox:"<<AppSettings::instance().filePath(); }
    void init() { AppSettings::instance().setChangeHook({}); AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().setChangeHook({}); }
    void rigctldControlsAndPtyDialectAreLive() {
        RadioModel model; model.addSlice(); CatService& service=*model.catService(); service.startConfigured(); CatTcpIpPage page(&model);
        auto* enabled=control<QCheckBox>(page,"cat1RigctldEnabled"); auto* port=control<QSpinBox>(page,"cat1RigctldPort");
        auto* address=control<QLineEdit>(page,"cat1RigctldAddress"); auto* dialect=control<QComboBox>(page,"cat1PtyDialect");
        QVERIFY(enabled && port && address && dialect); QVERIFY(enabled->isEnabled()); QVERIFY(!enabled->isChecked()); QCOMPARE(port->value(),0);
        enabled->setChecked(true); QVERIFY(!enabled->isChecked()); QVERIFY(!service.channelConfig(1).rigctldEnabled);
        const int chosen=unusedPort(); QVERIFY(chosen>0); port->setValue(chosen); enabled->setChecked(true);
        QVERIFY(service.isListening(1)); QCOMPARE(service.rigctldBoundPort(1),chosen); QCOMPARE(service.boundPort(1),0);
        QVERIFY(control<QLabel>(page,"cat1RigctldStatus")->text().contains(QString::number(chosen)));
        QTcpSocket socket; socket.connectToHost(QHostAddress::LocalHost,chosen); QTRY_COMPARE(service.rigctldClientCount(1),1);
        QVERIFY(control<QLabel>(page,"cat1RigctldStatus")->text().contains("Clients: 1"));
        dialect->setCurrentText("Rigctld"); QCOMPARE(service.channelConfig(1).ptyDialect,QString("Rigctld"));
        QVERIFY(control<QCheckBox>(page,"cat1Pty")->toolTip().contains("Rigctld"));
        QSignalSpy changed(&service,&CatService::configurationChanged);
        CatEndpointConfig external=service.channelConfig(1); external.rigctldEnabled=false; QVERIFY(service.reconfigureChannel(1,external));
        QCOMPARE(changed.size(),1); QVERIFY(!enabled->isChecked()); QCOMPARE(service.rigctldClientCount(1),0);
        QTcpServer occupied; QVERIFY(occupied.listen(QHostAddress::LocalHost,0)); port->setValue(occupied.serverPort()); enabled->setChecked(true);
        QVERIFY(control<QLabel>(page,"cat1RigctldStatus")->text().contains("error"));
        QVERIFY(!service.isListening(1)); QVERIFY(service.channelConfig(1).rigctldEnabled);
        address->setText("invalid"); QVERIFY(QMetaObject::invokeMethod(address,"editingFinished",Qt::DirectConnection));
        QCOMPARE(service.channelConfig(1).rigctldBindAddress,QString("127.0.0.1"));
        for (int channel=2;channel<=4;++channel) {
            const QString name=QStringLiteral("cat%1RigctldEnabled").arg(channel); auto* control=page.findChild<QCheckBox*>(name); QVERIFY(control && control->isEnabled()); QVERIFY(!control->isChecked());
        }
    }
    void synchronousRigctldCallbackMayDeletePage() {
        RadioModel model; CatService& service=*model.catService(); auto page=std::make_unique<CatTcpIpPage>(&model);
        auto* port=control<QSpinBox>(*page,"cat1RigctldPort"); const int chosen=unusedPort(); { const QSignalBlocker block(port); port->setValue(chosen); }
        QObject observer; QPointer<CatTcpIpPage> alive(page.get());
        connect(&service,&CatService::configurationChanged,&observer,[&] { page.reset(); });
        QVERIFY(QMetaObject::invokeMethod(port,"valueChanged",Qt::DirectConnection,Q_ARG(int,chosen))); QVERIFY(!alive); QCOMPARE(service.channelConfig(1).rigctldPort,chosen);
    }
    void tcpControlsAreLiveAndExternalSyncDoesNotEcho() {
        RadioModel model; CatService& service=*model.catService(); service.startConfigured();
        CatTcpIpPage page(&model); auto* enabled=control<QCheckBox>(page,"cat1Enabled"); auto* port=control<QSpinBox>(page,"cat1Port");
        QVERIFY(enabled && port); QVERIFY(enabled->isEnabled()); QCOMPARE(port->value(),13013);
        QSignalSpy changed(&service,&CatService::configurationChanged); const int chosen=unusedPort(); QVERIFY(chosen>0);
        port->setValue(chosen); QCOMPARE(changed.size(),1); enabled->setChecked(true); QCOMPARE(changed.size(),2);
        QVERIFY(service.isListening(1)); QCOMPARE(service.boundPort(1),chosen);
        QVERIFY(control<QLabel>(page,"cat1Status")->text().contains(QString::number(chosen)));
        CatEndpointConfig external=service.channelConfig(1); external.tcpEnabled=false; QVERIFY(service.reconfigureChannel(1,external));
        QCOMPARE(changed.size(),3); QVERIFY(!enabled->isChecked()); QVERIFY(!service.isListening(1));
        int writes=0; AppSettings::instance().setChangeHook([&](const QString& key) { if (key.startsWith("Cat/")) { ++writes; } });
        QVERIFY(service.reconfigureChannel(1,service.channelConfig(1))); page.syncFromModel(); QCOMPARE(writes,0); QCOMPARE(changed.size(),3);
        AppSettings::instance().setChangeHook({});
    }
    void failedActivationRemainsSavedAndVisible() {
        RadioModel model; CatService& service=*model.catService(); service.startConfigured(); CatTcpIpPage page(&model);
        QTcpServer occupied; QVERIFY(occupied.listen(QHostAddress::LocalHost,0));
        control<QSpinBox>(page,"cat1Port")->setValue(occupied.serverPort()); control<QCheckBox>(page,"cat1Enabled")->setChecked(true);
        QVERIFY(service.channelConfig(1).tcpEnabled); QVERIFY(!service.isListening(1)); QVERIFY(service.transportState(1,CatTransportKind::Tcp).contains("error"));
        QVERIFY(control<QCheckBox>(page,"cat1Enabled")->isChecked()); QVERIFY(control<QLabel>(page,"cat1Status")->text().contains("error"));
        QCOMPARE(AppSettings::instance().value("Cat/Channels/1/TcpEnabled").toString(),QString("True"));
        CatEndpointConfig invalid=service.channelConfig(1); invalid.tcpPort=0; QSignalSpy changed(&service,&CatService::configurationChanged);
        QVERIFY(!service.reconfigureChannel(1,invalid)); QCOMPARE(changed.size(),0); QCOMPARE(service.channelConfig(1).tcpPort,int(occupied.serverPort()));
    }
    void unrelatedLiveClientAndAcceptCallbackSurvive() {
        RadioModel model; CatService& service=*model.catService(); CatEndpointConfig one; one.tcpEnabled=true; one.tcpPort=unusedPort();
        CatEndpointConfig two=one; two.tcpPort=unusedPort(); QVERIFY(one.tcpPort && two.tcpPort && one.tcpPort!=two.tcpPort);
        QVERIFY(service.applyChannelConfig(1,one)); QVERIFY(service.applyChannelConfig(2,two)); service.startConfigured();
        QTcpSocket existing; existing.connectToHost(QHostAddress::LocalHost,two.tcpPort); QTRY_COMPARE(service.clientCount(2),1);
        const quint64 session=service.sessionIds(2).first(); one=service.channelConfig(1); one.tcpPort=unusedPort(); QVERIFY(service.reconfigureChannel(1,one));
        QCOMPARE(service.clientCount(2),1); QVERIFY(service.session(session));
        existing.write("ID;"); QTRY_VERIFY(existing.bytesAvailable()>0); QCOMPARE(existing.readAll(),QByteArray("ID019;"));
        QTcpSocket later; later.connectToHost(QHostAddress::LocalHost,two.tcpPort); QTRY_COMPARE(service.clientCount(2),2);
        later.write("ID;"); QTRY_VERIFY(later.bytesAvailable()>0); QCOMPARE(later.readAll(),QByteArray("ID019;"));
        QVERIFY(service.isListening(1)); QCOMPARE(service.boundPort(2),two.tcpPort);
    }
    void stableSelectorsExposeInvalidBindingAndExplicitRebind() {
        RadioModel model; model.addSlice(); model.addSlice(); CatService& service=*model.catService(); service.startConfigured();
        CatTcpIpPage page(&model); auto* primary=control<QComboBox>(page,"cat1Primary"); const quint64 original=service.channelConfig(1).binding.primaryIncarnation;
        QCOMPARE(primary->currentData().toInt(),0); model.removeSlice(0); QCOMPARE(model.addSlice(),0);
        QVERIFY(primary->currentText().contains("Invalid binding"));
        control<QSpinBox>(page,"cat1Port")->setValue(unusedPort()); QCOMPARE(service.channelConfig(1).binding.primaryIncarnation,original);
        const int live=primary->findData(0); QVERIFY(live>=0); QVERIFY(live!=primary->currentIndex()); primary->setCurrentIndex(live);
        QCOMPARE(service.channelConfig(1).binding.primaryIncarnation,model.sliceOwnership()->incarnation(0)); QVERIFY(!primary->currentText().contains("Invalid"));
    }
    void serialChoicesAndRemoteScopeAreTruthful() {
        RadioModel model; model.catService()->startConfigured(); CatSerialPortsPage serial(&model);
        auto* baud=control<QComboBox>(serial,"cat1Baud"); QVERIFY(baud); QCOMPARE(baud->count(),9); QCOMPARE(baud->itemText(0),QString("300"));
        QCOMPARE(control<QComboBox>(serial,"cat1Parity")->count(),5); QCOMPARE(control<QComboBox>(serial,"cat1Bits")->count(),3); QCOMPARE(control<QComboBox>(serial,"cat1Stops")->count(),3);
#ifndef HAVE_SERIALPORT
        QVERIFY(!control<QCheckBox>(serial,"cat1Enabled")->isEnabled());
#endif
        RadioModel remote(RadioModel::Role::Remote); CatTcpIpPage network(&remote); CatOptionsSetupPage options(&remote); CatPttSetupPage ptt(&remote);
        QVERIFY(!control<QCheckBox>(network,"cat1Enabled")->isEnabled()); QVERIFY(!control<QCheckBox>(options,"catWelcome")->isEnabled()); QVERIFY(!control<QCheckBox>(ptt,"catPttEnabled")->isEnabled());
        bool explained=false; for (QLabel* label:network.findChildren<QLabel*>()) { explained=explained || label->text().contains("local host"); } QVERIFY(explained); QVERIFY(!remote.catService()->isStarted());
    }
    void restoredNativeFormatsRemainExact() {
        RadioModel model; CatService& service=*model.catService();
        CatEndpointConfig endpoint=service.channelConfig(1); endpoint.serialBaud=14400; endpoint.serialDataBits=5;
        QVERIFY(service.reconfigureChannel(1,endpoint)); CatGlobalConfig global=service.globalConfig();
        global.pttSerialBaud=14400; global.pttSerialDataBits=5; QVERIFY(service.reconfigureGlobal(global));
        CatSerialPortsPage serial(&model); CatPttSetupPage ptt(&model);
        QCOMPARE(control<QComboBox>(serial,"cat1Baud")->currentText(),QString("14400"));
        QCOMPARE(control<QComboBox>(serial,"cat1Bits")->currentText(),QString("5"));
        QCOMPARE(control<QComboBox>(ptt,"catPttBaud")->currentText(),QString("14400"));
        QCOMPARE(control<QSpinBox>(ptt,"catPttBits")->value(),5);
        QCOMPARE(service.channelConfig(1).serialDataBits,5); QCOMPARE(service.globalConfig().pttSerialDataBits,5);
    }
    void testerCallbacksCannotLeaveDormantState() {
        RadioModel model; model.addSlice(); CatService& service=*model.catService(); service.startConfigured();
        connect(&service,&CatService::messageLogged,&model,[&](int,bool inbound,const QByteArray&) { if (inbound) { service.stopAll(); } });
        QCOMPARE(service.testCommand(1,"FA00014074000;"),QByteArray("?;")); QVERIFY(service.sessionIds(1).isEmpty());
        QVERIFY(model.sliceById(0)->frequency()!=14074000.0); QVERIFY(!service.isStarted());
    }
    void optionsRoundTripAndTesterHasRealEffectsWithoutSessions() {
        RadioModel model; model.addSlice(); model.sliceOwnership()->setOwner(0,SliceOwnership::stationDevice()); CatService& service=*model.catService(); service.startConfigured();
        CatOptionsSetupPage options(&model); QSignalSpy changed(&service,&CatService::globalConfigurationChanged);
        control<QComboBox>(options,"catRigIdentity")->setCurrentText("TS-480"); QCOMPARE(changed.size(),1); QCOMPARE(service.testCommand(1,"ID;"),QByteArray("ID020;"));
        control<QCheckBox>(options,"catRttyA")->setChecked(true); control<QSpinBox>(options,"catRttyDigu")->setValue(-1234);
        control<QCheckBox>(options,"catAllowAi")->setChecked(true); control<QCheckBox>(options,"catAi")->setChecked(true);
        const CatGlobalConfig global=service.globalConfig(); QCOMPARE(global.rigIdentity,QString("TS-480")); QVERIFY(global.rttyOffsetAEnabled && global.allowKenwoodAi && global.aiEnabled); QCOMPARE(global.rttyDiguHz,-1234);
        QCOMPARE(AppSettings::instance().value("Cat/RttyDiguHz").toInt(),-1234);
        QCOMPARE(service.testCommand(1,"FA00014074000;"),QByteArray()); QCOMPARE(model.sliceById(0)->frequency(),14074000.0);
        QCOMPARE(service.testCommand(1,"FA;"),QByteArray("FA00014074000;"));
        for (const QByteArray& command:QList<QByteArray>{"TX;","ZZTX1;","ZZTU1;","ZZUT1;","ZZVE1;","ZZUS;","ZZLI1;"}) { QCOMPARE(service.testCommand(1,command),QByteArray("?;")); }
        QVERIFY(!model.transmitModel().isMox()); QVERIFY(!model.transmitModel().voxEnabled()); QVERIFY(!model.isTune()); QVERIFY(service.sessionIds(1).isEmpty());
        QCOMPARE(service.testCommand(1,"ZZGA12345678-1234-1234-1234-123456789abc;"),QByteArray("ZZGA12345678-1234-1234-1234-123456789abc;")); QVERIFY(service.sessionIds(1).isEmpty());
    }
    void synchronousConfigurationCallbackMayDeletePage() {
        RadioModel model; CatService& service=*model.catService(); service.startConfigured();
        auto page=std::make_unique<CatTcpIpPage>(&model); QPointer<CatTcpIpPage> alive(page.get());
        auto* port=control<QSpinBox>(*page,"cat1Port"); const int chosen=unusedPort();
        { const QSignalBlocker blocked(port); port->setValue(chosen); }
        connect(&service,&CatService::configurationChanged,&model,[&](int) { page.reset(); });
        QVERIFY(QMetaObject::invokeMethod(port,"valueChanged",Qt::DirectConnection,Q_ARG(int,chosen)));
        QVERIFY(!alive); QCOMPARE(service.channelConfig(1).tcpPort,chosen);
    }
    void synchronousGlobalCallbackMayDeletePage() {
        RadioModel model; CatService& service=*model.catService(); auto page=std::make_unique<CatOptionsSetupPage>(&model);
        QPointer<CatOptionsSetupPage> alive(page.get()); auto* identity=control<QComboBox>(*page,"catRigIdentity");
        { const QSignalBlocker blocked(identity); identity->setCurrentText("TS-480"); }
        connect(&service,&CatService::globalConfigurationChanged,&model,[&] { page.reset(); });
        QVERIFY(QMetaObject::invokeMethod(identity,"currentTextChanged",Qt::DirectConnection,Q_ARG(QString,QString("TS-480"))));
        QVERIFY(!alive); QCOMPARE(service.globalConfig().rigIdentity,QString("TS-480"));
    }
    void synchronousTesterCallbackMayDeletePage() {
        RadioModel model; CatService& service=*model.catService(); auto page=std::make_unique<CatOptionsSetupPage>(&model);
        QPointer<CatOptionsSetupPage> alive(page.get()); auto* send=control<QPushButton>(*page,"catTesterSend");
        connect(&service,&CatService::messageLogged,&model,[&](int,bool,const QByteArray&) { page.reset(); });
        QVERIFY(QMetaObject::invokeMethod(send,"clicked",Qt::DirectConnection,Q_ARG(bool,false)));
        QVERIFY(!alive); QVERIFY(service.sessionIds(1).isEmpty());
    }
    void testerButtonAndLogRequestUseRealService() {
        RadioModel model; model.addSlice(); model.sliceOwnership()->setOwner(0,SliceOwnership::stationDevice());
        CatService& service=*model.catService(); service.startConfigured(); CatOptionsSetupPage options(&model);
        auto* command=control<QLineEdit>(options,"catTesterCommand"); auto* send=control<QPushButton>(options,"catTesterSend");
        command->setText("FA00014074000;"); send->click(); QCOMPARE(model.sliceById(0)->frequency(),14074000.0);
        QVERIFY(control<QLabel>(options,"catTesterReply")->text().contains("no wire reply"));
        command->setText("ID;"); send->click(); QCOMPARE(control<QLabel>(options,"catTesterReply")->text(),QString("ID019;"));
        command->setText("TX;"); send->click(); QCOMPARE(control<QLabel>(options,"catTesterReply")->text(),QString("?;"));
        QVERIFY(!model.transmitModel().isMox()); QVERIFY(service.sessionIds(1).isEmpty());
        QSignalSpy requested(&options,&CatOptionsSetupPage::showLogRequested);
        control<QPushButton>(options,"catShowLog")->click(); QCOMPARE(requested.size(),1);
    }
    void malformedTesterTrafficIsLoggedExactly() {
        RadioModel model; CatService& service=*model.catService(); CatLogWindow log(&service);
        QCOMPARE(service.testCommand(1,"unknown;"),QByteArray("?;"));
        const QString displayed=control<QPlainTextEdit>(log,"catLogText")->toPlainText();
        QVERIFY(displayed.contains("in bytes=8  unknown;  [hex 75 6e 6b 6e 6f 77 6e 3b]"));
        QVERIFY(displayed.contains("out bytes=2  ?;  [hex 3f 3b]")); QVERIFY(service.sessionIds(1).isEmpty());
    }
    void logSeparatesExactBytesDiagnosticsPauseAndBound() {
        RadioModel model; CatService& service=*model.catService(); service.startConfigured(); CatLogWindow log(&service);
        auto* text=control<QPlainTextEdit>(log,"catLogText"); QVERIFY(text); QCOMPARE(service.testCommand(1,"id;"),QByteArray("ID019;"));
        const QString first=text->toPlainText(); QVERIFY(first.contains("in bytes=3  id;  [hex 69 64 3b]")); QVERIFY(first.contains("out bytes=6  ID019;  [hex 49 44 30 31 39 3b]"));
        auto* pause=control<QPushButton>(log,"catLogPause"); pause->setChecked(true); service.testCommand(1,"FA;"); QCOMPARE(text->toPlainText(),first); pause->setChecked(false);
        CatEndpointConfig config=service.channelConfig(1); config.tcpEnabled=true; config.tcpPort=unusedPort(); QVERIFY(service.reconfigureChannel(1,config)); QVERIFY(text->toPlainText().contains("TCP: Listening"));
        for (int i=0;i<5100;++i) { service.testCommand(1,"ID;"); } QVERIFY(text->blockCount()<=10000);
        control<QComboBox>(log,"catLogFilter")->setCurrentIndex(1); QVERIFY(!text->toPlainText().contains("out bytes="));
        control<QComboBox>(log,"catLogFilter")->setCurrentIndex(0); QVERIFY(text->toPlainText().contains("out bytes="));
        control<QPushButton>(log,"catLogClear")->click(); QVERIFY(text->toPlainText().isEmpty());
    }
    void captureSandboxWidgets() {
        const QString directory=qEnvironmentVariable("NEREUS_CAT_CAPTURE_DIR"); if (directory.isEmpty()) { QSKIP("Visual capture is requested explicitly in the sandbox native/scaled runs."); }
        QVERIFY(QDir().mkpath(directory)); RadioModel model; model.addSlice(); model.addSlice(); CatService& service=*model.catService(); service.startConfigured();
        QTemporaryDir devices; QVERIFY(devices.isValid()); CatEndpointConfig first=service.channelConfig(1); first.tcpEnabled=true; first.tcpPort=unusedPort(); first.rigctldEnabled=true; first.rigctldPort=unusedPort(); QVERIFY(service.reconfigureChannel(1,first));
        QTcpServer occupied; QVERIFY(occupied.listen(QHostAddress::LocalHost,0)); CatEndpointConfig second=service.channelConfig(2); second.tcpEnabled=true; second.tcpPort=occupied.serverPort(); second.rigctldEnabled=true; second.rigctldPort=occupied.serverPort(); QVERIFY(service.reconfigureChannel(2,second));
        CatEndpointConfig third=service.channelConfig(3); third.ptyEnabled=true; third.ptyDialect="Rigctld"; third.serialEnabled=true; third.serialDevice=devices.filePath("absent.serial"); QVERIFY(service.reconfigureChannel(3,third));
        QWidget window; auto* root=new QVBoxLayout(&window); auto* applet=new CatApplet(&model,&window); root->addWidget(applet); auto* tabs=new QTabWidget(&window); root->addWidget(tabs,1);
        tabs->addTab(new CatTcpIpPage(&model,tabs),"TCP / PTY"); tabs->addTab(new CatSerialPortsPage(&model,tabs),"Serial"); tabs->addTab(new CatOptionsSetupPage(&model,tabs),"Options / Tester"); tabs->addTab(new CatPttSetupPage(&model,tabs),"Input PTT");
        RadioModel remote(RadioModel::Role::Remote); tabs->addTab(new CatTcpIpPage(&remote,tabs),"Remote scope");
        control<QPushButton>(*tabs->widget(2),"catTesterSend")->click();
        window.resize(1280,800); window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        for (int i=0;i<tabs->count();++i) { tabs->setCurrentIndex(i); QCoreApplication::processEvents(); const QString path=directory+QStringLiteral("/task-11-%1.png").arg(i); QVERIFY(window.grab().save(path)); qInfo()<<"CAPTURE"<<path<<"logical"<<window.size()<<"platform"<<QGuiApplication::platformName()<<"DPR"<<window.devicePixelRatioF(); }
        tabs->setCurrentIndex(2); for (QScrollArea* scroll:tabs->currentWidget()->findChildren<QScrollArea*>()) { scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum()); } QCoreApplication::processEvents(); QVERIFY(window.grab().save(directory+"/task-11-tester.png"));
        SetupDialog setup(&model); setup.resize(1280,800); setup.show(); QVERIFY(QTest::qWaitForWindowExposed(&setup));
        const QStringList pages={"TCP/IP CAT","Serial Ports","CAT Options","CAT PTT"};
        for (int i=0;i<pages.size();++i) {
            setup.selectPage(pages[i]); QCoreApplication::processEvents();
            if (pages[i]=="CAT Options") { control<QPushButton>(setup,"catTesterSend")->click(); QCoreApplication::processEvents(); auto* output=control<QLabel>(setup,"catTesterReply"); QCOMPARE(output->text(),QString("ID019;")); QVERIFY(output->width()>=output->fontMetrics().horizontalAdvance(output->text())); }
            if (QGuiApplication::platformName()=="offscreen" || qEnvironmentVariable("QT_SCALE_FACTOR").isEmpty()) { QCOMPARE(setup.size(),QSize(1280,800)); }
            const QString path=directory+QStringLiteral("/task-11-setup-%1.png").arg(i);
            QVERIFY(setup.grab().save(path)); qInfo()<<"SETUP CAPTURE"<<path<<"logical"<<setup.size()<<"platform"<<QGuiApplication::platformName()<<"DPR"<<setup.devicePixelRatioF();
        }
        setup.selectPage("TCP/IP CAT"); for (QScrollArea* scroll:setup.findChildren<QScrollArea*>()) { scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum()); }
        QCoreApplication::processEvents(); QVERIFY(setup.grab().save(directory+"/task-13-setup-rigctld-bottom.png"));
        setup.selectPage("CAT Options"); for (QScrollArea* scroll:setup.findChildren<QScrollArea*>()) { scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum()); }
        QCoreApplication::processEvents(); QVERIFY(setup.grab().save(directory+"/task-11-setup-tester.png"));
        SetupDialog remoteSetup(&remote); remoteSetup.resize(1280,800); remoteSetup.selectPage("TCP/IP CAT"); remoteSetup.show(); QVERIFY(QTest::qWaitForWindowExposed(&remoteSetup));
        QCoreApplication::processEvents(); QVERIFY(remoteSetup.grab().save(directory+"/task-11-setup-remote.png"));
        CatLogWindow log(&service); log.resize(1280,800); log.show(); service.testCommand(1,"id;"); service.testCommand(1,"TX;"); first.tcpEnabled=false; QVERIFY(service.reconfigureChannel(1,first)); QCoreApplication::processEvents(); QVERIFY(log.grab().save(directory+"/task-11-log.png"));
    }
};
QTEST_MAIN(TstCatSetup)
#include "tst_cat_setup.moc"
