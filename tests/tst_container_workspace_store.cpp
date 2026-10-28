// no-port-check: NereusSDR-original transactional presentation persistence tests.
// Modification history (NereusSDR):
//   2026-10-26 - The two states that refuse every commit until a reload (a
//                 load error, or the saved bytes changed under the store)
//                 are reported through blockedReason() and cleared by a
//                 good load; every refused commit is logged and a saved one
//                 is not. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include <QRegularExpression>
#include <limits>
#include "core/AppSettings.h"
#include "core/settings/ISettingsBackend.h"
#include "core/settings/SettingsScope.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContainerDocumentCodec.h"

using namespace NereusSDR;

class CountingStationBackend : public ISettingsBackend {
public:
    int writes = 0;
    bool handlesKey(const QString& key) const override
    { return classifySettingsKey(key) == SettingsScope::Station; }
    QVariant value(const QString&, const QVariant& fallback) const override { return fallback; }
    void setValue(const QString&, const QVariant&) override { ++writes; }
    bool contains(const QString&) const override { return false; }
    void remove(const QString&) override { ++writes; }
    QStringList handledKeys() const override { return {}; }
};

class TstContainerWorkspaceStore : public QObject {
    Q_OBJECT
private:
    static WorkspaceDocument sample()
    {
        WorkspaceDocument doc;
        doc.mainContainerId = "main";
        ContainerDocument container;
        container.id = "main";
        container.name = "Main";
        ContentEntry content;
        content.id = "one";
        content.typeId = "meter.custom";
        content.name = "A";
        container.contents.append(content);
        doc.containers.append(container);
        return doc;
    }
private slots:
    void commitsPublishOnlyAfterSavedAndRemainLocal()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("settings.xml");
        AppSettings settings(path);
        settings.setValue("ContainerData_old", "original|bytes");
        CountingStationBackend backend;
        settings.setRemoteBackend(&backend);
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        QSignalSpy spy(&store, &ContainerWorkspaceStore::committed);
        bool savedWhenSignalled = false;
        connect(&store, &ContainerWorkspaceStore::committed, &store, [&](quint64 revision) {
            AppSettings disk(path);
            disk.load();
            const DocumentResult result = ContainerDocumentCodec::decode(disk.value("ContainerWorkspace").toString().toUtf8());
            savedWhenSignalled = result.ok && result.document.revision == revision
                && store.snapshot() == result.document;
        });
        const CommitResult result = store.commit(sample(), 0);
        QCOMPARE(result.status, CommitStatus::Saved);
        QCOMPARE(result.revision, quint64(1));
        QCOMPARE(store.snapshot().revision, quint64(1));
        QCOMPARE(spy.count(), 1);
        QVERIFY(savedWhenSignalled);
        QCOMPARE(backend.writes, 0);
        QCOMPARE(settings.value("ContainerData_old").toString(), QString("original|bytes"));
        QCOMPARE(classifySettingsKey(QStringLiteral("ContainerWorkspace")), SettingsScope::OperatorLocal);
        QCOMPARE(classifySettingsKey(QStringLiteral("ContainerWorkspaceBackup")), SettingsScope::OperatorLocal);
        const WorkspaceDocument saved = store.snapshot();
        QCOMPARE(store.commit(sample(), 0).status, CommitStatus::Conflict);
        QCOMPARE(store.snapshot(), saved);
        QCOMPARE(spy.count(), 1);
        WorkspaceDocument invalid = saved;
        invalid.containers[0].contents.append(invalid.containers[0].contents.first());
        QCOMPARE(store.commit(invalid, 1).status, CommitStatus::Invalid);
        QCOMPARE(store.snapshot(), saved);
        QCOMPARE(spy.count(), 1);
    }
    void backupKeepsExactOriginalAndRevisionAdvancesOnce()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        const QByteArray original = ContainerDocumentCodec::encode(sample());
        settings.setValue("ContainerWorkspace", QString::fromUtf8(original));
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        WorkspaceDocument changed = store.snapshot();
        QVERIFY(!changed.containers.isEmpty());
        changed.containers[0].name = "Changed";
        changed.revision = 500; // The store owns revision allocation.
        QCOMPARE(store.commit(changed, 0).status, CommitStatus::Saved);
        QCOMPARE(store.snapshot().revision, quint64(1));
        QCOMPARE(settings.value("ContainerWorkspaceBackup").toString().toUtf8(), original);
        changed.containers[0].name = "Again";
        QCOMPARE(store.commit(changed, 1).status, CommitStatus::Saved);
        QCOMPARE(store.snapshot().revision, quint64(2));
        QCOMPARE(settings.value("ContainerWorkspaceBackup").toString().toUtf8(), original);
    }
    void failedSaveRestoresOnlyOwnedKeys_data()
    {
        QTest::addColumn<bool>("existing");
        QTest::addColumn<bool>("backupExisting");
        QTest::newRow("absent") << false << false;
        QTest::newRow("first-replacement") << true << false;
        QTest::newRow("existing-backup") << true << true;
    }
    void failedSaveRestoresOnlyOwnedKeys()
    {
        QFETCH(bool, existing);
        QFETCH(bool, backupExisting);
        QTemporaryDir dir;
        QFile blocker(dir.filePath("blocker"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        // A regular file cannot be a parent directory, even when run as root.
        AppSettings settings(dir.filePath("blocker/settings.xml"));
        if (existing) {
            settings.setValue("ContainerWorkspace", QString::fromUtf8(ContainerDocumentCodec::encode(sample())));
        }
        if (backupExisting) {
            settings.setValue("ContainerWorkspaceBackup", "exact original backup");
        }
        settings.setValue("ContainerData_old", "legacy untouched");
        const QVariant oldDocument = settings.value("ContainerWorkspace");
        const QVariant oldBackup = settings.value("ContainerWorkspaceBackup");
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        const WorkspaceDocument before = store.snapshot();
        QSignalSpy spy(&store, &ContainerWorkspaceStore::committed);
        settings.setChangeHook([&](const QString& key) {
            if (key == QStringLiteral("ContainerWorkspace")) {
                settings.setValue("UnrelatedPreference", "edited concurrently");
            }
        });
        const CommitResult result = store.commit(sample(), before.revision);
        QCOMPARE(result.status, CommitStatus::StorageError);
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(store.snapshot(), before);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(settings.contains("ContainerWorkspace"), existing);
        QCOMPARE(settings.contains("ContainerWorkspaceBackup"), backupExisting);
        QCOMPARE(settings.value("ContainerWorkspace"), oldDocument);
        QCOMPARE(settings.value("ContainerWorkspaceBackup"), oldBackup);
        QCOMPARE(settings.value("ContainerData_old").toString(), QString("legacy untouched"));
        QCOMPARE(settings.value("UnrelatedPreference").toString(), QString("edited concurrently"));
    }
    void corruptAndFutureDocumentsRemainRecoverable_data()
    {
        QTest::addColumn<QByteArray>("raw");
        QTest::newRow("corrupt") << QByteArray("{bad JSON | exact bytes\n");
        QTest::newRow("future") << QByteArray("{ \"schemaVersion\": 99, \"future\": [\"opaque\"] }");
    }
    void corruptAndFutureDocumentsRemainRecoverable()
    {
        QFETCH(QByteArray, raw);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        settings.setValue("ContainerWorkspace", QString::fromUtf8(raw));
        settings.setValue("ContainerWorkspaceBackup", "older original");
        ContainerWorkspaceStore store(settings);
        const WorkspaceDocument before = store.snapshot();
        QVERIFY(!store.load().ok);
        QSignalSpy spy(&store, &ContainerWorkspaceStore::committed);
        QCOMPARE(store.commit(sample(), 0).status, CommitStatus::Invalid);
        QCOMPARE(settings.value("ContainerWorkspace").toString().toUtf8(), raw);
        QCOMPARE(settings.value("ContainerWorkspaceBackup").toString(), QString("older original"));
        QCOMPARE(store.snapshot(), before);
        QCOMPARE(spy.count(), 0);
    }
    void failedReloadPreservesLastLiveSnapshot()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings);
        QCOMPARE(store.commit(sample(), 0).status, CommitStatus::Saved);
        const WorkspaceDocument before = store.snapshot();
        settings.setValue("ContainerWorkspace", "{truncated");
        QVERIFY(!store.load().ok);
        QCOMPARE(store.snapshot(), before);
        QCOMPARE(store.commit(sample(), 1).status, CommitStatus::Invalid);
        QCOMPARE(settings.value("ContainerWorkspace").toString(), QString("{truncated"));
        settings.setValue("ContainerWorkspace", QString::fromUtf8(ContainerDocumentCodec::encode(before)));
        QVERIFY(store.load().ok);
        QCOMPARE(store.commit(sample(), 1).status, CommitStatus::Saved);
        QCOMPARE(store.snapshot().revision, quint64(2));
    }
    void revisionCannotWrapAround()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        WorkspaceDocument full = sample();
        full.revision = std::numeric_limits<quint64>::max();
        const QString original = QString::fromUtf8(ContainerDocumentCodec::encode(full));
        settings.setValue("ContainerWorkspace", original);
        ContainerWorkspaceStore store(settings);
        QSignalSpy spy(&store, &ContainerWorkspaceStore::committed);
        QCOMPARE(store.commit(sample(), full.revision).status, CommitStatus::Invalid);
        QCOMPARE(store.snapshot(), full);
        QCOMPARE(settings.value("ContainerWorkspace").toString(), original);
        QCOMPARE(spy.count(), 0);
    }
    void anotherStoreOrExternalWriterCannotBeOverwritten()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore first(settings), second(settings);
        QVERIFY(first.load().ok);
        QVERIFY(second.load().ok);
        QCOMPARE(first.commit(sample(), 0).status, CommitStatus::Saved);
        QCOMPARE(second.commit(sample(), 0).status, CommitStatus::Conflict);
        QVERIFY(second.load().ok);
        WorkspaceDocument external = second.snapshot();
        QVERIFY(!external.containers.isEmpty());
        external.containers[0].name = "external change at the same revision";
        settings.setValue("ContainerWorkspace", QString::fromUtf8(ContainerDocumentCodec::encode(external)));
        QCOMPARE(second.commit(sample(), 1).status, CommitStatus::Conflict);
        QCOMPARE(ContainerDocumentCodec::decode(settings.value("ContainerWorkspace").toString().toUtf8()).document, external);
    }
    void bytesChangedUnderTheStoreRefuseEveryCommitUntilReload()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings);
        QCOMPARE(store.commit(sample(), 0).status, CommitStatus::Saved);
        QVERIFY(store.blockedReason().isEmpty());
        QSignalSpy blocked(&store, &ContainerWorkspaceStore::blockedChanged);
        // Another writer replaces the saved layout behind the store.
        WorkspaceDocument external = store.snapshot();
        external.containers[0].name = "written behind the store";
        settings.setValue("ContainerWorkspace", QString::fromUtf8(ContainerDocumentCodec::encode(external)));
        const QString reason = QStringLiteral("Workspace changed; reload before editing");
        for (int attempt = 0; attempt < 2; ++attempt) {
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(
                QStringLiteral("^Container workspace commit refused: Conflict - Workspace changed; reload before editing$")));
            const CommitResult result = store.commit(sample(), store.snapshot().revision);
            QCOMPARE(result.status, CommitStatus::Conflict);
            QCOMPARE(result.error, reason);
            QCOMPARE(store.blockedReason(), reason);
        }
        QCOMPARE(blocked.count(), 1);
        QCOMPARE(blocked.at(0).at(0).toString(), reason);
        // Reading the layout again clears it, and commits save.
        QVERIFY(store.load().ok);
        QVERIFY(store.blockedReason().isEmpty());
        QCOMPARE(blocked.count(), 2);
        QVERIFY(blocked.at(1).at(0).toString().isEmpty());
        QCOMPARE(store.commit(sample(), store.snapshot().revision).status, CommitStatus::Saved);
        QCOMPARE(blocked.count(), 2);
    }
    void aLoadErrorRefusesEveryCommitUntilAGoodReload()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings);
        QCOMPARE(store.commit(sample(), 0).status, CommitStatus::Saved);
        const QString good = settings.value("ContainerWorkspace").toString();
        QSignalSpy blocked(&store, &ContainerWorkspaceStore::blockedChanged);
        settings.setValue("ContainerWorkspace", "{truncated");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^Container workspace remains read-only:")));
        QVERIFY(!store.load().ok);
        QVERIFY(!store.loadError().isEmpty());
        QCOMPARE(store.blockedReason(), store.loadError());
        QCOMPARE(blocked.count(), 1);
        for (int attempt = 0; attempt < 2; ++attempt) {
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(
                QStringLiteral("^Container workspace commit refused: Invalid - %1$")
                    .arg(QRegularExpression::escape(store.loadError()))));
            QCOMPARE(store.commit(sample(), store.snapshot().revision).status, CommitStatus::Invalid);
        }
        QCOMPARE(blocked.count(), 1);
        settings.setValue("ContainerWorkspace", good);
        QVERIFY(store.load().ok);
        QVERIFY(store.blockedReason().isEmpty());
        QCOMPARE(blocked.count(), 2);
        QCOMPARE(store.commit(sample(), store.snapshot().revision).status, CommitStatus::Saved);
    }
    void aOneOffRefusalIsLoggedButDoesNotBlock()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings);
        QSignalSpy blocked(&store, &ContainerWorkspaceStore::blockedChanged);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(
            QStringLiteral("^Container workspace commit refused: Conflict - Workspace revision changed$")));
        QCOMPARE(store.commit(sample(), 7).status, CommitStatus::Conflict);
        QVERIFY(store.blockedReason().isEmpty());
        QCOMPARE(blocked.count(), 0);
        QCOMPARE(store.commit(sample(), 0).status, CommitStatus::Saved);
    }
    void aSavedCommitLogsNothing()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings.xml"));
        ContainerWorkspaceStore store(settings);
        QSignalSpy blocked(&store, &ContainerWorkspaceStore::blockedChanged);
        QCOMPARE(store.commit(sample(), 0).status, CommitStatus::Saved);
        QCOMPARE(store.commit(sample(), 1).status, CommitStatus::Saved);
        QCOMPARE(blocked.count(), 0);
    }
};
QTEST_GUILESS_MAIN(TstContainerWorkspaceStore)
#include "tst_container_workspace_store.moc"
