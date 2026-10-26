#pragma once
// no-port-check: NereusSDR-original committed mixed-content projection.
// Modification history (NereusSDR):
//   2026-10-26 - A meter item's own config edits (ContentConfigNotifier,
//                 the other buttons' reorder) are committed to its entry
//                 the same way. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - The container header mode is Shown or Hidden. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Workspace preview only: setTopOverlayInset keeps overlaid
//                 entry headers below the container's overlaid group
//                 header. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - Workspace preview only: a factory widget's own config
//                 edits (ContentConfigNotifier) are committed through the
//                 arrange controller, and a widget already showing the new
//                 config is kept. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - The host's minimum width is what its contents need (the
//                 widest visible row, the body margins and the vertical bar)
//                 and contentMinimumWidthChanged tells the container; in the
//                 workspace preview the body gives its right margin to the
//                 bar while the bar shows. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: popOutEntry for the entry header's
//                 pop-out button. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-07 - Workspace preview only: entry headers in place of the
//                 left gutter grips, Arrange and the container's header
//                 mode by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-04 — Runtime responsive stack projection by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "ContainerDocument.h"
#include "ContainerTypes.h"
#include "gui/NereusGuiExport.h"
#include <QHash>
#include <QWidget>
#include <QPointer>
#include <QVector>
class QVBoxLayout;
class QScrollArea;
class QMenu;
namespace NereusSDR {
class ContainerArrangeController;
class ContainerContentRegistry;
class MeterWidget;
class MeterItem;
class FreeCanvasSurface;
class WorkspaceEntryHeader;
class NEREUS_GUI_EXPORT ContainerContentHost : public QWidget {
    Q_OBJECT
public:
    struct EntryRow {
        QString entryId;
        QPointer<QWidget> widget;
        QPointer<MeterItem> item;
        QJsonObject context;
        int offset = 0, height = 0;
        int baseHeight = 0; // configured calibration, never a projected size
        bool effectiveVisible = false;
        bool presentationVisible = false; // includes a visible unavailable placeholder
    };
    explicit ContainerContentHost(ContainerContentRegistry& registry, QWidget* parent = nullptr);
    ~ContainerContentHost() override;
    void reconcile(const ContainerDocument& document);
    bool needsReconcile(const ContainerDocument& document) const;
    QVector<MeterWidget*> meterSurfaces() const;
    const QVector<EntryRow>& entryRows() const { return m_rows; }
    QRect entryBoundary(const QString& entryId) const;
    QJsonObject sourceContext(const MeterWidget* meter) const;
    ContainerDocument captureDocument() const;
    void releaseViews(); // synchronous native meter teardown before window reparent
    void setBannerMenu(QMenu* menu);
    void setArrangeController(ContainerArrangeController* controller);
    void addContentsMenu(QMenu& menu);
    int insertionIndex(const QPoint& position) const;
    int preferredContentHeight() const;
    QRect gripGeometry(const QString& entryId) const;
    // Workspace preview: Arrange shows every entry header; the container's
    // header mode decides whether they show outside Arrange (Always only).
    void setArrangeActive(bool active);
    bool arrangeActive() const { return m_arrangeActive; }
    void setContainerHeaderMode(HeaderMode mode);
    // Workspace preview: the height, from the host's top, that the
    // container's overlaid group header covers (0 when it is in the layout
    // or hidden). An overlaid entry header sits below it, so the group
    // header never covers an entry header's buttons.
    void setTopOverlayInset(int px);
    // Workspace preview: the entry header of one entry, or nullptr.
    WorkspaceEntryHeader* entryHeader(const QString& entryId) const;
    static QJsonObject effectiveContext(const ContainerDocument& document, const ContentEntry& entry);
    // The width the contents need: the scrolled body's minimum (the widest
    // visible row and the body margins) and the vertical bar's extent. A
    // free canvas needs none. Also the host's own minimum width.
    int contentMinimumWidth() const { return qMax(0, m_contentMinimumWidth); }
    // The body's right margin with no vertical bar beside it (it matches the
    // 8 px bar of the legacy applet panel).
    static constexpr int kBodyRightMarginPx = 8;
signals:
    void meterSurfaceReady(MeterWidget* meter, const QJsonObject& context);
    void reconciled();
    void preferredContentHeightChanged();
    // The content's minimum width changed (contentMinimumWidth()).
    void contentMinimumWidthChanged(int width);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    void reconcileFreeCanvas(const ContainerDocument&);
    void connectConfigNotifier(QWidget* widget, const QString& entryId);
    // A meter item with a ContentConfigNotifier child: the keys it emits
    // replace those of its entry's config, committed like a widget's.
    void connectItemConfigNotifier(MeterItem* item, const QString& entryId);
    void updateGrips();
    void scheduleStackProjection();
    void projectStackRows();
    void addEntryActions(QMenu& menu, const QString& entryId);
    void popOutEntry(const QString& entryId);
    void hideEntry(const QString& entryId);
    void updateEntryHeaders();
    int entryHeaderReserve() const;
    QString entryTitle(const ContentEntry& entry) const;
    void updateContentMinimumWidth();
    int verticalBarExtent() const;
    void applyPreviewRightMargin();
    QHash<QString, QPointer<WorkspaceEntryHeader>> m_entryHeaders;
    HeaderMode m_containerHeaderMode = HeaderMode::Shown;
    bool m_arrangeActive = false;
    int m_topOverlayInset = 0;
    QPointer<ContainerArrangeController> m_arrange;
    QVector<QPointer<QWidget>> m_grips;
    QWidget* m_indicator = nullptr;
    QPoint m_dragStart;
    QString m_pressedEntry;
    ContainerContentRegistry& m_registry;
    ContainerDocument m_document;
    QVector<EntryRow> m_rows;
    QVector<QPointer<MeterWidget>> m_meters;
    QVBoxLayout* m_layout = nullptr;
    QWidget* m_body = nullptr;
    QScrollArea* m_scroll = nullptr;
    FreeCanvasSurface* m_canvas = nullptr;
    quint64 m_generation = 0;
    quint64 m_projectionEpoch = 0;
    bool m_projectionPending = false;
    int m_lastPreferredHeight = -1;
    bool m_materialized = false;
    int m_surfaceMinimumWidth = 0;   // the meter surfaces' minimum and gutter
    int m_contentMinimumWidth = -1;  // last value announced; -1 before the first
    QPointer<QMenu> m_bannerMenu;
};
}
