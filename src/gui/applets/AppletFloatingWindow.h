// =================================================================
// src/gui/applets/AppletFloatingWindow.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file.
//
// Top-level window that hosts an AppletWidget popped out of the
// right-hand AppletPanelWidget stack.  Same shape as PanFloatingWindow:
// the applet is reparented into this window's layout, closing the
// window asks the panel to dock it back, and every move / resize
// reports saveGeometry() so the panel can persist it.
//
// Modification history (NereusSDR):
//   2026-09-09 — Created for the floating AM Mod Monitor (Lee,
//                 AI-assisted via Anthropic Claude Code).
// =================================================================
#pragma once

#include <QPointer>
#include <QWidget>

namespace NereusSDR {

class AppletWidget;

class AppletFloatingWindow : public QWidget {
    Q_OBJECT
public:
    AppletFloatingWindow(AppletWidget* applet, QWidget* parent = nullptr);
    ~AppletFloatingWindow() override;

    AppletWidget* applet() const { return m_applet.data(); }

    /// Detach the applet from this window's layout without deleting it,
    /// so the panel can take it back.
    AppletWidget* releaseApplet();

signals:
    void dockRequested(AppletWidget* applet);
    void geometryChanged(AppletWidget* applet, const QByteArray& geometry);

protected:
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QPointer<AppletWidget> m_applet;
};

} // namespace NereusSDR
