// no-port-check: NereusSDR-original atomic arrangement regressions.
// Modification history (NereusSDR):
//   2026-10-02 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QFile>
#include "core/AppSettings.h"
#include "core/settings/ISettingsBackend.h"
#include "core/settings/SettingsScope.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include <QSplitter>
using namespace NereusSDR;
namespace
{
ContentEntry entry(const QString& id)
{
    ContentEntry e;
    e.id = id;
    e.typeId = "opaque:test";
    e.name = id;
    e.config = {{"nested", QJsonObject{{"value", id}}}};
    e.paintOrder = 73;
    return e;
}
ContainerDocument container(const QString& id, const QStringList& ids)
{
    ContainerDocument c;
    c.id = id;
    c.layout = ContentLayout::VerticalStack;
    for (const auto& i : ids) {
        c.contents.append(entry(i));
    }
    return c;
}
QStringList order(const WorkspaceDocument& d, const QString& id)
{
    QStringList ids;
    for (const auto& c : d.containers) {
        if (c.id == id) {
            for (const auto& e : c.contents) {
                ids << e.id;
            }
        }
    }
    return ids;
}
class Session : public ISettingsBackend
{
  public:
    int requests = 0;
    bool handlesKey(const QString& k) const override
    {
        return classifySettingsKey(k) == SettingsScope::Station;
    }
    QVariant value(const QString&, const QVariant& v) const override { return v; }
    void setValue(const QString&, const QVariant&) override { ++requests; }
    bool contains(const QString&) const override { return false; }
    void remove(const QString&) override { ++requests; }
    QStringList handledKeys() const override { return {}; }
};
} // namespace
class TstContainerArrangement : public QObject
{
    Q_OBJECT
  private slots:
    void returns_data()
    {
        QTest::addColumn<QStringList>("survivors");
        QTest::addColumn<QStringList>("shell");
        QTest::addColumn<QStringList>("before");
        QTest::addColumn<QStringList>("after");
        QTest::addColumn<QStringList>("expected");
        const auto row = [](const char* name, QStringList live, QStringList out, QStringList pre,
                            QStringList post, QStringList expected) {
            QTest::newRow(name) << live << out << pre << post << expected;
        };
        row("left-first", {"A", "D"}, {"C", "B"}, {"A", "A"}, {"D", "C"}, {"A", "B", "C", "D"});
        row("right-first", {"A", "D"}, {"C", "B"}, {"B", "A"}, {"D", "D"}, {"A", "B", "C", "D"});
        row("three", {"A", "E"}, {"D", "C", "B"}, {"A", "B", "A"}, {"E", "D", "D"},
            {"A", "B", "C", "D", "E"});
        row("insertion", {"A", "X", "D"}, {"C", "B"}, {"A", "A"}, {"D", "C"},
            {"A", "X", "B", "C", "D"});
        row("live-reorder", {"E", "A", "D"}, {"C", "B"}, {"A", "A"}, {"D", "C"},
            {"E", "A", "B", "C", "D"});
        row("predecessor", {"A", "X"}, {"B"}, {"A"}, {"C"}, {"A", "B", "X"});
        row("successor", {"X", "C"}, {"B"}, {"A"}, {"C"}, {"X", "B", "C"});
        row("self-anchors", {"A", "D"}, {"B"}, {"B"}, {"B"}, {"A", "D", "B"});
        row("no-anchors", {"X", "Y"}, {"B"}, {"A"}, {"C"}, {"X", "Y", "B"});
        row("opaque-hidden", {"A", "D"}, {"C", "B"}, {"A", "A"}, {"D", "C"}, {"A", "B", "C", "D"});
        row("independent", {"X"}, {"C", "B"}, {"A", "A"}, {"D", "D"}, {"X", "B", "C"});
        row("reversed", {"C", "A"}, {"B"}, {"A"}, {"C"}, {"C", "A", "B"});
        row("cycle", {"A", "D"}, {"C", "B"}, {"D", "A"}, {"B", "C"}, {"A", "D", "B", "C"});
        row("linked-gap", {"A", "X", "D"}, {"C", "B"}, {"X", "A"}, {"D", "C"},
            {"A", "X", "B", "C", "D"});
        row("forced-separation", {"A", "X", "D"}, {"C", "B"}, {"X", "A"}, {"D", "X"},
            {"A", "B", "X", "C", "D"});
    }
    void returns()
    {
        QFETCH(QStringList, survivors);
        QFETCH(QStringList, shell);
        QFETCH(QStringList, before);
        QFETCH(QStringList, after);
        QFETCH(QStringList, expected);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        Session session;
        settings.setRemoteBackend(&session);
        Session station;
        settings.setRemoteBackend(&station);
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        WorkspaceDocument d;
        d.mainContainerId = "D";
        d.containers = {container("D", survivors), container("S", shell)};
        d.containers[1].popOutShell = true;
        for (int i = 0; i < shell.size(); ++i) {
            auto& e = d.containers[1].contents[i];
            e.returnLocation =
                ReturnLocation{"D", before[i], after[i], {{"opaque", QJsonObject{{"v", 17}}}}};
            e.visible = i != 0;
            e.extensions = {{"future", true}};
        }
        QCOMPARE(store.commit(d, 0).status, CommitStatus::Saved);
        const auto prior = store.snapshot();
        QSignalSpy commits(&store, &ContainerWorkspaceStore::committed);
        ContainerArrangeController controller(store);
        QVERIFY2(controller.closeContainer("S").ok, "shell return failed");
        QCOMPARE(order(store.snapshot(), "D"), expected);
        QCOMPARE(commits.count(), 1);
        QCOMPARE(store.snapshot().containers.size(), 1);
        QCOMPARE(session.requests, 0);
        QCOMPARE(station.requests, 0);
        const auto saved = store.snapshot();
        for (const auto& e : prior.containers[1].contents) {
            const auto& entries = saved.containers[0].contents;
            const auto it = std::find_if(entries.cbegin(), entries.cend(),
                                         [&](const auto& v) { return v.id == e.id; });
            QVERIFY(it != entries.cend());
            QCOMPARE(*it, e);
        }
    }
    void movesMimeDuplicateAndLocks()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        Session station;
        settings.setRemoteBackend(&station);
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        ContainerContentRegistry registry;
        WorkspaceDocument d;
        d.mainContainerId = "M";
        d.containers = {container("M", {"A", "B", "C"}), container("D", {})};
        d.containers[0].contents[1] = registry.makeEntry("meter.clock");
        const QString meter = d.containers[0].contents[1].id;
        QCOMPARE(store.commit(d, 0).status, CommitStatus::Saved);
        ContainerArrangeController controller(store);
        QSignalSpy commits(&store, &ContainerWorkspaceStore::committed);
        const QByteArray mime = controller.mimeData("A");
        const auto unchanged = ContainerDocumentCodec::encode(store.snapshot());
        QVERIFY(!controller.move("A","D",99).ok);
        QVERIFY(!controller.drop(mime,"missing",0).ok);
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()),unchanged);
        QCOMPARE(commits.count(),0);
        ContainerArrangeController foreign(store);
        QVERIFY(!foreign.drop(mime, "D", 0).ok);
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), unchanged);
        QVERIFY(controller.move("A", "M", 3).ok);
        QCOMPARE(order(store.snapshot(), "M"), QStringList({meter, "C", "A"}));
        QCOMPARE(store.snapshot().containers[0].contents.last().paintOrder, 73);
        QVERIFY(!controller.drop(mime, "D", 0).ok);
        QVERIFY(controller.move("A", "D", 0).ok);
        QCOMPARE(commits.count(), 2);
        QVERIFY(controller.duplicateEntry(meter, "D", 1).ok);
        const auto duplicate = store.snapshot().containers[1].contents.last();
        QVERIFY(duplicate.id != meter);
        QCOMPARE(duplicate.config, store.snapshot().containers[0].contents[0].config);
        d = store.snapshot();
        d.containers[0].contents.append(registry.makeEntry("applet:s_meter"));
        d.containers[1].locked = true;
        d.containers[1].popOutShell = true;
        QCOMPARE(store.commit(d, d.revision).status, CommitStatus::Saved);
        const auto locked = ContainerDocumentCodec::encode(store.snapshot());
        QVERIFY(!controller.move(meter, "D", 0).ok);
        QVERIFY(!controller.duplicateEntry(d.containers[0].contents.last().id, "M", 0).ok);
        QVERIFY(!controller.closeContainer("D").ok);
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), locked);
        QCOMPARE(station.requests, 0);
    }
    void inheritedSourcePopPreservesShellDefaults()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        WorkspaceDocument d;
        d.mainContainerId = "M";
        d.containers = {container("M", {"B"}), container("D", {})};
        d.containers[0].config = {{"sliceId", 1}, {"sessionId", "local-session"}};
        d.containers[1].config = {{"sliceId", 0}};
        QCOMPARE(store.commit(d, 0).status, CommitStatus::Saved);
        ContainerArrangeController controller(store);
        QVERIFY(controller.popOut("B").ok);
        auto shell = store.snapshot().containers.last();
        QCOMPARE(ContainerContentHost::effectiveContext(shell, shell.contents[0]),
                 d.containers[0].config);
        QVERIFY(shell.contents[0].context.isEmpty());
        QVERIFY(controller.popOut("B").ok);
        shell = store.snapshot().containers.last();
        QCOMPARE(ContainerContentHost::effectiveContext(shell, shell.contents[0]),
                 d.containers[0].config);
        QCOMPARE(shell.contents[0].returnLocation->containerId, QString("M"));
        QVERIFY(controller.move("B", "D", 0).ok);
        auto destination = store.snapshot().containers[1];
        QCOMPARE(ContainerContentHost::effectiveContext(destination, destination.contents[0]),
                 destination.config);
        QVERIFY(controller.returnEntry("B").ok);
        QCOMPARE(order(store.snapshot(), "M"), QStringList({"B"}));
    }
    void primitiveDuplicate()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        Session station;
        settings.setRemoteBackend(&station);
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        ContainerContentRegistry registry;
        WorkspaceDocument d;
        d.mainContainerId = "M";
        d.containers = {container("M", {})};
        auto primitive = registry.makeEntry("BAR");
        primitive.extensions = {{"opaque", QJsonObject{{"v", 3}}}};
        d.containers[0].contents = {primitive};
        QCOMPARE(store.commit(d, 0).status, CommitStatus::Saved);
        ContainerArrangeController controller(store);
        QVERIFY(controller.duplicateEntry(primitive.id, "M", 1).ok);
        auto duplicate = store.snapshot().containers[0].contents[1];
        QVERIFY(duplicate.id != primitive.id);
        duplicate.id = primitive.id;
        QCOMPARE(duplicate, primitive);
        QCOMPARE(station.requests, 0);
    }
    void destinationsShellHopsAndNormalClose()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        Session station;
        settings.setRemoteBackend(&station);
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        WorkspaceDocument d;
        d.mainContainerId = "M";
        d.containers = {container("M", {"P"}), container("D", {"A", "B", "C"}),
                        container("E", {"U", "V", "W"})};
        QCOMPARE(store.commit(d, 0).status, CommitStatus::Saved);
        ContainerArrangeController controller(store);
        QVERIFY(controller.popOut("B").ok);
        QVERIFY(controller.returnEntry("B").ok);
        QVERIFY(controller.popOut("B").ok);
        QString shell = store.snapshot().containers.last().id;
        QVERIFY(controller.move("V", shell, 0).ok);
        d = store.snapshot();
        d.containers.last().contents.prepend(entry("N"));
        d.containers.last().contents.prepend(entry("Z"));
        QCOMPARE(store.commit(d, d.revision).status, CommitStatus::Saved);
        QVERIFY(controller.popOut("B").ok);
        const auto hopped = store.snapshot().containers.last();
        QCOMPARE(hopped.contents[0].returnLocation->containerId, QString("D"));
        QVERIFY(controller.move("B", shell, 1).ok);
        QVERIFY(controller.closeContainer(shell).ok);
        QCOMPARE(order(store.snapshot(), "D"), QStringList({"A", "B", "C"}));
        QCOMPARE(order(store.snapshot(), "E"), QStringList({"U", "V", "W"}));
        QCOMPARE(order(store.snapshot(), "M"), QStringList({"P", "Z", "N"}));
        QVERIFY(controller.popOut("B").ok);
        shell = store.snapshot().containers.last().id;
        QVERIFY(controller.move("C", shell, 0).ok);
        QVERIFY(controller.removeContainer("D").ok);
        QVERIFY(controller.closeContainer(shell).ok);
        QCOMPARE(order(store.snapshot(), "M"), QStringList({"P", "Z", "N", "A", "B", "C"}));
        const quint64 returnedRevision = store.snapshot().revision;
        QVERIFY(controller.returnEntry("B").ok);
        QCOMPARE(store.snapshot().revision, returnedRevision);
        d = store.snapshot();
        auto normal = container("normal", {"Q"});
        normal.dockMode = DockMode::Floating;
        normal.locked = true;
        d.containers.append(normal);
        QCOMPARE(store.commit(d, d.revision).status, CommitStatus::Saved);
        QVERIFY(controller.closeContainer("normal").ok);
        QCOMPARE(order(store.snapshot(), "normal"), QStringList({"Q"}));
        QVERIFY(!store.snapshot().containers.last().visible);
        QCOMPARE(station.requests, 0);
    }
    void failedSaveLeavesParentsAndBytes_data()
    {
        QTest::addColumn<int>("failureMode");QTest::newRow("save-failure")<<0;QTest::newRow("external-CAS-conflict")<<1;QTest::newRow("external-invalid-workspace")<<2;
    }
    void failedSaveLeavesParentsAndBytes()
    {
        QFETCH(int,failureMode);
        QTemporaryDir dir;
        const QString parent = dir.filePath("live");
        QVERIFY(QDir().mkpath(parent));
        AppSettings settings(parent + "/settings.xml");
        Session station;
        settings.setRemoteBackend(&station);
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        QWidget dock;
        QSplitter splitter;
        ContainerContentRegistry registry;
        QWidget borrowed;
        registry.attachSingleton("applet:s_meter", &borrowed);
        ContainerManager manager(&dock, &splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        WorkspaceDocument d;
        d.mainContainerId = "M";
        d.containers = {container("M", {}), container("S", {})};
        d.containers[1].contents = {registry.makeEntry("applet:s_meter")};
        d.containers[1].popOutShell = true;
        d.containers[1].contents[0].returnLocation = ReturnLocation{"M", {}, {}, {}};
        QCOMPARE(manager.commitWorkspace(d, 0).status, CommitStatus::Saved);
        const auto before = store.snapshot();
        QWidget* owner = borrowed.parentWidget();
        if(failureMode==0) {
            QVERIFY(QDir().rename(parent, dir.filePath("moved")));
            QFile blocker(parent);QVERIFY(blocker.open(QIODevice::WriteOnly));blocker.close();
        } else if(failureMode==1) {
            auto external=before;external.containers[0].name="another client";++external.revision;
            settings.setValue("ContainerWorkspace",QString::fromUtf8(ContainerDocumentCodec::encode(external)));
        } else {settings.setValue("ContainerWorkspace",QStringLiteral("{\"schemaVersion\":99}"));}
        const auto bytes=settings.value("ContainerWorkspace");
        QSignalSpy commits(&store, &ContainerWorkspaceStore::committed);
        ContainerArrangeController controller(store, &manager);
        QVERIFY(!controller.closeContainer("S").ok);
        QCOMPARE(store.snapshot(), before);
        QCOMPARE(borrowed.parentWidget(), owner);
        QCOMPARE(settings.value("ContainerWorkspace"), bytes);
        QCOMPARE(commits.count(), 0);
        QVERIFY(manager.container("S"));
        QCOMPARE(station.requests, 0);
    }
};
QTEST_MAIN(TstContainerArrangement)
#include "tst_container_arrangement.moc"
