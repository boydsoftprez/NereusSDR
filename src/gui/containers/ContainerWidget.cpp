// =================================================================
// src/gui/containers/ContainerWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucMeter.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12 - A drag moves a floating window and never resizes it: the
//                 size at the press is put back on every pointer move and
//                 at the end of the drag when it is found different, with
//                 one warning per drag naming the sizes and the screen
//                 (a window dragged across the join of two screens grew
//                 without bound on Windows). A resize by the grip or an
//                 edge leaves a window no larger than all the screens
//                 together. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - Workspace preview only: the Options menu of a window says
//                 Return, as its body menu and header do. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: the body menu starts with Pop Out
//                 (Return on a window); a window's header shows the pin,
//                 and the pin and the Options menu's Pin on Top report the
//                 choice so it is saved. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: while arranging, a free or
//                 floating container's header drag reports through the
//                 edge drag signals as a strip container's does, follows
//                 the pointer without being held inside the pane, and
//                 leaves the drop to the manager; Escape or a lost mouse
//                 grab puts it back. Ctrl no longer steps a drag or a
//                 resize by 10 px there (the 8 px grid is the one snap
//                 aid). The menus tick "Arrange Workspace" from the
//                 switch, not from a Shift hold. Arrange's outline no
//                 longer paints a floating window's background over the
//                 container in it. With the look off
//                 dragging, the Ctrl step and the menus are unchanged.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: a header is shown or hidden. The
//                 header's eye button and the body menu's "Keep Header
//                 Showing" switch it, Arrange draws a hidden header over
//                 the content, and the look's hover timers, fade and focus
//                 rule, its Header submenus and "Reveal controls" are gone.
//                 With the look off the three header modes, their menus and
//                 "Reveal controls" are unchanged. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: the Options menu has no Lock item,
//                 the header no padlock, and nothing in the look's chrome
//                 or menu is disabled for Lock. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Workspace preview only: the header options and source menus
//                 live on the heap under the container too, and no menu
//                 touches the container after an action removed it. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - The right-click menu lives on the heap under the container
//                 (OwnedMenu.h): an action that removes the container while
//                 the menu is open no longer frees a menu on the stack (JJ's
//                 crash removing a popped-out container). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Workspace preview only: the content host learns how far the
//                 overlaid group header reaches into it, so an overlaid
//                 entry header sits below it. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The minimum width in every dock mode is the larger of
//                 260 px and what the content needs (setContentMinimumWidth),
//                 so a docked, overlaid or floating container never cuts its
//                 applets. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - Workspace preview only: the header's hide button hides the
//                 entry or container (Containers > Hidden brings it back).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: Arrange's outline and the Reveal
//                 shadow let clicks through as native views (macOS), so
//                 the header under them answers. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the source menu ticks the slice
//                 the chip shows, not the legacy rxSource key. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Workspace preview only: the header source chip opens the
//                 source menu (slices A to D, Active slice) and emits
//                 sourceRequested. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-02 — Effective contextual draft properties and portable settings by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Atomic container arrangement and reserved chrome by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-21: the receiver source names slices A to D (saved
//                 1..4, so a saved RX1 / RX2 reads as slice A / B), and an
//                 unavailable button's reason is relayed. J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 / R-R3-21 fix wave: a saved receiver value is
//                 clamped to slices A to D on load. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-49: the title bar uses the native open / closed hand
//                 cursors instead of Qt's drawn four-way move cursor, which
//                 crashed Qt 6.11.0 on macOS. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-28 - The VFO display's filter right-click goes out as
//                 vfoFilterContextRequested. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-06 - Radio speaker plan Task 5 (R-SPK-19, D7): the pin-on-top
//                 button shows the app's own pin / pinned icons (AppIcon) in
//                 place of colour emoji text. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-10-07 - Workspace preview only: the compact group header in place
//                 of the 22 px title bar slot (Always in the layout, Reveal
//                 as a zero-space overlay after 120 ms in the top band,
//                 Hidden), its options menu, Arrange presentation (header
//                 accent, dashed outline, inside corner and edge resize
//                 handles) and the glass plate. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Workspace preview only: Move to Edge and Place Free,
//                 the panel-docked header drag toward an edge, Escape
//                 cancels a drag, the optional 8 px alignment at release
//                 (Alt bypasses it), the floating resize corner and the
//                 opaque panel surface behind a free or floating container.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The container carries the background the user chose as
//                 WorkspacePreview::kUserBackgroundProperty, so a meter
//                 inside it shows that colour in place of the panel role
//                 (workspace preview only). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-09 - contentBackground() and userBackground() hold the content
//                 background and the user's colour, so the Container
//                 Settings preview paints through the same path (workspace
//                 preview only). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - Arrange's dashed outline is masked to its 1 px frame:
//                 unmasked, on Windows it hid the native meters under it.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - The two resize edge zones draw nothing in a window
//                 either: the window's background made each an opaque 4 px
//                 strip over the outline and the end of the header.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - Workspace preview only: a popped-out object's window
//                 shows its resize corner (and the bottom edge while
//                 arranging) although its height follows its content. A
//                 resize in height turns auto height off; the window is
//                 never shorter than its content needs (JJ, 2026-10-11).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Task 36: the VFO display's frequency requests name their
//                 slice and go to MainWindow directly (no relay here).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

/*  ucMeter.cs

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

#include "ContainerWidget.h"
#include "ContainerContentHost.h"
#include "gui/widgets/AppIcon.h"
#include <QMenu>
#include "gui/containers/OwnedMenu.h"
#include <QContextMenuEvent>
#include <QTimer>
#include <QKeyEvent>
#include "FloatingContainer.h"
#include "core/BoardCapabilities.h"
#include "core/LogCategories.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/TuneStepButtonItem.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/VoiceRecordPlayItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/styles/WorkspaceStyle.h"
#include "WorkspaceHeader.h"
#include "EdgeDockStrip.h"

#include <QUuid>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QApplication>
#include <QMap>
#include <QStringList>
#include <QCursor>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QScreen>
#include <algorithm>

namespace NereusSDR {

namespace {

// Workspace preview: the shadow under an overlay header,
// rgba(0,0,0,110) fading to transparent over kOverlayShadowPx.
constexpr int kOverlayShadowAlpha = 110;
// Workspace preview: Arrange's dashed outline, the accent at 60 %.
constexpr int kArrangeOutlineAlpha = 153;
// The legacy default content background; in the preview the glass plate
// shows through instead.
constexpr QRgb kLegacyContentBackground = 0xff0f0f1a;

class OverlayShadow final : public QWidget {
public:
    explicit OverlayShadow(QWidget* parent) : QWidget(parent)
    {
        // Native on macOS like every sibling here: it must pass clicks
        // through as a view, not only as a widget.
        WorkspacePreview::passInputThrough(this);
        hide();
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0.0, QColor(0, 0, 0, kOverlayShadowAlpha));
        g.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(rect(), g);
    }
};

class ArrangeOutline final : public QWidget {
    static constexpr int kFrameNearPx = 1;
    static constexpr int kFrameFarPx = 2;
public:
    explicit ArrangeOutline(QWidget* parent) : QWidget(parent)
    {
        // Covers the whole container in Arrange and is raised over the
        // header: as a native view it took every click on macOS.
        WorkspacePreview::passInputThrough(this);
        // A window a container floats in gives every widget in it its
        // background, and this one lies over the header and the content.
        setStyleSheet(QStringLiteral("background: transparent;"));
        setObjectName(QStringLiteral("containerArrangeOutline"));
        hide();
    }
protected:
    // Only the frame the outline draws belongs to it. Over a native meter
    // Qt makes this widget a native window, and on Windows a child window
    // is not blended with the swapchain of the sibling under it: unmasked,
    // its inside showed the window's backing store in place of the meter.
    // The frame is 1 px at the left and top and 2 px at the right and
    // bottom: on a 2x screen the 1 px pen along x = width - 1 and
    // y = height - 1 is two device pixels centered on that line, so it
    // reaches half a pixel further in.
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        const QRegion frame = QRegion(rect()).subtracted(
            QRegion(rect().adjusted(kFrameNearPx, kFrameNearPx, -kFrameFarPx, -kFrameFarPx)));
        if (frame.isEmpty()) {
            clearMask();
        } else if (mask() != frame) {
            setMask(frame);
        }
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        QColor c(Workspace::role("accent", Workspace::kAccent));
        c.setAlpha(kArrangeOutlineAlpha);
        p.setPen(QPen(c, 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }
};

// Logical size of the pin-on-top icon inside its 22 x 22 title-bar button.
constexpr int kPinIconPx = 16;

} // namespace

ContainerWidget::ContainerWidget(QWidget* parent)
    : QWidget(parent)
{
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    setMinimumSize(kMinContainerWidth, kMinContainerHeight);
    // No user background yet: the content takes the container's role.
    setProperty(WorkspacePreview::kUserBackgroundProperty, QColor());
    setMouseTracking(true);
    buildUI();
    updateTitleBar();
    updateTitle();
    setupBorder();
    storeLocation();

    // Default placeholder content — replaced by setContent() in later phases
    auto* placeholder = new QLabel(QStringLiteral("Container"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QStringLiteral(
        "color: #405060; font-size: 14px; background: transparent;"));
    if (WorkspacePreview::enabled()) {
        placeholder->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 14px; background: transparent;")
            .arg(Workspace::role("text-inactive", "#405060")));
    }
    setContent(placeholder);

    qCDebug(lcContainer) << "Container created:" << m_id;
}

ContainerWidget::~ContainerWidget()
{
    if (m_structuredChrome) {
        qApp->removeEventFilter(this);
    }
    qCDebug(lcContainer) << "Container destroyed:" << m_id;
}

void ContainerWidget::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Title bar — hidden by default, shown on hover (Thetis ucMeter.cs:156)
    m_titleBar = new QWidget(this);
    m_titleBar->setFixedHeight(22);
    m_titleBar->setVisible(false);
    m_titleBar->setStyleSheet(QStringLiteral(
        "background: #1a2a3a; border-bottom: 1px solid #203040;"));
    const bool workspaceLook = WorkspacePreview::enabled();
    if (workspaceLook) {
        // Workspace preview: Longpath's raised title bar, scoped to the bar
        // itself so its borders do not cascade onto the label.
        m_titleBar->setStyleSheet(QStringLiteral("QWidget#containerTitleBar { %1 }")
                                      .arg(Workspace::titleBarStyle()));
    }

    auto* barLayout = new QHBoxLayout(m_titleBar);
    barLayout->setContentsMargins(4, 0, 0, 0);
    barLayout->setSpacing(2);

    // Title label — "RX1" + notes (Thetis ucMeter.cs:625-639)
    m_titleLabel = new QLabel(QStringLiteral("RX1"), m_titleBar);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "color: #c8d8e8; font-size: 11px; font-weight: bold; background: transparent;"));
    if (workspaceLook) {
        m_titleLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; font-weight: bold; background: transparent;")
            .arg(Workspace::role("title-text", "#c8d8e8")));
    }
    // Native hand cursors only. Qt 6.11.0 on macOS draws SizeAllCursor (and
    // WaitCursor / BusyCursor) from its own ICC-tagged PNGs, and
    // QImage::toCGImage() frees the colour space before CGImageCreate uses
    // it: EXC_BREAKPOINT under QCocoaCursor::createCursorData, reached from
    // this title bar on hover (operator's Mac, 2026-09-25 and 2026-09-26).
    // Native NSCursor shapes never take that path. Open hand on hover,
    // closed hand while dragging (beginDrag / endDrag).
    // Guarded by scripts/verify-no-image-cursors.py and tst_native_cursors.
    m_titleLabel->setCursor(Qt::OpenHandCursor);
    barLayout->addWidget(m_titleLabel, 1);

    const QString btnStyle = workspaceLook
        ? QStringLiteral(
              "QPushButton { background: transparent; border: none; color: %1;"
              "  font-size: 11px; padding: 2px 4px; }"
              "QPushButton:hover { background: %2; color: %3; }")
              .arg(Workspace::role("text-secondary", "#8090a0"),
                   Workspace::role("button-hover", "#2a3a4a"),
                   Workspace::role("text", "#c8d8e8"))
        : QStringLiteral(
              "QPushButton { background: transparent; border: none; color: #8090a0;"
              "  font-size: 11px; padding: 2px 4px; }"
              "QPushButton:hover { background: #2a3a4a; color: #c8d8e8; }");

    // Axis lock button (overlay-docked only)
    m_btnAxis = new QPushButton(QStringLiteral("\u2196"), m_titleBar);
    m_btnAxis->setFixedSize(22, 22);
    m_btnAxis->setToolTip(QStringLiteral("Axis lock (click to cycle, right-click reverse)"));
    m_btnAxis->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnAxis);

    // Pin-on-top button (floating only)
    m_btnPin = new QPushButton(m_titleBar);
    m_btnPin->setObjectName(QStringLiteral("ContainerPinButton"));
    AppIcon::apply(m_btnPin, QStringLiteral("pin"), kPinIconPx);
    m_btnPin->setFixedSize(22, 22);
    m_btnPin->setToolTip(QStringLiteral("Pin on top"));
    m_btnPin->setStyleSheet(btnStyle);
    m_btnPin->setVisible(false);
    barLayout->addWidget(m_btnPin);

    // Float/Dock toggle
    m_btnFloat = new QPushButton(QStringLiteral("\u2197"), m_titleBar);
    m_btnFloat->setFixedSize(22, 22);
    m_btnFloat->setToolTip(QStringLiteral("Float / Dock"));
    m_btnFloat->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnFloat);

    // Settings gear
    m_btnSettings = new QPushButton(QStringLiteral("\u2699"), m_titleBar);
    m_btnSettings->setFixedSize(22, 22);
    m_btnSettings->setToolTip(QStringLiteral("Container settings"));
    m_btnSettings->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnSettings);

    m_titleBar->setObjectName(QStringLiteral("containerTitleBar"));
    m_headerSlot = new QWidget(this);
    m_headerSlot->setObjectName(QStringLiteral("containerHeaderExtent"));
    m_headerSlot->setFixedHeight(kTitleBarHeight);
    m_headerSlot->hide();
    auto *headerLayout = new QVBoxLayout(m_headerSlot);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->addWidget(m_titleBar);
    mainLayout->addWidget(m_headerSlot);
    mainLayout->addWidget(m_titleBar);

    // Content holder — layout slot for setContent()
    m_contentHolder = new QWidget(this);
    m_contentHolder->setMouseTracking(true);
    m_contentHolder->setStyleSheet(QStringLiteral("background: #0f0f1a;"));
    if (workspaceLook) {
        // Selector-less on purpose, like the original: the content's
        // descendants inherit this background.
        m_contentHolder->setStyleSheet(QStringLiteral("background: %1;")
                                           .arg(contentBackground(QColor::fromRgba(kLegacyContentBackground)).name()));
    }
    new QVBoxLayout(m_contentHolder);
    m_contentHolder->layout()->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_contentHolder, 1);

    // Resize grip (bottom-right, hidden until hover)
    if (workspaceLook) {
        // Workspace preview: three diagonals of dots, inside the bounds,
        // shown while Arrange is on.
        m_resizeGrip = new WorkspaceCornerGrip(this);
        m_resizeGrip->setObjectName(QStringLiteral("containerResizeGrip"));
    } else {
        m_resizeGrip = new QWidget(this);
        m_resizeGrip->setObjectName(QStringLiteral("containerResizeGrip"));
        m_resizeGrip->setFixedSize(12, 12);
        m_resizeGrip->setCursor(Qt::SizeFDiagCursor);
        m_resizeGrip->setStyleSheet(QStringLiteral(
            "background: #405060; border-radius: 2px;"));
    }
    m_resizeGrip->setVisible(false);

    // Wire button signals
    connect(m_btnFloat, &QPushButton::clicked, this, [this]() {
        if (isFloating()) {
            if (m_popOutShell) {
                emit returnContainerRequested();
                return;
            }
            emit dockRequested();
        } else {
            emit floatRequested();
        }
    });

    connect(m_btnAxis, &QPushButton::clicked, this, [this]() {
        cycleAxisLock(QApplication::keyboardModifiers() & Qt::ShiftModifier);
    });

    connect(m_btnPin, &QPushButton::clicked, this, [this]() {
        setPinOnTop(!m_pinOnTop);
    });

    connect(m_btnSettings, &QPushButton::clicked, this, [this]() {
        emit settingsRequested();
    });

    // Event filters for title bar drag + resize grip + hover detection
    m_titleBar->installEventFilter(this);
    m_titleLabel->installEventFilter(this);
    m_resizeGrip->installEventFilter(this);
    m_contentHolder->installEventFilter(this);

    if (workspaceLook) {
        buildWorkspaceHeader();
    }
}

void ContainerWidget::buildWorkspaceHeader()
{
    m_groupHeader = new WorkspaceGroupHeader(this);
    m_groupHeader->hide();
    m_groupHeader->installEventFilter(this);
    static_cast<QVBoxLayout*>(layout())->insertWidget(0, m_groupHeader);
    connect(m_groupHeader, &WorkspaceGroupHeader::popOutClicked, this, [this] {
        m_btnFloat->click();
    });
    connect(m_groupHeader, &WorkspaceGroupHeader::optionsClicked, this,
            &ContainerWidget::showWorkspaceOptions);
    connect(m_groupHeader, &WorkspaceGroupHeader::sourceClicked, this,
            &ContainerWidget::showSourceMenu);
    // The hide button: the same as the menu's "Hide container".
    connect(m_groupHeader, &WorkspaceGroupHeader::hideClicked, this,
            &ContainerWidget::hideContainerRequested);
    // The pin: the same as the Options menu's "Pin on Top".
    connect(m_groupHeader, &WorkspaceGroupHeader::pinToggled, this,
            &ContainerWidget::requestPinOnTop);
    // The eye: the same choice as the body menu's "Keep Header Showing".
    // The button shows the committed mode, so a commit that is refused
    // leaves it as it was.
    connect(m_groupHeader, &WorkspaceGroupHeader::keepShownToggled, this, [this](bool keepShown) {
        const QPointer<ContainerWidget> self(this);
        emit headerModeRequested(keepShown ? HeaderMode::Shown : HeaderMode::Hidden);
        if (self && m_groupHeader) {
            m_groupHeader->setKeepShown(m_headerMode == HeaderMode::Shown);
        }
    });

    // A hidden header drawn for Arrange is an overlay: slightly see-through,
    // with a shadow under it. Neither is animated.
    m_headerOpacity = new QGraphicsOpacityEffect(m_groupHeader);
    m_headerOpacity->setOpacity(kOverlayOpacity);
    m_headerOpacity->setEnabled(false);
    m_groupHeader->setGraphicsEffect(m_headerOpacity);

    m_overlayShadow = new OverlayShadow(this);
    m_arrangeFrame = new ArrangeOutline(this);
    m_edgeRight = new QWidget(this);
    m_edgeRight->setObjectName(QStringLiteral("containerResizeEdgeRight"));
    m_edgeRight->setCursor(Qt::SizeHorCursor);
    m_edgeBottom = new QWidget(this);
    m_edgeBottom->setObjectName(QStringLiteral("containerResizeEdgeBottom"));
    m_edgeBottom->setCursor(Qt::SizeVerCursor);
    for (QWidget* edge : {m_edgeRight, m_edgeBottom}) {
        // Nothing is drawn on a resize edge. The window a container floats
        // in gives every widget in it its background (FloatingContainer's
        // selector-less sheet): without this each edge was an opaque 4 px
        // strip over the outline, the end of the header and the content.
        edge->setStyleSheet(QStringLiteral("background: transparent;"));
        edge->hide();
        edge->installEventFilter(this);
    }
}

int ContainerWidget::headerExtent() const
{
    if (!m_groupHeader) {
        return kTitleBarHeight;
    }
    if (!m_structuredChrome) {
        return 0;
    }
    return (m_headerMode == HeaderMode::Shown || m_minimised) ? WorkspaceGroupHeader::kHeight : 0;
}

void ContainerWidget::setArrangeActive(bool active)
{
    if (m_arrangeActive == active) {
        return;
    }
    m_arrangeActive = active;
    // Leaving Arrange restores each container's own choice.
    if (!active) {
        m_recoverChrome = false;
    }
    updateChrome();
}

void ContainerWidget::setWorkspaceTitle(const QString& title)
{
    if (m_groupHeader) {
        m_groupHeader->setTitle(title);
    }
}

void ContainerWidget::setWorkspaceSource(const QString& text, const QColor& colour)
{
    if (m_groupHeader) {
        m_groupHeader->setSource(text, colour);
    }
}

void ContainerWidget::updateWorkspaceChrome()
{
    m_titleBar->setVisible(false);
    // A header is drawn when it is kept shown, when the container is
    // minimised, or while arranging. A hidden header drawn for Arrange lies
    // over the top of the content: it takes no layout room, so nothing
    // shifts.
    const bool inLayout = m_headerMode == HeaderMode::Shown || m_minimised;
    const bool overlayShown = !inLayout && m_arrangeActive;
    auto* box = static_cast<QVBoxLayout*>(layout());
    const bool placedInLayout = box->indexOf(m_groupHeader) >= 0;
    if (inLayout && !placedInLayout) {
        box->insertWidget(0, m_groupHeader);
    } else if (!inLayout && placedInLayout) {
        box->removeWidget(m_groupHeader);
    }
    m_headerOpacity->setEnabled(!inLayout);
    m_groupHeader->setVisible(inLayout || overlayShown);
    m_overlayShadow->setVisible(overlayShown);
    m_groupHeader->setKeepShown(m_headerMode == HeaderMode::Shown);
    m_groupHeader->setArrangeAccent(m_arrangeActive);
    m_groupHeader->setFloating(isFloating());
    m_groupHeader->setPinned(m_pinOnTop);

    const bool resizable = m_arrangeActive && !isPanelDocked();
    // A frameless floating window keeps its resize corner, inside its bounds.
    const bool floatingCorner = isFloating();
    // A popped-out object's window keeps both although its height follows
    // its content: a resize in height makes the height the operator's.
    const bool heightIsSet = !m_autoHeight || operatorSizesHeight();
    m_resizeGrip->setVisible((resizable || floatingCorner) && heightIsSet);
    m_edgeRight->setVisible(resizable);
    m_edgeBottom->setVisible(resizable && heightIsSet);
    m_arrangeFrame->setVisible(m_arrangeActive);
    m_btnFloat->setEnabled(true);
    m_btnAxis->setEnabled(true);
    m_btnPin->setEnabled(true);
    placeWorkspaceOverlays();
}

void ContainerWidget::placeWorkspaceOverlays()
{
    if (!m_groupHeader) {
        return;
    }
    const auto place = [](QWidget* w, const QRect& r) {
        if (w->geometry() != r) {
            w->setGeometry(r);
        }
        if (w->isVisible()) {
            w->raise();
        }
    };
    const bool overlaid = static_cast<QVBoxLayout*>(layout())->indexOf(m_groupHeader) < 0;
    if (overlaid) {
        place(m_groupHeader, QRect(0, 0, width(), WorkspaceGroupHeader::kHeight));
    }
    if (auto* host = qobject_cast<ContainerContentHost*>(m_content)) {
        // How far the overlaid group header reaches into the content.
        const int reach = overlaid && !m_groupHeader->isHidden()
            ? WorkspaceGroupHeader::kHeight - host->mapTo(this, QPoint()).y()
            : 0;
        host->setTopOverlayInset(reach);
    }
    place(m_overlayShadow, QRect(0, WorkspaceGroupHeader::kHeight, width(), kOverlayShadowPx));
    place(m_arrangeFrame, rect());
    place(m_edgeRight, QRect(width() - kArrangeEdgePx, 0, kArrangeEdgePx, height()));
    place(m_edgeBottom, QRect(0, height() - kArrangeEdgePx, width(), kArrangeEdgePx));
    place(m_resizeGrip, QRect(width() - WorkspaceCornerGrip::kSize, height() - WorkspaceCornerGrip::kSize,
                              WorkspaceCornerGrip::kSize, WorkspaceCornerGrip::kSize));
}

void ContainerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_groupHeader) {
        placeWorkspaceOverlays();
    }
    if (m_dragging && m_dragHoldSize.isValid() && !m_restoringDragSize && !m_dragSizeCheckQueued
        && !m_dragSizeQueuedRestoreDone) {
        // The window was resized while a drag moves it (the platform, on a
        // change of screen). Its size is never set from inside this event:
        // one check is queued, and the next pointer move checks too.
        m_dragSizeCheckQueued = true;
        QMetaObject::invokeMethod(this, [this] {
            m_dragSizeCheckQueued = false;
            if (!m_dragSizeQueuedRestoreDone && holdDragSize()) {
                m_dragSizeQueuedRestoreDone = true;
            }
        }, Qt::QueuedConnection);
    }
}

void ContainerWidget::populateWorkspaceMenu(QMenu& menu)
{
    if (isOverlayDocked()) {
        menu.addAction(tr("Axis Lock %1 (cycle)").arg(m_btnAxis->text()), this,
                       [this] { cycleAxisLock(false); });
    }
    if (isFloating()) {
        auto* pin = menu.addAction(tr("Pin on Top"), this, [this] { requestPinOnTop(!m_pinOnTop); });
        pin->setCheckable(true);
        pin->setChecked(m_pinOnTop);
    }
    const QString floatText = isFloating()
        ? (m_popOutShell ? tr("Return All Objects") : tr("Return")) : tr("Pop Out");
    menu.addAction(floatText, this, [this] { m_btnFloat->click(); });
    menu.addAction(tr("Container Settings…"), this, [this] { emit settingsRequested(); });
    menu.addSeparator();
    // Docks in the strip for that edge; Place Free puts the container over
    // the spectrum, as a drop there does while arranging.
    auto* edge = menu.addMenu(tr("Move to Edge"));
    const std::pair<DockEdge, QString> edges[] = {{DockEdge::Left, tr("Left")},
                                                  {DockEdge::Right, tr("Right")},
                                                  {DockEdge::Top, tr("Top")},
                                                  {DockEdge::Bottom, tr("Bottom")}};
    for (const auto& [value, name] : edges) {
        auto* action = edge->addAction(name, this, [this, value] { emit moveToEdgeRequested(value); });
        action->setCheckable(true);
        action->setChecked(isPanelDocked() && m_dockEdge == value);
    }
    if (!isOverlayDocked()) {
        auto* free = menu.addAction(tr("Place Free"), this, [this] { emit placeFreeRequested(); });
        free->setToolTip(tr("Place over the spectrum, inside the main window"));
    }
    auto* arrange = menu.addAction(tr("Arrange Workspace"), this,
                                   [this] { emit arrangeWorkspaceRequested(!m_arrangeSwitchOn); });
    arrange->setCheckable(true);
    // The switch, not a Shift hold: choosing it while Shift is held turns
    // the switch on.
    arrange->setChecked(m_arrangeSwitchOn);
    if (auto* host = qobject_cast<ContainerContentHost*>(m_content)) {
        menu.addSeparator();
        host->addContentsMenu(menu);
    }
}

void ContainerWidget::showWorkspaceOptions()
{
    OwnedMenu owned(this);
    QMenu& menu = owned.menu();
    populateWorkspaceMenu(menu);
    QPushButton* button = m_groupHeader->optionsButton();
    menu.exec(button->mapToGlobal(QPoint(0, button->height())));
}

void ContainerWidget::setSliceAvailability(std::function<bool(int sliceId)> available)
{
    m_sliceAvailable = std::move(available);
}

void ContainerWidget::populateSourceMenu(QMenu& menu)
{
    menu.setToolTipsVisible(true);
    // The tick follows the slice the chip names (sliceId first), so a
    // container keyed by sliceId never ticks its stale rxSource.
    const int checkedRx = m_sourceMenuRx > 0 ? m_sourceMenuRx : m_rxSource;
    for (int rx = kFirstRxSource; rx <= kLastRxSource; ++rx) {
        const QChar letter(QLatin1Char(static_cast<char>('A' + rx - 1)));
        auto* action = menu.addAction(tr("RX %1").arg(letter), this,
                                      [this, rx] { emit sourceRequested(rx); });
        action->setCheckable(true);
        action->setChecked(!m_followsSelectedRx && checkedRx == rx);
        const bool exists = !m_sliceAvailable || m_sliceAvailable(sliceIdForRxSource(rx));
        action->setEnabled(exists);
        if (!exists) {
            action->setToolTip(tr("Slice %1 is not open").arg(letter));
        }
    }
    menu.addSeparator();
    auto* active = menu.addAction(tr("Active slice"), this,
                                  [this] { emit sourceRequested(kActiveSliceSource); });
    active->setCheckable(true);
    active->setChecked(m_followsSelectedRx);
    active->setToolTip(tr("Follow the selected slice"));
}

void ContainerWidget::showSourceMenu()
{
    OwnedMenu owned(this);
    QMenu& menu = owned.menu();
    populateSourceMenu(menu);
    QWidget* chip = m_groupHeader->sourceChip();
    menu.exec(chip->mapToGlobal(QPoint(0, chip->height())));
}

// Nereus-origin reserved chrome: visibility never changes the content viewport.
void ContainerWidget::setPopOutShell(bool shell)
{
    m_popOutShell = shell;
    updateTitleBar();
    // Workspace preview: a popped-out object's window has its own resize rule.
    updateChrome();
}
void ContainerWidget::setHeaderMode(HeaderMode mode)
{
    if (!m_structuredChrome) {
        m_structuredChrome = true;
        layout()->removeWidget(m_titleBar);
        m_headerSlot->layout()->addWidget(m_titleBar);
        if (!m_groupHeader) {
            m_headerSlot->show();
        }
        setFocusPolicy(Qt::StrongFocus);
        qApp->installEventFilter(this);
        connect(qApp, &QApplication::focusChanged, this,
                [this](QWidget *, QWidget *) { updateChrome(); });
    }
    if(m_headerMode!=mode) {m_recoverChrome=false;}
    m_headerMode = mode;
    m_titleBarVisible = mode != HeaderMode::Hidden;
    updateChrome();
}
bool ContainerWidget::chromeVisible() const
{
    // Workspace preview: the group header is the chrome.
    return m_groupHeader ? m_groupHeader->isVisible() : m_titleBar->isVisible();
}
void ContainerWidget::recoverChrome()
{
    m_recoverChrome = true;
    updateChrome();
}
void ContainerWidget::updateChrome()
{
    if (!m_structuredChrome) {
        return;
    }
    if (m_groupHeader) {
        updateWorkspaceChrome();
        return;
    }
    QWidget *focused = QApplication::focusWidget();
    const bool focus = focused && (focused == this || isAncestorOf(focused));
    const bool shift = m_shiftChrome || (QApplication::keyboardModifiers() & Qt::ShiftModifier);
    const bool visible = m_headerMode == HeaderMode::Shown ||
                         (m_headerMode == HeaderMode::Reveal && (m_hoverChrome || focus)) ||
                         m_recoverChrome || shift;
    m_titleBar->setVisible(visible);
    m_resizeGrip->setVisible(visible && !m_locked && !m_autoHeight && !isPanelDocked());
    m_resizeGrip->move(width() - 12, height() - 12);
    m_btnFloat->setEnabled(!m_locked);
    m_btnAxis->setEnabled(!m_locked);
    m_btnPin->setEnabled(!m_locked);
}
void ContainerWidget::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_structuredChrome) {
        QWidget::contextMenuEvent(event);
        return;
    }
    // On the heap under the container: an action can remove the
    // container, and its shell, while the menu is open (OwnedMenu.h).
    OwnedMenu owned(this);
    QMenu& menu = owned.menu();
    if (m_groupHeader && !m_popOutShell) {
        // Workspace preview: what the header's pop-out button does. A
        // pop-out shell has its own return item below.
        menu.addAction(isFloating() ? tr("Return") : tr("Pop Out"), this, [this] { m_btnFloat->click(); });
    }
    menu.addAction(tr("Container Settings…"), this, [this] { emit settingsRequested(); });
    menu.addAction(tr("Hide container (retain placement)"), this,
                   [this] { emit hideContainerRequested(); });
    if (m_popOutShell) {
        auto *back = menu.addAction(tr("Return all objects and close shell"), this,
                                    [this] { emit returnContainerRequested(); });
        back->setEnabled(!m_locked);
    }
    if (m_groupHeader) {
        // Workspace preview: the same toggle as the header's eye button.
        auto *keep = menu.addAction(tr("Keep Header Showing"), this, [this](bool checked) {
            emit headerModeRequested(checked ? HeaderMode::Shown : HeaderMode::Hidden);
        });
        keep->setCheckable(true);
        keep->setChecked(m_headerMode == HeaderMode::Shown);
    } else {
        menu.addAction(tr("Reveal controls"), this, &ContainerWidget::recoverChrome);
        auto *header = menu.addMenu(tr("Header"));
        for (auto mode : {HeaderMode::Shown, HeaderMode::Reveal, HeaderMode::Hidden}) {
            auto *action =
                header->addAction(mode == HeaderMode::Shown    ? tr("Always visible")
                                  : mode == HeaderMode::Reveal ? tr("Reveal on hover/focus")
                                                               : tr("Hidden (hold Shift to recover)"));
            action->setCheckable(true);
            action->setChecked(mode == m_headerMode);
            connect(action, &QAction::triggered, this,
                    [this, mode] { emit headerModeRequested(mode); });
        }
    }
    if (m_groupHeader) {
        auto *arrange = menu.addAction(tr("Arrange Workspace"), this,
                                       [this] { emit arrangeWorkspaceRequested(!m_arrangeSwitchOn); });
        arrange->setCheckable(true);
        arrange->setChecked(m_arrangeSwitchOn);
    }
    if (auto *host = qobject_cast<ContainerContentHost *>(m_content)) {
        host->addContentsMenu(menu);
    }
    menu.exec(event->globalPos());
}

void ContainerWidget::setContentMinimumWidth(int width)
{
    // JJ's choice (2026-10-08): never narrower than the content. A splitter,
    // edge strip or floating window holding the container then keeps the
    // content whole, and the 260 px floor still applies.
    m_contentMinimumWidth = qMax(0, width);
    const int minimum = qMax(kMinContainerWidth, m_contentMinimumWidth);
    if (minimumWidth() != minimum) {
        setMinimumWidth(minimum);
    }
}

void ContainerWidget::setContent(QWidget* widget)
{
    QLayout* layout = m_contentHolder->layout();
    // A new content announces its own minimum width.
    setContentMinimumWidth(0);
    if (m_content) {
        layout->removeWidget(m_content);
        // Detach SYNCHRONOUSLY before deleteLater. When the content is a
        // MeterWidget (WA_NativeWindow QRhiWidget), leaving it under this
        // container's HWND through an upcoming reparent blocks the fresh
        // MeterWidget's D3D11 swapchain with E_ACCESSDENIED on Windows.
        m_content->hide();
        m_content->setParent(nullptr);
        m_content->deleteLater();
    }
    m_content = widget;
    if (m_content) {
        m_content->setParent(m_contentHolder);
        m_content->setMouseTracking(true);
        m_content->installEventFilter(this);
        // Enable mouse tracking + event filter on all descendants so hover
        // detection works even when mouse is over child widgets (scroll areas, applets).
        for (QWidget* child : m_content->findChildren<QWidget*>()) {
            child->setMouseTracking(true);
            child->installEventFilter(this);
        }
        layout->addWidget(m_content);
    }
    emit contentChanged(m_content);
}

void ContainerWidget::updateTitleBar()
{
    // Thetis ucMeter.cs:605-624 — adapted for 3 dock modes
    if (isFloating()) {
        m_btnFloat->setText(QStringLiteral("\u2199"));
        m_btnFloat->setToolTip(
            m_popOutShell ? QStringLiteral("Return all objects to their remembered containers")
                          : QStringLiteral("Dock"));
        m_btnAxis->setVisible(false);
        m_btnPin->setVisible(true);
    } else if (isPanelDocked()) {
        m_btnFloat->setText(QStringLiteral("\u2197"));
        m_btnFloat->setToolTip(QStringLiteral("Float"));
        m_btnAxis->setVisible(false);
        m_btnPin->setVisible(false);
    } else {
        m_btnFloat->setText(QStringLiteral("\u2197"));
        m_btnFloat->setToolTip(QStringLiteral("Float"));
        m_btnAxis->setVisible(true);
        m_btnPin->setVisible(false);
    }
}

void ContainerWidget::updateTitle()
{
    // Thetis ucMeter.cs:625-639 ("RX" + the receiver number); NereusSDR
    // names the container's slice.
    QString firstLine = m_notes.section(QLatin1Char('\n'), 0, 0);
    QString title = sliceNameForRxSource(m_rxSource);
    if (!firstLine.isEmpty()) {
        title += QStringLiteral(" ") + firstLine;
    }
    m_titleLabel->setText(title);
    if (m_groupHeader && m_groupHeader->title().isEmpty()) {
        m_groupHeader->setTitle(firstLine.isEmpty() ? title : firstLine);
    }
}

void ContainerWidget::setupBorder()
{
    // Thetis ucMeter.cs:640-643, extended with highlight outline
    // (Phase 3G-6). When m_highlighted the accent outline overrides the
    // normal border regardless of m_border, so the user always sees
    // which container the settings dialog is editing.
    if (m_groupHeader) {
        // Workspace preview: the glass plate, radius 10, a 1 px
        // border-subtle edge (or the 2 px accent while highlighted).
        setAttribute(Qt::WA_StyledBackground, true);
        const QString edge = m_highlighted
            ? QStringLiteral("border: 2px solid %1;").arg(Workspace::role("accent", "#00b4d8"))
            : m_border ? QStringLiteral("border: 1px solid %1;").arg(Workspace::role("border-subtle", "#203040"))
                       : QStringLiteral("border: none;");
        setStyleSheet(QStringLiteral("ContainerWidget { background: %1; %2 border-radius: %3px; }")
                          .arg(Workspace::glassPanelFill(), edge)
                          .arg(Workspace::kGlassPanelRadius));
    } else if (WorkspacePreview::enabled() && (m_highlighted || m_border)) {
        // Workspace preview: the highlight is the accent; the plain
        // border is #203040's border-subtle role.
        setStyleSheet(QStringLiteral("ContainerWidget { border: %1px solid %2; }")
                          .arg(m_highlighted ? 2 : 1)
                          .arg(m_highlighted ? Workspace::role("accent", "#00b4d8")
                                             : Workspace::role("border-subtle", "#203040")));
    } else if (m_highlighted) {
        setStyleSheet(QStringLiteral(
            "ContainerWidget { border: 2px solid #00b4d8; }"));
    } else if (m_border) {
        setStyleSheet(QStringLiteral(
            "ContainerWidget { border: 1px solid #203040; }"));
    } else {
        setStyleSheet(QString());
    }
}

// --- Property setters ---

void ContainerWidget::setId(const QString& id) { m_id = id; m_id.remove(QLatin1Char('|')); }
void ContainerWidget::setRxSource(int rx)
{
    const bool changed = (m_rxSource != rx);
    m_rxSource = rx;
    updateTitle();
    if (changed) { emit rxSourceChanged(rx); }
}

QString ContainerWidget::sliceNameForRxSource(int rx)
{
    if (rx < 1 || rx > 26) { return QStringLiteral("Slice"); }
    return QStringLiteral("Slice %1").arg(QChar(QLatin1Char(static_cast<char>('A' + rx - 1))));
}

void ContainerWidget::setDockMode(DockMode mode)
{
    bool changed = (m_dockMode != mode);
    m_dockMode = mode;

    // Enforce minimum width in all dock modes so the applets are always usable.
    // Never below what the content needs either (JJ, 2026-10-08).
    setMinimumWidth(qMax(kMinContainerWidth, m_contentMinimumWidth));

    if (m_groupHeader) {
        // Workspace preview: a free or floating container covers whatever
        // lies beneath it (VFO flags, the spectrum) with the opaque panel
        // surface over its whole rect, the plate's rounded corners included.
        // In a strip the app background shows around the plate instead.
        const bool covers = mode != DockMode::PanelDocked;
        QPalette surface = palette();
        surface.setColor(QPalette::Window, contentBackground(QColor::fromRgba(kLegacyContentBackground)));
        setPalette(surface);
        setAutoFillBackground(covers);
    }

    if (changed) {
        updateTitleBar();
        emit dockModeChanged(mode);
    }
}

void ContainerWidget::setAxisLock(AxisLock lock)
{
    m_axisLock = lock;

    // Update axis button icon — from Thetis ucMeter.cs:936-968
    static const QChar arrows[] = {
        QChar(0x2190),  // LEFT
        QChar(0x2196),  // TOPLEFT
        QChar(0x2191),  // TOP
        QChar(0x2197),  // TOPRIGHT
        QChar(0x2192),  // RIGHT
        QChar(0x2198),  // BOTTOMRIGHT
        QChar(0x2193),  // BOTTOM
        QChar(0x2199),  // BOTTOMLEFT
    };
    int idx = static_cast<int>(lock);
    if (idx >= 0 && idx < 8) {
        m_btnAxis->setText(QString(arrows[idx]));
    }
}

void ContainerWidget::cycleAxisLock(bool reverse)
{
    // From Thetis ucMeter.cs:912-935
    int n = static_cast<int>(m_axisLock);
    if (reverse) {
        n--;
    } else {
        n++;
    }
    if (n > static_cast<int>(AxisLock::BottomLeft)) {
        n = static_cast<int>(AxisLock::Left);
    }
    if (n < static_cast<int>(AxisLock::Left)) {
        n = static_cast<int>(AxisLock::BottomLeft);
    }
    setAxisLock(static_cast<AxisLock>(n));
    emit dockedMoved();
}

void ContainerWidget::setPinOnTop(bool pin)
{
    // From Thetis ucMeter.cs:974-978
    m_pinOnTop = pin;
    AppIcon::apply(m_btnPin, pin ? QStringLiteral("pinned") : QStringLiteral("pin"), kPinIconPx);
    if (m_groupHeader) {
        m_groupHeader->setPinned(pin);
    }
    setTopMost();
}

void ContainerWidget::requestPinOnTop(bool pin)
{
    const QPointer<ContainerWidget> self(this);
    setPinOnTop(pin);
    if (self) {
        emit pinOnTopRequested(pin);
    }
}

void ContainerWidget::setTopMost()
{
    // From Thetis ucMeter.cs:980-993
    if (isFloating() && parentWidget()) {
        FloatingContainer* fc = qobject_cast<FloatingContainer*>(parentWidget());
        if (fc) {
            bool wasVisible = fc->isVisible();
            if (m_pinOnTop) {
                fc->setWindowFlags(fc->windowFlags() | Qt::WindowStaysOnTopHint);
            } else {
                fc->setWindowFlags(fc->windowFlags() & ~Qt::WindowStaysOnTopHint);
            }
            if (wasVisible) {
                fc->show();
            }
        }
    }
}

void ContainerWidget::setBorder(bool border) { m_border = border; setupBorder(); }
void ContainerWidget::setLocked(bool locked)
{
    m_locked = locked;
    updateChrome();
}
void ContainerWidget::setContainerEnabled(bool enabled) { m_enabled = enabled; }
void ContainerWidget::setShowOnRx(bool show) { m_showOnRx = show; }
void ContainerWidget::setShowOnTx(bool show) { m_showOnTx = show; }
void ContainerWidget::setHiddenByMacro(bool hidden) { m_hiddenByMacro = hidden; }
void ContainerWidget::setContainerMinimises(bool minimises) { m_containerMinimises = minimises; }
void ContainerWidget::setContainerHidesWhenRxNotUsed(bool hides) { m_containerHidesWhenRxNotUsed = hides; }

void ContainerWidget::setNotes(const QString& notes)
{
    QString scrubbed = notes;
    scrubbed.remove(QLatin1Char('|'));
    if (m_notes == scrubbed) { return; }
    m_notes = scrubbed;
    updateTitle();
    emit notesChanged(m_notes);
}

void ContainerWidget::setBackgroundColor(const QColor& color)
{
    if(!color.isValid() || m_backgroundColor==color) {return;}
    m_backgroundColor=color;
    publishUserBackground();
    if (m_groupHeader && color.rgba() == kLegacyContentBackground) {
        // Workspace preview: the default background is the glass plate.
        m_contentHolder->setStyleSheet(QStringLiteral("background: transparent;"));
        return;
    }
    m_contentHolder->setStyleSheet(QStringLiteral("background: %1;").arg(color.name(QColor::HexArgb)));
}

// Before the content holder's stylesheet changes, so the meters it
// re-polishes read the new colour (WorkspacePreview::dataBackground()).
void ContainerWidget::publishUserBackground()
{
    setProperty(WorkspacePreview::kUserBackgroundProperty, userBackground(m_backgroundColor));
}

QColor ContainerWidget::contentBackground(const QColor& configured)
{
    if (WorkspacePreview::enabled() && configured.rgba() == kLegacyContentBackground) {
        return QColor(Workspace::role("panel", "#0f0f1a"));
    }
    return configured;
}

QColor ContainerWidget::userBackground(const QColor& configured)
{
    return configured.rgba() != kLegacyContentBackground ? configured : QColor();
}

void ContainerWidget::setNoControls(bool noControls) { m_noControls = noControls; }
void ContainerWidget::setAutoHeight(bool autoHeight)
{
    m_autoHeight = autoHeight;
    updateChrome();
}

bool ContainerWidget::operatorSizesHeight() const
{
    return m_groupHeader != nullptr && m_popOutShell && isFloating();
}

bool ContainerWidget::sizingHeightNow() const
{
    return m_resizing && m_resizeAxes.testFlag(Qt::Vertical) && operatorSizesHeight();
}

int ContainerWidget::naturalHeight() const
{
    const auto* host = qobject_cast<const ContainerContentHost*>(m_content);
    return (host ? host->preferredContentHeight() : 0) + headerExtent();
}

void ContainerWidget::setTitleBarVisible(bool visible)
{
    if (m_titleBarVisible == visible) { return; }
    m_titleBarVisible = visible;
    if (m_groupHeader && m_structuredChrome) {
        updateChrome();
    } else if (m_titleBar) {
        m_titleBar->setVisible(visible);
    }
    emit titleBarVisibilityChanged(visible);
    update();
}

void ContainerWidget::setMinimised(bool minimised)
{
    if (m_minimised == minimised) { return; }
    m_minimised = minimised;
    // Collapse the content area so only the title bar shows. The title
    // bar stays visible regardless of m_titleBarVisible while minimised
    // so the user retains a handle to un-minimise. FloatingContainer
    // (commit 10) picks up minimisedChanged() to resize its window.
    if (m_contentHolder) {
        m_contentHolder->setVisible(!minimised);
    }
    if (m_groupHeader && m_structuredChrome) {
        updateChrome();
    } else if (minimised && m_titleBar) {
        m_titleBar->setVisible(true);
    } else if (m_titleBar) {
        m_titleBar->setVisible(m_titleBarVisible);
    }
    emit minimisedChanged(minimised);
    update();
}

void ContainerWidget::setHighlighted(bool highlighted)
{
    if (m_highlighted == highlighted) { return; }
    m_highlighted = highlighted;
    setupBorder();
    update();
}
void ContainerWidget::setDockedLocation(const QPoint& loc) { m_dockedLocation = loc; }
void ContainerWidget::setDockedSize(const QSize& size) { m_dockedSize = size; }
void ContainerWidget::setDelta(const QPoint& delta) { m_delta = delta; }

void ContainerWidget::storeLocation()
{
    // From Thetis ucMeter.cs:567-572
    m_dockedLocation = pos();
    m_dockedSize = size();
}

void ContainerWidget::restoreLocation()
{
    // From Thetis ucMeter.cs:574-593
    bool moved = false;
    if (m_dockedLocation != pos()) {
        move(m_dockedLocation);
        moved = true;
    }
    if (m_dockedSize != size()) {
        resize(m_dockedSize);
        moved = true;
    }
    if (moved) {
        update();
    }
}

int ContainerWidget::roundToNearestTen(int value)
{
    return ((value + 5) / 10) * 10;
}

// --- Hover show/hide ---

void ContainerWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_structuredChrome) {
        if (!m_groupHeader) {
            m_hoverChrome = true;
            updateChrome();
        }
        if (m_dragging) {
            updateDrag(event->globalPosition().toPoint());
        }
        if (m_resizing) {
            updateResize(event->globalPosition().toPoint());
        }
        QWidget::mouseMoveEvent(event);
        return;
    }
    // From Thetis ucMeter.cs:1198-1229
    // Upstream inline attribution preserved verbatim (ucMeter.cs:1200):
    //   bool no_controls = _no_controls && !Common.ShiftKeyDown; //[2.10.3.6]MW0LGE no title or resize grabber, override by holding shift
    bool noControls = m_noControls && !(QApplication::keyboardModifiers() & Qt::ShiftModifier);

    if (!m_dragging && !noControls) {
        int y = static_cast<int>(event->position().y());
        if (y < kTitleHoverZone && !m_titleBar->isVisible()) {
            m_titleBar->setVisible(true);
            m_titleBar->raise();
        } else if (y >= kTitleBarHeight && m_titleBar->isVisible() && !m_dragging) {
            m_titleBar->setVisible(false);
        }
    }

    // Resize grip: only for overlay-docked and floating, not panel-docked
    if (!m_resizing && !noControls && !isPanelDocked()) {
        bool inGripRegion = event->position().x() > (width() - 16)
                         && event->position().y() > (height() - 16);
        if (inGripRegion && !m_resizeGrip->isVisible()) {
            m_resizeGrip->move(width() - 12, height() - 12);
            m_resizeGrip->setVisible(true);
            m_resizeGrip->raise();
        } else if (!inGripRegion && m_resizeGrip->isVisible() && !m_resizing) {
            m_resizeGrip->setVisible(false);
        }
    }

    if (m_dragging) {
        updateDrag(event->globalPosition().toPoint());
    }
    if (m_resizing) {
        updateResize(event->globalPosition().toPoint());
    }

    QWidget::mouseMoveEvent(event);
}

void ContainerWidget::leaveEvent(QEvent* event)
{
    if (m_structuredChrome) {
        m_hoverChrome = false;
        if(!m_dragging && !m_resizing) {m_recoverChrome = false;}
        updateChrome();
        QWidget::leaveEvent(event);
        return;
    }
    if (!m_dragging && !m_resizing) {
        m_titleBar->setVisible(false);
        m_resizeGrip->setVisible(false);
    }
    QWidget::leaveEvent(event);
}

// --- Event filter for title bar drag + resize grip ---

bool ContainerWidget::eventFilter(QObject* watched, QEvent* event)
{
    // Workspace preview: Escape cancels a drag or an edge drag; nothing of
    // it is kept.
    if (m_groupHeader && (m_edgePressed || m_dragging) && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        if (m_edgePressed) {
            endEdgeDrag(true, QCursor::pos());
        } else if (m_arrangeDrag) {
            finishArrangeDrag(true, QCursor::pos());
        } else {
            cancelDrag();
        }
        return true;
    }
    // While arranging, a drag that loses the mouse (the header's grab is
    // taken, or the app is left) will never see its release: it is
    // cancelled like Escape.
    if (m_groupHeader && m_arrangeActive && m_arrangeDropRule && (m_edgePressed || m_arrangeDrag)
        && ((watched == m_groupHeader && event->type() == QEvent::UngrabMouse)
            || event->type() == QEvent::ApplicationDeactivate)) {
        if (m_edgePressed) {
            endEdgeDrag(true, QCursor::pos());
        } else {
            finishArrangeDrag(true, QCursor::pos());
        }
        return false;
    }
    // Workspace preview: nothing here shows a hidden header; the eye
    // button, the body menu and Arrange do.
    if (m_structuredChrome && !m_groupHeader) {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Shift) {
                m_shiftChrome = event->type() == QEvent::KeyPress;
                updateChrome();
            }
        }
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut ||
            event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            QTimer::singleShot(0, this, &ContainerWidget::updateChrome);
        }
        if (auto *widget = qobject_cast<QWidget *>(watched);
            widget && (widget == this || isAncestorOf(widget))) {
            if (event->type() == QEvent::Enter || event->type() == QEvent::MouseMove) {
                m_hoverChrome = true;
                updateChrome();
            }
            if (event->type() == QEvent::Resize) {
                updateChrome();
            }
        }
    }
    // Workspace preview: the group header moves the container. Its buttons
    // accept their own presses, so only the stripe, grab handle, title,
    // chip and empty header area reach here. A panel-docked container
    // keeps its place (edges arrive with the four-edge strips).
    if (m_groupHeader && watched == m_groupHeader) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_grabOffset = mapFromGlobal(me->globalPosition().toPoint());
                if (!m_locked && !isPanelDocked()) {
                    // While arranging the manager decides where it lands.
                    m_arrangeDrag = m_arrangeActive && m_arrangeDropRule;
                    m_edgeDragging = false;
                    m_edgePressGlobal = me->globalPosition().toPoint();
                    beginDrag(me->globalPosition().toPoint());
                } else if (!m_locked) {
                    // A docked container moves between strips: past the
                    // drag distance the manager shows the destinations.
                    m_edgePressed = true;
                    m_edgeDragging = false;
                    m_edgePressGlobal = me->globalPosition().toPoint();
                }
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_edgePressed) {
            const QPoint global = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
            if (!m_edgeDragging
                && (global - m_edgePressGlobal).manhattanLength() >= QApplication::startDragDistance()) {
                m_edgeDragging = true;
                m_groupHeader->setCursor(Qt::ClosedHandCursor);
                emit edgeDragStarted();
            }
            if (m_edgeDragging) {
                emit edgeDragMoved(global);
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_edgePressed) {
            endEdgeDrag(false, static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            updateDrag(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            if (m_arrangeDrag) {
                finishArrangeDrag(false, static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            } else {
                endDrag();
            }
            return true;
        }
    }
    // Workspace preview: Arrange's inside edge zones resize one axis.
    if (m_groupHeader && (watched == m_edgeRight || watched == m_edgeBottom)
        && !m_locked && !isPanelDocked()) {
        if (event->type() == QEvent::MouseButtonPress
            && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            m_resizeAxes = watched == m_edgeRight ? Qt::Horizontal : Qt::Vertical;
            beginResize(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseMove && m_resizing) {
            updateResize(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_resizing) {
            endResize();
            m_resizeAxes = Qt::Horizontal | Qt::Vertical;
            return true;
        }
    }

    // Title bar drag
    if ((watched == m_titleBar || watched == m_titleLabel) && !m_locked) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && !isPanelDocked()) {
                beginDrag(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            updateDrag(me->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            endDrag();
            return true;
        }
    }

    // Content area hover — detect mouse in title region when title bar is hidden
    // (hidden title bar collapses in layout, so content fills top area)
    bool isContentArea = (watched == m_contentHolder || watched == m_content);
    if (!isContentArea && m_content) {
        QWidget* w = qobject_cast<QWidget*>(watched);
        if (w && w->isAncestorOf(m_content) == false && m_content->isAncestorOf(w)) {
            isContentArea = true;
        }
    }
    if (!m_structuredChrome && isContentArea && event->type() == QEvent::MouseMove && !m_locked) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        bool noControls = m_noControls && !(QApplication::keyboardModifiers() & Qt::ShiftModifier);
        if (!noControls) {
            QWidget* w = static_cast<QWidget*>(watched);
            int y = w->mapTo(this, me->position().toPoint()).y();
            if (y < kTitleHoverZone && !m_titleBar->isVisible()) {
                m_titleBar->setVisible(true);
                m_titleBar->raise();
            } else if (y >= kTitleBarHeight && m_titleBar->isVisible() && !m_dragging) {
                m_titleBar->setVisible(false);
            }
        }
    }

    // Resize grip (not for panel-docked)
    if (watched == m_resizeGrip && !m_locked && (!m_autoHeight || operatorSizesHeight()) && !isPanelDocked()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_resizeAxes = Qt::Horizontal | Qt::Vertical;
                beginResize(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_resizing) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            updateResize(me->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_resizing) {
            endResize();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

// --- Drag logic ---

void ContainerWidget::beginDrag(const QPoint& globalPos)
{
    // From Thetis ucMeter.cs:281-294
    m_dragging = true;
    emit geometryInteractionStarted();
    // Closed hand while the drag runs (native shape; see the Qt 6.11 macOS
    // cursor crash note at the title label). Set on the bar too, since the
    // press can land on the bar beside the label.
    m_titleBar->setCursor(Qt::ClosedHandCursor);
    m_titleLabel->setCursor(Qt::ClosedHandCursor);
    if (m_groupHeader) {
        m_groupHeader->setCursor(Qt::ClosedHandCursor);
    }
    m_dragHoldSize = QSize();
    m_dragSizeWarned = false;
    m_dragSizeQueuedRestoreDone = false;
    if (isFloating()) {
        m_dragStartPos = globalPos - parentWidget()->pos();
        m_dragOrigin = parentWidget()->pos();
        // A drag moves the window and never resizes it.
        m_dragHoldSize = parentWidget()->size();
    } else {
        raise();
        m_dragStartPos = globalPos - pos();
        m_dragOrigin = pos();
    }
}

void ContainerWidget::updateDrag(const QPoint& globalPos)
{
    if (!m_dragging) {
        return;
    }
    // Workspace preview: the 8 px grid is the one snap aid; Ctrl steps by
    // 10 px with the look off only.
    const bool ctrlSteps = !m_groupHeader && (QApplication::keyboardModifiers() & Qt::ControlModifier);
    // A size the platform changed since the last move is put back before
    // this move sends the window's geometry again, and after it.
    m_dragSizeQueuedRestoreDone = false;
    holdDragSize();

    if (m_arrangeDrag) {
        // Arranging: past the drag distance the manager shows where a
        // release would land. The container follows the pointer anywhere;
        // nothing holds it inside the pane until it is dropped.
        if (!m_edgeDragging) {
            if ((globalPos - m_edgePressGlobal).manhattanLength() < QApplication::startDragDistance()) {
                return;
            }
            m_edgeDragging = true;
            emit edgeDragStarted();
        }
        QWidget* moved = isFloating() ? parentWidget() : this;
        const QPoint newPos = globalPos - m_dragStartPos;
        if (moved && moved->pos() != newPos) {
            moved->move(newPos);
        }
        holdDragSize();
        emit edgeDragMoved(globalPos);
        return;
    }

    if (isFloating()) {
        // From Thetis ucMeter.cs:319-345
        QPoint newPos = globalPos - m_dragStartPos;
        if (ctrlSteps) {
            newPos.setX(roundToNearestTen(newPos.x()));
            newPos.setY(roundToNearestTen(newPos.y()));
        }
        if (parentWidget() && parentWidget()->pos() != newPos) {
            parentWidget()->move(newPos);
        }
        holdDragSize();
    } else {
        // From Thetis ucMeter.cs:346-374 — overlay-docked, clamped
        QPoint newPos = globalPos - m_dragStartPos;
        if (ctrlSteps) {
            newPos.setX(roundToNearestTen(newPos.x()));
            newPos.setY(roundToNearestTen(newPos.y()));
        }
        if (parentWidget()) {
            int maxX = parentWidget()->width() - width();
            int maxY = parentWidget()->height() - height();
            newPos.setX(std::clamp(newPos.x(), 0, std::max(0, maxX)));
            newPos.setY(std::clamp(newPos.y(), 0, std::max(0, maxY)));
        }
        if (pos() != newPos) {
            move(newPos);
            update();
        }
    }
}

void ContainerWidget::endDrag()
{
    // Workspace preview: optional grid alignment after release; Alt at
    // release places without it.
    if (m_groupHeader && m_gridPx > 1 && !(QApplication::keyboardModifiers() & Qt::AltModifier)) {
        if (isFloating() && parentWidget()) {
            const QPoint snapped = workspaceSnappedTopLeft(parentWidget()->pos(), m_gridPx);
            if (snapped != parentWidget()->pos()) {
                parentWidget()->move(snapped);
            }
        } else if (isOverlayDocked() && parentWidget()) {
            QPoint snapped = workspaceSnappedTopLeft(pos(), m_gridPx);
            snapped.setX(std::clamp(snapped.x(), 0, std::max(0, parentWidget()->width() - width())));
            snapped.setY(std::clamp(snapped.y(), 0, std::max(0, parentWidget()->height() - height())));
            if (snapped != pos()) {
                move(snapped);
            }
        }
    }
    endDragSizeHold();
    m_dragging = false;
    m_arrangeDrag = false;
    emit geometryInteractionFinished();
    m_dragStartPos = QPoint();
    m_titleBar->unsetCursor();
    m_titleLabel->setCursor(Qt::OpenHandCursor);
    if (m_groupHeader) {
        m_groupHeader->setCursor(Qt::OpenHandCursor);
    }
    if (isOverlayDocked()) {
        m_dockedLocation = pos();
        emit dockedMoved();
    }
}

void ContainerWidget::cancelDrag()
{
    // Back to where the drag began, so the commit after it changes nothing.
    if (!m_dragging) {
        return;
    }
    if (isFloating() && parentWidget()) {
        parentWidget()->move(m_dragOrigin);
    } else {
        move(m_dragOrigin);
    }
    endDragSizeHold();
    m_dragging = false;
    emit geometryInteractionFinished();
    m_dragStartPos = QPoint();
    m_titleBar->unsetCursor();
    m_titleLabel->setCursor(Qt::OpenHandCursor);
    if (m_groupHeader) {
        m_groupHeader->setCursor(Qt::OpenHandCursor);
    }
}

void ContainerWidget::finishArrangeDrag(bool cancelled, const QPoint& globalPos)
{
    if (!m_dragging) {
        return;
    }
    const bool reported = m_edgeDragging;
    m_arrangeDrag = false;
    m_edgeDragging = false;
    if (cancelled) {
        // Back to where the drag began; nothing of it is kept.
        QWidget* moved = isFloating() ? parentWidget() : this;
        if (moved && moved->pos() != m_dragOrigin) {
            moved->move(m_dragOrigin);
        }
    }
    // Before the manager judges the drop: what it places or saves is the
    // window at its press size.
    endDragSizeHold();
    m_dragging = false;
    m_dragStartPos = QPoint();
    m_titleBar->unsetCursor();
    m_titleLabel->setCursor(Qt::OpenHandCursor);
    m_groupHeader->setCursor(Qt::OpenHandCursor);
    // The manager decides the drop first; the interaction then ends.
    if (reported && cancelled) {
        emit edgeDragCancelled();
    } else if (reported) {
        emit edgeDragFinished(globalPos);
    }
    emit geometryInteractionFinished();
}

bool ContainerWidget::holdDragSize()
{
    if (!m_dragging || !m_dragHoldSize.isValid() || m_restoringDragSize || !isFloating()) {
        return false;
    }
    QWidget* form = parentWidget();
    if (!form) {
        return false;
    }
    // What the window can be given: its limits may have moved during the
    // drag (a monitor unplugged lowers its maximum), and a size it cannot
    // take is not asked for again and again.
    const QSize hold = m_dragHoldSize.boundedTo(form->maximumSize()).expandedTo(form->minimumSize());
    if (form->size() == hold) {
        return false;
    }
    if (!m_dragSizeWarned) {
        // Once per drag. A floating window dragged across the join of two
        // screens grew without bound on Windows; this line says what the
        // size was found at and on which screen.
        m_dragSizeWarned = true;
        const QScreen* screen = form->screen();
        qCWarning(lcContainer).noquote().nospace()
            << "Floating container " << m_id << " changed size during a drag: found "
            << form->width() << "x" << form->height() << ", restored "
            << hold.width() << "x" << hold.height() << ", screen "
            << (screen ? screen->name() : QStringLiteral("none")) << ", devicePixelRatio "
            << form->devicePixelRatioF();
    }
    // The resize below is delivered to this container at once (or later by
    // the platform); the flag keeps that event from asking for another.
    QScopedValueRollback<bool> restoring(m_restoringDragSize, true);
    form->resize(hold);
    return true;
}

void ContainerWidget::endDragSizeHold()
{
    holdDragSize();
    m_dragHoldSize = QSize();
    m_dragSizeQueuedRestoreDone = false;
}

void ContainerWidget::endEdgeDrag(bool cancelled, const QPoint& globalPos)
{
    const bool wasDragging = m_edgeDragging;
    m_edgePressed = false;
    m_edgeDragging = false;
    if (m_groupHeader) {
        m_groupHeader->setCursor(Qt::OpenHandCursor);
    }
    if (!wasDragging) {
        return;
    }
    if (cancelled) {
        emit edgeDragCancelled();
    } else {
        emit edgeDragFinished(globalPos);
    }
}

// --- Resize logic ---

void ContainerWidget::beginResize(const QPoint& globalPos)
{
    // From Thetis ucMeter.cs:400-407
    m_resizeStartGlobal = globalPos;
    m_resizeStartSize = isFloating() && parentWidget() ? parentWidget()->size() : size();
    m_resizing = true;
    emit geometryInteractionStarted();
    raise();
}

void ContainerWidget::updateResize(const QPoint& globalPos)
{
    if (!m_resizing) {
        return;
    }

    // From Thetis ucMeter.cs:489-518
    int dX = globalPos.x() - m_resizeStartGlobal.x();
    int dY = globalPos.y() - m_resizeStartGlobal.y();

    int newW = m_resizeStartSize.width() + dX;
    int newH = m_resizeStartSize.height() + dY;
    // Workspace preview edge zones resize one axis; otherwise both.
    if (!m_resizeAxes.testFlag(Qt::Horizontal)) {
        newW = m_resizeStartSize.width();
    }
    if (!m_resizeAxes.testFlag(Qt::Vertical)) {
        newH = m_resizeStartSize.height();
    }

    // Workspace preview: no 10 px step (see updateDrag).
    if (!m_groupHeader && (QApplication::keyboardModifiers() & Qt::ControlModifier)) {
        newW = roundToNearestTen(newW);
        newH = roundToNearestTen(newH);
    }

    doResize(newW, newH);
}

void ContainerWidget::endResize()
{
    if (m_resizing && m_autoHeight && operatorSizesHeight() && m_resizeAxes.testFlag(Qt::Vertical)
        && parentWidget() && parentWidget()->height() != m_resizeStartSize.height()) {
        // Workspace preview: the operator set this window's height. It no
        // longer follows the content; the manager saves that with the size.
        m_autoHeight = false;
    }
    m_resizing = false;
    emit geometryInteractionFinished();
    m_resizeStartGlobal = QPoint();
    if (isOverlayDocked()) {
        m_dockedSize = size();
    }
}

void ContainerWidget::doResize(int w, int h)
{
    // From Thetis ucMeter.cs:520-549
    w = std::max(w, std::max(kMinContainerWidth, minimumSizeHint().width()));
    h = std::max(h, std::max(kMinContainerHeight, minimumSizeHint().height()));
    if (operatorSizesHeight()) {
        // Workspace preview: never shorter than its content needs.
        h = std::max(h, naturalHeight());
    }

    if (isFloating()) {
        if (auto* form = qobject_cast<FloatingContainer*>(parentWidget())) {
            // Never larger than all the screens together.
            form->resize(form->fittedSize(QSize(w, h)));
        } else if (parentWidget()) {
            parentWidget()->resize(w, h);
        }
    } else {
        if (parentWidget()) {
            if (x() + w > parentWidget()->width()) {
                w = parentWidget()->width() - x();
            }
            if (y() + h > parentWidget()->height()) {
                h = parentWidget()->height() - y();
            }
        }
        QSize newSize(w, h);
        if (newSize != size()) {
            resize(newSize);
            update();
        }
    }
}

// --- Interactive item signal forwarding ---

void ContainerWidget::wireInteractiveItem(MeterItem* item)
{
    if (item->property("interactiveContainerWired").toBool()) { return; }
    item->setProperty("interactiveContainerWired",true);
    const auto forward = [this,item](const std::function<void()>& action) {
        if (item->property("containerUnsupportedSource").toBool()) {
            emit unavailableButtonClicked(item->property("unsupportedSourceReason").toString()); return;
        }
        const QVariant previous = property("containerDispatchContext");
        const QVariant context = item->property("containerSourceContext");
        if (context.isValid()) { setProperty("containerDispatchContext",context); }
        action(); setProperty("containerDispatchContext",previous);
    };
    if (auto* box = qobject_cast<ButtonBoxItem*>(item)) {
        connect(box, &ButtonBoxItem::unavailableButtonClicked, this,
                [this](int, const QString& reason) {
            emit unavailableButtonClicked(reason);
        });
    }
    if (auto* band = qobject_cast<BandButtonItem*>(item)) {
        connect(band, &BandButtonItem::bandClicked,
                this, [this,forward](int value) { forward([&] { emit bandClicked(value); }); });
        connect(band, &BandButtonItem::bandStackRequested,
                this, [this,forward](int value) { forward([&] { emit bandStackRequested(value); }); });
    } else if (auto* mode = qobject_cast<ModeButtonItem*>(item)) {
        connect(mode, &ModeButtonItem::modeClicked,
                this, [this,forward](int value) { forward([&] { emit modeClicked(value); }); });
    } else if (auto* filter = qobject_cast<FilterButtonItem*>(item)) {
        connect(filter, &FilterButtonItem::filterClicked,
                this, [this,forward](int value) { forward([&] { emit filterClicked(value); }); });
        connect(filter, &FilterButtonItem::filterContextRequested,
                this, [this,forward](int value) { forward([&] { emit filterContextRequested(value); }); });
    } else if (auto* ant = qobject_cast<AntennaButtonItem*>(item)) {
        connect(ant, &AntennaButtonItem::antennaSelected,
                this, [this,forward](int value) { forward([&] { emit antennaSelected(value); }); });
        // Phase 3P-I-a T17 — late-added antenna items inherit the
        // container's current hasAlex state (set by MainWindow on
        // connect / currentRadioChanged via setBoardCapabilities).
        ant->setHasAlex(m_hasAlex);
    } else if (auto* step = qobject_cast<TuneStepButtonItem*>(item)) {
        connect(step, &TuneStepButtonItem::tuneStepSelected,
                this, [this,forward](int value) { forward([&] { emit tuneStepSelected(value); }); });
    } else if (auto* other = qobject_cast<OtherButtonItem*>(item)) {
        connect(other, &OtherButtonItem::otherButtonClicked,
                this, [this,forward](int value) { forward([&] { emit otherButtonClicked(value); }); });
        connect(other, &OtherButtonItem::macroTriggered,
                this, [this,forward](int value) { forward([&] { emit macroTriggered(value); }); });
    } else if (auto* voice = qobject_cast<VoiceRecordPlayItem*>(item)) {
        connect(voice, &VoiceRecordPlayItem::voiceAction,
                this, [this,forward](int value) { forward([&] { emit voiceAction(value); }); });
    } else if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) {
        // Task 36: the VFO display's tuning and its other requests name
        // their slice; MainWindow::onContainerItemAdded hears them.
        connect(vfo, &VfoDisplayItem::bandStackRequested,
                this, [this,forward](int value) { forward([&] { emit bandStackRequested(value); }); });
        connect(vfo, &VfoDisplayItem::filterContextRequested,
                this, [this,forward](int) { forward([&] { emit vfoFilterContextRequested(); }); });
    }
}

// --- Serialization ---

QString ContainerWidget::axisLockToString(AxisLock lock)
{
    static const char* names[] = {
        "LEFT", "TOPLEFT", "TOP", "TOPRIGHT",
        "RIGHT", "BOTTOMRIGHT", "BOTTOM", "BOTTOMLEFT"
    };
    int idx = static_cast<int>(lock);
    return (idx >= 0 && idx < 8) ? QString::fromLatin1(names[idx]) : QStringLiteral("TOPLEFT");
}

AxisLock ContainerWidget::axisLockFromString(const QString& str)
{
    static const QMap<QString, AxisLock> map = {
        {QStringLiteral("LEFT"), AxisLock::Left},
        {QStringLiteral("TOPLEFT"), AxisLock::TopLeft},
        {QStringLiteral("TOP"), AxisLock::Top},
        {QStringLiteral("TOPRIGHT"), AxisLock::TopRight},
        {QStringLiteral("RIGHT"), AxisLock::Right},
        {QStringLiteral("BOTTOMRIGHT"), AxisLock::BottomRight},
        {QStringLiteral("BOTTOM"), AxisLock::Bottom},
        {QStringLiteral("BOTTOMLEFT"), AxisLock::BottomLeft},
    };
    return map.value(str.toUpper(), AxisLock::TopLeft);
}

QString ContainerWidget::dockModeToString(DockMode mode)
{
    switch (mode) {
    case DockMode::PanelDocked:   return QStringLiteral("PANEL");
    case DockMode::OverlayDocked: return QStringLiteral("OVERLAY");
    case DockMode::Floating:      return QStringLiteral("FLOATING");
    }
    return QStringLiteral("OVERLAY");
}

DockMode ContainerWidget::dockModeFromString(const QString& str)
{
    if (str == QStringLiteral("PANEL")) {
        return DockMode::PanelDocked;
    }
    if (str == QStringLiteral("FLOATING")) {
        return DockMode::Floating;
    }
    return DockMode::OverlayDocked;
}

QString ContainerWidget::serialize() const
{
    // Based on Thetis ucMeter.cs:1012-1038, extended with DockMode (field 23)
    QStringList p;
    p << m_id;                                                              // 0
    p << QString::number(m_rxSource);                                       // 1
    p << QString::number(m_dockedLocation.x());                             // 2
    p << QString::number(m_dockedLocation.y());                             // 3
    p << QString::number(m_dockedSize.width());                             // 4
    p << QString::number(m_dockedSize.height());                            // 5
    p << (isFloating() ? QStringLiteral("true") : QStringLiteral("false")); // 6 (Thetis compat)
    p << QString::number(m_delta.x());                                      // 7
    p << QString::number(m_delta.y());                                      // 8
    p << axisLockToString(m_axisLock);                                      // 9
    p << (m_pinOnTop ? QStringLiteral("true") : QStringLiteral("false"));   // 10
    p << (m_border ? QStringLiteral("true") : QStringLiteral("false"));     // 11
    p << m_backgroundColor.name(QColor::HexArgb);                           // 12
    p << (m_noControls ? QStringLiteral("true") : QStringLiteral("false")); // 13
    p << (m_enabled ? QStringLiteral("true") : QStringLiteral("false"));    // 14
    p << m_notes;                                                           // 15
    p << (m_containerMinimises ? QStringLiteral("true") : QStringLiteral("false")); // 16
    p << (m_autoHeight ? QStringLiteral("true") : QStringLiteral("false")); // 17
    p << (m_showOnRx ? QStringLiteral("true") : QStringLiteral("false"));   // 18
    p << (m_showOnTx ? QStringLiteral("true") : QStringLiteral("false"));   // 19
    p << (m_locked ? QStringLiteral("true") : QStringLiteral("false"));     // 20
    p << (m_containerHidesWhenRxNotUsed ? QStringLiteral("true") : QStringLiteral("false")); // 21
    p << (m_hiddenByMacro ? QStringLiteral("true") : QStringLiteral("false")); // 22
    p << dockModeToString(m_dockMode);                                      // 23 (NereusSDR extension)
    p << (m_titleBarVisible ? QStringLiteral("true") : QStringLiteral("false")); // 24 (Phase 3G-6)
    return p.join(QLatin1Char('|'));
}

bool ContainerWidget::deserialize(const QString& data)
{
    // Based on Thetis ucMeter.cs:1039-1160, backward-compatible
    if (data.isEmpty()) {
        return false;
    }

    QStringList p = data.split(QLatin1Char('|'));
    if (p.size() < 13) {
        qCWarning(lcContainer) << "deserialize: too few fields:" << p.size();
        return false;
    }

    if (p[0].isEmpty()) {
        return false;
    }
    setId(p[0]);

    bool ok = false;
    int rx = p[1].toInt(&ok);
    if (!ok) { return false; }
    // R-R3-49 / R-R3-21 (fix wave M5): slices A to D are 1..4. A value
    // outside them (a hand-edited or damaged layout) loads as the nearest
    // slice, so the title and the buttons' reasons always name one.
    setRxSource(std::clamp(rx, kFirstRxSource, kLastRxSource));

    int x = p[2].toInt(&ok); if (!ok) { return false; }
    int y = p[3].toInt(&ok); if (!ok) { return false; }
    int w = p[4].toInt(&ok); if (!ok) { return false; }
    int h = p[5].toInt(&ok); if (!ok) { return false; }
    setDockedLocation(QPoint(x, y));
    setDockedSize(QSize(w, h));

    // Field 6: Thetis floating bool — used as fallback if field 23 absent
    bool thetisFloating = (p[6].toLower() == QStringLiteral("true"));

    x = p[7].toInt(&ok); if (!ok) { return false; }
    y = p[8].toInt(&ok); if (!ok) { return false; }
    setDelta(QPoint(x, y));

    setAxisLock(axisLockFromString(p[9]));
    setPinOnTop(p[10].toLower() == QStringLiteral("true"));
    setBorder(p[11].toLower() == QStringLiteral("true"));

    QColor bgColor(p[12]);
    if (bgColor.isValid()) {
        m_backgroundColor = bgColor;
        publishUserBackground();
        m_contentHolder->setStyleSheet(
            QStringLiteral("background: %1;").arg(bgColor.name(QColor::HexArgb)));
    }

    if (p.size() > 13) { setNoControls(p[13].toLower() == QStringLiteral("true")); }
    if (p.size() > 14) { setContainerEnabled(p[14].toLower() == QStringLiteral("true")); }
    if (p.size() > 15) { setNotes(p[15]); }
    if (p.size() > 16) { setContainerMinimises(p[16].toLower() == QStringLiteral("true")); }
    if (p.size() > 17) { setAutoHeight(p[17].toLower() == QStringLiteral("true")); }
    if (p.size() > 19) {
        setShowOnRx(p[18].toLower() == QStringLiteral("true"));
        setShowOnTx(p[19].toLower() == QStringLiteral("true"));
    }
    if (p.size() > 20) { setLocked(p[20].toLower() == QStringLiteral("true")); }
    if (p.size() > 21) { setContainerHidesWhenRxNotUsed(p[21].toLower() == QStringLiteral("true")); }
    if (p.size() > 22) { setHiddenByMacro(p[22].toLower() == QStringLiteral("true")); }

    // Field 23: NereusSDR DockMode extension
    if (p.size() > 23) {
        setDockMode(dockModeFromString(p[23]));
    } else {
        setDockMode(thetisFloating ? DockMode::Floating : DockMode::OverlayDocked);
    }

    // Field 24: Phase 3G-6 title-bar visibility toggle
    if (p.size() > 24) {
        setTitleBarVisible(p[24].toLower() == QStringLiteral("true"));
    }

    qCDebug(lcContainer) << "Deserialized:" << m_id << "rx:" << m_rxSource
                          << "mode:" << dockModeToString(m_dockMode)
                          << "pos:" << m_dockedLocation << "size:" << m_dockedSize;
    return true;
}

// Phase 3P-I-a T17 — propagate hasAlex to every AntennaButtonItem
// nested inside this container's content tree. Walks all MeterWidgets
// reachable from m_content via findChildren (handles nested applet
// panels that host a header MeterWidget as well as flat content).
void ContainerWidget::setBoardCapabilities(const BoardCapabilities& caps)
{
    m_hasAlex = caps.hasAlex;
    if (!m_content) { return; }

    // The content may be the MeterWidget directly, or an applet panel
    // hosting one as its header. findChildren catches both.
    const QList<MeterWidget*> meters =
        m_content->findChildren<MeterWidget*>();
    QList<MeterWidget*> scan = meters;
    if (auto* asMeter = qobject_cast<MeterWidget*>(m_content)) {
        scan.prepend(asMeter);
    }
    for (MeterWidget* mw : scan) {
        for (MeterItem* item : mw->items()) {
            if (auto* ant = qobject_cast<AntennaButtonItem*>(item)) {
                ant->setHasAlex(m_hasAlex);
            }
        }
    }
}

} // namespace NereusSDR
