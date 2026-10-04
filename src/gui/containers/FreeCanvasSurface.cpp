// no-port-check: NereusSDR-original native QWidget placement and geometry gestures.
// Modification history (NereusSDR):
//   2026-10-03 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "FreeCanvasSurface.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <algorithm>
#include <cmath>
namespace NereusSDR {
namespace {
constexpr int kGripWidth=18, kCornerSize=14, kMargin=20;
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
        QPainter p(this);p.setPen(Qt::NoPen);p.setBrush(QColor("#00b4d8"));
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
    if(document.locked && !m_active.isEmpty()) {finishGesture(true);}
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
            Leaf leaf;leaf.grip=new CanvasHandle(false,this);leaf.corner=new CanvasHandle(true,this);
            for(QWidget* handle:{leaf.grip.data(),leaf.corner.data()}) {
                handle->setProperty("canvasEntryId",entry.id);handle->installEventFilter(this);
            }
            leaf.grip->setObjectName("freeCanvasGrip_"+entry.id);leaf.corner->setObjectName("freeCanvasResize_"+entry.id);
            leaf.corner->setProperty("canvasResize",true);m_leaves.insert(entry.id,leaf);
        }
        auto& leaf=m_leaves[entry.id];leaf.view=view;leaf.order=entry.paintOrder;view->setProperty("freeCanvasEntryId",entry.id);
        if(view->parentWidget()!=this) {view->setParent(this);}
        view->installEventFilter(this);
        const QSize hint=view->sizeHint().expandedTo(QSize(320,80));
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
    m_extent=QSizeF(qMax(40.,bounds.right()+m_origin.x()+kMargin),qMax(40.,bounds.bottom()+m_origin.y()+kMargin));
    setMinimumSize(pixels(m_extent.width()),pixels(m_extent.height()));
    auto ids=m_leaves.keys();
    std::sort(ids.begin(),ids.end(),[this](const QString& a,const QString& b){
        return m_leaves[a].order==m_leaves[b].order?a<b:m_leaves[a].order<m_leaves[b].order;
    });
    for(const QString& id:ids) {
        auto& leaf=m_leaves[id];if(!leaf.view) {continue;}
        const QRect rect(pixels(leaf.rect.x()+m_origin.x()),pixels(leaf.rect.y()+m_origin.y()),qMax(0,pixels(leaf.rect.width())),qMax(0,pixels(leaf.rect.height())));
        leaf.view->setGeometry(rect);leaf.view->raise();
        leaf.grip->move(rect.x()-kGripWidth,rect.y());leaf.corner->move(rect.x()+rect.width(),rect.y()+rect.height());
        const bool visible=leaf.view->isVisibleTo(this) && leaf.rect.width()>0 && leaf.rect.height()>0;
        leaf.grip->setVisible(visible);leaf.corner->setVisible(visible);
        leaf.grip->setCursor(m_document.locked?Qt::ArrowCursor:Qt::OpenHandCursor);
        leaf.corner->setCursor(m_document.locked?Qt::ArrowCursor:Qt::SizeFDiagCursor);
        leaf.grip->raise();leaf.corner->raise();
    }
}
QRectF FreeCanvasSurface::logicalRect(const QString& id) const {return m_leaves.value(id).rect;}
QRect FreeCanvasSurface::entryBoundary(const QString& id) const {const auto leaf=m_leaves.value(id);return leaf.view?leaf.view->geometry():QRect();}
int FreeCanvasSurface::contentHeight() const {return pixels(m_extent.height());}
void FreeCanvasSurface::selectEntry(const QString& id) {m_selected=id;updateReveal();}
void FreeCanvasSurface::updateReveal(const QString& hover) {
    for(auto it=m_leaves.begin();it!=m_leaves.end();++it) {
        for(QWidget* handle:{it->grip.data(),it->corner.data()}) {
            if(handle) {handle->setProperty("canvasReveal",it.key()==m_selected || it.key()==hover || it.key()==m_active);handle->update();}
        }
    }
}
void FreeCanvasSurface::mouseMoveEvent(QMouseEvent* event) {
    QString hover;
    auto ids=m_leaves.keys();std::sort(ids.begin(),ids.end(),[this](const QString& a,const QString& b){return m_leaves[a].order==m_leaves[b].order?a>b:m_leaves[a].order>m_leaves[b].order;});
    for(const QString& id:ids) {const auto& leaf=m_leaves[id];if(leaf.view && leaf.view->isVisibleTo(this) && leaf.view->geometry().adjusted(-kGripWidth,0,kCornerSize,kCornerSize).contains(event->position().toPoint())) {hover=id;break;}}
    updateReveal(hover);QWidget::mouseMoveEvent(event);
}
void FreeCanvasSurface::changeRect(const QString& id,const QRectF& rect) {
    if(!m_leaves.contains(id)) {return;}m_leaves[id].rect=rect;placeViews();emit geometryEdited(id,rect);
}
void FreeCanvasSurface::finishGesture(bool cancel) {
    if(m_active.isEmpty()) {return;}
    const QString id=m_active;const QRectF original=m_original;
    if(cancel) {changeRect(id,original);}
    if(m_capture) {m_capture->releaseMouse();}m_capture=nullptr;m_active.clear();placeViews();updateReveal();
    if(!cancel && logicalRect(id)!=original) {emit geometryCommitted(id,logicalRect(id),original);}
}
bool FreeCanvasSurface::eventFilter(QObject* watched,QEvent* event) {
    auto* handle=qobject_cast<QWidget*>(watched);if(!handle) {return false;}
    const QString id=handle->property("canvasEntryId").toString();
    if(id.isEmpty()) {
        const QString leafId=handle->property("freeCanvasEntryId").toString();
        if(!leafId.isEmpty() && (event->type()==QEvent::Enter || event->type()==QEvent::FocusIn)) {updateReveal(leafId);}
        if(event->type()==QEvent::Leave) {updateReveal();}
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
            if(m_document.locked) {return true;}
            const QRectF original=logicalRect(id);QRectF rect=original;
            if(handle->property("canvasResize").toBool()) {
                const QSizeF minimum=m_leaves[id].view->property("freeCanvasMinimum").toSizeF().expandedTo(QSizeF(24,24));
                rect.setSize(QSizeF(qMax(minimum.width(),rect.width()+delta.x()),qMax(minimum.height(),rect.height()+delta.y())));
            } else {rect.translate(delta);}
            changeRect(id,rect);emit geometryCommitted(id,rect,original);return true;
        }
    }
    if(event->type()==QEvent::MouseButtonPress) {
        auto* mouse=static_cast<QMouseEvent*>(event);
        if(mouse->button()==Qt::LeftButton) {
            selectEntry(id);emit entrySelected(id);
            if(m_document.locked) {return true;}
            m_active=id;m_resizing=handle->property("canvasResize").toBool();m_original=logicalRect(id);m_press=mouse->globalPosition();m_capture=handle;
            // Consuming a child's press requires explicit capture: observed in
            // AetherSDR TitleBar.cpp:585-588 [@1e0718a], no upstream logic ported.
            handle->grabMouse();handle->setFocus();updateReveal();return true;
        }
    }
    if(event->type()==QEvent::MouseMove && m_active==id) {
        auto* mouse=static_cast<QMouseEvent*>(event);const QPointF delta=mouse->globalPosition()-m_press;QRectF rect=m_original;
        if(m_resizing) {
            const QSizeF minimum=m_leaves[id].view->property("freeCanvasMinimum").toSizeF().expandedTo(QSizeF(24,24));
            rect.setSize(QSizeF(qMax(minimum.width(),rect.width()+delta.x()),qMax(minimum.height(),rect.height()+delta.y())));
        } else {rect.translate(delta);}
        changeRect(id,rect);return true;
    }
    if(event->type()==QEvent::MouseButtonRelease && m_active==id) {finishGesture(false);return true;}
    return QWidget::eventFilter(watched,event);
}
}
