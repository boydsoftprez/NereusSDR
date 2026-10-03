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
//   2026-09-28  J.J. Boyd / KG4VCF  latencyInputSamples; every process
//                 variant feeds r8brain at most maxBlockSamples per call
//                 (resampleChunked, r8bProcess). NereusSDR-original.
//                 AI tooling: Anthropic Claude Code.
// =================================================================

#include "core/Resampler.h"

#include "CDSPResampler.h"

#include <algorithm>

namespace NereusSDR {

// From AetherSDR src/core/Resampler.cpp:7-13 [@0cd4559]
Resampler::Resampler(double srcRate, double dstRate, int maxBlockSamples)
    : m_srcRate(srcRate)
    , m_dstRate(dstRate)
    , m_resampler(std::make_unique<r8b::CDSPResampler24>(srcRate, dstRate, maxBlockSamples))
    , m_maxBlockSamples(maxBlockSamples)
{
    m_inBuf.reserve(maxBlockSamples);
    m_chunk.resize(static_cast<size_t>(std::max(1, maxBlockSamples)));
}

// From AetherSDR src/core/Resampler.cpp:15 [@0cd4559]
Resampler::~Resampler() = default;

int Resampler::latencyInputSamples() const
{
    return m_resampler->getInLenBeforeOutPos(0);
}

void Resampler::clear()
{
    m_resampler->clear();
}

// NereusSDR: every r8brain process() call goes through here, so the
// largest single call is on record (largestInputBlock).
int Resampler::r8bProcess(double* in, int numSamples, double*& out)
{
    m_largestInputBlock = std::max(m_largestInputBlock, numSamples);
    return m_resampler->process(in, numSamples, out);
}

// NereusSDR: r8brain sizes its internal buffers for the maxBlockSamples it
// was constructed with and writes past them when one process() call is given
// more (the heap corruption RadeChannel.cpp records from PR #238). Every
// entry point below therefore feeds r8brain at most maxBlockSamples at a
// time; r8brain keeps its state across calls, so the output is the same as
// one call would give. `mono` holds the prepared input.
QByteArray Resampler::resampleChunked(const double* mono, int numSamples, bool stereoOut)
{
    QByteArray result;
    const int block = std::max(1, m_maxBlockSamples);
    for (int offset = 0; offset < numSamples; offset += block) {
        const int n = std::min(block, numSamples - offset);
        std::copy(mono + offset, mono + offset + n, m_chunk.begin());
        double* outPtr = nullptr;
        const int outLen = r8bProcess(m_chunk.data(), n, outPtr);
        if (outLen <= 0 || outPtr == nullptr) {
            continue;
        }
        const int channels = stereoOut ? 2 : 1;
        const qsizetype at = result.size();
        result.resize(at + qsizetype(outLen) * channels * qsizetype(sizeof(float)));
        auto* dst = reinterpret_cast<float*>(result.data() + at);
        for (int i = 0; i < outLen; ++i) {
            const float v = static_cast<float>(outPtr[i]);
            dst[channels * i] = v;
            if (stereoOut) {
                dst[2 * i + 1] = v;
            }
        }
    }
    return result;
}

// From AetherSDR src/core/Resampler.cpp:17-38 [@0cd4559]
QByteArray Resampler::process(const float* in, int numSamples)
{
    if (numSamples <= 0) return {};

    // Convert float32 -> double
    m_inBuf.resize(numSamples);
    for (int i = 0; i < numSamples; ++i)
        m_inBuf[i] = static_cast<double>(in[i]);

    // Resample (NereusSDR: in blocks of at most maxBlockSamples)
    return resampleChunked(m_inBuf.data(), numSamples, /*stereoOut=*/false);
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

    // Convert float32 -> double, and resample, in blocks of at most
    // maxBlockSamples; m_chunk was sized to that in the constructor, so
    // this stays allocation-free.
    const int block = std::max(1, m_maxBlockSamples);
    int written = 0;
    for (int offset = 0; offset < numSamples; offset += block) {
        const int n = std::min(block, numSamples - offset);
        for (int i = 0; i < n; ++i) {
            m_chunk[static_cast<size_t>(i)] = static_cast<double>(in[offset + i]);
        }
        double* outPtr = nullptr;
        int outLen = r8bProcess(m_chunk.data(), n, outPtr);
        if (outLen <= 0 || outPtr == nullptr) {
            continue;
        }
        // Clamp to caller's capacity.  r8brain's per-call output length
        // varies with the rate ratio and internal buffer fill, so the
        // caller's outCapacity must be sized for the worst case.
        if (outLen > outCapacity - written) {
            outLen = outCapacity - written;
        }
        for (int i = 0; i < outLen; ++i) {
            out[written + i] = static_cast<float>(outPtr[i]);
        }
        written += outLen;
        if (written >= outCapacity) {
            break;
        }
    }
    return written;
}

// From AetherSDR src/core/Resampler.cpp:40-61 [@0cd4559]
QByteArray Resampler::processStereoToMono(const float* stereoIn, int numStereoFrames)
{
    if (numStereoFrames <= 0) return {};

    // Downmix stereo -> mono
    m_inBuf.resize(numStereoFrames);
    for (int i = 0; i < numStereoFrames; ++i)
        m_inBuf[i] = (stereoIn[2 * i] + stereoIn[2 * i + 1]) * 0.5;

    // Resample (NereusSDR: in blocks of at most maxBlockSamples)
    return resampleChunked(m_inBuf.data(), numStereoFrames, /*stereoOut=*/false);
}

// From AetherSDR src/core/Resampler.cpp:63-87 [@0cd4559]
QByteArray Resampler::processMonoToStereo(const float* monoIn, int numSamples)
{
    if (numSamples <= 0) return {};

    // Convert float32 -> double
    m_inBuf.resize(numSamples);
    for (int i = 0; i < numSamples; ++i)
        m_inBuf[i] = static_cast<double>(monoIn[i]);

    // Resample, then duplicate mono to L+R (NereusSDR: in blocks of at most
    // maxBlockSamples)
    return resampleChunked(m_inBuf.data(), numSamples, /*stereoOut=*/true);
}

// From AetherSDR src/core/Resampler.cpp:89-111 [@0cd4559]
QByteArray Resampler::processStereoToStereo(const float* stereoIn, int numStereoFrames)
{
    if (numStereoFrames <= 0) return {};

    // Downmix stereo -> mono, resample, duplicate back to stereo
    m_inBuf.resize(numStereoFrames);
    for (int i = 0; i < numStereoFrames; ++i)
        m_inBuf[i] = (stereoIn[2 * i] + stereoIn[2 * i + 1]) * 0.5;

    return resampleChunked(m_inBuf.data(), numStereoFrames, /*stereoOut=*/true);
}

} // namespace NereusSDR
