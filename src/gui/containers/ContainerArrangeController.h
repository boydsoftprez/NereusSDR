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
// One block of a legacy canvas gesture: where it ended and the record that
// carries the same geometry. A move group reports all of its blocks so the
// gesture commits once.
struct LegacyItemRect {
    QString entryId;
    QRectF rect;
    QString record;
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
    ArrangeResult setEntryCanvasRect(const QString& entryId, const QRectF& canvasRect);
    // One atomic write for a dragged legacy item: its normalized rect and the
    // serialized legacy record that the legacy renderer actually reads.
    ArrangeResult setLegacyItem(const QString& entryId, const QRectF& canvasRect,
                                const QString& legacyRecord);
    // The same for a whole gesture: every block a move group carried, written
    // in one commit (one settings save, one reconcile).
    ArrangeResult setLegacyItems(const QVector<LegacyItemRect>& items);
    // The position a block had before it was first moved. Recorded on the
    // first drag (the entry keeps it), so "reset" can put it back.
    ArrangeResult resetEntryPosition(const QString& entryId);
    ArrangeResult resetContainerPositions(const QString& containerId);
    // Make where the blocks are now the position a future reset returns to.
    ArrangeResult rememberContainerPositions(const QString& containerId);
    // Free-canvas movement link: while linked, dragging either entry moves both
    // by the same offset, keeping their own geometry. Stored in the entries'
    // extensions so it survives restarts. Entries keep their own bindings.
    ArrangeResult setMoveLink(const QString& entryId, const QString& withEntryId, bool linked);
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
