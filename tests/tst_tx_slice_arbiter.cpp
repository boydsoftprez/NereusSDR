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
};

QTEST_MAIN(TestTxSliceArbiter)
#include "tst_tx_slice_arbiter.moc"
