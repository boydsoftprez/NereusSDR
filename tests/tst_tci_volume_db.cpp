// no-port-check: NereusSDR-original TDD test for the volume math from
// Thetis TCIServer.cs:4110-4132 [v2.10.3.13].
//
// tests/tst_tci_volume_db.cpp  (NereusSDR)
// NereusSDR-original — unit tests for TciVolume.h pure math helpers.
//
// Note: this is a test-impl bundle. Both TciVolume.h and this test file
// land in the same commit. The functions are simple enough that a TDD
// red-stage is not needed; the test-then-impl bundle is noted in the
// commit message per Phase 10 plan.
//
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 10 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.

#include <QtTest>
#include "core/TciVolume.h"

using namespace NereusSDR;

class TestTciVolumeDb : public QObject {
    Q_OBJECT
private slots:
    void linear_to_db_endpoints();
    void linear_to_db_midpoint();
    void linear_to_db_saturates();
    void db_to_linear_endpoints();
    void db_to_linear_midpoint();
    void db_to_linear_saturates();
    void roundtrip_within_one_step();
};

void TestTciVolumeDb::linear_to_db_endpoints()
{
    QCOMPARE(tciLinearToDbVolume(0), -60.0);
    QCOMPARE(tciLinearToDbVolume(100), 0.0);
}

void TestTciVolumeDb::linear_to_db_midpoint()
{
    QCOMPARE(tciLinearToDbVolume(50), -30.0);
}

void TestTciVolumeDb::linear_to_db_saturates()
{
    QCOMPARE(tciLinearToDbVolume(-10), -60.0);
    QCOMPARE(tciLinearToDbVolume(150), 0.0);
}

void TestTciVolumeDb::db_to_linear_endpoints()
{
    QCOMPARE(tciDbToLinearVolume(-60.0), 0);
    QCOMPARE(tciDbToLinearVolume(0.0), 100);
}

void TestTciVolumeDb::db_to_linear_midpoint()
{
    QCOMPARE(tciDbToLinearVolume(-30.0), 50);
}

void TestTciVolumeDb::db_to_linear_saturates()
{
    QCOMPARE(tciDbToLinearVolume(-100.0), 0);
    QCOMPARE(tciDbToLinearVolume(10.0), 100);
}

void TestTciVolumeDb::roundtrip_within_one_step()
{
    // db -> linear -> db should round-trip within one linear step (= 0.6 dB).
    for (int v = 0; v <= 100; v += 10) {
        const double db = tciLinearToDbVolume(v);
        const int back = tciDbToLinearVolume(db);
        QVERIFY2(qAbs(back - v) <= 1,
                 qPrintable(QStringLiteral("v=%1 db=%2 back=%3").arg(v).arg(db).arg(back)));
    }
}

QTEST_GUILESS_MAIN(TestTciVolumeDb)
#include "tst_tci_volume_db.moc"
