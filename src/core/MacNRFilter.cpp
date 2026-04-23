// =================================================================
// src/core/MacNRFilter.cpp  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR src/core/MacNRFilter.{h,cpp} [@0cd4559].
// AetherSDR has no per-file headers — project-level license applies
// (GPLv3 per https://github.com/ten9876/AetherSDR).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — Imported from AetherSDR and MODIFIED for 48 kHz
//                stereo float native operation. AetherSDR's MNR ran
//                at 24 kHz natively; NereusSDR's post-fexchange2
//                audio path is 48 kHz so we:
//                  * LOG2N 9 → 10 (FFT size 512 → 1024)
//                  * H 256 → 512  (hop size doubles with FFT size)
//                  * All N/H references update automatically via the
//                    constexpr constants in MacNRFilter.h
//                  * process() signature changed from QByteArray
//                    round-trip to in-place (outL, outR, sampleCount)
//                    to match RxChannel::processIq convention
//                Algorithm (MMSE-Wiener + minimum-statistics noise
//                floor + per-bin GSMOOTH Wiener gain smoothing)
//                preserved verbatim from AetherSDR [@0cd4559].
//                Authored by J.J. Boyd (KG4VCF) with AI-assisted
//                review via Anthropic Claude Code.
// =================================================================

#ifdef __APPLE__

#include "MacNRFilter.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <QLoggingCategory>

namespace NereusSDR {

// ── Constructor / destructor ────────────────────────────────────────────────
// Ported from AetherSDR src/core/MacNRFilter.cpp [@0cd4559] — constructor body.
// N/H constants are 1024/512 here vs AetherSDR's 512/256 at 24 kHz; all
// logic is identical.

MacNRFilter::MacNRFilter()
{
    m_fftSetup = vDSP_create_fftsetup(LOG2N, kFFTRadix2);
    if (!m_fftSetup)
        return;

    // vDSP split-complex scratch buffers (size H = N/2)
    m_splitRe.assign(H, 0.0f);
    m_splitIm.assign(H, 0.0f);

    // sqrt-Hann window (analysis × synthesis = Hann; perfect reconstruction
    // with 50 % overlap when frames are hop-sized)
    m_window.resize(N);
    for (int i = 0; i < N; ++i)
        m_window[i] = std::sqrt(0.5f * (1.0f - std::cos(2.0f * M_PI * i / N)));

    // Pre-fill input accumulator with one full frame of zeros so that the
    // first call to process() sees a centred first frame immediately and
    // latency is exactly one hop (H samples ≈ 10.7 ms at 48 kHz).
    m_inAccum.assign(N, 0.0f);

    // OLA and scratch buffers
    m_olaBuffer.assign(N, 0.0f);
    m_frameBuf .assign(N, 0.0f);
    m_synthBuf .assign(N, 0.0f);
    m_outAccum .clear();

    // Noise estimator
    m_noiseEst .assign(NBINS, 1e-6f);
    m_prevGain .assign(NBINS, 1.0f);
    m_prevPow  .assign(NBINS, 1e-6f);
    m_powerBuf .assign(NBINS, 0.0f);
    m_gainBuf  .assign(NBINS, 1.0f);
    m_smoothGain.assign(NBINS, 1.0f);

    // Warm up noise history so the estimator has a sensible floor from
    // frame zero; this prevents the algorithm applying excessive gain in
    // the first ~267 ms of operation.
    for (int h = 0; h < HIST; ++h)
        for (int k = 0; k < NBINS; ++k)
            m_powerHistory[h][k] = 1e-6f;
}

MacNRFilter::~MacNRFilter()
{
    if (m_fftSetup)
        vDSP_destroy_fftsetup(m_fftSetup);
}

// ── reset ───────────────────────────────────────────────────────────────────
// Ported verbatim from AetherSDR src/core/MacNRFilter.cpp [@0cd4559] — reset().

void MacNRFilter::reset()
{
    // Clear time-domain accumulators
    std::fill(m_inAccum .begin(), m_inAccum .end(), 0.0f);
    std::fill(m_olaBuffer.begin(), m_olaBuffer.end(), 0.0f);
    m_outAccum.clear();

    // Re-prime input accumulator for one-hop latency (same as constructor)
    m_inAccum.assign(N, 0.0f);

    // Reset noise estimator
    std::fill(m_noiseEst  .begin(), m_noiseEst  .end(), 1e-6f);
    std::fill(m_prevGain  .begin(), m_prevGain  .end(), 1.0f);
    std::fill(m_prevPow   .begin(), m_prevPow   .end(), 1e-6f);
    std::fill(m_gainBuf   .begin(), m_gainBuf   .end(), 1.0f);
    std::fill(m_smoothGain.begin(), m_smoothGain.end(), 1.0f);

    for (int h = 0; h < HIST; ++h)
        for (int k = 0; k < NBINS; ++k)
            m_powerHistory[h][k] = 1e-6f;

    m_histIdx    = 0;
    m_frameCount = 0;
}

// ── processFrame ────────────────────────────────────────────────────────────
//
// inBuf  : N windowed analysis samples
// outBuf : N synthesis samples (added into OLA buffer by caller)
//
// Ported verbatim from AetherSDR src/core/MacNRFilter.cpp [@0cd4559] —
// processFrame(). Algorithm unchanged; N/H/NBINS constants now reflect
// the 48 kHz retune (1024/512/513 instead of 512/256/257).

void MacNRFilter::processFrame(const float* inBuf, float* outBuf)
{
    // ── 1. Apply analysis window and copy into frame buffer ─────────────────
    vDSP_vmul(inBuf, 1, m_window.data(), 1, m_frameBuf.data(), 1, N);

    // ── 2. Forward real FFT ─────────────────────────────────────────────────
    // Pack N real samples into N/2 complex pairs for vDSP
    DSPSplitComplex sc{ m_splitRe.data(), m_splitIm.data() };
    vDSP_ctoz(reinterpret_cast<const DSPComplex*>(m_frameBuf.data()), 2, &sc, 1, H);
    vDSP_fft_zrip(m_fftSetup, &sc, 1, LOG2N, kFFTDirection_Forward);

    // Extract power spectrum  (NBINS = N/2+1 unique bins)
    // Bins 1 … H-1 : re² + im²
    // Bin 0        : DC only  (im part holds Nyquist in vDSP packed format)
    // Bin H        : Nyquist only
    m_powerBuf[0] = m_splitRe[0] * m_splitRe[0];
    m_powerBuf[H] = m_splitIm[0] * m_splitIm[0];
    for (int k = 1; k < H; ++k)
        m_powerBuf[k] = m_splitRe[k] * m_splitRe[k] + m_splitIm[k] * m_splitIm[k];

    // ── 3. Minimum-statistics noise floor update ─────────────────────────────
    for (int k = 0; k < NBINS; ++k)
        m_powerHistory[m_histIdx][k] = m_powerBuf[k];

    m_histIdx = (m_histIdx + 1) % HIST;

    // Minimum over history window, then apply bias correction
    for (int k = 0; k < NBINS; ++k) {
        float minPow = m_powerHistory[0][k];
        for (int h = 1; h < HIST; ++h)
            minPow = std::min(minPow, m_powerHistory[h][k]);
        m_noiseEst[k] = m_bias.load(std::memory_order_relaxed) * minPow;
    }

    // ── 4. Decision-directed MMSE-Wiener gain ────────────────────────────────
    for (int k = 0; k < NBINS; ++k) {
        // A-posteriori SNR
        const float postSnr = m_powerBuf[k] / std::max(m_noiseEst[k], 1e-10f);

        // A-priori SNR (decision-directed Ephraim-Malah: blend previous
        // clean-power estimate / noise with new a-posteriori observation).
        //
        // Bug fix 2026-04-23 vs AetherSDR [@0cd4559]: original formula
        // missed the /m_noiseEst on the prev-gain term, so it blended raw
        // power with SNR — result was priorSnr ≈ prev power (huge) and
        // Wiener gain = priorSnr / (priorSnr + OVER) ≈ 1.0 (inaudible NR).
        // Adding the noise-divide gives a proper dimensionless prior-SNR.
        const float prevCleanPow  = m_prevGain[k] * m_prevGain[k] * m_prevPow[k];
        const float prevSnrFromDD = prevCleanPow
                                  / std::max(m_noiseEst[k], 1e-10f);
        const float alpha_v  = m_alpha.load(std::memory_order_relaxed);
        const float priorSnr = alpha_v * prevSnrFromDD
                             + (1.0f - alpha_v) * std::max(postSnr - 1.0f, 0.0f);

        // Raw Wiener gain, clamped to [floor, 1]. OVER and FLOOR are now
        // runtime-tunable atomics (set via setOversub / setFloor).
        const float over  = m_oversub.load(std::memory_order_relaxed);
        const float floor = m_floor.load(std::memory_order_relaxed);
        const float g = priorSnr / (priorSnr + over);
        m_gainBuf[k] = std::clamp(g, floor, 1.0f);

        // ── Temporal gain smoothing ──────────────────────────────────────
        // Suppresses "musical noise" (rapid frame-to-frame gain swings)
        const float gsmooth_v = m_gsmooth.load(std::memory_order_relaxed);
        m_smoothGain[k] = gsmooth_v * m_smoothGain[k] + (1.0f - gsmooth_v) * m_gainBuf[k];

        // Effective gain: strength=0 → bypass (1.0), strength=1 → full NR
        const float appliedGain = 1.0f - m_strength.load() * (1.0f - m_smoothGain[k]);

        m_prevGain[k] = appliedGain;
        m_prevPow [k] = m_powerBuf[k];
    }

    // ── 5. Apply gain to spectrum ────────────────────────────────────────────
    m_splitRe[0] *= m_prevGain[0];   // DC
    m_splitIm[0] *= m_prevGain[H];   // Nyquist (stored in im[0] by vDSP)
    for (int k = 1; k < H; ++k) {
        m_splitRe[k] *= m_prevGain[k];
        m_splitIm[k] *= m_prevGain[k];
    }

    // ── 6. Inverse real FFT ──────────────────────────────────────────────────
    vDSP_fft_zrip(m_fftSetup, &sc, 1, LOG2N, kFFTDirection_Inverse);
    vDSP_ztoc(&sc, 1, reinterpret_cast<DSPComplex*>(m_synthBuf.data()), 2, H);

    // Normalise (vDSP's FFT is un-normalised; factor = 1/(2N))
    const float scale = 1.0f / (2.0f * N);
    vDSP_vsmul(m_synthBuf.data(), 1, &scale, m_synthBuf.data(), 1, N);

    // ── 7. Apply synthesis window ────────────────────────────────────────────
    vDSP_vmul(m_synthBuf.data(), 1, m_window.data(), 1, outBuf, 1, N);
}

// ── process ─────────────────────────────────────────────────────────────────
//
// Input:  outL/outR — separated-channel 48 kHz float32 arrays (sampleCount each)
//         on entry  these contain the post-fexchange2 decoded audio L/R
// Output: on return L and R are both overwritten with the noise-reduced mono signal
//
// Ported from AetherSDR src/core/MacNRFilter.cpp [@0cd4559] — process().
// Signature changed from QByteArray round-trip to in-place (outL, outR,
// sampleCount) to match RxChannel::processIq convention.  OLA algorithm
// is preserved verbatim.

void MacNRFilter::process(float* outL, float* outR, int sampleCount)
{
    if (!m_fftSetup || sampleCount <= 0)
        return;

    // ── Stereo float32 → mono float (average channels) ──────────────────────
    for (int i = 0; i < sampleCount; ++i) {
        m_inAccum.push_back(0.5f * (outL[i] + outR[i]));
    }

    // ── OLA processing — emit one hop per iteration ──────────────────────────
    while (static_cast<int>(m_inAccum.size()) >= N) {
        // Overlap-add: accumulate synthesis output
        std::vector<float> outFrame(N, 0.0f);
        processFrame(m_inAccum.data(), outFrame.data());

        // Add into OLA buffer
        for (int i = 0; i < N; ++i)
            m_olaBuffer[i] += outFrame[i];

        // Flush the first H samples to output
        for (int i = 0; i < H; ++i)
            m_outAccum.push_back(m_olaBuffer[i]);

        // Shift OLA buffer left by H
        std::copy(m_olaBuffer.begin() + H, m_olaBuffer.end(), m_olaBuffer.begin());
        std::fill(m_olaBuffer.begin() + H, m_olaBuffer.end(), 0.0f);

        // Consume H input samples
        m_inAccum.erase(m_inAccum.begin(), m_inAccum.begin() + H);

        ++m_frameCount;
    }

    // ── Drain exactly sampleCount processed mono samples → write L and R ─────
    // If the accumulator has fewer samples than needed (first few calls during
    // startup), pad with zeros rather than silence the whole buffer.
    const int available = static_cast<int>(m_outAccum.size());
    const int take      = std::min(available, sampleCount);

    for (int i = 0; i < take; ++i) {
        const float s = m_outAccum[i];
        outL[i] = s;
        outR[i] = s;
    }

    // Zero-fill any samples not yet produced (startup transient only)
    for (int i = take; i < sampleCount; ++i) {
        outL[i] = 0.0f;
        outR[i] = 0.0f;
    }

    // Remove consumed samples
    if (take > 0)
        m_outAccum.erase(m_outAccum.begin(), m_outAccum.begin() + take);
}

} // namespace NereusSDR

#endif // __APPLE__
