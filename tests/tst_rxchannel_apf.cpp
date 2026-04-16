// tst_rxchannel_apf.cpp
//
// Verifies RxChannel APF (Audio Peak Filter) setter/accessor contracts without
// requiring a live WDSP channel.
//
// Strategy: only test paths where no WDSP API call is made:
//   1. Default-value assertions — accessor returns Thetis-sourced default.
//      The setter is never called, so WDSP is never invoked.
//   2. Same-value idempotency — setter early-returns when value equals the
//      currently stored atomic. WDSP call skipped by the early return guard.
//
// Source citations:
//   From Thetis Project Files/Source/Console/radio.cs:1910-1927 — APF run flag
//   WDSP: third_party/wdsp/src/apfshadow.c:93

#include <QtTest/QtTest>
#include "core/RxChannel.h"

using namespace NereusSDR;

// Use a channel id that definitely has no WDSP channel allocated.
// All paths tested here must not invoke any WDSP API.
static constexpr int kTestChannel  = 99;  // Never opened via OpenChannel
static constexpr int kTestBufSize  = 1024;
static constexpr int kTestRate     = 48000;

class TestRxChannelApf : public QObject {
    Q_OBJECT

private slots:
    // ── setApfEnabled ────────────────────────────────────────────────────────

    void apfEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1910 — rx_apf_run default = false
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.apfEnabled(), false);
    }

    void setApfEnabledEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setApfEnabled(false);   // same as default → early return
        QCOMPARE(ch.apfEnabled(), false);
    }
};

QTEST_MAIN(TestRxChannelApf)
#include "tst_rxchannel_apf.moc"
