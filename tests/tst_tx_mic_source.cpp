// =================================================================
// tests/tst_tx_mic_source.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file.  Exercises TxMicSource — the Thetis
// Inbound/cm_main port at src/core/audio/TxMicSource.{h,cpp}.
//
// Test surface:
//   1. construct/start/stop clean
//   2. waitForBlock returns false before any inbound (with finite timeout)
//   3. inbound of exactly kBlockFrames releases exactly 1 semaphore;
//      drainBlock yields the samples (I has data, Q is zero)
//   4. inbound of 3 * kBlockFrames releases 3 semaphores
//   5. wrap-aware ring write preserves sample order on drain
//   6. partial-block inbound does not release semaphore; subsequent
//      inbound that completes the block does
//   7. stop() while consumer is waitForBlock(INFINITE) — consumer
//      unblocks and isRunning returns false
//   8. concurrent producer + consumer — every drained block is a whole,
//      intact produced block, tolerating the documented overwrite-on-
//      overrun that the ring inherits from Thetis Inbound()
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-29 — New test for Phase 3M-1c TX pump architecture redesign v3
//                 by J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via Anthropic Claude Code.
//   2026-07-31 — concurrent_producerConsumer_noDataCorruption rewritten to
//                 assert the ring's actual contract (whole intact blocks)
//                 instead of "no sample is ever dropped", which the ring
//                 never promised, and to move its assertions off the
//                 consumer thread. By J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.  No Thetis logic ported.

#include <QtTest/QtTest>
#include <QObject>
#include <QThread>

#include <atomic>
#include <thread>
#include <vector>

#include "core/audio/TxMicSource.h"

using namespace NereusSDR;

class TestTxMicSource : public QObject {
    Q_OBJECT

private slots:

    // ── 1. Construct / start / stop are clean ──────────────────────────────
    void constructStartStop_clean()
    {
        TxMicSource src;
        QCOMPARE(src.isRunning(), false);
        src.start();
        QCOMPARE(src.isRunning(), true);
        src.stop();
        QCOMPARE(src.isRunning(), false);
    }

    // ── 2. waitForBlock returns false before any inbound ───────────────────
    //
    // Use a finite (non-INFINITE) timeout so the test does not hang.  No
    // semaphore release has happened, so the wait should time out.
    void waitForBlock_returnsFalse_beforeAnyInbound()
    {
        TxMicSource src;
        src.start();
        QCOMPARE(src.waitForBlock(/*timeoutMs=*/10), false);
        src.stop();
    }

    // ── 3. Inbound of exactly kBlockFrames → 1 semaphore + drain matches ───
    void inbound_oneBlock_releasesOneSemaphore_drainYieldsSamples()
    {
        TxMicSource src;
        src.start();

        std::vector<float> samples(TxMicSource::kBlockFrames);
        for (int i = 0; i < TxMicSource::kBlockFrames; ++i) {
            samples[i] = static_cast<float>(i + 1) / 1000.0f;
        }
        src.inbound(samples.data(), TxMicSource::kBlockFrames);

        QVERIFY(src.waitForBlock(/*timeoutMs=*/100));
        // No second block ready
        QCOMPARE(src.waitForBlock(/*timeoutMs=*/10), false);

        std::vector<double> drained(2 * TxMicSource::kBlockFrames, 0.0);
        src.drainBlock(drained.data());

        // Verify all I samples land + all Q samples are zero.
        for (int i = 0; i < TxMicSource::kBlockFrames; ++i) {
            QCOMPARE(drained[2 * i + 0], static_cast<double>(samples[i]));
            QCOMPARE(drained[2 * i + 1], 0.0);
        }

        src.stop();
    }

    // ── 4. Inbound of 3 * kBlockFrames → 3 semaphores, all drain in order ──
    void inbound_threeBlocks_releasesThreeSemaphores()
    {
        TxMicSource src;
        src.start();

        const int n = 3 * TxMicSource::kBlockFrames;
        std::vector<float> samples(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            samples[static_cast<size_t>(i)] = static_cast<float>(i) / 10000.0f;
        }
        src.inbound(samples.data(), n);

        for (int blk = 0; blk < 3; ++blk) {
            QVERIFY(src.waitForBlock(/*timeoutMs=*/50));
            std::vector<double> drained(2 * TxMicSource::kBlockFrames, 0.0);
            src.drainBlock(drained.data());
            for (int i = 0; i < TxMicSource::kBlockFrames; ++i) {
                const int srcIdx = blk * TxMicSource::kBlockFrames + i;
                QCOMPARE(drained[2 * i + 0], static_cast<double>(samples[srcIdx]));
                QCOMPARE(drained[2 * i + 1], 0.0);
            }
        }
        QCOMPARE(src.waitForBlock(/*timeoutMs=*/10), false);

        src.stop();
    }

    // ── 5. Wrap-aware ring write preserves sample order on drain ───────────
    //
    // Push a stream that exactly fills the ring twice over so writes wrap.
    // Drain blocks one at a time — last block written should be the one
    // that drains last (we read the most recent ring contents).
    //
    // Note: we do NOT exceed ring capacity in a single inbound() call
    // (that's a separate clamp behaviour in the implementation).  Instead
    // we do many small inbound()s that span a wrap boundary.
    void inbound_wrapAware_preservesSampleOrder()
    {
        TxMicSource src;
        src.start();

        // Total samples = 6 blocks (2 ring fills at kRingBlockMultiple=8 is
        // less than full, so no overwrite — instead we fill, drain, fill,
        // crossing the wrap boundary).
        // Push 4 blocks → drain 4 blocks → push 4 more (which wraps the
        // write index past end-of-ring).
        const int kBlk = TxMicSource::kBlockFrames;
        std::vector<float> phase1(static_cast<size_t>(4 * kBlk));
        for (int i = 0; i < 4 * kBlk; ++i) {
            phase1[static_cast<size_t>(i)] = static_cast<float>(i) * 0.1f;
        }
        src.inbound(phase1.data(), static_cast<int>(phase1.size()));

        for (int blk = 0; blk < 4; ++blk) {
            QVERIFY(src.waitForBlock(/*timeoutMs=*/50));
            std::vector<double> drained(2 * kBlk, 0.0);
            src.drainBlock(drained.data());
            for (int i = 0; i < kBlk; ++i) {
                const int srcIdx = blk * kBlk + i;
                QCOMPARE(drained[2 * i + 0],
                         static_cast<double>(phase1[static_cast<size_t>(srcIdx)]));
            }
        }

        // Phase 2 — write 4 more blocks, which will wrap past end-of-ring
        // (ring is 8 blocks of capacity, write idx is at 4 after phase 1,
        // so phase 2 writes 4-8 then wraps to 0).
        std::vector<float> phase2(static_cast<size_t>(4 * kBlk));
        for (int i = 0; i < 4 * kBlk; ++i) {
            phase2[static_cast<size_t>(i)] = -static_cast<float>(i) * 0.1f;
        }
        src.inbound(phase2.data(), static_cast<int>(phase2.size()));

        for (int blk = 0; blk < 4; ++blk) {
            QVERIFY(src.waitForBlock(/*timeoutMs=*/50));
            std::vector<double> drained(2 * kBlk, 0.0);
            src.drainBlock(drained.data());
            for (int i = 0; i < kBlk; ++i) {
                const int srcIdx = blk * kBlk + i;
                QCOMPARE(drained[2 * i + 0],
                         static_cast<double>(phase2[static_cast<size_t>(srcIdx)]));
            }
        }

        src.stop();
    }

    // ── 6. Partial inbound does not release; completing it does ─────────────
    void inbound_partialBlock_doesNotReleaseUntilComplete()
    {
        TxMicSource src;
        src.start();

        const int half = TxMicSource::kBlockFrames / 2;
        std::vector<float> partial(static_cast<size_t>(half), 0.5f);
        src.inbound(partial.data(), half);

        // No semaphore yet — short timeout.
        QCOMPARE(src.waitForBlock(/*timeoutMs=*/10), false);

        // Push the second half.  This should release exactly one semaphore.
        std::vector<float> rest(static_cast<size_t>(TxMicSource::kBlockFrames - half), 0.7f);
        src.inbound(rest.data(), static_cast<int>(rest.size()));
        QVERIFY(src.waitForBlock(/*timeoutMs=*/100));
        QCOMPARE(src.waitForBlock(/*timeoutMs=*/10), false);

        std::vector<double> drained(2 * TxMicSource::kBlockFrames, 0.0);
        src.drainBlock(drained.data());
        // First half = 0.5f promoted to double, second half = 0.7f promoted
        // to double.  Compare against the same float-promotion to avoid
        // float→double rounding mismatches (0.7f != 0.7).
        for (int i = 0; i < half; ++i) {
            QCOMPARE(drained[2 * i + 0], static_cast<double>(0.5f));
        }
        for (int i = half; i < TxMicSource::kBlockFrames; ++i) {
            QCOMPARE(drained[2 * i + 0], static_cast<double>(0.7f));
        }

        src.stop();
    }

    // ── 7. stop() unblocks a consumer waiting in waitForBlock(INFINITE) ────
    //
    // Spawn a thread that calls waitForBlock(-1) (INFINITE).  Then call
    // stop() from the test thread.  The waiting thread must return.
    void stop_unblocksConsumer_inWaitForBlockInfinite()
    {
        TxMicSource src;
        src.start();

        std::atomic<bool> consumerReturned{false};
        std::atomic<bool> consumerSawRunning{true};
        std::thread t([&] {
            (void)src.waitForBlock(/*timeoutMs=*/-1);
            // After unblock, isRunning should be false (poison release path).
            consumerSawRunning.store(src.isRunning());
            consumerReturned.store(true);
        });

        // Give the consumer a moment to enter the wait.
        QTest::qWait(20);
        QCOMPARE(consumerReturned.load(), false);

        src.stop();
        t.join();

        QCOMPARE(consumerReturned.load(), true);
        QCOMPARE(consumerSawRunning.load(), false);
    }

    // ── 8. Concurrent producer + consumer with known sequence ──────────────
    //
    // Producer pushes N blocks of monotonically-increasing sample values.
    // Consumer drains all N blocks.  Verify the drained sequence equals
    // the produced sequence in order.  Mirrors the SPSC discipline of
    // Thetis Inbound() + cm_main + cmdata().
    // The ring overwrites on overrun by design (TxMicSource.h:112-118,
    // porting Thetis Inbound() at cmbuffs.c:108-109 [v2.10.3.13], whose own
    // comment records the same absent overwrite check).  The semaphore
    // counts blocks *produced*, so a consumer that falls behind still
    // completes every drain; it simply reads data the producer has since
    // replaced.  32 blocks are pushed 50 us apart through an 8-block ring,
    // so under load the producer can and does lap the consumer.
    //
    // "No data corruption" therefore cannot mean "every sample survives" —
    // that was never the contract.  What IS guaranteed follows from both
    // indices advancing in aligned kBlockFrames steps: drain n reads the
    // slot the producer wrote block n into, so it yields a whole, intact
    // produced block, either block n or whichever block later lapped it at
    // n + k*kRingBlockMultiple.
    //
    // Deliberately NOT asserted: that observed block indices increase.
    // Drain n can observe n+8 while drain n+1 observes n+1, so that
    // assertion would itself be load-sensitive.
    void concurrent_producerConsumer_noDataCorruption()
    {
        TxMicSource src;
        src.start();

        const int kBlocks = 32;
        const int kBlk = TxMicSource::kBlockFrames;
        const int kTotal = kBlocks * kBlk;

        std::vector<float> produced(static_cast<size_t>(kTotal));
        for (int i = 0; i < kTotal; ++i) {
            produced[static_cast<size_t>(i)] = static_cast<float>(i);
        }

        // Observations are recorded by the consumer thread and asserted on
        // the main thread after join().  QVERIFY/QCOMPARE expand to a bare
        // `return` on failure and write into QtTest's per-test global state;
        // from a std::thread a failure would silently truncate the consumer
        // loop rather than fail the test.
        std::vector<double> firstSample(static_cast<size_t>(kBlocks), -1.0);
        std::vector<bool>   contiguous(static_cast<size_t>(kBlocks), false);
        std::vector<bool>   qChannelZero(static_cast<size_t>(kBlocks), false);
        std::vector<bool>   acquired(static_cast<size_t>(kBlocks), false);

        std::thread prod([&] {
            // Push in chunks of 1 block at a time with a short yield
            // between, simulating the 1.33-ms-per-block radio cadence.
            for (int blk = 0; blk < kBlocks; ++blk) {
                src.inbound(produced.data() + blk * kBlk, kBlk);
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        });

        std::thread cons([&] {
            for (int blk = 0; blk < kBlocks; ++blk) {
                if (!src.waitForBlock(/*timeoutMs=*/2000)) {
                    break;
                }
                const size_t u = static_cast<size_t>(blk);
                acquired[u] = true;

                std::vector<double> tmp(2 * kBlk, 0.0);
                src.drainBlock(tmp.data());

                firstSample[u] = tmp[0];
                bool runContiguous = true;
                bool runQZero = true;
                for (int i = 0; i < kBlk; ++i) {
                    if (tmp[2 * i + 0] != tmp[0] + static_cast<double>(i)) {
                        runContiguous = false;
                    }
                    if (tmp[2 * i + 1] != 0.0) {
                        runQZero = false;
                    }
                }
                contiguous[u]   = runContiguous;
                qChannelZero[u] = runQZero;
            }
        });

        prod.join();
        cons.join();

        int lapped = 0;
        for (int blk = 0; blk < kBlocks; ++blk) {
            const size_t u = static_cast<size_t>(blk);

            // One semaphore release per produced block, so every drain must
            // succeed whether or not an overrun happened.
            QVERIFY2(acquired[u], qPrintable(QStringLiteral("drain %1 timed out").arg(blk)));

            // The actual no-corruption property: an intact, whole block.
            QVERIFY2(contiguous[u],
                     qPrintable(QStringLiteral("drain %1 is not a contiguous block (first sample %2)")
                                    .arg(blk).arg(firstSample[u])));
            QVERIFY2(qChannelZero[u],
                     qPrintable(QStringLiteral("drain %1 has a non-zero Q sample").arg(blk)));

            // ...and a block the producer really wrote, at or ahead of the
            // one this drain was scheduled against, a whole ring lap apart.
            const double first = firstSample[u];
            const qint64 firstInt = static_cast<qint64>(first);
            QVERIFY2(first >= 0.0 && static_cast<double>(firstInt) == first
                         && firstInt % kBlk == 0,
                     qPrintable(QStringLiteral("drain %1 first sample %2 is not block-aligned")
                                    .arg(blk).arg(first)));

            const int observed = static_cast<int>(firstInt / kBlk);
            QVERIFY2(observed >= blk && observed < kBlocks,
                     qPrintable(QStringLiteral("drain %1 returned out-of-range block %2")
                                    .arg(blk).arg(observed)));
            QVERIFY2((observed - blk) % TxMicSource::kRingBlockMultiple == 0,
                     qPrintable(QStringLiteral("drain %1 returned block %2, which is not a whole "
                                               "ring lap ahead").arg(blk).arg(observed)));

            if (observed != blk) {
                ++lapped;
            }
        }

        // Visible in ctest output without failing, so a change that starts
        // dropping audio constantly is still noticeable here.
        if (lapped > 0) {
            qInfo("producer lapped the consumer on %d of %d drains "
                  "(documented overrun, not a failure)", lapped, kBlocks);
        }

        src.stop();
    }
};

QTEST_MAIN(TestTxMicSource)
#include "tst_tx_mic_source.moc"
