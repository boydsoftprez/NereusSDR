// =================================================================
// src/core/TciBinaryFrame.cpp  (NereusSDR)
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

#include "TciBinaryFrame.h"

#include <QtEndian>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <vector>

namespace NereusSDR {

// ── writeUInt32LE ────────────────────────────────────────────────────────────
//
// From Thetis TCIServer.cs:5234-5238 [v2.10.3.13] — writeUInt32.
// Writes a 32-bit unsigned integer in little-endian byte order at offset
// within the given byte array. Direct translation of:
//   buffer[offset]     = (byte)(value & 0xFF);
//   buffer[offset + 1] = (byte)((value >> 8) & 0xFF);
//   buffer[offset + 2] = (byte)((value >> 16) & 0xFF);
//   buffer[offset + 3] = (byte)((value >> 24) & 0xFF);
static void writeUInt32LE(char* buf, int offset, quint32 value)
{
    // qToLittleEndian handles host-endian → LE conversion in one call.
    quint32 le = qToLittleEndian(value);
    std::memcpy(buf + offset, &le, sizeof(le));
}

// ── buildStreamPayload ────────────────────────────────────────────────────────
//
// From Thetis TCIServer.cs:5240-5262 [v2.10.3.13] — buildStreamPayload.
// Direct port of:
//   writeUInt32(payload, 0,  (uint)receiver);
//   writeUInt32(payload, 4,  (uint)sampleRate);
//   writeUInt32(payload, 8,  (uint)sampleType);
//   writeUInt32(payload, 12, 0);   // reserved
//   writeUInt32(payload, 16, 0);   // reserved
//   writeUInt32(payload, 20, (uint)length);
//   writeUInt32(payload, 24, (uint)streamType);
//   writeUInt32(payload, 28, (uint)channels);
//   for (int i = 0; i < 8; i++) writeUInt32(payload, 32+4*i, 0);  // reserved
//   Buffer.BlockCopy(samplePayload, 0, payload, 64, samplePayloadLength);
//
// When samples is nullptr this produces the 64-byte header-only frame
// (corresponds to Thetis's Array.Empty<byte>() TX_CHRONO payload).
QByteArray TciBinaryFrame::buildStreamPayload(int receiver, int sampleRate,
                                               int sampleType, int length,
                                               int streamType, int channels,
                                               const float* samples)
{
    // samples == nullptr → header-only; caller passes non-null + length > 0
    // for audio/IQ frames. Encode first so we know the payload byte count.
    QByteArray samplePayload;
    if (samples != nullptr && length > 0) {
        samplePayload = encodeSamples(samples, length, sampleType);
    }

    const int samplePayloadLength = samplePayload.size();  // From Thetis cs:5243
    QByteArray payload(64 + samplePayloadLength, '\0');    // From Thetis cs:5244
    char* buf = payload.data();

    writeUInt32LE(buf,  0, static_cast<quint32>(receiver));    // 0  receiver
    writeUInt32LE(buf,  4, static_cast<quint32>(sampleRate));  // 4  sample rate
    writeUInt32LE(buf,  8, static_cast<quint32>(sampleType));  // 8  sample type
    writeUInt32LE(buf, 12, 0u);                                // 12 reserved
    writeUInt32LE(buf, 16, 0u);                                // 16 reserved
    writeUInt32LE(buf, 20, static_cast<quint32>(length));      // 20 sample count
    writeUInt32LE(buf, 24, static_cast<quint32>(streamType));  // 24 stream type
    writeUInt32LE(buf, 28, static_cast<quint32>(channels));    // 28 channels

    // From Thetis cs:5259-5261 — 8 reserved uint32 fields at offsets 32..60
    for (int i = 0; i < 8; ++i) {
        writeUInt32LE(buf, 32 + 4 * i, 0u);
    }

    // From Thetis cs:5263 — Buffer.BlockCopy(samplePayload, 0, payload, 64, ...)
    if (samplePayloadLength > 0) {
        std::memcpy(buf + 64, samplePayload.constData(), samplePayloadLength);
    }

    return payload;
}

// ── encodeSamples ─────────────────────────────────────────────────────────────
//
// From Thetis TCIServer.cs:5264-5305 [v2.10.3.13] — encodeSamples.
// sampleCount is the total number of floats in the samples array (interleaved
// channels included).  Direct port of:
//
//   if (sampleType == TCISampleType.FLOAT32)
//       Buffer.BlockCopy(samples, 0, data, 0, data.Length);
//   else {
//       for each sample:
//           clippedSample = Math.Max(-1.0f, Math.Min(1.0f, sample));
//           INT16:  short s16 = (short)Math.Round(clippedSample * short.MaxValue);
//           INT24:  int s24   = (int)Math.Round(clippedSample * 8388607.0f);
//           INT32:  int s32   = (int)Math.Round(clippedSample * int.MaxValue);
//           write 2/3/4 LE bytes
//   }
QByteArray TciBinaryFrame::encodeSamples(const float* samples, int sampleCount,
                                          int sampleType)
{
    if (!samples || sampleCount <= 0) {
        return {};  // From Thetis cs:5265: return Array.Empty<byte>()
    }

    // From Thetis cs:5267 — getBytesPerSample
    int bytesPerSample = 0;
    switch (static_cast<TciSampleType>(sampleType)) {
        case TciSampleType::Int16:   bytesPerSample = 2; break;
        case TciSampleType::Int24:   bytesPerSample = 3; break;
        case TciSampleType::Int32:   bytesPerSample = 4; break;
        case TciSampleType::Float32: bytesPerSample = 4; break;
        default:                     bytesPerSample = 4; break;
    }

    const qsizetype dataSize =
        static_cast<qsizetype>(sampleCount) * static_cast<qsizetype>(bytesPerSample);
    if (dataSize <= 0 || dataSize > INT_MAX) {
        return {};
    }

    QByteArray data(static_cast<int>(dataSize), '\0');
    char* buf = data.data();

    // From Thetis cs:5272-5277 — FLOAT32 fast path: Buffer.BlockCopy(samples, 0, data, 0, data.Length)
    if (static_cast<TciSampleType>(sampleType) == TciSampleType::Float32) {
        const qsizetype floatBytes =
            static_cast<qsizetype>(sampleCount) * static_cast<qsizetype>(sizeof(float));
        if (floatBytes <= 0 || floatBytes > INT_MAX) {
            return {};
        }
        std::memcpy(buf, samples, static_cast<size_t>(floatBytes));
        return data;
    }

    // From Thetis cs:5279-5306 — INT16 / INT24 / INT32 per-sample path
    int offset = 0;
    for (int i = 0; i < sampleCount; ++i) {
        // From Thetis cs:5281 — clippedSample = Math.Max(-1.0f, Math.Min(1.0f, samples[i]))
        const float clipped = std::max(-1.0f, std::min(1.0f, samples[i]));

        switch (static_cast<TciSampleType>(sampleType)) {
            case TciSampleType::Int16: {
                // From Thetis cs:5284-5287:
                //   short s16 = (short)Math.Round(clippedSample * short.MaxValue);
                //   data[offset++] = (byte)(s16 & 0xFF);
                //   data[offset++] = (byte)((s16 >> 8) & 0xFF);
                const qint16 s16 = static_cast<qint16>(std::lroundf(clipped * 32767.0f));
                buf[offset++] = static_cast<char>(s16 & 0xFF);
                buf[offset++] = static_cast<char>((s16 >> 8) & 0xFF);
                break;
            }
            case TciSampleType::Int24: {
                // From Thetis cs:5289-5293:
                //   int s24 = (int)Math.Round(clippedSample * 8388607.0f);
                //   data[offset++] = (byte)(s24 & 0xFF);
                //   data[offset++] = (byte)((s24 >> 8) & 0xFF);
                //   data[offset++] = (byte)((s24 >> 16) & 0xFF);
                const qint32 s24 = static_cast<qint32>(std::lroundf(clipped * 8388607.0f));
                buf[offset++] = static_cast<char>(s24 & 0xFF);
                buf[offset++] = static_cast<char>((s24 >> 8) & 0xFF);
                buf[offset++] = static_cast<char>((s24 >> 16) & 0xFF);
                break;
            }
            case TciSampleType::Int32: {
                // From Thetis cs:5295-5300:
                //   int s32 = (int)Math.Round(clippedSample * int.MaxValue);
                //   data[offset++] = (byte)(s32 & 0xFF);
                //   data[offset++] = (byte)((s32 >> 8) & 0xFF);
                //   data[offset++] = (byte)((s32 >> 16) & 0xFF);
                //   data[offset++] = (byte)((s32 >> 24) & 0xFF);
                //
                // C# int.MaxValue == 2147483647 == INT_MAX
                const qint32 s32 = static_cast<qint32>(std::lroundf(clipped * static_cast<float>(INT_MAX)));
                buf[offset++] = static_cast<char>(s32 & 0xFF);
                buf[offset++] = static_cast<char>((s32 >> 8) & 0xFF);
                buf[offset++] = static_cast<char>((s32 >> 16) & 0xFF);
                buf[offset++] = static_cast<char>((s32 >> 24) & 0xFF);
                break;
            }
            default:
                // Unreachable — bytesPerSample already excludes Float32
                offset += bytesPerSample;
                break;
        }
    }

    return data;
}

// ── bytesPerSample ────────────────────────────────────────────────────────────
//
// From Thetis TCIServer.cs:5617 [v2.10.3.13] — getBytesPerSample.
// Returns the byte width for each encoded sample value of the given type.
// Used by handleBinaryFrame to compute the actual value count from raw payload
// byte length: actualValueCount = actualDataBytes / bytesPerSample(sampleType).
int TciBinaryFrame::bytesPerSample(int sampleType)
{
    switch (static_cast<TciSampleType>(sampleType)) {
        case TciSampleType::Int16:   return 2;
        case TciSampleType::Int24:   return 3;
        case TciSampleType::Int32:   return 4;
        case TciSampleType::Float32: return 4;
        default:                     return 4;  // Float32 fallthrough
    }
}

// ── decodeSamples ─────────────────────────────────────────────────────────────
//
// From Thetis TCIServer.cs:5307-5337 [v2.10.3.13] — decodeSamples.
// Symmetric inverse of encodeSamples. Direct port of:
//
//   float[] samples = new float[length];
//   int offset = dataOffset;
//   for (int i = 0; i < length; i++) {
//       switch (sampleType) {
//           case INT16:
//               samples[i] = BitConverter.ToInt16(payload, offset) / 32768.0f;
//               offset += 2; break;
//           case INT24:
//               int s24 = payload[offset] | (payload[offset+1]<<8) | (payload[offset+2]<<16);
//               if ((s24 & 0x800000) != 0) s24 |= unchecked((int)0xFF000000);
//               samples[i] = s24 / 8388608.0f; offset += 3; break;
//           case INT32:
//               samples[i] = BitConverter.ToInt32(payload, offset) / 2147483648.0f;
//               offset += 4; break;
//           case FLOAT32: default:
//               samples[i] = BitConverter.ToSingle(payload, offset);
//               offset += 4; break;
//       }
//   }
//   return samples;
std::vector<float> TciBinaryFrame::decodeSamples(const QByteArray& payload,
                                                   int dataOffset, int sampleCount,
                                                   int sampleType)
{
    if (sampleCount <= 0 || dataOffset < 0) {
        return {};  // From Thetis cs:5309 — return new float[length] (empty for length<=0)
    }

    std::vector<float> samples(static_cast<size_t>(sampleCount), 0.0f);
    const auto* bytes = reinterpret_cast<const unsigned char*>(payload.constData());
    const int payloadSize = payload.size();
    int offset = dataOffset;

    for (int i = 0; i < sampleCount; ++i) {
        switch (static_cast<TciSampleType>(sampleType)) {
            case TciSampleType::Int16: {
                // From Thetis cs:5316:
                //   samples[i] = BitConverter.ToInt16(payload, offset) / 32768.0f;
                if (offset + 2 > payloadSize) { break; }
                const qint16 s16 = static_cast<qint16>(
                    static_cast<quint16>(bytes[offset]) |
                    (static_cast<quint16>(bytes[offset + 1]) << 8));
                samples[static_cast<size_t>(i)] = s16 / 32768.0f;
                offset += 2;
                break;
            }
            case TciSampleType::Int24: {
                // From Thetis cs:5319-5322:
                //   int s24 = payload[offset] | (payload[offset+1]<<8) | (payload[offset+2]<<16);
                //   if ((s24 & 0x800000) != 0) s24 |= unchecked((int)0xFF000000);
                //   samples[i] = s24 / 8388608.0f;
                if (offset + 3 > payloadSize) { break; }
                qint32 s24 = static_cast<qint32>(bytes[offset])
                           | (static_cast<qint32>(bytes[offset + 1]) << 8)
                           | (static_cast<qint32>(bytes[offset + 2]) << 16);
                if (s24 & 0x800000) {
                    s24 |= static_cast<qint32>(0xFF000000u);
                }
                samples[static_cast<size_t>(i)] = s24 / 8388608.0f;
                offset += 3;
                break;
            }
            case TciSampleType::Int32: {
                // From Thetis cs:5325-5326:
                //   samples[i] = BitConverter.ToInt32(payload, offset) / 2147483648.0f;
                if (offset + 4 > payloadSize) { break; }
                qint32 s32 = static_cast<qint32>(bytes[offset])
                           | (static_cast<qint32>(bytes[offset + 1]) << 8)
                           | (static_cast<qint32>(bytes[offset + 2]) << 16)
                           | (static_cast<qint32>(bytes[offset + 3]) << 24);
                samples[static_cast<size_t>(i)] = s32 / 2147483648.0f;
                offset += 4;
                break;
            }
            case TciSampleType::Float32:
            default: {
                // From Thetis cs:5330-5331:
                //   samples[i] = BitConverter.ToSingle(payload, offset);
                if (offset + 4 > payloadSize) { break; }
                float v = 0.0f;
                std::memcpy(&v, bytes + offset, 4);
                samples[static_cast<size_t>(i)] = v;
                offset += 4;
                break;
            }
        }
    }

    return samples;
}

} // namespace NereusSDR
