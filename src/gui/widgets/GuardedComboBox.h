#pragma once

// =================================================================
// src/gui/widgets/GuardedComboBox.h  (NereusSDR)
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
//                 Qt6 pattern informed by AetherSDR
//                 `src/gui/GuardedSlider.h:56-79`.
// =================================================================

// NereusSDR native widget; Qt skeleton pattern informed by AetherSDR
// GuardedSlider.h (GuardedComboBox class, lines ~56-76).
// AetherSDR rationale preserved below; issue numbers stripped.
//
// When controls are locked, blocks mouse press to prevent opening the
// dropdown. Only responds to wheel events when the popup is visible,
// preventing accidental value changes when scrolling the applet panel.
// Normal wheel scrolling through the list works when the user has
// clicked to open the dropdown.

#include <QComboBox>
#include <QAbstractItemView>
#include <QMouseEvent>
#include <QWheelEvent>
#include "GuardedSlider.h"  // for ControlsLock

namespace NereusSDR {

// GuardedComboBox — QComboBox subclass with two guards:
//   (a) wheelEvent only forwards to QComboBox when the popup is open,
//       so scrolling a panel past a closed combo doesn't silently
//       change its value.
//   (b) mousePressEvent is ignored when ControlsLock::isLocked(),
//       preventing the operator from opening the dropdown while the
//       controls panel is locked.
class GuardedComboBox : public QComboBox {
public:
    using QComboBox::QComboBox;

    void wheelEvent(QWheelEvent* ev) override {
        if (view() && view()->isVisible()) {
            QComboBox::wheelEvent(ev);  // popup open — scroll the list
        } else {
            ev->ignore();  // popup closed — let parent handle scroll
        }
    }

    void mousePressEvent(QMouseEvent* ev) override {
        if (ControlsLock::isLocked()) {
            ev->ignore();
            return;
        }
        QComboBox::mousePressEvent(ev);
    }
};

}  // namespace NereusSDR
