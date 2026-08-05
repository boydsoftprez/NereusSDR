// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - tst_resampler: pins the Phase 3R Task I2a Resampler
// (r8brain wrapper) port from AetherSDR src/core/Resampler.{h,cpp}
// [@0cd4559].
//
// Five contracts verified:
//
//   1. identity24kTo24k        equal in/out rates produce N output
//                              samples for N input (within r8brain's
//                              internal latency budget)
//   2. downsample24kTo8k       input is ~3x larger than output
//   3. monoToStereoDoubles     stereo output has ~2x the float count
//                              of a mono-of-same-rate run
//   4. stereoToMonoHalves      mono output has ~1/2x the float count
//                              of a stereo-of-same-rate run
//   5. stereoToStereoRoundTrips equal in/out rates produce a stereo
//                              float byte count matching 2 * mono
//
// The r8brain CDSPResampler24 has nontrivial internal latency (a few
// hundred samples for typical rates) so tests check approximate sample
// counts within a tolerance band rather than exact equality. The DSP
// is exercised in N1/N2 bench tests.
//
// See src/core/Resampler.h for the upstream attribution block (verbatim
// AetherSDR project-level header per docs/attribution/HOW-TO-PORT.md
// rule 6).

#include <QtTest/QtTest>
#include <QByteArray>
#include <cmath>
#include <vector>

#include "core/Resampler.h"

using namespace NereusSDR;

namespace {

// Generate N samples of a normalized sine at the given frequency (Hz)
// at the given sample rate. Returns a vector<float> caller can pass to
// process() / processMonoToStereo() / processStereoToMono().
std::vector<float> makeMonoSine(int n, double freqHz, double sampleRateHz)
{
    std::vector<float> v(n);
    const double twoPiF = 2.0 * 3.14159265358979323846 * freqHz / sampleRateHz;
    for (int i = 0; i < n; ++i) {
        v[i] = static_cast<float>(std::sin(twoPiF * i) * 0.5);
    }
    return v;
}

// Generate N stereo frames (2 * N floats) of a normalized sine on both
// channels. Frame layout is interleaved L,R,L,R,...
std::vector<float> makeStereoSine(int nFrames, double freqHz, double sampleRateHz)
{
    std::vector<float> v(nFrames * 2);
    const double twoPiF = 2.0 * 3.14159265358979323846 * freqHz / sampleRateHz;
    for (int i = 0; i < nFrames; ++i) {
        const float s = static_cast<float>(std::sin(twoPiF * i) * 0.5);
        v[2 * i]     = s;
        v[2 * i + 1] = s;
    }
    return v;
}

// Returns true if |a - b| / max(a, b) <= tolerancePct. Used to compare
// expected vs actual resample output sample counts under r8brain's
// internal latency.
bool approxRatio(int actual, int expected, double tolerancePct)
{
    if (expected <= 0) return actual == 0;
    const double diff = std::abs(actual - expected);
    return (diff / static_cast<double>(expected)) <= tolerancePct;
}

}  // namespace

class TestResampler : public QObject {
    Q_OBJECT

private slots:
    void identity24kTo24k();
    void downsample24kTo8k();
    void monoToStereoDoubles();
    void stereoToMonoHalves();
    void stereoToStereoRoundTrips();
};

// 2026-05-12 (PR #238 review follow-up): all five tests below pass
// inputs that exceed Resampler's default maxBlockSamples (4096).
// r8b::CDSPResampler24 pre-allocates internal buffers sized for the
// maxBlockSamples value passed at construction; feeding a process()
// call with more samples than that overruns those buffers and
// trips glibc's heap-corruption sentinel under -D_FORTIFY_SOURCE / a
// debug malloc.  Linux CI surfaced this as "malloc(): invalid size
// (unsorted)" + SIGABRT inside downsample24kTo8k; the 1:1-rate
// tests likely also corrupted heap memory but did not happen to
// trip the sentinel on the macOS / Linux paths that pre-dated CI's
// NEREUS_BUILD_TESTS=ON gate.
//
// Each Resampler ctor below now explicitly sizes maxBlockSamples to
// the per-call input size (or larger) so r8brain's allocations fit.

void TestResampler::identity24kTo24k()
{
    // 1:1 rate. r8brain CDSPResampler24 still applies a polyphase FIR
    // filter so the first call swallows several hundred samples of
    // filter latency. We feed a 4-second buffer (96000 samples) so
    // latency amortizes to less than 5% of the total.
    constexpr int kInputSamples = 96000;
    Resampler r(24000, 24000, kInputSamples);
    auto in = makeMonoSine(kInputSamples, 1000.0, 24000.0);

    QByteArray out = r.process(in.data(), static_cast<int>(in.size()));
    const int outSamples = out.size() / static_cast<int>(sizeof(float));

    QVERIFY2(approxRatio(outSamples, 96000, 0.05),
             qPrintable(QString("identity24k: got %1 samples, expected ~96000")
                            .arg(outSamples)));
}

void TestResampler::downsample24kTo8k()
{
    // 3:1 rate ratio. r8brain's downsample latency at this ratio is
    // ~2400 samples at the OUTPUT rate (one full FIR-kernel worth);
    // even at a 96000-sample input (32000 expected output) that's
    // about 7-8% of the output count. We use a 10% tolerance.
    constexpr int kInputSamples = 96000;
    Resampler r(24000, 8000, kInputSamples);
    auto in = makeMonoSine(kInputSamples, 1000.0, 24000.0);

    QByteArray out = r.process(in.data(), static_cast<int>(in.size()));
    const int outSamples = out.size() / static_cast<int>(sizeof(float));

    QVERIFY2(approxRatio(outSamples, 32000, 0.10),
             qPrintable(QString("downsample24kTo8k: got %1 samples, expected ~32000")
                            .arg(outSamples)));
}

void TestResampler::monoToStereoDoubles()
{
    // processMonoToStereo at 1:1 rate. Output should be ~N stereo frames
    // = 2N floats for N input samples. Compare byte count.
    constexpr int kInputSamples = 16000;
    Resampler r(16000, 16000, kInputSamples);
    auto monoIn = makeMonoSine(kInputSamples, 1000.0, 16000.0);

    QByteArray monoOut = r.process(monoIn.data(), static_cast<int>(monoIn.size()));
    const int monoOutSamples = monoOut.size() / static_cast<int>(sizeof(float));

    Resampler r2(16000, 16000, kInputSamples);
    QByteArray stereoOut = r2.processMonoToStereo(monoIn.data(), static_cast<int>(monoIn.size()));
    const int stereoOutFloats = stereoOut.size() / static_cast<int>(sizeof(float));

    QVERIFY2(stereoOutFloats == 2 * monoOutSamples,
             qPrintable(QString("monoToStereo: got %1 floats, expected 2 * %2 = %3")
                            .arg(stereoOutFloats).arg(monoOutSamples).arg(2 * monoOutSamples)));
}

void TestResampler::stereoToMonoHalves()
{
    // processStereoToMono at 1:1 rate. N stereo frames in (2N floats),
    // ~N mono samples out. Mono output count matches the mono-input
    // path's output count.
    constexpr int nFrames = 16000;
    auto stereoIn = makeStereoSine(nFrames, 1000.0, 16000.0);
    auto monoIn = makeMonoSine(nFrames, 1000.0, 16000.0);

    Resampler r(16000, 16000, nFrames);
    QByteArray monoOut = r.process(monoIn.data(), nFrames);
    const int monoOutSamples = monoOut.size() / static_cast<int>(sizeof(float));

    Resampler r2(16000, 16000, nFrames);
    QByteArray downmixOut = r2.processStereoToMono(stereoIn.data(), nFrames);
    const int downmixOutSamples = downmixOut.size() / static_cast<int>(sizeof(float));

    QVERIFY2(downmixOutSamples == monoOutSamples,
             qPrintable(QString("stereoToMono: got %1 samples, expected %2 (matching mono path)")
                            .arg(downmixOutSamples).arg(monoOutSamples)));
}

void TestResampler::stereoToStereoRoundTrips()
{
    // processStereoToStereo at 1:1 rate. N stereo frames in, ~N stereo
    // frames out (2N floats), matching the byte count of processStereoTo
    // Mono * 2.
    constexpr int nFrames = 16000;
    auto stereoIn = makeStereoSine(nFrames, 1000.0, 16000.0);

    Resampler r(16000, 16000, nFrames);
    QByteArray monoOut = r.processStereoToMono(stereoIn.data(), nFrames);
    const int monoOutSamples = monoOut.size() / static_cast<int>(sizeof(float));

    Resampler r2(16000, 16000, nFrames);
    QByteArray stereoOut = r2.processStereoToStereo(stereoIn.data(), nFrames);
    const int stereoOutFloats = stereoOut.size() / static_cast<int>(sizeof(float));

    QVERIFY2(stereoOutFloats == 2 * monoOutSamples,
             qPrintable(QString("stereoToStereo: got %1 floats, expected 2 * %2 = %3")
                            .arg(stereoOutFloats).arg(monoOutSamples).arg(2 * monoOutSamples)));
}

QTEST_GUILESS_MAIN(TestResampler)
#include "tst_resampler.moc"
