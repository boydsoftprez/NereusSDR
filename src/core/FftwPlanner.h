// no-port-check: NereusSDR-original.
// =================================================================
// src/core/FftwPlanner.h  (NereusSDR)
// =================================================================
//
// FFTW's planner (every fftw*_plan_* and fftw*_destroy_plan call, and the
// wisdom functions) is not thread-safe unless it is made so, and the
// switch is per precision library: fftw_make_planner_thread_safe() covers
// libfftw3 (double: WDSP's plans) and fftwf_make_planner_thread_safe()
// covers libfftw3f (single: the display FFTs in FFTEngine and
// WidebandFftEngine). NereusSDR plans in both precisions on more than one
// thread, so both are made thread-safe, once per process, before the
// first plan in either.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-R3-39). Moved here from WdspEngine.cpp,
//               which made only the double-precision planner thread-safe.
// =================================================================
#pragma once

namespace NereusSDR {

// Makes FFTW's double- and single-precision planners thread-safe. Idempotent
// and itself thread-safe; call it before any code path that can plan.
void makeFftwPlannersThreadSafe();

} // namespace NereusSDR
