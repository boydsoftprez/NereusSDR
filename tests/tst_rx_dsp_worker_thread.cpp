// tests/tst_rx_dsp_worker_thread.cpp
//
// Regression test for the GUI-thread fexchange2 deadlock.
//
// Background: prior to the introduction of RxDspWorker, RadioModel
// connected ReceiverManager::iqDataForReceiver to a same-thread lambda
// that called RxChannel::processIq → fexchange2 on the GUI main thread.
// fexchange2 is opened with bfo=1 and blocks on Sem_OutReady whenever
// the WDSP DSP loop has not replenished its output ring. Running it on
// the main thread caused a deterministic two-way deadlock between the
// main thread and WDSP's wdspmain: each side waited on the semaphore
// the other side was supposed to post, and because the main thread was
// blocked the Qt event loop stopped delivering more I/Q events to feed
// fexchange2.
//
// The fix is structural: I/Q processing now lives on a dedicated DSP
// thread inside RxDspWorker, fed by a Qt::QueuedConnection from
// ReceiverManager::iqDataForReceiver. This test verifies the structural
// invariants of that fix without needing a real WDSP build:
//
//   1. processIqBatch() runs on a thread that is NOT the test main
//      thread (i.e. the worker is being driven via a queued connection
//      after moveToThread()).
//   2. A burst of N back-to-back signals is processed without the
//      sender thread blocking on the worker — proving that even if a
//      future hypothetical fexchange2 call were to block inside the
//      worker, it cannot freeze the caller's event loop.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <QThread>
#include <QVector>
#include <atomic>

#include "models/RxDspWorker.h"

using namespace NereusSDR;

class TestRxDspWorkerThread : public QObject {
    Q_OBJECT

private slots:
    // The worker's batchProcessed signal must fire on a thread that
    // is not the test main thread. We capture QThread::currentThread()
    // inside a Qt::DirectConnection lambda — direct connections run
    // on the emitter's thread, which for a QObject moved into a
    // QThread is that thread.
    void processIqBatch_runsOnWorkerThread();

    // 200 back-to-back signals must all be drained by the worker
    // within a generous timeout (5 s) without the sender (this main
    // thread) blocking. The original GUI-thread implementation would
    // never have reached the timeout because the first fexchange2
    // call would freeze the caller — but with no engines wired the
    // worker just emits batchProcessed and returns, so a regression
    // would have to come from the threading wiring itself (e.g. a
    // wrong connection type or a missed moveToThread).
    void processIqBatch_drainsBurstWithoutBlockingSender();

    // (Phase 3M-3a-iv added processIqBatch_emitsAntiVoxSampleReady_perChunk
    //  here, pinning the per-chunk slice-0 anti-VOX fork. Phase 3F Sub-Epic J
    //  Task 9 retired that fork: the reference is now AudioEngine's anti-VOX
    //  mixer, summing every audible slice. See
    //  tests/tst_audio_engine_antivox_mix.cpp.)
};

namespace {

// Spawn a worker on a fresh QThread and return everything the test
// needs to drive it. Caller is responsible for tearing down the
// thread (quit + wait + delete) before letting these go out of scope.
struct WorkerHarness {
    QThread*     thread{nullptr};
    RxDspWorker* worker{nullptr};

    void teardown()
    {
        if (thread != nullptr) {
            thread->quit();
            QVERIFY2(thread->wait(5000), "worker thread did not exit");
            delete worker;
            delete thread;
            worker = nullptr;
            thread = nullptr;
        }
    }
};

WorkerHarness makeHarness()
{
    WorkerHarness h;
    h.thread = new QThread();
    h.thread->setObjectName(QStringLiteral("TestDspThread"));
    h.worker = new RxDspWorker();   // no parent — moved to thread
    h.worker->moveToThread(h.thread);
    h.thread->start();
    return h;
}

} // namespace

void TestRxDspWorkerThread::processIqBatch_runsOnWorkerThread()
{
    WorkerHarness h = makeHarness();
    QThread* mainThread = QThread::currentThread();

    // Capture the thread the worker's signal is emitted from. A direct
    // connection runs the lambda on the emitter's thread (the worker
    // thread), with the worker itself as the context object so the
    // connection is automatically disconnected when the worker is
    // destroyed — no probe QObject leaked across thread teardown.
    std::atomic<QThread*> observed{nullptr};
    QObject::connect(h.worker, &RxDspWorker::batchProcessed,
                     h.worker, [&observed]() {
        observed.store(QThread::currentThread());
    }, Qt::DirectConnection);

    QSignalSpy spy(h.worker, &RxDspWorker::batchProcessed);
    QVERIFY(spy.isValid());

    QVector<float> samples(238 * 2, 0.0f);
    QMetaObject::invokeMethod(h.worker, "processIqBatch",
                              Qt::QueuedConnection,
                              Q_ARG(int, 0),
                              Q_ARG(QVector<float>, samples));

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);

    QThread* slotThread = observed.load();
    QVERIFY2(slotThread != nullptr, "direct-connected lambda did not run");
    QVERIFY2(slotThread != mainThread,
             "RxDspWorker::processIqBatch ran on the test main thread");
    QCOMPARE(slotThread, h.thread);

    h.teardown();
}

void TestRxDspWorkerThread::processIqBatch_drainsBurstWithoutBlockingSender()
{
    WorkerHarness h = makeHarness();

    QSignalSpy spy(h.worker, &RxDspWorker::batchProcessed);
    QVERIFY(spy.isValid());

    constexpr int kBursts = 200;
    QVector<float> samples(238 * 2, 0.5f);

    QElapsedTimer enqueueTimer;
    enqueueTimer.start();
    for (int i = 0; i < kBursts; ++i) {
        QMetaObject::invokeMethod(h.worker, "processIqBatch",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, 0),
                                  Q_ARG(QVector<float>, samples));
    }
    const qint64 enqueueMs = enqueueTimer.elapsed();

    // Sender posting 200 events should return essentially instantly —
    // anything over 1 s means the sender was blocked, which is the
    // exact regression we are guarding against.
    QVERIFY2(enqueueMs < 1000,
             qPrintable(QStringLiteral("enqueueing 200 batches took %1 ms")
                        .arg(enqueueMs)));

    // Drain on the test event loop until the spy has caught all
    // emissions. processEvents drains every pending queued signal in
    // one shot, which is more robust than spy.wait() (which only
    // synchronises on one new emission per call).
    QElapsedTimer drainTimer;
    drainTimer.start();
    while (spy.count() < kBursts && drainTimer.elapsed() < 10000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    QCOMPARE(spy.count(), kBursts);

    h.teardown();
}


QTEST_MAIN(TestRxDspWorkerThread)
#include "tst_rx_dsp_worker_thread.moc"
