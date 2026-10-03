// no-port-check: NereusSDR-original mixed-content projection, no radio actions.
// Modification history (NereusSDR):
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "ContainerContentHost.h"
#include "ContainerContentRegistry.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <QJsonArray>
#include <QScrollArea>
#include <algorithm>
namespace NereusSDR {
ContainerContentHost::ContainerContentHost(ContainerContentRegistry& registry, QWidget* parent)
    : QWidget(parent), m_registry(registry)
{
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0);
    auto* scroll = new QScrollArea(this); scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_body = new QWidget(scroll); m_layout = new QVBoxLayout(m_body);
    scroll->setWidget(m_body); root->addWidget(scroll);
    m_layout->setContentsMargins(0, 0, 8, 0);
    m_layout->setSpacing(0);
}
ContainerContentHost::~ContainerContentHost() { releaseViews(); }
QJsonObject ContainerContentHost::effectiveContext(const ContainerDocument& document, const ContentEntry& entry)
{
    QJsonObject context;
    // Preserve identity axes; binding/stack/display fields are per-item, not source.
    for (const QString& key : {QStringLiteral("sessionId"), QStringLiteral("sliceId"), QStringLiteral("rxSource")}) {
        if (document.config.contains(key)) { context[key] = document.config[key]; }
        if (entry.context.contains(key)) { context[key] = entry.context[key]; }
    }
    if (entry.context.contains("rxSource") && !entry.context.contains("sliceId")) {
        context["sliceId"] = entry.context["rxSource"].toInt() - 1;
    } else if (!context.contains("sliceId") && context.contains("rxSource")) {
        context["sliceId"] = context["rxSource"].toInt() - 1;
    }
    context.remove("rxSource");
    return context;
}
void ContainerContentHost::releaseViews()
{
    for (const auto& row : std::as_const(m_rows)) {
        if (row.widget && row.widget->parentWidget() == m_body && !row.item && !row.widget->property("singletonTypeId").toString().isEmpty()) {
            if (m_registry.singletonView(row.widget->property("singletonTypeId").toString()) == row.widget) { m_registry.parkSingleton(row.widget); }
            else { m_registry.returnBorrowedView(row.widget); }
        }
    }
    // aboutToDestroy synchronously removes poll targets while derived state exists.
    for (const auto& meter : std::as_const(m_meters)) {
        if (meter) { meter->hide(); meter->setParent(nullptr); delete meter.data(); }
    }
    m_meters.clear(); m_rows.clear();
    while (QLayoutItem* child = m_layout->takeAt(0)) {
        if (child->widget()) { delete child->widget(); }
        delete child;
    }
    m_materialized = false;
}
void ContainerContentHost::setBannerMenu(QMenu* menu)
{
    if (m_bannerMenu == menu) { return; }
    m_bannerMenu = menu;
    if (auto* existing=m_body->findChild<QPushButton*>(QStringLiteral("containerAppletsMenu"),Qt::FindDirectChildrenOnly)) { delete existing; }
    if (menu) {
        auto* button=new QPushButton(QStringLiteral("☰"),m_body); button->setObjectName(QStringLiteral("containerAppletsMenu"));
        button->setMenu(menu); button->setFixedHeight(22); m_layout->insertWidget(0,button);
    }
}
bool ContainerContentHost::needsReconcile(const ContainerDocument& document) const
{
    if (!m_materialized || document.contents.size()!=m_document.contents.size() || document.layout != m_document.layout) { return true; }
    for (int i=0;i<document.contents.size();++i) {
        const auto& entry=document.contents[i]; const auto& prior=m_document.contents[i];
        if (entry.id!=prior.id || entry.typeId!=prior.typeId) { return true; }
        // Singleton preference/source metadata can update its row independently.
        if (entry.typeId.startsWith("applet:")) { continue; }
        if (entry!=prior || effectiveContext(document,entry) != effectiveContext(m_document,prior)) { return true; }
        if (m_registry.isAvailable(entry.typeId) !=
            std::any_of(m_rows.cbegin(),m_rows.cend(),[&](const EntryRow& row){ return row.entryId==entry.id && row.effectiveVisible; }) && entry.visible) { return true; }
    }
    return false;
}
void ContainerContentHost::reconcile(const ContainerDocument& document)
{
    if (!needsReconcile(document)) {
        m_document = document;
        // Late singleton attachment/capability changes never reset meter history.
        for (int i=0;i<m_rows.size();++i) {
            auto& row = m_rows[i]; const auto& entry=document.contents[i];
            row.context=effectiveContext(document,entry);
            if (!entry.typeId.startsWith("applet:")) { continue; }
            QWidget* view=m_registry.singletonView(entry.typeId);
            const bool claim=m_registry.claimSingleton(entry.typeId,entry.id);
            row.effectiveVisible=entry.visible && m_registry.isAvailable(entry.typeId);
            if (view && claim && row.widget != view) {
                const int position=m_layout->indexOf(row.widget);
                if (row.widget) {
                    m_layout->removeWidget(row.widget);
                    if (!row.widget->property("singletonTypeId").toString().isEmpty()) { m_registry.returnBorrowedView(row.widget); }
                    else { delete row.widget.data(); }
                }
                view->setProperty("singletonTypeId",entry.typeId); view->setParent(m_body);
                view->setMinimumHeight(entry.typeId=="applet:s_meter"?130:0); view->setMaximumHeight(QWIDGETSIZE_MAX);
                view->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
                m_layout->insertWidget(position,view); row.widget=view;
            }
            if ((!view || !claim) && (!row.widget || !row.widget->property("singletonTypeId").toString().isEmpty())) {
                if (row.widget) { m_layout->removeWidget(row.widget); m_registry.returnBorrowedView(row.widget); }
                auto* label=new QLabel(m_body); label->setWordWrap(true);
                // Find the next surviving row; preserve ordered insertion boundaries.
                int position=m_layout->count()-1;
                for (int j=i+1;j<m_rows.size();++j) { if (m_rows[j].widget) { position=m_layout->indexOf(m_rows[j].widget); break; } }
                m_layout->insertWidget(qMax(0,position),label); row.widget=label;
            }
            if (auto* label=qobject_cast<QLabel*>(row.widget.data()); label && (!view || !claim)) {
                label->setText(entry.name + QStringLiteral(" — ") + (!claim?QStringLiteral("duplicate singleton placement; original data retained"):
                    (m_registry.isAvailable(entry.typeId)?QStringLiteral("waiting for its live view"):m_registry.unavailableReason(entry.typeId))));
            }
            if (row.widget) { row.widget->setVisible(view && claim ? row.effectiveVisible : entry.visible); }
        }
        emit reconciled(); return;
    }
    releaseViews(); m_document = document;
    if (m_bannerMenu) {
        auto* button = new QPushButton(QStringLiteral("☰"), m_body);
        button->setObjectName(QStringLiteral("containerAppletsMenu"));
        button->setMenu(m_bannerMenu); button->setFixedHeight(22);
        m_layout->addWidget(button);
    }
    MeterWidget* run = nullptr;
    QJsonObject runContext;
    QVector<int> runRows;
    int runHeight = 0;
    bool finalRun = false;
    const auto finishRun = [&] {
        if (run && run->items().isEmpty()) {
            // A retained malformed record gets an explanation, never an empty
            // native surface or a spurious registered polling target.
            m_meters.removeIf([&](const auto& meter) { return meter==run; });
            delete run; run=nullptr; runRows.clear(); runHeight=0; return;
        }
        if (!run || (document.layout == ContentLayout::LegacyCanvas && !finalRun)) { return; }
        if (document.layout == ContentLayout::VerticalStack) {
            for (int index : runRows) {
                auto& row = m_rows[index];
                row.item->clearStackMetadata();
                row.item->setRect(0, float(row.offset) / runHeight, 1, float(row.height) / runHeight);
            }
            run->setFixedHeight(runHeight);
        } else {
            run->setMinimumHeight(160);
        }
        if (document.layout == ContentLayout::LegacyCanvas) { m_layout->insertWidget(0,run); } else { m_layout->addWidget(run); }
        run->setProperty("containerSourceContext", runContext);
        emit meterSurfaceReady(run, runContext);
        run = nullptr; runRows.clear(); runHeight = 0;
    };
    for (const auto& entry : document.contents) {
        EntryRow row; row.entryId = entry.id; row.context = effectiveContext(document, entry);
        row.effectiveVisible = entry.visible && m_registry.isAvailable(entry.typeId);
        if (entry.typeId.startsWith("applet:")) {
            finishRun();
            QWidget* view = m_registry.singletonView(entry.typeId);
            const bool unique = m_registry.claimSingleton(entry.typeId, entry.id);
            if (view && unique) {
                view->setProperty("singletonTypeId", entry.typeId);
                view->setParent(m_body);
                view->setMinimumHeight(0); view->setMaximumHeight(QWIDGETSIZE_MAX);
                view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                if (entry.typeId == "applet:s_meter") { view->setMinimumHeight(130); }
                row.widget = view; m_layout->addWidget(view);
                view->setVisible(row.effectiveVisible);
            } else {
                auto* label = new QLabel(entry.name + QStringLiteral(" — ") +
                    (unique ? QStringLiteral("waiting for its live view") : QStringLiteral("duplicate singleton placement; original data retained")), m_body);
                label->setWordWrap(true); row.widget = label; m_layout->addWidget(label);
                label->setVisible(entry.visible);
            }
            m_rows.append(row); continue;
        }
        // A hidden/unavailable record is still an ordered insertion boundary.
        if (!row.effectiveVisible) {
            finishRun();
            auto* label = new QLabel(entry.name + QStringLiteral(" — ") + m_registry.unavailableReason(entry.typeId), m_body);
            label->setWordWrap(true); row.widget = label; m_layout->addWidget(label);
            label->setVisible(entry.visible); m_rows.append(row); continue;
        }
        if (run && document.layout == ContentLayout::VerticalStack && row.context != runContext) { finishRun(); }
        if (!run) {
            // A native sibling can promote children inside QWidget's base
            // constructor, before QRhiWidget selects its API. Configure the
            // leaf fully, then attach it to its final host before any paint.
            run = new MeterWidget(); run->setParent(m_body); runContext = row.context;
            m_meters.append(run);
        }
        MeterItem* item = m_registry.createMeterItem(entry, run);
        if (!item) {
            finishRun();
            auto* label = new QLabel(entry.name + QStringLiteral(" — unsupported configuration; original data retained"), this);
            row.widget = label; m_layout->addWidget(label); m_rows.append(row); continue;
        }
        if (document.layout == ContentLayout::LegacyCanvas && row.context != runContext) {
            // One canvas cannot be sampled as two contexts by a context-targeted
            // surface. Preserve its geometry/data and explain the unsupported
            // source instead of cross-feeding it from the first entry.
            item->setProperty("unsupportedSourceReason", QStringLiteral("This legacy canvas contains different slice sources; move this entry to a stack to read its source"));
            item->setProperty("containerUnsupportedSource",true);
        }
        item->setProperty("containerSourceContext", row.context);
        item->setProperty("containerEntryId", entry.id);
        int height = 72;
        if (auto* face = qobject_cast<BarPresetItem*>(item)) {
            height = face->preferredFaceHeight(); run->setMinimumWidth(qMax(run->minimumWidth(),face->minimumFaceSize().width()));
        }
        if (auto* face = qobject_cast<CompositePresetItem*>(item)) {
            height = face->preferredFaceHeight(); run->setMinimumWidth(qMax(run->minimumWidth(),face->minimumFaceSize().width()));
            for (MeterItem* child : face->internalItems()) {
                child->setProperty("containerSourceContext", row.context);
                child->setProperty("containerEntryId", entry.id);
                child->setProperty("containerUnsupportedSource",item->property("containerUnsupportedSource"));
                child->setProperty("unsupportedSourceReason",item->property("unsupportedSourceReason"));
            }
        }
        row.widget = run; row.item = item; row.offset = runHeight; row.height = height;
        runHeight += height; runRows.append(m_rows.size()); m_rows.append(row);
        run->addItem(item);
    }
    finalRun = true; finishRun(); m_layout->addStretch();
    m_generation = m_registry.generation(); m_materialized = true;
    int minimumWidth=0; for (auto* meter : meterSurfaces()) { minimumWidth=qMax(minimumWidth,meter->minimumWidth()); }
    setMinimumWidth(minimumWidth ? minimumWidth+8 : 0);
    emit reconciled();
}
QVector<MeterWidget*> ContainerContentHost::meterSurfaces() const
{
    QVector<MeterWidget*> result;
    for (const auto& meter : m_meters) { if (meter) { result.append(meter); } }
    return result;
}
QRect ContainerContentHost::entryBoundary(const QString& id) const
{
    for (const auto& row : m_rows) {
        if (row.entryId != id || !row.widget) { continue; }
        const QPoint origin = row.widget->mapTo(const_cast<ContainerContentHost*>(this), QPoint(0, 0));
        return row.item ? QRect(origin + QPoint(0, row.offset), QSize(row.widget->width(), row.height)) : QRect(origin, row.widget->size());
    }
    return {};
}
QJsonObject ContainerContentHost::sourceContext(const MeterWidget* meter) const
{
    return meter ? meter->property("containerSourceContext").toJsonObject() : QJsonObject();
}
ContainerDocument ContainerContentHost::captureDocument() const
{
    ContainerDocument document = m_document;
    for (auto& entry : document.contents) {
        for (const auto& row : m_rows) {
            if (entry.id != row.entryId || !row.item) { continue; }
            const QRectF canvas = entry.canvasRect;
            const QJsonObject oldContext = entry.context;
            const QJsonObject oldOverrides = entry.config.value("overrides").toObject();
            const QJsonObject oldProperties = entry.config.value("properties").toObject();
            entry = m_registry.captureMeterItem(*row.item, entry);
            if (document.layout == ContentLayout::VerticalStack) {
                entry.canvasRect = canvas;
                for (const QString& key : {QStringLiteral("stackSlot"),QStringLiteral("slotLocalY"),QStringLiteral("slotLocalH")}) {
                    if (oldContext.contains(key)) { entry.context[key]=oldContext[key]; } else { entry.context.remove(key); }
                }
                QJsonObject overrides=entry.config.value("overrides").toObject();
                for (const QString& key : {QStringLiteral("1"),QStringLiteral("2"),QStringLiteral("3"),QStringLiteral("4")}) {
                    if (oldOverrides.contains(key)) { overrides[key]=oldOverrides[key]; } else { overrides.remove(key); }
                }
                if (!overrides.isEmpty()) { entry.config["overrides"]=overrides; } else { entry.config.remove("overrides"); }
                QJsonObject properties = entry.config.value("properties").toObject();
                for (const QString& key : {QStringLiteral("x"),QStringLiteral("y"),QStringLiteral("w"),QStringLiteral("h")}) {
                    if (oldProperties.contains(key)) { properties[key] = oldProperties[key]; } else { properties.remove(key); }
                }
                if (!properties.isEmpty()) { entry.config["properties"] = properties; }
            }
        }
    }
    return document;
}
}
