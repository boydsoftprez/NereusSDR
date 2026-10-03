// =================================================================
// src/core/session/media/RemoteAudioRateMatcher.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis ivac.c and WDSP rmatch (v2.10.3.15, commit 3759d09).
// Porting from Project Files/Source/ChannelMaster/ivac.c:34-45,
// 145-168, and 254 — original C logic creates rmatchOUT with
// create_rmatchV(audio_size, vac_size, audio_rate, vac_rate,
// OUTringsize, initial_OUTvar), pushes mixed audio through xrmatchIN,
// and pulls each device block through xrmatchOUT.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-21 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via OpenAI Codex.
//                 Faithful 48 kHz stereo wrapper over the existing WDSP
//                 rmatch/varsamp engine. It adds no feedback or correction
//                 math and preserves the engine's variable interpolation and
//                 history across every valid push/take call.
//   2026-09-23: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code. stats() also reports WDSP getControlFlag()
//                 (rmatch.h:157, rmatch.c:699-706) as controlActive, so a
//                 caller can tell a measured ratio from the initial one.
//   2026-09-23: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code. R-R3-23: the output runs at the speaker device's
//                 rate (ivac.c:41 audio_rate in, vac_rate out), with WDSP's
//                 native output quantum from cmsetup.c getbuffsize().
//   2026-09-27: J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code. R-IOS-13: forceRatio() ports IVAC forceIVACvar
//                 (ivac.c:39, 723-741) onto this matcher.
// =================================================================
//
// === Verbatim Thetis Project Files/Source/ChannelMaster/ivac.c header ===
/*  ivac.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2015-2025 Warren Pratt, NR0V
Copyright (C) 2015-2016 Doug Wigley, W5WC

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

warren@wpratt.com

*/
// === Verbatim Thetis Project Files/Source/wdsp/rmatch.c header ===
/*  rmatch.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017, 2018, 2022 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/
// === Verbatim Thetis Project Files/Source/wdsp/rmatch.h header ===
/*  rmatch.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017, 2022 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/

// === Verbatim Thetis Project Files/Source/wdsp/varsamp.c header ===
/*  varsamp.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2017 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/
// === Verbatim Thetis Project Files/Source/ChannelMaster/cmsetup.c header ===
/*  cmsetup.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2014 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/

#include "core/session/media/RemoteAudioRateMatcher.h"

#include <algorithm>
#include <cmath>

#ifdef HAVE_WDSP
extern "C" {
// Exact declarations from Thetis Project Files/Source/wdsp/rmatch.h:117-139
// [v2.10.3.15 @3759d09]. Keeping this narrow avoids importing WDSP's private
// platform synchronization types into the C++ interface.
void* create_rmatchV(int in_size, int out_size, int nom_inrate, int nom_outrate,
                     int ringsize, double var);
void destroy_rmatchV(void* ptr);
void xrmatchOUT(void* b, double* out);
void xrmatchIN(void* b, double* in);
void getRMatchDiags(void* b, int* underflows, int* overflows, double* var,
                    int* ringsize, int* nring);
void forceRMatchVar(void* b, int force, double fvar);
// From Thetis Project Files/Source/wdsp/rmatch.h:157 [v2.10.3.15 @3759d09].
void getControlFlag(void* ptr, int* control_flag);
}
#endif

namespace NereusSDR {
namespace {

// Porting from Project Files/Source/ChannelMaster/cmsetup.c:getbuffsize()
// (lines 104-111) [v2.10.3.15 @3759d09] — original C logic:
// buffer sizes are a function of sample rate to yield constant latency
// const int base_rate = 48000; const int base_size = 64;
// return base_size * rate / base_rate;
// Keep each WDSP rmatch call at this native 48 kHz quantum. The public
// packet/device dimensions are bridged by bounded carries below; WDSP's
// feedback, interpolation, phase, and ratio control remain unchanged.
constexpr int kThetisNativeBlockFrames = 64;

// From Thetis Project Files/Source/ChannelMaster/cmsetup.c:104-111
// [v2.10.3.15 @3759d09]: getbuffsize(rate) is base_size * rate / base_rate,
// so each WDSP output call covers the same time as a 48 kHz input call.
int thetisBuffSize(int rate)
{
    const int base_rate = 48000;
    const int base_size = 64;
    return base_size * rate / base_rate;
}

} // namespace

int RemoteAudioRateMatcher::filterDelayFrames(int outputRateHz)
{
    // From Thetis Project Files/Source/wdsp/varsamp.c:41-60 [v2.10.3.15 @3759d09]:
    // min_rate is the lower rate, norm_rate the input rate when the output
    // is lower (the input rate equals min_rate otherwise), and
    // rsize = (int)(140.0 * norm_rate / min_rate).
    const double inRate = double(kSampleRateHz);
    const double minRate = std::min(inRate, double(std::max(1, outputRateHz)));
    const int rsize = static_cast<int>(140.0 * inRate / minRate);
    return rsize / 2 - 1;
}

RemoteAudioRateMatcher::RemoteAudioRateMatcher() = default;

RemoteAudioRateMatcher::~RemoteAudioRateMatcher()
{
    destroy();
}

bool RemoteAudioRateMatcher::configure(int inputFrames, int outputFrames, int ringFrames,
                                       int outputRateHz)
{
    destroy();

    // A call is at most one second and the ring at most two seconds, each
    // at its own side's rate (48 kHz in, the device's rate out).
    if (outputRateHz < kMinOutputRateHz || outputRateHz > kMaxOutputRateHz
        || inputFrames <= 0 || outputFrames <= 0
        || inputFrames > kMaxFramesPerCall
        || outputFrames > outputRateHz
        || ringFrames > 2 * outputRateHz) {
        return false;
    }
    const int nativeOutputFrames = thetisBuffSize(outputRateHz);
    if (nativeOutputFrames <= 0) {
        return false;
    }

    // From Thetis Project Files/Source/wdsp/rmatch.c:132-144 [v2.10.3.15 @3759d09]:
    // nom_ratio = nom_outrate / nom_inrate;
    // max_ring_insize = (int)(1.0 + insize * (1.05 * nom_ratio)); and
    // the rmatch ring is at least twice that size and twice outsize.
    const double nomRatio = double(outputRateHz) / double(kSampleRateHz);
    const int minimumResampledFrames = static_cast<int>(1.0 + inputFrames * (1.05 * nomRatio));
    const int minimumRingFrames = 2 * std::max(minimumResampledFrames, outputFrames);
    if (ringFrames < minimumRingFrames) {
        return false;
    }

#ifdef HAVE_WDSP
    // From Thetis Project Files/Source/ChannelMaster/ivac.c:41 [v2.10.3.15 @3759d09]
    // (data FROM RADIO TO VAC): create_rmatchV(audio_size, vac_size,
    // audio_rate, vac_rate, OUTringsize, initial_OUTvar). The remote
    // receive path is the same source-to-output direction: 48 kHz audio in,
    // the speaker device's rate out.
    m_matcher = create_rmatchV(kThetisNativeBlockFrames, nativeOutputFrames,
                               kSampleRateHz, outputRateHz,
                               ringFrames, 1.0);
    if (!m_matcher) {
        return false;
    }
    // From Thetis Project Files/Source/ChannelMaster/ivac.c:41-44
    // [v2.10.3.15 @3759d09]. `force == 0` preserves WDSP's natural
    // feedback controller; fvar is inactive in that mode. R-IOS-13: a
    // caller that forced the ratio (forceRatio) keeps it across a rebuild,
    // as ivac.c:39 re-applies INforce / INfvar after create_rmatchV.
    forceRMatchVar(m_matcher, m_force ? 1 : 0, m_forcedRatio);

    m_inputFrames = inputFrames;
    m_outputFrames = outputFrames;
    m_ringFrames = ringFrames;
    m_outputRateHz = outputRateHz;
    m_nativeOutputFrames = nativeOutputFrames;
    m_inputCarry.resize(kThetisNativeBlockFrames * kChannels);
    m_nativeOutput.resize(nativeOutputFrames * kChannels);
    m_inputCarryFrames = 0;
    m_outputCarryOffsetFrames = 0;
    m_outputCarryFrames = 0;
    return true;
#else
    Q_UNUSED(inputFrames);
    Q_UNUSED(outputFrames);
    Q_UNUSED(ringFrames);
    Q_UNUSED(nativeOutputFrames);
    return false;
#endif
}

bool RemoteAudioRateMatcher::push(const QVector<float>& pcmInterleaved)
{
    if (pcmInterleaved.size() != m_inputFrames * kChannels) {
        return false;
    }
    return push(pcmInterleaved.constData(), m_inputFrames);
}

bool RemoteAudioRateMatcher::push(const float* pcmInterleaved, int frameCount)
{
    if (!m_matcher || pcmInterleaved == nullptr || frameCount != m_inputFrames) {
        return false;
    }

    for (int index = 0; index < frameCount * kChannels; ++index) {
        if (!std::isfinite(pcmInterleaved[index])) {
            return false;
        }
    }

#ifdef HAVE_WDSP
    int sourceFrame = 0;
    while (sourceFrame < m_inputFrames) {
        const int frames = std::min(kThetisNativeBlockFrames - m_inputCarryFrames,
                                    m_inputFrames - sourceFrame);
        for (int frame = 0; frame < frames; ++frame) {
            const int carrySample = (m_inputCarryFrames + frame) * kChannels;
            const int sourceSample = (sourceFrame + frame) * kChannels;
            m_inputCarry[carrySample] = pcmInterleaved[sourceSample];
            m_inputCarry[carrySample + 1] = pcmInterleaved[sourceSample + 1];
        }
        m_inputCarryFrames += frames;
        sourceFrame += frames;
        if (m_inputCarryFrames == kThetisNativeBlockFrames) {
            // From Thetis Project Files/Source/ChannelMaster/ivac.c:168
            // [v2.10.3.15 @3759d09] — xrmatchIN(rmatchOUT, buff).
            xrmatchIN(m_matcher, m_inputCarry.data());
            m_inputCarryFrames = 0;
        }
    }
    return true;
#else
    return false;
#endif
}

QVector<float> RemoteAudioRateMatcher::take()
{
    if (!m_matcher) {
        return {};
    }
    QVector<float> pcm(m_outputFrames * kChannels);
    if (!takeInto(pcm.data(), m_outputFrames)) {
        return {};
    }
    return pcm;
}

bool RemoteAudioRateMatcher::takeInto(float* pcm, int frameCount)
{
    if (!m_matcher || pcm == nullptr || frameCount != m_outputFrames) {
        return false;
    }

#ifdef HAVE_WDSP
    int destinationFrame = 0;
    while (destinationFrame < m_outputFrames) {
        if (m_outputCarryOffsetFrames == m_outputCarryFrames) {
            // From Thetis Project Files/Source/ChannelMaster/ivac.c:254
            // [v2.10.3.15 @3759d09] — xrmatchOUT(rmatchOUT, out_ptr).
            xrmatchOUT(m_matcher, m_nativeOutput.data());
            m_outputCarryOffsetFrames = 0;
            m_outputCarryFrames = m_nativeOutputFrames;
        }
        const int frames = std::min(m_outputCarryFrames - m_outputCarryOffsetFrames,
                                    m_outputFrames - destinationFrame);
        for (int frame = 0; frame < frames; ++frame) {
            const int sourceSample = (m_outputCarryOffsetFrames + frame) * kChannels;
            const int destinationSample = (destinationFrame + frame) * kChannels;
            pcm[destinationSample] = static_cast<float>(m_nativeOutput.at(sourceSample));
            pcm[destinationSample + 1] = static_cast<float>(m_nativeOutput.at(sourceSample + 1));
        }
        m_outputCarryOffsetFrames += frames;
        destinationFrame += frames;
    }
    return true;
#else
    return false;
#endif
}

bool RemoteAudioRateMatcher::canTakeWithoutUnderflow() const
{
#ifdef HAVE_WDSP
    if (!m_matcher) {
        return false;
    }
    int underflows = 0;
    int overflows = 0;
    double ratio = 1.0;
    int capacity = 0;
    int ringFill = 0;
    getRMatchDiags(m_matcher, &underflows, &overflows, &ratio, &capacity, &ringFill);
    Q_UNUSED(underflows);
    Q_UNUSED(overflows);
    Q_UNUSED(ratio);
    Q_UNUSED(capacity);
    const int carry = m_outputCarryFrames - m_outputCarryOffsetFrames;
    const int remaining = std::max(0, m_outputFrames - carry);
    const int nativeFrames = ((remaining + m_nativeOutputFrames - 1)
                              / m_nativeOutputFrames)
        * m_nativeOutputFrames;
    return ringFill >= nativeFrames;
#else
    return false;
#endif
}

void RemoteAudioRateMatcher::reset()
{
    if (m_inputFrames == 0 || m_outputFrames == 0 || m_ringFrames == 0) {
        return;
    }

    const int inputFrames = m_inputFrames;
    const int outputFrames = m_outputFrames;
    const int ringFrames = m_ringFrames;
    const int outputRateHz = m_outputRateHz;
    configure(inputFrames, outputFrames, ringFrames, outputRateHz);
}

void RemoteAudioRateMatcher::forceRatio(bool force, double ratio)
{
    m_force = force;
    m_forcedRatio = ratio;
#ifdef HAVE_WDSP
    if (m_matcher) {
        // From Thetis Project Files/Source/ChannelMaster/ivac.c:723-741
        // [v2.10.3.15 @3759d09]: forceIVACvar(id, type, force, fvar) stores
        // the pair and calls forceRMatchVar(a, force, fvar); rmatch.c:310-313
        // then resamples at fvar instead of its controlled var.
        forceRMatchVar(m_matcher, force ? 1 : 0, ratio);
    }
#endif
}

RemoteAudioRateMatcherStats RemoteAudioRateMatcher::stats() const
{
    RemoteAudioRateMatcherStats result;
#ifdef HAVE_WDSP
    if (m_matcher) {
        // From Thetis Project Files/Source/ChannelMaster/ivac.c:718 [v2.10.3.15 @3759d09]
        // — getRMatchDiags(a, underflows, overflows, var, ringsize, nring).
        getRMatchDiags(m_matcher, &result.underflows, &result.overflows,
                       &result.currentRatio, &result.ringCapacityFrames,
                       &result.ringFillFrames);
        // xrmatchOUT removes a 64-frame native block before take() returns
        // its public-sized prefix. Include the bounded suffix so callers see
        // the fill available to playback, rather than artificial free room.
        result.ringFillFrames += m_outputCarryFrames - m_outputCarryOffsetFrames;
        // From Thetis Project Files/Source/wdsp/rmatch.c:699-706 [v2.10.3.15 @3759d09]:
        // getControlFlag(ptr, control_flag). rmatch sets control_flag once
        // readsamps and writesamps both reach their startup counts
        // (rmatch.c:356, 461); until then var holds its initial value.
        int controlFlag = 0;
        getControlFlag(m_matcher, &controlFlag);
        result.controlActive = controlFlag != 0;
    }
#endif
    return result;
}

void RemoteAudioRateMatcher::destroy() noexcept
{
#ifdef HAVE_WDSP
    if (m_matcher) {
        destroy_rmatchV(m_matcher);
    }
#endif
    m_matcher = nullptr;
    m_inputFrames = 0;
    m_outputFrames = 0;
    m_ringFrames = 0;
    m_outputRateHz = kSampleRateHz;
    m_nativeOutputFrames = 0;
    m_inputCarry.clear();
    m_nativeOutput.clear();
    m_inputCarryFrames = 0;
    m_outputCarryOffsetFrames = 0;
    m_outputCarryFrames = 0;
}

} // namespace NereusSDR
