#pragma once
// no-port-check: NereusSDR-original transactional client presentation store.
// Modification history (NereusSDR):
//   2026-10-27 - Notify consumers on every successful workspace load.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-08 - Workspace preview only: workspaceKey(), the preview's own
//                 ContainerWorkspacePreview key. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - blockedReason() and blockedChanged: the two states that
//                 refuse every commit until a reload (a load error, or the
//                 saved bytes changed underneath) are reported, and every
//                 refused commit is logged. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
#include "ContainerDocument.h"
#include "gui/NereusGuiExport.h"
#include <QByteArray>
#include <QObject>

namespace NereusSDR {
class AppSettings;
class NEREUS_GUI_EXPORT ContainerWorkspaceStore : public QObject {
    Q_OBJECT
public:
    explicit ContainerWorkspaceStore(AppSettings& settings, QObject* parent = nullptr);
    DocumentResult load();
    WorkspaceDocument snapshot() const;
    QString loadError() const { return m_loadError; }
    CommitResult commit(const WorkspaceDocument& document, quint64 expectedRevision);
    // The AppSettings key the document lives in: ContainerWorkspace, or
    // ContainerWorkspacePreview with the workspace preview switch on.
    QString workspaceKey() const { return m_workspaceKey; }
    // Why every commit is refused until the workspace is loaded again: the
    // stored load error, or the saved bytes changed under this store. Empty
    // while commits can be saved. A successful load() or commit clears it.
    QString blockedReason() const { return m_blockedReason; }
signals:
    void committed(quint64 revision);
    // The workspace was loaded successfully and snapshot() is current.
    void loaded();
    // blockedReason() changed; empty when commits can be saved again.
    void blockedChanged(const QString& reason);
private:
    void setBlockedReason(const QString& reason);
    CommitResult refuse(CommitStatus status, quint64 revision, const QString& reason) const;
    AppSettings& m_settings;
    QString m_workspaceKey;
    QString m_backupKey;
    WorkspaceDocument m_document;
    QByteArray m_loadedRaw;
    bool m_loadedPresent = false;
    QString m_loadError;
    bool m_committing = false;
    QString m_blockedReason;
};
} // namespace NereusSDR
