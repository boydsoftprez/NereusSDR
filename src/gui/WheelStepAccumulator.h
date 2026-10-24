#pragma once

// =================================================================
// src/gui/WheelStepAccumulator.h  (NereusSDR)
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
//   2026-10-18 - Wheel and trackpad step accumulator ported from AetherSDR
//                 SpectrumWidget::wheelEvent (src/gui/SpectrumWidget.cpp:
//                 11286-11328 [@1e0718ad]) for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code (Claude Opus 5.5). A trackpad is told by its
//                 scroll phase or touchpad device, NereusSDR's own test,
//                 not upstream's pixelDelta test (a Mac mouse wheel fills
//                 pixelDelta too).
// =================================================================

#include <QtGlobal>

#include <functional>

#include "gui/NereusGuiExport.h"

class QWheelEvent;

namespace NereusSDR {

// Turns a stream of wheel events into whole steps: -1, 0 or +1 per event.
//
// A trackpad reports pixel deltas many times a second; read as a mouse
// notch each one moved the radio a whole step, so a short swipe ran the
// receiver off across the band. A mouse wheel reports angle deltas, one
// notch being 120; some desktops send 960 per notch, which must still be
// one step.
//
// Which is which is NereusSDR's call, not AetherSDR's: upstream treats any
// event with a pixelDelta as a trackpad's, but Qt's macOS plugin fills
// pixelDelta for a plain mouse wheel too (with Qt::NoScrollPhase), which
// would give a Mac mouse the trackpad's 15 px and 50 ms rules. An event is a
// trackpad's when it carries a scroll phase (only gesture devices send one)
// or Qt names its device a touchpad; every other event is a mouse wheel's.
class NEREUS_GUI_EXPORT WheelStepAccumulator {
public:
    // -1, 0 or +1 for this event. Trackpad (isTrackpad): ignores
    // Qt::ScrollMomentum, ignores events whose |x| > |y|, resets the pixel
    // accumulator on Qt::ScrollBegin, one step per kTrackpadPixelsPerStep of
    // accumulated y (pixelDelta, or angleDelta / kAngleUnitsPerPixel when
    // the pixelDelta is null; the remainder carries over), clamped to +/-1 per event, and at most one step
    // per kTrackpadMinStepIntervalMs. Mouse wheel (everything else):
    // accumulates angleDelta().y(), one step per kAngleUnitsPerStep, clamped
    // to +/-1 per event, no rate limit; its pixelDelta is not read.
    int stepsFor(const QWheelEvent& event);
    // True when event has a scroll phase (not Qt::NoScrollPhase) or comes
    // from a QInputDevice::DeviceType::TouchPad. NereusSDR's test.
    static bool isTrackpad(const QWheelEvent& event);
    // Test seam: elapsed-time source in ms; defaults to a QElapsedTimer.
    void setClockForTest(std::function<qint64()> nowMs);

    // From AetherSDR src/gui/SpectrumWidget.cpp:11297 [@1e0718ad]
    static constexpr int kTrackpadPixelsPerStep = 15;
    // From AetherSDR src/gui/SpectrumWidget.cpp:11305 [@1e0718ad]
    static constexpr int kTrackpadMinStepIntervalMs = 50;
    // From AetherSDR src/gui/SpectrumWidget.cpp:11317 [@1e0718ad]
    static constexpr int kAngleUnitsPerStep = 120;
    // Angle units per pixel when a trackpad event reports angle only:
    // angleDelta is in eighths of a degree, and eight units are taken as a
    // pixel, so one 120-unit notch is 15 px, one step. NereusSDR's, not
    // upstream's.
    static constexpr int kAngleUnitsPerPixel = 8;

private:
    qint64 nowMs() const;

    std::function<qint64()> m_clock;
    int    m_pixelAccum{0};   // in angle units (kAngleUnitsPerPixel per px)
    int    m_angleAccum{0};
    qint64 m_lastTrackpadStepMs{0};
    bool   m_haveTrackpadStep{false};
};

} // namespace NereusSDR
