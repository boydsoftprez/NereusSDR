#pragma once

// =================================================================
// src/gui/widgets/CenterMarkSlider.h  (NereusSDR)
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
//                 `CenterMarkSlider` ported from AetherSDR
//                 `src/gui/VfoWidget.cpp:79-94`.
// =================================================================

#include "ResetSlider.h"
#include <QPainter>
#include <QPaintEvent>

namespace NereusSDR {

// CenterMarkSlider — ported from AetherSDR src/gui/VfoWidget.cpp:79-94
// ResetSlider that paints a small antialiased circle at the slider groove
// centre, providing a visual reference for the midpoint (e.g. pan = 0,
// APF offset = 0). Colour #608090, radius 2.5px.
class CenterMarkSlider : public ResetSlider {
public:
    explicit CenterMarkSlider(int resetVal, Qt::Orientation o, QWidget* parent = nullptr)
        : ResetSlider(resetVal, o, parent) {}

protected:
    void paintEvent(QPaintEvent* ev) override {
        ResetSlider::paintEvent(ev);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int cx = width() / 2;
        int cy = height() / 2;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#608090"));
        p.drawEllipse(QPointF(cx, cy), 2.5, 2.5);
    }
};

} // namespace NereusSDR
