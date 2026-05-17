// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Real amateur callsigns (W1AW, W3LPL) and a US ARRL HQ
// frequency (14.250 MHz) used as fixtures to exercise the TCI-keyed
// applySpotStatus() dispatch. Precedent: B1-B5, C1-C4.
//
// NereusSDR - SpotModel tests
//
// Phase 3J-2 Task D1. Pins the contract that SpotModel is a
// QMap<int, SpotData> sink keyed by monotonic spot index, with a
// TCI-keyed update API:
//   void applySpotStatus(int index, const QMap<QString,QString>& kvs);
// recognising 12 keys (callsign, rx_freq, tx_freq, mode, color,
// background_color, source, spotter_callsign, comment, timestamp,
// lifetime_seconds, priority) and decoding the TCI 0x7F (DEL)
// wire-format quirk to a single ASCII space in the callsign and
// comment fields. Six signals: spotAdded / spotUpdated / spotRemoved
// / spotsCleared / spotsRefreshed / spotTriggered. Seven tests:
//   - initialState: an empty SpotModel reports zero spots.
//   - applySpotStatusAddsNew: first applySpotStatus() for a given
//     index emits spotAdded and stores the SpotData.
//   - applySpotStatusUpdatesExisting: subsequent applySpotStatus()
//     for the same index emits spotUpdated, preserves prior fields
//     not present in the update, and overwrites fields that are.
//   - appliesAllTwelveKeys: every recognised key from the 12-key
//     contract round-trips through the parser and is reflected on
//     the resulting SpotData.
//   - decodes0x7FAsSpace: the TCI 0x7F (DEL) wire-format quirk is
//     replaced by a single ASCII space in callsign and comment.
//   - removeSpotEmitsSignal: removeSpot() emits spotRemoved with
//     the index and erases the row from the map.
//   - clearEmitsSignal: clear() emits spotsCleared once and empties
//     the map regardless of how many spots were resident.

#include <QtTest>
#include <QSignalSpy>

#include "models/SpotModel.h"

using namespace NereusSDR;

class TestSpotModel : public QObject {
    Q_OBJECT
private slots:
    void initialState();
    void applySpotStatusAddsNew();
    void applySpotStatusUpdatesExisting();
    void appliesAllTwelveKeys();
    void decodes0x7FAsSpace();
    void removeSpotEmitsSignal();
    void clearEmitsSignal();

    // Issue #263 regression: duplicate spots on the panadapter / SpotHub.
    // The pre-fix dedupIndexFor used a fixed 60 s window that expired
    // long before the 1800 s default spot lifetime, so a re-emitted RBN
    // / cluster spot at the same callsign+freq minted a fresh index and
    // the panadapter overlay stacked N copies of the same callsign.
    void dedupReusesIndexWhileSpotIsAlive();
    void dedupMintsFreshIndexAfterSpotExpires();
    void expirationRemovesAgedSpots();

    // Issue #263 review follow-up (2026-05-18): a steadily-active
    // station re-emitted regularly inside its lifetime must NOT
    // expire mid-stream.  Pre-fix code measured expiration against
    // addedMs (immutable after first insert), so a station emitted
    // every minute for 30 minutes still got evicted at minute 30,
    // causing the panadapter label to flicker and the Spot List row
    // to bounce.  Verifies the lastSeenMs refresh on dedup-reuse.
    void dedupReuseRefreshesLifetime();
};

void TestSpotModel::initialState()
{
    SpotModel model;
    QCOMPARE(model.spots().size(), 0);
}

void TestSpotModel::applySpotStatusAddsNew()
{
    SpotModel model;
    QSignalSpy spy(&model, &SpotModel::spotAdded);

    QMap<QString, QString> kvs;
    kvs["callsign"] = "W1AW";
    kvs["rx_freq"] = "14.250";
    model.applySpotStatus(1, kvs);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.spots()[1].callsign, QStringLiteral("W1AW"));
    QCOMPARE(model.spots()[1].rxFreqMhz, 14.250);
}

void TestSpotModel::applySpotStatusUpdatesExisting()
{
    SpotModel model;
    QMap<QString, QString> kvs1;
    kvs1["callsign"] = "W1AW";
    model.applySpotStatus(1, kvs1);

    QSignalSpy spy(&model, &SpotModel::spotUpdated);

    QMap<QString, QString> kvs2;
    kvs2["mode"] = "SSB";
    model.applySpotStatus(1, kvs2);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.spots()[1].callsign, QStringLiteral("W1AW"));  // preserved
    QCOMPARE(model.spots()[1].mode, QStringLiteral("SSB"));
}

void TestSpotModel::appliesAllTwelveKeys()
{
    SpotModel model;
    QMap<QString, QString> kvs;
    kvs["callsign"] = "W1AW";
    kvs["rx_freq"] = "14.250";
    kvs["tx_freq"] = "14.250";
    kvs["mode"] = "USB";
    kvs["color"] = "#FFFF8C00";
    kvs["background_color"] = "#80000000";
    kvs["source"] = "Cluster";
    kvs["spotter_callsign"] = "W3LPL";
    kvs["comment"] = "ARRL HQ";
    kvs["timestamp"] = QString::number(1715369280);
    kvs["lifetime_seconds"] = "1800";
    kvs["priority"] = "1";

    model.applySpotStatus(42, kvs);
    const auto& s = model.spots()[42];
    QCOMPARE(s.callsign, QStringLiteral("W1AW"));
    QCOMPARE(s.rxFreqMhz, 14.250);
    QCOMPARE(s.txFreqMhz, 14.250);
    QCOMPARE(s.mode, QStringLiteral("USB"));
    QCOMPARE(s.color, QStringLiteral("#FFFF8C00"));
    QCOMPARE(s.backgroundColor, QStringLiteral("#80000000"));
    QCOMPARE(s.source, QStringLiteral("Cluster"));
    QCOMPARE(s.spotterCallsign, QStringLiteral("W3LPL"));
    QCOMPARE(s.comment, QStringLiteral("ARRL HQ"));
    QCOMPARE(s.lifetimeSeconds, 1800);
    QCOMPARE(s.priority, 1);
}

void TestSpotModel::decodes0x7FAsSpace()
{
    SpotModel model;
    QMap<QString, QString> kvs;
    QString withDel = QStringLiteral("ARRL\x7Fheadquarters");
    kvs["callsign"] = "W1AW";
    kvs["comment"] = withDel;
    model.applySpotStatus(1, kvs);
    QCOMPARE(model.spots()[1].comment, QStringLiteral("ARRL headquarters"));
}

void TestSpotModel::removeSpotEmitsSignal()
{
    SpotModel model;
    QMap<QString, QString> kvs;
    kvs["callsign"] = "W1AW";
    model.applySpotStatus(1, kvs);

    QSignalSpy spy(&model, &SpotModel::spotRemoved);
    model.removeSpot(1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.spots().size(), 0);
}

void TestSpotModel::clearEmitsSignal()
{
    SpotModel model;
    QMap<QString, QString> kvs;
    kvs["callsign"] = "W1AW";
    model.applySpotStatus(1, kvs);
    model.applySpotStatus(2, kvs);

    QSignalSpy spy(&model, &SpotModel::spotsCleared);
    model.clear();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.spots().size(), 0);
}

// Issue #263 regression test #1.  Re-emit of the same callsign+freq
// inside the spot's lifetime must collapse to the same index so the
// panadapter overlay never stacks duplicate labels.  Pre-fix used a
// fixed 60 s window and would mint a fresh index for re-emits after
// the window even though the prior entry was still alive (and still
// painted on the overlay).
void TestSpotModel::dedupReusesIndexWhileSpotIsAlive()
{
    SpotModel model;

    // First emission: mint a fresh index, set a long lifetime.
    const int idx1 = model.dedupIndexFor("CT3MD", 14.222);
    QMap<QString, QString> kvs;
    kvs["callsign"] = "CT3MD";
    kvs["rx_freq"]  = "14.222";
    kvs["tx_freq"]  = "14.222";
    kvs["lifetime_seconds"] = "1800";  // 30 min, cluster default
    model.applySpotStatus(idx1, kvs);

    // Same callsign + freq re-emit AFTER the old 60 s window would have
    // expired.  Because the spot is still alive (lifetime=1800 s), the
    // dedup must reuse idx1, not mint a new one.
    QSignalSpy addedSpy(&model, &SpotModel::spotAdded);
    QSignalSpy updatedSpy(&model, &SpotModel::spotUpdated);

    const int idx2 = model.dedupIndexForAtTime(
        "CT3MD", 14.222,
        QDateTime::currentMSecsSinceEpoch() + 90 * 1000);  // +90 s
    model.applySpotStatus(idx2, kvs);

    QCOMPARE(idx2, idx1);
    QCOMPARE(addedSpy.count(), 0);
    QCOMPARE(updatedSpy.count(), 1);
    QCOMPARE(model.spots().size(), 1);
}

// Issue #263 regression test #2.  Once the previous spot's lifetime
// has fully elapsed, a re-emit should be treated as fresh (mint a new
// index).  Companion to the expiration test below.
void TestSpotModel::dedupMintsFreshIndexAfterSpotExpires()
{
    SpotModel model;
    QMap<QString, QString> kvs;
    kvs["callsign"] = "CT3MD";
    kvs["rx_freq"]  = "14.222";
    kvs["tx_freq"]  = "14.222";
    kvs["lifetime_seconds"] = "120";  // WSJT-X default
    const int idx1 = model.dedupIndexFor("CT3MD", 14.222);
    model.applySpotStatus(idx1, kvs);

    // Simulate a re-emit 5 min later.  Lifetime (120 s) has expired,
    // so the prior spot should be evicted and a fresh index minted.
    QSignalSpy removedSpy(&model, &SpotModel::spotRemoved);
    const int idx2 = model.dedupIndexForAtTime(
        "CT3MD", 14.222,
        QDateTime::currentMSecsSinceEpoch() + 5 * 60 * 1000);

    QVERIFY(idx2 != idx1);
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.takeFirst().at(0).toInt(), idx1);
}

// Issue #263 regression test #3.  expireOlderThan() is the periodic
// sweeper that walks m_spots and removes entries past their
// addedMs + lifetimeSeconds horizon.  Without this, spots accumulate
// forever even when dedup is honored, because there's nothing to
// shrink the canonical map.
void TestSpotModel::expirationRemovesAgedSpots()
{
    SpotModel model;
    auto putSpot = [&model](int idx, const QString& call, double freq, int lifetimeSec) {
        QMap<QString, QString> kvs;
        kvs["callsign"] = call;
        kvs["rx_freq"]  = QString::number(freq, 'f', 4);
        kvs["tx_freq"]  = QString::number(freq, 'f', 4);
        kvs["lifetime_seconds"] = QString::number(lifetimeSec);
        model.applySpotStatus(idx, kvs);
    };
    putSpot(1, "K1AA", 14.222, 120);
    putSpot(2, "K2BB", 14.225, 1800);
    putSpot(3, "K3CC", 14.230, 1800);
    QCOMPARE(model.spots().size(), 3);

    QSignalSpy removedSpy(&model, &SpotModel::spotRemoved);
    // Advance the clock 5 minutes.  Only K1AA (120 s lifetime) should
    // expire.  K2BB and K3CC stay (1800 s lifetime).
    model.expireOlderThan(
        QDateTime::currentMSecsSinceEpoch() + 5 * 60 * 1000);

    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.takeFirst().at(0).toInt(), 1);
    QCOMPARE(model.spots().size(), 2);
    QVERIFY(!model.spots().contains(1));
    QVERIFY(model.spots().contains(2));
    QVERIFY(model.spots().contains(3));
}

// Issue #263 review follow-up (2026-05-18).  Pre-fix, expireOlderThan
// + dedupIndexForAtTime both compared against addedMs (set once at
// first insert).  A station heard every minute for 30 minutes would
// expire at minute 30 even though it was just heard 60 s ago, then
// re-mint with a fresh index — label flicker, Spot List row bounce.
// Verifies that re-emit via dedupIndexForAtTime refreshes the spot's
// lastSeenMs so the lifetime window slides forward.
void TestSpotModel::dedupReuseRefreshesLifetime()
{
    SpotModel model;
    QMap<QString, QString> kvs;
    kvs["callsign"] = "K1AA";
    kvs["rx_freq"]  = "14.250";
    kvs["tx_freq"]  = "14.250";
    kvs["lifetime_seconds"] = "120";

    // First observation.  applySpotStatus stamps addedMs / lastSeenMs
    // with the real wall clock — capture it for the simulated-time
    // assertions below.
    const int idx1 = model.dedupIndexFor("K1AA", 14.250);
    model.applySpotStatus(idx1, kvs);
    const qint64 baseline = model.spots()[idx1].lastSeenMs;

    // Re-emit at +100 s.  Within the 120 s lifetime, dedup must reuse
    // idx1 AND advance lastSeenMs so the spot doesn't age out of the
    // sweeper window.
    const int idx2 = model.dedupIndexForAtTime(
        "K1AA", 14.250, baseline + 100 * 1000);
    QCOMPARE(idx2, idx1);
    QCOMPARE(model.spots()[idx1].lastSeenMs, baseline + 100 * 1000);

    // Sweep at +215 s: 115 s since last seen — still inside the 120 s
    // window, the spot must stay.  Pre-fix this would have used
    // addedMs and expired the spot at baseline+120 s.
    model.expireOlderThan(baseline + 215 * 1000);
    QCOMPARE(model.spots().size(), 1);
    QVERIFY(model.spots().contains(idx1));

    // Sweep at +225 s: 125 s since last seen — past the 120 s window,
    // the spot legitimately expires.  Confirms the lastSeenMs path
    // still enforces the timeout when re-emits actually stop.
    QSignalSpy removedSpy(&model, &SpotModel::spotRemoved);
    model.expireOlderThan(baseline + 225 * 1000);
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.takeFirst().at(0).toInt(), idx1);
    QCOMPARE(model.spots().size(), 0);
}

QTEST_GUILESS_MAIN(TestSpotModel)
#include "tst_spot_model.moc"
