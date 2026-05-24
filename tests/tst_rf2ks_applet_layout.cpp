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
    void gaugesUpdateFromPowerSnapshot();
    void antennasRowReflectsList();
    void clickingAntennaButtonEmitsRequest();
    void tunerStatusLineReflectsSnapshot();
    void tuneAndBypassButtonsAreDisabled();
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

void Rf2ksAppletLayoutTest::gaugesUpdateFromPowerSnapshot() {
    RadioModel m;
    Rf2ksApplet a(&m);
    RfKitPowerSnapshot snap;
    snap.forwardW = 850;
    snap.swr = 1.4f;
    snap.temperatureC = 38.0f;
    snap.voltageV = 48.0f;
    snap.currentA = 18.2f;
    a.setPower(snap);
    QCOMPARE(a.fwdGaugeValueForTesting(),  850);
    QCOMPARE(a.swrGaugeValueForTesting(),  1.4f);
    QCOMPARE(a.tempGaugeValueForTesting(), 38.0f);
    QVERIFY(a.telemetryStripTextForTesting().contains("48 V"));
    QVERIFY(a.telemetryStripTextForTesting().contains("18.2 A"));
}

void Rf2ksAppletLayoutTest::antennasRowReflectsList() {
    RadioModel m;
    Rf2ksApplet a(&m);
    a.setAntennaLabel(1, "80m dipole");
    a.setAntennaLabel(2, "20m beam");
    QList<RfKitAntenna> list{
        {RfKitAntenna::Type::Internal, 1, RfKitAntenna::State::Active},
        {RfKitAntenna::Type::Internal, 2, RfKitAntenna::State::Available},
        {RfKitAntenna::Type::Internal, 3, RfKitAntenna::State::Available},
        {RfKitAntenna::Type::Internal, 4, RfKitAntenna::State::Disabled},
    };
    a.setAntennas(list);
    QCOMPARE(a.antennaButtonTextForTesting(1), QString("80m dipole"));
    QCOMPARE(a.antennaButtonTextForTesting(2), QString("20m beam"));
    QCOMPARE(a.antennaButtonTextForTesting(4), QString("ANT 4"));
    QVERIFY(a.antennaButtonIsActiveForTesting(1));
    QVERIFY(!a.antennaButtonIsEnabledForTesting(4));
}

void Rf2ksAppletLayoutTest::clickingAntennaButtonEmitsRequest() {
    RadioModel m;
    Rf2ksApplet a(&m);
    QSignalSpy spy(&a, &Rf2ksApplet::antennaRequested);
    a.clickAntennaButtonForTesting(2);
    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(static_cast<int>(args.at(0).toInt()), static_cast<int>(RfKitAntenna::Type::Internal));
    QCOMPARE(args.at(1).toInt(), 2);
}

void Rf2ksAppletLayoutTest::tunerStatusLineReflectsSnapshot() {
    RadioModel m;
    Rf2ksApplet a(&m);
    RfKitTunerSnapshot snap;
    snap.mode = RfKitTunerSnapshot::Mode::Auto;
    snap.setup = "LC";
    snap.tunedFrequencyKHz = 14155;
    a.setTuner(snap);
    QCOMPARE(a.tunerStatusTextForTesting(),
             QString("TUNED 14.155 MHz (LC)"));

    snap.mode = RfKitTunerSnapshot::Mode::AutoTuning;
    a.setTuner(snap);
    QCOMPARE(a.tunerStatusTextForTesting(), QString("TUNING..."));

    snap.mode = RfKitTunerSnapshot::Mode::Bypass;
    a.setTuner(snap);
    QCOMPARE(a.tunerStatusTextForTesting(), QString("BYPASS"));
}

void Rf2ksAppletLayoutTest::tuneAndBypassButtonsAreDisabled() {
    RadioModel m;
    Rf2ksApplet a(&m);
    QVERIFY(!a.tuneButtonIsEnabledForTesting());
    QVERIFY(!a.bypassButtonIsEnabledForTesting());
    QVERIFY(a.tuneButtonTooltipForTesting().contains("front panel"));
}

QTEST_MAIN(Rf2ksAppletLayoutTest)
#include "tst_rf2ks_applet_layout.moc"
