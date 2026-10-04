// no-port-check: NereusSDR-original inert preview projection, no polling targets.
// Modification history (NereusSDR):
//   2026-10-03 — Scroll the full configured preview stack by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-02 — Safe previews by J.J. Boyd (KG4VCF), OpenAI Codex assisted.
#include "ContainerPreviewWidget.h"
#include "ContainerContentRegistry.h"
#include "ContainerContentHost.h"
#include "FreeCanvasSurface.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QSet>
#include <memory>
namespace NereusSDR {
ContainerPreviewWidget::ContainerPreviewWidget(ContainerContentRegistry& registry, MeterPoller& poller, QWidget* parent)
    : QWidget(parent),m_registry(&registry),m_poller(&poller),m_layout(new QVBoxLayout(this))
{
    setObjectName("containerDraftPreview"); setMinimumHeight(180);
    m_layout->setContentsMargins(0,0,0,0); m_layout->setSpacing(0);
    m_layout->setSizeConstraint(QLayout::SetMinimumSize);
    m_canvas=new FreeCanvasSurface(this);m_canvas->hide();
    connect(m_canvas,&FreeCanvasSurface::entrySelected,this,&ContainerPreviewWidget::entrySelected);
    connect(m_canvas,&FreeCanvasSurface::geometryEdited,this,[this](const QString& id,const QRectF& rect){
        if(m_document.locked) {return;}
        for(auto& entry:m_document.contents) {if(entry.id==id) {entry.setFreeCanvasRect(rect);break;}}
        emit freeCanvasRectEdited(id,rect);
    });
    connect(m_canvas,&FreeCanvasSurface::geometryRestored,this,[this](const QString& id,const QJsonValue& value,bool present){
        for(auto& entry:m_document.contents) {if(entry.id==id) {if(present) {entry.extensions["freeCanvasRect"]=value;}else {entry.extensions.remove("freeCanvasRect");}emit freeCanvasGeometryRestored(entry);break;}}
    });
    connect(&poller,&MeterPoller::frameAdvanced,this,&ContainerPreviewWidget::advance);
    connect(&poller,&MeterPoller::bindingAvailabilityChanged,this,[this]{
        if(!m_poller) { return; }
        for(const auto& surface:m_surfaces) {
            if(surface.widget) { m_poller->copyCachedReadings(surface.widget,surface.context); }
        }
    });
    connect(&poller,&MeterPoller::bindingSupportChanged,this,[this]{
        if(!m_poller) { return; }
        for(const auto& surface:m_surfaces) {
            if(surface.widget) { m_poller->copyCachedReadings(surface.widget,surface.context); }
        }
    });
    connect(&registry,&ContainerContentRegistry::runtimeChanged,this,[this]{ m_materialized=false; setDocument(m_document); });
}
void ContainerPreviewWidget::setDocument(const ContainerDocument& document)
{
    if (!m_registry || !m_poller || (m_materialized && document==m_document)) { return; }
    // Preserve unaffected native leaves and their dynamics across draft changes.
    if (m_materialized && document.layout==m_document.layout && document.contents==m_document.contents
        && document.config==m_document.config) {
        m_document=document;if(document.layout==ContentLayout::FreeCanvas) {m_canvas->project(document,m_views);}return;
    }
    const auto prior=m_document; const bool reuse=m_materialized && document.layout!=ContentLayout::LegacyCanvas && prior.layout==document.layout;
    const auto oldViews=m_views; QSet<QWidget*> retained;
    setStyleSheet(QStringLiteral("#containerDraftPreview {background:%1;}").arg(document.config.value("backgroundColor").toString("#0f0f1a")));
    m_document=document; m_materialized=true; m_surfaces.clear(); m_views.clear();
    while (auto* child=m_layout->takeAt(0)) { delete child; }
    if(document.layout==ContentLayout::FreeCanvas) {m_layout->addWidget(m_canvas);m_canvas->show();}
    else {m_canvas->clearViews();m_canvas->hide();}
    MeterWidget* canvas=nullptr; QSet<MeterWidget*> reusedMeters;
    for (const auto& entry : document.contents) {
        if (!entry.visible) { continue; }
        const auto context=ContainerContentHost::effectiveContext(document,entry);
        if (document.layout==ContentLayout::LegacyCanvas && !entry.typeId.startsWith("applet:") && m_registry->isAvailable(entry.typeId)) {
            std::unique_ptr<MeterItem> item(m_registry->createMeterItem(entry,nullptr,ContentRenderMode::Preview));
            if(item) {
                if (!canvas) { canvas=new MeterWidget(); canvas->setAttribute(Qt::WA_TransparentForMouseEvents); canvas->setParent(this); canvas->setMinimumHeight(220); m_layout->addWidget(canvas); m_surfaces.append({canvas,context}); }
                item->setProperty("containerSourceContext",context);
                if (context!=m_surfaces.first().context) {
                    item->setProperty("containerUnsupportedSource",true);
                    item->setProperty("unsupportedSourceReason",QStringLiteral("This legacy canvas contains different slice sources; move this entry to a stack to read its source"));
                }
                canvas->addItem(item.release()); m_views[entry.id]=canvas; continue;
            }
        }
        QWidget* view=nullptr;
        if(reuse && oldViews.value(entry.id)) {
            for(const auto& old:prior.contents) {
                ContentEntry previous=old,current=entry;
                if(document.layout==ContentLayout::FreeCanvas) {previous.extensions.remove("freeCanvasRect");current.extensions.remove("freeCanvasRect");previous.paintOrder=current.paintOrder;}
                if(previous==current && ContainerContentHost::effectiveContext(prior,old)==context) { view=oldViews.value(entry.id); break; }
            }
        }
        if(view) { retained.insert(view); if(auto* meter=qobject_cast<MeterWidget*>(view)) { reusedMeters.insert(meter); } }
        else { view=m_registry->createPreview(entry,this); }
        if(document.layout==ContentLayout::FreeCanvas && entry.typeId.startsWith("applet:")) {
            view->setProperty("freeCanvasMinimum",QSizeF(m_registry->singletonCanvasMinimum(entry.typeId)));
            view->setProperty("freeCanvasHint",m_registry->singletonCanvasSizeHint(entry.typeId));
            if(view->layout()) {view->layout()->setSizeConstraint(QLayout::SetNoConstraint);}
            view->setMinimumSize(0,0);view->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
        }
        m_views[entry.id]=view; if(document.layout!=ContentLayout::FreeCanvas) {m_layout->addWidget(view);}
        if (auto* meter=qobject_cast<MeterWidget*>(view)) {
            int height=80;QSize minimum(24,24);
            for (auto* item : meter->items()) {
                if (auto* face=qobject_cast<CompositePresetItem*>(item)) { height=face->preferredFaceHeight(); minimum=face->minimumFaceSize(); meter->setMinimumWidth(minimum.width()); }
                else if (auto* face=qobject_cast<BarPresetItem*>(item)) { height=face->preferredFaceHeight(); minimum=face->minimumFaceSize(); meter->setMinimumWidth(minimum.width()); }
                item->setZOrder(entry.paintOrder);item->clearStackMetadata(); item->setRect(0,0,1,1); item->setProperty("containerSourceContext",context);
            }
            if(document.layout==ContentLayout::FreeCanvas) {
                meter->setProperty("freeCanvasMinimum",QSizeF(minimum));
                meter->setMinimumSize(0,0);meter->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
            } else {meter->setFixedHeight(height);}
            m_surfaces.append({meter,context});
        }
    }
    QSet<QWidget*> retired;
    for(const auto& view:oldViews) { if(view && !retained.contains(view) && !retired.contains(view)) { retired.insert(view); delete view.data(); } }
    if(document.layout==ContentLayout::FreeCanvas) {m_canvas->project(document,m_views);}
    m_inTx=m_poller->inTx();
    for (const auto& surface : m_surfaces) {
        if (!surface.widget) { continue; }
        if (!reusedMeters.contains(surface.widget)) { m_poller->replayReadings(surface.widget,surface.context); }
        emit presentationRequested(surface.widget,surface.context);
    }
}
QRect ContainerPreviewWidget::entryBoundary(const QString& id) const
{
    QWidget* view=m_views.value(id);if(!view) {return {};}
    return QRect(view->mapTo(const_cast<ContainerPreviewWidget*>(this),QPoint()),view->size());
}
QRectF ContainerPreviewWidget::resolvedFreeCanvasRect(const QString& id) const {return m_canvas->logicalRect(id);}
QSizeF ContainerPreviewWidget::freeCanvasMinimum(const QString& id) const {const auto view=m_views.value(id);return view?view->property("freeCanvasMinimum").toSizeF().expandedTo(QSizeF(24,24)):QSizeF(24,24);}
void ContainerPreviewWidget::selectEntry(const QString& id) {m_canvas->selectEntry(id);}
ContainerDocument ContainerPreviewWidget::seededFromStack() const
{
    ContainerDocument result=m_document;
    if(result.locked || result.layout!=ContentLayout::VerticalStack || !m_registry) {return result;}
    m_layout->activate();
    QRectF extent;
    for(auto& entry:result.contents) {
        if(!entry.visible || !m_registry->isAvailable(entry.typeId)) {continue;}
        if(!entry.typeId.startsWith("applet:") && !qobject_cast<MeterWidget*>(m_views.value(entry.id).data())) {continue;}
        const QRect boundary=entryBoundary(entry.id);if(boundary.isEmpty()) {continue;}
        entry.setFreeCanvasRect(boundary);extent=extent.united(boundary);
    }
    result.setFreeCanvasExtent(QSizeF(qMax(0.,extent.right()),qMax(0.,extent.bottom())));
    result.layout=ContentLayout::FreeCanvas;return result;
}
ContainerDocument ContainerPreviewWidget::convertedLegacyPositions() const
{
    ContainerDocument result=m_document;if(result.locked) {return result;}
    const QSizeF extent(qMax(320,width()),qMax(220,height()));
    for(auto& entry:result.contents) {
        if(!entry.visible || !m_registry || !m_registry->isAvailable(entry.typeId)) {continue;}
        const QRectF saved=entry.canvasRect;
        if(saved.isNull()) {continue;}
        entry.setFreeCanvasRect(QRectF(saved.x()*extent.width(),saved.y()*extent.height(),saved.width()*extent.width(),saved.height()*extent.height()));
    }
    result.setFreeCanvasExtent(extent);result.layout=ContentLayout::FreeCanvas;return result;
}
void ContainerPreviewWidget::advance(qint64 timestamp)
{
    if (!m_poller) { return; }
    for (const auto& surface : m_surfaces) {
        if (!surface.widget) { continue; }
        if (m_inTx!=m_poller->inTx()) { surface.widget->resetForTxTransition(m_poller->inTx()); }
        m_poller->copyCachedReadings(surface.widget,surface.context);
        emit presentationRequested(surface.widget,surface.context);
        surface.widget->advanceMeters(timestamp);
    }
    m_inTx=m_poller->inTx();
}
}
