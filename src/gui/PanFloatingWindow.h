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
//   2026-08-08  J.J. Boyd / KG4VCF  Bench report: a floated pan froze
//                                    ("QRhiWidget: No QRhi" per display
//                                    tick) and arrived with no container
//                                    chrome. Constructor now takes the
//                                    main window as parent and the applet
//                                    arrives through adoptApplet() /
//                                    leaves through takeApplet(), so the
//                                    reparent never passes through
//                                    setParent(nullptr). Adds per-pan
//                                    geometry persistence
//                                    (FloatingPan_<panId>_Geometry) and
//                                    makes closeEvent dock instead of
//                                    destroy. Ported from AetherSDR
//                                    src/gui/PanFloatingWindow.{h,cpp}
//                                    and src/gui/PanadapterStack.cpp
//                                    [@1e0718ad]. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================
#pragma once

#include <QPointer>
#include <QWidget>
#include <QString>

class QVBoxLayout;

namespace NereusSDR {

class PanadapterApplet;

/// Top-level QWidget wrapping a PanadapterApplet for multi-monitor detach.
/// Ported structurally from AetherSDR src/gui/PanFloatingWindow.{h,cpp}
/// [@0cd4559]. The applet is reparented under this window's layout, so
/// destroying the window destroys the applet.
class PanFloatingWindow : public QWidget {
    Q_OBJECT
public:
    /// `parent` should be the main window. Qt::Window still makes this a
    /// top-level (so it can live on a second monitor); the parent only
    /// governs z-order and lifetime, which is what keeps a popped-out pan
    /// above the console instead of behind it, without the
    /// WindowStaysOnTopHint that would float it over other applications too.
    /// From AetherSDR src/gui/PanadapterStack.cpp:800-808 [@1e0718ad].
    explicit PanFloatingWindow(QWidget* parent = nullptr);
    ~PanFloatingWindow() override;

    /// Reparent `applet` into this window's layout.
    ///
    /// Goes straight from the splitter into this layout via addWidget, never
    /// through setParent(nullptr): an intermediate top-level state creates
    /// and destroys a transient NSWindow, which on macOS can corrupt the main
    /// window's NSResponder chain and freeze input everywhere.
    /// From AetherSDR src/gui/PanFloatingWindow.cpp:34-56 [@1e0718ad].
    void adoptApplet(PanadapterApplet* applet);

    /// Hand the applet back, reparented directly onto `newParent`.
    ///
    /// Takes the destination rather than orphaning to nullptr, for the same
    /// transient-NSWindow reason as adoptApplet.
    PanadapterApplet* takeApplet(QWidget* newParent);

    PanadapterApplet* applet() const;
    QString panId() const;

    /// Public hook (test seam + Stack-side trigger) that re-emits
    /// dockRequested without needing the user to close the window.
    void requestDock();

    /// Per-pan window geometry, keyed "FloatingPan_<panId>_Geometry".
    /// From AetherSDR src/gui/PanFloatingWindow.cpp:98-112 [@1e0718ad].
    void saveWindowGeometry();
    void restoreWindowGeometry();

signals:
    void dockRequested(const QString& panId);
    void geometryChanged(const QString& panId, const QByteArray& geometry);

protected:
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateWindowTitle();

    QPointer<PanadapterApplet> m_applet;  // reparented under our layout
    QVBoxLayout*               m_layout {nullptr};
};

} // namespace NereusSDR
