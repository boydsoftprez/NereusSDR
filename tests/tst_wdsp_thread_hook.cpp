// no-port-check: NereusSDR-original linked-WDSP integration test. It opens
// real WDSP channels to prove linux_port.c's thread-start hook (R-R3-41)
// fires once for each channel worker and flush thread, on that new thread,
// with the right kind and channel, and that each thread is named for its job;
// and that a worker reports, once, on itself, that it is about to end.
#include <QtTest>

#include <QMutex>
#include <QMutexLocker>

#include <chrono>
#include <thread>

#include <pthread.h>

#include "core/wdsp_api.h"

namespace {

// WDSP channel ids no application object (or other test) uses.
constexpr int kRxChannel = 23;
constexpr int kTxChannel = 24;
constexpr int kSampleRate = 48000;
constexpr int kInSize = 1024;
constexpr int kDspSize = 4096;

struct Start {
    int kind;
    int channel;
    pthread_t thread;
    QByteArray name;
};

QMutex g_mutex;
QList<Start> g_starts;

void recordStart(int kind, int channel)
{
    char name[32] = {};
    pthread_getname_np(pthread_self(), name, sizeof(name));
    QMutexLocker lock(&g_mutex);
    g_starts.append({kind, channel, pthread_self(), QByteArray(name)});
}

// Starts (every kind but a worker's exit report) or exits for a channel.
QList<Start> recordsFor(int channel, bool exits)
{
    QMutexLocker lock(&g_mutex);
    QList<Start> out;
    for (const Start& s : std::as_const(g_starts)) {
        if (s.channel == channel && (s.kind == kWdspThreadWorkerExit) == exits) {
            out.append(s);
        }
    }
    return out;
}

QList<Start> startsFor(int channel)
{
    return recordsFor(channel, false);
}

// The hook runs on the new thread, which may not have run yet when
// OpenChannel returns; a worker's exit report can follow CloseChannel.
bool waitForRecords(int channel, int count, bool exits)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (recordsFor(channel, exits).size() >= count) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

void openChannel(int channel, int type)
{
    OpenChannel(channel, kInSize, kDspSize, kSampleRate, kSampleRate, kSampleRate,
                type,
                0,       // state: off
                0.010, 0.025, 0.000, 0.010,
                0);      // bfo off
}

} // namespace

class TestWdspThreadHook : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        QMutexLocker lock(&g_mutex);
        g_starts.clear();
    }

    void cleanup()
    {
        WDSPSetThreadStartHook(nullptr);
    }

    void hookFiresOncePerWorkerAndFlushThread_data()
    {
        QTest::addColumn<int>("channel");
        QTest::addColumn<int>("type");
        QTest::addColumn<int>("workerKind");
        QTest::addColumn<QByteArray>("workerName");
        QTest::addColumn<QByteArray>("flushName");
        QTest::newRow("receive") << kRxChannel << 0 << kWdspThreadRxMain
                                 << QByteArray("WDSP rx23") << QByteArray("WDSP flush23");
        QTest::newRow("transmit") << kTxChannel << 1 << kWdspThreadTxMain
                                  << QByteArray("WDSP tx24") << QByteArray("WDSP flush24");
    }

    void hookFiresOncePerWorkerAndFlushThread()
    {
        QFETCH(int, channel);
        QFETCH(int, type);
        QFETCH(int, workerKind);
        QFETCH(QByteArray, workerName);
        QFETCH(QByteArray, flushName);

        WDSPSetThreadStartHook(&recordStart);
        openChannel(channel, type);
        QVERIFY(waitForRecords(channel, 2, false));
        // Nothing else starts for this channel.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const QList<Start> starts = startsFor(channel);
        QVERIFY(recordsFor(channel, true).isEmpty());
        CloseChannel(channel);
        // The worker reports once, on itself, that it is about to end.
        QVERIFY(waitForRecords(channel, 1, true));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        const QList<Start> exits = recordsFor(channel, true);
        QCOMPARE(exits.size(), 1);
        QCOMPARE(exits.first().name, workerName);

        QCOMPARE(starts.size(), 2);
        int workers = 0;
        int flushes = 0;
        for (const Start& s : starts) {
            QVERIFY(!pthread_equal(s.thread, pthread_self()));
            if (s.kind == workerKind) {
                ++workers;
                QCOMPARE(s.name, workerName);
            } else if (s.kind == kWdspThreadFlush) {
                ++flushes;
                QCOMPARE(s.name, flushName);
            } else {
                QFAIL(qPrintable(QStringLiteral("unexpected kind %1").arg(s.kind)));
            }
        }
        QCOMPARE(workers, 1);
        QCOMPARE(flushes, 1);
        for (const Start& s : starts) {
            if (s.kind == workerKind) {
                QVERIFY(pthread_equal(s.thread, exits.first().thread));
            }
        }
        QVERIFY(!pthread_equal(starts.at(0).thread, starts.at(1).thread));
    }

    void noHookMeansNoCalls()
    {
        WDSPSetThreadStartHook(nullptr);
        openChannel(kRxChannel, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CloseChannel(kRxChannel);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        QVERIFY(startsFor(kRxChannel).isEmpty());
        QVERIFY(recordsFor(kRxChannel, true).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestWdspThreadHook)
#include "tst_wdsp_thread_hook.moc"
