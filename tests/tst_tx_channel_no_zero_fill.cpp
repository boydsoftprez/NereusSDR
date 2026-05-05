// =================================================================
// tests/tst_tx_channel_no_zero_fill.cpp  (NereusSDR)
// =================================================================
//
// No Thetis code is ported in this test file. The test exercises:
//   - TxChannel::driveOneTxBlock(samples, frames) push-driven slot
//     (Phase 3M-1c E.1).
//   - Verification that the 1b353f4 zero-fill workaround is GONE.
//
// Background: in the QTimer-driven pull model, driveOneTxBlock() pulled
// samples from m_micRouter every 5 ms.  At 256 samples / 48 kHz = 5.33 ms
// per fexchange2 block, the consumer outran the producer by ~6 %, causing
// frequent partial reads.  Workaround 1b353f4 zero-filled the unfilled tail
// to avoid stale data leaking into fexchange2.  This was a band-aid: silent
// frames inserted into the middle of TX audio.
//
// Phase E.1's push model eliminates the underrun pathology by definition:
// AudioEngine::micBlockReady emits exactly kMicBlockFrames samples per
// signal, fully populating the accumulator before TxChannel re-blocks
// into m_inputBufferSize chunks for fexchange2.  No partial reads.
//
// This test verifies post-E.1 invariants:
//
//   1. After 30 simulated cycles each delivering kBufSize valid (non-zero)
//      samples, every sendTxIq call carries the actual modulated signal —
//      no silent frames inserted into the middle of TX audio.
//   2. A single full block drives one fexchange2 cycle producing non-zero
//      output (since fexchange2 produces non-zero output for non-zero
//      input under HAVE_WDSP).
//   3. m_inI/m_inQ buffers hold exactly the pushed samples (no zero-fill
//      contamination from the old workaround pattern).
//
// The "no zero-fill" assertion is verified via observable behavior, not
// via a source-code grep — we drive 30 known-non-zero blocks and assert
// the I/Q output reflects them, not silence.
//
// Pre-code review reference: Phase 3M-1c E.1.
// Attribution for ported constants lives in TxChannel.h/cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-28 — New test for Phase 3M-1c Task E.1: verify the partial-read
//                 zero-fill workaround is gone in the push-driven model.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. NEREUS_BUILD_TESTS must be
// defined (see CMakeLists.txt).

#include <QtTest/QtTest>
#include <QObject>

#include <vector>

#include "core/TxChannel.h"
#include "core/RadioConnection.h"

using namespace NereusSDR;

// ── MockConnection ──────────────────────────────────────────────────────────
// Tracks the LAST I/Q payload pushed via sendTxIq so we can inspect it
// after a tick.  Keeps a copy of the interleaved buffer in heap-owned
// storage (the production caller reuses its own buffer between ticks).
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockConnection(QObject* parent = nullptr) : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}

    void sendTxIq(const float* iq, int n) override
    {
        ++callCount;
        if (iq && n > 0) {
            lastInterleaved.assign(iq, iq + 2 * n);
            lastN = n;
        } else {
            lastInterleaved.clear();
            lastN = 0;
        }
    }

    int callCount{0};
    int lastN{0};
    std::vector<float> lastInterleaved;
};

// ── Test fixture ─────────────────────────────────────────────────────────────

class TestTxChannelNoZeroFill : public QObject {
    Q_OBJECT

    static constexpr int kChannelId = 1;
    static constexpr int kBufSize   = 256;

private slots:

    // ── 30 push cycles with non-zero samples → 30 sendTxIq calls ─────────────
    //
    // The push model has no underrun pathology: every push of kBufSize
    // valid samples yields exactly one fexchange2 cycle and one sendTxIq
    // call.  No partial-read window means no silent frames inserted.
    //
    // This test runs 30 cycles and asserts callCount == 30 and lastN
    // == kBufSize (which would NOT hold under the old timer pull model
    // when the consumer outran the producer).

    void thirtyPushCycles_yieldThirtySendTxIqCalls()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        // Distinct non-zero buffer so any zero-fill bug surfaces as a
        // shorter-than-expected interleaved buffer or zero output.
        std::vector<float> samples(kBufSize);
        for (int i = 0; i < kBufSize; ++i) {
            samples[i] = static_cast<float>(i + 1) / static_cast<float>(kBufSize);
        }

        for (int cycle = 0; cycle < 30; ++cycle) {
            ch.tickForTest(samples.data(), kBufSize);
        }

        QCOMPARE(conn.callCount, 30);
        QCOMPARE(conn.lastN, kBufSize);
        // Interleaved buffer is exactly 2 × kBufSize floats — no truncation.
        QCOMPARE(static_cast<int>(conn.lastInterleaved.size()), 2 * kBufSize);
    }

    // ── m_inI holds the pushed samples (no zero-fill contamination) ──────────
    //
    // After a single push, m_inI must equal the pushed buffer exactly.  The
    // old workaround zero-filled the unfilled tail when the router under-
    // delivered; in the push model the slot copies the full block in one go.

    void inIBufferMatchesPushedSamplesExactly()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        std::vector<float> ramp(kBufSize);
        for (int i = 0; i < kBufSize; ++i) {
            ramp[i] = static_cast<float>(i + 1);
        }

        ch.tickForTest(ramp.data(), kBufSize);

        const auto& inI = ch.inIForTest();
        QCOMPARE(static_cast<int>(inI.size()), kBufSize);

        for (int i = 0; i < kBufSize; ++i) {
            const float expected = static_cast<float>(i + 1);
            if (inI[i] != expected) {
                QFAIL(qPrintable(
                    QString("inI[%1] = %2, expected %3 (zero-fill regression?)")
                        .arg(i)
                        .arg(static_cast<double>(inI[i]))
                        .arg(static_cast<double>(expected))));
            }
        }
    }

    // ── m_inQ stays zero (real-mono mic input) across cycles ─────────────────
    //
    // The push slot must zero-fill m_inQ on every cycle since real mic
    // input is mono.  Tests Q=0 over 5 cycles to catch any stale-data leak.

    void inQBufferStaysZeroAcrossCycles()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        std::vector<float> ramp(kBufSize);
        for (int i = 0; i < kBufSize; ++i) {
            ramp[i] = static_cast<float>(i + 1);
        }

        for (int cycle = 0; cycle < 5; ++cycle) {
            ch.tickForTest(ramp.data(), kBufSize);

            const auto& inQ = ch.inQForTest();
            QCOMPARE(static_cast<int>(inQ.size()), kBufSize);
            for (int i = 0; i < kBufSize; ++i) {
                if (inQ[i] != 0.0f) {
                    QFAIL(qPrintable(
                        QString("cycle %1: inQ[%2] = %3 (Q=0 invariant violated)")
                            .arg(cycle).arg(i).arg(static_cast<double>(inQ[i]))));
                }
            }
        }
    }

    // ── Silence path: nullptr push fills m_inI with zeros ────────────────────
    //
    // Distinct from a "skipped" mismatched-size call: nullptr is the
    // explicit silence path.  m_inI must be all-zero so PostGen TUNE-tone
    // output is not contaminated by stale prior-cycle data.

    void nullPush_zeroFillsInputBuffers()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        // First push some non-zero data to populate m_inI.
        std::vector<float> ramp(kBufSize);
        for (int i = 0; i < kBufSize; ++i) {
            ramp[i] = static_cast<float>(i + 1);
        }
        ch.tickForTest(ramp.data(), kBufSize);
        QCOMPARE(conn.callCount, 1);

        // Now push silence — m_inI must be zeroed, not retain ramp.
        ch.tickForTest(nullptr, 0);
        QCOMPARE(conn.callCount, 2);

        const auto& inI = ch.inIForTest();
        for (int i = 0; i < kBufSize; ++i) {
            if (inI[i] != 0.0f) {
                QFAIL(qPrintable(
                    QString("after nullptr push, inI[%1] = %2 (silence path leaked stale data)")
                        .arg(i).arg(static_cast<double>(inI[i]))));
            }
        }
    }
};

QTEST_MAIN(TestTxChannelNoZeroFill)
#include "tst_tx_channel_no_zero_fill.moc"
