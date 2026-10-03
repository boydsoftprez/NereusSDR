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
class ContainerPreviewWidget : public QWidget {
    Q_OBJECT
public:
    ContainerPreviewWidget(ContainerContentRegistry&, MeterPoller&, QWidget* parent=nullptr);
    void setDocument(const ContainerDocument&);
    const ContainerDocument& document() const { return m_document; }
signals:
    void presentationRequested(MeterWidget*, const QJsonObject&);
private:
    void advance(qint64 timestamp);
    QPointer<ContainerContentRegistry> m_registry;
    QPointer<MeterPoller> m_poller;
    QVBoxLayout* m_layout;
    ContainerDocument m_document;
    struct Surface { QPointer<MeterWidget> widget; QJsonObject context; };
    QVector<Surface> m_surfaces;
    QHash<QString,QPointer<QWidget>> m_views;
    bool m_materialized=false, m_inTx=false;
};
}
