// src/gui/applets/AppletPanelWidget.h
#pragma once

#include <QWidget>
#include <QList>

class QVBoxLayout;
class QScrollArea;

namespace NereusSDR {

class AppletWidget;

// Scrollable vertical stack of applets with AetherSDR AppletPanel styling.
// This is a SINGLE widget that goes into ContainerWidget::setContent().
// It internally manages N child widgets (applets + meters) in a scrollable layout.
//
// From AetherSDR AppletPanel.cpp:
// - Root background: #0a0a18
// - Fixed width: 260px
// - Each child wrapped with a gradient title bar (16px)
// - Scroll area: QFrame::NoFrame, HScrollBarAlwaysOff, widgetResizable true
class AppletPanelWidget : public QWidget {
    Q_OBJECT
public:
    explicit AppletPanelWidget(QWidget* parent = nullptr);

    // Add an applet — wraps it with a title bar and adds to the scroll stack
    void addApplet(AppletWidget* applet);

    // Add a raw widget (e.g., MeterWidget) with a custom title
    void addWidget(QWidget* widget, const QString& title);

    QList<AppletWidget*> applets() const { return m_applets; }

private:
    QWidget* wrapWithTitleBar(QWidget* child, const QString& title);

    QScrollArea* m_scrollArea = nullptr;
    QVBoxLayout* m_stackLayout = nullptr;
    QList<AppletWidget*> m_applets;
};

} // namespace NereusSDR
