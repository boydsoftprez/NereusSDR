// src/gui/applets/AppletPanelWidget.cpp

// =================================================================
// src/gui/applets/AppletPanelWidget.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Scrollable applet-panel pattern (260px fixed width, 16px title
//                 bars) ported from AetherSDR `src/gui/AppletPanel.{h,cpp}`.
// =================================================================

#include "AppletPanelWidget.h"
#include "AppletWidget.h"
#include "gui/StyleConstants.h"

#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QResizeEvent>

namespace NereusSDR {

AppletPanelWidget::AppletPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    // Minimum width matches AetherSDR AppletPanel (260px), but allow
    // dynamic expansion when the user drags the splitter handle wider.
    setMinimumWidth(Style::kAppletPanelW);
    setStyleSheet(QStringLiteral("AppletPanelWidget { background: %1; }")
                      .arg(Style::kPanelBg));

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    // Fixed header area (above scroll) — for MeterWidget / S-Meter
    m_headerLayout = new QVBoxLayout;
    m_headerLayout->setContentsMargins(0, 0, 0, 0);
    m_headerLayout->setSpacing(0);
    m_rootLayout->addLayout(m_headerLayout);

    // Scrollable area for the applet stack (below header)
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: %1; border: none; }"
        "QScrollBar:vertical {"
        "  background: %1; width: 8px; border: none;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %2; border-radius: 4px; min-height: 20px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
    ).arg(Style::kPanelBg, Style::kGroove));

    // Inner widget for the scroll area
    auto* stackWidget = new QWidget(m_scrollArea);
    stackWidget->setStyleSheet(QStringLiteral("background: %1;").arg(Style::kPanelBg));
    m_stackLayout = new QVBoxLayout(stackWidget);
    // 8 px matches the QScrollBar:vertical width in the stylesheet above;
    // Qt::ScrollBarAsNeeded means the gutter is wasted when the bar hides,
    // but 8 px is negligible and avoids a layout reflow on bar show/hide.
    m_stackLayout->setContentsMargins(0, 0, 8, 0);
    m_stackLayout->setSpacing(0);
    m_stackLayout->addStretch();

    m_scrollArea->setWidget(stackWidget);
    m_rootLayout->addWidget(m_scrollArea);
}

void AppletPanelWidget::setHeaderWidget(QWidget* widget, const QString& title,
                                         float aspectRatio)
{
    if (!widget) { return; }
    clearHeaderWidget();

    m_headerAspect = aspectRatio;
    m_headerWidget = widget;

    // Let the widget expand to fill width; height set dynamically in resizeEvent
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QWidget* wrapped = wrapWithTitleBar(widget, title);
    wrapped->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_headerWrapper = wrapped;
    m_headerLayout->addWidget(wrapped);

    // Set initial height based on current width
    int h = qMax(80, static_cast<int>(width() / aspectRatio));
    widget->setFixedHeight(h);
}

void AppletPanelWidget::clearHeaderWidget()
{
    if (m_headerWrapper) {
        m_headerLayout->removeWidget(m_headerWrapper);
        // Detach from the widget tree SYNCHRONOUSLY before deleteLater.
        // The wrapper's MeterWidget child is a WA_NativeWindow QRhiWidget
        // with a live D3D11 swapchain. If the wrapper lingers in this
        // panel's parent chain across an upcoming container reparent, the
        // new MeterWidget's CreateSwapChainForHwnd returns E_ACCESSDENIED
        // — Windows refuses to attach a second swapchain while the old
        // native child is still under the parent HWND.
        m_headerWrapper->hide();
        m_headerWrapper->setParent(nullptr);
        m_headerWrapper->deleteLater();
        m_headerWrapper = nullptr;
    }
    m_headerWidget = nullptr;
    m_headerAspect = 0.0f;
}

void AppletPanelWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_headerWidget && m_headerAspect > 0.0f) {
        int w = event->size().width();
        int totalH = event->size().height();
        int h = static_cast<int>(w / m_headerAspect);
        // Clamp: min 80px, max 40% of total panel height so applets aren't crowded
        h = qBound(80, h, totalH * 40 / 100);
        m_headerWidget->setFixedHeight(h);
    }
}

void AppletPanelWidget::addApplet(AppletWidget* applet)
{
    if (!applet) { return; }
    if (m_applets.contains(applet)) { return; }  // already present
    m_applets.append(applet);

    applet->setParent(this);
    applet->show();
    QWidget* wrapped = wrapWithTitleBar(applet, applet->appletTitle());
    m_wrappers[applet] = wrapped;

    // Insert before the trailing stretch
    int idx = m_stackLayout->count() - 1;
    m_stackLayout->insertWidget(idx, wrapped);
}

void AppletPanelWidget::removeApplet(AppletWidget* applet)
{
    if (!applet) { return; }
    if (!m_applets.contains(applet)) { return; }

    QWidget* wrapper = m_wrappers.value(applet, nullptr);
    if (wrapper) {
        m_stackLayout->removeWidget(wrapper);
        // Reparent applet out of the wrapper before deleting it
        applet->setParent(nullptr);
        applet->hide();
        wrapper->deleteLater();
        m_wrappers.remove(applet);
    }
    m_applets.removeOne(applet);
}

void AppletPanelWidget::addWidget(QWidget* widget, const QString& title)
{
    if (!widget) { return; }

    QWidget* wrapped = wrapWithTitleBar(widget, title);

    // Insert before the trailing stretch
    int idx = m_stackLayout->count() - 1;
    m_stackLayout->insertWidget(idx, wrapped);
}

QWidget* AppletPanelWidget::wrapWithTitleBar(QWidget* child, const QString& title)
{
    // Wrapper container
    auto* wrapper = new QWidget(this);
    auto* wrapLayout = new QVBoxLayout(wrapper);
    wrapLayout->setContentsMargins(0, 0, 0, 0);
    wrapLayout->setSpacing(0);

    // Title bar — from AetherSDR AppletTitleBar:
    // 16px height, gradient, grip dots + title label
    auto* titleBar = new QWidget(wrapper);
    titleBar->setFixedHeight(Style::kTitleBarH);
    titleBar->setStyleSheet(Style::titleBarStyle());

    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(2, 0, 4, 0);
    titleLayout->setSpacing(4);

    // Grip dots (⋮⋮) — from AetherSDR: #607080, 10px
    auto* grip = new QLabel(QStringLiteral("\u22EE\u22EE"), titleBar);
    grip->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; background: transparent; }"
    ).arg(Style::kTextScale));
    titleLayout->addWidget(grip);

    // Title label — from AetherSDR: #8aa8c0, 10px bold
    auto* label = new QLabel(title, titleBar);
    label->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; font-weight: bold;"
        " background: transparent; }"
    ).arg(Style::kTitleText));
    titleLayout->addWidget(label);
    titleLayout->addStretch();

    wrapLayout->addWidget(titleBar);
    wrapLayout->addWidget(child);

    return wrapper;
}

} // namespace NereusSDR
