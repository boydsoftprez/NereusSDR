// no-port-check: NereusSDR-original transactional client presentation store.
#include "ContainerWorkspaceStore.h"
#include "ContainerDocumentCodec.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include <QScopedValueRollback>
#include <limits>

namespace NereusSDR {
namespace {
const QString kWorkspaceKey = QStringLiteral("ContainerWorkspace");
const QString kBackupKey = QStringLiteral("ContainerWorkspaceBackup");
}

ContainerWorkspaceStore::ContainerWorkspaceStore(AppSettings& settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
    load();
}

DocumentResult ContainerWorkspaceStore::load()
{
    if (m_committing) {
        return {false, {}, QStringLiteral("A workspace commit is in progress")};
    }
    const bool present = m_settings.contains(kWorkspaceKey);
    const QByteArray raw = m_settings.value(kWorkspaceKey).toString().toUtf8();
    const DocumentResult result = present ? ContainerDocumentCodec::decode(raw)
                                          : DocumentResult{true, {}, {}};
    if (!result.ok) {
        // Keep both the previous live document and the exact source bytes. No
        // fallback may turn a corrupt/future workspace into an editable empty one.
        m_loadError = result.error;
        qCWarning(lcContainer) << "Container workspace remains read-only:" << result.error;
        return result;
    }
    m_document = result.document;
    m_loadedRaw = raw;
    m_loadedPresent = present;
    m_loadError.clear();
    return result;
}

WorkspaceDocument ContainerWorkspaceStore::snapshot() const
{
    return m_document;
}

CommitResult ContainerWorkspaceStore::commit(const WorkspaceDocument& document, quint64 expectedRevision)
{
    if (m_committing) {
        return {CommitStatus::Conflict, m_document.revision, QStringLiteral("A workspace commit is in progress")};
    }
    if (!m_loadError.isEmpty()) {
        return {CommitStatus::Invalid, m_document.revision, m_loadError};
    }
    if (expectedRevision != m_document.revision) {
        return {CommitStatus::Conflict, m_document.revision, QStringLiteral("Workspace revision changed")};
    }
    const bool present = m_settings.contains(kWorkspaceKey);
    const QByteArray raw = m_settings.value(kWorkspaceKey).toString().toUtf8();
    if (present != m_loadedPresent || raw != m_loadedRaw) {
        // Multiple stores, or an external client-local settings write, cannot
        // overwrite one another even if they reused the same revision number.
        const DocumentResult current = present ? ContainerDocumentCodec::decode(raw)
                                               : DocumentResult{true, {}, {}};
        return {current.ok ? CommitStatus::Conflict : CommitStatus::Invalid,
                current.ok ? current.document.revision : m_document.revision,
                current.ok ? QStringLiteral("Workspace changed; reload before editing") : current.error};
    }
    const QString validation = ContainerDocumentCodec::validate(document);
    if (!validation.isEmpty()) {
        return {CommitStatus::Invalid, m_document.revision, validation};
    }
    if (m_document.revision == std::numeric_limits<quint64>::max()) {
        return {CommitStatus::Invalid, m_document.revision, QStringLiteral("Workspace revision is exhausted")};
    }
    WorkspaceDocument saved = document;
    saved.revision = m_document.revision + 1;
    const QByteArray encoded = ContainerDocumentCodec::encode(saved);
    const QVariant oldWorkspace = m_settings.value(kWorkspaceKey);
    const bool backupPresent = m_settings.contains(kBackupKey);
    const QVariant oldBackup = m_settings.value(kBackupKey);
    QScopedValueRollback<bool> guard(m_committing, true);

    // First replacement retains the original structured payload exactly. A
    // legacy importer can seed this key with its raw backup before committing.
    if (present && !backupPresent) {
        m_settings.setValue(kBackupKey, QString::fromUtf8(raw));
    }
    m_settings.setValue(kWorkspaceKey, QString::fromUtf8(encoded));
    QString error;
    if (!m_settings.save(&error)) {
        // Restore only owned keys: changes to other preferences during this
        // transaction belong to their writers and must survive a failed save.
        if (present) {
            m_settings.setValue(kWorkspaceKey, oldWorkspace);
        } else {
            m_settings.remove(kWorkspaceKey);
        }
        if (backupPresent) {
            m_settings.setValue(kBackupKey, oldBackup);
        } else {
            m_settings.remove(kBackupKey);
        }
        qCWarning(lcContainer) << "Container workspace save failed:" << error;
        return {CommitStatus::StorageError, m_document.revision, error};
    }
    m_document = saved;
    m_loadedRaw = encoded;
    m_loadedPresent = true;
    m_committing = false;
    guard.commit();
    emit committed(m_document.revision);
    return {CommitStatus::Saved, saved.revision, {}};
}
} // namespace NereusSDR
