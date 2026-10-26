#pragma once
// no-port-check: NereusSDR-original atomic presentation arrangement.
// Modification history (NereusSDR):
//   2026-10-02 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-08 - Workspace preview only: the header's hide button hides the
//                 entry or container (Containers > Hidden brings it back).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: setEntryConfig, an entry's own
//                 settings changed from its widget (the dial's right-click
//                 menu). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
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
    ArrangeResult placeFreeCanvas(const QString& entryId,const QRectF& rect,const QRectF& original);
    ArrangeResult move(const QString& entryId, const QString& destinationId, int insertionIndex);
    ArrangeResult popOut(const QString& entryId);
    ArrangeResult returnEntry(const QString& entryId);
    ArrangeResult closeContainer(const QString& containerId);
    // Workspace preview: an entry's saved visibility (the header's hide
    // button, Containers > Hidden). Hiding refuses a locked container.
    // Showing always works and also shows the entry's container, so the
    // entry is seen where it was.
    ArrangeResult setEntryVisible(const QString& entryId, bool visible);
    // Workspace preview: an entry's config, as its widget's own controls
    // changed it (ContentConfigNotifier). A setting, not the arrangement:
    // a locked container accepts it.
    ArrangeResult setEntryConfig(const QString& entryId, const QJsonObject& config);
    ArrangeResult removeContainer(const QString& containerId);
    ArrangeResult duplicateEntry(const QString& entryId, const QString& destinationId,
                                 int insertionIndex);
    bool canDuplicate(const ContentEntry& entry) const;
    WorkspaceDocument workspaceSnapshot() const;
    QByteArray mimeData(const QString& entryId) const;
    ArrangeResult validateDrop(const QByteArray& data, const QString& destinationId) const;
    ArrangeResult drop(const QByteArray& data, const QString& destinationId, int insertionIndex);
    static ArrangeResult moveDraft(WorkspaceDocument& document, const QString& entryId,
                                   const QString& destinationId, int insertionIndex);
    // Pure draft operation; validate every destination before mutating the candidate.
    static ArrangeResult returnBatch(WorkspaceDocument& document, int source,
                                     const QVector<ContentEntry>& entries);
    static constexpr const char* kMimeType = "application/x-nereussdr-container-entry";

  private:
    ArrangeResult commit(const WorkspaceDocument& document);
    ContainerWorkspaceStore& m_store;
    ContainerManager* m_manager;
    const QString m_identity;
};
} // namespace NereusSDR
