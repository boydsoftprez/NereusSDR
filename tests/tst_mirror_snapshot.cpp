// =================================================================
// tests/tst_mirror_snapshot.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Remote Daemon R2 Task 10: the connect-time snapshot burst
// (StateMirror::attachSession()) and the outbound coalescer
// (MirrorCoalescer) that keeps a mutation triggered synchronously DURING
// that burst from being observed ahead of the snapshot-complete marker.
//
// The central fact this file rests on: StateMirror::sessionMessageReady()
// for a Delta is emitted ONLY from flushCoalescedDeltas(), which
// attachSession() calls exactly once, as its LAST step. Every other
// watched-object notification during the burst routes into the
// coalescer's dirty set instead (onWatchedPropertyChanged(), once a
// session is attached), so nothing can reach a session sink out of order
// -- the same "one uninterrupted event-loop turn on one thread" guarantee
// Tasks 7-9 already lean on for their own ordering proofs.
//
// Also central: ObjectRegistry attaches to slices only going FORWARD
// (Task 9). Three slices that already exist before StateMirror /
// ObjectRegistry are even constructed become visible only once
// backfillExistingSlices() is called explicitly -- constructing the
// registry AFTER the slices already exist, then calling that method, is
// what the first test below does, matching
// tst_mirror_lifecycle.cpp's own backfill test.
//
// Modification history (NereusSDR):
//   2026-09-29 - The unattached forwarder's frequency change carries a
//                 second notify since the slice's diversityPattern moves
//                 with it: one emission for each. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QByteArray>
#include <QList>
#include <QSet>
#include <QSignalSpy>
#include <QVariant>

#include "core/WdspTypes.h"
#include "core/session/MirrorSchema.h"
#include "core/session/ObjectRegistry.h"
#include "core/session/SessionMessages.h"
#include "core/session/StateMirror.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <limits>

using namespace NereusSDR;

namespace {

// Records every SessionMessage StateMirror::sessionMessageReady() emits,
// in arrival order -- the "recording sink" the task brief asks for.
class Sink : public QObject {
public:
    explicit Sink(StateMirror* mirror)
    {
        connect(mirror, &StateMirror::sessionMessageReady, this,
                [this](const SessionMessage& message) { entries.append(message); });
    }

    int indexOfFirst(SessionMessageKind kind) const
    {
        for (int i = 0; i < entries.size(); ++i) {
            if (entries.at(i).kind == kind) {
                return i;
            }
        }
        return -1;
    }

    int indexOfCreate(const QByteArray& key) const
    {
        for (int i = 0; i < entries.size(); ++i) {
            const SessionMessage& m = entries.at(i);
            if (m.kind == SessionMessageKind::ObjectCreate && m.objectKey == key) {
                return i;
            }
        }
        return -1;
    }

    // First Delta entry naming `property` for `key`, or -1.
    int indexOfDelta(const QByteArray& key, const QByteArray& property) const
    {
        for (int i = 0; i < entries.size(); ++i) {
            const SessionMessage& m = entries.at(i);
            if (m.kind != SessionMessageKind::Delta || m.objectKey != key) {
                continue;
            }
            for (const MirrorUpdate& u : m.updates) {
                if (u.name == property) {
                    return i;
                }
            }
        }
        return -1;
    }

    int countOf(SessionMessageKind kind) const
    {
        int n = 0;
        for (const SessionMessage& m : entries) {
            if (m.kind == kind) {
                ++n;
            }
        }
        return n;
    }

    QList<SessionMessage> entries;
};

} // namespace

class TestMirrorSnapshot : public QObject {
    Q_OBJECT

private slots:

    // ── Step 1: the central ordering test ───────────────────────────────

    // Three slices, already fully created before StateMirror / ObjectRegistry
    // even exist -- exactly the case Task 9's review round closed:
    // backfillExistingSlices() is what makes them visible at all.
    // attachSession() must then produce, in order: one schema message per
    // distinct watched class (here, one: SliceModel), three object.create
    // each with a FULL property bag, the snapshot-complete marker, and
    // ONLY THEN whatever the burst itself caused to become dirty.
    void attachSessionOrdersSchemaCreatesMarkerThenDeltas()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice(QStringLiteral("pan-a"));
        const int b = model.addSlice(QStringLiteral("pan-b"));
        const int c = model.addSlice(QStringLiteral("pan-c"));
        QVERIFY(a >= 0 && b >= 0 && c >= 0);

        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA != nullptr);
        // A known, unambiguous starting point -- NOT whatever addSlice()
        // happened to seed -- so the mid-build mutation below can be
        // guaranteed to touch frequency alone (see its own comment).
        sliceA->setFrequency(14200000.0);
        QCOMPARE(sliceA->band(), Band::Band20m);

        // StateMirror + ObjectRegistry constructed AFTER all three slices
        // already exist: the exact ordering Task 9's review round found
        // completely unhandled before backfillExistingSlices() was added.
        StateMirror mirror;
        ObjectRegistry registry(&model, &mirror);
        registry.backfillExistingSlices();

        Sink sink(&mirror);

        // A receiver reacting to one of the burst's OWN messages,
        // synchronously, by writing a watched object -- the "mid-build"
        // case the brief calls out. Mutates slice A -- which the create
        // loop will already have visited before slice B's create fires,
        // since watch order is a < b < c -- to a frequency STILL inside
        // 20m, so this produces exactly one delta (frequency) with no
        // band delta riding along, keeping the index arithmetic below
        // unambiguous.
        bool mutated = false;
        const QMetaObject::Connection midBuildHook = QObject::connect(
            &mirror, &StateMirror::sessionMessageReady, &sink,
            [&](const SessionMessage& message) {
                if (mutated) {
                    return;
                }
                if (message.kind == SessionMessageKind::ObjectCreate
                    && message.objectKey == ObjectRegistry::keyForSlice(b)) {
                    mutated = true;
                    sliceA->setFrequency(14250000.0); // still 20m
                }
            });

        mirror.attachSession();
        QObject::disconnect(midBuildHook);
        QVERIFY2(mutated, "test setup: the mid-build mutation never ran");
        QCOMPARE(sliceA->band(), Band::Band20m); // confirms no band delta rides along

        // N schema messages, COMPUTED rather than hardcoded: one per
        // distinct class among what backfillExistingSlices() watched. All
        // three slices are SliceModel, so N == 1 here; a separate test
        // below proves N generalises past 1.
        QSet<QByteArray> classesWatched;
        for (const QByteArray& key : mirror.watchedKeys()) {
            classesWatched.insert(MirrorSchema::shortClassName(
                MirrorSchema::forObject(mirror.watchedObject(key)).className()));
        }
        const int n = classesWatched.size();
        QCOMPARE(n, 1);

        for (int i = 0; i < n; ++i) {
            QCOMPARE(sink.entries.at(i).kind, SessionMessageKind::Schema);
            QVERIFY(classesWatched.contains(sink.entries.at(i).className));
        }
        QCOMPARE(sink.entries.at(0).className, QByteArray("SliceModel"));

        const MirrorSchema& sliceSchema = MirrorSchema::forObject(sliceA);
        QVERIFY2(!sink.entries.at(0).fields.isEmpty(), "schema must declare fields");
        QCOMPARE(sink.entries.at(0).fields.size(), sliceSchema.size());
        bool sawSliceIndexField = false;
        for (const SessionSchemaField& f : sink.entries.at(0).fields) {
            if (f.name == "sliceIndex") {
                sawSliceIndexField = true;
            }
        }
        QVERIFY2(sawSliceIndexField,
                 "schema must declare CONSTANT properties too (sliceIndex is "
                 "the object identity)");

        const QByteArray keyA = ObjectRegistry::keyForSlice(a);
        const QByteArray keyB = ObjectRegistry::keyForSlice(b);
        const QByteArray keyC = ObjectRegistry::keyForSlice(c);

        const int createA = sink.indexOfCreate(keyA);
        const int createB = sink.indexOfCreate(keyB);
        const int createC = sink.indexOfCreate(keyC);
        QVERIFY(createA >= 0 && createB >= 0 && createC >= 0);
        QVERIFY2(createA == n && createB == n + 1 && createC == n + 2,
                 "the three creates must immediately follow the schema "
                 "block, in watch order");

        for (int idx : { createA, createB, createC }) {
            QCOMPARE(sink.entries.at(idx).updates.size(), sliceSchema.size());
        }

        const int markerIdx = sink.indexOfFirst(SessionMessageKind::SnapshotComplete);
        QVERIFY2(markerIdx == n + 3, "the marker must come immediately after the third create");
        QCOMPARE(sink.countOf(SessionMessageKind::SnapshotComplete), 1);

        // The mid-build mutation: must be present (not lost), and strictly
        // after the marker -- never interleaved into the burst even though
        // it was TRIGGERED synchronously from inside the burst.
        const int deltaIdx = sink.indexOfDelta(keyA, "frequency");
        QVERIFY2(deltaIdx >= 0, "the mid-build frequency change was lost");
        QVERIFY2(deltaIdx > markerIdx, "a mid-build change must not precede the marker");
        QVERIFY2(deltaIdx > createA, "a mid-build change must not precede its own object's create");
        QCOMPARE(sink.entries.at(deltaIdx).updates.first().value.toDouble(), 14250000.0);

        // Nothing after the delta: exactly one property was mutated, so
        // the coalescer had exactly one thing to drain.
        QCOMPARE(sink.entries.size(), deltaIdx + 1);
    }

    // ── N generalises past 1 ─────────────────────────────────────────────

    void attachSessionSendsOneSchemaPerDistinctWatchedClass()
    {
        RadioModel model;
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("radio", &model));
        QVERIFY(mirror.watch("slice:0", &slice));

        Sink sink(&mirror);
        mirror.attachSession();

        QCOMPARE(sink.countOf(SessionMessageKind::Schema), 2);
        const QSet<QByteArray> names{ sink.entries.at(0).className, sink.entries.at(1).className };
        QCOMPARE(names, (QSet<QByteArray>{ "RadioModel", "SliceModel" }));
        QCOMPARE(sink.countOf(SessionMessageKind::ObjectCreate), 2);
        // 2 schema + 2 create = indices 0-3; the marker is index 4.
        QCOMPARE(sink.indexOfFirst(SessionMessageKind::SnapshotComplete), 4);
    }

    // ── flushCoalescedDeltas() re-reads live state (review round 1) ─────
    //
    // Review round 1 found the coalescer's stored value can go stale:
    // applyInbound()'s m_applying guard (StateMirror.cpp) suppresses the
    // notify a write produces, specifically so the write does not echo
    // back to the peer that just sent it -- which means a property
    // already pending in the coalescer can be changed AGAIN on the model
    // without update() ever being called a second time for it.
    // flushCoalescedDeltas() must report what the model holds AT FLUSH
    // TIME, not whatever value was pending before the inbound write
    // superseded it.

    void flushReReadsLiveValueSoAnInboundApplyIsNotSupersededByAStalePendingOne()
    {
        SliceModel slice(0);
        slice.setFrequency(14200000.0); // 20m, a known baseline, unwatched yet
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        mirror.attachSession();

        Sink sink(&mirror);

        // A daemon-local path (band-stack restore, TCI, restoreFromSettings)
        // changes the property first: the coalescer pends this value.
        slice.setFrequency(14250000.0); // still 20m

        // Before that pending value is ever flushed, a GUI-originated
        // write arrives through applyInbound(). m_applying suppresses the
        // notify -- by design, Task 8 -- so the coalescer never learns the
        // model moved again; its own stored entry is still 14250000.0.
        const MirrorApplyResult result = mirror.applyInbound(
            QByteArray("slice:0"), QByteArray("frequency"), QVariant(14300000.0));
        QVERIFY2(result.accepted, "test setup: the inbound write must actually land");
        QCOMPARE(slice.frequency(), 14300000.0);

        const int emitted = mirror.flushCoalescedDeltas();
        QCOMPARE(emitted, 1);
        QCOMPARE(sink.entries.size(), 1);
        QCOMPARE(sink.entries.first().kind, SessionMessageKind::Delta);

        const MirrorUpdate* freq = nullptr;
        for (const MirrorUpdate& u : sink.entries.first().updates) {
            if (u.name == "frequency") {
                freq = &u;
            }
        }
        QVERIFY2(freq != nullptr, "the pending frequency property was lost entirely");
        QVERIFY2(freq->value.toDouble() != 14250000.0,
                 "must not report the STALE pre-applyInbound pending value");
        QCOMPARE(freq->value.toDouble(), 14300000.0);
    }

    // ── flushCoalescedDeltas() drops entries for an unwatched key ───────
    //
    // Review round 1's second finding: unwatch() does not purge the
    // coalescer, so a property that went dirty before its object was
    // unwatched (or destroyed) must not survive to be reported as a Delta
    // naming a key nothing is watching by the time it goes out. Both
    // findings share one fix: flushCoalescedDeltas() re-resolves each
    // pending (key, ordinal) against the LIVE watch list, so an unwatched
    // key resolves to nothing.

    void flushDropsPendingPropertiesForAKeyThatWasUnwatched()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        mirror.attachSession();

        Sink sink(&mirror);
        slice.setFrequency(14300000.0); // pends in the coalescer

        mirror.unwatch("slice:0");
        const int emitted = mirror.flushCoalescedDeltas();

        QCOMPARE(emitted, 0);
        QVERIFY2(sink.entries.isEmpty(),
                 "a property that went dirty before its object was unwatched "
                 "must not survive to be delivered as a delta naming a key "
                 "nothing is watching");
    }

    // The control for the test above: the IDENTICAL sequence minus the
    // unwatch() call must deliver normally, so the zero-message result
    // above is evidence the unwatch() dropped something real, not merely
    // that nothing was ever pending in the first place.
    void flushDeliversPendingPropertiesForAStillWatchedKey()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        mirror.attachSession();

        Sink sink(&mirror);
        slice.setFrequency(14300000.0);

        const int emitted = mirror.flushCoalescedDeltas();
        QCOMPARE(emitted, 1);
        QCOMPARE(sink.entries.size(), 1);
        QCOMPARE(sink.entries.first().kind, SessionMessageKind::Delta);
        QCOMPARE(sink.entries.first().objectKey, QByteArray("slice:0"));
    }

    // ── Reattach: complete fresh burst, nothing pre-attach survives ─────
    //
    // StateMirror.h's attachSession() doc comment promises this is safe
    // to call again for a reconnecting client, and Task 19 will rely on
    // it: a complete second burst, with no delta from before the second
    // attach riding along afterward.
    void reattachSendsACompleteFreshBurstAndDropsAnythingPendingFromBeforeIt()
    {
        RadioModel model;
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("radio", &model));
        QVERIFY(mirror.watch("slice:0", &slice));

        Sink sink(&mirror);
        mirror.attachSession(); // first burst
        QCOMPARE(sink.countOf(SessionMessageKind::Schema), 2);
        QCOMPARE(sink.countOf(SessionMessageKind::ObjectCreate), 2);
        QCOMPARE(sink.countOf(SessionMessageKind::SnapshotComplete), 1);

        slice.setFrequency(14300000.0); // pends; never flushed before the re-attach

        sink.entries.clear(); // isolate the second attach's own output
        mirror.attachSession(); // second burst

        // Complete: the same shape as the first.
        QCOMPARE(sink.countOf(SessionMessageKind::Schema), 2);
        QCOMPARE(sink.countOf(SessionMessageKind::ObjectCreate), 2);
        QCOMPARE(sink.countOf(SessionMessageKind::SnapshotComplete), 1);

        // The pre-second-attach pending change must not survive as a
        // standalone Delta riding along after the marker: attachSession()
        // clears the coalescer before rebuilding, and the fresh create for
        // slice:0 already carries the new frequency directly.
        QCOMPARE(sink.countOf(SessionMessageKind::Delta), 0);

        const int createIdx = sink.indexOfCreate("slice:0");
        QVERIFY(createIdx >= 0);
        bool sawFreq = false;
        for (const MirrorUpdate& u : sink.entries.at(createIdx).updates) {
            if (u.name == "frequency") {
                sawFreq = true;
                QCOMPARE(u.value.toDouble(), 14300000.0);
            }
        }
        QVERIFY2(sawFreq, "the second burst's create must reflect current state");
    }

    // ── Local direct mode is unaffected ─────────────────────────────────

    // propertiesChanged() -- the Task 7/8 forwarder -- must behave exactly
    // as it always has when attachSession() is never called: no
    // coalescing, no delay, one emission per notify. This is the
    // regression gate the brief names explicitly.
    void unattachedMirrorForwardsImmediatelyLikeTasksSevenAndEight()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        QVERIFY2(!mirror.hasAttachedSession(), "test setup: attachSession() must never have run");

        QSignalSpy spy(&mirror, &StateMirror::propertiesChanged);
        slice.setFrequency(14200000.0);
        // A frequency change also moves the slice's diversityPattern (phone
        // wire batch), a second notify: one emission for each, in order.
        QCOMPARE(spy.count(), 2);

        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toByteArray(), QByteArray("slice:0"));
        const auto updates = args.at(1).value<QList<NereusSDR::MirrorUpdate>>();
        QCOMPARE(updates.size(), 1);
        QCOMPARE(updates.first().name, QByteArray("frequency"));
        const auto second = spy.takeFirst();
        QCOMPARE(second.at(0).toByteArray(), QByteArray("slice:0"));
        const auto moved = second.at(1).value<QList<NereusSDR::MirrorUpdate>>();
        QCOMPARE(moved.size(), 1);
        QCOMPARE(moved.first().name, QByteArray("diversityPattern"));
    }

    // ── The coalescer, standalone ────────────────────────────────────────

    // Two properties on one object, one property on a second: flush()
    // must group by object (arrival order across objects) while keeping
    // each object's OWN ordinal arrival order, and a second update() for
    // an already-pending key must structurally overwrite rather than
    // append -- the "latest-wins" TciVfoCoalescer shape, generalised.
    void coalescerGroupsByObjectAndLatestValueWinsPerOrdinal()
    {
        MirrorCoalescer c;
        QCOMPARE(c.pendingObjectCount(), 0);

        c.update("slice:1", MirrorUpdate{ 5, "band", MirrorWireKind::Enum,
                                          QVariant(static_cast<qlonglong>(Band::Band40m)) });
        c.update("slice:0", MirrorUpdate{ 0, "frequency", MirrorWireKind::Float64, 14200000.0 });
        c.update("slice:0", MirrorUpdate{ 1, "dspMode", MirrorWireKind::Enum, QVariant(qlonglong(1)) });
        // Overwrite: same object, same ordinal, a genuinely different value.
        c.update("slice:0", MirrorUpdate{ 0, "frequency", MirrorWireKind::Float64, 14250000.0 });

        QCOMPARE(c.pendingObjectCount(), 2);
        QCOMPARE(c.pendingPropertyCount(), 3);

        const QList<QPair<QByteArray, QList<MirrorUpdate>>> batches = c.flush();
        QCOMPARE(batches.size(), 2);

        // Arrival order across objects: slice:1 went dirty before slice:0.
        QCOMPARE(batches.at(0).first, QByteArray("slice:1"));
        QCOMPARE(batches.at(1).first, QByteArray("slice:0"));

        QCOMPARE(batches.at(1).second.size(), 2);
        // Ordinal arrival order within slice:0: frequency (ordinal 0)
        // arrived before dspMode (ordinal 1).
        QCOMPARE(batches.at(1).second.at(0).name, QByteArray("frequency"));
        QCOMPARE(batches.at(1).second.at(0).value.toDouble(), 14250000.0); // latest, not first
        QCOMPARE(batches.at(1).second.at(1).name, QByteArray("dspMode"));

        // flush() drains fully.
        QCOMPARE(c.flush().size(), 0);
        QCOMPARE(c.pendingObjectCount(), 0);
    }

    void coalescerClearDropsEverythingUnflushed()
    {
        MirrorCoalescer c;
        c.update("slice:0", MirrorUpdate{ 0, "frequency", MirrorWireKind::Float64, 14200000.0 });
        QCOMPARE(c.pendingObjectCount(), 1);
        c.clear();
        QCOMPARE(c.pendingObjectCount(), 0);
        QCOMPARE(c.flush().size(), 0);
    }

    // ── SessionMessages: builders + JSON round-trip ─────────────────────

    void sessionMessagesRoundTripEveryKindThroughJson()
    {
        {
            const SessionMessage m = SessionMessages::schema(
                "SliceModel", { SessionSchemaField{ 0, "frequency", MirrorWireKind::Float64 },
                               SessionSchemaField{ 1, "sliceIndex", MirrorWireKind::Int64 } });
            const QByteArray wire = SessionMessages::encode(m);
            SessionMessage back;
            QVERIFY(SessionMessages::decode(wire, &back));
            QCOMPARE(back.kind, SessionMessageKind::Schema);
            QCOMPARE(back.className, QByteArray("SliceModel"));
            QCOMPARE(back.fields.size(), 2);
            QCOMPARE(back.fields.at(0).name, QByteArray("frequency"));
            QCOMPARE(back.fields.at(0).kind, MirrorWireKind::Float64);
            QCOMPARE(back.fields.at(1).ordinal, quint16(1));
        }
        {
            const QList<MirrorUpdate> bag{
                MirrorUpdate{ 0, "frequency", MirrorWireKind::Float64, 14200000.0 },
                MirrorUpdate{ 3, "active", MirrorWireKind::Bool, true },
                MirrorUpdate{ 7, "panKey", MirrorWireKind::Utf8, QStringLiteral("pan-0") },
            };
            const SessionMessage m = SessionMessages::objectCreate("slice:0", "SliceModel", bag);
            const QByteArray wire = SessionMessages::encode(m);
            SessionMessage back;
            QVERIFY(SessionMessages::decode(wire, &back));
            QCOMPARE(back.kind, SessionMessageKind::ObjectCreate);
            QCOMPARE(back.objectKey, QByteArray("slice:0"));
            QCOMPARE(back.className, QByteArray("SliceModel"));
            QCOMPARE(back.updates.size(), 3);
            QCOMPARE(back.updates.at(0).value.toDouble(), 14200000.0);
            QCOMPARE(back.updates.at(1).value.toBool(), true);
            QCOMPARE(back.updates.at(2).value.toString(), QStringLiteral("pan-0"));
        }
        {
            const SessionMessage m = SessionMessages::objectDestroy("slice:0", "SliceModel");
            SessionMessage back;
            QVERIFY(SessionMessages::decode(SessionMessages::encode(m), &back));
            QCOMPARE(back.kind, SessionMessageKind::ObjectDestroy);
            QCOMPARE(back.objectKey, QByteArray("slice:0"));
            QCOMPARE(back.className, QByteArray("SliceModel"));
        }
        {
            const SessionMessage m = SessionMessages::delta(
                "slice:0", { MirrorUpdate{ 0, "frequency", MirrorWireKind::Float64, 7100000.0 } });
            SessionMessage back;
            QVERIFY(SessionMessages::decode(SessionMessages::encode(m), &back));
            QCOMPARE(back.kind, SessionMessageKind::Delta);
            QCOMPARE(back.objectKey, QByteArray("slice:0"));
            QCOMPARE(back.updates.size(), 1);
            QCOMPARE(back.updates.first().value.toDouble(), 7100000.0);
        }
        {
            const SessionMessage m = SessionMessages::snapshotComplete();
            SessionMessage back;
            QVERIFY(SessionMessages::decode(SessionMessages::encode(m), &back));
            QCOMPARE(back.kind, SessionMessageKind::SnapshotComplete);
        }
    }

    void decodeRejectsMalformedInput()
    {
        SessionMessage out;
        QVERIFY(!SessionMessages::decode(QByteArray("not json"), &out));
        QVERIFY(!SessionMessages::decode(QByteArray("{}"), &out)); // no "type"
        QVERIFY(!SessionMessages::decode(QByteArray(R"({"type":"nonsense"})"), &out));

        // Unrecognised wire-kind token: fails at wireKindFromName(), before
        // ever reaching the value-type-mismatch branch below.
        QVERIFY(!SessionMessages::decode(
            QByteArray(
                R"({"type":"delta","key":"slice:0","properties":[{"ordinal":0,"name":"x","kind":"nope","value":1}]})"),
            &out));

        // A value whose JSON type does not match its declared kind -- the
        // branch the "nope" case above never reaches.
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(
                         R"({"type":"delta","key":"slice:0","properties":[{"ordinal":0,"name":"x","kind":"f64","value":"not a number"}]})"),
                     &out),
                 "a string value where f64 declares a number must be rejected");

        // Out-of-range ordinals: negative, and past quint16's 65535 max.
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(
                         R"({"type":"delta","key":"slice:0","properties":[{"ordinal":-1,"name":"x","kind":"f64","value":1.0}]})"),
                     &out),
                 "a negative ordinal must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(
                         R"({"type":"delta","key":"slice:0","properties":[{"ordinal":70000,"name":"x","kind":"f64","value":1.0}]})"),
                     &out),
                 "an ordinal past quint16's range must be rejected");

        // Missing structural fields: ABSENT, not merely empty -- a missing
        // field must not silently decode as though it were present and
        // empty. See decodeAcceptsGenuinelyEmptyStructuralFields for the
        // legitimate empty-but-present case this must stay distinct from.
        QVERIFY2(!SessionMessages::decode(QByteArray(R"({"type":"delta"})"), &out),
                 "delta with no key at all must be rejected, not decoded with an empty key");
        QVERIFY2(!SessionMessages::decode(QByteArray(R"({"type":"delta","key":"slice:0"})"), &out),
                 "delta with no properties array at all must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(R"({"type":"object.create","key":"slice:0","properties":[]})"), &out),
                 "object.create with no class must be rejected");
        QVERIFY2(!SessionMessages::decode(QByteArray(R"({"type":"schema","class":"SliceModel"})"), &out),
                 "schema with no fields array at all must be rejected");
        QVERIFY2(!SessionMessages::decode(QByteArray(R"({"type":"object.destroy"})"), &out),
                 "object.destroy with no key must be rejected");

        // Wrong TYPE for a structural field (a number instead of a string
        // key) must be rejected the same way as an absent one.
        QVERIFY2(!SessionMessages::decode(QByteArray(R"({"type":"delta","key":5,"properties":[]})"), &out),
                 "a non-string key must be rejected");
    }

    // ── Int64 / Enum value range (whole-branch review, Important 2) ──────

    // The Int64/Enum value path used to do a bare
    // static_cast<qlonglong>(json.toDouble()) behind nothing but an
    // isDouble() gate. {"value":1e300} parses cleanly and isDouble() is
    // true, so that cast is a floating-to-integer conversion of an
    // unrepresentable value: undefined behaviour, and platform-divergent
    // (a saturating result on arm64, the indefinite value on x86-64), so
    // a developer's Mac and the Pi 4 target would not even agree on the
    // wrong answer. Any UBSan build trips on it.
    //
    // Driven from raw wire bytes on purpose -- the whole defect is what
    // an attacker-controlled JSON number does on the way in, so a
    // hand-built MirrorUpdate cannot express it at all.
    void decodeRejectsIntegerValuesOutsideInt64sRange()
    {
        SessionMessage out;

        QVERIFY2(!SessionMessages::decode(
                     QByteArray(
                         R"({"type":"delta","key":"slice:0","properties":[{"ordinal":0,"name":"x","kind":"i64","value":1e300}]})"),
                     &out),
                 "an i64 value past qlonglong's range must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(
                         R"({"type":"delta","key":"slice:0","properties":[{"ordinal":0,"name":"x","kind":"i64","value":-1e300}]})"),
                     &out),
                 "an i64 value below qlonglong's range must be rejected");
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(
                         R"({"type":"delta","key":"slice:0","properties":[{"ordinal":0,"name":"x","kind":"enum","value":1e300}]})"),
                     &out),
                 "an enum value past qlonglong's range must be rejected");

        // The exact boundary. 2^63 is representable as a double but not
        // as a qlonglong, so it is the first value that must be refused;
        // -2^63 IS qlonglong's minimum and must still be accepted, as
        // must the largest double strictly below 2^63.
        QVERIFY2(!SessionMessages::decode(
                     QByteArray(
                         R"({"type":"delta","key":"slice:0","properties":[{"ordinal":0,"name":"x","kind":"i64","value":9223372036854775808}]})"),
                     &out),
                 "2^63 is one past qlonglong's maximum and must be rejected");
        QVERIFY2(SessionMessages::decode(
                     QByteArray(
                         R"({"type":"delta","key":"slice:0","properties":[{"ordinal":0,"name":"x","kind":"i64","value":-9223372036854775808}]})"),
                     &out),
                 "-2^63 is qlonglong's minimum and must still be accepted");
        QCOMPARE(out.updates.first().value.toLongLong(),
                 std::numeric_limits<qlonglong>::min());
        QVERIFY2(SessionMessages::decode(
                     QByteArray(
                         R"({"type":"delta","key":"slice:0","properties":[{"ordinal":0,"name":"x","kind":"i64","value":9223372036854774784}]})"),
                     &out),
                 "the largest double below 2^63 must still be accepted");
        QCOMPARE(out.updates.first().value.toLongLong(), Q_INT64_C(9223372036854774784));

        // Ordinary values are untouched by the check.
        QVERIFY(SessionMessages::decode(
            QByteArray(
                R"({"type":"delta","key":"slice:0","properties":[{"ordinal":0,"name":"x","kind":"i64","value":0}]})"),
            &out));
        QCOMPARE(out.updates.first().value.toLongLong(), Q_INT64_C(0));
        QVERIFY(SessionMessages::decode(
            QByteArray(
                R"({"type":"delta","key":"slice:0","properties":[{"ordinal":0,"name":"x","kind":"enum","value":6}]})"),
            &out));
        QCOMPARE(out.updates.first().value.toLongLong(), Q_INT64_C(6));
    }

    // The companion to the case above, and the one the reviewer asked for
    // by shape: bytes produced from a LIVE model rather than from a
    // hand-written MirrorUpdate literal. Every Int64- and Enum-kind
    // property of a real SliceModel is read through MirrorSchema, encoded,
    // decoded, and compared -- so a range check that were too tight would
    // fail here rather than only showing up on a bench.
    void everyIntegerPropertyOfALiveSliceSurvivesTheJsonRoundTrip()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int id = model.addSlice(QStringLiteral("pan-a"));
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice != nullptr);
        slice->setFrequency(14200000.0);
        slice->setDspMode(DSPMode::AM);
        slice->setStepHz(1000);

        const MirrorSchema& schema = MirrorSchema::forObject(slice);
        QList<MirrorUpdate> bag;
        for (const MirrorProperty& prop : schema.properties()) {
            if (prop.kind != MirrorWireKind::Int64 && prop.kind != MirrorWireKind::Enum) {
                continue;
            }
            const QVariant live = schema.read(prop, slice);
            QVERIFY2(live.isValid(), prop.name.constData());
            bag.append(MirrorUpdate{ prop.ordinal, prop.name, prop.kind, live });
        }
        QVERIFY2(bag.size() >= 12, "a real SliceModel must carry integer and enum properties");

        const QByteArray wire =
            SessionMessages::encode(SessionMessages::delta(ObjectRegistry::keyForSlice(id), bag));
        SessionMessage back;
        QVERIFY2(SessionMessages::decode(wire, &back), wire.constData());
        QCOMPARE(back.updates.size(), bag.size());
        for (int i = 0; i < bag.size(); ++i) {
            QCOMPARE(back.updates.at(i).name, bag.at(i).name);
            QCOMPARE(back.updates.at(i).kind, bag.at(i).kind);
            QCOMPARE(back.updates.at(i).value.toLongLong(), bag.at(i).value.toLongLong());
        }
    }

    // ── Whole-branch review, Minor 2: the three non-finite float tokens ──
    //
    // toJsonValue() encodes a non-finite Float64 as one of three explicit
    // string tokens, because JSON has no representation for them and
    // QJsonValue(qQNaN()) silently becomes null. Nothing pinned any of the
    // three directly: NaN was covered only incidentally, by whatever live
    // snapshot happened to include SliceModel::snrDb at its NaN default,
    // and +inf / -inf had no coverage at all in either direction. A NaN
    // defect on this exact path survived four task reviews before task 18
    // caught it, which is why this asserts on the ENCODED BYTES from a
    // LIVE model rather than round-tripping a hand-written MirrorUpdate:
    // a hand-built bag proves the two halves of the codec agree with each
    // other, not that either matches the wire.
    void theThreeNonFiniteFloatTokensSurviveTheWireFromALiveModel()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int id = model.addSlice(QStringLiteral("pan-a"));
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice != nullptr);

        const MirrorSchema& schema = MirrorSchema::forObject(slice);
        const MirrorProperty* snr = schema.byName("snrDb");
        QVERIFY(snr != nullptr);
        QCOMPARE(snr->kind, MirrorWireKind::Float64);

        struct Case {
            double value;
            const char* token;
        };
        // NaN LAST on purpose: setSnrDb() is change-guarded and treats
        // NaN -> NaN as a no-op, so starting from the NaN default and
        // asserting NaN first would pass without the setter ever running.
        const Case cases[] = {
            { std::numeric_limits<double>::infinity(), "inf" },
            { -std::numeric_limits<double>::infinity(), "-inf" },
            { std::numeric_limits<double>::quiet_NaN(), "nan" },
        };

        for (const Case& c : cases) {
            // A finite value between cases, and it is load-bearing rather
            // than tidiness. SliceModel::setSnrDb()'s change guard is
            // qFuzzyCompare(db, m_snrDb), and qFuzzyCompare(+inf, -inf) is
            // TRUE: the difference is -inf, its magnitude is inf, and the
            // tolerance term 1e-12 * qMin(inf, inf) is also inf, so
            // `inf <= inf` holds. Going straight from +inf to -inf left
            // the model at +inf and this test caught it, encoding "inf"
            // where it wanted "-inf". That is a pre-existing property of
            // the setter, unrelated to R2 and out of this round's scope
            // (snrDb is a WDSP SNR reading and never legitimately
            // infinite), recorded here so a later reader does not
            // rediscover it as a codec bug.
            slice->setSnrDb(0.0);
            slice->setSnrDb(c.value);
            const QVariant live = schema.read(*snr, slice);
            QVERIFY2(live.isValid(), c.token);

            const QByteArray wire = SessionMessages::encode(SessionMessages::delta(
                ObjectRegistry::keyForSlice(id),
                { MirrorUpdate{ snr->ordinal, snr->name, snr->kind, live } }));

            // The token, verbatim, in the bytes that leave the process --
            // and specifically as a JSON STRING, which is what keeps it
            // impossible to confuse with a real Float64 value.
            const QByteArray expected =
                QByteArray("\"value\":\"") + QByteArray(c.token) + QByteArray("\"");
            QVERIFY2(wire.contains(expected),
                     qPrintable(QStringLiteral("expected %1 in %2")
                                    .arg(QString::fromUtf8(expected),
                                         QString::fromUtf8(wire))));

            SessionMessage back;
            QVERIFY2(SessionMessages::decode(wire, &back), wire.constData());
            QCOMPARE(back.updates.size(), 1);
            const double decoded = back.updates.first().value.toDouble();
            if (std::isnan(c.value)) {
                QVERIFY2(std::isnan(decoded), "NaN did not survive the round trip");
            } else {
                QVERIFY2(std::isinf(decoded), "an infinity did not survive the round trip");
                // The SIGN matters and is the half a single "isinf" check
                // would miss: +inf and -inf are distinct tokens for a
                // reason.
                QCOMPARE(decoded > 0.0, c.value > 0.0);
            }
        }
    }

    // The companion to the "missing" cases above: a genuinely PRESENT but
    // EMPTY key/array is a different, legal shape -- an empty properties
    // array is exactly what a class with zero changed properties or zero
    // declared fields would encode -- and decode() must still accept it.
    void decodeAcceptsGenuinelyEmptyStructuralFields()
    {
        SessionMessage out;
        QVERIFY(SessionMessages::decode(
            QByteArray(R"({"type":"delta","key":"slice:0","properties":[]})"), &out));
        QCOMPARE(out.updates.size(), 0);
        QVERIFY(SessionMessages::decode(
            QByteArray(R"({"type":"schema","class":"SliceModel","fields":[]})"), &out));
        QCOMPARE(out.fields.size(), 0);
    }
};

QTEST_MAIN(TestMirrorSnapshot)
#include "tst_mirror_snapshot.moc"
