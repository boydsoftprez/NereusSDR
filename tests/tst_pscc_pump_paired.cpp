// no-port-check: test-only — NereusSDR-original.  Inline doc comments
// cite Thetis source filenames as pointers to the upstream behaviour
// each assertion verifies; no Thetis logic is reproduced here.
//
// Phase 3M-4 bench-fix 2026-05-23 (J.J. Boyd KG4VCF): per-packet paired
// PS streams.
//
// Replaces the independent-rings architecture (m_txMonRing + m_psFbRing,
// drained when both reach kBlockSize) with the Thetis-faithful
// sync.c InboundBlock(id=1) [v2.10.3.15] pairing.  The old path could
// drift by hundreds of samples between the two rings whenever Qt's
// queued-connection scheduler delivered the per-DDC `iqDataReceived`
// signals out-of-step.  Bench-observed drift on ANAN-G2E (HermesC10
// effective board) was bestLag = 189 samples / 985 us — enough to make
// calcc fit a Lissajous instead of a function, which is exactly the
// AmpView bow-tie pattern reported on 2026-05-23.
//
// The new path mirrors Thetis router.c:91-102 [v2.10.3.15] case 2 +
// sync.c:53-58 [v2.10.3.15] case 1: both pscc() pointers reference
// per-stream buffers that were populated in the SAME xrouter() call,
// so cross-stream sample alignment is guaranteed by construction.

#include <QtTest/QtTest>

#include "core/PsccPump.h"

using namespace NereusSDR;

class TestPsccPumpPaired : public QObject {
    Q_OBJECT

private slots:

    // ── 1. Single packet → exactly one pscc call with the correct size ────
    //
    // Mirrors sync.c:53-58 [v2.10.3.15]: one InboundBlock(id=1) call per
    // xrouter() invocation = one pscc() call per packet.  No buffering,
    // no draining loop.  Packet sps becomes the pscc `size` argument.
    void pairedCall_singlePacket_callsPsccOnce() {
        PsccPump pump;
        pump.setTxChannelId(1);
        pump.setActive(true, /*txMonDdc=*/1, /*psFbDdc=*/0);
        pump.setSkipPsccForTests(true);

        // Mock a packet that delivered 119 I/Q samples per stream (the
        // G2E P2 case: spp=238 total samples / nstreams=2 = sps=119).
        constexpr int kSps = 119;
        QVector<float> psFb(kSps * 2);
        QVector<float> txMon(kSps * 2);
        for (int i = 0; i < kSps * 2; ++i) {
            psFb[i]  = 0.001f * static_cast<float>(i);
            txMon[i] = -0.001f * static_cast<float>(i);
        }

        pump.onPsPairedIqData(/*psFbDdc=*/0, psFb,
                              /*txMonDdc=*/1, txMon);

        QCOMPARE(pump.totalBlocksPumped(), qint64{1});

        const auto& args = pump.lastPsccArgsForTests();
        QCOMPARE(args.callCount, qint64{1});
        QCOMPARE(args.channel, 1);
        QCOMPARE(args.size, kSps);
        QCOMPARE(args.tx.size(), static_cast<size_t>(kSps * 2));
        QCOMPARE(args.rx.size(), static_cast<size_t>(kSps * 2));
    }

    // ── 2. Sample-for-sample alignment from input to pscc ─────────────────
    //
    // The whole point of the source-first fix: PS-feedback samples that
    // entered as element [i] of psFbSamples MUST appear as element [i]
    // of the rx double array passed to pscc.  Same for TX-monitor → tx.
    // Distinguishable patterns make any misordering or cross-stream
    // contamination immediately visible.
    //
    // cmaster.cs:533-534 [v2.10.3.13]: SetPSRxIdx(0,0) + SetPSTxIdx(0,1)
    // → pscc(channel, size, data[1]=TX-mon, data[0]=PS-fb).  Our slot
    // takes (psFbDdc, psFbSamples, txMonDdc, txMonSamples); pscc gets
    // (txMon, psFb) in (tx, rx) order.  This test pins both assignments.
    void pairedCall_samplesPreserveAlignment() {
        PsccPump pump;
        pump.setTxChannelId(1);
        pump.setActive(true, /*txMonDdc=*/1, /*psFbDdc=*/0);
        pump.setSkipPsccForTests(true);

        QVector<float> psFb(8);   // 4 I/Q samples
        QVector<float> txMon(8);
        for (int i = 0; i < 8; ++i) {
            psFb[i]  = 1000.0f + static_cast<float>(i);  // 1000..1007
            txMon[i] = 2000.0f + static_cast<float>(i);  // 2000..2007
        }

        pump.onPsPairedIqData(/*psFbDdc=*/0, psFb,
                              /*txMonDdc=*/1, txMon);

        const auto& args = pump.lastPsccArgsForTests();
        QCOMPARE(args.size, 4);
        for (int i = 0; i < 8; ++i) {
            // PS-feedback → pscc rx (Thetis ps_rx_idx=0 maps to rx).
            QCOMPARE(args.rx[i], static_cast<double>(psFb[i]));
            // TX-monitor → pscc tx (Thetis ps_tx_idx=1 maps to tx).
            QCOMPARE(args.tx[i], static_cast<double>(txMon[i]));
        }
    }

    // ── 3. Consecutive paired calls have zero crosstalk ────────────────────
    //
    // The bug the rings caused was N-th call seeing data from the (N-K)-th
    // packet on the OTHER stream because the ring drift slid samples past
    // each other.  Pin the contract that each onPsPairedIqData call sees
    // ONLY its own samples in the immediately-following pscc call.
    void pairedCall_consecutiveCallsHaveNoCrosstalk() {
        PsccPump pump;
        pump.setTxChannelId(1);
        pump.setActive(true, /*txMonDdc=*/1, /*psFbDdc=*/0);
        pump.setSkipPsccForTests(true);

        constexpr int kCalls = 5;
        for (int call = 0; call < kCalls; ++call) {
            QVector<float> psFb(8), txMon(8);
            for (int i = 0; i < 8; ++i) {
                psFb[i]  = static_cast<float>(call * 100 + i);
                txMon[i] = static_cast<float>(call * 100 + 50 + i);
            }
            pump.onPsPairedIqData(0, psFb, 1, txMon);

            const auto& args = pump.lastPsccArgsForTests();
            QCOMPARE(args.callCount, static_cast<qint64>(call + 1));
            for (int i = 0; i < 8; ++i) {
                QCOMPARE(args.rx[i], static_cast<double>(call * 100 + i));
                QCOMPARE(args.tx[i], static_cast<double>(call * 100 + 50 + i));
            }
        }

        QCOMPARE(pump.totalBlocksPumped(), static_cast<qint64>(kCalls));
    }

    // ── 4. Inactive pump silently drops the call ──────────────────────────
    //
    // Matches the existing onIqData contract: only consume samples when
    // setActive(true, ...) has been called by ReceiverManager/codec.
    void pairedCall_inactive_dropsSilently() {
        PsccPump pump;
        pump.setTxChannelId(1);
        // setActive deliberately NOT called.
        pump.setSkipPsccForTests(true);

        QVector<float> psFb(8, 0.5f), txMon(8, 0.5f);
        pump.onPsPairedIqData(0, psFb, 1, txMon);

        QCOMPARE(pump.totalBlocksPumped(), qint64{0});
        QCOMPARE(pump.lastPsccArgsForTests().callCount, qint64{0});
    }

    // ── 5. Wrong-DDC paired call drops silently ───────────────────────────
    //
    // Defends against the connection layer emitting the signal with the
    // psFb/txMon DDC indices reversed.  Thetis prevents this by reading
    // SetPSRxIdx/SetPSTxIdx state when calling pscc — we match by
    // refusing the call if (psFbDdc, txMonDdc) don't agree with the
    // configured pair.
    void pairedCall_wrongDdcs_dropsSilently() {
        PsccPump pump;
        pump.setActive(true, /*txMonDdc=*/1, /*psFbDdc=*/0);
        pump.setSkipPsccForTests(true);

        QVector<float> psFb(8, 0.5f), txMon(8, 0.5f);
        // Caller swapped psFb and txMon DDC indices in the emit.
        pump.onPsPairedIqData(/*psFbDdc=*/1, psFb,
                              /*txMonDdc=*/0, txMon);

        QCOMPARE(pump.totalBlocksPumped(), qint64{0});
    }

    // ── 6. Mismatched buffer sizes drop the call ──────────────────────────
    //
    // Both buffers come from the same packet → they MUST have the same
    // sample count.  A size mismatch indicates a deinterleave bug
    // upstream; refuse to feed calcc and let the next packet through
    // cleanly.
    void pairedCall_sizeMismatch_dropsSilently() {
        PsccPump pump;
        pump.setActive(true, /*txMonDdc=*/1, /*psFbDdc=*/0);
        pump.setSkipPsccForTests(true);

        QVector<float> psFb(8, 0.5f), txMon(10, 0.5f);  // off-by-one
        pump.onPsPairedIqData(0, psFb, 1, txMon);

        QCOMPARE(pump.totalBlocksPumped(), qint64{0});
    }
};

QTEST_APPLESS_MAIN(TestPsccPumpPaired)
#include "tst_pscc_pump_paired.moc"
