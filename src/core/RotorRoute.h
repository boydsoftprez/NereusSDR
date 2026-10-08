// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/RotorRoute.h  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/core/BeamHeading.h [@551576e], original header from Longpath
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
//               Claude Code. Rotor control plan, Task 3a. Takes Stop (as
//               EndStop), Move and plan() from BeamHeading; greatCircle()
//               and longPath() are not taken (src/core/GreatCircle.h has
//               ours) and advice() is not taken (the windows word the
//               route). Extended to rotors with overlap: a span measured
//               clockwise from the counter-clockwise stop, up to 450
//               degrees, where a heading in the overlap has two span
//               positions and the nearer is taken (what JJ's ERC did,
//               tests/data/rotor/erc-gs232b-range-2026-10-07.log); and
//               SpanTracker, which follows the controller's modulo-360
//               replies by continuity. Neither Longpath nor Hamlib tracks
//               which end of the overlap a rotor is at.
// =================================================================
//
// --- From BeamHeading.h ---
//
// =================================================================
// src/core/BeamHeading.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Which way to point, and how far the rotor has to travel to get there.
//
// ── Short path and long path ─────────────────────────────────────────
//
// Every bearing in the logbook is the short path — the great circle the
// signal takes if nothing is in the way. On the low bands, at grey line,
// or across a pole in winter, the long path is often the stronger
// signal, and it is the short path plus 180°.
//
// Adding 180 in your head is not hard. Doing it at three in the morning
// while a rare station is calling, and getting 275 instead of 95, is.
// This exists so nobody has to.
//
// ── End stops, which are the part people get wrong ───────────────────
//
// A rotor is not a compass. Most have a mechanical stop somewhere —
// commonly at north or at south — and cannot pass through it. Asking a
// north-stop rotor to go from 350° to 10° is a twenty-degree move if it
// can wrap and a three-hundred-and-forty-degree move if it cannot.
//
// Software that ignores this sends the antenna the long way round while
// the operator watches, and on a windy day with a big beam that is not
// merely slow. So the travel is computed against the stop, and the
// answer says how far it will actually turn.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-09-26 — greatCircle(): one distance/bearing from coordinates
//                 for the logbook card and the map. AI-assisted via
//                 Anthropic Claude, operator Martin Fischer.
// =================================================================

#pragma once

#include <QString>

namespace NereusSDR::RotorRoute {

// From Longpath src/core/BeamHeading.h:68-73 [@551576e]
// Where a rotor cannot turn through.
//
// The numbers are the wire's `endStop` enum (remote rotor control v1).
enum class EndStop {
    None  = 0,   // continuous rotation, 0 and 360 are the same place
    North = 1,   // cannot pass 0° — the common case
    South = 2,   // cannot pass 180°
};

// A rotor without overlap: one turn from stop to stop.
constexpr double kFullTurnDeg = 360.0;

// A rotor with overlap. JJ's ERC on a Yaesu: counter-clockwise stop at
// south (reads 183), clockwise stop 446 degrees later (reads 269), sold as
// 450 (tests/data/rotor/erc-gs232b-range-2026-10-07.log). The contract's
// `rangeDeg` is 360 or 450; anything else is held to that span.
constexpr double kOverlapRangeDeg = 450.0;

// `spanPositionDeg` when the Core cannot tell where on its span the rotor
// is, and with no end stop (remote rotor control v1).
constexpr double kUnknownSpanDeg = -1.0;

// From Longpath src/core/BeamHeading.cpp:83 [@551576e]
// Travel beyond this is "the long way round".
constexpr double kLongWayDeg = 270.0;

// From Longpath src/core/BeamHeading.h:47-48 [@551576e]
// Normalise any angle to 0..360.
double wrap360(double deg);

// The calibration offset is added to the heading the controller reports,
// before anything else sees it, and removed from a target before it is
// sent. Both give 0 to under 360; the controller's 360 (north) reads as 0.
double applyOffset(double reportedDeg, double offsetDeg);
double removeOffset(double targetDeg, double offsetDeg);

// The rotor's span runs clockwise from its counter-clockwise stop: span 0
// is the stop (north 0, south 180), and compass = (stop + span) mod 360.
// With EndStop::None there is no span and these return the compass alone.
double stopCompassDeg(EndStop stop);
double compassForSpan(EndStop stop, double spanDeg);

// `rangeDeg` held to 360 to 450 (not-a-number gives 360).
double clampRangeDeg(double rangeDeg);

// The span positions a compass heading has on a rotor: s = (h - stop)
// mod 360, and s + 360 too when that is still within the range. A heading
// in the overlap band has two; any other has one. `count` is 0 with
// EndStop::None or a heading that is not a number.
struct SpanPositions {
    int    count{0};
    double first{kUnknownSpanDeg};    // the smaller
    double second{kUnknownSpanDeg};   // the larger, when count is 2
};
SpanPositions spanPositions(EndStop stop, double rangeDeg, double compassDeg);

// From Longpath src/core/BeamHeading.h:75-83 [@551576e]
struct Move {
    // False while the rotor's span position is unknown (inside the
    // overlap band since connecting, or no reply yet); travelDeg is then 0
    // and a window draws no route. Longpath's `reachable`, which its plan()
    // never set false.
    bool    routeKnown{false};
    double  targetDeg{0.0};
    // Where on the span the target is, the nearer of two in the overlap;
    // kUnknownSpanDeg with no end stop or no route.
    double  targetSpanDeg{kUnknownSpanDeg};
    // Degrees the rotor will actually turn. Signed: negative is
    // counter-clockwise. This is the number that says whether a move is
    // twenty degrees or three hundred and forty.
    double  travelDeg{0.0};
    QString note;      // why it is unreachable, or what is unusual
};

// From Longpath src/core/BeamHeading.h:85-91 [@551576e]
// Plan a move from `fromDeg` to `toDeg` for a rotor with `stop`.
//
// With Stop::None the shorter of the two directions wins. With a stop,
// the rotor is confined to one continuous span and there is only one
// route — which may be the long way round, and the returned travel says
// so rather than hiding it.
//
// planFree is the Stop::None case: `fromDeg` is the compass heading.
// planOnSpan is the stop case: `fromSpanDeg` is the rotor's span position
// (negative when unknown, which gives routeKnown false), `toDeg` a compass
// heading; in the overlap the nearer of the target's two span positions
// is taken, as the controller does.
Move planFree(double fromDeg, double toDeg);
Move planOnSpan(double fromSpanDeg, double toDeg, EndStop stop, double rangeDeg);

// Follows where on its span the rotor is from the controller's replies,
// which are compass headings modulo 360 (JJ's ERC: past north it reads 001
// again, at the clockwise stop 269, never 629).
//
// Replies come at least once a second and the rotor turns under 10 degrees
// a second, so each step between replies is taken as the one between -180
// and 180. The first reply, and the first after markStale(), places the
// rotor when its heading has one span position; a heading in the overlap
// band leaves it unknown until a reply outside the band. The span position
// is held to 0 to the range: a controller that reads a few degrees past
// its stop stays at the stop.
class SpanTracker {
public:
    SpanTracker() = default;
    SpanTracker(EndStop stop, double rangeDeg);

    // New end stop or range; forgets the position.
    void configure(EndStop stop, double rangeDeg);

    // One reply, offset already applied (applyOffset). False, and nothing
    // changes, for a heading that is not a number.
    bool update(double headingDeg);

    // No fresh reply (a stale spell or a reconnect): the next reply
    // places the rotor again.
    void markStale();

    EndStop endStop() const { return m_stop; }
    double  rangeDeg() const { return m_rangeDeg; }

    // The last heading, 0 to under 360, or -1 before the first reply.
    double headingDeg() const;

    // kUnknownSpanDeg with no end stop, before the first reply, or inside
    // the overlap band until the rotor has been placed.
    double spanPositionDeg() const;
    bool   spanKnown() const { return m_spanKnown; }

    // The route to a compass heading from where the rotor is now.
    Move planTo(double toDeg) const;

private:
    EndStop m_stop{EndStop::North};
    double  m_rangeDeg{kFullTurnDeg};
    bool    m_haveHeading{false};
    double  m_headingDeg{0.0};
    bool    m_spanKnown{false};
    double  m_spanDeg{0.0};
};

} // namespace NereusSDR::RotorRoute
