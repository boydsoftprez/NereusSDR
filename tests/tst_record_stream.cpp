// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_record_stream.cpp  (NereusSDR)
// =================================================================
//
// Parity Task 19 (R-IOS-25, R-R3-49; the iPhone app plan's Task 21 station
// half): the link's record streams.
//
//   - A subscriber gets the backlog (the newest records, up to what it
//     asked for and the stream's capacity), then only what changes.
//   - Changes merge by id between sends; a record added and removed
//     between sends is never sent.
//   - An unsubscribed peer is sent nothing.
//   - A slow peer's waiting changes never grow past the stream's capacity:
//     they turn into a reset carrying only the newest records.
//   - A reset starts a new generation and replaces every subscriber's copy.
//   - The record.batch message round-trips through the session codec.
//
//   cmake --build build --target tst_record_stream
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_record_stream$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include <QJsonObject>

#include "core/session/RecordStream.h"
#include "core/session/SessionMessages.h"

using namespace NereusSDR;

namespace {

QJsonObject fields(int n)
{
    return QJsonObject{{QStringLiteral("call"), QStringLiteral("K%1ABC").arg(n)}};
}

QStringList ids(const QList<RecordUpsert>& upserts)
{
    QStringList out;
    for (const RecordUpsert& u : upserts) {
        out.append(u.id);
    }
    return out;
}

int peerA = 0;
int peerB = 0;

} // namespace

class TstRecordStream : public QObject {
    Q_OBJECT

private slots:
    void backlogIsTheNewestUpToTheAskAndTheCapacity()
    {
        RecordStream stream(QStringLiteral("spots"), 5);
        for (int i = 1; i <= 8; ++i) {
            stream.upsert(QString::number(i), fields(i));
        }
        QCOMPARE(stream.size(), 5);
        QVERIFY(!stream.contains(QStringLiteral("3")));

        const RecordBatch three = stream.subscribe(&peerA, 3);
        QVERIFY(three.reset);
        QCOMPARE(three.stream, QStringLiteral("spots"));
        QCOMPARE(three.generation, quint64(1));
        QCOMPARE(ids(three.upserts), (QStringList{"6", "7", "8"}));

        const RecordBatch all = stream.subscribe(&peerB, 500);
        QCOMPARE(ids(all.upserts), (QStringList{"4", "5", "6", "7", "8"}));
    }

    void liveChangesMergeByIdAndGoOnlyToSubscribers()
    {
        RecordStream stream(QStringLiteral("spots"), 10);
        stream.upsert(QStringLiteral("1"), fields(1));
        (void)stream.subscribe(&peerA, 10);

        stream.upsert(QStringLiteral("2"), fields(2));
        stream.upsert(QStringLiteral("1"), fields(11)); // a change
        stream.upsert(QStringLiteral("3"), fields(3));
        stream.remove(QStringLiteral("3"));               // never sent: removed anyway
        stream.upsert(QStringLiteral("2"), fields(22));   // merged

        const auto pending = stream.takePending();
        QCOMPARE(pending.size(), 1);
        QCOMPARE(pending.first().first, static_cast<RecordStream::Subscriber>(&peerA));
        const RecordBatch& batch = pending.first().second;
        QVERIFY(!batch.reset);
        QCOMPARE(ids(batch.upserts), (QStringList{"1", "2"}));
        QCOMPARE(batch.upserts.at(0).fields, fields(11));
        QCOMPARE(batch.upserts.at(1).fields, fields(22));
        // The window ignores a remove for an id it never held.
        QCOMPARE(batch.removes, QStringList{QStringLiteral("3")});

        // Nothing owed now.
        QVERIFY(stream.takePending().isEmpty());

        stream.remove(QStringLiteral("1"));
        const auto removed = stream.takePending();
        QCOMPARE(removed.size(), 1);
        QCOMPARE(removed.first().second.removes, QStringList{QStringLiteral("1")});
    }

    void aRemoveAfterAWaitingUpdateReachesThePeer()
    {
        // Fix wave, I4: the peer holds record 1; an update of it waits, then
        // the record goes. The peer must be told to remove it.
        RecordStream stream(QStringLiteral("spots"), 10);
        stream.upsert(QStringLiteral("1"), fields(1));
        const RecordBatch first = stream.subscribe(&peerA, 10);
        QCOMPARE(ids(first.upserts), QStringList{QStringLiteral("1")});
        stream.upsert(QStringLiteral("1"), fields(11));
        stream.remove(QStringLiteral("1"));
        const auto pending = stream.takePending();
        QCOMPARE(pending.size(), 1);
        const RecordBatch& batch = pending.first().second;
        QVERIFY(batch.upserts.isEmpty());
        QCOMPARE(batch.removes, QStringList{QStringLiteral("1")});
    }

    void anUnsubscribedPeerGetsNothing()
    {
        RecordStream stream(QStringLiteral("spots"), 10);
        (void)stream.subscribe(&peerA, 10);
        stream.unsubscribe(&peerA);
        QVERIFY(!stream.isSubscribed(&peerA));
        stream.upsert(QStringLiteral("1"), fields(1));
        stream.remove(QStringLiteral("1"));
        stream.reset();
        QVERIFY(stream.takePending().isEmpty());
    }

    void aSlowPeerIsBoundedAndGetsTheNewestAsAReset()
    {
        RecordStream stream(QStringLiteral("spots"), 4);
        (void)stream.subscribe(&peerA, 3);
        for (int i = 1; i <= 50; ++i) {
            stream.upsert(QString::number(i), fields(i));
            QVERIFY(stream.pendingCountForTest(&peerA) <= stream.capacity());
        }
        QVERIFY(stream.pendingResetForTest(&peerA));
        const auto pending = stream.takePending();
        QCOMPARE(pending.size(), 1);
        const RecordBatch& batch = pending.first().second;
        QVERIFY(batch.reset);
        QCOMPARE(ids(batch.upserts), (QStringList{"48", "49", "50"}));
        QVERIFY(batch.removes.isEmpty());
        // Back to changes afterwards.
        stream.upsert(QStringLiteral("51"), fields(51));
        const auto next = stream.takePending();
        QCOMPARE(next.size(), 1);
        QVERIFY(!next.first().second.reset);
    }

    void aResetIsANewGenerationForEverySubscriber()
    {
        RecordStream stream(QStringLiteral("spotConsole:dxCluster"), 200);
        stream.upsert(QStringLiteral("1"), fields(1));
        (void)stream.subscribe(&peerA, 200);
        (void)stream.subscribe(&peerB, 200);
        stream.reset();
        stream.upsert(QStringLiteral("2"), fields(2));
        QCOMPARE(stream.generation(), quint64(2));
        const auto pending = stream.takePending();
        QCOMPARE(pending.size(), 2);
        for (const auto& [peer, batch] : pending) {
            Q_UNUSED(peer);
            QVERIFY(batch.reset);
            QCOMPARE(batch.generation, quint64(2));
            QCOMPARE(ids(batch.upserts), QStringList{QStringLiteral("2")});
        }
    }

    void theBatchRoundTripsOnTheWire()
    {
        RecordBatch batch;
        batch.stream = QStringLiteral("spots");
        batch.generation = 7;
        batch.reset = false;
        batch.upserts = {{QStringLiteral("12"), fields(12)}};
        batch.removes = {QStringLiteral("9")};
        const QByteArray wire = SessionMessages::encode(SessionMessages::recordBatch(batch));
        SessionMessage decoded;
        QVERIFY(SessionMessages::decode(wire, &decoded));
        QCOMPARE(decoded.kind, SessionMessageKind::RecordBatch);
        QCOMPARE(decoded.recordBatch.stream, batch.stream);
        QCOMPARE(decoded.recordBatch.generation, batch.generation);
        QCOMPARE(decoded.recordBatch.reset, false);
        QCOMPARE(decoded.recordBatch.upserts, batch.upserts);
        QCOMPARE(decoded.recordBatch.removes, batch.removes);

        // A batch missing its generation, or with an upsert with no id, is
        // not a batch.
        SessionMessage bad;
        QVERIFY(!SessionMessages::decode(
            QByteArrayLiteral(R"({"type":"record.batch","stream":"spots","reset":false,)"
                              R"("upserts":[],"removes":[]})"),
            &bad));
        QVERIFY(!SessionMessages::decode(
            QByteArrayLiteral(R"({"type":"record.batch","stream":"spots","generation":1,)"
                              R"("reset":false,"upserts":[{"fields":{}}],"removes":[]})"),
            &bad));
    }
};

QTEST_GUILESS_MAIN(TstRecordStream)
#include "tst_record_stream.moc"
