// =================================================================
// src/core/DeepFilterFilter.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR src/core/DeepFilterFilter.h @ 0cd4559.
// AetherSDR has no per-file headers — project-level license applies
// (GPLv3 per https://github.com/ten9876/AetherSDR).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — Imported from AetherSDR and MODIFIED for 48 kHz
//                stereo float native audio. Removed internal r8brain
//                24↔48 resampler pair (AetherSDR process() used
//                processStereoToMono + processMonoToStereo because
//                AetherSDR's audio pipeline runs at 24 kHz).
//                NereusSDR's post-fexchange2 output is already at
//                48 kHz stereo float, matching DeepFilterNet3's
//                native rate — saves 2 resample passes per block.
//                New process(outL, outR, sampleCount) signature
//                operates in-place on separated channel arrays.
//                Algorithm (df_create / df_process_frame / df_set_*)
//                and tuning surface (attenLimit, postFilterBeta)
//                unchanged.
//                Authored by J.J. Boyd (KG4VCF) with AI-assisted
//                review via Anthropic Claude Code.
// =================================================================

#pragma once

#ifdef HAVE_DFNR

#include <QByteArray>
#include <atomic>
#include <memory>

struct DFState;

namespace NereusSDR {

// Client-side neural noise reduction using DeepFilterNet3.
// Processes 48 kHz stereo float audio in-place via the DeepFilterNet3 model.
//
// NereusSDR's post-fexchange2 output is already at 48 kHz stereo float,
// matching DeepFilterNet3's native input rate exactly — no resampling needed.
// Frame size determined at runtime via df_get_frame_length().
// Thread-safe parameter setters (main thread writes, audio thread reads).

class DeepFilterFilter {
public:
    DeepFilterFilter();
    ~DeepFilterFilter();

    DeepFilterFilter(const DeepFilterFilter&) = delete;
    DeepFilterFilter& operator=(const DeepFilterFilter&) = delete;

    // Process a block of 48 kHz stereo float audio in-place.
    // outL / outR: pointers to the L and R channel buffers (sampleCount each).
    // DeepFilterNet3 processes mono; we mix L+R → mono, denoise, write mono
    // back to both L and R (matches AetherSDR's original effective behavior).
    void process(float* outL, float* outR, int sampleCount);

    // Returns true if df_create() succeeded and the model was located.
    bool isValid() const { return m_state != nullptr; }

    // Reset internal state (e.g., on band change).
    void reset();

    // Attenuation limit in dB (0 = passthrough, 100 = max removal)
    void setAttenLimit(float db);
    float attenLimit() const { return m_attenLimit.load(); }

    // Post-filter beta (0 = disabled, 0.05–0.3 typical)
    void setPostFilterBeta(float beta);
    float postFilterBeta() const { return m_postFilterBeta.load(); }

private:
    DFState* m_state{nullptr};
    int m_frameSize{0};                     // samples per frame (from df_get_frame_length)
    QByteArray m_inAccum;                   // accumulate 48kHz mono float input
    QByteArray m_outAccum;                  // accumulate processed mono float output
    std::atomic<float> m_attenLimit{100.0f};
    std::atomic<float> m_postFilterBeta{0.0f};
    std::atomic<bool>  m_paramsDirty{false};
};

} // namespace NereusSDR

#endif // HAVE_DFNR
