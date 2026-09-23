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
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
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
    buildFloatTitleBar();          // hidden until setFloatingState(true)
    layout->addWidget(m_floatTitleBar);
    layout->addWidget(m_spectrum, 1);

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
    // toggle (default true) and apply it to the embedded spectrum immediately.
    auto& s = AppSettings::instance();
    const QString stored = s.value(QStringLiteral("Pan_%1_ExtendedView").arg(m_panId),
                                   QStringLiteral("True")).toString();
    setExtendedViewEnabled(stored == QStringLiteral("True"));
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
    // Start below the floating title strip when there is one. The overlay is
    // positioned in APPLET coordinates, so without this it lands on top of
    // the strip -- the CH / WIDE / TX badges printed over the pan name and
    // the Dock button's row.
    //
    // Keyed on m_isFloating rather than the strip's isVisible(): the float
    // sequence hides the applet while it reparents, and a hidden ancestor
    // makes isVisible() false for a child that is not itself hidden, which
    // would put the overlay back over the strip for the frame that matters.
    const int stripH = (m_isFloating && m_floatTitleBar)
                           ? m_floatTitleBar->height() : 0;
    m_statusOverlay->setGeometry(width() - hint.width() - 8 - reserved,
                                 stripH + 8,
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

// Floating-only title strip. See the header for why it is not shown docked.
//
// Shape follows AetherSDR's pan title bar (PanadapterApplet.cpp:46-83
// [@1e0718ad]): grip glyph, name on the left, control button on the right.
// Colours are NereusSDR's existing container-title tokens rather than
// upstream's theme placeholders, so a popped-out pan reads as the same kind
// of surface as a floating meter container (ContainerWidget.cpp:117-167).
void PanadapterApplet::buildFloatTitleBar()
{
    m_floatTitleBar = new QWidget(this);
    m_floatTitleBar->setFixedHeight(18);
    m_floatTitleBar->setVisible(false);
    m_floatTitleBar->setStyleSheet(QStringLiteral(
        "QWidget { background: #16202c; border-bottom: 1px solid #203040; }"));

    auto* bar = new QHBoxLayout(m_floatTitleBar);
    bar->setContentsMargins(6, 0, 4, 0);
    bar->setSpacing(4);

    auto* grip = new QLabel(QString::fromUtf8("\xe2\x8b\xae\xe2\x8b\xae"),
                            m_floatTitleBar);
    grip->setStyleSheet(QStringLiteral(
        "QLabel { background: transparent; color: #5a7085; font-size: 10px; }"));
    bar->addWidget(grip);

    auto* title = new QLabel(QStringLiteral("Pan %1").arg(m_panId),
                             m_floatTitleBar);
    title->setStyleSheet(QStringLiteral(
        "QLabel { background: transparent; color: #9fb4c8; "
        "font-size: 10px; font-weight: bold; }"));
    bar->addWidget(title);
    bar->addStretch();

    auto* dockBtn = new QPushButton(QString::fromUtf8("\xe2\x86\xa9"),
                                    m_floatTitleBar);  // U+21A9 leftwards hook
    dockBtn->setFixedSize(18, 14);
    dockBtn->setToolTip(tr("Dock this pan back into the console"));
    dockBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: #6a8090; border: none; "
        "font-size: 11px; padding: 0; }"
        "QPushButton:hover { color: #c8d8e8; }"));
    connect(dockBtn, &QPushButton::clicked, this,
            [this]() { emit dockRequested(m_panId); });
    bar->addWidget(dockBtn);
}

// From AetherSDR src/gui/PanadapterApplet.cpp:552-565 [@1e0718ad]
//   adapter: upstream flips one always-present button between pop-out and
//   dock glyphs; NereusSDR shows or hides the whole strip, since the docked
//   pan has no title bar to carry a pop-out button on.
void PanadapterApplet::setFloatingState(bool floating)
{
    m_isFloating = floating;
    if (m_floatTitleBar) {
        m_floatTitleBar->setVisible(floating);
    }
    // The status overlay is positioned against the applet rect, and the strip
    // just changed how much of that rect the spectrum owns.
    repositionStatusOverlay();
}

// Phase 3F Sub-Epic F Task 13: operator-toggleable Extended view.
// Default is true so the SpectrumWidget zoom auto-derive (Task 7-10)
// gets to decide extendedMode based on bandwidth vs DDC sample rate.
// This toggle is permission, not the actual state: at normal zoom the
// wideband stream stays off even when permission is enabled.
void PanadapterApplet::setExtendedViewEnabled(bool on)
{
    const bool changed = (m_extendedViewEnabled != on);
    m_extendedViewEnabled = on;
    if (changed) {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Pan_%1_ExtendedView").arg(m_panId),
                   on ? QStringLiteral("True") : QStringLiteral("False"));
    }
    if (m_spectrum) {
        m_spectrum->setExtendedViewAllowed(on);
    }
}

// Phase 3F Sub-Epic F Task 13: right-click context menu, originally a single
// checkable Extended view entry. Pops at the global cursor position.
void PanadapterApplet::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = buildContextMenu(this);
    menu->exec(event->globalPos());
    menu->deleteLater();
}

// Task B5 (bottom-banner + pan-menu epic, design doc s8.5): "Add slice on
// this pan" / "Float this pan" used to live on the +PAN button's dropdown
// (Phase 3F Sub-Epic D Task 10, retired in Task B4) and resolved through
// m_panStack->activePanId() -- "active pan" meaning whichever pan happened
// to be active when the operator reached a button sitting nowhere near any
// pan. A control drawn ON a pan targets THAT pan, so both move here and
// carry this applet's own panId(). Neither action exists on AetherSDR's pan
// menu (design doc s8.5), so this is NereusSDR-original, not a further port.
//
// Split out of contextMenuEvent so a test can build the menu, find +
// trigger() these two actions by their operator-facing text, and assert the
// resulting signal without exec()-ing a real nested event loop -- mirrors
// SMeterWidget::buildContextMenu() / buildContextMenuForTesting()
// (tst_smeter_widget_context_menu.cpp).
QMenu* PanadapterApplet::buildContextMenu(QObject* parent)
{
    auto* menu = new QMenu(qobject_cast<QWidget*>(parent));

    menu->addAction(tr("Add slice on this pan"), this, [this]() {
        emit addSliceRequested(panId());
    });
    menu->addAction(tr("Float this pan"), this, [this]() {
        emit floatRequested(panId());
    });
    menu->addSeparator();

    QAction* extAct = menu->addAction(tr("Extended view (wideband wings)"));
    extAct->setCheckable(true);
    extAct->setChecked(m_extendedViewEnabled);
    connect(extAct, &QAction::toggled, this, &PanadapterApplet::setExtendedViewEnabled);

    return menu;
}

} // namespace NereusSDR
