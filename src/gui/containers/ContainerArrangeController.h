#pragma once
// no-port-check: NereusSDR-original atomic presentation arrangement.
// Modification history (NereusSDR):
//   2026-10-02 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "ContainerDocument.h"
#include <QObject>
namespace NereusSDR
{
class ContainerWorkspaceStore;
class ContainerManager;
struct ArrangeResult {
    bool ok = false;
    QString error;
};
class ContainerArrangeController : public QObject
{
    Q_OBJECT
  public:
    explicit ContainerArrangeController(ContainerWorkspaceStore& store,
                                        ContainerManager* manager = nullptr,
                                        QObject* parent = nullptr);
    ArrangeResult move(const QString& entryId, const QString& destinationId, int insertionIndex);
    ArrangeResult popOut(const QString& entryId);
    ArrangeResult returnEntry(const QString& entryId);
    ArrangeResult closeContainer(const QString& containerId);
    ArrangeResult removeContainer(const QString& containerId);
    ArrangeResult duplicateEntry(const QString& entryId, const QString& destinationId,
                                 int insertionIndex);
    bool canDuplicate(const ContentEntry& entry) const;
    WorkspaceDocument workspaceSnapshot() const;
    QByteArray mimeData(const QString& entryId) const;
    ArrangeResult validateDrop(const QByteArray& data, const QString& destinationId) const;
    ArrangeResult drop(const QByteArray& data, const QString& destinationId, int insertionIndex);
    static constexpr const char* kMimeType = "application/x-nereussdr-container-entry";

  private:
    ArrangeResult commit(const WorkspaceDocument& document);
    ArrangeResult returnBatch(WorkspaceDocument& document, int source,
                              const QVector<ContentEntry>& entries);
    ContainerWorkspaceStore& m_store;
    ContainerManager* m_manager;
    const QString m_identity;
};
} // namespace NereusSDR
