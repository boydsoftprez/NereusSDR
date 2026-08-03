// =================================================================
// tests/tst_spectrum_reducer.cpp  (NereusSDR)
// =================================================================
//
// R1 Task 5 -- crop-and-reduce extracted out of SpectrumWidget into
// core/spectrum/SpectrumReducer.  These tests pin the arithmetic that
// used to live in SpectrumWidget::visibleBinRange() and the reduction
// section of SpectrumWidget::updateSpectrumLinear().
//
// The five tests the R1 plan specified are marked "[plan]".  The rest
// were added after reading the implementation being extracted: each
// pins a guard or a clamping rule the five did not cover, and every
// one of those is a path where dropping the guard is a crash or a
// silent display change rather than a test failure.
//
// Expectation corrections against the plan text are called out inline
// where they occur (halfSpanSelectsMiddleHalf).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 3 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <cmath>

#include "core/spectrum/SpectrumReducer.h"

using namespace NereusSDR;

namespace {

// Stream: 192 kHz wide, centred on 14.200 MHz.  4096 bins => 46.875 Hz
// per bin, so every boundary in these tests lands on an exact bin edge
// and the floor/ceil convention is observable rather than masked by
// floating-point slop.
ReducerConfig baseConfig()
{
    ReducerConfig c;
    c.centreHz       = 14'200'000.0;
    c.spanHz         =    192'000.0;
    c.streamCentreHz = 14'200'000.0;
    c.sampleRateHz   =    192'000.0;
    return c;
}

QVector<float> flatBins(int n, float v)
{
    return QVector<float>(n, v);
}

}  // namespace

class TstSpectrumReducer : public QObject
{
    Q_OBJECT

private slots:

    // ---- [plan] the five tests the R1 plan specified ------------------

    // Design section 9.4: the pixel count is a bandwidth control, and must
    // come from config alone.  No widget geometry may influence it.
    void outputSizeAlwaysMatchesRequestedPixels()
    {
        SpectrumReducer r;
        QVector<float> out;
        for (int px : {128, 512, 1024, 1184}) {
            ReducerConfig c = baseConfig();
            c.pixels = px;
            r.setConfig(c);
            const QVector<float> bins = flatBins(4096, 1.0e-9f);
            r.reduce(bins, 1.0, 0.0, out);
            const qsizetype got = out.size();
            QVERIFY2(got == px,
                     qPrintable(QStringLiteral("pixels=%1 produced %2")
                                    .arg(px)
                                    .arg(got)));
        }
    }

    // Full-span crop must select every bin.
    void fullSpanSelectsAllBins()
    {
        const ReducerConfig c = baseConfig();
        const auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QCOMPARE(first, 0);
        QCOMPARE(last, 4095);
    }

    // A centred half-span crop must select the middle half.
    //
    // EXPECTATION CORRECTED against the R1 plan, which asserted last==3071.
    // SpectrumWidget::visibleBinRange takes ceil() of the high edge:
    //
    //   binWidth  = 192000 / 4096                     = 46.875 Hz
    //   fftLowHz  = 14200000 - 96000                  = 14104000
    //   highHz    = 14200000 + 48000                  = 14248000
    //   lastBin   = ceil((14248000 - 14104000) / 46.875)
    //             = ceil(3072.0)                      = 3072
    //
    // Bin 3071 is the last bin whose span ends at the window edge, so a
    // half-open reading gives 3071; the shipped code is deliberately
    // inclusive (floor on the low edge, ceil on the high edge) so the
    // crop can never under-cover the requested window.  Per the R1 task
    // rule, the shipped arithmetic wins and the plan's number was wrong.
    void halfSpanSelectsMiddleHalf()
    {
        ReducerConfig c = baseConfig();
        c.spanHz = 96'000.0;
        const auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QCOMPARE(first, 1024);
        QCOMPARE(last, 3072);
    }

    // The floor/ceil convention itself, which halfSpanSelectsMiddleHalf
    // cannot see: at 192 kHz over 4096 bins every edge in that test lands
    // exactly on a bin boundary, where floor and ceil agree, so swapping
    // one for the other leaves it green.  Nudging the span by 1 Hz puts
    // both edges strictly inside a bin:
    //
    //   low  edge -> 1023.989333  floor 1023  (ceil would say 1024)
    //   high edge -> 3072.010667  ceil  3073  (floor would say 3072)
    //
    // Outward rounding on both edges is what guarantees the crop always
    // covers the requested window rather than falling one bin short of it.
    void cropRoundsOutwardOnBothEdges()
    {
        ReducerConfig c = baseConfig();
        c.spanHz = 96'001.0;
        const auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QCOMPARE(first, 1023);
        QCOMPARE(last, 3073);
    }

    // Off-centre crops must not walk off the end of the bin array.
    void cropIsClampedToRange()
    {
        ReducerConfig c = baseConfig();
        c.centreHz = 14'290'000.0;
        const auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QVERIFY(first >= 0);
        QVERIFY(last <= 4095);
        QVERIFY(first <= last);
        // Exact values, so a future change to the clamp is visible rather
        // than merely still-in-bounds.
        QCOMPARE(first, 1920);
        QCOMPARE(last, 4095);
    }

    // A single strong bin must dominate its pixel under Peak detection.
    void peakDetectorFindsTheTone()
    {
        SpectrumReducer r;
        ReducerConfig c = baseConfig();
        c.pixels   = 256;
        c.detector = SpectrumDetectorMode::Peak;
        r.setConfig(c);
        QVector<float> bins = flatBins(4096, 1.0e-12f);
        bins[2048] = 1.0f;
        QVector<float> out;
        r.reduce(bins, 1.0, 0.0, out);
        int argmax = 0;
        for (int i = 1; i < out.size(); ++i) {
            if (out[i] > out[argmax]) { argmax = i; }
        }
        QCOMPARE(argmax, 128);
    }

    // ---- crop-window edge rules the five did not pin ------------------

    // visibleBinRange's first guard.  A reducer configured before the
    // stream is known (sampleRateHz still 0) must report an empty range,
    // NOT a range that a caller would then use to index the bin array.
    // The widget relies on this to bail via "sliceCount <= 0".
    void unconfiguredSampleRateYieldsEmptyRange()
    {
        ReducerConfig c = baseConfig();
        c.sampleRateHz = 0.0;
        const auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QCOMPARE(first, 0);
        QCOMPARE(last, -1);
        QVERIFY(last - first + 1 <= 0);   // the widget's bail condition
    }

    // Same guard, other half: an empty bin array.
    void zeroBinCountYieldsEmptyRange()
    {
        const ReducerConfig c = baseConfig();
        const auto [first, last] = SpectrumReducer::visibleBinRange(0, c);
        QCOMPARE(first, 0);
        QCOMPARE(last, -1);
    }

    // Crop entirely below the stream: both edges clamp to 0, and the
    // "firstBin > lastBin -> firstBin = lastBin" collapse keeps the range
    // one bin wide rather than inverted.
    void cropEntirelyBelowStreamCollapsesToFirstBin()
    {
        ReducerConfig c = baseConfig();
        c.centreHz = 13'000'000.0;
        const auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QCOMPARE(first, 0);
        QCOMPARE(last, 0);
    }

    // Crop entirely above the stream: both edges clamp to binCount-1.
    void cropEntirelyAboveStreamCollapsesToLastBin()
    {
        ReducerConfig c = baseConfig();
        c.centreHz = 15'000'000.0;
        const auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QCOMPARE(first, 4095);
        QCOMPARE(last, 4095);
    }

    // Zero span is reachable from the widget (bandwidth 0 before the first
    // setFrequencyRange).  floor and ceil of the same edge agree, so the
    // range is exactly one bin -- non-empty, so the pipeline still runs.
    void zeroSpanSelectsExactlyOneBin()
    {
        ReducerConfig c = baseConfig();
        c.spanHz = 0.0;
        const auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QCOMPARE(first, 2048);
        QCOMPARE(last, 2048);
    }

    // A span wider than the stream must clamp to the stream, not scale
    // the crop.  This is the zoomed-all-the-way-out case.
    void spanWiderThanStreamClampsToWholeStream()
    {
        ReducerConfig c = baseConfig();
        c.spanHz = 768'000.0;
        const auto [first, last] = SpectrumReducer::visibleBinRange(4096, c);
        QCOMPARE(first, 0);
        QCOMPARE(last, 4095);
    }

    // ---- reduce() degenerate-input guards ----------------------------
    //
    // updateSpectrumLinear returns early on empty bins and on an empty
    // crop, leaving the previously rendered frame on screen.  The reducer
    // keeps that semantic: a degenerate call is a no-op, not a wipe.

    void emptyBinsLeavesPreviousOutputUntouched()
    {
        SpectrumReducer r;
        ReducerConfig c = baseConfig();
        c.pixels = 64;
        r.setConfig(c);

        QVector<float> out;
        r.reduce(flatBins(4096, 1.0e-6f), 1.0, 0.0, out);
        QCOMPARE(out.size(), 64);
        const QVector<float> snapshot = out;

        r.reduce(QVector<float>{}, 1.0, 0.0, out);
        QCOMPARE(out, snapshot);
    }

    void emptyCropLeavesPreviousOutputUntouched()
    {
        SpectrumReducer r;
        ReducerConfig c = baseConfig();
        c.pixels = 64;
        r.setConfig(c);
        QVector<float> out;
        r.reduce(flatBins(4096, 1.0e-6f), 1.0, 0.0, out);
        const QVector<float> snapshot = out;

        c.sampleRateHz = 0.0;          // empty range -> sliceCount <= 0
        r.setConfig(c);
        r.reduce(flatBins(4096, 1.0e-3f), 1.0, 0.0, out);
        QCOMPARE(out, snapshot);
    }

    // pixels is caller-supplied now, so a non-positive value is reachable
    // in a way it never was from widget geometry (which floors at 800).
    // Must be a no-op, not a negative QVector::resize.
    void nonPositivePixelCountIsANoOp()
    {
        SpectrumReducer r;
        ReducerConfig c = baseConfig();
        c.pixels = 0;
        r.setConfig(c);
        QVector<float> out;
        r.reduce(flatBins(4096, 1.0e-6f), 1.0, 0.0, out);
        QVERIFY(out.isEmpty());

        c.pixels = -7;
        r.setConfig(c);
        r.reduce(flatBins(4096, 1.0e-6f), 1.0, 0.0, out);
        QVERIFY(out.isEmpty());
    }

    // windowEnb arrives from FFTEngine every frame; the widget clamps it
    // with qMax(windowEnb, 1e-9) before taking the reciprocal.  Drop the
    // clamp and Average/Sample/RMS detection divides by zero.
    void zeroWindowEnbDoesNotProduceNonFiniteOutput()
    {
        SpectrumReducer r;
        ReducerConfig c = baseConfig();
        c.pixels   = 128;
        c.detector = SpectrumDetectorMode::Average;
        r.setConfig(c);
        QVector<float> out;
        r.reduce(flatBins(4096, 1.0e-9f), 0.0, 0.0, out);
        QCOMPARE(out.size(), 128);
        for (int i = 0; i < out.size(); ++i) {
            QVERIFY2(std::isfinite(out[i]),
                     qPrintable(QStringLiteral("pixel %1 not finite").arg(i)));
        }
    }

    // ---- config plumbing ---------------------------------------------

    void configRoundTrips()
    {
        SpectrumReducer r;
        ReducerConfig c = baseConfig();
        c.pixels      = 333;
        c.detector    = SpectrumDetectorMode::RMS;
        c.averageMode = 3;
        c.averageTau  = 0.42;
        r.setConfig(c);
        QCOMPARE(r.config().pixels, 333);
        QCOMPARE(r.config().detector, SpectrumDetectorMode::RMS);
        QCOMPARE(r.config().averageMode, 3);
        QCOMPARE(r.config().averageTau, 0.42);
        QCOMPARE(r.config().centreHz, 14'200'000.0);
    }

    // The detector field must actually reach applySpectrumDetector.  A
    // hardcoded Peak would still satisfy peakDetectorFindsTheTone, so
    // prove a different mode gives a different answer: with one hot bin
    // per pixel window, Peak reports the tone and Average reports the
    // mean, which is 16x smaller (12 dB down) for binPerPix = 16.
    void detectorModeReachesTheDetector()
    {
        QVector<float> bins = flatBins(4096, 0.0f);
        for (int i = 0; i < 4096; i += 16) { bins[i] = 1.0e-6f; }

        ReducerConfig c = baseConfig();
        c.pixels = 256;

        QVector<float> peakOut;
        SpectrumReducer peak;
        c.detector = SpectrumDetectorMode::Peak;
        peak.setConfig(c);
        peak.reduce(bins, 1.0, 0.0, peakOut);
        const float peakDb = peakOut[128];

        QVector<float> avgOut;
        SpectrumReducer avg;
        c.detector = SpectrumDetectorMode::Average;
        avg.setConfig(c);
        avg.reduce(bins, 1.0, 0.0, avgOut);
        const float avgDb = avgOut[128];

        QVERIFY2(peakDb > avgDb + 10.0f,
                 qPrintable(QStringLiteral("peak %1 dB vs average %2 dB")
                                .arg(static_cast<double>(peakDb))
                                .arg(static_cast<double>(avgDb))));
    }

    // dbmOffset is folded into the avenger's power-domain scale so that
    // 10*log10(linear * scale) == 10*log10(linear) + dbmOffset.  Verify
    // the identity end to end, because a scale applied in the wrong
    // domain still looks plausible on screen.
    void dbmOffsetShiftsOutputByExactlyTheOffset()
    {
        SpectrumReducer r;
        ReducerConfig c = baseConfig();
        c.pixels = 64;
        r.setConfig(c);

        const QVector<float> bins = flatBins(4096, 1.0e-9f);
        QVector<float> zero;
        QVector<float> minus;
        r.reduce(bins, 1.0,   0.0, zero);
        r.clearAveraging();
        r.reduce(bins, 1.0, -12.5, minus);

        QCOMPARE(zero.size(), minus.size());
        for (int i = 0; i < zero.size(); ++i) {
            QVERIFY2(std::abs((zero[i] - 12.5f) - minus[i]) < 1.0e-3f,
                     qPrintable(QStringLiteral("pixel %1: %2 vs %3")
                                    .arg(i)
                                    .arg(static_cast<double>(zero[i]))
                                    .arg(static_cast<double>(minus[i]))));
        }
    }

    // ---- averaging state ---------------------------------------------

    // The reducer owns the avenger, so it owns per-frame state.  Log-
    // recursive averaging must converge across frames (proving state is
    // carried), and clearAveraging() must reset it (proving the widget's
    // FFT-replan and mode-change resets still have a lever).
    void averagingIsStatefulAndClearResetsIt()
    {
        SpectrumReducer r;
        ReducerConfig c = baseConfig();
        c.pixels      = 32;
        c.averageMode = 3;      // log recursive
        c.averageTau  = 0.9;    // heavy smoothing so convergence is slow
        r.setConfig(c);

        const QVector<float> quiet = flatBins(4096, 1.0e-12f);
        const QVector<float> loud  = flatBins(4096, 1.0e-6f);

        QVector<float> out;
        for (int i = 0; i < 20; ++i) { r.reduce(quiet, 1.0, 0.0, out); }
        r.reduce(quiet, 1.0, 0.0, out);
        const float settled = out[16];

        r.reduce(loud, 1.0, 0.0, out);
        const float firstLoud = out[16];
        QVERIFY2(firstLoud < settled + 55.0f,
                 "smoothing should hold the first loud frame well below its "
                 "un-averaged value");

        r.clearAveraging();
        r.reduce(loud, 1.0, 0.0, out);
        const float afterClear = out[16];
        QVERIFY2(afterClear > firstLoud,
                 "clearAveraging must drop the accumulated history");
    }

    // Changing the pixel count must re-size the avenger, which resets its
    // accumulators.  This is the resize path updateSpectrumLinear runs on
    // every widget resize; getting it wrong reads past the old array.
    void pixelCountChangeResizesEverything()
    {
        SpectrumReducer r;
        ReducerConfig c = baseConfig();
        c.pixels      = 64;
        c.averageMode = 1;
        c.averageTau  = 0.5;
        r.setConfig(c);

        const QVector<float> bins = flatBins(4096, 1.0e-9f);
        QVector<float> out;
        for (int i = 0; i < 5; ++i) { r.reduce(bins, 1.0, 0.0, out); }
        r.reduce(bins, 1.0, 0.0, out);
        QCOMPARE(out.size(), 64);

        c.pixels = 1024;
        r.setConfig(c);
        r.reduce(bins, 1.0, 0.0, out);
        QCOMPARE(out.size(), 1024);
        for (int i = 0; i < out.size(); ++i) {
            QVERIFY(std::isfinite(out[i]));
        }

        c.pixels = 16;
        r.setConfig(c);
        r.reduce(bins, 1.0, 0.0, out);
        QCOMPARE(out.size(), 16);
    }

    // Cropping must read from inside the bin array for every window the
    // range function can return.  Run a sweep with an ASAN-visible access
    // pattern: any off-by-one in firstBin + sliceCount reads past the end.
    void croppedReduceStaysInsideTheBinArray()
    {
        SpectrumReducer r;
        QVector<float> out;
        for (double centre : {13'000'000.0, 14'150'000.0, 14'200'000.0,
                              14'260'000.0, 15'000'000.0}) {
            for (double span : {0.0, 1'000.0, 96'000.0, 192'000.0, 768'000.0}) {
                for (int pixels : {1, 7, 800, 4096, 9000}) {
                    ReducerConfig c = baseConfig();
                    c.centreHz = centre;
                    c.spanHz   = span;
                    c.pixels   = pixels;
                    r.setConfig(c);
                    r.reduce(flatBins(4096, 1.0e-9f), 2.0, -10.0, out);
                    QCOMPARE(out.size(), pixels);
                }
            }
        }
    }

    // ---- allocation behaviour on the render hot path ------------------

    // The regression this out-parameter shape exists to prevent.  reduce()
    // used to return a const reference to an internal QVector, and the
    // widget assigned it: implicit sharing then put both handles on one
    // data block, and the avenger's non-const pixelsOut[i] writes detached
    // it on the very next frame.  That is one malloc plus one full-buffer
    // memcpy per plane per frame -- on a display path that runs at 30 fps
    // and, per the R1 design, on a Pi 4B that is memory-bandwidth bound.
    //
    // Once the buffer has settled at its final size, the data pointer must
    // not move again no matter how many frames go through.
    void steadyStateReduceDoesNotReallocate()
    {
        SpectrumReducer r;
        ReducerConfig c = baseConfig();
        c.pixels      = 1024;
        c.averageMode = 3;
        r.setConfig(c);

        const QVector<float> bins = flatBins(4096, 1.0e-9f);
        QVector<float> out;

        // Two warm-up frames: the first sizes the buffer, the second proves
        // the size has settled before the pointer is latched.
        r.reduce(bins, 1.0, 0.0, out);
        r.reduce(bins, 1.0, 0.0, out);
        QCOMPARE(out.size(), 1024);

        const float* const settled = out.constData();
        for (int f = 0; f < 60; ++f) {
            r.reduce(bins, 1.0, 0.0, out);
            QVERIFY2(out.constData() == settled,
                     qPrintable(QStringLiteral(
                         "frame %1 reallocated the output buffer; reduce() is "
                         "detaching a shared QVector every frame").arg(f)));
        }
    }
};

QTEST_MAIN(TstSpectrumReducer)
#include "tst_spectrum_reducer.moc"
