// no-port-check: NereusSDR-original functional CAT setup, diagnostics and sandbox visual tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-06 Remote and tester wording follows the operator text.
//            J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-06 rigctld starts on its default port. J.J. Boyd (KG4VCF),
//            AI-assisted via Anthropic Claude Code.
// 2026-10-07 Remote reasons and the log window follow CatControl.
//            J.J. Boyd / KG4VCF, AI tooling: Claude Code.
// 2026-10-09 "Always recenter VFOs" sits in the option rows' indicator
//            column, both looks. J.J. Boyd (KG4VCF), AI-assisted via
//            Anthropic Claude Code.
// 2026-10-12 A local window is told the rule a refused setting broke.
//            J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-12 The serial and Input PTT Enable ticks wait for what they need.
//            J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-22: ports a test hands to a later listener come from
// unusedLoopbackPort() (UnusedLoopbackPort.h), below the kernel's ephemeral
// range, so a parallel test cannot take one between the pick and the listen.
// A listener held for the whole test keeps port 0. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
#include "UnusedLoopbackPort.h"
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
#include "GridHeaderAlignment.h"
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
int unusedPort() { return unusedLoopbackPort(); }
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
        QVERIFY(enabled && port && address && dialect); QVERIFY(enabled->isEnabled()); QVERIFY(!enabled->isChecked()); QCOMPARE(port->value(),4532);
        port->setValue(0); QCOMPARE(service.channelConfig(1).rigctldPort,0);
        enabled->setChecked(true); QVERIFY(!enabled->isChecked()); QVERIFY(!service.channelConfig(1).rigctldEnabled);
        const int chosen=unusedPort(); QVERIFY(chosen>0); port->setValue(chosen); enabled->setChecked(true);
        QVERIFY(service.isListening(1)); QCOMPARE(service.rigctldBoundPort(1),chosen); QCOMPARE(service.boundPort(1),0);
        QVERIFY(control<QLabel>(page,"cat1RigctldStatus")->text().contains(QString::number(chosen)));
        QTcpSocket socket; socket.connectToHost(QHostAddress::LocalHost,chosen); QTRY_COMPARE(service.rigctldClientCount(1),1);
        QVERIFY(control<QLabel>(page,"cat1RigctldStatus")->text().contains("Clients: 1"));
        dialect->setCurrentText("Rigctld"); QCOMPARE(service.channelConfig(1).ptyDialect,QString("Rigctld"));
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        QVERIFY(control<QCheckBox>(page,"cat1Pty")->toolTip().contains("Rigctld"));
#else
        QVERIFY(!control<QCheckBox>(page,"cat1Pty")->isEnabled());
        QVERIFY(control<QCheckBox>(page,"cat1Pty")->toolTip().contains("only on macOS and Linux"));
#endif
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
    void ptyReasonsFollowNativePlatformAndHost() {
        RadioModel model; CatService& service=*model.catService(); CatTcpIpPage page(&model);
        for (int channel=1;channel<=4;++channel) {
            CatEndpointConfig config=service.channelConfig(channel); config.ptyDialect="Rigctld";
            QVERIFY(service.reconfigureChannel(channel,config)); page.syncFromModel();
            auto* pty=page.findChild<QCheckBox*>(QStringLiteral("cat%1Pty").arg(channel)); QVERIFY(pty);
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
            QVERIFY(pty->isEnabled()); QVERIFY(pty->toolTip().contains("Rigctld"));
#else
            QVERIFY(!pty->isEnabled()); QVERIFY(pty->toolTip().contains("only on macOS and Linux"));
            QVERIFY(pty->toolTip().contains("virtual COM")); QVERIFY(!pty->toolTip().contains("uses Rigctld"));
#endif
        }
        RadioModel remote(RadioModel::Role::Remote); CatTcpIpPage remotePage(&remote); remotePage.syncFromModel();
        for (int channel=1;channel<=4;++channel) {
            auto* pty=remotePage.findChild<QCheckBox*>(QStringLiteral("cat%1Pty").arg(channel)); QVERIFY(pty);
            QVERIFY(!pty->isEnabled()); QVERIFY(pty->toolTip().contains("Connect to the Core"));
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
    void lanListenWarnsAndPortsApplyWhenTypingEnds() {
        RadioModel model; CatService& service=*model.catService(); service.startConfigured(); CatTcpIpPage page(&model);
        QVERIFY(!control<QSpinBox>(page,"cat1Port")->keyboardTracking()); QVERIFY(!control<QSpinBox>(page,"cat1RigctldPort")->keyboardTracking());
        const QString warning=QStringLiteral("Open to your network");
        CatEndpointConfig config=service.channelConfig(1); config.tcpEnabled=true; config.tcpPort=unusedPort(); config.rigctldEnabled=true; config.rigctldPort=unusedPort();
        QVERIFY(config.tcpPort>0 && config.rigctldPort>0 && config.tcpPort!=config.rigctldPort); QVERIFY(service.reconfigureChannel(1,config));
        QVERIFY(!control<QLabel>(page,"cat1Status")->text().contains(warning)); QVERIFY(!control<QLabel>(page,"cat1RigctldStatus")->text().contains(warning));
        // 192.0.2.1 is the documentation range (RFC 5737): never bound here, so no listener opens.
        config.tcpBindAddress=QStringLiteral("192.0.2.1"); config.rigctldBindAddress=QStringLiteral("192.0.2.1"); QVERIFY(service.reconfigureChannel(1,config));
        QVERIFY(control<QLabel>(page,"cat1Status")->text().contains(warning)); QVERIFY(control<QLabel>(page,"cat1RigctldStatus")->text().contains(warning));
        config.tcpEnabled=false; config.rigctldEnabled=false; QVERIFY(service.reconfigureChannel(1,config));
        QVERIFY(!control<QLabel>(page,"cat1Status")->text().contains(warning)); QVERIFY(!control<QLabel>(page,"cat1RigctldStatus")->text().contains(warning));
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
        bool explained=false; for (QLabel* label:network.findChildren<QLabel*>()) { explained=explained || label->text().contains("Connect to the Core"); } QVERIFY(explained); QVERIFY(!remote.catService()->isStarted());
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
    void localWindowIsToldTheRuleARefusedSettingBroke() {
        RadioModel model; CatService& service=*model.catService(); CatControl* cat=model.catControl(); QVERIFY(cat && !cat->remote());
        int answers=0; bool accepted=true; QString reason;
        const auto answer=[&](bool ok,const QString& why) { ++answers; accepted=ok; reason=why; };
        CatEndpointConfig one=service.channelConfig(1); one.serialEnabled=true;
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression(QStringLiteral("This serial port has no device")));
        cat->reconfigureChannel(1,one,answer); QCOMPARE(answers,1); QVERIFY(!accepted); QCOMPARE(reason,QString("This serial port has no device. Choose a device, then turn the port on."));
        one.serialDevice=QStringLiteral("shared-device"); cat->reconfigureChannel(1,one,answer); QCOMPARE(answers,2); QVERIFY(accepted); QVERIFY(reason.isEmpty());
        CatEndpointConfig two=service.channelConfig(2); two.serialEnabled=true; two.serialDevice=QStringLiteral("shared-device");
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression(QStringLiteral("CAT 2 refused: serial device")));
        cat->reconfigureChannel(2,two,answer); QCOMPARE(answers,3); QVERIFY(!accepted); QCOMPARE(reason,QString("That serial device is already used by CAT 1."));
        CatGlobalConfig global=service.globalConfig(); global.pttEnabled=true;
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression(QStringLiteral("Input PTT has no input source")));
        cat->reconfigureGlobal(global,answer); QCOMPARE(answers,4); QVERIFY(!accepted); QCOMPARE(reason,QString("Input PTT has no input source. Choose a source, then turn it on."));
        global.pttDeviceSource=QStringLiteral("Physical"); global.pttUseDsr=true; global.pttSerialDevice=QStringLiteral("shared-device");
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression(QStringLiteral("CAT PTT refused: serial device")));
        cat->reconfigureGlobal(global,answer); QCOMPARE(answers,5); QVERIFY(!accepted); QCOMPARE(reason,QString("That serial device is already used by CAT 1."));
        // No rule to name (a port the Core does not have): the general sentence.
        cat->reconfigureChannel(5,one,answer); QCOMPARE(answers,6); QVERIFY(!accepted); QCOMPARE(reason,QString("Configuration refused: check address, port, format and exclusive device assignment."));
        // The pages show it: a listener turned on with no port, and Input PTT turned on with no source.
        CatTcpIpPage tcp(&model); control<QSpinBox>(tcp,"cat3Port")->setValue(0);
        QTest::ignoreMessage(QtWarningMsg,QRegularExpression(QStringLiteral("Invalid CAT listener address")));
        control<QCheckBox>(tcp,"cat3Enabled")->setChecked(true); QVERIFY(!control<QCheckBox>(tcp,"cat3Enabled")->isChecked());
        QCOMPARE(control<QLabel>(tcp,"cat3Status")->text(),QString("Invalid CAT listener address or port."));
    }
    void serialEnableWaitsForADevice() {
        const QString waits=QStringLiteral("Choose a device first.");
        RadioModel model; CatService& service=*model.catService(); CatSerialPortsPage page(&model);
        auto* enabled=control<QCheckBox>(page,"cat1Enabled"); auto* device=control<QComboBox>(page,"cat1Device"); QVERIFY(enabled && device && device->lineEdit());
        if (!model.catControl()->platform().serial) {
            // No serial port support: the section is off with its own reason, and the tick adds none.
            QVERIFY(!enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty()); return;
        }
        // Nothing in the Device box: the tick cannot be ticked, and says why.
        QVERIFY(device->currentText().trimmed().isEmpty()); QVERIFY(!enabled->isChecked()); QVERIFY(!enabled->isEnabled()); QCOMPARE(enabled->toolTip(),waits);
        for (int channel=2;channel<=4;++channel) { auto* other=page.findChild<QCheckBox*>(QStringLiteral("cat%1Enabled").arg(channel)); QVERIFY(other && !other->isEnabled()); QCOMPARE(other->toolTip(),waits); }
        // It follows the box as a device is typed, before the typing is finished.
        device->lineEdit()->setText(QStringLiteral("   ")); QVERIFY(!enabled->isEnabled()); QCOMPARE(enabled->toolTip(),waits);
        device->lineEdit()->setText(QStringLiteral("/dev/typed")); QVERIFY(enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty());
        QVERIFY(!control<QCheckBox>(page,"cat2Enabled")->isEnabled());
        device->lineEdit()->clear(); QVERIFY(!enabled->isEnabled()); QCOMPARE(enabled->toolTip(),waits);
        // A device picked, then the port turned on: nothing is refused.
        device->setCurrentText(QStringLiteral("/dev/picked")); QVERIFY(enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(device->lineEdit(),"editingFinished",Qt::DirectConnection)); QCOMPARE(service.channelConfig(1).serialDevice,QString("/dev/picked"));
        QVERIFY(enabled->isEnabled()); enabled->setChecked(true); QVERIFY(service.channelConfig(1).serialEnabled); QVERIFY(enabled->isChecked());
        // A port that is on can always be turned off, whatever the box holds.
        QVERIFY(enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty());
        device->lineEdit()->clear(); QVERIFY(enabled->isChecked()); QVERIFY(enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty());
        page.syncFromModel(); QCOMPARE(device->currentText(),QString("/dev/picked")); QVERIFY(enabled->isEnabled());
        enabled->setChecked(false); QVERIFY(!service.channelConfig(1).serialEnabled); QVERIFY(enabled->isEnabled());
        // A change from elsewhere: the tick follows the device the page then shows.
        auto* second=control<QCheckBox>(page,"cat2Enabled"); CatEndpointConfig two=service.channelConfig(2); two.serialDevice=QStringLiteral("/dev/elsewhere");
        QVERIFY(service.reconfigureChannel(2,two)); QCOMPARE(control<QComboBox>(page,"cat2Device")->currentText(),QString("/dev/elsewhere")); QVERIFY(second->isEnabled()); QVERIFY(second->toolTip().isEmpty());
        two.serialDevice.clear(); QVERIFY(service.reconfigureChannel(2,two)); QVERIFY(!second->isEnabled()); QCOMPARE(second->toolTip(),waits);
    }
    void inputPttEnableWaitsForItsSourceInputAndDevice() {
        const QString noSource=QStringLiteral("Choose an input source first."); const QString noInput=QStringLiteral("Tick the CTS or the DSR input first."); const QString noDevice=QStringLiteral("Enter the separate physical device first.");
        RadioModel model; CatService& service=*model.catService(); CatPttSetupPage page(&model);
        auto* enabled=control<QCheckBox>(page,"catPttEnabled"); auto* source=control<QComboBox>(page,"catPttSource"); auto* cts=control<QCheckBox>(page,"catPttCts"); auto* dsr=control<QCheckBox>(page,"catPttDsr"); auto* device=control<QLineEdit>(page,"catPttDevice");
        QVERIFY(enabled && source && cts && dsr && device);
        if (!model.catControl()->platform().serial) {
            // No serial port support: the section is off with its own reason, and the tick adds none.
            QVERIFY(!enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty()); return;
        }
        // Each missing thing in turn, the first one named.
        QVERIFY(!enabled->isChecked()); QVERIFY(!enabled->isEnabled()); QCOMPARE(enabled->toolTip(),noSource);
        source->setCurrentText(QStringLiteral("Physical")); QCOMPARE(service.globalConfig().pttDeviceSource,QString("Physical")); QVERIFY(!enabled->isEnabled()); QCOMPARE(enabled->toolTip(),noInput);
        dsr->setChecked(true); QVERIFY(service.globalConfig().pttUseDsr); QVERIFY(!enabled->isEnabled()); QCOMPARE(enabled->toolTip(),noDevice);
        device->setText(QStringLiteral("  ")); QVERIFY(!enabled->isEnabled()); QCOMPARE(enabled->toolTip(),noDevice);
        // It follows the device as it is typed, before the typing is finished.
        device->setText(QStringLiteral("ptt-device")); QVERIFY(enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty());
        device->clear(); QVERIFY(!enabled->isEnabled()); QCOMPARE(enabled->toolTip(),noDevice);
        // A CAT port as the source needs no device of its own.
        source->setCurrentText(QStringLiteral("CAT2")); QVERIFY(enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty());
        dsr->setChecked(false); QVERIFY(!enabled->isEnabled()); QCOMPARE(enabled->toolTip(),noInput);
        cts->setChecked(true); QVERIFY(enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty());
        source->setCurrentText(QStringLiteral("None")); QVERIFY(!enabled->isEnabled()); QCOMPARE(enabled->toolTip(),noSource);
        // Everything there: it can be ticked, and nothing is refused.
        source->setCurrentText(QStringLiteral("Physical")); device->setText(QStringLiteral("ptt-device")); QVERIFY(QMetaObject::invokeMethod(device,"editingFinished",Qt::DirectConnection));
        QCOMPARE(service.globalConfig().pttSerialDevice,QString("ptt-device")); QVERIFY(enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty());
        enabled->setChecked(true); QVERIFY(service.globalConfig().pttEnabled); QVERIFY(enabled->isChecked());
        // While it is ticked it can always be unticked, whatever the page then shows.
        QVERIFY(enabled->isEnabled()); device->clear(); QVERIFY(enabled->isChecked()); QVERIFY(enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty());
        page.syncFromModel(); QCOMPARE(device->text(),QString("ptt-device")); QVERIFY(enabled->isChecked()); QVERIFY(enabled->isEnabled()); QVERIFY(enabled->toolTip().isEmpty());
        enabled->setChecked(false); QVERIFY(!service.globalConfig().pttEnabled); QVERIFY(enabled->isEnabled());
        // A change from elsewhere: the tick follows what the page then shows.
        CatGlobalConfig global=service.globalConfig(); global.pttUseCts=false; QVERIFY(service.reconfigureGlobal(global)); QVERIFY(!cts->isChecked()); QVERIFY(!enabled->isEnabled()); QCOMPARE(enabled->toolTip(),noInput);
    }
    void unavailablePageReasonWinsOverWhatEnableWaitsFor() {
        // A window with no Core: both ticks are off with the page's reason, not with what they wait for.
        RadioModel remote(RadioModel::Role::Remote); const QString reason=remote.catControl()->unavailableReason(); QVERIFY(!reason.isEmpty()); QVERIFY(!remote.catControl()->available());
        CatSerialPortsPage serial(&remote); CatPttSetupPage ptt(&remote);
        for (int channel=1;channel<=4;++channel) { auto* tick=serial.findChild<QCheckBox*>(QStringLiteral("cat%1Enabled").arg(channel)); QVERIFY(tick && !tick->isEnabled()); QCOMPARE(tick->toolTip(),reason); }
        auto* pttTick=control<QCheckBox>(ptt,"catPttEnabled"); QVERIFY(pttTick && !pttTick->isEnabled()); QCOMPARE(pttTick->toolTip(),reason);
        // Typing in the boxes changes neither.
        control<QComboBox>(serial,"cat1Device")->lineEdit()->setText(QStringLiteral("/dev/typed")); QCOMPARE(control<QCheckBox>(serial,"cat1Enabled")->toolTip(),reason); QVERIFY(!control<QCheckBox>(serial,"cat1Enabled")->isEnabled());
        control<QLineEdit>(ptt,"catPttDevice")->setText(QStringLiteral("ptt-device")); QCOMPARE(pttTick->toolTip(),reason); QVERIFY(!pttTick->isEnabled());
    }
    void testerCallbacksCannotLeaveDormantState() {
        RadioModel model; model.addSlice(); CatService& service=*model.catService(); service.startConfigured();
        connect(&service,&CatService::messageLogged,&model,[&](int,bool inbound,const QByteArray&) { if (inbound) { service.stopAll(); } });
        QCOMPARE(service.testCommand(1,"FA00014074000;"),QByteArray("?;")); QVERIFY(service.sessionIds(1).isEmpty());
        QVERIFY(model.sliceById(0)->frequency()!=14074000.0); QVERIFY(!service.isStarted());
    }
    // The disabled "Always recenter VFOs" row draws its indicator where the
    // option rows above it draw theirs (it sat about 2 px left of them).
    // Builds the page only; nothing starts listening.
    void alwaysRecenterIndicatorSitsInItsColumn_data() { QTest::addColumn<bool>("workspace"); QTest::newRow("today")<<false; QTest::newRow("workspace")<<true; }
    void alwaysRecenterIndicatorSitsInItsColumn() {
        QFETCH(bool,workspace); GridHeaderAlignment::AppLook look(workspace);
        RadioModel model; CatOptionsSetupPage page(&model); look.apply(&page); page.resize(900,650); page.show(); QVERIFY(QTest::qWaitForWindowExposed(&page));
        for (int i=0;i<4;++i) { QCoreApplication::sendPostedEvents(nullptr,QEvent::LayoutRequest); QCoreApplication::processEvents(); }
        QCheckBox* recenter=nullptr; for (QCheckBox* box:page.findChildren<QCheckBox*>()) { if (box->text()==QStringLiteral("Always recenter VFOs")) { recenter=box; } }
        auto* above=control<QCheckBox>(page,"catLimitPower"); QVERIFY(recenter && above);
        const auto indicator=[&page](const QCheckBox* box) { QStyleOptionButton option; option.initFrom(box); const QRect r=box->style()->subElementRect(QStyle::SE_CheckBoxIndicator,&option,box); return QRect(box->mapTo(&page,r.topLeft()),r.size()); };
        QCOMPARE(indicator(recenter).left(),indicator(above).left()); QCOMPARE(indicator(recenter).size(),indicator(above).size());
        QCOMPARE(recenter->height(),above->height());
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
        QVERIFY(control<QLabel>(options,"catTesterReply")->text().contains("no reply"));
        command->setText("ID;"); send->click(); QCOMPARE(control<QLabel>(options,"catTesterReply")->text(),QString("ID019;"));
        command->setText("TX;"); send->click(); QCOMPARE(control<QLabel>(options,"catTesterReply")->text(),QString("?;"));
        QVERIFY(!model.transmitModel().isMox()); QVERIFY(service.sessionIds(1).isEmpty());
        QSignalSpy requested(&options,&CatOptionsSetupPage::showLogRequested);
        control<QPushButton>(options,"catShowLog")->click(); QCOMPARE(requested.size(),1);
    }
    void malformedTesterTrafficIsLoggedExactly() {
        RadioModel model; CatService& service=*model.catService(); CatLogWindow log(model.catControl());
        QCOMPARE(service.testCommand(1,"unknown;"),QByteArray("?;"));
        const QString displayed=control<QPlainTextEdit>(log,"catLogText")->toPlainText();
        QVERIFY(displayed.contains("in bytes=8  unknown;  [hex 75 6e 6b 6e 6f 77 6e 3b]"));
        QVERIFY(displayed.contains("out bytes=2  ?;  [hex 3f 3b]")); QVERIFY(service.sessionIds(1).isEmpty());
    }
    void logWindowGeometryIsStoredAsTextAndRestored() {
        RadioModel model; CatService& service=*model.catService();
        { CatLogWindow log(model.catControl()); log.resize(612,383); log.show(); QVERIFY(QTest::qWaitForWindowExposed(&log)); log.close(); }
        const QVariant stored=AppSettings::instance().value("CatLogWindowGeometry");
        QCOMPARE(stored.typeId(),QMetaType::QString); const QString text=stored.toString(); QVERIFY(!text.isEmpty());
        QVERIFY(!QByteArray::fromBase64(text.toLatin1()).isEmpty());
        CatLogWindow restored(model.catControl()); QCOMPARE(restored.size(),QSize(612,383));
    }
    void logSeparatesExactBytesDiagnosticsPauseAndBound() {
        RadioModel model; CatService& service=*model.catService(); service.startConfigured(); CatLogWindow log(model.catControl());
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
        CatLogWindow log(model.catControl()); log.resize(1280,800); log.show(); service.testCommand(1,"id;"); service.testCommand(1,"TX;"); first.tcpEnabled=false; QVERIFY(service.reconfigureChannel(1,first)); QCoreApplication::processEvents(); QVERIFY(log.grab().save(directory+"/task-11-log.png"));
    }
};
QTEST_MAIN(TstCatSetup)
#include "tst_cat_setup.moc"
