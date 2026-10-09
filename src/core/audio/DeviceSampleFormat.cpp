// =================================================================
// src/core/audio/DeviceSampleFormat.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/ChannelMaster/cmasio.c, original licence from
//   Thetis source is included below
//   Project Files/Source/ChannelMaster/ivac.c, original licence from
//   Thetis source is included below
//
// Conversion between stereo float and a device's sample format and
// channel layout (R-AUD-07): cmASIO's callback scaling (float times
// 2^(bits - 1) as a double, clamped, then cast; integer times
// 1 / 2^(bits - 1)), generalised from its 32-bit form to 16- and 24-bit
// and to float devices, and cmASIO's input mode with Thetis's combinebuff
// for the mic pick.  Differences from the upstream, each marked where it
// happens:
//   * one conversion for any device channel count and any pair, where
//     cmASIO converts one fixed stereo pair of 32-bit buffers;
//   * Left and Right picks are not doubled (see readDeviceToStereo).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09: Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted transformation via Anthropic
//               Claude Code.  Native audio plan Task 3 (R-AUD-07).
// =================================================================
//
// --- From cmasio.c ---
/*  cmasio.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2023 Bryan Rambo W4WMT

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

bryanr@bometals.com

*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//
//
// --- From ivac.c ---
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

#include "core/audio/DeviceSampleFormat.h"

#include <cstdint>
#include <cstring>

namespace NereusSDR {

namespace {

// The integer scaling of cmASIO's callback, for its 32-bit form:
// From Thetis Project Files/Source/ChannelMaster/cmasio.c:152-155 [v2.10.3.15 @3759d09]
//   const double inv_scale = 1.0 / 2147483648.0;
//   const double scale = 2147483648.0;
//   const double max_i32 = 2147483647.0;
//   const double min_i32 = -2147483648.0;
// The same rule with 2^15 for Int16 and 2^23 for the 24-bit forms.
constexpr double kScale32 = 2147483648.0;
constexpr double kInvScale32 = 1.0 / 2147483648.0;
constexpr double kMax32 = 2147483647.0;
constexpr double kMin32 = -2147483648.0;
constexpr double kScale24 = 8388608.0;
constexpr double kInvScale24 = 1.0 / 8388608.0;
constexpr double kMax24 = 8388607.0;
constexpr double kMin24 = -8388608.0;
constexpr double kScale16 = 32768.0;
constexpr double kInvScale16 = 1.0 / 32768.0;
constexpr double kMax16 = 32767.0;
constexpr double kMin16 = -32768.0;

// A one-channel device or pair gets left plus right, halved.
constexpr double kMonoFold = 0.5;

// float to integer as cmASIO's output loop does:
// From Thetis Project Files/Source/ChannelMaster/cmasio.c:190-200 [v2.10.3.15 @3759d09]
// scale as a double, clamp, then cast (truncation toward zero).
inline std::int32_t toInteger(double v, double scale, double maxV, double minV)
{
    double d = v * scale;
    if (d > maxV) {
        d = maxV;
    } else if (d < minV) {
        d = minV;
    }
    return static_cast<std::int32_t>(d);
}

void storeSample(std::byte* p, DeviceSampleFormat format, double v)
{
    switch (format) {
    case DeviceSampleFormat::Float32: {
        const float f = static_cast<float>(v);
        std::memcpy(p, &f, sizeof f);
        return;
    }
    case DeviceSampleFormat::Float64:
        std::memcpy(p, &v, sizeof v);
        return;
    case DeviceSampleFormat::Int16: {
        const auto s = static_cast<std::int16_t>(toInteger(v, kScale16, kMax16, kMin16));
        std::memcpy(p, &s, sizeof s);
        return;
    }
    case DeviceSampleFormat::Int24Packed: {
        const auto u = static_cast<std::uint32_t>(toInteger(v, kScale24, kMax24, kMin24));
        p[0] = static_cast<std::byte>(u & 0xFFu);
        p[1] = static_cast<std::byte>((u >> 8) & 0xFFu);
        p[2] = static_cast<std::byte>((u >> 16) & 0xFFu);
        return;
    }
    case DeviceSampleFormat::Int24In32Lsb: {
        const std::int32_t s = toInteger(v, kScale24, kMax24, kMin24);
        std::memcpy(p, &s, sizeof s);
        return;
    }
    case DeviceSampleFormat::Int32: {
        const std::int32_t s = toInteger(v, kScale32, kMax32, kMin32);
        std::memcpy(p, &s, sizeof s);
        return;
    }
    }
}

// integer to float as cmASIO's input loop does:
// From Thetis Project Files/Source/ChannelMaster/cmasio.c:180-181 [v2.10.3.15 @3759d09]
// (double)in[i] * inv_scale.
double loadSample(const std::byte* p, DeviceSampleFormat format)
{
    switch (format) {
    case DeviceSampleFormat::Float32: {
        float f = 0.0f;
        std::memcpy(&f, p, sizeof f);
        return static_cast<double>(f);
    }
    case DeviceSampleFormat::Float64: {
        double d = 0.0;
        std::memcpy(&d, p, sizeof d);
        return d;
    }
    case DeviceSampleFormat::Int16: {
        std::int16_t s = 0;
        std::memcpy(&s, p, sizeof s);
        return static_cast<double>(s) * kInvScale16;
    }
    case DeviceSampleFormat::Int24Packed: {
        std::uint32_t u = static_cast<std::uint32_t>(p[0])
            | (static_cast<std::uint32_t>(p[1]) << 8)
            | (static_cast<std::uint32_t>(p[2]) << 16);
        if ((u & 0x800000u) != 0) {
            u |= 0xFF000000u;   // sign-extend the 24-bit value
        }
        return static_cast<double>(static_cast<std::int32_t>(u)) * kInvScale24;
    }
    case DeviceSampleFormat::Int24In32Lsb: {
        std::uint32_t u = 0;
        std::memcpy(&u, p, sizeof u);
        u &= 0x00FFFFFFu;
        if ((u & 0x800000u) != 0) {
            u |= 0xFF000000u;   // the low three bytes carry the value
        }
        return static_cast<double>(static_cast<std::int32_t>(u)) * kInvScale24;
    }
    case DeviceSampleFormat::Int32: {
        std::int32_t s = 0;
        std::memcpy(&s, p, sizeof s);
        return static_cast<double>(s) * kInvScale32;
    }
    }
    return 0.0;
}

// Where sample (frame, channel) lives; channel is 0-based.
inline std::byte* sampleAt(void* dst, void* const* planes, bool interleaved,
                           int bytes, int deviceChannels, int frame, int channel)
{
    if (interleaved) {
        return static_cast<std::byte*>(dst)
            + (static_cast<std::ptrdiff_t>(frame) * deviceChannels + channel) * bytes;
    }
    return static_cast<std::byte*>(planes[channel])
        + static_cast<std::ptrdiff_t>(frame) * bytes;
}

inline const std::byte* sampleAt(const void* src, const void* const* planes, bool interleaved,
                                 int bytes, int deviceChannels, int frame, int channel)
{
    if (interleaved) {
        return static_cast<const std::byte*>(src)
            + (static_cast<std::ptrdiff_t>(frame) * deviceChannels + channel) * bytes;
    }
    return static_cast<const std::byte*>(planes[channel])
        + static_cast<std::ptrdiff_t>(frame) * bytes;
}

} // namespace

int deviceSampleBytes(DeviceSampleFormat format)
{
    switch (format) {
    case DeviceSampleFormat::Float32:
        return 4;
    case DeviceSampleFormat::Float64:
        return 8;
    case DeviceSampleFormat::Int16:
        return 2;
    case DeviceSampleFormat::Int24Packed:
        return 3;
    case DeviceSampleFormat::Int24In32Lsb:
        return 4;
    case DeviceSampleFormat::Int32:
        return 4;
    }
    return 4;
}

void writeStereoToDevice(const float* stereo, int frames, void* dst,
                         DeviceSampleFormat format, int deviceChannels,
                         AudioChannelPair pair, bool interleaved,
                         void* const* planes)
{
    if (stereo == nullptr || frames <= 0 || deviceChannels <= 0) {
        return;
    }
    if (interleaved ? dst == nullptr : planes == nullptr) {
        return;
    }
    const int bytes = deviceSampleBytes(format);
    const bool mono = deviceChannels == 1 || pair.channelCount == 1;
    const int left = pair.firstChannel - 1;
    const int right = mono ? -1 : left + 1;
    for (int f = 0; f < frames; ++f) {
        const double l = static_cast<double>(stereo[2 * f]);
        const double r = static_cast<double>(stereo[2 * f + 1]);
        for (int c = 0; c < deviceChannels; ++c) {
            double v = 0.0;
            if (c == left) {
                v = mono ? (l + r) * kMonoFold : l;
            } else if (c == right) {
                v = r;
            }
            if (!interleaved && planes[c] == nullptr) {
                continue;
            }
            storeSample(sampleAt(dst, planes, interleaved, bytes, deviceChannels, f, c), format, v);
        }
    }
}

// The mic pick, from cmASIO's input mode and Thetis's combinebuff:
//[2.10.3.13]MW0LGE added input mode, so can use ch1(L), ch2(R), or both for input
//clamp and speed refactor
// [original inline comments from cmasio.c:148-149, above Bryan Rambo
// W4WMT's CallbackASIO]
// From Thetis Project Files/Source/ChannelMaster/cmasio.c:148-171 [v2.10.3.15 @3759d09]
//   IM_LEFT:  inR = inL;   IM_RIGHT: inL = inR;
// From Thetis Project Files/Source/ChannelMaster/ivac.c:694-698 [v2.10.3.15 @3759d09]
//   combined[i] = combined[i + 1] = a[i] + a[i + 1];
// NereusSDR difference: Thetis's asioIN runs combinebuff after every input
// mode (cmasio.c:127, 133), so its Left and Right picks reach the mic at
// twice the channel's level.  Here Left and Right copy the one channel to
// both sides at its own level, and only Both adds the two.
void readDeviceToStereo(const void* src, const void* const* planes,
                        bool interleaved, DeviceSampleFormat format,
                        int deviceChannels, AudioChannelPair pair,
                        MicChannelPick pick, int frames, float* stereo)
{
    if (stereo == nullptr || frames <= 0) {
        return;
    }
    const int first = pair.firstChannel - 1;
    const bool mono = deviceChannels == 1 || pair.channelCount == 1;
    const int second = mono ? first : first + 1;
    const bool usable = deviceChannels > 0 && first >= 0 && second < deviceChannels
        && (interleaved ? src != nullptr
                        : (planes != nullptr && planes[first] != nullptr
                           && planes[second] != nullptr));
    if (!usable) {
        std::memset(stereo, 0, sizeof(float) * 2 * static_cast<std::size_t>(frames));
        return;
    }
    const int bytes = deviceSampleBytes(format);
    for (int f = 0; f < frames; ++f) {
        const double inL = loadSample(sampleAt(src, planes, interleaved, bytes, deviceChannels, f, first), format);
        const double inR = loadSample(sampleAt(src, planes, interleaved, bytes, deviceChannels, f, second), format);
        double v = 0.0;
        if (mono) {
            v = inL;   // one channel: every pick hears it once
        } else if (pick == MicChannelPick::Left) {
            v = inL;
        } else if (pick == MicChannelPick::Right) {
            v = inR;
        } else {
            v = inL + inR;
        }
        stereo[2 * f] = static_cast<float>(v);
        stereo[2 * f + 1] = static_cast<float>(v);
    }
}

} // namespace NereusSDR
