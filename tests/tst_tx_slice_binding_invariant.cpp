// =================================================================
// tests/tst_tx_slice_binding_invariant.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Thetis has no
// N-slice arbiter (it has a fixed VFO A / VFO B transmit model), so the
// behaviour asserted here is governed by
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §6
// rather than by an upstream port.
//
// THE SINGLE-TX INVARIANT, end to end through RadioModel.
//
// TxSliceArbiter::requestHandoff was the only writer of
// SliceModel::txSlice, and it early-returns on a no-op handoff. Since
// requestHandoff(0) took the already-bound arm without establishing state, so a
// session that never explicitly handed the transmitter to another slice
// never raised the flag on any slice at all. isTxSlice() was false for
// every slice, forever, which silently disabled every consumer that asks
// which slice transmits: the TX-bound removal re-home, the codec's
// SliceConfig::txBound, the panadapter TX badge, the VAX TX tags, and the
// transmit-frequency push.
//
// The invariant these tests pin: from the moment a slice exists, exactly
// one slice carries the flag. Never zero, never two, across add, remove
// and handoff.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/TxSliceArbiter.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Number of slices in the model currently flagged TX-bound. The invariant
// is that this is exactly 1 whenever the model holds any slices.
int flaggedCount(const RadioModel& model)
{
    int n = 0;
    for (SliceModel* s : model.slices()) {
        if (s && s->isTxSlice()) { ++n; }
    }
    return n;
}

SliceModel* flaggedSlice(const RadioModel& model)
{
    for (SliceModel* s : model.slices()) {
        if (s && s->isTxSlice()) { return s; }
    }
    return nullptr;
}

} // namespace

class TestTxSliceBindingInvariant : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // The defect in one line: a session that never performs a handoff still
    // has a transmitter, and it has to be bound to something.
    void firstSliceIsTxBoundWithoutAnyHandoff()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = model.addSlice();
        QCOMPARE(model.slices().size(), 1);
        QCOMPARE(flaggedCount(model), 1);
        QVERIFY(model.slices().at(0)->isTxSlice());
        QCOMPARE(model.txSliceArbiter()->txBoundSliceId(), 0);
        QCOMPARE(model.txBoundSlice(), model.slices().at(0));
        Q_UNUSED(a);
    }

    // Adding a second slice must not move the transmitter. The operator
    // moves TX explicitly, through the arbiter, or not at all.
    void addingMoreSlicesDoesNotMoveTheBinding()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        model.addSlice();
        SliceModel* a = model.slices().at(0);
        model.addSlice();
        model.addSlice();

        QCOMPARE(model.slices().size(), 3);
        QCOMPARE(flaggedCount(model), 1);
        QCOMPARE(flaggedSlice(model), a);
    }

    // Handoff moves it, and moves exactly one flag.
    void handoffKeepsExactlyOneSliceFlagged()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        model.addSlice();
        model.addSlice();
        QCOMPARE(flaggedCount(model), 1);

        QVERIFY(model.txSliceArbiter()->requestHandoff(1));
        QCOMPARE(flaggedCount(model), 1);
        QCOMPARE(flaggedSlice(model), model.slices().at(1));
        QCOMPARE(model.txBoundSlice(), model.slices().at(1));
    }

    // Consumer 1: RadioModel::removeSlice hands TX to a survivor before it
    // tears the victim out of the list. That guard reads isTxSlice(), so it
    // never fired while the flag was dead, and removing slice A left the
    // model with no transmitter at all.
    void removingTheTxBoundSliceRehomesTheTransmitter()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.addSlice();
        SliceModel* b = model.slices().at(1);

        QCOMPARE(flaggedSlice(model), model.slices().at(0));
        model.removeSlice(a);

        QCOMPARE(model.slices().size(), 1);
        QCOMPARE(flaggedCount(model), 1);
        QCOMPARE(flaggedSlice(model), b);
        QCOMPARE(model.txBoundSlice(), b);
    }

    // Removing a slice that is NOT the transmitter leaves the transmitter
    // where it was -- and, critically, leaves the arbiter's index and the
    // flag naming the SAME slice. removeSlice does not renumber survivors,
    // so a removal below the bound position shifts it: the index has to be
    // re-derived from the flag, not assumed.
    void removingASliceBelowTheBoundOneKeepsIndexAndFlagAgreeing()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.addSlice();
        model.addSlice();
        SliceModel* c = model.slices().at(2);

        QVERIFY(model.txSliceArbiter()->requestHandoff(c->sliceIndex()));
        QCOMPARE(flaggedSlice(model), c);

        model.removeSlice(a);  // remove a slice BELOW the bound position

        QCOMPARE(model.slices().size(), 2);
        QCOMPARE(flaggedCount(model), 1);
        QCOMPARE(flaggedSlice(model), c);
        // The gate (txBoundSlice) and the flag (isTxSlice) must name one
        // slice. If they diverge, a retune of C stops pushing the transmit
        // frequency while C still claims to be the transmitter -- which is
        // how the Alex TX low-pass ends up on the wrong band.
        QCOMPARE(model.txBoundSlice(), c);
    }

    void directArbiterHandoffUsesStableIdAfterADeletionGap()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        SliceModel* a = model.sliceById(model.addSlice());
        const int bId = model.addSlice();
        SliceModel* c = model.sliceById(model.addSlice());
        QVERIFY(a);
        QVERIFY(c);
        QCOMPARE(c->sliceIndex(), 2);

        model.removeSlice(bId);
        QCOMPARE(model.slices().indexOf(c), 1);

        QVERIFY(model.txSliceArbiter()->requestHandoff(c->sliceIndex()));
        QCOMPARE(model.txBoundSlice(), c);
        QCOMPARE(flaggedSlice(model), c);
    }

    // Design §6 "Restore on launch": if the persisted slice does not exist
    // this session, default to Slice A. What must never happen is the
    // restore leaving nothing bound.
    void persistedIdNamingAMissingSliceLeavesSliceABound()
    {
        const QString mac = QStringLiteral("11:22:33:44:55:66");
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/TxBoundSliceId").arg(mac), 4);

        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.addSlice();
        model.addSlice();

        model.txSliceArbiter()->setMacAddress(mac);
        model.txSliceArbiter()->load();

        QCOMPARE(flaggedCount(model), 1);
        QCOMPARE(flaggedSlice(model), model.slices().at(0));
        QCOMPARE(model.txSliceArbiter()->txBoundSliceId(), 0);
    }

    // A persisted index that DOES name a live slice is honoured.
    void persistedIdNamingALiveSliceIsRestored()
    {
        const QString mac = QStringLiteral("11:22:33:44:55:77");
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/TxBoundSliceId").arg(mac), 1);

        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.addSlice();
        model.addSlice();

        model.txSliceArbiter()->setMacAddress(mac);
        model.txSliceArbiter()->load();

        QCOMPARE(flaggedCount(model), 1);
        QCOMPARE(flaggedSlice(model), model.slices().at(1));
        QCOMPARE(model.txBoundSlice(), model.slices().at(1));
    }

    // Consumer 2: SliceConfig::txBound is the codec's input for "this DDC
    // stream carries the transmitter". It is the OR across the slices
    // sharing the stream, and it was false for every stream because the
    // flag it ORs was false for every slice.
    void txBoundReachesTheCodecConfigForTheBoundStream()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        model.addSlice();
        model.slices().at(0)->setFrequency(14225000.0);
        model.addSlice();
        model.slices().at(1)->setFrequency(3700000.0);  // far enough for its own DDC

        const int streamA = model.slices().at(0)->streamIndex();
        const int streamB = model.slices().at(1)->streamIndex();
        QVERIFY(streamA >= 0);
        QVERIFY(streamB >= 0);
        QVERIFY(streamA != streamB);

        auto cfgs = model.buildStreamConfigsForCodecForTest();
        QVERIFY(cfgs.at(streamA).live);
        QVERIFY2(cfgs.at(streamA).txBound,
                 "the stream carrying the TX-bound slice reached the codec "
                 "with txBound false");
        QVERIFY(!cfgs.at(streamB).txBound);

        // ...and it follows a handoff.
        QVERIFY(model.txSliceArbiter()->requestHandoff(1));
        cfgs = model.buildStreamConfigsForCodecForTest();
        QVERIFY(!cfgs.at(streamA).txBound);
        QVERIFY(cfgs.at(streamB).txBound);
    }
};

QTEST_MAIN(TestTxSliceBindingInvariant)
#include "tst_tx_slice_binding_invariant.moc"
