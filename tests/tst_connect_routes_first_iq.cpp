// =================================================================
// tests/tst_connect_routes_first_iq.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. ReceiverManager activation and the
// stream pool are NereusSDR constructs with no Thetis equivalent to port
// from. The DDC numbers asserted here come from the codecs, which are
// cited to Thetis / mi0bot at their own call sites.
//
// Bench report 2026-07-31 (JJ, KG4VCF), Hermes Lite 2 over a routed link:
// "when i connect it takes a retune to get things going ie no waterfall
// until i move the flag". Every I/Q packet between connect and the first
// VFO nudge was discarded:
//
//   P1: first ep6 frame received from "::ffff:x.x.x.17" (1032 bytes)
//   P1: first iqDataReceived emit; rx= 0 samples= 76
//   ReceiverManager: first feedIqData dropped; hwReceiverIndex= 0 map= "(empty)"
//
// An empty m_hwToLogical means no receiver was active, and
// rebuildHardwareMapping only maps active ones.
//
// connectToRadio activates receiver 0 (RadioModel.cpp:5527) and then calls
// bindUnboundSlices (:5579), whose requestDdcAssignment tail reaches
// publishDdcAssignment. At that point in the function the connection object
// does not exist yet (m_connection is assigned at :5855) and the codec has
// not been installed on ReceiverManager yet (wireConnectionSignals, called
// at :7674, does that). So computeDdcAssignment found no codec to ask and
// returned the default DdcAssignment, whose streamDdc is all -1, and the
// activation loop read that as "the radio is not streaming this" and
// deactivated the receiver that had been activated forty lines earlier.
//
// Nothing re-ran the assignment once the codec arrived, so the mapping
// stayed empty until the operator moved the VFO: frequencyChanged ->
// bindSliceToStream -> requestDdcAssignment, this time with a codec.
//
// Not the same fault as the suspended-stream deactivation
// (tst_suspended_stream_has_no_receiver): there the -1 was the codec's
// considered answer and honouring it is correct. Here there was no codec
// and therefore no answer at all, and the two must not be conflated --
// which is exactly what the shared -1 sentinel did.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/DdcAssignment.h"
#include "core/HpsdrModel.h"
#include "core/ReceiverManager.h"
#include "core/codec/P1CodecHl2.h"
#include "core/codec/P2CodecSaturn.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

// One ep6 frame's worth of interleaved I/Q. 76 samples is what the bench
// log reported for a 1032-byte Protocol 1 frame.
QVector<float> oneFrame(int samples = 76)
{
    QVector<float> v;
    v.reserve(samples * 2);
    for (int i = 0; i < samples; ++i) {
        v.append(0.1f);
        v.append(0.2f);
    }
    return v;
}

// RadioModel::connectToRadio's receiver / slice / pool sequence, in its
// production order, with only the socket-owning steps left out.
//
// The order is the whole point of this test, so each step carries the line
// it mirrors. Anything that reorders connectToRadio has to reorder this
// too, and should, because the ordering is what broke.
void runConnectSequence(RadioModel& model, int userDdcCount, int maxSlices)
{
    ReceiverManager* rm = model.receiverManager();

    rm->createReceiver();                                   // :5491
    model.addSlice(QStringLiteral("pan-0"));                // :5507
    rm->activateReceiver(0);                                // :5527
    model.configureStreamPool(userDdcCount, maxSlices, 192000);  // :5564
    for (int st = 1; st < userDdcCount; ++st) {             // :5569-5573
        if (rm->receiverConfig(st).receiverIndex < 0) {
            rm->createReceiver();
        }
    }
    model.bindUnboundSlices();                              // :5579
}

} // namespace

class TestConnectRoutesFirstIq : public QObject
{
    Q_OBJECT

private slots:

    // ── 1. The reported case, Protocol 1 / HL2 ───────────────────────────
    //
    // The codec arrives after the pool is bound, exactly as it does on a
    // live connect. The first ep6 frame must still be routed.
    void p1_connect_routes_the_first_frame()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        ReceiverManager* rm = model.receiverManager();
        QVERIFY(rm);

        // HL2: maxSlices = 1, userDdcCount = 1 (BoardCapabilities.cpp:819-820).
        runConnectSequence(model, /*userDdcCount*/ 1, /*maxSlices*/ 1);

        // wireConnectionSignals (:7978) installs the codec only now.
        P1CodecHl2 codec;
        rm->setP1Codec(&codec);

        QVERIFY2(rm->isReceiverActive(0),
                 "receiver 0 must survive connect: an inactive receiver holds "
                 "no DDC mapping and every ep6 frame is dropped until the "
                 "operator moves the VFO");

        QSignalSpy routed(rm, &ReceiverManager::iqDataForReceiver);
        rm->feedIqData(0, oneFrame());
        QCOMPARE(routed.count(), 1);
        QCOMPARE(routed.at(0).at(0).toInt(), 0);
    }

    // ── 2. The same ordering on Protocol 2 ───────────────────────────────
    //
    // connectToRadio is one function for both protocols, so the window is
    // not P1-specific even though the bench report was. Saturn's primary RX
    // is DDC2 (DDC0/DDC1 are the diversity / PureSignal pair), so this also
    // pins that the surviving mapping is the codec's DDC and not a
    // sequential fallback.
    void p2_connect_routes_the_first_frame()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        ReceiverManager* rm = model.receiverManager();
        QVERIFY(rm);

        // Saturn / ANAN-G2: maxSlices = 5, userDdcCount = 5
        // (BoardCapabilities.cpp:1000-1001).
        runConnectSequence(model, /*userDdcCount*/ 5, /*maxSlices*/ 5);

        P2CodecSaturn codec;
        rm->setP2Codec(&codec);

        QVERIFY2(rm->isReceiverActive(0),
                 "receiver 0 must survive connect on Protocol 2 as well: "
                 "connectToRadio is one function for both protocols");

        const int ddc = rm->receiverConfig(0).hardwareRx;
        QVERIFY2(ddc >= 0, "an active receiver must hold a hardware mapping");

        QSignalSpy routed(rm, &ReceiverManager::iqDataForReceiver);
        rm->feedIqData(ddc, oneFrame());
        QCOMPARE(routed.count(), 1);
        QCOMPARE(routed.at(0).at(0).toInt(), 0);
    }

    // ── 3. A stream with no slices still stays deactivated ───────────────
    //
    // The fix must not become "activate everything at connect". Streams 1-4
    // on a 5-DDC board host nothing after a single-slice connect and must
    // hold no DDC mapping, or their DDC numbers collide with whatever the
    // radio later assigns.
    void empty_streams_stay_deactivated_through_connect()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        ReceiverManager* rm = model.receiverManager();
        QVERIFY(rm);

        runConnectSequence(model, /*userDdcCount*/ 5, /*maxSlices*/ 5);

        P2CodecSaturn codec;
        rm->setP2Codec(&codec);

        QCOMPARE(rm->activeReceiverCount(), 1);
        for (int st = 1; st < 5; ++st) {
            QVERIFY2(!rm->isReceiverActive(st),
                     "only the stream Slice A bound to may be active after a "
                     "single-slice connect");
        }
    }

    // ── 4. Connect raises no suspended-stream notice ─────────────────────
    //
    // The dropped packets were the reported symptom, not the only one. The
    // same fabricated assignment also satisfied the suspension block twenty
    // lines above the activation loop -- stream 0 hosting a slice with no DDC
    // is exactly its condition -- so every connect emitted streamsSuspended
    // and MainWindow raised a six-second warning toast reading "Slice A has no
    // receiver: this radio has no DDC free for them right now."
    //
    // Worth its own assertion rather than being left to follow from the
    // activation fix: it is the operator-visible half, it is driven from a
    // different block, and a future change could restore one without the
    // other.
    void connect_raises_no_suspended_stream_notice()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        ReceiverManager* rm = model.receiverManager();
        QVERIFY(rm);

        QSignalSpy suspended(&model, &RadioModel::streamsSuspended);

        runConnectSequence(model, /*userDdcCount*/ 1, /*maxSlices*/ 1);
        P1CodecHl2 codec;
        rm->setP1Codec(&codec);

        QVERIFY2(suspended.isEmpty(),
                 "a normal connect must not tell the operator their slice has "
                 "no receiver: nothing was suspended, the codec had simply not "
                 "been asked yet");
    }

    // ── 5. A real suspension still deactivates ───────────────────────────
    //
    // Guard on the fix's blast radius, held here as well as in
    // tst_suspended_stream_has_no_receiver: once a codec IS present, a -1
    // is its considered answer and must still stop the receiver. Commit
    // 5851998a exists because PureSignal feedback reached a pan through a
    // receiver that had been left active, and no connect-time fix may
    // reopen that.
    void a_codec_that_says_minus_one_still_deactivates()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        ReceiverManager* rm = model.receiverManager();
        QVERIFY(rm);

        runConnectSequence(model, /*userDdcCount*/ 5, /*maxSlices*/ 5);

        P2CodecSaturn codec;
        rm->setP2Codec(&codec);
        QVERIFY2(rm->isReceiverActive(0), "precondition: receiving normally");

        // What the Hermes-class codec emits while PureSignal transmits:
        // every user stream loses its DDC while still hosting its slices.
        NereusSDR::DdcAssignment suspended;
        for (int st = 0; st < 5; ++st) { suspended.streamDdc[st] = -1; }
        suspended.psFwdDdc = 0;
        suspended.psRevDdc = 1;
        model.publishDdcAssignmentForTest(suspended);

        QCOMPARE(rm->activeReceiverCount(), 0);

        QSignalSpy routed(rm, &ReceiverManager::iqDataForReceiver);
        rm->feedIqData(0, oneFrame());
        QCOMPARE(routed.count(), 0);
    }
};

QTEST_MAIN(TestConnectRoutesFirstIq)
#include "tst_connect_routes_first_iq.moc"
