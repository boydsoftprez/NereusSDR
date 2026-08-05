// =================================================================
// src/core/TciBinaryFrame.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/TCIServer.cs,
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-10 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via
//                Anthropic Claude Code.
// =================================================================

/*  TCIServer.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

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

mw0lge@grange-lane.co.uk
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

#pragma once

#include <QtCore/QByteArray>
#include <vector>

namespace NereusSDR {

// ── TciBinaryFrame ────────────────────────────────────────────────────────────
//
// Encodes TCI binary stream frames as specified by the ExpertSDR3 TCI protocol.
// The binary frame format is byte-for-byte parity with Thetis buildStreamPayload:
//   64-byte little-endian header (16 × uint32) + interleaved sample payload.
//
// From Thetis TCIServer.cs:5240-5305 [v2.10.3.13]

// Sample type encoding per Thetis enum TCISampleType (TCIServer.cs:352 [v2.10.3.13])
enum class TciSampleType : int {
    Int16   = 0,
    Int24   = 1,
    Int32   = 2,
    Float32 = 3,
};

// Stream type encoding per Thetis enum TCIStreamType (TCIServer.cs:343 [v2.10.3.13])
enum class TciStreamType : int {
    IqStream      = 0,
    RxAudioStream = 1,
    TxAudioStream = 2,
    TxChrono      = 3,
    LineoutStream = 4,
};

class TciBinaryFrame {
public:
    // From Thetis TCIServer.cs:5240-5262 [v2.10.3.13] — buildStreamPayload.
    //
    // Constructs a TCI binary frame with a 64-byte LE header (16 × uint32)
    // followed by the encoded sample payload. When samples is nullptr the
    // header-only (64-byte) frame is returned (used for TX_CHRONO and probe
    // frames).
    //
    // Header layout (offsets in bytes):
    //   0  : receiver index (uint32 LE)
    //   4  : sample rate    (uint32 LE)
    //   8  : sample type    (uint32 LE) — see TciSampleType
    //   12 : reserved       (uint32 LE, always 0)
    //   16 : reserved       (uint32 LE, always 0)
    //   20 : sample count   (uint32 LE) — samples PER CHANNEL
    //   24 : stream type    (uint32 LE) — see TciStreamType
    //   28 : channels       (uint32 LE) — 1 mono, 2 stereo interleaved
    //   32..60 : reserved   (8 × uint32 LE, always 0)
    //
    // sampleCount is the number of samples PER CHANNEL (not total interleaved
    // samples), consistent with Thetis TCIServer.cs:5510 [v2.10.3.13]:
    //   buildStreamPayload(receiver, sampleRate, sampleType,
    //                      interleaved.Length, RX_AUDIO_STREAM, channels, ...)
    // where interleaved.Length is the flat array length (sampleCount * channels).
    // The length field in the header carries the flat count, matching Thetis.
    //
    // NOTE: `length` here is the flat count (sampleCount * channels) to exactly
    // match the Thetis argument — Thetis passes interleaved.Length at cs:5510.
    static QByteArray buildStreamPayload(int receiver, int sampleRate,
                                         int sampleType, int length,
                                         int streamType, int channels,
                                         const float* samples);

    // From Thetis TCIServer.cs:5264-5305 [v2.10.3.13] — encodeSamples.
    //
    // Encodes a float array per sampleType:
    //   Float32 — direct memcpy of IEEE-754 representation.
    //   Int16   — clip to [-1, +1], scale by 32767, round, write 2 LE bytes.
    //   Int24   — clip to [-1, +1], scale by 8388607, round, write 3 LE bytes.
    //   Int32   — clip to [-1, +1], scale by INT_MAX, round, write 4 LE bytes.
    //
    // sampleCount is the total number of float elements in the samples array
    // (i.e. channels * framesPerChannel for interleaved stereo).
    static QByteArray encodeSamples(const float* samples, int sampleCount,
                                    int sampleType);

    // From Thetis TCIServer.cs:5307-5337 [v2.10.3.13] — decodeSamples.
    //
    // Decodes `sampleCount` values from `payload` starting at `dataOffset`
    // bytes, returning them as a vector of floats.  Symmetric inverse of
    // encodeSamples.  Per-type decoding:
    //   Float32 — direct memcpy of IEEE-754 representation.
    //   Int16   — read 2 LE bytes as signed int16, divide by 32768.0f.
    //   Int24   — read 3 LE bytes, sign-extend from bit 23, divide by 8388608.0f.
    //   Int32   — read 4 LE bytes as signed int32, divide by 2147483648.0f.
    //
    // `dataOffset` is the byte offset within `payload` where sample data begins
    // (64 for TCI binary frames — after the fixed header).
    // `sampleCount` is the total number of decoded floats to produce.
    // Unknown sampleType is treated as Float32.
    static std::vector<float> decodeSamples(const QByteArray& payload,
                                             int dataOffset, int sampleCount,
                                             int sampleType);

    // Returns the byte width per encoded sample for the given sampleType.
    // Used by handleBinaryFrame to compute actualValueCount from the raw
    // payload byte count.
    // From Thetis TCIServer.cs:5617 [v2.10.3.13] — getBytesPerSample.
    static int bytesPerSample(int sampleType);
};

} // namespace NereusSDR
