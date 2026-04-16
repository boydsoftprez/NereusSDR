// tst_rxchannel_emnr.cpp
//
// Verifies RxChannel EMNR (NR2) setter/accessor contracts without
// requiring a live WDSP channel.
//
// Strategy: only test paths where no WDSP API call is made:
//   1. Default-value assertions — accessor returns Thetis-sourced default.
//      The setter is never called, so WDSP is never invoked.
//   2. Same-value idempotency — setter early-returns when value equals the
//      currently stored atomic. WDSP call skipped by the early return guard.
//
// Source citations:
//   From Thetis Project Files/Source/Console/radio.cs:2216-2232 — EMNR run flag
//   WDSP: third_party/wdsp/src/emnr.c:1283

#include <QtTest/QtTest>
#include "core/RxChannel.h"

using namespace NereusSDR;

// Use a channel id that definitely has no WDSP channel allocated.
// All paths tested here must not invoke any WDSP API.
static constexpr int kTestChannel  = 99;  // Never opened via OpenChannel
static constexpr int kTestBufSize  = 1024;
static constexpr int kTestRate     = 48000;

class TestRxChannelEmnr : public QObject {
    Q_OBJECT

private slots:
    // ── setEmnrEnabled ───────────────────────────────────────────────────────

    void emnrEnabledDefaultIsFalse() {
        // From Thetis radio.cs:2216 — rx_nr2_run default = 0
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.emnrEnabled(), false);
    }

    void setEmnrEnabledEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setEmnrEnabled(false);   // same as default → early return
        QCOMPARE(ch.emnrEnabled(), false);
    }
};

QTEST_MAIN(TestRxChannelEmnr)
#include "tst_rxchannel_emnr.moc"
