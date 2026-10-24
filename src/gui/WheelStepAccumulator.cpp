// =================================================================
// src/gui/WheelStepAccumulator.cpp  (NereusSDR)
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

#include "gui/WheelStepAccumulator.h"

#include <QElapsedTimer>
#include <QPointingDevice>
#include <QWheelEvent>

#include <utility>

namespace NereusSDR {

void WheelStepAccumulator::setClockForTest(std::function<qint64()> nowMs)
{
    m_clock = std::move(nowMs);
}

qint64 WheelStepAccumulator::nowMs() const
{
    if (m_clock) {
        return m_clock();
    }
    static QElapsedTimer timer;
    if (!timer.isValid()) {
        timer.start();
    }
    return timer.elapsed();
}

// From AetherSDR src/gui/SpectrumWidget.cpp:11286-11328 [@1e0718ad].
// NereusSDR differences: a Qt::ScrollBegin clears the pixel accumulator so a
// leftover from the last swipe cannot add a step to the next one, and the
// mouse wheel has no rate limit (two quick notches are two steps).
//
// The trackpad test is NereusSDR's, not AetherSDR's. Upstream calls an event
// a trackpad's when its pixelDelta is not null. Qt's macOS plugin fills
// pixelDelta for a plain mouse wheel too (its line delta times an estimated
// line height, with Qt::NoScrollPhase), so upstream's test would put a Mac
// mouse through the 15 px and 50 ms rules: a slow notch under 15 px gave no
// step, and quick notches were dropped. A trackpad is told here by its
// scroll phase (only gesture devices send one) or by Qt naming the device a
// touchpad; every other event steps by its angle delta.
bool WheelStepAccumulator::isTrackpad(const QWheelEvent& event)
{
    if (event.phase() != Qt::NoScrollPhase) {
        return true;
    }
    const QPointingDevice* device = event.pointingDevice();
    return device && device->type() == QInputDevice::DeviceType::TouchPad;
}

int WheelStepAccumulator::stepsFor(const QWheelEvent& event)
{
    // [original inline comment from src/gui/SpectrumWidget.cpp:11286 [@1e0718ad]]
    // Handle both trackpad (pixelDelta) and mouse wheel (angleDelta)
    int steps = 0;
    // A new swipe starts from nothing. macOS sends its ScrollBegin with no
    // pixel delta, so this is checked before the travel is read below.
    if (event.phase() == Qt::ScrollBegin) {
        m_pixelAccum = 0;
    }
    if (isTrackpad(event)) {
        // [original inline comments from src/gui/SpectrumWidget.cpp:11289-11290 [@1e0718ad]]
        // Trackpad: accumulate pixel delta; 1 step per ~15px
        // Ignore momentum (inertial) scrolling
        if (event.phase() == Qt::ScrollMomentum) {
            return 0;
        }
        // Travel is kept in angle units, kAngleUnitsPerPixel to a pixel, so a
        // phased event that carries no pixel delta (angle only, as a
        // libinput touchpad may send) counts at the same rate as pixels and
        // its remainder adds up with the rest.
        const QPoint travel = event.pixelDelta().isNull()
                                  ? event.angleDelta()
                                  : event.pixelDelta() * kAngleUnitsPerPixel;
        // [original inline comment from src/gui/SpectrumWidget.cpp:11292 [@1e0718ad]]
        // Ignore horizontal-dominant swipes
        if (qAbs(travel.x()) > qAbs(travel.y())) {
            return 0;
        }
        constexpr int kTravelPerStep = kTrackpadPixelsPerStep * kAngleUnitsPerPixel;
        m_pixelAccum += travel.y();
        steps = m_pixelAccum / kTravelPerStep;
        m_pixelAccum -= steps * kTravelPerStep;
        // [original inline comments from src/gui/SpectrumWidget.cpp:11299-11300 [@1e0718ad]]
        // Clamp to ±1: on Linux/libinput regular mice often report pixelDelta
        // (e.g. 120px per notch) which would produce 8 steps and an 8× jump.
        steps = qBound(-1, steps, 1);
        // [original inline comment from src/gui/SpectrumWidget.cpp:11302 [@1e0718ad]]
        // Debounce: same 50ms window as the angleDelta path below (#2150)
        if (steps != 0) {
            const qint64 now = nowMs();
            if (m_haveTrackpadStep && now - m_lastTrackpadStepMs < kTrackpadMinStepIntervalMs) {
                steps = 0;
            } else {
                m_lastTrackpadStepMs = now;
                m_haveTrackpadStep = true;
            }
        }
    } else {
        // [original inline comments from src/gui/SpectrumWidget.cpp:11312-11315 [@1e0718ad]]
        // Standard mouse wheel: angleDelta is in 1/8° units, one notch = 120.
        // Some desktops (KDE Plasma, Cinnamon) send inflated deltas
        // (e.g. 960 per notch) or multiple rapid events per physical notch.
        // Clamp to ±1 per event AND debounce within 80ms window. (#504, #556)
        // NereusSDR keeps the clamp and drops the debounce (upstream
        // :11320-11327): a mouse notch is a deliberate step.
        m_angleAccum += event.angleDelta().y();
        steps = m_angleAccum / kAngleUnitsPerStep;
        m_angleAccum -= steps * kAngleUnitsPerStep;
        steps = qBound(-1, steps, 1);
    }
    return steps;
}

} // namespace NereusSDR
