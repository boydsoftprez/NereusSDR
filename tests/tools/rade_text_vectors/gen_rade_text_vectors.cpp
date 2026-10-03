// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-original test tool. It links freedv-backend's own
// rade_text.cpp / ldpc_encode.cpp / ldpc_decode.cpp from a local clone at
// build time (scripts/gen-rade-text-vectors.sh) and records what they
// produce; no upstream logic is copied into this file.
//
// gen_rade_text_vectors: writes tests/data/rade_text/freedv_backend_vectors.h,
// the interoperability vectors tst_rade_text_codec checks NereusSDR's port
// against. Every number in that header comes from the upstream code:
//
//   encode vectors  rade_text_generate_tx_string(callsign) -> the 180 EOO
//                   floats (rade_n_eoo_bits for RADE V1) a FreeDV station
//                   hands rade_tx_set_eoo_bits.
//   decode vectors  a clean or noisy set of those floats, given to the
//                   upstream rade_text_rx as 90 symbols (rade_n_eoo_bits / 2,
//                   the call RADEReceiveStep makes), and the callsign the
//                   upstream callback returned, or none.
//
// Noise comes from a small fixed generator in this file, not from
// std::normal_distribution, so the recorded floats do not depend on the
// standard library that built the tool. The floats themselves are recorded,
// so the test never regenerates them.
//
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  RADE end-of-over callsigns: initial
//                 tool. AI tooling: Anthropic Claude Code.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "rade_text.h"

namespace {

// RADE V1: rade_n_eoo_bits() = (RADE_NS - 1) * RADE_NC * 2 = 180
// (third_party/rade/src/rade_tx.c:79 [@b289102]).
constexpr int kEooFloats = 180;
constexpr int kEooSymbols = kEooFloats / 2;

// xorshift64* with a Box-Muller transform: deterministic on every platform.
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
    uint64_t next()
    {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 0x2545F4914F6CDD1Dull;
    }
    double uniform()  // (0, 1]
    {
        return (static_cast<double>(next() >> 11) + 1.0) / 9007199254740992.0;
    }
    double gauss()
    {
        const double u1 = uniform();
        const double u2 = uniform();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
    }
};

struct RxResult {
    int calls = 0;
    std::string text;
};

void onRx(rade_text_t, const char* txt, int length, void* state)
{
    auto* r = static_cast<RxResult*>(state);
    r->calls++;
    r->text.assign(txt, static_cast<size_t>(length));
}

std::vector<float> encode(const char* callsign)
{
    std::vector<float> syms(kEooFloats, 0.0f);
    rade_text_t tx = rade_text_create();
    rade_text_enable_stats_output(tx, 0);
    rade_text_generate_tx_string(tx, callsign, static_cast<int>(std::strlen(callsign)),
                                 syms.data(), kEooFloats);
    rade_text_destroy(tx);
    return syms;
}

RxResult decode(std::vector<float> syms)
{
    RxResult result;
    rade_text_t rx = rade_text_create();
    rade_text_enable_stats_output(rx, 0);
    rade_text_set_rx_callback(rx, &onRx, &result);
    rade_text_rx(rx, syms.data(), kEooSymbols);
    rade_text_destroy(rx);
    return result;
}

std::string cString(const std::string& s)
{
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    return out + "\"";
}

void writeFloats(FILE* f, const std::vector<float>& v)
{
    for (size_t i = 0; i < v.size(); ++i) {
        if (i % 6 == 0) {
            std::fprintf(f, "\n        ");
        }
        // %.9g round-trips a float; a C++ float literal needs a '.' or an
        // exponent before its 'f'.
        char num[32];
        std::snprintf(num, sizeof num, "%.9g", static_cast<double>(v[i]));
        const bool needsPoint = std::strpbrk(num, ".eEn") == nullptr;
        std::fprintf(f, "%s%sf, ", num, needsPoint ? ".0" : "");
    }
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <out.h> <upstream-sha>\n", argv[0]);
        return 2;
    }
    FILE* f = std::fopen(argv[1], "w");
    if (!f) {
        std::perror(argv[1]);
        return 1;
    }

    // Callsigns: plain, long, lower case, a slash, the 38..46 punctuation
    // range, characters outside the set (dropped), longer than eight
    // characters (truncated), and empty.
    const char* encodeCases[] = {
        "K6AQ", "KG4VCF", "VK5DGR", "kg4vcf", "W1AW/P", "AB-1.2", "K6#AQ!",
        "KG4VCF/QRP", "A1B2C3D4", "",
    };

    struct Noisy { const char* call; double sigma; uint64_t seed; };
    const Noisy noisyCases[] = {
        {"K6AQ", 0.2, 1}, {"KG4VCF", 0.3, 2}, {"VK5DGR", 0.4, 3},
        {"W1AW/P", 0.5, 4}, {"KG4VCF", 0.6, 5}, {"K6AQ", 0.8, 6},
        {"KG4VCF", 1.2, 7}, {"VK5DGR", 2.0, 8},
    };

    std::fprintf(f,
        "// SPDX-License-Identifier: GPL-3.0-or-later\n"
        "//\n"
        "// no-port-check: generated data, not ported logic.\n"
        "//\n"
        "// GENERATED by scripts/gen-rade-text-vectors.sh from freedv-backend\n"
        "// src/pipeline/rade_text.cpp, ldpc_encode.cpp and ldpc_decode.cpp\n"
        "// [@%s]. Do not edit: regenerate.\n"
        "//\n"
        "// Encode vectors: the 180 EOO floats upstream's\n"
        "// rade_text_generate_tx_string writes for a callsign. Decode vectors:\n"
        "// the 180 floats given to upstream's rade_text_rx as 90 symbols, and\n"
        "// what its callback returned (decoded false: never called).\n"
        "\n#pragma once\n\nnamespace RadeTextVectors {\n\n"
        "constexpr int kEooFloats = %d;\n\n"
        "struct EncodeVector { const char* callsign; float syms[kEooFloats]; };\n"
        "struct DecodeVector { const char* label; float syms[kEooFloats]; bool decoded; const char* text; };\n\n",
        argv[2], kEooFloats);

    std::fprintf(f, "inline const EncodeVector kEncode[] = {\n");
    for (const char* call : encodeCases) {
        std::fprintf(f, "    { %s, {", cString(call).c_str());
        writeFloats(f, encode(call));
        std::fprintf(f, "\n    } },\n");
    }
    std::fprintf(f, "};\n\n");

    std::fprintf(f, "inline const DecodeVector kDecode[] = {\n");
    auto emitDecode = [f](const std::string& label, const std::vector<float>& syms) {
        const RxResult r = decode(syms);
        std::fprintf(f, "    { %s, {", cString(label).c_str());
        writeFloats(f, syms);
        std::fprintf(f, "\n    }, %s, %s },\n", r.calls > 0 ? "true" : "false",
                     cString(r.text).c_str());
    };
    for (const char* call : encodeCases) {
        emitDecode(std::string("clean ") + call, encode(call));
    }
    for (const Noisy& n : noisyCases) {
        std::vector<float> syms = encode(n.call);
        Rng rng(n.seed);
        for (float& s : syms) {
            s = static_cast<float>(s + n.sigma * rng.gauss());
        }
        char label[64];
        std::snprintf(label, sizeof label, "%s sigma %.1f seed %llu", n.call, n.sigma,
                      static_cast<unsigned long long>(n.seed));
        emitDecode(label, syms);
    }
    std::fprintf(f, "};\n\n}  // namespace RadeTextVectors\n");
    std::fclose(f);
    return 0;
}
