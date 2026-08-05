// SPDX-License-Identifier: MIT
//
// r8brain-free-src is a header-only library since the FFT engine was
// inlined into CDSPRealFFT.h. This translation unit exists so that the
// CMake target can compile something concrete and so that any
// function-local statics (CSyncObject singletons, FFT keepers) are
// anchored to a single object file, avoiding the cost of duplicating
// them across every consumer translation unit.
//
// Including CDSPResampler.h pulls in the full r8brain header set
// transitively (CDSPHBDownsampler, CDSPHBUpsampler, CDSPBlockConvolver,
// CDSPFracInterpolator, CDSPFIRFilter, CDSPRealFFT, r8bbase, r8bconf,
// fft/fft4g.h).
//
// Upstream: https://github.com/avaneev/r8brain-free-src
// Vendored at SHA 5c44bebe9c477d47b1dc7037fcaae2794ff2b4e1 (2026-03-12)

#include "CDSPResampler.h"
