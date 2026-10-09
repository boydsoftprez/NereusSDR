// no-port-check: unit tests for NereusSDR-original auto-zoom math;
// no Thetis logic ported (auto-zoom has no Thetis equivalent).
//
// =================================================================
// tests/tst_auto_zoom_fft.cpp  (NereusSDR)
// =================================================================
//
// Phase 2-polish-3 -- exercise the auto-zoom math that scales FFT
// size to maintain constant bins-per-pixel across zoom levels.
//
// Asserts:
//   1. setFftSizeBaseline / fftSizeBaseline round-trip across all
//      slider-valid values (1024..262144 power-of-two).
//   2. Invalid sizes (out of range, non-power-of-two) rejected.
//   3. Default baseline matches default fftSize (4096).
//
// NB: the auto-zoom formula and hysteresis live in MainWindow's
// bandwidthChangeRequested lambda (cannot test in headless mode
// without instantiating MainWindow + signal wiring).  This test
// covers the FFTEngine-side state API; the math is tested via the
// helper computeAutoZoomFftSize() defined inline below, which
// duplicates the lambda's algorithm so changes to either MUST be
// kept in sync.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 -- Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                  (KG4VCF), with AI-assisted transformation via
//                  Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <algorithm>

#include "core/ControlRanges.h"
#include "core/FFTEngine.h"

using namespace NereusSDR;

namespace {

// Mirror of the auto-zoom math in MainWindow's bandwidthChangeRequested
// lambda.  Keep in sync with src/gui/MainWindow.cpp.
struct AutoZoomResult {
    int  size;       // computed target FFT size
    bool replanned;  // true when ratio fell outside hysteresis band
};

// Auto-zoom cap (NereusSDR-original): bounds the buffer-fill pause on
// every replan to ~85 ms at 768 kHz DDC.  Slider may go higher manually
// (up to FFTEngine::kMaxFftSize=262144); auto-zoom won't push above the
// cap unless the slider already did.  Shared with the remote planner.
using ControlRanges::kAutoZoomMaxFftSize;

// Mirror of fftSizeFor in src/gui/RemoteMediaController.cpp: the smallest
// power of two from kDisplayFftPlanMinSize that reaches `target`, at most
// kDisplayFftPlanMaxSize.
int planFftSizeFor(double target)
{
    int size = ControlRanges::kDisplayFftPlanMinSize;
    while (size < ControlRanges::kDisplayFftPlanMaxSize && size < target) {
        size *= 2;
    }
    return size;
}

// Mirror of RemoteMediaController's plannedFftSize: the zoom term
// (sample rate x pixels / span) and the Hz/bin term, through the shared
// ControlRanges::autoZoomFftSize.
int remotePlannedFftSize(double sampleRate, int pixels, double spanHz,
                         double hzPerBinTarget, int baseline)
{
    double target = sampleRate * pixels / spanHz;
    if (hzPerBinTarget > 0.0) {
        target = std::max(target, sampleRate / hzPerBinTarget);
    }
    return ControlRanges::autoZoomFftSize(planFftSizeFor(target), baseline);
}

AutoZoomResult computeAutoZoomFftSize(int baseline,
                                      int currentSize,
                                      double sampleRate,
                                      double bwHz)
{
    AutoZoomResult r{currentSize, false};
    if (sampleRate <= 0.0 || bwHz <= 0.0) { return r; }
    const double scale = sampleRate / bwHz;
    double desired = static_cast<double>(baseline) * scale;
    int targetSize = 1024;
    while (targetSize < desired && targetSize < kAutoZoomMaxFftSize) {
        targetSize *= 2;
    }
    // Floor at baseline, cap at max(baseline, autoZoomMax): when the user
    // picks a slider value above the auto-zoom cap, baseline wins (their
    // explicit choice).
    targetSize = ControlRanges::autoZoomFftSize(targetSize, baseline);
    if (currentSize > 0) {
        const double ratio = static_cast<double>(targetSize)
                             / static_cast<double>(currentSize);
        if (ratio > 0.66 && ratio < 1.5) {
            return r;  // hysteresis: no replan
        }
    }
    r.size = targetSize;
    r.replanned = (targetSize != currentSize);
    return r;
}

}  // namespace

class TestAutoZoomFft : public QObject
{
    Q_OBJECT

private slots:

    void baseline_default_matches_initial_fft_size()
    {
        FFTEngine fe(/*receiverId=*/0);
        QCOMPARE(fe.fftSizeBaseline(), 4096);
        QCOMPARE(fe.fftSize(),         4096);
    }

    void baseline_round_trip_for_all_slider_values()
    {
        FFTEngine fe(0);
        for (int v = 0; v <= 6; ++v) {
            const int size = 4096 << v;
            fe.setFftSizeBaseline(size);
            QCOMPARE(fe.fftSizeBaseline(), size);
        }
    }

    void baseline_rejects_out_of_range()
    {
        FFTEngine fe(0);
        fe.setFftSizeBaseline(8192);
        QCOMPARE(fe.fftSizeBaseline(), 8192);
        fe.setFftSizeBaseline(512);            // below kMinFftSize
        QCOMPARE(fe.fftSizeBaseline(), 8192);  // unchanged
        fe.setFftSizeBaseline(524288);         // above kMaxFftSize
        QCOMPARE(fe.fftSizeBaseline(), 8192);  // unchanged
    }

    void baseline_rejects_non_power_of_two()
    {
        FFTEngine fe(0);
        fe.setFftSizeBaseline(8192);
        QCOMPARE(fe.fftSizeBaseline(), 8192);
        fe.setFftSizeBaseline(7000);
        QCOMPARE(fe.fftSizeBaseline(), 8192);  // unchanged
        fe.setFftSizeBaseline(12288);
        QCOMPARE(fe.fftSizeBaseline(), 8192);  // unchanged
    }

    // Auto-zoom formula: targetFftSize = baseline * (sampleRate/bwHz),
    // clamped to [baseline, 262144], next pow2.  At full DDC bandwidth
    // (bwHz == sampleRate) the target equals the baseline exactly.
    void formula_at_full_bandwidth_returns_baseline()
    {
        const auto r = computeAutoZoomFftSize(/*baseline=*/4096,
                                              /*current=*/0,
                                              /*sampleRate=*/768000.0,
                                              /*bwHz=*/768000.0);
        QCOMPARE(r.size, 4096);
    }

    void formula_at_half_bandwidth_doubles()
    {
        const auto r = computeAutoZoomFftSize(4096, 0, 768000.0, 384000.0);
        QCOMPARE(r.size, 8192);
    }

    void formula_at_quarter_bandwidth_quadruples()
    {
        // Default zoom (768k / 192k = 4x): slider=4096 -> FFT=16384.
        const auto r = computeAutoZoomFftSize(4096, 0, 768000.0, 192000.0);
        QCOMPARE(r.size, 16384);
    }

    void formula_caps_at_auto_zoom_max()
    {
        // Slider=4096 + 16x zoom (48 kHz visible at 768k DDC) -> hits cap
        // exactly at 65536.  Past this point K starts dropping (auto-zoom
        // can't push higher; user must move slider manually for more).
        const auto r = computeAutoZoomFftSize(4096, 0, 768000.0, 48000.0);
        QCOMPARE(r.size, 65536);
    }

    void formula_holds_at_cap_at_deeper_zoom()
    {
        // Slider=4096 + 64x zoom (12 kHz visible at 768k DDC).  Without
        // a cap the formula would request 262144; auto-zoom cap pins it
        // at 65536, accepting graceful K degradation past this point in
        // exchange for a sub-100 ms replan pause.
        const auto r = computeAutoZoomFftSize(4096, 0, 768000.0, 12000.0);
        QCOMPARE(r.size, 65536);
    }

    void formula_baseline_above_cap_is_authoritative()
    {
        // Slider=131072 (manually set above the auto-zoom cap of 65536):
        // auto-zoom respects baseline and never reduces below it.  This
        // is the "user opted in to large FFT" path -- they accepted the
        // longer one-time pause when they moved the slider.
        const auto r = computeAutoZoomFftSize(131072, 0, 192000.0, 768000.0);
        QCOMPARE(r.size, 131072);
    }

    void formula_baseline_above_cap_grows_with_zoom()
    {
        // Slider=131072, zoom in (bwHz < sampleRate): target ramps above
        // baseline up to the implied K, with the *baseline* defining the
        // upper bound (not the cap, since baseline > cap).
        // sampleRate=768k, bwHz=192k -> scale=4 -> desired=524288 -> next
        // pow2=524288 (above kMaxFftSize=262144).  Upper bound = max(
        // baseline=131072, autoZoomCap=65536) = 131072.  Final=131072.
        const auto r = computeAutoZoomFftSize(131072, 0, 768000.0, 192000.0);
        QCOMPARE(r.size, 131072);
    }

    // The shared helper: floor at the baseline, cap at 65536 unless the
    // baseline is above it.
    void shared_helper_floors_and_caps()
    {
        QCOMPARE(kAutoZoomMaxFftSize, 65536);
        QCOMPARE(ControlRanges::autoZoomFftSize(1024, 4096), 4096);
        QCOMPARE(ControlRanges::autoZoomFftSize(32768, 4096), 32768);
        QCOMPARE(ControlRanges::autoZoomFftSize(65536, 4096), 65536);
        QCOMPARE(ControlRanges::autoZoomFftSize(262144, 4096), 65536);
        QCOMPARE(ControlRanges::autoZoomFftSize(262144, 131072), 131072);
        QCOMPARE(ControlRanges::autoZoomFftSize(1024, 262144), 262144);
        static_assert(ControlRanges::autoZoomFftSize(262144, 4096) == 65536);
    }

    // The remote planner's size and tier ("fine" when the size is above
    // the baseline) for a 96 kHz receiver on a pan 1068 px wide.
    void remote_planner_caps_zoom_like_local_data()
    {
        QTest::addColumn<double>("spanHz");
        QTest::addColumn<double>("hzPerBin");
        QTest::addColumn<int>("baseline");
        QTest::addColumn<int>("size");
        QTest::addColumn<bool>("fine");
        QTest::newRow("300 Hz span: capped, was 262144") << 300.0 << 0.0 << 4096 << 65536 << true;
        QTest::newRow("96 kHz span: baseline, wide") << 96000.0 << 0.0 << 4096 << 4096 << false;
        QTest::newRow("5549 Hz span: below the cap") << 5549.0 << 0.0 << 4096 << 32768 << true;
        QTest::newRow("baseline 131072 wins over the cap") << 300.0 << 0.0 << 131072 << 131072 << false;
        QTest::newRow("Hz/bin 0.5: capped, was 262144") << 96000.0 << 0.5 << 4096 << 65536 << true;
    }

    void remote_planner_caps_zoom_like_local()
    {
        QFETCH(double, spanHz);
        QFETCH(double, hzPerBin);
        QFETCH(int, baseline);
        QFETCH(int, size);
        QFETCH(bool, fine);
        const int planned = remotePlannedFftSize(96000.0, 1068, spanHz, hzPerBin, baseline);
        QCOMPARE(planned, size);
        QCOMPARE(planned > baseline, fine);
    }

    // Hysteresis: small bandwidth change relative to current FFT size
    // does not trigger replan.  Bracket: ratio in (0.66, 1.5).
    void hysteresis_skips_replan_when_target_close_to_current()
    {
        // baseline=4096, current=16384, sampleRate=768k.  bwHz=192k
        // gives target=16384 (same as current); zero replan needed.
        const auto r = computeAutoZoomFftSize(4096, 16384, 768000.0, 192000.0);
        QVERIFY(!r.replanned);
        QCOMPARE(r.size, 16384);
    }

    void hysteresis_replans_when_target_doubles()
    {
        // current=16384, bwHz=96k -> target=32768.  Ratio 2.0, outside
        // (0.66, 1.5), so replan.
        const auto r = computeAutoZoomFftSize(4096, 16384, 768000.0, 96000.0);
        QVERIFY(r.replanned);
        QCOMPARE(r.size, 32768);
    }

    void hysteresis_replans_when_target_halves()
    {
        // current=32768, bwHz=192k -> target=16384.  Ratio 0.5, outside
        // band, so replan.
        const auto r = computeAutoZoomFftSize(4096, 32768, 768000.0, 192000.0);
        QVERIFY(r.replanned);
        QCOMPARE(r.size, 16384);
    }

    // Defensive: zero / negative inputs short-circuit without crashing.
    void defensive_zero_sample_rate_is_noop()
    {
        const auto r = computeAutoZoomFftSize(4096, 8192, 0.0, 192000.0);
        QVERIFY(!r.replanned);
        QCOMPARE(r.size, 8192);
    }

    void defensive_zero_bandwidth_is_noop()
    {
        const auto r = computeAutoZoomFftSize(4096, 8192, 768000.0, 0.0);
        QVERIFY(!r.replanned);
        QCOMPARE(r.size, 8192);
    }
};

QTEST_APPLESS_MAIN(TestAutoZoomFft)
#include "tst_auto_zoom_fft.moc"
