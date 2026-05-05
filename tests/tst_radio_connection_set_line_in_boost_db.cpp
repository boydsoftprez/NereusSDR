// =================================================================
// tst_radio_connection_set_line_in_boost_db.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test. Verifies the dB to 5-bit
// index translation derived from Thetis console.cs:40827-40859
// [v2.10.3.13+501e3f51] MakeLineInList() + SetMicGain().

#include <QtTest/QtTest>
#include "core/RadioConnection.h"

using namespace NereusSDR;

namespace {

class FakeRadioConnection : public RadioConnection {
    Q_OBJECT
public:
    int lastIndex{-1};
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
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
    void setLineInGain(int gain) override { lastIndex = gain; }
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    void setWatchdogEnabled(bool) override {}
};

} // namespace

class TestRadioConnectionSetLineInBoostDb : public QObject {
    Q_OBJECT
private slots:
    void minDbMapsToIndexZero() {
        FakeRadioConnection c;
        c.setLineInBoost(-34.5);
        QCOMPARE(c.lastIndex, 0);
    }
    void zeroDbMapsToIndexTwentyThree() {
        FakeRadioConnection c;
        c.setLineInBoost(0.0);
        QCOMPARE(c.lastIndex, 23);
    }
    void maxDbMapsToIndexThirtyOne() {
        FakeRadioConnection c;
        c.setLineInBoost(12.0);
        QCOMPARE(c.lastIndex, 31);
    }
    void belowMinClampsToZero() {
        FakeRadioConnection c;
        c.setLineInBoost(-50.0);
        QCOMPARE(c.lastIndex, 0);
    }
    void aboveMaxClampsToThirtyOne() {
        FakeRadioConnection c;
        c.setLineInBoost(20.0);
        QCOMPARE(c.lastIndex, 31);
    }
    void onePointFiveDbStepWalk() {
        FakeRadioConnection c;
        for (int i = 0; i < 32; ++i) {
            const double db = -34.5 + 1.5 * i;
            c.setLineInBoost(db);
            QCOMPARE(c.lastIndex, i);
        }
    }
};

QTEST_MAIN(TestRadioConnectionSetLineInBoostDb)
#include "tst_radio_connection_set_line_in_boost_db.moc"
