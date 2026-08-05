// =================================================================
// tst_tx_display_window.cpp — TX panadapter frequency window
//
// Covers TxAnalyzer::spanClipBins, the pure Thetis port that decides how
// much of the baseband the transmit analyzer emits:
//
//   specHPSDR.cs:762-775 [v2.10.3.15] — CalcSpectrum, whose results become
//   SetAnalyzer's fscLin / fscHin.
//
// Why it matters, from two bench sessions (J.J. Boyd, KG4VCF):
//   2026-08-04 — keying up put the TUNE tone at the wrong dial frequency
//   while the RF was correct. The analyzer emitted the whole +/-48 kHz
//   baseband, the pan kept its much wider receive window, and
//   SpectrumWidget stretched one across the other, so the axis lied.
//   2026-08-05 — with the window landing correctly the trace was still a
//   broad blob, because 8 kHz sliced out of an unclipped 96 kHz leaves
//   about a hundred real points to smear across a thousand pixels.
//
// Thetis avoids both by deriving analyzer span and display window from one
// zoom state (initAnalyzer), so its pixels always cover exactly what is on
// screen. This function is how we do the same.
//
// Pure, so unlike the rest of the transmit display it needs no WDSP channel
// and no bench.
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

    // ── WDSP must be left with bins to work with ───────────────────────────
    //
    // Bench 2026-08-05: the span clips are not applied to the raw FFT. WDSP
    // subtracts them from a span ALREADY reduced by the symmetric clip:
    //
    //   From wdsp/analyzer.c:1283 [TAPR v1.29]:
    //     a->pix_per_bin = (double)a->num_pixels /
    //       ((double)(a->num_stitch * (a->out_size - 1 - 2 * a->clip))
    //        - a->fsclipL - a->fsclipH - 1.0);
    //
    // Thetis sets `int sclip = 0;` in CalcSpectrum for exactly this reason
    // (specHPSDR.cs:776-777 [v2.10.3.15]), and TxAnalyzer does the same
    // whenever a window is set.
    //
    // What that buys is measured against the PIXELS, not against zero. On a
    // narrow window the 0.04 CLIP_FRACTION drives the surviving count
    // negative outright and the analyzer emits nothing at all; on the
    // shipped +/-4 kHz window it stays positive but collapses to a double
    // handful, which upsamples into the blob the bench reported. Both are
    // the same failure, so assert the useful property rather than the sign.
    void symmetricClipWouldStarveTheTransmitWindow()
    {
        constexpr int    fftSize   = 32768;
        constexpr double rateHz    = 96000.0;
        constexpr int    kPixels   = 1202;   // observed n_pix at the bench

        const auto [clipL, clipH] =
            TxAnalyzer::spanClipBins(-4000, 4000, rateHz, fftSize);

        auto surviving = [&](int sclip) {
            return static_cast<double>(fftSize - 1 - 2 * sclip)
                   - clipL - clipH - 1.0;
        };

        // What ships: comfortably more bins than pixels, so every pixel is
        // backed by real data.
        QVERIFY2(surviving(0) > kPixels,
                 qPrintable(QStringLiteral("only %1 bins for %2 pixels")
                                .arg(surviving(0)).arg(kPixels)));

        // What the 0.04 fraction would leave: nowhere near enough, which is
        // an upsampled trace no window or detector setting can sharpen.
        const int clipFraction = static_cast<int>(0.04 * fftSize);
        QVERIFY2(surviving(clipFraction) < kPixels / 4,
                 qPrintable(QStringLiteral("expected the 0.04 clip to starve "
                                           "the window; it left %1 bins")
                                .arg(surviving(clipFraction))));
    }

    // ── End to end: the shipped transmit window ────────────────────────────
    //
    // MainWindow clips the analyzer to Thetis's fixed +/-4 kHz transmit
    // window (display.cs:1284-1295 [v2.10.3.15]). Pin the arithmetic that
    // window depends on, since a clip pair that leaves too few bins is the
    // difference between a sharp trace and a blob.
    void shippedTransmitWindowLeavesOnePointPerPixel()
    {
        constexpr int    fftSize = 32768;
        constexpr double rateHz  = 96000.0;
        constexpr int    lowHz   = -4000;
        constexpr int    highHz  = +4000;

        const auto [clipL, clipH] =
            TxAnalyzer::spanClipBins(lowHz, highHz, rateHz, fftSize);
        const int surviving = fftSize - clipL - clipH;

        // A panadapter is order-1000 pixels wide. The surviving bins must
        // exceed that or the trace is upsampled and no window, detector or
        // averaging setting can sharpen it -- the 2026-08-05 "shoulders".
        QVERIFY2(surviving > 1500,
                 qPrintable(QStringLiteral("only %1 bins survive the transmit "
                                           "window; trace would be upsampled")
                                .arg(surviving)));

        // And the surviving span must actually be the window we asked for.
        const double binWidth = rateHz / fftSize;
        const double spanHz   = surviving * binWidth;
        QVERIFY2(std::abs(spanHz - 8000.0) < binWidth * 2.0,
                 qPrintable(QStringLiteral("surviving span %1 Hz, wanted 8000")
                                .arg(spanHz)));
    }
};

QTEST_APPLESS_MAIN(TestTxDisplayWindow)
#include "tst_tx_display_window.moc"
