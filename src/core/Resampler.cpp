// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/Resampler.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR - Resampler implementation. Wraps r8b::CDSPResampler24
// for float32 <-> double conversion with optional stereo<->mono
// convenience helpers.
//
// Ported byte-for-byte from AetherSDR src/core/Resampler.cpp [@0cd4559].
//
// License (upstream): see Resampler.h for the full attribution block.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I2a. Full port of
//                 AetherSDR src/core/Resampler.cpp [@0cd4559].
//                 Namespace renamed AetherSDR -> NereusSDR; the
//                 r8brain header is now resolved against
//                 third_party/r8brain/ (added in the same commit).
//                 AI tooling: Anthropic Claude Code.
// =================================================================

#include "core/Resampler.h"

#include "CDSPResampler.h"

namespace NereusSDR {

// From AetherSDR src/core/Resampler.cpp:7-13 [@0cd4559]
Resampler::Resampler(double srcRate, double dstRate, int maxBlockSamples)
    : m_srcRate(srcRate)
    , m_dstRate(dstRate)
    , m_resampler(std::make_unique<r8b::CDSPResampler24>(srcRate, dstRate, maxBlockSamples))
{
    m_inBuf.reserve(maxBlockSamples);
}

// From AetherSDR src/core/Resampler.cpp:15 [@0cd4559]
Resampler::~Resampler() = default;

// From AetherSDR src/core/Resampler.cpp:17-38 [@0cd4559]
QByteArray Resampler::process(const float* in, int numSamples)
{
    if (numSamples <= 0) return {};

    // Convert float32 -> double
    m_inBuf.resize(numSamples);
    for (int i = 0; i < numSamples; ++i)
        m_inBuf[i] = static_cast<double>(in[i]);

    // Resample
    double* outPtr = nullptr;
    int outLen = m_resampler->process(m_inBuf.data(), numSamples, outPtr);

    if (outLen <= 0 || !outPtr) return {};

    // Convert double -> float32
    QByteArray result(outLen * static_cast<int>(sizeof(float)), Qt::Uninitialized);
    auto* dst = reinterpret_cast<float*>(result.data());
    for (int i = 0; i < outLen; ++i)
        dst[i] = static_cast<float>(outPtr[i]);
    return result;
}

// NereusSDR-original: non-allocating variant of process() for use inside
// real-time audio callbacks (e.g. PortAudioBus paCallback).  Same math
// as process() but writes into a caller-provided float buffer instead
// of returning a QByteArray.  See Resampler.h for the rationale.
int Resampler::processInto(const float* in, int numSamples,
                           float* out, int outCapacity)
{
    if (numSamples <= 0 || in == nullptr ||
        out == nullptr || outCapacity <= 0) {
        return 0;
    }

    // Convert float32 -> double.  m_inBuf was reserved to maxBlockSamples
    // in the constructor; resize() within that capacity is allocation-free.
    m_inBuf.resize(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        m_inBuf[i] = static_cast<double>(in[i]);
    }

    double* outPtr = nullptr;
    int outLen = m_resampler->process(m_inBuf.data(), numSamples, outPtr);
    if (outLen <= 0 || outPtr == nullptr) {
        return 0;
    }
    // Clamp to caller's capacity.  r8brain's per-call output length
    // varies with the rate ratio and internal buffer fill, so the
    // caller's outCapacity must be sized for the worst case.
    if (outLen > outCapacity) {
        outLen = outCapacity;
    }
    for (int i = 0; i < outLen; ++i) {
        out[i] = static_cast<float>(outPtr[i]);
    }
    return outLen;
}

// From AetherSDR src/core/Resampler.cpp:40-61 [@0cd4559]
QByteArray Resampler::processStereoToMono(const float* stereoIn, int numStereoFrames)
{
    if (numStereoFrames <= 0) return {};

    // Downmix stereo -> mono
    m_inBuf.resize(numStereoFrames);
    for (int i = 0; i < numStereoFrames; ++i)
        m_inBuf[i] = (stereoIn[2 * i] + stereoIn[2 * i + 1]) * 0.5;

    // Resample
    double* outPtr = nullptr;
    int outLen = m_resampler->process(m_inBuf.data(), numStereoFrames, outPtr);

    if (outLen <= 0 || !outPtr) return {};

    // Convert double -> float32 mono
    QByteArray result(outLen * static_cast<int>(sizeof(float)), Qt::Uninitialized);
    auto* dst = reinterpret_cast<float*>(result.data());
    for (int i = 0; i < outLen; ++i)
        dst[i] = static_cast<float>(outPtr[i]);
    return result;
}

// From AetherSDR src/core/Resampler.cpp:63-87 [@0cd4559]
QByteArray Resampler::processMonoToStereo(const float* monoIn, int numSamples)
{
    if (numSamples <= 0) return {};

    // Convert float32 -> double
    m_inBuf.resize(numSamples);
    for (int i = 0; i < numSamples; ++i)
        m_inBuf[i] = static_cast<double>(monoIn[i]);

    // Resample
    double* outPtr = nullptr;
    int outLen = m_resampler->process(m_inBuf.data(), numSamples, outPtr);

    if (outLen <= 0 || !outPtr) return {};

    // Convert double -> float32 stereo (duplicate mono to L+R)
    QByteArray result(outLen * 2 * static_cast<int>(sizeof(float)), Qt::Uninitialized);
    auto* dst = reinterpret_cast<float*>(result.data());
    for (int i = 0; i < outLen; ++i) {
        float s = static_cast<float>(outPtr[i]);
        dst[2 * i]     = s;
        dst[2 * i + 1] = s;
    }
    return result;
}

// From AetherSDR src/core/Resampler.cpp:89-111 [@0cd4559]
QByteArray Resampler::processStereoToStereo(const float* stereoIn, int numStereoFrames)
{
    if (numStereoFrames <= 0) return {};

    // Downmix stereo -> mono, resample, duplicate back to stereo
    m_inBuf.resize(numStereoFrames);
    for (int i = 0; i < numStereoFrames; ++i)
        m_inBuf[i] = (stereoIn[2 * i] + stereoIn[2 * i + 1]) * 0.5;

    double* outPtr = nullptr;
    int outLen = m_resampler->process(m_inBuf.data(), numStereoFrames, outPtr);

    if (outLen <= 0 || !outPtr) return {};

    QByteArray result(outLen * 2 * static_cast<int>(sizeof(float)), Qt::Uninitialized);
    auto* dst = reinterpret_cast<float*>(result.data());
    for (int i = 0; i < outLen; ++i) {
        float s = static_cast<float>(outPtr[i]);
        dst[2 * i]     = s;
        dst[2 * i + 1] = s;
    }
    return result;
}

} // namespace NereusSDR
