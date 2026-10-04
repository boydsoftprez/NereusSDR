#pragma once
// no-port-check: NereusSDR-original inert document preview using shared GUI caches.
#include "ContainerDocument.h"
#include <QWidget>
#include <QPointer>
class QVBoxLayout;
namespace NereusSDR {
class ContainerContentRegistry;
class MeterPoller;
class MeterWidget;
class FreeCanvasSurface;
class ContainerPreviewWidget : public QWidget {
    Q_OBJECT
public:
    ContainerPreviewWidget(ContainerContentRegistry&, MeterPoller&, QWidget* parent=nullptr);
    void setDocument(const ContainerDocument&);
    ContainerDocument seededFromStack() const;
    ContainerDocument convertedLegacyPositions() const;
    QRect entryBoundary(const QString&) const;
    QRectF resolvedFreeCanvasRect(const QString&) const;
    QSizeF freeCanvasMinimum(const QString&) const;
    void selectEntry(const QString&);
    const ContainerDocument& document() const { return m_document; }
signals:
    void freeCanvasGeometryRestored(const ContentEntry&);
    void freeCanvasRectEdited(const QString&,const QRectF&);
    void entrySelected(const QString&);
    void presentationRequested(MeterWidget*, const QJsonObject&);
private:
    void advance(qint64 timestamp);
    QPointer<ContainerContentRegistry> m_registry;
    QPointer<MeterPoller> m_poller;
    QVBoxLayout* m_layout;
    FreeCanvasSurface* m_canvas=nullptr;
    ContainerDocument m_document;
    struct Surface { QPointer<MeterWidget> widget; QJsonObject context; };
    QVector<Surface> m_surfaces;
    QHash<QString,QPointer<QWidget>> m_views;
    bool m_materialized=false, m_inTx=false;
};
}
