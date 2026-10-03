// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_mirror_view.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 72 (R-IOS-02; the several-devices design, rulings
// 5.6 and 5.7): a MirrorView per device over one StateMirror.
//
// Echo per writer first, since it decides what every device receives:
//   - device A changes a shared noise blanker (two slices co-hosted on one
//     receiver, so RadioModel mirrors the blanker to the other slice as a
//     side effect): B's view gets both deltas, A's view gets neither;
//   - the RED CHECK: made global again (the one m_applying check the
//     shared mirror had), B misses A's change. Run once by hand for the
//     report, see task-72-report.md;
//   - a change made under WriterScope counts as that device's write (the
//     change a confirm.proceed applies, Task 74);
//   - a write with no view reaches every view; the single-session API
//     (attachSession, three-argument applyInbound) still withholds the
//     write from its own session.
// Then each view's own coalescer and burst: one device attaching leaves
// another's pending deltas in place; the burst goes to the attaching view
// alone and carries current values; a view collects nothing before it
// attaches and nothing after it closes; a pending property of an object
// no longer watched is dropped at flush.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 72 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QLoggingCategory>

#include <memory>

#include "core/WdspTypes.h"
#include "core/session/MirrorView.h"
#include "core/session/SessionMessages.h"
#include "core/session/StateMirror.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Everything one view sent, in order.
struct Received {
    QList<SessionMessage> messages;

    MirrorView::Sink sink()
    {
        return [this](const SessionMessage& message) { messages.append(message); };
    }

    QList<SessionMessage> ofKind(SessionMessageKind kind) const
    {
        QList<SessionMessage> out;
        for (const SessionMessage& m : messages) {
            if (m.kind == kind) {
                out.append(m);
            }
        }
        return out;
    }

    // The value `property` of `key` carries in the deltas received, or an
    // invalid QVariant when none carried it.
    QVariant deltaValue(const QByteArray& key, const QByteArray& property) const
    {
        QVariant value;
        for (const SessionMessage& m : ofKind(SessionMessageKind::Delta)) {
            if (m.objectKey != key) {
                continue;
            }
            for (const MirrorUpdate& u : m.updates) {
                if (u.name == property) {
                    value = u.value;
                }
            }
        }
        return value;
    }

    QVariant createValue(const QByteArray& key, const QByteArray& property) const
    {
        for (const SessionMessage& m : ofKind(SessionMessageKind::ObjectCreate)) {
            if (m.objectKey != key) {
                continue;
            }
            for (const MirrorUpdate& u : m.updates) {
                if (u.name == property) {
                    return u.value;
                }
            }
        }
        return {};
    }
};

// Two slices co-hosted on one receiver, as tst_mirror_inbound's
// peerNbModeMirrorAppliesLocallyButIsNotForwarded sets them up: 10 kHz
// apart inside one stream's window, so they share one physical blanker.
struct SharedReceiver {
    RadioModel model;
    SliceModel* a = nullptr;
    SliceModel* b = nullptr;
    StateMirror mirror;

    SharedReceiver()
    {
        model.configureStreamPool(5, 5, 192000);
        const int first = model.addSlice();
        const int second = model.addSlice();
        a = model.sliceById(first);
        b = model.sliceById(second);
        a->setFrequency(14200000.0);
        b->setFrequency(14210000.0);
        mirror.watch("slice:0", a);
        mirror.watch("slice:1", b);
    }
};

int asInt(const QVariant& v)
{
    return v.toInt();
}

} // namespace

class TstMirrorView : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // RadioModel's own placement and start-up logging is not this
        // test's subject.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus*=false\nnereussdr*=false"));
    }

    // ── Echo per writer (ruling 5.7) ─────────────────────────────────────

    void aSharedBlankerChangeReachesTheOtherDeviceNotTheWriter()
    {
        SharedReceiver r;
        QCOMPARE(r.b->streamIndex(), r.a->streamIndex());
        QCOMPARE(r.a->nbMode(), NbMode::Off);
        QCOMPARE(r.b->nbMode(), NbMode::Off);

        Received gotA;
        Received gotB;
        MirrorView viewA(&r.mirror, gotA.sink());
        MirrorView viewB(&r.mirror, gotB.sink());
        viewA.attach();
        viewB.attach();
        gotA.messages.clear();
        gotB.messages.clear();

        const MirrorApplyResult result = r.mirror.applyInbound(
            "slice:0", "nbMode", QVariant(static_cast<int>(NbMode::NB)), &viewA);
        QVERIFY2(result.accepted, qPrintable(result.reason));
        // The write landed, and RadioModel mirrored it to the co-hosted
        // slice (the one physical blanker).
        QCOMPARE(r.a->nbMode(), NbMode::NB);
        QCOMPARE(r.b->nbMode(), NbMode::NB);

        viewA.flush();
        viewB.flush();

        // B: the written property and the side effect on the other slice.
        QCOMPARE(asInt(gotB.deltaValue("slice:0", "nbMode")), static_cast<int>(NbMode::NB));
        QCOMPARE(asInt(gotB.deltaValue("slice:1", "nbMode")), static_cast<int>(NbMode::NB));
        // A: neither; its own write is answered elsewhere (property.result).
        QVERIFY2(gotA.ofKind(SessionMessageKind::Delta).isEmpty(),
                 "the writer's own view must not echo its write or its side effects");
    }

    void theWritersViewHearsLaterChangesAgain()
    {
        SharedReceiver r;
        Received gotA;
        MirrorView viewA(&r.mirror, gotA.sink());
        viewA.attach();
        gotA.messages.clear();

        r.mirror.applyInbound("slice:0", "nbMode", QVariant(static_cast<int>(NbMode::NB)),
                              &viewA);
        QCOMPARE(viewA.pendingPropertyCount(), 0);

        r.a->setFrequency(14205000.0); // a later, ordinary change
        QVERIFY(viewA.pendingPropertyCount() > 0);
        viewA.flush();
        QCOMPARE(gotA.deltaValue("slice:0", "frequency").toDouble(), 14205000.0);
    }

    void aWriterScopeCountsAsThatDevicesWrite()
    {
        SharedReceiver r;
        Received gotA;
        Received gotB;
        MirrorView viewA(&r.mirror, gotA.sink());
        MirrorView viewB(&r.mirror, gotB.sink());
        viewA.attach();
        viewB.attach();
        gotA.messages.clear();
        gotB.messages.clear();
        QSignalSpy local(&r.mirror, &StateMirror::propertiesChanged);

        {
            // A change applied another way (confirm.proceed, Task 74), as
            // A's write.
            StateMirror::WriterScope scope(r.mirror, &viewA);
            r.a->setNbMode(NbMode::NB);
        }
        viewA.flush();
        viewB.flush();

        QVERIFY(gotA.ofKind(SessionMessageKind::Delta).isEmpty());
        QCOMPARE(asInt(gotB.deltaValue("slice:0", "nbMode")), static_cast<int>(NbMode::NB));
        QCOMPARE(asInt(gotB.deltaValue("slice:1", "nbMode")), static_cast<int>(NbMode::NB));
        QCOMPARE(local.count(), 0);
    }

    void aNestedScopeRestoresItsCallersWriter()
    {
        SharedReceiver r;
        Received gotA;
        Received gotB;
        MirrorView viewA(&r.mirror, gotA.sink());
        MirrorView viewB(&r.mirror, gotB.sink());
        viewA.attach();
        viewB.attach();
        gotA.messages.clear();
        gotB.messages.clear();

        {
            StateMirror::WriterScope outer(r.mirror, &viewA);
            {
                StateMirror::WriterScope inner(r.mirror, &viewB);
            }
            // Still A's write after the nested one closed.
            r.a->setFrequency(14207000.0);
        }
        viewA.flush();
        viewB.flush();
        QVERIFY(gotA.ofKind(SessionMessageKind::Delta).isEmpty());
        QCOMPARE(gotB.deltaValue("slice:0", "frequency").toDouble(), 14207000.0);
    }

    void aWriteWithNoViewReachesEveryView()
    {
        SharedReceiver r;
        Received gotA;
        Received gotB;
        MirrorView viewA(&r.mirror, gotA.sink());
        MirrorView viewB(&r.mirror, gotB.sink());
        viewA.attach();
        viewB.attach();
        gotA.messages.clear();
        gotB.messages.clear();

        r.mirror.applyInbound("slice:0", "nbMode", QVariant(static_cast<int>(NbMode::NB)),
                              nullptr);
        viewA.flush();
        viewB.flush();
        QCOMPARE(asInt(gotA.deltaValue("slice:1", "nbMode")), static_cast<int>(NbMode::NB));
        QCOMPARE(asInt(gotB.deltaValue("slice:1", "nbMode")), static_cast<int>(NbMode::NB));
    }

    void theSingleSessionApiStillWithholdsItsOwnWrite()
    {
        // attachSession() and the three-argument applyInbound() are one
        // session, as before Task 72; a device view beside it still hears.
        SharedReceiver r;
        QList<SessionMessage> single;
        connect(&r.mirror, &StateMirror::sessionMessageReady, this,
                [&single](const SessionMessage& m) { single.append(m); });
        r.mirror.attachSession();
        QVERIFY(r.mirror.hasAttachedSession());
        Received gotB;
        MirrorView viewB(&r.mirror, gotB.sink());
        viewB.attach();
        single.clear();
        gotB.messages.clear();

        r.mirror.applyInbound("slice:0", "nbMode", QVariant(static_cast<int>(NbMode::NB)));
        QCOMPARE(r.mirror.flushCoalescedDeltas(), 0);
        QVERIFY(single.isEmpty());
        viewB.flush();
        QCOMPARE(asInt(gotB.deltaValue("slice:1", "nbMode")), static_cast<int>(NbMode::NB));
    }

    // ── A coalescer and a burst per view (ruling 5.6) ────────────────────

    void oneDeviceAttachingKeepsAnothersPendingDeltas()
    {
        SharedReceiver r;
        Received gotB;
        MirrorView viewB(&r.mirror, gotB.sink());
        viewB.attach();
        gotB.messages.clear();

        r.a->setFrequency(14215000.0);
        r.b->setAfGain(33);
        const int pendingBefore = viewB.pendingPropertyCount();
        QVERIFY(pendingBefore >= 2);

        // A attaches: its burst is its own and holds the current values.
        Received gotA;
        MirrorView viewA(&r.mirror, gotA.sink());
        viewA.attach();
        QCOMPARE(gotA.createValue("slice:0", "frequency").toDouble(), 14215000.0);
        QCOMPARE(gotA.createValue("slice:1", "afGain").toInt(), 33);
        QCOMPARE(gotA.ofKind(SessionMessageKind::SnapshotComplete).size(), 1);

        // B saw none of A's burst, and still has everything it had pending.
        QVERIFY(gotB.messages.isEmpty());
        QCOMPARE(viewB.pendingPropertyCount(), pendingBefore);
        viewB.flush();
        QCOMPARE(gotB.deltaValue("slice:0", "frequency").toDouble(), 14215000.0);
        QCOMPARE(gotB.deltaValue("slice:1", "afGain").toInt(), 33);
    }

    void theBurstIsSchemasThenCreatesThenTheMarker()
    {
        SharedReceiver r;
        Received got;
        MirrorView view(&r.mirror, got.sink());
        view.attach();
        QCOMPARE(got.messages.size(), 4);
        QCOMPARE(got.messages.at(0).kind, SessionMessageKind::Schema);
        QCOMPARE(got.messages.at(1).kind, SessionMessageKind::ObjectCreate);
        QCOMPARE(got.messages.at(1).objectKey, QByteArray("slice:0"));
        QCOMPARE(got.messages.at(2).objectKey, QByteArray("slice:1"));
        QCOMPARE(got.messages.at(3).kind, SessionMessageKind::SnapshotComplete);
    }

    void aChangeDuringTheBurstArrivesAfterTheMarker()
    {
        SharedReceiver r;
        Received got;
        bool reacted = false;
        MirrorView view(&r.mirror, [&](const SessionMessage& m) {
            got.messages.append(m);
            if (!reacted && m.kind == SessionMessageKind::ObjectCreate) {
                reacted = true;
                r.b->setAfGain(21); // the receiver reacts synchronously
            }
        });
        view.attach();
        const int marker = [&got]() {
            for (int i = 0; i < got.messages.size(); ++i) {
                if (got.messages.at(i).kind == SessionMessageKind::SnapshotComplete) {
                    return i;
                }
            }
            return -1;
        }();
        QVERIFY(marker >= 0);
        QCOMPARE(got.messages.size(), marker + 2);
        QCOMPARE(got.messages.last().kind, SessionMessageKind::Delta);
        QCOMPARE(got.deltaValue("slice:1", "afGain").toInt(), 21);
    }

    void aViewCollectsNothingBeforeItAttaches()
    {
        SharedReceiver r;
        Received got;
        MirrorView view(&r.mirror, got.sink());
        r.a->setFrequency(14220000.0);
        QCOMPARE(view.pendingPropertyCount(), 0);
        QVERIFY(!view.deliver(SessionMessages::snapshotComplete()));
        QVERIFY(got.messages.isEmpty());
    }

    void aClosedViewSendsNothingMore()
    {
        SharedReceiver r;
        Received got;
        MirrorView view(&r.mirror, got.sink());
        view.attach();
        r.a->setFrequency(14225000.0);
        QVERIFY(view.pendingPropertyCount() > 0);
        got.messages.clear();

        view.close();
        QVERIFY(view.isClosed());
        QCOMPARE(view.pendingPropertyCount(), 0);
        r.a->setFrequency(14226000.0);
        QCOMPARE(view.pendingPropertyCount(), 0);
        QCOMPARE(view.flush(), 0);
        QVERIFY(!view.deliver(SessionMessages::snapshotComplete()));
        view.attach();
        QVERIFY(got.messages.isEmpty());
    }

    void aDeliveredMessageGoesOnceAttached()
    {
        SharedReceiver r;
        Received got;
        MirrorView view(&r.mirror, got.sink());
        view.attach();
        got.messages.clear();
        QVERIFY(view.deliver(SessionMessages::settingsValue(QStringLiteral("K"),
                                                            QStringLiteral("1"),
                                                            QStringLiteral("origin-a"))));
        QCOMPARE(got.messages.size(), 1);
        QCOMPARE(got.messages.first().kind, SessionMessageKind::SettingsValue);
        QCOMPARE(got.messages.first().originTag, QStringLiteral("origin-a"));
    }

    void aPendingPropertyOfAnUnwatchedObjectIsDropped()
    {
        SharedReceiver r;
        Received got;
        MirrorView view(&r.mirror, got.sink());
        view.attach();
        got.messages.clear();
        r.b->setAfGain(44);
        r.a->setAfGain(45);
        r.mirror.unwatch("slice:1");
        view.flush();
        QVERIFY(!got.deltaValue("slice:1", "afGain").isValid());
        QCOMPARE(got.deltaValue("slice:0", "afGain").toInt(), 45);
    }

    void aDeletedViewLeavesTheMirror()
    {
        SharedReceiver r;
        Received gotB;
        MirrorView viewB(&r.mirror, gotB.sink());
        viewB.attach();
        {
            Received gotA;
            MirrorView viewA(&r.mirror, gotA.sink());
            viewA.attach();
        }
        gotB.messages.clear();
        r.a->setFrequency(14230000.0); // must not reach the deleted view
        viewB.flush();
        QCOMPARE(gotB.deltaValue("slice:0", "frequency").toDouble(), 14230000.0);
    }
};

QTEST_MAIN(TstMirrorView)
#include "tst_mirror_view.moc"
