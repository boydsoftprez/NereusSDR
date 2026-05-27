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
#include "core/AppSettings.h"
#include <QVBoxLayout>
#include <QSplitter>
#include <QSet>
#include <QStringList>

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

void PanadapterStack::applyLayout(const QString& layoutId, const QStringList& panIds)
{
    clearSplitters();

    // Retire orphan pans not referenced by the new layout. Without this,
    // switching from a layout that uses "pan-0" to one keyed on different ids
    // (e.g. "p0..p3" in 2x2 tests) would leak the prior pans into m_pans and
    // distort count(). NereusSDR-specific addition; AetherSDR's layout swap
    // assumes the caller passes the canonical id set.
    const QSet<QString> wanted(panIds.constBegin(), panIds.constEnd());
    const QList<QString> existing = m_pans.keys();
    for (const QString& id : existing) {
        if (!wanted.contains(id)) {
            removePanadapter(id);
        }
    }

    m_currentLayoutId = layoutId;

    if (layoutId == QStringLiteral("1") && !panIds.isEmpty()) {
        auto* applet = addPanadapter(panIds[0]);
        m_rootSplitter->setOrientation(Qt::Vertical);
        m_rootSplitter->addWidget(applet);
        applet->show();
    }
    else if (layoutId == QStringLiteral("2v") && panIds.size() >= 2) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        auto* a = addPanadapter(panIds[0]);
        auto* b = addPanadapter(panIds[1]);
        m_rootSplitter->addWidget(a);
        m_rootSplitter->addWidget(b);
        a->show();
        b->show();
    }
    else if (layoutId == QStringLiteral("2h") && panIds.size() >= 2) {
        m_rootSplitter->setOrientation(Qt::Horizontal);
        auto* a = addPanadapter(panIds[0]);
        auto* b = addPanadapter(panIds[1]);
        m_rootSplitter->addWidget(a);
        m_rootSplitter->addWidget(b);
        a->show();
        b->show();
    }
    else if (layoutId == QStringLiteral("12h") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        auto* top = addPanadapter(panIds[0]);
        m_rootSplitter->addWidget(top);
        top->show();

        auto* bottomSplitter = new QSplitter(Qt::Horizontal, m_rootSplitter);
        auto* bl = addPanadapter(panIds[1]);
        auto* br = addPanadapter(panIds[2]);
        bottomSplitter->addWidget(bl);
        bottomSplitter->addWidget(br);
        bl->show();
        br->show();
        m_rootSplitter->addWidget(bottomSplitter);

        m_rootSplitter->setStretchFactor(0, 2);  // wide top gets 2x weight
        m_rootSplitter->setStretchFactor(1, 1);
    }
    else if (layoutId == QStringLiteral("2x2") && panIds.size() >= 4) {
        m_rootSplitter->setOrientation(Qt::Vertical);

        auto* topRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        auto* tl = addPanadapter(panIds[0]);
        auto* tr = addPanadapter(panIds[1]);
        topRow->addWidget(tl);
        topRow->addWidget(tr);
        tl->show();
        tr->show();

        auto* bottomRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        auto* bl = addPanadapter(panIds[2]);
        auto* br = addPanadapter(panIds[3]);
        bottomRow->addWidget(bl);
        bottomRow->addWidget(br);
        bl->show();
        br->show();

        m_rootSplitter->addWidget(topRow);
        m_rootSplitter->addWidget(bottomRow);
    }
}

PanadapterApplet* PanadapterStack::panadapter(const QString& id) const { return m_pans.value(id, nullptr); }
QList<PanadapterApplet*> PanadapterStack::allApplets() const { return m_pans.values(); }
void PanadapterStack::setActivePan(const QString& id) { if (m_activePanId != id) { m_activePanId = id; emit activePanChanged(id); } }
void PanadapterStack::floatPanadapter(const QString&) { /* TODO Task 8 */ }
void PanadapterStack::rebuildSplitters(const QString&, const QStringList&) {}

void PanadapterStack::clearSplitters()
{
    // Detach all applets from the current splitter tree but do not delete them
    // (they live in m_pans and may be re-attached by the new layout).
    for (auto* applet : m_pans.values()) {
        applet->setParent(this);
        applet->hide();
    }
    // Tear down the splitter tree and rebuild from scratch.
    if (m_rootSplitter) {
        layout()->removeWidget(m_rootSplitter);
        m_rootSplitter->deleteLater();
    }
    m_rootSplitter = new QSplitter(Qt::Vertical, this);
    layout()->addWidget(m_rootSplitter);
}

// Phase 3F Sub-Epic D Task 6: persist splitter geometry across launches.
// Storage layout: PanLayoutId + PanSplitter0Sizes (root) + PanSplitter1Sizes /
// PanSplitter2Sizes (nested splitters for 12h / 2x2). Sub-Epic D ships only
// root-splitter persistence; nested splitter persistence may be wired in
// Sub-Epic H polish if bench feedback demands it.
void PanadapterStack::saveSplitterState()
{
    if (!m_rootSplitter) { return; }
    auto& s = AppSettings::instance();
    QStringList parts;
    const QList<int> sizes = m_rootSplitter->sizes();
    for (int sz : sizes) {
        parts << QString::number(sz);
    }
    s.setValue(QStringLiteral("PanSplitter0Sizes"), parts.join(QStringLiteral(",")));
    s.setValue(QStringLiteral("PanLayoutId"), m_currentLayoutId);
}

void PanadapterStack::restoreSplitterState()
{
    if (!m_rootSplitter) { return; }
    auto& s = AppSettings::instance();
    const QString raw = s.value(QStringLiteral("PanSplitter0Sizes"), QString()).toString();
    if (raw.isEmpty()) { return; }
    QList<int> sizes;
    const QStringList parts = raw.split(QStringLiteral(","), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        sizes << part.toInt();
    }
    if (!sizes.isEmpty()) {
        m_rootSplitter->setSizes(sizes);
    }
}

QList<int> PanadapterStack::rootSplitterSizes() const
{
    if (!m_rootSplitter) { return {}; }
    return m_rootSplitter->sizes();
}

void PanadapterStack::rootSplitterSetSizesForTest(const QList<int>& sizes)
{
    if (m_rootSplitter) {
        m_rootSplitter->setSizes(sizes);
    }
}

} // namespace NereusSDR
