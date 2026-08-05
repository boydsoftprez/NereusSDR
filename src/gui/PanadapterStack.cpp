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
#include "gui/PanFloatingWindow.h"
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

PanadapterStack::~PanadapterStack()
{
    dockAllFloatingPans();
}

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

    // Re-seat the active id if it named the pan just destroyed. activePanId()
    // feeds "Add slice on active pan" (Ctrl+R), "Float active pan..." and
    // rebuildFftRouting's last-resort pan resolution, so a stale id points all
    // three at a pan that no longer exists. Nothing would ever repair it:
    // setActivePan's only caller is guarded on m_activePanId.isEmpty(), so a
    // stale NON-empty id is permanent.
    //
    // Clearing on the last removal is what re-arms that guard, so the next
    // pan created becomes active. Untouched when some other pan was removed --
    // re-seating on every removal would move the operator's working pan out
    // from under them.
    //
    // m_pans is a QMap, so constBegin() is the lowest surviving pan id rather
    // than an arbitrary one -- "pan-0" before "pan-1". Deterministic on
    // purpose: which pan Ctrl+R targets after a close should not depend on
    // hash ordering.
    if (m_activePanId == panId) {
        setActivePan(m_pans.isEmpty() ? QString() : m_pans.constBegin().key());
    }

    emit countChanged(m_pans.size());
}

void PanadapterStack::removeAll() { /* TODO Task 5 */ }

void PanadapterStack::applyLayout(const QString& layoutId, const QStringList& panIds)
{
    dockAllFloatingPans();
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
    else if (layoutId == QStringLiteral("2h1") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);

        auto* topRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        auto* tl = addPanadapter(panIds[0]);
        auto* tr = addPanadapter(panIds[1]);
        topRow->addWidget(tl);
        topRow->addWidget(tr);
        tl->show();
        tr->show();
        m_rootSplitter->addWidget(topRow);

        auto* bottom = addPanadapter(panIds[2]);
        m_rootSplitter->addWidget(bottom);
        bottom->show();
    }
    else if (layoutId == QStringLiteral("3v") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        for (int i = 0; i < 3; ++i) {
            auto* p = addPanadapter(panIds[i]);
            m_rootSplitter->addWidget(p);
            p->show();
        }
    }
    else if (layoutId == QStringLiteral("4v") && panIds.size() >= 4) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        for (int i = 0; i < 4; ++i) {
            auto* p = addPanadapter(panIds[i]);
            m_rootSplitter->addWidget(p);
            p->show();
        }
    }
    else if (layoutId == QStringLiteral("3h2") && panIds.size() >= 5) {
        m_rootSplitter->setOrientation(Qt::Vertical);

        auto* topRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        for (int i = 0; i < 3; ++i) {
            auto* p = addPanadapter(panIds[i]);
            topRow->addWidget(p);
            p->show();
        }
        m_rootSplitter->addWidget(topRow);

        auto* bottomRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        for (int i = 3; i < 5; ++i) {
            auto* p = addPanadapter(panIds[i]);
            bottomRow->addWidget(p);
            p->show();
        }
        m_rootSplitter->addWidget(bottomRow);
    }
}

PanadapterApplet* PanadapterStack::panadapter(const QString& id) const { return m_pans.value(id, nullptr); }
QList<PanadapterApplet*> PanadapterStack::allApplets() const { return m_pans.values(); }
SpectrumWidget* PanadapterStack::spectrum(const QString& panId) const
{
    PanadapterApplet* applet = m_pans.value(panId, nullptr);
    return applet ? applet->spectrumWidget() : nullptr;
}
void PanadapterStack::setActivePan(const QString& id) { if (m_activePanId != id) { m_activePanId = id; emit activePanChanged(id); } }

// The writer PanadapterApplet::activeSliceIndex() never had after its one-shot
// seed in addSlice. See the header for the bench defect this closes.
//
// The scan is over single-digit pan counts on a user action, and it asks each
// pan the one question that matters -- do you host this slice -- rather than
// resolving through SliceModel::panKey(). panKey is the authoritative binding
// for WHERE a slice belongs, but associatedSlices() is what
// MainWindow::sliceForPan actually reads back, so keying off the same set is
// what makes the pan's answer and this function's answer agree. Every pan that
// lists the slice is updated rather than the first one found: if two ever
// disagree, moving both is the state the operator can act on, where stopping
// at the first would leave a pan silently tuning something else.
void PanadapterStack::setActiveSliceOnHostingPan(int sliceId)
{
    const QList<PanadapterApplet*> pans = allApplets();
    for (PanadapterApplet* applet : pans) {
        if (!applet) { continue; }
        if (!applet->associatedSlices().contains(sliceId)) { continue; }
        applet->setActiveSliceIndex(sliceId);
    }
}

// Keeps associatedSlices() honest across a pan change. See the header.
void PanadapterStack::moveSliceToPan(int sliceId, const QString& destPanId)
{
    PanadapterApplet* dest = m_pans.value(destPanId, nullptr);
    if (!dest) { return; }

    const QList<PanadapterApplet*> pans = allApplets();
    for (PanadapterApplet* applet : pans) {
        if (!applet || applet == dest) { continue; }
        applet->removeSlice(sliceId);
    }
    dest->addSlice(sliceId);
}

void PanadapterStack::floatPanadapter(const QString& panId)
{
    // Sub-Epic D Task 8: detach the pan into a top-level PanFloatingWindow
    // (multi-monitor). The applet is reparented into the window's layout, so
    // ownership transfers to the window for the duration of the float. On
    // dockRequested (close or explicit redock), the layout is rebuilt from
    // m_currentLayoutId so the applet is reparented back under the stack.
    auto* applet = m_pans.value(panId, nullptr);
    if (!applet || m_floating.contains(panId)) { return; }

    // Reparent while hidden, and re-show only once the new window is up.
    //
    // The applet hosts a SpectrumWidget, which is a QRhiWidget: its render
    // context belongs to the window it lives in. Reparenting a live, visible
    // one straight into a not-yet-shown window left Qt unable to re-acquire an
    // RHI for it, and the pan froze while logging "QRhiWidget: No QRhi" once
    // per frame forever. Hiding first lets the old context tear down cleanly
    // (releaseResources clears m_rhiInitialized), so the show below drives a
    // fresh initialize() against the floating window's own surface.
    applet->hide();

    auto* floater = new PanFloatingWindow(applet, nullptr);
    m_floating[panId] = floater;

    connect(floater, &PanFloatingWindow::dockRequested, this, [this, panId]() {
        auto* taken = m_floating.take(panId);
        if (!taken) { return; }
        // Re-attach all currently-known applets to the current layout. The
        // applyLayout path detaches every applet from its parent first
        // (clearSplitters reparents back to `this` and hides) and then
        // re-adds the ones listed in m_pans.keys(), so the floated applet
        // is pulled out of the window's layout cleanly before we delete it.
        applyLayout(m_currentLayoutId, m_pans.keys());
        taken->deleteLater();
    });

    floater->show();
    // Now that the floating window exists and is mapped, bring the pan back up
    // so its QRhiWidget initializes against that window's surface.
    applet->show();

    // Re-establish the pans that stayed behind.
    //
    // Pulling a widget out of a QSplitter costs its siblings their render
    // context too: floating one pan turned the OTHER pan black and logged
    // "QRhiWidget: No QRhi" once per frame. A hide/show cycle makes Qt tear
    // the context down cleanly (releaseResources clears m_rhiInitialized) and
    // build a fresh one against the main window.
    //
    // Deliberately NOT applyLayout() here: its clearSplitters() reparents
    // EVERY applet in m_pans back under the stack, floated ones included --
    // which is exactly what the dockRequested handler above relies on, and
    // exactly wrong while a float is being set up. Doing that emptied the
    // floating window and the main window at once.
    for (auto it = m_pans.cbegin(); it != m_pans.cend(); ++it) {
        if (it.key() == panId || m_floating.contains(it.key())) { continue; }
        if (auto* other = it.value()) {
            other->hide();
            other->show();
        }
    }
}
void PanadapterStack::rebuildSplitters(const QString&, const QStringList&) {}

void PanadapterStack::dockAllFloatingPans()
{
    while (!m_floating.isEmpty()) {
        auto it = m_floating.begin();
        PanFloatingWindow* floater = it.value();
        m_floating.erase(it);
        if (!floater) { continue; }

        QObject::disconnect(floater, nullptr, this, nullptr);
        if (PanadapterApplet* applet = floater->applet()) {
            applet->hide();
            applet->setParent(this);
        }
        floater->close();
        delete floater;
    }
}

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
