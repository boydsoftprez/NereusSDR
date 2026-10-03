// no-port-check: NereusSDR-original unit test. It drives RxDspWorker on its
// own thread with stamped I/Q batches to prove the receive input delay stays
// bounded when processing is slower than real time (R-R3-40).
//
// tests/tst_rx_dsp_worker_input_delay.cpp
//
// Each batch carries the ReceiverManager::enqueueClockNs() time it was
// queued, exactly as ReceiverManager::iqDataForReceiverStamped delivers it
// in production through a queued connection. The worker's test-only
// processing delay makes every processed batch take twice its own span, so
// without the bound the wait before processing grows by one second per
// second of input.

#include <QtTest/QtTest>
#include "RealtimeTestLoad.h"
#include <QObject>
#include <QRegularExpression>
#include <QThread>
#include <QVector>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "core/ReceiverManager.h"
#include "models/RxDspWorker.h"

using namespace NereusSDR;

namespace {

using Clock = std::chrono::steady_clock;

// 48 kHz stream: drain size 64 (64 * 48000 / 48000), so a 480-sample batch
// spans 10 ms of input.
constexpr int kInSize = 64;
constexpr int kBatchSamples = 480;
constexpr std::chrono::milliseconds kBatchSpan{10};
constexpr int kStream = 0;

// Processing that takes twice the input span: the chain cannot keep up.
constexpr int kOverloadDelayUs = 20000;
// Processing well inside the input span: normal load.
constexpr int kNormalDelayUs = 2000;

// The batch that starts an episode has waited just over the limit; allow
// one processing time and scheduling on a loaded machine on top.
constexpr qint64 kBoundMs = RxDspWorker::kDspInputDelayLimitMs + 300;

const QRegularExpression kEpisodeLine(
    QStringLiteral("^Receive processing fell behind; skipped (\\d+) ms of input to catch up\\.$"));

std::mutex g_logMutex;
QStringList g_episodeLines;
QtMessageHandler g_previousHandler = nullptr;

void captureEpisodeLines(QtMsgType type, const QMessageLogContext& context,
                         const QString& message)
{
    if (message.startsWith(QStringLiteral("Receive processing fell behind"))) {
        std::lock_guard<std::mutex> lock(g_logMutex);
        g_episodeLines.append(message);
        return;  // expected; the test reports them itself
    }
    if (g_previousHandler) {
        g_previousHandler(type, context, message);
    }
}

int episodeLineCount()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    return g_episodeLines.size();
}

QStringList episodeLines()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    return g_episodeLines;
}

class StampedFeeder : public QObject {
    Q_OBJECT
signals:
    void iq(int receiverIndex, const QVector<float>& samples, qint64 enqueuedNs);
};

struct Harness {
    QThread thread;
    RxDspWorker* worker{new RxDspWorker};
    StampedFeeder feeder;
    std::atomic<int> handled{0};

    Harness()
    {
        worker->setBufferSizes(kInSize, 64);
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        // Same connection type production uses for the stamped signal.
        QObject::connect(&feeder, &StampedFeeder::iq,
                         worker, &RxDspWorker::processStampedIqBatch,
                         Qt::QueuedConnection);
        QObject::connect(worker, &RxDspWorker::batchProcessed, worker,
                         [this] { handled.fetch_add(1); }, Qt::DirectConnection);
        thread.start();
    }

    ~Harness()
    {
        worker->setProcessingDelayUsForTest(0);
        thread.quit();
        thread.wait();
    }
};

struct FeedResult {
    int sent{0};
    qint64 maxDelayMs{0};
};

// Feeds real-time batches for `duration`, sampling the worker's latest
// delay after each one. `onBatch` runs after each send.
template <typename OnBatch>
FeedResult feed(Harness& h, std::chrono::milliseconds duration, OnBatch onBatch)
{
    const QVector<float> batch(2 * kBatchSamples, 0.0f);
    FeedResult result;
    const auto start = Clock::now();
    auto next = start;
    while (Clock::now() - start < duration) {
        emit h.feeder.iq(kStream, batch, ReceiverManager::enqueueClockNs());
        ++result.sent;
        result.maxDelayMs = std::max(result.maxDelayMs,
                                     h.worker->inputDelayStats(kStream).inputDelayMs);
        onBatch();
        next += kBatchSpan;
        std::this_thread::sleep_until(next);
    }
    return result;
}

// Waits until the worker has handled every sent batch, sampling the delay.
qint64 drain(Harness& h, int sent, std::chrono::seconds limit)
{
    qint64 maxDelayMs = 0;
    const auto start = Clock::now();
    while (h.handled.load() < sent && Clock::now() - start < limit) {
        maxDelayMs = std::max(maxDelayMs, h.worker->inputDelayStats(kStream).inputDelayMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return std::max(maxDelayMs, h.worker->inputDelayStats(kStream).inputDelayMs);
}

} // namespace

class TestRxDspWorkerInputDelay : public QObject {
    Q_OBJECT

private slots:
    // The load when a real-time case failed (R-R3-21, R-R3-40).
    void cleanup() { NereusSDR::RealtimeTestLoad::printLoadAverageIfFailed(); }

    void initTestCase()
    {
        g_previousHandler = qInstallMessageHandler(captureEpisodeLines);
    }

    void cleanupTestCase()
    {
        qInstallMessageHandler(g_previousHandler);
    }

    void init()
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        g_episodeLines.clear();
    }

    void delayStaysBoundedWhenProcessingCannotKeepUp()
    {
        Harness h;
        h.worker->setProcessingDelayUsForTest(kOverloadDelayUs);
        const FeedResult fed = feed(h, std::chrono::milliseconds(3000), [] {});
        const qint64 drainMaxMs = drain(h, fed.sent, std::chrono::seconds(10));
        const qint64 maxDelayMs = std::max(fed.maxDelayMs, drainMaxMs);
        const RxDspWorker::InputDelayStats stats = h.worker->inputDelayStats(kStream);

        qInfo("sent %d batches (%d ms of input); handled %d; worst delay %lld ms "
              "(bound %lld); skipped %lld ms; %d episode line(s)",
              fed.sent, fed.sent * int(kBatchSpan.count()), h.handled.load(),
              maxDelayMs, kBoundMs, stats.droppedInputMs, episodeLineCount());

        QCOMPARE(h.handled.load(), fed.sent);
        QVERIFY2(maxDelayMs <= kBoundMs, "the receive input delay grew past its bound");
        QVERIFY2(stats.droppedInputMs > 0, "no input was skipped while overloaded");
        QVERIFY(episodeLineCount() >= 1);
    }

    void oneLogLinePerEpisode()
    {
        Harness h;
        h.worker->setProcessingDelayUsForTest(kOverloadDelayUs);
        bool recovered = false;
        // Overload until the first episode ends, then process at normal
        // speed: exactly one episode, and it is logged exactly once.
        FeedResult fed = feed(h, std::chrono::milliseconds(2500), [&] {
            if (!recovered && episodeLineCount() > 0) {
                h.worker->setProcessingDelayUsForTest(0);
                recovered = true;
            }
        });
        drain(h, fed.sent, std::chrono::seconds(10));
        const RxDspWorker::InputDelayStats stats = h.worker->inputDelayStats(kStream);
        const QStringList lines = episodeLines();

        qInfo("skipped %lld ms; lines: %s", stats.droppedInputMs,
              qPrintable(lines.join(QStringLiteral(" | "))));

        QVERIFY(recovered);
        QCOMPARE(lines.size(), 1);
        const QRegularExpressionMatch match = kEpisodeLine.match(lines.first());
        QVERIFY2(match.hasMatch(), qPrintable(lines.first()));
        // The logged amount is the episode's skipped input, which is all of
        // the input skipped in this run.
        QCOMPARE(match.captured(1).toLongLong(), stats.droppedInputMs);
        QVERIFY(stats.droppedInputMs > 0);
        QVERIFY(stats.inputDelayMs < RxDspWorker::kDspInputDelayResumeMs);
    }

    void normalLoadSkipsNothing()
    {
        Harness h;
        h.worker->setProcessingDelayUsForTest(kNormalDelayUs);
        const FeedResult fed = feed(h, std::chrono::milliseconds(1500), [] {});
        const qint64 drainMaxMs = drain(h, fed.sent, std::chrono::seconds(10));
        const RxDspWorker::InputDelayStats stats = h.worker->inputDelayStats(kStream);

        qInfo("sent %d; worst delay %lld ms; skipped %lld ms",
              fed.sent, std::max(fed.maxDelayMs, drainMaxMs), stats.droppedInputMs);

        QCOMPARE(h.handled.load(), fed.sent);
        QCOMPARE(stats.droppedInputMs, 0LL);
        QCOMPARE(episodeLineCount(), 0);
    }

    void untrackedReceiverReportsZero()
    {
        Harness h;
        const RxDspWorker::InputDelayStats stats =
            h.worker->inputDelayStats(RxDspWorker::kMaxInputDelayReceivers);
        QCOMPARE(stats.inputDelayMs, 0LL);
        QCOMPARE(stats.droppedInputMs, 0LL);
    }
};

QTEST_GUILESS_MAIN(TestRxDspWorkerInputDelay)
#include "tst_rx_dsp_worker_input_delay.moc"
