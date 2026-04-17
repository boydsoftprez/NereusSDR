#pragma once

// =================================================================
// src/gui/widgets/ResetSlider.h  (NereusSDR)
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
//                 `ResetSlider` ported from AetherSDR
//                 `src/gui/VfoWidget.cpp:68-76`.
// =================================================================

#include "GuardedSlider.h"
#include <QMouseEvent>

namespace NereusSDR {

// ResetSlider — ported from AetherSDR src/gui/VfoWidget.cpp:68-76
// GuardedSlider that snaps back to a configurable reset value on double-click.
// Used for AF gain, pan, APF tune, and other sliders that have a meaningful
// "default" position the operator may want to restore quickly.
class ResetSlider : public GuardedSlider {
public:
    explicit ResetSlider(int resetVal, Qt::Orientation o, QWidget* parent = nullptr)
        : GuardedSlider(o, parent), m_resetVal(resetVal) {}

protected:
    void mouseDoubleClickEvent(QMouseEvent*) override { setValue(m_resetVal); }

private:
    int m_resetVal;
};

} // namespace NereusSDR
