// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Real DXCC entity prefixes (K, JA, VK, G, DL) used as
// fixtures to exercise the worked-status state machine. Precedent: C1, C2.
//
// NereusSDR - DxccWorkedStatus tests
//
// Phase 3J-2 Task C3. Pins the contract that DxccWorkedStatus tracks
// per-entity / per-band / per-modeGroup worked status from a vector
// of QsoRecord rows (typically produced by AdifParser). Six tests:
//   - newPrefixIsNewDxcc: query() against an unknown primary prefix
//     returns DxccStatus::NewDxcc.
//   - sameEntitySameBandSameModeIsWorked: a primary prefix already
//     worked on the same band + same modeGroup returns
//     DxccStatus::Worked.
//   - sameEntityNewBandIsNewBand: same primary prefix on a new band
//     returns DxccStatus::NewBand.
//   - sameEntityNewModeIsNewMode: same primary prefix on the same
//     band but a new modeGroup returns DxccStatus::NewMode.
//   - clearResetsAll: clear() returns the tracker to its empty
//     state (totalQsos == 0, query returns NewDxcc).
//   - totalQsoCounting: load() correctly accumulates totalQsos
//     across multiple input records and entityCount reflects the
//     unique-entity tally.
//
// Plan-spec divergence: the original task-C3 spec used
// `recordQso(QsoRecord)` + `statusFor(prefix, band, mode)` method
// names. AetherSDR's actual public API is `load(QVector<QsoRecord>)`
// + `query(prefix, band, modeGroup)` (see DxccWorkedStatus.h:28-35
// [@0cd4559]). Source-first protocol pins the upstream surface, so
// the tests below build a small QVector<QsoRecord> per case and
// drive the tracker through `load()` + `query()`. Test intent
// (one assertion per state-machine branch) preserved.

#include <QtTest>

#include "core/DxccWorkedStatus.h"
#include "core/AdifParser.h"  // for QsoRecord

using namespace NereusSDR;

class TestDxccWorkedStatus : public QObject {
    Q_OBJECT
private slots:
    void newPrefixIsNewDxcc();
    void sameEntitySameBandSameModeIsWorked();
    void sameEntityNewBandIsNewBand();
    void sameEntityNewModeIsNewMode();
    void clearResetsAll();
    void totalQsoCounting();
};

void TestDxccWorkedStatus::newPrefixIsNewDxcc()
{
    DxccWorkedStatus tracker;
    QCOMPARE(tracker.query("K", "20m", "PHONE"), DxccStatus::NewDxcc);
}

void TestDxccWorkedStatus::sameEntitySameBandSameModeIsWorked()
{
    DxccWorkedStatus tracker;
    QsoRecord q;
    q.callsign = "K1AA";
    q.dxccPrefix = "K";
    q.band = "20m";
    q.modeGroup = "PHONE";

    QVector<QsoRecord> records;
    records.append(q);
    tracker.load(records);

    QCOMPARE(tracker.query("K", "20m", "PHONE"), DxccStatus::Worked);
}

void TestDxccWorkedStatus::sameEntityNewBandIsNewBand()
{
    DxccWorkedStatus tracker;
    QsoRecord q;
    q.callsign = "K1AA";
    q.dxccPrefix = "K";
    q.band = "20m";
    q.modeGroup = "PHONE";

    QVector<QsoRecord> records;
    records.append(q);
    tracker.load(records);

    QCOMPARE(tracker.query("K", "40m", "PHONE"), DxccStatus::NewBand);
}

void TestDxccWorkedStatus::sameEntityNewModeIsNewMode()
{
    DxccWorkedStatus tracker;
    QsoRecord q;
    q.callsign = "K1AA";
    q.dxccPrefix = "K";
    q.band = "20m";
    q.modeGroup = "PHONE";

    QVector<QsoRecord> records;
    records.append(q);
    tracker.load(records);

    QCOMPARE(tracker.query("K", "20m", "CW"), DxccStatus::NewMode);
}

void TestDxccWorkedStatus::clearResetsAll()
{
    DxccWorkedStatus tracker;
    QsoRecord q;
    q.callsign = "K1AA";
    q.dxccPrefix = "K";
    q.band = "20m";
    q.modeGroup = "PHONE";

    QVector<QsoRecord> records;
    records.append(q);
    tracker.load(records);

    tracker.clear();
    QCOMPARE(tracker.totalQsos(), 0);
    QCOMPARE(tracker.entityCount(), 0);
    QCOMPARE(tracker.query("K", "20m", "PHONE"), DxccStatus::NewDxcc);
}

void TestDxccWorkedStatus::totalQsoCounting()
{
    DxccWorkedStatus tracker;
    QVector<QsoRecord> records;

    // Five QSOs all under primary prefix "K" -> entityCount == 1
    // but totalQsos == 5 (each load() row increments the counter).
    for (int i = 0; i < 5; ++i) {
        QsoRecord q;
        q.callsign = QString("K%1AA").arg(i);
        q.dxccPrefix = "K";
        q.band = "20m";
        q.modeGroup = "PHONE";
        records.append(q);
    }
    tracker.load(records);

    QCOMPARE(tracker.totalQsos(), 5);
    QCOMPARE(tracker.entityCount(), 1);
}

QTEST_GUILESS_MAIN(TestDxccWorkedStatus)
#include "tst_dxcc_worked_status.moc"
