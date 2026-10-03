// no-port-check: NereusSDR-original stable-ID workspace transaction regressions.
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QFile>
#include "core/AppSettings.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerDocumentCodec.h"
using namespace NereusSDR;
class TstContainerEditSession : public QObject {
    Q_OBJECT
    static WorkspaceDocument sample() {
        WorkspaceDocument d; d.mainContainerId="A";
        ContainerDocument a,b,c; a.id="A"; b.id="B"; c.id="C"; a.name="A"; b.name="B"; c.name="C";
        ContentEntry x,y,z; x.id="X"; y.id="Y"; z.id="Z"; x.typeId=y.typeId=z.typeId="meter.customBar";
        a.contents={x,y}; b.contents={z}; d.containers={a,b,c}; return d;
    }
    static void seed(AppSettings& settings) { settings.setValue("ContainerWorkspace",QString::fromUtf8(ContainerDocumentCodec::encode(sample()))); }
private slots:
    void atomicDraftsCancelAndCreateDelete() {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings")); seed(settings);
        ContainerWorkspaceStore store(settings); ContainerEditSession session(store); QSignalSpy saved(&store,&ContainerWorkspaceStore::committed);
        auto d=session.draft(); d.containers[0].name="renamed"; std::reverse(d.containers[0].contents.begin(),d.containers[0].contents.end());
        d.containers[1].config["opaque"]=17; ContainerDocument extra; extra.id="D"; d.containers.append(extra); d.containers.removeAt(2);
        const auto before=settings.value("ContainerWorkspace"); session.setDraft(d); QCOMPARE(store.snapshot(),sample()); QCOMPARE(settings.value("ContainerWorkspace"),before);
        session.cancel(); QCOMPARE(session.draft(),sample()); session.setDraft(d);
        QCOMPARE(session.apply().status,CommitStatus::Saved); QCOMPARE(saved.count(),1); d.revision=1; QCOMPARE(store.snapshot(),d); QCOMPARE(session.draft(),d);
    }
    void separateStoreRebaseAndSameRevisionRawReplacement() {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings")); seed(settings);
        ContainerWorkspaceStore store(settings),other(settings); ContainerEditSession session(store);
        auto draft=session.draft(); draft.containers[0].name="mine"; session.setDraft(draft);
        auto live=other.snapshot(); live.containers[2].name="external"; live.extensions["new"]=42;
        QCOMPARE(other.commit(live,0).status,CommitStatus::Saved); QVERIFY(session.conflictingContainers().isEmpty());
        QCOMPARE(session.apply().status,CommitStatus::Saved); QCOMPARE(store.snapshot().containers[2].name,QString("external")); QCOMPARE(store.snapshot().extensions.value("new").toInt(),42);
        ContainerEditSession next(store); draft=next.draft(); draft.containers[0].name="second"; next.setDraft(draft);
        live=store.snapshot(); live.containers[0].name="same-revision external";
        settings.setValue("ContainerWorkspace",QString::fromUtf8(ContainerDocumentCodec::encode(live)));
        QCOMPARE(next.conflictingContainers(),QStringList{"A"}); const auto raw=settings.value("ContainerWorkspace");
        QCOMPARE(next.apply().status,CommitStatus::Conflict); QCOMPARE(settings.value("ContainerWorkspace"),raw); QCOMPARE(next.draft(),draft);
    }
    void moveConflictPartialReloadRetainsUnrelatedFields() {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings")); seed(settings);
        ContainerWorkspaceStore store(settings),other(settings); ContainerEditSession session(store);
        auto draft=session.draft(); auto y=draft.containers[0].contents.takeLast(); draft.containers[1].contents.append(y);
        draft.containers[1].name="keep rename"; draft.containers[1].contents[0].config["keep"]=true; session.setDraft(draft);
        auto live=other.snapshot(); live.containers[0].name="external A"; QCOMPARE(other.commit(live,0).status,CommitStatus::Saved);
        QVERIFY(session.conflictingContainers().contains("A")); QCOMPARE(session.apply().status,CommitStatus::Conflict);
        session.reloadContainers({"A"}); const auto reloaded=session.draft(); QCOMPARE(reloaded.containers[0].contents.size(),2); QCOMPARE(reloaded.containers[1].contents.size(),1);
        QCOMPARE(reloaded.containers[1].name,QString("keep rename")); QVERIFY(reloaded.containers[1].contents[0].config.value("keep").toBool());
        QCOMPARE(session.apply().status,CommitStatus::Saved);
    }
    void competingMoveReloadDoesNotStealIdentity() {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings")); seed(settings);
        ContainerWorkspaceStore store(settings),other(settings); ContainerEditSession session(store);
        auto draft=session.draft(); draft.containers[1].contents.append(draft.containers[0].contents.takeLast()); draft.containers[1].name="keep B"; session.setDraft(draft);
        auto live=other.snapshot(); live.containers[2].contents.append(live.containers[0].contents.takeLast()); QCOMPARE(other.commit(live,0).status,CommitStatus::Saved);
        const auto names=session.conflictingContainers(); QVERIFY(names.contains("A")); QVERIFY(names.contains("B")); QVERIFY(names.contains("C"));
        session.reloadContainers({"A"}); QVERIFY(ContainerDocumentCodec::validate(session.draft()).isEmpty()); QCOMPARE(session.draft().containers[1].name,QString("keep B"));
        QCOMPARE(session.apply().status,CommitStatus::Saved); QCOMPARE(store.snapshot().containers[2].contents[0].id,QString("Y")); QCOMPARE(store.snapshot().containers[1].contents.size(),1);
    }
    void externalDeletionConfigurationAndReferencedDestination() {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings")); seed(settings);
        ContainerWorkspaceStore store(settings),other(settings); ContainerEditSession session(store);
        auto d=session.draft(); d.containers[0].contents[1].name="edit Y"; d.containers[0].contents[1].returnLocation=ReturnLocation{"C",{},{},{}}; session.setDraft(d);
        auto live=other.snapshot(); live.containers[0].contents.removeLast(); live.containers.removeLast(); QCOMPARE(other.commit(live,0).status,CommitStatus::Saved);
        QVERIFY(session.conflictingContainers().contains("A")); QVERIFY(session.conflictingContainers().contains("C")); QCOMPARE(session.apply().status,CommitStatus::Conflict);
        session.reloadContainers({"A","C"}); QCOMPARE(session.apply().status,CommitStatus::Saved);
        ContainerEditSession next(store); d=next.draft(); d.containers[0].name="draft"; next.setDraft(d); live=store.snapshot(); live.containers[0].contents[0].config["change"]=99;
        settings.setValue("ContainerWorkspace",QString::fromUtf8(ContainerDocumentCodec::encode(live))); QVERIFY(next.conflictingContainers().contains("A")); QCOMPARE(next.apply().status,CommitStatus::Conflict);
    }
    void reloadMainResolvesWorkspaceMetadataRetainingOtherDrafts() {
        QTemporaryDir dir; AppSettings settings(dir.filePath("settings")); auto base=sample(); base.extensions["x"]=0;
        settings.setValue("ContainerWorkspace",QString::fromUtf8(ContainerDocumentCodec::encode(base)));
        ContainerWorkspaceStore store(settings),other(settings); ContainerEditSession session(store);
        auto draft=session.draft(); draft.extensions["x"]=1; draft.containers[1].name="keep B"; session.setDraft(draft);
        auto live=other.snapshot(); live.extensions["x"]=2; QCOMPARE(other.commit(live,0).status,CommitStatus::Saved);
        QCOMPARE(session.apply().status,CommitStatus::Conflict); QVERIFY(session.conflictingContainers().contains("A"));
        session.reloadContainers({"A"}); QCOMPARE(session.draft().extensions.value("x").toInt(),2); QCOMPARE(session.draft().containers[1].name,QString("keep B"));
        QCOMPARE(session.apply().status,CommitStatus::Saved); QCOMPARE(store.snapshot().extensions.value("x").toInt(),2); QCOMPARE(store.snapshot().containers[1].name,QString("keep B"));
    }
    void saveFailureAndFutureBytesRetainDraft() {
        QTemporaryDir dir; QFile blocker(dir.filePath("blocker")); QVERIFY(blocker.open(QIODevice::WriteOnly)); blocker.close();
        AppSettings settings(dir.filePath("blocker/settings")); seed(settings); ContainerWorkspaceStore store(settings); ContainerEditSession session(store);
        auto d=session.draft(); d.containers[0].name="draft"; session.setDraft(d); const auto before=settings.value("ContainerWorkspace"); QSignalSpy spy(&store,&ContainerWorkspaceStore::committed);
        QCOMPARE(session.apply().status,CommitStatus::StorageError); QCOMPARE(session.draft(),d); QCOMPARE(store.snapshot(),sample()); QCOMPARE(settings.value("ContainerWorkspace"),before); QCOMPARE(spy.count(),0);
        settings.setValue("ContainerWorkspace","{\"schemaVersion\":99}"); QCOMPARE(session.apply().status,CommitStatus::Invalid); QCOMPARE(session.draft(),d); QCOMPARE(settings.value("ContainerWorkspace").toString(),QString("{\"schemaVersion\":99}"));
    }
};
QTEST_MAIN(TstContainerEditSession)
#include "tst_container_edit_session.moc"
