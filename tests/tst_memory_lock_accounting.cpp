// =================================================================
// tests/tst_memory_lock_accounting.cpp  (NereusSDR-native)
// =================================================================
// 2026-07-27  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//
// Regression coverage for the MemoryLock stat accounting.
//
// Codex review finding (PR #291, P2): callers invoke unlockMemory()
// unconditionally, including for regions whose lockMemory() returned
// false (routine on Linux once RLIMIT_MEMLOCK is exceeded, and on any
// host where the process is not privileged).  POSIX munlock() reports
// success for a mapped-but-unlocked range, so the unconditional
// fetch_sub() pair underflowed s_bytesLocked (size_t -> enormous) and
// drove s_regionsLocked negative, which the perf overlay then rendered
// as nonsense.
//
// These tests hold regardless of whether the host actually permits
// mlock: every assertion is expressed relative to the stats captured
// immediately before the call under test, so a privileged host and an
// RLIMIT-capped CI runner both exercise a meaningful contract.
// =================================================================
#include <QtTest/QtTest>

#include "core/MemoryLock.h"

#include <vector>

using namespace NereusSDR;

class TstMemoryLockAccounting : public QObject {
    Q_OBJECT

private slots:
    // Unlocking a region that was never locked must not touch the
    // counters at all.  This is the exact shape of the Codex finding:
    // lockMemory() failed (or was never called), the caller unlocks
    // anyway in its destructor, and munlock() returns 0.
    void unlockOfNeverLockedRegionLeavesStatsUnchanged()
    {
        std::vector<float> buf(64 * 1024, 0.0f);

        const MemoryLockStats before = memoryLockStats();
        unlockMemory(buf.data(), buf.size() * sizeof(float));
        const MemoryLockStats after = memoryLockStats();

        QCOMPARE(after.bytesLocked, before.bytesLocked);
        QCOMPARE(after.regionsLocked, before.regionsLocked);
    }

    // regionsLocked is a signed int precisely so an underflow is
    // observable rather than wrapping.  It must never go below zero.
    void regionCountNeverGoesNegative()
    {
        std::vector<float> buf(16 * 1024, 0.0f);

        for (int i = 0; i < 5; ++i) {
            unlockMemory(buf.data(), buf.size() * sizeof(float));
        }

        QVERIFY(memoryLockStats().regionsLocked >= 0);
    }

    // A successful lock/unlock pair returns the counters to their
    // starting values.  When the host refuses the lock (RLIMIT_MEMLOCK
    // on CI), the failure is counted and the byte/region counters stay
    // put through both calls -- also the contract.
    void lockThenUnlockIsBalanced()
    {
        std::vector<float> buf(32 * 1024, 0.0f);
        const std::size_t bytes = buf.size() * sizeof(float);

        const MemoryLockStats before = memoryLockStats();
        const bool locked = lockMemory(buf.data(), bytes, "tst_memory_lock");
        const MemoryLockStats held = memoryLockStats();

        if (locked) {
            QCOMPARE(held.regionsLocked, before.regionsLocked + 1);
            QVERIFY(held.bytesLocked > before.bytesLocked);
        } else {
            QCOMPARE(held.regionsLocked, before.regionsLocked);
            QCOMPARE(held.bytesLocked, before.bytesLocked);
            QCOMPARE(held.lockFailuresTotal, before.lockFailuresTotal + 1);
        }

        unlockMemory(buf.data(), bytes);
        const MemoryLockStats after = memoryLockStats();

        QCOMPARE(after.bytesLocked, before.bytesLocked);
        QCOMPARE(after.regionsLocked, before.regionsLocked);
    }

    // Double-unlock of a region that WAS successfully locked must
    // decrement exactly once.  Without a registry the second unlock
    // would subtract a second time and underflow.
    void doubleUnlockDecrementsOnlyOnce()
    {
        std::vector<float> buf(32 * 1024, 0.0f);
        const std::size_t bytes = buf.size() * sizeof(float);

        const MemoryLockStats before = memoryLockStats();
        if (!lockMemory(buf.data(), bytes, "tst_memory_lock_double")) {
            QSKIP("Host refuses mlock (RLIMIT_MEMLOCK); "
                  "unlock-underflow path covered by the other cases.");
        }

        unlockMemory(buf.data(), bytes);
        unlockMemory(buf.data(), bytes);

        const MemoryLockStats after = memoryLockStats();
        QCOMPARE(after.bytesLocked, before.bytesLocked);
        QCOMPARE(after.regionsLocked, before.regionsLocked);
    }

    // Real-world shape from SpectrumWidget / FFTEngine: a buffer is
    // locked, then reallocated to a different size, and the unlock
    // arrives carrying the NEW byte count.  The kernel lock must still
    // be released and the accounting must still balance.
    void unlockWithMismatchedLengthStillBalances()
    {
        std::vector<float> buf(32 * 1024, 0.0f);
        const std::size_t lockedBytes = buf.size() * sizeof(float);

        const MemoryLockStats before = memoryLockStats();
        if (!lockMemory(buf.data(), lockedBytes, "tst_memory_lock_resize")) {
            QSKIP("Host refuses mlock (RLIMIT_MEMLOCK); "
                  "unlock-underflow path covered by the other cases.");
        }

        // Caller now believes the region is half the size.
        unlockMemory(buf.data(), lockedBytes / 2);

        const MemoryLockStats after = memoryLockStats();
        QCOMPARE(after.bytesLocked, before.bytesLocked);
        QCOMPARE(after.regionsLocked, before.regionsLocked);
    }
};

QTEST_MAIN(TstMemoryLockAccounting)
#include "tst_memory_lock_accounting.moc"
