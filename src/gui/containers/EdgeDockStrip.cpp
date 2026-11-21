// src/gui/containers/EdgeDockStrip.cpp

// --- From src/gui/applets/AppletGrid.cpp ---
// =================================================================
// src/gui/applets/AppletGrid.cpp  (Longpath)
// =================================================================
// Siehe AppletGrid.h — Schritt 1 des freien Rasters.

// --- From src/gui/StyleConstants.h ---
// =================================================================
// src/gui/StyleConstants.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Theme palette imported from AetherSDR `src/gui/ComboStyle.h` /
//                 `HGauge.h` / `SliceColors.h` and inline QColor calls
//                 in `MainWindow.cpp` / `VfoWidget.cpp`.
// =================================================================

// --- From src/gui/WindowPlacement.h ---
// =================================================================
// src/gui/WindowPlacement.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Ein Fenster auf einen sichtbaren Schirm holen ────────────────────
//
// Der Rumpf dieser Funktion stand seit 2026-04-17 als
// FloatingContainer::ensureVisiblePosition() im Baum und löste den Fall
// „der Monitor ist beim Start nicht mehr da" bereits vollständig. Er
// hing dort nur an einer Klasse fest, die ein Meter-Fenster ist — er
// fasst nichts von ihr an ausser zwei Grössenkonstanten.
//
// Am 2026-08-16 wurde er hierher gehoben, damit die Applet-Fenster
// dieselbe Lösung BENUTZEN, statt eine zweite halbe zu bekommen.
// Verhalten unverändert; FloatingContainer ruft jetzt hierher.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-17 — Ursprung als FloatingContainer::ensureVisiblePosition,
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//   2026-08-16 — Als freie Funktion herausgezogen, Verhalten
//                 unverändert. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
//   2026-09-25 — coveredFraction(): eingeschaltete Applets nach vorne.
//                 Martin Fischer, AI-assisted via Anthropic Claude.
//   2026-09-27 — fitFrameIntoArea(): Werkzeugfenster nie groesser als
//                 der Schirm. Martin Fischer, AI-assisted via Anthropic
//                 Claude.
// =================================================================

// --- From src/gui/WindowPlacement.cpp ---
// =================================================================
// src/gui/WindowPlacement.cpp  (Longpath)
// =================================================================
// Siehe WindowPlacement.h für Herkunft und die zwei bekannten
// Schwächen, die hier bewusst unverändert bleiben.
// =================================================================

// =================================================================
// src/gui/containers/EdgeDockStrip.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source at revision
//   551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banners are copied above. See
//   EdgeDockStrip.h for what is ported; everything else here is
//   NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08 - Today's split that the right strip follows is set against
//                 the 260 px container floor, as today's splitter now is
//                 before the Main Panel's minimum rises to its content; the
//                 strip's own content minimum then floors it. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The right strip has no padding and a 3 px handle on its
//                 spectrum side, so it and its handle take exactly today's
//                 Main Panel and splitter handle width (JJ's choice).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The drop overlay lets clicks through as a native view
//                 (macOS), so Arrange answers input. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Review fixes: clear an edge thickness and reset
//                 strip extents to the default split. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - The 3 px right-strip handle keeps its drawn width and
//                 takes the 5 px grab area Qt gives today's 3 px splitter
//                 handle. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - The drop overlay is masked to its bands and preview and
//                 hidden with nothing to draw: unmasked, on Windows it
//                 blanked the spectrum for all of Arrange. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "EdgeDockStrip.h"
#include "ContainerWidget.h"

#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSplitterHandle>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {

namespace {

QColor role(const char* name)
{
    return Workspace::WorkspacePalette::glassAndDepth().color(QString::fromLatin1(name));
}

void fillWithAppBackground(QWidget* w)
{
    QPalette palette = w->palette();
    palette.setColor(QPalette::Window, role("app-bg"));
    w->setPalette(palette);
    w->setAutoFillBackground(true);
}

QString edgeObjectName(DockEdge edge)
{
    switch (edge) {
    case DockEdge::Left:
        return QStringLiteral("edgeDockStripLeft");
    case DockEdge::Right:
        return QStringLiteral("edgeDockStripRight");
    case DockEdge::Top:
        return QStringLiteral("edgeDockStripTop");
    case DockEdge::Bottom:
        return QStringLiteral("edgeDockStripBottom");
    }
    return {};
}

constexpr DockEdge kEdges[] = {DockEdge::Left, DockEdge::Right, DockEdge::Top, DockEdge::Bottom};

// 6 px handle (3 px on the spectrum side of the right strip), app
// background, a 1 px centre line in the border role that takes the accent
// role under the pointer.
class EdgeDockHandle : public QSplitterHandle {
public:
    EdgeDockHandle(Qt::Orientation orientation, QSplitter* parent)
        : QSplitterHandle(orientation, parent)
    {
        setAttribute(Qt::WA_Hover);
    }

    QSize sizeHint() const override
    {
        const QSize base = QSplitterHandle::sizeHint();
        const int px = extent();
        return orientation() == Qt::Horizontal ? QSize(px, base.height()) : QSize(base.width(), px);
    }

private:
    // The narrowest grab area QSplitterHandle gives a thin handle (Qt's
    // QSplitterHandle::resizeEvent, margin (5 - handleWidth) / 2).
    static constexpr int kMinGrabPx = 5;

    // A handle sits before the widget at its index: the strip it opens.
    int extent() const
    {
        QSplitter* s = splitter();
        const int index = s->indexOf(const_cast<EdgeDockHandle*>(this));
        if (const auto* strip = qobject_cast<const EdgeDockStrip*>(s->widget(index))) {
            return EdgeDockStrip::handleExtentFor(strip->edge());
        }
        return EdgeDockStrip::kHandlePx;
    }

protected:
    // The grab area QSplitterHandle::resizeEvent gives a handle thinner
    // than 5 px, worked out from this handle's own extent rather than the
    // splitter's handleWidth (6): margins on both sides that the splitter
    // adds to the handle's geometry, mouse input over all of it, and a
    // mask so only the extent itself draws. The 3 px right-strip handle
    // grabs 5 px, as today's 3 px splitter handle does.
    void resizeEvent(QResizeEvent* event) override
    {
        const int margin = (kMinGrabPx - extent()) / 2;
        const bool tiny = margin > 0;
        setAttribute(Qt::WA_MouseNoMask, tiny);
        if (tiny) {
            if (orientation() == Qt::Horizontal) {
                setContentsMargins(margin, 0, margin, 0);
            } else {
                setContentsMargins(0, margin, 0, margin);
            }
            setMask(QRegion(contentsRect()));
        } else {
            setContentsMargins(0, 0, 0, 0);
            clearMask();
        }
        QWidget::resizeEvent(event);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), role("app-bg"));
        const QColor line = underMouse() ? role("accent") : role("border");
        if (orientation() == Qt::Horizontal) {
            painter.fillRect(QRect(width() / 2, 0, 1, height()), line);
        } else {
            painter.fillRect(QRect(0, height() / 2, width(), 1), line);
        }
    }
};

} // namespace

// From Longpath src/gui/WindowPlacement.cpp:140-147 [@551576e3]
QPoint workspaceSnappedTopLeft(const QPoint& pos, int grid)
{
    if (grid <= 1) { return pos; }
    const auto snapAxis = [grid](int v) {
        return static_cast<int>(std::lround(v / static_cast<double>(grid))) * grid;
    };
    return QPoint(snapAxis(pos.x()), snapAxis(pos.y()));
}

// ---- EdgeDockSplitter -----------------------------------------------------

EdgeDockSplitter::EdgeDockSplitter(Qt::Orientation orientation, QWidget* parent)
    : QSplitter(orientation, parent)
{
    setHandleWidth(EdgeDockStrip::kHandlePx);
    setChildrenCollapsible(false);
}

QSplitterHandle* EdgeDockSplitter::createHandle()
{
    return new EdgeDockHandle(orientation(), this);
}

// ---- EdgeDockStrip --------------------------------------------------------

EdgeDockStrip::EdgeDockStrip(DockEdge edge, QWidget* parent)
    : QWidget(parent)
    , m_edge(edge)
{
    setObjectName(edgeObjectName(edge));
    fillWithAppBackground(this);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    const bool vertical = orientation() == Qt::Vertical;
    m_scroll->setHorizontalScrollBarPolicy(vertical ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(vertical ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    fillWithAppBackground(m_scroll->viewport());

    m_body = new QWidget(m_scroll);
    m_body->setObjectName(QStringLiteral("edgeDockStripBody"));
    fillWithAppBackground(m_body);
    auto* bodyLayout = new QVBoxLayout(m_body);
    const int pad = paddingFor(edge);
    bodyLayout->setContentsMargins(pad, pad, pad, pad);
    bodyLayout->setSpacing(0);
    m_splitter = new EdgeDockSplitter(orientation(), m_body);
    m_splitter->setHandleWidth(kGapPx);
    bodyLayout->addWidget(m_splitter);
    m_scroll->setWidget(m_body);
    outer->addWidget(m_scroll);

    connect(m_splitter, &QSplitter::splitterMoved, this, &EdgeDockStrip::extentsChanged);
}

Qt::Orientation EdgeDockStrip::orientationFor(DockEdge edge)
{
    return edge == DockEdge::Left || edge == DockEdge::Right ? Qt::Vertical : Qt::Horizontal;
}

Qt::Orientation EdgeDockStrip::orientation() const
{
    return orientationFor(m_edge);
}

int EdgeDockStrip::defaultThickness(DockEdge edge)
{
    return orientationFor(edge) == Qt::Vertical ? kSideThicknessPx : kEndThicknessPx;
}

int EdgeDockStrip::paddingFor(DockEdge edge)
{
    return edge == DockEdge::Right ? kRightPaddingPx : kPaddingPx;
}

int EdgeDockStrip::handleExtentFor(DockEdge edge)
{
    return edge == DockEdge::Right ? kRightHandlePx : kHandlePx;
}

void EdgeDockStrip::setContainers(const QList<QWidget*>& widgets)
{
    for (int i = 0; i < widgets.size(); ++i) {
        if (m_splitter->indexOf(widgets[i]) != i) {
            m_splitter->insertWidget(i, widgets[i]);
        }
    }
}

QList<QWidget*> EdgeDockStrip::containers() const
{
    QList<QWidget*> result;
    for (int i = 0; i < m_splitter->count(); ++i) {
        result.append(m_splitter->widget(i));
    }
    return result;
}

int EdgeDockStrip::count() const
{
    return m_splitter->count();
}

QList<QWidget*> EdgeDockStrip::shownContainers() const
{
    QList<QWidget*> result;
    for (QWidget* w : containers()) {
        if (!w->isHidden()) {
            result.append(w);
        }
    }
    return result;
}

bool EdgeDockStrip::hasShownContainer() const
{
    return !shownContainers().isEmpty();
}

QList<int> EdgeDockStrip::extents() const
{
    return m_splitter->sizes();
}

void EdgeDockStrip::setExtents(const QList<int>& extents)
{
    if (extents.size() == m_splitter->count()) {
        m_splitter->setSizes(extents);
    }
}

void EdgeDockStrip::resetExtents()
{
    const bool vertical = orientation() == Qt::Vertical;
    QList<int> sizes;
    for (QWidget* w : containers()) {
        const QSize hint = w->sizeHint();
        sizes.append(w->isHidden() ? 0 : qMax(1, vertical ? hint.height() : hint.width()));
    }
    if (!sizes.isEmpty()) {
        m_splitter->setSizes(sizes);
    }
}

QRect EdgeDockStrip::containerRect(QWidget* w) const
{
    return QRect(w->mapTo(this, QPoint(0, 0)), w->size());
}

int EdgeDockStrip::insertionIndexAt(const QPoint& pos) const
{
    const bool vertical = orientation() == Qt::Vertical;
    const int along = vertical ? pos.y() : pos.x();
    for (int i = 0; i < m_splitter->count(); ++i) {
        QWidget* w = m_splitter->widget(i);
        if (w->isHidden()) {
            continue;
        }
        const QRect r = containerRect(w);
        if (along < (vertical ? r.center().y() : r.center().x())) {
            return i;
        }
    }
    return m_splitter->count();
}

QRect EdgeDockStrip::insertionRect(int index) const
{
    const bool vertical = orientation() == Qt::Vertical;
    const int pad = padding();
    const QRect inner = rect().adjusted(pad, pad, -pad, -pad);
    QWidget* before = nullptr;
    QWidget* after = nullptr;
    for (int i = 0; i < m_splitter->count(); ++i) {
        QWidget* w = m_splitter->widget(i);
        if (w->isHidden()) {
            continue;
        }
        if (i < index) {
            before = w;
        } else if (after == nullptr) {
            after = w;
        }
    }
    if (before == nullptr && after == nullptr) {
        return inner;
    }
    const auto start = [&](QWidget* w) {
        return vertical ? containerRect(w).top() : containerRect(w).left();
    };
    const auto end = [&](QWidget* w) {
        return vertical ? containerRect(w).bottom() + 1 : containerRect(w).right() + 1;
    };
    int from = 0;
    if (before != nullptr && after != nullptr) {
        from = (end(before) + start(after)) / 2 - kInsertPreviewPx / 2;
    } else if (before == nullptr) {
        from = start(after);
    } else {
        from = end(before) - kInsertPreviewPx;
    }
    QRect r = vertical ? QRect(inner.left(), from, inner.width(), kInsertPreviewPx)
                       : QRect(from, inner.top(), kInsertPreviewPx, inner.height());
    if (vertical) {
        r.moveTop(qBound(inner.top(), r.top(), qMax(inner.top(), inner.bottom() + 1 - kInsertPreviewPx)));
    } else {
        r.moveLeft(qBound(inner.left(), r.left(), qMax(inner.left(), inner.right() + 1 - kInsertPreviewPx)));
    }
    return r;
}

// ---- EdgeDropOverlay ------------------------------------------------------

class EdgeDropOverlay : public QWidget {
public:
    explicit EdgeDropOverlay(EdgeDockArea* area)
        : QWidget(area)
        , m_area(area)
    {
        setObjectName(QStringLiteral("edgeDropOverlay"));
        // Shown over the whole area for all of Arrange: as a native view
        // (macOS) it took every click and drag in the workspace.
        WorkspacePreview::passInputThrough(this);
        setAttribute(Qt::WA_NoSystemBackground);
        hide();
    }

    bool bands{false};
    bool preview{false};
    QRect previewRect;

    // The pixels paintEvent draws: the four bands and the preview's
    // rectangle.
    QRegion drawnRegion() const
    {
        QRegion region;
        if (bands) {
            for (DockEdge edge : kEdges) {
                region += m_area->targetBandRect(edge);
            }
        }
        if (preview && previewRect.isValid()) {
            region += previewRect;
        }
        return region & m_area->rect();
    }

    // The overlay covers only what it draws. Next to a native surface
    // (SpectrumWidget and MeterWidget are WA_NativeWindow) Qt makes this
    // widget a native window too, and on Windows a child window is not
    // blended with the swapchain of the sibling under it: all of its
    // rectangle shows the window's backing store, which holds no spectrum.
    // Unmasked, the overlay blanked the whole pane for all of Arrange.
    void sync()
    {
        setGeometry(m_area->rect());
        const QRegion region = drawnRegion();
        // An empty mask means no mask at all, so with nothing to draw the
        // overlay is hidden.
        setVisible(!region.isEmpty());
        if (isVisible()) {
            if (mask() != region) {
                setMask(region);
            }
            raise();
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        const QColor accent = role("accent");
        if (bands) {
            QColor band = accent;
            band.setAlphaF(static_cast<float>(EdgeDockArea::kTargetBandAlpha));
            for (DockEdge edge : kEdges) {
                painter.fillRect(m_area->targetBandRect(edge), band);
            }
        }
        if (preview && previewRect.isValid()) {
            painter.setRenderHint(QPainter::Antialiasing);
            QColor fill = accent;
            fill.setAlphaF(static_cast<float>(EdgeDockArea::kPreviewFillAlpha));
            QPainterPath path;
            path.addRoundedRect(QRectF(previewRect).adjusted(0.5, 0.5, -0.5, -0.5),
                                EdgeDockArea::kPreviewRadiusPx, EdgeDockArea::kPreviewRadiusPx);
            painter.fillPath(path, fill);
            painter.setPen(QPen(QColor::fromRgba(EdgeDockArea::kPreviewBorder), 1.0));
            painter.drawPath(path);
        }
    }

private:
    EdgeDockArea* m_area;
};

// ---- EdgeDockArea ---------------------------------------------------------

EdgeDockArea::EdgeDockArea(QSplitter* middle, QWidget* center, QWidget* parent)
    : QWidget(parent)
    , m_middle(middle)
    , m_center(center)
{
    setObjectName(QStringLiteral("edgeDockArea"));
    fillWithAppBackground(this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_vertical = new EdgeDockSplitter(Qt::Vertical, this);
    m_vertical->setObjectName(QStringLiteral("edgeDockVertical"));
    for (DockEdge edge : kEdges) {
        auto* strip = new EdgeDockStrip(edge, this);
        strip->hide();
        m_strips[indexOf(edge)] = strip;
        connect(strip, &EdgeDockStrip::extentsChanged, this, [this] {
            if (!m_applying) {
                emit thicknessChanged();
            }
        });
    }
    m_vertical->addWidget(strip(DockEdge::Top));
    m_vertical->addWidget(m_middle);
    m_vertical->addWidget(strip(DockEdge::Bottom));
    m_middle->insertWidget(0, strip(DockEdge::Left));
    m_middle->addWidget(strip(DockEdge::Right));
    m_middle->setChildrenCollapsible(false);
    // Only the middle row and the centre stretch: a strip keeps its
    // thickness when the window is resized.
    m_vertical->setStretchFactor(1, 1);
    m_middle->setStretchFactor(m_middle->indexOf(m_center), 1);
    layout->addWidget(m_vertical);

    connect(m_vertical, &QSplitter::splitterMoved, this, &EdgeDockArea::captureThickness);
    connect(m_middle, &QSplitter::splitterMoved, this, &EdgeDockArea::captureThickness);

    m_overlay = new EdgeDropOverlay(this);
}

int EdgeDockArea::indexOf(DockEdge edge) const
{
    return qBound(0, int(edge), 3);
}

EdgeDockStrip* EdgeDockArea::strip(DockEdge edge) const
{
    return m_strips[indexOf(edge)];
}

int EdgeDockArea::migratedRightThickness() const
{
    const int total = width() > 0 ? width() : m_middle->width();
    // Today's Main Panel width, as the 3 px-handle QSplitter lays out
    // setSizes({1024, 256}): each pane in proportion to its size raised to
    // its minimum, so a 260 px panel minimum gives 306 of 1509 px
    // (measured on Qt 6.11). The right strip has no padding and its
    // handle is that splitter's 3 px, so the panel keeps that width and
    // the spectrum today's.
    const int space = qMax(1, total - kMigratedHandlePx);
    // The minimum that splitter saw when the split was set: the
    // container's 260 px floor. The Main Panel's content minimum is
    // applied after it (ContainerManager, queued), and here it is the
    // strip's floor in thickness(), so each look keeps today's width
    // where it fits and its own content minimum where it does not.
    const int minimum = ContainerWidget::kMinContainerWidth;
    const int panel = qMax(kMigratedPanelShare, minimum);
    const int spectrum = kMigratedTotalShare - kMigratedPanelShare;
    return qMax(1, int(std::lround(space * double(panel) / (spectrum + panel))))
        + 2 * EdgeDockStrip::paddingFor(DockEdge::Right);
}

int EdgeDockArea::thickness(DockEdge edge) const
{
    const int i = indexOf(edge);
    const int padding = 2 * EdgeDockStrip::paddingFor(edge);
    const bool side = EdgeDockStrip::orientationFor(edge) == Qt::Vertical;
    // A side strip is never narrower than its widest container's minimum.
    const int floor = side && m_minimumCross[i] > 0 ? m_minimumCross[i] + padding : 0;
    if (m_explicit[i]) {
        return qMax(m_thickness[i], floor);
    }
    if (side) {
        const int base = edge == DockEdge::Right ? migratedRightThickness()
                                                 : EdgeDockStrip::defaultThickness(edge);
        return qMax(base, floor);
    }
    // An end strip shows its tallest container whole by default; 112 px
    // stays the least it starts at. Made smaller by the user, it scrolls.
    const int preferred = m_preferredCross[i] > 0 ? m_preferredCross[i] + padding : 0;
    return qMax(EdgeDockStrip::defaultThickness(edge), preferred);
}

void EdgeDockArea::setStripContentHint(DockEdge edge, int minimumCross, int preferredCross)
{
    const int i = indexOf(edge);
    m_minimumCross[i] = qMax(0, minimumCross);
    m_preferredCross[i] = qMax(0, preferredCross);
    EdgeDockStrip* s = strip(edge);
    const int floor = m_minimumCross[i] > 0 ? m_minimumCross[i] + 2 * EdgeDockStrip::paddingFor(edge) : 0;
    if (EdgeDockStrip::orientationFor(edge) == Qt::Vertical) {
        s->setMinimumWidth(floor);
    }
}

bool EdgeDockArea::hasThickness(DockEdge edge) const
{
    return m_explicit[indexOf(edge)];
}

void EdgeDockArea::setThickness(DockEdge edge, int thickness)
{
    const int i = indexOf(edge);
    if (thickness <= 0 || (m_explicit[i] && m_thickness[i] == thickness)) {
        return;
    }
    m_thickness[i] = thickness;
    m_explicit[i] = true;
    applySizes();
}

void EdgeDockArea::clearThickness(DockEdge edge)
{
    const int i = indexOf(edge);
    if (!m_explicit[i]) {
        return;
    }
    m_explicit[i] = false;
    m_thickness[i] = 0;
    applySizes();
}

void EdgeDockArea::setUserCollapsed(DockEdge edge, bool collapsed)
{
    m_collapsed[indexOf(edge)] = collapsed;
    refreshLayout();
}

bool EdgeDockArea::userCollapsed(DockEdge edge) const
{
    return m_collapsed[indexOf(edge)];
}

void EdgeDockArea::refreshLayout()
{
    for (DockEdge edge : kEdges) {
        const int i = indexOf(edge);
        const bool shown = strip(edge)->hasShownContainer() && !m_collapsed[i];
        m_shown[i] = shown;
        if (strip(edge)->isHidden() == shown) {
            strip(edge)->setVisible(shown);
        }
    }
    applySizes();
}

void EdgeDockArea::applySizes()
{
    if (m_applying) {
        return;
    }
    QScopedValueRollback<bool> guard(m_applying, true);
    const auto across = [this](DockEdge edge) {
        return m_shown[indexOf(edge)] ? thickness(edge) : 0;
    };
    // The handle that opens a strip, as the splitter lays it out: its
    // size hint along the splitter (3 px beside the right strip).
    const auto handle = [](QSplitter* splitter, QWidget* w) {
        const int index = splitter->indexOf(w);
        QSplitterHandle* h = index >= 0 ? splitter->handle(index) : nullptr;
        if (h == nullptr) {
            return splitter->handleWidth();
        }
        return splitter->orientation() == Qt::Horizontal ? h->sizeHint().width() : h->sizeHint().height();
    };
    const int top = across(DockEdge::Top);
    const int bottom = across(DockEdge::Bottom);
    // The top strip's own handle is the first, hidden one; the handle
    // below it opens the middle row.
    const int verticalHandles = (top > 0 ? handle(m_vertical, m_middle) : 0)
                              + (bottom > 0 ? handle(m_vertical, strip(DockEdge::Bottom)) : 0);
    const int height = qMax(1, m_vertical->height() - top - bottom - verticalHandles);
    m_vertical->setSizes({top, height, bottom});

    const int left = across(DockEdge::Left);
    const int right = across(DockEdge::Right);
    // The left strip's own handle is the first, hidden one; the handle
    // after it opens the centre.
    const int middleHandles = (left > 0 ? handle(m_middle, m_center) : 0)
                            + (right > 0 ? handle(m_middle, strip(DockEdge::Right)) : 0);
    const int width = qMax(1, m_middle->width() - left - right - middleHandles);
    QList<int> sizes;
    for (int i = 0; i < m_middle->count(); ++i) {
        QWidget* w = m_middle->widget(i);
        sizes.append(w == strip(DockEdge::Left) ? left : w == strip(DockEdge::Right) ? right : width);
    }
    m_middle->setSizes(sizes);
}

void EdgeDockArea::captureThickness()
{
    if (m_applying) {
        return;
    }
    const QList<int> vertical = m_vertical->sizes();
    const QList<int> middle = m_middle->sizes();
    for (DockEdge edge : kEdges) {
        const int i = indexOf(edge);
        if (!m_shown[i]) {
            continue;
        }
        QSplitter* splitter = EdgeDockStrip::orientationFor(edge) == Qt::Vertical ? m_middle : m_vertical;
        const int index = splitter->indexOf(strip(edge));
        const QList<int>& sizes = splitter == m_middle ? middle : vertical;
        if (index >= 0 && index < sizes.size() && sizes[index] > 0) {
            m_thickness[i] = sizes[index];
            m_explicit[i] = true;
        }
    }
    emit thicknessChanged();
}

void EdgeDockArea::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_overlay->sync();
    // Until a thickness is saved or dragged, the right strip keeps
    // today's share of the window, as the Main Panel did.
    if (!m_explicit[indexOf(DockEdge::Right)] && m_shown[indexOf(DockEdge::Right)]) {
        applySizes();
    }
}

QRect EdgeDockArea::stripRect(DockEdge edge) const
{
    EdgeDockStrip* s = strip(edge);
    return QRect(s->mapTo(this, QPoint(0, 0)), s->size());
}

QRect EdgeDockArea::targetBandRect(DockEdge edge) const
{
    switch (edge) {
    case DockEdge::Left:
        return QRect(0, 0, kTargetBandPx, height());
    case DockEdge::Right:
        return QRect(width() - kTargetBandPx, 0, kTargetBandPx, height());
    case DockEdge::Top:
        return QRect(0, 0, width(), kTargetBandPx);
    case DockEdge::Bottom:
        return QRect(0, height() - kTargetBandPx, width(), kTargetBandPx);
    }
    return {};
}

EdgeDockArea::DropTarget EdgeDockArea::dropTargetAt(const QPoint& globalPos) const
{
    DropTarget target;
    // Pulled well outside the main window: detach to a floating shell.
    const QWidget* win = window();
    const int threshold = QApplication::startDragDistance();
    const QRect windowRect(win->mapToGlobal(QPoint(0, 0)), win->size());
    if (!windowRect.adjusted(-threshold, -threshold, threshold, threshold).contains(globalPos)) {
        target.detach = true;
        return target;
    }
    const QPoint p = mapFromGlobal(globalPos);
    for (DockEdge edge : kEdges) {
        if (!m_shown[indexOf(edge)]) {
            continue;
        }
        if (stripRect(edge).contains(p)) {
            target.valid = true;
            target.edge = edge;
            target.index = strip(edge)->insertionIndexAt(strip(edge)->mapFrom(this, p));
            return target;
        }
    }
    if (!rect().contains(p)) {
        return target;
    }
    const int distances[4] = {p.x(), width() - 1 - p.x(), p.y(), height() - 1 - p.y()};
    int nearest = 0;
    for (int i = 1; i < 4; ++i) {
        if (distances[i] < distances[nearest]) {
            nearest = i;
        }
    }
    if (distances[nearest] < kEdgeHotZonePx) {
        target.valid = true;
        target.edge = kEdges[nearest];
        target.index = strip(target.edge)->count();
    }
    return target;
}

QRect EdgeDockArea::previewRectFor(DockEdge edge, int index) const
{
    if (m_shown[indexOf(edge)]) {
        EdgeDockStrip* s = strip(edge);
        return s->insertionRect(index).translated(s->mapTo(this, QPoint(0, 0)));
    }
    // An empty strip: where it would open, between the strips that exist.
    const int t = thickness(edge);
    const QRect middle(m_middle->mapTo(this, QPoint(0, 0)), m_middle->size());
    QRect r;
    switch (edge) {
    case DockEdge::Top:
        r = QRect(0, 0, width(), t);
        break;
    case DockEdge::Bottom:
        r = QRect(0, height() - t, width(), t);
        break;
    case DockEdge::Left:
        r = QRect(0, middle.top(), t, middle.height());
        break;
    case DockEdge::Right:
        r = QRect(width() - t, middle.top(), t, middle.height());
        break;
    }
    const int pad = EdgeDockStrip::paddingFor(edge);
    return r.adjusted(pad, pad, -pad, -pad);
}

void EdgeDockArea::setTargetBandsVisible(bool visible)
{
    if (m_overlay->bands != visible) {
        m_overlay->bands = visible;
        m_overlay->sync();
    }
}

bool EdgeDockArea::targetBandsVisible() const
{
    return m_overlay->bands;
}

void EdgeDockArea::showDropPreview(DockEdge edge, int index)
{
    m_overlay->preview = true;
    m_overlay->previewRect = previewRectFor(edge, index);
    m_overlay->sync();
}

void EdgeDockArea::clearDropPreview()
{
    if (m_overlay->preview) {
        m_overlay->preview = false;
        m_overlay->previewRect = QRect();
        m_overlay->sync();
    }
}

bool EdgeDockArea::dropPreviewVisible() const
{
    return m_overlay->preview;
}

QRect EdgeDockArea::dropPreviewRect() const
{
    return m_overlay->preview ? m_overlay->previewRect : QRect();
}

} // namespace NereusSDR
