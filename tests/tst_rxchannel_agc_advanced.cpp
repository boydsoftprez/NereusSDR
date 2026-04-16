// tst_rxchannel_agc_advanced.cpp
//
// Verifies RxChannel AGC advanced setter/accessor contracts without
// requiring a live WDSP channel.
//
// Strategy: only test paths where no WDSP API call is made:
//   1. Default-value assertions — accessor returns Thetis-sourced default.
//      The setter is never called, so WDSP is never invoked.
//   2. Same-value idempotency — setter early-returns when value equals the
//      currently stored atomic. WDSP call skipped by the early return guard.
//
// This avoids calling OpenChannel / WDSPwisdom in the test binary, which
// would require the FFTW wisdom file to exist (slow/absent in CI). The
// actual WDSP call path is exercised by the integration tests that spin
// up a WdspEngine (tst_p1_loopback_connection, tst_rx_dsp_worker_thread).
//
// Source citations:
//   From Thetis Project Files/Source/Console/radio.cs:1037-1124 — AGC advanced
//   From Thetis Project Files/Source/Console/console.cs:45977  — AGCThresh
//   WDSP: third_party/wdsp/src/wcpAGC.c:418,427,436,504,537

#include <QtTest/QtTest>
#include "core/RxChannel.h"

using namespace NereusSDR;

// Use a channel id that definitely has no WDSP channel allocated.
// All paths tested here must not invoke any WDSP API.
static constexpr int kTestChannel  = 99;  // Never opened via OpenChannel
static constexpr int kTestBufSize  = 1024;
static constexpr int kTestRate     = 48000;

class TestRxChannelAgcAdvanced : public QObject {
    Q_OBJECT

private slots:
    // ── setAgcThreshold ──────────────────────────────────────────────────────

    void agcThresholdDefaultIsMinusTwenty() {
        // From Thetis console.cs:45977 — agc_thresh_point default = -20
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcThreshold(), -20);
    }

    void setAgcThresholdEarlyReturnOnSameValue() {
        // Same value as default — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcThreshold(-20);   // same as default → early return
        QCOMPARE(ch.agcThreshold(), -20);
    }

    // ── setAgcHang ───────────────────────────────────────────────────────────

    void agcHangDefaultIs250() {
        // From Thetis radio.cs:1056-1057 — rx_agc_hang = 250 ms
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcHang(), 250);
    }

    void setAgcHangEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcHang(250);   // same as default → early return
        QCOMPARE(ch.agcHang(), 250);
    }

    // ── setAgcSlope ──────────────────────────────────────────────────────────

    void agcSlopeDefaultIsZero() {
        // From Thetis radio.cs:1107-1108 — rx_agc_slope = 0
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcSlope(), 0);
    }

    void setAgcSlopeEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcSlope(0);   // same as default → early return
        QCOMPARE(ch.agcSlope(), 0);
    }

    // ── setAgcAttack ─────────────────────────────────────────────────────────

    void agcAttackDefaultIsTwo() {
        // From WDSP wcpAGC.c create_wcpagc — tau_attack default 2 ms
        // From Thetis dsp.cs:116-117 — P/Invoke declared, UI disabled/hidden
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcAttack(), 2);
    }

    void setAgcAttackEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcAttack(2);   // same as default → early return
        QCOMPARE(ch.agcAttack(), 2);
    }

    // ── setAgcDecay ──────────────────────────────────────────────────────────

    void agcDecayDefaultIs250() {
        // From Thetis radio.cs:1037-1038 — rx_agc_decay = 250 ms
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.agcDecay(), 250);
    }

    void setAgcDecayEarlyReturnOnSameValue() {
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAgcDecay(250);   // same as default → early return
        QCOMPARE(ch.agcDecay(), 250);
    }
};

QTEST_MAIN(TestRxChannelAgcAdvanced)
#include "tst_rxchannel_agc_advanced.moc"
