#pragma once
// no-port-check: NereusSDR-original committed mixed-content projection.
// Modification history (NereusSDR):
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "ContainerDocument.h"
#include <QWidget>
#include <QPointer>
#include <QVector>
class QVBoxLayout;
class QMenu;
namespace NereusSDR {
class ContainerContentRegistry;
class MeterWidget;
class MeterItem;
class ContainerContentHost : public QWidget {
    Q_OBJECT
public:
    struct EntryRow {
        QString entryId;
        QPointer<QWidget> widget;
        QPointer<MeterItem> item;
        QJsonObject context;
        int offset = 0, height = 0;
        bool effectiveVisible = false;
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
    static QJsonObject effectiveContext(const ContainerDocument& document, const ContentEntry& entry);
signals:
    void meterSurfaceReady(MeterWidget* meter, const QJsonObject& context);
    void reconciled();
private:
    ContainerContentRegistry& m_registry;
    ContainerDocument m_document;
    QVector<EntryRow> m_rows;
    QVector<QPointer<MeterWidget>> m_meters;
    QVBoxLayout* m_layout = nullptr;
    QWidget* m_body = nullptr;
    quint64 m_generation = 0;
    bool m_materialized = false;
    QPointer<QMenu> m_bannerMenu;
};
}
