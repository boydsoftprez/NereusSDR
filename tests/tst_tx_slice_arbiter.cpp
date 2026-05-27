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
#include "core/MoxController.h"
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

    void handoff_to_different_slice_flips_tx_flags_and_emits()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);

        QSignalSpy spy(&arb, &TxSliceArbiter::txBoundSliceChanged);

        QCOMPARE(slices[0]->isTxSlice(), true);
        QCOMPARE(slices[1]->isTxSlice(), false);

        const bool ok = arb.requestHandoff(1);
        QCOMPARE(ok, true);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 0);  // oldIndex
        QCOMPARE(spy.first().at(1).toInt(), 1);  // newIndex

        QCOMPARE(slices[0]->isTxSlice(), false);
        QCOMPARE(slices[1]->isTxSlice(), true);
        QCOMPARE(arb.txBoundSliceIndex(), 1);
    }

    void handoff_drops_mox_first_then_flips()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);

        MoxController mox;
        mox.setMox(true);  // simulate keyed

        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);

        QCOMPARE(mox.isMox(), true);

        // hardwareFlipped(false) fires synchronously inside setMox(false), before
        // the TX-to-RX timer chain (mox_delay + ptt_out_delay). moxChanged is the
        // async Post signal at the end of the chain; we only need synchronous
        // evidence that MOX was dropped before the flip.
        QSignalSpy hwSpy(&mox, &MoxController::hardwareFlipped);
        const bool ok = arb.requestHandoff(1);

        QCOMPARE(ok, true);
        QVERIFY(hwSpy.count() >= 1);
        QCOMPARE(hwSpy.last().at(0).toBool(), false);  // last hardwareFlipped was RX direction
        QCOMPARE(mox.isMox(), false);  // MOX dropped synchronously (m_mox = on commit)
        QCOMPARE(slices[1]->isTxSlice(), true);  // handoff completed
    }

    void tx_bound_index_persists_per_mac()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:ff");
        QVector<SliceModel*> slices;
        buildSlices(slices, 3);

        {
            TxSliceArbiter arb;
            arb.setSliceList(&slices);
            arb.setMacAddress(mac);
            arb.requestHandoff(2);
            arb.save();
        }
        // Reset slice flags so reconstruction is meaningful
        for (auto* s : slices) { s->setTxSlice(false); }
        slices[0]->setTxSlice(true);

        {
            TxSliceArbiter arb2;
            arb2.setSliceList(&slices);
            arb2.setMacAddress(mac);
            arb2.load();
            QCOMPARE(arb2.txBoundSliceIndex(), 2);
        }
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
