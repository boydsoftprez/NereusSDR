// =================================================================
// tests/tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.cpp
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Upstream file
// and line references in the comments below are context for a reviewer;
// the ported logic itself lives in src/gui/SpectrumWidget.cpp, which
// carries its own verbatim upstream header and PROVENANCE row.
//
// TNF Task 10. Design:
//   docs/architecture/2026-07-28-tunable-notch-filter-design.md
//   section 8.3 ("Visual notch (trace dent)") and section 11.
//
// The invariant under test, asserted in BOTH directions:
//
//   * processNoiseFloor() and peakDbmInSlicePassband() read a pristine,
//     UNDENTED copy of the spectrum pixels. A display preference must
//     not silently move the noise-floor estimate or the analog S-Meter's
//     MaxBin reading. That is the NereusSDR-only hazard section 8.3
//     names: Thetis reads MaxBin from WDSP upstream of its display code,
//     so a Thetis visual notch structurally cannot move its meter, and
//     ours would.
//
//   * ActivePeakHoldTrace and PeakBlobDetector DO see the dent. That is
//     Thetis-faithful (its spectral peak hold and blob detector both
//     read the dented array, display.cs:5269 / :5280 / :5337), and it is
//     asserted positively so a later reviewer does not "fix" it into a
//     divergence.
//
// Fixture geometry, chosen so every pixel index below is exact:
//   pan centre 14.200000 MHz, span 800 Hz across 800 display pixels
//     => 1.0 Hz per display pixel,
//   tone + notch at 14.200150 MHz => display pixel 550,
//   4096 synthetic FFT bins => 5.12 bins per pixel, tone in bin 2816.
//
// The NotchVisualEnabled model property (plan cycle A) is not re-tested
// here: Task 3 shipped it and tests/tst_notch_persistence.cpp already
// pins the default, the single-emit setter and the AppSettings
// round-trip. The chkVisualNotch two-way binding (plan cycle F) is
// likewise already pinned by tests/tst_mnf_setup_page.cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  TNF Task 10. Original test for
//                                    NereusSDR with AI-assisted
//                                    authoring via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QVector>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestNotchVisualDoesNotPerturbNoiseFloorOrMaxbin : public QObject
{
    Q_OBJECT

private:
    // ---- Fixture geometry ----
    static constexpr double kPanCentreHz  = 14200000.0;
    static constexpr double kPanSpanHz    = 800.0;      // 1.0 Hz per pixel
    static constexpr double kToneMhz      = 14.200150;
    static constexpr int    kTonePixel    = 550;
    static constexpr int    kDisplayWidth = 800;        // headless fallback
    static constexpr int    kToneBin      = 2816;       // 4096 bins over 800 Hz

    // Synthetic linear-power FFT bins: flat floor plus one carrier, so the
    // tone is the frame's only strict local maximum and the blob detector
    // has exactly one thing to find.
    QVector<float> makeBins() const
    {
        constexpr int kFftSize = 4096;
        QVector<float> bins(kFftSize, 1e-12f);   // ~ -120 dBm floor
        bins[kToneBin] = 1e-3f;                  // ~ -30 dBm carrier
        return bins;
    }

    void configure(SpectrumWidget& w) const
    {
        w.setFrequencyRange(kPanCentreHz, kPanSpanHz);
        w.setDdcCenterFrequency(kPanCentreHz);
        w.setSampleRate(kPanSpanHz);
        w.setVfoFrequency(kPanCentreHz);
        // Passband straddles the carrier so peakDbmInSlicePassband has
        // something to find inside the 800 Hz window.
        w.setFilterOffset(50, 300);
        // Peak + no averaging keeps a single frame deterministic and puts
        // the carrier in exactly one display pixel.
        w.setSpectrumDetector(SpectrumDetector::Peak);
        w.setSpectrumAveraging(SpectrumAveraging::None);
        w.setWaterfallDetector(SpectrumDetector::Peak);
        w.setWaterfallAveraging(SpectrumAveraging::None);
        w.setNotchMinWidthHz(100.0);
        w.setNotchGlobalEnabled(true);
    }

    static QVector<SpectrumWidget::NotchMarker> oneNotch(double widthHz,
                                                         bool active = true)
    {
        SpectrumWidget::NotchMarker m;
        m.id      = 1;
        m.freqMhz = kToneMhz;
        m.widthHz = widthHz;
        m.active  = active;
        return QVector<SpectrumWidget::NotchMarker>{m};
    }

    void feed(SpectrumWidget& w, int frames) const
    {
        const QVector<float> bins = makeBins();
        for (int i = 0; i < frames; ++i) {
            // windowEnb 2.0 ~ Blackman-Harris-4; dbmOffset -10 arbitrary.
            w.updateSpectrumLinear(0, bins, 2.0, -10.0);
        }
    }

    // Width in pixels of the region where `dented` sits below `base`.
    static int dentedSpanPixels(const QVector<float>& base,
                                const QVector<float>& dented)
    {
        int lo = -1;
        int hi = -1;
        const int n = qMin(base.size(), dented.size());
        for (int i = 0; i < n; ++i) {
            if (base[i] - dented[i] > 0.01f) {
                if (lo < 0) { lo = i; }
                hi = i;
            }
        }
        return (lo < 0) ? 0 : (hi - lo + 1);
    }

private slots:

    // ---- Cycle B: the spectrum-plane dent ----

    void widget_visual_notch_defaults_off()
    {
        SpectrumWidget w;
        QCOMPARE(w.visualNotchEnabled(), false);
    }

    void visual_notch_off_leaves_the_trace_undented()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> clean = base.renderedPixels();
        QCOMPARE(clean.size(), kDisplayWidth);

        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers(oneNotch(200.0));
        // Visual notch defaults off: a populated marker set on its own must
        // not touch the data the trace is drawn from.
        feed(w, 1);

        QCOMPARE(w.renderedPixels().size(), clean.size());
        QCOMPARE(dentedSpanPixels(clean, w.renderedPixels()), 0);
    }

    void visual_notch_dents_the_spectrum_trace_at_the_notch_centre()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> clean = base.renderedPixels();

        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers(oneNotch(200.0));
        w.setVisualNotchEnabled(true);
        feed(w, 1);

        const QVector<float>& dented = w.renderedPixels();
        // The left and right skirt loops both START at the centre pixel, so
        // it takes the full attenuation twice.
        QVERIFY(clean[kTonePixel] - dented[kTonePixel] > 150.0f);
        // Well inside the skirt.
        QVERIFY(clean[kTonePixel - 5] - dented[kTonePixel - 5] > 1.0f);
        // Well outside it (dent half-width is 110 px at 1.0 Hz per pixel).
        QCOMPARE(dented[kTonePixel - 200], clean[kTonePixel - 200]);

        // The pristine copy the measurement consumers read is NOT dented.
        QCOMPARE(w.undentedPixelsForTest().size(), clean.size());
        QCOMPARE(w.undentedPixelsForTest()[kTonePixel], clean[kTonePixel]);
    }

    void dent_span_carries_the_twenty_hz_fudge_factor()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> clean = base.renderedPixels();

        SpectrumWidget w;
        configure(w);                        // WDSP minimum pushed as 100 Hz
        w.setNotchMarkers(oneNotch(200.0));  // 200 > 100, so no clamp
        w.setVisualNotchEnabled(true);
        feed(w, 1);

        // 200 Hz + 20 Hz fudge = 220 Hz at 1.0 Hz per pixel. The skirt loops
        // cover [cX - wL + 1, cX + wR - 1] = 2 * 110 - 1 = 219 pixels.
        // Without the fudge it would be 199, so this band is decisive.
        const int span = dentedSpanPixels(clean, w.renderedPixels());
        QVERIFY2(span >= 217 && span <= 221,
                 qPrintable(QStringLiteral("dent span %1 px, expected ~219")
                                .arg(span)));
    }

    void dent_span_clamps_to_the_wdsp_minimum_notch_width()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> clean = base.renderedPixels();

        // 40 Hz notch, WDSP minimum 100 Hz -> dent is 100 + 20 = 120 Hz.
        SpectrumWidget clamped;
        configure(clamped);
        clamped.setNotchMarkers(oneNotch(40.0));
        clamped.setVisualNotchEnabled(true);
        feed(clamped, 1);
        const int clampedSpan =
            dentedSpanPixels(clean, clamped.renderedPixels());
        QVERIFY2(clampedSpan >= 117 && clampedSpan <= 121,
                 qPrintable(QStringLiteral("clamped span %1 px, expected ~119")
                                .arg(clampedSpan)));

        // Same notch with no WDSP minimum pushed -> dent is 40 + 20 = 60 Hz.
        SpectrumWidget unclamped;
        configure(unclamped);
        unclamped.setNotchMinWidthHz(0.0);
        unclamped.setNotchMarkers(oneNotch(40.0));
        unclamped.setVisualNotchEnabled(true);
        feed(unclamped, 1);
        const int unclampedSpan =
            dentedSpanPixels(clean, unclamped.renderedPixels());
        QVERIFY2(unclampedSpan >= 57 && unclampedSpan <= 61,
                 qPrintable(QStringLiteral("unclamped span %1 px, expected ~59")
                                .arg(unclampedSpan)));
    }

    // ---- Cycle C: the waterfall plane ----

    void visual_notch_dents_the_waterfall_plane_too()
    {
        SpectrumWidget base;
        configure(base);
        feed(base, 1);
        const QVector<float> cleanWf = base.wfRenderedPixels();
        QCOMPARE(cleanWf.size(), kDisplayWidth);

        SpectrumWidget w;
        configure(w);
        w.setNotchMarkers(oneNotch(200.0));
        w.setVisualNotchEnabled(true);
        feed(w, 1);

        // NereusSDR keeps the waterfall pixels in their own array, so denting
        // the spectrum plane does not reach them: this is a second explicit
        // call, matching the second modifyDataForNotches call upstream.
        QCOMPARE(w.wfRenderedPixels().size(), cleanWf.size());
        QVERIFY(cleanWf[kTonePixel]
                    - w.wfRenderedPixels()[kTonePixel] > 150.0f);
        const int span = dentedSpanPixels(cleanWf, w.wfRenderedPixels());
        QVERIFY2(span >= 217 && span <= 221,
                 qPrintable(QStringLiteral("waterfall dent span %1 px, "
                                           "expected ~219").arg(span)));
    }

    // ---- Cycle D: what reads the undented copy, and what does not ----

    void noise_floor_estimate_reads_the_undented_copy()
    {
        // The accumulator only counts pixels BELOW the running estimate, and
        // the estimate starts at -200 dBm and drifts up 1 dB per frame until
        // it reaches the synthetic floor. One frame therefore proves nothing:
        // both cases would sit in the same drift branch. Feed long enough for
        // the dented skirt to start qualifying and the two to diverge.
        constexpr int kFramesToConverge = 80;

        SpectrumWidget off;
        configure(off);
        off.setNotchMarkers(oneNotch(200.0));
        feed(off, kFramesToConverge);

        SpectrumWidget on;
        configure(on);
        on.setNotchMarkers(oneNotch(200.0));
        on.setVisualNotchEnabled(true);
        feed(on, kFramesToConverge);

        // Sanity: the dent really is on the trace for this frame, so the
        // comparison below is not vacuous.
        QVERIFY(off.renderedPixels()[kTonePixel]
                    - on.renderedPixels()[kTonePixel] > 150.0f);

        // A display preference must not move a measurement.
        QCOMPARE(on.nfFftBinAverageForTest(), off.nfFftBinAverageForTest());
    }

    void max_bin_passband_peak_reads_the_undented_copy()
    {
        SpectrumWidget off;
        configure(off);
        off.setNotchMarkers(oneNotch(200.0));
        feed(off, 1);

        SpectrumWidget on;
        configure(on);
        on.setNotchMarkers(oneNotch(200.0));
        on.setVisualNotchEnabled(true);
        feed(on, 1);

        const double offPeak = off.peakDbmInSlicePassband();
        const double onPeak  = on.peakDbmInSlicePassband();

        // Guard against the -400 sentinel making this pass for free.
        QVERIFY2(offPeak > -400.0,
                 "passband peak hit the sentinel; fixture geometry is wrong");
        // The analog S-Meter's MaxBin mode is fed from this. Notching a loud
        // carrier must not drop the needle.
        QCOMPARE(onPeak, offPeak);
    }

    void active_peak_hold_sees_the_dent()
    {
        SpectrumWidget off;
        configure(off);
        off.setActivePeakHoldEnabled(true);
        off.setNotchMarkers(oneNotch(200.0));
        feed(off, 1);

        SpectrumWidget on;
        configure(on);
        on.setActivePeakHoldEnabled(true);
        on.setNotchMarkers(oneNotch(200.0));
        on.setVisualNotchEnabled(true);
        feed(on, 1);

        QCOMPARE(on.activePeakHoldPeaksForTest().size(),
                 off.activePeakHoldPeaksForTest().size());
        QVERIFY(off.activePeakHoldPeaksForTest().size() > kTonePixel);

        // Thetis-faithful and deliberate: spectral peak hold reads the DENTED
        // array (display.cs:5337 [v2.10.3.15] feeds off `max`, not
        // `max_copy`). Do not "fix" this into a divergence.
        QVERIFY(off.activePeakHoldPeaksForTest()[kTonePixel]
                    - on.activePeakHoldPeaksForTest()[kTonePixel] > 150.0f);
    }

    void peak_blobs_see_the_dent()
    {
        SpectrumWidget off;
        configure(off);
        off.setPeakBlobsEnabled(true);
        off.setNotchMarkers(oneNotch(200.0));
        feed(off, 1);

        SpectrumWidget on;
        configure(on);
        on.setPeakBlobsEnabled(true);
        on.setNotchMarkers(oneNotch(200.0));
        on.setVisualNotchEnabled(true);
        feed(on, 1);

        int   offEnabled = 0;
        float offTop     = -400.0f;
        for (const PeakBlob& b : off.peakBlobsForTest()) {
            if (b.enabled) {
                ++offEnabled;
                offTop = qMax(offTop, b.max_dBm);
            }
        }
        QVERIFY2(offEnabled > 0, "undented frame produced no peak blob");

        float onTop = -400.0f;
        for (const PeakBlob& b : on.peakBlobsForTest()) {
            if (b.enabled) {
                onTop = qMax(onTop, b.max_dBm);
            }
        }

        // Also Thetis-faithful: the blob detector reads the dented array
        // (display.cs:5280 [v2.10.3.15]), so notching the only carrier in the
        // frame takes the top blob down with it.
        QVERIFY2(offTop - onTop > 50.0f,
                 qPrintable(QStringLiteral("top blob %1 dBm undented vs %2 dBm "
                                           "dented; blobs did not see the dent")
                                .arg(static_cast<double>(offTop))
                                .arg(static_cast<double>(onTop))));
    }
};

QTEST_MAIN(TestNotchVisualDoesNotPerturbNoiseFloorOrMaxbin)
#include "tst_notch_visual_does_not_perturb_noise_floor_or_maxbin.moc"
