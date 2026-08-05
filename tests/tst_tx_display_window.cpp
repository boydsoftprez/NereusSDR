// =================================================================
// tst_tx_display_window.cpp — TX panadapter frequency window
//
// Covers the two pure Thetis ports behind the transmit display's
// frequency axis:
//
//   TxAnalyzer::txDisplayWindowHz  <- console.cs:8024-8056  [v2.10.3.15]
//                                     UpdateTXDisplayVars
//   TxAnalyzer::spanClipBins       <- specHPSDR.cs:762-775  [v2.10.3.15]
//                                     CalcSpectrum
//
// Why these matter, from the bench on 2026-08-04 (J.J. Boyd, KG4VCF):
// keying up put the TUNE tone at the wrong dial frequency while the RF
// left the radio on the correct one. The analyzer was emitting the whole
// +/-48 kHz baseband with no span clip, the pan kept its much wider RX
// window, and SpectrumWidget stretched the one across the other -- so the
// frequency axis lied. Thetis avoids this by deriving a narrow window from
// the TX filter edges and clipping the analyzer to exactly that window.
// These two functions are that derivation.
//
// Both are pure and need no WDSP channel, so unlike most of the TX display
// they are fully testable here rather than only at a bench.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-04 — New test for the TX display window ports.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file. Upstream cites live in
// TxAnalyzer.h / TxAnalyzer.cpp next to the ported code.

#include <QtTest/QtTest>

#include "core/TxAnalyzer.h"

using namespace NereusSDR;

class TestTxDisplayWindow : public QObject {
    Q_OBJECT

private slots:

    // ── UpdateTXDisplayVars: the three sign cases ──────────────────────────

    void usbFilterGivesCarrierUpwardWindow()
    {
        // USB in IQ space is entirely positive, e.g. [+150, +2850].
        // Thetis: low = 0, high = 1.1 * h.
        const auto [low, high] = TxAnalyzer::txDisplayWindowHz(150, 2850);
        QCOMPARE(low, 0);
        QCOMPARE(high, static_cast<int>(2850 * 1.1));  // 3135
    }

    void lsbFilterGivesCarrierDownwardWindow()
    {
        // LSB is the mirror: [-2850, -150] -> high = 0, low = 1.1 * l.
        const auto [low, high] = TxAnalyzer::txDisplayWindowHz(-2850, -150);
        QCOMPARE(high, 0);
        QCOMPARE(low, static_cast<int>(-2850 * 1.1));  // -3135
    }

    void straddlingFilterIsSymmetricOnTheWiderEdge()
    {
        // AM / DSB straddle zero. Thetis takes the WIDER edge and mirrors
        // it, so an asymmetric filter still yields a symmetric window.
        const auto [low, high] = TxAnalyzer::txDisplayWindowHz(-1000, 2850);
        QCOMPARE(high, static_cast<int>(2850 * 1.1));
        QCOMPARE(low, static_cast<int>(2850 * -1.1));
        QCOMPARE(low, -high);
    }

    // ── The 910 Hz floor ───────────────────────────────────────────────────
    //
    // A narrow filter (CW) would otherwise collapse the window to a few
    // hundred Hz and leave nothing to look at. Thetis floors it at 1 kHz.

    void narrowUsbFilterIsFlooredAtOneKilohertz()
    {
        // h = 500 is under the 910 threshold, so the 1.1 factor does NOT
        // apply and the window opens to a flat 1000 Hz.
        const auto [low, high] = TxAnalyzer::txDisplayWindowHz(0, 500);
        QCOMPARE(low, 0);
        QCOMPARE(high, 1000);
    }

    void narrowLsbFilterIsFlooredAtOneKilohertz()
    {
        const auto [low, high] = TxAnalyzer::txDisplayWindowHz(-500, 0);
        QCOMPARE(high, 0);
        QCOMPARE(low, -1000);
    }

    void justOverTheFloorUsesTheScaleFactorInstead()
    {
        // 911 is the first value above the threshold, so 1.1 applies and
        // the result must NOT be the flat 1000. Pins the boundary in the
        // right place: an off-by-one here would silently widen every CW
        // window.
        const auto [low, high] = TxAnalyzer::txDisplayWindowHz(0, 911);
        QCOMPARE(low, 0);
        QCOMPARE(high, static_cast<int>(911 * 1.1));  // 1002
        QVERIFY(high != 1000);
    }

    void invertedFilterYieldsNoWindow()
    {
        // l >= 0 with h <= 0 matches none of Thetis's three branches, so
        // its `int low = 0, high = 0;` initialisation survives untouched.
        const auto [low, high] = TxAnalyzer::txDisplayWindowHz(100, -100);
        QCOMPARE(low, 0);
        QCOMPARE(high, 0);
    }

    // ── CalcSpectrum: window to bin clips ──────────────────────────────────

    void usbWindowClipsEverythingBelowCarrier()
    {
        // 96 kHz over 4096 bins = 23.4375 Hz per bin.
        // low_clip_bw  = 48000 + 0    = 48000 -> ceil (48000/23.4375) = 2048
        // high_clip_bw = 48000 - 3135 = 44865 -> floor(44865/23.4375) = 1914
        const auto [clipL, clipH] =
            TxAnalyzer::spanClipBins(0, 3135, 96000.0, 4096);
        QCOMPARE(clipL, 2048);
        QCOMPARE(clipH, 1914);

        // What survives must cover the requested window, not a fraction of
        // it -- this is the number that decides where the tone lands.
        const int surviving = 4096 - clipL - clipH;
        const double binWidth = 96000.0 / 4096.0;
        const double spanHz = surviving * binWidth;
        QVERIFY2(std::abs(spanHz - 3135.0) < binWidth * 2.0,
                 qPrintable(QStringLiteral("surviving span %1 Hz, wanted ~3135")
                                .arg(spanHz)));
    }

    void lsbWindowClipsEverythingAboveCarrier()
    {
        // Mirror image: the low clip should now be the small one.
        const auto [clipL, clipH] =
            TxAnalyzer::spanClipBins(-3135, 0, 96000.0, 4096);
        QCOMPARE(clipH, 2048);
        QCOMPARE(clipL, 1915);   // ceil(44865/23.4375), vs floor on the far side
    }

    // The floor/ceil asymmetry is Thetis's, and it is load-bearing: using
    // the same rounding on both ends shifts the surviving span by a bin,
    // which is a real frequency error on a 3 kHz window. Pin it.
    void lowSideCeilsWhileHighSideFloors()
    {
        const auto [clipL, clipH] =
            TxAnalyzer::spanClipBins(-3135, 0, 96000.0, 4096);
        // 44865 / 23.4375 = 1914.24 -> ceil 1915 on the low side...
        QCOMPARE(clipL, 1915);
        const auto [clipL2, clipH2] =
            TxAnalyzer::spanClipBins(0, 3135, 96000.0, 4096);
        // ...and floor 1914 for the identical bandwidth on the high side.
        QCOMPARE(clipH2, 1914);
        QVERIFY(clipL != clipH2);
    }

    // ── Degenerate inputs ──────────────────────────────────────────────────
    //
    // A clip pair that consumes every bin makes the analyzer emit nothing,
    // and a blank pan is indistinguishable at the bench from the frozen
    // waterfall this work exists to fix. Fall back to no clipping instead.

    void windowWiderThanBasebandFallsBackToFullSpan()
    {
        const auto [clipL, clipH] =
            TxAnalyzer::spanClipBins(-90000, 90000, 96000.0, 4096);
        QCOMPARE(clipL, 0);
        QCOMPARE(clipH, 0);
    }

    void zeroWidthWindowFallsBackToFullSpan()
    {
        // low == high leaves no bins; clipping to that would blank the pan.
        const auto [clipL, clipH] =
            TxAnalyzer::spanClipBins(0, 0, 96000.0, 4096);
        QCOMPARE(clipL, 0);
        QCOMPARE(clipH, 0);
    }

    void invalidRateOrSizeIsRefused()
    {
        QCOMPARE(TxAnalyzer::spanClipBins(0, 3135, 0.0, 4096).first, 0);
        QCOMPARE(TxAnalyzer::spanClipBins(0, 3135, 96000.0, 0).second, 0);
    }

    // ── End to end: the bench case ─────────────────────────────────────────

    void benchUsbFilterProducesAWindowMatchingTheFilter()
    {
        // The filter the bench was running: TX BW 100..2900 Hz, USB.
        const auto [low, high] = TxAnalyzer::txDisplayWindowHz(100, 2900);
        const auto [clipL, clipH] =
            TxAnalyzer::spanClipBins(low, high, 96000.0, 4096);

        const double binWidth  = 96000.0 / 4096.0;
        const int    surviving = 4096 - clipL - clipH;

        // The pan must end up showing roughly 3.2 kHz starting at the
        // carrier, NOT the 96 kHz it was showing when the tone appeared at
        // the wrong dial frequency.
        QVERIFY2(surviving * binWidth < 4000.0,
                 "TX window is still wide enough to mis-place the trace");
        QCOMPARE(low, 0);
        QVERIFY(high > 2900);        // 1.1 factor leaves the skirt visible
        QVERIFY(high < 3300);
    }
};

QTEST_APPLESS_MAIN(TestTxDisplayWindow)
#include "tst_tx_display_window.moc"
