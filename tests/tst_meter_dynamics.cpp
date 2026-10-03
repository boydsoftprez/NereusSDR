// no-port-check: NereusSDR-original numeric oracle, independent of implementation.
#include <QtTest>
#include "gui/meters/MeterDynamics.h"
using namespace NereusSDR;
class TestMeterDynamics : public QObject {
    Q_OBJECT
private slots:
    void recurrenceAndEqualInput() {
        MeterDynamics d; d.configure(0.8,0.1,100,2000,0); d.reset(-30);
        const double inputs[]{0,0,-30,-30};
        const double expected[]{-6,-1.2,-4.08,-6.672};
        for (int i=0;i<4;++i) { d.push(inputs[i]); QVERIFY(d.advance(100*(i+1))); QVERIFY(qAbs(d.value()-expected[i])<1e-10); }
        QCOMPARE(d.historySize(),4); QCOMPARE(d.maxHistory(),-1.2);
        QVERIFY(!d.advance(400));
    }
    void expiryHoldIgnoreAndAbsence() {
        MeterDynamics d; d.configure(1,1,100,200,200); d.reset(-30);
        d.push(12); d.advance(100); d.push(-12); d.advance(200);
        QCOMPARE(d.historySize(),0); d.advance(300); QCOMPARE(d.maxHistory(),-12.0);
        d.push(6); d.advance(400); QCOMPARE(d.maxHistory(),6.0);
        d.push(-20); d.advance(500); d.advance(600); QCOMPARE(d.maxHistory(),-20.0);
        d.push(-400,false); d.advance(700); QVERIFY(!d.hasReading()); QCOMPARE(d.historySize(),0);
        d.push(0); d.advance(800); QCOMPARE(d.value(),0.0);
    }
    void intervals_data() { QTest::addColumn<int>("ms"); for (int ms : {10,100,500,2000}) { QTest::newRow(qPrintable(QString::number(ms)))<<ms; } }
    void intervals() {
        QFETCH(int,ms); MeterDynamics d; d.configure(1,1,ms,ms*2,0); d.reset(-30);
        d.push(12); d.advance(ms); d.push(-30); d.advance(ms*2); d.advance(ms*3);
        QCOMPARE(d.historySize(),2); QCOMPARE(d.maxHistory(),-30.0);
    }
};
QTEST_GUILESS_MAIN(TestMeterDynamics)
#include "tst_meter_dynamics.moc"
