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
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QSignalSpy>
#include <QStringList>
#include <QThread>
#include <QVector>
#include <atomic>
#include <memory>

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

// R-R3-21: processIqBatch_runsOnWorkerThread's 5 s wait timed out once in
// a loaded Linux run (1 of 3 full runs, 0 of 200 alone or under stress) and
// the failure said nothing about why. This describes, after the wait has
// already failed, what each thread was doing, so the next occurrence
// carries its cause: whether the worker had emitted at all (the direct-
// connected probe ran), whether its event loop still answers a queued
// call posted behind the batch (idle versus stuck inside a slot), whether
// the batch ran by the time that call was answered, and on Linux every
// thread of the process with its scheduler state and kernel wait channel.
// It runs only on the failure path.
QString describeStalledBatch(const WorkerHarness& h,
                             const std::atomic<QThread*>& emittedOn,
                             const QSignalSpy& spy)
{
    const auto yesNo = [](bool b) {
        return b ? QStringLiteral("yes") : QStringLiteral("no");
    };
    const auto emitted = [&] {
        QThread* const thread = emittedOn.load();
        if (thread == nullptr) {
            return QStringLiteral("no");
        }
        return thread == h.thread ? QStringLiteral("yes, on the worker thread")
                                  : QStringLiteral("yes, on another thread");
    };
    QStringList lines;
    lines << QStringLiteral("at the timeout: worker emitted batchProcessed: %1; spy count: %2")
                 .arg(emitted())
                 .arg(spy.count());
    lines << QStringLiteral("worker thread: running=%1 finished=%2 dispatcher=%3")
                 .arg(yesNo(h.thread->isRunning()), yesNo(h.thread->isFinished()),
                      yesNo(h.thread->eventDispatcher() != nullptr));

    // Queued behind the batch, so an answer means the worker's loop reached
    // it. Shared, so a worker that answers after this returns writes to
    // live memory rather than to this frame.
    auto answered = std::make_shared<std::atomic<bool>>(false);
    QMetaObject::invokeMethod(h.worker, [answered] { answered->store(true); },
                              Qt::QueuedConnection);
    QElapsedTimer probe;
    probe.start();
    while (!answered->load() && probe.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
    QCoreApplication::processEvents();
    lines << QStringLiteral("worker event loop answered a call queued behind the batch "
                            "within 1 s: %1")
                 .arg(yesNo(answered->load()));
    lines << QStringLiteral("after that probe: worker emitted batchProcessed: %1; "
                            "spy count: %2")
                 .arg(emitted())
                 .arg(spy.count());

#ifdef Q_OS_LINUX
    const QDir tasks(QStringLiteral("/proc/self/task"));
    const QStringList tids = tasks.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    lines << QStringLiteral("threads in this process: %1").arg(tids.size());
    const auto readFirstLine = [](const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            return QStringLiteral("?");
        }
        return QString::fromUtf8(f.readLine()).trimmed();
    };
    for (const QString& tid : tids) {
        const QString base = tasks.filePath(tid);
        const QString comm = readFirstLine(base + QStringLiteral("/comm"));
        // Field 3 of stat, after the parenthesised name, is the state.
        const QString stat = readFirstLine(base + QStringLiteral("/stat"));
        const qsizetype close = stat.lastIndexOf(QLatin1Char(')'));
        const QString state = close >= 0 ? stat.mid(close + 2, 1) : QStringLiteral("?");
        const QString wchan = readFirstLine(base + QStringLiteral("/wchan"));
        lines << QStringLiteral("  tid %1 %2 state=%3 wchan=%4")
                     .arg(tid, comm, state, wchan.isEmpty() ? QStringLiteral("-") : wchan);
    }
#endif
    return lines.join(QLatin1Char('\n'));
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

    // R-R3-21: the worker can emit before this thread reaches the wait (a
    // loaded Linux run caught it: "worker emitted batchProcessed: yes, on
    // the worker thread; spy count: 1" at the timeout). QSignalSpy::wait
    // counts only emissions that land while it waits, so an early one read
    // as a stall. Wait on the count instead: an emission at any moment after
    // the batch was queued counts, and each short wait still returns on the
    // emission itself.
    QElapsedTimer waited;
    waited.start();
    while (spy.count() == 0 && waited.elapsed() < 5000) {
        spy.wait(20);
    }
    if (spy.count() == 0) {
        const QByteArray why = QByteArrayLiteral(
                                   "no batchProcessed within 5 s; thread states:\n")
            + describeStalledBatch(h, observed, spy).toUtf8();
        QFAIL(why.constData());
    }
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
