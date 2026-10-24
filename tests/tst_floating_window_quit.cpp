// no-port-check: NereusSDR-original application quit regression.
//
// Quitting was refused while a pan or an applet floated: both floating
// windows ignored every close event to turn it into a dock request, and
// QApplication cancels the whole quit when one window ignores its close.
// The fix follows Longpath PR 83 (src/gui/PanFloatingWindow.cpp:230-243
// [@383f058]); this test runs the real queued QApplication::quit() loop in a
// child process, as tst_container_application_quit does, so a refused quit
// shows as the child's watchdog rather than a hung test.
//
// Modification history (NereusSDR):
//   2026-10-22 - Created. By J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Claude Code.
#include <QtTest>
#include <QApplication>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTimer>

#include "gui/GuiApplication.h"
#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterApplet.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/ModMonitorApplet.h"

using namespace NereusSDR;

namespace {
constexpr int kWatchdogMs = 2000;
constexpr int kWatchdogExit = 70;
constexpr int kAssertionExit = 71;

int runQuitChild(QApplication& application)
{
    QStringList failures;
    const auto check = [&failures](bool condition, const QString& message) {
        if (!condition) {
            failures.append(message);
        }
    };

    // The main window stands in for MainWindow: both floating windows are
    // parented to it, as PanadapterStack and AppletPanelWidget parent them.
    QWidget mainWindow;
    mainWindow.resize(800, 600);
    mainWindow.show();

    QPointer<PanFloatingWindow> pan = new PanFloatingWindow(&mainWindow);
    pan->adoptApplet(new PanadapterApplet(QStringLiteral("pan-0")));
    pan->show();
    QPointer<AppletFloatingWindow> applet =
        new AppletFloatingWindow(new ModMonitorApplet(nullptr), &mainWindow);
    applet->show();

    QSignalSpy panDocks(pan.data(), &PanFloatingWindow::dockRequested);
    QSignalSpy appletDocks(applet.data(), &AppletFloatingWindow::dockRequested);
    QSignalSpy aboutToQuit(&application, &QCoreApplication::aboutToQuit);
    application.setQuitOnLastWindowClosed(false);

    bool watchdogFired = false;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &application, [&] {
        watchdogFired = true;
        qCritical() << "Quit child watchdog: the quit was refused; exiting";
        application.exit(kWatchdogExit);
    });
    QTimer::singleShot(0, &application, [&] {
        check(pan && pan->isVisible() && applet && applet->isVisible(),
              QStringLiteral("both floating windows are up before the quit"));
        watchdog.start(kWatchdogMs);
        // The real queued QEvent::Quit and its close sweep inside exec().
        QMetaObject::invokeMethod(&application, "quit", Qt::QueuedConnection);
    });

    const int eventLoopResult = application.exec();
    watchdog.stop();
    check(!watchdogFired, QStringLiteral("the quit finishes without the watchdog"));
    check(eventLoopResult == 0, QStringLiteral("the event loop returns zero"));
    check(aboutToQuit.count() == 1, QStringLiteral("the application emits aboutToQuit once"));
    check(pan && !pan->isVisible(), QStringLiteral("the floating pan closes on quit"));
    check(applet && !applet->isVisible(), QStringLiteral("the floating applet closes on quit"));
    check(panDocks.isEmpty(), QStringLiteral("the quit does not dock the pan"));
    check(appletDocks.isEmpty(), QStringLiteral("the quit does not dock the applet"));
    qInfo().noquote() << QStringLiteral("QUIT_CHILD exec=%1 watchdog=%2 aboutToQuit=%3 panDocks=%4 appletDocks=%5 failures=%6")
        .arg(eventLoopResult).arg(watchdogFired).arg(aboutToQuit.count())
        .arg(panDocks.count()).arg(appletDocks.count()).arg(failures.size());
    for (const QString& failure : failures) {
        qCritical().noquote() << "QUIT_CHILD assertion:" << failure;
    }
    return failures.isEmpty() ? 0 : kAssertionExit;
}
} // namespace

class TstFloatingWindowQuit : public QObject
{
    Q_OBJECT
private slots:
    void quitProceedsWithFloatingPanAndApplet()
    {
        QProcess child;
        child.setProcessChannelMode(QProcess::MergedChannels);
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        child.setProcessEnvironment(environment);
        child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--quit-child")});
        QVERIFY2(child.waitForStarted(10000), qPrintable(child.errorString()));
        const bool finished = child.waitForFinished(10000);
        const QByteArray output = child.readAll();
        qInfo().noquote() << QString::fromUtf8(output);
        QVERIFY2(finished, "Child exceeded its own clean-exit watchdog");
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QVERIFY2(child.exitCode() == 0, output.constData());
    }
};

int main(int argc, char** argv)
{
    GuiApplication application(argc, argv);
    if (application.arguments().size() == 2
        && application.arguments()[1] == QLatin1String("--quit-child")) {
        return runQuitChild(application);
    }
    TstFloatingWindowQuit test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_floating_window_quit.moc"
