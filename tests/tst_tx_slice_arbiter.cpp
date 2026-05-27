// =================================================================
// tests/tst_tx_slice_arbiter.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic C: TxSliceArbiter single-TX invariant.
// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §6.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QVector>
#include "core/TxSliceArbiter.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestTxSliceArbiter : public QObject {
    Q_OBJECT
private slots:
    void default_tx_bound_slice_index_is_0()
    {
        TxSliceArbiter arb;
        QCOMPARE(arb.txBoundSliceIndex(), 0);
    }

    void handoff_to_already_bound_slice_is_noop_returns_true()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);

        QSignalSpy spy(&arb, &TxSliceArbiter::txBoundSliceChanged);
        const bool ok = arb.requestHandoff(0);
        QCOMPARE(ok, true);
        QCOMPARE(spy.count(), 0);
    }

    void handoff_to_nonexistent_slice_returns_false_emits_blocked()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);

        QSignalSpy blocked(&arb, &TxSliceArbiter::handoffBlocked);
        const bool ok = arb.requestHandoff(5);
        QCOMPARE(ok, false);
        QCOMPARE(blocked.count(), 1);
    }

private:
    // Build a list of N SliceModel instances for testing. Each slice is parented
    // to `this` for automatic cleanup. Slice 0 is marked TX-bound to mirror the
    // RadioModel default state.
    void buildSlices(QVector<SliceModel*>& outSlices, int n)
    {
        for (int i = 0; i < n; ++i) {
            auto* s = new SliceModel(this);
            outSlices.append(s);
        }
        if (!outSlices.isEmpty()) {
            outSlices[0]->setTxSlice(true);
        }
    }
};

QTEST_MAIN(TestTxSliceArbiter)
#include "tst_tx_slice_arbiter.moc"
