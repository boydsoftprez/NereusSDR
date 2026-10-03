// =================================================================
// tests/tst_mirror_forwarder.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Remote Daemon R2 Task 7, outbound half: StateMirror watches a model
// instance with ONE zero-argument slot connected to every distinct NOTIFY
// signal in its MirrorSchema, maps senderSignalIndex() back to the dirty
// ordinals, and RE-READS each through QMetaProperty::read.
//
// Re-reading rather than taking the signal's arguments is the load-bearing
// decision, and two independent facts force it:
//
//   - Notifiers are one-to-many. PanadapterModel::levelChanged() carries no
//     arguments at all and names both dBmFloor and dBmCeiling; there is
//     nothing to take.
//   - SliceModel::setDspMode rewrites m_filterLow and m_filterHigh as a side
//     effect (SliceModel.cpp, the per-(band, mode) LastFilter block) and
//     emits filterChanged separately. A forwarder that trusted arguments
//     would still be correct only by coincidence of argument order.
//
// The inbound half (applying a remote write back into the model under the
// m_applying guard) is Task 8. Nothing here writes to a model from the wire.
//
// iPhone app Task 72 (R-IOS-02): a local change, which is nobody's write,
// reaches every device's MirrorView as well as propertiesChanged().
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: iPhone app plan Task 72 (R-IOS-02): a local change reaches
//               every view. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QMetaProperty>
#include <QVariant>

#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorSchema.h"
#include "core/session/MirrorView.h"
#include "core/session/StateMirror.h"
#include "models/Band.h"
#include "models/MeterModel.h"
#include "models/PanadapterModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Flattens every batch StateMirror emits into one list, keeping the batch
// boundaries alongside so tests can assert "one notifier fired, two
// properties came out of it".
class Collector : public QObject {
public:
    explicit Collector(StateMirror* mirror)
    {
        connect(mirror, &StateMirror::propertiesChanged, this,
                [this](const QByteArray& key, const QList<MirrorUpdate>& ups) {
                    batches.append(qMakePair(key, ups));
                    for (const MirrorUpdate& u : ups) {
                        flat.append(qMakePair(key, u));
                    }
                });
    }

    void clear()
    {
        batches.clear();
        flat.clear();
    }

    // Value forwarded for `name`, or an invalid QVariant when absent.
    QVariant valueOf(const char* name) const
    {
        for (const auto& e : flat) {
            if (e.second.name == name) { return e.second.value; }
        }
        return QVariant();
    }

    bool sawProperty(const char* name) const { return valueOf(name).isValid(); }

    int countOf(const char* name) const
    {
        int n = 0;
        for (const auto& e : flat) {
            if (e.second.name == name) { ++n; }
        }
        return n;
    }

    QList<QPair<QByteArray, QList<MirrorUpdate>>> batches;
    QList<QPair<QByteArray, MirrorUpdate>> flat;
};

} // namespace

class TestMirrorForwarder : public QObject {
    Q_OBJECT

private slots:

    // ── The basic outbound path ───────────────────────────────────────────

    void watchForwardsASinglePropertyChange()
    {
        SliceModel slice(0);
        StateMirror mirror;
        Collector c(&mirror);

        QVERIFY(mirror.watch("slice:0", &slice));
        QVERIFY(mirror.isWatching("slice:0"));
        QCOMPARE(mirror.watchedKeys(), QList<QByteArray>{ "slice:0" });

        slice.setFrequency(14200000.0);

        QCOMPARE(c.countOf("frequency"), 1);
        QCOMPARE(c.valueOf("frequency").toDouble(), 14200000.0);
        QCOMPARE(c.flat.first().first, QByteArray("slice:0"));
    }

    void watchIsIdempotentAndRejectsNulls()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(!mirror.watch("slice:0", nullptr));
        QVERIFY(!mirror.watch(QByteArray(), &slice));
        QVERIFY(mirror.watch("slice:0", &slice));
        QVERIFY(mirror.watch("slice:0", &slice)); // same object, same key: fine
        QCOMPARE(mirror.watchedKeys().size(), 1);
    }

    void watchRejectsClassesTheMirrorDoesNotCarry()
    {
        MeterModel meter;
        StateMirror mirror;
        QVERIFY2(!mirror.watch("meter:0", &meter),
                 "MeterModel is excluded from the mirrored surface");
        QVERIFY(!mirror.isWatching("meter:0"));
    }

    // ── Arity 2: the go/no-go case, end to end ────────────────────────────
    //
    // filterLow and filterHigh share filterChanged(int, int). One emission
    // must produce one batch carrying BOTH ordinals with their own values --
    // asymmetric on purpose, so an implementation that confused the two
    // argument positions (or reused arg 0 for both) fails here.
    void arityTwoNotifierForwardsBothPropertiesInOneBatch()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        Collector c(&mirror);

        slice.setFilter(-2700, -300);

        QCOMPARE(c.batches.size(), 1);
        QCOMPARE(c.batches.first().second.size(), 2);
        QCOMPARE(c.valueOf("filterLow").toLongLong(), -2700LL);
        QCOMPARE(c.valueOf("filterHigh").toLongLong(), -300LL);
        QCOMPARE(slice.filterLow(), -2700);
        QCOMPARE(slice.filterHigh(), -300);
    }

    // ── Arity 0: a notifier with nothing to take ──────────────────────────
    //
    // PanadapterModel::levelChanged() names dBmFloor AND dBmCeiling and
    // carries no arguments, so re-reading is the only mechanism available.
    void arityZeroSharedNotifierForwardsEveryPropertyItNames()
    {
        PanadapterModel pan;
        pan.setdBmCeiling(-20);
        StateMirror mirror;
        QVERIFY(mirror.watch("pan:0", &pan));
        Collector c(&mirror);

        pan.setdBmFloor(-135);

        QCOMPARE(c.batches.size(), 1);
        QCOMPARE(c.batches.first().second.size(), 2);
        QCOMPARE(c.valueOf("dBmFloor").toLongLong(), -135LL);
        QCOMPARE(c.valueOf("dBmCeiling").toLongLong(), -20LL);
    }

    // ── Re-read, never the signal argument ────────────────────────────────
    //
    // One setDspMode call changes three properties across two signals. Every
    // forwarded value must equal what QMetaProperty::read returns afterwards.
    void valuesAreReReadRatherThanTakenFromSignalArguments()
    {
        SliceModel slice(0);
        slice.setDspMode(DSPMode::LSB);
        slice.setFilter(-2700, -300);

        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        Collector c(&mirror);

        slice.setDspMode(DSPMode::CWU);

        QVERIFY(c.sawProperty("dspMode"));
        QCOMPARE(c.valueOf("dspMode").toLongLong(),
                 static_cast<qlonglong>(DSPMode::CWU));

        // setDspMode's per-(band, mode) LastFilter block rewrote both edges
        // and emitted filterChanged separately; the forwarder must report the
        // post-change values, not the pre-change ones it never saw.
        QVERIFY(c.sawProperty("filterLow"));
        QVERIFY(c.sawProperty("filterHigh"));
        QCOMPARE(c.valueOf("filterLow").toLongLong(),
                 static_cast<qlonglong>(slice.filterLow()));
        QCOMPARE(c.valueOf("filterHigh").toLongLong(),
                 static_cast<qlonglong>(slice.filterHigh()));
        QVERIFY2(slice.filterLow() != -2700 || slice.filterHigh() != -300,
                 "setDspMode is expected to move the filter edges; if it "
                 "stopped doing so, this test no longer proves re-read");
    }

    // Tasks 10 and 11 depend on this positively, not just as a caveat: one
    // frequency write can legitimately produce TWO deltas, because
    // SliceModel::setFrequency runs a Band::bandFromFrequency boundary
    // check and emits bandChanged separately when the move crosses a band.
    // A consumer that assumed one delta per operator action, or that
    // rebuilt band from frequency itself, would drift.
    void aBandCrossingFrequencyWriteAlsoForwardsBand()
    {
        SliceModel slice(0);
        slice.setFrequency(14200000.0); // 20m
        QCOMPARE(slice.band(), Band::Band20m);

        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        Collector c(&mirror);

        slice.setFrequency(7100000.0); // 40m: crosses a band boundary

        QCOMPARE(c.countOf("frequency"), 1);
        QCOMPARE(c.valueOf("frequency").toDouble(), 7100000.0);
        QVERIFY2(c.sawProperty("band"),
                 "a band-crossing frequency write must forward band too");
        QCOMPARE(c.valueOf("band").toLongLong(),
                 static_cast<qlonglong>(Band::Band40m));
        QCOMPARE(slice.band(), Band::Band40m);

        // ...and a move WITHIN one band forwards frequency alone, so the
        // band delta really is driven by the crossing and not emitted
        // unconditionally alongside every tune.
        c.clear();
        slice.setFrequency(7150000.0);
        QCOMPARE(c.countOf("frequency"), 1);
        QCOMPARE(c.countOf("band"), 0);
    }

    void enumsAreForwardedAsTheirUnderlyingIntegers()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        Collector c(&mirror);

        slice.setAgcMode(AGCMode::Fast);

        QCOMPARE(c.valueOf("agcMode").toLongLong(),
                 static_cast<qlonglong>(AGCMode::Fast));
        for (const auto& e : c.flat) {
            if (e.second.name == "agcMode") {
                QCOMPARE(e.second.kind, MirrorWireKind::Enum);
            }
        }
    }

    // ── UniqueConnection ──────────────────────────────────────────────────
    //
    // Every property asks for its notifier by name, so filterChanged is asked
    // for twice on one walk and a re-watch asks for the whole set again.
    // Neither may double the emissions.
    void repeatedWatchDoesNotDoubleEmit()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        QVERIFY(mirror.watch("slice:0", &slice));
        QVERIFY(mirror.watch("slice:0", &slice));
        Collector c(&mirror);

        slice.setFilter(-2700, -300);

        QCOMPARE(c.batches.size(), 1);
        QCOMPARE(c.countOf("filterLow"), 1);
        QCOMPARE(c.countOf("filterHigh"), 1);
    }

    // ── Object identity ───────────────────────────────────────────────────

    // One slot serves every watched object, so the record lookup by
    // sender() is the only thing keeping two slices' state apart.
    //
    // Note the band update riding along: SliceModel::setFrequency runs a
    // Band::bandFromFrequency boundary check and emits bandChanged when the
    // move crosses a band, so a single frequency write legitimately produces
    // two deltas. Asserting a count here would be asserting a fact about
    // SliceModel rather than about the mirror.
    void updatesCarryTheKeyOfTheirOwnObject()
    {
        SliceModel a(0);
        SliceModel b(1);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &a));
        QVERIFY(mirror.watch("slice:1", &b));
        Collector c(&mirror);

        b.setFrequency(7100000.0);

        QVERIFY(!c.flat.isEmpty());
        for (const auto& e : c.flat) {
            QVERIFY2(e.first == QByteArray("slice:1"),
                     "every update must carry the key of the object that "
                     "produced it");
        }
        QCOMPARE(c.countOf("frequency"), 1);
        QCOMPARE(c.valueOf("frequency").toDouble(), 7100000.0);

        // The untouched slice must produce nothing at all.
        c.clear();
        QVERIFY(c.flat.isEmpty());
        a.setFrequency(14200000.0);
        for (const auto& e : c.flat) {
            QCOMPARE(e.first, QByteArray("slice:0"));
        }
        QCOMPARE(c.countOf("frequency"), 1);
    }

    void unwatchStopsForwarding()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        Collector c(&mirror);

        mirror.unwatch("slice:0");
        QVERIFY(!mirror.isWatching("slice:0"));
        slice.setFrequency(14200000.0);
        QVERIFY(c.flat.isEmpty());

        // ...and can be resumed.
        QVERIFY(mirror.watch("slice:0", &slice));
        slice.setFrequency(14250000.0);
        QCOMPARE(c.countOf("frequency"), 1);
    }

    // A slice removed from RadioModel is deleted underneath the mirror; the
    // watch record has to go with it or the next lookup dereferences it.
    void destroyedObjectIsAutomaticallyUnwatched()
    {
        StateMirror mirror;
        {
            auto slice = std::make_unique<SliceModel>(0);
            QVERIFY(mirror.watch("slice:0", slice.get()));
            QVERIFY(mirror.isWatching("slice:0"));
        }
        QVERIFY(!mirror.isWatching("slice:0"));
        QVERIFY(mirror.watchedKeys().isEmpty());
    }

    // One object under two keys must be refused outright.
    //
    // Accepting it appends two Watch records for one pointer, and every
    // lookup is first-match, so the failures escalate: the second key
    // forwards nothing while isWatching() reports true; unwatch() of the
    // first key disconnects the object wholesale and leaves the second
    // permanently mute; and destruction removes only the first record, so
    // the second retains a dangling pointer that snapshot() then reads.
    void oneObjectUnderTwoKeysIsRefused()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("radio", &slice));
        QVERIFY2(!mirror.watch("radio:0", &slice),
                 "the same object must not be watchable under a second key");
        QCOMPARE(mirror.watchedKeys(), QList<QByteArray>{ "radio" });
        QVERIFY(!mirror.isWatching("radio:0"));
        QVERIFY(mirror.snapshot("radio:0").isEmpty());

        // The original binding is untouched by the refusal.
        Collector c(&mirror);
        slice.setFrequency(14200000.0);
        QCOMPARE(c.countOf("frequency"), 1);
        QCOMPARE(c.flat.first().first, QByteArray("radio"));
    }

    // The use-after-free this guards. Without both halves of the fix, the
    // second record survives destruction holding a freed pointer and
    // snapshot() dereferences it. Run under ASan this is a hard failure;
    // even without, watchedKeys() proves no record outlives the object.
    void noWatchRecordSurvivesItsObject()
    {
        StateMirror mirror;
        {
            auto slice = std::make_unique<SliceModel>(0);
            QVERIFY(mirror.watch("radio", slice.get()));
            // Refused, but assert on the state rather than the return value
            // so this still catches the leak if the refusal is ever relaxed.
            mirror.watch("radio:0", slice.get());
        }
        QVERIFY2(mirror.watchedKeys().isEmpty(),
                 "every record naming a destroyed object must be removed");
        QVERIFY(!mirror.isWatching("radio"));
        QVERIFY(!mirror.isWatching("radio:0"));
        // Reads through any surviving record would touch freed memory.
        QVERIFY(mirror.snapshot("radio").isEmpty());
        QVERIFY(mirror.snapshot("radio:0").isEmpty());
        QVERIFY(mirror.snapshotAll().isEmpty());
    }

    void unwatchAllClearsEveryRecord()
    {
        SliceModel a(0);
        PanadapterModel pan;
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &a));
        QVERIFY(mirror.watch("pan:0", &pan));
        QCOMPARE(mirror.watchedKeys().size(), 2);
        mirror.unwatchAll();
        QVERIFY(mirror.watchedKeys().isEmpty());

        Collector c(&mirror);
        a.setFrequency(14200000.0);
        QVERIFY(c.flat.isEmpty());
    }

    // ── The snapshot path ─────────────────────────────────────────────────
    //
    // CONSTANT properties have no NOTIFY, so the notify-enumerating watcher
    // never reaches them. sliceIndex is the mirror's object identity: without
    // an explicit snapshot read, every object.create on the wire is anonymous.
    void snapshotReachesConstantPropertiesTheWatcherCannot()
    {
        SliceModel slice(3);
        slice.setFrequency(14200000.0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:3", &slice));

        const QList<MirrorUpdate> snap = mirror.snapshot("slice:3");
        const MirrorSchema& schema = MirrorSchema::forObject(&slice);
        QCOMPARE(snap.size(), schema.size());

        bool sawSliceIndex = false;
        for (const MirrorUpdate& u : snap) {
            if (u.name == "sliceIndex") {
                sawSliceIndex = true;
                QCOMPARE(u.value.toLongLong(), 3LL);
            }
        }
        QVERIFY2(sawSliceIndex,
                 "sliceIndex is CONSTANT and must still be in the snapshot");

        // ...and the notifier path genuinely cannot produce it, which is why
        // the explicit read is needed rather than merely convenient.
        Collector c(&mirror);
        slice.setFrequency(14250000.0);
        QVERIFY(!c.sawProperty("sliceIndex"));
    }

    void snapshotOfAnUnwatchedKeyIsEmpty()
    {
        StateMirror mirror;
        QVERIFY(mirror.snapshot("slice:0").isEmpty());
    }

    void snapshotAllCoversEveryWatchedObject()
    {
        SliceModel a(0);
        PanadapterModel pan;
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &a));
        QVERIFY(mirror.watch("pan:0", &pan));

        const QList<QPair<QByteArray, QList<MirrorUpdate>>> all =
            mirror.snapshotAll();
        QCOMPARE(all.size(), 2);

        QSet<QByteArray> keys;
        for (const auto& e : all) {
            keys.insert(e.first);
            QCOMPARE(e.second.size(),
                     MirrorSchema::forMetaObject(
                         e.first.startsWith("slice")
                             ? &SliceModel::staticMetaObject
                             : &PanadapterModel::staticMetaObject).size());
        }
        QCOMPARE(keys, (QSet<QByteArray>{ "slice:0", "pan:0" }));
    }

    // ── Direction is an INBOUND gate only ─────────────────────────────────
    //
    // An Outbound-only property is exactly the case where the daemon must
    // keep telling the GUI what it is; suppressing it outbound would leave a
    // remote GUI unable to grey a PS-paused pan.
    void outboundOnlyPropertiesAreStillForwardedOutbound()
    {
        QCOMPARE(MirrorPolicy::directionFor("SliceModel", "psPaused"),
                 MirrorDirection::Outbound);

        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        Collector c(&mirror);

        slice.setPsPaused(true);

        QCOMPARE(c.countOf("psPaused"), 1);
        QCOMPARE(c.valueOf("psPaused").toBool(), true);
    }

    // ── Every device's view (iPhone app Task 72) ─────────────────────────
    //
    // A change nobody wrote (the Core's own, or the operator's at the Core)
    // is every device's to hear: each attached view gets it, re-read at
    // flush, and propertiesChanged() still fires for local direct mode.
    void aLocalChangeReachesEveryAttachedView()
    {
        SliceModel slice(0);
        StateMirror mirror;
        QVERIFY(mirror.watch("slice:0", &slice));
        Collector c(&mirror);
        QList<SessionMessage> toA;
        QList<SessionMessage> toB;
        MirrorView viewA(&mirror, [&toA](const SessionMessage& m) { toA.append(m); });
        MirrorView viewB(&mirror, [&toB](const SessionMessage& m) { toB.append(m); });
        viewA.attach();
        viewB.attach();
        toA.clear();
        toB.clear();

        slice.setFrequency(7123000.0);

        QCOMPARE(c.countOf("frequency"), 1);
        QCOMPARE(viewA.flush(), 1);
        QCOMPARE(viewB.flush(), 1);
        for (const QList<SessionMessage>* got : {&toA, &toB}) {
            QCOMPARE(got->size(), 1);
            QCOMPARE(got->first().kind, SessionMessageKind::Delta);
            QCOMPARE(got->first().objectKey, QByteArray("slice:0"));
            bool found = false;
            for (const MirrorUpdate& u : got->first().updates) {
                if (u.name == "frequency") {
                    found = true;
                    QCOMPARE(u.value.toDouble(), 7123000.0);
                }
            }
            QVERIFY(found);
        }
    }
};

QTEST_MAIN(TestMirrorForwarder)
#include "tst_mirror_forwarder.moc"
