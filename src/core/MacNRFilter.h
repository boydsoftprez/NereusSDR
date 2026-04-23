// =================================================================
// src/core/MacNRFilter.h  (NereusSDR)
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
//                  * change kSampleRate 24000 → 48000 (LOG2N comment)
//                  * double kFFTSize  512  → 1024  (LOG2N 9 → 10;
//                    preserves 46.9 Hz bin resolution at 48 kHz)
//                  * double kHopSize  256  → 512   (H = N/2)
//                  * noise history ~267 ms still satisfied by
//                    HIST=25 frames × (512/48000) s = 267 ms
//                Algorithm (MMSE-Wiener + min-statistics noise floor
//                + per-bin GSMOOTH Wiener gain smoothing) preserved
//                verbatim.
//                New process(outL, outR, sampleCount) signature
//                operates in-place on separated channel arrays to
//                match RxChannel::processIq.
//                Authored by J.J. Boyd (KG4VCF) with AI-assisted
//                review via Anthropic Claude Code.
// =================================================================

#pragma once

#ifdef __APPLE__

#include <atomic>
#include <vector>
#include <Accelerate/Accelerate.h>

namespace NereusSDR {

// macOS spectral noise reduction using Apple Accelerate (vDSP).
//
// MMSE-Wiener filter with minimum-statistics noise floor tracking.
// Uses vDSP's real FFT, hardware-accelerated on Apple Silicon via AMX.
//
// Ported from AetherSDR src/core/MacNRFilter.h [@0cd4559].
// Retuned for NereusSDR's 48 kHz audio path:
//   - 1024-point FFT at 48 kHz → 46.9 Hz/bin (same resolution as
//     AetherSDR's 512-point FFT at 24 kHz).
//   - 25-frame noise history (~267 ms) — identical to AetherSDR.
//   - Per-bin Wiener gain is temporally smoothed (GSMOOTH) to suppress
//     musical-noise artefacts caused by rapid frame-to-frame gain swings.
//   - Output accumulator ensures exact sample-count match with no silence
//     gaps; startup latency is one hop (≈10.7 ms) of pre-filled zeros.
//   - User-adjustable strength: 0 = bypass, 1 = full NR.
//
// Processing chain (all at 48 kHz):
//   stereo float32 (separated L/R) → mono float → FFT NR (1024-pt OLA)
//   → write processed mono back to L and R

class MacNRFilter {
public:
    MacNRFilter();
    ~MacNRFilter();

    bool isValid() const { return m_fftSetup != nullptr; }

    // Process 48 kHz stereo float32 PCM in-place.
    // outL/outR are separated channel arrays of sampleCount samples.
    // Averages L+R to mono, runs spectral NR, writes result to both channels.
    void process(float* outL, float* outR, int sampleCount);

    // Reset internal state (e.g. on band change or stream restart).
    void reset();

    // Noise-reduction strength: 0.0 = bypass, 2.0 = over-drive (phase flip).
    // Full range per user directive — no conservative clamp.
    // Thread-safe: audio thread and UI thread may access concurrently.
    void  setStrength(float s) { m_strength.store(std::clamp(s, 0.0f, 2.0f)); }
    float strength()     const { return m_strength.load(); }

    // Oversubtraction factor — higher = more aggressive attenuation on low-SNR
    // bins. Uncapped (0.01–1000) per user directive. Default 4.
    void  setOversub(float o) { m_oversub.store(std::clamp(o, 0.01f, 1000.0f)); }
    float oversub()     const { return m_oversub.load(); }

    // Noise-floor clamp — minimum Wiener gain per bin. Full range 0–2.
    // 0 = silence, 1 = no attenuation, 2 = amplify. Default 0.05 (-26 dB).
    void  setFloor(float f) { m_floor.store(std::clamp(f, 0.0f, 2.0f)); }
    float floor()     const { return m_floor.load(); }

    // Decision-directed smoothing coefficient (alpha). 0=no smoothing, 1=frozen.
    void  setAlpha(float a) { m_alpha.store(std::clamp(a, 0.0f, 1.0f)); }
    float alpha()     const { return m_alpha.load(); }

    // Min-stats noise-floor bias correction. 0=none, 10=max. Default 1.2.
    void  setBias(float b) { m_bias.store(std::clamp(b, 0.0f, 10.0f)); }
    float bias()      const { return m_bias.load(); }

    // Temporal gain smoothing coefficient. 0=off, 1=frozen. Default 0.70.
    void  setGsmooth(float g) { m_gsmooth.store(std::clamp(g, 0.0f, 1.0f)); }
    float gsmooth()   const { return m_gsmooth.load(); }

private:
    // Process one N-sample analysis frame; writes N output samples to outBuf.
    void processFrame(const float* inBuf, float* outBuf);

    // ── FFT parameters ─────────────────────────────────────────────────
    // Doubled from AetherSDR (LOG2N 9→10, N 512→1024, H 256→512) to
    // preserve 46.9 Hz/bin resolution at 48 kHz.
    // From AetherSDR src/core/MacNRFilter.h [@0cd4559] — retuned for 48 kHz.
    static constexpr int LOG2N = 10;           // log2(1024); AetherSDR had 9 (24 kHz)
    static constexpr int N     = 1 << LOG2N;  // 1024-point real FFT
    static constexpr int H     = N / 2;       // 512-sample hop (50 % overlap)
    static constexpr int NBINS = N / 2 + 1;   // 513 unique spectral bins

    // ── Algorithm tuning ───────────────────────────────────────────────
    // Baseline from AetherSDR [@0cd4559]; retuned 2026-04-23 for audible
    // NR depth after RMS-diagnostic run showed AetherSDR defaults gave
    // only -2 to -3 dB attenuation on voice+noise (imperceptible as NR).
    // Baseline from AetherSDR [@0cd4559]; after fixing the dimensional
    // bug in the decision-directed prior-SNR formula (MacNRFilter.cpp, the
    // /noise_est division that AetherSDR was missing), AetherSDR's OVER=2
    // gave ~-6 dB uniform attenuation that wasn't perceived as selective
    // NR. OVER=4 tilts the Wiener curve toward stronger attenuation on
    // low-SNR bins while leaving high-SNR (voice) bins mostly untouched.
    static constexpr int   HIST       = 25;
    // Compile-time defaults kept as DEF_* so init list can reference them.
    static constexpr float DEF_ALPHA  = 0.92f;
    static constexpr float DEF_BIAS   = 1.2f;
    static constexpr float DEF_GSMOOTH = 0.70f;
    // Default values for runtime-tunable knobs.
    static constexpr float DEF_OVER   = 4.0f;
    static constexpr float DEF_FLOOR  = 0.05f;

    // ── vDSP state ─────────────────────────────────────────────────────
    FFTSetup           m_fftSetup{nullptr};
    std::vector<float> m_splitRe;   // split-complex real  [H]
    std::vector<float> m_splitIm;   // split-complex imag  [H]

    // ── OLA buffers ────────────────────────────────────────────────────
    std::vector<float> m_window;    // sqrt-Hann analysis+synthesis window [N]
    std::vector<float> m_inAccum;   // 48 kHz mono float input accumulator
    std::vector<float> m_olaBuffer; // overlap-add accumulator [N]
    std::vector<float> m_frameBuf;  // windowed analysis frame [N]
    std::vector<float> m_synthBuf;  // synthesis frame [N]
    std::vector<float> m_outAccum;  // processed 48 kHz mono float output

    // ── Noise estimator state ─────────────────────────────────────────
    float              m_powerHistory[HIST][NBINS]{};
    int                m_histIdx{0};
    std::vector<float> m_noiseEst;   // current noise floor estimate [NBINS]
    std::vector<float> m_prevGain;   // previous-frame Wiener gain [NBINS]
    std::vector<float> m_prevPow;    // previous-frame power spectrum [NBINS]
    std::vector<float> m_powerBuf;   // current power spectrum [NBINS]
    std::vector<float> m_gainBuf;    // raw Wiener gain per bin [NBINS]
    std::vector<float> m_smoothGain; // temporally smoothed gain [NBINS]
    int                m_frameCount{0};

    std::atomic<float> m_strength{1.0f};
    std::atomic<float> m_oversub{DEF_OVER};
    std::atomic<float> m_floor{DEF_FLOOR};
    std::atomic<float> m_alpha{DEF_ALPHA};
    std::atomic<float> m_bias{DEF_BIAS};
    std::atomic<float> m_gsmooth{DEF_GSMOOTH};
};

} // namespace NereusSDR

#endif // __APPLE__
