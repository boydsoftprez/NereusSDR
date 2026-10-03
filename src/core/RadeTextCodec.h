// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/RadeTextCodec.h  (NereusSDR)
// =================================================================
//
// NereusSDR - RadeTextCodec: the over-the-air format FreeDV uses for the
// callsign in a RADE end-of-over (EOO) frame.
//
// Ported from freedv-gui's pipeline, now freedv-backend
// src/pipeline/rade_text.h [@f02e7e9]. FreeDV puts the callsign into the
// EOO with rade_text_generate_tx_string + rade_tx_set_eoo_bits (freedv-gui
// src/freedv_interface.cpp:697-711 [@a4ae053]) and reads it back with
// rade_text_rx (freedv-backend src/pipeline/RADEReceiveStep.cpp:231-242
// [@f02e7e9]). The format: a 6-bit character set, at most 8 characters,
// a CRC-8, LDPC(112,56), an interleaver, and QPSK symbols.
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
// --- From freedv-backend src/pipeline/rade_text.h [@f02e7e9] ---
//==========================================================================
// Name:            rade_text.h
//
// Purpose:         Handles reliable text (e.g. text with FEC).
// Created:         December 7, 2024
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
//                 rade_text.h [@f02e7e9]. NereusSDR divergence: the opaque
//                 rade_text_t object and its callback are replaced by two
//                 stateless functions, generateTxString and decodeRx;
//                 upstream's object holds only the callback, the stats
//                 switch and scratch buffers, none of which changes what is
//                 sent or decoded. AI tooling: Anthropic Claude Code.
// =================================================================

#pragma once

#include <string>

namespace NereusSDR::radetext {

// From freedv-backend src/pipeline/rade_text.cpp:49-56 [@f02e7e9]
constexpr int kLdpcTotalSizeBits = 112;   // LDPC_TOTAL_SIZE_BITS
constexpr int kRadeTextMaxLength = 8;     // RADE_TEXT_MAX_LENGTH
constexpr int kRadeTextCrcLength = 1;     // RADE_TEXT_CRC_LENGTH
constexpr int kRadeTextMaxRawLength =
    kRadeTextMaxLength + kRadeTextCrcLength;  // RADE_TEXT_MAX_RAW_LENGTH

/* Generates float array for use with RADE EOO functions. */
// From freedv-backend src/pipeline/rade_text.h:55-56 [@f02e7e9]
//   (rade_text_generate_tx_string). symSize counts floats: pass
//   rade_n_eoo_bits(). Floats past the first kLdpcTotalSizeBits are set to
//   the known filler symbol (1, 0), as upstream does.
void generateTxString(const char* str, int strlength, float* syms, int symSize);

/* Decode received symbols from RADE decoder. */
// From freedv-backend src/pipeline/rade_text.h:61-62 [@f02e7e9]
//   (rade_text_rx). symSize counts symbols (float pairs): pass
//   rade_n_eoo_bits() / 2, as RADEReceiveStep does. Returns true when
//   upstream would call its text callback (the LDPC decode converged and
//   the CRC matched), with the decoded text in *text. The text can be
//   empty: an empty callsign also carries a matching CRC.
bool decodeRx(const float* syms, int symSize, std::string* text);

}  // namespace NereusSDR::radetext
