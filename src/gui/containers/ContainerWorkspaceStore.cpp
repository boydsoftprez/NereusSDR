// no-port-check: NereusSDR-original transactional client presentation store.
// Modification history (NereusSDR):
//   2026-10-27 - Notify consumers after every successful load. J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-26 - Every refused commit is logged with its status and
//                 reason, and the two states that refuse every commit
//                 until a reload (a load error, or the saved bytes changed
//                 underneath) are reported through blockedReason(). J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - commit removes a pop-out shell that an edit emptied, so
//                 no blank shell stays open. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the preview reads and writes its
//                 own keys, seeded from the shipped workspace, so the
//                 shipped ContainerWorkspace key is never written while the
//                 switch is on. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include "ContainerWorkspaceStore.h"
#include "ContainerDocumentCodec.h"
#include "LegacyContainerImporter.h"
#include <QJsonDocument>
#include "core/AppSettings.h"
#include "core/SettingsFileWriter.h"
#include "core/LogCategories.h"
#include "gui/styles/WorkspacePreview.h"
#include <QScopedValueRollback>
#include <QElapsedTimer>
#include <limits>

namespace NereusSDR {
namespace {
const QString kShippedWorkspaceKey = QStringLiteral("ContainerWorkspace");
const QString kShippedBackupKey = QStringLiteral("ContainerWorkspaceBackup");
// Workspace preview: its own document. Until the preview first commits,
// LegacyContainerImporter::fromSettings seeds it from the shipped key.
const QString kPreviewWorkspaceKey = QStringLiteral("ContainerWorkspacePreview");
const QString kPreviewBackupKey = QStringLiteral("ContainerWorkspacePreviewBackup");
}

ContainerWorkspaceStore::ContainerWorkspaceStore(AppSettings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_workspaceKey(WorkspacePreview::enabled() ? kPreviewWorkspaceKey : kShippedWorkspaceKey)
    , m_backupKey(WorkspacePreview::enabled() ? kPreviewBackupKey : kShippedBackupKey)
{
    // Workspace commits write through the settings writer thread; a refused
    // write must still be visible in the log.
    if (auto* writer = m_settings.backgroundSaveNotifier()) {
        connect(writer, &SettingsFileWriter::finished, this,
                [](quint64, bool saved, const QString& error) {
                    if (!saved) { qCWarning(lcContainer) << "Container workspace save failed:" << error; }
                });
    }
    load();
}

DocumentResult ContainerWorkspaceStore::load()
{
    if (m_committing) {
        return {false, {}, QStringLiteral("A workspace commit is in progress")};
    }
    const bool present = m_settings.contains(m_workspaceKey);
    const QByteArray raw = m_settings.value(m_workspaceKey).toString().toUtf8();
    const DocumentResult result = present ? ContainerDocumentCodec::decode(raw)
                                          : LegacyContainerImporter::fromSettings(m_settings);
    if (!result.ok) {
        // Keep both the previous live document and the exact source bytes. No
        // fallback may turn a corrupt/future workspace into an editable empty one.
        m_loadError = result.error;
        qCWarning(lcContainer) << "Container workspace remains read-only:" << result.error;
        setBlockedReason(m_loadError);
        return result;
    }
    m_document = result.document;
    m_loadedRaw = raw;
    m_loadedPresent = present;
    m_loadError.clear();
    setBlockedReason({});
    emit loaded();
    return result;
}

void ContainerWorkspaceStore::setBlockedReason(const QString& reason)
{
    if (reason == m_blockedReason) {
        return;
    }
    m_blockedReason = reason;
    emit blockedChanged(m_blockedReason);
}

CommitResult ContainerWorkspaceStore::refuse(CommitStatus status, quint64 revision, const QString& reason) const
{
    // A refused change is otherwise only on screen; the log keeps why.
    qCWarning(lcContainer).noquote() << "Container workspace commit refused:"
        << (status == CommitStatus::Conflict ? "Conflict" : "Invalid") << "-" << reason;
    return {status, revision, reason};
}

WorkspaceDocument ContainerWorkspaceStore::snapshot() const
{
    return m_document;
}

CommitResult ContainerWorkspaceStore::commit(const WorkspaceDocument& document, quint64 expectedRevision)
{
    QElapsedTimer clock; clock.start();
    if (m_committing) {
        return refuse(CommitStatus::Conflict, m_document.revision, QStringLiteral("A workspace commit is in progress"));
    }
    if (!m_loadError.isEmpty()) {
        return refuse(CommitStatus::Invalid, m_document.revision, m_loadError);
    }
    if (expectedRevision != m_document.revision) {
        return refuse(CommitStatus::Conflict, m_document.revision, QStringLiteral("Workspace revision changed"));
    }
    const bool present = m_settings.contains(m_workspaceKey);
    const QByteArray raw = m_settings.value(m_workspaceKey).toString().toUtf8();
    if (present != m_loadedPresent || raw != m_loadedRaw) {
        // Multiple stores, or an external client-local settings write, cannot
        // overwrite one another even if they reused the same revision number.
        const DocumentResult current = present ? ContainerDocumentCodec::decode(raw)
                                               : DocumentResult{true, {}, {}};
        // Every later commit meets the same bytes, so it stays refused
        // until load() reads them again.
        const QString reason = current.ok ? QStringLiteral("Workspace changed; reload before editing") : current.error;
        setBlockedReason(reason);
        return refuse(current.ok ? CommitStatus::Conflict : CommitStatus::Invalid,
                      current.ok ? current.document.revision : m_document.revision, reason);
    }
    if (!present) {
        const DocumentResult currentLegacy = LegacyContainerImporter::fromSettings(m_settings);
        if (!currentLegacy.ok || currentLegacy.document != m_document) {
            const QString reason = QStringLiteral("Legacy workspace changed; reload before editing");
            setBlockedReason(reason);
            return refuse(CommitStatus::Conflict, m_document.revision, reason);
        }
    }
    const QString validation = ContainerDocumentCodec::validate(document);
    if (!validation.isEmpty()) {
        return refuse(CommitStatus::Invalid, m_document.revision, validation);
    }
    if (m_document.revision == std::numeric_limits<quint64>::max()) {
        return refuse(CommitStatus::Invalid, m_document.revision, QStringLiteral("Workspace revision is exhausted"));
    }
    WorkspaceDocument saved = document;
    saved.revision = m_document.revision + 1;
    // A pop-out shell exists only to hold what was popped out. Whatever
    // empties it (Remove object in Container Settings, Move to Container)
    // removes it, so no blank window stays open (JJ 2026-10-09).
    for (int i = saved.containers.size() - 1; i >= 0; --i) {
        const auto& shell = saved.containers[i];
        if (shell.popOutShell && shell.contents.isEmpty() && shell.id != saved.mainContainerId) {
            saved.containers.removeAt(i);
        }
    }
    const QByteArray encoded = ContainerDocumentCodec::encode(saved);
    const bool backupPresent = m_settings.contains(m_backupKey);
    QScopedValueRollback<bool> guard(m_committing, true);

    // Backups and replacement are written in the same settings document.
    if (!backupPresent) {
        if (present) { m_settings.setValue(m_backupKey, QString::fromUtf8(raw)); }
        else if (m_document.extensions.contains(QStringLiteral("legacySettings"))) {
            m_settings.setValue(m_backupKey, QString::fromUtf8(QJsonDocument(m_document.extensions.value(QStringLiteral("legacySettings")).toObject()).toJson(QJsonDocument::Compact)));
        }
    }
    m_settings.setValue(m_workspaceKey, QString::fromUtf8(encoded));
    // The values are in memory now; the file write (serializing and fsyncing
    // the whole settings document, about 350 ms with a 4 MB workspace) goes
    // to the settings writer thread. A legacy block drag must not stall the
    // GUI and the audio for it. A failed write is reported by the notifier
    // connected in the constructor; the values stay and the next save
    // carries them again.
    m_settings.saveInBackground();
    m_document = saved;
    m_loadedRaw = encoded;
    m_loadedPresent = true;
    m_committing = false;
    guard.commit();
    setBlockedReason({});
    emit committed(m_document.revision);
    qCInfo(lcContainer) << "workspace commit revision" << saved.revision
                        << clock.elapsed() << "ms" << encoded.size() << "bytes";
    return {CommitStatus::Saved, saved.revision, {}};
}
} // namespace NereusSDR
