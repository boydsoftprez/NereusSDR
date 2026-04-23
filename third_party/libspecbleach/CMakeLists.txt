# third_party/libspecbleach/CMakeLists.txt
#
# Vendored libspecbleach (LGPL-2.1) via FetchContent at a pinned commit.
# Used by third_party/wdsp/src/sbnr.c (NR4 backend). See README.md for
# strategy rationale and docs/attribution/LGPL-COMPLIANCE.md for
# static-link compliance notice.
#
# FFTW3 linkage: libspecbleach depends on FFTW3 single-precision (fftw3f).
# NereusSDR resolves FFTW3 at the top level and exposes it via the
# FFTW3_LIBRARIES + FFTW3_INCLUDE_DIRS variables (same as wdsp_static).
# We mirror that convention rather than using an imported FFTW3::fftw3f
# target, which may not exist in all build environments.

cmake_minimum_required(VERSION 3.20)

include(FetchContent)

# Pinned to match the API used by Thetis v2.10.3.13 @ 501e3f51 libspecbleach bundle.
# Verified 2026-04-23: sbnr.c uses specbleach_adenoiser.h (adaptive denoiser API —
# specbleach_adaptive_initialize / specbleach_adaptive_process). That header was
# present from commit 41d3f58 (2026-01-05) up to commit 622626c where it was merged
# into specbleach_denoiser.h. We pin to 41d3f58 (the refactor that first introduced
# the dedicated adenoiser header) so both specbleach_adenoiser.h and the matching
# adaptive C implementation are present.
# Note: the earlier GIT_TAG (d4cb7af) was post-merger and lacked the adenoiser header.
FetchContent_Declare(
    libspecbleach_upstream
    GIT_REPOSITORY https://github.com/lucianodato/libspecbleach.git
    GIT_TAG        41d3f58310391e05ecfb8b7c9efb62ea2ba8ef05
    GIT_SHALLOW    FALSE   # full clone required for specific SHA pin
)

FetchContent_MakeAvailable(libspecbleach_upstream)

# Build our own static-lib target — don't depend on upstream's CMake
# defaults which may diverge or produce unwanted shared-lib targets.
# sbnr.c includes <specbleach_adenoiser.h> with angle brackets, so the
# include/ directory at the source root must go PUBLIC.
file(GLOB_RECURSE SPECBLEACH_SOURCES "${libspecbleach_upstream_SOURCE_DIR}/src/*.c")

# Exclude any standalone test or example programs that have their own main().
list(FILTER SPECBLEACH_SOURCES EXCLUDE REGEX "/(test|example|demo)")

if(NOT SPECBLEACH_SOURCES)
    message(FATAL_ERROR
        "libspecbleach source glob returned no files — FetchContent did not "
        "populate libspecbleach_upstream_SOURCE_DIR "
        "(${libspecbleach_upstream_SOURCE_DIR})?")
endif()

add_library(specbleach_static STATIC ${SPECBLEACH_SOURCES})

target_include_directories(specbleach_static PUBLIC
    "${libspecbleach_upstream_SOURCE_DIR}/include")

target_include_directories(specbleach_static PRIVATE
    "${libspecbleach_upstream_SOURCE_DIR}/src")

set_target_properties(specbleach_static PROPERTIES POSITION_INDEPENDENT_CODE ON)

# FFTW3 linkage — mirror the convention used by third_party/wdsp/CMakeLists.txt.
# NereusSDR's top-level CMake populates FFTW3_INCLUDE_DIRS + FFTW3_LIBRARIES
# (which include both double and single-precision on Linux/macOS, or the
# Windows DLL paths). libspecbleach uses fftw3f (single-precision) internally.
if(FFTW3_INCLUDE_DIRS)
    target_include_directories(specbleach_static PRIVATE ${FFTW3_INCLUDE_DIRS})
endif()
if(FFTW3_LIBRARIES)
    target_link_libraries(specbleach_static PUBLIC ${FFTW3_LIBRARIES})
else()
    message(WARNING
        "FFTW3_LIBRARIES not set — libspecbleach (specbleach_static) may fail "
        "to link. Ensure top-level CMake resolves FFTW3 before including "
        "third_party/libspecbleach/.")
endif()

# Silence known upstream warnings so NereusSDR's -Wall / -Wextra don't fire
# on vendored third-party code.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(specbleach_static PRIVATE
        -Wno-unused-function
        -Wno-unused-variable
        -Wno-deprecated-declarations)
endif()
