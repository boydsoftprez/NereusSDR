// =================================================================
// src/core/audio/DeviceSampleFormat.h  (NereusSDR)
// =================================================================
// Independently implemented from Thetis cmASIO interface.
// no-port-check: NereusSDR-original interface.  The conversion logic is
// ported in DeviceSampleFormat.cpp, which carries the upstream headers and
// cites.
//
// Conversion between the 48 kHz stereo float the engines carry and a
// device's own sample format and channel layout (R-AUD-07).  Both calls
// run in a device callback: no lock, no allocation, no system call.
//
// writeStereoToDevice puts left and right on the pair's two channels and
// zero on every other channel; a one-channel device or a one-channel pair
// gets left plus right, halved.  readDeviceToStereo takes the pair's
// channels and makes stereo from them by the mic pick: Left copies the
// pair's first channel to both sides, Right its second, Both adds the two
// into both sides.
//
// interleaved true: dst / src hold frames of deviceChannels samples.
// interleaved false: planes[c] holds channel c (0-based) alone, as ASIO's
// one buffer per channel and Core Audio's non-interleaved lists do.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-07). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/AudioDeviceTypes.h"

namespace NereusSDR {

enum class DeviceSampleFormat {
    Float32,
    Float64,
    Int16,
    Int24Packed,     // three bytes, little-endian
    Int24In32Lsb,    // a 24-bit value in the low three bytes of a little-endian 32-bit word
    Int32
};

// Bytes one sample of the format takes.
int deviceSampleBytes(DeviceSampleFormat format);

void writeStereoToDevice(const float* stereo, int frames, void* dst,
                         DeviceSampleFormat format, int deviceChannels,
                         AudioChannelPair pair, bool interleaved,
                         void* const* planes);

void readDeviceToStereo(const void* src, const void* const* planes,
                        bool interleaved, DeviceSampleFormat format,
                        int deviceChannels, AudioChannelPair pair,
                        MicChannelPick pick, int frames, float* stereo);

} // namespace NereusSDR
