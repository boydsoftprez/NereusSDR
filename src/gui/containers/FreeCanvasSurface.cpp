// no-port-check: NereusSDR-original native QWidget placement and geometry gestures.
// Modification history (NereusSDR):
//   2026-10-08 - Workspace preview only: the header's hide button hides the
//                 entry or container (Containers > Hidden brings it back).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-03 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-07 - Workspace preview only: each object's handle is an entry
//                 header inside its top edge and its resize corner sits
//                 inside its bounds, in place of the outside grip and
//                 corner. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - Workspace preview only: a hover updates only the handles
//                 whose reveal changed, in place of a full presentation
//                 sync on every mouse move. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the leaf header's pop-out button
//                 asks for the entry menu's Pop Out; it is enabled when the
//                 projected document is unlocked. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-09 - The minimum follows where the views are now: each
//                 presentation sync starts from the extent placeViews laid
//                 out, not from the last sync's, and grows by each view's
//                 widget geometry, not its native window's, which can lag
//                 the geometry set here. A view's earlier larger boundary
//                 no longer keeps the canvas larger, and the minimum no
//                 longer depends on when the native window settled.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - A view moved after placeViews placed it goes back, a few
//                 times at most. Under a scale factor below one a native
//                 child window rounds its geometry and reports it back, and
//                 a report made for an earlier parent could land after the
//                 placement, leaving the view 1 px off and the minimum with
//                 it, differently each launch. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
#include "FreeCanvasSurface.h"
#include "WorkspaceHeader.h"
#include "gui/styles/WorkspacePreview.h"
#include <QCursor>
#include <QPushButton>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QJsonValue>
#include <QStringList>
#include <QWindow>
#include <QTimer>
#include <algorithm>
#include <cmath>
namespace NereusSDR {
namespace {
constexpr int kGripWidth=18, kCornerSize=14, kMargin=20;
// How often a sync puts a moved view back after one placement. A native
// window whose rounded report never matches the placed position would
// otherwise be moved back on every report.
constexpr int kMaxPutBacks=3;
class CanvasHandle final : public QWidget {
public:
    CanvasHandle(bool corner,QWidget* parent):QWidget(parent),m_corner(corner) {
        setFixedSize(corner?QSize(kCornerSize,kCornerSize):QSize(kGripWidth,22));
        setFocusPolicy(Qt::StrongFocus);
        setCursor(corner?Qt::SizeFDiagCursor:Qt::OpenHandCursor);
        setToolTip(corner?tr("Resize object; arrow keys resize; Escape cancels"):tr("Move object; arrow keys move; right-click for container actions; Escape cancels"));
    }
protected:
    void paintEvent(QPaintEvent*) override {
        if(!property("canvasReveal").toBool() && !underMouse() && !hasFocus()) {return;}
        QPainter p(this);p.setPen(Qt::NoPen);p.setBrush(WorkspacePreview::roleColour(QColor("#00b4d8"), "accent"));
        if(m_corner) {p.drawPolygon(QPolygon{QPoint(2,12),QPoint(12,2),QPoint(12,12)});}
        else {for(int x:{5,11}) {for(int y:{5,11,17}) {p.drawEllipse(QPoint(x,y),1,1);}}}
    }
private:
    bool m_corner;
};
int pixels(double value) {return qRound(qBound(-double(QWIDGETSIZE_MAX/2),value,double(QWIDGETSIZE_MAX/2)));}
}
FreeCanvasSurface::FreeCanvasSurface(QWidget* parent):QWidget(parent) {
    setObjectName("freeCanvasSurface");setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);
}
void FreeCanvasSurface::clearViews() {
    if(!m_active.isEmpty()) {finishGesture(true);}
    for(const auto& leaf:std::as_const(m_leaves)) {
        if(leaf.grip) {delete leaf.grip.data();}
        if(leaf.corner) {delete leaf.corner.data();}
    }
    m_leaves.clear();
}
void FreeCanvasSurface::project(const ContainerDocument& document,const QHash<QString,QPointer<QWidget>>& views) {
    if(document.lockItemPositions && !m_active.isEmpty()) {finishGesture(true);}
    m_document=document;
    const auto ids=m_leaves.keys();
    for(const QString& id:ids) {
        if(!views.value(id)) {
            if(m_active==id) {finishGesture(true);}
            const auto leaf=m_leaves.take(id);
            if(leaf.grip) {delete leaf.grip.data();}if(leaf.corner) {delete leaf.corner.data();}
        }
    }
    double fallbackY=0;
    for(const auto& entry:document.contents) {
        QWidget* view=views.value(entry.id);
        if(!view) {continue;}
        if(!m_leaves.contains(entry.id)) {
            Leaf leaf;
            if(WorkspacePreview::enabled()) {
                // Workspace preview: the handle is a header inside the object.
                auto* header=new WorkspaceEntryHeader(entry.id,this);header->setOverlay(true);header->hide();
                const QString id=entry.id;
                connect(header,&WorkspaceEntryHeader::optionsClicked,this,[this,id,header]{
                    QPushButton* button=header->optionsButton();
                    emit entryContextMenuRequested(id,button->mapToGlobal(QPoint(0,button->height())));
                });
                connect(header,&WorkspaceEntryHeader::popOutClicked,this,[this,id]{emit entryPopOutRequested(id);});
                connect(header,&WorkspaceEntryHeader::hideClicked,this,[this,id]{emit entryHideRequested(id);});
                leaf.grip=header;leaf.corner=new WorkspaceCornerGrip(this);leaf.corner->hide();
            } else {leaf.grip=new CanvasHandle(false,this);leaf.corner=new CanvasHandle(true,this);}
            for(QWidget* handle:{leaf.grip.data(),leaf.corner.data()}) {
                handle->setProperty("canvasEntryId",entry.id);handle->installEventFilter(this);
            }
            leaf.grip->setObjectName("freeCanvasGrip_"+entry.id);leaf.corner->setObjectName("freeCanvasResize_"+entry.id);
            leaf.corner->setProperty("canvasResize",true);m_leaves.insert(entry.id,leaf);
        }
        auto& leaf=m_leaves[entry.id];leaf.view=view;leaf.order=entry.paintOrder;view->setProperty("freeCanvasEntryId",entry.id);
        if(auto* header=qobject_cast<WorkspaceEntryHeader*>(leaf.grip.data())) {
            header->setTitle(entry.name.isEmpty()?entry.id:entry.name);
            // The host projects locked when there is no arrange controller, as the entry menu is then empty.
            header->setActionsEnabled(!document.locked);
        }
        if(view->parentWidget()!=this) {view->setParent(this);}
        view->installEventFilter(this);
        const QSize hint=(view->property("freeCanvasHint").isValid()?view->property("freeCanvasHint").toSize():view->sizeHint()).expandedTo(QSize(320,80));
        if(m_active!=entry.id) {leaf.rect=entry.freeCanvasRect().value_or(QRectF(0,fallbackY,hint.width(),hint.height()));}
        fallbackY=leaf.rect.bottom()+kMargin;
        view->setVisible(entry.visible && (!view->property("freeCanvasEffectiveVisible").isValid() || view->property("freeCanvasEffectiveVisible").toBool()));
    }
    placeViews();updateReveal();
}
void FreeCanvasSurface::placeViews() {
    QRectF bounds(0,0,m_document.freeCanvasExtent().width(),m_document.freeCanvasExtent().height());
    for(const auto& leaf:std::as_const(m_leaves)) {bounds=bounds.united(leaf.rect);}
    if(m_active.isEmpty()) {m_origin=QPointF(kMargin-qMin(0.,bounds.left()),kMargin-qMin(0.,bounds.top()));}
    m_logicalExtent=QSizeF(qMax(40.,bounds.right()+m_origin.x()+kMargin),qMax(40.,bounds.bottom()+m_origin.y()+kMargin));
    m_extent=m_logicalExtent;
    setMinimumSize(pixels(m_extent.width()),pixels(m_extent.height()));
    auto ids=m_leaves.keys();
    std::sort(ids.begin(),ids.end(),[this](const QString& a,const QString& b){
        return m_leaves[a].order==m_leaves[b].order?a<b:m_leaves[a].order<m_leaves[b].order;
    });
    for(const QString& id:ids) {
        auto& leaf=m_leaves[id];if(!leaf.view) {continue;}
        const QRect rect(pixels(leaf.rect.x()+m_origin.x()),pixels(leaf.rect.y()+m_origin.y()),qMax(0,pixels(leaf.rect.width())),qMax(0,pixels(leaf.rect.height())));
        leaf.view->setGeometry(rect);
        leaf.placed=leaf.view->geometry();leaf.putBacks=0;
    }
    syncPresentation();queuePresentationSync();
}
QRect FreeCanvasSurface::projectedBoundary(QWidget* view) const {
    if(view->isVisibleTo(this)) {
        QWindow* window=view->windowHandle();QWidget* nativeParent=view->nativeParentWidget();
        if(window && window->isVisible() && nativeParent) {
            // A native child window reports geometry relative to its native parent, which can differ from the QWidget parent.
            const QRect geometry=window->geometry();return QRect(mapFrom(nativeParent,geometry.topLeft()),geometry.size());
        }
    }
    return view->geometry();
}
void FreeCanvasSurface::queuePresentationSync() {
    if(m_presentationSyncQueued) {return;}m_presentationSyncQueued=true;
    QTimer::singleShot(0,this,[this]{m_presentationSyncQueued=false;syncPresentation();});
}
void FreeCanvasSurface::syncPresentation() {
    // From the laid-out extent, not the last sync's: a boundary that was
    // larger earlier (a native window before its geometry settles) must not
    // keep the canvas larger once the view is back where it belongs.
    QSize extent(pixels(m_logicalExtent.width()),pixels(m_logicalExtent.height()));
    auto ids=m_leaves.keys();
    std::sort(ids.begin(),ids.end(),[this](const QString& a,const QString& b){return m_leaves[a].order==m_leaves[b].order?a<b:m_leaves[a].order<m_leaves[b].order;});
    for(const QString& id:ids) {
        auto& leaf=m_leaves[id];if(!leaf.view) {continue;}
        // The surface owns where its views are. Under a scale factor below
        // one a native child window rounds its geometry and reports it
        // back, and a report made for an earlier parent can land after the
        // placement and move the view a pixel off, by launch; it goes back.
        if(!leaf.placed.isNull() && leaf.view->pos()!=leaf.placed.topLeft() && leaf.putBacks<kMaxPutBacks) {
            ++leaf.putBacks;leaf.view->move(leaf.placed.topLeft());
        }
        const QRect rect=projectedBoundary(leaf.view);leaf.view->raise();
        // The extent grows by the widget's own geometry, which is set
        // synchronously here: the native window's can still be settling, so
        // reading it made the minimum depend on timing.
        const QRect placed=leaf.view->geometry();
        extent=extent.expandedTo(QSize(placed.x()+placed.width()+kMargin,placed.y()+placed.height()+kMargin));
        const bool visible=leaf.view->isVisibleTo(this) && leaf.rect.width()>0 && leaf.rect.height()>0;
        if(WorkspacePreview::enabled()) {
            // Workspace preview: header and corner inside the bounds, shown
            // while the object is hovered, selected or being edited.
            leaf.grip->setGeometry(rect.x(),rect.y(),rect.width(),WorkspaceEntryHeader::kHeight);
            leaf.corner->move(rect.x()+rect.width()-WorkspaceCornerGrip::kSize,rect.y()+rect.height()-WorkspaceCornerGrip::kSize);
            const bool revealed=visible && leaf.grip->property("canvasReveal").toBool();
            leaf.grip->setVisible(revealed);leaf.corner->setVisible(revealed);
        } else {
            leaf.grip->move(rect.x()-kGripWidth,rect.y());leaf.corner->move(rect.x()+rect.width(),rect.y()+rect.height());
            leaf.grip->setVisible(visible);leaf.corner->setVisible(visible);
        }
        leaf.grip->setCursor(m_document.lockItemPositions?Qt::ArrowCursor:Qt::OpenHandCursor);
        leaf.corner->setCursor(m_document.lockItemPositions?Qt::ArrowCursor:Qt::SizeFDiagCursor);
        leaf.grip->raise();leaf.corner->raise();
    }
    m_extent=QSizeF(extent);setMinimumSize(extent);
}
QRectF FreeCanvasSurface::logicalRect(const QString& id) const {return m_leaves.value(id).rect;}
QRect FreeCanvasSurface::entryBoundary(const QString& id) const {const auto leaf=m_leaves.value(id);return leaf.view?projectedBoundary(leaf.view):QRect();}
int FreeCanvasSurface::contentHeight() const {return pixels(m_extent.height());}
void FreeCanvasSurface::selectEntry(const QString& id) {m_selected=id;updateReveal();}
void FreeCanvasSurface::updateReveal(const QString& hover) {
    if(WorkspacePreview::enabled()) {
        // Workspace preview: only the handles whose reveal changed are
        // shown or hidden; a mouse move that changes nothing restacks
        // nothing. Placement stays with syncPresentation().
        for(auto it=m_leaves.begin();it!=m_leaves.end();++it) {
            const bool reveal=it.key()==m_selected || it.key()==hover || it.key()==m_active;
            QWidget* grip=it->grip.data();QWidget* corner=it->corner.data();
            if(!grip || !corner || grip->property("canvasReveal").toBool()==reveal) {continue;}
            const bool visible=it->view && it->view->isVisibleTo(this) && it->rect.width()>0 && it->rect.height()>0;
            for(QWidget* handle:{grip,corner}) {
                handle->setProperty("canvasReveal",reveal);handle->setVisible(visible && reveal);
                if(visible && reveal) {handle->raise();}
                handle->update();
            }
        }
        return;
    }
    for(auto it=m_leaves.begin();it!=m_leaves.end();++it) {
        for(QWidget* handle:{it->grip.data(),it->corner.data()}) {
            if(handle) {handle->setProperty("canvasReveal",it.key()==m_selected || it.key()==hover || it.key()==m_active);handle->update();}
        }
    }
}
QString FreeCanvasSurface::hoverAt(const QPoint& position) const {
    // Workspace preview handles sit inside the bounds, so no outside margin.
    const int left=WorkspacePreview::enabled()?0:-kGripWidth;
    const int extra=WorkspacePreview::enabled()?0:kCornerSize;
    auto ids=m_leaves.keys();std::sort(ids.begin(),ids.end(),[this](const QString& a,const QString& b){return m_leaves[a].order==m_leaves[b].order?a>b:m_leaves[a].order>m_leaves[b].order;});
    for(const QString& id:ids) {const auto& leaf=m_leaves[id];if(leaf.view && leaf.view->isVisibleTo(this) && projectedBoundary(leaf.view).adjusted(left,0,extra,extra).contains(position)) {return id;}}
    return {};
}
void FreeCanvasSurface::mouseMoveEvent(QMouseEvent* event) {
    updateReveal(hoverAt(event->position().toPoint()));QWidget::mouseMoveEvent(event);
}
void FreeCanvasSurface::changeRect(const QString& id,const QRectF& rect) {
    if(!m_leaves.contains(id)) {return;}m_leaves[id].rect=rect;for(auto& entry:m_document.contents) {if(entry.id==id) {entry.setFreeCanvasRect(rect);break;}}placeViews();emit geometryEdited(id,rect);
}
QStringList FreeCanvasSurface::moveGroupFor(const QString& id) const {
    QStringList group{id};
    QString partner;
    for(const auto& entry:m_document.contents) {
        if(entry.id==id) {partner=entry.extensions.value(QStringLiteral("moveWith")).toString();break;}
    }
    if(!partner.isEmpty()) {group.append(partner);}
    for(const auto& entry:m_document.contents) {
        if(entry.id==id || group.contains(entry.id)) {continue;}
        if(entry.extensions.value(QStringLiteral("moveWith")).toString()==id) {group.append(entry.id);}
    }
    return group;
}
void FreeCanvasSurface::finishGesture(bool cancel) {
    if(m_active.isEmpty()) {return;}
    if(cancel) {
        for(const QString& gid:m_group) {
            const QRectF original=m_groupOriginal.value(gid,logicalRect(gid));
            if(m_leaves.contains(gid)) {m_leaves[gid].rect=original;}
            for(auto& entry:m_document.contents) {
                if(entry.id!=gid) {continue;}
                if(m_groupGeometryPresent.value(gid)) {entry.extensions["freeCanvasRect"]=m_groupGeometry.value(gid);}
                else {entry.extensions.remove("freeCanvasRect");}
                break;
            }
            emit geometryRestored(gid,m_groupGeometry.value(gid),m_groupGeometryPresent.value(gid));
        }
    }
    if(m_capture) {m_capture->releaseMouse();}m_capture=nullptr;m_active.clear();placeViews();updateReveal();
    if(!cancel) {
        const QStringList group=m_group.isEmpty()?QStringList{}:m_group;
        for(const QString& gid:group) {
            const QRectF original=m_groupOriginal.value(gid,logicalRect(gid));
            if(logicalRect(gid)!=original) {emit geometryCommitted(gid,logicalRect(gid),original);}
        }
    }
    m_group.clear();m_groupOriginal.clear();m_groupGeometry.clear();m_groupGeometryPresent.clear();
}
bool FreeCanvasSurface::eventFilter(QObject* watched,QEvent* event) {
    auto* handle=qobject_cast<QWidget*>(watched);if(!handle) {return false;}
    const QString id=handle->property("canvasEntryId").toString();
    if(id.isEmpty()) {
        const QString leafId=handle->property("freeCanvasEntryId").toString();
        if(!leafId.isEmpty() && (event->type()==QEvent::Enter || event->type()==QEvent::FocusIn)) {updateReveal(leafId);}
        if(event->type()==QEvent::Leave) {
            // Workspace preview: moving onto the object's own header is not
            // leaving the object.
            updateReveal(WorkspacePreview::enabled()?hoverAt(mapFromGlobal(QCursor::pos())):QString());
        }
        if(!leafId.isEmpty() && (event->type()==QEvent::Show || event->type()==QEvent::Move || event->type()==QEvent::Resize)) {queuePresentationSync();}
        return false;
    }
    if(event->type()==QEvent::ContextMenu) {
        emit entryContextMenuRequested(id,static_cast<QContextMenuEvent*>(event)->globalPos());return true;
    }
    if(event->type()==QEvent::FocusIn || event->type()==QEvent::Enter) {updateReveal(id);}
    if(event->type()==QEvent::KeyPress) {
        auto* key=static_cast<QKeyEvent*>(event);
        if(key->key()==Qt::Key_Escape && !m_active.isEmpty()) {finishGesture(true);return true;}
        QPointF delta;const double step=key->modifiers().testFlag(Qt::ShiftModifier)?10.:1.;
        if(key->key()==Qt::Key_Left) {delta.setX(-step);}if(key->key()==Qt::Key_Right) {delta.setX(step);}
        if(key->key()==Qt::Key_Up) {delta.setY(-step);}if(key->key()==Qt::Key_Down) {delta.setY(step);}
        if(!delta.isNull()) {
            if(m_document.lockItemPositions) {return true;}
            const bool resizing=handle->property("canvasResize").toBool();
            for(const QString& gid:moveGroupFor(id)) {
                const QRectF original=logicalRect(gid);QRectF rect=original;
                if(resizing && gid==id) {
                    const QSizeF minimum=m_leaves[id].view->property("freeCanvasMinimum").toSizeF().expandedTo(QSizeF(24,24));
                    rect.setSize(QSizeF(qMax(minimum.width(),rect.width()+delta.x()),qMax(minimum.height(),rect.height()+delta.y())));
                } else {rect.translate(delta);}
                changeRect(gid,rect);emit geometryCommitted(gid,rect,original);
            }
            return true;
        }
    }
    if(event->type()==QEvent::MouseButtonPress) {
        auto* mouse=static_cast<QMouseEvent*>(event);
        if(mouse->button()==Qt::LeftButton) {
            selectEntry(id);emit entrySelected(id);
            if(m_document.lockItemPositions) {return true;}
            for(const auto& entry:m_document.contents) {if(entry.id==id) {m_originalGeometryPresent=entry.extensions.contains("freeCanvasRect");m_originalGeometry=entry.extensions.value("freeCanvasRect");break;}}
            m_active=id;m_resizing=handle->property("canvasResize").toBool();m_original=logicalRect(id);m_press=mouse->globalPosition();m_capture=handle;
            // Movement link: capture the partner(s) at the press so the drag
            // moves them by the same offset, keeping their own geometry.
            m_group=moveGroupFor(id);m_groupOriginal.clear();m_groupGeometry.clear();m_groupGeometryPresent.clear();
            for(const QString& gid:m_group) {
                m_groupOriginal[gid]=(gid==id)?m_original:logicalRect(gid);
                for(const auto& entry:m_document.contents) {
                    if(entry.id!=gid) {continue;}
                    m_groupGeometryPresent[gid]=entry.extensions.contains("freeCanvasRect");
                    m_groupGeometry[gid]=entry.extensions.value("freeCanvasRect");
                    break;
                }
            }
            // Consuming a child's press requires explicit capture: observed in
            // AetherSDR TitleBar.cpp:585-588 [@1e0718a], no upstream logic ported.
            handle->grabMouse();handle->setFocus();updateReveal();return true;
        }
    }
    if(event->type()==QEvent::MouseMove && m_active==id) {
        auto* mouse=static_cast<QMouseEvent*>(event);const QPointF delta=mouse->globalPosition()-m_press;
        for(const QString& gid:(m_group.isEmpty()?QStringList{id}:m_group)) {
            const QRectF original=m_groupOriginal.value(gid,logicalRect(gid));QRectF rect=original;
            if(m_resizing && gid==id) {
                const QSizeF minimum=m_leaves[id].view->property("freeCanvasMinimum").toSizeF().expandedTo(QSizeF(24,24));
                rect.setSize(QSizeF(qMax(minimum.width(),rect.width()+delta.x()),qMax(minimum.height(),rect.height()+delta.y())));
            } else {rect.translate(delta);}
            changeRect(gid,rect);
        }
        return true;
    }
    if(event->type()==QEvent::MouseButtonRelease && m_active==id) {finishGesture(false);return true;}
    return QWidget::eventFilter(watched,event);
}
}
