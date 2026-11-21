#pragma once
// no-port-check: NereusSDR-original native free-placement presentation.
// Modification history (NereusSDR):
//   2026-10-08 - Workspace preview only: the header's hide button hides the
//                 entry or container (Containers > Hidden brings it back).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: entryPopOutRequested for the leaf
//                 header's pop-out button. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-09 - The minimum follows where the views are now: each
//                 presentation sync starts from the extent placeViews laid
//                 out, not from the last sync's, and grows by each view's
//                 widget geometry, not its native window's.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Each leaf remembers the geometry placeViews set, so a
//                 sync can put a view moved since back where it was placed.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include "ContainerDocument.h"
#include <QWidget>
#include <QPointer>
#include <QHash>
namespace NereusSDR {
// Does not own borrowed views. The host controls materialization and teardown.
class FreeCanvasSurface final : public QWidget {
    Q_OBJECT
public:
    explicit FreeCanvasSurface(QWidget* parent=nullptr);
    void project(const ContainerDocument&, const QHash<QString,QPointer<QWidget>>& views);
    void clearViews();
    QRectF logicalRect(const QString&) const;
    QRect entryBoundary(const QString&) const;
    int contentHeight() const;
    void selectEntry(const QString&);
signals:
    void entrySelected(const QString&);
    void geometryEdited(const QString&,const QRectF&);
    void geometryRestored(const QString&,const QJsonValue&,bool present);
    void geometryCommitted(const QString&,const QRectF&,const QRectF& original);
    void entryContextMenuRequested(const QString&,const QPoint& globalPosition);
    // Workspace preview: the entry header's pop-out button.
    void entryPopOutRequested(const QString&);
    // Workspace preview: the entry header's hide button.
    void entryHideRequested(const QString&);
protected:
    bool eventFilter(QObject*,QEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
private:
    // placed: the geometry placeViews set; putBacks: how often a sync has
    // moved the view back to it since.
    struct Leaf { QPointer<QWidget> view, grip, corner; QRectF rect; int order=0; QRect placed; int putBacks=0; };
    void placeViews();
    QRect projectedBoundary(QWidget*) const;
    void syncPresentation();
    void queuePresentationSync();
    void updateReveal(const QString& hover={});
    QString hoverAt(const QPoint& position) const;
    void changeRect(const QString&,const QRectF&);
    // Free-canvas movement link: the entries that move with `id` (itself first).
    QStringList moveGroupFor(const QString& id) const;
    void finishGesture(bool cancel);
    QHash<QString,Leaf> m_leaves;
    ContainerDocument m_document;
    QPointF m_origin, m_press;
    QRectF m_original;
    QJsonValue m_originalGeometry;
    bool m_originalGeometryPresent=false;
    QSizeF m_extent;
    QSizeF m_logicalExtent; // placeViews' extent; each presentation sync starts here
    QString m_selected, m_active;
    // Entries dragged with m_active, their geometry at the press, and the
    // saved free-canvas rect each had then (for Escape).
    QStringList m_group;
    QHash<QString,QRectF> m_groupOriginal;
    QHash<QString,QJsonValue> m_groupGeometry;
    QHash<QString,bool> m_groupGeometryPresent;
    QPointer<QWidget> m_capture;
    bool m_resizing=false, m_presentationSyncQueued=false;
};
}
