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
//                decaysWithoutNewPeaks, clampsBinRangeToArrayBounds.
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
    // From Thetis Console/console.cs:957 [@501e3f5] (RXA_S_AV selector)
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

    // ── Test 5: decaysWithoutNewPeaks ────────────────────────────────────────
    //
    // After a peak is established, frames with truly silent bins (-400 dBm)
    // let maxDb drift away from zero (slow release).  Then a new peak replaces.
    //
    // From Thetis wdsp/analyzer.c:815-818 [@501e3f5]:
    //   a->dmb_max_dB -= fabs((1.0 - a->dmb_decay) * a->dmb_max_dB);
    //   if (dmb_max_dB > a->dmb_max_dB) a->dmb_max_dB = dmb_max_dB;
    //
    // The drift direction is away from zero (more negative), not toward zero.
    // Important: if "quiet" bins are at -120 the peak-attack fires once maxDb
    // drifts below -120, clamping the decay at -120.  For a true decay test
    // we use -400 dBm (the WDSP sentinel) so no peak attack fires during the
    // quiet phase.
    //
    // With fps=60, tau=0.5s, decay~0.9672, per-frame drift factor = 3.28%.
    // After 60 silent frames starting from -50: maxDb ~ -346 (verified
    // analytically).  We assert < -200 with margin.
    void decaysWithoutNewPeaks() {
        WdspEngine engine;
        engine.setupMaxBinDetector(0, 0, 0, 192000.0, -3000.0, -300.0, 0.5, 60);

        const int N = 1024;
        // Peak frame: all bins at -400 sentinel except the peak bin.
        QVector<float> peakBins(N, -400.0f);
        const double binSpacing = 192000.0 / N;
        const int peakBin = 512 + static_cast<int>(std::round(-1500.0 / binSpacing));
        peakBins[peakBin] = -50.0f;

        // Drive one peak frame to establish maxDb = -50.
        engine.onSpectrumBinsForMaxBin(0, peakBins);
        QVERIFY(qFuzzyCompare(engine.getMaxBinDbm(0), -50.0));

        // Drive 60 frames at the -400 sentinel (truly silent -- no peak attack
        // fires so the slow-release drift is unobstructed).
        QVector<float> silentBins(N, -400.0f);
        for (int i = 0; i < 60; ++i) {
            engine.onSpectrumBinsForMaxBin(0, silentBins);
        }
        // After 60 silent frames from -50, decay ~ (1.03279)^60 ~ 7.15,
        // so maxDb ~ -50 * 7.15 = -357.  Assert well below -200 with margin.
        QVERIFY(engine.getMaxBinDbm(0) < -200.0);

        // Then drive a new peak at -55: fast attack replaces immediately.
        QVector<float> peak2Bins(N, -400.0f);
        peak2Bins[peakBin] = -55.0f;
        engine.onSpectrumBinsForMaxBin(0, peak2Bins);
        QVERIFY(qFuzzyCompare(engine.getMaxBinDbm(0), -55.0));
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
};

QTEST_GUILESS_MAIN(WdspEngineMaxBinTest)
#include "tst_wdsp_engine_max_bin.moc"
