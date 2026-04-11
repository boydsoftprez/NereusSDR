// src/gui/applets/AppletPanelWidget.cpp
#include "AppletPanelWidget.h"
#include "AppletWidget.h"
#include "gui/StyleConstants.h"

#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

namespace NereusSDR {

AppletPanelWidget::AppletPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    // From AetherSDR AppletPanel.cpp: fixedWidth 260, background #0a0a18
    setFixedWidth(Style::kAppletPanelW);
    setStyleSheet(QStringLiteral("AppletPanelWidget { background: %1; }")
                      .arg(Style::kPanelBg));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Scrollable area for the applet stack
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
    m_stackLayout->setContentsMargins(0, 0, 0, 0);
    m_stackLayout->setSpacing(0);
    m_stackLayout->addStretch();

    m_scrollArea->setWidget(stackWidget);
    root->addWidget(m_scrollArea);
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
