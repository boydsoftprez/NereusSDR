// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_core_init_fatal_log.cpp  (NereusSDR)
// =================================================================
//
// Fix wave (2026-09-30): a fatal message reaches the log file. CoreInit's
// message handler offers each line to the LogSink, whose writer thread
// writes the file every 20 ms; Qt aborts as soon as the handler returns
// from a fatal, so the line that said why the process died never reached
// the file. The handler now drains the sink for a fatal.
//
// The fatal runs in a child: this binary again, with --fatal-child, which
// starts CoreInit with a profile of its own and calls qFatal. The child
// leaves through its SIGABRT handler with _exit, so no crash report is
// written. The parent reads the child's log file.
//
// Files under the test-mode config directory only. No RF, no audio device.
//
// Modification history (NereusSDR):
//   2026-10-03: deterministic idle/contended fatal drain coverage by J.J. Boyd
//               (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-30: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QSemaphore>

#include "core/AppSettings.h"
#include "core/CoreInit.h"
#include "core/LogSink.h"

#include <csignal>
#include <cstdlib>

using namespace NereusSDR;

namespace {

const char* const kChildFlag = "--fatal-child";
const char* const kFullChildFlag = "--fatal-full-child";
const char* const kContendedChildFlag = "--fatal-contended-child";
constexpr int kChildAbortExit = 42;
const char* const kFatalMarker = "fatal-log-marker-7c1e";

QString profileForParent(qint64 pid)
{
    return QStringLiteral("fatal-log-%1").arg(pid);
}

extern "C" void exitOnAbort(int)
{
    std::_Exit(kChildAbortExit);
}

int runChild(const QString& profile, bool fillLog, bool contended)
{
    std::signal(SIGABRT, exitOnAbort);
    AppSettings::setProfileOverride(profile);
    if (!CoreInit::initialize(profile)) {
        return 1;
    }
    // File assertions exercise an idle drain deterministically. The separate
    // contended case proves the accepted nonblocking stderr fallback.
    LogSink& sink = LogSink::instance();
    sink.stop();
    QSemaphore entered, release;
    if (contended) {
        sink.setBeforeWriteForTest([&]() { entered.release(); release.acquire(); });
        sink.start();
        if (!sink.offer(QStringLiteral("held writer\n")) || !entered.tryAcquire(1, 10000)) {
            std::_Exit(5);
        }
        // The writer stays held through SIGABRT; a waiting fatal handler
        // could never let this child reach its required exit status.
    }
    if (fillLog) {
        const QStringList names = QDir(AppSettings::resolveConfigDir(profile)).entryList(
            {"nereussdr-*.log"}, QDir::Files, QDir::Name);
        if (names.size() != 1) { return 3; }
        QFile active(QDir(AppSettings::resolveConfigDir(profile)).filePath(names.first()));
        // A sparse real file sets the writer exactly at the production cap
        // without emitting 32 MiB through stderr just to fill the fixture.
        if (!active.open(QIODevice::ReadWrite) || !active.resize(32 * 1024 * 1024)) { return 4; }
        active.close();
    }
    qFatal("%s", kFatalMarker);
    return 2;   // not reached: qFatal aborts
}

} // namespace

class TstCoreInitFatalLog : public QObject {
    Q_OBJECT

private slots:
    void fatalMessageReachesTheLogFile()
    {
        checkFatal(false);
    }

    void fatalMessageRotatesAFullLogBeforeAbort()
    {
        checkFatal(true);
    }

    void contendedFatalReachesStderrWithoutWaitingForWriter()
    {
        checkFatal(false, true);
    }

private:
    void checkFatal(bool fillLog, bool contended = false)
    {
        const QString profile = profileForParent(QCoreApplication::applicationPid()) +
                                (contended ? "-contended" : fillLog ? "-full" : "-normal");
        const QString logDir = AppSettings::resolveConfigDir(profile);
        QDir(logDir).removeRecursively();
        const auto cleanup = qScopeGuard([&]() { QDir(logDir).removeRecursively(); });

        QProcess child;
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(),
                    {QString::fromLatin1(contended ? kContendedChildFlag : fillLog ? kFullChildFlag : kChildFlag), profile});
        const bool finished = child.waitForFinished(contended ? 10000 : 60000);
        if (!finished) { child.kill(); child.waitForFinished(10000); }
        QVERIFY2(finished, "the child did not finish while its writer remained held");
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), kChildAbortExit);
        const QByteArray stderrText = child.readAll();
        QVERIFY(stderrText.contains(kFatalMarker));
        if (contended) { QVERIFY(stderrText.contains(QByteArray("FTL: ") + kFatalMarker)); }

        const QStringList logs =
            QDir(logDir).entryList({QStringLiteral("nereussdr-*.log")}, QDir::Files, QDir::Name);
        QCOMPARE(logs.size(), fillLog ? 2 : 1);
        QFile log(logDir + QLatin1Char('/') + logs.last());
        QVERIFY(log.open(QIODevice::ReadOnly));
        const QByteArray text = log.readAll();
        QDir(logDir).removeRecursively();
        if (contended) {
            QVERIFY(!text.contains(kFatalMarker)); // held writer could not flush it
        } else {
            QVERIFY2(text.contains(QByteArray("FTL: ") + kFatalMarker),
                     text.right(400).constData());
        }
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    qsizetype flag = args.indexOf(QString::fromLatin1(kChildFlag));
    const qsizetype fullFlag = args.indexOf(QString::fromLatin1(kFullChildFlag));
    const qsizetype contendedFlag = args.indexOf(QString::fromLatin1(kContendedChildFlag));
    if (fullFlag >= 0) { flag = fullFlag; }
    if (contendedFlag >= 0) { flag = contendedFlag; }
    if (flag >= 0 && flag + 1 < args.size()) {
        return runChild(args.at(flag + 1), fullFlag >= 0, contendedFlag >= 0);
    }
    TstCoreInitFatalLog test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_core_init_fatal_log.moc"
