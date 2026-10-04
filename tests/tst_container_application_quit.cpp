// no-port-check: NereusSDR-original radio-free application quit regressions.
// Modification history (NereusSDR):
//   2026-10-04 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QFile>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QSignalSpy>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>

#include "core/AppSettings.h"
#include "gui/GuiApplication.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/FloatingContainer.h"

using namespace NereusSDR;

namespace {
constexpr int kWatchdogMs = 2000;
constexpr int kWatchdogExit = 70;
constexpr int kAssertionExit = 71;

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

// A modal top-level rejects the first Quit close sweep before Qt visits
// nonmodal forms. Its queued continuation runs after the Quit event unwinds.
class RejectingWindow : public QDialog
{
public:
    std::function<void()> afterRejectedClose;
    int rejectedCloses = 0;

protected:
    void closeEvent(QCloseEvent* event) override
    {
        ++rejectedCloses;
        event->ignore();
        QTimer::singleShot(0, this, [this] {
            if (afterRejectedClose) {
                afterRejectedClose();
            }
        });
    }
};

class SavingWindow : public QWidget
{
public:
    std::function<void()> beforeClose;

protected:
    void closeEvent(QCloseEvent* event) override
    {
        if (beforeClose) {
            beforeClose();
        }
        QWidget::closeEvent(event);
    }
};

class QuitPreflight : public QObject
{
public:
    std::function<void()> afterRejectedQuit;
    int rejectedQuits = 0;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == qApp && event->type() == QEvent::Quit) {
            ++rejectedQuits;
            event->ignore();
            QTimer::singleShot(0, this, [this] { afterRejectedQuit(); });
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

int runQuitChild(QApplication& application, const QString& scenario)
{
    QStringList failures;
    const auto check = [&failures](bool condition, const QString& message) {
        if (!condition) {
            failures.append(message);
        }
    };
    QTemporaryDir directory;
    if (!directory.isValid()) {
        qCritical() << "Could not create temporary settings directory";
        return kAssertionExit;
    }
    AppSettings settings(directory.filePath("settings.xml"));
    ContainerWorkspaceStore store(settings);
    if (!store.load().ok) {
        qCritical() << "Could not load empty fixture workspace";
        return kAssertionExit;
    }

    SavingWindow window;
    window.resize(800, 600);
    QVBoxLayout layout(&window);
    QSplitter splitter(&window);
    QWidget pane(&splitter);
    layout.addWidget(&splitter);
    splitter.addWidget(&pane);
    // Registry borrows this real control and returns it to its construction
    // owner when the host changes. No radio, meter renderer or DSP is needed.
    QPushButton borrowed(QStringLiteral("Borrowed RX control"));
    ContainerContentRegistry registry;
    registry.attachSingleton(QStringLiteral("applet:rx"), &borrowed);
    ContainerManager manager(&pane, &splitter);
    manager.setWorkspaceAdapter(&store, &registry);

    ContainerDocument main;
    main.id = QStringLiteral("main");
    main.layout = ContentLayout::VerticalStack;
    ContentEntry head;
    head.id = QStringLiteral("head");
    head.typeId = QStringLiteral("opaque:quit-fixture");
    ContentEntry rx = registry.makeEntry(QStringLiteral("applet:rx"));
    rx.id = QStringLiteral("rx-entry");
    ContentEntry tail = head;
    tail.id = QStringLiteral("tail");
    main.contents = {head, rx, tail};
    ContainerDocument ordinary;
    ordinary.id = QStringLiteral("ordinary");
    ordinary.layout = ContentLayout::VerticalStack;
    ordinary.dockMode = DockMode::Floating;
    ordinary.geometry = QRect(160, 160, 360, 240);
    WorkspaceDocument document;
    document.mainContainerId = main.id;
    document.containers = {main, ordinary};
    if (manager.commitWorkspace(document, store.snapshot().revision).status != CommitStatus::Saved
        || !manager.arrangeController()->popOut(rx.id).ok) {
        qCritical() << "Could not create actual workspace pop-out fixture";
        return kAssertionExit;
    }
    const QString shellId = store.snapshot().containers.last().id;
    const bool locked = scenario == QLatin1String("locked-quit")
        || scenario == QLatin1String("ordinary-close")
        || scenario == QLatin1String("shutdown-save");
    document = store.snapshot();
    document.containers.last().locked = locked;
    if (manager.commitWorkspace(document, document.revision).status != CommitStatus::Saved) {
        qCritical() << "Could not configure pop-out lock";
        return kAssertionExit;
    }
    QPointer<FloatingContainer> shell =
        qobject_cast<FloatingContainer*>(manager.container(shellId)->window());
    QPointer<FloatingContainer> form =
        qobject_cast<FloatingContainer*>(manager.container(ordinary.id)->window());
    if (!shell || !form) {
        qCritical() << "Expected real structured floating windows";
        return kAssertionExit;
    }
    QSignalSpy shellCloseRequests(shell, &FloatingContainer::aboutToClose);
    QSignalSpy formCloseRequests(form, &FloatingContainer::aboutToClose);
    QSignalSpy commits(&store, &ContainerWorkspaceStore::committed);
    QSignalSpy errors(&manager, &ContainerManager::workspaceError);
    QSignalSpy aboutToQuit(&application, &QCoreApplication::aboutToQuit);
    RejectingWindow rejectingWindow;
    QuitPreflight preflight;
    bool canceledContinuation = false;
    bool nestedRejected = false;
    bool watchdogFired = false;
    QByteArray beforeDocument;
    QByteArray beforeSettings;
    WorkspaceDocument shutdownExpected;
    bool shutdownSaveRan = false;
    window.show();
    application.setQuitOnLastWindowClosed(false);

    const auto ordinaryClose = [&] {
        check(shell && shell->isVisible(), QStringLiteral("ordinary close starts with visible shell"));
        if (!shell || !form) {
            return;
        }
        check(!shell->close(), QStringLiteral("ordinary shell close retains workspace interception"));
        check(!manager.container(shellId), QStringLiteral("ordinary shell close removes the pop-out"));
        check(borrowed.window() == &window, QStringLiteral("ordinary shell close rehomes borrowed control"));
        const WorkspaceDocument returned = store.snapshot();
        QStringList ids;
        for (const ContentEntry& entry : returned.containers.first().contents) {
            ids.append(entry.id);
        }
        check(ids == QStringList{QStringLiteral("head"), QStringLiteral("rx-entry"), QStringLiteral("tail")},
              QStringLiteral("ordinary close preserves return order"));
        check(!form->close(), QStringLiteral("ordinary form close retains workspace interception"));
        check(!form->isVisible(), QStringLiteral("ordinary form close hides the window"));
        check(store.snapshot().containers.size() == 2
                  && !store.snapshot().containers[1].visible,
              QStringLiteral("ordinary form close persists hidden state without deleting it"));
        check(commits.count() == 2, QStringLiteral("ordinary close commits exactly return and hide"));
        check(shellCloseRequests.count() == 1 && formCloseRequests.count() == 1,
              QStringLiteral("ordinary close emits one workspace request per form"));
    };

    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &application, [&] {
        watchdogFired = true;
        qCritical() << "Quit child watchdog: event loop did not quit; exiting cleanly";
        application.exit(kWatchdogExit);
    });
    QTimer::singleShot(0, &application, [&] {
        // Finish initial layout before defining the persisted baseline; the
        // explicit fixture commit also stops the manager's geometry debounce.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        manager.saveState();
        const WorkspaceDocument settled = store.snapshot();
        check(manager.commitWorkspace(settled, settled.revision).status == CommitStatus::Saved,
              QStringLiteral("fixture settles its geometry before exercise"));
        commits.clear();
        errors.clear();
        beforeDocument = ContainerDocumentCodec::encode(store.snapshot());
        beforeSettings = readFile(settings.filePath());
        check(!beforeSettings.isEmpty(), QStringLiteral("fixture persisted temporary settings"));
        check(shell && shell->isVisible() && form && form->isVisible(),
              QStringLiteral("fixture has multiple visible floating forms"));
        check(borrowed.window() == shell, QStringLiteral("borrowed applet begins in real pop-out"));
        check(!GuiApplication::applicationQuitInProgress(),
              QStringLiteral("ordinary running application has no quit scope"));
        watchdog.start(kWatchdogMs);

        if (scenario == QLatin1String("nested-quit")) {
            window.beforeClose = [&] {
                check(GuiApplication::applicationQuitInProgress(),
                      QStringLiteral("outer close runs inside application Quit"));
                rejectingWindow.setWindowModality(Qt::ApplicationModal);
                rejectingWindow.show();
                QEvent nestedQuit(QEvent::Quit);
                QCoreApplication::sendEvent(&application, &nestedQuit);
                nestedRejected = rejectingWindow.rejectedCloses == 1 && aboutToQuit.isEmpty();
                check(nestedRejected, QStringLiteral("nested Quit is rejected before terminal shutdown"));
                rejectingWindow.hide();
                check(GuiApplication::applicationQuitInProgress(),
                      QStringLiteral("rejected nested Quit restores outer scope"));
                check(shell && shell->close() && form && form->close(),
                      QStringLiteral("outer structured closes still accept after nested rejection"));
            };
        }
        if (scenario == QLatin1String("preflight-quit")) {
            preflight.afterRejectedQuit = [&] {
                canceledContinuation = true;
                check(aboutToQuit.isEmpty() && !GuiApplication::applicationQuitInProgress(),
                      QStringLiteral("preflight veto runs before application Quit scope"));
                check(ContainerDocumentCodec::encode(store.snapshot()) == beforeDocument,
                      QStringLiteral("preflight veto never closes or mutates workspace"));
                ordinaryClose();
                application.exit(failures.isEmpty() ? 0 : kAssertionExit);
            };
            application.installEventFilter(&preflight);
        }
        if (scenario == QLatin1String("late-save")) {
            shutdownExpected = store.snapshot();
            QObject::connect(&application, &QCoreApplication::aboutToQuit, &window, [&] {
                shutdownSaveRan = true;
                check(GuiApplication::applicationQuitInProgress(),
                      QStringLiteral("aboutToQuit save has terminal shutdown protection"));
                if (!form) {
                    return;
                }
                form->move(form->pos() + QPoint(13, 0));
                shutdownExpected.containers[1].geometry = form->geometry();
                ++shutdownExpected.revision;
                manager.saveState();
                check(shell && !shell->isVisible() && !form->isVisible(),
                      QStringLiteral("aboutToQuit save does not reshow accepted forms"));
            });
        }
        if (scenario == QLatin1String("shutdown-save")) {
            shutdownExpected = store.snapshot();
            window.beforeClose = [&] {
                shutdownSaveRan = true;
                // Exercise final save inside the actual application close
                // sweep. Explicitly close forms first to cover both possible
                // Qt top-level visitation orders without assuming one.
                check(shell && shell->close(), QStringLiteral("shutdown shell close accepts"));
                check(form && form->close(), QStringLiteral("shutdown ordinary form close accepts"));
                if (!form) {
                    return;
                }
                form->move(form->pos() + QPoint(13, 0));
                shutdownExpected.containers[1].geometry = form->geometry();
                ++shutdownExpected.revision;
                manager.saveState();
                check(shell && !shell->isVisible() && !form->isVisible(),
                      QStringLiteral("shutdown geometry save does not reshow accepted forms"));
            };
        }
        if (scenario == QLatin1String("ordinary-close")) {
            check(!shell->close(), QStringLiteral("locked ordinary close is rejected"));
            check(shell->isVisible() && manager.container(shellId),
                  QStringLiteral("locked ordinary close retains shell"));
            check(ContainerDocumentCodec::encode(store.snapshot()) == beforeDocument,
                  QStringLiteral("locked ordinary close does not mutate document"));
            check(commits.isEmpty() && errors.count() == 1,
                  QStringLiteral("locked ordinary close reports arrangement rejection"));
            document = store.snapshot();
            document.containers.last().locked = false;
            check(manager.commitWorkspace(document, document.revision).status == CommitStatus::Saved,
                  QStringLiteral("fixture unlocks for normal return"));
            commits.clear();
            shellCloseRequests.clear();
            errors.clear();
            ordinaryClose();
            application.exit(failures.isEmpty() ? 0 : kAssertionExit);
            return;
        }
        if (scenario == QLatin1String("canceled-quit")) {
            rejectingWindow.setWindowModality(Qt::ApplicationModal);
            rejectingWindow.afterRejectedClose = [&] {
                canceledContinuation = true;
                check(aboutToQuit.isEmpty(), QStringLiteral("rejecting window cancels Quit"));
                check(!GuiApplication::applicationQuitInProgress(),
                      QStringLiteral("canceled Quit rolls back application scope"));
                check(ContainerDocumentCodec::encode(store.snapshot()) == beforeDocument,
                      QStringLiteral("canceled Quit preserves workspace"));
                rejectingWindow.hide();
                ordinaryClose();
                application.exit(failures.isEmpty() ? 0 : kAssertionExit);
            };
            rejectingWindow.show();
        }
        // Exercise QApplication's real queued QEvent::Quit/closeAllWindows
        // path inside exec(), rather than sending a close to a naked helper.
        QMetaObject::invokeMethod(&application, "quit", Qt::QueuedConnection);
    });

    const int eventLoopResult = application.exec();
    watchdog.stop();
    check(!watchdogFired, QStringLiteral("queued application quit finishes without watchdog"));
    check(eventLoopResult == 0, QStringLiteral("application event loop returns zero"));
    check(aboutToQuit.count() == 1, QStringLiteral("application emits aboutToQuit once"));
    if (scenario == QLatin1String("locked-quit") || scenario == QLatin1String("unlocked-quit")
        || scenario == QLatin1String("shutdown-save") || scenario == QLatin1String("late-save")
        || scenario == QLatin1String("nested-quit")) {
        check(shell && !shell->isVisible() && form && !form->isVisible(),
              QStringLiteral("all structured floating closes accept the application quit"));
        check(shellCloseRequests.isEmpty() && formCloseRequests.isEmpty(),
              QStringLiteral("application quit emits no workspace arrange requests"));
        if (scenario == QLatin1String("shutdown-save") || scenario == QLatin1String("late-save")) {
            check(shutdownSaveRan && commits.count() == 1 && errors.isEmpty(),
                  QStringLiteral("shutdown saves geometry once without arrangement errors"));
            check(ContainerDocumentCodec::encode(store.snapshot())
                      == ContainerDocumentCodec::encode(shutdownExpected),
                  QStringLiteral("shutdown save changes only geometry and revision"));
            AppSettings persisted(settings.filePath());
            persisted.load();
            check(persisted.contains(QStringLiteral("ContainerWorkspace"))
                      && !persisted.wasCorruptedOnLoad(),
                  QStringLiteral("shutdown readback loads the persisted workspace without corruption"));
            ContainerWorkspaceStore reopened(persisted);
            const DocumentResult reopenedResult = reopened.load();
            check(reopenedResult.ok,
                  QStringLiteral("shutdown readback decodes the persisted workspace: %1").arg(reopenedResult.error));
            check(ContainerDocumentCodec::encode(reopened.snapshot())
                      == ContainerDocumentCodec::encode(shutdownExpected),
                  QStringLiteral("shutdown geometry is actually persisted"));
        } else {
            check(commits.isEmpty() && errors.isEmpty(),
                  QStringLiteral("application quit performs no workspace transactions or errors"));
            check(ContainerDocumentCodec::encode(store.snapshot()) == beforeDocument,
                  QStringLiteral("application quit leaves document and revision unchanged"));
            check(readFile(settings.filePath()) == beforeSettings,
                  QStringLiteral("application quit leaves persisted settings bytes unchanged"));
        }
        check(borrowed.window() == shell, QStringLiteral("application quit does not rehome applet"));
        if (scenario == QLatin1String("nested-quit")) {
            check(nestedRejected, QStringLiteral("nested Quit rejection was exercised"));
        }
    } else if (scenario == QLatin1String("canceled-quit")) {
        check(canceledContinuation && rejectingWindow.rejectedCloses == 1,
              QStringLiteral("ordinary closes run after one canceled Quit"));
    } else if (scenario == QLatin1String("preflight-quit")) {
        check(canceledContinuation && preflight.rejectedQuits == 1,
              QStringLiteral("ordinary closes run after one preflight Quit veto"));
    }
    qInfo().noquote() << QStringLiteral("QUIT_CHILD scenario=%1 exec=%2 watchdog=%3 aboutToQuit=%4 shellRequests=%5 formRequests=%6 commits=%7 failures=%8")
        .arg(scenario).arg(eventLoopResult).arg(watchdogFired).arg(aboutToQuit.count())
        .arg(shellCloseRequests.count()).arg(formCloseRequests.count()).arg(commits.count()).arg(failures.size());
    for (const QString& failure : failures) {
        qCritical().noquote() << "QUIT_CHILD assertion:" << failure;
    }
    return failures.isEmpty() ? 0 : kAssertionExit;
}
} // namespace

class TstContainerApplicationQuit : public QObject
{
    Q_OBJECT
private slots:
    void actualApplicationLoop_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::newRow("locked-shell-quit") << QStringLiteral("locked-quit");
        QTest::newRow("unlocked-shell-quit") << QStringLiteral("unlocked-quit");
        QTest::newRow("shutdown-geometry-save-keeps-forms-closed") << QStringLiteral("shutdown-save");
        QTest::newRow("aboutToQuit-geometry-save-keeps-forms-closed") << QStringLiteral("late-save");
        QTest::newRow("nested-rejected-quit-keeps-outer-scope") << QStringLiteral("nested-quit");
        QTest::newRow("preflight-veto-preserves-ordinary-close") << QStringLiteral("preflight-quit");
        QTest::newRow("ordinary-close-rehome-hide") << QStringLiteral("ordinary-close");
        QTest::newRow("canceled-quit-restores-ordinary-close") << QStringLiteral("canceled-quit");
    }

    void actualApplicationLoop()
    {
        QFETCH(QString, scenario);
        QProcess child;
        child.setProcessChannelMode(QProcess::MergedChannels);
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        child.setProcessEnvironment(environment);
        child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--quit-child"), scenario});
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
    if (application.arguments().size() == 3
        && application.arguments()[1] == QLatin1String("--quit-child")) {
        return runQuitChild(application, application.arguments()[2]);
    }
    TstContainerApplicationQuit test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_container_application_quit.moc"
