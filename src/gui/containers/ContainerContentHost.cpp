// no-port-check: NereusSDR-original mixed-content projection, no radio actions.
// Modification history (NereusSDR):
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "ContainerContentHost.h"
#include "ContainerArrangeController.h"
#include "ContainerWorkspaceStore.h"
#include "FreeCanvasSurface.h"
#include <QLoggingCategory>
Q_LOGGING_CATEGORY(lcFreeCanvas,"nereus.container.canvas")
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QDropEvent>
#include <QApplication>
#include <QPainter>
#include <QTimer>
#include <QSet>
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
#include <memory>
namespace NereusSDR {
ContainerContentHost::ContainerContentHost(ContainerContentRegistry& registry, QWidget* parent)
    : QWidget(parent), m_registry(registry)
{
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0);
    auto* scroll = new QScrollArea(this);m_scroll=scroll; scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_body = new QWidget(scroll); m_layout = new QVBoxLayout(m_body);
    scroll->setWidget(m_body); root->addWidget(scroll);
    m_layout->setContentsMargins(0, 0, 8, 0);
    scroll->viewport()->setAcceptDrops(true);
    scroll->viewport()->installEventFilter(this);
    m_body->installEventFilter(this);
    m_indicator=new QWidget(m_body);m_indicator->setObjectName(QStringLiteral("containerInsertionLine"));
    m_indicator->setStyleSheet(QStringLiteral("background:#00b4d8;"));m_indicator->setAttribute(Qt::WA_TransparentForMouseEvents);m_indicator->hide();
    m_layout->setSpacing(0);
    m_canvas=new FreeCanvasSurface(m_body);m_canvas->hide();
    connect(m_canvas,&FreeCanvasSurface::geometryCommitted,this,[this](const QString& id,const QRectF& rect,const QRectF& original){
        if(!m_arrange) {return;}
        const auto result=m_arrange->placeFreeCanvas(id,rect,original);
        if(!result.ok) {qCWarning(lcFreeCanvas)<<result.error;m_canvas->project(m_document,[this]{QHash<QString,QPointer<QWidget>> views;for(const auto& row:m_rows) {views[row.entryId]=row.widget;}return views;}());}
    });
    connect(m_canvas,&FreeCanvasSurface::entryContextMenuRequested,this,[this](const QString& id,const QPoint& position){QMenu menu(this);addEntryActions(menu,id);if(!menu.isEmpty()) {menu.exec(position);}});
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
        if (row.widget && !row.item && !row.widget->property("singletonTypeId").toString().isEmpty()) {
            if (m_registry.singletonView(row.widget->property("singletonTypeId").toString()) == row.widget) { m_registry.parkSingleton(row.widget); }
            else { m_registry.returnBorrowedView(row.widget); }
        }
    }
    // aboutToDestroy synchronously removes poll targets while derived state exists.
    for (const auto& meter : std::as_const(m_meters)) {
        if (meter) { meter->hide(); meter->setParent(nullptr); delete meter.data(); }
    }
    m_meters.clear();
    if(m_document.layout==ContentLayout::FreeCanvas) {
        for(const auto& row:std::as_const(m_rows)) {if(row.widget && row.widget->parentWidget()==m_canvas) {delete row.widget.data();}}
    }
    m_rows.clear();m_canvas->clearViews();m_canvas->hide();
    for(const auto& grip:m_grips) {if(grip) {delete grip.data();}} m_grips.clear();
    while (QLayoutItem* child = m_layout->takeAt(0)) {
        if (child->widget() && child->widget()!=m_canvas) { delete child->widget(); }
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
        ContentEntry comparable=entry,previous=prior;
        if(document.layout==ContentLayout::FreeCanvas) {
            comparable.extensions.remove("freeCanvasRect");previous.extensions.remove("freeCanvasRect");
            comparable.paintOrder=previous.paintOrder;comparable.visible=previous.visible;comparable.name=previous.name;
        }
        if (comparable!=previous || effectiveContext(document,entry) != effectiveContext(m_document,prior)) { return true; }
        if (m_registry.isAvailable(entry.typeId) !=
            std::any_of(m_rows.cbegin(),m_rows.cend(),[&](const EntryRow& row){ return row.entryId==entry.id && row.effectiveVisible; }) && entry.visible) { return true; }
    }
    return false;
}
void ContainerContentHost::reconcile(const ContainerDocument& document)
{
    m_scroll->setHorizontalScrollBarPolicy(document.layout==ContentLayout::FreeCanvas?Qt::ScrollBarAsNeeded:Qt::ScrollBarAlwaysOff);
    if(document.layout==ContentLayout::FreeCanvas) {reconcileFreeCanvas(document);return;}
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
        updateGrips();
        emit reconciled();
        return;
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
        if (document.layout == ContentLayout::LegacyCanvas) {
            auto items = run->items();
            std::sort(items.begin(), items.end(), [](MeterItem *a, MeterItem *b) {
                if (a->zOrder() != b->zOrder()) {
                    return a->zOrder() < b->zOrder();
                }
                return a->property("containerEntryId").toString() <
                       b->property("containerEntryId").toString();
            });
            run->replaceItems(items);
            m_layout->insertWidget(0, run);
        } else {
            m_layout->addWidget(run);
        }
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
    setMinimumWidth(minimumWidth ? minimumWidth + (m_arrange ? 28 : 8) : 0);
    updateGrips();
    emit reconciled();
}
void ContainerContentHost::reconcileFreeCanvas(const ContainerDocument& document)
{
    if(m_document.layout!=ContentLayout::FreeCanvas) {releaseViews();}
    const auto prior=m_document;const auto oldRows=m_rows;
    m_rows.clear();m_meters.clear();QSet<QWidget*> retained;
    QHash<QString,QPointer<QWidget>> views;
    m_document=document;setMinimumWidth(0);
    if(m_layout->indexOf(m_canvas)<0) {m_layout->addWidget(m_canvas);}
    m_canvas->show();
    for(const auto& entry:document.contents) {
        EntryRow row;row.entryId=entry.id;row.context=effectiveContext(document,entry);
        row.effectiveVisible=entry.visible && m_registry.isAvailable(entry.typeId);
        const EntryRow* old=nullptr;
        for(const auto& candidate:oldRows) {if(candidate.entryId==entry.id) {old=&candidate;break;}}
        if(entry.typeId.startsWith("applet:")) {
            QWidget* singleton=m_registry.singletonView(entry.typeId);const bool claim=m_registry.claimSingleton(entry.typeId,entry.id);
            if(singleton && claim) {
                singleton->setProperty("singletonTypeId",entry.typeId);singleton->setParent(m_canvas);
                const QSize minimum=singleton->minimumSizeHint().expandedTo(singleton->minimumSize()).expandedTo(QSize(24,24));
                singleton->setProperty("freeCanvasMinimum",QSizeF(minimum));singleton->setMinimumSize(0,0);singleton->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
                row.widget=singleton;
            } else {
                if(old && old->widget && old->widget->property("singletonTypeId").toString().isEmpty()) {row.widget=old->widget;}
                else {row.widget=new QLabel(m_canvas);}
                if(auto* label=qobject_cast<QLabel*>(row.widget.data())) {label->setWordWrap(true);label->setText(entry.name+QStringLiteral(" — ")+(claim?tr("waiting for its live view"):tr("duplicate singleton placement; original data retained")));}
            }
        } else if(row.effectiveVisible) {
            ContentEntry current=entry;current.extensions.remove("freeCanvasRect");current.paintOrder=0;
            for(auto previous:prior.contents) {
                previous.extensions.remove("freeCanvasRect");previous.paintOrder=0;
                if(old && old->item && previous==current && effectiveContext(prior,previous)==row.context) {row.widget=old->widget;row.item=old->item;break;}
            }
            if(!row.item) {
                auto meter=std::make_unique<MeterWidget>();
                if(auto* item=m_registry.createMeterItem(entry,meter.get())) {
                    item->clearStackMetadata();item->setRect(0,0,1,1);
                    item->setProperty("containerEntryId",entry.id);item->setProperty("containerSourceContext",row.context);
                    QSize minimum(24,24);
                    if(auto* face=qobject_cast<BarPresetItem*>(item)) {minimum=face->minimumFaceSize();}
                    if(auto* face=qobject_cast<CompositePresetItem*>(item)) {
                        minimum=face->minimumFaceSize();for(auto* child:face->internalItems()) {child->setProperty("containerSourceContext",row.context);child->setProperty("containerEntryId",entry.id);}
                    }
                    meter->setProperty("freeCanvasMinimum",QSizeF(minimum));meter->setProperty("containerSourceContext",row.context);
                    meter->addItem(item);meter->setParent(m_canvas);row.widget=meter.get();row.item=item;
                    emit meterSurfaceReady(meter.release(),row.context);
                }
            }
            if(row.item) {row.item->setZOrder(entry.paintOrder);}
            if(auto* meter=qobject_cast<MeterWidget*>(row.widget.data())) {m_meters.append(meter);}
        }
        if(!row.widget) {
            if(old && old->widget && !old->item && old->widget->property("singletonTypeId").toString().isEmpty()) {row.widget=old->widget;}
            else {row.widget=new QLabel(m_canvas);}
            if(auto* label=qobject_cast<QLabel*>(row.widget.data())) {label->setWordWrap(true);label->setText(entry.name+QStringLiteral(" — ")+m_registry.unavailableReason(entry.typeId));}
        }
        row.widget->setProperty("freeCanvasEffectiveVisible",!row.widget->property("singletonTypeId").toString().isEmpty()?row.effectiveVisible:entry.visible);
        retained.insert(row.widget);views[entry.id]=row.widget;m_rows.append(row);
    }
    for(const auto& row:oldRows) {
        if(!row.widget || retained.contains(row.widget)) {continue;}
        if(!row.widget->property("singletonTypeId").toString().isEmpty()) {m_registry.returnBorrowedView(row.widget);}
        else {delete row.widget.data();}
    }
    auto sceneDocument=document;sceneDocument.locked=document.locked || !m_arrange;
    m_canvas->project(sceneDocument,views);m_generation=m_registry.generation();m_materialized=true;emit reconciled();
}
QVector<MeterWidget*> ContainerContentHost::meterSurfaces() const
{
    QVector<MeterWidget*> result;
    for (const auto& meter : m_meters) { if (meter) { result.append(meter); } }
    return result;
}
QRect ContainerContentHost::entryBoundary(const QString& id) const
{
    if(m_document.layout==ContentLayout::FreeCanvas) {
        const QRect boundary=m_canvas->entryBoundary(id);
        return boundary.isNull()?QRect():QRect(m_canvas->mapTo(const_cast<ContainerContentHost*>(this),boundary.topLeft()),boundary.size());
    }
    for (const auto& row : m_rows) {
        if (row.entryId != id || !row.widget) { continue; }
        const QPoint origin = row.widget->mapTo(const_cast<ContainerContentHost*>(this), QPoint(0, 0));
        if (row.item && m_document.layout == ContentLayout::LegacyCanvas) {
            return QRectF(origin.x() + row.item->x() * row.widget->width(),
                          origin.y() + row.item->y() * row.widget->height(),
                          row.item->itemWidth() * row.widget->width(),
                          row.item->itemHeight() * row.widget->height())
                .toAlignedRect();
        }
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
            if (document.layout != ContentLayout::LegacyCanvas) {
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

namespace NereusSDR
{
namespace
{
class EntryGrip final : public QWidget
{
  public:
    explicit EntryGrip(QWidget *parent) : QWidget(parent)
    {
        setFixedSize(18, 22);
        setCursor(Qt::OpenHandCursor);
        setToolTip(QObject::tr("Drag to arrange; right-click for Move, Pop out and Return"));
    }

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setPen(Qt::NoPen);
        p.setBrush(underMouse() ? QColor("#00b4d8") : QColor("#8090a0"));
        for (int x : {5, 11}) {
            for (int y : {5, 11, 17}) {
                p.drawEllipse(QPoint(x, y), 1, 1);
            }
        }
    }
};
} // namespace
void ContainerContentHost::setArrangeController(ContainerArrangeController *controller)
{
    m_arrange = controller;
    m_layout->setContentsMargins(controller ? 20 : 0, 0, 8, 0);
}
void ContainerContentHost::updateGrips()
{
    if (!m_arrange || m_document.layout==ContentLayout::FreeCanvas) {
        return;
    }
    if (m_grips.size() != m_rows.size()) {
        for (const auto &grip : m_grips) {
            if (grip) {
                delete grip.data();
            }
        }
        m_grips.clear();
        for (const auto &row : m_rows) {
            auto *grip = new EntryGrip(m_body);
            grip->setObjectName(QStringLiteral("entryGrip_") + row.entryId);
            grip->setProperty("entryGripId", row.entryId);
            grip->installEventFilter(this);
            m_grips.append(grip);
            if (row.widget) {
                row.widget->installEventFilter(this);
            }
        }
    }
    for (int i = 0; i < m_rows.size(); ++i) {
        if (!m_grips[i]) {
            continue;
        }
        const QRect boundary = entryBoundary(m_rows[i].entryId);
        m_grips[i]->move(0, m_body->mapFrom(this, boundary.topLeft()).y());
        m_grips[i]->setVisible(m_rows[i].widget && m_rows[i].widget->isVisible() &&
                               boundary.height() > 0);
        m_grips[i]->raise();
        m_grips[i]->setCursor(m_document.locked ? Qt::ArrowCursor : Qt::OpenHandCursor);
    }
}
int ContainerContentHost::preferredContentHeight() const
{
    if(m_document.layout==ContentLayout::FreeCanvas) {return m_canvas->contentHeight()+(m_bannerMenu?22:0);}
    int height = 0;
    QSet<QWidget *> seen;
    for (const auto &row : m_rows) {
        if (!row.widget || !row.effectiveVisible || seen.contains(row.widget)) {
            continue;
        }
        seen.insert(row.widget);
        height += qBound(row.widget->minimumHeight(), row.widget->sizeHint().height(),
                         row.widget->maximumHeight());
    }
    return qMax(24, height + (m_bannerMenu ? 22 : 0));
}
QRect ContainerContentHost::gripGeometry(const QString &id) const
{
    if(m_document.layout==ContentLayout::FreeCanvas) {auto* grip=m_canvas->findChild<QWidget*>("freeCanvasGrip_"+id);return grip?QRect(grip->mapTo(const_cast<ContainerContentHost*>(this),QPoint()),grip->size()):QRect();}
    for (const auto &grip : m_grips) {
        if (grip && grip->property("entryGripId").toString() == id) {
            return QRect(grip->mapTo(const_cast<ContainerContentHost *>(this), QPoint()),
                         grip->size());
        }
    }
    return {};
}
int ContainerContentHost::insertionIndex(const QPoint &position) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        const QRect boundary = entryBoundary(m_rows[i].entryId);
        if (m_rows[i].widget && m_rows[i].widget->isVisible() &&
            position.y() < boundary.center().y()) {
            return i;
        }
    }
    return m_rows.size();
}
void ContainerContentHost::addEntryActions(QMenu &menu, const QString &id)
{
    if (!m_arrange) {
        return;
    }
    int index = -1;
    for (int i = 0; i < m_document.contents.size(); ++i) {
        if (m_document.contents[i].id == id) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        return;
    }
    const auto invoke = [](const ArrangeResult &result) {
        if (!result.ok) {
            qWarning() << result.error;
        }
    };
    auto *up = menu.addAction(tr("Move Up"), this, [this, id, index, invoke] {
        invoke(m_arrange->move(id, m_document.id, index - 1));
    });
    up->setEnabled(!m_document.locked && index > 0);
    auto *down = menu.addAction(tr("Move Down"), this, [this, id, index, invoke] {
        invoke(m_arrange->move(id, m_document.id, index + 2));
    });
    down->setEnabled(!m_document.locked && index + 1 < m_document.contents.size());
    auto *destinations = menu.addMenu(tr("Move to Container"));
    // The controller/store owns the latest destination catalog; the host carries
    // only committed presentation state and never serializes live projected rows.
    const auto workspace = m_arrange->workspaceSnapshot();
    for (const auto &c : workspace.containers) {
        if (c.id == m_document.id) {
            continue;
        }
        auto *action =
            destinations->addAction(c.name.isEmpty() ? c.id : c.name, this, [this, id, c, invoke] {
                invoke(m_arrange->move(id, c.id, c.contents.size()));
            });
        action->setEnabled(!m_document.locked && !c.locked);
    }
    auto *pop =
        menu.addAction(tr("Pop Out"), this, [this, id, invoke] { invoke(m_arrange->popOut(id)); });
    pop->setEnabled(!m_document.locked);
    auto *back = menu.addAction(tr("Return to remembered container"), this,
                                [this, id, invoke] { invoke(m_arrange->returnEntry(id)); });
    back->setEnabled(!m_document.locked);
    if (m_arrange->canDuplicate(m_document.contents[index])) {
        auto *duplicate = menu.addAction(tr("Duplicate meter"), this, [this, id, index, invoke] {
            invoke(m_arrange->duplicateEntry(id, m_document.id, index + 1));
        });
        duplicate->setEnabled(!m_document.locked);
    }
}
void ContainerContentHost::addContentsMenu(QMenu &menu)
{
    auto *contents = menu.addMenu(tr("Contents"));
    for (const auto &e : m_document.contents) {
        auto *entry = contents->addMenu(e.name.isEmpty() ? e.id : e.name);
        addEntryActions(*entry, e.id);
    }
}
bool ContainerContentHost::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_arrange) {
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest ||
        event->type() == QEvent::Show) {
        QTimer::singleShot(0, this, &ContainerContentHost::updateGrips);
    }
    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget) {
        return false;
    }
    const QString id = widget->property("entryGripId").toString();
    if (!id.isEmpty()) {
        if (event->type() == QEvent::ContextMenu) {
            auto *context = static_cast<QContextMenuEvent *>(event);
            QMenu menu(this);
            addEntryActions(menu, id);
            menu.exec(context->globalPos());
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && !m_document.locked) {
                m_dragStart = mouse->position().toPoint();
                m_pressedEntry = id;
                return true;
            }
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            m_pressedEntry.clear();
        }
        if (event->type() == QEvent::MouseMove && !m_pressedEntry.isEmpty()) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if ((mouse->buttons() & Qt::LeftButton) &&
                (mouse->position().toPoint() - m_dragStart).manhattanLength() >=
                    QApplication::startDragDistance()) {
                const QString entry = m_pressedEntry;
                m_pressedEntry.clear();
                QDrag drag(this);
                auto *mime = new QMimeData;
                mime->setData(ContainerArrangeController::kMimeType, m_arrange->mimeData(entry));
                drag.setMimeData(mime);
                drag.exec(Qt::MoveAction);
                // IgnoreAction includes Escape, invalid/blocked targets and outside
                // drops: cancellation never creates a pop-out or writes geometry.
                m_indicator->hide();
                return true;
            }
        }
    }
    if (event->type() == QEvent::DragLeave) {
        m_indicator->hide();
        return true;
    }
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove ||
        event->type() == QEvent::Drop) {
        auto *drop = static_cast<QDropEvent *>(event);
        const QByteArray data = drop->mimeData()->data(ContainerArrangeController::kMimeType);
        if (!m_arrange->validateDrop(data, m_document.id).ok) {
            drop->ignore();
            m_indicator->hide();
            return true;
        }
        const QPoint position = widget->mapTo(this, drop->position().toPoint());
        const int index = insertionIndex(position);
        if (event->type() == QEvent::Drop) {
            m_indicator->hide();
            const auto result = m_arrange->drop(data, m_document.id, index);
            if (result.ok) {
                drop->setDropAction(Qt::MoveAction);
                drop->accept();
            } else {
                drop->ignore();
            }
            return true;
        }
        int y = 0;
        if (index < m_rows.size()) {
            y = entryBoundary(m_rows[index].entryId).top();
        } else if (!m_rows.isEmpty()) {
            y = entryBoundary(m_rows.last().entryId).bottom() + 1;
        }
        m_indicator->setGeometry(0, m_body->mapFrom(this, QPoint(0, y)).y(), m_body->width(), 2);
        m_indicator->show();
        m_indicator->raise();
        drop->setDropAction(Qt::MoveAction);
        drop->accept();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
} // namespace NereusSDR
