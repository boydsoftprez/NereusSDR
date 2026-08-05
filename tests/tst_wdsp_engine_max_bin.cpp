// no-port-check: NereusSDR-original unit-test file. Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_wdsp_engine_max_bin.cpp  (NereusSDR)
// =================================================================
//
// Tests for WdspEngine Max Bin detector (Phase 3P-II + crash-fix):
//   WdspEngine::getRxaSignalAverage (Task 31, RXA_S_AV wrapper)
//   WdspEngine::setupMaxBinDetector (Task 32, NereusSDR-native algorithm)
//   WdspEngine::getMaxBinDbm        (Task 32, NereusSDR-native algorithm)
//   WdspEngine::onSpectrumBinsForMaxBin (crash-fix slot, NereusSDR-native)
//
// Background: the original Tasks 31-32 implementation called the WDSP
// ::SetupDetectMaxBin / ::GetDetectMaxBin C functions, which require a live
// pdisp[disp] pointer allocated by ::CreateAnalyzer.  NereusSDR's FFTEngine
// uses raw FFTW3 directly and never calls CreateAnalyzer, so pdisp[0] is
// always null.  The result was a SIGSEGV inside SetupDetectMaxBin+16
// triggered by the filterChanged -> QTimer::singleShot(100) lambda in
// MainWindow.cpp wireSliceToSpectrum().
//
// Fix (Option C): the public API names are preserved; the implementation
// is now NereusSDR-native state (WdspEngine::MaxBinDetector), fed by
// FFTEngine::fftReady via onSpectrumBinsForMaxBin.  The Thetis algorithm
// (scan + slow-release smoothing) runs unchanged against FFTEngine's dBm bins.
//
// This test file exercises both the safety surface (no crash with
// default-constructed engine, m_initialized guard now dropped from these
// wrappers) and the algorithm (bin-range formula, smoothing, clamping).
//
// Source references:
//   Thetis Console/dsp.cs:387-388 [@501e3f5]    - GetRXAMeter P/Invoke
//   Thetis Console/console.cs:957 [@501e3f5]     - RXA_S_AV selector
//   Thetis wdsp/analyzer.c:688-830 [@501e3f5]   - calc_dmb + DetectMaxBin
//   Thetis wdsp/analyzer.c:703 [@501e3f5]        - dmb_max_dB = -400.0 sentinel
//   Thetis wdsp/analyzer.c:1442 [@501e3f5]       - developer example (default values)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19 - New test file for Phase 3P-II Phase 2 Tasks 31-32:
//                WdspEngine MaxBin detector wrappers smoke test.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation
//                via Anthropic Claude Code.
//   2026-05-19 - Crash-fix (Option C): replaced WDSP-dependent paths
//                with NereusSDR-native MaxBinDetector algorithm tests.
//                Extended to cover findsMaxInPassbandWindow,
//                smoothsTowardSteadyState (was decaysWithoutNewPeaks
//                before per-bin averaging replaced output drift),
//                clampsBinRangeToArrayBounds.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation
//                via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <cmath>

#include "core/WdspEngine.h"

using namespace NereusSDR;

class WdspEngineMaxBinTest : public QObject {
    Q_OBJECT

private slots:

    // ── Test 1: getRxaSignalAverage is callable without crash ────────────────
    //
    // WdspEngine::getRxaSignalAverage returns -140.0 when the engine is not
    // initialized (m_initialized guard fires before any WDSP call).
    //
    // From Thetis Console/dsp.cs:387-388 [@501e3f5] (P/Invoke)
    // From Thetis Console/dsp.cs:957 [v2.10.3.15] (RXA_S_AV selector in
    //   CalculateRXMeter; the adjacent ADC_REAL case at dsp.cs:959 carries a
    //   //MW0LGE [2.9.0.7] attribution preserved from the same switch block.)
    // (Path corrected here to match the upstream-clean cite landed by commit
    // 6accd45c for the production WdspEngine + MeterPoller call sites; this
    // test was missed in that pass.)
    void getRxaSignalAverage_doesNotCrash() {
        WdspEngine engine;
        double result = engine.getRxaSignalAverage(0);
        QVERIFY(std::isfinite(result));
    }

    // ── Test 2: setupMaxBinDetector with defaults does not crash ─────────────
    //
    // WdspEngine::setupMaxBinDetector now writes to m_maxBinDetectors[0].
    // It must not crash for a default-constructed engine and must not require
    // m_initialized = true (the WDSP pdisp[] path is gone).
    //
    // Algorithm from Thetis wdsp/analyzer.c:688-756 [@501e3f5] (calc_dmb)
    void setupMaxBinDetector_doesNotCrash() {
        WdspEngine engine;
        // No m_initialized requirement for the NereusSDR-native path.
        engine.setupMaxBinDetector(0);  // defaults: rate=192000, fLow=-3000, fHigh=-300, tau=0.5, fps=60
        QVERIFY(true);
    }

    // ── Test 3: getMaxBinDbm returns a finite value ───────────────────────────
    //
    // After setupMaxBinDetector(0) the detector is active with maxDb = -400.0
    // (Thetis Init_DetectMaxBin sentinel at analyzer.c:703 [@501e3f5]).
    // std::isfinite(-400.0) is true.
    void getMaxBinDbm_returnsFiniteValue() {
        WdspEngine engine;
        engine.setupMaxBinDetector(0);
        double dbm = engine.getMaxBinDbm(0);
        QVERIFY(std::isfinite(dbm));
    }

    // ── Test 4: findsMaxInPassbandWindow ─────────────────────────────────────
    //
    // Verify that onSpectrumBinsForMaxBin finds the peak bin inside the
    // configured [fLow, fHigh] window and not a bin outside it.
    //
    // Setup: rate=192000, N=1024, fLow=-3000, fHigh=-300.
    //   binSpacing = 192000/1024 = 187.5 Hz/bin
    //   firstBin = 512 + round(-3000/187.5) = 512 + round(-16) = 512 - 16 = 496
    //   lastBin  = 512 + round(-300/187.5)  = 512 + round(-1.6) = 512 - 2 = 510
    //   peak bin: 512 + round(-1500/187.5) = 512 + round(-8) = 504 (inside window)
    //
    // On the first frame starting from maxDb=-400.0:
    //   decay = exp(-1/(0.5*60)) = exp(-1/30) ~ 0.9672
    //   smoothing step: maxDb -= |( 1 - 0.9672) * (-400.0)| = 400 * 0.0328 = 13.13
    //     => maxDb = -400 - 13.13 = -413.13
    //   peak attack: newMaxDb (-50) > maxDb (-413.13) => maxDb = -50.0
    // So result must be exactly -50.0 (qFuzzyCompare).
    //
    // Algorithm from Thetis wdsp/analyzer.c:800-822 [@501e3f5]
    void findsMaxInPassbandWindow() {
        WdspEngine engine;
        engine.setupMaxBinDetector(/*disp=*/0,
                                   /*ss=*/0, /*LO=*/0,
                                   /*rate=*/192000.0,
                                   /*fLow=*/-3000.0,
                                   /*fHigh=*/-300.0,
                                   /*tau=*/0.5,
                                   /*fps=*/60);

        const int N = 1024;
        QVector<float> bins(N, -120.0f);

        // Peak bin: 512 + round(-1500 / (192000/1024)) = 512 - 8 = 504.
        const double binSpacing = 192000.0 / N;
        const int peakBin = 512 + static_cast<int>(std::round(-1500.0 / binSpacing));
        QVERIFY(peakBin >= 496 && peakBin <= 510);  // inside window
        bins[peakBin] = -50.0f;

        engine.onSpectrumBinsForMaxBin(0, bins);

        const double result = engine.getMaxBinDbm(0);
        // After one frame from sentinel: smoothing drives maxDb to -413.13,
        // then peak attack replaces with -50.0.
        QVERIFY(qFuzzyCompare(result, -50.0));
    }

    // ── Test 5: smoothsTowardSteadyState ─────────────────────────────────────
    //
    // After per-bin averaging was added, MaxBin no longer uses the
    // analyzer.c:815-818 output-side drift formula.  Instead, each bin
    // is recursively averaged in dB domain (matching SpectrumAvenger's
    // LogRecursive mode), and the max-scan reads the smoothed bin
    // values.  Behavior:
    //   - First frame copies binsDbm[i] directly into binDbAvg[i]
    //     (initialization).
    //   - Subsequent frames update binDbAvg[i] = 0.85*prev + 0.15*new
    //     for bins in the scan range.
    //   - Bins outside the scan range hold their last value.
    //
    // So with 60 silent frames, the scan-range bins exponentially
    // average DOWN to the silent value, and a new peak takes several
    // frames to converge UP to the new peak value.  No output drift.
    void smoothsTowardSteadyState() {
        WdspEngine engine;
        engine.setupMaxBinDetector(0, 0, 0, 192000.0, -3000.0, -300.0, 0.5, 60);

        const int N = 1024;
        // Peak frame: all bins at -400 sentinel except the peak bin.
        QVector<float> peakBins(N, -400.0f);
        const double binSpacing = 192000.0 / N;
        const int peakBin = 512 + static_cast<int>(std::round(-1500.0 / binSpacing));
        peakBins[peakBin] = -50.0f;

        // Drive one peak frame to establish maxDb = -50 (first-frame copy).
        engine.onSpectrumBinsForMaxBin(0, peakBins);
        QVERIFY(qFuzzyCompare(engine.getMaxBinDbm(0), -50.0));

        // Drive 60 silent frames (-400) so the peak bin averages down.
        // alpha = 0.85, per-frame factor = 0.85.  After 60 frames:
        //   final = 0.85^60 * (-50) + (1 - 0.85^60) * (-400)
        //         ≈ 6.4e-5 * -50 + ~1.0 * -400 ≈ -400
        QVector<float> silentBins(N, -400.0f);
        for (int i = 0; i < 60; ++i) {
            engine.onSpectrumBinsForMaxBin(0, silentBins);
        }
        QVERIFY(engine.getMaxBinDbm(0) < -200.0);

        // Drive 60 frames at the new peak value -55: per-bin averaging
        // converges toward -55 (and well above the -400 floor).  After
        // many frames the peak bin's average is close to -55, well above
        // the silent floor everywhere else.
        QVector<float> peak2Bins(N, -400.0f);
        peak2Bins[peakBin] = -55.0f;
        for (int i = 0; i < 60; ++i) {
            engine.onSpectrumBinsForMaxBin(0, peak2Bins);
        }
        // After 60 steady-state frames: 0.85^60 * (-400) + (1 - 0.85^60) * (-55)
        //                              ≈ -55 (converged).
        const double result = engine.getMaxBinDbm(0);
        QVERIFY2(std::abs(result - (-55.0)) < 1.0,
                 qPrintable(QString("expected close to -55, got %1").arg(result)));
    }

    // ── Test 6: clampsBinRangeToArrayBounds ──────────────────────────────────
    //
    // When fLow/fHigh are far outside the array, the bin indices must clamp
    // to [0, N-1].  The entire array is effectively scanned; the global max wins.
    //
    // From Thetis wdsp/analyzer.c:688-756 [@501e3f5] (calc_dmb):
    //   NereusSDR uses qBound(0, half + round(f/binSpacing), N-1).
    void clampsBinRangeToArrayBounds() {
        WdspEngine engine;
        engine.setupMaxBinDetector(0, 0, 0, 192000.0,
                                   /*fLow=*/-100000.0,   // clamps to bin 0
                                   /*fHigh=*/ 100000.0,  // clamps to bin N-1
                                   0.5, 60);

        const int N = 1024;
        QVector<float> bins(N, -120.0f);
        bins[0]   = -40.0f;  // extreme low end
        bins[N-1] = -45.0f;  // extreme high end

        engine.onSpectrumBinsForMaxBin(0, bins);

        // Global max in the array is -40.0 (bins[0]).
        QVERIFY(qFuzzyCompare(engine.getMaxBinDbm(0), -40.0));
    }

    // ── Test 7: sliceOffsetHz shifts the scan window (CTUN-on case) ──────────
    //
    // FFTEngine bins are emitted in DDC baseband.  With CTUN on (NereusSDR's
    // default), the user's tuned slice does NOT sit at DDC center.  Without
    // applying the sliceOffsetHz term to the scan window, MaxBin always
    // points at DDC center bins (noise floor) and never tracks the signal
    // the user is listening to.  This test pins the fix: setting
    // setMaxBinSliceOffsetHz moves the scan window so the slice's signal
    // is found.
    //
    // Setup mirrors a CTUN-tuned LSB SSB receive scenario:
    //   - DDC at 14.200 MHz (FFT bin N/2)
    //   - Slice tuned to 14.225 MHz (25 kHz above DDC center)
    //   - Slice's audio filter: -2850 to -150 Hz (LSB SSB default)
    //   - Signal appears at DDC-baseband ~+24 kHz (slice center + ~-1 kHz
    //     in-passband audio offset)
    void sliceOffsetMovesScanWindow() {
        WdspEngine engine;
        engine.setupMaxBinDetector(/*disp=*/0, /*ss=*/0, /*LO=*/0,
                                   /*rate=*/192000.0,
                                   /*fLow=*/-2850.0,   // slice-baseband SSB filter
                                   /*fHigh=*/-150.0,
                                   /*tau=*/0.5, /*fps=*/60);

        const int N = 1024;
        const double binSpacing = 192000.0 / N;
        QVector<float> bins(N, -120.0f);  // -120 dBm noise floor everywhere

        // Place a strong peak at DDC-baseband +24000 Hz (matching a slice
        // tuned to DDC + 25 kHz with the signal landing inside the SSB
        // audio passband ~1 kHz below slice center).
        const int peakBin = 512 + static_cast<int>(std::round(+24000.0 / binSpacing));
        QVERIFY(peakBin > 512 && peakBin < N);
        bins[peakBin] = -50.0f;

        // Step A: no slice offset.  Default scan window is [-2850, -150] Hz
        // (slightly left of DDC center).  The +24 kHz peak is far OUTSIDE
        // that window; the scan only sees the -120 dBm noise floor.
        engine.onSpectrumBinsForMaxBin(0, bins);
        const double withoutOffset = engine.getMaxBinDbm(0);
        QVERIFY2(withoutOffset < -110.0,
                 "Without slice offset, MaxBin should read the noise floor, "
                 "not the +24 kHz peak.  Bug: scan window stuck at DDC center.");

        // Step B: set slice offset = +25 kHz (mirrors slice 25 kHz above DDC).
        // Scan window slides to [+25000 - 2850, +25000 - 150] Hz =
        // [+22150, +24850] Hz.  The +24 kHz peak now falls INSIDE.
        engine.setMaxBinSliceOffsetHz(/*disp=*/0, /*sliceOffsetHz=*/25000.0);
        engine.onSpectrumBinsForMaxBin(0, bins);
        const double withOffset = engine.getMaxBinDbm(0);
        QCOMPARE(withOffset, -50.0);  // peak found exactly
    }

    // ── Test 8: setMaxBinSliceOffsetHz is idempotent and survives setup ──────
    //
    // Calling setMaxBinSliceOffsetHz with the same value twice is a no-op
    // (no detector reset).  More importantly, a subsequent
    // setupMaxBinDetector call must NOT clear the stored offset -- otherwise
    // every filter change would silently re-zero the CTUN offset until the
    // next slice frequency emit re-pushes it.
    void sliceOffsetSurvivesSetupAndIsIdempotent() {
        WdspEngine engine;
        engine.setupMaxBinDetector(0, 0, 0, 192000.0, -2850.0, -150.0, 0.5, 60);
        engine.setMaxBinSliceOffsetHz(/*disp=*/0, /*sliceOffsetHz=*/12345.0);

        // Reconfigure with new filter edges (mirrors a CW filter change).
        engine.setupMaxBinDetector(0, 0, 0, 192000.0, -250.0, +250.0, 0.5, 60);

        // Offset must still be in effect.  Drive a peak at +12 kHz; with
        // the offset preserved, the scan window is roughly [+12095, +12595]
        // Hz so a peak at +12300 falls inside.
        const int N = 1024;
        const double binSpacing = 192000.0 / N;
        QVector<float> bins(N, -120.0f);
        const int peakBin = 512 + static_cast<int>(std::round(+12300.0 / binSpacing));
        bins[peakBin] = -55.0f;

        engine.onSpectrumBinsForMaxBin(0, bins);
        QCOMPARE(engine.getMaxBinDbm(0), -55.0);
    }

    // ── Test 9: zero slice offset matches the legacy DDC-center scan ─────────
    //
    // Default offset is 0 (CTUN-off equivalent: DDC follows slice).  Bin
    // scan behaves identically to the pre-fix code path; no regression.
    void zeroSliceOffsetMatchesLegacyScan() {
        WdspEngine engine;
        engine.setupMaxBinDetector(0, 0, 0, 192000.0, -3000.0, -300.0, 0.5, 60);
        // Offset stays at default 0.

        const int N = 1024;
        const double binSpacing = 192000.0 / N;
        QVector<float> bins(N, -120.0f);
        const int peakBin = 512 + static_cast<int>(std::round(-1500.0 / binSpacing));
        bins[peakBin] = -50.0f;

        engine.onSpectrumBinsForMaxBin(0, bins);
        QCOMPARE(engine.getMaxBinDbm(0), -50.0);
    }
};

QTEST_GUILESS_MAIN(WdspEngineMaxBinTest)
#include "tst_wdsp_engine_max_bin.moc"
