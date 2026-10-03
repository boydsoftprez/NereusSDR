// =================================================================
// src/core/ParaEqEnvelope.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/Common.cs, original licence from
//   Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Reimplemented in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Phase 3M-3a-ii follow-up Batch 6 — implementation
//                 pairs with ParaEqEnvelope.h.  Uses zlib's
//                 deflateInit2/inflateInit2 with windowBits=15+16=31
//                 (gzip format, NOT raw deflate, NOT zlib format) and
//                 compression level Z_BEST_COMPRESSION (9) to mirror
//                 the .NET CompressionLevel.Optimal Thetis selects.
//   2026-09-30 — The gzip header's OS byte is 0 on every platform, as
//                 Thetis's .NET GZipStream writes it, instead of zlib's
//                 build-dependent code (0x13 macOS, 0x03 Linux), so the
//                 Core sends the same envelope on every platform (CI:
//                 the link conformance fixtures failed on Linux).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
// =================================================================

//=================================================================
// common.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2012  FlexRadio Systems
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2025 Richard Samphire (MW0LGE)
//=================================================================
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

#include "ParaEqEnvelope.h"

#include <QByteArray>
#include <zlib.h>

namespace NereusSDR {
namespace ParaEqEnvelope {

namespace {

// gzip windowBits = 15 (max LZ77 window) + 16 (gzip wrapper).  Raw
// deflate would be -15 and zlib-format would be 15; the +16 magic
// flips the wrapper to gzip with a CRC-32 trailer.  Same value used on
// both encode (deflateInit2) and decode (inflateInit2).
constexpr int kGzipWindowBits = 15 + 16;

// Intermediate buffer for streaming deflate/inflate.  16 KiB matches
// the order of magnitude of Thetis's 8 KiB read buffer
// (Common.cs:1780); larger amortises syscall-style overhead in zlib.
constexpr int kChunkSize = 16384;

// gzip header OS byte (RFC 1952 section 2.3.1). zlib writes its build's
// OS_CODE there (0x13 on macOS, 0x03 on Linux, 0x0a for a Windows build),
// so the same input encoded to different bytes on each platform. Thetis's
// .NET GZipStream writes 0 on every machine: its stored blob at
// database.cs:11211 [v2.10.3.15] (_default_settings) begins
// 1f 8b 08 00 00 00 00 00 04 00. The XFL byte (04 there) stays zlib's
// own, which deflateSetHeader does not set.
constexpr int kGzipHeaderOs = 0;

}  // namespace

// From Thetis Common.cs:1745-1762 [v2.10.3.13].
QString encode(const QString& payload)
{
    // Thetis: "if (string.IsNullOrEmpty(uncompressed_input)) return null;"
    // Common.cs:1747 [v2.10.3.13].
    if (payload.isEmpty()) {
        return QString();
    }

    // Thetis: "byte[] input_bytes = Encoding.UTF8.GetBytes(uncompressed_input);"
    // Common.cs:1749 [v2.10.3.13].
    const QByteArray utf8 = payload.toUtf8();

    z_stream strm = {};
    // Thetis: "new GZipStream(output_stream, CompressionLevel.Optimal, true)"
    // Common.cs:1753 [v2.10.3.13].  CompressionLevel.Optimal == zlib level 9.
    // mem level 8 is zlib's documented default — match it explicitly so the
    // call is self-documenting.
    if (deflateInit2(&strm,
                     Z_BEST_COMPRESSION,
                     Z_DEFLATED,
                     kGzipWindowBits,
                     8,
                     Z_DEFAULT_STRATEGY) != Z_OK) {
        return QString();
    }
    // mtime 0, no name, comment or extra field, and the OS byte above.
    gz_header header = {};
    header.os = kGzipHeaderOs;
    if (deflateSetHeader(&strm, &header) != Z_OK) {
        deflateEnd(&strm);
        return QString();
    }

    QByteArray gz;
    char chunk[kChunkSize];

    strm.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(utf8.constData()));
    strm.avail_in = static_cast<uInt>(utf8.size());

    int ret = Z_OK;
    do {
        strm.next_out  = reinterpret_cast<Bytef*>(chunk);
        strm.avail_out = static_cast<uInt>(kChunkSize);
        ret = deflate(&strm, Z_FINISH);
        // Z_STREAM_ERROR (state inconsistency) and Z_BUF_ERROR
        // (no progress made) both indicate we cannot complete the
        // flush.  In practice neither can fire here — the input lives
        // entirely in `utf8` and we hand it over in one shot — but
        // bailing rather than looping is the defensive choice (the
        // decode side hit the same loop-forever issue on truncated
        // input before the matching guard was added).
        if (ret == Z_STREAM_ERROR || ret == Z_BUF_ERROR) {
            deflateEnd(&strm);
            return QString();
        }
        gz.append(chunk, kChunkSize - static_cast<int>(strm.avail_out));
    } while (ret != Z_STREAM_END);

    deflateEnd(&strm);

    // Thetis: "string base64 = Convert.ToBase64String(compressed_bytes);
    //          string result = base64.Replace('+', '-').Replace('/', '_').TrimEnd('=');"
    // Common.cs:1758-1759 [v2.10.3.13].  Qt's Base64UrlEncoding flag
    // performs the +/- and // substitution; OmitTrailingEquals does the
    // TrimEnd.  The two-step Thetis dance is equivalent to one Qt call.
    return QString::fromLatin1(gz.toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

// From Thetis Common.cs:1764-1790 [v2.10.3.13].
std::optional<QString> decode(const QString& blob, qsizetype maxDecodedBytes)
{
    // Thetis: "if (string.IsNullOrEmpty(compressed_input)) return null;"
    // Common.cs:1766 [v2.10.3.13].
    if (blob.isEmpty()) {
        return std::nullopt;
    }

    // Thetis: "string base64 = compressed_input.Replace('-', '+').Replace('_', '/');
    //          int pad = base64.Length % 4;
    //          if (pad == 2) base64 += "=="; else if (pad == 3) base64 += "=";
    //          else if (pad == 1) throw new FormatException(...);
    //          byte[] compressed_bytes = Convert.FromBase64String(base64);"
    // Common.cs:1768-1774 [v2.10.3.13].  Qt's Base64UrlEncoding flag
    // re-substitutes -/+ and _/// and OmitTrailingEquals tolerates the
    // trimmed trailing equals; pad==1 is invalid base64 and Qt's
    // fromBase64 returns an empty result for it (silent — we collapse
    // the FormatException path to nullopt below via emptiness check).
    const QByteArray gz = QByteArray::fromBase64(
        blob.toLatin1(),
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    if (gz.isEmpty()) {
        return std::nullopt;
    }

    z_stream strm = {};
    // Thetis: "new GZipStream(input_stream, CompressionMode.Decompress)"
    // Common.cs:1777 [v2.10.3.13].
    if (inflateInit2(&strm, kGzipWindowBits) != Z_OK) {
        return std::nullopt;
    }

    QByteArray out;
    char chunk[kChunkSize];

    strm.next_in  = reinterpret_cast<Bytef*>(const_cast<char*>(gz.constData()));
    strm.avail_in = static_cast<uInt>(gz.size());

    int ret = Z_OK;
    do {
        strm.next_out  = reinterpret_cast<Bytef*>(chunk);
        strm.avail_out = static_cast<uInt>(kChunkSize);
        ret = inflate(&strm, Z_NO_FLUSH);
        // Z_BUF_ERROR — no progress (typically: input exhausted before
        // the stream-end marker, e.g. a truncated gzip blob).  Without
        // this branch the loop spins forever because the next iteration
        // gets another Z_BUF_ERROR with the same empty input buffer.
        if (ret == Z_NEED_DICT
            || ret == Z_DATA_ERROR
            || ret == Z_MEM_ERROR
            || ret == Z_STREAM_ERROR
            || ret == Z_BUF_ERROR) {
            inflateEnd(&strm);
            return std::nullopt;
        }
        const int produced = kChunkSize - static_cast<int>(strm.avail_out);
        if (maxDecodedBytes < 0 || produced > maxDecodedBytes - out.size()) {
            inflateEnd(&strm);
            return std::nullopt;
        }
        out.append(chunk, produced);
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);

    // Thetis: "string result = Encoding.UTF8.GetString(decompressed_bytes);"
    // Common.cs:1787 [v2.10.3.13].
    return QString::fromUtf8(out);
}

}  // namespace ParaEqEnvelope
}  // namespace NereusSDR
