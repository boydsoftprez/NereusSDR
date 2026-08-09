// =================================================================
// tests/tst_extended_pan_wings.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic F Tasks 8 + 9, finished 2026-08-08 after a bench report
// that "the extendview wide wings is not working".
//
// The data half of Sub-Epic F shipped in 2026-05: P2 wideband packets ->
// WidebandFrameAccumulator -> WidebandFftEngine -> SpectrumWidget::
// setWidebandBins. The render half never did. setWidebandBins stored the
// bins and said so ("No update() call: paint is gated behind m_extendedMode
// (wired in F polish)"), and recomputeExtendedMode said the paint hook was
// "deferred to a post-bench polish iteration".
//
// Two defects follow from that, and both are covered here:
//   1. visibleBinRange() CLAMPS to the DDC's bin array, so zooming a pan past
//      its DDC rate stretched the DDC spectrum across the whole panel instead
//      of confining it to the span it actually covers. The trace stopped
//      agreeing with the frequency scale drawn under it.
//   2. Nothing ever read m_widebandBinsAdc0/1 back out, so the wings were
//      empty no matter how much wideband data arrived.
// =================================================================
#include <QtTest/QtTest>
#include <cmath>

#include "gui/SpectrumWidget.h"
#include "core/WidebandFftEngine.h"

using namespace NereusSDR;

class TestExtendedPanWings : public QObject {
    Q_OBJECT
private:
    // A pan zoomed out past its DDC: 192 kHz of DDC inside a 1.92 MHz window,
    // both centred on the same frequency, so the island should land dead
    // centre and occupy a tenth of the width.
    static void configureExtendedPan(SpectrumWidget& sw)
    {
        sw.setExtendedViewAllowed(true);
        sw.setSampleRate(192000.0);
        sw.setDdcCenterFrequency(14200000.0);
        sw.setFrequencyRange(14200000.0, 1920000.0);
    }

private slots:
    // Extended mode is permission plus need: the toggle alone must not put a
    // pan into it at ordinary zoom, or every pan would ask the radio for a
    // wideband stream it has no use for.
    void extended_mode_needs_zoom_beyond_the_ddc()
    {
        SpectrumWidget sw;
        sw.setExtendedViewAllowed(true);
        sw.setSampleRate(192000.0);
        sw.setDdcCenterFrequency(14200000.0);
        
        sw.setFrequencyRange(14200000.0, 96000.0);   // inside the DDC
        QVERIFY(!sw.extendedMode());

        sw.setFrequencyRange(14200000.0, 1920000.0);  // 10x past it
        QVERIFY(sw.extendedMode());

        sw.setExtendedViewAllowed(false);   // operator override wins
        QVERIFY(!sw.extendedMode());
    }

    // Defect 1. Outside extended mode the island must be the whole panel, or
    // every ordinary pan would silently change shape.
    void island_is_the_whole_panel_when_not_extended()
    {
        SpectrumWidget sw;
        sw.setExtendedViewAllowed(true);
        sw.setSampleRate(192000.0);
        sw.setDdcCenterFrequency(14200000.0);
        sw.setFrequencyRange(14200000.0, 96000.0);
        QVERIFY(!sw.extendedMode());

        const auto [first, last] = sw.listenableIslandPixels(1000);
        QCOMPARE(first, 0);
        QCOMPARE(last, 999);
    }

    // Defect 1, the real case: 192 kHz of DDC centred in a 1.92 MHz window is
    // a tenth of the width, centred. Before the fix the DDC covered all 1000
    // pixels, i.e. it claimed ten times the spectrum it had.
    void island_covers_only_the_ddc_span_when_extended()
    {
        SpectrumWidget sw;
        configureExtendedPan(sw);
        QVERIFY(sw.extendedMode());

        const int width = 1000;
        const auto [first, last] = sw.listenableIslandPixels(width);

        // 1/10th of 1000 px centred would be 450..549, less the filter
        // skirt clipped off each edge (see
        // island_excludes_the_ddc_filter_skirt), which pulls both edges in
        // by 4% of the island's own width.
        const double inset = 100.0 * SpectrumWidget::kDdcClipFraction;
        QVERIFY2(std::abs(first - (450.0 + inset)) <= 1.5,
                 qPrintable(QStringLiteral("island starts at %1, expected ~%2")
                                .arg(first).arg(450.0 + inset)));
        QVERIFY2(std::abs(last - (549.0 - inset)) <= 1.5,
                 qPrintable(QStringLiteral("island ends at %1, expected ~%2")
                                .arg(last).arg(549.0 - inset)));
        QVERIFY2(last - first + 1 < width,
                 "island still covers the whole panel — the DDC trace is being"
                 " stretched across spectrum it does not cover");
    }

    // Bench 2026-08-08: "still black gaps between wideband" and the DDC.
    // The outermost DDC bins are decimation-filter skirt and read
    // near-nothing; drawing the island at its full width put them on screen
    // as dark bands. Thetis discards 4% each side (specHPSDR.cs:529
    // [v2.10.3.15]) and so do we, so the island is narrower than the raw
    // sample rate would suggest and the wings cover the difference.
    void island_excludes_the_ddc_filter_skirt()
    {
        SpectrumWidget sw;
        configureExtendedPan(sw);
        QVERIFY(sw.extendedMode());

        const int width = 1000;
        const auto [first, last] = sw.listenableIslandPixels(width);
        const int islandPx = last - first + 1;

        // Unclipped the island would be 192/1920 of the panel = 100 px.
        // Clipped it is 92% of that.
        const double unclipped = width * (192000.0 / 1920000.0);
        const double expected  = unclipped * (1.0 - 2.0 * SpectrumWidget::kDdcClipFraction);

        QVERIFY2(std::abs(islandPx - expected) <= 2.0,
                 qPrintable(QStringLiteral("island is %1 px, expected ~%2 "
                                           "(unclipped would be %3) — the "
                                           "filter skirt is still being drawn")
                                .arg(islandPx).arg(expected).arg(unclipped)));
        QVERIFY2(islandPx < unclipped - 1.0,
                 "island still spans the full sample rate");
    }

    // An off-centre DDC (CTUN pan-drag) must move the island with it, not
    // keep it pinned mid-panel.
    void island_follows_the_ddc_off_centre()
    {
        SpectrumWidget sw;
        configureExtendedPan(sw);
        // Push the DDC 480 kHz low: a quarter of the window to the left of
        // centre, so the island centre lands at pixel 250.
        sw.setDdcCenterFrequency(14200000.0 - 480000.0);
        QVERIFY(sw.extendedMode());

        const auto [first, last] = sw.listenableIslandPixels(1000);
        const int centre = (first + last) / 2;
        QVERIFY2(std::abs(centre - 250) <= 2,
                 qPrintable(QStringLiteral("island centred at %1, expected ~250")
                                .arg(centre)));
    }

    // The island can run PARTWAY off the edge when the operator pans; the
    // span must clamp rather than produce out-of-range pixel indices that the
    // detector would write through.
    //
    // This case used to be tested with the DDC 5 MHz off-screen and asserted
    // `first <= last`, which described the defect rather than the contract:
    // clamping both ends together produced a one-pixel island at the nearest
    // edge, putting DDC bins on a pixel the DDC does not cover. The
    // no-overlap case is now its own test below. What stays here is the case
    // that really is a clamp, where the DDC straddles the window edge.
    void island_clamps_when_the_ddc_straddles_the_edge()
    {
        SpectrumWidget sw;
        configureExtendedPan(sw);
        // Window is 1.92 MHz wide around 14.2 MHz, so its low edge is at
        // 13.24 MHz. Park the 192 kHz DDC on that edge: half in, half out.
        sw.setDdcCenterFrequency(14200000.0 - 1920000.0 / 2.0);

        const int width = 1000;
        const auto [first, last] = sw.listenableIslandPixels(width);
        QVERIFY(first >= 0);
        QVERIFY(last <= width - 1);
        QVERIFY2(first < last, "a straddling DDC still covers real pixels");
        QCOMPARE(first, 0);   // clamped at the window edge, not off it
    }

    // Defect 2, the data-flow end: the bins must survive the trip in and be
    // kept per ADC, and the pan must be able to say which ADC it is on.
    void wideband_bins_are_stored_per_adc_and_selectable()
    {
        SpectrumWidget sw;
        QVector<float> adc0(8192, -120.0f);
        QVector<float> adc1(8192, -80.0f);
        sw.setWidebandBins(0, adc0);
        sw.setWidebandBins(1, adc1);

        QCOMPARE(sw.widebandBinsForTest(0).size(), 8192);
        QCOMPARE(sw.widebandBinsForTest(1).size(), 8192);
        QCOMPARE(sw.widebandBinsForTest(0).at(0), -120.0f);
        QCOMPARE(sw.widebandBinsForTest(1).at(0), -80.0f);

        // Default ADC0; a pan whose slice sits on chain 1 selects ADC1.
        QCOMPARE(sw.widebandAdcIndex(), 0);
        sw.setWidebandAdcIndex(1);
        QCOMPARE(sw.widebandAdcIndex(), 1);
        // Out-of-range must fall back rather than index off the end of the
        // two-ADC ceiling.
        sw.setWidebandAdcIndex(7);
        QCOMPARE(sw.widebandAdcIndex(), 0);
    }

    // The defect that made the whole feature dead on the bench: both zoom
    // gestures clamped visible bandwidth to the DDC rate, and
    // `m_bandwidthHz > m_sampleRateHz` is extended mode's only trigger, so
    // the toggle enabled a state no gesture could reach.
    void zoom_ceiling_opens_up_only_when_extended_view_is_allowed()
    {
        SpectrumWidget sw;
        sw.setSampleRate(192000.0);
        sw.setWidebandAdcRateHz(122880000.0);

        sw.setExtendedViewAllowed(false);
        QCOMPARE(sw.maxZoomOutBandwidthHz(), 192000.0);

        sw.setExtendedViewAllowed(true);
        QVERIFY2(sw.maxZoomOutBandwidthHz() > 192000.0,
                 "zoom is still pinned to the DDC rate — extended mode stays "
                 "unreachable no matter what the toggle says");
        // The ceiling is the wideband ADC's Nyquist: the span wing data
        // exists for. Anything past it is panel that can never be filled.
        QCOMPARE(sw.maxZoomOutBandwidthHz(), 61440000.0);
    }

    // A DDC faster than the wideband Nyquist must not have its zoom range
    // shrunk by turning extended view on.
    void zoom_ceiling_never_drops_below_the_ddc_rate()
    {
        SpectrumWidget sw;
        sw.setExtendedViewAllowed(true);
        sw.setWidebandAdcRateHz(122880000.0);
        sw.setSampleRate(100000000.0);      // wider than 61.44 MHz Nyquist
        QCOMPARE(sw.maxZoomOutBandwidthHz(), 100000000.0);
    }

    // Bench 2026-08-08. The correction is two terms and each one alone put
    // the wings off the panel in opposite directions: no normalisation
    // saturated, FFT-normalisation-plus-Thetis-constant fell through the
    // floor. Both are derived, neither is tuned.
    void wideband_calibration_has_both_terms()
    {
        // Term 1: coherent-gain reference. The contract is that it follows
        // the analysis window's sum, not a bare sample count -- the engine
        // windows with Hann, and hardcoding N would have left the wings
        // badly off the moment that window landed.
        const double coherentPeak = WidebandFftEngine::windowSum() / 2.0;
        const float expectedFft =
            -20.0f * std::log10(static_cast<float>(coherentPeak));
        const float fft = SpectrumWidget::widebandFftNormalisationDb();
        QVERIFY2(std::abs(fft - expectedFft) < 0.01f,
                 qPrintable(QStringLiteral("FFT term %1 dB does not follow the "
                                           "window sum (expected %2)")
                                .arg(fft).arg(expectedFft)));
        QVERIFY2(WidebandFftEngine::windowSum()
                     < double(WidebandFftEngine::kCaptureSamples),
                 "engine reports an unwindowed block");

        // Term 2: refer the wideband bin to the DDC bin, by NOISE bandwidth.
        SpectrumWidget sw;
        sw.setSampleRate(192000.0);
        sw.setWidebandAdcRateHz(122880000.0);
        const float bw = sw.widebandBandwidthNormalisationDb(SpectrumDetector::Peak);
        QVERIFY2(bw < 0.0f, "bandwidth term must pull the wideband side down");

        QVERIFY2(std::abs(sw.widebandTotalCalibrationDb(SpectrumDetector::Peak) - (fft + bw)) < 0.001f,
                 "total is not the sum of the two terms");
    }

    // Zero-padding moves the bins closer together WITHOUT narrowing what
    // each one integrates, so the bandwidth term has to be built from the
    // CAPTURE length. Keying it off the padded transform would under-correct
    // by exactly the padding factor and leave the wings that far hot.
    //
    // Asserted against the engine's own published noise bandwidth with a
    // known DDC reference, rather than against a hand-built figure: an
    // earlier version of this test compared two expressions that used
    // different DDC references and so could not fail for the right reason.
    void bandwidth_term_uses_capture_length_not_padded_length()
    {
        QVERIFY2(WidebandFftEngine::kFftSize
                     > WidebandFftEngine::kCaptureSamples,
                 "engine is not zero-padding, so this guard is meaningless");

        SpectrumWidget sw;
        sw.setSampleRate(192000.0);
        sw.setWidebandAdcRateHz(122880000.0);

        WidebandFftEngine engine;
        engine.setAdcSampleRateHz(122880000.0);

        // With no bins pushed in, binWidthHz() falls back to rate/4096 and
        // the DDC window ENB defaults to 1.0.
        const double ddcNoiseBw = 192000.0 / 4096.0;
        const float expected = -10.0f * std::log10(
            static_cast<float>(engine.noiseBandwidthHz() / ddcNoiseBw));

        QVERIFY2(std::abs(sw.widebandBandwidthNormalisationDb(SpectrumDetector::Peak) - expected) < 0.05f,
                 qPrintable(QStringLiteral("bandwidth term is %1 dB, expected "
                                           "%2 from the engine's capture-based "
                                           "noise bandwidth")
                                .arg(sw.widebandBandwidthNormalisationDb(SpectrumDetector::Peak))
                                .arg(expected)));

        // And the padded figure is a materially different number, so the
        // check above would actually catch the mistake.
        const float ifPadded = -10.0f * std::log10(
            static_cast<float>(engine.binSpacingHz() / ddcNoiseBw));
        QVERIFY2(std::abs(expected - ifPadded) > 3.0f,
                 "capture-based and padded-based figures are too close for "
                 "this test to discriminate");
    }

    // The bandwidth term must track BOTH widths, since a fixed constant would
    // be right for one DDC rate and wrong for every other.
    void bandwidth_normalisation_tracks_the_ddc_rate()
    {
        SpectrumWidget sw;
        sw.setWidebandAdcRateHz(122880000.0);

        sw.setSampleRate(192000.0);
        const float at192 = sw.widebandBandwidthNormalisationDb(SpectrumDetector::Peak);
        sw.setSampleRate(384000.0);
        const float at384 = sw.widebandBandwidthNormalisationDb(SpectrumDetector::Peak);

        // Doubling the DDC rate doubles its bin width, halving the ratio:
        // 3.01 dB less correction.
        QVERIFY2(std::abs((at384 - at192) - 3.0103f) < 0.05f,
                 qPrintable(QStringLiteral("correction moved %1 dB for a 2x "
                                           "rate change, expected 3.01")
                                .arg(at384 - at192)));
    }

    // The wing frequency axis comes from the ADC clock. From Thetis
    // wbDisplay.cs:4511 [v2.10.3.15] `private int sample_rate = 122880000;`
    // — the same value RadioModel seeds WidebandFftEngine with. A zero or
    // negative rate would make bin_width zero and divide the axis by nothing,
    // so it must be rejected rather than stored.
    void wideband_adc_rate_defaults_to_thetis_value_and_rejects_nonsense()
    {
        SpectrumWidget sw;
        QCOMPARE(sw.widebandAdcRateHz(), 122880000.0);

        sw.setWidebandAdcRateHz(153600000.0);
        QCOMPARE(sw.widebandAdcRateHz(), 153600000.0);

        sw.setWidebandAdcRateHz(0.0);
        QCOMPARE(sw.widebandAdcRateHz(), 153600000.0);
        sw.setWidebandAdcRateHz(-1.0);
        QCOMPARE(sw.widebandAdcRateHz(), 153600000.0);
    }

    // The island's noise reference depends on which detector produced it.
    //
    // applySpectrumDetector is handed invEnb = 1 / windowEnb and only
    // Average, Sample and RMS apply it (SpectrumDetector.cpp cases 2, 3, 4);
    // Peak and Rosenfell take a max and leave the window ENB in. So under a
    // normalising detector the island pixels are already ENB-corrected and
    // the wings must be referred to the bare bin width instead.
    //
    // One unconditional reference left the wings high by exactly the DDC
    // window ENB whenever Average / Sample / RMS was selected. Found by Codex
    // on PR #318.
    void bandwidth_term_follows_the_detector()
    {
        SpectrumWidget sw;
        sw.setSampleRate(192000.0);
        sw.setWidebandAdcRateHz(122880000.0);

        // m_fftWindowEnb is only written by a frame, so push one. Hann's
        // 1.5 is the interesting case: with the default 1.0 the two branches
        // coincide and the test could not fail.
        constexpr double kHannEnb = 1.5;
        QVector<float> bins(4096, 1.0e-12f);
        sw.updateSpectrumLinear(0, bins, kHannEnb, 0.0);

        const float peak = sw.widebandBandwidthNormalisationDb(
            SpectrumDetector::Peak);
        const float average = sw.widebandBandwidthNormalisationDb(
            SpectrumDetector::Average);

        // Average divides ENB out of the island, shrinking its reference
        // bandwidth, so the wings need that much MORE pulling down.
        const float expectedGap = 10.0f * std::log10(float(kHannEnb));
        QVERIFY2(std::abs((peak - average) - expectedGap) < 0.01f,
                 qPrintable(QStringLiteral("Peak and Average differ by %1 dB, "
                                           "expected %2 (the window ENB)")
                                .arg(peak - average).arg(expectedGap)));

        // Rosenfell is a peak-family detector and must track Peak, not
        // Average. Sample and RMS are normalising and must track Average.
        QCOMPARE(sw.widebandBandwidthNormalisationDb(SpectrumDetector::Rosenfell),
                 peak);
        QCOMPARE(sw.widebandBandwidthNormalisationDb(SpectrumDetector::Sample),
                 average);
        QCOMPARE(sw.widebandBandwidthNormalisationDb(SpectrumDetector::RMS),
                 average);
    }

    // One clamp helper, so a restored extended span survives every path that
    // re-applies a window.
    //
    // Three places in MainWindow re-apply a span: the startup restore, the
    // sample-rate handler, and applyStreamWindowToPan when a pan gains a
    // stream subscription. Each wrote its own clamp against the DDC rate, so
    // an extended zoom survived until whichever ran last, and Codex found
    // them one per round across three rounds on PR #318. This pins the shared
    // helper they now all use.
    void clamped_window_keeps_an_extended_span()
    {
        SpectrumWidget sw;
        sw.setExtendedViewAllowed(true);
        sw.setSampleRate(192000.0);
        sw.setDdcCenterFrequency(14200000.0);

        // A span well past the DDC rate but inside the wideband Nyquist.
        sw.setDisplayWindowClamped(14200000.0, 1920000.0);
        QCOMPARE(sw.bandwidth(), 1920000.0);
        QVERIFY2(sw.extendedMode(),
                 "the clamp collapsed an extended span onto the DDC");

        // Above the wideband Nyquist it does clamp, to the ceiling.
        sw.setDisplayWindowClamped(14200000.0, 1.0e9);
        QCOMPARE(sw.bandwidth(), sw.maxZoomOutBandwidthHz());

        // Non-positive means "give me the ceiling", the old contract.
        sw.setDisplayWindowClamped(14200000.0, 0.0);
        QCOMPARE(sw.bandwidth(), sw.maxZoomOutBandwidthHz());

        // And with extended view off the ceiling is the DDC rate again, so
        // the ordinary case is exactly what it always was.
        sw.setExtendedViewAllowed(false);
        sw.setDisplayWindowClamped(14200000.0, 1920000.0);
        QCOMPARE(sw.bandwidth(), 192000.0);
    }

    // Withdrawing extended-view permission has to pull the CURRENT span back,
    // not just lower the ceiling for the next gesture.
    //
    // maxZoomOutBandwidthHz is read by the wheel and the scale drag and by
    // nothing else, so switching Extended view off left the wide span in
    // place with m_extendedMode cleared. The next frame took the ordinary
    // path, treated the whole panel as island, and stretched the DDC's bins
    // across it with no wings: the stretched-island defect reachable through
    // the toggle meant to be the way out of it. Found by Codex on PR #318.
    void disabling_extended_view_clamps_the_current_span()
    {
        SpectrumWidget sw;
        sw.setExtendedViewAllowed(true);
        sw.setSampleRate(192000.0);
        sw.setDdcCenterFrequency(14200000.0);
        sw.setFrequencyRange(14200000.0, 1920000.0);   // 10x past the DDC
        QVERIFY2(sw.extendedMode(), "test needs extended mode engaged");

        sw.setExtendedViewAllowed(false);

        QVERIFY2(!sw.extendedMode(), "extended mode should be off");
        QCOMPARE(sw.bandwidth(), 192000.0);
        QVERIFY2(sw.bandwidth() <= sw.maxZoomOutBandwidthHz(),
                 "span left above the ceiling it was just clamped to");
    }

    // Pan far enough into a wing and the DDC leaves the window entirely.
    //
    // Clamping both endpoints together manufactured a one-pixel island at
    // whichever edge was nearest, putting DDC bins on a pixel the DDC does not
    // cover. Worse, the matching clamp in visibleBinRange plus the 4% clip
    // drove sliceCount negative, so updateSpectrumLinear returned before the
    // wings were filled and the whole survey froze on stale pixels. Found by
    // Codex on PR #318.
    void island_is_empty_when_the_ddc_is_off_screen()
    {
        SpectrumWidget sw;
        sw.setExtendedViewAllowed(true);
        sw.setSampleRate(192000.0);
        sw.setDdcCenterFrequency(14200000.0);

        // 400 kHz window, so extended mode engages, parked 5 MHz away: the
        // DDC's 192 kHz cannot reach it.
        sw.setFrequencyRange(19200000.0, 400000.0);
        QVERIFY2(sw.extendedMode(), "test needs extended mode engaged");

        const auto [first, last] = sw.listenableIslandPixels(1000);
        QVERIFY2(last < first,
                 qPrintable(QStringLiteral("expected an empty island, got "
                                           "[%1, %2] which is %3 pixel(s) of "
                                           "DDC data outside the DDC")
                                .arg(first).arg(last).arg(last - first + 1)));

        // And the window still resolves to a full-width wing region: an empty
        // island means the wideband plane owns every pixel, which is what
        // fillWidebandWings keys off.
        QVERIFY2(first == 0 && last == -1,
                 "empty island must be the canonical {0, -1}, since that is "
                 "the shape fillWidebandWings treats as 'no island'");
    }

    // One clipped half-span, shared by the paint, the markers and the click
    // router.
    //
    // The island is drawn kDdcClipFraction narrower than the DDC rate at each
    // edge, but drawExtendedIslandBounds used rate/2 for its markers and the
    // click router used rate/2 for its hit test. That left each 4% strip
    // painting wideband survey data while a click in it retuned the slice
    // rather than the DDC, with the boundary marker drawn outside the
    // boundary. Found by Codex on PR #318.
    void island_half_span_is_one_number()
    {
        SpectrumWidget sw;
        sw.setExtendedViewAllowed(true);
        sw.setSampleRate(192000.0);
        sw.setDdcCenterFrequency(14200000.0);
        sw.setFrequencyRange(14200000.0, 1920000.0);   // 10x past the DDC
        QVERIFY2(sw.extendedMode(), "test needs extended mode engaged");

        const double half = sw.ddcIslandHalfSpanHz();
        QVERIFY2(half < 192000.0 * 0.5,
                 "half-span is not clipped, so the three consumers cannot "
                 "disagree and this test proves nothing");
        QCOMPARE(half, 192000.0 * (0.5 - SpectrumWidget::kDdcClipFraction));

        // The painted island has to land on that same number: convert its
        // pixel edges back to Hz and they must bracket the clipped span, not
        // the raw one.
        constexpr int kWidth = 1000;
        const auto [first, last] = sw.listenableIslandPixels(kWidth);
        const double hzPerPixel = 1920000.0 / double(kWidth);
        const double displayLowHz = 14200000.0 - 1920000.0 / 2.0;
        const double islandLowHz  = displayLowHz + first * hzPerPixel;
        const double islandHighHz = displayLowHz + (last + 1) * hzPerPixel;

        QVERIFY2(std::abs(islandLowHz - (14200000.0 - half)) <= hzPerPixel,
                 "painted island's low edge is not the clipped half-span");
        QVERIFY2(std::abs(islandHighHz - (14200000.0 + half)) <= hzPerPixel,
                 "painted island's high edge is not the clipped half-span");
    }
};

QTEST_MAIN(TestExtendedPanWings)
#include "tst_extended_pan_wings.moc"
