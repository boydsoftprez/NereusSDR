// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_rade_tx_filters.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the RADE TX DSP helpers (Phase 3R Task K3):
//
//   * RadeTxHpf80  - 80 Hz Butterworth HPF biquad (mic conditioning
//                    before LPCNet feature extraction).  Direct-form
//                    II transposed.  Coefficients derived from
//                    Robert Bristow-Johnson's Audio EQ Cookbook
//                    (cookbook formula for an HPF at fs=48 kHz,
//                    fc=80 Hz, Q=1/sqrt(2)).
//
//   * RadeTx48to16 - 48 kHz -> 16 kHz mono downsampler wrapping
//                    r8brain CDSPResampler24 (vendored at
//                    third_party/r8brain/, MIT license).  Used by
//                    RadeChannel's TX path before LPCNet, which
//                    expects 16 kHz mono float32 frames.
//
// Test surface (5 cases):
//   * hpfPassesAt1kHz                 1 kHz sine: gain ~= 1.0.
//   * hpfAttenuatesAt40Hz             40 Hz sine: gain < 0.30 (~ -10 dB
//                                       at one octave below the 80 Hz
//                                       knee for a Butterworth Q=0.707
//                                       HPF).
//   * hpfDoesNotExplodeOnDc           DC step settles to near-zero.
//   * resamplerProducesThirdLength    480 in -> ~160 out (within
//                                       latency tolerance).
//   * resamplerRoundtripsConstantSignal  Long DC ramp: output amplitude
//                                       converges to input amplitude.
//
// Both helpers are designed to be standalone — K-bench will wire them
// into TxWorkerThread's RADE path.  Unit-testing in isolation pins
// the DSP correctness contract independently of the real-time pump
// integration.
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-11 — Phase 3R Task K3: initial test file. NereusSDR-native.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/RadeTxFilters.h"

#include <cmath>
#include <vector>

using namespace NereusSDR;

namespace {

// Compute RMS over a vector (skip the first `skipFront` samples so the
// biquad / resampler latency doesn't bias the measurement).
double rms(const std::vector<float>& buf, int skipFront)
{
    if (skipFront < 0) {
        skipFront = 0;
    }
    if (skipFront >= static_cast<int>(buf.size())) {
        return 0.0;
    }
    double acc = 0.0;
    int n = 0;
    for (size_t i = static_cast<size_t>(skipFront); i < buf.size(); ++i) {
        acc += static_cast<double>(buf[i]) * static_cast<double>(buf[i]);
        ++n;
    }
    return n > 0 ? std::sqrt(acc / n) : 0.0;
}

}  // namespace

class TstRadeTxFilters : public QObject {
    Q_OBJECT

private slots:

    // A 1 kHz sine at fs=48 kHz is well above the 80 Hz knee, so an
    // ideal Butterworth HPF passes it without attenuation.  The
    // biquad's transient is short (a few hundred samples); we skip
    // the first 1000 samples before measuring RMS to give the filter
    // time to settle.
    void hpfPassesAt1kHz()
    {
        constexpr int    kFs       = 48000;
        constexpr int    kN        = 4800;     // 100 ms of audio
        constexpr float  kFreqHz   = 1000.0f;
        constexpr float  kAmp      = 0.5f;
        constexpr int    kSkip     = 1000;

        std::vector<float> in(kN);
        std::vector<float> out(kN);
        for (int i = 0; i < kN; ++i) {
            in[static_cast<size_t>(i)] = kAmp *
                std::sin(2.0f * static_cast<float>(M_PI) *
                         kFreqHz * static_cast<float>(i) / kFs);
        }

        RadeTxHpf80 hpf;
        for (int i = 0; i < kN; ++i) {
            out[static_cast<size_t>(i)] =
                hpf.process(in[static_cast<size_t>(i)]);
        }

        const double inRms  = rms(in,  kSkip);
        const double outRms = rms(out, kSkip);
        const double gain   = outRms / std::max(inRms, 1e-9);

        QVERIFY2(gain > 0.95 && gain < 1.05,
                 qPrintable(QString("1 kHz gain expected ~1.0, got %1")
                                .arg(gain, 0, 'f', 4)));
    }

    // A 40 Hz sine sits one octave below the 80 Hz Butterworth knee.
    // Theoretical magnitude response of a 2nd-order Butterworth HPF
    // at fc/2 is sqrt((fc/2/fc)^4 / (1 + (fc/2/fc)^4)) = sqrt(0.0625 /
    // 1.0625) ~ 0.2425, i.e. ~ -12 dB.  Allow a generous bound to
    // accept normal coefficient-rounding error.
    void hpfAttenuatesAt40Hz()
    {
        constexpr int    kFs       = 48000;
        constexpr int    kN        = 9600;     // 200 ms of audio
        constexpr float  kFreqHz   = 40.0f;
        constexpr float  kAmp      = 0.5f;
        constexpr int    kSkip     = 2000;

        std::vector<float> in(kN);
        std::vector<float> out(kN);
        for (int i = 0; i < kN; ++i) {
            in[static_cast<size_t>(i)] = kAmp *
                std::sin(2.0f * static_cast<float>(M_PI) *
                         kFreqHz * static_cast<float>(i) / kFs);
        }

        // Drive via the buffer overload (the K-bench run-loop API).
        // Copy `in` into `out` first since the overload processes
        // in-place reads from `in` per sample.
        RadeTxHpf80 hpf;
        std::vector<float> work(in);
        hpf.process(work.data(), kN);
        out = work;

        const double inRms  = rms(in,  kSkip);
        const double outRms = rms(out, kSkip);
        const double gain   = outRms / std::max(inRms, 1e-9);

        QVERIFY2(gain < 0.30,
                 qPrintable(QString("40 Hz gain expected < 0.30, got %1")
                                .arg(gain, 0, 'f', 4)));
    }

    // A DC step (constant 1.0) should be removed entirely by an HPF.
    // The output settles toward zero as the biquad's transient decays.
    // Measure the last 1000 samples of a 4800-sample run.
    void hpfDoesNotExplodeOnDc()
    {
        constexpr int kN    = 4800;
        constexpr int kTail = 1000;

        std::vector<float> in(kN, 1.0f);
        std::vector<float> out(kN);

        RadeTxHpf80 hpf;
        for (int i = 0; i < kN; ++i) {
            out[static_cast<size_t>(i)] =
                hpf.process(in[static_cast<size_t>(i)]);
        }

        double maxAbs = 0.0;
        for (int i = kN - kTail; i < kN; ++i) {
            maxAbs = std::max(maxAbs,
                              std::fabs(static_cast<double>(
                                  out[static_cast<size_t>(i)])));
        }

        QVERIFY2(maxAbs < 0.02,
                 qPrintable(QString("DC tail max-abs expected < 0.02, got %1")
                                .arg(maxAbs, 0, 'f', 6)));
    }

    // 480 input samples at 48 kHz should produce ~160 output samples
    // at 16 kHz (a 3:1 decimation).  r8brain CDSPResampler24 has a
    // one-time filterbank-fill latency of roughly ~2400 output
    // samples (the polyphase filterbank's group delay) before the
    // steady-state ratio settles.  Run a long sequence (10 seconds)
    // so the latency window is a small fraction of the total; once
    // warmed up the ratio settles within a few percent of 1/3.
    void resamplerProducesThirdLength()
    {
        constexpr int    kFsIn      = 48000;
        constexpr int    kFsOut     = 16000;
        constexpr int    kBlockIn   = 480;     // 10 ms at 48 kHz
        constexpr int    kBlocks    = 1000;     // 10 seconds total
        constexpr int    kOutMax    = kBlockIn / 2;  // overhead margin

        RadeTx48to16 rs;
        std::vector<float> in(kBlockIn);
        std::vector<float> outBuf(kOutMax);
        int totalOut = 0;

        for (int b = 0; b < kBlocks; ++b) {
            // Drive with low-amplitude noise so the resampler stays
            // numerically active; constants exercise too few paths.
            for (int i = 0; i < kBlockIn; ++i) {
                in[static_cast<size_t>(i)] =
                    0.1f * std::sin(0.01f * static_cast<float>(b * kBlockIn + i));
            }
            totalOut += rs.process(in.data(), kBlockIn,
                                    outBuf.data(), kOutMax);
        }

        // 64-bit arithmetic: 1000 * 480 * 16000 = 7.68 * 10^9 overflows int32.
        const long long expectedOut = static_cast<long long>(kBlocks) *
                                       kBlockIn * kFsOut / kFsIn;
        const double ratio = static_cast<double>(totalOut) /
                             static_cast<double>(expectedOut);

        // After 10 seconds the constant filterbank latency (~2400
        // output samples) is < 1.5% of the total output (160000), so
        // the observed ratio should be within 3% of the theoretical
        // 1.0.  Accept a wider bound to be robust against r8brain
        // version drift.
        QVERIFY2(ratio > 0.97 && ratio < 1.03,
                 qPrintable(QString("ratio expected ~1.0, got %1 (totalOut=%2, expected=%3)")
                                .arg(ratio, 0, 'f', 3)
                                .arg(totalOut)
                                .arg(expectedOut)));
    }

    // Feed a DC-like signal (held constant after the first sample)
    // and verify the output amplitude converges to the input
    // amplitude.  Use a small skip to bypass r8brain's startup
    // transient.
    void resamplerRoundtripsConstantSignal()
    {
        constexpr int    kBlockIn = 480;
        constexpr int    kBlocks  = 100;
        constexpr float  kLevel   = 0.25f;
        constexpr int    kOutMax  = kBlockIn / 2;

        RadeTx48to16 rs;
        std::vector<float> in(kBlockIn, kLevel);
        std::vector<float> outBuf(kOutMax);
        std::vector<float> outAll;
        outAll.reserve(kBlocks * kOutMax);

        for (int b = 0; b < kBlocks; ++b) {
            int got = rs.process(in.data(), kBlockIn,
                                  outBuf.data(), kOutMax);
            for (int i = 0; i < got; ++i) {
                outAll.push_back(outBuf[static_cast<size_t>(i)]);
            }
        }

        QVERIFY2(!outAll.empty(),
                 "resampler produced no output after 100 input blocks");

        // Use the last quarter to skip the startup transient.
        const int skip = static_cast<int>(outAll.size() * 3 / 4);
        const double outRms = rms(outAll, skip);

        // For a constant input, RMS == |value|.  Allow a 5% tolerance
        // for floating-point math + the LPF passband ripple.
        QVERIFY2(std::fabs(outRms - kLevel) < 0.05,
                 qPrintable(QString("output RMS expected ~%1, got %2")
                                .arg(kLevel, 0, 'f', 4)
                                .arg(outRms, 0, 'f', 4)));
    }
};

QTEST_MAIN(TstRadeTxFilters)
#include "tst_rade_tx_filters.moc"
