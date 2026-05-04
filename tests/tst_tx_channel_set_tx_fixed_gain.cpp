// =================================================================
// tests/tst_tx_channel_set_tx_fixed_gain.cpp  (NereusSDR)
// =================================================================
//
// No Thetis code is directly ported in this test file.  The test exercises:
//   - TxChannel::setTxFixedGain(double)  — Phase 1C of issue #167
//
// Porting context (cited in TxChannel.h / TxChannel.cpp):
//   setTxFixedGain mirrors Thetis cmaster.cs:1115-1119 [v2.10.3.13]
//   CMSetTXOutputLevel:
//     double level = Audio.RadioVolume * Audio.HighSWRScale;
//     cmaster.SetTXFixedGain(0, level, level);
//   The wrapper takes the already-composed `level` argument and pushes it
//   to the WDSP/ChannelMaster SetTXFixedGain entry point with channel == m_id
//   (Igain == Qgain == level).
//
// Tests verify (NEREUS_BUILD_TESTS test-seam accessor required):
//   1. Initial state: m_lastFixedGain initialises to NaN so the first call
//      (any value) always passes the idempotent guard.
//   2. First call dispatches: NaN → value stored after setTxFixedGain(0.5).
//   3. First call with 0.0 dispatches: NaN guard passes even for zero.
//   4. Second identical call no-ops: setTxFixedGain(0.5); setTxFixedGain(0.5)
//      leaves m_lastFixedGain at 0.5 (no change observable).
//   5. Different value dispatches: setTxFixedGain(0.5); setTxFixedGain(0.3)
//      → m_lastFixedGain becomes 0.3.
//   6. Round-trip sequence: a chain of calls leaves the latest value stored.
//   7. Values >1 are accepted (Thetis applies no clamp).
//
// WDSP-call verification is not possible without a mock; in unit-test builds
// txa[].rsmpin.p == nullptr so the WDSP entry point is a documented no-op
// behind the null-guard.  The tests confirm storage state changes correctly
// and no crash occurs (matches the strategy used in tst_tx_channel_mic_mute.cpp).
//
// Total test cases: 7
//
// Requires NEREUS_BUILD_TESTS (set by CMakeLists target
// tst_tx_channel_set_tx_fixed_gain).  Test-seam accessor
// (lastFixedGainForTest) is compiled into TxChannel only when that define
// is set.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 — New test for issue #167 Phase 1 Agent 1C: TxChannel
//                 setTxFixedGain wrapper (idempotent NaN-aware guard +
//                 WDSP SetTXFixedGain entry-point pass-through).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis source cites are
// in TxChannel.h/cpp.

#define NEREUS_BUILD_TESTS 1

#include <QtTest/QtTest>
#include <cmath>   // std::isnan

#include "core/TxChannel.h"

using namespace NereusSDR;

class TestTxChannelSetTxFixedGain : public QObject {
    Q_OBJECT

    // WDSP TX channel ID — from Thetis cmaster.c:177-190 [v2.10.3.13] (TX=1).
    static constexpr int kChannelId = 1;

private slots:

    // ── Initial state ─────────────────────────────────────────────────────────
    //
    // m_lastFixedGain initialises to quiet_NaN so the first setTxFixedGain
    // call (any value, including 0.0) always passes the idempotent guard.
    // This matches the NaN-aware pattern used by setMicPreamp and the
    // VOX/anti-VOX double setters (D.3 / D.6).

    void setTxFixedGain_initialState_isNaN()
    {
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastFixedGainForTest()));
    }

    // ── First call dispatches ─────────────────────────────────────────────────
    //
    // NaN sentinel guarantees the first call passes the guard regardless of
    // the value.  After the call the value is stored in m_lastFixedGain.

    void setTxFixedGain_firstCall_storesValue()
    {
        TxChannel ch(kChannelId);
        QVERIFY(std::isnan(ch.lastFixedGainForTest()));
        ch.setTxFixedGain(0.5);
        QCOMPARE(ch.lastFixedGainForTest(), 0.5);
    }

    void setTxFixedGain_firstCallWithZero_storesZero()
    {
        // Guard must pass for 0.0 on first call.  The NaN sentinel ensures
        // any value (including 0.0) is dispatched the first time.
        TxChannel ch(kChannelId);
        ch.setTxFixedGain(0.0);
        QCOMPARE(ch.lastFixedGainForTest(), 0.0);
    }

    // ── Idempotent guard: second identical call no-ops ────────────────────────
    //
    // Once a value is stored, a second call with the same value is a no-op.
    // The stored value is unchanged (and the WDSP entry point is not called
    // again — important for a hot-path TX setter).

    void setTxFixedGain_secondIdenticalCall_isNoOp()
    {
        TxChannel ch(kChannelId);
        ch.setTxFixedGain(0.5);
        QCOMPARE(ch.lastFixedGainForTest(), 0.5);
        ch.setTxFixedGain(0.5);  // idempotent guard fires
        QCOMPARE(ch.lastFixedGainForTest(), 0.5);
    }

    // ── Different value dispatches ────────────────────────────────────────────
    //
    // After storing 0.5, a call with 0.3 must update m_lastFixedGain.

    void setTxFixedGain_differentValue_dispatches()
    {
        TxChannel ch(kChannelId);
        ch.setTxFixedGain(0.5);
        QCOMPARE(ch.lastFixedGainForTest(), 0.5);
        ch.setTxFixedGain(0.3);
        QCOMPARE(ch.lastFixedGainForTest(), 0.3);
    }

    // ── Sequence round-trip ───────────────────────────────────────────────────
    //
    // A chain of calls leaves the latest value stored.

    void setTxFixedGain_sequenceRoundTrip_storesLatest()
    {
        TxChannel ch(kChannelId);
        ch.setTxFixedGain(0.1);
        ch.setTxFixedGain(0.5);
        ch.setTxFixedGain(0.0);  // typical SWR-foldback path: drop level to 0
        ch.setTxFixedGain(0.8);  // user adjusts back up
        QCOMPARE(ch.lastFixedGainForTest(), 0.8);
    }

    // ── Values >1 are accepted ────────────────────────────────────────────────
    //
    // Thetis applies no clamp at the cmaster layer (cmaster.cs:1115-1119
    // [v2.10.3.13] just calls SetTXFixedGain(0, level, level) directly).
    // The NereusSDR wrapper matches this behaviour — downstream WDSP/
    // ChannelMaster handles any range concerns.

    void setTxFixedGain_valueAboveOne_accepted()
    {
        TxChannel ch(kChannelId);
        ch.setTxFixedGain(1.5);  // above [0, 1] range — no clamp expected
        QCOMPARE(ch.lastFixedGainForTest(), 1.5);
    }
};

QTEST_APPLESS_MAIN(TestTxChannelSetTxFixedGain)
#include "tst_tx_channel_set_tx_fixed_gain.moc"
