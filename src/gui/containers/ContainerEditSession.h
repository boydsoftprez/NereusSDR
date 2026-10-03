#pragma once
// no-port-check: NereusSDR-original client-local workspace draft transaction.
#include "ContainerWorkspaceStore.h"
#include <QStringList>
namespace NereusSDR {
class ContainerEditSession {
public:
    explicit ContainerEditSession(ContainerWorkspaceStore& store);
    const WorkspaceDocument& draft() const { return m_draft; }
    void setDraft(const WorkspaceDocument& document) { m_draft = document; m_casConflicts.clear(); }
    CommitResult apply();
    void cancel();
    QStringList conflictingContainers() const;
    void reloadContainers(const QStringList& ids);
private:
    ContainerWorkspaceStore& m_store;
    WorkspaceDocument m_base, m_draft;
    QStringList m_casConflicts;
    QStringList conflicts(const WorkspaceDocument& live) const;
};
}
