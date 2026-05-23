// =================================================================
// tests/tst_tune_memory_store.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. No AetherSDR equivalent; TuneMemoryStore is a
// NereusSDR-native class per design doc §4.8.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Tests: recallReturnsStoredValue, clearRemovesEntry,
//                 persistsAcrossInstances.
// =================================================================

#include <QtTest/QtTest>
#include "core/TuneMemoryStore.h"
#include "models/Band.h"

class TuneMemoryStoreTest : public QObject {
    Q_OBJECT
private slots:
    void recallReturnsStoredValue();
    void clearRemovesEntry();
    void persistsAcrossInstances();
};

// Store a memory for (antenna=1, Band20m) and recall it; verify relay values.
void TuneMemoryStoreTest::recallReturnsStoredValue()
{
    NereusSDR::TuneMemoryStore store;
    // Clear just this slot to avoid stale data from other runs.
    store.clear(1, NereusSDR::Band::Band20m);

    NereusSDR::TuneMemory mem;
    mem.antenna   = 1;
    mem.band      = NereusSDR::Band::Band20m;
    mem.c1        = 42;
    mem.l         = 199;
    mem.c2        = 88;
    mem.savedAtMs = qint64(12345000);
    store.store(mem);

    auto rec = store.recall(1, NereusSDR::Band::Band20m);
    QVERIFY(rec.has_value());
    QCOMPARE(rec->c1, 42);
    QCOMPARE(rec->l,  199);
    QCOMPARE(rec->c2, 88);
    QCOMPARE(rec->antenna, 1);
    QCOMPARE(rec->band,    NereusSDR::Band::Band20m);
}

// After clearing a slot, recall must return nullopt.
void TuneMemoryStoreTest::clearRemovesEntry()
{
    NereusSDR::TuneMemoryStore store;

    NereusSDR::TuneMemory mem;
    mem.antenna   = 2;
    mem.band      = NereusSDR::Band::Band40m;
    mem.c1        = 10;
    mem.l         = 20;
    mem.c2        = 30;
    mem.savedAtMs = qint64(55000);
    store.store(mem);

    // Verify it's there.
    QVERIFY(store.recall(2, NereusSDR::Band::Band40m).has_value());

    // Clear and verify it's gone.
    store.clear(2, NereusSDR::Band::Band40m);
    QVERIFY(!store.recall(2, NereusSDR::Band::Band40m).has_value());
}

// Store via instance A, construct instance B, verify the value survived.
void TuneMemoryStoreTest::persistsAcrossInstances()
{
    const int ant  = 3;
    const NereusSDR::Band band = NereusSDR::Band::Band80m;

    // Write via instance A.
    {
        NereusSDR::TuneMemoryStore a;
        a.clear(ant, band);

        NereusSDR::TuneMemory mem;
        mem.antenna   = ant;
        mem.band      = band;
        mem.c1        = 77;
        mem.l         = 128;
        mem.c2        = 55;
        mem.savedAtMs = qint64(777000);
        a.store(mem);
    }

    // Read via instance B.
    {
        NereusSDR::TuneMemoryStore b;
        auto rec = b.recall(ant, band);
        QVERIFY(rec.has_value());
        QCOMPARE(rec->c1,        77);
        QCOMPARE(rec->l,         128);
        QCOMPARE(rec->c2,        55);
        QCOMPARE(rec->savedAtMs, qint64(777000));

        // Clean up.
        b.clear(ant, band);
    }
}

QTEST_GUILESS_MAIN(TuneMemoryStoreTest)
#include "tst_tune_memory_store.moc"
