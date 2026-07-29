// no-port-check: AetherSDR-derived NereusSDR file. Per-pan container
// (SpectrumWidget host, slice association) is adapted structurally from
// AetherSDR src/gui/PanadapterApplet.{h,cpp} [@0cd4559]. Registered in
// docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanadapterApplet.cpp  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanadapterApplet.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// See PanadapterApplet.h for full Modification history (NereusSDR).
// =================================================================

#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "models/SliceModel.h"
#include "core/AppSettings.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace NereusSDR {

PanadapterApplet::PanadapterApplet(const QString& panId, QWidget* parent)
    : QWidget(parent)
    , m_panId(panId)
    , m_spectrum(new SpectrumWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_spectrum);

    // Phase 3F Sub-Epic E Task 2: per-pan status overlay in top-right.
    // Positioned manually in resizeEvent so the spectrum host owns the full
    // applet area underneath. Parented to `this`, not m_spectrum, so it sits
    // above the SpectrumWidget's QRhi surface without becoming a child of it.
    m_statusOverlay = new SpectrumStatusOverlay(this);
    m_statusOverlay->raise();

    // Phase 3F: clicking anywhere in this pan makes it the active pan.
    //
    // `activated` was declared with the comment "emitted on any click within
    // applet" and had no emitter, so PanadapterStack::setActivePan was only
    // ever called for the first pan created. Every consumer of activePanId()
    // was therefore pinned to pan-0 for the session -- including "Add slice on
    // active pan", which is the ONLY route that puts a slice on a pan (the
    // per-pan +RX button is still disabled, NYI). A second pan could be
    // created but never given a slice, so it showed no VFO flag, no dial
    // frequency, and no FFT subscription.
    //
    // Ported from AetherSDR PanadapterApplet.cpp:629 [@6a142807]:
    //     if (ev->type() == QEvent::MouseButtonPress)
    //         emit activated(m_panId);
    // in eventFilter rather than mousePressEvent, because the spectrum host
    // and the overlay consume presses before they ever reach this widget.
    m_spectrum->installEventFilter(this);
    m_statusOverlay->installEventFilter(this);
    installEventFilter(this);

    connect(m_statusOverlay, &SpectrumStatusOverlay::txBadgeClicked, this,
            [this]() { emit txBadgeClicked(m_panId); });
    connect(m_statusOverlay, &SpectrumStatusOverlay::wideBadgeClicked, this,
            [this]() { emit wideBadgeClicked(m_panId); });
    connect(m_statusOverlay, &SpectrumStatusOverlay::chainTagClicked, this,
            [this](int chainIdx) { emit chainTagClicked(m_panId, chainIdx); });

    // Phase 3F Sub-Epic F Task 13: restore persisted extended-view
    // toggle (default true). The SpectrumWidget's own auto-derive
    // decides extendedMode dynamically when this is true; when this
    // is false the auto-derive is overridden to off (see
    // setExtendedViewEnabled). We avoid pushing the state to
    // m_spectrum here because setFrequencyRange / setSampleRate run
    // their own auto-derive on first wire; we only force the override
    // when the operator explicitly toggles off via the right-click
    // menu below.
    auto& s = AppSettings::instance();
    const QString stored = s.value(QStringLiteral("Pan_%1_ExtendedView").arg(m_panId),
                                   QStringLiteral("True")).toString();
    m_extendedViewEnabled = (stored == QStringLiteral("True"));
}

PanadapterApplet::~PanadapterApplet() = default;

void PanadapterApplet::addSlice(int sliceIndex)
{
    m_associatedSlices.insert(sliceIndex);
    if (m_activeSliceIndex == -1) {
        setActiveSliceIndex(sliceIndex);
    }
}

void PanadapterApplet::removeSlice(int sliceIndex)
{
    m_associatedSlices.remove(sliceIndex);
    if (m_activeSliceIndex == sliceIndex) {
        m_activeSliceIndex = m_associatedSlices.isEmpty() ? -1 : *m_associatedSlices.begin();
        emit activeSliceChanged(m_panId, m_activeSliceIndex);
    }
}

void PanadapterApplet::setActiveSliceIndex(int sliceIndex)
{
    if (m_activeSliceIndex == sliceIndex) { return; }
    m_activeSliceIndex = sliceIndex;
    // Bench-reported 2026-07-28 (Sub-Epic J): the newly-active slice's flag
    // must come to the front of THIS pan's stack -- see
    // SpectrumWidget::setFrontSliceIndex for why a plain raise() would not
    // hold. This is the single writer of m_activeSliceIndex from outside
    // (the addSlice seed path below also routes through here), so hanging
    // the z-order pin off it covers every caller, including
    // PanadapterStack::setActiveSliceOnHostingPan, without each one having
    // to remember to raise the flag itself.
    if (m_spectrum) {
        m_spectrum->setFrontSliceIndex(sliceIndex);
    }
    emit activeSliceChanged(m_panId, sliceIndex);
}

void PanadapterApplet::setCenterMhz(double mhz) { m_centerMhz = mhz; }
void PanadapterApplet::setBandwidthMhz(double bw) { m_bandwidthMhz = bw; }

// Phase 3F: any press inside this pan claims active-pan status. Watches the
// spectrum host and the overlay as well as the applet itself, because both sit
// on top of it and accept the press first.
//
// Ported from AetherSDR PanadapterApplet.cpp:628-630 [@6a142807]:
//     if (ev->type() == QEvent::MouseButtonPress)
//         emit activated(m_panId);
//     return QWidget::eventFilter(obj, ev);
//
// Observing, not intercepting: always falls through to the base implementation
// so the spectrum keeps its own click handling (tune, drag, zoom).
bool PanadapterApplet::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() == QEvent::MouseButtonPress) {
        emit activated(m_panId);
    }
    return QWidget::eventFilter(obj, ev);
}

void PanadapterApplet::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    repositionStatusOverlay();
}

QByteArrayList PanadapterApplet::statusOverlaySliceProperties()
{
    // Every SliceModel property updateStatusOverlay reads below. Keep the two
    // in step: a field painted with no entry here is a field that goes stale.
    //
    // streamIndex earns its place even though the overlay never paints it --
    // it is what the caller's chain resolution keys off, so a slice moving
    // between DDC streams changes the CH tag without touching any painted
    // property.
    //
    // sliceLetter is deliberately absent: the letter is derived from the
    // stable slice id, which cannot change for the life of the slice.
    return QByteArrayList{
        QByteArrayLiteral("frequency"),
        QByteArrayLiteral("dspMode"),
        QByteArrayLiteral("txSlice"),
        QByteArrayLiteral("diversityEnabled"),
        QByteArrayLiteral("psPaused"),
        QByteArrayLiteral("streamIndex"),
    };
}

void PanadapterApplet::updateStatusOverlay(SliceModel* slice, int chainIndex)
{
    if (!m_statusOverlay || !slice) { return; }
    // SliceModel::sliceLetter() now derives from the stable slice id, so the
    // local workaround this used to carry is gone. (It reported 'A' for every
    // slice while the letter was a stored member no one ever set.)
    m_statusOverlay->setSliceLetter(slice->sliceLetter());
    // SliceModel::frequency() returns double Hz (default 14225000.0 = 14.225 MHz).
    // Cast directly; no MHz->Hz conversion.
    m_statusOverlay->setFrequencyHz(static_cast<qint64>(slice->frequency()));
    m_statusOverlay->setMode(SliceModel::modeName(slice->dspMode()));
    // An unbound slice (chainIndex < 0) is on no chain at all. Holding the
    // last tag is honest about "unknown"; writing 0 would assert it is on
    // chain 0, which is the failure mode this parameter exists to stop.
    if (chainIndex >= 0) {
        m_statusOverlay->setChainIndex(chainIndex);
    }
    m_statusOverlay->setTxBound(slice->isTxSlice());
    m_statusOverlay->setDiversityActive(slice->diversityEnabled());
    m_statusOverlay->setPsPaused(slice->psPaused());
    // TX / DIV / PS pills all change the strip's width; re-anchor.
    repositionStatusOverlay();
}

QChar PanadapterApplet::statusSliceLetter() const
{
    return m_statusOverlay ? m_statusOverlay->sliceLetter() : QChar();
}

qint64 PanadapterApplet::statusFrequencyHz() const
{
    return m_statusOverlay ? m_statusOverlay->frequencyHz() : 0;
}

QString PanadapterApplet::statusMode() const
{
    return m_statusOverlay ? m_statusOverlay->mode() : QString();
}

int PanadapterApplet::statusChainIndex() const
{
    return m_statusOverlay ? m_statusOverlay->chainIndex() : 0;
}

// Phase 3F: WIDE pill forwarder. Kept separate from updateStatusOverlay
// because the two have different triggers and different sources: the slice
// fields refresh when the active slice changes, whereas the bypass state is
// a property of the chain feeding this pan and changes on band crossings,
// slice add/remove, wideband toggles and Filter Policy edits -- none of
// which need be the active slice, or any slice on this pan at all.
// Right-align the status strip, clear of the dBm scale strip.
//
// Must re-run whenever a pill lights or goes dark, not only on resize.
// SpectrumStatusOverlay::paintEvent grows minimumWidth() to fit the pills it
// just drew, and setGeometry clamps a narrower request up to that minimum --
// expanding the widget RIGHTWARD from its fixed x. So a strip positioned while
// only "CH 0" was showing crept back under the dBm range arrows the moment the
// TX or WIDE pill appeared. Re-anchoring from the current hint puts the right
// edge back where it belongs.
void PanadapterApplet::repositionStatusOverlay()
{
    if (!m_statusOverlay) { return; }
    const QSize hint = m_statusOverlay->sizeHint();
    // Clear of the dBm scale strip: its range up/down arrows sit at the top of
    // that column, and the status strip accepts mouse events for its own
    // badges, so any overlap makes the arrows both hard to read and hard to
    // hit.
    const int reserved = m_spectrum ? m_spectrum->reservedRightEdgeWidth() : 0;
    m_statusOverlay->setGeometry(width() - hint.width() - 8 - reserved, 8,
                                 hint.width(), hint.height());
}

void PanadapterApplet::setWideBpf(bool wide, const QString& reason)
{
    if (!m_statusOverlay) { return; }
    m_statusOverlay->setWideBpf(wide, reason);
    // The WIDE pill changes the strip's width; re-anchor its right edge.
    repositionStatusOverlay();
}

bool PanadapterApplet::wideBpf() const
{
    return m_statusOverlay && m_statusOverlay->wideBpf();
}

QString PanadapterApplet::wideReason() const
{
    return m_statusOverlay ? m_statusOverlay->wideReason() : QString();
}

// Phase 3F Sub-Epic F Task 13: operator-toggleable Extended view.
// Default is true so the SpectrumWidget zoom auto-derive (Task 7-10)
// gets to decide extendedMode based on bandwidth vs DDC sample rate.
// When false, we force m_spectrum->setExtendedMode(false) so the
// wideband stream stays off regardless of zoom; future toggles back
// to true let the next setFrequencyRange / setSampleRate call re-run
// the auto-derive and pick the right state for the current zoom.
void PanadapterApplet::setExtendedViewEnabled(bool on)
{
    if (m_extendedViewEnabled == on) { return; }
    m_extendedViewEnabled = on;
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("Pan_%1_ExtendedView").arg(m_panId),
               on ? QStringLiteral("True") : QStringLiteral("False"));
    if (m_spectrum && !on) {
        // Force extended mode off (operator override). Turning the
        // toggle back on doesn't immediately push true because the
        // auto-derive should re-decide based on current zoom; the next
        // setFrequencyRange call inside the widget will rebase it.
        m_spectrum->setExtendedMode(false);
    }
}

// Phase 3F Sub-Epic F Task 13: right-click context menu with a single
// checkable Extended view entry. Pops at the global cursor position.
void PanadapterApplet::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* extAct = menu.addAction(tr("Extended view (wideband wings)"));
    extAct->setCheckable(true);
    extAct->setChecked(m_extendedViewEnabled);
    connect(extAct, &QAction::toggled, this, &PanadapterApplet::setExtendedViewEnabled);
    menu.exec(event->globalPos());
}

} // namespace NereusSDR
