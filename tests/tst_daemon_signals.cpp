// no-port-check: NereusSDR-original daemon signal-safety regression.
// Modification history (NereusSDR):
//   2026-09-20: J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via OpenAI Codex.

#include <QtTest/QtTest>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <new>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QUuid>
#ifdef Q_OS_UNIX
#include <signal.h>
#endif

// Exercise the actual entry-point handler. Renaming main keeps a second
// executable entry point out of this test without exposing a production
// test hook or making a copy of the handler under test.
#define main nereusdEntryPointForTest
#include "../src/server_main.cpp"
#undef main

namespace {
volatile std::sig_atomic_t s_insideSignal = 0;
volatile std::sig_atomic_t s_signalAllocated = 0;

void observeTerm(int signal)
{
    s_insideSignal = 1;
    onTerm(signal);
    s_insideSignal = 0;
}
}

// Replacing the executable's allocator detects Qt's queued-call event
// allocation inside the handler. Such an allocation can deadlock when a
// termination signal interrupts the allocator on a daemon thread.
void* operator new(std::size_t size)
{
    if (s_insideSignal) {
        s_signalAllocated = 1;
    }
    if (void* allocation = std::malloc(size == 0 ? 1 : size)) {
        return allocation;
    }
    throw std::bad_alloc();
}

void operator delete(void* allocation) noexcept
{
    std::free(allocation);
}

class TestDaemonSignals : public QObject {
    Q_OBJECT
private slots:
    void terminationHandlerDoesNotAllocate_data()
    {
        QTest::addColumn<int>("signal");
        QTest::newRow("SIGTERM") << SIGTERM;
        QTest::newRow("SIGINT") << SIGINT;
    }

    void terminationHandlerDoesNotAllocate()
    {
        QFETCH(int, signal);
        s_app = QCoreApplication::instance();
        s_signalAllocated = 0;
        const auto previous = std::signal(signal, observeTerm);
        QVERIFY(previous != SIG_ERR);
        const int result = std::raise(signal);
        std::signal(signal, previous);
        QCOMPARE(result, 0);
        QCOMPARE(static_cast<int>(s_signalAllocated), 0);
    }

    void terminationQuitsDaemon_data()
    {
        terminationHandlerDoesNotAllocate_data();
    }

    void terminationQuitsDaemon()
    {
#ifndef Q_OS_UNIX
        QSKIP("External POSIX signal delivery is tested on macOS and Linux.");
#else
        QFETCH(int, signal);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile config(directory.filePath(QStringLiteral("nereusd.conf")));
        QVERIFY(config.open(QIODevice::WriteOnly));
        // Never select a real radio. This exercises the real entry point's
        // timer and shutdown path with a disconnected, headless daemon.
        config.write("radio_mac = 00:00:00:00:00:00\nremote_port = 0\n");
        config.close();

        const QString profile = QStringLiteral("signal_shutdown_%1")
            .arg(QUuid::createUuid().toString(QUuid::Id128));
        const auto cleanupProfile = qScopeGuard([&profile]() {
            QDir(NereusSDR::AppSettings::resolveConfigDir(profile)).removeRecursively();
        });

        QProcess daemon;
        daemon.setProcessChannelMode(QProcess::MergedChannels);
        daemon.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--daemon-helper"), QStringLiteral("--config"),
                      config.fileName(), QStringLiteral("--profile"),
                      profile});
        const auto cleanup = qScopeGuard([&daemon]() {
            if (daemon.state() != QProcess::NotRunning) {
                daemon.kill();
                daemon.waitForFinished();
            }
        });
        QVERIFY(daemon.waitForStarted());
        QByteArray output;
        QTRY_VERIFY_WITH_TIMEOUT(([&]() {
            output += daemon.readAll();
            return output.contains("nereusd started, slices");
        })(), 20000);

        QCOMPARE(::kill(static_cast<pid_t>(daemon.processId()), signal), 0);
        QVERIFY2(daemon.waitForFinished(5000), "daemon ignored termination request");
        QCOMPARE(daemon.exitStatus(), QProcess::NormalExit);
        QCOMPARE(daemon.exitCode(), 0);
#endif
    }
};

int main(int argc, char* argv[])
{
    if (argc > 1 && std::strcmp(argv[1], "--daemon-helper") == 0) {
        return nereusdEntryPointForTest(argc - 1, argv + 1);
    }
    QCoreApplication app(argc, argv);
    TestDaemonSignals test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_daemon_signals.moc"
