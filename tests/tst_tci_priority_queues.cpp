// no-port-check: NereusSDR-original TDD tests for TciSendQueue.
// Phase 3J-1 Task 14.1 — three tests:
//   1. Drain order: Urgent before Binary before Control, FIFO within each tier.
//   2. Bounded overflow: oldest frame is dropped when capacity is exceeded.
//   3. Multi-thread safety: producer + consumer run concurrently; no torn reads,
//      no UB; drop count + drained count == total pushed.

#include <QtTest>
#include <QThread>
#include <atomic>

#include "core/TciSendQueue.h"

using namespace NereusSDR;

class TestTciPriorityQueues : public QObject {
    Q_OBJECT
private slots:
    void drain_order_urgent_first_then_binary_then_control();
    void bounded_overflow_drops_oldest();
    void multi_thread_no_drops_or_torn_reads();
};

// ── Test 1: drain order ───────────────────────────────────────────────────────
//
// Push Control, Binary, Urgent, Control (interleaved priorities).
// Expected drain order: u1 (Urgent), b1 (Binary), c1 (Control FIFO), c2.
// Mirrors Thetis tryDequeueNextOutboundFrameLocked at
// TCIServer.cs:1648-1679 [v2.10.3.13].
void TestTciPriorityQueues::drain_order_urgent_first_then_binary_then_control()
{
    TciSendQueue q(100);
    q.push(TciSendQueue::Priority::Control, QStringLiteral("c1"));
    q.push(TciSendQueue::Priority::Binary,  QStringLiteral("b1"));
    q.push(TciSendQueue::Priority::Urgent,  QStringLiteral("u1"));
    q.push(TciSendQueue::Priority::Control, QStringLiteral("c2"));

    QCOMPARE(q.size(), 4);
    QCOMPARE(q.dropCount(), 0);

    QString out;
    QVERIFY(q.tryPop(&out)); QCOMPARE(out, QStringLiteral("u1")); // Urgent first
    QVERIFY(q.tryPop(&out)); QCOMPARE(out, QStringLiteral("b1")); // Binary second
    QVERIFY(q.tryPop(&out)); QCOMPARE(out, QStringLiteral("c1")); // Control FIFO
    QVERIFY(q.tryPop(&out)); QCOMPARE(out, QStringLiteral("c2")); // Control FIFO
    QVERIFY(!q.tryPop(&out));                                      // empty
    QCOMPARE(q.dropCount(), 0);
    QCOMPARE(q.size(), 0);
}

// ── Test 2: bounded overflow drops oldest ────────────────────────────────────
//
// Push 5 frames into a capacity-3 Control queue.  Expect 2 drops (c0, c1)
// and the last 3 frames survive (c2, c3, c4) in FIFO order.
// The oldest-drop semantics are NereusSDR-original (Thetis queues are
// unbounded at TCIServer.cs:769-771 [v2.10.3.13]).
void TestTciPriorityQueues::bounded_overflow_drops_oldest()
{
    TciSendQueue q(3);  // capacity 3 per queue
    for (int i = 0; i < 5; ++i) {
        q.push(TciSendQueue::Priority::Control,
               QStringLiteral("c%1").arg(i));
    }

    QCOMPARE(q.dropCount(), 2);  // c0 and c1 dropped
    QCOMPARE(q.size(), 3);

    QString out;
    QVERIFY(q.tryPop(&out)); QCOMPARE(out, QStringLiteral("c2"));
    QVERIFY(q.tryPop(&out)); QCOMPARE(out, QStringLiteral("c3"));
    QVERIFY(q.tryPop(&out)); QCOMPARE(out, QStringLiteral("c4"));
    QVERIFY(!q.tryPop(&out));
}

// ── Test 3: multi-thread safety ──────────────────────────────────────────────
//
// Producer pushes kTotal frames on a worker thread while the main thread
// drains. Capacity == kTotal so no drops are expected in the steady-state
// happy path. Asserts: no torn reads / UB (QMutexLocker prevents data
// races), dropCount()==0, drained==kTotal.
void TestTciPriorityQueues::multi_thread_no_drops_or_torn_reads()
{
    constexpr int kTotal = 10000;
    TciSendQueue q(kTotal);  // large enough that no drops occur

    std::atomic<int>  drained{0};
    std::atomic<bool> producerDone{false};

    QThread producerThread;
    QObject* producer = new QObject;
    producer->moveToThread(&producerThread);
    QObject::connect(&producerThread, &QThread::started, producer, [&]() {
        for (int i = 0; i < kTotal; ++i) {
            q.push(TciSendQueue::Priority::Control,
                   QStringLiteral("m%1").arg(i));
        }
        producerDone.store(true, std::memory_order_release);
    });
    producerThread.start();

    // Consumer on the main thread.
    QString out;
    while (!producerDone.load(std::memory_order_acquire) || q.size() > 0) {
        if (q.tryPop(&out)) {
            ++drained;
        } else {
            QThread::yieldCurrentThread();
        }
    }

    producerThread.quit();
    producerThread.wait();
    delete producer;

    QCOMPARE(q.dropCount(), 0);
    QCOMPARE(drained.load(), kTotal);
}

QTEST_GUILESS_MAIN(TestTciPriorityQueues)
#include "tst_tci_priority_queues.moc"
