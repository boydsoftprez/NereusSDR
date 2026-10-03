// no-port-check: NereusSDR-original.
// =================================================================
// src/core/FftwPlanner.cpp  (NereusSDR)
// =================================================================
// See FftwPlanner.h. NereusSDR-original; no upstream logic.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-R3-39).
// =================================================================
#include "core/FftwPlanner.h"

#ifdef HAVE_FFTW3
// fftw_make_planner_thread_safe and fftwf_make_planner_thread_safe
// (R-R3-39). By full path, from CMake: a bare <fftw3.h> finds WDSP's pinned
// older copy in third_party/wdsp/src first on Linux, where the system
// header's /usr/include is never passed as -I, and that copy declares
// neither.
#include NEREUS_FFTW3_HEADER
#endif

#include <mutex>

namespace NereusSDR {

void makeFftwPlannersThreadSafe()
{
#ifdef HAVE_FFTW3
    static std::once_flag once;
    std::call_once(once, []() {
        // libfftw3_threads (double: WDSP's plans, RxChannel's).
        fftw_make_planner_thread_safe();
        // libfftw3f_threads (single: FFTEngine::replanFft on every spectrum
        // thread, WidebandFftEngine's constructor). A separate library
        // with its own planner; the double call above does not cover it.
        fftwf_make_planner_thread_safe();
    });
#endif
}

} // namespace NereusSDR
