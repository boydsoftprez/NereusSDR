// =================================================================
// tests/tst_tx_channel_cfc_display.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the TxChannel::getCfcDisplayCompression wrapper added in
// Phase 3M-3a-ii follow-up Batch 7.  Wraps WDSP::GetTXACFCOMPDisplayCompression
// (cfcomp.c:740-757 [v2.10.3.13]) — the live readback the parametric-EQ widget
// bar chart will pull at 50 ms cadence (Tasks 8 + 9 dialog rewrites).
//
// Test strategy: pin the validation paths (null buffer + too-small buffer)
// and the public constants (kCfcDisplayBinCount = 1025, kCfcDisplaySampleRateHz
// = 48000.0).  The "live data" path requires actual WDSP state and is
// integration-tested via the dialog (Tasks 8 + 9) rather than mocked here.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — New test for Phase 3M-3a-ii follow-up Batch 7:
//                 TxChannel::getCfcDisplayCompression validation arms +
//                 constant pinning.  J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis source cites are
// in TxChannel.h/cpp.

#include <QtTest/QtTest>

#include "core/TxChannel.h"

using namespace NereusSDR;

// WDSP TX channel ID — from Thetis cmaster.c:177-190 [v2.10.3.13].
static constexpr int kTxChannelId = 1;

class TestTxChannelCfcDisplay : public QObject {
    Q_OBJECT

private slots:

    // ── Validation: null buffer ─────────────────────────────────────────────
    //
    // Wrapper must reject nullptr without touching WDSP.  Returns false.

    void rejectsNullBuffer()
    {
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.getCfcDisplayCompression(nullptr, TxChannel::kCfcDisplayBinCount));
    }

    void rejectsNullBuffer_withZeroSize()
    {
        TxChannel ch(kTxChannelId);
        QVERIFY(!ch.getCfcDisplayCompression(nullptr, 0));
    }

    // ── Validation: too-small buffer ────────────────────────────────────────
    //
    // Wrapper must reject buffers smaller than kCfcDisplayBinCount without
    // touching WDSP (otherwise GetTXACFCOMPDisplayCompression would write
    // past the end of caller storage).  Returns false.

    void rejectsTooSmallBuffer()
    {
        TxChannel ch(kTxChannelId);
        double buf[10] = {};
        QVERIFY(!ch.getCfcDisplayCompression(buf, 10));
    }

    void rejectsZeroSizedBuffer()
    {
        TxChannel ch(kTxChannelId);
        double buf[1] = {};
        QVERIFY(!ch.getCfcDisplayCompression(buf, 0));
    }

    void rejectsOneShortOfFullSize()
    {
        TxChannel ch(kTxChannelId);
        // Allocate exactly kCfcDisplayBinCount-1 doubles and hand WDSP a
        // size argument one short of the required count.  Must reject.
        double buf[TxChannel::kCfcDisplayBinCount - 1] = {};
        QVERIFY(!ch.getCfcDisplayCompression(buf, TxChannel::kCfcDisplayBinCount - 1));
    }

    // ── Constant pinning ────────────────────────────────────────────────────
    //
    // Pin the two public constants so a future Thetis fsize change (or TX
    // dsp_rate change) trips the regression here instead of silently
    // corrupting the bar-chart bin layout downstream.
    //
    // From Thetis cfcomp.c:379 [v2.10.3.13] — msize = fsize/2+1 with
    //                                          fsize=2048 (TXA.c:209).
    // From Thetis frmCFCConfig.cs:411 [v2.10.3.13] — TX dsp_rate=96000,
    //                                                Nyquist=48 kHz.

    void publicConstantsMatchThetisFsize()
    {
        QCOMPARE(TxChannel::kCfcDisplayBinCount, 1025);
        QCOMPARE(TxChannel::kCfcDisplaySampleRateHz, 48000.0);
    }
};

QTEST_MAIN(TestTxChannelCfcDisplay)
#include "tst_tx_channel_cfc_display.moc"
