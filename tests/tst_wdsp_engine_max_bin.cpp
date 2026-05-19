// no-port-check: NereusSDR-original unit-test file. Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_wdsp_engine_max_bin.cpp  (NereusSDR)
// =================================================================
//
// Smoke tests for Phase 3P-II Phase 2 Tasks 31-32:
//   WdspEngine::getRxaSignalAverage (Task 31, RXA_S_AV wrapper)
//   WdspEngine::setupMaxBinDetector (Task 32, SetupDetectMaxBin wrapper)
//   WdspEngine::getMaxBinDbm        (Task 32, GetDetectMaxBin wrapper)
//
// Contract verified:
//   1. getRxaSignalAverage is callable without crashing; returns a
//      sentinel (not finite is acceptable before channel activation).
//   2. setupMaxBinDetector is callable with default args; no crash, no
//      exception.
//   3. getMaxBinDbm is callable without crashing; returns a finite value
//      (the WDSP sentinel -400.0 or the stub -400.0 from the non-HAVE_WDSP
//      path both satisfy std::isfinite).
//
// Initialization strategy -- Option 3 (no WdspEngine::initialize):
//
//   WdspEngine::initialize() triggers async FFTW wisdom generation
//   (30-60 s on first run) and is unsuitable for a unit test.  The
//   wrappers added in Tasks 31-32 are thin facades that forward to WDSP
//   C functions; their correctness is tested here at the API boundary,
//   not at the DSP numerical level.
//
//   Each wrapper checks m_initialized before calling any WDSP function,
//   so a default-constructed WdspEngine (m_initialized = false) is always
//   safe to call.  This covers both HAVE_WDSP and non-HAVE_WDSP builds
//   without needing the async wisdom path or CreateAnalyzer.
//
//   Full DSP numerical behavior is covered at bench time with a real WDSP
//   wisdom file and an open display channel.
//
// Source references:
//   Thetis Console/dsp.cs:387-388 [@501e3f5]  - GetRXAMeter P/Invoke
//   Thetis Console/console.cs:957 [@501e3f5]   - RXA_S_AV selector
//   Thetis Console/dsp.cs:846-850 [@501e3f5]  - SetupDetectMaxBin / GetDetectMaxBin P/Invoke
//   Thetis wdsp/analyzer.c:775+830 [@501e3f5] - C implementation
//   Thetis wdsp/analyzer.c:1442 [@501e3f5]    - developer example (default values)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19 - New test file for Phase 3P-II Phase 2 Tasks 31-32:
//                WdspEngine MaxBin detector wrappers smoke test.
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
    // initialized (m_initialized guard fires before any WDSP call, regardless
    // of whether HAVE_WDSP is defined).  The sentinel is finite.
    //
    // From Thetis Console/dsp.cs:387-388 [@501e3f5] (P/Invoke)
    // From Thetis Console/console.cs:957 [@501e3f5] (RXA_S_AV selector)
    void getRxaSignalAverage_doesNotCrash() {
        WdspEngine engine;
        // m_initialized is false -- the guard in getRxaSignalAverage fires
        // and returns -140.0 before touching any WDSP channel state.
        double result = engine.getRxaSignalAverage(0);
        // -140.0 is finite and within the expected dBm sentinel range.
        QVERIFY(std::isfinite(result));
    }

    // ── Test 2: setupMaxBinDetector with defaults does not crash ─────────────
    //
    // WdspEngine::setupMaxBinDetector returns early when the engine is not
    // initialized (m_initialized guard fires before calling SetupDetectMaxBin,
    // which requires pdisp[disp] to be non-null from CreateAnalyzer).
    // Calling with a default-constructed engine must be a safe no-op.
    //
    // From Thetis Console/dsp.cs:846-847 [@501e3f5] (P/Invoke)
    // From Thetis wdsp/analyzer.c:775 [@501e3f5] (DSP body)
    // From Thetis wdsp/analyzer.c:1442 [@501e3f5] (developer example defaults)
    void setupMaxBinDetector_doesNotCrash() {
        WdspEngine engine;
        // m_initialized is false -- early return before any pdisp[] access.
        engine.setupMaxBinDetector(0);  // defaults: rate=192000, fLow=-3000, fHigh=-300, tau=0.5, fps=60
        QVERIFY(true);  // no crash + no exception
    }

    // ── Test 3: getMaxBinDbm returns a finite value ───────────────────────────
    //
    // WdspEngine::getMaxBinDbm returns -400.0 when the engine is not
    // initialized (m_initialized guard, same pattern as getRxaSignalAverage).
    // -400.0 matches WDSP's dmb_max_dB initial value at analyzer.c:703.
    // std::isfinite(-400.0) is true.
    //
    // From Thetis Console/dsp.cs:849-850 [@501e3f5] (P/Invoke)
    // From Thetis wdsp/analyzer.c:830 [@501e3f5] (DSP body)
    // From Thetis wdsp/analyzer.c:703 [@501e3f5] (dmb_max_dB = -400.0 init)
    void getMaxBinDbm_returnsFiniteValue() {
        WdspEngine engine;
        // m_initialized is false -- early return with -400.0 sentinel.
        double dbm = engine.getMaxBinDbm(0);
        QVERIFY(std::isfinite(dbm));
    }
};

QTEST_APPLESS_MAIN(WdspEngineMaxBinTest)
#include "tst_wdsp_engine_max_bin.moc"
