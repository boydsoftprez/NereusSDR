// tests/tst_realtime_audio_priority.cpp
//
// Once-only operator warning when the OS refuses raised thread priority
// (R-R3-29). Per-thread detail stays at info level at each call site; the
// process-wide warning must appear exactly once however many threads are
// refused. Exercised through the platform-independent seam so the rule is
// checked on every platform, not only on Linux where refusal is common.

#include <QtTest/QtTest>

#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QThread>

#include <atomic>
#include <memory>
#include <vector>

#include "core/audio/RealtimeAudioPriority.h"

using namespace NereusSDR;

namespace {

QMutex g_captureMutex;
QStringList g_warnings;
QtMessageHandler g_previousHandler = nullptr;

void captureHandler(QtMsgType type, const QMessageLogContext& context,
                    const QString& message)
{
    if (type == QtWarningMsg && context.category
        && QByteArray(context.category) == "nereussdr.rt_audio") {
        QMutexLocker lock(&g_captureMutex);
        g_warnings.append(message);
        return;
    }
    if (g_previousHandler) {
        g_previousHandler(type, context, message);
    }
}

QStringList capturedWarnings()
{
    QMutexLocker lock(&g_captureMutex);
    return g_warnings;
}

} // namespace

class TestRealtimeAudioPriority final : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        resetThreadPriorityRefusedForTest();
        {
            QMutexLocker lock(&g_captureMutex);
            g_warnings.clear();
        }
        g_previousHandler = qInstallMessageHandler(captureHandler);
    }

    void cleanup()
    {
        qInstallMessageHandler(g_previousHandler);
        g_previousHandler = nullptr;
        resetThreadPriorityRefusedForTest();
    }

    void firstRefusalLogsOnePlainWarning()
    {
        QVERIFY(noteThreadPriorityRefused());
        QVERIFY(!noteThreadPriorityRefused());
        QVERIFY(!noteThreadPriorityRefused());

        const QStringList warnings = capturedWarnings();
        QCOMPARE(warnings.size(), 1);
        QCOMPARE(warnings.first(),
                 QStringLiteral("Raised thread priority was refused; audio and"
                                " signal processing threads run at normal"
                                " priority."));
    }

    // R-R3-41: nereusd's thread placement startup line says itself that
    // raised priority is not permitted; after it claims the warning, a later
    // refusal logs nothing.
    void claimedWarningIsNotRepeated()
    {
        QVERIFY(claimThreadPriorityRefusedWarning());
        QVERIFY(!claimThreadPriorityRefusedWarning());
        QVERIFY(!noteThreadPriorityRefused());
        QVERIFY(capturedWarnings().isEmpty());
    }

    void concurrentRefusalsStillWarnOnce()
    {
        constexpr int kThreads = 8;
        std::atomic<int> logged{0};
        std::vector<std::unique_ptr<QThread>> threads;
        threads.reserve(kThreads);
        for (int i = 0; i < kThreads; ++i) {
            threads.emplace_back(QThread::create([&logged]() {
                if (noteThreadPriorityRefused()) {
                    logged.fetch_add(1);
                }
            }));
        }
        for (const std::unique_ptr<QThread>& thread : threads) {
            thread->start();
        }
        for (const std::unique_ptr<QThread>& thread : threads) {
            QVERIFY(thread->wait(5000));
        }

        QCOMPARE(logged.load(), 1);
        QCOMPARE(capturedWarnings().size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestRealtimeAudioPriority)
#include "tst_realtime_audio_priority.moc"
