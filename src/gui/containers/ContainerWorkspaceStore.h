#pragma once
// no-port-check: NereusSDR-original transactional client presentation store.
// Modification history (NereusSDR):
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "ContainerDocument.h"
#include <QByteArray>
#include <QObject>

namespace NereusSDR {
class AppSettings;
class ContainerWorkspaceStore : public QObject {
    Q_OBJECT
public:
    explicit ContainerWorkspaceStore(AppSettings& settings, QObject* parent = nullptr);
    DocumentResult load();
    WorkspaceDocument snapshot() const;
    QString loadError() const { return m_loadError; }
    CommitResult commit(const WorkspaceDocument& document, quint64 expectedRevision);
signals:
    void committed(quint64 revision);
private:
    AppSettings& m_settings;
    WorkspaceDocument m_document;
    QByteArray m_loadedRaw;
    bool m_loadedPresent = false;
    QString m_loadError;
    bool m_committing = false;
};
} // namespace NereusSDR
