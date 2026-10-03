#include <QtTest/QtTest>
#include "gui/widgets/VfoLevelBar.h"
#include <limits>
using namespace NereusSDR;

class TestVfoLevelBar : public QObject {
    Q_OBJECT
private slots:
    void fillFractionAtFloor() {
        VfoLevelBar bar; bar.resize(200, 24);
        bar.setValue(-130.0f);
        QCOMPARE(bar.fillFraction(), 0.0);
    }
    void fillFractionAtCeiling() {
        VfoLevelBar bar; bar.resize(200, 24);
        bar.setValue(-20.0f);
        QCOMPARE(bar.fillFraction(), 1.0);
    }
    void fillFractionAtS9() {
        VfoLevelBar bar; bar.resize(200, 24);
        bar.setValue(-73.0f);  // S9 boundary
        QCOMPARE(bar.fillFraction(), (-73.0 - -130.0) / (-20.0 - -130.0));
    }
    void clampsBelowFloor() {
        VfoLevelBar bar; bar.setValue(-200.0f);
        QCOMPARE(bar.fillFraction(), 0.0);
    }
    void clampsAboveCeiling() {
        VfoLevelBar bar; bar.setValue(10.0f);
        QCOMPARE(bar.fillFraction(), 1.0);
    }
    void colorSwitchesAtS9() {
        VfoLevelBar bar;
        bar.setValue(-74.0f); QCOMPARE(bar.isAboveS9(), false);
        bar.setValue(-73.0f); QCOMPARE(bar.isAboveS9(), true);
    }
    // R-R3-13: the -400 dBm sentinel (and a non-finite level) is no
    // reading: an empty bar and "-- dBm", never "-400 dBm".
    void noReadingShowsDashesAndEmptyBar() {
        VfoLevelBar bar; bar.resize(230, 26);
        bar.setValue(-65.0f);
        QCOMPARE(bar.readoutText(), QStringLiteral("-65 dBm"));
        QVERIFY(bar.hasReading());
        bar.setValue(VfoLevelBar::kNoReadingDbm);
        QVERIFY(!bar.hasReading());
        QCOMPARE(bar.fillFraction(), 0.0);
        QCOMPARE(bar.isAboveS9(), false);
        QCOMPARE(bar.readoutText(), QStringLiteral("-- dBm"));
        QVERIFY(!bar.grab().isNull());
        bar.setValue(std::numeric_limits<float>::quiet_NaN());
        QCOMPARE(bar.readoutText(), QStringLiteral("-- dBm"));
        // The next real level displays exactly as before.
        bar.setValue(-130.0f);
        QVERIFY(bar.hasReading());
        QCOMPARE(bar.readoutText(), QStringLiteral("-130 dBm"));
        QCOMPARE(bar.fillFraction(), 0.0);
        bar.setValue(-20.0f);
        QCOMPARE(bar.fillFraction(), 1.0);
    }
};
QTEST_MAIN(TestVfoLevelBar)
#include "tst_vfo_level_bar.moc"
