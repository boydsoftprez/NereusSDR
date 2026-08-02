// =================================================================
// tests/tst_suspended_stream_has_no_receiver.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. ReceiverManager activation is a
// NereusSDR construct; Thetis has no equivalent bookkeeping layer to
// port from. The DDC numbers asserted here are cited to Thetis.
//
// Bench report 2026-07-31 (JJ, KG4VCF): tuning up on slice B, the TUNE
// tone appeared on pan 0's waterfall and came out the speakers, while the
// transmitter was on slice B's pan.
//
// One cause for both symptoms. publishDdcAssignment decided receiver
// activation from "does this stream host slices", twenty lines after the
// suspension block decided from "did the codec give this stream a DDC".
// During PureSignal transmit those disagree completely: the Hermes-class
// PS branch hands every user stream -1 while every stream still holds its
// slices, so the loop re-activated receivers the radio had just stopped.
//
// An active receiver holds a DDC mapping. On the 1-ADC HERMES class slice
// A's stream maps to DDC0 (P2CodecHermes streamDdc[0] = 0) and
// PureSignal's feedback leg also uses DDC0 (psFwdDdc = 0, Thetis
// cmaster.cs:538 [v2.10.3.15] SetPSRxIdx(0, 0)). So the feedback leg,
// which carries the transmitted signal, arrived on a DDC pan 0's receiver
// was still listening to, and took the ordinary RX path: onto pan 0's FFT
// and through that slice's WDSP channel to the speakers.
//
// The recurring shape on this branch, and the most expensive instance of
// it so far: a check reading a different thing from the one the code
// decides on. Here the two were in the same function.
// =================================================================

#include <QtTest/QtTest>

#include "core/DdcAssignment.h"
#include "core/ReceiverManager.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSuspendedStreamHasNoReceiver : public QObject
{
    Q_OBJECT

private:
    /// Two slices on two pans, so each holds its own stream. Mirrors the
    /// bench setup: slice A on 40 m, slice B on 80 m.
    static void seedTwoPans(RadioModel& model)
    {
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2E);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);

        // Receivers are created by connectToRadio, which a disconnected test
        // model never runs, and activateReceiver no-ops on a receiver that
        // does not exist. Same explicit seeding tst_pan_wide_badge does.
        if (ReceiverManager* rm = model.receiverManager()) {
            for (int st = 0; st < 4; ++st) {
                if (rm->receiverConfig(st).receiverIndex < 0) {
                    rm->createReceiver();
                }
            }
        }

        const int a = model.addSlice(QStringLiteral("pan-0"));
        if (SliceModel* sliceA = model.sliceById(a)) {
            sliceA->setFrequency(7183600.0);
        }
        const int b = model.addSlice(QStringLiteral("pan-1"));
        if (SliceModel* sliceB = model.sliceById(b)) {
            sliceB->setFrequency(3930100.0);
        }
    }

    /// An assignment with every user stream suspended, which is what the
    /// Hermes-class codec emits while PureSignal transmits.
    static NereusSDR::DdcAssignment allStreamsSuspended()
    {
        NereusSDR::DdcAssignment a;
        for (int st = 0; st < 5; ++st) { a.streamDdc[st] = -1; }
        a.psFwdDdc = 0;   // PureSignal takes DDC0, the one slice A had
        a.psRevDdc = 1;
        return a;
    }

private slots:

    // ── 1. The reported case ─────────────────────────────────────────────
    //
    // Every stream suspended, every stream still hosting slices. No
    // receiver may remain active, because an active receiver keeps a DDC
    // mapping and PureSignal is now using those DDCs.
    void a_suspended_stream_deactivates_its_receiver()
    {
        RadioModel model;
        seedTwoPans(model);

        ReceiverManager* rm = model.receiverManager();
        QVERIFY(rm);

        // Receiving normally first, which is what activates the receivers:
        // creating one leaves it inactive, and publishDdcAssignment is the
        // only thing that turns it on. This is the RX-then-key sequence the
        // bench hit, not a synthetic starting state.
        NereusSDR::DdcAssignment live;
        live.streamDdc[0] = 0;    // slice A on DDC0, per P2CodecHermes
        live.streamDdc[1] = 1;
        model.publishDdcAssignmentForTest(live);
        QVERIFY2(rm->activeReceiverCount() > 0,
                 "precondition: receivers are active while receiving");

        // Now key up with PureSignal. Every user stream loses its DDC and
        // PS takes DDC0, the one slice A was just listening to.
        model.publishDdcAssignmentForTest(allStreamsSuspended());

        QVERIFY2(rm->activeReceiverCount() == 0,
            "a stream the radio has stopped must not keep an active "
            "receiver: it holds the DDC mapping PureSignal feedback would "
            "arrive on, and that feedback is the transmitted signal");
    }

    // ── 2. It comes back afterwards ──────────────────────────────────────
    //
    // Deactivating on unkey and never re-activating would be a worse bug
    // than the one being fixed: silent receivers after every transmission.
    void unkeying_restores_the_receivers()
    {
        RadioModel model;
        seedTwoPans(model);
        ReceiverManager* rm = model.receiverManager();
        QVERIFY(rm);

        model.publishDdcAssignmentForTest(allStreamsSuspended());
        QCOMPARE(rm->activeReceiverCount(), 0);

        // Unkey: the codec hands the user streams their DDCs back.
        NereusSDR::DdcAssignment live;
        live.streamDdc[0] = 0;
        live.streamDdc[1] = 1;
        model.publishDdcAssignmentForTest(live);

        QVERIFY2(rm->activeReceiverCount() >= 2,
            "both slices' receivers must come back when the radio resumes "
            "streaming them, or the operator is deaf after every transmit");
    }

    // ── 3. Only the suspended streams go ─────────────────────────────────
    //
    // The ORION class keeps RX2 alive through PureSignal
    // (Thetis console.cs:8299-8303 [v2.10.3.15]), so a partial suspension
    // is a real wire state and must not take down the streams that
    // survived it.
    void a_stream_that_kept_its_ddc_keeps_its_receiver()
    {
        RadioModel model;
        seedTwoPans(model);
        ReceiverManager* rm = model.receiverManager();
        QVERIFY(rm);

        NereusSDR::DdcAssignment partial;
        partial.streamDdc[0] = -1;   // stream 0 suspended
        partial.streamDdc[1] = 3;    // stream 1 keeps DDC3
        model.publishDdcAssignmentForTest(partial);

        QVERIFY2(rm->activeReceiverCount() == 1,
            "exactly the stream that kept a DDC keeps a receiver");
    }

    // ── 4. An empty stream stays deactivated ─────────────────────────────
    //
    // The original condition still holds: a stream with no slices has no
    // receiver either, whatever DDC the codec offered it.
    void a_stream_with_no_slices_stays_deactivated()
    {
        RadioModel model;
        seedTwoPans(model);
        ReceiverManager* rm = model.receiverManager();
        QVERIFY(rm);

        NereusSDR::DdcAssignment generous;
        for (int st = 0; st < 5; ++st) { generous.streamDdc[st] = st; }
        model.publishDdcAssignmentForTest(generous);

        QVERIFY2(rm->activeReceiverCount() == 2,
            "only the two streams that actually host slices may be active, "
            "however many DDCs the codec handed out");
    }
};

QTEST_MAIN(TestSuspendedStreamHasNoReceiver)
#include "tst_suspended_stream_has_no_receiver.moc"
