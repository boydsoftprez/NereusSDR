// =================================================================
// tests/tst_tx_frequency_follows_tx_slice.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Expected
// behaviour is cited to Thetis in comments, but nothing here is a port.
//
// RF-SAFETY, companion to tst_alex_tx_lpf_source.cpp.
//
// Splitting the Alex LPF masks stops a RECEIVE retune from moving the
// transmit low-pass, but it only holds if the transmit frequency itself
// comes from the right slice. RadioModel used to push setTxFrequency from
// m_activeSlice — the slice the operator happens to be looking at — which
// in Phase 3F is not necessarily the slice bound to the transmitter.
//
// So with slice B TX-bound on 80 m and slice A merely active on 10 m,
// turning A's knob pushed a 10 m transmit frequency, and the TX low-pass
// followed it there. Same hazard as the LPF mirror, one layer up.
//
// Thetis takes the transmit frequency from the TX VFO, not the displayed
// one:
//   From Thetis console.cs:31889-31893 [v2.10.3.15] — the VFO A arm is
//   guarded so it does NOT run when VFO B is the transmit VFO:
//     if (!chkFullDuplex.Checked && !chkVFOBTX.Checked)
//     { tx_dds_freq_mhz = tx_freq; UpdateTXDDSFreq(); }
//   From Thetis console.cs:32866-32869 [v2.10.3.15] — the VFO B arm takes
// Upstream inline attribution preserved verbatim (console.cs:31897):
//   if (_click_tune_display) //-W2PA This was preventing proper receiver adjustment
//   over when B transmits:
//     if (!rx1_sub_drag) { tx_dds_freq_mhz = tx_freq; UpdateTXDDSFreq(); }
//
// and XIT is folded in while RIT deliberately is not:
//   From Thetis console.cs:31772-31784 [v2.10.3.15]
//     double rx_freq = freq;
//     double tx_freq = freq;
//     if (chkRIT.Checked && bRitOk) rx_freq += (int)udRIT.Value * 0.000001;
//     if (chkXIT.Checked)           tx_freq += (int)udXIT.Value * 0.000001;
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/TxSliceArbiter.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

// Records every transmit frequency the model pushes at the radio.
// File scope, not an anonymous namespace: moc cannot generate a
// staticMetaObject for a Q_OBJECT class with internal linkage.
class TxFreqMockConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<quint64> txFreqCalls;

    explicit TxFreqMockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64 hz) override { txFreqCalls.append(hz); }
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setAlexRxBpf(AlexRxBpf) override {}
    void setWatchdogEnabled(bool enabled) override { m_watchdogEnabled = enabled; }
    void sendTxIq(const float*, int) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

namespace {

// Detaches a stack-injected connection however the scope is left: QCOMPARE
// returns from the enclosing slot on failure, which would otherwise skip
// the trailing detach on exactly the run where it matters.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

constexpr double k10mHz = 28400000.0;   // slice A
constexpr double k80mHz =  3700000.0;   // slice B

} // namespace

class TestTxFrequencyFollowsTxSlice : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // Tuning a slice that is NOT bound to the transmitter must not move the
    // transmit frequency, even when it is the slice the operator is looking
    // at. Thetis's VFO A arm is guarded by `!chkVFOBTX.Checked` for exactly
    // this reason (console.cs:31889 [v2.10.3.15]).
    void tuningANonTxSlice_doesNotMoveTheTxFrequency()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        auto* mock = new TxFreqMockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(k10mHz);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(k80mHz);

        // Operator is looking at slice A, but slice B holds the transmitter.
        model.setActiveSlice(a);
        model.wireSliceSignalsForTest();
        QVERIFY(model.txSliceArbiter()->requestHandoff(b));
        QCOMPARE(model.txSliceArbiter()->txBoundSliceIndex(), b);

        mock->txFreqCalls.clear();
        model.slices().at(a)->setFrequency(k10mHz + 50000.0);

        QVERIFY2(mock->txFreqCalls.isEmpty(),
                 "tuning a non-TX slice pushed a transmit frequency");

        delete mock;
    }

    // The converse: the TX-bound slice must reach the radio even when it is
    // not the active one.
    void tuningTheTxBoundSlice_pushesTheNewTxFrequency()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new TxFreqMockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(k10mHz);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(k80mHz);

        model.setActiveSlice(a);
        model.wireSliceSignalsForTest();
        QVERIFY(model.txSliceArbiter()->requestHandoff(b));

        mock->txFreqCalls.clear();
        model.slices().at(b)->setFrequency(3750000.0);

        QVERIFY(!mock->txFreqCalls.isEmpty());
        QCOMPARE(mock->txFreqCalls.last(), quint64(3750000));

        delete mock;
    }

    // Handing the transmitter to another slice retunes the TX chain to that
    // slice straight away, rather than leaving the previous slice's
    // frequency latched until something else happens to move.
    //   From Thetis console.cs:32866-32869 [v2.10.3.15] — the VFO B arm
    //   assigns tx_dds_freq_mhz and calls UpdateTXDDSFreq() itself.
    void handoff_pushesTheNewTxSliceFrequency()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new TxFreqMockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(k10mHz);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(k80mHz);

        model.setActiveSlice(a);
        model.wireSliceSignalsForTest();

        mock->txFreqCalls.clear();
        QVERIFY(model.txSliceArbiter()->requestHandoff(b));

        QVERIFY2(!mock->txFreqCalls.isEmpty(),
                 "TX handoff left the transmit frequency on the old slice");
        QCOMPARE(mock->txFreqCalls.last(), quint64(k80mHz));

        delete mock;
    }

    // XIT belongs to the transmit frequency; RIT does not.
    //   From Thetis console.cs:31782-31784 [v2.10.3.15]
    //     if (chkXIT.Checked) tx_freq += (int)udXIT.Value * 0.000001;
    void xitOnTheTxBoundSlice_offsetsTheTxFrequency()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* mock = new TxFreqMockConnection();
        model.injectConnectionForTest(mock);
        DetachConnection detach{&model};

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(k10mHz);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(k80mHz);

        model.setActiveSlice(a);
        model.wireSliceSignalsForTest();
        QVERIFY(model.txSliceArbiter()->requestHandoff(b));

        mock->txFreqCalls.clear();
        model.slices().at(b)->setXitHz(1200);
        model.slices().at(b)->setXitEnabled(true);

        QVERIFY(!mock->txFreqCalls.isEmpty());
        QCOMPARE(mock->txFreqCalls.last(), quint64(k80mHz) + 1200);

        // RIT moves the receive frequency only — it must not reach the
        // transmit chain (Thetis applies udRIT to rx_freq alone).
        mock->txFreqCalls.clear();
        model.slices().at(b)->setRitHz(900);
        model.slices().at(b)->setRitEnabled(true);
        for (const quint64 hz : mock->txFreqCalls) {
            QCOMPARE(hz, quint64(k80mHz) + 1200);
        }

        delete mock;
    }
};

QTEST_MAIN(TestTxFrequencyFollowsTxSlice)
#include "tst_tx_frequency_follows_tx_slice.moc"
