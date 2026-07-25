// =================================================================
// tests/tst_stream_pool_binding.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic I Tasks 5-6: stream pool + slice binding.
// Phase 3F Sub-Epic I Task 7b: per-stream DDC assignment + routing.
// Phase 3F Sub-Epic I closeout, defect F1: bindings reach a late worker.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/ReceiverManager.h"
#include "core/codec/P1CodecRedPitaya.h"
#include "core/codec/P2CodecHermes.h"
#include "core/codec/P2CodecSaturn.h"
#include "models/RadioModel.h"
#include "models/RxDspWorker.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// One in_size chunk of interleaved I/Q, enough to make RxDspWorker drain
// and fan the chunk out to whatever slices it believes are on the stream.
QVector<float> oneChunk(int inSize)
{
    QVector<float> v;
    v.reserve(inSize * 2);
    for (int i = 0; i < inSize; ++i) {
        v.append(0.1f);
        v.append(0.2f);
    }
    return v;
}

// Slice indices the worker actually fanned a stream-`st` chunk out to.
// This reads the worker's real binding map through its production drain
// path rather than a test-only accessor, so it cannot pass on a map the
// DSP thread would never consult.
QVector<int> workerBindingsFor(RxDspWorker& w, int st, int inSize)
{
    QSignalSpy spy(&w, &RxDspWorker::sliceProcessed);
    w.processIqBatch(st, oneChunk(inSize));
    QVector<int> out;
    for (int i = 0; i < spy.count(); ++i) {
        out.append(spy.at(i).at(0).toInt());
    }
    return out;
}

} // namespace

class TestStreamPoolBinding : public QObject {
    Q_OBJECT
private slots:
    void pool_sizes_to_the_sku()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        QCOMPARE(model.streamPoolSize(), 5);
    }

    void first_slice_activates_stream_zero()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int idx = model.addSlice();
        SliceModel* s = model.slices().at(idx);

        QCOMPARE(s->streamIndex(), 0);
        QCOMPARE(s->shiftOffsetHz(), 0.0);
    }

    void same_band_slices_share_one_stream()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14225000.0);

        QCOMPARE(model.slices().at(b)->streamIndex(), 0);
        QCOMPARE(model.slices().at(b)->shiftOffsetHz(), 25000.0);
        QCOMPARE(model.activeStreamCount(), 1);
    }

    void cross_band_slices_take_separate_streams()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        QVERIFY(model.slices().at(b)->streamIndex() != 0);
        QCOMPARE(model.activeStreamCount(), 2);
    }

    void four_slices_fit_one_ddc_on_one_band()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const double base = 14200000.0;
        for (double off : {0.0, -40000.0, 25000.0, 60000.0}) {
            const int i = model.addSlice();
            model.slices().at(i)->setFrequency(base + off);
        }

        QCOMPARE(model.activeStreamCount(), 1);
        for (SliceModel* s : model.slices()) {
            QCOMPARE(s->streamIndex(), 0);
        }
    }

    void exhausting_ddcs_rejects_with_a_reason()
    {
        RadioModel model;
        // One DDC, room for several slices: the second slice on a
        // different band has nowhere to go.
        model.configureStreamPool(/*userDdcCount*/ 1, /*maxSlices*/ 5, 192000);

        QSignalSpy spy(&model, &RadioModel::sliceAddRejected);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        QVERIFY(spy.count() >= 1);
    }

    // ── Phase 3F Sub-Epic I Task 7b ─────────────────────────────────────
    //
    // Test seam: the codec is injected through ReceiverManager's existing
    // public setP2Codec(), which is exactly where connectToRadio puts it
    // (RadioModel.cpp p2CodecChanged wiring). No socket, no fake
    // connection, and the assertions still run the production path:
    // addSlice/setFrequency -> bindSliceToStream -> requestDdcAssignment
    // -> invokeCodecDdcAssignment -> publishDdcAssignment.

    void co_hosted_slices_share_one_ddc()
    {
        RadioModel model;
        P2CodecSaturn codec;
        model.receiverManager()->setP2Codec(&codec);
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14225000.0);

        // Both slices are on stream 0, so both must report the SAME DDC.
        // Indexing the codec by slice would hand them DDC2 and DDC3.
        QCOMPARE(model.slices().at(a)->streamIndex(),
                 model.slices().at(b)->streamIndex());
        QCOMPARE(model.slices().at(a)->ddcIndex(),
                 model.slices().at(b)->ddcIndex());

        // ...and it has to be a real DDC. Both would trivially agree on the
        // -1 sentinel if the publish path never ran at all.
        QVERIFY(model.slices().at(a)->ddcIndex() >= 0);
        QCOMPARE(model.activeStreamCount(), 1);
    }

    void each_active_stream_routes_its_ddc_to_its_receiver()
    {
        RadioModel model;
        P2CodecSaturn codec;
        model.receiverManager()->setP2Codec(&codec);
        model.configureStreamPool(5, 5, 192000);

        // One ReceiverManager receiver per stream, mirroring the loop
        // connectToRadio runs after configureStreamPool. Without them
        // setDdcMapping has no receiver to route onto.
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        // Two streams, two distinct DDCs, and each stream's logical receiver
        // must carry the DDC the codec chose or its packets get dropped by
        // ReceiverManager::feedIqData.
        const int ddcA = model.slices().at(a)->ddcIndex();
        const int ddcB = model.slices().at(b)->ddcIndex();
        QVERIFY(ddcA >= 0);
        QVERIFY(ddcB >= 0);
        QVERIFY(ddcA != ddcB);
        QCOMPARE(model.ddcForStream(0), ddcA);
        QCOMPARE(model.ddcForStream(1), ddcB);

        // Defect 1: ReceiverManager::ddcIndex() reports the resolved
        // hardwareRx, which is what feedIqData keys m_hwToLogical on. Before
        // Task 7b receiver 1 fell through rebuildHardwareMapping's
        // nextAutoHw fallback to DDC0 (reserved for the PureSignal /
        // diversity pair) while the codec enabled DDC3.
        QCOMPARE(model.receiverManager()->ddcIndex(0), ddcA);
        QCOMPARE(model.receiverManager()->ddcIndex(1), ddcB);

        // Streams 2..4 are idle: no DDC, and no stale routing left behind.
        QCOMPARE(model.ddcForStream(2), -1);
        QCOMPARE(model.receiverManager()->ddcIndex(2), -1);
    }

    void protocol1_leaves_receiver_routing_auto_assigned()
    {
        RadioModel model;
        // Plain-RX RedPitaya puts stream 0 on DDC2, so a P1 board that
        // wrongly routed by DDC number would look for frame slot 2.
        P1CodecRedPitaya codec;
        model.receiverManager()->setP1Codec(&codec);
        model.configureStreamPool(5, 5, 192000);
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        // The codec's DDC number reaches the slice: that is wire truth, and
        // the P1 C&C bytes really do enable DDC2.
        QCOMPARE(model.ddcForStream(0), 2);
        QCOMPARE(model.slices().at(a)->ddcIndex(), 2);

        // But Protocol 1 packs ACTIVE receivers sequentially into the EP6
        // frame and emits the frame-slot index, not the DDC number
        // (P1RadioConnection.cpp:2999-3007), so routing must stay on
        // rebuildHardwareMapping's sequential auto-assign. Routing by DDC
        // here would drop every EP6 packet (issue #263).
        QCOMPARE(model.receiverManager()->ddcIndex(0), 0);
    }

    void widening_a_stream_rate_admits_a_previously_excluded_slice()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        // 14.400 sits outside +-96 kHz but inside +-384 kHz, so it only
        // fits once the window is widened.
        model.setStreamSampleRate(0, 768000);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14400000.0);

        QCOMPARE(model.slices().at(b)->streamIndex(), 0);
        QCOMPARE(model.activeStreamCount(), 1);
    }

    void narrowing_a_stream_rate_evicts_an_out_of_window_slice()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        model.setStreamSampleRate(0, 768000);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14400000.0);
        QCOMPARE(model.slices().at(b)->streamIndex(), 0);

        // Narrowing to +-96 kHz puts B out of window. It must migrate to
        // its own DDC, not be left silently aliased on a window that no
        // longer contains it.
        model.setStreamSampleRate(0, 192000);

        QVERIFY(model.slices().at(b)->streamIndex() != 0);
        QCOMPARE(model.activeStreamCount(), 2);
    }

    // ── Phase 3F Sub-Epic I closeout, defect F1 ─────────────────────────
    //
    // connectToRadio sizes the pool and binds every slice BEFORE
    // wireConnectionSignals constructs the RxDspWorker, so every bind-time
    // republish hit the `if (m_dspWorker)` guard with a null pointer. The
    // worker then started life knowing only its constructor seed
    // ({stream 0: [slice 0]}) and anything on a non-zero stream demodulated
    // nothing.

    void bindings_reach_a_worker_constructed_after_the_binds()
    {
        constexpr int kInSize = 4;

        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        const int streamB = model.slices().at(b)->streamIndex();
        const int idB     = model.slices().at(b)->sliceIndex();
        QVERIFY(streamB > 0);

        // Production ordering: the worker only exists now, well after every
        // bind above already ran.
        RxDspWorker worker;
        worker.setBufferSizes(kInSize, 64);
        model.attachDspWorkerForTest(&worker);
        model.republishAllStreamBindings();
        // republishStreamBindings posts a queued call; drain it.
        QCoreApplication::processEvents();

        QCOMPARE(workerBindingsFor(worker, streamB, kInSize), QVector<int>{idB});
    }

    // Teardown left every slice's streamIndex set, so connectToRadio's
    // `streamIndex() < 0` bind loop skipped all of them, nothing
    // republished, and the fresh worker was left with the constructor seed
    // alone. A Slice B on stream 1 went silent until it was retuned.

    void reconnect_rebinds_a_slice_that_was_on_a_non_zero_stream()
    {
        constexpr int kInSize = 4;

        RadioModel model;

        // ── connect #1 ──────────────────────────────────────────────────
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        const int idB = model.slices().at(b)->sliceIndex();
        QVERIFY(model.slices().at(b)->streamIndex() > 0);
        QCOMPARE(model.activeStreamCount(), 2);

        RxDspWorker firstWorker;
        firstWorker.setBufferSizes(kInSize, 64);
        model.attachDspWorkerForTest(&firstWorker);
        model.republishAllStreamBindings();
        QCoreApplication::processEvents();

        // ── teardown ────────────────────────────────────────────────────
        // teardownConnection destroys the worker, then releases the
        // bindings so the next connect has something to re-bind.
        model.attachDspWorkerForTest(nullptr);
        model.releaseStreamBindings();

        QCOMPARE(model.slices().at(b)->streamIndex(), -1);
        QCOMPARE(model.slices().at(b)->ddcIndex(), -1);
        QCOMPARE(model.activeStreamCount(), 0);

        // ── connect #2 ──────────────────────────────────────────────────
        model.configureStreamPool(5, 5, 192000);
        model.bindUnboundSlices();

        const int streamB = model.slices().at(b)->streamIndex();
        QVERIFY(streamB > 0);
        QCOMPARE(model.activeStreamCount(), 2);

        RxDspWorker secondWorker;
        secondWorker.setBufferSizes(kInSize, 64);
        model.attachDspWorkerForTest(&secondWorker);
        model.republishAllStreamBindings();
        QCoreApplication::processEvents();

        QCOMPARE(workerBindingsFor(secondWorker, streamB, kInSize),
                 QVector<int>{idB});

        // ...and the constructor seed must not have survived onto a stream
        // that Slice A no longer occupies alone.
        QCOMPARE(workerBindingsFor(secondWorker,
                                   model.slices().at(a)->streamIndex(),
                                   kInSize),
                 QVector<int>{model.slices().at(a)->sliceIndex()});

        model.attachDspWorkerForTest(nullptr);
    }

    // ── Phase 3F Sub-Epic I closeout, defect F3 ─────────────────────────
    //
    // On the 1-ADC HERMES class Thetis collapses to a single synced pair the
    // moment PureSignal transmits, dropping every user receiver:
    //
    //   From Thetis console.cs:8448-8456 [v2.10.3.15]:
    //     else // transmitting and PS is ON
    //     { P1_DDCConfig = 6; DDCEnable = DDC0; SyncEnable = DDC1;
    //       Rate[0] = ps_rate; Rate[1] = ps_rate; cntrl1 = 4; cntrl2 = 0; }
    //
    // with no trailing rx2_enabled clause, unlike the ORION class at
    // console.cs:8299-8303 which keeps RX2 on DDC3 throughout. Thetis's own
    // GetDDC confirms it: the P2 Hermes-class MOX+PS cases are empty, so rx1
    // and rx2 both come back -1 (console.cs:8635-8636 [v2.10.3.15]).
    //
    // So the drop is correct and stays. What must not stay is the silence,
    // and a slice reporting a DDC the radio has stopped streaming.

    void puresignal_on_tx_suspends_the_extra_streams_visibly()
    {
        RadioModel model;
        P2CodecHermes codec;   // ANAN-10 / ANAN-100 / ANAN-G2E on P2
        model.receiverManager()->setP2Codec(&codec);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);
        for (int st = 0; st < 4; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        const int streamB = model.slices().at(b)->streamIndex();
        QVERIFY(streamB > 0);

        // Plain RX: both slices have a real DDC and nothing is suspended.
        QVERIFY(model.slices().at(a)->ddcIndex() >= 0);
        QVERIFY(model.slices().at(b)->ddcIndex() >= 0);
        QVERIFY(model.suspendedStreams().isEmpty());

        QSignalSpy spy(&model, &RadioModel::streamsSuspended);

        // Key MOX with PureSignal running.
        model.setDdcContextForTest(/*mox*/ true, /*puresignalRun*/ true,
                                   /*diversity*/ false);
        model.refreshDdcAssignmentForRadioState();

        // The drop itself: stream B has no DDC any more. Faithful to Thetis.
        QCOMPARE(model.suspendedStreams(), QVector<int>{streamB});

        // The model must say so rather than leaving the operator to notice
        // the audio stopped. Slice B, not stream 1: the operator sees letters.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<QVector<int>>(), QVector<int>{streamB});
        const QString reason = spy.at(0).at(1).toString();
        QVERIFY(!reason.isEmpty());
        QVERIFY(reason.contains(QLatin1String("B")));
        QVERIFY(reason.contains(QLatin1String("PureSignal")));

        // ...and slice B must not still be advertising the DDC it had before
        // the key. That stale number is what made this invisible.
        QCOMPARE(model.slices().at(b)->ddcIndex(), -1);
    }

    void unkeying_restores_the_suspended_streams()
    {
        RadioModel model;
        P2CodecHermes codec;
        model.receiverManager()->setP2Codec(&codec);
        model.configureStreamPool(4, 4, 192000);
        for (int st = 0; st < 4; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);
        const int streamB = model.slices().at(b)->streamIndex();

        model.setDdcContextForTest(true, true, false);
        model.refreshDdcAssignmentForRadioState();
        QCOMPARE(model.suspendedStreams(), QVector<int>{streamB});

        QSignalSpy spy(&model, &RadioModel::streamsSuspended);

        // Unkey.
        model.setDdcContextForTest(false, true, false);
        model.refreshDdcAssignmentForRadioState();

        QVERIFY(model.suspendedStreams().isEmpty());
        QVERIFY(model.slices().at(b)->ddcIndex() >= 0);
        // One transition out, carrying an empty list so the UI can clear the
        // warning instead of leaving it up for its full timeout.
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).value<QVector<int>>().isEmpty());
    }

    // The ORION class keeps its extra receivers through PureSignal
    // (console.cs:8299-8303 [v2.10.3.15]: the `if (rx2_enabled) DDCEnable +=
    // DDC3;` sits OUTSIDE the mox/PS/diversity chain and applies in every
    // case). Nothing may be reported suspended there.
    void orion_class_keeps_its_streams_through_puresignal()
    {
        RadioModel model;
        P2CodecSaturn codec;
        model.receiverManager()->setP2Codec(&codec);
        model.configureStreamPool(5, 5, 192000);
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        model.setDdcContextForTest(true, true, false);
        model.refreshDdcAssignmentForRadioState();

        QVERIFY(model.suspendedStreams().isEmpty());
        QVERIFY(model.slices().at(a)->ddcIndex() >= 0);
        QVERIFY(model.slices().at(b)->ddcIndex() >= 0);
    }
};

QTEST_MAIN(TestStreamPoolBinding)
#include "tst_stream_pool_binding.moc"
