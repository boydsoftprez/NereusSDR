// =================================================================
// src/gui/containers/FloatingContainer.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/frmMeterDisplay.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-25 - onMainWindowStateChanged replaces onConsoleWindowStateChanged:
//                 the slice-open test covers every slice, not only RX2.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-12 - A floating window is never wider or taller than all the
//                 screens together: fittedSize bounds a size to that
//                 (restoreGeometry and the height put back after a
//                 collapse go through it), and the window's maximum size
//                 is kept at it (refreshDesktopLimit). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - ensureVisiblePosition reads the screens through a
//                 ScreenSource and says whether it moved the form, by J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Workspace preview: the floating shell paints the palette's
//                 panel surface by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-04 — Application quit accepts structured form closure without
//                 arrangement by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Atomic container arrangement and reserved chrome by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  frmMeterDisplay.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "FloatingContainer.h"
#include "ContainerWidget.h"
#include "ScreenSource.h"
#include "gui/GuiApplication.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/styles/WorkspaceStyle.h"

#include <QCloseEvent>
#include <QVBoxLayout>

namespace NereusSDR {

FloatingContainer::FloatingContainer(int rxSource, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_rxSource(rxSource)
{
    setMinimumSize(ContainerWidget::kMinContainerWidth,
                   ContainerWidget::kMinContainerHeight);
    setStyleSheet(QStringLiteral("background: #0f0f1a;"));
    if (WorkspacePreview::enabled()) {
        // Workspace preview: an opaque surface behind the container's plate,
        // so nothing beneath the window shows through its corners.
        setStyleSheet(QStringLiteral("background: %1;")
                          .arg(Workspace::role("panel", "#0f0f1a")));
    }
    updateTitle();
    qCDebug(lcContainer) << "FloatingContainer created for RX" << rxSource;
}

FloatingContainer::~FloatingContainer()
{
    qCDebug(lcContainer) << "FloatingContainer destroyed:" << m_id;
}

void FloatingContainer::setId(const QString& id)
{
    m_id = id;
    // From Thetis frmMeterDisplay.cs:150-156 — setting ID triggers geometry restore
    restoreGeometry();
    updateTitle();
}

void FloatingContainer::takeOwner(ContainerWidget* container)
{
    // From Thetis frmMeterDisplay.cs:168-179
    m_containerMinimises = container->containerMinimises();
    m_formEnabled = container->isContainerEnabled();

    container->setParent(this);
    if (!layout()) {
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(0, 0, 0, 0);
    }
    // Remove any existing widgets from layout
    QLayoutItem* child = nullptr;
    while ((child = layout()->takeAt(0)) != nullptr) {
        delete child;
    }
    layout()->addWidget(container);
    container->show();
    container->raise();

    // Phase 3G-6 block 2: listen for the runtime minimised flag on
    // the owner so this top-level window can collapse to the title
    // bar and restore on un-minimise. Title-bar visibility is handled
    // entirely inside ContainerWidget and needs no FloatingContainer
    // involvement (the window just shrinks/grows to fit content).
    connect(container, &ContainerWidget::minimisedChanged, this,
            &FloatingContainer::onOwnerMinimisedChanged,
            Qt::UniqueConnection);

    qCDebug(lcContainer) << "FloatingContainer" << m_id
                          << "took ownership of" << container->id();
}

void FloatingContainer::onOwnerMinimisedChanged(bool minimised)
{
    if (minimised) {
        m_unminimisedHeight = height();
        // Collapse to just the title bar (plus frame margins). The
        // minimum is the inner title-bar height; give it a few pixels
        // slack for borders.
        const int collapsed = ContainerWidget::kTitleBarHeight + 4;
        resize(width(), collapsed);
    } else {
        if (m_unminimisedHeight > 0) {
            resize(fittedSize(QSize(width(), m_unminimisedHeight)));
        }
        m_unminimisedHeight = 0;
    }
}

void FloatingContainer::onMainWindowStateChanged(Qt::WindowStates state, bool sliceOpen)
{
    // From Thetis frmMeterDisplay.cs:114-139 [v2.10.3.15]
    if (m_formEnabled && m_floating && m_containerMinimises) {
        if (state & Qt::WindowMinimized) {
            hide();
        } else {
            // Thetis switches on _rx: RX1 always shows, RX2 shows when RX2
            // is enabled or the form does not hide for an unused RX. Every
            // slice here takes the RX2 rule, with its own open state.
            bool shouldShow = false;
            if (sliceOpen || !m_containerHidesWhenRxNotUsed) {
                shouldShow = !m_hiddenByMacro;
            }
            if (shouldShow) {
                show();
            }
        }
    }
}

void FloatingContainer::closeEvent(QCloseEvent* event)
{
    if (property("structuredWorkspace").toBool() && GuiApplication::applicationQuitInProgress()) {
        // Application teardown closes the form without a user arrangement
        // transaction; normal workspace closes still request return/hide below.
        QWidget::closeEvent(event);
        return;
    }
    if (property("structuredWorkspace").toBool()) { emit aboutToClose(); event->ignore(); return; }
    // From Thetis frmMeterDisplay.cs:158-166 — hide instead of close
    if (event->spontaneous()) {
        hide();
        event->ignore();
        return;
    }
    saveGeometry();
    QWidget::closeEvent(event);
}

void FloatingContainer::saveGeometry()
{
    if (property("structuredWorkspace").toBool()) { return; }
    auto& s = AppSettings::instance();
    QRect r = geometry();
    s.setValue(QStringLiteral("MeterDisplay_%1_Geometry").arg(m_id),
              QStringLiteral("%1,%2,%3,%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()));
}

void FloatingContainer::restoreGeometry()
{
    if (property("structuredWorkspace").toBool()) { return; }
    auto& s = AppSettings::instance();
    QString val = s.value(QStringLiteral("MeterDisplay_%1_Geometry").arg(m_id)).toString();
    if (val.isEmpty()) {
        return;
    }
    QStringList parts = val.split(QLatin1Char(','));
    if (parts.size() != 4) {
        return;
    }
    bool ok1, ok2, ok3, ok4;
    int x = parts[0].toInt(&ok1);
    int y = parts[1].toInt(&ok2);
    int w = parts[2].toInt(&ok3);
    int h = parts[3].toInt(&ok4);
    if (ok1 && ok2 && ok3 && ok4) {
        // A saved size is never larger than all the screens together.
        setGeometry(QRect(QPoint(x, y), fittedSize(QSize(w, h))));
    }
}

void FloatingContainer::setScreenSource(const ScreenSource* screens)
{
    m_screens = screens;
    refreshDesktopLimit();
}

QSize FloatingContainer::sizeWithin(const QSize& size, const QSize& limit, const QSize& minimum)
{
    if (!limit.isValid()) {
        return size.expandedTo(minimum);
    }
    return size.boundedTo(limit).expandedTo(minimum);
}

QSize FloatingContainer::fittedSize(const QSize& wanted) const
{
    const ScreenSource* source = m_screens ? m_screens : ScreenSource::system();
    // With no screen at all there is nothing to fit to.
    return sizeWithin(wanted, source->desktopSize(), minimumSize());
}

void FloatingContainer::refreshDesktopLimit()
{
    // The backstop: whatever asks for a size (this app, Qt or the
    // platform), the window is given no more than all the screens
    // together. Never less than the form's minimum, and no limit with no
    // screen. A limit smaller than the size the window has (a monitor
    // went) resizes it once, inside setMaximumSize.
    const ScreenSource* source = m_screens ? m_screens : ScreenSource::system();
    const QSize desktop = source->desktopSize();
    const QSize limit = desktop.isValid() ? desktop.expandedTo(minimumSize())
                                          : QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    if (maximumSize() != limit) {
        setMaximumSize(limit);
    }
}

QRect FloatingContainer::clampedGeometry(const QRect &geometry, const QRect &available,
                                         const QSize &minimum)
{
    const int w = qMax(minimum.width(), qMin(geometry.width(), available.width()));
    const int h = qMax(minimum.height(), qMin(geometry.height(), available.height()));
    return QRect(
        qBound(available.left(), geometry.x(), qMax(available.left(), available.right() - w + 1)),
        qBound(available.top(), geometry.y(), qMax(available.top(), available.bottom() - h + 1)), w,
        h);
}

bool FloatingContainer::ensureVisiblePosition(QWidget* anchor, const ScreenSource* screens)
{
    // A frameless Qt::Window with no explicit position defaults to (0,0)
    // on Windows, where it's usually obscured by the main application
    // window — which is how Container #0 "disappeared" when first
    // floated. Saved geometry at (0,0) (e.g., after the user closed the
    // invisible form) perpetuates the trap across restarts.
    const ScreenSource* source = screens ? screens : ScreenSource::system();
    const QRect g = geometry();
    const int minW = ContainerWidget::kMinContainerWidth;
    const int minH = ContainerWidget::kMinContainerHeight;

    bool onScreen = false;
    const QVector<ScreenInfo> present = source->screens();
    for (const ScreenInfo& s : present) {
        if (s.available.contains(g)) {
            onScreen = true;
            break;
        }
    }
    const bool atOrigin = (g.x() == 0 && g.y() == 0);
    if (onScreen && !atOrigin && g.width() >= minW && g.height() >= minH) {
        return false;
    }

    ScreenInfo screen = source->primary();
    QRect anchorRect;
    if (anchor && anchor->window()) {
        anchorRect = anchor->window()->geometry();
        screen = source->screenFor(anchorRect);
    }
    // With no screen at all, the area the form was always given.
    constexpr QRect kNoScreenArea(100, 100, 800, 600);
    const QRect avail = screen.available.isValid() ? screen.available : kNoScreenArea;

    const int w = qMax(g.width(),  minW > 0 ? minW : 260);
    const int h = qMax(g.height(), minH);

    int x = anchorRect.isValid()
        ? anchorRect.center().x() - w / 2
        : avail.x() + (avail.width()  - w) / 2;
    int y = anchorRect.isValid()
        ? anchorRect.center().y() - h / 2
        : avail.y() + (avail.height() - h) / 2;

    const QSize minimum =
        qMax(minimumSizeHint().width(), minW) > 0
            ? QSize(qMax(minimumSizeHint().width(), minW), qMax(minimumSizeHint().height(), minH))
            : QSize(minW, minH);
    setGeometry(clampedGeometry(QRect(x, y, w, h), avail, minimum));
    qCDebug(lcContainer) << "FloatingContainer: repositioned to visible area"
                          << QRect(x, y, w, h) << "on screen" << screen.name;
    return true;
}

void FloatingContainer::updateTitle()
{
    // From Thetis frmMeterDisplay.cs:140-144 — unique title for OBS/streaming
    uint hash = qHash(m_id) % 100000;
    setWindowTitle(QStringLiteral("NereusSDR Meter [%1]").arg(hash, 5, 10, QLatin1Char('0')));
}

void FloatingContainer::setContainerMinimises(bool minimises) { m_containerMinimises = minimises; }
void FloatingContainer::setContainerHidesWhenRxNotUsed(bool hides) { m_containerHidesWhenRxNotUsed = hides; }
void FloatingContainer::setFormEnabled(bool enabled) { m_formEnabled = enabled; }
void FloatingContainer::setHiddenByMacro(bool hidden) { m_hiddenByMacro = hidden; }
void FloatingContainer::setContainerFloating(bool floating) { m_floating = floating; }

} // namespace NereusSDR
