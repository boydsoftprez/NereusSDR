// no-port-check: AetherSDR-derived NereusSDR file. Top-level QWidget
// wrapper for detaching a PanadapterApplet to a second monitor is adapted
// structurally from AetherSDR src/gui/PanFloatingWindow.{h,cpp} [@0cd4559].
// Registered in docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanFloatingWindow.cpp  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanFloatingWindow.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// See PanFloatingWindow.h for full Modification history (NereusSDR).
// =================================================================

#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterApplet.h"

#include <QVBoxLayout>
#include <QCloseEvent>
#include <QMoveEvent>
#include <QResizeEvent>

namespace NereusSDR {

PanFloatingWindow::PanFloatingWindow(PanadapterApplet* applet, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , m_applet(applet)
{
    setWindowTitle(QStringLiteral("NereusSDR - Pan %1")
                       .arg(applet ? applet->panId() : QString()));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    if (applet) {
        layout->addWidget(applet);
    }
    resize(800, 400);
}

PanFloatingWindow::~PanFloatingWindow() = default;

PanadapterApplet* PanFloatingWindow::applet() const
{
    return m_applet.data();
}

QString PanFloatingWindow::panId() const
{
    return m_applet ? m_applet->panId() : QString();
}

void PanFloatingWindow::requestDock()
{
    emit dockRequested(panId());
}

void PanFloatingWindow::closeEvent(QCloseEvent* event)
{
    requestDock();
    event->accept();
}

void PanFloatingWindow::moveEvent(QMoveEvent*)
{
    emit geometryChanged(panId(), saveGeometry());
}

void PanFloatingWindow::resizeEvent(QResizeEvent*)
{
    emit geometryChanged(panId(), saveGeometry());
}

} // namespace NereusSDR
