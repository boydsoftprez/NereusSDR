// no-port-check: NereusSDR-original mixed-content projection, no radio actions.
// Modification history (NereusSDR):
//   2026-10-16 - An applet whose capability is off (Power Genius, Tuner
//                 Genius, RF-Kit, RADE) stays in its place grayed, with its
//                 reason, instead of being hidden: disabled, never hidden.
//                 Its entry's own visibility still shows or hides it.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: an auto-height stack scales its
//                 bars by the width without the vertical bar, so a container
//                 growing to fit them settles. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-10-11 - The container header mode is Shown or Hidden. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Workspace preview only: the entry header's options menu
//                 lives on the heap under the host too. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-09 - The entry menus live on the heap under the host
//                 (OwnedMenu.h): an entry action that removes the container
//                 while its menu is open no longer frees a menu on the
//                 stack. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-09 - Workspace preview only: an overlaid entry header sits below
//                 the container's overlaid group header (Arrange in Reveal
//                 and Hidden), which covered the first entry header's
//                 buttons. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - Workspace preview only: a factory widget's own config
//                 edits (ContentConfigNotifier) go to the workspace document
//                 through ContainerArrangeController::setEntryConfig, queued
//                 so the commit never runs inside the widget's menu; a widget
//                 that already shows the committed config is kept, not
//                 rebuilt. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - The host is never narrower than its contents: its minimum
//                 width counts the widest visible row, the body margins and
//                 the vertical bar, follows rows shown and hidden, and is
//                 announced to the container (today's look and the workspace
//                 preview). Workspace preview only: the body's 8 px right
//                 margin goes to the vertical bar while the bar shows.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the header's hide button hides the
//                 entry or container (Containers > Hidden brings it back).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the entry header's pop-out button
//                 runs the entry menu's Pop Out and is enabled when that
//                 item is. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - Workspace preview only: the insertion line lets a drop
//                 through as a native view (macOS). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: a free-canvas factory widget is not
//                 reused as an explanation label (gated on the switch).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Workspace preview only: per-instance widget content rows from
//                 ContainerContentRegistry widget factories, by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Workspace preview only: entry headers (in a reserved 16 px
//                 slot in Always, over the content in Arrange) replace the
//                 20 px left gutter and its grips by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-04 — Project native bar rows into responsive viewport allocations by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-04 — Fit stacked button groups to compact shared rows by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-04 — Guard integer boundary projection while preserving legacy float
//                 rounding and imports by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-03 — Usable independent native control sizing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-22  J.J. Boyd (KG4VCF): the arrange drag's QDrag is built
//               through ScopedChildWidget instead of on the stack, so a
//               host that dies during the drag no longer frees it twice.
//               With AI-assisted transformation via Claude Code.
#include "ContainerContentHost.h"
#include "gui/ScopedChildWidget.h"
#include "ContainerControlCatalog.h"
#include "ContainerArrangeController.h"
#include "ContainerWorkspaceStore.h"
#include "FreeCanvasSurface.h"
#include "WorkspaceHeader.h"
#include "gui/applets/AppletWidget.h"
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
#include "core/LogCategories.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/WaveformMeterItem.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/styles/WorkspaceStyle.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include "gui/containers/OwnedMenu.h"
#include <QStyle>
#include <QJsonArray>
#include <QScrollArea>
#include <QScrollBar>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
namespace NereusSDR {
namespace {
int preferredWidgetHeight(const QWidget* widget, int width)
{
    const int wrapped = widget->hasHeightForWidth() ? widget->heightForWidth(width) : -1;
    const int preferred = wrapped >= 0 ? wrapped : widget->sizeHint().height();
    return qBound(widget->minimumHeight(), preferred, widget->maximumHeight());
}
// Workspace preview: a factory widget whose own controls set its config
// (ContentConfigNotifier) already shows `current`; it may be kept when
// nothing but the config differs from `previous`.
bool showsConfigLive(const QWidget* widget, const ContentEntry& current, ContentEntry previous)
{
    if (!widget) { return false; }
    const QVariant applied = widget->property(ContentConfigNotifier::kAppliedConfigProperty);
    if (!applied.isValid() || applied.toJsonObject() != current.config) { return false; }
    previous.config = current.config;
    return previous == current;
}
// Disabled, never hidden: a live applet view whose capability is off is
// grayed with the registry's reason; available again, it is enabled. Only
// graying done here is undone, so a view its owner disabled stays so.
void presentAvailability(QWidget* view, const ContainerContentRegistry& registry, const QString& typeId)
{
    if (!view) { return; }
    const bool available = registry.isAvailable(typeId);
    if (auto* applet = qobject_cast<AppletWidget*>(view)) {
        applet->setUnavailableReason(available ? QString() : registry.unavailableReason(typeId));
        return;
    }
    static const char* const kGrayed = "containerGrayedUnavailable";
    // The view's own tooltip, kept while the reason stands in for it.
    static const char* const kOwnToolTip = "containerOwnToolTip";
    static const char* const kReasonToolTip = "containerReasonToolTip";
    if (!available) {
        const QString reason = registry.unavailableReason(typeId);
        if (!view->property(kReasonToolTip).isValid()) {
            view->setProperty(kOwnToolTip, view->toolTip());
        }
        view->setToolTip(reason);
        view->setProperty(kReasonToolTip, reason);
        if (!view->property(kGrayed).toBool() && !view->testAttribute(Qt::WA_ForceDisabled)) {
            view->setEnabled(false);
            view->setProperty(kGrayed, true);
        }
        return;
    }
    if (const QVariant reason = view->property(kReasonToolTip); reason.isValid()) {
        // Unless the view set a new one meanwhile.
        if (view->toolTip() == reason.toString()) {
            view->setToolTip(view->property(kOwnToolTip).toString());
        }
        view->setProperty(kReasonToolTip, QVariant());
        view->setProperty(kOwnToolTip, QVariant());
    }
    if (view->property(kGrayed).toBool()) {
        view->setProperty(kGrayed, false);
        view->setEnabled(true);
    }
}
} // namespace
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
    // Workspace preview: the body's right margin follows the vertical bar.
    if (WorkspacePreview::enabled()) { scroll->verticalScrollBar()->installEventFilter(this); }
    m_indicator=new QWidget(m_body);m_indicator->setObjectName(QStringLiteral("containerInsertionLine"));
    m_indicator->setStyleSheet(QStringLiteral("background:#00b4d8;"));m_indicator->setAttribute(Qt::WA_TransparentForMouseEvents);m_indicator->hide();
    m_layout->setSpacing(0);
    m_canvas=new FreeCanvasSurface(m_body);m_canvas->hide();
    connect(m_canvas,&FreeCanvasSurface::geometryCommitted,this,[this](const QString& id,const QRectF& rect,const QRectF& original){
        if(!m_arrange) {return;}
        const auto result=m_arrange->placeFreeCanvas(id,rect,original);
        if(!result.ok) {qCWarning(lcFreeCanvas)<<result.error;m_canvas->project(m_document,[this]{QHash<QString,QPointer<QWidget>> views;for(const auto& row:m_rows) {views[row.entryId]=row.widget;}return views;}());}
    });
    connect(m_canvas,&FreeCanvasSurface::entryContextMenuRequested,this,[this](const QString& id,const QPoint& position){OwnedMenu owned(this);QMenu& menu=owned.menu();addEntryActions(menu,id);if(!menu.isEmpty()) {menu.exec(position);}});
    connect(m_canvas,&FreeCanvasSurface::entryPopOutRequested,this,&ContainerContentHost::popOutEntry);
    connect(m_canvas,&FreeCanvasSurface::entryHideRequested,this,&ContainerContentHost::hideEntry);
    if (WorkspacePreview::enabled()) {
        // Workspace preview, explicit roles: the insertion line, the
        // applets banner button and Longpath's narrow scroll bar. The
        // background is the container's panel role (ContainerWidget's
        // content holder). The content itself goes through the scoped
        // pass after every reconcile.
        m_indicator->setStyleSheet(QStringLiteral("background:%1;")
                                       .arg(Workspace::role("accent", "#00b4d8")));
        // The insertion line is a native view on macOS while a drag runs;
        // the drop goes to the stack under it.
        WorkspacePreview::passInputThrough(m_indicator);
        // Longpath's scroll bar is 10 px thick; the preview keeps the
        // thickness the platform style gives the legacy bar, so the
        // content area does not change width.
        const int barPx = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
        QString scrollBars = QString::fromLatin1(Workspace::kScrollBarStyle);
        scrollBars.replace(QLatin1String("width: 10px; margin: 0;"),
                           QStringLiteral("width: %1px; margin: 0;").arg(barPx));
        scrollBars.replace(QLatin1String("height: 10px; margin: 0;"),
                           QStringLiteral("height: %1px; margin: 0;").arg(barPx));
        setStyleSheet(scrollBars
            + QStringLiteral(
                  "QPushButton#containerAppletsMenu {"
                  " background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2);"
                  " border: 1px solid %3; border-top-color: %7; border-bottom-color: %8;"
                  " border-radius: 4px; color: %4; }"
                  "QPushButton#containerAppletsMenu:hover { background: %5; color: %6; }"
                  "QPushButton#containerAppletsMenu::menu-indicator { image: none; width: 0; }")
                  .arg(Workspace::role("raised-top", "#222227"),
                       Workspace::role("raised-bottom", "#141417"),
                       Workspace::role("border", "#2c2c31"),
                       Workspace::role("text-secondary", "#a6a6ac"),
                       Workspace::role("button-hover", "#26262b"),
                       Workspace::role("text", "#dcdce1"),
                       Workspace::role("edge-light", "#38383e"),
                       Workspace::role("edge-dark", "#1f1f23")));
        connect(this, &ContainerContentHost::reconciled, this,
                [this]() { WorkspacePreview::themeSubtree(m_body); });
    }
}
ContainerContentHost::~ContainerContentHost() { releaseViews(); }
QJsonObject ContainerContentHost::effectiveContext(const ContainerDocument& document, const ContentEntry& entry)
{
    QJsonObject context;
    // Preserve identity axes; binding/stack/display fields are per-item, not source.
    for (const QString& key : {QStringLiteral("sessionId"), QStringLiteral("sliceId"), QStringLiteral("rxSource"), QStringLiteral("rxSourceMode")}) {
        if (document.config.contains(key)) { context[key] = document.config[key]; }
        if (entry.context.contains(key)) { context[key] = entry.context[key]; }
    }
    // Source choices override as one axis; session inheritance is independent.
    if (entry.context.value("rxSourceMode").toString() != QStringLiteral("followSelectedRx")
        && (entry.context.contains("sliceId") || entry.context.contains("rxSource"))) {
        context.remove("rxSourceMode");
    }
    if (context.value("rxSourceMode").toString() == QStringLiteral("followSelectedRx")) {
        context.remove("sliceId");
        context.remove("rxSource");
        return context;
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
    ++m_projectionEpoch;
    m_projectionPending = false;
    m_lastPreferredHeight = -1;
    m_materialized = false;
    m_body->setMinimumHeight(0);
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
    for (const auto& header : std::as_const(m_entryHeaders)) {
        if (header) { delete header.data(); }
    }
    m_entryHeaders.clear();
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
    scheduleStackProjection();
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
        if (comparable!=previous && m_registry.hasWidgetFactory(entry.typeId)) {
            // Workspace preview: the widget's own config edit, already shown.
            const auto row = std::find_if(m_rows.cbegin(), m_rows.cend(),
                                          [&](const EntryRow& r) { return r.entryId == entry.id; });
            if (row != m_rows.cend() && !row->item && showsConfigLive(row->widget, comparable, previous)) {
                previous = comparable;
            }
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
            // Unavailable, a claimed view is grayed, not hidden.
            row.presentationVisible = entry.visible;
            if (view && claim) { presentAvailability(view, m_registry, entry.typeId); }
            if (row.widget) { row.widget->setVisible(row.presentationVisible); }
        }
        // A row shown or hidden changes what the body needs across.
        updateContentMinimumWidth();
        scheduleStackProjection();
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
        row.presentationVisible = row.effectiveVisible;
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
                // Unavailable, it is grayed with its reason, not hidden.
                row.presentationVisible = entry.visible;
                presentAvailability(view, m_registry, entry.typeId);
                view->setVisible(entry.visible);
            } else {
                auto* label = new QLabel(entry.name + QStringLiteral(" — ") +
                    (unique ? QStringLiteral("waiting for its live view") : QStringLiteral("duplicate singleton placement; original data retained")), m_body);
                label->setWordWrap(true); row.widget = label; m_layout->addWidget(label);
                row.presentationVisible = entry.visible;
                label->setVisible(entry.visible);
            }
            m_rows.append(row); continue;
        }
        // A hidden/unavailable record is still an ordered insertion boundary.
        if (!row.effectiveVisible) {
            finishRun();
            auto* label = new QLabel(entry.name + QStringLiteral(" — ") + m_registry.unavailableReason(entry.typeId), m_body);
            label->setWordWrap(true); row.widget = label; m_layout->addWidget(label);
            row.presentationVisible = entry.visible;
            label->setVisible(entry.visible); m_rows.append(row); continue;
        }
        if (m_registry.hasWidgetFactory(entry.typeId)) {
            // Per-instance widget content (workspace preview): its own row,
            // like an applet, but created for this entry alone.
            ContentEntry effective = entry; effective.context = row.context;
            if (QWidget* widget = m_registry.createWidget(effective, m_body)) {
                finishRun();
                widget->setProperty("containerSourceContext", row.context);
                widget->setProperty(WorkspacePreview::kSkipProperty, true);
                connectConfigNotifier(widget, entry.id);
                row.widget = widget; m_layout->addWidget(widget);
                widget->setVisible(row.effectiveVisible);
                m_rows.append(row); continue;
            }
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
            row.widget = label; m_layout->addWidget(label); row.presentationVisible = entry.visible;
            label->setVisible(row.presentationVisible); m_rows.append(row); continue;
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
        int height = isSingleContainerControl(item) ? singleContainerControlSize().height() : 72;
        const QSize stackSize=document.layout==ContentLayout::VerticalStack ? containerStackButtonSize(item) : QSize();
        item->setProperty("containerStackGrid",!stackSize.isEmpty());
        if (!stackSize.isEmpty()) {
            height=stackSize.height();
            run->setMinimumWidth(qMax(run->minimumWidth(),containerStackButtonMinimum(item).width()));
        }
        if (auto* face = qobject_cast<WaveformMeterItem*>(item)) {
            height=face->preferredFaceHeight();run->setMinimumWidth(qMax(run->minimumWidth(),face->minimumFaceSize().width()));
        }
        if (auto* face = qobject_cast<BarPresetItem*>(item)) {
            height = face->preferredFaceHeight();
            // Stack bars follow the usable viewport width. Other layouts retain
            // the face minimum, and controls/composites keep their own minima.
            if (document.layout != ContentLayout::VerticalStack) {
                run->setMinimumWidth(qMax(run->minimumWidth(),face->minimumFaceSize().width()));
            }
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
        // Workspace preview, Always: each entry keeps a header slot above it.
        const int headerGap = entryHeaderReserve();
        row.widget = run; row.item = item; row.offset = runHeight + headerGap; row.height = height;
        row.baseHeight = height;
        runHeight += headerGap + height; runRows.append(m_rows.size()); m_rows.append(row);
        run->addItem(item);
    }
    finalRun = true; finishRun(); m_layout->addStretch();
    m_generation = m_registry.generation(); m_materialized = true;
    int minimumWidth=0; for (auto* meter : meterSurfaces()) { minimumWidth=qMax(minimumWidth,meter->minimumWidth()); }
    // Workspace preview: no left gutter, so Arrange adds no width.
    const int gutter = m_arrange && !WorkspacePreview::enabled() ? 28 : 8;
    m_surfaceMinimumWidth = minimumWidth ? minimumWidth + gutter : 0;
    setMinimumWidth(m_surfaceMinimumWidth);
    // Applet and widget rows and the vertical bar count too.
    updateContentMinimumWidth();
    scheduleStackProjection();
    updateGrips();
    emit reconciled();
}
void ContainerContentHost::reconcileFreeCanvas(const ContainerDocument& document)
{
    if(m_document.layout!=ContentLayout::FreeCanvas) {releaseViews();}
    const auto prior=m_document;const auto oldRows=m_rows;
    m_rows.clear();m_meters.clear();QSet<QWidget*> retained;
    QHash<QString,QPointer<QWidget>> views;
    m_document=document;m_surfaceMinimumWidth=0;setMinimumWidth(0);
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
                const QSize minimum=m_registry.singletonCanvasMinimum(entry.typeId);
                singleton->setProperty("freeCanvasHint",m_registry.singletonCanvasSizeHint(entry.typeId));
                singleton->setProperty("freeCanvasMinimum",QSizeF(minimum));singleton->setMinimumSize(0,0);singleton->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
                row.widget=singleton;
            } else {
                // Workspace preview: a factory widget (no item, no singleton) is
                // never reused as the explanation label. With the switch off the
                // old widget here is always that label.
                if(old && old->widget && old->widget->property("singletonTypeId").toString().isEmpty() && (!WorkspacePreview::enabled() || qobject_cast<QLabel*>(old->widget.data()))) {row.widget=old->widget;}
                else {row.widget=new QLabel(m_canvas);}
                if(auto* label=qobject_cast<QLabel*>(row.widget.data())) {label->setWordWrap(true);label->setText(entry.name+QStringLiteral(" — ")+(claim?tr("waiting for its live view"):tr("duplicate singleton placement; original data retained")));}
            }
        } else if(row.effectiveVisible && m_registry.hasWidgetFactory(entry.typeId)) {
            // Per-instance widget content (workspace preview).
            if(old && old->widget && !old->item && old->widget->property("containerFactoryType").toString()==entry.typeId) {
                ContentEntry current=entry,previous;current.extensions.remove("freeCanvasRect");current.paintOrder=0;
                for(const auto& candidate:prior.contents) {if(candidate.id==entry.id) {previous=candidate;previous.extensions.remove("freeCanvasRect");previous.paintOrder=0;break;}}
                previous.visible=current.visible;previous.name=current.name;
                if((previous==current || showsConfigLive(old->widget,current,previous)) && effectiveContext(prior,previous)==row.context) {row.widget=old->widget;}
            }
            if(!row.widget) {
                ContentEntry effective=entry;effective.context=row.context;
                if(QWidget* widget=m_registry.createWidget(effective,m_canvas)) {
                    widget->setProperty("containerSourceContext",row.context);
                    widget->setProperty(WorkspacePreview::kSkipProperty,true);
                    connectConfigNotifier(widget,entry.id);
                    widget->setProperty("freeCanvasMinimum",QSizeF(widget->minimumSizeHint()));
                    widget->setProperty("freeCanvasHint",widget->sizeHint());
                    widget->setMinimumSize(0,0);widget->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
                    row.widget=widget;
                }
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
                    QSize minimum = isSingleContainerControl(item) ? singleContainerControlMinimum() : QSize(24,24);
                    if(auto* face=qobject_cast<BarPresetItem*>(item)) {minimum=face->minimumFaceSize();}
                    if (auto* face = qobject_cast<WaveformMeterItem*>(item)) {
                        minimum = face->minimumFaceSize();
                    }
                    if(auto* face=qobject_cast<CompositePresetItem*>(item)) {
                        minimum=face->minimumFaceSize();for(auto* child:face->internalItems()) {child->setProperty("containerSourceContext",row.context);child->setProperty("containerEntryId",entry.id);}
                    }
                    // Exact imports may be smaller than edit minima, including zero.
                    meter->setMinimumSize(0,0);meter->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
                    meter->setProperty("freeCanvasMinimum",QSizeF(minimum));meter->setProperty("containerSourceContext",row.context);
                    meter->addItem(item);meter->setParent(m_canvas);row.widget=meter.get();row.item=item;
                    emit meterSurfaceReady(meter.release(),row.context);
                }
            }
            if(row.item) {row.item->setZOrder(entry.paintOrder);}
            if(auto* meter=qobject_cast<MeterWidget*>(row.widget.data())) {m_meters.append(meter);}
        }
        if(!row.widget) {
            // Workspace preview: as above, a factory widget is not the label.
            if(old && old->widget && !old->item && old->widget->property("singletonTypeId").toString().isEmpty() && (!WorkspacePreview::enabled() || qobject_cast<QLabel*>(old->widget.data()))) {row.widget=old->widget;}
            else {row.widget=new QLabel(m_canvas);}
            if(auto* label=qobject_cast<QLabel*>(row.widget.data())) {label->setWordWrap(true);label->setText(entry.name+QStringLiteral(" — ")+m_registry.unavailableReason(entry.typeId));}
        }
        // A singleton view is grayed when unavailable, never hidden for it.
        if(!row.widget->property("singletonTypeId").toString().isEmpty()) {presentAvailability(row.widget,m_registry,entry.typeId);}
        row.widget->setProperty("freeCanvasEffectiveVisible",entry.visible);
        retained.insert(row.widget);views[entry.id]=row.widget;m_rows.append(row);
    }
    for(const auto& row:oldRows) {
        if(!row.widget || retained.contains(row.widget)) {continue;}
        if(!row.widget->property("singletonTypeId").toString().isEmpty()) {m_registry.returnBorrowedView(row.widget);}
        else {delete row.widget.data();}
    }
    auto sceneDocument=document;sceneDocument.locked=document.locked || !m_arrange;
    m_canvas->project(sceneDocument,views);m_generation=m_registry.generation();m_materialized=true;
    updateContentMinimumWidth();
    emit reconciled();
}
void ContainerContentHost::connectConfigNotifier(QWidget* widget, const QString& entryId)
{
    auto* notifier = widget->findChild<ContentConfigNotifier*>(QString(), Qt::FindDirectChildrenOnly);
    if (!notifier) { return; }
    // Queued: the commit reconciles every host, which must not run inside
    // the widget's own menu. Several edits before the first delivery each
    // commit the widget's latest config (the applied-config property), so
    // an earlier one never rebuilds the widget away from what it shows.
    QPointer<QWidget> guard(widget);
    connect(notifier, &ContentConfigNotifier::configEdited, this, [this, entryId, guard](const QJsonObject& edited) {
        if (!m_arrange) {
            qCWarning(lcContainer) << "entry" << entryId << "config not saved: no arrange controller";
            return;
        }
        const QVariant applied = guard ? guard->property(ContentConfigNotifier::kAppliedConfigProperty) : QVariant();
        const QJsonObject config = applied.isValid() ? applied.toJsonObject() : edited;
        const ArrangeResult result = m_arrange->setEntryConfig(entryId, config);
        if (!result.ok) { qCWarning(lcContainer) << "entry" << entryId << "config not saved:" << result.error; }
    }, Qt::QueuedConnection);
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
            // Retain legacy float rounding: promoting the operands can move a
            // fractional edge by one pixel when toAlignedRect rounds outward.
            const float left = origin.x() + row.item->x() * row.widget->width();
            const float top = origin.y() + row.item->y() * row.widget->height();
            const float width = row.item->itemWidth() * row.widget->width();
            const float height = row.item->itemHeight() * row.widget->height();
            if (!std::isfinite(left) || !std::isfinite(top) ||
                !std::isfinite(width) || !std::isfinite(height)) { return {}; }
            const QRectF boundary(static_cast<qreal>(left), static_cast<qreal>(top),
                                  static_cast<qreal>(width), static_cast<qreal>(height));
            // Qt qrect.cpp toAlignedRect floors/ceils to int, subtracts the
            // endpoints, then QRect stores each exclusive far edge minus one.
            // Check that arithmetic in double before any integer conversion;
            // unrepresentable presentation must not rewrite the imported data.
            const double alignedLeft = std::floor(boundary.left());
            const double alignedTop = std::floor(boundary.top());
            const double alignedRight = std::ceil(boundary.right());
            const double alignedBottom = std::ceil(boundary.bottom());
            const auto fitsInt = [](double value) {
                return std::isfinite(value) && value >= std::numeric_limits<int>::min()
                    && value <= std::numeric_limits<int>::max();
            };
            const double alignedWidth = alignedRight - alignedLeft;
            const double alignedHeight = alignedBottom - alignedTop;
            if (!fitsInt(alignedLeft) || !fitsInt(alignedTop) ||
                !fitsInt(alignedRight) || !fitsInt(alignedBottom) ||
                !fitsInt(alignedWidth) || !fitsInt(alignedHeight) ||
                !fitsInt(alignedRight - 1) || !fitsInt(alignedBottom - 1) ||
                !fitsInt(alignedWidth - 1) || !fitsInt(alignedHeight - 1)) { return {}; }
            return boundary.toAlignedRect();
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
        p.setBrush(underMouse() ? WorkspacePreview::roleColour(QColor("#00b4d8"), "accent")
                                : WorkspacePreview::roleColour(QColor("#8090a0"), "text-scale"));
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
    // Workspace preview: entry headers replace the 20 px grip gutter.
    const int gutter = controller && !WorkspacePreview::enabled() ? 20 : 0;
    m_layout->setContentsMargins(gutter, 0, 8, 0);
    if (WorkspacePreview::enabled()) { applyPreviewRightMargin(); }
    // The gutter is part of what the contents need across.
    updateContentMinimumWidth();
    if (!controller) {
        for (const auto& grip : m_grips) {
            if (grip) { grip->hide(); }
        }
    }
    // Workspace preview: the entry pop-out buttons follow the controller.
    updateEntryHeaders();
    scheduleStackProjection();
}
int ContainerContentHost::verticalBarExtent() const
{
    // The width QAbstractScrollArea takes from the viewport for the bar
    // (its size hint), none when the bar is off or the style overlays it.
    QScrollBar *bar = m_scroll->verticalScrollBar();
    if (m_scroll->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff
        || bar->style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, bar)) {
        return 0;
    }
    return bar->sizeHint().width();
}
void ContainerContentHost::applyPreviewRightMargin()
{
    // Workspace preview: the 8 px right margin mirrors the legacy applet
    // panel's 8 px bar, which this scroll area does not use; while this
    // scroll area's own bar shows, the bar takes that place instead.
    if (!WorkspacePreview::enabled()) {
        return;
    }
    const bool barShown = !m_scroll->verticalScrollBar()->isHidden() && verticalBarExtent() > 0;
    QMargins margins = m_layout->contentsMargins();
    const int right = barShown ? 0 : kBodyRightMarginPx;
    if (margins.right() != right) {
        margins.setRight(right);
        m_layout->setContentsMargins(margins);
    }
}
void ContainerContentHost::updateContentMinimumWidth()
{
    // JJ's choice (2026-10-08): a container is never narrower than its
    // contents, in today's look and in the workspace preview. The body's
    // minimum is the widest visible row and the body margins (today's
    // 20 px grip gutter included); the vertical bar takes its extent from
    // the viewport whenever the rows scroll, so it counts as well.
    int minimum = 0;
    if (m_document.layout != ContentLayout::FreeCanvas) {
        minimum = m_surfaceMinimumWidth;
        const QMargins margins = m_layout->contentsMargins();
        const int body = m_body->minimumSizeHint().width();
        if (body > margins.left() + margins.right()) {
            const int bar = verticalBarExtent();
            // Workspace preview: the right margin and the bar share one place.
            const int need = WorkspacePreview::enabled()
                ? body - margins.right() + qMax(int(kBodyRightMarginPx), bar)
                : body + bar;
            minimum = qMax(minimum, need);
        }
    }
    if (minimumWidth() != minimum) {
        setMinimumWidth(minimum);
    }
    if (m_contentMinimumWidth != minimum) {
        m_contentMinimumWidth = minimum;
        emit contentMinimumWidthChanged(minimum);
    }
}
void ContainerContentHost::popOutEntry(const QString &entryId)
{
    // The entry menu's Pop Out, from the entry header's pop-out button.
    if (!m_arrange || m_document.locked) {
        return;
    }
    const ArrangeResult result = m_arrange->popOut(entryId);
    if (!result.ok) {
        qWarning() << result.error;
    }
}
void ContainerContentHost::hideEntry(const QString &entryId)
{
    // The entry header's hide button: the entry's saved visibility, the
    // state Containers > Applets also writes for an applet.
    if (!m_arrange || m_document.locked) {
        return;
    }
    const ArrangeResult result = m_arrange->setEntryVisible(entryId, false);
    if (!result.ok) {
        qWarning() << result.error;
    }
}
void ContainerContentHost::scheduleStackProjection()
{
    if (!m_materialized || m_document.layout != ContentLayout::VerticalStack || m_projectionPending) {
        return;
    }
    m_projectionPending = true;
    const quint64 epoch = m_projectionEpoch;
    QTimer::singleShot(0, this, [this, epoch] {
        if (epoch != m_projectionEpoch) { return; }
        m_projectionPending = false;
        projectStackRows();
    });
}
void ContainerContentHost::projectStackRows()
{
    if (!m_materialized || m_document.layout != ContentLayout::VerticalStack) { return; }
    // Same logical reference width as PresetGeometry's native bar face. Saved
    // rowHeight remains the calibration; only the allocated row is projected.
    constexpr int kBarReferenceWidth = 260;
    const QMargins margins = m_layout->contentsMargins();
    const int usableWidth = qMax(0, m_scroll->viewport()->width() - margins.left() - margins.right());
    if (usableWidth <= 0) { return; }
    double responsiveBaseHeight = 0;
    double fixedHeight = margins.top() + margins.bottom() + (m_bannerMenu ? 22 : 0);
    const int headerGap = entryHeaderReserve();
    QSet<QWidget*> fixedWidgets;
    for (const auto& row : std::as_const(m_rows)) {
        if (!row.widget || !row.presentationVisible) { continue; }
        fixedHeight += headerGap;
        if (row.item) {
            if (qobject_cast<BarPresetItem*>(row.item.data())) {
                responsiveBaseHeight += row.baseHeight;
            } else {
                fixedHeight += row.baseHeight;
            }
        } else if (!fixedWidgets.contains(row.widget)) {
            fixedWidgets.insert(row.widget);
            fixedHeight += preferredWidgetHeight(row.widget, usableWidth);
        }
    }
    // Workspace preview: header slots still need placing without bar rows.
    if (responsiveBaseHeight <= 0 && headerGap == 0) { return; }
    double scale = 1.0;
    if (responsiveBaseHeight > 0) {
        // Workspace preview, auto height: the bars scale by the width the
        // rows have with no vertical bar. A container growing to fit its
        // bars is too short for a moment and the bar shows; were the bars
        // then scaled to the narrower rows they would need less height, the
        // bar would go, the rows would widen and the bar would show again,
        // without end, and the container would stay short of its content.
        int scaleWidth = usableWidth;
        if (WorkspacePreview::enabled() && m_document.autoHeight) {
            // What the bar takes from the viewport; nothing when it is
            // hidden or the style lays it over the content.
            scaleWidth += qMax(0, m_scroll->maximumViewportSize().width() - m_scroll->viewport()->width());
        }
        const double widthRatio = double(scaleWidth) / kBarReferenceWidth;
        const double availableRatio = qMax(0.0, m_scroll->viewport()->height() - fixedHeight) / responsiveBaseHeight;
        const double desiredScale = m_document.autoHeight ? widthRatio : qMin(widthRatio, qMax(1.0, availableRatio));
        // Bound presentation arithmetic to Qt's representable widget extent even
        // when an imported calibration is huge; the stored value remains intact.
        scale = qMin(desiredScale, double(QWIDGETSIZE_MAX) / responsiveBaseHeight);
    }
    // Carry fractional rounding across all runs, so their combined allocation
    // fits the available height without one rounding pixel per row/run causing
    // a scrollbar to appear, shrink the width, then disappear again.
    double responsiveOffset = 0;
    int responsiveBottom = 0;
    for (MeterWidget* meter : meterSurfaces()) {
        int runHeight = 0;
        QVector<EntryRow*> runRows;
        bool changed = false;
        for (auto& row : m_rows) {
            if (row.widget != meter || !row.item) { continue; }
            int height = row.baseHeight;
            if (qobject_cast<BarPresetItem*>(row.item.data())) {
                responsiveOffset += row.baseHeight * scale;
                const int bottom = qRound(responsiveOffset);
                height = qMax(1, bottom - responsiveBottom);
                responsiveBottom += height;
            }
            changed = changed || row.offset != runHeight + headerGap || row.height != height;
            row.offset = runHeight + headerGap;
            row.height = height;
            runHeight += headerGap + height;
            runRows.append(&row);
        }
        if (runHeight <= 0) { continue; }
        if (changed || meter->minimumHeight() != runHeight || meter->maximumHeight() != runHeight) {
            for (EntryRow* row : runRows) {
                row->item->setRect(0, float(row->offset) / runHeight, 1, float(row->height) / runHeight);
            }
            meter->setFixedHeight(runHeight);
            meter->invalidateGeometry();
        }
    }
    // QScrollArea otherwise chooses the layout's minimum, which can reduce a
    // borrowed Preferred widget or word-wrapped placeholder to zero height in
    // a scrolling body. Reserve their measured hints without changing the views.
    m_body->setMinimumHeight(qRound(qMin(double(QWIDGETSIZE_MAX), fixedHeight + responsiveBottom)));
    updateGrips();
    const int preferred = preferredContentHeight();
    if (preferred != m_lastPreferredHeight) {
        m_lastPreferredHeight = preferred;
        emit preferredContentHeightChanged();
    }
}
void ContainerContentHost::updateGrips()
{
    if (WorkspacePreview::enabled()) {
        updateEntryHeaders();
        return;
    }
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
    const QMargins margins = m_layout->contentsMargins();
    const int usableWidth = qMax(0, m_scroll->viewport()->width() - margins.left() - margins.right());
    QSet<QWidget *> seen;
    for (const auto &row : m_rows) {
        const bool visible = m_document.layout == ContentLayout::VerticalStack ? row.presentationVisible : row.effectiveVisible;
        if (!row.widget || !visible || seen.contains(row.widget)) {
            continue;
        }
        seen.insert(row.widget);
        if (!row.item) {
            // Workspace preview, Always: the header slot above a widget row
            // (meter runs carry theirs inside their fixed height).
            height += entryHeaderReserve();
        }
        height += m_document.layout == ContentLayout::VerticalStack ? preferredWidgetHeight(row.widget, usableWidth)
            : qBound(row.widget->minimumHeight(), row.widget->sizeHint().height(), row.widget->maximumHeight());
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
    if (auto *header = entryHeader(id); header && header->isVisible()) {
        return QRect(header->mapTo(const_cast<ContainerContentHost *>(this), QPoint()), header->size());
    }
    return {};
}
void ContainerContentHost::setArrangeActive(bool active)
{
    if (m_arrangeActive == active) {
        return;
    }
    m_arrangeActive = active;
    updateEntryHeaders();
}
void ContainerContentHost::setTopOverlayInset(int px)
{
    const int inset = qMax(0, px);
    if (m_topOverlayInset == inset) {
        return;
    }
    m_topOverlayInset = inset;
    updateEntryHeaders();
}
void ContainerContentHost::setContainerHeaderMode(HeaderMode mode)
{
    if (m_containerHeaderMode == mode) {
        return;
    }
    m_containerHeaderMode = mode;
    if (!WorkspacePreview::enabled()) {
        return;
    }
    updateEntryHeaders();
    scheduleStackProjection();
}
WorkspaceEntryHeader *ContainerContentHost::entryHeader(const QString &entryId) const
{
    return m_entryHeaders.value(entryId).data();
}
int ContainerContentHost::entryHeaderReserve() const
{
    // Workspace preview, header shown: a multi-content stack keeps a 16 px
    // slot above each entry. Hidden shows the headers over the content.
    const bool reserve = WorkspacePreview::enabled() && m_document.layout == ContentLayout::VerticalStack
        && m_document.contents.size() > 1 && m_containerHeaderMode == HeaderMode::Shown;
    return reserve ? WorkspaceEntryHeader::kHeight : 0;
}
QString ContainerContentHost::entryTitle(const ContentEntry &entry) const
{
    if (!entry.name.isEmpty()) {
        return entry.name;
    }
    for (const auto &descriptor : m_registry.descriptors()) {
        if (descriptor.typeId == entry.typeId) {
            return descriptor.title;
        }
    }
    return entry.id;
}
void ContainerContentHost::updateEntryHeaders()
{
    if (!WorkspacePreview::enabled()) {
        return;
    }
    // A single entry's title is the container's header, so it never gets a
    // second bar. Free canvas leaves carry their own headers.
    const bool shown = m_materialized && m_document.layout != ContentLayout::FreeCanvas
        && m_document.contents.size() > 1
        && (m_containerHeaderMode == HeaderMode::Shown || m_arrangeActive);
    const bool reserve = shown && entryHeaderReserve() > 0;
    const QMargins margins = m_layout->contentsMargins();
    QSet<QString> live;
    for (int i = 0; i < m_rows.size(); ++i) {
        const auto &row = m_rows[i];
        live.insert(row.entryId);
        const bool visible = shown && row.widget
            && (m_document.layout == ContentLayout::VerticalStack ? row.presentationVisible : row.effectiveVisible);
        WorkspaceEntryHeader *header = entryHeader(row.entryId);
        if (!header && !visible) {
            continue;
        }
        if (!header) {
            header = new WorkspaceEntryHeader(row.entryId, m_body);
            header->setProperty("entryGripId", row.entryId);
            header->installEventFilter(this);
            const QString id = row.entryId;
            connect(header, &WorkspaceEntryHeader::popOutClicked, this,
                    [this, id] { popOutEntry(id); });
            connect(header, &WorkspaceEntryHeader::hideClicked, this,
                    [this, id] { hideEntry(id); });
            connect(header, &WorkspaceEntryHeader::optionsClicked, this, [this, id] {
                auto *source = entryHeader(id);
                if (!source) {
                    return;
                }
                OwnedMenu owned(this);
                QMenu &menu = owned.menu();
                addEntryActions(menu, id);
                if (!menu.isEmpty()) {
                    QPushButton *button = source->optionsButton();
                    menu.exec(button->mapToGlobal(QPoint(0, button->height())));
                }
            });
            m_entryHeaders.insert(row.entryId, header);
        }
        if (row.widget && !row.widget->property("workspaceHeaderWatched").toBool()) {
            row.widget->setProperty("workspaceHeaderWatched", true);
            row.widget->installEventFilter(this);
        }
        if (i < m_document.contents.size() && m_document.contents[i].id == row.entryId) {
            header->setTitle(entryTitle(m_document.contents[i]));
        }
        header->setCursor(m_document.locked ? Qt::ArrowCursor : Qt::OpenHandCursor);
        // Pop out and hide: enabled exactly when the entry menu's Pop Out is.
        header->setActionsEnabled(m_arrange && !m_document.locked);
        const bool inLayout = visible && reserve && !row.item;
        const int position = m_layout->indexOf(header);
        if (inLayout) {
            const int target = m_layout->indexOf(row.widget);
            if (position < 0 || position + 1 != target) {
                if (position >= 0) {
                    m_layout->removeWidget(header);
                }
                m_layout->insertWidget(qMax(0, m_layout->indexOf(row.widget)), header);
            }
            header->setOverlay(false);
            header->setVisible(true);
            continue;
        }
        if (position >= 0) {
            m_layout->removeWidget(header);
        }
        header->setVisible(visible);
        if (!visible) {
            continue;
        }
        const QRect boundary = entryBoundary(row.entryId);
        int top = m_body->mapFrom(this, boundary.topLeft()).y() - (reserve ? WorkspaceEntryHeader::kHeight : 0);
        if (!reserve && m_topOverlayInset > 0) {
            // Below the container's overlaid group header, which would
            // otherwise cover this header's buttons.
            top = qMax(top, m_body->mapFrom(this, QPoint(0, m_topOverlayInset)).y());
        }
        const QRect target(margins.left(), top, qMax(0, m_body->width() - margins.left() - margins.right()),
                           WorkspaceEntryHeader::kHeight);
        if (header->geometry() != target) {
            header->setGeometry(target);
        }
        header->setOverlay(!reserve);
        header->raise();
    }
    for (auto it = m_entryHeaders.begin(); it != m_entryHeaders.end();) {
        if (!it.value() || !live.contains(it.key())) {
            if (it.value()) {
                delete it.value().data();
            }
            it = m_entryHeaders.erase(it);
        } else {
            ++it;
        }
    }
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
    if (watched == m_scroll->verticalScrollBar()) {
        // Workspace preview: the bar showing or hiding moves the body's
        // right margin.
        if (event->type() == QEvent::ShowToParent || event->type() == QEvent::HideToParent) {
            applyPreviewRightMargin();
        }
        return QWidget::eventFilter(watched, event);
    }
    if (watched == m_body && event->type() == QEvent::LayoutRequest) {
        // A row's minimum, or the rows shown, changed.
        updateContentMinimumWidth();
    }
    if (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest ||
        event->type() == QEvent::Show) {
        scheduleStackProjection();
    }
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
            OwnedMenu owned(this);
            QMenu &menu = owned.menu();
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
                // Owned by ScopedChildWidget: freed once even if the parent dies in exec().
                ScopedChildWidget<QDrag> dragOwner(this);
                QDrag& drag = *dragOwner.get();
                auto *mime = new QMimeData;
                mime->setData(ContainerArrangeController::kMimeType, m_arrange->mimeData(entry));
                drag.setMimeData(mime);
                drag.exec(Qt::MoveAction);
                // The parent may have died in exec() and taken the drag with it.
                if (!dragOwner) { return true; }
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
