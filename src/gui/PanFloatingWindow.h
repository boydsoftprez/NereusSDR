// no-port-check: AetherSDR-derived NereusSDR file. Top-level QWidget
// wrapper for detaching a PanadapterApplet to a second monitor is adapted
// structurally from AetherSDR src/gui/PanFloatingWindow.{h,cpp} [@0cd4559].
// Registered in docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanFloatingWindow.h  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanFloatingWindow.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic D Task 8.
//                                    Top-level multi-monitor detach
//                                    window ported structurally from
//                                    AetherSDR src/gui/PanFloatingWindow
//                                    .{h,cpp} [@0cd4559]. Wraps a
//                                    PanadapterApplet (ownership taken
//                                    by the wrapping QVBoxLayout) and
//                                    emits dockRequested on closeEvent
//                                    so PanadapterStack can re-attach.
//                                    moveEvent / resizeEvent emit
//                                    geometryChanged so an AppSettings
//                                    consumer can persist per-pan
//                                    geometry; that consumer wiring
//                                    lands in Sub-Epic D Task 15.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================
#pragma once

#include <QWidget>
#include <QString>

namespace NereusSDR {

class PanadapterApplet;

/// Top-level QWidget wrapping a PanadapterApplet for multi-monitor detach.
/// Ported structurally from AetherSDR src/gui/PanFloatingWindow.{h,cpp}
/// [@0cd4559]. The applet is reparented under this window's layout, so
/// destroying the window destroys the applet.
class PanFloatingWindow : public QWidget {
    Q_OBJECT
public:
    PanFloatingWindow(PanadapterApplet* applet, QWidget* parent = nullptr);
    ~PanFloatingWindow() override;

    PanadapterApplet* applet() const { return m_applet; }
    QString panId() const;

    /// Public hook (test seam + Stack-side trigger) that re-emits
    /// dockRequested without needing the user to close the window.
    void requestDock();

signals:
    void dockRequested(const QString& panId);
    void geometryChanged(const QString& panId, const QByteArray& geometry);

protected:
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    PanadapterApplet* m_applet {nullptr};  // reparented under our layout
};

} // namespace NereusSDR
