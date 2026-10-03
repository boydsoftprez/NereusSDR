// =================================================================
// tests/tst_mirror_lifecycle.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Remote Daemon R2 Task 9: ObjectRegistry, the slice-lifecycle half of the
// mirror. StateMirror (Tasks 7-8) forwards PROPERTY deltas for objects it
// already knows about; ObjectRegistry is what tells a remote client a slice
// APPEARED or VANISHED at all, by watching/unwatching StateMirror on
// RadioModel::sliceAdded / sliceRemoved and emitting the corresponding
// object.create / object.destroy events.
//
// Everything here hinges on one fact: RadioModel::removeSlice() calls
// slice->deleteLater() BEFORE emitting sliceRemoved() (RadioModel.cpp,
// verified at :5122-5123 against this tree's HEAD). deleteLater() only
// SCHEDULES destruction; the object is fully alive, with a valid vtable and
// live signals, for the rest of this call stack. A same-event-loop-turn
// addSlice() can therefore remint the freed id while the corpse still
// exists (RadioModel always hands out the lowest id NOT currently in
// m_slices, and removeSlice() never renumbers survivors). ObjectRegistry
// keys its live-object bookkeeping by QPointer and detaches from
// StateMirror synchronously INSIDE the sliceRemoved handler -- never by
// waiting on QObject::destroyed, which does not arrive until the event loop
// next spins, by which point a same-turn re-add would already have
// collided with the still-watched corpse.
// =================================================================

#include <QtTest/QtTest>
#include <QByteArray>
#include <QCoreApplication>
#include <QList>
#include <QPointer>
#include <QSignalSpy>
#include <QVariant>

#include "core/session/MirrorSchema.h"
#include "core/session/ObjectRegistry.h"
#include "core/session/StateMirror.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// One ordered log of everything a remote client would have observed:
// StateMirror's property deltas AND ObjectRegistry's create/destroy events,
// interleaved in the exact order Qt's direct connections deliver them (the
// same order a real single-threaded session, per the R2 design addendum
// section 7, guarantees on the wire). Deliberately richer than the
// Collector class tst_mirror_forwarder.cpp / tst_mirror_inbound.cpp each
// define locally: this file needs ONE shared timeline across both signal
// sources, not a per-key value flatten.
struct LogEntry {
    enum class Kind { Create, Destroy, Delta };
    Kind kind{};
    QByteArray key;
    QByteArray className;        // Create / Destroy only
    int id = -1;                 // Create / Destroy only
    QList<MirrorUpdate> updates; // Create: full snapshot. Delta: the batch.
};

class Log : public QObject {
public:
    Log(StateMirror* mirror, ObjectRegistry* registry)
    {
        connect(mirror, &StateMirror::propertiesChanged, this,
                [this](const QByteArray& key, const QList<MirrorUpdate>& updates) {
                    entries.append(LogEntry{ LogEntry::Kind::Delta, key, QByteArray(), -1, updates });
                });
        connect(registry, &ObjectRegistry::objectCreated, this,
                [this](const QByteArray& key, const QByteArray& className, int id,
                       const QList<MirrorUpdate>& snapshot) {
                    entries.append(LogEntry{ LogEntry::Kind::Create, key, className, id, snapshot });
                });
        connect(registry, &ObjectRegistry::objectDestroyed, this,
                [this](const QByteArray& key, const QByteArray& className, int id) {
                    entries.append(LogEntry{ LogEntry::Kind::Destroy, key, className, id, {} });
                });
    }

    int indexOfCreate(const QByteArray& key) const { return indexOfKind(LogEntry::Kind::Create, key); }
    int indexOfDestroy(const QByteArray& key) const { return indexOfKind(LogEntry::Kind::Destroy, key); }

    // First index of a Delta batch for `key` carrying `property` == `value`.
    int indexOfDeltaValue(const QByteArray& key, const QByteArray& property,
                          const QVariant& value) const
    {
        for (int i = 0; i < entries.size(); ++i) {
            const LogEntry& e = entries.at(i);
            if (e.kind != LogEntry::Kind::Delta || e.key != key) { continue; }
            for (const MirrorUpdate& u : e.updates) {
                if (u.name == property && u.value == value) { return i; }
            }
        }
        return -1;
    }

    // `property`'s value inside the FIRST create logged for `key`, or
    // nullptr if there is no such create or it carries no such property.
    const MirrorUpdate* createSnapshotProperty(const QByteArray& key,
                                               const QByteArray& property) const
    {
        for (const LogEntry& e : entries) {
            if (e.kind != LogEntry::Kind::Create || e.key != key) { continue; }
            for (const MirrorUpdate& u : e.updates) {
                if (u.name == property) { return &u; }
            }
            return nullptr;
        }
        return nullptr;
    }

    int countDeltasFor(const QByteArray& key) const
    {
        int n = 0;
        for (const LogEntry& e : entries) {
            if (e.kind == LogEntry::Kind::Delta && e.key == key) { ++n; }
        }
        return n;
    }

    QList<LogEntry> entries;

private:
    int indexOfKind(LogEntry::Kind kind, const QByteArray& key) const
    {
        for (int i = 0; i < entries.size(); ++i) {
            if (entries.at(i).kind == kind && entries.at(i).key == key) { return i; }
        }
        return -1;
    }
};

} // namespace

class TestMirrorLifecycle : public QObject {
    Q_OBJECT

private slots:

    // ── Step 1: the central same-turn id-reuse test ─────────────────────

    // With A(0) B(1) C(2), remove 1 and add again in the SAME event-loop
    // turn (no processEvents() between the two calls), reminting id 1
    // while the corpse from the first B is still fully alive. destroy{Slice,1}
    // must strictly precede create{Slice,1}, and nothing the corpse still
    // does after removal may reach the wire.
    void sameTurnIdReuseOrdersDestroyBeforeCreateAndReferencesNothingDead()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        StateMirror mirror;
        ObjectRegistry registry(&model, &mirror);
        Log log(&mirror, &registry);

        const int a = model.addSlice();
        const int b = model.addSlice();
        const int c = model.addSlice();
        QCOMPARE(a, 0);
        QCOMPARE(b, 1);
        QCOMPARE(c, 2);
        log.entries.clear(); // only what follows matters to this test

        SliceModel* oldSliceRaw = model.sliceById(b);
        QVERIFY(oldSliceRaw != nullptr);
        // A distinctive, genuinely-different value. Task 7/8's own testing
        // discipline applies here too: SliceModel's setters are uniformly
        // change-guarded, so a value that does not differ from the ctor
        // default would produce no NOTIFY and no forwarded delta at all,
        // which would make the "no delta references the corpse" check
        // below pass whether or not the corpse is truly unwatched.
        oldSliceRaw->setFrequency(19'999'000.0);
        QVERIFY2(log.indexOfDeltaValue("slice:1", "frequency", 19'999'000.0) >= 0,
                 "setup: the pre-removal change must itself have forwarded "
                 "normally, or this is not a meaningful baseline");
        QPointer<SliceModel> oldSlice(oldSliceRaw);

        // Same event-loop turn as the addSlice() below: no processEvents()
        // runs between them. removeSlice() calls deleteLater() (schedules,
        // does not delete) before emitting sliceRemoved(), so oldSlice must
        // still be alive at every point up to the processEvents() call at
        // the end of this test.
        model.removeSlice(b);
        QVERIFY2(!oldSlice.isNull(),
                 "test invalid: deleteLater() must not have run yet, or "
                 "this is not exercising the same-turn hazard the task is "
                 "about");

        const int deltaCountJustAfterRemoval = log.countDeltasFor("slice:1");

        // Poke the still-alive corpse directly. If StateMirror were still
        // connected to it (the QObject::destroyed-based approach the task
        // brief warns against, which does not disconnect until the event
        // loop next spins), this would forward as an ordinary delta: there
        // is nothing broken about the C++ object itself, it is simply no
        // longer in RadioModel's slice list.
        oldSlice->setFrequency(1'234'000.0);
        QCOMPARE(log.countDeltasFor("slice:1"), deltaCountJustAfterRemoval);

        const int reAdded = model.addSlice(); // same turn: re-mints id 1
        QCOMPARE(reAdded, 1);
        SliceModel* newSlice = model.sliceById(1);
        QVERIFY(newSlice != nullptr);
        QVERIFY2(newSlice != oldSlice.data(),
                 "must be a genuinely different C++ object under the "
                 "reused id, not the same one reused");

        const int destroyIdx = log.indexOfDestroy("slice:1");
        const int createIdx = log.indexOfCreate("slice:1");
        QVERIFY2(destroyIdx >= 0, "destroy{Slice,1} never arrived");
        QVERIFY2(createIdx >= 0, "create{Slice,1} for the re-minted id never arrived");
        QVERIFY2(destroyIdx < createIdx,
                 "destroy{Slice,1} must strictly precede the re-create");

        // The re-create's own snapshot must reflect the NEW object's state,
        // not leak either value the corpse ever held.
        const MirrorUpdate* freq = log.createSnapshotProperty("slice:1", "frequency");
        QVERIFY(freq != nullptr);
        QVERIFY2(freq->value.toDouble() != 1'234'000.0
                     && freq->value.toDouble() != 19'999'000.0,
                 "the re-create must reflect the NEW object's state, not "
                 "the dead one's");

        // QTest::qExec() invokes test slots directly with no event loop
        // ever entered (no exec() on the call stack), so deleteLater()'s
        // DeferredDelete event was posted at loop level 0 and a plain
        // processEvents() call does not drain it -- measured directly:
        // two processEvents() calls in a row both left oldSlice non-null.
        // sendPostedEvents(nullptr, QEvent::DeferredDelete) reaches it
        // unconditionally.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY2(oldSlice.isNull(),
                 "confirms the corpse really was only DEFERRED, not "
                 "skipped -- otherwise everything above proved nothing");
    }

    // ── Step 2 / 3: attach on sliceAdded, settled snapshot ──────────────

    // Watching begins at sliceAdded, which fires only AFTER addSlice() has
    // already run the TX arbiter resync, the stream bind and
    // wireSliceSignals. The create must therefore be the ONLY event this
    // key ever produces up to this point, and it must already show settled,
    // bound state -- never -1 for a slice that IS bound.
    void createCarriesTheSettledSnapshotNotTheConstructionHistory()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        StateMirror mirror;
        ObjectRegistry registry(&model, &mirror);
        Log log(&mirror, &registry);

        const int a = model.addSlice(QStringLiteral("pan-a"));
        QVERIFY(a >= 0);
        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA != nullptr);
        QVERIFY2(sliceA->streamIndex() >= 0,
                 "test setup: the slice must be genuinely bound by the time "
                 "addSlice() returns, or the snapshot check below is vacuous");

        // Watching starts only once sliceAdded fires (inside addSlice(),
        // after the bind), so nothing about this slice's construction could
        // have been observed before the create -- there is exactly one
        // event for this key, and it is the create.
        QCOMPARE(log.entries.size(), 1);
        QCOMPARE(log.entries.first().kind, LogEntry::Kind::Create);
        QCOMPARE(log.entries.first().key, QByteArray("slice:0"));
        QCOMPARE(log.entries.first().className, QByteArray("SliceModel"));
        QCOMPARE(log.entries.first().id, a);

        const MirrorUpdate* streamIdx = log.createSnapshotProperty("slice:0", "streamIndex");
        QVERIFY(streamIdx != nullptr);
        QCOMPARE(streamIdx->value.toInt(), sliceA->streamIndex());
        QVERIFY2(streamIdx->value.toInt() >= 0,
                 "the create must show the SETTLED, bound value");

        // sliceIndex is CONSTANT (Task 7): reachable ONLY through
        // StateMirror::snapshot(), never through the notify map. Its
        // presence here, with the right value, is concrete proof this
        // create used the snapshot path (the whole schema) rather than
        // replaying whatever the notify map happened to see fire during
        // construction.
        const MirrorUpdate* idProp = log.createSnapshotProperty("slice:0", "sliceIndex");
        QVERIFY(idProp != nullptr);
        QCOMPARE(idProp->value.toInt(), a);
    }

    // ── Fix round 1: backfilling slices that predate the registry ──────

    // DaemonApp::start() (src/core/daemon/DaemonApp.cpp) already creates
    // slices directly through RadioModel::addSlice() with no
    // StateMirror/ObjectRegistry anywhere in src/core/daemon/. A daemon
    // that only builds the mirror pair once a remote client connects
    // therefore has live slices waiting for it from the moment it starts,
    // and the registry's constructor alone (two connect() calls, no
    // enumeration) cannot see them. backfillExistingSlices() is the fix:
    // this test constructs the registry AFTER three slices already exist
    // and proves all three become properly watched, full-snapshot objects.
    void backfillWatchesSlicesThatAlreadyExistedBeforeConstruction()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice(QStringLiteral("pan-a"));
        const int b = model.addSlice(QStringLiteral("pan-b"));
        const int c = model.addSlice(QStringLiteral("pan-c"));
        QVERIFY(a >= 0);
        QVERIFY(b >= 0);
        QVERIFY(c >= 0);

        // The registry is constructed AFTER all three already exist: the
        // exact ordering DaemonApp::start() creates today, and the one
        // review round 1 found completely unhandled.
        StateMirror mirror;
        ObjectRegistry registry(&model, &mirror);
        Log log(&mirror, &registry);
        QVERIFY2(!registry.isLive(a) && !registry.isLive(b) && !registry.isLive(c),
                 "construction alone must not retroactively watch anything; "
                 "that is precisely the gap backfillExistingSlices() closes");
        QVERIFY2(log.entries.isEmpty(),
                 "nothing should have been announced before the backfill runs");

        registry.backfillExistingSlices();

        QCOMPARE(log.entries.size(), 3);
        QVERIFY(registry.isLive(a));
        QVERIFY(registry.isLive(b));
        QVERIFY(registry.isLive(c));
        QCOMPARE(registry.liveSliceIds(), (QList<int>{ a, b, c }));

        // Each entry is a genuine create (not merely "something happened"),
        // and each carries a FULL, settled property bag -- streamIndex is
        // genuinely bound (configureStreamPool ran before any addSlice()),
        // so its presence with the real bound value proves this is the same
        // StateMirror::snapshot() path the live sliceAdded case uses, not a stub.
        for (int id : { a, b, c }) {
            const QByteArray key = ObjectRegistry::keyForSlice(id);
            const int idx = log.indexOfCreate(key);
            QVERIFY2(idx >= 0, qPrintable(key));
            QCOMPARE(log.entries.at(idx).className, QByteArray("SliceModel"));
            QCOMPARE(log.entries.at(idx).id, id);

            SliceModel* slice = model.sliceById(id);
            QVERIFY(slice != nullptr);
            const MirrorUpdate* streamIdx = log.createSnapshotProperty(key, "streamIndex");
            QVERIFY2(streamIdx != nullptr, qPrintable(key));
            QCOMPARE(streamIdx->value.toInt(), slice->streamIndex());
            QVERIFY(streamIdx->value.toInt() >= 0);

            const MirrorUpdate* sliceIdxProp = log.createSnapshotProperty(key, "sliceIndex");
            QVERIFY2(sliceIdxProp != nullptr, qPrintable(key));
            QCOMPARE(sliceIdxProp->value.toInt(), id);
        }

        // Idempotent: a second call with nothing new must be a silent no-op.
        log.entries.clear();
        registry.backfillExistingSlices();
        QVERIFY2(log.entries.isEmpty(), "a second backfill call must not re-create anything");
        QCOMPARE(registry.liveSliceIds(), (QList<int>{ a, b, c }));

        // The ordinary live path still works normally afterward: a slice
        // added AFTER the backfill produces a normal create through
        // onSliceAdded(), not through backfill logic.
        const int d = model.addSlice(QStringLiteral("pan-d"));
        QVERIFY(d >= 0);
        QCOMPARE(log.entries.size(), 1);
        QCOMPARE(log.entries.first().kind, LogEntry::Kind::Create);
        QCOMPARE(log.entries.first().key, ObjectRegistry::keyForSlice(d));
    }

    // Symmetric with the duplicate-create guard: backfilling must not
    // re-announce a slice a real sliceAdded() already caught, and must not
    // treat that overlap as a protocol error the way onSliceAdded() would.
    void backfillSkipsSlicesTheLiveSignalAlreadyCaught()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        StateMirror mirror;
        ObjectRegistry registry(&model, &mirror); // constructed FIRST this time
        Log log(&mirror, &registry);

        const int a = model.addSlice(); // real, live sliceAdded -> normal create
        QCOMPARE(log.entries.size(), 1);
        QVERIFY(registry.isLive(a));
        log.entries.clear();

        registry.backfillExistingSlices();

        QVERIFY2(log.entries.isEmpty(),
                 "a slice already watched via the live signal must not be "
                 "re-announced by a later backfill call");
    }

    // ── Step 4: a create for a live id is a protocol error ──────────────

    // RadioModel's own id allocator (addSlice() always scans for the
    // lowest id NOT currently in m_slices) makes a genuine double-create
    // for one id unreachable through its real API. This drives
    // ObjectRegistry's public slot directly to prove the internal guard,
    // not to claim RadioModel can trigger it by itself.
    void duplicateCreateForALiveIdIsRefusedNotReCreated()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        StateMirror mirror;
        ObjectRegistry registry(&model, &mirror);

        const int a = model.addSlice(); // the real, automatic create for id 0
        QVERIFY(registry.isLive(a));
        QObject* watchedBefore = mirror.watchedObject(ObjectRegistry::keyForSlice(a));
        QVERIFY(watchedBefore != nullptr);

        QSignalSpy createdSpy(&registry, &ObjectRegistry::objectCreated);
        QCOMPARE(createdSpy.count(), 0); // spy attached after the real add

        // Manufacture the malformed sequence: sliceAdded(0) again, with no
        // intervening sliceRemoved(0). Without the live-id guard,
        // StateMirror::watch() is idempotent for the SAME key and object
        // (Task 7's own documented behaviour), so this would silently
        // succeed and this registry would emit a SECOND object.create for
        // an id that never went anywhere.
        registry.onSliceAdded(a);

        QCOMPARE(createdSpy.count(), 0); // no second object.create
        QVERIFY(registry.isLive(a));     // still exactly the original entry
        QCOMPARE(mirror.watchedObject(ObjectRegistry::keyForSlice(a)), watchedBefore);
    }

    // ── Step 5: removal order ────────────────────────────────────────────

    // removeSlice() hands TX off to a survivor BEFORE tearing the victim
    // out of the list (RadioModel.cpp :5064-5069), then frees the victim's
    // own stream binding (:5090), then deleteLater()+sliceRemoved()
    // (:5122-5123) last. A remote client must observe the same order:
    // txSlice moves off the victim (and onto the fallback) while the
    // victim still exists, THEN the victim's own stream binding changes,
    // THEN object.destroy.
    void removalOrderTxHandoffThenStreamUnbindThenDestroy()
    {
        RadioModel model;
        model.configureStreamPool(2, 5, 192000); // 2 independent DDCs
        StateMirror mirror;
        ObjectRegistry registry(&model, &mirror);
        Log log(&mirror, &registry);

        const int a = model.addSlice(QStringLiteral("pan-a")); // own stream
        const int b = model.addSlice(QStringLiteral("pan-b")); // own stream
        QVERIFY(a >= 0);
        QVERIFY(b >= 0);
        SliceModel* sliceA = model.sliceById(a);
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceA != nullptr);
        QVERIFY(sliceB != nullptr);
        // TxSliceArbiter::syncToSliceList's "initial bind" arm raises the
        // flag on the first slice when nothing is flagged yet -- established
        // behaviour, pinned independently by
        // tst_tx_slice_binding_invariant.cpp::firstSliceIsTxBoundWithoutAnyHandoff.
        QVERIFY2(sliceA->isTxSlice(), "test setup: A must start TX-bound");
        QVERIFY(!sliceB->isTxSlice());
        QVERIFY(sliceA->streamIndex() >= 0);
        QVERIFY(sliceB->streamIndex() >= 0);
        QVERIFY2(sliceA->streamIndex() != sliceB->streamIndex(),
                 "test setup: A and B must be on independent streams, so "
                 "the only stream-binding delta possible is the victim's "
                 "own");
        log.entries.clear();

        model.removeSlice(a); // the TX-bound slice

        const int txOffA = log.indexOfDeltaValue("slice:0", "txSlice", false);
        const int txOnB = log.indexOfDeltaValue("slice:1", "txSlice", true);
        const int streamUnbindA = log.indexOfDeltaValue("slice:0", "streamIndex", -1);
        const int destroyA = log.indexOfDestroy("slice:0");

        QVERIFY2(txOffA >= 0, "victim's own txSlice never went to false");
        QVERIFY2(txOnB >= 0, "fallback's txSlice never went to true");
        QVERIFY2(streamUnbindA >= 0, "victim's streamIndex never reset to -1");
        QVERIFY2(destroyA >= 0, "object.destroy for the victim never arrived");

        QVERIFY2(txOffA < streamUnbindA,
                 "TX must move off the victim before its stream unbinds");
        QVERIFY2(txOnB < streamUnbindA,
                 "the fallback's TX pickup must precede the victim's "
                 "stream unbind");
        QVERIFY2(streamUnbindA < destroyA,
                 "the stream binding change must precede object.destroy");
    }

    // ── Step 6: neither rejection path produces an object.create ───────

    // RadioModel.cpp's bind path (the sliceAddRejected emit inside
    // bindSliceToStream, with the rollback that follows in addSlice()):
    // a new pan asks for its OWN DDC (preferOwnStream), and with the only
    // one already claimed, the allocator has nothing left to hand out.
    // addSlice() never emits sliceAdded on this path, so ObjectRegistry's
    // onSliceAdded is never called at all -- there is nothing special this
    // class has to DO to get this right; the test exists to prove it.
    void bindPathRejectionProducesNoObjectCreate()
    {
        RadioModel model;
        model.configureStreamPool(1, 5, 192000); // exactly one DDC
        StateMirror mirror;
        ObjectRegistry registry(&model, &mirror);
        Log log(&mirror, &registry);

        const int a = model.addSlice(QStringLiteral("pan-a")); // claims the only DDC
        QVERIFY(a >= 0);
        log.entries.clear();

        QSignalSpy rejectedSpy(&model, &RadioModel::sliceAddRejected);
        QSignalSpy addedSpy(&model, &RadioModel::sliceAdded);

        const int b = model.addSlice(QStringLiteral("pan-b")); // no DDC left

        QCOMPARE(b, -1);
        QCOMPARE(addedSpy.count(), 0);
        QCOMPARE(rejectedSpy.count(), 1);
        QVERIFY2(log.entries.isEmpty(),
                 "a rejected add must produce no object.create (or "
                 "anything else)");
        QVERIFY(!registry.isLive(1)); // the id the rejected slice would have taken
    }

    // addSliceOnPan()'s own cap check, refusing before addSlice() is even
    // entered. The more common rejection path in practice (every "+RX" /
    // "+PAN" click goes through addSliceOnPan).
    void addSliceOnPanCapRejectionProducesNoObjectCreate()
    {
        RadioModel model;
        // A stream pool sized for one slice: the cap addSliceOnPan holds
        // to (sliceChannelLimit()).
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 1, 192000);
        StateMirror mirror;
        ObjectRegistry registry(&model, &mirror);
        Log log(&mirror, &registry);

        model.addSliceOnPan(QStringLiteral("pan-a")); // succeeds: cap is 1
        QVERIFY(registry.isLive(0));
        log.entries.clear();

        QSignalSpy rejectedSpy(&model, &RadioModel::sliceAddRejected);
        QSignalSpy addedSpy(&model, &RadioModel::sliceAdded);

        model.addSliceOnPan(QStringLiteral("pan-b")); // cap reached (1 >= 1)

        QCOMPARE(addedSpy.count(), 0);
        QCOMPARE(rejectedSpy.count(), 1);
        QVERIFY2(log.entries.isEmpty(),
                 "a capacity-rejected add must produce no object.create");
        QVERIFY(!registry.isLive(1));
    }

    // ── Public API surface, and destruction safety ──────────────────────

    void publicApiReflectsTheLiveSet()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        StateMirror mirror;
        ObjectRegistry registry(&model, &mirror);

        QCOMPARE(ObjectRegistry::keyForSlice(0), QByteArray("slice:0"));
        QCOMPARE(ObjectRegistry::keyForSlice(12), QByteArray("slice:12"));

        QVERIFY(registry.liveSliceIds().isEmpty());
        QVERIFY(!registry.isLive(0));

        const int a = model.addSlice();
        const int b = model.addSlice();
        QCOMPARE(registry.liveSliceIds(), (QList<int>{ a, b }));
        QVERIFY(registry.isLive(a));
        QVERIFY(registry.isLive(b));

        model.removeSlice(a);
        QCOMPARE(registry.liveSliceIds(), (QList<int>{ b }));
        QVERIFY(!registry.isLive(a));
        QVERIFY(registry.isLive(b));
    }

    // Symmetric with StateMirror::~StateMirror()'s unwatchAll(): if the
    // registry is torn down while the mirror outlives it (a remote
    // session's own object going away before the process-lifetime mirror
    // does), nothing it watched is left stranded.
    void destructionUnwatchesEverythingStillLive()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        StateMirror mirror;
        {
            ObjectRegistry registry(&model, &mirror);
            model.addSlice();
            QVERIFY(mirror.isWatching("slice:0"));
        } // registry destroyed here; mirror OUTLIVES it
        QVERIFY2(!mirror.isWatching("slice:0"),
                 "ObjectRegistry's destructor must release everything it "
                 "watched");
    }

    // The opposite, riskier ordering: the mirror destroyed FIRST. Proves
    // m_mirror is held safely (QPointer, not a raw pointer) rather than
    // assuming an ordering the constructor cannot enforce.
    void mirrorDestroyedBeforeRegistryDoesNotCrash()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        auto* mirror = new StateMirror();
        auto* registry = new ObjectRegistry(&model, mirror);
        model.addSlice();

        delete mirror;   // gone first -- registry must not dereference it later
        delete registry; // must not crash
    }
};

QTEST_MAIN(TestMirrorLifecycle)
#include "tst_mirror_lifecycle.moc"
