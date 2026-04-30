// no-port-check: test-only — exercises RadioConnection::supplyVoltsChanged
// and userAdc0Changed signals. No Thetis logic is ported here; NereusSDR-original.
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/RadioConnection.h"

using namespace NereusSDR;

// Minimal concrete subclass so we can instantiate the abstract base.
class NullRadioConnection : public RadioConnection {
    Q_OBJECT
public:
    NullRadioConnection() : RadioConnection() {}
    void init() override {}
    void connectToRadio(const RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void sendTxIq(const float*, int) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setMicPTT(bool) override {}
    void setMicXlr(bool) override {}
    void setWatchdogEnabled(bool) override {}
};

class TstRadioConnectionSupplyVolts : public QObject {
    Q_OBJECT

private slots:
    void supplyVoltsChangedEmitsConvertedValue() {
        NullRadioConnection rc;
        QSignalSpy spy(&rc, &RadioConnection::supplyVoltsChanged);
        // Hermes DC voltage formula: V = (raw / 4095) * 3.3 * (4.7 + 0.82) / 0.82
        // From Thetis console.cs computeHermesDCVoltage() [v2.10.3.13].
        // raw=2700: (2700/4095) * 3.3 * (5.52/0.82) = 0.6593 * 3.3 * 6.7317 ≈ 14.65 V
        // raw=2543 would yield ~13.8 V; we use 2700 and assert the formula is applied.
        rc.handleSupplyRaw(2700);
        QCOMPARE(spy.count(), 1);
        const float v = spy.takeFirst().at(0).toFloat();
        QVERIFY2(qAbs(v - 14.65f) < 0.5f,
                 qPrintable(QStringLiteral("got %1 V, expected ~14.65").arg(v)));
    }

    void userAdc0ChangedEmitsConvertedValue() {
        NullRadioConnection rc;
        QSignalSpy spy(&rc, &RadioConnection::userAdc0Changed);
        rc.handleUserAdc0Raw(2400);
        QCOMPARE(spy.count(), 1);
        const float v = spy.takeFirst().at(0).toFloat();
        QVERIFY(v > 30.0f && v < 70.0f);
    }

    void identicalRawDoesNotReEmit() {
        NullRadioConnection rc;
        QSignalSpy spy(&rc, &RadioConnection::supplyVoltsChanged);
        rc.handleSupplyRaw(2700);
        rc.handleSupplyRaw(2700);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_APPLESS_MAIN(TstRadioConnectionSupplyVolts)
#include "tst_radio_connection_supply_volts.moc"
