// no-port-check: NereusSDR-original test infrastructure. No upstream port.
//
// =================================================================
// A band change must retune the DDC even while CTUN holds it.
// =================================================================
//
// CTUN pins the DDC so that spinning the VFO inside the pinned window moves
// the slice's shift offset instead of retuning hardware. ReceiverManager
// implements that with m_ddcFreqLocked, which gates the hardware emit in
// setReceiverFrequency.
//
// A band change is a different act. The allocator classifies it as
// NewStream or RetunedStream, meaning the stream CENTRE has to move, and a
// centre move is the operator asking for a retune rather than a nudge
// inside the window. ReceiverManager says so itself, on
// forceHardwareFrequency:
//
//   the lock bypass is untouched and deliberate: m_ddcFreqLocked gates
//   setReceiverFrequency's hardware emit so a VFO move inside a pinned
//   CTUN window does not retune the DDC, while the pan drag itself is
//   exactly the operator asking for a retune.
//
// bindSliceToStream used to push centre moves through setReceiverFrequency,
// so in CTUN the push was swallowed by that lock and the DDC never followed
// a band button. Confirmed on a live Hermes Lite 2 on 2026-07-31: a 40 m to
// 60 m band press produced `outcome=NewStream` from the allocator and then
// `ddcLocked=true` at the drop site, and neither the Alex high-pass nor the
// receive low-pass moved until the operator nudged the VFO far enough to
// force a re-placement.
//
// The operator-visible cost was the receive preselector sitting on the
// previous band, which is silent on the air and easy to miss.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/ReceiverManager.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

// Records the receive frequencies that actually reach a connection.
// File scope, not an anonymous namespace: moc cannot generate a
// staticMetaObject for a Q_OBJECT class with internal linkage.
class RxFreqMockConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<quint64> rxFreqCalls;

    explicit RxFreqMockConnection(QObject* parent = nullptr)
        : RadioConnection(parent) { setState(ConnectionState::Connected); }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64 hz) override { rxFreqCalls.append(hz); }
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setAlexRxBpf(AlexRxBpf) override {}
    void setWatchdogEnabled(bool e) override { m_watchdogEnabled = e; }
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

class TestBandChangeRetunesDdcInCtun : public QObject {
    Q_OBJECT
private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // The lock does its job for a nudge inside the window: no hardware push.
    void ctunLock_swallowsAnOrdinaryReceiverRetune()
    {
        ReceiverManager rm;
        QCOMPARE(rm.createReceiver(), 0);
        rm.activateReceiver(0);
        rm.setDdcMapping(0, 0);
        rm.setDdcFrequencyLocked(true);

        QSignalSpy hw(&rm, &ReceiverManager::hardwareFrequencyChanged);
        rm.setReceiverFrequency(0, 5354000ULL);

        QCOMPARE(hw.count(), 0);
    }

    // ...and must NOT swallow a deliberate centre move. This is the arm
    // bindSliceToStream takes for NewStream / RetunedStream.
    void centreMove_reachesHardwareDespiteTheLock()
    {
        ReceiverManager rm;
        QCOMPARE(rm.createReceiver(), 0);
        rm.activateReceiver(0);
        rm.setDdcMapping(0, 0);
        rm.setDdcFrequencyLocked(true);

        QSignalSpy hw(&rm, &ReceiverManager::hardwareFrequencyChanged);
        rm.forceHardwareFrequency(0, 5354000ULL);

        QCOMPARE(hw.count(), 1);
        QCOMPARE(hw.at(0).at(1).toULongLong(), 5354000ULL);
    }

    // The forced path still respects the two conditions that are about
    // whether the receiver exists at all, rather than about CTUN.
    void centreMove_staysSilentWhenTheReceiverIsNotStreaming()
    {
        ReceiverManager rm;
        QCOMPARE(rm.createReceiver(), 0);
        rm.setDdcMapping(0, 0);
        rm.deactivateReceiver(0);
        rm.setDdcFrequencyLocked(true);

        QSignalSpy hw(&rm, &ReceiverManager::hardwareFrequencyChanged);
        rm.forceHardwareFrequency(0, 5354000ULL);

        QCOMPARE(hw.count(), 0);
    }

    // The forced path records what it commanded, so a later
    // rebuildHardwareMapping re-emit does not resurrect the old centre and
    // yank the pan back. That property is why forceHardwareFrequency stores
    // as well as emits.
    void centreMove_isRememberedAcrossAMappingRebuild()
    {
        ReceiverManager rm;
        QCOMPARE(rm.createReceiver(), 0);
        rm.activateReceiver(0);
        rm.setDdcMapping(0, 0);
        rm.setDdcFrequencyLocked(true);

        rm.forceHardwareFrequency(0, 5354000ULL);

        QSignalSpy hw(&rm, &ReceiverManager::hardwareFrequencyChanged);
        rm.setDdcMapping(0, 1);   // forces a mapping rebuild + re-emit

        for (const QList<QVariant>& call : hw) {
            QCOMPARE(call.at(1).toULongLong(), 5354000ULL);
        }
    }

    // ── The call site, which is where the defect actually lived ─────────
    //
    // Everything above tests ReceiverManager's two primitives. This one
    // tests that bindSliceToStream reaches for the right one: with CTUN
    // holding the DDC, a band-sized retune must still reach the radio.
    //
    // Before the fix this asserted zero pushes, because bindSliceToStream
    // routed centre moves through setReceiverFrequency and the lock ate
    // them.
    void bandChangeInCtun_reachesTheRadio()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount=*/1, /*maxSlices=*/1,
                                  /*defaultRateHz=*/192000);

        auto* mock = new RxFreqMockConnection();
        model.injectConnectionForTest(mock);

        const int a = model.addSlice();
        SliceModel* const slice = model.slices().at(a);
        slice->setFrequency(7191000.0);          // 40 m
        model.setActiveSlice(a);
        model.wireSliceSignalsForTest();

        // A hardware push needs a receiver that is streaming, which on a
        // live radio publishDdcAssignment arranges from the per-board
        // codec. There is no codec here, so state it explicitly: this test
        // is about which ReceiverManager entry point bindSliceToStream
        // picks, not about DDC assignment.
        QVERIFY(model.receiverManager() != nullptr);
        // addSlice does not stand up a receiver; production does that from
        // publishDdcAssignment. Create it here so there is something for the
        // push to land on.
        model.receiverManager()->createReceiver();
        model.receiverManager()->activateReceiver(0);
        model.receiverManager()->setDdcMapping(0, 0);

        // CTUN pins the DDC, exactly as MainWindow does when the operator
        // turns it on (MainWindow.cpp:1820).
        model.receiverManager()->setDdcFrequencyLocked(true);

        // Spy on ReceiverManager's hardware emit rather than on the mock
        // connection: injectConnectionForTest only assigns m_connection, and
        // the hardwareFrequencyChanged -> connection wiring lives in
        // wireConnectionSignals, which needs a real connect. The emit IS the
        // boundary this fix is about; what consumes it is separate wiring.
        QSignalSpy hw(model.receiverManager(),
                      &ReceiverManager::hardwareFrequencyChanged);

        slice->setFrequency(5354000.0);          // band press: 40 m -> 60 m

        QVERIFY2(!hw.isEmpty(),
                 "band change in CTUN never reached the radio; the DDC and "
                 "both Alex filters stay on the previous band");
        QCOMPARE(hw.last().at(1).toULongLong(), quint64(5354000));

        model.injectConnectionForTest(nullptr);
        delete mock;
    }
};

QTEST_MAIN(TestBandChangeRetunesDdcInCtun)
#include "tst_band_change_retunes_ddc_in_ctun.moc"
