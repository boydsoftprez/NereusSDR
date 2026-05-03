// no-port-check: test-only — exercises RadioConnection::pingRttMeasured signal.
// No Thetis logic is ported here; NereusSDR-original.
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
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTT(bool) override {}
    void setMicXlr(bool) override {}
    void setWatchdogEnabled(bool) override {}
};

class TstRadioConnectionPingRtt : public QObject {
    Q_OBJECT

private slots:
    void signalEmitsValueOnRttMeasured() {
        NullRadioConnection rc;
        QSignalSpy spy(&rc, &RadioConnection::pingRttMeasured);
        rc.notePingSent();
        QTest::qWait(15);
        rc.notePingReceived();
        QCOMPARE(spy.count(), 1);
        const int rtt = spy.takeFirst().at(0).toInt();
        QVERIFY2(rtt >= 10 && rtt < 100,
                 qPrintable(QStringLiteral("rtt=%1 outside [10,100)").arg(rtt)));
    }

    void duplicateReceiveDoesNotEmit() {
        NullRadioConnection rc;
        QSignalSpy spy(&rc, &RadioConnection::pingRttMeasured);
        rc.notePingSent();
        QTest::qWait(5);
        rc.notePingReceived();
        rc.notePingReceived();
        QCOMPARE(spy.count(), 1);
    }

    void receiveWithoutSendDoesNotEmit() {
        NullRadioConnection rc;
        QSignalSpy spy(&rc, &RadioConnection::pingRttMeasured);
        rc.notePingReceived();
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_APPLESS_MAIN(TstRadioConnectionPingRtt)
#include "tst_radio_connection_ping_rtt.moc"
