// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - GreatCircle: station and spot positions and the beam heading
// between them.
//
// NereusSDR-original. The initial-course formula is the published
// great-circle course formula (Ed Williams, Aviation Formulary,
// https://edwilliams.org/avform147.htm, "Course between points"); the
// Maidenhead decoding follows the locator's definition (field 20 x 10
// degrees, square 2 x 1 degrees, subsquare 5 x 2.5 minutes). No upstream
// code is reproduced.
//
// Modification history (NereusSDR):
//   2026-10-07: Initial version for rotor control: Maidenhead to position,
//               initial great-circle bearing, long path. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.

#pragma once

#include <QString>

#include <optional>

namespace NereusSDR {

// A point on the Earth in degrees: latitude + north, longitude + east.
struct GeoPosition {
    double latitudeDeg{0.0};
    double longitudeDeg{0.0};
};

namespace GreatCircle {

// Position at the centre of a Maidenhead square (4 characters, e.g. "FN31")
// or subsquare (6 characters, e.g. "FN31pr"). Case-insensitive; surrounding
// whitespace is ignored. Anything else (empty, odd length, letters or digits
// out of range) gives std::nullopt, never (0, 0).
std::optional<GeoPosition> fromMaidenhead(const QString& grid);

// Initial great-circle bearing from `from` to `to`, degrees true in
// [0, 360). Coincident points give 0.
double initialBearingDeg(const GeoPosition& from, const GeoPosition& to);

// Long-path bearing for a short-path bearing: short + 180, in [0, 360).
double longPathBearingDeg(double shortPathBearingDeg);

// Short-path bearing from the station's grid square to `to`, or
// std::nullopt ("no bearing") when the grid is empty or invalid.
std::optional<double> bearingFromGrid(const QString& stationGrid, const GeoPosition& to);

} // namespace GreatCircle
} // namespace NereusSDR
