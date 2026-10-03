// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/RadeTextCodec.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR - RadeTextCodec: encode and decode of the callsign FreeDV
// carries in a RADE end-of-over frame.
//
// Ported from freedv-gui's pipeline, now freedv-backend
// src/pipeline/rade_text.cpp [@f02e7e9]. The character set, CRC-8, bit
// packing, LDPC(112,56), interleaver, QPSK mapping, filler symbols,
// noise estimate and symbol normalisation follow upstream line for line,
// so a NereusSDR station and a FreeDV station decode each other's
// callsigns (tst_rade_text_codec checks this against vectors the upstream
// code produced).
//
// License (upstream):
//   - freedv-backend (https://github.com/tmiw/freedv-backend) is the
//     freedv-gui audio pipeline and reporting code split out of
//     freedv-gui (https://github.com/drowe67/freedv-gui), which now builds
//     it (freedv-gui cmake/BuildFreeDVBackend.cmake). Its root LICENSE is
//     BSD-2-Clause, Copyright (c) 2026, Mooneer Salem. Each file carries
//     the BSD-2-Clause-style header reproduced verbatim below per the
//     upstream redistribution clause.
//
// BSD-2-Clause is GPL-compatible by its own terms. NereusSDR ships under
// GPLv3.
//
// --- From freedv-backend src/pipeline/rade_text.cpp [@f02e7e9] ---
//==========================================================================
// Name:            rade_text.cpp
//
// Purpose:         Handles reliable text (e.g. text with FEC).
// Created:         August 15, 2021
// Authors:         Mooneer Salem
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//==========================================================================
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  RADE end-of-over callsigns. Port of
//                 rade_text.cpp [@f02e7e9] into namespace
//                 NereusSDR::radetext. NereusSDR divergences, none of
//                 which changes a symbol sent or a callsign decoded:
//                 (1) the stats path (raw and coded BER against the
//                 file-scope LastEncodedLDPC / LastLDPCAsBits of the last
//                 local transmission, and the unused-EOO bit counts) and
//                 the ulog lines are dropped, as is the RMS computed only
//                 for a log line; (2) the rade_text_t object, its callback
//                 and create/destroy are replaced by generateTxString and
//                 decodeRx (see RadeTextCodec.h); (3) received symbols are
//                 copied into a local buffer instead of the object's
//                 inbound_pending_syms / inbound_pending_amps.
//                 AI tooling: Anthropic Claude Code.
// =================================================================

#include "core/RadeTextCodec.h"

#include <array>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstring>

#include "core/RadeLdpc.h"

namespace NereusSDR::radetext {

namespace {

using radeldpc::LdpcComp;

// From freedv-backend src/pipeline/rade_text.cpp:55-56 [@f02e7e9]
/* Two bytes of text/CRC equal four bytes of LDPC(112,56). */
constexpr int RADE_TEXT_BYTES_PER_ENCODED_SEGMENT = 8;

// From freedv-backend src/pipeline/rade_text.cpp:111-152 [@f02e7e9]
// 6 bit character set for text field use:
// 0: ASCII null
// 1-9: ASCII 38-46
// 10-19: ASCII '0'-'9'
// 20-45: ASCII 'A'-'Z'
// 46: ASCII '/'
// 47: ASCII ' '
void convert_callsign_to_ota_string_(const char *input, char *output, int maxLength)
{
    assert(input != NULL);
    assert(output != NULL);
    assert(maxLength >= 0);

    int outidx = 0;
    for (size_t index = 0; index < (size_t)maxLength; index++)
    {
        if (input[index] == 0)
            break;

        if (input[index] >= 38 && input[index] <= 46)
        {
            output[outidx++] = input[index] - 37;
        }
        else if (input[index] == '/')
        {
            output[outidx++] = 46;
        }
        else if (input[index] >= '0' && input[index] <= '9')
        {
            output[outidx++] = input[index] - '0' + 10;
        }
        else if (input[index] >= 'A' && input[index] <= 'Z')
        {
            output[outidx++] = input[index] - 'A' + 20;
        }
        else if (input[index] >= 'a' && input[index] <= 'z')
        {
            output[outidx++] = toupper(input[index]) - 'A' + 20;
        }
    }
    output[outidx] = 0;
}

// From freedv-backend src/pipeline/rade_text.cpp:154-184 [@f02e7e9]
void convert_ota_string_to_callsign_(const char *input, char *output, int maxLength)
{
    assert(input != NULL);
    assert(output != NULL);
    assert(maxLength >= 0);

    int outidx = 0;
    for (size_t index = 0; index < (size_t)maxLength; index++)
    {
        if (input[index] == 0)
            break;

        if (input[index] >= 1 && input[index] <= 9)
        {
            output[outidx++] = input[index] + 37;
        }
        else if (input[index] >= 10 && input[index] <= 19)
        {
            output[outidx++] = input[index] - 10 + '0';
        }
        else if (input[index] >= 20 && input[index] <= 45)
        {
            output[outidx++] = input[index] - 20 + 'A';
        }
        else if (input[index] == 46)
        {
            output[outidx++] = '/';
        }
    }
    output[outidx] = 0;
}

// From freedv-backend src/pipeline/rade_text.cpp:186-219 [@f02e7e9]
char calculateCRC8_(char *input, int length)
{
    assert(input != NULL);
    assert(length >= 0);

    unsigned char generator = 0x1D;
    unsigned char crc = 0x00; /* start with 0 so first byte can be 'xored' in */

    while (length > 0)
    {
        unsigned char ch = *input++;
        length--;

        // Break out if we see a null.
        if (ch == 0)
            break;

        crc ^= ch; /* XOR-in the next input byte */

        for (int i = 0; i < 8; i++)
        {
            if ((crc & 0x80) != 0)
            {
                crc = (unsigned char)((crc << 1) ^ generator);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

// From freedv-backend src/pipeline/rade_text.cpp:221-240 [@f02e7e9]
constexpr int INTERLEAVER_B = 37;
void deinterleave_comp(LdpcComp* out, const LdpcComp* in, int syms)
{
    for (int index = 0; index < syms; index++)
    {
        int newIndex = (INTERLEAVER_B * index) % syms;
        out[index].real = in[newIndex].real;
        out[index].imag = in[newIndex].imag;
    }
}

void interleave_bits(char* out, char* in, int syms)
{
    for (int index = 0; index < syms; index++)
    {
        int newIndex = (INTERLEAVER_B * index) % syms;
        out[2 * newIndex] = in[2 * index];
        out[2 * newIndex + 1] = in[2 * index + 1];
    }
}

// From freedv-backend src/pipeline/rade_text.cpp:242-323 [@f02e7e9]
//   (rade_text_ldpc_decode, less its stats and log lines)
int rade_text_ldpc_decode(const LdpcComp* pendingSyms, const float* pendingAmps,
                          char *dest, float noiseVar)
{
    assert(dest != NULL);

    float sigma2 = noiseVar;
    if (sigma2 < 1e-6f) sigma2 = 1e-6f;

    auto decodeResult = radeldpc::ldpc_decode(pendingSyms, pendingAmps, sigma2);

    if (decodeResult.converged)
    {
        memset(dest, 0, RADE_TEXT_BYTES_PER_ENCODED_SEGMENT);

        for (int bitIndex = 0; bitIndex < 8; bitIndex++)
        {
            if (decodeResult.message[bitIndex])
                dest[0] |= 1 << bitIndex;
        }
        for (int bitIndex = 8; bitIndex < (kLdpcTotalSizeBits / 2); bitIndex++)
        {
            int bitsSinceCrc = bitIndex - 8;
            if (decodeResult.message[bitIndex])
                dest[1 + (bitsSinceCrc / 6)] |= (1 << (bitsSinceCrc % 6));
        }
    }

    return decodeResult.converged;
}

}  // namespace

// From freedv-backend src/pipeline/rade_text.cpp:325-410 [@f02e7e9]
//   (rade_text_rx)
bool decodeRx(const float* syms, int symSize, std::string* text)
{
    assert(syms != NULL);
    // NereusSDR guard: upstream reads the first 56 symbols unconditionally.
    if (symSize < kLdpcTotalSizeBits / 2) {
        return false;
    }

    LdpcComp inbound_pending_syms[kLdpcTotalSizeBits / 2];
    float inbound_pending_amps[kLdpcTotalSizeBits / 2];

    // Deinterleave received bits.
    deinterleave_comp(inbound_pending_syms, reinterpret_cast<const LdpcComp*>(syms),
                      kLdpcTotalSizeBits / 2);

    // Calculate RMS of all symbols
    // NereusSDR: upstream sums the RMS of the first 56 symbols here only for
    // a log line; that sum is dropped.
    float ss = 0;
    int ssCnt = 0;
    for (int index = kLdpcTotalSizeBits / 2; index < symSize; index++)
    {
        // This is the unused part of the EOO that was filled with a known sequence.
        const float* sym = &syms[2 * index];
        float sym_amp = std::sqrt(sym[0] * sym[0] + sym[1] * sym[1]);
        if (sym_amp > 0)
        {
            ss += std::pow(1 - sym[0] / sym_amp, 2);
            ss += std::pow(0 - sym[1] / sym_amp, 2);
            ssCnt += 2;
        }
    }

    // Copy over symbols prior to decode.
    for (int index = 0; index < kLdpcTotalSizeBits / 2; index++)
    {
        LdpcComp *sym = &inbound_pending_syms[index];
        float sym_amp = sqrtf(sym->real * sym->real + sym->imag * sym->imag);
        sym->real /= sym_amp;
        sym->imag /= sym_amp;
        inbound_pending_amps[index] = sym_amp;
    }

    // We have all the bits we need, so we're ready to decode.
    char decodedStr[kRadeTextMaxRawLength + 1];
    char rawStr[kRadeTextMaxRawLength + 1];
    memset(rawStr, 0, kRadeTextMaxRawLength + 1);
    memset(decodedStr, 0, kRadeTextMaxRawLength + 1);

    float noiseVar = ss / (ssCnt - 1);
    if (rade_text_ldpc_decode(inbound_pending_syms, inbound_pending_amps, rawStr, noiseVar) != 0)
    {
        // BER is under limits.
        convert_ota_string_to_callsign_(&rawStr[kRadeTextCrcLength], &decodedStr[kRadeTextCrcLength],
                                        kRadeTextMaxLength);
        decodedStr[0] = rawStr[0]; // CRC

        // Get expected and actual CRC.
        unsigned char receivedCRC = decodedStr[0];
        unsigned char calcCRC = calculateCRC8_(&rawStr[kRadeTextCrcLength], kRadeTextMaxLength);

        if (receivedCRC == calcCRC)
        {
            // We got a valid string. Call assigned callback.
            if (text != nullptr) {
                text->assign(&decodedStr[kRadeTextCrcLength]);
            }
            return true;
        }
    }
    return false;
}

// From freedv-backend src/pipeline/rade_text.cpp:427-534 [@f02e7e9]
//   (rade_text_generate_tx_string, less its stats copy and log line)
void generateTxString(const char *str, int strlength, float *syms, int symSize)
{
    assert(str != NULL);
    assert(syms != NULL);

    char tmp[kRadeTextMaxRawLength + 1];
    memset(tmp, 0, kRadeTextMaxRawLength + 1);

    convert_callsign_to_ota_string_(str, &tmp[kRadeTextCrcLength],
                                    strlength < kRadeTextMaxLength ? strlength : kRadeTextMaxLength);

    int txt_length = strlen(&tmp[kRadeTextCrcLength]);
    if (txt_length >= kRadeTextMaxLength)
    {
        txt_length = kRadeTextMaxLength;
    }
    unsigned char crc = calculateCRC8_(&tmp[kRadeTextCrcLength], txt_length);
    tmp[0] = crc;

    // Encode block of text using LDPC(112,56).
    std::array<uint8_t, kLdpcTotalSizeBits / 2> ibits{};  // zero-initialize; bits not explicitly set below must be 0
    unsigned char pbits[kLdpcTotalSizeBits / 2];
    memset(pbits, 0, kLdpcTotalSizeBits / 2);
    for (int index = 0; index < 8; index++)
    {
        ibits[index] = 0;
        if (tmp[0] & (1 << index))
            ibits[index] = 1;
    }

    // Pack 6 bit characters into single LDPC block.
    for (int ibitsBitIndex = 8; ibitsBitIndex < (kLdpcTotalSizeBits / 2); ibitsBitIndex++)
    {
        int bitsFromCrc = ibitsBitIndex - 8;
        unsigned int byte = tmp[kRadeTextCrcLength + bitsFromCrc / 6];
        unsigned int bitToCheck = bitsFromCrc % 6;
        // fprintf(stderr, "bit index: %d, byte: %x, bit to check: %d, result:
        // %d\n", ibitsBitIndex, byte, bitToCheck, (byte & (1 << bitToCheck)) != 0);

        if (byte & (1 << bitToCheck))
        {
            ibits[ibitsBitIndex] = 1;
        }
    }

    auto totalBits = radeldpc::ldpc_encode(ibits);
    memcpy(pbits, &totalBits[kLdpcTotalSizeBits / 2], kLdpcTotalSizeBits / 2);

    // Split LDPC encoded bits into individual bits, with the first
    // RADE_TEXT_UW_LENGTH_BITS being UW.
    char tmpbits[kLdpcTotalSizeBits];
    char tx_text[kLdpcTotalSizeBits];

    memset(tx_text, 0, kLdpcTotalSizeBits);
    memcpy(&tmpbits[0], &ibits[0], kLdpcTotalSizeBits / 2);
    memcpy(&tmpbits[kLdpcTotalSizeBits / 2], &pbits[0], kLdpcTotalSizeBits / 2);

    // Interleave the bits together to enhance fading performance.
    interleave_bits(&tx_text[0], tmpbits, kLdpcTotalSizeBits / 2);

    // Generate floats based on the bits.
    for (int index = 0; index < kLdpcTotalSizeBits / 2; index++)
    {
        char *ptr = &tx_text[2 * index];
        if (*ptr == 0 && *(ptr + 1) == 0)
        {
            syms[2 * index] = 1;
            syms[2 * index + 1] = 0;
        }
        else if (*ptr == 0 && *(ptr + 1) == 1)
        {
            syms[2 * index] = 0;
            syms[2 * index + 1] = 1;
        }
        else if (*ptr == 1 && *(ptr + 1) == 0)
        {
            syms[2 * index] = 0;
            syms[2 * index + 1] = -1;
        }
        else if (*ptr == 1 && *(ptr + 1) == 1)
        {
            syms[2 * index] = -1;
            syms[2 * index + 1] = 0;
        }
    }

    if (symSize > kLdpcTotalSizeBits)
    {
        for (int index = kLdpcTotalSizeBits; index < symSize; index++)
        {
            syms[index] = index % 2 ? 0 : 1;
        }
    }
}

}  // namespace NereusSDR::radetext
