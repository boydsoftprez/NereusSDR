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
class QPushButton;
class QMenu;

namespace NereusSDR {

class AppletWidget;
class SMeterWidget;

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

    // Access the SMeterWidget installed as the fixed header.
    // Returns nullptr before the panel is built (should never happen after
    // buildUI is called from the constructor chain, but callers must guard).
    SMeterWidget* smeterWidget() const { return m_sMeter; }

    // Add an applet — wraps it with a title bar and adds to the scroll stack
    void addApplet(AppletWidget* applet);

    // Remove an applet (and its title bar wrapper) from the scroll stack.
    // The applet widget itself is hidden and reparented to nullptr (not deleted).
    void removeApplet(AppletWidget* applet);

    // Toggle visibility of an already-added applet without removing it
    // from the layout. Preserves stack position when re-shown. No-op for
    // null or unknown applets. NereusSDR-original (no Thetis equivalent).
    void setAppletVisible(AppletWidget* applet, bool visible);

    // Install a menu on the panel's top-right ☰ button. Until this is
    // called, the button is hidden. Pass nullptr to remove the menu and
    // re-hide the button.
    void setBannerMenu(QMenu* menu);

    // Add a raw widget (e.g., MeterWidget) with a custom title to the scroll area
    void addWidget(QWidget* widget, const QString& title);

    QList<AppletWidget*> applets() const { return m_applets; }

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    // Wrap a child widget with a 16 px title bar (grip dots + label).
    // When `trailing` is non-null, the title bar is bumped to 22 px to
    // accommodate the trailing widget (right-aligned, after the stretch).
    // Used by setHeaderWidget to embed the ☰ menu button in the S-Meter
    // title bar — keeps the button reachable without a dedicated row
    // that competes with the master-volume strip above.
    QWidget* wrapWithTitleBar(QWidget* child, const QString& title,
                              QWidget* trailing = nullptr);

    QVBoxLayout* m_rootLayout = nullptr;     // top-level: header + scroll
    QVBoxLayout* m_headerLayout = nullptr;   // fixed header above scroll area
    QScrollArea* m_scrollArea = nullptr;
    QVBoxLayout* m_stackLayout = nullptr;    // inside scroll area
    QList<AppletWidget*> m_applets;
    QMap<AppletWidget*, QWidget*> m_wrappers;  // applet → wrapper widget
    QWidget*      m_headerWidget  = nullptr;   // header widget for dynamic resize
    QWidget*      m_headerWrapper = nullptr;   // title-bar wrapper of the header
    float         m_headerAspect  = 0.0f;      // width/height ratio for header
    // SMeterWidget installed as the fixed header (Task 40, Phase 3P-II).
    // Non-owning after setHeaderWidget(); the title-bar wrapper owns the tree.
    // Accessed by MainWindow via smeterWidget() for Task 41/43 wiring.
    SMeterWidget* m_sMeter        = nullptr;

    // ☰ menu button. Created in the constructor (hidden, parented to
    // `this` for memory management). When setHeaderWidget runs it gets
    // re-parented into the S-Meter title bar via wrapWithTitleBar.
    QPushButton*  m_bannerMenuButton{nullptr};
};

} // namespace NereusSDR
