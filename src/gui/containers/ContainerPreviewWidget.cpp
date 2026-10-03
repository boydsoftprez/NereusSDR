// no-port-check: NereusSDR-original inert preview projection, no polling targets.
// Modification history (NereusSDR):
//   2026-10-03 — Scroll the full configured preview stack by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-02 — Safe previews by J.J. Boyd (KG4VCF), OpenAI Codex assisted.
#include "ContainerPreviewWidget.h"
#include "ContainerContentRegistry.h"
#include "ContainerContentHost.h"
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
    connect(&poller,&MeterPoller::frameAdvanced,this,&ContainerPreviewWidget::advance);
    connect(&poller,&MeterPoller::bindingAvailabilityChanged,this,[this]{
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
        && document.config==m_document.config) { m_document=document; return; }
    const auto prior=m_document; const bool reuse=m_materialized && document.layout==ContentLayout::VerticalStack && prior.layout==document.layout;
    const auto oldViews=m_views; QSet<QWidget*> retained;
    setStyleSheet(QStringLiteral("#containerDraftPreview {background:%1;}").arg(document.config.value("backgroundColor").toString("#0f0f1a")));
    m_document=document; m_materialized=true; m_surfaces.clear(); m_views.clear();
    while (auto* child=m_layout->takeAt(0)) { delete child; }
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
                if(old==entry && ContainerContentHost::effectiveContext(prior,old)==context) { view=oldViews.value(entry.id); break; }
            }
        }
        if(view) { retained.insert(view); if(auto* meter=qobject_cast<MeterWidget*>(view)) { reusedMeters.insert(meter); } }
        else { view=m_registry->createPreview(entry,this); }
        m_views[entry.id]=view; m_layout->addWidget(view);
        if (auto* meter=qobject_cast<MeterWidget*>(view)) {
            int height=80;
            for (auto* item : meter->items()) {
                if (auto* face=qobject_cast<CompositePresetItem*>(item)) { height=face->preferredFaceHeight(); meter->setMinimumWidth(face->minimumFaceSize().width()); }
                else if (auto* face=qobject_cast<BarPresetItem*>(item)) { height=face->preferredFaceHeight(); meter->setMinimumWidth(face->minimumFaceSize().width()); }
                item->clearStackMetadata(); item->setRect(0,0,1,1); item->setProperty("containerSourceContext",context);
            }
            meter->setFixedHeight(height); m_surfaces.append({meter,context});
        }
    }
    QSet<QWidget*> retired;
    for(const auto& view:oldViews) { if(view && !retained.contains(view) && !retired.contains(view)) { retired.insert(view); delete view.data(); } }
    m_inTx=m_poller->inTx();
    for (const auto& surface : m_surfaces) {
        if (!surface.widget) { continue; }
        if (!reusedMeters.contains(surface.widget)) { m_poller->replayReadings(surface.widget,surface.context); }
        emit presentationRequested(surface.widget,surface.context);
    }
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
