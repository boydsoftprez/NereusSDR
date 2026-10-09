// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - GreatCircle tests (rotor control, bearings).
//
// Bearing fixtures come from published worked examples, not from this
// implementation:
//   - Ed Williams, Aviation Formulary, "Course between points"
//     (https://edwilliams.org/avform147.htm): LAX 33deg57'N 118deg24'W to
//     JFK 40deg38'N 73deg47'W, initial true course 1.150035 radians =
//     66 degrees (65.89).
//   - Chris Veness, Movable Type Scripts, "Calculate distance, bearing and
//     more between Latitude/Longitude points"
//     (https://www.movable-type.co.uk/scripts/latlong.html), Bearing
//     section: 35N 45E (about Baghdad) to 35N 135E (about Osaka) starts on
//     a heading of 60 degrees.
// Maidenhead fixtures follow the locator's definition: fields of 20 x 10
// degrees from 180 W / 90 S (A to R), squares of 2 x 1 degrees (0 to 9),
// subsquares of 5 x 2.5 minutes (a to x); the position is the centre.
//   FN31   = lon -180 + 5*20 + 3*2 + 1 = -73,    lat -90 + 13*10 + 1 + 0.5 = 41.5
//   FN31pr = lon -74 + 15*5/60 + 2.5/60 = -72.7083..,
//            lat 41 + 17*2.5/60 + 1.25/60 = 41.7291..

#include <QtTest>

#include "core/GreatCircle.h"

using namespace NereusSDR;

namespace {

constexpr double kBearingToleranceDeg = 1.0;
constexpr double kPositionToleranceDeg = 1e-9;

GeoPosition dms(int latDeg, int latMin, bool north, int lonDeg, int lonMin, bool east)
{
    const double lat = (latDeg + latMin / 60.0) * (north ? 1.0 : -1.0);
    const double lon = (lonDeg + lonMin / 60.0) * (east ? 1.0 : -1.0);
    return GeoPosition{lat, lon};
}

} // namespace

class TestGreatCircle : public QObject {
    Q_OBJECT
private slots:
    void knownBearings_data();
    void knownBearings();
    void longPathIsShortPlus180();
    void maidenheadCentres_data();
    void maidenheadCentres();
    void maidenheadIsCaseInsensitive();
    void badGridHasNoPosition_data();
    void badGridHasNoPosition();
    void badGridHasNoBearing();
    void bearingFromGoodGrid();
};

void TestGreatCircle::knownBearings_data()
{
    QTest::addColumn<double>("fromLat");
    QTest::addColumn<double>("fromLon");
    QTest::addColumn<double>("toLat");
    QTest::addColumn<double>("toLon");
    QTest::addColumn<double>("expected");

    const GeoPosition lax = dms(33, 57, true, 118, 24, false);
    const GeoPosition jfk = dms(40, 38, true, 73, 47, false);
    QTest::newRow("Aviation Formulary LAX to JFK")
        << lax.latitudeDeg << lax.longitudeDeg << jfk.latitudeDeg << jfk.longitudeDeg << 66.0;
    QTest::newRow("Movable Type Baghdad to Osaka") << 35.0 << 45.0 << 35.0 << 135.0 << 60.0;
}

void TestGreatCircle::knownBearings()
{
    QFETCH(double, fromLat);
    QFETCH(double, fromLon);
    QFETCH(double, toLat);
    QFETCH(double, toLon);
    QFETCH(double, expected);

    const double got = GreatCircle::initialBearingDeg({fromLat, fromLon}, {toLat, toLon});
    QVERIFY2(std::abs(got - expected) <= kBearingToleranceDeg,
             qPrintable(QStringLiteral("got %1, expected %2").arg(got).arg(expected)));
}

void TestGreatCircle::longPathIsShortPlus180()
{
    QCOMPARE(GreatCircle::longPathBearingDeg(66.0), 246.0);
    QCOMPARE(GreatCircle::longPathBearingDeg(60.0), 240.0);
    QCOMPARE(GreatCircle::longPathBearingDeg(270.0), 90.0);
    QCOMPARE(GreatCircle::longPathBearingDeg(180.0), 0.0);
    QCOMPARE(GreatCircle::longPathBearingDeg(0.0), 180.0);
}

void TestGreatCircle::maidenheadCentres_data()
{
    QTest::addColumn<QString>("grid");
    QTest::addColumn<double>("lat");
    QTest::addColumn<double>("lon");

    QTest::newRow("FN31") << QStringLiteral("FN31") << 41.5 << -73.0;
    QTest::newRow("FN31pr") << QStringLiteral("FN31pr") << (41.0 + 17 * 2.5 / 60.0 + 1.25 / 60.0)
                            << (-74.0 + 15 * 5.0 / 60.0 + 2.5 / 60.0);
    QTest::newRow("AA00 south-west corner square") << QStringLiteral("AA00") << -89.5 << -179.0;
    QTest::newRow("RR99 north-east corner square") << QStringLiteral("RR99") << 89.5 << 179.0;
    QTest::newRow("JJ00aa at 0,0") << QStringLiteral("JJ00aa") << (1.25 / 60.0) << (2.5 / 60.0);
    QTest::newRow("RR99xx last subsquare") << QStringLiteral("RR99xx")
                                           << (90.0 - 1.25 / 60.0) << (180.0 - 2.5 / 60.0);
}

void TestGreatCircle::maidenheadCentres()
{
    QFETCH(QString, grid);
    QFETCH(double, lat);
    QFETCH(double, lon);

    const auto pos = GreatCircle::fromMaidenhead(grid);
    QVERIFY(pos.has_value());
    QVERIFY2(std::abs(pos->latitudeDeg - lat) < kPositionToleranceDeg,
             qPrintable(QStringLiteral("lat %1, expected %2").arg(pos->latitudeDeg).arg(lat)));
    QVERIFY2(std::abs(pos->longitudeDeg - lon) < kPositionToleranceDeg,
             qPrintable(QStringLiteral("lon %1, expected %2").arg(pos->longitudeDeg).arg(lon)));
}

void TestGreatCircle::maidenheadIsCaseInsensitive()
{
    const auto upper = GreatCircle::fromMaidenhead(QStringLiteral("FN31PR"));
    const auto lower = GreatCircle::fromMaidenhead(QStringLiteral("fn31pr"));
    const auto mixed = GreatCircle::fromMaidenhead(QStringLiteral(" Fn31pR "));
    QVERIFY(upper && lower && mixed);
    QCOMPARE(upper->latitudeDeg, lower->latitudeDeg);
    QCOMPARE(upper->longitudeDeg, lower->longitudeDeg);
    QCOMPARE(mixed->latitudeDeg, lower->latitudeDeg);
    QCOMPARE(mixed->longitudeDeg, lower->longitudeDeg);
}

void TestGreatCircle::badGridHasNoPosition_data()
{
    QTest::addColumn<QString>("grid");
    QTest::newRow("empty") << QString();
    QTest::newRow("blank") << QStringLiteral("   ");
    QTest::newRow("too short") << QStringLiteral("FN3");
    QTest::newRow("five characters") << QStringLiteral("FN31p");
    QTest::newRow("eight characters") << QStringLiteral("FN31pr12");
    QTest::newRow("field past R") << QStringLiteral("SN31");
    QTest::newRow("letter for square digit") << QStringLiteral("FNA1");
    QTest::newRow("digit for field") << QStringLiteral("1N31");
    QTest::newRow("subsquare past x") << QStringLiteral("FN31py");
    QTest::newRow("digit for subsquare") << QStringLiteral("FN3112");
}

void TestGreatCircle::badGridHasNoPosition()
{
    QFETCH(QString, grid);
    QVERIFY(!GreatCircle::fromMaidenhead(grid).has_value());
}

void TestGreatCircle::badGridHasNoBearing()
{
    const GeoPosition japan{36.40, 138.38};
    QVERIFY(!GreatCircle::bearingFromGrid(QString(), japan).has_value());
    QVERIFY(!GreatCircle::bearingFromGrid(QStringLiteral("XX99"), japan).has_value());
    QVERIFY(!GreatCircle::bearingFromGrid(QStringLiteral("FN3"), japan).has_value());
}

void TestGreatCircle::bearingFromGoodGrid()
{
    // Same answer as from the grid's centre directly.
    const GeoPosition osaka{35.0, 135.0};
    const auto centre = GreatCircle::fromMaidenhead(QStringLiteral("FN31pr"));
    QVERIFY(centre.has_value());
    const auto bearing = GreatCircle::bearingFromGrid(QStringLiteral("FN31pr"), osaka);
    QVERIFY(bearing.has_value());
    QCOMPARE(*bearing, GreatCircle::initialBearingDeg(*centre, osaka));
    QVERIFY(*bearing >= 0.0 && *bearing < 360.0);
}

QTEST_GUILESS_MAIN(TestGreatCircle)
#include "tst_great_circle.moc"
