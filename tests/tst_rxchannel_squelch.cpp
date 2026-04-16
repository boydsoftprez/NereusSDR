// tst_rxchannel_squelch.cpp
//
// Verifies RxChannel squelch (SSB/AM/FM) setter/accessor contracts without
// requiring a live WDSP channel.
//
// Strategy: only test paths where no WDSP API call is made:
//   1. Default-value assertions — accessor returns Thetis-sourced default.
//      The setter is never called, so WDSP is never invoked.
//   2. Same-value idempotency — setter early-returns when value equals the
//      currently stored atomic. WDSP call skipped by the early return guard.
//
// Source citations:
//   From Thetis Project Files/Source/Console/radio.cs:1185 — _bSSqlOn = false
//   From Thetis Project Files/Source/Console/radio.cs:1293 — rx_am_squelch_on = false
//   From Thetis Project Files/Source/Console/radio.cs:1312 — rx_fm_squelch_on = false
//   WDSP: third_party/wdsp/src/ssql.c:331, amsq.c, fmsq.c:236

#include <QtTest/QtTest>
#include "core/RxChannel.h"

using namespace NereusSDR;

// Use a channel id that definitely has no WDSP channel allocated.
// All paths tested here must not invoke any WDSP API.
static constexpr int kTestChannel  = 99;  // Never opened via OpenChannel
static constexpr int kTestBufSize  = 1024;
static constexpr int kTestRate     = 48000;

class TestRxChannelSquelch : public QObject {
    Q_OBJECT

private slots:
    // ── setSsqlEnabled ───────────────────────────────────────────────────────

    void ssqlEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1185 — _bSSqlOn default = false
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.ssqlEnabled(), false);
    }

    void setSsqlEnabledEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setSsqlEnabled(false);   // same as default → early return
        QCOMPARE(ch.ssqlEnabled(), false);
    }

    // ── setAmsqEnabled ───────────────────────────────────────────────────────

    void amsqEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1293 — rx_am_squelch_on default = false
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.amsqEnabled(), false);
    }

    void setAmsqEnabledEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setAmsqEnabled(false);   // same as default → early return
        QCOMPARE(ch.amsqEnabled(), false);
    }

    // ── setFmsqEnabled ───────────────────────────────────────────────────────

    void fmsqEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1312 — rx_fm_squelch_on default = false
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.fmsqEnabled(), false);
    }

    void setFmsqEnabledEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setFmsqEnabled(false);   // same as default → early return
        QCOMPARE(ch.fmsqEnabled(), false);
    }
};

QTEST_MAIN(TestRxChannelSquelch)
#include "tst_rxchannel_squelch.moc"
