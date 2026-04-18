// src/gui/applets/AppletPanelWidget.h

// =================================================================
// src/gui/applets/AppletPanelWidget.h  (NereusSDR)
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

#pragma once

#include <QWidget>
#include <QList>
#include <QMap>

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

    // Set a header widget (e.g., MeterWidget) that stays visible above
    // the scroll area. Height scales dynamically with width using the
    // given aspect ratio (default 2.0 = AetherSDR's 280:140). Calling
    // again replaces the existing header (old wrapper + widget deleted).
    void setHeaderWidget(QWidget* widget, const QString& title, float aspectRatio = 2.0f);

    // Remove the current header widget and its title-bar wrapper without
    // installing a new one. Safe to call when no header is set. Used by
    // ContainerManager to detach the MeterWidget before a top-level
    // reparent (Qt 6.11.0 D3D11 QRhiWidget does not survive the HWND
    // recreation that reparent triggers on Windows).
    void clearHeaderWidget();

    QWidget* headerWidget() const { return m_headerWidget; }

    // Add an applet — wraps it with a title bar and adds to the scroll stack
    void addApplet(AppletWidget* applet);

    // Remove an applet (and its title bar wrapper) from the scroll stack.
    // The applet widget itself is hidden and reparented to nullptr (not deleted).
    void removeApplet(AppletWidget* applet);

    // Add a raw widget (e.g., MeterWidget) with a custom title to the scroll area
    void addWidget(QWidget* widget, const QString& title);

    QList<AppletWidget*> applets() const { return m_applets; }

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QWidget* wrapWithTitleBar(QWidget* child, const QString& title);

    QVBoxLayout* m_rootLayout = nullptr;     // top-level: header + scroll
    QVBoxLayout* m_headerLayout = nullptr;   // fixed header above scroll area
    QScrollArea* m_scrollArea = nullptr;
    QVBoxLayout* m_stackLayout = nullptr;    // inside scroll area
    QList<AppletWidget*> m_applets;
    QMap<AppletWidget*, QWidget*> m_wrappers;  // applet → wrapper widget
    QWidget* m_headerWidget = nullptr;         // header widget for dynamic resize
    QWidget* m_headerWrapper = nullptr;        // title-bar wrapper of the header
    float m_headerAspect = 0.0f;               // width/height ratio for header
};

} // namespace NereusSDR
