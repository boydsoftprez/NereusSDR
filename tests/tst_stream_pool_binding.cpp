// =================================================================
// tests/tst_stream_pool_binding.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic I Tasks 5-6: stream pool + slice binding.
// Phase 3F Sub-Epic I Task 7b: per-stream DDC assignment + routing.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/ReceiverManager.h"
#include "core/codec/P1CodecRedPitaya.h"
#include "core/codec/P2CodecSaturn.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

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
};

QTEST_MAIN(TestStreamPoolBinding)
#include "tst_stream_pool_binding.moc"
