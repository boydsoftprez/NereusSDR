# third_party/libspecbleach/CMakeLists.txt
#
# Vendored libspecbleach (LGPL-2.1) via FetchContent at a pinned commit.
# Used by third_party/wdsp/src/sbnr.c (NR4 backend). See README.md for
# strategy rationale and docs/attribution/LGPL-COMPLIANCE.md for
# static-link compliance notice.
#
# Local change, 2026-11-05: the source filter below matches paths inside the
# upstream tree, not the whole path. J.J. Boyd (KG4VCF), AI-assisted via
# Anthropic Claude Code.
#
# FFTW3 linkage: libspecbleach depends on FFTW3 single-precision (fftw3f).
# NereusSDR resolves FFTW3 at the top level and exposes it via the
# FFTW3_LIBRARIES + FFTW3_INCLUDE_DIRS variables (same as wdsp_static).
# We mirror that convention rather than using an imported FFTW3::fftw3f
# target, which may not exist in all build environments.
#
# MSVC special-case: libspecbleach uses C99 VLAs (spectral_utils.c:260)
# and __attribute__((warn_unused_result)) (general_utils.h), neither of
# which MSVC's C compiler accepts. Mirror AetherSDR's solution: pre-build
# the static lib with clang-cl (Clang's MSVC-ABI driver — VLA-capable +
# .obj layout MSVC's linker accepts) when targeting MSVC.

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

# Glob upstream sources once. Both build paths (clang-cl on MSVC, native
# C compiler everywhere else) consume this list.
file(GLOB_RECURSE SPECBLEACH_SOURCES CONFIGURE_DEPENDS
    "${libspecbleach_upstream_SOURCE_DIR}/src/*.c")

# Exclude any standalone test or example programs that have their own main().
# Matched on the path inside the upstream tree: on the whole path, a build under
# any folder whose name starts with "test" lost every source.
set(_specbleach_all_sources ${SPECBLEACH_SOURCES})
set(SPECBLEACH_SOURCES "")
foreach(_specbleach_source IN LISTS _specbleach_all_sources)
    file(RELATIVE_PATH _specbleach_relative
        "${libspecbleach_upstream_SOURCE_DIR}" "${_specbleach_source}")
    if(NOT _specbleach_relative MATCHES "/(test|example|demo)")
        list(APPEND SPECBLEACH_SOURCES "${_specbleach_source}")
    endif()
endforeach()

if(NOT SPECBLEACH_SOURCES)
    message(FATAL_ERROR
        "libspecbleach source glob returned no files — FetchContent did not "
        "populate libspecbleach_upstream_SOURCE_DIR "
        "(${libspecbleach_upstream_SOURCE_DIR})?")
endif()

if(MSVC)
    # ── MSVC path: compile via clang-cl, archive into specbleach.lib ──
    find_program(CLANG_CL_EXECUTABLE clang-cl
        HINTS
            "$ENV{ProgramFiles}/LLVM/bin"
            "$ENV{ProgramW6432}/LLVM/bin"
            "C:/Program Files/LLVM/bin")

    if(NOT CLANG_CL_EXECUTABLE)
        message(FATAL_ERROR
            "libspecbleach (NR4) requires clang-cl on Windows because MSVC "
            "does not parse C99 VLAs (spectral_utils.c:260) or "
            "__attribute__ extensions (general_utils.h). "
            "Install LLVM/Clang (https://github.com/llvm/llvm-project/releases) "
            "and ensure clang-cl.exe is on PATH or in C:/Program Files/LLVM/bin/, "
            "OR set -DENABLE_SPECBLEACH=OFF at the top-level cmake invocation.")
    endif()

    set(SPECBLEACH_BUILD_DIR "${CMAKE_BINARY_DIR}/third_party/libspecbleach/clang-cl")
    file(MAKE_DIRECTORY "${SPECBLEACH_BUILD_DIR}")
    set(SPECBLEACH_OUT_LIB "${SPECBLEACH_BUILD_DIR}/specbleach.lib")

    # Build include-flag list with proper quoting for paths-with-spaces.
    set(SPECBLEACH_CL_INCLUDES
        "-I${libspecbleach_upstream_SOURCE_DIR}/include"
        "-I${libspecbleach_upstream_SOURCE_DIR}/src")
    if(FFTW3_INCLUDE_DIRS)
        foreach(_inc IN LISTS FFTW3_INCLUDE_DIRS)
            list(APPEND SPECBLEACH_CL_INCLUDES "-I${_inc}")
        endforeach()
    endif()

    # clang-cl flags:
    #   -w                                — silence upstream warnings
    #   --target=x86_64-pc-windows-msvc   — MSVC ABI .obj output
    #   /MD                               — match NereusSDR runtime
    #   /O2                               — release optimization
    add_custom_command(
        OUTPUT  "${SPECBLEACH_OUT_LIB}"
        COMMAND "${CLANG_CL_EXECUTABLE}" -w --target=x86_64-pc-windows-msvc /MD /O2
                ${SPECBLEACH_CL_INCLUDES}
                -c ${SPECBLEACH_SOURCES}
        COMMAND lib /nologo /out:specbleach.lib *.obj
        WORKING_DIRECTORY "${SPECBLEACH_BUILD_DIR}"
        DEPENDS ${SPECBLEACH_SOURCES}
        COMMENT "Building libspecbleach with clang-cl (VLA-capable, MSVC-ABI)"
        VERBATIM)

    add_custom_target(specbleach_clang_cl_build DEPENDS "${SPECBLEACH_OUT_LIB}")

    # Expose to consumers as an interface target — same name (specbleach_static)
    # so wdsp_static and the rest of the tree don't care which path built it.
    add_library(specbleach_static INTERFACE)
    add_dependencies(specbleach_static specbleach_clang_cl_build)
    target_link_libraries(specbleach_static INTERFACE
        "${SPECBLEACH_OUT_LIB}"
        ${FFTW3_LIBRARIES})
    target_include_directories(specbleach_static INTERFACE
        "${libspecbleach_upstream_SOURCE_DIR}/include")

    message(STATUS "libspecbleach (NR4) — clang-cl path: ${CLANG_CL_EXECUTABLE}")
else()
    # ── Linux / macOS / MinGW path: native C compiler, regular STATIC lib ──
    add_library(specbleach_static STATIC ${SPECBLEACH_SOURCES})

    target_include_directories(specbleach_static PUBLIC
        "${libspecbleach_upstream_SOURCE_DIR}/include")
    target_include_directories(specbleach_static PRIVATE
        "${libspecbleach_upstream_SOURCE_DIR}/src")

    # C_STANDARD 11 covers _Static_assert in configurations.h on toolchains
    # whose default C dialect is older (e.g. MSVC C89 — handled above — or a
    # cross-compile that defaults conservatively).
    set_target_properties(specbleach_static PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF)

    if(FFTW3_INCLUDE_DIRS)
        target_include_directories(specbleach_static PRIVATE ${FFTW3_INCLUDE_DIRS})
    endif()
    if(FFTW3_LIBRARIES)
        target_link_libraries(specbleach_static PUBLIC ${FFTW3_LIBRARIES})
    else()
        message(WARNING
            "FFTW3_LIBRARIES not set — libspecbleach (specbleach_static) may "
            "fail to link. Ensure top-level CMake resolves FFTW3 before "
            "including third_party/libspecbleach/.")
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(specbleach_static PRIVATE
            -Wno-unused-function
            -Wno-unused-variable
            -Wno-deprecated-declarations)
    endif()
endif()
