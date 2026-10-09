// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - GreatCircle: station and spot positions and the beam heading
// between them. See GreatCircle.h for sources.
//
// Modification history (NereusSDR):
//   2026-10-07: Initial version for rotor control. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.

#include "GreatCircle.h"

#include <cmath>
#include <numbers>

namespace NereusSDR::GreatCircle {

namespace {

// Maidenhead locator geometry: 18 x 18 fields of 20 x 10 degrees, 10 x 10
// squares of 2 x 1 degrees per field, 24 x 24 subsquares of 5 x 2.5 minutes
// per square, counted from 180 W and 90 S.
constexpr double kFieldLonDeg = 20.0;
constexpr double kFieldLatDeg = 10.0;
constexpr double kSquareLonDeg = 2.0;
constexpr double kSquareLatDeg = 1.0;
constexpr double kSubsquareLonDeg = 5.0 / 60.0;
constexpr double kSubsquareLatDeg = 2.5 / 60.0;
constexpr int kFieldCount = 18;     // A to R
constexpr int kSubsquareCount = 24; // a to x

constexpr double kDegToRad = std::numbers::pi / 180.0;
constexpr double kRadToDeg = 180.0 / std::numbers::pi;

double normalise360(double deg)
{
    double r = std::fmod(deg, 360.0);
    if (r < 0.0) {
        r += 360.0;
    }
    if (r >= 360.0) {
        r = 0.0;
    }
    return r;
}

// Index of `c` from `first`, or -1 if outside [0, count).
int letterIndex(QChar c, char first, int count)
{
    const int i = c.unicode() - first;
    if (i < 0 || i >= count) {
        return -1;
    }
    return i;
}

} // namespace

std::optional<GeoPosition> fromMaidenhead(const QString& grid)
{
    const QString g = grid.trimmed().toUpper();
    if (g.size() != 4 && g.size() != 6) {
        return std::nullopt;
    }

    const int fieldLon = letterIndex(g[0], 'A', kFieldCount);
    const int fieldLat = letterIndex(g[1], 'A', kFieldCount);
    const int squareLon = letterIndex(g[2], '0', 10);
    const int squareLat = letterIndex(g[3], '0', 10);
    if (fieldLon < 0 || fieldLat < 0 || squareLon < 0 || squareLat < 0) {
        return std::nullopt;
    }

    double lon = -180.0 + fieldLon * kFieldLonDeg + squareLon * kSquareLonDeg;
    double lat = -90.0 + fieldLat * kFieldLatDeg + squareLat * kSquareLatDeg;

    if (g.size() == 6) {
        const int subLon = letterIndex(g[4], 'A', kSubsquareCount);
        const int subLat = letterIndex(g[5], 'A', kSubsquareCount);
        if (subLon < 0 || subLat < 0) {
            return std::nullopt;
        }
        lon += subLon * kSubsquareLonDeg + kSubsquareLonDeg / 2.0;
        lat += subLat * kSubsquareLatDeg + kSubsquareLatDeg / 2.0;
    } else {
        lon += kSquareLonDeg / 2.0;
        lat += kSquareLatDeg / 2.0;
    }

    return GeoPosition{lat, lon};
}

double initialBearingDeg(const GeoPosition& from, const GeoPosition& to)
{
    // Initial course on a sphere, atan2 form of the Aviation Formulary's
    // "Course between points" (https://edwilliams.org/avform147.htm), with
    // longitude + east.
    const double lat1 = from.latitudeDeg * kDegToRad;
    const double lat2 = to.latitudeDeg * kDegToRad;
    const double dLon = (to.longitudeDeg - from.longitudeDeg) * kDegToRad;

    const double y = std::sin(dLon) * std::cos(lat2);
    const double x = std::cos(lat1) * std::sin(lat2)
                   - std::sin(lat1) * std::cos(lat2) * std::cos(dLon);
    if (y == 0.0 && x == 0.0) {
        return 0.0;
    }
    return normalise360(std::atan2(y, x) * kRadToDeg);
}

double longPathBearingDeg(double shortPathBearingDeg)
{
    return normalise360(shortPathBearingDeg + 180.0);
}

std::optional<double> bearingFromGrid(const QString& stationGrid, const GeoPosition& to)
{
    const std::optional<GeoPosition> station = fromMaidenhead(stationGrid);
    if (!station) {
        return std::nullopt;
    }
    return initialBearingDeg(*station, to);
}

} // namespace NereusSDR::GreatCircle
