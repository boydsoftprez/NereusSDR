// =================================================================
// tests/tst_tx_channel_push_driven.cpp  (NereusSDR)
// =================================================================
//
// No Thetis code is ported in this test file. The test exercises:
//   - TxChannel::driveOneTxBlock(const float*, int) push-driven slot
//     (Phase 3M-1c E.1).
//   - Removal of QTimer-driven pull model (m_txProductionTimer).
//   - tickForTest(samples, frames) NEREUS_BUILD_TESTS seam.
//
// Phase 3M-1c E.1 converts TxChannel from a QTimer-driven puller (which
// pulled mic samples from m_micRouter every 5 ms) to a push-driven slot
// receiving samples directly from AudioEngine::micBlockReady.  This test
// verifies the new contract:
//
//   1. Calling driveOneTxBlock(samples, frames) with non-null samples
//      and frames == m_inputBufferSize drives exactly one fexchange2
//      cycle and one sendTxIq() call.
//   2. Calling with (nullptr, 0) still drives fexchange2 (silence path
//      for TUNE-tone PostGen output).
//   3. Calling with mismatched frames (e.g., 256 when expecting 720, or
//      vice versa) skips fexchange2 entirely and logs a warning.
//   4. tickForTest(samples, frames) synchronously drives one block —
//      proving the push slot is the single entry point (no QTimer).
//
// Pre-code review reference: Phase 3M-1c E.1.
//
// Attribution for ported constants lives in TxChannel.h/cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-28 — New test for Phase 3M-1c Task E.1 (TxChannel push-driven
//                 refactor). Verifies the QTimer is gone and the (samples,
//                 frames) slot is the only entry point. J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. NEREUS_BUILD_TESTS must be
// defined (see CMakeLists.txt).

#include <QtTest/QtTest>
#include <QObject>
#include <QSignalSpy>

#include <vector>

#include "core/TxChannel.h"
#include "core/RadioConnection.h"

using namespace NereusSDR;

// ── MockConnection ──────────────────────────────────────────────────────────
// Records sendTxIq() calls so tests can assert exactly how many times the
// production slot fired.
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
    void setMicPTT(bool) override {}
    void setMicXlr(bool) override {}

    void sendTxIq(const float*, int n) override
    {
        ++callCount;
        lastN = n;
    }

    int callCount{0};
    int lastN{0};
};

// ── Test fixture ─────────────────────────────────────────────────────────────

class TestTxChannelPushDriven : public QObject {
    Q_OBJECT

    static constexpr int kChannelId = 1;
    static constexpr int kBufSize   = 256;  // matches default in_size — see WdspEngine.h kTxDspBufferSize note.

private slots:

    // ── Push-slot drives exactly one fexchange2 + sendTxIq cycle ─────────────
    //
    // After setRunning(true), pushing a full kBufSize-sample block via
    // tickForTest(samples, frames) must invoke sendTxIq exactly once.
    //
    // This proves the (samples, frames) slot is the single entry point and
    // a single full block consumes-and-dispatches exactly one fexchange2 cycle.

    void pushFullBlock_drivesOneSendTxIq()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        std::vector<float> samples(kBufSize, 0.5f);
        ch.tickForTest(samples.data(), kBufSize);

        QCOMPARE(conn.callCount, 1);
        QCOMPARE(conn.lastN, kBufSize);
    }

    // ── Pushing nullptr/zero still drives fexchange2 (TUNE-tone path) ────────
    //
    // The TUNE-tone PostGen runs inside fexchange2; even with no mic samples,
    // fexchange2 must fire so the PostGen output reaches sendTxIq.  This
    // preserves TUNE behavior under the new push model.

    void pushNullSamples_stillDrivesSendTxIq()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        ch.tickForTest(nullptr, 0);

        QCOMPARE(conn.callCount, 1);
        QCOMPARE(conn.lastN, kBufSize);
    }

    // ── Mismatched frame count: skip + warning, no fexchange2 ────────────────
    //
    // Contract: caller must push exactly m_inputBufferSize samples.  If the
    // caller pushes a different size (e.g., AudioEngine emitted 720 but
    // TxChannel was configured with in_size=256), the push must be ignored
    // with a warning, never silently truncated/padded.
    //
    // QTest::ignoreMessage suppresses the expected warning so it doesn't
    // pollute test output.

    void pushMismatchedFrameCount_skipsFexchange2()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        // Expect a single warning logged for the mismatched call.
        // QDebug stream operator << inserts spaces between values, so the
        // emitted text is "frames= 720 does not match m_inputBufferSize= 256".
        QTest::ignoreMessage(
            QtWarningMsg,
            QRegularExpression("frames=\\s*\\d+.*does not match.*inputBufferSize"));

        // Push 720 samples when channel expects 256 — should skip + warn.
        std::vector<float> wrong(720, 0.25f);
        ch.tickForTest(wrong.data(), 720);

        QCOMPARE(conn.callCount, 0);
    }

    // ── tickForTest(samples, frames) is the synchronous test seam ────────────
    //
    // After E.1, tickForTest takes (samples, frames) and forwards directly
    // into the push slot.  Confirms the seam and the production slot share
    // the same entry point — there is no separate timer-driven pump.

    void tickForTest_isSynchronousPushEntryPoint()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        // Two synchronous pushes => two sendTxIq calls.  Counts must increment
        // in lockstep with each tickForTest call (no async timer interfering).
        std::vector<float> a(kBufSize, 0.1f);
        std::vector<float> b(kBufSize, 0.2f);

        ch.tickForTest(a.data(), kBufSize);
        QCOMPARE(conn.callCount, 1);

        ch.tickForTest(b.data(), kBufSize);
        QCOMPARE(conn.callCount, 2);
    }

    // (The "silenceTimer_drivesFexchange2_whenNoPushArrives" test was
    //  dropped by the Phase 3M-1c TX pump architecture redesign on
    //  2026-04-29.  TxChannel no longer owns a silence-drive timer;
    //  TxWorkerThread::onPumpTick pulls every ~5 ms unconditionally
    //  and zero-fills the gap when AudioEngine::pullTxMic returns
    //  partial / no data.  Silence "falls out for free" via the
    //  zero-filled samples reaching driveOneTxBlock with frames =
    //  kBlockFrames (256), exercised by pushFullBlock_drivesOneSendTxIq
    //  with all-zero samples.  See plan §5.3.)

    // ── !m_running guard: push is a no-op ────────────────────────────────────
    //
    // If the channel is not running, the push slot returns early and does
    // NOT call fexchange2/sendTxIq.

    void pushWhileNotRunning_isNoOp()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        MockConnection conn;
        ch.setConnection(&conn);
        // Deliberately do NOT call setRunning(true).

        std::vector<float> samples(kBufSize, 0.5f);
        ch.tickForTest(samples.data(), kBufSize);

        QCOMPARE(conn.callCount, 0);
    }

    // ── Null connection: push is a no-op ─────────────────────────────────────
    //
    // The connection guard inside the push slot must skip sendTxIq when no
    // RadioConnection is attached (or after detach).

    void pushWithNullConnection_isNoOp()
    {
        TxChannel ch(kChannelId, kBufSize, kBufSize);
        ch.setRunning(true);  // running but no connection

        std::vector<float> samples(kBufSize, 0.5f);
        ch.tickForTest(samples.data(), kBufSize);

        // No crash; nothing observable since there is no connection to count.
        // The implicit assertion: this call returns without crashing.
        QVERIFY(true);
    }
};

QTEST_MAIN(TestTxChannelPushDriven)
#include "tst_tx_channel_push_driven.moc"
