// no-port-check: NereusSDR-original offscreen regression: a refused container change says why.
// Modification history (NereusSDR):
//   2026-10-26 - A refused container change says why (JJ's bench: adding a
//                 container failed with nothing he noticed). In the real
//                 MainWindow, offscreen: while the saved layout changed
//                 underneath or failed to load, a new container is refused,
//                 the refusal is logged, and a notice stays up past the
//                 toast's life until a good reload takes it down;
//                 Containers > New Container and Container Settings New
//                 say why in a message box; a one-off refusal toasts
//                 "Container change not saved"; a saved change shows
//                 nothing new. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - A refused New Container (menu or Container Settings)
//                 shows the box alone, with no toast on top: a save that
//                 fails on disk (settings folder read-only) is refused once
//                 and says so only in the box. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-27 - A successful reload clears a one-off disk save error even
//                 when the store was never blocked. J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
#include <QtTest/QtTest>
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <chrono>
#include <functional>
#include <memory>

#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/MainWindow.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/StatusToast.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

constexpr QSize kWindowSize(1500, 950);
// The one-off refusal toast's life in MainWindow.
constexpr int kToastLifeMs = 6000;
constexpr int kPastToastLifeMs = kToastLifeMs + 500;
// Generous: the capture itself waits 200 ms.
constexpr int kStartupCaptureTimeoutMs = 5000;

const QString kReadOnly = QStringLiteral("Container layout is read-only:");
const QString kNotSaved = QStringLiteral("Container change not saved:");
const QString kWhatToDo = QStringLiteral(
    "To save container changes again, open Container Settings from a container's menu and "
    "press Reload conflicting containers, or restart NereusSDR.");
const QString kChanged = QStringLiteral("Workspace changed; reload before editing");

void settle()
{
    for (int pass = 0; pass < 8; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

std::unique_ptr<MainWindow> buildWindow()
{
    WorkspacePreview::setEnabledForTest(false);
    auto& app = *qobject_cast<QApplication*>(QCoreApplication::instance());
    applyDarkPalette(app);
    applyAppBaselineQss(app);
    auto window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr,
                                               MainWindow::ConnectionStartup::Deferred);
    window->radioModel()->setBoardForTest(HPSDRHW::Saturn);
    window->resize(kWindowSize);
    window->show();
    settle();
    // The first show moves and sizes the containers, which starts the
    // manager's geometry capture. It saves 200 ms later; let it, so a case
    // that makes saving fail refuses only its own action, as an operator's
    // would long after the window came up.
    if (auto* manager = window->findChild<ContainerManager*>()) {
        if (!QTest::qWaitFor([manager] { return !manager->geometryCommitPendingForTest(); },
                             kStartupCaptureTimeoutMs)) {
            QTest::qFail("The startup geometry capture did not finish", __FILE__, __LINE__);
        }
    }
    return window;
}

// The shown toasts whose text starts with `prefix`.
QList<StatusToast*> toasts(const MainWindow& window, const QString& prefix)
{
    QList<StatusToast*> found;
    for (StatusToast* toast : window.findChildren<StatusToast*>()) {
        if (toast->isVisible() && toast->message().startsWith(prefix)) {
            found.append(toast);
        }
    }
    return found;
}

// Every message box shown while `action` runs, closed once it has been up
// for kBoxUpMs; any other modal dialog (Container Settings) is cancelled.
// A box stays up longer than ContainerManager's 200 ms geometry capture
// (m_geometryCommit), as it does in front of an operator and on a slow
// runner, so a save that runs while the box is up shows in the test.
constexpr int kBoxUpMs = 300;
struct ShownBox {
    QString title;
    QString text;
};
QList<ShownBox> boxesDuring(const std::function<void()>& action)
{
    QList<ShownBox> shown;
    QList<QPointer<QMessageBox>> seen;
    QTimer poll;
    poll.setInterval(20);
    QObject::connect(&poll, &QTimer::timeout, [&shown, &seen] {
        QWidget* modal = QApplication::activeModalWidget();
        if (auto* box = qobject_cast<QMessageBox*>(modal)) {
            if (!seen.contains(box)) {
                seen.append(box);
                shown.append({box->windowTitle(), box->text()});
                QTimer::singleShot(kBoxUpMs, box, [box] { box->done(QMessageBox::Ok); });
            }
        } else if (auto* dialog = qobject_cast<QDialog*>(modal)) {
            dialog->reject();
        }
    });
    poll.start();
    action();
    settle();
    poll.stop();
    return shown;
}

QAction* newContainerAction(MainWindow& window)
{
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == QStringLiteral("&New Container...")) {
            return action;
        }
    }
    return nullptr;
}

// Another writer replaces the saved layout behind the store.
void changeBytesUnderTheStore(ContainerWorkspaceStore& store)
{
    WorkspaceDocument external = store.snapshot();
    external.revision += 5;
    AppSettings::instance().setValue(store.workspaceKey(),
                                     QString::fromUtf8(ContainerDocumentCodec::encode(external)));
}

// A message box on macOS has no title of its own; elsewhere it names the
// action that was refused.
void checkTitle(const QString& title)
{
#ifdef Q_OS_MAC
    QVERIFY2(title.isEmpty() || title == QStringLiteral("New Container"), qPrintable(title));
#else
    QCOMPARE(title, QStringLiteral("New Container"));
#endif
}

// The settings folder made read-only for the life of this, so a save
// fails on disk: a one-off refusal, not a state that refuses every save.
class ReadOnlySettingsFolder {
public:
    ReadOnlySettingsFolder()
        : m_folder(QFileInfo(AppSettings::instance().filePath()).absolutePath())
        , m_before(QFile::permissions(m_folder))
    {
        QFile::setPermissions(m_folder, QFile::ReadOwner | QFile::ExeOwner);
    }
    ~ReadOnlySettingsFolder() { QFile::setPermissions(m_folder, m_before); }
    ReadOnlySettingsFolder(const ReadOnlySettingsFolder&) = delete;
    ReadOnlySettingsFolder& operator=(const ReadOnlySettingsFolder&) = delete;

private:
    QString m_folder;
    QFileDevice::Permissions m_before;
};

void expectRefusal(const QString& status, const QString& reason)
{
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(
        QStringLiteral("^Container workspace commit refused: %1 - %2$")
            .arg(status, QRegularExpression::escape(reason))));
}

} // namespace

class TstMainWindowContainerRefusedSave : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("mainwindow-container-refused-save")));
        qApp->setStyle(QStringLiteral("Fusion"));
        qApp->setQuitOnLastWindowClosed(false);
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{30});
    }
    void init() { QVERIFY(RemoteWindowHarness::clearIsolatedProfile()); }
    void cleanupTestCase()
    {
        WorkspacePreview::setEnabledForTest(false);
        RadioDiscovery::clearHoldOffForTest();
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
    }

    void bytesChangedUnderTheStoreStayOnScreenUntilReload()
    {
        auto window = buildWindow();
        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager && manager->workspaceStore());
        ContainerWorkspaceStore& store = *manager->workspaceStore();
        changeBytesUnderTheStore(store);

        expectRefusal(QStringLiteral("Conflict"), kChanged);
        QCOMPARE(manager->createContainer(1, DockMode::Floating), nullptr);
        settle();
        QList<StatusToast*> notices = toasts(*window, kReadOnly);
        QCOMPARE(notices.size(), 1);
        QVERIFY2(notices.first()->message().contains(kChanged), qPrintable(notices.first()->message()));
        QVERIFY2(notices.first()->message().endsWith(kWhatToDo), qPrintable(notices.first()->message()));
        // The notice says it once; no one-off toast on top of it.
        QCOMPARE(toasts(*window, kNotSaved).size(), 0);

        // Still there after the one-off toast would have gone.
        QTest::qWait(kPastToastLifeMs);
        QCOMPARE(toasts(*window, kReadOnly).size(), 1);

        // A good reload takes it down, and a new container saves.
        QVERIFY(store.load().ok);
        QTRY_COMPARE(toasts(*window, kReadOnly).size(), 0);
        QVERIFY(manager->createContainer(1, DockMode::Floating) != nullptr);
        window->close();
    }

    void aLoadErrorStaysOnScreenUntilAGoodReload()
    {
        auto window = buildWindow();
        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager && manager->workspaceStore());
        ContainerWorkspaceStore& store = *manager->workspaceStore();
        const QString good = AppSettings::instance().value(store.workspaceKey()).toString();
        AppSettings::instance().setValue(store.workspaceKey(), QStringLiteral("{truncated"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^Container workspace remains read-only:")));
        QVERIFY(!store.load().ok);
        const QString loadError = store.loadError();
        QVERIFY(!loadError.isEmpty());
        settle();
        QCOMPARE(toasts(*window, kReadOnly).size(), 1);

        expectRefusal(QStringLiteral("Invalid"), loadError);
        QCOMPARE(manager->createContainer(1, DockMode::Floating), nullptr);
        settle();
        QList<StatusToast*> notices = toasts(*window, kReadOnly);
        QCOMPARE(notices.size(), 1);
        QVERIFY2(notices.first()->message().endsWith(kWhatToDo), qPrintable(notices.first()->message()));
        QCOMPARE(toasts(*window, kNotSaved).size(), 0);
        QTest::qWait(kPastToastLifeMs);
        QCOMPARE(toasts(*window, kReadOnly).size(), 1);

        AppSettings::instance().setValue(store.workspaceKey(), good);
        QVERIFY(store.load().ok);
        QTRY_COMPARE(toasts(*window, kReadOnly).size(), 0);
        QVERIFY(manager->createContainer(1, DockMode::Floating) != nullptr);
        window->close();
    }

    void refusedNewContainerMenuSaysWhy()
    {
        auto window = buildWindow();
        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager && manager->workspaceStore());
        QAction* action = newContainerAction(*window);
        QVERIFY(action);
        const int before = manager->allContainers().size();
        changeBytesUnderTheStore(*manager->workspaceStore());

        expectRefusal(QStringLiteral("Conflict"), kChanged);
        const QList<ShownBox> boxes = boxesDuring([action] { action->trigger(); });
        QCOMPARE(boxes.size(), 1);
        checkTitle(boxes.first().title);
        QCOMPARE(boxes.first().text,
                 QStringLiteral("The new container could not be saved: %1. %2").arg(kChanged, kWhatToDo));
        QCOMPARE(manager->allContainers().size(), before);
        QCOMPARE(toasts(*window, kNotSaved).size(), 0);
        window->close();
    }

    void aRefusedNewContainerShowsTheBoxAlone()
    {
        auto window = buildWindow();
        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager && manager->workspaceStore());
        QAction* action = newContainerAction(*window);
        QVERIFY(action);
        const int before = manager->allContainers().size();
        QList<ShownBox> boxes;
        {
            ReadOnlySettingsFolder readOnly;
            boxes = boxesDuring([action] { action->trigger(); });
        }
        QCOMPARE(boxes.size(), 1);
        checkTitle(boxes.first().title);
        QVERIFY2(boxes.first().text.startsWith(QStringLiteral("The new container could not be saved: ")),
                 qPrintable(boxes.first().text));
        // A one-off refusal: the box says why and nothing about reloading.
        QVERIFY2(!boxes.first().text.contains(kWhatToDo), qPrintable(boxes.first().text));
        QCOMPARE(manager->allContainers().size(), before);
        QCOMPARE(toasts(*window, kNotSaved).size(), 0);
        QCOMPARE(toasts(*window, kReadOnly).size(), 0);
        // The next save goes through.
        QVERIFY(manager->createContainer(1, DockMode::Floating) != nullptr);
        window->close();
    }

    void aRefusedNewInContainerSettingsShowsTheBoxAlone()
    {
        auto window = buildWindow();
        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager && manager->workspaceStore());
        ContainerWorkspaceStore& store = *manager->workspaceStore();
        ContainerWidget* existing = manager->container(store.snapshot().mainContainerId);
        QVERIFY(existing);
        ContainerSettingsDialog dialog(existing, window.get(), manager);
        auto* create = dialog.findChild<QPushButton*>(QStringLiteral("createContainer"));
        QVERIFY(create);
        create->click();
        QPushButton* apply = nullptr;
        for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("Apply")) {
                apply = button;
            }
        }
        QVERIFY(apply);
        QList<ShownBox> boxes;
        {
            ReadOnlySettingsFolder readOnly;
            boxes = boxesDuring([apply] { apply->click(); });
        }
        QCOMPARE(boxes.size(), 1);
        QVERIFY2(boxes.first().text.startsWith(QStringLiteral("The new container could not be saved: ")),
                 qPrintable(boxes.first().text));
        QCOMPARE(toasts(*window, kNotSaved).size(), 0);
        dialog.reject();
        window->close();
    }

    void refusedNewInContainerSettingsSaysWhy()
    {
        auto window = buildWindow();
        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager && manager->workspaceStore());
        ContainerWorkspaceStore& store = *manager->workspaceStore();
        ContainerWidget* existing = manager->container(store.snapshot().mainContainerId);
        QVERIFY(existing);
        ContainerSettingsDialog dialog(existing, window.get(), manager);
        QVERIFY(dialog.editSession());
        auto* create = dialog.findChild<QPushButton*>(QStringLiteral("createContainer"));
        QVERIFY(create);
        create->click();
        QPushButton* ok = nullptr;
        for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("OK")) {
                ok = button;
            }
        }
        QVERIFY(ok);

        AppSettings::instance().setValue(store.workspaceKey(), QStringLiteral("{truncated"));
        const QList<ShownBox> boxes = boxesDuring([ok] { ok->click(); });
        QVERIFY(!store.loadError().isEmpty());
        QCOMPARE(boxes.size(), 1);
        checkTitle(boxes.first().title);
        QVERIFY2(boxes.first().text.startsWith(QStringLiteral("The new container could not be saved: %1")
                                                   .arg(store.loadError())),
                 qPrintable(boxes.first().text));
        QVERIFY2(boxes.first().text.endsWith(kWhatToDo), qPrintable(boxes.first().text));
        dialog.reject();
        window->close();
    }

    void aOneOffRefusalToastsChangeNotSaved()
    {
        auto window = buildWindow();
        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager && manager->workspaceStore());
        const WorkspaceDocument document = manager->workspaceStore()->snapshot();
        expectRefusal(QStringLiteral("Conflict"), QStringLiteral("Workspace revision changed"));
        QCOMPARE(manager->commitWorkspace(document, document.revision + 3).status, CommitStatus::Conflict);
        settle();
        const QList<StatusToast*> shown = toasts(*window, kNotSaved);
        QCOMPARE(shown.size(), 1);
        QCOMPARE(shown.first()->message(), QStringLiteral("Container change not saved: Workspace revision changed"));
        QCOMPARE(toasts(*window, kReadOnly).size(), 0);
        window->close();
    }

    void reloadClearsOneOffDiskSaveError()
    {
        auto window = buildWindow();
        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager && manager->workspaceStore());
        ContainerWorkspaceStore& store = *manager->workspaceStore();
        const WorkspaceDocument document = store.snapshot();
        QSignalSpy errors(manager, &ContainerManager::workspaceError);
        QVERIFY(errors.isValid());
        {
            ReadOnlySettingsFolder readOnly;
            QCOMPARE(manager->commitWorkspace(document, document.revision).status, CommitStatus::StorageError);
        }
        QVERIFY(!manager->storageError().isEmpty());
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors.first().first().toString(), manager->storageError());
        QVERIFY(store.blockedReason().isEmpty());

        // No manager commit may clear the error before this assertion.
        QVERIFY(store.load().ok);
        QVERIFY(store.blockedReason().isEmpty());
        QVERIFY(manager->storageError().isEmpty());
        QCOMPARE(errors.size(), 1);
        window->close();
    }

    void aSavedChangeShowsNothingNew()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral("^Container workspace commit refused")));
        auto window = buildWindow();
        auto* manager = window->findChild<ContainerManager*>();
        QVERIFY(manager && manager->workspaceStore());
        const QList<ShownBox> boxes = boxesDuring([manager] {
            ContainerWidget* created = manager->createContainer(1, DockMode::Floating);
            QVERIFY(created);
            const WorkspaceDocument document = manager->workspaceStore()->snapshot();
            QCOMPARE(manager->commitWorkspace(document, document.revision).status, CommitStatus::Saved);
        });
        QCOMPARE(boxes.size(), 0);
        QCOMPARE(toasts(*window, kReadOnly).size(), 0);
        QCOMPARE(toasts(*window, kNotSaved).size(), 0);
        window->close();
    }
};

QTEST_MAIN(TstMainWindowContainerRefusedSave)
#include "tst_mainwindow_container_refused_save.moc"
