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
#include <limits>

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

// Auto-zoom cap (NereusSDR-original): a time per transform
// (ControlRanges::kAutoZoomMaxTransformSeconds, 0.7 s), so the largest
// power of two <= sampleRate * 0.7 s, within the engine's 1024..262144.
// Slider may go higher manually; auto-zoom won't push above the cap unless
// the slider already did.  Shared with the remote planner.
using ControlRanges::autoZoomMaxFftSize;

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
    return ControlRanges::autoZoomFftSize(planFftSizeFor(target), baseline, sampleRate);
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
    const int cap = autoZoomMaxFftSize(sampleRate);
    while (targetSize < desired && targetSize < cap) {
        targetSize *= 2;
    }
    // Floor at baseline, cap at max(baseline, autoZoomMax(rate)): when the
    // user picks a slider value above the auto-zoom cap, baseline wins
    // (their explicit choice).
    targetSize = ControlRanges::autoZoomFftSize(targetSize, baseline, sampleRate);
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

    // Slider=4096 zoomed until the target reaches the cap for the rate
    // (16x at 96k, 32x at 192k, 64x at 768k).  Past this point K starts
    // dropping (auto-zoom can't push higher; user must move slider manually
    // for more).  The cap is about 0.7 s per transform, so it grows with
    // the rate up to the engine's 262144.
    void formula_caps_at_auto_zoom_max_data()
    {
        QTest::addColumn<double>("sampleRate");
        QTest::addColumn<double>("bwHz");
        QTest::addColumn<int>("size");
        QTest::newRow("96k, 6 kHz visible") << 96000.0 << 6000.0 << 65536;
        QTest::newRow("192k, 6 kHz visible") << 192000.0 << 6000.0 << 131072;
        QTest::newRow("768k, 12 kHz visible") << 768000.0 << 12000.0 << 262144;
    }

    void formula_caps_at_auto_zoom_max()
    {
        QFETCH(double, sampleRate);
        QFETCH(double, bwHz);
        QFETCH(int, size);
        const auto r = computeAutoZoomFftSize(4096, 0, sampleRate, bwHz);
        QCOMPARE(r.size, size);
    }

    // Four times deeper than the cap's zoom: the formula would request
    // four times the cap (or past the engine's 262144); auto-zoom holds at
    // the rate's cap, accepting graceful K degradation past this point.
    // 2026-10-09: the 768k row was 65536 under the fixed cap of 2026-05-08;
    // the time-per-transform cap is 262144 (the engine max) at 768k.
    void formula_holds_at_cap_at_deeper_zoom_data()
    {
        QTest::addColumn<double>("sampleRate");
        QTest::addColumn<double>("bwHz");
        QTest::addColumn<int>("size");
        QTest::newRow("96k, 1.5 kHz visible") << 96000.0 << 1500.0 << 65536;
        QTest::newRow("192k, 1.5 kHz visible") << 192000.0 << 1500.0 << 131072;
        QTest::newRow("768k, 3 kHz visible") << 768000.0 << 3000.0 << 262144;
    }

    void formula_holds_at_cap_at_deeper_zoom()
    {
        QFETCH(double, sampleRate);
        QFETCH(double, bwHz);
        QFETCH(int, size);
        const auto r = computeAutoZoomFftSize(4096, 0, sampleRate, bwHz);
        QCOMPARE(r.size, size);
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
        // sampleRate=96k, bwHz=24k -> scale=4 -> desired=524288.  The
        // loop stops at the 96k cap (65536); upper bound = max(
        // baseline=131072, autoZoomCap(96k)=65536) = 131072.  Final=131072.
        // 2026-10-09: this slot ran at 768k under the fixed cap of
        // 2026-05-08; the 768k cap is now 262144, above this baseline, so
        // the premise (baseline > cap) holds at 96k, and 768k is the
        // next slot.
        const auto r = computeAutoZoomFftSize(131072, 0, 96000.0, 24000.0);
        QCOMPARE(r.size, 131072);
    }

    void formula_baseline_below_rate_cap_grows_to_it()
    {
        // Slider=131072 at 768k: the cap there (262144) is above the
        // baseline, so a 4x zoom (desired 524288) grows to 262144.
        const auto r = computeAutoZoomFftSize(131072, 0, 768000.0, 192000.0);
        QCOMPARE(r.size, 262144);
    }

    // The cap per rate: the largest power of two <= rate * 0.7 s, within
    // [kDisplayFftPlanMinSize, kDisplayFftPlanMaxSize].  A rate that is not
    // finite or not positive gives the minimum.
    void max_fft_size_follows_the_rate_data()
    {
        QTest::addColumn<double>("sampleRate");
        QTest::addColumn<int>("cap");
        QTest::newRow("48 kHz") << 48000.0 << 32768;
        QTest::newRow("96 kHz") << 96000.0 << 65536;
        QTest::newRow("192 kHz") << 192000.0 << 131072;
        QTest::newRow("384 kHz") << 384000.0 << 262144;
        QTest::newRow("768 kHz (engine max)") << 768000.0 << 262144;
        QTest::newRow("1.536 MHz (engine max)") << 1536000.0 << 262144;
        QTest::newRow("1 kHz (plan min)") << 1000.0 << 1024;
        QTest::newRow("zero") << 0.0 << 1024;
        QTest::newRow("negative") << -96000.0 << 1024;
        QTest::newRow("NaN") << std::numeric_limits<double>::quiet_NaN() << 1024;
        QTest::newRow("infinity") << std::numeric_limits<double>::infinity() << 1024;
    }

    void max_fft_size_follows_the_rate()
    {
        QFETCH(double, sampleRate);
        QFETCH(int, cap);
        QCOMPARE(autoZoomMaxFftSize(sampleRate), cap);
    }

    // The shared helper: floor at the baseline, cap at the rate's cap
    // unless the baseline is above it.
    void shared_helper_floors_and_caps()
    {
        QCOMPARE(ControlRanges::kAutoZoomMaxTransformSeconds, 0.7);
        QCOMPARE(ControlRanges::autoZoomFftSize(1024, 4096, 96000.0), 4096);
        QCOMPARE(ControlRanges::autoZoomFftSize(32768, 4096, 96000.0), 32768);
        QCOMPARE(ControlRanges::autoZoomFftSize(65536, 4096, 96000.0), 65536);
        QCOMPARE(ControlRanges::autoZoomFftSize(262144, 4096, 96000.0), 65536);
        QCOMPARE(ControlRanges::autoZoomFftSize(262144, 131072, 96000.0), 131072);
        QCOMPARE(ControlRanges::autoZoomFftSize(1024, 262144, 96000.0), 262144);
        QCOMPARE(ControlRanges::autoZoomFftSize(262144, 4096, 192000.0), 131072);
        QCOMPARE(ControlRanges::autoZoomFftSize(262144, 4096, 768000.0), 262144);
        QCOMPARE(ControlRanges::autoZoomFftSize(262144, 131072, 768000.0), 262144);
        QCOMPARE(ControlRanges::autoZoomFftSize(262144, 4096, 48000.0), 32768);
        static_assert(ControlRanges::autoZoomFftSize(262144, 4096, 96000.0) == 65536);
        static_assert(ControlRanges::autoZoomMaxFftSize(768000.0) == 262144);
    }

    // The remote planner's size and tier ("fine" when the size is above
    // the baseline) on a pan 1068 px wide.
    void remote_planner_caps_zoom_like_local_data()
    {
        QTest::addColumn<double>("sampleRate");
        QTest::addColumn<double>("spanHz");
        QTest::addColumn<double>("hzPerBin");
        QTest::addColumn<int>("baseline");
        QTest::addColumn<int>("size");
        QTest::addColumn<bool>("fine");
        QTest::newRow("300 Hz span: capped, was 262144") << 96000.0 << 300.0 << 0.0 << 4096 << 65536 << true;
        QTest::newRow("96 kHz span: baseline, wide") << 96000.0 << 96000.0 << 0.0 << 4096 << 4096 << false;
        QTest::newRow("5549 Hz span: below the cap") << 96000.0 << 5549.0 << 0.0 << 4096 << 32768 << true;
        QTest::newRow("baseline 131072 wins over the cap") << 96000.0 << 300.0 << 0.0 << 131072 << 131072 << false;
        QTest::newRow("Hz/bin 0.5: capped, was 262144") << 96000.0 << 96000.0 << 0.5 << 4096 << 65536 << true;
        QTest::newRow("768k, 3 kHz span: engine max, was 65536") << 768000.0 << 3000.0 << 0.0 << 4096 << 262144 << true;
        QTest::newRow("192k, 300 Hz span: 131072") << 192000.0 << 300.0 << 0.0 << 4096 << 131072 << true;
    }

    void remote_planner_caps_zoom_like_local()
    {
        QFETCH(double, sampleRate);
        QFETCH(double, spanHz);
        QFETCH(double, hzPerBin);
        QFETCH(int, baseline);
        QFETCH(int, size);
        QFETCH(bool, fine);
        const int planned = remotePlannedFftSize(sampleRate, 1068, spanHz, hzPerBin, baseline);
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
