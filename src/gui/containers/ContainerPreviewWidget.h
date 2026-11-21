#pragma once
// no-port-check: NereusSDR-original inert document preview using shared GUI caches.
// Modification history (NereusSDR):
//   2026-11-05 - NEREUS_GUI_EXPORT: this class's signals are used from outside
//                 its DLL. Without the export their addresses were MinGW's
//                 auto-import thunks, which moc's code does not recognize,
//                 so a QSignalSpy or connect on them failed on Windows.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Widget-factory rows with Fade on RX / TX get the fade
//                 overlay and the radio's transmit state, read only
//                 (m_fadeViews). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include "ContainerDocument.h"
#include "gui/NereusGuiExport.h"
#include <QWidget>
#include <QPointer>
class QVBoxLayout;
namespace NereusSDR {
class ContainerContentRegistry;
class MeterPoller;
class MeterWidget;
class FreeCanvasSurface;
class NEREUS_GUI_EXPORT ContainerPreviewWidget : public QWidget {
    Q_OBJECT
public:
    ContainerPreviewWidget(ContainerContentRegistry&, MeterPoller&, QWidget* parent=nullptr);
    void setDocument(const ContainerDocument&);
    ContainerDocument seededFromStack() const;
    ContainerDocument convertedLegacyPositions(QSizeF targetExtent = {}) const;
    QRect entryBoundary(const QString&) const;
    QRectF resolvedFreeCanvasRect(const QString&) const;
    QSizeF freeCanvasMinimum(const QString&) const;
    void selectEntry(const QString&);
    const ContainerDocument& document() const { return m_document; }
protected:
    void resizeEvent(QResizeEvent*) override;
signals:
    void freeCanvasGeometryRestored(const ContentEntry&);
    void freeCanvasRectEdited(const QString&,const QRectF&);
    void entrySelected(const QString&);
    void presentationRequested(MeterWidget*, const QJsonObject&);
private:
    void updateLegacyCanvasHeight();
    void advance(qint64 timestamp);
    // Fade on RX / TX on the widget rows: the transmit state each overlay
    // shows (one on another session's radio stays in the receive state).
    void updateFadeViews();
    QPointer<ContainerContentRegistry> m_registry;
    QPointer<MeterPoller> m_poller;
    QVBoxLayout* m_layout;
    // A legacy Thetis canvas is drawn in the container's aspect ratio, not
    // stretched to the dialog's width at a fixed height: text sizes scale
    // with width, so a stretched canvas makes the rows overlap.
    MeterWidget* m_legacyCanvas{nullptr};
    double m_legacyAspect{0.5625};
    // The live container's content size, so the preview is a 1:1 copy
    // rather than a differently-scaled version of it.
    int m_legacyWidth{0};
    int m_legacyContentHeight{0};
    FreeCanvasSurface* m_canvas=nullptr;
    ContainerDocument m_document;
    struct Surface { QPointer<MeterWidget> widget; QJsonObject context; };
    QVector<Surface> m_surfaces;
    QHash<QString,QPointer<QWidget>> m_views;
    struct FadeView { QPointer<QWidget> widget; QJsonObject context; };
    QVector<FadeView> m_fadeViews;
    bool m_materialized=false, m_inTx=false;
};
}
