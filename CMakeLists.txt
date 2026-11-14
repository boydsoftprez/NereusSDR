cmake_minimum_required(VERSION 3.20)
project(Longpath VERSION 0.10.0 LANGUAGES C CXX)

# ── ccache compiler launcher (2026-07-25) ──────────────────────────────
# CI has always configured ccache explicitly; local dev machines never
# had it wired at all. Auto-detect and use it when present so branch
# switches and reconfigures stop paying full compile cost (measured:
# 1,537 CPU-s for a full compile, 313s app + 1,101s tests + 123s moc).
#
# Guarded on CMAKE_CXX_COMPILER_LAUNCHER being undefined so an explicit
# -DCMAKE_CXX_COMPILER_LAUNCHER=... on the configure line always wins,
# and so CI's own ccache setup is not double-applied.
#
# PREREQUISITE: ccache needs `sloppiness=pch_defines,time_macros` or it
# bypasses every TU compiled with -include-pch, which here is nearly all
# of them (see the §PCH block later in this file).  The symptom is a
# cheerful "ccache enabled" followed by a 0% hit rate.  Set it once with:
#     ccache --set-config sloppiness=pch_defines,time_macros
# CI does this in its "Configure ccache" step; see docs/development/
# fast-test-loop.md for the developer-side setup.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM AND NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}" CACHE STRING "C compiler launcher")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "CXX compiler launcher")
endif()
# Report the resulting state, not the branch taken above. The set() calls
# populate the CMake cache, so on every reconfigure after the first the
# `NOT DEFINED` guard is false — reporting from inside that branch would
# claim "not in use" on every run but the first, while ccache was in fact
# working the whole time.
if(CMAKE_CXX_COMPILER_LAUNCHER)
    message(STATUS "ccache enabled: ${CMAKE_CXX_COMPILER_LAUNCHER}")
else()
    message(STATUS "ccache not in use (not found on PATH)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for clangd / IDE integration

# Windows: prevent windows.h from defining min/max preprocessor macros
# (collides with std::min / std::max). Standard pattern for Qt apps;
# applied globally so any TU that transitively pulls in <windows.h>
# gets safe defaults.
#
# Intentionally NOT defining WIN32_LEAN_AND_MEAN: PortAudio's DirectSound
# backend (third_party dsound.h) depends on LPWAVEFORMATEX which lives in
# mmreg.h / mmsystem.h, both of which WIN32_LEAN_AND_MEAN excludes.
if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

# macOS: ensure Homebrew lib/include paths are in the search path
#
# Only for local builds. Releases switch it off
# (-DLONGPATH_HOMEBREW_PATHS=OFF, release.yml): link_directories() also puts
# /opt/homebrew/lib into the app's LC_RPATH, AHEAD of its own Frameworks.
# The shipped Intel 0.6.4 carried it; on a Mac with Homebrew Qt installed
# such an app loads that Qt instead of the bundled one -- with the official
# Qt 6.8 the bundled plugins then refuse to load ("mismatching Qt
# versions") and Longpath does not start (reproduced 2026-09-29).
option(LONGPATH_HOMEBREW_PATHS
       "macOS: add Homebrew lib/include to the search paths (local builds only)" ON)
if(APPLE AND LONGPATH_HOMEBREW_PATHS)
    execute_process(COMMAND brew --prefix OUTPUT_VARIABLE HOMEBREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(HOMEBREW_PREFIX)
        link_directories("${HOMEBREW_PREFIX}/lib")
        include_directories("${HOMEBREW_PREFIX}/include")
    endif()
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# --------------------------------------------------------------------------
# Performance build flags (2026-05-26 KG4VCF jitter investigation)
# --------------------------------------------------------------------------
# Bench finding: spectrum + DSP hot paths benefit measurably from -O3 over
# the default -O2 (RelWithDebInfo).  Apple Silicon also benefits from LTO
# (link-time optimization) because cross-TU inlining unlocks NEON
# vectorisation that O3-per-TU misses.  Both opt-in via CMake options so
# operators or CI can disable if a specific build hits an optimizer bug.
#
# These apply only to Release / RelWithDebInfo; Debug builds stay
# unoptimised so single-step debugging still works.
option(LONGPATH_AGGRESSIVE_OPT "Use -O3 (instead of -O2) for release builds" ON)
# 2026-05-26 KG4VCF: LTO default flipped to OFF.  With LTO enabled on
# the shared LongpathObjs object library, every test binary (526 of
# them) inherits the LTO bitcode and triggers a multi-minute LTO link
# of its own.  Net effect on a clean build: 1+ hour, load avg 250+,
# unusable while building.  Per-target IPO=FALSE on tests doesn't
# undo the bitcode embed.  Default LTO off makes dev builds fast;
# release artifacts can opt in via `cmake -DLONGPATH_ENABLE_LTO=ON`.
option(LONGPATH_ENABLE_LTO     "Enable link-time optimization (slow build; release artifacts only)" OFF)

if(LONGPATH_AGGRESSIVE_OPT)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        # Replace any pre-existing -O2 with -O3 so RelWithDebInfo gets
        # the upgrade (CMake's default for RelWithDebInfo on most
        # platforms is "-O2 -g").
        string(REGEX REPLACE "-O[0-9s]" "-O3" CMAKE_CXX_FLAGS_RELWITHDEBINFO
               "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
        string(REGEX REPLACE "-O[0-9s]" "-O3" CMAKE_C_FLAGS_RELWITHDEBINFO
               "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
        string(REGEX REPLACE "-O[0-9s]" "-O3" CMAKE_CXX_FLAGS_RELEASE
               "${CMAKE_CXX_FLAGS_RELEASE}")
        string(REGEX REPLACE "-O[0-9s]" "-O3" CMAKE_C_FLAGS_RELEASE
               "${CMAKE_C_FLAGS_RELEASE}")
        message(STATUS "Longpath: -O3 enabled for ${CMAKE_BUILD_TYPE}")
    endif()
endif()

if(LONGPATH_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _lto_ok OUTPUT _lto_err)
    if(_lto_ok)
        # 2026-05-26 KG4VCF: LTO is applied PER-TARGET below (only the
        # main Longpath executable + the parallel LongpathObjs_LTO
        # object library it links).  We deliberately do NOT set the
        # global CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO
        # flag, because that would make the shared LongpathObjs (used
        # by 526 test executables) produce LTO bitcode, and every test
        # link would then trigger a multi-minute LTO pass.  Net effect
        # of the old global setting on a clean build: ~1 hour, load
        # avg 250+, dev-loop unusable while building.
        set(LONGPATH_LTO_AVAILABLE TRUE)
        message(STATUS "Longpath: link-time optimization enabled (main app only)")
    else()
        message(WARNING "Longpath: LTO requested but unsupported -- ${_lto_err}")
    endif()
endif()

# --------------------------------------------------------------------------
# Qt6 components
# --------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Multimedia
    Svg
    WebSockets
    LinguistTools
)

# WebSockets is REQUIRED (not optional) because both the TCI server and the
# FreeDV Reporter / PSK Reporter spot ingest depend on it. v0.5.0 shipped a
# macOS x86_64 build with `qtwebsockets` missing from the install-qt-action
# modules list; CMake's QUIET find_package silently left HAVE_WEBSOCKETS
# undefined and the resulting binary disabled both features at runtime with
# only a warning in the log. Mirroring the v0.1.1 LONGPATH_GPU_SPECTRUM lesson
# (see release.yml Configure step): if a feature is mandatory, the build
# system has to refuse to configure without it.
#
# LinguistTools (2026-10-14) steht aus demselben Grund in derselben Liste.
# Betreiber: "englisch durchgehend, aber deutsch als option". Eine Sprache,
# die man waehlen kann, ist eine Zusage an den Bediener -- kein Extra, das
# bei einem Laeufer ohne qttools stillschweigend wegfallen darf. Wer das
# QUIET macht, baut genau die Fassung, die im Menue eine Sprache anbietet
# und dann englisch bleibt. An den Pipelines war dafuer nichts zu tun:
# ab Qt 6.8 steckt qttools in qtbase und ist nicht einmal als -m Modul
# auflistbar (so steht es in release.yml an der aqt-Zeile).

# Optional Qt components
find_package(Qt6 QUIET COMPONENTS SerialPort)
if(Qt6SerialPort_FOUND)
    message(STATUS "Qt6::SerialPort found — serial PTT/CW support enabled")
else()
    message(STATUS "Qt6::SerialPort not found — serial PTT/CW support disabled")
endif()

# --------------------------------------------------------------------------
# zlib (Phase 3M-3a-ii follow-up — gzip+base64url envelope helper for
# the parametric-EQ blob round-trip with Thetis profile storage)
# --------------------------------------------------------------------------
# Hard-required because ParaEqEnvelope is in the always-on TX pipeline.
#
# macOS:  SDK ships zlib at /Library/Developer/CommandLineTools/.../usr/include/zlib.h
# Linux:  distro package zlib1g-dev / zlib-devel
# Windows MinGW: Qt's bundled mingw_64 toolchain has no findable zlib (ci.yml
#   Windows job hits "Could NOT find ZLIB (missing: ZLIB_LIBRARY ...)" at
#   configure time). Vendor zlib via FetchContent on Windows only — same
#   approach the PortAudio block below uses for its dep.
if(WIN32)
    include(FetchContent)
    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        v1.3.1
    )
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Skip zlib examples" FORCE)
    FetchContent_MakeAvailable(zlib)
    if(NOT TARGET ZLIB::ZLIB)
        # zlib's CMake exports `zlibstatic` (and `zlib` for shared); alias to
        # the conventional ZLIB::ZLIB name so call sites stay portable.
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
    message(STATUS "zlib (vendored via FetchContent) — Windows MinGW build")
else()
    find_package(ZLIB REQUIRED)
    message(STATUS "zlib ${ZLIB_VERSION_STRING} found at ${ZLIB_INCLUDE_DIRS}")
endif()

# --------------------------------------------------------------------------
# PortAudio v19 (Phase 3O VAX — PortAudioBus fallback for IAudioBus)
# --------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    portaudio
    GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
    GIT_TAG        v19.7.0  # verified cmake-clean release
)

# PortAudio build options — turn off ASIO. Direct ASIO support was
# dropped during the 2026-04-19 GPL compliance review (see plan §"GPL
# Compliance Review"); PortAudio's built-in ASIO host API is gated off
# for the same reason (the Steinberg ASIO SDK license is
# GPL-incompatible). All other PortAudio host APIs remain enabled:
# MME / DS / WDM-KS / WASAPI on Windows, CoreAudio on macOS,
# ALSA / Pulse / PipeWire / JACK on Linux.
set(PA_BUILD_SHARED OFF CACHE BOOL "PortAudio static" FORCE)
set(PA_BUILD_STATIC ON  CACHE BOOL "PortAudio static" FORCE)
set(PA_USE_ASIO    OFF CACHE BOOL "PortAudio ASIO host API — forced OFF for GPL compliance (Steinberg SDK incompatible)" FORCE)

# Linux audio backends — force ON so the static PortAudio actually contains
# the host-API modules. PortAudio's CMake nominally auto-detects ALSA/JACK
# via find_package, but the FetchContent path in 19.7.0 silently gives them
# empty defaults if the option() calls fire before the auto-detect block.
# Without these forces the resulting libportaudio.a has zero host APIs and
# Pa_GetHostApiCount() returns 0 → all device dropdowns empty → no audio.
if(UNIX AND NOT APPLE)
    set(PA_USE_ALSA ON CACHE BOOL "PortAudio ALSA host API" FORCE)
    set(PA_USE_JACK ON CACHE BOOL "PortAudio JACK host API" FORCE)
endif()

# PortAudio v19.7.0 uses `cmake_minimum_required(VERSION 2.8)`, which CMake
# 4.x rejects because compatibility with CMake < 3.5 has been removed. Pin
# the minimum policy version for the nested project so FetchContent can
# configure it under modern CMake without touching the vendored source.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum CMake policy version for vendored subprojects" FORCE)

FetchContent_MakeAvailable(portaudio)

# PortAudio's Windows host APIs (pa_win_ds.c, pa_win_wasapi.c, pa_win_wdmks.c)
# call windows.h's min / max macros directly. The global NOMINMAX above
# strips those macros, leading to MinGW link errors against libportaudio.a.
# Undef NOMINMAX for portaudio_static only — same surgical pattern used for
# wdsp_static in third_party/wdsp/CMakeLists.txt.
if(WIN32 AND TARGET portaudio_static)
    target_compile_options(portaudio_static PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/UNOMINMAX>
        $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-UNOMINMAX>
    )
endif()

# --------------------------------------------------------------------------
# FFTW3 (for client-side FFT — spectrum and waterfall)
# --------------------------------------------------------------------------
find_package(PkgConfig QUIET)
if(WIN32)
    set(FFTW3_ROOT "${CMAKE_SOURCE_DIR}/third_party/fftw3")

    # The double-precision FFTW DLL is GPLv2 and gitignored
    # (third_party/fftw3/.gitignore), so a fresh Windows clone doesn't have
    # it. Auto-fetch from fftw.org on first configure (same URL release.yml
    # and ci.yml use). Without this, a clean Windows build fails with a
    # cryptic "No rule to make target libfftw3-3.dll" link error.
    if(NOT EXISTS "${FFTW3_ROOT}/bin/libfftw3-3.dll")
        set(_fftw_url "https://fftw.org/pub/fftw/fftw-3.3.5-dll64.zip")
        set(_fftw_zip "${CMAKE_BINARY_DIR}/fftw-3.3.5-dll64.zip")
        set(_fftw_tmp "${CMAKE_BINARY_DIR}/fftw-tmp")
        message(STATUS "FFTW3: libfftw3-3.dll missing, fetching ${_fftw_url}")
        file(DOWNLOAD "${_fftw_url}" "${_fftw_zip}"
             STATUS _fftw_dl_status
             TLS_VERIFY ON
             SHOW_PROGRESS)
        list(GET _fftw_dl_status 0 _fftw_dl_code)
        if(NOT _fftw_dl_code EQUAL 0)
            list(GET _fftw_dl_status 1 _fftw_dl_msg)
            message(FATAL_ERROR
                "FFTW3 download failed (${_fftw_dl_msg}).\n"
                "Manually download ${_fftw_url}, then extract:\n"
                "  fftw3.h            -> ${FFTW3_ROOT}/include/\n"
                "  libfftw3-3.dll     -> ${FFTW3_ROOT}/bin/\n"
                "  libfftw3-3.def     -> ${FFTW3_ROOT}/lib/\n"
                "and rerun CMake.")
        endif()
        file(MAKE_DIRECTORY "${_fftw_tmp}")
        file(ARCHIVE_EXTRACT INPUT "${_fftw_zip}" DESTINATION "${_fftw_tmp}")
        file(MAKE_DIRECTORY "${FFTW3_ROOT}/include")
        file(MAKE_DIRECTORY "${FFTW3_ROOT}/bin")
        file(MAKE_DIRECTORY "${FFTW3_ROOT}/lib")
        file(COPY "${_fftw_tmp}/fftw3.h"        DESTINATION "${FFTW3_ROOT}/include")
        file(COPY "${_fftw_tmp}/libfftw3-3.dll" DESTINATION "${FFTW3_ROOT}/bin")
        file(COPY "${_fftw_tmp}/libfftw3-3.def" DESTINATION "${FFTW3_ROOT}/lib")
        message(STATUS "FFTW3: fetched libfftw3-3.dll into ${FFTW3_ROOT}/bin/")
    endif()

    if(EXISTS "${FFTW3_ROOT}/include/fftw3.h")
        set(FFTW3_FOUND TRUE)
        set(FFTW3_INCLUDE_DIRS "${FFTW3_ROOT}/include")
        set(FFTW3_DLL "${FFTW3_ROOT}/bin/libfftw3-3.dll")
        # Prefer a proper .lib import library when available. MSVC link.exe
        # refuses to link against a DLL directly (LNK1107 "invalid or
        # corrupt file"), while MinGW's ld is happy either way. release.yml
        # generates third_party/fftw3/lib/fftw3.lib from the .def file;
        # ci.yml (MinGW) skips that and links the DLL directly.
        if(EXISTS "${FFTW3_ROOT}/lib/fftw3.lib")
            set(FFTW3_LIBRARIES "${FFTW3_ROOT}/lib/fftw3.lib")
            message(STATUS "FFTW3: using import library ${FFTW3_ROOT}/lib/fftw3.lib")
        else()
            set(FFTW3_LIBRARIES "${FFTW3_ROOT}/bin/libfftw3-3.dll")
            message(STATUS "FFTW3: linking directly against DLL (no fftw3.lib)")
        endif()
        # Float precision FFTW3 for FFTEngine display FFT
        if(EXISTS "${FFTW3_ROOT}/lib/libfftw3f-3.a")
            list(APPEND FFTW3_LIBRARIES "${FFTW3_ROOT}/lib/libfftw3f-3.a")
            set(FFTW3F_DLL "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
            message(STATUS "FFTW3F (float) found — FFTEngine enabled")
        elseif(EXISTS "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
            list(APPEND FFTW3_LIBRARIES "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
            set(FFTW3F_DLL "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
            message(STATUS "FFTW3F (float) found — FFTEngine enabled (DLL direct)")
        endif()
    else()
        message(STATUS "FFTW3 not found at ${FFTW3_ROOT}. Client-side FFT disabled.")
    endif()
else()
    if(PkgConfig_FOUND)
        pkg_check_modules(FFTW3 fftw3)
    endif()
    if(NOT FFTW3_FOUND)
        find_library(FFTW3_LIB fftw3)
        find_path(FFTW3_INC fftw3.h)
        if(FFTW3_LIB AND FFTW3_INC)
            set(FFTW3_FOUND TRUE)
            set(FFTW3_LIBRARIES ${FFTW3_LIB})
            set(FFTW3_INCLUDE_DIRS ${FFTW3_INC})
        endif()
    endif()
endif()
if(FFTW3_FOUND)
    message(STATUS "FFTW3 found — client-side FFT enabled")
    # Also find fftw3f (single-precision) for FFTEngine display FFT
    if(NOT WIN32)
        find_library(FFTW3F_LIB fftw3f HINTS ${FFTW3_LIBRARY_DIRS})
        if(FFTW3F_LIB)
            list(APPEND FFTW3_LIBRARIES ${FFTW3F_LIB})
            message(STATUS "FFTW3F (float) found — FFTEngine enabled")
        endif()
    endif()
else()
    message(STATUS "FFTW3 not found — client-side FFT disabled")
endif()

# --------------------------------------------------------------------------
# libpipewire-0.3 — optional native Linux audio bridge (Phase 3O PipeWire).
# Detection-only here; actual PipeWire sources land behind LONGPATH_HAVE_PIPEWIRE
# in follow-up tasks. If the dev package is absent the Linux audio path stays
# on the existing pactl / LinuxPipeBus FIFO route.
# --------------------------------------------------------------------------
if(UNIX AND NOT APPLE)
    pkg_check_modules(PIPEWIRE libpipewire-0.3>=0.3.50)
    if(PIPEWIRE_FOUND)
        message(STATUS "PipeWire ${PIPEWIRE_VERSION} — native Linux audio bridge enabled")
    else()
        message(STATUS "libpipewire-0.3 >= 0.3.50 not found — Linux audio limited to pactl path")
    endif()
endif()

# --------------------------------------------------------------------------
# WDSP DSP library (required for signal processing)
# --------------------------------------------------------------------------
option(ENABLE_WDSP "Build with WDSP DSP engine" ON)
if(ENABLE_WDSP)
    set(WDSP_DIR ${CMAKE_SOURCE_DIR}/third_party/wdsp)
    if(EXISTS "${WDSP_DIR}/src/comm.h")
        set(WDSP_FOUND TRUE)
        message(STATUS "WDSP found — DSP engine enabled")
    else()
        set(WDSP_FOUND FALSE)
        message(STATUS "WDSP not found at ${WDSP_DIR} — DSP engine disabled (stub mode)")
    endif()
endif()

# On Windows, generate FFTW3 import library from .def if .lib is missing
if(WIN32 AND FFTW3_FOUND AND NOT EXISTS "${FFTW3_ROOT}/lib/fftw3.lib")
    find_program(MSVC_LIB lib.exe)
    if(MSVC_LIB)
        execute_process(
            COMMAND ${MSVC_LIB} /def:${FFTW3_ROOT}/lib/libfftw3-3.def
                    /machine:x64 /out:${FFTW3_ROOT}/lib/fftw3.lib
            WORKING_DIRECTORY ${FFTW3_ROOT}/lib
            RESULT_VARIABLE LIB_RESULT
        )
        if(LIB_RESULT EQUAL 0)
            set(FFTW3_LIBRARIES "${FFTW3_ROOT}/lib/fftw3.lib")
            message(STATUS "Generated FFTW3 import library")
        endif()
    else()
        # Fallback: link directly against DLL (MinGW compatible)
        set(FFTW3_LIBRARIES "${FFTW3_ROOT}/bin/libfftw3-3.dll")
        # Re-append float import library if available
        if(EXISTS "${FFTW3_ROOT}/lib/libfftw3f-3.a")
            list(APPEND FFTW3_LIBRARIES "${FFTW3_ROOT}/lib/libfftw3f-3.a")
        elseif(EXISTS "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
            list(APPEND FFTW3_LIBRARIES "${FFTW3_ROOT}/bin/libfftw3f-3.dll")
        endif()
        message(STATUS "FFTW3: linking directly against DLL (lib.exe not found)")
    endif()
endif()

# --------------------------------------------------------------------------
# GPU-accelerated spectrum/waterfall (QRhi — requires Qt 6.7+)
#
# History: v0.1.1 shipped a Windows installer that silently disabled
# GPU spectrum because this block used `message(STATUS)` fallbacks when
# the private-module detection failed, and the shipping pipeline never
# explicitly requested -DLONGPATH_GPU_SPECTRUM=ON. Users saw a blank
# spectrum area and no log line from SpectrumWidget::initialize().
#
# The check now does three things differently:
#   1. Probes for both a _FOUND variable AND an imported target, since
#      Qt's private-module CMake exports have varied across 6.x minor
#      releases and install-qt-action layouts.
#   2. Captures a specific reason string when either component is
#      missing so the error message can point the user at the fix.
#   3. Promotes the fallback to FATAL_ERROR when the caller opted in
#      to GPU spectrum (the default) — silently shipping a CPU-only
#      binary is the exact footgun that caused v0.1.1. Users who
#      genuinely want the CPU path must pass -DLONGPATH_GPU_SPECTRUM=OFF
#      explicitly to acknowledge it.
# --------------------------------------------------------------------------
option(LONGPATH_GPU_SPECTRUM "Enable GPU-accelerated spectrum/waterfall via QRhi (requires Qt 6.7+ with ShaderTools and Gui private headers)" ON)

# Remember whether the caller requested it before any auto-disable, so
# we can produce a hard failure if we can't honor the request.
set(_longpath_gpu_spectrum_requested ${LONGPATH_GPU_SPECTRUM})
set(_longpath_gpu_fail_reason "")

# Defined here, before any branch, so that every later use is against a
# variable that certainly exists. An undefined variable in an if() is
# false, which would have made a missing definition look exactly like a
# missing ShaderTools — the same silent skip this is guarding against.
set(_longpath_have_shadertools FALSE)
set(_longpath_have_guiprivate  FALSE)

if(LONGPATH_GPU_SPECTRUM)
    if(Qt6_VERSION VERSION_LESS "6.7")
        set(_longpath_gpu_fail_reason
            "Qt ${Qt6_VERSION} is older than 6.7 (QRhiWidget requires 6.7+)")
        set(LONGPATH_GPU_SPECTRUM OFF)
    else()
        # Ask CMake to locate the private modules. QUIET so we can emit
        # a single, context-rich failure message below instead of
        # multiple cryptic find_package warnings.
        find_package(Qt6 QUIET COMPONENTS ShaderTools GuiPrivate)

        # Belt-and-braces detection: accept either the _FOUND variable
        # or the imported target. install-qt-action on Windows has
        # shipped Qt 6.7/6.8 layouts where Qt6GuiPrivate_FOUND is not
        # set even though the Qt6::GuiPrivate target exists, which is
        # what silently broke v0.1.1.
        if(Qt6ShaderTools_FOUND OR TARGET Qt6::ShaderTools)
            set(_longpath_have_shadertools TRUE)
        endif()
        if(Qt6GuiPrivate_FOUND OR TARGET Qt6::GuiPrivate)
            set(_longpath_have_guiprivate TRUE)
        endif()

        if(_longpath_have_shadertools AND _longpath_have_guiprivate)
            message(STATUS "QRhi support enabled — GPU spectrum/waterfall active")
        else()
            set(_longpath_gpu_fail_reason "Qt6 private modules missing:")
            if(NOT _longpath_have_shadertools)
                string(APPEND _longpath_gpu_fail_reason " ShaderTools")
            endif()
            if(NOT _longpath_have_guiprivate)
                string(APPEND _longpath_gpu_fail_reason " GuiPrivate")
            endif()
            set(LONGPATH_GPU_SPECTRUM OFF)
        endif()
    endif()
endif()

# Fail loudly if the caller asked for GPU spectrum and we couldn't
# deliver it. This prevents a repeat of the v0.1.1 silent-CPU-fallback
# shipping incident.
if(_longpath_gpu_spectrum_requested AND NOT LONGPATH_GPU_SPECTRUM)
    message(FATAL_ERROR
        "LONGPATH_GPU_SPECTRUM was requested but cannot be enabled.\n"
        "Reason: ${_longpath_gpu_fail_reason}\n"
        "Fix options:\n"
        "  - Ubuntu/Debian (25.10+ splits ShaderTools into its own package):\n"
        "      sudo apt install qt6-base-private-dev qt6-shadertools-dev\n"
        "  - Arch/CachyOS/Manjaro: sudo pacman -S qt6-shadertools\n"
        "  - Fedora: sudo dnf install qt6-qtshadertools-devel\n"
        "  - macOS (Homebrew): shadertools and private headers ship with qt@6.\n"
        "  - install-qt-action (CI): ensure 'qtshadertools' is in the modules "
        "list; private headers come with qtbase by default on Linux/macOS but "
        "some install-qt-action Windows layouts need the private headers "
        "pulled in via the Qt installer.\n"
        "  - Local dev (Qt online installer): check 'Qt Shader Tools'; private "
        "headers ship with the base Qt module.\n"
        "  - To build without GPU spectrum (CPU-only fallback), re-run cmake "
        "with -DLONGPATH_GPU_SPECTRUM=OFF.")
endif()

# --------------------------------------------------------------------------
# Source files
# --------------------------------------------------------------------------
set(CORE_SOURCES
    src/core/AntennaLabels.cpp
    src/core/antenna/AmateurBands.cpp
    src/core/antenna/Touchstone.cpp
    src/core/antenna/Feedline.cpp
    src/core/antenna/TrimSession.cpp
    src/core/antenna/AntennaSweep.cpp
    src/core/antenna/AntennaTrim.cpp
    src/core/antenna/RadioSweep.cpp
    src/core/AppSettings.cpp
    src/gui/FensterKnoepfe.cpp
    src/core/Sprache.cpp
    src/core/SpracheTabelle.cpp
    src/core/CrashReportDigest.cpp
    src/core/LogDatei.cpp
    src/core/audio/BusMitFrist.cpp
    src/core/audio/AudioRateMatcher.cpp
    src/core/audio/RxRatenAngleich.cpp
    src/core/LogbookDatei.cpp
    src/core/LogbuchRueckschau.cpp
    src/core/SpotAuswahl.cpp
    src/core/SettingsBackup.cpp
    src/core/RxProfileManager.cpp
    src/core/CtcssDetector.cpp
    src/core/DcsCode.cpp
    src/core/DcsDetector.cpp
    src/core/UiScale.cpp
    src/core/StartWatch.cpp
    src/core/SpeProtocol.cpp
    src/core/SpeConnection.cpp
    src/core/Kpa500Protocol.cpp
    src/core/Kpa500Connection.cpp
    src/core/AcomProtocol.cpp
    src/core/AcomConnection.cpp
    src/core/CwDecoder.cpp
    src/core/DigiFrequenzen.cpp
    src/core/Ft8Decoder.cpp
    src/core/Ft8Rufzeichen.cpp
    src/core/Ft8Subtract.cpp
    src/core/Ft8Apriori.cpp
    src/core/Ft8Ablauf.cpp
    src/core/Ft8Mehrfach.cpp
    src/core/Ft8Stream.cpp
    src/core/Ft8Uhrversatz.cpp
    src/core/Ft8Tx.cpp
    # Eine freie Sendestelle suchen -- Longpath-eigen, weder WSJT-X
    # noch MSHV tun das (2026-10-30)
    src/core/Ft8Tonwahl.cpp
    src/core/Ft8Warteschlange.cpp
    src/core/GridsGearbeitet.cpp
    # Locator-Jagd: Mosaik je Band + gehoerte Stationen (2026-11-11)
    src/core/LocatorJagd.cpp
    src/core/BeamHeading.cpp
    src/core/BoardCapabilities.cpp
    src/core/CatProtocol.cpp
    src/core/RigctldServer.cpp
    src/core/ThetisProfilImport.cpp
    src/core/ProfilHerkunft.cpp
    src/core/strip/ZeusKanalzug.cpp
    src/core/ZeusProfilImport.cpp
    src/core/ZeusAbfrage.cpp
    src/core/CatPuffer.cpp
    src/core/CatPtyServer.cpp
    src/core/CatServer.cpp
    src/core/CallsignCache.cpp
    src/core/CouplerZero.cpp
    src/core/ClientPuduMonitor.cpp
    src/core/BuildIdentity.cpp
    src/core/FFTRouter.cpp
    src/core/SliceStreamAllocator.cpp
    src/core/WidebandFrameAccumulator.cpp
    src/core/WidebandFftEngine.cpp
    src/core/AudioDeviceConfig.cpp
    src/core/SampleRateCatalog.cpp
    src/core/SkuUiProfile.cpp
    src/core/HardwareProfile.cpp
    src/core/LogCategories.cpp
    src/core/DevAutomationServer.cpp
    src/core/RttyDecoder.cpp
    src/core/RttyTx.cpp
    src/core/SupportBundle.cpp
    src/core/RadioDiscovery.cpp
    src/core/RadioConnection.cpp
    src/core/TciProtocol.cpp
    src/core/TciSendQueue.cpp
    src/core/TciVfoCoalescer.cpp
    src/core/TciBinaryFrame.cpp
    src/core/TciServer.cpp
    src/core/TciClient.cpp
    src/core/codec/AlexFilterMap.cpp
    src/core/codec/P1CodecStandard.cpp
    src/core/codec/P1CodecAnvelinaPro3.cpp
    src/core/codec/P1CodecRedPitaya.cpp
    src/core/codec/P1CodecHl2.cpp
    src/core/codec/P2CodecHermes.cpp
    src/core/codec/P2CodecOrionMkII.cpp
    src/core/codec/P2CodecSaturn.cpp
    src/core/P1RadioConnection.cpp
    src/core/P2RadioConnection.cpp
    src/core/PlaybackRadioConnection.cpp
    src/core/AudioZuIq.cpp
    src/core/CatRigConnection.cpp
    src/core/Ts590Cat.cpp
    src/core/CatRigBruecke.cpp
    src/core/Ts590Sprache.cpp
    src/core/SunSdrRadioConnection.cpp
    src/core/ReceiverManager.cpp
    src/core/AudioEngine.cpp
    # ── KiwiSDR (2026-08-23, Nachtschicht) ───────────────────────────
    # Portiert aus AetherSDR [@31b29583]. Stufenweise: erst das
    # Protokoll (reine Logik, vollstaendig pruefbar), dann Verbindung,
    # Verzeichnis, Bedienflaeche.
    src/core/KiwiSdrProtocol.cpp
    src/core/KiwiSdrClient.cpp
    src/core/KiwiPublicDirectory.cpp
    src/core/UpdateChecker.cpp
    src/core/UpdateInstaller.cpp
    src/core/KiwiSdrRedirectPolicy.cpp
    src/core/KiwiSdrCredentialStore.cpp
    src/core/KiwiSdrManager.cpp
    src/core/ReceivePresentationSync.cpp
    src/core/WdspEngine.cpp
    src/core/RxChannel.cpp
    # Rauschminderung fuer Ton, der schon demoduliert ankommt (Flex,
    # Kiwi, TCI). Siehe den Kopf der Datei: NR1/NR2/NR4 sind
    # WDSP-Einheiten und haben dort kein Gegenstueck, NR3 (rnnoise) und
    # MNR (Apple) schon.
    src/core/ExternalAudioNr.cpp
    src/core/dsp/FftwPlannerLock.cpp
    src/core/TxChannel.cpp
    src/core/RadeChannel.cpp
    src/core/RadeText.cpp
    src/core/Resampler.cpp
    src/core/PsFeedbackChannel.cpp
    src/core/PureSignal.cpp
    src/core/PsccPump.cpp
    src/core/PsccPump.h
    src/core/TxWorkerThread.cpp
    src/core/NbFamily.cpp
    src/core/NbFamily.h
    src/core/dsp/ChannelConfig.h
    src/core/dsp/RxChannelState.h
    src/core/MoxController.cpp
    src/core/MicProfileManager.cpp
    src/core/ParaEqEnvelope.cpp
    src/core/TwoToneController.cpp
    src/core/SwrSweepController.cpp
    src/core/TxSliceArbiter.cpp
    src/core/TxSliceArbiter.h
    src/core/FFTEngine.cpp
    src/core/NoiseFloorEstimator.cpp
    src/core/NoiseFloorTracker.cpp
    src/core/PassbandSnr.cpp
    src/core/LogbookStats.cpp
    src/core/ClarityController.cpp
    src/core/StepAttenuatorController.cpp
    src/core/CalibrationController.cpp
    src/core/PaCalProfile.cpp
    src/core/PaGainProfile.cpp
    src/core/PaProfile.cpp
    src/core/PaProfileManager.cpp
    src/core/PaTelemetryScaling.cpp
    src/core/PaTempUnit.cpp
    src/core/OcMatrix.cpp
    src/core/RadioStatus.cpp
    src/core/SettingsHygiene.cpp
    src/core/Hl2OptionsModel.cpp
    src/core/IoBoardHl2.cpp
    src/core/HermesLiteBandwidthMonitor.cpp
    src/core/ModelPaths.cpp
    src/core/accessories/AlexController.cpp
    src/core/accessories/ApolloController.cpp
    src/core/accessories/N2adrPreset.cpp
    src/core/accessories/PennyLaneController.cpp
    src/core/audio/MasterMixer.cpp
    src/core/audio/MicReorderBuffer.cpp
    src/core/audio/PcMicSource.cpp
    src/core/audio/RadeTxFilters.cpp
    src/core/audio/RadeTxFilters.h
    src/core/audio/RadioMicSource.cpp
    src/core/audio/VaxTxMicSource.cpp
    src/core/audio/CompositeTxMicRouter.cpp
    src/core/audio/PortAudioBus.cpp
    src/core/audio/QAudioSinkAdapter.cpp
    src/core/audio/RealtimeAudioPriority.cpp
    src/core/audio/TxMicSource.cpp
    src/core/audio/VirtualCableDetector.cpp
    src/core/safety/BandPlanGuard.cpp
    src/core/safety/RegionSetting.cpp
    src/core/safety/SwrProtectionController.cpp
    src/core/safety/SwrRiegel.cpp
    src/core/safety/FlexAbstimmRegler.cpp
    src/core/safety/FlexAbstimmRegler.h
    src/core/safety/TxInhibitMonitor.cpp
    src/core/safety/safety_constants.cpp
    src/core/SpotCollectorClient.cpp
    src/core/PotaClient.cpp
    src/core/PotaParkInfoClient.cpp
    src/core/PotaAlertsClient.cpp
    src/core/SotaClient.cpp
    src/core/DxClusterClient.cpp
    src/core/WsjtxClient.cpp
    src/core/FreeDVReporterClient.cpp
    src/core/FreeDVRadeReporterBridge.cpp
    src/core/PskReporterClient.cpp
    src/core/CtyDatParser.cpp
    src/core/VoiceSignalDetector.cpp
    src/core/SignalHistoryStore.cpp
    src/core/audio/AudioTapRing.cpp
    src/core/audio/QsoRecorderController.cpp
    src/core/audio/WavFile.cpp
    src/core/audio/WavPlayer.cpp
    src/core/audio/VoiceKeyer.cpp
    src/core/audio/VoiceKeyerPlayer.cpp
    src/core/audio/AnsageAufbereitung.cpp
    src/core/hotkeys/HotKeyActionList.cpp
    src/core/hotkeys/HotKeyBindings.cpp
    src/core/roundtable/RoundtableWire.cpp
    src/core/roundtable/RoundtableKey.cpp
    src/core/roundtable/RoundtableClient.cpp
    src/core/roundtable/RoundtableSignalFloor.cpp
    src/core/hotkeys/HotKeyChord.cpp
    src/core/audio/QsoRecorder.cpp
    src/core/audio/WavRecorder.cpp
    src/core/audio/WavRecorderController.cpp
    src/core/audio/IqRecorder.cpp
    src/core/audio/IqRecorderController.cpp
    src/core/audio/RecordingScheduler.cpp
    src/core/sunsdr/SunSdrProtocol.cpp
    src/core/sunsdr/SunSdrTxPacer.cpp
    src/core/AdifParser.cpp
    src/core/DxccWorkedStatus.cpp
    src/core/DxccColorProvider.cpp
    # Phase 3P-II PGXL/TGXL accessories
    src/core/PgxlConnection.cpp
    src/core/TgxlConnection.cpp
    # Phase 3P-III RF-Kit RF2K-S amplifier
    src/core/Rf2ksConnection.cpp
    src/core/LanDiscovery.cpp
    src/core/SmartSdrApiListener.cpp
    src/core/ConnectionDiagnostics.cpp
    # Phase 3P-II TGXL tuner model
    src/models/TunerModel.cpp
    # Phase 3P-II Phase 4 (Advanced UI + helpers)
    src/core/FaultLog.cpp
    src/core/TuneMemoryStore.cpp
    src/core/TxInterlockPolicy.cpp
    # FlexRadio UDP 4992 discovery beacon (PGXL/TGXL auto-detect)
    src/core/FlexRadioDiscoveryBroadcaster.cpp
    # Flex-Treiber Stufe 1: Erkennung (UDP 4992, passiv) und
    # Befehlskanal (TCP 4992, Saetze C/R/S/M). Keine Stroeme.
    src/core/FlexRadioDiscovery.cpp
    src/core/FlexRadioConnection.cpp
    src/core/FlexApdState.cpp
    # Stufe 2: der VITA-49-Leser. Nur Bytes und Rahmen, kein Sockel —
    # gegen den Mitschnitt vom 2026-10-18 pruefbar
    # (docs/protocols/flex-vita49-gemessen.md).
    src/core/FlexVitaReader.cpp
    # Der Sockel darum: ein UDP-Port fuer alles, Anmeldung per Null-Byte
    src/core/FlexVitaStream.cpp
    # Fehlende Tonpakete als Stille wieder einschieben, statt die Welle
    # springen zu lassen (gemessen 2026-10-29: 1,2 % Verlust ueber WLAN)
    src/core/FlexLueckenfuellung.cpp
    # Multi-homed-host source-IP picker (PGXL/TGXL/broadcaster per-peer
    # route probe + socket source bind)
    src/core/RouteProbe.cpp
    # Perf instrumentation (toggleable in-spectrum overlay + 1 Hz
    # memory-pressure poll; used to ground-truth jitter investigations)
    src/core/PerfMonitor.cpp
    src/core/MemoryPressure.cpp
    # mlock() helper -- pins hot buffers (waterfall, overlay, FFT
    # scratch, audio ring) so the kernel can not compress / page them
    # under heavy memory pressure (parallel builds, Spotlight, etc.).
    src/core/MemoryLock.cpp
)

# CoreAudioHalBus — macOS-only IAudioBus implementation bridging to the
# LongpathVAX HAL plugin via POSIX shared memory. The header is
# cross-platform-safe (falls through to error stubs off macOS) but the .cpp
# pulls in <sys/mman.h> / shm_open, so only compile it on Apple targets.
#
# MacMicPermission — Objective-C++ helper that calls
# AVCaptureDevice.requestAccessForMediaType:AVMediaTypeAudio at app launch so
# macOS surfaces the microphone permission dialog deterministically (issue
# #203). Without it, the prompt only fires as a side effect of opening a
# hardware mic, which silently fails on Mac mini / Mac Studio rigs and on
# machines whose only inputs are audio interfaces.
if(APPLE)
    list(APPEND CORE_SOURCES src/core/audio/CoreAudioHalBus.cpp)
    list(APPEND CORE_SOURCES src/core/MacMicPermission.mm)
    # Das eigene Fenster einer Audio Unit (CoreAudioKit).
    list(APPEND CORE_SOURCES src/core/strip/AuPluginEditor.mm)
else()
    # Ueberall sonst gibt es keine Audio Units -- aber AuPluginEditor.h
    # verspricht die Rufe plattformweit, also muessen sie auch da sein.
    # Ohne das fehlen beim Linken close/available/show.
    list(APPEND CORE_SOURCES src/core/strip/AuPluginEditorStub.cpp)
endif()

# LinuxPipeBus — Linux-only IAudioBus implementation bridging to PulseAudio /
# PipeWire virtual audio nodes via named FIFOs and pactl module-pipe-source /
# module-pipe-sink. The header compiles on all platforms (falls through to
# error stubs off Linux) but the .cpp uses mkfifo / QProcess(pactl), so only
# compile it on Linux targets. Phase 3O Sub-Phase 6 Task 6.1.
#
# LinuxAudioBackend — pure detection logic for the PipeWire-native bridge.
# Uses only Qt + std (no PipeWire symbols) so it is NOT gated on
# PIPEWIRE_FOUND. Linux-only because the whole decision tree is dead code
# on macOS / Windows (which have their own IAudioBus impls).
if(UNIX AND NOT APPLE)
    list(APPEND CORE_SOURCES src/core/audio/LinuxPipeBus.cpp)
    list(APPEND CORE_SOURCES src/core/audio/LinuxAudioBackend.cpp)
endif()

set(MODEL_SOURCES
    src/models/RecordingModel.cpp
    src/models/RadioModel.cpp
    src/models/RxDspWorker.cpp
    src/models/Band.cpp
    src/models/BandDefaults.cpp
    src/models/BandPlanManager.cpp
    src/models/SliceModel.cpp
    src/models/MemoryRecord.cpp
    src/models/MemoryList.cpp
    src/models/PanadapterModel.cpp
    src/models/MeterModel.cpp
    src/models/TransmitModel.cpp
    src/models/FilterPresetStore.cpp
    src/models/bandstack/BandStackStore.cpp
    src/models/bandstack/BandStackSeeds.cpp
    src/models/bandstack/BandStackController.cpp
    src/models/NotchModel.cpp
    src/models/SpotModel.cpp
    src/models/RoundtableHub.cpp
    src/models/SpotTableModel.cpp
    src/models/BandFilterProxy.cpp
    src/models/AlertsTableModel.cpp
    src/models/FreeDVStationModel.cpp
    src/models/RxDecodeModel.cpp
)

set(GUI_SOURCES
    # Theme substitution for Qt stylesheet strings — see
    # docs/design/HAUSSTIL.md and gui/styles/ThemeQss.h for why the
    # palette moves through a table rather than through 1733 edits.
    src/gui/LayoutProfiles.cpp
    src/gui/LayoutProfiles.h
    src/gui/WindowPlacement.cpp
    src/gui/WindowPlacement.h
    src/gui/SideAreaWindow.cpp
    src/gui/SideAreaWindow.h
    # Instrumente: eine Quellenliste, ein Zeichenweg. ReadingSource ist
    # die Beschreibung der Messgroessen (Skala, Schwelle, Teilung),
    # InstrumentSpine die Geometrie (Bogen / Gerade), InstrumentPainter
    # die drei Mittel (Mulde, Verlauf, Glut) -- genau einmal.
    src/gui/instruments/ReadingSource.cpp
    src/gui/instruments/ReadingSource.h
    src/gui/instruments/PeakHold.h
    src/gui/instruments/InstrumentSpine.cpp
    src/gui/instruments/InstrumentSpine.h
    src/gui/instruments/InstrumentPainter.cpp
    src/gui/instruments/InstrumentPainter.h
    src/gui/instruments/InstrumentFooter.cpp
    src/gui/instruments/InstrumentFooter.h
    src/gui/instruments/NeedleInstrument.cpp
    src/gui/instruments/NeedleInstrument.h
    src/gui/instruments/BarInstrument.cpp
    src/gui/instruments/BarInstrument.h
    src/gui/instruments/FrequencyInstrument.cpp
    src/gui/instruments/FrequencyInstrument.h
    src/gui/widgets/CommandBar.cpp
    src/gui/widgets/WidgetPicker.cpp
    src/gui/widgets/WidgetPicker.h
    src/gui/widgets/CommandBar.h
    src/gui/styles/ThemeQss.cpp
    src/gui/styles/ThemeQss.h
    src/gui/styles/Theme.cpp
    src/gui/styles/Theme.h
    src/gui/MainWindow.cpp
    src/gui/AmpViewChart.cpp
    src/gui/AmpViewWindow.cpp
    src/gui/PsaIndicatorWidget.cpp
    src/gui/PsaIndicatorWidget.h
    src/gui/PsForm.cpp
    src/gui/TitleBar.cpp
    src/gui/RelayBar.cpp
    src/gui/WaterfallHistoryBuffer.cpp
    src/gui/WaterfallHistoryBuffer.h
    src/gui/widgets/ProfileRail.cpp
    src/gui/widgets/ProfileRail.h
    src/gui/ConnectionPanel.cpp
    src/gui/AddCustomRadioDialog.cpp
    src/gui/SupportDialog.cpp
    src/gui/AboutDialog.cpp
    src/gui/VaxFirstRunDialog.cpp
    src/gui/spectrum/ActivePeakHoldTrace.cpp
    src/gui/spectrum/PeakBlobDetector.cpp
    src/gui/spectrum/SpectrumDetector.cpp
    src/gui/spectrum/Durchlass.cpp
    src/core/SMeterSkala.cpp
    src/core/FrequenzEichung.cpp
    src/core/RundumLauf.cpp
    src/core/RotorBereich.cpp
    src/core/FlexAutoRauschsperre.cpp
    src/core/LogbuchBilder.cpp
    src/core/LogbuchGesamtbilder.cpp
    src/core/SolarFluxArchiv.cpp
    src/core/Hoehenprofil.cpp
    src/core/HoehenDaten.cpp
    src/core/HttpAbruf.cpp
    src/core/SotaGipfel.cpp
    src/core/SotaNachtrag.cpp
    src/core/Grauzone.cpp
    src/core/GfzIndizes.cpp
    src/core/SingleInstanceGuard.cpp
    src/gui/spectrum/SpectrumAvenger.cpp
    src/gui/DssRenderer.cpp
    src/gui/SpectrumWidget.cpp
    src/gui/PanadapterApplet.cpp
    src/gui/PanadapterStack.cpp
    src/gui/PanFloatingWindow.cpp
    src/gui/PanLayoutDialog.cpp
    src/gui/ImdOverlay.h
    src/gui/ImdOverlay.cpp
    src/gui/dbm_strip_math.cpp
    src/gui/popup_placement.cpp
    src/gui/SpectrumOverlayMenu.cpp
    src/gui/spectrum/WaterfallTicker.cpp
    src/gui/spectrum/WaterfallTimeMarkers.cpp
    src/gui/widgets/CenterMarkSlider.h
    src/gui/widgets/DexpPeakMeter.h
    src/gui/widgets/DexpPeakMeter.cpp
    # Wie weit das FT8-Fenster ist, mit den Marken fuer Signalanfang
    # und -ende (Entwurf C, 2026-10-30)
    src/gui/widgets/Ft8Fensterbalken.cpp
    src/gui/widgets/DspParamPopup.cpp
    src/gui/widgets/RasterOverlay.cpp
    src/gui/widgets/RasterOverlay.h
    src/gui/widgets/DspQuickPopups.cpp
    src/gui/widgets/DspQuickPopups.h
    src/gui/widgets/BandwidthFilterPane.cpp
    src/gui/FramelessResizer.cpp
    src/gui/WindowChrome.cpp
    src/gui/ToolWindow.cpp
    src/gui/AuxiliaryWindowLeveler.cpp
    src/gui/UpdateDialog.cpp
    src/gui/widgets/FilterPresetEditDialog.cpp
    src/gui/widgets/GuardedComboBox.h
    src/gui/widgets/GuardedSlider.h
    src/gui/widgets/MeterSlider.h
    src/gui/widgets/MeterSlider.cpp
    src/gui/widgets/OcLedStripWidget.cpp
    src/gui/widgets/ParametricEqWidget.h
    src/gui/widgets/ParametricEqWidget.cpp
    src/gui/widgets/ResetSlider.h
    src/gui/AntennaWindow.h
    src/gui/AntennaWindow.cpp
    src/gui/widgets/TxSpectrumWidget.h
    src/gui/widgets/TxSpectrumWidget.cpp
    src/gui/widgets/SwrCurveWidget.h
    src/gui/widgets/SwrCurveWidget.cpp
    src/gui/widgets/SwrChartWidget.cpp
    src/gui/widgets/SwrSweepPanel.cpp
    src/gui/widgets/QsoDetailPane.h
    src/gui/widgets/QsoDetailPane.cpp
    src/gui/widgets/LogbookStatsWidget.cpp
    src/gui/widgets/ScrollableLabel.cpp
    src/gui/widgets/StationPhoto.h
    src/gui/widgets/StationPhoto.cpp
    src/gui/widgets/TriBtn.h
    src/gui/widgets/TriBtn.cpp
    src/gui/widgets/VaxChannelSelector.cpp
    src/gui/widgets/MasterOutputWidget.cpp
    src/gui/widgets/Ft8Wasserfall.cpp
    src/gui/widgets/VfoModeContainers.cpp
    src/gui/widgets/VfoModeContainers.h
    src/gui/widgets/SliceColors.h
    src/gui/widgets/VfoStyles.h
    src/gui/widgets/MetricLabel.cpp
    src/gui/widgets/MetricLabel.h
    src/gui/widgets/SystemTile.cpp
    src/gui/chrome/ChromeFoldPlan.cpp
    src/gui/chrome/ChromeBarController.cpp
    src/gui/chrome/ChromeBarItems.cpp
    src/gui/chrome/TxSwitchBar.cpp
    src/gui/widgets/StatusBadge.cpp
    src/gui/widgets/StatusBadge.h
    src/gui/widgets/AdcOverloadBadge.cpp
    src/gui/widgets/AdcOverloadBadge.h
    src/gui/widgets/OverflowChip.cpp
    src/gui/widgets/OverflowChip.h
    src/gui/widgets/RxDashboard.cpp
    src/gui/widgets/RxDashboard.h
    src/gui/widgets/StationBlock.cpp
    src/gui/widgets/StationBlock.h
    src/gui/widgets/SpectrumStatusOverlay.cpp
    src/gui/widgets/SpectrumStatusOverlay.h
    src/gui/widgets/FilterPolicyDialog.cpp
    src/gui/widgets/FilterPolicyDialog.h
    src/gui/widgets/AntennaPickerMenu.cpp
    src/gui/widgets/AntennaPickerMenu.h
    src/gui/widgets/AntennaSwitchToast.cpp
    src/gui/widgets/AntennaSwitchToast.h
    src/gui/widgets/LayoutThumbnail.cpp
    src/gui/widgets/StatusToast.cpp
    src/gui/widgets/StatusToast.h
    src/gui/widgets/DiversityRadarWidget.cpp
    src/gui/widgets/DiversityRadarWidget.h
    src/gui/widgets/TxBoundConfirmDialog.cpp
    src/gui/widgets/TxBoundConfirmDialog.h
    src/gui/NetworkDiagnosticsDialog.cpp
    src/gui/NetworkDiagnosticsDialog.h
    src/gui/DiversityDialog.cpp
    src/gui/DiversityDialog.h
    src/gui/SpotHubDialog.cpp
    src/gui/SpotHubDialog.h
    src/gui/MemoryDialog.cpp
    src/gui/MemoryDialog.h
    src/gui/ParkInfoDialog.cpp
    src/gui/ParkInfoDialog.h
    src/gui/FreeDVReporterDialog.cpp
    src/gui/FreeDVReporterDialog.h
    src/gui/applets/KiwiSdrApplet.cpp
    src/gui/KiwiWaterfallStripWidget.cpp
    src/gui/KiwiWaterfallPanel.cpp
    src/gui/applets/AsrApplet.cpp
    src/gui/widgets/VfoTileRow.cpp
    src/gui/widgets/SliceFlag.cpp
    src/asr/AsrSegmenter.cpp
    src/asr/AsrService.cpp
    src/asr/RemoteAsrBackend.cpp
    src/asr/WhisperServerLauncher.cpp
    src/gui/MainWindow_KiwiSdr.cpp
    # Flex-Treiber Stufe 2: Ton und Bild einer FlexRadio-Maschine
    src/gui/MainWindow_Flex.cpp
    src/gui/MainWindow_Asr.cpp
    src/gui/MainWindow_SunSdr.cpp
    src/gui/MainWindow_SideArea.cpp
    src/gui/MainWindow_HotKeys.cpp
    src/gui/hotkeys/FKeyPriority.cpp
    src/gui/hotkeys/HotKeyDispatcher.cpp
    src/gui/hotkeys/KeyboardRouter.cpp
    src/gui/hotkeys/MainWindowHotKeyTargets.cpp
    src/gui/KiwiPublicReceiverPicker.cpp
    src/gui/LanScanDialog.cpp
    src/gui/LanScanDialog.h
    src/gui/PgxlSaveRebootDialog.cpp
    src/gui/PgxlSaveRebootDialog.h
    src/gui/containers/ContainerWidget.cpp
    src/gui/containers/FloatingContainer.cpp
    src/gui/containers/ContainerManager.cpp
    src/gui/containers/ContainerSettingsDialog.cpp
    src/gui/containers/MmioEndpointsDialog.cpp
    src/gui/containers/MmioVariablePickerPopup.cpp
    src/core/mmio/ITransportWorker.cpp
    src/core/mmio/MmioEndpoint.cpp
    src/core/mmio/ExternalVariableEngine.cpp
    src/core/mmio/FormatParser.cpp
    src/core/mmio/UdpEndpointWorker.cpp
    src/core/mmio/TcpListenerEndpointWorker.cpp
    src/core/mmio/TcpClientEndpointWorker.cpp
    src/core/mmio/SerialEndpointWorker.cpp
    src/gui/containers/meter_property_editors/BaseItemEditor.cpp
    src/gui/containers/meter_property_editors/BarItemEditor.cpp
    src/gui/containers/meter_property_editors/SolidColourItemEditor.cpp
    src/gui/containers/meter_property_editors/SpacerItemEditor.cpp
    src/gui/containers/meter_property_editors/FadeCoverItemEditor.cpp
    src/gui/containers/meter_property_editors/ImageItemEditor.cpp
    src/gui/containers/meter_property_editors/ScaleItemEditor.cpp
    src/gui/containers/meter_property_editors/NeedleItemEditor.cpp
    src/gui/containers/meter_property_editors/NeedleScalePwrItemEditor.cpp
    src/gui/containers/meter_property_editors/TextItemEditor.cpp
    src/gui/containers/meter_property_editors/TextOverlayItemEditor.cpp
    src/gui/containers/meter_property_editors/SignalTextItemEditor.cpp
    src/gui/containers/meter_property_editors/LedItemEditor.cpp
    src/gui/containers/meter_property_editors/HistoryGraphItemEditor.cpp
    src/gui/containers/meter_property_editors/MagicEyeItemEditor.cpp
    src/gui/containers/meter_property_editors/DialItemEditor.cpp
    src/gui/containers/meter_property_editors/WebImageItemEditor.cpp
    src/gui/containers/meter_property_editors/FilterDisplayItemEditor.cpp
    src/gui/containers/meter_property_editors/RotatorItemEditor.cpp
    src/gui/containers/meter_property_editors/ClockItemEditor.cpp
    src/gui/containers/meter_property_editors/VfoDisplayItemEditor.cpp
    src/gui/containers/meter_property_editors/ClickBoxItemEditor.cpp
    src/gui/containers/meter_property_editors/DataOutItemEditor.cpp
    src/gui/containers/meter_property_editors/ButtonBoxItemEditor.cpp
    src/gui/containers/meter_property_editors/BandButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/ModeButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/FilterButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/AntennaButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/TuneStepButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/OtherButtonItemEditor.cpp
    src/gui/containers/meter_property_editors/VoiceRecordPlayItemEditor.cpp
    src/gui/containers/meter_property_editors/DiscordButtonItemEditor.cpp
    src/gui/meters/MeterWidget.cpp
    src/gui/meters/MeterItem.cpp
    src/gui/meters/ItemGroup.cpp
    src/gui/meters/MeterPoller.cpp
    src/gui/meters/SpacerItem.cpp
    src/gui/meters/FadeCoverItem.cpp
    src/gui/meters/LEDItem.cpp
    src/gui/meters/HistoryGraphItem.cpp
    src/gui/meters/MagicEyeItem.cpp
    src/gui/meters/NeedleScalePwrItem.cpp
    src/gui/meters/SignalTextItem.cpp
    src/gui/meters/DialItem.cpp
    src/gui/meters/TextOverlayItem.cpp
    src/gui/meters/WebImageItem.cpp
    src/gui/meters/FilterDisplayItem.cpp
    src/gui/meters/RotatorItem.cpp
    src/gui/meters/ButtonBoxItem.cpp
    src/gui/meters/BandButtonItem.cpp
    src/gui/meters/ModeButtonItem.cpp
    src/gui/meters/FilterButtonItem.cpp
    src/gui/meters/AntennaButtonItem.cpp
    src/gui/meters/TuneStepButtonItem.cpp
    src/gui/meters/OtherButtonItem.cpp
    src/gui/meters/VoiceRecordPlayItem.cpp
    src/gui/meters/DiscordButtonItem.cpp
    src/gui/meters/VfoDisplayItem.cpp
    src/gui/meters/ClockItem.cpp
    src/gui/meters/ClickBoxItem.cpp
    src/gui/meters/DataOutItem.cpp
    src/gui/AntennaPopupBuilder.cpp
    src/gui/HGauge.cpp
    src/gui/ColorSwatchButton.cpp
    src/gui/SpectrumOverlayPanel.cpp
    src/gui/SetupDialog.cpp
    src/gui/SetupPage.cpp
    src/gui/setup/GeneralSetupPages.cpp
    src/gui/setup/GeneralOptionsPage.cpp
    src/gui/setup/HardwarePage.cpp
    src/gui/setup/HardwareDdcRoutingPage.cpp
    src/gui/setup/PaSetupPages.cpp
    src/gui/setup/hardware/RadioInfoTab.cpp
    src/gui/setup/hardware/AntennaAlexTab.cpp
    src/gui/setup/hardware/AntennaAlexAntennaControlTab.cpp
    src/gui/setup/hardware/AntennaAlexAlex1Tab.cpp
    src/gui/setup/hardware/AntennaAlexAlex2Tab.cpp
    src/gui/setup/hardware/OcOutputsTab.cpp
    src/gui/setup/hardware/OcOutputsHfTab.cpp
    src/gui/setup/hardware/OcOutputsSwlTab.cpp
    src/gui/setup/hardware/XvtrTab.cpp
    src/gui/setup/hardware/DiversityTab.cpp
    src/gui/setup/hardware/CalibrationTab.cpp
    src/gui/setup/hardware/PaCalibrationGroup.cpp
    src/gui/setup/hardware/Hl2IoBoardTab.cpp
    src/gui/setup/hardware/Hl2OptionsTab.cpp
    src/gui/setup/hardware/BandwidthMonitorTab.cpp
    src/gui/setup/AudioAdvancedPage.cpp
    src/gui/setup/AudioBackendStrip.cpp
    src/gui/setup/AudioDevicesPage.cpp
    src/gui/setup/AudioTxInputPage.cpp
    src/gui/setup/TestTwoTonePage.cpp
    src/gui/setup/TxProfileSetupPage.cpp
    src/gui/setup/AsrPage.cpp
    src/gui/setup/AudioTciPage.cpp
    src/gui/setup/AudioVaxPage.cpp
    src/gui/setup/DeviceCard.cpp
    src/gui/setup/DspSetupPages.cpp
    src/gui/setup/DspOptionsPage.cpp
    src/gui/setup/FilterPresetsSetupPage.cpp
    src/gui/setup/DisplaySetupPages.cpp
    src/gui/setup/SpectrumPeaksPage.cpp
    src/gui/setup/MultimeterPage.cpp
    src/gui/setup/SetupHelpers.cpp
    src/gui/setup/TransmitSetupPages.cpp
    src/gui/setup/AppearanceSetupPages.cpp
    src/gui/setup/CatNetworkSetupPages.cpp
    src/gui/setup/CompanionRigPage.cpp
    src/gui/setup/PgxlAdvancedPage.cpp
    src/gui/setup/TgxlAdvancedPage.cpp
    src/gui/setup/PgxlInterlockPage.cpp
    src/gui/setup/FourO3APage.cpp
    src/gui/setup/RfKitPage.cpp
    src/gui/setup/SpePage.cpp
    src/gui/setup/Kpa500Page.cpp
    src/gui/setup/AcomPage.cpp
    src/gui/setup/FlexApdPage.cpp
    src/gui/setup/TciLogWindow.cpp
    src/gui/setup/KeyboardSetupPages.cpp
    src/gui/setup/DiagnosticsSetupPages.cpp
    src/gui/diagnostics/RadioStatusPage.cpp
    src/gui/diagnostics/DiagnosticsPhaseHPages.cpp
    src/gui/applets/AppletWidget.cpp
    src/gui/applets/NyiOverlay.cpp
    src/gui/applets/AppletPanelWidget.cpp
    src/gui/applets/GridCell.cpp
    src/gui/applets/GridCell.h
    src/gui/applets/GridCellWidget.cpp
    src/gui/applets/GridCellWidget.h
    src/gui/applets/AppletGrid.cpp
    src/gui/applets/AppletGrid.h
    src/gui/applets/AppletKeys.cpp
    src/gui/applets/AppletKeys.h
    src/gui/applets/AppletFensterMasse.cpp
    src/gui/applets/PanelAutomatik.cpp
    src/gui/applets/AppletFloatingWindow.cpp
    src/gui/applets/AppletFloatingWindow.h
    src/gui/applets/InstrumentApplet.cpp
    src/gui/applets/InstrumentApplet.h
    src/gui/applets/FrequencyApplet.cpp
    src/gui/applets/FrequencyApplet.h
    src/gui/applets/AppletVisibilityController.cpp
    src/gui/widgets/NotchEditPopup.cpp
    src/gui/widgets/RotorDialWidget.cpp
    src/gui/widgets/RotorBandWidget.cpp
    src/gui/widgets/RotorLogbookPanel.cpp
    src/gui/widgets/GlobeWidget.cpp
    src/gui/widgets/FlatMapWidget.cpp
    src/gui/widgets/GibsTileLayer.cpp
    src/gui/widgets/FlowLayout.cpp
    src/gui/widgets/BlueMarbleFetcher.cpp
    src/gui/widgets/DxRadarWidget.cpp
    src/gui/widgets/AzimutalKarteWidget.cpp
    src/gui/widgets/LocatorJagdZeichen.cpp
    src/gui/widgets/WorldMapCatalog.cpp
    src/gui/widgets/WorldTexture.cpp
    src/gui/LogbookWindow.cpp
    src/gui/SettingsBackupDialog.cpp
    src/gui/widgets/RxProfilePopup.cpp
    src/gui/QsoMapWindow.cpp
    src/gui/Google3dSetupDialog.cpp
    src/gui/widgets/Station3dSurface.cpp
    src/core/DxccFlag.cpp
    src/core/SolarTimes.cpp
    src/core/AdifLog.cpp
    src/core/KmlExport.cpp
    src/core/WorkedBefore.cpp
    src/core/LogFilter.cpp
    src/core/CredentialStore.cpp
    src/core/GoogleMapTiles.cpp
    src/core/QrzClient.cpp
    src/core/QrzLogbookUploader.cpp
    src/core/QrzLogbookFetcher.cpp
    src/core/sat/SatelliteTracker.cpp
    src/core/sat/TleStore.cpp
    src/core/sat/SatelliteService.cpp
    src/core/CloudlogUploader.cpp
    src/core/AdifNetworkUploader.cpp
    src/core/RotctldClient.cpp
    src/core/Gs232Rotor.cpp
    src/core/HamlibDaemonProcess.cpp
    src/core/RotctldProcess.cpp
    src/core/RigctldProcess.cpp
    src/core/RigctldClient.cpp
    src/core/HamlibInstaller.cpp

    # Das Begleitgeraet, so lange wie das Programm: Besitzer der
    # Dauerverbindung. Die Einstellseite bedient es, statt eine zweite
    # Verbindung auf dasselbe rigctld aufzumachen.
    src/core/CompanionRigController.cpp

    # Transverter: was das ZF-Geraet zeigt, ist nicht, was auf der
    # Antenne liegt. Reine Rechnung, ohne Geraet pruefbar.
    src/core/TransverterSetup.cpp

    # ── Nereus Audio Channel Strip — DSP (ported from AetherSDR) ──
    #
    # The strip's nine stages, taken before any of its user interface.
    # They are Qt-free apart from QtGlobal and lock-free by construction,
    # so they can be tested without a radio, an audio device or a window
    # — which is why they come first.
    src/core/strip/ClientPhaseRotator.cpp
    src/core/strip/ClientGate.cpp
    src/core/strip/ClientEq.cpp
    src/core/strip/ClientDeEss.cpp
    src/core/strip/ClientComp.cpp
    src/core/strip/ClientTube.cpp
    src/core/strip/ClientPudu.cpp
    src/core/strip/ClientReverb.cpp
    src/core/strip/ClientFinalLimiter.cpp
    # The runner: the eight stages in order, with the master bypass the
    # operator's A/B comparison depends on.
    src/core/strip/MicSpectrum.cpp
    src/core/strip/BandLimit.cpp
    src/core/strip/LoudnessMatch.cpp
    src/core/strip/ClientClipper.cpp
    src/core/strip/NoiseReduction.cpp
    src/core/strip/StripChain.cpp
    src/core/strip/AudioPluginAu.cpp
    src/core/strip/PluginPlacement.cpp
    src/core/strip/LtasSpectrum.cpp
    src/core/strip/StripLiveMonitor.cpp
    src/core/strip/StripSettings.cpp
    src/core/strip/AetherPreset.cpp
    src/core/strip/BroadcastOptimizer.cpp
    src/core/strip/StripTuner.cpp
    src/core/strip/EqLoudness.cpp
    src/core/strip/StripCharacters.cpp
    src/core/strip/StripTargets.cpp
    src/core/strip/TargetFromFile.cpp
    src/core/strip/TxSpectrumAnalysis.cpp
    src/core/TxAudioRecorder.cpp
    src/core/Loudness.cpp
    src/core/VoiceAnalyzer.cpp
    # Header-only Q_OBJECT: AUTOMOC needs the explicit listing, same as
    # QsoUploader above.
    src/core/RotorController.h
    # Header-only Q_OBJECT: AUTOMOC only picks up a header when a source
    # file of the same base name exists, so an interface class with no
    # .cpp has to be listed explicitly or its vtable never gets built.
    src/core/QsoConfirmation.cpp
    src/core/QsoUploader.h
    src/models/LogEntry.cpp
    src/gui/applets/KombiApplet.cpp
    src/gui/applets/RxApplet.cpp
    src/gui/applets/TxApplet.cpp
    src/gui/applets/TxEqDialog.cpp
    src/gui/applets/BroadcastOptimizerPanel.cpp
    src/gui/applets/TxVoiceCheckDialog.cpp
    src/gui/applets/eq/ClientEqCurveWidget.cpp
    src/gui/applets/eq/ClientEqOutputFader.cpp
    src/gui/applets/eq/EqHistory.cpp
    src/gui/applets/eq/EqHost.cpp
    src/gui/applets/eq/StripEqPanel.cpp
    src/gui/applets/eq/ClientEqEditorCanvas.cpp
    src/gui/applets/eq/ClientEqFftAnalyzer.cpp
    src/gui/applets/eq/ClientEqIconRow.cpp
    src/gui/applets/eq/ClientEqParamRow.cpp
    src/gui/applets/eq/StripEqSimpleBar.cpp
    src/gui/applets/StripGraphics.cpp
    src/gui/applets/StripWindow.cpp
    src/gui/applets/TxCfcDialog.cpp
    src/gui/applets/PhoneCwApplet.cpp
    src/gui/applets/RadeApplet.cpp
    src/gui/applets/EqApplet.cpp
    src/gui/applets/VaxApplet.cpp
    src/gui/applets/FmApplet.cpp
    src/gui/applets/DigitalApplet.cpp
    src/gui/applets/PureSignalApplet.cpp
    src/gui/applets/DiversityApplet.cpp
    src/gui/applets/CatRigApplet.cpp
    src/gui/applets/CwxApplet.cpp
    src/gui/applets/DvkApplet.cpp
    src/gui/applets/RttyDecoderApplet.cpp
    src/gui/applets/CwDecoderApplet.cpp
    src/gui/applets/Ft8Applet.cpp
    src/gui/applets/BandStackApplet.cpp
    src/gui/applets/Ft8PlaetzeApplet.cpp
    src/gui/Ft8KartenBruecke.cpp
    src/gui/applets/QsoRecorderApplet.cpp
    src/gui/applets/RecordingApplet.cpp
    src/gui/RecordingScheduleDialog.cpp
    src/gui/TraegerEichDialog.cpp
    src/gui/RundumDialog.cpp
    src/gui/widgets/LogbuchBilderWidgets.cpp
    src/gui/widgets/LogbuchGesamtbilderWidgets.cpp
    src/gui/widgets/HoehenprofilWidget.cpp
    src/gui/HoehenprofilFenster.cpp
    src/gui/SotaNachtragDialog.cpp
    src/gui/applets/BandwidthFilterApplet.cpp
    src/gui/applets/CatApplet.cpp
    src/gui/applets/TunerApplet.cpp
    src/gui/applets/AmpApplet.cpp
    src/gui/applets/Rf2ksApplet.cpp
    src/gui/applets/SpeApplet.cpp
    src/gui/applets/Kpa500Applet.cpp
    src/gui/applets/AcomApplet.cpp
    src/gui/applets/TciApplet.cpp
    src/gui/applets/ClientChainApplet.cpp
)

# MacFloatingWindowBehavior — Objective-C++ helper that sets
# NSWindowCollectionBehaviorFullScreenAuxiliary on the schwebenden
# Qt::Tool windows (AppletFloatingWindow, ToolWindow, FloatingContainer,
# PanFloatingWindow) so they follow the main window into native macOS
# fullscreen instead of being left behind on the previous Space.
# Betreiber, repeatedly reported. See MacFloatingWindowBehavior.h.
if(APPLE)
    list(APPEND GUI_SOURCES src/gui/MacFloatingWindowBehavior.mm)
    list(APPEND GUI_SOURCES src/gui/MacVollflaeche.mm)
    list(APPEND GUI_SOURCES src/gui/hotkeys/MacMenuTrackingWatch.mm)
    list(APPEND GUI_SOURCES src/gui/MacFloatingWindowBehavior.h)
endif()

# --------------------------------------------------------------------------
# Resources
# --------------------------------------------------------------------------
qt_add_resources(RESOURCES resources.qrc)

# --------------------------------------------------------------------------
# App icon (per-platform) — from main (PR #11)
# --------------------------------------------------------------------------
set(APP_ICON_SOURCES "")
if(APPLE)
    set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/resources/icons/Longpath.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND APP_ICON_SOURCES ${APP_ICON_MACOS})
elseif(WIN32)
    list(APPEND APP_ICON_SOURCES "${CMAKE_SOURCE_DIR}/resources/icons/Longpath.rc")
endif()

# --------------------------------------------------------------------------
# Object library — shared between the app executable and test targets so
# the same translation units are compiled once and consumed by both
# (Phase 3I test infrastructure). Properties on LongpathObjs propagate
# to consumers (PUBLIC) via the CMake INTERFACE; the thin executable just
# links src/main.cpp on top.
#
# APP_ICON_SOURCES is intentionally NOT in the OBJECT library: CMake's
# MACOSX_PACKAGE_LOCATION property only takes effect when the file is a
# direct source of a MACOSX_BUNDLE TRUE target. OBJECT libraries propagate
# compiled object files, not bundle-resource sources. Attach the icon to
# the executable target below so .icns lands in Contents/Resources/ and
# Windows .rc lands on the linker line.
# --------------------------------------------------------------------------
add_library(LongpathObjs OBJECT
    ${CORE_SOURCES}
    ${MODEL_SOURCES}
    ${GUI_SOURCES}
    ${RESOURCES}
)

# Linux-only sources: the PipeWire/pactl first-run + diagnose dialog uses
# POSIX <unistd.h> (getuid) and the LinuxAudioBackend probe. MainWindow.cpp
# already gates the menu wiring on Q_OS_LINUX, so the .cpp shouldn't enter
# the build on macOS or Windows.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(LongpathObjs PRIVATE src/gui/VaxLinuxFirstRunDialog.cpp)
endif()

# 2026-05-26 KG4VCF: parallel LTO-enabled object library, ONLY built
# when LONGPATH_ENABLE_LTO=ON.  Same sources as LongpathObjs above,
# but compiled with LTO bitcode so the main Longpath app's link can
# do cross-translation-unit optimization.  Tests continue to link the
# plain LongpathObjs (no bitcode), so test link times stay fast.
#
# This is more work to build a release artifact (sources compile
# twice) but it keeps the dev loop fast while preserving the LTO perf
# benefit for shipped binaries.
if(LONGPATH_LTO_AVAILABLE)
    add_library(LongpathObjs_LTO OBJECT
        ${CORE_SOURCES}
        ${MODEL_SOURCES}
        ${GUI_SOURCES}
        ${RESOURCES}
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_sources(LongpathObjs_LTO PRIVATE src/gui/VaxLinuxFirstRunDialog.cpp)
    endif()
    set_target_properties(LongpathObjs_LTO PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
endif()

# Der Text der Mikrofon-Begruendung. VOR dem Ziel gesetzt, weil ihn die
# Info.plist-Vorlage einsetzt (cmake/MacOSXBundleInfo.plist.in) — eine
# Variable, die erst danach gesetzt wird, kommt dort leer an, und ein
# leerer NSMicrophoneUsageDescription ist so gut wie keiner.
set(MACOSX_BUNDLE_MIC_USAGE
    "Longpath uses the microphone for SSB voice transmission and live mic level monitoring.")

add_executable(Longpath src/main.cpp ${APP_ICON_SOURCES})

# Auf macOS nach Contents/Resources, sonst neben die Binaerdatei.
#
# NICHT nach Contents/MacOS: dort gehoert ausfuehrbarer Code hin, und
# codesign meldet jede andere Datei als „In subcomponent" — eine
# Signatur, die eine Datendatei im Programmverzeichnis mitsiegelt, ist
# beim naechsten Aktualisieren der Datei ungueltig. Am 2026-08-20 beim
# Neusignieren der Schreibtischkopie aufgefallen.
#
# Theme::searchPaths() kennt beide Orte.
if(APPLE)
    set(_longpath_theme_dest "$<TARGET_BUNDLE_CONTENT_DIR:Longpath>/Resources/themes")
else()
    set(_longpath_theme_dest "$<TARGET_FILE_DIR:Longpath>/themes")
endif()

# ── Mitgelieferte Farbpaletten ───────────────────────────────────────
#
# Theme::searchPaths() sucht unter anderem "themes" NEBEN der
# Binaerdatei — genau dafuer ist dieser Schritt da.
#
# Nicht in themes/ im Quellbaum: das Verzeichnis steht in .gitignore
# und gehoert den eigenen Dateien des Betreibers, die eine
# gleichnamige mitgelieferte ueberschreiben duerfen (siehe
# Theme::available()).
add_custom_command(TARGET Longpath POST_BUILD
    # Erst leeren, dann fuellen (2026-10-14). copy_if_different raeumt
    # NICHT auf: eine Palette, die aus dem Quellbaum verschwindet,
    # bleibt im Paket liegen und steht weiter zur Auswahl. Genau das
    # passiert gerade mit kreide.json, die dieser Commit entfernt --
    # ohne diese Zeile haette der Betreiber sie nach dem naechsten Bau
    # immer noch im Menue.
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${_longpath_theme_dest}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_longpath_theme_dest}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/resources/themes/flach.json"
            "${CMAKE_SOURCE_DIR}/resources/themes/tief.json"
            "${CMAKE_SOURCE_DIR}/resources/themes/moos.json"
            "${_longpath_theme_dest}/"
    COMMENT "Paletten mitliefern")
if(LONGPATH_LTO_AVAILABLE)
    # Main app + its OBJECT-lib link both get LTO when enabled.  The
    # IPO property on the executable arranges -flto on the link line;
    # the IPO property on LongpathObjs_LTO above puts -flto on every
    # compile.  Together: full cross-TU optimization for the shipped
    # binary.
    target_link_libraries(Longpath PRIVATE LongpathObjs_LTO)
    set_target_properties(Longpath PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
else()
    target_link_libraries(Longpath PRIVATE LongpathObjs)
endif()

# ── Qt WebEngine: der 3D-Flug im Logbuch (2026-11-10) ──────────────────
#
# Betreiber: das Logbuch soll herausstechen -- nach dem Rufzeichen fliegt
# eine Ansicht mit Googles fotorealistischen 3D-Kacheln bis aufs Dach der
# Station. Dafuer CesiumJS in einem QWebEngineView.
#
# OPTIONAL, und zwar mit Absicht (anders als WebSockets oben): ohne
# WebEngine faellt nichts weg, was der Bediener schon hatte -- die Karte
# fliegt wie bisher ueber NASA-Bilder und bietet „View in Google Earth".
# Homebrew-Qt auf dem Mac hat WebEngine, die Linux- und Windows-Laeufer
# der CI nicht. -DLONGPATH_WEBENGINE=OFF schaltet es auch dort ab, wo es
# da waere.
#
# Eine EIGENE kleine Bibliothek statt LongpathObjs: daran haengt jeder der
# ~600 Pruefstaende, und keiner davon soll WebEngine mitlinken. Die Karte
# kennt nur die Schnittstelle Station3dSurface; main.cpp meldet die
# WebEngine-Fassung beim Start an (Station3dView::registerFactory).
option(LONGPATH_WEBENGINE
       "Build the Google 3D fly-to view in the logbook map (needs Qt WebEngine)" ON)
set(LONGPATH_HAS_WEBENGINE OFF)
if(LONGPATH_WEBENGINE)
    find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)
    if(Qt6WebEngineWidgets_FOUND)
        set(LONGPATH_HAS_WEBENGINE ON)
    endif()
endif()
if(LONGPATH_HAS_WEBENGINE)
    message(STATUS "Qt6::WebEngineWidgets found — Google 3D fly-to enabled")
    add_library(LongpathWeb3d STATIC src/gui/widgets/Station3dView.cpp)
    target_include_directories(LongpathWeb3d PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(LongpathWeb3d PUBLIC
        Qt6::Core Qt6::Widgets Qt6::Network Qt6::WebEngineWidgets)
    target_compile_definitions(LongpathWeb3d PUBLIC LONGPATH_HAS_WEBENGINE=1)
    target_link_libraries(Longpath PRIVATE LongpathWeb3d)
else()
    message(STATUS "Qt6::WebEngineWidgets not found or disabled — Google 3D "
                   "fly-to off (NASA imagery + Google Earth button instead)")
endif()

# ── Das Contestprogramm als Fenster in Longpath (2026-11-04) ──────────────
#
# Betreiber: "vielleicht sollte man das alles in longpath automatisch als
# option zusaetzlich einbauen ... es soll aber trotzdem alles auch einzeln
# funktionieren." Das Contestprogramm (oe5sos/Contestprogramm, eigenes
# Repo, GPLv3) liegt als Submodul unter third_party/contestprogramm und
# wird als Bibliothek ContestprogrammCore gelinkt; Longpath oeffnet sein
# Hauptfenster aus dem Tools-Menue (src/contest/ContestFenster.*).
#
# Vorgabe: AN, wenn das Submodul ausgecheckt ist, sonst AUS -- die CI
# checkt ohne Submodule aus und baut Longpath wie bisher. Lokal:
#     git submodule update --init third_party/contestprogramm
#
# Die Bruecke ist eine EIGENE kleine Bibliothek, weil beide Baeume ein
# src/core mit gleichnamigen Kopfdateien haben (Maidenhead.h,
# RigctldClient.h, ...): nur ContestFenster.cpp sieht die Include-Pfade
# des Contestprogramms, und ContestFenster.h zieht keine davon herein.
set(LONGPATH_CONTESTPROGRAMM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/contestprogramm"
    CACHE PATH "Quellbaum des Contestprogramms (Submodul)")
if(EXISTS "${LONGPATH_CONTESTPROGRAMM_DIR}/CMakeLists.txt")
    set(_longpath_contest_default ON)
else()
    set(_longpath_contest_default OFF)
endif()
option(LONGPATH_CONTESTPROGRAMM
    "Contestprogramm als Fenster in Longpath (braucht das Submodul third_party/contestprogramm)"
    ${_longpath_contest_default})
if(LONGPATH_CONTESTPROGRAMM)
    if(NOT EXISTS "${LONGPATH_CONTESTPROGRAMM_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "LONGPATH_CONTESTPROGRAMM ist an, aber ${LONGPATH_CONTESTPROGRAMM_DIR} "
                "ist leer. git submodule update --init third_party/contestprogramm")
    endif()
    # Qt6::Sql und Qt6::Multimedia braucht das Contestprogramm; sein
    # find_package im Unterverzeichnis legt die Ziele nur DORT an. Hier
    # noch einmal, damit auch der Pruefstand tst_contestfenster sie sieht.
    find_package(Qt6 REQUIRED COMPONENTS Sql Multimedia)
    add_subdirectory("${LONGPATH_CONTESTPROGRAMM_DIR}" contestprogramm EXCLUDE_FROM_ALL)
    add_library(LongpathContestBruecke STATIC
        src/contest/ContestFenster.h
        src/contest/ContestFenster.cpp)
    target_include_directories(LongpathContestBruecke PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/contest")
    target_link_libraries(LongpathContestBruecke
        PUBLIC  Qt6::Core Qt6::Widgets
        PRIVATE ContestprogrammCore)
    # LongpathObjs (MainWindow) ruft die Bruecke; ueber PUBLIC bekommen
    # Longpath UND jeder Pruefstand, der LongpathObjs linkt, die Bibliothek
    # und den Include-Pfad src/contest -- sonst fehlten ihnen die Symbole.
    target_compile_definitions(LongpathObjs PUBLIC LONGPATH_HAVE_CONTESTPROGRAMM=1)
    target_link_libraries(LongpathObjs PUBLIC LongpathContestBruecke)
    if(TARGET LongpathObjs_LTO)
        target_compile_definitions(LongpathObjs_LTO PUBLIC LONGPATH_HAVE_CONTESTPROGRAMM=1)
        target_link_libraries(LongpathObjs_LTO PUBLIC LongpathContestBruecke)
    endif()
    message(STATUS "Contestprogramm als Fenster: AN (${LONGPATH_CONTESTPROGRAMM_DIR})")
else()
    message(STATUS "Contestprogramm als Fenster: aus")
endif()

# ── Sprachen (2026-10-14) ────────────────────────────────────────────
#
# Betreiber: "englisch durchgehend, aber deutsch als option". Englisch
# ist die Quellsprache -- im Quelltext steht englischer Text, die
# Uebersetzung liegt daneben. qt_add_translations baut die .qm aus der
# .ts und legt sie unter :/i18n/ in die Binaerdatei, es gibt also keine
# Datei, die beim Ausliefern vergessen werden kann (genau das ist
# v0.5.0 mit rade.dll passiert).
#
# Gepflegt wird die .ts mit `cmake --build build --target update_translations`
# (ruft lupdate ueber alle Quellen) und danach dem Qt Linguist. Der
# Bau selbst ruft nur lrelease -- ein Lauf ohne lupdate bleibt
# reproduzierbar.
qt_add_translations(Longpath
    TS_FILES translations/longpath_de.ts
    RESOURCE_PREFIX "/i18n"
)

# LSMinimumSystemVersion im Info.plist (2026-09-29). Ohne den Schluessel
# startete ein zu altes macOS die App und dyld brach dann beim Laden der
# ersten zu neuen Bibliothek ab -- ein Absturz statt der Finder-Meldung
# „benoetigt macOS 12 oder neuer". Mit Zielversion (Releases) gilt sie;
# ohne (lokaler Bau gegen Homebrew) laeuft das Ergebnis nur auf diesem
# macOS, und genau das steht dann drin.
if(APPLE)
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(LONGPATH_MIN_MACOS "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        execute_process(COMMAND sw_vers -productVersion
                        OUTPUT_VARIABLE _longpath_host_macos
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET)
        string(REGEX MATCH "^[0-9]+\\.[0-9]+" LONGPATH_MIN_MACOS
               "${_longpath_host_macos}")
    endif()
endif()

set_target_properties(Longpath PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    # Eigene Vorlage: sie traegt NSMicrophoneUsageDescription, damit der
    # Schluessel schon beim Konfigurieren im Info.plist steht. Siehe den
    # Kopf von cmake/MacOSXBundleInfo.plist.in — der Nachtrag nach dem
    # Bauen ging bei jedem `cmake -B build` wieder verloren.
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in"
    MACOSX_BUNDLE_ICON_FILE "Longpath.icns"
    MACOSX_BUNDLE_BUNDLE_NAME "Longpath"
    MACOSX_BUNDLE_GUI_IDENTIFIER "at.oe5sos.longpath"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "© 2026 J.J. Boyd (KG4VCF); GPL port of Thetis (ramdor/Thetis) and mi0bot/OpenHPSDR-Thetis; descends from FlexRadio PowerSDR lineage. GPLv3."
)

# macOS strict mode requires NSMicrophoneUsageDescription in Info.plist
# for any app that opens an audio capture stream — without it the stream
# opens but macOS silently delivers zero-filled buffers (no permission
# dialog, no error). CMake's MACOSX_BUNDLE template doesn't include the
# key; inject it post-build + re-sign so TCC accepts the updated bundle.
#
# Uses Set-then-Add pattern via /bin/sh so the operation is idempotent:
# Set succeeds when the key already exists from a prior build; falls
# through to Add on the first build (when the key is absent).
if(APPLE)
    set(_longpath_mic_desc "${MACOSX_BUNDLE_MIC_USAGE}")
    add_custom_command(TARGET Longpath POST_BUILD
        COMMAND /bin/sh -c "/usr/libexec/PlistBuddy -c \"Set :NSMicrophoneUsageDescription '${_longpath_mic_desc}'\" \"$<TARGET_BUNDLE_CONTENT_DIR:Longpath>/Info.plist\" 2>/dev/null || /usr/libexec/PlistBuddy -c \"Add :NSMicrophoneUsageDescription string '${_longpath_mic_desc}'\" \"$<TARGET_BUNDLE_CONTENT_DIR:Longpath>/Info.plist\""
        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:Longpath>"
        COMMENT "Inject NSMicrophoneUsageDescription + re-sign bundle (mic-permission gate)"
        VERBATIM
    )
endif()

target_include_directories(LongpathObjs PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# --------------------------------------------------------------------------
# Linking
# --------------------------------------------------------------------------
target_link_libraries(LongpathObjs PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::Svg
    ZLIB::ZLIB
)

if(WIN32)
    # Needed by RadioDiscovery.cpp's setsockopt() call for SO_BROADCAST.
    target_link_libraries(Longpath PRIVATE ws2_32)
endif()

if(APPLE)
    # AVFoundation provides AVCaptureDevice for the launch-time mic-permission
    # prompt (src/core/MacMicPermission.mm, issue #203).
    target_link_libraries(LongpathObjs PUBLIC "-framework AVFoundation")

    # AppKit provides NSWindow/NSView for the floating-window fullscreen
    # fix (src/gui/MacFloatingWindowBehavior.mm) -- explicit here even
    # though Qt's own Cocoa platform plugin links it transitively,
    # matching this project's practice of declaring the frameworks a
    # source file directly calls into (see the AVFoundation line above).
    target_link_libraries(LongpathObjs PUBLIC "-framework AppKit")

    # IOKit: der F-Tasten-Modus des Systems (kIOHIDFKeyModeKey), solange
    # Longpath vorne ist (src/gui/hotkeys/FKeyPriority.cpp).
    target_link_libraries(LongpathObjs PUBLIC "-framework IOKit")

    # CoreAudio framework for AudioObjectGetPropertyData +
    # kAudioDevicePropertyIOThreadOSWorkgroup used by
    # src/core/audio/RealtimeAudioPriority.cpp.  The os_workgroup_*
    # APIs themselves come from libSystem (already linked) but the
    # property-query helpers and AudioDeviceID type live in CoreAudio.
    target_link_libraries(LongpathObjs PUBLIC "-framework CoreAudio")

    # AudioToolbox traegt den Audio-Unit-Wirt fuer die Einschubplaetze
    # der Sendetonkette (src/core/strip/AudioPluginAu.cpp). Systemsache,
    # kein fremdes SDK im Baum -- und genau deshalb zuerst AU und nicht
    # VST3.
    target_link_libraries(LongpathObjs PUBLIC "-framework AudioToolbox")

    # CoreAudioKit traegt AUGenericView -- Apples Reglertafel fuer
    # jede Audio Unit ohne eigene Oberflaeche
    # (src/core/strip/AuPluginEditor.mm).
    target_link_libraries(LongpathObjs PUBLIC "-framework CoreAudioKit")
endif()

if(WIN32)
    # avrt provides AvSetMmThreadCharacteristicsW + AvRevertMmThreadCharacteristics
    # used by src/core/audio/RealtimeAudioPriority.cpp to join the
    # "Pro Audio" MMCSS task class.
    target_link_libraries(LongpathObjs PUBLIC avrt)
endif()

if(Qt6SerialPort_FOUND)
    target_link_libraries(LongpathObjs PUBLIC Qt6::SerialPort)
    target_compile_definitions(LongpathObjs PUBLIC HAVE_SERIALPORT)
endif()

# Qt6::WebSockets is REQUIRED above; the link and HAVE_WEBSOCKETS define
# are unconditional. The #ifdef HAVE_WEBSOCKETS guards in TciServer,
# FreeDVReporterClient, MainWindow, and SpotHubDialog are now redundant
# but left in place to keep this PR surgical; cleanup tracked as a
# follow-up.
target_link_libraries(LongpathObjs PUBLIC Qt6::WebSockets)
target_compile_definitions(LongpathObjs PUBLIC HAVE_WEBSOCKETS)

if(FFTW3_FOUND)
    target_include_directories(LongpathObjs PUBLIC ${FFTW3_INCLUDE_DIRS})
    target_link_libraries(LongpathObjs PUBLIC ${FFTW3_LIBRARIES})
    target_compile_definitions(LongpathObjs PUBLIC HAVE_FFTW3)
endif()

# libpipewire-0.3 — gated by the pkg_check_modules block above. Only links on
# Linux when the dev package is present; LONGPATH_HAVE_PIPEWIRE lets follow-up
# PipeWire sources compile conditionally.
if(PIPEWIRE_FOUND)
    target_link_libraries(LongpathObjs PUBLIC ${PIPEWIRE_LIBRARIES})
    # SYSTEM: die PipeWire-/SPA-Kopfdateien sind fremde Kopfdateien, und sie
    # halten sich nicht an -Wpedantic. Ohne dieses Wort meldet der
    # Linux-Bau bei JEDEM Lauf 264 Warnungen aus /usr/include/spa-0.2 --
    # "ISO C++ forbids braced-groups within expressions" und
    # "ISO C++ forbids compound-literals", alle aus parser.h, builder.h,
    # hook.h und defs.h.
    #
    # Das ist kein Schoenheitsfehler: 264 fremde Warnungen decken die
    # eigenen zu. Im selben Protokoll stehen 27 echte eigene -- die findet
    # man darin nicht mehr, und genau deshalb hat sie monatelang niemand
    # angesehen. Eine Warnung, die bei jedem Bau da ist, deckt die zu, die
    # etwas bedeutet.
    #
    # SYSTEM aendert nichts am Bau ausser der Warnstufe fuer DIESE
    # Kopfdateien; unser eigener Code wird unveraendert streng geprueft.
    target_include_directories(LongpathObjs SYSTEM PUBLIC ${PIPEWIRE_INCLUDE_DIRS})
    target_compile_options(LongpathObjs PUBLIC ${PIPEWIRE_CFLAGS_OTHER})
    target_compile_definitions(LongpathObjs PUBLIC LONGPATH_HAVE_PIPEWIRE)
    target_sources(LongpathObjs PRIVATE
        src/core/audio/PipeWireThreadLoop.cpp
        src/core/audio/PipeWireStream.cpp
        src/core/audio/PipeWireBus.cpp
    )
endif()

if(WDSP_FOUND)
    # Sub-epic C-1 NR backends: rnnoise (NR3/rnnr.c) and libspecbleach (NR4/sbnr.c).
    # Must be add_subdirectory'd before wdsp so the targets exist when
    # third_party/wdsp/CMakeLists.txt pulls them in as PUBLIC link deps.
    add_subdirectory(third_party/rnnoise)
    add_subdirectory(third_party/libspecbleach)
    add_subdirectory(third_party/wdsp)
    target_link_libraries(LongpathObjs PUBLIC wdsp_static)
    target_compile_definitions(LongpathObjs PUBLIC HAVE_WDSP)

    # Der Kanalzug ruft rnnoise direkt (src/core/strip/NoiseReduction),
    # nicht ueber WDSPs rnnr.c wie der Empfang. Dieselbe Bibliothek,
    # dasselbe Modell -- nur ohne den Umweg ueber eine WDSP-Kette, die
    # es im Sendeweg gar nicht gibt.
    target_link_libraries(LongpathObjs PUBLIC rnnoise_static)
    target_compile_definitions(LongpathObjs PUBLIC HAVE_RNNOISE)

    # NNR (WDSP 2.10) weights — same treatment as the DeepFilterNet3 model
    # below: ModelPaths::probeModel("nnr", …) looks next to the binary, in
    # the bundle's Resources and in the install prefix, and only then in the
    # source tree. Without these copies NNR found nothing in an installed
    # package and ran as a pass-through, while the log claimed a model was
    # loaded (found 2026-09-22 at the HL2 workbench). nnr_model_stub.c's
    # built-in arrays are empty by design, so the .bin files are the only
    # source of weights.
    set(NNR_MODELS
        "${CMAKE_SOURCE_DIR}/third_party/wdsp/models/wdsp_nnr_0.bin"
        "${CMAKE_SOURCE_DIR}/third_party/wdsp/models/wdsp_nnr_1.bin")
    set(NNR_MODELS_PRESENT TRUE)
    foreach(_nnr_model IN LISTS NNR_MODELS)
        if(NOT EXISTS "${_nnr_model}")
            set(NNR_MODELS_PRESENT FALSE)
        endif()
    endforeach()
    if(NNR_MODELS_PRESENT)
        if(APPLE)
            add_custom_command(TARGET Longpath POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:Longpath>/Contents/Resources/models/nnr"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${NNR_MODELS} "$<TARGET_BUNDLE_DIR:Longpath>/Contents/Resources/models/nnr/"
                COMMENT "Copying NNR models to app bundle Resources/models/nnr")
        else()
            add_custom_command(TARGET Longpath POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:Longpath>/models/nnr"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${NNR_MODELS} "$<TARGET_FILE_DIR:Longpath>/models/nnr/"
                COMMENT "Copying NNR models to build directory models/nnr")
            install(FILES ${NNR_MODELS}
                DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/Longpath/models/nnr"
                OPTIONAL)
        endif()
    endif()

    # NR3 (rnnoise) weights -- dieselbe Luecke wie oben bei NNR, gefunden
    # 2026-09-27 in Martins Protokoll: die installierte Fassung (von
    # longpath-auto-install.sh aus dem CMake-Bau kopiert) meldete „NR3 model
    # not found at expected paths". Nur release.yml legte die Modelle von
    # Hand dazu; ein Bau im Quellbaum fand sie ueber den Rueckfall in
    # third_party/. third_party/rnnoise baut mit USE_WEIGHTS_FILE, also ohne
    # eingebaute Gewichte -- ohne Datei hatte NR3 gar kein Modell.
    set(RNNOISE_MODELS
        "${CMAKE_SOURCE_DIR}/third_party/rnnoise/models/Default_large.bin"
        "${CMAKE_SOURCE_DIR}/third_party/rnnoise/models/Default_small.bin")
    set(RNNOISE_MODELS_PRESENT TRUE)
    foreach(_rnn_model IN LISTS RNNOISE_MODELS)
        if(NOT EXISTS "${_rnn_model}")
            set(RNNOISE_MODELS_PRESENT FALSE)
        endif()
    endforeach()
    if(RNNOISE_MODELS_PRESENT)
        if(APPLE)
            add_custom_command(TARGET Longpath POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:Longpath>/Contents/Resources/models/rnnoise"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${RNNOISE_MODELS} "$<TARGET_BUNDLE_DIR:Longpath>/Contents/Resources/models/rnnoise/"
                COMMENT "Copying rnnoise (NR3) models to app bundle Resources/models/rnnoise")
        else()
            add_custom_command(TARGET Longpath POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:Longpath>/models/rnnoise"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${RNNOISE_MODELS} "$<TARGET_FILE_DIR:Longpath>/models/rnnoise/"
                COMMENT "Copying rnnoise (NR3) models to build directory models/rnnoise")
            install(FILES ${RNNOISE_MODELS}
                DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/Longpath/models/rnnoise"
                OPTIONAL)
        endif()
    endif()
endif()

# --------------------------------------------------------------------------
# RADE neural codec for FreeDV digital voice (Phase 3R)
# License: BSD 2-Clause (radae_nopy at b2891023f3aecdf8b1793618000b1be6bcb2c4d1)
# See docs/architecture/phase3j2-3r-spots-and-rade-design.md Section 5
#
# radae_nopy's CMakeLists.txt fetches and builds Opus from xiph at first
# build via ExternalProject_Add (cmake/BuildOpus.cmake). This adds 90-180
# seconds to the first clean configure/build cycle; incremental builds
# are unchanged. Opus build needs autoconf, automake, libtool, and a
# POSIX make on the build host.
#
# rade_api.h transitively includes opus_types.h and nnet.h from the
# Opus source tree (under build_opus-prefix/src/build_opus). Upstream
# adds these via directory-scope include_directories() in BuildOpus.cmake,
# which only reaches sources compiled inside the rade subdirectory.
# We re-expose them as target-scope include dirs on LongpathObjs so
# any future Longpath consumer that #includes "rade_api.h" can resolve
# the transitive Opus headers.
# --------------------------------------------------------------------------
add_subdirectory(third_party/rade)

if(NOT TARGET rade)
    message(FATAL_ERROR "RADE library target 'rade' not found after add_subdirectory(third_party/rade)")
endif()

target_link_libraries(LongpathObjs PUBLIC rade)
target_include_directories(LongpathObjs PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/rade/src
    ${CMAKE_BINARY_DIR}/third_party/rade/build_opus-prefix/src/build_opus/include
    ${CMAKE_BINARY_DIR}/third_party/rade/build_opus-prefix/src/build_opus/dnn
    ${CMAKE_BINARY_DIR}/third_party/rade/build_opus-prefix/src/build_opus/celt
    ${CMAKE_BINARY_DIR}/third_party/rade/build_opus-prefix/src/build_opus
)

# rade is a SHARED library on Windows (rade.dll). Longpath.exe load-time
# imports it via __declspec(dllimport), so without rade.dll on the DLL
# search path the .exe fails to launch with "rade.dll was not found".
# CMake places rade.dll in third_party/rade/src/ but the .exe lives at
# $<TARGET_FILE_DIR:Longpath>, so copy it next to the .exe at build time.
# Mirrors the ENABLE_DFNR / deepfilter.dll pattern below.
if(WIN32)
    add_custom_command(TARGET Longpath POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rade>" "$<TARGET_FILE_DIR:Longpath>"
        COMMENT "Copying rade.dll to build directory")
endif()

# --------------------------------------------------------------------------
# r8brain-free-src high-quality polyphase resampler (Phase 3R Task I2)
# License: MIT (see third_party/r8brain/LICENSE.txt)
# Vendored at SHA 5c44bebe9c477d47b1dc7037fcaae2794ff2b4e1 (2026-03-12).
#
# r8brain is header-only; the OBJECT library exists to anchor the
# function-local statics in CDSPRealFFT.h to one translation unit and
# to expose the include directory to Longpath sources that #include
# "CDSPResampler.h".
# --------------------------------------------------------------------------
add_subdirectory(third_party/r8brain)
target_link_libraries(LongpathObjs PUBLIC r8brain)

# --------------------------------------------------------------------------
# SGP4/SDP4 (Vallado reference, MIT via python-sgp4) — satellites in view
# at QSO time and on the map. See docs/attribution/SGP4-PROVENANCE.md.
# --------------------------------------------------------------------------
add_subdirectory(third_party/sgp4)
target_link_libraries(LongpathObjs PUBLIC sgp4)

# --------------------------------------------------------------------------
# ggmorse (MIT) -- the CW (Morse) decoder behind src/core/CwDecoder
# (2026-09-21). Vendored unmodified; see third_party/ggmorse/README-LONGPATH.md.
# --------------------------------------------------------------------------
add_subdirectory(third_party/ggmorse)
target_link_libraries(LongpathObjs PUBLIC ggmorse)

# --------------------------------------------------------------------------
# Mbed TLS 3.6.7, nur ECDSA P-256 + SHA-256 (Apache-2.0) -- die Anmeldung
# bei Roundtable (src/core/roundtable/RoundtableKey, 2026-11-14). Byte fuer
# Byte uebernommen; siehe third_party/mbedtls/README-LONGPATH.md.
# --------------------------------------------------------------------------
add_subdirectory(third_party/mbedtls)
target_link_libraries(LongpathObjs PUBLIC longpath_mbedcrypto)

# ── ft8_lib (FT8/FT4, MIT) ──────────────────────────────────────────
#
# Der Kern von Kārlis Gobas ft8_lib, Byte fuer Byte uebernommen; siehe
# third_party/ft8_lib/FT8LIB-PROVENANCE.md fuer Herkunft, Umfang und
# die Begruendung. Eigene statische Bibliothek, damit Longpaths
# Warnungseinstellungen nicht auf fremden Quelltext losgehen.
add_library(ft8_static STATIC
    third_party/ft8_lib/ft8/constants.c
    third_party/ft8_lib/ft8/crc.c
    third_party/ft8_lib/ft8/decode.c
    third_party/ft8_lib/ft8/encode.c
    third_party/ft8_lib/ft8/ldpc.c
    third_party/ft8_lib/ft8/message.c
    third_party/ft8_lib/ft8/text.c
    third_party/ft8_lib/common/monitor.c
    third_party/ft8_lib/common/gfsk.c
    third_party/ft8_lib/fft/kiss_fft.c
    third_party/ft8_lib/fft/kiss_fftr.c)
target_include_directories(ft8_static PUBLIC third_party/ft8_lib)
set_target_properties(ft8_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Fremder Quelltext: keine Warnungen, keine Warnungen-als-Fehler.
if(NOT MSVC)
    target_compile_options(ft8_static PRIVATE -w)
else()
    target_compile_options(ft8_static PRIVATE /w)
endif()

# Und keine drei Zeilen nach stderr bei JEDEM Fenster.
#
# common/monitor.c:4 setzt `#define LOG_LEVEL LOG_INFO` fest und
# schreibt beim Aufbau des Wasserfalls „Block size = …", „Subblock
# size = …", „N_FFT = …". Ft8Stream baut den Wasserfall einmal je
# Fenster — alle 15 s drei Zeilen, rund 720 in der Stunde, in
# derselben Datei, in der Fehler stehen sollen.
#
# Die Quelle bleibt byte-gleich (FT8LIB-PROVENANCE.md): ft8/debug.h
# definiert LOG_PRINTF nur, WENN es noch nicht definiert ist. Hier
# wird es leer definiert, die Zeilen entfallen beim Uebersetzen.
#
# Als OPTION, nicht als DEFINITION: CMake laesst Makros mit Argumenten
# durch target_compile_definitions() nicht durch („CMake is dropping a
# preprocessor definition: LOG_PRINTF(...)=", gesehen 2026-10-15) und
# haette die Zeile stillschweigend verschluckt. Als Uebersetzungsoption
# geht sie woertlich an den Uebersetzer.
#
# MSVC kennt keine Makros mit Argumenten auf der Befehlszeile; dort
# bleiben die Zeilen stehen.
if(NOT MSVC)
    target_compile_options(ft8_static PRIVATE "-DLOG_PRINTF(...)=")
endif()

target_link_libraries(LongpathObjs PUBLIC ft8_static)

# --------------------------------------------------------------------------
# DeepFilterNet3 DFNR — pre-built neural noise reduction library (MIT/Apache-2.0)
# Sub-epic C-1. Library is built by ./setup-deepfilter.sh (external to CMake
# because the Rust build is slow + needs rustc). If the lib is absent at
# configure time, ENABLE_DFNR falls back to OFF.
# Ported from AetherSDR CMakeLists.txt:323-363 [@0cd4559].
# --------------------------------------------------------------------------
option(ENABLE_DFNR "Enable DFNR DeepFilterNet3 noise reduction" ON)
if(ENABLE_DFNR)
    set(DEEPFILTER_DIR ${CMAKE_SOURCE_DIR}/third_party/deepfilter)
    if(WIN32)
        set(DFNR_LIB_DIR "${DEEPFILTER_DIR}/lib/windows-x86_64")
        if(MINGW)
            set(DFNR_LIB "${DFNR_LIB_DIR}/libdeepfilter.dll.a")
            set(DFNR_DLL "${DFNR_LIB_DIR}/deepfilter.dll")
        else()
            # MSVC: prefer .dll.lib import library, fall back to .lib
            if(EXISTS "${DFNR_LIB_DIR}/deepfilter.dll.lib")
                set(DFNR_LIB "${DFNR_LIB_DIR}/deepfilter.dll.lib")
            else()
                set(DFNR_LIB "${DFNR_LIB_DIR}/deepfilter.lib")
            endif()
            set(DFNR_DLL "${DFNR_LIB_DIR}/deepfilter.dll")
        endif()
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(DFNR_LIB_DIR "${DEEPFILTER_DIR}/lib/darwin-arm64")
        else()
            set(DFNR_LIB_DIR "${DEEPFILTER_DIR}/lib/darwin-x86_64")
        endif()
        set(DFNR_LIB "${DFNR_LIB_DIR}/libdeepfilter.a")
    else()
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
            set(DFNR_LIB_DIR "${DEEPFILTER_DIR}/lib/linux-aarch64")
        else()
            set(DFNR_LIB_DIR "${DEEPFILTER_DIR}/lib/linux-x86_64")
        endif()
        set(DFNR_LIB "${DFNR_LIB_DIR}/libdeepfilter.a")
    endif()
    if(EXISTS ${DFNR_LIB})
        message(STATUS "DFNR (DeepFilterNet3) enabled — ${DFNR_LIB}")
    else()
        set(ENABLE_DFNR OFF)
        message(STATUS "DFNR (DeepFilterNet3) disabled — library not found at ${DFNR_LIB} "
                       "(run ./setup-deepfilter.sh before cmake to enable)")
    endif()
endif()

if(ENABLE_DFNR)
    target_sources(LongpathObjs PRIVATE src/core/DeepFilterFilter.cpp)
    target_include_directories(LongpathObjs PRIVATE ${DEEPFILTER_DIR}/include)
    target_link_libraries(LongpathObjs PRIVATE ${DFNR_LIB})
    target_compile_definitions(LongpathObjs PRIVATE HAVE_DFNR)
    if(WIN32)
        target_link_libraries(Longpath PRIVATE ws2_32 bcrypt userenv ntdll)
        if(DFNR_DLL AND EXISTS ${DFNR_DLL})
            add_custom_command(TARGET Longpath POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DFNR_DLL}" "$<TARGET_FILE_DIR:Longpath>"
                COMMENT "Copying deepfilter.dll to build directory")
        endif()
    elseif(APPLE)
        # Rust runtime deps on macOS
        target_link_libraries(Longpath PRIVATE "-framework Security" "-framework CoreFoundation")
    else()
        # Rust runtime deps on Linux
        target_link_libraries(Longpath PRIVATE pthread dl m)
    endif()
    # Copy the model tarball to build dir + app bundle Resources so
    # findModelPath() in DeepFilterFilter.cpp can locate it at runtime.
    set(DFNR_MODEL "${DEEPFILTER_DIR}/models/DeepFilterNet3_onnx.tar.gz")
    if(EXISTS ${DFNR_MODEL})
        if(APPLE)
            # macOS: place in Resources/models/dfnet3/ so notarization doesn't
            # reject a non-Mach-O file inside Contents/MacOS/, and so
            # ModelPaths::probeModel() probe #2 finds it at
            # <exeDir>/../Resources/models/dfnet3/<file>.
            add_custom_command(TARGET Longpath POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:Longpath>/Contents/Resources/models/dfnet3"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DFNR_MODEL}" "$<TARGET_BUNDLE_DIR:Longpath>/Contents/Resources/models/dfnet3/"
                COMMENT "Copying DeepFilterNet3 model to app bundle Resources/models/dfnet3")
        else()
            # Non-macOS: place adjacent to exe in models/dfnet3/ subfolder so
            # ModelPaths::probeModel() probe #1 finds it at
            # <exeDir>/models/dfnet3/<file>.
            add_custom_command(TARGET Longpath POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:Longpath>/models/dfnet3"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DFNR_MODEL}" "$<TARGET_FILE_DIR:Longpath>/models/dfnet3/"
                COMMENT "Copying DeepFilterNet3 model to build directory models/dfnet3")
        endif()
        # Install the model alongside the binary for cmake --install.
        # ModelPaths probe #3 expects <exeDir>/../share/Longpath/models/dfnet3/<file>.
        if(NOT APPLE)
            install(FILES "${DFNR_MODEL}"
                DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/Longpath/models/dfnet3"
                OPTIONAL)
        endif()
    endif()
endif()

# --------------------------------------------------------------------------
# MNR — macOS Accelerate (vDSP) spectral noise reduction (Sub-epic C-1, Task 11).
# Native Apple framework — no external build script or dependencies.
# --------------------------------------------------------------------------
option(ENABLE_MNR "Enable MNR Apple Accelerate noise reduction (macOS only)" ON)
if(ENABLE_MNR AND APPLE)
    target_sources(LongpathObjs PRIVATE src/core/MacNRFilter.cpp)
    target_link_libraries(LongpathObjs PUBLIC "-framework Accelerate")
    target_compile_definitions(LongpathObjs PUBLIC HAVE_MNR)
    message(STATUS "MNR (Apple Accelerate) enabled")
elseif(ENABLE_MNR)
    set(ENABLE_MNR OFF)
    message(STATUS "MNR (Apple Accelerate) disabled — macOS only")
endif()

# PortAudio (Phase 3O VAX) — always linked; PortAudioBus is the cross-platform
# IAudioBus fallback when no native low-latency backend is available.
target_link_libraries(LongpathObjs PRIVATE portaudio_static)
target_include_directories(LongpathObjs PUBLIC ${portaudio_SOURCE_DIR}/include)

if(LONGPATH_GPU_SPECTRUM)
    target_link_libraries(LongpathObjs PUBLIC Qt6::GuiPrivate)
    target_compile_definitions(LongpathObjs PUBLIC LONGPATH_GPU_SPECTRUM)

    # NOTE: this must use the SAME test as the gate at the top of this
    # file, and for a long time it did not.
    #
    # The gate accepts `Qt6ShaderTools_FOUND OR TARGET Qt6::ShaderTools`,
    # precisely because Qt's private-module exports have shipped layouts
    # where the target exists and the _FOUND variable is never set. This
    # block tested only _FOUND. In that layout the gate passes, no
    # FATAL_ERROR fires, LONGPATH_GPU_SPECTRUM stays ON and is defined —
    # and then the shaders are silently skipped. The result is a full GPU
    # build with zero shaders embedded, no warning anywhere, and a
    # waterfall that draws whatever undefined GPU memory holds.
    #
    # That is the v0.1.1 footgun rebuilt out of an inconsistent copy of
    # its own fix. One variable, set once at the gate, is the only way
    # these two places cannot drift apart again.
    if(_longpath_have_shadertools)
        qt_add_shaders(LongpathObjs "longpath_shaders"
            PREFIX "/shaders"
            FILES
                resources/shaders/waterfall.vert
                resources/shaders/waterfall.frag
                resources/shaders/spectrum.vert
                resources/shaders/spectrum.frag
                resources/shaders/overlay.vert
                resources/shaders/overlay.frag
        )

        qt_add_shaders(LongpathObjs "longpath_meter_shaders"
            PREFIX "/shaders"
            FILES
                resources/shaders/meter_textured.vert
                resources/shaders/meter_textured.frag
                resources/shaders/meter_geometry.vert
                resources/shaders/meter_geometry.frag
        )
    endif()
endif()

# --------------------------------------------------------------------------
# Build tag: identifies a smoke build in the window title
# --------------------------------------------------------------------------
# Every non-release build stamps its branch and short SHA into the title bar,
# so a build under test can be told apart from a release, from another
# worktree's build, and from whatever was already running.
#
# Standing rule (JJ, KG4VCF, 2026-07-30). Earned: this session had a build
# running from one worktree while another was being tested, and settling which
# binary was on screen took a `pgrep` against the executable path. The title
# bar should have answered it.
#
# Release artifacts get an empty tag and keep the clean "Longpath 0.5.2"
# title. Detection needs no CI change: release.yml checks out `ref: <tag>`
# (.github/workflows/release.yml), so HEAD sits exactly on a tag there and
# nowhere else.
#
# The derivation itself lives in cmake/LongpathBuildTag.cmake and runs once per
# build, not once per configure. It used to run here, which meant the tag
# froze at whatever HEAD was when the build tree was last configured; a bench
# build was observed advertising a sha five commits stale. The script writes
# build/generated/LongpathBuildTag.h only when the text changes, so the extra
# build step costs one git call and recompiles nothing on a no-op build.
set(LONGPATH_BUILD_TAG "" CACHE STRING
    "Text appended to the window title identifying a non-release build. Auto-derived from git when empty; set to a single space to force it off.")

set(LONGPATH_BUILD_TAG_HEADER "${CMAKE_BINARY_DIR}/generated/LongpathBuildTag.h")
set(LONGPATH_BUILD_TAG_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LongpathBuildTag.cmake")

# Resolve git once here rather than in the script, which would repeat the
# find_program on every build. Copied into a local rather than clearing
# GIT_EXECUTABLE itself: that is a cache variable, and a directory-scope
# shadow of it would be inherited by add_subdirectory(tests) and
# add_subdirectory(hal-plugin) below.
find_package(Git QUIET)
set(LONGPATH_BUILD_TAG_GIT "")
if(GIT_FOUND)
    set(LONGPATH_BUILD_TAG_GIT "${GIT_EXECUTABLE}")
endif()

set(LONGPATH_BUILD_TAG_ARGS
    "-DLONGPATH_REPO_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DLONGPATH_OUTPUT=${LONGPATH_BUILD_TAG_HEADER}"
    "-DLONGPATH_OVERRIDE=${LONGPATH_BUILD_TAG}"
    "-DLONGPATH_GIT_EXECUTABLE=${LONGPATH_BUILD_TAG_GIT}")

# Configure-time run: the header has to exist before the first compile (and
# before AUTOMOC scans main.cpp), and this is where the one-line smoke-vs-
# release status message comes from.
execute_process(
    COMMAND "${CMAKE_COMMAND}" ${LONGPATH_BUILD_TAG_ARGS} -DLONGPATH_ANNOUNCE=ON
            -P "${LONGPATH_BUILD_TAG_SCRIPT}")

# Build-time run: always out of date on purpose, so the tag tracks HEAD
# without needing a re-configure.
add_custom_target(longpath_build_tag ALL
    COMMAND "${CMAKE_COMMAND}" ${LONGPATH_BUILD_TAG_ARGS}
            -P "${LONGPATH_BUILD_TAG_SCRIPT}"
    BYPRODUCTS "${LONGPATH_BUILD_TAG_HEADER}"
    COMMENT "Re-deriving the smoke-build tag"
    VERBATIM)
add_dependencies(Longpath longpath_build_tag)

# Deliberately PRIVATE to the executable, and deliberately consumed only by
# src/main.cpp. Every one of the ~450 test executables links LongpathObjs,
# so a generated header reaching that object library would relink all of them
# each time HEAD moved. Confined here, a new commit recompiles one small
# translation unit and relinks one binary. main.cpp hands the value to
# BuildIdentity, which is what MainWindow reads when it composes the title.
target_include_directories(Longpath PRIVATE "${CMAKE_BINARY_DIR}/generated")

# --------------------------------------------------------------------------
# Version define
# --------------------------------------------------------------------------
target_compile_definitions(LongpathObjs PUBLIC
    LONGPATH_VERSION="${PROJECT_VERSION}"
)

# --------------------------------------------------------------------------
# Compiler warnings
# --------------------------------------------------------------------------
if(MSVC)
    target_compile_options(LongpathObjs PRIVATE /W3 /Zc:__cplusplus)
else()
    target_compile_options(LongpathObjs PRIVATE -Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(LongpathObjs PUBLIC -fsanitize=address)
        target_link_options(Longpath PRIVATE -fsanitize=address)
    endif()
endif()

# --------------------------------------------------------------------------
# Precompiled header (LONGPATH_USE_PCH=ON by default)
#
# CI bake-off (ubuntu-24.04, 4 vCPU, 451 test executables) showed Linux
# spending ~33m in the build step vs ~6m on macOS / ~12m on Windows.
# Most of that is repeated parsing of Qt6 + STL headers across the
# LongpathObjs translation units AND across every test target that
# REUSE_FROMs this PCH (see tests/CMakeLists.txt longpath_add_test()).
#
# Keep the list conservative: only headers used by the vast majority of
# TUs. Adding rarely-used headers slows incremental builds because every
# PCH change retriggers all consumers.
#
# ccache must be configured with sloppiness=pch_defines,time_macros for
# PCH'd compiles to be cacheable; CI sets this in the "Configure ccache"
# step. Local builds inherit ccache's defaults — fine for clean builds,
# may have lower hit rates on incremental until ~/.config/ccache/ccache.conf
# matches CI.
# --------------------------------------------------------------------------
option(LONGPATH_USE_PCH "Use precompiled headers to speed up builds" ON)
# Phase 3J-1 closeout follow-up (2026-05-13): exclude Objective-C++ (.mm)
# translation units from the PCH.  The PCH is generated as plain C++; an
# Objective-C++ TU that tries to reuse it errors out with "Objective-C was
# disabled in precompiled file ... but is currently enabled".  Affects
# MacMicPermission.mm on macOS.  set_source_files_properties with
# SKIP_PRECOMPILE_HEADERS bypasses the include for that single file
# without disabling PCH for the whole target.
if(APPLE)
    set_source_files_properties(src/core/strip/AuPluginEditor.mm
                                PROPERTIES SKIP_PRECOMPILE_HEADERS ON
                                           SKIP_UNITY_BUILD_INCLUSION ON)
    set_source_files_properties(src/core/MacMicPermission.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    set_source_files_properties(src/gui/MacFloatingWindowBehavior.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    set_source_files_properties(src/gui/MacVollflaeche.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    set_source_files_properties(src/gui/hotkeys/MacMenuTrackingWatch.mm
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()
if(LONGPATH_USE_PCH)
    target_precompile_headers(LongpathObjs PRIVATE
        # Qt6 core (every model + worker TU)
        <QObject>
        <QString>
        <QStringList>
        <QByteArray>
        <QVariant>
        <QHash>
        <QMap>
        <QList>
        <QVector>
        <QPointer>
        <QTimer>
        <QDebug>
        # GUI commons (every applet + dialog + widget TU)
        <QWidget>
        <QPainter>
        <QPaintEvent>
        <QColor>
        <QFont>
        <QPen>
        <QBrush>
        # STL fundamentals
        <vector>
        <string>
        <memory>
        <atomic>
        <functional>
        <algorithm>
        <array>
        <chrono>
        <cstdint>
        <utility>
    )
endif()

# --------------------------------------------------------------------------
# Install
# --------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS Longpath
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .
)

# Bundle third-party license notices + Longpath's LICENSE into release artifacts.
# LICENSE-DUAL-LICENSING is shipped alongside LICENSE because Thetis maintains
# both at project root and GPL §1 "keep intact all notices that refer to this
# License" extends to every license-related file upstream, not only the main
# LICENSE text. A binary recipient obtaining the source is owed both files.
install(DIRECTORY packaging/third-party-licenses/
        DESTINATION share/doc/nereussdr/licenses
        COMPONENT licenses)
install(FILES LICENSE LICENSE-DUAL-LICENSING
        DESTINATION share/doc/nereussdr
        COMPONENT licenses)

# --------------------------------------------------------------------------
# Tests (Qt6::Test). Opt-in via -DLONGPATH_BUILD_TESTS=ON.
# --------------------------------------------------------------------------
option(LONGPATH_BUILD_TESTS "Build Longpath test suite" OFF)
if(LONGPATH_BUILD_TESTS)
    # Propagate the define to the object library so test-guard blocks
    # (e.g. isTabVisibleForTest) are compiled into LongpathObjs and
    # therefore available for linking into test executables.
    target_compile_definitions(LongpathObjs PUBLIC LONGPATH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --------------------------------------------------------------------------
# macOS HAL Audio Server Plug-In (Phase 3O VAX — Sub-Phase 5)
#
# Builds LongpathVAX.driver, a standalone CoreAudio HAL plugin that
# exposes 4 virtual RX devices ("Longpath VAX 1..4") plus 1 TX device
# ("Longpath TX"). Off by default because the plugin pulls libASPL via
# FetchContent and its preferred build is a separate cmake tree
# (`cmake -B build-hal -S hal-plugin`) — see docs/architecture/
# 2026-04-19-vax-design.md §8.1. Enable here only when you want the
# driver target wired into the main configure for IDE integration.
# --------------------------------------------------------------------------
option(LONGPATH_BUILD_HAL_PLUGIN "Build macOS CoreAudio HAL Audio Server plug-in" OFF)
if(APPLE AND LONGPATH_BUILD_HAL_PLUGIN)
    add_subdirectory(hal-plugin)
endif()

# ── TCI-Sonde (2026-08-23) ────────────────────────────────────────────
#
# WERKZEUG, kein Programmteil. Beantwortet die eine Frage, an der der
# SunSDR-QRP-Weg haengt: gibt ExpertSDR3 ueber TCI auch Spektrum und
# Ton heraus, oder nur Steuerbefehle?
#
# EXCLUDE_FROM_ALL: sie wird nicht mitgebaut und nicht ausgeliefert.
#   cmake --build build --target tci_probe
#   ./build/tci_probe ws://127.0.0.1:40001 12
add_executable(tci_probe EXCLUDE_FROM_ALL tools/tci_probe.cpp)
target_link_libraries(tci_probe PRIVATE Qt6::Core Qt6::WebSockets)

# ── Begleitgeraet-Sonde (2026-10-28) ──────────────────────────────────
#
# WERKZEUG, kein Programmteil. Die ganze CAT-Kette gegen ein ECHTES
# rigctld (Hamlibs Dummy-Geraet, Modell 1), nicht gegen den
# selbstgebauten Server aus tst_rigctld_client: faengt Zeilenenden,
# Reihenfolge, Begruessung und die Form der RPRT-Antworten, die ein
# Server, den wir selbst geschrieben haben, nicht faengt.
#
#   cmake --build build --target begleitgeraet_probe
#   ./build/begleitgeraet_probe
add_executable(begleitgeraet_probe EXCLUDE_FROM_ALL
    tools/begleitgeraet_probe.cpp)
target_link_libraries(begleitgeraet_probe PRIVATE
    LongpathObjs Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

# ── SunSDR-TX-Pacer-Jitter-Sonde (Schritt 5, 2026-09-02) ──────────────
#
# WERKZEUG, kein Programmteil. Misst, wie praezise SunSdrTxPacer's
# QTimer(Qt::PreciseTimer) im echten Qt-Event-Loop tickt, gegen das
# 5,12-ms-Ziel aus der Design-Doku — die Bank-Jitter-Messung, die der
# Entwurfs-Workflow fuer Schritt 5 vorgesehen hat, um zu entscheiden, ob
# ein einfacher Timer reicht oder ein eigener OS-Thread noetig ist (eine
# Architekturentscheidung, die laut CLAUDE.md ohnehin eine Freigabe
# braucht). Braucht kein Funkgeraet — SunSdrTxPacer sendet auf keinen
# Socket. Baut SunSdrTxPacer.cpp/SunSdrProtocol.cpp direkt mit, statt
# gegen LongpathObjs zu linken, weil beide Dateien ausschliesslich
# Qt6::Core brauchen (keine Multimedia-/WDSP-Abhaengigkeiten mitziehen).
#
#   cmake --build build --target sunsdr_tx_pacer_jitter_probe
#   ./build/sunsdr_tx_pacer_jitter_probe 30
add_executable(sunsdr_tx_pacer_jitter_probe EXCLUDE_FROM_ALL
    tools/sunsdr_tx_pacer_jitter_probe.cpp
    src/core/sunsdr/SunSdrTxPacer.cpp
    src/core/sunsdr/SunSdrProtocol.cpp
)
target_include_directories(sunsdr_tx_pacer_jitter_probe PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sunsdr_tx_pacer_jitter_probe PRIVATE Qt6::Core)
