// no-port-check: NereusSDR-original radio-free container position regressions.
// Modification history (NereusSDR):
//   2026-10-04 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QScreen>
#include <QSignalSpy>
#include <QSplitter>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/settings/ISettingsBackend.h"
#include "core/settings/SettingsScope.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/FloatingContainer.h"

using namespace NereusSDR;

namespace {
class StationWrites : public ISettingsBackend
{
public:
    int requests = 0;
    bool handlesKey(const QString& key) const override
    {
        return classifySettingsKey(key) == SettingsScope::Station;
    }
    QVariant value(const QString&, const QVariant& fallback) const override { return fallback; }
    void setValue(const QString&, const QVariant&) override { ++requests; }
    bool contains(const QString&) const override { return false; }
    void remove(const QString&) override { ++requests; }
    QStringList handledKeys() const override { return {}; }
};

WorkspaceDocument fixture(DockMode mode, const QRect& geometry)
{
    ContentEntry opaque;
    opaque.id = QStringLiteral("opaque-entry");
    opaque.typeId = QStringLiteral("opaque:position-fixture");
    opaque.config = {{QStringLiteral("serializedFont"), QStringLiteral("Example Font,13,-1,5,700,1,0,0,0,0")},
                     {QStringLiteral("legacyRecord"), QStringLiteral("FUTURE| exact bytes |\nfont=custom")}};
    opaque.extensions = {{QStringLiteral("futureEntry"), QJsonObject{{QStringLiteral("value"), 37}}}};
    ContainerDocument container;
    container.id = QStringLiteral("position");
    container.dockMode = mode;
    container.layout = ContentLayout::VerticalStack;
    container.geometry = geometry;
    container.contents = {opaque};
    container.config = {{QStringLiteral("futureContainer"), QStringLiteral("preserve")}};
    container.extensions = {{QStringLiteral("futureShell"), QJsonArray{1, 2, 3}}};
    WorkspaceDocument document;
    document.mainContainerId = container.id;
    document.containers = {container};
    document.extensions = {{QStringLiteral("futureWorkspace"), QStringLiteral("retain")}};
    return document;
}

void settleLayouts()
{
    // Deliver layout work synchronously without giving the geometry debounce
    // timer an opportunity to hide the pending-position regression.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
}
} // namespace

class TstContainerPositionRestore : public QObject
{
    Q_OBJECT
private slots:
    void pendingFloatingPositionSurvivesRuntimeProjection_data()
    {
        QTest::addColumn<QString>("projection");
        QTest::newRow("availability-before-debounce") << QStringLiteral("availability");
        QTest::newRow("singleton-attach-before-debounce") << QStringLiteral("attach");
        QTest::newRow("unrelated-commit-before-debounce") << QStringLiteral("unrelated");
        QTest::newRow("intentional-geometry-supersedes-pending") << QStringLiteral("intentional");
    }

    void pendingFloatingPositionSurvivesRuntimeProjection()
    {
        QFETCH(QString, projection);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect available = screen->availableGeometry();
        if (available.width() < 440 || available.height() < 360) {
            QSKIP("Position fixture requires room for a valid visible floating rectangle");
        }
        const QString path = directory.filePath(QStringLiteral("position.settings"));
        StationWrites backend;
        WorkspaceDocument saved;
        QRect requested;
        QRect projected;
        const WorkspaceDocument initial = fixture(DockMode::Floating,
                                                  QRect(available.topLeft() + QPoint(30, 30), QSize(320, 200)));
        {
            AppSettings settings(path);
            settings.setRemoteBackend(&backend);
            ContainerWorkspaceStore store(settings);
            QWidget dock;
            dock.resize(1000, 700);
            QSplitter splitter;
            QWidget borrowed(&dock);
            ContainerContentRegistry registry;
            ContainerManager manager(&dock, &splitter);
            manager.setWorkspaceAdapter(&store, &registry);
            QSignalSpy errors(&manager, &ContainerManager::workspaceError);
            QCOMPARE(manager.commitWorkspace(initial, 0).status, CommitStatus::Saved);
            settleLayouts();
            manager.saveState();
            ContainerWidget* container = manager.container(QStringLiteral("position"));
            QVERIFY(container);
            FloatingContainer* form = qobject_cast<FloatingContainer*>(container->window());
            QVERIFY(form);
            requested = form->geometry();
            requested.moveTopLeft(available.topLeft() + QPoint(70, 65));
            QVERIFY2(available.contains(requested), "Target must avoid screen recovery policy");
            form->setGeometry(requested);
            QCOMPARE(form->geometry(), requested);
            // Both events are synchronous. No sleep or event-loop spin lets
            // the pending 200 ms geometry save run before reconciliation.
            if (projection == QLatin1String("attach")) {
                registry.attachSingleton(QStringLiteral("applet:rx"), &borrowed);
            } else if (projection == QLatin1String("availability")) {
                registry.setAvailable(QStringLiteral("applet:rx"), false, QStringLiteral("Fixture unavailable"));
            } else {
                WorkspaceDocument draft = store.snapshot();
                draft.containers.first().name = QStringLiteral("Unrelated metadata edit");
                if (projection == QLatin1String("intentional")) {
                    requested.moveTopLeft(available.topLeft() + QPoint(85, 80));
                    QVERIFY(available.contains(requested));
                    draft.containers.first().geometry = requested;
                }
                QCOMPARE(manager.commitWorkspace(draft, draft.revision).status, CommitStatus::Saved);
            }
            projected = form->geometry();
            manager.saveState();
            saved = store.snapshot();
            QCOMPARE(errors.count(), 0);
        }
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().isEmpty());
        AppSettings actual(path);
        actual.load();
        actual.setRemoteBackend(&backend);
        ContainerWorkspaceStore reopened(actual);
        QVERIFY(reopened.load().ok);
        const DocumentResult decoded = ContainerDocumentCodec::decode(actual.value(QStringLiteral("ContainerWorkspace")).toString().toUtf8());
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, saved);
        QCOMPARE(saved.containers.first().contents, initial.containers.first().contents);
        QCOMPARE(saved.containers.first().extensions, initial.containers.first().extensions);
        QCOMPARE(saved.containers.first().config.value(QStringLiteral("futureContainer")),
                 initial.containers.first().config.value(QStringLiteral("futureContainer")));
        QCOMPARE(saved.extensions.value(QStringLiteral("futureWorkspace")), initial.extensions.value(QStringLiteral("futureWorkspace")));
        QWidget dock;
        dock.resize(1000, 700);
        QSplitter splitter;
        ContainerContentRegistry registry;
        ContainerManager manager(&dock, &splitter);
        manager.setWorkspaceAdapter(&reopened, &registry);
        manager.restoreState();
        settleLayouts();
        ContainerWidget* restored = manager.container(QStringLiteral("position"));
        QVERIFY(restored);
        QCOMPARE(projected, requested);
        QCOMPARE(saved.containers.first().geometry, requested);
        QCOMPARE(restored->window()->geometry(), requested);
        QCOMPARE(backend.requests, 0);
    }

    void restoredOverlayUsesFinalParentAnchorBaseline()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("overlay.settings"));
        const QRect requested(40, 55, 320, 200);
        const WorkspaceDocument initial = fixture(DockMode::OverlayDocked, requested);
        QCOMPARE(initial.containers.first().anchor, AxisLock::Left);
        {
            AppSettings settings(path);
            ContainerWorkspaceStore store(settings);
            QCOMPARE(store.commit(initial, 0).status, CommitStatus::Saved);
        }
        AppSettings actual(path);
        actual.load();
        ContainerWorkspaceStore reopened(actual);
        QVERIFY(reopened.load().ok);
        QCOMPARE(reopened.snapshot().containers.first().geometry, requested);
        QWidget dock;
        dock.resize(1000, 700);
        QSplitter splitter;
        ContainerContentRegistry registry;
        ContainerManager manager(&dock, &splitter);
        manager.setWorkspaceAdapter(&reopened, &registry);
        manager.restoreState();
        dock.show();
        settleLayouts();
        ContainerWidget* restored = manager.container(QStringLiteral("position"));
        QVERIFY(restored);
        QCOMPARE(restored->dockMode(), DockMode::OverlayDocked);
        QCOMPARE(restored->geometry(), requested);
        // MainWindow forwards final pane dimensions on its initial resize.
        // No size change means no anchored displacement is warranted.
        manager.updateDockedPositions(dock.width(), dock.height());
        const QRect projected = restored->geometry();
        manager.saveState();
        AppSettings persisted(path);
        persisted.load();
        ContainerWorkspaceStore disk(persisted);
        QVERIFY(disk.load().ok);
        QCOMPARE(disk.snapshot().containers.first().contents, initial.containers.first().contents);
        QCOMPARE(disk.snapshot().containers.first().extensions, initial.containers.first().extensions);
        QCOMPARE(projected, requested);
        QCOMPARE(disk.snapshot().containers.first().geometry, requested);
        // Left anchoring keeps x and follows the parent's vertical change.
        // The first unchanged-parent update must not have accumulated an offset.
        dock.resize(1000, 720);
        manager.updateDockedPositions(dock.width(), dock.height());
        QCOMPARE(restored->pos(), requested.topLeft() + QPoint(0, 20));
    }

    void failedGeometrySaveRestoresCommittedPlacement()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("failed.settings"));
        const QRect original(40, 55, 320, 200);
        AppSettings settings(path);
        ContainerWorkspaceStore store(settings);
        QWidget dock;
        dock.resize(1000, 700);
        QSplitter splitter;
        ContainerContentRegistry registry;
        ContainerManager manager(&dock, &splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        const WorkspaceDocument initial = fixture(DockMode::OverlayDocked, original);
        QCOMPARE(manager.commitWorkspace(initial, 0).status, CommitStatus::Saved);
        dock.show();
        settleLayouts();
        manager.saveState();
        const WorkspaceDocument committed = store.snapshot();
        ContainerWidget* container = manager.container(QStringLiteral("position"));
        QVERIFY(container);
        QCOMPARE(container->geometry(), original);
        QSignalSpy errors(&manager, &ContainerManager::workspaceError);
        container->move(75, 80);
        // A directory cannot be atomically replaced by the settings XML file.
        // Only this test's explicitly temporary path is made unwritable.
        QVERIFY(QFile::remove(path));
        QVERIFY(QDir().mkpath(path));
        manager.saveState();
        QCOMPARE(errors.count(), 1);
        QCOMPARE(store.snapshot(), committed);
        QCOMPARE(container->geometry(), original);
        QCOMPARE(settings.value(QStringLiteral("ContainerWorkspace")).toString().toUtf8(),
                 ContainerDocumentCodec::encode(committed));
        // Runtime refresh after the rejected save must not resurrect the draft.
        registry.setAvailable(QStringLiteral("applet:rx"), false, QStringLiteral("Fixture unavailable"));
        QCOMPARE(container->geometry(), original);
    }
};

QTEST_MAIN(TstContainerPositionRestore)
#include "tst_container_position_restore.moc"
