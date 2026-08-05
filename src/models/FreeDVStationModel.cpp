// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - FreeDVStationModel: live station map keyed by Socket.IO sid.
// Implementation.
//
// Ported from freedv-gui src/gui/dialogs/freedv_reporter.cpp:2312-2410
// (calculateDistance_ / calculateLatLonFromGridSquare_ /
// calculateBearingInDegrees_ / DegreesToRadians_ / RadiansToDegrees_)
// [@77e793a]. Modeled (class shape) on freedv-gui
// src/gui/dialogs/freedv_reporter.h:367-417 (`ReporterData` per-station
// record) [@77e793a].
//
// License (upstream): freedv-gui carries an LGPLv2.1+ root license
// (`freedv-gui/COPYING`); the specific `freedv_reporter.h` /
// `freedv_reporter.cpp` files have no per-file Copyright header, so the
// project root header applies. LGPL is upgrade-compatible to GPLv2-or-later
// when linked into a GPL work (LGPL section 3 conversion clause), which is
// the model NereusSDR uses.
//
// Copyright (C) 2026 NereusSDR contributors.
// Distance / heading math: derived from freedv-gui source (LGPLv2.1+,
// copyright the freedv-gui contributors / FreeDV project).
//
// Modification history (NereusSDR)
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task D3. Initial create.
//                                    Translation of freedv-gui's wxString-
//                                    based grid-square parser into a
//                                    QString-based equivalent (charAt
//                                    indexing, .toUpper() for case
//                                    folding, .mid()/.size() instead of
//                                    Mid()/Length()). Optional 6-char
//                                    sub-square segment is honored when
//                                    both characters are letters
//                                    [a-zA-Z]; otherwise the 4-char
//                                    field-square center is used (lon
//                                    += 1, lat += 0.5). Haversine
//                                    radius constant 6371 km preserved
//                                    verbatim. Bearing wrap-around
//                                    (`(result == 360) ? 0 : result`)
//                                    preserved verbatim. AI tooling:
//                                    Anthropic Claude Code.

#include "FreeDVStationModel.h"

#include <QtMath>

#include <cmath>

namespace NereusSDR {

namespace {

// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:2401-2404 [@77e793a]
inline double DegreesToRadians(double degrees)
{
    return degrees * (M_PI / 180.0);
}

// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:2406-2410 [@77e793a]
inline double RadiansToDegrees(double radians)
{
    auto result = (radians > 0 ? radians : (2 * M_PI + radians)) * 360 / (2 * M_PI);
    return (result == 360) ? 0 : result;
}

// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:2336-2374 [@77e793a]
//
// Translation: wxString -> QString. `gridSquare.MakeUpper()` becomes
// `gridSquare = gridSquare.toUpper()`. `gridSquare.GetChar(N)` becomes
// `gridSquare.at(N)` (returns QChar). `gridSquare.Mid(4, 2)` becomes
// `gridSquare.mid(4, 2)`. `gridSquare.Length()` becomes `gridSquare.size()`.
// freedv-gui's optional-segment regex `[A-Za-z]{2}` is folded into a
// pair of QChar::isLetter() checks since the surrounding toUpper() has
// already normalized case.
void calculateLatLonFromGridSquare(QString gridSquare, double& lat, double& lon)
{
    const char charA = 'A';
    const char char0 = '0';

    // Uppercase grid square for easier processing
    gridSquare = gridSquare.toUpper();

    // Start from antimeridian South Pole (e.g. over the Pacific, not over the UK)
    lon = -180.0;
    lat = -90.0;

    if (gridSquare.size() < 4) {
        return;
    }

    // Process first two characters
    lon += (gridSquare.at(0).toLatin1() - charA) * 20;
    lat += (gridSquare.at(1).toLatin1() - charA) * 10;

    // Then next two
    lon += (gridSquare.at(2).toLatin1() - char0) * 2;
    lat += (gridSquare.at(3).toLatin1() - char0) * 1;

    // If grid square is 6 or more letters, THEN use the next two.
    // Otherwise, optional.
    QString optionalSegment = gridSquare.mid(4, 2);
    const bool sixCharSubSquare =
        gridSquare.size() >= 6
        && optionalSegment.size() == 2
        && optionalSegment.at(0).isLetter()
        && optionalSegment.at(1).isLetter();
    if (sixCharSubSquare) {
        lon += (gridSquare.at(4).toLatin1() - charA) * 5.0 / 60;
        lat += (gridSquare.at(5).toLatin1() - charA) * 2.5 / 60;

        // Center in middle of grid square
        lon += 5.0 / 60 / 2;
        lat += 2.5 / 60 / 2;
    } else {
        lon += 2 / 2;
        lat += 1.0 / 2;
    }
}

// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:2312-2334 [@77e793a]
double calculateDistanceKm(const QString& gridSquare1, const QString& gridSquare2)
{
    double lat1 = 0;
    double lon1 = 0;
    double lat2 = 0;
    double lon2 = 0;

    // Grab latitudes and longitudes for the two locations.
    calculateLatLonFromGridSquare(gridSquare1, lat1, lon1);
    calculateLatLonFromGridSquare(gridSquare2, lat2, lon2);

    // Use Haversine formula to calculate distance. See
    // https://stackoverflow.com/questions/27928/calculate-distance-between-two-latitude-longitude-points-haversine-formula.
    const double EARTH_RADIUS = 6371;
    double dLat = DegreesToRadians(lat2 - lat1);
    double dLon = DegreesToRadians(lon2 - lon1);
    double a =
        sin(dLat / 2) * sin(dLat / 2) +
        cos(DegreesToRadians(lat1)) * cos(DegreesToRadians(lat2)) *
        sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return EARTH_RADIUS * c;
}

// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:2676-2681 [@77e793a]
//
// Translation: wxString return becomes QString. 16-direction cardinal
// table (N / NNE / NE / ENE / E / ESE / SE / SSE / S / SSW / SW / WSW /
// W / WNW / NW / NNW) ported verbatim; the rounding formula
// (degrees / 360 * 16 + 0.5) % 16 is unchanged so an input of 360 deg
// wraps back to index 0 (N) the same way upstream wraps it.
QString getCardinalDirection(int degrees)
{
    int cardinalDirectionNumber( static_cast<int>( ( ( degrees / 360.0 ) * 16 ) + 0.5 )  % 16 );
    static const char* const cardinalDirectionTexts[] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    return QString::fromLatin1(cardinalDirectionTexts[cardinalDirectionNumber]);
}

// From freedv-gui src/gui/dialogs/freedv_reporter.cpp:2376-2399 [@77e793a]
double calculateBearingInDegrees(const QString& gridSquare1, const QString& gridSquare2)
{
    double lat1 = 0;
    double lon1 = 0;
    double lat2 = 0;
    double lon2 = 0;

    // Grab latitudes and longitudes for the two locations.
    calculateLatLonFromGridSquare(gridSquare1, lat1, lon1);
    calculateLatLonFromGridSquare(gridSquare2, lat2, lon2);

    // Convert latitudes and longitudes into radians
    lat1 = DegreesToRadians(lat1);
    lat2 = DegreesToRadians(lat2);
    lon1 = DegreesToRadians(lon1);
    lon2 = DegreesToRadians(lon2);

    double diffLongitude = lon2 - lon1;
    double x = cos(lat2) * sin(diffLongitude);
    double y = (cos(lat1) * sin(lat2)) - (sin(lat1) * cos(lat2) * cos(diffLongitude));
    double radians = atan2(x, y);

    return RadiansToDegrees(radians);
}

} // namespace

FreeDVStationModel::FreeDVStationModel(QObject* parent)
    : QObject(parent)
{
}

QHash<QString, FreeDVStation> FreeDVStationModel::stations() const
{
    return m_stations;
}

FreeDVStation FreeDVStationModel::stationBySid(const QString& sid) const
{
    return m_stations.value(sid);
}

int FreeDVStationModel::stationCount() const
{
    return m_stations.size();
}

void FreeDVStationModel::setOurGridSquare(const QString& grid)
{
    if (m_ourGrid == grid) {
        return;
    }
    m_ourGrid = grid;

    // Re-stamp every existing station with new distance/heading AND
    // emit stationUpdated so subscribers (FreeDVReporterDialog) repaint
    // the Distance / Hdg columns live.
    //
    // 2026-05-12 bench fix (PR #238 review P2): without the per-station
    // emit, an open Reporter dialog keeps showing zeroed Distance / Hdg
    // until each station receives a network update or the dialog is
    // rebuilt — Save & Propagate looks broken from the user's
    // perspective.  Reuses the existing stationUpdated path
    // (FreeDVReporterDialog.cpp:772-780) instead of introducing a new
    // bulk-refresh signal.
    for (auto it = m_stations.begin(); it != m_stations.end(); ++it) {
        applyDistanceHeading(it.value());
        emit stationUpdated(it.key(), it.value());
    }
}

void FreeDVStationModel::onStationAdded(const QString& sid, const FreeDVStation& info)
{
    FreeDVStation stamped = info;
    applyDistanceHeading(stamped);
    m_stations.insert(sid, stamped);
    emit stationAdded(sid, stamped);
}

void FreeDVStationModel::onStationUpdated(const QString& sid, const FreeDVStation& info)
{
    FreeDVStation stamped = info;
    applyDistanceHeading(stamped);
    m_stations.insert(sid, stamped);
    emit stationUpdated(sid, stamped);
}

void FreeDVStationModel::onStationRemoved(const QString& sid)
{
    if (m_stations.remove(sid) > 0) {
        emit stationRemoved(sid);
    }
}

void FreeDVStationModel::clear()
{
    m_stations.clear();
    emit cleared();
}

void FreeDVStationModel::applyDistanceHeading(FreeDVStation& info) const
{
    if (m_ourGrid.size() < 4 || info.gridSquare.size() < 4) {
        info.distanceKm = 0.0;
        info.headingDeg = 0.0;
        info.headingCardinal.clear();
        return;
    }
    info.distanceKm = calculateDistanceKm(m_ourGrid, info.gridSquare);
    info.headingDeg = calculateBearingInDegrees(m_ourGrid, info.gridSquare);
    // From freedv-gui src/gui/dialogs/freedv_reporter.cpp:3202+3388 [@77e793a]
    // (refreshAllRows + add-station paths call GetCardinalDirection_ on
    // the bearing integer to populate the per-row "Hdg" cell).
    info.headingCardinal = getCardinalDirection(static_cast<int>(info.headingDeg + 0.5));
}

} // namespace NereusSDR
