// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/RadeLdpc.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR - RadeLdpc: LDPC(112,56) encoder and sum-product decoder
// for the RADE end-of-over callsign.
//
// Ported from freedv-gui's pipeline, now freedv-backend
// src/pipeline/ldpc_encode.cpp and src/pipeline/ldpc_decode.cpp
// [@f02e7e9]. The arithmetic follows upstream line for line, so a
// NereusSDR station encodes and decodes the same codewords as FreeDV.
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
// --- From freedv-backend src/pipeline/ldpc_encode.cpp [@f02e7e9] ---
//==========================================================================
// Name:            ldpc_encode.cpp
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
// --- From freedv-backend src/pipeline/ldpc_decode.cpp [@f02e7e9] ---
//==========================================================================
// Name:            ldpc_decode.cpp
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
//                 ldpc_encode.cpp + ldpc_decode.cpp [@f02e7e9] into
//                 namespace NereusSDR::radeldpc. NereusSDR divergences,
//                 none of which changes a result: (1) the message arrays
//                 m_vc / m_cv, file-scope statics upstream, are locals
//                 of ldpc_decode so two decoders on two threads cannot
//                 share them; (2) calc_likelihood and max_star get
//                 internal linkage; (3) the symbol type is LdpcComp
//                 (see RadeLdpc.h). AI tooling: Anthropic Claude Code.
// =================================================================

#include "core/RadeLdpc.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include "core/RadeHra5656.h"

namespace NereusSDR::radeldpc {

// From freedv-backend src/pipeline/ldpc_encode.cpp:38-71 [@f02e7e9]
std::array<uint8_t, 112> ldpc_encode(const std::array<uint8_t, 56>& s)
{
    std::array<uint8_t, 112> codeword{};

    // Codeword must satisfy Hc^t = 0. The first half of c is known
    // to be s, so we can prepopulate now.
    for (int i = 0; i < 56; i++)
    {
        codeword[i] = s[i];
    }

    // Assumption: right half of H is (mostly) diagonal. For each parity bit,
    // we use p[index - 1] and p[index] (except for the first parity bit,
    // which is just p[index]).
    for (int i = 0; i < 56; i++)
    {
        int parityCtr = 0;
        for (int j = 0; j < 56; j++)
        {
            parityCtr += codeword[j] * HRA_56_56[i][j];
        }
        if (i == 0)
        {
            codeword[56] = (parityCtr % 2) ? 1 : 0;
        }
        else
        {
            parityCtr += codeword[56 + i - 1];
            codeword[56 + i] = (parityCtr % 2) ? 1 : 0;
        }
    }

    return codeword;
}

namespace {

// From freedv-backend src/pipeline/ldpc_decode.cpp:43-66 [@f02e7e9]
// ---- Tanner graph built from H = [H_a | H_b] ----
struct LDPCGraph {
    struct Edge { int check, var; };

    std::vector<Edge> edges;
    std::vector<int>  check_edges[56];   // edge indices per check node
    std::vector<int>  var_edges[112];    // edge indices per variable node

    LDPCGraph() {
        auto add = [&](int c, int v) {
            int e = (int)edges.size();
            edges.push_back({c, v});
            check_edges[c].push_back(e);
            var_edges[v].push_back(e);
        };

        // H_a edges
        for (int i = 0; i < 56; i++)
            for (int j = 0; j < 112; j++)
                if (HRA_56_56[i][j]) add(i, j);
    }
};

const LDPCGraph graph;

// From freedv-backend src/pipeline/ldpc_decode.cpp:68-79 [@f02e7e9]
// ---- Belief-propagation decoder ----
constexpr float LLR_MAX = 10000.0f;

float phi(float x)
{
    if (x < 1e-10f) return LLR_MAX;

    auto expx = std::exp((double)x);
    if (expx < 1e-10f) return LLR_MAX;

    return std::log((expx + 1.0f) / (expx - 1.0f));
}

// From freedv-backend src/pipeline/ldpc_decode.cpp:179-204 [@f02e7e9]
float calc_likelihood(const LdpcComp* sym, float var, float real, float imag, float amp, float avg_amp)
{
    float errR = sym->real - real;
    float errI = sym->imag - imag;
    float a = amp / avg_amp;
    return -var * a * a * (errR * errR + errI * errI);
}

// Linear constant log-map. See https://sciencedirect.com/science/article/pii/S001600321200035X
float max_star(float x, float y)
{
    float maxXY = std::max(x, y);
    float absdiff = std::abs(x - y);
    float logXY = 0;

    if (absdiff <= 2.45)
    {
        logXY = -0.24 * absdiff + 0.596;
    }
    else if (absdiff > 2.45 && absdiff <= 3.5)
    {
        logXY = 0.048;
    }

    return maxXY + logXY;
}

}  // namespace

// From freedv-backend src/pipeline/ldpc_decode.cpp:81-177 [@f02e7e9]
LDPCDecodeResult ldpc_decode(const LdpcComp* syms,
                              const float*    amplitudes,
                              float           noise_var,
                              int             max_iter)
{
    // NereusSDR: upstream's file-scope `static const int E` and
    // `static std::vector<float> m_vc(E), m_cv(E, 0.0f);` (ldpc_decode.cpp
    // :81-82) are locals here; every entry is written below before it is
    // read, as upstream does.
    const int E = (int)graph.edges.size();
    std::vector<float> m_vc(E), m_cv(E, 0.0f);

    if (noise_var < 1e-10f) noise_var = 1e-10f;

    // Compute channel LLRs.
    float llr_ch[112];
    ldpc_linear_log_map(syms, amplitudes, noise_var, llr_ch);

    // m_vc[e]: variable-to-check message on edge e
    // m_cv[e]: check-to-variable message on edge e
    for (int e = 0; e < E; e++)
    {
        m_vc[e] = llr_ch[graph.edges[e].var];
        m_cv[e] = 0.0f;
    }

    LDPCDecodeResult result{};
    result.converged  = false;
    result.iterations = 0;

    for (int iter = 0; iter < max_iter; iter++) {

        // ---- Check-to-variable update (tanh / sum-product rule) ----
        //
        // For check i and its neighbor set N(i), the outgoing message to
        // variable j is:
        //
        //   r_{i→j} = ∏_{j'≠j}(sign(q_j'i)) * phi(sum_{i'!=i}(phi(abs(q_ji')))
        //
        for (int i = 0; i < 56; i++) {
            const auto& ce = graph.check_edges[i];
            const int   nd = (int)ce.size();

            int sign = 1;
            float sum = 0;
            for (int k = 0; k < nd; k++) {
                const float v = m_vc[ce[k]];
                sign *= (v >= 0.0f) ? 1 : -1;
                sum += std::max(0.0f, phi(std::abs(v)));
            }

            for (int k = 0; k < nd; k++) {
                const float v = m_vc[ce[k]];
                int inv_sign = (v >= 0.0f) ? 1 : -1;
                m_cv[ce[k]] = std::clamp(inv_sign * sign * std::max(0.0f, phi(sum - phi(std::abs(v)))), -LLR_MAX, LLR_MAX);
            }
        }

        // ---- Variable-to-check update ----
        //
        //   q_{j→i} = λ_j + Σ_{i'≠i} r_{i'→j}
        //
        // Computed as (sum of all incoming check messages + channel LLR) minus
        // the one edge being excluded.

        for (int j = 0; j < 112; j++) {
            const auto& ve = graph.var_edges[j];
            float total = llr_ch[j];
            for (int e : ve) total += m_cv[e];
            for (int e : ve)
                m_vc[e] = std::clamp(total - m_cv[e], -LLR_MAX, LLR_MAX);
        }

        // ---- Posterior LLR, hard decision, syndrome check ----

        for (int j = 0; j < 112; j++) {
            float L = llr_ch[j];
            for (int e : graph.var_edges[j]) L += m_cv[e];
            result.message[j] = (L < 0.0f) ? 1 : 0;
        }

        // bits * H' must equal 0. Check here.
        bool ok = true;
        for (int i = 0; i < 56 && ok; i++) {
            int ctr = 0;
            for (int j = 0; j < 112; j++) {
                ctr += HRA_56_56[i][j] * result.message[j];
            }
            ok = (ctr % 2) == 0; // non-zero check
        }

        result.iterations = iter + 1;
        if (ok) {
            result.converged = true;
            break;
        }
    }

    // Return result, even if not converged.
    return result;
}

// From freedv-backend src/pipeline/ldpc_decode.cpp:206-277 [@f02e7e9]
// ---- Simplified-MAX-Log-MAP channel LLR computation ----
//
// For each QPSK symbol the exact MAP log-likelihood ratio:
//
//   LLR_b = log( Σ_{s: b=0} exp(-||r - a·s||² / (2σ²)) )
//         - log( Σ_{s: b=1} exp(-||r - a·s||² / (2σ²)) )
//
// is approximated by replacing log-sum-exp with max (i.e., min squared distance):
//
//   LLR_b ≈ min_{s: b=1} ||r - a·s||² / (2σ²)
//          - min_{s: b=0} ||r - a·s||² / (2σ²)
//
// Constellation (amplitude a):
//   s00 = ( a,  0)  bits (0,0)   s01 = ( 0,  a)  bits (0,1)
//   s10 = ( 0, -a)  bits (1,0)   s11 = (-a,  0)  bits (1,1)

void ldpc_linear_log_map(const LdpcComp* syms,
                         const float*    amplitudes,
                         float           noise_var,
                         float*          llr_out)
{
    constexpr int NUM_BITS_PER_SYMBOL = 2;
    constexpr int NUM_SYMBOLS = 56;

    float mean_amp = 0;
    for (int k = 0; k < NUM_SYMBOLS; k++)
    {
        mean_amp += amplitudes[k];
    }
    mean_amp /= NUM_SYMBOLS;
    float EsNo = 1.0f / (2.0f * noise_var);

    for (int k = 0; k < NUM_SYMBOLS; k++) {
        const float a  = amplitudes[k];

        float num[NUM_BITS_PER_SYMBOL];
        float den[NUM_BITS_PER_SYMBOL];
        float sym_likelihoods[] = {
            calc_likelihood(&syms[k], EsNo, 1, 0, a, mean_amp),  // 00
            calc_likelihood(&syms[k], EsNo, 0, 1, a, mean_amp),  // 01
            calc_likelihood(&syms[k], EsNo, 0, -1, a, mean_amp), // 10
            calc_likelihood(&syms[k], EsNo, -1, 0, a, mean_amp)  // 11
        };

        for (int index = 0; index < NUM_BITS_PER_SYMBOL; index++)
        {
            num[index] = den[index] = -LLR_MAX;
        }

        // 00
        den[0] = max_star(den[0], sym_likelihoods[0]);
        den[1] = max_star(den[1], sym_likelihoods[0]);

        // 01
        den[0] = max_star(den[0], sym_likelihoods[1]);
        num[1] = max_star(num[1], sym_likelihoods[1]);

        // 10
        num[0] = max_star(num[0], sym_likelihoods[2]);
        den[1] = max_star(den[1], sym_likelihoods[2]);

        // 11
        num[0] = max_star(num[0], sym_likelihoods[3]);
        num[1] = max_star(num[1], sym_likelihoods[3]);

        const float llr_bit0 = num[0] - den[0];
        const float llr_bit1 = num[1] - den[1];

        llr_out[2*k]     = -std::clamp(llr_bit0, -LLR_MAX, LLR_MAX);
        llr_out[2*k + 1] = -std::clamp(llr_bit1, -LLR_MAX, LLR_MAX);
    }
}

}  // namespace NereusSDR::radeldpc
