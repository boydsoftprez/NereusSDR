// tst_rxchannel_snb.cpp
//
// Verifies RxChannel SNB setter/accessor contracts without requiring a live
// WDSP channel.
//
// Strategy: only test paths where no WDSP API call is made:
//   1. Default-value assertions — accessor returns correct default.
//      The setter is never called, so WDSP is never invoked.
//   2. Same-value idempotency — setter early-returns when value equals the
//      currently stored atomic. WDSP call skipped by the early return guard.
//
// Source citations:
//   From Thetis Project Files/Source/Console/console.cs:36347 — SNB run flag
//   From Thetis Project Files/Source/Console/dsp.cs:692-693 — P/Invoke decl
//   WDSP: third_party/wdsp/src/snb.c:579

#include <QtTest/QtTest>
#include "core/RxChannel.h"

using namespace NereusSDR;

// Use a channel id that definitely has no WDSP channel allocated.
// All paths tested here must not invoke any WDSP API.
static constexpr int kTestChannel  = 99;  // Never opened via OpenChannel
static constexpr int kTestBufSize  = 1024;
static constexpr int kTestRate     = 48000;

class TestRxChannelSnb : public QObject {
    Q_OBJECT

private slots:
    // ── setSnbEnabled ────────────────────────────────────────────────────────

    void snbEnabledDefaultIsFalse() {
        // SNB off at startup — Thetis chkDSPNB2.Checked default = false
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        QCOMPARE(ch.snbEnabled(), false);
    }

    void setSnbEnabledEarlyReturnOnSameValue() {
        // Same value as default (false) — early return, no WDSP call.
        RxChannel ch(kTestChannel, kTestBufSize, kTestRate);
        ch.setSnbEnabled(false);   // same as default → early return
        QCOMPARE(ch.snbEnabled(), false);
    }
};

QTEST_MAIN(TestRxChannelSnb)
#include "tst_rxchannel_snb.moc"
