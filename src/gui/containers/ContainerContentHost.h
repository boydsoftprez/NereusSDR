#pragma once
// no-port-check: NereusSDR-original committed mixed-content projection.
// Modification history (NereusSDR):
//   2026-10-04 — Runtime responsive stack projection by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "ContainerDocument.h"
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
class ContainerContentHost : public QWidget {
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
    static QJsonObject effectiveContext(const ContainerDocument& document, const ContentEntry& entry);
signals:
    void meterSurfaceReady(MeterWidget* meter, const QJsonObject& context);
    void reconciled();
    void preferredContentHeightChanged();
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    void reconcileFreeCanvas(const ContainerDocument&);
    void updateGrips();
    void scheduleStackProjection();
    void projectStackRows();
    void addEntryActions(QMenu& menu, const QString& entryId);
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
    QPointer<QMenu> m_bannerMenu;
};
}
