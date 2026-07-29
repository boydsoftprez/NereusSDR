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
#include "core/AppSettings.h"
#include "core/TxSliceArbiter.h"
#include "core/MoxController.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestTxSliceArbiter : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    void default_tx_bound_slice_id_is_unbound()
    {
        TxSliceArbiter arb;
        QCOMPARE(arb.txBoundSliceId(), -1);
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
        QCOMPARE(arb.txBoundSliceId(), 1);
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

    void tx_bound_id_persists_per_mac()
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
            QCOMPARE(arb2.txBoundSliceId(), 2);
        }
    }

    void stable_id_handoff_selects_the_matching_slice_and_emits_its_id()
    {
        QVector<SliceModel*> slices;
        buildSlicesWithIds(slices, {0, 4, 9});
        TxSliceArbiter arb;
        arb.setSliceList(&slices);

        QSignalSpy spy(&arb, &TxSliceArbiter::txBoundSliceChanged);
        QVERIFY(arb.requestHandoff(9));

        QCOMPARE(arb.txBoundSlice(), slices[2]);
        QCOMPARE(slices[2]->isTxSlice(), true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 0);
        QCOMPARE(spy.first().at(1).toInt(), 9);
    }

    void removing_an_unbound_slice_does_not_change_the_bound_stable_id()
    {
        QVector<SliceModel*> slices;
        buildSlicesWithIds(slices, {0, 4, 9});
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        QVERIFY(arb.requestHandoff(9));

        SliceModel* bound = slices[2];
        slices.removeAt(1);
        arb.syncToSliceList();

        QCOMPARE(arb.txBoundSlice(), bound);
        QCOMPARE(bound->sliceIndex(), 9);
        QCOMPARE(bound->isTxSlice(), true);
    }

    void stable_id_key_restores_the_matching_slice()
    {
        const QString mac = QStringLiteral("aa:bb:cc:44:99:00");
        const QString key = QStringLiteral("hardware/%1/TxBoundSliceId").arg(mac);
        AppSettings::instance().setValue(key, 9);

        QVector<SliceModel*> slices;
        buildSlicesWithIds(slices, {0, 4, 9}, /*flagFirst*/ false);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMacAddress(mac);
        arb.load();

        QCOMPARE(arb.txBoundSlice(), slices[2]);
        QCOMPARE(slices[2]->isTxSlice(), true);
    }

    void legacy_position_is_migrated_once_to_a_stable_id()
    {
        const QString mac = QStringLiteral("aa:bb:cc:44:99:01");
        const QString legacyKey =
            QStringLiteral("hardware/%1/TxBoundSliceIndex").arg(mac);
        const QString idKey =
            QStringLiteral("hardware/%1/TxBoundSliceId").arg(mac);
        AppSettings::instance().setValue(legacyKey, 2);

        QVector<SliceModel*> slices;
        buildSlicesWithIds(slices, {0, 4, 9}, /*flagFirst*/ false);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMacAddress(mac);
        arb.load();

        QCOMPARE(arb.txBoundSlice(), slices[2]);
        QCOMPARE(AppSettings::instance().value(idKey, -1).toInt(), 9);

        AppSettings::instance().setValue(legacyKey, 0);
        for (SliceModel* slice : slices) {
            slice->setTxSlice(false);
        }
        arb.load();
        QCOMPARE(arb.txBoundSlice(), slices[2]);
    }

    // ── The initial binding ────────────────────────────────────────────
    //
    // requestHandoff is the only writer of SliceModel::txSlice and it
    // early-returns on a no-op, so without an explicit sync a
    // session that never hands TX to another slice never raised the flag on
    // anything. syncToSliceList is the arm that establishes the binding the
    // first time a slice exists, without requiring an operator handoff.
    void sync_binds_slice_zero_when_nothing_is_flagged()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2, /*flagFirst*/ false);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);

        QSignalSpy spy(&arb, &TxSliceArbiter::txBoundSliceChanged);
        arb.syncToSliceList();

        QCOMPARE(slices[0]->isTxSlice(), true);
        QCOMPARE(slices[1]->isTxSlice(), false);
        QCOMPARE(arb.txBoundSliceId(), 0);
        QCOMPARE(arb.txBoundSlice(), slices[0]);
        // oldIndex is -1: there was no previous binding to hand off from.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), -1);
        QCOMPARE(spy.first().at(1).toInt(), 0);
    }

    // An existing binding is left alone. In particular the initial-bind arm
    // must not fire on every later add, or adding slice B would silently
    // yank the transmitter back to slice A.
    void sync_is_a_noop_when_a_binding_already_exists()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 3, /*flagFirst*/ false);
        slices[1]->setTxSlice(true);

        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        QVERIFY(arb.requestHandoff(1));

        QSignalSpy spy(&arb, &TxSliceArbiter::txBoundSliceChanged);
        arb.syncToSliceList();

        QCOMPARE(spy.count(), 0);
        QCOMPARE(arb.txBoundSliceId(), 1);
        QCOMPARE(slices[1]->isTxSlice(), true);
    }

    // RF-SAFETY: the initial bind arm is the only one that can raise a flag
    // outside requestHandoff, so it carries the same MOX-drop guard. It can
    // only ever fire with nothing bound, which on the true first bind means
    // nothing could have been keyed either -- but a keyed transmitter with
    // no bound slice is exactly the state you do not want to flip a binding
    // underneath, so the guard stays.
    void sync_drops_mox_before_an_initial_bind()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2, /*flagFirst*/ false);

        MoxController mox;
        mox.setMox(true);

        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);

        arb.syncToSliceList();

        QCOMPARE(mox.isMox(), false);
        QCOMPARE(slices[0]->isTxSlice(), true);
    }

    // ...and conversely, a sync that has nothing to bind must not unkey the
    // operator. Adding a slice mid-transmission is not a reason to drop RF.
    void sync_does_not_drop_mox_when_a_binding_exists()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);  // slice 0 flagged

        MoxController mox;
        mox.setMox(true);

        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);

        arb.syncToSliceList();

        QCOMPARE(mox.isMox(), true);
    }

    // The flag lives on the SliceModel, so it survives a list mutation that
    // moves the object. The arbiter keeps the object's stable ID, independent
    // of its new list position.
    void sync_keeps_the_flagged_slices_stable_id()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 3, /*flagFirst*/ false);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        QVERIFY(arb.requestHandoff(2));
        QCOMPARE(arb.txBoundSliceId(), 2);

        SliceModel* bound = slices[2];
        slices.removeAt(0);  // everything above shifts down one

        QSignalSpy spy(&arb, &TxSliceArbiter::txBoundSliceChanged);
        arb.syncToSliceList();

        QCOMPARE(arb.txBoundSliceId(), 2);
        QCOMPARE(arb.txBoundSlice(), bound);
        QCOMPARE(bound->isTxSlice(), true);
        // The transmitter did not move -- only its position in the list did.
        // Announcing a handoff that did not happen would tell every
        // subscriber to re-badge and re-push for nothing.
        QCOMPARE(spy.count(), 0);
    }

    // Never two. Nothing outside the arbiter writes the flag today, so this
    // is a guard against a future second writer rather than a live path.
    void sync_normalises_two_flagged_slices_to_one()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 3);  // slice 0 flagged
        slices[2]->setTxSlice(true);

        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.syncToSliceList();

        int flagged = 0;
        for (SliceModel* s : slices) { if (s->isTxSlice()) { ++flagged; } }
        QCOMPARE(flagged, 1);
        QCOMPARE(slices[0]->isTxSlice(), true);  // the one the index named
        QCOMPARE(arb.txBoundSliceId(), 0);
    }

    // Design §6 "Restore on launch": a persisted ID naming a slice that
    // does not exist this session falls back to Slice A. What it must not do
    // is leave the transmitter unbound.
    void load_with_missing_id_still_leaves_one_slice_bound()
    {
        const QString mac = QStringLiteral("de:ad:be:ef:00:01");
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/TxBoundSliceId").arg(mac), 7);

        QVector<SliceModel*> slices;
        buildSlices(slices, 2, /*flagFirst*/ false);

        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMacAddress(mac);
        arb.load();

        QCOMPARE(slices[0]->isTxSlice(), true);
        QCOMPARE(slices[1]->isTxSlice(), false);
        QCOMPARE(arb.txBoundSliceId(), 0);
    }

    // txBoundSlice resolves the same POSITION requestHandoff writes, so
    // `arb.txBoundSlice() == s` and `s->isTxSlice()` are one predicate.
    void tx_bound_slice_resolves_the_flagged_slice()
    {
        QVector<SliceModel*> slices;
        TxSliceArbiter arb;
        QCOMPARE(arb.txBoundSlice(), nullptr);  // no list wired yet

        buildSlices(slices, 2, /*flagFirst*/ false);
        arb.setSliceList(&slices);
        arb.syncToSliceList();
        QVERIFY(arb.requestHandoff(1));

        QCOMPARE(arb.txBoundSlice(), slices[1]);
        QCOMPARE(arb.txBoundSlice()->isTxSlice(), true);
    }

private:
    // Build a list of N SliceModel instances for testing. Each slice is parented
    // to `this` for automatic cleanup. Slice 0 is marked TX-bound to mirror the
    // RadioModel default state; pass flagFirst = false to build a list with no
    // binding at all, which is what the arbiter sees before its first sync.
    void buildSlices(QVector<SliceModel*>& outSlices, int n, bool flagFirst = true)
    {
        for (int i = 0; i < n; ++i) {
            auto* s = new SliceModel(this);
            s->setSliceIndex(i);
            outSlices.append(s);
        }
        if (flagFirst && !outSlices.isEmpty()) {
            outSlices[0]->setTxSlice(true);
        }
    }

    void buildSlicesWithIds(QVector<SliceModel*>& outSlices,
                            std::initializer_list<int> ids,
                            bool flagFirst = true)
    {
        for (const int id : ids) {
            auto* slice = new SliceModel(this);
            slice->setSliceIndex(id);
            outSlices.append(slice);
        }
        if (flagFirst && !outSlices.isEmpty()) {
            outSlices[0]->setTxSlice(true);
        }
    }
};

QTEST_MAIN(TestTxSliceArbiter)
#include "tst_tx_slice_arbiter.moc"
