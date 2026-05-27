// no-port-check: AetherSDR-derived NereusSDR file. Pan layout manager
// (5-template QSplitter tree, active-pan tracking, float-pan signal) is
// adapted structurally from AetherSDR src/gui/PanadapterStack.{h,cpp}
// [@0cd4559]. Registered in
// docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanadapterStack.cpp  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanadapterStack.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// See PanadapterStack.h for full Modification history (NereusSDR).
// =================================================================

#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include <QVBoxLayout>
#include <QSplitter>

namespace NereusSDR {

PanadapterStack::PanadapterStack(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_rootSplitter = new QSplitter(Qt::Vertical, this);
    layout->addWidget(m_rootSplitter);

    addPanadapter(QStringLiteral("pan-0"));
}

PanadapterStack::~PanadapterStack() = default;

PanadapterApplet* PanadapterStack::addPanadapter(const QString& panId)
{
    if (m_pans.contains(panId)) {
        return m_pans[panId];
    }
    auto* applet = new PanadapterApplet(panId, this);
    m_pans[panId] = applet;
    if (m_activePanId.isEmpty()) { setActivePan(panId); }
    if (m_pans.size() == 1) {
        m_rootSplitter->addWidget(applet);
    }
    emit countChanged(m_pans.size());
    return applet;
}

void PanadapterStack::removePanadapter(const QString& panId)
{
    auto* applet = m_pans.take(panId);
    if (!applet) { return; }
    applet->deleteLater();
    emit countChanged(m_pans.size());
}

void PanadapterStack::removeAll() { /* TODO Task 5 */ }
void PanadapterStack::applyLayout(const QString&, const QStringList&) { /* TODO Task 4 */ }
PanadapterApplet* PanadapterStack::panadapter(const QString& id) const { return m_pans.value(id, nullptr); }
QList<PanadapterApplet*> PanadapterStack::allApplets() const { return m_pans.values(); }
void PanadapterStack::setActivePan(const QString& id) { if (m_activePanId != id) { m_activePanId = id; emit activePanChanged(id); } }
void PanadapterStack::floatPanadapter(const QString&) { /* TODO Task 8 */ }
void PanadapterStack::rebuildSplitters(const QString&, const QStringList&) {}
void PanadapterStack::clearSplitters() {}

} // namespace NereusSDR
