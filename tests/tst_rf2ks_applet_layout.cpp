#include <QtTest/QtTest>
#include "gui/applets/Rf2ksApplet.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class Rf2ksAppletLayoutTest : public QObject {
    Q_OBJECT
private slots:
    void appletIdAndTitle();
    void headerShowsDeviceName();
    void operateButtonReflectsModelState();
    void operateButtonClickEmitsRequest();
};

void Rf2ksAppletLayoutTest::appletIdAndTitle() {
    RadioModel m;
    Rf2ksApplet a(&m);
    QCOMPARE(a.appletId(),    QString("RfKit"));
    QCOMPARE(a.appletTitle(), QString("RF-Kit RF2K-S"));
}

void Rf2ksAppletLayoutTest::headerShowsDeviceName() {
    RadioModel m;
    Rf2ksApplet a(&m);
    a.setNicknameAndVersion("KG4VCF", "G200C267");
    QCOMPARE(a.deviceLabelTextForTesting(),   QString("RF-Kit RF2K-S"));
    QCOMPARE(a.nicknameLabelTextForTesting(), QString("KG4VCF  G200C267"));
}

void Rf2ksAppletLayoutTest::operateButtonReflectsModelState() {
    RadioModel m;
    Rf2ksApplet a(&m);
    a.setOperateMode("STANDBY");
    QCOMPARE(a.operateButtonTextForTesting(), QString("STANDBY"));
    a.setOperateMode("OPERATE");
    QCOMPARE(a.operateButtonTextForTesting(), QString("OPERATE"));
}

void Rf2ksAppletLayoutTest::operateButtonClickEmitsRequest() {
    RadioModel m;
    Rf2ksApplet a(&m);
    a.setOperateMode("STANDBY");
    QSignalSpy spy(&a, &Rf2ksApplet::operateToggled);
    a.clickOperateButtonForTesting();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);   // wants OPERATE
}

QTEST_MAIN(Rf2ksAppletLayoutTest)
#include "tst_rf2ks_applet_layout.moc"
