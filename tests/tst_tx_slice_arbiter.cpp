// =================================================================
// tests/tst_tx_slice_arbiter.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic C: TxSliceArbiter single-TX invariant.
// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §6.
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control plan Task 2: the
//                                    arbiter's owner lookup is a transmit
//                                    access check (setTransmitAccess).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control fix wave: releasing
//                                    the binding while keyed unkeys
//                                    through the unkey gate first.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QVector>
#include <QScopeGuard>
#include "core/AppSettings.h"
#include "core/TxSliceArbiter.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/safety/UnkeyGate.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

// iPhone app plan Task 34: every MOX and TX-frequency write, in order.
class HandoffLogConnection : public RadioConnection {
    Q_OBJECT
public:
    QStringList log;

    explicit HandoffLogConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64 hz) override { log.append(QStringLiteral("tx %1").arg(hz)); }
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool on) override { log.append(on ? QStringLiteral("MOX on") : QStringLiteral("MOX off")); }
    void setAntennaRouting(AntennaRouting) override {}
    void setAlexRxBpf(AlexRxBpf) override {}
    void setWatchdogEnabled(bool enabled) override { m_watchdogEnabled = enabled; }
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

        // txAboutToEnd fires synchronously inside setMox(false): the TX-to-RX
        // walk began before the flip. Task 33 (Thetis's unkey order): the
        // hardware is released after the TX drain and mox_delay, so
        // hardwareFlipped(false) follows on the walk's timer.
        QSignalSpy endSpy(&mox, &MoxController::txAboutToEnd);
        QSignalSpy hwSpy(&mox, &MoxController::hardwareFlipped);
        const bool ok = arb.requestHandoff(1);

        QCOMPARE(ok, true);
        QCOMPARE(endSpy.count(), 1);
        QCOMPARE(mox.isMox(), false);  // MOX dropped synchronously (m_mox = on commit)
        QCOMPARE(slices[1]->isTxSlice(), true);  // handoff completed
        QTRY_VERIFY_WITH_TIMEOUT(hwSpy.count() >= 1, 2000);
        QCOMPARE(hwSpy.last().at(0).toBool(), false);  // last hardwareFlipped was RX direction
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

    // ── iPhone app plan Task 34 (R-IOS-03): the handoff waits for the unkey ──

    void a_keyed_handoff_moves_the_flag_only_after_the_unkey_is_confirmed()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox.setMox(true);
        QTRY_COMPARE(mox.state(), MoxState::Tx);
        QList<QPair<int, std::function<void()>>> timers;
        UnkeyGate gate(&mox, [&mox]() { mox.setMox(false); }, [](const QString&) {});
        gate.setScheduler([&timers](int ms, QObject*, std::function<void()> fire) {
            timers.append({ms, std::move(fire)});
        });
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);
        arb.setUnkeyGate(&gate);
        QSignalSpy moved(&arb, &TxSliceArbiter::txBoundSliceChanged);

        QVERIFY(arb.requestHandoff(1));
        QVERIFY(!mox.isMox());                 // the unkey began at once
        QVERIFY(arb.isHandoffPending());
        QVERIFY(slices[0]->isTxSlice());      // the flag waits
        QCOMPARE(moved.count(), 0);
        QTRY_COMPARE(moved.count(), 1);        // Confirmed: MOX reached receive
        QCOMPARE(mox.state(), MoxState::Rx);
        QVERIFY(slices[1]->isTxSlice());
        QVERIFY(!slices[0]->isTxSlice());
        QVERIFY(!arb.isHandoffPending());
    }

    void a_keyed_handoff_that_times_out_moves_after_the_stop()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox.setMox(true);
        QTRY_COMPARE(mox.state(), MoxState::Tx);
        QStringList order;
        QList<QPair<int, std::function<void()>>> timers;
        UnkeyGate gate(&mox, []() { /* the radio never reaches receive */ },
                       [&order](const QString&) { order.append(QStringLiteral("stop")); });
        gate.setScheduler([&timers](int ms, QObject*, std::function<void()> fire) {
            timers.append({ms, std::move(fire)});
        });
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);
        arb.setUnkeyGate(&gate);
        connect(&arb, &TxSliceArbiter::txBoundSliceChanged, this,
                [&order](int, int) { order.append(QStringLiteral("moved")); });
        QVERIFY(arb.requestHandoff(1));
        QCOMPARE(timers.size(), 1);
        QCOMPARE(timers.first().first, 2000);
        QVERIFY(order.isEmpty());
        timers.first().second();
        QCOMPARE(order, QStringList({QStringLiteral("stop"), QStringLiteral("moved")}));
    }

    void a_second_request_while_waiting_takes_the_latest_target()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 3);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox.setMox(true);
        QTRY_COMPARE(mox.state(), MoxState::Tx);
        UnkeyGate gate(&mox, [&mox]() { mox.setMox(false); }, [](const QString&) {});
        gate.setScheduler([](int, QObject*, std::function<void()>) {});
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);
        arb.setUnkeyGate(&gate);
        QVERIFY(arb.requestHandoff(1));
        QVERIFY(arb.requestHandoff(2));
        QTRY_VERIFY(slices[2]->isTxSlice());
        QVERIFY(!slices[1]->isTxSlice());
        QCOMPARE(arb.txBoundSliceId(), 2);
    }

    // Slice control fix wave (Critical 1): a move that waits for the unkey
    // is checked again when the gate answers. A slice another device took
    // meanwhile never receives the flag.
    void a_waiting_move_to_a_slice_that_changed_hands_does_not_land()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox.setMox(true);
        QTRY_COMPARE(mox.state(), MoxState::Tx);
        QList<std::function<void()>> bounds;
        UnkeyGate gate(&mox, []() { /* the radio never reaches receive */ },
                       [](const QString&) {});
        gate.setScheduler([&bounds](int, QObject*, std::function<void()> fire) {
            bounds.append(std::move(fire));
        });
        QHash<int, QByteArray> controller{{0, QByteArrayLiteral("A")},
                                          {1, QByteArrayLiteral("A")}};
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);
        arb.setUnkeyGate(&gate);
        arb.setTransmitAccess(
            [&controller](const QByteArray& device, int sliceId) {
                return controller.value(sliceId) == device;
            },
            [](const QByteArray&) { return 0; });
        arb.setHolderLookup([]() { return QByteArrayLiteral("A"); });
        QSignalSpy moved(&arb, &TxSliceArbiter::txBoundSliceChanged);
        QSignalSpy blocked(&arb, &TxSliceArbiter::handoffBlocked);
        QSignalSpy pending(&arb, &TxSliceArbiter::pendingHandoffChanged);

        QVERIFY(arb.requestHandoff(1, QByteArrayLiteral("A")));
        QCOMPARE(arb.pendingHandoffSliceId(), 1);
        QCOMPARE(pending.count(), 1);
        QCOMPARE(pending.last().at(0).toInt(), 1);
        controller.insert(1, QByteArrayLiteral("B"));   // taken while the key ended
        QCOMPARE(bounds.size(), 1);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unkey was not confirmed")));
        bounds.first()();                                // the gate answers

        QCOMPARE(moved.count(), 0);
        QVERIFY(slices[0]->isTxSlice());
        QVERIFY(!slices[1]->isTxSlice());
        QCOMPARE(arb.pendingHandoffSliceId(), -1);
        QCOMPARE(blocked.count(), 1);
        QCOMPARE(blocked.first().at(0).toInt(), 1);
        QCOMPARE(blocked.first().at(1).toString(),
                 QStringLiteral("Another device controls that slice now, so the transmit slice "
                                "did not move."));
        QCOMPARE(pending.count(), 2);
        QCOMPARE(pending.last().at(0).toInt(), -1);
    }

    // Slice control fix wave, round 2: transmit, not the slice, changed
    // hands while the move waited. The move is dropped and the reason says
    // what changed.
    void a_waiting_move_dropped_because_transmit_passed_says_so()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox.setMox(true);
        QTRY_COMPARE(mox.state(), MoxState::Tx);
        QList<std::function<void()>> bounds;
        UnkeyGate gate(&mox, []() {}, [](const QString&) {});
        gate.setScheduler([&bounds](int, QObject*, std::function<void()> fire) {
            bounds.append(std::move(fire));
        });
        const QHash<int, QByteArray> controller{{0, QByteArrayLiteral("A")},
                                                {1, QByteArrayLiteral("A")}};
        QByteArray holder = QByteArrayLiteral("A");
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);
        arb.setUnkeyGate(&gate);
        arb.setTransmitAccess(
            [&controller](const QByteArray& device, int sliceId) {
                return controller.value(sliceId) == device;
            },
            [](const QByteArray&) { return 0; });
        arb.setHolderLookup([&holder]() { return holder; });
        QSignalSpy blocked(&arb, &TxSliceArbiter::handoffBlocked);

        QVERIFY(arb.requestHandoff(1, QByteArrayLiteral("A")));
        holder = QByteArrayLiteral("B");                 // transmit passed while the key ended
        QCOMPARE(bounds.size(), 1);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unkey was not confirmed")));
        bounds.first()();

        QVERIFY(slices[0]->isTxSlice());
        QVERIFY(!slices[1]->isTxSlice());
        QCOMPARE(blocked.count(), 1);
        QCOMPARE(blocked.first().at(1).toString(),
                 QStringLiteral("Transmit passed to another device, so the transmit slice did "
                                "not move."));
    }

    // The holder at the answer counts too: the local window's move (no
    // requester) lands only on a slice the holder may transmit on.
    void a_waiting_move_lands_only_on_a_slice_the_holder_may_transmit_on()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 2);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox.setMox(true);
        QTRY_COMPARE(mox.state(), MoxState::Tx);
        QList<std::function<void()>> bounds;
        UnkeyGate gate(&mox, []() {}, [](const QString&) {});
        gate.setScheduler([&bounds](int, QObject*, std::function<void()> fire) {
            bounds.append(std::move(fire));
        });
        QHash<int, QByteArray> controller{{0, QByteArrayLiteral("S")},
                                          {1, QByteArrayLiteral("S")}};
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);
        arb.setUnkeyGate(&gate);
        arb.setTransmitAccess(
            [&controller](const QByteArray& device, int sliceId) {
                return controller.value(sliceId) == device;
            },
            [](const QByteArray&) { return 0; });
        arb.setHolderLookup([]() { return QByteArrayLiteral("S"); });

        QVERIFY(arb.requestHandoff(1));
        controller.insert(1, QByteArrayLiteral("B"));
        QCOMPARE(bounds.size(), 1);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unkey was not confirmed")));
        bounds.first()();
        QVERIFY(slices[0]->isTxSlice());
        QVERIFY(!slices[1]->isTxSlice());

        // Still its own: the move lands as before.
        controller.insert(1, QByteArrayLiteral("S"));
        QVERIFY(arb.requestHandoff(1));
        QCOMPARE(bounds.size(), 2);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unkey was not confirmed")));
        bounds.last()();
        QVERIFY(slices[1]->isTxSlice());
        QVERIFY(!slices[0]->isTxSlice());
    }

    // Carried from Task 33: a local handoff while keyed put the new slice's
    // frequency on the wire before MOX off. The new slice's frequency now
    // reaches the connection only after MOX off.
    void a_keyed_handoff_sends_no_new_frequency_before_mox_off()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* conn = new HandoffLogConnection();
        model.injectConnectionForTest(conn);
        auto detach = qScopeGuard([&model, conn]() {
            model.injectConnectionForTest(nullptr);
            delete conn;
        });
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(28400000.0);
        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(3700000.0);
        model.wireSliceSignalsForTest();
        QCOMPARE(model.txSliceArbiter()->txBoundSliceId(), a);

        model.moxController()->setMox(true);
        QTRY_COMPARE(model.moxController()->state(), MoxState::Tx);
        QTRY_VERIFY(conn->log.contains(QStringLiteral("MOX on")));
        conn->log.clear();

        QVERIFY(model.txSliceArbiter()->requestHandoff(b));
        QTRY_VERIFY(conn->log.contains(QStringLiteral("tx 3700000")));
        const int moxOff = conn->log.indexOf(QStringLiteral("MOX off"));
        const int newFreq = conn->log.indexOf(QStringLiteral("tx 3700000"));
        QVERIFY2(moxOff >= 0 && moxOff < newFreq, qPrintable(conn->log.join(QStringLiteral(", "))));
        QCOMPARE(model.txSliceArbiter()->txBoundSliceId(), b);
    }

    // ---- iPhone app plan Task 77 (rulings 8.10 to 8.13) ------------------

    // Refusals first: tx.setTxSlice names only the requester's own slices,
    // and the flag never moves while the station device is keyed.
    void handoff_for_a_requester_refuses_another_owners_slice()
    {
        QVector<SliceModel*> slices;
        buildSlicesWithIds(slices, {0, 1, 2});
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.syncToSliceList();
        const QHash<int, QByteArray> owners{{0, "phone"}, {1, "pad"}, {2, "pad"}};
        arb.setTransmitAccess(
            [owners](const QByteArray& device, int id) { return owners.value(id) == device; },
            [](const QByteArray&) { return -1; });
        QSignalSpy blocked(&arb, &TxSliceArbiter::handoffBlocked);
        QVERIFY(!arb.requestHandoff(1, QByteArrayLiteral("phone")));
        QCOMPARE(blocked.count(), 1);
        QCOMPARE(arb.txBoundSliceId(), 0);
        QVERIFY(arb.requestHandoff(2, QByteArrayLiteral("pad")));
        QCOMPARE(arb.txBoundSliceId(), 2);
    }

    void the_flag_never_moves_while_frozen()
    {
        QVector<SliceModel*> slices;
        buildSlicesWithIds(slices, {0, 1});
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.syncToSliceList();
        bool frozen = true;
        arb.setFrozen([&frozen]() { return frozen; });
        QVERIFY(!arb.requestHandoff(1));
        QVERIFY(!arb.bindForHolder(QByteArrayLiteral("pad"), 1));
        QCOMPARE(arb.txBoundSliceId(), 0);
        QVERIFY(slices[0]->isTxSlice());
        frozen = false;   // the press ended
        QVERIFY(arb.requestHandoff(1));
        QCOMPARE(arb.txBoundSliceId(), 1);
    }

    void bind_for_holder_takes_its_chosen_slice_else_its_active_one()
    {
        QVector<SliceModel*> slices;
        buildSlicesWithIds(slices, {0, 1, 2, 3});
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.syncToSliceList();
        const QHash<int, QByteArray> owners{{0, "phone"}, {1, "pad"}, {2, "pad"}, {3, "pad"}};
        arb.setTransmitAccess(
            [owners](const QByteArray& device, int id) { return owners.value(id) == device; },
            [](const QByteArray& owner) { return owner == "pad" ? 3 : 0; });
        // Its chosen transmit slice, still its own.
        QVERIFY(arb.bindForHolder(QByteArrayLiteral("pad"), 2));
        QCOMPARE(arb.txBoundSliceId(), 2);
        // A choice that is not its own any more: its active slice.
        QVERIFY(arb.bindForHolder(QByteArrayLiteral("pad"), 0));
        QCOMPARE(arb.txBoundSliceId(), 3);
        // A holder that owns no slice: nothing moves.
        QVERIFY(!arb.bindForHolder(QByteArrayLiteral("mac"), -1));
        QCOMPARE(arb.txBoundSliceId(), 3);
    }

    void the_first_bind_picks_among_the_holders_slices()
    {
        QVector<SliceModel*> slices;
        buildSlicesWithIds(slices, {0, 1, 2}, /*flagFirst=*/false);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        const QHash<int, QByteArray> owners{{0, "phone"}, {1, "pad"}, {2, "pad"}};
        arb.setTransmitAccess(
            [owners](const QByteArray& device, int id) { return owners.value(id) == device; },
            [](const QByteArray& owner) { return owner == "pad" ? 2 : 0; });
        arb.setHolderLookup([]() { return QByteArrayLiteral("pad"); });
        arb.syncToSliceList();
        QCOMPARE(arb.txBoundSliceId(), 2);
        QVERIFY(slices[2]->isTxSlice());
        QVERIFY(!slices[0]->isTxSlice());
    }

    void a_remote_windows_arbiter_still_does_nothing()
    {
        QVector<SliceModel*> slices;
        buildSlicesWithIds(slices, {0, 1});
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setRemote(true);
        arb.setTransmitAccess([](const QByteArray& device, int) { return device == "pad"; },
                              [](const QByteArray&) { return 1; });
        QVERIFY(!arb.bindForHolder(QByteArrayLiteral("pad"), 1));
        QVERIFY(!arb.requestHandoff(1, QByteArrayLiteral("pad")));
        QVERIFY(slices[0]->isTxSlice());
    }

    // Slice control fix wave (whole-branch review, Critical 1): the last
    // slice closes while the radio is keyed. The binding ends only after
    // the unkey gate saw receive, never with the radio still keyed.
    void releasing_the_binding_while_keyed_waits_for_the_unkey()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 1);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);
        QVERIFY(arb.requestHandoff(0));
        QCOMPARE(arb.txBoundSliceId(), 0);
        mox.setMox(true);
        QTRY_COMPARE(mox.state(), MoxState::Tx);
        QList<std::function<void()>> bounds;
        UnkeyGate gate(&mox, [&mox]() { mox.setMox(false); }, [](const QString&) {});
        gate.setScheduler([&bounds](int, QObject*, std::function<void()> fire) {
            bounds.append(std::move(fire));
        });
        arb.setUnkeyGate(&gate);
        QList<MoxState> stateAtRelease;
        connect(&arb, &TxSliceArbiter::txBoundSliceChanged, this,
                [&stateAtRelease, &mox](int, int now) {
                    if (now == -1) { stateAtRelease.append(mox.state()); }
                });

        slices.clear();                       // the last slice closed
        arb.releaseBinding();
        QVERIFY(!mox.isMox());                // the unkey began at once
        QTRY_COMPARE(stateAtRelease.size(), 1);
        QCOMPARE(stateAtRelease.first(), MoxState::Rx);
        QCOMPARE(arb.txBoundSliceId(), -1);
    }

    void releasing_the_binding_while_keyed_without_a_gate_unkeys_first()
    {
        QVector<SliceModel*> slices;
        buildSlices(slices, 1);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        TxSliceArbiter arb;
        arb.setSliceList(&slices);
        arb.setMoxController(&mox);
        QVERIFY(arb.requestHandoff(0));
        mox.setMox(true);
        QTRY_COMPARE(mox.state(), MoxState::Tx);
        QList<bool> moxAtRelease;
        connect(&arb, &TxSliceArbiter::txBoundSliceChanged, this,
                [&moxAtRelease, &mox](int, int now) {
                    if (now == -1) { moxAtRelease.append(mox.isMox()); }
                });

        slices.clear();
        arb.releaseBinding();
        QCOMPARE(moxAtRelease, QList<bool>({false}));
        QCOMPARE(arb.txBoundSliceId(), -1);
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
