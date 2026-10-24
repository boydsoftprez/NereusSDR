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

// --- From PanFloatingWindow.cpp ---
// The quit rule in closeEvent() is ported from Longpath
// src/gui/PanFloatingWindow.cpp:230-243 [@383f058] (Longpath PR 83,
// 3a997ed3). Longpath's banner for that file, byte for byte:
// no-port-check: AetherSDR-derived Longpath file. Top-level QWidget
// wrapper for detaching a PanadapterApplet to a second monitor is adapted
// structurally from AetherSDR src/gui/PanFloatingWindow.{h,cpp} [@0cd4559].
// Registered in docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanFloatingWindow.cpp  (Longpath)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanFloatingWindow.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// See PanFloatingWindow.h for full Modification history (Longpath).
// =================================================================

#include "gui/PanFloatingWindow.h"
#include "gui/PanadapterApplet.h"
#include "core/AppSettings.h"

#include <QVBoxLayout>
#include <QCloseEvent>
#include <QMoveEvent>
#include <QResizeEvent>

namespace NereusSDR {

PanFloatingWindow::PanFloatingWindow(QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(QStringLiteral("NereusSDR - Pan"));
    setMinimumSize(400, 300);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    resize(800, 400);
}

PanFloatingWindow::~PanFloatingWindow() = default;

// From AetherSDR src/gui/PanFloatingWindow.cpp:34-56 [@1e0718ad]
//   adapter: NereusSDR's applet exposes no slice title, so the window is
//   titled by pan id; the dock affordance is the applet's own floating-only
//   title strip (PanadapterApplet::setFloatingState) rather than upstream's
//   always-present one.
void PanFloatingWindow::adoptApplet(PanadapterApplet* applet)
{
    if (!applet) { return; }
    m_applet = applet;
    // addWidget() reparents internally, so the applet goes straight from the
    // splitter to this window in one step -- no intermediate nullptr parent,
    // and therefore no transient top-level NSWindow.
    m_layout->addWidget(applet, 1);
    updateWindowTitle();
}

PanadapterApplet* PanFloatingWindow::takeApplet(QWidget* newParent)
{
    PanadapterApplet* applet = m_applet.data();
    if (!applet) { return nullptr; }
    m_layout->removeWidget(applet);
    // Straight onto the destination, never through nullptr; same reason as
    // adoptApplet.
    applet->setParent(newParent);
    m_applet = nullptr;
    return applet;
}

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

void PanFloatingWindow::updateWindowTitle()
{
    setWindowTitle(QStringLiteral("NereusSDR - Pan %1").arg(panId()));
}

// From AetherSDR src/gui/PanFloatingWindow.cpp:87-96 [@1e0718ad]
//   The close box docks rather than destroys, and the event is IGNORED so Qt
//   does not tear the window down underneath the stack. PanadapterStack::
//   dockPanadapter owns the teardown, because it also has to move the applet
//   out first -- letting Qt close the window here would delete the applet
//   with it.
//
// From Longpath src/gui/PanFloatingWindow.cpp:230-243 [@383f058]
//   Application quit closes every top-level window with a close event that
//   is not spontaneous, and one ignored close cancels the whole quit. So a
//   non-spontaneous close is accepted (the window only hides; the pan is not
//   docked on the way out, and its geometry is saved) and only a close the
//   window system sends -- the user's close box -- docks.
void PanFloatingWindow::closeEvent(QCloseEvent* event)
{
    saveWindowGeometry();
    // ABER: ein ignore() beim Beenden bricht bei Qt das GANZE Beenden ab
    // (QApplication schliesst alle Fenster der Reihe nach und gibt auf,
    // sobald eines ablehnt). Am 2026-09-24 liess sich Longpath darum mit
    // schwebendem Panadapter ueberhaupt nicht mehr beenden -- sechs
    // Versuche hintereinander, jeder endete hier mit m_shuttingDown=false,
    // spontaneous=false, und MainWindow::closeEvent lief nie. Das Schliessen
    // aller Fenster beim Beenden kommt NICHT spontan an; darum wird es
    // angenommen (versteckt nur, kein Andocken -- das Profil fragt
    // isPanFloating() und die Lage des Fensters, nicht die Sichtbarkeit).
    // Abgelehnt wird nur noch, was das Fenstersystem spontan schickt.
    if (!event->spontaneous()) {
        event->accept();
        return;
    }
    // NereusSDR: a programmatic close() is not spontaneous either, so it
    // hides the window without docking. To dock from code, call
    // requestDock() (dockRequested -> PanadapterStack::dockPanadapter).
    requestDock();
    event->ignore();
}

// From AetherSDR src/gui/PanFloatingWindow.cpp:98-112 [@1e0718ad]
void PanFloatingWindow::saveWindowGeometry()
{
    const QString pan = panId();
    if (pan.isEmpty()) { return; }
    AppSettings::instance().setValue(
        QStringLiteral("FloatingPan_%1_Geometry").arg(pan),
        QString::fromLatin1(saveGeometry().toBase64()));
}

void PanFloatingWindow::restoreWindowGeometry()
{
    const QString pan = panId();
    if (pan.isEmpty()) { return; }
    const QString geom =
        AppSettings::instance()
            .value(QStringLiteral("FloatingPan_%1_Geometry").arg(pan), QString())
            .toString();
    if (!geom.isEmpty()) {
        restoreGeometry(QByteArray::fromBase64(geom.toLatin1()));
    }
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
