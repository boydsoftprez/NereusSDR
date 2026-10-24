// src/gui/styles/PopupMenuStyle.h (NereusSDR)
//
// Universal dark-palette QMenu stylesheet — applied to every antenna
// popup menu (VFO Flag, RxApplet, SpectrumOverlayPanel, meter items,
// future band/mode menus). Fixes issue #98 where Ubuntu 25.10's default
// theme rendered antenna menu items as dark-on-dark (only visible on
// hover). Use via:
//
//     ScopedChildWidget<QMenu> menuOwner(this);  // gui/ScopedChildWidget.h
//     QMenu& menu = *menuOwner.get();
//     menu.setStyleSheet(QString::fromLatin1(kPopupMenu));
//
// Every call site MUST set this stylesheet — enforced by
// tests/tst_popup_style_coverage.cpp.
//
// Phase 3P-I-a. Design: docs/architecture/antenna-routing-design.md §6.1 Rule 2.

#pragma once

namespace NereusSDR {

inline constexpr const char* kPopupMenu =
    "QMenu {"
    "  background: #1a2a3a;"
    "  color: #e0e8f0;"
    "  border: 1px solid #304050;"
    "}"
    "QMenu::item {"
    "  padding: 4px 20px;"
    "}"
    "QMenu::item:selected {"
    "  background: #2a5a8a;"
    "  color: #ffffff;"
    "}"
    "QMenu::item:disabled {"
    "  color: #587080;"
    "}";

} // namespace NereusSDR
