// =================================================================
// src/core/DeepFilterFilter.cpp  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR src/core/DeepFilterFilter.cpp @ 0cd4559.
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
//                findModelPath() removed — now resolves via
//                ModelPaths::dfnrModelTarball().
//                df_create / df_process_frame / df_set_* calls
//                and tuning surface (attenLimit, postFilterBeta)
//                unchanged from AetherSDR original.
//                Authored by J.J. Boyd (KG4VCF) with AI-assisted
//                review via Anthropic Claude Code.
// =================================================================

#ifdef HAVE_DFNR

#include "DeepFilterFilter.h"
#include "deep_filter.h"
#include "LogCategories.h"
#include "ModelPaths.h"

#include <algorithm>
#include <cstring>
#include <vector>
#include <QDebug>

namespace NereusSDR {

DeepFilterFilter::DeepFilterFilter()
{
    QByteArray modelPath = NereusSDR::ModelPaths::dfnrModelTarball().toUtf8();
    if (modelPath.isEmpty()) {
        qCWarning(lcDsp) << "DeepFilterFilter: model not found via ModelPaths::dfnrModelTarball() — DFNR disabled";
        return;
    }
    qCDebug(lcDsp) << "DeepFilterFilter: loading model from" << modelPath;
    m_state = df_create(modelPath.constData(), m_attenLimit.load(), nullptr);
    if (m_state) {
        m_frameSize = static_cast<int>(df_get_frame_length(m_state));
        qCDebug(lcDsp) << "DeepFilterFilter: initialized, frame size =" << m_frameSize;
    } else {
        qCWarning(lcDsp) << "DeepFilterFilter: df_create() failed!";
    }
}

DeepFilterFilter::~DeepFilterFilter()
{
    if (m_state) {
        df_free(m_state);
    }
}

void DeepFilterFilter::reset()
{
    if (m_state) {
        df_free(m_state);
        m_state = nullptr;
    }
    QByteArray modelPath = NereusSDR::ModelPaths::dfnrModelTarball().toUtf8();
    if (modelPath.isEmpty()) {
        qCWarning(lcDsp) << "DeepFilterFilter::reset(): model not found — DFNR stays disabled";
    } else {
        m_state = df_create(modelPath.constData(), m_attenLimit.load(), nullptr);
        if (m_state) {
            m_frameSize = static_cast<int>(df_get_frame_length(m_state));
        }
    }
    m_inAccum.clear();
    m_outAccum.clear();
    m_paramsDirty.store(true);
}

void DeepFilterFilter::setAttenLimit(float db)
{
    m_attenLimit.store(db);
    m_paramsDirty.store(true);
}

void DeepFilterFilter::setPostFilterBeta(float beta)
{
    m_postFilterBeta.store(beta);
    m_paramsDirty.store(true);
}

void DeepFilterFilter::process(float* outL, float* outR, int sampleCount)
{
    if (!m_state || m_frameSize <= 0 || sampleCount <= 0) {
        return;
    }

    // Apply pending parameter changes (main thread writes atomics; audio
    // thread applies them on the next block).
    if (m_paramsDirty.exchange(false)) {
        df_set_atten_lim(m_state, m_attenLimit.load());
        df_set_post_filter_beta(m_state, m_postFilterBeta.load());
    }

    // NereusSDR is already at 48 kHz stereo float — no resample pass needed
    // (removed from AetherSDR's 24kHz-pipeline-targeted wrapper). Mix L+R
    // to mono, feed DeepFilterNet3, write processed mono back to both L and R.
    // Accumulator handles the DFNet3 frame-size quantization.
    const int prevAccumSamples =
        m_inAccum.size() / static_cast<int>(sizeof(float));
    m_inAccum.resize((prevAccumSamples + sampleCount) * sizeof(float));
    auto* accum = reinterpret_cast<float*>(m_inAccum.data());
    for (int i = 0; i < sampleCount; ++i) {
        accum[prevAccumSamples + i] = 0.5f * (outL[i] + outR[i]);
    }

    const int totalSamples = prevAccumSamples + sampleCount;
    const int completeFrames = totalSamples / m_frameSize;

    if (completeFrames > 0) {
        std::vector<float> processed(completeFrames * m_frameSize);
        for (int f = 0; f < completeFrames; ++f) {
            df_process_frame(m_state,
                             &accum[f * m_frameSize],
                             &processed[f * m_frameSize]);
        }
        // Keep leftover
        const int consumed = completeFrames * m_frameSize;
        const int leftoverSamples = totalSamples - consumed;
        if (leftoverSamples > 0) {
            QByteArray leftover(
                reinterpret_cast<const char*>(&accum[consumed]),
                leftoverSamples * static_cast<int>(sizeof(float)));
            m_inAccum = leftover;
        } else {
            m_inAccum.clear();
        }
        m_outAccum.append(
            reinterpret_cast<const char*>(processed.data()),
            static_cast<qsizetype>(processed.size() * sizeof(float)));
    }

    // Drain output accumulator back into outL/outR (mono → duplicate).
    const int availableSamples =
        m_outAccum.size() / static_cast<int>(sizeof(float));
    const int toConsume = std::min(sampleCount, availableSamples);
    const auto* src = reinterpret_cast<const float*>(m_outAccum.constData());
    for (int i = 0; i < toConsume; ++i) {
        outL[i] = src[i];
        outR[i] = src[i];
    }
    if (toConsume > 0) {
        m_outAccum.remove(0, toConsume * static_cast<int>(sizeof(float)));
    }
    // If we don't have enough output yet (happens during startup until we
    // accumulate a full frame), leave the remainder of outL/outR as-is
    // (passing the input through — avoids dropouts while DFNR warms up).
}

} // namespace NereusSDR

#endif // HAVE_DFNR
