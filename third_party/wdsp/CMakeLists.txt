# WDSP DSP Library — static build for NereusSDR
#
# Based on TAPR/OpenHPSDR-wdsp 2.10 by Warren Pratt, NR0V
# Pinned source and retained extensions: docs/attribution/WDSP-PROVENANCE.md
# Cross-platform support via linux_port.h/c from g0orx/wdsp (John Melton, G0ORX)
#
# WDSP is licensed under GPL-2.0-or-later. NereusSDR distributes the
# combined work under GPLv3 via GPLv3 §5(b) upgrade. See
# docs/attribution/WDSP-PROVENANCE.md.

cmake_minimum_required(VERSION 3.20)
project(wdsp C)

# Reviewed WDSP 2.10 source inventory. Keep this explicit so dormant debug
# sources (notably upstream snoop.c) and future vendor additions do not enter
# the library without review.
set(WDSP_SOURCES
    src/RXA.c
    src/TXA.c
    src/amd.c
    src/ammod.c
    src/amsq.c
    src/analyzer.c
    src/anf.c
    src/anr.c
    src/apfshadow.c
    src/bandpass.c
    src/calcc.c
    src/calculus.c
    src/cblock.c
    src/cfcomp.c
    src/cfir.c
    src/channel.c
    src/cmath.c
    src/compress.c
    src/delay.c
    src/dexp.c
    src/div.c
    src/doublepole.c
    src/dsplock.c
    src/eer.c
    src/emnr.c
    src/emph.c
    src/eq.c
    src/extrapolate.c
    src/fcurve.c
    src/fir.c
    src/firmin.c
    src/fmd.c
    src/fmmod.c
    src/fmsq.c
    src/gain.c
    src/gaussian.c
    src/gen.c
    src/icfir.c
    src/iir.c
    src/impulse_cache.c
    src/iobuffs.c
    src/iqc.c
    src/linux_port.c
    src/lmath.c
    src/main.c
    src/matchedCW.c
    src/meter.c
    src/meterlog10.c
    src/nbp.c
    src/netinterface_stub.c
    src/nnet.c
    src/nnio.c
    src/nnr.c
    src/nnr_model_0.c
    src/nnr_model_1.c
    src/nob.c
    src/nobII.c
    src/nurbs.c
    src/nurbs_fit.c
    src/nurbs_spline.c
    src/osctrl.c
    src/patchpanel.c
    src/phrot.c
    src/ps_sync_stub.c
    src/resample.c
    src/reshb.c
    src/rmatch.c
    src/rnnr.c
    src/sbnr.c
    src/sender.c
    src/shift.c
    src/siphon.c
    src/slew.c
    src/snb.c
    src/ssql.c
    src/syncbuffs.c
    src/txgain_stub.c
    src/utilities.c
    src/varsamp.c
    src/version.c
    src/wbfm.c
    src/wcpAGC.c
    src/wisdom.c
    src/zetaHat.c
)

# linux_port.c is only compiled on non-Windows
if(WIN32)
    list(FILTER WDSP_SOURCES EXCLUDE REGEX "linux_port\\.c$")
endif()

add_library(wdsp_static STATIC ${WDSP_SOURCES})

target_include_directories(wdsp_static
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Retained NR3 and NR4 backends:
#   rnnr.c (NR3) needs rnnoise.h + rnnoise symbols → rnnoise_static
#   sbnr.c (NR4) needs specbleach_adenoiser.h + specbleach symbols → specbleach_static
# PUBLIC linkage transitively exposes the targets' include dirs so rnnr.c
# and sbnr.c find their headers without an explicit include_directories call.
# if(TARGET ...) guards keep this CMakeLists.txt usable in standalone builds.
if(TARGET rnnoise_static)
    target_link_libraries(wdsp_static PUBLIC rnnoise_static)
endif()
if(TARGET specbleach_static)
    target_link_libraries(wdsp_static PUBLIC specbleach_static)

    # API compatibility shim: libspecbleach commit 41d3f58 renamed
    # SpectralBleachParameters → SpectralBleachAdaptiveParameters.
    # sbnr.c (ported byte-for-byte from Thetis v2.10.3.13 @ 501e3f51)
    # uses the old name. We generate a thin compat header rather than
    # editing the byte-for-byte port.
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/specbleach_compat")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/specbleach_compat/specbleach_compat.h"
"/* NereusSDR specbleach API compatibility shim.
 * libspecbleach commit 41d3f58 renamed SpectralBleachParameters to
 * SpectralBleachAdaptiveParameters. sbnr.c was ported byte-for-byte from
 * Thetis v2.10.3.13 @ 501e3f51 which used the old name.
 * Generated at configure time — do not edit by hand.
 */
#pragma once
#include <specbleach_adenoiser.h>
/* Thetis-compatible alias for the renamed struct. */
typedef SpectralBleachAdaptiveParameters SpectralBleachParameters;
")
    target_include_directories(wdsp_static PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/specbleach_compat")
    # Force sbnr.c to see the compat header before specbleach_adenoiser.h
    # by setting a PRIVATE compile definition that triggers inclusion via
    # the -include flag on GCC/Clang/AppleClang.
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU|AppleClang")
        target_compile_options(wdsp_static PRIVATE
            "-include${CMAKE_CURRENT_BINARY_DIR}/specbleach_compat/specbleach_compat.h")
    elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(wdsp_static PRIVATE
            "/FI${CMAKE_CURRENT_BINARY_DIR}/specbleach_compat/specbleach_compat.h")
    endif()
endif()

# FFTW3 — header needed for compilation, library for final link
if(FFTW3_INCLUDE_DIRS)
    target_include_directories(wdsp_static PRIVATE ${FFTW3_INCLUDE_DIRS})
endif()
if(FFTW3_LIBRARIES)
    target_link_libraries(wdsp_static PUBLIC ${FFTW3_LIBRARIES})
endif()

# Static build: PORT macro is empty (no dllexport)
target_compile_definitions(wdsp_static PRIVATE
    WDSP_STATIC_BUILD
)

# Platform-specific configuration
if(WIN32)
    target_link_libraries(wdsp_static PRIVATE avrt)
    target_compile_definitions(wdsp_static PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _USE_MATH_DEFINES
    )
    # The top-level CMakeLists sets NOMINMAX globally so windows.h does not
    # define min/max macros that collide with std::min / std::max in C++ TUs.
    # WDSP's vendored C source (RXA.c, cfcomp.c, emnr.c, snb.c, ssql.c, ...)
    # uses those macros directly and has no std::min/std::max collision risk.
    # Undef NOMINMAX for the wdsp_static target only so windows.h restores them.
    target_compile_options(wdsp_static PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/UNOMINMAX>
        $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-UNOMINMAX>
    )
else()
    target_link_libraries(wdsp_static PRIVATE pthread m)
    target_compile_definitions(wdsp_static PRIVATE _GNU_SOURCE)
    if(APPLE)
        # macOS: dispatch framework not needed — linux_port uses named sem_open
    endif()
endif()

# Warnings are suppressed only in sources that are byte-identical to the
# pinned WDSP 2.10 tree. Sources NereusSDR edits or adds (the "Modified" and
# "NereusSDR-original" rows of docs/attribution/WDSP-PROVENANCE.md) build with
# the compiler's default warnings, so a problem in our own changes shows up.
# On Clang the edited upstream files still use WDSP's `if (x = f())` idiom,
# so -Wparentheses alone is turned off for them. Keep this list in step with
# the provenance table when a file is edited or synced back to the pin;
# scripts/audit-wdsp-headers.py (run in CI) fails when a source here matches
# the pinned hash in source-manifest.csv, or one left out of it does not.
set(WDSP_EDITED_SOURCES
    src/RXA.c
    src/TXA.c
    src/amd.c
    src/anf.c
    src/anr.c
    src/calcc.c
    src/cfcomp.c
    src/cfir.c
    src/channel.c
    src/dsplock.c
    src/emnr.c
    src/emph.c
    src/eq.c
    src/extrapolate.c
    src/fmd.c
    src/iobuffs.c
    src/iqc.c
    src/linux_port.c
    src/main.c
    src/netinterface_stub.c
    src/nnet.c
    src/nnio.c
    src/nnr.c
    src/nurbs_fit.c
    src/nurbs_spline.c
    src/ps_sync_stub.c
    src/resample.c
    src/rnnr.c
    src/sbnr.c
    src/snb.c
    src/txgain_stub.c
    src/wbfm.c
)
set(WDSP_PINNED_SOURCES ${WDSP_SOURCES})
list(REMOVE_ITEM WDSP_PINNED_SOURCES ${WDSP_EDITED_SOURCES})
set_source_files_properties(${WDSP_PINNED_SOURCES}
    TARGET_DIRECTORY wdsp_static
    PROPERTIES COMPILE_OPTIONS
        "$<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-w>;$<$<C_COMPILER_ID:MSVC>:/W0>"
)
set_source_files_properties(${WDSP_EDITED_SOURCES}
    TARGET_DIRECTORY wdsp_static
    PROPERTIES COMPILE_OPTIONS
        "$<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-parentheses>"
)
