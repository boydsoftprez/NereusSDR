// =================================================================
// src/gui/containers/FloatingContainer.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/frmMeterDisplay.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
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
#include "core/AppSettings.h"
#include "core/LogCategories.h"

#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QScreen>
#include <QVBoxLayout>

namespace NereusSDR {

FloatingContainer::FloatingContainer(int rxSource, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_rxSource(rxSource)
{
    setMinimumSize(ContainerWidget::kMinContainerWidth,
                   ContainerWidget::kMinContainerHeight);
    setStyleSheet(QStringLiteral("background: #0f0f1a;"));
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

    // Task 16 — remember the embedded child so context-menu /
    // double-click events landing on the top-level frame can be
    // forwarded down into ContainerWidget's own handlers.
    m_embedded = container;

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
            resize(width(), m_unminimisedHeight);
        }
        m_unminimisedHeight = 0;
    }
}

void FloatingContainer::onConsoleWindowStateChanged(Qt::WindowStates state, bool rx2Enabled)
{
    // From Thetis frmMeterDisplay.cs:114-139
    if (m_formEnabled && m_floating && m_containerMinimises) {
        if (state & Qt::WindowMinimized) {
            hide();
        } else {
            bool shouldShow = false;
            if (m_rxSource == 1) {
                shouldShow = !m_hiddenByMacro;
            } else if (m_rxSource == 2) {
                if (rx2Enabled || !m_containerHidesWhenRxNotUsed) {
                    shouldShow = !m_hiddenByMacro;
                }
            }
            if (shouldShow) {
                show();
            }
        }
    }
}

void FloatingContainer::closeEvent(QCloseEvent* event)
{
    // From Thetis frmMeterDisplay.cs:158-166 — hide instead of close
    if (event->spontaneous()) {
        hide();
        event->ignore();
        return;
    }
    saveGeometry();
    QWidget::closeEvent(event);
}

// ---------------------------------------------------------------------------
// Task 16 — forward right-click and double-click to the embedded
// ContainerWidget so the Task 15 on-container affordances work
// identically when the container is floating.
// ---------------------------------------------------------------------------

void FloatingContainer::contextMenuEvent(QContextMenuEvent* event)
{
    if (m_embedded) {
        // Translate the global click point into the embedded child's
        // local coordinates so ContainerWidget::contextMenuEvent places
        // the popup menu at the right spot.
        QContextMenuEvent ev(event->reason(),
                              m_embedded->mapFromGlobal(event->globalPos()),
                              event->globalPos(),
                              event->modifiers());
        QCoreApplication::sendEvent(m_embedded, &ev);
        event->accept();
        return;
    }
    QWidget::contextMenuEvent(event);
}

void FloatingContainer::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_embedded) {
        const QPointF globalPos = event->globalPosition();
        QMouseEvent ev(event->type(),
                        m_embedded->mapFromGlobal(globalPos.toPoint()),
                        globalPos,
                        event->button(),
                        event->buttons(),
                        event->modifiers());
        QCoreApplication::sendEvent(m_embedded, &ev);
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void FloatingContainer::saveGeometry()
{
    auto& s = AppSettings::instance();
    QRect r = geometry();
    s.setValue(QStringLiteral("MeterDisplay_%1_Geometry").arg(m_id),
              QStringLiteral("%1,%2,%3,%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()));
}

void FloatingContainer::restoreGeometry()
{
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
        setGeometry(x, y, w, h);
    }
}

void FloatingContainer::ensureVisiblePosition(QWidget* anchor)
{
    // A frameless Qt::Window with no explicit position defaults to (0,0)
    // on Windows, where it's usually obscured by the main application
    // window — which is how Container #0 "disappeared" when first
    // floated. Saved geometry at (0,0) (e.g., after the user closed the
    // invisible form) perpetuates the trap across restarts.
    const QRect g = geometry();
    const int minW = ContainerWidget::kMinContainerWidth;
    const int minH = ContainerWidget::kMinContainerHeight;

    bool onScreen = false;
    for (QScreen* s : QGuiApplication::screens()) {
        if (s && s->availableGeometry().intersects(g)) {
            onScreen = true;
            break;
        }
    }
    const bool atOrigin = (g.x() == 0 && g.y() == 0);
    if (onScreen && !atOrigin && g.width() >= minW && g.height() >= minH) {
        return;
    }

    QScreen* screen = nullptr;
    QRect anchorRect;
    if (anchor && anchor->window()) {
        screen = anchor->window()->screen();
        anchorRect = anchor->window()->geometry();
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect avail = screen ? screen->availableGeometry()
                               : QRect(100, 100, 800, 600);

    const int w = qMax(g.width(),  minW > 0 ? minW : 260);
    const int h = qMax(g.height(), minH > 24 ? minH : 300);

    int x = anchorRect.isValid()
        ? anchorRect.center().x() - w / 2
        : avail.x() + (avail.width()  - w) / 2;
    int y = anchorRect.isValid()
        ? anchorRect.center().y() - h / 2
        : avail.y() + (avail.height() - h) / 2;

    x = qBound(avail.x(), x, avail.right()  - w);
    y = qBound(avail.y(), y, avail.bottom() - h);

    setGeometry(x, y, w, h);
    qCDebug(lcContainer) << "FloatingContainer: repositioned to visible area"
                          << QRect(x, y, w, h) << "on screen" << (screen ? screen->name() : QStringLiteral("(null)"));
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
