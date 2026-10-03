// =================================================================
// tests/tst_first_run_prompts_test_run.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It builds a real local
// MainWindow in a test run; no upstream logic is ported here.
//
// R-R3-49 / R-R3-21: a test run never opens a first-run prompt that blocks
// for a click. On a Linux host with no sound server the window posted the
// Linux audio first-run dialog, which runs modally (exec()), so the two
// window sweeps (tst_unbuilt_features, tst_no_placeholder_marks) sat in it
// until QtTest's 300 s watchdog killed them. The window is built here with
// every first-run setting left unset, and any dialog that becomes the
// active modal widget fails the test (and is closed, so the test fails
// rather than hangs).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  Linux sweep hang fix (R-R3-49,
//                                    R-R3-21). AI-assisted transformation
//                                    via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>

#include <chrono>

#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/RadioDiscovery.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TstFirstRunPromptsTestRun : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("first-run-prompts-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        QVERIFY(!AppSettings::instance().remoteBackend());
        AppSettings::instance().clear();
        AppSettings::instance().ensureSettingsAtVersion(6);
        // No discovery broadcast onto the LAN.
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{5});
        BuildIdentity::setBuildTag(QString());
    }

    void cleanup()
    {
        RadioDiscovery::clearHoldOffForTest();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // The rule: a test binary runs in QStandardPaths test mode, and in it
    // first-run prompts are barred.
    void firstRunPromptsAreBarredInATestRun()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QVERIFY(MainWindow::firstRunPromptsBarredForTestRun());
    }

    // A local window built with no first-run setting recorded opens no
    // modal dialog while its deferred start-up calls run.
    void localWindowOpensNoModalDialog()
    {
        QStringList modals;
        QTimer watch;
        watch.setInterval(10);
        connect(&watch, &QTimer::timeout, this, [&modals] {
            QWidget* w = QApplication::activeModalWidget();
            if (w == nullptr) { return; }
            modals << QString::fromLatin1(w->metaObject()->className())
                          + QStringLiteral(" \"") + w->windowTitle() + QStringLiteral("\"");
            // Close it so the test fails instead of waiting for a click.
            if (auto* d = qobject_cast<QDialog*>(w)) {
                d->reject();
            } else {
                w->close();
            }
        });
        watch.start();

        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        QPointer<MainWindow> window = sessions.window();
        QVERIFY(window != nullptr);
        QVERIFY(window->radioModel()->ownsLocalDsp());
        window->show();
        // The start-up prompts are posted with singleShot(0); give them
        // several passes of the event loop.
        QTest::qWait(300);
        watch.stop();

        QVERIFY2(modals.isEmpty(), qPrintable(modals.join(QStringLiteral("; "))));
        QVERIFY(sessions.replace({}, false));
    }
};

QTEST_MAIN(TstFirstRunPromptsTestRun)
#include "tst_first_run_prompts_test_run.moc"
