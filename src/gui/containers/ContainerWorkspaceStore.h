#pragma once
// no-port-check: NereusSDR-original transactional client presentation store.
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
