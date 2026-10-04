#pragma once
// no-port-check: NereusSDR-original native free-placement presentation.
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
protected:
    bool eventFilter(QObject*,QEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
private:
    struct Leaf { QPointer<QWidget> view, grip, corner; QRectF rect; int order=0; };
    void placeViews();
    void updateReveal(const QString& hover={});
    void changeRect(const QString&,const QRectF&);
    void finishGesture(bool cancel);
    QHash<QString,Leaf> m_leaves;
    ContainerDocument m_document;
    QPointF m_origin, m_press;
    QRectF m_original;
    QJsonValue m_originalGeometry;
    bool m_originalGeometryPresent=false;
    QSizeF m_extent;
    QString m_selected, m_active;
    QPointer<QWidget> m_capture;
    bool m_resizing=false;
};
}
