// no-port-check: NereusSDR-original main-thread stall trace regressions.
// Modification history (NereusSDR):
//   2026-10-22 - J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
#include <QtTest>
#include <QEventLoop>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include "gui/GuiApplication.h"

using namespace NereusSDR;

namespace {

QMutex g_linesMutex;
QStringList g_lines;
QtMessageHandler g_previousHandler = nullptr;

void captureHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    // MAIN_STALL also covers MAIN_STALL_SUPPRESSED.
    if (message.startsWith(QLatin1String("MAIN_STALL"))
        || message.startsWith(QLatin1String("MAIN_LATE"))) {
        QMutexLocker locker(&g_linesMutex);
        g_lines.append(message);
        return;
    }
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, message);
    }
}

QStringList takeLines()
{
    QMutexLocker locker(&g_linesMutex);
    QStringList lines = g_lines;
    g_lines.clear();
    return lines;
}

// Builds the application with the trace variable as given, posts a queued
// call that sleeps 30 ms, runs the loop until it has run and a little after,
// and returns the trace lines it logged.
QStringList runSlowQueuedCall(const char* traceValue)
{
    if (traceValue != nullptr) {
        qputenv("NEREUS_TRACE_MAIN_STALLS", traceValue);
    } else {
        qunsetenv("NEREUS_TRACE_MAIN_STALLS");
    }
    takeLines();

    int argc = 1;
    char arg0[] = "tst_gui_application_stall_trace";
    char* argv[] = {arg0, nullptr};
    {
        GuiApplication app(argc, argv);
        QObject slowReceiver;
        slowReceiver.setObjectName(QStringLiteral("slowReceiver"));
        QMetaObject::invokeMethod(&slowReceiver, [] {
            QThread::msleep(30);
        }, Qt::QueuedConnection);
        QEventLoop loop;
        QTimer::singleShot(100, &loop, &QEventLoop::quit);
        loop.exec();
    }
    qunsetenv("NEREUS_TRACE_MAIN_STALLS");
    return takeLines();
}

// Builds the traced application with a short window, posts `calls` queued
// calls that each sleep 15 ms, runs the loop past the window's end, and
// returns the trace lines it logged.
QStringList runManySlowQueuedCalls(int calls, qint64 windowNs, int runMs)
{
    qputenv("NEREUS_TRACE_MAIN_STALLS", "1");
    takeLines();

    int argc = 1;
    char arg0[] = "tst_gui_application_stall_trace";
    char* argv[] = {arg0, nullptr};
    {
        GuiApplication app(argc, argv);
        app.setStallTraceWindowNsForTest(windowNs);
        QObject slowReceiver;
        slowReceiver.setObjectName(QStringLiteral("slowReceiver"));
        for (int call = 0; call < calls; ++call) {
            QMetaObject::invokeMethod(&slowReceiver, [] {
                QThread::msleep(15);
            }, Qt::QueuedConnection);
        }
        QEventLoop loop;
        QTimer::singleShot(runMs, &loop, &QEventLoop::quit);
        loop.exec();
    }
    qunsetenv("NEREUS_TRACE_MAIN_STALLS");
    return takeLines();
}

} // namespace

class TestGuiApplicationStallTrace : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        g_previousHandler = qInstallMessageHandler(captureHandler);
    }

    void cleanupTestCase()
    {
        qInstallMessageHandler(g_previousHandler);
    }

    void slowDeliveryLogsMainStall()
    {
        const QStringList lines = runSlowQueuedCall("1");
        QStringList stalls;
        for (const QString& line : lines) {
            if (line.startsWith(QLatin1String("MAIN_STALL"))) {
                stalls.append(line);
            }
        }
        QVERIFY2(!stalls.isEmpty(), qPrintable(lines.join(QLatin1Char('\n'))));
        const QRegularExpression pattern(QStringLiteral(
            "^MAIN_STALL ms=\\d+\\.\\d depth=1 type=MetaCall receiver=QObject "
            "name=slowReceiver parent=- wall=\\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d$"));
        bool matched = false;
        for (const QString& line : stalls) {
            const QRegularExpressionMatch match = pattern.match(line);
            if (match.hasMatch()) {
                matched = true;
                const double ms = line.section(QLatin1String("ms="), 1).section(QLatin1Char(' '), 0, 0).toDouble();
                QVERIFY2(ms >= 12.0, qPrintable(line));
            }
        }
        QVERIFY2(matched, qPrintable(stalls.join(QLatin1Char('\n'))));
    }

    // A slow delivery every frame must not log a line a frame: one window
    // logs at most the cap, then one line counting what it held back.
    void manySlowDeliveriesAreCapped()
    {
        constexpr int kCalls = GuiApplication::kStallTraceLineCap + 10;
        // 50 calls of 15 ms run well inside the 1.5 s window; the loop runs
        // on past its end so the heartbeat rolls it.
        const QStringList lines = runManySlowQueuedCalls(kCalls, 1'500'000'000LL, 2'200);
        const QString all = lines.join(QLatin1Char('\n'));
        int logged = 0;
        QStringList suppressed;
        for (const QString& line : lines) {
            if (line.startsWith(QLatin1String("MAIN_STALL_SUPPRESSED"))) {
                suppressed.append(line);
            } else if (suppressed.isEmpty()) {
                ++logged;
            }
        }
        QCOMPARE(logged, GuiApplication::kStallTraceLineCap);
        QVERIFY2(suppressed.size() == 1, qPrintable(all));
        const QRegularExpressionMatch match =
            QRegularExpression(QStringLiteral("^MAIN_STALL_SUPPRESSED count=(\\d+)$"))
                .match(suppressed.constFirst());
        QVERIFY2(match.hasMatch(), qPrintable(suppressed.constFirst()));
        // MAIN_LATE shares the budget, so at least the slow calls past the cap.
        QVERIFY2(match.captured(1).toInt() >= kCalls - GuiApplication::kStallTraceLineCap,
                 qPrintable(suppressed.constFirst()));
    }

    void traceOffLogsNothing()
    {
        const QStringList lines = runSlowQueuedCall(nullptr);
        QVERIFY2(lines.isEmpty(), qPrintable(lines.join(QLatin1Char('\n'))));
    }
};

int main(int argc, char** argv)
{
    TestGuiApplicationStallTrace test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_gui_application_stall_trace.moc"
