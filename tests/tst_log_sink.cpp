// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_log_sink.cpp  (NereusSDR)
// =================================================================
//
// Remote-window parity Task 22 (R-R3-49; the controller's ruling 2): a
// logging thread never waits on the log. A debug category turned on while
// the radio is on the air, in a local window or a remote one, must not put
// file I/O or a lock on the audio thread.
//
//   - Offering a line returns at once even while the writer is stuck (a
//     slow disk): past the ring's size lines are dropped and counted, and
//     the log says how many.
//   - Lines reach the file in order, and the newest are kept, numbered,
//     for the Core's `coreLog` stream.
//
// Files in a temporary directory only. No RF, no audio device.
//
//   cmake --build build --target tst_log_sink
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_log_sink$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: sustained-producer drain regression added with OpenAI
//               Codex assistance.
//   2026-09-30: tryDrainNeverWaitsOnARunningDrain (fix wave), J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QFile>
#include <QSemaphore>
#include <QTemporaryDir>

#include "core/LogSink.h"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

using namespace NereusSDR;

class TstLogSink : public QObject {
    Q_OBJECT

private slots:
    void offeringNeverWaitsForAStuckWriter()
    {
        LogSink sink(64);
        std::atomic<bool> stuck{true};
        std::atomic<bool> writing{false};
        sink.setBeforeWriteForTest([&]() {
            writing.store(true);
            while (stuck.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
        sink.setOutputs(nullptr, false);
        sink.start();
        QVERIFY(sink.offer(QStringLiteral("first\n")));
        QTRY_VERIFY(writing.load());   // the writer is now stuck on its disk

        // Four threads log 20000 lines between them while it stays stuck.
        // Every one of them finishes with the writer still stuck: an offer
        // that waited for the writer would never return, so the joins below
        // are the proof, with no clock to race on a loaded machine.
        std::atomic<int> accepted{0};
        std::vector<std::thread> loggers;
        for (int t = 0; t < 4; ++t) {
            loggers.emplace_back([&, t]() {
                for (int i = 0; i < 5000; ++i) {
                    if (sink.offer(QStringLiteral("t%1 line %2\n").arg(t).arg(i))) {
                        accepted.fetch_add(1);
                    }
                }
            });
        }
        for (std::thread& logger : loggers) {
            logger.join();
        }
        QVERIFY(stuck.load());
        // The ring holds 64; everything past it was dropped, not waited for.
        QVERIFY2(accepted.load() <= 64, qPrintable(QString::number(accepted.load())));
        QCOMPARE(sink.droppedCount(), quint64(20000 - accepted.load()));

        stuck.store(false);
        sink.stop();
        // The log says how many lines it lost.
        const QList<LogSinkLine> lines = sink.linesSince(0);
        QVERIFY(!lines.isEmpty());
        bool noted = false;
        for (const LogSinkLine& line : lines) {
            noted = noted || line.text.contains(QStringLiteral("log lines were dropped"));
        }
        QVERIFY(noted);
    }

    void linesReachTheFileInOrderAndTheNewestAreKept()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(dir.filePath(QStringLiteral("sink.log")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        LogSink sink;
        sink.setOutputs(&file, false);
        sink.start();
        const int count = LogSink::kRecentCapacity + 500;
        for (int i = 0; i < count; ++i) {
            QVERIFY(sink.offer(QStringLiteral("line %1\n").arg(i)));
        }
        sink.stop();
        file.close();

        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QList<QByteArray> written = file.readAll().split('\n');
        QCOMPARE(written.size(), count + 1);   // the last newline
        for (int i = 0; i < count; ++i) {
            QCOMPARE(written.at(i), QByteArray("line ") + QByteArray::number(i));
        }

        const QList<LogSinkLine> recent = sink.linesSince(0);
        QCOMPARE(recent.size(), LogSink::kRecentCapacity);
        QCOMPARE(recent.last().text, QStringLiteral("line %1").arg(count - 1));
        for (int i = 1; i < recent.size(); ++i) {
            QVERIFY(recent.at(i).sequence == recent.at(i - 1).sequence + 1);
        }
        QCOMPARE(sink.lastSequence(), recent.last().sequence);
        // Only what is newer than a sequence.
        const QList<LogSinkLine> newer = sink.linesSince(recent.last().sequence - 3);
        QCOMPARE(newer.size(), 3);
    }

    void drainingNowNeedsNoWriterThread()
    {
        LogSink sink;
        QVERIFY(!sink.isRunning());
        QVERIFY(sink.offer(QStringLiteral("before any writer\n")));
        QVERIFY(sink.linesSince(0).isEmpty());
        sink.drainNow();
        const QList<LogSinkLine> lines = sink.linesSince(0);
        QCOMPARE(lines.size(), 1);
        QCOMPARE(lines.first().text, QStringLiteral("before any writer"));
    }

    // Fix wave (2026-09-30): the drain a fatal message runs. It drains at
    // once with no writer, and returns at once, draining nothing, while
    // the writer is inside a drain (stuck on its disk) or when called from
    // inside a drain on the same thread.
    void tryDrainNeverWaitsOnARunningDrain()
    {
        LogSink sink(64);
        QVERIFY(sink.offer(QStringLiteral("no writer\n")));
        QVERIFY(sink.tryDrainNow());
        QCOMPARE(sink.linesSince(0).size(), 1);

        std::atomic<bool> stuck{true};
        std::atomic<bool> writing{false};
        bool nested = true;
        sink.setBeforeWriteForTest([&]() {
            if (!writing.exchange(true)) {
                // From inside the writer's own drain: refused, not a
                // second lock of the drain mutex.
                nested = sink.tryDrainNow();
            }
            while (stuck.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
        sink.start();
        QVERIFY(sink.offer(QStringLiteral("held\n")));
        QTRY_VERIFY(writing.load());
        QVERIFY(sink.offer(QStringLiteral("waiting\n")));
        QVERIFY(!sink.tryDrainNow());
        QCOMPARE(sink.linesSince(0).size(), 1);
        stuck.store(false);
        sink.stop();
        QVERIFY(!nested);
        QCOMPARE(sink.linesSince(0).size(), 3);
    }

    void oneDrainReturnsWhileAProducerRefillsEverySlot()
    {
        LogSink sink(8);
        for (int i = 0; i < 8; ++i) {
            QVERIFY(sink.offer(QStringLiteral("initial %1\n").arg(i)));
        }
        QSemaphore refillRequested;
        QSemaphore refillDone;
        std::atomic<bool> stop{false};
        sink.setAfterTakeForTest([&]() {
            refillRequested.release();
            refillDone.acquire();
        });
        std::thread producer([&]() {
            while (true) {
                refillRequested.acquire();
                if (stop.load()) { return; }
                while (!sink.offer(QStringLiteral("refill\n"))) {
                    std::this_thread::yield();
                }
                refillDone.release();
            }
        });
        auto drain = std::async(std::launch::async, [&]() { sink.drainNow(); });
        const bool returned = drain.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
        stop.store(true);
        refillDone.release(16);
        refillRequested.release();
        producer.join();
        drain.get();
        QVERIFY2(returned, "one drain kept taking lines from a producer that refilled the ring");
        sink.setAfterTakeForTest({});
        QCOMPARE(sink.linesSince(0).size(), 8);
        sink.drainNow();
        QCOMPARE(sink.linesSince(0).size(), 16);
    }
};

QTEST_GUILESS_MAIN(TstLogSink)
#include "tst_log_sink.moc"
