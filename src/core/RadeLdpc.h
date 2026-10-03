// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/RadeLdpc.h  (NereusSDR)
// =================================================================
//
// NereusSDR - RadeLdpc: the LDPC(112,56) code FreeDV puts on the RADE
// end-of-over callsign.
//
// Ported from freedv-gui's pipeline, now freedv-backend
// src/pipeline/ldpc_encode.h and src/pipeline/ldpc_decode.h
// [@f02e7e9]. The declarations, their comments and the result struct
// follow upstream line for line.
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
// --- From freedv-backend src/pipeline/ldpc_encode.h [@f02e7e9] ---
//==========================================================================
// Name:            ldpc_encode.h
//
// Purpose:         Handles encode of LDPC(112, 56) codewords.
// Created:         May 20, 2026
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
// --- From freedv-backend src/pipeline/ldpc_decode.h [@f02e7e9] ---
//==========================================================================
// Name:            ldpc_decode.h
//
// Purpose:         Handles decode of LDPC(112, 56) codewords.
// Created:         May 20, 2026
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
//                 ldpc_encode.h + ldpc_decode.h [@f02e7e9] into namespace
//                 NereusSDR::radeldpc. NereusSDR divergence: the symbol
//                 type is LdpcComp, laid out as librade's RADE_COMP
//                 ({float real; float imag;}), so this header does not
//                 pull in rade_api.h and librade's internals.
//                 AI tooling: Anthropic Claude Code.
// =================================================================

#pragma once

#include <array>
#include <cstdint>

namespace NereusSDR::radeldpc {

// Laid out as librade's RADE_COMP (third_party/rade/src/rade_api.h:69-74
// [@b289102]); upstream takes RADE_COMP* here.
struct LdpcComp {
    float real;
    float imag;
};

// From freedv-backend src/pipeline/ldpc_encode.h:41-52 [@f02e7e9]
// LDPC(112,56) systematic encoder using the HRA_56_56 parity check matrix.
//
// H = [H_a | H_b] (56x112), where H_b is lower-bidiagonal, allowing parity
// bits p to be solved from H_a*s + H_b*p = 0 (mod 2) via forward substitution:
//   p[0]   = r[0]
//   p[i]   = r[i] XOR p[i-1]   for i = 1..55
// where r = H_a * s (mod 2).
//
// Input:  56 bits (each element must be 0 or 1)
// Output: 112-bit codeword [ s | p ]

std::array<uint8_t, 112> ldpc_encode(const std::array<uint8_t, 56>& s);

// From freedv-backend src/pipeline/ldpc_decode.h:43-97 [@f02e7e9]
struct LDPCDecodeResult {
    std::array<uint8_t, 112> message{}; // decoded message bits (0 or 1)
    bool  converged;                    // true if all parity checks are satisfied
    int   iterations;                   // number of BP iterations performed
};

// Soft-decision LDPC(112,56) decoder using sum-product belief propagation.
//
// QPSK bit mapping (sequential Gray-coded):
//   bit 2k   -> sym[k].real  (I component)
//   bit 2k+1 -> sym[k].imag  (Q component)
//
// LLR model (AWGN with known fading):
//   LLR = 2 * amplitude * received_component / noise_var
//   positive LLR => bit is more likely 0
//   negative LLR => bit is more likely 1
//
// Parameters:
//   syms       - 56 received QPSK symbols
//   amplitudes - per-symbol channel fading amplitude (use 1.0 for flat/unfaded channel)
//   noise_var  - noise variance per I/Q component (sigma^2 of the AWGN)
//   max_iter   - maximum belief-propagation iterations (default 30)
LDPCDecodeResult ldpc_decode(const LdpcComp* syms,
                              const float*    amplitudes,
                              float           noise_var,
                              int             max_iter = 30);

// Compute 112 channel LLRs from 56 received QPSK symbols using the
// Simplified-MAX-Log-MAP (a.k.a. Max-Log-MAP) algorithm.
//
// The algorithm approximates the exact MAP log-likelihood ratio by replacing
// the log-sum-exp over all constellation points sharing a bit value with a
// plain max (equivalently, minimum squared Euclidean distance):
//
//   LLR_b ≈ min_{s: b=1} ||r - a·s||² / (2σ²)
//          - min_{s: b=0} ||r - a·s||² / (2σ²)
//
// QPSK constellation and bit mapping (bit 2k = I, bit 2k+1 = Q):
//   s = ( a,  0)  ->  bits (0,0)
//   s = ( 0,  a)  ->  bits (0,1)
//   s = ( 0, -a)  ->  bits (1,0)
//   s = (-a,  0)  ->  bits (1,1)
//
// Sign convention: positive LLR => bit more likely 0,
//                 negative LLR => bit more likely 1.
//
// Parameters:
//   syms       - 56 received QPSK symbols
//   amplitudes - per-symbol channel fading amplitude (use 1.0 for flat channel)
//   noise_var  - noise variance per I/Q component (sigma^2 of the AWGN)
//   llr_out    - caller-allocated output buffer of 112 floats
void ldpc_linear_log_map(const LdpcComp* syms,
                         const float*    amplitudes,
                         float           noise_var,
                         float*          llr_out);

}  // namespace NereusSDR::radeldpc
