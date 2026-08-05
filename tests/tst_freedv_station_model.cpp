// SPDX-License-Identifier: GPL-2.0-or-later
//
// no-port-check: Test references real DXCC entity callsigns as
// fixtures. Precedent: B2-B6, C1-C4, D1-D2.

#include <QtTest>
#include <QSignalSpy>

#include "models/FreeDVStationModel.h"
#include "core/FreeDVStation.h"

using namespace NereusSDR;

class TestFreeDVStationModel : public QObject {
    Q_OBJECT
private slots:
    void initialState();
    void onStationAddedEmits();
    void onStationUpdatedReplaces();
    void onStationRemovedDeletes();
    void clearEmitsCleared();
    void distanceComputedFromGrid();
    void stationBySidLookup();
    void applyDistanceHeadingStampsCardinal();
    void cardinalEmptyWhenGridSquareEmpty();
    void cardinalEmptyWhenOurGridEmpty();
};

static FreeDVStation makeStation(const QString& sid, const QString& call, const QString& grid) {
    FreeDVStation s;
    s.sid = sid;
    s.callsign = call;
    s.gridSquare = grid;
    return s;
}

void TestFreeDVStationModel::initialState() {
    FreeDVStationModel m;
    QCOMPARE(m.stationCount(), 0);
}

void TestFreeDVStationModel::onStationAddedEmits() {
    FreeDVStationModel m;
    QSignalSpy spy(&m, &FreeDVStationModel::stationAdded);
    m.onStationAdded("sid1", makeStation("sid1", "DL2RD", "JO31"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m.stationCount(), 1);
}

void TestFreeDVStationModel::onStationUpdatedReplaces() {
    FreeDVStationModel m;
    m.onStationAdded("sid1", makeStation("sid1", "DL2RD", "JO31"));
    QSignalSpy spy(&m, &FreeDVStationModel::stationUpdated);
    auto updated = makeStation("sid1", "DL2RD", "JO31");
    updated.frequencyHz = 7180000;
    m.onStationUpdated("sid1", updated);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m.stationBySid("sid1").frequencyHz, quint64(7180000));
}

void TestFreeDVStationModel::onStationRemovedDeletes() {
    FreeDVStationModel m;
    m.onStationAdded("sid1", makeStation("sid1", "DL2RD", "JO31"));
    QSignalSpy spy(&m, &FreeDVStationModel::stationRemoved);
    m.onStationRemoved("sid1");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m.stationCount(), 0);
}

void TestFreeDVStationModel::clearEmitsCleared() {
    FreeDVStationModel m;
    m.onStationAdded("sid1", makeStation("sid1", "DL2RD", "JO31"));
    m.onStationAdded("sid2", makeStation("sid2", "G4XXX", "IO91"));

    QSignalSpy spy(&m, &FreeDVStationModel::cleared);
    m.clear();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m.stationCount(), 0);
}

void TestFreeDVStationModel::distanceComputedFromGrid() {
    FreeDVStationModel m;
    m.setOurGridSquare("FN42");  // Boston-ish

    auto info = makeStation("sid1", "DL2RD", "JO31");  // Frankfurt-ish
    m.onStationAdded("sid1", info);

    auto stored = m.stationBySid("sid1");
    // Boston FN42 to Frankfurt JO31 is roughly 6000-6500 km via great circle
    QVERIFY(stored.distanceKm > 5500);
    QVERIFY(stored.distanceKm < 7000);
    QVERIFY(stored.headingDeg >= 0);
    QVERIFY(stored.headingDeg <= 360);
}

void TestFreeDVStationModel::stationBySidLookup() {
    FreeDVStationModel m;
    m.onStationAdded("sid42", makeStation("sid42", "VK3FOO", "QF22"));
    QCOMPARE(m.stationBySid("sid42").callsign, QStringLiteral("VK3FOO"));
    QCOMPARE(m.stationBySid("nonexistent").callsign, QString());
}

void TestFreeDVStationModel::applyDistanceHeadingStampsCardinal() {
    FreeDVStationModel m;

    // Our grid EM00 (centered at lat 30.5, lon -99). Station 1 sits at
    // EM01 (lat 31.5, lon -99); same longitude, latitude one degree
    // north. Bearing == 0 deg -> cardinal "N" per
    // GetCardinalDirection_(int) in
    // freedv-gui src/gui/dialogs/freedv_reporter.cpp:2676-2681 [@77e793a].
    m.setOurGridSquare("EM00");
    auto north = makeStation("sid-north", "K1NORTH", "EM01");
    m.onStationAdded("sid-north", north);
    QCOMPARE(m.stationBySid("sid-north").headingCardinal, QStringLiteral("N"));

    // Station 2 sits at FM00 (lat 30.5, lon -79); same latitude, 20 deg
    // east. Initial bearing computes to roughly 85 deg, which rounds
    // to cardinal-index 4 -> "E" per the upstream 16-direction table.
    auto east = makeStation("sid-east", "K1EAST", "FM00");
    m.onStationAdded("sid-east", east);
    QCOMPARE(m.stationBySid("sid-east").headingCardinal, QStringLiteral("E"));
}

void TestFreeDVStationModel::cardinalEmptyWhenGridSquareEmpty() {
    FreeDVStationModel m;
    m.setOurGridSquare("EM00");

    auto noGrid = makeStation("sid-nogrid", "AA0AA", QString());
    m.onStationAdded("sid-nogrid", noGrid);
    QVERIFY(m.stationBySid("sid-nogrid").headingCardinal.isEmpty());
}

void TestFreeDVStationModel::cardinalEmptyWhenOurGridEmpty() {
    FreeDVStationModel m;
    // Do not call setOurGridSquare -> our grid stays empty.

    auto s = makeStation("sid1", "AA0AA", "EM01");
    m.onStationAdded("sid1", s);
    QVERIFY(m.stationBySid("sid1").headingCardinal.isEmpty());
}

QTEST_GUILESS_MAIN(TestFreeDVStationModel)
#include "tst_freedv_station_model.moc"
