// =================================================================
// src/gui/applets/AppletFloatingWindow.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original file.  See header.
// =================================================================
#include "gui/applets/AppletFloatingWindow.h"

#include "gui/applets/AppletWidget.h"
#include "gui/StyleConstants.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMoveEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace NereusSDR {

AppletFloatingWindow::AppletFloatingWindow(AppletWidget* applet, QWidget* parent)
    // Qt::Tool keeps the window above the main window (and hides with the
    // app on macOS) without stealing the dock/taskbar slot a Qt::Window would.
    : QWidget(parent, Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint
                      | Qt::CustomizeWindowHint)
    , m_applet(applet)
{
    setWindowTitle(QStringLiteral("NereusSDR - %1")
                       .arg(applet ? applet->appletTitle() : QStringLiteral("Applet")));
    setStyleSheet(QStringLiteral("background:%1;").arg(QLatin1String(Style::kPanelBg)));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Slim header with a "dock" button, mirroring the in-panel title bar.
    auto* bar = new QWidget(this);
    bar->setFixedHeight(Style::kTitleBarH);
    bar->setStyleSheet(Style::titleBarStyle());
    auto* barLay = new QHBoxLayout(bar);
    barLay->setContentsMargins(6, 0, 4, 0);
    barLay->setSpacing(4);
    auto* label = new QLabel(applet ? applet->appletTitle() : QString(), bar);
    label->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; font-weight: bold; background: transparent; }")
        .arg(QLatin1String(Style::kTitleText)));
    barLay->addWidget(label);
    barLay->addStretch();
    auto* dock = new QPushButton(QStringLiteral("↙ Dock"), bar);
    dock->setToolTip(QStringLiteral("Put the applet back into the right-hand panel."));
    dock->setFixedHeight(16);
    dock->setCursor(Qt::PointingHandCursor);
    dock->setStyleSheet(QStringLiteral(
        "QPushButton { color: %1; background: transparent; border: 1px solid %2;"
        " border-radius: 3px; font-size: 10px; padding: 0 6px; }"
        "QPushButton:hover { background: %3; }")
        .arg(QLatin1String(Style::kTitleText), QLatin1String(Style::kTitleBorder),
             QLatin1String(Style::kButtonHover)));
    connect(dock, &QPushButton::clicked, this, [this]() {
        emit dockRequested(m_applet.data());
    });
    barLay->addWidget(dock);
    layout->addWidget(bar);

    if (applet) {
        layout->addWidget(applet, 1);
        applet->show();
    }
    resize(360, 420);
}

AppletFloatingWindow::~AppletFloatingWindow() = default;

AppletWidget* AppletFloatingWindow::releaseApplet()
{
    AppletWidget* a = m_applet.data();
    if (a && layout()) {
        layout()->removeWidget(a);
        a->setParent(nullptr);
    }
    m_applet.clear();
    return a;
}

void AppletFloatingWindow::closeEvent(QCloseEvent* event)
{
    // Closing means "dock it back", never "lose the applet".
    emit dockRequested(m_applet.data());
    event->ignore();
}

void AppletFloatingWindow::moveEvent(QMoveEvent* /*event*/)
{
    emit geometryChanged(m_applet.data(), saveGeometry());
}

void AppletFloatingWindow::resizeEvent(QResizeEvent* /*event*/)
{
    emit geometryChanged(m_applet.data(), saveGeometry());
}

} // namespace NereusSDR
