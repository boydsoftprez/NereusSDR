// no-port-check: NereusSDR-original CAT applet regression tests.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <memory>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QTcpServer>
#include "gui/applets/CatApplet.h"
#include "models/RadioModel.h"
#include "core/cat/CatService.h"
#include "core/AppSettings.h"
using namespace NereusSDR;
class TstCatApplet : public QObject {
    Q_OBJECT
private slots:
    void indicatorsExternalSyncAndCatOneScope() {
        AppSettings::instance().clear(); RadioModel model; CatService& service=*model.catService();
        QTcpServer reservation; QVERIFY(reservation.listen(QHostAddress::LocalHost,0)); const int port=reservation.serverPort(); reservation.close();
        CatEndpointConfig two=service.channelConfig(2); two.tcpEnabled=true; two.tcpPort=port; QVERIFY(service.applyChannelConfig(2,two));
        service.startConfigured(); CatApplet applet(&model); auto* tcp=applet.findChild<QPushButton*>("catTcpButton"); QVERIFY(tcp);
        QCOMPARE(applet.findChild<QLabel*>("catTcpLed2")->toolTip().contains("Listening"),true);
        QVERIFY(tcp->toolTip().contains("CAT1")); QSignalSpy changed(&service,&CatService::configurationChanged);
        QTcpServer oneReservation; QVERIFY(oneReservation.listen(QHostAddress::LocalHost,0)); const int onePort=oneReservation.serverPort(); oneReservation.close();
        QVERIFY(onePort!=port); CatEndpointConfig one=service.channelConfig(1); one.tcpEnabled=true; one.tcpPort=onePort; QVERIFY(service.reconfigureChannel(1,one));
        QCOMPARE(changed.size(),1); QVERIFY(tcp->isChecked()); tcp->click(); QCOMPARE(changed.size(),2); QVERIFY(!service.channelConfig(1).tcpEnabled);
        QVERIFY(service.isListening(2)); QCOMPARE(service.boundPort(2),port);
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        one=service.channelConfig(1); one.ptyEnabled=true; QVERIFY(service.reconfigureChannel(1,one));
        QVERIFY(!service.ptySlavePath(1).isEmpty()); QCOMPARE(applet.findChild<QLabel*>("catPtyPath1")->text(),service.ptySlavePath(1));
#endif
        RadioModel remote(RadioModel::Role::Remote); CatApplet remoteApplet(&remote); auto* remoteTcp=remoteApplet.findChild<QPushButton*>("catTcpButton");
        QVERIFY(!remoteTcp->isEnabled()); QVERIFY(remoteTcp->toolTip().contains("local host"));
        bool vax=false,iq=false; for (QPushButton* button:applet.findChildren<QPushButton*>()) { vax=vax || button->text()=="VAX"; iq=iq || button->text()=="IQ"; } QVERIFY(vax && iq);
    }
    void unavailableAudioControlsKeepTheirReasons() {
        AppSettings::instance().clear(); RadioModel model; CatApplet applet(&model);
        int retained=0;
        for (QPushButton* button:applet.findChildren<QPushButton*>()) {
            if (button->text()=="VAX" || button->text()=="IQ") {
                ++retained; QVERIFY(!button->isEnabled());
                QVERIFY(button->toolTip().contains("CAT applet"));
                QVERIFY(!button->toolTip().contains("not built"));
            }
        }
        QCOMPARE(retained,2);
        QComboBox* rates=applet.findChild<QComboBox*>(); QVERIFY(rates);
        QCOMPARE(rates->count(),3); QVERIFY(!rates->isEnabled());
        QVERIFY(rates->toolTip().contains("I/Q audio output"));
        CatEndpointConfig config=model.catService()->channelConfig(1);
        config.ptyDialect="Rigctld"; QVERIFY(model.catService()->reconfigureChannel(1,config));
        auto* pty=applet.findChild<QPushButton*>("catPtyButton"); QVERIFY(pty);
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
        QVERIFY(pty->isEnabled()); QVERIFY(pty->toolTip().contains("Rigctld PTY"));
#else
        QVERIFY(!pty->isEnabled()); QVERIFY(pty->toolTip().contains("only on macOS and Linux"));
        QVERIFY(!pty->toolTip().contains("Enable Rigctld"));
#endif
        RadioModel remote(RadioModel::Role::Remote); CatApplet remoteApplet(&remote);
        auto* remotePty=remoteApplet.findChild<QPushButton*>("catPtyButton"); QVERIFY(remotePty);
        QVERIFY(!remotePty->isEnabled()); QVERIFY(remotePty->toolTip().contains("local host"));
    }
    void configurationCallbackMayDeleteApplet() {
        AppSettings::instance().clear(); RadioModel model; CatService& service=*model.catService();
        CatEndpointConfig configured; configured.tcpPort=12345; QVERIFY(service.applyChannelConfig(1,configured));
        auto applet=std::make_unique<CatApplet>(&model); QPointer<CatApplet> alive(applet.get());
        auto* button=applet->findChild<QPushButton*>("catTcpButton");
        connect(&service,&CatService::configurationChanged,&model,[&](int) { applet.reset(); });
        QVERIFY(QMetaObject::invokeMethod(button,"toggled",Qt::DirectConnection,Q_ARG(bool,true)));
        QVERIFY(!alive); QVERIFY(service.channelConfig(1).tcpEnabled); QVERIFY(!service.isStarted());
    }
    void tcpButtonAppliesCatOne() {
        AppSettings::instance().clear(); RadioModel model;
        CatEndpointConfig config; config.tcpPort=12345; QVERIFY(model.catService()->applyChannelConfig(1,config));
        CatApplet applet(&model); QPushButton* tcp=nullptr;
        for (QPushButton* button:applet.findChildren<QPushButton*>()) { if (button->text()=="TCP") { tcp=button; } }
        QVERIFY(tcp); tcp->click(); QVERIFY(model.catService()->channelConfig(1).tcpEnabled);
    }
};
QTEST_MAIN(TstCatApplet)
#include "tst_cat_applet.moc"
