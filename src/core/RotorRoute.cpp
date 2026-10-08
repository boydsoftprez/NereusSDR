// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/RotorRoute.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/core/BeamHeading.cpp [@551576e], original header from Longpath
//   source is included below.
//
// Longpath (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath)
// is a fork of NereusSDR distributed under the GNU General Public
// License version 3 (its root LICENSE). Upstream source has no
// top-of-file GPL header; project-level LICENSE applies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08: Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted transformation via Anthropic
//               Claude Code. Rotor control plan, Task 3a. wrap360() and
//               plan() ported; plan() split into planFree (Stop::None)
//               and planOnSpan, which takes the rotor's span position and
//               a range of up to 450 degrees so a heading in the overlap
//               has two span positions and the nearer wins. wrap360()
//               never returns 360 itself. SpanTracker and the offset
//               helpers are NereusSDR's own.
// =================================================================
//
// --- From BeamHeading.cpp ---
//
// =================================================================
// src/core/BeamHeading.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See BeamHeading.h for why the end stop is the
// part that matters.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/RotorRoute.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR::RotorRoute {

namespace {

// The signed difference wrapped into -180..180: the shorter way round.
double shorterWay(double d)
{
    if (d > 180.0)  { d -= 360.0; }
    if (d < -180.0) { d += 360.0; }
    return d;
}

} // namespace

// From Longpath src/core/BeamHeading.cpp:22-27 [@551576e]
double wrap360(double deg)
{
    double d = std::fmod(deg, 360.0);
    if (d < 0.0) { d += 360.0; }
    // NereusSDR: fmod of a tiny negative angle plus 360 can round to 360
    // itself, and 360 is 0.
    if (d >= 360.0) { d = 0.0; }
    return d;
}

double applyOffset(double reportedDeg, double offsetDeg)
{
    return wrap360(reportedDeg + offsetDeg);
}

double removeOffset(double targetDeg, double offsetDeg)
{
    return wrap360(targetDeg - offsetDeg);
}

// From Longpath src/core/BeamHeading.cpp:72 [@551576e]
double stopCompassDeg(EndStop stop)
{
    return (stop == EndStop::South) ? 180.0 : 0.0;
}

double compassForSpan(EndStop stop, double spanDeg)
{
    return wrap360(stopCompassDeg(stop) + spanDeg);
}

double clampRangeDeg(double rangeDeg)
{
    if (!std::isfinite(rangeDeg)) { return kFullTurnDeg; }
    return std::clamp(rangeDeg, kFullTurnDeg, kOverlapRangeDeg);
}

SpanPositions spanPositions(EndStop stop, double rangeDeg, double compassDeg)
{
    SpanPositions p;
    if (stop == EndStop::None || !std::isfinite(compassDeg)) { return p; }
    const double range = clampRangeDeg(rangeDeg);
    // From Longpath src/core/BeamHeading.cpp:73-74 [@551576e], the
    // rotation that puts the stop at 0.
    const double s = wrap360(compassDeg - stopCompassDeg(stop));
    p.count = 1;
    p.first = s;
    if (s + 360.0 <= range) {
        p.count = 2;
        p.second = s + 360.0;
    }
    return p;
}

// From Longpath src/core/BeamHeading.cpp:51-67 [@551576e]
Move planFree(double fromDeg, double toDeg)
{
    Move m;
    m.targetDeg = wrap360(toDeg);
    const double from = wrap360(fromDeg);

    // Free to rotate: take whichever direction is shorter. The
    // signed difference wrapped into -180..180 IS that direction.
    m.travelDeg = shorterWay(m.targetDeg - from);
    m.routeKnown = std::isfinite(fromDeg) && std::isfinite(toDeg);
    if (!m.routeKnown) { m.travelDeg = 0.0; }
    return m;
}

// From Longpath src/core/BeamHeading.cpp:69-89 [@551576e]
Move planOnSpan(double fromSpanDeg, double toDeg, EndStop stop, double rangeDeg)
{
    if (stop == EndStop::None) {
        return planFree(compassForSpan(stop, fromSpanDeg), toDeg);
    }

    Move m;
    m.targetDeg = wrap360(toDeg);
    const double range = clampRangeDeg(rangeDeg);
    if (!std::isfinite(fromSpanDeg) || fromSpanDeg < 0.0 || !std::isfinite(toDeg)) {
        // Where the rotor is on its span is not known yet, so neither is
        // the way it will go.
        return m;
    }
    const double a = std::clamp(fromSpanDeg, 0.0, range);

    // With a stop, the rotor lives on one continuous span and cannot
    // cross the boundary. Rotate both angles so the stop sits at 0, and
    // then the only route is the plain difference — no wrapping, because
    // wrapping is exactly what the stop forbids.
    //
    // In the overlap the target has two span positions; the nearer one is
    // the route the controller takes (JJ's ERC: W180 from 301 went to 180,
    // not 540).
    const SpanPositions to = spanPositions(stop, range, m.targetDeg);
    double b = to.first;
    if (to.count == 2 && std::abs(to.second - a) < std::abs(to.first - a)) {
        b = to.second;
    }

    m.targetSpanDeg = b;
    m.travelDeg = b - a;
    m.routeKnown = true;

    // A stop does not usually forbid a heading, only a route — a rotor
    // with a north stop still reaches 350° and 10°, just never directly
    // between them. So nothing here is unreachable; it is only
    // sometimes a very long way round, and the travel says which.
    if (std::abs(m.travelDeg) > kLongWayDeg) {
        m.note = QStringLiteral(
            "The rotor cannot turn through its end stop, so this is the "
            "long way round.");
    }
    return m;
}

SpanTracker::SpanTracker(EndStop stop, double rangeDeg)
{
    configure(stop, rangeDeg);
}

void SpanTracker::configure(EndStop stop, double rangeDeg)
{
    m_stop = stop;
    m_rangeDeg = clampRangeDeg(rangeDeg);
    m_haveHeading = false;
    m_headingDeg = 0.0;
    m_spanKnown = false;
    m_spanDeg = 0.0;
}

bool SpanTracker::update(double headingDeg)
{
    if (!std::isfinite(headingDeg)) { return false; }
    // The ERC reads north as 360, never 000; wrap360 makes it 0.
    const double h = wrap360(headingDeg);
    m_headingDeg = h;
    m_haveHeading = true;

    if (m_stop == EndStop::None) { return true; }

    if (m_spanKnown) {
        // Polls come at least once a second and the rotor turns under 10
        // degrees a second, so the step is the one in -180..180.
        const double step = shorterWay(h - compassForSpan(m_stop, m_spanDeg));
        m_spanDeg = std::clamp(m_spanDeg + step, 0.0, m_rangeDeg);
        return true;
    }

    const SpanPositions p = spanPositions(m_stop, m_rangeDeg, h);
    if (p.count == 1) {
        m_spanDeg = p.first;
        m_spanKnown = true;
    }
    return true;
}

void SpanTracker::markStale()
{
    m_haveHeading = false;
    m_spanKnown = false;
}

double SpanTracker::headingDeg() const
{
    return m_haveHeading ? m_headingDeg : -1.0;
}

double SpanTracker::spanPositionDeg() const
{
    if (m_stop == EndStop::None || !m_spanKnown) { return kUnknownSpanDeg; }
    return m_spanDeg;
}

Move SpanTracker::planTo(double toDeg) const
{
    if (m_stop == EndStop::None) {
        if (!m_haveHeading) {
            Move m;
            m.targetDeg = wrap360(toDeg);
            return m;
        }
        return planFree(m_headingDeg, toDeg);
    }
    return planOnSpan(spanPositionDeg(), toDeg, m_stop, m_rangeDeg);
}

} // namespace NereusSDR::RotorRoute
