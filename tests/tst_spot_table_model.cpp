// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Test references real DXCC entity callsigns as
// fixtures. Precedent: B2-B6, C1-C4, D1.

#include <QtTest>

#include "models/SpotTableModel.h"
#include "models/BandFilterProxy.h"
#include "core/DxSpot.h"

using namespace NereusSDR;

class TestSpotTableModel : public QObject {
    Q_OBJECT
private slots:
    void initialState();
    void addSpotIncrementsRowCount();
    void columnCountIsEight();
    void dataRoundTrip();
    void freqAtRowMatchesSource();
    void setMaxSpotsBounds();
    void bandFilterHidesMatchingBand();
    void bandFilterShowsAfterUnhide();
};

static DxSpot makeSpot(const QString& call, double mhz, const QString& band = {}) {
    Q_UNUSED(band);
    DxSpot s;
    s.dxCall = call;
    s.freqMhz = mhz;
    s.spotterCall = "TEST";
    s.comment = "fixture";
    s.utcTime = QTime(12, 0);
    s.source = "Cluster";
    return s;
}

void TestSpotTableModel::initialState() {
    SpotTableModel m;
    QCOMPARE(m.rowCount(), 0);
    QCOMPARE(m.columnCount(), int(SpotTableModel::ColCount));
}

void TestSpotTableModel::addSpotIncrementsRowCount() {
    SpotTableModel m;
    m.addSpot(makeSpot("W1AW", 14.250));
    QCOMPARE(m.rowCount(), 1);
}

void TestSpotTableModel::columnCountIsEight() {
    SpotTableModel m;
    QCOMPARE(m.columnCount(), 8);
}

void TestSpotTableModel::dataRoundTrip() {
    SpotTableModel m;
    m.addSpot(makeSpot("W1AW", 14.250));
    QCOMPARE(m.data(m.index(0, SpotTableModel::ColDxCall), Qt::DisplayRole).toString(),
             QStringLiteral("W1AW"));
}

void TestSpotTableModel::freqAtRowMatchesSource() {
    SpotTableModel m;
    m.addSpot(makeSpot("W1AW", 14.250));
    QCOMPARE(m.freqAtRow(0), 14.250);
}

void TestSpotTableModel::setMaxSpotsBounds() {
    SpotTableModel m;
    m.setMaxSpots(3);
    for (int i = 0; i < 10; ++i)
        m.addSpot(makeSpot(QString("CALL%1").arg(i), 14.0 + i*0.001));
    QCOMPARE(m.rowCount(), 3);
}

void TestSpotTableModel::bandFilterHidesMatchingBand() {
    auto* src = new SpotTableModel;
    BandFilterProxy proxy;
    proxy.setSourceModel(src);

    src->addSpot(makeSpot("W1AW", 14.250));  // 20m
    src->addSpot(makeSpot("VK6APH", 7.150)); // 40m
    QCOMPARE(proxy.rowCount(), 2);

    proxy.setBandVisible("20m", false);
    QCOMPARE(proxy.rowCount(), 1);
}

void TestSpotTableModel::bandFilterShowsAfterUnhide() {
    auto* src = new SpotTableModel;
    BandFilterProxy proxy;
    proxy.setSourceModel(src);

    src->addSpot(makeSpot("W1AW", 14.250));
    proxy.setBandVisible("20m", false);
    QCOMPARE(proxy.rowCount(), 0);
    proxy.setBandVisible("20m", true);
    QCOMPARE(proxy.rowCount(), 1);
}

QTEST_GUILESS_MAIN(TestSpotTableModel)
#include "tst_spot_table_model.moc"
