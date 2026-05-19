// =================================================================
// tests/tst_fault_log.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent; FaultLog is a
// NereusSDR-native class per design doc §4.7.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Tests: ringBufferKeepsNewestTen, likelyCauseSwrTrip,
//                 persistsAcrossInstances.
// =================================================================

#include <QtTest/QtTest>
#include "core/FaultLog.h"
#include "core/AppSettings.h"

class FaultLogTest : public QObject {
    Q_OBJECT
private slots:
    void ringBufferKeepsNewestTen();
    void likelyCauseSwrTrip();
    void persistsAcrossInstances();
};

// Capture 12 events and verify only the 10 newest are retained, newest first.
void FaultLogTest::ringBufferKeepsNewestTen()
{
    NereusSDR::FaultLog log("PGXL_FaultHistory");
    log.clear();
    for (int i = 0; i < 12; ++i) {
        NereusSDR::FaultEvent ev;
        ev.whenMs       = static_cast<qint64>(i) * 1000;
        ev.state        = "FAULT";
        ev.fwdAtFaultW  = 1500.0f;
        ev.swrAtFault   = 1.5f;
        ev.tempAtFaultC = 70.0f;
        ev.likelyCause  = "X";
        log.capture(ev);
    }
    QCOMPARE(log.events().size(), 10);
    // Events are prepended so the last captured (i=11, whenMs=11000) is first.
    QCOMPARE(log.events().first().whenMs, qint64(11000));
    // The oldest surviving event should be i=2 (whenMs=2000); i=0 and i=1 were evicted.
    QCOMPARE(log.events().last().whenMs, qint64(2000));
}

// Verify the SWR-trip heuristic fires when SWR exceeds 2.5.
void FaultLogTest::likelyCauseSwrTrip()
{
    const QString cause = NereusSDR::FaultLog::likelyCauseFor(1500.0f, 3.0f, 60.0f);
    QCOMPARE(cause, QString("SWR trip"));
}

// Store events via instance A, construct instance B with the same key, and
// verify the events round-trip through AppSettings JSON correctly.
void FaultLogTest::persistsAcrossInstances()
{
    const QString key = "PGXL_FaultHistory_PersistTest";

    // Write via instance A.
    {
        NereusSDR::FaultLog a(key);
        a.clear();
        NereusSDR::FaultEvent ev;
        ev.whenMs       = qint64(99000);
        ev.state        = "FAULT_PROTECT";
        ev.fwdAtFaultW  = 1800.0f;
        ev.swrAtFault   = 2.9f;
        ev.tempAtFaultC = 72.0f;
        ev.likelyCause  = NereusSDR::FaultLog::likelyCauseFor(
                              ev.fwdAtFaultW, ev.swrAtFault, ev.tempAtFaultC);
        a.capture(ev);
        QCOMPARE(a.events().size(), 1);
    }

    // Read via instance B with the same key -- should see the persisted event.
    {
        NereusSDR::FaultLog b(key);
        const QVector<NereusSDR::FaultEvent> evs = b.events();
        QCOMPARE(evs.size(), 1);
        const NereusSDR::FaultEvent& ev = evs.first();
        QCOMPARE(ev.whenMs,       qint64(99000));
        QCOMPARE(ev.state,        QString("FAULT_PROTECT"));
        QCOMPARE(ev.likelyCause,  QString("SWR trip"));
        QVERIFY(qFuzzyCompare(ev.fwdAtFaultW, 1800.0f));

        // Clean up the test key so repeated runs start clean.
        b.clear();
    }
}

QTEST_GUILESS_MAIN(FaultLogTest)
#include "tst_fault_log.moc"
