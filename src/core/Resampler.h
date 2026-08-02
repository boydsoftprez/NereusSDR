// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/Resampler.h  (NereusSDR)
// =================================================================
//
// NereusSDR - High-quality sample rate converter wrapper around
// r8brain-free-src's CDSPResampler24 (MIT). One instance handles
// one fixed source-to-destination rate ratio; create separate
// instances for upsample and downsample paths.
//
// Ported byte-for-byte from AetherSDR src/core/Resampler.h [@0cd4559].
//
// License (upstream):
//   - AetherSDR has no per-file copyright header, so per
//     docs/attribution/HOW-TO-PORT.md rule 6 we cite the project URL
//     and primary author at NereusSDR block level rather than copying
//     a verbatim header that does not exist:
//       Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//         - per https://github.com/ten9876/AetherSDR (GPLv3; see
//           LICENSE and About dialog for the live contributor list)
//   - The r8brain headers it forward-declares carry their own MIT
//     license; see third_party/r8brain/LICENSE.txt.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I2a. Full port of
//                 AetherSDR src/core/Resampler.{h,cpp} [@0cd4559].
//                 Replaces the 17-line I1 stub. Namespace renamed
//                 AetherSDR -> NereusSDR; otherwise byte-for-byte.
//                 AI tooling: Anthropic Claude Code.
// =================================================================

#pragma once

#include <QByteArray>
#include <memory>
#include <vector>
#include <cstdint>

namespace r8b { class CDSPResampler24; }

namespace NereusSDR {

// High-quality sample rate converter using r8brain-free-src (MIT).
// Wraps r8b::CDSPResampler24 with float32 <-> double conversion.
//
// Each instance handles one fixed rate ratio. Create separate instances
// for upsample and downsample paths.
//
// Thread safety: NOT thread-safe. Use one instance per thread/path.

class Resampler {
public:
    // maxBlockSamples: max mono samples per process() call
    Resampler(double srcRate, double dstRate, int maxBlockSamples = 4096);
    ~Resampler();

    // Resample mono float32 PCM. Returns resampled mono float32.
    QByteArray process(const float* in, int numSamples);

    // Non-allocating mono float32 resample.  Writes up to `outCapacity`
    // samples into `out` and returns the number written.  Safe to call
    // from a real-time audio callback because the only heap activity
    // is the one-time resize of the internal double scratch buffer in
    // the constructor / first call -- subsequent calls reuse it.
    // Used by the macOS mic-input path in PortAudioBus where we open
    // the device at its native rate and resample to 48 kHz on our own
    // clock to bypass CoreAudio AUHAL's sample-rate converter.
    int processInto(const float* in, int numSamples,
                    float* out, int outCapacity);

    // Convenience: stereo float32 -> mono downsample -> resampled mono float32
    QByteArray processStereoToMono(const float* stereoIn, int numStereoFrames);

    // Convenience: mono float32 -> resampled -> duplicated to stereo float32
    QByteArray processMonoToStereo(const float* monoIn, int numSamples);

    // Convenience: stereo float32 -> downmix to mono -> resample -> duplicate to stereo float32
    QByteArray processStereoToStereo(const float* stereoIn, int numStereoFrames);

    double srcRate() const { return m_srcRate; }
    double dstRate() const { return m_dstRate; }

private:
    double m_srcRate;
    double m_dstRate;
    std::unique_ptr<r8b::CDSPResampler24> m_resampler;
    std::vector<double> m_inBuf;   // float32 -> double conversion buffer
};

} // namespace NereusSDR
